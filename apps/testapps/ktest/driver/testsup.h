/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testsup.h

Abstract:

    This header contains definitions for the test support header.

Author:

    Evan Green 5-Nov-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define KTEST_ALLOCATION_TAG 0x5453544B // 'TSTK'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the information for an active test.

Members:

    Progress - Stores the current progress value so far.

    Total - Stores the total progress amount.

    ThreadsStarted - Stores the number of threads that have started running.

    ThreadsFinished - Stores the number of threads that have finished running.

    Cancel - Stores a boolean set to TRUE if the user would like to cancel the
        test.

    Parameters - Stores the test parameters.

    Results - Stores the test results.

--*/

typedef struct _KTEST_ACTIVE_TEST {
    ULONG Progress;
    ULONG Total;
    ULONG ThreadsStarted;
    ULONG ThreadsFinished;
    BOOL Cancel;
    KTEST_PARAMETERS Parameters;
    KTEST_RESULTS Results;
} KTEST_ACTIVE_TEST, *PKTEST_ACTIVE_TEST;

typedef
KSTATUS
(*PKTEST_START) (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    );

/*++

Routine Description:

    This routine starts a new test invocation.

Arguments:

    Command - Supplies a pointer to the start command.

    Test - Supplies a pointer to the active test structure to initialize.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines the function table for a kernel test.

Members:

    Start - Stores a pointer to a routine used to start a new test.

--*/

typedef struct _KTEST_FUNCTION_TABLE {
    PKTEST_START Start;
} KTEST_FUNCTION_TABLE, *PKTEST_FUNCTION_TABLE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
KTestInitializeTestSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes the kernel test support structures.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
KTestStartTest (
    PVOID Buffer,
    ULONG BufferSize
    );

/*++

Routine Description:

    This routine starts a new test invocation.

Arguments:

    Buffer - Supplies a pointer to the user mode buffer.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    Status code.

--*/

KSTATUS
KTestRequestCancellation (
    PVOID Buffer,
    ULONG BufferSize
    );

/*++

Routine Description:

    This routine sends a cancel request to an active test.

Arguments:

    Buffer - Supplies a pointer to the user mode buffer.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    Status code.

--*/

KSTATUS
KTestPoll (
    PVOID Buffer,
    ULONG BufferSize
    );

/*++

Routine Description:

    This routine sends a cancel request to an active test.

Arguments:

    Buffer - Supplies a pointer to the user mode buffer.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    Status code.

--*/

VOID
KTestFlushAllTests (
    VOID
    );

/*++

Routine Description:

    This routine does not return until all tests have been cancelled or
    completed.

Arguments:

    None.

Return Value:

    None.

--*/

ULONG
KTestGetRandomValue (
    VOID
    );

/*++

Routine Description:

    This routine returns a random value.

Arguments:

    None.

Return Value:

    Returns a random 32-bit value.

--*/

