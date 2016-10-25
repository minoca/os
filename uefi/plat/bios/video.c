/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    video.c

Abstract:

    This module implements support for VESA BIOS video services via int 0x10.

Author:

    Evan Green 25-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/uefi/protocol/graphout.h>
#include "biosfw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts the segmented addresses given in the VESA information
// structure into linear addresses that can be dereferenced in protected mode.
//

#define VESA_SEGMENTED_TO_LINEAR_ADDRESS(_Address) \
    (VOID *)((((_Address) & 0xFFFF0000) >> 12) +   \
             ((_Address) & 0x0000FFFF))

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_VESA_DEVICE_MAGIC 0x61736556 // 'aseV'

#define EFI_VESA_DEVICE_GUID                                \
    {                                                       \
        0x19EEE1EB, 0x8F2A, 0x4DFA,                         \
        {0xB0, 0xF9, 0xB1, 0x0B, 0xD5, 0xB8, 0x71, 0xB9}    \
    }

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
    UINT32 Signature;
    UINT16 VesaVersion;
    UINT32 OemStringPointer;
    UINT32 Capabilities;
    UINT32 VideoModePointer;
    UINT16 TotalMemoryBlocks;
    UINT16 OemSoftwareRevision;
    UINT32 OemVendorNamePointer;
    UINT32 OemProductNamePointer;
    UINT32 OemProductRevisionPointer;
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
    UINT16 ModeAttributes;
    UINT8 WindowAAttributes;
    UINT8 WindowBAttributes;
    UINT16 WindowGranularity;
    UINT16 WindowSize;
    UINT16 WindowASegment;
    UINT16 WindowBSegment;
    UINT32 WindowFunctionPointer;
    UINT16 BytesPerScanLine;
    UINT16 XResolution;
    UINT16 YResolution;
    UINT8 XCharacterSize;
    UINT8 YCharacterSize;
    UINT8 NumberOfPlanes;
    UINT8 BitsPerPixel;
    UINT8 NumberOfBanks;
    UINT8 MemoryModel;
    UINT8 BankSize;
    UINT8 NumberOfImagePages;
    UINT8 Reserved1;
    UINT8 RedMaskSize;
    UINT8 RedFieldPosition;
    UINT8 GreenMaskSize;
    UINT8 GreenFieldPosition;
    UINT8 BlueMaskSize;
    UINT8 BlueFieldPosition;
    UINT8 ReservedMaskSize;
    UINT8 ReservedFieldPosition;
    UINT8 DirectColorModeInformation;
    UINT32 PhysicalBasePointer;
    UINT32 OffScreenMemoryOffset;
    UINT16 OffScreenMemorySize;
    //UCHAR Reserved2[206];
} PACKED VESA_MODE_INFORMATION, *PVESA_MODE_INFORMATION;

/*++

Structure Description:

    This structure stores the VESA graphics output mode information.

Members:

    Information - Stores the information structure.

    VesaModeNumber - Stores the VESA mode number associated with this mode.

    BitsPerPixels - Stores the number of bits of data in a pixel.

    FrameBufferBase - Stores the frame buffer base address associated with this
        mode.

    Failed - Stores a boolean indicating if a failure occurred trying to set
        this mode.

--*/

typedef struct _EFI_VESA_MODE {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Information;
    UINT16 VesaModeNumber;
    UINT16 BitsPerPixel;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    BOOLEAN Failed;
} EFI_VESA_MODE, *PEFI_VESA_MODE;

/*++

Structure Description:

    This structure stores the internal context for a VESA device.

Members:

    Magic - Stores the constant magic value EFI_VESA_DEVICE_MAGIC.

    Handle - Stores the graphics out handle.

    GraphicsOut - Stores the graphics output protocol.

    GraphicsOutMode - Stores the graphics output protocol mode.

--*/

typedef struct _EFI_VESA_DEVICE {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOut;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GraphicsOutMode;
} EFI_VESA_DEVICE, *PEFI_VESA_DEVICE;

/*++

Structure Description:

    This structure stores the structure of a VESA device path.

Members:

    VendorPath - Stores the vendor path portion of the device path.

    End - Stores the end device path node.

--*/

typedef struct _EFI_VESA_DEVICE_PATH {
    VENDOR_DEVICE_PATH VendorPath;
    EFI_DEVICE_PATH_PROTOCOL End;
} EFI_VESA_DEVICE_PATH, *PEFI_VESA_DEVICE_PATH;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipPcatGraphicsQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

EFIAPI
EFI_STATUS
EfipPcatGraphicsSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    );

EFIAPI
EFI_STATUS
EfipPcatGraphicsBlt (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
    EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    UINTN SourceX,
    UINTN SourceY,
    UINTN DestinationX,
    UINTN DestinationY,
    UINTN Width,
    UINTN Height,
    UINTN Delta
    );

EFI_STATUS
EfipPcatEnumerateVesaModes (
    UINT16 *VesaModeList,
    UINTN VesaModeCount
    );

UINTN
EfipPcatSelectInitialVideoMode (
    VOID
    );

EFI_STATUS
EfipPcatGetVesaInformation (
    PVESA_INFORMATION Information
    );

EFI_STATUS
EfipPcatGetVesaModeInformation (
    UINT16 ModeNumber,
    PVESA_MODE_INFORMATION ModeInformation
    );

EFI_STATUS
EfipPcatSetVesaMode (
    UINT16 ModeNumber
    );

//
// -------------------------------------------------------------------- Globals
//

PEFI_VESA_MODE EfiVesaModes;
UINTN EfiVesaModeCount;

EFI_VESA_DEVICE_PATH EfiVesaDevicePathTemplate = {
    {
        {
            HARDWARE_DEVICE_PATH,
            HW_VENDOR_DP,
            sizeof(VENDOR_DEVICE_PATH)
        },

        EFI_VESA_DEVICE_GUID,
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipPcatEnumerateVideo (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the video display on a BIOS machine.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    PEFI_VESA_DEVICE Device;
    VESA_INFORMATION Information;
    PEFI_VESA_MODE Mode;
    UINT16 ModeCount;
    UINTN ModeIndex;
    UINT16 *ModeList;
    UINTN SelectedMode;
    EFI_STATUS Status;
    UINT16 VesaModeList[VESA_MAX_MODES];

    //
    // Attempt to get the VESA information structure.
    //

    Information.VesaVersion = 0;
    Information.Signature = VESA_2_SIGNATURE;
    Status = EfipPcatGetVesaInformation(&Information);
    if (EFI_ERROR(Status)) {
        goto PcatEnumerateVideoEnd;
    }

    if ((Information.Signature != VESA_1_SIGNATURE) ||
        (Information.VesaVersion < 0x0200)) {

        Status = EFI_UNSUPPORTED;
        goto PcatEnumerateVideoEnd;
    }

    ModeList = VESA_SEGMENTED_TO_LINEAR_ADDRESS(Information.VideoModePointer);

    //
    // Copy the mode list to the global.
    //

    ModeCount = 0;
    while ((ModeList[ModeCount] != 0xFFFF) &&
           (ModeCount < VESA_MAX_MODES - 1)) {

        VesaModeList[ModeCount] = ModeList[ModeCount];
        ModeCount += 1;
    }

    Status = EfipPcatEnumerateVesaModes(VesaModeList, ModeCount);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Attempt to find and switch to the best video mode.
    //

    Mode = NULL;
    for (ModeIndex = 0; ModeIndex < EfiVesaModeCount; ModeIndex += 1) {
        SelectedMode = EfipPcatSelectInitialVideoMode();
        Mode = &(EfiVesaModes[SelectedMode]);

        //
        // All video modes have been tried.
        //

        if (Mode->Failed != FALSE) {
            Status = EFI_UNSUPPORTED;
            break;
        }

        Status = EfipPcatSetVesaMode(Mode->VesaModeNumber);
        if (!EFI_ERROR(Status)) {
            break;
        }

        Mode->Failed = TRUE;
    }

    if (EFI_ERROR(Status)) {
        goto PcatEnumerateVideoEnd;
    }

    //
    // Everything's all set up, create the graphics output protocol.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_VESA_DEVICE),
                             (VOID **)&Device);

    if (EFI_ERROR(Status)) {
        goto PcatEnumerateVideoEnd;
    }

    EfiSetMem(Device, sizeof(EFI_VESA_DEVICE), 0);
    Device->Magic = EFI_VESA_DEVICE_MAGIC;
    Device->GraphicsOut.QueryMode = EfipPcatGraphicsQueryMode;
    Device->GraphicsOut.SetMode = EfipPcatGraphicsSetMode;
    Device->GraphicsOut.Blt = EfipPcatGraphicsBlt;
    Device->GraphicsOut.Mode = &(Device->GraphicsOutMode);
    Device->GraphicsOutMode.MaxMode = EfiVesaModeCount;
    Device->GraphicsOutMode.Mode = ModeIndex;
    Device->GraphicsOutMode.Info = &(Mode->Information);
    Device->GraphicsOutMode.SizeOfInfo =
                                  sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    Device->GraphicsOutMode.FrameBufferBase = Mode->FrameBufferBase;
    Device->GraphicsOutMode.FrameBufferSize =
                                          Mode->Information.PixelsPerScanLine *
                                          (Mode->BitsPerPixel / 8) *
                                          Mode->Information.VerticalResolution;

    Status = EfiInstallMultipleProtocolInterfaces(
                                                &(Device->Handle),
                                                &EfiGraphicsOutputProtocolGuid,
                                                &(Device->GraphicsOut),
                                                &EfiDevicePathProtocolGuid,
                                                &EfiVesaDevicePathTemplate,
                                                NULL);

    if (EFI_ERROR(Status)) {
        EfiFreePool(Device);
    }

PcatEnumerateVideoEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipPcatGraphicsQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    )

/*++

Routine Description:

    This routine returns information about available graphics modes that the
    graphics device and set of active video output devices support.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ModeNumber - Supplies the mode number to return information about.

    SizeOfInfo - Supplies a pointer that on input contains the size in bytes of
        the information buffer.

    Info - Supplies a pointer where a callee-allocated buffer will be returned
        containing information about the mode. The caller is responsible for
        calling FreePool to free this data.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a hardware error occurred trying to retrieve the video
    mode.

    EFI_INVALID_PARAMETER if the mode number is not valid.

--*/

{

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Information;
    EFI_STATUS Status;

    if ((ModeNumber >= EfiVesaModeCount) || (SizeOfInfo == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                             (VOID **)&Information);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Information,
               &(EfiVesaModes[ModeNumber].Information),
               sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

    *Info = Information;
    *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipPcatGraphicsSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    )

/*++

Routine Description:

    This routine sets the video device into the specified mode and clears the
    visible portions of the output display to black.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ModeNumber - Supplies the mode number to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a hardware error occurred trying to set the video mode.

    EFI_UNSUPPORTED if the mode number is not supported by this device.

--*/

{

    PEFI_VESA_MODE Mode;
    EFI_STATUS Status;

    if (ModeNumber >= EfiVesaModeCount) {
        return EFI_UNSUPPORTED;
    }

    Mode = &(EfiVesaModes[ModeNumber]);
    Status = EfipPcatSetVesaMode(Mode->VesaModeNumber);
    if (!EFI_ERROR(Status)) {
        This->Mode->Info = &(Mode->Information);
        This->Mode->Mode = ModeNumber;
        This->Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
        This->Mode->FrameBufferBase = Mode->FrameBufferBase;
        This->Mode->FrameBufferSize = Mode->Information.PixelsPerScanLine *
                                      (Mode->BitsPerPixel / 8) *
                                      Mode->Information.VerticalResolution;
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfipPcatGraphicsBlt (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer,
    EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    UINTN SourceX,
    UINTN SourceY,
    UINTN DestinationX,
    UINTN DestinationY,
    UINTN Width,
    UINTN Height,
    UINTN Delta
    )

/*++

Routine Description:

    This routine performs a Blt (copy) operation of pixels on the graphics
    screen. Blt stands for Block Transfer for those not up on their video lingo.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BltBuffer - Supplies an optional pointer to the data to transfer to the
        graphics screen. The size must be at least width * height *
        sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL).

    BltOperation - Supplies the operation to perform when copying the buffer to
        the screen.

    SourceX - Supplies the X coordinate of the source of the operation.

    SourceY - Supplies the Y coordinate of the source of the operation.

    DestinationX - Supplies the X coordinate of the destination of the
        operation.

    DestinationY - Supplies the Y coordinate of the destination of the
        operation.

    Width - Supplies the width of the rectangle in pixels.

    Height - Supplies the height of the rectangle in pixels.

    Delta - Supplies an optional number of bytes in a row of the given buffer.
        If a delta of zero is used, the entire buffer is being operated on.
        This is not used for EfiBltVideoFill or EfiBltVideoToVideo operations.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the operation was not valid.

    EFI_DEVICE_ERROR if a hardware error occurred and the request could not be
    completed.

--*/

{

    return EFI_UNSUPPORTED;
}

EFI_STATUS
EfipPcatEnumerateVesaModes (
    UINT16 *VesaModeList,
    UINTN VesaModeCount
    )

/*++

Routine Description:

    This routine creates the mode information array from the VESA mode list.

Arguments:

    VesaModeList - Supplies a pointer to the list of VESA modes.

    VesaModeCount - Supplies the number of valid mode elements in the supplied
        list.

Return Value:

    EFI status.

--*/

{

    UINTN AllocationSize;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Information;
    UINTN ModeCount;
    PEFI_VESA_MODE Modes;
    EFI_STATUS Status;
    VESA_MODE_INFORMATION VesaInformation;
    UINTN VesaModeIndex;

    Modes = NULL;
    if (VesaModeCount == 0) {
        return EFI_DEVICE_ERROR;
    }

    AllocationSize = sizeof(EFI_VESA_MODE) * VesaModeCount;
    Status = EfiAllocatePool(EfiBootServicesData,
                             AllocationSize,
                             (VOID **)&Modes);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiSetMem(Modes, AllocationSize, 0);
    ModeCount = 0;
    for (VesaModeIndex = 0; VesaModeIndex < VesaModeCount; VesaModeIndex += 1) {
        Status = EfipPcatGetVesaModeInformation(VesaModeList[VesaModeIndex],
                                                &VesaInformation);

        if (EFI_ERROR(Status)) {
            continue;
        }

        //
        // Skip non-graphical modes and modes without a linear frame buffer.
        //

        if (((VesaInformation.ModeAttributes &
              VESA_MODE_ATTRIBUTE_GRAPHICS) == 0) ||
            ((VesaInformation.ModeAttributes &
              VESA_MODE_ATTRIBUTE_LINEAR) == 0)) {

            continue;
        }

        //
        // Fill out the EFI mode information based on the VESA mode information.
        //

        Information = &(Modes[ModeCount].Information);
        Information->Version = 0;
        Information->HorizontalResolution = VesaInformation.XResolution;
        Information->VerticalResolution = VesaInformation.YResolution;
        Information->PixelFormat = PixelBitMask;
        Information->PixelInformation.RedMask =
            ((1 << VesaInformation.RedMaskSize) - 1) <<
            VesaInformation.RedFieldPosition;

        Information->PixelInformation.GreenMask =
            ((1 << VesaInformation.GreenMaskSize) - 1) <<
            VesaInformation.GreenFieldPosition;

        Information->PixelInformation.BlueMask =
            ((1 << VesaInformation.BlueMaskSize) - 1) <<
            VesaInformation.BlueFieldPosition;

        Information->PixelInformation.ReservedMask =
            ((1 << VesaInformation.ReservedMaskSize) - 1) <<
            VesaInformation.ReservedFieldPosition;

        Information->PixelsPerScanLine = VesaInformation.BytesPerScanLine /
                                         (VesaInformation.BitsPerPixel / 8);

        if ((Information->PixelInformation.RedMask |
             Information->PixelInformation.GreenMask |
             Information->PixelInformation.BlueMask |
             Information->PixelInformation.ReservedMask) == 0) {

            continue;
        }

        Modes[ModeCount].VesaModeNumber = VesaModeList[VesaModeIndex];
        Modes[ModeCount].BitsPerPixel = VesaInformation.BitsPerPixel;
        Modes[ModeCount].FrameBufferBase = VesaInformation.PhysicalBasePointer;
        ModeCount += 1;
    }

    if (EfiVesaModes != NULL) {
        EfiFreePool(EfiVesaModes);
        EfiVesaModes = NULL;
    }

    EfiVesaModeCount = 0;
    if (ModeCount == 0) {
        EfiFreePool(Modes);
        return EFI_UNSUPPORTED;
    }

    EfiVesaModes = Modes;
    EfiVesaModeCount = ModeCount;
    return EFI_SUCCESS;
}

UINTN
EfipPcatSelectInitialVideoMode (
    VOID
    )

/*++

Routine Description:

    This routine attempts to select an initial VESA graphics mode.

Arguments:

    None.

Return Value:

    Returns the mode index of the best video mode.

--*/

{

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Information;
    PEFI_VESA_MODE Mode;
    UINTN ModeIndex;
    BOOLEAN NewWinner;
    UINTN WinnerBitsPerPixel;
    UINTN WinnerIndex;
    UINTN WinnerX;
    UINTN WinnerY;

    //
    // Find the biggest supported video mode.
    //

    WinnerIndex = 0;
    WinnerBitsPerPixel = 0;
    WinnerX = 0;
    WinnerY = 0;
    for (ModeIndex = 0; ModeIndex < EfiVesaModeCount; ModeIndex += 1) {
        Mode = &(EfiVesaModes[ModeIndex]);
        Information = &(Mode->Information);
        if (Mode->Failed != FALSE) {
            continue;
        }

        NewWinner = FALSE;

        //
        // If the resolution is just better, take it.
        //

        if ((Information->HorizontalResolution > WinnerX) &&
            (Information->VerticalResolution > WinnerY)) {

            NewWinner = TRUE;

        //
        // If the resolution is at least the same but the bits per pixel is
        // better, take it.
        //

        } else if ((Information->HorizontalResolution >= WinnerX) &&
                   (Information->VerticalResolution >= WinnerY) &&
                   (Mode->BitsPerPixel > WinnerBitsPerPixel)) {

            NewWinner = TRUE;
        }

        if (NewWinner != FALSE) {
            WinnerX = Information->HorizontalResolution;
            WinnerY = Information->VerticalResolution;
            WinnerBitsPerPixel = Mode->BitsPerPixel;
            WinnerIndex = ModeIndex;
        }
    }

    return WinnerIndex;
}

EFI_STATUS
EfipPcatGetVesaInformation (
    PVESA_INFORMATION Information
    )

/*++

Routine Description:

    This routine attempts to get the VESA information structure.

Arguments:

    Information - Supplies a pointer where the VESA information will be
        returned.

Return Value:

    EFI Status code.

--*/

{

    PVESA_INFORMATION InformationData;
    BIOS_CALL_CONTEXT RealModeContext;
    EFI_STATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = EfipCreateBiosCallContext(&RealModeContext, 0x10);
    if (EFI_ERROR(Status)) {
        goto GetVesaInformationEnd;
    }

    //
    // Copy the signature into the data page.
    //

    InformationData = RealModeContext.DataPage;
    InformationData->Signature = Information->Signature;

    //
    // Set up the call to int 10, VESA function 0, get information.
    //

    RealModeContext.Eax = VESA_FUNCTION_GET_VESA_INFORMATION;
    RealModeContext.Es = ADDRESS_TO_SEGMENT((UINTN)RealModeContext.DataPage);
    RealModeContext.Edi = (UINTN)(RealModeContext.DataPage) & 0x0F;

    //
    // Execute the firmware call.
    //

    EfipExecuteBiosCall(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eax & 0x00FF) != 0x4F)) {

        Status = EFI_DEVICE_ERROR;
        goto GetVesaInformationEnd;
    }

    EfiCopyMem(Information, InformationData, sizeof(VESA_INFORMATION));
    Status = EFI_SUCCESS;

GetVesaInformationEnd:
    EfipDestroyBiosCallContext(&RealModeContext);
    return Status;
}

EFI_STATUS
EfipPcatGetVesaModeInformation (
    UINT16 ModeNumber,
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

    EFI Status code.

--*/

{

    BIOS_CALL_CONTEXT RealModeContext;
    EFI_STATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = EfipCreateBiosCallContext(&RealModeContext, 0x10);
    if (EFI_ERROR(Status)) {
        goto GetVesaModeInformationEnd;
    }

    //
    // Set up the call to int 10, VESA function 1, get mode information.
    //

    RealModeContext.Eax = VESA_FUNCTION_GET_MODE_INFORMATION;
    RealModeContext.Es = ADDRESS_TO_SEGMENT((UINTN)RealModeContext.DataPage);
    RealModeContext.Edi = (UINTN)RealModeContext.DataPage & 0x0F;
    RealModeContext.Ecx = ModeNumber;

    //
    // Execute the firmware call.
    //

    EfipExecuteBiosCall(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eax & 0x00FF) != 0x4F)) {

        Status = EFI_DEVICE_ERROR;
        goto GetVesaModeInformationEnd;
    }

    EfiCopyMem(ModeInformation,
               RealModeContext.DataPage,
               sizeof(VESA_MODE_INFORMATION));

    Status = EFI_SUCCESS;

GetVesaModeInformationEnd:
    EfipDestroyBiosCallContext(&RealModeContext);
    return Status;
}

EFI_STATUS
EfipPcatSetVesaMode (
    UINT16 ModeNumber
    )

/*++

Routine Description:

    This routine attempts to set the given VESA mode.

Arguments:

    ModeNumber - Supplies the mode number to set.

Return Value:

    EFI Status code.

--*/

{

    BIOS_CALL_CONTEXT RealModeContext;
    EFI_STATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = EfipCreateBiosCallContext(&RealModeContext, 0x10);
    if (EFI_ERROR(Status)) {
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

    EfipExecuteBiosCall(&RealModeContext);

    //
    // Check for an error. The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eax & 0x00FF) != 0x4F)) {

        Status = EFI_DEVICE_ERROR;
        goto PcatSetVesaModeEnd;
    }

    Status = EFI_SUCCESS;

PcatSetVesaModeEnd:
    EfipDestroyBiosCallContext(&RealModeContext);
    return Status;
}

