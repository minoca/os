/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cond.c

Abstract:

    This module implements support for POSIX threads condition variables.

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

#define PTHREAD_CONDITION_SHARED 0x00000001
#define PTHREAD_CONDITION_CLOCK_MONOTONIC 0x00000002
#define PTHREAD_CONDITION_FLAGS 0x00000003
#define PTHREAD_CONDITION_COUNTER_SHIFT 2
#define PTHREAD_CONDITION_COUNTER_MASK (~PTHREAD_CONDITION_FLAGS)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ClpPulseCondition (
    PPTHREAD_CONDITION Condition,
    UINTN Count
    );

int
ClpWaitOnCondition (
    PPTHREAD_CONDITION Condition,
    pthread_mutex_t *Mutex,
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
pthread_cond_init (
    pthread_cond_t *Condition,
    pthread_condattr_t *Attribute
    )

/*++

Routine Description:

    This routine initializes a condition variable structure.

Arguments:

    Condition - Supplies a pointer to the condition variable structure.

    Attribute - Supplies an optional pointer to the condition variable
        attributes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_CONDITION_ATTRIBUTE AttributeInternal;
    PPTHREAD_CONDITION ConditionInternal;

    ConditionInternal = (PPTHREAD_CONDITION)Condition;
    if (Attribute == NULL) {
        ConditionInternal->State = 0;
        return 0;
    }

    AttributeInternal = (PPTHREAD_CONDITION_ATTRIBUTE)Attribute;

    ASSERT((AttributeInternal->Flags & PTHREAD_CONDITION_COUNTER_MASK) == 0);

    ConditionInternal->State = AttributeInternal->Flags &
                               PTHREAD_CONDITION_FLAGS;

    return 0;
}

PTHREAD_API
int
pthread_cond_destroy (
    pthread_cond_t *Condition
    )

/*++

Routine Description:

    This routine destroys a condition variable structure.

Arguments:

    Condition - Supplies a pointer to the condition variable structure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_CONDITION ConditionInternal;

    ConditionInternal = (PPTHREAD_CONDITION)Condition;
    ConditionInternal->State = MAX_ULONG;
    return 0;
}

PTHREAD_API
int
pthread_cond_broadcast (
    pthread_cond_t *Condition
    )

/*++

Routine Description:

    This routine wakes up all threads waiting on the given condition variable.
    This is useful when there are multiple different predicates behind the
    same condition variable.

Arguments:

    Condition - Supplies a pointer to the condition variable to wake.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return ClpPulseCondition((PPTHREAD_CONDITION)Condition, MAX_ULONG);
}

PTHREAD_API
int
pthread_cond_signal (
    pthread_cond_t *Condition
    )

/*++

Routine Description:

    This routine wakes up at least one thread waiting on the given condition
    variable. This is preferred over the broadcast function if all waiting
    threads are checking the same mutex, as it prevents the thundering herd
    associated with broadcast (all woken threads race to acquire the same
    mutex). Multiple threads may exit a condition wait, so it is critical to
    check the predicate on return.

Arguments:

    Condition - Supplies a pointer to the condition variable to wake.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return ClpPulseCondition((PPTHREAD_CONDITION)Condition, 1);
}

PTHREAD_API
int
pthread_cond_wait (
    pthread_cond_t *Condition,
    pthread_mutex_t *Mutex
    )

/*++

Routine Description:

    This routine unlocks the given mutex, blocks until the given condition
    variable is signaled, and then reacquires the mutex.

Arguments:

    Condition - Supplies a pointer to the condition variable to wait on.

    Mutex - Supplies a pointer to the mutex to operate on.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return ClpWaitOnCondition((PPTHREAD_CONDITION)Condition, Mutex, NULL);
}

PTHREAD_API
int
pthread_cond_timedwait (
    pthread_cond_t *Condition,
    pthread_mutex_t *Mutex,
    const struct timespec *AbsoluteTimeout
    )

/*++

Routine Description:

    This routine unlocks the given mutex, blocks until the given condition
    variable is signaled, and then reacquires the mutex. This wait can time
    out after the specified deadline.

Arguments:

    Condition - Supplies a pointer to the condition variable to wait on.

    Mutex - Supplies a pointer to the mutex to operate on.

    AbsoluteTimeout - Supplies a pointer to an absolute time after which the
        wait should return anyway.

Return Value:

    0 on success.

    ETIMEDOUT if the operation timed out. The predicate may have become true
    naturally anyway, so the caller should always check their predicates.

    Returns an error number on failure.

--*/

{

    int Status;

    Status = ClpWaitOnCondition((PPTHREAD_CONDITION)Condition,
                                Mutex,
                                AbsoluteTimeout);

    return Status;
}

PTHREAD_API
int
pthread_condattr_init (
    pthread_condattr_t *Attribute
    )

/*++

Routine Description:

    This routine initializes a condition variable attribute structure.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_CONDITION_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_CONDITION_ATTRIBUTE)Attribute;
    AttributeInternal->Flags = 0;
    return 0;
}

PTHREAD_API
int
pthread_condattr_destroy (
    pthread_condattr_t *Attribute
    )

/*++

Routine Description:

    This routine destroys a condition variable attribute structure.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_CONDITION_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_CONDITION_ATTRIBUTE)Attribute;
    AttributeInternal->Flags = MAX_ULONG;
    return 0;
}

PTHREAD_API
int
pthread_condattr_getpshared (
    const pthread_condattr_t *Attribute,
    int *Shared
    )

/*++

Routine Description:

    This routine determines the shared attribute in a condition variable.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

    Shared - Supplies a pointer where the shared attribute will be returned,
        indicating whether the condition variable is visible across processes.
        See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_CONDITION_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_CONDITION_ATTRIBUTE)Attribute;
    *Shared = PTHREAD_PROCESS_PRIVATE;
    if ((AttributeInternal->Flags & PTHREAD_CONDITION_SHARED) != 0) {
        *Shared = PTHREAD_PROCESS_SHARED;
    }

    return 0;
}

PTHREAD_API
int
pthread_condattr_setpshared (
    pthread_condattr_t *Attribute,
    int Shared
    )

/*++

Routine Description:

    This routine sets the shared attribute in a condition variable.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

    Shared - Supplies the value indicating whether this condition variable
        should be visible across processes. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_CONDITION_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_CONDITION_ATTRIBUTE)Attribute;
    if (Shared == PTHREAD_PROCESS_PRIVATE) {
        AttributeInternal->Flags &= ~PTHREAD_CONDITION_SHARED;

    } else if (Shared == PTHREAD_PROCESS_SHARED) {
        AttributeInternal->Flags |= PTHREAD_CONDITION_SHARED;

    } else {
        return EINVAL;
    }

    return 0;
}

PTHREAD_API
int
pthread_condattr_getclock (
    const pthread_condattr_t *Attribute,
    int *Clock
    )

/*++

Routine Description:

    This routine determines which clock the condition variable uses for timed
    waits.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

    Clock - Supplies a pointer where the clock source for timed waits on the
        condition variable with this attribute will be returned. See CLOCK_*
        definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_CONDITION_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_CONDITION_ATTRIBUTE)Attribute;
    *Clock = CLOCK_REALTIME;
    if ((AttributeInternal->Flags & PTHREAD_CONDITION_CLOCK_MONOTONIC) != 0) {
        *Clock = CLOCK_MONOTONIC;
    }

    return 0;
}

PTHREAD_API
int
pthread_condattr_setclock (
    pthread_condattr_t *Attribute,
    int Clock
    )

/*++

Routine Description:

    This routine sets the clock used for condition variable timed waits.

Arguments:

    Attribute - Supplies a pointer to the condition variable attribute
        structure.

    Clock - Supplies the clock to use when performing timed waits.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_CONDITION_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_CONDITION_ATTRIBUTE)Attribute;
    if ((Clock == CLOCK_REALTIME) || (Clock == CLOCK_REALTIME_COARSE)) {
        AttributeInternal->Flags &= ~PTHREAD_CONDITION_CLOCK_MONOTONIC;

    } else if ((Clock == CLOCK_MONOTONIC) ||
               (Clock == CLOCK_MONOTONIC_COARSE)) {

        AttributeInternal->Flags |= PTHREAD_CONDITION_CLOCK_MONOTONIC;

    } else {
        return EINVAL;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ClpPulseCondition (
    PPTHREAD_CONDITION Condition,
    UINTN Count
    )

/*++

Routine Description:

    This routine wakes the given number of threads blocked on the condition
    variable.

Arguments:

    Condition - Supplies a pointer to the condition variable to wake threads on.

    Count - Supplies the number of threads to wake.

Return Value:

    0 always.

--*/

{

    ULONG Operation;
    ULONG ThreadCount;

    //
    // Change the value so everyone in the process of waiting fails once they
    // get into the kernel.
    //

    RtlAtomicAdd32(&(Condition->State), 1 << PTHREAD_CONDITION_COUNTER_SHIFT);
    ThreadCount = Count;
    Operation = UserLockWake;
    if ((Condition->State & PTHREAD_CONDITION_SHARED) == 0) {
        Operation |= USER_LOCK_PRIVATE;
    }

    OsUserLock(&(Condition->State), Operation, &ThreadCount, 0);
    return 0;
}

int
ClpWaitOnCondition (
    PPTHREAD_CONDITION Condition,
    pthread_mutex_t *Mutex,
    const struct timespec *AbsoluteTimeout
    )

/*++

Routine Description:

    This routine unlocks the given mutex, blocks until the given condition
    variable is signaled, and then reacquires the mutex.

Arguments:

    Condition - Supplies a pointer to the condition variable to wait on.

    Mutex - Supplies a pointer to the mutex to operate on.

    AbsoluteTimeout - Supplies an optional pointer to an absolute time after
        which the wait should return anyway.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Clock;
    KSTATUS KernelStatus;
    ULONG OldState;
    ULONG Operation;
    ULONG TimeoutInMilliseconds;

    //
    // Make this a cancellation point.
    //

    pthread_testcancel();

    //
    // Snap the old counter value before unlocking the mutex so that the
    // kernel will return immediately if the condition is signaled in between
    // unlocking the mutex and going to sleep.
    //

    OldState = Condition->State;

    //
    // Unlock the mutex and perform the wait.
    //

    pthread_mutex_unlock(Mutex);
    Operation = UserLockWait;
    if ((OldState & PTHREAD_CONDITION_SHARED) == 0) {
        Operation |= USER_LOCK_PRIVATE;
    }

    //
    // If a signal is delivered, the thread is to continue waiting on the
    // condition after the signal handler completes. Do not take another snap
    // of the counter, as this should still be waiting on the original
    // condition.
    //

    do {
        if (AbsoluteTimeout != NULL) {
            Clock = CLOCK_REALTIME;
            if ((OldState & PTHREAD_CONDITION_CLOCK_MONOTONIC) != 0) {
                Clock = CLOCK_MONOTONIC;
            }

            TimeoutInMilliseconds =
                            ClpConvertAbsoluteTimespecToRelativeMilliseconds(
                                                               AbsoluteTimeout,
                                                               Clock);

        } else {
            TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
        }

        KernelStatus = OsUserLock(&(Condition->State),
                                  Operation,
                                  &OldState,
                                  TimeoutInMilliseconds);

    } while (KernelStatus == STATUS_INTERRUPTED);

    pthread_mutex_lock(Mutex);
    if (KernelStatus == STATUS_TIMEOUT) {
        return ETIMEDOUT;
    }

    return 0;
}

