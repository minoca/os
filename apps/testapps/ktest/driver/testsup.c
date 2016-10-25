/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testsup.c

Abstract:

    This module implements support infrastructure for the kernel test.

Author:

    Evan Green 5-Nov-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "ktestdrv.h"
#include "testsup.h"
#include "ktests.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of tests that can be going on simultaneously.
// This can be increased if necessary.
//

#define KTEST_MAX_CONCURRENT_TESTS 30

//
// Define the number of seconds to wait for a test to cancel itself.
//

#define KTEST_CANCEL_TIMEOUT 30

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KTestCreateTest (
    PINT Handle,
    PKTEST_ACTIVE_TEST *Test
    );

PKTEST_ACTIVE_TEST
KTestLookupTest (
    INT Handle
    );

VOID
KTestDestroyTest (
    PKTEST_ACTIVE_TEST Test
    );

//
// -------------------------------------------------------------------- Globals
//

KSPIN_LOCK KTestHandleLock;
PKTEST_ACTIVE_TEST KTestHandles[KTEST_MAX_CONCURRENT_TESTS];

//
// Define the global test dispatch table, indexed by the test enum.
//

KTEST_FUNCTION_TABLE KTestFunctionTable[KTestCount] = {
    {NULL},
    {KTestPoolStressStart},
    {KTestPoolStressStart},
    {KTestWorkStressStart},
    {KTestThreadStressStart},
    {KTestDescriptorStressStart},
    {KTestBlockStressStart},
    {KTestBlockStressStart},
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KTestInitializeTestSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes the kernel test support structures.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KeInitializeSpinLock(&KTestHandleLock);
    RtlZeroMemory(KTestHandles, sizeof(KTestHandles));
    return STATUS_SUCCESS;
}

KSTATUS
KTestStartTest (
    PVOID Buffer,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine starts a new test invocation.

Arguments:

    Buffer - Supplies a pointer to the user mode buffer.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    Status code.

--*/

{

    PKTEST_ACTIVE_TEST ActiveTest;
    INT Handle;
    KSTATUS OverallStatus;
    PKTEST_START StartRoutine;
    KTEST_START_TEST StartTest;
    KSTATUS Status;

    Status = STATUS_UNSUCCESSFUL;

    //
    // Copy the parameters from user mode.
    //

    if (BufferSize < sizeof(KTEST_START_TEST)) {
        OverallStatus = STATUS_DATA_LENGTH_MISMATCH;
        goto StartTestEnd;
    }

    OverallStatus = MmCopyFromUserMode(&StartTest,
                                       Buffer,
                                       sizeof(KTEST_START_TEST));

    if (!KSUCCESS(OverallStatus)) {
        goto StartTestEnd;
    }

    //
    // Create a handle table entry.
    //

    Status = KTestCreateTest(&Handle, &ActiveTest);
    if (!KSUCCESS(Status)) {
        goto StartTestEnd;
    }

    OverallStatus = STATUS_SUCCESS;
    if ((StartTest.Parameters.TestType == KTestAll) ||
        (StartTest.Parameters.TestType >= KTestCount)) {

        Status = STATUS_INVALID_PARAMETER;
        goto StartTestEnd;
    }

    //
    // Call the corresponding start routine.
    //

    StartRoutine = KTestFunctionTable[StartTest.Parameters.TestType].Start;
    StartTest.Status = StartRoutine(&StartTest, ActiveTest);
    if (!KSUCCESS(Status)) {
        KTestDestroyTest(ActiveTest);
        goto StartTestEnd;
    }

    StartTest.Handle = Handle;

StartTestEnd:
    StartTest.Status = Status;
    if (KSUCCESS(OverallStatus)) {
        OverallStatus = MmCopyToUserMode(Buffer,
                                         &StartTest,
                                         sizeof(KTEST_START_TEST));
    }

    return OverallStatus;
}

KSTATUS
KTestRequestCancellation (
    PVOID Buffer,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine sends a cancel request to an active test.

Arguments:

    Buffer - Supplies a pointer to the user mode buffer.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    Status code.

--*/

{

    PKTEST_ACTIVE_TEST ActiveTest;
    KSTATUS OverallStatus;
    KTEST_CANCEL_TEST Request;
    KSTATUS Status;

    Status = STATUS_UNSUCCESSFUL;

    //
    // Copy the parameters from user mode.
    //

    if (BufferSize < sizeof(KTEST_CANCEL_TEST)) {
        OverallStatus = STATUS_DATA_LENGTH_MISMATCH;
        goto RequestCancellationEnd;
    }

    OverallStatus = MmCopyFromUserMode(&Request,
                                       Buffer,
                                       sizeof(KTEST_CANCEL_TEST));

    if (!KSUCCESS(OverallStatus)) {
        goto RequestCancellationEnd;
    }

    ActiveTest = KTestLookupTest(Request.Handle);
    if (ActiveTest == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto RequestCancellationEnd;
    }

    ActiveTest->Cancel = TRUE;
    RtlMemoryBarrier();
    Status = STATUS_SUCCESS;

RequestCancellationEnd:
    Request.Status = Status;
    if (KSUCCESS(OverallStatus)) {
        OverallStatus = MmCopyToUserMode(Buffer,
                                         &Request,
                                         sizeof(KTEST_CANCEL_TEST));
    }

    return OverallStatus;
}

KSTATUS
KTestPoll (
    PVOID Buffer,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine sends a status request to an active test.

Arguments:

    Buffer - Supplies a pointer to the user mode buffer.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    Status code.

--*/

{

    PKTEST_ACTIVE_TEST ActiveTest;
    KSTATUS OverallStatus;
    KTEST_POLL Request;
    KSTATUS Status;

    Status = STATUS_UNSUCCESSFUL;

    //
    // Copy the parameters from user mode.
    //

    if (BufferSize < sizeof(KTEST_POLL)) {
        OverallStatus = STATUS_DATA_LENGTH_MISMATCH;
        goto PollEnd;
    }

    OverallStatus = MmCopyFromUserMode(&Request, Buffer, sizeof(KTEST_POLL));
    if (!KSUCCESS(OverallStatus)) {
        goto PollEnd;
    }

    ActiveTest = KTestLookupTest(Request.Handle);
    if (ActiveTest == NULL) {
        Status = STATUS_INVALID_HANDLE;
        goto PollEnd;
    }

    Request.Progress = ActiveTest->Progress;
    Request.Total = ActiveTest->Total;
    Request.TestFinished = FALSE;
    if ((ActiveTest->ThreadsStarted == ActiveTest->Parameters.Threads) &&
        (ActiveTest->ThreadsFinished != 0) &&
        (ActiveTest->ThreadsFinished == ActiveTest->ThreadsStarted)) {

        Request.TestFinished = TRUE;
        RtlCopyMemory(&(Request.Parameters),
                      &(ActiveTest->Parameters),
                      sizeof(KTEST_PARAMETERS));

        RtlCopyMemory(&(Request.Results),
                      &(ActiveTest->Results),
                      sizeof(KTEST_RESULTS));

        //
        // Reap the test structure.
        //

        KTestDestroyTest(ActiveTest);
    }

    Status = STATUS_SUCCESS;

PollEnd:
    Request.Status = Status;
    if (KSUCCESS(OverallStatus)) {
        OverallStatus = MmCopyToUserMode(Buffer,
                                         &Request,
                                         sizeof(KTEST_POLL));
    }

    return OverallStatus;
}

VOID
KTestFlushAllTests (
    VOID
    )

/*++

Routine Description:

    This routine does not return until all tests have been cancelled or
    completed.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKTEST_ACTIVE_TEST ActiveTest;
    ULONG HandleIndex;
    ULONGLONG Timeout;

    for (HandleIndex = 0;
         HandleIndex < KTEST_MAX_CONCURRENT_TESTS;
         HandleIndex += 1) {

        ActiveTest = KTestHandles[HandleIndex];
        if (ActiveTest == NULL) {
            continue;
        }

        Timeout = KeGetRecentTimeCounter() +
                  (HlQueryTimeCounterFrequency() * KTEST_CANCEL_TIMEOUT);

        ActiveTest->Cancel = TRUE;
        while ((ActiveTest->ThreadsStarted != ActiveTest->Parameters.Threads) ||
               (ActiveTest->ThreadsFinished == 0) ||
               (ActiveTest->ThreadsFinished != ActiveTest->ThreadsStarted)) {

            KeYield();
            if (KeGetRecentTimeCounter() > Timeout) {
                RtlDebugPrint("KTest: KTEST_ACTIVE_TEST 0x%x hung.\n",
                              ActiveTest);

                ASSERT(FALSE);

                break;
            }
        }

        KTestDestroyTest(ActiveTest);
    }

    return;
}

ULONG
KTestGetRandomValue (
    VOID
    )

/*++

Routine Description:

    This routine returns a random value.

Arguments:

    None.

Return Value:

    Returns a random 32-bit value.

--*/

{

    ULONGLONG Value;

    Value = HlQueryTimeCounter();
    Value ^= Value >> 32;
    return (ULONG)Value * 1103515245;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KTestCreateTest (
    PINT Handle,
    PKTEST_ACTIVE_TEST *Test
    )

/*++

Routine Description:

    This routine creates a new active test structure and handle.

Arguments:

    Handle - Supplies a pointer where the handle will be returned.

    Test - Supplies a pointer where a pointer to the new test will be returned.

Return Value:

    Status code.

--*/

{

    ULONG Index;
    PKTEST_ACTIVE_TEST NewTest;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Index = -1;
    NewTest = MmAllocatePagedPool(sizeof(KTEST_ACTIVE_TEST),
                                  KTEST_ALLOCATION_TAG);

    if (NewTest == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateActiveTestEnd;
    }

    Status = STATUS_TOO_MANY_HANDLES;
    RtlZeroMemory(NewTest, sizeof(KTEST_ACTIVE_TEST));
    KeAcquireSpinLock(&KTestHandleLock);
    for (Index = 0; Index < KTEST_MAX_CONCURRENT_TESTS; Index += 1) {
        if (KTestHandles[Index] == NULL) {
            KTestHandles[Index] = NewTest;
            Status = STATUS_SUCCESS;
            break;
        }
    }

    KeReleaseSpinLock(&KTestHandleLock);

CreateActiveTestEnd:
    if (!KSUCCESS(Status)) {
        if (NewTest != NULL) {
            MmFreePagedPool(NewTest);
            NewTest = NULL;
        }
    }

    *Handle = Index;
    *Test = NewTest;
    return Status;
}

PKTEST_ACTIVE_TEST
KTestLookupTest (
    INT Handle
    )

/*++

Routine Description:

    This routine looks up the test structure given a handle.

Arguments:

    Handle - Supplies the handle returned when the test was created.

Return Value:

    Returns a pointer to the test structure on success.

    NULL on failure.

--*/

{

    PKTEST_ACTIVE_TEST Test;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (Handle >= KTEST_MAX_CONCURRENT_TESTS) {
        return NULL;
    }

    Test = KTestHandles[Handle];
    return Test;
}

VOID
KTestDestroyTest (
    PKTEST_ACTIVE_TEST Test
    )

/*++

Routine Description:

    This routine destroys an active test and removes it from the handle table.

Arguments:

    Test - Supplies a pointer to the test structure.

Return Value:

    None.

--*/

{

    ULONG Index;

    KeAcquireSpinLock(&KTestHandleLock);
    for (Index = 0; Index < KTEST_MAX_CONCURRENT_TESTS; Index += 1) {
        if (KTestHandles[Index] == Test) {
            KTestHandles[Index] = NULL;
            break;
        }
    }

    KeReleaseSpinLock(&KTestHandleLock);

    ASSERT(Index != KTEST_MAX_CONCURRENT_TESTS);

    MmFreePagedPool(Test);
    return;
}

