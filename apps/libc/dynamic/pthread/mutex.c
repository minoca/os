/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mutex.c

Abstract:

    This module implements mutex support functions for the POSIX thread library.

Author:

    Evan Green 27-Apr-2015

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

#define PTHREAD_MUTEX_STATE_UNLOCKED 0
#define PTHREAD_MUTEX_STATE_LOCKED 1
#define PTHREAD_MUTEX_STATE_LOCKED_WITH_WAITERS 2
#define PTHREAD_MUTEX_STATE_MASK 0x00000007

#define PTHREAD_MUTEX_STATE_COUNTER_SHIFT 4
#define PTHREAD_MUTEX_STATE_COUNTER_MASK 0x0000FFFF
#define PTHREAD_MUTEX_STATE_COUNTER_MAX 0x0000FFFF

#define PTHREAD_MUTEX_STATE_SHARED 0x20000000
#define PTHREAD_MUTEX_STATE_RECURSIVE 0x40000000
#define PTHREAD_MUTEX_STATE_ERRORCHECK 0x80000000
#define PTHREAD_MUTEX_STATE_TYPE_MASK 0xC0000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ClpAcquireMutexWithTimeout (
    PPTHREAD_MUTEX Mutex,
    const struct timespec *AbsoluteTimeout,
    clockid_t Clock
    );

int
ClpAcquireNormalMutex (
    PPTHREAD_MUTEX Mutex,
    ULONG Shared,
    const struct timespec *AbsoluteTimeout,
    INT Clock
    );

int
ClpTryToAcquireNormalMutex (
    PPTHREAD_MUTEX Mutex,
    ULONG Shared
    );

VOID
ClpReleaseNormalMutex (
    PPTHREAD_MUTEX Mutex,
    ULONG Shared
    );

int
ClpMutexIncrementAcquireCount (
    PPTHREAD_MUTEX Mutex
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PTHREAD_API
int
pthread_mutex_init (
    pthread_mutex_t *Mutex,
    const pthread_mutexattr_t *Attribute
    )

/*++

Routine Description:

    This routine initializes a mutex.

Arguments:

    Mutex - Supplies a pointer to the mutex to initialize.

    Attribute - Supplies an optional pointer to the initialized attributes to
        set in the mutex.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX_ATTRIBUTE AttributeInternal;
    ULONG Flags;
    PPTHREAD_MUTEX MutexInternal;
    ULONG State;

    MutexInternal = (PPTHREAD_MUTEX)Mutex;

    ASSERT(sizeof(pthread_mutex_t) >= sizeof(PTHREAD_MUTEX));

    memset(MutexInternal, 0, sizeof(PTHREAD_MUTEX));
    if (Attribute == NULL) {
        return 0;
    }

    AttributeInternal = (PPTHREAD_MUTEX_ATTRIBUTE)Attribute;
    Flags = AttributeInternal->Flags;
    State = 0;
    if ((Flags & PTHREAD_MUTEX_SHARED) != 0) {
        State |= PTHREAD_MUTEX_STATE_SHARED;
    }

    switch (Flags & PTHREAD_MUTEX_TYPE_MASK) {
    case PTHREAD_MUTEX_NORMAL:
        break;

    case PTHREAD_MUTEX_RECURSIVE:
        State |= PTHREAD_MUTEX_STATE_RECURSIVE;
        break;

    case PTHREAD_MUTEX_ERRORCHECK:
        State |= PTHREAD_MUTEX_STATE_ERRORCHECK;
        break;

    default:
        return EINVAL;
    }

    MutexInternal->State = State;
    return 0;
}

PTHREAD_API
int
pthread_mutex_destroy (
    pthread_mutex_t *Mutex
    )

/*++

Routine Description:

    This routine destroys a mutex.

Arguments:

    Mutex - Supplies a pointer to the mutex to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX MutexInternal;
    int Status;

    //
    // Try to acquire the lock to ensure it's not invalid and not already
    // locked.
    //

    Status = pthread_mutex_trylock(Mutex);
    if (Status != 0) {
        return Status;
    }

    MutexInternal = (PPTHREAD_MUTEX)Mutex;
    MutexInternal->State = -1;
    return Status;
}

PTHREAD_API
int
pthread_mutex_lock (
    pthread_mutex_t *Mutex
    )

/*++

Routine Description:

    This routine acquires a mutex.

Arguments:

    Mutex - Supplies a pointer to the mutex to acquire.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX MutexInternal;
    ULONG MutexType;
    ULONG Shared;

    MutexInternal = (PPTHREAD_MUTEX)Mutex;
    MutexType = MutexInternal->State & PTHREAD_MUTEX_STATE_TYPE_MASK;
    Shared = MutexInternal->State & PTHREAD_MUTEX_STATE_SHARED;
    if (MutexType == 0) {
        if (ClpTryToAcquireNormalMutex(MutexInternal, Shared) == 0) {
            return 0;
        }
    }

    return ClpAcquireMutexWithTimeout(MutexInternal, NULL, 0);
}

PTHREAD_API
int
pthread_mutex_unlock (
    pthread_mutex_t *Mutex
    )

/*++

Routine Description:

    This routine releases a mutex.

Arguments:

    Mutex - Supplies a pointer to the mutex to release.

Return Value:

    0 on success.

    EPERM if this thread is not the thread that originally acquire the mutex.

--*/

{

    ULONG Count;
    ULONG Counter;
    PPTHREAD_MUTEX MutexInternal;
    ULONG MutexType;
    ULONG OldState;
    ULONG Operation;
    ULONG ReleasedState;
    ULONG Shared;
    UINTN ThreadId;

    MutexInternal = (PPTHREAD_MUTEX)Mutex;
    MutexType = MutexInternal->State & PTHREAD_MUTEX_STATE_TYPE_MASK;
    Shared = MutexInternal->State & PTHREAD_MUTEX_STATE_SHARED;

    //
    // Do a fast release for normal locks.
    //

    if (MutexType == 0) {
        ClpReleaseNormalMutex(MutexInternal, Shared);
        return 0;
    }

    //
    // Check the ownership of the mutex.
    //

    ThreadId = OsGetThreadId();
    if (ThreadId != MutexInternal->Owner) {
        return EPERM;
    }

    //
    // If the counter is non-zero, just decrement it.
    //

    Counter = (MutexInternal->State >> PTHREAD_MUTEX_STATE_COUNTER_SHIFT) &
              PTHREAD_MUTEX_STATE_COUNTER_MASK;

    if (Counter != 0) {
        RtlAtomicAdd32(&(MutexInternal->State),
                       0 - (1 << PTHREAD_MUTEX_STATE_COUNTER_SHIFT));

        return 0;
    }

    //
    // Set the state to free, and release any waiters if contended.
    //

    MutexInternal->Owner = 0;
    ReleasedState = MutexType | Shared | PTHREAD_MUTEX_STATE_UNLOCKED;
    OldState = RtlAtomicExchange32(&(MutexInternal->State), ReleasedState);
    if ((OldState & PTHREAD_MUTEX_STATE_MASK) ==
        PTHREAD_MUTEX_STATE_LOCKED_WITH_WAITERS) {

        Operation = UserLockWake;
        if (Shared == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        Count = 1;
        OsUserLock(&(MutexInternal->State), Operation, &Count, 0);
    }

    return 0;
}

PTHREAD_API
int
pthread_mutex_trylock (
    pthread_mutex_t *Mutex
    )

/*++

Routine Description:

    This routine attempts to acquire the given mutex once.

Arguments:

    Mutex - Supplies a pointer to the mutex to attempt to acquire.

Return Value:

    0 on success.

    EBUSY if the mutex is already held by another thread and this is an error
    checking mutex.

--*/

{

    ULONG Locked;
    PPTHREAD_MUTEX MutexInternal;
    ULONG MutexType;
    ULONG OldState;
    ULONG Shared;
    UINTN ThreadId;
    ULONG Unlocked;

    MutexInternal = (PPTHREAD_MUTEX)Mutex;
    MutexType = MutexInternal->State & PTHREAD_MUTEX_STATE_TYPE_MASK;
    Shared = MutexInternal->State & PTHREAD_MUTEX_STATE_SHARED;

    //
    // Handle the normal fast path.
    //

    if (MutexType == 0) {
        return ClpTryToAcquireNormalMutex(MutexInternal, Shared);
    }

    //
    // Determine if the thread already owns the mutex.
    //

    ThreadId = OsGetThreadId();
    if (MutexInternal->Owner == ThreadId) {
        if (MutexType == PTHREAD_MUTEX_STATE_ERRORCHECK) {
            return EBUSY;
        }

        return ClpMutexIncrementAcquireCount(MutexInternal);
    }

    Unlocked = MutexType | Shared | PTHREAD_MUTEX_STATE_UNLOCKED;
    Locked = MutexType | Shared | PTHREAD_MUTEX_STATE_LOCKED;

    //
    // Try to go from unlocked to locked, which is the only case under which
    // this attempt could succeed.
    //

    OldState = RtlAtomicCompareExchange32(&(MutexInternal->State),
                                          Locked,
                                          Unlocked);

    //
    // If acquired, set the owner and return happily.
    //

    if (OldState == Unlocked) {
        MutexInternal->Owner = ThreadId;
        return 0;
    }

    return EBUSY;
}

PTHREAD_API
int
pthread_mutex_timedlock (
    pthread_mutex_t *Mutex,
    const struct timespec *AbsoluteTimeout
    )

/*++

Routine Description:

    This routine attempts to acquire a mutex, giving up after a specified
    deadline.

Arguments:

    Mutex - Supplies a pointer to the mutex to acquire.

    AbsoluteTimeout - Supplies the absolute timeout after which the attempt
        shall fail and time out.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Status;

    Status = ClpAcquireMutexWithTimeout((PPTHREAD_MUTEX)Mutex,
                                        AbsoluteTimeout,
                                        CLOCK_REALTIME);

    return Status;
}

PTHREAD_API
int
pthread_mutexattr_init (
    pthread_mutexattr_t *Attribute
    )

/*++

Routine Description:

    This routine initializes a mutex attribute object.

Arguments:

    Attribute - Supplies a pointer to the attribute to initialize.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX_ATTRIBUTE MutexAttribute;

    MutexAttribute = (PPTHREAD_MUTEX_ATTRIBUTE)Attribute;
    MutexAttribute->Flags = 0;
    return 0;
}

PTHREAD_API
int
pthread_mutexattr_destroy (
    pthread_mutexattr_t *Attribute
    )

/*++

Routine Description:

    This routine destroys a mutex attribute object.

Arguments:

    Attribute - Supplies a pointer to the attribute to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX_ATTRIBUTE MutexAttribute;

    MutexAttribute = (PPTHREAD_MUTEX_ATTRIBUTE)Attribute;
    MutexAttribute->Flags = -1;
    return 0;
}

PTHREAD_API
int
pthread_mutexattr_gettype (
    const pthread_mutexattr_t *Attribute,
    int *Type
    )

/*++

Routine Description:

    This routine returns the mutex type given an attribute that was previously
    set.

Arguments:

    Attribute - Supplies a pointer to the attribute to get the type from.

    Type - Supplies a pointer where the mutex type will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX_ATTRIBUTE MutexAttribute;
    INT MutexType;

    MutexAttribute = (PPTHREAD_MUTEX_ATTRIBUTE)Attribute;
    MutexType = MutexAttribute->Flags & PTHREAD_MUTEX_TYPE_MASK;
    if ((MutexType < PTHREAD_MUTEX_NORMAL) ||
        (MutexType > PTHREAD_MUTEX_RECURSIVE)) {

        return EINVAL;
    }

    *Type = MutexType;
    return 0;
}

PTHREAD_API
int
pthread_mutexattr_settype (
    pthread_mutexattr_t *Attribute,
    int Type
    )

/*++

Routine Description:

    This routine sets a mutex type in the given mutex attributes object.

Arguments:

    Attribute - Supplies a pointer to the attribute to set the type in.

    Type - Supplies the mutex type to set. See PTHREAD_MUTEX_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX_ATTRIBUTE MutexAttribute;

    MutexAttribute = (PPTHREAD_MUTEX_ATTRIBUTE)Attribute;
    if ((Type < PTHREAD_MUTEX_NORMAL) || (Type > PTHREAD_MUTEX_RECURSIVE)) {
        return EINVAL;
    }

    MutexAttribute->Flags &= ~PTHREAD_MUTEX_TYPE_MASK;
    MutexAttribute->Flags |= Type;
    return 0;
}

PTHREAD_API
int
pthread_mutexattr_getpshared (
    const pthread_mutexattr_t *Attribute,
    int *Shared
    )

/*++

Routine Description:

    This routine returns the mutex sharing type given an attribute that was
    previously set.

Arguments:

    Attribute - Supplies a pointer to the attribute to get the sharing
    information from.

    Shared - Supplies a pointer where the sharing type will be returned on
        success. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX_ATTRIBUTE MutexAttribute;

    MutexAttribute = (PPTHREAD_MUTEX_ATTRIBUTE)Attribute;
    *Shared = PTHREAD_PROCESS_PRIVATE;
    if ((MutexAttribute->Flags & PTHREAD_MUTEX_SHARED) != 0) {
        *Shared = PTHREAD_PROCESS_SHARED;
    }

    return 0;
}

PTHREAD_API
int
pthread_mutexattr_setpshared (
    pthread_mutexattr_t *Attribute,
    int Shared
    )

/*++

Routine Description:

    This routine sets a mutex sharing type in the given mutex attributes object.

Arguments:

    Attribute - Supplies a pointer to the attribute to set the type in.

    Shared - Supplies the mutex type to set. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_MUTEX_ATTRIBUTE MutexAttribute;
    int Status;

    MutexAttribute = (PPTHREAD_MUTEX_ATTRIBUTE)Attribute;
    Status = 0;
    switch (Shared) {
    case PTHREAD_PROCESS_SHARED:
        MutexAttribute->Flags |= PTHREAD_MUTEX_SHARED;
        break;

    case PTHREAD_PROCESS_PRIVATE:
        MutexAttribute->Flags &= ~PTHREAD_MUTEX_SHARED;
        break;

    default:
        Status = EINVAL;
        break;
    }

    return Status;
}

ULONG
ClpConvertAbsoluteTimespecToRelativeMilliseconds (
    const struct timespec *AbsoluteTime,
    int Clock
    )

/*++

Routine Description:

    This routine converts an absolute timespec structure into a number of
    milliseconds from now.

Arguments:

    AbsoluteTime - Supplies a pointer to the absolute timespec to convert.

    Clock - Supplies the clock to query from.

Return Value:

    Returns the number of milliseconds from now the timespec expires in.

    0 if the absolute time is in the past.

--*/

{

    struct timespec Delta;
    ULONG Result;
    time_t Seconds;
    int Status;

    Status = clock_gettime(Clock, &Delta);
    if (Status != 0) {
        return 0;
    }

    Delta.tv_sec = AbsoluteTime->tv_sec - Delta.tv_sec;
    Delta.tv_nsec = AbsoluteTime->tv_nsec - Delta.tv_nsec;
    if (Delta.tv_nsec < 0) {
        Delta.tv_sec -= 1;
        Delta.tv_nsec += NANOSECONDS_PER_SECOND;
    }

    if ((Delta.tv_nsec < 0) || (Delta.tv_sec < 0)) {
        return 0;
    }

    if (Delta.tv_nsec >= NANOSECONDS_PER_SECOND) {
          Seconds = Delta.tv_nsec / NANOSECONDS_PER_SECOND;
          Delta.tv_sec += Seconds;
          Delta.tv_nsec -= (Seconds * NANOSECONDS_PER_SECOND);
    }

    Status = ClpConvertSpecificTimeoutToSystemTimeout(&Delta, &Result);
    if (Status != 0) {
        return 0;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ClpAcquireMutexWithTimeout (
    PPTHREAD_MUTEX Mutex,
    const struct timespec *AbsoluteTimeout,
    clockid_t Clock
    )

/*++

Routine Description:

    This routine attempts to acquire a mutex with a timeout.

Arguments:

    Mutex - Supplies a pointer to the mutex to acquire.

    AbsoluteTimeout - Supplies an optional pointer to the deadline in absolute
        time after which the operation should time out and fail.

    Clock - Supplies the clock to measure the timeout against.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    KSTATUS KernelStatus;
    ULONG Locked;
    ULONG LockedWithWaiters;
    ULONG MutexType;
    ULONG NewState;
    ULONG OldState;
    ULONG Operation;
    ULONG Shared;
    UINTN ThreadId;
    ULONG TimeoutInMilliseconds;
    ULONG Unlocked;

    OldState = Mutex->State;
    MutexType = Mutex->State & PTHREAD_MUTEX_STATE_TYPE_MASK;
    Shared = Mutex->State & PTHREAD_MUTEX_STATE_SHARED;

    //
    // Handle the fast-ish path for normal types.
    //

    if (MutexType == 0) {
        return ClpAcquireNormalMutex(Mutex, Shared, AbsoluteTimeout, Clock);
    }

    //
    // Determine if the thread already owns the mutex.
    //

    ThreadId = OsGetThreadId();
    if (ThreadId == Mutex->Owner) {
        if (MutexType == PTHREAD_MUTEX_STATE_ERRORCHECK) {
            return EDEADLK;
        }

        return ClpMutexIncrementAcquireCount(Mutex);
    }

    Unlocked = MutexType | Shared | PTHREAD_MUTEX_STATE_UNLOCKED;
    Locked = MutexType | Shared | PTHREAD_MUTEX_STATE_LOCKED;
    LockedWithWaiters = MutexType | Shared |
                        PTHREAD_MUTEX_STATE_LOCKED_WITH_WAITERS;

    //
    // Take an optimistic stab at acquiring the lock assuming it's uncontended.
    // If this works, then it gets left as locked (without waiters), which
    // makes the release operation lightweight.
    //

    if (OldState == Unlocked) {
        OldState = RtlAtomicCompareExchange32(&(Mutex->State),
                                              Locked,
                                              Unlocked);

        if (OldState == Unlocked) {
            Mutex->Owner = ThreadId;
            return 0;
        }
    }

    //
    // Contend for the mutex.
    //

    while (TRUE) {
        if (OldState == Unlocked) {

            //
            // Attempt to go from unlocked to locked with waiters. Being inside
            // this loop means there are definitely other threads bouncing
            // around here, so going directly to locked with waiters saves them
            // the trouble of having to go from locked to locked with waiters.
            //

            OldState = RtlAtomicCompareExchange32(&(Mutex->State),
                                                  LockedWithWaiters,
                                                  OldState);

            if (OldState == Unlocked) {
                Mutex->Owner = ThreadId;
                return 0;
            }

            continue;

        //
        // If the mutex is locked (without waiters), set it to locked with
        // with waiters to tell whoever does have it that they need to wake
        // this thread up. The comparison cannot simply be against the locked
        // local variable because a recursive lock may have added to the
        // counter.
        //

        } else if ((OldState & PTHREAD_MUTEX_STATE_MASK) ==
                   PTHREAD_MUTEX_STATE_LOCKED) {

            NewState = (OldState & ~PTHREAD_MUTEX_STATE_MASK) |
                       PTHREAD_MUTEX_STATE_LOCKED_WITH_WAITERS;

            OldState = RtlAtomicCompareExchange32(&(Mutex->State),
                                                  NewState,
                                                  OldState);

            continue;
        }

        ASSERT((OldState & PTHREAD_MUTEX_STATE_MASK) ==
               PTHREAD_MUTEX_STATE_LOCKED_WITH_WAITERS);

        if (AbsoluteTimeout != NULL) {
            TimeoutInMilliseconds =
                           ClpConvertAbsoluteTimespecToRelativeMilliseconds(
                                                              AbsoluteTimeout,
                                                              Clock);

            if (TimeoutInMilliseconds == 0) {
                return ETIMEDOUT;
            }

        } else {
            TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
        }

        //
        // Call the kernel to go down for a wait.
        //

        Operation = UserLockWait;
        if (Shared == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        KernelStatus = OsUserLock(&(Mutex->State),
                                  Operation,
                                  &OldState,
                                  TimeoutInMilliseconds);

        if (KernelStatus == STATUS_TIMEOUT) {
            return ETIMEDOUT;
        }

        OldState = Mutex->State;
    }

    //
    // This code is never reached.
    //

    ASSERT(FALSE);

    return EINVAL;
}

int
ClpAcquireNormalMutex (
    PPTHREAD_MUTEX Mutex,
    ULONG Shared,
    const struct timespec *AbsoluteTimeout,
    INT Clock
    )

/*++

Routine Description:

    This routine acquires a normal mutex. That is one without any recursive or
    error checking attributes.

Arguments:

    Mutex - Supplies a pointer to the mutex to acquire.

    Shared - Supplies the shared flag for the mutex.

    AbsoluteTimeout - Supplies an optional pointer to the absolute timeout for
        the operation.

    Clock - Supplies the clock source.

Return Value:

    0 if the lock was acquired.

    Returns an error code on failure or timeout.

--*/

{

    KSTATUS KernelStatus;
    ULONG LockedWithWaiters;
    ULONG OldState;
    ULONG Operation;
    ULONG TimeoutInMilliseconds;
    ULONG Unlocked;

    //
    // Give it a quick fast attempt first.
    //

    if (ClpTryToAcquireNormalMutex(Mutex, Shared) == 0) {
        return 0;
    }

    LockedWithWaiters = Shared | PTHREAD_MUTEX_STATE_LOCKED_WITH_WAITERS;
    Unlocked = Shared | PTHREAD_MUTEX_STATE_UNLOCKED;

    //
    // Set the lock to acquired with waiters (since the quick attempt above
    // failed).
    //

    while (TRUE) {
        OldState = RtlAtomicExchange32(&(Mutex->State), LockedWithWaiters);

        //
        // If the lock was acquired, break out for success.
        //

        if (OldState == Unlocked) {
            break;
        }

        if (AbsoluteTimeout != NULL) {
            TimeoutInMilliseconds =
                          ClpConvertAbsoluteTimespecToRelativeMilliseconds(
                                                             AbsoluteTimeout,
                                                             Clock);

            if (TimeoutInMilliseconds == 0) {
                return ETIMEDOUT;
            }

        } else {
            TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
        }

        //
        // Call the kernel to go down for a wait.
        //

        Operation = UserLockWait;
        if (Shared == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        OldState = LockedWithWaiters;
        KernelStatus = OsUserLock(&(Mutex->State),
                                  Operation,
                                  &OldState,
                                  TimeoutInMilliseconds);

        if (KernelStatus == STATUS_TIMEOUT) {
            return ETIMEDOUT;
        }
    }

    return 0;
}

int
ClpTryToAcquireNormalMutex (
    PPTHREAD_MUTEX Mutex,
    ULONG Shared
    )

/*++

Routine Description:

    This routine performs a single non-blocking attempt at acquiring a mutex
    without any fancy attributes like error checking or recursion.

Arguments:

    Mutex - Supplies a pointer to the mutex to attempt to acquire.

    Shared - Supplies the shared flag for the mutex.

Return Value:

    0 if the mutex was acquired.

    EBUSY if the mutex was not successfully acquired.

--*/

{

    ULONG Locked;
    ULONG OldState;
    ULONG Unlocked;

    Locked = Shared | PTHREAD_MUTEX_STATE_LOCKED;
    Unlocked = Shared | PTHREAD_MUTEX_STATE_UNLOCKED;
    OldState = RtlAtomicCompareExchange32(&(Mutex->State), Locked, Unlocked);
    if (OldState == Unlocked) {
        return 0;
    }

    return EBUSY;
}

VOID
ClpReleaseNormalMutex (
    PPTHREAD_MUTEX Mutex,
    ULONG Shared
    )

/*++

Routine Description:

    This routine releases a mutex without any recursive or error checking
    attributes.

Arguments:

    Mutex - Supplies a pointer to the mutex to release.

    Shared - Supplies the shared flag for the mutex.

Return Value:

    None.

--*/

{

    ULONG Count;
    ULONG LockedWithWaiters;
    ULONG OldState;
    ULONG Operation;
    ULONG Unlocked;

    Unlocked = Shared | PTHREAD_MUTEX_STATE_UNLOCKED;
    LockedWithWaiters = Shared | PTHREAD_MUTEX_STATE_LOCKED_WITH_WAITERS;

    //
    // Exchange out the state to unlocked. If it had waiters, wake them up.
    //

    OldState = RtlAtomicExchange32(&(Mutex->State), Unlocked);
    if (OldState == LockedWithWaiters) {
        Operation = UserLockWake;
        if (Shared == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        Count = 1;
        OsUserLock(&(Mutex->State), Operation, &Count, 0);
    }

    return;
}

int
ClpMutexIncrementAcquireCount (
    PPTHREAD_MUTEX Mutex
    )

/*++

Routine Description:

    This routine increments the acquire count on a mutex that's already held
    by the current thread.

Arguments:

    Mutex - Supplies a pointer to the mutex to increment.

Return Value:

    0 on success.

    EAGAIN if the maximum number of recursive acquires was reached (the
    internal counter would overflow).

--*/

{

    ULONG Count;

    Count = (Mutex->State >> PTHREAD_MUTEX_STATE_COUNTER_SHIFT) &
            PTHREAD_MUTEX_STATE_COUNTER_MASK;

    if (Count == PTHREAD_MUTEX_STATE_COUNTER_MAX) {
        return EAGAIN;
    }

    //
    // Since other threads might be atomically changing the lower bits, the
    // atomic add is necessary.
    //

    RtlAtomicAdd32(&(Mutex->State), 1 << PTHREAD_MUTEX_STATE_COUNTER_SHIFT);
    return 0;
}

