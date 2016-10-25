/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    video.c

Abstract:

    This module implements support for the RK3288 VOP (Video Output Processor).

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/uefi/protocol/graphout.h>
#include "veyronfw.h"

//
// --------------------------------------------------------------------- Macros
//

#define READ_LCD(_Register) \
    EfiReadRegister32((VOID *)(RK32_LCD_BASE + (_Register)))

#define WRITE_LCD(_Register, _Value) \
    EfiWriteRegister32((VOID *)(RK32_LCD_BASE + (_Register)), (_Value))

#define RK32_CPU_AXI_QOS_PRIORITY_LEVEL(_HValue, _LValue) \
    ((((_HValue) & 3) << 2) | ((_LValue) & 3))

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_RK32_VIDEO_DEVICE_GUID                          \
    {                                                       \
        0x32B82BC3, 0xFAF1, 0x41BB,                         \
        {0xB0, 0xBC, 0xEF, 0xF5, 0x6D, 0xE7, 0x8F, 0x0F}    \
    }

#define EFI_RK32_VIDEO_DEVICE_MAGIC 0x56336B52

//
// Define the default mode to initialize in.
//

#define EFI_RK32_VIDEO_DEFAULT_MODE 0

#define EFI_RK32_VIDEO_MODE_COUNT \
    (sizeof(EfiRk32VideoModes) / sizeof(EfiRk32VideoModes[0]))

//
// Define the size of the frame buffer to allocate, which should be large
// enough to support the largest resolution.
//

#define EFI_RK32_FRAME_BUFFER_SIZE (1366 * 768 * sizeof(UINT16))

//
// Define the LCD controller to talk to.
//

#define RK32_LCD_BASE RK32_VOP_LITTLE_BASE

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the RK32xx graphics output mode information.

Members:

    Information - Stores the information structure.

--*/

typedef struct _EFI_RK32_VIDEO_MODE {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Information;
} EFI_RK32_VIDEO_MODE, *PEFI_RK32_VIDEO_MODE;

/*++

Structure Description:

    This structure stores the structure of an RK32 video device path.

Members:

    VendorPath - Stores the vendor path portion of the device path.

    End - Stores the end device path node.

--*/

typedef struct _EFI_RK32_VIDEO_DEVICE_PATH {
    VENDOR_DEVICE_PATH VendorPath;
    EFI_DEVICE_PATH_PROTOCOL End;
} EFI_RK32_VIDEO_DEVICE_PATH, *PEFI_RK32_VIDEO_DEVICE_PATH;

/*++

Structure Description:

    This structure stores the internal context for an RK32xx video device.

Members:

    Magic - Stores the constant magic value EFI_RK32_VIDEO_DEVICE_MAGIC.

    Handle - Stores the graphics out handle.

    GraphicsOut - Stores the graphics output protocol.

    GraphicsOutMode - Stores the graphics output protocol mode.

--*/

typedef struct _EFI_RK32_VIDEO_DEVICE {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOut;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GraphicsOutMode;
} EFI_RK32_VIDEO_DEVICE, *PEFI_RK32_VIDEO_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipRk32GraphicsQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

EFIAPI
EFI_STATUS
EfipRk32GraphicsSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    );

EFIAPI
EFI_STATUS
EfipRk32GraphicsBlt (
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
EfipRk32VideoInitialize (
    EFI_PHYSICAL_ADDRESS FrameBufferBase,
    UINT32 FrameBufferWidth,
    UINT32 FrameBufferHeight
    );

VOID
EfipRk32LcdMask (
    RK32_LCD_REGISTER Register,
    UINT32 Mask,
    UINT32 Value
    );

VOID
EfipRk32GpioMask (
    VOID *GpioBase,
    RK32_GPIO_REGISTER Register,
    UINT32 Mask,
    UINT32 Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the device path of the video controller.
//

EFI_RK32_VIDEO_DEVICE_PATH EfiRk32VideoDevicePathTemplate = {
    {
        {
            HARDWARE_DEVICE_PATH,
            HW_VENDOR_DP,
            sizeof(VENDOR_DEVICE_PATH)
        },

        EFI_RK32_VIDEO_DEVICE_GUID,
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

EFI_RK32_VIDEO_MODE EfiRk32VideoModes[] = {
    {
        {
            0,
            1366,
            768,
            PixelBitMask,
            {
                0x0000F800,
                0x000007E0,
                0x0000001F,
                0x00000000
            },

            1366
        },
    }
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipVeyronEnumerateVideo (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the display on the Veyron.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    PEFI_RK32_VIDEO_DEVICE Device;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    PEFI_RK32_VIDEO_MODE Mode;
    EFI_STATUS Status;

    FrameBufferBase = -1;
    Device = NULL;
    Mode = &(EfiRk32VideoModes[EFI_RK32_VIDEO_DEFAULT_MODE]);

    //
    // Allocate space for the frame buffer.
    //

    Status = EfiAllocatePages(AllocateAnyPages,
                              EfiMemoryMappedIO,
                              EFI_SIZE_TO_PAGES(EFI_RK32_FRAME_BUFFER_SIZE),
                              &FrameBufferBase);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Initialize the video to the default mode.
    //

    Status = EfipRk32VideoInitialize(FrameBufferBase,
                                     Mode->Information.HorizontalResolution,
                                     Mode->Information.VerticalResolution);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

    //
    // Everything's all set up, create the graphics output protocol.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_RK32_VIDEO_DEVICE),
                             (VOID **)&Device);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

    EfiSetMem(Device, sizeof(EFI_RK32_VIDEO_DEVICE), 0);
    Device->Magic = EFI_RK32_VIDEO_DEVICE_MAGIC;
    Device->GraphicsOut.QueryMode = EfipRk32GraphicsQueryMode;
    Device->GraphicsOut.SetMode = EfipRk32GraphicsSetMode;
    Device->GraphicsOut.Blt = EfipRk32GraphicsBlt;
    Device->GraphicsOut.Mode = &(Device->GraphicsOutMode);
    Device->GraphicsOutMode.MaxMode = EFI_RK32_VIDEO_MODE_COUNT;
    Device->GraphicsOutMode.Mode = EFI_RK32_VIDEO_DEFAULT_MODE;
    Device->GraphicsOutMode.Info = &(Mode->Information);
    Device->GraphicsOutMode.SizeOfInfo =
                                  sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    Device->GraphicsOutMode.FrameBufferBase = FrameBufferBase;
    Device->GraphicsOutMode.FrameBufferSize = EFI_RK32_FRAME_BUFFER_SIZE;
    Status = EfiInstallMultipleProtocolInterfaces(
                                              &(Device->Handle),
                                              &EfiGraphicsOutputProtocolGuid,
                                              &(Device->GraphicsOut),
                                              &EfiDevicePathProtocolGuid,
                                              &EfiRk32VideoDevicePathTemplate,
                                              NULL);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

EnumerateVideoEnd:
    if (EFI_ERROR(Status)) {
        if (FrameBufferBase != -1) {
            EfiFreePages(FrameBufferBase,
                         EFI_SIZE_TO_PAGES(EFI_RK32_FRAME_BUFFER_SIZE));
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
EfipRk32GraphicsQueryMode (
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

    if ((ModeNumber >= EFI_RK32_VIDEO_MODE_COUNT) || (SizeOfInfo == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                             (VOID **)&Information);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Information,
               &(EfiRk32VideoModes[ModeNumber].Information),
               sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

    *Info = Information;
    *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipRk32GraphicsSetMode (
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

    PEFI_RK32_VIDEO_MODE Mode;
    EFI_STATUS Status;

    if (ModeNumber >= EFI_RK32_VIDEO_MODE_COUNT) {
        return EFI_UNSUPPORTED;
    }

    Mode = &(EfiRk32VideoModes[ModeNumber]);
    Status = EfipRk32VideoInitialize(This->Mode->FrameBufferBase,
                                     Mode->Information.HorizontalResolution,
                                     Mode->Information.VerticalResolution);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    This->Mode->Info = &(Mode->Information);
    This->Mode->Mode = ModeNumber;
    This->Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipRk32GraphicsBlt (
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
EfipRk32VideoInitialize (
    EFI_PHYSICAL_ADDRESS FrameBufferBase,
    UINT32 FrameBufferWidth,
    UINT32 FrameBufferHeight
    )

/*++

Routine Description:

    This routine initialize the video subsystem on the RK3288.

Arguments:

    FrameBufferBase - Supplies the physical address where the frame buffer is
        located.

    FrameBufferWidth - Supplies the width of the frame buffer in pixels.

    FrameBufferHeight - Supplies the height of the frame buffer in pixels.

Return Value:

    Status code.

--*/

{

    VOID *Gpio7Base;
    UINT32 Height;
    UINT32 Mask;
    UINT32 Value;
    UINT32 Width;

    //
    // Make sure window 0's display matches the given frame buffer dimensions.
    //

    Value = READ_LCD(Rk32LcdWin0DisplayInformation);
    Width = (Value & RK32_LCD_DSP_INFORMATION_WIDTH_MASK) >>
            RK32_LCD_DSP_INFORMATION_WIDTH_SHIFT;

    Width += 1;
    Height = (Value & RK32_LCD_DSP_INFORMATION_HEIGHT_MASK) >>
             RK32_LCD_DSP_INFORMATION_HEIGHT_SHIFT;

    Height += 1;
    if ((Width != FrameBufferWidth) || (Height != FrameBufferHeight)) {
        return EFI_UNSUPPORTED;
    }

    //
    // Update the window 0 framebuffer.
    //

    WRITE_LCD(Rk32LcdWin0YrgbFrameBufferBase, (UINT32)FrameBufferBase);

    //
    // Take the LCD out of standby and enable EDP out.
    //

    Mask = RK32_LCD_SYSTEM_CONTROL_AUTO_GATING |
           RK32_LCD_SYSTEM_CONTROL_STANDBY |
           RK32_LCD_SYSTEM_CONTROL_EDP_OUT |
           RK32_LCD_SYSTEM_CONTROL_DMA_STOP |
           RK32_LCD_SYSTEM_CONTROL_MMU_ENABLE;

    Value = RK32_LCD_SYSTEM_CONTROL_AUTO_GATING |
            RK32_LCD_SYSTEM_CONTROL_EDP_OUT;

    EfipRk32LcdMask(Rk32LcdSystemControl, Mask, Value);
    WRITE_LCD(Rk32LcdConfigurationDone, 1);

    //
    // Enable the backlight. Set the Port A backligh enable direction bit to
    // output and then set the bit in the data register.
    //

    Gpio7Base = (VOID *)RK32_GPIO7_BASE;
    Value = RK32_GPIO7_BACKLIGHT_ENABLE | RK32_GPIO7_LCD_BACKLIGHT;
    EfipRk32GpioMask(Gpio7Base, Rk32GpioPortADirection, 0, Value);
    Mask = RK32_GPIO7_BACKLIGHT_ENABLE;
    EfipRk32GpioMask(Gpio7Base, Rk32GpioPortAData, Mask, 0);
    Value = RK32_GPIO7_LCD_BACKLIGHT;
    EfipRk32GpioMask(Gpio7Base, Rk32GpioPortAData, 0, Value);
    EfiStall(10000);
    Value = RK32_GPIO7_BACKLIGHT_ENABLE;
    EfipRk32GpioMask(Gpio7Base, Rk32GpioPortAData, 0, Value);
    return EFI_SUCCESS;
}

VOID
EfipRk32LcdMask (
    RK32_LCD_REGISTER Register,
    UINT32 Mask,
    UINT32 Value
    )

/*++

Routine Description:

    This routine masks out the given mask, then ORs in the given value.

Arguments:

    Register - Supplies the register to operate on.

    Mask - Supplies the mask to remove.

    Value - Supplies the value to OR in.

Return Value:

    None.

--*/

{

    UINT32 NewValue;

    NewValue = READ_LCD(Register);
    NewValue &= ~Mask;
    NewValue |= Value;
    WRITE_LCD(Register, NewValue);
    return;
}

VOID
EfipRk32GpioMask (
    VOID *GpioBase,
    RK32_GPIO_REGISTER Register,
    UINT32 Mask,
    UINT32 Value
    )

/*++

Routine Description:

    This routine masks out the given mask, then ORs in the given value.

Arguments:

    GpioBase - Supplies the GPIO base address.

    Register - Supplies the register to operate on.

    Mask - Supplies the mask to remove.

    Value - Supplies the value to OR in.

Return Value:

    None.

--*/

{

    UINT32 NewValue;

    NewValue = EfiReadRegister32(GpioBase + Register);
    NewValue &= ~Mask;
    NewValue |= Value;
    EfiWriteRegister32(GpioBase + Register, NewValue);
    return;
}

