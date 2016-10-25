/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#include "../veyronfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define RK32_GPIO0_HARD_RESET (1 << 13)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

VOID *EfiRk32Gpio0Base = (VOID *)RK32_GPIO0_BASE;

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

    volatile UINT32 *GpioRegister;
    UINT32 Value;

    //
    // Attempt to flush non-volatile variable data out to storage.
    //

    EfiCoreFlushVariableData();
    if ((ResetType == EfiResetCold) || (ResetType == EfiResetWarm)) {
        GpioRegister = (UINT32 *)(EfiRk32Gpio0Base + Rk32GpioPortADirection);
        Value = *GpioRegister;
        Value |= RK32_GPIO0_HARD_RESET;
        *GpioRegister = Value;
        GpioRegister = (UINT32 *)(EfiRk32Gpio0Base + Rk32GpioPortAData);
        Value = *GpioRegister;
        Value |= RK32_GPIO0_HARD_RESET;
        *GpioRegister = Value;

    } else {
        EfipRk808Shutdown();
    }

    while (TRUE) {
        NOTHING;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

