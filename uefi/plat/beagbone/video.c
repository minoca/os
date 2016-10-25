/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    video.c

Abstract:

    This module fires up a basic frame buffer on the TI BeagleBone Black.

Author:

    Evan Green 22-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/uefi/protocol/graphout.h>
#include "bbonefw.h"
#include <minoca/soc/am335x.h>

//
// --------------------------------------------------------------------- Macros
//

#define AM335_SOC_READ(_Register)                                           \
    EfiReadRegister32((VOID *)(AM335_SOC_CONTROL_REGISTERS + (_Register)))

#define AM335_SOC_WRITE(_Register, _Value)                                  \
    EfiWriteRegister32((VOID *)(AM335_SOC_CONTROL_REGISTERS + (_Register)), \
                       (_Value))

#define AM335_LCD_READ(_Register)                                           \
    EfiReadRegister32((VOID *)(AM335_LCD_REGISTERS + (_Register)))

#define AM335_LCD_WRITE(_Register, _Value)                                  \
    EfiWriteRegister32((VOID *)(AM335_LCD_REGISTERS + (_Register)),         \
                       (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_AM335_VIDEO_DEVICE_GUID                         \
    {                                                       \
        0x19EEE1EB, 0x8F2A, 0x4DFA,                         \
        {0xB0, 0xF9, 0xB1, 0x0B, 0xD5, 0xB8, 0x71, 0x05}    \
    }

#define EFI_AM335_VIDEO_DEVICE_MAGIC 0x64695641 // 'diVA'

//
// Define the default mode to initialize in.
//

#define EFI_AM335_VIDEO_DEFAULT_MODE 1

#define EFI_AM335_VIDEO_MODE_COUNT \
    (sizeof(EfiAm335VideoModes) / sizeof(EfiAm335VideoModes[0]))

//
// Define the frame buffer size, which should be large enough to support the
// biggest resolution.
//

#define EFI_AM335_FRAME_BUFFER_SIZE \
    (AM335_PALETTE_SIZE + (1024 * 768 * sizeof(UINT16)))

#define AM335_LCD_MODULE_CLOCK 192000000

#define AM335_PALETTE_SIZE 0
#define AM335_PALETTE_NONE 0x4000

//
// Define specific video parameters. This is calculated for standard VESA
// 1024x768 60Hz display.
//

#if 1

#define BEAGLE_BONE_BLACK_PIXEL_CLOCK 65000000
#define BEAGLE_BONE_BLACK_RESOLUTION_X 1024
#define BEAGLE_BONE_BLACK_HSYNC 136
#define BEAGLE_BONE_BLACK_HORIZONTAL_FRONT_PORCH 24
#define BEAGLE_BONE_BLACK_HORIZONTAL_BACK_PORCH 160
#define BEAGLE_BONE_BLACK_RESOLUTION_Y 768
#define BEAGLE_BONE_BLACK_VSYNC 6
#define BEAGLE_BONE_BLACK_VERTICAL_FRONT_PORCH 3
#define BEAGLE_BONE_BLACK_VERTICAL_BACK_PORCH 29

#elif 0

//
// Other resolutions were fiddled with, not sure if they actually work though.
//

#define BEAGLE_BONE_BLACK_PIXEL_CLOCK 40000000
#define BEAGLE_BONE_BLACK_RESOLUTION_X 800
#define BEAGLE_BONE_BLACK_HSYNC 128
#define BEAGLE_BONE_BLACK_HORIZONTAL_FRONT_PORCH 40
#define BEAGLE_BONE_BLACK_HORIZONTAL_BACK_PORCH 88
#define BEAGLE_BONE_BLACK_RESOLUTION_Y 600
#define BEAGLE_BONE_BLACK_VSYNC 4
#define BEAGLE_BONE_BLACK_VERTICAL_FRONT_PORCH 1
#define BEAGLE_BONE_BLACK_VERTICAL_BACK_PORCH 23

#else

#define BEAGLE_BONE_BLACK_PIXEL_CLOCK 25175000
#define BEAGLE_BONE_BLACK_RESOLUTION_X 640
#define BEAGLE_BONE_BLACK_HSYNC 96
#define BEAGLE_BONE_BLACK_HORIZONTAL_FRONT_PORCH 18
#define BEAGLE_BONE_BLACK_HORIZONTAL_BACK_PORCH 48
#define BEAGLE_BONE_BLACK_RESOLUTION_Y 480
#define BEAGLE_BONE_BLACK_VSYNC 2
#define BEAGLE_BONE_BLACK_VERTICAL_FRONT_PORCH 10
#define BEAGLE_BONE_BLACK_VERTICAL_BACK_PORCH 33

#endif

//
// TDA19988 definitions.
//

//
// Define software flags for a TDA19988 video mode.
//

#define TDA19988_MODE_FLAG_NEGATE_HSYNC 0x00000001
#define TDA19988_MODE_FLAG_NEGATE_VSYNC 0x00000002
#define TDA19988_MODE_FLAG_INTERLACE 0x00000004
#define TDA19988_MODE_FLAG_HORIZONTAL_SKEW 0x00000008

#define TDA19988_CONTROL_RESET_DDC 0x03

#define EFI_TDA19988_HDMI_BUS_ADDRESS 0x70
#define EFI_TDA19988_CEC_BUS_ADDRESS 0x34

#define TDA19988_CEC_FRO_IM_CLOCK_CONTROL 0xFB
#define TDA19988_CEC_FRO_IM_CLOCK_CONTROL_VALUE 0x82

#define TDA19988_CEC_STATUS 0xFE
#define TDA19988_CEC_STATUS_RX_SENSE 0x01
#define TDA19988_CEC_STATUS_HOT_PLUG_DETECT 0x02

#define TDA19988_CEC_ENABLE 0xFF
#define TDA19988_CEC_ENABLE_RX_SENSE 0x04
#define TDA19988_CEC_ENABLE_HDMI 0x02
#define TDA19988_CEC_ENABLE_ALL 0x87

//
// Define TDA19988 control pages.
//

#define TDA19988_CONTROL_PAGE 0x00
#define TDA19988_PLL_PAGE 0x02
#define TDA19988_EDID_PAGE 0x09
#define TDA19988_INFORMATION_PAGE 0x10
#define TDA19988_AUDIO_PAGE 0x11
#define TDA19988_HDCP_OTP_PAGE 0x12
#define TDA19988_GAMUT_PAGE 0x13

//
// The page select register exists in all pages.
//

#define TDA19988_PAGE_SELECT_REGISTER 0xFF

//
// Define TDA19988 control page registers.
//

#define TDA19988_CONTROL_REVISION_LOW 0x0000
#define TDA19988_CONTROL_MAIN_CONTROL 0x0001
#define TDA19988_CONTROL_REVISION_HIGH 0x0002
#define TDA19988_CONTROL_RESET 0x000A
#define TDA19988_CONTROL_DDC_CONTROL 0x000B
#define TDA19988_CONTROL_DDC_CLOCK 0x000C
#define TDA19988_CONTROL_INTERRUPT_CONTROL 0x000F
#define TDA19988_CONTROL_INTERRUPT 0x0011
#define TDA19988_CONTROL_ENABLE_VIDEO_0_PORT 0x0018
#define TDA19988_CONTROL_ENABLE_VIDEO_1_PORT 0x0019
#define TDA19988_CONTROL_ENABLE_VIDEO_2_PORT 0x001A
#define TDA19988_CONTROL_ENABLE_AUDIO_PORT 0x001E
#define TDA19988_CONTROL_VIP_CONTROL_0 0x0020
#define TDA19988_CONTROL_VIP_CONTROL_1 0x0021
#define TDA19988_CONTROL_VIP_CONTROL_2 0x0022
#define TDA19988_CONTROL_VIP_CONTROL_3 0x0023
#define TDA19988_CONTROL_VIP_CONTROL_4 0x0024
#define TDA19988_CONTROL_VIP_CONTROL_5 0x0025
#define TDA19988_CONTROL_VP_VIP_OUT 0x0027
#define TDA19988_CONTROL_MATRIX_CONTROL 0x0080
#define TDA19988_CONTROL_VIDEOFORMAT 0x00A0
#define TDA19988_CONTROL_REFERENCE_PIXEL_HIGH 0x00A1
#define TDA19988_CONTROL_REFERENCE_PIXEL_LOW 0x00A2
#define TDA19988_CONTROL_REFERENCE_LINE_HIGH 0x00A3
#define TDA19988_CONTROL_REFERENCE_LINE_LOW 0x00A4
#define TDA19988_CONTROL_NPIXELS_HIGH 0x00A5
#define TDA19988_CONTROL_NPIXELS_LOW 0x00A6
#define TDA19988_CONTROL_NLINES_HIGH 0x00A7
#define TDA19988_CONTROL_NLINES_LOW 0x00A8
#define TDA19988_CONTROL_VS_LINE_START_1_HIGH 0x00A9
#define TDA19988_CONTROL_VS_LINE_START_1_LOW 0x00AA
#define TDA19988_CONTROL_VS_PIXEL_START_1_HIGH 0x00AB
#define TDA19988_CONTROL_VS_PIXEL_START_1_LOW 0x00AC
#define TDA19988_CONTROL_VS_LINE_END_1_HIGH 0x00AD
#define TDA19988_CONTROL_VS_LINE_END_1_LOW 0x00AE
#define TDA19988_CONTROL_VS_PIXEL_END_1_HIGH 0x00AF
#define TDA19988_CONTROL_VS_PIXEL_END_1_LOW 0x00B0
#define TDA19988_CONTROL_VS_LINE_START_2_HIGH 0x00B1
#define TDA19988_CONTROL_VS_LINE_START_2_LOW 0x00B2
#define TDA19988_CONTROL_VS_PIXEL_START_2_HIGH 0x00B3
#define TDA19988_CONTROL_VS_PIXEL_START_2_LOW 0x00B4
#define TDA19988_CONTROL_VS_LINE_END_2_HIGH 0x00B5
#define TDA19988_CONTROL_VS_LINE_END_2_LOW 0x00B6
#define TDA19988_CONTROL_VS_PIXEL_END_2_HIGH 0x00B7
#define TDA19988_CONTROL_VS_PIXEL_END_2_LOW 0x00B8
#define TDA19988_CONTROL_HS_PIXEL_START_HIGH 0x00B9
#define TDA19988_CONTROL_HS_PIXEL_START_LOW 0x00BA
#define TDA19988_CONTROL_HS_PIXEL_STOP_HIGH 0x00BB
#define TDA19988_CONTROL_HS_PIXEL_STOP_LOW 0x00BC
#define TDA19988_CONTROL_VWIN_START_1_HIGH 0x00BD
#define TDA19988_CONTROL_VWIN_START_1_LOW 0x00BE
#define TDA19988_CONTROL_VWIN_END_1_HIGH 0x00BF
#define TDA19988_CONTROL_VWIN_END_1_LOW 0x00C0
#define TDA19988_CONTROL_VWIN_START_2_HIGH 0x00C1
#define TDA19988_CONTROL_VWIN_START_2_LOW 0x00C2
#define TDA19988_CONTROL_VWIN_END_2_HIGH 0x00C3
#define TDA19988_CONTROL_VWIN_END_2_LOW 0x00C4
#define TDA19988_CONTROL_DE_START_HIGH 0x00C5
#define TDA19988_CONTROL_DE_START_LOW 0x00C6
#define TDA19988_CONTROL_DE_STOP_HIGH 0x00C7
#define TDA19988_CONTROL_DE_STOP_LOW 0x00C8
#define TDA19988_CONTROL_TBG_CONTROL_0 0x00CA
#define TDA19988_CONTROL_TBG_CONTROL_1 0x00CB
#define TDA19988_CONTROL_VSPACE_START_HIGH 0x00D2
#define TDA19988_CONTROL_VSPACE_START_LOW 0x00D3
#define TDA19988_CONTROL_VSPACE_END_HIGH 0x00D4
#define TDA19988_CONTROL_VSPACE_END_LOW 0x00D5
#define TDA19988_CONTROL_ENABLE_SPACE 0x00D6
#define TDA19988_CONTROL_VSPACE_Y_DATA 0x00D7
#define TDA19988_CONTROL_VSPACE_U_DATA 0x00D8
#define TDA19988_CONTROL_VSPACE_V_DATA 0x00D9
#define TDA19988_CONTROL_HVF_CONTROL_0 0x00E4
#define TDA19988_CONTROL_HVF_CONTROL_1 0x00E5
#define TDA19988_CONTROL_RPT_CONTROL 0x00F0

#define TDA19988_CONTROL_MAIN_CONTROL_SOFT_RESET 0x01

#define TDA19988_CONTROL_DDC_CONTROL_ENABLE 0x00

#define TDA19988_CONTROL_DDC_CLOCK_ENABLE 0x01

#define TDA19988_CONTROL_INTERRUPT_CONTROL_GLOBAL_ENABLE 0x04

#define TDA19988_CONTROL_INTERRUPT_EDID 0x02

#define TDA19988_CONTROL_ENABLE_ALL 0xFF

#define TDA19988_CONTROL_VIP_CONTROL_0_SYNC_METHOD 0x40

#define TDA19988_CONTROL_VIP_CONTROL_3_SYNC_HS (0x2 << 4)
#define TDA19988_CONTROL_VIP_CONTROL_3_EMBEDDED_SYNC 0x08
#define TDA19988_CONTROL_VIP_CONTROL_3_V_TOGGLE 0x04
#define TDA19988_CONTROL_VIP_CONTROL_3_H_TOGGLE 0x02
#define TDA19988_CONTROL_VIP_CONTROL_3_X_TOGGLE 0x01

#define TDA19988_CONTROL_VIP_CONTROL_4_TEST_PATTERN 0x80

#define TDA19988_CONTROL_VP_VIP_OUT_VALUE 0x24

#define TDA19988_CONTROL_MATRIX_CONTROL_BYPASS 0x04

#define TDA19988_CONTROL_TBG_CONTROL_0_SYNC_ONCE 0x80
#define TDA19988_CONTROL_TBG_CONTROL_0_SYNC_METHOD 0x40

#define TDA19988_CONTROL_TBG_CONTROL_1_DISABLE_DWIN 0x40
#define TDA19988_CONTROL_TBG_CONTROL_1_TOGGLE_ENABLE 0x04
#define TDA19988_CONTROL_TBG_CONTROL_1_V_TOGGLE 0x02
#define TDA19988_CONTROL_TBG_CONTROL_1_H_TOGGLE 0x01

#define TDA19988_CONTROL_ENABLE_SPACE_ENABLE 0x01

#define TDA19988_CONTROL_HVF_CONTROL_0_SERVICE_MODE 0x80

#define TDA19988_CONTROL_HVF_CONTROL_1_DEPTH_MASK 0x30
#define TDA19988_CONTROL_HVF_CONTROL_1_DEPTH_COLOR_PC 0x10
#define TDA19988_CONTROL_HVF_CONTROL_1_VQR_FULL (0x0 << 2)

//
// Define PLL register definitions.
//

#define TDA19988_PLL_SERIAL_1 0x0200
#define TDA19988_PLL_SERIAL_2 0x0201
#define TDA19988_PLL_SERIAL_3 0x0202
#define TDA19988_PLL_SERIALIZER 0x0203
#define TDA19988_PLL_BUFFER_OUT 0x0204
#define TDA19988_PLL_SCG1 0x0205
#define TDA19988_PLL_SCG2 0x0206
#define TDA19988_PLL_SCGN1 0x0207
#define TDA19988_PLL_SCGN2 0x0208
#define TDA19988_PLL_SCGR1 0x0209
#define TDA19988_PLL_SCGR2 0x020A
#define TDA19988_PLL_AUDIO_DIVISOR 0x020E
#define TDA19988_PLL_CLOCK_SELECT 0x0211
#define TDA19988_PLL_ANALOG_CONTROL 0x0212

#define TDA19988_PLL_SERIAL_1_SRL_MAN_IP 0x40

#define TDA19988_PLL_SERIAL_2_SRL_NOSC(_Divisor) (((_Divisor) & 0x03) << 0)
#define TDA19988_PLL_SERIAL_2_SRL_PR(_Value) (((_Value) & 0xF) << 4)

#define TDA19988_PLL_SERIAL_3_SRL_CCIR 0x02
#define TDA19988_PLL_SERIAL_3_DE 0x04

#define TDA19988_PLL_BUFFER_OUT_SRL_FORCE_MASK 0x0C
#define TDA19988_PLL_BUFFER_OUT_SRL_FORCE_0 0x08

#define TDA19988_PLL_SCG2_VALUE 0x10

#define TDA19988_PLL_SCGN1_VALUE 0xFA

#define TDA19988_PLL_SCGR1_VALUE 0x5B

#define TDA19988_PLL_AUDIO_DIVISOR_VALUE 0x03

#define TDA19988_PLL_CLOCK_SELECT_VALUE 0x09

#define TDA19988_PLL_ANALOG_TX_VSWING_VALUE 0x09

//
// Define EDID page registers.
//

#define TDA19988_EDID_DATA 0x0900
#define TDA19988_EDID_REQUEST 0x09FA
#define TDA19988_EDID_DEVICE_ADDRESS 0x09FB
#define TDA19988_EDID_OFFSET 0x09FC
#define TDA19988_EDID_SEGMENT_POINTER_ADDRESS 0x09FD
#define TDA19988_EDID_SEGMENT_ADDRESS 0x09FE

#define TDA19988_EDID_REQUEST_READ 0x01

#define TDA19988_EDID_DEVICE_ADDRESS_EDID 0xA0

#define TDA19988_EDID_OFFSET_VALUE 0x00

#define TDA19988_EDID_SEGMENT_POINTER_ADDRESS_VALUE 0x00

#define TDA19988_EDID_SEGMENT_ADDRESS_VALUE 0x00

//
// Define TDA19988 audio control registers.
//

#define TDA19988_AUDIO_AIP_CONTROL 0x1100
#define TDA19988_AUDIO_ENCODE_CONTROL 0x110D
#define TDA19988_AUDIO_IF_FLAGS 0x110F

#define TDA19988_AUDIO_AIP_CONTROL_RESET_FIFO 0x01
#define TDA19988_HDMI_REVISION_VALUE 0x0331

//
// Define TDA19988 HDCP/OTP page registers.
//

#define TDA19988_HDCP_OTP_TX3 0x129A
#define TDA19988_HDCP_OTP_TX4 0x129B
#define TDA19988_HDCP_OTP_TX33 0x12B8

#define TDA19988_HDCP_OTP_TX3_VALUE 0x27
#define TDA19988_HDCP_OTP_TX33_HDMI 0x02
#define TDA19988_HDCP_OTP_TX4_PD_RAM 0x02

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the AM335x graphics output mode information.

Members:

    Information - Stores the information structure.

--*/

typedef struct _EFI_AM335_VIDEO_MODE {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Information;
} EFI_AM335_VIDEO_MODE, *PEFI_AM335_VIDEO_MODE;

/*++

Structure Description:

    This structure stores the structure of an AM335x video device path.

Members:

    VendorPath - Stores the vendor path portion of the device path.

    End - Stores the end device path node.

--*/

typedef struct _EFI_AM335_VIDEO_DEVICE_PATH {
    VENDOR_DEVICE_PATH VendorPath;
    EFI_DEVICE_PATH_PROTOCOL End;
} EFI_AM335_VIDEO_DEVICE_PATH, *PEFI_AM335_VIDEO_DEVICE_PATH;

/*++

Structure Description:

    This structure stores the internal context for an AM335x video device.

Members:

    Magic - Stores the constant magic value EFI_OMAP4_VIDEO_DEVICE_MAGIC.

    Handle - Stores the graphics out handle.

    GraphicsOut - Stores the graphics output protocol.

    GraphicsOutMode - Stores the graphics output protocol mode.

--*/

typedef struct _EFI_AM335_VIDEO_DEVICE {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOut;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GraphicsOutMode;
} EFI_AM335_VIDEO_DEVICE, *PEFI_AM335_VIDEO_DEVICE;

typedef struct _EFI_TDA19988_MODE {
    UINT32 Clock;
    UINT32 HorizontalDisplay;
    UINT32 HorizontalSyncStart;
    UINT32 HorizontalSyncEnd;
    UINT32 HorizontalTotal;
    UINT32 HorizontalSkew;
    UINT32 VerticalDisplay;
    UINT32 VerticalSyncStart;
    UINT32 VerticalSyncEnd;
    UINT32 VerticalTotal;
    UINT32 VerticalScan;
    UINT32 Flags;
} EFI_TDA19988_MODE, *PEFI_TDA19988_MODE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipAm335GraphicsQueryMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber,
    UINTN *SizeOfInfo,
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info
    );

EFIAPI
EFI_STATUS
EfipAm335GraphicsSetMode (
    EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    UINT32 ModeNumber
    );

EFIAPI
EFI_STATUS
EfipAm335GraphicsBlt (
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
EfipBeagleBoneBlackInitializeVideo (
    UINTN FrameBufferBase
    );

VOID
EfipBeagleBoneBlackSetVideoPinMuxing (
    VOID
    );

VOID
EfipTda19988Initialize (
    VOID
    );

BOOLEAN
EfipTda19988IsDisplayConnected (
    VOID
    );

EFI_STATUS
EfipTda19988HdmiInitialize (
    VOID
    );

VOID
EfipTda19988InitializeEncoder (
    PEFI_TDA19988_MODE Mode
    );

EFI_STATUS
EfipTda19988ReadEdid (
    UINT8 *Buffer,
    UINTN Size
    );

VOID
EfipTda19988Set (
    UINT16 Register,
    UINT8 Bits
    );

VOID
EfipTda19988Clear (
    UINT16 Register,
    UINT8 Bits
    );

VOID
EfipTda19988Read (
    UINT16 Register,
    UINT8 *Data
    );

VOID
EfipTda19988Write (
    UINT16 Register,
    UINT8 Data
    );

VOID
EfipTda19988Write2 (
    UINT16 Register,
    UINT16 Data
    );

VOID
EfipTda19988ReadMultiple (
    UINT16 Register,
    UINT8 Size,
    UINT8 *Data
    );

VOID
EfipTda19988SetPage (
    UINT8 PageNumber
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the device path of the video controller.
//

EFI_AM335_VIDEO_DEVICE_PATH EfiAm335VideoDevicePathTemplate = {
    {
        {
            HARDWARE_DEVICE_PATH,
            HW_VENDOR_DP,
            sizeof(VENDOR_DEVICE_PATH)
        },

        EFI_AM335_VIDEO_DEVICE_GUID,
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

EFI_AM335_VIDEO_MODE EfiAm335VideoModes[] = {
    {
        {
            0,
            640,
            480,
            PixelBitMask,
            {
                0x0000001F,
                0x000007E0,
                0x0000F800,
                0x00000000
            },

            640
        },
    },

    {
        {
            0,
            1024,
            768,
            PixelBitMask,
            {
                0x0000001F,
                0x000007E0,
                0x0000F800,
                0x00000000
            },

            1024
        },
    },
};

EFI_TDA19988_MODE EfiTda19988Mode640x480 = {
    25175,
    640,
    640 + 16,
    640 + 16 + 96,
    640 + 16 + 96 + 48,
    96,
    480,
    480 + 10,
    480 + 10 + 2,
    480 + 10 + 2 + 33,
    0,
    TDA19988_MODE_FLAG_NEGATE_HSYNC | TDA19988_MODE_FLAG_NEGATE_VSYNC |
        TDA19988_MODE_FLAG_HORIZONTAL_SKEW
};

EFI_TDA19988_MODE EfiTda19988Mode800x600 = {
    40000,
    800,
    800 + 40,
    800 + 40 + 128,
    800 + 40 + 128 + 88,
    0,
    600,
    600 + 1,
    600 + 1 + 4,
    600 + 1 + 4 + 22 - 1,
    0,
    TDA19988_MODE_FLAG_HORIZONTAL_SKEW
};

EFI_TDA19988_MODE EfiTda19988Mode1024x768 = {
    65000,
    1024,
    1024 + 24,
    1024 + 24 + 136,
    1024 + 24 + 136 + 160,
    136,
    768,
    768 + 4,
    768 + 4 + 6,
    768 + 4 + 6 + 29 - 1,
    0,
    TDA19988_MODE_FLAG_NEGATE_HSYNC | TDA19988_MODE_FLAG_NEGATE_VSYNC |
        TDA19988_MODE_FLAG_HORIZONTAL_SKEW
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBeagleBoneBlackEnumerateVideo (
    VOID
    )

/*++

Routine Description:

    This routine enumerates the display on the BeagleBone Black.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    PEFI_AM335_VIDEO_DEVICE Device;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    PEFI_AM335_VIDEO_MODE Mode;
    EFI_STATUS Status;

    FrameBufferBase = -1;
    Device = NULL;
    Mode = &(EfiAm335VideoModes[EFI_AM335_VIDEO_DEFAULT_MODE]);

    //
    // Allocate space for the frame buffer.
    //

    Status = EfiAllocatePages(AllocateAnyPages,
                              EfiMemoryMappedIO,
                              EFI_SIZE_TO_PAGES(EFI_AM335_FRAME_BUFFER_SIZE),
                              &FrameBufferBase);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiSetMem((VOID *)(UINTN)FrameBufferBase, AM335_PALETTE_SIZE, 0);
    *((UINT16 *)(UINTN)FrameBufferBase) = AM335_PALETTE_NONE;

    //
    // Initialize the video to the default mode.
    //

    EfipTda19988Initialize();
    EfipBeagleBoneBlackInitializeVideo(FrameBufferBase);

    //
    // Everything's all set up, create the graphics output protocol.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_AM335_VIDEO_DEVICE),
                             (VOID **)&Device);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

    EfiSetMem(Device, sizeof(EFI_AM335_VIDEO_DEVICE), 0);
    Device->Magic = EFI_AM335_VIDEO_DEVICE_MAGIC;
    Device->GraphicsOut.QueryMode = EfipAm335GraphicsQueryMode;
    Device->GraphicsOut.SetMode = EfipAm335GraphicsSetMode;
    Device->GraphicsOut.Blt = EfipAm335GraphicsBlt;
    Device->GraphicsOut.Mode = &(Device->GraphicsOutMode);
    Device->GraphicsOutMode.MaxMode = EFI_AM335_VIDEO_MODE_COUNT;
    Device->GraphicsOutMode.Mode = EFI_AM335_VIDEO_DEFAULT_MODE;
    Device->GraphicsOutMode.Info = &(Mode->Information);
    Device->GraphicsOutMode.SizeOfInfo =
                                  sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    Device->GraphicsOutMode.FrameBufferBase =
                                          FrameBufferBase + AM335_PALETTE_SIZE;

    Device->GraphicsOutMode.FrameBufferSize = EFI_AM335_FRAME_BUFFER_SIZE -
                                              AM335_PALETTE_SIZE;

    Status = EfiInstallMultipleProtocolInterfaces(
                                              &(Device->Handle),
                                              &EfiGraphicsOutputProtocolGuid,
                                              &(Device->GraphicsOut),
                                              &EfiDevicePathProtocolGuid,
                                              &EfiAm335VideoDevicePathTemplate,
                                              NULL);

    if (EFI_ERROR(Status)) {
        goto EnumerateVideoEnd;
    }

EnumerateVideoEnd:
    if (EFI_ERROR(Status)) {
        if (FrameBufferBase != -1) {
            EfiFreePages(FrameBufferBase,
                         EFI_SIZE_TO_PAGES(EFI_AM335_FRAME_BUFFER_SIZE));
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
EfipAm335GraphicsQueryMode (
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

    if ((ModeNumber >= EFI_AM335_VIDEO_MODE_COUNT) || (SizeOfInfo == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                             (VOID **)&Information);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCopyMem(Information,
               &(EfiAm335VideoModes[ModeNumber].Information),
               sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

    *Info = Information;
    *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipAm335GraphicsSetMode (
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

    PEFI_AM335_VIDEO_MODE Mode;

    if (ModeNumber >= EFI_AM335_VIDEO_MODE_COUNT) {
        return EFI_UNSUPPORTED;
    }

    Mode = &(EfiAm335VideoModes[ModeNumber]);
    EfipBeagleBoneBlackInitializeVideo(This->Mode->FrameBufferBase);
    This->Mode->Info = &(Mode->Information);
    This->Mode->Mode = ModeNumber;
    This->Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipAm335GraphicsBlt (
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
EfipBeagleBoneBlackInitializeVideo (
    UINTN FrameBufferBase
    )

/*++

Routine Description:

    This routine initializes a basic frame buffer for the TI AM335x BeagleBone
    Black.

Arguments:

    FrameBufferBase - Supplies the base of the frame buffer memory to use.

Return Value:

    None.

--*/

{

    UINT32 Divisor;
    UINT32 HorizontalBackPorch;
    UINT32 HorizontalFrontPorch;
    UINT32 HorizontalPixels;
    UINT32 HorizontalSync;
    UINT32 Timing0;
    UINT32 Timing1;
    UINT32 Timing2;
    UINT32 Value;
    UINT32 VerticalBackPorch;
    UINT32 VerticalFrontPorch;
    UINT32 VerticalLines;
    UINT32 VerticalSync;

    EfipBeagleBoneBlackSetVideoPinMuxing();

    //
    // Enable the clocks for the DMA submodule, LIDD submodule, and for the
    // core (including raster active and passive). Disable raster control.
    //

    Value = AM335_LCD_CLOCK_ENABLE_CORE |
            AM335_LCD_CLOCK_ENABLE_DMA |
            AM335_LCD_CLOCK_ENABLE_LIDD;

    AM335_LCD_WRITE(AM335_LCD_CLOCK_ENABLE, Value);
    Value = AM335_LCD_READ(AM335_LCD_RASTER_CONTROL);
    Value &= ~AM335_LCD_RASTER_CONTROL_ENABLE;
    AM335_LCD_WRITE(AM335_LCD_RASTER_CONTROL, Value);

    //
    // Configure the divisor for the pixel clock. The divisor must be less than
    // 255.
    //

    Value = AM335_LCD_CONTROL_RASTER_MODE;
    AM335_LCD_WRITE(AM335_LCD_CONTROL, Value);
    for (Divisor = 2; Divisor < 255; Divisor += 1) {
        if (AM335_LCD_MODULE_CLOCK / Divisor <= BEAGLE_BONE_BLACK_PIXEL_CLOCK) {
            break;
        }
    }

    Value = AM335_LCD_CONTROL_RASTER_MODE |
            (Divisor << AM335_LCD_CONTROL_DIVISOR_SHIFT);

    AM335_LCD_WRITE(AM335_LCD_CONTROL, Value);

    //
    // Configure DMA properties of the controller.
    //

    Value = AM335_LCD_DMA_BURST_SIZE_16 | AM335_LCD_DMA_FIFO_THRESHOLD_8;
    AM335_LCD_WRITE(AM335_LCD_DMA_CONTROL, Value);

    //
    // Configure the LCD mode.
    //

    Value = AM335_LCD_RASTER_CONTROL_TFT;
    AM335_LCD_WRITE(AM335_LCD_RASTER_CONTROL, Value);

    //
    // Configure the LCD timing.
    //

    HorizontalBackPorch = BEAGLE_BONE_BLACK_HORIZONTAL_BACK_PORCH - 1;
    HorizontalFrontPorch = BEAGLE_BONE_BLACK_HORIZONTAL_FRONT_PORCH - 1;
    HorizontalPixels = BEAGLE_BONE_BLACK_RESOLUTION_X - 1;
    HorizontalSync = BEAGLE_BONE_BLACK_HSYNC - 1;
    VerticalBackPorch = BEAGLE_BONE_BLACK_VERTICAL_BACK_PORCH - 1;
    VerticalFrontPorch = BEAGLE_BONE_BLACK_VERTICAL_FRONT_PORCH - 1;
    VerticalLines = BEAGLE_BONE_BLACK_RESOLUTION_Y - 1;
    VerticalSync = BEAGLE_BONE_BLACK_VSYNC - 1;
    Timing2 = 0;
    Timing0 = Value = AM335_LCD_RESOLUTION_X_TO_TIMING_0(HorizontalPixels);
    Timing0 |= (HorizontalBackPorch &
                AM335_LCD_RASTER_TIMING_PORCH_LOW_MASK) <<
               AM335_LCD_RASTER_TIMING_0_HORIZONTAL_BACK_PORCH_SHIFT;

    Timing0 |= (HorizontalFrontPorch &
                AM335_LCD_RASTER_TIMING_PORCH_LOW_MASK) <<
               AM335_LCD_RASTER_TIMING_0_HORIZONTAL_FRONT_PORCH_SHIFT;

    Timing0 |= (HorizontalSync & AM335_LCD_RASTER_TIMING_0_HSYNC_MASK) <<
               AM335_LCD_RASTER_TIMING_0_HSYNC_SHIFT;

    Timing1 = AM335_LCD_RESOLUTION_Y_TO_TIMING_1(VerticalLines);
    Timing1 |= (VerticalBackPorch &
                AM335_LCD_RASTER_TIMING_PORCH_LOW_MASK) <<
               AM335_LCD_RASTER_TIMING_1_VERTICAL_BACK_PORCH_SHIFT;

    Timing1 |= (VerticalFrontPorch &
                AM335_LCD_RASTER_TIMING_PORCH_LOW_MASK) <<
               AM335_LCD_RASTER_TIMING_1_VERTICAL_FRONT_PORCH_SHIFT;

    Timing1 |= (VerticalSync & AM335_LCD_RASTER_TIMING_PORCH_LOW_MASK) <<
               AM335_LCD_RASTER_TIMING_1_VSYNC_SHIFT;

    Timing2 = AM335_LCD_RESOLUTION_Y_TO_TIMING_2(VerticalLines);
    Timing2 |= AM335_LCD_RASTER_TIMING_2_INVERT_VERTICAL_SYNC;
    Timing2 |= 255 << AM335_LCD_RASTER_TIMING_2_AC_BIAS_FREQUENCY_SHIFT;
    HorizontalFrontPorch >>= AM335_LCD_RASTER_TIMING_PORCH_HIGH_SHIFT;
    HorizontalBackPorch >>= AM335_LCD_RASTER_TIMING_PORCH_HIGH_SHIFT;
    HorizontalSync >>= AM335_LCD_RASTER_TIMING_HSYNC_HIGH_SHIFT;
    Timing2 |= (HorizontalBackPorch &
                AM335_LCD_RASTER_TIMING_PORCH_HIGH_MASK) <<
               AM335_LCD_RASTER_TIMING_2_HORIZONTAL_BACK_PORCH_HIGH_SHIFT;

    Timing2 |= (HorizontalFrontPorch &
                AM335_LCD_RASTER_TIMING_PORCH_HIGH_MASK) <<
               AM335_LCD_RASTER_TIMING_2_HORIZONTAL_FRONT_PORCH_HIGH_SHIFT;

    Timing2 |= (HorizontalSync & AM335_LCD_RASTER_TIMING_HSYNC_HIGH_MASK) <<
               AM335_LCD_RASTER_TIMING_2_HORIZONTAL_SYNC_HIGH_SHIFT;

    AM335_LCD_WRITE(AM335_LCD_RASTER_TIMING_0, Timing0);
    AM335_LCD_WRITE(AM335_LCD_RASTER_TIMING_1, Timing1);
    AM335_LCD_WRITE(AM335_LCD_RASTER_TIMING_2, Timing2);

    //
    // Configure the palette load delay.
    //

    Value = AM335_LCD_READ(AM335_LCD_RASTER_CONTROL);
    Value &= ~AM335_LCD_RASTER_CONTROL_FIFO_DMA_DELAY_MASK;
    Value |= 128 << AM335_LCD_RASTER_CONTROL_FIFO_DMA_DELAY_SHIFT;
    Value &= ~AM335_LCD_RASTER_CONTROL_PALETTE_LOAD_MASK;
    Value |= AM335_LCD_RASTER_CONTROL_PALETTE_LOAD_DATA_ONLY;
    AM335_LCD_WRITE(AM335_LCD_RASTER_CONTROL, Value);

    //
    // Set up the frame buffer base.
    //

    Value = FrameBufferBase;
    AM335_LCD_WRITE(AM335_LCD_FB0_BASE, Value);
    Value += EFI_AM335_FRAME_BUFFER_SIZE - 1;
    AM335_LCD_WRITE(AM335_LCD_FB0_CEILING, Value);

    //
    // Reset the LCD module.
    //

    Value = AM335_LCD_CLOCK_RESET_MAIN;
    AM335_LCD_WRITE(AM335_LCD_CLOCK_RESET, Value);
    EfiStall(100000);
    AM335_LCD_WRITE(AM335_LCD_CLOCK_RESET, 0);

    //
    // Enable output.
    //

    Value = AM335_LCD_READ(AM335_LCD_RASTER_CONTROL);
    Value |= AM335_LCD_RASTER_CONTROL_ENABLE;
    AM335_LCD_WRITE(AM335_LCD_RASTER_CONTROL, Value);
    Value = AM335_LCD_SYSTEM_CONFIG_STANDBY_SMART |
            AM335_LCD_SYSTEM_CONFIG_IDLE_SMART;

    AM335_LCD_WRITE(AM335_LCD_SYSTEM_CONFIG, Value);
    return;
}

VOID
EfipBeagleBoneBlackSetVideoPinMuxing (
    VOID
    )

/*++

Routine Description:

    This routine sets up the proper pin muxing for the LCD on a BeagleBone
    Black.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // The first 16 data pins are mux mode 0.
    //

    Value = 0;
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(0), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(1), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(2), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(3), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(4), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(5), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(6), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(7), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(8), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(9), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(10), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(11), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(12), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(13), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(14), Value);
    AM335_SOC_WRITE(AM335_PAD_LCD_DATA(15), Value);

    //
    // The other control signals are mux mode 0.
    //

    Value = 0;
    AM335_SOC_WRITE(AM335_SOC_CONTROL_CONF_LCD_VSYNC, Value);
    AM335_SOC_WRITE(AM335_SOC_CONTROL_CONF_LCD_HSYNC, Value);
    AM335_SOC_WRITE(AM335_SOC_CONTROL_CONF_LCD_PCLK, Value);
    AM335_SOC_WRITE(AM335_SOC_CONTROL_CONF_LCD_AC_BIAS_EN, Value);

    //
    // Set ball A15 to output CLKOUT1.
    //

    AM335_SOC_WRITE(AM335_SOC_CONTROL_CONF_XDMA_EVENT_INTR0, 3);
    return;
}

VOID
EfipTda19988Initialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the TDA19988 HDMI framer chip.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BOOLEAN DisplayConnected;
    UINT8 EdidData[128];
    UINT32 Try;

    EfipAm335I2c0Initialize();
    EfipTda19988HdmiInitialize();
    for (Try = 0; Try < 20; Try += 1) {
        DisplayConnected = EfipTda19988IsDisplayConnected();
        if (DisplayConnected != FALSE) {
            break;
        }

        EfiStall(10000);
    }

    //
    // This code always sets the resolution to 1024x768, but the framework is
    // here to potentially support native resolutions.
    //

    if (DisplayConnected != FALSE) {
        EfiSetMem(EdidData, sizeof(EdidData), 0);
        EfipTda19988ReadEdid(EdidData, sizeof(EdidData));
    }

    EfipTda19988InitializeEncoder(&EfiTda19988Mode1024x768);

    //
    // Write default values for RBG 4:4:4.
    //

    EfipTda19988Write(TDA19988_CONTROL_VIP_CONTROL_0, 0x23);
    EfipTda19988Write(TDA19988_CONTROL_VIP_CONTROL_1, 0x45);
    EfipTda19988Write(TDA19988_CONTROL_VIP_CONTROL_2, 0x01);
    return;
}

BOOLEAN
EfipTda19988IsDisplayConnected (
    VOID
    )

/*++

Routine Description:

    This routine determines if a display is connected.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT8 Value;

    EfipAm335I2c0SetSlaveAddress(EFI_TDA19988_CEC_BUS_ADDRESS);
    EfipAm335I2c0Read(TDA19988_CEC_STATUS, 1, &Value);

    //
    // Accept either the official hot-plug detect or the jankier RX sense, as
    // a pre-connected monitor seems to sometimes never set HPD.
    //

    if ((Value &
         (TDA19988_CEC_STATUS_HOT_PLUG_DETECT |
          TDA19988_CEC_STATUS_RX_SENSE)) != 0) {

        return TRUE;
    }

    return FALSE;
}

EFI_STATUS
EfipTda19988HdmiInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the TDA19988.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Revision;
    UINT8 Value;

    //
    // Enable the CEC module.
    //

    EfipAm335I2c0SetSlaveAddress(EFI_TDA19988_CEC_BUS_ADDRESS);
    Value = TDA19988_CEC_ENABLE_RX_SENSE | TDA19988_CEC_ENABLE_HDMI;
    EfipAm335I2c0Write(TDA19988_CEC_ENABLE, 1, &Value);
    EfiStall(10000);
    EfipAm335I2c0Read(TDA19988_CEC_STATUS, 1, &Value);

    //
    // Perform a soft reset on the DDC bus.
    //

    Value = TDA19988_CONTROL_RESET_DDC;
    EfipTda19988Set(TDA19988_CONTROL_RESET, Value);
    EfiStall(100000);
    EfipTda19988Clear(TDA19988_CONTROL_RESET, Value);
    EfiStall(100000);
    Value = TDA19988_CONTROL_MAIN_CONTROL_SOFT_RESET;
    EfipTda19988Set(TDA19988_CONTROL_MAIN_CONTROL, Value);
    EfiStall(10000);
    EfipTda19988Clear(TDA19988_CONTROL_MAIN_CONTROL, Value);

    //
    // Set the TMDS bias.
    //

    Value = TDA19988_PLL_ANALOG_TX_VSWING_VALUE;
    EfipTda19988Write(TDA19988_PLL_ANALOG_CONTROL, Value);

    //
    // Set PLL registers.
    //

    EfipTda19988Write(TDA19988_PLL_SERIAL_1, 0);
    Value = TDA19988_PLL_SERIAL_2_SRL_NOSC(1);
    EfipTda19988Write(TDA19988_PLL_SERIAL_2, Value);
    EfipTda19988Write(TDA19988_PLL_SERIAL_3, 0);
    EfipTda19988Write(TDA19988_PLL_SERIALIZER, 0);
    EfipTda19988Write(TDA19988_PLL_BUFFER_OUT, 0);
    EfipTda19988Write(TDA19988_PLL_SCG1, 0);
    Value = TDA19988_PLL_CLOCK_SELECT_VALUE;
    EfipTda19988Write(TDA19988_PLL_CLOCK_SELECT, Value);

    //
    // Configure for video in format that is not 480i or 576i.
    //

    Value = TDA19988_PLL_SCGN1_VALUE;
    EfipTda19988Write(TDA19988_PLL_SCGN1, Value);
    Value = 0;
    EfipTda19988Write(TDA19988_PLL_SCGN2, Value);
    Value = TDA19988_PLL_SCGR1_VALUE;
    EfipTda19988Write(TDA19988_PLL_SCGR1, Value);
    Value = 0;
    EfipTda19988Write(TDA19988_PLL_SCGR2, Value);

    //
    // Set single edge mode (for formats that are not 480i or 576i).
    //

    Value = TDA19988_PLL_SCG2_VALUE;
    EfipTda19988Write(TDA19988_PLL_SCG2, Value);
    Value = TDA19988_CONTROL_VP_VIP_OUT_VALUE;
    EfipTda19988Write(TDA19988_CONTROL_VP_VIP_OUT, Value);

    //
    // Verify the TDA19988 chip revision.
    //

    EfipTda19988Read(TDA19988_CONTROL_REVISION_LOW, &Value);
    Revision = Value;
    EfipTda19988Read(TDA19988_CONTROL_REVISION_HIGH, &Value);
    Revision |= ((UINT32)Value) << 8;
    if (Revision != TDA19988_HDMI_REVISION_VALUE) {
        return EFI_NOT_FOUND;
    }

    //
    // Enable DDC.
    //

    Value = TDA19988_CONTROL_DDC_CONTROL_ENABLE;
    EfipTda19988Write(TDA19988_CONTROL_DDC_CONTROL, Value);

    //
    // Set up the DDC clock.
    //

    Value = TDA19988_HDCP_OTP_TX3_VALUE;
    EfipTda19988Write(TDA19988_HDCP_OTP_TX3, Value);
    EfipAm335I2c0SetSlaveAddress(EFI_TDA19988_CEC_BUS_ADDRESS);
    Value = TDA19988_CEC_FRO_IM_CLOCK_CONTROL_VALUE;
    EfipAm335I2c0Write(TDA19988_CEC_FRO_IM_CLOCK_CONTROL, 1, &Value);
    return EFI_SUCCESS;
}

VOID
EfipTda19988InitializeEncoder (
    PEFI_TDA19988_MODE Mode
    )

/*++

Routine Description:

    This routine sets the video parameters for the HDMI encoder of the TDA19988.

Arguments:

    Mode - Supplies a pointer to the requested mode.

Return Value:

    None.

--*/

{

    UINT16 DeStart;
    UINT16 DeStop;
    UINT8 Divisor;
    UINT16 HsPixelStart;
    UINT16 HsPixelStop;
    UINT16 NumberOfLines;
    UINT16 NumberOfPixels;
    UINT16 ReferenceLine;
    UINT16 ReferencePixel;
    UINT16 Value;
    UINT16 Vs1LineEnd;
    UINT16 Vs1LineStart;
    UINT16 Vs1PixelEnd;
    UINT16 Vs1PixelStart;
    UINT16 Vs2LineEnd;
    UINT16 Vs2LineStart;
    UINT16 Vs2PixelEnd;
    UINT16 Vs2PixelStart;
    UINT16 Vwin1LineEnd;
    UINT16 Vwin1LineStart;
    UINT16 Vwin2LineEnd;
    UINT16 Vwin2LineStart;

    NumberOfPixels = Mode->HorizontalTotal;
    NumberOfLines = Mode->VerticalTotal;
    HsPixelStop = Mode->HorizontalSyncEnd - Mode->HorizontalDisplay;
    HsPixelStart = Mode->HorizontalSyncStart - Mode->HorizontalDisplay;
    DeStop = Mode->HorizontalTotal;
    DeStart = Mode->HorizontalTotal - Mode->HorizontalDisplay;
    ReferencePixel = HsPixelStart + 3;
    if ((Mode->Flags & TDA19988_MODE_FLAG_HORIZONTAL_SKEW) != 0) {
        ReferencePixel += Mode->HorizontalSkew;
    }

    if ((Mode->Flags & TDA19988_MODE_FLAG_INTERLACE) != 0) {
        ReferenceLine = ((Mode->VerticalSyncStart -
                          Mode->VerticalDisplay) / 2) + 1;

        Vwin1LineStart = (Mode->VerticalTotal - Mode->VerticalDisplay) / 2;
        Vwin1LineEnd = Vwin1LineStart + (Mode->VerticalDisplay / 2);
        Vs1PixelStart = HsPixelStart;
        Vs1PixelEnd = Vs1PixelStart;
        Vs1LineStart = (Mode->VerticalSyncStart - Mode->VerticalDisplay) / 2;
        Vs1LineEnd = Vs1LineStart +
                     ((Mode->VerticalSyncEnd - Mode->VerticalSyncStart) / 2);

        Vwin2LineStart = Vwin1LineStart + (Mode->VerticalTotal / 2);
        Vwin2LineEnd = Vwin2LineStart + (Mode->VerticalDisplay / 2);
        Vs2PixelStart = HsPixelStart + (Mode->HorizontalTotal / 2);
        Vs2PixelEnd = Vs1PixelStart;
        Vs2LineStart = Vs1LineStart + (Mode->VerticalTotal / 2);
        Vs2LineEnd = Vs2LineStart +
                     ((Mode->VerticalSyncEnd - Mode->VerticalSyncStart) / 2);

    } else {
        ReferenceLine = (Mode->VerticalSyncStart - Mode->VerticalDisplay) + 1;
        Vwin1LineStart = Mode->VerticalTotal - Mode->VerticalDisplay - 1;
        Vwin1LineEnd = Vwin1LineStart + Mode->VerticalDisplay;
        Vs1PixelStart = HsPixelStart;
        Vs1PixelEnd = Vs1PixelStart;
        Vs1LineStart = Mode->VerticalSyncStart - Mode->VerticalDisplay;
        Vs1LineEnd = Vs1LineStart +
                     Mode->VerticalSyncEnd - Mode->VerticalSyncStart;

        Vwin2LineStart = 0;
        Vwin2LineEnd = 0;
        Vs2PixelStart = 0;
        Vs2PixelEnd = 0;
        Vs2LineStart = 0;
        Vs2LineEnd = 0;
    }

    Divisor = 148500 / Mode->Clock;
    if (Divisor != 0) {
        Divisor -= 1;
        if (Divisor > 3) {
            Divisor = 3;
        }
    }

    //
    // Switch HDCP mode off for DVI.
    //

    Value = TDA19988_CONTROL_TBG_CONTROL_1_DISABLE_DWIN;
    EfipTda19988Set(TDA19988_CONTROL_TBG_CONTROL_1, Value);
    Value = TDA19988_HDCP_OTP_TX33_HDMI;
    EfipTda19988Clear(TDA19988_HDCP_OTP_TX33, Value);

    //
    // Set the encoder to DVI mode.
    //

    Value = 0;
    EfipTda19988Write(TDA19988_AUDIO_ENCODE_CONTROL, Value);

    //
    // Disable pre-filter and interpolator.
    //

    Value = 0;
    EfipTda19988Write(TDA19988_CONTROL_HVF_CONTROL_0, Value);
    EfipTda19988Write(TDA19988_CONTROL_VIP_CONTROL_5, Value);
    EfipTda19988Write(TDA19988_CONTROL_VIP_CONTROL_4, Value);
    Value = TDA19988_PLL_SERIAL_3_SRL_CCIR;
    EfipTda19988Clear(TDA19988_PLL_SERIAL_3, Value);
    Value = TDA19988_PLL_SERIAL_1_SRL_MAN_IP;
    EfipTda19988Clear(TDA19988_PLL_SERIAL_1, Value);
    Value = TDA19988_PLL_SERIAL_3_DE;
    EfipTda19988Clear(TDA19988_PLL_SERIAL_3, Value);
    Value = 0;
    EfipTda19988Write(TDA19988_PLL_SERIALIZER, Value);
    Value = TDA19988_CONTROL_HVF_CONTROL_1_VQR_FULL;
    EfipTda19988Write(TDA19988_CONTROL_HVF_CONTROL_1, Value);
    Value = 0;
    EfipTda19988Write(TDA19988_CONTROL_RPT_CONTROL, Value);
    Value = TDA19988_PLL_CLOCK_SELECT_VALUE;
    EfipTda19988Write(TDA19988_PLL_CLOCK_SELECT, Value);
    Value = TDA19988_PLL_SERIAL_2_SRL_NOSC(Divisor) |
            TDA19988_PLL_SERIAL_2_SRL_PR(0);

    EfipTda19988Write(TDA19988_PLL_SERIAL_2, Value);

    //
    // Set video input/output parameters. Set the matrix conversion to bypass
    // the matrix.
    //

    Value = TDA19988_CONTROL_MATRIX_CONTROL_BYPASS;
    EfipTda19988Set(TDA19988_CONTROL_MATRIX_CONTROL, Value);
    Value = TDA19988_CONTROL_TBG_CONTROL_0_SYNC_METHOD;
    EfipTda19988Clear(TDA19988_CONTROL_TBG_CONTROL_0, Value);

    //
    // Set the TMDS bias.
    //

    Value = TDA19988_PLL_ANALOG_TX_VSWING_VALUE;
    EfipTda19988Write(TDA19988_PLL_ANALOG_CONTROL, Value);

    //
    // Sync on risiding edge.
    // Set embedded sync, and enable V, H, and X toggle.
    //

    Value = TDA19988_CONTROL_VIP_CONTROL_3_SYNC_HS;
    if ((Mode->Flags & TDA19988_MODE_FLAG_NEGATE_HSYNC) != 0) {
        Value |= TDA19988_CONTROL_VIP_CONTROL_3_H_TOGGLE;
    }

    if ((Mode->Flags & TDA19988_MODE_FLAG_NEGATE_VSYNC) != 0) {
        Value |= TDA19988_CONTROL_VIP_CONTROL_3_V_TOGGLE;
    }

    EfipTda19988Write(TDA19988_CONTROL_VIP_CONTROL_3, Value);
    Value = TDA19988_CONTROL_TBG_CONTROL_1_TOGGLE_ENABLE;
    if ((Mode->Flags & TDA19988_MODE_FLAG_NEGATE_HSYNC) != 0) {
        Value |= TDA19988_CONTROL_TBG_CONTROL_1_H_TOGGLE;
    }

    if ((Mode->Flags & TDA19988_MODE_FLAG_NEGATE_VSYNC) != 0) {
        Value |= TDA19988_CONTROL_TBG_CONTROL_1_V_TOGGLE;
    }

    EfipTda19988Write(TDA19988_CONTROL_TBG_CONTROL_1, Value);

    //
    // Set video parameters.
    //

    Value = 0;
    EfipTda19988Write(TDA19988_CONTROL_VIDEOFORMAT, Value);
    EfipTda19988Write2(TDA19988_CONTROL_REFERENCE_PIXEL_HIGH, ReferencePixel);
    EfipTda19988Write2(TDA19988_CONTROL_REFERENCE_LINE_HIGH, ReferenceLine);
    EfipTda19988Write2(TDA19988_CONTROL_NPIXELS_HIGH, NumberOfPixels);
    EfipTda19988Write2(TDA19988_CONTROL_NLINES_HIGH, NumberOfLines);
    EfipTda19988Write2(TDA19988_CONTROL_VS_LINE_START_1_HIGH, Vs1LineStart);
    EfipTda19988Write2(TDA19988_CONTROL_VS_PIXEL_START_1_HIGH, Vs1PixelStart);
    EfipTda19988Write2(TDA19988_CONTROL_VS_LINE_END_1_HIGH, Vs1LineEnd);
    EfipTda19988Write2(TDA19988_CONTROL_VS_PIXEL_END_1_HIGH, Vs1PixelEnd);
    EfipTda19988Write2(TDA19988_CONTROL_VS_LINE_START_2_HIGH, Vs2LineStart);
    EfipTda19988Write2(TDA19988_CONTROL_VS_PIXEL_START_2_HIGH, Vs2PixelStart);
    EfipTda19988Write2(TDA19988_CONTROL_VS_LINE_END_2_HIGH, Vs2LineEnd);
    EfipTda19988Write2(TDA19988_CONTROL_VS_PIXEL_END_2_HIGH, Vs2PixelEnd);
    EfipTda19988Write2(TDA19988_CONTROL_HS_PIXEL_START_HIGH, HsPixelStart);
    EfipTda19988Write2(TDA19988_CONTROL_HS_PIXEL_STOP_HIGH, HsPixelStop);
    EfipTda19988Write2(TDA19988_CONTROL_VWIN_START_1_HIGH, Vwin1LineStart);
    EfipTda19988Write2(TDA19988_CONTROL_VWIN_END_1_HIGH, Vwin1LineEnd);
    EfipTda19988Write2(TDA19988_CONTROL_VWIN_START_2_HIGH, Vwin2LineStart);
    EfipTda19988Write2(TDA19988_CONTROL_VWIN_END_2_HIGH, Vwin2LineEnd);
    EfipTda19988Write2(TDA19988_CONTROL_DE_START_HIGH, DeStart);
    EfipTda19988Write2(TDA19988_CONTROL_DE_STOP_HIGH, DeStop);
    EfipTda19988Write(TDA19988_CONTROL_ENABLE_SPACE, 0);

    //
    // Control 0 must be the last register set.
    //

    EfipTda19988Clear(TDA19988_CONTROL_TBG_CONTROL_0,
                      TDA19988_CONTROL_TBG_CONTROL_0_SYNC_ONCE);

    return;
}

EFI_STATUS
EfipTda19988ReadEdid (
    UINT8 *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine reads the EDID data from the connected monitor.

Arguments:

    Buffer - Supplies a pointer where the EDID data will be returned.

    Size - Supplies the number of bytes to read. This must be at least 128
        bytes.

Return Value:

    EFI Status code.

--*/

{

    UINTN Try;
    UINT8 Value;

    Value = TDA19988_HDCP_OTP_TX4_PD_RAM;
    EfipTda19988Clear(TDA19988_HDCP_OTP_TX4, Value);

    //
    // Enable the EDID block read interrupt.
    //

    Value = TDA19988_CONTROL_INTERRUPT_EDID;
    EfipTda19988Set(TDA19988_CONTROL_INTERRUPT, Value);

    //
    // Enable global interrupts.
    //

    Value = TDA19988_CONTROL_INTERRUPT_CONTROL_GLOBAL_ENABLE;
    EfipTda19988Set(TDA19988_CONTROL_INTERRUPT_CONTROL, Value);

    //
    // Set the device address.
    //

    Value = TDA19988_EDID_DEVICE_ADDRESS_EDID;
    EfipTda19988Write(TDA19988_EDID_DEVICE_ADDRESS, Value);

    //
    // Set the EDID offset.
    //

    Value = TDA19988_EDID_OFFSET_VALUE;
    EfipTda19988Write(TDA19988_EDID_OFFSET, Value);

    //
    // Set the EDID segment pointer address.
    //

    Value = TDA19988_EDID_SEGMENT_POINTER_ADDRESS_VALUE;
    EfipTda19988Write(TDA19988_EDID_SEGMENT_POINTER_ADDRESS, Value);

    //
    // Set the EDID segment address.
    //

    Value = TDA19988_EDID_SEGMENT_ADDRESS_VALUE;
    EfipTda19988Write(TDA19988_EDID_SEGMENT_ADDRESS, Value);

    //
    // Pulse the EDID read request bit to make the read happen.
    //

    Value = TDA19988_EDID_REQUEST_READ;
    EfipTda19988Write(TDA19988_EDID_REQUEST, Value);
    Value = 0;
    EfipTda19988Write(TDA19988_EDID_REQUEST, Value);

    //
    // Poll the interrupt status flag.
    //

    Value = 0;
    for (Try = 0; Try < 100; Try += 1) {
        EfipTda19988Read(TDA19988_CONTROL_INTERRUPT, &Value);
        if ((Value & TDA19988_CONTROL_INTERRUPT_EDID) != 0) {
            break;
        }
    }

    if ((Value & TDA19988_CONTROL_INTERRUPT_EDID) == 0) {
        return EFI_DEVICE_ERROR;
    }

    //
    // Perform the block read.
    //

    EfipTda19988ReadMultiple(TDA19988_EDID_DATA,
                             Size,
                             Buffer);

    Value = TDA19988_HDCP_OTP_TX4_PD_RAM;
    EfipTda19988Set(TDA19988_HDCP_OTP_TX4, Value);

    //
    // Disable the EDID read interrupt.
    //

    Value = TDA19988_CONTROL_INTERRUPT_EDID;
    EfipTda19988Clear(TDA19988_CONTROL_INTERRUPT, Value);
    return EFI_SUCCESS;
}

VOID
EfipTda19988Set (
    UINT16 Register,
    UINT8 Bits
    )

/*++

Routine Description:

    This routine performs a read-modify-write to set bits in a register.

Arguments:

    Register - Supplies the register number to read. The high 8 bits contain
        the page number and the low 8 bits contain the register number.

    Bits - Supplies the bits to set in the register.

Return Value:

    None.

--*/

{

    UINT8 Data;

    EfipTda19988Read(Register, &Data);
    Data |= Bits;
    EfipTda19988Write(Register, Data);
    return;
}

VOID
EfipTda19988Clear (
    UINT16 Register,
    UINT8 Bits
    )

/*++

Routine Description:

    This routine performs a read-modify-write to clear bits in a register.

Arguments:

    Register - Supplies the register number to read. The high 8 bits contain
        the page number and the low 8 bits contain the register number.

    Bits - Supplies the bits to set in the register.

Return Value:

    None.

--*/

{

    UINT8 Data;

    EfipTda19988Read(Register, &Data);
    Data &= ~Bits;
    EfipTda19988Write(Register, Data);
    return;
}

VOID
EfipTda19988Read (
    UINT16 Register,
    UINT8 *Data
    )

/*++

Routine Description:

    This routine reads from the TDA19988 HDMI block.

Arguments:

    Register - Supplies the register number to read. The high 8 bits contain
        the page number and the low 8 bits contain the register number.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    None.

--*/

{

    UINT8 PageNumber;

    EfipAm335I2c0SetSlaveAddress(EFI_TDA19988_HDMI_BUS_ADDRESS);
    PageNumber = Register >> 8;
    EfipTda19988SetPage(PageNumber);
    EfipAm335I2c0Read(Register & 0xFF, 1, Data);
    return;
}

VOID
EfipTda19988Write (
    UINT16 Register,
    UINT8 Data
    )

/*++

Routine Description:

    This routine writes to the TDA19988 HDMI block.

Arguments:

    Register - Supplies the register number to read. The high 8 bits contain
        the page number and the low 8 bits contain the register number.

    Data - Supplies a pointer to the data to write.

Return Value:

    None.

--*/

{

    UINT8 PageNumber;

    EfipAm335I2c0SetSlaveAddress(EFI_TDA19988_HDMI_BUS_ADDRESS);
    PageNumber = Register >> 8;
    EfipTda19988SetPage(PageNumber);
    EfipAm335I2c0Write(Register & 0xFF, 1, &Data);
    return;
}

VOID
EfipTda19988Write2 (
    UINT16 Register,
    UINT16 Data
    )

/*++

Routine Description:

    This routine writes a two-byte register value to the TDA19988 HDMI block.

Arguments:

    Register - Supplies the register number to read. The high 8 bits contain
        the page number and the low 8 bits contain the register number.

    Data - Supplies a pointer to the data to write.

Return Value:

    None.

--*/

{

    UINT8 PageNumber;
    UINT8 Value[2];

    EfipAm335I2c0SetSlaveAddress(EFI_TDA19988_HDMI_BUS_ADDRESS);
    PageNumber = Register >> 8;
    EfipTda19988SetPage(PageNumber);
    Value[0] = Data >> 8;
    Value[1] = Data & 0xFF;
    EfipAm335I2c0Write(Register & 0xFF, 2, Value);
    return;
}

VOID
EfipTda19988ReadMultiple (
    UINT16 Register,
    UINT8 Size,
    UINT8 *Data
    )

/*++

Routine Description:

    This routine reads multiple bytes from the TDA19988 HDMI block.

Arguments:

    Register - Supplies the register number to read. The high 8 bits contain
        the page number and the low 8 bits contain the register number.

    Size - Supplies the number of bytes to read.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    None.

--*/

{

    UINT8 PageNumber;

    EfipAm335I2c0SetSlaveAddress(EFI_TDA19988_HDMI_BUS_ADDRESS);
    PageNumber = Register >> 8;
    EfipTda19988SetPage(PageNumber);
    EfipAm335I2c0Read(Register & 0xFF, Size, Data);
    return;
}

VOID
EfipTda19988SetPage (
    UINT8 PageNumber
    )

/*++

Routine Description:

    This routine sets the current register page in the TDA19988.

Arguments:

    PageNumber - Supplies the page to set.

Return Value:

    None.

--*/

{

    EfipAm335I2c0Write(TDA19988_PAGE_SELECT_REGISTER, 1, &PageNumber);
    return;
}

