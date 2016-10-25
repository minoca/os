/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>

//
// ---------------------------------------------------------------- Definitions
//

#define QUEUED_LOCK_TAG 0x6C51654B // 'lQeK'
#define SHARED_EXCLUSIVE_LOCK_TAG 0x6553654B // 'eSeK'

//
// Define shared exclusive lock states.
//

#define SHARED_EXCLUSIVE_LOCK_FREE 0
#define SHARED_EXCLUSIVE_LOCK_EXCLUSIVE ((ULONG)-1)
#define SHARED_EXCLUSIVE_LOCK_MAX_WAITERS ((ULONG)-2)

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
    SharedExclusiveLock->Event = KeCreateEvent(NULL);
    if (SharedExclusiveLock->Event == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedExclusiveLockEnd;
    }

    KeSignalEvent(SharedExclusiveLock->Event, SignalOptionSignalOne);
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

    ULONG ExclusiveWaiters;
    BOOL IsWaiter;
    ULONG PreviousState;
    ULONG PreviousWaiters;
    ULONG SharedWaiters;
    ULONG State;

    IsWaiter = FALSE;
    while (TRUE) {
        State = SharedExclusiveLock->State;
        ExclusiveWaiters = SharedExclusiveLock->ExclusiveWaiters;

        //
        // If no one is trying to acquire exclusive, or this is not the first
        // time around, try to acquire the lock. The reason subsequent attempts
        // are allowed to try to acquire even with exclusive waiters is that
        // without this, shared acquires may go down indefinitely on a free
        // lock (since they soaked up the "signal for one" and got woken up
        // ahead of the exclusive waiter).
        //

        if (((ExclusiveWaiters == 0) || (IsWaiter != FALSE)) &&
            (State < SHARED_EXCLUSIVE_LOCK_EXCLUSIVE - 1)) {

            PreviousState = State;
            State = RtlAtomicCompareExchange32(&(SharedExclusiveLock->State),
                                               PreviousState + 1,
                                               PreviousState);

            if (State == PreviousState) {

                //
                // Let all the blocked reader brethren go if this thread was
                // also blocked.
                //

                if (SharedExclusiveLock->SharedWaiters != 0) {
                    KeSignalEvent(SharedExclusiveLock->Event,
                                  SignalOptionPulse);
                }

                break;

            //
            // The addition got foiled, go try again.
            //

            } else {
                continue;
            }
        }

        //
        // Either someone is trying to get it exclusive, or the attempt to
        // get it shared failed. Become a waiter so that the event will be
        // signaled when the lock is released. Use compare-exchange to avoid
        // overflowing.
        //

        if (IsWaiter == FALSE) {
            SharedWaiters = SharedExclusiveLock->SharedWaiters;
            if (SharedWaiters >= SHARED_EXCLUSIVE_LOCK_MAX_WAITERS) {
                continue;
            }

            PreviousWaiters = RtlAtomicCompareExchange32(
                                         &(SharedExclusiveLock->SharedWaiters),
                                         SharedWaiters + 1,
                                         SharedWaiters);

            if (PreviousWaiters != SharedWaiters) {
                continue;
            }

            IsWaiter = TRUE;
        }

        //
        // Recheck the condition now that the waiter count is incremented, as a
        // release may not have seen any waiters and therefore never signaled
        // the event.
        //

        if ((SharedExclusiveLock->ExclusiveWaiters == 0) &&
            (SharedExclusiveLock->State != SHARED_EXCLUSIVE_LOCK_EXCLUSIVE)) {

            continue;
        }

        KeWaitForEvent(SharedExclusiveLock->Event, FALSE, WAIT_TIME_INDEFINITE);
    }

    //
    // This thread is no longer waiting, away it goes.
    //

    if (IsWaiter != FALSE) {
        PreviousWaiters =
                     RtlAtomicAdd32(&(SharedExclusiveLock->SharedWaiters), -1);

        ASSERT(PreviousWaiters != 0);
    }

    return;
}

KERNEL_API
BOOL
KeTryToAcquireSharedExclusiveLockShared (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine makes a single attempt to acquire the given shared-exclusive
    lock in shared mode.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    TRUE if the lock was successfully acquired shared.

    FALSE if the lock was not successfully acquired shared.

--*/

{

    ULONG ExclusiveWaiters;
    ULONG PreviousState;
    ULONG State;

    State = SharedExclusiveLock->State;
    ExclusiveWaiters = SharedExclusiveLock->ExclusiveWaiters;
    if ((ExclusiveWaiters == 0) &&
        (State < SHARED_EXCLUSIVE_LOCK_EXCLUSIVE - 1)) {

        PreviousState = State;
        State = RtlAtomicCompareExchange32(&(SharedExclusiveLock->State),
                                           PreviousState + 1,
                                           PreviousState);

        if (State == PreviousState) {

            //
            // Let all the blocked reader brethren go if this thread was
            // also blocked.
            //

            if (SharedExclusiveLock->SharedWaiters != 0) {
                KeSignalEvent(SharedExclusiveLock->Event,
                              SignalOptionPulse);
            }

            return TRUE;
        }
    }

    return FALSE;
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

    ULONG PreviousState;

    PreviousState = RtlAtomicAdd32(&(SharedExclusiveLock->State), -1);

    ASSERT((PreviousState < SHARED_EXCLUSIVE_LOCK_EXCLUSIVE) &&
           (PreviousState != SHARED_EXCLUSIVE_LOCK_FREE));

    //
    // If this was the last reader and there are writers waiting, signal the
    // event.
    //

    if ((PreviousState - 1 == SHARED_EXCLUSIVE_LOCK_FREE) &&
        (SharedExclusiveLock->ExclusiveWaiters != 0)) {

        KeSignalEvent(SharedExclusiveLock->Event, SignalOptionSignalOne);
    }

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

    ULONG CurrentState;
    ULONG ExclusiveWaiters;
    BOOL IsWaiting;
    ULONG PreviousWaiters;
    ULONG State;

    IsWaiting = FALSE;
    while (TRUE) {
        State = RtlAtomicCompareExchange32(&(SharedExclusiveLock->State),
                                           SHARED_EXCLUSIVE_LOCK_EXCLUSIVE,
                                           SHARED_EXCLUSIVE_LOCK_FREE);

        if (State == SHARED_EXCLUSIVE_LOCK_FREE) {
            break;
        }

        //
        // Increment the exclusive waiters count to indicate to readers that
        // the event needs to be signaled. Use compare-exchange to avoid
        // overflowing.
        //

        if (IsWaiting == FALSE) {
            ExclusiveWaiters = SharedExclusiveLock->ExclusiveWaiters;
            if (ExclusiveWaiters >= SHARED_EXCLUSIVE_LOCK_MAX_WAITERS) {
                continue;
            }

            PreviousWaiters = RtlAtomicCompareExchange32(
                                      &(SharedExclusiveLock->ExclusiveWaiters),
                                      ExclusiveWaiters + 1,
                                      ExclusiveWaiters);

            if (PreviousWaiters != ExclusiveWaiters) {
                continue;
            }

            IsWaiting = TRUE;
        }

        //
        // Recheck the state now that the exclusive waiters count has been
        // incremented, in case the release didn't see the increment and never
        // signaled the event.
        //

        CurrentState = SharedExclusiveLock->State;
        if (CurrentState == SHARED_EXCLUSIVE_LOCK_FREE) {
            continue;
        }

        KeWaitForEvent(SharedExclusiveLock->Event, FALSE, WAIT_TIME_INDEFINITE);
    }

    //
    // This lucky writer is no longer waiting.
    //

    if (IsWaiting != FALSE) {
        PreviousWaiters =
                  RtlAtomicAdd32(&(SharedExclusiveLock->ExclusiveWaiters), -1);

        ASSERT(PreviousWaiters != 0);
    }

    return;
}

KERNEL_API
BOOL
KeTryToAcquireSharedExclusiveLockExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine makes a single attempt to acquire the given shared-exclusive
    lock exclusively.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    TRUE if the lock was successfully acquired exclusively.

    FALSE if the lock was not successfully acquired.

--*/

{

    ULONG State;

    State = RtlAtomicCompareExchange32(&(SharedExclusiveLock->State),
                                       SHARED_EXCLUSIVE_LOCK_EXCLUSIVE,
                                       SHARED_EXCLUSIVE_LOCK_FREE);

    if (State == SHARED_EXCLUSIVE_LOCK_FREE) {
        return TRUE;
    }

    return FALSE;
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

    ASSERT(SharedExclusiveLock->State == SHARED_EXCLUSIVE_LOCK_EXCLUSIVE);

    RtlAtomicExchange32(&(SharedExclusiveLock->State),
                        SHARED_EXCLUSIVE_LOCK_FREE);

    if ((SharedExclusiveLock->SharedWaiters != 0) ||
        (SharedExclusiveLock->ExclusiveWaiters != 0)) {

        KeSignalEvent(SharedExclusiveLock->Event, SignalOptionSignalOne);
    }

    return;
}

KERNEL_API
VOID
KeSharedExclusiveLockConvertToExclusive (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine converts a lock that the caller holds shared into one that
    the caller holds exclusive. This routine will most likely fully release
    and reacquire the lock.

Arguments:

    SharedExclusiveLock - Supplies a pointer to the shared-exclusive lock.

Return Value:

    None.

--*/

{

    ULONG State;

    //
    // Try a shortcut in the case that this caller is the only one that has it
    // shared.
    //

    State = RtlAtomicCompareExchange32(&(SharedExclusiveLock->State),
                                       SHARED_EXCLUSIVE_LOCK_EXCLUSIVE,
                                       1);

    ASSERT((State >= 1) && (State < SHARED_EXCLUSIVE_LOCK_EXCLUSIVE));

    //
    // If the fast conversion failed, get in line like everybody else.
    //

    if (State != 1) {
        KeReleaseSharedExclusiveLockShared(SharedExclusiveLock);
        KeAcquireSharedExclusiveLockExclusive(SharedExclusiveLock);
    }

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

    if (SharedExclusiveLock->State != SHARED_EXCLUSIVE_LOCK_FREE) {
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

    if (SharedExclusiveLock->State == SHARED_EXCLUSIVE_LOCK_EXCLUSIVE) {
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

    if ((SharedExclusiveLock->State != SHARED_EXCLUSIVE_LOCK_FREE) &&
        (SharedExclusiveLock->State < SHARED_EXCLUSIVE_LOCK_EXCLUSIVE)) {

        return TRUE;
    }

    return FALSE;
}

KERNEL_API
BOOL
KeIsSharedExclusiveLockContended (
    PSHARED_EXCLUSIVE_LOCK SharedExclusiveLock
    )

/*++

Routine Description:

    This routine determines whether a shared-exclusive lock is being waited on
    for shared or exclusive access.

Arguments:

    SharedExclusiveLock - Supplies a pointer to a shared-exclusive lock.

Return Value:

    Returns TRUE if other threads are waiting to acquire the lock, or FALSE
    if the lock is uncontented.

--*/

{

    if ((SharedExclusiveLock->SharedWaiters != 0) ||
        (SharedExclusiveLock->ExclusiveWaiters != 0)) {

        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

