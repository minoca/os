/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sdp.h

Abstract:

    This header contains internal definitions for the SD library. This file
    should only be included by the library itself, not by external consumers of
    the library.

Author:

    Evan Green 27-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <dev/sd.h>

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
// Define the amount of time to wait in microseconds for the controller to
// respond.
//

#define EFI_SD_CONTROLLER_TIMEOUT 1000000

//
// Define the amount of time to wait in seconds for the status to go green.
//

#define EFI_SD_CONTROLLER_STATUS_TIMEOUT 60000000

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
// Define the maximum number of times to retry I/O.
//

#define EFI_SD_IO_RETRIES 5

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an SD/MMC controller instance.

Members:

    ControllerBase - Stores a pointer to the base address of the host
        controller registers.

    ConsumerContext - Stores a context pointer passed to the function pointers
        contained in this structure.

    FunctionTable - Stores the table of functions used to perform SD operations
        that require accessing registers. This is either filled with the
        standard host controller routines or override routines supplied during
        initialization.

    GetCardDetectStatus - Stores an optional pointer to a function used to
        determine if there is a card in the slot.

    GetWriteProtectStatus - Stores an optional pointer to a function used to
        determine the state of the physical write protect switch on the card.

    Voltages - Stores a bitmask of supported voltages.

    Version - Stores the specification revision of the card.

    HighCapacity - Stores a boolean indicating if the card is high capacity or
        not.

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

--*/

struct _EFI_SD_CONTROLLER {
    VOID *ControllerBase;
    VOID *ConsumerContext;
    SD_FUNCTION_TABLE FunctionTable;
    PSD_GET_CARD_DETECT_STATUS GetCardDetectStatus;
    PSD_GET_WRITE_PROTECT_STATUS GetWriteProtectStatus;
    UINT32 Voltages;
    SD_VERSION Version;
    BOOLEAN HighCapacity;
    UINT16 CardAddress;
    UINT16 BusWidth;
    SD_CLOCK_SPEED ClockSpeed;
    UINT32 FundamentalClock;
    UINT32 ReadBlockLength;
    UINT32 WriteBlockLength;
    UINT64 UserCapacity;
    UINT64 BootCapacity;
    UINT64 RpmbCapacity;
    UINT64 GeneralPartitionCapacity[SD_MMC_GENERAL_PARTITION_COUNT];
    UINT32 EraseGroupSize;
    UINT32 CardSpecificData[4];
    UINT32 PartitionConfiguration;
    UINT32 HostCapabilities;
    UINT32 CardCapabilities;
    UINT32 MaxBlocksPerTransfer;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Stores the standard SD host controller function table.
//

extern SD_FUNCTION_TABLE EfiSdStdFunctionTable;

//
// -------------------------------------------------------- Function Prototypes
//
