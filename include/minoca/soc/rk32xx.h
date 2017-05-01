/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
// Define the number of timers in the SoC.
//

#define RK32_TIMER_COUNT 8

//
// Define the RK3288 register base map.
//

#define RK32_SD_BASE 0xFF0C0000
#define RK32_EMMC_BASE 0xFF0F0000
#define RK32_I2C_TP_BASE 0xFF160000
#define RK32_I2C_PMU_BASE 0xFF650000
#define RK32_TIMER0_5_BASE 0xFF6B0000
#define RK32_UART_DEBUG_BASE 0xFF690000
#define RK32_SRAM_BASE 0xFF700000
#define RK32_PMU_BASE 0xFF730000
#define RK32_GPIO0_BASE 0xFF750000
#define RK32_CRU_BASE 0xFF760000
#define RK32_GRF_BASE 0xFF770000
#define RK32_GPIO7_BASE 0xFF7E0000
#define RK32_WATCHDOG_BASE 0xFF800000
#define RK32_TIMER6_7_BASE 0xFF810000
#define RK32_VOP_BIG_BASE 0xFF930000
#define RK32_VOP_LITTLE_BASE 0xFF940000
#define RK32_GIC_DISTRIBUTOR_BASE 0xFFC01000
#define RK32_GIC_CPU_INTERFACE_BASE 0xFFC02000

#define RK32_I2C_PMU_SIZE 0x1000
#define RK32_GPIO0_SIZE 0x1000

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
// Define generic PLL register bits, organized by configuration register.
//

#define RK32_PLL_CONFIGURATION0_NR_MASK  (0x3F << 8)
#define RK32_PLL_CONFIGURATION0_NR_SHIFT 8
#define RK32_PLL_CONFIGURATION0_OD_MASK  (0xF << 0)
#define RK32_PLL_CONFIGURATION0_OD_SHIFT 0

#define RK32_PLL_CONFIGURATION1_NF_MASK  (0x1FFF << 0)
#define RK32_PLL_CONFIGURATION1_NF_SHIFT 0

#define RK32_PLL_CONFIGURATION2_BWADJ_MASK  (0xFFF << 0)
#define RK32_PLL_CONFIGURATION2_BWADJ_SHIFT 0

#define RK32_PLL_CONFIGURATION3_RESET (1 << 5)

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
// Define the CRU clock select 1 register bits.
//

#define RK32_CRU_CLOCK_SELECT1_PROTECT_SHIFT       16
#define RK32_CRU_CLOCK_SELECT1_GENERAL_PLL         (1 << 15)
#define RK32_CRU_CLOCK_SELECT1_PCLK_DIVIDER_MASK   (0x7 << 12)
#define RK32_CRU_CLOCK_SELECT1_PCLK_DIVIDER_SHIFT  12
#define RK32_CRU_CLOCK_SELECT1_HCLK_DIVIDER_MASK   (0x3 << 8)
#define RK32_CRU_CLOCK_SELECT1_HCLK_DIVIDER_SHIFT  8
#define RK32_CRU_CLOCK_SELECT1_ACLK_DIVIDER_MASK   (0x1F << 3)
#define RK32_CRU_CLOCK_SELECT1_ACLK_DIVIDER_SHIFT  3
#define RK32_CRU_CLOCK_SELECT1_ACLK_DIVIDER1_MASK  (0x7 << 0)
#define RK32_CRU_CLOCK_SELECT1_ACLK_DIVIDER1_SHIFT 0

//
// Define the CRU clock select 10 register bits.
//

#define RK32_CRU_CLOCK_SELECT10_PROTECT_SHIFT       16
#define RK32_CRU_CLOCK_SELECT10_GENERAL_PLL         (1 << 15)
#define RK32_CRU_CLOCK_SELECT10_PCLK_DIVIDER_MASK   (0x3 << 12)
#define RK32_CRU_CLOCK_SELECT10_PCLK_DIVIDER_SHIFT  12
#define RK32_CRU_CLOCK_SELECT10_HCLK_DIVIDER_MASK   (0x3 << 8)
#define RK32_CRU_CLOCK_SELECT10_HCLK_DIVIDER_SHIFT  8
#define RK32_CRU_CLOCK_SELECT10_ACLK_DIVIDER_MASK   (0x1F << 0)
#define RK32_CRU_CLOCK_SELECT10_ACLK_DIVIDER_SHIFT  0

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
// Define CRU clock select 12 register bits.
//

#define RK32_CRU_CLOCK_SELECT12_EMMC_CODEC_PLL          0
#define RK32_CRU_CLOCK_SELECT12_EMMC_GENERAL_PLL        1
#define RK32_CRU_CLOCK_SELECT12_EMMC_24MHZ              2
#define RK32_CRU_CLOCK_SELECT12_EMMC_CLOCK_SHIFT        14
#define RK32_CRU_CLOCK_SELECT12_EMMC_DIVIDER_SHIFT      8
#define RK32_CRU_CLOCK_SELECT12_EMMC_CLOCK_MASK         (0x3 << 14)
#define RK32_CRU_CLOCK_SELECT12_EMMC_DIVIDER_MASK       (0x3F << 8)
#define RK32_CRU_CLOCK_SELECT12_PROTECT_SHIFT           16

//
// Define the CRU clock select 33 register bits.
//

#define RK32_CRU_CLOCK_SELECT33_PROTECT_SHIFT            16
#define RK32_CRU_CLOCK_SELECT33_ALIVE_PCLK_DIVIDER_MASK  (0x1F << 8)
#define RK32_CRU_CLOCK_SELECT33_ALIVE_PCLK_DIVIDER_SHIFT 8
#define RK32_CRU_CLOCK_SELECT33_PMU_PCLK_DIVIDER_MASK    (0x1F << 0)
#define RK32_CRU_CLOCK_SELECT33_PMU_PCLK_DIVIDER_SHIFT   0

//
// Define generic CRU clock select value for SD/eMMC.
//

#define RK32_CRU_MAX_MMC_DIVISOR 0x3F

#define RK32_CRU_CLOCK_SELECT_PROTECT_SHIFT      16
#define RK32_CRU_CLOCK_SELECT_CODEC_PLL          0
#define RK32_CRU_CLOCK_SELECT_GENERAL_PLL        1
#define RK32_CRU_CLOCK_SELECT_24MHZ              2
#define RK32_CRU_CLOCK_SELECT_CLOCK_MASK         (0x3 << 6)
#define RK32_CRU_CLOCK_SELECT_CLOCK_SHIFT        6
#define RK32_CRU_CLOCK_SELECT_DIVIDER_MASK       (0x3F << 0)
#define RK32_CRU_CLOCK_SELECT_DIVIDER_SHIFT      0

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
// Define the default values for the I2C PMU iomux.
//

#define RK32_PMU_IOMUX_GPIO0B_I2C0_SDA (1 << 14)
#define RK32_PMU_IOMUX_GPIO0C_I2C0_SCL (1 << 0)

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
// Define the GRF GPIO7CL IOMUX initialization value.
//

#define RK32_GRF_GPIO7CL_IOMUX_VALUE 0x01100110

//
// Define the GRF GPIO7CH IOMUX initialization values.
//

#define RK32_GRF_GPIO7CH_IOMUX_VALUE 0x33001100

//
// Defien the GRF GPIO7A pull value.
//

#define RK32_GRF_GPIO7A_PULL_VALUE 0x00C00040

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
// Define the bits for the GPIO 7 data register.
//

#define RK32_GPIO7_BACKLIGHT_ENABLE 0x00000001
#define RK32_GPIO7_LCD_BACKLIGHT    0x00000004

//
// Define the bits for the I2C control register.
//

#define RK32_I2C_CONTROL_STOP_ON_NAK           (1 << 6)
#define RK32_I2C_CONTROL_SEND_NAK              (1 << 5)
#define RK32_I2C_CONTROL_STOP                  (1 << 4)
#define RK32_I2C_CONTROL_START                 (1 << 3)
#define RK32_I2C_CONTROL_MODE_TRANSMIT         (0x0 << 1)
#define RK32_I2C_CONTROL_MODE_TRANSMIT_RECEIVE (0x1 << 1)
#define RK32_I2C_CONTROL_MODE_RECEIVE          (0x2 << 1)
#define RK32_I2C_CONTROL_MODE_MASK             (0x3 << 1)
#define RK32_I2C_CONTROL_MODE_SHIFT            1
#define RK32_I2C_CONTROL_ENABLE                (1 << 0)

#define RK32_I2C_BUFFER_SIZE 32

//
// Define the bits for the I2C clock divisor register.
//

#define RK32_I2C_CLOCK_DIVISOR_HIGH_MASK  (0xFFFF << 16)
#define RK32_I2C_CLOCK_DIVISOR_HIGH_SHIFT 16
#define RK32_I2C_CLOCK_DIVISOR_LOW_MASK   (0xFFFF << 0)
#define RK32_I2C_CLOCK_DIVISOR_LOW_SHIFT  0

//
// Define the bits for the I2C master receive slave address register.
//

#define RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_HIGH_BYTE_VALID   (1 << 26)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_MIDDLE_BYTE_VALID (1 << 25)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_LOW_BYTE_VALID    (1 << 24)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_MASK              (0x7FFFFF << 1)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_SHIFT             1
#define RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_WRITE             (0 << 0)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_READ              (1 << 0)

//
// Define the bits for the I2C master receive slave register.
//

#define RK32_I2C_MASTER_RECEIVE_SLAVE_REGISTER_HIGH_BYTE_VALID   (1 << 26)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_REGISTER_MIDDLE_BYTE_VALID (1 << 25)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_REGISTER_LOW_BYTE_VALID    (1 << 24)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_REGISTER_MASK              (0xFFFFFF << 0)
#define RK32_I2C_MASTER_RECEIVE_SLAVE_REGISTER_SHIFT             0

//
// Define the bits for the I2C master transmit count register.
//

#define RK32_I2C_MASTER_TRANSMIT_COUNT_MASK  (0x3F << 0)
#define RK32_I2C_MASTER_TRANSMIT_COUNT_SHIFT 0

//
// Define the bits for the I2C master receive count register.
//

#define RK32_I2C_MASTER_RECEIVE_COUNT_MASK  (0x3F << 0)
#define RK32_I2C_MASTER_RECEIVE_COUNT_SHIFT 0

//
// Define the bits for the I2C interrupt registers.
//

#define RK32_I2C_INTERRUPT_NAK                      (1 << 6)
#define RK32_I2C_INTERRUPT_STOP                     (1 << 5)
#define RK32_I2C_INTERRUPT_START                    (1 << 4)
#define RK32_I2C_INTERRUPT_MASTER_RECEIVE_FINISHED  (1 << 3)
#define RK32_I2C_INTERRUPT_MASTER_TRANSMIT_FINISHED (1 << 2)
#define RK32_I2C_INTERRUPT_BYTE_RECEIVE_FINISHED    (1 << 1)
#define RK32_I2C_INTERRUPT_BYTE_TRANSMIT_FINISHED   (1 << 0)

#define RK32_I2C_INTERRUPT_MASK                    \
    (RK32_I2C_INTERRUPT_NAK |                      \
     RK32_I2C_INTERRUPT_STOP |                     \
     RK32_I2C_INTERRUPT_START |                    \
     RK32_I2C_INTERRUPT_MASTER_RECEIVE_FINISHED |  \
     RK32_I2C_INTERRUPT_MASTER_TRANSMIT_FINISHED | \
     RK32_I2C_INTERRUPT_BYTE_RECEIVE_FINISHED |         \
     RK32_I2C_INTERRUPT_BYTE_TRANSMIT_FINISHED)

//
// Define the bits for the I2C finished count register.
//

#define RK32_I2C_FINISHED_COUNT_MASK  (0x3F << 0)
#define RK32_I2C_FINISHED_COUNT_SHIFT 0

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
// Define attributes of the timers.
//

#define RK32_TIMER_BIT_WIDTH 64
#define RK32_TIMER_BLOCK_SIZE 0x1000

//
// Define RK32 timer register bits.
//

//
// Control bits
//

#define RK32_TIMER_CONTROL_ENABLE           0x00000001
#define RK32_TIMER_CONTROL_ONE_SHOT         0x00000002
#define RK32_TIMER_CONTROL_INTERRUPT_ENABLE 0x00000004

#define RK32_WATCHDOG_CONTROL_ENABLE     0x00000001
#define RK32_WATCHDOG_CONTROL_BARK_FIRST 0x00000002

#define RK32_WATCHDOG_RESTART_VALUE 0x00000076

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
    Rk32CruClockSelect10 = 0x88,
    Rk32CruClockSelect11 = 0x8C,
    Rk32CruClockSelect12 = 0x90,
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

typedef enum _RK32_PLL_TYPE {
    Rk32PllNew,
    Rk32PllGeneral,
    Rk32PllCodec,
    Rk32PllDdr,
    Rk32PllArm
} RK32_PLL_TYPE, *PRK32_PLL_TYPE;

typedef enum _RK32_PMU_REGISTER {
    Rk32PmuPowerDownControl = 0x08,
    Rk32PmuPowerDownStatus = 0x0C,
    Rk32PmuIomuxGpio0A = 0x84,
    Rk32PmuIomuxGpio0B = 0x88,
    Rk32PmuIomuxGpio0C = 0x8C
} RK32_PMU_REGISTER, *PRK32_PMU_REGISTER;

typedef enum _RK32_GRF_REGISTER {
    Rk32GrfGpio6cIomux = 0x064,
    Rk32GrfGpio7clIomux = 0x074,
    Rk32GrfGpio7chIomux = 0x078,
    Rk32GrfGpio7aPull = 0x1A0,
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

typedef enum _RK32_I2C_REGISTER {
    Rk32I2cControl = 0x00,
    Rk32I2cClockDivisor = 0x04,
    Rk32I2cMasterReceiveSlaveAddress = 0x08,
    Rk32I2cMasterReceiveSlaveRegister = 0x0C,
    Rk32I2cMasterTransmitCount = 0x10,
    Rk32I2cMasterReceiveCount = 0x14,
    Rk32I2cInterruptEnable = 0x18,
    Rk32I2cInterruptPending = 0x1C,
    Rk32I2cFinishedCount = 0x20,
    Rk32I2cTransmitData0 = 0x100,
    Rk32I2cTransmitData1 = 0x104,
    Rk32I2cTransmitData2 = 0x108,
    Rk32I2cTransmitData3 = 0x10C,
    Rk32I2cTransmitData4 = 0x110,
    Rk32I2cTransmitData5 = 0x114,
    Rk32I2cTransmitData6 = 0x118,
    Rk32I2cTransmitData7 = 0x11C,
    Rk32I2cReceiveData0 = 0x200,
    Rk32I2cReceiveData1 = 0x204,
    Rk32I2cReceiveData2 = 0x208,
    Rk32I2cReceiveData3 = 0x20C,
    Rk32I2cReceiveData4 = 0x210,
    Rk32I2cReceiveData5 = 0x214,
    Rk32I2cReceiveData6 = 0x218,
    Rk32I2cReceiveData7 = 0x21C
} RK32_I2C_REGISTER, *PRK32_I2C_REGISTER;

typedef enum _RK32_TIMER_REGISTER {
    Rk32TimerLoadCountLow     = 0x00,
    Rk32TimerLoadCountHigh    = 0x04,
    Rk32TimerCurrentValueLow  = 0x08,
    Rk32TimerCurrentValueHigh = 0x0C,
    Rk32TimerControl          = 0x10,
    Rk32TimerInterruptStatus  = 0x18
} RK32_TIMER_REGISTER, *PRK32_TIMER_REGISTER;

typedef enum _RK32_WATCHDOG_REGISTER {
    Rk32WatchdogControl         = 0x00,
    Rk32WatchdogTimeoutRange    = 0x04,
    Rk32WatchdogCurrentCount    = 0x08,
    Rk32WatchdogCounterRestart  = 0x0C,
    Rk32WatchdogInterruptStatus = 0x10,
    Rk32WatchdogInterruptClear  = 0x14
} RK32_WATCHDOG_REGISTER, *PRK32_WATCHDOG_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

