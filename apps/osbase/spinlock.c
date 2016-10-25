/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spinlock.c

Abstract:

    This module implements a basic lock primitive.

Author:

    Evan Green 19-Nov-2013

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

#define OS_LOCK_UNLOCKED 0
#define OS_LOCK_LOCKED 1
#define OS_LOCK_LOCKED_WITH_WAITERS 2

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

OS_API
VOID
OsInitializeLock (
    POS_LOCK Lock,
    ULONG SpinCount
    )

/*++

Routine Description:

    This routine initializes an OS lock.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

    SpinCount - Supplies the number of initial consecutive attempts to make
        when acquiring the lock. Larger values here minimize the delay between
        when the lock is freed and subsequently reacquired, but are bad for
        power performance as the thread is burning energy doing nothing. Most
        applications should set this to SPIN_LOCK_DEFAULT_SPIN_COUNT.

Return Value:

    None.

--*/

{

    Lock->SpinCount = SpinCount;
    RtlAtomicExchange(&(Lock->Value), OS_LOCK_UNLOCKED);
    return;
}

OS_API
VOID
OsAcquireLock (
    POS_LOCK Lock
    )

/*++

Routine Description:

    This routine acquires the given OS lock. It is not recursive, meaning
    that if the lock is already held by the current thread this routine will
    never return.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

{

    ULONG OriginalValue;
    ULONG SpinCount;
    ULONG SpinIndex;

    SpinCount = Lock->SpinCount;
    for (SpinIndex = 0; SpinIndex < SpinCount; SpinIndex += 1) {
        OriginalValue = RtlAtomicCompareExchange32(&(Lock->Value),
                                                   OS_LOCK_LOCKED,
                                                   OS_LOCK_UNLOCKED);

        if (OriginalValue == OS_LOCK_UNLOCKED) {
            return;
        }
    }

    //
    // Loop setting the lock to contended and waiting for it to become free.
    //

    while (TRUE) {
        OriginalValue = RtlAtomicExchange32(&(Lock->Value),
                                            OS_LOCK_LOCKED_WITH_WAITERS);

        if (OriginalValue == OS_LOCK_UNLOCKED) {
            break;
        }

        OriginalValue = OS_LOCK_LOCKED_WITH_WAITERS;
        OsUserLock(&(Lock->Value),
                   UserLockWait | USER_LOCK_PRIVATE,
                   &OriginalValue,
                   WAIT_TIME_INDEFINITE);
    }

    return;
}

OS_API
BOOL
OsTryToAcquireLock (
    POS_LOCK Lock
    )

/*++

Routine Description:

    This routine performs a single attempt to acquire the given OS lock.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    TRUE if the lock was successfully acquired.

    FALSE if the lock was already held and could not be acquired.

--*/

{

    ULONG OriginalValue;

    OriginalValue = RtlAtomicCompareExchange32(&(Lock->Value),
                                               OS_LOCK_LOCKED,
                                               OS_LOCK_UNLOCKED);

    if (OriginalValue == OS_LOCK_UNLOCKED) {
        return TRUE;
    }

    return FALSE;
}

OS_API
VOID
OsReleaseLock (
    POS_LOCK Lock
    )

/*++

Routine Description:

    This routine releases the given OS lock. The lock must have been
    previously acquired, obviously.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

{

    ULONG Count;
    ULONG OriginalValue;

    OriginalValue = RtlAtomicExchange32(&(Lock->Value), OS_LOCK_UNLOCKED);

    ASSERT(OriginalValue != OS_LOCK_UNLOCKED);

    if (OriginalValue == OS_LOCK_LOCKED_WITH_WAITERS) {
        Count = 1;
        OsUserLock(&(Lock->Value),
                   UserLockWake | USER_LOCK_PRIVATE,
                   &Count,
                   0);
    }

    return;
}

OS_API
KSTATUS
OsUserLock (
    PVOID Address,
    ULONG Operation,
    PULONG Value,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine performs a cooperative locking operation with the kernel.

Arguments:

    Address - Supplies a pointer to a 32-bit value representing the lock in
        user mode.

    Operation - Supplies the operation of type USER_LOCK_OPERATION, as well as
        any flags, see USER_LOCK_* definitions. Valid operations are:

        UserLockWait - Puts the current thread to sleep atomically if the value
        at the given address is the same as the value parameter passed in.

        UserLockWake - Wakes the number of threads given in the value that are
        blocked on the given address.

    Value - Supplies a pointer whose value depends on the operation. For wait
        operations, this contains the value to check the address against. This
        is not used on output for wait operations. For wake operations, this
        contains the number of processes to wake on input. On output, contains
        the number of processes woken.

    TimeoutInMilliseconds - Supplies the number of milliseconds for a wait
        operation to complete before timing out. Set to
        SYS_WAIT_TIME_INDEFINITE to wait forever. This is not used on wake
        operations.

Return Value:

    STATUS_SUCCESS if the wait or wake succeeded.

    STATUS_OPERATION_WOULD_BLOCK if for a wait operation the value at the given
    address was not equal to the supplied value.

    STATUS_TIMEOUT if a wait operation timed out before the wait was satisfied.

    STATUS_INTERRUPTED if a signal arrived before a wait was completed or timed
    out.

--*/

{

    SYSTEM_CALL_USER_LOCK Parameters;
    KSTATUS Status;

    Parameters.Address = Address;
    Parameters.Value = *Value;
    Parameters.Operation = Operation;
    Parameters.TimeoutInMilliseconds = TimeoutInMilliseconds;
    Status = OsSystemCall(SystemCallUserLock, &Parameters);
    *Value = Parameters.Value;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

