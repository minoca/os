/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    malloc.c

Abstract:

    This module implements the performance benchmark tests for the malloc() C
    library routine.

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
#include <string.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_MALLOC_TEST_SMALL_ALLOCATION 32
#define PT_MALLOC_TEST_LARGE_ALLOCATION (128 * 1024)
#define PT_MALLOC_TEST_ALLOCATION_LIMIT (256 * 1024)
#define PT_MALLOC_TEST_ALLOCATION_COUNT 32
#define PT_MALLOC_TEST_THREAD_COUNT 8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void *
MallocStartRoutine (
    void *Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

volatile int MallocReadyThreadCount;

//
// ------------------------------------------------------------------ Functions
//

void
MallocMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the malloc performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    void **Allocations;
    size_t AllocationSize;
    int Index;
    unsigned long long Iterations;
    pthread_mutex_t Mutex;
    int RandomSize;
    unsigned int Seed;
    int Status;
    int ThreadCount;
    int ThreadIndex;
    pthread_t *Threads;

    Iterations = 0;
    RandomSize = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;
    ThreadIndex = 0;
    Threads = NULL;

    //
    // Allocate an array to hold the allocations.
    //

    AllocationSize = sizeof(void *) * PT_MALLOC_TEST_ALLOCATION_COUNT;
    Allocations = malloc(AllocationSize);
    if (Allocations == NULL) {
        Result->Status = ENOMEM;
        goto MainEnd;
    }

    memset(Allocations, 0, AllocationSize);
    AllocationSize = 0;

    //
    // Figure out the allocation size based on the test.
    //

    switch (Test->TestType) {
    case PtTestMallocSmall:
        AllocationSize = PT_MALLOC_TEST_SMALL_ALLOCATION;
        break;

    case PtTestMallocLarge:
        AllocationSize = PT_MALLOC_TEST_LARGE_ALLOCATION;
        break;

    case PtTestMallocContended:
        Threads = malloc(sizeof(pthread_t) * PT_MALLOC_TEST_THREAD_COUNT);
        if (Threads == NULL) {
            Result->Status = ENOMEM;
            goto MainEnd;
        }

        pthread_mutex_init(&Mutex, NULL);
        for (ThreadIndex = 0;
             ThreadIndex < PT_MALLOC_TEST_THREAD_COUNT;
             ThreadIndex += 1) {

            Status = pthread_create(&(Threads[ThreadIndex]),
                                    NULL,
                                    MallocStartRoutine,
                                    &Mutex);

            if (Status != 0) {
                Result->Status = Status;
                goto MainEnd;
            }
        }

        //
        // Wait until all threads are spun up.
        //

        while (MallocReadyThreadCount != PT_MALLOC_TEST_THREAD_COUNT) {
            sleep(1);
        }

        RandomSize = 1;
        break;

    case PtTestMallocRandom:
        RandomSize = 1;
        break;

    default:

        assert(0);

        Result->Status = EINVAL;
        return;
    }

    //
    // Get a thread-safe seed for the random number generator.
    //

    Seed = time(NULL);

    //
    // Start the test. This snaps resource usage and starts the clock ticking.
    //

    Status = PtStartTimedTest(Test->Duration);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Measure the performance of the malloc() C library routine by counting
    // the number of times a memory can be allocated and freed. Randomly keep
    // around some allocations to make this somewhat realistic.
    //

    while (PtIsTimedTestRunning() != 0) {
        if (RandomSize != 0) {
            AllocationSize = rand_r(&Seed) % PT_MALLOC_TEST_ALLOCATION_LIMIT;
        }

        //
        // Pick a random allocation slot and either make an allocation if it is
        // empty or free the existing allocation.
        //

        Index = rand_r(&Seed) % PT_MALLOC_TEST_ALLOCATION_COUNT;
        if (Allocations[Index] == NULL) {
            Allocations[Index] = malloc(AllocationSize);
            if (Allocations[Index] == NULL) {
                Result->Status = ENOMEM;
                break;
            }

        } else {
            free(Allocations[Index]);
            Allocations[Index] = NULL;
        }

        Iterations += 1;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:
    switch (Test->TestType) {
    case PtTestMallocContended:
        if (Threads != NULL) {
            ThreadCount = ThreadIndex;
            for (ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex += 1) {
                pthread_cancel(Threads[ThreadIndex]);
                pthread_join(Threads[ThreadIndex], (void **)&Status);
                if ((Status != 0) && (Result->Status == 0)) {
                    Result->Status = Status;
                }
            }

            free(Threads);
        }

        break;

    case PtTestMallocSmall:
    case PtTestMallocLarge:
    case PtTestMallocRandom:
    default:
        break;
    }

    if (Allocations != NULL) {
        for (Index = 0; Index < PT_MALLOC_TEST_ALLOCATION_COUNT; Index += 1) {
            if (Allocations[Index] != NULL) {
                free(Allocations[Index]);
            }
        }

        free(Allocations);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

void *
MallocStartRoutine (
    void *Parameter
    )

/*++

Routine Description:

    This routine implements the start routine for a new test thread. It will
    wait in a loop for the test to start and then loop allocating and freeing
    memory regions of random size.

Arguments:

    Parameter - Supplies a pointer to the mutex that protexts the ready thread
        count.

Return Value:

    0 on success or an errno value otherwise.

--*/

{

    void **Allocations;
    int AllocationSize;
    int Index;
    pthread_mutex_t *Mutex;
    unsigned int Seed;

    AllocationSize = sizeof(void *) * PT_MALLOC_TEST_ALLOCATION_COUNT;
    Allocations = malloc(AllocationSize);
    if (Allocations == NULL) {
        return (void *)ENOMEM;
    }

    memset(Allocations, 0, AllocationSize);
    Seed = time(NULL);

    //
    // Announce that this thread is ready.
    //

    Mutex = (pthread_mutex_t *)Parameter;
    pthread_mutex_lock(Mutex);
    MallocReadyThreadCount += 1;
    pthread_mutex_unlock(Mutex);

    //
    // Busy spin waiting for the test to start.
    //

    while (PtIsTimedTestRunning() == 0) {
        pthread_testcancel();
    }

    //
    // As long as the test is running, allocation and free blocks of memory.
    //

    while (PtIsTimedTestRunning() != 0) {
        AllocationSize = rand_r(&Seed) % PT_MALLOC_TEST_ALLOCATION_LIMIT;

        //
        // Pick a random allocation slot and either make an allocation if it is
        // empty or free the existing allocation.
        //

        Index = rand_r(&Seed) % PT_MALLOC_TEST_ALLOCATION_COUNT;
        if (Allocations[Index] == NULL) {
            Allocations[Index] = malloc(AllocationSize);
            if (Allocations[Index] == NULL) {
                break;
            }

        } else {
            free(Allocations[Index]);
            Allocations[Index] = NULL;
        }
    }

    for (Index = 0; Index < PT_MALLOC_TEST_ALLOCATION_COUNT; Index += 1) {
        if (Allocations[Index] != NULL) {
            free(Allocations[Index]);
        }
    }

    free(Allocations);
    return (void *)0;
}

