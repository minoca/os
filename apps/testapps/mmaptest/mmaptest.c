/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mmaptest.c

Abstract:

    This module implements the tests used to verify that memory map operations
    are working.

Author:

    Chris Stevens 10-Mar-2014

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>

//
// --------------------------------------------------------------------- Macros
//

#define DEBUG_PRINT(...)                                \
    if (MemoryMapTestVerbosity >= TestVerbosityDebug) {      \
        printf(__VA_ARGS__);                            \
    }

#define PRINT(...)                                      \
    if (MemoryMapTestVerbosity >= TestVerbosityNormal) {     \
        printf(__VA_ARGS__);                            \
    }

#define PRINT_ERROR(...) fprintf(stderr, "\nmmaptest: " __VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

#define MEMORY_MAP_TEST_VERSION_MAJOR 1
#define MEMORY_MAP_TEST_VERSION_MINOR 0

#define MEMORY_MAP_TEST_USAGE                                                  \
    "Usage: mmaptest [options] \n"                                             \
    "This utility test memory map functionality. Options are:\n"               \
    "  -c, --file-count <count> -- Set the number of files to create.\n"       \
    "  -s, --file-size <size> -- Set the size of each file in bytes.\n"        \
    "  -i, --iterations <count> -- Set the number of operations to perform.\n" \
    "  -p, --threads <count> -- Set the number of threads to spin up.\n"       \
    "  -t, --test -- Set the test to perform. Valid values are all, \n"        \
    "      basic, private, shared, shmprivate, and shmshared.\n"               \
    "  --debug -- Print lots of information about what's happening.\n"         \
    "  --quiet -- Print only errors.\n"                                        \
    "  --no-cleanup -- Leave test files around for debugging.\n"               \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the test version and exit.\n"                        \

#define MEMORY_MAP_TEST_OPTIONS_STRING "c:s:i:t:p:ndqhV"

#define MEMORY_MAP_TEST_CREATE_PERMISSIONS (S_IRUSR | S_IWUSR)

#define DEFAULT_FILE_COUNT 20
#define DEFAULT_FILE_SIZE (1024 * 17)
#define DEFAULT_OPERATION_COUNT (DEFAULT_FILE_COUNT * 50)
#define DEFAULT_THREAD_COUNT 1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MEMORY_MAP_TEST_ACTION {
    MemoryMapTestActionMap,
    MemoryMapTestActionUnmap,
    MemoryMapTestActionMappedWrite,
    MemoryMapTestActionMappedRead,
    MemoryMapTestActionSync,
    MemoryMapTestActionFileWrite,
    MemoryMapTestActionFileRead,
    MemoryMapTestActionCount
} MEMORY_MAP_TEST_ACTION, *PMEMORY_MAP_TEST_ACTION;

typedef enum _TEST_VERBOSITY {
    TestVerbosityQuiet,
    TestVerbosityNormal,
    TestVerbosityDebug
} TEST_VERBOSITY, *PTEST_VERBOSITY;

typedef enum _MEMORY_MAP_TEST_TYPE {
    MemoryMapTestAll,
    MemoryMapTestBasic,
    MemoryMapTestPrivate,
    MemoryMapTestShared,
    MemoryMapTestShmPrivate,
    MemoryMapTestShmShared
} MEMORY_MAP_TEST_TYPE, *PMEMORY_MAP_TEST_TYPE;

typedef
ULONG
(*PMEMORY_MAP_BASIC_TEST_ROUTINE) (
    INT FileSize
    );

/*++

Routine Description:

    This routine prototype represents a function that gets called to perform
    a basic memory map test.

Arguments:

    FileSize - Supplies the size of the file to create, if needed.

Return Value:

    Returns the number of failures encountered during the test.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
MemoryMapTestCreateFiles (
    MEMORY_MAP_TEST_TYPE Test,
    INT FileCount
    );

VOID
MemoryMapTestDestroyFiles (
    MEMORY_MAP_TEST_TYPE Test,
    INT FileCount
    );

ULONG
RunMemoryMapBasicTests (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

ULONG
RunMemoryMapPrivateTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

ULONG
RunMemoryMapSharedTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

ULONG
RunMemoryMapShmPrivateTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

ULONG
RunMemoryMapShmSharedTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

static
VOID
MemoryMapTestExpectedSignalHandler (
    INT SignalNumber,
    siginfo_t *SignalInformation,
    PVOID Context
    );

static
VOID
MemoryMapTestUnexpectedSignalHandler (
    INT SignalNumber,
    siginfo_t *SignalInformation,
    PVOID Context
    );

ULONG
PrintTestTime (
    struct timeval *StartTime
    );

ULONG
MemoryMapEmptyTest (
    INT FileSize
    );

ULONG
MemoryMapTruncateTest (
    INT FileSize
    );

ULONG
MemoryMapReadOnlyTest (
    INT FileSize
    );

ULONG
MemoryMapNoAccessTest (
    INT FileSize
    );

ULONG
MemoryMapAnonymousTest (
    INT FileSize
    );

ULONG
MemoryMapSharedAnonymousTest (
    INT FileSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Higher levels here print out more stuff.
//

TEST_VERBOSITY MemoryMapTestVerbosity = TestVerbosityNormal;

//
// Set this boolean to skip cleaning up files.
//

BOOL MemoryMapTestNoCleanup = FALSE;

struct option MemoryMapTestLongOptions[] = {
    {"file-count", required_argument, 0, 'c'},
    {"file-size", required_argument, 0, 's'},
    {"iterations", required_argument, 0, 'i'},
    {"threads", required_argument, 0, 'p'},
    {"test", required_argument, 0, 't'},
    {"no-cleanup", no_argument, 0, 'n'},
    {"debug", no_argument, 0, 'd'},
    {"quiet", no_argument, 0, 'q'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// Define an array of basic memory map tests.
//

PMEMORY_MAP_BASIC_TEST_ROUTINE MemoryMapBasicTests[] = {
    MemoryMapEmptyTest,
    MemoryMapTruncateTest,
    MemoryMapReadOnlyTest,
    MemoryMapNoAccessTest,
    MemoryMapAnonymousTest,
    MemoryMapSharedAnonymousTest
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

    This routine implements the memory map test program.

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
    pid_t *Children;
    BOOL DestroyFiles;
    INT Failures;
    INT FileCount;
    INT FileSize;
    BOOL IsParent;
    INT Iterations;
    INT Option;
    INT Status;
    MEMORY_MAP_TEST_TYPE Test;
    INT Threads;

    Children = NULL;
    DestroyFiles = FALSE;
    Failures = 0;
    FileCount = DEFAULT_FILE_COUNT;
    FileSize = DEFAULT_FILE_SIZE;
    Iterations = DEFAULT_OPERATION_COUNT;
    Test = MemoryMapTestAll;
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
                             MEMORY_MAP_TEST_OPTIONS_STRING,
                             MemoryMapTestLongOptions,
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
            FileCount = strtol(optarg, &AfterScan, 0);
            if ((FileCount <= 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid file count %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 's':
            FileSize = strtol(optarg, &AfterScan, 0);
            if ((FileSize < 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid file size %s.\n", optarg);
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

        case 'n':
            MemoryMapTestNoCleanup = TRUE;
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
                Test = MemoryMapTestAll;

            } else if (strcasecmp(optarg, "basic") == 0) {
                Test = MemoryMapTestBasic;

            } else if (strcasecmp(optarg, "private") == 0) {
                Test = MemoryMapTestPrivate;

            } else if (strcasecmp(optarg, "shared") == 0) {
                Test = MemoryMapTestShared;

            } else if (strcasecmp(optarg, "shmprivate") == 0) {
                Test = MemoryMapTestShmPrivate;

            } else if (strcasecmp(optarg, "shmshared") == 0) {
                Test = MemoryMapTestShmShared;

            } else {
                PRINT_ERROR("Invalid test: %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'd':
            MemoryMapTestVerbosity = TestVerbosityDebug;
            break;

        case 'q':
            MemoryMapTestVerbosity = TestVerbosityQuiet;
            break;

        case 'V':
            printf("Minoca mmaptest version %d.%d\n",
                   MEMORY_MAP_TEST_VERSION_MAJOR,
                   MEMORY_MAP_TEST_VERSION_MINOR);

            return 1;

        case 'h':
            printf(MEMORY_MAP_TEST_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // Create any files that are necessary for the test.
    //

    DestroyFiles = TRUE;
    Failures += MemoryMapTestCreateFiles(Test, FileCount);
    if (Failures != 0) {
        goto MainEnd;
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

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestBasic)) {
        Failures += RunMemoryMapBasicTests(FileCount, FileSize, Iterations);
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestPrivate)) {
        Failures += RunMemoryMapPrivateTest(FileCount, FileSize, Iterations);
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShared)) {
        Failures += RunMemoryMapSharedTest(FileCount, FileSize, Iterations);
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShmPrivate)) {
        Failures += RunMemoryMapShmPrivateTest(FileCount, FileSize, Iterations);
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShmShared)) {
        Failures += RunMemoryMapShmSharedTest(FileCount, FileSize, Iterations);
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

    //
    // Destroy any files that were needed for the test, if necessary.
    //

    if ((MemoryMapTestNoCleanup == FALSE) && (DestroyFiles != FALSE)) {
        MemoryMapTestDestroyFiles(Test, FileCount);
    }

    if (Children != NULL) {
        free(Children);
    }

    if (Status != 0) {
        PRINT_ERROR("Error: %d.\n", Status);
    }

    if (Failures != 0) {
        PRINT_ERROR("\n   *** %d failures in mmaptest ***\n", Failures);
        return Failures;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions

ULONG
MemoryMapTestCreateFiles (
    MEMORY_MAP_TEST_TYPE Test,
    INT FileCount
    )

/*++

Routine Description:

    This routine creates any files that are necessary for the given test.

Arguments:

    Test - Supplies the type of test being run.

    FileCount - Supplies the number of files to be used during the test.

Return Value:

    Returns the number of failures encountered when creating files.

--*/

{

    ULONG Failures;
    INT File;
    INT FileIndex;
    CHAR FileName[16];
    INT OpenFlags;

    Failures = 0;
    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestPrivate)) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "mmpt-%06x",
                     FileIndex);

            OpenFlags = O_RDWR | O_CREAT;
            File = open(FileName,
                        OpenFlags,
                        MEMORY_MAP_TEST_CREATE_PERMISSIONS);

            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                goto MemoryMapTestCreateFilesEnd;
            }

            if (close(File) != 0) {
                Failures += 1;
                goto MemoryMapTestCreateFilesEnd;
            }
        }
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShared)) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "mmst-%06x",
                     FileIndex);

            OpenFlags = O_RDWR | O_CREAT;
            File = open(FileName,
                        OpenFlags,
                        MEMORY_MAP_TEST_CREATE_PERMISSIONS);

            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                goto MemoryMapTestCreateFilesEnd;
            }

            if (close(File) != 0) {
                Failures += 1;
                goto MemoryMapTestCreateFilesEnd;
            }
        }
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShmPrivate)) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "shmpt-%06x",
                     FileIndex);

            OpenFlags = O_RDWR | O_CREAT;
            File = shm_open(FileName,
                            OpenFlags,
                            MEMORY_MAP_TEST_CREATE_PERMISSIONS);

            if (File < 0) {
                PRINT_ERROR("Failed to open shared memory object %s "
                            "(flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                goto MemoryMapTestCreateFilesEnd;
            }

            if (close(File) != 0) {
                Failures += 1;
                goto MemoryMapTestCreateFilesEnd;
            }
        }
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShmShared)) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "shmst-%06x",
                     FileIndex);

            OpenFlags = O_RDWR | O_CREAT;
            File = shm_open(FileName,
                            OpenFlags,
                            MEMORY_MAP_TEST_CREATE_PERMISSIONS);

            if (File < 0) {
                PRINT_ERROR("Failed to open shared memory object %s "
                            "(flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                goto MemoryMapTestCreateFilesEnd;
            }

            if (close(File) != 0) {
                Failures += 1;
                goto MemoryMapTestCreateFilesEnd;
            }
        }
    }

MemoryMapTestCreateFilesEnd:
    return Failures;
}

VOID
MemoryMapTestDestroyFiles (
    MEMORY_MAP_TEST_TYPE Test,
    INT FileCount
    )

/*++

Routine Description:

    This routine destroys any files that were necessary for the given test.

Arguments:

    Test - Supplies the type of test being run.

    FileCount - Supplies the number of files used during the test.

Return Value:

    None.

--*/

{

    INT FileIndex;
    CHAR FileName[16];
    INT Result;

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestPrivate)) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "mmpt-%06x",
                     FileIndex);

            Result = unlink(FileName);
            if ((Result != 0) && (errno != ENOENT)) {
                PRINT_ERROR("Failed to unlink %s: %s.\n",
                            FileName,
                            strerror(errno));
            }
        }
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShared)) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "mmst-%06x",
                     FileIndex);

            Result = unlink(FileName);
            if ((Result != 0) && (errno != ENOENT)) {
                PRINT_ERROR("Failed to unlink %s: %s.\n",
                            FileName,
                            strerror(errno));
            }
        }
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShmPrivate)) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "shmpt-%06x",
                     FileIndex);

            Result = shm_unlink(FileName);
            if ((Result != 0) && (errno != ENOENT)) {
                PRINT_ERROR("Failed to unlink %s: %s.\n",
                            FileName,
                            strerror(errno));
            }
        }
    }

    if ((Test == MemoryMapTestAll) || (Test == MemoryMapTestShmShared)) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "shmst-%06x",
                     FileIndex);

            Result = shm_unlink(FileName);
            if ((Result != 0) && (errno != ENOENT)) {
                PRINT_ERROR("Failed to unlink %s: %s.\n",
                            FileName,
                            strerror(errno));
            }
        }
    }

    return;
}

ULONG
RunMemoryMapBasicTests (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the memory map basic test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    ULONG Failures;
    INT Iteration;
    INT Percent;
    pid_t Process;
    INT Test;
    INT TestCount;
    PMEMORY_MAP_BASIC_TEST_ROUTINE TestRoutine;

    Failures = 0;

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running memory map basic tests with %d iterations.\n",
          Process,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    //
    // Determine how many basic tests there are.
    //

    TestCount = sizeof(MemoryMapBasicTests) / sizeof(MemoryMapBasicTests[0]);

    //
    // For each iteration, pick a random test and execute it.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {
        Test = rand() % TestCount;
        TestRoutine = MemoryMapBasicTests[Test];
        Failures += TestRoutine(FileSize);
        if ((Iteration % Percent) == 0) {
            PRINT("b");
        }
    }

    PRINT("\n");
    return Failures;
}

ULONG
RunMemoryMapPrivateTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the memory map private consistency test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    MEMORY_MAP_TEST_ACTION Action;
    unsigned ActionSeed;
    size_t BytesComplete;
    INT ExpectedValue;
    ULONG Failures;
    INT File;
    PINT FileBuffer;
    INT FileIndex;
    CHAR FileName[16];
    PINT FileOffset;
    INT FillIndex;
    INT Iteration;
    PINT *MapBuffer;
    INT MaxSimultaneousFiles;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    struct sigaction SignalAction;
    INT SimultaneousFiles;
    struct timeval StartTime;
    size_t TotalBytesComplete;

    Failures = 0;
    FileBuffer = NULL;
    FileOffset = NULL;
    MapBuffer = NULL;

    //
    // Set up the signal handler.
    //

    SignalAction.sa_sigaction = MemoryMapTestUnexpectedSignalHandler;
    sigemptyset(&(SignalAction.sa_mask));
    SignalAction.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &SignalAction, NULL);
    sigaction(SIGBUS, &SignalAction, NULL);

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunMemoryMapPrivateTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running memory map private test with %d files of %d "
          "bytes each. %d iterations.\n",
          Process,
          FileCount,
          FileSize,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    MaxSimultaneousFiles = 0;
    SimultaneousFiles = 0;
    FileBuffer = malloc(FileSize);
    if (FileBuffer == NULL) {
        Failures += 1;
        goto RunMemoryMapPrivateTestEnd;
    }

    FileOffset = malloc(FileCount * sizeof(INT));
    if (FileOffset == NULL) {
        Failures += 1;
        goto RunMemoryMapPrivateTestEnd;
    }

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        FileOffset[FileIndex] = -1;
    }

    MapBuffer = malloc(FileCount * sizeof(PINT));
    if (MapBuffer == NULL) {
        Failures += 1;
        goto RunMemoryMapPrivateTestEnd;
    }

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        MapBuffer[FileIndex] = MAP_FAILED;
    }

    FileSize = ALIGN_RANGE_UP(FileSize, sizeof(INT));

    //
    // Get a separate seed for the random actions.
    //

    ActionSeed = time(NULL);

    //
    // Perform the memory map operations. This test writes an entire private
    // memory mapped file with incremental values and then tests that any file
    // reads return the same values.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % FileCount;
        snprintf(FileName,
                 sizeof(FileName),
                 "mmpt-%06x",
                 FileIndex);

        Action = rand_r(&ActionSeed) % MemoryMapTestActionCount;

        //
        // If the file has yet to be created, then the action must be map.
        //

        if (MapBuffer[FileIndex] == MAP_FAILED) {
            Action = MemoryMapTestActionMap;

        //
        // Otherwise if the file is set to be mapped again, go unmap it first.
        //

        } else if (Action == MemoryMapTestActionMap) {
            Action = MemoryMapTestActionUnmap;
        }

        switch (Action) {
        case MemoryMapTestActionMap:

            assert(MapBuffer[FileIndex] == MAP_FAILED);

            SimultaneousFiles += 1;
            if (SimultaneousFiles > MaxSimultaneousFiles) {
                MaxSimultaneousFiles = SimultaneousFiles;
            }

            //
            // Open the file read/write.
            //

            OpenFlags = O_RDWR;
            File = open(FileName, OpenFlags);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            //
            // Use ftruncate to make the file bigger so that access to the
            // mmapped region succeeds.
            //

            Result = ftruncate(File, FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to ftruncate file %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }

            //
            // Mmap the file privately with read/write permissions.
            //

            MapBuffer[FileIndex] = mmap(0,
                                        FileSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE,
                                        File,
                                        0);

            //
            // Close the file.
            //

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            //
            // Increment the failure count if the mapping failed.
            //

            if (MapBuffer[FileIndex] == MAP_FAILED) {
                PRINT_ERROR("Failed to map file %s rw + private: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MemoryMapTestActionUnmap:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            //
            // Unmap the file.
            //

            Result = munmap(MapBuffer[FileIndex], FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to unmap %s.\n", strerror(errno));
                Failures += 1;
                continue;
            }

            MapBuffer[FileIndex] = MAP_FAILED;
            FileOffset[FileIndex] = -1;
            SimultaneousFiles -= 1;
            break;

        case MemoryMapTestActionMappedWrite:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            FileOffset[FileIndex] = rand();
            DEBUG_PRINT("Writing file %s, Value %x.\n",
                        FileName,
                        FileOffset[FileIndex]);

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                MapBuffer[FileIndex][FillIndex] = FileOffset[FileIndex] +
                                                  FillIndex;
            }

            break;

        case MemoryMapTestActionMappedRead:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            DEBUG_PRINT("Reading file %s, Value should be %x.\n",
                        FileName,
                        FileOffset[FileIndex]);

            //
            // If the mapped section has been written to, then the buffer
            // should always read the incrementing random values. Otherwise the
            // file should be all zero's.
            //

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                if (FileOffset[FileIndex] == -1) {
                    ExpectedValue = 0;

                } else {
                    ExpectedValue = FileOffset[FileIndex] + FillIndex;
                }

                if (MapBuffer[FileIndex][FillIndex] != ExpectedValue) {
                    PRINT_ERROR("Mapped read file %s index %x came back %x, "
                                "should have been %x.\n",
                                FileName,
                                FillIndex,
                                MapBuffer[FileIndex][FillIndex],
                                ExpectedValue);

                    Failures += 1;
                }
            }

            break;

        case MemoryMapTestActionSync:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            DEBUG_PRINT("Syncing file %s, Value should be %x.\n",
                        FileName,
                        FileOffset[FileIndex]);

            Result = msync(MapBuffer[FileIndex], FileSize, MS_SYNC);
            if (Result != 0) {
                PRINT_ERROR("Failed to msync file %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MemoryMapTestActionFileWrite:

            //
            // Open the file and write to it using the file I/O APIs.
            //

            OpenFlags = O_WRONLY;
            File = open(FileName, OpenFlags);
            if (File < 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                                FileName,
                                OpenFlags,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            //
            // Fill the file buffer with 0's and write it out to the file. The
            // memory mappings are marked private, so none of their prior
            // writes should get erased by this action.
            //

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = 0;
            }

            DEBUG_PRINT("Writing to file %s.\n", FileName);
            do {
                BytesComplete = write(File, FileBuffer, FileSize);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != FileSize) {
                PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                            BytesComplete,
                            FileSize,
                            strerror(errno));

                Failures += 1;
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            break;

        case MemoryMapTestActionFileRead:

            //
            // Open the file and read from it using the file I/O APIs.
            //

            OpenFlags = O_RDONLY;
            File = open(FileName, OpenFlags);
            if (File < 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                                FileName,
                                OpenFlags,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            DEBUG_PRINT("Reading from file %s.\n", FileName);

            //
            // Fill the file buffer with junk and read it in.
            //

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = 0xFEEDF00D;
            }

            TotalBytesComplete = 0;
            while (TotalBytesComplete < FileSize) {
                do {
                    BytesComplete = read(File,
                                         FileBuffer + TotalBytesComplete,
                                         FileSize - TotalBytesComplete);

                } while ((BytesComplete < 0) && (errno == EINTR));

                if (BytesComplete <= 0) {
                    PRINT_ERROR("Read failed. Read %d (%d total) of "
                                "%d bytes: %s.\n",
                                BytesComplete,
                                TotalBytesComplete,
                                FileSize,
                                strerror(errno));

                    Failures += 1;
                    break;
                }

                TotalBytesComplete += BytesComplete;
            }

            //
            // The result should either be -2 or 0. The mappings were done
            // privately, so any mapped writes should not have had an effect.
            //

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                if (FileBuffer[FillIndex] != 0) {
                    PRINT_ERROR("Read data file %s index %x came back %x, "
                                "should have been 0.\n",
                                FileName,
                                FillIndex,
                                FileBuffer[FillIndex]);

                    Failures += 1;
                }
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("p");
        }
    }

    //
    //  Unmap all the files that remain mapped.
    //

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        if (MapBuffer[FileIndex] != MAP_FAILED) {
            Result = munmap(MapBuffer[FileIndex], FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to unmap %p: %s.\n",
                            MapBuffer[FileIndex],
                            strerror(errno));

                Failures += 1;
            }
        }
    }

    PRINT("\nMax usage: %d files, %I64d bytes.\n",
          MaxSimultaneousFiles,
          (ULONGLONG)MaxSimultaneousFiles * (ULONGLONG)FileSize);

    Failures += PrintTestTime(&StartTime);

RunMemoryMapPrivateTestEnd:
    if (FileBuffer != NULL) {
        free(FileBuffer);
    }

    if (FileOffset != NULL) {
        free(FileOffset);
    }

    if (MapBuffer != NULL) {
        free(MapBuffer);
    }

    return Failures;
}

ULONG
RunMemoryMapSharedTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the memory map shared consistency test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    MEMORY_MAP_TEST_ACTION Action;
    unsigned ActionSeed;
    ssize_t BytesComplete;
    ULONG Failures;
    INT File;
    INT FileIndex;
    CHAR FileName[16];
    INT Iteration;
    PBYTE *MapBuffer;
    INT MaxSimultaneousFiles;
    INT Offset;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    struct sigaction SignalAction;
    INT SimultaneousFiles;
    struct timeval StartTime;
    BYTE Value;

    Failures = 0;
    MapBuffer = NULL;

    //
    // Set up the signal handler.
    //

    SignalAction.sa_sigaction = MemoryMapTestUnexpectedSignalHandler;
    sigemptyset(&(SignalAction.sa_mask));
    SignalAction.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &SignalAction, NULL);
    sigaction(SIGBUS, &SignalAction, NULL);

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunMemoryMapSharedTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running memory map shared test with %d files of %d "
          "bytes each. %d iterations.\n",
          Process,
          FileCount,
          FileSize,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    MaxSimultaneousFiles = 0;
    SimultaneousFiles = 0;
    MapBuffer = malloc(FileCount * sizeof(PINT));
    if (MapBuffer == NULL) {
        Failures += 1;
        goto RunMemoryMapSharedTestEnd;
    }

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        MapBuffer[FileIndex] = MAP_FAILED;
    }

    FileSize = ALIGN_RANGE_UP(FileSize, sizeof(INT));

    //
    // Get a separate seed for the random actions.
    //

    ActionSeed = time(NULL);

    //
    // Perform the memory map operations. This test picks a random offset and
    // writes the low byte of the offset to the offet's location in a file.
    // Memory mapped regions of the file that are mapped shared are then tested
    // at random offsets to determine if the correct value, or 0, is stored
    // there.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % FileCount;
        snprintf(FileName,
                 sizeof(FileName),
                 "mmst-%06x",
                 FileIndex);

        Action = rand_r(&ActionSeed) % MemoryMapTestActionCount;

        //
        // If the file has yet to be created, then the action must be map.
        //

        if (MapBuffer[FileIndex] == MAP_FAILED) {
            Action = MemoryMapTestActionMap;

        //
        // Otherwise if the file is set to be mapped again, go unmap it first.
        //

        } else if (Action == MemoryMapTestActionMap) {
            Action = MemoryMapTestActionUnmap;
        }

        switch (Action) {
        case MemoryMapTestActionMap:

            assert(MapBuffer[FileIndex] == MAP_FAILED);

            SimultaneousFiles += 1;
            if (SimultaneousFiles > MaxSimultaneousFiles) {
                MaxSimultaneousFiles = SimultaneousFiles;
            }

            //
            // Open the file read/write.
            //

            OpenFlags = O_RDWR;
            File = open(FileName, OpenFlags);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            //
            // Use ftruncate to make the file bigger so that access to the
            // mmapped region succeeds.
            //

            Result = ftruncate(File, FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to ftruncate file %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }

            //
            // Mmap the file privately with read/write permissions.
            //

            MapBuffer[FileIndex] = mmap(0,
                                        FileSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        File,
                                        0);

            //
            // Close the file.
            //

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            //
            // Increment the failure count if the mapping failed.
            //

            if (MapBuffer[FileIndex] == MAP_FAILED) {
                PRINT_ERROR("Failed to map file %s rw + private: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MemoryMapTestActionUnmap:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            //
            // Unmap the file.
            //

            Result = munmap(MapBuffer[FileIndex], FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to unmap %s.\n", strerror(errno));
                Failures += 1;
                continue;
            }

            MapBuffer[FileIndex] = MAP_FAILED;
            SimultaneousFiles -= 1;
            break;

        case MemoryMapTestActionMappedWrite:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            Offset = rand() % FileSize;
            DEBUG_PRINT("Writing file %s at offset %x.\n", FileName, Offset);
            MapBuffer[FileIndex][Offset] = (BYTE)Offset;
            break;

        case MemoryMapTestActionMappedRead:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            Offset = rand() % FileSize;
            DEBUG_PRINT("Reading file %s at offset %x.\n", FileName, Offset);
            Value = MapBuffer[FileIndex][Offset];
            if ((Value != 0) && (Value != (BYTE)Offset)) {
                PRINT_ERROR("Read data file %s at offset %x came back %x, "
                            "should have been %x or 0.\n",
                            FileName,
                            Offset,
                            Value,
                            (BYTE)Offset);

                Failures += 1;
            }

            break;

        case MemoryMapTestActionSync:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            DEBUG_PRINT("Syncing file %s.\n", FileName);
            Result = msync(MapBuffer[FileIndex], FileSize, MS_SYNC);
            if (Result != 0) {
                PRINT_ERROR("Failed to msync file %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MemoryMapTestActionFileWrite:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            //
            // Open the file and write to it using the file I/O APIs.
            //

            OpenFlags = O_WRONLY;
            File = open(FileName, OpenFlags);
            if (File < 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                                FileName,
                                OpenFlags,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            Offset = rand() % FileSize;
            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on file %s offset %d failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
            }

            DEBUG_PRINT("Writing to file %s at offset %x.\n", FileName, Offset);
            do {
                BytesComplete = write(File, &Offset, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != 1) {
                PRINT_ERROR("Write failed. Wrote %d of 1 byte: %s.\n",
                            BytesComplete,
                            strerror(errno));

                Failures += 1;
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            //
            // If the write succeeded and the mapped buffer at this offset
            // does not match the low byte of the offset, then something is
            // wrong.
            //

            if ((BytesComplete == 1) &&
                (MapBuffer[FileIndex][Offset] != (BYTE)Offset)) {

                PRINT_ERROR("Wrote to %s at offset %x with value %x, but "
                            "mapped buffer read %x.\n",
                            FileName,
                            Offset,
                            (BYTE)Offset,
                            MapBuffer[FileIndex][Offset]);

                Failures += 1;
            }

            break;

        case MemoryMapTestActionFileRead:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            //
            // Open the file and read from it using the file I/O APIs.
            //

            OpenFlags = O_RDONLY;
            File = open(FileName, OpenFlags);
            if (File < 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                                FileName,
                                OpenFlags,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            Offset = rand() % FileSize;
            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on file %s offset %d failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
            }

            DEBUG_PRINT("Reading from file %s at offset %x.\n",
                        FileName,
                        Offset);

            Value = 0;
            do {
                BytesComplete = read(File, &Value, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            if (BytesComplete < 0) {
                PRINT_ERROR("Read failed. Read %d of 1 bytes: %s.\n",
                            BytesComplete,
                            strerror(errno));

                Failures += 1;
                break;
            }

            //
            // The value should be 0 or the low byte of the offset.
            //

            if ((BytesComplete == 1) &&
                ((Value != 0) && (Value != (BYTE)Offset))) {

                PRINT_ERROR("Read file %s at offset %x. Read value %x but "
                            "expected 0 or %x.\n",
                            FileName,
                            Offset,
                            Value,
                            (BYTE)Offset);

                Failures += 1;
            }

            //
            // The value should match what the mapped buffer sees unless the
            // value is 0 and the mapped buffer is now equal to the low by
            // of the offset.
            //

            if ((BytesComplete == 1) &&
                (Value != MapBuffer[FileIndex][Offset]) &&
                ((Value != 0) ||
                 (MapBuffer[FileIndex][Offset] != (BYTE)Offset))) {

                PRINT_ERROR("Read file %s at offset %x. Read value %x but "
                            "expected %x.\n",
                            FileName,
                            Offset,
                            Value,
                            MapBuffer[FileIndex][Offset]);

                Failures += 1;
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("s");
        }
    }

    //
    //  Unmap all the files that remain mapped.
    //

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        if (MapBuffer[FileIndex] != MAP_FAILED) {
            Result = munmap(MapBuffer[FileIndex], FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to unmap %p: %s.\n",
                            MapBuffer[FileIndex],
                            strerror(errno));

                Failures += 1;
            }
        }
    }

    PRINT("\nMax usage: %d files, %I64d bytes.\n",
          MaxSimultaneousFiles,
          (ULONGLONG)MaxSimultaneousFiles * (ULONGLONG)FileSize);

    Failures += PrintTestTime(&StartTime);

RunMemoryMapSharedTestEnd:
    if (MapBuffer != NULL) {
        free(MapBuffer);
    }

    return Failures;
}

ULONG
RunMemoryMapShmPrivateTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the memory map shared memory object private test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    MEMORY_MAP_TEST_ACTION Action;
    unsigned ActionSeed;
    size_t BytesComplete;
    INT ExpectedValue;
    ULONG Failures;
    INT File;
    PINT FileBuffer;
    INT FileIndex;
    CHAR FileName[16];
    PINT FileOffset;
    INT FillIndex;
    INT Iteration;
    PINT *MapBuffer;
    INT MaxSimultaneousObjects;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    struct sigaction SignalAction;
    INT SimultaneousObjects;
    struct timeval StartTime;
    size_t TotalBytesComplete;

    Failures = 0;
    FileBuffer = NULL;
    FileOffset = NULL;
    MapBuffer = NULL;

    //
    // Set up the signal handler.
    //

    SignalAction.sa_sigaction = MemoryMapTestUnexpectedSignalHandler;
    sigemptyset(&(SignalAction.sa_mask));
    SignalAction.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &SignalAction, NULL);
    sigaction(SIGBUS, &SignalAction, NULL);

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunMemoryMapShmPrivateTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running shared memory object private test with %d files "
          "of %d bytes each. %d iterations.\n",
          Process,
          FileCount,
          FileSize,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    MaxSimultaneousObjects = 0;
    SimultaneousObjects = 0;
    FileBuffer = malloc(FileSize);
    if (FileBuffer == NULL) {
        Failures += 1;
        goto RunMemoryMapShmPrivateTestEnd;
    }

    FileOffset = malloc(FileCount * sizeof(INT));
    if (FileOffset == NULL) {
        Failures += 1;
        goto RunMemoryMapShmPrivateTestEnd;
    }

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        FileOffset[FileIndex] = -1;
    }

    MapBuffer = malloc(FileCount * sizeof(PINT));
    if (MapBuffer == NULL) {
        Failures += 1;
        goto RunMemoryMapShmPrivateTestEnd;
    }

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        MapBuffer[FileIndex] = MAP_FAILED;
    }

    FileSize = ALIGN_RANGE_UP(FileSize, sizeof(INT));

    //
    // Get a separate seed for the random actions.
    //

    ActionSeed = time(NULL);

    //
    // Perform the memory map operations. This test writes an entire shared
    // memory object with incremental values and then tests that any file
    // reads return the same values.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random shared memory object and a random action.
        //

        FileIndex = rand() % FileCount;
        snprintf(FileName,
                 sizeof(FileName),
                 "shmpt-%06x",
                 FileIndex);

        Action = rand_r(&ActionSeed) % MemoryMapTestActionCount;

        //
        // If the file has yet to be created, then the action must be map.
        //

        if (MapBuffer[FileIndex] == MAP_FAILED) {
            Action = MemoryMapTestActionMap;

        //
        // Otherwise if the file is set to be mapped again, go unmap it first.
        //

        } else if (Action == MemoryMapTestActionMap) {
            Action = MemoryMapTestActionUnmap;
        }

        switch (Action) {
        case MemoryMapTestActionMap:

            assert(MapBuffer[FileIndex] == MAP_FAILED);

            SimultaneousObjects += 1;
            if (SimultaneousObjects > MaxSimultaneousObjects) {
                MaxSimultaneousObjects = SimultaneousObjects;
            }

            //
            // Open the shared memory object read/write.
            //

            OpenFlags = O_RDWR;
            File = shm_open(FileName, OpenFlags, 0);
            if (File < 0) {
                PRINT_ERROR("Failed to open shm object %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            //
            // Use ftruncate to make the shared memory object bigger so that
            // access to the mmapped region succeeds.
            //

            Result = ftruncate(File, FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to ftruncate shm object %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }

            //
            // Mmap the shared memory object privately with read/write
            // permissions.
            //

            MapBuffer[FileIndex] = mmap(0,
                                        FileSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_PRIVATE,
                                        File,
                                        0);

            //
            // Close the file.
            //

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            //
            // Increment the failure count if the mapping failed.
            //

            if (MapBuffer[FileIndex] == MAP_FAILED) {
                PRINT_ERROR("Failed to map shm object %s rw + private: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MemoryMapTestActionUnmap:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            //
            // Unmap the file.
            //

            Result = munmap(MapBuffer[FileIndex], FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to unmap %s.\n", strerror(errno));
                Failures += 1;
                continue;
            }

            MapBuffer[FileIndex] = MAP_FAILED;
            FileOffset[FileIndex] = -1;
            SimultaneousObjects -= 1;
            break;

        case MemoryMapTestActionMappedWrite:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            FileOffset[FileIndex] = rand();
            DEBUG_PRINT("Writing shm object %s, Value %x.\n",
                        FileName,
                        FileOffset[FileIndex]);

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                MapBuffer[FileIndex][FillIndex] = FileOffset[FileIndex] +
                                                  FillIndex;
            }

            break;

        case MemoryMapTestActionMappedRead:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            DEBUG_PRINT("Reading shm object %s, Value should be %x.\n",
                        FileName,
                        FileOffset[FileIndex]);

            //
            // If the mapped section has been written to, then the buffer
            // should always read the incrementing random values. Otherwise the
            // file should be all zero's.
            //

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                if (FileOffset[FileIndex] == -1) {
                    ExpectedValue = 0;

                } else {
                    ExpectedValue = FileOffset[FileIndex] + FillIndex;
                }

                if (MapBuffer[FileIndex][FillIndex] != ExpectedValue) {
                    PRINT_ERROR("Mapped read shm object %s index %x came back"
                                "%x, should have been %x.\n",
                                FileName,
                                FillIndex,
                                MapBuffer[FileIndex][FillIndex],
                                ExpectedValue);

                    Failures += 1;
                }
            }

            break;

        case MemoryMapTestActionSync:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            DEBUG_PRINT("Syncing shm object %s, Value should be %x.\n",
                        FileName,
                        FileOffset[FileIndex]);

            Result = msync(MapBuffer[FileIndex], FileSize, MS_SYNC);
            if (Result != 0) {
                PRINT_ERROR("Failed to msync shm object %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MemoryMapTestActionFileWrite:

            //
            // Open the shared memory object and write to it using the file
            // I/O APIs.
            //

            OpenFlags = O_RDWR;
            File = shm_open(FileName, OpenFlags, 0);
            if (File < 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to open shm object %s (flags %x): "
                                "%s.\n",
                                FileName,
                                OpenFlags,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            //
            // Fill the file buffer with 0's and write it out to the file. The
            // memory mappings are marked private, so none of their prior
            // writes should get erased by this action.
            //

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = 0;
            }

            DEBUG_PRINT("Writing to shm object %s.\n", FileName);
            do {
                BytesComplete = write(File, FileBuffer, FileSize);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != FileSize) {
                PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                            BytesComplete,
                            FileSize,
                            strerror(errno));

                Failures += 1;
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            break;

        case MemoryMapTestActionFileRead:

            //
            // Open the shared memory object and read from it using the file
            // I/O APIs.
            //

            OpenFlags = O_RDONLY;
            File = shm_open(FileName, OpenFlags, 0);
            if (File < 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to open shm object %s (flags %x): "
                                "%s.\n",
                                FileName,
                                OpenFlags,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            DEBUG_PRINT("Reading from shm object %s.\n", FileName);

            //
            // Fill the file buffer with junk and read it in.
            //

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = 0xFEEDF00D;
            }

            TotalBytesComplete = 0;
            while (TotalBytesComplete < FileSize) {
                do {
                    BytesComplete = read(File,
                                         FileBuffer + TotalBytesComplete,
                                         FileSize - TotalBytesComplete);

                } while ((BytesComplete < 0) && (errno == EINTR));

                if (BytesComplete <= 0) {
                    PRINT_ERROR("Read failed. Read %d (%d total) of "
                                "%d bytes: %s.\n",
                                BytesComplete,
                                TotalBytesComplete,
                                FileSize,
                                strerror(errno));

                    Failures += 1;
                    break;
                }

                TotalBytesComplete += BytesComplete;
            }

            //
            // The result should either be -2 or 0. The mappings were done
            // privately, so any mapped writes should not have had an effect.
            //

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                if (FileBuffer[FillIndex] != 0) {
                    PRINT_ERROR("Read data shm object %s index %x came back "
                                "%x, should have been 0.\n",
                                FileName,
                                FillIndex,
                                FileBuffer[FillIndex]);

                    Failures += 1;
                }
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("p");
        }
    }

    //
    //  Unmap all the files that remain mapped.
    //

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        if (MapBuffer[FileIndex] != MAP_FAILED) {
            Result = munmap(MapBuffer[FileIndex], FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to unmap %p: %s.\n",
                            MapBuffer[FileIndex],
                            strerror(errno));

                Failures += 1;
            }
        }
    }

    PRINT("\nMax usage: %d shm objects, %I64d bytes.\n",
          MaxSimultaneousObjects,
          (ULONGLONG)MaxSimultaneousObjects * (ULONGLONG)FileSize);

    Failures += PrintTestTime(&StartTime);

RunMemoryMapShmPrivateTestEnd:
    if (FileBuffer != NULL) {
        free(FileBuffer);
    }

    if (FileOffset != NULL) {
        free(FileOffset);
    }

    if (MapBuffer != NULL) {
        free(MapBuffer);
    }

    return Failures;
}

ULONG
RunMemoryMapShmSharedTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the memory map shared consistency test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    MEMORY_MAP_TEST_ACTION Action;
    unsigned ActionSeed;
    size_t BytesComplete;
    ULONG Failures;
    INT File;
    INT FileIndex;
    CHAR FileName[16];
    INT Iteration;
    PBYTE *MapBuffer;
    INT MaxSimultaneousObjects;
    INT Offset;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    struct sigaction SignalAction;
    INT SimultaneousObjects;
    struct timeval StartTime;
    BYTE Value;

    Failures = 0;
    MapBuffer = NULL;

    //
    // Set up the signal handler.
    //

    SignalAction.sa_sigaction = MemoryMapTestUnexpectedSignalHandler;
    sigemptyset(&(SignalAction.sa_mask));
    SignalAction.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &SignalAction, NULL);
    sigaction(SIGBUS, &SignalAction, NULL);

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunMemoryMapShmSharedTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running shared memroy object shared test with %d files "
          "of %d bytes each. %d iterations.\n",
          Process,
          FileCount,
          FileSize,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    MaxSimultaneousObjects = 0;
    SimultaneousObjects = 0;
    MapBuffer = malloc(FileCount * sizeof(PINT));
    if (MapBuffer == NULL) {
        Failures += 1;
        goto RunMemoryMapShmSharedTestEnd;
    }

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        MapBuffer[FileIndex] = MAP_FAILED;
    }

    FileSize = ALIGN_RANGE_UP(FileSize, sizeof(INT));

    //
    // Get a separate seed for the random actions.
    //

    ActionSeed = time(NULL);

    //
    // Perform the memory map operations. This test picks a random offset and
    // writes the low byte of the offset to the offet's location in a shared
    // memory object. Memory mapped regions of the object that are mapped
    // shared are then tested at random offsets to determine if the correct
    // value, or 0, is stored there.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % FileCount;
        snprintf(FileName,
                 sizeof(FileName),
                 "shmst-%06x",
                 FileIndex);

        Action = rand_r(&ActionSeed) % MemoryMapTestActionCount;

        //
        // If the file has yet to be created, then the action must be map.
        //

        if (MapBuffer[FileIndex] == MAP_FAILED) {
            Action = MemoryMapTestActionMap;

        //
        // Otherwise if the file is set to be mapped again, go unmap it first.
        //

        } else if (Action == MemoryMapTestActionMap) {
            Action = MemoryMapTestActionUnmap;
        }

        switch (Action) {
        case MemoryMapTestActionMap:

            assert(MapBuffer[FileIndex] == MAP_FAILED);

            SimultaneousObjects += 1;
            if (SimultaneousObjects > MaxSimultaneousObjects) {
                MaxSimultaneousObjects = SimultaneousObjects;
            }

            //
            // Open the shared memory object read/write.
            //

            OpenFlags = O_RDWR;
            File = shm_open(FileName, OpenFlags, 0);
            if (File < 0) {
                PRINT_ERROR("Failed to open shm object %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            //
            // Use ftruncate to make the shared memory object bigger so that
            // access to the mapped region succeeds.
            //

            Result = ftruncate(File, FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to ftruncate shm object %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }

            //
            // Mmap the file privately with read/write permissions.
            //

            MapBuffer[FileIndex] = mmap(0,
                                        FileSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        File,
                                        0);

            //
            // Close the file.
            //

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            //
            // Increment the failure count if the mapping failed.
            //

            if (MapBuffer[FileIndex] == MAP_FAILED) {
                PRINT_ERROR("Failed to map shm object %s rw + private: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MemoryMapTestActionUnmap:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            //
            // Unmap the shared memory object.
            //

            Result = munmap(MapBuffer[FileIndex], FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to unmap %s.\n", strerror(errno));
                Failures += 1;
                continue;
            }

            MapBuffer[FileIndex] = MAP_FAILED;
            SimultaneousObjects -= 1;
            break;

        case MemoryMapTestActionMappedWrite:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            Offset = rand() % FileSize;
            DEBUG_PRINT("Writing shm object %s at offset %x.\n",
                        FileName,
                        Offset);

            MapBuffer[FileIndex][Offset] = (BYTE)Offset;
            break;

        case MemoryMapTestActionMappedRead:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            Offset = rand() % FileSize;
            DEBUG_PRINT("Reading shm object %s at offset %x.\n",
                        FileName,
                        Offset);

            Value = MapBuffer[FileIndex][Offset];
            if ((Value != 0) && (Value != (BYTE)Offset)) {
                PRINT_ERROR("Read data shm object %s at offset %x came back "
                            "%x, should have been %x or 0.\n",
                            FileName,
                            Offset,
                            Value,
                            (BYTE)Offset);

                Failures += 1;
            }

            break;

        case MemoryMapTestActionSync:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            DEBUG_PRINT("Syncing shm object %s.\n", FileName);
            Result = msync(MapBuffer[FileIndex], FileSize, MS_SYNC);
            if (Result != 0) {
                PRINT_ERROR("Failed to msync shm object %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MemoryMapTestActionFileWrite:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            //
            // Open the file and write to it using the file I/O APIs.
            //

            OpenFlags = O_RDWR;
            File = shm_open(FileName, OpenFlags, 0);
            if (File < 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to open shm object %s (flags %x): "
                                "%s.\n",
                                FileName,
                                OpenFlags,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            Offset = rand() % FileSize;
            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on shm object %s offset %d failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
                break;
            }

            DEBUG_PRINT("Writing to shm object %s at offset %x.\n",
                        FileName,
                        Offset);

            do {
                BytesComplete = write(File, &Offset, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != 1) {
                PRINT_ERROR("Write failed. Wrote %d of 1 byte: %s.\n",
                            BytesComplete,
                            strerror(errno));

                Failures += 1;
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
                break;
            }

            //
            // If the write succeeded and the mapped buffer at this offset
            // does not match the low byte of the offset, then something is
            // wrong.
            //

            if ((BytesComplete == 1) &&
                (MapBuffer[FileIndex][Offset] != (BYTE)Offset)) {

                PRINT_ERROR("Wrote to %s at offset %x with value %x, but "
                            "mapped buffer read %x.\n",
                            FileName,
                            Offset,
                            (BYTE)Offset,
                            MapBuffer[FileIndex][Offset]);

                Failures += 1;
            }

            break;

        case MemoryMapTestActionFileRead:

            assert(MapBuffer[FileIndex] != MAP_FAILED);

            //
            // Open the file and read from it using the file I/O APIs.
            //

            OpenFlags = O_RDONLY;
            File = shm_open(FileName, OpenFlags, 0);
            if (File < 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to open shm object %s (flags %x): "
                                "%s.\n",
                                FileName,
                                OpenFlags,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            Offset = rand() % FileSize;
            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on shm object %s offset %d failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
                break;
            }

            DEBUG_PRINT("Reading from shm object %s at offset %x.\n",
                        FileName,
                        Offset);

            Value = 0;
            do {
                BytesComplete = read(File, &Value, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
                break;
            }

            if (BytesComplete < 0) {
                PRINT_ERROR("Read failed. Read %d of 1 bytes: %s.\n",
                            BytesComplete,
                            strerror(errno));

                Failures += 1;
                break;
            }

            //
            // The value should be 0 or the low byte of the offset.
            //

            if ((BytesComplete == 1) &&
                ((Value != 0) && (Value != (BYTE)Offset))) {

                PRINT_ERROR("Read shm object %s at offset %x. Read value %x "
                            "but expected 0 or %x.\n",
                            FileName,
                            Offset,
                            Value,
                            (BYTE)Offset);

                Failures += 1;
            }

            //
            // The value should match what the mapped buffer sees unless the
            // value is 0 and the mapped buffer is now equal to the low by
            // of the offset.
            //

            if ((BytesComplete == 1) &&
                (Value != MapBuffer[FileIndex][Offset]) &&
                ((Value != 0) ||
                 (MapBuffer[FileIndex][Offset] != (BYTE)Offset))) {

                PRINT_ERROR("Read shm object %s at offset %x. Read value %x "
                            "but expected %x.\n",
                            FileName,
                            Offset,
                            Value,
                            MapBuffer[FileIndex][Offset]);

                Failures += 1;
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("s");
        }
    }

    //
    //  Unmap all the files that remain mapped.
    //

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        if (MapBuffer[FileIndex] != MAP_FAILED) {
            Result = munmap(MapBuffer[FileIndex], FileSize);
            if (Result != 0) {
                PRINT_ERROR("Failed to unmap %p: %s.\n",
                            MapBuffer[FileIndex],
                            strerror(errno));

                Failures += 1;
            }
        }
    }

    PRINT("\nMax usage: %d shm objects, %I64d bytes.\n",
          MaxSimultaneousObjects,
          (ULONGLONG)MaxSimultaneousObjects * (ULONGLONG)FileSize);

    Failures += PrintTestTime(&StartTime);

RunMemoryMapShmSharedTestEnd:
    if (MapBuffer != NULL) {
        free(MapBuffer);
    }

    return Failures;
}

static
VOID
MemoryMapTestExpectedSignalHandler (
    INT SignalNumber,
    siginfo_t *SignalInformation,
    PVOID Context
    )

/*++

Routine Description:

    This routine handles expected signals generated by the memory map tests.

Arguments:

    SignalNumber - Supplies the signal number.

    SignalInformation - Supplies a pointer to the signal information.

    Context - Supplies a pointer to the signal context.

Return Value:

    None.

--*/

{

    DEBUG_PRINT("Caught expected signal %d, code %d in process %d.\n",
                SignalNumber,
                SignalInformation->si_code,
                SignalInformation->si_pid);

    exit(SignalNumber);
    return;
}

static
VOID
MemoryMapTestUnexpectedSignalHandler (
    INT SignalNumber,
    siginfo_t *SignalInformation,
    PVOID Context
    )

/*++

Routine Description:

    This routine handles unexpected signals generated by the memory map tests.

Arguments:

    SignalNumber - Supplies the signal number.

    SignalInformation - Supplies a pointer to the signal information.

    Context - Supplies a pointer to the signal context.

Return Value:

    None.

--*/

{

    PRINT_ERROR("Caught unexpected signal %d, code %d in process %d.\n",
                SignalNumber,
                SignalInformation->si_code,
                SignalInformation->si_pid);

    exit(0);
    return;
}

ULONG
PrintTestTime (
    struct timeval *StartTime
    )

/*++

Routine Description:

    This routine prints the total time it took to run the test, given the
    starting time of the test.

Arguments:

    StartTime - Supplies a pointer to the test's start time.

Return Value:

    Returns the number of failures.

--*/

{

    struct timeval EndTime;
    ULONG Failures;
    INT Result;
    struct timeval TotalTime;

    Failures = 0;

    //
    // Record the end time and display the total time, in seconds.
    //

    Result = gettimeofday(&EndTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto PrintTestTimeEnd;
    }

    TotalTime.tv_sec = EndTime.tv_sec - StartTime->tv_sec;
    TotalTime.tv_usec = EndTime.tv_usec - StartTime->tv_usec;
    if (TotalTime.tv_usec < 0) {
        TotalTime.tv_sec -= 1;
        TotalTime.tv_usec += 1000000;
    }

    PRINT("Time: %d.%06d seconds.\n", TotalTime.tv_sec, TotalTime.tv_usec);

PrintTestTimeEnd:
    return Failures;
}

ULONG
MemoryMapEmptyTest (
    INT FileSize
    )

/*++

Routine Description:

    This routine tests access to an empty file that has been memory mapped.

Arguments:

    FileSize - Supplies the size of the file to create, if any.

Return Value:

    Returns the number of failures encountered.

--*/

{

    pid_t Child;
    struct sigaction ExpectedSignalAction;
    ULONG Failures;
    INT File;
    CHAR FileName[16];
    PBYTE MapBuffer;
    INT OpenFlags;
    pid_t Process;
    INT Result;
    BOOL SharedMemoryObject;
    INT Status;
    BYTE Value;
    pid_t WaitChild;

    Failures = 0;
    File = -1;
    MapBuffer = MAP_FAILED;

    //
    // Create a file or shared memory object private to this process.
    //

    Process = getpid();
    snprintf(FileName,
             sizeof(FileName),
             "mmbt-%06x",
             Process);

    //
    // Determine whether to test a file or shared memory object.
    //

    OpenFlags = O_RDWR | O_CREAT;
    if ((rand() % 2) == 0) {
        SharedMemoryObject = TRUE;
        File = shm_open(FileName,
                        OpenFlags,
                        MEMORY_MAP_TEST_CREATE_PERMISSIONS);

    } else {
        SharedMemoryObject = FALSE;
        DEBUG_PRINT("Creating file %s.\n", FileName);
        File = open(FileName, OpenFlags, MEMORY_MAP_TEST_CREATE_PERMISSIONS);
    }

    if (File < 0) {
        PRINT_ERROR("Failed to create file %s (flags %x): %s.\n",
                    FileName,
                    OpenFlags,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapEmptyTestEnd;
    }

    //
    // Test access beyond the end of the file. It should SIGBUS.
    //

    DEBUG_PRINT("Testing access beyond end of file %s.\n", FileName);
    MapBuffer = mmap(0, FileSize, PROT_WRITE | PROT_READ, MAP_PRIVATE, File, 0);
    if (MapBuffer == MAP_FAILED) {
        PRINT_ERROR("Failed to map file %s starting at offset 0 for %x "
                    "bytes: %s\n",
                    FileName,
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapEmptyTestEnd;
    }

    //
    // Set up the signal handler expecting a SIGBUS fault.
    //

    ExpectedSignalAction.sa_sigaction = MemoryMapTestExpectedSignalHandler;
    sigemptyset(&(ExpectedSignalAction.sa_mask));
    ExpectedSignalAction.sa_flags = SA_SIGINFO;
    sigaction(SIGBUS, &ExpectedSignalAction, NULL);

    //
    // Fork to have the child try to write to the section.
    //

    Child = fork();
    if (Child == 0) {
        sigaction(SIGBUS, &ExpectedSignalAction, NULL);
        *MapBuffer = 0x1;
        PRINT_ERROR("Wrote to mapping at %p for empty file %s.\n",
                    MapBuffer,
                    FileName);

        exit(0);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (WEXITSTATUS(Status) != SIGBUS) {
                PRINT_ERROR("Child %d exited with status %d, expected %d.\n",
                            Child,
                            WEXITSTATUS(Status),
                            SIGBUS);

                Failures += 1;
            }
        }
    }

    //
    // Fork to have the child try to read from the section.
    //

    Child = fork();
    if (Child == 0) {
        sigaction(SIGBUS, &ExpectedSignalAction, NULL);
        Value = *MapBuffer;
        PRINT_ERROR("Read %x from mapping at %p for empty file %s.\n",
                    Value,
                    MapBuffer,
                    FileName);

        exit(0);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (WEXITSTATUS(Status) != SIGBUS) {
                PRINT_ERROR("Child %d exited with status %d, expected %d.\n",
                            Child,
                            WEXITSTATUS(Status),
                            SIGBUS);

                Failures += 1;
            }
        }
    }

MemoryMapEmptyTestEnd:
    if (MapBuffer != MAP_FAILED) {
        Result = munmap(MapBuffer, FileSize);
        if (Result != 0) {
            PRINT_ERROR("Failed to unmap file %s at %p: %s.\n",
                        FileName,
                        MapBuffer,
                        strerror(errno));

            Failures += 1;
        }
    }

    if (File >= 0) {
        Result = close(File);
        if (Result != 0) {
            PRINT_ERROR("Failed to close file %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }

        if (SharedMemoryObject != FALSE) {
            Result = shm_unlink(FileName);

        } else {
            Result = unlink(FileName);
        }

        if (Result != 0) {
            PRINT_ERROR("Failed to unlink file %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
MemoryMapTruncateTest (
    INT FileSize
    )

/*++

Routine Description:

    This routine tests writing to a file beyond the point of truncation.

Arguments:

    FileSize - Supplies the size of the file to use.

Return Value:

    Returns the number of failures encountered.

--*/

{

    pid_t Child;
    struct sigaction ExpectedSignalAction;
    ULONG Failures;
    INT File;
    CHAR FileName[16];
    PBYTE MapBuffer;
    INT NewFileSize;
    INT OpenFlags;
    LONG PageSize;
    pid_t Process;
    INT Result;
    BOOL SharedMemoryObject;
    INT Status;
    struct sigaction UnexpectedSignalAction;
    BYTE Value;
    pid_t WaitChild;

    Failures = 0;
    File = -1;
    MapBuffer = MAP_FAILED;

    //
    // Create a file private to this process.
    //

    Process = getpid();
    snprintf(FileName,
             sizeof(FileName),
             "mmbt-%06x",
             Process);

    //
    // Determine whether to create a file or shared memory object.
    //

    OpenFlags = O_RDWR | O_CREAT;
    if ((rand() % 2) == 0) {
        SharedMemoryObject = TRUE;
        DEBUG_PRINT("Creating shm object %s.\n", FileName);
        File = shm_open(FileName,
                        OpenFlags,
                        MEMORY_MAP_TEST_CREATE_PERMISSIONS);

    } else {
        SharedMemoryObject = FALSE;
        DEBUG_PRINT("Creating file %s.\n", FileName);
        File = open(FileName, OpenFlags, MEMORY_MAP_TEST_CREATE_PERMISSIONS);
    }

    if (File < 0) {
        PRINT_ERROR("Failed to create file %s (flags %x): %s.\n",
                    FileName,
                    OpenFlags,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapTruncateFileTestEnd;
    }

    //
    // Map the whole file read/write.
    //

    MapBuffer = mmap(0, FileSize, PROT_WRITE | PROT_READ, MAP_PRIVATE, File, 0);
    if (MapBuffer == MAP_FAILED) {
        PRINT_ERROR("Failed to map file %s starting at offset 0 for %x "
                    "bytes: %s\n",
                    FileName,
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapTruncateFileTestEnd;
    }

    //
    // Use ftruncate to extend the file size to the supplied size.
    //

    DEBUG_PRINT("Testing file access after truncate.\n");
    Result = ftruncate(File, FileSize);
    if (Result != 0) {
        PRINT_ERROR("ftruncate failed to increase file size to %x: %s.\n",
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapTruncateFileTestEnd;
    }

    //
    // Initialize the unexpected signal handler and try to write to the last
    // byte of the file.
    //

    UnexpectedSignalAction.sa_sigaction = MemoryMapTestUnexpectedSignalHandler;
    sigemptyset(&(UnexpectedSignalAction.sa_mask));
    UnexpectedSignalAction.sa_flags = SA_SIGINFO;
    sigaction(SIGBUS, &UnexpectedSignalAction, NULL);
    *(MapBuffer + FileSize - 0x1) = 0x1;

    //
    // Trucate the file down by a page so that the last byte is no longer
    // mapped. Keep in mind that only whole pages beyond the end of the new
    // file size get unmapped.
    //

    PageSize = sysconf(_SC_PAGE_SIZE);
    NewFileSize = FileSize - PageSize;
    Result = ftruncate(File, NewFileSize);
    if (Result != 0) {
        PRINT_ERROR("ftruncate failed to decrease file size from %x to %x: "
                    "%s.\n",
                    FileSize,
                    NewFileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapTruncateFileTestEnd;
    }

    //
    // Writing within the new file size should still work.
    //

    *(MapBuffer + NewFileSize - 0x1) = 0x1;

    //
    // Now set up for an expected fault.
    //

    ExpectedSignalAction.sa_sigaction = MemoryMapTestExpectedSignalHandler;
    sigemptyset(&(ExpectedSignalAction.sa_mask));
    ExpectedSignalAction.sa_flags = SA_SIGINFO;

    //
    // Fork to have the child try to write to the last byte in the orignal
    // file size.
    //

    Child = fork();
    if (Child == 0) {
        sigaction(SIGBUS, &ExpectedSignalAction, NULL);
        *(MapBuffer + FileSize - 0x1) = 0x1;
        PRINT_ERROR("Wrote beyond the end of file %s with mapping at %p.\n",
                    FileName,
                    MapBuffer);

        exit(0);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (WEXITSTATUS(Status) != SIGBUS) {
                PRINT_ERROR("Child %d exited with status %d, expected %d.\n",
                            Child,
                            WEXITSTATUS(Status),
                            SIGBUS);

                Failures += 1;
            }
        }
    }

    //
    // Fork to have the child try to read from the last byte in the orignal
    // file size.
    //

    Child = fork();
    if (Child == 0) {
        sigaction(SIGBUS, &ExpectedSignalAction, NULL);
        Value = *(MapBuffer + FileSize - 0x1);
        PRINT_ERROR("Read %x from beyond the end of file %s with mapping at "
                    "%p.\n",
                    Value,
                    FileName,
                    MapBuffer);

        exit(0);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (WEXITSTATUS(Status) != SIGBUS) {
                PRINT_ERROR("Child %d exited with status %d, expected %d.\n",
                            Child,
                            WEXITSTATUS(Status),
                            SIGBUS);

                Failures += 1;
            }
        }
    }

MemoryMapTruncateFileTestEnd:
    if (MapBuffer != MAP_FAILED) {
        Result = munmap(MapBuffer, FileSize);
        if (Result != 0) {
            PRINT_ERROR("Failed to unmap file %s at %p: %s.\n",
                        FileName,
                        MapBuffer,
                        strerror(errno));

            Failures += 1;
        }
    }

    if (File >= 0) {
        Result = close(File);
        if (Result != 0) {
            PRINT_ERROR("Failed to close file %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }

        if (SharedMemoryObject != FALSE) {
            Result = shm_unlink(FileName);

        } else {
            Result = unlink(FileName);
        }

        if (Result != 0) {
            PRINT_ERROR("Failed to unlink file %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
MemoryMapReadOnlyTest (
    INT FileSize
    )

/*++

Routine Description:

    This routine tests writing to a read-only memory mapped section.

Arguments:

    FileSize - Supplies the size of the file to use, if needed.

Return Value:

    Returns the number of errors encountered.

--*/

{

    pid_t Child;
    struct sigaction ExpectedSignalAction;
    ULONG Failures;
    INT File;
    CHAR FileName[16];
    PBYTE MapBuffer;
    INT OpenFlags;
    pid_t Process;
    INT Result;
    BOOL SharedMemoryObject;
    INT Status;
    struct sigaction UnexpectedSignalAction;
    BYTE Value;
    pid_t WaitChild;

    Failures = 0;
    File = -1;
    MapBuffer = MAP_FAILED;

    //
    // Create a file private to this process.
    //

    Process = getpid();
    snprintf(FileName,
             sizeof(FileName),
             "mmbt-%06x",
             Process);

    //
    // Determine whether to create a regular file or shared memory object.
    //

    OpenFlags = O_RDWR | O_CREAT;
    if ((rand() % 2) == 0) {
        SharedMemoryObject = TRUE;
        DEBUG_PRINT("Creating shm object %s.\n", FileName);
        File = shm_open(FileName,
                        OpenFlags,
                        MEMORY_MAP_TEST_CREATE_PERMISSIONS);

    } else {
        SharedMemoryObject = FALSE;
        DEBUG_PRINT("Creating file %s.\n", FileName);
        File = open(FileName, OpenFlags, MEMORY_MAP_TEST_CREATE_PERMISSIONS);
    }

    if (File < 0) {
        PRINT_ERROR("Failed to create file %s (flags %x): %s.\n",
                    FileName,
                    OpenFlags,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapReadOnlyFileTestEnd;
    }

    //
    // Map the whole file read-only.
    //

    MapBuffer = mmap(0, FileSize, PROT_READ, MAP_PRIVATE, File, 0);
    if (MapBuffer == MAP_FAILED) {
        PRINT_ERROR("Failed to map file %s starting at offset 0 for %x "
                    "bytes: %s\n",
                    FileName,
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapReadOnlyFileTestEnd;
    }

    //
    // Use ftruncate to extend the file size to the supplied size.
    //

    DEBUG_PRINT("Testing read-only mapping write access.\n");
    Result = ftruncate(File, FileSize);
    if (Result != 0) {
        PRINT_ERROR("ftruncate failed to increase file size to %x: %s.\n",
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapReadOnlyFileTestEnd;
    }

    //
    // Initialize the unexpected signal handler and try a read from the memory
    // map.
    //

    UnexpectedSignalAction.sa_sigaction = MemoryMapTestUnexpectedSignalHandler;
    sigemptyset(&(UnexpectedSignalAction.sa_mask));
    UnexpectedSignalAction.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &UnexpectedSignalAction, NULL);
    Value = *MapBuffer;
    DEBUG_PRINT("Successfully read %x from buffer %p for file %s.\n",
                Value,
                MapBuffer,
                FileName);

    //
    // Now set up for an expected fault.
    //

    ExpectedSignalAction.sa_sigaction = MemoryMapTestExpectedSignalHandler;
    sigemptyset(&(ExpectedSignalAction.sa_mask));
    ExpectedSignalAction.sa_flags = SA_SIGINFO;

    //
    // Fork to have the child try to write to the first byte in the file.
    //

    Child = fork();
    if (Child == 0) {
        sigaction(SIGSEGV, &ExpectedSignalAction, NULL);
        *MapBuffer = 0x1;
        PRINT_ERROR("Wrote to a read-only mapping at %p for file %s.\n",
                    MapBuffer,
                    FileName);

        exit(0);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (WEXITSTATUS(Status) != SIGSEGV) {
                PRINT_ERROR("Child %d exited with status %d, expected %d.\n",
                            Child,
                            WEXITSTATUS(Status),
                            SIGSEGV);

                Failures += 1;
            }
        }
    }

MemoryMapReadOnlyFileTestEnd:
    if (MapBuffer != MAP_FAILED) {
        Result = munmap(MapBuffer, FileSize);
        if (Result != 0) {
            PRINT_ERROR("Failed to unmap file %s at %p: %s.\n",
                        FileName,
                        MapBuffer,
                        strerror(errno));

            Failures += 1;
        }
    }

    if (File >= 0) {
        Result = close(File);
        if (Result != 0) {
            PRINT_ERROR("Failed to close file %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }

        if (SharedMemoryObject != FALSE) {
            Result = shm_unlink(FileName);

        } else {
            Result = unlink(FileName);
        }

        if (Result != 0) {
            PRINT_ERROR("Failed to unlink file %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
MemoryMapNoAccessTest (
    INT FileSize
    )

/*++

Routine Description:

    This routine tests access of a file that is memory mapped with no access
    permissions.

Arguments:

    FileSize - Supplies the size of the file to use, if needed.

Return Value:

    Returns the number of errors encountered.

--*/

{

    pid_t Child;
    struct sigaction ExpectedSignalAction;
    ULONG Failures;
    INT File;
    CHAR FileName[16];
    PBYTE MapBuffer;
    INT OpenFlags;
    pid_t Process;
    INT Result;
    BOOL SharedMemoryObject;
    INT Status;
    BYTE Value;
    pid_t WaitChild;

    Failures = 0;
    File = -1;
    MapBuffer = MAP_FAILED;

    //
    // Create a file private to this process.
    //

    Process = getpid();
    snprintf(FileName,
             sizeof(FileName),
             "mmbt-%06x",
             Process);

    OpenFlags = O_RDWR | O_CREAT;
    if ((rand() % 2) == 0) {
        SharedMemoryObject = TRUE;
        DEBUG_PRINT("Creating shm object %s.\n", FileName);
        File = shm_open(FileName,
                        OpenFlags,
                        MEMORY_MAP_TEST_CREATE_PERMISSIONS);

    } else {
        SharedMemoryObject = FALSE;
        DEBUG_PRINT("Creating file %s.\n", FileName);
        File = open(FileName, OpenFlags, MEMORY_MAP_TEST_CREATE_PERMISSIONS);
    }

    if (File < 0) {
        PRINT_ERROR("Failed to create file %s (flags %x): %s.\n",
                    FileName,
                    OpenFlags,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapNoAccessFileTestEnd;
    }

    //
    // Map the whole file with no access permissions..
    //

    MapBuffer = mmap(0, FileSize, PROT_NONE, MAP_PRIVATE, File, 0);
    if (MapBuffer == MAP_FAILED) {
        PRINT_ERROR("Failed to map file %s starting at offset 0 for %x "
                    "bytes: %s\n",
                    FileName,
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapNoAccessFileTestEnd;
    }

    //
    // Use ftruncate to extend the file size to the supplied size.
    //

    DEBUG_PRINT("Testing no access mapping access.\n");
    Result = ftruncate(File, FileSize);
    if (Result != 0) {
        PRINT_ERROR("ftruncate failed to increase file size to %x: %s.\n",
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapNoAccessFileTestEnd;
    }

    //
    // Set up for an expected fault.
    //

    ExpectedSignalAction.sa_sigaction = MemoryMapTestExpectedSignalHandler;
    sigemptyset(&(ExpectedSignalAction.sa_mask));
    ExpectedSignalAction.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &ExpectedSignalAction, NULL);

    //
    // Fork to have the child try to write to the first byte in the file.
    //

    Child = fork();
    if (Child == 0) {
        sigaction(SIGSEGV, &ExpectedSignalAction, NULL);
        *MapBuffer = 0x1;
        PRINT_ERROR("Wrote to a no-access mapping at %p for file %s.\n",
                    MapBuffer,
                    FileName);

        exit(0);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (WEXITSTATUS(Status) != SIGSEGV) {
                PRINT_ERROR("Child %d exited with status %d, expected %d.\n",
                            Child,
                            WEXITSTATUS(Status),
                            SIGSEGV);

                Failures += 1;
            }
        }
    }

    //
    // Fork to have the child try to read the first byte in the file.
    //

    Child = fork();
    if (Child == 0) {
        sigaction(SIGSEGV, &ExpectedSignalAction, NULL);
        Value = *MapBuffer;
        PRINT_ERROR("Read %x from a no-access mapping at %p for file %s.\n",
                    Value,
                    MapBuffer,
                    FileName);

        exit(0);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (WEXITSTATUS(Status) != SIGSEGV) {
                PRINT_ERROR("Child %d exited with status %d, expected %d.\n",
                            Child,
                            WEXITSTATUS(Status),
                            SIGSEGV);

                Failures += 1;
            }
        }
    }

MemoryMapNoAccessFileTestEnd:
    if (MapBuffer != MAP_FAILED) {
        Result = munmap(MapBuffer, FileSize);
        if (Result != 0) {
            PRINT_ERROR("Failed to unmap file %s at %p: %s.\n",
                        FileName,
                        MapBuffer,
                        strerror(errno));

            Failures += 1;
        }
    }

    if (File >= 0) {
        Result = close(File);
        if (Result != 0) {
            PRINT_ERROR("Failed to close file %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }

        if (SharedMemoryObject != FALSE) {
            Result = shm_unlink(FileName);

        } else {
            Result = unlink(FileName);
        }

        if (Result != 0) {
            PRINT_ERROR("Failed to unlink file %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
MemoryMapAnonymousTest (
    INT FileSize
    )

/*++

Routine Description:

    This routine tests anonymous memory mapped sections that are not backed
    by a file.

Arguments:

    FileSize - Supplies the size of the file to use, if needed.

Return Value:

    Returns the number of errors encountered.

--*/

{

    pid_t Child;
    ULONG Failures;
    PBYTE MapBuffer;
    INT Result;
    INT Status;
    BYTE Value;
    pid_t WaitChild;

    Failures = 0;

    //
    // Create an anonymous memory mapping of the given size.
    //

    DEBUG_PRINT("Creating an anonymous memory mapping.\n");
    MapBuffer = mmap(0,
                     FileSize,
                     PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE,
                     -1,
                     0);

    if (MapBuffer == MAP_FAILED) {
        PRINT_ERROR("Failed to create anonymous memory mapping of size 0x%x "
                    "bytes: %s.\n",
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapAnonymousTestEnd;
    }

    //
    // Read the memory mapping to make sure it reads zero.
    //

    if ((*MapBuffer != 0) || (*(MapBuffer + 1) != 0)) {
        PRINT_ERROR("Failed to read zero from anonymous mapping.\n");
        Failures += 1;
    }

    //
    // Write to the memory mapping.
    //

    *MapBuffer = 0x1;

    //
    // Fork to have the child read the first byte to validate it shares with
    // the parent. Then have the child write the second byte so the parent can
    // validate that it's mapping did not change.
    //

    Child = fork();
    if (Child == 0) {
        Failures = 0;
        Value = *MapBuffer;
        if (Value != 0x1) {
            PRINT_ERROR("Anonymous child failed to read first byte. Expected "
                        "1, read %x.\n",
                        Value);

            Failures += 1;
        }

        Value = *(MapBuffer + 1);
        if (Value != 0) {
            PRINT_ERROR("Anonymous child failed to read first byte. Expected "
                        "0, read %x.\n",
                        Value);

            Failures += 1;
        }

        *(MapBuffer + 1) = 0x2;
        exit(Failures);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (!WIFEXITED(Status)) {
                PRINT_ERROR("Child %d returned with status %x\n",
                            Status);

                Failures += 1;
            }

            Failures += WEXITSTATUS(Status);
        }
    }

    //
    // Because the mapping was private, the parent should be able to re-read
    // the second byte and see a 0 and not a 2.
    //

    Value = *(MapBuffer + 1);
    if (Value != 0) {
        PRINT_ERROR("Anonymous parent failed to read 0 from mapping %p. Read "
                    "%x.\n",
                    MapBuffer + 1,
                    Value);

        Failures += 1;
    }

MemoryMapAnonymousTestEnd:
    if (MapBuffer != MAP_FAILED) {
        Result = munmap(MapBuffer, FileSize);
        if (Result != 0) {
            PRINT_ERROR("Anonymous failed to unmap memory map at %p: %s.\n",
                        MapBuffer,
                        strerror(errno));

            Failures += 1;
        }
    }

    return Failures;
}

ULONG
MemoryMapSharedAnonymousTest (
    INT FileSize
    )

/*++

Routine Description:

    This routine tests shared anonymous memory mapped sections that are not
    backed by a file.

Arguments:

    FileSize - Supplies the size of the file to use, if needed.

Return Value:

    Returns the number of errors encountered.

--*/

{

    pid_t Child;
    ULONG Failures;
    PBYTE MapBuffer;
    INT Result;
    INT Status;
    BYTE Value;
    pid_t WaitChild;

    Failures = 0;

    //
    // Create an anonymous memory mapping of the given size.
    //

    DEBUG_PRINT("Creating an anonymous shared memory mapping.\n");
    MapBuffer = mmap(0,
                     FileSize,
                     PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_SHARED,
                     -1,
                     0);

    if (MapBuffer == MAP_FAILED) {
        PRINT_ERROR("Failed to create shared anonymous memory mapping of size "
                    "0x%x bytes: %s.\n",
                    FileSize,
                    strerror(errno));

        Failures += 1;
        goto MemoryMapSharedAnonymousTestEnd;
    }

    //
    // Read the memory mapping to make sure it reads zero.
    //

    if ((*MapBuffer != 0) || (*(MapBuffer + 1) != 0)) {
        PRINT_ERROR("Failed to read zero from shared anonymous mapping.\n");
        Failures += 1;
    }

    //
    // Write to the memory mapping.
    //

    *MapBuffer = 0x1;

    //
    // Fork to have the child read the first byte to validate it shares with
    // the parent. Then have the child write the second byte so the parent can
    // validate that it's mapping changed.
    //

    Child = fork();
    if (Child == 0) {
        Failures = 0;
        Value = *MapBuffer;
        if (Value != 0x1) {
            PRINT_ERROR("Shared anonymous child failed to read first byte. "
                        "Expected 1, read %x.\n",
                        Value);

            Failures += 1;
        }

        Value = *(MapBuffer + 1);
        if (Value != 0) {
            PRINT_ERROR("Shared anonymous child failed to read first byte. "
                        "Expected 0, read %x.\n",
                        Value);

            Failures += 1;
        }

        *(MapBuffer + 1) = 0x2;
        exit(Failures);

    } else {
        WaitChild = waitpid(Child, &Status, 0);
        if (WaitChild == -1) {
            PRINT_ERROR("Failed to wait for child %d: %s.\n",
                        Child,
                        strerror(errno));

        } else {

            assert(WaitChild == Child);

            if (!WIFEXITED(Status)) {
                PRINT_ERROR("Child %d returned with status %x\n",
                            Status);

                Failures += 1;
            }

            Failures += WEXITSTATUS(Status);
        }
    }

    //
    // Because the mapping was private, the parent should be able to re-read
    // the second byte and see a 0 and not a 2.
    //

    Value = *(MapBuffer + 1);
    if (Value != 0x2) {
        PRINT_ERROR("Shared anonymous parent failed to read 0x2 from mapping "
                    "%p. Read %x.\n",
                    MapBuffer + 1,
                    Value);

        Failures += 1;
    }

MemoryMapSharedAnonymousTestEnd:
    if (MapBuffer != MAP_FAILED) {
        Result = munmap(MapBuffer, FileSize);
        if (Result != 0) {
            PRINT_ERROR("Shared anonymous failed to unmap memory map at %p: "
                        "%s.\n",
                        MapBuffer,
                        strerror(errno));

            Failures += 1;
        }
    }

    return Failures;
}

