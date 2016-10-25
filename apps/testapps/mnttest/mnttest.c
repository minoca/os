/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mnttest.c

Abstract:

    This module implements the tests used to verify that basic mount operations
    are working.

Author:

    Chris Stevens 25-Nov-2013

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
#include <math.h>

//
// --------------------------------------------------------------------- Macros
//

#define DEBUG_PRINT(...)                                \
    if (MountTestVerbosity >= TestVerbosityDebug) {     \
        printf(__VA_ARGS__);                            \
    }

#define PRINT(...)                                      \
    if (MountTestVerbosity >= TestVerbosityNormal) {    \
        printf(__VA_ARGS__);                            \
    }

#define PRINT_ERROR(...) fprintf(stderr, "\nmnttest: " __VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

#define MOUNT_TEST_VERSION_MAJOR 1
#define MOUNT_TEST_VERSION_MINOR 0

#define MOUNT_TEST_USAGE                                                       \
    "Usage: mnttest [options] \n"                                              \
    "This utility tests the mount and umount programs. Options are:\n"         \
    "  -c, --mount-count <count> -- Set the number of mounts to create.\n"     \
    "  -i, --iterations <count> -- Set the number of operations to perform.\n" \
    "  -p, --threads <count> -- Set the number of threads to spin up.\n"       \
    "  -t, --test -- Set the test to perform. Valid values are all, \n"        \
    "      file, directory, and concurrency.\n"                                \
    "  --debug -- Print lots of information about what's happening.\n"         \
    "  --quiet -- Print only errors.\n"                                        \
    "  --no-cleanup -- Leave test mount points and files around for \n"        \
    "      debugging.\n"                                                       \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the test version and exit.\n"                        \

#define MOUNT_TEST_OPTIONS_STRING "c:i:t:p:ndqhV"

#define MOUNT_TEST_CREATE_PERMISSIONS (S_IRUSR | S_IWUSR)

#define DEFAULT_MOUNT_COUNT 20
#define DEFAULT_OPERATION_COUNT (DEFAULT_MOUNT_COUNT * 1)
#define DEFAULT_THREAD_COUNT 1

#define MOUNT_TEST_LOG "mnttest.log"
#define MOUNT_TEST_MOUNT_FORMAT "mount --bind %s %s 2>> %s"
#define MOUNT_TEST_MOUNT_RECURSIVE_FORMAT "mount --rbind %s %s 2>> %s"
#define MOUNT_TEST_UNMOUNT_FORMAT "umount %s 2>> %s"
#define MOUNT_TEST_UNMOUNT_LAZY_FORMAT "umount -l %s 2>> %s"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MOUNT_TEST_ACTION {
    MountTestActionMount,
    MountTestActionUnmount,
    MountTestActionStat,
    MountTestActionDelete,
    MountTestActionCount
} MOUNT_TEST_ACTION, *PMOUNT_TEST_ACTION;

typedef enum _TEST_VERBOSITY {
    TestVerbosityQuiet,
    TestVerbosityNormal,
    TestVerbosityDebug
} TEST_VERBOSITY, *PTEST_VERBOSITY;

typedef enum _MOUNT_TEST_TYPE {
    MountTestAll,
    MountTestFile,
    MountTestDirectory,
    MountTestConcurrency,
    MountTestTypeCount
} MOUNT_TEST_TYPE, *PMOUNT_TEST_TYPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
RunMountFileTest (
    INT FileCount,
    INT Iterations
    );

ULONG
RunMountDirectoryTest (
    INT DirectoryCount,
    INT Iterations
    );

ULONG
RunMountConcurrencyTest (
    INT MountCount,
    INT Iterations
    );

ULONG
RunMountGenericTest (
    MOUNT_TEST_TYPE TestType,
    INT MountCount,
    INT Iterations
    );

INT
MountTestCreateFile (
    PSTR FileName,
    BOOL FileIsDirectory
    );

INT
MountTestDeleteFile (
    PSTR FileName,
    BOOL FileIsDirectory
    );

PSTR
AppendPaths (
    PSTR Path1,
    PSTR Path2
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Higher levels here print out more stuff.
//

TEST_VERBOSITY MountTestVerbosity = TestVerbosityNormal;

//
// Set this boolean to skip cleaning up files.
//

BOOL MountTestNoCleanup = FALSE;

struct option MountTestLongOptions[] = {
    {"mount-count", required_argument, 0, 'c'},
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

PSTR MountTestName[MountTestTypeCount] = {
    NULL,
    "file",
    "directory",
    "concurrency"
};

PSTR MountTestFileNameFormat[MountTestTypeCount] = {
    NULL,
    "mft%x-%06x",
    "mdt%x-%06x",
    "mct-%06x"
};

PSTR MountTestProgressCharacter[MountTestTypeCount] = {
    NULL,
    "f",
    "d",
    "c"
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
    BOOL IsParent;
    INT Iterations;
    INT MountCount;
    INT Option;
    INT Status;
    MOUNT_TEST_TYPE Test;
    INT Threads;

    Children = NULL;
    Failures = 0;
    MountCount = DEFAULT_MOUNT_COUNT;
    Iterations = DEFAULT_OPERATION_COUNT;
    Test = MountTestAll;
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
                             MOUNT_TEST_OPTIONS_STRING,
                             MountTestLongOptions,
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
            MountCount = strtol(optarg, &AfterScan, 0);
            if ((MountCount <= 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid file count %s.\n", optarg);
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
            MountTestNoCleanup = TRUE;
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
                Test = MountTestAll;

            } else if (strcasecmp(optarg, "file") == 0) {
                Test = MountTestFile;

            } else if (strcasecmp(optarg, "directory") == 0) {
                Test = MountTestDirectory;

            } else if (strcasecmp(optarg, "concurrency") == 0) {
                Test = MountTestConcurrency;

            } else {
                PRINT_ERROR("Invalid test: %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'd':
            MountTestVerbosity = TestVerbosityDebug;
            break;

        case 'q':
            MountTestVerbosity = TestVerbosityQuiet;
            break;

        case 'V':
            printf("Minoca mnttest version %d.%d\n",
                   MOUNT_TEST_VERSION_MAJOR,
                   MOUNT_TEST_VERSION_MINOR);

            return 1;

        case 'h':
            printf(MOUNT_TEST_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // Destroy the mount test log file.
    //

    unlink(MOUNT_TEST_LOG);

    //
    // Fork off any children test processes.
    //

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

    if ((Test == MountTestAll) || (Test == MountTestFile)) {
        Failures += RunMountFileTest(MountCount, Iterations);
    }

    if ((Test == MountTestAll) || (Test == MountTestDirectory)) {
        Failures += RunMountDirectoryTest(MountCount, Iterations);
    }

    if ((Test == MountTestAll) || (Test == MountTestConcurrency)) {
        Failures += RunMountConcurrencyTest(MountCount, Iterations);
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
        PRINT_ERROR("\n   *** %d failures in mnttest ***\n", Failures);
        return Failures;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
RunMountFileTest (
    INT FileCount,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the mount file test.

Arguments:

    FileCount - Supplies the number of files to work with.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    return RunMountGenericTest(MountTestFile, FileCount, Iterations);
}

ULONG
RunMountDirectoryTest (
    INT DirectoryCount,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes the mount directory test.

Arguments:

    DirectoryCount - Supplies the number of directories to work with.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    return RunMountGenericTest(MountTestDirectory, DirectoryCount, Iterations);
}

ULONG
RunMountConcurrencyTest (
    INT MountCount,
    INT Iterations
    )

/*++

Routine Description:

    This routine tests creating and removing mount points concurrently.

Arguments:

    MountCount - Supplies the number of mount points to work with.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    MOUNT_TEST_ACTION Action;
    ULONG Failures;
    INT File;
    INT FileIndex;
    CHAR FileName[16];
    struct stat FileStat;
    ino_t Inode;
    INT Iteration;
    CHAR MountCommand[64];
    INT OpenFlags;
    INT Percent;
    pid_t Process;
    INT Result;
    INT TargetIndex;
    CHAR TargetName[16];

    Failures = 0;

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running mount %s test with %d files, %d iterations.\n",
          Process,
          MountTestName[MountTestConcurrency],
          MountCount,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    //
    // Perform the mount operations. This test creates files and then mounts
    // them, unmounts them, or checks their file ID. The files are shared
    // across all threads running the test.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % MountCount;
        snprintf(FileName,
                 sizeof(FileName),
                 MountTestFileNameFormat[MountTestConcurrency],
                 FileIndex);

        Action = rand() % MountTestActionCount;
        switch (Action) {
        case MountTestActionStat:
            OpenFlags = O_RDWR | O_CREAT;
            File = open(FileName, OpenFlags, MOUNT_TEST_CREATE_PERMISSIONS);
            if (File < 0) {
                PRINT_ERROR("Failed to open file %s (flags %x): %s.\n",
                            FileName,
                            OpenFlags,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            Result = fstat(File, &FileStat);
            if (Result < 0) {
                PRINT_ERROR("Failed to stat file %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            if (FileStat.st_size == 0) {
                Inode = FileStat.st_ino;
                do {
                    Result = write(File, &Inode, sizeof(ino_t));

                } while ((Result < 0) && (errno == EINTR));

                if (Result != sizeof(ino_t)) {
                    PRINT_ERROR("Write failed. Wrote %d of %d bytes to file "
                                "%s: %s.\n",
                                Result,
                                sizeof(ino_t),
                                FileName,
                                strerror(errno));

                    Failures += 1;
                }

            } else {
                Inode = 0;
                do {
                    Result = read(File, &Inode, sizeof(ino_t));

                } while ((Result < 0) && (errno == EINTR));

                if (Result != sizeof(ino_t)) {
                    PRINT_ERROR("Read failed. Read %d of %d bytes from file "
                                "%s: %s\n.",
                                Result,
                                sizeof(ino_t),
                                FileName,
                                strerror(errno));

                    Failures += 1;
                }

                if (Inode != FileStat.st_ino) {
                    PRINT_ERROR("Mismatching inode for file %s. Read %d, "
                                "expected %d.\n",
                                FileName,
                                Inode,
                                FileStat.st_ino);

                    Failures += 1;
                }
            }

            Result = close(File);
            if (Result != 0) {
                PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
                Failures += 1;
                continue;
            }

            break;

        case MountTestActionMount:

            //
            // Pick another random file to mount on.
            //

            TargetIndex = rand() % MountCount;
            snprintf(TargetName,
                     sizeof(TargetName),
                     MountTestFileNameFormat[MountTestConcurrency],
                     TargetIndex);

            //
            // Create both files to give the mount some chance of working.
            //

            Result = MountTestCreateFile(FileName, FALSE);
            if (Result < 0) {
                Failures += 1;
                continue;
            }

            Result = MountTestCreateFile(TargetName, FALSE);
            if (Result < 0) {
                Failures += 1;
                continue;
            }

            DEBUG_PRINT("Mount file %s onto file %s\n", FileName, TargetName);
            snprintf(MountCommand,
                     sizeof(MountCommand),
                     MOUNT_TEST_MOUNT_FORMAT,
                     FileName,
                     TargetName,
                     MOUNT_TEST_LOG);

            Result = system(MountCommand);
            if (WIFSIGNALED(Result) &&
                ((WTERMSIG(Result) == SIGINT) ||
                 (WTERMSIG(Result) == SIGQUIT))) {

                goto RunMountConcurrencyTestEnd;
            }

            if ((Result != 0) && (Result != ENOFILE)) {
                PRINT_ERROR("Failed to mount %s onto %s: %s.\n",
                            FileName,
                            TargetName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        case MountTestActionUnmount:
            DEBUG_PRINT("Unmounting file %s\n", FileName);
            snprintf(MountCommand,
                     sizeof(MountCommand),
                     MOUNT_TEST_UNMOUNT_LAZY_FORMAT,
                     FileName,
                     MOUNT_TEST_LOG);

            Result = system(MountCommand);
            if (WIFSIGNALED(Result) &&
                ((WTERMSIG(Result) == SIGINT) ||
                 (WTERMSIG(Result) == SIGQUIT))) {

                goto RunMountConcurrencyTestEnd;
            }

            if ((Result != 0) && (Result != EINVAL) && (Result != ENOFILE)) {
                PRINT_ERROR("Failed to unmount %s: %d.\n",
                            FileName,
                            Result);

                Failures += 1;
                continue;
            }

            break;

        case MountTestActionDelete:
            DEBUG_PRINT("Deleting file %s\n", FileName);
            Result = MountTestDeleteFile(FileName, TRUE);
            if ((Result != 0) && (errno != EBUSY) && (errno != ENOFILE)) {
                PRINT_ERROR("Failed to delete %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("%s", MountTestProgressCharacter[MountTestConcurrency]);
        }
    }

    //
    // Clean up all files.
    //

    if (MountTestNoCleanup == FALSE) {
        for (FileIndex = 0; FileIndex < MountCount; FileIndex += 1) {
            snprintf(FileName,
                     sizeof(FileName),
                     MountTestFileNameFormat[MountTestConcurrency],
                     FileIndex);

            snprintf(MountCommand,
                     sizeof(MountCommand),
                     MOUNT_TEST_UNMOUNT_LAZY_FORMAT,
                     FileName,
                     MOUNT_TEST_LOG);

            while (TRUE) {
                Result = system(MountCommand);
                if (WIFSIGNALED(Result) &&
                    ((WTERMSIG(Result) == SIGINT) ||
                     (WTERMSIG(Result) == SIGQUIT))) {

                    goto RunMountConcurrencyTestEnd;
                }

                if (Result != 0) {
                    if ((Result != EINVAL) && (Result != ENOFILE)) {
                        PRINT_ERROR("Failed to unmount %s: %s.\n",
                                    FileName,
                                    Result,
                                    strerror(errno));

                        Failures += 1;
                    }

                    break;
                }
            }

            Result = MountTestDeleteFile(FileName, FALSE);
            if ((Result != 0) && (errno != ENOFILE)) {
                PRINT_ERROR("Failed to delete %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
            }
        }
    }

    PRINT("\n");

RunMountConcurrencyTestEnd:
    return Failures;
}

ULONG
RunMountGenericTest (
    MOUNT_TEST_TYPE TestType,
    INT MountCount,
    INT Iterations
    )

/*++

Routine Description:

    This routine executes a generic non-concurrent mount test.

Arguments:

    TestType - Supplies the type of generic test being executed.

    MountCount - Supplies the number of mountable items to work with.

    Iterations - Supplies the number of iterations to perform.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    MOUNT_TEST_ACTION Action;
    ULONG Failures;
    PINT FileId;
    INT FileIndex;
    BOOL FileIsDirectory;
    CHAR FileName[16];
    struct stat FileStat;
    INT Iteration;
    INT MaxSimultaneousMounts;
    CHAR MountCommand[64];
    PINT MountCounts;
    INT Percent;
    pid_t Process;
    INT Result;
    INT SimultaneousMounts;
    INT TargetIndex;
    CHAR TargetName[16];

    Failures = 0;
    FileId = NULL;
    MountCounts = NULL;

    //
    // Announce the test.
    //

    Process = getpid();
    PRINT("Process %d Running mount %s test with %d mount points, "
          "%d iterations.\n",
          Process,
          MountTestName[TestType],
          MountCount,
          Iterations);

    Percent = Iterations / 100;
    if (Percent == 0) {
        Percent = 1;
    }

    if (TestType == MountTestFile) {
        FileIsDirectory = FALSE;

    } else {

        assert(TestType == MountTestDirectory);

        FileIsDirectory = TRUE;
    }

    MaxSimultaneousMounts = 0;
    SimultaneousMounts = 0;
    FileId = malloc(MountCount * sizeof(INT));
    if (FileId == NULL) {
        Failures += 1;
        goto RunMountGenericTestEnd;
    }

    for (FileIndex = 0; FileIndex < MountCount; FileIndex += 1) {
        FileId[FileIndex] = -1;
    }

    MountCounts = malloc(MountCount * sizeof(INT));
    if (MountCounts == NULL) {
        Failures += 1;
        goto RunMountGenericTestEnd;
    }

    memset(MountCounts, 0, MountCount * sizeof(INT));

    //
    // Perform the mount operations. This test creates files and then mounts
    // them, unmounts them, or checks their file ID.
    //

    for (Iteration = 0; Iteration < Iterations; Iteration += 1) {

        //
        // Pick a random file and a random action.
        //

        FileIndex = rand() % MountCount;
        snprintf(FileName,
                 sizeof(FileName),
                 MountTestFileNameFormat[TestType],
                 Process & 0xFFFF,
                 FileIndex);

        Action = rand() % MountTestActionCount;

        //
        // If the file has yet to be created, then the action must be a stat.
        //

        if (FileId[FileIndex] == -1) {
            Action = MountTestActionStat;

        //
        // If the file has yet to be mounted, do not unmount it.
        //

        } else if ((Action == MountTestActionUnmount) &&
                   (MountCounts[FileIndex] == 0)) {

            Action = MountTestActionMount;
        }

        switch (Action) {
        case MountTestActionStat:
            if (FileId[FileIndex] == -1) {
                Result = MountTestCreateFile(FileName, FileIsDirectory);
                if (Result < 0) {
                    Failures += 1;
                    continue;
                }
            }

            Result = stat(FileName, &FileStat);
            if (Result < 0) {
                PRINT_ERROR("Failed to stat file %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            if (FileId[FileIndex] == -1) {
                FileId[FileIndex] = FileStat.st_ino;
                DEBUG_PRINT("Set file %s ID to %d.\n",
                            FileName,
                            FileStat.st_ino);

            } else if (FileId[FileIndex] != FileStat.st_ino) {
                PRINT_ERROR("Failed to match file ID for file %s. Expected %d "
                            "but read %d.\n",
                            FileName,
                            FileId[FileIndex],
                            FileStat.st_ino);

                 Failures += 1;
                 continue;

            } else {
                DEBUG_PRINT("File %s ID is %d, as expected.\n",
                            FileName,
                            FileStat.st_ino);
            }

            break;

        case MountTestActionMount:

            //
            // Pick another random file to mount on.
            //

            TargetIndex = rand() % MountCount;
            snprintf(TargetName,
                     sizeof(TargetName),
                     MountTestFileNameFormat[TestType],
                     Process & 0xFFFF,
                     TargetIndex);

            if (FileId[TargetIndex] == -1) {
                Result = MountTestCreateFile(TargetName, FileIsDirectory);
                if (Result < 0) {
                    Failures += 1;
                    continue;
                }

                Result = stat(TargetName, &FileStat);
                if (Result < 0) {
                    PRINT_ERROR("Failed to stat file %s: %s.\n",
                                TargetName,
                                strerror(errno));

                    Failures += 1;
                    continue;
                }

                FileId[TargetIndex] = FileStat.st_ino;
            }

            DEBUG_PRINT("Mount file %s onto file %s\n", FileName, TargetName);
            snprintf(MountCommand,
                     sizeof(MountCommand),
                     MOUNT_TEST_MOUNT_FORMAT,
                     FileName,
                     TargetName,
                     MOUNT_TEST_LOG);

            Result = system(MountCommand);
            if (WIFSIGNALED(Result) &&
                ((WTERMSIG(Result) == SIGINT) ||
                 (WTERMSIG(Result) == SIGQUIT))) {

                goto RunMountGenericTestEnd;
            }

            if (Result != 0) {
                PRINT_ERROR("Failed to mount %s onto %s: %s.\n",
                            FileName,
                            TargetName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            SimultaneousMounts += 1;
            if (SimultaneousMounts > MaxSimultaneousMounts) {
                MaxSimultaneousMounts = SimultaneousMounts;
            }

            //
            // Update the targets mount count and file ID information.
            //

            MountCounts[TargetIndex] += 1;
            FileId[TargetIndex] = FileId[FileIndex];
            DEBUG_PRINT("Set file %s ID to %d.\n",
                        TargetName,
                        FileId[TargetIndex]);

            break;

        case MountTestActionUnmount:
            DEBUG_PRINT("Unmounting file %s\n", FileName);
            snprintf(MountCommand,
                     sizeof(MountCommand),
                     MOUNT_TEST_UNMOUNT_FORMAT,
                     FileName,
                     MOUNT_TEST_LOG);

            Result = system(MountCommand);
            if (WIFSIGNALED(Result) &&
                ((WTERMSIG(Result) == SIGINT) ||
                 (WTERMSIG(Result) == SIGQUIT))) {

                goto RunMountGenericTestEnd;
            }

            if (Result != 0) {
                if (MountCounts[FileIndex] != 0) {
                    PRINT_ERROR("Failed to unmount %s: %s.\n",
                                FileName,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            //
            // Recollect the stat information now that a file has been
            // unmounted at this path.
            //

            Result = stat(FileName, &FileStat);
            if (Result < 0) {
                PRINT_ERROR("Failed to stat file %s: %s.\n",
                            FileName,
                            strerror(errno));

                Failures += 1;
                continue;
            }

            FileId[FileIndex] = FileStat.st_ino;
            DEBUG_PRINT("Set file %s ID to %d.\n",
                        FileName,
                        FileId[FileIndex]);

            MountCounts[FileIndex] -= 1;
            SimultaneousMounts -= 1;
            break;

        case MountTestActionDelete:
            DEBUG_PRINT("Deleting file %s\n", FileName);
            Result = MountTestDeleteFile(FileName, FileIsDirectory);
            if (Result != 0) {
                if ((MountCounts[FileIndex] == 0) || (errno != EBUSY)) {
                    PRINT_ERROR("Failed to delete %s: %s.\n",
                                FileName,
                                strerror(errno));

                    Failures += 1;
                }

                continue;
            }

            assert(MountCounts[FileIndex] == 0);

            FileId[FileIndex] = -1;
            break;

        default:

            assert(FALSE);

            break;
        }

        if ((Iteration % Percent) == 0) {
            PRINT("%s", MountTestProgressCharacter[TestType]);
        }
    }

    //
    // Clean up all files.
    //

    if (MountTestNoCleanup == FALSE) {
        for (FileIndex = 0; FileIndex < MountCount; FileIndex += 1) {
            if (FileId[FileIndex] != -1) {
                snprintf(FileName,
                         sizeof(FileName),
                         MountTestFileNameFormat[TestType],
                         Process & 0xFFFF,
                         FileIndex);

            while (MountCounts[FileIndex] != 0) {
                snprintf(MountCommand,
                         sizeof(MountCommand),
                         MOUNT_TEST_UNMOUNT_LAZY_FORMAT,
                         FileName,
                         MOUNT_TEST_LOG);

                Result = system(MountCommand);
                if (WIFSIGNALED(Result) &&
                    ((WTERMSIG(Result) == SIGINT) ||
                     (WTERMSIG(Result) == SIGQUIT))) {

                    goto RunMountGenericTestEnd;
                }

                if (Result != 0) {
                    PRINT_ERROR("Failed to unmount %s: %s.\n",
                                FileName,
                                strerror(errno));

                    Failures += 1;
                    break;
                }

                MountCounts[FileIndex] -= 1;
            }

            if (MountCounts[FileIndex] == 0) {
                Result = MountTestDeleteFile(FileName, FileIsDirectory);
                if (Result != 0) {
                    PRINT_ERROR("Failed to delete %s: %s.\n",
                                FileName,
                                strerror(errno));

                    Failures += 1;
                }
            }
            }
        }
    }

    PRINT("\nMax usage: %d mounts.\n", MaxSimultaneousMounts);

RunMountGenericTestEnd:
    if (FileId != NULL) {
        free(FileId);
    }

    if (MountCounts != NULL) {
        free(MountCounts);
    }

    return Failures;
}

INT
MountTestCreateFile (
    PSTR FileName,
    BOOL FileIsDirectory
    )

/*++

Routine Description:

    This routine creates a file with the given name.

Arguments:

    FileName - Supplies a string representing the desired file name.

    FileIsDirectory - Supplies a boolean indicating if the file is a directory.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    INT File;
    INT OpenFlags;
    INT Result;

    if (FileIsDirectory == FALSE) {
        OpenFlags = O_RDWR | O_CREAT;
        Result = open(FileName, OpenFlags, MOUNT_TEST_CREATE_PERMISSIONS);
        if (Result < 0) {
            PRINT_ERROR("Failed to create file %s (flags %x): %s.\n",
                        FileName,
                        OpenFlags,
                        strerror(errno));

            goto CreateFileEnd;
        }

        File = Result;
        Result = close(File);
        if (Result != 0) {
            PRINT_ERROR("Failed to close: %s.\n", strerror(errno));
            goto CreateFileEnd;
        }

    } else {
        Result = mkdir(FileName, MOUNT_TEST_CREATE_PERMISSIONS);
        if (Result < 0) {
            PRINT_ERROR("Failed to create directory %s: %s.\n",
                        FileName,
                        strerror(errno));

            goto CreateFileEnd;
        }
    }

CreateFileEnd:
    return Result;
}

INT
MountTestDeleteFile (
    PSTR FileName,
    BOOL FileIsDirectory
    )

/*++

Routine Description:

    This routine delets a file with the given name.

Arguments:

    FileName - Supplies a string representing the desired file name.

    FileIsDirectory - Supplies a boolean indicating if the file is a directory.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    INT Result;

    if (FileIsDirectory == FALSE) {
        Result = unlink(FileName);

    } else {
        Result = rmdir(FileName);
    }

    return Result;
}

PSTR
AppendPaths (
    PSTR Path1,
    PSTR Path2
    )

/*++

Routine Description:

    This routine creates a concatenated string of "Path1/Path2".

Arguments:

    Path1 - Supplies a pointer to the prefix of the combined path.

    Path2 - Supplies a pointer to the path to append.

Return Value:

    Returns a pointer to the appended path on success. The caller is
    responsible for freeing this memory.

    NULL on failure.

--*/

{

    PSTR AppendedPath;
    size_t Offset;
    size_t Path1Length;
    size_t Path2Length;
    BOOL SlashNeeded;

    assert((Path1 != NULL) && (Path2 != NULL));

    Path1Length = strlen(Path1);
    Path2Length = strlen(Path2);
    SlashNeeded = TRUE;
    if ((Path1Length == 0) || (Path1[Path1Length - 1] == '/') ||
        (Path1[Path1Length - 1] == '\\')) {

        SlashNeeded = FALSE;
    }

    AppendedPath = malloc(Path1Length + Path2Length + 2);
    if (AppendedPath == NULL) {
        return NULL;
    }

    Offset = 0;
    if (Path1Length != 0) {
        memcpy(AppendedPath, Path1, Path1Length);
        Offset += Path1Length;
    }

    if (SlashNeeded != FALSE) {
        AppendedPath[Offset] = '/';
        Offset += 1;
    }

    strcpy(AppendedPath + Offset, Path2);
    return AppendedPath;
}

