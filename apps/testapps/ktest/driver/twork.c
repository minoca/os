/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    twork.c

Abstract:

    This module implements the kernel work item test.

Author:

    Evan Green 11-Nov-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "ktestdrv.h"
#include "testsup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define KTEST_WORK_DEFAULT_ITERATIONS 500000
#define KTEST_WORK_DEFAULT_THREAD_COUNT 20
#define KTEST_WORK_DEFAULT_ALLOCATION_SIZE 512

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _KTEST_WORK_ITEM_CONTEXT {
    PKTEST_PARAMETERS Parameters;
    PKEVENT Event;
} KTEST_WORK_ITEM_CONTEXT, *PKTEST_WORK_ITEM_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KTestWorkStressRoutine (
    PVOID Parameter
    );

VOID
KTestWorkStressWorkRoutine (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KTestWorkStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    )

/*++

Routine Description:

    This routine starts a new invocation of the work item stress test.

Arguments:

    Command - Supplies a pointer to the start command.

    Test - Supplies a pointer to the active test structure to initialize.

Return Value:

    Status code.

--*/

{

    PKTEST_PARAMETERS Parameters;
    KSTATUS Status;
    ULONG ThreadIndex;

    Parameters = &(Test->Parameters);
    RtlCopyMemory(Parameters, &(Command->Parameters), sizeof(KTEST_PARAMETERS));
    if (Parameters->Iterations == 0) {
        Parameters->Iterations = KTEST_WORK_DEFAULT_ITERATIONS;
    }

    if (Parameters->Threads == 0) {
        Parameters->Threads = KTEST_WORK_DEFAULT_THREAD_COUNT;
    }

    if (Parameters->Parameters[0] == 0) {
        Parameters->Parameters[0] = KTEST_WORK_DEFAULT_ALLOCATION_SIZE;
    }

    Test->Total = Test->Parameters.Iterations;
    Test->Results.Status = STATUS_SUCCESS;
    Test->Results.Failures = 0;
    for (ThreadIndex = 0;
         ThreadIndex < Test->Parameters.Threads;
         ThreadIndex += 1) {

        Status = PsCreateKernelThread(KTestWorkStressRoutine,
                                      Test,
                                      "KTestWorkStressRoutine");

        if (!KSUCCESS(Status)) {
            goto WorkStressStartEnd;
        }
    }

    Status = STATUS_SUCCESS;

WorkStressStartEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KTestWorkStressRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the work item stress test.

Arguments:

    Parameter - Supplies a pointer to the thread parameter, which in this
        case is a pointer to the active test structure.

Return Value:

    None.

--*/

{

    ULONG Failures;
    PKTEST_ACTIVE_TEST Information;
    ULONG Iteration;
    PKTEST_PARAMETERS Parameters;
    KSTATUS Status;
    ULONG ThreadNumber;
    KTEST_WORK_ITEM_CONTEXT WorkContext;

    Failures = 0;
    Information = Parameter;
    Parameters = &(Information->Parameters);
    RtlZeroMemory(&WorkContext, sizeof(KTEST_WORK_ITEM_CONTEXT));
    WorkContext.Parameters = Parameters;
    WorkContext.Event = KeCreateEvent(NULL);
    if (WorkContext.Event == NULL) {
        Failures += 1;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TestWorkStressRoutineEnd;
    }

    ThreadNumber = RtlAtomicAdd32(&(Information->ThreadsStarted), 1);
    for (Iteration = 0; Iteration < Parameters->Iterations; Iteration += 1) {
        if (Information->Cancel != FALSE) {
            Status = STATUS_SUCCESS;
            goto TestWorkStressRoutineEnd;
        }

        KeSignalEvent(WorkContext.Event, SignalOptionUnsignal);
        Status = KeCreateAndQueueWorkItem(NULL,
                                          WorkPriorityNormal,
                                          KTestWorkStressWorkRoutine,
                                          &WorkContext);

        if (!KSUCCESS(Status)) {
            Failures += 1;
            goto TestWorkStressRoutineEnd;
        }

        Status = KeWaitForEvent(WorkContext.Event,
                                FALSE,
                                WAIT_TIME_INDEFINITE);

        if (!KSUCCESS(Status)) {
            goto TestWorkStressRoutineEnd;
        }

        if (ThreadNumber == 0) {
            Information->Progress += 1;
        }
    }

    Status = STATUS_SUCCESS;

TestWorkStressRoutineEnd:
    KeDestroyEvent(WorkContext.Event);
    WorkContext.Event = NULL;

    //
    // Save the results.
    //

    if (!KSUCCESS(Status)) {
        Information->Results.Status = Status;
    }

    Information->Results.Failures += Failures;
    RtlAtomicAdd32(&(Information->ThreadsFinished), 1);
    return;
}

VOID
KTestWorkStressWorkRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the work item routine for the work item stress
    test.

Arguments:

    Parameter - Supplies a pointer to the parameter, which in this
        case is a pointer to the work item context.

Return Value:

    None.

--*/

{

    PUCHAR Allocation;
    ULONG AllocationSize;
    ULONG ByteIndex;
    PKTEST_WORK_ITEM_CONTEXT WorkContext;

    WorkContext = Parameter;

    //
    // Allocate and scribble on some memory to make it seem like some work is
    // being done.
    //

    AllocationSize = (KTestGetRandomValue() %
                      WorkContext->Parameters->Parameters[0]) + 1;

    Allocation = MmAllocatePagedPool(AllocationSize, KTEST_ALLOCATION_TAG);
    if (Allocation == NULL) {
        goto TestWorkStressWorkRoutineEnd;
    }

    for (ByteIndex = 0; ByteIndex < AllocationSize; ByteIndex += 1) {
        Allocation[ByteIndex] = 0xB0 + ByteIndex;
    }

TestWorkStressWorkRoutineEnd:
    if (Allocation != NULL) {
        MmFreePagedPool(Allocation);
    }

    KeSignalEvent(WorkContext->Event, SignalOptionSignalAll);
    return;
}

