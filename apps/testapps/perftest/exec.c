/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    exec.c

Abstract:

    This module implements the performance benchmark tests for the exec()
    family of C library calls.

Author:

    Chris Stevens 27-Apr-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "perftest.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the argument array indices for the various arguments to the exec
// loop.
//

#define EXEC_LOOP_START_TIME_INDEX 2
#define EXEC_LOOP_DURATION_INDEX 3
#define EXEC_LOOP_ITERATIONS_INDEX 4

//
// Define the maximum string lengths for each of the arguments.
//

#define EXEC_MAX_START_TIME_STRING_LENGTH 32
#define EXEC_MAX_DURATION_STRING_LENGTH 32
#define EXEC_MAX_ITERATIONS_STRING_LENGTH 32

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
ExecMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine performs the execute performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

{

    char *Arguments[EXEC_LOOP_ARGUMENT_COUNT + 1];
    ssize_t BytesRead;
    pid_t Child;
    int CollectionActive;
    char DurationString[EXEC_MAX_DURATION_STRING_LENGTH];
    int PipeDescriptors[2];
    time_t StartTime;
    char StartTimeString[EXEC_MAX_START_TIME_STRING_LENGTH];
    int Status;
    pid_t WaitedChild;

    CollectionActive = 0;

    //
    // Create a communication pipe for the child to write its results into.
    //

    Status = pipe(PipeDescriptors);
    if (Status < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // Start collecting resource usage data.
    //

    Status = PtCollectResourceUsageStart();
    if (Status != 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    CollectionActive = 1;

    //
    // Fork off a child to perform the exec loop so that this one can
    // eventually return with the results.
    //

    Child = fork();
    if (Child < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    //
    // The child needs to duplicate the write end of the pipe to standard out
    // so that the results can be sent back to the parent within the exec loop.
    //

    if (Child == 0) {
        Status = dup2(PipeDescriptors[1], STDOUT_FILENO);
        if (Status < 0) {
            exit(errno);
        }

        close(PipeDescriptors[0]);
        close(PipeDescriptors[1]);

        //
        // Build the arguments to re-execute this application.
        //

        Arguments[0] = PtProgramPath;
        Arguments[1] = EXEC_TEST_NAME;
        Arguments[EXEC_LOOP_ARGUMENT_COUNT] = NULL;
        Arguments[EXEC_LOOP_ITERATIONS_INDEX] = "0";
        Status = snprintf(DurationString,
                          EXEC_MAX_DURATION_STRING_LENGTH,
                          "%llu",
                          (signed long long)Test->Duration);

        if (Status < 0) {
            exit(EINVAL);
        }

        Arguments[EXEC_LOOP_DURATION_INDEX] = DurationString;
        time(&StartTime);
        Status = snprintf(StartTimeString,
                          EXEC_MAX_START_TIME_STRING_LENGTH,
                          "%llu",
                          (signed long long)StartTime);

        if (Status < 0) {
            exit(EINVAL);
        }

        Arguments[EXEC_LOOP_START_TIME_INDEX] = StartTimeString;

        //
        // Fire it off!
        //

        execv(PtProgramPath, Arguments);
        exit(errno);
    }

    //
    // Close the write end of the pipe in the parent.
    //

    close(PipeDescriptors[1]);

    //
    // Wait for the child to return from its exec loop.
    //

    WaitedChild = waitpid(Child, &Status, 0);
    if (WaitedChild < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    if (WaitedChild != Child) {
        Result->Status = ECHILD;
        goto MainEnd;
    }

    if ((!WIFEXITED(Status)) ||
        (WEXITSTATUS(Status) != 0)) {

        Result->Status = WEXITSTATUS(Status);
        goto MainEnd;
    }

    //
    // Collect the results from the child's pipe.
    //

    do {
        BytesRead = read(PipeDescriptors[0], Result, sizeof(PT_TEST_RESULT));

    } while ((BytesRead < 0) && (errno == EINTR));

    if (BytesRead < 0) {
        Result->Status = errno;
        goto MainEnd;
    }

    if (BytesRead != sizeof(PT_TEST_RESULT)) {
        Result->Status = EIO;
        goto MainEnd;
    }

MainEnd:
    if (CollectionActive != 0) {
        Status = PtCollectResourceUsageStop(Result);
        if ((Status != 0) && (Result->Status == 0)) {
            Result->Status = errno;
        }
    }

    return;
}

int
ExecLoop (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements an interation of the execute test.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    char *AfterScan;
    ssize_t BytesWritten;
    time_t CurrentTime;
    time_t Duration;
    unsigned long long Iterations;
    char IterationsString[EXEC_MAX_ITERATIONS_STRING_LENGTH];
    char *OldIterations;
    PT_TEST_RESULT Result;
    time_t StartTime;
    int Status;

    assert(ArgumentCount == EXEC_LOOP_ARGUMENT_COUNT);
    assert(strcasecmp(Arguments[1], EXEC_TEST_NAME) == 0);

    //
    // Get the test duration.
    //

    Duration = (time_t)strtoll(Arguments[EXEC_LOOP_DURATION_INDEX],
                               &AfterScan,
                               10);

    assert(Duration > 0);

    //
    // Get the start time.
    //

    StartTime = (time_t)strtoll(Arguments[EXEC_LOOP_START_TIME_INDEX],
                                &AfterScan,
                                10);

    assert(StartTime > 0);

    //
    // Get the iterations and add this one to the count.
    //

    errno = 0;
    Iterations = strtoull(Arguments[EXEC_LOOP_ITERATIONS_INDEX],
                          &AfterScan,
                          10);

    assert((Iterations != 0) || (errno == 0));

    Iterations += 1;

    //
    // If time has expired, send the results back to the waiting parent via
    // standard out, which is actually the pipe the parent is waiting to read
    // from.
    //

    time(&CurrentTime);
    if ((CurrentTime - StartTime) >= Duration) {
        memset(&Result, 0, sizeof(PT_TEST_RESULT));
        Result.Type = PtResultIterations;
        Result.Data.Iterations = Iterations;
        do {
            BytesWritten = write(STDOUT_FILENO,
                                 &Result,
                                 sizeof(PT_TEST_RESULT));

        } while ((BytesWritten < 0) && (errno == EINTR));

        if (BytesWritten < 0) {
            Status = errno;
            goto LoopEnd;
        }

        if (BytesWritten < sizeof(PT_TEST_RESULT)) {
            Status = EIO;
            goto LoopEnd;
        }

        Status = 0;

    //
    // Otherwise fire off another iteration of the loop.
    //

    } else {
        Status = snprintf(IterationsString,
                          EXEC_MAX_ITERATIONS_STRING_LENGTH,
                          "%llu",
                          Iterations);

        if (Status < 0) {
            Status = EINVAL;
            goto LoopEnd;
        }

        OldIterations = Arguments[EXEC_LOOP_ITERATIONS_INDEX];
        Arguments[EXEC_LOOP_ITERATIONS_INDEX] = IterationsString;
        execv(Arguments[0], Arguments);
        Status = errno;
        Arguments[EXEC_LOOP_ITERATIONS_INDEX] = OldIterations;
    }

LoopEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

