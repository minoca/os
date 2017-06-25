/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pcexcept.c

Abstract:

    This module implements common interrupt dispatch functionality between
    x86 and AMD64 processors.

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

#if __SIZEOF_LONG__ == 8

#include <minoca/kernel/x64.h>

#else

#include <minoca/kernel/x86.h>

#endif

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
KeDispatchInterrupt (
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

    //
    // The vector byte was sign extended, so cast back down to get rid of the
    // high bytes.
    //

    HlDispatchInterrupt((UCHAR)(TrapFrame->ErrorCode), TrapFrame);
    KeBeginCycleAccounting(PreviousPeriod);
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
        PsSignalThread(Thread, SIGNAL_TRAP, NULL, TRUE);
        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
        ArDisableInterrupts();
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(EXCEPTION_BREAK, NULL, TrapFrame);
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

        ASSERT(ArAreInterruptsEnabled() != FALSE);

        Thread = KeGetCurrentThread();
        PsSignalThread(Thread, SIGNAL_MATH_ERROR, NULL, TRUE);
        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(EXCEPTION_DIVIDE_BY_ZERO, NULL, TrapFrame);
        KeCrashSystem(CRASH_DIVIDE_BY_ZERO,
                      (UINTN)TrapFrame,
                      (UINTN)ArGetInstructionPointer(TrapFrame),
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

        ASSERT((Thread->FpuFlags & THREAD_FPU_FLAG_IN_USE) == 0);

        Thread->FpuContext =
                           ArAllocateFpuContext(PS_FPU_CONTEXT_ALLOCATION_TAG);

        if (Thread->FpuContext == NULL) {
            PsSignalThread(Thread, SIGNAL_BUS_ERROR, NULL, TRUE);
            goto DispatchFpuAccessTrapEnd;
        }
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);

    //
    // Restore context if this is not the thread's first time using the FPU. If
    // the thread happens to already have its state on the CPU, then there's no
    // need to do the restore.
    //

    if ((Thread->FpuFlags & THREAD_FPU_FLAG_IN_USE) != 0) {
        if ((Thread->FpuFlags & THREAD_FPU_FLAG_OWNER) != 0) {
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
        Thread->FpuFlags |= THREAD_FPU_FLAG_IN_USE;
    }

    Thread->FpuFlags |= THREAD_FPU_FLAG_OWNER;
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

        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(EXCEPTION_ACCESS_VIOLATION, NULL, TrapFrame);
        KeCrashSystem(CRASH_PAGE_FAULT,
                      (UINTN)TrapFrame,
                      (UINTN)ArGetInstructionPointer(TrapFrame),
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
        PsSignalThread(Thread, SIGNAL_MATH_ERROR, NULL, TRUE);
        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
        KeBeginCycleAccounting(PreviousPeriod);

    } else {
        KdDebugExceptionHandler(EXCEPTION_MATH_FAULT, NULL, TrapFrame);
        KeCrashSystem(CRASH_MATH_FAULT,
                      (UINTN)TrapFrame,
                      (UINTN)ArGetInstructionPointer(TrapFrame),
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

