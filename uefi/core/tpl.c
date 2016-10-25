/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tpl.c

Abstract:

    This module implements core Task Priority Level services for UEFI firmware.

Author:

    Evan Green 28-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

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
// Define the current TPL.
//

EFI_TPL EfiCurrentTpl = TPL_APPLICATION;

//
// Remember whether or not interrupts were enabled when the raise function
// disabled them.
//

BOOLEAN EfiTplInterruptsWereEnabled;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_TPL
EfiCoreRaiseTpl (
    EFI_TPL NewTpl
    )

/*++

Routine Description:

    This routine raises the current Task Priority Level.

Arguments:

    NewTpl - Supplies the new TPL to set.

Return Value:

    Returns the previous TPL.

--*/

{

    EFI_TPL OldTpl;

    OldTpl = EfiCurrentTpl;
    if ((NewTpl >= TPL_HIGH_LEVEL) && (OldTpl < TPL_HIGH_LEVEL)) {
        EfiTplInterruptsWereEnabled = EfiDisableInterrupts();
    }

    EfiCurrentTpl = NewTpl;
    return OldTpl;
}

EFIAPI
VOID
EfiCoreRestoreTpl (
    EFI_TPL OldTpl
    )

/*++

Routine Description:

    This routine restores the Task Priority Level back to its original value
    before it was raised.

Arguments:

    OldTpl - Supplies the original TPL to restore back to.

Return Value:

    None.

--*/

{

    EFI_TPL PreviousTpl;

    PreviousTpl = EfiCurrentTpl;

    ASSERT(OldTpl <= PreviousTpl);
    ASSERT(OldTpl <= TPL_HIGH_LEVEL);

    //
    // If for some reason the TPL was above high and is going below high, set
    // it directly to high.
    //

    if ((PreviousTpl >= TPL_HIGH_LEVEL) && (OldTpl < TPL_HIGH_LEVEL)) {
        PreviousTpl = TPL_HIGH_LEVEL;
        EfiCurrentTpl = TPL_HIGH_LEVEL;
    }

    //
    // Dispatch pending events.
    //

    while ((((UINTN)-2 << OldTpl) & EfiEventsPending) != 0) {
        EfiCurrentTpl = (UINTN)EfiCoreFindHighBitSet64(EfiEventsPending);
        if ((EfiCurrentTpl < TPL_HIGH_LEVEL) &&
            (PreviousTpl >= TPL_HIGH_LEVEL)) {

            if (EfiTplInterruptsWereEnabled != FALSE) {
                EfiEnableInterrupts();
            }
        }

        PreviousTpl = EfiCurrentTpl;
        EfiCoreDispatchEventNotifies(EfiCurrentTpl);
    }

    //
    // Set the new value. If the TPL is crossing below high,
    //

    EfiCurrentTpl = OldTpl;
    if ((PreviousTpl >= TPL_HIGH_LEVEL) && (OldTpl < TPL_HIGH_LEVEL)) {
        if (EfiTplInterruptsWereEnabled != FALSE) {
            EfiEnableInterrupts();
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

