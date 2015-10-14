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

#include <minoca/kernel.h>
#include <minoca/x86.h>
#include <minoca/dbgproto.h>
#include "../processp.h"

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

#define X86_SYSENTER_INSTRUCTION 0x340F
#define X86_SYSENTER_INSTRUCTION_LENGTH 2

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

//
// -------------------------------------------------------------------- Globals
//

//
// Define the initial architecture-specific contents of the thread pointer data
// for a newly created thread.
//

ULONGLONG PsInitialThreadPointer = X86_INITIAL_THREAD_POINTER;

//
// ------------------------------------------------------------------ Functions
//

ULONG
PsDispatchPendingSignalsOnCurrentThread (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine dispatches any pending signals that should be run on the
    current thread.

Arguments:

    TrapFrame - Supplies a pointer to the current trap frame. If this trap frame
        is not destined for user mode, this function exits immediately.

Return Value:

    Returns a signal number if a signal was queued.

    -1 if no signal was dispatched.

--*/

{

    BOOL SignalHandled;
    ULONG SignalNumber;
    SIGNAL_PARAMETERS SignalParameters;

    //
    // If the trap frame is not destined for user mode, then forget it.
    //

    if (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) != FALSE) {
        return -1;
    }

    do {
        SignalNumber = PspDequeuePendingSignal(&SignalParameters, TrapFrame);
        if (SignalNumber == -1) {
            return -1;
        }

        SignalHandled = PspSignalAttemptDefaultProcessing(SignalNumber);

    } while (SignalHandled != FALSE);

    PsApplySynchronousSignal(TrapFrame, &SignalParameters);
    return SignalNumber;
}

VOID
PsApplySynchronousSignal (
    PTRAP_FRAME TrapFrame,
    PSIGNAL_PARAMETERS SignalParameters
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

Return Value:

    None.

--*/

{

    PKTHREAD Thread;

    ASSERT(IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame) == FALSE);

    Thread = KeGetCurrentThread();

    ASSERT(Thread->SignalInProgress == FALSE);

    //
    // Copy the original trap frame into the saved area.
    //

    RtlCopyMemory(Thread->SavedSignalContext, TrapFrame, sizeof(TRAP_FRAME));

    //
    // If the thread was in the middle of a system call, then reset the
    // state that may not have been fully saved by the sysenter handler.
    //

    if ((Thread->Flags & THREAD_FLAG_IN_SYSTEM_CALL) != 0) {
        Thread->SavedSignalContext->Cs = USER_CS;
        Thread->SavedSignalContext->Ds = USER_DS;
        Thread->SavedSignalContext->Es = USER_DS;
        Thread->SavedSignalContext->Fs = GDT_THREAD;
        Thread->SavedSignalContext->Gs = GDT_THREAD;
        Thread->SavedSignalContext->Ss = USER_DS;
        Thread->SavedSignalContext->Ecx = 0;
        Thread->SavedSignalContext->Edx = 0;
    }

    //
    // Modify the trap frame to make the signal handler run. Shove the
    // parameters in registers to avoid having to write to user mode
    // memory.
    //

    TrapFrame->Eip = (ULONG)(Thread->OwningProcess->SignalHandlerRoutine);
    TrapFrame->Edi = SignalParameters->SignalNumber |
                     (SignalParameters->SignalCode <<
                      (sizeof(USHORT) * BITS_PER_BYTE));

    TrapFrame->Ebp = SignalParameters->ErrorNumber;

    //
    // The faulting address, sending process, and band event parameters are
    // all unioned together.
    //

    TrapFrame->Eax = (UINTN)SignalParameters->FromU.FaultingAddress;
    TrapFrame->Ebx = SignalParameters->SendingUserId;

    //
    // The value parameter and exit status are unioned together.
    //

    TrapFrame->Esi = SignalParameters->Parameter;
    Thread->SignalInProgress = TRUE;
    return;
}

VOID
PspRestorePreSignalTrapFrame (
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine restores the original user mode thread context for the thread
    before the signal was invoked.

Arguments:

    Thread - Supplies a pointer to this thread.

    TrapFrame - Supplies a pointer to the trap frame generated by this jump
        from user mode to kernel mode.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_HANDLED if a signal was not in process.

--*/

{

    ASSERT(Thread->SignalInProgress != FALSE);

    RtlCopyMemory(TrapFrame, Thread->SavedSignalContext, sizeof(TRAP_FRAME));
    return;
}

VOID
PspPrepareThreadForFirstRun (
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine performs any architecture specific initialization to prepare a
    thread for being context swapped for the first time.

Arguments:

    Thread - Supplies a pointer to the thread being prepared for its first run.

    TrapFrame - Supplies an optional pointer for the thread to restore on its
        first run.

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

        //
        // Set up the values on the user mode stack. Push the parameter and a
        // dummy return address.
        //

        UserStackPointer = Thread->UserStack + Thread->UserStackSize -
                           sizeof(PVOID);

        if (TrapFrame == NULL) {
            MmUserWrite(UserStackPointer, (UINTN)Thread->ThreadParameter);
            UserStackPointer -= 1;
            MmUserWrite(UserStackPointer, 0);
            TrapStackPointer = (UINTN)UserStackPointer;
        }

        //
        // Set the variables that will be used to set up the kernel stack.
        //

        CodeSelector = USER_CS;
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
        RtlCopyMemory(StackTrapFrame, TrapFrame, sizeof(TRAP_FRAME));

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

VOID
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

    None.

--*/

{

    PUINTN UserStackPointer;

    Thread->ThreadPointer = PsInitialThreadPointer;
    UserStackPointer = Thread->UserStack + Thread->UserStackSize -
                       sizeof(PVOID);

    MmUserWrite(UserStackPointer, (UINTN)(Thread->ThreadParameter));
    UserStackPointer -= 1;
    MmUserWrite(UserStackPointer, 0);
    RtlZeroMemory(TrapFrame, sizeof(TRAP_FRAME));
    TrapFrame->Cs = USER_CS;
    TrapFrame->Ds = USER_DS;
    TrapFrame->Es = USER_DS;
    TrapFrame->Fs = USER_DS;
    TrapFrame->Gs = GDT_THREAD;
    TrapFrame->Ss = USER_DS;
    TrapFrame->Eip = (UINTN)Thread->ThreadRoutine;
    TrapFrame->Eflags = IA32_EFLAG_ALWAYS_1 | IA32_EFLAG_IF;
    TrapFrame->Esp = (UINTN)UserStackPointer;
    TrapFrame->Ecx = (UINTN)Thread->ThreadParameter;
    if ((Thread->Flags & THREAD_FLAG_USING_FPU) != 0) {
        Thread->Flags &= ~(THREAD_FLAG_USING_FPU | THREAD_FLAG_FPU_OWNER);
        ArDisableFpu();
    }

    return;
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
    BOOL FullTrapFrame;
    USHORT PreviousInstruction;
    PVOID PreviousInstructionPointer;
    PKPROCESS Process;
    KSTATUS Status;
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
    // Be careful. A trap frame that resulted from a sysenter only contains
    // EIP, ESP, EAX, EBX, ESI, EDI, EBP, and EFLAGS. The rest is just garbage
    // from the kernel mode stack, which shouldn't be leaked to the debugger.
    // Start by copying all the common registers.
    //

    Break->InstructionPointer = TrapFrame->Eip;
    RtlZeroMemory(Break->InstructionStream, sizeof(Break->InstructionStream));
    MmCopyFromUserMode(Break->InstructionStream,
                       (PVOID)TrapFrame->Eip,
                       sizeof(Break->InstructionStream));

    Break->Registers.X86.Eflags = TrapFrame->Eflags;
    Break->Registers.X86.Eip = TrapFrame->Eip;
    Break->Registers.X86.Esp = TrapFrame->Esp;
    Break->Registers.X86.Eax = TrapFrame->Eax;
    Break->Registers.X86.Ebx = TrapFrame->Ebx;
    Break->Registers.X86.Edi = TrapFrame->Edi;
    Break->Registers.X86.Ebp = TrapFrame->Ebp;

    //
    // Now read backwards in the instruction stream to see if the last
    // instruction was a sysenter.
    //

    FullTrapFrame = TRUE;
    PreviousInstructionPointer = (PVOID)TrapFrame->Eip -
                                 X86_SYSENTER_INSTRUCTION_LENGTH;

    Status = MmCopyFromUserMode(&PreviousInstruction,
                                PreviousInstructionPointer,
                                sizeof(USHORT));

    if (!KSUCCESS(Status) ||
        (PreviousInstruction == X86_SYSENTER_INSTRUCTION)) {

        FullTrapFrame = FALSE;
    }

    if (FullTrapFrame != FALSE) {
        Break->ErrorCode = TrapFrame->ErrorCode;
        Break->Registers.X86.Ecx = TrapFrame->Ecx;
        Break->Registers.X86.Edx = TrapFrame->Edx;
        Break->Registers.X86.Esi = TrapFrame->Esi;
        Break->Registers.X86.Cs = TrapFrame->Cs;
        Break->Registers.X86.Ds = TrapFrame->Ds;
        Break->Registers.X86.Es = TrapFrame->Es;
        Break->Registers.X86.Fs = TrapFrame->Fs;
        Break->Registers.X86.Gs = TrapFrame->Gs;
        Break->Registers.X86.Ss = TrapFrame->Ss;

    } else {
        Break->ErrorCode = 0;
        Break->Registers.X86.Ecx = 0;
        Break->Registers.X86.Edx = 0;
        Break->Registers.X86.Esi = 0;
        Break->Registers.X86.Cs = USER_CS;
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

