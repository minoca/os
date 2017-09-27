/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sched.c

Abstract:

    This module implements the kernel thread scheduler.

Author:

    Evan Green 7-Apr-2015

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
// Define the minimum number of ready threads on a scheduler before another
// scheduler will consider stealing from it.
//

#define SCHEDULER_REBALANCE_MINIMUM_THREADS 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepIdle (
    PPROCESSOR_BLOCK Processor
    );

VOID
KepBalanceIdleScheduler (
    VOID
    );

BOOL
KepEnqueueSchedulerEntry (
    PSCHEDULER_ENTRY Entry,
    BOOL LockHeld
    );

VOID
KepDequeueSchedulerEntry (
    PSCHEDULER_ENTRY Entry,
    BOOL LockHeld
    );

PKTHREAD
KepGetNextThread (
    PSCHEDULER_DATA Scheduler,
    BOOL SkipRunning
    );

KSTATUS
KepCreateSchedulerGroup (
    PSCHEDULER_GROUP *NewGroup
    );

VOID
KepDestroySchedulerGroup (
    PSCHEDULER_GROUP Group
    );

VOID
KepInitializeSchedulerGroupEntry (
    PSCHEDULER_GROUP_ENTRY GroupEntry,
    PSCHEDULER_DATA Scheduler,
    PSCHEDULER_GROUP Group,
    PSCHEDULER_GROUP_ENTRY ParentEntry
    );

//
// -------------------------------------------------------------------- Globals
//

SCHEDULER_GROUP KeRootSchedulerGroup;
KSPIN_LOCK KeSchedulerGroupLock;

//
// Set this to TRUE to move a thread onto the current processor when unblocking
// that thread.
//

BOOL KeSchedulerStealReadyThreads = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
KeYield (
    VOID
    )

/*++

Routine Description:

    This routine yields the current thread's execution. The thread remains in
    the ready state, and may not actually be scheduled out if no other threads
    are ready.

Arguments:

    None.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeSchedulerEntry(SchedulerReasonThreadYielding);
    KeLowerRunLevel(OldRunLevel);
    return;
}

VOID
KeSchedulerEntry (
    SCHEDULER_REASON Reason
    )

/*++

Routine Description:

    This routine serves as the entry point to the thread scheduler. It may
    decide to schedule a new thread or simply return.

Arguments:

    Reason - Supplies the scheduler with the reason why it's being called (ie
        run-level lowering, the thread is waiting, exiting, etc).

Return Value:

    None.

--*/

{

    BOOL Enabled;
    BOOL FirstTime;
    PKTHREAD NextThread;
    PVOID NextThreadStack;
    THREAD_STATE NextThreadState;
    PKTHREAD OldThread;
    PPROCESSOR_BLOCK Processor;
    PVOID *SaveLocation;

    Enabled = FALSE;
    FirstTime = FALSE;
    Processor = KeGetCurrentProcessorBlock();

    //
    // The scheduler must be called at dispatch.
    //

    ASSERT(KeGetRunLevel() == RunLevelDispatch);
    ASSERT(ArAreInterruptsEnabled() != FALSE);

    //
    // It is illegal for a DPC routine to block.
    //

    if (Processor->DpcInProgress != NULL) {
        KeCrashSystem(CRASH_DPC_FAILURE,
                      DpcCrashDpcBlocked,
                      (UINTN)(Processor->DpcInProgress),
                      0,
                      0);
    }

    OldThread = Processor->RunningThread;
    KeAcquireSpinLock(&(Processor->Scheduler.Lock));

    //
    // Remove the old thread from the scheduler. Immediately put it back if
    // it's not blocking.
    //

    if (OldThread != Processor->IdleThread) {
        KepDequeueSchedulerEntry(&(OldThread->SchedulerEntry), TRUE);
        if ((Reason != SchedulerReasonThreadBlocking) &&
            (Reason != SchedulerReasonThreadSuspending) &&
            (Reason != SchedulerReasonThreadExiting)) {

            KepEnqueueSchedulerEntry(&(OldThread->SchedulerEntry), TRUE);
        }
    }

    //
    // Now that the old thread has accounted for its time, get the next thread
    // to run. This might be the old thread again.
    //

    NextThread = KepGetNextThread(&(Processor->Scheduler), FALSE);

    //
    // If there are no threads to run, run the idle thread.
    //

    if (NextThread == NULL) {
        NextThread = Processor->IdleThread;

        //
        // This had better not be the idle thread blocking.
        //

        ASSERT((OldThread != Processor->IdleThread) ||
               ((Reason != SchedulerReasonThreadBlocking) &&
                (Reason != SchedulerReasonThreadSuspending) &&
                (Reason != SchedulerReasonThreadExiting)));
    }

    //
    // Set the thread to running before releasing the scheduler lock to prevent
    // others from trying to steal this thread.
    //

    NextThreadState = NextThread->State;
    NextThread->State = ThreadStateRunning;
    KeReleaseSpinLock(&(Processor->Scheduler.Lock));

    //
    // Just return if there's no change.
    //

    if (OldThread == NextThread) {
        goto SchedulerEntryEnd;
    }

    //
    // Keep track of the old thread's behavior record.
    //

    if (Reason == SchedulerReasonDispatchInterrupt) {
        OldThread->ResourceUsage.Preemptions += 1;

    } else {
        OldThread->ResourceUsage.Yields += 1;
    }

    //
    // Profile this context switch if enabled.
    //

    SpCollectThreadStatistic(OldThread, Processor, Reason);

    ASSERT((NextThreadState == ThreadStateReady) ||
           (NextThreadState == ThreadStateFirstTime));

    //
    // Before setting the running thread to the new thread, charge the previous
    // time to the previous thread. If the next thread is a new user mode
    // thread, then start charging to usermode directly as the context swap
    // is going to jump there directly.
    //

    if (NextThreadState == ThreadStateFirstTime) {
        FirstTime = TRUE;
        if ((NextThread->Flags & THREAD_FLAG_USER_MODE) != 0) {
            KeBeginCycleAccounting(CycleAccountUser);

        } else {
            KeBeginCycleAccounting(CycleAccountKernel);
        }

    } else {
        KeBeginCycleAccounting(CycleAccountKernel);
    }

    KepArchPrepareForContextSwap(Processor, OldThread, NextThread);

    //
    // Disable interrupts and begin the transition to the new thread.
    //

    Enabled = ArDisableInterrupts();
    Processor->RunningThread = NextThread;
    Processor->PreviousThread = OldThread;

    //
    // Deal with reasons other than being preempted for scheduling the old
    // thread out.
    //

    switch (Reason) {

    //
    // If the scheduler wasn't invoked to block the thread, then it remains
    // ready to run. It must be set to ready only after it's been swapped out.
    //

    case SchedulerReasonDispatchInterrupt:
    case SchedulerReasonThreadYielding:
        break;

    //
    // The thread is waiting on an object. Let it be known that this thread is
    // on its way out (but isn't quite yet).
    //

    case SchedulerReasonThreadBlocking:

        ASSERT(OldThread != Processor->IdleThread);

        OldThread->State = ThreadStateBlocking;
        break;

    //
    // The thread is suspending, begin to take it down.
    //

    case SchedulerReasonThreadSuspending:

        ASSERT(OldThread != Processor->IdleThread);

        OldThread->State = ThreadStateSuspending;
        break;

    //
    // The thread is exiting. Set the state to exited and set this thread as
    // the previous thread to know to do something with it.
    //

    case SchedulerReasonThreadExiting:
        OldThread->State = ThreadStateExited;
        break;

    //
    // Unknown case!
    //

    default:

        ASSERT(FALSE);

        break;
    }

    //
    // Prepare threads running for the first time.
    //

    if (FirstTime != FALSE) {

        //
        // The thread should start at low level, and is jumped to immediately
        // by the context swap assembly. Interrupts will be enabled on the new
        // thread because of the return from exception mechanism.
        //

        KeLowerRunLevel(RunLevelLow);
    }

    SaveLocation = &(OldThread->KernelStackPointer);

    //
    // The thread is running, it shouldn't have a saved stack.
    //

    ASSERT(*SaveLocation == NULL);

    NextThreadStack = NextThread->KernelStackPointer;

    ASSERT(NextThreadStack >= KERNEL_VA_START);

    NextThread->KernelStackPointer = NULL;
    KepContextSwap(SaveLocation,
                   NextThreadStack,
                   NextThread->ThreadPointer,
                   FirstTime);

    //
    // If this thread is being run again and had launched a new thread the last
    // time it was scheduled out, it will come back with interrupts disabled.
    // Re-enable them here.
    //

    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

SchedulerEntryEnd:

    ASSERT(KeGetRunLevel() == RunLevelDispatch);
    ASSERT(ArAreInterruptsEnabled() != FALSE);

    return;
}

VOID
KeSetThreadReady (
    PKTHREAD Thread
    )

/*++

Routine Description:

    This routine unblocks a previously blocked thread and adds it to the
    ready queue.

Arguments:

    Thread - Supplies a pointer to the blocked thread.

Return Value:

    None.

--*/

{

    BOOL FirstThread;
    PSCHEDULER_GROUP Group;
    PSCHEDULER_GROUP_ENTRY GroupEntry;
    PSCHEDULER_GROUP_ENTRY NewGroupEntry;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;

    ASSERT((Thread->State == ThreadStateWaking) ||
           (Thread->State == ThreadStateFirstTime));

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    GroupEntry = PARENT_STRUCTURE(Thread->SchedulerEntry.Parent,
                                  SCHEDULER_GROUP_ENTRY,
                                  Entry);

    if (Thread->State == ThreadStateFirstTime) {
        RtlAtomicAdd(&(GroupEntry->Group->ThreadCount), 1);

    } else {
        Thread->State = ThreadStateReady;
    }

    //
    // If the configuration option is set, steal the thread to run on the
    // current processor. This is bad for cache locality, but doesn't need an
    // IPI.
    //

    if (KeSchedulerStealReadyThreads != FALSE) {
        ProcessorBlock = KeGetCurrentProcessorBlock();
        Group = GroupEntry->Group;
        if (Group == &KeRootSchedulerGroup) {
            NewGroupEntry = &(ProcessorBlock->Scheduler.Group);

        } else {

            ASSERT(Group->EntryCount > ProcessorBlock->ProcessorNumber);

            NewGroupEntry = &(Group->Entries[ProcessorBlock->ProcessorNumber]);
        }

        Thread->SchedulerEntry.Parent = &(NewGroupEntry->Entry);
        KepEnqueueSchedulerEntry(&(Thread->SchedulerEntry), FALSE);

    //
    // Enqueue the thread on the processor it was previously on. This may
    // require waking that processor up.
    //

    } else {
        FirstThread = KepEnqueueSchedulerEntry(&(Thread->SchedulerEntry),
                                               FALSE);

        //
        // If this is the first thread being scheduled on the processor, then
        // make sure the clock is running (or wake it up).
        //

        if (FirstThread != FALSE) {
            ProcessorBlock = PARENT_STRUCTURE(GroupEntry->Scheduler,
                                              PROCESSOR_BLOCK,
                                              Scheduler);

            KepSetClockToPeriodic(ProcessorBlock);
        }
    }

    KeLowerRunLevel(OldRunLevel);
    return;
}

VOID
KeSuspendExecution (
    VOID
    )

/*++

Routine Description:

    This routine suspends execution of the current thread until such time as
    another thread wakes it (usually because of a user mode signal).

Arguments:

    None.

Return Value:

    None. The function returns when another thread has woken this thread.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeSchedulerEntry(SchedulerReasonThreadSuspending);
    KeLowerRunLevel(OldRunLevel);
    return;
}

VOID
KeUnlinkSchedulerEntry (
    PSCHEDULER_ENTRY Entry
    )

/*++

Routine Description:

    This routine unlinks a scheduler entry from its parent group.

Arguments:

    Entry - Supplies a pointer to the entry to be unlinked.

Return Value:

    None.

--*/

{

    PSCHEDULER_GROUP Group;
    PSCHEDULER_GROUP_ENTRY GroupEntry;
    UINTN Index;
    UINTN OldCount;
    PSCHEDULER_GROUP_ENTRY ParentGroupEntry;

    while (Entry->Parent != NULL) {
        ParentGroupEntry = PARENT_STRUCTURE(Entry->Parent,
                                            SCHEDULER_GROUP_ENTRY,
                                            Entry);

        if (Entry->Type == SchedulerEntryThread) {

            ASSERT(Entry->ListEntry.Next == NULL);

            OldCount = RtlAtomicAdd(&(ParentGroupEntry->Group->ThreadCount),
                                    -1);

            ASSERT((OldCount != 0) && (OldCount < 0x10000000));

        } else {
            GroupEntry = PARENT_STRUCTURE(Entry, SCHEDULER_GROUP_ENTRY, Entry);

            ASSERT(GroupEntry->Entry.Type == SchedulerEntryGroup);

            //
            // If the group entry became completely empty, check the other
            // entries too.
            //

            if ((GroupEntry->Group->ThreadCount == 0) &&
                (LIST_EMPTY(&(GroupEntry->Children)) != FALSE)) {

                Group = GroupEntry->Group;
                for (Index = 0; Index < Group->EntryCount; Group += 1) {
                    GroupEntry = &(Group->Entries[Index]);
                    if (LIST_EMPTY(&(GroupEntry->Children)) == FALSE) {
                        break;
                    }
                }

                //
                // If all the group's thread counts and children are zero,
                // destroy the group.
                //

                if (Index == Group->EntryCount) {
                    KepDestroySchedulerGroup(Group);
                }
            }
        }

        Entry = &(ParentGroupEntry->Entry);
    }

    return;
}

VOID
KeIdleLoop (
    VOID
    )

/*++

Routine Description:

    This routine executes the idle loop. It does not return. It can be
    executed only from the idle thread.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BOOL Enabled;
    PPROCESSOR_BLOCK ProcessorBlock;

    ProcessorBlock = KeGetCurrentProcessorBlock();
    while (TRUE) {
        KeYield();
        if (ProcessorBlock->Scheduler.Group.ReadyThreadCount != 0) {
            continue;
        }

        KepBalanceIdleScheduler();

        //
        // Disable interrupts to commit to going down for idle. Without this
        // IPIs could come in and schedule new work after the ready thread
        // check but before halting.
        //

        Enabled = ArDisableInterrupts();

        ASSERT(Enabled != FALSE);

        //
        // After disabling interrupts, check to see if any threads snuck on
        // in the meantime, and abort the idle if so.
        //

        if (ProcessorBlock->Scheduler.Group.ReadyThreadCount != 0) {
            ArEnableInterrupts();
            continue;
        }

        KepIdle(ProcessorBlock);
    }

    return;
}

VOID
KepInitializeScheduler (
    PPROCESSOR_BLOCK ProcessorBlock
    )

/*++

Routine Description:

    This routine initializes the scheduler for a processor.

Arguments:

    ProcessorBlock - Supplies a pointer to the processor block.

Return Value:

    None.

--*/

{

    KeInitializeSpinLock(&KeSchedulerGroupLock);
    INITIALIZE_LIST_HEAD(&(KeRootSchedulerGroup.Children));
    KeInitializeSpinLock(&(ProcessorBlock->Scheduler.Lock));
    KepInitializeSchedulerGroupEntry(&(ProcessorBlock->Scheduler.Group),
                                     &(ProcessorBlock->Scheduler),
                                     &KeRootSchedulerGroup,
                                     NULL);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepIdle (
    PPROCESSOR_BLOCK Processor
    )

/*++

Routine Description:

    This routine is called when a processor has nothing to run. This routine is
    called with interrupts disabled, and returns with interrupts enabled.

Arguments:

    Processor - Supplies a pointer to the processor block of the current
        processor.

Return Value:

    None.

--*/

{

    KepClockIdle(Processor);

    //
    // Begin counting this time as idle time. There's no need to put it back
    // to its previous setting at the end because the next thing this thread
    // will do is yield, and the scheduler will set the new period.
    //

    KeBeginCycleAccounting(CycleAccountIdle);
    PmIdle(Processor);
    return;
}

VOID
KepBalanceIdleScheduler (
    VOID
    )

/*++

Routine Description:

    This routine is called when the processor is idle. It tries to steal
    threads from a busier processor.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG ActiveCount;
    ULONG CurrentNumber;
    PSCHEDULER_GROUP_ENTRY DestinationGroupEntry;
    BOOL FirstThread;
    PSCHEDULER_GROUP Group;
    ULONG Number;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;
    PSCHEDULER_GROUP_ENTRY SourceGroupEntry;
    PSCHEDULER_DATA VictimScheduler;
    PKTHREAD VictimThread;

    ActiveCount = KeGetActiveProcessorCount();
    if (ActiveCount == 1) {
        return;
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);

    ASSERT(OldRunLevel == RunLevelLow);

    CurrentNumber = KeGetCurrentProcessorNumber();
    VictimThread = NULL;

    //
    // Try to steal from another processor, starting with the next neighbor.
    //

    Number = CurrentNumber + 1;
    while (TRUE) {
        if (Number == ActiveCount) {
            Number = 0;
        }

        if (Number == CurrentNumber) {
            break;
        }

        ProcessorBlock = KeProcessorBlocks[Number];
        VictimScheduler = &(ProcessorBlock->Scheduler);
        if (VictimScheduler->Group.ReadyThreadCount >=
            SCHEDULER_REBALANCE_MINIMUM_THREADS) {

            KeAcquireSpinLock(&(VictimScheduler->Lock));
            VictimThread = KepGetNextThread(VictimScheduler, TRUE);
            if (VictimThread != NULL) {

                ASSERT((VictimThread->State == ThreadStateReady) ||
                       (VictimThread->State == ThreadStateFirstTime));

                //
                // Pull the thread out of the ready queue.
                //

                KepDequeueSchedulerEntry(&(VictimThread->SchedulerEntry), TRUE);
            }

            KeReleaseSpinLock(&(VictimScheduler->Lock));
            if (VictimThread != NULL) {

                //
                // Move the entry to this processor's queue.
                //

                SourceGroupEntry = PARENT_STRUCTURE(
                                           VictimThread->SchedulerEntry.Parent,
                                           SCHEDULER_GROUP_ENTRY,
                                           Entry);

                Group = SourceGroupEntry->Group;
                if (Group == &KeRootSchedulerGroup) {
                    DestinationGroupEntry =
                          &(KeProcessorBlocks[CurrentNumber]->Scheduler.Group);

                } else {

                    ASSERT(Group->EntryCount > CurrentNumber);

                    DestinationGroupEntry = &(Group->Entries[CurrentNumber]);
                }

                VictimThread->SchedulerEntry.Parent =
                                               &(DestinationGroupEntry->Entry);

                //
                // Enqueue the thread on this processor.
                //

                FirstThread =
                      KepEnqueueSchedulerEntry(&(VictimThread->SchedulerEntry),
                                               FALSE);

                if (FirstThread != FALSE) {
                    KepSetClockToPeriodic(KeProcessorBlocks[CurrentNumber]);
                }

                break;
            }
        }

        Number += 1;
    }

    KeLowerRunLevel(OldRunLevel);
    return;
}

BOOL
KepEnqueueSchedulerEntry (
    PSCHEDULER_ENTRY Entry,
    BOOL LockHeld
    )

/*++

Routine Description:

    This routine adds the given entry to the active scheduler. This routine
    assumes the current runlevel is at dispatch, or interrupts are disabled.

Arguments:

    Entry - Supplies a pointer to the entry to add.

    LockHeld - Supplies a boolean indicating whether or not the caller has the
        scheduler lock already held.

Return Value:

    TRUE if this was the first thread scheduled on the top level group. This
    may indicate to callers that the processor may be out and idle.

    FALSE if this was not the first thread scheduled, or the entry being
    scheduled was not a thread.

--*/

{

    BOOL FirstThread;
    PSCHEDULER_GROUP_ENTRY GroupEntry;
    PSCHEDULER_DATA Scheduler;

    ASSERT((KeGetRunLevel() == RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    FirstThread = FALSE;
    if (LockHeld != FALSE) {
        GroupEntry = PARENT_STRUCTURE(Entry->Parent,
                                      SCHEDULER_GROUP_ENTRY,
                                      Entry);

        Scheduler = GroupEntry->Scheduler;

    } else {

        //
        // Chase the entity around as it bounces from group entry to group
        // entry.
        //

        while (TRUE) {
            GroupEntry = PARENT_STRUCTURE(Entry->Parent,
                                          SCHEDULER_GROUP_ENTRY,
                                          Entry);

            Scheduler = GroupEntry->Scheduler;
            KeAcquireSpinLock(&(Scheduler->Lock));
            if (Entry->Parent == &(GroupEntry->Entry)) {
                break;
            }

            KeReleaseSpinLock(&(Scheduler->Lock));
        }
    }

    //
    // Add the entry to the list.
    //

    ASSERT(Entry->ListEntry.Next == NULL);

    INSERT_BEFORE(&(Entry->ListEntry), &(GroupEntry->Children));

    //
    // Propagate the ready thread up through all levels.
    //

    if (Entry->Type == SchedulerEntryThread) {
        while (TRUE) {
            GroupEntry->ReadyThreadCount += 1;
            if (GroupEntry->Entry.Parent == NULL) {

                //
                // Remember if this is the first thread to become ready on the
                // top level group.
                //

                if (GroupEntry->ReadyThreadCount == 1) {
                    FirstThread = TRUE;
                }

                break;
            }

            GroupEntry = PARENT_STRUCTURE(GroupEntry->Entry.Parent,
                                          SCHEDULER_GROUP_ENTRY,
                                          Entry);
        }
    }

    if (LockHeld == FALSE) {
        KeReleaseSpinLock(&(Scheduler->Lock));
    }

    return FirstThread;
}

VOID
KepDequeueSchedulerEntry (
    PSCHEDULER_ENTRY Entry,
    BOOL LockHeld
    )

/*++

Routine Description:

    This routine removes the given entry to the active scheduler. This routine
    assumes the current runlevel is at dispatch, or interrupts are disabled.

Arguments:

    Entry - Supplies a pointer to the entry to remove.

    LockHeld - Supplies a boolean indicating whether or not the caller has the
        scheduler lock already held.

Return Value:

    None.

--*/

{

    PSCHEDULER_GROUP_ENTRY GroupEntry;
    PSCHEDULER_GROUP_ENTRY ParentGroupEntry;
    PSCHEDULER_DATA Scheduler;

    ASSERT((KeGetRunLevel() == RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    if (LockHeld != FALSE) {
        GroupEntry = PARENT_STRUCTURE(Entry->Parent,
                                      SCHEDULER_GROUP_ENTRY,
                                      Entry);

        Scheduler = GroupEntry->Scheduler;

    } else {

        //
        // Chase the entity around as it bounces from group entry to group
        // entry.
        //

        while (TRUE) {
            GroupEntry = PARENT_STRUCTURE(Entry->Parent,
                                          SCHEDULER_GROUP_ENTRY,
                                          Entry);

            Scheduler = GroupEntry->Scheduler;
            KeAcquireSpinLock(&(Scheduler->Lock));
            if (Entry->Parent == &(GroupEntry->Entry)) {
                break;
            }

            KeReleaseSpinLock(&(Scheduler->Lock));
        }
    }

    //
    // Remove the entry from the list.
    //

    ASSERT(Entry->ListEntry.Next != NULL);

    LIST_REMOVE(&(Entry->ListEntry));
    Entry->ListEntry.Next = NULL;

    //
    // Propagate the no-longer-ready thread up through all levels.
    //

    if (Entry->Type == SchedulerEntryThread) {
        while (TRUE) {
            GroupEntry->ReadyThreadCount -= 1;
            if (GroupEntry->Entry.Parent == NULL) {
                break;
            }

            ParentGroupEntry = PARENT_STRUCTURE(GroupEntry->Entry.Parent,
                                                SCHEDULER_GROUP_ENTRY,
                                                Entry);

            //
            // Rotate the groups so others at higher levels get a chance to run.
            //

            LIST_REMOVE(&(GroupEntry->Entry.ListEntry));
            INSERT_BEFORE(&(GroupEntry->Entry.ListEntry),
                          &(ParentGroupEntry->Children));

            GroupEntry = ParentGroupEntry;
        }

    } else {
        GroupEntry = PARENT_STRUCTURE(Entry, SCHEDULER_GROUP_ENTRY, Entry);

        ASSERT(GroupEntry->ReadyThreadCount == 0);
    }

    if (LockHeld == FALSE) {
        KeReleaseSpinLock(&(Scheduler->Lock));
    }

    return;
}

PKTHREAD
KepGetNextThread (
    PSCHEDULER_DATA Scheduler,
    BOOL SkipRunning
    )

/*++

Routine Description:

    This routine returns the next thread to run in the scheduler. This routine
    assumes the scheduler lock is already held.

Arguments:

    Scheduler - Supplies a pointer to the scheduler to work on.

    SkipRunning - Supplies a boolean indicating whether to ignore the first
        thread on the queue if it's marked as running. This is used when trying
        to steal threads from another scheduler.

Return Value:

    Returns a pointer to the next thread to run.

    NULL if no threads are ready to run.

--*/

{

    PSCHEDULER_GROUP_ENTRY ChildGroupEntry;
    PLIST_ENTRY CurrentEntry;
    PSCHEDULER_ENTRY Entry;
    PSCHEDULER_GROUP_ENTRY GroupEntry;
    PKTHREAD Thread;

    GroupEntry = &(Scheduler->Group);
    if (GroupEntry->ReadyThreadCount == 0) {
        return NULL;
    }

    CurrentEntry = GroupEntry->Children.Next;
    while (CurrentEntry != &(GroupEntry->Children)) {

        //
        // Get the next child of the group. If it's a thread, return it.
        //

        Entry = LIST_VALUE(CurrentEntry, SCHEDULER_ENTRY, ListEntry);
        if (Entry->Type == SchedulerEntryThread) {
            Thread = PARENT_STRUCTURE(Entry, KTHREAD, SchedulerEntry);
            if ((SkipRunning == FALSE) ||
                (Thread->State != ThreadStateRunning)) {

                return Thread;
            }

            //
            // This thread was not acceptable. Try to continue to the next
            // entry in the list, or pop back up to the parent group.
            //

            while (TRUE) {
                if (CurrentEntry->Next != &(GroupEntry->Children)) {
                    CurrentEntry = CurrentEntry->Next;
                    break;
                }

                if (GroupEntry->Entry.Parent == NULL) {
                    CurrentEntry = CurrentEntry->Next;
                    break;
                }

                CurrentEntry = &(GroupEntry->Entry.ListEntry);
                GroupEntry = PARENT_STRUCTURE(GroupEntry->Entry.Parent,
                                              SCHEDULER_GROUP_ENTRY,
                                              Entry);
            }

            continue;
        }

        //
        // The child is a group. If it has no children, continue to the sibling.
        //

        ASSERT(Entry->Type == SchedulerEntryGroup);

        ChildGroupEntry = PARENT_STRUCTURE(Entry, SCHEDULER_GROUP_ENTRY, Entry);
        if (ChildGroupEntry->ReadyThreadCount == 0) {
            CurrentEntry = CurrentEntry->Next;

        //
        // The child group has ready threads somewhere down there. Descend into
        // it.
        //

        } else {
            GroupEntry = ChildGroupEntry;
            CurrentEntry = GroupEntry->Children.Next;
        }
    }

    //
    // The end of the group was hit without finding a thread.
    //

    return NULL;
}

KSTATUS
KepCreateSchedulerGroup (
    PSCHEDULER_GROUP *NewGroup
    )

/*++

Routine Description:

    This routine creates a new scheduler group underneath the current thread's
    scheduler group.

Arguments:

    NewGroup - Supplies a pointer where a pointer to the new group will be
        returned on success.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    UINTN EntryCount;
    PSCHEDULER_GROUP Group;
    UINTN Index;
    RUNLEVEL OldRunLevel;
    PSCHEDULER_GROUP ParentGroup;
    PSCHEDULER_GROUP_ENTRY ParentGroupEntry;
    PKTHREAD Thread;

    //
    // Get the current thread's group, which serves as this new group's parent.
    //

    Thread = KeGetCurrentThread();
    ParentGroupEntry = PARENT_STRUCTURE(Thread->SchedulerEntry.Parent,
                                        SCHEDULER_GROUP_ENTRY,
                                        Entry);

    ASSERT(ParentGroupEntry->Entry.Type == SchedulerEntryGroup);

    ParentGroup = ParentGroupEntry->Group;

    //
    // Determine the number of entries in this group, which is capped by the
    // parent group.
    //

    EntryCount = KeGetActiveProcessorCount();
    if ((ParentGroup != &KeRootSchedulerGroup) &&
        (EntryCount > ParentGroup->EntryCount)) {

        EntryCount = ParentGroup->EntryCount;
    }

    AllocationSize = sizeof(SCHEDULER_GROUP) +
                     (EntryCount * sizeof(SCHEDULER_GROUP_ENTRY));

    Group = MmAllocateNonPagedPool(AllocationSize, KE_SCHEDULER_ALLOCATION_TAG);
    if (Group == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Group, AllocationSize);
    INITIALIZE_LIST_HEAD(&(Group->Children));
    Group->Entries = (PSCHEDULER_GROUP_ENTRY)(Group + 1);
    Group->EntryCount = EntryCount;
    Group->Parent = ParentGroup;

    //
    // Add the group to the global tree.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&KeSchedulerGroupLock);
    INSERT_BEFORE(&(Group->ListEntry), &(ParentGroup->Children));
    KeReleaseSpinLock(&KeSchedulerGroupLock);
    KeLowerRunLevel(OldRunLevel);
    for (Index = 0; Index < EntryCount; Index += 1) {

        //
        // If the parent is the root, then schedule directly onto the processor.
        //

        if (ParentGroup == &KeRootSchedulerGroup) {
            ParentGroupEntry = &(KeProcessorBlocks[Index]->Scheduler.Group);

        } else {
            ParentGroupEntry = &(ParentGroup->Entries[Index]);
        }

        KepInitializeSchedulerGroupEntry(&(Group->Entries[Index]),
                                         &(KeProcessorBlocks[Index]->Scheduler),
                                         Group,
                                         ParentGroupEntry);

        //
        // Add the scheduler group entry to the parent scheduler group entry.
        //

        KepEnqueueSchedulerEntry(&(Group->Entries[Index].Entry), FALSE);
    }

    *NewGroup = Group;
    return STATUS_SUCCESS;
}

VOID
KepDestroySchedulerGroup (
    PSCHEDULER_GROUP Group
    )

/*++

Routine Description:

    This routine unlinks and destroys a scheduler group.

Arguments:

    Group - Supplies a pointer to the group to destroy.

Return Value:

    None.

--*/

{

    PSCHEDULER_GROUP_ENTRY GroupEntry;
    UINTN Index;
    RUNLEVEL OldRunLevel;

    ASSERT(Group != &KeRootSchedulerGroup);
    ASSERT(Group->ThreadCount == 0);

    for (Index = 0; Index < Group->EntryCount; Index += 1) {
        GroupEntry = &(Group->Entries[Index]);

        ASSERT((GroupEntry->ReadyThreadCount == 0) &&
               (LIST_EMPTY(&(GroupEntry->Children)) != FALSE));

        KepDequeueSchedulerEntry(&(GroupEntry->Entry), FALSE);
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&KeSchedulerGroupLock);
    LIST_REMOVE(&(Group->ListEntry));
    Group->ListEntry.Next = NULL;
    KeReleaseSpinLock(&KeSchedulerGroupLock);
    KeLowerRunLevel(OldRunLevel);
    MmFreeNonPagedPool(Group);
    return;
}

VOID
KepInitializeSchedulerGroupEntry (
    PSCHEDULER_GROUP_ENTRY GroupEntry,
    PSCHEDULER_DATA Scheduler,
    PSCHEDULER_GROUP Group,
    PSCHEDULER_GROUP_ENTRY ParentEntry
    )

/*++

Routine Description:

    This routine initializes a scheduler group entry structure.

Arguments:

    GroupEntry - Supplies a pointer to the group entry to initialize.

    Scheduler - Supplies a pointer to the scheduler that the group entry is on.

    Group - Supplies the scheduler group that owns the group entry.

    ParentEntry - Supplies a pointer to the parent group entry.

Return Value:

    None.

--*/

{

    GroupEntry->Entry.Type = SchedulerEntryGroup;
    if (ParentEntry == NULL) {
        GroupEntry->Entry.Parent = NULL;

    } else {
        GroupEntry->Entry.Parent = &(ParentEntry->Entry);
    }

    INITIALIZE_LIST_HEAD(&(GroupEntry->Children));
    GroupEntry->ReadyThreadCount = 0;
    GroupEntry->Group = Group;
    GroupEntry->Scheduler = Scheduler;
    return;
}

