/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgapi.c

Abstract:

    This module implements support for low level debugger core services.

Author:

    Evan Green 7-May-2013

Environment:

    Debug client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/debug/spproto.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "dbgapi.h"
#include "dbgrprof.h"
#include "console.h"
#include "userdbg.h"
#include "symbols.h"
#include "dbgrcomm.h"
#include "dbgsym.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of milliseconds to poll and see if the user has requested
// a break-in.
//

#define DEBUG_USER_POLL_MILLISECONDS 200

//
// Define the length of the standard x86 function prologue.
//

#define X86_FUNCTION_PROLOGUE_LENGTH 3

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DEBUG_COMPLETE_ACKNOWLEDGE_PACKET {
    DEBUG_PACKET_HEADER Header;
    DEBUG_PACKET_ACKNOWLEDGE Acknowledge;
} DEBUG_COMPLETE_ACKNOWLEDGE_PACKET, *_DEBUG_COMPLETE_ACKNOWLEDGE_PACKET;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
DbgpKdContinue (
    VOID
    );

BOOL
DbgpKdSetRegisters (
    PREGISTERS_UNION Registers
    );

BOOL
DbgpKdGetSpecialRegisters (
    PSPECIAL_REGISTERS_UNION SpecialRegisters
    );

BOOL
DbgpKdSetSpecialRegisters (
    PSET_SPECIAL_REGISTERS Command
    );

BOOL
DbgpKdSingleStep (
    VOID
    );

BOOL
DbgpKdWaitForEvent (
    PDEBUGGER_EVENT Event
    );

BOOL
DbgpKdRangeStep (
    PRANGE_STEP RangeStep
    );

BOOL
DbgpKdSwitchProcessors (
    ULONG ProcessorNumber
    );

BOOL
DbgpKdGetLoadedModuleList (
    PMODULE_LIST_HEADER *ModuleList
    );

BOOL
DbgpKdReadWriteMemory (
    BOOL WriteOperation,
    BOOL VirtualMemory,
    ULONGLONG Address,
    PVOID Buffer,
    ULONG BufferSize,
    PULONG BytesCompleted
    );

BOOL
DbgpKdReboot (
    DEBUG_REBOOT_TYPE RebootType
    );

BOOL
DbgpKdSendPacket (
    PDEBUG_PACKET Packet
    );

BOOL
DbgpKdReceivePacket (
    PDEBUG_PACKET Packet,
    ULONG TimeoutMilliseconds,
    PBOOL TimeoutOccurred
    );

BOOL
DbgpKdReceivePacketHeader (
    PDEBUG_PACKET_HEADER Packet,
    ULONG TimeoutMilliseconds,
    PBOOL TimeoutOccurred
    );

INT
DbgpKdSynchronize (
    VOID
    );

USHORT
DbgpKdCalculateChecksum (
    PVOID Data,
    ULONG DataLength
    );

BOOL
DbgpKdReceiveBytes (
    PVOID Buffer,
    ULONG BytesToRead
    );

BOOL
DbgpKdSendBytes (
    PVOID Buffer,
    ULONG BytesToSend
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL DbgBreakInDesired;
BOOL DbgBreakInRequestSent;

//
// Define the globals used to transmit and receive kernel debug packets.
//

DEBUG_PACKET DbgRxPacket;
DEBUG_PACKET DbgTxPacket;

//
// Define what the function prologue might look like.
//

UCHAR DbgX86FunctionPrologue[X86_FUNCTION_PROLOGUE_LENGTH] = {0x55, 0x89, 0xE5};

//
// Set this to TRUE to view the bytes going across the wire.
//

BOOL DbgKdPrintRawBytes = FALSE;
BOOL DbgKdPrintMemoryAccesses = FALSE;

//
// Set this to TRUE to enable byte escaping for transports that cannot send
// certain bytes.
//

BOOL DbgKdEncodeBytes = FALSE;

//
// This boolean gets set to TRUE when a resynchronization byte is found in the
// data stream between packets.
//

BOOL DbgKdConnectionReset;

//
// ------------------------------------------------------------------ Functions
//

INT
DbgInitialize (
    PDEBUGGER_CONTEXT Context,
    DEBUG_CONNECTION_TYPE ConnectionType
    )

/*++

Routine Description:

    This routine initializes data structures for common debugger API.

Arguments:

    Context - Supplies a pointer to the application context.

    ConnectionType - Supplies the type of debug connection to set the debugger
        up in.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    assert(ConnectionType != DebugConnectionInvalid);

    Context->ConnectionType = ConnectionType;
    if (ConnectionType == DebugConnectionUser) {
        Context->MachineType = DbgGetHostMachineType();
    }

    return 0;
}

VOID
DbgDestroy (
    PDEBUGGER_CONTEXT Context,
    DEBUG_CONNECTION_TYPE ConnectionType
    )

/*++

Routine Description:

    This routine destroys data structures for common debugger API.

Arguments:

    Context - Supplies a pointer to the application context.

    ConnectionType - Supplies the type of debug connection for which the
        debugger was initialized.

Return Value:

    None.

--*/

{

    return;
}

INT
DbgKdConnect (
    PDEBUGGER_CONTEXT Context,
    BOOL RequestBreak,
    PCONNECTION_RESPONSE *ConnectionDetails,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine establishes a link with the target. It is assumed that
    the underlying communication layer has already been established (COM ports
    have been opened and initialized, etc).

Arguments:

    Context - Supplies a pointer to the application context.

    RequestBreak - Supplies a boolean indicating whether or not the host
        should break in.

    ConnectionDetails - Supplies a pointer where a pointer to details about the
        kernel connection will be returned. The caller is responsible for
        freeing this memory when done.

    BufferSize - Supplies a pointer where the total size of the connection
        details buffer will be returned.

Return Value:

    0 on success.

    Non-zero on error.

--*/

{

    PCONNECTION_REQUEST ConnectionRequest;
    PCONNECTION_RESPONSE ConnectionResponse;
    PCONNECTION_RESPONSE NewResponse;
    INT Result;

    *BufferSize = 0;
    *ConnectionDetails = NULL;

    assert(Context->ConnectionType == DebugConnectionKernel);

    //
    // Set the connection reset flag so that receive knows to ignore incoming
    // resync bytes.
    //

    DbgKdConnectionReset = TRUE;

    //
    // Synchronize with the target to make sure it is ready and listening.
    //

    Result = DbgpKdSynchronize();
    if (Result != 0) {
        return Result;
    }

    //
    // Fill out the connection request structure, and send the initial packet.
    //

    ConnectionRequest = (PCONNECTION_REQUEST)DbgTxPacket.Payload;
    ConnectionRequest->ProtocolMajorVersion = DEBUG_PROTOCOL_MAJOR_VERSION;
    ConnectionRequest->ProtocolRevision = DEBUG_PROTOCOL_REVISION;
    ConnectionRequest->BreakRequested = RequestBreak;
    DbgTxPacket.Header.Command = DbgConnectionRequest;
    DbgTxPacket.Header.PayloadSize = sizeof(CONNECTION_REQUEST);
    Result = DbgpKdSendPacket(&DbgTxPacket);
    if (Result == FALSE) {
        DbgOut("Unable to send Connection Request packet!\n");
        return EPIPE;
    }

    //
    // Attempt to receive the connection response packet. Get through resync
    // bytes.
    //

    Result = DbgpKdReceivePacket(&DbgRxPacket, 0, NULL);
    if (Result == FALSE) {
        DbgOut("Unable to receive Connection Response packet!\n");
        return EPIPE;
    }

    //
    // The connection is now established, so future resync bytes reset it.
    //

    DbgKdConnectionReset = FALSE;
    ConnectionResponse = (PCONNECTION_RESPONSE)DbgRxPacket.Payload;
    if (DbgRxPacket.Header.Command != DbgConnectionAcknowledge) {

        //
        // Check for a debugger/target version mismatch, and bail out if one
        // occurred.
        //

        if (DbgRxPacket.Header.Command == DbgConnectionWrongVersion) {
            DbgOut("Version mismatch! Debugger version: %d.%02d, Target "
                   "version: %d.%02d.\n",
                   DEBUG_PROTOCOL_MAJOR_VERSION,
                   DEBUG_PROTOCOL_REVISION,
                   ConnectionResponse->ProtocolMajorVersion,
                   ConnectionResponse->ProtocolRevision);

        //
        // Check if the target rejected the request gracefully.
        //

        } else if (DbgRxPacket.Header.Command == DbgConnectionInvalidRequest) {
            DbgOut("Command rejected by target\n");

        //
        // The target sent something completely unexpected.
        //

        } else {
            DbgOut("Expecting DbgConnectionAcknowledge, got %d\n",
                   DbgRxPacket.Header.Command);
        }

        return EIO;
    }

    NewResponse = malloc(DbgRxPacket.Header.PayloadSize);
    if (NewResponse == NULL) {
        return ENOMEM;
    }

    //
    // A connection was successfully established. Copy the connection details
    // to the caller's structure.
    //

    RtlCopyMemory(NewResponse,
                  ConnectionResponse,
                  DbgRxPacket.Header.PayloadSize);

    *ConnectionDetails = NewResponse;
    *BufferSize = DbgRxPacket.Header.PayloadSize;
    return 0;
}

INT
DbgContinue (
    PDEBUGGER_CONTEXT Context,
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine sends the "go" command to the target, signaling to continue
    execution.

Arguments:

    Context - Supplies a pointer to the application context.

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdContinue();

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserContinue(SignalToDeliver);

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result != FALSE) {
        Context->TargetFlags |= DEBUGGER_TARGET_RUNNING;
        Result = 0;

    } else {
        Result = EINVAL;
    }

    return Result;
}

ULONG
DbgGetSignalToDeliver (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine returns the value for the "signal to deliver" parameters when
    letting the target continue. For user mode processes, breaks into the
    debugger occur because of signal delivery, and the debugger has the choice
    of whether or not to actually deliver a signal.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns the signal to deliver for the upcoming target continuation.

    0 for kernel debugging or if no signal should be delivered to the target.
    This value can just be passed through.

--*/

{

    ULONG SignalToDeliver;

    SignalToDeliver = 0;
    if (Context->ConnectionType == DebugConnectionUser) {
        SignalToDeliver = Context->CurrentEvent.SignalParameters.SignalNumber;
        SignalToDeliver = DbgpUserGetSignalToDeliver(SignalToDeliver);
    }

    return SignalToDeliver;
}

INT
DbgSetRegisters (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers
    )

/*++

Routine Description:

    This routine sets the registers of the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the registers to set. All register values
        will be written.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdSetRegisters(Registers);

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserSetRegisters(Registers);

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result != FALSE) {
        Result = 0;

    } else {
        Result = EINVAL;
    }

    return Result;
}

INT
DbgGetSpecialRegisters (
    PDEBUGGER_CONTEXT Context,
    PSPECIAL_REGISTERS_UNION SpecialRegisters
    )

/*++

Routine Description:

    This routine gets the registers of the target.

Arguments:

    Context - Supplies a pointer to the application context.

    SpecialRegisters - Supplies a pointer where the registers will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdGetSpecialRegisters(SpecialRegisters);

    } else if (Context->ConnectionType == DebugConnectionUser) {
        DbgOut("Special registers cannot be accessed in user mode.\n");
        Result = FALSE;

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result != FALSE) {
        Result = 0;

    } else {
        Result = EINVAL;
    }

    return Result;
}

INT
DbgSetSpecialRegisters (
    PDEBUGGER_CONTEXT Context,
    PSET_SPECIAL_REGISTERS Command
    )

/*++

Routine Description:

    This routine sets the special registers from the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the details of the set special registers
        command.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdSetSpecialRegisters(Command);

    } else if (Context->ConnectionType == DebugConnectionUser) {
        DbgOut("Special registers cannot be accessed in user mode.\n");
        Result = FALSE;

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result != FALSE) {
        Result = 0;

    } else {
        Result = EINVAL;
    }

    return Result;
}

INT
DbgSingleStep (
    PDEBUGGER_CONTEXT Context,
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine steps the target by one instruction.

Arguments:

    Context - Supplies a pointer to the application context.

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdSingleStep();

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserSingleStep(SignalToDeliver);

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result == FALSE) {
        return EINVAL;
    }

    Context->TargetFlags |= DEBUGGER_TARGET_RUNNING;
    Result = 0;
    return Result;
}

INT
DbgWaitForEvent (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine gets an event from the target, such as a break event or other
    exception.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    Context->CurrentEvent.Type = DebuggerEventInvalid;
    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdWaitForEvent(&(Context->CurrentEvent));

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserWaitForEvent(&(Context->CurrentEvent));

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result != FALSE) {
        Result = 0;

    } else {
        Result = EINVAL;
    }

    return Result;
}

INT
DbgRangeStep (
    PDEBUGGER_CONTEXT Context,
    PRANGE_STEP RangeStep,
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine continues execution until a range of execution addresses is
    reached.

Arguments:

    Context - Supplies the application context.

    RangeStep - Supplies a pointer to the range to go to.

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdRangeStep(RangeStep);

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserRangeStep(RangeStep, SignalToDeliver);

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result == FALSE) {
        return EINVAL;
    }

    Result = 0;
    Context->TargetFlags |= DEBUGGER_TARGET_RUNNING;
    return Result;
}

INT
DbgSwitchProcessors (
    PDEBUGGER_CONTEXT Context,
    ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine switches the debugger to another processor.

Arguments:

    Context - Supplies a pointer to the debugger context.

    ProcessorNumber - Supplies the processor number to switch to.

    DebuggeeRunning - Supplies a pointer where a boolean will be returned
        indicating whether or not the target has continued.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdSwitchProcessors(ProcessorNumber);
        if (Result == FALSE) {
            Result = EINVAL;
            goto SwitchProcessorsEnd;

        } else {
            Context->TargetFlags |= DEBUGGER_TARGET_RUNNING;
        }

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserSwitchThread(ProcessorNumber,
                                      &(Context->CurrentEvent));

        if (Result == FALSE) {
            Result = EINVAL;
            goto SwitchProcessorsEnd;
        }

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = EINVAL;
        goto SwitchProcessorsEnd;
    }

    Result = 0;

SwitchProcessorsEnd:
    return Result;
}

INT
DbgGetThreadList (
    PDEBUGGER_CONTEXT Context,
    PULONG ThreadCount,
    PULONG *ThreadIds
    )

/*++

Routine Description:

    This routine gets the list of active threads in the process (or active
    processors in the machine for kernel mode).

Arguments:

    Context - Supplies a pointer to the application context.

    ThreadCount - Supplies a pointer where the number of threads will be
        returned on success.

    ThreadIds - Supplies a pointer where an array of thread IDs (or processor
        numbers) will be returned on success. It is the caller's responsibility
        to free this memory.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG AllocationSize;
    PULONG CurrentThreadEntry;
    ULONG ProcessorCount;
    PULONG Processors;
    INT Result;
    ULONG ThreadIndex;

    *ThreadCount = 0;
    *ThreadIds = NULL;
    if (Context->ConnectionType == DebugConnectionKernel) {
        ProcessorCount =
                Context->CurrentEvent.BreakNotification.ProcessorOrThreadCount;

        assert(ProcessorCount != 0);

        *ThreadCount = ProcessorCount;
        AllocationSize = sizeof(ULONG) * ProcessorCount;
        Processors = malloc(AllocationSize);
        if (Processors == NULL) {
            DbgOut("Error: Failed to malloc %d for processor list.\n",
                   AllocationSize);

            return ENOMEM;
        }

        CurrentThreadEntry = Processors;
        for (ThreadIndex = 0; ThreadIndex < ProcessorCount; ThreadIndex += 1) {
            *CurrentThreadEntry = ThreadIndex;
        }

        *ThreadIds = Processors;
        Result = TRUE;

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserGetThreadList(ThreadCount, ThreadIds);

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result != FALSE) {
        Result = 0;

    } else {
        Result = EINVAL;
    }

    return Result;
}

INT
DbgGetLoadedModuleList (
    PDEBUGGER_CONTEXT Context,
    PMODULE_LIST_HEADER *ModuleList
    )

/*++

Routine Description:

    This routine retrieves the list of loaded binaries from the target.

Arguments:

    Context - Supplies a pointer to the application context.

    ModuleList - Supplies a pointer where a pointer to the loaded module header
        and subsequent array of entries will be returned. It is the caller's
        responsibility to free this allocated memory when finished.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdGetLoadedModuleList(ModuleList);

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserGetLoadedModuleList(ModuleList);

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result != FALSE) {
        Result = 0;

    } else {
        Result = EINVAL;
    }

    return Result;
}

VOID
DbgRequestBreakIn (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to stop the running target.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    if (Context->ConnectionType == DebugConnectionKernel) {
        DbgBreakInRequestSent = FALSE;
        DbgBreakInDesired = TRUE;

    } else if (Context->ConnectionType == DebugConnectionUser) {
        DbgpUserRequestBreakIn();

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
    }

    return;
}

INT
DbgReadMemory (
    PDEBUGGER_CONTEXT Context,
    BOOL VirtualMemory,
    ULONGLONG Address,
    ULONG BytesToRead,
    PVOID Buffer,
    PULONG BytesRead
    )

/*++

Routine Description:

    This routine retrieves the debuggee's memory.

Arguments:

    Context - Supplies a pointer to the application context.

    VirtualMemory - Supplies a flag indicating whether the read should be
        virtual or physical.

    Address - Supplies the address to read from the target's memory.

    BytesToRead - Supplies the number of bytes to be read.

    Buffer - Supplies a pointer to the buffer where the memory contents will be
        returned.

    BytesRead - Supplies a pointer that receive the number of bytes that were
        actually read from the target.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    BOOL Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdReadWriteMemory(FALSE,
                                       VirtualMemory,
                                       Address,
                                       Buffer,
                                       BytesToRead,
                                       BytesRead);

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserReadWriteMemory(FALSE,
                                         VirtualMemory,
                                         Address,
                                         Buffer,
                                         BytesToRead,
                                         BytesRead);

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result == FALSE) {
        return EINVAL;
    }

    return 0;
}

INT
DbgWriteMemory (
    PDEBUGGER_CONTEXT Context,
    BOOL VirtualMemory,
    ULONGLONG Address,
    ULONG BytesToWrite,
    PVOID Buffer,
    PULONG BytesWritten
    )

/*++

Routine Description:

    This routine writes to the debuggee's memory.

Arguments:

    Context - Supplies a pointer to the application context.

    VirtualMemory - Supplies a flag indicating whether the read should be
        virtual or physical.

    Address - Supplies the address to write to the target's memory.

    BytesToWrite - Supplies the number of bytes to be written.

    Buffer - Supplies a pointer to the buffer containing the values to write.

    BytesWritten - Supplies a pointer that receives the number of bytes that
        were actually written to the target.

Return Value:

    0 if the write was successful.

    Returns an error code on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdReadWriteMemory(TRUE,
                                       VirtualMemory,
                                       Address,
                                       Buffer,
                                       BytesToWrite,
                                       BytesWritten);

    } else if (Context->ConnectionType == DebugConnectionUser) {
        Result = DbgpUserReadWriteMemory(TRUE,
                                         VirtualMemory,
                                         Address,
                                         Buffer,
                                         BytesToWrite,
                                         BytesWritten);

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = FALSE;
    }

    if (Result == FALSE) {
        return EINVAL;
    }

    return 0;
}

INT
DbgReboot (
    PDEBUGGER_CONTEXT Context,
    ULONG RebootType
    )

/*++

Routine Description:

    This routine attempts to reboot the target machine.

Arguments:

    Context - Supplies a pointer to the application context.

    RebootType - Supplies the type of reboot to perform. See the
        DEBUG_REBOOT_TYPE enumeration.

Return Value:

    0 if the write was successful.

    Returns an error code on failure.

--*/

{

    INT Result;

    if (Context->ConnectionType == DebugConnectionKernel) {
        Result = DbgpKdReboot(RebootType);
        if (Result == FALSE) {
            Result = EINVAL;

        } else {
            Result = 0;
        }

    } else if (Context->ConnectionType == DebugConnectionUser) {
        DbgOut("Reboot is only supported on kernel debug targets.\n");
        Result = ENODEV;

    } else {
        DbgOut("Error: Unknown connection type %d.\n", Context->ConnectionType);
        Result = EINVAL;
    }

    if (Result == 0) {
        Context->TargetFlags |= DEBUGGER_TARGET_RUNNING;
    }

    return Result;
}

INT
DbgGetCallStack (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    PSTACK_FRAME Frames,
    PULONG FrameCount
    )

/*++

Routine Description:

    This routine attempts to unwind the call stack starting at the given
    machine state.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies an optional pointer to the registers on input. On
        output, these registers will be updated with the unwound value. If this
        is NULL, then the current break notification registers will be used.

    Frames - Supplies a pointer where the array of stack frames will be
        returned.

    FrameCount - Supplies the number of frames allocated in the frames
        argument, representing the maximum number of frames to get. On output,
        returns the number of valid frames in the array.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG FrameIndex;
    REGISTERS_UNION LocalRegisters;
    INT Status;
    BOOL Unwind;

    if (Registers == NULL) {

        assert(Context->CurrentEvent.Type == DebuggerEventBreak);

        RtlCopyMemory(&LocalRegisters,
                      &(Context->CurrentEvent.BreakNotification.Registers),
                      sizeof(REGISTERS_UNION));

        Registers = &LocalRegisters;
    }

    Unwind = TRUE;
    FrameIndex = 0;
    Status = 0;
    while (FrameIndex < *FrameCount) {
        Status = DbgStackUnwind(Context,
                                Registers,
                                &Unwind,
                                &(Frames[FrameIndex]));

        if (Status == EOF) {
            Status = 0;
            break;

        } else if (Status != 0) {
            break;
        }

        FrameIndex += 1;
    }

    *FrameCount = FrameIndex;
    return Status;
}

INT
DbgStackUnwind (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    PBOOL Unwind,
    PSTACK_FRAME Frame
    )

/*++

Routine Description:

    This routine attempts to unwind the stack by one frame.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the registers on input. On output, these
        registers will be updated with the unwound value.

    Unwind - Supplies a pointer that on input should initially be set to TRUE,
        indicating to use the symbol unwinder if possible. If unwinding is not
        possible, this will be set to FALSE, and should remain FALSE for the
        remainder of the stack frames unwound.

    Frame - Supplies a pointer where the basic frame information for this
        frame will be returned.

Return Value:

    0 on success.

    EOF if there are no more stack frames.

    Returns an error code on failure.

--*/

{

    ULONGLONG BasePointer;
    ULONG ByteIndex;
    ULONG BytesRead;
    ULONGLONG DebasedPc;
    PDEBUGGER_MODULE Module;
    ULONGLONG Pc;
    ULONG PointerSize;
    ULONGLONG StackPointer;
    INT Status;
    PDEBUG_SYMBOLS Symbols;
    UCHAR WorkingBuffer[24];
    UCHAR X86InstructionContents[X86_FUNCTION_PROLOGUE_LENGTH];

    //
    // First look up the symbols and see if they can unwind the stack.
    //

    Pc = DbgGetPc(Context, Registers);
    if (Pc == 0) {
        Status = EOF;
        goto StackUnwindEnd;
    }

    if (*Unwind != FALSE) {
        Module = DbgpFindModuleFromAddress(Context, Pc, &DebasedPc);
        if ((Module != NULL) && (Module->Symbols != NULL) &&
            (Module->Symbols->Interface->Unwind != NULL)) {

            Symbols = Module->Symbols;

            assert(Symbols->RegistersContext == NULL);

            Symbols->RegistersContext = Registers;
            Status = Symbols->Interface->Unwind(Symbols, DebasedPc, Frame);
            Symbols->RegistersContext = NULL;
            if (Status == 0) {

                //
                // Ignore the return address from the symbols interface, but
                // look at how they restored the PC. This is done because
                // DWARF returns the "return address" register, since that's
                // all they know. But they may have restored the PC (such as
                // from a trap frame), which is even better.
                //

                Frame->ReturnAddress = DbgGetPc(Context, Registers);
                goto StackUnwindEnd;
            }

            if (Status != ENOENT) {
                DbgOut("Failed to unwind stack at PC 0x%I64x\n", Pc);
            }
        }

        *Unwind = FALSE;
    }

    //
    // Symbols do not exist or were no help. Use traditional frame chaining to
    // unwind the stack.
    //

    PointerSize = DbgGetTargetPointerSize(Context);
    DbgGetStackRegisters(Context, Registers, &StackPointer, &BasePointer);
    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:

        //
        // If the instruction pointer was supplied, check the contents of the
        // instruction against the standard prologue. If they're equal, then
        // set up the first stack frame to more accurately represent the
        // first stack frame that hasn't quite yet been created.
        //

        if (Pc != 0) {
            Status = DbgReadMemory(Context,
                                   TRUE,
                                   Pc,
                                   X86_FUNCTION_PROLOGUE_LENGTH,
                                   X86InstructionContents,
                                   &BytesRead);

            if ((Status == 0) &&
                (BytesRead == X86_FUNCTION_PROLOGUE_LENGTH)) {

                for (ByteIndex = 0;
                     ByteIndex < X86_FUNCTION_PROLOGUE_LENGTH;
                     ByteIndex += 1) {

                    if (X86InstructionContents[ByteIndex] !=
                        DbgX86FunctionPrologue[ByteIndex]) {

                        break;
                    }
                }

                //
                // If the for loop made it all the way through without breaking,
                // a function prologue is about to execute. The base pointer is
                // in the stack pointer, and is about to be pushed, so parse the
                // first frame keeping that in mind. The return address was the
                // most recent thing pushed on the stack.
                //

                if (ByteIndex == X86_FUNCTION_PROLOGUE_LENGTH) {
                    Frame->FramePointer = StackPointer + PointerSize;
                    Frame->ReturnAddress = 0;
                    Status = DbgReadMemory(Context,
                                           TRUE,
                                           StackPointer,
                                           PointerSize,
                                           &(Frame->ReturnAddress),
                                           &BytesRead);

                    if ((Status != 0) || (BytesRead != PointerSize)) {
                        if (Status == 0) {
                            Status = EINVAL;
                        }

                        goto StackUnwindEnd;
                    }
                }
            }
        }

        //
        // Stop if the base pointer is zero.
        //

        if (BasePointer == 0) {
            Status = EOF;
            goto StackUnwindEnd;
        }

        //
        // Store the base pointer for the current frame.
        //

        Frame->FramePointer = BasePointer;

        //
        // From the base pointer, the next two pointers in memory are the
        // next base pointer and then the return address.
        //

        Status = DbgReadMemory(Context,
                               TRUE,
                               BasePointer,
                               PointerSize * 2,
                               WorkingBuffer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != (PointerSize * 2))) {
            if (Status == 0) {
                Status = EINVAL;
            }

            goto StackUnwindEnd;
        }

        BasePointer = 0;
        RtlCopyMemory(&BasePointer, WorkingBuffer, PointerSize);
        Frame->ReturnAddress = 0;
        RtlCopyMemory(&(Frame->ReturnAddress),
                      (PUCHAR)WorkingBuffer + PointerSize,
                      PointerSize);

        //
        // Update the registers.
        //

        Registers->X86.Eip = Frame->ReturnAddress;
        Registers->X86.Esp = Registers->X86.Ebp;
        Registers->X86.Ebp = BasePointer;
        break;

    case MACHINE_TYPE_ARM:

        //
        // Stop if the base pointer is zero.
        //

        if (BasePointer == 0) {
            Status = EOF;
            goto StackUnwindEnd;
        }

        //
        // The newer AAPCS calling convention sets up the frames where
        // *(fp-4) is next frame pointer, and *fp is the return address.
        // Store the base pointer for the current frame.
        //

        Frame->FramePointer = BasePointer;
        Frame->ReturnAddress = 0;

        //
        // Read in just below the base of the frame to get the return
        // address and next frame address.
        //

        Status = DbgReadMemory(Context,
                               TRUE,
                               BasePointer - PointerSize,
                               PointerSize * 2,
                               WorkingBuffer,
                               &BytesRead);

        if ((Status != 0) || (BytesRead != (PointerSize * 2))) {
            if (Status == 0) {
                Status = EINVAL;
            }

            goto StackUnwindEnd;
        }

        //
        // Store the return address for this function, then follow the base
        // pointer to get the base pointer for the calling function.
        //

        RtlCopyMemory(&(Frame->ReturnAddress),
                      (PUCHAR)WorkingBuffer + PointerSize,
                      PointerSize);

        Registers->Arm.R13Sp = BasePointer;
        BasePointer = 0;
        RtlCopyMemory(&BasePointer, WorkingBuffer, PointerSize);
        Registers->Arm.R15Pc = Frame->ReturnAddress;
        if ((Registers->Arm.R15Pc & ARM_THUMB_BIT) != 0) {
            Registers->Arm.R7 = BasePointer;
            Registers->Arm.Cpsr |= PSR_FLAG_THUMB;

        } else {
            Registers->Arm.R11Fp = BasePointer;
            Registers->Arm.Cpsr &= ~PSR_FLAG_THUMB;
        }

        break;

    case MACHINE_TYPE_X64:
        Status = EINVAL;
        goto StackUnwindEnd;

    default:

        assert(FALSE);

        Status = EINVAL;
    }

StackUnwindEnd:
    return Status;
}

INT
DbgPrintCallStack (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    BOOL PrintFrameNumbers
    )

/*++

Routine Description:

    This routine prints a call stack starting with the given registers.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies an optional pointer to the registers to use when
        unwinding.

    PrintFrameNumbers - Supplies a boolean indicating whether or not frame
        numbers should be printed to the left of every frame.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONGLONG CallSite;
    ULONG FrameCount;
    ULONG FrameIndex;
    PSTACK_FRAME Frames;
    PFUNCTION_SYMBOL Function;
    REGISTERS_UNION LocalRegisters;
    INT Result;
    PSTR SymbolName;

    Frames = NULL;
    if (Registers == NULL) {

        assert(Context->CurrentEvent.Type == DebuggerEventBreak);

        memcpy(&LocalRegisters,
               &(Context->CurrentEvent.BreakNotification.Registers),
               sizeof(REGISTERS_UNION));

        Registers = &LocalRegisters;
    }

    //
    // Initialize the call site with the current instruction pointer.
    //

    CallSite = DbgGetPc(Context, Registers);

    //
    // Allocate the call stack frames buffer.
    //

    Frames = malloc(sizeof(STACK_FRAME) * MAX_CALL_STACK);
    if (Frames == NULL) {
        DbgOut("Failed to allocate memory for call stack buffer.\n");
        Result = ENOMEM;
        goto PrintCallStackEnd;
    }

    FrameCount = MAX_CALL_STACK;
    Result = DbgGetCallStack(Context, Registers, Frames, &FrameCount);
    if (Result != 0) {
        DbgOut("Error: Failed to get call stack: %s.\n", strerror(Result));
    }

    //
    // Print the column headings.
    //

    if (PrintFrameNumbers != FALSE) {
        DbgOut("No ");
    }

    DbgOut("Frame    RetAddr  Call Site\n");
    for (FrameIndex = 0; FrameIndex < FrameCount; FrameIndex += 1) {
        SymbolName = DbgGetAddressSymbol(Context, CallSite, &Function);

        //
        // If this function is inlined, print out it and its inlined parents as
        // such.
        //

        if ((Function != NULL) && (Function->ParentFunction != NULL)) {
            if (PrintFrameNumbers != FALSE) {
                DbgOut("   ");
            }

            DbgOut("<inline>          %s\n", SymbolName);
            free(SymbolName);
            SymbolName = NULL;
            Function = Function->ParentFunction;
            while (Function->ParentFunction != NULL) {
                if (PrintFrameNumbers != FALSE) {
                    DbgOut("   ");
                }

                DbgOut("<inline>          %s\n", Function->Name);
                Function = Function->ParentFunction;
            }
        }

        //
        // Now print the real frame.
        //

        if (PrintFrameNumbers != FALSE) {
            DbgOut("%2d ", FrameIndex);
        }

        DbgOut("%08I64x %08I64x ",
               Frames[FrameIndex].FramePointer,
               Frames[FrameIndex].ReturnAddress);

        if (SymbolName != NULL) {
            DbgOut("%s\n", SymbolName);
            free(SymbolName);

        } else if (Function != NULL) {
            DbgOut("%s\n", Function->Name);

        } else {
            DbgOut("%s\n");
        }

        //
        // The next stack frame's call site is this frame's return address. Set
        // up the variables and loop again.
        //

        CallSite = Frames[FrameIndex].ReturnAddress;
    }

    Result = 0;

PrintCallStackEnd:
    if (Frames != NULL) {
        free(Frames);
    }

    return Result;
}

INT
DbgGetTargetInformation (
    PDEBUGGER_CONTEXT Context,
    PDEBUG_TARGET_INFORMATION TargetInformation,
    ULONG TargetInformationSize
    )

/*++

Routine Description:

    This routine returns information about the machine being debugged.

Arguments:

    Context - Supplies a pointer to the application context.

    TargetInformation - Supplies a pointer where the target information will
        be returned.

    TargetInformationSize - Supplies the size of the target information buffer.
        This must be the size of a debug target information structure.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    if ((TargetInformation == NULL) ||
        (TargetInformationSize != sizeof(DEBUG_TARGET_INFORMATION))) {

        return ENOSPC;
    }

    RtlZeroMemory(TargetInformation, sizeof(DEBUG_TARGET_INFORMATION));
    TargetInformation->MachineType = Context->MachineType;
    return 0;
}

ULONG
DbgGetTargetPointerSize (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine returns the size of a pointer on the target machine, in bytes.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    The size of a pointer on the target system, in bytes.

--*/

{

    ULONG PointerSize;

    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
    case MACHINE_TYPE_ARM:
        PointerSize = sizeof(ULONG);
        break;

    case MACHINE_TYPE_X64:
        PointerSize = sizeof(ULONGLONG);
        break;

    default:
        PointerSize = 0;

        assert(FALSE);

        break;
    }

    return PointerSize;
}

VOID
DbgGetStackRegisters (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    PULONGLONG StackPointer,
    PULONGLONG FramePointer
    )

/*++

Routine Description:

    This routine returns the stack and/or frame pointer registers from a
    given registers union.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the filled out registers union.

    StackPointer - Supplies an optional pointer where the stack register value
        will be returned.

    FramePointer - Supplies an optional pointer where the stack frame base
        register value will be returned.

Return Value:

    None.

--*/

{

    ULONGLONG FrameValue;
    ULONGLONG StackValue;

    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        StackValue = Registers->X86.Esp;
        FrameValue = Registers->X86.Ebp;
        break;

    case MACHINE_TYPE_ARM:
        StackValue = Registers->Arm.R13Sp;
        if ((Registers->Arm.Cpsr & PSR_FLAG_THUMB) != 0) {
            FrameValue = Registers->Arm.R7;

        } else {
            FrameValue = Registers->Arm.R11Fp;
        }

        break;

    case MACHINE_TYPE_X64:
        StackValue = Registers->X64.Rsp;
        FrameValue = Registers->X64.Rbp;
        break;

    default:

        assert(FALSE);

        StackValue = 0;
        FrameValue = 0;
        break;
    }

    if (StackPointer != NULL) {
        *StackPointer = StackValue;
    }

    if (FramePointer != NULL) {
        *FramePointer = FrameValue;
    }

    return;
}

ULONGLONG
DbgGetPc (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers
    )

/*++

Routine Description:

    This routine returns the value of the program counter (instruction pointer)
    register in the given registers union.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies an optional pointer to the filled out registers union.
        If NULL, then the registers from the current frame will be used.

Return Value:

    Returns the instruction pointer member from the given registers.

--*/

{

    ULONGLONG Value;

    if (Registers == NULL) {
        Registers = &(Context->FrameRegisters);
    }

    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        Value = Registers->X86.Eip;
        break;

    case MACHINE_TYPE_ARM:
        Value = Registers->Arm.R15Pc;
        break;

    case MACHINE_TYPE_X64:
        Value = Registers->X64.Rip;
        break;

    default:

        assert(FALSE);

        Value = 0;
        break;
    }

    return Value;
}

VOID
DbgSetPc (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine sets the value of the program counter (instruction pointer)
    register in the given registers union.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies an optional pointer to the filled out registers union.
        If NULL, then the registers from the current frame will be used.

    Value - Supplies the new value to set.

Return Value:

    None.

--*/

{

    if (Registers == NULL) {
        Registers = &(Context->FrameRegisters);
    }

    switch (Context->MachineType) {
    case MACHINE_TYPE_X86:
        Registers->X86.Eip = Value;
        break;

    case MACHINE_TYPE_ARM:
        Registers->Arm.R15Pc = Value;
        break;

    case MACHINE_TYPE_X64:
        Registers->X64.Rip = Value;
        break;

    default:

        assert(FALSE);

        break;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
DbgpKdContinue (
    VOID
    )

/*++

Routine Description:

    This routine sends the "go" command to the target, signaling to continue
    execution.

Arguments:

    None.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    BOOL Result;

    DbgTxPacket.Header.Command = DbgCommandGo;
    DbgTxPacket.Header.PayloadSize = 0;
    Result = DbgpKdSendPacket(&DbgTxPacket);
    if (Result == FALSE) {
        DbgOut("Error sending go command.\n");
    }

    return Result;
}

BOOL
DbgpKdSetRegisters (
    PREGISTERS_UNION Registers
    )

/*++

Routine Description:

    This routine sets the registers of the kernel debugging target.

Arguments:

    Registers - Supplies a pointer to the registers to set. All register values
        will be written.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    BOOL Result;

    DbgTxPacket.Header.Command = DbgCommandSetRegisters;
    DbgTxPacket.Header.PayloadSize = sizeof(REGISTERS_UNION);
    RtlCopyMemory(DbgTxPacket.Payload, Registers, sizeof(REGISTERS_UNION));
    Result = DbgpKdSendPacket(&DbgTxPacket);
    if (Result == FALSE) {
        DbgOut("Error setting registers.\n");
    }

    return Result;
}

BOOL
DbgpKdGetSpecialRegisters (
    PSPECIAL_REGISTERS_UNION SpecialRegisters
    )

/*++

Routine Description:

    This routine gets the special registers from the target.

Arguments:

    SpecialRegisters - Supplies a pointer where the registers will be returned
        on success.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    BOOL Result;

    DbgTxPacket.Header.Command = DbgCommandGetSpecialRegisters;
    DbgTxPacket.Header.PayloadSize = 0;
    Result = DbgpKdSendPacket(&DbgTxPacket);
    if (Result == FALSE) {
        DbgOut("Error setting registers.\n");
    }

    Result = DbgpKdReceivePacket(&DbgRxPacket, 0, NULL);
    if (Result == FALSE) {
        goto KdGetSpecialRegistersEnd;
    }

    if (DbgRxPacket.Header.Command != DbgCommandReturnSpecialRegisters) {
        DbgOut("Error: Got packet %d, expected special registers return.\n",
               DbgRxPacket.Header.Command);

        Result = FALSE;
        goto KdGetSpecialRegistersEnd;
    }

    if (DbgRxPacket.Header.PayloadSize != sizeof(SPECIAL_REGISTERS_UNION)) {
        DbgOut("Error: Unexpected payload size %d. Expected %d.\n",
               DbgRxPacket.Header.PayloadSize,
               sizeof(SPECIAL_REGISTERS_UNION));

        Result = FALSE;
        goto KdGetSpecialRegistersEnd;
    }

    RtlCopyMemory(SpecialRegisters,
                  DbgRxPacket.Payload,
                  sizeof(SPECIAL_REGISTERS_UNION));

    Result = TRUE;

KdGetSpecialRegistersEnd:
    return Result;
}

BOOL
DbgpKdSetSpecialRegisters (
    PSET_SPECIAL_REGISTERS Command
    )

/*++

Routine Description:

    This routine sets the special registers from the target.

Arguments:

    Command - Supplies a pointer to the details of the set special registers
        command.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    BOOL Result;

    DbgTxPacket.Header.Command = DbgCommandSetSpecialRegisters;
    DbgTxPacket.Header.PayloadSize = sizeof(SET_SPECIAL_REGISTERS);
    RtlCopyMemory(DbgTxPacket.Payload, Command, sizeof(SET_SPECIAL_REGISTERS));
    Result = DbgpKdSendPacket(&DbgTxPacket);
    if (Result == FALSE) {
        DbgOut("Error: Failed to send set special registers.\n");
    }

    return Result;
}

BOOL
DbgpKdSingleStep (
    VOID
    )

/*++

Routine Description:

    This routine steps one instruction in the kernel debugging target.

Arguments:

    None.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    BOOL Result;

    DbgTxPacket.Header.Command = DbgCommandSingleStep;
    DbgTxPacket.Header.PayloadSize = 0;
    Result = DbgpKdSendPacket(&DbgTxPacket);
    if (Result == FALSE) {
        DbgOut("Error sending single step command.\n");
    }

    return Result;
}

BOOL
DbgpKdWaitForEvent (
    PDEBUGGER_EVENT Event
    )

/*++

Routine Description:

    This routine gets an event from the target, such as a break event or other
    exception.

Arguments:

    Event - Supplies a pointer where the event details will be returned.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    BOOL Result;
    ULONG Retries;
    BOOL Return;
    BOOL TimeoutOccurred;

    Retries = 5;
    Return = FALSE;
    while (Return == FALSE) {

        //
        // Attempt to get a packet from the target.
        //

        while (TRUE) {

            //
            // If the connection was reset, attempt to reinitialize.
            //

            if (DbgKdConnectionReset != FALSE) {
                Event->Type = DebuggerEventShutdown;
                Event->ShutdownNotification.ShutdownType =
                                               ShutdownTypeSynchronizationLost;

                Result = TRUE;
                goto KdWaitForEventEnd;
            }

            Result = DbgpKdReceivePacket(&DbgRxPacket,
                                         DEBUG_USER_POLL_MILLISECONDS,
                                         &TimeoutOccurred);

            //
            // If the packet failed to be received from something other than
            // a timeout, bail out.
            //

            if ((Result == FALSE) && (TimeoutOccurred == FALSE)) {
                if (DbgKdConnectionReset != FALSE) {
                    continue;
                }

                DbgOut("Communication Error.\n");
                goto KdWaitForEventEnd;
            }

            //
            // If the packet was successfully received, break out of this loop.
            //

            if (Result != FALSE) {
                break;
            }

            //
            // A packet could not be received due to a timeout. Check to see if
            // a break-in packet should be sent, and continue.
            //

            if ((DbgBreakInDesired != FALSE) &&
                (DbgBreakInRequestSent == FALSE)) {

                DbgTxPacket.Header.Command = DbgBreakRequest;
                DbgTxPacket.Header.PayloadSize = 0;
                Result = DbgpKdSendPacket(&DbgTxPacket);
                if (Result == FALSE) {
                    if (DbgKdConnectionReset != FALSE) {
                        continue;
                    }

                    DbgOut("Error: Could not send break request.\n");
                    Retries -= 1;
                    if (Retries == 0) {
                        return FALSE;
                    }
                }

                DbgBreakInRequestSent = TRUE;
            }
        }

        Return = TRUE;
        switch (DbgRxPacket.Header.Command) {

        //
        // A breakpoint has been reached.
        //

        case DbgBreakNotification:
            Event->Type = DebuggerEventBreak;
            RtlCopyMemory(&(Event->BreakNotification),
                          DbgRxPacket.Payload,
                          sizeof(BREAK_NOTIFICATION));

            DbgBreakInDesired = FALSE;
            DbgBreakInRequestSent = FALSE;
            break;

        //
        // A shutdown occurred.
        //

        case DbgShutdownNotification:
            Event->Type = DebuggerEventShutdown;
            RtlCopyMemory(&(Event->ShutdownNotification),
                          DbgRxPacket.Payload,
                          sizeof(SHUTDOWN_NOTIFICATION));

            break;

        //
        // The profiler sent some data. Because the profiler notification has
        // a variable data array, the event only stores a pointer to a global
        // buffer for the current profiler notification.
        //

        case DbgProfilerNotification:
            Event->Type = DebuggerEventProfiler;

            assert(DbgRxPacket.Header.PayloadSize <= DEBUG_PAYLOAD_SIZE);

            Event->ProfilerNotification =
                                 (PPROFILER_NOTIFICATION)(DbgRxPacket.Payload);

            break;

        default:

            //
            // The target sent an unknown command.
            //

            DbgOut("Unknown event received: 0x%x\n",
                   DbgRxPacket.Header.Command);

            Return = FALSE;
        }

        if (Return != FALSE) {
            break;
        }
    }

    Result = TRUE;

KdWaitForEventEnd:
    return Result;
}

BOOL
DbgpKdRangeStep (
    PRANGE_STEP RangeStep
    )

/*++

Routine Description:

    This routine continues execution until a range of execution addresses is
    reached.

Arguments:

    RangeStep - Supplies a pointer to the range to go to.

Return Value:

    Returns TRUE if successful, or FALSE on failure.

--*/

{

    BOOL Result;

    DbgTxPacket.Header.Command = DbgCommandRangeStep;
    DbgTxPacket.Header.PayloadSize = sizeof(RANGE_STEP);
    RtlCopyMemory(DbgTxPacket.Payload, RangeStep, sizeof(RANGE_STEP));
    Result = DbgpKdSendPacket(&DbgTxPacket);
    if (Result == FALSE) {
        DbgOut("Error sending single step command.\n");
    }

    return Result;
}

BOOL
DbgpKdSwitchProcessors (
    ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine switches the debugger to another processor.

Arguments:

    ProcessorNumber - Supplies the processor number to switch to.

Return Value:

    Returns TRUE if successful, or FALSE if there was no change.

--*/

{

    PSWITCH_PROCESSOR_REQUEST Request;
    BOOL Result;

    //
    // Send the switch command.
    //

    DbgTxPacket.Header.Command = DbgCommandSwitchProcessor;
    Request = (PSWITCH_PROCESSOR_REQUEST)(DbgTxPacket.Payload);
    DbgTxPacket.Header.PayloadSize = sizeof(SWITCH_PROCESSOR_REQUEST);
    RtlZeroMemory(Request, sizeof(SWITCH_PROCESSOR_REQUEST));
    Request->ProcessorNumber = ProcessorNumber;
    Result = DbgpKdSendPacket(&DbgTxPacket);
    return Result;
}

BOOL
DbgpKdGetLoadedModuleList (
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

    ULONG AllocationSize;
    PMODULE_LIST_HEADER List;
    ULONG ModuleCount;
    PMODULE_LIST_HEADER ModuleListHeader;
    PMODULE_LIST_HEADER NewList;
    ULONG Offset;
    PLOADED_MODULE_ENTRY PacketModuleEntry;
    BOOL Result;

    List = NULL;

    //
    // Request the loaded modules list header to determine how many modules
    // are loaded in the target.
    //

    DbgTxPacket.Header.Command = DbgModuleListEntriesRequest;
    DbgTxPacket.Header.PayloadSize = 0;
    Result = DbgpKdSendPacket(&DbgTxPacket);
    if (Result == FALSE) {
        DbgOut("Failed to send list entries request.\n");
        goto KdGetLoadedModuleListEnd;
    }

    Result = DbgpKdReceivePacket(&DbgRxPacket, 0, NULL);
    if ((Result == FALSE) ||
        (DbgRxPacket.Header.Command != DbgModuleListHeader)) {

        DbgOut("Failed to receive list header. Got %d %d\n",
               Result,
               DbgRxPacket.Header.Command);

        Result = FALSE;
        goto KdGetLoadedModuleListEnd;
    }

    ModuleListHeader = (PMODULE_LIST_HEADER)DbgRxPacket.Payload;

    //
    // Allocate space for the header and no entries.
    //

    ModuleCount = ModuleListHeader->ModuleCount;
    AllocationSize = sizeof(MODULE_LIST_HEADER);
    List = malloc(AllocationSize);
    if (List == NULL) {
        DbgOut("Error: Failed to allocate %d bytes for module list.\n",
               AllocationSize);

        Result = FALSE;
        goto KdGetLoadedModuleListEnd;
    }

    RtlZeroMemory(List, AllocationSize);
    RtlCopyMemory(List, ModuleListHeader, sizeof(MODULE_LIST_HEADER));
    Offset = sizeof(MODULE_LIST_HEADER);
    PacketModuleEntry = (PLOADED_MODULE_ENTRY)(DbgRxPacket.Payload);

    //
    // Get all modules. For each module, attempt to match it to an existing
    // loaded module. If none is found, attempt to load the module.
    //

    while (ModuleCount != 0) {
        Result = DbgpKdReceivePacket(&DbgRxPacket, 0, NULL);
        if ((Result == FALSE) ||
            (DbgRxPacket.Header.Command != DbgModuleListEntry) ||
            (PacketModuleEntry->StructureSize < sizeof(LOADED_MODULE_ENTRY))) {

            DbgOut("Failed to get module list. Got %x %x %x\n",
                   Result,
                   DbgRxPacket.Header.Command,
                   PacketModuleEntry->StructureSize);

            Result = FALSE;
            goto KdGetLoadedModuleListEnd;
        }

        AllocationSize += PacketModuleEntry->StructureSize;
        NewList = realloc(List, AllocationSize);
        if (NewList == NULL) {
            DbgOut("Error: Failed to realloc %d bytes (for entry of %d "
                   "bytes).\n",
                   AllocationSize,
                   PacketModuleEntry->StructureSize);

            Result = FALSE;
            goto KdGetLoadedModuleListEnd;
        }

        List = NewList;
        RtlCopyMemory((PUCHAR)List + Offset,
                      DbgRxPacket.Payload,
                      PacketModuleEntry->StructureSize);

        Offset += PacketModuleEntry->StructureSize;
        ModuleCount -= 1;
    }

KdGetLoadedModuleListEnd:
    if (Result == FALSE) {
        if (List != NULL) {
            free(List);
            List = NULL;
        }
    }

    *ModuleList = List;
    return Result;
}

BOOL
DbgpKdReadWriteMemory (
    BOOL WriteOperation,
    BOOL VirtualMemory,
    ULONGLONG Address,
    PVOID Buffer,
    ULONG BufferSize,
    PULONG BytesCompleted
    )

/*++

Routine Description:

    This routine retrieves or writes to the kernel target's memory.

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

    PSTR AccessString;
    ULONG BytesThisRound;
    ULONG MaxSize;
    PBYTE PacketData;
    PMEMORY_REQUEST Request;
    PMEMORY_CONTENTS Response;
    BOOL Result;
    PWRITE_REQUEST_ACKNOWLEDGEMENT WriteAcknowledge;

    *BytesCompleted = 0;
    Result = TRUE;
    if (DbgKdPrintMemoryAccesses != FALSE) {
        AccessString = "Read";
        if (WriteOperation != FALSE) {
            AccessString = "Write";
        }

        printf("%s %d bytes at address %08llx.\n",
               AccessString,
               BufferSize,
               Address);
    }

    //
    // Currently only virtual reads are supported.
    //

    if (VirtualMemory == FALSE) {
        Result = FALSE;
        goto KdReadWriteMemoryEnd;
    }

    //
    // Calculate the maximum amount of data that can be transferred in one
    // packet. If the request is larger than that, it will need to be broken up.
    //

    if (WriteOperation != FALSE) {
        MaxSize = DEBUG_PACKET_SIZE - sizeof(DEBUG_PACKET_HEADER) -
                  sizeof(MEMORY_REQUEST);

    } else {
        MaxSize = DEBUG_PACKET_SIZE - sizeof(DEBUG_PACKET_HEADER) -
                  sizeof(MEMORY_CONTENTS);
    }

    Request = (PMEMORY_REQUEST)DbgTxPacket.Payload;
    while (*BytesCompleted < BufferSize) {

        //
        // If the request is too big for one packet, cap it.
        //

        if (BufferSize - *BytesCompleted > MaxSize) {
            BytesThisRound = MaxSize;

        } else {
            BytesThisRound = BufferSize - *BytesCompleted;
        }

        DbgTxPacket.Header.PayloadSize = sizeof(MEMORY_REQUEST);
        if (WriteOperation != FALSE) {
            DbgTxPacket.Header.Command = DbgMemoryWriteVirtual;
            DbgTxPacket.Header.PayloadSize += BytesThisRound;
            memcpy(Request + 1, Buffer + *BytesCompleted, BytesThisRound);

        } else {
            DbgTxPacket.Header.Command = DbgMemoryReadVirtual;
        }

        Request->Address = Address;
        Request->Size = BytesThisRound;

        //
        // Send the request packet and get the response.
        //

        Result = DbgpKdSendPacket(&DbgTxPacket);
        if (Result == FALSE) {
            goto KdReadWriteMemoryEnd;
        }

        Result = DbgpKdReceivePacket(&DbgRxPacket, 0, NULL);
        if (Result == FALSE) {
            goto KdReadWriteMemoryEnd;
        }

        //
        // Verify the packet that was received, and bail out if it was
        // unexpected.
        //

        if (DbgRxPacket.Header.Command == DbgInvalidCommand) {
            DbgOut("Error: Target rejected the memory request!\n");
            Result = FALSE;
            goto KdReadWriteMemoryEnd;
        }

        //
        // Handle the result from a write operation.
        //

        if (WriteOperation != FALSE) {
            if ((DbgRxPacket.Header.Command != DbgMemoryWriteAcknowledgement) ||
                (DbgRxPacket.Header.PayloadSize !=
                 sizeof(WRITE_REQUEST_ACKNOWLEDGEMENT))) {

                DbgOut("Error: Received garbage command %d from target!\n",
                       DbgRxPacket.Header.Command);

                Result = FALSE;
                goto KdReadWriteMemoryEnd;
            }

            WriteAcknowledge =
                           (PWRITE_REQUEST_ACKNOWLEDGEMENT)DbgRxPacket.Payload;

            *BytesCompleted += WriteAcknowledge->BytesWritten;
            BufferSize -= WriteAcknowledge->BytesWritten;
            Address += WriteAcknowledge->BytesWritten;

            //
            // If the target didn't write as much as requested, stop asking.
            //

            if (WriteAcknowledge->BytesWritten != BytesThisRound) {
                break;
            }

        //
        // Handle the result from a read operation.
        //

        } else {
            if ((DbgRxPacket.Header.Command != DbgMemoryContents) ||
                (DbgRxPacket.Header.PayloadSize < sizeof(MEMORY_CONTENTS))) {

                DbgOut("Error: Received garbage command %d from target!\n",
                       DbgRxPacket.Header.Command);

                Result = FALSE;
                goto KdReadWriteMemoryEnd;
            }

            //
            // Get the data from the packet and put it in the buffer.
            //

            Response = (PMEMORY_CONTENTS)DbgRxPacket.Payload;
            PacketData = (PBYTE)DbgRxPacket.Payload + sizeof(MEMORY_CONTENTS);
            RtlCopyMemory(Buffer + *BytesCompleted, PacketData, Response->Size);
            *BytesCompleted += Response->Size;
            Address += Response->Size;

            //
            // If the target didn't return as much data as requested, then don't
            // request any more.
            //

            if (Response->Size != BytesThisRound) {
                break;
            }
        }
    }

KdReadWriteMemoryEnd:
    return Result;
}

BOOL
DbgpKdReboot (
    DEBUG_REBOOT_TYPE RebootType
    )

/*++

Routine Description:

    This routine forcefully reboots the target machine.

Arguments:

    RebootType - Supplies the reboot type.

Return Value:

    Returns TRUE if successful, or FALSE if there was no change.

--*/

{

    PDEBUG_REBOOT_REQUEST Request;
    BOOL Result;

    //
    // Send the reboot command.
    //

    DbgTxPacket.Header.Command = DbgCommandReboot;
    Request = (PDEBUG_REBOOT_REQUEST)(DbgTxPacket.Payload);
    DbgTxPacket.Header.PayloadSize = sizeof(DEBUG_REBOOT_REQUEST);
    RtlZeroMemory(Request, sizeof(DEBUG_REBOOT_REQUEST));
    Request->ResetType = RebootType;
    Result = DbgpKdSendPacket(&DbgTxPacket);
    return Result;
}

BOOL
DbgpKdSendPacket (
    PDEBUG_PACKET Packet
    )

/*++

Routine Description:

    This routine sends a packet across the wire to the debugging target when
    connected to a kernel.

Arguments:

    Packet - Supplies a pointer to the debug packet. The checksum field will be
        calculated in this function.

Return Value:

    TRUE if successful, FALSE on error.

--*/

{

    DEBUG_PACKET_HEADER Acknowledge;
    USHORT Checksum;
    ULONG HeaderSize;
    UCHAR Payload;
    ULONG Retries;
    BOOL Status;
    BOOL TimeoutOccurred;

    HeaderSize = sizeof(DEBUG_PACKET_HEADER);
    Status = FALSE;
    if (Packet->Header.PayloadSize > DEBUG_PACKET_SIZE - HeaderSize) {
        DbgOut("Error: Oversized packet attempting to be sent!\n");
        return FALSE;
    }

    Packet->Header.Magic = DEBUG_PACKET_MAGIC;
    Packet->Header.PayloadSizeComplement = ~(Packet->Header.PayloadSize);
    Packet->Header.Checksum = 0;
    Checksum = DbgpKdCalculateChecksum(Packet,
                                       Packet->Header.PayloadSize + HeaderSize);

    Packet->Header.Checksum = Checksum;
    Retries = 10;
    while (Retries > 0) {

        //
        // Send the packet, then attempt to receive the response acknowledge.
        //

        Status = DbgpKdSendBytes(Packet,
                                 HeaderSize + Packet->Header.PayloadSize);

        if (Status == FALSE) {
            DbgOut("Error: Unable to send packet!\n");
            break;
        }

        Status = DbgpKdReceivePacketHeader(&Acknowledge,
                                           5000,
                                           &TimeoutOccurred);

        if (Status != FALSE) {
            if (Acknowledge.Command == DbgPacketAcknowledge) {
                Status = TRUE;
                break;

            } else {
                while (Acknowledge.PayloadSize != 0) {
                    if (DbgpKdReceiveBytes(&Payload, 1) == FALSE) {
                        break;
                    }

                    Acknowledge.PayloadSize -= 1;
                }
            }
        }

        //
        // If nonsense seems to have been received, start over.
        //

        Retries -= 1;
        Status = FALSE;
        if (DbgpKdSynchronize() != 0) {
            break;
        }
    }

    return Status;
}

BOOL
DbgpKdReceivePacket (
    PDEBUG_PACKET Packet,
    ULONG TimeoutMilliseconds,
    PBOOL TimeoutOccurred
    )

/*++

Routine Description:

    This routine receives a packet across the wire from the host when connected
    to a kernel.

Arguments:

    Packet - Supplies a pointer to the buffer that will receive the debug
        packet.

    TimeoutMilliseconds - Supplies the number of milliseconds to wait for a
        packet before giving up. Once a packet header has been received, the
        function will block until an entire packet is received.

    TimeoutOccurred - Supplies an optional pointer to a boolean indicating that
        the timeout occurred before a header could be received.

Return Value:

    TRUE if a packet was received.

    FALSE if a communication error occurred.

--*/

{

    DEBUG_COMPLETE_ACKNOWLEDGE_PACKET Acknowledge;
    USHORT CalculatedChecksum;
    USHORT Checksum;
    USHORT HeaderChecksum;
    ULONG HeaderSize;
    PSTR LastCharacter;
    ULONG Retries;
    BOOL Status;

    HeaderSize = sizeof(DEBUG_PACKET_HEADER);
    Retries = 10;
    if (TimeoutOccurred != NULL) {
        *TimeoutOccurred = FALSE;
    }

    while (TRUE) {
        Status = DbgpKdReceivePacketHeader(&(Packet->Header),
                                           TimeoutMilliseconds,
                                           TimeoutOccurred);

        if (Status == FALSE) {
            break;
        }

        //
        // If the packet has a payload, get that as well.
        //

        if (Packet->Header.PayloadSize != 0) {
            Status = DbgpKdReceiveBytes(&(Packet->Payload),
                                        Packet->Header.PayloadSize);

            if (Status == FALSE) {
                DbgOut("Error: Unable to receive packet payload.\n");
                goto KdReceivePacketHeaderRetry;
            }
        }

        //
        // Ensure that the packet came across okay. The checksum field is not
        // included in the checksum calculation, so zero it out while
        // calculating.
        //

        HeaderChecksum = Packet->Header.Checksum;
        Packet->Header.Checksum = 0;
        CalculatedChecksum = DbgpKdCalculateChecksum(
                                      Packet,
                                      HeaderSize + Packet->Header.PayloadSize);

        Packet->Header.Checksum = HeaderChecksum;
        if (HeaderChecksum != CalculatedChecksum) {
            DbgOut("Error: Checksum mismatch on received packet!\n"
                   "Calculated %x Header %x\n",
                   CalculatedChecksum,
                   HeaderChecksum);

            goto KdReceivePacketHeaderRetry;
        }

        //
        // Ignore spurious acknowledges.
        //

        if (Packet->Header.Command == DbgPacketAcknowledge) {
            DbgOut("Skipping spurious acknowledge.\n");
            continue;
        }

        //
        // The payload is okay. Send the acknowledge and break.
        //

        RtlZeroMemory(&Acknowledge, sizeof(DEBUG_COMPLETE_ACKNOWLEDGE_PACKET));
        Acknowledge.Header.Magic = DEBUG_PACKET_MAGIC;
        Acknowledge.Header.Command = DbgPacketAcknowledge;
        Acknowledge.Header.PayloadSize = sizeof(DEBUG_PACKET_ACKNOWLEDGE);
        Acknowledge.Header.PayloadSizeComplement =
                                             ~(Acknowledge.Header.PayloadSize);

        Acknowledge.Acknowledge.BreakInRequested = DbgBreakInDesired;
        Checksum = DbgpKdCalculateChecksum(
                                    &Acknowledge,
                                    sizeof(DEBUG_COMPLETE_ACKNOWLEDGE_PACKET));

        Acknowledge.Header.Checksum = Checksum;
        Status = DbgpKdSendBytes(&Acknowledge,
                                 sizeof(DEBUG_COMPLETE_ACKNOWLEDGE_PACKET));

        if (Status == FALSE) {
            goto KdReceivePacketHeaderRetry;
        }

        //
        // Handle certain events inline.
        //

        if (Packet->Header.Command == DbgPrintString) {
            LastCharacter = ((PCHAR)(Packet->Payload)) + DEBUG_PAYLOAD_SIZE - 1;

            //
            // Terminate the last character for safety, and print out the
            // string.
            //

            *LastCharacter = '\0';
            DbgOut("%s", Packet->Payload);
            continue;
        }

        Status = TRUE;
        break;

KdReceivePacketHeaderRetry:
        if (Retries == 0) {
            Status = FALSE;
            break;
        }

        //
        // Ask the host to resend and loop.
        //

        DbgOut("Asking for Resend, %d retries.\n", Retries);
        Acknowledge.Header.Command = DbgPacketResend;
        Acknowledge.Header.PayloadSize = 0;
        Acknowledge.Header.PayloadSizeComplement =
                                             ~(Acknowledge.Header.PayloadSize);

        Checksum = DbgpKdCalculateChecksum(&Acknowledge,
                                           sizeof(DEBUG_PACKET_HEADER));

        Acknowledge.Header.Checksum = Checksum;
        Status = DbgpKdSendBytes(&Acknowledge, sizeof(DEBUG_PACKET_HEADER));
        if (Status == FALSE) {
            break;
        }

        Retries -= 1;
    }

    return Status;
}

BOOL
DbgpKdReceivePacketHeader (
    PDEBUG_PACKET_HEADER Packet,
    ULONG TimeoutMilliseconds,
    PBOOL TimeoutOccurred
    )

/*++

Routine Description:

    This routine receives a packet header across the wire from the host when
    connected to a kernel.

Arguments:

    Packet - Supplies a pointer to the buffer that will receive the debug
        packet header.

    TimeoutMilliseconds - Supplies the number of milliseconds to wait for a
        packet before giving up. Once a packet header has been received, the
        function will block until an entire packet is received.

    TimeoutOccurred - Supplies an optional pointer to a boolean indicating that
        the timeout occurred before a header could be received.

Return Value:

    TRUE if a packet was received.

    FALSE if a communication error occurred.

--*/

{

    ULONG HeaderSize;
    UCHAR Magic;
    ULONG Retries;
    BOOL Status;
    ULONG TimeWaited;

    HeaderSize = sizeof(DEBUG_PACKET_HEADER);
    if (TimeoutOccurred != NULL) {
        *TimeoutOccurred = FALSE;
    }

    Retries = 10;
    while (Retries != 0) {

        //
        // If a timeout is specified, ensure that at least a header's worth of
        // data is available. If the timeout expires, return.
        //

        if (TimeoutMilliseconds != 0) {
            TimeWaited = 0;
            while (TRUE) {
                if (CommReceiveBytesReady() != FALSE) {
                    break;
                }

                CommStall(15);
                TimeWaited += 15;
                if (TimeWaited >= TimeoutMilliseconds) {
                    if (TimeoutOccurred != NULL) {
                        *TimeoutOccurred = TRUE;
                    }

                    return FALSE;
                }
            }
        }

        //
        // Attempt to synchronize on the magic field.
        //

        Magic = 0;
        Status = DbgpKdReceiveBytes(&Magic, 1);
        if (Status == FALSE) {
            Retries -= 1;
            continue;
        }

        if (Magic != DEBUG_PACKET_MAGIC_BYTE1) {

            //
            // Check for a resynchronization byte, indicating the target is
            // new or confused. If the state is already reset, then this is
            // probably during the initial connect, where extra resync bytes
            // from the target are ignored.
            //

            if ((Magic == DEBUG_SYNCHRONIZE_TARGET) &&
                (DbgKdConnectionReset == FALSE)) {

                DbgKdConnectionReset = TRUE;
                Status = FALSE;
                break;
            }

            continue;
        }

        Magic = 0;
        Status = DbgpKdReceiveBytes(&Magic, 1);
        if (Status == FALSE) {
            Retries -= 1;
            continue;
        }

        if (Magic != DEBUG_PACKET_MAGIC_BYTE2) {
            continue;
        }

        //
        // Get the packet header. Sometimes this is all that's required.
        //

        Packet->Magic = DEBUG_PACKET_MAGIC;
        Status = DbgpKdReceiveBytes((PUCHAR)Packet + DEBUG_PACKET_MAGIC_SIZE,
                                    HeaderSize - DEBUG_PACKET_MAGIC_SIZE);

        if (Status == FALSE) {
            DbgOut("Error: Unable to receive packet header!\n");
            Retries -= 1;
            continue;
        }

        if ((USHORT)~(Packet->PayloadSize) != Packet->PayloadSizeComplement) {
            DbgOut("Resynchronizing due to payload size complement "
                   "mismatch.\n");

            Retries -= 1;
            Status = FALSE;
            continue;
        }

        if (Packet->PayloadSize > DEBUG_PACKET_SIZE - HeaderSize) {
            DbgOut("Error: Oversized packet received. Command 0x%x, "
                   "PayloadSize 0x%x.\n",
                   Packet->Command,
                   Packet->PayloadSize);

            Retries -= 1;
            Status = FALSE;
            continue;
        }

        Status = TRUE;
        break;
    }

    return Status;
}

INT
DbgpKdSynchronize (
    VOID
    )

/*++

Routine Description:

    This routine synchronizes with the target machine, making sure it is ready
    to receive the connection request.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on error.

--*/

{

    BOOL Result;
    ULONG Retries;
    ULONG Status;
    UCHAR SynchronizeByte;
    ULONG TimeWaited;

    //
    // Check to see if the target has already sent a sync to the host.
    //

    while (CommReceiveBytesReady() != FALSE) {
        Result = DbgpKdReceiveBytes(&SynchronizeByte, sizeof(UCHAR));
        if (Result == FALSE) {
            Status = EPIPE;
            goto KdSynchronizeEnd;
        }

        if (SynchronizeByte == DEBUG_SYNCHRONIZE_TARGET) {
            Status = 0;
            goto KdSynchronizeEnd;
        }
    }

    Retries = 10;
    while (Retries > 0) {

        //
        // Send a little query.
        //

        SynchronizeByte = DEBUG_SYNCHRONIZE_HOST;
        Result = DbgpKdSendBytes(&SynchronizeByte, 1);
        if (Result == FALSE) {
            Status = EPIPE;
            Retries -= 1;
            continue;
        }

        //
        // Wait for a response.
        //

        TimeWaited = 0;
        while (TimeWaited < 5000) {
            if (CommReceiveBytesReady() != FALSE) {
                Result = DbgpKdReceiveBytes(&SynchronizeByte, 1);
                if (Result == FALSE) {
                    Status = EPIPE;
                    break;
                }

                if (SynchronizeByte == DEBUG_SYNCHRONIZE_TARGET) {
                    Status = 0;
                    goto KdSynchronizeEnd;
                }

            } else {
                CommStall(15);
                TimeWaited += 15;
            }
        }

        Retries -= 1;
    }

    Status = EPIPE;

KdSynchronizeEnd:
    return Status;
}

USHORT
DbgpKdCalculateChecksum (
    PVOID Data,
    ULONG DataLength
    )

/*++

Routine Description:

    This routine computes a checksum over a given length for kernel debug
    transport packets. It can handle both odd and even length data.

Arguments:

    Data - Supplies a pointer to the data that is to be checksummed.

    DataLength - Supplies the length of the data buffer, in bytes.

Return Value:

    Returns the checksum.

--*/

{

    USHORT Checksum;
    PUSHORT CurrentData;
    ULONG Index;
    ULONG ShortLength;

    Checksum = 0;
    Index = 0;

    //
    // Checksums are calculated by adding up a series of two-byte values.
    // Convert the pointer to a short pointer and divide bytes by 2 to get size
    // in shorts.
    //

    ShortLength = DataLength / 2;
    CurrentData = (PUSHORT)Data;
    while (Index < ShortLength) {
        Checksum += *CurrentData;
        CurrentData += 1;
        Index += 1;
    }

    //
    // If the data was an odd length, then there's one byte left to be added.
    // Add only that byte.
    //

    if ((ShortLength * 2) != DataLength) {
        Checksum += *((PUCHAR)CurrentData);
    }

    return Checksum;
}

BOOL
DbgpKdReceiveBytes (
    PVOID Buffer,
    ULONG BytesToRead
    )

/*++

Routine Description:

    This routine receives a number of bytes from the debugger connection.

Arguments:

    Buffer - Supplies a pointer to the buffer where the data should be returned.

    BytesToRead - Supplies the number of bytes that should be received into the
        buffer.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ULONG ByteIndex;
    PUCHAR Bytes;
    CHAR Character;
    ULONG Count;
    ULONG Index;
    BOOL NextEscaped;
    BOOL Status;

    NextEscaped = FALSE;
    Status = TRUE;
    Bytes = Buffer;
    while (BytesToRead != 0) {
        Status = CommReceive(Bytes, BytesToRead);
        if (Status == FALSE) {
            DbgOut("Failed to receive %d bytes.\n", BytesToRead);
            return Status;
        }

        if (DbgKdPrintRawBytes != FALSE) {
            DbgOut("RX: ");
            for (ByteIndex = 0; ByteIndex < BytesToRead; ByteIndex += 1) {
                DbgOut("%02X ", Bytes[ByteIndex]);
            }

            DbgOut("\nRX: ");
            for (ByteIndex = 0; ByteIndex <  BytesToRead; ByteIndex += 1) {
                Character = Bytes[ByteIndex];
                if (!isprint(Character)) {
                    Character = '.';
                }

                DbgOut("%02c ", Character);
            }

            DbgOut("\n");
        }

        //
        // If escaping is on, then remove any escape bytes found, and fix up
        // the escaped byte.
        //

        Count = 0;
        if (DbgKdEncodeBytes != FALSE) {

            //
            // If the last byte received was an escape, then unescape this
            // first byte.
            //

            if (NextEscaped != FALSE) {
                NextEscaped = FALSE;
                Bytes[0] -= DEBUG_ESCAPE;
                Bytes += 1;
                BytesToRead -= 1;
                if (BytesToRead == 0) {
                    break;
                }
            }

            for (Index = 0; Index < BytesToRead - 1; Index += 1) {
                if (Bytes[Index] == DEBUG_ESCAPE) {
                    memmove(&(Bytes[Index]),
                            &(Bytes[Index + 1]),
                            BytesToRead - Index - 1);

                    Count += 1;
                    Bytes[Index] -= DEBUG_ESCAPE;
                }
            }

            if (Bytes[Index] == DEBUG_ESCAPE) {
                Count += 1;
                NextEscaped = TRUE;
            }
        }

        Bytes += BytesToRead - Count;
        BytesToRead = Count;
    }

    return Status;
}

BOOL
DbgpKdSendBytes (
    PVOID Buffer,
    ULONG BytesToSend
    )

/*++

Routine Description:

    This routine sends a number of bytes through the debugger connection.

Arguments:

    Buffer - Supplies a pointer to the buffer where the data to be sent resides.

    BytesToSend - Supplies the number of bytes that should be sent.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ULONG ByteIndex;
    PUCHAR Bytes;
    UCHAR EncodedByte[2];
    ULONG SendSize;
    BOOL Status;

    Bytes = Buffer;
    if (DbgKdPrintRawBytes != FALSE) {
        DbgOut("TX: ");
        for (ByteIndex = 0; ByteIndex < BytesToSend; ByteIndex += 1) {
            DbgOut("%02X ", Bytes[ByteIndex]);
        }

        DbgOut("\n");
    }

    Status = TRUE;
    while (BytesToSend != 0) {
        SendSize = 0;
        if (DbgKdEncodeBytes != FALSE) {

            //
            // Gather bytes until one is found that needs escaping.
            //

            while ((SendSize < BytesToSend) &&
                   (Bytes[SendSize] != DEBUG_XON) &&
                   (Bytes[SendSize] != DEBUG_XOFF) &&
                   (Bytes[SendSize] != DEBUG_ESCAPE)) {

                SendSize += 1;
            }

        //
        // If no escaping is needed, just send everything.
        //

        } else {
            SendSize = BytesToSend;
        }

        if (SendSize != 0) {
            Status = CommSend(Bytes, SendSize);
            if (Status == FALSE) {
                DbgOut("Failed to send %d bytes.\n", BytesToSend);
                break;
            }
        }

        Bytes += SendSize;
        BytesToSend -= SendSize;
        if (BytesToSend != 0) {
            EncodedByte[0] = DEBUG_ESCAPE;
            EncodedByte[1] = *Bytes + DEBUG_ESCAPE;
            Status = CommSend(EncodedByte, 2);
            if (Status == FALSE) {
                DbgOut("Failed to send %d bytes.\n", BytesToSend);
                break;
            }

            Bytes += 1;
            BytesToSend -= 1;
        }
    }

    return Status;
}

