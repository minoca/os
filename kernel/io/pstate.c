/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pstate.c

Abstract:

    This module implements support for processor performance states.

Author:

    Evan Green 9-Sep-2015

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
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the state necessary for computing the load for a
    given processor.

Members:

    LastBusyCycles - Stores the last snap of the sum of the processor's
        non-idle cycles.

    CurrentState - Stores the performance state index this processor should
        operate in.

    DesiredState - Stores the performance state the system would like to
        switch this processor to.

--*/

typedef struct _PM_PROCESSOR_LOAD {
    ULONGLONG LastBusyCycles;
    ULONG CurrentState;
    ULONG DesiredState;
} PM_PROCESSOR_LOAD, *PPM_PROCESSOR_LOAD;

/*++

Structure Description:

    This structure defines the kernel performance state interface.

Members:

    Interface - Stores a pointer to the interface.

    TimerDpc - Stores a pointer to the DPC associated with the re-evaluation
        DPC.

    Timer - Stores a pointer to the timer used to periodically re-evaluate the
        current performance state.

    ChangeDpc - Stores a pointer to the DPC queued to actually change the
        performance state, if the performance state is per-processor.

    ChangeWorkItem - Stores a pointer to the work item used to actually change
        the performance state, if the performance state is global.

    ChangeRunning - Stores an atomic variable indicating whether or not the
        a change is already in progress.

    Load - Stores a pointer to an arry of processor load structures, one for
        each processor in the system.

    LastTimestamp - Stores the time counter value the last time this evaluation
        was performed.

    TimeCounterFrequency - Stores the frequency of the time counter.

    ProcessorCounter - Stores information about the processor counter.

    ConstantCycleFrequency - Stores a boolean indicating if the frequency of
        the processor cycle counter is constant across all p-states (TRUE) or
        varies according to the current p-state (FALSE).

    ProcessorCount - Stores the number of processors in the system, which
        equals the number of elements in the load array.

    CurrentState - Stores the current performance state index if the
        performance state is global across all processors.

    DesiredState - Stores the desired performance state index to switch to if
        the performance state is global across all processors.

--*/

typedef struct _PM_PSTATE_DATA {
    PPM_PERFORMANCE_STATE_INTERFACE Interface;
    PDPC TimerDpc;
    PKTIMER Timer;
    PDPC ChangeDpc;
    PWORK_ITEM ChangeWorkItem;
    BOOL ChangeRunning;
    PPM_PROCESSOR_LOAD Load;
    ULONGLONG LastTimestamp;
    ULONGLONG TimeCounterFrequency;
    HL_PROCESSOR_COUNTER_INFORMATION ProcessorCounter;
    BOOL ConstantCycleFrequency;
    ULONG ProcessorCount;
    KSPIN_LOCK Lock;
    ULONG CurrentState;
    ULONG DesiredState;
} PM_PSTATE_DATA, *PPM_PSTATE_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PmpInitializePerformanceStates (
    PPM_PERFORMANCE_STATE_INTERFACE Interface
    );

VOID
PmpReevaluatePerformanceStateDpc (
    PDPC Dpc
    );

VOID
PmpChangePerformanceStateDpc (
    PDPC Dpc
    );

VOID
PmpChangePerformanceStateWorker (
    PVOID Parameter
    );

RUNLEVEL
PmpAcquirePerformanceStateLock (
    VOID
    );

VOID
PmpReleasePerformanceStateLock (
    RUNLEVEL OldRunLevel
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this debug boolean to disable future p-state changes.
//

BOOL PmDisablePstateChanges;

//
// Define the global pointer to the p-state data.
//

PPM_PSTATE_DATA PmPstateData;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
PmpGetSetPerformanceStateHandlers (
    BOOL FromKernelMode,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the performance state handlers. In this case
    the data pointer is used directly (so the interface structure must not
    disappear after the call). This can only be set, can only be set once, and
    can only be set from kernel mode for obvious reasons.

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
    PM_PERFORMANCE_STATE_INTERFACE structure.

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

    if (*DataSize != sizeof(PM_PERFORMANCE_STATE_INTERFACE)) {
        *DataSize = sizeof(PM_PERFORMANCE_STATE_INTERFACE);
        return STATUS_DATA_LENGTH_MISMATCH;
    }

    if (PmPstateData != NULL) {
        return STATUS_TOO_LATE;
    }

    return PmpInitializePerformanceStates(Data);
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PmpInitializePerformanceStates (
    PPM_PERFORMANCE_STATE_INTERFACE Interface
    )

/*++

Routine Description:

    This routine initializes performance state support in the kernel. It
    assumes a performance state interface has been registered.

Arguments:

    Interface - Supplies a pointer to the interface.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PPM_PSTATE_DATA Data;
    ULONG ProcessorCount;
    ULONG ProcessorIndex;
    KSTATUS Status;

    ASSERT(PmPstateData == NULL);

    ProcessorCount = KeGetActiveProcessorCount();
    AllocationSize = sizeof(PM_PSTATE_DATA) +
                     (ProcessorCount * sizeof(PM_PROCESSOR_LOAD));

    Data = MmAllocateNonPagedPool(AllocationSize, PM_PSTATE_ALLOCATION_TAG);
    if (Data == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePerformanceStatesEnd;
    }

    RtlZeroMemory(Data, AllocationSize);
    KeInitializeSpinLock(&(Data->Lock));
    Data->Interface = Interface;
    Data->ProcessorCount = ProcessorCount;
    Data->TimeCounterFrequency = HlQueryTimeCounterFrequency();
    Status = HlGetProcessorCounterInformation(&(Data->ProcessorCounter));
    if (!KSUCCESS(Status)) {
        goto InitializePerformanceStatesEnd;
    }

    Data->Load = (PPM_PROCESSOR_LOAD)(Data + 1);
    Data->TimerDpc = KeCreateDpc(PmpReevaluatePerformanceStateDpc, NULL);
    Data->Timer = KeCreateTimer(PM_PSTATE_ALLOCATION_TAG);
    if ((Data->TimerDpc == NULL) || (Data->Timer == NULL)) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePerformanceStatesEnd;
    }

    if ((Interface->Flags & PM_PERFORMANCE_STATE_PER_PROCESSOR) != 0) {
        Data->ChangeDpc = KeCreateDpc(PmpChangePerformanceStateDpc, NULL);
        if (Data->ChangeDpc == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializePerformanceStatesEnd;
        }

    } else {
        Data->ChangeWorkItem = KeCreateWorkItem(NULL,
                                                WorkPriorityNormal,
                                                PmpChangePerformanceStateWorker,
                                                NULL,
                                                PM_PSTATE_ALLOCATION_TAG);

        if (Data->ChangeWorkItem == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializePerformanceStatesEnd;
        }
    }

    //
    // Remember whether or not the processor counter runs at the same rate
    // regardless of p-states.
    //

    if ((Data->ProcessorCounter.Features &
         TIMER_FEATURE_P_STATE_VARIANT) == 0) {

        Data->ConstantCycleFrequency = TRUE;
    }

    //
    // Initialize all the current states to be the fastest, which it's assumed
    // they start at.
    //

    for (ProcessorIndex = 0;
         ProcessorIndex < ProcessorCount;
         ProcessorIndex += 1) {

        Data->Load[ProcessorIndex].CurrentState = Interface->StateCount - 1;
    }

    Data->CurrentState = Interface->StateCount - 1;

    //
    // Queue the timer to get the party started.
    //

    Status = KeQueueTimer(Data->Timer,
                          TimerQueueSoft,
                          0,
                          Data->Interface->MinimumPeriod,
                          0,
                          Data->TimerDpc);

    if (!KSUCCESS(Status)) {
        goto InitializePerformanceStatesEnd;
    }

    Status = STATUS_SUCCESS;

InitializePerformanceStatesEnd:
    if (!KSUCCESS(Status)) {
        if (Data != NULL) {
            if (Data->Timer != NULL) {
                KeDestroyTimer(Data->Timer);
            }

            if (Data->TimerDpc != NULL) {
                KeDestroyDpc(Data->TimerDpc);
            }

            MmFreeNonPagedPool(Data);
            Data = NULL;
        }
    }

    PmPstateData = Data;
    return Status;
}

VOID
PmpReevaluatePerformanceStateDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine is called from the periodic timer. It re-evaluates the current
    performance state.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    ULONGLONG BusyCycles;
    LONGLONG BusyDelta;
    ULONG CurrentLoad;
    ULONG CurrentState;
    ULONGLONG CurrentTime;
    ULONGLONG CycleCounterFrequency;
    PPM_PSTATE_DATA Data;
    ULONG DesiredIndex;
    ULONG FirstChangedProcessor;
    PPM_PROCESSOR_LOAD Load;
    ULONG MaxIndex;
    RUNLEVEL OldRunLevel;
    BOOL PerProcessor;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG ProcessorIndex;
    BOOL QueueChangeDpc;
    KSTATUS Status;
    ULONGLONG TimeCounterFrequency;
    ULONGLONG TimeDelta;
    ULONGLONG TimeDeltaCycles;
    ULONG WeightSum;

    Data = PmPstateData;

    //
    // Do nothing if P-state changes are locked out via the debug boolean.
    //

    if (PmDisablePstateChanges != FALSE) {
        return;
    }

    OldRunLevel = PmpAcquirePerformanceStateLock();
    FirstChangedProcessor = Data->ProcessorCount;
    MaxIndex = 0;
    Load = Data->Load;
    PerProcessor = FALSE;
    if ((Data->Interface->Flags & PM_PERFORMANCE_STATE_PER_PROCESSOR) != 0) {
        PerProcessor = TRUE;
    }

    //
    // Update the time counter snap.
    //

    TimeCounterFrequency = Data->TimeCounterFrequency;
    CurrentTime = HlQueryTimeCounter();
    TimeDelta = CurrentTime - Data->LastTimestamp;
    Data->LastTimestamp = CurrentTime;

    //
    // Compute p-state data for all processors at once.
    //

    for (ProcessorIndex = 0;
         ProcessorIndex < Data->ProcessorCount;
         ProcessorIndex += 1) {

        ProcessorBlock = KeGetProcessorBlock(ProcessorIndex);

        //
        // Grab the current busy cycles. This can tear, but it just means the
        // calculations will be off this iteration and the next.
        //

        BusyCycles = ProcessorBlock->UserCycles +
                     ProcessorBlock->KernelCycles +
                     ProcessorBlock->InterruptCycles;

        BusyDelta = BusyCycles - Load->LastBusyCycles;
        Load->LastBusyCycles = BusyCycles;

        //
        // Assume a tear occurred if the numbers appear to go backwards.
        //

        if (BusyDelta < 0) {
            Load += 1;
            continue;
        }

        //
        // Convert the time counter ticks into cycles so they can be compared.
        // The frequency of the cycle counter depends on how it behaves. It
        // might either be constant no matter what the p-state is, or it might
        // depend on the current p-state.
        //

        if (Data->ConstantCycleFrequency != FALSE) {
            CycleCounterFrequency = Data->ProcessorCounter.Frequency;

        } else {
            if (PerProcessor != FALSE) {
                CurrentState = Load->CurrentState;

            } else {
                CurrentState = Data->CurrentState;
            }

            CycleCounterFrequency =
                        Data->Interface->States[CurrentState].Frequency * 1000;

            //
            // Some cycle counters (like potentially the ARM cycle counter) run
            // at a divisor of their actual speed.
            //

            BusyDelta *= Data->ProcessorCounter.Multiplier;
        }

        //
        // If the time counter is the processor counter, don't bother with all
        // the math.
        //

        if (CycleCounterFrequency == TimeCounterFrequency) {
            TimeDeltaCycles = TimeDelta;

        //
        // Cancel the units: Tticks * Cticks/s * s/Tticks = Cticks.
        //

        } else {
            TimeDeltaCycles = (TimeDelta * CycleCounterFrequency) /
                              TimeCounterFrequency;
        }

        //
        // Compute the load over the previous duration.
        //

        CurrentLoad = (BusyDelta << PM_PERFORMANCE_STATE_WEIGHT_SHIFT) /
                      TimeDeltaCycles;

        //
        // Figure out what state that load corresponds to.
        //

        DesiredIndex = 0;
        WeightSum = 0;
        for (DesiredIndex = 0;
             DesiredIndex < Data->Interface->StateCount - 1;
             DesiredIndex += 1) {

            WeightSum += Data->Interface->States[DesiredIndex].Weight;
            if (WeightSum > CurrentLoad) {
                break;
            }
        }

        Load->DesiredState = DesiredIndex;
        if (DesiredIndex != Load->CurrentState) {
            if (FirstChangedProcessor == Data->ProcessorCount) {
                FirstChangedProcessor = ProcessorIndex;
            }
        }

        //
        // Keep track of the highest requested state.
        //

        if (DesiredIndex > MaxIndex) {
            MaxIndex = DesiredIndex;
        }

        //
        // If the state appears to have changed.
        //

        Load += 1;
    }

    QueueChangeDpc = FALSE;
    if (Data->ChangeRunning == FALSE) {
        if (PerProcessor != FALSE) {
            if (FirstChangedProcessor != Data->ProcessorCount) {
                Data->ChangeRunning = TRUE;
                QueueChangeDpc = TRUE;
            }

        } else {
            if (MaxIndex != Data->CurrentState) {
                Data->DesiredState = MaxIndex;
                Data->ChangeRunning = TRUE;
                Status = KeQueueWorkItem(Data->ChangeWorkItem);
                if (!KSUCCESS(Status)) {

                    ASSERT(FALSE);

                    Data->ChangeRunning = FALSE;
                }
            }
        }
    }

    PmpReleasePerformanceStateLock(OldRunLevel);

    //
    // Queue the change DPC if needed. Do this with the lock dropped since
    // queuing the DPC might immediately run it.
    //

    if (QueueChangeDpc != FALSE) {

        ASSERT(PerProcessor != FALSE);

        KeQueueDpcOnProcessor(Data->ChangeDpc, FirstChangedProcessor);
    }

    return;
}

VOID
PmpChangePerformanceStateDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine is called to change the current performance state on a
    particular processor.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    ULONG CurrentProcessor;
    PPM_PSTATE_DATA Data;
    PPM_PROCESSOR_LOAD Load;
    RUNLEVEL OldRunLevel;
    ULONG Processor;
    KSTATUS Status;

    Data = PmPstateData;
    CurrentProcessor = KeGetCurrentProcessorNumber();
    OldRunLevel = PmpAcquirePerformanceStateLock();
    Load = &(Data->Load[CurrentProcessor]);
    if (Load->CurrentState != Load->DesiredState) {
        Status = Data->Interface->SetPerformanceState(Data->Interface,
                                                      Load->DesiredState);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to set p-state on core %d: %d\n",
                          CurrentProcessor,
                          Status);

        } else {
            Load->CurrentState = Load->DesiredState;
        }
    }

    //
    // Queue the DPC now on the next processor that requires a change.
    //

    for (Processor = 0; Processor < Data->ProcessorCount; Processor += 1) {
        if (Processor == CurrentProcessor) {
            continue;
        }

        Load = &(Data->Load[CurrentProcessor]);
        if (Load->CurrentState != Load->DesiredState) {

            //
            // This DPC is not running on this processor, so it can be queued
            // without worrying about this thread calling into it.
            //

            KeQueueDpcOnProcessor(Data->ChangeDpc, Processor);
            break;
        }
    }

    //
    // If nothing was queued, then mark the DPC as no longer running.
    //

    if (Processor == Data->ProcessorCount) {
        Data->ChangeRunning = FALSE;
    }

    PmpReleasePerformanceStateLock(OldRunLevel);
    return;
}

VOID
PmpChangePerformanceStateWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is called to change the global performance state.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    None.

--*/

{

    PPM_PSTATE_DATA Data;
    ULONG DesiredState;
    BOOL Loop;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    Data = PmPstateData;

    //
    // Loop changing the performance state while this worker thread is behind
    // the times.
    //

    Loop = TRUE;
    do {

        //
        // Perform the change without acquiring the (dispatch level) lock.
        //

        DesiredState = Data->DesiredState;
        Status = Data->Interface->SetPerformanceState(Data->Interface,
                                                      DesiredState);

        //
        // Now acquire the lock and reconcile. There's no need to worry about
        // two of these calls racing since the change running boolean (which is
        // synchronized) prevents that.
        //

        OldRunLevel = PmpAcquirePerformanceStateLock();
        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Failed to set p-state on core %d: %d\n", -1, Status);

        } else {
            Data->CurrentState = DesiredState;
        }

        //
        // Stop looping if the state caught up, or a failure occurred.
        //

        if ((!KSUCCESS(Status)) || (Data->CurrentState == Data->DesiredState)) {
            Data->ChangeRunning = FALSE;
            Loop = FALSE;
        }

        PmpReleasePerformanceStateLock(OldRunLevel);

    } while (Loop != FALSE);

    return;
}

RUNLEVEL
PmpAcquirePerformanceStateLock (
    VOID
    )

/*++

Routine Description:

    This routine acquires the global performance state lock and raises to
    dispatch level.

Arguments:

    None.

Return Value:

    Returns the original runlevel.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(PmPstateData->Lock));
    return OldRunLevel;
}

VOID
PmpReleasePerformanceStateLock (
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the global performance state lock.

Arguments:

    OldRunLevel - Supplies the previous runlevel to return to before the lock
        was acquired.

Return Value:

    None.

--*/

{

    KeReleaseSpinLock(&(PmPstateData->Lock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

