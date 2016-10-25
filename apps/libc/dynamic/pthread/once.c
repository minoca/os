/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    once.c

Abstract:

    This module implements support for POSIX threads once objects.

Author:

    Evan Green 1-May-2015

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

#define PTHREAD_ONCE_NOT_STARTED 0
#define PTHREAD_ONCE_RUNNING 1
#define PTHREAD_ONCE_COMPLETE 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void
ClpCleanUpCanceledOnce (
    void *Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PTHREAD_API
int
pthread_once (
    pthread_once_t *Once,
    void (*Routine)(void)
    )

/*++

Routine Description:

    This routine can be called by any thread in the process. The first call
    to this routine will execute the given method. All others calls will do
    nothing. On return from this routine, the routine will have completed
    executing. If the routine is a cancellation point and is canceled, then
    the effect will be as if the routine was never called.

Arguments:

    Once - Supplies a pointer to the initialized once object. Initialize it
        with the value PTHREAD_ONCE_INIT.

    Routine - Supplies a pointer to the routine to be called exactly once.

Return Value:

    0 on success.

    EINVAL if the given once object or routine is invalid.

--*/

{

    __pthread_cleanup_t Cleanup;
    ULONG Count;
    ULONG OldValue;

    OldValue = *Once;
    if ((OldValue > PTHREAD_ONCE_COMPLETE) || (Routine == NULL)) {
        return EINVAL;
    }

    if (OldValue == PTHREAD_ONCE_COMPLETE) {
        return 0;
    }

    while (TRUE) {

        //
        // Try to switch it to running, from either running or not started.
        //

        OldValue = RtlAtomicCompareExchange32((PULONG)Once,
                                              PTHREAD_ONCE_RUNNING,
                                              OldValue);

        if (OldValue == PTHREAD_ONCE_COMPLETE) {
            break;
        }

        //
        // If this thread won, then call the routine.
        //

        if (OldValue == PTHREAD_ONCE_NOT_STARTED) {

            //
            // If the thread exits during the init routine, the once object
            // will need to be reset to not started.
            //

            __pthread_cleanup_push(&Cleanup, ClpCleanUpCanceledOnce, Once);
            Routine();
            *Once = PTHREAD_ONCE_COMPLETE;
            __pthread_cleanup_pop(&Cleanup, 0);
            Count = MAX_ULONG;

            //
            // Wake up any waiters.
            //

            OsUserLock((PULONG)Once, UserLockWake, &Count, 0);
            break;
        }

        //
        // Wait for the value to change.
        //

        OsUserLock((PULONG)Once,
                   UserLockWait,
                   &OldValue,
                   SYS_WAIT_TIME_INDEFINITE);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

void
ClpCleanUpCanceledOnce (
    void *Parameter
    )

/*++

Routine Description:

    This routine cleans up a once object that was in progress and got canceled.

Arguments:

    Parameter - Supplies the parameter, in this case a pointer to the once
        object.

Return Value:

    None.

--*/

{

    ULONG Count;
    pthread_once_t *Once;

    //
    // Try to flip it back from running to not started.
    //

    Once = Parameter;
    RtlAtomicCompareExchange32((PULONG)Once,
                               PTHREAD_ONCE_NOT_STARTED,
                               PTHREAD_ONCE_RUNNING);

    //
    // Wake everyone up too.
    //

    Count = MAX_ULONG;
    OsUserLock(Once, UserLockWake, &Count, 0);
    return;
}

