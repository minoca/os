/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cstate.c

Abstract:

    This module implements generic kernel support for processor C-state
    transitions.

Author:

    Evan Green 25-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "pmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define how many CPU idle history events to keep around, as a log2 (bit
// shift) value.
//

#define PM_CSTATE_HISTORY_SHIFT 7

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines statistics for C-state transitions on a specific
    processor.

Members:

    EntryCount - Stores the number of times this state has been entered.

    TimeSpent - Stores the total number of time counter ticks spent in this
        state.

--*/

typedef struct _PM_CSTATE_STATISTICS {
    ULONGLONG EntryCount;
    ULONGLONG TimeSpent;
} PM_CSTATE_STATISTICS, *PPM_CSTATE_STATISTICS;

/*++

Structure Description:

    This structure defines the internal idle state context for a particular
    processor.

Members:

    Driver - Stores the driver information for this processor.

    History - Stores the processor's idle history.

    Statistics - Stores an array of statistics for each C-state. The first
        index is reserved for halt-only transitions. Starting with index one,
        each entry corresponds to a C-state described by the C-state driver.

--*/

typedef struct _PM_PROCESSOR_CSTATE_DATA {
    PM_IDLE_PROCESSOR_STATE Driver;
    PIDLE_HISTORY History;
    PPM_CSTATE_STATISTICS Statistics;
} PM_PROCESSOR_CSTATE_DATA, *PPM_PROCESSOR_CSTATE_DATA;

/*++

Structure Description:

    This structure defines the global kernel idle state information.

Members:

    Interface - Stores a pointer to the interface.

    Processors - Stores a pointer to the array of processor idle state
        structures.

    ProcessorCount - Stores the number of processors in the array.

--*/

typedef struct _PM_CSTATE_DATA {
    PPM_IDLE_STATE_INTERFACE Interface;
    PPM_PROCESSOR_CSTATE_DATA Processors;
    ULONG ProcessorCount;
} PM_CSTATE_DATA, *PPM_CSTATE_DATA;

/*++

Structure Description:

    This structure defines the context used while the C-states are initializing
    on each processor.

Members:

    Data - Stores a pointer to the interface data.

    FinishEvent - Stores a pointer to an event that will be signaled when the
        initialization is finished one way or another.

    Status - Stores the resulting status code of the initialization.

--*/

typedef struct _PM_CSTATE_INITIALIZATION_CONTEXT {
    PPM_CSTATE_DATA Data;
    PKEVENT Event;
    KSTATUS Status;
} PM_CSTATE_INITIALIZATION_CONTEXT, *PPM_CSTATE_INITIALIZATION_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PmpInitializeIdleStates (
    PPM_IDLE_STATE_INTERFACE Interface
    );

VOID
PmpInitializeProcessorIdleStatesDpc (
    PDPC Dpc
    );

VOID
PmpDebugPrintCstateStatistics (
    PPM_PROCESSOR_CSTATE_DATA Data
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this boolean to prevent entering any advanced C-states.
//

BOOL PmDisableCstates;

//
// Set this boolean to print all C-state information.
//

ULONG PmPrintCstateStatistics;

//
// Set this boolean to prevent halting at all when idle. The disable C-states
// boolean also needs to be set to full disable any processor hardware idle
// states.
//

BOOL PmDisableHalt;

//
// Store the global C-state context.
//

PPM_CSTATE_DATA PmCstateData;

//
// ------------------------------------------------------------------ Functions
//

VOID
PmIdle (
    PPROCESSOR_BLOCK Processor
    )

/*++

Routine Description:

    This routine is called on a processor to go into a low power idle state. If
    no processor idle driver has been registered or processor idle states have
    been disabled, then the processor simply halts waiting for an interrupt.
    This routine is called with interrupts disabled and returns with interrupts
    enabled. This routine should only be called internally by the idle thread.

Arguments:

    Processor - Supplies a pointer to the processor going idle.

Return Value:

    None. This routine returns from the idle state with interrupts enabled.

--*/

{

    PPM_CSTATE_DATA Data;
    ULONGLONG Duration;
    ULONGLONG EndTime;
    ULONGLONG Estimate;
    ULONG Index;
    PPM_PROCESSOR_CSTATE_DATA ProcessorData;
    ULONGLONG StartTime;
    PPM_IDLE_STATE States;
    ULONG TargetState;

    Data = PmCstateData;

    //
    // If there is no C-state data, just do a halt.
    //

    if (Data == NULL) {
        if (PmDisableHalt != FALSE) {
            ArEnableInterrupts();

        } else {
            ArWaitForInterrupt();
        }

        return;
    }

    ProcessorData = &(Data->Processors[Processor->ProcessorNumber]);
    if (PmPrintCstateStatistics != 0) {
        if (RtlAtomicExchange32(&PmPrintCstateStatistics, 0) != 0) {
            for (Index = 0; Index < PmCstateData->ProcessorCount; Index += 1) {
                PmpDebugPrintCstateStatistics(
                                           &(PmCstateData->Processors[Index]));
            }
        }
    }

    //
    // Figure out an estimate for how long this processor will be idle by
    // looking at the average of its last few idle transitions.
    //

    Estimate = PmpIdleHistoryGetAverage(ProcessorData->History);

    //
    // Figure out the best state to go to, overshooting by one.
    //

    TargetState = 0;
    if (PmDisableCstates == FALSE) {
        States = ProcessorData->Driver.States;
        while ((TargetState < ProcessorData->Driver.StateCount) &&
               (Estimate >= States[TargetState].TargetResidency)) {

            TargetState += 1;
        }
    }

    //
    // Snap the start time, and go idle.
    //

    StartTime = HlQueryTimeCounter();
    if (TargetState == 0) {
        ProcessorData->Driver.CurrentState = PM_IDLE_STATE_HALT;
        ArWaitForInterrupt();
        ArDisableInterrupts();

    } else {

        //
        // The loop above overshot by one, so back it down, and go to the idle
        // state.
        //

        ProcessorData->Driver.CurrentState = TargetState - 1;
        Data->Interface->EnterIdleState(&(ProcessorData->Driver),
                                        TargetState - 1);

        //
        // The driver may have actually entered a different state.
        //

        TargetState = ProcessorData->Driver.CurrentState + 1;
    }

    EndTime = HlQueryTimeCounter();
    if (EndTime < StartTime) {
        RtlDebugPrint("CSTATE: Time went backwards from 0x%I64x to 0x%I64x\n",
                      StartTime,
                      EndTime);
    }

    //
    // Compute this last idle duration and add it as a historical datapoint.
    //

    Duration = EndTime - StartTime;
    ProcessorData->Driver.CurrentState = PM_IDLE_STATE_NONE;
    PmpIdleHistoryAddDataPoint(ProcessorData->History, Duration);

    //
    // Mark the statistics as well. The index is offset by one to make room for
    // the "halt-only" entry.
    //

    ProcessorData->Statistics[TargetState].EntryCount += 1;
    ProcessorData->Statistics[TargetState].TimeSpent += Duration;
    ArEnableInterrupts();
    return;
}

KSTATUS
PmpGetSetIdleStateHandlers (
    BOOL FromKernelMode,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the idle state handlers. In this case the data
    pointer is used directly (so the interface structure must not disappear
    after the call). This can only be set, can only be set once, and can only
    be set from kernel mode for obvious reasons.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS if the performance state information was initialized.

    STATUS_NOT_SUPPORTED for a get operation.

    STATUS_PERMISSION_DENIED if this is a user mode request.

    STATUS_DATA_LENGTH_MISMATCH if the data size is not the size of the
    PM_IDLE_STATE_INTERFACE structure.

    STATUS_TOO_LATE if performance state handlers have already been registered.

    Other errors if the performance state runtime could not be initialized.

--*/

{

    if (FromKernelMode == FALSE) {
        return STATUS_PERMISSION_DENIED;
    }

    if (Set == FALSE) {
        return STATUS_NOT_SUPPORTED;
    }

    if (*DataSize != sizeof(PM_IDLE_STATE_INTERFACE)) {
        *DataSize = sizeof(PM_IDLE_STATE_INTERFACE);
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    if (PmCstateData != NULL) {
        return STATUS_TOO_LATE;
    }

    return PmpInitializeIdleStates(Data);
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PmpInitializeIdleStates (
    PPM_IDLE_STATE_INTERFACE Interface
    )

/*++

Routine Description:

    This routine initializes the CPU idle state interface.

Arguments:

    Interface - Supplies a pointer to the idle state interface.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PPM_CSTATE_DATA Data;
    PDPC Dpc;
    ULONG Index;
    PM_CSTATE_INITIALIZATION_CONTEXT InitializationContext;
    ULONG ProcessorCount;
    PPM_PROCESSOR_CSTATE_DATA ProcessorData;
    KSTATUS Status;

    ASSERT(PmCstateData == NULL);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    RtlZeroMemory(&InitializationContext,
                  sizeof(PM_CSTATE_INITIALIZATION_CONTEXT));

    Dpc = NULL;
    ProcessorCount = KeGetActiveProcessorCount();
    AllocationSize = sizeof(PM_CSTATE_DATA) +
                     (ProcessorCount * sizeof(PM_PROCESSOR_CSTATE_DATA));

    Data = MmAllocateNonPagedPool(AllocationSize, PM_ALLOCATION_TAG);
    if (Data == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeIdleStatesEnd;
    }

    RtlZeroMemory(Data, AllocationSize);
    Data->Interface = Interface;
    Data->ProcessorCount = ProcessorCount;
    Data->Processors = (PPM_PROCESSOR_CSTATE_DATA)(Data + 1);
    for (Index = 0; Index < ProcessorCount; Index += 1) {
        ProcessorData = &(Data->Processors[Index]);
        ProcessorData->Driver.CurrentState = PM_IDLE_STATE_NONE;
        ProcessorData->History = PmpCreateIdleHistory(IDLE_HISTORY_NON_PAGED,
                                                      PM_CSTATE_HISTORY_SHIFT);

        if (ProcessorData->History == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeIdleStatesEnd;
        }
    }

    Dpc = KeCreateDpc(PmpInitializeProcessorIdleStatesDpc,
                      &InitializationContext);

    if (Dpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeIdleStatesEnd;
    }

    InitializationContext.Data = Data;
    InitializationContext.Event = KeCreateEvent(NULL);
    if (InitializationContext.Event == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeIdleStatesEnd;
    }

    KeSignalEvent(InitializationContext.Event, SignalOptionUnsignal);

    //
    // Queue the DPC on each processor successively, starting with 0, and wait
    // for it to finish.
    //

    KeQueueDpcOnProcessor(Dpc, 0);
    KeWaitForEvent(InitializationContext.Event, FALSE, WAIT_TIME_INDEFINITE);
    Status = InitializationContext.Status;

InitializeIdleStatesEnd:
    if (InitializationContext.Event != NULL) {
        KeDestroyEvent(InitializationContext.Event);
    }

    if (Dpc != NULL) {
        KeDestroyDpc(Dpc);
    }

    if (!KSUCCESS(Status)) {
        if (Data != NULL) {
            for (Index = 0; Index < ProcessorCount; Index += 1) {
                ProcessorData = &(Data->Processors[Index]);
                if (ProcessorData->History != NULL) {
                    PmpDestroyIdleHistory(ProcessorData->History);
                }

                if (ProcessorData->Statistics != NULL) {
                    MmFreeNonPagedPool(ProcessorData->Statistics);
                }
            }

            MmFreeNonPagedPool(Data);
            Data = NULL;
        }
    }

    PmCstateData = Data;
    return Status;
}

VOID
PmpInitializeProcessorIdleStatesDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine initializes the processor idle state information for a
    particular processor. It then queues itself on the next processor.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    UINTN AllocationSize;
    PPM_CSTATE_INITIALIZATION_CONTEXT Context;
    PPROCESSOR_BLOCK Processor;
    PPM_PROCESSOR_CSTATE_DATA ProcessorData;
    KSTATUS Status;

    Processor = KeGetCurrentProcessorBlock();
    Context = Dpc->UserData;
    ProcessorData = &(Context->Data->Processors[Processor->ProcessorNumber]);
    ProcessorData->Driver.ProcessorNumber = Processor->ProcessorNumber;
    Status = Context->Data->Interface->InitializeIdleStates(
                                                     Context->Data->Interface,
                                                     &(ProcessorData->Driver));

    if (!KSUCCESS(Status)) {
        goto InitializeProcessorIdleStatesDpcEnd;
    }

    AllocationSize = (ProcessorData->Driver.StateCount + 1) *
                     sizeof(PM_CSTATE_STATISTICS);

    ProcessorData->Statistics = MmAllocateNonPagedPool(AllocationSize,
                                                       PM_ALLOCATION_TAG);

    if (ProcessorData->Statistics == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeProcessorIdleStatesDpcEnd;
    }

    RtlZeroMemory(ProcessorData->Statistics, AllocationSize);
    Status = STATUS_SUCCESS;

InitializeProcessorIdleStatesDpcEnd:

    //
    // If it failed or this was the last one, end now.
    //

    if ((!KSUCCESS(Status)) ||
        (Processor->ProcessorNumber == Context->Data->ProcessorCount - 1)) {

        Context->Status = Status;

        //
        // As soon as the event is signaled, none of these structures are safe
        // to touch anymore.
        //

        KeSignalEvent(Context->Event, SignalOptionSignalAll);

    //
    // This initialization was successful and this is not the last processor,
    // so queue this DPC on the next processor. Again, now the structures are
    // no longer safe to touch.
    //

    } else {
        KeQueueDpcOnProcessor(Dpc, Processor->ProcessorNumber + 1);
    }

    return;
}

VOID
PmpDebugPrintCstateStatistics (
    PPM_PROCESSOR_CSTATE_DATA Data
    )

/*++

Routine Description:

    This routine prints C-state statistics for the given processor.

Arguments:

    Data - Supplies a pointer to the processor data.

Return Value:

    None.

--*/

{

    PSTR Ending;
    ULONGLONG ExitLatency;
    ULONGLONG Frequency;
    ULONG Index;
    PSTR Name;
    PPM_IDLE_STATE State;
    PPM_CSTATE_STATISTICS Statistics;
    ULONGLONG TargetResidency;
    ULONGLONG TimeSpent;
    ULONGLONG TotalEvents;

    Frequency = HlQueryTimeCounterFrequency();
    RtlDebugPrint("Processor %d C-States:\n"
                  "    Name   Exit Target,    Count Time\n",
                  Data->Driver.ProcessorNumber);

    TotalEvents = 0;
    for (Index = 0; Index <= Data->Driver.StateCount; Index += 1) {
        if (Index == 0) {
            Name = "(halt)";
            ExitLatency = 0;
            TargetResidency = 0;

        } else {
            State = &(Data->Driver.States[Index - 1]);
            Name = State->Name;
            TargetResidency = (State->TargetResidency *
                               MICROSECONDS_PER_SECOND) / Frequency;

            ExitLatency = (State->ExitLatency *
                           MICROSECONDS_PER_SECOND) / Frequency;
        }

        Statistics = &(Data->Statistics[Index]);
        TimeSpent = (Statistics->TimeSpent * MICROSECONDS_PER_SECOND) /
                    Frequency;

        Ending = "us";
        if (TimeSpent > 10 * MICROSECONDS_PER_SECOND) {
            TimeSpent = Statistics->TimeSpent / Frequency;
            Ending = "s";
        }

        RtlDebugPrint("%8s: %5I64d %6I64d, %8I64d %I64d %s\n",
                      Name,
                      ExitLatency,
                      TargetResidency,
                      Statistics->EntryCount,
                      TimeSpent,
                      Ending);

        TotalEvents += Statistics->EntryCount;
    }

    RtlDebugPrint("Total Events: %I64d\n", TotalEvents);
    return;
}

