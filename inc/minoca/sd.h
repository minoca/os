/*++

Copyright (c) 2014 Minoca Corp. All rights reserved.

Module Name:

    sd.h

Abstract:

    This header contains definitions for the SD/MMC driver library.

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
// Define the API decorator.
//

#ifndef SD_API

#define SD_API DLLIMPORT

#endif

#define SD_ALLOCATION_TAG 0x636D6453 // 'cMdS'

//
// Define the device ID for an SD bus slot.
//

#define SD_SLOT_DEVICE_ID "SdSlot"

//
// Define the device ID for an SD Card.
//

#define SD_CARD_DEVICE_ID "SdCard"

//
// Define SD card voltages.
//

#define SD_VOLTAGE_165_195  0x00000080
#define SD_VOLTAGE_20_21    0x00000100
#define SD_VOLTAGE_21_22    0x00000200
#define SD_VOLTAGE_22_23    0x00000400
#define SD_VOLTAGE_23_24    0x00000800
#define SD_VOLTAGE_24_25    0x00001000
#define SD_VOLTAGE_25_26    0x00002000
#define SD_VOLTAGE_26_27    0x00004000
#define SD_VOLTAGE_27_28    0x00008000
#define SD_VOLTAGE_28_29    0x00010000
#define SD_VOLTAGE_29_30    0x00020000
#define SD_VOLTAGE_30_31    0x00040000
#define SD_VOLTAGE_31_32    0x00080000
#define SD_VOLTAGE_32_33    0x00100000
#define SD_VOLTAGE_33_34    0x00200000
#define SD_VOLTAGE_34_35    0x00400000
#define SD_VOLTAGE_35_36    0x00800000

//
// Define software-only capability flags (ie these bits don't show up in the
// hardware).
//

#define SD_MODE_HIGH_SPEED          0x0001
#define SD_MODE_HIGH_SPEED_52MHZ    0x0002
#define SD_MODE_4BIT                0x0004
#define SD_MODE_8BIT                0x0008
#define SD_MODE_SPI                 0x0010
#define SD_MODE_HIGH_CAPACITY       0x0020
#define SD_MODE_AUTO_CMD12          0x0040
#define SD_MODE_ADMA2               0x0080
#define SD_MODE_RESPONSE136_SHIFTED 0x0100
#define SD_MODE_SDMA                0x0200
#define SD_MODE_SYSTEM_DMA          0x0400

//
// SD block size/count registeer definitions.
//

#define SD_SIZE_SDMA_BOUNDARY_4K (0x0 << 12)
#define SD_SIZE_SDMA_BOUNDARY_8K (0x1 << 12)
#define SD_SIZE_SDMA_BOUNDARY_16K (0x2 << 12)
#define SD_SIZE_SDMA_BOUNDARY_32K (0x3 << 12)
#define SD_SIZE_SDMA_BOUNDARY_64K (0x4 << 12)
#define SD_SIZE_SDMA_BOUNDARY_128K (0x5 << 12)
#define SD_SIZE_SDMA_BOUNDARY_256K (0x6 << 12)
#define SD_SIZE_SDMA_BOUNDARY_512K (0x7 << 12)

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
     SD_INTERRUPT_ENABLE_DMA |                      \
     SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE |        \
     SD_INTERRUPT_ENABLE_COMMAND_COMPLETE)

#define SD_INTERRUPT_ENABLE_DEFAULT_MASK \
    (SD_INTERRUPT_ENABLE_CARD_INSERTION | SD_INTERRUPT_ENABLE_CARD_REMOVAL)

//
// Define the maximum divisor for the controller.
//

#define SD_V2_MAX_DIVISOR 0x100
#define SD_V3_MAX_DIVISOR 2046

//
// SD Host controller version register definitions.
//

#define SD_HOST_VERSION_MASK 0x00FF

//
// SD operating condition flags.
//

#define SD_OPERATING_CONDITION_BUSY             0x80000000
#define SD_OPERATING_CONDITION_HIGH_CAPACITY    0x40000000
#define SD_OPERATING_CONDITION_VOLTAGE_MASK     0x007FFF80
#define SD_OPERATING_CONDITION_ACCESS_MODE      0x60000000

//
// SD configuration register values.
//

#define SD_CONFIGURATION_REGISTER_VERSION3_SHIFT 15
#define SD_CONFIGURATION_REGISTER_DATA_4BIT 0x00040000
#define SD_CONFIGURATION_REGISTER_VERSION_SHIFT 24
#define SD_CONFIGURATION_REGISTER_VERSION_MASK 0xF

//
// Define SD response flags.
//

#define SD_RESPONSE_PRESENT (1 << 0)
#define SD_RESPONSE_136_BIT (1 << 1)
#define SD_RESPONSE_VALID_CRC (1 << 2)
#define SD_RESPONSE_BUSY (1 << 3)
#define SD_RESPONSE_OPCODE (1 << 4)

#define SD_RESPONSE_NONE 0
#define SD_RESPONSE_R1 \
    (SD_RESPONSE_PRESENT | SD_RESPONSE_VALID_CRC | SD_RESPONSE_OPCODE)

#define SD_RESPONSE_R1B                                                 \
    (SD_RESPONSE_PRESENT | SD_RESPONSE_VALID_CRC | SD_RESPONSE_OPCODE | \
     SD_RESPONSE_BUSY)

#define SD_RESPONSE_R2 \
    (SD_RESPONSE_PRESENT | SD_RESPONSE_VALID_CRC | SD_RESPONSE_136_BIT)

#define SD_RESPONSE_R3 SD_RESPONSE_PRESENT
#define SD_RESPONSE_R4 SD_RESPONSE_PRESENT
#define SD_RESPONSE_R5 \
    (SD_RESPONSE_PRESENT | SD_RESPONSE_VALID_CRC | SD_RESPONSE_OPCODE)

#define SD_RESPONSE_R6 \
    (SD_RESPONSE_PRESENT | SD_RESPONSE_VALID_CRC | SD_RESPONSE_OPCODE)

#define SD_RESPONSE_R7 \
    (SD_RESPONSE_PRESENT | SD_RESPONSE_VALID_CRC | SD_RESPONSE_OPCODE)

//
// Define the R1 response bits.
//

#define SD_RESPONSE_R1_IDLE 0x01
#define SD_RESPONSE_R1_ERASE_RESET 0x02
#define SD_RESPONSE_R1_ILLEGAL_COMMAND 0x04
#define SD_RESPONSE_R1_CRC_ERROR 0x08
#define SD_RESPONSE_R1_ERASE_SEQUENCE_ERROR 0x10
#define SD_RESPONSE_R1_ADDRESS_ERROR 0x20
#define SD_RESPONSE_R1_PARAMETER_ERROR 0x40

#define SD_RESPONSE_R1_ERROR_MASK 0x7E

//
// Define the SD CMD8 check argument.
//

#define SD_COMMAND8_ARGUMENT 0x1AA

//
// Define Card Specific Data (CSD) fields coming out of the response words.
//

#define SD_CARD_SPECIFIC_DATA_0_FREQUENCY_BASE_MASK             0x7
#define SD_CARD_SPECIFIC_DATA_0_FREQUENCY_MULTIPLIER_SHIFT      3
#define SD_CARD_SPECIFIC_DATA_0_FREQUENCY_MULTIPLIER_MASK       0xF
#define SD_CARD_SPECIFIC_DATA_0_MMC_VERSION_SHIFT               26
#define SD_CARD_SPECIFIC_DATA_0_MMC_VERSION_MASK                0xF
#define SD_CARD_SPECIFIC_DATA_1_READ_BLOCK_LENGTH_SHIFT         16
#define SD_CARD_SPECIFIC_DATA_1_READ_BLOCK_LENGTH_MASK          0x0F
#define SD_CARD_SPECIFIC_DATA_1_WRITE_BLOCK_LENGTH_SHIFT        22
#define SD_CARD_SPECIFIC_DATA_1_WRITE_BLOCK_LENGTH_MASK         0x0F
#define SD_CARD_SPECIFIC_DATA_1_HIGH_CAPACITY_MASK              0x3F
#define SD_CARD_SPECIFIC_DATA_1_HIGH_CAPACITY_SHIFT             16
#define SD_CARD_SPECIFIC_DATA_2_HIGH_CAPACITY_MASK              0xFFFF0000
#define SD_CARD_SPECIFIC_DATA_2_HIGH_CAPACITY_SHIFT             16
#define SD_CARD_SPECIFIC_DATA_HIGH_CAPACITY_MULTIPLIER          8
#define SD_CARD_SPECIFIC_DATA_1_CAPACITY_MASK                   0x3FF
#define SD_CARD_SPECIFIC_DATA_1_CAPACITY_SHIFT                  2
#define SD_CARD_SPECIFIC_DATA_2_CAPACITY_MASK                   0xC0000000
#define SD_CARD_SPECIFIC_DATA_2_CAPACITY_SHIFT                  30
#define SD_CARD_SPECIFIC_DATA_2_CAPACITY_MULTIPLIER_MASK        0x00038000
#define SD_CARD_SPECIFIC_DATA_2_CAPACITY_MULTIPLIER_SHIFT       15
#define SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_SIZE_MASK           0x00007C00
#define SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_SIZE_SHIFT          10
#define SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_MULTIPLIER_MASK     0x000003E0
#define SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_MULTIPLIER_SHIFT    5

//
// Define Extended Card specific data fields.
//

#define SD_MMC_EXTENDED_CARD_DATA_GENERAL_PARTITION_SIZE    143
#define SD_MMC_EXTENDED_CARD_DATA_PARTITIONS_ATTRIBUTE      156
#define SD_MMC_EXTENDED_CARD_DATA_PARTITIONING_SUPPORT      160
#define SD_MMC_EXTENDED_CARD_DATA_RPMB_SIZE                 168
#define SD_MMC_EXTENDED_CARD_DATA_ERASE_GROUP_DEF           175
#define SD_MMC_EXTENDED_CARD_DATA_PARTITION_CONFIGURATION   179
#define SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH                 183
#define SD_MMC_EXTENDED_CARD_DATA_HIGH_SPEED                185
#define SD_MMC_EXTENDED_CARD_DATA_REVISION                  192
#define SD_MMC_EXTENDED_CARD_DATA_CARD_TYPE                 196
#define SD_MMC_EXTENDED_CARD_DATA_SECTOR_COUNT              212
#define SD_MMC_EXTENDED_CARD_DATA_WRITE_PROTECT_GROUP_SIZE  221
#define SD_MMC_EXTENDED_CARD_DATA_ERASE_GROUP_SIZE          224
#define SD_MMC_EXTENDED_CARD_DATA_BOOT_SIZE                 226

#define SD_MMC_EXTENDED_CARD_DATA_PARTITION_SHIFT 17

#define SD_MMC_GENERAL_PARTITION_COUNT 4

#define SD_MMC_EXTENDED_SECTOR_COUNT_MINIMUM \
    (1024ULL * 1024ULL * 1024ULL * 2ULL)

#define SD_MMC_PARTITION_NONE               0xFF
#define SD_MMC_PARTITION_SUPPORT            0x01
#define SD_MMC_PARTITION_ACCESS_MASK        0x07
#define SD_MMC_PARTITION_ENHANCED_ATTRIBUTE 0x1F

#define SD_MMC_EXTENDED_CARD_DATA_CARD_TYPE_MASK 0x0F
#define SD_MMC_CARD_TYPE_HIGH_SPEED_52MHZ 0x02

#define SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH_8 2
#define SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH_4 1
#define SD_MMC_EXTENDED_CARD_DATA_BUS_WIDTH_1 0

//
// Define switch command parameters.
//

//
// Switch the command set.
//

#define SD_MMC_SWITCH_MODE_COMMAND_SET 0x00

//
// Set bits in the extended CSD.
//

#define SD_MMC_SWITCH_MODE_SET_BITS 0x01

//
// Clear bits in the extended CSD.
//

#define SD_MMC_SWITCH_MODE_CLEAR_BITS 0x02

//
// Set a byte's value in the extended CSD.
//

#define SD_MMC_SWITCH_MODE_WRITE_BYTE 0x03

#define SD_MMC_SWITCH_MODE_SHIFT 24
#define SD_MMC_SWITCH_INDEX_SHIFT 16
#define SD_MMC_SWITCH_VALUE_SHIFT 8

#define SD_SWITCH_CHECK 0
#define SD_SWITCH_SWITCH 1

#define SD_SWITCH_STATUS_3_HIGH_SPEED_SUPPORTED 0x00020000
#define SD_SWITCH_STATUS_4_HIGH_SPEED_MASK 0x0F000000
#define SD_SWITCH_STATUS_4_HIGH_SPEED_VALUE 0x01000000
#define SD_SWITCH_STATUS_7_HIGH_SPEED_BUSY 0x00020000

//
// Status command response bits.
//

#define SD_STATUS_MASK (~0x0206BF7F)
#define SD_STATUS_ILLEGAL_COMMAND (1 << 22)
#define SD_STATUS_READY_FOR_DATA (1 << 8)
#define SD_STATUS_CURRENT_STATE (0xF << 9)
#define SD_STATUS_ERROR (1 << 19)

#define SD_STATUS_STATE_IDLE        (0x0 << 9)
#define SD_STATUS_STATE_READY       (0x1 << 9)
#define SD_STATUS_STATE_IDENTIFY    (0x2 << 9)
#define SD_STATUS_STATE_STANDBY     (0x3 << 9)
#define SD_STATUS_STATE_TRANSFER    (0x4 << 9)
#define SD_STATUS_STATE_DATA        (0x5 << 9)
#define SD_STATUS_STATE_RECEIVE     (0x6 << 9)
#define SD_STATUS_STATE_PROGRAM     (0x7 << 9)
#define SD_STATUS_STATE_DISABLED    (0x8 << 9)

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
// Define the maximum transfer length for SDMA.
//

#define SD_SDMA_MAX_TRANSFER_SIZE 0x80000

//
// Define the maximum transfer length to put in one descriptor. Technically
// it's 0xFFFF, but round it down to the nearest page for better arithmetic.
//

#define SD_ADMA2_MAX_TRANSFER_SIZE 0xF000

//
// Define the software only reset flags.
//

#define SD_RESET_FLAG_ALL          0x00000001
#define SD_RESET_FLAG_COMMAND_LINE 0x00000002
#define SD_RESET_FLAG_DATA_LINE    0x00000004

//
// Define the bitmask of SD controller flags.
//

#define SD_CONTROLLER_FLAG_HIGH_CAPACITY          0x00000001
#define SD_CONTROLLER_FLAG_MEDIA_PRESENT          0x00000002
#define SD_CONTROLLER_FLAG_DMA_ENABLED            0x00000004
#define SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED 0x00000008
#define SD_CONTROLLER_FLAG_CRITICAL_MODE          0x00000010
#define SD_CONTROLLER_FLAG_DMA_COMMAND_ENABLED    0x00000020

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

typedef enum _SD_COMMAND_VALUE {
    SdCommandReset                           = 0,
    SdCommandSendMmcOperatingCondition       = 1,
    SdCommandAllSendCardIdentification       = 2,
    SdCommandSetRelativeAddress              = 3,
    SdCommandSwitch                          = 6,
    SdCommandSetBusWidth                     = 6,
    SdCommandSelectCard                      = 7,
    SdCommandSendInterfaceCondition          = 8,
    SdCommandMmcSendExtendedCardSpecificData = 8,
    SdCommandSendCardSpecificData            = 9,
    SdCommandSendCardIdentification          = 10,
    SdCommandStopTransmission                = 12,
    SdCommandSendStatus                      = 13,
    SdCommandSetBlockLength                  = 16,
    SdCommandReadSingleBlock                 = 17,
    SdCommandReadMultipleBlocks              = 18,
    SdCommandWriteSingleBlock                = 24,
    SdCommandWriteMultipleBlocks             = 25,
    SdCommandEraseGroupStart                 = 35,
    SdCommandEraseGroupEnd                   = 36,
    SdCommandErase                           = 38,
    SdCommandSendSdOperatingCondition        = 41,
    SdCommandSendSdConfigurationRegister     = 51,
    SdCommandApplicationSpecific             = 55,
    SdCommandSpiReadOperatingCondition       = 58,
    SdCommandSpiCrcOnOff                     = 59,
} SD_COMMAND_VALUE, *PSD_COMMAND_VALUE;

typedef enum _SD_VERSION {
    SdVersionInvalid,
    SdVersion1p0,
    SdVersion1p10,
    SdVersion2,
    SdVersion3,
    SdVersionMaximum,
    SdMmcVersionMinimum,
    SdMmcVersion1p2,
    SdMmcVersion1p4,
    SdMmcVersion2p2,
    SdMmcVersion3,
    SdMmcVersion4,
    SdMmcVersion4p1,
    SdMmcVersion4p2,
    SdMmcVersion4p3,
    SdMmcVersion4p41,
    SdMmcVersion4p5,
    SdMmcVersionMaximum
} SD_VERSION, *PSD_VERSION;

typedef enum _SD_HOST_VERSION {
    SdHostVersion1 = 0x0,
    SdHostVersion2 = 0x1,
    SdHostVersion3 = 0x2,
} SD_HOST_VERSION, *PSD_HOST_VERSION;

typedef enum _SD_CLOCK_SPEED {
    SdClockInvalid,
    SdClock400kHz = 400000,
    SdClock25MHz = 25000000,
    SdClock26MHz = 26000000,
    SdClock50MHz = 50000000,
    SdClock52MHz = 52000000,
} SD_CLOCK_SPEED, *PSD_CLOCK_SPEED;

typedef struct _SD_CONTROLLER SD_CONTROLLER, *PSD_CONTROLLER;

/*++

Structure Description:

    This structure stores information about an SD card command.

Members:

    Command - Stores the command number.

    ResponseType - Stores the response class expected from this command.

    CommandArgument - Stores the argument to the command.

    Response - Stores the response data from the executed command.

    BufferSize - Stores the size of the data buffer in bytes.

    BufferVirtual - Stores the virtual address of the data buffer.

    BufferPhysical - Stores the physical address of the data buffer.

    Write - Stores a boolean indicating if this is a data read or write. This
        is only used if the buffer size is non-zero.

    Dma - Stores a boolean indicating if this is a DMA or non-DMA operation.

--*/

typedef struct _SD_COMMAND {
    SD_COMMAND_VALUE Command;
    ULONG ResponseType;
    ULONG CommandArgument;
    ULONG Response[4];
    ULONG BufferSize;
    PVOID BufferVirtual;
    PHYSICAL_ADDRESS BufferPhysical;
    BOOL Write;
    BOOL Dma;
} SD_COMMAND, *PSD_COMMAND;

typedef
KSTATUS
(*PSD_INITIALIZE_CONTROLLER) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Phase
    );

/*++

Routine Description:

    This routine performs any controller specific initialization steps.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Phase - Supplies the phase of initialization. Phase 0 happens after the
        initial software reset and Phase 1 happens after the bus width has been
        set to 1 and the speed to 400KHz.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSD_RESET_CONTROLLER) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Flags
    );

/*++

Routine Description:

    This routine performs a soft reset of the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Flags - Supplies a bitmask of reset flags. See SD_RESET_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSD_SEND_COMMAND) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PSD_COMMAND Command
    );

/*++

Routine Description:

    This routine sends the given command to the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Command - Supplies a pointer to the command parameters.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSD_GET_SET_BUS_WIDTH) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the controller's bus width. The bus width is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSD_GET_SET_CLOCK_SPEED) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the controller's clock speed. The clock speed is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSD_STOP_DATA_TRANSFER) (
    PSD_CONTROLLER Controller,
    PVOID Context
    );

/*++

Routine Description:

    This routine stops any current data transfer on the controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSD_GET_CARD_DETECT_STATUS) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PBOOL CardPresent
    );

/*++

Routine Description:

    This routine determines if there is currently a card in the given SD/MMC
    controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    CardPresent - Supplies a pointer where a boolean will be returned
        indicating if a card is present or not.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSD_GET_WRITE_PROTECT_STATUS) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PBOOL WriteProtect
    );

/*++

Routine Description:

    This routine determines the state of the write protect switch on the
    SD/MMC card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    WriteProtect - Supplies a pointer where a boolean will be returned
        indicating if writes are disallowed (TRUE) or if writing is allowed
        (FALSE).

Return Value:

    Status code.

--*/

typedef
VOID
(*PSD_MEDIA_CHANGE_CALLBACK) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Removal,
    BOOL Insertion
    );

/*++

Routine Description:

    This routine is called by the SD library to notify the user of the SD
    library that media has been removed, inserted, or both. This routine is
    called from a DPC and, as a result, can get called back at dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Removal - Supplies a boolean indicating if a removal event has occurred.

    Insertion - Supplies a boolean indicating if an insertion event has
        occurred.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the set of SD functions that may need to be supplied
    to the base SD driver in case the host controller is not standard.

Members:

    InitializeController - Store a pointer to a function used to initialize the
        controller.

    ResetController - Stores a pointer to a function used to reset the
        controller.

    SendCommand - Stores a pointer to a function used to send commands to the
        SD/MMC device.

    GetSetBusWidth - Store a pointer to a function used to get or set the
        controller's bus width.

    GetSetClockSpeed - Stores a pointer to a function used to get or set the
        controller's clock speed.

    StopDataTransfer - Stores a pointer to a function that stops any active
        data transfers before returning.

    GetCardDetectStatus - Stores an optional pointer to a function used to
        determine if there is a card in the slot.

    GetWriteProtectStatus - Stores an optional pointer to a function used to
        determine the state of the physical write protect switch on the card.

    MediaChangeCallback - Stores an optional pointer to a function called when
        media is inserted or removed.

--*/

typedef struct _SD_FUNCTION_TABLE {
    PSD_INITIALIZE_CONTROLLER InitializeController;
    PSD_RESET_CONTROLLER ResetController;
    PSD_SEND_COMMAND SendCommand;
    PSD_GET_SET_BUS_WIDTH GetSetBusWidth;
    PSD_GET_SET_CLOCK_SPEED GetSetClockSpeed;
    PSD_STOP_DATA_TRANSFER StopDataTransfer;
    PSD_GET_CARD_DETECT_STATUS GetCardDetectStatus;
    PSD_GET_WRITE_PROTECT_STATUS GetWriteProtectStatus;
    PSD_MEDIA_CHANGE_CALLBACK MediaChangeCallback;
} SD_FUNCTION_TABLE, *PSD_FUNCTION_TABLE;

/*++

Structure Description:

    This structure defines the initialization parameters passed upon creation
    of a new SD controller.

Members:

    StandardControllerBase - Stores an optional pointer to the base address of
        the standard SD host controller registers. If this is not supplied,
        then a function table must be supplied.

    ConsumerContext - Stores a context pointer passed to the function pointers
        contained in this structure.

    FunctionTable - Stores a table of functions used to override the standard
        SD behavior.

    Voltages - Stores a bitmask of supported voltages. See SD_VOLTAGE_*
        definitions.

    FundamentalClock - Stores the fundamental clock speed in Hertz.

    HostCapabilities - Stores the host controller capability bits See SD_MODE_*
        definitions.

--*/

typedef struct _SD_INITIALIZATION_BLOCK {
    PVOID StandardControllerBase;
    PVOID ConsumerContext;
    SD_FUNCTION_TABLE FunctionTable;
    ULONG Voltages;
    ULONG FundamentalClock;
    ULONG HostCapabilities;
} SD_INITIALIZATION_BLOCK, *PSD_INITIALIZATION_BLOCK;

typedef
VOID
(*PSD_IO_COMPLETION_ROUTINE) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    UINTN BytesCompleted,
    KSTATUS Status
    );

/*++

Routine Description:

    This routine is called by the SD library when a DMA transfer completes.
    This routine is called from a DPC and, as a result, can get called back
    at dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the library when the DMA
        request was issued.

    BytesCompleted - Supplies the number of bytes successfully read or written.

    Status - Supplies the status code representing the completion of the I/O.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the context for an SD/MMC controller instance.

Members:

    ControllerBase - Stores a pointer to the base address of the host
        controller registers.

    InterruptHandle - Stores the interrupt handle of the controller.

    ConsumerContext - Stores a context pointer passed to the function pointers
        contained in this structure.

    FunctionTable - Stores a table of routines used to implement controller
        specific

    Voltages - Stores a bitmask of supported voltages.

    Version - Stores the specification revision of the card.

    HostVersion - Stores the version of the host controller interface.

    Flags - Stores a bitmask of SD controller flags. See SD_CONTROLLER_FLAG_*
        for definitions.

    CardAddress - Stores the card address.

    BusWidth - Stores the width of the bus. Valid values are 1, 4 and 8.

    ClockSpeed - Stores the bus clock speed. This must start at the lowest
        setting (400kHz) until it's known how fast the card can go.

    FundamentalClock - Stores the fundamental clock speed in Hertz.

    ReadBlockLength - Stores the block length when reading blocks from the
        card.

    WriteBlockLength - Stores the block length when writing blocks to the
        card.

    UserCapacity - Stores the primary capacity of the controller, in bytes.

    BootCapacity - Stores the capacity of the boot partition, in bytes.

    RpmbCapacity - Stores the capacity of the Replay Protected Memory Block, in
        bytes.

    GeneralPartitionCapacity - Stores the capacity of the general partitions,
        in bytes.

    EraseGroupSize - Stores the erase group size of the card, in blocks.

    CardSpecificData - Stores the card specific data.

    PartitionConfiguration - Stores the partition configuration of this device.

    HostCapabilities - Stores the host controller capability bits.

    CardCapabilities - Stores the card capability bits.

    MaxBlocksPerTransfer - Stores the maximum number of blocks that can occur
        in a single transfer. The default is SD_MAX_BLOCK_COUNT.

    EnabledInterrupts - Stores a shadow copy of the bitmask of flags set in
        the interrupt enable register (not the interrupt status enable
        register).

    DmaDescriptorTable - Stores a pointer to the I/O buffer of the DMA
        descriptor table.

    IoCompletionRoutine - Stores a pointer to a routine called when DMA I/O
        completes.

    IoCompletionContext - Stores the I/O completion context associated with the
        DMA transfer.

    IoRequestSize - Stores the request size of the pending DMA operation.

    PendingStatusBits - Stores the mask of pending interrupt status bits.

    Timeout - Stores the timeout duration, in time counter ticks.

--*/

struct _SD_CONTROLLER {
    PVOID ControllerBase;
    HANDLE InterruptHandle;
    PVOID ConsumerContext;
    SD_FUNCTION_TABLE FunctionTable;
    ULONG Voltages;
    SD_VERSION Version;
    SD_HOST_VERSION HostVersion;
    volatile ULONG Flags;
    USHORT CardAddress;
    USHORT BusWidth;
    SD_CLOCK_SPEED ClockSpeed;
    ULONG FundamentalClock;
    ULONG ReadBlockLength;
    ULONG WriteBlockLength;
    ULONGLONG UserCapacity;
    ULONGLONG BootCapacity;
    ULONGLONG RpmbCapacity;
    ULONGLONG GeneralPartitionCapacity[SD_MMC_GENERAL_PARTITION_COUNT];
    ULONG EraseGroupSize;
    ULONG CardSpecificData[4];
    ULONG PartitionConfiguration;
    ULONG HostCapabilities;
    ULONG CardCapabilities;
    ULONG MaxBlocksPerTransfer;
    ULONG EnabledInterrupts;
    PIO_BUFFER DmaDescriptorTable;
    PSD_IO_COMPLETION_ROUTINE IoCompletionRoutine;
    PVOID IoCompletionContext;
    UINTN IoRequestSize;
    volatile ULONG PendingStatusBits;
    ULONGLONG Timeout;
};

/*++

Structure Description:

    This structure describes the card identification data from the card.

Members:

    Crc7 - Stores the CRC7, shifted by 1. The lowest bit is always 1.

    ManufacturingDate - Stores a binary coded decimal date, in the form yym,
        where year is offset from 2000. For example, April 2001 is 0x014.

    SerialNumber - Stores the product serial number.

    ProductRevision - Stores the product revision code.

    ProductName - Stores the product name string in ASCII.

    OemId - Stores the Original Equipment Manufacturer identifier.

    ManufacturerId - Stores the manufacturer identification number.

--*/

typedef struct _SD_CARD_IDENTIFICATION {
    UCHAR Crc7;
    UCHAR ManufacturingDate[2];
    UCHAR SerialNumber[4];
    UCHAR ProductRevision;
    UCHAR ProductName[5];
    UCHAR OemId[2];
    UCHAR ManufacturerId;
} PACKED SD_CARD_IDENTIFICATION, *PSD_CARD_IDENTIFICATION;

/*++

Structure Description:

    This structure describes the card identification data from the card.

Members:

    Attributes - Stores the attributes and length of this descriptor. See
        SD_ADMA2_* definitions.

    Address - Stores the 32-bit physical address of the data buffer this
        transfer descriptor refers to.

--*/

typedef struct _SD_ADMA2_DESCRIPTOR {
    ULONG Attributes;
    ULONG Address;
} PACKED SD_ADMA2_DESCRIPTOR, *PSD_ADMA2_DESCRIPTOR;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

SD_API
PSD_CONTROLLER
SdCreateController (
    PSD_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine creates a new SD controller object.

Arguments:

    Parameters - Supplies a pointer to the parameters to use when creating the
        controller. This can be stack allocated, as the SD library won't use
        this memory after this routine returns.

Return Value:

    Returns a pointer to the controller structure on success.

    NULL on allocation failure or if a required parameter was not filled in.

--*/

SD_API
VOID
SdDestroyController (
    PSD_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys an SD controller object.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

SD_API
KSTATUS
SdInitializeController (
    PSD_CONTROLLER Controller,
    BOOL ResetController
    );

/*++

Routine Description:

    This routine resets and initializes the SD host controller.

Arguments:

    Controller - Supplies a pointer to the controller to initialize.

    ResetController - Supplies a boolean indicating whether or not to reset
        the controller.

Return Value:

    Status code.

--*/

SD_API
KSTATUS
SdBlockIoPolled (
    PSD_CONTROLLER Controller,
    ULONGLONG BlockOffset,
    UINTN BlockCount,
    PVOID BufferVirtual,
    BOOL Write
    );

/*++

Routine Description:

    This routine performs a block I/O read or write using the CPU and not
    DMA.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    BufferVirtual - Supplies the virtual address of the I/O buffer.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation.

Return Value:

    Status code.

--*/

SD_API
KSTATUS
SdGetMediaParameters (
    PSD_CONTROLLER Controller,
    PULONGLONG BlockCount,
    PULONG BlockSize
    );

/*++

Routine Description:

    This routine returns information about the media card.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockCount - Supplies a pointer where the number of blocks in the user
        area of the medium will be returned.

    BlockSize - Supplies a pointer where the block size of the medium will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_MEDIA if there is no card in the slot.

--*/

SD_API
KSTATUS
SdAbortTransaction (
    PSD_CONTROLLER Controller,
    BOOL SynchronousAbort
    );

/*++

Routine Description:

    This routine aborts the current SD transaction on the controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    SynchronousAbort - Supplies a boolean indicating if an synchronous abort
        is requested or not. Note that an asynchronous abort is not actually
        asynchronous from the drivers perspective. The name is taken from
        Section 3.8 "Abort Transaction" of the SD Version 3.0 specification.

Return Value:

    Status code.

--*/

SD_API
VOID
SdSetCriticalMode (
    PSD_CONTROLLER Controller,
    BOOL Enable
    );

/*++

Routine Description:

    This routine sets the SD controller into and out of critical execution
    mode. Critical execution mode is necessary for crash dump scenarios in
    which timeouts must be calculated by querying the hardware time counter
    directly, as the clock is not running to update the kernel's time counter.

Arguments:

    Controller - Supplies a pointer to the controller.

    Enable - Supplies a boolean indicating if critical mode should be enabled
        or disabled.

Return Value:

    None.

--*/

SD_API
KSTATUS
SdErrorRecovery (
    PSD_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine attempts to perform recovery after an error.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

SD_API
ULONGLONG
SdQueryTimeCounter (
    PSD_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine returns a snap of the time counter. Depending on the mode of
    the SD controller, this may be just a recent snap of the time counter or
    the current value in the hardware.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Returns the number of ticks that have elapsed since the system was booted,
    or a recent tick value.

--*/

//
// Standard SD host controller functions
//

SD_API
INTERRUPT_STATUS
SdStandardInterruptService (
    PSD_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine implements the interrupt service routine for a standard SD
    controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Returns whether or not the SD controller caused the interrupt.

--*/

SD_API
INTERRUPT_STATUS
SdStandardInterruptServiceDispatch (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the interrupt handler that is called at dispatch
    level.

Arguments:

    Context - Supplies a context pointer, which in this case is a pointer to
        the SD controller.

Return Value:

    None.

--*/

SD_API
KSTATUS
SdStandardInitializeDma (
    PSD_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine initializes standard DMA support in the host controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the host controller does not support ADMA2.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_NO_MEDIA if there is no card in the slot.

--*/

SD_API
VOID
SdStandardBlockIoDma (
    PSD_CONTROLLER Controller,
    ULONGLONG BlockOffset,
    UINTN BlockCount,
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset,
    BOOL Write,
    PSD_IO_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
    );

/*++

Routine Description:

    This routine performs a block I/O read or write using standard ADMA2.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    IoBuffer - Supplies a pointer to the buffer containing the data to write
        or where the read data should be returned.

    IoBufferOffset - Supplies the offset from the beginning of the I/O buffer
        where this I/O should begin. This is relative to the I/O buffer's
        current offset.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation.

    CompletionRoutine - Supplies a pointer to a function to call when the I/O
        completes.

    CompletionContext - Supplies a context pointer to pass as a parameter to
        the completion routine.

Return Value:

    None. The status of the operation is returned when the completion routine
    is called, which may be during the execution of this function in the case
    of an early failure.

--*/

SD_API
KSTATUS
SdStandardInitializeController (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Phase
    );

/*++

Routine Description:

    This routine performs any controller specific initialization steps.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Phase - Supplies the phase of initialization. Phase 0 happens after the
        initial software reset and Phase 1 happens after the bus width has been
        set to 1 and the speed to 400KHz.

Return Value:

    Status code.

--*/

SD_API
KSTATUS
SdStandardResetController (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Flags
    );

/*++

Routine Description:

    This routine performs a soft reset of the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Flags - Supplies a bitmask of reset flags. See SD_RESET_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

SD_API
KSTATUS
SdStandardSendCommand (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PSD_COMMAND Command
    );

/*++

Routine Description:

    This routine sends the given command to the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Command - Supplies a pointer to the command parameters.

Return Value:

    Status code.

--*/

SD_API
KSTATUS
SdStandardGetSetBusWidth (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the controller's bus width. The bus width is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

SD_API
KSTATUS
SdStandardGetSetClockSpeed (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the controller's clock speed. The clock speed is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

SD_API
KSTATUS
SdStandardStopDataTransfer (
    PSD_CONTROLLER Controller,
    PVOID Context
    );

/*++

Routine Description:

    This routine stops any current data transfer on the controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

Return Value:

    Status code.

--*/

