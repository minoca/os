/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    signals.c

Abstract:

    This module implements support for sending signals to user mode.

Author:

    Evan Green 28-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include "psp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the set of signals that if not handled, ignored, or traced get
// bubbled up to kernel mode.
//

#define KERNEL_REPORTED_USER_SIGNALS                  \
    ((1 << (SIGNAL_ABORT - 1)) |                      \
     (1 << (SIGNAL_BUS_ERROR - 1)) |                  \
     (1 << (SIGNAL_MATH_ERROR - 1)) |                 \
     (1 << (SIGNAL_ILLEGAL_INSTRUCTION - 1)) |        \
     (1 << (SIGNAL_ACCESS_VIOLATION - 1)) |           \
     (1 << (SIGNAL_BAD_SYSTEM_CALL - 1)) |            \
     (1 << (SIGNAL_TRAP - 1)) |                       \
     (1 << (SIGNAL_REQUEST_CORE_DUMP - 1)) |          \
     (1 << (SIGNAL_FILE_SIZE_TOO_LARGE - 1)))

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _SEND_SIGNAL_ITERATOR_CONTEXT {
    PKTHREAD CurrentThread;
    PKPROCESS SkipProcess;
    ULONG Signal;
    PSIGNAL_QUEUE_ENTRY QueueEntry;
    BOOL CheckPermissions;
    ULONG SentSignals;
    KSTATUS Status;
} SEND_SIGNAL_ITERATOR_CONTEXT, *PSEND_SIGNAL_ITERATOR_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
PspCheckForNonMaskableSignals (
    PSIGNAL_PARAMETERS SignalParameters,
    PTRAP_FRAME TrapFrame
    );

VOID
PspQueueChildSignal (
    PKPROCESS Process,
    PKPROCESS Destination,
    UINTN ExitStatus,
    USHORT Reason
    );

PSIGNAL_QUEUE_ENTRY
PspGetChildSignalEntry (
    PROCESS_ID ProcessId,
    ULONG WaitFlags
    );

KSTATUS
PspValidateWaitParameters (
    PKPROCESS Process,
    LONG ProcessId
    );

BOOL
PspMatchChildWaitRequestWithProcessId (
    LONG WaitPidRequest,
    ULONG WaitFlags,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    );

VOID
PspChildSignalCompletionRoutine (
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    );

VOID
PspMarkThreadStopped (
    PKPROCESS Process,
    PBOOL FirstThread
    );

VOID
PspTracerBreak (
    PSIGNAL_PARAMETERS Signal,
    PTRAP_FRAME TrapFrame,
    BOOL ThreadAlreadyStopped,
    PBOOL ThreadStopHandled
    );

VOID
PspForwardUserModeExceptionToKernel (
    PSIGNAL_PARAMETERS Signal,
    PTRAP_FRAME TrapFrame
    );

VOID
PspQueueSignal (
    PKPROCESS Process,
    PKTHREAD Thread,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry,
    BOOL Force
    );

KSTATUS
PspSignalProcess (
    PKPROCESS Process,
    ULONG SignalNumber,
    USHORT SignalCode,
    UINTN SignalParameter
    );

BOOL
PspSendSignalIterator (
    PVOID Context,
    PKPROCESS Process
    );

KSTATUS
PspCheckSendSignalPermission (
    PKTHREAD CurrentThread,
    PKPROCESS Process,
    ULONG Signal
    );

VOID
PspUpdateSignalPending (
    VOID
    );

VOID
PspMoveSignalSet (
    SIGNAL_SET SignalSet
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR PsSignalNames[STANDARD_SIGNAL_COUNT] = {
    "0",
    "Hangup",
    "Interrupt",
    "Core Dump",
    "Illegal Instruction",
    "Trap",
    "Abort",
    "Bus Error",
    "Math Error",
    "Kill",
    "Application1",
    "Access Violation",
    "Application2",
    "Broken Pipe",
    "Timer",
    "Request Termination",
    "Child Process",
    "Continue",
    "Stop",
    "Request Stop",
    "Background Input",
    "Background Output",
    "Urgent Data",
    "CPU Quota",
    "File Size",
    "Execution Timer",
    "Profile Timer",
    "Window Change",
    "Asynchronous I/O",
    "Bad System Call",
    "30",
    "31"
};

//
// ------------------------------------------------------------------ Functions
//

VOID
PsSetSignalMask (
    PSIGNAL_SET NewMask,
    PSIGNAL_SET OriginalMask
    )

/*++

Routine Description:

    This routine sets the blocked signal mask for the current thread.

Arguments:

    NewMask - Supplies a pointer to the new mask to set.

    OriginalMask - Supplies an optional pointer to the previous mask.

Return Value:

    None.

--*/

{

    SIGNAL_SET NewBlockedSet;
    SIGNAL_SET NewMaskLocal;
    PKPROCESS Process;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;

    ASSERT(Process != PsGetKernelProcess());

    NewMaskLocal = *NewMask;
    REMOVE_SIGNAL(NewMaskLocal, SIGNAL_STOP);
    REMOVE_SIGNAL(NewMaskLocal, SIGNAL_KILL);
    REMOVE_SIGNAL(NewMaskLocal, SIGNAL_CONTINUE);
    KeAcquireQueuedLock(Process->QueuedLock);
    if (OriginalMask != NULL) {
        *OriginalMask = Thread->BlockedSignals;
    }

    NewBlockedSet = NewMaskLocal;
    REMOVE_SIGNALS_FROM_SET(NewBlockedSet, Thread->BlockedSignals);
    Thread->BlockedSignals = NewMaskLocal;

    //
    // Determine if the pending signal state needs to be changed now that the
    // blocked mask has been updated.
    //

    PspUpdateSignalPending();

    //
    // Move the newly blocked signals off to other threads.
    //

    PspMoveSignalSet(NewBlockedSet);
    KeReleaseQueuedLock(Process->QueuedLock);
    return;
}

INTN
PsSysSetSignalHandler (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine sets the user mode signal handler for the given thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSYSTEM_CALL_SET_SIGNAL_HANDLER Parameters;
    PVOID PreviousHandler;
    PKPROCESS Process;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Parameters = (PSYSTEM_CALL_SET_SIGNAL_HANDLER)SystemCallParameter;
    Process = PsGetCurrentProcess();
    KeAcquireQueuedLock(Process->QueuedLock);
    PreviousHandler = Process->SignalHandlerRoutine;
    Process->SignalHandlerRoutine = Parameters->SignalHandler;
    KeReleaseQueuedLock(Process->QueuedLock);
    Parameters->SignalHandler = PreviousHandler;
    return STATUS_SUCCESS;
}

INTN
PsSysRestoreContext (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine restores the original user mode thread context for the thread
    before the signal was invoked.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. The parameter is a user mode pointer to the signal
        context to restore.

Return Value:

    Returns the architecture-specific return register from the thread context.
    The architecture-specific system call assembly routines do not restore the
    return register out of the trap frame in order to allow a system call to
    return a value via a register. The restore context system call, however,
    must restore the old context, including the return register.

--*/

{

    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    return PspRestorePreSignalTrapFrame(Thread->TrapFrame, SystemCallParameter);
}

INTN
PsSysSendSignal (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call that allows usermode processes and
    threads to send signals to one another.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS CurrentProcess;
    PKTHREAD CurrentThread;
    SEND_SIGNAL_ITERATOR_CONTEXT Iterator;
    PKPROCESS KernelProcess;
    PROCESS_ID_TYPE MatchType;
    PKPROCESS Process;
    PSYSTEM_CALL_SEND_SIGNAL Request;
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry;
    KSTATUS Status;
    ULONG TargetId;
    PKTHREAD Thread;

    CurrentThread = KeGetCurrentThread();
    CurrentProcess = CurrentThread->OwningProcess;
    Request = (PSYSTEM_CALL_SEND_SIGNAL)SystemCallParameter;
    if (Request->SignalNumber >= SIGNAL_COUNT) {
        Status = STATUS_INVALID_PARAMETER;
        goto SendSignalEnd;
    }

    //
    // Only signals sent by the kernel can have positive values.
    //

    if (Request->SignalCode > 0) {
        Request->SignalCode = SIGNAL_CODE_USER;
    }

    TargetId = Request->TargetId;
    switch (Request->TargetType) {
    case SignalTargetThread:
        Process = CurrentProcess;
        if (TargetId == 0) {
            Thread = CurrentThread;
            ObAddReference(Thread);

        } else {
            Thread = PspGetThreadById(Process, TargetId);
        }

        if (Thread == NULL) {
            Status = STATUS_NO_SUCH_THREAD;
            goto SendSignalEnd;
        }

        Status = PspCheckSendSignalPermission(CurrentThread,
                                              Process,
                                              Request->SignalNumber);

        if (!KSUCCESS(Status)) {
            ObReleaseReference(Thread);
            break;
        }

        if (Request->SignalNumber < STANDARD_SIGNAL_COUNT) {
            if (Request->SignalNumber != 0) {
                PsSignalThread(Thread, Request->SignalNumber, NULL, FALSE);
            }

        } else {

            ASSERT(KeGetRunLevel() == RunLevelLow);

            SignalQueueEntry = MmAllocatePagedPool(sizeof(SIGNAL_QUEUE_ENTRY),
                                                   PS_ALLOCATION_TAG);

            if (SignalQueueEntry == NULL) {
                ObReleaseReference(Thread);
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto SendSignalEnd;
            }

            RtlZeroMemory(SignalQueueEntry, sizeof(SIGNAL_QUEUE_ENTRY));
            SignalQueueEntry->Parameters.SignalNumber = Request->SignalNumber;
            SignalQueueEntry->Parameters.SignalCode = Request->SignalCode;
            SignalQueueEntry->Parameters.FromU.SendingProcess =
                                                Process->Identifiers.ProcessId;

            SignalQueueEntry->Parameters.Parameter = Request->SignalParameter;
            SignalQueueEntry->Parameters.SendingUserId =
                                            CurrentThread->Identity.RealUserId;

            SignalQueueEntry->CompletionRoutine =
                                              PsDefaultSignalCompletionRoutine;

            PsSignalThread(Thread,
                           SignalQueueEntry->Parameters.SignalNumber,
                           SignalQueueEntry,
                           FALSE);
        }

        ObReleaseReference(Thread);
        Status = STATUS_SUCCESS;
        break;

    case SignalTargetCurrentProcessGroup:
    case SignalTargetProcessGroup:
    case SignalTargetAllProcesses:
        RtlZeroMemory(&Iterator, sizeof(SEND_SIGNAL_ITERATOR_CONTEXT));
        Iterator.CheckPermissions = TRUE;
        Iterator.Status = STATUS_SUCCESS;
        Iterator.Signal = Request->SignalNumber;
        MatchType = ProcessIdProcessGroup;
        if (Request->TargetType == SignalTargetAllProcesses) {
            TargetId = -1;
            MatchType = ProcessIdProcess;
            Iterator.SkipProcess = CurrentProcess;

        } else if (Request->TargetType == SignalTargetCurrentProcessGroup) {
            TargetId = CurrentProcess->Identifiers.ProcessGroupId;
        }

        PsIterateProcess(MatchType, TargetId, PspSendSignalIterator, &Iterator);
        Status = Iterator.Status;
        if ((KSUCCESS(Status)) && (Iterator.SentSignals == 0)) {
            Status = STATUS_NO_SUCH_PROCESS;
        }

        break;

    //
    // Handle cases that target a single process.
    //

    case SignalTargetCurrentProcess:
    case SignalTargetProcess:
        Process = CurrentProcess;
        if ((Request->TargetType != SignalTargetCurrentProcess) &&
            (Process->Identifiers.ProcessId != TargetId) &&
            (TargetId != 0)) {

            Process = PspGetProcessById(TargetId);
            if (Process == NULL) {
                Status = STATUS_NO_SUCH_PROCESS;
                goto SendSignalEnd;
            }

            KernelProcess = PsGetKernelProcess();
            if (Process == KernelProcess) {
                ObReleaseReference(Process);
                Status = STATUS_ACCESS_DENIED;
                goto SendSignalEnd;
            }
        }

        Status = PspCheckSendSignalPermission(CurrentThread,
                                              Process,
                                              Request->SignalNumber);

        if (KSUCCESS(Status)) {
            Status = PspSignalProcess(Process,
                                      Request->SignalNumber,
                                      Request->SignalCode,
                                      Request->SignalParameter);
        }

        if (Process != CurrentProcess) {
            ObReleaseReference(Process);
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

SendSignalEnd:
    return Status;
}

INTN
PsSysSetSignalBehavior (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call that allows a thread to set its
    varios signal behavior masks.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSIGNAL_SET DestinationSet;
    SIGNAL_SET NewBlockedSet;
    SIGNAL_SET NewMask;
    PSYSTEM_CALL_SET_SIGNAL_BEHAVIOR Parameters;
    PKPROCESS Process;
    PKTHREAD Thread;

    Parameters = (PSYSTEM_CALL_SET_SIGNAL_BEHAVIOR)SystemCallParameter;
    Thread = KeGetCurrentThread();
    Process = PsGetCurrentProcess();

    //
    // Remove the signals that can't be altered. Note that the continue signal
    // can be ignored or handled, but not blocked.
    //

    NewMask = Parameters->SignalSet;
    REMOVE_SIGNAL(NewMask, SIGNAL_STOP);
    REMOVE_SIGNAL(NewMask, SIGNAL_KILL);

    //
    // Get the signal mask to manipulate.
    //

    switch (Parameters->MaskType) {
    case SignalMaskBlocked:
        REMOVE_SIGNAL(NewMask, SIGNAL_CONTINUE);
        DestinationSet = &(Thread->BlockedSignals);
        break;

    case SignalMaskIgnored:
        DestinationSet = &(Process->IgnoredSignals);
        break;

    //
    // If the handled set is being manipulated, then clear the ignore bits to
    // avoid two system calls.
    //

    case SignalMaskHandled:
        DestinationSet = &(Process->HandledSignals);
        REMOVE_SIGNALS_FROM_SET(Process->IgnoredSignals, NewMask);
        break;

    case SignalMaskPending:
        DestinationSet = NULL;
        Parameters->Operation = SignalMaskOperationNone;
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Lock the proess and return the original mask.
    //

    KeAcquireQueuedLock(Process->QueuedLock);

    //
    // If this is the pending mask, just get the set of blocked pending signals
    // and return, ignoring the operation.
    //

    if (Parameters->MaskType == SignalMaskPending) {
        OR_SIGNAL_SETS(Parameters->SignalSet,
                       Thread->PendingSignals,
                       Process->PendingSignals);

        AND_SIGNAL_SETS(Parameters->SignalSet,
                        Parameters->SignalSet,
                        Thread->BlockedSignals);

        goto SetSignalBehaviorEnd;
    }

    Parameters->SignalSet = *DestinationSet;

    //
    // Change out the mask.
    //

    switch (Parameters->Operation) {
    case SignalMaskOperationOverwrite:
        *DestinationSet = NewMask;
        break;

    case SignalMaskOperationSet:
        OR_SIGNAL_SETS(*DestinationSet, *DestinationSet, NewMask);
        break;

    case SignalMaskOperationClear:
        REMOVE_SIGNALS_FROM_SET(*DestinationSet, NewMask);
        break;

    case SignalMaskOperationNone:
    default:
        break;
    }

    //
    // Re-queue all blocked signals that haven't already been delivered to see
    // if they might be deliverable now.
    //

    if ((Parameters->MaskType == SignalMaskBlocked) &&
        (Parameters->Operation != SignalMaskOperationNone)) {

        PspUpdateSignalPending();

        //
        // Move any newly blocked signals to other threads.
        //

        if (Parameters->Operation != SignalMaskOperationClear) {
            NewBlockedSet = NewMask;
            REMOVE_SIGNALS_FROM_SET(NewBlockedSet, Parameters->SignalSet);
            PspMoveSignalSet(NewBlockedSet);
        }
    }

SetSignalBehaviorEnd:
    KeReleaseQueuedLock(Process->QueuedLock);
    return STATUS_SUCCESS;
}

INTN
PsSysWaitForChildProcess (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call that suspends the current thread
    until a child process exits.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PKPROCESS ChildProcess;
    PSYSTEM_CALL_WAIT_FOR_CHILD Parameters;
    PSIGNAL_PARAMETERS SignalParameters;
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry;
    KSTATUS Status;
    PKTHREAD Thread;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // The caller must have specified one of the three required wait flags.
    //

    Parameters = (PSYSTEM_CALL_WAIT_FOR_CHILD)SystemCallParameter;
    if ((Parameters->Flags & SYSTEM_CALL_WAIT_FLAG_CHILD_MASK) == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto SysWaitForChildProcessEnd;
    }

    //
    // Loop attempting to service a signal and suspending until the thread
    // actually catches a signal.
    //

    SignalQueueEntry = NULL;
    Thread = KeGetCurrentThread();
    while (TRUE) {

        //
        // If there are no eligible child processes to wait for, break now.
        //

        Status = PspValidateWaitParameters(Thread->OwningProcess,
                                           Parameters->ChildPid);

        if (!KSUCCESS(Status)) {
            goto SysWaitForChildProcessEnd;
        }

        //
        // Attempt to pull a child signal off one of the queues.
        //

        SignalQueueEntry = PspGetChildSignalEntry(Parameters->ChildPid,
                                                  Parameters->Flags);

        if (SignalQueueEntry != NULL) {
            SignalParameters = &(SignalQueueEntry->Parameters);

            ASSERT(SignalParameters->SignalNumber ==
                   SIGNAL_CHILD_PROCESS_ACTIVITY);

            Parameters->ChildPid = SignalParameters->FromU.SendingProcess;
            Parameters->Reason = SignalParameters->SignalCode;

            ASSERT(Parameters->Reason != 0);

            Parameters->ChildExitValue = SignalParameters->Parameter;
            Status = STATUS_SUCCESS;
            if (Parameters->ResourceUsage != NULL) {
                ChildProcess = PARENT_STRUCTURE(SignalQueueEntry,
                                                KPROCESS,
                                                ChildSignal);

                Status = MmCopyToUserMode(Parameters->ResourceUsage,
                                          &(ChildProcess->ResourceUsage),
                                          sizeof(RESOURCE_USAGE));
            }

            //
            // Call the signal completion routine if this signal is being
            // discarded.
            //

            if ((SignalQueueEntry->ListEntry.Next == NULL) &&
                (SignalQueueEntry->CompletionRoutine != NULL)) {

                SignalQueueEntry->CompletionRoutine(SignalQueueEntry);
            }

            break;
        }

        //
        // If the caller wanted to return immediately and nothing was available,
        // then bail out now.
        //

        if ((Parameters->Flags &
             SYSTEM_CALL_WAIT_FLAG_RETURN_IMMEDIATELY) != 0) {

            Status = STATUS_NO_DATA_AVAILABLE;
            break;
        }

        //
        // Wake back up when something has changed.
        //

        PsCheckRuntimeTimers(Thread);
        KeSuspendExecution();

        //
        // Check for interruptions from the signal dispatch now that it's
        // known nothing was found. This needs to happen after the "return
        // immediately" breakout because many apps (such as make) expect that
        // if WNOHANG is set then EINTR will never be returned.
        //

        if (Thread->SignalPending == ThreadSignalPending) {
            Status = STATUS_RESTART_AFTER_SIGNAL;
            break;
        }
    }

SysWaitForChildProcessEnd:
    if (!KSUCCESS(Status)) {
        Parameters->ChildPid = -1;
    }

    return Status;
}

INTN
PsSysSuspendExecution (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call that suspends the current thread
    until a signal comes in.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    SIGNAL_SET BlockedSignals;
    ULONGLONG ElapsedTimeInMilliseconds;
    ULONGLONG EndTime;
    ULONGLONG Frequency;
    SIGNAL_SET NewBlockedSet;
    SIGNAL_SET OriginalMask;
    PSYSTEM_CALL_SUSPEND_EXECUTION Parameters;
    PKPROCESS Process;
    BOOL RestoreOriginalMask;
    ULONG SignalNumber;
    SIGNAL_PARAMETERS SignalParameters;
    ULONGLONG StartTime;
    KSTATUS Status;
    PKTHREAD Thread;
    ULONGLONG TimeoutInMicroseconds;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    INITIALIZE_SIGNAL_SET(OriginalMask);
    Parameters = (PSYSTEM_CALL_SUSPEND_EXECUTION)SystemCallParameter;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    RestoreOriginalMask = FALSE;
    SignalNumber = -1;
    Status = STATUS_SUCCESS;

    //
    // If requested, temporarily modify the signal mask for this call.
    //

    if (Parameters->SignalOperation != SignalMaskOperationNone) {

        //
        // Stop, kill and continue signals can never be blocked.
        //

        REMOVE_SIGNAL(Parameters->SignalSet, SIGNAL_STOP);
        REMOVE_SIGNAL(Parameters->SignalSet, SIGNAL_CONTINUE);
        REMOVE_SIGNAL(Parameters->SignalSet, SIGNAL_KILL);

        //
        // If the signal is going to be stolen, then attempt to dequeue it. The
        // given mask contains the set of signals that can be stolen. Call the
        // helper routine directly as this does not want to execute default
        // processing.
        //

        if (Parameters->SignalParameters != NULL) {
            BlockedSignals = Parameters->SignalSet;
            NOT_SIGNAL_SET(BlockedSignals);
            PsCheckRuntimeTimers(Thread);
            SignalNumber = PspDequeuePendingSignal(&SignalParameters,
                                                   Thread->TrapFrame,
                                                   &BlockedSignals);

            if (SignalNumber != -1) {
                Status = MmCopyToUserMode(Parameters->SignalParameters,
                                          &SignalParameters,
                                          sizeof(SIGNAL_PARAMETERS));

                goto SysSuspendExecutionEnd;
            }
        }

        //
        // Updates must be synchronized with the queueing of signals on this
        // thread. The operations acquire the process lock and will read and
        // modify the blocked signal set under that lock. And while the lock is
        // held, replay any blocked signals.
        //

        KeAcquireQueuedLock(Process->QueuedLock);
        OriginalMask = Thread->BlockedSignals;
        switch (Parameters->SignalOperation) {
        case SignalMaskOperationOverwrite:
            Thread->BlockedSignals = Parameters->SignalSet;
            break;

        case SignalMaskOperationClear:
            REMOVE_SIGNALS_FROM_SET(Thread->BlockedSignals,
                                    Parameters->SignalSet);

            break;

        case SignalMaskOperationSet:
            OR_SIGNAL_SETS(Thread->BlockedSignals,
                           Thread->BlockedSignals,
                           Parameters->SignalSet);

            break;

        default:
            Status = STATUS_NOT_IMPLEMENTED;
            break;
        }

        //
        // If something changed, requeue the blocked signals.
        //

        if (OriginalMask != Thread->BlockedSignals) {
            PspUpdateSignalPending();
            RestoreOriginalMask = TRUE;
            if (Parameters->SignalOperation != SignalMaskOperationClear) {
                NewBlockedSet = Parameters->SignalSet;
                REMOVE_SIGNALS_FROM_SET(NewBlockedSet, OriginalMask);
                PspMoveSignalSet(NewBlockedSet);
            }
        }

        KeReleaseQueuedLock(Process->QueuedLock);
        if (!KSUCCESS(Status)) {
            goto SysSuspendExecutionEnd;
        }
    }

    //
    // Wake back up when something has changed. Ignore child signals here.
    //

    Status = STATUS_RESTART_NO_SIGNAL;
    while (Thread->SignalPending != ThreadSignalPending) {
        PsCheckRuntimeTimers(Thread);
        if (Parameters->TimeoutInMilliseconds != SYS_WAIT_TIME_INDEFINITE) {
            StartTime = KeGetRecentTimeCounter();
            TimeoutInMicroseconds = Parameters->TimeoutInMilliseconds *
                                    MICROSECONDS_PER_MILLISECOND;

            //
            // Success on the interruptible wait is actually a timeout.
            //

            Status = KeDelayExecution(TRUE, FALSE, TimeoutInMicroseconds);
            if (KSUCCESS(Status)) {
                Status = STATUS_TIMEOUT;
                break;
            }

            if (Status != STATUS_INTERRUPTED) {
                break;
            }

            //
            // Calculate the time remaining in case this thread does not
            // dispatch the signal.
            //

            EndTime = KeGetRecentTimeCounter();
            Frequency = HlQueryTimeCounterFrequency();
            ElapsedTimeInMilliseconds = ((EndTime - StartTime) *
                                         MILLISECONDS_PER_SECOND) /
                                        Frequency;

            if (ElapsedTimeInMilliseconds < Parameters->TimeoutInMilliseconds) {
                Parameters->TimeoutInMilliseconds -= ElapsedTimeInMilliseconds;

            } else {
                Parameters->TimeoutInMilliseconds = 0;
            }

            Status = STATUS_RESTART_NO_SIGNAL;

        } else {
            KeSuspendExecution();
        }
    }

    //
    // The thread woke up because a signal came in. If signals are being stolen,
    // then check to see if there is a signal in the supplied mask. Always
    // restore the old mask before checking for stolen signals. Other attempts
    // to dispatch signals when this system call exits should only see the
    // original mask.
    //

    if ((Parameters->SignalOperation != SignalMaskOperationNone) &&
        (Parameters->SignalParameters != NULL)) {

        if (RestoreOriginalMask != FALSE) {
            PsSetSignalMask(&OriginalMask, NULL);
            RestoreOriginalMask = FALSE;
        }

        BlockedSignals = Parameters->SignalSet;
        NOT_SIGNAL_SET(BlockedSignals);
        SignalNumber = PspDequeuePendingSignal(&SignalParameters,
                                               Thread->TrapFrame,
                                               &BlockedSignals);

        if (SignalNumber != -1) {
            Status = MmCopyToUserMode(Parameters->SignalParameters,
                                      &SignalParameters,
                                      sizeof(SIGNAL_PARAMETERS));

            goto SysSuspendExecutionEnd;
        }
    }

SysSuspendExecutionEnd:

    //
    // Potentially restore the original signal mask. If a signal is pending,
    // then save it so the signal dispatcher eventually restores it. Otherwise
    // restore it now.
    //

    if (RestoreOriginalMask != FALSE) {
        if (Thread->SignalPending == ThreadSignalPending) {
            Thread->RestoreSignals = OriginalMask;
            Thread->Flags |= THREAD_FLAG_RESTORE_SIGNALS;

        } else {
            PsSetSignalMask(&OriginalMask, NULL);
        }
    }

    return Status;
}

VOID
PsSignalThread (
    PKTHREAD Thread,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry,
    BOOL Force
    )

/*++

Routine Description:

    This routine sends a signal to the given thread.

Arguments:

    Thread - Supplies a pointer to the thread to send the signal to.

    SignalNumber - Supplies the signal number to send.

    SignalQueueEntry - Supplies an optional pointer to a queue entry to place
        on the thread's queue.

    Force - Supplies a boolean that if set indicates the thread cannot block
        or ignore this signal.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Some signals are really only delivered to the process as a whole.
    //

    if ((SignalNumber == SIGNAL_STOP) ||
        (SignalNumber == SIGNAL_KILL) ||
        (SignalNumber == SIGNAL_CONTINUE)) {

        if (SignalNumber == SIGNAL_KILL) {
            PspSetProcessExitStatus(Thread->OwningProcess,
                                    CHILD_SIGNAL_REASON_KILLED,
                                    SIGNAL_KILL);
        }

        PsSignalProcess(Thread->OwningProcess, SignalNumber, SignalQueueEntry);
        return;
    }

    KeAcquireQueuedLock(Thread->OwningProcess->QueuedLock);
    PspQueueSignal(Thread->OwningProcess,
                   Thread,
                   SignalNumber,
                   SignalQueueEntry,
                   Force);

    KeReleaseQueuedLock(Thread->OwningProcess->QueuedLock);
    return;
}

VOID
PsSignalProcess (
    PKPROCESS Process,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    )

/*++

Routine Description:

    This routine sends a signal to the given process.

Arguments:

    Process - Supplies a pointer to the process to send the signal to.

    SignalNumber - Supplies the signal number to send.

    SignalQueueEntry - Supplies an optional pointer to a queue entry to place
        on the process' queue.

Return Value:

    None.

--*/

{

    BOOL ExecuteCompletionRoutine;

    ExecuteCompletionRoutine = FALSE;

    //
    // If a kill signal is being set, the exit flags had better be correctly
    // prepared.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(SignalNumber < SIGNAL_COUNT);
    ASSERT((SignalQueueEntry == NULL) ||
           (SignalNumber == SignalQueueEntry->Parameters.SignalNumber));

    if (SignalNumber == SIGNAL_KILL) {
        PspSetProcessExitStatus(Process,
                                CHILD_SIGNAL_REASON_KILLED,
                                SIGNAL_KILL);
    }

    KeAcquireQueuedLock(Process->QueuedLock);

    //
    // If there are no more threads in the process to service signals, then
    // just complete the signal now. If it's a child signal, execute the
    // completion routine outside the lock.
    //

    if (Process->ThreadCount == 0) {
        if ((SignalQueueEntry != NULL) &&
            (SignalQueueEntry->CompletionRoutine != NULL)) {

            ASSERT(SignalQueueEntry->ListEntry.Next == NULL);

            if (SignalNumber != SIGNAL_CHILD_PROCESS_ACTIVITY) {
                SignalQueueEntry->CompletionRoutine(SignalQueueEntry);

            } else {
                ExecuteCompletionRoutine = TRUE;
            }
        }

        goto SignalProcessEnd;
    }

    if ((SignalNumber == SIGNAL_STOP) ||
        (SignalNumber == SIGNAL_KILL) ||
        (SignalNumber == SIGNAL_CONTINUE)) {

        if (SignalNumber == SIGNAL_STOP) {

            //
            // Don't allow a process to stop if it has already been killed.
            //

            if (IS_SIGNAL_SET(Process->PendingSignals, SIGNAL_KILL) == FALSE) {
                REMOVE_SIGNAL(Process->PendingSignals, SIGNAL_CONTINUE);
                KeSignalEvent(Process->StopEvent, SignalOptionUnsignal);
            }

        } else if (SignalNumber == SIGNAL_CONTINUE) {
            REMOVE_SIGNAL(Process->PendingSignals, SIGNAL_STOP);
            KeSignalEvent(Process->StopEvent, SignalOptionSignalAll);

        } else if (SignalNumber == SIGNAL_KILL) {
            REMOVE_SIGNAL(Process->PendingSignals, SIGNAL_STOP);
            REMOVE_SIGNAL(Process->PendingSignals, SIGNAL_CONTINUE);
            KeSignalEvent(Process->StopEvent, SignalOptionSignalAll);
        }
    }

    PspQueueSignal(Process, NULL, SignalNumber, SignalQueueEntry, FALSE);

SignalProcessEnd:
    KeReleaseQueuedLock(Process->QueuedLock);
    if (ExecuteCompletionRoutine != FALSE) {
        SignalQueueEntry->CompletionRoutine(SignalQueueEntry);
    }

    return;
}

KSTATUS
PsSignalProcessId (
    PROCESS_ID ProcessId,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    )

/*++

Routine Description:

    This routine sends a signal to the given process.

Arguments:

    ProcessId - Supplies the identifier of the process to send the signal to.

    SignalNumber - Supplies the signal number to send.

    SignalQueueEntry - Supplies an optional pointer to a queue entry to place
        on the process' queue.

Return Value:

    None.

--*/

{

    PKPROCESS Process;

    Process = PspGetProcessById(ProcessId);
    if (Process == NULL) {
        return STATUS_NO_SUCH_PROCESS;
    }

    PsSignalProcess(Process, SignalNumber, SignalQueueEntry);
    ObReleaseReference(Process);
    return STATUS_SUCCESS;
}

KSTATUS
PsSignalAllProcesses (
    BOOL FromKernel,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY QueueEntry
    )

/*++

Routine Description:

    This routine sends a signal to every process currently in the system
    (except the kernel process). Processes created during the execution of this
    call may not receive the signal. This routine is used mainly during system
    shutdown.

Arguments:

    FromKernel - Supplies a boolean indicating whether the origin of the signal
        is the the kernel or not. Permissions are not checked if the origin
        is the kernel.

    SignalNumber - Supplies the signal number to send.

    QueueEntry - Supplies an optional pointer to the queue structure to send.
        A copy of this memory will be made in paged pool for each process a
        signal is sent to.

Return Value:

    STATUS_SUCCESS if some processes were signaled.

    STATUS_PERMISSION_DENIED if the caller did not have permission to signal
        some of the processes.

    STATUS_INSUFFICIENT_RESOURCES if there was not enough memory to enumerate
    all the processes in the system.

--*/

{

    SEND_SIGNAL_ITERATOR_CONTEXT Iterator;

    RtlZeroMemory(&Iterator, sizeof(SEND_SIGNAL_ITERATOR_CONTEXT));
    Iterator.Signal = SignalNumber;
    Iterator.QueueEntry = QueueEntry;
    if (FromKernel == FALSE) {
        Iterator.CheckPermissions = TRUE;
        Iterator.SkipProcess = PsGetCurrentProcess();
    }

    Iterator.Status = STATUS_SUCCESS;
    PsIterateProcess(ProcessIdProcess, -1, PspSendSignalIterator, &Iterator);
    return Iterator.Status;
}

BOOL
PsIsThreadAcceptingSignal (
    PKTHREAD Thread,
    ULONG SignalNumber
    )

/*++

Routine Description:

    This routine determines if the given thread is currently accepting a given
    signal, or if it is being either blocked or ignored.

Arguments:

    Thread - Supplies a pointer to the process to query. If NULL is supplied
        the current thread will be used.

    SignalNumber - Supplies the signal number to check.

Return Value:

    TRUE if the process has the signal action set to either default or a
    handler.

    FALSE if the signal is currently blocked or ignored.

--*/

{

    PKPROCESS Process;

    if (Thread == NULL) {
        Thread = KeGetCurrentThread();
    }

    Process = Thread->OwningProcess;

    ASSERT((Thread->Flags & THREAD_FLAG_USER_MODE) != 0);
    ASSERT(Process != PsGetKernelProcess());

    if (IS_SIGNAL_BLOCKED(Thread, SignalNumber) != FALSE) {
        return FALSE;
    }

    if (IS_SIGNAL_SET(Process->IgnoredSignals, SignalNumber) != FALSE) {
        return FALSE;
    }

    return TRUE;
}

VOID
PsDefaultSignalCompletionRoutine (
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    )

/*++

Routine Description:

    This routine implements the default signal completion routine, which
    simply frees the signal queue entry from paged pool. The caller should
    not touch the signal queue entry after this routine has returned, as it's
    gone back to the pool.

Arguments:

    SignalQueueEntry - Supplies a pointer to the signal queue entry that just
        completed.

Return Value:

    None.

--*/

{

    MmFreePagedPool(SignalQueueEntry);
    return;
}

KSTATUS
PspCancelQueuedSignal (
    PKPROCESS Process,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    )

/*++

Routine Description:

    This routine attempts to cancel a queued signal. This only works in
    specific circumstances where it's known that the signal queue entry cannot
    be freed or queued to a different process during this time.

Arguments:

    Process - Supplies a pointer to the process the signal is on.

    SignalQueueEntry - Supplies a pointer to the entry to attempt to remove.

Return Value:

    STATUS_SUCCESS if the signal was successfully removed. The completion
    routine will be run in this case.

    STATUS_TOO_LATE if the signal is already in service or was previously
    serviced.

--*/

{

    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_TOO_LATE;
    KeAcquireQueuedLock(Process->QueuedLock);
    if (SignalQueueEntry->ListEntry.Next != NULL) {
        LIST_REMOVE(&(SignalQueueEntry->ListEntry));
        SignalQueueEntry->ListEntry.Next = NULL;
        Status = STATUS_SUCCESS;
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    if (KSUCCESS(Status)) {
        if (SignalQueueEntry->CompletionRoutine != NULL) {
            SignalQueueEntry->CompletionRoutine(SignalQueueEntry);
        }
    }

    return Status;
}

BOOL
PsDispatchPendingSignalsOnCurrentThread (
    PTRAP_FRAME TrapFrame,
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine dispatches any pending signals that should be run on the
    current thread.

Arguments:

    TrapFrame - Supplies a pointer to the current trap frame. If this trap frame
        is not destined for user mode, this function exits immediately.

    SystemCallNumber - Supplies the number of the system call that is
        attempting to dispatch a pending signal. Supply SystemCallInvalid if
        the caller is not a system call.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call that is attempting to dispatch a signal. Supply NULL if
        the caller is not a system call.

Return Value:

    FALSE if no signals are pending.

    TRUE if a signal was applied.

--*/

{

    BOOL Applied;
    ULONG SignalNumber;
    SIGNAL_PARAMETERS SignalParameters;
    PKTHREAD Thread;

    Applied = FALSE;
    while (TRUE) {
        SignalNumber = PsDequeuePendingSignal(&SignalParameters, TrapFrame);
        if (SignalNumber == -1) {
            break;
        }

        Applied = TRUE;
        PsApplySynchronousSignal(TrapFrame,
                                 &SignalParameters,
                                 SystemCallNumber,
                                 SystemCallParameter);
    }

    //
    // If a signal did not get applied, restore the signal mask if necessary
    // and potentially restart the system call.
    //

    if (SystemCallNumber != SystemCallInvalid) {
        if (Applied == FALSE) {
            Thread = KeGetCurrentThread();
            if ((Thread->Flags & THREAD_FLAG_RESTORE_SIGNALS) != 0) {
                Thread->Flags &= ~THREAD_FLAG_RESTORE_SIGNALS;
                PsSetSignalMask(&(Thread->RestoreSignals), NULL);
            }

            PspArchRestartSystemCall(TrapFrame,
                                     SystemCallNumber,
                                     SystemCallParameter);
        }
    }

    return Applied;
}

ULONG
PspDequeuePendingSignal (
    PSIGNAL_PARAMETERS SignalParameters,
    PTRAP_FRAME TrapFrame,
    PSIGNAL_SET BlockedSignalsOverride
    )

/*++

Routine Description:

    This routine gets and clears the first signal in the thread or process
    signal mask of the current thread. For stop or terminate signals, this
    routine will act on the signal.

Arguments:

    SignalParameters - Supplies a pointer to a caller-allocated structure where
        the signal parameter information might get returned.

    TrapFrame - Supplies a pointer to the user mode trap that got execution
        into kernel mode.

    BlockedSignalsOverride - Supplies an optional pointer to a set of signals
        that replaces the blocked signal set during this dequeue.

Return Value:

    Returns the signal number of the first pending signal.

    -1 if no signals are pending.

--*/

{

    PSIGNAL_SET BlockedSignals;
    SIGNAL_SET CombinedSignalMask;
    PLIST_ENTRY CurrentEntry;
    ULONG DequeuedSignal;
    PSIGNAL_QUEUE_ENTRY FoundSignal;
    PLIST_ENTRY ListHead;
    PKPROCESS Process;
    SIGNAL_SET ProcessSignalMask;
    ULONG QueueLoop;
    PSIGNAL_QUEUE_ENTRY SignalEntry;
    BOOL SignalHandled;
    ULONG SignalNumber;
    BOOL StillPending;
    PKTHREAD Thread;
    SIGNAL_SET ThreadSignalMask;

    DequeuedSignal = -1;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;

    //
    // Don't look at or modify the pending state if a blocked signal override
    // is provided.
    //

    BlockedSignals = BlockedSignalsOverride;
    if (BlockedSignals == NULL) {
        if (Thread->SignalPending != ThreadSignalPending) {
            return -1;
        }

        DequeuedSignal = PspCheckForNonMaskableSignals(SignalParameters,
                                                       TrapFrame);

        if (DequeuedSignal != -1) {
            return DequeuedSignal;
        }

        //
        // Clear the pending signals flag, as they're about to get dealt with.
        // Any new signals added after this point will set the flag.
        //

        Thread->SignalPending = ThreadNoSignalPending;
        RtlMemoryBarrier();

        //
        // Use the thread's default blocked list if an override is not supplied.
        //

        BlockedSignals = &(Thread->BlockedSignals);
    }

    //
    // Perform a preliminary check without the lock held. While this can't say
    // for sure that there is a signal, it can say for sure if there isn't.
    //

    if ((IS_SIGNAL_SET_EMPTY(Thread->PendingSignals) != FALSE) &&
        (IS_SIGNAL_SET_EMPTY(Process->PendingSignals) != FALSE)) {

        return -1;
    }

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Acquire the lock and get the pending bits. If stop or kill is in there,
    // go handle those and try again.
    //

    while (TRUE) {
        KeAcquireQueuedLock(Process->QueuedLock);
        ThreadSignalMask = Thread->PendingSignals;
        ProcessSignalMask = Process->PendingSignals;
        OR_SIGNAL_SETS(CombinedSignalMask, ThreadSignalMask, ProcessSignalMask);
        REMOVE_SIGNALS_FROM_SET(CombinedSignalMask, *BlockedSignals);
        if ((IS_SIGNAL_SET(CombinedSignalMask, SIGNAL_STOP)) ||
            (IS_SIGNAL_SET(CombinedSignalMask, SIGNAL_KILL))) {

            KeReleaseQueuedLock(Process->QueuedLock);
            DequeuedSignal = PspCheckForNonMaskableSignals(SignalParameters,
                                                           TrapFrame);

            if (DequeuedSignal != -1) {

                //
                // A stop signal was likely removed. Update the pending signal
                // state to make sure the next attempt to dequeue a signal does
                // the correct work.
                //

                KeAcquireQueuedLock(Process->QueuedLock);
                PspUpdateSignalPending();
                KeReleaseQueuedLock(Process->QueuedLock);
                return DequeuedSignal;
            }

            continue;
        }

        break;
    }

    //
    // Loop trying to dispatch a signal as long as there is something in the
    // combined mask.
    //

    DequeuedSignal = -1;
    while (IS_SIGNAL_SET_EMPTY(CombinedSignalMask) == FALSE) {
        SignalNumber = RtlCountTrailingZeros64(CombinedSignalMask) + 1;

        //
        // Attempt to find the signal in the lists of signals while checking to
        // see if there is an additional signal pending.
        //

        for (QueueLoop = 0; QueueLoop < 2; QueueLoop += 1) {
            if (QueueLoop == 0) {
                ListHead = &(Thread->SignalListHead);

            } else {
                ListHead = &(Process->SignalListHead);
            }

            FoundSignal = NULL;
            StillPending = FALSE;
            CurrentEntry = ListHead->Next;
            while (CurrentEntry != ListHead) {
                SignalEntry = LIST_VALUE(CurrentEntry,
                                         SIGNAL_QUEUE_ENTRY,
                                         ListEntry);

                CurrentEntry = CurrentEntry->Next;
                if (SignalEntry->Parameters.SignalNumber != SignalNumber) {
                    continue;
                }

                if (FoundSignal != NULL) {
                    StillPending = TRUE;
                    break;
                }

                SignalHandled = IS_SIGNAL_SET(Process->HandledSignals,
                                              SignalNumber);

                //
                // If the signal is on the queue, it's assumed to not be
                // ignored. If it's not handled and the default action is to
                // ignore it, then delete this signal now. Don't listen to the
                // default handling if a blocked signal override was provided;
                // any pending signal gets dequeued in that case.
                //

                if ((BlockedSignalsOverride == NULL) &&
                    (SignalHandled == FALSE) &&
                    (IS_SIGNAL_DEFAULT_IGNORE(SignalNumber))) {

                    LIST_REMOVE(&(SignalEntry->ListEntry));
                    SignalEntry->ListEntry.Next = NULL;

                    //
                    // Let the debugger have a go at it.
                    //

                    if ((Process->DebugData != NULL) &&
                        (Process->DebugData->TracingProcess != NULL)) {

                        KeReleaseQueuedLock(Process->QueuedLock);
                        PspTracerBreak(&(SignalEntry->Parameters),
                                       TrapFrame,
                                       FALSE,
                                       NULL);

                        KeAcquireQueuedLock(Process->QueuedLock);
                    }

                    //
                    // Child signals are moved onto the unreaped list so they
                    // can get picked up by wait.
                    //

                    if (IS_CHILD_SIGNAL(SignalEntry)) {
                        INSERT_BEFORE(&(SignalEntry->ListEntry),
                                      &(Process->UnreapedChildList));

                    //
                    // Discard the signal entry.
                    //

                    } else {
                        if (SignalEntry->CompletionRoutine != NULL) {
                            SignalEntry->CompletionRoutine(SignalEntry);
                        }
                    }

                //
                // This signal is not discarded. Take it.
                //

                } else {
                    LIST_REMOVE(&(SignalEntry->ListEntry));
                    SignalEntry->ListEntry.Next = NULL;
                    FoundSignal = SignalEntry;
                }
            }

            //
            // If a signal was found on this queue, take it and attempt to
            // dispatch it.
            //

            if (FoundSignal != NULL) {

                //
                // If a second signal with the same number was not found on the
                // queue, then clear the pending bit from the masks.
                //

                if (StillPending == FALSE) {
                    if (QueueLoop == 0) {
                        REMOVE_SIGNAL(ThreadSignalMask, SignalNumber);
                        REMOVE_SIGNAL(Thread->PendingSignals, SignalNumber);

                    } else {
                        REMOVE_SIGNAL(ProcessSignalMask, SignalNumber);
                        REMOVE_SIGNAL(Process->PendingSignals, SignalNumber);
                    }
                }

                break;
            }
        }

        //
        // If the signal was not found on the queue, always remove it from the
        // pending masks.
        //

        if (FoundSignal == NULL) {
            if (IS_SIGNAL_SET(ThreadSignalMask, SignalNumber)) {
                REMOVE_SIGNAL(ThreadSignalMask, SignalNumber);
                REMOVE_SIGNAL(Thread->PendingSignals, SignalNumber);

            } else {

                ASSERT(IS_SIGNAL_SET(ProcessSignalMask, SignalNumber));

                REMOVE_SIGNAL(ProcessSignalMask, SignalNumber);
                REMOVE_SIGNAL(Process->PendingSignals, SignalNumber);
            }
        }

        //
        // The pending signal masks were changed. Update the signal pending
        // state.
        //

        PspUpdateSignalPending();

        //
        // Release the process lock and hopefully dequeue the signal.
        //

        KeReleaseQueuedLock(Process->QueuedLock);

        //
        // If no signal was found on the queues, then it is a basic signal
        // without the signal information. Use the caller allocated temporary
        // structure to create signal information.
        //

        if (FoundSignal == NULL) {
            RtlZeroMemory(SignalParameters, sizeof(SIGNAL_PARAMETERS));
            SignalParameters->SignalNumber = SignalNumber;

        } else {
            RtlCopyMemory(SignalParameters,
                          &(FoundSignal->Parameters),
                          sizeof(SIGNAL_PARAMETERS));

            //
            // The queue entry is no longer needed. If it's a child move it to
            // the unreaped list to be picked up by wait.
            //

            if (IS_CHILD_SIGNAL(FoundSignal)) {
                INSERT_BEFORE(&(FoundSignal->ListEntry),
                              &(Process->UnreapedChildList));

            //
            // Otherwise, call the completion routine.
            //

            } else if (FoundSignal->CompletionRoutine != NULL) {
                FoundSignal->CompletionRoutine(FoundSignal);
            }
        }

        //
        // Allow a tracer a chance to ignore the signal unless a blocked
        // signals override was provided.
        //

        if (BlockedSignalsOverride == NULL) {
            PspTracerBreak(SignalParameters, TrapFrame, FALSE, NULL);
        }

        DequeuedSignal = SignalParameters->SignalNumber;
        if (DequeuedSignal != 0) {

            //
            // If this was and still is a continue signal, then alert
            // the parent. Skip this if the parent is already tracing
            // the process.
            //

            if ((SignalNumber == SIGNAL_CONTINUE) &&
                (SignalNumber == DequeuedSignal) &&
                ((Process->DebugData == NULL) ||
                 (Process->DebugData->TracingProcess != Process->Parent))) {

                PspQueueChildSignalToParent(
                                       Process,
                                       SIGNAL_CONTINUE,
                                       CHILD_SIGNAL_REASON_CONTINUED);
            }

            goto DequeuePendingSignalEnd;
        }

        //
        // Update the local combined pending signal mask and try again. This
        // avoids processing newly arriving signals and keeps dequeue moving
        // forward through the mask.
        //

        DequeuedSignal = -1;
        KeAcquireQueuedLock(Process->QueuedLock);
        OR_SIGNAL_SETS(CombinedSignalMask, ThreadSignalMask, ProcessSignalMask);
        REMOVE_SIGNALS_FROM_SET(CombinedSignalMask, *BlockedSignals);
    }

    //
    // Make sure the signal pending status is up to date. It could be out of
    // date if a stop signal got passed over by the tracer above.
    //

    PspUpdateSignalPending();
    KeReleaseQueuedLock(Process->QueuedLock);

DequeuePendingSignalEnd:
    return DequeuedSignal;
}

BOOL
PspQueueChildSignalToParent (
    PKPROCESS Process,
    UINTN ExitStatus,
    USHORT Reason
    )

/*++

Routine Description:

    This routine queues the child signal the given process' parent, indicating
    the process has terminated, stopped, or continued.

Arguments:

    Process - Supplies a pointer to the child process that just exited, stopped,
        or continued.

    ExitStatus - Supplies the exit status on graceful exits or the signal number
        that caused the termination.

    Reason - Supplies the reason for the child signal.

Return Value:

    Returns TRUE if the signal was queued to the parent or FALSE otherwise.

--*/

{

    PKPROCESS Parent;
    BOOL SignalQueued;

    KeAcquireQueuedLock(Process->QueuedLock);
    Parent = Process->Parent;
    if (Parent != NULL) {
        ObAddReference(Parent);
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    SignalQueued = FALSE;
    if (Parent != NULL) {
        PspQueueChildSignal(Process, Parent, ExitStatus, Reason);
        ObReleaseReference(Parent);
        SignalQueued = TRUE;
    }

    return SignalQueued;
}

BOOL
PspSignalAttemptDefaultProcessing (
    ULONG Signal
    )

/*++

Routine Description:

    This routine check to see if a signal is marked to be ignored or provide
    the default action, and if so perfoms those actions.

Arguments:

    Signal - Supplies the pending signal number.

Return Value:

    TRUE if the signal was handled by this routine and there's no need to go
    to user mode with it.

    FALSE if this routine did not handle the signal and it should be dealt with
    in user mode.

--*/

{

    PKPROCESS Process;
    BOOL Result;
    ULONG SendSignal;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;

    //
    // Handle the non-maskable signals. Stop and kill are always marked
    // handled. Normally the check for non-maskable signals function filters
    // these out, but this path lights up if a normal signal is turned into a
    // non-maskable one by the debugger process.
    //

    if ((Signal == SIGNAL_KILL) ||
        (Signal == SIGNAL_STOP)) {

        return TRUE;
    }

    //
    // The signal is assumed not to be ignored if it got this far.
    // If the signal is set to have a handler, then it must go to user mode.
    //

    if ((IS_SIGNAL_SET(Process->HandledSignals, Signal) != FALSE) &&
        (Process->SignalHandlerRoutine != NULL)) {

        return FALSE;
    }

    //
    // Continue is non-maskable but different in that it can be handled and
    // "ignored". It is never actually ignored, however, as the act of queuing
    // should have already continued the process. If a handler is set, then
    // that handler can be set to be ignored. Those checks are handled above.
    // If a continue makes it this far, however, act like it was handled.
    //

    if (Signal == SIGNAL_CONTINUE) {
        return TRUE;
    }

    //
    // Do nothing for child signals if they are not handled.
    //

    if (IS_SIGNAL_DEFAULT_IGNORE(Signal)) {
        return TRUE;
    }

    Result = FALSE;
    SendSignal = 0;
    KeAcquireQueuedLock(Process->QueuedLock);

    //
    // Apply the default action here, which depends on the signal. Start by
    // processing the signals whose default action is to abort.
    //

    if ((Signal == SIGNAL_ABORT) ||
        (Signal == SIGNAL_BUS_ERROR) ||
        (Signal == SIGNAL_MATH_ERROR) ||
        (Signal == SIGNAL_ILLEGAL_INSTRUCTION) ||
        (Signal == SIGNAL_REQUEST_CORE_DUMP) ||
        (Signal == SIGNAL_ACCESS_VIOLATION) ||
        (Signal == SIGNAL_BAD_SYSTEM_CALL) ||
        (Signal == SIGNAL_TRAP) ||
        (Signal == SIGNAL_CPU_QUOTA_REACHED) ||
        (Signal == SIGNAL_FILE_SIZE_TOO_LARGE)) {

        Process->ExitReason = CHILD_SIGNAL_REASON_DUMPED;
        Process->ExitStatus = Signal;
        SendSignal = SIGNAL_KILL;
        Result = TRUE;

    //
    // Process the signals whose default action is to terminate.
    //

    } else if ((Signal == SIGNAL_TIMER) ||
               (Signal == SIGNAL_CONTROLLING_TERMINAL_CLOSED) ||
               (Signal == SIGNAL_KEYBOARD_INTERRUPT) ||
               (Signal == SIGNAL_BROKEN_PIPE) ||
               (Signal == SIGNAL_REQUEST_TERMINATION) ||
               (Signal == SIGNAL_APPLICATION1) ||
               (Signal == SIGNAL_APPLICATION2) ||
               (Signal == SIGNAL_ASYNCHRONOUS_IO_COMPLETE) ||
               (Signal == SIGNAL_PROFILE_TIMER) ||
               (Signal == SIGNAL_EXECUTION_TIMER_EXPIRED) ||
               (Signal >= STANDARD_SIGNAL_COUNT)) {

        Process->ExitReason = CHILD_SIGNAL_REASON_KILLED;
        Process->ExitStatus = Signal;
        SendSignal = SIGNAL_KILL;
        Result = TRUE;

    //
    // Process the signals whose default action is to stop.
    //

    } else if ((Signal == SIGNAL_REQUEST_STOP) ||
               (Signal == SIGNAL_BACKGROUND_TERMINAL_INPUT) ||
               (Signal == SIGNAL_BACKGROUND_TERMINAL_OUTPUT)) {

        Process->ExitReason = CHILD_SIGNAL_REASON_STOPPED;
        Process->ExitStatus = Signal;
        SendSignal = SIGNAL_STOP;
        Result = TRUE;

    //
    // If the signal would be delivered but there is no handler, abort.
    //

    } else if (Process->SignalHandlerRoutine == NULL) {
        Process->ExitReason = CHILD_SIGNAL_REASON_DUMPED;
        Process->ExitStatus = Signal;
        SendSignal = SIGNAL_KILL;
        Result = TRUE;
    }

    KeReleaseQueuedLock(Process->QueuedLock);

    //
    // If the default action causes the process to do something like die or
    // stop, then queue that signal process-wide.
    //

    if (SendSignal != 0) {
        PsSignalProcess(Process, SendSignal, NULL);
    }

    return Result;
}

VOID
PspCleanupThreadSignals (
    VOID
    )

/*++

Routine Description:

    This routine cleans up the current thread's signal state, potentially
    waking up other threads if it was on the hook for handling a signal. This
    should only be called during thread termination in the context of the thead
    whose signal state needs to be cleaned up.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKTHREAD CurrentThread;
    SIGNAL_SET PendingSignals;
    PKPROCESS Process;

    CurrentThread = KeGetCurrentThread();
    Process = CurrentThread->OwningProcess;

    ASSERT(KeIsQueuedLockHeld(Process->QueuedLock) != FALSE);
    ASSERT((CurrentThread->Flags & THREAD_FLAG_EXITING) != 0);

    //
    // If no signals are pending, then this thread is not responsible for
    // making sure other threads are awake to handle process-wide signals.
    //

    if (CurrentThread->SignalPending != ThreadSignalPending) {
        return;
    }

    //
    // Move the set of process-wide signals that this thread does not block.
    //

    PendingSignals = Process->PendingSignals;
    REMOVE_SIGNALS_FROM_SET(PendingSignals, CurrentThread->BlockedSignals);
    PspMoveSignalSet(PendingSignals);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
PspCheckForNonMaskableSignals (
    PSIGNAL_PARAMETERS SignalParameters,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine checks for and handles kill, stop, and continue signals. It
    also checks for trace break requests.

Arguments:

    SignalParameters - Supplies a pointer where signal parameters may be
        returned. Signal parameters may be returned if the tracer process
        changes the signal.

    TrapFrame - Supplies a pointer to the user mode trap frame.

Return Value:

    Returns the signal number of the first pending signal.

    -1 if no signals are pending or a signal is already in progress.

--*/

{

    ULONG CombinedSignalMask;
    ULONG DequeuedSignal;
    BOOL FirstThread;
    PKPROCESS Process;
    SIGNAL_SET ProcessSignalMask;
    BOOL StopHandled;
    PKTHREAD Thread;
    SIGNAL_SET ThreadSignalMask;

    DequeuedSignal = -1;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;

    //
    // Loop as long as there is an unmaskable signal set. Note that a continue
    // signal is "non-maskable" in that it always signals the stop event to
    // release stopped threads, but it can also be handled. As such, it is not
    // processed here.
    //

    while (TRUE) {
        ThreadSignalMask = Thread->PendingSignals;
        ProcessSignalMask = Process->PendingSignals;
        OR_SIGNAL_SETS(CombinedSignalMask, ThreadSignalMask, ProcessSignalMask);

        //
        // Handle a termination signal.
        //

        if (IS_SIGNAL_SET(CombinedSignalMask, SIGNAL_KILL) != FALSE) {
            PspThreadTermination();
        }

        //
        // Handle a stop signal.
        //

        if (IS_SIGNAL_SET(CombinedSignalMask, SIGNAL_STOP) != FALSE) {
            PspMarkThreadStopped(Process, &FirstThread);

            //
            // The first thread drives the notifications to the parent and
            // tracer process.
            //

            StopHandled = TRUE;
            if (FirstThread != FALSE) {
                RtlZeroMemory(SignalParameters, sizeof(SIGNAL_PARAMETERS));
                SignalParameters->SignalNumber = SIGNAL_STOP;
                PspTracerBreak(SignalParameters, TrapFrame, TRUE, &StopHandled);

                //
                // If it's no longer stop, then the tracer turned this into a
                // real signal, so return it now. If the signal changed, then
                // the stop must have been handled by the tracer break.
                //

                if (SignalParameters->SignalNumber != SIGNAL_STOP) {

                    ASSERT(StopHandled != FALSE);

                    if (SignalParameters->SignalNumber != 0) {
                        DequeuedSignal = SignalParameters->SignalNumber;
                        goto CheckForNonMaskableSignalsEnd;
                    }

                    continue;
                }

                //
                // It's still a stop signal, let the parent know via a child
                // signal. Skip it if the tracing process is also the parent.
                //

                if ((Process->DebugData == NULL) ||
                    (Process->DebugData->TracingProcess != Process->Parent)) {

                    PspQueueChildSignalToParent(Process,
                                                SIGNAL_STOP,
                                                CHILD_SIGNAL_REASON_STOPPED);
                }
            }

            //
            // Actually perform the stop on all threads that aren't the first
            // and on the first thread if the tracer break did not handle the
            // stop.
            //

            if ((FirstThread == FALSE) || (StopHandled == FALSE)) {
                PspWaitOnStopEvent(Process, TrapFrame);
            }

            //
            // Loop around and look for more unmaskable signals.
            //

            continue;
        }

        //
        // No signals anywhere, stop looping.
        //

        break;
    }

CheckForNonMaskableSignalsEnd:
    return DequeuedSignal;
}

VOID
PspQueueChildSignal (
    PKPROCESS Process,
    PKPROCESS Destination,
    UINTN ExitStatus,
    USHORT Reason
    )

/*++

Routine Description:

    This routine queues the child signal to the given process' parent or tracer,
    indicating the process has terminated, stopped, or continued.

Arguments:

    Process - Supplies a pointer to the child process that just exited, stopped,
        or continued.

    Destination - Supplies a pointer to the destination process to send the
        signal to. This is always either the parent or the tracer process.

    ExitStatus - Supplies the exit status on graceful exits or the signal number
        that caused the termination.

    Reason - Supplies the reason code for the child signal.

Return Value:

    None.

--*/

{

    BOOL EntryRemoved;
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Reason != 0);

    if (Destination == NULL) {
        return;
    }

    if (Destination == PsGetKernelProcess()) {

        ASSERT(FALSE);

        return;
    }

    SignalQueueEntry = &(Process->ChildSignal);

    //
    // If the signal is already queued, remove it.
    //

    KeAcquireSpinLock(&(Process->ChildSignalLock));
    if (SignalQueueEntry->ListEntry.Next != NULL) {
        EntryRemoved = FALSE;
        KeAcquireQueuedLock(Process->ChildSignalDestination->QueuedLock);
        if (SignalQueueEntry->ListEntry.Next != NULL) {
            LIST_REMOVE(&(SignalQueueEntry->ListEntry));
            EntryRemoved = TRUE;
        }

        KeReleaseQueuedLock(Process->ChildSignalDestination->QueuedLock);
        if (EntryRemoved != FALSE) {
            ObReleaseReference(Process);
        }
    }

    //
    // Queue the signal.
    //

    SignalQueueEntry->Parameters.SignalNumber = SIGNAL_CHILD_PROCESS_ACTIVITY;

    ASSERT(Reason > SIGNAL_CODE_USER);

    SignalQueueEntry->Parameters.SignalCode = Reason;
    SignalQueueEntry->Parameters.FromU.SendingProcess =
                                                Process->Identifiers.ProcessId;

    SignalQueueEntry->Parameters.SendingUserId = 0;
    SignalQueueEntry->Parameters.Parameter = ExitStatus;
    SignalQueueEntry->CompletionRoutine = PspChildSignalCompletionRoutine;
    Process->ChildSignalDestination = Destination;
    ObAddReference(Process);
    PsSignalProcess(Destination,
                    SIGNAL_CHILD_PROCESS_ACTIVITY,
                    SignalQueueEntry);

    KeReleaseSpinLock(&(Process->ChildSignalLock));
    return;
}

PSIGNAL_QUEUE_ENTRY
PspGetChildSignalEntry (
    PROCESS_ID ProcessId,
    ULONG WaitFlags
    )

/*++

Routine Description:

    This routine combs through the current process' pending signals mask and
    attempts to find a child signal entry that matches the given criteria.

Arguments:

    ProcessId - Supplies a process ID that indicates what children satisfy the
        query:

        If -1 is supplied, any child signal will be pulled off and returned.

        If a number greater than 0 is supplied, only the specific process ID
        will be pulled off and returned.

        If 0 is supplied, any child process whose process group ID is equal to
        that of the calling process will be pulled.

        If a number less than zero (but not -1) is supplied, then any process
        whose process group ID is equal to the absolute value of this parameter
        will be dequeued and returned.

    WaitFlags - Supplies the bitfield of child actions to accept. See
        SYSTEM_CALL_WAIT_FLAG_* definitions.

Return Value:

    Returns the signal number of the first pending signal that matches the
    criteria. Unless the flags specify to leave it on the queue, the queue
    entry will be removed from the pending signal queue.

    -1 if no signals are pending or a signal is already in progress.

--*/

{

    PKPROCESS ChildProcess;
    USHORT ChildSignal;
    BOOL ClearPending;
    PLIST_ENTRY CurrentEntry;
    BOOL EntryFound;
    PKPROCESS Process;
    PSIGNAL_QUEUE_ENTRY SignalEntry;
    PKTHREAD Thread;

    EntryFound = FALSE;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    SignalEntry = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(Process->QueuedLock);

    //
    // If the signal state is "child signal pending", reset it so suspend
    // doesn't break out immediately.
    //

    if (Thread->SignalPending == ThreadChildSignalPending) {
        Thread->SignalPending = ThreadNoSignalPending;
    }

    //
    // Check the unreaped child list first.
    //

    CurrentEntry = Process->UnreapedChildList.Next;
    while (CurrentEntry != &(Process->UnreapedChildList)) {
        SignalEntry = LIST_VALUE(CurrentEntry, SIGNAL_QUEUE_ENTRY, ListEntry);

        ASSERT(IS_CHILD_SIGNAL(SignalEntry));

        EntryFound = PspMatchChildWaitRequestWithProcessId(ProcessId,
                                                           WaitFlags,
                                                           SignalEntry);

        if (EntryFound != FALSE) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Child signals always get queued to a process, not a thread, so only look
    // through the process list if nothing was found above.
    //

    ClearPending = FALSE;
    ChildSignal = SIGNAL_CHILD_PROCESS_ACTIVITY;
    if ((EntryFound == FALSE) &&
        (IS_SIGNAL_SET(Process->PendingSignals, ChildSignal) != FALSE)) {

        //
        // If one is found, then keep looking for a second child signal. It
        // doesn't need to match the request.
        //

        ClearPending = TRUE;
        CurrentEntry = Process->SignalListHead.Next;
        while (CurrentEntry != &(Process->SignalListHead)) {
            SignalEntry = LIST_VALUE(CurrentEntry,
                                     SIGNAL_QUEUE_ENTRY,
                                     ListEntry);

            ASSERT((SignalEntry->Parameters.SignalNumber != 0) &&
                   (SignalEntry->Parameters.SignalNumber < SIGNAL_COUNT));

            //
            // If a match has already been found, keep looking until another
            // child signal is found.
            //

            if (EntryFound != FALSE) {
                if (SignalEntry->Parameters.SignalNumber == ChildSignal) {
                    ClearPending = FALSE;
                    break;
                }

                continue;
            }

            EntryFound = PspMatchChildWaitRequestWithProcessId(ProcessId,
                                                               WaitFlags,
                                                               SignalEntry);

            CurrentEntry = CurrentEntry->Next;
        }
    }

    //
    // If an entry was found, prepare to return it.
    //

    if (EntryFound != FALSE) {

        //
        // If the entry is not to be discarded, then the job is done.
        //

        if ((WaitFlags & SYSTEM_CALL_WAIT_FLAG_DONT_DISCARD_CHILD) != 0) {
            goto GetChildSignalEntryEnd;
        }

        //
        // Otherwise remove it from its signal list, never to be waited on
        // again. Clear it from the pending signal set if it was found on the
        // signal queue and there are no more child signals.
        //

        LIST_REMOVE(&(SignalEntry->ListEntry));
        SignalEntry->ListEntry.Next = NULL;
        if (ClearPending != FALSE) {
            REMOVE_SIGNAL(Process->PendingSignals, ChildSignal);
        }

        //
        // If the child exited, then accumulate the child's resource usage
        // data. Only the parent's process lock needs to be held. The child has
        // terminated so it's cycle values are not changing any time soon.
        //

        if ((WaitFlags & SYSTEM_CALL_WAIT_FLAG_EXITED_CHILDREN) != 0) {
            ChildProcess = PARENT_STRUCTURE(SignalEntry, KPROCESS, ChildSignal);
            PspAddResourceUsages(&(Process->ChildResourceUsage),
                                 &(ChildProcess->ResourceUsage));

            PspAddResourceUsages(&(Process->ChildResourceUsage),
                                 &(ChildProcess->ChildResourceUsage));
        }

    } else {
        SignalEntry = NULL;
    }

GetChildSignalEntryEnd:
    KeReleaseQueuedLock(Process->QueuedLock);
    return SignalEntry;
}

KSTATUS
PspValidateWaitParameters (
    PKPROCESS Process,
    LONG ProcessId
    )

/*++

Routine Description:

    This routine validates that the given parameter to a wait system call is
    valid.

Arguments:

    Process - Supplies a pointer to the (current) process.

    ProcessId - Supplies the wait parameter.
        -1 waits for any process, so the request is valid if this process has
        any children.

        0 waits for any process in the current process group, so the request
        is valid if there are any children in the child process group.

        >0 waits for a specific process, so the request is valid if the process
        exists and is a child.

        <-1 waits for any child in the given process group (negated), so the
        request is valid if there are any children in the given process group.

Return Value:

    STATUS_SUCCESS if the request is valid.

    STATUS_NO_ELIGIBLE_CHILDREN if the request is invalid.

--*/

{

    PKPROCESS Child;
    PLIST_ENTRY CurrentEntry;
    PROCESS_ID MatchingProcess;
    PROCESS_GROUP_ID MatchingProcessGroup;
    KSTATUS Status;

    ASSERT(Process == PsGetCurrentProcess());
    ASSERT(KeGetRunLevel() == RunLevelLow);

    MatchingProcess = 0;
    MatchingProcessGroup = 0;
    Status = STATUS_NO_ELIGIBLE_CHILDREN;
    KeAcquireQueuedLock(Process->QueuedLock);

    //
    // A value of -1 matches any child.
    //

    if (ProcessId == -1) {
        if (LIST_EMPTY(&(Process->ChildListHead)) == FALSE) {
            Status = STATUS_SUCCESS;
        }

        goto ValidateWaitParametersEnd;

    //
    // A value of 0 matches the current process group.
    //

    } else if (ProcessId == 0) {
        MatchingProcessGroup = Process->Identifiers.ProcessGroupId;

    //
    // A positive value matches a specific process ID.
    //

    } else if (ProcessId > 0) {
        MatchingProcess = ProcessId;

    //
    // A negative value matches a specific process group ID (negated of course).
    //

    } else {
        MatchingProcessGroup = -ProcessId;
    }

    //
    // Loop looking for a child that matches.
    //

    CurrentEntry = Process->ChildListHead.Next;
    while (CurrentEntry != &(Process->ChildListHead)) {
        Child = LIST_VALUE(CurrentEntry, KPROCESS, SiblingListEntry);
        if ((Child->Identifiers.ProcessId == MatchingProcess) ||
            (Child->Identifiers.ProcessGroupId == MatchingProcessGroup)) {

            Status = STATUS_SUCCESS;
            goto ValidateWaitParametersEnd;
        }

        CurrentEntry = CurrentEntry->Next;
    }

ValidateWaitParametersEnd:
    KeReleaseQueuedLock(Process->QueuedLock);
    return Status;
}

BOOL
PspMatchChildWaitRequestWithProcessId (
    LONG WaitPidRequest,
    ULONG WaitFlags,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    )

/*++

Routine Description:

    This routine matches a child PID request ID with an actual PID that had a
    child signal sent.

Arguments:

    WaitPidRequest - Supplies the child process ID request, which can be one of
        the following:

        If -1 is supplied, any child signal will be pulled off and returned.

        If a number greater than 0 is supplied, only the specific process ID
        will be pulled off and returned.

        If 0 is supplied, any child process whose process group ID is equal to
        that of the calling process will be pulled.

        If a number less than zero (but not -1) is supplied, then any process
        whose process group ID is equal to the absolute value of this parameter
        will be dequeued and returned.

    WaitFlags - Supplies the wait flags that govern which child signals can
        satisfy the wait. See SYSTEM_CALL_WAIT_FLAG_* definitions.

    SignalQueueEntry - Supplies a pointer to the signal queue entry in question.

Return Value:

    TRUE if the process ID matches the request.

    FALSE if the process ID does not match the request.

--*/

{

    PKPROCESS CurrentProcess;
    BOOL Match;
    PKPROCESS Process;
    USHORT Reason;
    PSIGNAL_PARAMETERS SignalParameters;

    SignalParameters = &(SignalQueueEntry->Parameters);
    if (SignalParameters->SignalNumber != SIGNAL_CHILD_PROCESS_ACTIVITY) {
        return FALSE;
    }

    Match = FALSE;

    //
    // A positive value matches against a specific process ID.
    //

    if (WaitPidRequest > 0) {
        if (SignalParameters->FromU.SendingProcess == WaitPidRequest) {
            Match = TRUE;
        }

    //
    // A value of zero matches against any process in the current process
    // group.
    //

    } else if (WaitPidRequest == 0) {
        CurrentProcess = PsGetCurrentProcess();
        Process = PspGetChildProcessById(
                                       CurrentProcess,
                                       SignalParameters->FromU.SendingProcess);

        ASSERT(Process != NULL);

        if (CurrentProcess->Identifiers.ProcessGroupId ==
            Process->Identifiers.ProcessGroupId) {

            Match = TRUE;
        }

        ObReleaseReference(Process);

    //
    // A value of -1 matches against any process.
    //

    } else if (WaitPidRequest == -1) {
        Match = TRUE;

    //
    // Any other negative value matches against any process of a specific
    // process group (negated).
    //

    } else {
        CurrentProcess = PsGetCurrentProcess();
        Process = PspGetChildProcessById(
                                       CurrentProcess,
                                       SignalParameters->FromU.SendingProcess);

        ASSERT(Process != NULL);

        if (Process->Identifiers.ProcessGroupId == -WaitPidRequest) {
            Match = TRUE;
        }

        ObReleaseReference(Process);
    }

    //
    // Now, if there's a match, filter the status against the desired wait
    // flags.
    //

    if (Match != FALSE) {
        Match = FALSE;
        Reason = SignalParameters->SignalCode;
        switch (Reason) {
        case CHILD_SIGNAL_REASON_EXITED:
        case CHILD_SIGNAL_REASON_KILLED:
        case CHILD_SIGNAL_REASON_DUMPED:
            if ((WaitFlags & SYSTEM_CALL_WAIT_FLAG_EXITED_CHILDREN) != 0) {
                Match = TRUE;
            }

            break;

        case CHILD_SIGNAL_REASON_STOPPED:
        case CHILD_SIGNAL_REASON_TRAPPED:
            if ((WaitFlags & SYSTEM_CALL_WAIT_FLAG_STOPPED_CHILDREN) != 0) {
                Match = TRUE;
            }

            break;

        case CHILD_SIGNAL_REASON_CONTINUED:
            if ((WaitFlags & SYSTEM_CALL_WAIT_FLAG_CONTINUED_CHILDREN) != 0) {
                Match = TRUE;
            }

            break;

        //
        // Empty or unhandled reason. This is unexpected.
        //

        default:

            ASSERT(FALSE);

            break;
        }
    }

    return Match;
}

VOID
PspChildSignalCompletionRoutine (
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    )

/*++

Routine Description:

    This routine is called when a child signal completes. It simply decrements
    the reference count on the owning (child) process, allowing it to
    deallocate that memory if that's all that was being waited for.

Arguments:

    SignalQueueEntry - Supplies a pointer to the signal queue entry that was
        successfully completed.

Return Value:

    None.

--*/

{

    PKPROCESS ChildProcess;

    ChildProcess = PARENT_STRUCTURE(SignalQueueEntry, KPROCESS, ChildSignal);
    ChildProcess->ChildSignalDestination = NULL;

    //
    // If the signal queue entry's exit status matches that of the child
    // process, then this was the exit signal. Let the child process know that
    // it is now time to drift away.
    //

    if ((ChildProcess->ExitReason != 0) &&
        (SignalQueueEntry->Parameters.SignalCode == ChildProcess->ExitReason) &&
        (SignalQueueEntry->Parameters.Parameter == ChildProcess->ExitStatus)) {

        PspRemoveProcessFromLists(ChildProcess);
    }

    ObReleaseReference(ChildProcess);
    return;
}

VOID
PspMarkThreadStopped (
    PKPROCESS Process,
    PBOOL FirstThread
    )

/*++

Routine Description:

    This routine marks a thread as stopped.

Arguments:

    Process - Supplies a pointer to the process this thread belongs to.

    FirstThread - Supplies a pointer to a boolean indicating whether this is
        the first thread to be stopped.

Return Value:

    None.

--*/

{

    ULONG StoppedThreadCount;

    if (Process->DebugData != NULL) {
        KeAcquireQueuedLock(Process->QueuedLock);
    }

    *FirstThread = FALSE;
    StoppedThreadCount = RtlAtomicAdd32(&(Process->StoppedThreadCount),
                                       (ULONG)1);

    StoppedThreadCount += 1;
    if (StoppedThreadCount == 1) {
        *FirstThread = TRUE;
    }

    //
    // When being traced, the last thread to be stopped must signal so that the
    // first thread knows it can alert the tracer. This is synchronized under
    // the process' queued lock as a terminating thread may also notice that it
    // would have been the last thread to stop and then signal the event.
    //

    if (Process->DebugData != NULL) {
        if (StoppedThreadCount == Process->ThreadCount) {
            KeSignalEvent(Process->DebugData->AllStoppedEvent,
                          SignalOptionSignalAll);
        }

        KeReleaseQueuedLock(Process->QueuedLock);
    }

    return;
}

VOID
PspTracerBreak (
    PSIGNAL_PARAMETERS Signal,
    PTRAP_FRAME TrapFrame,
    BOOL ThreadAlreadyStopped,
    PBOOL ThreadStopHandled
    )

/*++

Routine Description:

    This routine forwards a signal onto the tracing process. This routine
    assumes the process lock is already held.

Arguments:

    Signal - Supplies a pointer to the signal that this process would be
        getting (or ignoring).

    TrapFrame - Supplies a pointer to the user mode trap frame.

    ThreadAlreadyStopped - Supplies a boolean indicating if the thread has
        already been marked as stopped. This occurs for stop signals.

    ThreadStopHandled - Supplies an optional pointer to a boolean that returns
        whether or not this routine waited on the stop event. This must be
        supplied if the thread was already stopped by the caller.

Return Value:

    None.

--*/

{

    PPROCESS_DEBUG_BREAK_RANGE BreakRange;
    PPROCESS_DEBUG_DATA DebugData;
    BOOL FirstThread;
    BOOL InRange;
    PVOID InstructionPointer;
    ULONG NewSignal;
    ULONG OriginalSignal;
    PKPROCESS Process;
    BOOL ProcessLockHeld;
    USHORT Reason;
    BOOL StopHandled;
    BOOL StopSent;
    PKTHREAD Thread;
    BOOL TracerLockHeld;
    PKPROCESS TracingProcess;

    ASSERT((ThreadAlreadyStopped == FALSE) || (ThreadStopHandled != NULL));

    ProcessLockHeld = FALSE;
    TracerLockHeld = FALSE;
    StopHandled = FALSE;
    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    DebugData = Process->DebugData;
    TracingProcess = NULL;

    //
    // If debugging is not enabled or there's no other process debugging this
    // one, forward the issue onto the kernel debugger first if it is not
    // handled and looks unexpected. This used to forward all unhandled signals,
    // but that caused the kernel to break in even when a user mode process
    // was expecting the signal during suspended execution.
    //

    if ((DebugData == NULL) || (DebugData->TracingProcess == NULL)) {
        if ((Signal->SignalNumber == SIGNAL_ABORT) ||
            (!IS_SIGNAL_SET(Process->HandledSignals, Signal->SignalNumber) &&
             ((Signal->SignalNumber == SIGNAL_ILLEGAL_INSTRUCTION) ||
              (Signal->SignalNumber == SIGNAL_BUS_ERROR) ||
              (Signal->SignalNumber == SIGNAL_MATH_ERROR) ||
              (Signal->SignalNumber == SIGNAL_ACCESS_VIOLATION) ||
              (Signal->SignalNumber == SIGNAL_TRAP)))) {

            PspForwardUserModeExceptionToKernel(Signal, TrapFrame);
        }

        goto TracerBreakEnd;
    }

    Reason = 0;
    if (Signal->SignalNumber == SIGNAL_CONTINUE) {
        Reason = CHILD_SIGNAL_REASON_CONTINUED;

    } else {
        Reason = CHILD_SIGNAL_REASON_TRAPPED;
    }

    //
    // Loop trying to acquire the lock and servicing others who were fortunate
    // enough to get the lock.
    //

    while (TRUE) {
        TracerLockHeld = KeTryToAcquireSpinLock(&(DebugData->TracerLock));
        if (TracerLockHeld != FALSE) {
            break;
        }

        if (DebugData->TracerStopRequested != FALSE) {
            if (ThreadAlreadyStopped == FALSE) {
                PspMarkThreadStopped(Process, &FirstThread);
            }

            PspWaitOnStopEvent(Process, TrapFrame);
            ThreadAlreadyStopped = FALSE;
            StopHandled = TRUE;
        }
    }

    ASSERT(DebugData->TracerStopRequested == FALSE);
    ASSERT(DebugData->DebugLeaderThread == NULL);

    //
    // If it's a trap signal coming in and the previous command was a single
    // step or range step, clear single step mode now.
    //

    if ((Signal->SignalNumber == SIGNAL_TRAP) &&
        ((DebugData->DebugCommand.PreviousCommand == DebugCommandSingleStep) ||
         (DebugData->DebugCommand.PreviousCommand == DebugCommandRangeStep))) {

        PspArchSetOrClearSingleStep(TrapFrame, FALSE);

        //
        // If it was a range step command, evaluate whether this trap fits the
        // range.
        //

        if (DebugData->DebugCommand.PreviousCommand == DebugCommandRangeStep) {
            BreakRange = &(DebugData->BreakRange);
            InstructionPointer = ArGetInstructionPointer(TrapFrame);

            //
            // This should get turned into a break if it's inside the break
            // range but not inside the hole.
            //

            InRange = FALSE;
            if ((InstructionPointer >= BreakRange->BreakRangeStart) &&
                (InstructionPointer < BreakRange->BreakRangeEnd)) {

                InRange = TRUE;
                if ((InstructionPointer >= BreakRange->RangeHoleStart) &&
                    (InstructionPointer < BreakRange->RangeHoleEnd)) {

                    InRange = FALSE;
                }
            }

            if (InRange == FALSE) {
                Signal->SignalNumber = 0;
                PspArchSetOrClearSingleStep(TrapFrame, TRUE);
                goto TracerBreakEnd;
            }
        }
    }

    //
    // If the tracer pulled out while the lock was being acquired, just end it
    // now. The tracer stop requested variable was never set, so there should
    // be no stopped threads or anything to wake up.
    //

    if (DebugData->TracingProcess == NULL) {
        goto TracerBreakEnd;
    }

    //
    // Copy the signal information over.
    //

    RtlCopyMemory(&(DebugData->TracerSignalInformation),
                  Signal,
                  sizeof(SIGNAL_PARAMETERS));

    //
    // This routine needs to initialize the debug command and unsignal the stop
    // event, but it needs to synchronize with the tracer killing all its
    // tracee threads. If the kill signal has not been sent by the tracer by
    // the time the lock is held, then it is safe to invalidate the debug
    // command and unsignal the stop event. A kill signal cannot come in and
    // set the debug command to continue until after the lock is released.
    //

    KeAcquireQueuedLock(Process->QueuedLock);
    ProcessLockHeld = TRUE;
    if (IS_SIGNAL_SET(Process->PendingSignals, SIGNAL_KILL) != FALSE) {
        goto TracerBreakEnd;
    }

    //
    // A new continue or kill signal will signal the stop event, so set the
    // command to invalid to keep the tracing alive until the tracer continues.
    //

    DebugData->DebugCommand.Command = DebugCommandInvalid;

    //
    // If this is not a stop signal, then none of the other threads should be
    // trying to stop yet. Make sure they wait on the stop event. For the stop
    // signal, the event should have already been unsignaled. No harm in doing
    // it again.
    //

    KeSignalEvent(Process->StopEvent, SignalOptionUnsignal);

    //
    // The tracing process may disappear at any moment if it terminates. To
    // communicate its termination to the tracee, it NULLs its pointer while
    // holding the tracee's lock. Attempt to grab it and take a reference if it
    // is found.
    //

    TracingProcess = DebugData->TracingProcess;
    if (TracingProcess == NULL) {
        goto TracerBreakEnd;
    }

    ObAddReference(TracingProcess);
    KeReleaseQueuedLock(Process->QueuedLock);
    ProcessLockHeld = FALSE;

    //
    // If the thread is not already stopped, then mark it stopped. Request a
    // tracer stop to halt the other threads. This is necessary so that this
    // thread will wait for all other threads to stop.
    //

    if (ThreadAlreadyStopped == FALSE) {
        PspMarkThreadStopped(Process, &FirstThread);
    }

    //
    // The tracer stop request is necessary to halt other threads that are
    // looping in attempt to acquire the tracer lock. Without it, those other
    // threads may incorrectly wait on the stop event even if this thread
    // exited this routine somewhere above.
    //

    DebugData->TracerStopRequested = TRUE;

    //
    // The other threads might be running around thinking everything is just
    // fine. Send a STOP signal to the process to halt them. Only do this if
    // there is more than 1 thread. The count will not go from 1 to 2, as this
    // thread is a bit busy. It may go from 2 to 1 after the check, but that's
    // life.
    //
    // This needs to be done even if the original signal was a STOP. It may be
    // that the STOP came in after another signal had acquired the tracer lock.
    // In that case, the first signal (on a different thread) sent and cleared
    // a STOP, but all other threads need to be stopped again.
    //

    StopSent = FALSE;
    if (Process->ThreadCount > 1) {
        PsSignalProcess(Process, SIGNAL_STOP, NULL);
        StopSent = TRUE;
    }

    KeWaitForEvent(DebugData->AllStoppedEvent, FALSE, WAIT_TIME_INDEFINITE);

    ASSERT(DebugData->TracerStopRequested != FALSE);

    //
    // This thread can only reach this point after the last thread has signaled
    // the all stopped event. Unsignal it now. All the other threads should be
    // waiting on the stop event. They can only continue from there if a KILL
    // or CONTINUE comes in. At which point they will loop trying to process
    // the invalid debug command set above.
    //

    KeSignalEvent(DebugData->AllStoppedEvent, SignalOptionUnsignal);
    DebugData->TracerStopRequested = FALSE;
    DebugData->DebugLeaderThread = Thread;

    //
    // As soon as the tracer is signaled, a command could come in to continue
    // the process. If the STOP signal were still set, then the first thread
    // to race out of the all-stopped event would hit the STOP again. Remove it
    // from the signals now.
    //

    if ((Signal->SignalNumber == SIGNAL_STOP) || (StopSent != FALSE)) {
        KeAcquireQueuedLock(Process->QueuedLock);
        ProcessLockHeld = TRUE;
        if (Signal->SignalNumber == SIGNAL_STOP) {
            if (IS_SIGNAL_SET(Thread->PendingSignals, SIGNAL_STOP) != FALSE) {
                REMOVE_SIGNAL(Thread->PendingSignals, SIGNAL_STOP);
                if (StopSent != FALSE) {
                    REMOVE_SIGNAL(Process->PendingSignals, SIGNAL_STOP);
                }

            } else {
                REMOVE_SIGNAL(Process->PendingSignals, SIGNAL_STOP);
            }

        } else {

            ASSERT(StopSent != FALSE);

            REMOVE_SIGNAL(Process->PendingSignals, SIGNAL_STOP);
        }

        KeReleaseQueuedLock(Process->QueuedLock);
        ProcessLockHeld = FALSE;
    }

    //
    // Send the child signal over to the tracer. The tracer lock is held, so
    // the tracing process cannot be released during this period.
    //

    PspQueueChildSignal(Process, TracingProcess, Signal->SignalNumber, Reason);

    //
    // Wait for the tracer to continue this process.
    //

    PspWaitOnStopEvent(Process, TrapFrame);
    StopHandled = TRUE;

    //
    // Wait for all threads to get all the way out. The last thread will signal
    // the all-stopped event and only this thread will wait on it. This allows
    // the thread to safely unsignal the event after waiting. If all threads
    // waited on the event, one may not begin the wait until after it has been
    // signaled and then unsignaled.
    //

    KeWaitForEvent(DebugData->AllStoppedEvent, FALSE, WAIT_TIME_INDEFINITE);
    KeSignalEvent(DebugData->AllStoppedEvent, SignalOptionUnsignal);
    DebugData->DebugLeaderThread = NULL;

    //
    // Copy the possibly modified information back.
    //

    OriginalSignal = Signal->SignalNumber;
    RtlCopyMemory(Signal,
                  &(DebugData->TracerSignalInformation),
                  sizeof(SIGNAL_PARAMETERS));

    NewSignal = Signal->SignalNumber;

    //
    // Check for a kill signal. If the tracing process just died, it will have
    // sent a kill signal, which signals the stop event so all threads can
    // continue. And for them to continue, they all need to decrement the stop
    // thread count so that the all stopped event is signaled.
    //
    // The tracing process issues a continue command with SIG_KILL so the
    // tracee threads will not be stuck on an invalid command.
    //

    if (IS_SIGNAL_SET(Process->PendingSignals, SIGNAL_KILL) != FALSE) {
        goto TracerBreakEnd;
    }

    //
    // If the signal coming out is different and non-maskable, set it process
    // wide. If the signal did not change, it should not be replayed. This
    // would likely create a loop.
    //

    if ((NewSignal != OriginalSignal) &&
        ((NewSignal == SIGNAL_KILL) ||
         (NewSignal == SIGNAL_STOP) ||
         (NewSignal == SIGNAL_CONTINUE))) {

        PsSignalProcess(Process, NewSignal, NULL);
    }

TracerBreakEnd:
    if (ProcessLockHeld != FALSE) {
        KeReleaseQueuedLock(Process->QueuedLock);
    }

    if (TracerLockHeld != FALSE) {
        KeReleaseSpinLock(&(DebugData->TracerLock));
    }

    if (ThreadStopHandled != NULL) {
        *ThreadStopHandled = StopHandled;
    }

    if (TracingProcess != NULL) {
        ObReleaseReference(TracingProcess);
    }

    return;
}

VOID
PspForwardUserModeExceptionToKernel (
    PSIGNAL_PARAMETERS Signal,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine forwards a tracer break on to the kernel mode debugger.

Arguments:

    Signal - Supplies a pointer to the signal that this process would be
        getting (or ignoring).

    TrapFrame - Supplies a pointer to the user mode trap frame.

Return Value:

    None.

--*/

{

    PKPROCESS Process;

    //
    // Do nothing if the debugger is not connected or user mode exceptions are
    // not allowed.
    //

    if ((KdIsDebuggerConnected() == FALSE) ||
        (KdAreUserModeExceptionsEnabled() == FALSE)) {

        return;
    }

    Process = PsGetCurrentProcess();

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Process != PsGetKernelProcess());

    //
    // If the signal is not in the mask of signals sent up to kernel mode, then
    // ignore it.
    //

    if ((Signal->SignalNumber < STANDARD_SIGNAL_COUNT) &&
        (!IS_SIGNAL_SET(KERNEL_REPORTED_USER_SIGNALS, Signal->SignalNumber))) {

        return;
    }

    //
    // The queued lock must be held to avoid racing with an execute image call
    // that changes the process name.
    //

    KeAcquireQueuedLock(Process->QueuedLock);
    if (Signal->SignalNumber < STANDARD_SIGNAL_COUNT) {
        if (Signal->SignalNumber != SIGNAL_TRAP) {
            RtlDebugPrint(" *** User mode process %d (%s) caught signal %s "
                          "(SIGNAL_PARAMETERS %x) ***\n",
                          Process->Identifiers.ProcessId,
                          Process->Header.Name,
                          PsSignalNames[Signal->SignalNumber],
                          Signal);
        }

    } else {
        RtlDebugPrint(" *** User mode process %d (%s) caught signal %d "
                      "(SIGNAL_PARAMETERS at %x) ***\n",
                      Process->Identifiers.ProcessId,
                      Process->Header.Name,
                      Signal->SignalNumber,
                      Signal);
    }

    KeReleaseQueuedLock(Process->QueuedLock);
    PspLoadProcessImagesIntoKernelDebugger(Process);
    RtlDebugService(EXCEPTION_USER_MODE, TrapFrame);

    //
    // If this was a trap signal, clear it to allow the process to continue
    // rather than dying.
    //

    if (Signal->SignalNumber == SIGNAL_TRAP) {
        Signal->SignalNumber = 0;
    }

    return;
}

VOID
PspQueueSignal (
    PKPROCESS Process,
    PKTHREAD Thread,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry,
    BOOL Force
    )

/*++

Routine Description:

    This routine sends a signal to a process or thread. This routine assumes
    the process lock is already held.

Arguments:

    Process - Supplies a pointer to the process to send the signal to.

    Thread - Supplies an optional pointer to the specific thread to send the
        signal to.

    SignalNumber - Supplies the signal number to send.

    SignalQueueEntry - Supplies an optional pointer to a queue entry to place
        on the thread's queue.

    Force - Supplies a boolean that if set indicates the thread cannot block
        or ignore this signal.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    BOOL OnlyWakeSuspendedThreads;
    BOOL SignalBlocked;
    BOOL SignalHandled;
    BOOL SignalIgnored;
    THREAD_SIGNAL_PENDING_TYPE SignalPendingType;
    THREAD_SIGNAL_PENDING_TYPE ThreadSignalPendingType;
    BOOL WokeThread;

    ASSERT(KeIsQueuedLockHeld(Process->QueuedLock) != FALSE);

    SignalPendingType = ThreadNoSignalPending;
    if (Force != FALSE) {

        ASSERT(Thread != NULL);

        //
        // If the signal is blocked, that's the indication that it's already
        // running. Set it back to its default disposition, which will kill
        // the process when delivered.
        //

        if (IS_SIGNAL_SET(Thread->BlockedSignals, SignalNumber)) {
            REMOVE_SIGNAL(Thread->BlockedSignals, SignalNumber);
            REMOVE_SIGNAL(Process->HandledSignals, SignalNumber);
        }

        SignalBlocked = FALSE;
        SignalIgnored = FALSE;

    } else {

        //
        // If the process is being debugged, then no signals are ignored.
        //

        SignalIgnored = IS_SIGNAL_SET(Process->IgnoredSignals, SignalNumber);
        if ((Process->DebugData != NULL) &&
            (Process->DebugData->TracingProcess != NULL)) {

            SignalIgnored = FALSE;

        //
        // A signal can also be ignored if it is not handled and is set to be
        // ignored by default.
        //

        } else if (SignalIgnored == FALSE) {
            SignalHandled = IS_SIGNAL_SET(Process->HandledSignals,
                                          SignalNumber);

            if ((SignalHandled == FALSE) &&
                (IS_SIGNAL_DEFAULT_IGNORE(SignalNumber))) {

                SignalIgnored = TRUE;
            }
        }

        SignalBlocked = FALSE;
        if (Thread != NULL) {
            SignalBlocked = IS_SIGNAL_BLOCKED(Thread, SignalNumber);
        }
    }

    if (SignalQueueEntry != NULL) {

        ASSERT(SignalNumber == SignalQueueEntry->Parameters.SignalNumber);

        //
        // If this is a child signal, then suspended threads should be woken
        // by this signal regardless of whether or not the signal is blocked,
        // so that wait calls can succeed and return.
        //

        if (IS_CHILD_SIGNAL(SignalQueueEntry)) {
            SignalPendingType = ThreadChildSignalPending;
        }

        //
        // If the signal is ignored, then discard it now (except for child
        // signals, so they can get picked up by wait).
        //

        if (SignalIgnored != FALSE) {
            if (IS_CHILD_SIGNAL(SignalQueueEntry)) {
                INSERT_BEFORE(&(SignalQueueEntry->ListEntry),
                              &(Process->UnreapedChildList));

            } else if (SignalQueueEntry->CompletionRoutine != NULL) {
                SignalQueueEntry->ListEntry.Next = NULL;
                SignalQueueEntry->CompletionRoutine(SignalQueueEntry);
            }

        //
        // The signal is not ignored or discarded, so actually queue it. It may
        // be blocked on other threads, but that is handled appropriately below.
        //

        } else {
            if (Thread != NULL) {
                INSERT_BEFORE(&(SignalQueueEntry->ListEntry),
                              &(Thread->SignalListHead));

            } else {
                INSERT_BEFORE(&(SignalQueueEntry->ListEntry),
                              &(Process->SignalListHead));
            }
        }
    }

    //
    // If the signal is not ignored, set the appropriate pending signal mask.
    //

    if (SignalIgnored == FALSE) {
        if (Thread != NULL) {
            ADD_SIGNAL(Thread->PendingSignals, SignalNumber);

        } else {
            ADD_SIGNAL(Process->PendingSignals, SignalNumber);
        }

        //
        // If the signal is not blocked, then prepare to wake a thread.
        //

        if (SignalBlocked == FALSE) {
            if (SignalPendingType == ThreadNoSignalPending) {
                SignalPendingType = ThreadSignalPending;
            }
        }
    }

    if (SignalPendingType != ThreadNoSignalPending) {
        if (Thread != NULL) {

            ASSERT(SignalPendingType == ThreadSignalPending);

            if (Thread->SignalPending < ThreadSignalPending) {
                Thread->SignalPending = ThreadSignalPending;

                //
                // Ensure that this added signal and new signal pending state
                // is visible to the new process before trying to wake it up.
                //

                RtlMemoryBarrier();
                ObWakeBlockedThread(Thread, FALSE);
            }

        } else {

            //
            // Wake up the first thread that doesn't block this signal. Child
            // signals and kill signals are an exception. Kill signals wake up
            // everyone as the process is going down. Child signals wake up
            // everyone, including suspended threads that block the child
            // signal.
            //

            WokeThread = FALSE;
            CurrentEntry = Process->ThreadListHead.Next;
            while (CurrentEntry != &(Process->ThreadListHead)) {
                Thread = LIST_VALUE(CurrentEntry, KTHREAD, ProcessEntry);
                CurrentEntry = CurrentEntry->Next;

                //
                // Do not wake an exiting thread. It will never dispatch the
                // signal.
                //

                if ((Thread->Flags & THREAD_FLAG_EXITING) != 0) {
                    continue;
                }

                //
                // Handle the case where a non-child signal was queued. To
                // reach this point it must not have been ignored. The goal
                // here is to wake the first thread that does not have the
                // signal blocked. The exception is the kill signal, which
                // wakes all threads (and should never be blocked).
                //

                if (SignalPendingType != ThreadChildSignalPending) {

                    ASSERT(SignalIgnored == FALSE);

                    if (!IS_SIGNAL_BLOCKED(Thread, SignalNumber)) {
                        if (Thread->SignalPending < ThreadSignalPending) {
                            Thread->SignalPending = ThreadSignalPending;

                            //
                            // Ensure that this added signal and new signal
                            // pending state are visible to the new process
                            // before trying to wake it up.
                            //

                            RtlMemoryBarrier();
                            ObWakeBlockedThread(Thread, FALSE);
                        }

                        //
                        // Kill, stop, and continue are the only signals that
                        // wake all threads.
                        //

                        if ((SignalNumber != SIGNAL_KILL) &&
                            (SignalNumber != SIGNAL_STOP) &&
                            (SignalNumber != SIGNAL_CONTINUE)) {

                            break;
                        }
                    }

                //
                // Child signals are a bit different. All suspended threads
                // will be woken up, even if they block or ignore (the default)
                // the child signal. The exception here is if the child signal
                // is not ignored - one thread that does not block the signal
                // will be woken up, regardless of the thread state.
                //

                } else {
                    ThreadSignalPendingType = ThreadChildSignalPending;
                    OnlyWakeSuspendedThreads = TRUE;
                    if ((SignalIgnored == FALSE) &&
                        (WokeThread == FALSE) &&
                        (!IS_SIGNAL_BLOCKED(Thread, SignalNumber))) {

                        ThreadSignalPendingType = ThreadSignalPending;
                        OnlyWakeSuspendedThreads = FALSE;
                        WokeThread = TRUE;
                    }

                    if (Thread->SignalPending < ThreadSignalPendingType) {
                        Thread->SignalPending = ThreadSignalPendingType;

                        //
                        // Ensure that this added signal and new signal pending
                        // state are visible to the new process before trying
                        // to wake it up.
                        //

                        RtlMemoryBarrier();
                        ObWakeBlockedThread(Thread, OnlyWakeSuspendedThreads);
                    }
                }
            }
        }
    }

    return;
}

KSTATUS
PspSignalProcess (
    PKPROCESS Process,
    ULONG SignalNumber,
    USHORT SignalCode,
    UINTN SignalParameter
    )

/*++

Routine Description:

    This routine sends a signal to the given process, creating the appropriate
    signal queue structure if necessary.

Arguments:

    Process - Supplies a pointer to the process to send the signal to.

    SignalNumber - Supplies the signal number to send.

    SignalCode - Supplies the signal code to send.

    SignalParameter - Supplies the parameter to send with the signal for real
        time signals.

Return Value:

    Status code.

--*/

{

    PKPROCESS CurrentProcess;
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry;
    KSTATUS Status;

    ASSERT(Process != PsGetKernelProcess());

    Status = STATUS_SUCCESS;
    if (SignalNumber < STANDARD_SIGNAL_COUNT) {
        if (SignalNumber != 0) {
            PsSignalProcess(Process, SignalNumber, NULL);
        }

    } else {

        ASSERT(KeGetRunLevel() == RunLevelLow);

        SignalQueueEntry = MmAllocatePagedPool(sizeof(SIGNAL_QUEUE_ENTRY),
                                               PS_ALLOCATION_TAG);

        if (SignalQueueEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SignalProcessEnd;
        }

        RtlZeroMemory(SignalQueueEntry, sizeof(SIGNAL_QUEUE_ENTRY));
        SignalQueueEntry->Parameters.SignalNumber = SignalNumber;
        SignalQueueEntry->Parameters.SignalCode = SignalCode;
        CurrentProcess = PsGetCurrentProcess();
        SignalQueueEntry->Parameters.FromU.SendingProcess =
                                         CurrentProcess->Identifiers.ProcessId;

        SignalQueueEntry->Parameters.Parameter = SignalParameter;
        SignalQueueEntry->CompletionRoutine = PsDefaultSignalCompletionRoutine;
        PsSignalProcess(Process,
                        SignalQueueEntry->Parameters.SignalNumber,
                        SignalQueueEntry);
    }

SignalProcessEnd:
    return Status;
}

BOOL
PspSendSignalIterator (
    PVOID Context,
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine implements the iterator callback which sends a signal to
    each process it's called on.

Arguments:

    Context - Supplies a pointer's worth of context passed into the iterate
        routine. This is a send signal iterator context.

    Process - Supplies the process to examine.

Return Value:

    FALSE always to indicate the iteration should continue.

--*/

{

    PSEND_SIGNAL_ITERATOR_CONTEXT Iterator;
    PSIGNAL_QUEUE_ENTRY QueueEntry;
    KSTATUS Status;

    Iterator = Context;
    if (Iterator->CurrentThread == NULL) {
        Iterator->CurrentThread = KeGetCurrentThread();
    }

    if ((Process == Iterator->SkipProcess) ||
        (Process == PsGetKernelProcess())) {

        return FALSE;
    }

    if (Iterator->CheckPermissions != FALSE) {
        Status = PspCheckSendSignalPermission(Iterator->CurrentThread,
                                              Process,
                                              Iterator->Signal);

        if (!KSUCCESS(Status)) {
            Iterator->Status = Status;
            return FALSE;
        }
    }

    QueueEntry = NULL;
    if (Iterator->QueueEntry != NULL) {
        QueueEntry = MmAllocatePagedPool(sizeof(SIGNAL_QUEUE_ENTRY),
                                         PS_ALLOCATION_TAG);

        if (QueueEntry == NULL) {
            Iterator->Status = STATUS_INSUFFICIENT_RESOURCES;

        } else {
            RtlCopyMemory(QueueEntry,
                          Iterator->QueueEntry,
                          sizeof(SIGNAL_QUEUE_ENTRY));
        }
    }

    PsSignalProcess(Process, Iterator->Signal, QueueEntry);
    Iterator->SentSignals += 1;
    return FALSE;
}

KSTATUS
PspCheckSendSignalPermission (
    PKTHREAD CurrentThread,
    PKPROCESS Process,
    ULONG Signal
    )

/*++

Routine Description:

    This routine ensures the current process has permission to send a signal to
    the given process.

Arguments:

    CurrentThread - Supplies a pointer to the current thread.

    Process - Supplies a pointer to the potential recipient of a signal.

    Signal - Supplies the proposed signal to send.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if the process is a zombie.

    STATUS_PERMISSION_DENIED on failure.

--*/

{

    PKPROCESS CurrentProcess;
    THREAD_IDENTITY Identity;
    KSTATUS Status;

    CurrentProcess = CurrentThread->OwningProcess;
    Status = PspGetProcessIdentity(Process, &Identity);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if ((CurrentThread->Identity.EffectiveUserId == Identity.RealUserId) ||
        (CurrentThread->Identity.RealUserId == Identity.RealUserId) ||
        (CurrentThread->Identity.EffectiveUserId == Identity.SavedUserId) ||
        (CurrentThread->Identity.RealUserId == Identity.SavedUserId)) {

        return STATUS_SUCCESS;
    }

    //
    // Continue can be sent to any process in this process' session.
    //

    if (Signal == SIGNAL_CONTINUE) {
        if (CurrentProcess->Identifiers.SessionId ==
            Process->Identifiers.SessionId) {

            return STATUS_SUCCESS;
        }
    }

    //
    // Check for the overriding permission of the superuser.
    //

    Status = PsCheckPermission(PERMISSION_KILL);
    return Status;
}

VOID
PspUpdateSignalPending (
    VOID
    )

/*++

Routine Description:

    This routine updates the signal pending state for the current thread based
    on the current pending signal masks and the blocked signal mask.

Arguments:

    None.

Return Value:

    None.

--*/

{

    SIGNAL_SET PendingSignals;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();

    ASSERT(KeIsQueuedLockHeld(Thread->OwningProcess->QueuedLock) != FALSE);

    OR_SIGNAL_SETS(PendingSignals,
                   Thread->PendingSignals,
                   Thread->OwningProcess->PendingSignals);

    REMOVE_SIGNALS_FROM_SET(PendingSignals, Thread->BlockedSignals);
    if (IS_SIGNAL_SET_EMPTY(PendingSignals) != FALSE) {
        Thread->SignalPending = ThreadNoSignalPending;

    } else {
        Thread->SignalPending = ThreadSignalPending;
    }

    return;
}

VOID
PspMoveSignalSet (
    SIGNAL_SET SignalSet
    )

/*++

Routine Description:

    This routine attempts to "move" the given set of signals to other threads
    in the process. Moving simply consists of finding a thread that does not
    block the signal and making sure it is awake. There is no protection
    against rewaking the current thread, but it is assume that the current
    thread is marked as exiting or has the given signal set blocked.

Arguments:

    SignalSet - Supplies a set of signals that need to moved to other threads
        in the current process.

Return Value:

    None.

--*/

{

    SIGNAL_SET PendingSignals;
    PKPROCESS Process;
    PKTHREAD Thread;
    PLIST_ENTRY ThreadEntry;
    SIGNAL_SET ThreadPendingSignals;

    Process = PsGetCurrentProcess();

    ASSERT(KeIsQueuedLockHeld(Process->QueuedLock) != FALSE);

    //
    // If the set is empty, there is no work to do.
    //

    if (IS_SIGNAL_SET_EMPTY(SignalSet) != FALSE) {
        return;
    }

    //
    // Wake up threads until all the process-wide pending signals in the signal
    // set are accounted for.
    //

    AND_SIGNAL_SETS(PendingSignals, Process->PendingSignals, SignalSet);
    ThreadEntry = Process->ThreadListHead.Next;
    while ((IS_SIGNAL_SET_EMPTY(PendingSignals) == FALSE) &&
           (ThreadEntry != &(Process->ThreadListHead))) {

        Thread = LIST_VALUE(ThreadEntry, KTHREAD, ProcessEntry);
        ThreadEntry = ThreadEntry->Next;
        if ((Thread->Flags & THREAD_FLAG_EXITING) != 0) {
            continue;
        }

        //
        // Determine if this thread has any of the pending signals unblocked.
        //

        ThreadPendingSignals = PendingSignals;
        REMOVE_SIGNALS_FROM_SET(ThreadPendingSignals, Thread->BlockedSignals);
        if (IS_SIGNAL_SET_EMPTY(ThreadPendingSignals) != FALSE) {
            continue;
        }

        //
        // Wake this thread up for this batch of signals. It is now responsible
        // for dispatching them.
        //

        if (Thread->SignalPending < ThreadSignalPending) {
            Thread->SignalPending = ThreadSignalPending;
            RtlMemoryBarrier();
            ObWakeBlockedThread(Thread, FALSE);
        }

        //
        // Remove the batch of signals from the set of pending signals.
        //

        REMOVE_SIGNALS_FROM_SET(PendingSignals, ThreadPendingSignals);
    }

    return;
}

