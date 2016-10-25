/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cycsupc.c

Abstract:

    This module implements support for using the cycle counter on the ARMv7
    architecture.

Author:

    Chris Stevens 24-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>

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
// ------------------------------------------------------------------ Functions
//

BOOL
HlpArmIsCycleCounterSupported (
    VOID
    )

/*++

Routine Description:

    This routine determines whether or not the cycle counter is supported on
    the current architecture.

Arguments:

    None.

Return Value:

    Returns TRUE if the cycle counter is supported or FALSE otherwise.

--*/

{

    if (ArGetPerformanceControlRegister() == 0) {
        return FALSE;
    }

    return TRUE;
}

KSTATUS
HlpArmEnableCycleCounter (
    VOID
    )

/*++

Routine Description:

    This routine enables the ARM cycle counter.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG CountEnable;

    ArSetPerformanceCounterEnableRegister(PERF_MONITOR_CYCLE_COUNTER);
    CountEnable = ArGetPerformanceCounterEnableRegister();
    if ((CountEnable & PERF_MONITOR_CYCLE_COUNTER) == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

VOID
HlpArmDisableCycleCounterInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine disables overflow interrupts for the ARM cycle counter.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ArClearPerformanceInterruptRegister(PERF_MONITOR_CYCLE_COUNTER);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

