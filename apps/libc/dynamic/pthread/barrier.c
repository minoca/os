/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    barrier.c

Abstract:

    This module implements barrier support functions for the POSIX thread
    library.

Author:

    Chris Stevens 1-Jul-2016

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

#define PTHREAD_BARRIER_SHARED        0x00000001
#define PTHREAD_BARRIER_FLAGS         0x00000001
#define PTHREAD_BARRIER_COUNTER_SHIFT 1
#define PTHREAD_BARRIER_COUNTER_MASK  (~PTHREAD_BARRIER_FLAGS)

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
pthread_barrier_init (
    pthread_barrier_t *Barrier,
    const pthread_barrierattr_t *Attribute,
    unsigned Count
    )

/*++

Routine Description:

    This routine initializes the given POSIX thread barrier with the given
    attributes and thread count.

Arguments:

    Barrier - Supplies a pointer to the POSIX thread barrier to be initialized.

    Attribute - Supplies an optional pointer to the attribute to use for
        initializing the barrier.

    Count - Supplies the number of threads that must wait on the barrier for it
        to be satisfied.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_BARRIER_ATTRIBUTE AttributeInternal;
    PPTHREAD_BARRIER BarrierInternal;
    pthread_mutexattr_t MutexAttribute;
    pthread_mutexattr_t *MutexAttributePointer;
    INT Result;

    ASSERT(sizeof(pthread_barrier_t) >= sizeof(PTHREAD_BARRIER));

    if (Count == 0) {
        return EINVAL;
    }

    BarrierInternal = (PPTHREAD_BARRIER)Barrier;
    MutexAttributePointer = NULL;
    if (Attribute == NULL) {
        BarrierInternal->State = 0;

    } else {
        AttributeInternal = (PPTHREAD_BARRIER_ATTRIBUTE)Attribute;
        BarrierInternal->State = AttributeInternal->Flags &
                                 PTHREAD_BARRIER_FLAGS;

        if ((BarrierInternal->State & PTHREAD_BARRIER_SHARED) != 0) {
            MutexAttributePointer = &MutexAttribute;
            pthread_mutexattr_init(MutexAttributePointer);
            pthread_mutexattr_setpshared(MutexAttributePointer,
                                         PTHREAD_PROCESS_SHARED);
        }
    }

    Result = pthread_mutex_init(&(BarrierInternal->Mutex),
                                MutexAttributePointer);

    if (Result != 0) {
        return Result;
    }

    BarrierInternal->WaitingThreadCount = 0;
    BarrierInternal->ThreadCount = Count;
    return 0;
}

PTHREAD_API
int
pthread_barrier_destroy (
    pthread_barrier_t *Barrier
    )

/*++

Routine Description:

    This routine destroys the given POSIX thread barrier.

Arguments:

    Barrier - Supplies a pointer to the POSIX thread barrier to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_BARRIER BarrierInternal;

    BarrierInternal = (PPTHREAD_BARRIER)Barrier;
    BarrierInternal->State = MAX_ULONG;
    BarrierInternal->ThreadCount = 0;
    BarrierInternal->WaitingThreadCount = 0;
    pthread_mutex_destroy(&(BarrierInternal->Mutex));
    return 0;
}

PTHREAD_API
int
pthread_barrier_wait (
    pthread_barrier_t *Barrier
    )

/*++

Routine Description:

    This routine blocks untils the required number of threads have waited on
    the barrier. Upon success, an arbitrary thread will receive
    PTHREAD_BARRIER_SERIAL_THREAD as a return value; the rest will receive 0.
    This routine does not get interrupted by signals and will continue to block
    after a signal is handled.

Arguments:

    Barrier - Supplies a pointer to the POSIX thread barrier.

Return Value:

    0 or PTHREAD_BARRIER_SERIAL_THREAD on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_BARRIER BarrierInternal;
    KSTATUS KernelStatus;
    ULONG OldState;
    ULONG Operation;
    INT Status;
    ULONG ThreadCount;

    BarrierInternal = (PPTHREAD_BARRIER)Barrier;

    //
    // Acquire the mutex and increment the waiting thread count. If this
    // thread's wait satisfies the barrier, then attempt to wake all of the
    // other waiting threads.
    //

    pthread_mutex_lock(&(BarrierInternal->Mutex));
    BarrierInternal->WaitingThreadCount += 1;
    if (BarrierInternal->WaitingThreadCount >= BarrierInternal->ThreadCount) {
        Operation = UserLockWake;
        if ((BarrierInternal->State & PTHREAD_BARRIER_SHARED) == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        ThreadCount = MAX_ULONG;
        KernelStatus = OsUserLock(&(BarrierInternal->State),
                                  Operation,
                                  &ThreadCount,
                                  0);

        //
        // On success, this thread gets the unique serialization return value.
        // Also, reset the barrier to the initialization state and increment
        // the state, so that any threads about to wait on this now satisfied
        // barrier will fail in the kernel.
        //

        if (KSUCCESS(KernelStatus)) {
            Status = PTHREAD_BARRIER_SERIAL_THREAD;
            BarrierInternal->WaitingThreadCount = 0;
            RtlAtomicAdd32(&(BarrierInternal->State),
                           1 << PTHREAD_BARRIER_COUNTER_SHIFT);

        } else {
            Status = ClConvertKstatusToErrorNumber(KernelStatus);
        }

        pthread_mutex_unlock(&(BarrierInternal->Mutex));

    //
    // Otherwise, wait on the current state until the required number of
    // threads arrive.
    //

    } else {
        OldState = BarrierInternal->State;
        pthread_mutex_unlock(&(BarrierInternal->Mutex));
        Operation = UserLockWait;
        if ((OldState & PTHREAD_BARRIER_SHARED) == 0) {
            Operation |= USER_LOCK_PRIVATE;
        }

        //
        // If a signal interrupts the wait, the barrier should continue waiting
        // after the signal is handled.
        //

        do {
            KernelStatus = OsUserLock(&(BarrierInternal->State),
                                      Operation,
                                      &OldState,
                                      SYS_WAIT_TIME_INDEFINITE);

        } while (KernelStatus == STATUS_INTERRUPTED);

        //
        // The wait may have failed immediately if the barrier was satisfied
        // between this thread releasing the lock and executing the wait.
        // Convert this failure into success.
        //

        if (KSUCCESS(KernelStatus) ||
            (KernelStatus == STATUS_OPERATION_WOULD_BLOCK)) {

            Status = 0;

        } else {
            Status = ClConvertKstatusToErrorNumber(KernelStatus);
        }
    }

    return Status;
}

PTHREAD_API
int
pthread_barrierattr_init (
    pthread_barrierattr_t *Attribute
    )

/*++

Routine Description:

    This routine initializes a barrier attribute structure.

Arguments:

    Attribute - Supplies a pointer to the barrier attribute structure to
        initialize.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_BARRIER_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_BARRIER_ATTRIBUTE)Attribute;
    AttributeInternal->Flags = 0;
    return 0;
}

PTHREAD_API
int
pthread_barrierattr_destroy (
    pthread_barrierattr_t *Attribute
    )

/*++

Routine Description:

    This routine destroys the given barrier attribute structure.

Arguments:

    Attribute - Supplies a pointer to the barrier attribute to destroy.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_BARRIER_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_BARRIER_ATTRIBUTE)Attribute;
    AttributeInternal->Flags = MAX_ULONG;
    return 0;
}

PTHREAD_API
int
pthread_barrierattr_getpshared (
    const pthread_barrierattr_t *Attribute,
    int *Shared
    )

/*++

Routine Description:

    This routine determines the shared state in a barrier attribute.

Arguments:

    Attribute - Supplies a pointer to the barrier attribute structure.

    Shared - Supplies a pointer where the shared attribute will be returned,
        indicating whether the condition variable is visible across processes.
        See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_BARRIER_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_BARRIER_ATTRIBUTE)Attribute;
    *Shared = PTHREAD_PROCESS_PRIVATE;
    if ((AttributeInternal->Flags & PTHREAD_BARRIER_SHARED) != 0) {
        *Shared = PTHREAD_PROCESS_SHARED;
    }

    return 0;
}

PTHREAD_API
int
pthread_barrierattr_setpshared (
    const pthread_barrierattr_t *Attribute,
    int Shared
    )

/*++

Routine Description:

    This routine sets the shared state in a barrier attribute.

Arguments:

    Attribute - Supplies a pointer to the barrier attribute structure.

    Shared - Supplies the value indicating whether this barrier should be
        visible across processes. See PTHREAD_PROCESS_* definitions.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PPTHREAD_BARRIER_ATTRIBUTE AttributeInternal;

    AttributeInternal = (PPTHREAD_BARRIER_ATTRIBUTE)Attribute;
    if (Shared == PTHREAD_PROCESS_PRIVATE) {
        AttributeInternal->Flags &= ~PTHREAD_BARRIER_SHARED;

    } else if (Shared == PTHREAD_PROCESS_SHARED) {
        AttributeInternal->Flags |= PTHREAD_BARRIER_SHARED;

    } else {
        return EINVAL;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

