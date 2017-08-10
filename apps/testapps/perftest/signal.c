/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    signal.c

Abstract:

    This module implements the performance benchmark tests for signal related
    activities.

Author:

    Evan Green 9-Aug-2017

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

void
SignalHandler (
    int Signal
    );

void *
SignalHammerThread (
    void *Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Remember the number of signals that occurred.
//

unsigned long long SignalCount;

//
// ------------------------------------------------------------------ Functions
//

void
SignalMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the signal performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    struct sigaction Action;
    int Character;
    unsigned long long Iterations;
    struct sigaction OldAction;
    int Pipe[2];
    int Status;
    pthread_t Thread;

    SignalCount = 0;
    Iterations = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;
    memset(&Action, 0, sizeof(Action));
    memset(&OldAction, 0, sizeof(OldAction));
    Action.sa_handler = SIG_DFL;
    Action.sa_flags = SA_RESTART;
    Pipe[0] = -1;
    Pipe[1] = -1;

    //
    // Perform setup specific to each test.
    //

    switch (Test->TestType) {
    case PtTestSignalIgnored:
        Action.sa_handler = SIG_IGN;
        break;

    case PtTestSignalHandled:
        Action.sa_handler = SignalHandler;
        break;

    case PtTestSignalRestart:
        Action.sa_handler = SignalHandler;
        if (pipe(Pipe) != 0) {
            Result->Status = errno;
            goto MainEnd;
        }

        break;

    default:
        fprintf(stderr, "Unknown signal test type %d\n", Test->TestType);
        Result->Status = EINVAL;
        break;
    }

    //
    // Set up the signal handler if desired.
    //

    if (Action.sa_handler != SIG_DFL) {
        if (sigaction(SIGUSR1, &Action, &OldAction) != 0) {
            Action.sa_handler = SIG_DFL;
            Result->Status = errno;
            goto MainEnd;
        }
    }

    //
    // Start the test. This snaps resource usage and starts the clock ticking.
    //

    Status = PtStartTimedTest(Test->Duration);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    switch (Test->TestType) {
    case PtTestSignalIgnored:
    case PtTestSignalHandled:
        while (PtIsTimedTestRunning() != 0) {
            if (raise(SIGUSR1) != 0) {
                Result->Status = errno;
                break;
            }

            Iterations += 1;
        }

        if ((Test->TestType == PtTestSignalHandled) &&
            (Iterations != SignalCount)) {

            fprintf(stderr,
                    "Error: Raised %lld times but only saw %lld signals.\n",
                    Iterations,
                    SignalCount);

            Result->Status = EINVAL;
        }

        break;

    case PtTestSignalRestart:
        Result->Status = pthread_create(&Thread,
                                        NULL,
                                        SignalHammerThread,
                                        Pipe);

        if (Result->Status != 0) {
            break;
        }

        //
        // Try to read from a pipe that will never have any data ready until
        // the end of the test. This operation should get interrupted and
        // restarted many times.
        //

        if (read(Pipe[0], &Character, 1) != 1) {
            fprintf(stderr, "Failed to read from pipe\n");
            Result->Status = errno;
        }

        if (PtIsTimedTestRunning() != 0) {
            fprintf(stderr, "Error: Read completed before test\n");
            Result->Status = EINVAL;
        }

        Status = pthread_join(Thread, NULL);
        if (Status != 0) {
            Result->Status = Status;
        }

        Iterations = SignalCount;
        break;

    default:
        fprintf(stderr, "Unknown signal test type %d\n", Test->TestType);
        Result->Status = EINVAL;
        break;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:
    if (Pipe[0] >= 0) {
        close(Pipe[0]);
    }

    if (Pipe[1] >= 0) {
        close(Pipe[1]);
    }

    if (Action.sa_handler != SIG_DFL) {
        sigaction(SIGUSR1, &OldAction, NULL);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

void
SignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine implements the signal handler for the signal performance test.

Arguments:

    Signal - Supplies the signal coming in.

Return Value:

    None.

--*/

{

    SignalCount += 1;
    return;
}

void *
SignalHammerThread (
    void *Parameter
    )

/*++

Routine Description:

    This routine implements the thread that hammers the main test thread with
    signals while it's trying to do a read. At the end of the test, it does a
    write so the read can complete happily.

Arguments:

    Parameter - Supplies a pointer to the pipe array.

Return Value:

    None.

--*/

{

    int *Pipe;
    sigset_t SignalSet;

    //
    // Block SIGUSR1 so this thread doesn't get interrupted by it.
    //

    sigemptyset(&SignalSet);
    sigaddset(&SignalSet, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &SignalSet, NULL);
    Pipe = Parameter;
    while (PtIsTimedTestRunning() != 0) {
        raise(SIGUSR1);
        sched_yield();
    }

    write(Pipe[1], &SignalSet, 1);
    return NULL;
}

