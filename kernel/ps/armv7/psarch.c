/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

ULONGLONG PsInitialThreadPointer = 0;

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
        SignalNumber = PspDequeuePendingSignal(SignalParameters, TrapFrame);
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
    INTN SystemCallResult,
    PVOID SystemCallParameter,
    ULONG SystemCallNumber
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

    SystemCallResult - Supplies the result of the system call that is
        attempting to dispatch a pending signal. This is only valid if the
        system call number is valid.

    SystemCallParameter - Supplies a pointer to the parameters of the system
        call that is attempting to dispatch a pending signal. This is a pointer
        to user mode. This is only valid if the system call number if valid.

    SystemCallNumber - Supplies the number of the system call that is
        attempting to dispatch a pending signal. Supplied SystemCallInvalid if
        the caller is not a system call.

Return Value:

    None.

--*/

{

    SIGNAL_CONTEXT_ARM Context;
    KSTATUS Status;
    KSTATUS Status2;
    PKTHREAD Thread;

    ASSERT(!IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame));

    Thread = KeGetCurrentThread();

    //
    // The trap frame may be incomplete if the thread is currently in a system
    // call. Volatile registers don't matter in this case.
    //

    RtlZeroMemory(&Context, sizeof(SIGNAL_CONTEXT_ARM));
    Context.Common.Signal = SignalParameters->SignalNumber;
    Context.Pc = TrapFrame->Pc;
    Context.Sp = TrapFrame->UserSp;
    Context.Lr = TrapFrame->UserLink;
    Context.Cpsr = TrapFrame->Cpsr;
    if (ArIsTrapFrameComplete(TrapFrame) != FALSE) {
        Context.R0 = TrapFrame->R0;
        Context.R1 = TrapFrame->R1;
        Context.R2 = TrapFrame->R2;
        Context.R3 = TrapFrame->R3;
        Context.R12 = TrapFrame->R12;
    }

    //
    // If this signal is being applied in the middle of a system call, preserve
    // the system call's return value.
    //

    if (SystemCallNumber != SystemCallInvalid) {

        //
        // If the result indicates that the system call is restartable, then
        // let user mode know by setting the restart flag in the context and
        // save the system call number and parameters in volatile registers.
        // This should not be done for restore context or execute image, as
        // their result values are unsigned and may overlap with the restart
        // system call status. Convert the system call restart return value
        // into an interrupted status in case the signal call handler does not
        // allow restarting.
        //

        if (IS_SYSTEM_CALL_RESTARTABLE(SystemCallNumber, SystemCallResult)) {
            Context.R0 = STATUS_INTERRUPTED;
            Context.R1 = SystemCallNumber;
            Context.R2 = (ULONG)SystemCallParameter;
            Context.Common.Flags |= SIGNAL_CONTEXT_FLAG_RESTART;

        //
        // Otherwise just preserve the system call result.
        //

        } else {
            Context.R0 = SystemCallResult;
        }
    }

    //
    // Mark the signal as running so that more don't come down on the thread
    // while it's servicing this one.
    //

    ASSERT(!IS_SIGNAL_SET(Thread->RunningSignals, Context.Common.Signal));

    ADD_SIGNAL(Thread->RunningSignals, Context.Common.Signal);

    //
    // Copy the signal frame onto the stack.
    // TODO: Support an alternate signal stack.
    //

    TrapFrame->UserSp = ALIGN_RANGE_DOWN(TrapFrame->UserSp, STACK_ALIGNMENT);
    TrapFrame->UserSp -= sizeof(SIGNAL_CONTEXT_ARM);
    Status = MmCopyToUserMode((PVOID)(TrapFrame->UserSp),
                              &Context,
                              sizeof(SIGNAL_CONTEXT_ARM));

    TrapFrame->UserSp -= sizeof(SIGNAL_PARAMETERS);
    Status2 = MmCopyToUserMode((PVOID)(TrapFrame->UserSp),
                               SignalParameters,
                               sizeof(SIGNAL_PARAMETERS));

    if ((!KSUCCESS(Status)) || (!KSUCCESS(Status2))) {
        PsHandleUserModeFault((PVOID)(TrapFrame->UserSp),
                              FAULT_FLAG_WRITE | FAULT_FLAG_PAGE_NOT_PRESENT,
                              TrapFrame,
                              Thread->OwningProcess);

        PsDispatchPendingSignalsOnCurrentThread(TrapFrame,
                                                SystemCallResult,
                                                SystemCallParameter,
                                                SystemCallNumber);

        return;
    }

    TrapFrame->Pc = (ULONG)(Thread->OwningProcess->SignalHandlerRoutine);
    TrapFrame->Cpsr = ARM_MODE_USER;
    if ((TrapFrame->Pc & ARM_THUMB_BIT) != 0) {
        TrapFrame->Cpsr |= PSR_FLAG_THUMB;
    }

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

    SIGNAL_CONTEXT_ARM Context;
    INTN Result;
    KSTATUS Status;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    Status = MmCopyFromUserMode(&Context,
                                (PSIGNAL_CONTEXT_ARM)UserContext,
                                sizeof(SIGNAL_CONTEXT_ARM));

    if (!KSUCCESS(Status)) {
        PsSignalThread(Thread, SIGNAL_ACCESS_VIOLATION, NULL, TRUE);
        return 0;
    }

    ASSERT((Context.Cpsr & ARM_MODE_MASK) == ARM_MODE_USER);

    REMOVE_SIGNAL(Thread->RunningSignals, Context.Common.Signal);
    TrapFrame->R0 = Context.R0;
    TrapFrame->R1 = Context.R1;
    TrapFrame->R2 = Context.R2;
    TrapFrame->R3 = Context.R3;
    TrapFrame->R12 = Context.R12;
    TrapFrame->UserSp = Context.Sp;
    TrapFrame->UserLink = Context.Lr;
    TrapFrame->Pc = Context.Pc;
    TrapFrame->Cpsr = Context.Cpsr;
    Result = TrapFrame->R0;

    //
    // If the signal context indicates that a restart is necessary, then fire
    // off the system call again. The trap frame must be restored before the
    // restart or else the context saved on application of another signal would
    // cause the next restore to return to the signal handler.
    //

    if ((Context.Common.Flags & SIGNAL_CONTEXT_FLAG_RESTART) != 0) {
        Result = KeSystemCallHandler(Context.R1, (PVOID)Context.R2, TrapFrame);
    }

    return Result;
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
    PULONG StackPointer;
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
    TrapFrame->R0 = (UINTN)Thread->ThreadParameter;
    TrapFrame->Cpsr = ARM_MODE_USER;
    TrapFrame->Pc = (UINTN)Thread->ThreadRoutine;
    if ((TrapFrame->Pc & ARM_THUMB_BIT) != 0) {
        TrapFrame->Cpsr |= PSR_FLAG_THUMB;
    }

    if ((Thread->FpuFlags & THREAD_FPU_FLAG_IN_USE) != 0) {
        Thread->FpuFlags &= ~(THREAD_FPU_FLAG_IN_USE | THREAD_FPU_FLAG_OWNER);
        ArDisableFpu();
    }

    return TrapFrame->R0;
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

    RtlZeroMemory(Break->InstructionStream, sizeof(Break->InstructionStream));
    MmCopyFromUserMode(Break->InstructionStream,
                       (PVOID)REMOVE_THUMB_BIT(TrapFrame->Pc),
                       ARM_INSTRUCTION_LENGTH);

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
    Break->Registers.Arm.R13Sp = TrapFrame->UserSp;
    Break->Registers.Arm.R14Lr = TrapFrame->UserLink;
    Break->Registers.Arm.R15Pc = TrapFrame->Pc;
    Break->Registers.Arm.Cpsr = TrapFrame->Cpsr;
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

