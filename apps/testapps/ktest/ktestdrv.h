/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ktestdrv.h

Abstract:

    This header contains definitions shared between the kernel test driver
    and the app.

Author:

    Evan Green 5-Nov-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/devinfo/test.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the name of the test device that is created.
//

#define KTEST_DEVICE_NAME "KTestDevice"

//
// Define the number of extra parameters that are included in each test.
//

#define KTEST_PARAMETER_COUNT 4

//
// Define the number of result parameters included in each test.
//

#define KTEST_RESULT_COUNT 4

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _KTEST_TYPE {
    KTestAll,
    KTestPagedPoolStress,
    KTestNonPagedPoolStress,
    KTestWorkStress,
    KTestThreadStress,
    KTestDescriptorStress,
    KTestPagedBlockStress,
    KTestNonPagedBlockStress,
    KTestCount
} KTEST_TYPE, *PKTEST_TYPE;

typedef enum _KTEST_REQUEST {
    KTestRequestInvalid,
    KTestRequestUnload,
    KTestRequestStartTest,
    KTestRequestCancelTest,
    KTestRequestPoll,
} KTEST_REQUEST, *PKTEST_REQUEST;

/*++

Structure Description:

    This structure defines a set of kernel test parameters.

Members:

    TestType - Stores the type of test to fire up.

    Iterations - Stores the number of iterations of the test to perform.

    Threads - Stores the number of threads to spin up.

    Parameters - Stores an array of test-specific parameters.

--*/

typedef struct _KTEST_PARAMETERS {
    KTEST_TYPE TestType;
    INTN Iterations;
    UINTN Threads;
    UINTN Parameters[KTEST_PARAMETER_COUNT];
} KTEST_PARAMETERS, *PKTEST_PARAMETERS;

/*++

Structure Description:

    This structure defines the results for a kernel test.

Members:

    Iterations - Stores the number of iterations of the test to perform.

    Threads - Stores the number of threads to spin up.

    Parameters - Stores an array of test-specific parameters.

    Status - Stores a status code associated with one of the failures.

--*/

typedef struct _KTEST_RESULTS {
    UINTN Failures;
    UINTN Results[KTEST_RESULT_COUNT];
    KSTATUS Status;
} KTEST_RESULTS, *PKTEST_RESULTS;

/*++

Structure Description:

    This structure defines the parameters for a start test command.

Members:

    Iterations - Stores the number of iterations of the test to perform.

    Threads - Stores the number of threads to spin up.

    Parameters - Stores the array of test-specific parameters.

    Status - Stores the resulting status code from the driver.

    Handle - Stores the handle for the test invocation on success.

--*/

typedef struct _KTEST_START_TEST {
    KTEST_PARAMETERS Parameters;
    KSTATUS Status;
    INT Handle;
} KTEST_START_TEST, *PKTEST_START_TEST;

/*++

Structure Description:

    This structure defines the parameters for a cancel test command.

Members:

    Handle - Stores the handle of the test to cancel.

    Status - Stores the result of the operation.

--*/

typedef struct _KTEST_CANCEL_TEST {
    INT Handle;
    KSTATUS Status;
} KTEST_CANCEL_TEST, *PKTEST_CANCEL_TEST;

/*++

Structure Description:

    This structure defines the parameters for a poll command.

Members:

    Handle - Stores the handle of the test to cancel.

    Status - Stores the result of the operation.

    Progress - Stores the test progress so far.

    Total - Stores the value the progress indicator is climbing towards.

    TestFinished - Stores a boolean returned from the kernel indicating if the
        test just finished.

    Parameters - Stores the test parameters used by the test, with any default
        values filled in.

    Results - Stores the current test results.

--*/

typedef struct _KTEST_POLL {
    INT Handle;
    KSTATUS Status;
    UINTN Progress;
    UINTN Total;
    BOOL TestFinished;
    KTEST_PARAMETERS Parameters;
    KTEST_RESULTS Results;
} KTEST_POLL, *PKTEST_POLL;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
