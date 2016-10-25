/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mutex.c

Abstract:

    This module implements the mutex performance benchmark tests.

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
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_MUTEXT_TEST_THREAD_COUNT 8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void *
MutexStartRoutine (
    void *Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

volatile int MutexReadyThreadCount;

//
// ------------------------------------------------------------------ Functions
//

void
MutexMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the mutex performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    unsigned long long Iterations;
    pthread_mutex_t Mutex;
    int MutexInitialized;
    int Status;
    int ThreadCount;
    int ThreadIndex;
    pthread_t *Threads;

    Iterations = 0;
    MutexInitialized = 0;
    Threads = NULL;
    Result->Type = PtResultIterations;
    Result->Status = 0;
    ThreadIndex = 0;

    //
    // Initialize a mutex for use by the main thread and any additional threads.
    //

    Status = pthread_mutex_init(&Mutex, NULL);
    if (Status != 0) {
        Result->Status = Status;
        goto MainEnd;
    }

    MutexInitialized = 1;

    //
    // Initialize the given test state.
    //

    switch (Test->TestType) {
    case PtTestMutex:
        break;

    case PtTestMutexContended:
        Threads = malloc(sizeof(pthread_t) * PT_MUTEXT_TEST_THREAD_COUNT);
        if (Threads == NULL) {
            Result->Status = ENOMEM;
            goto MainEnd;
        }

        for (ThreadIndex = 0;
             ThreadIndex < PT_MUTEXT_TEST_THREAD_COUNT;
             ThreadIndex += 1) {

            Status = pthread_create(&(Threads[ThreadIndex]),
                                    NULL,
                                    MutexStartRoutine,
                                    &Mutex);

            if (Status != 0) {
                Result->Status = Status;
                goto MainEnd;
            }
        }

        //
        // Wait until all threads are spun up.
        //

        while (MutexReadyThreadCount != PT_MUTEXT_TEST_THREAD_COUNT) {
            sleep(1);
        }

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
    // Measure the performance of the mutex lock and unlock by seeing how many
    // times it can be acquired and released.
    //

    while (PtIsTimedTestRunning() != 0) {
        pthread_mutex_lock(&Mutex);
        pthread_mutex_unlock(&Mutex);
        Iterations += 1;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:

    //
    // Tear down the test state.
    //

    switch (Test->TestType) {
    case PtTestMutexContended:
        if (Threads != NULL) {
            ThreadCount = ThreadIndex;
            for (ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex += 1) {
                pthread_cancel(Threads[ThreadIndex]);
                pthread_join(Threads[ThreadIndex], NULL);
            }

            free(Threads);
        }

        break;

    case PtTestMutex:
    default:
        break;
    }

    if (MutexInitialized != 0) {
        pthread_mutex_destroy(&Mutex);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

void *
MutexStartRoutine (
    void *Parameter
    )

/*++

Routine Description:

    This routine implements the start routine for a new test thread. It will
    wait in a loop for the test to start and then loop acquiring and releasing
    the given mutex.

Arguments:

    Parameter - Supplies a pointer to the mutex to use.

Return Value:

    Returns the NULL pointer.

--*/

{

    pthread_mutex_t *Mutex;

    Mutex = (pthread_mutex_t *)Parameter;

    //
    // Announce that the thread is ready.
    //

    pthread_mutex_lock(Mutex);
    MutexReadyThreadCount += 1;
    pthread_mutex_unlock(Mutex);

    //
    // Busy spin waiting for the test to start.
    //

    while (PtIsTimedTestRunning() == 0) {
        pthread_testcancel();
    }

    //
    // Loop running the test.
    //

    while (PtIsTimedTestRunning() != 0) {
        pthread_mutex_lock(Mutex);
        pthread_mutex_unlock(Mutex);
    }

    return NULL;
}

