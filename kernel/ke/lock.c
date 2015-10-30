/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    lock.c

Abstract:

    This module implements common synchronization primitives in the kernel.

Author:

    Evan Green 6-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>

//
// ---------------------------------------------------------------- Definitions
//

#define QUEUED_LOCK_TAG 0x6C51654B // 'lQeK'
#define SHARED_EXCLUSIVE_LOCK_TAG 0x6553654B // 'eSeK'

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
// Queued lock directory where all queued locks are stored. This is primarily
// done to keep the root directory tidy.
//

POBJECT_HEADER KeQueuedLockDirectory = NULL;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PQUEUED_LOCK
KeCreateQueuedLock (
    VOID
    )

/*++

Routine Description:

    This routine creates a new queued lock under the current thread. These locks
    can be used at up to dispatch level if non-paged memory is used.

Arguments:

    None.

Return Value:

    Returns a pointer to the new lock on success.

    NULL on failure.

--*/

{

    PQUEUED_LOCK NewLock;
    POBJECT_HEADER NewObject;

    NewObject = ObCreateObject(ObjectQueuedLock,
                               KeQueuedLockDirectory,
                               NULL,
                               0,
                               sizeof(QUEUED_LOCK),
                               NULL,
                               0,
                               QUEUED_LOCK_TAG);

    NewLock = (PQUEUED_LOCK)NewObject;
    if (NewLock != NULL) {

        //
        // Initialize the lock to signal one thread so the first wait acquires
        // it.
        //

        ObSignalObject(NewObject, SignalOptionSignalOne);
    }

    return NewLock;
}

KERNEL_API
VOID
KeDestroyQueuedLock (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine destroys a queued lock by decrementing its reference count.

Arguments:

    Lock - Supplies a pointer to the queued lock to destroy.

Return Value:

    None. When the function returns, the lock must not be used again.

--*/

{

    ObReleaseReference(&(Lock->Header));
    return;
}

KERNEL_API
VOID
KeAcquireQueuedLock (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine acquires the queued lock. If the lock is held, the thread
    blocks until it becomes available.

Arguments:

    Lock - Supplies a pointer to the queued lock to acquire.

Return Value:

    None. When the function returns, the lock will be held.

--*/

{

    KSTATUS Status;

    Status = KeAcquireQueuedLockTimed(Lock, WAIT_TIME_INDEFINITE);

    ASSERT(KSUCCESS(Status));

    return;
}

KERNEL_API
KSTATUS
KeAcquireQueuedLockTimed (
    PQUEUED_LOCK Lock,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine acquires the queued lock. If the lock is held, the thread
    blocks until it becomes available or the specified timeout expires.

Arguments:

    Lock - Supplies a pointer to the queued lock to acquire.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        object should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on the object.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the specified amount of time expired and the lock could
    not be acquired.

--*/

{

    KSTATUS Status;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);
    ASSERT((Lock->OwningThread != Thread) || (Thread == NULL));

    Status = ObWaitOnObject(&(Lock->Header), 0, TimeoutInMilliseconds);
    if (KSUCCESS(Status)) {
        Lock->OwningThread = Thread;
    }

    return Status;
}

KERNEL_API
VOID
KeReleaseQueuedLock (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine releases a queued lock that has been previously acquired.

Arguments:

    Lock - Supplies a pointer to the queued lock to release.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    Lock->OwningThread = NULL;
    ObSignalObject(&(Lock->Header), SignalOptionSignalOne);
    return;
}

KERNEL_API
BOOL
KeTryToAcquireQueuedLock (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine attempts to acquire the queued lock. If the lock is busy, it
    does not add this thread to the queue of waiters.

Arguments:

    Lock - Supplies a pointer to a queued lock.

Return Value:

    Returns TRUE if the lock was acquired, or FALSE otherwise.

--*/

{

    KSTATUS Status;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    Status = ObWaitOnObject(&(Lock->Header), 0, 0);
    if (!KSUCCESS(Status)) {
        return FALSE;
    }

    Lock->OwningThread = KeGetCurrentThread();
    return TRUE;
}

KERNEL_API
BOOL
KeIsQueuedLockHeld (
    PQUEUED_LOCK Lock
    )

/*++

Routine Description:

    This routine determines whether a queued lock is acquired or free.

Arguments:

    Lock - Supplies a pointer to the queued lock.

Return Value:

    TRUE if the queued lock is held.

    FALSE if the queued lock is free.

--*/

{

    if (Lock->Header.WaitQueue.State == SignaledForOne) {
        return FALSE;
    }

    return TRUE;
}

KERNEL_API
VOID
KeInitializeSpinLock (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine initializes a spinlock.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

Return Value:

    None.

--*/

{

    Lock->LockHeld = 0;
    Lock->OwningThread = NULL;

    //
    // This atomic exchange serves as a memory barrier and serializing
    // instruction.
    //

    RtlAtomicExchange32(&(Lock->LockHeld), 0);
    return;
}

KERNEL_API
VOID
KeAcquireSpinLock (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine acquires a kernel spinlock. It must be acquired at or below
    dispatch level. This routine may yield the processor.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

{

    ULONG LockValue;

    while (TRUE) {
        LockValue = RtlAtomicCompareExchange32(&(Lock->LockHeld), 1, 0);
        if (LockValue == 0) {
            break;
        }

        ArProcessorYield();
    }

    Lock->OwningThread = KeGetCurrentThread();
    return;
}

KERNEL_API
VOID
KeReleaseSpinLock (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine releases a kernel spinlock.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

{

    ULONG LockValue;

    //
    // The interlocked version is a serializing instruction, so this avoids
    // unsafe processor and compiler reordering. Simply setting the lock to
    // FALSE is not safe.
    //

    LockValue = RtlAtomicExchange32(&(Lock->LockHeld), 0);

    //
    // Assert if the lock was not held.
    //

    ASSERT(LockValue != 0);

    return;
}

KERNEL_API
BOOL
KeTryToAcquireSpinLock (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine makes one attempt to acquire a spinlock.

Arguments:

    Lock - Supplies a pointer to the lock to attempt to acquire.

Return Value:

    TRUE if the lock was acquired.

    FALSE if the lock was not acquired.

--*/

{

    ULONG LockValue;

    LockValue = RtlAtomicCompareExchange32(&(Lock->LockHeld), 1, 0);
    if (LockValue == 0) {
        Lock->OwningThread = KeGetCurrentThread();
        return TRUE;
    }

    return FALSE;
}

KERNEL_API
BOOL
KeIsSpinLockHeld (
    PKSPIN_LOCK Lock
    )

/*++

Routine Description:

    This routine determines whether a spin lock is held or free.

Arguments:

    Lock - Supplies a pointer to the lock to check.

Return Value:

    TRUE if the lock has been acquired.

    FALSE if the lock is free.

--*/

{

    ULONG Held;

    Held = RtlAtomicOr32(&(Lock->LockHeld), 0);
    if (Held != 0) {
        return TRUE;
    }

    return FALSE;
}

KERNEL_API
PSHARED_EXCLUSIVE_LOCK
KeCreateSharedExclusiveLock (
    VOID
    )

/*++

Routine Description:

    This routine creates a shared-exclusive lock.

Arguments:

    None.

Return Value:

    Returns a pointer to a shared-exclusive lock on success, or NULL on failure.

--*/

{

    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock;
    KSTATUS Status;

    SharedExclusiveLock = MmAllocateNonPagedPool(sizeof(SHARED_EXCLUSIVE_LOCK),
                                                 SHARED_EXCLUSIVE_LOCK_TAG);

    if (SharedExclusiveLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedExclusiveLockEnd;
    }

    RtlZeroMemory(SharedExclusiveLock, sizeof(SHARED_EXCLUSIVE_LOCK));
    SharedExclusiveLock->ExclusiveLock = KeCreateQueuedLock();
    if (SharedExclusiveLock->ExclusiveLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedExclusiveLockEnd;
    }

    SharedExclusiveLock->Event = KeCreateEvent(NULL);
    if (SharedExclusiveLock->Event == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedExclusiveLockEnd;
    }

    //
    // Signal all waiters on this event to let successive exclusive acquires
    // succeed. Otherwise each exclusive release would need to signal the event.
    //

    KeSignalEvent(SharedExclusiveLock->Event, SignalOptionSignalAll);
    KeInitializeSpinLock(&(SharedExclusiveLock->SharedLock));
    SharedExclusiveLock->ShareCount = 0;
    Status = STATUS_SUCCESS;

CreateSharedExclusiveLockEnd:
    if (!KSUCCESS(Status)) {
        if (SharedExclusiveLock != NULL) {
            KeDestroySharedExclusiveLock(SharedExclusiveLock);
            SharedExclusiveLock = NULL;
        }
    }

    return SharedExclusiveLock;
}

KERNEL_API
VOID
KeDestroySharedExclusiveLock (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine destroys a shared-exclusive lock.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    None.

--*/

{

    if (SharedExclusiveLock->ExclusiveLock != NULL) {
        KeDestroyQueuedLock(SharedExclusiveLock->ExclusiveLock);
    }

    if (SharedExclusiveLock->Event != NULL) {
        KeDestroyEvent(SharedExclusiveLock->Event);
    }

    MmFreeNonPagedPool(SharedExclusiveLock);
    return;
}

KERNEL_API
VOID
KeAcquireSharedExclusiveLockShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine acquired the given shared-exclusive lock in shared mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    KeAcquireQueuedLock(SharedExclusiveLock->ExclusiveLock);
    KeAcquireSpinLock(&(SharedExclusiveLock->SharedLock));
    SharedExclusiveLock->ShareCount += 1;
    if (SharedExclusiveLock->ShareCount == 1) {
        KeSignalEvent(SharedExclusiveLock->Event, SignalOptionUnsignal);
    }

    KeReleaseSpinLock(&(SharedExclusiveLock->SharedLock));
    KeReleaseQueuedLock(SharedExclusiveLock->ExclusiveLock);
    return;
}

KERNEL_API
VOID
KeReleaseSharedExclusiveLockShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine releases the given shared-exclusive lock from shared mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    ASSERT(KeIsSharedExclusiveLockHeldShared(SharedExclusiveLock) != FALSE);

    KeAcquireSpinLock(&(SharedExclusiveLock->SharedLock));
    SharedExclusiveLock->ShareCount -= 1;

    //
    // Signal all waiters on this event to let successive exclusive acquires
    // succeed. Otherwise each exclusive release would need to signal the event.
    // In practice, however, there is only one waiter because waiting on the
    // event is protected by the exclusive queued lock.
    //

    if (SharedExclusiveLock->ShareCount == 0) {
        KeSignalEvent(SharedExclusiveLock->Event, SignalOptionSignalAll);
    }

    KeReleaseSpinLock(&(SharedExclusiveLock->SharedLock));
    return;
}

KERNEL_API
VOID
KeAcquireSharedExclusiveLockExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine acquired the given shared-exclusive lock in exclusive mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    KeAcquireQueuedLock(SharedExclusiveLock->ExclusiveLock);
    KeWaitForEvent(SharedExclusiveLock->Event, FALSE, WAIT_TIME_INDEFINITE);
    return;
}

KERNEL_API
VOID
KeReleaseSharedExclusiveLockExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine releases the given shared-exclusive lock from exclusive mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(SharedExclusiveLock) != FALSE);

    KeReleaseQueuedLock(SharedExclusiveLock->ExclusiveLock);
    return;
}

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeld (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held or free.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held, or FALSE if not.

--*/

{

    if ((KeIsSharedExclusiveLockHeldExclusive(SharedExclusiveLock) != FALSE) ||
        (KeIsSharedExclusiveLockHeldShared(SharedExclusiveLock) != FALSE)) {

        return TRUE;
    }

    return FALSE;
}

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeldExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held exclusively
    or not.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held exclusively, or FALSE
    otherwise.

--*/

{

    if ((KeIsQueuedLockHeld(SharedExclusiveLock->ExclusiveLock) != FALSE) &&
        (SharedExclusiveLock->ShareCount == 0)) {

        return TRUE;
    }

    return FALSE;
}

KERNEL_API
BOOL
KeIsSharedExclusiveLockHeldShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is held shared or
    not.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if the shared-exclusive lock is held shared, or FALSE
    otherwise.

--*/

{

    if (SharedExclusiveLock->ShareCount != 0) {
        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

