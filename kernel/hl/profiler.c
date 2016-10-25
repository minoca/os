/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    profiler.c

Abstract:

    This module implements profiler interrupt support at the hardware level.

Author:

    Chris Stevens 1-Jul-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "profiler.h"
#include "timer.h"
#include "intrupt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default profiler rate in 100ns. As the profiler is backed by the
// RTC on x86, the fastest sample rate is 122 microseconds.
//

#define DEFAULT_PROFILER_RATE 50000

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store a variable indicating whether profiler interrupts are broadcasted.
//

BOOL HlBroadcastProfilerInterrupts = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlStartProfilerTimer (
    VOID
    )

/*++

Routine Description:

    This routine activates the profiler by arming the profiler timer.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONGLONG TickCount;

    //
    // Fail if the profiler timer is not available.
    //

    if (HlProfilerTimer == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Fire up the profiler timer.
    //

    TickCount = HlpTimerTimeToTicks(HlProfilerTimer, DEFAULT_PROFILER_RATE);
    Status = HlpTimerArm(HlProfilerTimer, TimerModePeriodic, TickCount);
    return Status;
}

VOID
HlStopProfilerTimer (
    VOID
    )

/*++

Routine Description:

    This routine stops the profiler by disarming the profiler timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Disarm the profiler timer if it exists.
    //

    if (HlProfilerTimer != NULL) {
        HlpTimerDisarm(HlProfilerTimer);
    }

    return;
}

KSTATUS
HlpTimerInitializeProfiler (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system profiler source. It does not start the
    profiler timer.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG Processor;
    PKINTERRUPT ProfilerInterrupt;
    INTERRUPT_LINE_STATE State;
    KSTATUS Status;
    PROCESSOR_SET Target;

    if (HlProfilerTimer == NULL) {
        return STATUS_SUCCESS;
    }

    Processor = KeGetCurrentProcessorNumber();
    if (Processor == 0) {

        //
        // Configure the interrupt for the profiler timer.
        //

        RtlZeroMemory(&Target, sizeof(PROCESSOR_SET));
        Target.Target = ProcessorTargetSelf;
        State.Mode = HlProfilerTimer->Interrupt.TriggerMode;
        State.Polarity = HlProfilerTimer->Interrupt.ActiveLevel;
        State.Flags = INTERRUPT_LINE_STATE_FLAG_ENABLED;
        HlpInterruptGetStandardCpuLine(&(State.Output));
        ProfilerInterrupt = HlpInterruptGetProfilerKInterrupt();
        HlProfilerTimer->InterruptRunLevel = ProfilerInterrupt->RunLevel;
        Status = HlpInterruptSetLineState(&(HlProfilerTimer->Interrupt.Line),
                                          &State,
                                          ProfilerInterrupt,
                                          &Target,
                                          NULL,
                                          0);

        if (!KSUCCESS(Status)) {
            goto InitializeProfilerEnd;
        }

    } else {

        //
        // Always enable broadcast if there are additional processors.
        //

        ASSERT((HlProfilerTimer->Features & TIMER_FEATURE_PER_PROCESSOR) == 0);

        HlBroadcastProfilerInterrupts = TRUE;
    }

    Status = STATUS_SUCCESS;

InitializeProfilerEnd:
    return Status;
}

INTERRUPT_STATUS
HlpProfilerInterruptHandler (
    PVOID Context
    )

/*++

Routine Description:

    This routine is the main profiler ISR.

Arguments:

    Context - Supplies a pointer to the current trap frame.

Return Value:

    Claimed always.

--*/

{

    ULONG Processor;
    PROCESSOR_SET Processors;

    ASSERT(HlProfilerTimer != NULL);

    //
    // If this is P0, acknowledge the timer and send it off to the other
    // processors if broadcast is set.
    //

    Processor = KeGetCurrentProcessorNumber();
    if (Processor == 0) {
        HlpTimerAcknowledgeInterrupt(HlProfilerTimer);
        if (HlBroadcastProfilerInterrupts != FALSE) {
            Processors.Target = ProcessorTargetAllExcludingSelf;
            HlSendIpi(IpiTypeProfiler, &Processors);
        }
    }

    SpProfilerInterrupt(Context);
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

