/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    obapi.c

Abstract:

    This module implements the Object Manager API.

Author:

    Evan Green 4-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// This bit is set if the wait block has entries actively queued on objects.
//

#define WAIT_BLOCK_FLAG_ACTIVE 0x80000000

//
// This bit is set if the thread came out of the wait due to an interruption
// (versus an actual satisfaction of the wait).
//

#define WAIT_BLOCK_FLAG_INTERRUPTED 0x40000000

//
// Define the maximum number of allowed wait block entries.
//

#define WAIT_BLOCK_MAX_CAPACITY MAX_USHORT

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ObpWaitFast (
    PWAIT_QUEUE WaitQueue
    );

VOID
ObpCleanUpWaitBlock (
    PWAIT_BLOCK WaitBlock,
    ULONG InitializedCount
    );

BOOL
ObpAreObjectNamesEqual (
    PCSTR ExistingObject,
    PCSTR QueryObject,
    ULONG QuerySize
    );

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a single entry in a wait block.

Members:

    WaitBlock - Stores a pointer to the parent wait block that owns this entry.

    WaitListEntry - Stores pointers to the next and previous wait block entries
        in the target queue's wait list.

    Queue - Stores a pointer to the queue being waited on.

--*/

typedef struct _WAIT_BLOCK_ENTRY {
    PWAIT_BLOCK WaitBlock;
    LIST_ENTRY WaitListEntry;
    PWAIT_QUEUE Queue;
} WAIT_BLOCK_ENTRY, *PWAIT_BLOCK_ENTRY;

/*++

Structure Description:

    This structure defines a wait block.

Members:

    Capacity - Stores the total number of wait entries in the block. The
        structure defines a minimum size, but wait blocks may be allocated with
        space for more than the structure amount (the array just keeps going
        because it's on the end.

    Count - Stores the number of active queues in the wait block.

    UnsignaledCount - Stores the number of active queues in the wait block that
        have not yet been signaled.

    Thread - Stores a pointer to the thread to be signaled when the wait block
        is satisfied.

    Lock - Stores a spin lock synchronizing access to the wait block.

    Flags - Stores a bitfield of flags regarding the wait block. See
        WAIT_BLOCK_FLAG_* definitions.

    SignalingQueue - Stores a pointer to the queue that broke the wait. If all
        queues must be satisfied, this field contains the last queue to be
        satisfied.

    Timeout - Stores a wait block entry for the timeout on this wait block.

    Entry - Stores the array of wait block entries, one for each object being
        waited on. This array may be larger than the defined size in the
        structure (see the Capacity parameter), in which case the wait block
        entries simply keep going.

--*/

struct _WAIT_BLOCK {
    USHORT Capacity;
    USHORT Count;
    USHORT UnsignaledCount;
    PVOID Thread;
    ULONG Flags;
    PWAIT_QUEUE SignalingQueue;
    KSPIN_LOCK Lock;
    WAIT_BLOCK_ENTRY Entry[BUILTIN_WAIT_BLOCK_ENTRY_COUNT];
};

//
// -------------------------------------------------------------------- Globals
//

POBJECT_HEADER ObRootObject = NULL;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
ObInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the Object Manager. It requires that the MM pools
    are online.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    //
    // Manually create the root object.
    //

    ObRootObject = MmAllocateNonPagedPool(sizeof(OBJECT_HEADER),
                                          OBJECT_MANAGER_POOL_TAG);

    if (ObRootObject == NULL) {
        return STATUS_NO_MEMORY;
    }

    RtlZeroMemory(ObRootObject, sizeof(OBJECT_HEADER));
    ObRootObject->Type = ObjectDirectory;
    ObRootObject->Name = "";
    ObRootObject->NameLength = sizeof("");
    ObInitializeWaitQueue(&(ObRootObject->WaitQueue), NotSignaled);
    ObRootObject->Parent = NULL;
    INITIALIZE_LIST_HEAD(&(ObRootObject->SiblingEntry));
    INITIALIZE_LIST_HEAD(&(ObRootObject->ChildListHead));
    ObRootObject->Flags = OBJECT_FLAG_USE_NAME_DIRECTLY;
    ObRootObject->ReferenceCount = 1;
    return STATUS_SUCCESS;
}

PVOID
ObGetRootObject (
    VOID
    )

/*++

Routine Description:

    This routine returns the root object of the system.

Arguments:

    None.

Return Value:

    Returns a pointer to the root object.

--*/

{

    return ObRootObject;
}

VOID
ObInitializeWaitQueue (
    PWAIT_QUEUE WaitQueue,
    SIGNAL_STATE InitialState
    )

/*++

Routine Description:

    This routine initializes a wait queue structure.

Arguments:

    WaitQueue - Supplies a pointer to the wait queue to initialize.

    InitialState - Supplies the initial state to set the queue to.

Return Value:

    None.

--*/

{

    KeInitializeSpinLock(&(WaitQueue->Lock));
    WaitQueue->State = InitialState;
    INITIALIZE_LIST_HEAD(&(WaitQueue->Waiters));
    return;
}

PVOID
ObCreateObject (
    OBJECT_TYPE Type,
    PVOID Parent,
    PCSTR ObjectName,
    ULONG NameLength,
    ULONG DataSize,
    PDESTROY_OBJECT_ROUTINE DestroyRoutine,
    ULONG Flags,
    ULONG Tag
    )

/*++

Routine Description:

    This routine creates a new system object.

Arguments:

    Type - Supplies the type of object being created.

    Parent - Supplies a pointer to the object that this object is a child under.
        Supply NULL to create an object off the root node.

    ObjectName - Supplies an optional name for the object. A copy of this
        string will be made unless the flags specify otherwise.

    NameLength - Supplies the length of the name string in bytes, including
        the null terminator.

    DataSize - Supplies the size of the object body, *including* the object
        header.

    DestroyRoutine - Supplies an optional pointer to a function to be called
        when the reference count of the object drops to zero (immediately before
        the object is deallocated).

    Flags - Supplies optional flags indicating various properties of the object.
        See OBJECT_FLAG_* definitions.

    Tag - Supplies the pool tag that should be used for the memory allocation.

Return Value:

    Returns a pointer to the new object, on success. The returned structure is
        assumed to start with an OBJECT_HEADER structure.

    NULL if the object could not be allocated, the object already exists, or
        an invalid parameter was passed in.

--*/

{

    POBJECT_HEADER NewObject;
    RUNLEVEL OldRunLevel;
    POBJECT_HEADER ParentObject;
    KSTATUS Status;

    NewObject = NULL;
    ParentObject = (POBJECT_HEADER)Parent;
    if (ParentObject == NULL) {
        ParentObject = ObRootObject;
    }

    //
    // If there's not even enough room for the object header, fail.
    //

    ASSERT(DataSize >= sizeof(OBJECT_HEADER));

    if (DataSize < sizeof(OBJECT_HEADER)) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateObjectEnd;
    }

    //
    // Allocate the new object and potentially its name string.
    //

    NewObject = MmAllocateNonPagedPool(DataSize, Tag);
    if (NewObject == NULL) {
        Status = STATUS_NO_MEMORY;
        goto CreateObjectEnd;
    }

    RtlZeroMemory((NewObject + 1), (DataSize - sizeof(OBJECT_HEADER)));
    NewObject->Flags = Flags;
    NewObject->Name = NULL;
    if ((Flags & OBJECT_FLAG_USE_NAME_DIRECTLY) != 0) {
        NewObject->Name = ObjectName;

    } else if (ObjectName != NULL) {
        NewObject->Name = MmAllocateNonPagedPool(NameLength, Tag);
        if (NewObject->Name == NULL) {
            Status = STATUS_NO_MEMORY;
            goto CreateObjectEnd;
        }

        RtlStringCopy((PSTR)(NewObject->Name), ObjectName, NameLength);
        ((PSTR)(NewObject->Name))[NameLength - 1] = '\0';
    }

    NewObject->NameLength = NameLength;
    NewObject->Type = Type;
    NewObject->DestroyRoutine = DestroyRoutine;

    //
    // Add a reference to the parent tree.
    //

    ObAddReference(ParentObject);
    ObInitializeWaitQueue(&(NewObject->WaitQueue), NotSignaled);
    NewObject->Parent = ParentObject;
    INITIALIZE_LIST_HEAD(&(NewObject->ChildListHead));
    NewObject->ReferenceCount = 1;

    //
    // Link the new object to the parent.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(ParentObject->WaitQueue.Lock));
    INSERT_AFTER(&(NewObject->SiblingEntry), &(ParentObject->ChildListHead));
    KeReleaseSpinLock(&(ParentObject->WaitQueue.Lock));
    KeLowerRunLevel(OldRunLevel);
    Status = STATUS_SUCCESS;

CreateObjectEnd:
    if (!KSUCCESS(Status)) {
        if (NewObject != NULL) {
            if (((Flags & OBJECT_FLAG_USE_NAME_DIRECTLY) == 0) &&
                (NewObject->Name != NULL)) {

                MmFreeNonPagedPool((PVOID)(NewObject->Name));
            }

            MmFreeNonPagedPool(NewObject);
            NewObject = NULL;
        }
    }

    return NewObject;
}

KERNEL_API
VOID
ObAddReference (
    PVOID Object
    )

/*++

Routine Description:

    This routine increases the reference count on an object by 1.

Arguments:

    Object - Supplies a pointer to the object to add a reference to. This
        structure is presumed to begin with an OBJECT_HEADER.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;
    POBJECT_HEADER TypedObject;

    TypedObject = (POBJECT_HEADER)Object;
    OldReferenceCount = RtlAtomicAdd32(&(TypedObject->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    return;
}

KERNEL_API
VOID
ObReleaseReference (
    PVOID Object
    )

/*++

Routine Description:

    This routine decreases the reference count of an object by 1. If this
    causes the reference count of the object to drop to 0, the object will be
    freed. This may cascade up the tree.

Arguments:

    Object - Supplies a pointer to the object to subtract a reference from.
        This structure is presumed to begin with an OBJECT_HEADER structure.

Return Value:

    None.

--*/

{

    POBJECT_HEADER CurrentObject;
    ULONG OldReferenceCount;
    RUNLEVEL OldRunLevel;
    POBJECT_HEADER ParentObject;

    ParentObject = NULL;
    CurrentObject = (POBJECT_HEADER)Object;
    while (TRUE) {
        OldReferenceCount = RtlAtomicAdd32(&(CurrentObject->ReferenceCount),
                                           (ULONG)-1);

        ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

        ParentObject = CurrentObject->Parent;

        //
        // If this decrement caused the reference count to drop to 0, free the
        // object.
        //

        if (OldReferenceCount == 1) {

            //
            // There should be no waiters if this is the last reference since
            // presumably whomever was waiting on the object had a reference
            // to it. There should also be no children.
            //

            ASSERT(LIST_EMPTY(&(CurrentObject->WaitQueue.Waiters)));
            ASSERT(LIST_EMPTY(&(CurrentObject->ChildListHead)));

            //
            // Attempt to unlink this from the tree. Until the parent's lock
            // is held, the find object routine can come in and increment the
            // reference count. Presumably no one else could increase the
            // reference count on this object since this was the last one.
            //

            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            KeAcquireSpinLock(&(ParentObject->WaitQueue.Lock));

            ASSERT(CurrentObject->ReferenceCount == 0);

            LIST_REMOVE(&(CurrentObject->SiblingEntry));
            KeReleaseSpinLock(&(ParentObject->WaitQueue.Lock));
            KeLowerRunLevel(OldRunLevel);
            CurrentObject->Parent = NULL;

            //
            // Call the destroy routine.
            //

            if (CurrentObject->DestroyRoutine != NULL) {
                CurrentObject->DestroyRoutine(CurrentObject);
            }

            if (((CurrentObject->Flags & OBJECT_FLAG_USE_NAME_DIRECTLY) == 0) &&
                (CurrentObject->Name != NULL)) {

                MmFreeNonPagedPool((PVOID)(CurrentObject->Name));
            }

            MmFreeNonPagedPool(CurrentObject);
            CurrentObject = ParentObject;
            continue;
        }

        break;
    }

    return;
}

KSTATUS
ObUnlinkObject (
    PVOID Object
    )

/*++

Routine Description:

    This routine unlinks an object.

Arguments:

    Object - Supplies a pointer to the object to unlink.

Return Value:

    Status code.

--*/

{

    PSTR NameToFree;
    POBJECT_HEADER ObjectHeader;
    RUNLEVEL OldRunLevel;
    POBJECT_HEADER Parent;

    ObjectHeader = (POBJECT_HEADER)Object;

    //
    // Do nothing if there is no name. The object is already "unlinked" in the
    // sense that it cannot be found on search.
    //

    if (ObjectHeader->Name == NULL) {
        return STATUS_SUCCESS;
    }

    //
    // Unlink is achieved by setting the object's name to NULL. This prevents
    // future lookups from finding the object. The alternative would be to
    // remove the object from its parent's list of children, but that would
    // require a change to ObReleaseReference. As unlinking an object is less
    // common that releasing a reference, the method of setting the name to
    // NULL is preferred, albeit odd.
    //

    NameToFree = NULL;
    Parent = ObjectHeader->Parent;
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Parent->WaitQueue.Lock));
    if ((ObjectHeader->Name != NULL) &&
        ((ObjectHeader->Flags & OBJECT_FLAG_USE_NAME_DIRECTLY) == 0)) {

        NameToFree = (PSTR)(ObjectHeader->Name);
    }

    ObjectHeader->Name = NULL;
    ObjectHeader->NameLength = 0;
    if ((ObjectHeader->Flags & OBJECT_FLAG_USE_NAME_DIRECTLY) != 0) {
        RtlAtomicAnd32(&(ObjectHeader->Flags), ~OBJECT_FLAG_USE_NAME_DIRECTLY);
    }

    KeReleaseSpinLock(&(Parent->WaitQueue.Lock));
    KeLowerRunLevel(OldRunLevel);
    if (NameToFree != NULL) {
        MmFreeNonPagedPool(NameToFree);
    }

    return STATUS_SUCCESS;
}

KSTATUS
ObNameObject (
    PVOID Object,
    PCSTR Name,
    ULONG NameLength,
    ULONG Tag,
    BOOL UseNameDirectly
    )

/*++

Routine Description:

    This routine names an object.

Arguments:

    Object - Supplies a pointer to the object to name.

    Name - Supplies a pointer to the object name.

    NameLength - Supplies the new name's length.

    Tag - Supplies the pool tag to use to allocate the name.

    UseNameDirectly - Supplies the new value of the "Use name directly" flag
        in the object.

Return Value:

    Status code.

--*/

{

    PSTR NameToSet;
    POBJECT_HEADER ObjectHeader;
    RUNLEVEL OldRunLevel;
    POBJECT_HEADER Parent;
    KSTATUS Status;

    ObjectHeader = (POBJECT_HEADER)Object;

    ASSERT(Name != NULL);
    ASSERT(NameLength != 0);

    //
    // Fail if the object is already named.
    //

    if (ObjectHeader->Name != NULL) {
        return STATUS_TOO_LATE;
    }

    //
    // Create a copy of the name unless the flag is set.
    //

    if (UseNameDirectly != FALSE) {
        NameToSet = (PSTR)Name;

    } else {
        NameToSet = MmAllocateNonPagedPool(NameLength, Tag);
        if (NameToSet == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlStringCopy(NameToSet, Name, NameLength);
    }

    //
    // Lock the parent during the set to synchronize with another request to
    // name the object and with any lookup requests.
    //

    Parent = ObjectHeader->Parent;
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Parent->WaitQueue.Lock));
    if (ObjectHeader->Name != NULL) {
        Status = STATUS_TOO_LATE;

    } else {
        ObjectHeader->Name = NameToSet;
        ObjectHeader->NameLength = NameLength;
        if (UseNameDirectly != FALSE) {
            RtlAtomicOr32(&(ObjectHeader->Flags),
                          OBJECT_FLAG_USE_NAME_DIRECTLY);
        }

        Status = STATUS_SUCCESS;
    }

    KeReleaseSpinLock(&(Parent->WaitQueue.Lock));
    KeLowerRunLevel(OldRunLevel);
    if (!KSUCCESS(Status)) {
        if ((UseNameDirectly == FALSE) && (NameToSet != NULL)) {
            MmFreeNonPagedPool(NameToSet);
        }
    }

    return Status;
}

PWAIT_BLOCK
ObCreateWaitBlock (
    ULONG Capacity
    )

/*++

Routine Description:

    This routine creates a wait block. While this can be done on the fly,
    creating a wait block ahead of time is potentially faster if the number of
    elements being waited on is fairly large (greater than approximately 7).

Arguments:

    Capacity - Supplies the maximum number of queues that will be waited on
        with this wait block.

Return Value:

    Returns a pointer to the wait block on success.

    NULL on allocation failure.

--*/

{

    ULONG AllocationSize;
    PWAIT_BLOCK WaitBlock;

    //
    // As this routine is not exported, assert that the requested capacity is
    // less than the maximum allowed capacity. This accounts for the built-in
    // timer.
    //

    ASSERT(Capacity < WAIT_BLOCK_MAX_CAPACITY);

    //
    // Add space for the timeout timer slot.
    //

    Capacity += 1;
    if (Capacity <= BUILTIN_WAIT_BLOCK_ENTRY_COUNT) {
        AllocationSize = sizeof(WAIT_BLOCK);
        Capacity = BUILTIN_WAIT_BLOCK_ENTRY_COUNT;

    } else {
        AllocationSize = sizeof(WAIT_BLOCK) +
                         ((Capacity - BUILTIN_WAIT_BLOCK_ENTRY_COUNT) *
                          sizeof(WAIT_BLOCK_ENTRY));
    }

    WaitBlock = MmAllocateNonPagedPool(AllocationSize, OBJECT_MANAGER_POOL_TAG);
    if (WaitBlock == NULL) {
        return NULL;
    }

    RtlZeroMemory(WaitBlock, AllocationSize);
    KeInitializeSpinLock(&(WaitBlock->Lock));
    WaitBlock->Capacity = Capacity;
    return WaitBlock;
}

VOID
ObDestroyWaitBlock (
    PWAIT_BLOCK WaitBlock
    )

/*++

Routine Description:

    This routine destroys an explicitly created wait block. The wait block must
    not be actively waiting on anything.

Arguments:

    WaitBlock - Supplies a pointer to the wait block.

Return Value:

    None.

--*/

{

    ASSERT((WaitBlock->Flags & WAIT_BLOCK_FLAG_ACTIVE) == 0);

    MmFreeNonPagedPool(WaitBlock);
    return;
}

KSTATUS
ObWait (
    PWAIT_BLOCK WaitBlock,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine executes a wait block, waiting on the given list of wait
    queues for the specified amount of time.

Arguments:

    WaitBlock - Supplies a pointer to the wait block to wait for.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        queues should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever.

Return Value:

    STATUS_SUCCESS if the wait completed successfully.

    STATUS_TIMEOUT if the wait timed out.

    STATUS_INTERRUPTED if the wait timed out early due to a signal.

--*/

{

    BOOL Block;
    ULONG Count;
    ULONGLONG DueTime;
    ULONG Index;
    BOOL LockHeld;
    RUNLEVEL OldRunLevel;
    PWAIT_QUEUE Queue;
    KSTATUS Status;
    PKTHREAD Thread;
    BOOL TimerQueued;
    PWAIT_BLOCK_ENTRY WaitEntry;

    ASSERT((WaitBlock->Capacity != 0) &&
           (WaitBlock->Count <= WaitBlock->Capacity));

    ASSERT(WaitBlock->UnsignaledCount == 0);
    ASSERT(WaitBlock->Entry[0].Queue == NULL);
    ASSERT((WaitBlock->Flags & WAIT_BLOCK_FLAG_ACTIVE) == 0);
    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT(WaitBlock->Thread == NULL);

    Block = TRUE;
    Count = WaitBlock->Count;
    Thread = KeGetCurrentThread();
    TimerQueued = FALSE;
    Status = STATUS_SUCCESS;

    //
    // Acquire the wait block lock and loop through each object in the array.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(WaitBlock->Lock));
    LockHeld = TRUE;
    WaitBlock->SignalingQueue = NULL;
    for (Index = 0; Index < Count; Index += 1) {

        //
        // Tweak the order a bit, adding all the non-timer objects first and
        // then the timer at the end. This optimizes for cases where a timed
        // wait is immediately satisfied.
        //

        if (Index == (Count - 1)) {
            WaitEntry = &(WaitBlock->Entry[0]);
            WaitEntry->Queue = NULL;
            if (TimeoutInMilliseconds == WAIT_TIME_INDEFINITE) {
                continue;
            }

            //
            // If the timeout is zero, return immediately, no need to even
            // queue the timer.
            //

            Queue = &(((POBJECT_HEADER)(Thread->BuiltinTimer))->WaitQueue);
            if (TimeoutInMilliseconds == 0) {
                Block = FALSE;
                WaitBlock->SignalingQueue = Queue;
                Status = STATUS_TIMEOUT;
                break;
            }

            DueTime = HlQueryTimeCounter() +
                      KeConvertMicrosecondsToTimeTicks(
                         TimeoutInMilliseconds * MICROSECONDS_PER_MILLISECOND);

            Status = KeQueueTimer(Thread->BuiltinTimer,
                                  TimerQueueSoftWake,
                                  DueTime,
                                  0,
                                  0,
                                  NULL);

            if (!KSUCCESS(Status)) {
                goto WaitEnd;
            }

            TimerQueued = TRUE;
            WaitEntry->Queue = Queue;

        } else {
            WaitEntry = &(WaitBlock->Entry[Index + 1]);
        }

        Queue = WaitEntry->Queue;

        ASSERT(Queue != NULL);
        ASSERT(WaitEntry->WaitListEntry.Next == NULL);

        WaitEntry->WaitBlock = WaitBlock;

        //
        // Add the wait entry onto the queue's waiters list. The normal rule
        // of locks is that they must be acquired in the same order, and one
        // might think this acquire here breaks the rule, since all other
        // acquires go Queue then WaitBlock. In this case however, since the
        // queue has not yet been added to the wait block, there is no
        // scenario in which other code could attempt to acquire Queue then
        // WaitBlock, as the wait block would need to be queued on the queue
        // for those paths to run. So this is safe with this caveat.
        //

        KeAcquireSpinLock(&(Queue->Lock));
        if (ObpWaitFast(Queue) == FALSE) {
            INSERT_BEFORE(&(WaitEntry->WaitListEntry), &(Queue->Waiters));

            //
            // The built-in timer does not count towards a wait-all attempt so
            // do not increment the unsignaled count for that queue.
            //

            if (Index != (Count - 1)) {
                WaitBlock->UnsignaledCount += 1;
            }

        //
        // If the queue has already been signaled, then determine if this setup
        // loop can exit early.
        //

        } else {

            //
            // If this is not the last queue, then setup can exit if not all
            // queues need to be waited on or if this is the second to last
            // queue and there are no unsignaled queues.
            //

            if (Index != (Count - 1)) {
                if (((WaitBlock->Flags & WAIT_FLAG_ALL) == 0) ||
                    ((Index == (Count - 2)) &&
                     (WaitBlock->UnsignaledCount == 0))) {

                    KeReleaseSpinLock(&(Queue->Lock));
                    WaitBlock->SignalingQueue = Queue;
                    Block = FALSE;
                    break;
                }

            //
            // Otherwise this is the last queue - the built-in timer - and it
            // expired. As the wait block lock is still held, none of the other
            // queues have had a chance to satisfy this wait, so count this as
            // a timeout. The loop should exit the next time around.
            //

            } else {
                Block = FALSE;
                WaitBlock->SignalingQueue = Queue;
                TimerQueued = FALSE;
                Status = STATUS_TIMEOUT;
            }
        }

        KeReleaseSpinLock(&(Queue->Lock));
    }

    //
    // Count the wait block as active, even if it won't block.
    //

    WaitBlock->Flags |= WAIT_BLOCK_FLAG_ACTIVE;

    //
    // If blocking, set the thread to wake so that if something is waiting to
    // acquire the wait block lock and wake the thread, it knows which thread
    // to wake.
    //

    if (Block != FALSE) {
        WaitBlock->Thread = Thread;
    }

    KeReleaseSpinLock(&(WaitBlock->Lock));
    LockHeld = FALSE;

    //
    // Block the thread if the wait condition was not satisfied above.
    //

    if (Block != FALSE) {

        ASSERT(Status == STATUS_SUCCESS);

        Thread->WaitBlock = WaitBlock;
        KeSchedulerEntry(SchedulerReasonThreadBlocking);
        Thread->WaitBlock = NULL;

        //
        // Check to see if this thread has resumed due to a signal.
        //

        if ((WaitBlock->Flags & WAIT_BLOCK_FLAG_INTERRUPTED) != 0) {
            Status = STATUS_INTERRUPTED;

        //
        // If it wasn't an interruption, then one of the objects actually
        // being waited on must have caused execution to resume.
        //

        } else {

            ASSERT(WaitBlock->SignalingQueue != NULL);

            if (WaitBlock->SignalingQueue ==
                &(((POBJECT_HEADER)(Thread->BuiltinTimer))->WaitQueue)) {

                Status = STATUS_TIMEOUT;
                TimerQueued = FALSE;
            }
        }

    //
    // Otherwise a queue signaled during initialization. Success better be on
    // horizon unless it was a timeout and the built-in timer signaled.
    //

    } else {

        ASSERT(WaitBlock->SignalingQueue != NULL);
        ASSERT(KSUCCESS(Status) ||
               ((Status == STATUS_TIMEOUT) &&
                (WaitBlock->SignalingQueue ==
                 &(((POBJECT_HEADER)(Thread->BuiltinTimer))->WaitQueue))));
    }

    if (TimerQueued != FALSE) {
        KeCancelTimer(Thread->BuiltinTimer);
    }

WaitEnd:
    if (LockHeld != FALSE) {
        KeReleaseSpinLock(&(WaitBlock->Lock));
    }

    //
    // Clean up the wait block. The index stores how many wait entries were
    // initialized above.
    //

    ObpCleanUpWaitBlock(WaitBlock, Index);
    KeLowerRunLevel(OldRunLevel);
    return Status;
}

KERNEL_API
KSTATUS
ObWaitOnQueue (
    PWAIT_QUEUE Queue,
    ULONG Flags,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine waits on a given wait queue. It is assumed that the caller can
    ensure externally that the wait queue will remain allocated.

Arguments:

    Queue - Supplies a pointer to the queue to wait on.

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See WAIT_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        queues should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever.

Return Value:

    Status code.

--*/

{

    PKTHREAD CurrentThread;
    PWAIT_BLOCK WaitBlock;

    //
    // Try a fast wait, which saves a whole bunch of effort if it works.
    //

    if (ObpWaitFast(Queue) != FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Slow path, really go wait on this thing.
    //

    CurrentThread = KeGetCurrentThread();
    WaitBlock = CurrentThread->BuiltinWaitBlock;
    WaitBlock->Count = 2;
    WaitBlock->Entry[1].Queue = Queue;
    WaitBlock->Flags = Flags;
    return ObWait(WaitBlock, TimeoutInMilliseconds);
}

KERNEL_API
KSTATUS
ObWaitOnObjects (
    PVOID *ObjectArray,
    ULONG ObjectCount,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PWAIT_BLOCK PreallocatedWaitBlock,
    PVOID *SignalingObject
    )

/*++

Routine Description:

    This routine waits on multiple objects until one (or all in some cases) is
    signaled. The caller is responsible for maintaining references to these
    objects.

Arguments:

    ObjectArray - Supplies an array of object pointers containing the objects
        to wait on. Each object must only be on the list once.

    ObjectCount - Supplies the number of elements in the array.

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See WAIT_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        objects should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on these objects.

    PreallocatedWaitBlock - Supplies an optional pointer to a pre-allocated
        wait block to use for the wait. This is optional, however if there are
        a large number of objects to wait on and this is a common operation, it
        is a little faster to pre-allocate a wait block and reuse it.

    SignalingObject - Supplies an optional pointer where the object that
        satisfied the wait will be returned on success. If the wait was
        interrupted, this returns NULL. If the WAIT_FLAG_ALL_OBJECTS flag was
        not specified, the first object to be signaled will be returned. If the
        WAIT_FLAG_ALL_OBJECTS was specified, the caller should not depend on
        a particular object being returned here.

Return Value:

    Status code.

--*/

{

    PVOID LocalSignalingObject;
    ULONG ObjectIndex;
    KSTATUS Status;
    PKTHREAD Thread;
    POBJECT_HEADER *TypedObjectArray;
    PWAIT_BLOCK WaitBlock;
    BOOL WaitBlockAllocated;

    LocalSignalingObject = NULL;
    TypedObjectArray = (POBJECT_HEADER *)ObjectArray;
    WaitBlockAllocated = FALSE;
    if (PreallocatedWaitBlock != NULL) {
        WaitBlock = PreallocatedWaitBlock;

    } else if (ObjectCount + 1 <= BUILTIN_WAIT_BLOCK_ENTRY_COUNT) {
        Thread = KeGetCurrentThread();
        WaitBlock = Thread->BuiltinWaitBlock;

    } else {
        WaitBlock = ObCreateWaitBlock(ObjectCount);
        if (WaitBlock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto WaitForObjectsEnd;
        }

        WaitBlockAllocated = TRUE;
    }

    ASSERT(ObjectCount + 1 <= WaitBlock->Capacity);

    for (ObjectIndex = 0; ObjectIndex < ObjectCount; ObjectIndex += 1) {

        ASSERT(WaitBlock->Entry[ObjectIndex + 1].WaitListEntry.Next == NULL);

        WaitBlock->Entry[ObjectIndex + 1].Queue =
                                   &(TypedObjectArray[ObjectIndex]->WaitQueue);
    }

    WaitBlock->Count = ObjectCount + 1;
    WaitBlock->Flags = Flags;
    Status = ObWait(WaitBlock, TimeoutInMilliseconds);
    LocalSignalingObject = WaitBlock->SignalingQueue;
    if (LocalSignalingObject != NULL) {
        LocalSignalingObject = PARENT_STRUCTURE(LocalSignalingObject,
                                                OBJECT_HEADER,
                                                WaitQueue);
    }

WaitForObjectsEnd:
    if (WaitBlockAllocated != FALSE) {
        ObDestroyWaitBlock(WaitBlock);
    }

    if (SignalingObject != NULL) {
        *SignalingObject = LocalSignalingObject;
    }

    return Status;
}

KERNEL_API
KSTATUS
ObWaitOnQueues (
    PWAIT_QUEUE *QueueArray,
    ULONG Count,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PWAIT_BLOCK PreallocatedWaitBlock,
    PWAIT_QUEUE *SignalingQueue
    )

/*++

Routine Description:

    This routine waits on multiple wait queues until one (or all in some cases)
    is signaled. The caller is responsible for ensuring externally that these
    wait queues will not somehow be deallocated over the course of the wait.

Arguments:

    QueueArray - Supplies an array of wait queue pointers containing the queues
        to wait on. Each queue must only be on the list once.

    Count - Supplies the number of elements in the array.

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See WAIT_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        queues should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever.

    PreallocatedWaitBlock - Supplies an optional pointer to a pre-allocated
        wait block to use for the wait. This is optional, however if there are
        a large number of queues to wait on and this is a common operation, it
        is a little faster to pre-allocate a wait block and reuse it.

    SignalingQueue - Supplies an optional pointer where the queue that
        satisfied the wait will be returned on success. If the wait was
        interrupted, this returns NULL. If the WAIT_FLAG_ALL_OBJECTS flag was
        not specified, the first queue to be signaled will be returned. If the
        WAIT_FLAG_ALL_OBJECTS was specified, the caller should not depend on
        a particular queue being returned here.

Return Value:

    Status code.

--*/

{

    PWAIT_QUEUE LocalSignalingQueue;
    ULONG ObjectIndex;
    KSTATUS Status;
    PKTHREAD Thread;
    PWAIT_BLOCK WaitBlock;
    BOOL WaitBlockAllocated;

    LocalSignalingQueue = NULL;
    WaitBlockAllocated = FALSE;
    if (PreallocatedWaitBlock != NULL) {
        WaitBlock = PreallocatedWaitBlock;

    } else if (Count + 1 <= BUILTIN_WAIT_BLOCK_ENTRY_COUNT) {
        Thread = KeGetCurrentThread();
        WaitBlock = Thread->BuiltinWaitBlock;

    } else {
        WaitBlock = ObCreateWaitBlock(Count);
        if (WaitBlock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto WaitOnQueuesEnd;
        }

        WaitBlockAllocated = TRUE;
    }

    ASSERT(Count + 1 <= WaitBlock->Capacity);

    for (ObjectIndex = 0; ObjectIndex < Count; ObjectIndex += 1) {

        ASSERT(WaitBlock->Entry[ObjectIndex].WaitListEntry.Next == NULL);

        WaitBlock->Entry[ObjectIndex + 1].Queue = QueueArray[ObjectIndex];
    }

    WaitBlock->Count = Count + 1;
    WaitBlock->Flags = Flags;
    Status = ObWait(WaitBlock, TimeoutInMilliseconds);
    LocalSignalingQueue = WaitBlock->SignalingQueue;

WaitOnQueuesEnd:
    if (WaitBlockAllocated != FALSE) {
        ObDestroyWaitBlock(WaitBlock);
    }

    if (SignalingQueue != NULL) {
        *SignalingQueue = LocalSignalingQueue;
    }

    return Status;
}

VOID
ObSignalQueue (
    PWAIT_QUEUE Queue,
    SIGNAL_OPTION Signal
    )

/*++

Routine Description:

    This routine signals (or unsignals) a wait queue, potentially releasing
    threads blocking on this object.

Arguments:

    Queue - Supplies a pointer to the wait queue to signal.

    Signal - Supplies the type of signaling to provide on the queue. Valid
        values are:

        SignalOptionSignalAll - Sets the queue to a signaled state and leaves
            it that way. All threads waiting on this queue will continue.

        SignalOptionSignalOne - Wakes up one thread waiting on the queue.
            If no threads are waiting on the queue, the state will be
            signaled until one thread waits on the queue, at which point it
            will go back to being unsignaled.

        SignalOptionPulse - Satisfies all waiters currently waiting on the
            queue, but does not set the state to signaled.

        SignalOptionUnsignal - Sets the queue's state to unsignaled.

Return Value:

    None.

--*/

{

    PLIST_ENTRY Entry;
    BOOL LockHeld;
    RUNLEVEL OldRunLevel;
    SIGNAL_STATE OldState;
    THREAD_STATE OldThreadState;
    SIGNAL_STATE PreviousState;
    LIST_ENTRY ReleaseList;
    PKTHREAD Thread;
    BOOL ThreadWoken;
    PWAIT_BLOCK WaitBlock;
    PWAIT_BLOCK_ENTRY WaitEntry;

    //
    // Signaling for one is the tricky bit. Try to set the state to signaled
    // for one, unless there are already threads waiting on it. In that case,
    // leave it alone for now, as the lock will need be acquired to figure out
    // what the next state is.
    //

    if (Signal == SignalOptionSignalOne) {
        OldState = NotSignaled;
        do {
            PreviousState = OldState;
            OldState = RtlAtomicCompareExchange32(&(Queue->State),
                                                  SignaledForOne,
                                                  OldState);

            if (OldState == PreviousState) {
                break;
            }

        } while ((OldState != NotSignaledWithWaiters) &&
                 (OldState != SignaledForOne));

    //
    // To unsignal, the goal is to get it to the not signaled state, but not
    // clobber if it's unsignaled with waiters.
    //

    } else if (Signal == SignalOptionUnsignal) {
        OldState = Signaled;
        do {
            PreviousState = OldState;
            OldState = RtlAtomicCompareExchange32(&(Queue->State),
                                                  NotSignaled,
                                                  OldState);

            if (OldState == PreviousState) {
                break;
            }

        } while ((OldState != NotSignaledWithWaiters) &&
                 (OldState != NotSignaled));

        return;

    //
    // Pulsing does not change the state, it only releases anyone currently
    // waiting.
    //

    } else if (Signal == SignalOptionPulse) {
        OldState = RtlAtomicOr32(&(Queue->State), 0);

    //
    // Signaling for all just exchanges the new value in, there's no need to
    // be timid.
    //

    } else {

        ASSERT(Signal == SignalOptionSignalAll);

        OldState = RtlAtomicExchange32(&(Queue->State), Signaled);
    }

    //
    // If there are no threads to be released, then rejoice for the fast path.
    //

    if (OldState != NotSignaledWithWaiters) {
        return;
    }

    //
    // Heavy times, raise to dispatch and acquire the lock to potentially
    // release threads.
    //

    INITIALIZE_LIST_HEAD(&ReleaseList);
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Queue->Lock));
    LockHeld = TRUE;

    //
    // Loop attempting to run the waiters. If only signalling for one, the
    // first selected waiter may not be waiting by the time it's wait block
    // gets inspected. As a result, this routine must loop around to adjust the
    // queue's state or to pick another waiter.
    //

    while (TRUE) {

        //
        // Attempt to pull one waiter off of the list or to set the correct
        // state if there are no more waiters.
        //

        if (Signal == SignalOptionSignalOne) {

            //
            // If there are no more waiters, then try to set the signal state
            // to signaled for one. This may race with another entity signaling
            // it (for all or one). It's fine if it loses in those cases. But
            // it may also be racing with a waking thread trying to change the
            // state from not signaled with waiters to not signaled. It is not
            // OK to lose in that case. Try again until it's in some signaled
            // state.
            //

            if (LIST_EMPTY(&(Queue->Waiters)) != FALSE) {
                do {
                    PreviousState = OldState;
                    OldState = RtlAtomicCompareExchange32(&(Queue->State),
                                                          SignaledForOne,
                                                          OldState);

                    if (OldState == PreviousState) {
                        break;
                    }

                } while ((OldState == NotSignaledWithWaiters) ||
                         (OldState == NotSignaled));

                //
                // There is no work to be done if there is nothing to wake.
                // Break out of the loop.
                //

                break;

            //
            // Otherwise, rip an entry off the wait list. There is no guarantee
            // that this queue will actually wake the entry. The entry could be
            // in the process of being woken up by a different queue.
            //

            } else {
                Entry = Queue->Waiters.Next;
                LIST_REMOVE(Entry);
                INSERT_BEFORE(Entry, &ReleaseList);
            }

        //
        // Everybody gets to run.
        //

        } else {

            ASSERT((Signal == SignalOptionSignalAll) ||
                   (Signal == SignalOptionPulse));

            if (LIST_EMPTY(&(Queue->Waiters)) == FALSE) {
                MOVE_LIST(&(Queue->Waiters), &ReleaseList);
                INITIALIZE_LIST_HEAD(&(Queue->Waiters));
            }
        }

        //
        // Update the wait blocks of every item on the local list. The queue's
        // lock must still be held to protect the local list's use of each wait
        // block entry's list entry. However, as soon as the last thread has
        // been set ready, the queue may be destroyed. So, drop the queue's
        // lock when the list is found to be empty, but before setting the
        // thread ready. If the last wait block has already been satisfied by
        // another queue, that wait block's thread will not destroy this queue
        // until it has acquired this queue's lock to remove the wait block
        // entry off this local list.
        //

        ThreadWoken = FALSE;
        while (LIST_EMPTY(&ReleaseList) == FALSE) {
            Entry = ReleaseList.Next;
            LIST_REMOVE(Entry);
            WaitEntry = LIST_VALUE(Entry, WAIT_BLOCK_ENTRY, WaitListEntry);
            WaitBlock = WaitEntry->WaitBlock;
            KeAcquireSpinLock(&(WaitBlock->Lock));

            //
            // After setting the next entry to NULL, the hold on the lock is
            // the only thing keeping the wait block from getting released or
            // reused.
            //

            WaitEntry->WaitListEntry.Next = NULL;

            ASSERT(WaitEntry->Queue == Queue);

            //
            // The built-in timer does not count towards signaling all.
            //

            if (WaitEntry != &(WaitBlock->Entry[0])) {

                ASSERT(WaitBlock->UnsignaledCount != 0);

                WaitBlock->UnsignaledCount -= 1;
            }

            //
            // Determine if the signaling of this queue satisfies the wait
            // block. The wait block must still have its thread set, indicating
            // that no other queue has satisfied the wait and that it has not
            // been interrrupted. If this is the built-in timer, then the wait
            // is satisfied without a need to check the unsignaled count.
            // Finally either all queues must have signaled or the wait block
            // is just waiting for the first queue to do so.
            //

            Thread = NULL;
            if ((WaitBlock->Thread != NULL) &&
                ((WaitEntry == &(WaitBlock->Entry[0])) ||
                 ((WaitBlock->Flags & WAIT_FLAG_ALL) == 0) ||
                 (WaitBlock->UnsignaledCount == 0))) {

                WaitBlock->SignalingQueue = Queue;
                Thread = WaitBlock->Thread;
                WaitBlock->Thread = NULL;
            }

            KeReleaseSpinLock(&(WaitBlock->Lock));

            //
            // If the wait was satisfied, as indicated by the local thread
            // being set, fire off the thread.
            //

            if (Thread != NULL) {

                //
                // If the local list is now empty, release the queue lock
                // before letting the last thread go and do not touch the queue
                // again.
                //

                if (LIST_EMPTY(&ReleaseList) != FALSE) {

                    //
                    // If signaling for one and there are no more waiters, try
                    // to transition the state from not signaled with waiters
                    // to not signaled. This may race with an attempt to signal
                    // all. It's OK to lose in that case.
                    //

                    if ((Signal == SignalOptionSignalOne) &&
                        (LIST_EMPTY(&(Queue->Waiters)) != FALSE)) {

                        RtlAtomicCompareExchange32(&(Queue->State),
                                                   NotSignaled,
                                                   OldState);
                    }

                    KeReleaseSpinLock(&(Queue->Lock));
                    LockHeld = FALSE;
                }

                //
                // This must wait until it can transition the thread into the
                // waking state as it might be competing with attempts to
                // signal the thread.
                //

                while (TRUE) {
                    while (Thread->State != ThreadStateBlocked) {
                        ArProcessorYield();
                    }

                    OldThreadState = RtlAtomicCompareExchange32(
                                                           &(Thread->State),
                                                           ThreadStateWaking,
                                                           ThreadStateBlocked);

                    if (OldThreadState == ThreadStateBlocked) {
                        KeSetThreadReady(Thread);
                        break;
                    }
                }

                ThreadWoken = TRUE;
            }
        }

        //
        // Signal all or pulse attempts are done after the first loop. The list
        // of waiters was emptied.
        //

        if (Signal != SignalOptionSignalOne) {
            break;
        }

        //
        // In the signal for one case, the thread of the selected wait block
        // may not have actually been woken. In that case, this routine needs
        // to try to wake another waiter. Otherwise, exit the loop.
        //

        if (ThreadWoken != FALSE) {
            break;
        }
    }

    if (LockHeld != FALSE) {
        KeReleaseSpinLock(&(Queue->Lock));
    }

    KeLowerRunLevel(OldRunLevel);
    return;
}

BOOL
ObWakeBlockedThread (
    PVOID ThreadToWake,
    BOOL OnlyWakeSuspendedThreads
    )

/*++

Routine Description:

    This routine wakes up a blocked or suspended thread, interrupting any wait
    it may have been performing. If the thread was not blocked or suspended or
    the wait is not interruptible, then this routine does nothing.

Arguments:

    ThreadToWake - Supplies a pointer to the thread to poke.

    OnlyWakeSuspendedThreads - Supplies a boolean indicating that the thread
        should only be poked if it's in the suspended state (not if it's in
        the blocked state).

Return Value:

    TRUE if the thread was actually pulled out of a blocked or suspended state.

    FALSE if no action was performed because the thread was not stopped.

--*/

{

    RUNLEVEL OldRunLevel;
    THREAD_STATE OldThreadState;
    PKTHREAD Thread;
    THREAD_STATE ThreadState;
    PWAIT_BLOCK WaitBlock;
    BOOL WakeThread;

    WakeThread = FALSE;
    Thread = (PKTHREAD)ThreadToWake;

    //
    // Make sure the thread moves out of one of the transitioning states
    // before attempting to wake it. The state will be checked again below
    // in case it moved to a state other than blocked or suspended.
    //

    while (TRUE) {
        ThreadState = Thread->State;
        if ((ThreadState != ThreadStateSuspending) &&
            ((OnlyWakeSuspendedThreads != FALSE) ||
             (ThreadState != ThreadStateBlocking))) {

            break;
        }

        ArProcessorYield();
    }

    //
    // Now that the thread is out of the transitioning states, figure out if
    // it can be awoken.
    //

    if ((OnlyWakeSuspendedThreads == FALSE) &&
        (Thread->State == ThreadStateBlocked)) {

        //
        // Attempt to win the race to set the thread as waking. This needs to
        // be done at dispatch. A context switch after this thread wins could
        // result in another thread running. That thread may win the race to
        // release the wait block, at which point it will start spinning on the
        // blocked thread's state at dispatch. This would lock down a single
        // core.
        //

        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        OldThreadState = RtlAtomicCompareExchange32(&(Thread->State),
                                                    ThreadStateWaking,
                                                    ThreadStateBlocked);

        if (OldThreadState == ThreadStateBlocked) {

            ASSERT(Thread->WaitBlock != NULL);

            WaitBlock = Thread->WaitBlock;

            //
            // If the wait block is interruptible, then try to wake the thread.
            //

            if ((WaitBlock->Thread != NULL) &&
                ((WaitBlock->Flags & WAIT_FLAG_INTERRUPTIBLE) != 0)) {

                KeAcquireSpinLock(&(WaitBlock->Lock));
                if (WaitBlock->Thread != NULL) {
                    WaitBlock->Flags |= WAIT_BLOCK_FLAG_INTERRUPTED;
                    WaitBlock->Thread = NULL;
                    WakeThread = TRUE;
                }

                KeReleaseSpinLock(&(WaitBlock->Lock));
            }

            //
            // If this call did not win the race to wake the thread, then the
            // state is incorrectly marked waking. Set it back to being
            // blocked.
            //

            if (WakeThread == FALSE) {
                Thread->State = ThreadStateBlocked;
            }
        }

        KeLowerRunLevel(OldRunLevel);

    //
    // If the thread's just in limbo, it's not blocked on anything, so just
    // set it to ready.
    //

    } else if (Thread->State == ThreadStateSuspended) {

        ASSERT(Thread->WaitBlock == NULL);

        OldThreadState = RtlAtomicCompareExchange32(&(Thread->State),
                                                    ThreadStateWaking,
                                                    ThreadStateSuspended);

        if (OldThreadState == ThreadStateSuspended) {
            WakeThread = TRUE;
        }
    }

    //
    // Send the thread off it it needs to be woken.
    //

    if (WakeThread != FALSE) {
        KeSetThreadReady(Thread);
    }

    return WakeThread;
}

BOOL
ObWakeBlockingThread (
    PVOID ThreadToWake
    )

/*++

Routine Description:

    This routine wakes up a blocking or suspending thread, interrupting any
    wait it may have been performing. This routine assumes that the thread is
    either blocking or suspending. It should not be in another state.

Arguments:

    ThreadToWake - Supplies a pointer to the thread to poke.

Return Value:

    TRUE if the thread was actually pulled out of a blocking or suspending
    state.

    FALSE if no action was performed because the thread had already been awoken.

--*/

{

    PKTHREAD Thread;
    PWAIT_BLOCK WaitBlock;
    BOOL WakeThread;

    ASSERT((KeGetRunLevel() == RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    WakeThread = FALSE;
    Thread = (PKTHREAD)ThreadToWake;

    //
    // If the thread is blocking, test to see if the wait block is
    // interruptible and not satisfied. This is necessary in case a queue has
    // already signaled the wait block and is now waiting for the thread to
    // transition into the blocked state.
    //

    if (Thread->State == ThreadStateBlocking) {

        ASSERT(Thread->WaitBlock != NULL);

        WaitBlock = Thread->WaitBlock;
        if ((WaitBlock->Thread != NULL) &&
            ((WaitBlock->Flags & WAIT_FLAG_INTERRUPTIBLE) != 0)) {

            KeAcquireSpinLock(&(WaitBlock->Lock));
            if (WaitBlock->Thread != NULL) {
                WaitBlock->Flags |= WAIT_BLOCK_FLAG_INTERRUPTED;
                WaitBlock->Thread = NULL;
                WakeThread = TRUE;
            }

            KeReleaseSpinLock(&(WaitBlock->Lock));
        }

    //
    // Suspending threads always get set to ready. There are no races.
    //

    } else {

        ASSERT(Thread->State == ThreadStateSuspending);
        ASSERT(Thread->WaitBlock == NULL);

        WakeThread = TRUE;
    }

    if (WakeThread != FALSE) {
        Thread->State = ThreadStateWaking;
        KeSetThreadReady(Thread);
    }

    return WakeThread;
}

PVOID
ObFindObject (
    PCSTR ObjectName,
    ULONG BufferLength,
    POBJECT_HEADER ParentObject
    )

/*++

Routine Description:

    This routine locates an object by name. The found object will be returned
    with an incremented reference count as to ensure it does not disappear
    while the caller is handling it. It is the caller's responsibility to
    release this reference.

Arguments:

    ObjectName - Supplies the name of the object to find, either
        fully-specified or relative. Fully specified names begin with a /.

    BufferLength - Supplies the length of the name buffer.

    ParentObject - Supplies the parent object for relative names.

Return Value:

    Returns a pointer to the object with the specified name, or NULL if the
    object could not be found.

--*/

{

    POBJECT_HEADER CurrentChild;
    PLIST_ENTRY CurrentEntry;
    POBJECT_HEADER CurrentRoot;
    ULONG ElementLength;
    BOOL LockHeld;
    BOOL Match;
    PCSTR Name;
    PCSTR NextDelimiter;
    ULONG OldReferenceCount;
    RUNLEVEL OldRunLevel;

    LockHeld = FALSE;
    Match = FALSE;

    ASSERT((ObjectName != NULL) && (BufferLength != 0));

    Name = ObjectName;
    CurrentRoot = ParentObject;
    if (*Name == OBJECT_PATH_SEPARATOR) {
        CurrentRoot = ObRootObject;
        Name += 1;
        BufferLength -= 1;
    }

    if (CurrentRoot == NULL) {
        return NULL;
    }

    //
    // Loop until the object is found or not.
    //

    ObAddReference(CurrentRoot);
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(CurrentRoot->WaitQueue.Lock));
    LockHeld = TRUE;
    while (TRUE) {
        if ((*Name == '\0') || (BufferLength == 0)) {
            break;
        }

        //
        // Find the next separator.
        //

        NextDelimiter = RtlStringFindCharacter(Name,
                                               OBJECT_PATH_SEPARATOR,
                                               BufferLength);

        if (NextDelimiter != NULL) {
            ElementLength = (UINTN)NextDelimiter - (UINTN)Name + 1;

        } else {
            ElementLength = BufferLength;
        }

        //
        // Loop through all child objects.
        //

        Match = FALSE;
        CurrentChild = NULL;
        CurrentEntry = CurrentRoot->ChildListHead.Next;
        while (CurrentEntry != &(CurrentRoot->ChildListHead)) {
            CurrentChild = LIST_VALUE(CurrentEntry,
                                      OBJECT_HEADER,
                                      SiblingEntry);

            CurrentEntry = CurrentEntry->Next;
            if (CurrentChild->Name != NULL) {
                Match = ObpAreObjectNamesEqual(CurrentChild->Name,
                                               Name,
                                               ElementLength);
            }

            //
            // Found a match for this piece of the name. Attempt to grab a
            // reference.
            //

            if (Match != FALSE) {
                OldReferenceCount =
                            RtlAtomicAdd32(&(CurrentChild->ReferenceCount), 1);

                //
                // If the object reference was at 0, then another thread is
                // spinning on the parent spinlock trying to remove it from the
                // list. Simply pretend it was never seen by decrementing the
                // reference count. No other threads can be operating on the
                // reference count.
                //

                if (OldReferenceCount == 0) {
                    RtlAtomicAdd32(&(CurrentChild->ReferenceCount), (ULONG)-1);
                    Match = FALSE;
                }

                //
                // Exit the loop either way. A parent shouldn't have two
                // children with the same name.
                //

                break;
            }
        }

        //
        // If an object was not found, error out now. If one was found, loop
        // unless this was the last object in the path.
        //

        if (Match == FALSE) {
            goto FindObjectEnd;
        }

        ASSERT(CurrentChild != NULL);
        ASSERT(BufferLength >= ElementLength);

        //
        // Release the current object's lock and acquire the child's lock.
        // The child object will definitely not disappear during this
        // sequence because the reference count was incremented above (and
        // verified not to be zero at the time).
        //

        KeReleaseSpinLock(&(CurrentRoot->WaitQueue.Lock));
        ObReleaseReference(CurrentRoot);
        CurrentRoot = CurrentChild;
        KeAcquireSpinLock(&(CurrentRoot->WaitQueue.Lock));
        BufferLength -= ElementLength;
        Name += ElementLength;
        if (NextDelimiter == NULL) {

            ASSERT(BufferLength == 0);

            goto FindObjectEnd;
        }
    }

FindObjectEnd:
    if (LockHeld != FALSE) {
        KeReleaseSpinLock(&(CurrentRoot->WaitQueue.Lock));
    }

    KeLowerRunLevel(OldRunLevel);
    if (Match != FALSE) {
        return CurrentRoot;

    } else {
        ObReleaseReference(CurrentRoot);
    }

    return NULL;
}

PSTR
ObGetFullPath (
    PVOID Object,
    ULONG AllocationTag
    )

/*++

Routine Description:

    This routine returns the full path of the given object. The return value
    will be allocated from paged pool, and it is the caller's responsibility
    to free it. The object path must not have any unnamed objects anywhere in
    its parent chain.

Arguments:

    Object - Supplies a pointer to the object whose path to return.

    AllocationTag - Supplies the allocation tag to use when allocating memory
        for the return value.

Return Value:

    Returns a pointer to the full path of the object, allocated from paged
    pool. It is the caller's responsibility to free this memory.

    NULL on failure, usually a memory allocation failure or an unnamed object
    somewhere in the path.

--*/

{

    ULONG AllocationSize;
    POBJECT_HEADER CurrentObject;
    ULONG CurrentObjectLength;
    PSTR CurrentPath;
    BOOL FirstObject;
    PSTR FullPath;
    BOOL LockHeld;

    FullPath = NULL;
    LockHeld = FALSE;
    if (Object == NULL) {
        Object = ObRootObject;
    }

    //
    // Loop up to the parent once to determine the size of the string to
    // allocate. Initially allocate space for the beginning separator and the
    // null terminator.
    //

    AllocationSize = 2;
    CurrentObject = (POBJECT_HEADER)Object;
    FirstObject = TRUE;
    while (CurrentObject != ObRootObject) {
        KeAcquireSpinLock(&(CurrentObject->Parent->WaitQueue.Lock));
        LockHeld = TRUE;

        //
        // The object type is invalid. This is either corruption in the tree
        // or a garbage initial parameter.
        //

        if ((CurrentObject->Type == ObjectInvalid) ||
            (CurrentObject->Type >= ObjectMaxTypes)) {

            ASSERT(FALSE);

            goto GetFullPathEnd;
        }

        //
        // Unnamed objects cannot have a full path.
        //

        if (CurrentObject->Name == NULL) {
            goto GetFullPathEnd;
        }

        CurrentObjectLength = CurrentObject->NameLength;
        if (CurrentObjectLength == 0) {
            goto GetFullPathEnd;
        }

        //
        // An element in the path needs a path separator for the child (unless
        // this is the end of the path).
        //

        AllocationSize += CurrentObjectLength - 1;
        if (FirstObject != FALSE) {
            FirstObject = FALSE;

        } else {
            AllocationSize += 1;
        }

        CurrentObject = CurrentObject->Parent;
        KeReleaseSpinLock(&(CurrentObject->WaitQueue.Lock));
        LockHeld = FALSE;
    }

    //
    // Allocate space for the full path. Use the caller supplied tag because
    // it's really the caller's responsibility not to let this memory leak.
    //

    FullPath = MmAllocatePagedPool(AllocationSize, AllocationTag);
    if (FullPath == NULL) {
        return NULL;
    }

    //
    // Add the leading separator, and a terminator just in case this is the root
    // object.
    //

    *FullPath = PATH_SEPARATOR;
    *(FullPath + 1) = '\0';

    //
    // Iterate up the tree again, creating the string backwards.
    //

    CurrentPath = FullPath + AllocationSize;
    CurrentObject = (POBJECT_HEADER)Object;
    FirstObject = TRUE;
    while (CurrentObject != ObRootObject) {
        KeAcquireSpinLock(&(CurrentObject->Parent->WaitQueue.Lock));
        LockHeld = TRUE;

        ASSERT(CurrentObject->Name != NULL);
        ASSERT(CurrentObject->NameLength != 0);

        CurrentObjectLength = CurrentObject->NameLength - 1;
        CurrentPath = (PSTR)((UINTN)CurrentPath - CurrentObjectLength - 1);

        //
        // Check for overflows and underflows.
        //

        if (((UINTN)CurrentPath < (UINTN)FullPath) ||
            (CurrentPath + CurrentObjectLength >= FullPath + AllocationSize)) {

            MmFreePagedPool(FullPath);
            FullPath = NULL;
            goto GetFullPathEnd;
        }

        //
        // Copy the name of the object in.
        //

        RtlCopyMemory(CurrentPath, CurrentObject->Name, CurrentObjectLength);
        if (FirstObject != FALSE) {
            CurrentPath[CurrentObjectLength] = STRING_TERMINATOR;
            FirstObject = FALSE;

        } else {
            CurrentPath[CurrentObjectLength] = PATH_SEPARATOR;
        }

        CurrentObject = CurrentObject->Parent;
        KeReleaseSpinLock(&(CurrentObject->WaitQueue.Lock));
        LockHeld = FALSE;
    }

GetFullPathEnd:
    if (LockHeld != FALSE) {
        KeReleaseSpinLock(&(CurrentObject->Parent->WaitQueue.Lock));
    }

    return FullPath;
}

PWAIT_QUEUE
ObGetBlockingQueue (
    PVOID Thread
    )

/*++

Routine Description:

    This routine returns one of the wait queues the given thread is blocking on.
    The caller is not guaranteed that the queue returned has a reference on it
    and won't disappear before being accessed. Generally this routine is only
    used by the scheduler for profiling.

Arguments:

    Thread - Supplies a pointer to the thread.

Return Value:

    Returns a pointer to the first wait queue the thread is blocking on, if any.
    The built-in thread timer does not count.

    NULL if the thread's wait block is empty.

--*/

{

    PKTHREAD TypedThread;

    TypedThread = (PKTHREAD)Thread;
    return TypedThread->WaitBlock->Entry[1].Queue;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ObpWaitFast (
    PWAIT_QUEUE WaitQueue
    )

/*++

Routine Description:

    This routine attempts to determine atomically whether or not to block for
    an object, and commits to blocking if needed.

Arguments:

    WaitQueue - Supplies a pointer to the queue to wait on.

Return Value:

    TRUE if the wait succeeded and the caller does not need to block for this
    wait.

    FALSE if the wait failed and the caller must block.

--*/

{

    SIGNAL_STATE State;

    //
    // The object's signal state needs to be atomically read. To avoid an extra
    // atomic read, start by guessing the state is signaled for one. This will
    // be true in the common case of acquiring an unheld queued lock.
    //

    State = SignaledForOne;

    //
    // Loop until it's not ambiguous.
    //

    while (TRUE) {

        //
        // If it's signaled for one, try to win and be that lucky one.
        //

        if (State == SignaledForOne) {
            State = RtlAtomicCompareExchange32(&(WaitQueue->State),
                                               NotSignaled,
                                               State);

            if (State == SignaledForOne) {
                return TRUE;
            }
        }

        //
        // If the state is signaled for everyone, then it definitely passes.
        //

        if (State == Signaled) {
            return TRUE;

        //
        // If this is not the first thread to the rodeo, then it definitely
        // fails.
        //

        } else if (State == NotSignaledWithWaiters) {
            return FALSE;

        //
        // If the state is not signaled, then attempt to change it to not
        // signaled with waiters. If that wins, then this thread must block.
        //

        } else if (State == NotSignaled) {
            State = RtlAtomicCompareExchange32(&(WaitQueue->State),
                                               NotSignaledWithWaiters,
                                               State);

            if (State == NotSignaled) {
                return FALSE;
            }

        //
        // Invalid state. Break out of the loop and assert.
        //

        } else {
            break;
        }
    }

    //
    // Bad recent object manager changes, or memory corruption.
    //

    ASSERT(FALSE);

    return FALSE;
}

VOID
ObpCleanUpWaitBlock (
    PWAIT_BLOCK WaitBlock,
    ULONG InitializedCount
    )

/*++

Routine Description:

    This routine removes the wait entries on the given wait block from any
    objects they may be queued on.

Arguments:

    WaitBlock - Supplies a pointer to the wait block to clean up.

    InitializedCount - Supplies the number of wait block entries that were
        initialized. If the wait block never blocked, this may be less than the
        count stored in the wait block.

Return Value:

    None.

--*/

{

    ULONG Index;
    BOOL MissedEntry;
    PWAIT_QUEUE Queue;
    PWAIT_BLOCK_ENTRY WaitEntry;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);
    ASSERT(WaitBlock->Thread == NULL);
    ASSERT(InitializedCount <= WaitBlock->Count);

    //
    // Keep track of wait block entries that were not found on their queue's
    // list. If such a wait block entry is found and the wait block's lock is
    // not subsequently acquired, then the wait block entry may still be using
    // the wait block.
    //

    MissedEntry = FALSE;

    //
    // This loop follows the same pattern as the wait block initialization loop
    // in that the first entry, the built-in timer, is handled last. The
    // supplied count indicates how many wait entries were initialized, meaning
    // that if it is less than the wait block's count then the built-in timer
    // was not initialized.
    //

    for (Index = 0; Index < InitializedCount; Index += 1) {
        if (Index == (WaitBlock->Count - 1)) {
            WaitEntry = &(WaitBlock->Entry[0]);

        } else {
            WaitEntry = &(WaitBlock->Entry[Index + 1]);
        }

        //
        // If the wait entry is still on its queue's list, acquire the locks
        // and make sure it is removed.
        //

        Queue = WaitEntry->Queue;
        if (WaitEntry->WaitListEntry.Next != NULL) {
            KeAcquireSpinLock(&(Queue->Lock));
            KeAcquireSpinLock(&(WaitBlock->Lock));

            //
            // If the entry is still on a list, remove it. This check is
            // necessary because in between the time the object was snapped and
            // when the lock was acquired, the object may have been signaled.
            //

            if (WaitEntry->WaitListEntry.Next != NULL) {

                ASSERT(WaitEntry->Queue == Queue);

                LIST_REMOVE(&(WaitEntry->WaitListEntry));
                WaitEntry->WaitListEntry.Next = NULL;

                //
                // The built-in timer does not count towards signalling all.
                //

                if (WaitEntry != &(WaitBlock->Entry[0])) {

                    ASSERT(WaitBlock->UnsignaledCount != 0);

                    WaitBlock->UnsignaledCount -= 1;
                }

                //
                // If that emptied the wait list for the object, try to
                // change the state from not signaled with waiters to just not
                // signaled. The one interesting case is if a thread has set it
                // to not signaled with waiters but not yet blocked. In that
                // path the thread blocking will call the wait routine again
                // in the slow path (with this lock held), restoring it to
                // not signaled with waiters.
                //

                if (LIST_EMPTY(&(Queue->Waiters)) != FALSE) {
                    RtlAtomicCompareExchange32(&(Queue->State),
                                               NotSignaled,
                                               NotSignaledWithWaiters);
                }
            }

            KeReleaseSpinLock(&(WaitBlock->Lock));
            KeReleaseSpinLock(&(Queue->Lock));
            MissedEntry = FALSE;

        //
        // Otherwise the wait entry is off of its queue's list, but may still
        // be using the wait block. Record that it was missed so that the wait
        // block lock can be acquired at the end to flush it out. The wait
        // entry for the signaling queue does not count as missed because it
        // was definitely done with the wait block when it woke the thread.
        //

        } else if ((Queue != NULL) &&
                   (Queue != WaitBlock->SignalingQueue)) {

            MissedEntry = TRUE;
        }
    }

    //
    // If a non-signaling wait entry was not found on its queue's list, then
    // acquire the wait block lock to ensure that it makes its way out.
    //

    if (MissedEntry != FALSE) {
        KeAcquireSpinLock(&(WaitBlock->Lock));
        KeReleaseSpinLock(&(WaitBlock->Lock));
    }

    ASSERT(WaitBlock->UnsignaledCount == 0);

    WaitBlock->Count = 0;
    WaitBlock->Entry[0].Queue = NULL;
    WaitBlock->Flags &= ~WAIT_BLOCK_FLAG_ACTIVE;
    return;
}

BOOL
ObpAreObjectNamesEqual (
    PCSTR ExistingObject,
    PCSTR QueryObject,
    ULONG QuerySize
    )

/*++

Routine Description:

    This routine compares two object components.

Arguments:

    ExistingObject - Supplies a pointer to the existing null terminated object
        name string.

    QueryObject - Supplies a pointer the query string, which may not be null
        terminated.

    QuerySize - Supplies the size of the string including the assumed
        null terminator that is never checked.

Return Value:

    TRUE if the object names are equals.

    FALSE if the object names differ in some way.

--*/

{

    BOOL Result;

    ASSERT(QuerySize != 0);

    Result = RtlAreStringsEqual(ExistingObject, QueryObject, QuerySize - 1);
    if (Result == FALSE) {
        return FALSE;
    }

    if (ExistingObject[QuerySize - 1] != STRING_TERMINATOR) {
        return FALSE;
    }

    return TRUE;
}

