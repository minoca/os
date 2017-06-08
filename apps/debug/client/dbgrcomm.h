/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgrcomm.h

Abstract:

    This header contains definitions for the debugger protocol communication
    helper routines.

Author:

    Evan Green 3-Jul-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define ARM_INSTRUCTION_LENGTH 4
#define ARM_FUNCTION_PROLOGUE 0xE1A0C00D

#define DEBUGGER_MAX_COMMAND_ARGUMENTS 32

//
// Define the maximum number of call stack elements that can be printed.
//

#define MAX_CALL_STACK 4096

//
// Define debugger flags.
//

//
// This flag is set if the debugger is in the process of exiting the
// application.
//

#define DEBUGGER_FLAG_EXITING 0x00000001

//
// This flag is set if the debugger should echo commands sent to standard in.
//

#define DEBUGGER_FLAG_ECHO_COMMANDS 0x00000002

//
// This flag is set to enable stepping by source line numbers, or cleared to
// step by instruction.
//

#define DEBUGGER_FLAG_SOURCE_LINE_STEPPING 0x00000004

//
// This flag is set if the debugger should print source file and line numbers
// along with each address symbol.
//

#define DEBUGGER_FLAG_PRINT_LINE_NUMBERS 0x00000008

//
// This flag is set if the user would like an initial break-in upon
// connection.
//

#define DEBUGGER_FLAG_INITIAL_BREAK 0x00000010

//
// This flag is set if the user wants to print all attempted source file loads
// (useful when trying to figure out why source display isn't working).
//

#define DEBUGGER_FLAG_PRINT_SOURCE_LOADS 0x00000020

//
// Define debugger target flags.
//

//
// This flag is set if the target is running.
//

#define DEBUGGER_TARGET_RUNNING 0x00000001

//
// x86 Flags.
//

#define IA32_EFLAG_CF 0x00000001
#define IA32_EFLAG_PF 0x00000004
#define IA32_EFLAG_AF 0x00000010
#define IA32_EFLAG_ZF 0x00000040
#define IA32_EFLAG_SF 0x00000080
#define IA32_EFLAG_TF 0x00000100
#define IA32_EFLAG_IF 0x00000200
#define IA32_EFLAG_DF 0x00000400
#define IA32_EFLAG_OF 0x00000800
#define IA32_EFLAG_IOPL_MASK 0x00003000
#define IA32_EFLAG_IOPL_SHIFT 12
#define IA32_EFLAG_NT 0x00004000
#define IA32_EFLAG_RF 0x00010000
#define IA32_EFLAG_VM 0x00020000
#define IA32_EFLAG_AC 0x00040000
#define IA32_EFLAG_VIF 0x00080000
#define IA32_EFLAG_VIP 0x00100000
#define IA32_EFLAG_ID 0x00200000
#define IA32_EFLAG_ALWAYS_0 0xFFC08028
#define IA32_EFLAG_ALWAYS_1 0x00000002

//
// ARM Processor modes.
//

#define ARM_MODE_USER   0x00000010
#define ARM_MODE_FIQ    0x00000011
#define ARM_MODE_IRQ    0x00000012
#define ARM_MODE_SVC    0x00000013
#define ARM_MODE_ABORT  0x00000017
#define ARM_MODE_UNDEF  0x0000001B
#define ARM_MODE_SYSTEM 0x0000001F
#define ARM_MODE_MASK   0x0000001F

//
// ARM Program Status Register flags.
//

#define PSR_FLAG_NEGATIVE   0x80000000
#define PSR_FLAG_ZERO       0x40000000
#define PSR_FLAG_CARRY      0x20000000
#define PSR_FLAG_OVERFLOW   0x10000000
#define PSR_FLAG_SATURATION 0x08000000
#define PSR_FLAG_JAZELLE    0x01000000
#define PSR_FLAG_THUMB      0x00000020
#define PSR_FLAG_IRQ        0x00000080
#define PSR_FLAG_FIQ        0x00000040

//
// ARM Instruction information.
//

#define ARM_BREAK_INSTRUCTION 0xE7F000F3
#define ARM_BREAK_INSTRUCTION_LENGTH ARM_INSTRUCTION_LENGTH
#define THUMB_BREAK_INSTRUCTION 0xDE20
#define THUMB_BREAK_INSTRUCTION_LENGTH 2
#define ARM_THUMB_BIT 0x00000001

#define X86_BREAK_INSTRUCTION 0xCC
#define X86_BREAK_INSTRUCTION_LENGTH 1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DEBUGGER_BREAK_POINT_TYPE {
    BreakpointTypeInvalid,
    BreakpointTypeExecution,
    BreakpointTypeRead,
    BreakpointTypeWrite,
    BreakpointTypeReadWrite
} DEBUGGER_BREAK_POINT_TYPE, *PDEBUGGER_BREAK_POINT_TYPE;

/*++

Structure Description:

    This structure defines a breakpoint.

Members:

    ListEntry - Stores pointers to the next and previous breakpoints in the
        global list of breakpoints.

    Index - Stores the index of the breakpoint.

    Type - Stores the type of breakpoint, whether it be software or some sort of
        hardware breakpoint.

    Address - Stores the virtual address of the breakpoint.

    AccessSize - Stores the access size of the breakpoint if it is a break on
        access type of breakpoint.

    OriginalValue - Stores the original contents of the instruction stream
        before the breakpoint was inserted. This only applies to software
        execution breakpoints.

    Enabled - Stores a flag indicating whether or not this breakpoint is
        currently active in the system.

--*/

typedef struct _DEBUGGER_BREAK_POINT {
    LIST_ENTRY ListEntry;
    LONG Index;
    DEBUGGER_BREAK_POINT_TYPE Type;
    ULONGLONG Address;
    UCHAR AccessSize;
    ULONG OriginalValue;
    BOOL Enabled;
} DEBUGGER_BREAK_POINT, *PDEBUGGER_BREAK_POINT;

/*++

Structure Description:

    This structure defines parameters for dumping memory.

Members:

    Virtual - Stores a boolean indicating if the address is virtual (TRUE) or
        physical (FALSE).

    PrintCharacters - Stores a boolean indicating if characters should be
        printed as well.

    NextAddress - Stores the next address to dump.

    Columns - Stores the desired number of columns.

    TotalValues - Stores the desired total number of values to dump.

--*/

typedef struct _DEBUGGER_DUMP_MEMORY_PARAMETERS {
    BOOL Virtual;
    BOOL PrintCharacters;
    ULONGLONG NextAddress;
    ULONG Columns;
    ULONG TotalValues;
} DEBUGGER_DUMP_MEMORY_PARAMETERS, *PDEBUGGER_DUMP_MEMORY_PARAMETERS;

/*++

Structure Description:

    This structure defines a mutable array of pointer sized elements.

Members:

    Elements - Stores the array of pointers.

    Size - Stores the number of valid elements in the array.

    Capacity - Storse the maximum number of elements that can be put in the
        array before the array itself needs to be reallocated.

--*/

typedef struct _POINTER_ARRAY {
    PVOID *Elements;
    ULONGLONG Size;
    ULONGLONG Capacity;
} POINTER_ARRAY, *PPOINTER_ARRAY;

/*++

Structure Description:

    This structure defines thread profiling parameters.

Members:

    StatisticsListHead - Stores the head of the list of thread statistics data.

    StatisticsListLock - Stores a handle to the lock serializing access to the
        thread statistics list.

    StatisticsLock - Stores a handle to the lock serializing access to the
        pointer arrays and other thread profiling data.

    ContextSwaps - Stores the array of context swap data.

    Processes - Stores the array of process data.

    Threads - Stores the array of thread data.

    ProcessorCount - Stores the number of processors in the system.

    ReferenceTime - Stores the reference time for thread profiling data.

    ProcessNameWidth - Stores the width in characters of the process name field.

    ThreadNameWidth - Stores the width in characters of the thread name field.

--*/

typedef struct _DEBUGGER_THREAD_PROFILING_DATA {
    LIST_ENTRY StatisticsListHead;
    HANDLE StatisticsListLock;
    HANDLE StatisticsLock;
    PPOINTER_ARRAY ContextSwaps;
    PPOINTER_ARRAY Processes;
    PPOINTER_ARRAY Threads;
    ULONG ProcessorCount;
    PROFILER_THREAD_TIME_COUNTER ReferenceTime;
    ULONG ProcessNameWidth;
    ULONG ThreadNameWidth;
} DEBUGGER_THREAD_PROFILING_DATA, *PDEBUGGER_THREAD_PROFILING_DATA;

/*++

Structure Description:

    This structure stores profiling information.

Members:

    StackListHead - Stores the head of the list of stack sampling data.

    StackListLock - Stores a handle to the lock serializing access to the stack
        list.

    MemoryListHead - Stores the head of the list of memory profiling data.

    MemoryListLock - Stores a handle to the lock serializing access to the
        memory list.

    MemoryCollectionActive - Stores a boolean indicating if memory data is
        being collected.

    CommandLineStackRoot - Stores the stack profiling root node when running
        from the command line.

    CommandLinePoolListHead - Stores a pointer to the most recent pool when
        running command line commands.

    CommandLineBaseListHead - Stores a pointer to the base line memory list
        list when running command line commands.

--*/

typedef struct _DEBUGGER_PROFILING_DATA {
    LIST_ENTRY StackListHead;
    HANDLE StackListLock;
    LIST_ENTRY MemoryListHead;
    HANDLE MemoryListLock;
    BOOL MemoryCollectionActive;
    PSTACK_DATA_ENTRY CommandLineStackRoot;
    PLIST_ENTRY CommandLinePoolListHead;
    PLIST_ENTRY CommandLineBaseListHead;
} DEBUGGER_PROFILING_DATA, *PDEBUGGER_PROFILING_DATA;

/*++

Structure Description:

    This structure stores information pertaining to the maintenance of standard
    output.

Members:

    Lock - Stores the handle to the lock synchronizing access to the console
        buffer.

    ConsoleBuffer - Stores a pointer to the console buffer.

    ConsoleBufferSize - Stores the number of valid bytes in the console
        buffer, including the null terminator if there is one.

    ConsoleBufferCapacity - Stores the allocation size of the console buffer.

    Prompt - Stores the current prompt.

--*/

typedef struct _DEBUGGER_STANDARD_OUT {
    HANDLE Lock;
    PCHAR ConsoleBuffer;
    ULONGLONG ConsoleBufferSize;
    ULONGLONG ConsoleBufferCapacity;
    PSTR Prompt;
} DEBUGGER_STANDARD_OUT, *PDEBUGGER_STANDARD_OUT;

/*++

Structure Description:

    This structure stores information pertaining to the maintenance of standard
    input.

Members:

    Lock - Stores a lock serializing access to the remote command list.

    RemoteCommandList - Stores the head of the list of remote commands waiting
        to be executed.

--*/

typedef struct _DEBUGGER_STANDARD_IN {
    HANDLE Lock;
    LIST_ENTRY RemoteCommandList;
} DEBUGGER_STANDARD_IN, *PDEBUGGER_STANDARD_IN;

/*++

Structure Description:

    This structure stores information pertaining to managing a remote debug
    server.

Members:

    Socket - Stores the socket listening to incoming connections.

    Host - Stores the host name or IP the server is listening on.

    Port - Stores the port the server is listening on.

    ShutDown - Stores a value indicating that the server should be shut down.

    ClientList - Stores the list of connected clients.

--*/

typedef struct _DEBUGGER_SERVER_CONTEXT {
    int Socket;
    char *Host;
    int Port;
    volatile int ShutDown;
    LIST_ENTRY ClientList;
} DEBUGGER_SERVER_CONTEXT, *PDEBUGGER_SERVER_CONTEXT;

/*++

Structure Description:

    This structure stores information about the remote connection of the client
    (this application) to a remote server.

Members:

    Socket - Stores the socket connected to the server.

    ShutDown - Stores a value indicating that the client network thread is
        finished.

--*/

typedef struct _DEBUGGER_CLIENT_CONTEXT {
    volatile int ShutDown;
    int Socket;
} DEBUGGER_CLIENT_CONTEXT, *PDEBUGGER_CLIENT_CONTEXT;

/*++

Structure Description:

    This structure stores information about the current source line.

Members:

    Path - Stores the full path to the source file, as represented in the
        debug symbols. This needs to be freed.

    ActualPath - Stores the full path to the source file where it was actually
        found. This needs to be freed.

    Contents - Stores the contents of the source file. This needs to be freed.

    Size - Stores the size of the file in bytes.

    LineNumber - Stores the currently highlighted line number, or 0 if no line
        is highlighted.

--*/

typedef struct _DEBUGGER_SOURCE_FILE {
    PSTR Path;
    PSTR ActualPath;
    PVOID Contents;
    ULONGLONG Size;
    ULONGLONG LineNumber;
} DEBUGGER_SOURCE_FILE, *PDEBUGGER_SOURCE_FILE;

/*++

Structure Description:

    This structure stores information about the current source line.

Members:

    ListEntry - Stores pointers to the next and previous source paths.

    Prefix - Stores a pointer to the prefix to pull off of the potential
        source path. If the source in question does not match, it will be
        skipped. An empty prefix matches everything.

    PrefixLength - Stores the length of the prefix in characters, not including
        the null terminator.

    Path - Stores a pointer to the path to search for sources in.

    PathLength - Stores the length of the path string in characters, not
        including the null terminator.

--*/

typedef struct _DEBUGGER_SOURCE_PATH {
    LIST_ENTRY ListEntry;
    PSTR Prefix;
    UINTN PrefixLength;
    PSTR Path;
    UINTN PathLength;
} DEBUGGER_SOURCE_PATH, *PDEBUGGER_SOURCE_PATH;

/*++

Structure Description:

    This structure defines an instance of the debugger application.

Members:

    Flags - Stores a bitfield of global flags, see DEBUGGER_FLAG_* for
        definitions.

    TargetFlags - Stores a bitfield of flags regarding the target connection.
        See DEBUGGER_TARGET_* definitions.

    SymbolPath - Stores a pointer to an array of strings containing the symbol
        path to search when trying to load symbols.

    SymbolPathCount - Stores the number of elements in the symbol path array.

    RangeStepValid - Stores a boolean indicating whether the range step
        parameters are active.

    RangeStepParameters - Stores the parameters for the current range step.

    OneTimeBreakValid - Stores a boolean indicating if the one-time breakpoint
        (used for "g <addr>") is valid.

    OneTimeBreakAddress - Stores the target memory address of the one-time
        breakpoint.

    OneTimeBreakOriginalValue - Stores the original value at the one-time
        break address.

    LastMemoryDump - Stores the parameters used in the most recent memory
        dump.

    BreakpointList - Stores the head of the list of breakpoints.

    BreakpointToRestore - Stores a pointer to the breakpoint to reset after
        stepping off of it.

    PreviousProcess - Stores the process ID of the last debugger event, used to
        determine if the symbol list should be reloaded.

    BreakInstructionLength - Stores the length of the instruction of the most
        recent break.

    DisassemblyAddress - Stores the address of the next disassembly command if
        no address is provided.

    LoadedExtensions - Stores the list head of loaded debugger extensions.

    SourceFile - Stores information about the currently displayed source file.

    SourcePathList - Stores the head of the list of DEBUGGER_SOURCE_PATHs.

    ModuleList - Stores the loaded module list.

    RemoteModuleListSignature - Stores the most recent loaded module list
        signature.

    MachineType - Stores the target machine type.

    ThreadProfiling - Stores the thread profiling data.

    ProfilingData - Stores generic profiling data.

    StandardOut - Stores the standard out information.

    StandardIn - Stores the standard in information.

    CommandBuffer - Stores the buffer that holds the currently running debugger
        command.

    CommandBufferSize - Stores the total size of the command buffer in bytes.

    CommandHistory - Stores the debugger command history array.

    CommandHistorySize - Stores the size of the command history array. Not all
        of these elements are necessarily valid.

    CommandHistoryNextIndex - Stores the index where the next history entry
        will be deposited.

    Server - Stores context when this debugger is acting as a remote server.

    Client - Stores context when this debugger is acting as a remote client.

    CurrentEvent - Stores the current debug event.

    ConnectionType - Stores the current debug connection type.

    FrameRegisters - Stores the registers as unwound to the currently displayed
        stack frame.

    CurrentFrame - Stores the current stack frame index.

--*/

struct _DEBUGGER_CONTEXT {
    ULONG Flags;
    ULONG TargetFlags;
    PSTR *SymbolPath;
    ULONG SymbolPathCount;
    BOOL RangeStepValid;
    RANGE_STEP RangeStepParameters;
    BOOL OneTimeBreakValid;
    ULONGLONG OneTimeBreakAddress;
    ULONG OneTimeBreakOriginalValue;
    DEBUGGER_DUMP_MEMORY_PARAMETERS LastMemoryDump;
    LIST_ENTRY BreakpointList;
    PDEBUGGER_BREAK_POINT BreakpointToRestore;
    ULONG PreviousProcess;
    ULONG BreakInstructionLength;
    ULONGLONG DisassemblyAddress;
    LIST_ENTRY LoadedExtensions;
    DEBUGGER_SOURCE_FILE SourceFile;
    LIST_ENTRY SourcePathList;
    ULONG HighlightedLineNumber;
    DEBUGGER_MODULE_LIST ModuleList;
    ULONGLONG RemoteModuleListSignature;
    ULONG MachineType;
    DEBUGGER_THREAD_PROFILING_DATA ThreadProfiling;
    DEBUGGER_PROFILING_DATA ProfilingData;
    DEBUGGER_STANDARD_OUT StandardOut;
    DEBUGGER_STANDARD_IN StandardIn;
    PSTR CommandBuffer;
    ULONG CommandBufferSize;
    PSTR *CommandHistory;
    ULONG CommandHistorySize;
    ULONG CommandHistoryNextIndex;
    DEBUGGER_SERVER_CONTEXT Server;
    DEBUGGER_CLIENT_CONTEXT Client;
    DEBUGGER_EVENT CurrentEvent;
    DEBUG_CONNECTION_TYPE ConnectionType;
    REGISTERS_UNION FrameRegisters;
    ULONG CurrentFrame;
};

typedef
INT
(*PDEBUGGER_COMMAND_ROUTINE) (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine implements a registered debugger command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

/*++

Structure Description:

    This structure defines a debugger command registration entry.

Members:

    Command - Stores a pointer to a string containing the name of the command
        to register.

    CommandRoutine - Stores a pointer to the routine to execute when this
        command is invoked.

    HelpText - Stores a pointer to a one-liner string describing what the
        command does.

--*/

typedef struct _DEBUGGER_COMMAND_ENTRY {
    PSTR Command;
    PDEBUGGER_COMMAND_ROUTINE CommandRoutine;
    PSTR HelpText;
} DEBUGGER_COMMAND_ENTRY, *PDEBUGGER_COMMAND_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOL
DbgrGetCommand (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine retrieves a command from the user or a remote client.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

VOID
DbgrpSetPromptText (
    PDEBUGGER_CONTEXT Context,
    PSTR Prompt
    );

/*++

Routine Description:

    This routine sets the command prompt to the given string.

Arguments:

    Context - Supplies a pointer to the application context.

    Prompt - Supplies a pointer to the null terminated string containing the
        prompt to set.

Return Value:

    None.

--*/

BOOL
DbgrpSplitCommandArguments (
    PSTR Input,
    PSTR Arguments[DEBUGGER_MAX_COMMAND_ARGUMENTS],
    PULONG ArgumentCount
    );

/*++

Routine Description:

    This routine splits a command line into its arguments.

Arguments:

    Input - Supplies a pointer to the input command line buffer.

    Arguments - Supplies a pointer to an array of strings that will receive any
        additional arguments.

    ArgumentCount - Supplies a pointer where the count of arguments will be
        returned.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

//
// Profiling functions
//

INT
DbgrProfilerInitialize (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes the debugger for profiler data consumption.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
DbgrProfilerDestroy (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys any structures used to consume profiler data.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

INT
DbgrDispatchProfilerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine handles a profiler command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments.

    ArgumentCount - Supplies the number of arguments in the Arguments array.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
DbgrProcessProfilerNotification (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine processes a profiler notification that the debuggee sends to
    the debugger. The routine should collect the profiler data and return as
    quickly as possible.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

//
// General debugger functions
//

PDEBUGGER_COMMAND_ENTRY
DbgrLookupCommand (
    PSTR Command
    );

/*++

Routine Description:

    This routine attempts to find a debugger command entry.

Arguments:

    Command - Supplies a pointer to the null-terminated string containing the
        name of the command that was invoked. This command is split on the
        period character, and the first segment is looked up.

Return Value:

    Returns a pointer to the command entry on success.

    NULL if there no such command, or on failure.

--*/

INT
DbgrInitialize (
    PDEBUGGER_CONTEXT Context,
    DEBUG_CONNECTION_TYPE ConnectionType
    );

/*++

Routine Description:

    This routine initializes data structures for common debugger functionality.

Arguments:

    Context - Supplies a pointer to the application context.

    ConnectionType - Supplies the type of debug connection to set the debugger
        up in.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

VOID
DbgrDestroy (
    PDEBUGGER_CONTEXT Context,
    DEBUG_CONNECTION_TYPE ConnectionType
    );

/*++

Routine Description:

    This routine destroys any data structures used for common debugger
    functionality.

Arguments:

    Context - Supplies a pointer to the application context.

    ConnectionType - Supplies the type of debug connection the debugger was set
        up in.

Return Value:

    None.

--*/

INT
DbgrConnect (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine establishes a link with the target debuggee. It is assumed that
    the underlying communication layer has already been established (COM ports
    have been opened and initialized, etc).

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrQuit (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine exits the local debugger.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrGo (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine interprets the "go" command from the user.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrSingleStep (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine steps the target by a single instruction.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrGetSetRegisters (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine prints or modifies the target machine's registers.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrGetSetSpecialRegisters (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine prints or modifies the target machine's special registers.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrPrintCallStack (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine prints the current call stack.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrSetFrame (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine changes the current stack frame, so that local variables may
    come from a different function in the call stack.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrDisassemble (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine disassembles instructions from the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrWaitForEvent (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine gets an event from the target, such as a break event or other
    exception.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    0 on success,

    Returns an error code on failure.

--*/

INT
DbgrSearchSymbols (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine searches for symbols. Wildcards are accepted. If the search
    string is preceded by "modulename!" then only that module will be searched.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrDumpTypeCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine prints information about a type description or value. If only
    a type is specified, the type format will be printed. If an address is
    passed as a second parameter, then the values will be dumped. If a global
    or local variable is passed as the first parameter, the values will also be
    dumped.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrDumpMemory (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine prints the contents of debuggee memory to the screen.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrDumpList (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine interates over a linked list and prints out the structure
    information for each entry. It also performs basic validation on the list,
    checking for bad previous pointers.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrEditMemory (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine writes to the target memory space.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrEvaluate (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine evaluates a numerical expression and prints it out in both
    decimal and hexadecimal.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrShowSourceAtAddressCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine shows the source file for the provided address and highlights
    the specific line associated with the address.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrPrintLocals (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine prints the values of the local variables inside the currently
    selected stack frame.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
DbgrUnhighlightCurrentLine (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine restores the currently executing line to the normal background
    color in the source window.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

INT
DbgrListBreakPoints (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine lists all valid breakpoints in the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrEnableBreakPoint (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine lists all valid breakpoints in the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrDeleteBreakPoint (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine deletes a breakpoint from the target.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrCreateBreakPoint (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine creates a new breakpoint in the debuggee.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrStep (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine performs a source or assembly line step in the debugger.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrSetSourceStepping (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine turns source line stepping on or off.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrSetSourceLinePrinting (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine turns on or off the option to print the source file and line
    next to every text address.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrReturnToCaller (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine interprets the "go" command from the user.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrSetSymbolPathCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine sets or updates the symbol search path.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrSetSourcePathCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine sets or updates the source search path.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrReloadSymbols (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine unloads and reloads all symbols from the search path.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrSetSymbolPath (
    PDEBUGGER_CONTEXT Context,
    PSTR Path,
    BOOL Append
    );

/*++

Routine Description:

    This routine sets or updates the symbol search path.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the new symbol path. This could contain
        multiple symbol paths if separated by semicolons.

    Append - Supplies a boolean indicating whether the new path should replace
        or append the existing path.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrLoadExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine loads or unloads a debugger extension.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrSwitchProcessor (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine switches the debugger to another processor in kernel mode or
    thread in user mode.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrPrintProcessorBlock (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine prints the contents of the current processor block.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrDumpPointerSymbols (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine dumps memory at the provided address and attempts to match
    symbols at the dumped memory addresses.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrProfileCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine handles the profile command. It essentially just forwards on
    to the profile handler.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrRebootCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine handles the profile command. It essentially just forwards on
    to the profile handler.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrServerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine starts or stops a remote server interface.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrContinue (
    PDEBUGGER_CONTEXT Context,
    BOOL SetOneTimeBreak,
    ULONGLONG Address
    );

/*++

Routine Description:

    This routine sends the "go" command to the target, signaling to continue
    execution.

Arguments:

    Context - Supplies a pointer to the application context.

    SetOneTimeBreak - Supplies a flag indicating whether to go unconditionally
        (FALSE) or with a one-time breakpoint (TRUE).

    Address - Supplies the address of the one-time breakpoint if one was
        specified.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
DbgrShowSourceAtAddress (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address
    );

/*++

Routine Description:

    This routine loads the source file and highlights the source line
    corresponding to the given target address.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the target's executing virtual address.

Return Value:

    None.

--*/

INT
DbgrDumpType (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount,
    PVOID RawDataStream,
    ULONG RawDataStreamSizeInBytes
    );

/*++

Routine Description:

    This routine prints information about a type description or value. If only
    a type is specified, the type format will be printed. If an address is
    passed as a second parameter, then the values will be dumped. If a global
    or local variable is passed as the first parameter, the values will also be
    dumped.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies a pointer to an array of argument strings.

    ArgumentCount - Supplies the number of arguments in the argument array.

    RawDataStream - Supplies the actual memory to dump in the given type form.
        If this parameter is non-NULL, then the AddressString parameter is
        ignored. The size of this buffer must be non-zero.

    RawDataStreamSizeInBytes - Supplies the size of the raw data stream buffer,
        in bytes.

Return Value:

    0 if the information was printed successfully.

    Returns an error code on failure.

--*/

INT
DbgrpHighlightExecutingLine (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG LineNumber
    );

/*++

Routine Description:

    This routine highlights the currently executing source line and scrolls to
    it, or removes the highlight.

Arguments:

    Context - Supplies the application context.

    LineNumber - Supplies the one-based line number to highlight, or 0 to
        disable highlighting.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
DbgrpLoadSourceFile (
    PDEBUGGER_CONTEXT Context,
    PSTR Path,
    PSTR *FoundPath,
    PVOID *Contents,
    PULONGLONG Size
    );

/*++

Routine Description:

    This routine loads a source file into memory.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the path to load.

    FoundPath - Supplies a pointer where a pointer to the path of the actual
        file loaded will be returned. The path may be modified by the source
        path list, so this represents the final found path.

    Contents - Supplies a pointer where a pointer to the loaded file will be
        returned on success. The caller is responsible for freeing this memory.

    Size - Supplies a pointer where the size of the file will be returned on
        success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgrpAddSourcePath (
    PDEBUGGER_CONTEXT Context,
    PSTR PathString
    );

/*++

Routine Description:

    This routine adds a source path entry to the given application context.

Arguments:

    Context - Supplies a pointer to the application context.

    PathString - Supplies a pointer to the path string, which takes the form
        prefix=path. If there is no equals sign, then the prefix is assumed to
        be empty.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
DbgrpDestroyAllSourcePaths (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys all source path entries in the given application
    context.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

PSTR
DbgGetAddressSymbol (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PFUNCTION_SYMBOL *Function
    );

/*++

Routine Description:

    This routine gets a descriptive string version of the given address,
    including the module and function name if possible. It is the caller's
    responsibility to free the returned string.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the virtual address of the target to get information
        about.

    Function - Supplies an optional pointer where the function symbol will be
        returned if this turned out to be a function.

Return Value:

    Returns a null-terminated string if successfull, or NULL on failure.

--*/

BOOL
DbgGetDataSymbolTypeInformation (
    PDATA_SYMBOL DataSymbol,
    PTYPE_SYMBOL *TypeSymbol,
    PUINTN TypeSize
    );

/*++

Routine Description:

    This routine computes the type and type size of the given data symbol.

Arguments:

    DataSymbol - Supplies a pointer to the data symbol whose type and type size
        are to be calculated.

    TypeSymbol - Supplies a pointer that receives a pointer to the type symbol
        that corresponds to the given data symbol.

    TypeSize - Supplies a pointer that receives the type size of the given
        data symbol.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

INT
DbgGetDataSymbolAddress (
    PDEBUGGER_CONTEXT Context,
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL DataSymbol,
    ULONGLONG DebasedPc,
    PULONGLONG Address
    );

/*++

Routine Description:

    This routine returns the memory address of the given data symbol.

Arguments:

    Context - Supplies a pointer to the application context.

    Symbols - Supplies a pointer to the module symbols.

    DataSymbol - Supplies a pointer to the data symbol whose address is to be
        returned.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus the
        loaded base difference of the module).

    Address - Supplies a pointer where the debased memory address of the symbol
        will be returned. That is, the caller needs to add any loaded base
        difference of the module to this value.

Return Value:

    0 on success.

    ENOENT if the data symbol is not currently valid.

    ERANGE if the data symbol is not stored in memory.

    Other error codes on other failures.

--*/

INT
DbgGetDataSymbolData (
    PDEBUGGER_CONTEXT Context,
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL DataSymbol,
    ULONGLONG DebasedPc,
    PVOID DataStream,
    ULONG DataStreamSize,
    PSTR Location,
    ULONG LocationSize
    );

/*++

Routine Description:

    This routine returns the data contained by the given data symbol.

Arguments:

    Context - Supplies a pointer to the application context.

    Symbols - Supplies a pointer to the module symbols.

    DataSymbol - Supplies a pointer to the data symbol whose data is to be
        retrieved.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus the
        loaded base difference of the module).

    DataStream - Supplies a pointer that receives the data from the data symbol.

    DataStreamSize - Supplies the size of the data stream buffer.

    Location - Supplies an optional pointer where a string describing the
        location of the data symbol will be returned on success.

    LocationSize - Supplies the size of the location in bytes.

Return Value:

    0 on success.

    ENOENT if the data symbol is not currently active given the current state
    of the machine.

    Returns an error code on failure.

--*/

INT
DbgPrintDataSymbol (
    PDEBUGGER_CONTEXT Context,
    PDEBUG_SYMBOLS Symbols,
    PDATA_SYMBOL DataSymbol,
    ULONGLONG DebasedPc,
    ULONG SpaceLevel,
    ULONG RecursionDepth
    );

/*++

Routine Description:

    This routine prints the location and value of a data symbol.

Arguments:

    Context - Supplies a pointer to the application context.

    Symbols - Supplies a pointer to the module symbols.

    DataSymbol - Supplies a pointer to the data symbol to print.

    DebasedPc - Supplies the program counter value, assuming the image were
        loaded at its preferred base address (that is, actual PC minus the
        loaded base difference of the module).

    SpaceLevel - Supplies the number of spaces to print after every newline.
        Used for nesting types.

    RecursionDepth - Supplies how many times this should recurse on structure
        members. If 0, only the name of the type is printed.

Return Value:

    0 on success.

    ENOENT if the data symbol is not currently active given the current state
    of the machine.

    Returns an error code on failure.

--*/

INT
DbgGetRegister (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    ULONG RegisterNumber,
    PULONGLONG RegisterValue
    );

/*++

Routine Description:

    This routine returns the contents of a register given a debug symbol
    register index.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the current machine context.

    RegisterNumber - Supplies the register index to get.

    RegisterValue - Supplies a pointer where the register value will be
        returned on success.

Return Value:

    0 on success.

    EINVAL if the register number is invalid.

    Other error codes on other failures.

--*/

INT
DbgSetRegister (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    ULONG RegisterNumber,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the contents of a register given its register number.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the current machine context. The register
        value will be set in this context.

    RegisterNumber - Supplies the register index to set.

    Value - Supplies the new value to set.

Return Value:

    0 on success.

    EINVAL if the register number is invalid.

    Other error codes on other failures.

--*/

INT
DbgPrintTypeByName (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PSTR TypeName,
    ULONG SpaceLevel,
    ULONG RecursionCount
    );

/*++

Routine Description:

    This routine prints a structure or value at a specified address, whose type
    is specified by a string.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies a target address pointer where the data resides.

    TypeName - Supplies a pointer to a string containing the type name to get.
        This should start with a type name, and can use dot '.' notation to
        specify field members, and array[] notation to specify dereferences.

    SpaceLevel - Supplies the number of spaces worth of indentation to print
        for subsequent lines.

    RecursionCount - Supplies the number of substructures to recurse into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
DbgPrintType (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL Type,
    PVOID Data,
    UINTN DataSize,
    ULONG SpaceLevel,
    ULONG RecursionCount
    );

/*++

Routine Description:

    This routine prints the given type to the debugger console.

Arguments:

    Context - Supplies a pointer to the application context.

    Type - Supplies a pointer to the data type to print.

    Data - Supplies a pointer to the data contents.

    DataSize - Supplies the size of the data buffer in bytes.

    SpaceLevel - Supplies the number of spaces worth of indentation to print
        for subsequent lines.

    RecursionCount - Supplies the number of substructures to recurse into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

VOID
DbgPrintStringData (
    PSTR String,
    UINTN Size,
    ULONG SpaceDepth
    );

/*++

Routine Description:

    This routine prints string data to the debugger console.

Arguments:

    String - Supplies a pointer to the string data.

    Size - Supplies the number of bytes to print out.

    SpaceDepth - Supplies the indentation to use when breaking up a string into
        multiple lines.

Return Value:

    None.

--*/

BOOL
EvalGetRegister (
    PDEBUGGER_CONTEXT Context,
    PCSTR Register,
    PULONGLONG Value
    );

/*++

Routine Description:

    This routine gets the value of a register by name.

Arguments:

    Context - Supplies a pointer to the application context.

    Register - Supplies the name of the register to get.

    Value - Supplies a pointer where the value of the register will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
EvalSetRegister (
    PDEBUGGER_CONTEXT Context,
    PCSTR Register,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the value of a register by name.

Arguments:

    Context - Supplies a pointer to the application context.

    Register - Supplies the name of the register to get.

    Value - Supplies the value to set in the register.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/
