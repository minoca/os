/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archtimr.c

Abstract:

    This module implements architecture-specific timer support for the hardware
    library.

Author:

    Chris Stevens 2-Feb-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include "../hlp.h"
#include "../timer.h"
#include "../clock.h"
#include "../intrupt.h"
#include "../profiler.h"

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
// Builtin hardware module function prototypes.
//

VOID
HlpArmCycleCounterModuleEntry (
    VOID
    );

VOID
HlpBcm2709TimerModuleEntry (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Timer modules that are initialized before the debugger.
//

PHARDWARE_MODULE_ENTRY HlPreDebuggerTimerModules[] = {
    HlpArmCycleCounterModuleEntry,
};

//
// Built-in hardware modules.
//

PHARDWARE_MODULE_ENTRY HlBuiltinTimerModules[] = {
    HlpBcm2709TimerModuleEntry,
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlGetProcessorCounterInformation (
    PHL_PROCESSOR_COUNTER_INFORMATION Information
    )

/*++

Routine Description:

    This routine returns information about the cycle counter built into the
    processor.

Arguments:

    Information - Supplies a pointer where the processor counter information
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the processor does not have a processor cycle
    counter.

--*/

{

    Information->Frequency = HlProcessorCounter->CounterFrequency;

    //
    // The ARM cycle counter is configured to divide actual cycles by 64, since
    // it's only a 32 bit value and can roll over quickly.
    //

    Information->Multiplier = 64;
    Information->Features = HlProcessorCounter->Features;
    return STATUS_SUCCESS;
}

VOID
HlpArchInitializeTimersPreDebugger (
    VOID
    )

/*++

Routine Description:

    This routine implements early timer initialization for the hardware module
    API layer. This routine is *undebuggable*, as it is called before the
    debugger is brought online.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PHARDWARE_MODULE_ENTRY ModuleEntry;
    UINTN TimerCount;
    UINTN TimerIndex;

    TimerCount = sizeof(HlPreDebuggerTimerModules) /
                 sizeof(HlPreDebuggerTimerModules[0]);

    for (TimerIndex = 0; TimerIndex < TimerCount; TimerIndex += 1) {
        ModuleEntry = HlPreDebuggerTimerModules[TimerIndex];
        ModuleEntry();
    }

    return;
}

KSTATUS
HlpArchInitializeTimers (
    VOID
    )

/*++

Routine Description:

    This routine performs architecture-specific initialization for the timer
        subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    UINTN ModuleCount;
    PHARDWARE_MODULE_ENTRY ModuleEntry;
    UINTN ModuleIndex;

    //
    // On the boot processor, perform one-time initialization.
    //

    if (KeGetCurrentProcessorNumber() == 0) {

        //
        // Loop through and initialize every built in hardware module.
        //

        ModuleCount = sizeof(HlBuiltinTimerModules) /
                      sizeof(HlBuiltinTimerModules[0]);

        for (ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex += 1) {
            ModuleEntry = HlBuiltinTimerModules[ModuleIndex];
            ModuleEntry();
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
HlpArchInitializeCalendarTimers (
    VOID
    )

/*++

Routine Description:

    This routine performs architecture-specific initialization for the
    calendar timer subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

KERNEL_API
ULONGLONG
HlQueryProcessorCounter (
    VOID
    )

/*++

Routine Description:

    This routine queries the processor counter hardware and returns a 64-bit
    monotonically non-decreasing value that correlates to "processor" time.
    This does not necessarily correspond to wall-clock time, as the frequency
    of this counter may vary over time. This counter may also vary across
    processors, so this routine must be called at dispatch level or higher.
    Failing to call this routine at or above dispatch level may cause the
    counter's internal accounting to malfunction.

    This routine is intended primarily for the scheduler to track processor
    cycles. Users looking to measure units of time should query the time
    counter.

Arguments:

    None.

Return Value:

    Returns a 64-bit non-decreasing value corresponding to "processor" time.

--*/

{

    //
    // Ideally there wouldn't be an assert in such a hot path, but it's very
    // important that this not be called below dispatch level, as doing so
    // would cause the "current count" software managed bits to get miscounted.
    //

    ASSERT((KeGetRunLevel() >= RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    return HlpTimerExtendedQuery(HlProcessorCounter);
}

//
// --------------------------------------------------------- Internal Functions
//

