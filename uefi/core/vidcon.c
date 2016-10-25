/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    vidcon.c

Abstract:

    This module

Author:

    Evan Green

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define away the API decorator.
//

#define KERNEL_API

#include "ueficore.h"
#include <minoca/kernel/sysres.h>
#include <minoca/lib/basevid.h>
#include <minoca/uefi/protocol/graphout.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the disk I/O data given a pointer to the
// block I/O protocol instance.
//

#define EFI_GRAPHICS_CONSOLE_FROM_THIS(_TextOutput) \
    PARENT_STRUCTURE(_TextOutput, EFI_GRAPHICS_CONSOLE, TextOutput)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_GRAPHICS_CONSOLE_MAGIC 0x43646956 // 'CdiV'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the internal data structure of a graphical
    console.

Members:

    Magic - Stores the magic constant EFI_GRAPHICS_CONSOLE_MAGIC.

    Graphics - Stores a pointer to the graphics output protocol.

    Handle - Stores the console handle.

    TextOutput - Stores the simple text output protocol.

    Mode - Stores the mode information.

    HorizontalResolution - Stores the horizontal resolution of the graphics
        device, in pixels.

    VerticalResolution - Stores the vertical resolution of the graphics device,
        in pixels.

    PixelsPerScanLine - Stores the number of pixels per scan line in the
        frame buffer.

    BitsPerPixel - Stores the width of a pixel in the frame buffer.

    GraphicsMode - Stores the graphics mode number the console was initialized
        on.

--*/

typedef struct _EFI_GRAPHICS_CONSOLE {
    UINTN Magic;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics;
    EFI_HANDLE Handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL TextOutput;
    EFI_SIMPLE_TEXT_OUTPUT_MODE Mode;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 PixelsPerScanLine;
    UINT32 BitsPerPixel;
    UINT32 GraphicsMode;
} EFI_GRAPHICS_CONSOLE, *PEFI_GRAPHICS_CONSOLE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
VOID
EfipGraphicsOutputNotify (
    EFI_EVENT Event,
    VOID *Context
    );

EFIAPI
EFI_STATUS
EfipGraphicsTextReset (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

EFI_STATUS
EfipGraphicsTextStringOut (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
    );

EFIAPI
EFI_STATUS
EfipGraphicsTextTestString (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
    );

EFIAPI
EFI_STATUS
EfipGraphicsTextQueryMode (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber,
    UINTN *Columns,
    UINTN *Rows
    );

EFIAPI
EFI_STATUS
EfipGraphicsTextSetMode (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber
    );

EFIAPI
EFI_STATUS
EfipGraphicsTextSetAttribute (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Attribute
    );

EFIAPI
EFI_STATUS
EfipGraphicsTextClearScreen (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This
    );

EFIAPI
EFI_STATUS
EfipGraphicsTextSetCursorPosition (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Column,
    UINTN Row
    );

EFIAPI
EFI_STATUS
EfipGraphicsTextEnableCursor (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN Visible
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_GUID EfiGraphicsOutputProtocolGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_GUID EfiSimpleTextOutputProtocolGuid = EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;

EFI_EVENT EfiGraphicsOutputNotifyEvent;
VOID *EfiGraphicsOutputNotifyRegistration;

//
// For now, only install onto one graphics device.
//

BOOLEAN EfiGraphicsConsoleInstalled;

EFI_GRAPHICS_CONSOLE EfiGraphicsConsoleTemplate = {
    EFI_GRAPHICS_CONSOLE_MAGIC,
    NULL,
    NULL,
    {
        EfipGraphicsTextReset,
        EfipGraphicsTextStringOut,
        EfipGraphicsTextTestString,
        EfipGraphicsTextQueryMode,
        EfipGraphicsTextSetMode,
        EfipGraphicsTextSetAttribute,
        EfipGraphicsTextClearScreen,
        EfipGraphicsTextSetCursorPosition,
        EfipGraphicsTextEnableCursor,
        NULL
    }
};

BASE_VIDEO_CONTEXT EfiVideoContext;

BASE_VIDEO_PALETTE EfiVideoPalette = {
    {
        BASE_VIDEO_COLOR_RGB(250, 250, 250),
        BASE_VIDEO_COLOR_RGB(0, 0, 0),
        BASE_VIDEO_COLOR_RGB(194, 54, 33),
        BASE_VIDEO_COLOR_RGB(37, 188, 36),
        BASE_VIDEO_COLOR_RGB(173, 173, 39),
        BASE_VIDEO_COLOR_RGB(73, 46, 225),
        BASE_VIDEO_COLOR_RGB(211, 56, 211),
        BASE_VIDEO_COLOR_RGB(51, 187, 200),
        BASE_VIDEO_COLOR_RGB(203, 204, 206),
    },

    {
        BASE_VIDEO_COLOR_RGB(255, 255, 170),
        BASE_VIDEO_COLOR_RGB(131, 131, 131),
        BASE_VIDEO_COLOR_RGB(252, 57, 31),
        BASE_VIDEO_COLOR_RGB(49, 231, 34),
        BASE_VIDEO_COLOR_RGB(234, 236, 35),
        BASE_VIDEO_COLOR_RGB(88, 51, 255),
        BASE_VIDEO_COLOR_RGB(249, 53, 248),
        BASE_VIDEO_COLOR_RGB(20, 240, 240),
        BASE_VIDEO_COLOR_RGB(233, 235, 237),
    },

    BASE_VIDEO_COLOR_RGB(35, 0, 35),
    BASE_VIDEO_COLOR_RGB(35, 0, 35),
    BASE_VIDEO_COLOR_RGB(250, 250, 250),
    BASE_VIDEO_COLOR_RGB(142, 40, 0),
};

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiGraphicsTextDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine initializes support for UEFI video consoles.

Arguments:

    ImageHandle - Supplies the image handle for this driver. This is probably
        the firmware core image handle.

    SystemTable - Supplies a pointer to the system table.

Return Value:

    EFI status code.

--*/

{

    //
    // Sign up to be notified whenever a new firmware volume block device
    // protocol crops up.
    //

    EfiGraphicsOutputNotifyEvent = EfiCoreCreateProtocolNotifyEvent(
                                         &EfiGraphicsOutputProtocolGuid,
                                         TPL_CALLBACK,
                                         EfipGraphicsOutputNotify,
                                         NULL,
                                         &EfiGraphicsOutputNotifyRegistration);

    ASSERT(EfiGraphicsOutputNotifyEvent != NULL);

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
VOID
EfipGraphicsOutputNotify (
    EFI_EVENT Event,
    VOID *Context
    )

/*++

Routine Description:

    This routine is called when a new graphics output protocol appears in the
    system.

Arguments:

    Event - Supplies a pointer to the event that fired.

    Context - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    UINT32 BitsPerPixel;
    UINTN BufferSize;
    UINT32 CombinedMask;
    PEFI_GRAPHICS_CONSOLE Device;
    SYSTEM_RESOURCE_FRAME_BUFFER FrameBuffer;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GraphicsMode;
    EFI_HANDLE Handle;
    EFI_STATUS Status;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextOut;
    KSTATUS VideoStatus;

    //
    // Examine all new handles.
    //

    while (TRUE) {
        Status = EfiCoreLocateHandle(ByRegisterNotify,
                                     NULL,
                                     EfiGraphicsOutputNotifyRegistration,
                                     &BufferSize,
                                     &Handle);

        if (Status == EFI_NOT_FOUND) {
            break;
        }

        if (EFI_ERROR(Status)) {
            continue;
        }

        //
        // Get the graphics output protocol on the handle.
        //

        Status = EfiCoreHandleProtocol(Handle,
                                       &EfiGraphicsOutputProtocolGuid,
                                       (VOID **)&Graphics);

        if (EFI_ERROR(Status)) {

            ASSERT(FALSE);

            continue;
        }

        //
        // Skip any graphics protocols that aren't in graphical mode.
        //

        if ((Graphics->Mode == NULL) || (Graphics->Mode->Info == NULL) ||
            (Graphics->Mode->SizeOfInfo <
             sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION))) {

            continue;
        }

        GraphicsMode = Graphics->Mode->Info;
        if (GraphicsMode->PixelFormat >= PixelBltOnly) {
            continue;
        }

        //
        // For now, only install once.
        //

        if (EfiGraphicsConsoleInstalled != FALSE) {
            return;
        }

        //
        // Check to see if there is a simple text output protocol already
        // installed on this handle.
        //

        Status = EfiCoreHandleProtocol(Handle,
                                       &EfiSimpleTextOutputProtocolGuid,
                                       (VOID **)&TextOut);

        //
        // If there's a previously existing text output protocol, then
        // update the block device if it was created by this driver.
        //

        if (!EFI_ERROR(Status)) {
            Device = PARENT_STRUCTURE(TextOut,
                                      EFI_GRAPHICS_CONSOLE,
                                      TextOutput);

            if (Device->Magic == EFI_GRAPHICS_CONSOLE_MAGIC) {
                Device->Graphics = Graphics;
            }

            continue;
        }

        //
        // No text output protocol is present, create a new one.
        //

        Device = EfiCoreAllocateBootPool(sizeof(EFI_GRAPHICS_CONSOLE));
        if (Device == NULL) {
            return;
        }

        EfiCoreCopyMemory(Device,
                          &EfiGraphicsConsoleTemplate,
                          sizeof(EFI_GRAPHICS_CONSOLE));

        Device->Graphics = Graphics;
        Device->Handle = Handle;
        Device->TextOutput.Mode = &(Device->Mode);
        Device->VerticalResolution = GraphicsMode->VerticalResolution;
        Device->HorizontalResolution = GraphicsMode->HorizontalResolution;
        Device->PixelsPerScanLine = GraphicsMode->PixelsPerScanLine;
        Device->GraphicsMode = Graphics->Mode->Mode;
        Device->Mode.CursorVisible = TRUE;
        Device->Mode.Attribute = EFI_BACKGROUND_BLACK | EFI_LIGHTGRAY;
        EfiSetMem(&FrameBuffer, sizeof(SYSTEM_RESOURCE_FRAME_BUFFER), 0);
        FrameBuffer.Mode = BaseVideoModeFrameBuffer;
        FrameBuffer.Width = GraphicsMode->HorizontalResolution;
        FrameBuffer.Height = GraphicsMode->VerticalResolution;
        FrameBuffer.PixelsPerScanLine = GraphicsMode->PixelsPerScanLine;
        FrameBuffer.Header.PhysicalAddress = Graphics->Mode->FrameBufferBase;
        FrameBuffer.Header.VirtualAddress =
                           (VOID *)(UINTN)(FrameBuffer.Header.PhysicalAddress);

        switch (GraphicsMode->PixelFormat) {
        case PixelRedGreenBlueReserved8BitPerColor:
            FrameBuffer.BitsPerPixel = 32;
            FrameBuffer.RedMask = 0x000000FF;
            FrameBuffer.GreenMask = 0x0000FF00;
            FrameBuffer.BlueMask = 0x00FF0000;
            break;

        case PixelBlueGreenRedReserved8BitPerColor:
            FrameBuffer.BitsPerPixel = 32;
            FrameBuffer.RedMask = 0x00FF0000;
            FrameBuffer.GreenMask = 0x0000FF00;
            FrameBuffer.BlueMask = 0x000000FF;
            break;

        case PixelBitMask:
            FrameBuffer.RedMask = GraphicsMode->PixelInformation.RedMask;
            FrameBuffer.GreenMask = GraphicsMode->PixelInformation.GreenMask;
            FrameBuffer.BlueMask = GraphicsMode->PixelInformation.BlueMask;
            CombinedMask = GraphicsMode->PixelInformation.RedMask |
                           GraphicsMode->PixelInformation.GreenMask |
                           GraphicsMode->PixelInformation.BlueMask |
                           GraphicsMode->PixelInformation.ReservedMask;

            ASSERT(CombinedMask != 0);

            BitsPerPixel = 32;
            while ((BitsPerPixel != 0) &&
                   ((CombinedMask & (1 << (BitsPerPixel - 1))) == 0)) {

                BitsPerPixel -= 1;
            }

            FrameBuffer.BitsPerPixel = BitsPerPixel;
            break;

        default:
            EfiCoreFreePool(Device);
            continue;
        }

        Device->BitsPerPixel = FrameBuffer.BitsPerPixel;
        FrameBuffer.Header.Size = FrameBuffer.PixelsPerScanLine *
                                  (FrameBuffer.BitsPerPixel / 8) *
                                  FrameBuffer.Height;

        FrameBuffer.Header.Type = SystemResourceFrameBuffer;
        VideoStatus = VidInitialize(&EfiVideoContext, &FrameBuffer);
        if (KSUCCESS(VideoStatus)) {
            VidSetPalette(&EfiVideoContext, &EfiVideoPalette, NULL);
            VidClearScreen(&EfiVideoContext, 0, 0, -1, -1);
            Status = EFI_SUCCESS;

        } else {
            Status = EFI_DEVICE_ERROR;
        }

        if (!EFI_ERROR(Status)) {
            Status = EfiCoreInstallProtocolInterface(
                                           &Handle,
                                           &EfiSimpleTextOutputProtocolGuid,
                                           EFI_NATIVE_INTERFACE,
                                           &(Device->TextOutput));

            ASSERT(!EFI_ERROR(Status));

        }

        if (EFI_ERROR(Status)) {
            EfiCoreFreePool(Device);

        } else {
            EfiGraphicsConsoleInstalled = TRUE;
        }
    }

    return;
}

EFIAPI
EFI_STATUS
EfipGraphicsTextReset (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    )

/*++

Routine Description:

    This routine resets the output device hardware and optionally runs
    diagnostics.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ExtendedVerification - Supplies a boolean indicating if the driver should
        perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    reset.

--*/

{

    PEFI_GRAPHICS_CONSOLE Console;

    Console = EFI_GRAPHICS_CONSOLE_FROM_THIS(This);

    ASSERT(Console->Magic == EFI_GRAPHICS_CONSOLE_MAGIC);

    This->SetAttribute(
            This,
            EFI_TEXT_ATTR(This->Mode->Attribute & 0x0F, EFI_BACKGROUND_BLACK));

    return This->SetMode(This, Console->GraphicsMode);
}

EFI_STATUS
EfipGraphicsTextStringOut (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
    )

/*++

Routine Description:

    This routine writes a (wide) string to the output device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    String - Supplies the null-terminated string to be displayed on the output
        device(s). All output devices must also support the Unicode drawing
        character codes defined in this file.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    written to.

    EFI_UNSUPPORTED if the output device's mode indicates some of the
    characters in the string could not be rendered and were skipped.

--*/

{

    CHAR8 Ascii[2];
    UINT32 CellHeight;
    UINT32 CellWidth;
    UINTN ColumnCount;
    PEFI_GRAPHICS_CONSOLE Console;
    UINTN CopySize;
    VOID *FrameBuffer;
    UINTN LastLineY;
    VOID *LineOne;
    EFI_SIMPLE_TEXT_OUTPUT_MODE *Mode;
    UINTN RowCount;
    EFI_STATUS Status;

    Console = EFI_GRAPHICS_CONSOLE_FROM_THIS(This);

    ASSERT(Console->Magic == EFI_GRAPHICS_CONSOLE_MAGIC);

    Mode = This->Mode;
    Status = This->QueryMode(This, Mode->Mode, &ColumnCount, &RowCount);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Skip it if the graphics output has been configured for a different mode
    // than the one initialized in.
    //

    if (Console->Graphics->Mode->Mode != Console->GraphicsMode) {
        return EFI_DEVICE_ERROR;
    }

    FrameBuffer = (VOID *)(UINTN)(Console->Graphics->Mode->FrameBufferBase);
    if (FrameBuffer == NULL) {
        return EFI_DEVICE_ERROR;
    }

    //
    // Calculate the number of bytes to copy when scrolling, which is the
    // number of console lines minus one.
    //

    CellWidth = EfiVideoContext.Font->CellWidth;
    CellHeight = EfiVideoContext.Font->CellHeight;
    CopySize = Console->PixelsPerScanLine * (Console->BitsPerPixel / 8) *
               ((RowCount - 1) * CellHeight);

    LineOne = FrameBuffer + (Console->PixelsPerScanLine *
                             (Console->BitsPerPixel / 8) *
                             CellHeight);

    //
    // Loop printing each character.
    //

    Ascii[1] = '\0';
    Status = EFI_SUCCESS;
    while (*String != L'\0') {
        if (*String == CHAR_BACKSPACE) {
            if (Mode->CursorColumn == 0) {
                if (Mode->CursorRow != 0) {
                    Mode->CursorRow -= 1;
                }

                Mode->CursorColumn = ColumnCount - 1;

            } else {
                Mode->CursorColumn -= 1;
            }

        } else if (*String == CHAR_LINEFEED) {
            if (Mode->CursorRow < RowCount - 1) {
                Mode->CursorRow += 1;

            //
            // If already at the last line, scroll.
            //

            } else {
                EfiCopyMem(FrameBuffer, LineOne, CopySize);
                LastLineY = (RowCount - 1) * CellHeight;
                VidClearScreen(&EfiVideoContext,
                               0,
                               LastLineY,
                               ColumnCount * CellWidth,
                               LastLineY + CellHeight);
            }

        } else if (*String == CHAR_CARRIAGE_RETURN) {
            Mode->CursorColumn = 0;

        } else if ((*String >= L' ') && (*String <= L'~')) {
            Ascii[0] = *String;

            //
            // If the cursor is in the last position, scroll.
            //

            if (Mode->CursorColumn >= ColumnCount - 1) {
                Mode->CursorColumn = 0;
                if (Mode->CursorRow == RowCount - 1) {
                    EfiCopyMem(FrameBuffer, LineOne, CopySize);
                    LastLineY = (RowCount - 1) * CellHeight;
                    VidClearScreen(&EfiVideoContext,
                                   0,
                                   LastLineY,
                                   ColumnCount * CellWidth,
                                   LastLineY + CellHeight);

                } else {
                    Mode->CursorRow += 1;
                }
            }

            VidPrintString(&EfiVideoContext,
                           Mode->CursorColumn,
                           Mode->CursorRow,
                           Ascii);

            Mode->CursorColumn += 1;

        //
        // Some of the characters could not be rendered.
        //

        } else {
            Status = EFI_UNSUPPORTED;
        }

        String += 1;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipGraphicsTextTestString (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    CHAR16 *String
    )

/*++

Routine Description:

    This routine verifies that all characters in a string can be output to the
    target device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    String - Supplies the null-terminated string to be examined for the output
        device(s).

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the output device's mode indicates some of the
    characters in the string could not be rendered and were skipped.

--*/

{

    EFI_STATUS Status;

    Status = EFI_SUCCESS;
    while (*String != L'\0') {
        if ((*String != CHAR_BACKSPACE) &&
            (*String != CHAR_LINEFEED) &&
            (*String != CHAR_CARRIAGE_RETURN) &&
            ((*String < L' ') || (*String > L'~'))) {

            Status = EFI_UNSUPPORTED;
            break;
        }

        String += 1;
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfipGraphicsTextQueryMode (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber,
    UINTN *Columns,
    UINTN *Rows
    )

/*++

Routine Description:

    This routine requests information for an available text mode that the
    output device(s) can support.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ModeNumber - Supplies the mode number to return information on.

    Columns - Supplies a pointer where the number of columns on the output
        device will be returned.

    Rows - Supplies a pointer where the number of rows on the output device
        will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the mode number was not valid.

--*/

{

    PEFI_GRAPHICS_CONSOLE Console;

    Console = EFI_GRAPHICS_CONSOLE_FROM_THIS(This);

    ASSERT(Console->Magic == EFI_GRAPHICS_CONSOLE_MAGIC);

    if (ModeNumber == 0) {
        if (EfiVideoContext.Font == NULL) {
            return EFI_NOT_READY;
        }

        *Columns = Console->HorizontalResolution /
                   EfiVideoContext.Font->CellWidth;

        *Rows = Console->VerticalResolution / EfiVideoContext.Font->CellHeight;
        return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfipGraphicsTextSetMode (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN ModeNumber
    )

/*++

Routine Description:

    This routine sets the output device to a specified mode.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ModeNumber - Supplies the mode number to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the mode number was not valid.

--*/

{

    if (ModeNumber != 0) {
        return EFI_UNSUPPORTED;
    }

    This->Mode->Mode = 0;
    This->ClearScreen(This);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipGraphicsTextSetAttribute (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Attribute
    )

/*++

Routine Description:

    This routine sets the background and foreground colors for the output
    string and clear screen functions.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Attribute - Supplies the attributes to set. Bits 0 to 3 are the foreground
        color, and bits 4 to 6 are the background color. All other bits must
        be zero. See the EFI_TEXT_ATTR macro.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the attribute requested is not defined.

--*/

{

    This->Mode->Attribute = Attribute;
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipGraphicsTextClearScreen (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This
    )

/*++

Routine Description:

    This routine clears the output device(s) display to the currently selected
    background color.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the output device is not in a valid text mode.

--*/

{

    PEFI_GRAPHICS_CONSOLE Console;

    Console = EFI_GRAPHICS_CONSOLE_FROM_THIS(This);

    ASSERT(Console->Magic == EFI_GRAPHICS_CONSOLE_MAGIC);

    VidClearScreen(&EfiVideoContext, 0, 0, -1, -1);
    return This->SetCursorPosition(This, 0, 0);
}

EFIAPI
EFI_STATUS
EfipGraphicsTextSetCursorPosition (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    UINTN Column,
    UINTN Row
    )

/*++

Routine Description:

    This routine sets the current coordinates of the cursor position.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Column - Supplies the desired column. This must be greater than or equal to
        zero and less than the number of columns as reported by the query mode
        function.

    Row - Supplies the desired row. This must be greater than or equal to zero
        and less than the number of rows as reported by the query mode function.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the output device is not in a valid text mode, or the
    requested cursor position is out of range.

--*/

{

    UINTN ColumnCount;
    PEFI_GRAPHICS_CONSOLE Console;
    UINTN RowCount;
    EFI_STATUS Status;

    Console = EFI_GRAPHICS_CONSOLE_FROM_THIS(This);

    ASSERT(Console->Magic == EFI_GRAPHICS_CONSOLE_MAGIC);

    Status = This->QueryMode(This,
                             This->Mode->Mode,
                             &ColumnCount,
                             &RowCount);

    if (EFI_ERROR(Status)) {
        return EFI_DEVICE_ERROR;
    }

    if ((Column >= ColumnCount) || (Row >= RowCount)) {
        return EFI_UNSUPPORTED;
    }

    This->Mode->CursorColumn = Column;
    This->Mode->CursorRow = Row;
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipGraphicsTextEnableCursor (
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
    BOOLEAN Visible
    )

/*++

Routine Description:

    This routine makes the cursor visible or invisible.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Visible - Supplies a boolean indicating whether to make the cursor visible
        (TRUE) or invisible (FALSE).

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_UNSUPPORTED if the output device is not in a valid text mode.

--*/

{

    return EFI_UNSUPPORTED;
}

