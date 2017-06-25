/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    psarch.c

Abstract:

    This module implements architecture specific functionality for the process
    and thread library.

Author:

    Evan Green 28-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include <minoca/debug/dbgproto.h>
#include "../psp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the length of thumb instructions used to execute full and fast system
// calls.
//

#define THUMB_SWI_INSTRUCTION_LENGTH THUMB16_INSTRUCTION_LENGTH
#define THUMB_EOR_INSTRUCTION_LENGTH THUMB32_INSTRUCTION_LENGTH
#define THUMB_MOV_INSTRUCTION_LENGTH THUMB32_INSTRUCTION_LENGTH

//
// Define the length of the required PC back-up when restarting a system call.
// Luckily, both eor and mov are the same size, so full and fast system calls
// have the same back-up length.
//

#define THUMB_RESTART_PC_BACKUP_LENGTH \
    (THUMB_SWI_INSTRUCTION_LENGTH + THUMB_EOR_INSTRUCTION_LENGTH)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PspArchGetNextPcReadMemory (
    PVOID Address,
    ULONG Size,
    PVOID Data
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the initial architecture-specific contents of the thread pointer data
// for a newly created thread.
//

const ULONGLONG PsInitialThreadPointer = 0;

//
// ------------------------------------------------------------------ Functions
//

ULONG
PsDequeuePendingSignal (
    PSIGNAL_PARAMETERS SignalParameters,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dequeues the first signal in the thread or process signal mask
    of the current thread that is not handled by any default processing.

Arguments:

    SignalParameters - Supplies a pointer to a caller-allocated structure where
        the signal parameter information might get returned.

    TrapFrame - Supplies a pointer to the current trap frame. If this trap frame
        is not destined for user mode, this function exits immediately.

Return Value:

    Returns a signal number if a signal was queued.

    -1 if no signal was dispatched.

--*/

{

    BOOL SignalHandled;
    ULONG SignalNumber;

    //
    // If the trap frame is not destined for user mode, then forget it.
    //

    if (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) != FALSE) {
        return -1;
    }

    do {
        SignalNumber = PspDequeuePendingSignal(SignalParameters,
                                               TrapFrame,
                                               NULL);

        if (SignalNumber == -1) {
            break;
        }

        SignalHandled = PspSignalAttemptDefaultProcessing(SignalNumber);

    } while (SignalHandled != FALSE);

    return SignalNumber;
}

VOID
PsApplySynchronousSignal (
    PTRAP_FRAME TrapFrame,
    PSIGNAL_PARAMETERS SignalParameters,
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine applies the given signal onto the current thread. It is
    required that no signal is already in progress, nor will any other signals
    be applied for the duration of the system call.

Arguments:

    TrapFrame - Supplies a pointer to the current trap frame. This trap frame
        must be destined for user mode.

    SignalParameters - Supplies a pointer to the signal information to apply.

    SystemCallNumber - Supplies the number of the system call that is
        attempting to dispatch a pending signal. Supply SystemCallInvalid if
        the caller is not a system call.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call that is attempting to dispatch a signal. Supply NULL if
        the caller is not a system call.

Return Value:

    None.

--*/

{

    PSIGNAL_CONTEXT_ARM Context;
    ULONG ContextSp;
    ULONG Flags;
    PSIGNAL_SET RestoreSignals;
    BOOL Result;
    KSTATUS Status;
    INTN SystemCallResult;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    ContextSp = ALIGN_RANGE_DOWN(TrapFrame->UserSp - sizeof(SIGNAL_CONTEXT_ARM),
                                 FPU_CONTEXT_ALIGNMENT);

    Context = (PVOID)ContextSp;
    Flags = 0;
    Result = MmUserWrite(&(Context->Common.Next), 0);
    RestoreSignals = &(Thread->BlockedSignals);
    if ((Thread->Flags & THREAD_FLAG_RESTORE_SIGNALS) != 0) {
        RestoreSignals = &(Thread->RestoreSignals);
        Thread->Flags &= ~THREAD_FLAG_RESTORE_SIGNALS;
    }

    Status = MmCopyToUserMode(&(Context->Common.Mask),
                              RestoreSignals,
                              sizeof(SIGNAL_SET));

    //
    // TODO: Support alternate signal stacks.
    //

    Result &= MmUserWrite(&(Context->Common.Stack.Base), 0);
    Result &= MmUserWrite(&(Context->Common.Stack.Size), 0);
    Result &= MmUserWrite32(&(Context->Common.Stack.Flags), 0);
    Status |= MmCopyToUserMode(&(Context->TrapFrame),
                               TrapFrame,
                               sizeof(TRAP_FRAME));

    TrapFrame->UserSp = ContextSp;
    if ((Thread->FpuFlags & THREAD_FPU_FLAG_IN_USE) != 0) {
        Flags |= SIGNAL_CONTEXT_FLAG_FPU_VALID;
        if ((Thread->FpuFlags & THREAD_FPU_FLAG_OWNER) != 0) {
            ArSaveFpuState(Thread->FpuContext);
        }

        Status |= MmCopyToUserMode(&(Context->FpuContext),
                                   Thread->FpuContext,
                                   sizeof(FPU_CONTEXT));
    }

    Result &= MmUserWrite32(&(Context->TrapFrame.SvcSp), 0);
    Result &= MmUserWrite32(&(Context->TrapFrame.SvcLink), 0);

    //
    // If this signal is being applied in the middle of a system call, the trap
    // frame needs modification if it is restartable. R0 holds the system call
    // result.
    //

    SystemCallResult = (INTN)TrapFrame->R0;
    if ((SystemCallNumber != SystemCallInvalid) &&
        (IS_SYSTEM_CALL_NUMBER_RESTARTABLE(SystemCallNumber) != FALSE) &&
        (IS_SYSTEM_CALL_RESULT_RESTARTABLE(SystemCallResult) != FALSE)) {

        //
        // If the result indicates that the system call is restartable after a
        // signal is applied, then let user mode know by setting the restart
        // flag in the context. Also save the system call number and parameters
        // in volatile registers so that they can be placed in the correct
        // registers for restart.
        //

        if (IS_SYSTEM_CALL_RESULT_RESTARTABLE_AFTER_SIGNAL(SystemCallResult)) {
            Flags |= SIGNAL_CONTEXT_FLAG_RESTART;
            MmUserWrite32(&(Context->TrapFrame.R1), (UINTN)SystemCallParameter);
            MmUserWrite32(&(Context->TrapFrame.R2), SystemCallNumber);
        }

        //
        // In case the handler does not allow restarts, convert the saved
        // restart status to the interrupted status.
        //

        MmUserWrite32(&(Context->TrapFrame.R0), STATUS_INTERRUPTED);
    }

    Result &= MmUserWrite32(&(Context->Common.Flags), Flags);
    TrapFrame->UserSp -= sizeof(SIGNAL_PARAMETERS);
    Status |= MmCopyToUserMode((PVOID)(TrapFrame->UserSp),
                               SignalParameters,
                               sizeof(SIGNAL_PARAMETERS));

    if ((Status != STATUS_SUCCESS) || (Result == FALSE)) {
        PsHandleUserModeFault((PVOID)(TrapFrame->UserSp),
                              FAULT_FLAG_WRITE | FAULT_FLAG_PAGE_NOT_PRESENT,
                              TrapFrame,
                              Thread->OwningProcess);

        PsDispatchPendingSignalsOnCurrentThread(TrapFrame,
                                                SystemCallNumber,
                                                SystemCallParameter);
    }

    TrapFrame->Pc = (ULONG)(Thread->OwningProcess->SignalHandlerRoutine);
    TrapFrame->Cpsr = ARM_MODE_USER;
    if ((TrapFrame->Pc & ARM_THUMB_BIT) != 0) {
        TrapFrame->Cpsr |= PSR_FLAG_THUMB;
    }

    ADD_SIGNAL(Thread->BlockedSignals, SignalParameters->SignalNumber);
    return;
}

INTN
PspRestorePreSignalTrapFrame (
    PTRAP_FRAME TrapFrame,
    PSIGNAL_CONTEXT UserContext
    )

/*++

Routine Description:

    This routine restores the original user mode thread context for the thread
    before a signal was invoked.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame from this system call.

    UserContext - Supplies the user context to restore.

Return Value:

    Returns the architecture-specific return register from the thread context.

--*/

{

    PSIGNAL_CONTEXT_ARM Context;
    ULONG Flags;
    TRAP_FRAME Frame;
    SIGNAL_SET SignalMask;
    KSTATUS Status;
    PKTHREAD Thread;

    Context = (PSIGNAL_CONTEXT_ARM)UserContext;
    Thread = KeGetCurrentThread();
    Status = MmCopyFromUserMode(&Frame,
                                &(Context->TrapFrame),
                                sizeof(TRAP_FRAME));

    Status |= MmCopyFromUserMode(&SignalMask,
                                 &(Context->Common.Mask),
                                 sizeof(SIGNAL_SET));

    if (!MmUserRead32(&(UserContext->Flags), &Flags)) {
        Status = STATUS_ACCESS_VIOLATION;
    }

    if (!KSUCCESS(Status)) {
        goto RestorePreSignalTrapFrameEnd;
    }

    PsSetSignalMask(&SignalMask, NULL);

    //
    // Sanitize the CPSR. Preserve the current trap frame's SVC SP and LR; they
    // were zero'd in the context and the SVC SP is needed for restoring the
    // trap frame. Also preserve the exception CPSR. It was possibly bogus in
    // the saved context.
    //

    Frame.Cpsr &= ~(ARM_MODE_MASK | PSR_FLAG_IRQ | PSR_FLAG_FIQ |
                    PSR_FLAG_ALIGNMENT);

    Frame.Cpsr |= ARM_MODE_USER;
    Frame.SvcSp = TrapFrame->SvcSp;
    Frame.SvcLink = TrapFrame->SvcLink;
    Frame.ExceptionCpsr = TrapFrame->ExceptionCpsr;
    RtlCopyMemory(TrapFrame, &Frame, sizeof(TRAP_FRAME));
    if (((Flags & SIGNAL_CONTEXT_FLAG_FPU_VALID) != 0) &&
        (Thread->FpuContext != NULL)) {

        Status = MmCopyFromUserMode(Thread->FpuContext,
                                    &(Context->FpuContext),
                                    sizeof(FPU_CONTEXT));

        if (!KSUCCESS(Status)) {
            goto RestorePreSignalTrapFrameEnd;
        }

        Thread->FpuFlags |= THREAD_FPU_FLAG_IN_USE;
        if ((Thread->FpuFlags & THREAD_FPU_FLAG_OWNER) != 0) {
            ArDisableFpu();
            Thread->FpuFlags &= ~THREAD_FPU_FLAG_OWNER;
        }
    }

    //
    // If a restart is necessary, back up the PC so that the system call
    // gets executed again when the trap frame gets restored. Also make sure
    // that the system call number and parameters are in R0 and R1, which just
    // requires copying R2 to R0, as the system call number was saved in R2.
    //

    if ((Flags & SIGNAL_CONTEXT_FLAG_RESTART) != 0) {
        TrapFrame->Pc -= THUMB_RESTART_PC_BACKUP_LENGTH;
        TrapFrame->R0 = TrapFrame->R2;
    }

RestorePreSignalTrapFrameEnd:
    if (!KSUCCESS(Status)) {
        PsSignalThread(Thread, SIGNAL_ACCESS_VIOLATION, NULL, TRUE);
    }

    return (INTN)TrapFrame->R0;
}

VOID
PspArchRestartSystemCall (
    PTRAP_FRAME TrapFrame,
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine determines whether or not a system call needs to be restarted.
    If so, it modifies the given trap frame such that the system call return
    to user mode will fall right back into calling the system call.

Arguments:

    TrapFrame - Supplies a pointer to the full trap frame saved by a system
        call in order to attempt dispatching a signal.

    SystemCallNumber - Supplies the number of the system call that is
        attempting to dispatch a pending signal. Supplied SystemCallInvalid if
        the caller is not a system call.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call that is attempting to dispatch a signal. Supply NULL if
        the caller is not a system call.

Return Value:

    None.

--*/

{

    //
    // On ARM, the trap frame holds the system call return value in R0. Check
    // to see if the system call can be restarted. If it cannot be restarted,
    // exit without modifying the trap frame.
    //

    if (!IS_SYSTEM_CALL_NUMBER_RESTARTABLE(SystemCallNumber) ||
        !IS_SYSTEM_CALL_RESULT_RESTARTABLE_NO_SIGNAL((INTN)TrapFrame->R0)) {

        return;
    }

    //
    // This system call needs to be restarted. Back up the PC. And restore the
    // system call number and parameter into R0 and R1.
    //

    TrapFrame->Pc -= THUMB_RESTART_PC_BACKUP_LENGTH;
    TrapFrame->R0 = SystemCallNumber;
    TrapFrame->R1 = (ULONG)(UINTN)SystemCallParameter;
    return;
}

VOID
PspPrepareThreadForFirstRun (
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame,
    BOOL ParameterIsStack
    )

/*++

Routine Description:

    This routine performs any architecture specific initialization to prepare a
    thread for being context swapped for the first time.

Arguments:

    Thread - Supplies a pointer to the thread being prepared for its first run.

    TrapFrame - Supplies an optional pointer for the thread to restore on its
        first run.

    ParameterIsStack - Supplies a boolean indicating whether the thread
        parameter is the value that should be used as the initial stack pointer.

Return Value:

    None.

--*/

{

    UINTN EntryPoint;
    ULONG Flags;
    PUINTN StackPointer;
    PTRAP_FRAME StackTrapFrame;
    PVOID UserStackPointer;

    //
    // Get the initial stack pointer, and word align it.
    //

    StackPointer = Thread->KernelStack + Thread->KernelStackSize;

    //
    // Determine the appropriate flags value.
    //

    Flags = 0;
    if ((Thread->Flags & THREAD_FLAG_USER_MODE) != 0) {
        Flags |= ARM_MODE_USER;
        EntryPoint = (UINTN)Thread->ThreadRoutine;

        ASSERT((TrapFrame == NULL) || (ParameterIsStack == FALSE));

        if (ParameterIsStack != FALSE) {
            UserStackPointer = Thread->ThreadParameter;

            ASSERT((UserStackPointer >= Thread->UserStack) &&
                   (UserStackPointer <
                    Thread->UserStack + Thread->UserStackSize));

        } else {
            UserStackPointer = Thread->UserStack + Thread->UserStackSize;
        }

    } else {
        Flags |= ARM_MODE_SVC;
        EntryPoint = (UINTN)PspKernelThreadStart;
        UserStackPointer = (PVOID)0x66666666;
    }

    if (((UINTN)EntryPoint & ARM_THUMB_BIT) != 0) {
        Flags |= PSR_FLAG_THUMB;
    }

    //
    // Make room for a trap frame to be restored.
    //

    StackPointer = (PUINTN)((PUCHAR)StackPointer -
                            ALIGN_RANGE_UP(sizeof(TRAP_FRAME), 8));

    StackTrapFrame = (PTRAP_FRAME)StackPointer;
    if (TrapFrame != NULL) {
        if (ArIsTrapFrameComplete(TrapFrame) != FALSE) {
            RtlCopyMemory(StackTrapFrame, TrapFrame, sizeof(TRAP_FRAME));

            //
            // Return a process ID of 0 to the child on fork.
            //

            StackTrapFrame->R0 = 0;

        } else {

            //
            // User mode tried to pull a fast one by forking with the fast
            // system call handler path. Joke's on them; zero out the registers
            // that didn't get saved.
            //

            RtlZeroMemory(StackTrapFrame, sizeof(TRAP_FRAME));
            StackTrapFrame->Cpsr = TrapFrame->Cpsr;
            StackTrapFrame->Pc = TrapFrame->Pc;
            StackTrapFrame->UserLink = TrapFrame->UserLink;
            StackTrapFrame->UserSp = TrapFrame->UserSp;
        }

        StackTrapFrame->SvcSp = (UINTN)StackPointer;

    } else {
        RtlZeroMemory(StackTrapFrame, sizeof(TRAP_FRAME));
        StackTrapFrame->SvcSp = (UINTN)StackPointer;
        StackTrapFrame->UserSp = (UINTN)UserStackPointer;
        StackTrapFrame->R0 = (UINTN)Thread->ThreadParameter;
        StackTrapFrame->Cpsr = Flags;
        StackTrapFrame->Pc = EntryPoint;
    }

    Thread->KernelStackPointer = StackPointer;
    return;
}

INTN
PspArchResetThreadContext (
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine sets up the given trap frame as if the user mode portion of
    the thread was running for the first time.

Arguments:

    Thread - Supplies a pointer to the thread being reset.

    TrapFrame - Supplies a pointer where the initial thread's trap frame will
        be returned.

Return Value:

    The value that the thread should return when exiting back to user mode.

--*/

{

    ULONG OldSvcLink;
    ULONG OldSvcStackPointer;
    PVOID UserStackPointer;

    UserStackPointer = Thread->ThreadParameter;

    ASSERT((UserStackPointer >= Thread->UserStack) &&
           (UserStackPointer < Thread->UserStack + Thread->UserStackSize));

    OldSvcLink = TrapFrame->SvcLink;
    OldSvcStackPointer = TrapFrame->SvcSp;
    RtlZeroMemory(TrapFrame, sizeof(TRAP_FRAME));
    TrapFrame->SvcLink = OldSvcLink;
    TrapFrame->SvcSp = OldSvcStackPointer;
    TrapFrame->UserSp = (UINTN)UserStackPointer;
    TrapFrame->Cpsr = ARM_MODE_USER;
    TrapFrame->Pc = (UINTN)Thread->ThreadRoutine;
    if ((TrapFrame->Pc & ARM_THUMB_BIT) != 0) {
        TrapFrame->Cpsr |= PSR_FLAG_THUMB;
    }

    if ((Thread->FpuFlags & THREAD_FPU_FLAG_IN_USE) != 0) {
        Thread->FpuFlags &= ~(THREAD_FPU_FLAG_IN_USE | THREAD_FPU_FLAG_OWNER);
        ArDisableFpu();
    }

    //
    // Return the thread parameter so that is gets placed in R0 when the system
    // call returns.
    //

    return (UINTN)Thread->ThreadParameter;
}

KSTATUS
PspArchCloneThread (
    PKTHREAD OldThread,
    PKTHREAD NewThread
    )

/*++

Routine Description:

    This routine performs architecture specific operations upon cloning a
    thread.

Arguments:

    OldThread - Supplies a pointer to the thread being copied.

    NewThread - Supplies a pointer to the newly created thread.

Return Value:

    Status code.

--*/

{

    PFPU_CONTEXT NewContext;
    PFPU_CONTEXT OldContext;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    //
    // Copy the FPU state across, since there are some non-volatile FPU
    // registers across function calls.
    //

    Status = STATUS_SUCCESS;
    if ((OldThread->FpuFlags & THREAD_FPU_FLAG_IN_USE) != 0) {

        ASSERT(OldThread->FpuContext != NULL);

        NewThread->FpuContext =
                           ArAllocateFpuContext(PS_FPU_CONTEXT_ALLOCATION_TAG);

        if (NewThread->FpuContext == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ArchCloneThreadEnd;
        }

        //
        // If it's also the owner, save the latest context into the new. Avoid
        // being pre-empted and losing the FPU context while saving it.
        //

        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        if ((OldThread->FpuFlags & THREAD_FPU_FLAG_OWNER) != 0) {

            ASSERT(KeGetCurrentThread() == OldThread);

            ArSaveFpuState(NewThread->FpuContext);

        //
        // If it's not the owner, copy the latest context into the new
        // structure.
        //

        } else {
            NewContext = (PVOID)(UINTN)ALIGN_RANGE_UP(
                                                  (UINTN)NewThread->FpuContext,
                                                  FPU_CONTEXT_ALIGNMENT);

            OldContext = (PVOID)(UINTN)ALIGN_RANGE_UP(
                                                  (UINTN)OldThread->FpuContext,
                                                  FPU_CONTEXT_ALIGNMENT);

            RtlCopyMemory(NewContext, OldContext, sizeof(FPU_CONTEXT));
        }

        KeLowerRunLevel(OldRunLevel);
        NewThread->FpuFlags |= THREAD_FPU_FLAG_IN_USE;
    }

ArchCloneThreadEnd:
    return Status;
}

KSTATUS
PspArchGetDebugBreakInformation (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine gets the current debug break information.

Arguments:

    TrapFrame - Supplies a pointer to the user mode trap frame that caused the
        break.

Return Value:

    Status code.

--*/

{

    PBREAK_NOTIFICATION Break;
    PPROCESS_DEBUG_DATA DebugData;
    PKPROCESS Process;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    DebugData = Process->DebugData;

    ASSERT(DebugData != NULL);
    ASSERT(DebugData->DebugLeaderThread == Thread);
    ASSERT(DebugData->DebugCommand.Command == DebugCommandGetBreakInformation);
    ASSERT(DebugData->DebugCommand.Size == sizeof(BREAK_NOTIFICATION));

    Break = DebugData->DebugCommand.Data;
    Break->Exception = ExceptionSignal;
    Break->ProcessorOrThreadNumber = Thread->ThreadId;
    Break->ProcessorOrThreadCount = Process->ThreadCount;
    Break->Process = Process->Identifiers.ProcessId;
    Break->ProcessorBlock = (UINTN)NULL;
    Break->ErrorCode = 0;
    Break->LoadedModuleCount = Process->ImageCount;
    Break->LoadedModuleSignature = Process->ImageListSignature;
    Break->InstructionPointer = TrapFrame->Pc;
    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        Break->InstructionPointer |= ARM_THUMB_BIT;
    }

    //
    // Be careful. A trap frame that resulted from a fast system call
    // (before becoming complete for signal dispatching) only contains CPSR,
    // PC, user LR, user SP, and a dummy exception code. The rest is garbage
    // from the kernel mode stack, which shouldn't be leaked to the debugger.
    //

    RtlZeroMemory(Break->InstructionStream, sizeof(Break->InstructionStream));
    MmCopyFromUserMode(Break->InstructionStream,
                       (PVOID)REMOVE_THUMB_BIT(TrapFrame->Pc),
                       ARM_INSTRUCTION_LENGTH);

    Break->Registers.Arm.R15Pc = TrapFrame->Pc;
    Break->Registers.Arm.Cpsr = TrapFrame->Cpsr;
    Break->Registers.Arm.R13Sp = TrapFrame->UserSp;
    Break->Registers.Arm.R14Lr = TrapFrame->UserLink;
    if (ArIsTrapFrameComplete(TrapFrame) != FALSE) {
        Break->Registers.Arm.R0 = TrapFrame->R0;
        Break->Registers.Arm.R1 = TrapFrame->R1;
        Break->Registers.Arm.R2 = TrapFrame->R2;
        Break->Registers.Arm.R3 = TrapFrame->R3;
        Break->Registers.Arm.R4 = TrapFrame->R4;
        Break->Registers.Arm.R5 = TrapFrame->R5;
        Break->Registers.Arm.R6 = TrapFrame->R6;
        Break->Registers.Arm.R7 = TrapFrame->R7;
        Break->Registers.Arm.R8 = TrapFrame->R8;
        Break->Registers.Arm.R9 = TrapFrame->R9;
        Break->Registers.Arm.R10 = TrapFrame->R10;
        Break->Registers.Arm.R11Fp = TrapFrame->R11;
        Break->Registers.Arm.R12Ip = TrapFrame->R12;

    } else {
        Break->Registers.Arm.R0 = 0;
        Break->Registers.Arm.R1 = 0;
        Break->Registers.Arm.R2 = 0;
        Break->Registers.Arm.R3 = 0;
        Break->Registers.Arm.R4 = 0;
        Break->Registers.Arm.R5 = 0;
        Break->Registers.Arm.R6 = 0;
        Break->Registers.Arm.R7 = 0;
        Break->Registers.Arm.R8 = 0;
        Break->Registers.Arm.R9 = 0;
        Break->Registers.Arm.R10 = 0;
        Break->Registers.Arm.R11Fp = 0;
        Break->Registers.Arm.R12Ip = 0;
    }

    return STATUS_SUCCESS;
}

KSTATUS
PspArchSetDebugBreakInformation (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine sets the current debug break information, mostly just the
    register.

Arguments:

    TrapFrame - Supplies a pointer to the user mode trap frame that caused the
        break.

Return Value:

    Status code.

--*/

{

    PBREAK_NOTIFICATION Break;
    PPROCESS_DEBUG_DATA DebugData;
    PKPROCESS Process;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    Process = Thread->OwningProcess;
    DebugData = Process->DebugData;

    ASSERT(DebugData != NULL);
    ASSERT(DebugData->DebugLeaderThread == Thread);
    ASSERT(DebugData->DebugCommand.Command == DebugCommandSetBreakInformation);
    ASSERT(DebugData->DebugCommand.Size == sizeof(BREAK_NOTIFICATION));

    Break = DebugData->DebugCommand.Data;
    TrapFrame->R0 = Break->Registers.Arm.R0;
    TrapFrame->R1 = Break->Registers.Arm.R1;
    TrapFrame->R2 = Break->Registers.Arm.R2;
    TrapFrame->R3 = Break->Registers.Arm.R3;
    TrapFrame->R4 = Break->Registers.Arm.R4;
    TrapFrame->R5 = Break->Registers.Arm.R5;
    TrapFrame->R6 = Break->Registers.Arm.R6;
    TrapFrame->R7 = Break->Registers.Arm.R7;
    TrapFrame->R8 = Break->Registers.Arm.R8;
    TrapFrame->R9 = Break->Registers.Arm.R9;
    TrapFrame->R10 = Break->Registers.Arm.R10;
    TrapFrame->R11 = Break->Registers.Arm.R11Fp;
    TrapFrame->R12 = Break->Registers.Arm.R12Ip;
    TrapFrame->UserSp = Break->Registers.Arm.R13Sp;
    TrapFrame->UserLink = Break->Registers.Arm.R14Lr;
    TrapFrame->Pc = Break->Registers.Arm.R15Pc;
    TrapFrame->Cpsr = (Break->Registers.Arm.Cpsr & ~ARM_MODE_MASK) |
                      ARM_MODE_USER;

    return STATUS_SUCCESS;
}

KSTATUS
PspArchSetOrClearSingleStep (
    PTRAP_FRAME TrapFrame,
    BOOL Set
    )

/*++

Routine Description:

    This routine sets the current thread into single step mode.

Arguments:

    TrapFrame - Supplies a pointer to the user mode trap frame that caused the
        break.

    Set - Supplies a boolean indicating whether to set single step mode (TRUE)
        or clear single step mode (FALSE).

Return Value:

    Status code.

--*/

{

    PVOID Address;
    PVOID BreakingAddress;
    ULONG BreakInstruction;
    PPROCESS_DEBUG_DATA DebugData;
    BOOL FunctionReturning;
    ULONG Length;
    PVOID NextPc;
    PKPROCESS Process;
    KSTATUS Status;

    Process = PsGetCurrentProcess();
    DebugData = Process->DebugData;

    ASSERT(DebugData != NULL);

    Status = STATUS_SUCCESS;
    BreakingAddress = (PVOID)REMOVE_THUMB_BIT(TrapFrame->Pc);
    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        BreakingAddress -= THUMB16_INSTRUCTION_LENGTH;

    } else {
        BreakingAddress -= ARM_INSTRUCTION_LENGTH;
    }

    //
    // Always clear the current single step address if there is one.
    //

    if (DebugData->DebugSingleStepAddress != NULL) {
        Address = (PVOID)REMOVE_THUMB_BIT(
                                   (UINTN)(DebugData->DebugSingleStepAddress));

        if (((UINTN)(DebugData->DebugSingleStepAddress) & ARM_THUMB_BIT) != 0) {
            Length = THUMB16_INSTRUCTION_LENGTH;

        } else {
            Length = ARM_INSTRUCTION_LENGTH;
        }

        //
        // If the debugger broke in because of the single step
        // breakpoint, set the PC back so the correct instruction gets
        // executed.
        //

        if (Address == BreakingAddress) {
            TrapFrame->Pc -= Length;
        }

        Status = MmCopyToUserMode(Address,
                                  &(DebugData->DebugSingleStepOriginalContents),
                                  Length);

        DebugData->DebugSingleStepAddress = NULL;
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Status = MmSyncCacheRegion(Address, Length);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Now set a new one if desired.
    //

    if (Set != FALSE) {

        ASSERT(DebugData->DebugSingleStepAddress == NULL);

        //
        // First determine where to put this new breakpoint.
        //

        Status = ArGetNextPc(TrapFrame,
                             PspArchGetNextPcReadMemory,
                             &FunctionReturning,
                             &NextPc);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Address = (PVOID)REMOVE_THUMB_BIT((UINTN)NextPc);
        if (((UINTN)NextPc & ARM_THUMB_BIT) != 0) {
            BreakInstruction = THUMB_BREAK_INSTRUCTION;
            Length = THUMB16_INSTRUCTION_LENGTH;

        } else {
            BreakInstruction = ARM_BREAK_INSTRUCTION;
            Length = ARM_INSTRUCTION_LENGTH;
        }

        //
        // Read the original contents of memory there so it can be put back.
        //

        Status = MmCopyFromUserMode(
                                 &(DebugData->DebugSingleStepOriginalContents),
                                 Address,
                                 Length);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Write the break instruction in there.
        //

        Status = MmCopyToUserMode(Address, &BreakInstruction, Length);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Status = MmSyncCacheRegion(Address, Length);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        DebugData->DebugSingleStepAddress = NextPc;
        Status = STATUS_SUCCESS;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PspArchGetNextPcReadMemory (
    PVOID Address,
    ULONG Size,
    PVOID Data
    )

/*++

Routine Description:

    This routine attempts to read memory on behalf of the function trying to
    figure out what the next instruction will be.

Arguments:

    Address - Supplies the virtual address that needs to be read.

    Size - Supplies the number of bytes to be read.

    Data - Supplies a pointer to the buffer where the read data will be
        returned on success.

Return Value:

    Status code. STATUS_SUCCESS will only be returned if all the requested
    bytes could be read.

--*/

{

    return MmCopyFromUserMode(Data, Address, Size);
}

