/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pthread.c

Abstract:

    This module implements the performance benchmark tests for pthreads.

Author:

    Chris Stevens 8-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void *
PthreadStartRoutine (
    void *Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

void
PthreadMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the pthread performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    int Argument;
    unsigned long long Iterations;
    int JoinThread;
    pthread_t NewThread;
    int Status;

    Iterations = 0;
    JoinThread = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;

    //
    // Determine what the main thread should do with the created thread.
    //

    switch (Test->TestType) {
    case PtTestPthreadJoin:
        JoinThread = 1;
        break;

    case PtTestPthreadDetach:
        JoinThread = 0;
        break;

    default:

        assert(0);

        Result->Status = EINVAL;
        return;
    }

    //
    // Start the test. This snaps resource usage and starts the clock ticking.
    //

    Status = PtStartTimedTest(Test->Duration);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Measure the performance of the pthread_create() C library routine by
    // counting the number of times a thread can be created and destroyed.
    //

    while (PtIsTimedTestRunning() != 0) {
        Argument = Iterations;
        Status = pthread_create(&NewThread,
                                NULL,
                                PthreadStartRoutine,
                                &Argument);

        if (Status != 0) {

            //
            // The detach test case may create a bloom of new threads, too many
            // to support on the system. Allow memory failures.
            //

            if ((JoinThread == 0) &&
                ((Status == ENOMEM) || (Status == EAGAIN))) {

                continue;
            }

            Result->Status = Status;
            break;
        }

        if (JoinThread != 0) {
            Status = pthread_join(NewThread, NULL);

        } else {
            Status = pthread_detach(NewThread);
        }

        if (Status != 0) {
            Result->Status = Status;
            break;
        }

        Iterations += 1;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:
    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

void *
PthreadStartRoutine (
    void *Parameter
    )

/*++

Routine Description:

    This routine implements the start routine for a new test thread. It
    immediately exits.

Arguments:

    Parameter - Supplies a pointer to the current number of iterations the main
        thread has completed.

Return Value:

    Returns the NULL pointer.

--*/

{

    struct timespec CurrentTime;
    int *Iterations;
    struct timespec StartTime;
    int Status;

    //
    // If this is an even instance, then busy spin a little bit before
    // returning.
    //

    Iterations = (int *)Parameter;
    if ((*Iterations % 2) == 0) {
        Status = clock_gettime(CLOCK_MONOTONIC, &StartTime);
        if (Status != 0) {
            goto StartRoutineEnd;
        }

        while (1) {
            Status = clock_gettime(CLOCK_MONOTONIC, &CurrentTime);
            if (Status != 0) {
                goto StartRoutineEnd;
            }

            CurrentTime.tv_sec = CurrentTime.tv_sec - StartTime.tv_sec;
            CurrentTime.tv_nsec = CurrentTime.tv_nsec - StartTime.tv_nsec;
            while (CurrentTime.tv_nsec < 0) {
                CurrentTime.tv_sec -= 1;
                CurrentTime.tv_nsec += 1000000000ULL;
            }

            if ((CurrentTime.tv_nsec > 1000000) ||
                (CurrentTime.tv_sec > 0)) {

                break;
            }
        }
    }

StartRoutineEnd:
    return NULL;
}

