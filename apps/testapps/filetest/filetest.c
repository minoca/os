/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    filetest.c

Abstract:

    This module implements the tests used to verify that basic file operations
    are working.

Author:

    Evan Green 27-Sep-2013

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

//
// --------------------------------------------------------------------- Macros
//

#define DEBUG_PRINT(...)                                \
    if (FileTestVerbosity >= TestVerbosityDebug) {      \
        printf(__VA_ARGS__);                            \
    }

#define PRINT(...)                                      \
    if (FileTestVerbosity >= TestVerbosityNormal) {     \
        printf(__VA_ARGS__);                            \
    }

#define PRINT_ERROR(...) fprintf(stderr, "\nfiletest: " __VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

#define FILE_TEST_VERSION_MAJOR 1
#define FILE_TEST_VERSION_MINOR 0

#define FILE_TEST_USAGE                                                        \
    "Usage: filetest [options] \n"                                             \
    "This utility hammers on the file system. Options are:\n"                  \
    "  -c, --file-count <count> -- Set the number of files to create.\n"       \
    "  -s, --file-size <size> -- Set the size of each file in bytes.\n"        \
    "  -i, --iterations <count> -- Set the number of operations to perform.\n" \
    "  -p, --threads <count> -- Set the number of threads to spin up.\n"       \
    "  -r, --seed=int -- Set the random seed for deterministic results.\n"     \
    "  -t, --test -- Set the test to perform. Valid values are all, \n"        \
    "      consistency, concurrency, seek, streamseek, append, and \n"         \
    "      uninitialized.\n"                                                   \
    "  --debug -- Print lots of information about what's happening.\n"         \
    "  --quiet -- Print only errors.\n"                                        \
    "  --no-cleanup -- Leave test files around for debugging.\n"               \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the test version and exit.\n"                        \

#define FILE_TEST_OPTIONS_STRING "c:s:i:t:p:r:ndqhV"

#define FILE_TEST_CREATE_PERMISSIONS (S_IRUSR | S_IWUSR)

#define DEFAULT_FILE_COUNT 20
#define DEFAULT_FILE_SIZE (1024 * 17)
#define DEFAULT_OPERATION_COUNT (DEFAULT_FILE_COUNT * 50)
#define DEFAULT_THREAD_COUNT 1

#define UNINITIALIZED_DATA_PATTERN 0xAB
#define UNINITIALIZED_DATA_SEEK_MAX 0x200

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _FILE_TEST_ACTION {
    FileTestActionWrite,
    FileTestActionRead,
    FileTestActionDelete,
    FileTestActionRename,
    FileTestActionCount
} FILE_TEST_ACTION, *PFILE_TEST_ACTION;

typedef enum _TEST_VERBOSITY {
    TestVerbosityQuiet,
    TestVerbosityNormal,
    TestVerbosityDebug
} TEST_VERBOSITY, *PTEST_VERBOSITY;

typedef enum _FILE_TEST_TYPE {
    FileTestAll,
    FileTestConsistency,
    FileTestSeek,
    FileTestStreamSeek,
    FileTestConcurrency,
    FileTestAppend,
    FileTestUninitializedData
} FILE_TEST_TYPE, *PFILE_TEST_TYPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
RunFileConsistencyTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

ULONG
RunFileConcurrencyTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

ULONG
RunFileAppendTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

ULONG
RunFileSeekTest (
    INT BlockCount,
    INT BlockSize,
    INT Iterations
    );

ULONG
RunStreamSeekTest (
    INT BlockCount,
    INT BlockSize,
    INT Iterations
    );

ULONG
RunFileUninitializedDataTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    );

ULONG
PrintTestTime (
    struct timeval *StartTime
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Higher levels here print out more stuff.
//

TEST_VERBOSITY FileTestVerbosity = TestVerbosityNormal;

//
// Set this boolean to skip cleaning up files.
//

BOOL FileTestNoCleanup = FALSE;

struct option FileTestLongOptions[] = {
    {"file-count", required_argument, 0, 'c'},
    {"file-size", required_argument, 0, 's'},
    {"iterations", required_argument, 0, 'i'},
    {"seed", required_argument, 0, 'r'},
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
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the file test program.

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
    INT Failures;
    INT FileCount;
    INT FileSize;
    BOOL IsParent;
    INT Iterations;
    INT Option;
    INT Seed;
    INT Status;
    FILE_TEST_TYPE Test;
    INT Threads;

    Children = NULL;
    Failures = 0;
    FileCount = DEFAULT_FILE_COUNT;
    FileSize = DEFAULT_FILE_SIZE;
    Iterations = DEFAULT_OPERATION_COUNT;
    Seed = time(NULL) ^ getpid();
    Test = FileTestAll;
    Threads = DEFAULT_THREAD_COUNT;
    Status = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             FILE_TEST_OPTIONS_STRING,
                             FileTestLongOptions,
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
            FileTestNoCleanup = TRUE;
            break;

        case 'p':
            Threads = strtol(optarg, &AfterScan, 0);
            if ((Threads <= 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid thread count %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'r':
            Seed = strtol(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                PRINT_ERROR("Invalid seed %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 't':
            if (strcasecmp(optarg, "all") == 0) {
                Test = FileTestAll;

            } else if (strcasecmp(optarg, "consistency") == 0) {
                Test = FileTestConsistency;

            } else if (strcasecmp(optarg, "seek") == 0) {
                Test = FileTestSeek;

            } else if (strcasecmp(optarg, "streamseek") == 0) {
                Test = FileTestStreamSeek;

            } else if (strcasecmp(optarg, "concurrency") == 0) {
                Test = FileTestConcurrency;

            } else if (strcasecmp(optarg, "append") == 0) {
                Test = FileTestAppend;

            } else if (strcasecmp(optarg, "uninitialized") == 0) {
                Test = FileTestUninitializedData;

            } else {
                PRINT_ERROR("Invalid test: %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'd':
            FileTestVerbosity = TestVerbosityDebug;
            break;

        case 'q':
            FileTestVerbosity = TestVerbosityQuiet;
            break;

        case 'V':
            printf("Minoca filetest version %d.%d\n",
                   FILE_TEST_VERSION_MAJOR,
                   FILE_TEST_VERSION_MINOR);

            return 1;

        case 'h':
            printf(FILE_TEST_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    srand(Seed);
    DEBUG_PRINT("Seed: %d.\n", Seed);
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

    if ((Test == FileTestAll) || (Test == FileTestConsistency)) {
        Failures += RunFileConsistencyTest(FileCount, FileSize, Iterations);
    }

    if ((Test == FileTestAll) || (Test == FileTestSeek)) {
        Failures += RunFileSeekTest(FileCount, FileSize, Iterations);
    }

    if ((Test == FileTestAll) || (Test == FileTestStreamSeek)) {
        Failures += RunStreamSeekTest(FileCount, FileSize, Iterations);
    }

    if ((Test == FileTestAll) || (Test == FileTestConcurrency)) {
        Failures += RunFileConcurrencyTest(FileCount, FileSize, Iterations);
    }

    if ((Test == FileTestAll) || (Test == FileTestAppend)) {
        Failures += RunFileAppendTest(FileCount, FileSize, Iterations);
    }

    if ((Test == FileTestAll) || (Test == FileTestUninitializedData)) {
        Failures += RunFileUninitializedDataTest(FileCount,
                                                 FileSize,
                                                 Iterations);
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
    if (Children != NULL) {
        free(Children);
    }

    if (Status != 0) {
        PRINT_ERROR("Error: %d.\n", Status);
    }

    if (Failures != 0) {
        PRINT_ERROR("\n   *** %d failures in filetest ***\n", Failures);
        return Failures;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
RunFileConsistencyTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the file consistency test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    FILE_TEST_ACTION Action;
    ssize_t BytesComplete;
    ULONG Failures;
    INT File;
    PINT FileBuffer;
    INT FileIndex;
    CHAR FileName[16];
    PINT FileOffset;
    INT FillIndex;
    INT Iteration;
    INT MaxSimultaneousFiles;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    INT SimultaneousFiles;
    struct timeval StartTime;
    INT TotalBytesComplete;

    Failures = 0;
    FileBuffer = NULL;
    FileOffset = NULL;

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunFileConsistencyTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running file consistency with %d files of %d bytes each. "
          "%d iterations.\n",
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
    FileOffset = malloc(FileCount * sizeof(INT));
    if (FileOffset == NULL) {
        Failures += 1;
        goto RunFileConsistencyTestEnd;
    }

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        FileOffset[FileIndex] = -1;
    }

    FileSize = ALIGN_RANGE_UP(FileSize, sizeof(INT));
    FileBuffer = malloc(FileSize);
    if (FileBuffer == NULL) {
        Failures += 1;
        goto RunFileConsistencyTestEnd;
    }

    //
    // Perform the file operations. This test writes an entire file with
    // incremental values and then tests that any file reads return the same
    // values.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % FileCount;
        snprintf(FileName,
                 sizeof(FileName),
                 "fot%x-%06x",
                 Process & 0xFFFF,
                 FileIndex);

        Action = rand() % FileTestActionRename;

        //
        // If the file has yet to be created, then the action must be write.
        //

        if (FileOffset[FileIndex] == -1) {
            Action = FileTestActionWrite;
        }

        switch (Action) {
        case FileTestActionWrite:
            if (FileOffset[FileIndex] == -1) {
                SimultaneousFiles += 1;
                if (SimultaneousFiles > MaxSimultaneousFiles) {
                    MaxSimultaneousFiles = SimultaneousFiles;
                }
            }

            OpenFlags = O_WRONLY | O_CREAT;
            if ((rand() & 0x1) != 0) {
                OpenFlags |= O_TRUNC;
            }

            File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            FileOffset[FileIndex] = rand();
            DEBUG_PRINT("Writing file %s, Value %x\n",
                        FileName,
                        FileOffset[FileIndex]);

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = FileOffset[FileIndex] + FillIndex;
            }

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

        case FileTestActionRead:
            DEBUG_PRINT("Reading file %s, Value should be %x\n",
                        FileName,
                        FileOffset[FileIndex]);

            OpenFlags = O_RDONLY;
            if ((rand() & 0x1) != 0) {
                OpenFlags = O_RDWR;
            }

            File = open(FileName, OpenFlags);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = 0xFEEEF00D;
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

            for (FillIndex = 0;
                 FillIndex < FileSize / sizeof(INT);
                 FillIndex += 1) {

                if (FileBuffer[FillIndex] !=
                    FileOffset[FileIndex] + FillIndex) {

                    PRINT_ERROR("Read data file %s index %x came back %x, "
                                "should have been %x.\n",
                                FileName,
                                FillIndex,
                                FileBuffer[FillIndex],
                                FileOffset[FileIndex] + FillIndex);

                    Failures += 1;
                }
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            break;

        case FileTestActionDelete:
            DEBUG_PRINT("Deleting file %s\n", FileName);
            if (unlink(FileName) != 0) {
                PRINT_ERROR("Failed to unlink %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }

            FileOffset[FileIndex] = -1;
            SimultaneousFiles -= 1;
            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("o");
        }
    }

    //
    // Clean up all files.
    //

    if (FileTestNoCleanup == FALSE) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            if (FileOffset[FileIndex] != -1) {
                snprintf(FileName,
                         sizeof(FileName),
                         "fot%x-%06x",
                         Process & 0xFFFF,
                         FileIndex);

                if (unlink(FileName) != 0) {
                    PRINT_ERROR("Failed to unlink %s: %s.\n",
                                FileName,
                                strerror(errno));

                    Failures += 1;
                }
            }
        }
    }

    PRINT("\nMax usage: %d files, %I64d bytes.\n",
          MaxSimultaneousFiles,
          (ULONGLONG)MaxSimultaneousFiles * (ULONGLONG)FileSize);

    Failures += PrintTestTime(&StartTime);

RunFileConsistencyTestEnd:
    if (FileOffset != NULL) {
        free(FileOffset);
    }

    if (FileBuffer != NULL) {
        free(FileBuffer);
    }

    return Failures;
}

ULONG
RunFileConcurrencyTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the file concurrency test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    FILE_TEST_ACTION Action;
    unsigned ActionSeed;
    ssize_t BytesComplete;
    INT DestinationFileIndex;
    CHAR DestinationFileName[16];
    ULONG Failures;
    INT File;
    INT FileIndex;
    CHAR FileName[16];
    INT Iteration;
    INT Offset;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    struct timeval StartTime;
    INT Value;

    Failures = 0;

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunFileConcurrencyTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running file concurrency test with %d files of %d bytes "
          "each. %d iterations.\n",
          Process,
          FileCount,
          FileSize,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    FileSize = ALIGN_RANGE_UP(FileSize, sizeof(INT));

    //
    // Get a separate seed for the random actions.
    //

    ActionSeed = time(NULL);

    //
    // Perform the file operations.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % FileCount;
        snprintf(FileName,
                 sizeof(FileName),
                 "fct-%06x",
                 FileIndex);

        Action = rand_r(&ActionSeed) % FileTestActionCount;
        switch (Action) {
        case FileTestActionWrite:
            Offset = rand() % FileSize;
            DEBUG_PRINT("Writing file %s, Offset %x\n", FileName, Offset);
            OpenFlags = O_WRONLY | O_CREAT;
            File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on file %s offset %d failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
            }

            do {
                BytesComplete = write(File, &Offset, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != 1) {
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

        case FileTestActionRead:
            Offset = rand() % FileSize;
            DEBUG_PRINT("Reading file %s, Offset %x\n", FileName, Offset);
            OpenFlags = O_RDWR | O_CREAT;
            File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on file %s offset %d failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
            }

            //
            // Reads are tricky as the file can be deleted and recreated by
            // other threads. At least validate that if the read succeeded the
            // byte should be zero or the low byte of the offset.
            //

            Value = 0;
            do {
                BytesComplete = read(File, &Value, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete < 0) {
                PRINT_ERROR("Read failed. Read %d of 1 bytes: %s.\n",
                            BytesComplete,
                            strerror(errno));

                Failures += 1;
                break;
            }

            if ((BytesComplete == 1) &&
                (Value != 0) && (Value != (Offset & 0xFF))) {

                PRINT_ERROR("Error: read of file %s at offset %x turned up "
                            "%x (should have been %x or 0).\n",
                            FileName,
                            Offset,
                            Value,
                            Offset & 0xFF);
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            break;

        case FileTestActionDelete:
            DEBUG_PRINT("Deleting file %s\n", FileName);
            if (unlink(FileName) != 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to unlink %s: %s.\n",
                                FileName,
                                strerror(errno));

                    Failures += 1;
                }
            }

            break;

        case FileTestActionRename:

            //
            // Pick a random destination file.
            //

            DestinationFileIndex = rand() % FileCount;
            snprintf(DestinationFileName,
                     sizeof(DestinationFileName),
                     "fct-%06x",
                     DestinationFileIndex);

            //
            // Rename the current file to the destination file.
            //

            DEBUG_PRINT("Renaming file %s to %s.\n",
                        FileName,
                        DestinationFileName);

            if (rename(FileName, DestinationFileName) != 0) {
                if (errno != ENOENT) {
                    PRINT_ERROR("Failed to rename %s to %s: %s.\n",
                                FileName,
                                DestinationFileName,
                                strerror(errno));

                    Failures += 1;
                }
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("c");
        }
    }

    //
    // Clean up. Sure, other threads could still be running the test, but they
    // should all clean up too.
    //

    if (FileTestNoCleanup == FALSE) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "fct-%06x",
                     FileIndex);

            Result = unlink(FileName);
            if ((Result != 0) && (errno != ENOENT)) {
                PRINT_ERROR("Failed to unlink %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }
        }
    }

    PRINT("\n");
    Failures += PrintTestTime(&StartTime);

RunFileConcurrencyTestEnd:
    return Failures;
}

ULONG
RunFileAppendTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the file append test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    FILE_TEST_ACTION Action;
    ssize_t BytesComplete;
    ULONG Failures;
    INT File;
    INT FileIndex;
    CHAR FileName[16];
    PINT FileOffset;
    INT Iteration;
    INT MaxSimultaneousFiles;
    LONG Offset;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    INT SimultaneousFiles;
    struct timeval StartTime;
    INT TotalBytesComplete;
    INT Value;

    Failures = 0;
    FileOffset = NULL;

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunFileAppendTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running file append test with %d files of %d bytes each. "
          "%d iterations.\n",
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
    FileOffset = malloc(FileCount * sizeof(INT));
    if (FileOffset == NULL) {
        Failures += 1;
        goto RunFileAppendTestEnd;
    }

    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        FileOffset[FileIndex] = 0;
    }

    FileSize = ALIGN_RANGE_UP(FileSize, sizeof(INT));

    //
    // Perform the file operations.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % FileCount;
        snprintf(FileName,
                 sizeof(FileName),
                 "fat%x-%06x",
                 Process & 0xFFFF,
                 FileIndex);

        Action = rand() % FileTestActionRename;

        //
        // If the file has yet to be created, then the action must be write.
        //

        if (FileOffset[FileIndex] == 0) {
            Action = FileTestActionWrite;
        }

        //
        // If the file shouldn't grow anymore, change writes into reads.
        //

        if ((FileOffset[FileIndex] > FileSize) &&
            (Action == FileTestActionWrite)) {

            Action = FileTestActionRead;
        }

        switch (Action) {
        case FileTestActionWrite:
            OpenFlags = O_WRONLY | O_APPEND;
            if (FileOffset[FileIndex] == 0) {
                OpenFlags |= O_CREAT | O_EXCL;
                SimultaneousFiles += 1;
                if (SimultaneousFiles > MaxSimultaneousFiles) {
                    MaxSimultaneousFiles = SimultaneousFiles;
                }
            }

            DEBUG_PRINT("Writing file %s, Value %x\n",
                        FileName,
                        FileOffset[FileIndex]);

            File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            //
            // Seek somewhere to try and throw it off.
            //

            Result = lseek(File, rand(), SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek failed. Result %d: %s.\n",
                            Result,
                            strerror(errno));

                Failures += 1;
            }

            Value = FileOffset[FileIndex];
            FileOffset[FileIndex] += 1;
            do {
                BytesComplete = write(File, &Value, sizeof(INT));

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != sizeof(INT)) {
                PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                            BytesComplete,
                            sizeof(INT),
                            strerror(errno));

                Failures += 1;
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            break;

        case FileTestActionRead:
            if (FileOffset[FileIndex] == 0) {
                DEBUG_PRINT("Skipping read from empty file %s.\n", FileName);
                continue;
            }

            Offset = (rand() % FileOffset[FileIndex]) * sizeof(INT);
            DEBUG_PRINT("Reading file %s offset %x, Value should be %x\n",
                        FileName,
                        Offset,
                        Offset / sizeof(INT));

            OpenFlags = O_RDONLY;
            if ((rand() & 0x1) != 0) {
                OpenFlags = O_RDWR;
            }

            File = open(FileName, OpenFlags);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek failed. Result %d: %s.\n",
                            Result,
                            strerror(errno));

                Failures += 1;
            }

            Value = 0;
            TotalBytesComplete = 0;
            while (TotalBytesComplete < sizeof(INT)) {
                do {
                    BytesComplete = read(File,
                                         (PVOID)(&Value) + TotalBytesComplete,
                                         sizeof(INT) - TotalBytesComplete);

                } while ((BytesComplete < 0) && (errno == EINTR));

                if (BytesComplete <= 0) {
                    PRINT_ERROR("Read failed. Read %d (%d total) of "
                                "%d bytes: %s.\n",
                                BytesComplete,
                                TotalBytesComplete,
                                sizeof(INT),
                                strerror(errno));

                    Failures += 1;
                    break;
                }

                TotalBytesComplete += BytesComplete;
            }

            if (Value != (Offset / sizeof(INT))) {
                PRINT_ERROR("Read append data file %s offset %x came back %x, "
                            "should have been %x.\n",
                            FileName,
                            Offset,
                            Value,
                            Offset / sizeof(INT));

                Failures += 1;
            }

            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }

            break;

        case FileTestActionDelete:
            DEBUG_PRINT("Deleting file %s\n", FileName);
            if (unlink(FileName) != 0) {
                PRINT_ERROR("Failed to unlink %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }

            FileOffset[FileIndex] = 0;
            SimultaneousFiles -= 1;
            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("a");
        }
    }

    //
    // Clean up all files.
    //

    if (FileTestNoCleanup == FALSE) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            if (FileOffset[FileIndex] != 0) {
                snprintf(FileName,
                         sizeof(FileName),
                         "fat%x-%06x",
                         Process & 0xFFFF,
                         FileIndex);

                if (unlink(FileName) != 0) {
                    PRINT_ERROR("Failed to unlink %s: %s.\n",
                                FileName,
                                strerror(errno));

                    Failures += 1;
                }
            }
        }
    }

    PRINT("\nMax usage: %d files, %I64d bytes.\n",
          MaxSimultaneousFiles,
          (ULONGLONG)MaxSimultaneousFiles * (ULONGLONG)FileSize);

    Failures += PrintTestTime(&StartTime);

RunFileAppendTestEnd:
    if (FileOffset != NULL) {
        free(FileOffset);
    }

    return Failures;
}

ULONG
RunFileSeekTest (
    INT BlockCount,
    INT BlockSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the file seek test.

Arguments:

    BlockCount - Supplies the number of blocks to play with in the file.

    BlockSize - Supplies the size of each block.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    FILE_TEST_ACTION Action;
    INT BlockErrorCount;
    INT BlockIndex;
    ssize_t BytesComplete;
    ULONG Failures;
    INT File;
    PINT FileBuffer;
    CHAR FileName[10];
    PINT FileOffset;
    INT FillIndex;
    INT Iteration;
    INT MaxBlock;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    struct timeval StartTime;
    INT TotalBytesComplete;

    Failures = 0;
    FileBuffer = NULL;
    FileOffset = NULL;

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunFileSeekTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running file seek test with %d blocks of %d bytes each. "
          "%d iterations.\n",
          Process,
          BlockCount,
          BlockSize,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    MaxBlock = -1;
    FileOffset = malloc(BlockCount * sizeof(INT));
    if (FileOffset == NULL) {
        Failures += 1;
        goto RunFileSeekTestEnd;
    }

    for (BlockIndex = 0; BlockIndex < BlockCount; BlockIndex += 1) {
        FileOffset[BlockIndex] = -1;
    }

    BlockSize = ALIGN_RANGE_UP(BlockSize, sizeof(INT));
    FileBuffer = malloc(BlockSize);
    if (FileBuffer == NULL) {
        Failures += 1;
        goto RunFileSeekTestEnd;
    }

    //
    // Open up the file.
    //

    snprintf(FileName, sizeof(FileName), "ft%x", Process & 0xFFFF);
    OpenFlags = O_RDWR | O_CREAT | O_TRUNC;
    File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
    if (File < 0) {
        PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                    FileName,
                    OpenFlags,
                    strerror(errno));

        Failures += 1;
        goto RunFileSeekTestEnd;
    }

    //
    // Perform the file operations.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random block and a random action.
        //

        BlockIndex = rand() % BlockCount;
        Action = rand() % FileTestActionDelete;

        //
        // A read beyond the end of the file so far won't work, so change it
        // into a write.
        //

        if ((Action == FileTestActionRead) && (BlockIndex > MaxBlock)) {
            Action = FileTestActionWrite;
        }

        //
        // Seek to the right spot.
        //

        Result = lseek(File,
                       (ULONGLONG)BlockIndex * (ULONGLONG)BlockSize,
                       SEEK_SET);

        if (Result < 0) {
            PRINT_ERROR("Failed to seek to offset %I64x: %s.\n",
                        (ULONGLONG)BlockIndex * (ULONGLONG)BlockSize,
                        strerror(errno));

            Failures += 1;
            FileOffset[BlockIndex] = -1;
            continue;
        }

        switch (Action) {
        case FileTestActionWrite:
            if (FileOffset[BlockIndex] == -1) {
                if (MaxBlock < BlockIndex) {
                    MaxBlock = BlockIndex;
                }
            }

            FileOffset[BlockIndex] = rand();
            DEBUG_PRINT("Writing block %d, Value %x\n",
                        BlockIndex,
                        FileOffset[BlockIndex]);

            for (FillIndex = 0;
                 FillIndex < BlockSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = FileOffset[BlockIndex] + FillIndex;
            }

            do {
                BytesComplete = write(File, FileBuffer, BlockSize);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != BlockSize) {
                PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                            BytesComplete,
                            BlockSize,
                            strerror(errno));

                Failures += 1;
            }

            break;

        case FileTestActionRead:
            DEBUG_PRINT("Reading block %d, Value should be %x\n",
                        BlockIndex,
                        FileOffset[BlockIndex]);

            for (FillIndex = 0;
                 FillIndex < BlockSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = 0xFEEEF00D;
            }

            TotalBytesComplete = 0;
            while (TotalBytesComplete < BlockSize) {
                do {
                    BytesComplete = read(File,
                                         FileBuffer + TotalBytesComplete,
                                         BlockSize - TotalBytesComplete);

                } while ((BytesComplete < 0) && (errno == EINTR));

                if (BytesComplete <= 0) {
                    PRINT_ERROR("Read failed. Read %d (%d total) of "
                                "%d bytes: %s.\n",
                                BytesComplete,
                                TotalBytesComplete,
                                BlockSize,
                                strerror(errno));

                    Failures += 1;
                    break;
                }

                TotalBytesComplete += BytesComplete;
            }

            BlockErrorCount = 0;
            for (FillIndex = 0;
                 FillIndex < BlockSize / sizeof(INT);
                 FillIndex += 1) {

                //
                // If the file was never written before, it should be all
                // zeroes.
                //

                if (FileOffset[BlockIndex] == -1) {
                    if (FileBuffer[FillIndex] != 0) {
                        PRINT_ERROR("Read data block %d index %x came back %x, "
                                    "should have been zero.\n",
                                    BlockIndex,
                                    FillIndex,
                                    FileBuffer[FillIndex]);

                        Failures += 1;
                        BlockErrorCount += 1;
                    }

                //
                // If the file was written before, validate that the data is
                // still there and correct.
                //

                } else if (FileBuffer[FillIndex] !=
                           FileOffset[BlockIndex] + FillIndex) {

                    PRINT_ERROR("Read data block %d index %x came back %x, "
                                "should have been %x.\n",
                                BlockIndex,
                                FillIndex,
                                FileBuffer[FillIndex],
                                FileOffset[BlockIndex] + FillIndex);

                    Failures += 1;
                    BlockErrorCount += 1;
                }

                if (BlockErrorCount > 15) {
                    PRINT_ERROR("...you get the idea...\n");
                    break;
                }
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

    if (FileTestNoCleanup == FALSE) {
        DEBUG_PRINT("Deleting file %s\n", FileName);
        if (unlink(FileName) != 0) {
            PRINT_ERROR("Failed to unlink %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }
    }

    if (close(File) != 0) {
        PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
        Failures += 1;
    }

    PRINT("\nMax block: %d, %I64d bytes.\n",
          MaxBlock,
          (ULONGLONG)MaxBlock * (ULONGLONG)BlockSize);

    Failures += PrintTestTime(&StartTime);

RunFileSeekTestEnd:
    if (FileOffset != NULL) {
        free(FileOffset);
    }

    if (FileBuffer != NULL) {
        free(FileBuffer);
    }

    return Failures;
}

ULONG
RunStreamSeekTest (
    INT BlockCount,
    INT BlockSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the stream seek test, which is the same as the file
    seek test except it uses streams instead of raw file descriptors..

Arguments:

    BlockCount - Supplies the number of blocks to play with in the file.

    BlockSize - Supplies the size of each block.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    FILE_TEST_ACTION Action;
    INT BlockErrorCount;
    INT BlockIndex;
    ssize_t BytesComplete;
    ULONG Failures;
    FILE *File;
    PINT FileBuffer;
    CHAR FileName[10];
    PINT FileOffset;
    INT FillIndex;
    INT Iteration;
    INT MaxBlock;
    INT Percent;
    pid_t Process;
    INT Result;
    struct timeval StartTime;
    INT TotalBytesComplete;

    Failures = 0;
    FileBuffer = NULL;
    FileOffset = NULL;

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunStreamSeekTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running stream seek test with %d blocks of %d bytes "
          "each. %d iterations.\n",
          Process,
          BlockCount,
          BlockSize,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    MaxBlock = -1;
    FileOffset = malloc(BlockCount * sizeof(INT));
    if (FileOffset == NULL) {
        Failures += 1;
        goto RunStreamSeekTestEnd;
    }

    for (BlockIndex = 0; BlockIndex < BlockCount; BlockIndex += 1) {
        FileOffset[BlockIndex] = -1;
    }

    BlockSize = ALIGN_RANGE_UP(BlockSize, sizeof(INT));
    FileBuffer = malloc(BlockSize);
    if (FileBuffer == NULL) {
        Failures += 1;
        goto RunStreamSeekTestEnd;
    }

    //
    // Open up the file.
    //

    snprintf(FileName, sizeof(FileName), "st%x", Process & 0xFFFF);
    File = fopen(FileName, "w+");
    if (File == NULL) {
        PRINT_ERROR("Failed to open file %s (mode %s): %s.\n",
                    FileName,
                    "w+",
                    strerror(errno));

        Failures += 1;
        goto RunStreamSeekTestEnd;
    }

    //
    // Perform the file operations.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random block and a random action.
        //

        BlockIndex = rand() % BlockCount;
        Action = rand() % FileTestActionDelete;

        //
        // A read beyond the end of the file so far won't work, so change it
        // into a write.
        //

        if ((Action == FileTestActionRead) && (BlockIndex > MaxBlock)) {
            Action = FileTestActionWrite;
        }

        //
        // Seek to the right spot.
        //

        Result = fseeko64(File,
                          (ULONGLONG)BlockIndex * (ULONGLONG)BlockSize,
                          SEEK_SET);

        if (Result < 0) {
            PRINT_ERROR("Failed to seek to offset %I64x: %s.\n",
                        (ULONGLONG)BlockIndex * (ULONGLONG)BlockSize,
                        strerror(errno));

            Failures += 1;
            FileOffset[BlockIndex] = -1;
            continue;
        }

        switch (Action) {
        case FileTestActionWrite:
            if (FileOffset[BlockIndex] == -1) {
                if (MaxBlock < BlockIndex) {
                    MaxBlock = BlockIndex;
                }
            }

            FileOffset[BlockIndex] = rand();
            DEBUG_PRINT("Writing block %d, Value %x\n",
                        BlockIndex,
                        FileOffset[BlockIndex]);

            for (FillIndex = 0;
                 FillIndex < BlockSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = FileOffset[BlockIndex] + FillIndex;
            }

            BytesComplete = fwrite(FileBuffer, 1, BlockSize, File);
            if (BytesComplete != BlockSize) {
                PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                            BytesComplete,
                            BlockSize,
                            strerror(errno));

                Failures += 1;
            }

            break;

        case FileTestActionRead:
            DEBUG_PRINT("Reading block %d, Value should be %x\n",
                        BlockIndex,
                        FileOffset[BlockIndex]);

            for (FillIndex = 0;
                 FillIndex < BlockSize / sizeof(INT);
                 FillIndex += 1) {

                FileBuffer[FillIndex] = 0xFEEEF00D;
            }

            TotalBytesComplete = 0;
            while (TotalBytesComplete < BlockSize) {
                BytesComplete = fread(FileBuffer + TotalBytesComplete,
                                      1,
                                      BlockSize - TotalBytesComplete,
                                      File);

                if (BytesComplete <= 0) {
                    PRINT_ERROR("Read failed. Read %d (%d total) of "
                                "%d bytes: %s.\n",
                                BytesComplete,
                                TotalBytesComplete,
                                BlockSize,
                                strerror(errno));

                    Failures += 1;
                    break;
                }

                TotalBytesComplete += BytesComplete;
            }

            BlockErrorCount = 0;
            for (FillIndex = 0;
                 FillIndex < BlockSize / sizeof(INT);
                 FillIndex += 1) {

                //
                // If the file was never written before, it should be all
                // zeroes.
                //

                if (FileOffset[BlockIndex] == -1) {
                    if (FileBuffer[FillIndex] != 0) {
                        PRINT_ERROR("Read data block %d index %x came back %x, "
                                    "should have been zero.\n",
                                    BlockIndex,
                                    FillIndex,
                                    FileBuffer[FillIndex]);

                        Failures += 1;
                        BlockErrorCount += 1;
                    }

                //
                // If the file was written before, validate that the data is
                // still there and correct.
                //

                } else if (FileBuffer[FillIndex] !=
                           FileOffset[BlockIndex] + FillIndex) {

                    PRINT_ERROR("Read data block %d index %x came back %x, "
                                "should have been %x.\n",
                                BlockIndex,
                                FillIndex,
                                FileBuffer[FillIndex],
                                FileOffset[BlockIndex] + FillIndex);

                    Failures += 1;
                    BlockErrorCount += 1;
                }

                if (BlockErrorCount > 15) {
                    PRINT_ERROR("...you get the idea...\n");
                    break;
                }
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("S");
        }
    }

    if (FileTestNoCleanup == FALSE) {
        DEBUG_PRINT("Deleting file %s\n", FileName);
        if (unlink(FileName) != 0) {
            PRINT_ERROR("Failed to unlink %s: %s.\n",
                        FileName,
                        strerror(errno));

            Failures += 1;
        }
    }

    if (fclose(File) != 0) {
        PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
        Failures += 1;
    }

    PRINT("\nMax block: %d, %I64d bytes.\n",
          MaxBlock,
          (ULONGLONG)MaxBlock * (ULONGLONG)BlockSize);

    Failures += PrintTestTime(&StartTime);

RunStreamSeekTestEnd:
    if (FileOffset != NULL) {
        free(FileOffset);
    }

    if (FileBuffer != NULL) {
        free(FileBuffer);
    }

    return Failures;
}

ULONG
RunFileUninitializedDataTest (
    INT FileCount,
    INT FileSize,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the file uninitialized data test.

Arguments:

    FileCount - Supplies the number of files to work with.

    FileSize - Supplies the size of each file.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    size_t ArraySize;
    ssize_t BytesComplete;
    INT Expected;
    ULONG Failures;
    INT File;
    INT FileIndex;
    CHAR FileName[16];
    PBYTE *FileState;
    INT Iteration;
    ULONGLONG Offset;
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    off_t ResultOffset;
    INT Seek;
    struct timeval StartTime;
    PCHAR UninitializedDataBuffer;
    INT UninitializedDataSize;
    INT Value;

    Failures = 0;
    FileState = NULL;
    UninitializedDataBuffer = NULL;

    //
    // Record the test start time.
    //

    Result = gettimeofday(&StartTime, NULL);
    if (Result != 0) {
        PRINT_ERROR("Failed to get time of day: %s.\n", strerror(errno));
        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running file uninitialized data test with %d files of "
          "%d bytes each. %d iterations.\n",
          Process,
          FileCount,
          FileSize,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    FileSize = ALIGN_RANGE_UP(FileSize, sizeof(INT));

    //
    // Before starting this test, create a big file with distinct byte pattern
    // and flush it to disk and then delete it. After this any clusters
    // allocated by the test will have the pattern in the unmodified portions.
    // If the system is working correctly, this pattern should never be read.
    //

    PRINT("Scribbling the pattern 0x%x over the disk.\n",
          UNINITIALIZED_DATA_PATTERN);

    UninitializedDataSize = FileSize * FileCount;
    UninitializedDataBuffer = malloc(UninitializedDataSize);
    if (UninitializedDataBuffer == NULL) {
        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    for (FileIndex = 0; FileIndex < UninitializedDataSize; FileIndex += 1) {
        UninitializedDataBuffer[FileIndex] = (CHAR)UNINITIALIZED_DATA_PATTERN;
    }

    snprintf(FileName, sizeof(FileName), "fudt-init%x", Process & 0xFFFF);
    OpenFlags = O_WRONLY | O_CREAT;
    File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
    if (File < 0) {
        PRINT_ERROR("Filed to open file %s (flags %x): %s.\n",
                    FileName,
                    OpenFlags,
                    strerror(errno));

        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    DEBUG_PRINT("Writing file %s\n", FileName);
    do {
        BytesComplete = write(File,
                              UninitializedDataBuffer,
                              UninitializedDataSize);

    } while ((BytesComplete < 0) && (errno == EINTR));

    if (BytesComplete != UninitializedDataSize) {
        PRINT_ERROR("Write to %s failed. Wrote %d of %d bytes: %s.\n",
                    FileName,
                    BytesComplete,
                    UninitializedDataSize,
                    strerror(errno));

        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    //
    // Now flush the file to make sure the bytes make it to disk.
    //

    DEBUG_PRINT("Flushing file %s\n", FileName);
    Result = fsync(File);
    if (Result < 0) {
        PRINT_ERROR("Flush of %s failed: %s.\n", FileName, strerror(errno));
        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    //
    // Close, truncate and unlink the file to free up the clusters.
    //

    DEBUG_PRINT("Closing file %s\n", FileName);
    if (close(File) != 0) {
        PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    DEBUG_PRINT("Opening file for truncate %s\n", FileName);
    OpenFlags = O_TRUNC;
    File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
    if (File < 0) {
        PRINT_ERROR("Failed to open file %s for truncate: %s.\n",
                    FileName,
                    strerror(errno));

        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    DEBUG_PRINT("Closing file %s\n", FileName);
    if (close(File) != 0) {
        PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    DEBUG_PRINT("Deleting file %s\n", FileName);
    Result = unlink(FileName);
    if (Result != 0) {
        PRINT_ERROR("Failed to unlink %s: %s.\n",
                    FileName,
                    strerror(errno));

        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    //
    // Create an array to hold the expected state for each file.
    //

    FileState = malloc(FileCount * sizeof(PBYTE));
    if (FileState == NULL) {
        Failures += 1;
        goto RunFileUninitializedDataTestEnd;
    }

    memset(FileState, 0, FileCount * sizeof(PBYTE));

    //
    // Perform the file operations.
    //

    PRINT("Starting tests.\n");
    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % FileCount;
        snprintf(FileName,
                 sizeof(FileName),
                 "fudt%x-%06x",
                 Process & 0xFFFF,
                 FileIndex);

        //
        // If the file is yet to be created, then write to the first byte and
        // the last byte and do some flushes to make sure partial pages are
        // handled correctly.
        //

        if (FileState[FileIndex] == 0) {
            ArraySize = FileSize + UNINITIALIZED_DATA_SEEK_MAX + 1;
            FileState[FileIndex] = malloc(ArraySize);
            memset(FileState[FileIndex], 0, ArraySize);
            OpenFlags = O_RDWR | O_CREAT;
            File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            //
            // Write the first byte of the file and flush it.
            //

            Offset = 0;
            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on file %s offset 0x%I64x failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
            }

            DEBUG_PRINT("Writing file %s, Offset 0x%I64x\n", FileName, Offset);
            do {
                BytesComplete = write(File, &Offset, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != 1) {
                PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                            BytesComplete,
                            FileSize,
                            strerror(errno));

                Failures += 1;
            }

            FileState[FileIndex][Offset] = 1;
            DEBUG_PRINT("Flushing file %s\n", FileName);
            Result = fsync(File);
            if (Result < 0) {
                PRINT_ERROR("Flush of %s failed: %s.\n", FileName,
                            strerror(errno));

                Failures += 1;
                goto RunFileUninitializedDataTestEnd;
            }

            //
            // Write the second byte of the file and the last byte and then
            // flush it.
            //

            Offset = 2;
            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on file %s offset 0x%I64x failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
            }

            DEBUG_PRINT("Writing file %s, Offset 0x%I64x\n", FileName, Offset);
            do {
                BytesComplete = write(File, &Offset, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != 1) {
                PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                            BytesComplete,
                            FileSize,
                            strerror(errno));

                Failures += 1;
            }

            FileState[FileIndex][Offset] = 1;
            Offset = FileSize - 1;
            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on file %s offset 0x%I64x failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
            }

            DEBUG_PRINT("Writing file %s, Offset 0x%I64x\n", FileName, Offset);
            do {
                BytesComplete = write(File, &Offset, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete != 1) {
                PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                            BytesComplete,
                            FileSize,
                            strerror(errno));

                Failures += 1;
            }

            FileState[FileIndex][Offset] = 1;
            DEBUG_PRINT("Flushing file %s\n", FileName);
            Result = fsync(File);
            if (Result < 0) {
                PRINT_ERROR("Flush of %s failed: %s.\n", FileName,
                            strerror(errno));

                Failures += 1;
                goto RunFileUninitializedDataTestEnd;
            }

            //
            // Now read the second byte again. Flushing out the last byte of
            // the file should not have zero'd out the remainder of the first
            // page. This read is here to make sure things are correct.
            //

            Offset = 2;
            Result = lseek(File, Offset, SEEK_SET);
            if (Result < 0) {
                PRINT_ERROR("Seek on file %s offset 0x%I64x failed.\n",
                            FileName,
                            Offset);

                Failures += 1;
            }

            Value = 0;
            DEBUG_PRINT("Reading file %s, Offset 0x%I64x\n", FileName, Offset);
            do {
                BytesComplete = read(File, &Value, 1);

            } while ((BytesComplete < 0) && (errno == EINTR));

            if (BytesComplete < 0) {
                PRINT_ERROR("Read failed. Read %d of 1 bytes: %s.\n",
                            BytesComplete,
                            strerror(errno));

                Failures += 1;
                break;
            }

            if ((BytesComplete == 1) &&
                (((FileState[FileIndex][Offset] == 0) && (Value != 0)) ||
                 ((FileState[FileIndex][Offset] == 1) &&
                  (Value != (Offset & 0xFF))))) {

                PRINT_ERROR("Error: initial read of file %s at offset 0x%I64x "
                            "turned up %x (should have been %x or 0).\n",
                            FileName,
                            Offset,
                            Value,
                            Offset & 0xFF);
            }

            FileState[FileIndex][Offset] = 1;
            if (close(File) != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
            }
        }

        //
        // Picks a random spot and writes a byte. Then read a few bytes after
        // that to make sure the expected value is there.
        //

        Offset = rand() % FileSize;
        if ((Offset & 0xFF) == UNINITIALIZED_DATA_PATTERN) {
            Offset += 1;
        }

        DEBUG_PRINT("Writing file %s, Offset 0x%I64x\n", FileName, Offset);
        OpenFlags = O_RDWR | O_CREAT;
        File = open(FileName, OpenFlags, FILE_TEST_CREATE_PERMISSIONS);
        if (File < 0) {
            PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                        FileName,
                        OpenFlags,
                        strerror(errno));

            Failures += 1;
            continue;
        }

        ResultOffset = lseek(File, Offset, SEEK_SET);
        if (ResultOffset != Offset) {
            PRINT_ERROR("Seek on file %s offset 0x%I64x failed: got 0x%I64x\n",
                        FileName,
                        Offset,
                        ResultOffset);

            Failures += 1;
        }

        do {
            BytesComplete = write(File, &Offset, 1);

        } while ((BytesComplete < 0) && (errno == EINTR));

        if (BytesComplete != 1) {
            PRINT_ERROR("Write failed. Wrote %d of %d bytes: %s.\n",
                        BytesComplete,
                        FileSize,
                        strerror(errno));

            Failures += 1;
        }

        FileState[FileIndex][Offset] = 1;

        //
        // Now seek forward a bit and read.
        //

        Seek = rand() % UNINITIALIZED_DATA_SEEK_MAX;
        Offset = lseek(File, Seek, SEEK_CUR);
        if (Offset == (ULONGLONG)-1) {
            PRINT_ERROR("Seek on file %s failed to seek %d from current.\n",
                        FileName,
                        Seek);

            Failures += 1;
        }

        //
        // Reads are tricky as the file can be deleted and recreated by
        // other threads. At least validate that if the read succeeded the
        // byte should be zero or the low byte of the offset.
        //

        Value = 0;
        DEBUG_PRINT("Reading file %s, Offset 0x%I64x\n", FileName, Offset);
        do {
            BytesComplete = read(File, &Value, 1);

        } while ((BytesComplete < 0) && (errno == EINTR));

        if (BytesComplete < 0) {
            PRINT_ERROR("Read failed. Read %d of 1 bytes: %s.\n",
                        BytesComplete,
                        strerror(errno));

            Failures += 1;
            break;
        }

        Expected = 0;
        if (FileState[FileIndex][Offset] != 0) {
            Expected = (Offset & 0xFF);
        }

        if ((BytesComplete == 1) && (Value != Expected)) {
            PRINT_ERROR("Error: Read of file %s at offset 0x%I64x turned up "
                        "%x (should have been %x).\n",
                        FileName,
                        Offset,
                        Value,
                        Expected);
        }

        if (close(File) != 0) {
            PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
            Failures += 1;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("u");
        }
    }

    //
    // Clean up. Sure, other threads could still be running the test, but they
    // should all clean up too.
    //

    if (FileTestNoCleanup == FALSE) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     "fudt%x-%06x",
                     Process & 0xFFFF,
                     FileIndex);

            Result = unlink(FileName);
            if ((Result != 0) && (errno != ENOENT)) {
                PRINT_ERROR("Failed to unlink %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }
        }
    }

    PRINT("\n");
    Failures += PrintTestTime(&StartTime);

RunFileUninitializedDataTestEnd:
    if (FileState != NULL) {
        for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
            if (FileState[FileIndex] != NULL) {
                free(FileState[FileIndex]);
            }
        }

        free(FileState);
    }

    if (UninitializedDataBuffer != NULL) {
        free(UninitializedDataBuffer);
    }

    return Failures;
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

    PRINT("Time: %lld.%06d seconds.\n",
          (long long)TotalTime.tv_sec,
          TotalTime.tv_usec);

PrintTestTimeEnd:
    return Failures;
}

