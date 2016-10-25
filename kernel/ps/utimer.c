/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    utimer.c

Abstract:

    This module implements user mode timer support.

Author:

    Evan Green 11-Aug-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "psp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PROCESS_TIMER_ALLOCATION_TAG 0x6D547350 // 'mTsP'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a user mode timer.

Members:

    ListEntry - Stores pointers to the next and previous timers in the process
        list.

    ReferenceCount - Stores the reference count on the timer.

    Process - Stores a pointer to the process that owns this timer.

    Thread - Stores an optional pointer to the thread that is to be signaled
        when the timer expires. If NULL, then the process is signaled.

    TimerNumber - Stores the timer's identifying number.

    DueTime - Stores the due time of the timer.

    Interval - Stores the periodic interval of the timer.

    ExpirationCount - Stores the number of timer expirations that have occurred
        since the last work item ran.

    OverflowCount - Stores the number of overflows that have occurred since the
        last time the caller asked.

    Timer - Stores a pointer to the timer backing this user mode timer.

    Dpc - Stores a pointer to the DPC that runs when the timer fires.

    WorkItem - Stores a pointer to the work item that's queued when the DPC
        runs.

    SignalQueueEntry - Stores the signal queue entry that gets queued when the
        timer expires.

--*/

typedef struct _PROCESS_TIMER {
    LIST_ENTRY ListEntry;
    ULONG ReferenceCount;
    PKPROCESS Process;
    PKTHREAD Thread;
    LONG TimerNumber;
    ULONGLONG DueTime;
    ULONGLONG Interval;
    ULONG ExpirationCount;
    ULONG OverflowCount;
    PKTIMER Timer;
    PDPC Dpc;
    PWORK_ITEM WorkItem;
    SIGNAL_QUEUE_ENTRY SignalQueueEntry;
} PROCESS_TIMER, *PPROCESS_TIMER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PspCreateTimer (
    PKPROCESS Process,
    PKTHREAD Thread,
    PPROCESS_TIMER *Timer
    );

KSTATUS
PspSetTimer (
    PKPROCESS Process,
    PPROCESS_TIMER Timer,
    PULONGLONG DueTime,
    PULONGLONG Period
    );

KSTATUS
PspCreateProcessTimer (
    PKPROCESS Process,
    PKTHREAD Thread,
    PPROCESS_TIMER *Timer
    );

VOID
PspProcessTimerAddReference (
    PPROCESS_TIMER Timer
    );

VOID
PspProcessTimerReleaseReference (
    PPROCESS_TIMER Timer
    );

VOID
PspDestroyProcessTimer (
    PPROCESS_TIMER Timer
    );

VOID
PspFlushProcessTimer (
    PKPROCESS Process,
    PPROCESS_TIMER Timer
    );

VOID
PspProcessTimerDpcRoutine (
    PDPC Dpc
    );

VOID
PspProcessTimerWorkRoutine (
    PVOID Parameter
    );

VOID
PspProcessTimerSignalCompletion (
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    );

VOID
PspExpireRuntimeTimer (
    PKTHREAD Thread,
    PRUNTIME_TIMER Timer,
    ULONG Signal,
    ULONGLONG CurrentTime
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INTN
PsSysQueryTimeCounter (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for getting the current time
    counter value.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSYSTEM_CALL_QUERY_TIME_COUNTER Parameters;

    Parameters = (PSYSTEM_CALL_QUERY_TIME_COUNTER)SystemCallParameter;
    Parameters->Value = HlQueryTimeCounter();
    return STATUS_SUCCESS;
}

INTN
PsSysTimerControl (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine performs timer control operations.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PPROCESS_TIMER CurrentTimer;
    BOOL LockHeld;
    PSYSTEM_CALL_TIMER_CONTROL Parameters;
    PKPROCESS Process;
    KSTATUS Status;
    PKTHREAD Thread;
    PPROCESS_TIMER Timer;

    LockHeld = FALSE;
    Parameters = (PSYSTEM_CALL_TIMER_CONTROL)SystemCallParameter;
    Process = PsGetCurrentProcess();

    ASSERT(Process != PsGetKernelProcess());

    Timer = NULL;
    Thread = NULL;

    //
    // If it's not a create operation, find the timer being referenced.
    //

    if (Parameters->Operation != TimerOperationCreateTimer) {
        KeAcquireQueuedLock(Process->QueuedLock);
        LockHeld = TRUE;
        CurrentEntry = Process->TimerList.Next;
        while (CurrentEntry != &(Process->TimerList)) {
            CurrentTimer = LIST_VALUE(CurrentEntry, PROCESS_TIMER, ListEntry);
            if (CurrentTimer->TimerNumber == Parameters->TimerNumber) {
                Timer = CurrentTimer;
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (Timer == NULL) {
            Status = STATUS_INVALID_HANDLE;
            goto SysTimerControlEnd;
        }
    }

    Status = STATUS_SUCCESS;
    switch (Parameters->Operation) {

    //
    // Create a new processs timer and add it to the list.
    //

    case TimerOperationCreateTimer:

        //
        // If a thread is to be signaled, validate that the thread belongs to
        // the current process.
        //

        if ((Parameters->Flags & TIMER_CONTROL_FLAG_SIGNAL_THREAD) != 0) {
            Thread = PspGetThreadById(Process, Parameters->ThreadId);
            if (Thread == NULL) {
                Status = STATUS_INVALID_PARAMETER;
                goto SysTimerControlEnd;
            }
        }

        Status = PspCreateTimer(Process, Thread, &Timer);
        if (!KSUCCESS(Status)) {
            goto SysTimerControlEnd;
        }

        Timer->SignalQueueEntry.Parameters.SignalNumber =
                                                      Parameters->SignalNumber;

        Timer->SignalQueueEntry.Parameters.SignalCode = SIGNAL_CODE_TIMER;
        if ((Parameters->Flags & TIMER_CONTROL_FLAG_USE_TIMER_NUMBER) != 0) {
            Timer->SignalQueueEntry.Parameters.Parameter = Timer->TimerNumber;

        } else {
            Timer->SignalQueueEntry.Parameters.Parameter =
                                                       Parameters->SignalValue;
        }

        Parameters->TimerNumber = Timer->TimerNumber;
        break;

    //
    // Delete an existing process timer.
    //

    case TimerOperationDeleteTimer:
        LIST_REMOVE(&(Timer->ListEntry));
        KeReleaseQueuedLock(Process->QueuedLock);
        LockHeld = FALSE;
        PspFlushProcessTimer(Process, Timer);
        PspProcessTimerReleaseReference(Timer);
        break;

    //
    // Get timer information, including the next due time and overflow count.
    //

    case TimerOperationGetTimer:
        Parameters->TimerInformation.DueTime = KeGetTimerDueTime(Timer->Timer);
        Parameters->TimerInformation.Period = Timer->Interval;
        Parameters->TimerInformation.OverflowCount = Timer->OverflowCount;
        break;

    //
    // Arm or disarm the timer. Save and return the original information.
    //

    case TimerOperationSetTimer:
        Parameters->TimerInformation.OverflowCount = 0;
        Status = PspSetTimer(Process,
                             Timer,
                             &(Parameters->TimerInformation.DueTime),
                             &(Parameters->TimerInformation.Period));

        if (!KSUCCESS(Status)) {
            goto SysTimerControlEnd;
        }

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto SysTimerControlEnd;
    }

SysTimerControlEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Process->QueuedLock);
    }

    if (Thread != NULL) {
        ObReleaseReference(Thread);
    }

    return Status;
}

INTN
PsSysSetITimer (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine performs gets or sets a thread interval timer.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONGLONG CurrentCycles;
    ULONGLONG CurrentTime;
    ULONGLONG DueTime;
    ULONGLONG OldPeriod;
    PKPROCESS Process;
    PPROCESS_TIMER RealTimer;
    PSYSTEM_CALL_SET_ITIMER Request;
    KSTATUS Status;
    PKTHREAD Thread;
    RESOURCE_USAGE Usage;
    PRUNTIME_TIMER UserTimer;

    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    Request = SystemCallParameter;
    if (Request->Type >= ITimerTypeCount) {
        Status = STATUS_INVALID_PARAMETER;
        goto SysSetITimerEnd;
    }

    if (Request->Set == FALSE) {
        switch (Request->Type) {
        case ITimerReal:
            RealTimer = Thread->RealTimer;
            if (RealTimer == NULL) {
                Request->DueTime = 0;
                Request->Period = 0;
                break;
            }

            Request->DueTime = KeGetTimerDueTime(RealTimer->Timer);
            CurrentTime = HlQueryTimeCounter();
            if (Request->DueTime > CurrentTime) {
                Request->DueTime -= CurrentTime;

            } else {
                Request->DueTime = 0;
            }

            Request->Period = RealTimer->Interval;
            break;

        case ITimerVirtual:
        case ITimerProfile:
            PspGetThreadResourceUsage(Thread, &Usage);
            UserTimer = &(Thread->UserTimer);
            CurrentCycles = Usage.UserCycles;
            if (Request->Type == ITimerProfile) {
                CurrentCycles += Usage.KernelCycles;
                UserTimer = &(Thread->ProfileTimer);
            }

            Request->Period = UserTimer->Period;
            Request->DueTime = UserTimer->DueTime;
            if (Request->DueTime > CurrentCycles) {
                Request->DueTime -= CurrentCycles;

            } else {
                Request->DueTime = 0;
            }

            break;

        default:

            ASSERT(FALSE);

            break;
        }

        Status = STATUS_SUCCESS;
        goto SysSetITimerEnd;
    }

    //
    // This is a set timer request.
    //

    switch (Request->Type) {
    case ITimerReal:
        if (Thread->RealTimer == NULL) {
            Status = PspCreateTimer(Process, NULL, &RealTimer);
            if (!KSUCCESS(Status)) {
                goto SysSetITimerEnd;
            }

            RealTimer->SignalQueueEntry.Parameters.SignalNumber = SIGNAL_TIMER;
            RealTimer->SignalQueueEntry.Parameters.SignalCode =
                                                             SIGNAL_CODE_TIMER;

            Thread->RealTimer = RealTimer;
        }

        //
        // Set the new real timer. The due time in the request is always
        // relative, so convert it to absolute and back.
        //

        CurrentTime = HlQueryTimeCounter();
        KeAcquireQueuedLock(Process->QueuedLock);
        DueTime = Request->DueTime;
        if (DueTime != 0) {
            DueTime += CurrentTime;
        }

        Status = PspSetTimer(Process,
                             Thread->RealTimer,
                             &DueTime,
                             &(Request->Period));

        KeReleaseQueuedLock(Process->QueuedLock);
        if (!KSUCCESS(Status)) {
            goto SysSetITimerEnd;
        }

        if (DueTime > CurrentTime) {
            DueTime -= CurrentTime;

        } else {
            DueTime = 0;
        }

        break;

    case ITimerVirtual:
    case ITimerProfile:
        PspGetThreadResourceUsage(Thread, &Usage);
        UserTimer = &(Thread->UserTimer);
        CurrentCycles = Usage.UserCycles;
        if (Request->Type == ITimerProfile) {
            CurrentCycles += Usage.KernelCycles;
            UserTimer = &(Thread->ProfileTimer);
        }

        DueTime = Request->DueTime;
        if (DueTime != 0) {
            DueTime += CurrentCycles;
        }

        OldPeriod = UserTimer->Period;
        Request->DueTime = UserTimer->Period;
        if (Request->DueTime > CurrentCycles) {
            Request->DueTime -= CurrentCycles;

        } else {
            Request->DueTime = 0;
        }

        UserTimer->DueTime = DueTime;
        UserTimer->Period = Request->Period;
        Request->Period = OldPeriod;
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto SysSetITimerEnd;
    }

    Status = STATUS_SUCCESS;

SysSetITimerEnd:
    return Status;
}

VOID
PsEvaluateRuntimeTimers (
    PKTHREAD Thread
    )

/*++

Routine Description:

    This routine checks the runtime timers for expiration on the current thread.

Arguments:

    Thread - Supplies a pointer to the current thread.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentCycles;
    RESOURCE_USAGE Usage;

    //
    // If they're both zero, return.
    //

    if ((Thread->UserTimer.DueTime | Thread->ProfileTimer.DueTime) == 0) {
        return;
    }

    //
    // Potentially expire the user timer. This read can never tear since user
    // mode can't sneak in and run a bit more.
    //

    if ((Thread->UserTimer.DueTime != 0) &&
        (Thread->ResourceUsage.UserCycles >= Thread->UserTimer.DueTime)) {

        PspExpireRuntimeTimer(Thread,
                              &(Thread->UserTimer),
                              SIGNAL_EXECUTION_TIMER_EXPIRED,
                              Thread->ResourceUsage.UserCycles);
    }

    //
    // Potentially expire the profiling timer. The kernel time might tear, so
    // do a torn read and if it succeeds, do a legit read. If the torn read
    // results in a false negative then the timer will be a little late, but
    // will expire on the next check.
    //

    if ((Thread->ProfileTimer.DueTime != 0) &&
        ((Thread->ResourceUsage.UserCycles +
          Thread->ResourceUsage.KernelCycles) >=
         Thread->ProfileTimer.DueTime)) {

        PspGetThreadResourceUsage(Thread, &Usage);
        CurrentCycles = Usage.UserCycles + Usage.KernelCycles;
        if (CurrentCycles >= Thread->ProfileTimer.DueTime) {
            PspExpireRuntimeTimer(Thread,
                                  &(Thread->ProfileTimer),
                                  SIGNAL_PROFILE_TIMER,
                                  CurrentCycles);
        }
    }

    return;
}

VOID
PspDestroyProcessTimers (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine cleans up any timers a process may have. This routine assumes
    the process lock is already held.

Arguments:

    Process - Supplies a pointer to the process.

Return Value:

    None.

--*/

{

    PPROCESS_TIMER Timer;

    while (LIST_EMPTY(&(Process->TimerList)) == FALSE) {
        Timer = LIST_VALUE(Process->TimerList.Next, PROCESS_TIMER, ListEntry);
        LIST_REMOVE(&(Timer->ListEntry));

        //
        // Cancel the timer and flush the DPC to ensure that the reference
        // count is up to date. Then release the reference. This will either
        // clean up the object right away or the work item will run on its
        // own time.
        //

        KeCancelTimer(Timer->Timer);
        if (!KSUCCESS(KeCancelDpc(Timer->Dpc))) {
            KeFlushDpc(Timer->Dpc);
        }

        PspProcessTimerReleaseReference(Timer);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PspCreateTimer (
    PKPROCESS Process,
    PKTHREAD Thread,
    PPROCESS_TIMER *Timer
    )

/*++

Routine Description:

    This routine attempts to create and add a new process timer.

Arguments:

    Process - Supplies a pointer to the process that owns the timer.

    Thread - Supplies an optional pointer to the thread to be signaled when the
        timer expires.

    Timer - Supplies a pointer where a pointer to the new timer is returned on
        success.

Return Value:

    Status code.

--*/

{

    PPROCESS_TIMER PreviousTimer;
    PPROCESS_TIMER ProcessTimer;
    KSTATUS Status;

    Status = PspCreateProcessTimer(Process, Thread, &ProcessTimer);
    if (!KSUCCESS(Status)) {
        goto CreateTimerEnd;
    }

    ProcessTimer->SignalQueueEntry.Parameters.SignalCode = SIGNAL_CODE_TIMER;

    //
    // Insert this timer in the process. Assign the timer the ID of the
    // last timer in the list plus one.
    //

    KeAcquireQueuedLock(Process->QueuedLock);
    if (LIST_EMPTY(&(Process->TimerList)) != FALSE) {
        ProcessTimer->TimerNumber = 1;

    } else {
        PreviousTimer = LIST_VALUE(Process->TimerList.Previous,
                                   PROCESS_TIMER,
                                   ListEntry);

        ProcessTimer->TimerNumber = PreviousTimer->TimerNumber + 1;
    }

    INSERT_BEFORE(&(ProcessTimer->ListEntry), &(Process->TimerList));
    KeReleaseQueuedLock(Process->QueuedLock);

CreateTimerEnd:
    *Timer = ProcessTimer;
    return Status;
}

KSTATUS
PspSetTimer (
    PKPROCESS Process,
    PPROCESS_TIMER Timer,
    PULONGLONG DueTime,
    PULONGLONG Period
    )

/*++

Routine Description:

    This routine attempts to arm a process timer. This routien assumes the
    process lock is already held.

Arguments:

    Process - Supplies a pointer to the process that owns the timer.

    Timer - Supplies a pointer where a pointer to the new timer is returned on
        success.

    DueTime - Supplies the new due time in time counter ticks. Returns the
        previous due time.

    Period - Supplies the new interval in time counter ticks. Returns the
        previous interval.

Return Value:

    Status code.

--*/

{

    ULONGLONG OriginalDueTime;
    ULONGLONG OriginalPeriod;
    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(Process->QueuedLock) != FALSE);

    OriginalDueTime = KeGetTimerDueTime(Timer->Timer);
    OriginalPeriod = Timer->Interval;
    if (Timer->DueTime != 0) {
        KeCancelTimer(Timer->Timer);
    }

    Timer->DueTime = *DueTime;
    Timer->Interval = *Period;
    if (Timer->DueTime != 0) {
        Status = KeQueueTimer(Timer->Timer,
                              TimerQueueSoftWake,
                              Timer->DueTime,
                              Timer->Interval,
                              0,
                              Timer->Dpc);

        if (!KSUCCESS(Status)) {
            goto SetTimerEnd;
        }
    }

    Status = STATUS_SUCCESS;

SetTimerEnd:
    *DueTime = OriginalDueTime;
    *Period = OriginalPeriod;
    return Status;
}

KSTATUS
PspCreateProcessTimer (
    PKPROCESS Process,
    PKTHREAD Thread,
    PPROCESS_TIMER *Timer
    )

/*++

Routine Description:

    This routine attempts to create a new process timer.

Arguments:

    Process - Supplies a pointer to the process that owns the timer.

    Thread - Supplies an optional pointer to the thread to be signaled when the
        timer expires.

    Timer - Supplies a pointer where a pointer to the new timer is returned on
        success.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    PPROCESS_TIMER NewTimer;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_INSUFFICIENT_RESOURCES;
    NewTimer = MmAllocateNonPagedPool(sizeof(PROCESS_TIMER),
                                      PROCESS_TIMER_ALLOCATION_TAG);

    if (NewTimer == NULL) {
        goto CreateProcessTimerEnd;
    }

    RtlZeroMemory(NewTimer, sizeof(PROCESS_TIMER));
    NewTimer->Process = Process;
    NewTimer->Thread = Thread;
    NewTimer->ReferenceCount = 1;
    NewTimer->Timer = KeCreateTimer(PROCESS_TIMER_ALLOCATION_TAG);
    if (NewTimer->Timer == NULL) {
        goto CreateProcessTimerEnd;
    }

    NewTimer->Dpc = KeCreateDpc(PspProcessTimerDpcRoutine, NewTimer);
    if (NewTimer->Dpc == NULL) {
        goto CreateProcessTimerEnd;
    }

    NewTimer->WorkItem = KeCreateWorkItem(NULL,
                                          WorkPriorityNormal,
                                          PspProcessTimerWorkRoutine,
                                          NewTimer,
                                          PROCESS_TIMER_ALLOCATION_TAG);

    if (NewTimer->WorkItem == NULL) {
        goto CreateProcessTimerEnd;
    }

    NewTimer->SignalQueueEntry.CompletionRoutine =
                                               PspProcessTimerSignalCompletion;

    //
    // Take a reference on the process to avoid a situation where the
    // process is destroyed before the work item gets around to running. Do the
    // same for the thread if it is present.
    //

    ObAddReference(Process);
    if (Thread != NULL) {
        ObAddReference(Thread);
    }

    Status = STATUS_SUCCESS;

CreateProcessTimerEnd:
    if (!KSUCCESS(Status)) {
        if (NewTimer != NULL) {
            if (NewTimer->Timer != NULL) {
                KeDestroyTimer(NewTimer->Timer);
            }

            if (NewTimer->Dpc != NULL) {
                KeDestroyDpc(NewTimer->Dpc);
            }

            if (NewTimer->WorkItem != NULL) {
                KeDestroyWorkItem(NewTimer->WorkItem);
            }

            MmFreeNonPagedPool(NewTimer);
            NewTimer = NULL;
        }
    }

    *Timer = NewTimer;
    return Status;
}

VOID
PspProcessTimerAddReference (
    PPROCESS_TIMER Timer
    )

/*++

Routine Description:

    This routine adds a reference to a process timer.

Arguments:

    Timer - Supplies a pointer to the timer.

Return Value:

    None.

--*/

{

    RtlAtomicAdd32(&(Timer->ReferenceCount), 1);
    return;
}

VOID
PspProcessTimerReleaseReference (
    PPROCESS_TIMER Timer
    )

/*++

Routine Description:

    This routine releases a reference on a process timer.

Arguments:

    Timer - Supplies a pointer to the timer.

Return Value:

    None.

--*/

{

    if (RtlAtomicAdd32(&(Timer->ReferenceCount), -1) == 1) {
        PspDestroyProcessTimer(Timer);
    }

    return;
}

VOID
PspDestroyProcessTimer (
    PPROCESS_TIMER Timer
    )

/*++

Routine Description:

    This routine destroys a process timer.

Arguments:

    Timer - Supplies a pointer to the timer to destroy.

Return Value:

    None.

--*/

{

    KeDestroyTimer(Timer->Timer);
    KeDestroyDpc(Timer->Dpc);
    KeDestroyWorkItem(Timer->WorkItem);
    ObReleaseReference(Timer->Process);
    if (Timer->Thread != NULL) {
        ObReleaseReference(Timer->Thread);
    }

    MmFreeNonPagedPool(Timer);
    return;
}

VOID
PspFlushProcessTimer (
    PKPROCESS Process,
    PPROCESS_TIMER Timer
    )

/*++

Routine Description:

    This routine flushes a process timer to the point where the reference
    count is prepared for anyone about to release a reference, and the signal
    is either queued or cancelled.

Arguments:

    Process - Supplies a pointer to the process that owns the timer.

    Timer - Supplies a pointer to the timer to cancel/flush.

Return Value:

    None.

--*/

{

    //
    // After the timer's cancelled, the DPC is queued or it isn't going to be.
    //

    KeCancelTimer(Timer->Timer);

    //
    // Cancelling or flushing the DPC means that either the work item is queued
    // or isn't going to be.
    //

    if (!KSUCCESS(KeCancelDpc(Timer->Dpc))) {
        KeFlushDpc(Timer->Dpc);
    }

    //
    // After the work queue's flushed, either the signal is queued or it isn't
    // going to be.
    //

    KeFlushWorkQueue(NULL);

    //
    // Attempt to cancel the signal to prevent signals from coming in way
    // after the timer was deleted.
    //

    PspCancelQueuedSignal(Process, &(Timer->SignalQueueEntry));
    return;
}

VOID
PspProcessTimerDpcRoutine (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the DPC routine that fires when a process timer
    expires. It queues the work item.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    KSTATUS Status;
    PPROCESS_TIMER Timer;

    //
    // Increment the number of expirations, and queue the work item if this was
    // the first one.
    //

    Timer = (PPROCESS_TIMER)(Dpc->UserData);
    if (RtlAtomicAdd32(&(Timer->ExpirationCount), 1) == 0) {

        //
        // Increment the reference count to ensure this structure doesn't go
        // away while the signal is queued. Anybody trying to make the structure
        // go away needs to flush the DPC before decrementing their referecne
        // to ensure this gets a chance to run.
        //

        PspProcessTimerAddReference(Timer);
        Status = KeQueueWorkItem(Timer->WorkItem);

        ASSERT(KSUCCESS(Status));
    }

    return;
}

VOID
PspProcessTimerWorkRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the process timer expiration work routine.

Arguments:

    Parameter - Supplies a pointer to the process timer.

Return Value:

    None.

--*/

{

    ULONG ExpirationCount;
    PPROCESS_TIMER Timer;

    Timer = (PPROCESS_TIMER)Parameter;

    //
    // Read the current expiration count to determine how to set the overflow
    // count.
    //

    ExpirationCount = RtlAtomicOr32(&(Timer->ExpirationCount), 0);

    ASSERT(ExpirationCount != 0);

    Timer->OverflowCount = ExpirationCount - 1;
    Timer->SignalQueueEntry.Parameters.FromU.OverflowCount =
                                                          Timer->OverflowCount;

    if (Timer->Thread != NULL) {
        PsSignalThread(Timer->Thread,
                       Timer->SignalQueueEntry.Parameters.SignalNumber,
                       &(Timer->SignalQueueEntry),
                       FALSE);

    } else {
        PsSignalProcess(Timer->Process,
                        Timer->SignalQueueEntry.Parameters.SignalNumber,
                        &(Timer->SignalQueueEntry));
    }

    return;
}

VOID
PspProcessTimerSignalCompletion (
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    )

/*++

Routine Description:

    This routine is called when a process timer's signal was successfully
    completed in usermode.

Arguments:

    SignalQueueEntry - Supplies a pointer to the signal queue entry that was
        successfully sent to user mode.

Return Value:

    None.

--*/

{

    ULONG ExpirationCount;
    ULONG OverflowCount;
    PPROCESS_TIMER Timer;

    Timer = PARENT_STRUCTURE(SignalQueueEntry, PROCESS_TIMER, SignalQueueEntry);

    //
    // Slam a zero into the overflow count.
    //

    OverflowCount = Timer->OverflowCount;
    Timer->OverflowCount = 0;

    //
    // Subtract off the overflow count (plus one for the original non-overflow
    // expiration) from the expiration count.
    //

    OverflowCount += 1;
    ExpirationCount = RtlAtomicAdd32(&(Timer->ExpirationCount), -OverflowCount);

    ASSERT(ExpirationCount >= OverflowCount);

    //
    // If new intervals came in already, re-queue the work item immediately,
    // as the DPC is never going to.
    //

    if (ExpirationCount - OverflowCount != 0) {
        KeQueueWorkItem(Timer->WorkItem);

    //
    // Release the reference, until the next DPC runs all parties are done
    // touching this memory.
    //

    } else {
        PspProcessTimerReleaseReference(Timer);
    }

    return;
}

VOID
PspExpireRuntimeTimer (
    PKTHREAD Thread,
    PRUNTIME_TIMER Timer,
    ULONG Signal,
    ULONGLONG CurrentTime
    )

/*++

Routine Description:

    This routine is called when a runtime timer expires.

Arguments:

    Thread - Supplies a pointer to the current thread.

    Timer - Supplies a pointer to the thread's runtime timer that expired.

    Signal - Supplies the signal to send the current process.

    CurrentTime - Supplies the current user or user/kernel time, for rearming
        of periodic timers.

Return Value:

    None.

--*/

{

    ULONGLONG NextTime;

    //
    // Fire off a signal to the process as a whole.
    //

    PsSignalProcess(Thread->OwningProcess, Signal, NULL);

    //
    // Rearm the timer if it's periodic.
    //

    if (Timer->Period != 0) {
        NextTime = Timer->DueTime + Timer->Period;
        while ((NextTime > Timer->DueTime) && (NextTime <= CurrentTime)) {
            NextTime += Timer->Period;
        }

        if (NextTime <= Timer->DueTime) {
            NextTime = 0;
        }

        Timer->DueTime = NextTime;

    //
    // This was a one-shot timer. Disable it now.
    //

    } else {
        Timer->DueTime = 0;
    }

    return;
}

