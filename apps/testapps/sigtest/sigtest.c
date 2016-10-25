/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sigtest.c

Abstract:

    This module implements the tests used to verify that user mode signals are
    functioning properly.

Author:

    Evan Green 31-Mar-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>
#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>

//
// --------------------------------------------------------------------- Macros
//

#define DEBUG_PRINT(...)                                  \
    if (SignalTestVerbosity >= TestVerbosityDebug) {      \
        printf(__VA_ARGS__);                              \
    }

#define PRINT(...)                                        \
    if (SignalTestVerbosity >= TestVerbosityNormal) {     \
        printf(__VA_ARGS__);                              \
    }

#define PRINT_ERROR(...) fprintf(stderr, "sigtest: " __VA_ARGS__)

#define DEFAULT_OPERATION_COUNT 10
#define DEFAULT_CHILD_PROCESS_COUNT 3
#define DEFAULT_THREAD_COUNT 1

#define SIGNAL_TEST_CONTEXT_STACK_SIZE 16384

//
// ---------------------------------------------------------------- Definitions
//

#define SIGNAL_TEST_VERSION_MAJOR 1
#define SIGNAL_TEST_VERSION_MINOR 0

#define SIGNAL_TEST_USAGE                                                      \
    "Usage: sigtest [options] \n"                                              \
    "This utility hammers on signals. Options are:\n"                          \
    "  -c, --child-count <count> -- Set the number of child processes.\n"      \
    "  -i, --iterations <count> -- Set the number of operations to perform.\n" \
    "  -p, --threads <count> -- Set the number of threads to spin up to \n"    \
    "      simultaneously run the test.\n"                                     \
    "  -t, --test -- Set the test to perform. Valid values are all, \n"        \
    "      waitpid, sigchld, quickwait, nested, and context.\n"                \
    "  --debug -- Print lots of information about what's happening.\n"         \
    "  --quiet -- Print only errors.\n"                                        \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the test version and exit.\n"                        \

#define SIGNAL_TEST_OPTIONS_STRING "c:i:t:p:"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TEST_VERBOSITY {
    TestVerbosityQuiet,
    TestVerbosityNormal,
    TestVerbosityDebug
} TEST_VERBOSITY, *PTEST_VERBOSITY;

typedef enum _SIGNAL_TEST_TYPE {
    SignalTestAll,
    SignalTestWaitpid,
    SignalTestSigchld,
    SignalTestQuickWait,
    SignalTestNested,
    SignalTestContext,
} SIGNAL_TEST_TYPE, *PSIGNAL_TEST_TYPE;

typedef enum _SIGNAL_TEST_WAIT_TYPE {
    SignalTestWaitBusy,
    SignalTestWaitSigsuspend,
    SignalTestWaitSigwait,
    SignalTestWaitSigwaitinfo,
    SignalTestWaitSigtimedwait,
    SignalTestWaitTypeCount
} SIGNAL_TEST_WAIT_TYPE, *PSIGNAL_TEST_WAIT_TYPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
RunWaitpidTest (
    ULONG Iterations
    );

ULONG
RunSigchldTest (
    ULONG Iterations,
    ULONG ChildCount
    );

ULONG
RunQuickWaitTest (
    ULONG Iterations,
    ULONG ChildCount
    );

ULONG
TestWaitpid (
    BOOL BurnTimeInChild,
    BOOL BurnTimeInParent
    );

ULONG
TestSigchild (
    ULONG ChildCount,
    ULONG ChildAdditionalThreads,
    SIGNAL_TEST_WAIT_TYPE WaitType,
    BOOL ChildrenExitVoluntarily
    );

void
TestWaitpidChildSignalHandler (
    int Signal,
    siginfo_t *SignalInformation,
    void *Context
    );

void
TestWaitpidProcessChildSignal (
    int Signal,
    siginfo_t *SignalInformation
    );

void
TestSigchldRealtime1SignalHandler (
    int Signal,
    siginfo_t *SignalInformation,
    void *Context
    );

PVOID
TestThreadSpinForever (
    PVOID Parameter
    );

ULONG
RunNestedSignalsTest (
    VOID
    );

VOID
TestNestedSignalHandler (
    int Signal,
    siginfo_t *Info,
    void *Ignored
    );

ULONG
RunSetContextTest (
    VOID
    );

ULONG
TestContextSwap (
    BOOL Exit
    );

VOID
TestMakecontext (
    ucontext_t *OldContext,
    ucontext_t *NextContext,
    INT Identifier
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Higher levels here print out more stuff.
//

TEST_VERBOSITY SignalTestVerbosity = TestVerbosityNormal;

struct option SignalTestLongOptions[] = {
    {"child-count", required_argument, 0, 'c'},
    {"iterations", required_argument, 0, 'i'},
    {"threads", required_argument, 0, 'p'},
    {"test", required_argument, 0, 't'},
    {"debug", no_argument, 0, 'd'},
    {"quiet", no_argument, 0, 'q'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// These variables communicate between the signal handler and main function.
//

volatile ULONG ChildSignalsExpected;
volatile LONG ChildSignalPid;
volatile ULONG ChildSignalFailures;
volatile ULONG ChildProcessesReady;

int SigtestWritePipe;
volatile int SigtestSignalCount[2];

PSTR SignalTestWaitTypeStrings[SignalTestWaitTypeCount] = {
    "busy spin",
    "sigsuspend",
    "sigwait",
    "sigwaitinfo",
    "sigtimedwait"
};

int SigtestContextHits;

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

    PSTR AfterScan;
    pid_t Child;
    INT ChildIndex;
    INT ChildProcessCount;
    pid_t *Children;
    INT Failures;
    BOOL IsParent;
    INT Iterations;
    INT Option;
    INT Status;
    SIGNAL_TEST_TYPE Test;
    INT Threads;

    Children = NULL;
    Failures = 0;
    ChildProcessCount = DEFAULT_CHILD_PROCESS_COUNT;
    Iterations = DEFAULT_OPERATION_COUNT;
    Test = SignalTestAll;
    Threads = DEFAULT_THREAD_COUNT;
    Status = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    srand(time(NULL));

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SIGNAL_TEST_OPTIONS_STRING,
                             SignalTestLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'c':
            ChildProcessCount = strtol(optarg, &AfterScan, 0);
            if ((ChildProcessCount <= 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid child process count %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'i':
            Iterations = strtol(optarg, &AfterScan, 0);
            if ((Iterations < 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid iteration count %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'p':
            Threads = strtol(optarg, &AfterScan, 0);
            if ((Threads <= 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid thread count %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 't':
            if (strcasecmp(optarg, "all") == 0) {
                Test = SignalTestAll;

            } else if (strcasecmp(optarg, "waitpid") == 0) {
                Test = SignalTestWaitpid;

            } else if (strcasecmp(optarg, "sigchld") == 0) {
                Test = SignalTestSigchld;

            } else if (strcasecmp(optarg, "quickwait") == 0) {
                Test = SignalTestQuickWait;

            } else if (strcasecmp(optarg, "nested") == 0) {
                Test = SignalTestNested;

            } else if (strcasecmp(optarg, "context") == 0) {
                Test = SignalTestContext;

            } else {
                PRINT_ERROR("Invalid test: %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'd':
            SignalTestVerbosity = TestVerbosityDebug;
            break;

        case 'q':
            SignalTestVerbosity = TestVerbosityQuiet;
            break;

        case 'V':
            printf("Minoca signal test version %d.%d\n",
                   SIGNAL_TEST_VERSION_MAJOR,
                   SIGNAL_TEST_VERSION_MINOR);

            return 1;

        case 'h':
            printf(SIGNAL_TEST_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    IsParent = TRUE;
    if (Threads > 1) {
        Children = malloc(sizeof(pid_t) * (Threads - 1));
        if (Children == NULL) {
            Status = ENOMEM;
            goto MainEnd;
        }

        memset(Children, 0, sizeof(pid_t) * (Threads - 1));
        for (ChildIndex = 0; ChildIndex < Threads - 1; ChildIndex += 1) {
            Child = fork();

            //
            // If this is the child, break out and run the tests.
            //

            if (Child == 0) {
                srand(time(NULL) + ChildIndex);
                IsParent = FALSE;
                break;
            }

            Children[ChildIndex] = Child;
        }
    }

    //
    // Run the tests.
    //

    if ((Test == SignalTestAll) || (Test == SignalTestWaitpid)) {
        Failures += RunWaitpidTest(Iterations);
    }

    if ((Test == SignalTestAll) || (Test == SignalTestSigchld)) {
        Failures += RunSigchldTest(Iterations, ChildProcessCount);
    }

    if ((Test == SignalTestAll) || (Test == SignalTestQuickWait)) {
        Failures += RunQuickWaitTest(Iterations, ChildProcessCount);
    }

    if ((Test == SignalTestAll) || (Test == SignalTestNested)) {
        Failures += RunNestedSignalsTest();
    }

    if ((Test == SignalTestAll) || (Test == SignalTestContext)) {
        Failures += RunSetContextTest();
    }

    //
    // Wait for any children.
    //

    if (IsParent != FALSE) {
        if (Threads > 1) {
            for (ChildIndex = 0; ChildIndex < Threads - 1; ChildIndex += 1) {
                Child = waitpid(Children[ChildIndex], &Status, 0);
                if (Child == -1) {
                    PRINT_ERROR("Failed to wait for child %d: %s.\n",
                                Children[ChildIndex],
                                strerror(errno));

                    Status = errno;

                } else {

                    assert(Child == Children[ChildIndex]);

                    if (!WIFEXITED(Status)) {
                        PRINT_ERROR("Child %d returned with status %x\n",
                                    Child,
                                    Status);

                        Failures += 1;
                    }

                    Failures += WEXITSTATUS(Status);
                    Status = 0;
                }
            }
        }

    //
    // If this is a child, just report back the number of failures to the
    // parent.
    //

    } else {
        if (Failures > 100) {
            exit(100);

        } else {
            exit(Failures);
        }
    }

MainEnd:
    if (Children != NULL) {
        free(Children);
    }

    if (Status != 0) {
        PRINT_ERROR("Error: %d.\n", Status);
    }

    if (Failures != 0) {
        PRINT_ERROR("\n   *** %d failures in signal test ***\n", Failures);
        return Failures;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
RunWaitpidTest (
    ULONG Iterations
    )

/*++

Routine Description:

    This routine runs several variations of the waitpid test.

Arguments:

    Iterations - Supplies the number of times to run the test.

Return Value:

    Returns the number of failures in the test.

--*/

{

    ULONG Errors;
    ULONG Iteration;
    ULONG Percent;

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    PRINT("Running waitpid test with %d iterations.\n", Iterations);
    Errors = 0;
    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {
        Errors += TestWaitpid(FALSE, FALSE);
        Errors += TestWaitpid(TRUE, FALSE);
        Errors += TestWaitpid(FALSE, TRUE);
        Errors += TestWaitpid(TRUE, TRUE);
        if ((Iteration % Percent) == 0) {
            PRINT("w");
        }
    }

    PRINT("\n");
    return Errors;
}

ULONG
RunSigchldTest (
    ULONG Iterations,
    ULONG ChildCount
    )

/*++

Routine Description:

    This routine runs several variations of the waitpid test.

Arguments:

    Iterations - Supplies the number of times to run the test.

    ChildCount - Supplies the number of child processes to spin up and wait
        for.

Return Value:

    Returns the number of failures in the test.

--*/

{

    ULONG Errors;
    ULONG Iteration;
    ULONG Percent;
    ULONG WaitType;

    PRINT("Running sigchld test with %d iterations and %d children.\n",
          Iterations,
          ChildCount);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    Errors = 0;
    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {
        for (WaitType = 0; WaitType < SignalTestWaitTypeCount; WaitType += 1) {
            Errors += TestSigchild(ChildCount, 3, WaitType, FALSE);
            Errors += TestSigchild(ChildCount, 3, WaitType, TRUE);
        }

        if ((Iteration % Percent) == 0) {
            PRINT("c");
        }
    }

    PRINT("\n");
    return Errors;
}

ULONG
RunQuickWaitTest (
    ULONG Iterations,
    ULONG ChildCount
    )

/*++

Routine Description:

    This routine runs the quick wait test, which just forks a process that dies
    and waits for it.

Arguments:

    Iterations - Supplies the number of times to run the test.

    ChildCount - Supplies the number of child processes to spin up and wait
        for.

Return Value:

    Returns the number of failures in the test.

--*/

{

    pid_t Child;
    LONG ChildIndex;
    pid_t *Children;
    ULONG Failures;
    ULONG Iteration;
    ULONG Percent;
    int Status;

    Failures = 0;
    PRINT("Running QuickWait test with %d iterations and %d children.\n",
          Iterations,
          ChildCount);

    assert(ChildCount != 0);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    Children = malloc(sizeof(pid_t) * ChildCount);
    if (Children == NULL) {
        Failures += 1;
        goto RunQuickWaitTestEnd;
    }

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {
        memset(Children, 0, sizeof(pid_t) * ChildCount);

        //
        // Loop creating all the child processes.
        //

        for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex += 1) {
            Child = fork();
            if (Child == -1) {
                PRINT_ERROR("Failed to fork: %s.\n", strerror(errno));
                Failures += 1;
                continue;
            }

            //
            // If this is the child, die immediately.
            //

            if (Child == 0) {
                exit(ChildIndex);
            }

            Children[ChildIndex] = Child;
        }

        //
        // Loop reaping all the child processes. Backwards, for added flavor.
        //

        for (ChildIndex = ChildCount - 1; ChildIndex >= 0; ChildIndex -= 1) {
            Child = waitpid(Children[ChildIndex], &Status, 0);
            if (Child == -1) {
                PRINT_ERROR("Failed to wait for child %d: %s.\n",
                            Child,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            if ((!WIFEXITED(Status)) ||
                (WEXITSTATUS(Status) != (ChildIndex & 0x7F))) {

                PRINT_ERROR("Child returned with invalid status %x\n", Status);
                Failures += 1;
            }
        }

        if ((Iteration % Percent) == 0) {
            PRINT("q");
        }
    }

    PRINT("\n");

RunQuickWaitTestEnd:
    if (Children != NULL) {
        free(Children);
    }

    return Failures;
}

ULONG
TestWaitpid (
    BOOL BurnTimeInChild,
    BOOL BurnTimeInParent
    )

/*++

Routine Description:

    This routine tests that an application can exit, be waited on, and
    successfully report its status.

Arguments:

    BurnTimeInParent - Supplies a boolean indicating if some time should be
        wasted in the parent process.

    BurnTimeInChild - Supplies a boolean indicating if some time should be
        wasted in the child process.

Return Value:

    Returns the number of failures in the test.

--*/

{

    pid_t Child;
    struct sigaction ChildAction;
    sigset_t ChildSignalMask;
    ULONG Errors;
    struct sigaction OriginalChildAction;
    sigset_t OriginalSignalMask;
    int Status;
    pid_t WaitPid;

    //
    // Block child signals, and set up a handler.
    //

    sigemptyset(&ChildSignalMask);
    sigaddset(&ChildSignalMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &ChildSignalMask, &OriginalSignalMask);
    ChildAction.sa_sigaction = TestWaitpidChildSignalHandler;
    sigemptyset(&(ChildAction.sa_mask));
    ChildAction.sa_flags = SA_NODEFER | SA_SIGINFO;
    sigaction(SIGCHLD, &ChildAction, &OriginalChildAction);
    Errors = 0;
    Child = fork();
    if (Child == -1) {
        PRINT_ERROR("Failed to fork()!\n");
        return 1;
    }

    //
    // If this is the child process, exit with a specific status code. Only the
    // first 8 bits can be accessed with the macro.
    //

    if (Child == 0) {
        if (BurnTimeInChild != FALSE) {
            sleep(1);
        }

        DEBUG_PRINT("Child %d exiting with status 99.\n", getpid());
        exit(99);

    //
    // In the parent process, wait for the child.
    //

    } else {
        if (BurnTimeInParent != FALSE) {
            sleep(1);
        }

        DEBUG_PRINT("Parent waiting for child %d.\n", Child);
        Status = 0;
        WaitPid = waitpid(Child, &Status, WUNTRACED | WCONTINUED);
        if (WaitPid != Child) {
            PRINT_ERROR("waitpid returned %d instead of child pid %d.\n",
                        WaitPid,
                        Child);

            Errors += 1;
        }

        //
        // Check the flags and return value.
        //

        if ((!WIFEXITED(Status)) ||
            (WIFCONTINUED(Status)) ||
            (WIFSIGNALED(Status)) ||
            (WIFSTOPPED(Status))) {

            PRINT_ERROR("Child status was not exited as expected. Was %x\n",
                        Status);

            Errors += 1;
        }

        if (WEXITSTATUS(Status) != 99) {
            PRINT_ERROR("Child exit status was an unexpected %d.\n",
                        WEXITSTATUS(Status));

            Errors += 1;
        }
    }

    //
    // Restore the original signal mask.
    //

    sigaction(SIGCHLD, &OriginalChildAction, NULL);
    sigprocmask(SIG_SETMASK, &OriginalSignalMask, NULL);
    Errors += ChildSignalFailures;
    ChildSignalFailures = 0;
    return Errors;
}

ULONG
TestSigchild (
    ULONG ChildCount,
    ULONG ChildAdditionalThreads,
    SIGNAL_TEST_WAIT_TYPE WaitType,
    BOOL ChildrenExitVoluntarily
    )

/*++

Routine Description:

    This routine tests child signals.

Arguments:

    ChildCount - Supplies the number of simultaneous children to create.

    ChildAdditionalThreads - Supplies the number of additional threads each
        child should spin up.

    WaitType - Supplies the type of waiting that should be used in the main
        loop.

    ChildrenExitVoluntarily - Supplies a boolean indicating whether children
        exit on their own or need to be killed.

Return Value:

    Returns the number of failures in the test.

--*/

{

    pid_t Child;
    struct sigaction ChildAction;
    ULONG ChildIndex;
    volatile ULONG ChildInitializing;
    pid_t *Children;
    sigset_t ChildSignalMask;
    time_t EndTime;
    ULONG Errors;
    struct sigaction OriginalChildAction;
    struct sigaction OriginalRealtimeAction;
    sigset_t OriginalSignalMask;
    siginfo_t SignalInformation;
    int SignalNumber;
    union sigval SignalValue;
    int Status;
    pthread_t Thread;
    ULONG ThreadIndex;
    struct timespec Timeout;
    pid_t WaitPid;

    if (WaitType >= SignalTestWaitTypeCount) {
        PRINT_ERROR("Invalid wait type %d.\n", WaitType);
        return 1;
    }

    //
    // Allocate child array.
    //

    DEBUG_PRINT("Testing SIGCHLD: %d children each with %d extra "
                "threads. WaitType: %s, ChildrenExitVoluntarily: "
                "%d.\n\n",
                ChildCount,
                ChildAdditionalThreads,
                SignalTestWaitTypeStrings[WaitType],
                ChildrenExitVoluntarily);

    Children = malloc(sizeof(pid_t) * ChildCount);
    if (Children == NULL) {
        PRINT_ERROR("Failed to malloc %d bytes.\n", sizeof(pid_t) * ChildCount);
        return 1;
    }

    RtlZeroMemory(Children, sizeof(pid_t) * ChildCount);

    //
    // Block child signals, and set up a handler.
    //

    sigemptyset(&ChildSignalMask);
    sigaddset(&ChildSignalMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &ChildSignalMask, &OriginalSignalMask);
    ChildAction.sa_sigaction = TestWaitpidChildSignalHandler;
    sigemptyset(&(ChildAction.sa_mask));
    ChildAction.sa_flags = SA_NODEFER | SA_SIGINFO;
    sigaction(SIGCHLD, &ChildAction, &OriginalChildAction);
    ChildAction.sa_sigaction = TestSigchldRealtime1SignalHandler;
    sigaction(SIGRTMIN + 0, &ChildAction, &OriginalRealtimeAction);
    Errors = 0;

    //
    // Create child processes.
    //

    ChildProcessesReady = 0;
    ChildSignalsExpected = ChildCount;
    Child = -1;
    for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex += 1) {
        Child = fork();
        if (Child == -1) {
            PRINT_ERROR("Failed to fork()!\n");
            return 1;
        }

        //
        // If this is the child process, spin up any additional threads
        // requested, send the signal once everything's up and running, and
        // exit.
        //

        if (Child == 0) {
            DEBUG_PRINT("Child %d alive.\n", getpid());
            for (ThreadIndex = 0;
                 ThreadIndex < ChildAdditionalThreads;
                 ThreadIndex += 1) {

                ChildInitializing = 1;
                Status = pthread_create(&Thread,
                                        NULL,
                                        TestThreadSpinForever,
                                        (PVOID)&ChildInitializing);

                if (Status != 0) {
                    PRINT_ERROR("Child %d failed to create thread: %d.\n",
                                getpid(),
                                Status);
                }

                //
                // Wait for the thread to come to life and start doing
                // something.
                //

                EndTime = time(NULL) + 10;
                while (time(NULL) <= EndTime) {
                    if (ChildInitializing == 0) {
                        break;
                    }
                }

                if (ChildInitializing != 0) {
                    PRINT_ERROR("Thread failed to initialize!\n");
                }
            }

            //
            // Send a signal to the parent letting them know everything's
            // initialized.
            //

            SignalValue.sival_int = getpid();
            Status = sigqueue(getppid(), SIGRTMIN + 0, SignalValue);
            if (Status != 0) {
                PRINT_ERROR("Failed to sigqueue to parent: errno %d.\n",
                            errno);
            }

            //
            // Exit the process or spin forever.
            //

            if (ChildrenExitVoluntarily != FALSE) {
                DEBUG_PRINT("Child %d exiting with status 99.\n", getpid());
                exit(99);

            } else {
                DEBUG_PRINT("Child %d spinning forever.\n", getpid());
                while (TRUE) {
                    sleep(1);
                }
            }

        //
        // This is the parent process, save the child PID.
        //

        } else {
            Children[ChildIndex] = Child;
        }
    }

    //
    // This is the parent process, wait for all processes to be ready.
    //

    EndTime = time(NULL) + 30;
    while (time(NULL) <= EndTime) {
        if (ChildProcessesReady == ChildCount) {
            break;
        }
    }

    if (ChildProcessesReady != ChildCount) {
        PRINT_ERROR("Only %d of %d children ready.\n",
                    ChildProcessesReady,
                    ChildCount);

        Errors += 1;
    }

    //
    // If the children aren't going to go quietly, kill them.
    //

    if (ChildrenExitVoluntarily == FALSE) {
        for (ChildIndex = 0; ChildIndex < ChildCount; ChildIndex += 1) {
            DEBUG_PRINT("Killing child index %d PID %d.\n",
                        ChildIndex,
                        Children[ChildIndex]);

            Status = kill(Children[ChildIndex], SIGKILL);
            if (Status != 0) {
                PRINT_ERROR("Failed to kill pid %d, errno %d.\n",
                            Children[ChildIndex],
                            errno);

                Errors += 1;
            }
        }
    }

    //
    // In the parent process, wait for the children.
    //

    DEBUG_PRINT("Parent waiting for children via %s.\n",
                SignalTestWaitTypeStrings[WaitType]);

    EndTime = time(NULL) + 30;
    Status = 0;
    switch (WaitType) {
    case SignalTestWaitSigsuspend:
        while (time(NULL) <= EndTime) {
            if (ChildSignalsExpected == 0) {
                break;
            }

            DEBUG_PRINT("Expecting %d more child signals. "
                        "Running sigsuspend.\n",
                        ChildSignalsExpected);

            sigsuspend(&OriginalSignalMask);
            DEBUG_PRINT("Returned from sigsuspend.\n");
        }

        break;

    case SignalTestWaitSigwait:
        while (time(NULL) <= EndTime) {
            if (ChildSignalsExpected == 0) {
                break;
            }

            DEBUG_PRINT("Expecting %d more child signals. "
                        "Running sigwait.\n",
                        ChildSignalsExpected);

            Status = sigwait(&ChildSignalMask, &SignalNumber);
            DEBUG_PRINT("Returned from sigwait.\n");
            if (Status != 0) {
                PRINT_ERROR("Failed sigwait: %s.\n", strerror(Status));
                Errors += 1;
                continue;
            }

            //
            // The signal handler was not called and the parameters are not
            // available, so just process the signal without the parameters.
            //

            TestWaitpidProcessChildSignal(SignalNumber, NULL);
        }

        break;

    case SignalTestWaitSigwaitinfo:
        while (time(NULL) <= EndTime) {
            if (ChildSignalsExpected == 0) {
                break;
            }

            DEBUG_PRINT("Expecting %d more child signals. "
                        "Running sigwaitinfo.\n",
                        ChildSignalsExpected);

            SignalNumber = sigwaitinfo(&ChildSignalMask, &SignalInformation);
            DEBUG_PRINT("Returned from sigwaitinfo.\n");
            if (SignalNumber == -1) {
                if (errno != EINTR) {
                    PRINT_ERROR("Failed sigwaitinfo: %s.\n", strerror(errno));
                    Errors += 1;
                }

                continue;
            }

            //
            // Handle the signal in-line as the handler was not called.
            //

            TestWaitpidProcessChildSignal(SignalNumber, &SignalInformation);
        }

        break;

    case SignalTestWaitSigtimedwait:
        Timeout.tv_nsec = 0;
        Timeout.tv_sec = 1;
        while (time(NULL) <= EndTime) {
            if (ChildSignalsExpected == 0) {
                break;
            }

            DEBUG_PRINT("Expecting %d more child signals. "
                        "Running sigtimedwait.\n",
                        ChildSignalsExpected);

            SignalNumber = sigtimedwait(&ChildSignalMask,
                                        &SignalInformation,
                                        &Timeout);

            DEBUG_PRINT("Returned from sigtimedwait.\n");
            if (SignalNumber == -1) {
                if (errno == EAGAIN) {
                    DEBUG_PRINT("sigtimedwait timed out. Retrying.\n");

                } else if (errno != EINTR) {
                    PRINT_ERROR("Failed sigtimedwait: %s.\n", strerror(errno));
                    Errors += 1;
                }

                continue;
            }

            //
            // Handle the signal in-line as the handler was not called.
            //

            TestWaitpidProcessChildSignal(SignalNumber, &SignalInformation);
        }

        break;

    case SignalTestWaitBusy:
    default:
        sigprocmask(SIG_UNBLOCK, &ChildSignalMask, NULL);
        while (time(NULL) <= EndTime) {
            if (ChildSignalsExpected == 0) {
                break;
            }
        }

        sigprocmask(SIG_BLOCK, &ChildSignalMask, NULL);
        break;
    }

    if (ChildSignalsExpected != 0) {
        PRINT_ERROR("Error: Never saw SIGCHLD.\n");
        Errors += 1;
    }

    ChildSignalsExpected = 0;

    //
    // Waitpid better not find anything.
    //

    WaitPid = waitpid(-1, &Status, WUNTRACED | WCONTINUED | WNOHANG);
    if (WaitPid > 0) {
        PRINT_ERROR("Error: waitpid unexpectedly gave up a %d\n", WaitPid);
        Errors += 1;
    }

    if (ChildSignalFailures != 0) {
        PRINT_ERROR("Error: %d child signal failures.\n", ChildSignalFailures);
    }

    Errors += ChildSignalFailures;
    ChildSignalFailures = 0;
    ChildProcessesReady = 0;

    //
    // Restore the original signal mask.
    //

    sigaction(SIGCHLD, &OriginalChildAction, NULL);
    sigaction(SIGRTMIN + 0, &OriginalRealtimeAction, NULL);
    sigprocmask(SIG_SETMASK, &OriginalSignalMask, NULL);
    free(Children);
    DEBUG_PRINT("Done with SIGCHLD test.\n");
    return Errors;
}

void
TestWaitpidChildSignalHandler (
    int Signal,
    siginfo_t *SignalInformation,
    void *Context
    )

/*++

Routine Description:

    This routine responds to child signals.

Arguments:

    Signal - Supplies the signal number coming in, in this case always SIGCHLD.

    SignalInformation - Supplies a pointer to the signal information.

    Context - Supplies a pointer to some unused context information.

Return Value:

    None.

--*/

{

    TestWaitpidProcessChildSignal(Signal, SignalInformation);
    return;
}

void
TestWaitpidProcessChildSignal (
    int Signal,
    siginfo_t *SignalInformation
    )

/*++

Routine Description:

    This routine processes a child signal.

Arguments:

    Signal - Supplies the signal number coming in, in this case always SIGCHLD.

    SignalInformation - Supplies an optional pointer to the signal information.

Return Value:

    None.

--*/

{

    int PidStatus;
    BOOL SignaledPidFound;
    int Status;
    LONG WaitPidResult;

    if (SignalInformation != NULL) {
        DEBUG_PRINT("SIGCHLD Pid %d Status %d.\n",
                    SignalInformation->si_pid,
                    SignalInformation->si_status);
    }

    if (Signal != SIGCHLD) {
        PRINT_ERROR("Error: Signal %d came in instead of SIGCHLD.\n", Signal);
        ChildSignalFailures += 1;
    }

    if (ChildSignalsExpected == 0) {
        PRINT_ERROR("Error: Unexpected child signal.\n");
        ChildSignalFailures += 1;
    }

    if (SignalInformation != NULL) {
        if (SignalInformation->si_signo != SIGCHLD) {
            PRINT_ERROR("Error: Signal %d came in si_signo instead of "
                        "SIGCHLD.\n",
                        SignalInformation->si_signo);

            ChildSignalFailures += 1;
        }

        if (SignalInformation->si_code == CLD_EXITED) {
            if (SignalInformation->si_status != 99) {
                PRINT_ERROR("Error: si_status was %d instead of %d.\n",
                            SignalInformation->si_status,
                            99);

                ChildSignalFailures += 1;
            }

        } else if (SignalInformation->si_code != CLD_KILLED) {
            PRINT_ERROR("Error: unexpected si_code %x.\n",
                        SignalInformation->si_code);

            ChildSignalFailures += 1;
        }
    }

    //
    // Make sure a wait also gets the same thing.
    //

    if (ChildSignalsExpected == 1) {
        SignaledPidFound = TRUE;
        WaitPidResult = waitpid(-1, &Status, WNOHANG);
        if ((SignalInformation != NULL) &&
            (WaitPidResult != SignalInformation->si_pid)) {

            SignaledPidFound = FALSE;
            PRINT_ERROR("Error: SignalInformation->si_pid = %d but "
                        "waitpid() = %d\n.",
                        SignalInformation->si_pid,
                        WaitPidResult);

            ChildSignalFailures += 1;
        }

        ChildSignalsExpected -= 1;

    } else {
        SignaledPidFound = FALSE;
        while (ChildSignalsExpected != 0) {
            WaitPidResult = waitpid(-1, &PidStatus, WNOHANG);
            if ((SignalInformation != NULL) &&
                (WaitPidResult == SignalInformation->si_pid)) {

                Status = PidStatus;
                SignaledPidFound = TRUE;
            }

            DEBUG_PRINT("SIGCHLD handler waited and got %d.\n", WaitPidResult);
            if ((WaitPidResult == -1) || (WaitPidResult == 0)) {
                break;
            }

            ChildSignalsExpected -= 1;
        }
    }

    if (SignalInformation != NULL) {
        if (SignaledPidFound == FALSE) {
            PRINT_ERROR("Error: Pid %d signaled but waitpid could not find "
                        "it.\n",
                        SignalInformation->si_pid);

            ChildSignalFailures += 1;

        } else {
            if (SignalInformation->si_code == CLD_EXITED) {
                if ((!WIFEXITED(Status)) || (WEXITSTATUS(Status) != 99)) {
                    PRINT_ERROR("Error: Status was %x, not returning exited "
                                "or exit status %d.\n",
                                Status,
                                99);

                    ChildSignalFailures += 1;
                }

            } else if (SignalInformation->si_code == CLD_KILLED) {
                if ((!WIFSIGNALED(Status)) || (WTERMSIG(Status) != SIGKILL)) {
                    PRINT_ERROR("Error: Status was %x, not returning signaled "
                                "or SIGKILL.\n",
                                Status);

                    ChildSignalFailures += 1;
                }
            }
        }
    }

    //
    // If all the children have been accounted for, make sure there's not
    // another signal in the queue too.
    //

    if (ChildSignalsExpected == 0) {
        WaitPidResult = waitpid(-1, NULL, WNOHANG);
        if (WaitPidResult > 0) {
            PRINT_ERROR("Error: waitpid got another child %d unexpectedly.\n",
                        WaitPidResult);

            ChildSignalFailures += 1;
        }
    }

    if (SignalInformation != NULL) {
        ChildSignalPid = SignalInformation->si_pid;
    }

    return;
}

void
TestSigchldRealtime1SignalHandler (
    int Signal,
    siginfo_t *SignalInformation,
    void *Context
    )

/*++

Routine Description:

    This routine responds to the first real time signal, used to count ready
    processes.

Arguments:

    Signal - Supplies the signal number coming in, in this case always SIGCHLD.

    SignalInformation - Supplies a pointer to the signal information.

    Context - Supplies a pointer to some unused context information.

Return Value:

    None.

--*/

{

    DEBUG_PRINT("SIGRTMIN+0 %d\n", SignalInformation->si_value.sival_int);
    if (SignalInformation->si_signo != SIGRTMIN + 0) {
        PRINT_ERROR("Got si_signo %d when expected %d.\n",
                    SignalInformation->si_signo,
                    SIGRTMIN + 0);

        ChildSignalFailures += 1;
    }

    ChildProcessesReady += 1;
    return;
}

PVOID
TestThreadSpinForever (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements a thread routine that simply spins forever.

Arguments:

    Parameter - Supplies a parameter assumed to be of type PULONG whose
        contents will be set to 0.

Return Value:

    None. This thread never returns voluntarily.

--*/

{

    *((PULONG)Parameter) = 0;
    while (TRUE) {
        sleep(1);
    }

    return NULL;
}

ULONG
RunNestedSignalsTest (
    VOID
    )

/*++

Routine Description:

    This routine tests nested signal reception.

Arguments:

    None.

Return Value:

    Returns the number of failures.

--*/

{

    struct sigaction Action;
    UCHAR Byte;
    pid_t Child;
    ULONG Count;
    ULONG Failures;
    ULONG Index;
    int Pipe[2];
    int Received[2];
    union sigval SigVal;

    Count = 200;
    Child = -1;
    Failures = 0;
    PRINT("Running nested signals test\n");
    memset(&Action, 0, sizeof(Action));
    Action.sa_flags = SA_SIGINFO;
    Action.sa_sigaction = TestNestedSignalHandler;
    if (pipe(Pipe) != 0) {
        PRINT_ERROR("pipe failed.\n");
        return 1;
    }

    Child = fork();
    if (Child == -1) {
        PRINT_ERROR("fork failed.\n");
        Failures += 1;
        goto TestNestedSignalsEnd;

    //
    // This is the child.
    //

    } else if (Child == 0) {
        close(Pipe[0]);
        Pipe[0] = -1;
        if ((sigaction(SIGRTMIN, &Action, NULL) != 0) ||
            (sigaction(SIGRTMIN + 1, &Action, NULL) != 0)) {

            PRINT_ERROR("Sigaction failed.\n");
            exit(1);
        }

        SigtestWritePipe = Pipe[1];
        Byte = 1;
        write(Pipe[1], &Byte, 1);
        while ((SigtestSignalCount[0] < Count) ||
               (SigtestSignalCount[1] < Count)) {

            pause();
        }

        DEBUG_PRINT("Got %d and %d signals\n",
                    SigtestSignalCount[0],
                    SigtestSignalCount[1]);

        exit(0);

    //
    // This is the parent.
    //

    } else {
        close(Pipe[1]);
        Pipe[1] = -1;
        if ((read(Pipe[0], &Byte, 1) != 1) || (Byte != 1)) {
            PRINT_ERROR("Child not read\n");
            Failures += 1;
            goto TestNestedSignalsEnd;
        }

        Received[0] = 0;
        Received[1] = 0;
        fcntl(Pipe[0], F_SETFL, O_NONBLOCK);
        for (Index = 0; Index < Count; Index += 1) {
            if ((sigqueue(Child, SIGRTMIN, SigVal) != 0) ||
                (sigqueue(Child, SIGRTMIN + 1, SigVal) != 0)) {

                PRINT_ERROR("Failed to queue signa.\n");
                Failures += 1;
                goto TestNestedSignalsEnd;
            }

            while (read(Pipe[0], &Byte, 1) == 1) {
                if (Byte == SIGRTMIN) {
                    Received[0] += 1;

                } else if (Byte == SIGRTMIN + 1) {
                    Received[1] += 1;

                } else {
                    PRINT_ERROR("Unknown signal received\n");
                    Failures += 1;
                    goto TestNestedSignalsEnd;
                }
            }
        }

        DEBUG_PRINT("Sent %d signals\n", Index);
        fcntl(Pipe[0], F_SETFL, 0);
        while ((Received[0] != Count) ||
               (Received[1] != Count)) {

            if (read(Pipe[0], &Byte, 1) != 1) {
                perror("Error");
                PRINT_ERROR("Pipe read failure.\n");
                Failures += 1;
                goto TestNestedSignalsEnd;
            }

            if (Byte == SIGRTMIN) {
                Received[0] += 1;

            } else if (Byte == SIGRTMIN + 1) {
                Received[1] += 1;

            } else {
                PRINT_ERROR("Unknown signal received\n");
                Failures += 1;
                goto TestNestedSignalsEnd;
            }
        }
    }

    DEBUG_PRINT("\n");

TestNestedSignalsEnd:
    if (Pipe[0] >= 0) {
        close(Pipe[0]);
    }

    if (Pipe[1] >= 0) {
        close(Pipe[1]);
    }

    if (Child > 0) {
        kill(Child, SIGKILL);
        waitpid(Child, NULL, 0);
    }

    return Failures;
}

VOID
TestNestedSignalHandler (
    int Signal,
    siginfo_t *Info,
    void *Ignored
    )

/*++

Routine Description:

    This routine tests nested signal reception.

Arguments:

    Signal - Supplies the signal that occurred.

    Info - Supplies a pointer to the signal information.

    Ignored - Supplies an ignored context pointer.

Return Value:

    None.

--*/

{

    ssize_t BytesComplete;

    assert(Info->si_signo == Signal);
    assert((Signal == SIGRTMIN) || (Signal == SIGRTMIN + 1));

    do {
        BytesComplete = write(SigtestWritePipe, &(Info->si_signo), 1);

    } while ((BytesComplete < 0) && (errno == EINTR));

    if (BytesComplete != 1) {

        assert(FALSE);
    }

    if (Signal == SIGRTMIN) {
        DEBUG_PRINT("A%d ", SigtestSignalCount[0]);
        SigtestSignalCount[0] += 1;

    } else {

        assert(Signal == SIGRTMIN + 1);

        DEBUG_PRINT("B%d ", SigtestSignalCount[1]);
        SigtestSignalCount[1] += 1;
    }

    return;
}

ULONG
RunSetContextTest (
    VOID
    )

/*++

Routine Description:

    This routine tests the ucontext related functions.

Arguments:

    None.

Return Value:

    Returns the number of failures.

--*/

{

    ULONG Failures;

    Failures = TestContextSwap(TRUE);
    Failures += TestContextSwap(FALSE);
    return Failures;
}

ULONG
TestContextSwap (
    BOOL Exit
    )

/*++

Routine Description:

    This routine tests the ucontext related functions.

Arguments:

    Exit - Supplies a boolean indicating whether to test a context swap that
        exits or returns.

Return Value:

    Returns the number of failures.

--*/

{

    pid_t Child;
    ucontext_t Context1;
    ucontext_t Context2;
    ucontext_t MainContext;
    int Status;

    Child = fork();
    if (Child < 0) {
        PRINT_ERROR("Failed to fork\n");
        return 1;

    } else if (Child > 0) {
        if (waitpid(Child, &Status, 0) != Child) {
            PRINT_ERROR("Failed to wait\n");
            return 1;
        }

        if ((!WIFEXITED(Status)) || (WEXITSTATUS(Status) != 0)) {
            PRINT_ERROR("Child exited with %x\n", Status);
            return 1;
        }

        return 0;
    }

    //
    // This is the child.
    //

    SigtestContextHits = 0;
    if (getcontext(&Context1) != 0) {
        PRINT_ERROR("getcontext failed");
        exit(1);
    }

    Context1.uc_stack.ss_sp = alloca(SIGNAL_TEST_CONTEXT_STACK_SIZE);
    Context1.uc_stack.ss_size = SIGNAL_TEST_CONTEXT_STACK_SIZE;
    Context1.uc_link = &MainContext;
    makecontext(&Context1,
                TestMakecontext,
                3,
                &Context1,
                &Context2,
                5);

    if (getcontext(&Context2) != 0) {
        PRINT_ERROR("getcontext failed");
        exit(1);
    }

    Context2.uc_stack.ss_sp = alloca(SIGNAL_TEST_CONTEXT_STACK_SIZE);
    Context2.uc_stack.ss_size = SIGNAL_TEST_CONTEXT_STACK_SIZE;
    Context2.uc_link = NULL;
    if (Exit == FALSE) {
        Context2.uc_link = &Context1;
    }

    makecontext(&Context2,
                TestMakecontext,
                3,
                &Context2,
                &Context1,
                10);

    DEBUG_PRINT("MainContext swapping\n");
    SigtestContextHits += 1;
    if (swapcontext(&MainContext, &Context2) != 0) {
        PRINT_ERROR("swapcontext failed.\n");
        exit(1);
    }

    SigtestContextHits += 1;
    if (Exit != FALSE) {
        PRINT_ERROR("Main context returned instead of exited!\n");
        exit(1);
    }

    if (SigtestContextHits != ((5 * 2) + (10 * 2) + 2)) {
        PRINT_ERROR("Context hits were %d.\n", SigtestContextHits);
        exit(1);
    }

    DEBUG_PRINT("MainContext exiting\n");
    exit(0);
    return 0;
}

VOID
TestMakecontext (
    ucontext_t *OldContext,
    ucontext_t *NextContext,
    INT Identifier
    )

/*++

Routine Description:

    This routine swaps contexts.

Arguments:

    OldContext - Supplies a pointer to the previous context.

    NextContext - Supplies a pointer to the next context.

    Identifier - Supplies an identifier for this context.

Return Value:

    None.

--*/

{

    DEBUG_PRINT("Context %d: Swapping\n", Identifier);
    SigtestContextHits += Identifier;
    if (swapcontext(OldContext, NextContext) != 0) {
        PRINT_ERROR("Swapcontext failed from %d\n", Identifier);
        exit(1);
    }

    DEBUG_PRINT("Context %d: Exiting\n", Identifier);
    SigtestContextHits += Identifier;
    return;
}

