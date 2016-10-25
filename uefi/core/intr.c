/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intr.c

Abstract:

    This module implements UEFI core interrupt support.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/kernel/hmod.h>
#include <minoca/kernel/kdebug.h>

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

EFI_PLATFORM_BEGIN_INTERRUPT EfiBeginInterruptFunction;
EFI_PLATFORM_HANDLE_INTERRUPT EfiHandleInterruptFunction;
EFI_PLATFORM_END_INTERRUPT EfiEndInterruptFunction;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiCoreInitializeInterruptServices (
    VOID
    )

/*++

Routine Description:

    This routine initializes core interrupt services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    Status = EfiPlatformInitializeInterrupts(&EfiBeginInterruptFunction,
                                             &EfiHandleInterruptFunction,
                                             &EfiEndInterruptFunction);

    if (EFI_ERROR(Status)) {
        goto CoreInitializeInterruptServicesEnd;
    }

CoreInitializeInterruptServicesEnd:
    return Status;
}

VOID
EfiCoreTerminateInterruptServices (
    VOID
    )

/*++

Routine Description:

    This routine terminates interrupt services in preparation for transitioning
    out of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfiPlatformTerminateInterrupts();
    return;
}

VOID
EfiCoreDispatchInterrupt (
    VOID
    )

/*++

Routine Description:

    This routine is called to service an interrupt.

Arguments:

    None.

Return Value:

    None.

--*/

{

    VOID *InterruptContext;
    UINT32 InterruptNumber;
    EFI_TPL OldTpl;

    ASSERT(EfiAreInterruptsEnabled() == FALSE);
    ASSERT((EfiBeginInterruptFunction != NULL) &&
           (EfiEndInterruptFunction != NULL));

    OldTpl = EfiCoreRaiseTpl(TPL_HIGH_LEVEL);
    EfiBeginInterruptFunction(&InterruptNumber, &InterruptContext);
    if (EfiHandleInterruptFunction != NULL) {
        EfiHandleInterruptFunction(InterruptNumber, &InterruptContext);
    }

    if (InterruptNumber == EfiClockTimerInterruptNumber) {
        KdPollForBreakRequest();
        EfiCoreServiceClockInterrupt(InterruptNumber);
    }

    EfiEndInterruptFunction(InterruptNumber, InterruptContext);
    EfiCoreRestoreTpl(OldTpl);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

