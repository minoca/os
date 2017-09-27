/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sysclock.c

Abstract:

    This module implements system clock support.

Author:

    Evan Green 5-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// The regular debug break poll interval is once per second, and profiling
// events are sent twice per second.
//

#define CLOCK_DEBUG_POLL_EVENT_RATE_SHIFT 0
#define CLOCK_PROFILING_EVENT_RATE_SHIFT 1

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepMaintainClock (
    PPROCESSOR_BLOCK Processor
    );

VOID
KepUpdateSystemTime (
    PPROCESSOR_BLOCK Processor
    );

VOID
KepAddTimeCounterToSystemTime (
    PSYSTEM_TIME SystemTime,
    ULONGLONG TimeCounter
    );

VOID
KepUpdateTimeOffset (
    PSYSTEM_TIME NewTimeOffset
    );

VOID
KepSetTimeOffsetDpc (
    PDPC Dpc
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to true to disable dynamic tick. This reverts back to a periodic
// timer tick that's always running.
//

BOOL KeDisableDynamicTick = FALSE;

//
// Store the number of the processor that "owns" the clock, and is therefore
// responsible for updates to system time. The clock owner can never be
// stolen from a processor, it can only be taken if it belongs to no one. When
// a processor disables its periodic clock, it abandons its ownership of the
// clock.
//

volatile ULONG KeClockOwner = 0;

//
// Store the current periodic clock rate, in time counter ticks.
//

ULONGLONG KeClockRate;

//
// Store the time counter interval for debug events, either polling for a
// break request or sending profiling data. The units here are time counter
// ticks.
//

ULONGLONG KeClockDebugEventRate;
BOOL KeClockProfilingEnabled;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
ULONGLONG
KeGetRecentTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine returns a relatively recent snap of the time counter.

Arguments:

    None.

Return Value:

    Returns the fairly recent snap of the time counter.

--*/

{

    BOOL Enabled;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONGLONG RecentTimestamp;

    Enabled = ArDisableInterrupts();
    ProcessorBlock = KeGetCurrentProcessorBlock();

    ASSERT(KeGetRunLevel() <= RunLevelClock);

    RecentTimestamp = ProcessorBlock->Clock.CurrentTime;
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    return RecentTimestamp;
}

KERNEL_API
VOID
KeGetSystemTime (
    PSYSTEM_TIME Time
    )

/*++

Routine Description:

    This routine returns the current system time.

Arguments:

    Time - Supplies a pointer where the system time will be returned.

Return Value:

    None.

--*/

{

    ULONGLONG TickCount;
    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = MmGetUserSharedData();

    //
    // Loop reading the two tick count values to ensure the read of the system
    // time structure wasn't torn.
    //

    do {
        TickCount = UserSharedData->TickCount;
        *Time = UserSharedData->SystemTime;

    } while (TickCount != UserSharedData->TickCount2);

    return;
}

VOID
KeGetHighPrecisionSystemTime (
    PSYSTEM_TIME Time
    )

/*++

Routine Description:

    This routine returns a high precision snap of the current system time.

Arguments:

    Time - Supplies a pointer that receives the precise system time.

Return Value:

    None.

--*/

{

    ULONGLONG TimeCounter;

    //
    // Get the time offset and time counter and calculate the system time from
    // those two values.
    //

    KepGetTimeOffset(Time);
    TimeCounter = HlQueryTimeCounter();
    KepAddTimeCounterToSystemTime(Time, TimeCounter);
    return;
}

KSTATUS
KeSetSystemTime (
    PSYSTEM_TIME NewTime,
    ULONGLONG TimeCounter
    )

/*++

Routine Description:

    This routine sets the system time.

Arguments:

    NewTime - Supplies a pointer to the new system time to set.

    TimeCounter - Supplies the time counter value corresponding with the
        moment the new system time was meant to be set by the caller.

Return Value:

    Status code.

--*/

{

    ULONGLONG Delta;
    PDPC Dpc;
    ULONGLONG Frequency;
    ULONGLONG Seconds;
    KSTATUS Status;
    SYSTEM_TIME TimeOffset;

    Dpc = NULL;
    Status = PsCheckPermission(PERMISSION_TIME);
    if (!KSUCCESS(Status)) {
        goto SetSystemTimeEnd;
    }

    //
    // Create the DPC first in case the allocation fails.
    //

    if (KeGetCurrentProcessorNumber() != KeClockOwner) {
        Dpc = KeCreateDpc(KepSetTimeOffsetDpc, &TimeOffset);
        if (Dpc == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SetSystemTimeEnd;
        }
    }

    //
    // Adjust the system time backward so that it lines up with a time counter
    // value of zero. This is necessary for setting the time offset.
    //

    TimeOffset = *NewTime;
    Frequency = HlQueryTimeCounterFrequency();
    Seconds = TimeCounter / Frequency;
    TimeOffset.Seconds -= Seconds;
    Delta = TimeCounter - (Seconds * Frequency);

    //
    // Since the seconds were subtracted off, there could be at most a
    // billion nanoseconds to subtract. If the nanoseconds are currently
    // between 0 and a billion like they should be, then this subtract should
    // never underflow. Unless the time counter itself is overflowing
    // constantly, the multiply should also be nowhere near overflowing.
    //

    ASSERT(Frequency <= (MAX_ULONGLONG / NANOSECONDS_PER_SECOND));

    TimeOffset.Nanoseconds -= (Delta * NANOSECONDS_PER_SECOND) / Frequency;

    //
    // Normalize the nanoseconds back into the 0 to 1 billion range.
    //

    if (TimeOffset.Nanoseconds < 0) {
        TimeOffset.Nanoseconds += NANOSECONDS_PER_SECOND;
        TimeOffset.Seconds -= 1;

        ASSERT((TimeOffset.Nanoseconds > 0) &&
               (TimeOffset.Nanoseconds < NANOSECONDS_PER_SECOND));
    }

    //
    // Update the time offset. Once the time offset is updated, then the next
    // clock interrupt should pick up the new system time and update the user
    // shared data page appropriately.
    //

    Status = KepSetTimeOffset(&TimeOffset, Dpc);

    ASSERT(KSUCCESS(Status));

    //
    // Call into the hardware layer to set the calendar time. This may fail if
    // there is no calendar time device.
    //

    Status = HlUpdateCalendarTime();
    if (!KSUCCESS(Status) && (Status != STATUS_NO_SUCH_DEVICE)) {
        goto SetSystemTimeEnd;
    }

    Status = STATUS_SUCCESS;

SetSystemTimeEnd:
    if (Dpc != NULL) {
        KeDestroyDpc(Dpc);
    }

    return Status;
}

KERNEL_API
KSTATUS
KeDelayExecution (
    BOOL Interruptible,
    BOOL TimeTicks,
    ULONGLONG Interval
    )

/*++

Routine Description:

    This routine blocks the current thread for the specified amount of time.
    This routine can only be called at low level.

Arguments:

    Interruptible - Supplies a boolean indicating if the wait can be
        interrupted by a dispatched signal. If TRUE, the caller must check the
        return status code to see if the wait expired or was interrupted.

    TimeTicks - Supplies a boolean indicating if the interval parameter is
        represented in time counter ticks (TRUE) or microseconds (FALSE).

    Interval - Supplies the interval to wait. If the time ticks parameter is
        TRUE, this parameter represents an absolute time in time counter ticks.
        If the time ticks parameter is FALSE, this parameter represents a
        relative time from now in microseconds. If an interval of 0 is
        supplied, this routine is equivalent to KeYield.

Return Value:

    STATUS_SUCCESS if the wait completed.

    STATUS_INTERRUPTED if the wait was interrupted.

--*/

{

    ULONGLONG DueTime;
    ULONG Flags;
    KSTATUS Status;
    PKTHREAD Thread;
    PKTIMER Timer;

    if (Interval == 0) {
        KeYield();
        return STATUS_SUCCESS;
    }

    //
    // Use the thread's builtin timer, which means that no other waits can
    // occur during this routine.
    //

    Thread = KeGetCurrentThread();
    Timer = Thread->BuiltinTimer;
    Flags = 0;
    if (Interruptible != FALSE) {
        Flags = WAIT_FLAG_INTERRUPTIBLE;
    }

    if (TimeTicks != FALSE) {
        DueTime = Interval;

    } else {
        DueTime = HlQueryTimeCounter() +
                  KeConvertMicrosecondsToTimeTicks(Interval);
    }

    Status = KeQueueTimer(Timer, TimerQueueSoftWake, DueTime, 0, 0, NULL);

    ASSERT(KSUCCESS(Status));

    //
    // Wait for the timer, being careful to pass a timeout of "infinite" to the
    // object manager routine to ensure it doesn't also try to use the timer.
    //

    Status = ObWaitOnObject(Timer, Flags, WAIT_TIME_INDEFINITE);
    if (!KSUCCESS(Status)) {

        //
        // Cancel the timer if the wait was interrupted.
        //

        KeCancelTimer(Timer);
    }

    return Status;
}

KERNEL_API
KSTATUS
KeGetProcessorCycleAccounting (
    ULONG ProcessorNumber,
    PPROCESSOR_CYCLE_ACCOUNTING Accounting
    )

/*++

Routine Description:

    This routine returns a snapshot of the given processor's cycle accounting
    information.

Arguments:

    ProcessorNumber - Supplies the processor number to query.

    Accounting - Supplies a pointer where the processor accounting information
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid processor number was supplied.

--*/

{

    PROCESSOR_CYCLE_ACCOUNTING Copy;
    PPROCESSOR_BLOCK ProcessorBlock;

    if (ProcessorNumber >= KeGetActiveProcessorCount()) {
        return STATUS_INVALID_PARAMETER;
    }

    ProcessorBlock = KeProcessorBlocks[ProcessorNumber];

    //
    // Loop until the same thing was read twice.
    //

    do {
        Accounting->UserCycles = ProcessorBlock->UserCycles;
        Accounting->KernelCycles = ProcessorBlock->KernelCycles;
        Accounting->InterruptCycles = ProcessorBlock->InterruptCycles;
        Accounting->IdleCycles = ProcessorBlock->IdleCycles;
        RtlMemoryBarrier();
        Copy.UserCycles = ProcessorBlock->UserCycles;
        Copy.KernelCycles = ProcessorBlock->KernelCycles;
        Copy.InterruptCycles = ProcessorBlock->InterruptCycles;
        Copy.IdleCycles = ProcessorBlock->IdleCycles;

    } while ((Copy.UserCycles != Accounting->UserCycles) ||
             (Copy.KernelCycles != Accounting->KernelCycles) ||
             (Copy.InterruptCycles != Accounting->InterruptCycles) ||
             (Copy.IdleCycles != Accounting->IdleCycles));

    return STATUS_SUCCESS;
}

KERNEL_API
VOID
KeGetTotalProcessorCycleAccounting (
    PPROCESSOR_CYCLE_ACCOUNTING Accounting
    )

/*++

Routine Description:

    This routine returns a snapshot of the accumulation of all processors'
    cycle accounting information.

Arguments:

    Accounting - Supplies a pointer where the processor accounting information
        will be returned.

Return Value:

    None.

--*/

{

    PROCESSOR_CYCLE_ACCOUNTING ProcessorAccounting;
    ULONG ProcessorCount;
    ULONG ProcessorIndex;
    KSTATUS Status;

    RtlZeroMemory(Accounting, sizeof(PROCESSOR_CYCLE_ACCOUNTING));
    ProcessorCount = KeGetActiveProcessorCount();
    for (ProcessorIndex = 0;
         ProcessorIndex < ProcessorCount;
         ProcessorIndex += 1) {

        Status = KeGetProcessorCycleAccounting(ProcessorIndex,
                                               &ProcessorAccounting);

        ASSERT(KSUCCESS(Status));

        Accounting->UserCycles += ProcessorAccounting.UserCycles;
        Accounting->KernelCycles += ProcessorAccounting.KernelCycles;
        Accounting->InterruptCycles += ProcessorAccounting.InterruptCycles;
        Accounting->IdleCycles += ProcessorAccounting.IdleCycles;
    }

    return;
}

INTN
KeSysDelayExecution (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for delaying execution of the
    current thread by a specified amount of time.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSYSTEM_CALL_DELAY_EXECUTION Parameters;

    Parameters = (PSYSTEM_CALL_DELAY_EXECUTION)SystemCallParameter;
    return KeDelayExecution(TRUE, Parameters->TimeTicks, Parameters->Interval);
}

INTN
KeSysSetSystemTime (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for setting the system time.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PSYSTEM_CALL_SET_SYSTEM_TIME Parameters;
    KSTATUS Status;

    Parameters = (PSYSTEM_CALL_SET_SYSTEM_TIME)SystemCallParameter;
    Status = KeSetSystemTime(&(Parameters->SystemTime),
                             Parameters->TimeCounter);

    if (!KSUCCESS(Status)) {
        goto SysSetSystemTimeEnd;
    }

SysSetSystemTimeEnd:
    return Status;
}

VOID
KeClockInterrupt (
    VOID
    )

/*++

Routine Description:

    This routine handles periodic clock interrupts, updating system time and
    providing pre-emptive scheduling.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG ClockOwner;
    ULONGLONG CurrentTime;
    PPROCESSOR_BLOCK ProcessorBlock;

    ASSERT(KeGetRunLevel() == RunLevelClock);

    ProcessorBlock = KeGetCurrentProcessorBlock();
    ProcessorBlock->Clock.InterruptCount += 1;

    //
    // If the clock is unowned, try to become the clock owner. If this
    // processor won, it will update system time. If it lost, another processor
    // must be doing it. The clock owner can never be stolen from a processor,
    // only given away.
    //

    ClockOwner = KeClockOwner;
    if (ClockOwner == (ULONG)-1) {
        ClockOwner = RtlAtomicCompareExchange32(&KeClockOwner,
                                                ProcessorBlock->ProcessorNumber,
                                                -1);

        if (ClockOwner == (ULONG)-1) {
            ClockOwner = ProcessorBlock->ProcessorNumber;
        }
    }

    if (ProcessorBlock->ProcessorNumber == ClockOwner) {
        KepUpdateSystemTime(ProcessorBlock);

    } else {
        CurrentTime = HlQueryTimeCounter();
        ProcessorBlock->Clock.CurrentTime = CurrentTime;
    }

    //
    // Maintain the debugger connection.
    //

    if (ProcessorBlock->Clock.CurrentTime >=
        ProcessorBlock->Clock.NextDebugEvent) {

        //
        // Send profiling data (which also checks for a debug break), or just
        // check for a debug break. Sending profiling data can take a bit of
        // time, so take another snap of the time counter when calculating the
        // next event time.
        //

        if (KeClockProfilingEnabled != FALSE) {
            SpSendProfilingData();
            ProcessorBlock->Clock.NextDebugEvent =
                                  HlQueryTimeCounter() + KeClockDebugEventRate;

        } else {
            KdPollForBreakRequest();
            ProcessorBlock->Clock.NextDebugEvent =
                     ProcessorBlock->Clock.CurrentTime + KeClockDebugEventRate;
        }
    }

    KepMaintainClock(ProcessorBlock);

    //
    // Queue a dispatch interrupt to run the scheduler.
    //

    ProcessorBlock->PendingDispatchInterrupt = TRUE;
    return;
}

ULONG
KeGetClockInterruptCount (
    ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine returns the clock interrupt count of the given processor.

Arguments:

    ProcessorNumber - Supplies the number of the processor whose clock
        interrupt count is to be returned.

Return Value:

    Returns the given processor's clock interrupt count.

--*/

{

    ASSERT(ProcessorNumber < KeActiveProcessorCount);

    return KeProcessorBlocks[ProcessorNumber]->Clock.InterruptCount;
}

VOID
KeUpdateClockForProfiling (
    BOOL ProfilingEnabled
    )

/*++

Routine Description:

    This routine configures the clock interrupt handler for profiling.

Arguments:

    ProfilingEnabled - Supplies a boolean indicating if profiling is being
        enabled (TRUE) or disabled (FALSE).

Return Value:

    None.

--*/

{

    ULONGLONG Interval;

    Interval = HlQueryTimeCounterFrequency();
    if (ProfilingEnabled != FALSE) {
        Interval >>= CLOCK_PROFILING_EVENT_RATE_SHIFT;

    } else {
        Interval >>= CLOCK_DEBUG_POLL_EVENT_RATE_SHIFT;
    }

    if (Interval == 0) {
        Interval = 1;
    }

    KeClockDebugEventRate = Interval;
    KeClockProfilingEnabled = ProfilingEnabled;
    return;
}

VOID
KeDispatchSoftwareInterrupt (
    RUNLEVEL RunLevel,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles a software interrupt. Consider it the ISR for
    software interrupts. On entry, interrupts are disabled. This routine may
    enable interrupts, but must exit with the interrupts disabled.

Arguments:

    RunLevel - Supplies the run level that that interrupt occurred on.

    TrapFrame - Supplies an optional pointer to the trap frame if this interrupt
        is being dispatched off a hardware interrupt. Supplying this variable
        enables checking for any pending user-mode signals.

Return Value:

    None.

--*/

{

    PPROCESSOR_BLOCK ProcessorBlock;
    ULONGLONG TimeCounter;

    if (RunLevel == RunLevelDispatch) {

        //
        // While interrupts are disabled, collect a recent snap of the time
        // counter.
        //

        ProcessorBlock = KeGetCurrentProcessorBlock();
        TimeCounter = ProcessorBlock->Clock.CurrentTime;

        //
        // Run any pending DPCs. This routine enters with interrupts disabled
        // and exits with them enabled.
        //

        KepExecutePendingDpcs();

        //
        // Expire any timers. This does not need to be before DPCs, because any
        // DPC queued by a timer will run immediately as the processor's run
        // level is dispatch and the timers will queue the DPCs on the current
        // processor.
        //

        KepDispatchTimers(TimeCounter);
        KeSchedulerEntry(SchedulerReasonDispatchInterrupt);
        ArDisableInterrupts();

    //
    // Other types of software interrupts are not known.
    //

    } else {

        ASSERT(FALSE);
    }

    return;
}

CYCLE_ACCOUNT
KeBeginCycleAccounting (
    CYCLE_ACCOUNT CycleAccount
    )

/*++

Routine Description:

    This routine begins a new period of cycle accounting for the current
    processor.

Arguments:

    CycleAccount - Supplies the type of time to attribute these cycles to.

Return Value:

    Returns the previous type that cycles were being attributed to.

--*/

{

    ULONGLONG CurrentCount;
    ULONGLONG Delta;
    BOOL Enabled;
    ULONGLONG PreviousCount;
    CYCLE_ACCOUNT PreviousPeriod;
    PPROCESSOR_BLOCK Processor;
    PKTHREAD Thread;

    //
    // If the run-level is below dispatch, disable interrupts to prevent
    // migrating around processors.
    //

    Enabled = FALSE;
    if (KeGetRunLevel() < RunLevelDispatch) {
        Enabled = ArDisableInterrupts();
    }

    Processor = KeGetCurrentProcessorBlock();

    //
    // Perform the transition.
    //

    PreviousPeriod = Processor->CyclePeriodAccount;
    PreviousCount = Processor->CyclePeriodStart;
    CurrentCount = HlQueryProcessorCounter();
    Processor->CyclePeriodAccount = CycleAccount;
    Processor->CyclePeriodStart = CurrentCount;
    Delta = CurrentCount - PreviousCount;

    //
    // Charge somebody for those cycles.
    //

    switch (PreviousPeriod) {
    case CycleAccountUser:
        Thread = Processor->RunningThread;
        if (Thread != NULL) {
            Thread->ResourceUsage.UserCycles += Delta;
        }

        Processor->UserCycles += Delta;
        break;

    case CycleAccountKernel:
        Thread = Processor->RunningThread;
        if (Thread != NULL) {
            Thread->ResourceUsage.KernelCycles += Delta;
        }

        Processor->KernelCycles += Delta;
        break;

    case CycleAccountInterrupt:
        Processor->InterruptCycles += Delta;
        break;

    case CycleAccountIdle:
        Processor->IdleCycles += Delta;
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    return PreviousPeriod;
}

VOID
KepPostContextSwapWork (
    VOID
    )

/*++

Routine Description:

    This routine performs cleanup work necessary after a thread has context
    swapped out. It should be called ONLY from the scheduler or during new
    thread initialization.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKTHREAD PreviousThread;
    PPROCESSOR_BLOCK Processor;
    BOOL ThreadWasWoken;

    ASSERT((KeGetRunLevel() == RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    Processor = KeGetCurrentProcessorBlock();
    MmSwitchAddressSpace(Processor,
                         Processor->RunningThread->OwningProcess->AddressSpace);

    if (Processor->PreviousThread != NULL) {
        PreviousThread = Processor->PreviousThread;
        Processor->PreviousThread = NULL;
        switch (PreviousThread->State) {

        //
        // The thread wasn't blocking, set it to ready to make it eligible
        // for being run or stolen by another processor.
        //

        case ThreadStateRunning:
            PreviousThread->State = ThreadStateReady;
            break;

        //
        // If the thread is exited, queue the thread cleanup.
        //

        case ThreadStateExited:
            PsQueueThreadCleanup(PreviousThread);
            break;

        //
        // If a thread is blocking, check for any pending signals that are
        // trying to fire it back up. This must be done before marking the
        // thread as fully blocked, because once it is fully blocked it may run
        // and exit on another core. This means the last operation this routine
        // should do on the thread is set its state.
        //

        case ThreadStateBlocking:

            //
            // If there's a signal pending, fire up the thread again.
            //

            ThreadWasWoken = FALSE;
            if (PreviousThread->SignalPending == ThreadSignalPending) {
                ThreadWasWoken = ObWakeBlockingThread(PreviousThread);
            }

            if (ThreadWasWoken == FALSE) {
                PreviousThread->State = ThreadStateBlocked;
            }

            break;

        //
        // If a thread is suspending, check for any pending signals that are
        // trying to fire it back up. This must be done before marking the
        // thread as fully suspended, because once it is fully suspended it may
        // run and exit on another core. This means the last operation this
        // routine should do on the thread is set its state.
        //

        case ThreadStateSuspending:

            //
            // If there's a signal or child signal pending, fire up the thread
            // again.
            //

            if (PreviousThread->SignalPending >= ThreadChildSignalPending) {
                ThreadWasWoken = ObWakeBlockingThread(PreviousThread);

                ASSERT(ThreadWasWoken != FALSE);

            } else {
                PreviousThread->State = ThreadStateSuspended;
            }

            break;

        //
        // It's not clear why there is a previous thread set.
        //

        default:

            ASSERT(FALSE);

            break;
        }
    }

    return;
}

VOID
KepPreThreadStartWork (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine performs any work needed on a new thread right before it
    begins.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame to be restored to
        initialize this thread.

Return Value:

    None.

--*/

{

    PKTHREAD Thread;

    Thread = KeGetCurrentThread();

    //
    // If this is a cloned thread that already has a thread ID pointer in it,
    // write the new thread ID in there now that execution is on the child
    // process page tables.
    //

    if (Thread->ThreadIdPointer != NULL) {
        MmUserWrite32(Thread->ThreadIdPointer, Thread->ThreadId);
    }

    //
    // The thread may have already got a signal pending on it.
    //

    if ((Thread->Flags & THREAD_FLAG_USER_MODE) != 0) {
        ArEnableInterrupts();
        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
    }

    return;
}

VOID
KepGetTimeOffset (
    PSYSTEM_TIME TimeOffset
    )

/*++

Routine Description:

    This routine reads the time offset from the shared user data page.

Arguments:

    TimeOffset - Supplies a pointer that receives the time offset from the
        shared user data page.

Return Value:

    None.

--*/

{

    ULONGLONG TickCount;
    PUSER_SHARED_DATA UserSharedData;

    UserSharedData = MmGetUserSharedData();

    //
    // Loop reading the two tick count values to ensure the read of the time
    // offset structure wasn't torn.
    //

    do {
        TickCount = UserSharedData->TickCount;
        *TimeOffset = UserSharedData->TimeOffset;

    } while (TickCount != UserSharedData->TickCount2);

    return;
}

KSTATUS
KepSetTimeOffset (
    PSYSTEM_TIME NewTimeOffset,
    PDPC Dpc
    )

/*++

Routine Description:

    This routine sets the time offset in the shared user data page. For
    synchronization purposes, the time offset can only be updated by the clock
    owner at the clock run level. If the caller requires this routine to
    succeed, then a DPC can be supplied, otherwise the DPC will be allocated if
    necessary and said allocation could fail.

Arguments:

    NewTimeOffset - Supplies a pointer to the new time offset. This cannot be
        a pointer to paged pool as it may be used at dispatch level by the DPC.

    Dpc - Supplies a pointer to an optional DPC to use when tracking down the
        clock owner.

Return Value:

    Status code.

--*/

{

    ULONG ClockOwner;
    ULONG CurrentProcessor;
    BOOL DpcAllocated;
    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    CurrentProcessor = KeGetCurrentProcessorNumber();

    //
    // Try to become the clock owner, assuming it's not owned.
    //

    ClockOwner = RtlAtomicCompareExchange32(&KeClockOwner,
                                            CurrentProcessor,
                                            -1);

    if (ClockOwner == (ULONG)-1) {
        ClockOwner = CurrentProcessor;
    }

    //
    // The time offset in the user shared data page can only be updated by the
    // clock owner at the clock run level. Either schedule a DPC to run on the
    // clock owner, or update it directly here.
    //

    if (ClockOwner != CurrentProcessor) {
        DpcAllocated = FALSE;
        if (Dpc == NULL) {
            Dpc = KeCreateDpc(KepSetTimeOffsetDpc, NewTimeOffset);
            if (Dpc == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            DpcAllocated = TRUE;
        }

        KeQueueDpcOnProcessor(Dpc, ClockOwner);
        KeFlushDpc(Dpc);
        if (DpcAllocated != FALSE) {
            KeDestroyDpc(Dpc);
        }

    } else {
        KeRaiseRunLevel(RunLevelClock);
        KepUpdateTimeOffset(NewTimeOffset);
    }

    KeLowerRunLevel(OldRunLevel);
    return STATUS_SUCCESS;
}

VOID
KepInitializeClock (
    PPROCESSOR_BLOCK Processor
    )

/*++

Routine Description:

    This routine initializes system clock information.

Arguments:

    Processor - Supplies a pointer to the processor block being initialized.

Return Value:

    None.

--*/

{

    ULONGLONG TimeCounterFrequency;

    if (Processor->ProcessorNumber == 0) {

        //
        // Initialize the clock rate, in time counter ticks.
        //

        TimeCounterFrequency = HlQueryTimeCounterFrequency();

        ASSERT(TimeCounterFrequency != 0);

        KeClockRate = (TimeCounterFrequency * DEFAULT_CLOCK_RATE) / 10000000ULL;
        if (KeClockRate == 0) {
            KeClockRate = 1;
        }
    }

    Processor->Clock.Mode = ClockTimerPeriodic;
    Processor->Clock.NextMode = ClockTimerPeriodic;
    return;
}

VOID
KepUpdateClockDeadline (
    VOID
    )

/*++

Routine Description:

    This routine is called when the next clock deadline is potentially changed.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONGLONG Deadline;
    BOOL Hard;
    ULONGLONG NextDeadline;
    ULONGLONG NextOneShotDeadline;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK Processor;

    Processor = KeGetCurrentProcessorBlock();
    NextOneShotDeadline = Processor->Clock.DueTime;
    NextDeadline = NextOneShotDeadline;
    Deadline = KepGetNextTimerDeadline(Processor, &Hard);

    //
    // Based on the current mode, figure out when the clock will fire next.
    //

    switch (Processor->Clock.Mode) {
    case ClockTimerPeriodic:

        //
        // If this is a hard deadline, then figure out if the next periodic
        // time is going to be before or after the deadline.
        //

        if (Hard != FALSE) {
            NextDeadline = KeGetRecentTimeCounter() + KeClockRate;

        //
        // If it's not a hard deadline, then the next periodic clock interrupt
        // will do just fine.
        //

        } else {
            NextDeadline = 0;
        }

        break;

    case ClockTimerOneShot:
        break;

    case ClockTimerOff:
        NextOneShotDeadline = -1ULL;
        NextDeadline = NextOneShotDeadline;
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    //
    // Mark the new deadline if it's the winner, no matter what mode the clock
    // is in.
    //

    if (Deadline < NextOneShotDeadline) {
        Processor->Clock.DueTime = Deadline;
        Processor->Clock.Hard = Hard;
    }

    //
    // If the new deadline is coming up before the next scheduled clock
    // interrupt, re-schedule the clock.
    //

    if (Deadline < NextDeadline) {
        OldRunLevel = KeRaiseRunLevel(RunLevelClock);
        HlSetClockTimer(ClockTimerOneShot, Deadline, Hard);
        Processor->Clock.Mode = ClockTimerOneShot;
        Processor->Clock.NextMode = ClockTimerOneShot;
        KeLowerRunLevel(OldRunLevel);
    }

    return;
}

VOID
KepClockIdle (
    PPROCESSOR_BLOCK Processor
    )

/*++

Routine Description:

    This routine is called when the processor goes idle. It potentially
    requests a clock transition to disable the clock.

Arguments:

    Processor - Supplies a pointer to the current processor block.

Return Value:

    None.

--*/

{

    switch (Processor->Clock.Mode) {
    case ClockTimerPeriodic:

        //
        // If there are threads ready, stay in periodic mode.
        //

        if (Processor->Scheduler.Group.ReadyThreadCount != 0) {
            break;
        }

        //
        // Request a transition to one-shot mode. Don't do the transition now
        // because it may lead to ugly ping-ponging of going idle briefly then
        // having work scheduled, which means needless setting of the hardware
        // clock timer. The grace period is a single clock cycle, which limits
        // the hammering to once every clock period.
        //

        if (KeDisableDynamicTick == FALSE) {
            Processor->Clock.NextMode = ClockTimerOneShot;
        }

        break;

    //
    // If the clock is already set to one-shot or off, don't fuss with it, just
    // go down. New threads or timers being scheduled on this processor are
    // responsible for switching out of this mode.
    //

    case ClockTimerOneShot:
    case ClockTimerOff:
        break;

    //
    // Corrupt data structures.
    //

    default:

        ASSERT(FALSE);

        Processor->Clock.Mode = ClockTimerOff;
        break;
    }

    return;
}

VOID
KepSetClockToPeriodic (
    PPROCESSOR_BLOCK Processor
    )

/*++

Routine Description:

    This routine sets the clock to be periodic on the given processor. This
    routine must be called at or above dispatch level.

Arguments:

    Processor - Supplies a pointer to the processor block for the processor
        whose clock should be switched to periodic.

Return Value:

    None.

--*/

{

    PPROCESSOR_BLOCK CurrentProcessor;
    ULONGLONG NextClockTick;
    RUNLEVEL OldRunLevel;
    PROCESSOR_SET ProcessorTarget;

    ASSERT(KeGetRunLevel() >= RunLevelDispatch);

    CurrentProcessor = KeGetCurrentProcessorBlock();

    //
    // If it's not this processor, then send a clock interrupt to that
    // processor to force it to wake up and deal with life.
    //

    if (Processor != CurrentProcessor) {
        ProcessorTarget.Target = ProcessorTargetSingleProcessor;
        ProcessorTarget.U.Number = Processor->ProcessorNumber;
        HlSendIpi(IpiTypeClock, &ProcessorTarget);
        return;
    }

    //
    // Go periodic on the current processor, which depends on the current
    // mode.
    //

    switch (Processor->Clock.Mode) {

    //
    // If it's already periodic, great. Leave it as such.
    //

    case ClockTimerPeriodic:
        break;

    //
    // If the clock was in one-shot mode, check to see if its deadline is
    // within the normal clock rate.
    //

    case ClockTimerOneShot:
        NextClockTick = KeGetRecentTimeCounter() + KeClockRate;

        //
        // If the next clock tick is sooner than when the timer would fire
        // anyway, then the timer needs to be re-armed now.
        //

        if (NextClockTick < CurrentProcessor->Clock.DueTime) {
            OldRunLevel = KeRaiseRunLevel(RunLevelClock);
            HlSetClockTimer(ClockTimerPeriodic, 0, FALSE);
            Processor->Clock.Mode = ClockTimerPeriodic;
            CurrentProcessor->Clock.NextMode = ClockTimerPeriodic;
            KeLowerRunLevel(OldRunLevel);
        }

        break;

    //
    // If the clock is off, then turn it on.
    //

    case ClockTimerOff:
        OldRunLevel = KeRaiseRunLevel(RunLevelClock);
        HlSetClockTimer(ClockTimerPeriodic, 0, FALSE);
        Processor->Clock.Mode = ClockTimerPeriodic;
        CurrentProcessor->Clock.NextMode = ClockTimerPeriodic;
        KeLowerRunLevel(OldRunLevel);
        break;

    //
    // Corrupt data structures.
    //

    default:

        ASSERT(FALSE);

        OldRunLevel = KeRaiseRunLevel(RunLevelClock);
        HlSetClockTimer(ClockTimerPeriodic, 0, FALSE);
        Processor->Clock.Mode = ClockTimerPeriodic;
        CurrentProcessor->Clock.NextMode = ClockTimerPeriodic;
        KeLowerRunLevel(OldRunLevel);
        break;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepMaintainClock (
    PPROCESSOR_BLOCK Processor
    )

/*++

Routine Description:

    This routine is called from within the clock handler whenever a clock
    interrupt occurs. It potentially performs clock transitions. This routine
    must be called at clock level.

Arguments:

    Processor - Supplies a pointer to the processor.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentTime;
    ULONGLONG NextDeadline;

    //
    // If already in one-shot mode, set the current mode to off, as this
    // interrupt was probably the one-shot timer firing.
    //

    if (Processor->Clock.Mode == ClockTimerOneShot) {
        Processor->Clock.Mode = ClockTimerOff;
    }

    //
    // If there are threads ready, the clock must be set to periodic mode.
    //

    if (Processor->Scheduler.Group.ReadyThreadCount != 0) {
        Processor->Clock.NextMode = ClockTimerPeriodic;
    }

    //
    // If currently in periodic mode and the next deadline is hard, check to
    // see if it's before the next clock cycle. If so, this interrupt needs to
    // be one-shot.
    //

    if ((Processor->Clock.NextMode == ClockTimerPeriodic) &&
        (Processor->Clock.AnyHard != FALSE)) {

        NextDeadline = KepGetNextTimerDeadline(Processor,
                                               &(Processor->Clock.Hard));

        Processor->Clock.DueTime = NextDeadline;
        if (Processor->Clock.Hard != FALSE) {
            CurrentTime = Processor->Clock.CurrentTime;
            if (CurrentTime + KeClockRate > Processor->Clock.DueTime) {
                Processor->Clock.NextMode = ClockTimerOneShot;
            }
        }
    }

    //
    // Take the fast path if there's no transition.
    //

    if (Processor->Clock.Mode == Processor->Clock.NextMode) {

        //
        // Give away the clock if not in periodic mode.
        //

        if ((Processor->Clock.Mode != ClockTimerPeriodic) &&
            (KeClockOwner == Processor->ProcessorNumber)) {

            RtlAtomicExchange32(&KeClockOwner, -1);
        }

        //
        // This is to make sure that clock timers that do not support one-shot
        // mode are no longer firing if they are meant to be off.
        //

        if (Processor->Clock.Mode == ClockTimerOff) {
            HlSetClockTimer(ClockTimerOff, 0, FALSE);
        }

        return;
    }

    //
    // If a clock transition was requested, do it.
    //

    switch (Processor->Clock.NextMode) {
    case ClockTimerPeriodic:
        HlSetClockTimer(ClockTimerPeriodic, 0, FALSE);
        break;

    case ClockTimerOneShot:
        NextDeadline = KepGetNextTimerDeadline(Processor,
                                               &(Processor->Clock.Hard));

        Processor->Clock.DueTime = NextDeadline;
        if (NextDeadline == -1ULL) {
            Processor->Clock.NextMode = ClockTimerOff;
            HlSetClockTimer(ClockTimerOff, 0, FALSE);

        } else {
            CurrentTime = Processor->Clock.CurrentTime;

            //
            // Set the new one-shot deadline. Don't keep resetting the same
            // deadline, as that would starve out the dispatch timer routine.
            //

            if (CurrentTime <= NextDeadline) {
                HlSetClockTimer(ClockTimerOneShot,
                                NextDeadline,
                                Processor->Clock.Hard);

            } else {
                Processor->Clock.NextMode = ClockTimerOff;
            }
        }

        //
        // If the current processor is the clock owner, abandon it.
        //

        if (KeClockOwner == Processor->ProcessorNumber) {
            RtlAtomicExchange32(&KeClockOwner, -1);
        }

        break;

    case ClockTimerOff:
    default:

        ASSERT(FALSE);

        Processor->Clock.NextMode = ClockTimerPeriodic;
        break;
    }

    Processor->Clock.Mode = Processor->Clock.NextMode;
    return;
}

VOID
KepUpdateSystemTime (
    PPROCESSOR_BLOCK Processor
    )

/*++

Routine Description:

    This routine updates the system time.

Arguments:

    Processor - Supplies a pointer to the current processor block.

Return Value:

    None.

--*/

{

    ULONGLONG LocalTimeCounter;
    SYSTEM_TIME NewSystemTime;
    ULONGLONG TickCount;
    PUSER_SHARED_DATA UserSharedData;

    ASSERT(KeGetRunLevel() == RunLevelClock);
    ASSERT(KeGetCurrentProcessorNumber() == KeClockOwner);

    LocalTimeCounter = HlQueryTimeCounter();
    Processor->Clock.CurrentTime = LocalTimeCounter;
    UserSharedData = MmGetUserSharedData();

    //
    // It is OK to read the time offset directly from the user shared data page
    // here without checking the tick counts because the time offset is only
    // ever updated by the clock owner at clock level.
    //

    NewSystemTime = UserSharedData->TimeOffset;
    KepAddTimeCounterToSystemTime(&NewSystemTime, LocalTimeCounter);
    TickCount = UserSharedData->TickCount;
    TickCount += 1;

    //
    // Write the update to the shared user data page.
    //

    UserSharedData->TickCount = TickCount;
    RtlMemoryBarrier();
    UserSharedData->TimeCounter = LocalTimeCounter;
    UserSharedData->SystemTime = NewSystemTime;

    //
    // Readers use the two tick count variables to ensure they didn't get
    // torn reads of any time values. A memory barrier ensures all these writes
    // went out, and then the second tick count variable can be updated.
    //

    RtlMemoryBarrier();
    UserSharedData->TickCount2 = TickCount;
    return;
}

VOID
KepAddTimeCounterToSystemTime (
    PSYSTEM_TIME SystemTime,
    ULONGLONG TimeCounter
    )

/*++

Routine Description:

    This routine adds the time specified by the time counter value to the given
    system time.

Arguments:

    SystemTime - Supplies a pointer to the base system time to which the time
        interval will be added. Upon return, this parameter will be store the
        final time.

    TimeCounter - Supplies the time counter time duration to add.

Return Value:

    None.

--*/

{

    ULONGLONG Delta;
    ULONGLONG Frequency;
    ULONGLONG Seconds;

    Frequency = HlQueryTimeCounterFrequency();
    Seconds = TimeCounter / Frequency;
    SystemTime->Seconds += Seconds;
    Delta = TimeCounter - (Seconds * Frequency);

    //
    // Since the seconds were subtracted off, there could be at most a billion
    // nanoseconds to add. If the nanoseconds are currently under a billion like
    // they should be, then this add should never overflow. Unless the time
    // counter itself is overflowing constantly, the multiply should also be
    // nowhere near overflowing.
    //

    ASSERT(Frequency <= (MAX_ULONGLONG / NANOSECONDS_PER_SECOND));

    SystemTime->Nanoseconds += (Delta * NANOSECONDS_PER_SECOND) / Frequency;

    //
    // Normalize the nanoseconds back into the 0 to 1 billion range.
    //

    if (SystemTime->Nanoseconds > NANOSECONDS_PER_SECOND) {
        SystemTime->Nanoseconds -= NANOSECONDS_PER_SECOND;
        SystemTime->Seconds += 1;

        ASSERT((SystemTime->Nanoseconds > 0) &&
               (SystemTime->Nanoseconds < NANOSECONDS_PER_SECOND));
    }

    return;
}

VOID
KepUpdateTimeOffset (
    PSYSTEM_TIME NewTimeOffset
    )

/*++

Routine Description:

    This routine updates the time offset in the user shared data page and then
    triggers an update to the system time. It must be called at the clock run
    level on the processor that owns the clock.

Arguments:

    NewTimeOffset - Supplies a pointer to the new time offset to set in the
        shared user data page.

Return Value:

    None.

--*/

{

    PPROCESSOR_BLOCK Processor;
    ULONGLONG TickCount;
    PUSER_SHARED_DATA UserSharedData;

    Processor = KeGetCurrentProcessorBlock();

    ASSERT(KeGetRunLevel() == RunLevelClock);
    ASSERT(Processor->ProcessorNumber == KeClockOwner);

    UserSharedData = MmGetUserSharedData();
    TickCount = UserSharedData->TickCount;
    TickCount += 1;

    //
    // Write the update to the shared user data page.
    //

    UserSharedData->TickCount = TickCount;
    UserSharedData->TimeOffset = *NewTimeOffset;

    //
    // Readers use the two tick count variables to ensure they don't get
    // torn reads of the time offset. A memory barrier ensures the write went
    // out, and then the second tick count variable can be updated.
    //

    RtlMemoryBarrier();
    UserSharedData->TickCount2 = TickCount;

    //
    // Now that the time offset is updated, update the system time.
    //

    KepUpdateSystemTime(Processor);
    return;
}

VOID
KepSetTimeOffsetDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine attempts to update the system time on the processor that owns
    the clock.

Arguments:

    Dpc - Supplies a pointer to the DPC that is currently running.

Return Value:

    None.

--*/

{

    ULONG ClockOwner;
    ULONG CurrentProcessor;
    RUNLEVEL OldRunLevel;

    //
    // If the clock owner changed since this DPC was queued, queue it again on
    // the correct processor.
    //

    CurrentProcessor = KeGetCurrentProcessorNumber();

    //
    // Try to become the clock owner, assuming it's not owned.
    //

    ClockOwner = RtlAtomicCompareExchange32(&KeClockOwner,
                                            CurrentProcessor,
                                            -1);

    if (ClockOwner == (ULONG)-1) {
        ClockOwner = CurrentProcessor;
    }

    if (CurrentProcessor != ClockOwner) {
        KeQueueDpcOnProcessor(Dpc, ClockOwner);

    //
    // Otherwise, raise to clock level and update the system time. The clock
    // owner shouldn't change at this point since code is running on the owning
    // processor.
    //

    } else {
        OldRunLevel = KeRaiseRunLevel(RunLevelClock);
        KepUpdateTimeOffset((PSYSTEM_TIME)Dpc->UserData);
        KeLowerRunLevel(OldRunLevel);
    }

    return;
}

