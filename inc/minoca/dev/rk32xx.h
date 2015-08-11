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
// Define the SD control register bits.
//

#define RK32_SD_CONTROL_USE_INTERNAL_DMAC       (1 << 25)
#define RK32_SD_CONTROL_ENABLE_OD_PULLUP        (1 << 24)
#define RK32_SD_CONTROL_CARD_VOLTAGE_B_MASK     (0xF << 20)
#define RK32_SD_CONTROL_CARD_VOLTAGE_B_SHIFT    20
#define RK32_SD_CONTROL_CARD_VOLTAGE_A_MASK     (0xF << 16)
#define RK32_SD_CONTROL_CARD_VOLTAGE_A_SHIFT    16
#define RK32_SD_CONTROL_CE_ATA_INTERRUPT_ENABLE (1 << 11)
#define RK32_SD_CONTROL_SEND_AUTO_STOP_CCSD     (1 << 10)
#define RK32_SD_CONTROL_SEND_CCSD               (1 << 9)
#define RK32_SD_CONTROL_ABORT_READ_DATA         (1 << 8)
#define RK32_SD_CONTROL_SEND_IRQ_RESPONSE       (1 << 7)
#define RK32_SD_CONTROL_READ_WAIT               (1 << 6)
#define RK32_SD_CONTROL_DMA_ENABLE              (1 << 5)
#define RK32_SD_CONTROL_INTERRUPT_ENABLE        (1 << 4)
#define RK32_SD_CONTROL_DMA_RESET               (1 << 2)
#define RK32_SD_CONTROL_FIFO_RESET              (1 << 1)
#define RK32_SD_CONTROL_CONTROLLER_RESET        (1 << 0)

//
// Define the SD power register bits.
//

#define RK32_SD_POWER_DISABLE (0 << 0)
#define RK32_SD_POWER_ENABLE  (1 << 0)

//
// Define the SD clock divider register bits.
//

#define RK32_SD_CLOCK_DIVIDER_3_MASK  (0xFF << 24)
#define RK32_SD_CLOCK_DIVIDER_3_SHIFT 24
#define RK32_SD_CLOCK_DIVIDER_2_MASK  (0xFF << 16)
#define RK32_SD_CLOCK_DIVIDER_2_SHIFT 16
#define RK32_SD_CLOCK_DIVIDER_1_MASK  (0xFF << 8)
#define RK32_SD_CLOCK_DIVIDER_1_SHIFT 8
#define RK32_SD_CLOCK_DIVIDER_0_MASK  (0xFF << 0)
#define RK32_SD_CLOCK_DIVIDER_0_SHIFT 0

#define RK32_SD_MAX_DIVISOR (0xFF * 2)

//
// Define the SD clock source register bits.
//

#define RK32_SD_CLOCK_SOURCE_DIVIDER_3     0x3
#define RK32_SD_CLOCK_SOURCE_DIVIDER_2     0x2
#define RK32_SD_CLOCK_SOURCE_DIVIDER_1     0x1
#define RK32_SD_CLOCK_SOURCE_DIVIDER_0     0x0
#define RK32_SD_CLOCK_SOURCE_DIVIDER_MASK  (0x3 << 0)
#define RK32_SD_CLOCK_SOURCE_DIVIDER_SHIFT 0

//
// Define the SD clock enable register bits.
//

#define RK32_SD_CLOCK_ENABLE_LOW_POWER (1 << 16)
#define RK32_SD_CLOCK_ENABLE_ON        (1 << 0)

//
// Define the SD clock timeout register bits.
//

#define RK32_SD_TIMEOUT_DATA_MASK      (0xFFFFFF << 8)
#define RK32_SD_TIMEOUT_DATA_SHIFT     8
#define RK32_SD_TIMEOUT_RESPONSE_MASK  (0xFF << 0)
#define RK32_SD_TIMEOUT_RESPONSE_SHIFT 0

#define RK32_SD_TIMEOUT_DEFAULT 0xFFFFFF40

//
// Define the SD card type register bits.
//

#define RK32_SD_CARD_TYPE_8_BIT_WIDTH (1 << 16)
#define RK32_SD_CARD_TYPE_4_BIT_WIDTH (1 << 0)
#define RK32_SD_CARD_TYPE_1_BIT_WIDTH (0 << 0)

//
// Define the SD block size register bits.
//

#define RK32_SD_BLOCK_SIZE_MASK  (0xFFFF << 0)
#define RK32_SD_BLOCK_SIZE_SHIFT 0

#define RK32_SD_BLOCK_SIZE_MAX 0xFFFF

//
// Define the SD interrupt mask register bits.
//

#define RK32_SD_INTERRUPT_MASK_SDIO                       (1 << 24)
#define RK32_SD_INTERRUPT_MASK_DATA_NO_BUSY               (1 << 16)
#define RK32_SD_INTERRUPT_MASK_ERROR_END_BIT              (1 << 15)
#define RK32_SD_INTERRUPT_MASK_AUTO_COMMAND_DONE          (1 << 14)
#define RK32_SD_INTERRUPT_MASK_ERROR_START_BIT            (1 << 13)
#define RK32_SD_INTERRUPT_MASK_ERROR_HARDWARE_LOCKED      (1 << 12)
#define RK32_SD_INTERRUPT_MASK_ERROR_FIFO_UNDERRUN        (1 << 11)
#define RK32_SD_INTERRUPT_MASK_ERROR_HOST_TIMEOUT         (1 << 10)
#define RK32_SD_INTERRUPT_MASK_ERROR_DATA_READ_TIMEOUT    (1 << 9)
#define RK32_SD_INTERRUPT_MASK_ERROR_RESPONSE_TIMEOUT     (1 << 8)
#define RK32_SD_INTERRUPT_MASK_ERROR_DATA_CRC             (1 << 7)
#define RK32_SD_INTERRUPT_MASK_ERROR_RESPONSE_CRC         (1 << 6)
#define RK32_SD_INTERRUPT_MASK_RECEIVE_FIFO_DATA_REQUEST  (1 << 5)
#define RK32_SD_INTERRUPT_MASK_TRANSMIT_FIFO_DATA_REQUEST (1 << 4)
#define RK32_SD_INTERRUPT_MASK_DATA_TRANSFER_OVER         (1 << 3)
#define RK32_SD_INTERRUPT_MASK_COMMAND_DONE               (1 << 2)
#define RK32_SD_INTERRUPT_MASK_ERROR_RESPONSE             (1 << 1)
#define RK32_SD_INTERRUPT_MASK_CARD_DETECT                (1 << 0)

#define RK32_SD_INTERRUPT_ERROR_MASK                     \
    (RK32_SD_INTERRUPT_MASK_ERROR_END_BIT |              \
     RK32_SD_INTERRUPT_MASK_ERROR_START_BIT |            \
     RK32_SD_INTERRUPT_MASK_ERROR_DATA_READ_TIMEOUT |    \
     RK32_SD_INTERRUPT_MASK_ERROR_RESPONSE_TIMEOUT |     \
     RK32_SD_INTERRUPT_MASK_ERROR_DATA_CRC |             \
     RK32_SD_INTERRUPT_MASK_ERROR_RESPONSE_CRC |         \
     RK32_SD_INTERRUPT_MASK_ERROR_RESPONSE)

#define RK32_SD_INTERRUPT_DEFAULT_MASK RK32_SD_INTERRUPT_MASK_CARD_DETECT

//
// Define the SD interrupt status register bits.
//

#define RK32_SD_INTERRUPT_STATUS_SDIO                       (1 << 24)
#define RK32_SD_INTERRUPT_STATUS_DATA_NO_BUSY_DISABLE       (1 << 16)
#define RK32_SD_INTERRUPT_STATUS_ERROR_END_BIT              (1 << 15)
#define RK32_SD_INTERRUPT_STATUS_AUTO_COMMAND_DONE          (1 << 14)
#define RK32_SD_INTERRUPT_STATUS_ERROR_START_BIT            (1 << 13)
#define RK32_SD_INTERRUPT_STATUS_ERROR_HARDWARE_LOCKED      (1 << 12)
#define RK32_SD_INTERRUPT_STATUS_ERROR_FIFO_UNDERRUN        (1 << 11)
#define RK32_SD_INTERRUPT_STATUS_ERROR_HOST_TIMEOUT         (1 << 10)
#define RK32_SD_INTERRUPT_STATUS_ERROR_DATA_READ_TIMEOUT    (1 << 9)
#define RK32_SD_INTERRUPT_STATUS_ERROR_RESPONSE_TIMEOUT     (1 << 8)
#define RK32_SD_INTERRUPT_STATUS_ERROR_DATA_CRC             (1 << 7)
#define RK32_SD_INTERRUPT_STATUS_ERROR_RESPONSE_CRC         (1 << 6)
#define RK32_SD_INTERRUPT_STATUS_RECEIVE_FIFO_DATA_REQUEST  (1 << 5)
#define RK32_SD_INTERRUPT_STATUS_TRANSMIT_FIFO_DATA_REQUEST (1 << 4)
#define RK32_SD_INTERRUPT_STATUS_DATA_TRANSFER_OVER         (1 << 3)
#define RK32_SD_INTERRUPT_STATUS_COMMAND_DONE               (1 << 2)
#define RK32_SD_INTERRUPT_STATUS_ERROR_RESPONSE             (1 << 1)
#define RK32_SD_INTERRUPT_STATUS_CARD_DETECT                (1 << 0)
#define RK32_SD_INTERRUPT_STATUS_ALL_MASK                   0xFFFFFFFF

#define RK32_SD_INTERRUPT_STATUS_COMMAND_ERROR_MASK \
    (RK32_SD_INTERRUPT_STATUS_ERROR_RESPONSE |      \
     RK32_SD_INTERRUPT_STATUS_ERROR_RESPONSE_CRC)

#define RK32_SD_INTERRUPT_STATUS_DATA_ERROR_MASK        \
    (RK32_SD_INTERRUPT_STATUS_ERROR_DATA_CRC |          \
     RK32_SD_INTERRUPT_STATUS_ERROR_DATA_READ_TIMEOUT | \
     RK32_SD_INTERRUPT_STATUS_ERROR_HOST_TIMEOUT |      \
     RK32_SD_INTERRUPT_STATUS_ERROR_START_BIT |         \
     RK32_SD_INTERRUPT_STATUS_ERROR_END_BIT)

//
// Define the SD command register bits.
//

#define RK32_SD_COMMAND_START                       (1 << 31)
#define RK32_SD_COMMAND_USE_HOLD_REGISTER           (1 << 29)
#define RK32_SD_COMMAND_VOLT_SWITCH                 (1 << 28)
#define RK32_SD_COMMAND_BOOT_MODE                   (1 << 27)
#define RK32_SD_COMMAND_DISABLE_BOOT                (1 << 26)
#define RK32_SD_COMMAND_EXPECT_BOOT_ACK             (1 << 25)
#define RK32_SD_COMMAND_ENABLE_BOOT                 (1 << 24)
#define RK32_SD_COMMAND_CSS_EXPECTED                (1 << 23)
#define RK32_SD_COMMAND_READ_CE_ATA                 (1 << 22)
#define RK32_SD_COMMAND_UPDATE_CLOCK_REGISTERS      (1 << 21)
#define RK32_SD_COMMAND_CARD_NUMBER_MASK            (0x1F << 16)
#define RK32_SD_COMMAND_CARD_NUMBER_SHIFT           16
#define RK32_SD_COMMAND_SEND_INITIALIZATION         (1 << 15)
#define RK32_SD_COMMAND_STOP_ABORT                  (1 << 14)
#define RK32_SD_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE (1 << 13)
#define RK32_SD_COMMAND_SEND_AUTO_STOP              (1 << 12)
#define RK32_SD_COMMAND_TRANSFER_MODE_BLOCK         (0 << 11)
#define RK32_SD_COMMAND_TRANSFER_MODE_STREAM        (1 << 11)
#define RK32_SD_COMMAND_READ                        (0 << 10)
#define RK32_SD_COMMAND_WRITE                       (1 << 10)
#define RK32_SD_COMMAND_DATA_EXPECTED               (1 << 9)
#define RK32_SD_COMMAND_CHECK_RESPONSE_CRC          (1 << 8)
#define RK32_SD_COMMAND_LONG_RESPONSE               (1 << 7)
#define RK32_SD_COMMAND_RESPONSE_EXPECTED           (1 << 6)
#define RK32_SD_COMMAND_INDEX_MASK                  (0x3F << 0)
#define RK32_SD_COMMAND_INDEX_SHIFT                 0

//
// Define the SD status register bits.
//

#define RK32_SD_STATUS_DMA_REQUEST              (1 << 31)
#define RK32_SD_STATUS_DMA_ACK                  (1 << 30)
#define RK32_SD_STATUS_FIFO_COUNT_MASK          (0x1FFF << 17)
#define RK32_SD_STATUS_FIFO_COUNT_SHIFT         17
#define RK32_SD_STATUS_RESPONSE_INDEX_MASK      (0x3F << 11)
#define RK32_SD_STATUS_RESPONSE_INDEX_SHIFT     11
#define RK32_SD_STATUS_DATA_STATE_MACHINE_BUSY  (1 << 10)
#define RK32_SD_STATUS_DATA_BUSY                (1 << 9)
#define RK32_SD_STATUS_DATA_3_STATUS            (1 << 8)
#define RK32_SD_STATUS_COMMAND_FSM_STATE_MASK   (0xF << 4)
#define RK32_SD_STATUS_COMMAND_FSM_STATE_SHIFT  4
#define RK32_SD_STATUS_FIFO_FULL                (1 << 3)
#define RK32_SD_STATUS_FIFO_EMPTY               (1 << 2)
#define RK32_SD_STATUS_FIFO_TRANSMIT_WATERMARK  (1 << 1)
#define RK32_SD_STATUS_FIFO_RECEIVE_WATERMARK   (1 << 0)

//
// Define the SD FIFO threshold register bits.
//

#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_1     0
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_4     1
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_8     2
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_16    3
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_32    4
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_64    5
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_128   6
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_256   7
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_MASK  (0x7 << 28)
#define RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_SHIFT 28
#define RK32_SD_FIFO_THRESHOLD_RECEIVE_WATERMARK_MASK              (0xFFF << 16)
#define RK32_SD_FIFO_THRESHOLD_RECEIVE_WATERMARK_SHIFT             16
#define RK32_SD_FIFO_THRESHOLD_TRANSMIT_WATERMARK_MASK             (0xFFF << 0)
#define RK32_SD_FIFO_THRESHOLD_TRANSMIT_WATERMARK_SHIFT            0

#define RK32_SD_FIFO_THRESHOLD_DEFAULT                              \
    ((RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_16 <<    \
      RK32_SD_FIFO_THRESHOLD_DMA_MULTIPLE_TRANSACTION_SIZE_SHIFT) | \
      ((((RK32_SD_FIFO_DEPTH / 2) - 1) <<                           \
        RK32_SD_FIFO_THRESHOLD_RECEIVE_WATERMARK_SHIFT) &           \
       RK32_SD_FIFO_THRESHOLD_RECEIVE_WATERMARK_MASK) |             \
      (((RK32_SD_FIFO_DEPTH / 2) <<                                 \
        RK32_SD_FIFO_THRESHOLD_RECEIVE_WATERMARK_SHIFT) &           \
       RK32_SD_FIFO_THRESHOLD_RECEIVE_WATERMARK_MASK))

#define RK32_SD_FIFO_DEPTH 0x100

//
// Define the SD UHS register bits.
//

#define RK32_SD_UHS_DDR_MODE     (1 << 16)
#define RK32_SD_UHS_VOLTAGE_MASK (1 << 0)
#define RK32_SD_UHS_VOLTAGE_3V3  (0 << 0)
#define RK32_SD_UHS_VOLTAGE_1V8  (1 << 0)

//
// Define the SD reset register bits.
//

#define RK32_SD_RESET_ENABLE (1 << 0)

//
// Define the SD bus mode register bits.
//

#define RK32_SD_BUS_MODE_BURST_LENGTH_1               0
#define RK32_SD_BUS_MODE_BURST_LENGTH_4               1
#define RK32_SD_BUS_MODE_BURST_LENGTH_8               2
#define RK32_SD_BUS_MODE_BURST_LENGTH_16              3
#define RK32_SD_BUS_MODE_BURST_LENGTH_32              4
#define RK32_SD_BUS_MODE_BURST_LENGTH_64              5
#define RK32_SD_BUS_MODE_BURST_LENGTH_128             6
#define RK32_SD_BUS_MODE_BURST_LENGTH_256             7
#define RK32_SD_BUS_MODE_BURST_LENGTH_MASK            (0x7 << 8)
#define RK32_SD_BUS_MODE_BURST_LENGTH_SHIFT           8
#define RK32_SD_BUS_MODE_IDMAC_ENABLE                 (1 << 7)
#define RK32_SD_BUS_MODE_DESCRIPTOR_SKIP_LENGTH_MASK  (0x1F << 2)
#define RK32_SD_BUS_MODE_DESCRIPTOR_SKIP_LENGTH_SHIFT 2
#define RK32_SD_BUS_MODE_FIXED_BURST                  (1 << 1)
#define RK32_SD_BUS_MODE_SOFTWARE_RESET               (1 << 0)

//
// Define the DMA descriptor control and status bits.
//

#define RK32_SD_DMA_DESCRIPTOR_CONTROL_OWN                             (1 << 31)
#define RK32_SD_DMA_DESCRIPTOR_CONTROL_CARD_ERROR_SUMMARY              (1 << 30)
#define RK32_SD_DMA_DESCRIPTOR_CONTROL_END_OF_RING                     (1 << 5)
#define RK32_SD_DMA_DESCRIPTOR_CONTROL_SECOND_ADDRESS_CHAINED          (1 << 4)
#define RK32_SD_DMA_DESCRIPTOR_CONTROL_FIRST_DESCRIPTOR                (1 << 3)
#define RK32_SD_DMA_DESCRIPTOR_CONTROL_LAST_DESCRIPTOR                 (1 << 2)
#define RK32_SD_DMA_DESCRIPTOR_CONTROL_DISABLE_INTERRUPT_ON_COMPLETION (1 << 1)

//
// Define the maximum buffer size for a DMA descriptor. Technically it is
// 0x1FFF, but round down to the nearest page for better arithmetic.
//

#define RK32_SD_DMA_DESCRIPTOR_MAX_BUFFER_SIZE 0x1000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RK32_CRU_REGISTER {
    Rk32CruCodecPllControl0 = 0x20,
    Rk32CruCodecPllControl1 = 0x24,
    Rk32CruCodecPllControl2 = 0x28,
    Rk32CruCodecPllControl3 = 0x2C,
    Rk32CruGeneralPllControl0 = 0x30,
    Rk32CruGeneralPllControl1 = 0x34,
    Rk32CruGeneralPllControl2 = 0x38,
    Rk32CruGeneralPllControl3 = 0x3C,
    Rk32CruModeControl = 0x50,
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

typedef enum _RK32_GRF_REGISTER {
    Rk32GrfGpio6cIomux = 0x64,
    Rk32GrfGpio7chIomux = 0x78,
    Rk32GrfSocStatus0 = 0x280,
    Rk32GrfSocStatus1 = 0x284,
    Rk32GrfIoVsel = 0x380,
} RK32_GRF_REGISTER, *PRK32_GRF_REGISTER;

typedef enum _RK32_SD_REGISTER {
    Rk32SdControl = 0x000,
    Rk32SdPower = 0x004,
    Rk32SdClockDivider = 0x008,
    Rk32SdClockSource = 0x00C,
    Rk32SdClockEnable = 0x010,
    Rk32SdTimeout = 0x014,
    Rk32SdCardType = 0x018,
    Rk32SdBlockSize = 0x01C,
    Rk32SdByteCount = 0x020,
    Rk32SdInterruptMask = 0x024,
    Rk32SdCommandArgument = 0x028,
    Rk32SdCommand = 0x02C,
    Rk32SdResponse0 = 0x030,
    Rk32SdResponse1 = 0x034,
    Rk32SdResponse2 = 0x038,
    Rk32SdResponse3 = 0x03C,
    Rk32SdMaskedInterruptStatus = 0x040,
    Rk32SdInterruptStatus = 0x044,
    Rk32SdStatus = 0x048,
    Rk32SdFifoThreshold = 0x04C,
    Rk32SdCardDetect = 0x050,
    Rk32SdWriteProtect = 0x054,
    Rk32SdTransferredCiuByteCount = 0x058,
    Rk32SdTransferredBiuByteCount = 0x05C,
    Rk32SdUhs = 0x074,
    Rk32SdResetN = 0x078,
    Rk32SdBusMode = 0x080,
    Rk32SdDescriptorBaseAddress = 0x088,
    Rk32SdFifoBase = 0x200,
} RK32_SD_REGISTER, *PRK32_SD_REGISTER;

/*++

Structure Description:

    This structure defines the Rockchip RK32xx SD DMA descriptor.

Members:

    Control - Stores control and status bits for the descriptor. See
        RK32_SD_DMA_DESCRIPTOR_CONTROL_* for definitions.

    Size - Stores the size of the buffer.

    Address - Stores the physical address of the data buffer to use for the DMA.

    NextDescriptor - Stores the physical address of the next DMA descriptor.

--*/

typedef struct _RK32_SD_DMA_DESCRIPTOR {
    ULONG Control;
    ULONG Size;
    ULONG Address;
    ULONG NextDescriptor;
} PACKED RK32_SD_DMA_DESCRIPTOR, *PRK32_SD_DMA_DESCRIPTOR;

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

    GrfBase - Stores the physical address of the general register files.

--*/

typedef struct _RK32XX_TABLE {
    DESCRIPTION_HEADER Header;
    ULONGLONG TimerBase[RK32_TIMER_COUNT];
    ULONG TimerGsi[RK32_TIMER_COUNT];
    ULONG TimerCountDownMask;
    ULONG TimerEnabledMask;
    ULONGLONG CruBase;
    ULONGLONG GrfBase;
} PACKED RK32XX_TABLE, *PRK32XX_TABLE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

