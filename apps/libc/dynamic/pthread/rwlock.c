/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    rwlock.c

Abstract:

    This module implements support for POSIX read/write locks.

Author:

    Evan Green 28-Apr-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pthreadp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PTHREAD_RWLOCK_SHARED 0x00000001

#define PTHREAD_RWLOCK_UNLOCKED 0
#define PTHREAD_RWLOCK_WRITE_LOCKED ((ULONG)-1)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ClpAcquireReadWriteLockForRead (
    PPTHREAD_RWLOCK Lock,
    const struct timespec *AbsoluteTimeout
    );

int
ClpAcquireReadWriteLockForWrite (
    PPTHREAD_RWLOCK Lock,
    const struct timespec *AbsoluteTimeout
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PTHREAD_API
int
pthread_rwlock_init (
    pthread_rwlock_t *Lock,
    pthread_rwlockattr_t *Attribute
    )

/*++

Routine Description:

    This routine initializes a read/write lock.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    Attribute - Supplies an optional pointer to an initialized attribute
        structure governing the internal behavior of the lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_RWLOCK_ATTRIBUTE AttributeInternal;
    PPTHREAD_RWLOCK LockInternal;

    ASSERT(sizeof(PTHREAD_RWLOCK) <= sizeof(pthread_rwlock_t));

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    memset(LockInternal, 0, sizeof(PTHREAD_RWLOCK));
    if (Attribute != NULL) {
        AttributeInternal = (PPTHREAD_RWLOCK_ATTRIBUTE)Attribute;
        LockInternal->Attributes = AttributeInternal->Flags;
    }

    return 0;
}

PTHREAD_API
int
pthread_rwlock_destroy (
    pthread_rwlock_t *Lock
    )

/*++

Routine Description:

    This routine destroys a read/write lock.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_RWLOCK LockInternal;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    if (LockInternal->State != PTHREAD_RWLOCK_UNLOCKED) {
        return EBUSY;
    }

    //
    // Set it to some crazy value for debugability sake.
    //

    LockInternal->State = PTHREAD_RWLOCK_WRITE_LOCKED - 1;
    return 0;
}

PTHREAD_API
int
pthread_rwlock_rdlock (
    pthread_rwlock_t *Lock
    )

/*++

Routine Description:

    This routine acquires the read/write lock for read access. Multiple readers
    can acquire the lock simultaneously, but any writes that try to acquire
    the lock while it's held for read will block. Readers that try to
    acquire the lock while it's held for write will also block.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return ClpAcquireReadWriteLockForRead((PPTHREAD_RWLOCK)Lock, NULL);
}

PTHREAD_API
int
pthread_rwlock_timedrdlock (
    pthread_rwlock_t *Lock,
    const struct timespec *AbsoluteTimeout
    )

/*++

Routine Description:

    This routine acquires the read/write lock for read access just like the
    read lock function, except that this function will return after the
    specified deadline if the lock could not be acquired.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    AbsoluteTimeout - Supplies a pointer to the absolute deadline after which
        this function should give up and return failure.

Return Value:

    0 on success.

    ETIMEDOUT if the operation timed out. This thread will not own the lock.

    Returns an error number on failure.

--*/

{

    int Status;

    Status = ClpAcquireReadWriteLockForRead((PPTHREAD_RWLOCK)Lock,
                                            AbsoluteTimeout);

    return Status;
}

PTHREAD_API
int
pthread_rwlock_tryrdlock (
    pthread_rwlock_t *Lock
    )

/*++

Routine Description:

    This routine performs a single attempt at acquiring the lock for read
    access.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_RWLOCK LockInternal;
    ULONG NewState;
    ULONG OldState;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    OldState = LockInternal->State;
    if (OldState != PTHREAD_RWLOCK_WRITE_LOCKED) {
        NewState = RtlAtomicCompareExchange32(&(LockInternal->State),
                                              OldState + 1,
                                              OldState);

        if (NewState == OldState) {
            return 0;
        }
    }

    return EBUSY;
}

PTHREAD_API
int
pthread_rwlock_wrlock (
    pthread_rwlock_t *Lock
    )

/*++

Routine Description:

    This routine acquires the read/write lock for write access. The lock can
    only be acquired for write access if there are no readers and no other
    writers.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return ClpAcquireReadWriteLockForWrite((PPTHREAD_RWLOCK)Lock, NULL);
}

PTHREAD_API
int
pthread_rwlock_timedwrlock (
    pthread_rwlock_t *Lock,
    const struct timespec *AbsoluteTimeout
    )

/*++

Routine Description:

    This routine acquires the read/write lock for write access just like the
    write lock function, except that this function will return after the
    specified deadline if the lock could not be acquired.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    AbsoluteTimeout - Supplies a pointer to the absolute deadline after which
        this function should give up and return failure.

Return Value:

    0 on success.

    ETIMEDOUT if the operation timed out. This thread will not own the lock.

    Returns an error number on failure.

--*/

{

    int Status;

    Status = ClpAcquireReadWriteLockForWrite((PPTHREAD_RWLOCK)Lock,
                                             AbsoluteTimeout);

    return Status;
}

PTHREAD_API
int
pthread_rwlock_trywrlock (
    pthread_rwlock_t *Lock
    )

/*++

Routine Description:

    This routine performs a single attempt at acquiring the lock for write
    access.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_RWLOCK LockInternal;
    ULONG OldState;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    OldState = LockInternal->State;
    if (OldState == PTHREAD_RWLOCK_UNLOCKED) {
        OldState = RtlAtomicCompareExchange32(&(LockInternal->State),
                                              PTHREAD_RWLOCK_WRITE_LOCKED,
                                              OldState);

        if (OldState == PTHREAD_RWLOCK_UNLOCKED) {
            LockInternal->WriterThreadId = OsGetThreadId();
            return 0;
        }
    }

    return EBUSY;
}

PTHREAD_API
int
pthread_rwlock_unlock (
    pthread_rwlock_t *Lock
    )

/*++

Routine Description:

    This routine unlocks a read/write lock that's been acquired by this thread
    for either read or write.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONG Count;
    PPTHREAD_RWLOCK LockInternal;
    ULONG NewState;
    ULONG OldState;
    ULONG Operation;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    OldState = LockInternal->State;
    if (OldState == PTHREAD_RWLOCK_UNLOCKED) {
        return EPERM;
    }

    //
    // If this lock is held by a writer, make sure that this thread is that
    // writer, then set it to unlocked.
    //

    if (OldState == PTHREAD_RWLOCK_WRITE_LOCKED) {
        if (LockInternal->WriterThreadId != OsGetThreadId()) {
            return EPERM;
        }

        LockInternal->WriterThreadId = 0;
        LockInternal->State = PTHREAD_RWLOCK_UNLOCKED;

    //
    // The lock is held by a reader.
    //

    } else {
        while (OldState > PTHREAD_RWLOCK_UNLOCKED) {
            NewState = RtlAtomicCompareExchange32(&(LockInternal->State),
                                                  OldState - 1,
                                                  OldState);

            if (NewState == OldState) {
                break;
            }
        }

        if (OldState == 0) {
            return EPERM;

        //
        // If there are still other readers, don't release the writers.
        //

        } else if (OldState > 1) {
            return 0;
        }
    }

    //
    // Wake anyone blocking (chaos ensues).
    //

    if ((LockInternal->PendingReaders != 0) ||
        (LockInternal->PendingWriters != 0)) {

        Count = MAX_ULONG;
        Operation = UserLockWake;
        if ((LockInternal->Attributes & PTHREAD_RWLOCK_SHARED) == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        OsUserLock(&(LockInternal->State), Operation, &Count, 0);
    }

    return 0;
}

PTHREAD_API
int
pthread_rwlockattr_init (
    pthread_rwlockattr_t *Attribute
    )

/*++

Routine Description:

    This routine initializes a read/write lock attribute structure.

Arguments:

    Attribute - Supplies a pointer to the read/write lock attribute.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_RWLOCK_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_RWLOCK_ATTRIBUTE)Attribute;
    AttributeInternal->Flags = 0;
    return 0;
}

PTHREAD_API
int
pthread_rwlockattr_destroy (
    pthread_rwlockattr_t *Attribute
    )

/*++

Routine Description:

    This routine destroys a read/write lock attribute structure.

Arguments:

    Attribute - Supplies a pointer to the read/write lock attribute.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_RWLOCK_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_RWLOCK_ATTRIBUTE)Attribute;
    AttributeInternal->Flags = -1;
    return 0;
}

PTHREAD_API
int
pthread_rwlockattr_getpshared (
    pthread_rwlockattr_t *Attribute,
    int *Shared
    )

/*++

Routine Description:

    This routine reads the shared attribute from a read/write lock attributes
    structure.

Arguments:

    Attribute - Supplies a pointer to the read/write lock attribute.

    Shared - Supplies a pointer where the shared attribute of the lock will be
        returned on success. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_RWLOCK_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_RWLOCK_ATTRIBUTE)Attribute;
    *Shared = PTHREAD_PROCESS_PRIVATE;
    if ((AttributeInternal->Flags & PTHREAD_RWLOCK_SHARED) != 0) {
        *Shared = PTHREAD_PROCESS_SHARED;
    }

    return 0;
}

PTHREAD_API
int
pthread_rwlockattr_setpshared (
    pthread_rwlockattr_t *Attribute,
    int Shared
    )

/*++

Routine Description:

    This routine sets the shared attribute in a read/write lock attributes
    structure.

Arguments:

    Attribute - Supplies a pointer to the read/write lock attribute.

    Shared - Supplies the new shared value to set. See PTHREAD_PROCESS_*
        definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_RWLOCK_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_RWLOCK_ATTRIBUTE)Attribute;
    if (Shared == PTHREAD_PROCESS_PRIVATE) {
        AttributeInternal->Flags &= ~PTHREAD_RWLOCK_SHARED;

    } else if (Shared == PTHREAD_PROCESS_SHARED) {
        AttributeInternal->Flags |= PTHREAD_RWLOCK_SHARED;

    } else {
        return EINVAL;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ClpAcquireReadWriteLockForRead (
    PPTHREAD_RWLOCK Lock,
    const struct timespec *AbsoluteTimeout
    )

/*++

Routine Description:

    This routine acquires the given read/write lock for read access.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

    AbsoluteTimeout - Supplies a pointer to the absolute deadline after which
        this function should give up and return failure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    KSTATUS KernelStatus;
    ULONG NewState;
    ULONG OldState;
    ULONG Operation;
    UINTN ThreadId;
    ULONG TimeoutInMilliseconds;

    ThreadId = OsGetThreadId();
    if (ThreadId == Lock->WriterThreadId) {
        return EDEADLK;
    }

    while (TRUE) {
        OldState = Lock->State;
        if (OldState != PTHREAD_RWLOCK_WRITE_LOCKED) {
            NewState = RtlAtomicCompareExchange32(&(Lock->State),
                                                  OldState + 1,
                                                  OldState);

            //
            // If the old value wasn't write locked, then the reader was
            // successfully added.
            //

            if (NewState == OldState) {
                break;
            }

        //
        // The lock is already acquired for write access.
        //

        } else {
            if (AbsoluteTimeout != NULL) {
                TimeoutInMilliseconds =
                            ClpConvertAbsoluteTimespecToRelativeMilliseconds(
                                                               AbsoluteTimeout,
                                                               CLOCK_REALTIME);

            } else {
                TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
            }

            Operation = UserLockWait;
            if ((Lock->Attributes & PTHREAD_RWLOCK_SHARED) == 0) {
                Operation |= USER_LOCK_PRIVATE;
            }

            RtlAtomicAdd32(&(Lock->PendingReaders), 1);
            KernelStatus = OsUserLock(&(Lock->State),
                                      Operation,
                                      &OldState,
                                      TimeoutInMilliseconds);

            RtlAtomicAdd32(&(Lock->PendingReaders), -1);
            if (KernelStatus == STATUS_TIMEOUT) {
                return ETIMEDOUT;
            }
        }
    }

    return 0;
}

int
ClpAcquireReadWriteLockForWrite (
    PPTHREAD_RWLOCK Lock,
    const struct timespec *AbsoluteTimeout
    )

/*++

Routine Description:

    This routine acquires the given read/write lock for write access.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

    AbsoluteTimeout - Supplies a pointer to the absolute deadline after which
        this function should give up and return failure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    KSTATUS KernelStatus;
    ULONG OldState;
    ULONG Operation;
    UINTN ThreadId;
    ULONG TimeoutInMilliseconds;

    ThreadId = OsGetThreadId();
    if (ThreadId == Lock->WriterThreadId) {
        return EDEADLK;
    }

    while (TRUE) {
        OldState = Lock->State;
        if (OldState == PTHREAD_RWLOCK_UNLOCKED) {
            OldState = RtlAtomicCompareExchange32(&(Lock->State),
                                                  PTHREAD_RWLOCK_WRITE_LOCKED,
                                                  OldState);

            //
            // If the old value was unlocked, then this thread successfully
            // got the write lock.
            //

            if (OldState == PTHREAD_RWLOCK_UNLOCKED) {
                Lock->WriterThreadId = ThreadId;
                break;
            }

        //
        // The lock is already acquired for read or write access.
        //

        } else {
            if (AbsoluteTimeout != NULL) {
                TimeoutInMilliseconds =
                            ClpConvertAbsoluteTimespecToRelativeMilliseconds(
                                                               AbsoluteTimeout,
                                                               CLOCK_REALTIME);

            } else {
                TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
            }

            Operation = UserLockWait;
            if ((Lock->Attributes & PTHREAD_RWLOCK_SHARED) == 0) {
                Operation |= USER_LOCK_PRIVATE;
            }

            RtlAtomicAdd32(&(Lock->PendingWriters), 1);
            KernelStatus = OsUserLock(&(Lock->State),
                                      Operation,
                                      &OldState,
                                      TimeoutInMilliseconds);

            RtlAtomicAdd32(&(Lock->PendingWriters), -1);
            if (KernelStatus == STATUS_TIMEOUT) {
                return ETIMEDOUT;
            }
        }
    }

    return 0;
}

