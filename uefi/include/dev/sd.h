/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/sd/sdstd.h>

//
// ---------------------------------------------------------------- Definitions
//

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
// Define the software only reset flags.
//

#define SD_RESET_FLAG_ALL          0x00000001
#define SD_RESET_FLAG_COMMAND_LINE 0x00000002
#define SD_RESET_FLAG_DATA_LINE    0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

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

