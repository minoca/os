/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dup.c

Abstract:

    This module implements the performance benchmark tests for the dup() C
    library call.

Author:

    Chris Stevens 5-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
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

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

void
DupMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the dup performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    int FileDescriptor;
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
    // Measure the performance of the dup() C library routine by counting the
    // number of times standard out can be duplicated and closed.
    //

    while (PtIsTimedTestRunning() != 0) {
        FileDescriptor = dup(STDOUT_FILENO);
        if (FileDescriptor < 0) {
            Result->Status = errno;
            break;
        }

        Status = close(FileDescriptor);
        if (Status != 0) {
            Result->Status = errno;
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

