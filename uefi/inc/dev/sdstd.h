/*++

Copyright (c) 2014 Minoca Corp. All rights reserved.

Module Name:

    sdstd.h

Abstract:

    This header contains hardware definitions for a standard SD/MMC device.

Author:

    Evan Green 27-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// SD Command register definitions.
//

#define SD_COMMAND_DMA_ENABLE                 (1 << 0)
#define SD_COMMAND_BLOCK_COUNT_ENABLE         (1 << 1)
#define SD_COMMAND_AUTO_COMMAND_DISABLE       (0 << 2)
#define SD_COMMAND_AUTO_COMMAND12_ENABLE      (1 << 2)
#define SD_COMMAND_AUTO_COMMAND23_ENABLE      (2 << 2)
#define SD_COMMAND_TRANSFER_READ              (1 << 4)
#define SD_COMMAND_TRANSFER_WRITE             (0 << 4)
#define SD_COMMAND_SINGLE_BLOCK               (0 << 5)
#define SD_COMMAND_MULTIPLE_BLOCKS            (1 << 5)
#define SD_COMMAND_RESPONSE_NONE              (0 << 16)
#define SD_COMMAND_RESPONSE_136               (1 << 16)
#define SD_COMMAND_RESPONSE_48                (2 << 16)
#define SD_COMMAND_RESPONSE_48_BUSY           (3 << 16)
#define SD_COMMAND_CRC_CHECK_ENABLE           (1 << 19)
#define SD_COMMAND_COMMAND_INDEX_CHECK_ENABLE (1 << 20)
#define SD_COMMAND_DATA_PRESENT               (1 << 21)
#define SD_COMMAND_TYPE_NORMAL                (0 << 22)
#define SD_COMMAND_TYPE_SUSPEND               (1 << 22)
#define SD_COMMAND_TYPE_RESUME                (2 << 22)
#define SD_COMMAND_TYPE_ABORT                 (3 << 22)
#define SD_COMMAND_INDEX_SHIFT                24

//
// SD Present State register definitions.
//

#define SD_STATE_COMMAND_INHIBIT         (1 << 0)
#define SD_STATE_DATA_INHIBIT            (1 << 1)
#define SD_STATE_DATA_LINE_ACTIVE        (1 << 2)
#define SD_STATE_RETUNING_REQUEST        (1 << 3)
#define SD_STATE_WRITE_TRANSFER_ACTIVE   (1 << 8)
#define SD_STATE_READ_TRANSFER_ACTIVE    (1 << 9)
#define SD_STATE_BUFFER_WRITE_ENABLE     (1 << 10)
#define SD_STATE_BUFFER_READ_ENABLE      (1 << 11)
#define SD_STATE_CARD_INSERTED           (1 << 16)
#define SD_STATE_CARD_STATE_STABLE       (1 << 17)
#define SD_STATE_CARD_DETECT_PIN_LEVEL   (1 << 18)
#define SD_STATE_WRITE_PROTECT_PIN_LEVEL (1 << 19)
#define SD_STATE_DATA_LINE_LEVEL_MASK    (0xF << 20)
#define SD_STATE_COMMAND_LINE_LEVEL      (1 << 24)

//
// SD Host control register definitions.
//

#define SD_HOST_CONTROL_LED_ON                  (1 << 0)
#define SD_HOST_CONTROL_DATA_1BIT               (0 << 1)
#define SD_HOST_CONTROL_DATA_4BIT               (1 << 1)
#define SD_HOST_CONTROL_HIGH_SPEED              (1 << 2)
#define SD_HOST_CONTROL_SDMA                    (0 << 3)
#define SD_HOST_CONTROL_32BIT_ADMA2             (2 << 3)
#define SD_HOST_CONTROL_DMA_MODE_MASK           (3 << 3)
#define SD_HOST_CONTROL_DATA_8BIT               (1 << 5)
#define SD_HOST_CONTROL_CARD_DETECT_TEST        (1 << 6)
#define SD_HOST_CONTROL_USE_CARD_DETECT_TEST    (1 << 7)
#define SD_HOST_CONTROL_POWER_ENABLE            (1 << 8)
#define SD_HOST_CONTROL_POWER_MASK              (7 << 9)
#define SD_HOST_CONTROL_POWER_1V8               (5 << 9)
#define SD_HOST_CONTROL_POWER_3V0               (6 << 9)
#define SD_HOST_CONTROL_POWER_3V3               (7 << 9)
#define SD_HOST_CONTROL_STOP_AT_BLOCK_GAP       (1 << 16)
#define SD_HOST_CONTROL_CONTINUE                (1 << 17)
#define SD_HOST_CONTROL_READ_WAIT_CONTROL       (1 << 18)
#define SD_HOST_CONTROL_INTERRUPT_AT_BLOCK_GAP  (1 << 19)
#define SD_HOST_CONTROL_WAKE_CARD_INTERRUPT     (1 << 24)
#define SD_HOST_CONTROL_WAKE_CARD_INSERTION     (1 << 25)
#define SD_HOST_CONTROL_WAKE_CARD_REMOVAL       (1 << 26)

#define SD_HOST_CONTROL_BUS_WIDTH_MASK \
    (SD_HOST_CONTROL_DATA_4BIT | SD_HOST_CONTROL_DATA_8BIT)

//
// SD Clock control register definitions.
//

#define SD_CLOCK_CONTROL_INTERNAL_CLOCK_ENABLE      (1 << 0)
#define SD_CLOCK_CONTROL_CLOCK_STABLE               (1 << 1)
#define SD_CLOCK_CONTROL_SD_CLOCK_ENABLE            (1 << 2)
#define SD_CLOCK_CONTROL_PROGRAMMABLE_CLOCK_MODE    (1 << 5)
#define SD_CLOCK_CONTROL_DIVISOR_MASK               0xFF
#define SD_CLOCK_CONTROL_DIVISOR_SHIFT              8
#define SD_CLOCK_CONTROL_DIVISOR_HIGH_MASK          (0x3 << 8)
#define SD_CLOCK_CONTROL_DIVISOR_HIGH_SHIFT         (8 - 6)
#define SD_CLOCK_CONTROL_TIMEOUT_MASK               (0xF << 16)
#define SD_CLOCK_CONTROL_TIMEOUT_SHIFT              16
#define SD_CLOCK_CONTROL_RESET_ALL                  (1 << 24)
#define SD_CLOCK_CONTROL_RESET_COMMAND_LINE         (1 << 25)
#define SD_CLOCK_CONTROL_RESET_DATA_LINE            (1 << 26)

#define SD_CLOCK_CONTROL_DEFAULT_TIMEOUT            14

//
// Define the maximum divisor for the controller. These are based on the values
// that can be stored in the clock control register.
//

#define SD_V2_MAX_DIVISOR 256
#define SD_V3_MAX_DIVISOR 2046

//
// SD interrupt status flags.
//

#define SD_INTERRUPT_STATUS_COMMAND_COMPLETE      (1 << 0)
#define SD_INTERRUPT_STATUS_TRANSFER_COMPLETE     (1 << 1)
#define SD_INTERRUPT_STATUS_BLOCK_GAP_EVENT       (1 << 2)
#define SD_INTERRUPT_STATUS_DMA_INTERRUPT         (1 << 3)
#define SD_INTERRUPT_STATUS_BUFFER_WRITE_READY    (1 << 4)
#define SD_INTERRUPT_STATUS_BUFFER_READ_READY     (1 << 5)
#define SD_INTERRUPT_STATUS_CARD_INSERTION        (1 << 6)
#define SD_INTERRUPT_STATUS_CARD_REMOVAL          (1 << 7)
#define SD_INTERRUPT_STATUS_CARD_INTERRUPT        (1 << 8)
#define SD_INTERRUPT_STATUS_INTERRUPT_A           (1 << 9)
#define SD_INTERRUPT_STATUS_INTERRUPT_B           (1 << 10)
#define SD_INTERRUPT_STATUS_INTERRUPT_C           (1 << 11)
#define SD_INTERRUPT_STATUS_RETUNING_EVENT        (1 << 12)
#define SD_INTERRUPT_STATUS_ERROR_INTERRUPT       (1 << 15)
#define SD_INTERRUPT_STATUS_COMMAND_TIMEOUT_ERROR (1 << 16)
#define SD_INTERRUPT_STATUS_COMMAND_CRC_ERROR     (1 << 17)
#define SD_INTERRUPT_STATUS_COMMAND_END_BIT_ERROR (1 << 18)
#define SD_INTERRUPT_STATUS_COMMAND_INDEX_ERROR   (1 << 19)
#define SD_INTERRUPT_STATUS_DATA_TIMEOUT_ERROR    (1 << 20)
#define SD_INTERRUPT_STATUS_DATA_CRC_ERROR        (1 << 21)
#define SD_INTERRUPT_STATUS_DATA_END_BIT_ERROR    (1 << 22)
#define SD_INTERRUPT_STATUS_CURRENT_LIMIT_ERROR   (1 << 23)
#define SD_INTERRUPT_STATUS_AUTO_COMMAND12_ERROR  (1 << 24)
#define SD_INTERRUPT_STATUS_ADMA_ERROR            (1 << 25)
#define SD_INTERRUPT_STATUS_TUNING_ERROR          (1 << 26)
#define SD_INTERRUPT_STATUS_VENDOR_MASK           (0xF << 28)
#define SD_INTERRUPT_STATUS_ALL_MASK              0xFFFFFFFF

//
// SD interrupt signal and status enable flags.
//

#define SD_INTERRUPT_ENABLE_COMMAND_COMPLETE      (1 << 0)
#define SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE     (1 << 1)
#define SD_INTERRUPT_ENABLE_BLOCK_GAP_EVENT       (1 << 2)
#define SD_INTERRUPT_ENABLE_DMA                   (1 << 3)
#define SD_INTERRUPT_ENABLE_BUFFER_WRITE_READY    (1 << 4)
#define SD_INTERRUPT_ENABLE_BUFFER_READ_READY     (1 << 5)
#define SD_INTERRUPT_ENABLE_CARD_INSERTION        (1 << 6)
#define SD_INTERRUPT_ENABLE_CARD_REMOVAL          (1 << 7)
#define SD_INTERRUPT_ENABLE_CARD_INTERRUPT        (1 << 8)
#define SD_INTERRUPT_ENABLE_INTERRUPT_A           (1 << 9)
#define SD_INTERRUPT_ENABLE_INTERRUPT_B           (1 << 10)
#define SD_INTERRUPT_ENABLE_INTERRUPT_C           (1 << 11)
#define SD_INTERRUPT_ENABLE_RETUNING_EVENT        (1 << 12)
#define SD_INTERRUPT_ENABLE_ERROR_INTERRUPT       (1 << 15)
#define SD_INTERRUPT_ENABLE_ERROR_COMMAND_TIMEOUT (1 << 16)
#define SD_INTERRUPT_ENABLE_ERROR_COMMAND_CRC     (1 << 17)
#define SD_INTERRUPT_ENABLE_ERROR_COMMAND_END_BIT (1 << 18)
#define SD_INTERRUPT_ENABLE_ERROR_COMMAND_INDEX   (1 << 19)
#define SD_INTERRUPT_ENABLE_ERROR_DATA_TIMEOUT    (1 << 20)
#define SD_INTERRUPT_ENABLE_ERROR_DATA_CRC        (1 << 21)
#define SD_INTERRUPT_ENABLE_ERROR_DATA_END_BIT    (1 << 22)
#define SD_INTERRUPT_ENABLE_ERROR_CURRENT_LIMIT   (1 << 23)
#define SD_INTERRUPT_ENABLE_ERROR_AUTO_COMMAND12  (1 << 24)
#define SD_INTERRUPT_ENABLE_ERROR_ADMA            (1 << 25)
#define SD_INTERRUPT_ENABLE_ERROR_TUNING          (1 << 26)
#define SD_INTERRUPT_STATUS_VENDOR_MASK           (0xF << 28)

#define SD_INTERRUPT_ENABLE_ERROR_MASK              \
    (SD_INTERRUPT_ENABLE_ERROR_COMMAND_TIMEOUT |    \
     SD_INTERRUPT_ENABLE_ERROR_COMMAND_CRC |        \
     SD_INTERRUPT_ENABLE_ERROR_COMMAND_END_BIT |    \
     SD_INTERRUPT_ENABLE_ERROR_COMMAND_INDEX |      \
     SD_INTERRUPT_ENABLE_ERROR_DATA_TIMEOUT |       \
     SD_INTERRUPT_ENABLE_ERROR_DATA_CRC |           \
     SD_INTERRUPT_ENABLE_ERROR_DATA_END_BIT |       \
     SD_INTERRUPT_ENABLE_ERROR_CURRENT_LIMIT |      \
     SD_INTERRUPT_ENABLE_ERROR_AUTO_COMMAND12 |     \
     SD_INTERRUPT_ENABLE_ERROR_ADMA |               \
     SD_INTERRUPT_STATUS_VENDOR_MASK)

#define SD_INTERRUPT_STATUS_ENABLE_DEFAULT_MASK     \
    (SD_INTERRUPT_ENABLE_ERROR_MASK |               \
     SD_INTERRUPT_ENABLE_CARD_INSERTION |           \
     SD_INTERRUPT_ENABLE_CARD_REMOVAL |             \
     SD_INTERRUPT_ENABLE_BUFFER_WRITE_READY |       \
     SD_INTERRUPT_ENABLE_BUFFER_READ_READY |        \
     SD_INTERRUPT_STATUS_DMA_INTERRUPT |            \
     SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE |        \
     SD_INTERRUPT_ENABLE_COMMAND_COMPLETE)

//
// SD Capabilities.
//

#define SD_CAPABILITY_TIMEOUT_CLOCK_MASK             (0x1F << 0)
#define SD_CAPABILITY_TIMEOUT_CLOCK_UNIT_MHZ         (1 << 7)
#define SD_CAPABILITY_V3_BASE_CLOCK_FREQUENCY_MASK   0xFF
#define SD_CAPABILITY_BASE_CLOCK_FREQUENCY_MASK      0x3F
#define SD_CAPABILITY_BASE_CLOCK_FREQUENCY_SHIFT     8
#define SD_CAPABILITY_MAX_BLOCK_LENGTH_MASK          (0x3 << 16)
#define SD_CAPABILITY_MAX_BLOCK_LENGTH_512           (0x0 << 16)
#define SD_CAPABILITY_MAX_BLOCK_LENGTH_1024          (0x1 << 16)
#define SD_CAPABILITY_MAX_BLOCK_LENGTH_2048          (0x2 << 16)
#define SD_CAPABILITY_8_BIT_WIDTH                    (1 << 18)
#define SD_CAPABILITY_ADMA2                          (1 << 19)
#define SD_CAPABILITY_HIGH_SPEED                     (1 << 21)
#define SD_CAPABILITY_SDMA                           (1 << 22)
#define SD_CAPABILITY_SUSPEND_RESUME                 (1 << 23)
#define SD_CAPABILITY_VOLTAGE_3V3                    (1 << 24)
#define SD_CAPABILITY_VOLTAGE_3V0                    (1 << 25)
#define SD_CAPABILITY_VOLTAGE_1V8                    (1 << 26)
#define SD_CAPABILITY_64_BIT                         (1 << 28)
#define SD_CAPABILITY_ASYNCHRONOUS_INTERRUPT         (1 << 29)
#define SD_CAPABILITY_SLOT_TYPE_MASK                 (0x3 << 30)
#define SD_CAPABILITY_SLOT_TYPE_REMOVABLE            (0x0 << 30)
#define SD_CAPABILITY_SLOT_TYPE_EMBEDDED_SINGLE_SLOT (0x1 << 30)
#define SD_CAPABILITY_SLOT_TYPE_SHARED_BUS           (0x2 << 30)

#define SD_CAPABILITY2_SDR50                         (1 << 0)
#define SD_CAPABILITY2_SDR104                        (1 << 1)
#define SD_CAPABILITY2_SDDR50                        (1 << 2)
#define SD_CAPABILITY2_DRIVER_TYPE_A                 (1 << 4)
#define SD_CAPABILITY2_DRIVER_TYPE_C                 (1 << 5)
#define SD_CAPABILITY2_DRIVER_TYPE_D                 (1 << 6)
#define SD_CAPABILITY2_RETUNING_COUNT_MASK           (0xF << 8)
#define SD_CAPABILITY2_USE_TUNING_SDR50              (1 << 13)
#define SD_CAPABILITY2_RETUNING_MODE_MASK            (0x3 << 14)
#define SD_CAPABILITY2_CLOCK_MULTIPLIER_SHIFT        16

//
// SD Host controller version register definitions.
//

#define SD_HOST_VERSION_MASK 0x00FF

//
// ADMA2 attributes.
//

#define SD_ADMA2_VALID 0x00000001
#define SD_ADMA2_END   0x00000002
#define SD_ADMA2_INTERRUPT 0x00000004
#define SD_ADMA2_ACTION_MASK (0x3 << 4)
#define SD_ADMA2_ACTION_NOP (0 << 4)
#define SD_ADMA2_ACTION_TRANSFER (2 << 4)
#define SD_ADMA2_ACTION_LINK (3 << 4)
#define SD_ADMA2_LENGTH_SHIFT 16

//
// Define the maximum transfer length to put in one descriptor. Technically
// it's 0xFFFF, but round it down to the nearest page for better arithmetic.
//

#define SD_ADMA2_MAX_TRANSFER_SIZE 0xF000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SD_REGISTER {
    SdRegisterSdmaAddress           = 0x00,
    SdRegisterArgument2             = 0x00,
    SdRegisterBlockSizeCount        = 0x04,
    SdRegisterArgument1             = 0x08,
    SdRegisterCommand               = 0x0C,
    SdRegisterResponse10            = 0x10,
    SdRegisterResponse32            = 0x14,
    SdRegisterResponse54            = 0x18,
    SdRegisterResponse76            = 0x1C,
    SdRegisterBufferDataPort        = 0x20,
    SdRegisterPresentState          = 0x24,
    SdRegisterHostControl           = 0x28,
    SdRegisterClockControl          = 0x2C,
    SdRegisterInterruptStatus       = 0x30,
    SdRegisterInterruptStatusEnable = 0x34,
    SdRegisterInterruptSignalEnable = 0x38,
    SdRegisterControlStatus2        = 0x3C,
    SdRegisterCapabilities          = 0x40,
    SdRegisterCapabilities2         = 0x44,
    SdRegisterMaxCapabilities       = 0x48,
    SdRegisterMaxCapabilities2      = 0x4C,
    SdRegisterForceEvent            = 0x50,
    SdRegisterAdmaErrorStatus       = 0x54,
    SdRegisterAdmaAddressLow        = 0x58,
    SdRegisterAdmaAddressHigh       = 0x5C,
    SdRegisterSharedBusControl      = 0xE0,
    SdRegisterSlotStatusVersion     = 0xFC,
    SdRegisterSize                  = 0x100
} SD_REGISTER, *PSD_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

