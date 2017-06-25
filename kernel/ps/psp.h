/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    psp.h

Abstract:

    This header contains internal definitions for the process and thread
    library.

Author:

    Evan Green 6-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro evaluates to non-zero if the given signal is blocked (or running)
// on the given thread.
//

#define IS_SIGNAL_BLOCKED(_Thread, _Signal) \
    (IS_SIGNAL_SET((_Thread)->BlockedSignals, (_Signal)))

//
// This macro evaluates to non-zero if the given signal queue entry is a child
// signal sent by the kernel.
//

#define IS_CHILD_SIGNAL(_SignalQueueEntry)              \
    (((_SignalQueueEntry)->Parameters.SignalNumber ==   \
     SIGNAL_CHILD_PROCESS_ACTIVITY) &&                  \
     ((_SignalQueueEntry)->Parameters.SignalCode > SIGNAL_CODE_USER))

//
// ---------------------------------------------------------------- Definitions
//

#define PS_DEBUG_ALLOCATION_TAG 0x44707350 // 'DpsP'

#define PS_DEFAULT_UMASK FILE_PERMISSION_OTHER_WRITE

//
// Define the base OS library, which is loaded into every executable address
// space.
//

#define OS_BASE_LIBRARY "libminocaos.so.1"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Process related globals.
//

extern POBJECT_HEADER PsProcessDirectory;
extern PQUEUED_LOCK PsProcessListLock;
extern LIST_ENTRY PsProcessListHead;
extern ULONG PsProcessCount;
extern volatile PROCESS_ID PsNextProcessId;
extern PKPROCESS PsKernelProcess;

//
// Define the initial architecture-specific contents of the thread pointer data
// for a newly created thread.
//

extern const ULONGLONG PsInitialThreadPointer;

//
// Stores the ID for the next thread to be created.
//

extern volatile THREAD_ID PsNextThreadId;

//
// Stores handles to frequently used locations.
//

extern PIO_HANDLE PsOsBaseLibrary;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
PspSetThreadUserStackSize (
    PKTHREAD Thread,
    UINTN NewStackSize
    );

/*++

Routine Description:

    This routine changes the given thread's user mode stack size.

Arguments:

    Thread - Supplies a pointer to the thread whose stack size should be
        changed.

    NewStackSize - Supplies the new stack size to set. If 0 is supplied, the
        user mode stack will be destroyed.

Return Value:

    Status code.

--*/

VOID
PspKernelThreadStart (
    VOID
    );

/*++

Routine Description:

    This routine performs common initialization for all kernel mode threads, and
    executes the primary thread routine.

Arguments:

    None.

Return Value:

    Does not return. Eventually exits by killing the thread.

--*/

KSTATUS
PspInitializeThreadSupport (
    VOID
    );

/*++

Routine Description:

    This routine performs one-time system initialization for thread support.

Arguments:

    None.

Return Value:

    Status code.

--*/

PKTHREAD
PspCloneThread (
    PKPROCESS DestinationProcess,
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine clones a user mode thread from another process into the
    destination thread. This routine is designed to support the fork process
    system call.

Arguments:

    DestinationProcess - Supplies a pointer to the process the new thread
        should be created under.

    Thread - Supplies a pointer to the thread to clone.

    TrapFrame - Supplies a pointer to the trap frame to set initial thread
        state to. A copy of this trap frame will be made.

Return Value:

    Returns a pointer to the new thread on success, or NULL on failure.

--*/

KSTATUS
PspResetThread (
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame,
    PINTN ReturnValue
    );

/*++

Routine Description:

    This routine resets a user mode thread. It assumes that the user mode stack
    was freed out from under it, and sets up a new stack.

Arguments:

    Thread - Supplies a pointer to the thread to reset. The thread must be a
        user mode thread. A new user mode stack will be allocated for it, the
        old one will not be freed. Commonly, this parameter will be a pointer
        to the currently running thread.

    TrapFrame - Supplies a pointer to the initial trap frame to reset the thread
        to.

    ReturnValue - Supplies a pointer that receives the value that the reset
        user mode thread should return when exiting back to user mode.

Return Value:

    Status code.

--*/

PKTHREAD
PspGetThreadById (
    PKPROCESS Process,
    THREAD_ID ThreadId
    );

/*++

Routine Description:

    This routine returns the thread with the given thread ID under the given
    process. This routine also increases the reference count of the returned
    thread.

Arguments:

    Process - Supplies a pointer to the process to search under.

    ThreadId - Supplies the thread ID to search for.

Return Value:

    Returns a pointer to the thread with the corresponding ID. The reference
    count will be increased by one.

    NULL if no such thread could be found.

--*/

VOID
PspThreadTermination (
    VOID
    );

/*++

Routine Description:

    This routine is called when a thread finishes execution, it performs some
    cleanup and calls the scheduler to exit the thread. This routine runs in
    the context of the thread itself.

Arguments:

    None.

Return Value:

    Does not return. Eventually exits by killing the thread.

--*/

KSTATUS
PspInitializeImageSupport (
    PVOID KernelLowestAddress,
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine initializes the image library for use in the kernel.

Arguments:

    KernelLowestAddress - Supplies the lowest address of the kernel's image.
        This is used to avoid loading the kernel image in the debugger twice.

    ListHead - Supplies a pointer to the head of the list of loaded images.

Return Value:

    Status code.

--*/

KSTATUS
PspImCloneProcessImages (
    PKPROCESS Source,
    PKPROCESS Destination
    );

/*++

Routine Description:

    This routine makes a copy of the given process' image list.

Arguments:

    Source - Supplies a pointer to the source process.

    Destination - Supplies a pointer to the destination process.

Return Value:

    Status code.

--*/

VOID
PspImUnloadAllImages (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine unloads all images in the given process.

Arguments:

    Process - Supplies a pointer to the process whose images should be unloaded.

Return Value:

    None.

--*/

KSTATUS
PspProcessUserModeModuleChange (
    PPROCESS_DEBUG_MODULE_CHANGE ModuleChangeUser
    );

/*++

Routine Description:

    This routine handles module change notifications from user mode.

Arguments:

    ModuleChangeUser - Supplies the module change notification from user mode.

Return Value:

    None.

--*/

KSTATUS
PspLoadProcessImagesIntoKernelDebugger (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine loads the images in the given process into the kernel debugger.

Arguments:

    Process - Supplies a pointer to the process.

Return Value:

    Status code.

--*/

KSTATUS
PspCopyProcess (
    PKPROCESS Process,
    PKTHREAD MainThread,
    PTRAP_FRAME TrapFrame,
    ULONG Flags,
    PKPROCESS *CreatedProcess
    );

/*++

Routine Description:

    This routine creates a copy of the given process. It copies all images,
    image sections, and open file handles, but copies only the main thread. This
    routine must only be called at low level.

Arguments:

    Process - Supplies a pointer to the process to copy.

    MainThread - Supplies a pointer to the thread to copy (which is assumed to
        be owned by the process to be copied).

    TrapFrame - Supplies a pointer to the trap frame of the interrupted main
        thread.

    Flags - Supplies a bitfield of flags governing the creation of the new
        process. See FORK_FLAG_* definitions.

    CreatedProcess - Supplies an optional pointer that will receive a pointer to
        the created process on success.

Return Value:

    Status code.

--*/

PKPROCESS
PspCreateProcess (
    PCSTR CommandLine,
    ULONG CommandLineSize,
    PPROCESS_ENVIRONMENT SourceEnvironment,
    PPROCESS_IDENTIFIERS Identifiers,
    PVOID ControllingTerminal,
    PPATH_POINT RootDirectory,
    PPATH_POINT WorkingDirectory,
    PPATH_POINT SharedMemoryDirectory
    );

/*++

Routine Description:

    This routine creates a new process with no threads.

Arguments:

    CommandLine - Supplies a pointer to a string containing the command line to
        invoke (the executable and any arguments).

    CommandLineSize - Supplies the size of the command line in bytes, including
        the null terminator.

    SourceEnvironment - Supplies an optional pointer to the initial environment.
        The image name and arguments will be replaced with those given on the
        command line.

    Identifiers - Supplies an optional pointer to the parent process
        identifiers.

    ControllingTerminal - Supplies a pointer to the controlling terminal to set
        for this process.

    RootDirectory - Supplies a pointer to the root directory path point for
        this process. Processes cannot go farther up in the directory hierarchy
        than this without a link. A reference will be added to the path entry
        and mount point of this path point.

    WorkingDirectory - Supplies a pointer to the path point to use for the
        working directory. A reference will be added to the path entry and
        mount point of this path point.

    SharedMemoryDirectory - Supplies a pointer to the path point to use as the
        shared memory object root. A reference will be added to the path entry
        and mount point of this path point.

Return Value:

    Returns a pointer to the new process, or NULL if the process could not be
    created.

--*/

PKPROCESS
PspGetProcessById (
    PROCESS_ID ProcessId
    );

/*++

Routine Description:

    This routine returns the process with the given process ID, and increments
    the reference count on the process returned.

Arguments:

    ProcessId - Supplies the process ID to search for.

Return Value:

    Returns a pointer to the process with the corresponding ID. The reference
    count will be increased by one.

    NULL if no such process could be found.

--*/

PKPROCESS
PspGetChildProcessById (
    PKPROCESS Parent,
    PROCESS_ID ProcessId
    );

/*++

Routine Description:

    This routine returns the child process with the given process ID, and
    increments the reference count on the process returned.

Arguments:

    Parent - Supplies a pointer to the parent process whose children are
        searched.

    ProcessId - Supplies the child process ID to search for.

Return Value:

    Returns a pointer to the child process with the corresponding ID. The
    reference count will be increased by one.

    NULL if no such process could be found.

--*/

VOID
PspWaitOnStopEvent (
    PKPROCESS Process,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine waits on the stop event and potentially services any tracer
    requests.

Arguments:

    Process - Supplies a pointer to the current process.

    TrapFrame - Supplies a pointer to the user mode trap frame.

Return Value:

    None.

--*/

BOOL
PspSetProcessExitStatus (
    PKPROCESS Process,
    USHORT ExitReason,
    UINTN ExitStatus
    );

/*++

Routine Description:

    This routine sets the process exit status and flags if they are not already
    set.

Arguments:

    Process - Supplies a pointer to the process that is exiting.

    ExitReason - Supplies the reason code for the child exit.

    ExitStatus - Supplies the exit status to set.

Return Value:

    TRUE if the values were set in the process.

    FALSE if an exit status was already set in the process.

--*/

KSTATUS
PspGetProcessList (
    PKPROCESS **Array,
    PULONG ArraySize
    );

/*++

Routine Description:

    This routine returns an array of pointers to all the processes in the
    system. This array may be incomplete if additional processes come in while
    the array is being created.

Arguments:

    Array - Supplies a pointer where an array of pointers to processes will be
        returned. Each process in the array will have its reference count
        incremented. The caller is responsible for releasing the references
        and freeing the array from non-paged pool.

    ArraySize - Supplies a pointer where the number of elements in the array
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

VOID
PspDestroyProcessList (
    PKPROCESS *Array,
    ULONG ArrayCount
    );

/*++

Routine Description:

    This routine destroys a process array, releasing the reference on each
    process and freeing the array from non-paged pool.

Arguments:

    Array - Supplies an array of pointers to processes to destroy.

    ArrayCount - Supplies the number of elements in the array.

Return Value:

    None.

--*/

KSTATUS
PspGetProcessIdList (
    PPROCESS_ID Array,
    PUINTN ArraySize
    );

/*++

Routine Description:

    This routine fills in the given array with process IDs from the currently
    running processes.

Arguments:

    Array - Supplies a pointer to an array where process IDs will be stored on
        success.

    ArraySize - Supplies a pointer where on input will contain the size of the
        supplied array. On output, it will return the actual size of the array
        needed.

Return Value:

    Status code.

--*/

VOID
PspProcessTermination (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine is called when the final thread in a process terminates.

Arguments:

    Process - Supplies a pointer to the process to gut.

Return Value:

    None.

--*/

VOID
PspGetThreadResourceUsage (
    PKTHREAD Thread,
    PRESOURCE_USAGE Usage
    );

/*++

Routine Description:

    This routine returns resource usage information for the given thread.

Arguments:

    Thread - Supplies a pointer to the thread to get usage information for.

    Usage - Supplies a pointer where the usage information is returned.

Return Value:

    None.

--*/

VOID
PspAddResourceUsages (
    PRESOURCE_USAGE Destination,
    PRESOURCE_USAGE Add
    );

/*++

Routine Description:

    This routine adds two resource usage structures together, returning the
    result in the destination. This routine assumes neither structure is going
    to change mid-copy.

Arguments:

    Destination - Supplies a pointer to the first structure to add, and the
        destination for the sum.

    Add - Supplies a pointer to the second structure to add.

Return Value:

    None. The sum of each structure element is returned in the destination.

--*/

VOID
PspRemoveProcessFromLists (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine removes the given process from its parent's list of children
    and from the global list of processes.

Arguments:

    Process - Supplies a pointer to the process to be removed.

Return Value:

    None.

--*/

KSTATUS
PspGetProcessIdentity (
    PKPROCESS Process,
    PTHREAD_IDENTITY Identity
    );

/*++

Routine Description:

    This routine gets the identity of the process, which is simply that of
    an arbitrary thread in the process.

Arguments:

    Process - Supplies a pointer to the process to get an identity for.

    Identity - Supplies a pointer where the process identity will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if the given process ID does not correspond to any
    known process.

--*/

VOID
PspInitializeProcessStartData (
    PPROCESS_START_DATA StartData,
    PLOADED_IMAGE OsBaseLibrary,
    PLOADED_IMAGE Executable,
    PLOADED_IMAGE Interpreter
    );

/*++

Routine Description:

    This routine initializes a process start data structure.

Arguments:

    StartData - Supplies a pointer to the start data structure to initialize.

    OsBaseLibrary - Supplies a pointer to the OS base library, loaded into
        every address space.

    Executable - Supplies a pointer to the executable image.

    Interpreter - Supplies an optional pointer to the interpreter image.

Return Value:

    None.

--*/

INTN
PspRestorePreSignalTrapFrame (
    PTRAP_FRAME TrapFrame,
    PSIGNAL_CONTEXT UserContext
    );

/*++

Routine Description:

    This routine restores the original user mode thread context for the thread
    before a signal was invoked.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame from this system call.

    UserContext - Supplies the user context to restore.

Return Value:

    Returns the architecture-specific return register from the thread context.

--*/

VOID
PspArchRestartSystemCall (
    PTRAP_FRAME TrapFrame,
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine determines whether or not a system call needs to be restarted.
    If so, it modifies the given trap frame such that the system call return
    to user mode will fall right back into calling the system call.

Arguments:

    TrapFrame - Supplies a pointer to the full trap frame saved by a system
        call in order to attempt dispatching a signal.

    SystemCallNumber - Supplies the number of the system call that is
        attempting to dispatch a pending signal. Supplied SystemCallInvalid if
        the caller is not a system call.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call that is attempting to dispatch a signal. Supply NULL if
        the caller is not a system call.

Return Value:

    None.

--*/

VOID
PspPrepareThreadForFirstRun (
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame,
    BOOL ParameterIsStack
    );

/*++

Routine Description:

    This routine performs any architecture specific initialization to prepare a
    thread for being context swapped for the first time.

Arguments:

    Thread - Supplies a pointer to the thread being prepared for its first run.

    TrapFrame - Supplies an optional pointer for the thread to restore on its
        first run.

    ParameterIsStack - Supplies a boolean indicating whether the thread
        parameter is the value that should be used as the initial stack pointer.

Return Value:

    None.

--*/

INTN
PspArchResetThreadContext (
    PKTHREAD Thread,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine sets up the given trap frame as if the user mode portion of
    the thread was running for the first time.

Arguments:

    Thread - Supplies a pointer to the thread being reset.

    TrapFrame - Supplies a pointer where the initial thread's trap frame will
        be returned.

Return Value:

    The value that the thread should return when exiting back to user mode.

--*/

KSTATUS
PspArchCloneThread (
    PKTHREAD OldThread,
    PKTHREAD NewThread
    );

/*++

Routine Description:

    This routine performs architecture specific operations upon cloning a
    thread.

Arguments:

    OldThread - Supplies a pointer to the thread being copied.

    NewThread - Supplies a pointer to the newly created thread.

Return Value:

    Status code.

--*/

KSTATUS
PspArchGetDebugBreakInformation (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine gets the current debug break information.

Arguments:

    TrapFrame - Supplies a pointer to the user mode trap frame that caused the
        break.

Return Value:

    Status code.

--*/

KSTATUS
PspArchSetDebugBreakInformation (
    PTRAP_FRAME TrapFrame
    );

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

KSTATUS
PspArchSetOrClearSingleStep (
    PTRAP_FRAME TrapFrame,
    BOOL Set
    );

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

KSTATUS
PspCancelQueuedSignal (
    PKPROCESS Process,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    );

/*++

Routine Description:

    This routine attempts to cancel a queued signal. This only works in
    specific circumstances where it's known that the signal queue entry cannot
    be freed or queued to a different process during this time.

Arguments:

    Process - Supplies a pointer to the process the signal is on.

    SignalQueueEntry - Supplies a pointer to the entry to attempt to remove.

Return Value:

    STATUS_SUCCESS if the signal was successfully removed. The completion
    routine will be run in this case.

    STATUS_TOO_LATE if the signal is already in service or was previously
    serviced.

--*/

ULONG
PspDequeuePendingSignal (
    PSIGNAL_PARAMETERS SignalParameters,
    PTRAP_FRAME TrapFrame,
    PSIGNAL_SET BlockedSignalsOverride
    );

/*++

Routine Description:

    This routine gets and clears the first signal in the thread or process
    signal mask of the current thread. For stop or terminate signals, this
    routine will act on the signal.

Arguments:

    SignalParameters - Supplies a pointer to a caller-allocated structure where
        the signal parameter information might get returned.

    TrapFrame - Supplies a pointer to the user mode trap that got execution
        into kernel mode.

    BlockedSignalsOverride - Supplies an optional pointer to a set of signals
        that replaces the blocked signal set during this dequeue.

Return Value:

    Returns the signal number of the first pending signal.

    -1 if no signals are pending or a signal is already in progress.

--*/

BOOL
PspQueueChildSignalToParent (
    PKPROCESS Process,
    UINTN ExitStatus,
    USHORT Reason
    );

/*++

Routine Description:

    This routine queues the child signal the given process' parent, indicating
    the process has terminated, stopped, or continued.

Arguments:

    Process - Supplies a pointer to the child process that just exited, stopped,
        or continued.

    ExitStatus - Supplies the exit status on graceful exits or the signal number
        that caused the termination.

    Reason - Supplies the reason for the child signal.

Return Value:

    Returns TRUE if the signal was queued to the parent or FALSE otherwise.

--*/

BOOL
PspSignalAttemptDefaultProcessing (
    ULONG Signal
    );

/*++

Routine Description:

    This routine check to see if a signal is marked to be ignored or provide
    the default action, and if so perfoms those actions.

Arguments:

    Signal - Supplies the pending signal number.

Return Value:

    TRUE if the signal was handled by this routine and there's no need to go
    to user mode with it.

    FALSE if this routine did not handle the signal and it should be dealt with
    in user mode.

--*/

VOID
PspCleanupThreadSignals (
    VOID
    );

/*++

Routine Description:

    This routine cleans up the current thread's signal state, potentially
    waking up other threads if it was on the hook for handling a signal. This
    should only be called during thread termination in the context of the thead
    whose signal state needs to be cleaned up.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
PspInitializeProcessGroupSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for process groups.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
PspJoinProcessGroup (
    PKPROCESS Process,
    PROCESS_GROUP_ID ProcessGroupId,
    BOOL NewSession
    );

/*++

Routine Description:

    This routine creates a new process group and places the given process in it.

Arguments:

    Process - Supplies a pointer to the process to move to the new process
        group. Again, a process only gets to play this trick once; to play it
        again it needs to fork.

    ProcessGroupId - Supplies the identifier of the process group to join or
        create.

    NewSession - Supplies a boolean indicating whether to also join a new
        session or not.

Return Value:

    Status code.

--*/

VOID
PspAddProcessToParentProcessGroup (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine adds the given new process to its parent's process group. The
    caller cannot have any of the process locks held.

Arguments:

    Process - Supplies a pointer to the new process.

Return Value:

    None.

--*/

VOID
PspRemoveProcessFromProcessGroup (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine removes a dying process from its process group, potentially
    orphaning its childrens' process groups. The process lock should not be
    held by the caller.

Arguments:

    Process - Supplies a pointer to the process that's being destroyed.

Return Value:

    None.

--*/

VOID
PspDestroyProcessTimers (
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine cleans up any timers a process may have. This routine assumes
    the process lock is already held.

Arguments:

    Process - Supplies a pointer to the process.

Return Value:

    None.

--*/

VOID
PspPerformExecutePermissionChanges (
    PIO_HANDLE ExecutableHandle
    );

/*++

Routine Description:

    This routine fixes up the user identity and potentially permissions in
    preparation for executing an image.

Arguments:

    ExecutableHandle - Supplies an open file handle to the executable image.

Return Value:

    None.

--*/

KSTATUS
PspCopyThreadCredentials (
    PKTHREAD NewThread,
    PKTHREAD ThreadToCopy
    );

/*++

Routine Description:

    This routine copies the credentials of a thread onto a new yet-to-be-run
    thread.

Arguments:

    NewThread - Supplies a pointer to the new thread to initialize.

    ThreadToCopy - Supplies a pointer to the thread to copy identity and
        permissions from.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

--*/

VOID
PspDestroyCredentials (
    PKTHREAD Thread
    );

/*++

Routine Description:

    This routine destroys credentials associated with a dying thread.

Arguments:

    Thread - Supplies a pointer to the thread being terminated.

Return Value:

    None.

--*/

VOID
PspInitializeUserLocking (
    VOID
    );

/*++

Routine Description:

    This routine sets up the user locking subsystem.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
PspUserLockWake (
    PSYSTEM_CALL_USER_LOCK Parameters
    );

/*++

Routine Description:

    This routine wakes up those blocked on the given user mode address.

Arguments:

    Parameters - Supplies a pointer to the wait parameters.

Return Value:

    Status code.

--*/

//
// UTS realm functions
//

KSTATUS
PspInitializeUtsRealm (
    PKPROCESS KernelProcess
    );

/*++

Routine Description:

    This routine initializes the UTS realm space as the kernel process is
    coming online.

Arguments:

    KernelProcess - Supplies a pointer to the kernel process.

Return Value:

    Status code.

--*/

PUTS_REALM
PspCreateUtsRealm (
    PUTS_REALM Source
    );

/*++

Routine Description:

    This routine creates a new UTS realm.

Arguments:

    Source - Supplies a pointer to the realm to copy from.

Return Value:

    Returns a pointer to a new realm with a single reference on success.

    NULL on allocation failure.

--*/

KSTATUS
PspGetSetUtsInformation (
    BOOL FromKernelMode,
    PS_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets process informaiton.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

VOID
PspUtsRealmAddReference (
    PUTS_REALM Realm
    );

/*++

Routine Description:

    This routine adds a reference to the given UTS realm.

Arguments:

    Realm - Supplies a pointer to the realm.

Return Value:

    None.

--*/

VOID
PspUtsRealmReleaseReference (
    PUTS_REALM Realm
    );

/*++

Routine Description:

    This routine releases a reference to the given UTS realm. If the reference
    count drops to zero, the realm will be destroyed.

Arguments:

    Realm - Supplies a pointer to the realm.

Return Value:

    None.

--*/
