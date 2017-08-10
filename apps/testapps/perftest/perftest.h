/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    perftest.h

Abstract:

    This header contains definitions for the performance benchmark tests.

Author:

    Chris Stevens 27-Apr-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <sys/time.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Performance test names and descriptions.
//

#define ALL_TEST_NAME "all"
#define ALL_TEST_DESCRIPTION "Runs all of the performance tests in sequence."
#define FORK_TEST_NAME "fork"
#define FORK_TEST_DESCRIPTION "Benchmarks the fork() C library routine."
#define EXEC_TEST_NAME "exec"
#define EXEC_TEST_DESCRIPTION "Benchmarks the exec() C library routine."
#define OPEN_TEST_NAME "open"
#define OPEN_TEST_DESCRIPTION \
    "Benchmarks the open() and close() C library routines."

#define CREATE_TEST_NAME "create"
#define CREATE_TEST_DESCRIPTION \
    "Benchmarks the create() and remove() C library routines."

#define DUP_TEST_NAME "dup"
#define DUP_TEST_DESCRIPTION "Benchmarks the dup() C library routine."
#define RENAME_TEST_NAME "rename"
#define RENAME_TEST_DESCRIPTION "Benchmakrs the rename() C library routine."
#define GETPPID_TEST_NAME "getppid"
#define GETPPID_TEST_DESCRIPTION "Benchmarks the getppid() C library routine."
#define PIPE_IO_TEST_NAME "pipe_io"
#define PIPE_IO_TEST_DESCRIPTION "Benchmarks pipe I/O throughput."
#define READ_TEST_NAME "read"
#define READ_TEST_DESCRIPTION "Benchmarks read() throughput."
#define WRITE_TEST_NAME "write"
#define WRITE_TEST_DESCRIPTION "Benchmarks write() throughput."
#define COPY_TEST_NAME "copy"
#define COPY_TEST_DESCRIPTION \
    "Benchmarks read()/write() throughput copying data between files."

#define DLOPEN_TEST_NAME "dlopen"
#define DLOPEN_TEST_DESCRIPTION \
    "Benchmarks the dlopen() and dlclose() C library routines."

#define MMAP_PRIVATE_TEST_NAME "mmap_private"
#define MMAP_PRIVATE_TEST_DESCRIPTION \
    "Benchmarks the mmap() and munmap() C library routines with MAP_PRIVATE."

#define MMAP_SHARED_TEST_NAME "mmap_shared"
#define MMAP_SHARED_TEST_DESCRIPTION \
    "Benchmarks the mmap() and munmap() C library routines with MAP_SHARED."

#define MMAP_ANON_TEST_NAME "mmap_anon"
#define MMAP_ANON_TEST_DESCRIPTION \
    "Benchmarks the mmap() and munmap() C library routines with MAP_ANON."

#define MMAP_IO_PRIVATE_TEST_NAME "mmap_io_private"
#define MMAP_IO_PRIVATE_TEST_DESCRIPTION \
    "Benchmarks the I/O throughput on private memory mapped regions."

#define MMAP_IO_SHARED_TEST_NAME "mmap_io_shared"
#define MMAP_IO_SHARED_TEST_DESCRIPTION \
    "Benchmarks the I/O throughput on shared memory mapped regions."

#define MMAP_IO_ANON_TEST_NAME "mmap_io_anon"
#define MMAP_IO_ANON_TEST_DESCRIPTION \
    "Benchmarks the I/O throughput on anonymous memory mapped regions."

#define MALLOC_SMALL_TEST_NAME "malloc_small"
#define MALLOC_SMALL_TEST_DESCRIPTION \
    "Benchmarks malloc() and free() using a small allocation size."

#define MALLOC_LARGE_TEST_NAME "malloc_large"
#define MALLOC_LARGE_TEST_DESCRIPTION \
    "Benchmarks malloc() and free() using a large allocation size."

#define MALLOC_RANDOM_TEST_NAME "malloc_random"
#define MALLOC_RANDOM_TEST_DESCRIPTION \
    "Benchmarks malloc() and free() using a random allocation size."

#define MALLOC_CONTENDED_TEST_NAME "malloc_contended"
#define MALLOC_CONTENDED_TEST_DESCRIPTION \
    "Benchmarks malloc() and free() with multiple threads."

#define PTHREAD_JOIN_TEST_NAME "pthread_join"
#define PTHREAD_JOIN_TEST_DESCRIPTION \
    "Benchmarks thread creation with pthread_join()."

#define PTHREAD_DETACH_TEST_NAME "pthread_detach"
#define PTHREAD_DETACH_TEST_DESCRIPTION \
    "Benchmarks thread creation with pthread_detach()."

#define MUTEX_TEST_NAME "mutex"
#define MUTEX_TEST_DESCRIPTION \
    "Benchmarks pthread mutex lock and unlock routines."

#define MUTEX_CONTENDED_TEST_NAME "mutex_contended"
#define MUTEX_CONTENDED_TEST_DESCRIPTION \
    "Benchmarks pthread mutex lock and unlock routines under contention."

#define STAT_TEST_NAME "stat"
#define STAT_TEST_DESCRIPTION \
    "Benchmarks the stat() C library routine."

#define FSTAT_TEST_NAME "fstat"
#define FSTAT_TEST_DESCRIPTION \
    "Benchmarks the fstat() C library routine."

#define SIGNAL_IGNORED_NAME "sigign"
#define SIGNAL_IGNORED_DESCRIPTION \
    "Benchmarks how many ignored signals can be raised."

#define SIGNAL_HANDLED_NAME "sighand"
#define SIGNAL_HANDLED_DESCRIPTION \
    "Benchmarks how many handled signals can be raised."

#define SIGNAL_RESTART_NAME "sarestart"
#define SIGNAL_RESTART_DESCRIPTION \
    "Benchmarks how many system call restarts can be made."

//
// Default test durations, in seconds.
//

#define FORK_TEST_DEFAULT_DURATION 60
#define EXEC_TEST_DEFAULT_DURATION 60
#define OPEN_TEST_DEFAULT_DURATION 30
#define CREATE_TEST_DEFAULT_DURATION 30
#define DUP_TEST_DEFAULT_DURATION 30
#define RENAME_TEST_DEFAULT_DURATION 30
#define GETPPID_TEST_DEFAULT_DURATION 10
#define PIPE_IO_TEST_DEFAULT_DURATION 30
#define READ_TEST_DEFAULT_DURATION 60
#define WRITE_TEST_DEFAULT_DURATION 60
#define COPY_TEST_DEFAULT_DURATION 60
#define DLOPEN_TEST_DEFAULT_DURATION 30
#define MMAP_PRIVATE_TEST_DEFAULT_DURATION 30
#define MMAP_SHARED_TEST_DEFAULT_DURATION 30
#define MMAP_ANON_TEST_DEFAULT_DURATION 30
#define MMAP_IO_PRIVATE_TEST_DEFAULT_DURATION 30
#define MMAP_IO_SHARED_TEST_DEFAULT_DURATION 30
#define MMAP_IO_ANON_TEST_DEFAULT_DURATION 30
#define MALLOC_SMALL_TEST_DEFAULT_DURATION 30
#define MALLOC_LARGE_TEST_DEFAULT_DURATION 30
#define MALLOC_RANDOM_TEST_DEFAULT_DURATION 30
#define MALLOC_CONTENDED_TEST_DEFAULT_DURATION 30
#define PTHREAD_JOIN_TEST_DEFAULT_DURATION 30
#define PTHREAD_DETACH_TEST_DEFAULT_DURATION 30
#define MUTEX_TEST_DEFAULT_DURATION 30
#define MUTEX_CONTENDED_TEST_DEFAULT_DURATION 30
#define STAT_TEST_DEFAULT_DURATION 30
#define FSTAT_TEST_DEFAULT_DURATION 30
#define SIGNAL_IGNORED_DEFAULT_DURATION 30
#define SIGNAL_HANDLED_DEFAULT_DURATION 30
#define SIGNAL_RESTART_DEFAULT_DURATION 30

//
// Define the number of variables supplied to an iteration of the execute test
// loop.
//

#define EXEC_LOOP_ARGUMENT_COUNT 5

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PT_TEST_TYPE {
    PtTestAll,
    PtTestFork,
    PtTestExec,
    PtTestOpen,
    PtTestCreate,
    PtTestDup,
    PtTestRename,
    PtTestGetppid,
    PtTestPipeIo,
    PtTestRead,
    PtTestWrite,
    PtTestCopy,
    PtTestDlopen,
    PtTestMmapPrivate,
    PtTestMmapShared,
    PtTestMmapAnon,
    PtTestMmapIoPrivate,
    PtTestMmapIoShared,
    PtTestMmapIoAnon,
    PtTestMallocSmall,
    PtTestMallocLarge,
    PtTestMallocRandom,
    PtTestMallocContended,
    PtTestPthreadJoin,
    PtTestPthreadDetach,
    PtTestMutex,
    PtTestMutexContended,
    PtTestStat,
    PtTestFstat,
    PtTestSignalIgnored,
    PtTestSignalHandled,
    PtTestSignalRestart,
    PtTestTypeCount
} PT_TEST_TYPE, *PPT_TEST_TYPE;

/*++

Enumeration Description:

    This enumeration describes the type of results a performance benchmark test
    can return.

Values:

    PtResultInvalid - Indicates an invalid test result type.

    PtResulIterations - Indicates that the results are stored as the raw number
        of test iterations executed over the duration of the test.

    PtResultsBytes - Indicates that the results are stored as the number of
        bytes processed over the duration of the test.

    PtResultTypeCount - Indicates the number of different result types.

--*/

typedef enum _PT_RESULT_TYPE {
    PtResultInvalid,
    PtResultIterations,
    PtResultBytes,
    PtResultTypeCount
} PT_RESULT_TYPE, *PPT_RESULT_TYPE;

/*++

Structure Description:

    This structure defines a performance test resource usage.

Members:

    RealTime - Stores the actual wall clock consumed by the test.

    UserTime - Stores the user time used by the test.

    SystemTime - Stores the system time used by the test.

--*/

typedef struct _PT_TEST_RESOURCE_USAGE {
    struct timeval RealTime;
    struct timeval UserTime;
    struct timeval SystemTime;
} PT_TEST_RESOURCE_USAGE, *PPT_TEST_RESOURCE_USAGE;

/*++

Structure Description:

    This structure defines a performance test result.

Members:

    Status - Stores 0 on success or a non-zero error value.

    ResourceUsageValid - Stores 0 if the resource usage data is invalid and 1
        if it is valid.

    ResourceUsage - Stores resource usage information for the test.

    Type - Stores the type of result returned by the test.

    Iterations - Stores the number of iterations the test made.

    Bytes - Stores the number of bytes the test processed.

--*/

typedef struct _PT_TEST_RESULT {
    int Status;
    int ResourceUsageValid;
    PT_TEST_RESOURCE_USAGE ResourceUsage;
    PT_RESULT_TYPE Type;
    union {
        unsigned long long Iterations;
        unsigned long long Bytes;
    } Data;

} PT_TEST_RESULT, *PPT_TEST_RESULT;

typedef struct _PT_TEST_INFORMATION PT_TEST_INFORMATION, *PPT_TEST_INFORMATION;

typedef
void
(*PPT_TEST_ROUTINE) (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine prototype represents a function that gets called to perform
    a performance test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines an individual performance benchmark test.

Members:

    Name - Stores a pointer to a string containing the name of the test.

    Description - Stores a pointer to a null terminated string containing a
        short one-liner describing the test.

    Type - Stores the type value for the test.

    Routine - Stores a pointer to the routine to execute for the test.

    Duration - Stores the duration of the test, in seconds.

    ThreadCount - Stores the number of threads to spin up for tests that
        benchmark contention.

--*/

struct _PT_TEST_INFORMATION {
    char *Name;
    char *Description;
    PPT_TEST_ROUTINE Routine;
    PT_TEST_TYPE TestType;
    PT_RESULT_TYPE ResultType;
    time_t Duration;
    long ThreadCount;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Store a point to the program name for the individual tests to access.
//

extern char *PtProgramPath;

//
// -------------------------------------------------------- Function Prototypes
//

//
// Performance test library routines.
//

int
PtStartTimedTest (
    time_t Duration
    );

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

int
PtFinishTimedTest (
    PPT_TEST_RESULT Result
    );

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

int
PtIsTimedTestRunning (
    void
    );

/*++

Routine Description:

    This routine determines whether or not a timed test is running.

Arguments:

    None.

Return Value:

    1 if a test is currently running, or 0 otherwise.

--*/

int
PtCollectResourceUsageStart (
    void
    );

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

int
PtCollectResourceUsageStop (
    PPT_TEST_RESULT Result
    );

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

//
// Individual test routines.
//

void
ForkMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the fork performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
ExecMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the exec performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

int
ExecLoop (
    int ArgumentCount,
    char **Arguments
    );

/*++

Routine Description:

    This routine implements an interation of the exec test.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

void
OpenMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the open performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
CreateMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the create performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
DupMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the dup performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
RenameMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

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

void
GetppidMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the getppid performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
PipeIoMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

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

void
ReadMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the read performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
WriteMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the write performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
CopyMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the file copy performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
DlopenMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the dynamic library open performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
MmapMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the memory map performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
MallocMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the malloc performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
PthreadMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the pthread performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
MutexMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the mutex performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
StatMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the stat performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
FstatMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the fstat performance benchmark tests.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

void
SignalMain (
    PPT_TEST_INFORMATION Test,
    PPT_TEST_RESULT Result
    );

/*++

Routine Description:

    This routine performs the signal performance benchmark test.

Arguments:

    Test - Supplies a pointer to the performance test being executed.

    Result - Supplies a pointer to a performance test result structure that
        receives the tests results.

Return Value:

    None.

--*/

