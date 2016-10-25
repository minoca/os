/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntusrdbg.c

Abstract:

    This module implements the required functions to support user mode
    debugging. These functions are currently all stubs.

Author:

    Evan Green 3-Jun-2013

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "dbgrtl.h"
#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "ntusrsup.h"
#include "dbgapi.h"
#include "userdbg.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
DbgpUserConvertNtDebuggerEvent (
    PNT_DEBUGGER_EVENT NtEvent,
    PDEBUGGER_EVENT Event
    );

VOID
DbgpUserConvertFromNtRegisters (
    PNT_X86_REGISTERS NtRegisters,
    PREGISTERS_UNION Registers
    );

VOID
DbgpUserConvertToNtRegisters (
    PREGISTERS_UNION Registers,
    PNT_X86_REGISTERS NtRegisters
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOL
LaunchChildProcess (
    ULONG ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine launches a new child process to be debugged.

Arguments:

    ArgumentCount - Supplies the number of command line arguments for the
        executable.

    Arguments - Supplies the array of arguments to pass.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return DbgpNtLaunchChildProcess(ArgumentCount, Arguments);
}

BOOL
DbgpUserContinue (
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine sends the "go" command to the target, signaling to continue
    execution.

Arguments:

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    return DbgpNtUserContinue(SignalToDeliver);
}

BOOL
DbgpUserSetRegisters (
    PREGISTERS_UNION Registers
    )

/*++

Routine Description:

    This routine sets the registers of the debugging target.

Arguments:

    Registers - Supplies a pointer to the registers to set. All register values
        will be written.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    NT_X86_REGISTERS NtRegisters;

    memset(&NtRegisters, 0, sizeof(NT_X86_REGISTERS));
    DbgpUserConvertToNtRegisters(Registers, &NtRegisters);
    return DbgpNtUserSetRegisters(&NtRegisters);
}

BOOL
DbgpUserSingleStep (
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine steps the target by one instruction.

Arguments:

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    return DbgpNtUserSingleStep(SignalToDeliver);
}

BOOL
DbgpUserWaitForEvent (
    PDEBUGGER_EVENT Event
    )

/*++

Routine Description:

    This routine gets an event from the target, such as a break event or other
    exception.

Arguments:

    Event - Supplies a pointer where the event details will be returned.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    NT_DEBUGGER_EVENT NtEvent;
    BOOL Result;

    Result = DbgpNtUserWaitForEvent(&NtEvent);
    if (Result == FALSE) {
        return FALSE;
    }

    DbgpUserConvertNtDebuggerEvent(&NtEvent, Event);
    return Result;
}

BOOL
DbgpUserRangeStep (
    PRANGE_STEP RangeStep,
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine continues execution until a range of execution addresses is
    reached.

Arguments:

    RangeStep - Supplies a pointer to the range to go to.

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE on failure.

--*/

{

    return FALSE;
}

BOOL
DbgpUserReadWriteMemory (
    BOOL WriteOperation,
    BOOL VirtualMemory,
    ULONGLONG Address,
    PVOID Buffer,
    ULONG BufferSize,
    PULONG BytesCompleted
    )

/*++

Routine Description:

    This routine retrieves or writes to the target's memory.

Arguments:

    WriteOperation - Supplies a flag indicating whether this is a read
        operation (FALSE) or a write operation (TRUE).

    VirtualMemory - Supplies a flag indicating whether the memory accessed
        should be virtual or physical.

    Address - Supplies the address to read from or write to in the target's
        memory.

    Buffer - Supplies a pointer to the buffer where the memory contents will be
        returned for read operations, or supplies a pointer to the values to
        write to memory on for write operations.

    BufferSize - Supplies the size of the supplied buffer, in bytes.

    BytesCompleted - Supplies a pointer that receive the number of bytes that
        were actually read from or written to the target.

Return Value:

    Returns TRUE if the operation was successful.

    FALSE if there was an error.

--*/

{

    BOOL Result;

    Result = DbgpNtUserReadWriteMemory(WriteOperation,
                                       VirtualMemory,
                                       Address,
                                       Buffer,
                                       BufferSize,
                                       BytesCompleted);

    return Result;
}

BOOL
DbgpUserGetThreadList (
    PULONG ThreadCount,
    PULONG *ThreadIds
    )

/*++

Routine Description:

    This routine gets the list of active threads in the process (or active
    processors in the machine for kernel mode).

Arguments:

    ThreadCount - Supplies a pointer where the number of threads will be
        returned on success.

    ThreadIds - Supplies a pointer where an array of thread IDs (or processor
        numbers) will be returned on success. It is the caller's responsibility
        to free this memory.

Return Value:

    Returns TRUE if successful, FALSE on failure.

--*/

{

    return DbgpNtUserGetThreadList(ThreadCount, ThreadIds);
}

BOOL
DbgpUserSwitchThread (
    ULONG ThreadId,
    PDEBUGGER_EVENT NewBreakInformation
    )

/*++

Routine Description:

    This routine switches the debugger to another thread.

Arguments:

    ThreadId - Supplies the ID of the thread to switch to.

    NewBreakInformation - Supplies a pointer where the updated break information
        will be returned.

Return Value:

    Returns TRUE if successful, or FALSE if there was no change.

--*/

{

    NT_DEBUGGER_EVENT NtEvent;
    BOOL Result;

    Result = DbgpNtUserSwitchThread(ThreadId, &NtEvent);
    if (Result == FALSE) {
        return FALSE;
    }

    DbgpUserConvertNtDebuggerEvent(&NtEvent, NewBreakInformation);
    return Result;
}

BOOL
DbgpUserGetLoadedModuleList (
    PMODULE_LIST_HEADER *ModuleList
    )

/*++

Routine Description:

    This routine retrieves the list of loaded binaries from the kernel debugging
    target.

Arguments:

    ModuleList - Supplies a pointer where a pointer to the loaded module header
        and subsequent array of entries will be returned. It is the caller's
        responsibility to free this allocated memory when finished.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    return DbgpNtGetLoadedModuleList(ModuleList);
}

VOID
DbgpUserRequestBreakIn (
    VOID
    )

/*++

Routine Description:

    This routine attempts to stop the running target.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DbgpNtUserRequestBreakIn();
    return;
}

ULONG
DbgpUserGetSignalToDeliver (
    ULONG SignalNumber
    )

/*++

Routine Description:

    This routine returns the value for the "signal to deliver" parameters when
    letting the target continue. For user mode processes, breaks into the
    debugger occur because of signal delivery, and the debugger has the choice
    of whether or not to actually deliver a signal.

Arguments:

    SignalNumber - Supplies the last signal caught by the debugger.

Return Value:

    Returns the signal to deliver for the upcoming target continuation.

    0 if no signal should be delivered to the target.

--*/

{

    if (SignalNumber != SIGNAL_TRAP) {
        return SignalNumber;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgpUserConvertNtDebuggerEvent (
    PNT_DEBUGGER_EVENT NtEvent,
    PDEBUGGER_EVENT Event
    )

/*++

Routine Description:

    This routine converts an NT debugger event to a regular debugger event.

Arguments:

    NtEvent - Supplies a pointer to the NT event.

    Event - Supplies a pointer where the filled out event will be returned.

Return Value:

    None.

--*/

{

    ULONG InstructionStreamSize;

    memset(Event, 0, sizeof(DEBUGGER_EVENT));
    switch (NtEvent->Type) {
    case NtDebuggerEventBreak:
        Event->Type = DebuggerEventBreak;
        break;

    case NtDebuggerEventShutdown:
        Event->Type = DebuggerEventShutdown;
        Event->ShutdownNotification.ShutdownType = ShutdownTypeExit;
        Event->ShutdownNotification.UnloadAllSymbols = TRUE;
        Event->ShutdownNotification.ExitStatus = NtEvent->ExitCode;
        Event->ShutdownNotification.Process = NtEvent->Process;
        return;

    default:

        assert(FALSE);

        break;
    }

    switch (NtEvent->Exception) {
    case NtExceptionDebugBreak:
        Event->BreakNotification.Exception = ExceptionDebugBreak;
        break;

    case NtExceptionSingleStep:
        Event->BreakNotification.Exception = ExceptionSingleStep;
        break;

    case NtExceptionAssertionFailure:
        Event->BreakNotification.Exception = ExceptionAssertionFailure;
        break;

    case NtExceptionAccessViolation:
        Event->BreakNotification.Exception = ExceptionAccessViolation;
        break;

    default:

        assert(FALSE);

        break;
    }

    Event->BreakNotification.ProcessorOrThreadNumber = NtEvent->ThreadNumber;
    Event->BreakNotification.ProcessorOrThreadCount = NtEvent->ThreadCount;
    Event->BreakNotification.Process = NtEvent->Process;
    Event->BreakNotification.LoadedModuleCount = NtEvent->LoadedModuleCount;
    Event->BreakNotification.LoadedModuleSignature =
                                           NtEvent->LoadedModuleSignature;

    Event->BreakNotification.InstructionPointer =
                                            (ULONG)NtEvent->InstructionPointer;

    InstructionStreamSize = sizeof(Event->BreakNotification.InstructionStream);
    if (sizeof(NtEvent->InstructionStream) < InstructionStreamSize) {
        InstructionStreamSize = sizeof(NtEvent->InstructionStream);
    }

    memcpy(Event->BreakNotification.InstructionStream,
           NtEvent->InstructionStream,
           InstructionStreamSize);

    DbgpUserConvertFromNtRegisters(&(NtEvent->Registers),
                                   &(Event->BreakNotification.Registers));

    return;
}

VOID
DbgpUserConvertFromNtRegisters (
    PNT_X86_REGISTERS NtRegisters,
    PREGISTERS_UNION Registers
    )

/*++

Routine Description:

    This routine converts an NT register structure to the regular one.

Arguments:

    NtRegisters - Supplies a pointer to the NT registers to convert.

    Registers - Supplies a pointer where the registers will be returned.

Return Value:

    None.

--*/

{

    PX86_GENERAL_REGISTERS IaRegisters;

    IaRegisters = &(Registers->X86);
    IaRegisters->Gs = NtRegisters->SegGs;
    IaRegisters->Fs = NtRegisters->SegFs;
    IaRegisters->Es = NtRegisters->SegEs;
    IaRegisters->Ds = NtRegisters->SegDs;
    IaRegisters->Edi = NtRegisters->Edi;
    IaRegisters->Esi = NtRegisters->Esi;
    IaRegisters->Ebx = NtRegisters->Ebx;
    IaRegisters->Edx = NtRegisters->Edx;
    IaRegisters->Ecx = NtRegisters->Ecx;
    IaRegisters->Eax = NtRegisters->Eax;
    IaRegisters->Ebp = NtRegisters->Ebp;
    IaRegisters->Eip = NtRegisters->Eip;
    IaRegisters->Cs = NtRegisters->SegCs;
    IaRegisters->Eflags = NtRegisters->EFlags;
    IaRegisters->Esp = NtRegisters->Esp;
    IaRegisters->Ss = NtRegisters->SegSs;
    return;
}

VOID
DbgpUserConvertToNtRegisters (
    PREGISTERS_UNION Registers,
    PNT_X86_REGISTERS NtRegisters
    )

/*++

Routine Description:

    This routine converts registers to the NT registers format.

Arguments:

    Registers - Supplies a pointer to the registers to convert.

    NtRegisters - Supplies a pointer where the NT registers will be returned.

Return Value:

    None.

--*/

{

    PX86_GENERAL_REGISTERS IaRegisters;

    IaRegisters = &(Registers->X86);
    NtRegisters->SegGs = IaRegisters->Gs;
    NtRegisters->SegFs = IaRegisters->Fs;
    NtRegisters->SegEs = IaRegisters->Es;
    NtRegisters->SegDs = IaRegisters->Ds;
    NtRegisters->Edi = (ULONG)IaRegisters->Edi;
    NtRegisters->Esi = (ULONG)IaRegisters->Esi;
    NtRegisters->Ebx = (ULONG)IaRegisters->Ebx;
    NtRegisters->Edx = (ULONG)IaRegisters->Edx;
    NtRegisters->Ecx = (ULONG)IaRegisters->Ecx;
    NtRegisters->Eax = (ULONG)IaRegisters->Eax;
    NtRegisters->Ebp = (ULONG)IaRegisters->Ebp;
    NtRegisters->Eip = (ULONG)IaRegisters->Eip;
    NtRegisters->SegCs = IaRegisters->Cs;
    NtRegisters->EFlags = (ULONG)IaRegisters->Eflags;
    NtRegisters->Esp = (ULONG)IaRegisters->Esp;
    NtRegisters->SegSs = IaRegisters->Ss;
    return;
}

