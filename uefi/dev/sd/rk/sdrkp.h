/*++

Copyright (c) 2015 Minoca Corp. All rights reserved.

Module Name:

    sdrkp.h

Abstract:

    This header contains internal definitions for the SD Rockchip library. This
    file should only be included by the library itself, not by external
    consumers of the library.

Author:

    Chris Stevens 16-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <dev/sdrk.h>
#include <cpu/rk32xx.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait in microseconds for the controller to
// respond.
//

#define EFI_SD_RK_CONTROLLER_TIMEOUT 1000000

//
// Define the amount of time to wait in seconds for the status to go green.
//

#define EFI_SD_RK_CONTROLLER_STATUS_TIMEOUT 60000000

//
// Define the block sized used by the SD library.
//

#define SD_RK_BLOCK_SIZE 512
#define SD_RK_MMC_MAX_BLOCK_SIZE 512

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an SD/MMC Rockchip controller
    instance.

Members:

    ControllerBase - Stores a pointer to the base address of the host
        controller registers.

    SdController - Stores a pointer to the controller for the SD/MMC library
        instance.

    Voltages - Stores a bitmask of supported voltages.

    HostCapabilities - Stores the host controller capability bits.

    FundamentalClock - Stores the fundamental clock speed in Hertz.

--*/

typedef struct _EFI_SD_RK_CONTROLLER {
    VOID *ControllerBase;
    PEFI_SD_CONTROLLER SdController;
    UINT32 Voltages;
    UINT32 HostCapabilities;
    UINT32 FundamentalClock;
} EFI_SD_RK_CONTROLLER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

