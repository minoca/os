/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cyccnt.c

Abstract:

    This module implements the hardware module for the ARM cycle counter.

Author:

    Evan Green 7-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// This module should not have kernel.h included, except that the cycle counter
// is always builtin and will not be separated out into a dynamic module.
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

BOOL
HlpArmIsCycleCounterSupported (
    VOID
    );

KSTATUS
HlpArmEnableCycleCounter (
    VOID
    );

VOID
HlpArmDisableCycleCounterInterrupts (
    VOID
    );

KSTATUS
HlpArmCycleCounterInitialize (
    PVOID Context
    );

ULONGLONG
HlpArmCycleCounterRead (
    PVOID Context
    );

VOID
HlpArmCycleCounterWrite (
    PVOID Context,
    ULONGLONG NewCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpArmCycleCounterModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the ARM cycle counter hardware module.
    Its role is to report the cycle counter.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module.

Return Value:

    None.

--*/

{

    TIMER_DESCRIPTION CycleCounter;
    KSTATUS Status;

    //
    // Don't even register the timer if it is not supported on the current
    // platform and/or architecture.
    //

    if (HlpArmIsCycleCounterSupported() == FALSE) {
        return;
    }

    RtlZeroMemory(&CycleCounter, sizeof(TIMER_DESCRIPTION));
    CycleCounter.TableVersion = TIMER_DESCRIPTION_VERSION;
    CycleCounter.FunctionTable.Initialize = HlpArmCycleCounterInitialize;
    CycleCounter.FunctionTable.ReadCounter = HlpArmCycleCounterRead;
    CycleCounter.FunctionTable.WriteCounter = HlpArmCycleCounterWrite;
    CycleCounter.FunctionTable.Arm = NULL;
    CycleCounter.FunctionTable.Disarm = NULL;
    CycleCounter.FunctionTable.AcknowledgeInterrupt = NULL;
    CycleCounter.Context = NULL;
    CycleCounter.Features = TIMER_FEATURE_PER_PROCESSOR |
                            TIMER_FEATURE_READABLE |
                            TIMER_FEATURE_WRITABLE |
                            TIMER_FEATURE_P_STATE_VARIANT |
                            TIMER_FEATURE_C_STATE_VARIANT |
                            TIMER_FEATURE_PROCESSOR_COUNTER;

    //
    // The timer's frequency is not hardcoded, as it runs at the main CPU speed,
    // which must be measured.
    //

    CycleCounter.CounterFrequency = 0;
    CycleCounter.CounterBitWidth = 32;

    //
    // Register the cycle counter with the system.
    //

    Status = HlRegisterHardware(HardwareModuleTimer, &CycleCounter);
    if (!KSUCCESS(Status)) {
        goto ArmCycleCounterModuleEntryEnd;
    }

ArmCycleCounterModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpArmCycleCounterInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes the ARM cycle counter.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    ULONG Control;

    //
    // Disable cycle counter interrupts.
    //

    HlpArmDisableCycleCounterInterrupts();

    //
    // Enable performance counters in general, and set the cycle counter to
    // divide by 64.
    //

    Control = ArGetPerformanceControlRegister();
    Control |= PERF_CONTROL_CYCLE_COUNT_DIVIDE_64 | PERF_CONTROL_ENABLE;
    ArSetPerformanceControlRegister(Control);

    //
    // Enable the cycle counter.
    //

    return HlpArmEnableCycleCounter();
}

ULONGLONG
HlpArmCycleCounterRead (
    PVOID Context
    )

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    Returns the timer's current count.

--*/

{

    return ArGetCycleCountRegister();
}

VOID
HlpArmCycleCounterWrite (
    PVOID Context,
    ULONGLONG NewCount
    )

/*++

Routine Description:

    This routine writes to the timer's hardware counter.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    NewCount - Supplies the value to write into the counter. It is expected that
        the counter will not stop after the write.

Return Value:

    None.

--*/

{

    ArSetCycleCountRegister((ULONG)NewCount);
    return;
}

