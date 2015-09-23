/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    ttpc.c

Abstract:

    This module implements the kernel Thread Procedure Call test.

Author:

    Chris Stevens 23-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
#include "ktestdrv.h"
#include "testsup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define KTEST_TPC_DEFAULT_ITERATIONS 500000
#define KTEST_TPC_DEFAULT_THREAD_COUNT 20

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _KTEST_TPC_CONTEXT {
    PTPC Tpc;
    BOOL TpcRan;
    PKEVENT Event;
} KTEST_TPC_CONTEXT, *PKTEST_TPC_CONTEXT;

typedef struct _KTEST_TPC_WORK_ITEM_CONTEXT {
    PKTEST_PARAMETERS Parameters;
    PKEVENT Event;
    PKTEST_TPC_CONTEXT TpcContext;
} KTEST_TPC_WORK_ITEM_CONTEXT, *PKTEST_TPC_WORK_ITEM_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KTestTpcStressRoutine (
    PVOID Parameter
    );

VOID
KTestTpcStressWorkRoutine (
    PVOID Parameter
    );

VOID
KTestTpcCallbackRoutine (
    PTPC Tpc
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KTestTpcStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    )

/*++

Routine Description:

    This routine starts a new invocation of the Thread Procedure Call (TPC)
    stress test.

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
        Parameters->Iterations = KTEST_TPC_DEFAULT_ITERATIONS;
    }

    if (Parameters->Threads == 0) {
        Parameters->Threads = KTEST_TPC_DEFAULT_THREAD_COUNT;
    }

    Test->Total = Parameters->Iterations;
    Test->Results.Status = STATUS_SUCCESS;
    Test->Results.Failures = 0;
    for (ThreadIndex = 0;
         ThreadIndex < Test->Parameters.Threads;
         ThreadIndex += 1) {

        Status = PsCreateKernelThread(KTestTpcStressRoutine,
                                      Test,
                                      "KTestTpcStressRoutine");

        if (!KSUCCESS(Status)) {
            goto TpcStressStartEnd;
        }
    }

    Status = STATUS_SUCCESS;

TpcStressStartEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KTestTpcStressRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the TPC stress test.

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
    TPC Tpc;
    KTEST_TPC_CONTEXT TpcContext;
    KTEST_TPC_WORK_ITEM_CONTEXT WorkContext;

    Failures = 0;
    Information = Parameter;
    Parameters = &(Information->Parameters);
    RtlZeroMemory(&TpcContext, sizeof(KTEST_TPC_CONTEXT));
    KeInitializeTpc(&Tpc, KTestTpcCallbackRoutine, &TpcContext);
    TpcContext.Tpc = &Tpc;
    RtlZeroMemory(&WorkContext, sizeof(KTEST_TPC_WORK_ITEM_CONTEXT));
    WorkContext.Parameters = Parameters;
    WorkContext.TpcContext = &TpcContext;
    WorkContext.Event = KeCreateEvent(NULL);
    if (WorkContext.Event == NULL) {
        Failures += 1;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TestWorkStressRoutineEnd;
    }

    TpcContext.Event = WorkContext.Event;
    ThreadNumber = RtlAtomicAdd32(&(Information->ThreadsStarted), 1);
    for (Iteration = 0; Iteration < Parameters->Iterations; Iteration += 1) {
        if (Information->Cancel != FALSE) {
            Status = STATUS_SUCCESS;
            goto TestWorkStressRoutineEnd;
        }

        //
        // Prepare the TPC to run on this thread and then queue a work item to
        // actually queue it.
        //

        TpcContext.TpcRan = FALSE;
        KePrepareTpc(TpcContext.Tpc, NULL, TRUE);
        KeSignalEvent(WorkContext.Event, SignalOptionUnsignal);
        Status = KeCreateAndQueueWorkItem(NULL,
                                          WorkPriorityNormal,
                                          KTestTpcStressWorkRoutine,
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

        if (TpcContext.TpcRan == FALSE) {
            KePrepareTpc(TpcContext.Tpc, NULL, FALSE);
            Failures += 1;
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
KTestTpcStressWorkRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the work item routine for the TPC stress test.

Arguments:

    Parameter - Supplies a pointer to the parameter, which in this case is a
        pointer to the work item context.

Return Value:

    None.

--*/

{

    KSTATUS Status;
    PTPC Tpc;
    KTEST_TPC_CONTEXT TpcContext;
    PKTEST_TPC_WORK_ITEM_CONTEXT WorkContext;

    WorkContext = Parameter;
    RtlZeroMemory(&TpcContext, sizeof(KTEST_TPC_CONTEXT));

    //
    // Allocate and queue a TPC on the current thread.
    //

    Tpc = KeCreateTpc(KTestTpcCallbackRoutine, &TpcContext);
    if (Tpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TestTpcStressWorkRoutineEnd;
    }

    TpcContext.Tpc = Tpc;
    KePrepareTpc(Tpc, NULL, TRUE);
    KeQueueTpc(Tpc, NULL);
    KeDestroyTpc(Tpc);
    if (TpcContext.TpcRan == FALSE) {
        Status = STATUS_UNSUCCESSFUL;
        goto TestTpcStressWorkRoutineEnd;
    }

    KeQueueTpc(WorkContext->TpcContext->Tpc, NULL);
    Status = STATUS_SUCCESS;

TestTpcStressWorkRoutineEnd:
    if (!KSUCCESS(Status)) {
        KeSignalEvent(WorkContext->Event, SignalOptionSignalAll);
    }

    return;
}

VOID
KTestTpcCallbackRoutine (
    PTPC Tpc
    )

/*++

Routine Description:

    This routine is used to signal that the TPC ran.

Arguments:

    Tpc - Supplies a pointer to the TPC that is running.

Return Value:

    None.

--*/

{

    PKTEST_TPC_CONTEXT TpcContext;

    TpcContext = Tpc->UserData;
    TpcContext->TpcRan = TRUE;
    if (TpcContext->Event != NULL) {
        KeSignalEvent(TpcContext->Event, SignalOptionSignalAll);
    }

    return;
}

