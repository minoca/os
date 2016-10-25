/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    perfsup.c

Abstract:

    This module implements the helper routines for all performance benchmark
    tests.

Author:

    Chris Stevens 5-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

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
PtAlarmSignalHandler (
    int Signal
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a value indicating whether ot not a test is running.
//

int PtTestRunning;

//
// Store the original alarm signal handler action.
//

struct sigaction PtAlarmOriginalAction;

//
// Store a value indicating whether or not the resource usage is in use.
//

int PtResourceUsageBusy;

//
// Store the starting resource usage.
//

PT_TEST_RESOURCE_USAGE PtStartResourceUsage;

//
// ------------------------------------------------------------------ Functions
//

int
PtStartTimedTest (
    time_t Duration
    )

/*++

Routine Description:

    This routine starts a timed performance test. It will collect initial
    resource usage and then set an alarm to stop the test after the given
    duration. Only one test can run at a time for each process.

Arguments:

    Duration - Supplies the desired duration of the test, in seconds.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    struct sigaction NewAction;
    unsigned int PreviousAlarm;
    int Status;

    //
    // Only one test can be running at a given time within one process.
    //

    if (PtTestRunning != 0) {
        errno = EBUSY;
        return -1;
    }

    //
    // Set the alarm to modify the test running variable after the duration.
    //

    memset(&NewAction, 0, sizeof(struct sigaction));
    NewAction.sa_handler = PtAlarmSignalHandler;
    Status = sigaction(SIGALRM, &NewAction, &PtAlarmOriginalAction);
    if (Status != 0) {
        return Status;
    }

    //
    // Start collecting the resource usage before the alarm is set.
    //

    Status = PtCollectResourceUsageStart();
    if (Status != 0) {
        goto StartTimedTestEnd;
    }

    //
    // Set the alarm to stop the test after the given number of seconds.
    //

    PtTestRunning = 1;
    Status = 0;
    errno = 0;
    PreviousAlarm = alarm((unsigned int)Duration);
    if (PreviousAlarm != 0) {
        errno = EBUSY;
        Status = -1;

    } else if (errno != 0) {
        Status = -1;
    }

StartTimedTestEnd:

    //
    // Set the original alarm action back if the test failed to start.
    //

    if (Status != 0) {
        PtTestRunning = 0;
        sigaction(SIGALRM, &PtAlarmOriginalAction, NULL);
    }

    return Status;
}

int
PtFinishTimedTest (
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine finalizes a running test that has stopped. It will collect the
    final usage statistics and set them in the given result. It makes sure that
    the alarm is disabled and stops the test.

Arguments:

    Result - Supplies a pointer to the test result that will receive the test's
        resource usage, including child resources.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    int Status;

    //
    // Stop collecting the resource usage.
    //

    Status = PtCollectResourceUsageStop(Result);

    //
    // Always disable the timer and restore the original action.
    //

    alarm(0);
    sigaction(SIGALRM, &PtAlarmOriginalAction, NULL);
    PtTestRunning = 0;
    return Status;
}

int
PtIsTimedTestRunning (
    void
    )

/*++

Routine Description:

    This routine determines whether or not a timed test is running.

Arguments:

    None.

Return Value:

    1 if a test is currently running, or 0 otherwise.

--*/

{

    return PtTestRunning;
}

int
PtCollectResourceUsageStart (
    void
    )

/*++

Routine Description:

    This routine starts collecting resource usage by taking a snapshot of the
    current process's usage and the usage of any children it has waited on.

Arguments:

    None.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    struct rusage ChildrenUsage;
    struct rusage SelfUsage;
    int Status;

    if (PtResourceUsageBusy != 0) {
        errno = EBUSY;
        return -1;
    }

    //
    // Get the real time first to make sure the user and system times are
    // always less than the real time.
    //

    Status = gettimeofday(&(PtStartResourceUsage.RealTime), NULL);
    if (Status != 0) {
        return Status;
    }

    Status = getrusage(RUSAGE_CHILDREN, &ChildrenUsage);
    if (Status != 0) {
        return Status;
    }

    Status = getrusage(RUSAGE_SELF, &SelfUsage);
    if (Status != 0) {
        return Status;
    }

    timeradd(&(SelfUsage.ru_utime),
             &(ChildrenUsage.ru_utime),
             &(PtStartResourceUsage.UserTime));

    timeradd(&(SelfUsage.ru_stime),
             &(ChildrenUsage.ru_stime),
             &(PtStartResourceUsage.SystemTime));

    PtResourceUsageBusy = 1;
    return 0;
}

int
PtCollectResourceUsageStop (
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine stops collecting resource usage data for the current test and
    fills result with the test's resource usage stats.

Arguments:

    Result - Supplies a pointer to the test result that will receive the test's
        resource usage, including child resources.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    struct rusage ChildrenUsage;
    struct rusage EndUsage;
    struct timeval RealTime;
    struct rusage SelfUsage;
    int Status;

    if (PtResourceUsageBusy == 0) {
        errno = EINVAL;
        return -1;
    }

    PtResourceUsageBusy = 0;

    //
    // Measure the tests resource usage and store it in the result.
    //

    Status = getrusage(RUSAGE_SELF, &SelfUsage);
    if (Status != 0) {
        return Status;
    }

    Status = getrusage(RUSAGE_CHILDREN, &ChildrenUsage);
    if (Status != 0) {
        return Status;
    }

    //
    // Get the real time after collecting the usage times, so that the user and
    // system times are always a percentage of the real time.
    //

    Status = gettimeofday(&RealTime, NULL);
    if (Status != 0) {
        return Status;
    }

    timeradd(&(SelfUsage.ru_utime),
             &(ChildrenUsage.ru_utime),
             &(EndUsage.ru_utime));

    timeradd(&(SelfUsage.ru_stime),
             &(ChildrenUsage.ru_stime),
             &(EndUsage.ru_stime));

    timersub(&(EndUsage.ru_utime),
             &(PtStartResourceUsage.UserTime),
             &(Result->ResourceUsage.UserTime));

    timersub(&(EndUsage.ru_stime),
             &(PtStartResourceUsage.SystemTime),
             &(Result->ResourceUsage.SystemTime));

    timersub(&RealTime,
             &(PtStartResourceUsage.RealTime),
             &(Result->ResourceUsage.RealTime));

    Result->ResourceUsageValid = 1;
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

void
PtAlarmSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine is called when a signal fires. If it is the alarm signal, then
    it will mark the performance test timer as expired.

Arguments:

    Signal - Supplies the signal that fired.

Return Value:

    None.

--*/

{

    if (Signal == SIGALRM) {
        PtTestRunning = 0;
    }

    return;
}

