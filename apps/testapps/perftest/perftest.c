/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    perftest.c

Abstract:

    This module implements the performance benchmark application.

Author:

    Chris Stevens 27-Apr-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include "perftest.h"

//
// --------------------------------------------------------------------- Macros
//

#define PT_DEBUG_PRINT(...)                             \
    if (PtTestVerbosity >= PtTestVerbosityDebug) {      \
        printf(__VA_ARGS__);                            \
    }

#define PT_PRINT(...)                                   \
    if (PtTestVerbosity >= PtTestVerbosityNormal) {     \
        printf(__VA_ARGS__);                            \
    }

#define PT_PRINT_ERROR(...) fprintf(stderr, "\nperftest: " __VA_ARGS__)

#define PT_PRINT_RESULT(...) fprintf(PtResultFile, __VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

#define PT_VERSION_MAJOR 1
#define PT_VERSION_MINOR 0

#define PT_USAGE                                                               \
    "Usage: perf [options] \n"                                                 \
    "This utility runs performance benchmark tests. Options are:\n"            \
    "  -t, --test -- Set the test to perform. Use -l option to list the\n"     \
    "      valid test values.\n"                                               \
    "  -p, --processes <count> -- Set the number of processes to spin up.\n"   \
    "  -d, --duration <seconds> -- Set the duration, in seconds, to run each\n"\
    "      test.\n"                                                            \
    "  -r, --results <file> -- Set the file where results will be written.\n"  \
    "      The default will print to standard out.\n"                          \
    "  -l, --list -- List the set of available tests.\n"                       \
    "  -s, --summary -- Print the results in the summary format.\n"            \
    "  --verbose -- Print lots of information about what's happening.\n"       \
    "  --quiet -- Print only errors.\n"                                        \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the test version and exit.\n"                        \

#define PT_OPTION_STRING "t:p:d:r:slnvqhV"

//
// Define the default values for each argument.
//

#define PT_DEFAULT_TEST PtTestAll
#define PT_DEFAULT_PROCESS_COUNT 1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PT_TEST_VERBOSITY {
    PtTestVerbosityQuiet,
    PtTestVerbosityNormal,
    PtTestVerbosityDebug
} PT_TEST_VERBOSITY, *PPT_TEST_VERBOSITY;

typedef enum _PT_RESULT_FORMAT {
    PtResultFormatDefault,
    PtResultFormatSummary,
    PtResultFormatCount,
} PT_RESULT_FORMAT, *PPT_RESULT_FORMAT;

/*++

Structure Description:

    This structure defines information on a test process.

Members:

    Id - Stores the process ID of the process.

    PipeDescriptors - Stores the read and write file descriptors for the pipe
        to which the process will write and from which its parent will read.

    Result - Stores the test result for the process.

--*/

typedef struct _PT_PROCESS {
    pid_t Id;
    int PipeDescriptors[2];
    PT_TEST_RESULT Result;
} PT_PROCESS, *PPT_PROCESS;

//
// ----------------------------------------------- Internal Function Prototypes
//

void
PtpRunPerformanceTest (
    PPT_TEST_INFORMATION Test,
    long ProcessCount,
    int *TotalFailures
    );

void
PtpPrintTestResults (
    PPT_TEST_INFORMATION Test,
    PPT_PROCESS Processes,
    long ProcessCount
    );

void
PtpPrintTestResult (
    PPT_TEST_RESULT Result
    );

int
PtpReadResultFromChild (
    PPT_PROCESS Child
    );

int
PtpWriteResultToParent (
    PPT_PROCESS Process
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the verbosity level for debug prints.
//

PT_TEST_VERBOSITY PtTestVerbosity = PtTestVerbosityNormal;

//
// Store the array of options for the application.
//

struct option PtLongOptions[] = {
    {"test", required_argument, 0, 't'},
    {"processes", required_argument, 0, 'p'},
    {"duration", required_argument, 0, 'd'},
    {"results", required_argument, 0, 'r'},
    {"list", no_argument, 0, 'l'},
    {"summary", no_argument, 0, 's'},
    {"verbose", no_argument, 0, 'v'},
    {"quiet", no_argument, 0, 'q'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// Store the array of performance benchmark tests.
//

PT_TEST_INFORMATION PerformanceTests[PtTestTypeCount] = {
    {ALL_TEST_NAME, ALL_TEST_DESCRIPTION, NULL, PtTestAll, PtResultInvalid, 0},
    {FORK_TEST_NAME,
     FORK_TEST_DESCRIPTION,
     ForkMain,
     PtTestFork,
     PtResultIterations,
     FORK_TEST_DEFAULT_DURATION},

    {EXEC_TEST_NAME,
     EXEC_TEST_DESCRIPTION,
     ExecMain,
     PtTestExec,
     PtResultIterations,
     EXEC_TEST_DEFAULT_DURATION},

    {OPEN_TEST_NAME,
     OPEN_TEST_DESCRIPTION,
     OpenMain,
     PtTestOpen,
     PtResultIterations,
     OPEN_TEST_DEFAULT_DURATION},

    {CREATE_TEST_NAME,
     CREATE_TEST_DESCRIPTION,
     CreateMain,
     PtTestCreate,
     PtResultIterations,
     CREATE_TEST_DEFAULT_DURATION},

    {DUP_TEST_NAME,
     DUP_TEST_DESCRIPTION,
     DupMain,
     PtTestDup,
     PtResultIterations,
     DUP_TEST_DEFAULT_DURATION},

    {RENAME_TEST_NAME,
     RENAME_TEST_DESCRIPTION,
     RenameMain,
     PtTestRename,
     PtResultIterations,
     RENAME_TEST_DEFAULT_DURATION},

    {GETPPID_TEST_NAME,
     GETPPID_TEST_DESCRIPTION,
     GetppidMain,
     PtTestGetppid,
     PtResultIterations,
     GETPPID_TEST_DEFAULT_DURATION},

    {PIPE_IO_TEST_NAME,
     PIPE_IO_TEST_DESCRIPTION,
     PipeIoMain,
     PtTestPipeIo,
     PtResultIterations,
     PIPE_IO_TEST_DEFAULT_DURATION},

    {READ_TEST_NAME,
     READ_TEST_DESCRIPTION,
     ReadMain,
     PtTestRead,
     PtResultBytes,
     READ_TEST_DEFAULT_DURATION},

    {WRITE_TEST_NAME,
     WRITE_TEST_DESCRIPTION,
     WriteMain,
     PtTestWrite,
     PtResultBytes,
     WRITE_TEST_DEFAULT_DURATION},

    {COPY_TEST_NAME,
     COPY_TEST_DESCRIPTION,
     CopyMain,
     PtTestCopy,
     PtResultBytes,
     COPY_TEST_DEFAULT_DURATION},

    {DLOPEN_TEST_NAME,
     DLOPEN_TEST_DESCRIPTION,
     DlopenMain,
     PtTestDlopen,
     PtResultIterations,
     DLOPEN_TEST_DEFAULT_DURATION},

    {MMAP_PRIVATE_TEST_NAME,
     MMAP_PRIVATE_TEST_DESCRIPTION,
     MmapMain,
     PtTestMmapPrivate,
     PtResultIterations,
     MMAP_PRIVATE_TEST_DEFAULT_DURATION},

    {MMAP_SHARED_TEST_NAME,
     MMAP_SHARED_TEST_DESCRIPTION,
     MmapMain,
     PtTestMmapShared,
     PtResultIterations,
     MMAP_SHARED_TEST_DEFAULT_DURATION},

    {MMAP_ANON_TEST_NAME,
     MMAP_ANON_TEST_DESCRIPTION,
     MmapMain,
     PtTestMmapAnon,
     PtResultIterations,
     MMAP_ANON_TEST_DEFAULT_DURATION},

    {MMAP_IO_PRIVATE_TEST_NAME,
     MMAP_IO_PRIVATE_TEST_DESCRIPTION,
     MmapMain,
     PtTestMmapIoPrivate,
     PtResultIterations,
     MMAP_IO_PRIVATE_TEST_DEFAULT_DURATION},

    {MMAP_IO_SHARED_TEST_NAME,
     MMAP_IO_SHARED_TEST_DESCRIPTION,
     MmapMain,
     PtTestMmapIoShared,
     PtResultIterations,
     MMAP_IO_SHARED_TEST_DEFAULT_DURATION},

    {MMAP_IO_ANON_TEST_NAME,
     MMAP_IO_ANON_TEST_DESCRIPTION,
     MmapMain,
     PtTestMmapIoAnon,
     PtResultIterations,
     MMAP_IO_ANON_TEST_DEFAULT_DURATION},

    {MALLOC_SMALL_TEST_NAME,
     MALLOC_SMALL_TEST_DESCRIPTION,
     MallocMain,
     PtTestMallocSmall,
     PtResultIterations,
     MALLOC_SMALL_TEST_DEFAULT_DURATION},

    {MALLOC_LARGE_TEST_NAME,
     MALLOC_LARGE_TEST_DESCRIPTION,
     MallocMain,
     PtTestMallocLarge,
     PtResultIterations,
     MALLOC_LARGE_TEST_DEFAULT_DURATION},

    {MALLOC_RANDOM_TEST_NAME,
     MALLOC_RANDOM_TEST_DESCRIPTION,
     MallocMain,
     PtTestMallocRandom,
     PtResultIterations,
     MALLOC_RANDOM_TEST_DEFAULT_DURATION},

    {MALLOC_CONTENDED_TEST_NAME,
     MALLOC_CONTENDED_TEST_DESCRIPTION,
     MallocMain,
     PtTestMallocContended,
     PtResultIterations,
     MALLOC_CONTENDED_TEST_DEFAULT_DURATION},

    {PTHREAD_JOIN_TEST_NAME,
     PTHREAD_JOIN_TEST_DESCRIPTION,
     PthreadMain,
     PtTestPthreadJoin,
     PtResultIterations,
     PTHREAD_JOIN_TEST_DEFAULT_DURATION},

    {PTHREAD_DETACH_TEST_NAME,
     PTHREAD_DETACH_TEST_DESCRIPTION,
     PthreadMain,
     PtTestPthreadDetach,
     PtResultIterations,
     PTHREAD_DETACH_TEST_DEFAULT_DURATION},

    {MUTEX_TEST_NAME,
     MUTEX_TEST_DESCRIPTION,
     MutexMain,
     PtTestMutex,
     PtResultIterations,
     MUTEX_TEST_DEFAULT_DURATION},

    {MUTEX_CONTENDED_TEST_NAME,
     MUTEX_CONTENDED_TEST_DESCRIPTION,
     MutexMain,
     PtTestMutexContended,
     PtResultIterations,
     MUTEX_CONTENDED_TEST_DEFAULT_DURATION},

    {STAT_TEST_NAME,
     STAT_TEST_DESCRIPTION,
     StatMain,
     PtTestStat,
     PtResultIterations,
     STAT_TEST_DEFAULT_DURATION},

    {FSTAT_TEST_NAME,
     FSTAT_TEST_DESCRIPTION,
     FstatMain,
     PtTestFstat,
     PtResultIterations,
     FSTAT_TEST_DEFAULT_DURATION},

    {SIGNAL_IGNORED_NAME,
     SIGNAL_IGNORED_DESCRIPTION,
     SignalMain,
     PtTestSignalIgnored,
     PtResultIterations,
     SIGNAL_IGNORED_DEFAULT_DURATION},

    {SIGNAL_HANDLED_NAME,
     SIGNAL_HANDLED_DESCRIPTION,
     SignalMain,
     PtTestSignalHandled,
     PtResultIterations,
     SIGNAL_HANDLED_DEFAULT_DURATION},

    {SIGNAL_RESTART_NAME,
     SIGNAL_RESTART_DESCRIPTION,
     SignalMain,
     PtTestSignalRestart,
     PtResultIterations,
     SIGNAL_RESTART_DEFAULT_DURATION},
};

//
// Store the string values for the various test result types.
//

char *PtResultTypeStrings[PtResultTypeCount] = {
    "Invalid",
    "Iterations",
    "Bytes",
};

//
// Store the handle for the file to which the test results will be written.
//

FILE *PtResultFile;

//
// Store a pointer to the program path for individual tests to access.
//

char *PtProgramPath;

//
// Store the result format type that will be used to print the results.
//

PT_RESULT_FORMAT PtResultFormat = PtResultFormatDefault;

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

    This routine implements the performance test program.

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
    time_t Duration;
    int Failures;
    int Index;
    int Option;
    long ProcessCount;
    PT_TEST_TYPE RequestedTest;
    char *ResultFilePath;
    int Status;

    PtProgramPath = Arguments[0];

    //
    // Quickly check for the exec test loop path.
    //

    if ((ArgumentCount == EXEC_LOOP_ARGUMENT_COUNT) &&
        (strcasecmp(Arguments[1], EXEC_TEST_NAME) == 0)) {

        return ExecLoop(ArgumentCount, Arguments);
    }

    Duration = 0;
    Failures = 0;
    ProcessCount = PT_DEFAULT_PROCESS_COUNT;
    RequestedTest = PT_DEFAULT_TEST;
    ResultFilePath = NULL;
    Status = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    //
    // Process the control arguments.
    //

    while (1) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             PT_OPTION_STRING,
                             PtLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = EINVAL;
            goto MainEnd;
        }

        switch (Option) {
        case 'p':
            ProcessCount = strtol(optarg, &AfterScan, 0);
            if ((ProcessCount <= 0) || (AfterScan == optarg)) {
                PT_PRINT_ERROR("Invalid process count: %s.\n", optarg);
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 'd':
            Duration = (time_t)strtoll(optarg, &AfterScan, 0);
            if ((Duration <= 0) || (AfterScan == optarg)) {
                PT_PRINT_ERROR("Invalid number of seconds: %s.\n", optarg);
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 't':
            for (Index = 0; Index < PtTestTypeCount; Index += 1) {
                if (strcasecmp(optarg, PerformanceTests[Index].Name) == 0) {
                    RequestedTest = PerformanceTests[Index].TestType;
                    break;
                }
            }

            if (Index == PtTestTypeCount) {
                PT_PRINT_ERROR("Invalid test name: %s.\n", optarg);
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 'l':
            for (Index = 0; Index < PtTestTypeCount; Index += 1) {
                printf("%s -- %s\n",
                       PerformanceTests[Index].Name,
                       PerformanceTests[Index].Description);
            }

            return 1;

        case 's':
            PtResultFormat = PtResultFormatSummary;
            break;

        case 'r':
            ResultFilePath = optarg;
            break;

        case 'v':
            PtTestVerbosity = PtTestVerbosityDebug;
            break;

        case 'q':
            PtTestVerbosity = PtTestVerbosityQuiet;
            break;

        case 'V':
            printf("Minoca performance benchmark test version %d.%d\n",
                   PT_VERSION_MAJOR,
                   PT_VERSION_MINOR);

            return 1;

        case 'h':
            printf(PT_USAGE);
            return 1;

        default:

            assert(0);

            Status = EINVAL;
            goto MainEnd;
        }
    }

    //
    // Attempt to open the result file.
    //

    if (ResultFilePath != NULL) {
        PtResultFile = fopen(ResultFilePath, "w");
        if (PtResultFile == NULL) {
            Status = errno;
            goto MainEnd;
        }

    } else {
        PtResultFile = stdout;
    }

    //
    // Run each of the requested tests with the requested number of threads.
    //

    for (Index = 0; Index < PtTestTypeCount; Index += 1) {
        if ((PerformanceTests[Index].Routine != NULL) &&
            ((RequestedTest == PtTestAll) ||
             (RequestedTest == PerformanceTests[Index].TestType))) {

            //
            // Overwrite the default duration if one was supplied.
            //

            if (Duration != 0) {
                PerformanceTests[Index].Duration = Duration;
            }

            PtpRunPerformanceTest(&(PerformanceTests[Index]),
                                  ProcessCount,
                                  &Failures);
        }
    }

MainEnd:
    if (Status != 0) {
        PT_PRINT_ERROR("Error: %d, %s.\n", Status, strerror(Status));
    }

    if (Failures != 0) {
        PT_PRINT_ERROR("\n   *** %d failures in perftest ***\n", Failures);
    }

    if ((PtResultFile != NULL) && (PtResultFile != stdout)) {
        fclose(PtResultFile);
    }

    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

void
PtpRunPerformanceTest (
    PPT_TEST_INFORMATION Test,
    long ProcessCount,
    int *TotalFailures
    )

/*++

Routine Description:

    This routine runs the given performance test on the given number of
    processes.

Arguments:

    Test - Supplies a pointer to the performance test to execute.

    ProcessCount - Supplies the number of processes to spin up when executing
        the test.

    TotalFailures - Supplies a pointer to the total numbers encountered so far
        during all tests. This specific test adds its failure count.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    pid_t ChildId;
    int ChildIndex;
    PPT_PROCESS Children;
    int IsParent;
    PT_PROCESS ParentProcess;
    PPT_PROCESS Process;
    PPT_PROCESS Processes;
    PPT_TEST_RESULT Result;
    int Status;
    int TestFailures;

    Children = NULL;
    IsParent = 1;
    Process = NULL;
    Status = 0;
    TestFailures = 0;
    PT_PRINT("Running %s test with %ld process(es) for %lld second(s).\n",
             Test->Name,
             ProcessCount,
             (signed long long)Test->Duration);

    //
    // Fork off the desired number of processes to run the test in parallel.
    //

    if (ProcessCount > 1) {
        Processes = malloc(sizeof(PT_PROCESS) * ProcessCount);
        if (Processes == NULL) {
            Status = ENOMEM;
            TestFailures += 1;
            goto RunPerformanceTestEnd;
        }

        memset(Processes, 0, sizeof(PT_PROCESS) * ProcessCount);

        //
        // Initialize the parent process.
        //

        Processes[0].Id = getpid();
        Processes[0].Result.Status = -1;
        Result = &(Processes[0].Result);

        //
        // Create and initialize the child processes.
        //

        Children = &(Processes[1]);
        for (ChildIndex = 0; ChildIndex < ProcessCount - 1; ChildIndex += 1) {

            //
            // Create a pipe for the child to communicate with the parent. Stop
            // creating children if this fails.
            //

            Status = pipe(Children[ChildIndex].PipeDescriptors);
            if (Status != 0) {
                ProcessCount = ChildIndex + 1;
                TestFailures += 1;
                PT_PRINT_ERROR("Failed to open pipe: %s.\n", strerror(errno));
                break;
            }

            //
            // Initialize the child's test result to a failure status.
            //

            Children[ChildIndex].Result.Status = -1;

            //
            // Fork the child, closing the pipe's read channel in the child
            // and breaking out to run the test.
            //

            ChildId = fork();
            if (ChildId == 0) {
                IsParent = 0;
                Process = &(Children[ChildIndex]);
                Result = &(Process->Result);
                close(Process->PipeDescriptors[0]);
                break;
            }

            //
            // Set the child ID so the parent can wait on it later and close
            // the write pipe.
            //

            Children[ChildIndex].Id = ChildId;
            close(Children[ChildIndex].PipeDescriptors[1]);
        }

    } else {
        memset(&ParentProcess, 0, sizeof(PT_PROCESS));
        ParentProcess.Id = getpid();
        ParentProcess.Result.Status = -1;
        Processes = &ParentProcess;
        Result = &(ParentProcess.Result);
    }

    //
    // Run the actual test and write the results to the parent if this is a
    // child process.
    //

    Test->Routine(Test, Result);

    //
    // Collect the results
    //

    if (IsParent != 0) {

        //
        // Wait for all the children.
        //

        for (ChildIndex = 0; ChildIndex < ProcessCount - 1; ChildIndex += 1) {
            ChildId = waitpid(Children[ChildIndex].Id, &Status, 0);

            assert((ChildId == -1 ) || (ChildId == Children[ChildIndex].Id));

            if (ChildId == -1) {
                PT_PRINT_ERROR("Failed to wait for child %d: %s.\n",
                               Children[ChildIndex].Id,
                               strerror(errno));

                Status = -1;

            } else if (!WIFEXITED(Status)) {
                PT_PRINT_ERROR("ChildId %d returned with status %x\n",
                               ChildId,
                               Status);

            } else if (WEXITSTATUS(Status) != 0) {
                PT_PRINT_ERROR("ChildId %d exited with error %s\n",
                               ChildId,
                               strerror(WEXITSTATUS(Status)));

            } else {
                Status = PtpReadResultFromChild(&(Children[ChildIndex]));
            }

            if (Status != 0) {
                TestFailures += 1;
            }

            close(Children[ChildIndex].PipeDescriptors[0]);
        }

        //
        // Check the status of the current parent process.
        //

        if (Result->Status != 0) {
            TestFailures += 1;
        }

        PtpPrintTestResults(Test, Processes, ProcessCount);

    //
    // If this is a child, write the results to the parent and report any
    // errors encountered while writing.
    //

    } else {
        Status = PtpWriteResultToParent(Process);
        if (Status != 0) {
            Status = errno;
        }

        close(Process->PipeDescriptors[1]);
        exit(Status);
    }

RunPerformanceTestEnd:
    if ((Processes != NULL) && (Processes != &ParentProcess)) {
        free(Processes);
    }

    PT_PRINT("Completed %s test with %d failure(s).\n",
             Test->Name,
             TestFailures);

    *TotalFailures += TestFailures;
    return;
}

void
PtpPrintTestResults (
    PPT_TEST_INFORMATION Test,
    PPT_PROCESS Processes,
    long ProcessCount
    )

/*++

Routine Description:

    This routine prints the test results to the results file.

Arguments:

    Test - Supplies a pointer to the test whose results are to be printed.

    Processes - Supplies a pointer to an array of processes that ran the test.

    ProcessCount - Supplies the number of processes that ran the test.

Return Value:

    None.

--*/

{

    double Average;
    double AverageDurationMicroseconds;
    double Frequency;
    int Index;
    unsigned long long Microseconds;
    PPT_PROCESS Process;
    PT_TEST_RESULT TotalResult;
    long ValidProcessCount;

    assert(ProcessCount > 0);

    //
    // Print the results based on the desired output format.
    //

    switch (PtResultFormat) {

    //
    // The summary prints one line for the test. This format is what the Minoca
    // build system expects during test automation. It includes the test name,
    // the type of the result (e.g. integer, decimal, string, duration, etc.),
    // and the raw value in string format.
    //

    case PtResultFormatSummary:

        //
        // Collect the average value and print the result based on the type.
        //

        ValidProcessCount = 0;
        memset(&TotalResult, 0, sizeof(PT_TEST_RESULT));
        for (Index = 0; Index < ProcessCount; Index += 1) {
            Process = &(Processes[Index]);

            assert(Process->Result.Type == Test->ResultType);

            if (Process->Result.Status != 0) {
                PT_PRINT_ERROR("%s test: failed: %s\n",
                               Test->Name,
                               strerror(Process->Result.Status));

                continue;
            }

            switch (Test->ResultType) {
            case PtResultIterations:
                TotalResult.Data.Iterations += Process->Result.Data.Iterations;
                break;

            case PtResultBytes:
                TotalResult.Data.Bytes += Process->Result.Data.Iterations;
                break;

            default:

                assert(0);

                return;
            }

            ValidProcessCount += 1;
        }

        //
        // If not all of the processes succeeded, don't count the result. The
        // summary is only valid if all of the processes succeed.
        //

        if (ValidProcessCount != ProcessCount) {
            PT_PRINT_ERROR("%s test: %ld out of %ld processes failed.\n",
                           Test->Name,
                           ProcessCount - ValidProcessCount,
                           ProcessCount);

            break;
        }

        //
        // If all of the process had a valid result, report the summary as the
        // average result value over the duration of the tests.
        //

        switch (Test->ResultType) {
        case PtResultIterations:
            Average = (double)TotalResult.Data.Iterations / ProcessCount;
            break;

        case PtResultBytes:
            Average = (double)TotalResult.Data.Bytes / ProcessCount;
            break;

        default:

            assert(0);

            return;
        }

        assert(Test->Duration > 0);

        Frequency = (double)Average / (double)(Test->Duration);
        PT_PRINT_RESULT("%s (%ldp):decimal:%.3f\n",
                        Test->Name,
                        ProcessCount,
                        Frequency);

        //
        // Not every test reports resource usage data. But, knowning that all
        // processes succeeded, if the first process reports it, then all
        // processes should have successfully reported it.
        //

        if (Processes[0].Result.ResourceUsageValid == 0) {
            break;
        }

        //
        // Collect the total resource usage in order to take an average.
        //

        for (Index = 0; Index < ProcessCount; Index += 1) {
            Process = &(Processes[Index]);

            assert(Process->Result.ResourceUsageValid != 0);
            assert(Process->Result.Status == 0);

            timeradd(&(TotalResult.ResourceUsage.RealTime),
                     &(Process->Result.ResourceUsage.RealTime),
                     &(TotalResult.ResourceUsage.RealTime));

            timeradd(&(TotalResult.ResourceUsage.UserTime),
                     &(Process->Result.ResourceUsage.UserTime),
                     &(TotalResult.ResourceUsage.UserTime));

            timeradd(&(TotalResult.ResourceUsage.SystemTime),
                     &(Process->Result.ResourceUsage.SystemTime),
                     &(TotalResult.ResourceUsage.SystemTime));
        }

        assert((TotalResult.ResourceUsage.RealTime.tv_sec >= 0) &&
               (TotalResult.ResourceUsage.RealTime.tv_usec >= 0));

        assert((TotalResult.ResourceUsage.UserTime.tv_sec >= 0) &&
               (TotalResult.ResourceUsage.UserTime.tv_usec >= 0));

        assert((TotalResult.ResourceUsage.SystemTime.tv_sec >= 0) &&
               (TotalResult.ResourceUsage.SystemTime.tv_usec >= 0));

        //
        // Report the average user and system time as a percentage of the
        // average real time. The real time is taken from the results and not
        // the test duration as there may have been some time between test
        // completion and usage collection.
        //

        Microseconds = (TotalResult.ResourceUsage.RealTime.tv_sec * 1000000) +
                       TotalResult.ResourceUsage.RealTime.tv_usec;

        AverageDurationMicroseconds = (double)Microseconds /ProcessCount;
        Microseconds = (TotalResult.ResourceUsage.UserTime.tv_sec * 1000000) +
                       TotalResult.ResourceUsage.UserTime.tv_usec;

        Average = (double)Microseconds / ProcessCount;
        Average /= AverageDurationMicroseconds;
        Average *= 100;
        PT_PRINT_RESULT("%s (%ldp) User Time %%:decimal:%.02f\n",
                        Test->Name,
                        ProcessCount,
                        Average);

        Microseconds = (TotalResult.ResourceUsage.SystemTime.tv_sec * 1000000) +
                       TotalResult.ResourceUsage.SystemTime.tv_usec;

        Average = (double)Microseconds / ProcessCount;
        Average /= AverageDurationMicroseconds;
        Average *= 100;
        PT_PRINT_RESULT("%s (%ldp) Kernel Time %%:decimal:%.02f\n",
                        Test->Name,
                        ProcessCount,
                        Average);

        break;

    //
    // The default result format prints detailed information for each process.
    //

    case PtResultFormatDefault:
    default:

        //
        // Mark the start of the test in the results file.
        //

        PT_PRINT_RESULT("Test Name: %s\n"
                        "Process Count: %ld\n"
                        "Seconds: %lld\n"
                        "Result Type: %s\n"
                        "Results:\n",
                        Test->Name,
                        ProcessCount,
                        (signed long long)Test->Duration,
                        PtResultTypeStrings[Test->ResultType]);

        //
        // Print all the processes' results.
        //

        for (Index = 0; Index < ProcessCount; Index += 1) {
            PtpPrintTestResult(&(Processes[Index].Result));
        }

        PT_PRINT_RESULT("\n");
        break;
    }

    return;
}

void
PtpPrintTestResult (
    PPT_TEST_RESULT Result
    )

/*++

Routine Description:

    This routine prints the given result to the result file.

Arguments:

    Result - Supplies a pointer to the result to be printed.

Return Value:

    None.

--*/

{

    struct timeval *RealTime;
    struct timeval *SystemTime;
    struct timeval *UserTime;

    if (Result->Status != 0) {
        PT_PRINT_RESULT("error: %s\n", strerror(Result->Status));
        return;
    }

    switch (Result->Type) {
    case PtResultIterations:
        PT_PRINT_RESULT("%llu", Result->Data.Iterations);
        break;

    case PtResultBytes:
        PT_PRINT_RESULT("%llu", Result->Data.Bytes);
        break;

    default:

        assert(0);

        PT_PRINT_RESULT("Invalid result type %d.\n", Result->Type);
        return;
    }

    if (Result->ResourceUsageValid != 0) {
        RealTime = &(Result->ResourceUsage.RealTime);
        UserTime = &(Result->ResourceUsage.UserTime);
        SystemTime = &(Result->ResourceUsage.SystemTime);
        PT_PRINT_RESULT(" - real %lld.%06ld, user %lld.%06ld, sys %lld.%06ld\n",
                        (signed long long)RealTime->tv_sec,
                        RealTime->tv_usec,
                        (signed long long)UserTime->tv_sec,
                        UserTime->tv_usec,
                        (signed long long)SystemTime->tv_sec,
                        SystemTime->tv_usec);

    } else {
        PT_PRINT_RESULT("\n");
    }

    return;
}

int
PtpReadResultFromChild (
    PPT_PROCESS Child
    )

/*++

Routine Description:

    This routine reads a test result from a child, populating the process's
    result member.

Arguments:

    Child - Supplies a pointer to the child process from which to read.

Return Value:

    0 on success.

    -1 on failure and errno will be set.

--*/

{

    ssize_t BytesRead;

    do {
        BytesRead = read(Child->PipeDescriptors[0],
                         &(Child->Result),
                         sizeof(PT_TEST_RESULT));

    } while ((BytesRead < 0) && (errno == EINTR));

    if (BytesRead < 0) {
        return -1;
    }

    if (BytesRead != sizeof(PT_TEST_RESULT)) {
        errno = EIO;
        return -1;
    }

    return 0;
}

int
PtpWriteResultToParent (
    PPT_PROCESS Process
    )

/*++

Routine Description:

    This routine writes the process's test result to the parent process.

Arguments:

    Process - Supplies a pointer to the child process whose result is being
        written.

Return Value:

    0 on success.

    -1 on failure and errno will be set.

--*/

{

    ssize_t BytesWritten;

    do {
        BytesWritten = write(Process->PipeDescriptors[1],
                             &(Process->Result),
                             sizeof(PT_TEST_RESULT));

    } while ((BytesWritten < 0) && (errno == EINTR));

    if (BytesWritten < 0) {
        return -1;
    }

    if (BytesWritten != sizeof(PT_TEST_RESULT)) {
        errno = EIO;
        return -1;
    }

    return 0;
}

