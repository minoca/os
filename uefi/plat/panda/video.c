/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    video.c

Abstract:

    This module implements support for the Texas Instruments OMAP4
    DSS/DISPC display controller.

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
#include "pandafw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to a display controller register.
//

#define READ_DISPLAY_REGISTER(_Register) \
    *(volatile UINT32 *)((UINT8 *)EfiOmap4DispcAddress + (_Register))

#define WRITE_DISPLAY_REGISTER(_Register, _Value) \
    *((volatile UINT32 *)((UINT8 *)EfiOmap4DispcAddress + (_Register))) = \
    (_Value)

//
// These macros read from and write to the display subsystem.
//

#define READ_DISPLAY_SUBSYSTEM_REGISTER(_Register) \
    *(volatile UINT32 *)((UINT8 *)EfiOmap4DssAddress + (_Register))

#define WRITE_DISPLAY_SUBSYSTEM_REGISTER(_Register, _Value) \
    *((volatile UINT32 *)((UINT8 *)EfiOmap4DssAddress + (_Register))) = (_Value)

//
// These macros read from and write to the DSS PRM (Power and Reset Manager).
//

#define READ_DSS_PRM_REGISTER(_Register) \
    *(volatile UINT32 *)((UINT8 *)EfiOmap4DssPrmAddress + (_Register))

#define WRITE_DSS_PRM_REGISTER(_Register, _Value) \
    *((volatile UINT32 *)((UINT8 *)EfiOmap4DssPrmAddress + (_Register))) = \
    (_Value)

//
// These macros read from and write to the DSS CM (Clock Manager).
//

#define READ_DSS_CM_REGISTER(_Register) \
    *(volatile UINT32 *)((UINT8 *)EfiOmap4DssCm2Address + (_Register))

#define WRITE_DSS_CM_REGISTER(_Register, _Value) \
    *((volatile UINT32 *)((UINT8 *)EfiOmap4DssCm2Address + (_Register))) = \
    (_Value)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_OMAP4_VIDEO_DEVICE_GUID                         \
    {                                                       \
        0x19EEE1EB, 0x8F2A, 0x4DFA,                         \
        {0xB0, 0xF9, 0xB1, 0x0B, 0xD5, 0xB8, 0x71, 0x04}    \
    }

#define EFI_OMAP4_VIDEO_DEVICE_MAGIC 0x6469564F // 'diVO'

//
// Define the default mode to initialize in.
//

#define EFI_OMAP4_VIDEO_DEFAULT_MODE 1

#define EFI_OMAP4_VIDEO_MODE_COUNT \
    (sizeof(EfiOmap4VideoModes) / sizeof(EfiOmap4VideoModes[0]))

//
// Define the size of the frame buffer to allocate, which should be large
// enough to support the largest resolution.
//

#define EFI_OMAP4_FRAME_BUFFER_SIZE (1024 * 768 * sizeof(UINT32))

//
// Define the default physical address of the DISPC module on OMAP4 chips.
//

#define OMAP4_DISPC_BASE 0x58001000

//
// Define the default physical address of the DSS module on OMAP4 chips.
//

#define OMAP4_DSS_BASE 0x48040000

//
// Define the default physical address of the DSS Power and Reset Manager (PRM).
//

#define OMAP4_DSS_PRM_BASE 0x4A307100

//
// Define the default physical address of the DSS Clock Manager (CM).
//

#define OMAP4_DSS_CM2_BASE 0x4A009100

//
// Define the timing parameters to use.
//

#define OMAP4_DISPLAY_SUBSYSTEM_DIVISOR 1
#define OMAP4_HORIZONTAL_BACK_PORCH 47
#define OMAP4_HORIZONTAL_FRONT_PORCH 15
#define OMAP4_VERTICAL_BACK_PORCH 32
#define OMAP4_VERTICAL_FRONT_PORCH 9
#define OMAP4_HORIZONTAL_SYNC_PULSE_WIDTH 95
#define OMAP4_VERTICAL_SYNC_PULSE_WIDTH 2
#define OMAP4_DISPLAY_SUBSYSTEM_DIVISOR 1

//
// Define the number of 128-bit words to pre-load into the video DMA pipeline.
//

#define OMAP4_VIDEO_PRELOAD_VALUE 0x100

//
// Define DMA buffer attributes.
//

#define OMAP_VIDEO_BUFFER_LOW_THRESHOLD 0x00C0
#define OMAP_VIDEO_BUFFER_HIGH_THRESHOLD 0x00FC
#define OMAP_VIDEO_BUFFER_SIZE 0x00000400

//
// Define register bit definitions for the DSS control register.
//

//
// Set this bit to select the HDMI encoder.
//

#define OMAP_DSS_CONTROL_SELECT_HDMI (1 << 15)

//
// Define register bit definitions for the system configuration register.
//

//
// Set this bit to allow auto idling.
//

#define OMAP_VIDEO_SYSTEM_CONFIGURATION_AUTO_IDLE (1 << 0)

//
// Set this bit to enable the wakeup feature.
//

#define OMAP_VIDEO_SYSTEM_CONFIGURATION_ENABLE_WAKEUP (1 << 2)

//
// Set this bit to tell the controller to never idle out.
//

#define OMAP_VIDEO_SYSTEM_CONFIGURATION_NO_IDLE (1 << 3)

//
// Set these bits to enable smart idle.
//

#define OMAP_VIDEO_SYSTEM_CONFIGURATION_SMART_IDLE (2 << 3)

//
// Set this bit to tell the controller never to assert standby.
//

#define OMAP_VIDEO_SYSTEM_CONIGURATION_NO_STANDBY (1 << 12)

//
// Set this bit to enable smart standby.
//

#define OMAP_VIDEO_SYSTEM_CONFIGURATION_SMART_STANDBY (2 << 12)

//
// Define register bit definitions for the attributes register.
//

//
// Set this bit to enable the given video pipeline.
//

#define OMAP_VIDEO_ATTRIBUTES_ENABLED (1 << 0)

//
// This format sets the frame buffer to be in ARGB32-8888 mode.
//

#define OMAP_VIDEO_ATTRIBUTES_FORMAT_ARGB32_8888 (0xC << 1)

//
// This format sets the frame buffer to be in xRGB24-8888 mode
// (32-bit container).
//

#define OMAP_VIDEO_ATTRIBUTES_FORMAT_XRGB24_8888 (0x8 << 1)

//
// Set this bit to enable a DMA burst size of 8x128 bits.
//

#define OMAP_VIDEO_ATTRIBUTES_BURST_8X128_BITS (0x2 << 6)

//
// Set this bit to make this video pipeline high priority in the DMA arbiter.
//

#define OMAP_VIDEO_ATTRIBUTES_ARBITRATION (1 << 14)

//
// Set this bit to select the TV out over the LCD0, LCD1, or WB pipelines.
//

#define OMAP_VIDEO_ATTRIBUTES_TV_OUTPUT (1 << 16)

//
// Set this bit to cause the video pipeline to fetch from the DMA buffer.
//

#define OMAP_VIDEO_ATTRIBUTES_SELF_REFRESH (1 << 24)

//
// Set this bit to send the output to the secondary LCD.
//

#define OMAP_VIDEO_ATTRIBUTES_LCD2_OUTPUT (1 << 30)

//
// Define register bit definitions for the picture size register.
//

//
// Define the shift amounts for picture size fields.
//

#define OMAP_VIDEO_PICTURE_SIZE_X_SHIFT 0
#define OMAP_VIDEO_PICTURE_SIZE_Y_SHIFT 16

//
// Define register bit definitions for the size register.
//

//
// Define the shift amounts for the size fields.
//

#define OMAP_VIDEO_SIZE_X_SHIFT 0
#define OMAP_VIDEO_SIZE_Y_SHIFT 16

//
// Define register bit definitions for the TV size register.
//

//
// Define the shift amounts for the size fields.
//

#define OMAP_VIDEO_TV_SIZE_X_SHIFT 0
#define OMAP_VIDEO_TV_SIZE_Y_SHIFT 16

//
// Define register bit definitions for the LCD size registers.
//

//
// Define the shift amounts for the size fields.
//

#define OMAP_VIDEO_LCD_SIZE_X_SHIFT 0
#define OMAP_VIDEO_LCD_SIZE_Y_SHIFT 16

//
// Define the buffer threshold register.
//

#define OMAP_VIDEO_BUFFER_THRESHOLD_HIGH_SHIFT 16

//
// Define register bit definitions for the control 1 register.
//

//
// Set this bit to enable TV output.
//

#define OMAP_VIDEO_CONTROL1_TV_ENABLED (1 << 1)

//
// Set this bit for Active matrix (TFT) displays.
//

#define OMAP_VIDEO_CONTROL1_ACTIVE_TFT (1 << 3)

//
// Set this bit to snap all configured shadow registers into service at the
// next VSYNC.
//

#define OMAP_VIDEO_CONTROL1_GO_TV (1 << 6)

//
// Set this bit to set the first GPIO line to high.
//

#define OMAP_VIDEO_CONTROL1_GPIO0_SET (1 << 15)

//
// Set this bit to set the second GPIO line to high.
//

#define OMAP_VIDEO_CONTROL1_GPIO1_SET (1 << 16)

//
// Define register bit definitions for the configuration 1 register.
//

#define OMAP_VIDEO_CONFIGURATION1_LOAD_ONLY_FRAME_DATA (2 << 1)

//
// Define register bit definitions for the control 2 register.
//

//
// Set this bit to enable LCD2.
//

#define OMAP_VIDEO_CONTROL2_LCD2_ENABLED (1 << 0)

//
// Set this bit for Active matrix (TFT) displays.
//

#define OMAP_VIDEO_CONTROL2_ACTIVE_TFT (1 << 3)

//
// Set this bit to pull the configured pipeline values into service.
//

#define OMAP_VIDEO_CONTROL2_GO_LCD2 (1 << 5)

//
// Set these bits to output 24-bits of data aligned on the LSB of the pixel
// data interface.
//

#define OMAP_VIDEO_CONTROL2_24_BIT_TFT_DATA (3 << 8)

//
// Define the register definitions for the horizontal timing registers.
//

//
// Define the shifts for the horizontal back porch and horizontal front porch.
//

#define OMAP_VIDEO_TIMING_HORIZONTAL_BACK_PORCH_SHIFT 20
#define OMAP_VIDEO_TIMING_HORIZONTAL_FRONT_PORCH_SHIFT 8

//
// Define the register definitions for the vertical timing registers.
//

//
// Define the shifts for the vertical back porch, and veritical front porch.
//

#define OMAP_VIDEO_TIMING_VERTICAL_BACK_PORCH_SHIFT 20
#define OMAP_VIDEO_TIMING_VERTICAL_FRONT_PORCH_SHIFT 8

//
// Define the register definitions for the divisor registers.
//

//
// Define the shifts for the display subsystem divisor.
//

#define OMAP_VIDEO_DIVISOR_DISPLAY_SUBSYSTEM_DIVISOR_SHIFT 16

//
// Define register definitions for the DSS PRM Power state control register.
//

//
// Set these bits to enable power to the DSS subsystem.
//

#define OMAP_DSS_PRM_POWER_CONTROL_POWER_ON (0x3 << 0)

//
// Define register definitions for the DSS CM Clock state control register.
//

//
// Set this bit to force a software wakeup of the DSS clock domain.
//

#define OMAP_DSS_CM_CLOCK_STATE_CONTROL_SOFTWARE_WAKEUP (0x2 << 0)

//
// Define register definitions for the DSS CM Clock control register.
//

//
// These bits define the idle state mask.
//

#define OMAP_DSS_CM_CLOCK_CONTROL_IDLE_STATE_MASK (0x3 << 16)

//
// This bit is set if the module is in standby.
//

#define OMAP_DSS_CM_CLOCK_CONTROL_STANDBY (1 << 18)

//
// Set these bits to enable various optional clocks.
//

#define OMAP_DSS_CM_CLOCK_CONTROL_TV_CLOCK_ENABLED (1 << 11)
#define OMAP_DSS_CM_CLOCK_CONTROL_SYSTEM_CLOCK_ENABLED (1 << 10)
#define OMAP_DSS_CM_CLOCK_CONTROL_48MHZ_CLOCK_ENABLED (1 << 9)
#define OMAP_DSS_CM_CLOCK_CONTROL_DSS_CLOCK_ENABLED (1 << 8)

//
// Set this bit to explicitly enable the functional clock.
//

#define OMAP_DSS_CM_CLOCK_CONTROL_ENABLE (0x2 << 0)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define register offsets in the DISPC Display controller. All offsets are in
// bytes.
//

typedef enum _OMAP_DISPLAY_CONTROLLER_REGISTER {
    OmapDisplaySystemConfiguration       = 0x010, // DISPC_SYSCONFIG
    OmapDisplayInterruptStatus           = 0x018, // DISPC_IRQSTATUS
    OmapDisplayControl1                  = 0x040, // DISPC_CONTROL1
    OmapDisplayConfiguration1            = 0x044, // DISPC_CONFIG1
    OmapDisplayDefaultColor0             = 0x04C, // DISPC_DEFAULT_COLOR0
    OmapDisplayDefaultColor1             = 0x050, // DISPC_DEFAULT_COLOR1
    OmapDisplayDivisor1                  = 0x070, // DISPC_DIVISOR1
    OmapDisplayGlobalAlpha               = 0x074, // DISPC_GLOBAL_ALPHA
    OmapDisplayTvSize                    = 0x078, // DISPC_SIZE_TV
    OmapDisplayGraphicsFrameBufferAddress0 = 0x080, // DISPC_GFX_BA_0
    OmapDisplayGraphicsFrameBufferAddress1 = 0x084, // DISPC_GFX_BA_1
    OmapDisplayGraphicsPosition          = 0x088, // DISPC_GFX_POSITION
    OmapDisplayGraphicsSize              = 0x08C, // DISPC_GFX_SIZE
    OmapDisplayGraphicsAttributes        = 0x0A0, // DISPC_GFX_ATTRIBUTES
    OmapDisplayGraphicsBufferThreshold   = 0x0A4, // DISPC_GFX_BUF_THRESHOLD
    OmapDisplayGraphicsBufferSize        = 0x0A8, // DISPC_GFX_BUF_SIZE_STATUS
    OmapDisplayGraphicsRowIncrement      = 0x0AC, // DISPC_GFX_ROW_INC
    OmapDisplayGraphicsPixelIncrement    = 0x0B0, // DISPC_GFX_PIXEL_INC
    OmapDisplayGraphicsWindowSkip        = 0x0B4, // DISPC_GFX_WINDOW_SKIP
    OmapDisplayVideo1FrameBufferAddress0 = 0x0BC, // DISPC_VID1_BA_0
    OmapDisplayVideo1FrameBufferAddress1 = 0x0C0, // DISPC_VID1_BA_1
    OmapDisplayVideo1Position            = 0x0C4, // DISPC_VID1_POSITION
    OmapDisplayVideo1Size                = 0x0C8, // DISPC_VID1_SIZE
    OmapDisplayVideo1Attributes          = 0x0CC, // DISPC_VID1_ATTRIBUTES
    OmapDisplayVideo1PictureSize         = 0x0E4, // DISPC_VID1_PICTURE_SIZE
    OmapDisplayGraphicsDmaPreload        = 0x22C, // DISPC_GFX_PRELOAD
    OmapDisplayVideo1DmaPreload          = 0x230, // DISPC_VID1_PRELOAD
    OmapDisplayControl2                  = 0x238, // DISPC_CONTROL2
    OmapDisplayDefaultColor2             = 0x3AC, // DISPC_DEFAULT_COLOR2
    OmapDisplayData2Cycle1               = 0x3C0, // DISPC_DATA2_CYCLE1
    OmapDisplayLcd2Size                  = 0x3CC, // DISPC_SIZE_LCD2
    OmapDisplayHorizontalTiming2         = 0x400, // DISPC_TIMING_H2
    OmapDisplayVerticalTiming2           = 0x404, // DISPC_TIMING_V2
    OmapDisplayPolarity2                 = 0x408, // DISPC_POL_FREQ2
    OmapDisplayDivisor2                  = 0x40C, // DISPC_DIVISOR2
    OmapDisplayConfiguration2            = 0x620, // DISPC_CONFIG2
    OmapDisplayVideo1Attributes2         = 0x624, // DISPC_VID1_ATTRIBUTES2
} OMAP_DISPLAY_CONTROLLER_REGISTER, *POMAP_DISPLAY_CONTROLLER_REGISTER;

//
// Define register offsets in the DSS (Display Subsystem) module. All offsets
// are in bytes.
//

typedef enum _OMAP_DISPLAY_SUBSYSTEM_REGISTER {
    OmapDisplaySubsystemControl          = 0x040, // DSS_CTRL
} OMAP_DISPLAY_SUBSYSTEM_REGISTER, *POMAP_DISPLAY_SUBSYSTEM_REGISTER;

//
// Define register offsets for the DSS PRM. All offsets are in bytes.
//

typedef enum _DSS_PRM_REGISTER {
    OmapDssPrmPowerStateControl          = 0x0
} DSS_PRM_REGISTER, *PDSS_PRM_REGISTER;

//
// Define register offsets for the DSS CM. All offsets are in bytes.
//

typedef enum _DSS_CM2_REGISTER {
    OmapDssCmClockStateControl           = 0x00, // CM_DSS_CLKSTCTRL
    OmapDssCmClockControl                = 0x20, // CM_DSS_DSS_CLKCTRL
} DSS_CM2_REGISTER, *PDSS_CM2_REGISTER;

/*++

Structure Description:

    This structure stores the OMAP4 graphics output mode information.

Members:

    Information - Stores the information structure.

    PixelClockDivisor - Stores the pixel clock divisor to set for that mode.

--*/

typedef struct _EFI_OMAP4_VIDEO_MODE {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Information;
    UINT32 PixelClockDivisor;
} EFI_OMAP4_VIDEO_MODE, *PEFI_OMAP4_VIDEO_MODE;

/*++

Structure Description:

    This structure stores the structure of an OMAP4 video device path.

Members:

    VendorPath - Stores the vendor path portion of the device path.

    End - Stores the end device path node.

--*/

typedef struct _EFI_OMAP4_VIDEO_DEVICE_PATH {
    VENDOR_DEVICE_PATH VendorPath;
    EFI_DEVICE_PATH_PROTOCOL End;
} EFI_OMAP4_VIDEO_DEVICE_PATH, *PEFI_OMAP4_VIDEO_DEVICE_PATH;

/*++

Structure Description:

    This structure stores the internal context for an OMAP4 video device.

Members:

    Magic - Stores the constant magic value EFI_OMAP4_VIDEO_DEVICE_MAGIC.

    Handle - Stores the graphics out handle.

    GraphicsOut - Stores the graphics output protocol.

    GraphicsOutMode - Stores the graphics output protocol mode.

--*/

typedef struct _EFI_OMAP4_VIDEO_DEVICE {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOut;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GraphicsOutMode;
} EFI_OMAP4_VIDEO_DEVICE, *PEFI_OMAP4_VIDEO_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipOmap4GraphicsQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

EFIAPI
EFI_STATUS
EfipOmap4GraphicsSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    );

EFIAPI
EFI_STATUS
EfipOmap4GraphicsBlt (
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

VOID
EfipOmap4VideoInitialize (
    EFI_PHYSICAL_ADDRESS FrameBufferBase,
    UINT32 FrameBufferWidth,
    UINT32 FrameBufferHeight,
    UINT32 PixelClockDivisor
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the DISPC module.
//

VOID *EfiOmap4DispcAddress = (VOID *)OMAP4_DISPC_BASE;

//
// Store a pointer to the more global DSS (Display Subsystem) module.
//

VOID *EfiOmap4DssAddress = (VOID *)OMAP4_DSS_BASE;

//
// Store a pointer to the DSS Power and Reset Manager (PRM).
//

VOID *EfiOmap4DssPrmAddress = (VOID *)OMAP4_DSS_PRM_BASE;

//
// Store a pointer to the DSS Clock Manager (CM).
//

VOID *EfiOmap4DssCm2Address = (VOID *)OMAP4_DSS_CM2_BASE;

//
// Store the device path of the video controller.
//

EFI_OMAP4_VIDEO_DEVICE_PATH EfiOmap4VideoDevicePathTemplate = {
    {
        {
            HARDWARE_DEVICE_PATH,
            HW_VENDOR_DP,
            sizeof(VENDOR_DEVICE_PATH)
        },

        EFI_OMAP4_VIDEO_DEVICE_GUID,
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

EFI_OMAP4_VIDEO_MODE EfiOmap4VideoModes[] = {
    {
        {
            0,
            1024,
            600,
            PixelBitMask,
            {
                0x00FF0000,
                0x0000FF00,
                0x000000FF,
                0xFF000000
            },

            1024
        },

        18
    },

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

        13
    },
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipPandaEnumerateVideo (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the display on the PandaBoard.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    PEFI_OMAP4_VIDEO_DEVICE Device;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    PEFI_OMAP4_VIDEO_MODE Mode;
    EFI_STATUS Status;

    FrameBufferBase = -1;
    Device = NULL;
    Mode = &(EfiOmap4VideoModes[EFI_OMAP4_VIDEO_DEFAULT_MODE]);

    //
    // Allocate space for the frame buffer.
    //

    Status = EfiAllocatePages(AllocateAnyPages,
                              EfiMemoryMappedIO,
                              EFI_SIZE_TO_PAGES(EFI_OMAP4_FRAME_BUFFER_SIZE),
                              &FrameBufferBase);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Initialize the video to the default mode.
    //

    EfipOmap4VideoInitialize(FrameBufferBase,
                             Mode->Information.HorizontalResolution,
                             Mode->Information.VerticalResolution,
                             Mode->PixelClockDivisor);

    //
    // Everything's all set up, create the graphics output protocol.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_OMAP4_VIDEO_DEVICE),
                             (VOID **)&Device);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

    EfiSetMem(Device, sizeof(EFI_OMAP4_VIDEO_DEVICE), 0);
    Device->Magic = EFI_OMAP4_VIDEO_DEVICE_MAGIC;
    Device->GraphicsOut.QueryMode = EfipOmap4GraphicsQueryMode;
    Device->GraphicsOut.SetMode = EfipOmap4GraphicsSetMode;
    Device->GraphicsOut.Blt = EfipOmap4GraphicsBlt;
    Device->GraphicsOut.Mode = &(Device->GraphicsOutMode);
    Device->GraphicsOutMode.MaxMode = EFI_OMAP4_VIDEO_MODE_COUNT;
    Device->GraphicsOutMode.Mode = EFI_OMAP4_VIDEO_DEFAULT_MODE;
    Device->GraphicsOutMode.Info = &(Mode->Information);
    Device->GraphicsOutMode.SizeOfInfo =
                                  sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    Device->GraphicsOutMode.FrameBufferBase = FrameBufferBase;
    Device->GraphicsOutMode.FrameBufferSize = EFI_OMAP4_FRAME_BUFFER_SIZE;
    Status = EfiInstallMultipleProtocolInterfaces(
                                              &(Device->Handle),
                                              &EfiGraphicsOutputProtocolGuid,
                                              &(Device->GraphicsOut),
                                              &EfiDevicePathProtocolGuid,
                                              &EfiOmap4VideoDevicePathTemplate,
                                              NULL);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

EnumerateVideoEnd:
    if (EFI_ERROR(Status)) {
        if (FrameBufferBase != -1) {
            EfiFreePages(FrameBufferBase,
                         EFI_SIZE_TO_PAGES(EFI_OMAP4_FRAME_BUFFER_SIZE));
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
EfipOmap4GraphicsQueryMode (
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

    if ((ModeNumber >= EFI_OMAP4_VIDEO_MODE_COUNT) || (SizeOfInfo == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                             (VOID **)&Information);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Information,
               &(EfiOmap4VideoModes[ModeNumber].Information),
               sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

    *Info = Information;
    *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipOmap4GraphicsSetMode (
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

    PEFI_OMAP4_VIDEO_MODE Mode;
    EFI_STATUS Status;

    if (ModeNumber >= EFI_OMAP4_VIDEO_MODE_COUNT) {
        return EFI_UNSUPPORTED;
    }

    Mode = &(EfiOmap4VideoModes[ModeNumber]);
    EfipOmap4VideoInitialize(This->Mode->FrameBufferBase,
                             Mode->Information.HorizontalResolution,
                             Mode->Information.VerticalResolution,
                             Mode->PixelClockDivisor);

    This->Mode->Info = &(Mode->Information);
    This->Mode->Mode = ModeNumber;
    This->Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return Status;
}

EFIAPI
EFI_STATUS
EfipOmap4GraphicsBlt (
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

VOID
EfipOmap4VideoInitialize (
    EFI_PHYSICAL_ADDRESS FrameBufferBase,
    UINT32 FrameBufferWidth,
    UINT32 FrameBufferHeight,
    UINT32 PixelClockDivisor
    )

/*++

Routine Description:

    This routine initialize the video subsystem on the TI OMAP4.

Arguments:

    FrameBufferBase - Supplies the physical address where the frame buffer is
        located.

    FrameBufferWidth - Supplies the width of the frame buffer in pixels.

    FrameBufferHeight - Supplies the height of the frame buffer in pixels.

    PixelClockDivisor - Supplies the pixel clock divisor to set.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Set GPIO0 to HI to enable the TFP410PAP. For the output enable register,
    // when a bit is 0, then the GPIO is in output mode.
    //

    Value = READ_GPIO1_REGISTER(OmapGpioOutputEnable);
    WRITE_GPIO1_REGISTER(OmapGpioOutputEnable, Value & ~(1 << 0));
    WRITE_GPIO1_REGISTER(OmapGpioOutputSet, (1 << 0));

    //
    // Enable clocks and power for the Display Subsystem.
    //

    WRITE_DSS_PRM_REGISTER(OmapDssPrmPowerStateControl,
                           OMAP_DSS_PRM_POWER_CONTROL_POWER_ON);

    WRITE_DSS_CM_REGISTER(OmapDssCmClockStateControl,
                          OMAP_DSS_CM_CLOCK_STATE_CONTROL_SOFTWARE_WAKEUP);

    Value = OMAP_DSS_CM_CLOCK_CONTROL_DSS_CLOCK_ENABLED |
            OMAP_DSS_CM_CLOCK_CONTROL_ENABLE;

    WRITE_DSS_CM_REGISTER(OmapDssCmClockControl, Value);

    //
    // Wait for the module to exit an idle state before attempting to access it.
    //

    while (TRUE) {
        if ((READ_DSS_CM_REGISTER(OmapDssCmClockControl) &
            OMAP_DSS_CM_CLOCK_CONTROL_IDLE_STATE_MASK) == 0) {

            break;
        }
    }

    //
    // Reset DSS control to its default value.
    //

    WRITE_DISPLAY_SUBSYSTEM_REGISTER(OmapDisplaySubsystemControl, 0);

    //
    // Set up smart auto-idling.
    //

    Value = OMAP_VIDEO_SYSTEM_CONFIGURATION_SMART_STANDBY |
            OMAP_VIDEO_SYSTEM_CONFIGURATION_SMART_IDLE |
            OMAP_VIDEO_SYSTEM_CONFIGURATION_ENABLE_WAKEUP |
            OMAP_VIDEO_SYSTEM_CONFIGURATION_AUTO_IDLE;

    WRITE_DISPLAY_REGISTER(OmapDisplaySystemConfiguration, Value);

    //
    // Set up the configuration register to only load frame data (and not
    // palette/gamma tables) every frame.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayConfiguration1,
                           OMAP_VIDEO_CONFIGURATION1_LOAD_ONLY_FRAME_DATA);

    //
    // Set up the divisor.
    //

    Value = (OMAP4_DISPLAY_SUBSYSTEM_DIVISOR <<
             OMAP_VIDEO_DIVISOR_DISPLAY_SUBSYSTEM_DIVISOR_SHIFT) |
            PixelClockDivisor;

    WRITE_DISPLAY_REGISTER(OmapDisplayDivisor2, Value);

    //
    // Disable the global alpha channel on all video pipelines.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayGlobalAlpha, 0xFFFFFFFF);

    //
    // Set the address of the frame buffer.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsFrameBufferAddress0,
                           (UINT32)FrameBufferBase);

    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsFrameBufferAddress1,
                           (UINT32)FrameBufferBase);

    //
    // Set the position of this frame buffer in the overlay manager. This is
    // the only frame buffer, so set it to the top left.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsPosition, 0);

    //
    // Set up the dimensions of the frame buffer itself.
    //

    Value = ((FrameBufferWidth - 1) << OMAP_VIDEO_SIZE_X_SHIFT) |
            ((FrameBufferHeight - 1) << OMAP_VIDEO_SIZE_Y_SHIFT);

    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsSize, Value);

    //
    // Set up the attributes register, which sets up the pixel format, enables
    // the pipeline, and sets LCD2 as the destination.
    //

    Value = OMAP_VIDEO_ATTRIBUTES_LCD2_OUTPUT |
            OMAP_VIDEO_ATTRIBUTES_BURST_8X128_BITS |
            OMAP_VIDEO_ATTRIBUTES_FORMAT_XRGB24_8888 |
            OMAP_VIDEO_ATTRIBUTES_ENABLED;

    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsAttributes, Value);
    Value = (OMAP_VIDEO_BUFFER_HIGH_THRESHOLD <<
             OMAP_VIDEO_BUFFER_THRESHOLD_HIGH_SHIFT) |
            OMAP_VIDEO_BUFFER_LOW_THRESHOLD;

    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsBufferThreshold, Value);
    Value = OMAP_VIDEO_BUFFER_SIZE;
    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsBufferSize, Value);
    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsWindowSkip, 0);

    //
    // Set up the row and pixel increments to nothing fancy.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsRowIncrement, 1);
    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsPixelIncrement, 1);

    //
    // Set the preload value to its default.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayGraphicsDmaPreload,
                           OMAP4_VIDEO_PRELOAD_VALUE);

    //
    // Set the default color to red.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayDefaultColor0, 0x00FF0000);
    WRITE_DISPLAY_REGISTER(OmapDisplayDefaultColor1, 0x00FF0000);
    WRITE_DISPLAY_REGISTER(OmapDisplayDefaultColor2, 0x00FF0000);

    //
    // Configure all the pin polarities to their normal values.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayPolarity2, 0);

    //
    // Set up the dimensions to output to LCD2.
    //

    Value = ((FrameBufferWidth - 1) << OMAP_VIDEO_LCD_SIZE_X_SHIFT) |
            ((FrameBufferHeight - 1) << OMAP_VIDEO_LCD_SIZE_Y_SHIFT);

    WRITE_DISPLAY_REGISTER(OmapDisplayLcd2Size, Value);

    //
    // Set up the timing parameters.
    //

    Value = (OMAP4_HORIZONTAL_BACK_PORCH <<
             OMAP_VIDEO_TIMING_HORIZONTAL_BACK_PORCH_SHIFT) |
            (OMAP4_HORIZONTAL_FRONT_PORCH <<
             OMAP_VIDEO_TIMING_HORIZONTAL_FRONT_PORCH_SHIFT) |
            OMAP4_HORIZONTAL_SYNC_PULSE_WIDTH;

    WRITE_DISPLAY_REGISTER(OmapDisplayHorizontalTiming2, Value);
    Value = (OMAP4_VERTICAL_BACK_PORCH <<
             OMAP_VIDEO_TIMING_VERTICAL_BACK_PORCH_SHIFT) |
            (OMAP4_VERTICAL_FRONT_PORCH <<
             OMAP_VIDEO_TIMING_HORIZONTAL_FRONT_PORCH_SHIFT) |
            OMAP4_VERTICAL_SYNC_PULSE_WIDTH;

    WRITE_DISPLAY_REGISTER(OmapDisplayVerticalTiming2, Value);

    //
    // Set up the control 2 register to turn on LCD2.
    //

    Value = OMAP_VIDEO_CONTROL2_24_BIT_TFT_DATA |
            OMAP_VIDEO_CONTROL2_ACTIVE_TFT |
            OMAP_VIDEO_CONTROL2_LCD2_ENABLED;

    WRITE_DISPLAY_REGISTER(OmapDisplayControl2, Value);
    WRITE_DISPLAY_REGISTER(OmapDisplayControl2,
                           Value | OMAP_VIDEO_CONTROL2_GO_LCD2);

    //
    // Wait for the pipeline to suck up the new parameters.
    //

    while (TRUE) {
        if ((READ_DISPLAY_REGISTER(OmapDisplayControl2) &
            OMAP_VIDEO_CONTROL2_GO_LCD2) == 0) {

            break;
        }
    }

    //
    // Clear any pending interrupts.
    //

    WRITE_DISPLAY_REGISTER(OmapDisplayInterruptStatus, 0xFFFFFFFF);
    return;
}

