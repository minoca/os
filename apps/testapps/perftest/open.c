/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    open.c

Abstract:

    This module implements the performance benchmark tests for the open() and
    close() C library calls.

Author:

    Chris Stevens 5-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_OPEN_TEST_FILE_NAME_LENGTH 48

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
OpenMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the open performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    int FileCreated;
    int FileDescriptor;
    char FileName[PT_OPEN_TEST_FILE_NAME_LENGTH];
    unsigned long long Iterations;
    pid_t ProcessId;
    int Status;

    FileCreated = 0;
    Iterations = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;

    //
    // Get the process ID and create a process safe file to open and close.
    //

    ProcessId = getpid();
    Status = snprintf(FileName,
                      PT_OPEN_TEST_FILE_NAME_LENGTH,
                      "open_%d.txt",
                      ProcessId);

    if (Status < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    FileDescriptor = creat(FileName, S_IRUSR | S_IWUSR);
    if (FileDescriptor < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    close(FileDescriptor);
    FileCreated = 1;

    //
    // Start the test. This snaps resource usage and starts the clock ticking.
    //

    Status = PtStartTimedTest(Test->Duration);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Measure the performance of the open() and close() C library routines by
    // counting the number of times a file can be opened and closed.
    //

    while (PtIsTimedTestRunning() != 0) {
        FileDescriptor = open(FileName, O_RDWR);
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
    if (FileCreated != 0) {
        remove(FileName);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

