/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pathtest.c

Abstract:

    This module implements the tests used to verify that system's paths are
    functioning properly.

Author:

    Chris Stevens 17-Sept-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

//
// --------------------------------------------------------------------- Macros
//

#define PATHTEST_DEBUG_PRINT(...)      \
    if (PathTestVerbose != false) {    \
        printf(__VA_ARGS__);           \
    }

#define PATHTEST_ERROR(...) printf(__VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
RunAllPathTests (
    void
    );

int
RunSerialDirectoryTests (
    void
    );

int
RunParallelDirectoryTests (
    int Index
    );

int
RunHardLinkTests (
    void
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to enable more verbose debug output.
//

bool PathTestVerbose = false;

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

    This routine implements the path test program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    if ((ArgumentCount == 2) && (strcmp(Arguments[1], "-v") == 0)) {
        PathTestVerbose = true;
    }

    return RunAllPathTests();
}

//
// --------------------------------------------------------- Internal Functions
//

int
RunAllPathTests (
    void
    )

/*++

Routine Description:

    This routine executes all path tests.

Arguments:

    None.

Return Value:

    Returns the number of failures in the test suite.

--*/

{

    int Failures;
    int Index;

    Failures = 0;

    //
    // Run the serial tests a few times.
    //

    PATHTEST_DEBUG_PRINT("Start Serial Tests\n");
    for (Index = 0; Index < 5; Index += 1) {
        Failures += RunSerialDirectoryTests();
    }

    PATHTEST_DEBUG_PRINT("End Serial Tests\n");

    //
    // Run the parallel tests a few times.
    //

    PATHTEST_DEBUG_PRINT("Start Parallel Tests\n");
    for (Index = 0; Index < 6; Index += 1) {
        Failures += RunParallelDirectoryTests(Index);
    }

    PATHTEST_DEBUG_PRINT("End Parallel Tests\n");

    //
    // Run the hard link tests a few times.
    //

    PATHTEST_DEBUG_PRINT("Start Hard Link Tests\n");
    for (Index = 0; Index < 5; Index += 1) {
        Failures += RunHardLinkTests();
    }

    PATHTEST_DEBUG_PRINT("End Hard Link Tests\n");

    //
    // Display the test pass state.
    //

    if (Failures != 0) {
        PATHTEST_ERROR("*** %d failures in path tests. ***\n", Failures);

    } else {
        printf("All path tests pass.\n");
    }

    return Failures;
}

int
RunSerialDirectoryTests (
    void
    )

/*++

Routine Description:

    This routine runs tests on directories serially.

Arguments:

    None.

Return Value:

    Returns the number of failures in the test.

--*/

{

    int Failures;
    int Result;

    //
    // Make a directory and remove it. It will be a child of the current
    // directory.
    //

    Failures = 0;
    Result = mkdir("pathtest1", S_IRWXU | S_IRWXG | S_IRWXO);
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to create directory pathtest1!\n");
        goto SerialDirectoryTestsEnd;
    }

    Result = rmdir("pathtest1");
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to remove directory pathtest1!\n");
        goto SerialDirectoryTestsEnd;
    }

    //
    // Great, it cleaned up well. Create it again.
    //

    Result = mkdir("pathtest1", S_IRWXU | S_IRWXG | S_IRWXO);
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to create directory pathtest1!\n");
        goto SerialDirectoryTestsEnd;
    }

    //
    // Now make a child directory.
    //

    Result = mkdir("pathtest1/pathtest2", S_IRWXU | S_IRWXG | S_IRWXO);
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to create directory pathtest1/pathtest2!\n");
        goto SerialDirectoryTestsEnd;
    }

    //
    // Change directories into it and back.
    //

    Result = chdir("pathtest1/pathtest2");
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to change directory to pathtest1/pathtest2\n");
        goto SerialDirectoryTestsEnd;
    }

    Result = chdir("../..");
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to change directory to ../..\n");
        goto SerialDirectoryTestsEnd;
    }

    //
    // Now try to remove the parent directory. This should fail.
    //

    Result = rmdir("pathtest1");
    if (Result == 0) {
        Failures += 1;
        PATHTEST_ERROR("Succeeded to remove directory pathtest1, expected to "
                       "fail with status ENOTEMPTY!\n");

        goto SerialDirectoryTestsEnd;
    }

    if (errno != ENOTEMPTY) {
        Failures += 1;
        PATHTEST_ERROR("'rmdir' failed with incorrect status. Expected %d, "
                       "received %d.\n", ENOTEMPTY, errno);

        goto SerialDirectoryTestsEnd;
    }

    //
    // Now remove the child directory.
    //

    Result = rmdir("pathtest1/pathtest2");
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to remove directory pathtest1/pathtest2!\n");
        goto SerialDirectoryTestsEnd;
    }

    //
    // Now try to change directories into pathtest1\pathtest2.
    //

    Result = chdir("pathtest1/pathtest2");
    if (Result == 0) {
        Failures += 1;
        PATHTEST_ERROR("Successfully changed directories to "
                       "pathtest1/pathtest2. Expected to fail.\n");

        goto SerialDirectoryTestsEnd;
    }

    if (errno != ENOENT) {
        Failures += 1;
        PATHTEST_ERROR("Failed to change directories to pathtest1/pathtest2. "
                       "Failure expected %d, received %d.\n",
                       ENOENT,
                       errno);

        goto SerialDirectoryTestsEnd;
    }

    //
    // Try to create a directory inside of the removed directory.
    //

    Result = mkdir("pathtest1/pathtest2/pathtest3",
                   S_IRWXU | S_IRWXG | S_IRWXO);

    if (Result == 0) {
        Failures += 1;
        PATHTEST_ERROR("Successfully created directory "
                       "pathtest1/pathtest2/pathtest3. Expected to fail.\n");

        goto SerialDirectoryTestsEnd;
    }

    if (errno != ENOENT) {
        Failures += 1;
        PATHTEST_ERROR("Failed to make directory to "
                       "pathtest1/pathtest2/pathtest3. Failure expected %d, "
                       "received %d.\n",
                       ENOENT,
                       errno);

        goto SerialDirectoryTestsEnd;
    }

    //
    // Ok. Now clean it up.
    //

    Result = rmdir("pathtest1");
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to remove directory pathtest1!\n");
        goto SerialDirectoryTestsEnd;
    }

SerialDirectoryTestsEnd:
    return Failures;
}

int
RunParallelDirectoryTests (
    int Index
    )

/*++

Routine Description:

    This routine runs tests on directories in parallel.

Arguments:

    Index - Supplies a value indicating which iteration of the parallel test
        this is.

Return Value:

    Returns the number of failures in the test.

--*/

{

    pid_t Child;
    int ChildResult;
    int Failures;
    char *OriginalDirectory;
    int Result;
    int Status;
    pid_t WaitPid;

    Failures = 0;

    //
    // Save the original directory.
    //

    OriginalDirectory = getcwd(NULL, 0);
    if (OriginalDirectory == NULL) {
        Failures += 1;
        PATHTEST_ERROR("Failed to get original directory.\n");
        goto ParallelDirectoryTestsEnd;
    }

    //
    // Test adding a child to a directory while it is in the middle of being
    // removed. First create the directory.
    //

    Result = mkdir("pathtest1", S_IRWXU | S_IRWXG | S_IRWXO);
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to create directory pathtest1 with error %d.\n",
                       errno);

        goto ParallelDirectoryTestsEnd;
    }

    Child = fork();
    if (Child == -1) {
        Failures += 1;
        PATHTEST_ERROR("Failed to create child process.\n");
        goto ParallelDirectoryTestsEnd;
    }

    //
    // If this is the child process, try to create a directory if the index is
    // even and try to remove the directory if the index is odd.
    //

    if (Child == 0) {
        if ((Index % 2) == 0) {
            Result = mkdir("pathtest1/pathtest2", S_IRWXU | S_IRWXG | S_IRWXO);
            if (Result == 0) {
                PATHTEST_DEBUG_PRINT("Child created pathtest1/pathtest2.\n");

            } else {
                PATHTEST_DEBUG_PRINT("Child failed to create "
                                     "pathtest1/pathtest2 with error %d.\n",
                                     errno);

                if (errno != ENOENT) {
                    Result = 1;
                    PATHTEST_ERROR("Child failed to create pathtest1/pathtest2 "
                                   "with error %d, expected error %d.\n",
                                   errno,
                                   ENOENT);
                }
            }

        } else {
            Result = rmdir("pathtest1");
            if (Result == 0) {
                PATHTEST_DEBUG_PRINT("Child removed pathtest1.\n");

            } else {
                PATHTEST_DEBUG_PRINT("Child failed to remove pathtest1 with "
                                     "error %d.\n",
                                     errno);

                if (errno != ENOTEMPTY) {
                    Result = 1;
                    PATHTEST_ERROR("Child failed to remove pathtest1 with "
                                   "error %d, expected error %d.\n",
                                   errno,
                                   ENOTEMPTY);
                }
            }
        }

        PATHTEST_DEBUG_PRINT("Child %d exiting with status %d.\n",
                             getpid(),
                             Result);

        exit(Result);

    //
    // If this is the parent process, try to remove the directory if the index
    // is even and try to create a new directory if the index is odd.
    //

    } else {
        if ((Index % 2) == 0) {
            Result = rmdir("pathtest1");
            if (Result == 0) {
                PATHTEST_DEBUG_PRINT("Parent removed pathtest1.\n");

            } else {
                PATHTEST_DEBUG_PRINT("Parent failed to remove pathtest1 with "
                                     "error %d.\n",
                                     errno);

                if (errno != ENOTEMPTY) {
                    Result = 1;
                    PATHTEST_ERROR("Parent failed to remove pathtest1 with "
                                   "error %d, expected error %d.\n",
                                   errno,
                                   ENOTEMPTY);
                }
            }

        } else {
            Result = mkdir("pathtest1/pathtest2", S_IRWXU | S_IRWXG | S_IRWXO);
            if (Result == 0) {
                PATHTEST_DEBUG_PRINT("Parent created pathtest1/pathtest2.\n");

            } else {
                PATHTEST_DEBUG_PRINT("Parent failed to create "
                                     "pathtest1/pathtest2 with error %d.\n",
                                     errno);

                if (errno != ENOENT) {
                    Result = 1;
                    PATHTEST_ERROR("Parent failed to create "
                                   "pathtest1/pathtest2 with error %d, "
                                   "expected error %d.\n",
                                   errno,
                                   ENOENT);
                }
            }
        }

        //
        // Wait for the child to exit.
        //

        WaitPid = waitpid(Child, &Status, WUNTRACED | WCONTINUED);
        if (WaitPid != Child) {
            Failures += 1;
            PATHTEST_ERROR("waitpid returned %d instead of child pid %d.\n",
                           WaitPid,
                           Child);
        }

        //
        // Check the flags and return value.
        //

        if ((!WIFEXITED(Status)) ||
            (WIFCONTINUED(Status)) ||
            (WIFSIGNALED(Status)) ||
            (WIFSTOPPED(Status))) {

            Failures += 1;
            PATHTEST_ERROR("Child status was not exited as expected. Was %x\n",
                           Status);
        }

        //
        // The child exit status is the result of it's attempt to create a
        // directory. Make sure the child's experience matches the parent's.
        //

        ChildResult = WEXITSTATUS(Status);

        //
        // Both should not have succeeded.
        //

        if ((ChildResult == 0) && (Result == 0)) {
            Failures += 1;
            PATHTEST_ERROR("Both parent and child succeeded. One of them "
                           "should have failed.\n");

        //
        // If the one succeeded and the other did not fail with the appropriate
        // error (-1), then something went wrong.
        //

        } else if ((ChildResult == 0) && (Result != -1)) {
            Failures += 1;
            PATHTEST_ERROR("Child succeeded, but parent failed with unexpected "
                           "error.\n");

        } else if ((Result == 0) &&
                   ((unsigned char)ChildResult != (unsigned char)-1)) {

            Failures += 1;
            PATHTEST_ERROR("Parent succeeded, but child failed with unexpected "
                           "error.\n");
        }
    }

ParallelDirectoryTestsEnd:

    //
    // Change back to the original directory and try to remove the created
    // directory.
    //

    if (OriginalDirectory != NULL) {
        Result = chdir(OriginalDirectory);
        if (Result != 0) {
            Failures += 1;
            PATHTEST_ERROR("Failed to 'cd' to %s.\n", OriginalDirectory);

        } else {
            rmdir("pathtest1/pathtest2");
            rmdir("pathtest1");
        }

        free(OriginalDirectory);
    }

    return Failures;
}

int
RunHardLinkTests (
    void
    )

/*++

Routine Description:

    This routine runs tests on directory hard links.

Arguments:

    None.

Return Value:

    Returns the number of failures in the test.

--*/

{

    pid_t Child;
    int ChildResult;
    int Failures;
    char *OriginalDirectory;
    int Result;
    struct stat Stat;
    int Status;
    pid_t WaitPid;

    Failures = 0;

    //
    // Save the original directory.
    //

    OriginalDirectory = getcwd(NULL, 0);
    if (OriginalDirectory == NULL) {
        Failures += 1;
        PATHTEST_ERROR("Failed to get original directory.\n");
        goto HardLinkTestsEnd;
    }

    //
    // Test stating a directory after it has been removed. First create the
    // directory and 'cd' into it.
    //

    Result = mkdir("pathtest1", S_IRWXU | S_IRWXG | S_IRWXO);
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to create directory pathtest1 with error %d.\n",
                       errno);

        goto HardLinkTestsEnd;
    }

    Result = chdir("pathtest1");
    if (Result != 0) {
        Failures += 1;
        PATHTEST_ERROR("Failed to change directories to pathtest1 with error "
                       "%d.\n",
                       errno);

        goto HardLinkTestsEnd;
    }

    Child = fork();
    if (Child == -1) {
        Failures += 1;
        PATHTEST_ERROR("Failed to create child process.\n");
        goto HardLinkTestsEnd;
    }

    //
    // If this is the child process, 'cd' out of the directory and then
    // remove it.
    //

    if (Child == 0) {
        Result = chdir("..");
        if (Result != 0) {
            PATHTEST_ERROR("Child failed to change directories to .. with "
                           "error %d.\n",
                           errno);

            exit(errno);
        }

        //
        // Try to remove the directory.
        //

        Result = rmdir("pathtest1");
        if (Result != 0) {
            PATHTEST_ERROR("Child failed to remove pathtest1 with error %d.\n",
                           errno);

            exit(errno);
        }

        PATHTEST_DEBUG_PRINT("Child %d exiting with status %d.\n",
                             getpid(),
                             Result);

        exit(Result);

    //
    // If this is the parent process, wait for the child to exit and then make
    // sure that the current directory is 'pathtest1' and stat the directory to
    // check to make sure it has a hard link count of 0.
    //

    } else {

        //
        // Wait for the child to exit.
        //

        WaitPid = waitpid(Child, &Status, WUNTRACED | WCONTINUED);
        if (WaitPid != Child) {
            PATHTEST_ERROR("waitpid returned %d instead of child pid %d.\n",
                           WaitPid,
                           Child);

            Failures += 1;
        }

        //
        // Check the flags and return value.
        //

        if ((!WIFEXITED(Status)) ||
            (WIFCONTINUED(Status)) ||
            (WIFSIGNALED(Status)) ||
            (WIFSTOPPED(Status))) {

            Failures += 1;
            PATHTEST_ERROR("Child status was not exited as expected. Was %x\n",
                           Status);
        }

        //
        // The child exit status is the result of it's attempt to create a
        // directory. Make sure the child's experience matches the parent's.
        //

        ChildResult = WEXITSTATUS(Status);
        if (ChildResult != 0) {
            Failures += 1;
            PATHTEST_ERROR("Child did not exit with expected status. Expected "
                           "0, received %d.\n",
                           ChildResult);

            goto HardLinkTestsEnd;
        }

        //
        // Validate the current working directory is not accessible via
        // 'getcwd' which performs a reverse path walk.
        //

        if (creat("myfile", 0777) == 0) {
            Failures += 1;
            PATHTEST_ERROR("Succeeded in creating a file in a deleted "
                           "directory.\n");
        }

        //
        // Now stat the current directory.
        //

        Result = stat(".", &Stat);
        if (Result != 0) {
            Failures += 1;
            PATHTEST_ERROR("Failed to stat current directory in parent with "
                           "error %d.\n",
                           errno);

            goto HardLinkTestsEnd;
        }

        //
        // Make sure the hard link count is 0.
        //

        if (Stat.st_nlink != 0) {
            Failures += 1;
            PATHTEST_ERROR("Unexpected hard link count for directory "
                           "pathtest1. Expected 0, but it has %lu hard "
                           "links.\n",
                           Stat.st_nlink);

            goto HardLinkTestsEnd;
        }
    }

HardLinkTestsEnd:

    //
    // Change back to the original directory and try to remove the created
    // directory.
    //

    if (OriginalDirectory != NULL) {
        Result = chdir(OriginalDirectory);
        if (Result != 0) {
            Failures += 1;
            PATHTEST_ERROR("Failed to 'cd' to %s.\n", OriginalDirectory);

        } else {
            rmdir("pathtest1");
        }

        free(OriginalDirectory);
    }

    return Failures;
}

