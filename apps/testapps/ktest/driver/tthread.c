/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tthread.c

Abstract:

    This module implements the kernel thread test.

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

#define KTEST_THREAD_DEFAULT_ITERATIONS 30000
#define KTEST_THREAD_DEFAULT_THREAD_COUNT 20

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KTestThreadStressRoutine (
    PVOID Parameter
    );

VOID
KTestThreadStressThread (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KTestThreadStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    )

/*++

Routine Description:

    This routine starts a new invocation of the thread stress test.

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
        Parameters->Iterations = KTEST_THREAD_DEFAULT_ITERATIONS;
    }

    if (Parameters->Threads == 0) {
        Parameters->Threads = KTEST_THREAD_DEFAULT_THREAD_COUNT;
    }

    Test->Total = Test->Parameters.Iterations;
    Test->Results.Status = STATUS_SUCCESS;
    Test->Results.Failures = 0;
    for (ThreadIndex = 0;
         ThreadIndex < Test->Parameters.Threads;
         ThreadIndex += 1) {

        Status = PsCreateKernelThread(KTestThreadStressRoutine,
                                      Test,
                                      "KTestThreadStressRoutine");

        if (!KSUCCESS(Status)) {
            goto ThreadStressStartEnd;
        }
    }

    Status = STATUS_SUCCESS;

ThreadStressStartEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KTestThreadStressRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the thread stress test.

Arguments:

    Parameter - Supplies a pointer to the thread parameter, which in this
        case is a pointer to the active test structure.

Return Value:

    None.

--*/

{

    PKEVENT Event;
    ULONG Failures;
    PKTEST_ACTIVE_TEST Information;
    ULONG Iteration;
    PKTEST_PARAMETERS Parameters;
    KSTATUS Status;
    ULONG ThreadNumber;

    Failures = 0;
    Information = Parameter;
    Parameters = &(Information->Parameters);
    ThreadNumber = RtlAtomicAdd32(&(Information->ThreadsStarted), 1);
    Event = KeCreateEvent(NULL);
    if (Event == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto TestWorkStressRoutineEnd;
    }

    for (Iteration = 0; Iteration < Parameters->Iterations; Iteration += 1) {
        if (Information->Cancel != FALSE) {
            Status = STATUS_SUCCESS;
            goto TestWorkStressRoutineEnd;
        }

        KeSignalEvent(Event, SignalOptionUnsignal);
        Status = PsCreateKernelThread(KTestThreadStressThread,
                                      Event,
                                      "KTestThreadStressThread");

        if (!KSUCCESS(Status)) {
            Failures += 1;

        } else {
            Status = KeWaitForEvent(Event, FALSE, WAIT_TIME_INDEFINITE);
            if (!KSUCCESS(Status)) {
                Failures += 1;
            }
        }

        if (ThreadNumber == 0) {
            Information->Progress += 1;
        }
    }

    Status = STATUS_SUCCESS;

TestWorkStressRoutineEnd:
    if (Event != FALSE) {
        KeDestroyEvent(Event);
    }

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
KTestThreadStressThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the thread test thread, which does nothing but exit.

Arguments:

    Parameter - Supplies a pointer to the parameter, which in this
        case is a pointer to the work item context.

Return Value:

    None.

--*/

{

    KeSignalEvent(Parameter, SignalOptionSignalAll);
    return;
}

