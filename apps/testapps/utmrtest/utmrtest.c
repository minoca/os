/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    utmrtest.c

Abstract:

    This module implements the tests used to verify that user mode timers are
    functioning properly.

Author:

    Evan Green 11-Aug-2012

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <osbase.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

//
// --------------------------------------------------------------------- Macros
//

#define DEBUG_PRINT(...)                \
    if (TimerTestVerbose != FALSE) {   \
        printf(__VA_ARGS__);            \
    }

#define PRINT_ERROR(...) printf(__VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of timers to fire up.
//

#define TEST_TIMER_COUNT 50

//
// Define the number of time ticks all the timers should get up to.
//

#define TEST_TIMER_GOAL 50

//
// Define the test timer period.
//

#define TEST_TIMER_PERIOD_SECONDS 0
#define TEST_TIMER_PERIOD_NANOSECONDS 250000000

//
// Define the waiting period of the test.
//

#define TEST_TIMER_TIMEOUT ((LONGLONG)500)

//
// Define how often progress gets printed in verbose mode.
//

#define TEST_TIMER_UPDATE_INTERVAL 5

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
RunAllTimerTests (
    );

ULONG
RunTimerTest (
    );

VOID
TimerTestAlarmSignalHandler (
    INT SignalNumber,
    siginfo_t *Information,
    void *SomethingElse
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to enable more verbose debug output.
//

BOOL TimerTestVerbose = TRUE;

//
// Store the test timers.
//

timer_t TestTimers[TEST_TIMER_COUNT];
volatile int TestTimerCount[TEST_TIMER_COUNT];

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the signal test program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG Failures;

    Failures = RunAllTimerTests();
    if (Failures == 0) {
        return 0;
    }

    return 1;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
RunAllTimerTests (
    )

/*++

Routine Description:

    This routine executes all timer tests.

Arguments:

    None.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    ULONG Failures;

    Failures = 0;
    Failures += RunTimerTest();
    if (Failures != 0) {
        PRINT_ERROR("*** %d failures in timer test. ***\n", Failures);

    } else {
        DEBUG_PRINT("All timer tests pass.\n");
    }

    return Failures;
}

ULONG
RunTimerTest (
    )

/*++

Routine Description:

    This routine tests user mode timers.

Arguments:

    None.

Return Value:

    Returns the number of failures in the test.

--*/

{

    struct sigaction Action;
    ULONGLONG CurrentTime;
    ULONGLONG EndTime;
    ULONG Failures;
    ULONGLONG Frequency;
    ULONGLONG LastUpdate;
    int MaxCount;
    ULONG MaxTimer;
    int MinCount;
    ULONG MinTimer;
    struct sigaction OriginalAction;
    struct itimerspec Rate;
    int Result;
    BOOL Stop;
    ULONG TimerIndex;

    Failures = 0;
    Stop = TRUE;

    //
    // Set up the signal handler.
    //

    Action.sa_sigaction = TimerTestAlarmSignalHandler;
    sigemptyset(&(Action.sa_mask));
    Action.sa_flags = SA_SIGINFO;
    Result = sigaction(SIGALRM, &Action, &OriginalAction);
    if (Result != 0) {
        PRINT_ERROR("TimerTest: sigaction failed: %s\n", strerror(errno));
        return 1;
    }

    //
    // Create a bunch of timers.
    //

    for (TimerIndex = 0; TimerIndex < TEST_TIMER_COUNT; TimerIndex += 1) {
        TestTimers[TimerIndex] = -1;
        Result = timer_create(CLOCK_REALTIME, NULL, &(TestTimers[TimerIndex]));
        if (Result != 0) {
            PRINT_ERROR("TimerTest: Failed to create timer: %s.\n",
                        strerror(errno));

            Failures += 1;
        }
    }

    //
    // Arm them.
    //

    Rate.it_value.tv_sec = TEST_TIMER_PERIOD_SECONDS;
    Rate.it_value.tv_nsec = TEST_TIMER_PERIOD_NANOSECONDS;
    Rate.it_interval.tv_sec = TEST_TIMER_PERIOD_SECONDS;
    Rate.it_interval.tv_nsec = TEST_TIMER_PERIOD_NANOSECONDS;
    for (TimerIndex = 0; TimerIndex < TEST_TIMER_COUNT; TimerIndex += 1) {
        Result = timer_settime(TestTimers[TimerIndex], 0, &Rate, NULL);
        if (Result != 0) {
            PRINT_ERROR("TimerTest: Failed to create timer: %s.\n",
                        strerror(errno));

            Failures += 1;
        }
    }

    //
    // Compute the timeout time.
    //

    Frequency = OsGetTimeCounterFrequency();
    EndTime = OsGetRecentTimeCounter() + (TEST_TIMER_TIMEOUT * Frequency);
    LastUpdate = OsGetRecentTimeCounter() / Frequency;
    while (OsGetRecentTimeCounter() < EndTime) {
        Stop = TRUE;
        MinTimer = -1;
        MinCount = MAX_LONG;
        MaxTimer = -1;
        MaxCount = 0;
        for (TimerIndex = 0; TimerIndex < TEST_TIMER_COUNT; TimerIndex += 1) {
            if (TestTimerCount[TimerIndex] < TEST_TIMER_GOAL) {
                Stop = FALSE;
            }

            if ((MinTimer == -1) || (TestTimerCount[TimerIndex] < MinCount)) {
                MinTimer = TimerIndex;
                MinCount = TestTimerCount[TimerIndex];
            }

            if ((MaxTimer == -1) || (TestTimerCount[TimerIndex] > MaxCount)) {
                MaxTimer = TimerIndex;
                MaxCount = TestTimerCount[TimerIndex];
            }
        }

        if (Stop != FALSE) {
            DEBUG_PRINT("All timers reached threshold.\n");
            break;
        }

        CurrentTime = OsGetRecentTimeCounter() / Frequency;
        if (CurrentTime - LastUpdate >= TEST_TIMER_UPDATE_INTERVAL) {
            DEBUG_PRINT("%I64d: Min count %d, timer %d. "
                        "Max count %d, timer %d.\n",
                        CurrentTime,
                        MinCount,
                        MinTimer,
                        MaxCount,
                        MaxTimer);

            fflush(NULL);
            LastUpdate = CurrentTime;
        }
    }

    if (Stop == FALSE) {
        PRINT_ERROR("TimerTest: Some timers did not count!\n");
    }

    //
    // Delete all the timers.
    //

    for (TimerIndex = 0; TimerIndex < TEST_TIMER_COUNT; TimerIndex += 1) {
        Result = timer_delete(TestTimers[TimerIndex]);
        if (Result != 0) {
            PRINT_ERROR("TimerTest: Failed to delete timer: %s.\n",
                        strerror(errno));

            Failures += 1;
        }
    }

    //
    // Restore the signal handler.
    //

    Result = sigaction(SIGALRM, &OriginalAction, NULL);
    if (Result != 0) {
        PRINT_ERROR("TimerTest: Failed to restore SIGALRM: %s.\n",
                    strerror(errno));

        Failures += 1;
    }

    return Failures;
}

VOID
TimerTestAlarmSignalHandler (
    INT SignalNumber,
    siginfo_t *Information,
    void *SomethingElse
    )

/*++

Routine Description:

    This routine implements the alarm signal handler.

Arguments:

    SignalNumber - Supplies the incoming signal, always SIGALRM in this case.

    Information - Supplies a pointer to the signal information structure.

    SomethingElse - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    ULONG TimerIndex;

    assert(SignalNumber == SIGALRM);

    for (TimerIndex = 0; TimerIndex < TEST_TIMER_COUNT; TimerIndex += 1) {
        if (TestTimers[TimerIndex] == Information->si_value.sival_int) {
            TestTimerCount[TimerIndex] += 1;
            break;
        }
    }

    assert(TimerIndex != TEST_TIMER_COUNT);

    return;
}

