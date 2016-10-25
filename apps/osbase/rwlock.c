/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rwlock.c

Abstract:

    This module implements support for read/write locks.

Author:

    Evan Green 28-Apr-2015

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OS_RWLOCK_UNLOCKED 0
#define OS_RWLOCK_WRITE_LOCKED ((ULONG)-1)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
OspAcquireReadWriteLockForRead (
    POS_RWLOCK Lock,
    ULONG TimeoutInMilliseconds
    );

KSTATUS
OspAcquireReadWriteLockForWrite (
    POS_RWLOCK Lock,
    ULONG TimeoutInMilliseconds
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

OS_API
VOID
OsRwLockInitialize (
    POS_RWLOCK Lock,
    ULONG Flags
    )

/*++

Routine Description:

    This routine initializes a read/write lock.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    Flags - Supplies a bitfield of flags governing the lock behavior.

Return Value:

    None.

--*/

{

    RtlZeroMemory(Lock, sizeof(OS_RWLOCK));
    Lock->Attributes = Flags;
    return;
}

OS_API
KSTATUS
OsRwLockRead (
    POS_RWLOCK Lock
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

    Status code.

--*/

{

    return OspAcquireReadWriteLockForRead(Lock, SYS_WAIT_TIME_INDEFINITE);
}

OS_API
KSTATUS
OsRwLockReadTimed (
    POS_RWLOCK Lock,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine acquires the read/write lock for read access just like the
    read lock function, except that this function will return after the
    specified deadline if the lock could not be acquired.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    TimeoutInMilliseconds - Supplies the duration to wait in milliseconds.

Return Value:

    Status code.

--*/

{

    return OspAcquireReadWriteLockForRead(Lock, TimeoutInMilliseconds);
}

OS_API
KSTATUS
OsRwLockTryRead (
    POS_RWLOCK Lock
    )

/*++

Routine Description:

    This routine performs a single attempt at acquiring the lock for read
    access.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_RESOURCE_IN_USE if the lock is already held.

--*/

{

    ULONG NewState;
    ULONG OldState;

    OldState = Lock->State;
    if (OldState != OS_RWLOCK_WRITE_LOCKED) {
        NewState = RtlAtomicCompareExchange32(&(Lock->State),
                                              OldState + 1,
                                              OldState);

        if (NewState == OldState) {
            return STATUS_SUCCESS;
        }
    }

    return STATUS_RESOURCE_IN_USE;
}

OS_API
KSTATUS
OsRwLockWrite (
    POS_RWLOCK Lock
    )

/*++

Routine Description:

    This routine acquires the read/write lock for write access. The lock can
    only be acquired for write access if there are no readers and no other
    writers.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    Status code.

--*/

{

    return OspAcquireReadWriteLockForWrite(Lock, SYS_WAIT_TIME_INDEFINITE);
}

OS_API
KSTATUS
OsRwLockWriteTimed (
    POS_RWLOCK Lock,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine acquires the read/write lock for write access just like the
    write lock function, except that this function will return after the
    specified deadline if the lock could not be acquired.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    TimeoutInMilliseconds - Supplies the timeout to wait in milliseconds.

Return Value:

    Status code.

--*/

{

    return OspAcquireReadWriteLockForWrite(Lock, TimeoutInMilliseconds);
}

OS_API
KSTATUS
OsRwLockTryWrite (
    POS_RWLOCK Lock
    )

/*++

Routine Description:

    This routine performs a single attempt at acquiring the lock for write
    access.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_RESOURCE_IN_USE if the lock is already held.

--*/

{

    ULONG OldState;

    OldState = Lock->State;
    if (OldState == OS_RWLOCK_UNLOCKED) {
        OldState = RtlAtomicCompareExchange32(&(Lock->State),
                                              OS_RWLOCK_WRITE_LOCKED,
                                              OldState);

        if (OldState == OS_RWLOCK_UNLOCKED) {
            Lock->WriterThreadId = OsGetThreadId();
            return 0;
        }
    }

    return STATUS_RESOURCE_IN_USE;
}

OS_API
KSTATUS
OsRwLockUnlock (
    POS_RWLOCK Lock
    )

/*++

Routine Description:

    This routine unlocks a read/write lock that's been acquired by this thread
    for either read or write.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PERMISSION_DENIED if the lock is not held or was not held by this
    thread.

--*/

{

    ULONG Count;
    POS_RWLOCK LockInternal;
    ULONG NewState;
    ULONG OldState;
    ULONG Operation;

    LockInternal = (POS_RWLOCK)Lock;
    OldState = LockInternal->State;
    if (OldState == OS_RWLOCK_UNLOCKED) {
        return STATUS_PERMISSION_DENIED;
    }

    //
    // If this lock is held by a writer, make sure that this thread is that
    // writer, then set it to unlocked.
    //

    if (OldState == OS_RWLOCK_WRITE_LOCKED) {
        if (LockInternal->WriterThreadId != OsGetThreadId()) {
            return STATUS_PERMISSION_DENIED;
        }

        LockInternal->WriterThreadId = 0;
        LockInternal->State = OS_RWLOCK_UNLOCKED;

    //
    // The lock is held by a reader.
    //

    } else {
        while (OldState > OS_RWLOCK_UNLOCKED) {
            NewState = RtlAtomicCompareExchange32(&(LockInternal->State),
                                                  OldState - 1,
                                                  OldState);

            if (NewState == OldState) {
                break;
            }

            OldState = NewState;
        }

        if (OldState == 0) {
            return STATUS_PERMISSION_DENIED;

        //
        // If there are still other readers, don't release the writers.
        //

        } else if (OldState > 1) {
            return STATUS_SUCCESS;
        }
    }

    //
    // Wake anyone blocking (chaos ensues).
    //

    if ((LockInternal->PendingReaders != 0) ||
        (LockInternal->PendingWriters != 0)) {

        Count = MAX_ULONG;
        Operation = UserLockWake;
        if ((LockInternal->Attributes & OS_RWLOCK_SHARED) == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        OsUserLock(&(LockInternal->State), Operation, &Count, 0);
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
OspAcquireReadWriteLockForRead (
    POS_RWLOCK Lock,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine acquires the given read/write lock for read access.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

    TimeoutInMilliseconds - Supplies the timeout to wait in milliseconds.

Return Value:

    Status code.

--*/

{

    KSTATUS KernelStatus;
    ULONG NewState;
    ULONG OldState;
    ULONG Operation;
    UINTN ThreadId;

    ThreadId = OsGetThreadId();
    if (ThreadId == Lock->WriterThreadId) {
        return STATUS_DEADLOCK;
    }

    while (TRUE) {
        OldState = Lock->State;
        if (OldState != OS_RWLOCK_WRITE_LOCKED) {
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
            Operation = UserLockWait;
            if ((Lock->Attributes & OS_RWLOCK_SHARED) == 0) {
                Operation |= USER_LOCK_PRIVATE;
            }

            RtlAtomicAdd32(&(Lock->PendingReaders), 1);
            KernelStatus = OsUserLock(&(Lock->State),
                                      Operation,
                                      &OldState,
                                      TimeoutInMilliseconds);

            RtlAtomicAdd32(&(Lock->PendingReaders), -1);
            if (KernelStatus == STATUS_TIMEOUT) {
                return KernelStatus;
            }
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
OspAcquireReadWriteLockForWrite (
    POS_RWLOCK Lock,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine acquires the given read/write lock for write access.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

    TimeoutInMilliseconds - Supplies the timeout to wait in milliseconds.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    KSTATUS KernelStatus;
    ULONG OldState;
    ULONG Operation;
    UINTN ThreadId;

    ThreadId = OsGetThreadId();
    if (ThreadId == Lock->WriterThreadId) {
        return STATUS_DEADLOCK;
    }

    while (TRUE) {
        OldState = Lock->State;
        if (OldState == OS_RWLOCK_UNLOCKED) {
            OldState = RtlAtomicCompareExchange32(&(Lock->State),
                                                  OS_RWLOCK_WRITE_LOCKED,
                                                  OldState);

            //
            // If the old value was unlocked, then this thread successfully
            // got the write lock.
            //

            if (OldState == OS_RWLOCK_UNLOCKED) {
                Lock->WriterThreadId = ThreadId;
                break;
            }

        //
        // The lock is already acquired for read or write access.
        //

        } else {
            Operation = UserLockWait;
            if ((Lock->Attributes & OS_RWLOCK_SHARED) == 0) {
                Operation |= USER_LOCK_PRIVATE;
            }

            RtlAtomicAdd32(&(Lock->PendingWriters), 1);
            KernelStatus = OsUserLock(&(Lock->State),
                                      Operation,
                                      &OldState,
                                      TimeoutInMilliseconds);

            RtlAtomicAdd32(&(Lock->PendingWriters), -1);
            if (KernelStatus == STATUS_TIMEOUT) {
                return KernelStatus;
            }
        }
    }

    return STATUS_SUCCESS;
}

