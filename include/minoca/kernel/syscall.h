/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    syscall.h

Abstract:

    This header contains definitions for the kernel interface to user-mode.

Author:

    Evan Green 6-Feb-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines whether or not a system call is eligible for being
// restarted based on its system call number.
//

#define IS_SYSTEM_CALL_NUMBER_RESTARTABLE(_SystemCallNumber) \
    (((_SystemCallNumber) != SystemCallRestoreContext) &&     \
     ((_SystemCallNumber) != SystemCallExecuteImage))

//
// This macro determines whether or not a system call is eligible for being
// restarted based on its result.
//

#define IS_SYSTEM_CALL_RESULT_RESTARTABLE(_SystemCallResult) \
    (((_SystemCallResult) == STATUS_RESTART_AFTER_SIGNAL) || \
     ((_SystemCallResult) == STATUS_RESTART_NO_SIGNAL))

//
// This macro determines whether or not a system call is eligible for being
// restarted after a signal is dispatched based on its result.
//

#define IS_SYSTEM_CALL_RESULT_RESTARTABLE_AFTER_SIGNAL(_SystemCallResult) \
    ((_SystemCallResult) == STATUS_RESTART_AFTER_SIGNAL)

//
// This macro determines whether or not a signal call is eligible for being
// restarted if no signal is applied based on its result.
//

#define IS_SYSTEM_CALL_RESULT_RESTARTABLE_NO_SIGNAL(_SystemCallResult) \
    IS_SYSTEM_CALL_RESULT_RESTARTABLE(_SystemCallResult)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the attributes applied to all system call structures (namely
// alignment for fast copies).
//

#define SYSCALL_STRUCT ALIGNED64

#define SYS_WAIT_TIME_INDEFINITE MAX_ULONG
#define SYS_WAIT_TIME_MAX (MAX_ULONG - 1)

//
// Define open flags.
//

#define SYS_OPEN_FLAG_CREATE                  0x00000001
#define SYS_OPEN_FLAG_TRUNCATE                0x00000002
#define SYS_OPEN_FLAG_FAIL_IF_EXISTS          0x00000004
#define SYS_OPEN_FLAG_APPEND                  0x00000008
#define SYS_OPEN_FLAG_DIRECTORY               0x00000010
#define SYS_OPEN_FLAG_NON_BLOCKING            0x00000020
#define SYS_OPEN_FLAG_SHARED_MEMORY           0x00000040
#define SYS_OPEN_FLAG_NO_SYMBOLIC_LINK        0x00000080
#define SYS_OPEN_FLAG_SYNCHRONIZED            0x00000100
#define SYS_OPEN_FLAG_NO_CONTROLLING_TERMINAL 0x00000200
#define SYS_OPEN_FLAG_NO_ACCESS_TIME          0x00000400
#define SYS_OPEN_FLAG_ASYNCHRONOUS            0x00000800

#define SYS_OPEN_ACCESS_SHIFT 29
#define SYS_OPEN_FLAG_READ    (IO_ACCESS_READ << SYS_OPEN_ACCESS_SHIFT)
#define SYS_OPEN_FLAG_WRITE   (IO_ACCESS_WRITE << SYS_OPEN_ACCESS_SHIFT)
#define SYS_OPEN_FLAG_EXECUTE (IO_ACCESS_EXECUTE << SYS_OPEN_ACCESS_SHIFT)

#define SYS_OPEN_FLAG_CLOSE_ON_EXECUTE        0x10000000

//
// Define the mask of system call open flags that get translated directly to
// kernel open flags.
//

#define SYS_OPEN_FLAG_MASK                      \
    (SYS_OPEN_FLAG_CREATE |                     \
     SYS_OPEN_FLAG_TRUNCATE |                   \
     SYS_OPEN_FLAG_FAIL_IF_EXISTS |             \
     SYS_OPEN_FLAG_APPEND |                     \
     SYS_OPEN_FLAG_DIRECTORY |                  \
     SYS_OPEN_FLAG_NON_BLOCKING |               \
     SYS_OPEN_FLAG_SHARED_MEMORY |              \
     SYS_OPEN_FLAG_NO_SYMBOLIC_LINK |           \
     SYS_OPEN_FLAG_SYNCHRONIZED |               \
     SYS_OPEN_FLAG_NO_CONTROLLING_TERMINAL |    \
     SYS_OPEN_FLAG_NO_ACCESS_TIME |             \
     SYS_OPEN_FLAG_ASYNCHRONOUS)

#define SYS_FILE_CONTROL_EDITABLE_STATUS_FLAGS \
    (SYS_OPEN_FLAG_APPEND |                    \
     SYS_OPEN_FLAG_NON_BLOCKING |              \
     SYS_OPEN_FLAG_SYNCHRONIZED |              \
     SYS_OPEN_FLAG_NO_ACCESS_TIME |            \
     SYS_OPEN_FLAG_ASYNCHRONOUS)

//
// Define delete flags.
//

#define SYS_DELETE_FLAG_SHARED_MEMORY 0x00000001
#define SYS_DELETE_FLAG_DIRECTORY     0x00000002

//
// Define mount flags.
//

#define SYS_MOUNT_FLAG_UNMOUNT         0x00000001
#define SYS_MOUNT_FLAG_BIND            0x00000002
#define SYS_MOUNT_FLAG_RECURSIVE       0x00000004
#define SYS_MOUNT_FLAG_READ            0x00000008
#define SYS_MOUNT_FLAG_WRITE           0x00000010
#define SYS_MOUNT_FLAG_TARGET_UNLINKED 0x00000020
#define SYS_MOUNT_FLAG_DETACH          0x00000040

//
// Define file I/O flags.
//

#define SYS_IO_FLAG_WRITE 0x00000001
#define SYS_IO_FLAG_MASK  0x00000001

//
// Define flush flags.
//

#define SYS_FLUSH_FLAG_ALL          0x00000001
#define SYS_FLUSH_FLAG_READ         0x00000002
#define SYS_FLUSH_FLAG_WRITE        0x00000004
#define SYS_FLUSH_FLAG_DISCARD      0x00000008

//
// Define memory mapping flags.
//

#define SYS_MAP_FLAG_READ      0x00000001
#define SYS_MAP_FLAG_WRITE     0x00000002
#define SYS_MAP_FLAG_EXECUTE   0x00000004
#define SYS_MAP_FLAG_SHARED    0x00000008
#define SYS_MAP_FLAG_FIXED     0x00000010
#define SYS_MAP_FLAG_ANONYMOUS 0x00000020

//
// Define memory mapping flush flags.
//

#define SYS_MAP_FLUSH_FLAG_ASYNC 0x00000001

//
// Define wait system call flags.
//

//
// Set this flag to return immediately if no signals are pending.
//

#define SYSTEM_CALL_WAIT_FLAG_RETURN_IMMEDIATELY 0x00000001

//
// Set this flag to wait specifically for children that have stopped, and
// discard the signal when found.
//

#define SYSTEM_CALL_WAIT_FLAG_STOPPED_CHILDREN   0x00000002

//
// Set this flag to wait specifially for continued children, and discard the
// signal when found.
//

#define SYSTEM_CALL_WAIT_FLAG_CONTINUED_CHILDREN 0x00000004

//
// This bitmask waits for any child action (exited, stopped, or continued).
//

#define SYSTEM_CALL_WAIT_FLAG_CHILD_MASK \
    (SYSTEM_CALL_WAIT_FLAG_EXITED_CHILDREN | \
     SYSTEM_CALL_WAIT_FLAG_STOPPED_CHILDREN | \
     SYSTEM_CALL_WAIT_FLAG_CONTINUED_CHILDREN)

//
// Set this flag to wait specifically for children that have exited, and
// discard the signal when found.
//

#define SYSTEM_CALL_WAIT_FLAG_EXITED_CHILDREN    0x00000008

//
// Set this flag to not discard a pending child signal.
//

#define SYSTEM_CALL_WAIT_FLAG_DONT_DISCARD_CHILD 0x00000010

//
// Define the polling events.
//

#define POLL_EVENT_IN                0x00000001
#define POLL_EVENT_IN_HIGH_PRIORITY  0x00000002
#define POLL_EVENT_OUT               0x00000004
#define POLL_EVENT_OUT_HIGH_PRIORITY 0x00000008
#define POLL_EVENT_ERROR             0x00000010
#define POLL_EVENT_DISCONNECTED      0x00000020
#define POLL_EVENT_INVALID_HANDLE    0x00000040

//
// Define the mask of error events.
//

#define POLL_ERROR_EVENTS \
    (POLL_EVENT_ERROR | POLL_EVENT_DISCONNECTED | POLL_EVENT_INVALID_HANDLE)

//
// Define the mask of events that is always returned.
//

#define POLL_NONMASKABLE_EVENTS \
    (POLL_EVENT_ERROR | POLL_EVENT_DISCONNECTED | POLL_EVENT_INVALID_HANDLE)

//
// Define the mask of events that are always returned for files.
//

#define POLL_NONMASKABLE_FILE_EVENTS \
    (POLL_EVENT_IN | POLL_EVENT_IN_HIGH_PRIORITY | POLL_EVENT_OUT | \
     POLL_EVENT_OUT_HIGH_PRIORITY)

//
// Define the effective access permission flags.
//

#define EFFECTIVE_ACCESS_EXECUTE     0x00000001
#define EFFECTIVE_ACCESS_WRITE       0x00000002
#define EFFECTIVE_ACCESS_READ        0x00000004

//
// Define the timer control flags.
//

#define TIMER_CONTROL_FLAG_USE_TIMER_NUMBER 0x00000001
#define TIMER_CONTROL_FLAG_SIGNAL_THREAD    0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SYSTEM_CALL_NUMBER {
    SystemCallInvalid,
    SystemCallRestoreContext,
    SystemCallExitThread,
    SystemCallOpen,
    SystemCallClose,
    SystemCallPerformIo,
    SystemCallCreatePipe,
    SystemCallCreateThread,
    SystemCallForkProcess,
    SystemCallExecuteImage,
    SystemCallChangeDirectory,
    SystemCallSetSignalHandler,
    SystemCallSendSignal,
    SystemCallGetSetProcessId,
    SystemCallSetSignalBehavior,
    SystemCallWaitForChildProcess,
    SystemCallSuspendExecution,
    SystemCallExitProcess,
    SystemCallPoll,
    SystemCallSocketCreate,
    SystemCallSocketBind,
    SystemCallSocketListen,
    SystemCallSocketAccept,
    SystemCallSocketConnect,
    SystemCallSocketPerformIo,
    SystemCallFileControl,
    SystemCallGetSetFileInformation,
    SystemCallDebug,
    SystemCallSeek,
    SystemCallCreateSymbolicLink,
    SystemCallReadSymbolicLink,
    SystemCallDelete,
    SystemCallRename,
    SystemCallMountOrUnmount,
    SystemCallQueryTimeCounter,
    SystemCallTimerControl,
    SystemCallGetEffectiveAccess,
    SystemCallDelayExecution,
    SystemCallUserControl,
    SystemCallFlush,
    SystemCallGetResourceUsage,
    SystemCallLoadDriver,
    SystemCallFlushCache,
    SystemCallGetCurrentDirectory,
    SystemCallSocketGetSetInformation,
    SystemCallSocketShutdown,
    SystemCallCreateHardLink,
    SystemCallMapOrUnmapMemory,
    SystemCallFlushMemory,
    SystemCallLocateDeviceInformation,
    SystemCallGetSetDeviceInformation,
    SystemCallOpenDevice,
    SystemCallGetSetSystemInformation,
    SystemCallResetSystem,
    SystemCallSetSystemTime,
    SystemCallSetMemoryProtection,
    SystemCallSetThreadIdentity,
    SystemCallSetThreadPermissions,
    SystemCallSetSupplementaryGroups,
    SystemCallSocketCreatePair,
    SystemCallCreateTerminal,
    SystemCallSocketPerformVectoredIo,
    SystemCallSetThreadPointer,
    SystemCallUserLock,
    SystemCallSetThreadIdPointer,
    SystemCallSetUmask,
    SystemCallDuplicateHandle,
    SystemCallPerformVectoredIo,
    SystemCallSetITimer,
    SystemCallSetResourceLimit,
    SystemCallSetBreak,
    SystemCallCount
} SYSTEM_CALL_NUMBER, *PSYSTEM_CALL_NUMBER;

typedef enum _SIGNAL_MASK_OPERATION {
    SignalMaskOperationNone,
    SignalMaskOperationOverwrite,
    SignalMaskOperationSet,
    SignalMaskOperationClear,
} SIGNAL_MASK_OPERATION, *PSIGNAL_MASK_OPERATION;

typedef enum _SIGNAL_MASK_TYPE {
    SignalMaskTypeInvalid,
    SignalMaskBlocked,
    SignalMaskIgnored,
    SignalMaskHandled,
    SignalMaskPending,
} SIGNAL_MASK_TYPE, *PSIGNAL_MASK_TYPE;

typedef enum _SIGNAL_TARGET_TYPE {
    SignalTargetTypeInvalid,
    SignalTargetProcess,
    SignalTargetThread,
    SignalTargetAllProcesses,
    SignalTargetProcessGroup,
    SignalTargetCurrentProcess,
    SignalTargetCurrentProcessGroup,
} SIGNAL_TARGET_TYPE, *PSIGNAL_TARGET_TYPE;

typedef enum _FILE_LOCK_TYPE {
    FileLockInvalid,
    FileLockRead,
    FileLockReadWrite,
    FileLockUnlock,
    FileLockTypeCount
} FILE_LOCK_TYPE, *PFILE_LOCK_TYPE;

typedef enum _FILE_CONTROL_COMMAND {
    FileControlCommandInvalid,
    FileControlCommandDuplicate,
    FileControlCommandGetFlags,
    FileControlCommandSetFlags,
    FileControlCommandGetStatusAndAccess,
    FileControlCommandSetStatus,
    FileControlCommandGetSignalOwner,
    FileControlCommandSetSignalOwner,
    FileControlCommandGetLock,
    FileControlCommandSetLock,
    FileControlCommandBlockingSetLock,
    FileControlCommandGetFileInformation,
    FileControlCommandSetFileInformation,
    FileControlCommandSetDirectoryFlag,
    FileControlCommandCloseFrom,
    FileControlCommandGetPath,
    FileControlCommandCount
} FILE_CONTROL_COMMAND, *PFILE_CONTROL_COMMAND;

typedef enum _TIMER_OPERATION {
    TimerOperationInvalid,
    TimerOperationCreateTimer,
    TimerOperationDeleteTimer,
    TimerOperationGetTimer,
    TimerOperationSetTimer
} TIMER_OPERATION, *PTIMER_OPERATION;

typedef enum _ITIMER_TYPE {
    ITimerReal,
    ITimerVirtual,
    ITimerProfile,
    ITimerTypeCount
} ITIMER_TYPE, *PITIMER_TYPE;

typedef enum _RESOURCE_USAGE_REQUEST {
    ResourceUsageRequestInvalid,
    ResourceUsageRequestProcess,
    ResourceUsageRequestProcessChildren,
    ResourceUsageRequestThread,
} RESOURCE_USAGE_REQUEST, *PRESOURCE_USAGE_REQUEST;

//
// System call parameter structures
//

/*++

Structure Description:

    This structure defines the system call parameters for exiting the current
    thread.

Members:

    UnmapAddress - Supplies an optional pointer to a region to unmap (usually
        the thread stack if it was allocated in user mode).

    UnmapSize - Supplies the size of the region to unmap. This must be aligned
        to a page boundary.

--*/

typedef struct _SYSTEM_CALL_EXIT_THREAD {
    PVOID UnmapAddress;
    UINTN UnmapSize;
} SYSCALL_STRUCT SYSTEM_CALL_EXIT_THREAD, *PSYSTEM_CALL_EXIT_THREAD;

/*++

Structure Description:

    This structure defines the system call parameters for the open call.

Members:

    Directory - Stores an optional handle to the directory to start path
        traversal from if the specified path is relative. Supply INVALID_HANDLE
        here to use the current directory for relative paths.

    Path - Stores a pointer to a string containing the path of the object to
        open.

    PathBufferLength - Stores the length of the path buffer, in bytes,
        including the null terminator.

    Flags - Stores a bitfield of flags. See SYS_OPEN_FLAG_* definitions.

    CreatePermissions - Stores the permissions to apply to a created file.

    Handle - Stores a handle where the file handle will be returned on success.

--*/

typedef struct _SYSTEM_CALL_OPEN {
    HANDLE Directory;
    PCSTR Path;
    ULONG PathBufferLength;
    ULONG Flags;
    FILE_PERMISSIONS CreatePermissions;
    HANDLE Handle;
} SYSCALL_STRUCT SYSTEM_CALL_OPEN, *PSYSTEM_CALL_OPEN;

/*++

Structure Description:

    This structure defines the system call parameters for the call to do I/O.

Members:

    Handle - Stores the handle to do file I/O to.

    Buffer - Stores the buffer (in user mode) to read from or write to.

    Flags - Stores flags related to the I/O operation. See SYS_IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Stores the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        SYS_WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Offset - Stores the offset the I/O should occur at. Supply -1ULL to use the
        current file pointer offset.

    Size - Stores the number of bytes of I/O to complete on input.

--*/

typedef struct _SYSTEM_CALL_PERFORM_IO {
    HANDLE Handle;
    PVOID Buffer;
    ULONG Flags;
    ULONG TimeoutInMilliseconds;
    IO_OFFSET Offset;
    INTN Size;
} SYSCALL_STRUCT SYSTEM_CALL_PERFORM_IO, *PSYSTEM_CALL_PERFORM_IO;

/*++

Structure Description:

    This structure defines the system call parameters for the call to do I/O.

Members:

    Handle - Stores the handle to do file I/O to.

    Buffer - Stores the buffer (in user mode) to read from or write to.

    Flags - Stores flags related to the I/O operation. See SYS_IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Stores the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        SYS_WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Offset - Stores the offset the I/O should occur at. Supply -1ULL to use the
        current file pointer offset.

    Size - Stores the number of bytes of I/O to complete on input.

    VectoryArray - Stores a pointer to an array of I/O vector structures which
        specify the buffers to read or write.

    VectorCount - Stores the number of elements in the vector array.

--*/

typedef struct _SYSTEM_CALL_PERFORM_VECTORED_IO {
    HANDLE Handle;
    PVOID Buffer;
    ULONG Flags;
    ULONG TimeoutInMilliseconds;
    IO_OFFSET Offset;
    INTN Size;
    PIO_VECTOR VectorArray;
    UINTN VectorCount;
} SYSCALL_STRUCT SYSTEM_CALL_PERFORM_VECTORED_IO,
    *PSYSTEM_CALL_PERFORM_VECTORED_IO;

/*++

Structure Description:

    This structure defines the system call parameters for the create pipe call.

Members:

    Directory - Stores an optional handle to the directory to start path
        traversal from if the specified path is relative. Supply INVALID_HANDLE
        here to use the current directory for relative paths.

    Path - Stores an optional pointer to a names path for the pipe.

    PathLength - Stores the length of the path buffer in bytes, including the
        null terminator.

    OpenFlags - Stores the set of open flags associated with the handle. Only
        SYS_OPEN_FLAG_CLOSE_ON_EXECUTE and SYS_OPEN_FLAG_NON_BLOCKING are
        accepted.

    Permissions - Stores the permissions to apply to the new pipe.

    ReadHandle - Stores the returned handle to the read side of the pipe.

    WriteHandle - Stores the returned handle to the write side of the pipe.

--*/

typedef struct _SYSTEM_CALL_CREATE_PIPE {
    HANDLE Directory;
    PSTR Path;
    ULONG PathLength;
    ULONG OpenFlags;
    FILE_PERMISSIONS Permissions;
    HANDLE ReadHandle;
    HANDLE WriteHandle;
} SYSCALL_STRUCT SYSTEM_CALL_CREATE_PIPE, *PSYSTEM_CALL_CREATE_PIPE;

/*++

Structure Description:

    This structure defines the system call parameters for the create thread
    call.

Members:

    Name - Stores a pointer to an optional string containing the name of the
        new thread.

    NameBufferLength - Stores the length of the name buffer. Supply 0 if no
        name was given.

    ThreadRoutine - Stores a pointer to the function that should be executed
        on the new thread.

    Parameter - Stores a pointer parameter that will be passed directly to the
        thread routine.

    StackBase - Stores an optional pointer on input where the thread stack
        should be located. Supply NULL to have the stack placed anywhere. On
        output, contains the base address of the stack.

    StackSize - Stores the requested size of the stack for this thread. Supply
        0 to use the system default value. On output, contains the actual size
        of the stack.

    ThreadPointer - Stores the thread pointer to set for the new thread.

    ThreadId - Stores an optional pointer where the thread ID is returned. This
        address is also set as the thread's thread ID address. If the thread
        terminates, zero is written to this value and a UserLockWake operation
        is called on that address to wake up one thread.

--*/

typedef struct _SYSTEM_CALL_CREATE_THREAD {
    PSTR Name;
    ULONG NameBufferLength;
    PTHREAD_ENTRY_ROUTINE ThreadRoutine;
    PVOID Parameter;
    PVOID StackBase;
    ULONG StackSize;
    PVOID ThreadPointer;
    PTHREAD_ID ThreadId;
} SYSCALL_STRUCT SYSTEM_CALL_CREATE_THREAD, *PSYSTEM_CALL_CREATE_THREAD;

/*++

Structure Description:

    This structure defines the system call parameters for the fork call.

Members:

    Flags - Supplies a bitfield of flags governing the behavior of the child.

--*/

typedef struct _SYSTEM_CALL_FORK {
    ULONG Flags;
} SYSCALL_STRUCT SYSTEM_CALL_FORK, *PSYSTEM_CALL_FORK;

/*++

Structure Description:

    This structure defines the system call parameters for the execute image
    system call.

Members:

    Environment - Supplies the image name, arguments, and environment.

--*/

typedef struct _SYSTEM_CALL_EXECUTE_IMAGE {
    PROCESS_ENVIRONMENT Environment;
} SYSCALL_STRUCT SYSTEM_CALL_EXECUTE_IMAGE, *PSYSTEM_CALL_EXECUTE_IMAGE;

/*++

Structure Description:

    This structure defines the system call parameters for changing the current
    directory or the root directory.

Members:

    Root - Stores a boolean indicating whether to change the current working
        directory (FALSE) or the current root directory (TRUE).

    Buffer - Stores a pointer to the buffer containing the directory to change
        to. Either this parameter or the handle must be valid.

    BufferLength - Stores the length of the aforementioned buffer, in bytes,
        including the null terminator.

    Handle - Stores the open handle to the directory to change to. If this is
        not INVALID_HANDLE, then this will be used. Otherwise, the path
        pointed to by the buffer will be used.

--*/

typedef struct _SYSTEM_CALL_CHANGE_DIRECTORY {
    BOOL Root;
    PSTR Buffer;
    ULONG BufferLength;
    HANDLE Handle;
} SYSCALL_STRUCT SYSTEM_CALL_CHANGE_DIRECTORY, *PSYSTEM_CALL_CHANGE_DIRECTORY;

/*++

Structure Description:

    This structure defines the system call parameters for setting a new signal
    handler routine.

Members:

    SignalHandler - Stores a pointer to the user mode routine that will be
        called to handle signals. The parameters to the function are the
        signal number and optional parameter, both of which will be passed in
        registers. On output, this pointer will contain the original signal
        handler pointer, or NULL if no signal handler was previouly registered.

--*/

typedef struct _SYSTEM_CALL_SET_SIGNAL_HANDLER {
    PVOID SignalHandler;
} SYSCALL_STRUCT SYSTEM_CALL_SET_SIGNAL_HANDLER,
    *PSYSTEM_CALL_SET_SIGNAL_HANDLER;

/*++

Structure Description:

    This structure defines the system call parameters for sending a signal to
    a process, process group, or thread.

Members:

    TargetType - Supplies the target to which the signal is being sent. It can
        be either a process, process group, or thread.

    TargetId - Supplies the ID for the signal's target process, process group,
        or thread.

    SignalNumber - Stores the signal number to send.

    SignalCode - Stores the code to send. For user generated signals this must
        be less than or equal to 0, otherwise it will be set to 0. See
        SIGNAL_CODE_* definitions.

    SignalParameter - Stores the parameter to send with the signal for real time
        signals.

    Status - Stores the result returned by the kernel from the operation.

--*/

typedef struct _SYSTEM_CALL_SEND_SIGNAL {
    SIGNAL_TARGET_TYPE TargetType;
    ULONG TargetId;
    ULONG SignalNumber;
    SHORT SignalCode;
    UINTN SignalParameter;
} SYSCALL_STRUCT SYSTEM_CALL_SEND_SIGNAL, *PSYSTEM_CALL_SEND_SIGNAL;

/*++

Structure Description:

    This structure defines the system call parameters for getting and setting
    various process IDs, including the process ID, thread ID, process group
    ID, session ID, and parent process ID.

Members:

    ProcessIdType - Stores the type of identifier to get or set. Not all types
        can be set.

    ProcessId - Stores the process ID parameter on input if applicable, and
        returns the result on success.

    NewValue - Stores the new value to set for types that can be set (like
        process group ID).

    Set - Stores a boolean indicating whether to get the process ID of the
        given type or set it.

--*/

typedef struct _SYSTEM_CALL_GET_SET_PROCESS_ID {
    PROCESS_ID_TYPE ProcessIdType;
    PROCESS_ID ProcessId;
    PROCESS_ID NewValue;
    BOOL Set;
} SYSCALL_STRUCT SYSTEM_CALL_GET_SET_PROCESS_ID,
    *PSYSTEM_CALL_GET_SET_PROCESS_ID;

/*++

Structure Description:

    This structure defines the system call parameters for setting the current
    thread signal behavior.

Members:

    Operation - Stores the operation to perform: set, clear, or overwrite.

    MaskType - Stores the signal mask to operate on.

    SignalSet - Stores the new signal set on input. On output, contains the
        original signal set of the specified type.

--*/

typedef struct _SYSTEM_CALL_SET_SIGNAL_BEHAVIOR {
    SIGNAL_MASK_OPERATION Operation;
    SIGNAL_MASK_TYPE MaskType;
    SIGNAL_SET SignalSet;
} SYSCALL_STRUCT SYSTEM_CALL_SET_SIGNAL_BEHAVIOR,
    *PSYSTEM_CALL_SET_SIGNAL_BEHAVIOR;

/*++

Structure Description:

    This structure defines the system call parameters for suspending execution
    until a signal comes in. This may be any old signal, or it may wait for
    and dequeue a child signal.

Members:

    Flags - Stores a bitfield of flags governing behavior of the wait. See
        SYSTEM_CALL_WAIT_FLAG_* definititions.

    ChildPid - Stores the PID parameter to wait for on input. The value can be
        positive to wait on a specific pid, 0 to wait any child process whose
        process group is equal to that of the calling process, -1 to wait on
        any process, or another negative number to wait on a process whose
        process group ID is equal to the absolute value of the PID parameter.
        On output, contains the child PID causing the signal, returned by the
        kernel. If the parameter indicating that the wait is for a child is
        FALSE, this parameter should be ignored by user mode.

    ChildExitValue - Stores the exit status code returned by the child process
        if the reason was that the child exited. Otherwise, this contains the
        signal number that caused the child to terminate, dump, or stop.

    Reason - Stores the reason for the child event. See CHILD_SIGNAL_REASON_*
        definitions.

    ResourceUsage - Stores an optional pointer where the kernel will fill in
        resource usage of the child on success.

--*/

typedef struct _SYSTEM_CALL_WAIT_FOR_CHILD {
    ULONG Flags;
    PROCESS_ID ChildPid;
    UINTN ChildExitValue;
    ULONG Reason;
    PRESOURCE_USAGE ResourceUsage;
} SYSCALL_STRUCT SYSTEM_CALL_WAIT_FOR_CHILD, *PSYSTEM_CALL_WAIT_FOR_CHILD;

/*++

Structure Description:

    This structure defines the system call parameters for suspending execution
    until a signal comes in.

Members:

    SignalOperation - Stores the operation to perform with the signal set: set,
        clear, overwrite, or none.

    SignalSet - Stores the signal set to apply for the duration of this call as
        dictated by the signal operation.

    TimeoutInMilliseconds - Stores the timeout in milliseconds the caller
        should wait. Set to SYS_WAIT_TIME_INDEFINITE to wait forever.

    SignalParameters - Stores an optional pointer where the signal information
        for the signal that occurred will be returned.

--*/

typedef struct _SYSTEM_CALL_SUSPEND_EXECUTION {
    SIGNAL_MASK_OPERATION SignalOperation;
    SIGNAL_SET SignalSet;
    ULONG TimeoutInMilliseconds;
    PSIGNAL_PARAMETERS SignalParameters;
} SYSCALL_STRUCT SYSTEM_CALL_SUSPEND_EXECUTION, *PSYSTEM_CALL_SUSPEND_EXECUTION;

/*++

Structure Description:

    This structure defines an element in the array of file descriptors to be
    polled.

Members:

    Handle - Stores the I/O handle to wait for.

    Events - Stores the bitmask of events to wait for.

    ReturnedEvents - Stores the bitmask of events that occurred for this file
        descriptor.

--*/

typedef struct _POLL_DESCRIPTOR {
    HANDLE Handle;
    USHORT Events;
    USHORT ReturnedEvents;
} POLL_DESCRIPTOR, *PPOLL_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the system call parameters for polling several I/O
    handles.

Members:

    SignalMask - Stores an optional pointer to a signal mask to set for the
        duration of the poll.

    Descriptors - Stores a pointer to a buffer containing an array of poll
        descriptors.

    DescriptorCount - Stores the number of elements in the descriptors array.

    TimeoutInMilliseconds - Stores the number of milliseconds to wait for one
        of the descriptors to become ready before giving up.

--*/

typedef struct _SYSTEM_CALL_POLL {
    PSIGNAL_SET SignalMask;
    PPOLL_DESCRIPTOR Descriptors;
    LONG DescriptorCount;
    ULONG TimeoutInMilliseconds;
} SYSCALL_STRUCT SYSTEM_CALL_POLL, *PSYSTEM_CALL_POLL;

/*++

Structure Description:

    This structure defines the system call parameters for creating a new
    socket.

Members:

    Domain - Stores the network domain to use on the socket.

    Type - Stores the socket connection type.

    Protocol - Stores the raw network protocol to use on the socket.
        These are network specific. For example, for IPv4 and IPv6, the values
        are taken from the Internet Assigned Numbers Authority (IANA).

    OpenFlags - Stores an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

    Socket - Stores the returned socket file descriptor on success.

--*/

typedef struct _SYSTEM_CALL_SOCKET_CREATE {
    NET_DOMAIN_TYPE Domain;
    NET_SOCKET_TYPE Type;
    ULONG Protocol;
    ULONG OpenFlags;
    HANDLE Socket;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_CREATE, *PSYSTEM_CALL_SOCKET_CREATE;

/*++

Structure Description:

    This structure defines the system call parameters for binding a socket to
    an address.

Members:

    Socket - Stores the socket to bind to.

    Address - Stores the local address to bind the socket to.

    Path - Stores a pointer to the path, in the case that this is a Unix socket.

    PathSize - Stores the size of the path, in bytes, including the null
        terminator.

--*/

typedef struct _SYSTEM_CALL_SOCKET_BIND {
    HANDLE Socket;
    NETWORK_ADDRESS Address;
    PSTR Path;
    UINTN PathSize;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_BIND, *PSYSTEM_CALL_SOCKET_BIND;

/*++

Structure Description:

    This structure defines the system call parameters for making a socket
    eligible to accept incoming connections.

Members:

    Socket - Stores the socket to activate.

    BacklogCount - Stores a suggested number of pending (un-accepted) incoming
        connections the kernel should queue before rejecting additional
        incoming connections.

--*/

typedef struct _SYSTEM_CALL_SOCKET_LISTEN {
    HANDLE Socket;
    ULONG BacklogCount;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_LISTEN, *PSYSTEM_CALL_SOCKET_LISTEN;

/*++

Structure Description:

    This structure defines the system call parameters for accepting a new
    incoming connection on a listening socket.

Members:

    Socket - Stores the socket to accept a new connection from.

    NewSocket - Stores the new socket file descriptor on success, which
        represents the new connection.

    Address - Stores the network address of the party that just created this
        new incoming connection.

    RemotePath - Stores a pointer where the remote path of the client socket
        will be copied on success. This only applies to local sockets.

    RemotePathSize - Stores on input the size of the remote path buffer. On
        output, contains the true size of the remote path, even if it was
        bigger than the input.

    OpenFlags - Stores an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

--*/

typedef struct _SYSTEM_CALL_SOCKET_ACCEPT {
    HANDLE Socket;
    HANDLE NewSocket;
    NETWORK_ADDRESS Address;
    PSTR RemotePath;
    UINTN RemotePathSize;
    ULONG OpenFlags;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_ACCEPT, *PSYSTEM_CALL_SOCKET_ACCEPT;

/*++

Structure Description:

    This structure defines the system call parameters for connecting to another
    socket.

Members:

    Socket - Stores the socket to use to initiate the connection.

    Address - Stores the network address to connect to.

    RemotePath - Stores a pointer to the remote path if this is a local socket.

    RemotePathSize - Stores the size of the remote path buffer in bytes,
        including the null terminator.

--*/

typedef struct _SYSTEM_CALL_SOCKET_CONNECT {
    HANDLE Socket;
    NETWORK_ADDRESS Address;
    PSTR RemotePath;
    UINTN RemotePathSize;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_CONNECT, *PSYSTEM_CALL_SOCKET_CONNECT;

/*++

Structure Description:

    This structure defines the system call parameters for sending or receiving
    socket data to or from a specified host. Sockets may also use the generic
    perform I/O system call if the caller does not with to specify or learn the
    remote host address.

Members:

    Socket - Stores the socket to use.

    Parameters - Stores a required pointer to the socket I/O parameters.

    Buffer - Stores the buffer to read from or write to.

--*/

typedef struct _SYSTEM_CALL_SOCKET_PERFORM_IO {
    HANDLE Socket;
    PSOCKET_IO_PARAMETERS Parameters;
    PVOID Buffer;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_PERFORM_IO, *PSYSTEM_CALL_SOCKET_PERFORM_IO;

/*++

Structure Description:

    This structure defines the system call parameters for performing socket
    I/O using an I/O vector.

Members:

    Socket - Stores the socket to use.

    Parameters - Stores a required pointer to the socket I/O parameters.

    VectorArray - Stores a pointer to an array of I/O vectors.

    VectorCount - Stores the number of elements in the vector array.

--*/

typedef struct _SYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO {
    HANDLE Socket;
    PSOCKET_IO_PARAMETERS Parameters;
    PIO_VECTOR VectorArray;
    UINTN VectorCount;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO,
    *PSYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO;

/*++

Structure Description:

    This structure defines the parameters of a file lock.

Members:

    Type - Stores the file lock type.

    Offset - Stores the starting offset of the file lock.

    Size - Stores the size of the file lock. If zero, then the lock runs to the
        end of the file.

    ProcessId - Stores the process ID of the process that owns the lock. This
        is returned when getting the lock, and is ignored when setting the
        lock.

--*/

typedef struct _FILE_LOCK {
    FILE_LOCK_TYPE Type;
    ULONGLONG Offset;
    ULONGLONG Size;
    PROCESS_ID ProcessId;
} FILE_LOCK, *PFILE_LOCK;

/*++

Structure Description:

    This structure defines a file path.

Members:

    Path - Stores a pointer to the path.

    PathSize - Stores the size of the path.

--*/

typedef struct _FILE_PATH {
    PSTR Path;
    UINTN PathSize;
} FILE_PATH, *PFILE_PATH;

/*++

Structure Description:

    This structure defines union of various parameters used by the file control
    call.

Members:

    DuplicateDescriptor - Stores the requested minimum file descriptor of the
        duplicate on input for duplicate operations. On output, returns the
        new open file descriptor.

    SetFileInformation - Stores the request to get or set file information.

    FileLock - Stores the file lock information.

    Flags - Stores the file descriptor flags.

    FilePath - Stores the path of the file.

    Owner - Stores the ID of the process to receive signals on asynchronous
        I/O events.

--*/

typedef union _FILE_CONTROL_PARAMETERS_UNION {
    HANDLE DuplicateDescriptor;
    SET_FILE_INFORMATION SetFileInformation;
    FILE_LOCK FileLock;
    ULONG Flags;
    FILE_PATH FilePath;
    PROCESS_ID Owner;
} FILE_CONTROL_PARAMETERS_UNION, *PFILE_CONTROL_PARAMETERS_UNION;

/*++

Structure Description:

    This structure defines the system call parameters for file control
    operations.

Members:

    File - Stores the file handle to operate on.

    Command - Stores the file control command.

    Parameters - Stores a pointer to any additional command dependent
        parameters.

--*/

typedef struct _SYSTEM_CALL_FILE_CONTROL {
    HANDLE File;
    FILE_CONTROL_COMMAND Command;
    PFILE_CONTROL_PARAMETERS_UNION Parameters;
} SYSCALL_STRUCT SYSTEM_CALL_FILE_CONTROL, *PSYSTEM_CALL_FILE_CONTROL;

/*++

Structure Description:

    This structure defines the system call parameters for the get/set file
    information system call.

Members:

    Directory - Stores an optional handle to the directory to start path
        traversal from if the specified path is relative. Supply INVALID_HANDLE
        here to use the current directory for relative paths.

    FilePath - Stores a pointer to the file path string to get file information
        for.

    FilePathSize - Stores the size of the file path buffer in bytes including
        the null terminator.

    FollowLink - Stores a boolean indicating what to do if the file path points
        to a symbolic link. If set to TRUE, the file information set or
        returned will be for the file the link points to. If FALSE, the call
        will set or get information for the link itself.

    Request - Stores the file information request. For "get file information"
        requests, the file properties will be returned here. For "set file
        information" requests, this contains the fields to set.

--*/

typedef struct _SYSTEM_CALL_GET_SET_FILE_INFORMATION {
    HANDLE Directory;
    PSTR FilePath;
    ULONG FilePathSize;
    BOOL FollowLink;
    SET_FILE_INFORMATION Request;
} SYSCALL_STRUCT SYSTEM_CALL_GET_SET_FILE_INFORMATION,
    *PSYSTEM_CALL_GET_SET_FILE_INFORMATION;

/*++

Structure Description:

    This structure defines the system call parameters for debug interface.

Members:

    Process - Stores the ID of the process the command is operating on.

    Command - Supplies the command information.

--*/

typedef struct _SYSTEM_CALL_DEBUG {
    PROCESS_ID Process;
    PROCESS_DEBUG_COMMAND Command;
} SYSCALL_STRUCT SYSTEM_CALL_DEBUG, *PSYSTEM_CALL_DEBUG;

/*++

Structure Description:

    This structure defines the system call parameters for a file seek operation.

Members:

    Handle - Stores the handle to seek on.

    Command - Stores the seek command.

    Offset - Stores the offset to apply from the seek command. On return from
        the system call this contains the offset (after any seek has been
        applied).

--*/

typedef struct _SYSTEM_CALL_SEEK {
    HANDLE Handle;
    SEEK_COMMAND Command;
    IO_OFFSET Offset;
} SYSCALL_STRUCT SYSTEM_CALL_SEEK, *PSYSTEM_CALL_SEEK;

/*++

Structure Description:

    This structure defines the system call parameters for creating a symbolic
    link.

Members:

    Directory - Stores an optional handle to the directory to start path
        traversal from if the specified path is relative. Supply INVALID_HANDLE
        here to use the current directory for relative paths.

    Path - Stores a pointer to the symbolic link path.

    PathSize - Stores the size of the symbolic link pointer in bytes including
        the null terminator.

    LinkDestinationBuffer - Stores a pointer containing the target path of the
        link.

    LinkDestinationBufferSize - Stores the size of the link destination
        buffer in bytes including the null terminator.

--*/

typedef struct _SYSTEM_CALL_CREATE_SYMBOLIC_LINK {
    HANDLE Directory;
    PSTR Path;
    ULONG PathSize;
    PSTR LinkDestinationBuffer;
    ULONG LinkDestinationBufferSize;
} SYSCALL_STRUCT SYSTEM_CALL_CREATE_SYMBOLIC_LINK,
    *PSYSTEM_CALL_CREATE_SYMBOLIC_LINK;

/*++

Structure Description:

    This structure defines the system call parameters for getting the value of
    a symbolic link.

Members:

    Directory - Stores an optional handle to the directory to start path
        traversal from if the specified path is relative. Supply INVALID_HANDLE
        here to use the current directory for relative paths.

    Path - Stores a pointer to the symbolic link path.

    PathSize - Stores the size of the symbolic link pointer in bytes including
        the null terminator.

    LinkDestinationBuffer - Stores a pointer to a buffer where the destination
        of the link will be returned. A null terminator is not written.

    LinkDestinationBufferSize - Stores the size of the link destination
        buffer in bytes.

    LinkDestinationSize - Stores a pointer where the actual size of the link
        destination will be returned on either success or a
        STATUS_BUFFER_TOO_SMALL case. On failure, 0 will be returned here. This
        size does not include a null terminator.

--*/

typedef struct _SYSTEM_CALL_READ_SYMBOLIC_LINK {
    HANDLE Directory;
    PSTR Path;
    ULONG PathSize;
    PSTR LinkDestinationBuffer;
    ULONG LinkDestinationBufferSize;
    ULONG LinkDestinationSize;
} SYSCALL_STRUCT SYSTEM_CALL_READ_SYMBOLIC_LINK,
    *PSYSTEM_CALL_READ_SYMBOLIC_LINK;

/*++

Structure Description:

    This structure defines the system call parameters for deleting an entry
    from a directory.

Members:

    Directory - Stores an optional handle to the directory to start path
        traversal from if the specified path is relative. Supply INVALID_HANDLE
        here to use the current directory for relative paths.

    Path - Stores a pointer to the entry to delete.

    PathSize - Stores the size of the path in bytes including the null
        terminator.

    Flags - Stores a bitfield of flags. See SYS_DELETE_FLAG_* definitions.

--*/

typedef struct _SYSTEM_CALL_DELETE {
    HANDLE Directory;
    PSTR Path;
    ULONG PathSize;
    ULONG Flags;
} SYSCALL_STRUCT SYSTEM_CALL_DELETE, *PSYSTEM_CALL_DELETE;

/*++

Structure Description:

    This structure defines the system call parameters for renaming a file or
    directory.

Members:

    SourceDirectory - Stores an optional handle to the directory to start
        relative source path searches from. If set to INVALID_HANDLE, relative
        source paths will start from the current working directory.

    SourcePath - Stores a pointer to the string containing the file or directory
        to rename.

    SourcePathSize - Stores the size of the source path string in bytes
        including the null terminator.

    DestinationDirectory - Stores an optional handle to the directory to start
        relative destination path searches from. If set to INVALID_HANDLE,
        relative destination paths will start from the current working
        directory.

    DestinationPath - Stores a pointer to the string containing the path to
        rename the file or directory to.

    DestinationPathSize - Stores the size of the destination path string in
        bytes including the null terminator.

--*/

typedef struct _SYSTEM_CALL_RENAME {
    HANDLE SourceDirectory;
    PSTR SourcePath;
    ULONG SourcePathSize;
    HANDLE DestinationDirectory;
    PSTR DestinationPath;
    ULONG DestinationPathSize;
} SYSCALL_STRUCT SYSTEM_CALL_RENAME, *PSYSTEM_CALL_RENAME;

/*++

Structure Description:

    This structure defines the system call parameters for mounting or
    unmounting a file, directory, volume, pipe, socket or device.

Members:

    MountPointPath - Stores a pointer to the string containing the path to the
        mount point where the target is to be mounted, or from which it
        should be unmounted.

    MountPointPathSize - Stores the size of the mount point path string in
        bytes, including the null terminator.

    TargetPath - Stores a pointer to the string containing the path to the
        target file, directory, volume, pipe, socket, or device that is to be
        mounted.

    TargetPathSize - Stores the size of the target path string in bytes,
        including the null terminator.

    Flags - Stores a bitfield of flags. See SYS_MOUNT_FLAG_* definitions.

--*/

typedef struct _SYSTEM_CALL_MOUNT_UNMOUNT {
    PSTR MountPointPath;
    ULONG MountPointPathSize;
    PSTR TargetPath;
    ULONG TargetPathSize;
    ULONG Flags;
} SYSCALL_STRUCT SYSTEM_CALL_MOUNT_UNMOUNT, *PSYSTEM_CALL_MOUNT_UNMOUNT;

/*++

Structure Description:

    This structure defines the system call parameters for retrieving the
    current time counter value.

Members:

    Value - Stores the time counter value returned by the kernel.

--*/

typedef struct _SYSTEM_CALL_QUERY_TIME_COUNTER {
    ULONGLONG Value;
} SYSCALL_STRUCT SYSTEM_CALL_QUERY_TIME_COUNTER,
    *PSYSTEM_CALL_QUERY_TIME_COUNTER;

/*++

Structure Description:

    This structure stores information about a timer.

Members:

    DueTime - Stores the next absolute due time of the timer, in time
        counter ticks.

    Period - Stores the period of the timer, in time counter ticks. A value of
        0 indicates a one-shot timer.

    OverflowCount - Stores the number of additional timer overflows that have
        occurred since the timer originally expired.

--*/

typedef struct _TIMER_INFORMATION {
    ULONGLONG DueTime;
    ULONGLONG Period;
    ULONG OverflowCount;
} TIMER_INFORMATION, *PTIMER_INFORMATION;

/*++

Structure Description:

    This structure defines the system call parameters for the timer control
    operations.

Members:

    Operation - Stores the operation to perform.

    Flags - Stores a bitmask of time control flags. See TIMER_CONTROL_FLAG_*
        for definitions.

    TimerNumber - Stores either the timer number to operate on, or returns
        the new timer number for create operations.

    SignalNumber - Stores the number of the signal to raise when this timer
        expires. This is only used for create timer requests.

    SignalValue - Stores the signal value to send along with the raised signal
        when the timer expires. This is only used for create timer requests.

    ThreadId - Stores an optional ID of the thread to signal when the timer
        expires. This is only used for create timer requests.

    TimerInformation - Stores the timer information, either presented to the
        kernel or returned by the kernel.

--*/

typedef struct _SYSTEM_CALL_TIMER_CONTROL {
    TIMER_OPERATION Operation;
    ULONG Flags;
    LONG TimerNumber;
    ULONG SignalNumber;
    UINTN SignalValue;
    THREAD_ID ThreadId;
    TIMER_INFORMATION TimerInformation;
} SYSCALL_STRUCT SYSTEM_CALL_TIMER_CONTROL, *PSYSTEM_CALL_TIMER_CONTROL;

/*++

Structure Description:

    This structure defines the system call parameters for getting the
    effective access permissions on a file.

Members:

    Directory - Stores an optional handle to the directory to start path
        traversal from if the specified path is relative. Supply INVALID_HANDLE
        here to use the current directory for relative paths.

    FilePath - Stores a pointer to the string containing the file path.

    FilePathSize - Stores the size of the file path buffer in bytes including
        the null terminator.

    UseRealIds - Stores a boolean indicating that the real user and group IDs
        should be used for the access check instead of the effective user and
        group IDs.

    DesiredFlags - Stores the bitfield of flags the caller would like the
        kernel to check on.

    EffectiveAccess - Stores the set of flags returned by the kernel describing
        the access the user has to the file. Only the desired flags will be
        checked, all others will be zero.

--*/

typedef struct _SYSTEM_CALL_GET_EFFECTIVE_ACCESS {
    HANDLE Directory;
    PSTR FilePath;
    ULONG FilePathSize;
    BOOL UseRealIds;
    ULONG DesiredFlags;
    ULONG EffectiveAccess;
} SYSCALL_STRUCT SYSTEM_CALL_GET_EFFECTIVE_ACCESS,
    *PSYSTEM_CALL_GET_EFFECTIVE_ACCESS;

/*++

Structure Description:

    This structure defines the system call parameters for delaying execution
    for a specified amount of time.

Members:

    TimeTicks - Stores a boolean indicating if the interval parameter is
        represented in time counter ticks (TRUE) or microseconds (FALSE).

    Interval - Stores the interval to wait. If the time ticks parameter is
        TRUE, this parameter represents an absolute time in time counter ticks.
        If the time ticks parameter is FALSE, this parameter represents a
        relative time from now in microseconds.

--*/

typedef struct _SYSTEM_CALL_DELAY_EXECUTION {
    BOOL TimeTicks;
    ULONGLONG Interval;
} SYSCALL_STRUCT SYSTEM_CALL_DELAY_EXECUTION, *PSYSTEM_CALL_DELAY_EXECUTION;

/*++

Structure Description:

    This structure defines the system call parameters for a user I/O control
    operation.

Members:

    Handle - Stores the open file handle to perform the request on.

    RequestCode - Stores the request code to send to the object. For devices,
        this is the IRP minor code.

    Context - Stores an optional context pointer.

    ContextSize - Stores the size of the supplied context buffer in bytes. Set
        this to zero if the context is not supplied or not a pointer.

--*/

typedef struct _SYSTEM_CALL_USER_CONTROL {
    HANDLE Handle;
    ULONG RequestCode;
    PVOID Context;
    UINTN ContextSize;
} SYSCALL_STRUCT SYSTEM_CALL_USER_CONTROL, *PSYSTEM_CALL_USER_CONTROL;

/*++

Structure Description:

    This structure defines the system call parameters for a flush operation.

Members:

    Handle - Stores the open file handle to perform the flush request on.

    Flags - Stores flags related to the flush operation. See SYS_FLUSH_FLAG_*
        for definitions.

--*/

typedef struct _SYSTEM_CALL_FLUSH {
    HANDLE Handle;
    ULONG Flags;
} SYSCALL_STRUCT SYSTEM_CALL_FLUSH, *PSYSTEM_CALL_FLUSH;

/*++

Structure Description:

    This structure defines the system call parameters for getting resource
    usage for a process or thread.

Members:

    Request - Stores the request type.

    Id - Stores the process or thread ID to get. Supply -1 to use the current
        process or thread.

    Usage - Stores the returned resource usage from the kernel.

    Frequency - Stores the frequency of the processor(s).

--*/

typedef struct _SYSTEM_CALL_GET_RESOURCE_USAGE {
    RESOURCE_USAGE_REQUEST Request;
    PROCESS_ID Id;
    RESOURCE_USAGE Usage;
    ULONGLONG Frequency;
} SYSCALL_STRUCT SYSTEM_CALL_GET_RESOURCE_USAGE,
    *PSYSTEM_CALL_GET_RESOURCE_USAGE;

/*++

Structure Description:

    This structure defines the system call parameters for loading a kernel
    driver.

Members:

    DriverName - Stores a pointer to a null terminated string containing the
        name of the driver to load.

    DriverNameSize - Stores the size of the driver name buffer in bytes
        including the null terminator.

--*/

typedef struct _SYSTEM_CALL_LOAD_DRIVER {
    PSTR DriverName;
    ULONG DriverNameSize;
} SYSCALL_STRUCT SYSTEM_CALL_LOAD_DRIVER, *PSYSTEM_CALL_LOAD_DRIVER;

/*++

Structure Description:

    This structure defines the system call parameters for flushing a region of
    memory after its instruction contents have been modified.

Members:

    Address - Stores the starting address of the region that was modified.

    Size - Stores the size of the region that was modified in bytes.

--*/

typedef struct _SYSTEM_CALL_FLUSH_CACHE {
    PVOID Address;
    UINTN Size;
} SYSCALL_STRUCT SYSTEM_CALL_FLUSH_CACHE, *PSYSTEM_CALL_FLUSH_CACHE;

/*++

Structure Description:

    This structure defines the system call parameters for getting the current
    directory.

Members:

    Root - Stores a boolean indicating whether to get the path to the current
        working directory (FALSE) or to get the path of the current chroot
        environment (TRUE). If the caller does not have permission to escape
        a changed root, or the root has not been changed, then / is returned
        here.

    Buffer - Stores a pointer to the buffer to hold the current directory's
        path.

    BufferSize - Stores the size of the buffer on input. On output, stores the
        required size of the buffer.

--*/

typedef struct _SYSTEM_CALL_GET_CURRENT_DIRECTORY {
    BOOL Root;
    PSTR Buffer;
    UINTN BufferSize;
} SYSCALL_STRUCT SYSTEM_CALL_GET_CURRENT_DIRECTORY,
    *PSYSTEM_CALL_GET_CURRENT_DIRECTORY;

/*++

Structure Description:

    This structure defines the system call parameters for the get/set socket
    information call.

Members:

    Socket - Stores the handle to the socket to query.

    InformationType - Stores the socket information type category to which the
        specified option belongs.

    Option - Stores the option to get or set, which is specific to the
        information type. The type of this value is generally
        SOCKET_<information_type>_OPTION.

    Data - Stores a pointer to the data buffer where the information will
        be read or written to.

    DataSize - Stores a value that on input contains the size of the data
        buffer in bytes. On output, returns the actual size of the data.

    Set - Stores a boolean indicating whether to set socket information
        (TRUE) or get device information (FALSE).

--*/

typedef struct _SYSTEM_CALL_SOCKET_GET_SET_INFORMATION {
    HANDLE Socket;
    SOCKET_INFORMATION_TYPE InformationType;
    UINTN Option;
    PVOID Data;
    UINTN DataSize;
    BOOL Set;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_GET_SET_INFORMATION,
    *PSYSTEM_CALL_SOCKET_GET_SET_INFORMATION;

/*++

Structure Description:

    This structure defines the system call parameters for partially shutting
    down I/O on a socket.

Members:

    Socket - Stores the handle to the socket to shut down.

    ShutdownType - Stores the type of shutdown to perform. See
        SOCKET_SHUTDOWN_* flags, which can be ORed together.

--*/

typedef struct _SYSTEM_CALL_SOCKET_SHUTDOWN {
    HANDLE Socket;
    ULONG ShutdownType;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_SHUTDOWN, *PSYSTEM_CALL_SOCKET_SHUTDOWN;

/*++

Structure Description:

    This structure defines the system call parameters for creating a hard link.

Members:

    ExistingFileDirectory - Stores an optional handle to the directory to
        start path traversal from if the specified existing file path is
        relative. Supply INVALID_HANDLE here to use the current directory for
        relative paths.

    ExistingFilePath - Stores a pointer to a null-terminated string
        containing the path of the existing file to link to.

    ExistingFilePathSize - Stores the size of the existing file path buffer
        in bytes, including the null terminator.

    NewLinkDirectory - Stores an optional handle to the directory to start path
        traversal from if the specified new link path is relative. Supply
        INVALID_HANDLE here to use the current directory for relative paths.

    NewLinkPath - Stores a pointer to the null-terminated string containing
        the path of the new link to create.

    NewLinkPathSize - Stores the size of the new link path buffer in bytes,
        including the null terminator.

    FollowLinks - Stores a boolean indicating whether to follow the link in
        the source (existing file path) if it is a symbolic link.

--*/

typedef struct _SYSTEM_CALL_CREATE_HARD_LINK {
    HANDLE ExistingFileDirectory;
    PSTR ExistingFilePath;
    ULONG ExistingFilePathSize;
    HANDLE NewLinkDirectory;
    PSTR NewLinkPath;
    ULONG NewLinkPathSize;
    BOOL FollowLinks;
} SYSCALL_STRUCT SYSTEM_CALL_CREATE_HARD_LINK, *PSYSTEM_CALL_CREATE_HARD_LINK;

/*++

Structure Description:

    This structure defines the system call paramters for mapping a file object
    and then unmapping the object.

Members:

    Map - Stores a boolean indicating whether or not a map (TRUE) or unmap
        (FALSE) operation is being requested.

    Flags - Stores a bitmask of flags. See SYS_MAP_FLAG_* for definitions.

    Handle - Stores a handle to the file object to be mapped.

    Address - Stores the address pointer. For a map operation, this can contain
        a suggested address for the mapping. On exit it contains the location
        of the mapping. For unmap operations, this contains the starting
        address of the region to be unmapped.

    Offset - Stores the offset, in bytes, of the file object where the
        requested mapping should start.

    Size - Stores the size of the memory region, in bytes. For a map operation
        this stores the requested number of bytes of the file to be mapped,
        starting at the given offset. For an unmap operation, this stores the
        size of the memory region that starts at the given address.

--*/

typedef struct _SYSTEM_CALL_MAP_UNMAP_MEMORY {
    BOOL Map;
    ULONG Flags;
    HANDLE Handle;
    PVOID Address;
    ULONGLONG Offset;
    UINTN Size;
} SYSCALL_STRUCT SYSTEM_CALL_MAP_UNMAP_MEMORY, *PSYSTEM_CALL_MAP_UNMAP_MEMORY;

/*++

Structure Description:

    This structure defines the system call parameters for flushing a region of
    memory to the permament storage that backs it, if any.

Members:

    Address - Stores the starting address of the memory region that is to be
        synchronized.

    Size - Supplies the size, in bytes, of the region to synchronize.

    Flags - Stores a bitmask of flags. See SYS_MAP_SYNC_FLAG_* for definitions.

--*/

typedef struct _SYSTEM_CALL_FLUSH_MEMORY {
    PVOID Address;
    ULONGLONG Size;
    ULONG Flags;
} SYSCALL_STRUCT SYSTEM_CALL_FLUSH_MEMORY, *PSYSTEM_CALL_FLUSH_MEMORY;

/*++

Structure Description:

    This structure defines the system call parameters for locating a
    device information registration.

Members:

    ByDeviceId - Stores a boolean indicating if the search should filter by
        device ID number.

    ByUuid - Stores a boolean indicating if the search should filter by
        the information type identifier.

    DeviceId - Stores the device ID that device information results must
        match against. This is only used if the "by device ID" parameter is
        TRUE.

    Uuid - Stores the information type identifier that the device information
        results must match against. This is only used if the "by UUID"
        parameter is TRUE.

    Results - Stores a pointer to a caller allocated buffer where the results
        will be returned on success.

    ResultCount - Stores a value that on input contains the size of the
        result buffer in elements. On output, returns the number of elements
        in the results, even if the supplied buffer was too small.

--*/

typedef struct _SYSTEM_CALL_LOCATE_DEVICE_INFORMATION {
    BOOL ByDeviceId;
    BOOL ByUuid;
    DEVICE_ID DeviceId;
    UUID Uuid;
    PDEVICE_INFORMATION_RESULT Results;
    ULONG ResultCount;
} SYSCALL_STRUCT SYSTEM_CALL_LOCATE_DEVICE_INFORMATION,
    *PSYSTEM_CALL_LOCATE_DEVICE_INFORMATION;

/*++

Structure Description:

    This structure defines the system call parameters for getting or setting
    device information.

Members:

    DeviceId - Stores the numerical identifier of the device to get or set
        information for.

    Uuid - Stores the device information type identifier.

    Data - Stores a pointer to the data buffer where the information will
        be read or written to.

    DataSize - Stores a value that on input contains the size of the data
        buffer in bytes. On output, returns the actual size of the data.

    Set - Stores a boolean indicating whether to set device information
        (TRUE) or get device information (FALSE).

--*/

typedef struct _SYSTEM_CALL_GET_SET_DEVICE_INFORMATION {
    DEVICE_ID DeviceId;
    UUID Uuid;
    PVOID Data;
    UINTN DataSize;
    BOOL Set;
} SYSCALL_STRUCT SYSTEM_CALL_GET_SET_DEVICE_INFORMATION,
    *PSYSTEM_CALL_GET_SET_DEVICE_INFORMATION;

/*++

Structure Description:

    This structure defines the system call parameters for the open device call.

Members:

    DeviceId - Stores the numerical identifier of the device to open.

    Flags - Stores a bitfield of flags. See SYS_OPEN_FLAG_* definitions.

    Handle - Stores a handle where the handle will be returned on success.

--*/

typedef struct _SYSTEM_CALL_OPEN_DEVICE {
    DEVICE_ID DeviceId;
    ULONG Flags;
    HANDLE Handle;
} SYSCALL_STRUCT SYSTEM_CALL_OPEN_DEVICE, *PSYSTEM_CALL_OPEN_DEVICE;

/*++

Structure Description:

    This structure defines the system call parameters for getting or setting
    system information.

Members:

    Subsystem - Stores the subsystem to query or set information for.

    InformationType - Stores the information type, which is specific to
        the subsystem. The type of this value is generally
        <subsystem>_INFORMATION_TYPE (eg IO_INFORMATION_TYPE).

    Data - Stores a pointer to the data buffer where the information will
        be read or written to.

    DataSize - Stores a value that on input contains the size of the data
        buffer in bytes. On output, returns the actual size of the data.

    Set - Stores a boolean indicating whether to set device information
        (TRUE) or get device information (FALSE).

--*/

typedef struct _SYSTEM_CALL_GET_SET_SYSTEM_INFORMATION {
    SYSTEM_INFORMATION_SUBSYSTEM Subsystem;
    UINTN InformationType;
    PVOID Data;
    UINTN DataSize;
    BOOL Set;
} SYSCALL_STRUCT SYSTEM_CALL_GET_SET_SYSTEM_INFORMATION,
    *PSYSTEM_CALL_GET_SET_SYSTEM_INFORMATION;

/*++

Structure Description:

    This structure defines the system call parameters for setting the system
    time.

Members:

    SystemTime - Stores the system time to set.

    TimeCounter - Stores the time counter value corresponding with the
        moment the system time was meant to be set by the caller.

--*/

typedef struct _SYSTEM_CALL_SET_SYSTEM_TIME {
    SYSTEM_TIME SystemTime;
    ULONGLONG TimeCounter;
} SYSCALL_STRUCT SYSTEM_CALL_SET_SYSTEM_TIME, *PSYSTEM_CALL_SET_SYSTEM_TIME;

/*++

Structure Description:

    This structure defines the system call parameters for setting the system
    time.

Members:

    Address - Stores the starting address (inclusive) to change the memory
        protection for. This must be aligned to a page boundary.

    Length - Stores the length, in bytes, of the region to change attributes
        for.

    NewAttributes - Stores the new attributes to set. See SYS_MAP_FLAG_*
        definitions.

--*/

typedef struct _SYSTEM_CALL_SET_MEMORY_PROTECTION {
    PVOID Address;
    UINTN Size;
    ULONG NewAttributes;
} SYSCALL_STRUCT SYSTEM_CALL_SET_MEMORY_PROTECTION,
    *PSYSTEM_CALL_SET_MEMORY_PROTECTION;

/*++

Structure Description:

    This structure defines the system call parameters for getting and setting
    the current thread identity, including the thread's user and group IDs.

Members:

    Request - Stores the request details of the get or set thread identity
        operation.

--*/

typedef struct _SYSTEM_CALL_SET_THREAD_IDENTITY {
    SET_THREAD_IDENTITY Request;
} SYSCALL_STRUCT SYSTEM_CALL_SET_THREAD_IDENTITY,
    *PSYSTEM_CALL_SET_THREAD_IDENTITY;

/*++

Structure Description:

    This structure defines the system call parameters for getting and setting
    the current thread permission masks.

Members:

    Request - Stores the request details of the get or set thread permissions
        operation.

--*/

typedef struct _SYSTEM_CALL_SET_THREAD_PERMISSIONS {
    SET_THREAD_PERMISSIONS Request;
} SYSCALL_STRUCT SYSTEM_CALL_SET_THREAD_PERMISSIONS,
    *PSYSTEM_CALL_SET_THREAD_PERMISSIONS;

/*++

Structure Description:

    This structure defines the system call parameters for getting and setting
    the supplementary group membership of the calling thread.

Members:

    Set - Stores a boolean indicating if the caller wants to set the
        supplementary group membership or just get it.

    Groups - Stores a pointer to an array where the supplementary group IDs
        will be returned on read, or the buffer containing the new IDs on
        writes. This buffer does not contain the primary real/effective/saved
        group IDs.

    Count - Stores the number of elements the buffer stores on input, on output,
        returns the number of elements in the buffer (or needed if the buffer
        was not large enough).

--*/

typedef struct _SYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS {
    BOOL Set;
    PGROUP_ID Groups;
    UINTN Count;
} SYSCALL_STRUCT SYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS,
    *PSYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS;

/*++

Structure Description:

    This structure defines the system call parameters for creating a pair of
    connected sockets.

Members:

    Domain - Stores the network domain to use on the sockets.

    Type - Stores the socket connection type.

    Protocol - Stores the raw network protocol to use on the socket.
        These are network specific. For example, for IPv4 and IPv6, the values
        are taken from the Internet Assigned Numbers Authority (IANA).

    OpenFlags - Stores an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

    Socket1 - Stores one of the returned socket file descriptor on success.

    Socket2 - Stores the other returned socket file descriptor on success.

--*/

typedef struct _SYSTEM_CALL_SOCKET_CREATE_PAIR {
    NET_DOMAIN_TYPE Domain;
    NET_SOCKET_TYPE Type;
    ULONG Protocol;
    ULONG OpenFlags;
    HANDLE Socket1;
    HANDLE Socket2;
} SYSCALL_STRUCT SYSTEM_CALL_SOCKET_CREATE_PAIR,
    *PSYSTEM_CALL_SOCKET_CREATE_PAIR;

/*++

Structure Description:

    This structure defines the system call parameters for creating a new
    pseudo-terminal device.

Members:

    MasterDirectory - Stores an optional handle to a directory for relative
        paths when creating the master. Supply INVALID_HANDLE to use the
        current working directory.

    SlaveDirectory - Stores an optional handle to a directory for relative
        paths when creating the slave. Supply INVALID_HANDLE to use the
        current working directory.

    MasterPath - Stores an optional pointer to the path to create for the
        master.

    MasterPathLength - Stores the length of the master side path buffer in
        bytes, including the null terminator.

    SlavePath - Stores an optional pointer to the path to create for the
        master.

    SlavePathLength - Stores the length of the slave side path buffer in
        bytes, including the null terminator.

    MasterOpenFlags - Stores the open flags to use when opening the master.
        Only read, write, and "no controlling terminal" flags are honored.

    MasterCreatePermissions - Stores the permissions to apply to the created
        master side.

    SlaveCreatePermissions - Stores the permission to apply to the created
        slave side.

    MasterHandle - Stores the returned handle to the master terminal side on
        success.

--*/

typedef struct _SYSTEM_CALL_CREATE_TERMINAL {
    HANDLE MasterDirectory;
    HANDLE SlaveDirectory;
    PSTR MasterPath;
    UINTN MasterPathLength;
    PSTR SlavePath;
    UINTN SlavePathLength;
    ULONG MasterOpenFlags;
    FILE_PERMISSIONS MasterCreatePermissions;
    FILE_PERMISSIONS SlaveCreatePermissions;
    HANDLE MasterHandle;
} SYSCALL_STRUCT SYSTEM_CALL_CREATE_TERMINAL, *PSYSTEM_CALL_CREATE_TERMINAL;

/*++

Structure Description:

    This structure defines the system call parameters for the user lock
    operation, which provides basic synchronization building blocks to user
    mode.

Members:

    Address - Stores a pointer to the address of the lock.

    Value - Stores the value, whose meaning depends on the lock operation.

    Operation - Stores the type of operation to perform on the lock. This is of
        type USER_LOCK_OPERATION, but is also combined with USER_LOCK_* flags.

    TimeoutInMilliseconds - Stores the timeout in milliseconds the caller
        should wait. Set to SYS_WAIT_TIME_INDEFINITE to wait forever.

--*/

typedef struct _SYSTEM_CALL_USER_LOCK {
    PULONG Address;
    ULONG Value;
    ULONG Operation;
    ULONG TimeoutInMilliseconds;
} SYSCALL_STRUCT SYSTEM_CALL_USER_LOCK, *PSYSTEM_CALL_USER_LOCK;

/*++

Structure Description:

    This structure defines the system call parameters for the set umask system
    call. The umask is the set of permission bits that cannot be set on
    newly created files or directories.

Members:

    Mask - Supplies the new mask to set on input. On output, contains the old
        mask.

--*/

typedef struct _SYSTEM_CALL_SET_UMASK {
    FILE_PERMISSIONS Mask;
} SYSCALL_STRUCT SYSTEM_CALL_SET_UMASK, *PSYSTEM_CALL_SET_UMASK;

/*++

Structure Description:

    This structure defines the system call parameters for duplicating a handle.

Members:

    OldHandle - Stores the handle to be duplicated.

    NewHandle - Stores the destination handle value for the new handle. If this
        is INVALID_HANDLE, then the duplicated handle will be the lowest
        available handle value, and will be returned here. If this is not
        INVALID_HANDLE, then the previous handle at that location will be
        closed. If the new handle equals the existing handle, failure is
        returned.

    OpenFlags - Stores open flags to be set on the new handle. Only
        SYS_OPEN_FLAG_CLOSE_ON_EXECUTE is permitted. If not set, the new handle
        will have the close on execute flag cleared.

--*/

typedef struct _SYSTEM_CALL_DUPLICATE_HANDLE {
    HANDLE OldHandle;
    HANDLE NewHandle;
    ULONG OpenFlags;
} SYSCALL_STRUCT SYSTEM_CALL_DUPLICATE_HANDLE, *PSYSTEM_CALL_DUPLICATE_HANDLE;

/*++

Structure Description:

    This structure defines the system call parameters for getting or setting an
    interval timer.

Members:

    Type - Stores the type of timer to get or set.

    Set - Stores a boolean indicating whether to get the current timer
        expiration information (FALSE) or set it (TRUE).

    DueTime - Stores the relative due time to set for a set operation. For both
        get and set operations, returns the previous due time. Zero means
        disabled. The units here are processor counter ticks for profile and
        virtual timers, or time counter ticks for real timers.

    Period - Stores the periodic interval to set for a set operation. For both
        get and set operations, returns the previous interval period. Zero
        means non-periodic. Units here are processor counter ticks for profile
        and virtual timers, or time counter ticks for real timers.

--*/

typedef struct _SYSTEM_CALL_SET_ITIMER {
    ITIMER_TYPE Type;
    BOOL Set;
    ULONGLONG DueTime;
    ULONGLONG Period;
} SYSCALL_STRUCT SYSTEM_CALL_SET_ITIMER, *PSYSTEM_CALL_SET_ITIMER;

/*++

Structure Description:

    This structure defines the system call parameters for getting or setting
    the program break address. Increasing the program break dynamically gives
    the application more memory, usually used for the heap.

Members:

    Break - Stores the new break address to set. If this is greater than the
        original program, then the kernel will attempt to set the given
        break address, subject to memory limitations. If this is NULL or less
        than the original break, the kernel will not attempt a set. Returns
        the current program break.

--*/

typedef struct _SYSTEM_CALL_SET_BREAK {
    PVOID Break;
} SYSCALL_STRUCT SYSTEM_CALL_SET_BREAK, *PSYSTEM_CALL_SET_BREAK;

/*++

Structure Description:

    This structure defines the system call parameters for getting or setting
    the current thread's resource limits.

Members:

    Type - Stores the type of resource limit to get or set.

    Set - Stores a boolean indicating whether to get the resource limit (FALSE)
        or set it (TRUE).

    Value - Stores the new value to set for set operations on input. Returns
        the previous value that was set for the limit.

    Status - Stores the resulting status code returned by the kernel.

--*/

typedef struct _SYSTEM_CALL_SET_RESOURCE_LIMIT {
    RESOURCE_LIMIT_TYPE Type;
    BOOL Set;
    RESOURCE_LIMIT Value;
} SYSCALL_STRUCT SYSTEM_CALL_SET_RESOURCE_LIMIT,
    *PSYSTEM_CALL_SET_RESOURCE_LIMIT;

/*++

Structure Description:

    This structure defines a union of all possible system call parameter
    structures. The size of this structure acts as an upper bound for the
    required space neede to make a stack local copy of the user mode parameters.

Members:

    Stores every possible system call parameter structure.

--*/

typedef union _SYSTEM_CALL_PARAMETER_UNION {
    SYSTEM_CALL_EXIT_THREAD ExitThread;
    SYSTEM_CALL_OPEN Open;
    SYSTEM_CALL_PERFORM_IO PerformIo;
    SYSTEM_CALL_PERFORM_VECTORED_IO PerformVectoredIo;
    SYSTEM_CALL_CREATE_PIPE CreatePipe;
    SYSTEM_CALL_CREATE_THREAD CreateThread;
    SYSTEM_CALL_FORK Fork;
    SYSTEM_CALL_EXECUTE_IMAGE ExecuteImage;
    SYSTEM_CALL_CHANGE_DIRECTORY ChangeDirectory;
    SYSTEM_CALL_SET_SIGNAL_HANDLER SetSignalHandler;
    SYSTEM_CALL_SEND_SIGNAL SendSignal;
    SYSTEM_CALL_GET_SET_PROCESS_ID GetSetProcessId;
    SYSTEM_CALL_SET_SIGNAL_BEHAVIOR SetSignalBehavior;
    SYSTEM_CALL_WAIT_FOR_CHILD WaitForChild;
    SYSTEM_CALL_SUSPEND_EXECUTION SuspendExecution;
    SYSTEM_CALL_POLL Poll;
    SYSTEM_CALL_SOCKET_CREATE SocketCreate;
    SYSTEM_CALL_SOCKET_BIND SocketBind;
    SYSTEM_CALL_SOCKET_LISTEN SocketListen;
    SYSTEM_CALL_SOCKET_ACCEPT SocketAccept;
    SYSTEM_CALL_SOCKET_CONNECT SocketConnect;
    SYSTEM_CALL_SOCKET_PERFORM_IO SocketPerformIo;
    SYSTEM_CALL_FILE_CONTROL FileControl;
    SYSTEM_CALL_GET_SET_FILE_INFORMATION GetSetFileInformation;
    SYSTEM_CALL_DEBUG Debug;
    SYSTEM_CALL_SEEK Seek;
    SYSTEM_CALL_CREATE_SYMBOLIC_LINK CreateSymbolicLink;
    SYSTEM_CALL_READ_SYMBOLIC_LINK ReadSymbolicLink;
    SYSTEM_CALL_DELETE Delete;
    SYSTEM_CALL_RENAME Rename;
    SYSTEM_CALL_MOUNT_UNMOUNT MountUnmount;
    SYSTEM_CALL_QUERY_TIME_COUNTER QueryTimeCounter;
    SYSTEM_CALL_TIMER_CONTROL TimerControl;
    SYSTEM_CALL_GET_EFFECTIVE_ACCESS GetEffectiveAccess;
    SYSTEM_CALL_DELAY_EXECUTION DelayExecution;
    SYSTEM_CALL_USER_CONTROL UserControl;
    SYSTEM_CALL_FLUSH Flush;
    SYSTEM_CALL_GET_RESOURCE_USAGE GetResourceUsage;
    SYSTEM_CALL_LOAD_DRIVER LoadDriver;
    SYSTEM_CALL_FLUSH_CACHE FlushCache;
    SYSTEM_CALL_GET_CURRENT_DIRECTORY GetCurrentDirectory;
    SYSTEM_CALL_SOCKET_GET_SET_INFORMATION GetSetSocketInformation;
    SYSTEM_CALL_SOCKET_SHUTDOWN SocketShutdown;
    SYSTEM_CALL_CREATE_HARD_LINK CreateHardLink;
    SYSTEM_CALL_MAP_UNMAP_MEMORY MapUnmapMemory;
    SYSTEM_CALL_FLUSH_MEMORY FlushMemory;
    SYSTEM_CALL_LOCATE_DEVICE_INFORMATION LocateDeviceInformation;
    SYSTEM_CALL_GET_SET_DEVICE_INFORMATION GetSetDeviceInformation;
    SYSTEM_CALL_OPEN_DEVICE OpenDevice;
    SYSTEM_CALL_GET_SET_SYSTEM_INFORMATION GetSetSystemInformation;
    SYSTEM_CALL_SET_SYSTEM_TIME SetSystemTime;
    SYSTEM_CALL_SET_MEMORY_PROTECTION SetMemoryProtection;
    SYSTEM_CALL_SET_THREAD_IDENTITY SetThreadIdentity;
    SYSTEM_CALL_SET_THREAD_PERMISSIONS SetThreadPermissions;
    SYSTEM_CALL_SET_SUPPLEMENTARY_GROUPS SetSupplementaryGroups;
    SYSTEM_CALL_SOCKET_CREATE_PAIR SocketCreatePair;
    SYSTEM_CALL_CREATE_TERMINAL CreateTerminal;
    SYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO SocketPerformVectoredIo;
    SYSTEM_CALL_USER_LOCK UserLock;
    SYSTEM_CALL_SET_UMASK SetUmask;
    SYSTEM_CALL_DUPLICATE_HANDLE DuplicateHandle;
    SYSTEM_CALL_SET_ITIMER SetITimer;
    SYSTEM_CALL_SET_RESOURCE_LIMIT SetResourceLimit;
    SYSTEM_CALL_SET_BREAK SetBreak;
} SYSCALL_STRUCT SYSTEM_CALL_PARAMETER_UNION, *PSYSTEM_CALL_PARAMETER_UNION;

typedef
INTN
(*PSYSTEM_CALL_ROUTINE) (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the kernel mode functionality behind a particular
    system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
