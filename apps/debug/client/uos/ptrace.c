/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ptrace.c

Abstract:

    This module implements support for debugging child applications using
    ptrace.

Author:

    Evan Green 20-Mar-2017

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>

#include <minoca/lib/types.h>
#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>

#include "dbgapi.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

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

    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
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

    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
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

    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
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

    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
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

    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
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

    DbgOut("ptrace debugging not yet supported.\n");
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

    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
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

    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
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

    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
}

BOOL
DbgpUserGetLoadedModuleList (
    PMODULE_LIST_HEADER *ModuleList
    )

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

{
    DbgOut("ptrace debugging not yet supported.\n");
    return FALSE;
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

    DbgOut("ptrace debugging not yet supported.\n");
    return 0;
}


//
// --------------------------------------------------------- Internal Functions
//

