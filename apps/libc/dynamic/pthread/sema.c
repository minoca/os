/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sema.c

Abstract:

    This module implements support for POSIX semaphores.

Author:

    Evan Green 4-May-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pthreadp.h"
#include <limits.h>
#include <semaphore.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros manipulate the count inside the semaphore state.
//

#define PTHREAD_SEMAPHORE_GET_COUNT(_State) \
    ((INT)(_State) >> PTHREAD_SEMAPHORE_VALUE_SHIFT)

//
// This macro returns the semaphore state for a given count.
//

#define PTHREAD_SEMAPHORE_SET_COUNT(_Count)                 \
    (((ULONG)(_Count) << PTHREAD_SEMAPHORE_VALUE_SHIFT) &   \
     PTHREAD_SEMAPHORE_VALUE_MASK)

//
// These macros increment and decrement the count in the given state.
//

#define PTHREAD_SEMAPHORE_DECREMENT_STATE(_State)           \
    (((_State) - (1U << PTHREAD_SEMAPHORE_VALUE_SHIFT)) &   \
     PTHREAD_SEMAPHORE_VALUE_MASK)

#define PTHREAD_SEMAPHORE_INCREMENT_STATE(_State)           \
    (((_State) + (1U << PTHREAD_SEMAPHORE_VALUE_SHIFT)) &   \
     PTHREAD_SEMAPHORE_VALUE_MASK)

//
// ---------------------------------------------------------------- Definitions
//

#define PTHREAD_SEMAPHORE_SHARED 0x00000001
#define PTHREAD_SEMAPHORE_VALUE_SHIFT 1
#define PTHREAD_SEMAPHORE_VALUE_MASK 0xFFFFFFFE

#define PTHREAD_SEMAPHORE_WAITED_ON PTHREAD_SEMAPHORE_SET_COUNT(-1)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ClpSemaphoreDecrement (
    PPTHREAD_SEMAPHORE Semaphore
    );

INT
ClpSemaphoreTryToDecrement (
    PPTHREAD_SEMAPHORE Semaphore
    );

INT
ClpSemaphoreIncrement (
    PPTHREAD_SEMAPHORE Semaphore
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PTHREAD_API
int
sem_init (
    sem_t *Semaphore,
    int Shared,
    unsigned int Value
    )

/*++

Routine Description:

    This routine initializes a semaphore object.

Arguments:

    Semaphore - Supplies a pointer to the semaphore to initialize.

    Shared - Supplies a boolean indicating whether the semaphore should be
        shared across processes (non-zero) or private to a particular process
        (zero).

    Value - Supplies the initial value to set in the semaphore.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    PPTHREAD_SEMAPHORE SemaphoreInternal;
    ULONG State;

    if (Value > SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }

    State = PTHREAD_SEMAPHORE_SET_COUNT(Value);
    if (Shared != 0) {
        State |= PTHREAD_SEMAPHORE_SHARED;
    }

    SemaphoreInternal = (PPTHREAD_SEMAPHORE)Semaphore;
    SemaphoreInternal->State = State;
    return 0;
}

PTHREAD_API
int
sem_destroy (
    sem_t *Semaphore
    )

/*++

Routine Description:

    This routine releases all resources associated with a POSIX semaphore.

Arguments:

    Semaphore - Supplies a pointer to the semaphore to destroy.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return 0;
}

PTHREAD_API
int
sem_wait (
    sem_t *Semaphore
    )

/*++

Routine Description:

    This routine blocks until the given semaphore can be decremented to zero or
    above. On success, the semaphore value will be decremented.

Arguments:

    Semaphore - Supplies a pointer to the semaphore to wait on.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    ULONG Operation;
    PPTHREAD_SEMAPHORE SemaphoreInternal;
    ULONG Shared;
    KSTATUS Status;
    ULONG Value;

    SemaphoreInternal = (PPTHREAD_SEMAPHORE)Semaphore;
    Shared = SemaphoreInternal->State & PTHREAD_SEMAPHORE_SHARED;
    while (TRUE) {
        if (ClpSemaphoreDecrement(SemaphoreInternal) > 0) {
            return 0;
        }

        Operation = UserLockWait;
        if (Shared == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        Value = PTHREAD_SEMAPHORE_WAITED_ON | Shared;
        Status = OsUserLock(&(SemaphoreInternal->State),
                            Operation,
                            &Value,
                            SYS_WAIT_TIME_INDEFINITE);

        if (Status == STATUS_INTERRUPTED) {
            errno = EINTR;
            return -1;
        }
    }

    return 0;
}

PTHREAD_API
int
sem_timedwait (
    sem_t *Semaphore,
    const struct timespec *AbsoluteTimeout
    )

/*++

Routine Description:

    This routine blocks until the given semaphore can be decremented to zero or
    above. This routine may time out after the specified deadline.

Arguments:

    Semaphore - Supplies a pointer to the semaphore to wait on.

    AbsoluteTimeout - Supplies the deadline as an absolute time after which
        the operation should fail and return ETIMEDOUT.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    ULONG Operation;
    PPTHREAD_SEMAPHORE SemaphoreInternal;
    ULONG Shared;
    KSTATUS Status;
    ULONG TimeoutInMilliseconds;
    ULONG Value;

    SemaphoreInternal = (PPTHREAD_SEMAPHORE)Semaphore;

    //
    // Try to decrement the semaphore before checking the timeout.
    //

    if (ClpSemaphoreTryToDecrement(SemaphoreInternal) > 0) {
        return 0;
    }

    if ((AbsoluteTimeout == NULL) ||
        (AbsoluteTimeout->tv_sec < 0) ||
        (AbsoluteTimeout->tv_nsec < 0) ||
        (AbsoluteTimeout->tv_nsec > NANOSECONDS_PER_SECOND)) {

        errno = EINVAL;
        return -1;
    }

    Shared = SemaphoreInternal->State & PTHREAD_SEMAPHORE_SHARED;
    Operation = UserLockWait;
    if (Shared == 0) {
        Operation |= USER_LOCK_PRIVATE;
    }

    while (TRUE) {
        TimeoutInMilliseconds =
              ClpConvertAbsoluteTimespecToRelativeMilliseconds(AbsoluteTimeout,
                                                               CLOCK_REALTIME);

        //
        // Try to grab the semaphore.
        //

        if (ClpSemaphoreDecrement(SemaphoreInternal) > 0) {
            break;
        }

        Value = PTHREAD_SEMAPHORE_WAITED_ON | Shared;
        Status = OsUserLock(&(SemaphoreInternal->State),
                            Operation,
                            &Value,
                            TimeoutInMilliseconds);

        if (Status == STATUS_TIMEOUT) {
            errno = ETIMEDOUT;
            return -1;

        } else if (Status == STATUS_INTERRUPTED) {
            errno = EINTR;
            return -1;
        }
    }

    return 0;
}

PTHREAD_API
int
sem_trywait (
    sem_t *Semaphore
    )

/*++

Routine Description:

    This routine blocks until the given semaphore can be decremented to zero or
    above. This routine may time out after the specified deadline.

Arguments:

    Semaphore - Supplies a pointer to the semaphore to wait on.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    if (ClpSemaphoreTryToDecrement((PPTHREAD_SEMAPHORE)Semaphore) > 0) {
        return 0;
    }

    errno = EAGAIN;
    return -1;
}

PTHREAD_API
int
sem_getvalue (
    sem_t *Semaphore,
    int *SemaphoreValue
    )

/*++

Routine Description:

    This routine returns the current count of the semaphore.

Arguments:

    Semaphore - Supplies a pointer to the semaphore.

    SemaphoreValue - Supplies a pointer where the semaphore value will be
        returned.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    PPTHREAD_SEMAPHORE SemaphoreInternal;
    INT Value;

    SemaphoreInternal = (PPTHREAD_SEMAPHORE)Semaphore;
    Value = PTHREAD_SEMAPHORE_GET_COUNT(SemaphoreInternal->State);
    if (Value < 0) {
        Value = 0;
    }

    *SemaphoreValue = Value;
    return 0;
}

PTHREAD_API
int
sem_post (
    sem_t *Semaphore
    )

/*++

Routine Description:

    This routine increments the semaphore value. If the value is incremented
    above zero, then threads waiting on the semaphore will be released to
    try and acquire it.

Arguments:

    Semaphore - Supplies a pointer to the semaphore.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    ULONG Count;
    ULONG Operation;
    PPTHREAD_SEMAPHORE SemaphoreInternal;
    INT Value;

    SemaphoreInternal = (PPTHREAD_SEMAPHORE)Semaphore;
    Value = ClpSemaphoreIncrement(SemaphoreInternal);

    //
    // If there were waiters, wake everyone up.
    //

    if (Value < 0) {
        Operation = UserLockWake;
        if ((SemaphoreInternal->State & PTHREAD_SEMAPHORE_SHARED) == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        Count = MAX_ULONG;
        OsUserLock(&(SemaphoreInternal->State), Operation, &Count, 0);

    } else if (Value == SEM_VALUE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ClpSemaphoreDecrement (
    PPTHREAD_SEMAPHORE Semaphore
    )

/*++

Routine Description:

    This routine decrements the semaphore value and returns the old value,
    honoring the value of negative one, which means there are waiters.

Arguments:

    Semaphore - Supplies a pointer to the semaphore to decrement.

Return Value:

    Returns the previous count.

--*/

{

    ULONG NewValue;
    ULONG OldValue;
    ULONG SetValue;
    ULONG Shared;

    OldValue = Semaphore->State;
    Shared = OldValue & PTHREAD_SEMAPHORE_SHARED;

    //
    // Loop trying to perform a sane decrement.
    //

    while (TRUE) {
        if (PTHREAD_SEMAPHORE_GET_COUNT(OldValue) < 0) {
            break;
        }

        SetValue = PTHREAD_SEMAPHORE_DECREMENT_STATE(OldValue) | Shared;
        NewValue = RtlAtomicCompareExchange32(&(Semaphore->State),
                                              SetValue,
                                              OldValue);

        if (NewValue == OldValue) {
            break;
        }

        OldValue = NewValue;
    }

    return PTHREAD_SEMAPHORE_GET_COUNT(OldValue);
}

INT
ClpSemaphoreTryToDecrement (
    PPTHREAD_SEMAPHORE Semaphore
    )

/*++

Routine Description:

    This routine attempts to decrement the semaphore value and returns the old
    value. It will not change the value if it was zero or "waited on".

Arguments:

    Semaphore - Supplies a pointer to the semaphore to attempt to decrement.

Return Value:

    Returns the previous count.

--*/

{

    ULONG NewValue;
    ULONG OldValue;
    ULONG SetValue;
    ULONG Shared;

    OldValue = Semaphore->State;
    Shared = OldValue & PTHREAD_SEMAPHORE_SHARED;

    //
    // Loop trying to perform a sane decrement.
    //

    while (TRUE) {
        if (PTHREAD_SEMAPHORE_GET_COUNT(OldValue) <= 0) {
            break;
        }

        SetValue = PTHREAD_SEMAPHORE_DECREMENT_STATE(OldValue) | Shared;
        NewValue = RtlAtomicCompareExchange32(&(Semaphore->State),
                                              SetValue,
                                              OldValue);

        if (NewValue == OldValue) {
            break;
        }

        OldValue = NewValue;
    }

    return PTHREAD_SEMAPHORE_GET_COUNT(OldValue);
}

INT
ClpSemaphoreIncrement (
    PPTHREAD_SEMAPHORE Semaphore
    )

/*++

Routine Description:

    This routine increments the count in the semaphore and returns the old
    value. The count of -1 is treated equal to the count of 0 (as in, -1 goes
    directly to 1).

Arguments:

    Semaphore - Supplies a pointer to the semaphore to increment.

Return Value:

    Returns the previous count.

--*/

{

    ULONG NewValue;
    ULONG OldValue;
    ULONG SetValue;
    ULONG Shared;

    OldValue = Semaphore->State;
    Shared = OldValue & PTHREAD_SEMAPHORE_SHARED;

    //
    // Loop trying to perform a sane decrement.
    //

    while (TRUE) {
        if (PTHREAD_SEMAPHORE_GET_COUNT(OldValue) == SEM_VALUE_MAX) {
            break;
        }

        //
        // Negative values to straight to one.
        //

        if (PTHREAD_SEMAPHORE_GET_COUNT(OldValue) < 0) {
            SetValue = PTHREAD_SEMAPHORE_SET_COUNT(1) | Shared;

        } else {
            SetValue = PTHREAD_SEMAPHORE_INCREMENT_STATE(OldValue) | Shared;
        }

        NewValue = RtlAtomicCompareExchange32(&(Semaphore->State),
                                              SetValue,
                                              OldValue);

        if (NewValue == OldValue) {
            break;
        }

        OldValue = NewValue;
    }

    return PTHREAD_SEMAPHORE_GET_COUNT(OldValue);
}

