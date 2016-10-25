/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pipeio.c

Abstract:

    This module implements the performance benchmark tests pipe I/O throughput.

Author:

    Chris Stevens 5-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PT_PIPE_IO_BUFFER_SIZE 4096

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
PipeIoMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the pipe I/O performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    char *Buffer;
    ssize_t BytesCompleted;
    unsigned long long Iterations;
    int PipeCreated;
    int PipeDescriptors[2];
    int Status;

    Iterations = 0;
    PipeCreated = 0;
    Result->Type = PtResultIterations;
    Result->Status = 0;

    //
    // Allocate a scratch buffer to use for reads and writes.
    //

    Buffer = malloc(PT_PIPE_IO_BUFFER_SIZE);
    if (Buffer == NULL) {
        Result->Status = ENOMEM;
        goto MainEnd;
    }

    //
    // Create the pipe.
    //

    Status = pipe(PipeDescriptors);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    PipeCreated = 1;

    //
    // Start the test. This snaps resource usage and starts the clock ticking.
    //

    Status = PtStartTimedTest(Test->Duration);
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Measure the performance of pipe I/O throughput by alternating between
    // writing to and reading from a pipe.
    //

    while (PtIsTimedTestRunning() != 0) {
        do {
            BytesCompleted = write(PipeDescriptors[1],
                                   Buffer,
                                   PT_PIPE_IO_BUFFER_SIZE);

        } while ((BytesCompleted < 0) && (errno == EINTR));

        if (BytesCompleted != PT_PIPE_IO_BUFFER_SIZE) {
            if (errno == 0) {
                errno = EIO;
            }

            Result->Status = errno;
            break;
        }

        do {
            BytesCompleted = read(PipeDescriptors[0],
                                  Buffer,
                                  PT_PIPE_IO_BUFFER_SIZE);

        } while ((BytesCompleted < 0) && (errno == EINTR));

        if (BytesCompleted != PT_PIPE_IO_BUFFER_SIZE) {
            if (errno == 0) {
                errno = EIO;
            }

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
    if (PipeCreated != 0) {
        close(PipeDescriptors[0]);
        close(PipeDescriptors[1]);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    Result->Data.Iterations = Iterations;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

