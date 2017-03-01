/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dispatch.c

Abstract:

    This module implements interrupt dispatch functionality for ARM processors.

Author:

    Evan Green 11-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include <minoca/kernel/kdebug.h>

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
    PTRAP_FRAME TrapFrame,
    BOOL PrefetchAbort
    )

/*++

Routine Description:

    This routine receives a generic exception and dispatches it to the correct
    handler based on the type of exception and the previous execution mode.

Arguments:

    TrapFrame - Supplies a pointer to the state immediately before the
        exception.

    PrefetchAbort - Supplies a boolean indicating if this is a prefetch abort
        or data abort. For non-aborts, this parameter is undefined.

Return Value:

    None.

--*/

{

    ULONG FaultFlags;
    PVOID FaultingAddress;
    ULONG FaultStatus;
    CYCLE_ACCOUNT PreviousPeriod;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    PreviousPeriod = CycleAccountInvalid;

    //
    // The SVC mode stack pointer is wrong because it has the trap frame on it.
    // "Add" that off to get the real stack pointer.
    //

    TrapFrame->SvcSp += sizeof(TRAP_FRAME);

    //
    // Dispatch the exception according to which mode it came from.
    //

    switch (TrapFrame->ExceptionCpsr & ARM_MODE_MASK) {
    case ARM_MODE_FIQ:
    case ARM_MODE_IRQ:
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountInterrupt);
        TrapFrame->Pc -= 4;
        HlDispatchInterrupt(0, TrapFrame);
        break;

    case ARM_MODE_ABORT:
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);

        //
        // The trap handlers set the overflow flag of the exception-mode PSR for
        // prefetch (instruction) aborts. This helps determine which Fault
        // Address Register to read.
        //

        if (PrefetchAbort != FALSE) {
            FaultingAddress = ArGetInstructionFaultingAddress();
            FaultStatus = ArGetInstructionFaultStatus();

        } else {
            FaultingAddress = ArGetDataFaultingAddress();
            FaultStatus = ArGetDataFaultStatus();
        }

        ArEnableInterrupts();

        //
        // Translate the fault status register a bit.
        //

        FaultFlags = 0;
        if ((FaultStatus & ARM_FAULT_STATUS_WRITE) != 0) {
            FaultFlags |= FAULT_FLAG_WRITE;
        }

        if (IS_ARM_PAGE_FAULT(FaultStatus)) {
            FaultFlags |= FAULT_FLAG_PAGE_NOT_PRESENT;

        } else if (IS_ARM_PERMISSION_FAULT(FaultStatus)) {
            FaultFlags |= FAULT_FLAG_PERMISSION_ERROR;
        }

        MmHandleFault(FaultFlags, FaultingAddress, TrapFrame);
        ArDisableInterrupts();
        break;

    case ARM_MODE_UNDEF:
        KdDebugExceptionHandler(EXCEPTION_UNDEFINED_INSTRUCTION,
                                NULL,
                                TrapFrame);

        break;

    default:
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
        KdDebugExceptionHandler(EXCEPTION_ACCESS_VIOLATION, NULL, TrapFrame);
        break;
    }

    //
    // Re-adjust the SVC stack pointer. If it was changed since the first
    // adjustment, the routine changing it *must* copy the trap frame over.
    //

    TrapFrame->SvcSp -= sizeof(TRAP_FRAME);

    //
    // Restore the previous cycle accounting type.
    //

    if (PreviousPeriod != CycleAccountInvalid) {
        KeBeginCycleAccounting(PreviousPeriod);
    }

    return;
}

VOID
KeDispatchUndefinedInstructionException (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine is called from the assembly trap handlers to handle the
    undefined instruction exception, which is usually an intentional debug
    break.

Arguments:

    TrapFrame - Supplies a pointer to the state immediately before the
        exception.

Return Value:

    None.

--*/

{

    PVOID Address;
    ULONG Exception;
    BOOL Handled;
    ULONG Instruction;
    BOOL IsBreak;
    PVOID Parameter;
    CYCLE_ACCOUNT PreviousPeriod;
    PKTHREAD Thread;

    Parameter = NULL;
    if (ArIsTrapFrameFromPrivilegedMode(TrapFrame) == FALSE) {
        ArEnableInterrupts();
    }

    //
    // Get the instruction. Use the user-mode read routines since they're also
    // safe on kernel mode memory.
    //

    Instruction = 0;
    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        Address = (PVOID)REMOVE_THUMB_BIT(TrapFrame->Pc) -
                  THUMB16_INSTRUCTION_LENGTH;

        MmUserRead16(Address, (PUSHORT)&Instruction);

        //
        // Watch out for this being a 32-bit Thumb-2 instruction. If it is,
        // the processor put the PC in the middle of it. Advance beyond.
        //

        if ((Instruction >> THUMB32_OP_SHIFT) >= THUMB32_OP_MIN) {
            Instruction = (Instruction << 16);
            TrapFrame->Pc += 2;
            MmUserRead16(Address + 2, (PUSHORT)&Instruction);
        }

    } else {
        Address = (PVOID)(TrapFrame->Pc - ARM_INSTRUCTION_LENGTH);
        MmUserRead32(Address, &Instruction);
    }

    //
    // The SVC mode stack pointer is wrong because it has the trap frame on it.
    // "Add" that off to get the real stack pointer.
    //

    TrapFrame->SvcSp += sizeof(TRAP_FRAME);
    if (ArIsTrapFrameFromPrivilegedMode(TrapFrame) == FALSE) {
        PreviousPeriod = KeBeginCycleAccounting(CycleAccountKernel);
        Thread = KeGetCurrentThread();
        IsBreak = FALSE;
        if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
            if ((Instruction == THUMB_BREAK_INSTRUCTION) ||
                (Instruction == THUMB_SINGLE_STEP_INSTRUCTION)) {

                IsBreak = TRUE;
            }

        } else {
            if ((Instruction == ARM_BREAK_INSTRUCTION) ||
                (Instruction == ARM_SINGLE_STEP_INSTRUCTION)) {

                IsBreak = TRUE;
            }
        }

        if (IsBreak == FALSE) {

            //
            // Walk the PC backwards as this is a real undefined instruction.
            //

            TrapFrame->Pc = (UINTN)Address;
            Handled = ArCheckForVfpException(TrapFrame, Instruction);
            if (Handled == FALSE) {
                PsSignalThread(Thread, SIGNAL_ILLEGAL_INSTRUCTION, NULL, TRUE);
            }

        } else {
            PsSignalThread(Thread, SIGNAL_TRAP, NULL, TRUE);
        }

        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
        KeBeginCycleAccounting(PreviousPeriod);

    } else {

        //
        // If this is a debug service exception, get parameters.
        //

        Exception = EXCEPTION_UNDEFINED_INSTRUCTION;
        IsBreak = FALSE;
        Parameter = NULL;
        if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
            if (Instruction == THUMB_DEBUG_SERVICE_INSTRUCTION) {
                Exception = TrapFrame->R0;
                Parameter = (PVOID)TrapFrame->R1;
                IsBreak = TRUE;

            } else if ((Instruction == THUMB_BREAK_INSTRUCTION) ||
                       (Instruction == THUMB_SINGLE_STEP_INSTRUCTION)) {

                IsBreak = TRUE;
            }

        } else {
            if (Instruction == ARM_DEBUG_SERVICE_INSTRUCTION) {
                Exception = TrapFrame->R0;
                Parameter = (PVOID)TrapFrame->R1;
                IsBreak = TRUE;

            } else if ((Instruction == ARM_BREAK_INSTRUCTION) ||
                       (Instruction == ARM_SINGLE_STEP_INSTRUCTION)) {

                IsBreak = TRUE;
            }
        }

        //
        // If this is a break instruction, then continue executing at the next
        // address so the machine makes progress if no debugger is attached. If
        // this isn't a debug-related instruction, then set the PC back as it's
        // a real undefined instruction.
        //

        if (IsBreak == FALSE) {
            TrapFrame->Pc = (UINTN)Address;
        }

        //
        // Dispatch the exception according to which mode it came from.
        //

        KdDebugExceptionHandler(Exception, Parameter, TrapFrame);
        if (IsBreak == FALSE) {
            KeCrashSystem(CRASH_ILLEGAL_INSTRUCTION,
                          Instruction,
                          (UINTN)TrapFrame,
                          0,
                          0);
        }
    }

    //
    // Re-adjust the SVC stack pointer. If it was changed since the first
    // adjustment, the routine changing it *must* copy the trap frame over.
    //

    TrapFrame->SvcSp -= sizeof(TRAP_FRAME);
    return;
}

VOID
KeDoubleFaultHandler (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine is called when a stack exception is taken by the trap handlers.
    It attmepts to take the system down gracefully.

Arguments:

    TrapFrame - Supplies a pointer to the state immediately before the
        exception.

Return Value:

    This routine does not return.

--*/

{

    //
    // First enter the debugger with this context, then crash.
    //

    KdDebugExceptionHandler(EXCEPTION_DOUBLE_FAULT, NULL, TrapFrame);
    KeCrashSystem(CRASH_KERNEL_STACK_EXCEPTION, (UINTN)TrapFrame, 0, 0, 0);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

