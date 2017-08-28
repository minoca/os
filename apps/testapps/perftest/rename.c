/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rename.c

Abstract:

    This module implements the performance benchmark tests for the rename() C
    library call.

Author:

    Chris Stevens 6-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_RENAME_TEST_FILE_NAME_LENGTH 48

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
RenameMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the rename performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    char *DestinationFile;
    int FileCreated;
    int FileDescriptor;
    char FileNames[2][PT_RENAME_TEST_FILE_NAME_LENGTH];
    int Index;
    unsigned long long Iterations;
    pid_t ProcessId;
    char *SourceFile;
    int Status;
    char *TempFile;

    FileCreated = 0;
    Iterations = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;
    SourceFile = FileNames[0];
    DestinationFile = FileNames[1];

    //
    // Create two process safe file names. One will start as the source and the
    // other the destination.
    //

    ProcessId = getpid();
    for (Index = 0; Index < 2; Index += 1) {
        Status = snprintf(FileNames[Index],
                          PT_RENAME_TEST_FILE_NAME_LENGTH,
                          "rename%d_%d.txt",
                          Index,
                          ProcessId);

        if (Status < 0) {
            Result->Status = errno;
            goto MainEnd;
        }
    }

    SourceFile = FileNames[0];
    DestinationFile = FileNames[1];

    //
    // Create the source file.
    //

    FileDescriptor = creat(SourceFile, S_IRUSR | S_IWUSR);
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
    // Measure the performance of the rename() C library routine by counting
    // the number of times a file can be renamed.
    //

    while (PtIsTimedTestRunning() != 0) {
        Status = rename(SourceFile, DestinationFile);
        if (Status != 0) {
            Result->Status = errno;
            break;
        }

        //
        // Swap the two pointers.
        //

        TempFile = SourceFile;
        SourceFile = DestinationFile;
        DestinationFile = TempFile;
        Iterations += 1;
    }

    Status = PtFinishTimedTest(Result);
    if ((Status != 0) && (Result->Status == 0)) {
        Result->Status = errno;
    }

MainEnd:

    //
    // If the file was created, try to delete it at both possible file paths.
    //

    if (FileCreated != 0) {
        remove(FileNames[0]);
        remove(FileNames[1]);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

