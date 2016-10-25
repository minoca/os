/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

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
    ULONG Flags;
    PPTHREAD_RWLOCK LockInternal;

    ASSERT(sizeof(PTHREAD_RWLOCK) <= sizeof(pthread_rwlock_t));

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    Flags = 0;
    if (Attribute != NULL) {
        AttributeInternal = (PPTHREAD_RWLOCK_ATTRIBUTE)Attribute;
        Flags = AttributeInternal->Flags;
    }

    OsRwLockInitialize(&(LockInternal->Lock), Flags);
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
    if (LockInternal->Lock.State != 0) {
        return EBUSY;
    }

    //
    // Set it to some crazy value for debugability sake.
    //

    LockInternal->Lock.State = -2;
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

    PPTHREAD_RWLOCK LockInternal;
    KSTATUS Status;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    Status = OsRwLockRead(&(LockInternal->Lock));
    if (Status == STATUS_SUCCESS) {
        return Status;
    }

    return ClConvertKstatusToErrorNumber(Status);
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

    PPTHREAD_RWLOCK LockInternal;
    KSTATUS Status;
    ULONG TimeoutInMilliseconds;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    if (AbsoluteTimeout != NULL) {
        TimeoutInMilliseconds =
              ClpConvertAbsoluteTimespecToRelativeMilliseconds(AbsoluteTimeout,
                                                               CLOCK_REALTIME);

    } else {
        TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
    }

    Status = OsRwLockReadTimed(&(LockInternal->Lock), TimeoutInMilliseconds);
    if (Status == STATUS_SUCCESS) {
        return Status;
    }

    return ClConvertKstatusToErrorNumber(Status);
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
    KSTATUS Status;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    Status = OsRwLockTryRead(&(LockInternal->Lock));
    if (Status == STATUS_SUCCESS) {
        return Status;
    }

    return ClConvertKstatusToErrorNumber(Status);
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

    PPTHREAD_RWLOCK LockInternal;
    KSTATUS Status;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    Status = OsRwLockWrite(&(LockInternal->Lock));
    if (Status == STATUS_SUCCESS) {
        return Status;
    }

    return ClConvertKstatusToErrorNumber(Status);
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

    PPTHREAD_RWLOCK LockInternal;
    KSTATUS Status;
    ULONG TimeoutInMilliseconds;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    if (AbsoluteTimeout != NULL) {
        TimeoutInMilliseconds =
              ClpConvertAbsoluteTimespecToRelativeMilliseconds(AbsoluteTimeout,
                                                               CLOCK_REALTIME);

    } else {
        TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
    }

    Status = OsRwLockWriteTimed(&(LockInternal->Lock), TimeoutInMilliseconds);
    if (Status == STATUS_SUCCESS) {
        return Status;
    }

    return ClConvertKstatusToErrorNumber(Status);
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
    KSTATUS Status;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    Status = OsRwLockTryWrite(&(LockInternal->Lock));
    if (Status == STATUS_SUCCESS) {
        return Status;
    }

    return ClConvertKstatusToErrorNumber(Status);
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

    PPTHREAD_RWLOCK LockInternal;
    KSTATUS Status;

    LockInternal = (PPTHREAD_RWLOCK)Lock;
    Status = OsRwLockUnlock(&(LockInternal->Lock));
    if (Status == STATUS_SUCCESS) {
        return Status;
    }

    return ClConvertKstatusToErrorNumber(Status);
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
    const pthread_rwlockattr_t *Attribute,
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
    if ((AttributeInternal->Flags & OS_RWLOCK_SHARED) != 0) {
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
        AttributeInternal->Flags &= ~OS_RWLOCK_SHARED;

    } else if (Shared == PTHREAD_PROCESS_SHARED) {
        AttributeInternal->Flags |= OS_RWLOCK_SHARED;

    } else {
        return EINVAL;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

