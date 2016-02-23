/*++

Copyright (c) 2014 Minoca Corp. All rights reserved.

Module Name:

    sd.h

Abstract:

    This header contains definitions for the SD/MMC device library and
    definitions common to controllers that follow the SD specification.

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

#define SD_VOLTAGE_165_195 0x00000080
#define SD_VOLTAGE_20_21   0x00000100
#define SD_VOLTAGE_21_22   0x00000200
#define SD_VOLTAGE_22_23   0x00000400
#define SD_VOLTAGE_23_24   0x00000800
#define SD_VOLTAGE_24_25   0x00001000
#define SD_VOLTAGE_25_26   0x00002000
#define SD_VOLTAGE_26_27   0x00004000
#define SD_VOLTAGE_27_28   0x00008000
#define SD_VOLTAGE_28_29   0x00010000
#define SD_VOLTAGE_29_30   0x00020000
#define SD_VOLTAGE_30_31   0x00040000
#define SD_VOLTAGE_31_32   0x00080000
#define SD_VOLTAGE_32_33   0x00100000
#define SD_VOLTAGE_33_34   0x00200000
#define SD_VOLTAGE_34_35   0x00400000
#define SD_VOLTAGE_35_36   0x00800000

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

#define SD_RESPONSE_R1B      \
    (SD_RESPONSE_PRESENT |   \
     SD_RESPONSE_VALID_CRC | \
     SD_RESPONSE_OPCODE |    \
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

#define SD_CARD_SPECIFIC_DATA_0_FREQUENCY_BASE_MASK          0x7
#define SD_CARD_SPECIFIC_DATA_0_FREQUENCY_MULTIPLIER_SHIFT   3
#define SD_CARD_SPECIFIC_DATA_0_FREQUENCY_MULTIPLIER_MASK    0xF
#define SD_CARD_SPECIFIC_DATA_0_MMC_VERSION_SHIFT            26
#define SD_CARD_SPECIFIC_DATA_0_MMC_VERSION_MASK             0xF
#define SD_CARD_SPECIFIC_DATA_1_READ_BLOCK_LENGTH_SHIFT      16
#define SD_CARD_SPECIFIC_DATA_1_READ_BLOCK_LENGTH_MASK       0x0F
#define SD_CARD_SPECIFIC_DATA_1_WRITE_BLOCK_LENGTH_SHIFT     22
#define SD_CARD_SPECIFIC_DATA_1_WRITE_BLOCK_LENGTH_MASK      0x0F
#define SD_CARD_SPECIFIC_DATA_1_HIGH_CAPACITY_MASK           0x3F
#define SD_CARD_SPECIFIC_DATA_1_HIGH_CAPACITY_SHIFT          16
#define SD_CARD_SPECIFIC_DATA_2_HIGH_CAPACITY_MASK           0xFFFF0000
#define SD_CARD_SPECIFIC_DATA_2_HIGH_CAPACITY_SHIFT          16
#define SD_CARD_SPECIFIC_DATA_HIGH_CAPACITY_MULTIPLIER       8
#define SD_CARD_SPECIFIC_DATA_1_CAPACITY_MASK                0x3FF
#define SD_CARD_SPECIFIC_DATA_1_CAPACITY_SHIFT               2
#define SD_CARD_SPECIFIC_DATA_2_CAPACITY_MASK                0xC0000000
#define SD_CARD_SPECIFIC_DATA_2_CAPACITY_SHIFT               30
#define SD_CARD_SPECIFIC_DATA_2_CAPACITY_MULTIPLIER_MASK     0x00038000
#define SD_CARD_SPECIFIC_DATA_2_CAPACITY_MULTIPLIER_SHIFT    15
#define SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_SIZE_MASK        0x00007C00
#define SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_SIZE_SHIFT       10
#define SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_MULTIPLIER_MASK  0x000003E0
#define SD_CARD_SPECIFIC_DATA_2_ERASE_GROUP_MULTIPLIER_SHIFT 5

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
#define SD_SWITCH_STATUS_4_HIGH_SPEED_MASK      0x0F000000
#define SD_SWITCH_STATUS_4_HIGH_SPEED_VALUE     0x01000000
#define SD_SWITCH_STATUS_7_HIGH_SPEED_BUSY      0x00020000

//
// Status command response bits.
//

#define SD_STATUS_MASK            (~0x0206BF7F)
#define SD_STATUS_ILLEGAL_COMMAND (1 << 22)
#define SD_STATUS_READY_FOR_DATA  (1 << 8)
#define SD_STATUS_CURRENT_STATE   (0xF << 9)
#define SD_STATUS_ERROR           (1 << 19)

#define SD_STATUS_STATE_IDLE     (0x0 << 9)
#define SD_STATUS_STATE_READY    (0x1 << 9)
#define SD_STATUS_STATE_IDENTIFY (0x2 << 9)
#define SD_STATUS_STATE_STANDBY  (0x3 << 9)
#define SD_STATUS_STATE_TRANSFER (0x4 << 9)
#define SD_STATUS_STATE_DATA     (0x5 << 9)
#define SD_STATUS_STATE_RECEIVE  (0x6 << 9)
#define SD_STATUS_STATE_PROGRAM  (0x7 << 9)
#define SD_STATUS_STATE_DISABLED (0x8 << 9)

//
// Define the software only reset flags.
//

#define SD_RESET_FLAG_ALL          0x00000001
#define SD_RESET_FLAG_COMMAND_LINE 0x00000002
#define SD_RESET_FLAG_DATA_LINE    0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

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

typedef struct _EFI_SD_CONTROLLER EFI_SD_CONTROLLER, *PEFI_SD_CONTROLLER;

/*++

Structure Description:

    This structure stores information about an SD card command.

Members:

    Command - Stores the command number.

    ResponseType - Stores the response class expected from this command.

    CommandArgument - Stores the argument to the command.

    Response - Stores the response data from the executed command.

    BufferSize - Stores the size of the data buffer in bytes.

    Buffer - Stores the physical address of the data buffer.

    Write - Stores a boolean indicating if this is a data read or write. This
        is only used if the buffer size is non-zero.

--*/

typedef struct _SD_COMMAND {
    SD_COMMAND_VALUE Command;
    UINT32 ResponseType;
    UINT32 CommandArgument;
    UINT32 Response[4];
    UINT32 BufferSize;
    VOID *Buffer;
    BOOLEAN Write;
} SD_COMMAND, *PSD_COMMAND;

typedef
EFI_STATUS
(*PSD_INITIALIZE_CONTROLLER) (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Phase
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
EFI_STATUS
(*PSD_RESET_CONTROLLER) (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 Flags
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
EFI_STATUS
(*PSD_SEND_COMMAND) (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
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
EFI_STATUS
(*PSD_GET_SET_BUS_WIDTH) (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT16 *BusWidth,
    BOOLEAN Set
    );

/*++

Routine Description:

    This routine gets or sets the controller's bus width.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    BusWidth - Supplies a pointer that receives bus width information on get
        and contains bus width information on set.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

typedef
EFI_STATUS
(*PSD_GET_SET_CLOCK_SPEED) (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    UINT32 *ClockSpeed,
    BOOLEAN Set
    );

/*++

Routine Description:

    This routine gets or sets the controller's clock speed.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    ClockSpeed - Supplies a pointer that receives the current clock speed on
        get and contains the desired clock speed on set.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

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

--*/

typedef struct _SD_FUNCTION_TABLE {
    PSD_INITIALIZE_CONTROLLER InitializeController;
    PSD_RESET_CONTROLLER ResetController;
    PSD_SEND_COMMAND SendCommand;
    PSD_GET_SET_BUS_WIDTH GetSetBusWidth;
    PSD_GET_SET_CLOCK_SPEED GetSetClockSpeed;
} SD_FUNCTION_TABLE, *PSD_FUNCTION_TABLE;

typedef
EFI_STATUS
(*PSD_GET_CARD_DETECT_STATUS) (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    BOOLEAN *CardPresent
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
EFI_STATUS
(*PSD_GET_WRITE_PROTECT_STATUS) (
    PEFI_SD_CONTROLLER Controller,
    VOID *Context,
    BOOLEAN *WriteProtect
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

/*++

Structure Description:

    This structure defines the initialization parameters passed upon creation
    of a new SD controller.

Members:

    StandardControllerBase - Stores an optional pointer to the base address of
        the standard SD host controller registers. If this is not supplied,
        then the override function table must be supplied.

    OverrideFunctionTable - Stores an optional pointer to a table of functions
        used to override the standard SD behavior. This should be used when a
        device follows the SD specification but has a non-standard register
        set.

    ConsumerContext - Stores a context pointer passed to the function pointers
        contained in this structure.

    GetCardDetectStatus - Stores an optional pointer to a function used to
        determine if there is a card in the slot.

    GetWriteProtectStatus - Stores an optional pointer to a function used to
        determine the state of the physical write protect switch on the card.

    Voltages - Stores a bitmask of supported voltages. See SD_VOLTAGE_*
        definitions.

    FundamentalClock - Stores the fundamental clock speed in Hertz.

    HostCapabilities - Stores the host controller capability bits See SD_MODE_*
        definitions.

--*/

typedef struct _EFI_SD_INITIALIZATION_BLOCK {
    VOID *StandardControllerBase;
    PSD_FUNCTION_TABLE OverrideFunctionTable;
    VOID *ConsumerContext;
    PSD_GET_CARD_DETECT_STATUS GetCardDetectStatus;
    PSD_GET_WRITE_PROTECT_STATUS GetWriteProtectStatus;
    UINT32 Voltages;
    UINT32 FundamentalClock;
    UINT32 HostCapabilities;
} EFI_SD_INITIALIZATION_BLOCK, *PEFI_SD_INITIALIZATION_BLOCK;

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
    UINT8 Crc7;
    UINT8 ManufacturingDate[2];
    UINT8 SerialNumber[4];
    UINT8 ProductRevision;
    UINT8 ProductName[5];
    UINT8 OemId[2];
    UINT8 ManufacturerId;
} PACKED SD_CARD_IDENTIFICATION, *PSD_CARD_IDENTIFICATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

PEFI_SD_CONTROLLER
EfiSdCreateController (
    PEFI_SD_INITIALIZATION_BLOCK Parameters
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

VOID
EfiSdDestroyController (
    PEFI_SD_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys an SD controller object.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

EFI_STATUS
EfiSdInitializeController (
    PEFI_SD_CONTROLLER Controller,
    BOOLEAN ResetController
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

EFI_STATUS
EfiSdBlockIoPolled (
    PEFI_SD_CONTROLLER Controller,
    UINT64 BlockOffset,
    UINTN BlockCount,
    VOID *BufferVirtual,
    BOOLEAN Write
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

EFI_STATUS
EfiSdGetMediaParameters (
    PEFI_SD_CONTROLLER Controller,
    UINT64 *BlockCount,
    UINT32 *BlockSize
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

    EFI_SUCCESS on success.

    EFI_NO_MEDIA if there is no card in the slot.

--*/

