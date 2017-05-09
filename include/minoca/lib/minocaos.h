/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    minocaos.h

Abstract:

    This header contains definitions for the Operating System Base library.

Author:

    Evan Green 25-Feb-2013

--*/

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>

//
// --------------------------------------------------------------------- Macros
//

//
// Use this macro to initialize a spin lock with default values.
//

#define OsInitializeLockDefault(_SpinLock)              \
    OsInitializeLock((_SpinLock), OS_LOCK_DEFAULT_SPIN_COUNT)

//
// ---------------------------------------------------------------- Definitions
//

#ifndef OS_API

#define OS_API __DLLIMPORT

#endif

//
// Define the allocation tag used by OS thread routines: OsTh
//

#define OS_THREAD_ALLOCATION_TAG 0x6854734F

//
// Define the default system value for number of spins before spin locks start
// sleeping.
//

#define OS_LOCK_DEFAULT_SPIN_COUNT 500

//
// Set this flag when initializing a read-write lock to indicate the lock
// should be shared across processes.
//

#define OS_RWLOCK_SHARED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _OS_ARM_PROCESSOR_FEATURE {
    OsArmFeatureInvalid,
    OsArmArmv7,
    OsArmVfp,
    OsArmVfp3,
    OsArmNeon32,
    OsArmFeatureCount
} OS_ARM_PROCESSOR_FEATURE, *POS_ARM_PROCESSOR_FEATURE;

typedef enum _OS_X86_PROCESSOR_FEATURE {
    OsX86FeatureInvalid,
    OsX86Sysenter,
    OsX86I686,
    OsX86FxSave,
    OsX86FeatureCount
} OS_X86_PROCESSOR_FEATURE, *POS_X86_PROCESSOR_FEATURE;

typedef
BOOL
(*PSIGNAL_HANDLER_ROUTINE) (
    PSIGNAL_PARAMETERS SignalInformation,
    PSIGNAL_CONTEXT Context
    );

/*++

Routine Description:

    This routine is called whenever a signal occurs for the current process or
    thread.

Arguments:

    SignalInformation - Supplies a pointer to the signal information. This
        pointer may be stack allocated, and should not be referenced once the
        handler has returned.

    Context - Supplies a pointer to the signal context, including the machine
        state before the signal was applied.

Return Value:

    TRUE if an interrupted function can restart.

    FALSE otherwise.

--*/

typedef
VOID
(*PIMAGE_ITERATOR_ROUTINE) (
    PLOADED_IMAGE Image,
    PVOID Context
    );

/*++

Routine Description:

    This routine is called for each loaded image in the process.

Arguments:

    Image - Supplies a pointer to the loaded image.

    Context - Supplies the context pointer that was passed into the iterate
        request function.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a basic lock type.

Members:

    Value - Stores the current lock state.

    SpinCount - Stores the number of times the lock will simply spin and yield
        trying to acquire the lock at first.

--*/

typedef struct _OS_LOCK {
    ULONG Value;
    ULONG SpinCount;
} OS_LOCK, *POS_LOCK;

/*++

Structure Description:

    This structure stores the internal structure of a read/write lock.

Members:

    State - Stores the state of the lock. 0 is unlocked, -1 is locked for
        write, and any other value contains a count of readers.

    WriterThreadId - Stores the thread ID of the thread that has this lock for
        writing, if any.

    PendingReaders - Stores the number of threads waiting to acquire the lock
        for read access.

    PendingWriters - Stores the number of threads waiting to acquire the lock
        for write access.

--*/

typedef struct _OS_RWLOCK {
    ULONG State;
    UINTN WriterThreadId;
    ULONG PendingReaders;
    ULONG PendingWriters;
    ULONG Attributes;
} OS_RWLOCK, *POS_RWLOCK;

/*++

Structure Description:

    This structure defines a thread local storage index entry, the format of
    which is defined by the ABI and assumed by the compiler.

Members:

    Module - Stores the module ID. Valid module IDs start at 1.

    Offset - Stores the offset from the beginning of the thread local storage
        section to the desired symbol.

--*/

typedef struct _TLS_INDEX {
    UINTN Module;
    UINTN Offset;
} TLS_INDEX, *PTLS_INDEX;

/*++

Structure Description:

    This structure defines a loaded executable image symbol.

Members:

    ImagePath - Stores the file path of the image that contains the symbol.

    ImageBase - Stores the loaded base address of the image that contains the
        symbol.

    SymbolName - Stores the name of the symbol.

    SymbolAddress - Stores the address of the symbol.

--*/

typedef struct _OS_IMAGE_SYMBOL {
    PSTR ImagePath;
    PVOID ImageBase;
    PSTR SymbolName;
    PVOID SymbolAddress;
} OS_IMAGE_SYMBOL, *POS_IMAGE_SYMBOL;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the routine used to get environment variable contents.
//

OS_API extern PIM_GET_ENVIRONMENT_VARIABLE OsImGetEnvironmentVariable;

//
// -------------------------------------------------------- Function Prototypes
//

OS_API
VOID
OsInitializeLibrary (
    PPROCESS_ENVIRONMENT Environment
    );

/*++

Routine Description:

    This routine initializes the base OS library. It needs to be called only
    once, when the library is first loaded.

Arguments:

    Environment - Supplies a pointer to the environment information.

Return Value:

    None.

--*/

OS_API
VOID
OsTestSystemCall (
    VOID
    );

/*++

Routine Description:

    This routine performs a meaningless system call.

Arguments:

    None.

Return Value:

    None.

--*/

OS_API
KSTATUS
OsOpen (
    HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Flags,
    FILE_PERMISSIONS CreatePermissions,
    PHANDLE Handle
    );

/*++

Routine Description:

    This routine opens a file or other I/O object.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to a string containing the path of the object
        to open.

    PathLength - Supplies the length of the path buffer, in bytes, including
        the null terminator.

    Flags - Supplies flags associated with the open operation. See
        SYS_OPEN_FLAG_* definitions.

    CreatePermissions - Supplies the permissions for create operations.

    Handle - Supplies a pointer where a handle will be returned on success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsOpenDevice (
    DEVICE_ID DeviceId,
    ULONG Flags,
    PHANDLE Handle
    );

/*++

Routine Description:

    This routine opens a device directly.

Arguments:

    DeviceId - Supplies the identifier of the device to open.

    Flags - Supplies flags associated with the open operation. See
        SYS_OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a handle will be returned on success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsClose (
    HANDLE Handle
    );

/*++

Routine Description:

    This routine closes an I/O handle.

Arguments:

    Handle - Supplies a pointer to the open handle.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsPerformIo (
    HANDLE Handle,
    IO_OFFSET Offset,
    UINTN Size,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PVOID Buffer,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine performs I/O on an open handle.

Arguments:

    Handle - Supplies a pointer to the opened I/O handle.

    Offset - Supplies the offset into the file to read from or write to. Set
        this to IO_OFFSET_NONE to do I/O at the current file position or for
        handles that are not seekable.

    Size - Supplies the number of bytes to transfer.

    Flags - Supplies a bitfield of flags. See SYS_IO_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        SYS_WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Buffer - Supplies a pointer to the buffer containing the data to write or
        where the read data should be returned, depending on the operation.

    BytesCompleted - Supplies a pointer where the number of bytes completed
        will be returned.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsPerformVectoredIo (
    HANDLE Handle,
    IO_OFFSET Offset,
    UINTN Size,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PIO_VECTOR VectorArray,
    UINTN VectorCount,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine performs I/O on an open handle.

Arguments:

    Handle - Supplies a pointer to the opened I/O handle.

    Offset - Supplies the offset into the file to read from or write to. Set
        this to IO_OFFSET_NONE to do I/O at the current file position or for
        handles that are not seekable.

    Size - Supplies the number of bytes to transfer.

    Flags - Supplies a bitfield of flags. See SYS_IO_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        SYS_WAIT_TIME_INDEFINITE to wait forever on the I/O.

    VectorArray - Supplies an array of I/O vector structures to do I/O to/from.

    VectorCount - Supplies the number of elements in the vector array.

    BytesCompleted - Supplies a pointer where the number of bytes completed
        will be returned.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsFlush (
    HANDLE Handle,
    ULONG Flags
    );

/*++

Routine Description:

    This routine flushes handle data to its backing device. If the flags
    specify that all data is to be flushed, then a handle is not required.

Arguments:

    Handle - Supplies an open I/O handle. This parameter is not required if
        SYS_FLUSH_FLAG_ALL is set.

    Flags - Supplies a bitfield of flags. See SYS_FLUSH_FLAG_* definitions.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsCreatePipe (
    HANDLE Directory,
    PSTR Path,
    ULONG PathLength,
    ULONG Flags,
    FILE_PERMISSIONS Permissions,
    PHANDLE ReadHandle,
    PHANDLE WriteHandle
    );

/*++

Routine Description:

    This routine creates a pipe.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies an optional pointer to the path to create the pipe at.

    PathLength - Supplies the length of the path buffer in bytes, including
        the null terminator character.

    Flags - Supplies a bitfield of flags governing the behavior of the new pipe
        descriptors. Only SYS_OPEN_FLAG_CLOSE_ON_EXECUTE and
        SYS_OPEN_FLAG_NON_BLOCKING are permitted.

    Permissions - Supplies the initial permissions to set on the pipe.

    ReadHandle - Supplies a pointer where the handle to the read end of the
        pipe will be returned on success. Handles are only returned if a
        NULL path was passed in.

    WriteHandle - Supplies a pointer where the handle to the write end of the
        pipe will be returned on success. Handles are only returned in a NULL
        path was passed in.

Return Value:

    Status code.

--*/

OS_API
VOID
OsExitThread (
    PVOID UnmapAddress,
    UINTN UnmapSize
    );

/*++

Routine Description:

    This routine terminates the current thread, and optionally attempts to
    unmap a region of memory on its way out. Usually this is the stack of the
    thread that is exiting.

Arguments:

    UnmapAddress - Supplies an optional pointer to a region of memory to unmap
        as the thread exits. Supply NULL to skip unmapping.

    UnmapSize - Supplies the size of the region to unmap in bytes. This must be
        aligned to the page size. If it is not, the unmap simply won't happen.
        Supply 0 to skip the unmap and just exit the thread. If -1 is supplied,
        this routine returns. This value can be used to warm up the PLT entry,
        since lazy binding cannot take place after the thread's control block
        has been destroyed.

Return Value:

    This routine does not return, unless the magic size is passed in.

--*/

OS_API
KSTATUS
OsCreateThread (
    PSTR ThreadName,
    ULONG ThreadNameBufferLength,
    PTHREAD_ENTRY_ROUTINE ThreadRoutine,
    PVOID Parameter,
    PVOID StackBase,
    ULONG StackSize,
    PVOID ThreadPointer,
    PTHREAD_ID ThreadId
    );

/*++

Routine Description:

    This routine creates a new thread.

Arguments:

    ThreadName - Supplies an optional pointer to the thread name.

    ThreadNameBufferLength - Supplies the size of the thread name buffer,
        including the null terminator.

    ThreadRoutine - Supplies a pointer to the funtion that should be run on the
        new thread.

    Parameter - Supplies a pointer that will be passed directly to the thread
        routine.

    StackBase - Supplies an optional pointer to the stack base address. If
        supplied, the kernel will not add a guard region or automatically
        expand the stack.

    StackSize - Supplies the size of the new thread's stack. Supply 0 to use
        the system default.

    ThreadPointer - Supplies the thread pointer to set for the new thread.

    ThreadId - Supplies an optional pointer where the ID of the new thread will
        be returned on success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsForkProcess (
    ULONG Flags,
    PPROCESS_ID NewProcessId
    );

/*++

Routine Description:

    This routine forks the current process into two separate processes. The
    child process begins executing in the middle of this function.

Arguments:

    Flags - Supplies a bitfield of flags governing the behavior of the newly
        forked process. See FORK_FLAG_* definitions.

    NewProcessId - Supplies a pointer that on success contains the process ID
        of the child process in the parent, and 0 in the child. This value
        contains -1 if the new process failed to spawn.

Return Value:

    STATUS_SUCCESS in both the parent and child on success.

    Other status codes are returned to the parent if the child failed to spawn.

--*/

OS_API
KSTATUS
OsExecuteImage (
    PPROCESS_ENVIRONMENT Environment
    );

/*++

Routine Description:

    This routine replaces the currently running process with the given binary
    image.

Arguments:

    Environment - Supplies a pointer to the environment to execute, which
        includes the image name, parameters, and environment variables.

Return Value:

    If this routine succeeds, it will not return, as the process will be
    replaced with the new executable. If the process could not be started,
    a failing status code will be returned to the caller.

--*/

OS_API
KSTATUS
OsGetSystemVersion (
    PSYSTEM_VERSION_INFORMATION VersionInformation,
    BOOL WantStrings
    );

/*++

Routine Description:

    This routine gets the system version information.

Arguments:

    VersionInformation - Supplies a pointer where the system version
        information will be returned. The caller should not attempt to modify
        or free the strings pointed to by members of this structure.

    WantStrings - Supplies a boolean indicating if the build strings are
        desired or just the major/minor version information.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetCurrentDirectory (
    BOOL Root,
    PSTR *Buffer,
    PUINTN BufferSize
    );

/*++

Routine Description:

    This routine retrieves a pointer to a null terminated string containing the
    path to the current working directory or the current root directory.

Arguments:

    Root - Supplies a boolean indicating whether caller would like the current
        working directory (FALSE) or the path to the current root directory
        (TRUE). If the caller does not have permission to escape roots, or
        does not currently have an altered root directory, then / is returned.

    Buffer - Supplies a pointer that receives a pointer to a buffer that
        contains a null terminated string for the path to the current
        directory.

    BufferSize - Supplies a pointer that receives the size of the buffer, in
        bytes.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsChangeDirectory (
    BOOL Root,
    PSTR Path,
    ULONG PathSize
    );

/*++

Routine Description:

    This routine sets the current working directory or current root directory.

Arguments:

    Root - Supplies a boolean indicating whether to change the current working
        directory (FALSE) or the current root directory (TRUE). If attempting
        to change the root, the caller must have permission to change the root,
        must be running a single thread, and must not have any other handles
        to directories open.

    Path - Supplies a pointer to the path of the new working directory. If
        trying to escape the root, supply NULL here. The caller must have
        sufficient privileges to escape a root.

    PathSize - Supplies the size of the path directory string in bytes
        including the null terminator.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsChangeDirectoryHandle (
    BOOL Root,
    HANDLE Handle
    );

/*++

Routine Description:

    This routine sets the current working directory or root directory to the
    same directory opened with the given file handle.

Arguments:

    Root - Supplies a boolean indicating whether to change the current working
        directory (FALSE) or the current root directory (TRUE). If attempting
        to change the root, the caller must have permission to change the root,
        must be running a single thread, and must not have any other handles
        to directories open.

    Handle - Supplies an open handle to a directory to change the current
        working directroy to. Supply INVALID_HANDLE here to escape the root.
        The caller must have sufficient privileges to escape a root.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsPoll (
    PSIGNAL_SET SignalMask,
    PPOLL_DESCRIPTOR Descriptors,
    ULONG DescriptorCount,
    ULONG TimeoutInMilliseconds,
    PULONG DescriptorsSelected
    );

/*++

Routine Description:

    This routine polls several I/O handles.

Arguments:

    SignalMask - Supplies an optional pointer to a mask to set for the
        duration of the wait.

    Descriptors - Supplies a pointer to an array of poll descriptor structures
        describing the descriptors and events to wait on.

    DescriptorCount - Supplies the number of descriptors in the array.

    TimeoutInMilliseconds - Supplies the number of milliseconds to wait before
        giving up.

    DescriptorsSelected - Supplies a pointer where the number of descriptors
        that had activity will be returned on success.

Return Value:

    STATUS_SUCCESS if one or more descriptors is ready for action.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
        operation.

    STATUS_INTERRUPTED if a signal was caught during the wait.

    STATUS_TIMEOUT if no descriptors were ready in the given amount of time.

    STATUS_INVALID_PARAMETER if more than MAX_LONG descriptors are supplied.

--*/

OS_API
PSIGNAL_HANDLER_ROUTINE
OsSetSignalHandler (
    PSIGNAL_HANDLER_ROUTINE NewHandler
    );

/*++

Routine Description:

    This routine sets the signal handler routine called whenever a signal is
    delivered by the kernel.

Arguments:

    NewHandler - Supplies a pointer to the new handler routine to use.

Return Value:

    Returns a pointer to the old handler, or NULL if no other signal handlers
    were registered.

--*/

OS_API
KSTATUS
OsSendSignal (
    SIGNAL_TARGET_TYPE TargetType,
    ULONG TargetId,
    ULONG SignalNumber,
    SHORT SignalCode,
    UINTN SignalParameter
    );

/*++

Routine Description:

    This routine sends a signal to a process, process group or thread.

Arguments:

    TargetType - Supplies the target type to which the signal is being sent. It
        can be either a process, process group, or thread.

    TargetId - Supplies the ID for the signal's target process, process group,
        or thread.

    SignalNumber - Supplies the signal number to send.

    SignalCode - Supplies the signal code to send. See SIGNAL_CODE_*
        definitions.

    SignalParameter - Supplies a parameter to send with the signal if the
        signal is in the real time signal range.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetProcessId (
    PROCESS_ID_TYPE ProcessIdType,
    PPROCESS_ID ProcessId
    );

/*++

Routine Description:

    This routine gets an identifier associated with the process, such as the
    process ID, thread ID, parent process ID, process group ID, and session ID.

Arguments:

    ProcessIdType - Supplies the type of ID to get.

    ProcessId - Supplies a pointer that on input contains the process ID
        argument if the operation takes a parameter. On successful output,
        returns the desired ID. Supply zero to use the calling process ID.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSetProcessId (
    PROCESS_ID_TYPE ProcessIdType,
    PROCESS_ID ProcessId,
    PROCESS_ID NewValue
    );

/*++

Routine Description:

    This routine sets an identifier associated with the process, such as the
    process group ID or session ID.

Arguments:

    ProcessIdType - Supplies the type of ID to set. Not all types can be set.

    ProcessId - Supplies the ID of the process to change. Supply 0 to use the
        current process.

    NewValue - Supplies the new value to set.

Return Value:

    Status code.

--*/

OS_API
SIGNAL_SET
OsSetSignalBehavior (
    SIGNAL_MASK_TYPE MaskType,
    SIGNAL_MASK_OPERATION Operation,
    PSIGNAL_SET NewMask
    );

/*++

Routine Description:

    This routine sets signal behavior, either for the current thread in the
    case of the blocked signals, or for the process for other signal masks.

Arguments:

    MaskType - Supplies the type of mask to change.

    Operation - Supplies the interaction between the new mask and the previous
        signal mask.

    NewMask - Supplies a pointer to the mask of signals that are affected by
        this operation.

Return Value:

    Returns the original signal mask before this function was called.

--*/

OS_API
KSTATUS
OsWaitForChildProcess (
    ULONG Flags,
    PPROCESS_ID ChildPid,
    PULONG Reason,
    PUINTN ChildExitValue,
    PRESOURCE_USAGE ChildResourceUsage
    );

/*++

Routine Description:

    This routine is called to suspend execution of the current thread until
    a child process completes.

Arguments:

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See SYSTEM_CALL_WAIT_FLAG_* definitions.

    ChildPid - Supplies a pointer that on input supplies the child process ID
        parameter. This parameter can be one of the following:

        If -1 is supplied, any child signal will be pulled off and returned.

        If a number greater than 0 is supplied, only the specific process ID
        will be pulled off and returned.

        If 0 is supplied, any child process whose process group ID is equal to
        that of the calling process will be pulled.

        If a number less than zero (but not -1) is supplied, then any process
        whose process group ID is equal to the absolute value of this parameter
        will be dequeued and returned.

        On output, this parameter will contain the process ID of the child that
        generated the signal activity, and the child signal will be discarded.
        If the wait for child parameter is set to FALSE, then this parameter
        is ignored. If a non-child signal caused the routine to return, then
        the value at this parameter is undefined.

    Reason - Supplies a pointer where the reason for the child event will be
        returned. See CHILD_SIGNAL_REASON_* definitions.

    ChildExitValue - Supplies a pointer where the child exit value (or signal
        that caused the event) will be returned.

    ChildResourceUsage - Supplies an optional pointer where the resource usage
        of the child will be returned on success.

Return Value:

    STATUS_SUCCESS if the wait was successfully satisfied.

    STATUS_NO_DATA_AVAILABLE if the SYSTEM_CALL_WAIT_FLAG_RETURN_IMMEDIATELY
    flag is set and there are no children ready to be reaped. The child PID is
    returned as -1.

    STATUS_INTERRUPTED if the wait was interrupted by a signal.

    STATUS_NO_ELIGIBLE_CHILDREN if no eligible children could be reaped.

--*/

OS_API
KSTATUS
OsSuspendExecution (
    SIGNAL_MASK_OPERATION SignalOperation,
    PSIGNAL_SET SignalSet,
    PSIGNAL_PARAMETERS SignalParameters,
    ULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine suspends execution of the current thread until a signal comes
    in. The current thread's blocked signal mask can be changed for the
    duration of the call by providing an operation and a signal set.

Arguments:

    SignalOperation - Supplies the operation to perform with the signal set for
        the duration of the call: set, clear, overwrite or none.

    SignalSet - Supplies a pointer to the signal set to apply for the duration
        of this system call as dictated by the signal operation.

    SignalParameters - Supplies an optional pointer where the signal
        information for the signal that occurred will be returned.

    TimeoutInMilliseconds - Supplies the timeout of the operation in
        milliseconds.

Return Value:

    STATUS_SUCCESS if a signal arrived.

    STATUS_INTERRUPTED on a clear signal operation if a signal that is not in
    the given set arrived.

    STATUS_TIMEOUT if no signal arrived before the given timeout expires.

    STATUS_INVALID_PARAMETER if no signal set is supplied for an operation
    other than SignalMaskOperationNone.

--*/

OS_API
NO_RETURN
VOID
OsExitProcess (
    UINTN Status
    );

/*++

Routine Description:

    This routine terminates the current process and any threads that may be
    running in it.

Arguments:

    Status - Supplies the exit status, returned to the parent in the wait
        calls. Conventionally 0 indicates success, and non-zero indicates
        failure. The C library only receives the first eight bits of the return
        status, portable applications should not set bits beyond that.

Return Value:

    This routine does not return.

--*/

OS_API
KSTATUS
OsFileControl (
    HANDLE Handle,
    FILE_CONTROL_COMMAND Command,
    PFILE_CONTROL_PARAMETERS_UNION Parameters
    );

/*++

Routine Description:

    This routine performs a file control operation on the given handle.

Arguments:

    Handle - Supplies the file handle to operate on.

    Command - Supplies the command to perform.

    Parameters - Supplies an optional pointer to any additional parameters
        needed by the command.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetFileInformation (
    HANDLE Directory,
    PSTR Path,
    ULONG PathLength,
    BOOL FollowLink,
    PFILE_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine gets the file properties for a given file.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the string containing the file path to get
        properties for.

    PathLength - Supplies the length of the path string in bytes including the
        null terminator.

    FollowLink - Supplies a boolean indicating what to do if the file path
        points to a symbolic link. If set to TRUE, the file information set or
        returned will be for the file the link points to. If FALSE, the call
        will set or get information for the link itself.

    Properties - Supplies a pointer where the file properties will be returned
        on success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSetFileInformation (
    HANDLE Directory,
    PSTR Path,
    ULONG PathLength,
    BOOL FollowLink,
    PSET_FILE_INFORMATION Request
    );

/*++

Routine Description:

    This routine sets the file properties for a given file.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the string containing the file path to set
        properties for.

    PathLength - Supplies the length of the path string in bytes including the
        null terminator.

    FollowLink - Supplies a boolean indicating what to do if the file path
        points to a symbolic link. If set to TRUE, the file information set or
        returned will be for the file the link points to. If FALSE, the call
        will set or get information for the link itself.

    Request - Supplies a pointer to the set file information request.

Return Value:

    Status code.

--*/

OS_API
VOID
OsDebugPrint (
    PSTR String,
    ULONG StringSize
    );

/*++

Routine Description:

    This routine prints a message to the debug console. No formatting is
    provided.

Arguments:

    String - Supplies a pointer to the string to print.

    StringSize - Supplies the size of the string in bytes including the null
        terminator.

Return Value:

    None.

--*/

OS_API
KSTATUS
OsDebug (
    DEBUG_COMMAND_TYPE Command,
    PROCESS_ID Process,
    PVOID Address,
    PVOID Data,
    ULONG Size,
    ULONG SignalToDeliver
    );

/*++

Routine Description:

    This routine sends a debug command to a process.

Arguments:

    Command - Supplies the command to send.

    Process - Supplies the process ID to send the command to.

    Address - Supplies the address parameter of the command, usually the
        address in the target to read from or write to.

    Data - Supplies the data parameter, usually the buffer containing the data
        to write or the buffer where the read data will be returned.

    Size - Supplies the size of the buffer, in bytes.

    SignalToDeliver - Supplies the signal number to deliver to the debugged
        process for step and continue commands. For other commands, this
        parameter is ignored.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSeek (
    HANDLE Handle,
    SEEK_COMMAND SeekCommand,
    IO_OFFSET Offset,
    PIO_OFFSET NewOffset
    );

/*++

Routine Description:

    This routine seeks to the given position in a file. This routine is only
    relevant for normal file or block based devices.

Arguments:

    Handle - Supplies the open file handle.

    SeekCommand - Supplies the reference point for the seek offset. Usual
        reference points are the beginning of the file, current file position,
        and the end of the file.

    Offset - Supplies the offset from the reference point to move in bytes.

    NewOffset - Supplies an optional pointer where the file position after the
        move will be returned on success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsCreateSymbolicLink (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    PSTR LinkDestinationBuffer,
    ULONG LinkDestinationBufferSize
    );

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the symbolic link path.

    PathSize - Supplies the size of the symbolic link pointer in bytes
        including the null terminator.

    LinkDestinationBuffer - Supplies a pointer to a string containing the link's
        target path, the location the link points to.

    LinkDestinationBufferSize - Supplies the size of the link destination
        buffer in bytes, NOT including the null terminator.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsReadSymbolicLink (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    PSTR LinkDestinationBuffer,
    ULONG LinkDestinationBufferSize,
    PULONG LinkDestinationSize
    );

/*++

Routine Description:

    This routine reads the destination path of a symbolic link.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the symbolic link path.

    PathSize - Supplies the size of the symbolic link pointer in bytes
        including the null terminator.

    LinkDestinationBuffer - Supplies a pointer to a buffer where the
        destination of the link will be returned.

    LinkDestinationBufferSize - Supplies the size of the link destination
        buffer in bytes.

    LinkDestinationSize - Supplies a pointer where the actual size of the
        link destination (including the null terminator) will be returned on
        either success or a STATUS_BUFFER_TOO_SMALL case. On failure, 0 will be
        returned here.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the link destination buffer was not large
    enough to store the link destination.

    Other errors on other failures.

--*/

OS_API
KSTATUS
OsCreateHardLink (
    HANDLE ExistingFileDirectory,
    PSTR ExistingFile,
    ULONG ExistingFileSize,
    HANDLE LinkDirectory,
    PSTR LinkPath,
    ULONG LinkPathSize,
    BOOL FollowExistingFileLinks
    );

/*++

Routine Description:

    This routine creates a hard link.

Arguments:

    ExistingFileDirectory - Supplies an optional handle to the directory to
        start path traversal from if the specified existing file path is
        relative. Supply INVALID_HANDLE here to use the current directory for
        relative paths.

    ExistingFile - Supplies a pointer to the path of the existing file to
        create the link from.

    ExistingFileSize - Supplies the size of the existing file path buffer in
        bytes, including the null terminator.

    LinkDirectory - Supplies an optional handle to the directory to start path
        traversal from if the specified new link path is relative. Supply
        INVALID_HANDLE here to use the current directory for relative paths.

    LinkPath - Supplies a pointer to a string containing the destination path
        of the new link.

    LinkPathSize - Supplies the size of the link path buffer in bytes.

    FollowExistingFileLinks - Supplies a boolean indicating that if the
        existing file path exists and is a symbolic link, the new link shall be
        for the target of that link (TRUE) rather than the link itself (FALSE).

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsDelete (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    ULONG Flags
    );

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path points to
    a file, the hard link count on the file is decremented. If the hard link
    count reaches zero and no processes have the file open, the contents of the
    file are destroyed. If processes have open handles to the file, the
    destruction of the file contents are deferred until the last handle to the
    old file is closed. If the path points to a symbolic link, the link itself
    is removed and not the destination. The removal of the entry from the
    directory is immediate.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to the path to delete.

    PathSize - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Flags - Supplies flags associated with the delete operation. See
        SYS_DELETE_FLAG_* definitions.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsRename (
    HANDLE SourceDirectory,
    PSTR SourcePath,
    ULONG SourcePathSize,
    HANDLE DestinationDirectory,
    PSTR DestinationPath,
    ULONG DestinationPathSize
    );

/*++

Routine Description:

    This routine attempts to rename the object at the given path. This routine
    operates on symbolic links themselves, not the destinations of symbolic
    links. If the source and destination paths are equal, this routine will do
    nothing and return successfully. If the source path is not a directory, the
    destination path must not be a directory. If the destination file exists,
    it will be deleted. The caller must have write access in both the old and
    new directories. If the source path is a directory, the destination path
    must not exist or be an empty directory. The destination path must not have
    a path prefix of the source (ie it's illegal to move /my/path into
    /my/path/stuff).

Arguments:

    SourceDirectory - Supplies an optional handle to the directory to start
        source path searches from. If the source path is absolute, this value
        is ignored. If this is INVALID_HANDLE, then source path searches will
        start from the current working directory.

    SourcePath - Supplies a pointer to the path of the file to rename.

    SourcePathSize - Supplies the length of the source path buffer in bytes,
        including the null terminator.

    DestinationDirectory - Supplies an optional handle to the directory to
        start destination path searches from. If the destination path is
        absolute, this value is ignored. If this is INVALID_HANDLE, then
        destination path searches will start from the current working directory.

    DestinationPath - Supplies a pointer to the path to rename the file to.

    DestinationPathSize - Supplies the size of the destination path buffer in
        bytes, including the null terminator.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsUserControl (
    HANDLE Handle,
    ULONG RequestCode,
    PVOID Context,
    UINTN ContextSize
    );

/*++

Routine Description:

    This routine sends a user I/O request to the given file/device/etc.

Arguments:

    Handle - Supplies the open file handle to send the request to.

    RequestCode - Supplies the request to send. For device handles, this is the
        minor code of the IRP.

    Context - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsMount (
    PSTR MountPointPath,
    ULONG MountPointPathSize,
    PSTR TargetPath,
    ULONG TargetPathSize,
    ULONG Flags
    );

/*++

Routine Description:

    This routine attempts to mount the given target at the given mount point.

Arguments:

    MountPointPath - Supplies a pointer to a string containing the path to the
        mount point where the target is to be mounted.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    TargetPath - Supplies a pointer to a string containing the path to the
        target file, directory, volume, or device that is to be mounted.

    TargetPathSize - Supplies the size of the target path string in bytes,
        including the null terminator.

    Flags - Supplies the flags associated with the mount operation. See
        SYS_MOUNT_FLAG_* for definitions.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsUnmount (
    PSTR MountPointPath,
    ULONG MountPointPathSize,
    ULONG Flags
    );

/*++

Routine Description:

    This routine attempts to unmount the given target. If the target is not a
    mount point, or the user does not have access to the mount point, then the
    routine will return the appropriate error. Otherwise, it will remove the
    mount point based on the supplied flags.

Arguments:

    MountPointPath - Supplies a pointer to a string containing the path to the
        mount point that is to be unmounted.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    Flags - Supplies the flags associated with the mount operation. See
        SYS_MOUNT_FLAG_* for definitions.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetMountPoints (
    PVOID *Buffer,
    PUINTN BufferSize
    );

/*++

Routine Description:

    This routine returns the list of mount points currently in the system. It
    only returns the mounts that are visible to the calling process. The caller
    is responsible for releasing the buffer.

Arguments:

    Buffer - Supplies a pointer that receives a buffer of mount point data.

    BufferSize - Supplies a pointer that receives the size of the mount point
        data.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetEffectiveAccess (
    HANDLE Directory,
    PSTR Path,
    ULONG PathSize,
    ULONG DesiredFlags,
    BOOL UseRealIds,
    PULONG EffectiveAccess
    );

/*++

Routine Description:

    This routine determines the effective access for the given path.

Arguments:

    Directory - Supplies an optional handle to a directory to start the
        search from if the supplied path is relative. Supply INVALID_HANDLE
        here to to use the current directory for relative paths.

    Path - Supplies a pointer to a string containing the path to the object
        to query access of.

    PathSize - Supplies the size of the supplied path buffer in bytes including
        the null terminator.

    DesiredFlags - Supplies the bitfield of flags the user is interested in.
        See EFFECTIVE_ACCESS_* definitions.

    UseRealIds - Supplies a boolean indicating whether the access check should
        use the real user and group IDs (TRUE) or the effective user and group
        IDs (FALSE).

    EffectiveAccess - Supplies a pointer where the effective access permission
        bits are returned. Only bits set in the desired flags parameter will be
        potentially set. All other bits will be set to 0. See
        EFFECTIVE_ACCESS_* definitions.

Return Value:

    Status code indicating if the request completed successfully.

--*/

OS_API
KSTATUS
OsLoadDriver (
    PSTR Path,
    ULONG PathSize
    );

/*++

Routine Description:

    This routine loads the given driver into kernel address space.

Arguments:

    Path - Supplies a pointer to a string containing the path to the driver.

    PathSize - Supplies the size of the supplied path buffer in bytes including
        the null terminator.

Return Value:

    Status code indicating if the request completed successfully.

--*/

OS_API
KSTATUS
OsLocateDeviceInformation (
    PUUID Uuid,
    PDEVICE_ID DeviceId,
    PDEVICE_INFORMATION_RESULT Results,
    PULONG ResultCount
    );

/*++

Routine Description:

    This routine returns instances of devices enumerating information. Callers
    can get all devices enumerating the given information type, or all
    information types enumerated by a given device. This routine must be called
    at low level.

Arguments:

    Uuid - Supplies an optional pointer to the information identifier to
        filter on. If NULL, any information type will match.

    DeviceId - Supplies an optional pointer to the device ID to match against.
        If NULL, then any device will match.

    Results - Supplies a pointer to a caller allocated buffer where the
        results will be returned.

    ResultCount - Supplies a pointer that upon input contains the size of the
        buffer in information result elements. On output, returns the number
        of elements in the query, even if the provided buffer was too small.
        Do note however that the number of results can change between two
        successive searches.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the provided buffer was not large enough to
    contain all the results. The result count will contain the required number
    of elements to contain the results.

--*/

OS_API
KSTATUS
OsGetSetDeviceInformation (
    DEVICE_ID DeviceId,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets device information.

Arguments:

    DeviceId - Supplies the device ID of the device to get or set information
        for.

    Uuid - Supplies a pointer to the identifier of the device information type
        to get or set.

    Data - Supplies a pointer to the data buffer that either contains the
        information to set or will contain the information to get on success.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output contains the actual size of the data.

    Set - Supplies a boolean indicating whether to get information (FALSE) or
        set information (TRUE).

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetSetSystemInformation (
    SYSTEM_INFORMATION_SUBSYSTEM Subsystem,
    UINTN InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    Subsystem - Supplies the subsystem to query or set information of.

    InformationType - Supplies the information type, which is specific to
        the subsystem. The type of this value is generally
        <subsystem>_INFORMATION_TYPE (eg IO_INFORMATION_TYPE).

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS if the information was successfully queried or set.

    STATUS_BUFFER_TOO_SMALL if the buffer size specified was too small. The
    required buffer size will be returned in the data size parameter.

    STATUS_DATA_LENGTH_MISMATCH if the buffer size was not correct. The
    correct buffer size will be returned in the data size parameter.

    STATUS_INVALID_PARAMETER if the given subsystem or information type is
    not known.

    Other status codes on other failures.

--*/

OS_API
KSTATUS
OsResetSystem (
    SYSTEM_RESET_TYPE ResetType
    );

/*++

Routine Description:

    This routine attempts to reboot the system.

Arguments:

    ResetType - Supplies the desired system reset type. If the given type is
        not supported and a cold reset is, then a cold reset will be
        performed.

Return Value:

    STATUS_SUCCESS if the reset request was successfully queued. The process
    should expect to receive a termination signal shortly, followed by a
    kill signal shortly after that.

    STATUS_INVALID_PARAMETER if the given reset type is not valid.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failure in the kernel
    prevented queuing of the reset system work item.

--*/

OS_API
PVOID
OsSetProgramBreak (
    PVOID NewBreak
    );

/*++

Routine Description:

    This routine gets or sets the application program break for the process.

Arguments:

    NewBreak - Supplies an optional pointer to the new break to set. If this
        is less than the original break, then no change is made. Set to NULL
        to simply get the current program break.

Return Value:

    Returns the current program break, which is either the new value set or
    the previous value.

--*/

OS_API
KSTATUS
OsMemoryMap (
    HANDLE Handle,
    IO_OFFSET Offset,
    UINTN Size,
    ULONG Flags,
    PVOID *Address
    );

/*++

Routine Description:

    This routine maps the specified object starting at the given offset for the
    requested size, in bytes. A suggested address can optionally be supplied.

Arguments:

    Handle - Supplies a pointer to an opened I/O handle.

    Offset - Supplies the offset into the I/O object where the mapping should
        begin.

    Size - Supplies the size of the mapping region, in bytes.

    Flags - Supplies a bitfield of flags. See SYS_MAP_FLAG_* for definitions.

    Address - Supplies a pointer that receives the address of the mapped
        region. The caller can optionally specify a suggested address using
        this paramter.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsMemoryUnmap (
    PVOID Address,
    UINTN Size
    );

/*++

Routine Description:

    This routine unmaps the specified region from the current process' address
    space.

Arguments:

    Address - Supplies the starting address of the memory region to unmap.

    Size - Supplies the size of the region to unmap, in bytes.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSetMemoryProtection (
    PVOID Address,
    UINTN Size,
    ULONG NewAttributes
    );

/*++

Routine Description:

    This routine set the memory protection attributes for the given region.

Arguments:

    Address - Supplies the starting address (inclusive) to change the memory
        protection for. This must be aligned to a page boundary.

    Size - Supplies the length, in bytes, of the region to change attributes
        for.

    NewAttributes - Supplies the new attributes to set. See SYS_MAP_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsMemoryFlush (
    PVOID Address,
    ULONGLONG Size,
    ULONG Flags
    );

/*++

Routine Description:

    This routine flushes a region of the current process' mapped memory to
    permament storage, if the region has a backing image.

Arguments:

    Address - Supplies the starting address of the memory region to synchronize.

    Size - Supplies the size of the region to synchronize, in bytes.

    Flags - Supplies a bitfield of flags. See SYS_MAP_SYNC_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSetThreadIdentity (
    ULONG FieldsToSet,
    PTHREAD_IDENTITY Identity
    );

/*++

Routine Description:

    This routine gets or sets a thread's identity.

Arguments:

    FieldsToSet - Supplies a bitfield indicating which identity fields to set.
        Supply zero to simply get the current thread identity. See
        THREAD_IDENTITY_FIELD_* definitions.

    Identity - Supplies a pointer that on input contains the thread identity
        fields to set. On successful output, will contain the complete new
        thread identity.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSetThreadPermissions (
    ULONG FieldsToSet,
    PTHREAD_PERMISSIONS Permissions
    );

/*++

Routine Description:

    This routine gets or sets a thread's permission masks.

Arguments:

    FieldsToSet - Supplies a bitfield indicating which permission sets or
        fields to modify. Supply zero to simply get the current thread
        permission sets. See THREAD_PERMISSION_FIELD_* definitions.

    Permissions - Supplies a pointer that on input contains the thread
        permission masks to set. On successful output, will contain the
        complete new set of permission masks.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSetSupplementaryGroups (
    BOOL Set,
    PGROUP_ID Groups,
    PUINTN Count
    );

/*++

Routine Description:

    This routine gets or sets a thread's set of supplementary groups. To set
    the supplementary groups, the thread must have the set group ID permission.

Arguments:

    Set - Supplies a boolean indicating whether to set the new groups (TRUE) or
        just get the current list of supplementary groups.

    Groups - Supplies a pointer that receives the supplementary groups for a
        get operation or contains the new group IDs to set for a set operation.

    Count - Supplies a pointer that on input contains the number of elements
        in the given buffer. On output, contains the number of valid elements.
        If STATUS_BUFFER_TOO_SMALL is returned, the number of elements needed
        will be retunred.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the given groups buffer is invalid.

    STATUS_PERMISSION_DENIED if the caller does not have the set group ID
    permission.

    STATUS_INSUFFICIENT_RESOURCES if an internal kernel allocation failed.

    STATUS_INVALID_PARAMETER if the count was too big.

    STATUS_BUFFER_TOO_SMALL if the given buffer was not big enough to contain
    all the current supplementary groups. In this case, count contains the
    number of elements needed.

--*/

OS_API
KSTATUS
OsSetResourceLimit (
    RESOURCE_LIMIT_TYPE Type,
    PRESOURCE_LIMIT NewValue,
    PRESOURCE_LIMIT OldValue
    );

/*++

Routine Description:

    This routine gets or sets the current resource limit value for a given type.

Arguments:

    Type - Supplies the resource limit type to get the limit for.

    NewValue - Supplies an optional pointer to the new limit to set. If this is
        NULL, then a new value is not set.

    OldValue - Supplies an optional pointer where the previous limit will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the resource type is not valid or the current
    value was greater than the max.

    STATUS_PERMISSION_DENIED if the caller is trying to raise the max/hard
    limit and does not have the resources permission.

--*/

OS_API
KSTATUS
OsCreateTerminal (
    HANDLE MasterDirectory,
    HANDLE SlaveDirectory,
    PSTR MasterPath,
    UINTN MasterPathLength,
    PSTR SlavePath,
    UINTN SlavePathLength,
    ULONG MasterOpenFlags,
    FILE_PERMISSIONS MasterCreatePermissions,
    FILE_PERMISSIONS SlaveCreatePermissions,
    PHANDLE MasterHandle
    );

/*++

Routine Description:

    This routine creates a new pseudo-terminal master and slave at the given
    paths.

Arguments:

    MasterDirectory - Supplies an optional handle to a directory for relative
        paths when creating the master. Supply INVALID_HANDLE to use the
        current working directory.

    SlaveDirectory - Supplies an optional handle to a directory for relative
        paths when creating the slave. Supply INVALID_HANDLE to use the
        current working directory.

    MasterPath - Supplies an optional pointer to the path to create for the
        master.

    MasterPathLength - Supplies the length of the master side path buffer in
        bytes, including the null terminator.

    SlavePath - Supplies an optional pointer to the path to create for the
        master.

    SlavePathLength - Supplies the length of the slave side path buffer in
        bytes, including the null terminator.

    MasterOpenFlags - Supplies the open flags to use when opening the master.
        Only read, write, and "no controlling terminal" flags are honored.

    MasterCreatePermissions - Supplies the permissions to apply to the created
        master side.

    SlaveCreatePermissions - Supplies the permission to apply to the created
        slave side.

    MasterHandle - Supplies a pointer where the opened handle to the master
        will be returned on success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetFilePath (
    HANDLE Handle,
    PSTR Path,
    PUINTN PathSize
    );

/*++

Routine Description:

    This routine returns the file path for the given handle, if possible.

Arguments:

    Handle - Supplies the open handle to get the file path of.

    Path - Supplies a pointer where the path will be returned on success.

    PathSize - Supplies a pointer that on input contains the size of the path
        buffer. On output, returns the needed size of the path buffer, even
        if the supplied buffer is too small.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if the given handle is not valid.

    STATUS_PATH_NOT_FOUND if no path exists for the given handle.

    STATUS_BUFFER_TOO_SMALL if the supplied path buffer was not large enough to
    contain the complete path. In this case the path size returned is the size
    needed.

    STATUS_ACCESS_VIOLATION if the path buffer was invalid.

--*/

OS_API
VOID
OsSetThreadIdPointer (
    PTHREAD_ID Pointer
    );

/*++

Routine Description:

    This routine sets the thread ID pointer in the kernel. If this value is
    non-null when the thread exits, then zero will be written to this address,
    and a UserLockWake operation will be performed to wake up one thread.

Arguments:

    Pointer - Supplies the new user mode pointer to the thread ID that will be
        cleared when the thread exits. Supply NULL to clear the thread ID
        pointer in the kernel.

Return Value:

    None.

--*/

OS_API
FILE_PERMISSIONS
OsSetUmask (
    FILE_PERMISSIONS NewMask
    );

/*++

Routine Description:

    This routine sets file permission mask for the current process. Bits set
    in this mask will be automatically cleared out of the permissions of any
    file or directory created.

Arguments:

    NewMask - Supplies the new mask to set.

Return Value:

    Returns the previously set mask.

--*/

OS_API
KSTATUS
OsDuplicateHandle (
    HANDLE ExistingHandle,
    PHANDLE NewHandle,
    ULONG Flags
    );

/*++

Routine Description:

    This routine duplicates a given handle at a new handle.

Arguments:

    ExistingHandle - Supplies the handle to duplicate.

    NewHandle - Supplies a pointer that contains the destination handle value
        for the new handle. If this is INVALID_HANDLE, then the duplicated
        handle will be the lowest available handle value, and will be returned
        here. If this is not INVALID_HANDLE, then the previous handle at that
        location will be closed. If the new handle equals the existing handle,
        failure is returned.

    Flags - Supplies open flags to be set on the new handle. Only
        SYS_OPEN_FLAG_CLOSE_ON_EXECUTE is permitted. If not set, the new handle
        will have the close on execute flag cleared.

Return Value:

    Status code.

--*/

OS_API
PVOID
OsHeapAllocate (
    UINTN Size,
    UINTN Tag
    );

/*++

Routine Description:

    This routine allocates memory from the heap.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identifier to associate with the allocation, which aides
        in debugging.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

OS_API
VOID
OsHeapFree (
    PVOID Memory
    );

/*++

Routine Description:

    This routine frees memory, making it available for other users of the heap.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

OS_API
PVOID
OsHeapReallocate (
    PVOID Memory,
    UINTN NewSize,
    UINTN Tag
    );

/*++

Routine Description:

    This routine resizes the given allocation, potentially creating a new
    buffer and copying the old contents in.

Arguments:

    Memory - Supplies the original active allocation. If this parameter is
        NULL, this routine will simply allocate memory.

    NewSize - Supplies the new required size of the allocation. If this is
        0, then the original allocation will simply be freed.

    Tag - Supplies an identifier to associate with the allocation, which aides
        in debugging.

Return Value:

    Returns a pointer to a buffer with the new size (and original contents) on
    success. This may be a new buffer or the same one.

    NULL on failure or if the new size supplied was zero.

--*/

OS_API
KSTATUS
OsHeapAlignedAllocate (
    PVOID *Memory,
    UINTN Alignment,
    UINTN Size,
    UINTN Tag
    );

/*++

Routine Description:

    This routine allocates aligned memory from the heap.

Arguments:

    Memory - Supplies a pointer that receives the pointer to the aligned
        allocation on success.

    Alignment - Supplies the requested alignment for the allocation, in bytes.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identification tag to mark this allocation with.

Return Value:

    Status code.

--*/

OS_API
VOID
OsValidateHeap (
    VOID
    );

/*++

Routine Description:

    This routine validates the memory heap for consistency, ensuring that no
    corruption or other errors are present in the heap.

Arguments:

    None.

Return Value:

    None.

--*/

OS_API
PPROCESS_ENVIRONMENT
OsCreateEnvironment (
    PSTR ImagePath,
    ULONG ImagePathLength,
    PSTR *ArgumentValues,
    ULONG ArgumentValuesTotalLength,
    ULONG ArgumentCount,
    PSTR *EnvironmentValues,
    ULONG EnvironmentValuesTotalLength,
    ULONG EnvironmentCount
    );

/*++

Routine Description:

    This routine creates an environment that can be passed to the kernel for
    execution of an image. This routine will use the heap.

Arguments:

    ImagePath - Supplies a pointer to the name of the image that will be
        executed.

    ImagePathLength - Supplies the length of the image path buffer in bytes,
        including the null terminator.

    ArgumentValues - Supplies an array of pointers to arguments to pass to the
        image.

    ArgumentValuesTotalLength - Supplies the total length of all arguments, in
        bytes, including their null terminators.

    ArgumentCount - Supplies the number of arguments in the argument values
        array.

    EnvironmentValues - Supplies an array of pointers to environment variables
        to set in the new environment. Environment variables take the form
        "name=value".

    EnvironmentValuesTotalLength - Supplies the total length of all environment
        variables, in bytes, including their null terminators.

    EnvironmentCount - Supplies the number of environment variable strings
        present in the environment values array.

Return Value:

    Returns a pointer to a heap allocated environment, suitable for sending to
    the execute image system call.

    NULL on failure.

--*/

OS_API
VOID
OsDestroyEnvironment (
    PPROCESS_ENVIRONMENT Environment
    );

/*++

Routine Description:

    This routine destroys an environment created with the create environment
    function.

Arguments:

    Environment - Supplies a pointer to the environment to destroy.

Return Value:

    None.

--*/

OS_API
PPROCESS_ENVIRONMENT
OsGetCurrentEnvironment (
    VOID
    );

/*++

Routine Description:

    This routine gets the environment for the current process.

Arguments:

    None.

Return Value:

    Returns a pointer to the current environment. This is shared memory, and
    should not be altered by the caller.

--*/

OS_API
KSTATUS
OsSocketCreatePair (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    HANDLE Sockets[2]
    );

/*++

Routine Description:

    This routine creates a pair of sockets that are connected to each other.

Arguments:

    Domain - Supplies the network domain to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value used on the network.

    OpenFlags - Supplies an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

    Sockets - Supplies an array where the two handles to the connected
        sockets will be returned on success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketCreate (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    PHANDLE Socket
    );

/*++

Routine Description:

    This routine creates a new socket for communication.

Arguments:

    Domain - Supplies the network addressing domain to use for the socket.

    Type - Supplies the socket type.

    Protocol - Supplies the raw protocol value to use for the socket. This is
        network specific.

    OpenFlags - Supplies an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

    Socket - Supplies a pointer where the new socket handle will be returned on
        success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketBind (
    HANDLE Socket,
    PNETWORK_ADDRESS Address,
    PSTR Path,
    UINTN PathSize
    );

/*++

Routine Description:

    This routine binds a newly created socket to a local address.

Arguments:

    Socket - Supplies a handle to the fresh socket.

    Address - Supplies a pointer to the local network address to bind to.

    Path - Supplies a pointer to the path to bind to in the case that this is
        a local (Unix) socket.

    PathSize - Supplies the size of the path in bytes including the null
        terminator.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketListen (
    HANDLE Socket,
    ULONG SuggestedBacklog
    );

/*++

Routine Description:

    This routine activates a socket, making it eligible to accept new
    incoming connections.

Arguments:

    Socket - Supplies the socket to activate.

    SuggestedBacklog - Supplies a suggestion to the kernel as to the number of
        un-accepted incoming connections to queue up before incoming
        connections are refused.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketAccept (
    HANDLE Socket,
    PHANDLE NewSocket,
    PNETWORK_ADDRESS Address,
    PSTR RemotePath,
    PUINTN RemotePathSize,
    ULONG OpenFlags
    );

/*++

Routine Description:

    This routine accepts an incoming connection on a listening socket and spins
    it off into a new socket. This routine will block until an incoming
    connection request is received.

Arguments:

    Socket - Supplies the listening socket to accept a new connection from.

    NewSocket - Supplies a pointer where a new socket representing the
        incoming connection will be returned on success.

    Address - Supplies an optional pointer where the address of the remote host
        will be returned.

    RemotePath - Supplies a pointer where the remote path of the client socket
        will be copied on success. This only applies to local sockets.

    RemotePathSize - Supplies a pointer that on input contains the size of the
        remote path buffer. On output, contains the true size of the remote
        path, even if it was bigger than the input.

    OpenFlags - Supplies an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketConnect (
    HANDLE Socket,
    PNETWORK_ADDRESS Address,
    PSTR RemotePath,
    UINTN RemotePathSize
    );

/*++

Routine Description:

    This routine attempts to establish a new outgoing connection on a socket.

Arguments:

    Socket - Supplies a handle to the fresh socket to use to establish the
        connection.

    Address - Supplies a pointer to the destination socket address.

    RemotePath - Supplies a pointer to the path to connect to, if this is a
        local socket.

    RemotePathSize - Supplies the size of the remote path buffer in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketPerformIo (
    HANDLE Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PVOID Buffer
    );

/*++

Routine Description:

    This routine performs I/O on an open handle.

Arguments:

    Socket - Supplies a pointer to the socket.

    Parameters - Supplies a pointer to the socket I/O request details.

    Buffer - Supplies a pointer to the buffer containing the data to write or
        where the read data should be returned, depending on the operation.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketPerformVectoredIo (
    HANDLE Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_VECTOR VectorArray,
    UINTN VectorCount
    );

/*++

Routine Description:

    This routine performs vectored I/O on an open handle.

Arguments:

    Socket - Supplies a pointer to the socket.

    Parameters - Supplies a pointer to the socket I/O request details.

    VectorArray - Supplies an array of I/O vector structures to do I/O to/from.

    VectorCount - Supplies the number of elements in the vector array.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketGetSetInformation (
    HANDLE Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN Option,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets socket information.

Arguments:

    Socket - Supplies the socket handle.

    InformationType - Supplies the socket information type category to which
        specified option belongs.

    Option - Supplies the option to get or set, which is specific to the
        information type. The type of this value is generally
        SOCKET_<information_type>_OPTION.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation. If the
        buffer is too small for a get request, the truncated data will be
        returned and the routine will fail with STATUS_BUFFER_TOO_SMALL.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, this contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSocketShutdown (
    HANDLE Socket,
    ULONG ShutdownType
    );

/*++

Routine Description:

    This routine shuts down communication with a given socket.

Arguments:

    Socket - Supplies the socket handle.

    ShutdownType - Supplies the shutdown type to perform. See the
        SOCKET_SHUTDOWN_* definitions. These are NOT the same as the C library
        definitions.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_A_SOCKET if the given handle wasn't a socket.

    Other error codes on failure.

--*/

//
// Timekeeping functions
//

OS_API
ULONGLONG
OsGetRecentTimeCounter (
    VOID
    );

/*++

Routine Description:

    This routine returns a relatively recent snap of the time counter.

Arguments:

    None.

Return Value:

    Returns the fairly recent snap of the time counter.

--*/

OS_API
ULONGLONG
OsQueryTimeCounter (
    VOID
    );

/*++

Routine Description:

    This routine returns the current (most up to date) value of the system's
    time counter.

Arguments:

    None.

Return Value:

    Returns the time counter value.

--*/

OS_API
ULONGLONG
OsGetTimeCounterFrequency (
    VOID
    );

/*++

Routine Description:

    This routine returns the frequency of the time counter.

Arguments:

    None.

Return Value:

    Returns the frequency, in Hertz (ticks per second) of the time counter.

--*/

OS_API
ULONGLONG
OsGetProcessorCounterFrequency (
    VOID
    );

/*++

Routine Description:

    This routine returns the frequency of the boot processor counter.

Arguments:

    None.

Return Value:

    Returns the frequency, in Hertz (ticks per second) of the boot processor
    counter.

--*/

OS_API
VOID
OsConvertSystemTimeToTimeCounter (
    PSYSTEM_TIME SystemTime,
    PULONGLONG TimeCounter
    );

/*++

Routine Description:

    This routine converts a system time value into a time counter value.

Arguments:

    SystemTime - Supplies a pointer to the system time convert to a time
        counter value.

    TimeCounter - Supplies a pointer where the time counter value will be
        returned.

Return Value:

    None.

--*/

OS_API
VOID
OsConvertTimeCounterToSystemTime (
    ULONGLONG TimeCounter,
    PSYSTEM_TIME SystemTime
    );

/*++

Routine Description:

    This routine converts a time counter value into a system time value.

Arguments:

    TimeCounter - Supplies the time counter value to convert.

    SystemTime - Supplies a pointer where the converted system time will be
        returned.

Return Value:

    None.

--*/

OS_API
VOID
OsGetSystemTime (
    PSYSTEM_TIME Time
    );

/*++

Routine Description:

    This routine returns the current system time.

Arguments:

    Time - Supplies a pointer where the system time will be returned.

Return Value:

    None.

--*/

OS_API
VOID
OsGetHighPrecisionSystemTime (
    PSYSTEM_TIME Time
    );

/*++

Routine Description:

    This routine returns a high precision snap of the current system time.

Arguments:

    Time - Supplies a pointer where the system time will be returned.

Return Value:

    None.

--*/

OS_API
KSTATUS
OsSetSystemTime (
    PSYSTEM_TIME NewTime,
    ULONGLONG TimeCounter
    );

/*++

Routine Description:

    This routine sets the current system time.

Arguments:

    NewTime - Supplies a pointer to the new system time to set.

    TimeCounter - Supplies the time counter value corresponding with the
        moment the system time was meant to be set by the caller.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetResourceUsage (
    RESOURCE_USAGE_REQUEST Request,
    PROCESS_ID Id,
    PRESOURCE_USAGE Usage,
    PULONGLONG Frequency
    );

/*++

Routine Description:

    This routine returns resource usage information for the specified process
    or thread.

Arguments:

    Request - Supplies the request type, indicating whether to get resource
        usage for a process, a process' children, or a thread.

    Id - Supplies the process or thread ID. Supply -1 to use the current
        process or thread.

    Usage - Supplies a pointer where the resource usage is returned on success.

    Frequency - Supplies a pointer that receives the frequency of the
        processor(s).

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsCreateTimer (
    ULONG SignalNumber,
    PUINTN SignalValue,
    PTHREAD_ID ThreadId,
    PLONG TimerHandle
    );

/*++

Routine Description:

    This routine creates a new timer.

Arguments:

    SignalNumber - Supplies the signal number to raise when the timer expires.

    SignalValue - Supplies an optional pointer to the signal value to put in
        the signal information structure when the signal is raised. If this is
         NULL, the timer number will be returned as the signal value.

    ThreadId - Supplies an optional ID of the thread to signal when the timer
        expires. If not supplied, the process will be signaled.

    TimerHandle - Supplies a pointer where the timer handle will be returned on
        success.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsDeleteTimer (
    LONG Timer
    );

/*++

Routine Description:

    This routine disarms and deletes a timer.

Arguments:

    Timer - Supplies the timer to delete.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetTimerInformation (
    LONG Timer,
    PTIMER_INFORMATION Information
    );

/*++

Routine Description:

    This routine gets the given timer's information.

Arguments:

    Timer - Supplies the timer to query.

    Information - Supplies a pointer where the timer information will be
        returned.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSetTimerInformation (
    LONG Timer,
    PTIMER_INFORMATION Information
    );

/*++

Routine Description:

    This routine sets the given timer's information.

Arguments:

    Timer - Supplies the timer to set.

    Information - Supplies a pointer to the information to set.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsGetITimer (
    ITIMER_TYPE Type,
    PULONGLONG DueTime,
    PULONGLONG Period
    );

/*++

Routine Description:

    This routine gets the current value of one of the per-thread interval
    timers.

Arguments:

    Type - Supplies the timer type. Valid values are ITimerReal, which tracks
        wall clock time, ITimerVirtual, which tracks user mode CPU cycles
        spent in this thread, and ITimerProfile, which tracks user and kernel
        CPU cycles spent in this thread.

    DueTime - Supplies a pointer where the relative due time will be returned
        for this timer. Zero will be returned if the timer is not currently
        armed or has already expired. The units here are time counter ticks for
        the real timer, and processor counter ticks for the virtual and profile
        timers.

    Period - Supplies a pointer where the periodic interval of the timer
        will be returned. Zero indicates the timer is not set to rearm itself.
        The units here are time counter ticks for the real timer, and processor
        counter ticks for the firtual and profile timers.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsSetITimer (
    ITIMER_TYPE Type,
    PULONGLONG DueTime,
    PULONGLONG Period
    );

/*++

Routine Description:

    This routine sets the current value of one of the per-thread interval
    timers.

Arguments:

    Type - Supplies the timer type. Valid values are ITimerReal, which tracks
        wall clock time, ITimerVirtual, which tracks user mode CPU cycles
        spent in this thread, and ITimerProfile, which tracks user and kernel
        CPU cycles spent in this thread.

    DueTime - Supplies a pointer to the relative time to set in the timer.
        Supply zero to disable the timer. The units here are time counter ticks
        for the real timer, and processor counter ticks for the virtual and
        profile timers. On output, this will contain the remaining time left on
        the previously set value for the timer.

    Period - Supplies a pointer to the periodic interval to set. Set zero to
        make the timer only fire once. The units here are time counter ticks
        for the real timer, and processor counter ticks for the firtual and
        profile timers. On output, the previous period will be returned.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsDelayExecution (
    BOOL TimeTicks,
    ULONGLONG Interval
    );

/*++

Routine Description:

    This routine blocks the current thread for the specified amount of time.

Arguments:

    TimeTicks - Supplies a boolean indicating if the interval parameter is
        represented in time counter ticks (TRUE) or microseconds (FALSE).

    Interval - Supplies the interval to wait. If the time ticks parameter is
        TRUE, this parameter represents an absolute time in time counter ticks.
        If the time ticks parameter is FALSE, this parameter represents a
        relative time from now in microseconds. If an interval of 0 is
        supplied, this routine is equivalent to KeYield.

Return Value:

    STATUS_SUCCESS if the wait completed.

    STATUS_INTERRUPTED if the wait was interrupted.

--*/

OS_API
KSTATUS
OsLoadLibrary (
    PSTR LibraryName,
    ULONG Flags,
    PHANDLE Handle
    );

/*++

Routine Description:

    This routine loads a dynamic library.

Arguments:

    LibraryName - Supplies a pointer to the library name to load.

    Flags - Supplies a bitfield of flags associated with the request.

    Handle - Supplies a pointer where a handle to the dynamic library will be
        returned on success. INVALID_HANDLE will be returned on failure.

Return Value:

    Status code.

--*/

OS_API
VOID
OsFreeLibrary (
    HANDLE Library
    );

/*++

Routine Description:

    This routine indicates a release of the resources associated with a
    previously loaded library. This may or may not actually unload the library
    depending on whether or not there are other references to it.

Arguments:

    Library - Supplies the library to release.

Return Value:

    None.

--*/

OS_API
KSTATUS
OsGetSymbolAddress (
    HANDLE Library,
    PSTR SymbolName,
    HANDLE Skip,
    PVOID *Address
    );

/*++

Routine Description:

    This routine returns the address of the given symbol in the given image.
    Both the image and all of its imports will be searched.

Arguments:

    Library - Supplies the image to look up. Supply NULL to search the global
        scope.

    SymbolName - Supplies a pointer to a null terminated string containing the
        name of the symbol to look up.

    Skip - Supplies an optional pointer to a library to skip. Supply NULL or
        INVALID_HANDLE here to not skip any libraries.

    Address - Supplies a pointer that on success receives the address of the
        symbol, or NULL on failure.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if the library handle is not valid.

    STATUS_NOT_FOUND if the symbol could not be found.

--*/

OS_API
KSTATUS
OsGetImageSymbolForAddress (
    PVOID Address,
    POS_IMAGE_SYMBOL Symbol
    );

/*++

Routine Description:

    This routine resolves the given address into an image and closest symbol
    whose address is less than or equal to the given address.

Arguments:

    Address - Supplies the address to look up.

    Symbol - Supplies a pointer to a structure that receives the resolved
        symbol information.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if the library handle is not valid.

    STATUS_NOT_FOUND if the address could not be found.

--*/

OS_API
HANDLE
OsGetImageForAddress (
    PVOID Address
    );

/*++

Routine Description:

    This routine returns a handle to the image that contains the given address.

Arguments:

    Address - Supplies the address to look up.

Return Value:

    INVALID_HANDLE if no image contains the given address.

    On success, returns the dynamic image handle that contains the given
    address.

--*/

OS_API
KSTATUS
OsFlushCache (
    PVOID Address,
    UINTN Size
    );

/*++

Routine Description:

    This routine flushes the caches for a region of memory after executable
    code has been modified.

Arguments:

    Address - Supplies the address of the region to flush.

    Size - Supplies the number of bytes in the region.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the given address was not valid.

--*/

OS_API
KSTATUS
OsCreateThreadData (
    PVOID *ThreadData
    );

/*++

Routine Description:

    This routine creates the OS library data necessary to manage a new thread.
    This function is usually called by the C library.

Arguments:

    ThreadData - Supplies a pointer where a pointer to the thread data will be
        returned on success. It is the callers responsibility to destroy this
        thread data. The contents of this data are opaque and should not be
        interpreted. The caller should set this returned pointer as the
        thread pointer.

Return Value:

    Status code.

--*/

OS_API
VOID
OsDestroyThreadData (
    PVOID ThreadData
    );

/*++

Routine Description:

    This routine destroys the previously created OS library thread data.

Arguments:

    ThreadData - Supplies the previously returned thread data.

Return Value:

    Status code.

--*/

OS_API
VOID
OsIterateImages (
    PIMAGE_ITERATOR_ROUTINE IteratorRoutine,
    PVOID Context
    );

/*++

Routine Description:

    This routine iterates over all images currently loaded in the process.

Arguments:

    IteratorRoutine - Supplies a pointer to the routine to call for each image.

    Context - Supplies an opaque context pointer that is passed directly into
        the iterator routine.

Return Value:

    None.

--*/

OS_API
VOID
OsInitializeLock (
    POS_LOCK Lock,
    ULONG SpinCount
    );

/*++

Routine Description:

    This routine initializes an OS lock.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

    SpinCount - Supplies the number of initial consecutive attempts to make
        when acquiring the lock. Larger values here minimize the delay between
        when the lock is freed and subsequently reacquired, but are bad for
        power performance as the thread is burning energy doing nothing. Most
        applications should set this to SPIN_LOCK_DEFAULT_SPIN_COUNT.

Return Value:

    None.

--*/

OS_API
VOID
OsAcquireLock (
    POS_LOCK Lock
    );

/*++

Routine Description:

    This routine acquires the given OS lock. It is not recursive, meaning
    that if the lock is already held by the current thread this routine will
    never return.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

OS_API
BOOL
OsTryToAcquireLock (
    POS_LOCK Lock
    );

/*++

Routine Description:

    This routine performs a single attempt to acquire the given OS lock.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    TRUE if the lock was successfully acquired.

    FALSE if the lock was already held and could not be acquired.

--*/

OS_API
VOID
OsReleaseLock (
    POS_LOCK Lock
    );

/*++

Routine Description:

    This routine releases the given OS lock. The lock must have been
    previously acquired, obviously.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

OS_API
VOID
OsRwLockInitialize (
    POS_RWLOCK Lock,
    ULONG Flags
    );

/*++

Routine Description:

    This routine initializes a read/write lock.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    Flags - Supplies a bitfield of flags governing the lock behavior.

Return Value:

    None.

--*/

OS_API
KSTATUS
OsRwLockRead (
    POS_RWLOCK Lock
    );

/*++

Routine Description:

    This routine acquires the read/write lock for read access. Multiple readers
    can acquire the lock simultaneously, but any writes that try to acquire
    the lock while it's held for read will block. Readers that try to
    acquire the lock while it's held for write will also block.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsRwLockReadTimed (
    POS_RWLOCK Lock,
    ULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine acquires the read/write lock for read access just like the
    read lock function, except that this function will return after the
    specified deadline if the lock could not be acquired.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    TimeoutInMilliseconds - Supplies the duration to wait in milliseconds.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsRwLockTryRead (
    POS_RWLOCK Lock
    );

/*++

Routine Description:

    This routine performs a single attempt at acquiring the lock for read
    access.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_RESOURCE_IN_USE if the lock is already held.

--*/

OS_API
KSTATUS
OsRwLockWrite (
    POS_RWLOCK Lock
    );

/*++

Routine Description:

    This routine acquires the read/write lock for write access. The lock can
    only be acquired for write access if there are no readers and no other
    writers.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsRwLockWriteTimed (
    POS_RWLOCK Lock,
    ULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine acquires the read/write lock for write access just like the
    write lock function, except that this function will return after the
    specified deadline if the lock could not be acquired.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

    AbsoluteTimeout - Supplies a pointer to the absolute deadline after which
        this function should give up and return failure.

Return Value:

    Status code.

--*/

OS_API
KSTATUS
OsRwLockTryWrite (
    POS_RWLOCK Lock
    );

/*++

Routine Description:

    This routine performs a single attempt at acquiring the lock for write
    access.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_RESOURCE_IN_USE if the lock is already held.

--*/

OS_API
KSTATUS
OsRwLockUnlock (
    POS_RWLOCK Lock
    );

/*++

Routine Description:

    This routine unlocks a read/write lock that's been acquired by this thread
    for either read or write.

Arguments:

    Lock - Supplies a pointer to the read/write lock.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PERMISSION_DENIED if the lock is not held or was not held by this
    thread.

--*/

OS_API
KSTATUS
OsUserLock (
    PVOID Address,
    ULONG Operation,
    PULONG Value,
    ULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine performs a cooperative locking operation with the kernel.

Arguments:

    Address - Supplies a pointer to a 32-bit value representing the lock in
        user mode.

    Operation - Supplies the operation of type USER_LOCK_OPERATION, as well as
        any flags, see USER_LOCK_* definitions. Valid operations are:

        UserLockWait - Puts the current thread to sleep atomically if the value
        at the given address is the same as the value parameter passed in.

        UserLockWake - Wakes the number of threads given in the value that are
        blocked on the given address.

    Value - Supplies a pointer whose value depends on the operation. For wait
        operations, this contains the value to check the address against. This
        is not used on output for wait operations. For wake operations, this
        contains the number of processes to wake on input. On output, contains
        the number of processes woken.

    TimeoutInMilliseconds - Supplies the number of milliseconds for a wait
        operation to complete before timing out. Set to
        SYS_WAIT_TIME_INDEFINITE to wait forever. This is not used on wake
        operations.

Return Value:

    STATUS_SUCCESS if the wait or wake succeeded.

    STATUS_OPERATION_WOULD_BLOCK if for a wait operation the value at the given
    address was not equal to the supplied value.

    STATUS_TIMEOUT if a wait operation timed out before the wait was satisfied.

    STATUS_INTERRUPTED if a signal arrived before a wait was completed or timed
    out.

--*/

OS_API
PVOID
OsGetTlsAddress (
    PTLS_INDEX Entry
    );

/*++

Routine Description:

    This routine returns the address of the given thread local storage symbol.
    This routine supports a C library call, references to which are emitted
    directly by the compiler.

Arguments:

    Entry - Supplies a pointer to the TLS entry to get the symbol address for.

Return Value:

    Returns a pointer to the thread local storage symbol on success.

--*/

OS_API
UINTN
OsGetThreadId (
    VOID
    );

/*++

Routine Description:

    This routine returns the currently running thread's identifier.

Arguments:

    None.

Return Value:

    Returns the current thread's ID. This number will be unique to the current
    thread as long as the thread is running.

--*/

OS_API
KSTATUS
OsSetThreadPointer (
    PVOID Pointer
    );

/*++

Routine Description:

    This routine sets the thread control pointer, which points to the thread
    control block. This function should only be called by the C library, not by
    user applications.

Arguments:

    Pointer - Supplies a pointer to associate with the thread in an
        architecture-specific way.

Return Value:

    Status code.

--*/

OS_API
BOOL
OsTestProcessorFeature (
    ULONG Feature
    );

/*++

Routine Description:

    This routine determines if a given processor features is supported or not.

Arguments:

    Feature - Supplies the feature to test, which is an enum of type
        OS_<arch>_PROCESSOR_FEATURE.

Return Value:

    TRUE if the feature is set.

    FALSE if the feature is not set or not recognized.

--*/

#ifdef __cplusplus

}

#endif

