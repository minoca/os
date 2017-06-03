/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>
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

#define VESA_SEGMENTED_TO_LINEAR_ADDRESS(_Address)      \
    (PVOID)(UINTN)((((_Address) & 0xFFFF0000) >> 12) +  \
                   ((_Address) & 0x0000FFFF))

//
// These macros convert a TrueColor 8-bit RGB 3:3:2 value into its VGA DAC
// register value. This is breaking the color up into its components, and then
// shifting it out to a max value of 0x3F.
//

#define TRUECOLOR_TO_PALETTE_RED(_Color) ((((_Color) >> 5) & 0x7) << 3)
#define TRUECOLOR_TO_PALETTE_GREEN(_Color) ((((_Color) >> 2) & 0x7) << 3)
#define TRUECOLOR_TO_PALETTE_BLUE(_Color) (((_Color) & 0x3) << 3)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the INT 10 function for setting several DAC registers at once (the
// color palette).
//

#define VIDEO_FUNCTION_SET_DAC_REGISTER_BLOCK 0x1012

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
#define VESA_FUNCTION_SET_PALETTE_CONTROL  0x4F08
#define VESA_FUNCTION_SET_PALETTE_ENTRIES  0x4F09

//
// Define the values for BL in the get/set palette control call.
//

#define VESA_PALETTE_CONTROL_SET 0x00
#define VESA_PALETTE_CONTROL_GET 0x01

//
// Define the values for BL in the get/set palette entries call.
//

#define VESA_PALETTE_SET_PRIMARY 0x00
#define VESA_PALETTE_GET_PRIMARY 0x01
#define VESA_PALETTE_SET_SECONDARY 0x02
#define VESA_PALETTE_GET_SECONDARY 0x03
#define VESA_PALETTE_SET_DURING_VERTICAL_TRACE 0x80

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

/*++

Structure Description:

    This structure stores the parameters for a requested video mode.

Members:

    XResolution - Stores the requested horizontal resolution in pixels.

    YResolution - Stores the requested vertical resolution in pixels.

    BitsPerPixel - Stores the requested pixel depth.

--*/

typedef struct _VIDEO_MODE_REQUEST {
    USHORT XResolution;
    USHORT YResolution;
    USHORT BitsPerPixel;
} VIDEO_MODE_REQUEST, *PVIDEO_MODE_REQUEST;

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
    PULONG RedMask,
    PULONG GreenMask,
    PULONG BlueMask,
    PUSHORT ModeNumber,
    PPHYSICAL_ADDRESS FrameBufferAddress
    );

KSTATUS
FwpPcatFindHighestResolutionVesaMode (
    PUSHORT ModeList,
    PULONG Width,
    PULONG Height,
    PULONG BitsPerPixel,
    PULONG RedMask,
    PULONG GreenMask,
    PULONG BlueMask,
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

VOID
FwpPcatGetColorMasks (
    PVESA_MODE_INFORMATION Mode,
    PULONG RedMask,
    PULONG GreenMask,
    PULONG BlueMask
    );

ULONG
FwpPcatCreatePixelMask (
    ULONG Position,
    ULONG Size
    );

VOID
FwpPcatSetPalette (
    VOID
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
ULONG FwFrameBufferRedMask;
ULONG FwFrameBufferGreenMask;
ULONG FwFrameBufferBlueMask;

BASE_VIDEO_CONTEXT FwVideoContext;

const VIDEO_MODE_REQUEST FwModePreferences[] = {
    {1024, 768, 32},
    {1024, 768, 24},
    {1024, 768, 16},
    {1024, 768, 8},
    {1024, 600, 24},
    {1024, 600, 16},
    {800, 600, 32},
    {800, 600, 24},
    {800, 600, 16},
    {640, 480, 24},
    {640, 480, 16},
    {640, 480, 4},
    {0, 0, 0}
};

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
                ModeCount = 0;
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
        FrameBuffer.RedMask = FwFrameBufferRedMask;
        FrameBuffer.GreenMask = FwFrameBufferGreenMask;
        FrameBuffer.BlueMask = FwFrameBufferBlueMask;
    }

    Status = VidInitialize(&FwVideoContext, &FrameBuffer);
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
        return;
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
        return Status;
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

    ULONG BlueMask;
    ULONG Depth;
    ULONG GreenMask;
    ULONG Height;
    USHORT ModeNumber;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG RedMask;
    const VIDEO_MODE_REQUEST *Request;
    KSTATUS Status;
    ULONG Width;

    RedMask = 0;
    GreenMask = 0;
    BlueMask = 0;
    if (FwVesaUseHighestResolution != FALSE) {
        Status = FwpPcatFindHighestResolutionVesaMode(ModeList,
                                                      &Width,
                                                      &Height,
                                                      &Depth,
                                                      &RedMask,
                                                      &GreenMask,
                                                      &BlueMask,
                                                      &ModeNumber,
                                                      &PhysicalAddress);

        if (KSUCCESS(Status)) {
            Status = FwpPcatSetVesaMode(ModeNumber);
        }

        goto SetBestVideoModeEnd;
    }

    //
    // Go down the list of requests trying to get one.
    //

    Request = FwModePreferences;
    while (Request->XResolution != 0) {
        Width = Request->XResolution;
        Height = Request->YResolution;
        Depth = Request->BitsPerPixel;
        Status = FwpPcatFindVesaMode(ModeList,
                                     Width,
                                     Height,
                                     Depth,
                                     &RedMask,
                                     &GreenMask,
                                     &BlueMask,
                                     &ModeNumber,
                                     &PhysicalAddress);

        if (KSUCCESS(Status)) {
            Status = FwpPcatSetVesaMode(ModeNumber);
            if (KSUCCESS(Status)) {
                goto SetBestVideoModeEnd;
            }
        }

        Request += 1;
    }

    Status = STATUS_NOT_FOUND;

SetBestVideoModeEnd:
    if (KSUCCESS(Status)) {
        FwFrameBufferMode = BaseVideoModeFrameBuffer;
        FwFrameBufferPhysical = PhysicalAddress;
        FwFrameBufferWidth = Width;
        FwFrameBufferHeight = Height;
        FwFrameBufferBitsPerPixel = Depth;
        FwFrameBufferRedMask = RedMask;
        FwFrameBufferGreenMask = GreenMask;
        FwFrameBufferBlueMask = BlueMask;
        if (Depth == 8) {
            FwpPcatSetPalette();
        }
    }

    return Status;
}

KSTATUS
FwpPcatFindVesaMode (
    PUSHORT ModeList,
    ULONG Width,
    ULONG Height,
    ULONG BitsPerPixel,
    PULONG RedMask,
    PULONG GreenMask,
    PULONG BlueMask,
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

    RedMask - Supplies a pointer where the red pixel mask will be returned.

    GreenMask - Supplies a pointer where the green pixel mask will be returned.

    BlueMask - Supplies a pointer where the blue pixel mask will be returned.

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
                FwpPcatGetColorMasks(&ModeInformation,
                                     RedMask,
                                     GreenMask,
                                     BlueMask);

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
    PULONG RedMask,
    PULONG GreenMask,
    PULONG BlueMask,
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

    RedMask - Supplies a pointer where the red pixel mask will be returned.

    GreenMask - Supplies a pointer where the green pixel mask will be returned.

    BlueMask - Supplies a pointer where the blue pixel mask will be returned.

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
                    FwpPcatGetColorMasks(&ModeInformation,
                                         RedMask,
                                         GreenMask,
                                         BlueMask);

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
        return Status;
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
        return Status;
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

VOID
FwpPcatGetColorMasks (
    PVESA_MODE_INFORMATION Mode,
    PULONG RedMask,
    PULONG GreenMask,
    PULONG BlueMask
    )

/*++

Routine Description:

    This routine returns the pixel format masks for a given mode.

Arguments:

    Mode - Supplies a pointer to the VESA mode information being translated.

    RedMask - Supplies a pointer where the red pixel mask will be returned.

    GreenMask - Supplies a pointer where the green pixel mask will be returned.

    BlueMask - Supplies a pointer where the blue pixel mask will be returned.

Return Value:

    None.

--*/

{

    //
    // In Packed Pixel format, 16 bit is 1:5:5:5, 24 bit is 8:8:8, and 32 bit
    // is 8:8:8:8.
    //

    if ((Mode->MemoryModel == VesaMemoryModelPackedPixel) ||
        (Mode->RedMaskSize == 0) ||
        (Mode->GreenMaskSize == 0) ||
        (Mode->BlueMaskSize == 0)) {

        switch (Mode->BitsPerPixel) {

        //
        // Assume 8-bit TrueColor, which might not be right.
        //

        case 8:
            *RedMask = 0x7 << 5;
            *GreenMask = 0x7 << 3;
            *BlueMask = 0x3;
            break;

        case 16:
            *RedMask = 0x1F << 10;
            *GreenMask = 0x1F << 5;
            *BlueMask = 0x1F;
            break;

        case 24:
        case 32:
        default:
            *RedMask = 0xFF << 16;
            *GreenMask = 0xFF << 8;
            *BlueMask = 0xFF;
            break;
        }

    } else {
        *RedMask = FwpPcatCreatePixelMask(Mode->RedFieldPosition,
                                          Mode->RedMaskSize);

        *GreenMask = FwpPcatCreatePixelMask(Mode->GreenFieldPosition,
                                            Mode->GreenMaskSize);

        *BlueMask = FwpPcatCreatePixelMask(Mode->BlueFieldPosition,
                                           Mode->BlueMaskSize);
    }

    return;
}

ULONG
FwpPcatCreatePixelMask (
    ULONG Position,
    ULONG Size
    )

/*++

Routine Description:

    This routine converts a bit position and size into a mask.

Arguments:

    Position - Supplies the bit position where this color's pixel bits start.

    Size - Supplies the number of bits the color gets in the pixel.

Return Value:

    Returns a mask of the specified bits.

--*/

{

    ULONG Index;
    ULONG Mask;

    Mask = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Mask |= 1 << (Position + Index);
    }

    return Mask;
}

VOID
FwpPcatSetPalette (
    VOID
    )

/*++

Routine Description:

    This routine sets an 8-bit color palette equivalent to TrueColor. Note that
    doing this will change the colors for Text mode too.

Arguments:

    None.

Return Value:

    None, as failure is not fatal.

--*/

{

    ULONG Color;
    PUCHAR Palette;
    REAL_MODE_CONTEXT RealModeContext;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x10);
    if (!KSUCCESS(Status)) {
        return;
    }

    //
    // Set up a BIOS call to set a block of DAC registers. BX contains the
    // first DAC register to set (0 - 0xFF) and CX contains the number of
    // registers to set (0 - 0xFF). ES:DX points to the table to set, which
    // should be 3 * (CX + 1).
    //

    RealModeContext.Eax = VIDEO_FUNCTION_SET_DAC_REGISTER_BLOCK;
    RealModeContext.Ebx = 0;
    RealModeContext.Ecx = 0x00FF;
    RealModeContext.Es =
                  ADDRESS_TO_SEGMENT(RealModeContext.DataPage.RealModeAddress);

    RealModeContext.Edx = RealModeContext.DataPage.RealModeAddress & 0x0F;

    //
    // Set up a TrueColor palette, in which the 8 bits of color are broken up
    // into 3 bits of red, 3 bits of green, and 2 bits of blue. The palette
    // registers are 6 bits wide each.
    //

    Palette = RealModeContext.DataPage.Page;
    for (Color = 0; Color < 256; Color += 1) {
        *Palette = TRUECOLOR_TO_PALETTE_RED(Color);
        Palette += 1;
        *Palette = TRUECOLOR_TO_PALETTE_GREEN(Color);
        Palette += 1;
        *Palette = TRUECOLOR_TO_PALETTE_BLUE(Color);
        Palette += 1;
    }

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return;
}

