/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ktest.c

Abstract:

    This module implements the kernel test application.

Author:

    Evan Green 5-Nov-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <minoca/lib/mlibc.h>

#include "ktestdrv.h"

//
// --------------------------------------------------------------------- Macros
//

#define DEBUG_PRINT(...)                                \
    if (KTestVerbosity >= TestVerbosityDebug) {      \
        printf(__VA_ARGS__);                            \
    }

#define PRINT(...)                                      \
    if (KTestVerbosity >= TestVerbosityNormal) {     \
        printf(__VA_ARGS__);                            \
    }

#define PRINT_ERROR(...) fprintf(stderr, "\nktest: " __VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

#define KTEST_VERSION_MAJOR 1
#define KTEST_VERSION_MINOR 0

#define KTEST_USAGE                                                            \
    "Usage: ktest [options] \n"                                                \
    "This utility hammers on various subsystems in the kernel. Options are:\n" \
    "  -A <value>, -- Set the first test-specific parameter. -B sets the \n"   \
    "      second, -C the third, etc.\n"                                       \
    "  -i, --iterations <count> -- Set the number of operations to perform.\n" \
    "  -p, --threads <count> -- Set the number of threads to spin up.\n"       \
    "  -t, --test -- Set the test to perform. Valid values are all, \n"        \
    "      pagedpoolstress, nonpagedpoolstress, workstress, threadstress, \n"  \
    "      descriptorstress, pagedblockstress and nonpagedblockstress.\n"      \
    "  --debug -- Print lots of information about what's happening.\n"         \
    "  --quiet -- Print only errors.\n"                                        \
    "  --no-cleanup -- Leave test files around for debugging.\n"               \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the test version and exit.\n"                        \

#define KTEST_OPTIONS_STRING "A:B:C:D:i:p:t:dqVh"

#define KTEST_DRIVER_NAME "ktestdrv.drv"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TEST_VERBOSITY {
    TestVerbosityQuiet,
    TestVerbosityNormal,
    TestVerbosityDebug
} TEST_VERBOSITY, *PTEST_VERBOSITY;

typedef struct _KTEST_PROGRESS {
    INT PreviousPercent;
    INT Handle;
    CHAR Character;
    KTEST_TYPE Test;
} KTEST_PROGRESS, *PKTEST_PROGRESS;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KTestLoadDriver (
    VOID
    );

KSTATUS
KTestOpenDriver (
    PINT DriverHandle
    );

VOID
KTestCloseDriver (
    INT DriverHandle
    );

INT
KTestSendStartRequest (
    INT DriverHandle,
    KTEST_TYPE Test,
    PKTEST_START_TEST Request,
    PINT HandleCount
    );

void
KTestSignalHandler (
    int Signal
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Higher levels here print out more stuff.
//

TEST_VERBOSITY KTestVerbosity = TestVerbosityNormal;

struct option KTestLongOptions[] = {
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
// Store the handle array.
//

KTEST_PROGRESS KTestProgress[KTestCount];

//
// Store the friendly names of the tests.
//

PSTR KTestNames[KTestCount] = {
    "all",
    "pagedpoolstress",
    "nonpagedpoolstress",
    "workstress",
    "threadstress",
    "descriptorstress",
    "pagedblockstress",
    "nonpagedblockstress",
};

//
// Store the UUID for querying test device information.
//

UUID KTestTestDeviceInformationUuid = TEST_DEVICE_INFORMATION_UUID;

//
// Stores a boolean indicating if a cancellation request was received.
//

BOOL KTestCancelAllTests = FALSE;

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

    This routine implements the kernel test program.

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
    BOOL AllDone;
    BOOL AnyProgress;
    KTEST_CANCEL_TEST Cancel;
    INT DriverHandle;
    INT Failures;
    INT HandleCount;
    INT HandleIndex;
    INT Option;
    struct sigaction OriginalSigIntAction;
    ULONGLONG Percent;
    KTEST_POLL Poll;
    struct sigaction SigIntAction;
    KTEST_START_TEST Start;
    INT Status;
    KSTATUS StatusCode;
    KTEST_TYPE Test;
    ULONG TestIndex;
    PSTR TestName;

    DriverHandle = -1;
    Failures = 0;
    HandleCount = 0;
    Test = KTestAll;
    Status = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    srand(time(NULL));
    memset(&Start, 0, sizeof(KTEST_START_TEST));
    for (HandleIndex = 0; HandleIndex < KTestCount; HandleIndex += 1) {
        KTestProgress[HandleIndex].Handle = -1;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             KTEST_OPTIONS_STRING,
                             KTestLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'A':
        case 'B':
        case 'C':
        case 'D':
            Start.Parameters.Parameters[Option - 'A'] =
                                                 strtol(optarg, &AfterScan, 0);

            if (AfterScan == optarg) {
                PRINT_ERROR("Invalid parameter: %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'i':
            Start.Parameters.Iterations = strtol(optarg, &AfterScan, 0);
            if ((Start.Parameters.Iterations < 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid iteration count %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'p':
            Start.Parameters.Threads = strtol(optarg, &AfterScan, 0);
            if ((Start.Parameters.Threads <= 0) || (AfterScan == optarg)) {
                PRINT_ERROR("Invalid thread count %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 't':
            for (TestIndex = 0; TestIndex < KTestCount; TestIndex += 1) {
                if (strcasecmp(optarg, KTestNames[TestIndex]) == 0) {
                    Test = TestIndex;
                    break;
                }
            }

            if (TestIndex == KTestCount) {
                PRINT_ERROR("Invalid test: %s.\n", optarg);
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'd':
            KTestVerbosity = TestVerbosityDebug;
            break;

        case 'q':
            KTestVerbosity = TestVerbosityQuiet;
            break;

        case 'V':
            printf("Minoca kernel test version %d.%d\n",
                   KTEST_VERSION_MAJOR,
                   KTEST_VERSION_MINOR);

            return 1;

        case 'h':
            printf(KTEST_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // Fire up the driver.
    //

    StatusCode = KTestLoadDriver();
    if (!KSUCCESS(StatusCode)) {
        PRINT_ERROR("Error: Failed to load driver: %d.\n", StatusCode);
        Status = ClConvertKstatusToErrorNumber(StatusCode);
        goto MainEnd;
    }

    //
    // Give the device time to enumerate.
    //

    sleep(2);

    //
    // Open a connection to the driver.
    //

    StatusCode = KTestOpenDriver(&DriverHandle);
    if (!KSUCCESS(StatusCode)) {
        PRINT_ERROR("Error: Failed to open driver: %d.\n", StatusCode);
        Status = ClConvertKstatusToErrorNumber(StatusCode);
        goto MainEnd;
    }

    //
    // Run the tests.
    //

    HandleCount = 0;
    if ((Test == KTestAll) || (Test == KTestPagedPoolStress)) {
        Status = KTestSendStartRequest(DriverHandle,
                                       KTestPagedPoolStress,
                                       &Start,
                                       &HandleCount);

        if (Status != 0) {
            PRINT_ERROR("Failed to send start request.\n");
            Failures += 1;
        }
    }

    if ((Test == KTestAll) || (Test == KTestNonPagedPoolStress)) {
        Status = KTestSendStartRequest(DriverHandle,
                                       KTestNonPagedPoolStress,
                                       &Start,
                                       &HandleCount);

        if (Status != 0) {
            PRINT_ERROR("Failed to send start request.\n");
            Failures += 1;
        }
    }

    if ((Test == KTestAll) || (Test == KTestWorkStress)) {
        Status = KTestSendStartRequest(DriverHandle,
                                       KTestWorkStress,
                                       &Start,
                                       &HandleCount);

        if (Status != 0) {
            PRINT_ERROR("Failed to send start request.\n");
            Failures += 1;
        }
    }

    if ((Test == KTestAll) || (Test == KTestThreadStress)) {
        Status = KTestSendStartRequest(DriverHandle,
                                       KTestThreadStress,
                                       &Start,
                                       &HandleCount);

        if (Status != 0) {
            PRINT_ERROR("Failed to send start request.\n");
            Failures += 1;
        }
    }

    if ((Test == KTestAll) || (Test == KTestDescriptorStress)) {
        Status = KTestSendStartRequest(DriverHandle,
                                       KTestDescriptorStress,
                                       &Start,
                                       &HandleCount);

        if (Status != 0) {
            PRINT_ERROR("Failed to send start request.\n");
            Failures += 1;
        }
    }

    if ((Test == KTestAll) || (Test == KTestPagedBlockStress)) {
        Status = KTestSendStartRequest(DriverHandle,
                                       KTestPagedBlockStress,
                                       &Start,
                                       &HandleCount);

        if (Status != 0) {
            PRINT_ERROR("Failed to send start request.\n");
            Failures += 1;
        }
    }

    if ((Test == KTestAll) || (Test == KTestNonPagedBlockStress)) {
        Status = KTestSendStartRequest(DriverHandle,
                                       KTestNonPagedBlockStress,
                                       &Start,
                                       &HandleCount);

        if (Status != 0) {
            PRINT_ERROR("Failed to send start request.\n");
            Failures += 1;
        }
    }

    //
    // Poll the tests until they are all complete.
    //

    if (HandleCount == 0) {
        PRINT_ERROR("Error: No tests were started.\n");
        Failures += 1;
        goto MainEnd;
    }

    //
    // Handle cancellation signals.
    //

    memset(&SigIntAction, 0, sizeof(struct sigaction));
    SigIntAction.sa_handler = KTestSignalHandler;
    Status = sigaction(SIGINT, &SigIntAction, &OriginalSigIntAction);
    if (Status != 0) {
        PRINT_ERROR("Error: Failed to set SIGINT handler: %s.\n",
                    strerror(errno));

        Failures += 1;
        KTestCancelAllTests = TRUE;
        goto MainEnd;
    }

    do {
        AllDone = TRUE;
        AnyProgress = FALSE;
        for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex += 1) {
            Poll.Handle = KTestProgress[HandleIndex].Handle;
            if (Poll.Handle < 0) {
                continue;
            }

            Status = ioctl(DriverHandle, KTestRequestPoll, &Poll);
            if (Status != 0) {
                PRINT_ERROR("Error: Failed to poll: %s.\n", strerror(errno));
                Failures += 1;
                continue;
            }

            if (!KSUCCESS(Poll.Status)) {
                PRINT_ERROR("Error: Poll returned %d.\n", Poll.Status);
                Failures += 1;
                continue;
            }

            if (Poll.TestFinished == FALSE) {
                AllDone = FALSE;
            }

            //
            // Spit out some progress characters if the needle has moved.
            //

            if (Poll.Total != 0) {
                Percent = (ULONGLONG)(Poll.Progress) * 100 / Poll.Total;
                while (Percent != KTestProgress[HandleIndex].PreviousPercent) {
                    AnyProgress = TRUE;
                    PRINT("%c", KTestProgress[HandleIndex].Character);
                    KTestProgress[HandleIndex].PreviousPercent += 1;
                }
            }

            //
            // Print the results if finished.
            //

            if (Poll.TestFinished != FALSE) {
                TestName = KTestNames[KTestProgress[HandleIndex].Test];
                KTestProgress[HandleIndex].Handle = -1;
                PRINT("\n");
                if (Poll.Results.Failures != 0) {
                    Failures += Poll.Results.Failures;
                    PRINT_ERROR("Test %s finished with %d errors. Status %d.\n",
                                TestName,
                                Poll.Results.Failures,
                                Poll.Results.Status);
                }

                switch (KTestProgress[HandleIndex].Test) {
                case KTestPagedPoolStress:
                case KTestNonPagedPoolStress:
                    DEBUG_PRINT("%s: Max Allocation Count: %d\n"
                                "Max Single Allocation Size: %d\n"
                                "Max Allocated Memory: %d\n",
                                TestName,
                                Poll.Results.Results[0],
                                Poll.Results.Results[1],
                                Poll.Results.Results[2]);

                    break;

                case KTestWorkStress:
                case KTestThreadStress:
                case KTestDescriptorStress:
                    break;

                case KTestPagedBlockStress:
                case KTestNonPagedBlockStress:
                    DEBUG_PRINT("%s: Max Allocation Count: %d\n"
                                "Max Allocated Memory: %d\n",
                                TestName,
                                Poll.Results.Results[0],
                                Poll.Results.Results[1]);

                    break;

                default:

                    assert(FALSE);

                    break;
                }
            }
        }

        //
        // If no progress was printed, then sleep for a bit instead of
        // pounding the processor polling.
        //

        if ((AnyProgress == FALSE) && (AllDone == FALSE)) {
            sleep(1);
        }

        //
        // Kick out of the loop of all tests need to be canceled.
        //

        if (KTestCancelAllTests != FALSE) {
            break;
        }

    } while (AllDone == FALSE);

    Status = sigaction(SIGINT, &OriginalSigIntAction, NULL);
    if (Status != 0) {
        PRINT_ERROR("Error: Failed to restore SIGINT action: %s.\n",
                    strerror(errno));

        Failures += 1;
    }

MainEnd:
    PRINT("\n");
    if (KTestCancelAllTests != FALSE) {
        for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex += 1) {
            Cancel.Handle = KTestProgress[HandleIndex].Handle;
            Cancel.Status = STATUS_SUCCESS;
            Status = ioctl(DriverHandle, KTestRequestCancelTest, &Cancel);
            if (Status != 0) {
                PRINT_ERROR("Error: Failed to cacnel: %s.\n", strerror(errno));
                Failures += 1;
                continue;
            }

            if (!KSUCCESS(Cancel.Status)) {
                PRINT_ERROR("Error: Cancel returned %d.\n", Cancel.Status);
                Failures += 1;
                continue;
            }
        }
    }

    if (DriverHandle != -1) {
        Status = ioctl(DriverHandle, KTestRequestUnload, NULL);
        if (Status != 0) {
            PRINT_ERROR("Error: Failed to send unload ioctl: %s.\n",
                        strerror(errno));

            Failures += 1;
        }

        KTestCloseDriver(DriverHandle);
    }

    if (Status != 0) {
        PRINT_ERROR("Error: %d.\n", Status);
    }

    if (Failures != 0) {
        PRINT_ERROR("\n   *** %d failures in ktest ***\n", Failures);
        return Failures;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KTestLoadDriver (
    VOID
    )

/*++

Routine Description:

    This routine loads the kernel test driver.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PSTR CompletePath;
    UINTN CompletePathSize;
    PSTR CurrentDirectory;
    UINTN CurrentDirectorySize;
    UINTN DriverNameSize;
    KSTATUS Status;

    CompletePath = NULL;
    CurrentDirectory = getcwd(NULL, 0);
    if (CurrentDirectory == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto LoadDriverEnd;
    }

    CurrentDirectorySize = strlen(CurrentDirectory);
    DriverNameSize = strlen(KTEST_DRIVER_NAME);
    CompletePathSize = CurrentDirectorySize + 1 + DriverNameSize + 1;
    CompletePath = malloc(CompletePathSize);
    if (CompletePath == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadDriverEnd;
    }

    memcpy(CompletePath, CurrentDirectory, CurrentDirectorySize);
    CompletePath[CurrentDirectorySize] = '/';
    memcpy(CompletePath + CurrentDirectorySize + 1,
           KTEST_DRIVER_NAME,
           DriverNameSize);

    CompletePath[CompletePathSize - 1] = '\0';
    Status = OsLoadDriver(CompletePath, CompletePathSize);
    if (!KSUCCESS(Status)) {
        goto LoadDriverEnd;
    }

LoadDriverEnd:
    if (CurrentDirectory != NULL) {
        free(CurrentDirectory);
    }

    if (CompletePath != NULL) {
        free(CompletePath);
    }

    return Status;
}

KSTATUS
KTestOpenDriver (
    PINT DriverHandle
    )

/*++

Routine Description:

    This routine opens a handle to the kernel test driver.

Arguments:

    DriverHandle - Supplies a pointer that receives the handle to the kernel
        test driver.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    UINTN DataSize;
    HANDLE Handle;
    ULONG ResultCount;
    UINTN ResultIndex;
    PDEVICE_INFORMATION_RESULT Results;
    KSTATUS Status;
    TEST_DEVICE_INFORMATION TestDeviceInformation;

    Handle = INVALID_HANDLE;
    ResultCount = 0;
    Results = NULL;

    //
    // Enumerate all the devices that support getting kernel test information.
    //

    Status = OsLocateDeviceInformation(&KTestTestDeviceInformationUuid,
                                       NULL,
                                       NULL,
                                       &ResultCount);

    if (Status != STATUS_BUFFER_TOO_SMALL) {
        goto OpenDriverEnd;
    }

    if (ResultCount == 0) {
        Status = STATUS_SUCCESS;
        goto OpenDriverEnd;
    }

    AllocationSize = sizeof(DEVICE_INFORMATION_RESULT) * ResultCount;
    Results = malloc(AllocationSize);
    if (Results == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenDriverEnd;
    }

    memset(Results, 0, AllocationSize);
    Status = OsLocateDeviceInformation(&KTestTestDeviceInformationUuid,
                                       NULL,
                                       Results,
                                       &ResultCount);

    if (!KSUCCESS(Status)) {
        goto OpenDriverEnd;
    }

    if (ResultCount == 0) {
        Status = STATUS_SUCCESS;
        goto OpenDriverEnd;
    }

    //
    // Loop through the results trying to open the first kernel test device
    // that succeeds.
    //

    for (ResultIndex = 0; ResultIndex < ResultCount; ResultIndex += 1) {
        DataSize = sizeof(TEST_DEVICE_INFORMATION);
        Status = OsGetSetDeviceInformation(Results[ResultIndex].DeviceId,
                                           &KTestTestDeviceInformationUuid,
                                           &TestDeviceInformation,
                                           &DataSize,
                                           FALSE);

        if (KSUCCESS(Status) &&
            (TestDeviceInformation.Version >=
             TEST_DEVICE_INFORMATION_VERSION) &&
            (TestDeviceInformation.DeviceType == TestDeviceKernel)) {

            Status = OsOpenDevice(Results[ResultIndex].DeviceId,
                                  SYS_OPEN_FLAG_READ | SYS_OPEN_FLAG_WRITE,
                                  &Handle);

            if (KSUCCESS(Status)) {
                goto OpenDriverEnd;
            }
        }
    }

    Status = STATUS_NO_SUCH_DEVICE;

OpenDriverEnd:
    if (Results != NULL) {
        free(Results);
    }

    *DriverHandle = (INT)(INTN)Handle;
    return Status;
}

VOID
KTestCloseDriver (
    INT DriverHandle
    )

/*++

Routine Description:

    This routine closes the given driver handle.

Arguments:

    DriverHandle - Supplies a handle to the driver to be closed.

Return Value:

    None.

--*/

{

    OsClose((HANDLE)(UINTN)DriverHandle);
    return;
}

INT
KTestSendStartRequest (
    INT DriverHandle,
    KTEST_TYPE Test,
    PKTEST_START_TEST Request,
    PINT HandleCount
    )

/*++

Routine Description:

    This routine sends a start request for the given test.

Arguments:

    DriverHandle - Supplies the file descriptor to the open ktest device.

    Test - Supplies the test to start.

    Request - Supplies a pointer to the request parameters.

    HandleCount - Supplies a pointer that on input contains the valid handle
        count. This will be incremented on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Status;

    Request->Parameters.TestType = Test;
    Request->Handle = -1;
    Status = ioctl(DriverHandle, KTestRequestStartTest, Request);
    if (Status != 0) {
        PRINT_ERROR("Failed to send start ioctl: %s.\n", strerror(errno));
        return 1;
    }

    assert(*HandleCount < KTestCount);

    if (!KSUCCESS(Request->Status)) {
        PRINT_ERROR("Start ioctl failed: %d\n", Request->Status);
        return 1;
    }

    KTestProgress[*HandleCount].Test = Test;
    KTestProgress[*HandleCount].Handle = Request->Handle;
    KTestProgress[*HandleCount].Character = 'A' + *HandleCount;
    KTestProgress[*HandleCount].PreviousPercent = 0;
    *HandleCount += 1;
    return 0;
}

void
KTestSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine handles the SIGINT signal while running the kernel test.

Arguments:

    Signal - Supplies the signal number that fired.

Return Value:

    None.

--*/

{

    assert(Signal == SIGINT);

    KTestCancelAllTests = TRUE;
    return;
}

