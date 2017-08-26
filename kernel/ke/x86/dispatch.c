/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dispatch.c

Abstract:

    This module implements interrupt dispatch functionality for x86 processors.

Author:

    Evan Green 27-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/x86.h>

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
        // If there is no handler or debugger yet, go into the kernel debugger.
        //

        if ((Thread->OwningProcess->SignalHandlerRoutine == NULL) &&
            (Thread->OwningProcess->DebugData == NULL)) {

            KdDebugExceptionHandler(EXCEPTION_SINGLE_STEP, NULL, TrapFrame);
        }

        KeBeginCycleAccounting(PreviousPeriod);

    } else {

        //
        // Here's something interesting. The sysenter instruction doesn't clear
        // the trap flag, so if usermode sets TF and executes sysenter, it
        // produces a single step exception in kernel mode. Move to the slow
        // system call path (so that eflags gets restored), and move Eip to a
        // version that sets TF in the trap frame.
        //

        if (TrapFrame->Eip == (UINTN)ArSysenterHandlerAsm) {
            TrapFrame->Eflags &= ~IA32_EFLAG_TF;
            TrapFrame->Eip = (UINTN)ArTrapSystemCallHandlerAsm;
            return;
        }

        KdDebugExceptionHandler(EXCEPTION_SINGLE_STEP, NULL, TrapFrame);
    }

    return;
}

VOID
KeDispatchNmiTrap (
    VOID
    )

/*++

Routine Description:

    This routine dispatches an NMI interrupt. NMIs are task switches (to avoid
    a race with the sysret instruction), so the previous context is saved in a
    task structure.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PTSS KernelTask;
    CYCLE_ACCOUNT PreviousPeriod;
    PPROCESSOR_BLOCK Processor;
    TRAP_FRAME TrapFrame;

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
    ArGetKernelTssTrapFrame(&TrapFrame);
    if (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(&TrapFrame) == FALSE) {
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
    }

    //
    // Switch to the kernel task's CR3 in order to allow peeking at user mode
    // addresses if this NMI is for a debugger freeze.
    //

    KernelTask = Processor->Tss;
    if (KernelTask != NULL) {
        ArSetCurrentPageDirectory(KernelTask->Cr3);
    }

    KdNmiHandler(&TrapFrame);
    ArSetKernelTssTrapFrame(&TrapFrame);
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
        KdDebugExceptionHandler(TrapFrame->Eax,
                                (PVOID)(TrapFrame->Ecx),
                                TrapFrame);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

