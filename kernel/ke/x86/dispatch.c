/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
#include <minoca/kdebug.h>
#include <minoca/x86.h>

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
KeDispatchException (
    ULONG Vector,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches a device interrupt.

Arguments:

    Vector - Supplies the vector this interrupt came in on.

    TrapFrame - Supplies a pointer to the machine state immediately before the
        interrupt.

Return Value:

    None.

--*/

{

    CYCLE_ACCOUNT PreviousPeriod;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    PreviousPeriod = KeBeginCycleAccounting(CycleAccountInterrupt);
    HlDispatchInterrupt(Vector, TrapFrame);
    KeBeginCycleAccounting(PreviousPeriod);
    return;
}

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
        PsSignalThread(Thread, SIGNAL_TRAP, NULL);
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
        // Here's something interesting. The sysenter instruction doesn't clear
        // the trap flag, so if usermode sets TF and executes sysenter, it
        // produces a single step exception in kernel mode. Watch out for this
        // specifically.
        //

        if (TrapFrame->Eip == (UINTN)ArSysenterHandlerAsm) {
            TrapFrame->Eflags &= ~IA32_EFLAG_TF;
            Thread = KeGetCurrentThread();
            Thread->Flags |= THREAD_FLAG_SINGLE_STEP;
            return;
        }

        KdDebugExceptionHandler(EXCEPTION_SINGLE_STEP, NULL, TrapFrame);
    }

    return;
}

VOID
KeDispatchBreakPointTrap (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches a breakpoint trap.

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
        PsSignalThread(Thread, SIGNAL_TRAP, NULL);
        PsDispatchPendingSignals(Thread, TrapFrame);
        ArDisableInterrupts();
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(EXCEPTION_BREAK, NULL, TrapFrame);
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

    CYCLE_ACCOUNT PreviousPeriod;
    PPROCESSOR_BLOCK Processor;
    TRAP_FRAME TrapFrame;
    PTSS Tss;

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

    KdNmiHandler(&TrapFrame);
    ArSetKernelTssTrapFrame(&TrapFrame);
    if (PreviousPeriod != CycleAccountInvalid) {
        KeBeginCycleAccounting(PreviousPeriod);
    }

    //
    // The processor doesn't save the old CR3, so put the correct one back.
    //

    Tss = Processor->Tss;
    Tss->Cr3 = Processor->RunningThread->OwningProcess->PageDirectoryPhysical;
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
        PsSignalThread(Thread, SIGNAL_TRAP, NULL);
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

VOID
KeDispatchDivideByZeroTrap (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches a divide-by-zero trap.

Arguments:

    TrapFrame - Supplies a pointer to the machine state immediately before the
        trap.

Return Value:

    None.

--*/

{

    CYCLE_ACCOUNT PreviousPeriod;
    PKTHREAD Thread;

    if (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) == FALSE) {
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
        ArEnableInterrupts();
        Thread = KeGetCurrentThread();
        PsSignalThread(Thread, SIGNAL_MATH_ERROR, NULL);
        PsDispatchPendingSignals(Thread, TrapFrame);
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(EXCEPTION_DIVIDE_BY_ZERO, NULL, TrapFrame);
        KeCrashSystem(CRASH_DIVIDE_BY_ZERO,
                      (UINTN)TrapFrame,
                      TrapFrame->Eip,
                      0,
                      0);
    }

    ArDisableInterrupts();
    return;
}

VOID
KeDispatchFpuAccessTrap (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches a floating point access trap.

Arguments:

    TrapFrame - Supplies a pointer to the machine state immediately before the
        trap.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;
    CYCLE_ACCOUNT PreviousPeriod;
    PKTHREAD Thread;

    //
    // FPU access faults are "trap" type gates, so they shouldn't disable
    // interrupts.
    //

    ASSERT(ArAreInterruptsEnabled() != FALSE);

    PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
    Thread = KeGetCurrentThread();

    //
    // If the thread has never used the FPU before, allocate FPU context while
    // still at low level.
    //

    if (Thread->FpuContext == NULL) {

        ASSERT((Thread->Flags & THREAD_FLAG_USING_FPU) == 0);

        Thread->FpuContext =
                           ArAllocateFpuContext(PS_FPU_CONTEXT_ALLOCATION_TAG);

        if (Thread->FpuContext == NULL) {
            PsSignalThread(Thread, SIGNAL_BUS_ERROR, NULL);
            goto DispatchFpuAccessTrapEnd;
        }
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);

    //
    // Restore context if this is not the thread's first time using the FPU. If
    // the thread happens to already have its state on the CPU, then there's no
    // need to do the restore.
    //

    if ((Thread->Flags & THREAD_FLAG_USING_FPU) != 0) {
        if ((Thread->Flags & THREAD_FLAG_FPU_OWNER) != 0) {
            ArEnableFpu();

        } else {
            ArRestoreFpuState(Thread->FpuContext);
        }

    //
    // If this is the first time using the FPU, enable it, initialize it, and
    // mark the thread as using it. An NMI could come in between the enable
    // and initialize, which would cause the initialize to fault.
    //

    } else {
        ArEnableFpu();
        ArInitializeFpu();
        Thread->Flags |= THREAD_FLAG_USING_FPU;
    }

    Thread->Flags |= THREAD_FLAG_FPU_OWNER;
    KeLowerRunLevel(OldRunLevel);

DispatchFpuAccessTrapEnd:
    KeBeginCycleAccounting(PreviousPeriod);
    return;
}

VOID
KeDispatchProtectionFault (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches a protection fault trap.

Arguments:

    TrapFrame - Supplies a pointer to the machine state immediately before the
        trap.

Return Value:

    None.

--*/

{

    CYCLE_ACCOUNT PreviousPeriod;
    PKTHREAD Thread;

    if (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) == FALSE) {
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
        ArEnableInterrupts();
        Thread = KeGetCurrentThread();
        PsHandleUserModeFault(NULL,
                              FAULT_FLAG_PROTECTION_FAULT,
                              TrapFrame,
                              Thread->OwningProcess);

        PsDispatchPendingSignals(Thread, TrapFrame);
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(EXCEPTION_ACCESS_VIOLATION, NULL, TrapFrame);
        KeCrashSystem(CRASH_PAGE_FAULT,
                      (UINTN)TrapFrame,
                      TrapFrame->Eip,
                      0,
                      0);
    }

    ArDisableInterrupts();
    return;
}

VOID
KeDispatchMathFault (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches a math fault from the x87 unit.

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
        PsSignalThread(Thread, SIGNAL_MATH_ERROR, NULL);
        PsDispatchPendingSignals(Thread, TrapFrame);
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(EXCEPTION_MATH_FAULT, NULL, TrapFrame);
        KeCrashSystem(CRASH_MATH_FAULT,
                      (UINTN)TrapFrame,
                      TrapFrame->Eip,
                      0,
                      0);
    }

    ArDisableInterrupts();
    return;
}

VOID
KeDispatchPageFault (
    PVOID FaultingAddress,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles page faults.

Arguments:

    FaultingAddress - Supplies the address that caused the fault.

    TrapFrame - Supplies a pointer to the trap frame of the fault.

Return Value:

    None.

--*/

{

    ULONG FaultFlags;
    CYCLE_ACCOUNT PreviousPeriod;

    PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
    FaultFlags = 0;
    if ((TrapFrame->ErrorCode & X86_FAULT_FLAG_PROTECTION_VIOLATION) == 0) {
        FaultFlags |= FAULT_FLAG_PAGE_NOT_PRESENT;
    }

    if ((TrapFrame->ErrorCode & X86_FAULT_ERROR_CODE_WRITE) != 0) {
        FaultFlags |= FAULT_FLAG_WRITE;
    }

    MmHandleFault(FaultFlags, FaultingAddress, TrapFrame);
    KeBeginCycleAccounting(PreviousPeriod);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

