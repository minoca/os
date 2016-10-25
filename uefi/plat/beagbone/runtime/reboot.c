/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    reboot.c

Abstract:

    This module implements reset support on the TI OMAP4430.

Author:

    Evan Green 26-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/soc/am335x.h>
#include "../bbonefw.h"

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

//
// Define the base of the AM335 PRM Device registers.
//

VOID *EfiAm335PrmDeviceBase = (VOID *)AM335_PRM_DEVICE_REGISTERS;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
VOID
EfipAm335ResetSystem (
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

    volatile UINT32 *ResetControl;
    UINT32 ResetFlag;

    //
    // Attempt to flush non-volatile variable data out to storage.
    //

    EfiCoreFlushVariableData();
    ResetControl = EfiAm335PrmDeviceBase + AM335_PRM_DEVICE_RESET_CONTROL;
    if (ResetType == EfiResetWarm) {
        ResetFlag = AM335_PRM_DEVICE_RESET_CONTROL_WARM_RESET;

    } else {
        ResetFlag = AM335_PRM_DEVICE_RESET_CONTROL_COLD_RESET;
    }

    *ResetControl = *ResetControl | ResetFlag;
    while (TRUE) {
        if ((*ResetControl & ResetFlag) != 0) {
            break;
        }
    }

    //
    // Execution really should not get this far.
    //

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

