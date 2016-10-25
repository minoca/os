/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    video.c

Abstract:

    This module implements support for the ARM Integrator/CP.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/uefi/protocol/graphout.h>
#include "integfw.h"
#include "dev/pl110.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_INTEGRATOR_VIDEO_DEVICE_GUID                    \
    {                                                       \
        0x19EEE1EB, 0x8F2A, 0x4DFA,                         \
        {0xB0, 0xF9, 0xB1, 0x0B, 0xD5, 0xB8, 0x71, 0x05}    \
    }

#define EFI_INTEGRATOR_VIDEO_DEVICE_MAGIC 0x4969564F // 'diVI'

//
// Define the default mode to initialize in.
//

#define EFI_INTEGRATOR_VIDEO_DEFAULT_MODE 0

#define EFI_INTEGRATOR_VIDEO_MODE_COUNT \
    (sizeof(EfiIntegratorVideoModes) / sizeof(EfiIntegratorVideoModes[0]))

//
// Define the size of the frame buffer to allocate, which should be large
// enough to support the largest resolution.
//

#define EFI_INTEGRATOR_FRAME_BUFFER_SIZE (1024 * 768 * sizeof(UINT32))

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the integrator graphics output mode information.

Members:

    Information - Stores the information structure.

--*/

typedef struct _EFI_INTEGRATOR_VIDEO_MODE {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Information;
} EFI_INTEGRATOR_VIDEO_MODE, *PEFI_INTEGRATOR_VIDEO_MODE;

/*++

Structure Description:

    This structure stores the structure of an Integrator video device path.

Members:

    VendorPath - Stores the vendor path portion of the device path.

    End - Stores the end device path node.

--*/

typedef struct _EFI_INTEGRATOR_VIDEO_DEVICE_PATH {
    VENDOR_DEVICE_PATH VendorPath;
    EFI_DEVICE_PATH_PROTOCOL End;
} EFI_INTEGRATOR_VIDEO_DEVICE_PATH, *PEFI_INTEGRATOR_VIDEO_DEVICE_PATH;

/*++

Structure Description:

    This structure stores the internal context for an OMAP4 video device.

Members:

    Magic - Stores the constant magic value EFI_INTEGRATOR_VIDEO_DEVICE_MAGIC.

    Handle - Stores the graphics out handle.

    GraphicsOut - Stores the graphics output protocol.

    GraphicsOutMode - Stores the graphics output protocol mode.

--*/

typedef struct _EFI_INTEGRATOR_VIDEO_DEVICE {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOut;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GraphicsOutMode;
} EFI_INTEGRATOR_VIDEO_DEVICE, *PEFI_INTEGRATOR_VIDEO_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipIntegratorGraphicsQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

EFIAPI
EFI_STATUS
EfipIntegratorGraphicsSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    );

EFIAPI
EFI_STATUS
EfipIntegratorGraphicsBlt (
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

//
// -------------------------------------------------------------------- Globals
//

EFI_PHYSICAL_ADDRESS EfiIntegratorFrameBuffer;

//
// Store the device path of the video controller.
//

EFI_INTEGRATOR_VIDEO_DEVICE_PATH EfiIntegratorVideoDevicePathTemplate = {
    {
        {
            HARDWARE_DEVICE_PATH,
            HW_VENDOR_DP,
            sizeof(VENDOR_DEVICE_PATH)
        },

        EFI_INTEGRATOR_VIDEO_DEVICE_GUID,
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

//
// Define the supported video modes.
//

EFI_INTEGRATOR_VIDEO_MODE EfiIntegratorVideoModes[] = {
    {
        {
            0,
            1024,
            768,
            PixelBitMask,
            {
                0x00FF0000,
                0x0000FF00,
                0x000000FF,
                0xFF000000
            },

            1024
        },
    },
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipIntegratorEnumerateVideo (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the display on the Integrator/CP.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    PEFI_INTEGRATOR_VIDEO_DEVICE Device;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    PEFI_INTEGRATOR_VIDEO_MODE Mode;
    EFI_STATUS Status;

    FrameBufferBase = -1;
    Device = NULL;
    Mode = &(EfiIntegratorVideoModes[EFI_INTEGRATOR_VIDEO_DEFAULT_MODE]);

    //
    // Allocate space for the frame buffer.
    //

    Status = EfiAllocatePages(
                           AllocateAnyPages,
                           EfiMemoryMappedIO,
                           EFI_SIZE_TO_PAGES(EFI_INTEGRATOR_FRAME_BUFFER_SIZE),
                           &FrameBufferBase);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipPl110Initialize(INTEGRATOR_PL110_BASE,
                                 FrameBufferBase,
                                 Mode->Information.HorizontalResolution,
                                 Mode->Information.VerticalResolution);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

    //
    // Everything's all set up, create the graphics output protocol.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_INTEGRATOR_VIDEO_DEVICE),
                             (VOID **)&Device);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

    EfiSetMem(Device, sizeof(EFI_INTEGRATOR_VIDEO_DEVICE), 0);
    Device->Magic = EFI_INTEGRATOR_VIDEO_DEVICE_MAGIC;
    EfiIntegratorFrameBuffer = FrameBufferBase;
    Device->GraphicsOut.QueryMode = EfipIntegratorGraphicsQueryMode;
    Device->GraphicsOut.SetMode = EfipIntegratorGraphicsSetMode;
    Device->GraphicsOut.Blt = EfipIntegratorGraphicsBlt;
    Device->GraphicsOut.Mode = &(Device->GraphicsOutMode);
    Device->GraphicsOutMode.MaxMode = EFI_INTEGRATOR_VIDEO_MODE_COUNT;
    Device->GraphicsOutMode.Mode = EFI_INTEGRATOR_VIDEO_DEFAULT_MODE;
    Device->GraphicsOutMode.Info = &(Mode->Information);
    Device->GraphicsOutMode.SizeOfInfo =
                                  sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    Device->GraphicsOutMode.FrameBufferBase = FrameBufferBase;
    Device->GraphicsOutMode.FrameBufferSize = EFI_INTEGRATOR_FRAME_BUFFER_SIZE;
    Status = EfiInstallMultipleProtocolInterfaces(
                                         &(Device->Handle),
                                         &EfiGraphicsOutputProtocolGuid,
                                         &(Device->GraphicsOut),
                                         &EfiDevicePathProtocolGuid,
                                         &EfiIntegratorVideoDevicePathTemplate,
                                         NULL);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

EnumerateVideoEnd:
    if (EFI_ERROR(Status)) {
        if (FrameBufferBase != -1) {
            EfiFreePages(FrameBufferBase,
                         EFI_SIZE_TO_PAGES(EFI_INTEGRATOR_FRAME_BUFFER_SIZE));
        }

        if (Device != NULL) {
            EfiFreePool(Device);
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipIntegratorGraphicsQueryMode (
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

    if ((ModeNumber >= EFI_INTEGRATOR_VIDEO_MODE_COUNT) ||
        (SizeOfInfo == NULL)) {

        return EFI_INVALID_PARAMETER;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                             (VOID **)&Information);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Information,
               &(EfiIntegratorVideoModes[ModeNumber].Information),
               sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

    *Info = Information;
    *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipIntegratorGraphicsSetMode (
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

    PEFI_INTEGRATOR_VIDEO_MODE Mode;
    EFI_STATUS Status;

    if (ModeNumber >= EFI_INTEGRATOR_VIDEO_MODE_COUNT) {
        return EFI_UNSUPPORTED;
    }

    Mode = &(EfiIntegratorVideoModes[ModeNumber]);
    Status = EfipPl110Initialize(INTEGRATOR_PL110_BASE,
                                 EfiIntegratorFrameBuffer,
                                 Mode->Information.HorizontalResolution,
                                 Mode->Information.VerticalResolution);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    This->Mode->Info = &(Mode->Information);
    This->Mode->Mode = ModeNumber;
    This->Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return Status;
}

EFIAPI
EFI_STATUS
EfipIntegratorGraphicsBlt (
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

