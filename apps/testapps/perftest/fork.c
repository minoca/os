/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fork.c

Abstract:

    This module implements the performance benchmark tests for the fork()
    C library call.

Author:

    Chris Stevens 27-Apr-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

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

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

void
ForkMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the fork performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    pid_t Child;
    unsigned long long Iterations;
    int Status;

    Iterations = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;

    //
    // Start the test. This snaps resource usage and starts the clock ticking.
    //

    Status = PtStartTimedTest(Test->Duration);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Measure the performance of the fork() C library routine by counting the
    // number of times a forked child can be waited on during the given
    // duration. The child, in this case, exits immediately.
    //

    while (PtIsTimedTestRunning() != 0) {
        Child = fork();
        if (Child < 0) {
            Result->Status = errno;
            break;

        } else if (Child == 0) {
            exit(0);

        } else {
            Child = waitpid(Child, &Status, 0);
            if (Child == -1) {
                if (PtIsTimedTestRunning() == 0) {
                    break;
                }

                Result->Status = errno;
                break;
            }

            if (Status != 0) {
                Result->Status = WEXITSTATUS(Status);
                break;
            }

            Iterations += 1;
        }
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

