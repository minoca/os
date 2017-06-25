/*++

Copyright (c) 2017 Minoca Corp.

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

    Evan Green 11-Jun-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x64.h>
#include <minoca/debug/dbgproto.h>
#include "../psp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns non-zero if the given segment descriptor is a valid
// user segment selector.
//

#define VALID_USER_SEGMENT(_Segment)                        \
    (((_Segment) & SEGMENT_PRIVILEGE_USER) == SEGMENT_PRIVILEGE_USER)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the length of both the int $N instruction and the syscall instruction.
//

#define X86_SYSCALL_INSTRUCTION_LENGTH 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

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

    PSIGNAL_CONTEXT_X64 Context;
    UINTN ContextSp;
    ULONG Flags;
    PSIGNAL_SET RestoreSignals;
    BOOL Result;
    KSTATUS Status;
    INTN SystemCallResult;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();

    //
    // TODO: Check signal apply for x64. Alignment? Can signal handling be
    // all C?
    //

    ContextSp = TrapFrame->Rsp - X64_RED_ZONE - sizeof(SIGNAL_CONTEXT_X64);
    ContextSp = ALIGN_RANGE_DOWN(ContextSp, FPU_CONTEXT_ALIGNMENT);
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

    ASSERT(ArIsTrapFrameComplete(TrapFrame) != FALSE);

    Status |= MmCopyToUserMode(&(Context->TrapFrame),
                               TrapFrame,
                               sizeof(TRAP_FRAME));

    TrapFrame->Rsp = ContextSp;
    if ((Thread->FpuFlags & THREAD_FPU_FLAG_IN_USE) != 0) {
        Flags |= SIGNAL_CONTEXT_FLAG_FPU_VALID;
        if ((Thread->FpuFlags & THREAD_FPU_FLAG_OWNER) != 0) {
            ArSaveFpuState(Thread->FpuContext);
        }

        Status |= MmCopyToUserMode(&(Context->FpuContext),
                                   Thread->FpuContext,
                                   sizeof(FPU_CONTEXT));
    }

    //
    // If this signal is being applied in the middle of a system call, the trap
    // frame needs modification if it is restartable. EAX holds the system call
    // result.
    //

    SystemCallResult = (INTN)TrapFrame->Rax;
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
            MmUserWrite(&(Context->TrapFrame.Rdi), SystemCallNumber);
            MmUserWrite(&(Context->TrapFrame.Rsi), (UINTN)SystemCallParameter);
        }

        //
        // In case the handler does not allow restarts, convert the saved
        // restart status to the interrupted status.
        //

        MmUserWrite(&(Context->TrapFrame.Rax), STATUS_INTERRUPTED);
    }

    Result &= MmUserWrite32(&(Context->Common.Flags), Flags);
    TrapFrame->Rsp -= sizeof(SIGNAL_PARAMETERS);
    Status |= MmCopyToUserMode((PVOID)(TrapFrame->Rsp),
                               SignalParameters,
                               sizeof(SIGNAL_PARAMETERS));

    if ((Status != STATUS_SUCCESS) || (Result == FALSE)) {
        PsHandleUserModeFault((PVOID)(TrapFrame->Rsp),
                              FAULT_FLAG_WRITE | FAULT_FLAG_PAGE_NOT_PRESENT,
                              TrapFrame,
                              Thread->OwningProcess);

        PsDispatchPendingSignalsOnCurrentThread(TrapFrame,
                                                SystemCallNumber,
                                                SystemCallParameter);
    }

    TrapFrame->Rip = (UINTN)(Thread->OwningProcess->SignalHandlerRoutine);
    TrapFrame->Rflags &= ~IA32_EFLAG_TF;
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

    PSIGNAL_CONTEXT_X64 Context;
    ULONG Flags;
    TRAP_FRAME Frame;
    UINTN Rflags;
    SIGNAL_SET SignalMask;
    KSTATUS Status;
    PKTHREAD Thread;

    Context = (PSIGNAL_CONTEXT_X64)UserContext;
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
    // Sanitize RFLAGS, ES, and DS. Then copy the whole trap frame.
    //

    Rflags = TrapFrame->Rflags & ~IA32_EFLAG_USER;
    Frame.Rflags = (Frame.Rflags & IA32_EFLAG_USER) | Rflags;
    Frame.Ds = USER_DS;
    Frame.Es = USER_DS;
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
    // If the signal context indicates that a system call restart is necessary,
    // then back up RIP so that the system call gets executed again when the
    // trap frame gets restored. Both int $N and syscall instructions are two
    // bytes long, so there's no need to distinguish.
    //

    if ((Flags & SIGNAL_CONTEXT_FLAG_RESTART) != 0) {
        TrapFrame->Rip -= X86_SYSCALL_INSTRUCTION_LENGTH;
    }

RestorePreSignalTrapFrameEnd:
    if (!KSUCCESS(Status)) {
        PsSignalThread(Thread, SIGNAL_ACCESS_VIOLATION, NULL, TRUE);
    }

    //
    // Preserve RAX by returning it. The system call assembly return path
    // guarantees this.
    //

    return (INTN)TrapFrame->Rax;
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
    // On x86, the trap frame holds the system call return value in RAX. Check
    // to see if the system call can be restarted. If not, exit.
    //

    if (!IS_SYSTEM_CALL_NUMBER_RESTARTABLE(SystemCallNumber) ||
        !IS_SYSTEM_CALL_RESULT_RESTARTABLE_NO_SIGNAL((INTN)TrapFrame->Rax)) {

        return;
    }

    //
    // Back up over the syscall or int $N instruction, and reset the
    // number/parameter to restart the call.
    //

    TrapFrame->Rdi = SystemCallNumber;
    TrapFrame->Rsi = (UINTN)SystemCallParameter;
    TrapFrame->Rip -= X86_SYSCALL_INSTRUCTION_LENGTH;
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

    ULONG CodeSelector;
    ULONG DataSelector;
    UINTN Rip;
    PUINTN StackPointer;
    PTRAP_FRAME StackTrapFrame;
    UINTN TrapStackPointer;
    PUINTN UserStackPointer;

    TrapStackPointer = (UINTN)Thread->KernelStack + Thread->KernelStackSize;
    StackPointer = (PUINTN)TrapStackPointer;

    //
    // Determine the appropriate value for the flags, code selector, and entry
    // point.
    //

    if ((Thread->Flags & THREAD_FLAG_USER_MODE) != 0) {

        ASSERT((TrapFrame == NULL) || (ParameterIsStack == FALSE));

        //
        // Set up the values on the user mode stack. Push a dummy return
        // address.
        //

        if (TrapFrame == NULL) {
            if (ParameterIsStack != FALSE) {
                UserStackPointer = Thread->ThreadParameter - sizeof(PVOID);

                ASSERT(((PVOID)UserStackPointer >= Thread->UserStack) &&
                       ((PVOID)UserStackPointer <
                        Thread->UserStack + Thread->UserStackSize));

            } else {
                UserStackPointer = Thread->UserStack + Thread->UserStackSize -
                                   sizeof(PVOID);
            }

            MmUserWrite(UserStackPointer, 0);
            TrapStackPointer = (UINTN)UserStackPointer;
        }

        //
        // Set the variables that will be used to set up the kernel stack.
        //

        CodeSelector = USER_CS;
        DataSelector = USER_DS;
        Rip = (UINTN)Thread->ThreadRoutine;

    } else {
        CodeSelector = KERNEL_CS;
        DataSelector = KERNEL_DS;
        Rip = (UINTN)PspKernelThreadStart;
    }

    //
    // Make room for a trap frame to be restored.
    //

    StackPointer = (PUINTN)((PUCHAR)StackPointer - sizeof(TRAP_FRAME));
    StackTrapFrame = (PTRAP_FRAME)StackPointer;
    if (TrapFrame != NULL) {
        if (ArIsTrapFrameComplete(TrapFrame) != FALSE) {
            RtlCopyMemory(StackTrapFrame, TrapFrame, sizeof(TRAP_FRAME));

            //
            // Return a process ID of 0 to the child on fork.
            //

            StackTrapFrame->Rax = 0;

        } else {

            //
            // User mode tried to pull a fast one by forking with the fast
            // system call handler path. Joke's on them; zero out the registers
            // that didn't get saved.
            //

            RtlZeroMemory(StackTrapFrame, sizeof(TRAP_FRAME));
            StackTrapFrame->Rip = TrapFrame->Rip;
            StackTrapFrame->Rsp = TrapFrame->Rsp;
        }

    } else {
        RtlZeroMemory(StackTrapFrame, sizeof(TRAP_FRAME));
        StackTrapFrame->Rip = Rip;
        StackTrapFrame->Rsp = TrapStackPointer;
        StackTrapFrame->Rdi = (UINTN)Thread->ThreadParameter;
    }

    StackTrapFrame->Ds = DataSelector;
    StackTrapFrame->Es = DataSelector;
    StackTrapFrame->Fs = DataSelector;
    StackTrapFrame->Gs = DataSelector;
    StackTrapFrame->Ss = DataSelector;
    StackTrapFrame->Cs = CodeSelector;
    StackTrapFrame->Rflags = IA32_EFLAG_ALWAYS_1 | IA32_EFLAG_IF;
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

    PUINTN UserStackPointer;

    Thread->ThreadPointer = 0;
    UserStackPointer = Thread->ThreadParameter - sizeof(PVOID);

    ASSERT(((PVOID)UserStackPointer >= Thread->UserStack) &&
           ((PVOID)UserStackPointer <
            Thread->UserStack + Thread->UserStackSize));

    MmUserWrite(UserStackPointer, 0);
    RtlZeroMemory(TrapFrame, sizeof(TRAP_FRAME));
    TrapFrame->Cs = USER_CS;
    TrapFrame->Ds = USER_DS;
    TrapFrame->Es = USER_DS;
    TrapFrame->Fs = USER_DS;
    TrapFrame->Gs = USER_DS;
    TrapFrame->Ss = USER_DS;
    TrapFrame->Rip = (UINTN)Thread->ThreadRoutine;
    TrapFrame->Rflags = IA32_EFLAG_ALWAYS_1 | IA32_EFLAG_IF;
    TrapFrame->Rsp = (UINTN)UserStackPointer;
    TrapFrame->Rdi = (UINTN)Thread->ThreadParameter;
    if ((Thread->FpuFlags & THREAD_FPU_FLAG_IN_USE) != 0) {
        Thread->FpuFlags &= ~(THREAD_FPU_FLAG_IN_USE | THREAD_FPU_FLAG_OWNER);
        ArDisableFpu();
    }

    //
    // Return 0 as this will make its way to EAX when the system call returns.
    //

    return 0;
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

    return STATUS_SUCCESS;
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
    Break->LoadedModuleCount = Process->ImageCount;
    Break->LoadedModuleSignature = Process->ImageListSignature;
    Break->InstructionPointer = TrapFrame->Rip;
    RtlZeroMemory(Break->InstructionStream, sizeof(Break->InstructionStream));
    MmCopyFromUserMode(Break->InstructionStream,
                       (PVOID)TrapFrame->Rip,
                       sizeof(Break->InstructionStream));

    if (ArIsTrapFrameComplete(TrapFrame) != FALSE) {
        Break->ErrorCode = TrapFrame->ErrorCode;
        Break->Registers.X64.Rax = TrapFrame->Rax;
        Break->Registers.X64.Rbx = TrapFrame->Rbx;
        Break->Registers.X64.Rcx = TrapFrame->Rcx;
        Break->Registers.X64.Rdx = TrapFrame->Rdx;
        Break->Registers.X64.Rbp = TrapFrame->Rbp;
        Break->Registers.X64.Rsi = TrapFrame->Rsi;
        Break->Registers.X64.Rdi = TrapFrame->Rdi;
        Break->Registers.X64.R8 = TrapFrame->R8;
        Break->Registers.X64.R9 = TrapFrame->R9;
        Break->Registers.X64.R10 = TrapFrame->R10;
        Break->Registers.X64.R11 = TrapFrame->R11;
        Break->Registers.X64.R12 = TrapFrame->R12;
        Break->Registers.X64.R13 = TrapFrame->R13;
        Break->Registers.X64.R14 = TrapFrame->R14;
        Break->Registers.X64.R15 = TrapFrame->R15;
        Break->Registers.X64.Rflags = TrapFrame->Rflags;
        Break->Registers.X64.Cs = TrapFrame->Cs;
        Break->Registers.X64.Ds = TrapFrame->Ds;
        Break->Registers.X64.Es = TrapFrame->Es;
        Break->Registers.X64.Fs = TrapFrame->Fs;
        Break->Registers.X64.Gs = TrapFrame->Gs;
        Break->Registers.X64.Ss = TrapFrame->Ss;

    } else {
        RtlZeroMemory(&(Break->Registers.X64), sizeof(Break->Registers.X64));
        Break->ErrorCode = 0;
        Break->Registers.X64.Cs = USER_CS;
        Break->Registers.X64.Ds = USER_DS;
        Break->Registers.X64.Es = USER_DS;
        Break->Registers.X64.Fs = USER_DS;
        Break->Registers.X64.Gs = USER_DS;
        Break->Registers.X64.Ss = USER_DS;
    }

    Break->Registers.X64.Rip = TrapFrame->Rip;
    Break->Registers.X64.Rsp = TrapFrame->Rsp;
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
    if ((!VALID_USER_SEGMENT(Break->Registers.X64.Cs)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X64.Ds)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X64.Es)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X64.Fs)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X64.Gs)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X64.Ss))) {

        return STATUS_INVALID_PARAMETER;
    }

    TrapFrame->Rax = Break->Registers.X64.Rax;
    TrapFrame->Rbx = Break->Registers.X64.Rbx;
    TrapFrame->Rcx = Break->Registers.X64.Rcx;
    TrapFrame->Rdx = Break->Registers.X64.Rdx;
    TrapFrame->Rbp = Break->Registers.X64.Rbp;
    TrapFrame->Rsp = Break->Registers.X64.Rsp;
    TrapFrame->Rsi = Break->Registers.X64.Rsi;
    TrapFrame->Rdi = Break->Registers.X64.Rdi;
    TrapFrame->R8 = Break->Registers.X64.R8;
    TrapFrame->R9 = Break->Registers.X64.R9;
    TrapFrame->R10 = Break->Registers.X64.R10;
    TrapFrame->R11 = Break->Registers.X64.R11;
    TrapFrame->R12 = Break->Registers.X64.R12;
    TrapFrame->R13 = Break->Registers.X64.R13;
    TrapFrame->R14 = Break->Registers.X64.R14;
    TrapFrame->R15 = Break->Registers.X64.R15;
    TrapFrame->Rip = Break->Registers.X64.Rip;
    TrapFrame->Rflags = (Break->Registers.X64.Rflags & IA32_EFLAG_USER) |
                        IA32_EFLAG_ALWAYS_1 | IA32_EFLAG_IF;

    TrapFrame->Cs = Break->Registers.X64.Cs | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Ds = Break->Registers.X64.Ds | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Es = Break->Registers.X64.Es | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Fs = Break->Registers.X64.Fs | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Gs = Break->Registers.X64.Gs | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Ss = Break->Registers.X64.Ss | SEGMENT_PRIVILEGE_USER;
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

    PKPROCESS Process;

    Process = PsGetCurrentProcess();

    ASSERT(Process != PsGetKernelProcess());
    ASSERT(IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) == FALSE);

    if (Set != FALSE) {
        TrapFrame->Rflags |= IA32_EFLAG_TF;

    } else {
        TrapFrame->Rflags &= ~IA32_EFLAG_TF;
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

