/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    reboot.c

Abstract:

    This module implements reset support on the RK3288.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "veyronfw.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

VOID *EfiRk32CruBase = (VOID *)RK32_CRU_BASE;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
VOID
EfipRk32ResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    )

/*++

Routine Description:

    This routine resets the entire platform.

Arguments:

    ResetType - Supplies the type of reset to perform.

    ResetStatus - Supplies the status code for this reset.

    DataSize - Supplies the size of the reset data.

    ResetData - Supplies an optional pointer for reset types of cold, warm, or
        shutdown to a null-terminated string, optionally followed by additional
        binary data.

Return Value:

    None. This routine does not return.

--*/

{

    volatile UINT32 *ResetRegister;
    UINT32 ResetValue;

    //
    // Attempt to flush non-volatile variable data out to storage.
    //

    EfiCoreFlushVariableData();
    if (ResetType == EfiResetWarm) {
        ResetRegister = (UINT32 *)(EfiRk32CruBase + Rk32CruGlobalReset1);
        ResetValue = RK32_GLOBAL_RESET1_VALUE;

    } else {
        ResetRegister = (UINT32 *)(EfiRk32CruBase + Rk32CruGlobalReset2);
        ResetValue = RK32_GLOBAL_RESET2_VALUE;
    }

    *ResetRegister = ResetValue;
    while (TRUE) {
        NOTHING;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

