/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/lib/minocaos.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

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
// Define the number of timers and threads to fire up.
//

#define TEST_THREAD_TIMER_COUNT 50

//
// Define the number of time ticks all the thread timers should get up to.
//

#define TEST_THREAD_TIMER_GOAL 50

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

ULONG
RunThreadTimerTest (
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

void *
TimerThreadTestStartRoutine (
    void *Parameter
    );

VOID
TimerThreadTestAlarmSignalHandler (
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
// Store the timer thread test timers and threads.
//

pthread_t TestThreads[TEST_THREAD_TIMER_COUNT];
timer_t TestThreadTimers[TEST_THREAD_TIMER_COUNT];
volatile int TestThreadTimerCount[TEST_THREAD_TIMER_COUNT];

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

    Failures += RunThreadTimerTest();
    if (Failures != 0) {
        PRINT_ERROR("*** %d failures in thread timer test. ***\n", Failures);
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

ULONG
RunThreadTimerTest (
    VOID
    )

/*++

Routine Description:

    This routine tests user mode timers using SIGEV_THREAD_ID .

Arguments:

    None.

Return Value:

    Returns the number of failures in the test.

--*/

{

    struct sigaction Action;
    pthread_barrier_t Barrier;
    ULONGLONG CurrentTime;
    ULONGLONG EndTime;
    struct sigevent Event;
    ULONG Failures;
    ULONGLONG Frequency;
    ULONG Index;
    ULONGLONG LastUpdate;
    int MaxCount;
    ULONG MaxTimer;
    int MinCount;
    ULONG MinTimer;
    struct sigaction OriginalAction;
    struct itimerspec Rate;
    int Result;
    BOOL Stop;
    pthread_t Thread;

    Failures = 0;
    Stop = TRUE;

    //
    // Set up the signal handler.
    //

    Action.sa_sigaction = TimerThreadTestAlarmSignalHandler;
    sigemptyset(&(Action.sa_mask));
    Action.sa_flags = SA_SIGINFO;
    Result = sigaction(SIGALRM, &Action, &OriginalAction);
    if (Result != 0) {
        PRINT_ERROR("ThreadTimerTest: sigaction failed: %s\n", strerror(errno));
        return 1;
    }

    //
    // Use a barrier to make the signal threads wait to exit.
    //

    pthread_barrier_init(&Barrier, NULL, TEST_THREAD_TIMER_COUNT + 1);

    //
    // Initialize the portions of the signal event common to each timer.
    //

    memset(&Event, 0, sizeof(struct sigevent));
    Event.sigev_notify = SIGEV_SIGNAL | SIGEV_THREAD_ID;
    Event.sigev_signo = SIGALRM;

    //
    // Create a bunch of timers and a thread to receive the signal for each.
    //

    for (Index = 0; Index < TEST_THREAD_TIMER_COUNT; Index += 1) {
        TestThreads[Index] = -1;
        TestThreadTimers[Index] = -1;
        Result = pthread_create(&Thread,
                                NULL,
                                TimerThreadTestStartRoutine,
                                &Barrier);

        if (Result != 0) {
            PRINT_ERROR("ThreadTimerTest: pthread_create failed: %s.\n",
                        strerror(errno));

            return Failures;
        }

        Event.sigev_value.sival_int = Index;
        Event.sigev_notify_thread_id = pthread_gettid_np(Thread);
        TestThreads[Index] = Thread;
        Result = timer_create(CLOCK_REALTIME,
                              &Event,
                              &(TestThreadTimers[Index]));

        if (Result != 0) {
            PRINT_ERROR("ThreadTimerTest: Failed to create timer: %s.\n",
                        strerror(errno));

            Failures += 1;
        }
    }

    //
    // Arm the timers.
    //

    Rate.it_value.tv_sec = TEST_TIMER_PERIOD_SECONDS;
    Rate.it_value.tv_nsec = TEST_TIMER_PERIOD_NANOSECONDS;
    Rate.it_interval.tv_sec = TEST_TIMER_PERIOD_SECONDS;
    Rate.it_interval.tv_nsec = TEST_TIMER_PERIOD_NANOSECONDS;
    for (Index = 0; Index < TEST_THREAD_TIMER_COUNT; Index += 1) {
        Result = timer_settime(TestThreadTimers[Index], 0, &Rate, NULL);
        if (Result != 0) {
            PRINT_ERROR("ThreadTimerTest: Failed to create timer: %s.\n",
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
        for (Index = 0; Index < TEST_THREAD_TIMER_COUNT; Index += 1) {
            if (TestThreadTimerCount[Index] < TEST_THREAD_TIMER_GOAL) {
                Stop = FALSE;
            }

            if ((MinTimer == -1) || (TestThreadTimerCount[Index] < MinCount)) {
                MinTimer = Index;
                MinCount = TestThreadTimerCount[Index];
            }

            if ((MaxTimer == -1) || (TestThreadTimerCount[Index] > MaxCount)) {
                MaxTimer = Index;
                MaxCount = TestThreadTimerCount[Index];
            }
        }

        if (Stop != FALSE) {
            DEBUG_PRINT("All timer threads reached threshold.\n");
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
        PRINT_ERROR("ThreadTimerTest: Some timers did not count!\n");
    }

    //
    // Delete all the timers so they stop firing.
    //

    for (Index = 0; Index < TEST_THREAD_TIMER_COUNT; Index += 1) {
        Result = timer_delete(TestThreadTimers[Index]);
        if (Result != 0) {
            PRINT_ERROR("ThreadTimerTest: Failed to delete timer: %s.\n",
                        strerror(errno));

            Failures += 1;
        }
    }

    //
    // With the timers destroyed, the threads are free to exit. Wait on the
    // barrier to make sure all threads get released. Then wait for each
    // thread's exit status.
    //

    pthread_barrier_wait(&Barrier);
    pthread_barrier_destroy(&Barrier);
    for (Index = 0; Index < TEST_THREAD_TIMER_COUNT; Index += 1) {
        Result = pthread_join(TestThreads[Index], NULL);
        if (Result != 0) {
            PRINT_ERROR("ThreadTimerTest: Failed to join thread: %s.\n",
                        strerror(errno));

            Failures += 1;
        }
    }

    //
    // Restore the signal handler.
    //

    Result = sigaction(SIGALRM, &OriginalAction, NULL);
    if (Result != 0) {
        PRINT_ERROR("ThreadTimerTest: Failed to restore SIGALRM: %s.\n",
                    strerror(errno));

        Failures += 1;
    }

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

void *
TimerThreadTestStartRoutine (
    void *Parameter
    )

/*++

Routine Description:

    This routine implements the start routine for a timer thread test.

Arguments:

    Parameter - Supplies a pointer to a POSIX thread barrier to wait on.

Return Value:

    Returns NULL.

--*/

{

    pthread_barrier_t *Barrier;

    //
    // Wait on the barrier.
    //

    Barrier = (pthread_barrier_t *)Parameter;
    pthread_barrier_wait(Barrier);
    return NULL;
}

VOID
TimerThreadTestAlarmSignalHandler (
    INT SignalNumber,
    siginfo_t *Information,
    void *SomethingElse
    )

/*++

Routine Description:

    This routine implements the alarm signal handler for the timer thread test.

Arguments:

    SignalNumber - Supplies the incoming signal, always SIGALRM in this case.

    Information - Supplies a pointer to the signal information structure.

    SomethingElse - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    assert(SignalNumber == SIGALRM);

    TestThreadTimerCount[Information->si_value.sival_int] += 1;
    return;
}

