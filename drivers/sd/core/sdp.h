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
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern SD_FUNCTION_TABLE SdStdFunctionTable;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
SdpInterruptServiceDpc (
    PDPC Dpc
    );

/*++

Routine Description:

    This routine implements the SD DPC that is queued when an interrupt fires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

