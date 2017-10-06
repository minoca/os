/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    semaphore.h

Abstract:

    This header contains definitions for POSIX semaphores.

Author:

    Evan Green 4-May-2015

--*/

#ifndef _SEMAPHORE_H
#define _SEMAPHORE_H

//
// ------------------------------------------------------------------- Includes
//

#include <pthread.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

typedef union {
    char Data[32];
    long int AlignMember;
} sem_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

PTHREAD_API
int
sem_init (
    sem_t *Semaphore,
    int Shared,
    unsigned int Value
    );

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

PTHREAD_API
int
sem_destroy (
    sem_t *Semaphore
    );

/*++

Routine Description:

    This routine releases all resources associated with a POSIX semaphore.

Arguments:

    Semaphore - Supplies a pointer to the semaphore to destroy.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

PTHREAD_API
int
sem_wait (
    sem_t *Semaphore
    );

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

PTHREAD_API
int
sem_timedwait (
    sem_t *Semaphore,
    const struct timespec *AbsoluteTimeout
    );

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

PTHREAD_API
int
sem_trywait (
    sem_t *Semaphore
    );

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

PTHREAD_API
int
sem_getvalue (
    sem_t *Semaphore,
    int *SemaphoreValue
    );

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

PTHREAD_API
int
sem_post (
    sem_t *Semaphore
    );

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

#ifdef __cplusplus

}

#endif
#endif

