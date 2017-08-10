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
#include <minoca/kernel/x86.h>
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

#define X86_INT_INSTRUCTION_PREFIX 0xCD
#define X86_INT_INSTRUCTION_LENGTH 2
#define X86_CALL_INSTRUCTION_LENGTH 5

//
// Define an intial value for the thread pointer, which is a valid user mode
// GDT entry with offset and limit at zero.
//

#define X86_INITIAL_THREAD_POINTER 0x00CFF2000000FFFFULL

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PspIsSysenterTrapFrame (
    PTRAP_FRAME TrapFrame,
    PBOOL FromSysenter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the initial architecture-specific contents of the thread pointer data
// for a newly created thread.
//

const ULONGLONG PsInitialThreadPointer = X86_INITIAL_THREAD_POINTER;

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
    BOOL InSystemCall
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

    InSystemCall - Supplies a boolean indicating if the trap frame came from
        a system call, in which case this routine will look inside to
        potentially prepare restart information.

Return Value:

    None.

--*/

{

    PSIGNAL_CONTEXT_X86 Context;
    UINTN ContextSp;
    ULONG Flags;
    PSIGNAL_SET RestoreSignals;
    BOOL Result;
    KSTATUS Status;
    INTN SystemCallResult;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    ContextSp = ALIGN_RANGE_DOWN(TrapFrame->Esp - sizeof(SIGNAL_CONTEXT_X86),
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

    //
    // The trap frame had better be complete or else kernel data might be being
    // leaked.
    //

    ASSERT(ArIsTrapFrameComplete(TrapFrame));

    Status |= MmCopyToUserMode(&(Context->TrapFrame),
                               TrapFrame,
                               sizeof(TRAP_FRAME));

    TrapFrame->Esp = ContextSp;
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

    SystemCallResult = (INTN)TrapFrame->Eax;
    if ((InSystemCall != FALSE) &&
        (IS_SYSTEM_CALL_NUMBER_RESTARTABLE(Context->TrapFrame.Ecx)) &&
        (IS_SYSTEM_CALL_RESULT_RESTARTABLE(SystemCallResult) != FALSE)) {

        //
        // If the result indicates that the system call is restartable after a
        // signal is applied, then let user mode know by setting the restart
        // flag in the context.
        //

        if (IS_SYSTEM_CALL_RESULT_RESTARTABLE_AFTER_SIGNAL(SystemCallResult)) {
            Flags |= SIGNAL_CONTEXT_FLAG_RESTART;
        }

        //
        // In case the handler does not allow restarts, convert the saved
        // restart status to the interrupted status.
        //

        MmUserWrite(&(Context->TrapFrame.Eax), STATUS_INTERRUPTED);
    }

    Result &= MmUserWrite32(&(Context->Common.Flags), Flags);
    TrapFrame->Esp -= sizeof(SIGNAL_PARAMETERS);
    Status |= MmCopyToUserMode((PVOID)(TrapFrame->Esp),
                               SignalParameters,
                               sizeof(SIGNAL_PARAMETERS));

    if ((Status != STATUS_SUCCESS) || (Result == FALSE)) {
        PsHandleUserModeFault((PVOID)(TrapFrame->Esp),
                              FAULT_FLAG_WRITE | FAULT_FLAG_PAGE_NOT_PRESENT,
                              TrapFrame,
                              Thread->OwningProcess);

        PsApplyPendingSignals(TrapFrame);
    }

    TrapFrame->Eip = (ULONG)(Thread->OwningProcess->SignalHandlerRoutine);
    TrapFrame->Eflags &= ~IA32_EFLAG_TF;
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

    PSIGNAL_CONTEXT_X86 Context;
    ULONG Eflags;
    ULONG Flags;
    TRAP_FRAME Frame;
    BOOL FromSysenter;
    SIGNAL_SET SignalMask;
    KSTATUS Status;
    PKTHREAD Thread;

    Context = (PSIGNAL_CONTEXT_X86)UserContext;
    Thread = KeGetCurrentThread();
    Status = MmCopyFromUserMode(&Frame,
                                &(Context->TrapFrame),
                                sizeof(TRAP_FRAME));

    Status |= MmCopyFromUserMode(&SignalMask,
                                 &(Context->Common.Mask),
                                 sizeof(SIGNAL_SET));

    if (!MmUserRead(&(UserContext->Flags), &Flags)) {
        Status = STATUS_ACCESS_VIOLATION;
    }

    if (!KSUCCESS(Status)) {
        goto RestorePreSignalTrapFrameEnd;
    }

    PsSetSignalMask(&SignalMask, NULL);

    //
    // Sanitize EFLAGS, ES, and DS. Then copy the whole trap frame.
    //

    Eflags = TrapFrame->Eflags & ~IA32_EFLAG_USER;
    Frame.Eflags = (Frame.Eflags & IA32_EFLAG_USER) | Eflags;
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
    // then back up EIP so that the system call gets executed again when the
    // trap frame gets restored.
    //

    if ((Flags & SIGNAL_CONTEXT_FLAG_RESTART) != 0) {
        Status = PspIsSysenterTrapFrame(TrapFrame, &FromSysenter);
        if (!KSUCCESS(Status)) {
            goto RestorePreSignalTrapFrameEnd;
        }

        //
        // If the saved trap frame was from a full system call (not sysenter),
        // only back up over the INT instruction. When the signal was applied,
        // the system call number and parameter were stored in ECX and EDX.
        //

        if (FromSysenter == FALSE) {
            TrapFrame->Eip -= X86_INT_INSTRUCTION_LENGTH;

        //
        // On sysenter trap frames, EIP points to the instruction after the
        // dummy call to OspSysenter. Back up over that, which will replay
        // setting up the arguments for sysenter.
        //

        } else {
            TrapFrame->Eip -= X86_CALL_INSTRUCTION_LENGTH;
        }
    }

RestorePreSignalTrapFrameEnd:
    if (!KSUCCESS(Status)) {
        PsSignalThread(Thread, SIGNAL_ACCESS_VIOLATION, NULL, TRUE);
    }

    //
    // Preserve EAX by returning it. The system call assembly return path
    // guarantees this.
    //

    return (INTN)TrapFrame->Eax;
}

VOID
PspArchRestartSystemCall (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine determines whether or not a system call needs to be restarted.
    If so, it modifies the given trap frame such that the system call return
    to user mode will fall right back into calling the system call.

Arguments:

    TrapFrame - Supplies a pointer to the full trap frame saved by a system
        call in order to attempt dispatching a signal.

Return Value:

    None.

--*/

{

    BOOL FromSysenter;
    KSTATUS Status;

    //
    // On x86, the trap frame holds the system call return value in EAX. Check
    // to see if the system call can be restarted. If not, exit.
    //

    if (!IS_SYSTEM_CALL_NUMBER_RESTARTABLE(TrapFrame->Ecx) ||
        !IS_SYSTEM_CALL_RESULT_RESTARTABLE_NO_SIGNAL((INTN)TrapFrame->Eax)) {

        return;
    }

    //
    // Attempt to determine if this trap frame was created by sysenter. If this
    // fails, then signal the thread and dispatch the new signal. This restart
    // call is likely already in the middle of dispatching signals and found
    // there were none.
    //

    Status = PspIsSysenterTrapFrame(TrapFrame, &FromSysenter);
    if (!KSUCCESS(Status)) {
        PsSignalThread(KeGetCurrentThread(),
                       SIGNAL_ACCESS_VIOLATION,
                       NULL,
                       TRUE);

        PsApplyPendingSignals(TrapFrame);
        return;
    }

    //
    // If the trap frame was from a full system call (not sysenter), only back
    // up over the INT instruction.
    //

    if (FromSysenter == FALSE) {
        TrapFrame->Eip -= X86_INT_INSTRUCTION_LENGTH;

    //
    // A sysenter trap frame's EIP points to the instruction after the dummy
    // call to OspSysenter. Back up over that.
    //

    } else {
        TrapFrame->Eip -= X86_CALL_INSTRUCTION_LENGTH;
    }

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
    ULONG Eip;
    ULONG Fs;
    PUINTN StackPointer;
    PTRAP_FRAME StackTrapFrame;
    UINTN TrapStackPointer;
    PUINTN UserStackPointer;

    TrapStackPointer = (UINTN)Thread->KernelStack + Thread->KernelStackSize -
                       sizeof(PVOID);

    StackPointer = (PUINTN)TrapStackPointer;

    //
    // Determine the appropriate value for the flags, code selector, and entry
    // point.
    //

    if ((Thread->Flags & THREAD_FLAG_USER_MODE) != 0) {

        ASSERT((TrapFrame == NULL) || (ParameterIsStack == FALSE));

        //
        // Set up the values on the user mode stack. Push the parameter and a
        // dummy return address.
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

            MmUserWrite(UserStackPointer, (UINTN)Thread->ThreadParameter);
            UserStackPointer -= 1;
            MmUserWrite(UserStackPointer, 0);
            TrapStackPointer = (UINTN)UserStackPointer;
        }

        //
        // Set the variables that will be used to set up the kernel stack.
        //

        CodeSelector = USER32_CS;
        DataSelector = USER_DS;
        Eip = (UINTN)Thread->ThreadRoutine;
        Fs = DataSelector;

        //
        // Make room for SS ESP (in that order), as they're part of the
        // hardware trap frame when returning to user mode. Don't worry about
        // filling them out, the restore trap frame function will handle that.
        //

        StackPointer -= 2;

    } else {
        CodeSelector = KERNEL_CS;
        DataSelector = KERNEL_DS;
        Fs = GDT_PROCESSOR;
        Eip = (UINTN)PspKernelThreadStart;
    }

    //
    // Make room for Eflags, CS, and EIP, and a dummy error code expected by the
    // restore trap frame code.
    //

    StackPointer -= 4;

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

            StackTrapFrame->Eax = 0;

        } else {

            //
            // User mode tried to pull a fast one by forking with the fast
            // system call handler path. Joke's on them; zero out the registers
            // that didn't get saved.
            //

            RtlZeroMemory(StackTrapFrame, sizeof(TRAP_FRAME));
            StackTrapFrame->Eip = TrapFrame->Eip;
            StackTrapFrame->Esp = TrapFrame->Esp;
        }

    } else {
        RtlZeroMemory(StackTrapFrame, sizeof(TRAP_FRAME));
        StackTrapFrame->Eip = Eip;
        StackTrapFrame->Esp = TrapStackPointer;
        StackTrapFrame->Ecx = (UINTN)Thread->ThreadParameter;
    }

    StackTrapFrame->Ds = DataSelector;
    StackTrapFrame->Es = DataSelector;
    StackTrapFrame->Fs = Fs;
    StackTrapFrame->Gs = GDT_THREAD;
    StackTrapFrame->Ss = DataSelector;
    StackTrapFrame->Cs = CodeSelector;
    StackTrapFrame->Eflags = IA32_EFLAG_ALWAYS_1 | IA32_EFLAG_IF;
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

    Thread->ThreadPointer = PsInitialThreadPointer;
    UserStackPointer = Thread->ThreadParameter - sizeof(PVOID);

    ASSERT(((PVOID)UserStackPointer >= Thread->UserStack) &&
           ((PVOID)UserStackPointer <
            Thread->UserStack + Thread->UserStackSize));

    MmUserWrite(UserStackPointer, (UINTN)(Thread->ThreadParameter));
    UserStackPointer -= 1;
    MmUserWrite(UserStackPointer, 0);
    RtlZeroMemory(TrapFrame, sizeof(TRAP_FRAME));
    TrapFrame->Cs = USER32_CS;
    TrapFrame->Ds = USER_DS;
    TrapFrame->Es = USER_DS;
    TrapFrame->Fs = USER_DS;
    TrapFrame->Gs = GDT_THREAD;
    TrapFrame->Ss = USER_DS;
    TrapFrame->Eip = (UINTN)Thread->ThreadRoutine;
    TrapFrame->Eflags = IA32_EFLAG_ALWAYS_1 | IA32_EFLAG_IF;
    TrapFrame->Esp = (UINTN)UserStackPointer;
    TrapFrame->Ecx = (UINTN)Thread->ThreadParameter;
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

    //
    // Be careful. A trap frame that resulted from a sysenter (before becoming
    // complete for signal dispatching) only contains EIP and ESP. The rest is
    // just garbage from the kernel mode stack, which shouldn't be leaked to
    // the debugger.
    //

    Break->InstructionPointer = TrapFrame->Eip;
    RtlZeroMemory(Break->InstructionStream, sizeof(Break->InstructionStream));
    MmCopyFromUserMode(Break->InstructionStream,
                       (PVOID)TrapFrame->Eip,
                       sizeof(Break->InstructionStream));

    Break->Registers.X86.Eip = TrapFrame->Eip;
    Break->Registers.X86.Esp = TrapFrame->Esp;
    Break->Registers.X86.Ecx = TrapFrame->Ecx;
    Break->Registers.X86.Edx = TrapFrame->Edx;
    if (ArIsTrapFrameComplete(TrapFrame) != FALSE) {
        Break->ErrorCode = TrapFrame->ErrorCode;
        Break->Registers.X86.Eax = TrapFrame->Eax;
        Break->Registers.X86.Ebx = TrapFrame->Ebx;
        Break->Registers.X86.Ebp = TrapFrame->Ebp;
        Break->Registers.X86.Esi = TrapFrame->Esi;
        Break->Registers.X86.Edi = TrapFrame->Edi;
        Break->Registers.X86.Eflags = TrapFrame->Eflags;
        Break->Registers.X86.Cs = TrapFrame->Cs;
        Break->Registers.X86.Ds = TrapFrame->Ds;
        Break->Registers.X86.Es = TrapFrame->Es;
        Break->Registers.X86.Fs = TrapFrame->Fs;
        Break->Registers.X86.Gs = TrapFrame->Gs;
        Break->Registers.X86.Ss = TrapFrame->Ss;

    } else {
        Break->ErrorCode = 0;
        Break->Registers.X86.Eax = 0;
        Break->Registers.X86.Ebx = 0;
        Break->Registers.X86.Ebp = 0;
        Break->Registers.X86.Esi = 0;
        Break->Registers.X86.Edi = 0;
        Break->Registers.X86.Eflags = 0;
        Break->Registers.X86.Cs = USER32_CS;
        Break->Registers.X86.Ds = USER_DS;
        Break->Registers.X86.Es = USER_DS;
        Break->Registers.X86.Fs = GDT_THREAD;
        Break->Registers.X86.Gs = GDT_THREAD;
        Break->Registers.X86.Ss = USER_DS;
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
    if ((!VALID_USER_SEGMENT(Break->Registers.X86.Cs)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X86.Ds)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X86.Es)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X86.Fs)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X86.Gs)) ||
        (!VALID_USER_SEGMENT(Break->Registers.X86.Ss))) {

        return STATUS_INVALID_PARAMETER;
    }

    TrapFrame->Eax = Break->Registers.X86.Eax;
    TrapFrame->Ebx = Break->Registers.X86.Ebx;
    TrapFrame->Ecx = Break->Registers.X86.Ecx;
    TrapFrame->Edx = Break->Registers.X86.Edx;
    TrapFrame->Ebp = Break->Registers.X86.Ebp;
    TrapFrame->Esp = Break->Registers.X86.Esp;
    TrapFrame->Esi = Break->Registers.X86.Esi;
    TrapFrame->Edi = Break->Registers.X86.Edi;
    TrapFrame->Eip = Break->Registers.X86.Eip;
    TrapFrame->Eflags = Break->Registers.X86.Eflags |
                        IA32_EFLAG_ALWAYS_1 | IA32_EFLAG_IF;

    TrapFrame->Cs = Break->Registers.X86.Cs | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Ds = Break->Registers.X86.Ds | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Es = Break->Registers.X86.Es | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Fs = Break->Registers.X86.Fs | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Gs = Break->Registers.X86.Gs | SEGMENT_PRIVILEGE_USER;
    TrapFrame->Ss = Break->Registers.X86.Ss | SEGMENT_PRIVILEGE_USER;
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
        TrapFrame->Eflags |= IA32_EFLAG_TF;

    } else {
        TrapFrame->Eflags &= ~IA32_EFLAG_TF;
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PspIsSysenterTrapFrame (
    PTRAP_FRAME TrapFrame,
    PBOOL FromSysenter
    )

/*++

Routine Description:

    This routine determines whether or not the given trap frame was created by
    sysenter.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame whose origin is in
        question.

    FromSysenter - Supplies a pointer to a boolean that receives whether or not
        the trap frame came from sysenter.

Return Value:

    TRUE if the trap frame was created by sysenter. FALSE otherwise.

--*/

{

    UCHAR Instruction;
    PVOID PreviousInstructionPointer;
    BOOL Result;

    ASSERT(IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) == FALSE);

    PreviousInstructionPointer = (PVOID)TrapFrame->Eip -
                                 X86_INT_INSTRUCTION_LENGTH;

    Result = MmUserRead8(PreviousInstructionPointer, &Instruction);
    if (Result == FALSE) {
        return STATUS_ACCESS_VIOLATION;
    }

    *FromSysenter = FALSE;
    if (Instruction != X86_INT_INSTRUCTION_PREFIX) {
        *FromSysenter = TRUE;
    }

    return STATUS_SUCCESS;
}

