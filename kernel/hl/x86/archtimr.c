/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    archtimr.c

Abstract:

    This module implements architecture-specific timer support for the hardware
    library.

Author:

    Evan Green 28-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/x86.h>
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
HlpApicTimerModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    );

VOID
HlpPmTimerModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    );

VOID
HlpTscModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    );

VOID
HlpRtcModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Timer modules that are initialized before the debugger.
//

PHARDWARE_MODULE_ENTRY HlPreDebuggerTimerModules[] = {
    HlpTscModuleEntry
};

//
// Built-in hardware modules.
//

PHARDWARE_MODULE_ENTRY HlBuiltinTimerModules[] = {
    HlpApicTimerModuleEntry,
    HlpPmTimerModuleEntry,
    HlpRtcModuleEntry
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
    Information->Multiplier = 1;
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
        ModuleEntry(&HlHardwareModuleServices);
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
            ModuleEntry(&HlHardwareModuleServices);
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

//
// --------------------------------------------------------- Internal Functions
//

