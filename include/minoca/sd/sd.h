/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/sd/sdstd.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines if the given card is an SD card. It returns non-zero
// if it is an SD card, or 0 if it is an MMC card.
//

#define SD_IS_CARD_SD(_Controller) \
    ((_Controller)->Version < SdVersionMaximum)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the API decorator.
//

#ifndef SD_API

#define SD_API __DLLIMPORT

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
#define SD_MMC_DEVICE_ID "MmcDisk"

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
#define SD_MODE_CMD23               0x0800

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
#define SD_CONTROLLER_FLAG_CRITICAL_MODE          0x00000008
#define SD_CONTROLLER_FLAG_DMA_COMMAND_ENABLED    0x00000010
#define SD_CONTROLLER_FLAG_MEDIA_CHANGED          0x00000020
#define SD_CONTROLLER_FLAG_REMOVAL_PENDING        0x00000040
#define SD_CONTROLLER_FLAG_INSERTION_PENDING      0x00000080

//
// Define the maximum number of times to retry IO.
//

#define SD_MAX_IO_RETRIES 5

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SD_VOLTAGE {
    SdVoltage0V = 0,
    SdVoltage1V8 = 1800,
    SdVoltage3V0 = 3000,
    SdVoltage3V3 = 3300,
} SD_VOLTAGE, *PSD_VOLTAGE;

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

    Set - Supplies a boolean indicating whether the clock speed should be
        queried (FALSE) or set (TRUE).

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PSD_GET_SET_VOLTAGE) (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the current bus voltage. The current voltage is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus voltage should be
        queried (FALSE) or set (TRUE).

Return Value:

    Status code.

--*/

typedef
VOID
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

    None.

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

    GetSetVoltage - Stores a pointer to a function used to get or set the
        bus voltage.

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
    PSD_GET_SET_VOLTAGE GetSetVoltage;
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

    OsDevice - Stores a pointer to the OS device.

--*/

typedef struct _SD_INITIALIZATION_BLOCK {
    PVOID StandardControllerBase;
    PVOID ConsumerContext;
    SD_FUNCTION_TABLE FunctionTable;
    ULONG Voltages;
    ULONG FundamentalClock;
    ULONG HostCapabilities;
    PDEVICE OsDevice;
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

    CurrentVoltage - Stores the current voltage, in millivolts.

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

    SendStop - Stores a boolean indicating whether a stop CMD12 needs to be
        sent after the data transfer or not.

    Try - Stores the number of times the current I/O has been attempted.

    OsDevice - Stores a pointer to the OS device.

--*/

struct _SD_CONTROLLER {
    PVOID ControllerBase;
    HANDLE InterruptHandle;
    PVOID ConsumerContext;
    SD_FUNCTION_TABLE FunctionTable;
    ULONG Voltages;
    SD_VOLTAGE CurrentVoltage;
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
    ULONG CardSpecificData[SD_MMC_CSD_WORDS];
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
    BOOL SendStop;
    LONG Try;
    PDEVICE OsDevice;
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

#pragma pack(push, 1)

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

#pragma pack(pop)

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
    BOOL UseR1bResponse
    );

/*++

Routine Description:

    This routine aborts the current SD transaction on the controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    UseR1bResponse - Supplies a boolean indicating whether to use the R1
        (FALSE) or R1b (TRUE) response for the STOP (CMD12) command.

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
KSTATUS
SdSendBlockCount (
    PSD_CONTROLLER Controller,
    ULONG BlockCount,
    BOOL Write,
    BOOL InterruptCompletion
    );

/*++

Routine Description:

    This routine sends a CMD23 to pre-specify the block count.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockCount - Supplies the block count to set.

    Write - Supplies a boolean indicating if this is a write (TRUE) or read
        (FALSE).

    InterruptCompletion - Supplies a boolean indicating whether to poll on
        completion of the command (FALSE) or wait for a transfer done interrupt
        (TRUE).

Return Value:

    STATUS_SUCCESS if the command has been queued.

    STATUS_NOT_SUPPORTED if the card or controller does not support ACMD23.

--*/

SD_API
KSTATUS
SdSendStop (
    PSD_CONTROLLER Controller,
    BOOL UseR1bResponse,
    BOOL InterruptCompletion
    );

/*++

Routine Description:

    This routine sends a CMD12 to stop the current transfer.

Arguments:

    Controller - Supplies a pointer to the controller.

    UseR1bResponse - Supplies a boolean indicating whether to use an R1b
        response (TRUE) or just R1 (FALSE) for more asynchronous aborts.

    InterruptCompletion - Supplies a boolean indicating whether to poll on
        completion of the command (FALSE) or wait for a transfer done interrupt
        (TRUE).

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

    Set - Supplies a boolean indicating whether the clock speed should be
        queried (FALSE) or set (TRUE).

Return Value:

    Status code.

--*/

SD_API
KSTATUS
SdStandardGetSetVoltage (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the bus voltage. The bus voltage is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus voltage should be
        queried (FALSE) or set (TRUE).

Return Value:

    Status code.

--*/

SD_API
VOID
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

    None.

--*/

SD_API
VOID
SdStandardMediaChangeCallback (
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

