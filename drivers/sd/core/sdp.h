/*++

Copyright (c) 2014 Minoca Corp. All rights reserved.

Module Name:

    sdp.h

Abstract:

    This header contains internal definitions for the SD library. This file
    should only be included by the driver and library itself, not by external
    consumers of the library.

Author:

    Evan Green 27-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#define SD_API DLLEXPORT

#include <minoca/sd.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines if the given controller is an SD controller. It
// returns non-zero if it is an SD controller, or 0 if it is an MMC controller.
//

#define SD_IS_CONTROLLER_SD(_Controller) \
    ((_Controller)->Version < SdVersionMaximum)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait in seconds for the controller to respond.
//

#define SD_CONTROLLER_TIMEOUT 1

//
// Define the amount of time to wait in seconds for the status to go green.
//

#define SD_CONTROLLER_STATUS_TIMEOUT 60

//
// Define the amount of time to wait for the card to initialize, in
// microseconds.
//

#define SD_CARD_DELAY 1000
#define SD_POST_RESET_DELAY 2000

//
// Define the number of attempts to try certain commands.
//

#define SD_CARD_INITIALIZE_RETRY_COUNT 3
#define SD_CARD_OPERATING_CONDITION_RETRY_COUNT 1000
#define SD_CONFIGURATION_REGISTER_RETRY_COUNT 3
#define SD_SWITCH_RETRY_COUNT 4
#define SD_INTERFACE_CONDITION_RETRY_COUNT 10
#define SD_SET_BLOCK_LENGTH_RETRY_COUNT 10

//
// Define the block sized used by the SD library.
//

#define SD_BLOCK_SIZE 512
#define SD_MMC_MAX_BLOCK_SIZE 512

//
// Define the maximum number of blocks that can be sent in a single command.
//

#define SD_MAX_BLOCK_COUNT 0xFFFF

//
// Define the size of the ADMA2 descriptor table, which is an entry for the
// transfer and an entry for the terminator.
//

#define SD_ADMA2_DESCRIPTOR_COUNT 0x100
#define SD_ADMA2_DESCRIPTOR_TABLE_SIZE \
    (SD_ADMA2_DESCRIPTOR_COUNT * sizeof(SD_ADMA2_DESCRIPTOR))

//
// Define the bitmaks of SD controller flags.
//

#define SD_CONTROLLER_FLAG_HIGH_CAPACITY          0x00000001
#define SD_CONTROLLER_FLAG_MEDIA_PRESENT          0x00000002
#define SD_CONTROLLER_FLAG_ADMA2_ENABLED          0x00000004
#define SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED 0x00000008
#define SD_CONTROLLER_FLAG_CRITICAL_MODE          0x00000010

//
// ------------------------------------------------------ Data Type Definitions
//

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
    SdClock50MHz = 50000000,
    SdClock52MHz = 52000000,
} SD_CLOCK_SPEED, *PSD_CLOCK_SPEED;

/*++

Structure Description:

    This structure defines the context for an SD/MMC controller instance.

Members:

    ControllerBase - Stores a pointer to the base address of the host
        controller registers.

    InterruptHandle - Stores the interrupt handle of the controller.

    ConsumerContext - Stores a context pointer passed to the function pointers
        contained in this structure.

    GetCardDetectStatus - Stores an optional pointer to a function used to
        determine if there is a card in the slot.

    GetWriteProtectStatus - Stores an optional pointer to a function used to
        determine the state of the physical write protect switch on the card.

    MediaChangeCallback - Stores an optional pointer to a function called when
        media is inserted or removed.

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

    InterruptLock - Stores the spin lock held at device interrupt runlevel.

    InterruptDpc - Stores the DPC queued when an interrupt occurs.

--*/

struct _SD_CONTROLLER {
    PVOID ControllerBase;
    HANDLE InterruptHandle;
    PVOID ConsumerContext;
    PSD_GET_CARD_DETECT_STATUS GetCardDetectStatus;
    PSD_GET_WRITE_PROTECT_STATUS GetWriteProtectStatus;
    PSD_MEDIA_CHANGE_CALLBACK MediaChangeCallback;
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
    ULONG PendingStatusBits;
    KSPIN_LOCK InterruptLock;
    PDPC InterruptDpc;
};

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

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
