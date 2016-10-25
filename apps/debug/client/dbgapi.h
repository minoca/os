/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgapi.h

Abstract:

    This header defines the interface to the low level debugger APIs.

Author:

    Evan Green 7-May-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/ksignals.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns non-zero if the given loaded module is applicable to the
// current event, which is true if the module's process ID is zero or equal
// to the current process.
//

#define IS_MODULE_IN_CURRENT_PROCESS(_Context, _LoadedModule)                  \
    (((_LoadedModule)->Process == 0) ||                                        \
     ((_LoadedModule)->Process ==                                              \
      (_Context)->CurrentEvent.BreakNotification.Process))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DEBUG_CONNECTION_TYPE {
    DebugConnectionInvalid,
    DebugConnectionKernel,
    DebugConnectionUser,
    DebugConnectionRemote,
} DEBUG_CONNECTION_TYPE, *PDEBUG_CONNECTION_TYPE;

typedef enum _DEBUGGER_EVENT_TYPE {
    DebuggerEventInvalid,
    DebuggerEventBreak,
    DebuggerEventShutdown,
    DebuggerEventProfiler
} DEBUGGER_EVENT_TYPE, *PDEBUGGER_EVENT_TYPE;

/*++

Structure Description:

    This structure defines an incoming debugger event.

Members:

    Type - Stores the type of debug event, which defines which of the union
        members is valid.

    BreakNotification - Stores the break notification information.

    ShutdownNotification - Stores the shutdown notification information.

    ProfilerNotification - Stores a pointer to the profiler notification
        information.

    SignalParameters - Stores the optional signal parameter information for
        debug events of type signal.

--*/

typedef struct _DEBUGGER_EVENT {
    DEBUGGER_EVENT_TYPE Type;
    union {
        BREAK_NOTIFICATION BreakNotification;
        SHUTDOWN_NOTIFICATION ShutdownNotification;
        PPROFILER_NOTIFICATION ProfilerNotification;
    };

    SIGNAL_PARAMETERS SignalParameters;
} DEBUGGER_EVENT, *PDEBUGGER_EVENT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
DbgInitialize (
    PDEBUGGER_CONTEXT Context,
    DEBUG_CONNECTION_TYPE ConnectionType
    );

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

VOID
DbgDestroy (
    PDEBUGGER_CONTEXT Context,
    DEBUG_CONNECTION_TYPE ConnectionType
    );

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

INT
DbgKdConnect (
    PDEBUGGER_CONTEXT Context,
    BOOL RequestBreak,
    PCONNECTION_RESPONSE *ConnectionDetails,
    PULONG BufferSize
    );

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

INT
DbgContinue (
    PDEBUGGER_CONTEXT Context,
    ULONG SignalToDeliver
    );

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

ULONG
DbgGetSignalToDeliver (
    PDEBUGGER_CONTEXT Context
    );

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

INT
DbgSetRegisters (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers
    );

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

INT
DbgGetSpecialRegisters (
    PDEBUGGER_CONTEXT Context,
    PSPECIAL_REGISTERS_UNION SpecialRegisters
    );

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

INT
DbgSetSpecialRegisters (
    PDEBUGGER_CONTEXT Context,
    PSET_SPECIAL_REGISTERS Command
    );

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

INT
DbgSingleStep (
    PDEBUGGER_CONTEXT Context,
    ULONG SignalToDeliver
    );

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

INT
DbgWaitForEvent (
    PDEBUGGER_CONTEXT Context
    );

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

INT
DbgRangeStep (
    PDEBUGGER_CONTEXT Context,
    PRANGE_STEP RangeStep,
    ULONG SignalToDeliver
    );

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

INT
DbgSwitchProcessors (
    PDEBUGGER_CONTEXT Context,
    ULONG ProcessorNumber
    );

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

INT
DbgGetThreadList (
    PDEBUGGER_CONTEXT Context,
    PULONG ThreadCount,
    PULONG *ThreadIds
    );

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

INT
DbgGetLoadedModuleList (
    PDEBUGGER_CONTEXT Context,
    PMODULE_LIST_HEADER *ModuleList
    );

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

VOID
DbgRequestBreakIn (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine attempts to stop the running target.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

ULONG
DbgGetHostMachineType (
    VOID
    );

/*++

Routine Description:

    This routine returns the machine type for the currently running host (this
    machine).

Arguments:

    None.

Return Value:

    Returns the machine type. See MACHINE_TYPE_* definitions.

--*/

