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
#include <sys/time.h>
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
    VOID
    );

ULONG
RunTimerTest (
    VOID
    );

ULONG
RunITimerTest (
    VOID
    );

VOID
ITimerTestSignalHandler (
    INT SignalNumber
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

volatile int TestTimerSignals[NSIG];
int TestITimerTypes[3] = {
    ITIMER_REAL,
    ITIMER_VIRTUAL,
    ITIMER_PROF
};

int TestITimerTypeSignals[3] = {
    SIGALRM,
    SIGVTALRM,
    SIGPROF
};

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
    VOID
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
    }

    Failures += RunITimerTest();
    if (Failures != 0) {
        PRINT_ERROR("*** %d failures in itimer test. ***\n", Failures);
    }

    if (Failures == 0) {
        DEBUG_PRINT("All timer tests pass.\n");
    }

    return Failures;
}

ULONG
RunTimerTest (
    VOID
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

ULONG
RunITimerTest (
    VOID
    )

/*++

Routine Description:

    This routine tests user mode interval timers.

Arguments:

    None.

Return Value:

    Returns the number of failures in the test.

--*/

{

    struct sigaction Action;
    int Actual;
    struct timespec EndTime;
    int Expected;
    ULONG Failures;
    struct sigaction OldAlarm;
    struct sigaction OldProfile;
    struct sigaction OldVirtualAlarm;
    int Signal;
    struct timespec Time;
    int Tolerance;
    int Type;
    struct itimerval Value;

    Failures = 0;
    memset((void *)TestTimerSignals, 0, sizeof(TestTimerSignals));
    memset(&Action, 0, sizeof(Action));
    Action.sa_handler = ITimerTestSignalHandler;
    if ((sigaction(SIGALRM, &Action, &OldAlarm) != 0) ||
        (sigaction(SIGVTALRM, &Action, &OldVirtualAlarm) != 0) ||
        (sigaction(SIGPROF, &Action, &OldProfile) != 0)) {

        PRINT_ERROR("TimerTest: Failed to set signal handlers: %s.\n",
                    strerror(errno));

        return 1;
    }

    //
    // Ensure that wacky values don't work.
    //

    memset(&Value, 0, sizeof(Value));
    if ((setitimer(33, &Value, NULL) != -1) || (errno != EINVAL)) {
        PRINT_ERROR("TimerTest: Wacky itimer type succeeded.\n");
        Failures += 1;
    }

    Value.it_value.tv_usec = 1000001;
    if ((setitimer(ITIMER_REAL, &Value, NULL) != -1) || (errno != EINVAL)) {
        PRINT_ERROR("TimerTest: Wacky itimer value succeeded.\n");
        Failures += 1;
    }

    Value.it_value.tv_sec = 2;
    Value.it_value.tv_usec = 500000;
    Value.it_interval.tv_usec = 1000001;
    if ((setitimer(ITIMER_REAL, &Value, NULL) != -1) || (errno != EINVAL)) {
        PRINT_ERROR("TimerTest: Wacky itimer period succeeded.\n");
        Failures += 1;
    }

    //
    // Getting the timers before setting them should return zero.
    //

    for (Type = 0; Type < 3; Type += 1) {
        if (getitimer(TestITimerTypes[Type], &Value) != 0) {
            PRINT_ERROR("TimerTest: getitimer failed.\n");
            Failures += 1;
        }

        if ((Value.it_value.tv_sec | Value.it_value.tv_usec) != 0) {
            PRINT_ERROR("TimerTest: getitimer had a value!\n");
            Failures += 1;
        }
    }

    //
    // Create timers with a period.
    //

    for (Type = 0; Type < 3; Type += 1) {
        Value.it_value.tv_sec = 2;
        Value.it_value.tv_usec = 500000;
        Value.it_interval.tv_sec = 1;
        Value.it_interval.tv_usec = 250000;

        //
        // Get aligned to a one second boundary.
        //

        if (clock_gettime(CLOCK_REALTIME, &Time) != 0) {
            PRINT_ERROR("TimerTest: clock_gettime(CLOCK_REALTIME) failed.\n");
            Failures += 1;
        }

        do {
            clock_gettime(CLOCK_REALTIME, &EndTime);

        } while (Time.tv_sec == EndTime.tv_sec);

        //
        // Set the timer.
        //

        if (setitimer(TestITimerTypes[Type], &Value, NULL) != 0) {
            PRINT_ERROR("TimerTest: setitimer failed.\n");
            Failures += 1;
        }

        if (getitimer(TestITimerTypes[Type], &Value) != 0) {
            PRINT_ERROR("TimerTest: getitimer failed.\n");
            Failures += 1;
        }

        if ((Value.it_value.tv_sec != 2) ||
            (Value.it_value.tv_usec >= 500000) ||
            (Value.it_interval.tv_sec != 1) ||
            (abs(Value.it_interval.tv_usec - 250000) > 1000)) {

            PRINT_ERROR("TimerTest: getitimer value was off: "
                        "%lld.%d %lld.%d.\n",
                        (LONGLONG)Value.it_value.tv_sec,
                        Value.it_value.tv_usec,
                        (LONGLONG)Value.it_interval.tv_sec,
                        Value.it_interval.tv_usec);

            Failures += 1;
        }

        //
        // 2.5 + (4 * 1.25) = 7.5. So wait 8 seconds. Sleep for the first 3,
        // which should not affect the real timer but should delay the virtual
        // ones.
        //

        sleep(3);
        clock_gettime(CLOCK_REALTIME, &Time);
        if (TestITimerTypes[Type] == ITIMER_REAL) {
            if (Time.tv_sec != EndTime.tv_sec + 2) {
                PRINT_ERROR("TimerTest: RealTime itimer did not interrupt "
                            "sleep.\n");

                Failures += 1;
            }

        } else {
            if (Time.tv_sec != EndTime.tv_sec + 3) {
                PRINT_ERROR("TimerTest: Virtual itimer interrupted sleep.\n");
                Failures += 1;
            }
        }

        //
        // Busy spin for the remaining 5 or 5.5 seconds.
        //

        EndTime.tv_sec += 8;
        do {
            clock_gettime(CLOCK_REALTIME, &Time);

        } while (Time.tv_sec < EndTime.tv_sec);

        //
        // Now stop the timer and see how many signals came in.
        //

        memset(&Value, 0, sizeof(Value));
        if (setitimer(TestITimerTypes[Type], &Value, NULL) != 0) {
            PRINT_ERROR("TimerTest: setitimer failed.\n");
            Failures += 1;
        }

        Signal = TestITimerTypeSignals[Type];
        Actual = TestTimerSignals[Signal];
        TestTimerSignals[Signal] = 0;
        if (TestITimerTypes[Type] == ITIMER_REAL) {
            Expected = 5;
            Tolerance = 0;

        } else if (TestITimerTypes[Type] == ITIMER_PROF) {
            Expected = 3;
            Tolerance = 1;

        } else {
            Expected = 3;
            Tolerance = 2;
        }

        if (!((Actual >= Expected - Tolerance) &&
              (Actual <= Expected + Tolerance))) {

            PRINT_ERROR("TimerTest: Expected %d interrupts for timer type "
                        "%d (tolerance %d), got %d.\n",
                        Expected,
                        TestITimerTypes[Type],
                        Tolerance,
                        Actual);

            Failures += 1;
        }
    }

    //
    // Ensure that there are no extra signals.
    //

    for (Signal = 0; Signal < NSIG; Signal += 1) {
        if (TestTimerSignals[Signal] != 0) {
            PRINT_ERROR("TimerTest: %d extra %d signals.\n",
                        TestTimerSignals[Signal],
                        Signal);

            Failures += 1;
        }
    }

    sigaction(SIGALRM, &OldAlarm, NULL);
    sigaction(SIGVTALRM, &OldVirtualAlarm, NULL);
    sigaction(SIGPROF, &OldProfile, NULL);
    return Failures;
}

VOID
ITimerTestSignalHandler (
    INT SignalNumber
    )

/*++

Routine Description:

    This routine implements the signal handler for the interval timer test.

Arguments:

    SignalNumber - Supplies the incoming signal, always SIGALRM in this case.

Return Value:

    None.

--*/

{

    TestTimerSignals[SignalNumber] += 1;
    return;
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

