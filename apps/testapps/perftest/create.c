/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    create.c

Abstract:

    This module implements the performance benchmark tests for the creat() and
    remove() C library calls.

Author:

    Chris Stevens 5-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_CREATE_TEST_FILE_NAME_LENGTH 48

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
CreateMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the create performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    int FileDescriptor;
    char FileName[PT_CREATE_TEST_FILE_NAME_LENGTH];
    unsigned long long Iterations;
    pid_t ProcessId;
    int Status;

    Iterations = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;

    //
    // Get the process ID and create a process safe file path to create and
    // remove.
    //

    ProcessId = getpid();
    Status = snprintf(FileName,
                      PT_CREATE_TEST_FILE_NAME_LENGTH,
                      "create_%d.txt",
                      ProcessId);

    if (Status < 0) {
        Result->Status = errno;
        goto MainEnd;
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
    // Measure the performance of the creat() and remove() C library routines
    // by counting the number of times a file can be created and removed.
    //

    while (PtIsTimedTestRunning() != 0) {
        FileDescriptor = creat(FileName, S_IRUSR | S_IWUSR);
        if (FileDescriptor < 0) {
            Result->Status = errno;
            break;
        }

        Status = close(FileDescriptor);
        if (Status != 0) {
            Result->Status = errno;
            break;
        }

        Status = remove(FileName);
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

