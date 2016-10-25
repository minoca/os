/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements support for software timers in the kernel.

Author:

    Evan Green 2-Feb-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the internal timer flags.
//

#define KTIMER_FLAG_INTERNAL_QUEUED 0x80000000

//
// Define the mask of internal flags.
//

#define KTIMER_FLAG_INTERNAL_MASK (KTIMER_FLAG_INTERNAL_QUEUED)

//
// Define the threshold above which the microsecond to time tick calculation is
// done the low-precision way to avoid potential rollover. At 10 seconds, the
// time counter would have to run at 115 GHz.
//

#define TIME_COUNTER_MICROSECOND_CUTOFF (10 * MICROSECONDS_PER_SECOND)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _KTIMER_CRASH_REASON {
    KTimerCrashInvalid,
    KTimerCrashDoubleQueue,
    KTimerCrashUnqueuedTimerFoundInQueue,
    KTimerCrashCorrupt,
    KTimerCrashQueuingTimerFromTimerDpc,
} KTIMER_CRASH_REASON, *PKTIMER_CRASH_REASON;

/*++

Structure Description:

    This structure defines a kernel software timer.

Members:

    Header - Stores the object header.

    TreeNode - Stores the information about this timer's entry in the timer
        queue.

    DueTime - Stores the time counter expiration time, in ticks.

    Period - Stores the period of the timer if it is periodic, or 0 if it is a
        one-shot timer.

    QueueType - Stores the queue type of this timer.

    Dpc - Stores an optional pointer to a DPC to queue when this timer
        completes.

    Flags - Stores a bitfield of flags governing the operation and state of the
        timer. See KTIMER_FLAG_* definitions.

    Processor - Stores the processor number that the timer is queued on, if it
        is queued.

--*/

struct _KTIMER {
    OBJECT_HEADER Header;
    RED_BLACK_TREE_NODE TreeNode;
    ULONGLONG DueTime;
    ULONGLONG Period;
    TIMER_QUEUE_TYPE QueueType;
    PDPC Dpc;
    ULONG Flags;
    ULONG Processor;
};

/*++

Structure Description:

    This structure defines a kernel software timer queue.

Members:

    Tree - Stores the Red-Black tree structure that timers are stored in.

    NextTimer - Stores a pointer to the next timer that will expire, or NULL if
        the queue is empty.

    NextDueTime - Stores the due time of the next timer.

    QueuedTimerCount - Stores the number of times a timer has been added to
        this queue.

    ExpiredTimerCount - Stores the number of times a timer has come out of this
        queue because it expired.

    CancelledTimerCount - Stores the number of times a timer has come out of
        this queue because it was cancelled.

--*/

typedef struct _KTIMER_QUEUE {
    RED_BLACK_TREE Tree;
    PKTIMER NextTimer;
    ULONGLONG NextDueTime;
    UINTN QueuedTimerCount;
    UINTN ExpiredTimerCount;
    UINTN CancelledTimerCount;
} KTIMER_QUEUE, *PKTIMER_QUEUE;

/*++

Structure Description:

    This structure defines the context for per-processor kernel software timer
    management.

Members:

    Lock - Stores a spin lock protecting access to the queues.

    NextTimer - Stores the next timer to expire across all queues.

    NextDueTime - Stores the next due time across all timer queues.

    Queues - Stores the timer queues, except for the soft timer queue, which is
        global. Since the soft timer queue is not in this array, the array is
        indexed by the timer queue type minus one.

--*/

struct _KTIMER_DATA {
    KSPIN_LOCK Lock;
    PKTIMER NextTimer;
    ULONGLONG NextDueTime;
    PKTIMER NextWakingTimer;
    ULONGLONG NextWakeTime;
    KTIMER_QUEUE Queues[TimerQueueCount - 1];
};

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepInsertTimer (
    PPROCESSOR_BLOCK ProcessorBlock,
    PKTIMER_QUEUE Queue,
    PKTIMER Timer
    );

VOID
KepRemoveTimer (
    PPROCESSOR_BLOCK ProcessorBlock,
    PKTIMER_QUEUE Queue,
    PKTIMER Timer
    );

COMPARISON_RESULT
KepCompareTimerTreeNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global soft timer queue, which is serviced by whichever processor
// gets there first.
//

KTIMER_QUEUE KeSoftTimerQueue;
KSPIN_LOCK KeSoftTimerLock;
POBJECT_HEADER KeTimerDirectory;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PKTIMER
KeCreateTimer (
    ULONG AllocationTag
    )

/*++

Routine Description:

    This routine creates a new timer object. Once created, this timer needs to
    be initialized before it can be queued. This routine must be called at or
    below dispatch level.

Arguments:

    AllocationTag - Supplies a pointer to an identifier to use for the
        allocation that uniquely identifies the driver or module allocating the
        timer.

Return Value:

    Returns a pointer to the timer on success.

    NULL on resource allocation failure.

--*/

{

    PKTIMER Timer;

    Timer = ObCreateObject(ObjectTimer,
                           KeTimerDirectory,
                           NULL,
                           0,
                           sizeof(KTIMER),
                           NULL,
                           0,
                           AllocationTag);

    return Timer;
}

KERNEL_API
VOID
KeDestroyTimer (
    PKTIMER Timer
    )

/*++

Routine Description:

    This routine destroys a timer object. If the timer is currently queued, this
    routine cancels the timer and then destroys it. This routine must be called
    at or below dispatch level.

Arguments:

    Timer - Supplies a pointer to the timer to destroy.

Return Value:

    None.

--*/

{

    //
    // Check to see if the timer is queued. If it is, cancel it.
    //

    if ((Timer->Flags & KTIMER_FLAG_INTERNAL_QUEUED) != 0) {
        KeCancelTimer(Timer);
    }

    ObReleaseReference(Timer);
    return;
}

KERNEL_API
KSTATUS
KeQueueTimer (
    PKTIMER Timer,
    TIMER_QUEUE_TYPE QueueType,
    ULONGLONG DueTime,
    ULONGLONG Period,
    ULONG Flags,
    PDPC Dpc
    )

/*++

Routine Description:

    This routine configures and queues a timer object. The timer must not
    already be queued, otherwise the system will crash. This routine must be
    called at or below dispatch level.

Arguments:

    Timer - Supplies a pointer to the timer to configure and queue.

    QueueType - Supplies the queue the timer should reside on. Valid values are:

        TimerQueueSoft - The timer will be expired at the first applicable
            clock interrupt, but a clock interrupt will not be scheduled solely
            for this timer. This timer type has the best power management
            profile, but may cause the expiration of the timer to be fairly
            late, as the system will not come out of idle to service this timer.
            The DPC for this timer may run on any processor.

        TimerQueueSoftWake - The timer will be expired at the first applicable
            clock interrupt. If the system was otherwise idle, a clock
            interrupt will be scheduled for this timer. This is a balanced
            choice for timers that can have some slack in their expiration, but
            need to run approximately when scheduled, even if the system is
            idle. The DPC will run on the processor where the timer was queued.

        TimerQueueHard - A clock interrupt will be scheduled for exactly the
            specified deadline. This is the best choice for high performance
            timers that need to expire as close to their deadlines as possible.
            It is the most taxing on power management, as it pulls the system
            out of idle, schedules an extra clock interrupt, and requires
            programming hardware. The DPC will run on the processor where the
            timer was queued.

    DueTime - Supplies the value of the time tick counter when this timer
        should expire (an absolute value in time counter ticks). If this value
        is 0, then an automatic due time of the current time plus the given
        period will be computed.

    Period - Supplies an optional period, in time counter ticks, for periodic
        timers. If this value is non-zero, the period will be added to the
        original due time and the timer will be automatically rearmed.

    Flags - Supplies an optional bitfield of flags. See KTIMER_FLAG_*
        definitions.

    Dpc - Supplies an optional pointer to a DPC that will be queued when this
        timer expires.

Return Value:

    Status code.

--*/

{

    PKSPIN_LOCK Lock;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;
    PKTIMER_QUEUE Queue;
    PKTIMER_DATA TimerData;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    if (DueTime == 0) {
        DueTime = HlQueryTimeCounter() + Period;
    }

    if (QueueType >= TimerQueueCount) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Raise to dispatch and acquire the lock.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();

    //
    // Because the DPCs get run directly with the timer lock held, it is
    // illegal to queue a timer from its DPC. This doesn't catch all cases,
    // but its a start.
    //

    if ((Dpc != NULL) && (Dpc == ProcessorBlock->DpcInProgress)) {
        KeCrashSystem(CRASH_KTIMER_FAILURE,
                      KTimerCrashQueuingTimerFromTimerDpc,
                      (UINTN)Timer,
                      (UINTN)ProcessorBlock,
                      (UINTN)Dpc);
    }

    TimerData = ProcessorBlock->TimerData;
    if (QueueType == TimerQueueSoft) {
        Timer->Processor = -1;
        Lock = &KeSoftTimerLock;
        Queue = &KeSoftTimerQueue;

    } else {
        Timer->Processor = ProcessorBlock->ProcessorNumber;
        Lock = &(TimerData->Lock);
        Queue = &(TimerData->Queues[QueueType - 1]);
    }

    KeAcquireSpinLock(Lock);

    //
    // Crash the system if the timer is already queued.
    //

    if ((Timer->Flags & KTIMER_FLAG_INTERNAL_QUEUED) != 0) {
        KeCrashSystem(CRASH_KTIMER_FAILURE,
                      KTimerCrashDoubleQueue,
                      (UINTN)Timer,
                      0,
                      0);
    }

    ObSignalObject(Timer, SignalOptionUnsignal);
    Timer->QueueType = QueueType;
    Timer->DueTime = DueTime;
    Timer->Period = Period;
    Timer->Flags &= ~KTIMER_FLAG_PUBLIC_MASK;
    Timer->Flags |= Flags & KTIMER_FLAG_PUBLIC_MASK;
    Timer->Dpc = Dpc;
    KepInsertTimer(ProcessorBlock, Queue, Timer);

    //
    // Release the lock and return to the old run level.
    //

    KeReleaseSpinLock(Lock);
    KeLowerRunLevel(OldRunLevel);
    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
KeCancelTimer (
    PKTIMER Timer
    )

/*++

Routine Description:

    This routine attempts to cancel a queued timer. This routine must be called
    at or below dispatch level. This routine will ensure that the DPC
    associated with the timer will have either been fully queued or not queued
    by the time this function returns, even if the timer was too late to
    cancel.

Arguments:

    Timer - Supplies a pointer to the timer to cancel.

Return Value:

    STATUS_SUCCESS if the timer was successfully cancelled.

    STATUS_TOO_LATE if the timer expired before the timer queue could be
    accessed.

--*/

{

    PKSPIN_LOCK Lock;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG ProcessorCount;
    ULONG ProcessorNumber;
    PKTIMER_QUEUE Queue;
    KSTATUS Status;
    PKTIMER_DATA TimerData;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    ProcessorCount = KeGetActiveProcessorCount();
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    if (Timer->QueueType == TimerQueueSoft) {
        ProcessorBlock = KeGetCurrentProcessorBlock();
        ProcessorNumber = -1;
        Queue = &KeSoftTimerQueue;
        Lock = &KeSoftTimerLock;
        KeAcquireSpinLock(Lock);

    } else {

        //
        // Loop chasing the timer around the processor it's on.
        //

        while (TRUE) {
            ProcessorNumber = Timer->Processor;
            if (ProcessorNumber >= ProcessorCount) {
                KeCrashSystem(CRASH_KTIMER_FAILURE,
                              KTimerCrashCorrupt,
                              (UINTN)Timer,
                              0,
                              0);
            }

            ProcessorBlock = KeProcessorBlocks[Timer->Processor];
            TimerData = ProcessorBlock->TimerData;
            Lock = &(TimerData->Lock);
            KeAcquireSpinLock(Lock);
            if (Timer->Processor == ProcessorNumber) {
                break;
            }

            KeReleaseSpinLock(Lock);
        }

        Queue = &(TimerData->Queues[Timer->QueueType - 1]);
    }

    //
    // In all cases, unsignal the timer.
    //

    ObSignalObject(Timer, SignalOptionUnsignal);

    //
    // Check the flag, and fail if the timer is no longer queued. The fact that
    // the lock is held also means it's not in the process of queuing the DPC;
    // either the DPC is queued or it isn't going to be.
    //

    if ((Timer->Flags & KTIMER_FLAG_INTERNAL_QUEUED) == 0) {
        Status = STATUS_TOO_LATE;
        goto CancelTimerEnd;
    }

    ASSERT(Timer->Processor == ProcessorNumber);

    //
    // Remove the timer from the queue and return successfully.
    //

    KepRemoveTimer(ProcessorBlock, Queue, Timer);
    Queue->CancelledTimerCount += 1;
    Status = STATUS_SUCCESS;

CancelTimerEnd:
    KeReleaseSpinLock(Lock);
    KeLowerRunLevel(OldRunLevel);
    return Status;
}

KERNEL_API
VOID
KeSignalTimer (
    PKTIMER Timer,
    SIGNAL_OPTION Option
    )

/*++

Routine Description:

    This routine sets a timer to the given signal state.

Arguments:

    Timer - Supplies a pointer to the timer to signal or unsignal.

    Option - Supplies the signaling behavior to apply.

Return Value:

    None.

--*/

{

    ObSignalObject(Timer, Option);
    return;
}

KERNEL_API
SIGNAL_STATE
KeGetTimerState (
    PKTIMER Timer
    )

/*++

Routine Description:

    This routine returns the signal state of a timer.

Arguments:

    Timer - Supplies a pointer to the timer to get the state of.

Return Value:

    Returns the signal state of the timer.

--*/

{

    return Timer->Header.WaitQueue.State;
}

KERNEL_API
ULONGLONG
KeGetTimerDueTime (
    PKTIMER Timer
    )

/*++

Routine Description:

    This routine returns the next due time of the given timer. This could be in
    the past. This routine must be called at or below dispatch level.

Arguments:

    Timer - Supplies a pointer to the timer to query.

Return Value:

    Returns the due time of the timer.

    0 if the timer is not currently queued.

--*/

{

    ULONGLONG DueTime;
    PKSPIN_LOCK Lock;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG ProcessorCount;
    ULONG ProcessorNumber;
    PKTIMER_DATA TimerData;

    if ((Timer->Flags & KTIMER_FLAG_INTERNAL_QUEUED) == 0) {
        return 0;
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    if (Timer->QueueType == TimerQueueSoft) {
        Lock = &KeSoftTimerLock;
        KeAcquireSpinLock(Lock);

    } else {

        //
        // Loop chasing the timer around the processor it's on.
        //

        ProcessorCount = KeGetActiveProcessorCount();
        while (TRUE) {
            ProcessorNumber = Timer->Processor;
            if (ProcessorNumber >= ProcessorCount) {
                KeCrashSystem(CRASH_KTIMER_FAILURE,
                              KTimerCrashCorrupt,
                              (UINTN)Timer,
                              0,
                              0);
            }

            ProcessorBlock = KeProcessorBlocks[Timer->Processor];
            TimerData = ProcessorBlock->TimerData;
            Lock = &(TimerData->Lock);
            KeAcquireSpinLock(Lock);
            if (Timer->Processor == ProcessorNumber) {
                break;
            }

            KeReleaseSpinLock(Lock);
        }
    }

    //
    // Recheck the flag now that the queue is locked.
    //

    if ((Timer->Flags & KTIMER_FLAG_INTERNAL_QUEUED) == 0) {
        DueTime = 0;
        goto TimerGetRemainingTimerEnd;
    }

    DueTime = Timer->DueTime;

TimerGetRemainingTimerEnd:
    KeReleaseSpinLock(Lock);
    KeLowerRunLevel(OldRunLevel);
    return DueTime;
}

KERNEL_API
ULONGLONG
KeConvertMicrosecondsToTimeTicks (
    ULONGLONG Microseconds
    )

/*++

Routine Description:

    This routine converts the given number of microseconds into time counter
    ticks.

Arguments:

    Microseconds - Supplies the microsecond count.

Return Value:

    Returns the number of time ticks that correspond to the given number of
    microseconds.

--*/

{

    ULONGLONG CounterFrequency;
    ULONGLONG Result;

    CounterFrequency = HlQueryTimeCounterFrequency();

    //
    // If the value is above a certain threshold, do the division first to
    // avoid potential rollovers.
    //

    if (Microseconds > TIME_COUNTER_MICROSECOND_CUTOFF) {
        Result = (Microseconds / MICROSECONDS_PER_SECOND) * CounterFrequency;

    } else {
        Result = Microseconds * CounterFrequency / MICROSECONDS_PER_SECOND;
    }

    return Result;
}

PKTIMER_DATA
KepCreateTimerData (
    VOID
    )

/*++

Routine Description:

    This routine is called upon system initialization to create a timer
    management context for a new processor.

Arguments:

    None.

Return Value:

    Returns a pointer to the timer data on success.

    NULL on resource allocation failure.

--*/

{

    PKTIMER_DATA Data;
    PKTIMER_QUEUE Queue;
    UINTN QueueIndex;

    //
    // Cheat a little and initialize the global soft timer queue if this is
    // processor 0.
    //

    if (KeGetCurrentProcessorNumber() == 0) {
        RtlRedBlackTreeInitialize(&(KeSoftTimerQueue.Tree),
                                  0,
                                  KepCompareTimerTreeNodes);

        KeSoftTimerQueue.NextDueTime = -1ULL;
        KeInitializeSpinLock(&KeSoftTimerLock);
        KeTimerDirectory = ObCreateObject(ObjectDirectory,
                                          NULL,
                                          "Timer",
                                          sizeof("Timer"),
                                          sizeof(OBJECT_HEADER),
                                          NULL,
                                          OBJECT_FLAG_USE_NAME_DIRECTLY,
                                          KE_ALLOCATION_TAG);

        if (KeTimerDirectory == NULL) {
            return NULL;
        }
    }

    Data = MmAllocateNonPagedPool(sizeof(KTIMER_DATA), KE_ALLOCATION_TAG);
    if (Data == NULL) {
        return NULL;
    }

    RtlZeroMemory(Data, sizeof(KTIMER_DATA));
    KeInitializeSpinLock(&(Data->Lock));
    for (QueueIndex = TimerQueueSoftWake;
         QueueIndex < TimerQueueCount;
         QueueIndex += 1) {

        Queue = &(Data->Queues[QueueIndex - 1]);
        RtlRedBlackTreeInitialize(&(Queue->Tree), 0, KepCompareTimerTreeNodes);
        Queue->NextDueTime = -1ULL;
    }

    Data->NextDueTime = -1ULL;
    return Data;
}

VOID
KepDestroyTimerData (
    PKTIMER_DATA Data
    )

/*++

Routine Description:

    This routine tears down a processor's timer management context.

Arguments:

    Data - Supplies a pointer to the data to destroy.

Return Value:

    None.

--*/

{

    MmFreeNonPagedPool(Data);
    return;
}

VOID
KepDispatchTimers (
    ULONGLONG CurrentTime
    )

/*++

Routine Description:

    This routine is called at regular intervals to check for and expire timers
    whose time has come. This routine must only be called internally, and must
    be called at dispatch level.

Arguments:

    Queue - Supplies a pointer to the timer queue for the current processor.

    CurrentTime - Supplies the current time counter value. Any timers with this
        due time or earlier will be expired.

Return Value:

    None.

--*/

{

    ULONGLONG MissedCycles;
    PPROCESSOR_BLOCK ProcessorBlock;
    PKTIMER_QUEUE Queue;
    INTN QueueIndex;
    SIGNAL_OPTION SignalOption;
    PKTIMER Timer;
    PKTIMER_DATA TimerData;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);

    ProcessorBlock = KeGetCurrentProcessorBlock();
    TimerData = ProcessorBlock->TimerData;

    //
    // If no timers are expired, just return. The soft timer queue read could
    // tear on 32-bit systems, but it doesn't matter. Even if the torn read
    // causes the condition to incorrectly become true (and return without
    // expiring), the soft timers will be expired on the next go-round.
    //

    if ((CurrentTime < TimerData->NextDueTime) &&
        (CurrentTime < KeSoftTimerQueue.NextDueTime)) {

        return;
    }

    KeAcquireSpinLock(&(TimerData->Lock));

    //
    // Iterate backwards so that hard timers, who care most about their
    // deadlines, run first.
    //

    for (QueueIndex = TimerQueueCount - 1; QueueIndex >= 0; QueueIndex -= 1) {

        //
        // The soft queue is global. Make an effort to grab the lock, but only
        // try once. Failure means another processor is already servicing those
        // timers (great, no work required here), or another processor is in
        // there queuing or cancelling. If that's the case, the soft timer
        // queue missed its chance this round, better luck next time.
        //

        if (QueueIndex == TimerQueueSoft) {
            Queue = &KeSoftTimerQueue;
            if ((CurrentTime < KeSoftTimerQueue.NextDueTime) ||
                (KeTryToAcquireSpinLock(&KeSoftTimerLock) == FALSE)) {

                continue;
            }

        } else {
            Queue = &(TimerData->Queues[QueueIndex - 1]);
        }

        while (CurrentTime >= Queue->NextDueTime) {
            Timer = Queue->NextTimer;
            KepRemoveTimer(ProcessorBlock, Queue, Timer);
            Queue->ExpiredTimerCount += 1;

            //
            // If the timer is periodic, adjust the due time and reinsert.
            // Make sure to adjust the due time to a point in the future.
            //

            if (Timer->Period != 0) {

                //
                // In the common case, the timer won't have missed any cycles,
                // and so the period can simply be added, avoiding a divide.
                //

                if (Timer->DueTime + Timer->Period > CurrentTime) {
                    Timer->DueTime += Timer->Period;

                } else {
                    MissedCycles = (CurrentTime - Timer->DueTime) /
                                   Timer->Period;

                    Timer->DueTime += (MissedCycles + 1) * Timer->Period;
                }

                KepInsertTimer(ProcessorBlock, Queue, Timer);
                SignalOption = SignalOptionPulse;

            //
            // If the timer is one-shot, leave it removed, and signal
            // permanently.
            //

            } else {
                SignalOption = SignalOptionSignalAll;
            }

            //
            // Signal the timer, and if there's a DPC there, queue that up.
            //

            ObSignalObject(Timer, SignalOption);
            if (Timer->Dpc != NULL) {
                KeQueueDpc(Timer->Dpc);
            }
        }

        //
        // Release the global lock acquired if this is the soft queue.
        //

        if (QueueIndex == TimerQueueSoft) {
            KeReleaseSpinLock(&KeSoftTimerLock);
        }
    }

    KeReleaseSpinLock(&(TimerData->Lock));
    return;
}

ULONGLONG
KepGetNextTimerDeadline (
    PPROCESSOR_BLOCK Processor,
    PBOOL Hard
    )

/*++

Routine Description:

    This routine returns the next waking deadline of timers on the given
    processor. This routine must be called at or above dispatch level.

Arguments:

    Processor - Supplies a pointer to the processor.

    Hard - Supplies a pointer where a boolean will be returned indicating if
        this is a hard deadline or a soft deadline.

Return Value:

    Returns the next waking timer deadline.

    -1 if there are no waking timers.

--*/

{

    ULONGLONG Deadline;
    ULONGLONG HardDeadline;
    ULONGLONG SoftDeadline;
    PKTIMER_DATA TimerData;

    TimerData = Processor->TimerData;
    SoftDeadline = TimerData->Queues[TimerQueueSoftWake - 1].NextDueTime;
    HardDeadline = TimerData->Queues[TimerQueueHard - 1].NextDueTime;
    if (SoftDeadline == -1ULL) {
        Deadline = HardDeadline;
        *Hard = TRUE;

    //
    // The soft-wake time needs to be far enough before the hard deadline such
    // that even if the soft-wake time slips a whole clock cycle, as it might,
    // the hard deadline won't be missed. If there's a chance the hard deadline
    // might be missed, just return the hard deadline.
    //

    } else if (SoftDeadline + KeClockRate <= HardDeadline) {
        *Hard = FALSE;
        Deadline = SoftDeadline;

    } else {
        *Hard = TRUE;
        Deadline = HardDeadline;
    }

    if ((Deadline == -1ULL) || (KeDisableDynamicTick != FALSE)) {
        *Hard = FALSE;
    }

    return Deadline;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepInsertTimer (
    PPROCESSOR_BLOCK ProcessorBlock,
    PKTIMER_QUEUE Queue,
    PKTIMER Timer
    )

/*++

Routine Description:

    This routine inserts a timer from into a timer queue. This routine assumes
    the timer data lock is already held.

Arguments:

    ProcessorBlock - Supplies a pointer to the processor block to add the
        timer to.

    Queue - Supplies a pointer to the timer queue.

    Timer - Supplies a pointer to the timer.

Return Value:

    None.

--*/

{

    PKTIMER_DATA TimerData;

    TimerData = ProcessorBlock->TimerData;

    //
    // Crash the system if the timer is already queued.
    //

    if ((Timer->Flags & KTIMER_FLAG_INTERNAL_QUEUED) != 0) {
        KeCrashSystem(CRASH_KTIMER_FAILURE,
                      KTimerCrashDoubleQueue,
                      (UINTN)Timer,
                      0,
                      0);
    }

    Timer->Flags |= KTIMER_FLAG_INTERNAL_QUEUED;
    if (Timer->QueueType == TimerQueueHard) {
        if (KeDisableDynamicTick == FALSE) {
            ProcessorBlock->Clock.AnyHard = TRUE;
        }
    }

    //
    // Add the timer to the tree.
    //

    Queue->QueuedTimerCount += 1;
    RtlRedBlackTreeInsert(&(Queue->Tree), &(Timer->TreeNode));

    //
    // Maintain the next pointer of the queue for quick queries.
    //

    if ((Queue->NextTimer == NULL) ||
        (Timer->DueTime < Queue->NextTimer->DueTime)) {

        Queue->NextTimer = Timer;
        Queue->NextDueTime = Timer->DueTime;
        if (Timer->QueueType != TimerQueueSoft) {

            //
            // Maintain the next timer globally.
            //

            if ((TimerData->NextTimer == NULL) ||
                (Timer->DueTime < TimerData->NextDueTime)) {

                TimerData->NextTimer = Timer;
                TimerData->NextDueTime = Timer->DueTime;
            }

            //
            // Tell the clock scheduler about all new winning hard and soft
            // wake timers. New soft wake timers need to poke the clock because
            // the clock might be off right now.
            //

            KepUpdateClockDeadline();
        }
    }

    return;
}

VOID
KepRemoveTimer (
    PPROCESSOR_BLOCK ProcessorBlock,
    PKTIMER_QUEUE Queue,
    PKTIMER Timer
    )

/*++

Routine Description:

    This routine removes a timer from a timer queue. This routine assumes the
    timer data lock is already held.

Arguments:

    ProcessorBlock - Supplies a pointer to the processor block the timer is on.

    Queue - Supplies a pointer to the timer queue.

    Timer - Supplies a pointer to the timer.

Return Value:

    None.

--*/

{

    PRED_BLACK_TREE_NODE NextNode;
    PKTIMER NextTimer;
    PKTIMER_DATA TimerData;

    TimerData = ProcessorBlock->TimerData;
    if ((Timer->Flags & KTIMER_FLAG_INTERNAL_QUEUED) == 0) {
        KeCrashSystem(CRASH_KTIMER_FAILURE,
                      KTimerCrashUnqueuedTimerFoundInQueue,
                      (UINTN)Timer,
                      (UINTN)TimerData,
                      0);
    }

    RtlRedBlackTreeRemove(&(Queue->Tree), &(Timer->TreeNode));
    Timer->Flags &= ~KTIMER_FLAG_INTERNAL_QUEUED;

    //
    // Maintain the next timer for the queue.
    //

    if (Timer == Queue->NextTimer) {
        NextNode = RtlRedBlackTreeGetNextNode(&(Queue->Tree),
                                              FALSE,
                                              &(Timer->TreeNode));

        if (NextNode != NULL) {
            NextTimer = RED_BLACK_TREE_VALUE(NextNode, KTIMER, TreeNode);
            Queue->NextDueTime = NextTimer->DueTime;

            //
            // Tell the clock scheduler about the next hard or soft-wake timer.
            // The soft-wake timer case is necessary if the clock is now off.
            //

            if (Timer->QueueType != TimerQueueSoft) {
                KepUpdateClockDeadline();
            }

        } else {
            NextTimer = NULL;
            Queue->NextDueTime = -1ULL;
            if (Timer->QueueType == TimerQueueHard) {
                ProcessorBlock->Clock.AnyHard = FALSE;
            }
        }

        Queue->NextTimer = NextTimer;

        //
        // If this was also the winner globally, find the next winner.
        //

        if (Timer == TimerData->NextTimer) {

            //
            // Soft timers are global and should never be listed as a specific
            // processor's next deadline.
            //

            ASSERT(Timer->QueueType != TimerQueueSoft);

            //
            // Figure out the next global timer.
            //

            Queue = &(TimerData->Queues[TimerQueueSoftWake - 1]);
            TimerData->NextTimer = Queue->NextTimer;
            TimerData->NextDueTime = Queue->NextDueTime;
            Queue = &(TimerData->Queues[TimerQueueHard - 1]);
            if ((TimerData->NextTimer == NULL) ||
                (Queue->NextDueTime < TimerData->NextDueTime)) {

                TimerData->NextTimer = Queue->NextTimer;
                TimerData->NextDueTime = Queue->NextDueTime;
            }
        }

    } else {

        //
        // A timer cannot be the winner globally but not the winner of its own
        // queue.
        //

        ASSERT((Timer->QueueType == TimerQueueSoft) ||
               (Timer != TimerData->NextTimer));
    }

    return;
}

COMPARISON_RESULT
KepCompareTimerTreeNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two kernel timer Red-Black tree nodes.

Arguments:

    Tree - Supplies a pointer to the tree being traversed.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PKTIMER FirstTimer;
    PKTIMER SecondTimer;

    FirstTimer = RED_BLACK_TREE_VALUE(FirstNode, KTIMER, TreeNode);
    SecondTimer = RED_BLACK_TREE_VALUE(SecondNode, KTIMER, TreeNode);
    if (FirstTimer->DueTime < SecondTimer->DueTime) {
        return ComparisonResultAscending;

    } else if (FirstTimer->DueTime > SecondTimer->DueTime) {
        return ComparisonResultDescending;
    }

    ASSERT(FirstTimer->DueTime == SecondTimer->DueTime);

    return ComparisonResultSame;
}

