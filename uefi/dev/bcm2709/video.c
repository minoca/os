/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    video.c

Abstract:

    This module implements support for the BCM2709 SoC Family display
    controller.

Author:

    Chris Stevens 21-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/bcm2709.h>
#include <minoca/uefi/protocol/graphout.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_BCM2709_VIDEO_DEVICE_GUID                       \
    {                                                       \
        0x87FC0212, 0x9519, 0x11E4,                         \
        {0x92, 0x76, 0x04, 0x01, 0x0F, 0xDD, 0x74, 0x01}    \
    }

#define EFI_BCM2709_VIDEO_DEVICE_MAGIC 0x64695642 // 'diVB'

#define EFI_BCM2709_VIDEO_MODE_ARRAY_LENGTH \
    (sizeof(EfiBcm2709VideoModes) / sizeof(EfiBcm2709VideoModes[0]))

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the data necessary to get the current mode.

Members:

    Header - Stores a header that defines the total size of the messages being
        received from the mailbox.

    PhysicalResolution - Stores a request for the physical resolution when sent
        to the mailbox and receives the current physical resolution on return.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _EFI_BCM2709_VIDEO_GET_MODE {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_RESOLUTION PhysicalResolution;
    UINT32 EndTag;
} EFI_BCM2709_VIDEO_GET_MODE, *PEFI_BCM2709_VIDEO_GET_MODE;

/*++

Structure Description:

    This structure defines the data necessary to initialize video and get a
    frame buffer.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    PhysicalResolution - Stores a request to set the physical resolution and
        receives the current physical resolution.

    VirtualResolution - Stores a request to set the virtual resolution and
        receives the current virtual resolution.

    BitsPerPixel - Stores a request to set the bits per pixel and receives the
        current bits per pixel.

    PixelOrder - Stores a request to set the pixel order and receives the
        current pixel order.

    AlphaMode - Stores a request to set the alpha mode.

    VirtualOffset - Stores a request to set the virtual offset.

    Overscan - Stores a request to set the screen's overscan.

    Pitch - Stores a request to get the pitch information.

    FrameBuffer - Stores a request to get the frame buffer.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _EFI_BCM2709_VIDEO_INITIALIZE {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_RESOLUTION PhysicalResolution;
    BCM2709_MAILBOX_RESOLUTION VirtualResolution;
    BCM2709_MAILBOX_BITS_PER_PIXEL BitsPerPixelMessage;
    BCM2709_MAILBOX_PIXEL_ORDER PixelOrderMessage;
    BCM2709_MAILBOX_ALPHA_MODE AlphaModeMessage;
    BCM2709_MAILBOX_VIRTUAL_OFFSET VirtualOffset;
    BCM2709_MAILBOX_OVERSCAN OverscanMessage;
    BCM2709_MAILBOX_PITCH Pitch;
    BCM2709_MAILBOX_FRAME_BUFFER FrameBufferMessage;
    UINT32 EndTag;
} EFI_BCM2709_VIDEO_INITIALIZE, *PEFI_BCM2709_VIDEO_INITIALIZE;

/*++

Structure Description:

    This structure stores the structure of an BCM2709 video device path.

Members:

    VendorPath - Stores the vendor path portion of the device path.

    End - Stores the end device path node.

--*/

typedef struct _EFI_BCM2709_VIDEO_DEVICE_PATH {
    VENDOR_DEVICE_PATH VendorPath;
    EFI_DEVICE_PATH_PROTOCOL End;
} EFI_BCM2709_VIDEO_DEVICE_PATH, *PEFI_BCM2709_VIDEO_DEVICE_PATH;

/*++

Structure Description:

    This structure stores the internal context for an BCM2709 video device.

Members:

    Magic - Stores the constant magic value EFI_BCM2709_VIDEO_DEVICE_MAGIC.

    Handle - Stores the graphics out handle.

    GraphicsOut - Stores the graphics output protocol.

    GraphicsOutMode - Stores the graphics output protocol mode.

--*/

typedef struct _EFI_BCM2709_VIDEO_DEVICE {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOut;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GraphicsOutMode;
} EFI_BCM2709_VIDEO_DEVICE, *PEFI_BCM2709_VIDEO_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipBcm2709GraphicsQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

EFIAPI
EFI_STATUS
EfipBcm2709GraphicsSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    );

EFIAPI
EFI_STATUS
EfipBcm2709GraphicsBlt (
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
EfipBcm2709VideoInitialize (
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Mode,
    EFI_PHYSICAL_ADDRESS *FrameBufferBase,
    UINTN *FrameBufferSize
    );

VOID
EfipBcm2709VideoInitializeModes (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define a template for the call to get the video mode.
//

EFI_BCM2709_VIDEO_GET_MODE EfiBcm2709GetModeTemplate = {
    {
        sizeof(EFI_BCM2709_VIDEO_GET_MODE),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_PHYSICAL_RESOLUTION,
            sizeof(BCM2709_RESOLUTION),
            0
        },

        {
            0,
            0
        }
    },

    0
};

//
// Define a template for the call to initialize the video core and get a frame
// buffer.
//

EFI_BCM2709_VIDEO_INITIALIZE EfiBcm2709InitializeVideoTemplate = {
    {
        sizeof(EFI_BCM2709_VIDEO_INITIALIZE),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_PHYSICAL_RESOLUTION,
            sizeof(BCM2709_RESOLUTION),
            sizeof(BCM2709_RESOLUTION)
        },

        {
            0,
            0
        }
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_VIRTUAL_RESOLUTION,
            sizeof(BCM2709_RESOLUTION),
            sizeof(BCM2709_RESOLUTION)
        },

        {
            0,
            0
        }
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_BITS_PER_PIXEL,
            sizeof(UINT32),
            sizeof(UINT32)
        },

        BCM2709_DEFAULT_BITS_PER_PIXEL
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_PIXEL_ORDER,
            sizeof(UINT32),
            sizeof(UINT32)
        },

        BCM2709_MAILBOX_PIXEL_ORDER_BGR
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_ALPHA_MODE,
            sizeof(UINT32),
            sizeof(UINT32)
        },

        BCM2709_MAILBOX_ALPHA_MODE_IGNORED
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_VIRTUAL_OFFSET,
            sizeof(BCM2709_OFFSET),
            sizeof(BCM2709_OFFSET)
        },

        {
            0,
            0
        }
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_OVERSCAN,
            sizeof(BCM2709_OVERSCAN),
            sizeof(BCM2709_OVERSCAN)
        },

        {
            0,
            0,
            0,
            0
        }
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_PITCH,
            sizeof(UINT32),
            0,
        },

        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_FRAME_BUFFER,
            sizeof(BCM2709_FRAME_BUFFER),
            0
        },

        {
            0,
            0
        }
    },

    0
};

//
// Store the device path of the video controller.
//

EFI_BCM2709_VIDEO_DEVICE_PATH EfiBcm2709VideoDevicePathTemplate = {
    {
        {
            HARDWARE_DEVICE_PATH,
            HW_VENDOR_DP,
            sizeof(VENDOR_DEVICE_PATH)
        },

        EFI_BCM2709_VIDEO_DEVICE_GUID,
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

EFI_GRAPHICS_OUTPUT_MODE_INFORMATION EfiBcm2709VideoModes[] = {
    {
        0,
        1024,
        600,
        PixelBitMask,
        {
            BCM2709_BGR_RED_MASK,
            BCM2709_BGR_GREEN_MASK,
            BCM2709_BGR_BLUE_MASK,
            BCM2709_BGR_RESERVED_MASK
        },

        1024
    },

    {
        0,
        1024,
        768,
        PixelBitMask,
        {
            BCM2709_BGR_RED_MASK,
            BCM2709_BGR_GREEN_MASK,
            BCM2709_BGR_BLUE_MASK,
            BCM2709_BGR_RESERVED_MASK
        },

        1024
    },

    {
        0,
        0,
        0,
        PixelBitMask,
        {
            BCM2709_BGR_RED_MASK,
            BCM2709_BGR_GREEN_MASK,
            BCM2709_BGR_BLUE_MASK,
            BCM2709_BGR_RESERVED_MASK
        },

        0
    },
};

UINT32 EfiBcm2709VideoModeCount = 2;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709EnumerateVideo (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the display on BCM2709 SoCs.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    PEFI_BCM2709_VIDEO_DEVICE Device;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
    UINT32 Index;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Mode;
    UINT32 ModeIndex;
    EFI_STATUS Status;

    Device = NULL;

    //
    // If the BCM2709 device library is not initialized, fail.
    //

    if (EfiBcm2709Initialized == FALSE) {
        return EFI_NOT_READY;
    }

    //
    // Initialize the set of available video modes.
    //

    EfipBcm2709VideoInitializeModes();

    //
    // Iterate over the list of available modes backwards until a suitable mode
    // is found.
    //

    Status = EFI_UNSUPPORTED;
    for (Index = EfiBcm2709VideoModeCount; Index != 0; Index -= 1) {
        ModeIndex = Index - 1;
        Mode = &(EfiBcm2709VideoModes[ModeIndex]);
        Status = EfipBcm2709VideoInitialize(Mode,
                                            &FrameBufferBase,
                                            &FrameBufferSize);

        if (!EFI_ERROR(Status)) {
            break;
        }

        //
        // That mode didn't work, so don't advertise it.
        //

        EfiBcm2709VideoModeCount -= 1;
    }

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

    //
    // Everything's all set up, create the graphics output protocol.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_BCM2709_VIDEO_DEVICE),
                             (VOID **)&Device);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

    EfiSetMem(Device, sizeof(EFI_BCM2709_VIDEO_DEVICE), 0);
    Device->Magic = EFI_BCM2709_VIDEO_DEVICE_MAGIC;
    Device->GraphicsOut.QueryMode = EfipBcm2709GraphicsQueryMode;
    Device->GraphicsOut.SetMode = EfipBcm2709GraphicsSetMode;
    Device->GraphicsOut.Blt = EfipBcm2709GraphicsBlt;
    Device->GraphicsOut.Mode = &(Device->GraphicsOutMode);
    Device->GraphicsOutMode.MaxMode = EfiBcm2709VideoModeCount;
    Device->GraphicsOutMode.Mode = ModeIndex;
    Device->GraphicsOutMode.Info = Mode;
    Device->GraphicsOutMode.SizeOfInfo =
                                  sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    Device->GraphicsOutMode.FrameBufferBase = FrameBufferBase;
    Device->GraphicsOutMode.FrameBufferSize = FrameBufferSize;
    Status = EfiInstallMultipleProtocolInterfaces(
                                            &(Device->Handle),
                                            &EfiGraphicsOutputProtocolGuid,
                                            &(Device->GraphicsOut),
                                            &EfiDevicePathProtocolGuid,
                                            &EfiBcm2709VideoDevicePathTemplate,
                                            NULL);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

EnumerateVideoEnd:
    if (EFI_ERROR(Status)) {
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
EfipBcm2709GraphicsQueryMode (
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

    if ((ModeNumber >= EfiBcm2709VideoModeCount) || (SizeOfInfo == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                             (VOID **)&Information);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Information,
               &(EfiBcm2709VideoModes[ModeNumber]),
               sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

    *Info = Information;
    *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipBcm2709GraphicsSetMode (
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

    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Mode;
    EFI_STATUS Status;

    if (ModeNumber >= EfiBcm2709VideoModeCount) {
        return EFI_UNSUPPORTED;
    }

    Mode = &(EfiBcm2709VideoModes[ModeNumber]);
    Status = EfipBcm2709VideoInitialize(Mode,
                                        &FrameBufferBase,
                                        &FrameBufferSize);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    This->Mode->Info = Mode;
    This->Mode->Mode = ModeNumber;
    This->Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    This->Mode->FrameBufferBase = FrameBufferBase;
    This->Mode->FrameBufferSize = FrameBufferSize;
    return Status;
}

EFIAPI
EFI_STATUS
EfipBcm2709GraphicsBlt (
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
EfipBcm2709VideoInitialize (
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Mode,
    EFI_PHYSICAL_ADDRESS *FrameBufferBase,
    UINTN *FrameBufferSize
    )

/*++

Routine Description:

    This routine initializes video by setting the controller to the given video
    mode.

Arguments:

    Mode - Supplies a pointer to the desired video mode.

    FrameBufferBase - Supplies a pointer that receives the base address of the
        frame buffer.

    FrameBufferSize - Supplies a pointer that receives the size of the frame
        buffer in bytes.

Return Value:

    Status code.

--*/

{

    UINT32 ExpectedLength;
    UINT32 Height;
    EFI_BCM2709_VIDEO_INITIALIZE InitializeVideo;
    UINT32 Length;
    UINT32 PixelOrder;
    UINT32 PixelsPerScanLine;
    EFI_STATUS Status;
    UINT32 Width;

    //
    // Update the video initialization template with the given mode information.
    //

    EfiCopyMem(&InitializeVideo,
               &EfiBcm2709InitializeVideoTemplate,
               sizeof(EFI_BCM2709_VIDEO_INITIALIZE));

    Width = Mode->HorizontalResolution;
    Height = Mode->VerticalResolution;
    InitializeVideo.PhysicalResolution.Resolution.Width = Width;
    InitializeVideo.PhysicalResolution.Resolution.Height = Height;
    InitializeVideo.VirtualResolution.Resolution.Width = Width;
    InitializeVideo.VirtualResolution.Resolution.Height = Height;

    //
    // Determine the pixel order and update the template if necessary.
    //

    if ((Mode->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) ||
        ((Mode->PixelFormat == PixelBitMask) &&
         (Mode->PixelInformation.RedMask == BCM2709_BGR_RED_MASK))) {

        PixelOrder = BCM2709_MAILBOX_PIXEL_ORDER_BGR;

    } else {
        PixelOrder = BCM2709_MAILBOX_PIXEL_ORDER_RGB;
    }

    InitializeVideo.PixelOrderMessage.PixelOrder = PixelOrder;

    //
    // Send the initialization command to the BCM2709 mailbox. This is also a
    // GET operation as the frame buffer will be returned. The set actually
    // triggers a frame buffer allocation and the frame buffer cannot be
    // queried separately.
    //

    Status = EfipBcm2709MailboxSendCommand(
                                      BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                      &InitializeVideo,
                                      sizeof(EFI_BCM2709_VIDEO_INITIALIZE),
                                      FALSE);

    if (EFI_ERROR(Status)) {
        goto Bcm2709VideoInitializeEnd;
    }

    //
    // Check the values that are going to be used.
    //

    Length = InitializeVideo.PhysicalResolution.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_RESOLUTION) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    Length = InitializeVideo.VirtualResolution.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_RESOLUTION) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    Length = InitializeVideo.BitsPerPixelMessage.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_BITS_PER_PIXEL) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    Length = InitializeVideo.PixelOrderMessage.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_PIXEL_ORDER) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    Length = InitializeVideo.Pitch.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_PITCH) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    Length = InitializeVideo.FrameBufferMessage.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_FRAME_BUFFER) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    //
    // Make sure the virtual and physical resolutions match.
    //

    if ((InitializeVideo.PhysicalResolution.Resolution.Width !=
         InitializeVideo.VirtualResolution.Resolution.Width) ||
        (InitializeVideo.PhysicalResolution.Resolution.Height !=
         InitializeVideo.VirtualResolution.Resolution.Height)) {

        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    //
    // Make sure the resolution matches the requested modes resolution.
    //

    if ((InitializeVideo.PhysicalResolution.Resolution.Width != Width) ||
        (InitializeVideo.PhysicalResolution.Resolution.Height != Height)) {

        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    //
    // Make sure the result pixel order matches the requested pixel order.
    //

    if (InitializeVideo.PixelOrderMessage.PixelOrder != PixelOrder) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    //
    // Check the pitch. The pixels per scan line better match that of the
    // requested mode.
    //

    PixelsPerScanLine = InitializeVideo.Pitch.BytesPerScanLine /
                        (InitializeVideo.BitsPerPixelMessage.BitsPerPixel / 8);

    if (PixelsPerScanLine != Mode->PixelsPerScanLine) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709VideoInitializeEnd;
    }

    //
    // The video core may return an aliased address out of range for the ARM
    // core. Shift the base address until it is accessible by the ARM core.
    //

    *FrameBufferBase = InitializeVideo.FrameBufferMessage.FrameBuffer.Base;
    *FrameBufferBase &= BCM2709_ARM_PHYSICAL_ADDRESS_MASK;
    *FrameBufferSize = InitializeVideo.FrameBufferMessage.FrameBuffer.Size;
    Status = EFI_SUCCESS;

Bcm2709VideoInitializeEnd:
    return Status;
}

VOID
EfipBcm2709VideoInitializeModes (
    VOID
    )

/*++

Routine Description:

    This routine initializes the video modes by adding a mode with the current
    resolution to the globally defined list if such a mode does not already
    exist.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 ExpectedLength;
    UINT32 Height;
    UINT32 Index;
    UINT32 Length;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Mode;
    EFI_STATUS Status;
    EFI_BCM2709_VIDEO_GET_MODE VideoMode;
    UINT32 Width;

    //
    // Get the current video mode's resolution.
    //

    EfiCopyMem(&VideoMode,
               &EfiBcm2709GetModeTemplate,
               sizeof(EFI_BCM2709_VIDEO_GET_MODE));

    Status = EfipBcm2709MailboxSendCommand(
                                      BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                      &VideoMode,
                                      sizeof(EFI_BCM2709_VIDEO_GET_MODE),
                                      FALSE);

    if (EFI_ERROR(Status)) {
        goto Bcm2709VideoInitializeModesEnd;
    }

    //
    // Validate the returned data.
    //

    Length = VideoMode.PhysicalResolution.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_RESOLUTION) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        goto Bcm2709VideoInitializeModesEnd;
    }

    Width = VideoMode.PhysicalResolution.Resolution.Width;
    Height = VideoMode.PhysicalResolution.Resolution.Height;

    //
    // Check to see if this resolution matches any of the current resolutions.
    //

    for (Index = 0; Index < EfiBcm2709VideoModeCount; Index += 1) {
        Mode = &(EfiBcm2709VideoModes[Index]);
        if ((Mode->HorizontalResolution == Width) &&
            (Mode->VerticalResolution == Height)) {

            EfiBcm2709VideoModeCount = Index + 1;
            goto Bcm2709VideoInitializeModesEnd;
        }
    }

    //
    // If there is no more space in the array, then skip it.
    //

    if (Index >= EFI_BCM2709_VIDEO_MODE_ARRAY_LENGTH) {
        goto Bcm2709VideoInitializeModesEnd;
    }

    //
    // Otherwise add this resolution as the next element in the array. The
    // pixel format is already set.
    //

    Mode = &(EfiBcm2709VideoModes[Index]);
    Mode->HorizontalResolution = Width;
    Mode->VerticalResolution = Height;
    Mode->PixelsPerScanLine = Width;
    EfiBcm2709VideoModeCount += 1;

Bcm2709VideoInitializeModesEnd:
    return;
}

