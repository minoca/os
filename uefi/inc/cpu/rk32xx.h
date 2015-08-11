/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    rk32xx.h

Abstract:

    This header contains definitions for the Rockchip 32xx ARMv7 SoC.

Author:

    Evan Green 9-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro computes a PLL clock frequency based on an input frequency of
// 24MHz and the formula given in section 3.9.1. PLL Usage of the RK3288 TRM.
//

#define RK32_CRU_PLL_COMPUTE_CLOCK_FREQUENCY(_Nf, _Nr, _No) \
    ((24 * (_Nf)) / ((_Nr) * (_No))) * 1000000

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the RK3288 register base map.
//

#define RK32_SD_BASE 0xFF0C0000
#define RK32_TIMER0_5_BASE 0xFF6B0000
#define RK32_UART_DEBUG_BASE 0xFF690000
#define RK32_SRAM_BASE 0xFF700000
#define RK32_PMU_BASE 0xFF730000
#define RK32_CRU_BASE 0xFF760000
#define RK32_GRF_BASE 0xFF770000
#define RK32_GPIO0_BASE 0xFF750000
#define RK32_GPIO7_BASE 0xFF7E0000
#define RK32_WATCHDOG_BASE 0xFF800000
#define RK32_TIMER6_7_BASE 0xFF810000
#define RK32_VOP_BIG_BASE 0xFF930000
#define RK32_VOP_LITTLE_BASE 0xFF940000
#define RK32_GIC_DISTRIBUTOR_BASE 0xFFC01000
#define RK32_GIC_CPU_INTERFACE_BASE 0xFFC02000

//
// Define the RK3288 interrupt map.
//

#define RK32_INTERRUPT_USBOTG 55
#define RK32_INTERRUPT_EHCI   56
#define RK32_INTERRUPT_SDMMC  64
#define RK32_INTERRUPT_EMMC   67
#define RK32_INTERRUPT_OHCI   73
#define RK32_INTERRUPT_TIMER0 98
#define RK32_INTERRUPT_TIMER1 99
#define RK32_INTERRUPT_TIMER2 100
#define RK32_INTERRUPT_TIMER3 101
#define RK32_INTERRUPT_TIMER4 102
#define RK32_INTERRUPT_TIMER5 103
#define RK32_INTERRUPT_TIMER6 104
#define RK32_INTERRUPT_TIMER7 105

#define RK32_CRU_SIZE 0x1000

//
// Define the RK32 watchdog range.
//

#define RK32_WATCHDOG_MIN 0x0000FFFF
#define RK32_WATCHDOG_MAX 0x7FFFFFFF

//
// Define timer parameters.
//

#define RK32_TIMER_FREQUENCY 24000000
#define RK32_TIMER_REGISTER_STRIDE 0x00000020

//
// Define generic PLL register bits.
//

#define RK32_PLL_RESET (1 << 5)
#define RK32_PLL_OD_MASK 0x0F
#define RK32_PLL_NR_MASK (0x3F << 8)
#define RK32_PLL_NR_SHIFT 8
#define RK32_PLL_NF_MASK 0x00001FFF
#define RK32_PLL_BWADJ_MASK 0x00000FFF

//
// Define the CRU codec PLL control 0 register bits.
//

#define RK32_CRU_CODEC_PLL_CONTROL0_PROTECT_SHIFT 16
#define RK32_CRU_CODEC_PLL_CONTROL0_CLKR_MASK     (0x3F << 8)
#define RK32_CRU_CODEC_PLL_CONTROL0_CLKR_SHIFT    8
#define RK32_CRU_CODEC_PLL_CONTROL0_CLKOD_MASK    (0xF << 0)
#define RK32_CRU_CODEC_PLL_CONTROL0_CLKOD_SHIFT   0

//
// Define the CRU codec PLL control 1 register bits.
//

#define RK32_CRU_CODEC_PLL_CONTROL1_LOCK       (1 << 31)
#define RK32_CRU_CODEC_PLL_CONTROL1_CLKF_MASK  (0x1FFF << 0)
#define RK32_CRU_CODEC_PLL_CONTROL1_CLKF_SHIFT 0

//
// Define the CRU general PLL control 0 register bits.
//

#define RK32_CRU_GENERAL_PLL_CONTROL0_PROTECT_SHIFT 16
#define RK32_CRU_GENERAL_PLL_CONTROL0_CLKR_MASK     (0x3F << 8)
#define RK32_CRU_GENERAL_PLL_CONTROL0_CLKR_SHIFT    8
#define RK32_CRU_GENERAL_PLL_CONTROL0_CLKOD_MASK    (0xF << 0)
#define RK32_CRU_GENERAL_PLL_CONTROL0_CLKOD_SHIFT   0

//
// Define the CRU general PLL control 1 register bits.
//

#define RK32_CRU_GENERAL_PLL_CONTROL1_LOCK       (1 << 31)
#define RK32_CRU_GENERAL_PLL_CONTROL1_CLKF_MASK  (0x1FFF << 0)
#define RK32_CRU_GENERAL_PLL_CONTROL1_CLKF_SHIFT 0

//
// Define the PLL clock mode frequencies.
//

#define RK32_CRU_PLL_SLOW_MODE_FREQUENCY 24000000
#define RK32_CRU_PLL_DEEP_SLOW_MODE_FREQUENCY 32768

//
// Define the three mode values for the CRU mode control register.
//

#define RK32_CRU_MODE_CONTROL_SLOW_MODE 0
#define RK32_CRU_MODE_CONTROL_NORMAL_MODE 1
#define RK32_CRU_MODE_CONTROL_DEEP_SLOW_MODE 2

//
// Define the CRU mode control register bits.
//

#define RK32_CRU_MODE_CONTROL_PROTECT_SHIFT          16
#define RK32_CRU_MODE_CONTROL_NEW_PLL_MODE_MASK      (0x3 << 14)
#define RK32_CRU_MODE_CONTROL_NEW_PLL_MODE_SHIFT     14
#define RK32_CRU_MODE_CONTROL_GENERAL_PLL_MODE_MASK  (0x3 << 12)
#define RK32_CRU_MODE_CONTROL_GENERAL_PLL_MODE_SHIFT 12
#define RK32_CRU_MODE_CONTROL_CODEC_PLL_MODE_MASK    (0x3 << 8)
#define RK32_CRU_MODE_CONTROL_CODEC_PLL_MODE_SHIFT   8
#define RK32_CRU_MODE_CONTROL_DDR_PLL_MODE_MASK      (0x3 << 4)
#define RK32_CRU_MODE_CONTROL_DDR_PLL_MODE_SHIFT     4
#define RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_SLOW      (0x0 << 0)
#define RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_NORMAL    (0x1 << 0)
#define RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_DEEP_SLOW (0x2 << 0)
#define RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_MASK      (0x3 << 0)
#define RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_SHIFT     0

//
// Define the CRU clock select 11 register bits.
//

#define RK32_CRU_CLOCK_SELECT11_PROTECT_SHIFT           16
#define RK32_CRU_CLOCK_SELECT11_HSIC_PHY_DIVIDER_MASK   (0x3F << 8)
#define RK32_CRU_CLOCK_SELECT11_HSIC_PHY_DIVIDER_SHIFT  8
#define RK32_CRU_CLOCK_SELECT11_MMC0_CODEC_PLL          0
#define RK32_CRU_CLOCK_SELECT11_MMC0_GENERAL_PLL        1
#define RK32_CRU_CLOCK_SELECT11_MMC0_24MHZ              2
#define RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_MASK         (0x3 << 6)
#define RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_SHIFT        6
#define RK32_CRU_CLOCK_SELECT11_MMC0_DIVIDER_MASK       (0x3F << 0)
#define RK32_CRU_CLOCK_SELECT11_MMC0_DIVIDER_SHIFT      0

//
// Define CRU global reset values.
//

#define RK32_GLOBAL_RESET1_VALUE 0x0000FDB9
#define RK32_GLOBAL_RESET2_VALUE 0x0000ECA8

//
// Define CRU soft reset 0 register bits.
//

#define RK32_CRU_SOFT_RESET0_PROTECT_SHIFT 16
#define RK32_CRU_SOFT_RESET0_CORE0 0x00000001
#define RK32_CRU_SOFT_RESET0_CORE1 0x00000002
#define RK32_CRU_SOFT_RESET0_CORE2 0x00000004
#define RK32_CRU_SOFT_RESET0_CORE3 0x00000008

//
// Define CRU soft reset 8 register bits.
//

#define RK32_CRU_SOFT_RESET8_PROTECT_SHIFT 16
#define RK32_CRU_SOFT_RESET8_MMC0 0x00000001

//
// Define PMU power down control register bits.
//

#define RK32_PMU_POWER_DOWN_CONTROL_A17_0 0x00000001
#define RK32_PMU_POWER_DOWN_CONTROL_A17_1 0x00000002
#define RK32_PMU_POWER_DOWN_CONTROL_A17_2 0x00000004
#define RK32_PMU_POWER_DOWN_CONTROL_A17_3 0x00000008

//
// Define PMU power down status register bits.
//

#define RK32_PMU_POWER_DOWN_STATUS_A17_0 0x00000001
#define RK32_PMU_POWER_DOWN_STATUS_A17_1 0x00000002
#define RK32_PMU_POWER_DOWN_STATUS_A17_2 0x00000004
#define RK32_PMU_POWER_DOWN_STATUS_A17_3 0x00000008

//
// Define GRF I/O Vsel register bits.
//

#define RK32_GRF_IO_VSEL_LCD_V18 0x00000001
#define RK32_GRF_IO_VSEL_PROTECT_SHIFT 16

//
// Define GPIO SoC status 1 register bits.
//

#define RK32_GRF_SOC_STATUS1_ARM_PLL_LOCK (1 << 6)

//
// Define the GRF GPIO6C IOMUX value for SD/MMC.
//

#define RK32_GRF_GPIO6C_IOMUX_VALUE 0x2AAA1555

//
// Define the GRF GPIO7CH IOMUX initialization values.
//

#define RK32_GRF_GPIO7CH_IOMUX_VALUE 0x33001100

//
// Define LCD system control register bits.
//

#define RK32_LCD_SYSTEM_CONTROL_AUTO_GATING (1 << 23)
#define RK32_LCD_SYSTEM_CONTROL_STANDBY     (1 << 22)
#define RK32_LCD_SYSTEM_CONTROL_DMA_STOP    (1 << 21)
#define RK32_LCD_SYSTEM_CONTROL_MMU_ENABLE  (1 << 20)
#define RK32_LCD_SYSTEM_CONTROL_MIPI_OUT    (1 << 15)
#define RK32_LCD_SYSTEM_CONTROL_EDP_OUT     (1 << 14)
#define RK32_LCD_SYSTEM_CONTROL_HDMI_OUT    (1 << 13)
#define RK32_LCD_SYSTEM_CONTROL_RGB_OUT     (1 << 12)

//
// Define LCD DSP Control 0 register bits.
//

#define RK32_LCD_DSP_CONTROL0_BLACK            (1 << 19)
#define RK32_LCD_DSP_CONTROL0_BLANK            (1 << 18)
#define RK32_LCD_DSP_CONTROL0_OUT_ZERO         (1 << 17)
#define RK32_LCD_DSP_CONTROL0_DCLOCK_POLARITY  (1 << 7)
#define RK32_LCD_DSP_CONTROL0_DENABLE_POLARITY (1 << 6)
#define RK32_LCD_DSP_CONTROL0_VSYNC_POLARITY   (1 << 5)
#define RK32_LCD_DSP_CONTROL0_HSYNC_POLARITY   (1 << 4)

//
// Define LCD DSP control 1 register bits.
//

#define RK32_LCD_DSP_CONTROL1_LAYER3_SEL_SHIFT   14
#define RK32_LCD_DSP_CONTROL1_LAYER2_SEL_SHIFT   12
#define RK32_LCD_DSP_CONTROL1_LAYER1_SEL_SHIFT   10
#define RK32_LCD_DSP_CONTROL1_LAYER0_SEL_SHIFT   8
#define RK32_LCD_DSP_CONTROL1_DITHER_UP          (1 << 6)
#define RK32_LCD_DSP_CONTROL1_DITHER_DOWN_SELECT (1 << 4)
#define RK32_LCD_DSP_CONTROL1_DITHER_DOWN_MODE   (1 << 3)
#define RK32_LCD_DSP_CONTROL1_DITHER_DOWN        (1 << 2)
#define RK32_LCD_DSP_CONTROL1_PRE_DITHER_DOWN    (1 << 1)

//
// Define the LCD display information register bits.
//

#define RK32_LCD_DSP_INFORMATION_HEIGHT_MASK  (0xFFF << 16)
#define RK32_LCD_DSP_INFORMATION_HEIGHT_SHIFT 16
#define RK32_LCD_DSP_INFORMATION_WIDTH_MASK   (0xFFF << 0)
#define RK32_LCD_DSP_INFORMATION_WIDTH_SHIFT  0

//
// Define the UART parameters.
//

#define RK32_UART_BASE_BAUD 1497600
#define RK32_UART_REGISTER_OFFSET 0
#define RK32_UART_REGISTER_SHIFT 2

//
// Define the default frequency for the SD/MMC.
//

#define RK32_SDMMC_FREQUENCY_24MHZ 24000000

//
// Define the bits for the GPIO 7 data register.
//

#define RK32_GPIO7_BACKLIGHT_ENABLE 0x00000001
#define RK32_GPIO7_LCD_BACKLIGHT    0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RK32_CRU_REGISTER {
    Rk32CruArmPllConfiguration0 = 0x00,
    Rk32CruArmPllConfiguration1 = 0x04,
    Rk32CruArmPllConfiguration2 = 0x08,
    Rk32CruArmPllConfiguration3 = 0x0C,
    Rk32CruCodecPllControl0 = 0x20,
    Rk32CruCodecPllControl1 = 0x24,
    Rk32CruCodecPllControl2 = 0x28,
    Rk32CruCodecPllControl3 = 0x2C,
    Rk32CruGeneralPllControl0 = 0x30,
    Rk32CruGeneralPllControl1 = 0x34,
    Rk32CruGeneralPllControl2 = 0x38,
    Rk32CruGeneralPllControl3 = 0x3C,
    Rk32CruModeControl = 0x50,
    Rk32CruClockSelect0 = 0x60,
    Rk32CruClockSelect11 = 0x8C,
    Rk32CruGlobalReset1 = 0x1B0,
    Rk32CruGlobalReset2 = 0x1B4,
    Rk32CruSoftReset0 = 0x1B8,
    Rk32CruSoftReset1 = 0x1BC,
    Rk32CruSoftReset2 = 0x1C0,
    Rk32CruSoftReset3 = 0x1C4,
    Rk32CruSoftReset4 = 0x1C8,
    Rk32CruSoftReset5 = 0x1CC,
    Rk32CruSoftReset6 = 0x1D0,
    Rk32CruSoftReset7 = 0x1D4,
    Rk32CruSoftReset8 = 0x1D8,
    Rk32CruSoftReset9 = 0x1DC,
    Rk32CruSoftReset10 = 0x1E0,
    Rk32CruSoftReset11 = 0x1E4,
} RK32_CRU_REGISTER, *PRK32_CRU_REGISTER;

typedef enum _RK32_PMU_REGISTER {
    Rk32PmuPowerDownControl = 0x08,
    Rk32PmuPowerDownStatus = 0x0C
} RK32_PMU_REGISTER, *PRK32_PMU_REGISTER;

typedef enum _RK32_GRF_REGISTER {
    Rk32GrfGpio6cIomux = 0x64,
    Rk32GrfGpio7chIomux = 0x78,
    Rk32GrfSocStatus0 = 0x280,
    Rk32GrfSocStatus1 = 0x284,
    Rk32GrfIoVsel = 0x380,
} RK32_GRF_REGISTER, *PRK32_GRF_REGISTER;

typedef enum _RK32_LCD_REGISTER {
    Rk32LcdConfigurationDone = 0x00,
    Rk32LcdSystemControl = 0x08,
    Rk32LcdSystemControl1 = 0x0C,
    Rk32LcdDspControl0 = 0x10,
    Rk32LcdDspControl1 = 0x14,
    Rk32LcdBackground = 0x18,
    Rk32LcdMcuControl = 0x1C,
    Rk32LcdInterruptControl0 = 0x20,
    Rk32LcdInterruptControl1 = 0x24,
    Rk32LcdWin0YrgbFrameBufferBase = 0x40,
    Rk32LcdWin0ActiveInformation = 0x48,
    Rk32LcdWin0DisplayInformation = 0x4C,
} RK32_LCD_REGISTER, *PRK32_LCD_REGISTER;

typedef enum _RK32_GPIO_REGISTER {
    Rk32GpioPortAData = 0x00,
    Rk32GpioPortADirection = 0x04,
    Rk32GpioInterruptEnable = 0x30,
    Rk32GpioInterrupMask = 0x34,
    Rk32GpioInterruptLevel = 0x38,
    Rk32GpioInterruptPolarity = 0x3C,
    Rk32GpioInterruptStatus = 0x40,
    Rk32GpioRawInterruptStatus = 0x44,
    Rk32GpioDebounce = 0x48,
    Rk32GpioClearInterrupt = 0x4C,
    Rk32GpioPortAExternal = 0x50,
    Rk32GpioLevelSensitiveSync = 0x60,
} RK32_GPIO_REGISTER, *PRK32_GPIO_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

