/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stat.c

Abstract:

    This module implements the performance benchmark tests for the stat() and
    fstat() C library calls.

Author:

    Chris Stevens 26-Sept-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_STAT_TEST_FILE_NAME_LENGTH 48
#define PT_FSTAT_TEST_FILE_NAME_LENGTH 49

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
StatMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the stat performance benchmark test.

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
    char FileName[PT_STAT_TEST_FILE_NAME_LENGTH];
    unsigned long long Iterations;
    pid_t ProcessId;
    struct stat Stat;
    int Status;

    FileCreated = 0;
    Iterations = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;

    //
    // Get the process ID and create a process safe file to stat.
    //

    ProcessId = getpid();
    Status = snprintf(FileName,
                      PT_STAT_TEST_FILE_NAME_LENGTH,
                      "stat_%d.txt",
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
    // Measure the performance of the stat() C library routine by counting the
    // number of times the stats for the created file can be queried.
    //

    while (PtIsTimedTestRunning() != 0) {
        Status = stat(FileName, &Stat);
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

void
FstatMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the fstat performance benchmark test.

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
    char FileName[PT_FSTAT_TEST_FILE_NAME_LENGTH];
    unsigned long long Iterations;
    pid_t ProcessId;
    struct stat Stat;
    int Status;

    FileCreated = 0;
    Iterations = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;

    //
    // Get the process ID and create a process safe file to fstat.
    //

    ProcessId = getpid();
    Status = snprintf(FileName,
                      PT_FSTAT_TEST_FILE_NAME_LENGTH,
                      "fstat_%d.txt",
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
    // Measure the performance of the fstat() C library routine by counting the
    // number of times stats can be retrieved for a file descriptor.
    //

    while (PtIsTimedTestRunning() != 0) {
        Status = fstat(FileDescriptor, &Stat);
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
        close(FileDescriptor);
        remove(FileName);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

