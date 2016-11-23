/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    workitem.c

Abstract:

    This module handles kernel work items.

Author:

    Evan Green 12-Sep-2012

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
// Define work item crash codes.
//

#define WORK_ITEM_CRASH_MODIFY_QUEUED_ITEM 0x1
#define WORK_ITEM_CRASH_BAD_QUEUE_STATE 0x2

//
// Work item flags.
//

//
// This bit is set when the work item is actively in a queue. It cannot be
// used directly to prevent double-queuing, as it is subject to multiprocessor
// races if used that way.
//

#define WORK_ITEM_FLAG_QUEUED 0x00000001

//
// This bit is set if the work item can be added to a queue or destroyed at
// dispatch level. It is automatically inherited from the queue flags if the
// queue is set to support dispatch level.
//

#define WORK_ITEM_FLAG_SUPPORT_DISPATCH_LEVEL 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a work queue.

Members:

    State - Stoers a pointer to the current work queue state.

    Lock - Stores either a pointer to a queued lock or a spin lock protecting
        the work item list, depending on whether the queue needs to accept
        work items at dispatch level.

    WorkItemListHead - Stores the head of the list of work items to execute.

    WorkItemCount - Stores the number of work items currently queued.

    Event - Stores a pointer to the event used to kick the work item threads
        into action.

    Flags - Stores a bitfield of flags governing the behavior of the work
        queue. See WORK_QUEUE_FLAG_* definitions.

    CurrentThreadCount - Stores the number of threads that are alive and
        processing (or waiting on) the work queue.

    Name - Stores a pointer to a string containing the name of the worker
        threads.

--*/

struct _WORK_QUEUE {
    volatile WORK_QUEUE_STATE State;
    union {
        PQUEUED_LOCK QueuedLock;
        KSPIN_LOCK SpinLock;
    } Lock;

    LIST_ENTRY WorkItemListHead;
    UINTN WorkItemCount;
    PKEVENT Event;
    ULONG Flags;
    volatile ULONG CurrentThreadCount;
    PSTR Name;
};

/*++

Structure Description:

    This structure defines a work item, to be performed by a worker thread at
    low level.

Members:

    ListEntry - Stores pointers to the next and previous work items in the
        work queue. The work queue is sorted by priority.

    ReferenceCount - Stores the reference count of the work item.

    Queue - Stores a pointer to the queue this work item was or will be
        put on.

    Event - Stores a pointer to an event that is signaled when the work item
        completes.

    Routine - Stores the worker routine.

    Parameter - Stores the parameter to pass to the worker routine.

    Priority - Stores the priority of the work item.

    Flags - Stores a pointer to internal flags used by the operating system.
        Do not modify these directly. See WORK_ITEM_FLAG_* definitions.

--*/

struct _WORK_ITEM {
    LIST_ENTRY ListEntry;
    UINTN ReferenceCount;
    PWORK_QUEUE Queue;
    PKEVENT Event;
    PWORK_ITEM_ROUTINE Routine;
    PVOID Parameter;
    WORK_PRIORITY Priority;
    ULONG Flags;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepWorkerThread (
    );

VOID
KepDestroyWorkQueue (
    PWORK_QUEUE Queue
    );

VOID
KepWorkItemAddReference (
    PWORK_ITEM WorkItem
    );

VOID
KepWorkItemReleaseReference (
    PWORK_ITEM WorkItem
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the system work queue.
//

PWORK_QUEUE KeSystemWorkQueue = NULL;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PWORK_QUEUE
KeCreateWorkQueue (
    ULONG Flags,
    PCSTR Name
    )

/*++

Routine Description:

    This routine creates a new work queue.

Arguments:

    Flags - Supplies a bitfield of flags governing the behavior of the work
        queue. See WORK_QUEUE_FLAG_* definitions.

    Name - Supplies an optional pointer to the name of the worker threads
        created. A copy of this memory will be made. This should only be used
        for debugging, as text may be added to the end of the name supplied
        here to the actual worker thread names.

Return Value:

    Returns a pointer to the new work queue on success.

    NULL on failure.

--*/

{

    ULONG NameSize;
    BOOL NonPaged;
    PWORK_QUEUE Queue;
    KSTATUS Status;

    //
    // Parse the flags.
    //

    NonPaged = FALSE;
    if ((Flags & WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
        NonPaged = TRUE;
    }

    //
    // Create and initialize the work queue structure.
    //

    if (NonPaged != FALSE) {
        Queue = MmAllocateNonPagedPool(sizeof(WORK_QUEUE), KE_ALLOCATION_TAG);

    } else {
        Queue = MmAllocatePagedPool(sizeof(WORK_QUEUE), KE_ALLOCATION_TAG);
    }

    if (Queue == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateWorkQueueEnd;
    }

    RtlZeroMemory(Queue, sizeof(WORK_QUEUE));

    //
    // Create a copy of the name, if supplied.
    //

    if (Name != NULL) {
        NameSize = RtlStringLength(Name) + 1;
        Queue->Name = MmAllocatePagedPool(NameSize, KE_ALLOCATION_TAG);
        if (Queue->Name == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateWorkQueueEnd;
        }

        RtlStringCopy(Queue->Name, Name, NameSize);
    }

    if (NonPaged != FALSE) {
        KeInitializeSpinLock(&(Queue->Lock.SpinLock));

    } else {
        Queue->Lock.QueuedLock = KeCreateQueuedLock();
        if (Queue->Lock.QueuedLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateWorkQueueEnd;
        }
    }

    INITIALIZE_LIST_HEAD(&(Queue->WorkItemListHead));
    Queue->Event = KeCreateEvent(NULL);
    if (Queue->Event == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateWorkQueueEnd;
    }

    Queue->Flags = Flags;
    Queue->State = WorkQueueStateOpen;

    //
    // Create a worker thread.
    //

    Status = PsCreateKernelThread(KepWorkerThread, Queue, Name);
    if (!KSUCCESS(Status)) {
        goto CreateWorkQueueEnd;
    }

    Status = STATUS_SUCCESS;

CreateWorkQueueEnd:
    if (!KSUCCESS(Status)) {
        if (Queue != NULL) {
            if (Queue->Name != NULL) {
                MmFreePagedPool(Queue->Name);
            }

            if (NonPaged == FALSE) {
                if (Queue->Lock.QueuedLock != NULL) {
                    KeDestroyQueuedLock(Queue->Lock.QueuedLock);
                }
            }

            if (Queue->Event != NULL) {
                KeDestroyEvent(Queue->Event);
            }

            if (NonPaged != FALSE) {
                MmFreeNonPagedPool(Queue);

            } else {
                MmFreePagedPool(Queue);
            }

            Queue = NULL;
        }
    }

    return Queue;
}

KERNEL_API
VOID
KeDestroyWorkQueue (
    PWORK_QUEUE WorkQueue
    )

/*++

Routine Description:

    This routine destroys a work queue. If there are items on the work queue,
    they will be completed.

Arguments:

    WorkQueue - Supplies a pointer to the work queue to destroy.

Return Value:

    None.

--*/

{

    BOOL DispatchLevel;
    RUNLEVEL OldRunLevel;

    OldRunLevel = RunLevelCount;

    ASSERT((WorkQueue->State != WorkQueueStateInvalid) &&
           (WorkQueue->State != WorkQueueStateDestroying) &&
           (WorkQueue->State != WorkQueueStateDestroyed));

    DispatchLevel = FALSE;
    if ((WorkQueue->Flags & WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
        DispatchLevel = TRUE;
    }

    if (DispatchLevel != FALSE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    }

    //
    // Indicate to the worker threads that a transition is occurring. This
    // routine cannot just set the state directly to destroying because if the
    // thread happens to see that and deletes the queue before this routine
    // gets around to signalling the event, this routine will touch freed
    // memory. The signal event routine must be called because the queues might
    // be asleep from inactivity. So move to this transitory state where the
    // queues know to stay awake but spin waiting for the state to move to
    // destroying.
    //

    WorkQueue->State = WorkQueueStateWakingForDestroying;
    KeSignalEvent(WorkQueue->Event, SignalOptionSignalAll);

    //
    // Now that all workers are awake and spinning, let them destroy themselves.
    // Muah.
    //

    WorkQueue->State = WorkQueueStateDestroying;
    if (DispatchLevel != FALSE) {
        KeLowerRunLevel(OldRunLevel);
    }

    return;
}

KERNEL_API
VOID
KeFlushWorkQueue (
    PWORK_QUEUE WorkQueue
    )

/*++

Routine Description:

    This routine flushes a work queue. If there are items on the work queue,
    they will be completed before this routine returns.

Arguments:

    WorkQueue - Supplies a pointer to the work queue to flush.

Return Value:

    None.

--*/

{

    BOOL DispatchLevel;
    RUNLEVEL OldRunLevel;
    PWORK_ITEM Sentinal;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    OldRunLevel = RunLevelCount;
    if (WorkQueue == NULL) {
        WorkQueue = KeSystemWorkQueue;
    }

    ASSERT(WorkQueue != NULL);
    ASSERT((WorkQueue->State != WorkQueueStateInvalid) &&
           (WorkQueue->State != WorkQueueStateDestroying) &&
           (WorkQueue->State != WorkQueueStateDestroyed));

    DispatchLevel = FALSE;
    if ((WorkQueue->Flags & WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
        DispatchLevel = TRUE;
    }

    //
    // Acquire the queue's lock at the appropriate run level.
    //

    if (DispatchLevel != FALSE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&(WorkQueue->Lock.SpinLock));

    } else {
        KeAcquireQueuedLock(WorkQueue->Lock.QueuedLock);
    }

    //
    // If the queue is empty, then there is no sentinal to record and no work
    // to do.
    //

    if (LIST_EMPTY(&(WorkQueue->WorkItemListHead)) != FALSE) {
        Sentinal = NULL;

    //
    // Otherwise, record the last item in the work queue and signal the worker
    // threads.
    //

    } else {
        Sentinal = (PWORK_ITEM)LIST_VALUE(WorkQueue->WorkItemListHead.Previous,
                                          WORK_ITEM,
                                          ListEntry);

        ASSERT(Sentinal != NULL);

        KeSignalEvent(WorkQueue->Event, SignalOptionSignalAll);
    }

    //
    // Unlock the list to let work proceed.
    //

    if (DispatchLevel != FALSE) {
        KeReleaseSpinLock(&(WorkQueue->Lock.SpinLock));
        KeLowerRunLevel(OldRunLevel);

    } else {
        KeReleaseQueuedLock(WorkQueue->Lock.QueuedLock);
    }

    //
    // If there is a sentinal, wait on it to complete.
    //

    if (Sentinal != NULL) {
        KeWaitForEvent(Sentinal->Event, FALSE, WAIT_TIME_INDEFINITE);
    }

    return;
}

KERNEL_API
PWORK_ITEM
KeCreateWorkItem (
    PWORK_QUEUE WorkQueue,
    WORK_PRIORITY Priority,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID Parameter,
    ULONG AllocationTag
    )

/*++

Routine Description:

    This routine creates a new reusable work item.

Arguments:

    WorkQueue - Supplies a pointer to the queue this work item will
        eventually be queued to. Supply NULL to use the system work queue.

    Priority - Supplies the work priority.

    WorkRoutine - Supplies the routine to execute to do the work. This routine
        should be prepared to take one parameter.

    Parameter - Supplies an optional parameter to pass to the worker routine.

    AllocationTag - Supplies an allocation tag to associate with the work item.

Return Value:

    Returns a pointer to the new work item on success.

    NULL on failure.

--*/

{

    PWORK_ITEM NewWorkItem;
    BOOL NonPaged;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    NewWorkItem = NULL;
    if ((Priority < WorkPriorityNormal) || (Priority > WorkPriorityHigh) ||
        (WorkRoutine == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CreateWorkItemEnd;
    }

    //
    // If no work queue was specified, use the system work queue.
    //

    if (WorkQueue == NULL) {
        WorkQueue = KeSystemWorkQueue;
    }

    NonPaged = FALSE;
    if ((WorkQueue->Flags & WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
        NonPaged = TRUE;
    }

    //
    // Allocate space for a work item.
    //

    if (NonPaged != FALSE) {
        NewWorkItem = MmAllocateNonPagedPool(sizeof(WORK_ITEM), AllocationTag);

    } else {
        NewWorkItem = MmAllocatePagedPool(sizeof(WORK_ITEM), AllocationTag);
    }

    if (NewWorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateWorkItemEnd;
    }

    RtlZeroMemory(NewWorkItem, sizeof(WORK_ITEM));
    NewWorkItem->ReferenceCount = 1;

    //
    // If the work queue has to support dispatch level, then the work item
    // needs to as well.
    //

    if (NonPaged != FALSE) {
        NewWorkItem->Flags |= WORK_ITEM_FLAG_SUPPORT_DISPATCH_LEVEL;
    }

    //
    // Initialize the rest of the work item. With the above flag set the
    // destroy routine can be used if things do not work out.
    //

    KeSetWorkItemParameters(NewWorkItem, Priority, WorkRoutine, Parameter);
    NewWorkItem->Queue = WorkQueue;
    NewWorkItem->Event = KeCreateEvent(NULL);
    if (NewWorkItem->Event == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateWorkItemEnd;
    }

    KeSignalEvent(NewWorkItem->Event, SignalOptionSignalAll);
    Status = STATUS_SUCCESS;

CreateWorkItemEnd:
    if (!KSUCCESS(Status)) {
        if (NewWorkItem != NULL) {
            KeDestroyWorkItem(NewWorkItem);
            NewWorkItem = NULL;
        }
    }

    return NewWorkItem;
}

KERNEL_API
VOID
KeDestroyWorkItem (
    PWORK_ITEM WorkItem
    )

/*++

Routine Description:

    This routine destroys a reusable work item. If this is a work item that
    can re-queue itself, then the caller needs to make sure that that can no
    longer happen before trying to destroy the work item.

Arguments:

    WorkItem - Supplies a pointer to the work item.

Return Value:

    None.

--*/

{

    //
    // Always attempt to cancel the work item.
    //

    KeCancelWorkItem(WorkItem);
    KepWorkItemReleaseReference(WorkItem);
    return;
}

KERNEL_API
KSTATUS
KeCancelWorkItem (
    PWORK_ITEM WorkItem
    )

/*++

Routine Description:

    This routine attempts to cancel the work item. If the work item is still on
    its work queue then this routine will pull it off and return successfully.
    Otherwise the work item may have been selected to run and this routine will
    return that the cancel was too late. Keep in mind that "too late" may also
    mean "too early" if the work item was never queued.

Arguments:

    WorkItem - Supplies a pointer to the work item to cancel.

Return Value:

    Status code.

--*/

{

    BOOL DispatchLevel;
    RUNLEVEL OldRunLevel;
    PWORK_QUEUE Queue;
    KSTATUS Status;

    OldRunLevel = RunLevelCount;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    //
    // Quickly return "too late" if the work item is not queued. It may be
    // about to run or running, or it might not have been queued.
    //

    if ((WorkItem->Flags & WORK_ITEM_FLAG_QUEUED) == 0) {
        return STATUS_TOO_LATE;
    }

    //
    // If the queue is not in a state to cancel a work item. Crash.
    //

    Queue = WorkItem->Queue;
    if ((Queue->State != WorkQueueStateOpen) &&
        (Queue->State != WorkQueueStatePaused)) {

        KeCrashSystem(CRASH_WORK_ITEM_CORRUPTION,
                      WORK_ITEM_CRASH_BAD_QUEUE_STATE,
                      (UINTN)WorkItem,
                      (UINTN)Queue,
                      Queue->State);
    }

    //
    // Determine whether or not to raise to dispatch level before acquiring
    // the lock.
    //

    DispatchLevel = FALSE;
    if ((Queue->Flags & WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
        DispatchLevel = TRUE;
    }

    //
    // Acquire the work queue lock.
    //

    if (DispatchLevel != FALSE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&(Queue->Lock.SpinLock));

    } else {
        KeAcquireQueuedLock(Queue->Lock.QueuedLock);
    }

    //
    // Now that the lock is held, check again to see if the work item was
    // selected to run and pulled off the list.
    //

    if ((WorkItem->Flags & WORK_ITEM_FLAG_QUEUED) == 0) {
        WorkItem = NULL;
        Status = STATUS_TOO_LATE;
        goto CancelWorkItemEnd;
    }

    ASSERT((WorkItem->Flags & WORK_ITEM_FLAG_QUEUED) != 0);
    ASSERT(WorkItem->ListEntry.Next != NULL);

    //
    // Remove the work item from the queue, signal it, and return successfully.
    //

    LIST_REMOVE(&(WorkItem->ListEntry));
    WorkItem->ListEntry.Next = NULL;
    Queue->WorkItemCount -= 1;
    WorkItem->Flags &= ~WORK_ITEM_FLAG_QUEUED;
    KeSignalEvent(WorkItem->Event, SignalOptionSignalAll);
    Status = STATUS_SUCCESS;

CancelWorkItemEnd:
    if (DispatchLevel != FALSE) {
        KeReleaseSpinLock(&(Queue->Lock.SpinLock));
        KeLowerRunLevel(OldRunLevel);

    } else {
        KeReleaseQueuedLock(Queue->Lock.QueuedLock);
    }

    if (WorkItem != NULL) {
        KepWorkItemReleaseReference(WorkItem);
    }

    return Status;
}

KERNEL_API
VOID
KeFlushWorkItem (
    PWORK_ITEM WorkItem
    )

/*++

Routine Description:

    This routine does not return until the given work item has completed.

Arguments:

    WorkItem - Supplies a pointer to the work item.

Return Value:

    None.

--*/

{

    KeWaitForEvent(WorkItem->Event, FALSE, WAIT_TIME_INDEFINITE);
    return;
}

KERNEL_API
VOID
KeSetWorkItemParameters (
    PWORK_ITEM WorkItem,
    WORK_PRIORITY Priority,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID Parameter
    )

/*++

Routine Description:

    This routine resets the parameters of a work item to the given parameters.
    The work item must not be queued. This routine must be called at or below
    dispatch level.

Arguments:

    WorkItem - Supplies a pointer to the work item to modify.

    Priority - Supplies the new work priority.

    WorkRoutine - Supplies the routine to execute to do the work. This routine
        should be prepared to take one parameter.

    Parameter - Supplies an optional parameter to pass to the worker routine.

Return Value:

    None.

--*/

{

    if ((WorkItem->Flags & WORK_ITEM_FLAG_QUEUED) != 0) {
        KeCrashSystem(CRASH_WORK_ITEM_CORRUPTION,
                      WORK_ITEM_CRASH_MODIFY_QUEUED_ITEM,
                      (UINTN)WorkItem,
                      (UINTN)WorkRoutine,
                      (UINTN)Parameter);
    }

    WorkItem->Priority = Priority;
    WorkItem->Routine = WorkRoutine;
    WorkItem->Parameter = Parameter;
    return;
}

KERNEL_API
KSTATUS
KeQueueWorkItem (
    PWORK_ITEM WorkItem
    )

/*++

Routine Description:

    This routine queues a work item onto the work queue for execution as soon
    as possible. This routine must be called from dispatch level or below.

Arguments:

    WorkItem - Supplies a pointer to the work item to queue.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_RESOURCE_IN_USE if the work item is already queued.

--*/

{

    BOOL DispatchLevel;
    RUNLEVEL OldRunLevel;
    PWORK_QUEUE Queue;
    KSTATUS Status;

    OldRunLevel = RunLevelCount;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    if ((WorkItem->Flags & WORK_ITEM_FLAG_QUEUED) != 0) {
        return STATUS_RESOURCE_IN_USE;
    }

    Queue = WorkItem->Queue;
    if ((Queue->State != WorkQueueStateOpen) &&
        (Queue->State != WorkQueueStatePaused)) {

        KeCrashSystem(CRASH_WORK_ITEM_CORRUPTION,
                      WORK_ITEM_CRASH_BAD_QUEUE_STATE,
                      (UINTN)WorkItem,
                      (UINTN)Queue,
                      Queue->State);
    }

    //
    // Determine whether or not to raise to dispatch level before acquiring
    // the lock.
    //

    DispatchLevel = FALSE;
    if ((Queue->Flags & WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
        DispatchLevel = TRUE;
    }

    KepWorkItemAddReference(WorkItem);

    //
    // Acquire the work queue lock.
    //

    if (DispatchLevel != FALSE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&(Queue->Lock.SpinLock));

    } else {
        KeAcquireQueuedLock(Queue->Lock.QueuedLock);
    }

    //
    // Now that the lock is held, check again to see if someone else snuck in
    // and queued this work item.
    //

    if ((WorkItem->Flags & WORK_ITEM_FLAG_QUEUED) != 0) {
        Status = STATUS_RESOURCE_IN_USE;
        goto QueueWorkItemEnd;
    }

    //
    // Mark the work item as having been queued now that the lock is held.
    //

    WorkItem->Flags |= WORK_ITEM_FLAG_QUEUED;
    KeSignalEvent(WorkItem->Event, SignalOptionUnsignal);

    //
    // Insert high priority items on the beginning of the list, and normal items
    // on the end.
    //

    if (WorkItem->Priority == WorkPriorityHigh) {
        INSERT_AFTER(&(WorkItem->ListEntry), &(Queue->WorkItemListHead));

    } else {
        INSERT_BEFORE(&(WorkItem->ListEntry), &(Queue->WorkItemListHead));
    }

    Queue->WorkItemCount += 1;
    WorkItem = NULL;
    Status = STATUS_SUCCESS;

QueueWorkItemEnd:
    if (DispatchLevel != FALSE) {
        KeReleaseSpinLock(&(Queue->Lock.SpinLock));
        KeLowerRunLevel(OldRunLevel);

    } else {
        KeReleaseQueuedLock(Queue->Lock.QueuedLock);
    }

    if (KSUCCESS(Status)) {

        //
        // Signal the event to kick off the worker threads.
        //

        KeSignalEvent(Queue->Event, SignalOptionSignalAll);
    }

    if (WorkItem != NULL) {
        KepWorkItemReleaseReference(WorkItem);
    }

    return Status;
}

KERNEL_API
KSTATUS
KeCreateAndQueueWorkItem (
    PWORK_QUEUE WorkQueue,
    WORK_PRIORITY Priority,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID Parameter
    )

/*++

Routine Description:

    This routine creates and queues a work item. This work item will get
    executed in a worker thread an arbitrary amount of time later. The work
    item will be automatically freed after the work routine is executed.

Arguments:

    WorkQueue - Supplies a pointer to the queue this work item will
        eventually be queued to. Supply NULL to use the system work queue.

    Priority - Supplies the work priority.

    WorkRoutine - Supplies the routine to execute to doe the work. This
        routine should be prepared to take one parameter.

    Parameter - Supplies an optional parameter to pass to the worker routine.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNSUCCESSFUL on failure.

--*/

{

    KSTATUS Status;
    PWORK_ITEM WorkItem;

    //
    // Create the new work item, and set the flag to automatically free it.
    //

    WorkItem = KeCreateWorkItem(WorkQueue,
                                Priority,
                                WorkRoutine,
                                Parameter,
                                KE_WORK_ITEM_ALLOCATION_TAG);

    if (WorkItem == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    Status = KeQueueWorkItem(WorkItem);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Release the reference on the work item from when it was created, so that
    // after it runs it will automatically destroy itself.
    //

    KepWorkItemReleaseReference(WorkItem);
    return Status;
}

KSTATUS
KepInitializeSystemWorkQueue (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system work queue. This must happen after the
    Object Manager initializes.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG Flags;

    Flags = WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL;
    KeSystemWorkQueue = KeCreateWorkQueue(Flags, "KeWorker");
    if (KeSystemWorkQueue == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepWorkerThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes work items off of a work queue.

Arguments:

    Parameter - Supplies a pointer to a parameter that in this case contains a
        pointer to the work queue to service.

Return Value:

    None. Does not return.

--*/

{

    RUNLEVEL OldRunLevel;
    PWORK_QUEUE Queue;
    BOOL RaiseToDispatch;
    ULONG RemainingThreads;
    PWORK_ITEM WorkItem;

    OldRunLevel = RunLevelCount;
    Queue = (PWORK_QUEUE)Parameter;
    RtlAtomicAdd32(&(Queue->CurrentThreadCount), 1);
    RaiseToDispatch = FALSE;
    if ((Queue->Flags & WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
        RaiseToDispatch = TRUE;
    }

    while (TRUE) {

        //
        // Wait for the event, then process work items until none are left.
        //

        KeWaitForEvent(Queue->Event, FALSE, WAIT_TIME_INDEFINITE);
        while (TRUE) {
            WorkItem = NULL;
            if (RaiseToDispatch != FALSE) {
                OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
                KeAcquireSpinLock(&(Queue->Lock.SpinLock));

            } else {
                KeAcquireQueuedLock(Queue->Lock.QueuedLock);
            }

            if (LIST_EMPTY(&(Queue->WorkItemListHead)) == FALSE) {
                WorkItem = LIST_VALUE(Queue->WorkItemListHead.Next,
                                      WORK_ITEM,
                                      ListEntry);

                LIST_REMOVE(&(WorkItem->ListEntry));
                WorkItem->ListEntry.Next = NULL;
                Queue->WorkItemCount -= 1;
                WorkItem->Flags &= ~WORK_ITEM_FLAG_QUEUED;

            } else {
                KeSignalEvent(Queue->Event, SignalOptionUnsignal);
            }

            if (RaiseToDispatch != FALSE) {
                KeReleaseSpinLock(&(Queue->Lock.SpinLock));
                KeLowerRunLevel(OldRunLevel);

            } else {
                KeReleaseQueuedLock(Queue->Lock.QueuedLock);
            }

            //
            // If there is a work item, execute it.
            //

            if (WorkItem != NULL) {
                WorkItem->Routine(WorkItem->Parameter);
                KeSignalEvent(WorkItem->Event, SignalOptionSignalAll);
                KepWorkItemReleaseReference(WorkItem);

            //
            // If there was no work item, stop looking.
            //

            } else {
                break;
            }

            //
            // If the work queue became paused, stop processing events.
            //

            if (Queue->State == WorkQueueStatePaused) {
                break;
            }
        }

        //
        // If this thread happened to catch someone else marking this queue
        // for destruction, politely wait for that operation to complete and
        // avoid destroying the queue out from under it.
        //

        while (Queue->State == WorkQueueStateWakingForDestroying) {
            KeYield();
        }

        if (Queue->State == WorkQueueStateDestroying) {
            RemainingThreads = RtlAtomicAdd32(&(Queue->CurrentThreadCount), -1);

            //
            // If this is the last thread standing, turn out the lights by
            // destroying the work queue.
            //

            if (RemainingThreads == 1) {
                Queue->State = WorkQueueStateDestroyed;
                KepDestroyWorkQueue(Queue);
                break;
            }
        }
    }

    return;
}

VOID
KepDestroyWorkQueue (
    PWORK_QUEUE Queue
    )

/*++

Routine Description:

    This routine destroys and frees a work queue. This routine will be
    called automatically by the last worker thread to exit.

Arguments:

    Queue - Supplies a pointer to the queue to destroy.

Return Value:

    None.

--*/

{

    BOOL NonPaged;

    ASSERT(Queue->CurrentThreadCount == 0);

    NonPaged = FALSE;
    if ((Queue->Flags & WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
        NonPaged = TRUE;
    }

    if (Queue->Name != NULL) {
        MmFreePagedPool(Queue->Name);
    }

    if ((NonPaged != FALSE) && (Queue->Lock.QueuedLock != NULL)) {
        KeDestroyQueuedLock(Queue->Lock.QueuedLock);
    }

    if (Queue->Event != NULL) {
        KeDestroyEvent(Queue->Event);
    }

    if (NonPaged != FALSE) {
        MmFreeNonPagedPool(Queue);

    } else {
        MmFreePagedPool(Queue);
    }

    return;
}

VOID
KepWorkItemAddReference (
    PWORK_ITEM WorkItem
    )

/*++

Routine Description:

    This routine adds a reference to the given work item.

Arguments:

    WorkItem - Supplies a pointer to the work item to add a reference to.

Return Value:

    None.

--*/

{

    UINTN OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd(&(WorkItem->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
KepWorkItemReleaseReference (
    PWORK_ITEM WorkItem
    )

/*++

Routine Description:

    This routine release the reference on a work item. If the reference count
    drops to zero, the work item will be destroyed.

Arguments:

    WorkItem - Supplies a pointer to the work item to release.

Return Value:

    None.

--*/

{

    BOOL NonPaged;
    UINTN OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd(&(WorkItem->ReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {

        ASSERT((WorkItem->Flags & WORK_ITEM_FLAG_QUEUED) == 0);

        if (WorkItem->Event != NULL) {
            KeDestroyEvent(WorkItem->Event);
        }

        NonPaged = FALSE;
        if ((WorkItem->Flags & WORK_ITEM_FLAG_SUPPORT_DISPATCH_LEVEL) != 0) {
            NonPaged = TRUE;
        }

        if (NonPaged != FALSE) {
            MmFreeNonPagedPool(WorkItem);

        } else {
            MmFreePagedPool(WorkItem);
        }
    }

    return;
}

