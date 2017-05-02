/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    videocon.c

Abstract:

    This module implements functionality for a basic console over a video
    frame buffer.

Author:

    Evan Green 15-Feb-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/kernel/sysres.h>
#include <minoca/lib/basevid.h>
#include <minoca/lib/termlib.h>
#include <minoca/video/fb.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This routine gets the line structure for the given row.
//

#define GET_CONSOLE_LINE(_Console, _Row) \
    (PVIDEO_CONSOLE_LINE)((_Console)->Lines + \
                          (CONSOLE_LINE_SIZE(_Console) * \
                           CONSOLE_ROW_INDEX(_Console, _Row)))

//
// This routine gets the effective console row number, taking into account the
// rotating nature of the console lines. This verbose conditional avoids
// divides.
//

#define CONSOLE_ROW_INDEX(_Console, _Row)                           \
    ((((_Console)->TopLine + (_Row)) < (_Console)->BufferRows) ?    \
     ((_Console)->TopLine + (_Row)) :                               \
     ((_Console)->TopLine + (_Row) - (_Console)->BufferRows))

//
// This macro determines the size of one console line.
//

#define CONSOLE_LINE_SIZE(_Console)                     \
    (sizeof(VIDEO_CONSOLE_LINE) +                       \
     ((((_Console)->Columns) + 1 - ANYSIZE_ARRAY) *     \
      sizeof(BASE_VIDEO_CHARACTER)))

//
// This macro determines if the cursor is within the scroll region.
//

#define CURSOR_IN_SCROLL_REGION(_Console)                   \
    (((_Console)->NextRow >= (_Console)->TopMargin) &&      \
     ((_Console)->NextRow <=                                \
      (_Console)->ScreenRows - 1 - (_Console)->BottomMargin))

//
// These macros determine the index and bit position of a tab stop bit.
//

#define TAB_STOP_INDEX(_Column) ((_Column) / (sizeof(ULONG) * BITS_PER_BYTE))
#define TAB_STOP_MASK(_Column) \
    (1 << ((_Column) % (sizeof(ULONG) * BITS_PER_BYTE)))

//
// These macros manipulate the tab stop bits given a column.
//

#define SET_TAB_STOP(_Console, _Column) \
    (_Console)->TabStops[TAB_STOP_INDEX(_Column)] |= TAB_STOP_MASK(_Column)

#define CLEAR_TAB_STOP(_Console, _Column) \
    (_Console)->TabStops[TAB_STOP_INDEX(_Column)] &= ~TAB_STOP_MASK(_Column)

#define IS_TAB_STOP(_Console, _Column) \
    ((_Console)->TabStops[TAB_STOP_INDEX(_Column)] & TAB_STOP_MASK(_Column))

#define TAB_STOPS_SIZE(_Column) \
    (TAB_STOP_INDEX(_Column + (sizeof(ULONG) * BITS_PER_BYTE) - 1) * \
     sizeof(ULONG))

#define CLEAR_ALL_TAB_STOPS(_Console) \
    RtlZeroMemory((_Console)->TabStops, TAB_STOPS_SIZE((_Console)->Columns))

//
// ---------------------------------------------------------------- Definitions
//

#define VIDEO_CONSOLE_ALLOCATION_TAG 0x6E6F4356 // 'noCV'

#define VIDEO_CONSOLE_READ_BUFFER_SIZE 2048
#define VIDEO_CONSOLE_MAX_LINES 10000

//
// Define the number of milliseconds between blinks.
//

#define VIDEO_CONSOLE_BLINK_RATE 500
#define VIDEO_CONSOLE_CURSOR_BLINK_COUNT 60

//
// Define the number of rows to leave at the top for a banner.
//

#define VIDEO_CONSOLE_TOP_BANNER_ROWS 3

//
// Define known characters.
//

#define VIDEO_CHARACTER_SHIFT_IN 0xF
#define VIDEO_CHARACTER_SHIFT_OUT 0xE

//
// Define pending actions.
//

#define VIDEO_ACTION_REDRAW_ENTIRE_SCREEN 0x00000001
#define VIDEO_ACTION_RESET_SCROLL 0x00000002

//
// Define modes.
//

//
// Keyboard action mode locks the keyboard, preventing all further interactions
// with the user until it is unlocked.
//

#define CONSOLE_MODE_KEYBOARD_ACTION 0x00000002

//
// Insert mode causes characters to get shifted over. Characters that move
// past the right margin are lost. If this is not set, it is in replace mode,
// where characters overwrite the previous ones.
//

#define CONSOLE_MODE_INSERT 0x00000004

//
// If this bit is set, characters from the keyboard are not automatically
// echoed to the screen.
//

#define CONSOLE_MODE_DISABLE_LOCAL_ECHO 0x00000008

//
// If this bit is set then Line Feed, Form Feed, and Vertical Tab characters
// all reset the column position to zero in addition to incrementing the
// vertical position.
//

#define CONSOLE_MODE_NEW_LINE 0x00000010

//
// If this bit is set, then the cursor is visible.
//

#define CONSOLE_MODE_CURSOR 0x00000020

//
// If this bit is set, the cursor keys send application control functions. If
// clear, the cursor keys send ANSI cursor control sequences.
//

#define CONSOLE_MODE_APPLICATION_CURSOR_KEYS 0x00000040

//
// If this bit is set, the console switches to VT52 compatibility mode.
//

#define CONSOLE_MODE_VT52 0x00000080

//
// If this bit is set, the console has 132 (or more) columns. If clear, the
// console is set to 80 columns.
//

#define CONSOLE_MODE_132_COLUMN 0x00000100

//
// If this bit is set, smooth scrolling is performed, a maximum of 6 lines per
// second is output. If clear, lines are displayed as they come in.
//

#define CONSOLE_MODE_SMOOTH_SCROLL 0x00000200

//
// If this bit is set, the screen's default foreground and background colors
// are switched.
//

#define CONSOLE_MODE_VIDEO_REVERSED 0x00000400

//
// If this bit is set, the home position is set to the top left of the user
// defined scroll region. The user cannot move out of the scroll region. The
// erase in display command is an exception to that. If this is clear, the
// home position is the upper-left corner of the screen.
//

#define CONSOLE_MODE_ORIGIN 0x00000800

//
// If this bit is set, characters received when the cursor is at the right
// margin appear on the next line. The display scrolls up if the cursor is at
// the end of the scrolling region. If this bit is clear, characters that
// appear at the right replace previously displayed characters.
//

#define CONSOLE_MODE_AUTO_WRAP 0x00001000

//
// If this bit is set, keypad keys send application control functions. If clear,
// keypad keys send numeric values (plus comma, period, plus minus, etc.)
//

#define CONSOLE_MODE_APPLICATION_KEYPAD 0x00002000

//
// If this bit is set, the cursor blinks.
//

#define CONSOLE_MODE_CURSOR_BLINK 0x00004000

//
// Define the default video mode bits when the console is initialized.
//

#define VIDEO_CONSOLE_MODE_DEFAULTS                                            \
    (CONSOLE_MODE_CURSOR | CONSOLE_MODE_CURSOR_BLINK | CONSOLE_MODE_AUTO_WRAP)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines state associated with a single horizontal line of
    the video console.

Members:

    Attributes - Stores attributes for the entire line.

    Character - Stores the array of printable characters in this line.

--*/

typedef struct _VIDEO_CONSOLE_LINE {
    USHORT Attributes;
    BASE_VIDEO_CHARACTER Character[ANYSIZE_ARRAY];
} VIDEO_CONSOLE_LINE, *PVIDEO_CONSOLE_LINE;

/*++

Structure Description:

    This structure defines state associated with a video console.

Members:

    PhysicalAddress - Stores the physical address of the frame buffer.

    VideoContext - Stores the base video library context used for low level
        print routines.

    FrameBuffer - Stores the virtual address of the frame buffer.

    Width - Stores the width of the frame buffer, in pixels.

    Height - Stores the height of the frame buffer, in pixels.

    BitsPerPixel - Stores the number of bits that correspond to one pixel.

    Columns - Stores the number of text columns in the console.

    ScreenRows - Stores the number of rows that can be displayed on the screen.

    BufferRows - Stores the number of rows in the buffer. This must be at least
        as large as the number of rows on the screen.

    MaxRows - Stores the maximum number of rows that should be stored in this
        console. Set to 0 for unlimited.

    TopMargin - Stores the top margin of the scroll area in lines. A count of
        zero means the console will scroll with scrollback.

    BottomMargin - Stores the bottom margin of the scroll area in lines. A
        count of zero means the console goes to the bottom of the screen.

    Lines - Stores a pointer to the array of lines representing the contents of
        this console.

    Screen - Stores a pointer to the array of lines that represents what's
        actually on the screen.

    TopLine - Stores the index of the line displaying at the top of the screen.

    Lock - Stores a pointer to a lock that serializes access to the console.

    NextColumn - Stores the zero-based column number where the next character
        will be printed. This might be equal to the column count in order to
        handle the old VT100 wraparound bug.

    NextRow - Stores the zero-based row number where the next character will be
        printed. This is a screen row, not a buffer row.

    RowViewOffset - Stores the number of lines down from the screen top row to
        display the screen.

    TextAttributes - Stores the current text attributes for printed text.

    Command - Stores the terminal input command data.

    PendingAction - Stores a bitfield of flags containing actions that need to
        be performed.

    Mode - Stores the console mode selections. See VIDEO_*_MODE definitions.

    SavedColumn - Stores the cursor column when a save cursor command occurred.

    SavedRow - Stores the cursor row when a save cursor command occurred.

    SavedAttributes - Stores the next attributes when a save cursor command
        occurred.

    TabStops - Stores a bitfield of the current tab stops. Each bit represents
        a column, and that bit is set if the column is a tab stop.

    CreationTime - Stores the time of creation of this device.

    OpenHandles - Stores the number of open device handles. If any device
        handles are open, then the terminal is not drawn.

    Size - Stores the size of the frame buffer in bytes.

    BaseVideoMode - Stores the base video mode.

    RedMask - Stores the maxk of red bits in each pixel.

    GreenMask - Stores the mask of green bits in each pixel.

    BlueMask - Stores the mask of blue bits in each pixel.

    PixelsPerScanLine - Stores the number of pixels in a line, including
        any extra non-visual pixels.

    BannerThreadEnabled - Stores a boolean indicating if the banner thread was
        previously enabled or not.

--*/

typedef struct _VIDEO_CONSOLE_DEVICE {
    PHYSICAL_ADDRESS PhysicalAddress;
    BASE_VIDEO_CONTEXT VideoContext;
    PVOID FrameBuffer;
    LONG Width;
    LONG Height;
    LONG BitsPerPixel;
    LONG Columns;
    LONG ScreenRows;
    LONG BufferRows;
    LONG MaxRows;
    LONG TopMargin;
    LONG BottomMargin;
    PVOID Lines;
    PVOID Screen;
    LONG TopLine;
    PQUEUED_LOCK Lock;
    LONG NextColumn;
    LONG NextRow;
    LONG RowViewOffset;
    USHORT TextAttributes;
    TERMINAL_COMMAND_DATA Command;
    ULONG PendingAction;
    ULONG Mode;
    LONG SavedColumn;
    LONG SavedRow;
    LONG SavedAttributes;
    PULONG TabStops;
    SYSTEM_TIME CreationTime;
    ULONG OpenHandles;
    UINTN Size;
    ULONG BaseVideoMode;
    ULONG RedMask;
    ULONG GreenMask;
    ULONG BlueMask;
    ULONG PixelsPerScanLine;
    ULONG BannerThreadEnabled;
} VIDEO_CONSOLE_DEVICE, *PVIDEO_CONSOLE_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
VcAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
VcDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
VcDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
VcDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
VcDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
VcDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
VcDispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
VcpLocalTerminalRedrawThread (
    PVOID Parameter
    );

VOID
VcpWriteToConsole (
    PVIDEO_CONSOLE_DEVICE Console,
    PSTR String,
    UINTN StringLength
    );

VOID
VcpProcessCommand (
    PVIDEO_CONSOLE_DEVICE Console,
    PTERMINAL_COMMAND_DATA Command
    );

VOID
VcpEraseArea (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG StartColumn,
    LONG StartRow,
    LONG EndColumn,
    LONG EndRow,
    BOOL ResetAttributes
    );

VOID
VcpRedrawArea (
    PVIDEO_CONSOLE_DEVICE Console,
    BOOL Force,
    LONG StartColumn,
    LONG StartRow,
    LONG EndColumn,
    LONG EndRow
    );

VOID
VcpSetOrClearMode (
    PVIDEO_CONSOLE_DEVICE Console,
    ULONG ModeNumber,
    TERMINAL_COMMAND Command
    );

VOID
VcpAdvanceRow (
    PVIDEO_CONSOLE_DEVICE Console
    );

VOID
VcpSetColorFromParameters (
    PVIDEO_CONSOLE_DEVICE Console,
    PTERMINAL_COMMAND_DATA Command
    );

VOID
VcpSaveRestoreCursor (
    PVIDEO_CONSOLE_DEVICE Console,
    BOOL Save
    );

VOID
VcpMoveCursorRelative (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG DistanceX,
    LONG DistanceY
    );

VOID
VcpMoveCursorAbsolute (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG Column,
    LONG Row,
    BOOL ProcessOriginMode
    );

VOID
VcpDeleteLines (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG Count,
    LONG StartingRow
    );

VOID
VcpInsertLines (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG Count,
    LONG StartingRow
    );

VOID
VcpMoveCursorTabStops (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG Advance
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER VcDriver = NULL;

//
// Store the next identifier.
//

volatile ULONG VcNextIdentifier = 0;

//
// Store a pointer to the local terminal.
//

PIO_HANDLE VcLocalTerminal;

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the video console driver. It registers
    its other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    ULONG AllocationSize;
    LONG Columns;
    PVIDEO_CONSOLE_DEVICE ConsoleDevice;
    volatile ULONG DeviceId;
    CHAR DeviceIdString[15];
    PSYSTEM_RESOURCE_FRAME_BUFFER FrameBufferResource;
    DRIVER_FUNCTION_TABLE FunctionTable;
    PSYSTEM_RESOURCE_HEADER GenericHeader;
    ULONG Height;
    LONG LineSize;
    LONG Rows;
    ULONG RowSize;
    KSTATUS Status;
    LONG TabIndex;
    ULONG TabStopSize;
    UINTN TopOffset;
    SYSTEM_RESOURCE_FRAME_BUFFER VideoResource;
    ULONG Width;
    TERMINAL_WINDOW_SIZE WindowSize;

    ConsoleDevice = NULL;
    VcDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = VcAddDevice;
    FunctionTable.DispatchStateChange = VcDispatchStateChange;
    FunctionTable.DispatchOpen = VcDispatchOpen;
    FunctionTable.DispatchClose = VcDispatchClose;
    FunctionTable.DispatchIo = VcDispatchIo;
    FunctionTable.DispatchSystemControl = VcDispatchSystemControl;
    FunctionTable.DispatchUserControl = VcDispatchUserControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Get all frame buffers from the boot environment.
    //

    while (TRUE) {
        GenericHeader = KeAcquireSystemResource(SystemResourceFrameBuffer);
        if (GenericHeader == NULL) {
            break;
        }

        //
        // TODO: The base video library can only handle one frame buffer at a
        // time. If multiple frame buffers crop up, retrofit that library to
        // support multiple consoles.
        //

        ASSERT(VcNextIdentifier == 0);

        FrameBufferResource = (PSYSTEM_RESOURCE_FRAME_BUFFER)GenericHeader;

        //
        // Ensure the frame buffer is big enough for at least a character.
        //

        Height = FrameBufferResource->Height;
        Width = FrameBufferResource->Width;
        if (FrameBufferResource->Mode == BaseVideoModeBiosText) {
            if ((Height <= VIDEO_CONSOLE_TOP_BANNER_ROWS) || (Width < 1)) {
                continue;
            }

            Height -= VIDEO_CONSOLE_TOP_BANNER_ROWS;
            RowSize = FrameBufferResource->Width *
                      FrameBufferResource->BitsPerPixel / BITS_PER_BYTE;

            TopOffset = RowSize * VIDEO_CONSOLE_TOP_BANNER_ROWS;
            Columns = Width;
            Rows = Height;

        } else {

            ASSERT(FrameBufferResource->Mode == BaseVideoModeFrameBuffer);

            if ((Height <=
                 VIDEO_CONSOLE_TOP_BANNER_ROWS * VidDefaultFont->CellHeight) ||
                (Width < VidDefaultFont->CellWidth)) {

                continue;
            }

            Height -= VIDEO_CONSOLE_TOP_BANNER_ROWS *
                      VidDefaultFont->CellHeight;

            RowSize = FrameBufferResource->Width *
                      FrameBufferResource->BitsPerPixel / BITS_PER_BYTE;

            TopOffset = RowSize * (VIDEO_CONSOLE_TOP_BANNER_ROWS *
                                   VidDefaultFont->CellHeight);

            Columns = Width / VidDefaultFont->CellWidth;
            Rows = Height / VidDefaultFont->CellHeight;
        }

        ConsoleDevice = MmAllocatePagedPool(sizeof(VIDEO_CONSOLE_DEVICE),
                                            VIDEO_CONSOLE_ALLOCATION_TAG);

        if (ConsoleDevice == NULL) {
            goto DriverEntryEnd;
        }

        RtlZeroMemory(ConsoleDevice, sizeof(VIDEO_CONSOLE_DEVICE));

        //
        // Determine the size of the allocation needed for the lines.
        //

        LineSize = sizeof(VIDEO_CONSOLE_LINE) +
                   ((Columns + 1 - ANYSIZE_ARRAY) *
                    sizeof(BASE_VIDEO_CHARACTER));

        AllocationSize = LineSize * Rows;

        //
        // Allocate the internal data structure.
        //

        ConsoleDevice->Lines = MmAllocatePagedPool(
                                                 AllocationSize,
                                                 VIDEO_CONSOLE_ALLOCATION_TAG);

        if (ConsoleDevice->Lines == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto DriverEntryEnd;
        }

        RtlZeroMemory(ConsoleDevice->Lines, AllocationSize);
        ConsoleDevice->Screen = MmAllocatePagedPool(
                                                 AllocationSize,
                                                 VIDEO_CONSOLE_ALLOCATION_TAG);

        if (ConsoleDevice->Screen == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto DriverEntryEnd;
        }

        RtlZeroMemory(ConsoleDevice->Screen, AllocationSize);
        TabStopSize = TAB_STOPS_SIZE(Columns);
        ConsoleDevice->TabStops = MmAllocatePagedPool(
                                                 TabStopSize,
                                                 VIDEO_CONSOLE_ALLOCATION_TAG);

        if (ConsoleDevice->TabStops == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto DriverEntryEnd;
        }

        ConsoleDevice->PhysicalAddress =
                                   FrameBufferResource->Header.PhysicalAddress;

        ConsoleDevice->FrameBuffer = FrameBufferResource->Header.VirtualAddress;

        //
        // The frame buffer must be page aligned because otherwise handing
        // back direct I/O buffers to the frame buffer won't work for mmap.
        //

        ASSERT((IS_ALIGNED(ConsoleDevice->PhysicalAddress, MmPageSize())) &&
               (IS_POINTER_ALIGNED(ConsoleDevice->FrameBuffer, MmPageSize())));

        ConsoleDevice->Width = Width;
        ConsoleDevice->Height = FrameBufferResource->Height;
        ConsoleDevice->BitsPerPixel = FrameBufferResource->BitsPerPixel;
        ConsoleDevice->Columns = Columns;
        ConsoleDevice->ScreenRows = Rows;
        ConsoleDevice->BufferRows = Rows;
        ConsoleDevice->MaxRows = VIDEO_CONSOLE_MAX_LINES;
        ConsoleDevice->Mode = VIDEO_CONSOLE_MODE_DEFAULTS;
        ConsoleDevice->Size = RowSize * FrameBufferResource->Height;
        ConsoleDevice->BaseVideoMode = FrameBufferResource->Mode;
        ConsoleDevice->RedMask = FrameBufferResource->RedMask;
        ConsoleDevice->GreenMask = FrameBufferResource->GreenMask;
        ConsoleDevice->BlueMask = FrameBufferResource->BlueMask;
        ConsoleDevice->PixelsPerScanLine =
                                        FrameBufferResource->PixelsPerScanLine;

        KeGetSystemTime(&(ConsoleDevice->CreationTime));

        //
        // Set up some default tab stops every 8 characters, since things seem
        // to expect that.
        //

        CLEAR_ALL_TAB_STOPS(ConsoleDevice);
        TabIndex = 8;
        while (TabIndex < ConsoleDevice->Columns) {
            SET_TAB_STOP(ConsoleDevice, TabIndex);
            TabIndex += 8;
        }

        ConsoleDevice->Lock = KeCreateQueuedLock();
        if (ConsoleDevice->Lock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto DriverEntryEnd;
        }

        RtlCopyMemory(&VideoResource,
                      FrameBufferResource,
                      sizeof(SYSTEM_RESOURCE_FRAME_BUFFER));

        VideoResource.Header.VirtualAddress =
                                        ConsoleDevice->FrameBuffer + TopOffset;

        VideoResource.Header.PhysicalAddress =
                                    ConsoleDevice->PhysicalAddress + TopOffset;

        VideoResource.Width = Width;
        VideoResource.Height = Height;
        Status = VidInitialize(&(ConsoleDevice->VideoContext), &VideoResource);
        if (!KSUCCESS(Status)) {
            goto DriverEntryEnd;
        }

        //
        // Ensure the calculation agrees with the macro. Ideally the macro would
        // have been used directly to calculate the line size, but it attempts
        // to dereference the console object and thus cannot be used.
        //

        ASSERT(LineSize == CONSOLE_LINE_SIZE(ConsoleDevice));

        DeviceId = RtlAtomicAdd32(&VcNextIdentifier, 1);
        RtlPrintToString(DeviceIdString,
                         15,
                         CharacterEncodingDefault,
                         "VideoConsole%x",
                         DeviceId);

        //
        // Get a handle to the master side of the local console terminal and
        // create the local console redraw thread.
        //

        ASSERT(VcLocalTerminal == NULL);

        Status = IoOpenLocalTerminalMaster(&VcLocalTerminal);
        if (KSUCCESS(Status)) {
            Status = PsCreateKernelThread(VcpLocalTerminalRedrawThread,
                                          ConsoleDevice,
                                          "VcpLocalTerminalRedrawThread");

            if (!KSUCCESS(Status)) {
                goto DriverEntryEnd;
            }
        }

        //
        // Set the window size in the terminal.
        //

        RtlZeroMemory(&WindowSize, sizeof(TERMINAL_WINDOW_SIZE));
        WindowSize.Rows = Rows;
        WindowSize.Columns = Columns;
        WindowSize.PixelsX = Width;
        WindowSize.PixelsY = Height;
        IoUserControl(VcLocalTerminal,
                      TerminalControlSetWindowSize,
                      TRUE,
                      &WindowSize,
                      sizeof(TERMINAL_WINDOW_SIZE));

        //
        // Create the video console device.
        //

        Status = IoCreateDevice(VcDriver,
                                ConsoleDevice,
                                NULL,
                                DeviceIdString,
                                CHARACTER_CLASS_ID,
                                NULL,
                                NULL);

        if (!KSUCCESS(Status)) {
            goto DriverEntryEnd;
        }
    }

DriverEntryEnd:
    if (!KSUCCESS(Status)) {

        ASSERT(VcNextIdentifier <= 1);

        if (ConsoleDevice != NULL) {
            if (ConsoleDevice->Lock != NULL) {
                KeDestroyQueuedLock(ConsoleDevice->Lock);
            }

            if (ConsoleDevice->Lines != NULL) {
                MmFreePagedPool(ConsoleDevice->Lines);
            }

            if (ConsoleDevice->Screen != NULL) {
                MmFreePagedPool(ConsoleDevice->Screen);
            }

            MmFreePagedPool(ConsoleDevice);
        }
    }

    return Status;
}

KSTATUS
VcAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the video console
    device acts as the function driver. The driver will attach itself to the
    stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    //
    // The Video console is not a real device, so it is not expected to be
    // attaching to emerging stacks.
    //

    return STATUS_NOT_IMPLEMENTED;
}

VOID
VcDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    BOOL CompleteIrp;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    //
    // The IRP is on its way down the stack. Do most processing here.
    //

    if (Irp->Direction == IrpDown) {
        Status = STATUS_NOT_SUPPORTED;
        CompleteIrp = TRUE;
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            Status = STATUS_SUCCESS;
            break;

        case IrpMinorStartDevice:
            Status = STATUS_SUCCESS;
            break;

        case IrpMinorQueryChildren:
            Irp->U.QueryChildren.Children = NULL;
            Irp->U.QueryChildren.ChildCount = 0;
            Status = STATUS_SUCCESS;
            break;

        //
        // Pass all other IRPs down.
        //

        default:
            CompleteIrp = FALSE;
            break;
        }

        //
        // Complete the IRP unless there's a reason not to.
        //

        if (CompleteIrp != FALSE) {
            IoCompleteIrp(VcDriver, Irp, Status);
        }

    //
    // The IRP is completed and is on its way back up.
    //

    } else {

        ASSERT(Irp->Direction == IrpUp);
    }

    return;
}

VOID
VcDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PVIDEO_CONSOLE_DEVICE Console;
    UINTN DataSize;
    ULONG PreviousHandles;

    Console = (PVIDEO_CONSOLE_DEVICE)DeviceContext;
    PreviousHandles = RtlAtomicAdd32(&(Console->OpenHandles), 1);

    ASSERT(PreviousHandles < 0x10000000);

    if (PreviousHandles == 0) {

        //
        // Disable the banner thread since the frame buffer is about to be
        // owned by user mode. Failure is not fatal, it just means people will
        // be competing for the frame buffer.
        //

        Console->BannerThreadEnabled = FALSE;
        DataSize = sizeof(Console->BannerThreadEnabled);
        KeGetSetSystemInformation(SystemInformationKe,
                                  KeInformationBannerThread,
                                  &(Console->BannerThreadEnabled),
                                  &DataSize,
                                  TRUE);
    }

    IoCompleteIrp(VcDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
VcDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PVIDEO_CONSOLE_DEVICE Console;
    UINTN DataSize;
    ULONG PreviousHandles;

    Console = (PVIDEO_CONSOLE_DEVICE)DeviceContext;
    PreviousHandles = RtlAtomicAdd32(&(Console->OpenHandles), -1);

    ASSERT((PreviousHandles <= 0x10000000) && (PreviousHandles != 0));

    if (PreviousHandles == 1) {

        //
        // Re-enable the banner thread if it was previously enabled.
        //

        if (Console->BannerThreadEnabled != FALSE) {
            DataSize = sizeof(Console->BannerThreadEnabled);
            KeGetSetSystemInformation(SystemInformationKe,
                                      KeInformationBannerThread,
                                      &(Console->BannerThreadEnabled),
                                      &DataSize,
                                      TRUE);
        }

        VcpRedrawArea(Console,
                      TRUE,
                      0,
                      0,
                      Console->Columns,
                      Console->ScreenRows - 1);
    }

    IoCompleteIrp(VcDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
VcDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PVIDEO_CONSOLE_DEVICE Console;
    ULONGLONG Offset;
    UINTN Size;
    KSTATUS Status;

    Console = (PVIDEO_CONSOLE_DEVICE)DeviceContext;
    Offset = Irp->U.ReadWrite.IoOffset;
    if (Offset >= Console->Size) {
        Status = STATUS_END_OF_FILE;
        goto DispatchIoEnd;
    }

    Size = Irp->U.ReadWrite.IoSizeInBytes;
    if ((Offset + Size < Offset) || (Offset + Size > Console->Size)) {
        Size = Console->Size - Offset;
    }

    //
    // Writes just copy to the frame buffer.
    //

    if (Irp->MinorCode == IrpMinorIoWrite) {
        Status = MmCopyIoBufferData(Irp->U.ReadWrite.IoBuffer,
                                    Console->FrameBuffer + Offset,
                                    0,
                                    Size,
                                    FALSE);

        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

    } else {

        //
        // If an I/O buffer was already supplied, then copy into it (for things
        // like regular user mode reads).
        //

        if (Irp->U.ReadWrite.IoBuffer->FragmentCount != 0) {
            Status = MmCopyIoBufferData(Irp->U.ReadWrite.IoBuffer,
                                        Console->FrameBuffer + Offset,
                                        0,
                                        Size,
                                        TRUE);

        //
        // Return the frame buffer directly (for things like mmap).
        //

        } else {
            Status = MmAppendIoBufferData(Irp->U.ReadWrite.IoBuffer,
                                          Console->FrameBuffer + Offset,
                                          Console->PhysicalAddress + Offset,
                                          Size);
        }

        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }
    }

DispatchIoEnd:
    if (KSUCCESS(Status)) {
        Irp->U.ReadWrite.IoBytesCompleted = Size;
        Irp->U.ReadWrite.NewIoOffset = Offset + Size;
    }

    IoCompleteIrp(VcDriver, Irp, Status);
    return;
}

VOID
VcDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PVIDEO_CONSOLE_DEVICE Console;
    PVOID Context;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    KSTATUS Status;

    Console = (PVIDEO_CONSOLE_DEVICE)DeviceContext;
    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Lookup->Flags = LOOKUP_FLAG_NO_PAGE_CACHE;
        Lookup->MapFlags = MAP_FLAG_WRITE_THROUGH;
        Status = STATUS_PATH_NOT_FOUND;
        if (Lookup->Root != FALSE) {

            //
            // Enable opening of the root as a single file.
            //

            Properties = Lookup->Properties;
            Properties->FileId = 0;
            Properties->Type = IoObjectCharacterDevice;
            Properties->HardLinkCount = 1;
            Properties->BlockSize = 1;
            Properties->BlockCount = 0;
            Properties->UserId = 0;
            Properties->GroupId = 0;
            Properties->StatusChangeTime = Console->CreationTime;
            Properties->ModifiedTime = Properties->StatusChangeTime;
            Properties->AccessTime = Properties->StatusChangeTime;
            Properties->Permissions = FILE_PERMISSION_ALL;
            Properties->Size = 0;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(VcDriver, Irp, Status);
        break;

    //
    // Succeed for the basics.
    //

    case IrpMinorSystemControlWriteFileProperties:
    case IrpMinorSystemControlTruncate:
        Status = STATUS_SUCCESS;
        IoCompleteIrp(VcDriver, Irp, Status);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
VcDispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles User Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PVIDEO_CONSOLE_DEVICE Console;
    PVOID CopyAddress;
    ULONG CopySize;
    FRAME_BUFFER_INFO Info;
    FRAME_BUFFER_MODE Mode;
    KSTATUS Status;
    PIRP_USER_CONTROL UserControl;

    CopyAddress = NULL;
    CopySize = 0;
    Console = (PVIDEO_CONSOLE_DEVICE)DeviceContext;
    Status = STATUS_SUCCESS;
    UserControl = &(Irp->U.UserControl);
    switch ((ULONG)(Irp->MinorCode)) {
    case FrameBufferGetInfo:
        if (UserControl->UserBufferSize < sizeof(FRAME_BUFFER_INFO)) {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        RtlZeroMemory(&Info, sizeof(Info));
        Info.Magic = FRAME_BUFFER_MAGIC;
        RtlStringCopy(Info.Identifier, "VideoCon", sizeof(Info.Identifier));
        Info.Type = FrameBufferTypeLinear;

        ASSERT((Console->BaseVideoMode == BaseVideoModeFrameBuffer) ||
               (Console->BaseVideoMode == BaseVideoModeBiosText));

        if (Console->BaseVideoMode == BaseVideoModeBiosText) {
            Info.Type = FrameBufferTypeText;
        }

        Info.Address = Console->PhysicalAddress;
        Info.Length = Console->Size;
        Info.LineLength = Console->PixelsPerScanLine * Console->BitsPerPixel /
                          BITS_PER_BYTE;

        CopySize = sizeof(Info);
        CopyAddress = &Info;
        break;

    case FrameBufferGetMode:
        if (UserControl->UserBufferSize < sizeof(FRAME_BUFFER_MODE)) {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        RtlZeroMemory(&Mode, sizeof(Mode));
        Mode.Magic = FRAME_BUFFER_MAGIC;
        Mode.ResolutionX = Console->Width;
        Mode.ResolutionY = Console->Height;
        Mode.VirtualResolutionX = Mode.ResolutionX;
        Mode.VirtualResolutionY = Mode.ResolutionY;
        Mode.BitsPerPixel = Console->BitsPerPixel;
        Mode.RedMask = Console->RedMask;
        Mode.GreenMask = Console->GreenMask;
        Mode.BlueMask = Console->BlueMask;
        CopySize = sizeof(Mode);
        CopyAddress = &Mode;
        break;

    case FrameBufferSetMode:
        if (UserControl->UserBufferSize < sizeof(FRAME_BUFFER_MODE)) {
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        //
        // See if there's no difference.
        //

        if (UserControl->FromKernelMode != FALSE) {
            RtlCopyMemory(&Mode,
                          UserControl->UserBuffer,
                          sizeof(FRAME_BUFFER_MODE));

        } else {
            Status = MmCopyFromUserMode(&Mode,
                                        UserControl->UserBuffer,
                                        sizeof(FRAME_BUFFER_MODE));

            if (!KSUCCESS(Status)) {
                break;
            }
        }

        //
        // See if there's no change.
        //

        if ((Mode.ResolutionX == Console->Width) &&
            (Mode.ResolutionY == Console->Height) &&
            (Mode.VirtualResolutionX == Mode.ResolutionX) &&
            (Mode.VirtualResolutionY == Mode.ResolutionY) &&
            (Mode.BitsPerPixel == Console->BitsPerPixel) &&
            (Mode.OffsetX == 0) &&
            (Mode.OffsetY == 0) &&
            (Mode.Rotate == 0)) {

            Status = STATUS_SUCCESS;
            break;
        }

        Status = STATUS_NOT_HANDLED;
        break;

    default:
        Status = STATUS_NOT_HANDLED;
        break;
    }

    if ((KSUCCESS(Status)) && (CopySize != 0)) {
        if (UserControl->FromKernelMode != FALSE) {
            RtlCopyMemory(UserControl->UserBuffer, CopyAddress, CopySize);

        } else {
            Status = MmCopyToUserMode(UserControl->UserBuffer,
                                      CopyAddress,
                                      CopySize);
        }
    }

    IoCompleteIrp(VcDriver, Irp, Status);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
VcpLocalTerminalRedrawThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the video console redraw thread, which reads from
    the terminal master and draws the output.

Arguments:

    Parameter - Supplies the thread parameter, in this case a pointer to the
        video console device.

Return Value:

    None.

--*/

{

    ULONG BlinkCount;
    UINTN BytesRead;
    USHORT CursorAttributes;
    LONG CursorColumn;
    LONG CursorRow;
    PVIDEO_CONSOLE_DEVICE Device;
    PIO_BUFFER IoBuffer;
    PVIDEO_CONSOLE_LINE Line;
    PCHAR ReadBuffer;
    KSTATUS Status;
    ULONG Timeout;

    BlinkCount = 0;
    CursorAttributes = 0;
    Device = (PVIDEO_CONSOLE_DEVICE)Parameter;
    ReadBuffer = MmAllocatePagedPool(VIDEO_CONSOLE_READ_BUFFER_SIZE,
                                     VIDEO_CONSOLE_ALLOCATION_TAG);

    if (ReadBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LocalTerminalRedrawThread;
    }

    Status = MmCreateIoBuffer(ReadBuffer,
                              VIDEO_CONSOLE_READ_BUFFER_SIZE,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &IoBuffer);

    if (!KSUCCESS(Status)) {
        goto LocalTerminalRedrawThread;
    }

    //
    // Loop reading the slave's standard out and printing it to the screen.
    //

    while (TRUE) {
        Timeout = WAIT_TIME_INDEFINITE;
        if (((Device->Mode & CONSOLE_MODE_CURSOR) != 0) &&
            ((Device->Mode & CONSOLE_MODE_CURSOR_BLINK) != 0)) {

            //
            // Stop blinking after a little while to save power, but make sure
            // the blinking stops on having the cursor drawn.
            //

            if ((BlinkCount < VIDEO_CONSOLE_CURSOR_BLINK_COUNT) ||
                ((CursorAttributes & BASE_VIDEO_CURSOR) == 0)) {

                Timeout = VIDEO_CONSOLE_BLINK_RATE;
            }
        }

        Status = IoRead(VcLocalTerminal,
                        IoBuffer,
                        VIDEO_CONSOLE_READ_BUFFER_SIZE,
                        0,
                        Timeout,
                        &BytesRead);

        if (Status == STATUS_TIMEOUT) {

            ASSERT(BytesRead == 0);

            CursorRow = Device->NextRow;
            CursorColumn = Device->NextColumn;
            if (CursorColumn == Device->Columns) {
                CursorColumn -= 1;
            }

            Line = GET_CONSOLE_LINE(Device, CursorRow);
            Line->Character[CursorColumn].Data.Attributes ^= BASE_VIDEO_CURSOR;
            CursorAttributes = Line->Character[CursorColumn].Data.Attributes;
            if (Device->OpenHandles == 0) {
                VcpRedrawArea(Device,
                              FALSE,
                              CursorColumn,
                              CursorRow,
                              CursorColumn + 1,
                              CursorRow);
            }

            BlinkCount += 1;

        //
        // Device I/O error probably means there are no slaves connected. Wait
        // a little while and see if one connects.
        //

        } else if (Status == STATUS_DEVICE_IO_ERROR) {
            KeDelayExecution(FALSE, FALSE, 5 * MICROSECONDS_PER_SECOND);

        } else if (!KSUCCESS(Status)) {
            break;
        }

        if ((BytesRead != 0) && (Device->OpenHandles == 0)) {
            BlinkCount = 0;
            VcpWriteToConsole(Device, ReadBuffer, BytesRead);
        }
    }

LocalTerminalRedrawThread:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("VideoCon: TerminalRedrawThread failure: %d\n", Status);
    }

    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    if (ReadBuffer != NULL) {
        MmFreePagedPool(ReadBuffer);
    }

    return;
}

VOID
VcpWriteToConsole (
    PVIDEO_CONSOLE_DEVICE Console,
    PSTR String,
    UINTN StringLength
    )

/*++

Routine Description:

    This routine writes the given string to the video console.

Arguments:

    Console - Supplies a pointer to the video console to write to.

    String - Supplies a pointer to the string to print.

    StringLength - Supplies the length of the string buffer, including the null
        terminator.

Return Value:

    None.

--*/

{

    UCHAR Character;
    PBASE_VIDEO_CHARACTER Characters;
    LONG Column;
    LONG CursorColumn;
    LONG CursorRow;
    LONG EndColumn;
    LONG EndRow;
    PVIDEO_CONSOLE_LINE Line;
    TERMINAL_PARSE_RESULT OutputResult;
    LONG StartColumn;
    LONG StartRow;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(Console->Lock);
    StartRow = Console->NextRow;
    StartColumn = Console->NextColumn;
    if (StartColumn == Console->Columns) {
        StartColumn -= 1;
    }

    EndColumn = StartColumn;
    EndRow = StartRow;

    ASSERT(StartColumn < Console->Columns);
    ASSERT(StartRow < Console->ScreenRows);

    //
    // Clear the cursor flag assuming it's going to move.
    //

    Line = GET_CONSOLE_LINE(Console, StartRow);
    Characters = (PBASE_VIDEO_CHARACTER)(Line->Character);
    Characters[StartColumn].Data.Attributes &= ~BASE_VIDEO_CURSOR;

    //
    // Loop over each character in the string.
    //

    while (TRUE) {
        if (StringLength == 0) {
            break;
        }

        Character = *String;
        if (Character == '\0') {
            String += 1;
            StringLength -= 1;
            continue;
        }

        OutputResult = TermProcessOutput(&(Console->Command), Character);
        switch (OutputResult) {

        //
        // This is just an ordinary joe character.
        //

        case TerminalParseResultNormalCharacter:
            Console->PendingAction |= VIDEO_ACTION_RESET_SCROLL;
            if (Character == '\t') {
                VcpMoveCursorTabStops(Console, 1);

            //
            // A newline, vertical time, or form feed moves to the next line,
            // and potentially resets the column too.
            //

            } else if ((Character == '\n') || (Character == '\v') ||
                       (Character == '\f')) {

                if ((Console->NextColumn == Console->Columns) ||
                    ((Console->Mode & CONSOLE_MODE_NEW_LINE) != 0)) {

                    Console->NextColumn = 0;
                }

                VcpAdvanceRow(Console);
                Line = NULL;

            //
            // Handle a carriage return.
            //

            } else if (Character == '\r') {
                Console->NextColumn = 0;

            //
            // Handle a backspace.
            //

            } else if (Character == '\b') {
                if (Console->NextColumn != 0) {
                    Console->NextColumn -= 1;

                } else {
                    if (Console->NextRow != 0) {
                        Console->NextColumn = Console->Columns - 1;
                        Console->NextRow -= 1;
                        Line = NULL;
                    }
                }

            //
            // Handle a rubout, which moves the cursor back one and erases the
            // character at that new position. It does not go back up lines.
            //

            } else if (Character == TERMINAL_RUBOUT) {
                if (Console->NextColumn != 0) {
                    Console->NextColumn -= 1;
                }

                if (Line == NULL) {
                    CursorRow = Console->NextRow;
                    Line = GET_CONSOLE_LINE(Console, CursorRow);
                    Characters = (PBASE_VIDEO_CHARACTER)(Line->Character);
                }

                Characters[Console->NextColumn].Data.Character = ' ';

            } else if ((Character >= ' ') && (Character < 0x80)) {
                if (Line == NULL) {
                    CursorRow = Console->NextRow;
                    Line = GET_CONSOLE_LINE(Console, CursorRow);
                    Characters = (PBASE_VIDEO_CHARACTER)(Line->Character);
                }

                if ((Console->Mode & CONSOLE_MODE_INSERT) != 0) {
                    for (Column = Console->Columns - 1;
                         Column > Console->NextColumn;
                         Column -= 1) {

                        Characters[Column].AsUint32 =
                                               Characters[Column - 1].AsUint32;
                    }

                    if (EndRow == Console->NextRow) {
                        EndColumn = Console->Columns - 1;
                    }
                }

                //
                // If the column was actually overhanging, move it down now.
                //

                if (Console->NextColumn == Console->Columns) {
                    Console->NextColumn = 0;
                    VcpAdvanceRow(Console);
                    Line = GET_CONSOLE_LINE(Console, Console->NextRow);
                    Characters = (PBASE_VIDEO_CHARACTER)(Line->Character);
                }

                Characters[Console->NextColumn].Data.Attributes =
                                                       Console->TextAttributes;

                Characters[Console->NextColumn].Data.Character = Character;

                //
                // Move the column forward.
                //

                if ((Console->Mode & CONSOLE_MODE_AUTO_WRAP) != 0) {
                    if (Console->NextColumn < Console->Columns) {
                        Console->NextColumn += 1;
                    }

                } else if (Console->NextColumn < Console->Columns - 1) {
                    Console->NextColumn += 1;
                }

            } else if (Character == VIDEO_CHARACTER_SHIFT_IN) {

                //
                // TODO: Handle shift in, which invokes the G0 character set.
                //

            } else if (Character == VIDEO_CHARACTER_SHIFT_OUT) {

                //
                // TODO: Handle shift out, which invokes the G1 character set.
                //

            }

            break;

        case TerminalParseResultPartialCommand:
            break;

        case TerminalParseResultCompleteCommand:
            TermNormalizeParameters(&(Console->Command));
            VcpProcessCommand(Console, &(Console->Command));
            Line = NULL;
            break;

        default:

            ASSERT(FALSE);

            break;
        }

        //
        // Potentially widen the redraw area unless a scroll has already
        // occurred, in which case the entire screen will be redrawn anyway.
        //

        if ((Console->PendingAction & VIDEO_ACTION_REDRAW_ENTIRE_SCREEN) == 0) {

            //
            // Potentially move the end region out.
            //

            if (Console->NextRow > EndRow) {
                EndRow = Console->NextRow;
                EndColumn = Console->NextColumn;

            } else if ((Console->NextRow == EndRow) &&
                       (Console->NextColumn > EndColumn)) {

                EndColumn = Console->NextColumn;
            }

            //
            // Potentially move the start region.
            //

            if (Console->NextRow < StartRow) {
                StartRow = Console->NextRow;
                StartColumn = Console->NextColumn;

            } else if ((Console->NextRow == StartRow) &&
                       (Console->NextColumn < StartColumn)) {

                StartColumn = Console->NextColumn;
            }
        }

        //
        // Move on to the next character.
        //

        String += 1;
        StringLength -= 1;
    }

    //
    // Make the cursor visible on any real events.
    //

    if ((Console->PendingAction & VIDEO_ACTION_RESET_SCROLL) != 0) {
        Console->PendingAction &= ~VIDEO_ACTION_RESET_SCROLL;
        if ((Console->RowViewOffset > Console->NextRow) ||
            (Console->RowViewOffset + Console->ScreenRows < Console->NextRow)) {

            Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
            Console->RowViewOffset = 0;
        }
    }

    if ((Console->PendingAction & VIDEO_ACTION_REDRAW_ENTIRE_SCREEN) != 0) {
        Console->PendingAction &= ~VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
        StartColumn = 0;
        StartRow = 0;
        EndColumn = Console->Columns;
        EndRow = Console->ScreenRows - 1;

    //
    // Add one extra for the cursor, and adjust for the row view offset.
    //

    } else {
        EndColumn += 1;
        if (EndColumn > Console->Columns) {
            EndColumn = Console->Columns;
        }

        StartRow -= Console->RowViewOffset;
        if (StartRow < 0) {
            StartRow = 0;

        } else if (StartRow > Console->ScreenRows - 1) {
            StartRow = Console->ScreenRows - 1;
        }

        EndRow -= Console->RowViewOffset;
        if (EndRow < 0) {
            EndRow = 0;

        } else if (EndRow > Console->ScreenRows - 1) {
            EndRow = Console->ScreenRows - 1;
        }

        if ((EndRow == StartRow) && (EndColumn < StartColumn)) {
            EndColumn = StartColumn;
        }
    }

    //
    // Set the cursor character.
    //

    CursorRow = Console->NextRow;
    CursorColumn = Console->NextColumn;
    if (CursorColumn == Console->Columns) {
        CursorColumn -= 1;
    }

    Line = GET_CONSOLE_LINE(Console, CursorRow);
    Characters = (PBASE_VIDEO_CHARACTER)(Line->Character);
    if ((Console->Mode & CONSOLE_MODE_CURSOR) != 0) {
        Characters[CursorColumn].Data.Attributes |= BASE_VIDEO_CURSOR;
    }

    //
    // Redraw the portion of the screen that was modified.
    //

    VcpRedrawArea(Console, FALSE, StartColumn, StartRow, EndColumn, EndRow);
    KeReleaseQueuedLock(Console->Lock);
    return;
}

VOID
VcpProcessCommand (
    PVIDEO_CONSOLE_DEVICE Console,
    PTERMINAL_COMMAND_DATA Command
    )

/*++

Routine Description:

    This routine processes a terminal control sequence.

Arguments:

    Console - Supplies a pointer to the video console.

    Command - Supplies a pointer to the command to run.

Return Value:

    None.

--*/

{

    LONG Bottom;
    LONG Column;
    LONG Count;
    PVIDEO_CONSOLE_LINE Line;
    UINTN ParameterIndex;
    BOOL ResetCharacterAttributes;
    LONG Row;
    LONG Top;

    //
    // For the purposes of handling a command, the console cannot be
    // overhanging.
    //

    if (Console->NextColumn == Console->Columns) {
        Console->NextColumn -= 1;
    }

    switch (Command->Command) {
    case TerminalCommandInvalid:

        ASSERT(FALSE);

        break;

    case TerminalCommandCursorUp:
        Count = Command->Parameter[0];

        ASSERT((Command->ParameterCount != 0) && (Count > 0));

        VcpMoveCursorRelative(Console, 0, -Count);
        break;

    case TerminalCommandCursorDown:
        Count = Command->Parameter[0];

        ASSERT((Command->ParameterCount != 0) && (Count > 0) &&
               (Console->NextRow <= Console->ScreenRows - 1));

        VcpMoveCursorRelative(Console, 0, Count);
        break;

    case TerminalCommandCursorLeft:
        Count = Command->Parameter[0];

        ASSERT((Command->ParameterCount != 0) && (Count > 0));

        VcpMoveCursorRelative(Console, -Count, 0);
        break;

    case TerminalCommandCursorRight:
        Count = Command->Parameter[0];

        ASSERT((Command->ParameterCount != 0) && (Count > 0));
        ASSERT(Console->NextColumn < Console->Columns);

        VcpMoveCursorRelative(Console, Count, 0);
        break;

    case TerminalCommandSetCursorRowAbsolute:
        Row = Command->Parameter[0];

        ASSERT((Command->ParameterCount != 0) && (Row > 0));

        Row -= 1;
        VcpMoveCursorAbsolute(Console, Console->NextColumn, Row, TRUE);
        break;

    case TerminalCommandSetCursorColumnAbsolute:
        Column = Command->Parameter[0];

        ASSERT((Command->ParameterCount != 0) && (Column > 0));

        Column -= 1;
        VcpMoveCursorAbsolute(Console, Column, Console->NextRow, FALSE);
        break;

    case TerminalCommandCursorMove:
        Column = Command->Parameter[1];
        Row = Command->Parameter[0];

        ASSERT((Command->ParameterCount == 2) && (Column > 0) && (Row > 0));

        Column -= 1;
        Row -= 1;
        VcpMoveCursorAbsolute(Console, Column, Row, TRUE);
        break;

    case TerminalCommandNextLine:
        Console->NextColumn = 0;
        VcpAdvanceRow(Console);
        break;

    case TerminalCommandReverseLineFeed:
        if (Console->NextRow < Console->TopMargin) {
            if (Console->NextRow != 0) {
                Console->NextRow -= 1;
            }

        } else if (Console->NextRow == Console->TopMargin) {
            VcpInsertLines(Console, 1, Console->NextRow);

        } else {

            ASSERT(Console->NextRow > 0);

            Console->NextRow -= 1;
        }

        break;

    case TerminalCommandSaveCursorAndAttributes:
        VcpSaveRestoreCursor(Console, TRUE);
        break;

    case TerminalCommandRestoreCursorAndAttributes:
        VcpSaveRestoreCursor(Console, FALSE);
        break;

    case TerminalCommandSetHorizontalTab:
        if (Console->NextColumn < Console->Columns) {
            SET_TAB_STOP(Console, Console->NextColumn);
        }

        break;

    case TerminalCommandClearHorizontalTab:
        if (Command->Parameter[0] == 3) {
            CLEAR_ALL_TAB_STOPS(Console);

        } else {
            if (Console->NextColumn < Console->Columns) {
                CLEAR_TAB_STOP(Console, Console->NextColumn);
            }
        }

        break;

    case TerminalCommandSetTopAndBottomMargin:
        Top = 1;
        Bottom = Console->ScreenRows;
        if (Command->ParameterCount > 0) {
            if ((Command->Parameter[0] != 0) &&
                (Command->Parameter[0] <= Console->ScreenRows)) {

                Top = Command->Parameter[0];
            }

            if ((Command->ParameterCount > 1) && (Command->Parameter[1] != 0) &&
                (Command->Parameter[1] <= Console->ScreenRows)) {

                Bottom = Command->Parameter[1];
            }
        }

        if (Top < Bottom) {

            ASSERT((Top > 0) && (Top <= Console->ScreenRows) &&
                   (Bottom > Top) && (Bottom <= Console->ScreenRows));

            Console->TopMargin = Top - 1;
            Console->BottomMargin = Console->ScreenRows - Bottom;
        }

        Console->NextColumn = 0;
        Console->NextRow = 0;
        if ((Console->Mode & CONSOLE_MODE_ORIGIN) != 0) {
            Console->NextRow += Console->TopMargin;
        }

        break;

    case TerminalCommandEraseInDisplay:
    case TerminalCommandEraseInDisplaySelective:
        ResetCharacterAttributes = TRUE;
        if (Command->Command == TerminalCommandEraseInDisplaySelective) {
            ResetCharacterAttributes = FALSE;
        }

        //
        // For no parameter or zero, erase from the cursor to the end of the
        // screen, including the cursor.
        //

        if ((Command->ParameterCount == 0) || (Command->Parameter[0] == 0)) {
            VcpEraseArea(Console,
                         Console->NextColumn,
                         Console->NextRow,
                         Console->Columns - 1,
                         Console->ScreenRows - 1,
                         ResetCharacterAttributes);

        //
        // If the parameter is 1, erase from the top of the screen to the
        // current cursor, including the cursor.
        //

        } else if (Command->Parameter[0] == 1) {
            VcpEraseArea(Console,
                         0,
                         0,
                         Console->NextColumn,
                         Console->NextRow,
                         ResetCharacterAttributes);

        //
        // If the parameter is 2, erase the entire display.
        //

        } else if (Command->Parameter[0] == 2) {
            VcpEraseArea(Console,
                         0,
                         0,
                         Console->Columns - 1,
                         Console->ScreenRows - 1,
                         ResetCharacterAttributes);
        }

        break;

    case TerminalCommandEraseInLine:
    case TerminalCommandEraseInLineSelective:
        ResetCharacterAttributes = TRUE;
        if (Command->Command == TerminalCommandEraseInLineSelective) {
            ResetCharacterAttributes = FALSE;
        }

        //
        // For no parameters or zero, erase from the cursor to the end of the
        // line, including the cursor.
        //

        if ((Command->ParameterCount == 0) || (Command->Parameter[0] == 0)) {
            VcpEraseArea(Console,
                         Console->NextColumn,
                         Console->NextRow,
                         Console->Columns - 1,
                         Console->NextRow,
                         ResetCharacterAttributes);

        //
        // Erase from the beginning of the line to the cursor, including the
        // cursor.
        //

        } else if (Command->Parameter[0] == 1) {
            VcpEraseArea(Console,
                         0,
                         Console->NextRow,
                         Console->NextColumn,
                         Console->NextRow,
                         ResetCharacterAttributes);

        } else if (Command->Parameter[0] == 2) {
            VcpEraseArea(Console,
                         0,
                         Console->NextRow,
                         Console->Columns - 1,
                         Console->NextRow,
                         ResetCharacterAttributes);
        }

        break;

    case TerminalCommandInsertLines:
        Count = 1;
        if ((Command->ParameterCount != 0) && (Command->Parameter[0] > 0)) {
            Count = Command->Parameter[0];
        }

        //
        // If the cursor is outside the scroll area.
        //

        if (!CURSOR_IN_SCROLL_REGION(Console)) {
            break;
        }

        Console->NextColumn = 0;
        VcpInsertLines(Console, Count, Console->NextRow);
        break;

    case TerminalCommandDeleteLines:
        Count = 1;
        if ((Command->ParameterCount != 0) && (Command->Parameter[0] > 0)) {
            Count = Command->Parameter[0];
        }

        //
        // If the cursor is outside the scroll area or at the very bottom of it,
        // this command is ignored.
        //

        if (!CURSOR_IN_SCROLL_REGION(Console)) {
            break;
        }

        Console->NextColumn = 0;
        if (Console->NextRow ==
            Console->ScreenRows - 1 - Console->BottomMargin) {

            break;
        }

        VcpDeleteLines(Console, Count, Console->NextRow);
        break;

    case TerminalCommandInsertCharacters:
        Count = 1;
        if ((Command->ParameterCount != 0) && (Command->Parameter[0] != 0)) {
            Count = Command->Parameter[0];
        }

        if (Count > Console->Columns - Console->NextColumn) {
            Count = Console->Columns - Console->NextColumn;
        }

        //
        // If insert mode is set, shift the remaining characters out.
        //

        Line = GET_CONSOLE_LINE(Console, Console->NextRow);
        for (Column = Console->Columns - 1;
             Column >= Console->NextColumn + Count;
             Column -= 1) {

            Line->Character[Column].AsUint32 =
                                      Line->Character[Column - Count].AsUint32;
        }

        RtlZeroMemory(&(Line->Character[Console->NextColumn]),
                      Count * sizeof(BASE_VIDEO_CHARACTER));

        Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
        break;

    case TerminalCommandDeleteCharacters:
        Count = 1;
        if ((Command->ParameterCount != 0) && (Command->Parameter[0] != 0)) {
            Count = Command->Parameter[0];
        }

        if (Count > Console->Columns - Console->NextColumn) {
            Count = Console->Columns - Console->NextColumn;
        }

        //
        // Move the remaining characters backwards.
        //

        Line = GET_CONSOLE_LINE(Console, Console->NextRow);
        for (Column = Console->NextColumn;
             Column < Console->Columns - Count;
             Column += 1) {

            Line->Character[Column].AsUint32 =
                                      Line->Character[Column + Count].AsUint32;
        }

        //
        // Clear out the space at the right.
        //

        RtlZeroMemory(&(Line->Character[Console->Columns - Count]),
                      Count * sizeof(BASE_VIDEO_CHARACTER));

        Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
        break;

    case TerminalCommandEraseCharacters:
        Count = 1;
        if ((Command->ParameterCount != 0) && (Command->Parameter[0] != 0)) {
            Count = Command->Parameter[0];
        }

        if (Count > Console->Columns - Console->NextColumn) {
            Count = Console->Columns - Console->NextColumn;
        }

        //
        // Erase characters starting at the cursor without shifting the line
        // contents.
        //

        VcpEraseArea(Console,
                     Console->NextColumn,
                     Console->NextRow,
                     Console->NextColumn + Count - 1,
                     Console->NextRow,
                     TRUE);

        break;

    case TerminalCommandKeypadNumeric:
    case TerminalCommandKeypadApplication:
        break;

    case TerminalCommandSetMode:
    case TerminalCommandClearMode:
    case TerminalCommandSetPrivateMode:
    case TerminalCommandClearPrivateMode:
        for (ParameterIndex = 0;
             ParameterIndex < Command->ParameterCount;
             ParameterIndex += 1) {

            VcpSetOrClearMode(Console,
                              Command->Parameter[ParameterIndex],
                              Command->Command);
        }

        break;

    case TerminalCommandSelectG0CharacterSet:
    case TerminalCommandSelectG1CharacterSet:
    case TerminalCommandSelectG2CharacterSet:
    case TerminalCommandSelectG3CharacterSet:
        break;

    case TerminalCommandSelectGraphicRendition:
        VcpSetColorFromParameters(Console, Command);
        break;

    case TerminalCommandReset:
    case TerminalCommandSoftReset:
        Console->TextAttributes = 0;
        Console->NextRow = 0;
        Console->NextColumn = 0;
        Console->Mode = VIDEO_CONSOLE_MODE_DEFAULTS;
        Console->TopMargin = 0;
        Console->BottomMargin = 0;
        VcpEraseArea(Console,
                     0,
                     0,
                     Console->Columns - 1,
                     Console->ScreenRows - 1,
                     TRUE);

        break;

    case TerminalCommandDeviceAttributesPrimary:
    case TerminalCommandDeviceAttributesSecondary:
        break;

    case TerminalCommandScrollUp:
        Count = Command->Parameter[0];
        if ((Command->ParameterCount == 0) || (Count <= 0)) {
            Count = 1;
        }

        if (Console->TopMargin == 0) {
            Console->RowViewOffset -= Count;

        } else {
            VcpDeleteLines(Console, Count, Console->TopMargin);
        }

        Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
        break;

    case TerminalCommandScrollDown:
        Count = Command->Parameter[0];
        if ((Command->ParameterCount == 0) || (Count <= 0)) {
            Count = 1;
        }

        if (Console->TopMargin == 0) {
            Console->RowViewOffset += Count;

        } else {
            VcpInsertLines(Console, Count, Console->TopMargin);
        }

        Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
        break;

    case TerminalCommandCursorForwardTabStops:
        VcpMoveCursorTabStops(Console, Command->Parameter[0]);
        break;

    case TerminalCommandCursorBackwardTabStops:
        VcpMoveCursorTabStops(Console, -(Command->Parameter[0]));
        break;

    case TerminalCommandDoubleLineHeightTopHalf:
    case TerminalCommandDoubleLineHeightBottomHalf:
    case TerminalCommandSingleWidthLine:
    case TerminalCommandDoubleWidthLine:
        break;

    //
    // Do nothing for unknown commands.
    //

    default:
        break;
    }

    return;
}

VOID
VcpEraseArea (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG StartColumn,
    LONG StartRow,
    LONG EndColumn,
    LONG EndRow,
    BOOL ResetAttributes
    )

/*++

Routine Description:

    This routine erases a portion of the screen.

Arguments:

    Console - Supplies a pointer to the video console to erase.

    StartColumn - Supplies the starting column, inclusive, to erase.

    StartRow - Supplies the starting row, inclusive, to erase.

    EndColumn - Supplies the ending column, inclusive, to erase.

    EndRow - Supplies the ending row, inclusive, to erase.

    ResetAttributes - Supplies a boolean indicating if the attributes should be
        reset to zero as well or left alone.

Return Value:

    None.

--*/

{

    BOOL Blank;
    LONG Column;
    LONG EndColumnThisRow;
    PVIDEO_CONSOLE_LINE Line;
    LONG LineCount;
    LONG Row;
    LONG SavedBottomMargin;
    LONG SavedTopMargin;

    ASSERT((EndColumn < Console->Columns) && (EndRow < Console->ScreenRows));

    Console->RowViewOffset = 0;
    Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
    if (StartColumn == Console->Columns) {
        StartColumn -= 1;
    }

    if (EndColumn == Console->Columns) {
        EndColumn -= 1;
    }

    //
    // If erasing the whole screen, then actually scroll up until the screen
    // is blank.
    //

    if ((ResetAttributes != FALSE) && (StartColumn == 0) && (StartRow == 0) &&
        (EndColumn == Console->Columns - 1) &&
        (EndRow == Console->ScreenRows - 1)) {

        //
        // Find the last non-blank line.
        //

        for (Row = Console->ScreenRows - 1; Row >= 0; Row -= 1) {
            Blank = TRUE;
            Line = GET_CONSOLE_LINE(Console, Row);
            for (Column = 0; Column < Console->Columns; Column += 1) {
                if ((Line->Character[Column].Data.Character != 0) &&
                    (Line->Character[Column].Data.Character != ' ')) {

                    Blank = FALSE;
                    break;
                }

                if (Line->Character[Column].Data.Attributes != 0) {
                    Blank = FALSE;
                    break;
                }
            }

            if (Blank == FALSE) {
                break;
            }
        }

        //
        // Scroll up by the number of non-blank lines.
        //

        LineCount = Row + 1;
        Row = Console->NextRow;
        SavedTopMargin = Console->TopMargin;
        SavedBottomMargin = Console->BottomMargin;
        Console->NextRow = Console->ScreenRows - 1;
        Console->TopMargin = 0;
        Console->BottomMargin = 0;
        while (LineCount != 0) {
            VcpAdvanceRow(Console);
            LineCount -= 1;
        }

        Console->NextRow = Row;
        Console->TopMargin = SavedTopMargin;
        Console->BottomMargin = SavedBottomMargin;
        return;
    }

    //
    // Really erase the given region, rather than just scrolling up.
    //

    for (Row = StartRow; Row <= EndRow; Row += 1) {
        Line = GET_CONSOLE_LINE(Console, Row);
        Column = 0;
        if (Row == StartRow) {
            Column = StartColumn;
        }

        EndColumnThisRow = Console->Columns - 1;
        if (Row == EndRow) {
            EndColumnThisRow = EndColumn;
        }

        ASSERT(Column <= EndColumnThisRow);

        if (ResetAttributes != FALSE) {
            while (Column <= EndColumnThisRow) {
                Line->Character[Column].Data.Character = ' ';
                Line->Character[Column].Data.Attributes =
                                                       Console->TextAttributes;

                Column += 1;
            }

        } else {
            while (Column <= EndColumnThisRow) {
                Line->Character[Column].Data.Character = ' ';
                Column += 1;
            }
        }
    }

    return;
}

VOID
VcpSetOrClearMode (
    PVIDEO_CONSOLE_DEVICE Console,
    ULONG ModeNumber,
    TERMINAL_COMMAND Command
    )

/*++

Routine Description:

    This routine sets or clears a console mode setting.

Arguments:

    Console - Supplies a pointer to the video console to alter.

    ModeNumber - Supplies the mode number to set or clear.

    Command - Supplies the command number.

Return Value:

    None.

--*/

{

    ULONG Mask;
    BOOL Set;

    Mask = 0;
    Set = FALSE;
    if ((Command == TerminalCommandSetMode) ||
        (Command == TerminalCommandSetPrivateMode)) {

        Set = TRUE;
    }

    if ((Command == TerminalCommandSetMode) ||
        (Command == TerminalCommandClearMode)) {

        switch (ModeNumber) {
        case TERMINAL_MODE_KEYBOARD_LOCKED:
            Mask = CONSOLE_MODE_KEYBOARD_ACTION;
            break;

        case TERMINAL_MODE_INSERT:
            Mask = CONSOLE_MODE_INSERT;
            break;

        case TERMINAL_MODE_DISABLE_LOCAL_ECHO:
            Mask = CONSOLE_MODE_DISABLE_LOCAL_ECHO;
            break;

        case TERMINAL_MODE_NEW_LINE:
            Mask = CONSOLE_MODE_NEW_LINE;
            break;

        default:
            break;
        }

    } else {

        ASSERT((Command == TerminalCommandSetPrivateMode) ||
               (Command == TerminalCommandClearPrivateMode));

        switch (ModeNumber) {
        case TERMINAL_PRIVATE_MODE_APPLICATION_CURSOR_KEYS:
            Mask = CONSOLE_MODE_APPLICATION_CURSOR_KEYS;
            break;

        case TERMINAL_PRIVATE_MODE_VT52:
            Mask = CONSOLE_MODE_VT52;
            break;

        case TERMINAL_PRIVATE_MODE_132_COLUMNS:
            Mask = CONSOLE_MODE_132_COLUMN;
            break;

        case TERMINAL_PRIVATE_MODE_SMOOTH_SCROLLING:
            Mask = CONSOLE_MODE_KEYBOARD_ACTION;
            break;

        case TERMINAL_PRIVATE_MODE_REVERSE_VIDEO:
            Mask = CONSOLE_MODE_VIDEO_REVERSED;
            Console->PendingAction = VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
            break;

        case TERMINAL_PRIVATE_MODE_ORIGIN:
            Mask = CONSOLE_MODE_ORIGIN;
            break;

        case TERMINAL_PRIVATE_MODE_AUTO_WRAP:
            Mask = CONSOLE_MODE_AUTO_WRAP;
            break;

        case TERMINAL_PRIVATE_MODE_BLINKING_CURSOR:
            Mask = CONSOLE_MODE_CURSOR_BLINK;
            break;

        case TERMINAL_PRIVATE_MODE_CURSOR:
            Mask = CONSOLE_MODE_CURSOR;
            break;

        case TERMINAL_PRIVATE_MODE_SAVE_CURSOR:
            VcpSaveRestoreCursor(Console, Set);
            break;

        case TERMINAL_PRIVATE_MODE_ALTERNATE_SCREEN_SAVE_CURSOR:
            VcpSaveRestoreCursor(Console, Set);

            //
            // Erase the screen in lieu of a keeping secondary screen buffer.
            //

            VcpEraseArea(Console,
                         0,
                         0,
                         Console->Columns - 1,
                         Console->ScreenRows - 1,
                         TRUE);

            Console->TopMargin = 0;
            Console->BottomMargin = 0;
            break;

        case TERMINAL_PRIVATE_MODE_AUTO_REPEAT:
        case TERMINAL_PRIVATE_MODE_FORM_FEED:
        case TERMINAL_PRIVATE_MODE_PRINT_FULL_SCREEN:
        case TERMINAL_PRIVATE_MODE_NATIONAL:
        case TERMINAL_PRIVATE_MODE_ALTERNATE_SCREEN:
            break;
        }
    }

    if (Set != FALSE) {
        Console->Mode |= Mask;

    } else {
        Console->Mode &= ~Mask;
    }

    return;
}

VOID
VcpRedrawArea (
    PVIDEO_CONSOLE_DEVICE Console,
    BOOL Force,
    LONG StartColumn,
    LONG StartRow,
    LONG EndColumn,
    LONG EndRow
    )

/*++

Routine Description:

    This routine redraws a portion of the screen.

Arguments:

    Console - Supplies a pointer to the video console to write to.

    Force - Supplies a boolean indicating whether or not to redraw characters
        even if supposedly unnecessary. Set this if someone else has drawn on
        the frame buffer.

    StartColumn - Supplies the starting column, inclusive, to redraw at.

    StartRow - Supplies the starting row, inclusive, to redraw at.

    EndColumn - Supplies the ending column, exclusive, to redraw until.

    EndRow - Supplies the ending row, inclusive, to redraw until.

Return Value:

    None.

--*/

{

    BASE_VIDEO_CHARACTER Blank;
    LONG BufferRow;
    PBASE_VIDEO_CHARACTER Characters;
    LONG CurrentColumn;
    LONG CurrentRow;
    LONG EndColumnThisRow;
    LONG Height;
    PVIDEO_CONSOLE_LINE Line;
    LONG Remainder;
    PBASE_VIDEO_CHARACTER ScreenCharacters;
    PVIDEO_CONSOLE_LINE ScreenLine;
    LONG StartDrawColumn;
    LONG Width;

    CurrentColumn = StartColumn;
    CurrentRow = StartRow;
    Width = Console->Columns;
    Blank.Data.Attributes = Console->TextAttributes;
    Blank.Data.Character = ' ';

    ASSERT((StartColumn <= Console->Columns) &&
           (EndColumn <= Console->Columns));

    ASSERT((StartRow < Console->ScreenRows) && (EndRow < Console->ScreenRows));

    if (StartColumn >= Console->Columns) {
        StartColumn = Console->Columns - 1;
    }

    //
    // Loop through each row on the screen.
    //

    while (TRUE) {

        //
        // Get the line associated with this row. If the offset plus the
        // current row is greater than the screen size, this is an empty row.
        //

        if (CurrentRow + Console->RowViewOffset >= Console->ScreenRows) {
            Line = NULL;

        //
        // The current row plus the offset also needs to be greater than the
        // bottom of the screen (otherwise the bottom of the screen would show
        // up again if scrolled far enough up).
        //

        } else if (CurrentRow + Console->RowViewOffset <
                   -(Console->BufferRows - Console->ScreenRows)) {

            Line = NULL;

        //
        // The offset is reasonable enough that there's a line associated with
        // it. Go find that line. The macro can't be used here because of the
        // potential for the buffer row to go negative during the calculation.
        //

        } else {
            BufferRow = Console->TopLine + CurrentRow + Console->RowViewOffset;
            if (BufferRow >= Console->BufferRows) {
                BufferRow -= Console->BufferRows;

            } else if (BufferRow < 0) {
                BufferRow += Console->BufferRows;
            }

            ASSERT((BufferRow >= 0) && (BufferRow < Console->BufferRows));

            Line = (PVIDEO_CONSOLE_LINE)(Console->Lines +
                                         (CONSOLE_LINE_SIZE(Console) *
                                          BufferRow));
        }

        //
        // Figure out the ending column for this row.
        //

        if (CurrentRow == EndRow) {
            EndColumnThisRow = EndColumn;

        } else {
            EndColumnThisRow = Width;
        }

        ScreenLine = (PVIDEO_CONSOLE_LINE)(Console->Screen +
                                           (CONSOLE_LINE_SIZE(Console) *
                                            CurrentRow));

        ScreenCharacters = ScreenLine->Character;
        if (Line != NULL) {
            Characters = Line->Character;

            //
            // Line attributes need support here if they're implemented.
            //

            ASSERT(ScreenLine->Attributes == Line->Attributes);

            while (CurrentColumn < EndColumnThisRow) {

                //
                // Skip characters that are already drawn correctly.
                //

                if (Force == FALSE) {
                    if (ScreenCharacters[CurrentColumn].AsUint32 ==
                        Characters[CurrentColumn].AsUint32) {

                        CurrentColumn += 1;
                        continue;
                    }

                    //
                    // Collect characters that need redrawing.
                    //

                    StartDrawColumn = CurrentColumn;
                    while ((CurrentColumn < EndColumnThisRow) &&
                           (ScreenCharacters[CurrentColumn].AsUint32 !=
                            Characters[CurrentColumn].AsUint32)) {

                        ScreenCharacters[CurrentColumn].AsUint32 =
                                            Characters[CurrentColumn].AsUint32;

                        CurrentColumn += 1;
                    }

                //
                // Redraw the whole row.
                //

                } else {
                    StartDrawColumn = CurrentColumn;
                    CurrentColumn = EndColumnThisRow;
                }

                VidPrintCharacters(&(Console->VideoContext),
                                   StartDrawColumn,
                                   CurrentRow,
                                   &(ScreenCharacters[StartDrawColumn]),
                                   CurrentColumn - StartDrawColumn);
            }

        } else {
            while (CurrentColumn < EndColumnThisRow) {

                //
                // Skip characters that are already blank.
                //

                if (Force == FALSE) {
                    if (ScreenCharacters[CurrentColumn].AsUint32 ==
                        Blank.AsUint32) {

                        CurrentColumn += 1;
                        continue;
                    }

                    //
                    // Batch together characters that need redrawing.
                    //

                    StartDrawColumn = CurrentColumn;
                    while ((CurrentColumn < EndColumnThisRow) &&
                           (ScreenCharacters[CurrentColumn].AsUint32 !=
                            Blank.AsUint32)) {

                        ScreenCharacters[CurrentColumn] = Blank;
                        CurrentColumn += 1;
                    }

                //
                // Redraw the whole row.
                //

                } else {
                    StartDrawColumn = CurrentColumn;
                    CurrentColumn = EndColumnThisRow;
                }

                VidPrintCharacters(&(Console->VideoContext),
                                   StartDrawColumn,
                                   CurrentRow,
                                   &(ScreenCharacters[StartDrawColumn]),
                                   CurrentColumn - StartDrawColumn);
            }
        }

        //
        // Potentially break if this was the last row.
        //

        if (CurrentRow == EndRow) {
            break;
        }

        //
        // On to the next row.
        //

        CurrentColumn = 0;
        CurrentRow += 1;
    }

    //
    // If clearing the whole screen, also clear any remainder along the right
    // and bottom edges that doesn't divide evenly by text cell.
    //

    if ((Force != FALSE) &&
        (StartRow == 0) && (StartColumn == 0) &&
        (EndRow >= Console->ScreenRows - 1) &&
        (EndColumn >= Console->Columns - 1)) {

        Width = Console->VideoContext.Width;
        Height = Console->VideoContext.Height;
        Remainder = Console->Columns * Console->VideoContext.Font->CellWidth;
        if (Remainder < Width) {
            VidClearScreen(&(Console->VideoContext),
                           Remainder,
                           0,
                           Width,
                           Height);
        }

        Remainder = Console->ScreenRows *
                    Console->VideoContext.Font->CellHeight;

        if (Remainder < Height) {
            VidClearScreen(&(Console->VideoContext),
                           0,
                           Remainder,
                           Width,
                           Height);
        }
    }

    return;
}

VOID
VcpAdvanceRow (
    PVIDEO_CONSOLE_DEVICE Console
    )

/*++

Routine Description:

    This routine move the console's "next row" up by one (visually down to the
    next row).

Arguments:

    Console - Supplies a pointer to the video console to write to.

    ScreenRedrawNeeded - Supplies a pointer where a boolean will be returned
        indicating if the entire screen needs to be redrawn. If not, this
        boolean will be left uninitialized. If so, this boolean will be set to
        TRUE.

Return Value:

    None.

--*/

{

    UINTN LineSize;
    ULONG NewAllocationSize;
    PVIDEO_CONSOLE_LINE NewLastLine;
    PVOID NewLines;
    LONG NewRowCount;
    ULONG OriginalSize;
    LONG Row;

    //
    // It's really easy if there are still extra rows on the screen to be
    // used.
    //

    if (Console->NextRow < Console->ScreenRows - 1 - Console->BottomMargin) {
        Console->NextRow += 1;
        return;
    }

    Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;

    //
    // If the cursor made it beyond the bottom of the scroll area, then allow
    // movement towards the bottom of the screen. Don't scroll beyond that.
    //

    if (Console->NextRow > Console->ScreenRows - 1 - Console->BottomMargin) {
        if (Console->NextRow < Console->ScreenRows - 1) {
            Console->NextRow += 1;
        }

        return;
    }

    //
    // If the bottom console line is also the bottom buffer line, look into
    // expanding the buffer.
    //

    if ((Console->TopLine + Console->ScreenRows == Console->BufferRows) &&
        ((Console->BufferRows < Console->MaxRows) ||
         (Console->MaxRows == 0))) {

        NewRowCount = Console->BufferRows * 2;
        if ((Console->MaxRows != 0) && (NewRowCount > Console->MaxRows)) {
            NewRowCount = Console->MaxRows;
        }

        ASSERT(NewRowCount > Console->BufferRows);

        NewAllocationSize = CONSOLE_LINE_SIZE(Console) * NewRowCount;
        NewLines = MmAllocatePagedPool(NewAllocationSize,
                                       VIDEO_CONSOLE_ALLOCATION_TAG);

        if (NewLines != NULL) {
            OriginalSize = CONSOLE_LINE_SIZE(Console) * Console->BufferRows;
            RtlCopyMemory(NewLines, Console->Lines, OriginalSize);
            RtlZeroMemory(NewLines + OriginalSize,
                          NewAllocationSize - OriginalSize);

            MmFreePagedPool(Console->Lines);
            Console->Lines = NewLines;
            Console->BufferRows = NewRowCount;
        }
    }

    LineSize = CONSOLE_LINE_SIZE(Console);

    //
    // If there's a top margin, then actually perform the scroll by copying
    // the lines up.
    //

    if (Console->TopMargin != 0) {
        for (Row = Console->TopMargin;
             Row < Console->ScreenRows - Console->BottomMargin - 1;
             Row += 1) {

            RtlCopyMemory(GET_CONSOLE_LINE(Console, Row),
                          GET_CONSOLE_LINE(Console, Row + 1),
                          LineSize);
        }

        RtlZeroMemory(GET_CONSOLE_LINE(Console, Row), LineSize);
        Console->RowViewOffset = 0;
        Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
        return;
    }

    //
    // Initialize and reset a fresh line.
    //

    if (Console->BottomMargin == 0) {
        NewLastLine = GET_CONSOLE_LINE(Console, Console->ScreenRows);

    //
    // There's a bottom margin (but not a top one), so move everything below
    // the bottom margin down one and zero out the bottom margin line.
    //

    } else {
        for (Row = Console->ScreenRows;
             Row > Console->ScreenRows - 1 - Console->BottomMargin;
             Row -= 1) {

            RtlCopyMemory(GET_CONSOLE_LINE(Console, Row),
                          GET_CONSOLE_LINE(Console, Row - 1),
                          LineSize);
        }

        NewLastLine = GET_CONSOLE_LINE(Console, Row + 1);
        Console->RowViewOffset = 0;
        Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN;
    }

    RtlZeroMemory(NewLastLine, LineSize);
    Console->TopLine = Console->TopLine + 1;
    if (Console->TopLine >= Console->BufferRows) {
        Console->TopLine -= Console->BufferRows;

        ASSERT(Console->TopLine < Console->BufferRows);
    }

    //
    // Create the appearance of filling up the space shown because the user
    // scrolled past the end.
    //

    if (Console->RowViewOffset > 0) {
        Console->RowViewOffset -= 1;
    }

    return;
}

VOID
VcpSetColorFromParameters (
    PVIDEO_CONSOLE_DEVICE Console,
    PTERMINAL_COMMAND_DATA Command
    )

/*++

Routine Description:

    This routine sets the current text attributes based on the paramters in
    the input parse state.

Arguments:

    Console - Supplies a pointer to the video console that just got the
        set colors command.

    Command - Supplies a pointer to the command to set attributes from.

Return Value:

    None.

--*/

{

    USHORT Attributes;
    LONG Parameter;
    LONG ParameterIndex;

    Attributes = 0;
    for (ParameterIndex = 0;
         ParameterIndex < Command->ParameterCount;
         ParameterIndex += 1) {

        Parameter = Command->Parameter[ParameterIndex];
        if (Parameter == TERMINAL_GRAPHICS_BOLD) {
            Attributes |= BASE_VIDEO_FOREGROUND_BOLD;

        } else if (Parameter == TERMINAL_GRAPHICS_NEGATIVE) {
            Attributes |= BASE_VIDEO_NEGATIVE;

        } else if ((Parameter >= TERMINAL_GRAPHICS_FOREGROUND) &&
                   (Parameter <
                    TERMINAL_GRAPHICS_FOREGROUND + AnsiColorCount)) {

            Attributes &= ~BASE_VIDEO_COLOR_MASK;
            Attributes |= Parameter - TERMINAL_GRAPHICS_FOREGROUND +
                          AnsiColorBlack;

        } else if ((Parameter >= TERMINAL_GRAPHICS_BACKGROUND) &&
                   (Parameter <
                    TERMINAL_GRAPHICS_BACKGROUND + AnsiColorCount)) {

            Attributes &= ~(BASE_VIDEO_COLOR_MASK <<
                            BASE_VIDEO_BACKGROUND_SHIFT);

            Attributes |= (Parameter - TERMINAL_GRAPHICS_BACKGROUND +
                           AnsiColorBlack) << BASE_VIDEO_BACKGROUND_SHIFT;
        }
    }

    Console->TextAttributes = Attributes;
    return;
}

VOID
VcpSaveRestoreCursor (
    PVIDEO_CONSOLE_DEVICE Console,
    BOOL Save
    )

/*++

Routine Description:

    This routine saves or restores the cursor position and text attributes.

Arguments:

    Console - Supplies a pointer to the video console.

    Save - Supplies a boolean indicating whether to save the attributes (TRUE)
        or restore them (FALSE).

Return Value:

    None.

--*/

{

    if (Save != FALSE) {
        Console->SavedColumn = Console->NextColumn;
        Console->SavedRow = Console->NextRow;
        Console->SavedAttributes = Console->TextAttributes;

    } else {
        Console->NextColumn = Console->SavedColumn;
        Console->NextRow = Console->SavedRow;
        Console->TextAttributes = Console->SavedAttributes;
    }

    return;
}

VOID
VcpMoveCursorRelative (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG DistanceX,
    LONG DistanceY
    )

/*++

Routine Description:

    This routine moves the cursor relative to its current position.

Arguments:

    Console - Supplies a pointer to the video console.

    DistanceX - Supplies the distance to move the cursor right. Negative values
        move left.

    DistanceY - Supplies the distance to move the cursor down. Negative values
        move up.

Return Value:

    None.

--*/

{

    LONG NewColumn;
    LONG NewRow;

    NewColumn = Console->NextColumn + DistanceX;
    if (NewColumn < 0) {
        NewColumn = 0;

    } else if (NewColumn >= Console->Columns) {
        NewColumn = Console->Columns - 1;
    }

    NewRow = Console->NextRow + DistanceY;
    if (NewRow < Console->TopMargin) {
        NewRow = Console->TopMargin;

    } else if (NewRow >= Console->ScreenRows - Console->BottomMargin) {
        NewRow = Console->ScreenRows - 1 - Console->BottomMargin;
    }

    Console->NextRow = NewRow;
    Console->NextColumn = NewColumn;
    Console->PendingAction |= VIDEO_ACTION_RESET_SCROLL;
    return;
}

VOID
VcpMoveCursorAbsolute (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG Column,
    LONG Row,
    BOOL ProcessOriginMode
    )

/*++

Routine Description:

    This routine moves the cursor to a new absolute position.

Arguments:

    Console - Supplies a pointer to the video console.

    Column - Supplies the new zero-based column to move to.

    Row - Supplies the new zero-based row to move to.

    ProcessOriginMode - Supplies a boolean indicating if this routine should
        adjust the position if origin mode is set.

Return Value:

    None.

--*/

{

    LONG MaxRow;
    LONG MinRow;

    if (Column < 0) {
        Column = 0;

    } else if (Column >= Console->Columns) {
        Column = Console->Columns - 1;
    }

    MinRow = 0;
    MaxRow = Console->ScreenRows - 1;
    if (((Console->Mode & CONSOLE_MODE_ORIGIN) != 0) &&
        (ProcessOriginMode != FALSE)) {

        MinRow = Console->TopMargin;
        MaxRow -= Console->BottomMargin;
        Row += Console->TopMargin;
    }

    if (Row < MinRow) {
        Row = MinRow;
    }

    if (Row > MaxRow) {
        Row = MaxRow;
    }

    Console->NextRow = Row;
    Console->NextColumn = Column;
    Console->PendingAction |= VIDEO_ACTION_RESET_SCROLL;
    return;
}

VOID
VcpDeleteLines (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG Count,
    LONG StartingRow
    )

/*++

Routine Description:

    This routine deletes lines from the console screen, moving following lines
    up.

Arguments:

    Console - Supplies a pointer to the video console.

    Count - Supplies the number of lines to delete.

    StartingRow - Supplies the first row of the region.

Return Value:

    None.

--*/

{

    UINTN LineSize;
    LONG Row;

    if (StartingRow == Console->ScreenRows - 1 - Console->BottomMargin) {
        return;
    }

    //
    // If more lines are being deleted than can exist in the scroll area,
    // just erase the scroll area.
    //

    if (Count >
        (Console->ScreenRows - Console->BottomMargin -
         StartingRow + 1)) {

        VcpEraseArea(Console,
                     0,
                     StartingRow,
                     Console->Columns - 1,
                     Console->ScreenRows - 1 - Console->BottomMargin,
                     TRUE);

        return;
    }

    //
    // Move lines up within the scroll region.
    //

    LineSize = CONSOLE_LINE_SIZE(Console);
    for (Row = StartingRow;
         Row < Console->ScreenRows - Console->BottomMargin - Count;
         Row += 1) {

        RtlCopyMemory(GET_CONSOLE_LINE(Console, Row),
                      GET_CONSOLE_LINE(Console, Row + Count),
                      LineSize);
    }

    ASSERT(Row <= Console->ScreenRows - 1 - Console->BottomMargin);

    VcpEraseArea(Console,
                 0,
                 Row,
                 Console->Columns - 1,
                 Console->ScreenRows - 1 - Console->BottomMargin,
                 TRUE);

    Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN |
                              VIDEO_ACTION_RESET_SCROLL;

    return;
}

VOID
VcpInsertLines (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG Count,
    LONG StartingRow
    )

/*++

Routine Description:

    This routine inserts lines on the console screen, moving following lines
    down.

Arguments:

    Console - Supplies a pointer to the video console.

    Count - Supplies the number of lines to delete.

    StartingRow - Supplies the first row of the region.

Return Value:

    None.

--*/

{

    UINTN LineSize;
    LONG Row;

    //
    // If more lines are being inserted than exist in the scroll area, just
    // erase the scroll area.
    //

    if (Count > Console->ScreenRows - Console->BottomMargin - StartingRow) {
        VcpEraseArea(Console,
                     0,
                     StartingRow,
                     Console->Columns - 1,
                     Console->ScreenRows - 1 - Console->BottomMargin,
                     TRUE);

        return;
    }

    //
    // Move lines down within the scroll region.
    //

    LineSize = CONSOLE_LINE_SIZE(Console);
    for (Row = Console->ScreenRows - Console->BottomMargin - 1;
         Row >= StartingRow + Count;
         Row -= 1) {

        RtlCopyMemory(GET_CONSOLE_LINE(Console, Row),
                      GET_CONSOLE_LINE(Console, Row - Count),
                      LineSize);
    }

    VcpEraseArea(Console,
                 0,
                 StartingRow,
                 Console->Columns - 1,
                 StartingRow + Count - 1,
                 TRUE);

    Console->PendingAction |= VIDEO_ACTION_REDRAW_ENTIRE_SCREEN |
                              VIDEO_ACTION_RESET_SCROLL;

    return;
}

VOID
VcpMoveCursorTabStops (
    PVIDEO_CONSOLE_DEVICE Console,
    LONG Advance
    )

/*++

Routine Description:

    This routine advances the cursor forward or backwards by the given number
    of tab stops.

Arguments:

    Console - Supplies a pointer to the video console.

    Advance - Supplies the number of tab stops to advance. Negative values
        move the cursor backwards to previous tab stops. A value of zero is a
        no-op.

Return Value:

    None.

--*/

{

    LONG Increment;

    if (Advance > 0) {
        Increment = 1;

    } else {
        Increment = -1;
    }

    while (Advance != 0) {

        //
        // Perform at least one cursor movement to get off a current tab stop.
        //

        if ((Console->NextColumn + Increment >= 0) &&
            (Console->NextColumn + Increment <= Console->Columns - 1)) {

            Console->NextColumn += Increment;

        } else {
            break;
        }

        //
        // Find the next tab stop or end.
        //

        while ((Console->NextColumn + Increment >= 0) &&
               (Console->NextColumn + Increment <= Console->Columns - 1) &&
               (!IS_TAB_STOP(Console, Console->NextColumn))) {

            Console->NextColumn += Increment;
        }

        Advance -= Increment;
    }

    ASSERT((Console->NextColumn >= 0) &&
           (Console->NextColumn <= Console->Columns));

    return;
}

