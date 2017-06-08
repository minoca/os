/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ptrace.c

Abstract:

    This module implements support for controlling another user mode process.

Author:

    Evan Green 27-May-2013

Environment:

    Minoca

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "dbgapi.h"
#include "dbgrprof.h"
#include "console.h"
#include "userdbg.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define a comfortable size that will fit most complete module lists.
//

#define INITIAL_MODULE_LIST_SIZE 512

//
// Define a comfortable size that will fit most complete thread lists.
//

#define INITIAL_THREAD_LIST_SIZE 256

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
// Store the ID of the currently broken in process.
//

pid_t DbgTargetProcessId;

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

    pid_t Child;
    struct sigaction OriginalAction;
    int Result;
    struct sigaction SignalAction;
    KSTATUS Status;

    //
    // Set SIGTTOU to be ignored. Both the child and the parent are going to
    // attempt to make the child's process group the foreground process group
    // of the controlling terminal. If the child gets there first, it would be
    // calling from a background process group. The parent would also fail if
    // STDIN, STDOUT, and/or STDERR are attached to the same terminal.
    //

    memset(&SignalAction, 0, sizeof(struct sigaction));
    SignalAction.sa_handler = SIG_IGN;
    Result = sigaction(SIGTTOU, &SignalAction, &OriginalAction);
    if (Result != 0) {
        return FALSE;
    }

    Child = fork();
    if (Child == -1) {
        DbgOut("Error: Failed to fork into new process. Errno: %d\n", errno);
        return FALSE;
    }

    //
    // If this is the child process, enable debugging and launch the process.
    //

    if (Child == 0) {
        Status = OsDebug(DebugCommandEnableDebugging, 0, NULL, NULL, 0, 0);
        if (!KSUCCESS(Status)) {
            DbgOut("Error: Failed to enable debugging on child process. "
                   "Status %d\n",
                   Status);

            exit(1);
        }

        //
        // Just like a child would in a shell, create a new process group and
        // make it the controlling terminal's foreground process group.
        //

        Child = getpid();
        setpgid(Child, Child);
        tcsetpgrp(STDOUT_FILENO, Child);
        tcsetpgrp(STDIN_FILENO, Child);
        tcsetpgrp(STDERR_FILENO, Child);

        //
        // Be the ball.
        //

        Result = execvp(Arguments[0], (char *const *)Arguments);
        DbgOut("Error: Failed to execute image \"%s\"\n", Arguments[0]);
        exit(Result);

    //
    // If this is the parent, make sure the child is in its own process group
    // and is the foreground process ground of the controlling terminal. Then
    // continue on.
    //

    } else {
        setpgid(Child, Child);
        tcsetpgrp(STDOUT_FILENO, Child);
        tcsetpgrp(STDIN_FILENO, Child);
        tcsetpgrp(STDERR_FILENO, Child);
        DbgOut("Created process %x.\n", Child);

        //
        // Return SIGTTOU to its original state. This does not need to happen
        // in the child as the exec call wipes out the original handlers.
        //

        sigaction(SIGTTOU, &OriginalAction, NULL);
    }

    return TRUE;
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

    KSTATUS Status;

    Status = OsDebug(DebugCommandContinue,
                     DbgTargetProcessId,
                     NULL,
                     NULL,
                     0,
                     SignalToDeliver);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to continue process %x. Status %d\n",
               DbgTargetProcessId,
               Status);

        return FALSE;
    }

    return TRUE;
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

    BREAK_NOTIFICATION Break;
    KSTATUS Status;

    //
    // Get the break information.
    //

    Status = OsDebug(DebugCommandGetBreakInformation,
                     DbgTargetProcessId,
                     NULL,
                     &Break,
                     sizeof(Break),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to get break information. Status %d\n", Status);
        return FALSE;
    }

    //
    // Set the registers and then set the break information.
    //

    RtlCopyMemory(&(Break.Registers), Registers, sizeof(REGISTERS_UNION));
    Status = OsDebug(DebugCommandSetBreakInformation,
                     DbgTargetProcessId,
                     NULL,
                     &Break,
                     sizeof(Break),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to set break information. Status %d\n", Status);
        return FALSE;
    }

    return TRUE;
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

    KSTATUS Status;

    Status = OsDebug(DebugCommandSingleStep,
                     DbgTargetProcessId,
                     NULL,
                     NULL,
                     0,
                     SignalToDeliver);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to continue process %x. Status %d\n",
               DbgTargetProcessId,
               Status);

        return FALSE;
    }

    return TRUE;
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

    pid_t Process;
    int ProcessStatus;
    KSTATUS Status;

    //
    // Block until something happens.
    //

    while (TRUE) {
        Process = waitpid(-1, &ProcessStatus, WUNTRACED | WCONTINUED);
        if (Process == -1) {
            if (errno == EINTR) {
                continue;
            }

            DbgOut("Error: Failed to wait(): %s\n", strerror(errno));
            return FALSE;
        }

        break;
    }

    //
    // Handle the process exiting.
    //

    if (WIFEXITED(ProcessStatus)) {
        Event->Type = DebuggerEventShutdown;
        Event->ShutdownNotification.ShutdownType = ShutdownTypeExit;
        Event->ShutdownNotification.Process = Process;
        Event->ShutdownNotification.ExitStatus = ProcessStatus;
        DbgTargetProcessId = -1;
        return TRUE;
    }

    DbgTargetProcessId = Process;

    //
    // Get the break information.
    //

    Status = OsDebug(DebugCommandGetBreakInformation,
                     Process,
                     NULL,
                     &(Event->BreakNotification),
                     sizeof(Event->BreakNotification),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to get break information. Status %d\n", Status);
        return FALSE;
    }

    assert(Event->BreakNotification.Process == Process);

    //
    // Get the signal information.
    //

    Status = OsDebug(DebugCommandGetSignalInformation,
                     Process,
                     NULL,
                     &(Event->SignalParameters),
                     sizeof(Event->SignalParameters),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to get signal information. Status %d\n", Status);
        return FALSE;
    }

    Event->Type = DebuggerEventBreak;
    return TRUE;
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

    PROCESS_DEBUG_BREAK_RANGE BreakRange;
    KSTATUS Status;

    RtlZeroMemory(&BreakRange, sizeof(PROCESS_DEBUG_BREAK_RANGE));
    BreakRange.BreakRangeStart = (PVOID)(UINTN)RangeStep->BreakRangeMinimum;
    BreakRange.BreakRangeEnd = (PVOID)(UINTN)RangeStep->BreakRangeMaximum;
    BreakRange.RangeHoleStart = (PVOID)(UINTN)RangeStep->RangeHoleMinimum;
    BreakRange.RangeHoleEnd = (PVOID)(UINTN)RangeStep->RangeHoleMaximum;
    Status = OsDebug(DebugCommandRangeStep,
                     DbgTargetProcessId,
                     NULL,
                     &BreakRange,
                     sizeof(PROCESS_DEBUG_BREAK_RANGE),
                     SignalToDeliver);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to range step process %x. Status %d.\n",
               DbgTargetProcessId,
               Status);

        return FALSE;
    }

    return TRUE;
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

    DEBUG_COMMAND_TYPE Command;
    KSTATUS Status;

    if (VirtualMemory == FALSE) {
        DbgOut("Error: Writing to physical memory in user mode is not "
               "allowed.\n");

        return FALSE;
    }

    Command = DebugCommandReadMemory;
    if (WriteOperation != FALSE) {
        Command = DebugCommandWriteMemory;
    }

    Status = OsDebug(Command,
                     DbgTargetProcessId,
                     (PVOID)(UINTN)Address,
                     Buffer,
                     BufferSize,
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Unable to read memory at %I64x. Status %d\n",
               Address,
               Status);

        if (BytesCompleted != NULL) {
            *BytesCompleted = 0;
        }

        return FALSE;
    }

    if (BytesCompleted != NULL) {
        *BytesCompleted = BufferSize;
    }

    return TRUE;
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

    PULONG ListBuffer;
    PULONG ListToReturn;
    ULONG Size;
    KSTATUS Status;
    PTHREAD_ID ThreadIdList;
    ULONG ThreadIndex;

    Size = INITIAL_THREAD_LIST_SIZE;
    ListBuffer = malloc(Size);
    if (ListBuffer == NULL) {
        DbgOut("Error: Failed to allocate %d bytes for thread list.\n", Size);
        return FALSE;
    }

    ListToReturn = malloc(Size);
    if (ListToReturn == NULL) {
        DbgOut("Error: Failed to allocate %d bytes for thread list.\n", Size);
        return FALSE;
    }

    while (TRUE) {
        Status = OsDebug(DebugCommandGetThreadList,
                         DbgTargetProcessId,
                         NULL,
                         ListBuffer,
                         Size,
                         0);

        //
        // On success, the buffer starts with a thread count, and then an array
        // of that may IDs. Copy the thread IDs over to the list.
        //

        if (KSUCCESS(Status)) {
            *ThreadCount = *ListBuffer;
            ThreadIdList = (PTHREAD_ID)(ListBuffer + 1);
            for (ThreadIndex = 0;
                 ThreadIndex < *ThreadCount;
                 ThreadIndex += 1) {

                ListToReturn[ThreadIndex] = (ULONG)ThreadIdList[ThreadIndex];
            }

            free(ListBuffer);
            *ThreadIds = ListToReturn;
            return TRUE;
        }

        //
        // Double the size of the buffer and try again.
        //

        if (Status == STATUS_BUFFER_TOO_SMALL) {
            free(ListBuffer);
            free(ListToReturn);
            Size *= 2;
            ListBuffer = malloc(Size);
            if (ListBuffer == NULL) {
                DbgOut("Error: Failed to allocate %d bytes for module list.\n",
                       Size);

                return FALSE;
            }

            ListToReturn = malloc(Size);
            if (ListToReturn == NULL) {
                DbgOut("Error: Failed to allocate %d bytes for module list.\n",
                       Size);

                return FALSE;
            }

            continue;
        }

        //
        // Some other error occurred.
        //

        DbgOut("Error: Unable to get module list for process %x. Status %d\n",
               DbgTargetProcessId,
               Status);

        free(ListBuffer);
        free(ListToReturn);
        return FALSE;
    }

    //
    // Execution should never get here.
    //

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

    BOOL Result;
    KSTATUS Status;
    ULONG ThreadCount;
    ULONG ThreadIndex;
    PULONG ThreadList;
    BOOL Valid;

    //
    // First ensure that the destination thread is a viable thread.
    //

    Result = DbgpUserGetThreadList(&ThreadCount, &ThreadList);
    if (Result == FALSE) {
        DbgOut("Error: Unable to get thread list for thread switch.\n");
        return FALSE;
    }

    Valid = FALSE;
    for (ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex += 1) {
        if (ThreadList[ThreadIndex] == ThreadId) {
            Valid = TRUE;
            break;
        }
    }

    if (Valid == FALSE) {
        DbgOut("Error: %x does not appear to be a valid thread.\n", ThreadId);
        return FALSE;
    }

    Status = OsDebug(DebugCommandSwitchThread,
                     DbgTargetProcessId,
                     (PVOID)(UINTN)ThreadId,
                     NULL,
                     0,
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Unable to switch to thread %x. Status %d\n",
               ThreadId,
               Status);

        return FALSE;
    }

    //
    // Get the new break information.
    //

    Status = OsDebug(DebugCommandGetBreakInformation,
                     DbgTargetProcessId,
                     NULL,
                     &(NewBreakInformation->BreakNotification),
                     sizeof(NewBreakInformation->BreakNotification),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Unable to get break information after thread switch. "
               "Status %d\n",
               Status);
    }

    return TRUE;
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

    PMODULE_LIST_HEADER List;
    ULONG Size;
    KSTATUS Status;

    Size = INITIAL_MODULE_LIST_SIZE;
    List = malloc(Size);
    if (List == NULL) {
        DbgOut("Error: Failed to allocate %d bytes for module list.\n", Size);
        return FALSE;
    }

    while (TRUE) {
        Status = OsDebug(DebugCommandGetLoadedModules,
                         DbgTargetProcessId,
                         NULL,
                         List,
                         Size,
                         0);

        if (KSUCCESS(Status)) {
            *ModuleList = List;
            return TRUE;
        }

        //
        // Double the size of the buffer and try again.
        //

        if (Status == STATUS_BUFFER_TOO_SMALL) {
            free(List);
            Size *= 2;
            List = malloc(Size);
            if (List == NULL) {
                DbgOut("Error: Failed to allocate %d bytes for module list.\n",
                       Size);

                return FALSE;
            }

            continue;
        }

        //
        // Some other error occurred.
        //

        DbgOut("Error: Unable to get module list for process %x. Status %d\n",
               DbgTargetProcessId,
               Status);

        free(List);
        return FALSE;
    }

    //
    // Execution should never get here.
    //

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

    //
    // Never deliver traps or keyboard interrupts.
    //

    if ((SignalNumber == SIGNAL_TRAP) ||
        (SignalNumber == SIGNAL_KEYBOARD_INTERRUPT)) {

        return 0;
    }

    //
    // Otherwise, deliver the signal.
    //

    return SignalNumber;
}

//
// --------------------------------------------------------- Internal Functions
//

