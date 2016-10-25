/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntusrsup.h

Abstract:

    This header contains definitions for the Win32 private interface for
    supporting user mode debugging. This go-between interface is needed because
    the debug protocol headers don't include well with Win32 headers.

Author:

    Evan Green 4-Jun-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _NT_DEBUGGER_EVENT_TYPE {
    NtDebuggerEventInvalid,
    NtDebuggerEventBreak,
    NtDebuggerEventShutdown,
} NT_DEBUGGER_EVENT_TYPE, *PNT_DEBUGGER_EVENT_TYPE;

typedef enum _NT_EXCEPTION_TYPE {
    NtExceptionInvalid,
    NtExceptionDebugBreak,
    NtExceptionSingleStep,
    NtExceptionAssertionFailure,
    NtExceptionAccessViolation,
    NtExceptionUnknown
} NT_EXCEPTION_TYPE, *PNT_EXCEPTION_TYPE;

typedef struct _NT_X86_REGISTERS {
    DWORD SegGs;
    DWORD SegFs;
    DWORD SegEs;
    DWORD SegDs;
    DWORD Edi;
    DWORD Esi;
    DWORD Ebx;
    DWORD Edx;
    DWORD Ecx;
    DWORD Eax;
    DWORD Ebp;
    DWORD Eip;
    DWORD SegCs;
    DWORD EFlags;
    DWORD Esp;
    DWORD SegSs;
} NT_X86_REGISTERS, *PNT_X86_REGISTERS;

typedef struct _NT_DEBUGGER_EVENT {
    NT_DEBUGGER_EVENT_TYPE Type;
    NT_EXCEPTION_TYPE Exception;
    ULONG ThreadNumber;
    ULONG ThreadCount;
    ULONG LoadedModuleCount;
    ULONGLONG LoadedModuleSignature;
    PVOID InstructionPointer;
    BYTE InstructionStream[16];
    NT_X86_REGISTERS Registers;
    ULONG Process;
    ULONG ExitCode;
} NT_DEBUGGER_EVENT, *PNT_DEBUGGER_EVENT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

BOOL
DbgpNtLaunchChildProcess (
    ULONG ArgumentCount,
    PSTR *Arguments
    );

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

BOOL
DbgpNtUserContinue (
    ULONG SignalToDeliver
    );

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

BOOL
DbgpNtUserSetRegisters (
    PNT_X86_REGISTERS Registers
    );

/*++

Routine Description:

    This routine sets the registers of the debugging target.

Arguments:

    Registers - Supplies a pointer to the registers to set. All register values
        will be written.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

BOOL
DbgpNtUserSingleStep (
    ULONG SignalToDeliver
    );

/*++

Routine Description:

    This routine steps the target by one instruction.

Arguments:

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

BOOL
DbgpNtUserWaitForEvent (
    PNT_DEBUGGER_EVENT Event
    );

/*++

Routine Description:

    This routine gets an event from the target, such as a break event or other
    exception.

Arguments:

    Event - Supplies a pointer where the event details will be returned.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

BOOL
DbgpNtUserReadWriteMemory (
    BOOL WriteOperation,
    BOOL VirtualMemory,
    ULONGLONG Address,
    PVOID Buffer,
    ULONG BufferSize,
    PULONG BytesCompleted
    );

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

BOOL
DbgpNtUserGetThreadList (
    PULONG ThreadCount,
    PULONG *ThreadIds
    );

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

BOOL
DbgpNtUserSwitchThread (
    ULONG ThreadId,
    PNT_DEBUGGER_EVENT NewBreakInformation
    );

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

BOOL
DbgpNtUserGetImageDetails (
    PSTR *ImageName,
    PVOID *Base,
    PVOID *LowestAddress,
    PULONGLONG Size
    );

/*++

Routine Description:

    This routine retrieves information about where the primary image of the
    process was loaded.

Arguments:

    ImageName - Supplies a pointer where a string will be returned containing
        the image name. The caller does not own this memory after it's returned,
        and should not modify or free it.

    Base - Supplies a pointer where the image base will be returned.

    LowestAddress - Supplies a pointer where the loaded lowest address of the
        image will be returned.

    Size - Supplies a pointer where the size of the image in bytes will be
        returned.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

VOID
DbgpNtUserRequestBreakIn (
    VOID
    );

/*++

Routine Description:

    This routine attempts to stop the running target.

Arguments:

    None.

Return Value:

    None.

--*/

BOOL
DbgpNtGetLoadedModuleList (
    PMODULE_LIST_HEADER *ModuleList
    );

/*++

Routine Description:

    This routine retrieves the list of loaded binaries from the kernel
    debugging target.

Arguments:

    ModuleList - Supplies a pointer where a pointer to the loaded module header
        and subsequent array of entries will be returned. It is the caller's
        responsibility to free this allocated memory when finished.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

