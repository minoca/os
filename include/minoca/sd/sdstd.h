/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#define SD_VOLTAGE_18       0x01000000

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
     SD_INTERRUPT_ENABLE_DMA |                      \
     SD_INTERRUPT_ENABLE_TRANSFER_COMPLETE |        \
     SD_INTERRUPT_ENABLE_COMMAND_COMPLETE)

#define SD_INTERRUPT_ENABLE_DEFAULT_MASK \
    (SD_INTERRUPT_ENABLE_CARD_INSERTION | SD_INTERRUPT_ENABLE_CARD_REMOVAL)

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
#define SD_CAPABILITY2_DDR50                         (1 << 2)
#define SD_CAPABILITY2_DRIVER_TYPE_A                 (1 << 4)
#define SD_CAPABILITY2_DRIVER_TYPE_C                 (1 << 5)
#define SD_CAPABILITY2_DRIVER_TYPE_D                 (1 << 6)
#define SD_CAPABILITY2_RETUNING_COUNT_MASK           (0xF << 8)
#define SD_CAPABILITY2_USE_TUNING_SDR50              (1 << 13)
#define SD_CAPABILITY2_RETUNING_MODE_MASK            (0x3 << 14)
#define SD_CAPABILITY2_CLOCK_MULTIPLIER_SHIFT        16

//
// Auto CMD Error Status Register and Host Control 2 Register bits.
//

#define SD_CONTROL_STATUS2_AUTO_CMD12_NOT_EXECUTED  (1 << 0)
#define SD_CONTROL_STATUS2_AUTO_CMD_TIMEOUT         (1 << 1)
#define SD_CONTROL_STATUS2_AUTO_CMD_CRC_ERROR       (1 << 2)
#define SD_CONTROL_STATUS2_AUTO_CMD_END_BIT_ERROR   (1 << 3)
#define SD_CONTROL_STATUS2_AUTO_CMD_INDEX_ERROR     (1 << 4)
#define SD_CONTROL_STATUS2_AUTO_CMD_NOT_ISSUED      (1 << 7)

#define SD_CONTROL_STATUS2_MODE_SDR12               (0x0 << 16)
#define SD_CONTROL_STATUS2_MODE_SDR25               (0x1 << 16)
#define SD_CONTROL_STATUS2_MODE_SDR50               (0x2 << 16)
#define SD_CONTROL_STATUS2_MODE_SDR104              (0x3 << 16)
#define SD_CONTROL_STATUS2_MODE_DDR50               (0x4 << 16)
#define SD_CONTROL_STATUS2_1_8V_ENABLE              (1 << 19)
#define SD_CONTROL_STATUS2_DRIVER_B                 (0x0 << 20)
#define SD_CONTROL_STATUS2_DRIVER_A                 (0x1 << 20)
#define SD_CONTROL_STATUS2_DRIVER_C                 (0x2 << 20)
#define SD_CONTROL_STATUS2_DRIVER_D                 (0x3 << 20)
#define SD_CONTROL_STATUS2_EXECUTE_TUNING           (1 << 22)
#define SD_CONTROL_STATUS2_SAMPLING_CLOCK_SELECT    (1 << 23)
#define SD_CONTROL_STATUS2_ASYNC_INTERRUPTS         (1 << 30)
#define SD_CONTROL_STATUS2_PRESET_VALUE_ENABLE      (1 << 31)

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
#define SD_OPERATING_CONDITION_1_8V             0x01000000

//
// SD configuration register values.
//

#define SD_CONFIGURATION_REGISTER_CMD23 0x00000002
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
#define SD_MMC_CSD_WORDS 4

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

#define SD_STATUS_SEQ_ERROR (1 << 3)
#define SD_STATUS_APP_CMD (1 << 5)
#define SD_STATUS_READY_FOR_DATA (1 << 8)
#define SD_STATUS_CURRENT_STATE (0xF << 9)
#define SD_STATUS_ERASE_RESET (1 << 13)
#define SD_STATUS_CARD_ECC_DISABLE (1 << 14)
#define SD_STATUS_WP_ERASE_SKIP (1 << 15)
#define SD_STATUS_CSD_OVERWRITE (1 << 16)
#define SD_STATUS_ERROR (1 << 19)
#define SD_STATUS_CC_ERROR (1 << 20)
#define SD_STATUS_CARD_ECC_FAILED (1 << 21)
#define SD_STATUS_ILLEGAL_COMMAND (1 << 22)
#define SD_STATUS_COMMAND_CRC_ERROR (1 << 23)
#define SD_STATUS_LOCK_UNLOCK_FAILED (1 << 24)
#define SD_STATUS_CARD_IS_LOCKED (1 << 25)
#define SD_STATUS_WP_VIOLATION (1 << 26)
#define SD_STATUS_ERASE_PARAM (1 << 27)
#define SD_STATUS_ERASE_SEQ_ERROR (1 << 28)
#define SD_STATUS_BLOCK_LENGTH_ERROR (1 << 29)
#define SD_STATUS_ADDRESS_ERROR (1 << 30)
#define SD_STATUS_OUT_OF_RANGE (1 << 31)

#define SD_STATUS_ERROR_MASK \
    (SD_STATUS_SEQ_ERROR | SD_STATUS_ERROR | SD_STATUS_CC_ERROR | \
     SD_STATUS_CARD_ECC_FAILED | SD_STATUS_ILLEGAL_COMMAND | \
     SD_STATUS_COMMAND_CRC_ERROR | SD_STATUS_LOCK_UNLOCK_FAILED | \
     SD_STATUS_WP_VIOLATION | SD_STATUS_ERASE_PARAM | \
     SD_STATUS_ERASE_SEQ_ERROR | SD_STATUS_BLOCK_LENGTH_ERROR | \
     SD_STATUS_ADDRESS_ERROR | SD_STATUS_OUT_OF_RANGE)

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

#define SD_MAX_CMD23_BLOCKS 0x003FFFFF

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
    SdCommandVoltageSwitch                   = 11,
    SdCommandStopTransmission                = 12,
    SdCommandSendStatus                      = 13,
    SdCommandSetBlockLength                  = 16,
    SdCommandReadSingleBlock                 = 17,
    SdCommandReadMultipleBlocks              = 18,
    SdCommandSetBlockCount                   = 23,
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

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

