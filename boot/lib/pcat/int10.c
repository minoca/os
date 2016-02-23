/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    int10.c

Abstract:

    This module implements basic BIOS video services using the INT 10 call.

Author:

    Evan Green 30-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/x86.h>
#include "firmware.h"
#include <minoca/lib/basevid.h>
#include "realmode.h"
#include "bios.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts the segmented addresses given in the VESA information
// structure into linear addresses that can be dereferenced in protected mode.
//

#define VESA_SEGMENTED_TO_LINEAR_ADDRESS(_Address) \
    (PVOID)((((_Address) & 0xFFFF0000) >> 12) +    \
            ((_Address) & 0x0000FFFF))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of supported modes.
//

#define VESA_MAX_MODES 70

//
// Define the signature at the header of the VESA Information structure, for
// versions 1 and 2.
//

#define VESA_1_SIGNATURE 0x41534556 // 'VESA'
#define VESA_2_SIGNATURE 0x32454256 // 'VBE2'

//
// Define video mode attribute flags.
//

#define VESA_MODE_ATTRIBUTE_SUPPORTED     0x0001
#define VESA_MODE_ATTRIBUTE_TTY_SUPPORTED 0x0004
#define VESA_MODE_ATTRIBUTE_COLOR         0x0008
#define VESA_MODE_ATTRIBUTE_GRAPHICS      0x0010
#define VESA_MODE_ATTRIBUTE_NON_VGA       0x0020
#define VESA_MODE_ATTRIBUTE_VGA_WINDOWED  0x0040
#define VESA_MODE_ATTRIBUTE_LINEAR        0x0080

//
// Define the meaningful bits in the mode number.
//

#define VESA_MODE_NUMBER_USE_LINEAR_MODEL   0x4000
#define VESA_MODE_NUMBER_DONT_CLEAR_DISPLAY 0x8000

//
// Define values for AX (the different VESA function calls).
//

#define VESA_FUNCTION_GET_VESA_INFORMATION 0x4F00
#define VESA_FUNCTION_GET_MODE_INFORMATION 0x4F01
#define VESA_FUNCTION_SET_MODE             0x4F02

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _VESA_MEMORY_MODEL {
    VesaMemoryModelText        = 0,
    VesaMemoryModelCga         = 1,
    VesaMemoryModelHercules    = 2,
    VesaMemoryModel4Plane      = 3,
    VesaMemoryModelPackedPixel = 4,
    VesaMemoryModelNonChain4   = 5,
    VesaMemoryModelDirectColor = 6,
    VesaMemoryModelYuv         = 7
} VESA_MEMORY_MODEL, *PVESA_MEMORY_MODEL;

/*++

Structure Description:

    This structure stores information about BIOS compatibility with the VESA
    video standard.

Members:

    Signature - Stores the signature indicating this is a valid structure. The
        magic constant for VESA v1 means the structure content is valid through
        the Total Memory field. In v2 all fields are valid.

    VesaVersion - Stores the major version in the high byte, and the minor
        version in the low byte.

    OemStringPointer - Supplies a pointer to a NULL terminated string describing
        the Original Equipment Manufacturer chip, board, configuration, or
        anything else.

    Capabilities - Stores a bitfield of capabilities. The only valid bit is bit
        0, which defines whether or not the DAC is switchable (has multiple
        modes).

    VideoModePointer - Stores a pointer to a list of possibly supported mode
        numbers. Each mode number is 16 bits in length, and the list is
        terminated with a -1.

    TotalMemoryBlocks - Storse the number of 64kB blocks of video memory in the
        card.

    OemSoftwareRevision - Stores the OEM software revision number. This is the
        first field that is only filled out in VESA 2.0.

    OemVendorNamePointer - Stores a pointer to a NULL terminated string of the
        vendor name.

    OemProductNamePointer - Stores a pointer to a NULL terminated string of the
        product name.

    OemProductRevisionPointer - Stores a pointer to a NULL terminated string of
        the product revision.

    Reserved - Stores a bunch of bytes that shouldn't be touched.

    OemData - Stores a section of space reserved for OEM-specific information.

--*/

typedef struct _VESA_INFORMATION {
    ULONG Signature;
    USHORT VesaVersion;
    ULONG OemStringPointer;
    ULONG Capabilities;
    ULONG VideoModePointer;
    USHORT TotalMemoryBlocks;
    USHORT OemSoftwareRevision;
    ULONG OemVendorNamePointer;
    ULONG OemProductNamePointer;
    ULONG OemProductRevisionPointer;
    //UCHAR Reserved[222];
    //UCHAR OemData[256];
} PACKED VESA_INFORMATION, *PVESA_INFORMATION;

/*++

Structure Description:

    This structure stores information about a particular video mode.

Members:

    ModeAttributes - Stores a bitfield of attributes about the mode. Bit 0
        specifies if the mode can be initialized in the present video
        configuration. Bit 1 specifies if the optional fields are filled in.
        Bit 2 specifies if the BIOS supports output functions like TTY output,
        scrolling, pixel output, etc.

    WindowAAttributes - Stores a bitfield of attributes relating to the first
        window buffer. Bit 0 stores whether or not the window is supported, bit
        1 stores whether or not the window is readable, and bit 2 stores
        whether or not the window is writeable.

    WindowBAttributes - Stores a bitfield of attributes relating to the second
        window. The meaning of the bits are the same as window A.

    WindowGranularity - Stores the smallest boundary, in kilobytes, on which the
        window can be placed in the video memory.

    WindowSize - Stores the size of each window, in kilobytes.

    WindowASegment - Stores the segment address of the window in CPU address
        space.

    WindowBSegment - Stores the segment address of the second window in CPU
        address space.

    WindowFunctionPointer - Stores the address of the CPU video memory windowing
        function. The windowing function can be invoked by VESA BIOS function
        5, or by calling this address directly.

    BytesPerScanLine - Stores the number of bytes in one horizontal line of the
        video frame. This value must be at least as large as the visible bytes
        per line.

    XResolution - Stores the width of the video mode, in pixels (for text modes,
        the units here are characters).

    YResolution - Stores the height of the video mode, in pixels or characters.

    XCharacterSize - Stores the width of one text character, in pixels.

    YCharacterSize - Stores the height of one text character, in pixels.

    NumberOfPlanes - Stores the number of memory planes available to software in
        the mode. For standard 16-color VGA graphics, this would be set to 4.
        For standard packed pixel modes, the field would be set to 1.

    BitsPerPixel - Stores the number of bits in one pixel on the screen.

    NumberOfBanks - Stores the number of memory banks used to display a window
        of the mode.

    MemoryModel - Stores the memory model for this video mode. See the
        VESA_MEMORY_MODEL for possible values.

    BankSize - Stores the size of a bank in 1 kilobyte units.

    NumberOfImagePages - Stores the number of additional complete display images
        that will fit into the VGA's memory at one time in this mode.

    Reserved1 - Stores a field reserved for a future VESA feature, and will
        always be set to 1.

    RedMaskSize - Stores the number of bits in the red component of a direct
        color pixel.

    RedFieldPosition - Stores the bit position of the least significant bit of
        the red color component.

    GreenMaskSize - Stores the number of bits in the green component of a direct
        color pixel.

    GreenFieldPosition - Stores the bit position of the least significant bit of
        the green color component.

    BlueMaskSize - Stores the number of bits in the blue component of a direct
        color pixel.

    BlueFieldPosition - Stores the bit position of the least significant bit of
        the Blue color component.

    ReservedMaskSize - Stores the number of bits in the reserved component of a
        direct color pixel.

    ReservedFieldPosition - Stores the bit position of the least significant
        bit of the reserved component.

    DirectColorModeInformation - Stores a bitfield of flags relating to the
        direct color mode.

    PhysicalBasePointer - Stores the physical address of the linear frame
        buffer.

    OffScreenMemoryOffset - Stores a pointer to the start of the off screen
        memory.

    OffScreenMemorySize - Stores the size of the off screen memory, in 1
        kilobyte units.

--*/

typedef struct _VESA_MODE_INFORMATION {
    USHORT ModeAttributes;
    UCHAR WindowAAttributes;
    UCHAR WindowBAttributes;
    USHORT WindowGranularity;
    USHORT WindowSize;
    USHORT WindowASegment;
    USHORT WindowBSegment;
    ULONG WindowFunctionPointer;
    USHORT BytesPerScanLine;
    USHORT XResolution;
    USHORT YResolution;
    UCHAR XCharacterSize;
    UCHAR YCharacterSize;
    UCHAR NumberOfPlanes;
    UCHAR BitsPerPixel;
    UCHAR NumberOfBanks;
    UCHAR MemoryModel;
    UCHAR BankSize;
    UCHAR NumberOfImagePages;
    UCHAR Reserved1;
    UCHAR RedMaskSize;
    UCHAR RedFieldPosition;
    UCHAR GreenMaskSize;
    UCHAR GreenFieldPosition;
    UCHAR BlueMaskSize;
    UCHAR BlueFieldPosition;
    UCHAR ReservedMaskSize;
    UCHAR ReservedFieldPosition;
    UCHAR DirectColorModeInformation;
    ULONG PhysicalBasePointer;
    ULONG OffScreenMemoryOffset;
    USHORT OffScreenMemorySize;
    //UCHAR Reserved2[206];
} PACKED VESA_MODE_INFORMATION, *PVESA_MODE_INFORMATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
FwpPcatSetTextCursor (
    UCHAR DisplayPage,
    UCHAR Row,
    UCHAR Column
    );

KSTATUS
FwpPcatGetVesaInformation (
    PVESA_INFORMATION Information
    );

KSTATUS
FwpPcatSetBestVesaMode (
    PUSHORT ModeList
    );

KSTATUS
FwpPcatFindVesaMode (
    PUSHORT ModeList,
    ULONG Width,
    ULONG Height,
    ULONG BitsPerPixel,
    PUSHORT ModeNumber,
    PPHYSICAL_ADDRESS FrameBufferAddress
    );

KSTATUS
FwpPcatFindHighestResolutionVesaMode (
    PUSHORT ModeList,
    PULONG Width,
    PULONG Height,
    PULONG BitsPerPixel,
    PUSHORT ModeNumber,
    PPHYSICAL_ADDRESS FrameBufferAddress
    );

KSTATUS
FwpPcatGetVesaModeInformation (
    USHORT ModeNumber,
    PVESA_MODE_INFORMATION ModeInformation
    );

KSTATUS
FwpPcatSetVesaMode (
    USHORT ModeNumber
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a boolean indicating whether to go for the highest resolution mode
// or just a pretty decent compatible mode.
//

BOOL FwVesaUseHighestResolution = FALSE;

//
// Store a boolean that if set will simply leave the BIOS alone in text mode.
//

BOOL FwVideoTextMode = FALSE;

//
// Store a copy of the mode list coming from the VESA information
//

USHORT FwVesaModeList[VESA_MAX_MODES];

//
// Store the frame buffer attributes.
//

ULONG FwFrameBufferMode = BaseVideoInvalidMode;
PHYSICAL_ADDRESS FwFrameBufferPhysical;
ULONG FwFrameBufferWidth;
ULONG FwFrameBufferHeight;
ULONG FwFrameBufferBitsPerPixel;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FwpPcatInitializeVideo (
    )

/*++

Routine Description:

    This routine attempts to initialize the video subsystem on a PCAT machine.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    SYSTEM_RESOURCE_FRAME_BUFFER FrameBuffer;
    VESA_INFORMATION Information;
    USHORT ModeCount;
    PUSHORT ModeList;
    KSTATUS Status;

    if (FwVideoTextMode != FALSE) {
        ModeCount = 0;

    } else {

        //
        // Attempt to get the VESA information structure.
        //

        Information.VesaVersion = 0;
        Information.Signature = VESA_2_SIGNATURE;
        Status = FwpPcatGetVesaInformation(&Information);
        if (!KSUCCESS(Status)) {
            goto InitializeVideoEnd;
        }

        if ((Information.Signature != VESA_1_SIGNATURE) ||
            (Information.VesaVersion < 0x0200)) {

            Status = STATUS_NOT_SUPPORTED;
            goto InitializeVideoEnd;
        }

        ModeList =
                VESA_SEGMENTED_TO_LINEAR_ADDRESS(Information.VideoModePointer);

        //
        // Copy the mode list to the global.
        //

        ModeCount = 0;
        while ((ModeList[ModeCount] != 0xFFFF) &&
               (ModeCount < VESA_MAX_MODES - 1)) {

            FwVesaModeList[ModeCount] = ModeList[ModeCount];
            ModeCount += 1;
        }

        FwVesaModeList[ModeCount] = 0xFFFF;
        if (ModeCount != 0) {
            Status = FwpPcatSetBestVesaMode(FwVesaModeList);
            if (!KSUCCESS(Status)) {
                goto InitializeVideoEnd;
            }
        }
    }

    //
    // Just use old text mode if no graphical video modes could be found.
    //

    if (ModeCount == 0) {

        //
        // Set the cursor off the screen to hide it since the kernel is not
        // going to be manipulating it. It's also a nice very early indication
        // that this code is running.
        //

        FwpPcatSetTextCursor(0, BIOS_TEXT_VIDEO_ROWS, 0);
        ModeCount = 0;
        FwFrameBufferMode = BaseVideoModeBiosText;
        FwFrameBufferPhysical = BIOS_TEXT_VIDEO_BASE;
        FwFrameBufferWidth = BIOS_TEXT_VIDEO_COLUMNS;
        FwFrameBufferHeight = BIOS_TEXT_VIDEO_ROWS;
        FwFrameBufferBitsPerPixel = BIOS_TEXT_VIDEO_CELL_WIDTH * BITS_PER_BYTE;
    }

    //
    // Fire up the frame buffer support library with the acquired frame buffer.
    //

    RtlZeroMemory(&FrameBuffer, sizeof(SYSTEM_RESOURCE_FRAME_BUFFER));
    FrameBuffer.Header.PhysicalAddress = FwFrameBufferPhysical;
    FrameBuffer.Header.VirtualAddress = (PVOID)(UINTN)FwFrameBufferPhysical;
    FrameBuffer.Mode = FwFrameBufferMode;
    FrameBuffer.Width = FwFrameBufferWidth;
    FrameBuffer.Height = FwFrameBufferHeight;
    FrameBuffer.BitsPerPixel = FwFrameBufferBitsPerPixel;
    FrameBuffer.PixelsPerScanLine = FrameBuffer.Width;
    FrameBuffer.Header.Size = FrameBuffer.Height *
                              FrameBuffer.PixelsPerScanLine *
                              (FrameBuffer.BitsPerPixel / BITS_PER_BYTE);

    if (FrameBuffer.Mode == BaseVideoModeFrameBuffer) {
        FrameBuffer.RedMask = 0x00FF0000;
        FrameBuffer.GreenMask = 0x0000FF00;
        FrameBuffer.BlueMask = 0x000000FF;
    }

    Status = VidInitialize(&FrameBuffer);
    if (!KSUCCESS(Status)) {
        goto InitializeVideoEnd;
    }

InitializeVideoEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
FwpPcatSetTextCursor (
    UCHAR DisplayPage,
    UCHAR Row,
    UCHAR Column
    )

/*++

Routine Description:

    This routine sets the text cursor position in the text mode BIOS.

Arguments:

    DisplayPage - Supplies the display page to set for. Supply 0 by default.

    Row - Supplies the row to set the cursor position to. Set to off the screen
        to hide the cursor.

    Column - Supplies the column to set the cursor position to.

Return Value:

    None.

--*/

{

    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x10);
    if (!KSUCCESS(Status)) {
        goto SetTextCursorEnd;
    }

    //
    // Set up the call to int 10, function 2, Set Cursor Position.
    //

    RealModeContext.Eax = INT10_SET_CURSOR_POSITION << BITS_PER_BYTE;
    RealModeContext.Ebx = DisplayPage << BITS_PER_BYTE;
    RealModeContext.Edx = (Row << BITS_PER_BYTE) | Column;

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);

SetTextCursorEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return;
}

KSTATUS
FwpPcatGetVesaInformation (
    PVESA_INFORMATION Information
    )

/*++

Routine Description:

    This routine attempts to get the VESA information structure.

Arguments:

    Information - Supplies a pointer where the VESA information will be
        returned.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    STATUS_CHECKSUM_MISMATCH if the structure signature did not match.

    Other error codes.

--*/

{

    PVESA_INFORMATION InformationData;
    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x10);
    if (!KSUCCESS(Status)) {
        goto GetVesaInformationEnd;
    }

    //
    // Copy the signature into the data page.
    //

    InformationData = RealModeContext.DataPage.Page;
    InformationData->Signature = Information->Signature;

    //
    // Set up the call to int 10, VESA function 0, get information.
    //

    RealModeContext.Eax = VESA_FUNCTION_GET_VESA_INFORMATION;
    RealModeContext.Es =
                  ADDRESS_TO_SEGMENT(RealModeContext.DataPage.RealModeAddress);

    RealModeContext.Edi = RealModeContext.DataPage.RealModeAddress & 0x0F;

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eax & 0x00FF) != 0x4F)) {

        Status = STATUS_FIRMWARE_ERROR;
        goto GetVesaInformationEnd;
    }

    RtlCopyMemory(Information, InformationData, sizeof(VESA_INFORMATION));
    Status = STATUS_SUCCESS;

GetVesaInformationEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

KSTATUS
FwpPcatSetBestVesaMode (
    PUSHORT ModeList
    )

/*++

Routine Description:

    This routine attempts to find and set a VESA mode at 1024x768 at a
    bit-depth of 24. If it cannot find that, it settles for the closest thing
    to it.

Arguments:

    ModeList - Supplies a pointer to the list of modes as enumerated in the VESA
        information structure.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_NOT_FOUND if no such mode could be found.

--*/

{

    ULONG Depth;
    ULONG Height;
    USHORT ModeNumber;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONG Width;

    if (FwVesaUseHighestResolution != FALSE) {
        Status = FwpPcatFindHighestResolutionVesaMode(ModeList,
                                                      &Width,
                                                      &Height,
                                                      &Depth,
                                                      &ModeNumber,
                                                      &PhysicalAddress);

        if (KSUCCESS(Status)) {
            Status = FwpPcatSetVesaMode(ModeNumber);
        }

        goto SetBestVideoModeEnd;
    }

    //
    // Try for 1024x769 at 32bpp.
    //

    Width = 1024;
    Height = 768;
    Depth = 32;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 1024x768 at 24bpp.
    //

    Depth = 24;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 1024x768 at 16bpp.
    //

    Depth = 16;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 1024x768 at 8bpp.
    //

    Depth = 8;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 1024x600 at 24bpp.
    //

    Width = 1024;
    Height = 600;
    Depth = 24;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 1024x600 at 16bpp.
    //

    Depth = 16;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 800x600 at 32bpp.
    //

    Width = 800;
    Height = 600;
    Depth = 32;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 800x600 at 24bpp.
    //

    Depth = 24;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 800x600 at 16bpp.
    //

    Depth = 16;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 800x600 at 8bpp.
    //

    Depth = 8;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    //
    // Try for 800x600 at 4bpp.
    //

    Depth = 4;
    Status = FwpPcatFindVesaMode(ModeList,
                                 Width,
                                 Height,
                                 Depth,
                                 &ModeNumber,
                                 &PhysicalAddress);

    if (KSUCCESS(Status)) {
        Status = FwpPcatSetVesaMode(ModeNumber);
        if (KSUCCESS(Status)) {
            goto SetBestVideoModeEnd;
        }
    }

    Status = STATUS_NOT_FOUND;

SetBestVideoModeEnd:
    if (KSUCCESS(Status)) {
        FwFrameBufferMode = BaseVideoModeFrameBuffer;
        FwFrameBufferPhysical = PhysicalAddress;
        FwFrameBufferWidth = Width;
        FwFrameBufferHeight = Height;
        FwFrameBufferBitsPerPixel = Depth;
    }

    return Status;
}

KSTATUS
FwpPcatFindVesaMode (
    PUSHORT ModeList,
    ULONG Width,
    ULONG Height,
    ULONG BitsPerPixel,
    PUSHORT ModeNumber,
    PPHYSICAL_ADDRESS FrameBufferAddress
    )

/*++

Routine Description:

    This routine attempts to find a VESA mode with a linear graphical
    framebuffer and the given width, height, and color depth.

Arguments:

    ModeList - Supplies a pointer to the list of modes as enumerated in the VESA
        information structure.

    Width - Supplies the desired width, in pixels or characters, of the video
        mode.

    Height - Supplies the desired height, in pixels or characters, of the video
        mode.

    BitsPerPixel - Supplies the desired color depth of the video mode.

    ModeNumber - Supplies a pointer where the mode number of the mode matching
        the characteristics will be returned.

    FrameBufferAddress - Supplies a pointer where the physical address of the
        frame buffer matching the mode will be returned on success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_NOT_FOUND if no such mode could be found.

--*/

{

    ULONG Attributes;
    ULONG ModeCount;
    VESA_MODE_INFORMATION ModeInformation;
    KSTATUS Status;

    *ModeNumber = -1;
    *FrameBufferAddress = INVALID_PHYSICAL_ADDRESS;
    ModeCount = 100;
    Attributes = VESA_MODE_ATTRIBUTE_SUPPORTED |
                 VESA_MODE_ATTRIBUTE_GRAPHICS |
                 VESA_MODE_ATTRIBUTE_LINEAR;

    while ((*ModeList != 0xFFFF) && (ModeCount != 0)) {
        Status = FwpPcatGetVesaModeInformation(*ModeList, &ModeInformation);
        if (KSUCCESS(Status)) {
            if ((ModeInformation.XResolution == Width) &&
                (ModeInformation.YResolution == Height) &&
                (ModeInformation.BitsPerPixel == BitsPerPixel) &&
                ((ModeInformation.ModeAttributes & Attributes) == Attributes)) {

                *ModeNumber = *ModeList;
                *FrameBufferAddress = ModeInformation.PhysicalBasePointer;
                Status = STATUS_SUCCESS;
                goto FindVesaModeEnd;
            }
        }

        //
        // Move on to the next mode.
        //

        ModeList += 1;
        ModeCount -= 1;
    }

    Status = STATUS_NOT_FOUND;

FindVesaModeEnd:
    return Status;
}

KSTATUS
FwpPcatFindHighestResolutionVesaMode (
    PUSHORT ModeList,
    PULONG Width,
    PULONG Height,
    PULONG BitsPerPixel,
    PUSHORT ModeNumber,
    PPHYSICAL_ADDRESS FrameBufferAddress
    )

/*++

Routine Description:

    This routine attempts to find the VESA mode with a linear graphical
    framebuffer and the highest width, height, and color depth.

Arguments:

    ModeList - Supplies a pointer to the list of modes as enumerated in the VESA
        information structure.

    Width - Supplies a pointer where the video width in pixels will be returned.

    Height - Supplies a pointer where the video height in pixels will be
        returned.

    BitsPerPixel - Supplies a pointer where the color depth of the best mode
        will be returned.

    ModeNumber - Supplies a pointer where the mode number of the mode matching
        the characteristics will be returned.

    FrameBufferAddress - Supplies a pointer where the physical address of the
        frame buffer matching the mode will be returned on success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_NOT_FOUND if no modes could be found.

--*/

{

    ULONG Attributes;
    ULONG ModeCount;
    VESA_MODE_INFORMATION ModeInformation;
    KSTATUS Status;

    *ModeNumber = -1;
    *Width = 0;
    *Height = 0;
    *BitsPerPixel = 0;
    *FrameBufferAddress = INVALID_PHYSICAL_ADDRESS;
    Attributes = VESA_MODE_ATTRIBUTE_SUPPORTED |
                 VESA_MODE_ATTRIBUTE_GRAPHICS |
                 VESA_MODE_ATTRIBUTE_LINEAR;

    ModeCount = 100;
    Status = STATUS_NOT_FOUND;
    while ((*ModeList != 0xFFFF) && (ModeCount != 0)) {
        Status = FwpPcatGetVesaModeInformation(*ModeList, &ModeInformation);
        if (KSUCCESS(Status)) {
            if ((ModeInformation.ModeAttributes & Attributes) == Attributes) {

                //
                // Find the highest resolution with the best depth.
                //

                if ((((ModeInformation.XResolution > *Width) ||
                      (ModeInformation.YResolution > *Height)) &&
                     (ModeInformation.BitsPerPixel >= *BitsPerPixel)) ||
                    ((ModeInformation.XResolution >= *Width) &&
                     (ModeInformation.YResolution >= *Height) &&
                     (ModeInformation.BitsPerPixel > *BitsPerPixel))) {

                    *Width = ModeInformation.XResolution;
                    *Height = ModeInformation.YResolution;
                    *BitsPerPixel = ModeInformation.BitsPerPixel;
                    *ModeNumber = *ModeList;
                    *FrameBufferAddress = ModeInformation.PhysicalBasePointer;
                    Status = STATUS_SUCCESS;
                }
            }
        }

        //
        // Move on to the next mode.
        //

        ModeList += 1;
        ModeCount -= 1;
    }

    return Status;
}

KSTATUS
FwpPcatGetVesaModeInformation (
    USHORT ModeNumber,
    PVESA_MODE_INFORMATION ModeInformation
    )

/*++

Routine Description:

    This routine attempts to get detailed information for the given VESA mode
    number.

Arguments:

    ModeNumber - Supplies the mode number to query.

    ModeInformation - Supplies a pointer where the detailed mode information
        will be returned.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes on other failures.

--*/

{

    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x10);
    if (!KSUCCESS(Status)) {
        goto GetVesaModeInformationEnd;
    }

    //
    // Set up the call to int 10, VESA function 1, get mode information.
    //

    RealModeContext.Eax = VESA_FUNCTION_GET_MODE_INFORMATION;
    RealModeContext.Es =
                  ADDRESS_TO_SEGMENT(RealModeContext.DataPage.RealModeAddress);

    RealModeContext.Edi = RealModeContext.DataPage.RealModeAddress & 0x0F;
    RealModeContext.Ecx = ModeNumber;

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eax & 0x00FF) != 0x4F)) {

        Status = STATUS_FIRMWARE_ERROR;
        goto GetVesaModeInformationEnd;
    }

    RtlCopyMemory(ModeInformation,
                  RealModeContext.DataPage.Page,
                  sizeof(VESA_MODE_INFORMATION));

    Status = STATUS_SUCCESS;

GetVesaModeInformationEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

KSTATUS
FwpPcatSetVesaMode (
    USHORT ModeNumber
    )

/*++

Routine Description:

    This routine attempts to set the given VESA mode.

Arguments:

    ModeNumber - Supplies the mode number to set.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes on other failures.

--*/

{

    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x10);
    if (!KSUCCESS(Status)) {
        goto PcatSetVesaModeEnd;
    }

    //
    // Set up the call to int 10, VESA function 1, get mode information.
    //

    RealModeContext.Eax = VESA_FUNCTION_SET_MODE;
    RealModeContext.Ebx = ModeNumber |
                          VESA_MODE_NUMBER_USE_LINEAR_MODEL |
                          VESA_MODE_NUMBER_DONT_CLEAR_DISPLAY;

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eax & 0x00FF) != 0x4F)) {

        Status = STATUS_FIRMWARE_ERROR;
        goto PcatSetVesaModeEnd;
    }

    Status = STATUS_SUCCESS;

PcatSetVesaModeEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

