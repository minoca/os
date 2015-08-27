/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    rk32xx.h

Abstract:

    This header contains definitions for the Rockchip 32xx SoC.

Author:

    Chris Stevens 30-Jul-2015

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
// Define the signature of the RK32xx ACPI table: Rk32
//

#define RK32XX_SIGNATURE 0x32336B52 // '23kR'

//
// Define the number of timers in the SoC.
//

#define RK32_TIMER_COUNT 8

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
// Define CRU soft reset 8 register bits.
//

#define RK32_CRU_SOFT_RESET8_PROTECT_SHIFT 16
#define RK32_CRU_SOFT_RESET8_MMC0 0x00000001

//
// Define the GRF GPIO6C IOMUX value for SD/MMC.
//

#define RK32_GRF_GPIO6C_IOMUX_VALUE 0x2AAA1555

//
// Define the default frequency for the SD/MMC.
//

#define RK32_SDMMC_FREQUENCY_24MHZ 24000000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RK32_CRU_REGISTER {
    Rk32CruArmPllConfiguration0 = 0x00,
    Rk32CruArmPllConfiguration1 = 0x04,
    Rk32CruArmPllConfiguration2 = 0x08,
    Rk32CruArmPllConfiguration3 = 0x0C,
    Rk32CruDdrPllConfiguration0 = 0x10,
    Rk32CruDdrPllConfiguration1 = 0x14,
    Rk32CruDdrPllConfiguration2 = 0x18,
    Rk32CruDdrPllConfiguration3 = 0x1C,
    Rk32CruCodecPllConfiguration0 = 0x20,
    Rk32CruCodecPllConfiguration1 = 0x24,
    Rk32CruCodecPllConfiguration2 = 0x28,
    Rk32CruCodecPllConfiguration3 = 0x2C,
    Rk32CruGeneralPllConfiguration0 = 0x30,
    Rk32CruGeneralPllConfiguration1 = 0x34,
    Rk32CruGeneralPllConfiguration2 = 0x38,
    Rk32CruGeneralPllConfiguration3 = 0x3C,
    Rk32CruNewPllConfiguration0 = 0x40,
    Rk32CruNewPllConfiguration1 = 0x44,
    Rk32CruNewPllConfiguration2 = 0x48,
    Rk32CruNewPllConfiguration3 = 0x4C,
    Rk32CruModeControl = 0x50,
    Rk32CruClockSelect0 = 0x60,
    Rk32CruClockSelect1 = 0x64,
    Rk32CruClockSelect11 = 0x8C,
    Rk32CruClockSelect33 = 0xE4,
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

typedef enum _RK32_GRF_REGISTER {
    Rk32GrfGpio6cIomux = 0x64,
    Rk32GrfGpio7chIomux = 0x78,
    Rk32GrfSocStatus0 = 0x280,
    Rk32GrfSocStatus1 = 0x284,
    Rk32GrfIoVsel = 0x380,
} RK32_GRF_REGISTER, *PRK32_GRF_REGISTER;

typedef enum _RK32_GPIO_REGISTER {
    Rk32GpioPortAData = 0x00,
    Rk32GpioPortADirection = 0x04,
    Rk32GpioInterruptEnable = 0x30,
    Rk32GpioInterruptMask = 0x34,
    Rk32GpioInterruptLevel = 0x38,
    Rk32GpioInterruptPolarity = 0x3C,
    Rk32GpioInterruptStatus = 0x40,
    Rk32GpioRawInterruptStatus = 0x44,
    Rk32GpioDebounce = 0x48,
    Rk32GpioClearInterrupt = 0x4C,
    Rk32GpioPortAExternal = 0x50,
    Rk32GpioLevelSensitiveSync = 0x60,
} RK32_GPIO_REGISTER, *PRK32_GPIO_REGISTER;

/*++

Structure Description:

    This structure describes the Rockchip RK32xx ACPI table.

Members:

    Header - Stores the standard ACPI table header. The signature here is
        'Rk32'.

    TimerBase - Stores the array of physical addresses of all the timers.

    TimerGsi - Stores the array of Global System Interrupt numbers for each of
        the timers.

    TimerCountDownMask - Stores a mask of bits, one for each timer, where if a
        bit is set that timer counts down. If the bit for a timer is clear, the
        timer counts up.

    TimerEnabledMask - Stores a bitfield of which timers are available for use
        by the kernel.

    CruBase - Stores the physical address of the clock and reset unit.

--*/

typedef struct _RK32XX_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG TimerBase[RK32_TIMER_COUNT];
    ULONG TimerGsi[RK32_TIMER_COUNT];
    ULONG TimerCountDownMask;
    ULONG TimerEnabledMask;
    ULONGLONG CruBase;
} PACKED RK32XX_TABLE, *PRK32XX_TABLE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

