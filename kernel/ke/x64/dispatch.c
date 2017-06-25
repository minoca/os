/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dispatch.c

Abstract:

    This module implements interrupt dispatch functionality for AMD64
    processors.

Author:

    Evan Green 11-Jun-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/x64.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
KeDispatchSingleStepTrap (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches a single step trap.

Arguments:

    TrapFrame - Supplies a pointer to the machine state immediately before the
        trap.

Return Value:

    None.

--*/

{

    CYCLE_ACCOUNT PreviousPeriod;
    PKTHREAD Thread;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    if (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) == FALSE) {
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
        ArEnableInterrupts();
        Thread = KeGetCurrentThread();
        PsSignalThread(Thread, SIGNAL_TRAP, NULL, FALSE);
        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
        ArDisableInterrupts();

        //
        // If there is no handler yet, go into the kernel debugger.
        //

        if (Thread->OwningProcess->SignalHandlerRoutine == NULL) {
            KdDebugExceptionHandler(EXCEPTION_SINGLE_STEP, NULL, TrapFrame);
        }

        KeBeginCycleAccounting(PreviousPeriod);

    } else {

        //
        // TODO: On x64, does syscall clear the TF flag?
        //

        KdDebugExceptionHandler(EXCEPTION_SINGLE_STEP, NULL, TrapFrame);
    }

    return;
}

VOID
KeDispatchNmiTrap (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches an NMI interrupt. NMIs are task switches (to avoid
    a race with the sysret instruction), so the previous context is saved in a
    task structure.

Arguments:

    TrapFrame - Supplies a pointer to the machine state immediately before the
        trap.

Return Value:

    None.

--*/

{

    CYCLE_ACCOUNT PreviousPeriod;
    PPROCESSOR_BLOCK Processor;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    //
    // Do a little detection of nested NMIs, which are currently not supported.
    //

    Processor = KeGetCurrentProcessorBlock();
    Processor->NmiCount += 1;
    if (Processor->NmiCount == 2) {
        RtlDebugBreak();
    }

    PreviousPeriod = CycleAccountInvalid;
    if (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) == FALSE) {
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
    }

    KdNmiHandler(TrapFrame);
    if (PreviousPeriod != CycleAccountInvalid) {
        KeBeginCycleAccounting(PreviousPeriod);
    }

    Processor->NmiCount -= 1;
    return;
}

VOID
KeDispatchDebugServiceTrap (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches a debug service trap.

Arguments:

    TrapFrame - Supplies a pointer to the machine state immediately before the
        trap.

Return Value:

    None.

--*/

{

    CYCLE_ACCOUNT PreviousPeriod;
    PKTHREAD Thread;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    if (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) == FALSE) {
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
        ArEnableInterrupts();
        Thread = KeGetCurrentThread();
        PsSignalThread(Thread, SIGNAL_TRAP, NULL, FALSE);
        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
        ArDisableInterrupts();
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(TrapFrame->Rdi,
                                (PVOID)(TrapFrame->Rsi),
                                TrapFrame);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

