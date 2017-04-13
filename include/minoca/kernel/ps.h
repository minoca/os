/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ps.h

Abstract:

    This header contains definitions for the kernel process and thread library.

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
// This macro creates a mask from the given permission.
//

#define PERMISSION_TO_MASK(_Permission) (1ULL << (_Permission))

//
// This macro adds a permission to a permission set.
//

#define PERMISSION_ADD(_PermissionSet, _Permission) \
    ((_PermissionSet) |= PERMISSION_TO_MASK(_Permission))

//
// This macro removes a permission from a permission set.
//

#define PERMISSION_REMOVE(_PermissionSet, _Permission) \
    ((_PermissionSet) &= ~PERMISSION_TO_MASK(_Permission))

//
// This macro evaluates to non-zero if the given permission is in the given
// set.
//

#define PERMISSION_CHECK(_PermissionSet, _Permission) \
    (((_PermissionSet) & PERMISSION_TO_MASK(_Permission)) != 0)

//
// This macro ORs two permission sets together, writing the result to the
// first set.
//

#define PERMISSION_OR(_DestinationSet, _SetToOr) \
    ((_DestinationSet) |= (_SetToOr))

//
// This macro ANDs two permission sets together, writing the result to the
// first set.
//

#define PERMISSION_AND(_DestinationSet, _SetToAnd) \
    ((_DestinationSet) &= (_SetToAnd))

//
// This macro removes all signals in the second set from the first set.
//

#define PERMISSION_REMOVE_SET(_DestinationSet, _SetToRemove) \
    ((_DestinationSet) &= ~(_SetToRemove))

//
// This macro evaluates to non-zero if the given permission set is the empty
// set.
//

#define PERMISSION_IS_EMPTY(_Set) ((_Set) == PERMISSION_SET_EMPTY)

//
// This macro dispatches pending signals on the given thread if there are any.
//

#define PsDispatchPendingSignals(_Thread, _TrapFrame)     \
    ((_Thread)->SignalPending == ThreadNoSignalPending) ? \
    FALSE :                                               \
    PsDispatchPendingSignalsOnCurrentThread(_TrapFrame, SystemCallInvalid, NULL)

//
// This macro performs a quick inline check to see if any of the runtime timers
// are armed, and only then calls the real check function.
//

#define PsCheckRuntimeTimers(_Thread)                                   \
    (((_Thread)->UserTimer.DueTime | (_Thread)->ProfileTimer.DueTime) ? \
     PsEvaluateRuntimeTimers((_Thread)) : 0)

//
// These macros acquire the lock protecting the loaded image list.
//

#define PsAcquireImageListLock(_Process) \
    KeAcquireQueuedLock((_Process)->QueuedLock)

#define PsReleaseImageListLock(_Process) \
    KeReleaseQueuedLock((_Process)->QueuedLock)

//
// This macro evaluates to non-zero if the given process is a session leader.
//

#define PsIsSessionLeader(_Process) \
    ((_Process)->Identifiers.SessionId == (_Process)->Identifiers.ProcessId)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define kernel command line information for the process/thread library.
//

#define PS_KERNEL_ARGUMENT_COMPONENT "ps"
#define PS_KERNEL_ARGUMENT_ENVIRONMENT "env"

//
// Define the standard superuser ID.
//

#define USER_ID_ROOT 0

//
// Define the arbitrary maximum number of supplementary groups a user can
// belong to. Also define a minimum size (just for the allocations) to avoid
// making blocks with silly sizes.
//

#define SUPPLEMENTARY_GROUP_MAX 128
#define SUPPLEMENTARY_GROUP_MIN 8

//
// Define privileged permission bit indices.
//

//
// This permission overrides the restrictions associated with changing file
// ownership and group ownership.
//

#define PERMISSION_CHOWN 0

//
// This permission overrides all discretionary access control.
//

#define PERMISSION_FILE_ACCESS 1

//
// This permission overrides access control restrictions regarding reading
// and searching directories, and reading files.
//

#define PERMISSION_READ_SEARCH 2

//
// This permission overrides restrictions on file operations that assert that
// the file owner must be equal to the user ID, except where the set ID
// permission is applicable.
//

#define PERMISSION_FILE_OWNER 3

//
// This permission overrides restrictions that would otherwise prevent the
// caller from setting the set-user-ID and set-group-ID bits on a file. The
// restrictions are normally that the effective user ID shall match the file
// owner, and that the effective group ID (or a supplementary group ID) shall
// match the file owner ID when setting the set-group-ID bit.
//

#define PERMISSION_FILE_SET_ID 4

//
// This permission overrides the restriction that the real or effective user ID
// of a process sending a signal must match the real or effective user ID of
// the process receiving the signal.
//

#define PERMISSION_KILL 5

//
// This permission allows the thread to change its group IDs and supplementary
// group IDs.
//

#define PERMISSION_SET_GROUP_ID 6

//
// This permission allows the thread to change its user IDs.
//

#define PERMISSION_SET_USER_ID 7

//
// This permission allows any permission within the thread's limit to be added
// to the inheritable set, allows removal of bits from the limit, and allows
// modification of the permission behavior bits.
//

#define PERMISSION_SET_PERMISSIONS 8

//
// This permission allows binding to TCP and UDP ports below 1024.
//

#define PERMISSION_NET_BIND 9

//
// This permission allows broadcasting and listening to multicasts.
//

#define PERMISSION_NET_BROADCAST 10

//
// This permission allows general network administration, including:
//   * Interface configuration.
//   * Setting debug options on sockets.
//   * Modifying routing tables.
//   * Setting arbitraty process and process group ownership on sockets.
//   * Binding to any address for transparent proxying.
//   * Setting promiscuous mode.
//   * Clearing statistics.
//   * Multicasting.
//

#define PERMISSION_NET_ADMINISTRATOR 11

//
// This permission allows the use of raw sockets.
//

#define PERMISSION_NET_RAW 12

//
// This permission allows locking of memory map segments.
//

#define PERMISSION_LOCK_MEMORY 13

//
// This permission allows loading and unloading of kernel drivers.
//

#define PERMISSION_DRIVER_LOAD 14

//
// This permission allows changing the root directory.
//

#define PERMISSION_CHROOT 15

//
// This permission allows escaping a changed root.
//

#define PERMISSION_ESCAPE_CHROOT 16

//
// This permission allows debugging of other processes.
//

#define PERMISSION_DEBUG 17

//
// This permission allows system-wide administration, including:
//   * Setting the host name
//   * Configuring paging
//   * Configuring storage devices
//   * Open any master terminal.
//   * Send any process ID in SCM_CREDENTIALS socket control messages.
//

#define PERMISSION_SYSTEM_ADMINISTRATOR 18

//
// This permission allows system shutdown and reboot.
//

#define PERMISSION_REBOOT 19

//
// This permission allows raising the thread's priority, manipulating other
// process' priorities, and adjusting the scheduling algorithms.
//

#define PERMISSION_SCHEDULING 20

//
// This permission allows setting resource and quota limits. It also allows
// changing of the system clock frequency.
//

#define PERMISSION_RESOURCES 21

//
// This permission allows manipulation of the system time and time zone.
//

#define PERMISSION_TIME 22

//
// This permission allows preventing system sleep.
//

#define PERMISSION_PREVENT_SLEEP 23

//
// This permission allows creating timers that will wake the system.
//

#define PERMISSION_WAKE 24

//
// This permission allows mounting and unmounting.
//

#define PERMISSION_MOUNT 25

//
// This permission allows arbitrary control over IPC objects.
//

#define PERMISSION_IPC 26

//
// Define the mask of valid permission. It must be kept up to date if new
// permissions are added.
//

#define PERMISSION_MAX PERMISSION_IPC
#define PERMISSION_MASK (PERMISSION_TO_MASK(PERMISSION_MAX + 1) - 1)

//
// Define some standard permission set values.
//

#define PERMISSION_SET_EMPTY 0ULL
#define PERMISSION_SET_FULL PERMISSION_MASK

//
// Define permission behavior flags.
//

//
// Set this flag to allow a thread that has one or more root (0) user IDs to
// retain its permissions when it switches all of its user IDs to non-zero
// values. Without this bit, the thread loses all its permissions on such a
// change. This flag is always cleared on an exec call.
//

#define PERMISSION_BEHAVIOR_KEEP_PERMISSIONS        0x00000001
#define PERMISSION_BEHAVIOR_KEEP_PERMISSIONS_LOCKED 0x00010000

//
// Set this flag to prevent the kernel from adjusting permission sets when the
// thread's effective user ID is switched between zero and non-zero values.
//

#define PERMISSION_BEHAVIOR_NO_SETUID_FIXUP         0x00000002
#define PERMISSION_BEHAVIOR_NO_SETUID_FIXUP_LOCKED  0x00020000

//
// Set this flag to prevent the kernel from granting capabilities when a
// set-user-ID root program is executed, or when a process with an effective or
// real user ID of root calls exec.
//

#define PERMISSION_BEHAVIOR_NO_ROOT                 0x00000004
#define PERMISSION_BEHAVIOR_NO_ROOT_LOCKED          0x00040000

#define PERMISSION_BEHAVIOR_VALID_MASK              \
    (PERMISSION_BEHAVIOR_KEEP_PERMISSIONS |         \
     PERMISSION_BEHAVIOR_KEEP_PERMISSIONS_LOCKED |  \
     PERMISSION_BEHAVIOR_NO_SETUID_FIXUP |          \
     PERMISSION_BEHAVIOR_NO_SETUID_FIXUP_LOCKED |   \
     PERMISSION_BEHAVIOR_NO_ROOT |                  \
     PERMISSION_BEHAVIOR_NO_ROOT_LOCKED)

//
// Define thread identity fields that can be set.
//

#define THREAD_IDENTITY_FIELD_REAL_USER_ID          0x00000001
#define THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID     0x00000002
#define THREAD_IDENTITY_FIELD_SAVED_USER_ID         0x00000004
#define THREAD_IDENTITY_FIELD_REAL_GROUP_ID         0x00000008
#define THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID    0x00000010
#define THREAD_IDENTITY_FIELD_SAVED_GROUP_ID        0x00000020

#define THREAD_IDENTITY_FIELDS_USER             \
    (THREAD_IDENTITY_FIELD_REAL_USER_ID |       \
     THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID |  \
     THREAD_IDENTITY_FIELD_SAVED_USER_ID)

#define THREAD_IDENTITY_FIELDS_GROUP            \
    (THREAD_IDENTITY_FIELD_REAL_GROUP_ID |      \
     THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID | \
     THREAD_IDENTITY_FIELD_SAVED_GROUP_ID)

//
// Define thread permission fields that can be set.
//

#define THREAD_PERMISSION_FIELD_BEHAVIOR    0x00000001
#define THREAD_PERMISSION_FIELD_LIMIT       0x00000002
#define THREAD_PERMISSION_FIELD_PERMITTED   0x00000004
#define THREAD_PERMISSION_FIELD_INHERITABLE 0x00000008
#define THREAD_PERMISSION_FIELD_EFFECTIVE   0x00000010

//
// Define thread flags.
//

#define THREAD_FLAG_USER_MODE       0x0001
#define THREAD_FLAG_IN_SYSTEM_CALL  0x0002
#define THREAD_FLAG_FREE_USER_STACK 0x0004
#define THREAD_FLAG_EXITING         0x0008
#define THREAD_FLAG_RESTORE_SIGNALS 0x0010

//
// Define thread FPU flags.
//

#define THREAD_FPU_FLAG_IN_USE      0x0001
#define THREAD_FPU_FLAG_OWNER       0x0002

//
// Define the set of thread flags that can be specified on creation. This is
// also the set of flags that will propagate when a thread is copied.
//

#define THREAD_FLAG_CREATION_MASK (THREAD_FLAG_USER_MODE)

#define DEFAULT_USER_STACK_SIZE (8 * _1MB)
#define DEFAULT_KERNEL_STACK_SIZE 0x3000
#define DEFAULT_KERNEL_STACK_SIZE_ALIGNMENT 0x4000
#define STACK_ALIGNMENT 16
#define MAX_PROCESS_NAME 255
#define PS_ALLOCATION_TAG 0x636F7250 // 'corP'
#define PS_ACCOUNTANT_ALLOCATION_TAG 0x63417350 // 'cAsP'
#define PS_FPU_CONTEXT_ALLOCATION_TAG 0x46637250 // 'FcrP'
#define PS_IMAGE_ALLOCATION_TAG 0x6D497350 // 'mIsP'
#define PS_GROUP_ALLOCATION_TAG 0x70477350 // 'pGsP'
#define PS_UTS_ALLOCATION_TAG 0x74557350 // 'tUsP'

#define PROCESS_INFORMATION_VERSION 1

#define PROCESS_DEBUG_MODULE_CHANGE_VERSION 1

//
// Define the set of process flags.
//

#define PROCESS_FLAG_EXECUTED_IMAGE 0x000000001

//
// Define the user lock operation flags and masks.
//

//
// This mask defines the bits reserved for the operation code.
//

#define USER_LOCK_OPERATION_MASK 0x0000007F

//
// Set this bit if the lock is private to the process, which results in
// slightly faster accesses.
//

#define USER_LOCK_PRIVATE 0x00000080

//
// Define the current version of the process start data structure.
//

#define PROCESS_START_DATA_VERSION 2

//
// Define the number of random bytes of data supplied to new processes.
//

#define PROCESS_START_DATA_RANDOM_SIZE 16

//
// Define the infinite resource limit value.
//

#define RESOURCE_LIMIT_INFINITE MAX_UINTN

//
// Define the largest valid user mode address.
//

#define MAX_USER_ADDRESS ((PVOID)0x7FFFFFFF)

//
// Define the maximum length of a UTS name.
//

#define UTS_NAME_MAX 80

//
// Define flags to the fork call.
//

//
// Set this flag to have the child process fork into an independent UTS
// realm (which stores the host and domain name).
//

#define FORK_FLAG_REALM_UTS 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PROCESS_STATE {
    ProcessStateInvalid,
    ProcessStateReady,
    ProcessStateRunning,
    ProcessStateBlocked,
    ProcessStateSuspended,
    ProcessStateExited,
    MaxProcessStates
} PROCESS_STATE, *PPROCESS_STATE;

typedef enum _THREAD_STATE {
    ThreadStateInvalid,
    ThreadStateFirstTime,
    ThreadStateReady,
    ThreadStateRunning,
    ThreadStateBlocking,
    ThreadStateBlocked,
    ThreadStateWaking,
    ThreadStateSuspending,
    ThreadStateSuspended,
    ThreadStateExited,
    MaxThreadStates
} THREAD_STATE, *PTHREAD_STATE;

typedef enum _DEBUG_COMMAND_TYPE {
    DebugCommandInvalid,
    DebugCommandEnableDebugging,
    DebugCommandPrint,
    DebugCommandReportModuleChange,
    DebugCommandContinue,
    DebugCommandReadMemory,
    DebugCommandWriteMemory,
    DebugCommandSwitchThread,
    DebugCommandGetBreakInformation,
    DebugCommandSetBreakInformation,
    DebugCommandGetSignalInformation,
    DebugCommandSetSignalInformation,
    DebugCommandSingleStep,
    DebugCommandRangeStep,
    DebugCommandGetLoadedModules,
    DebugCommandGetThreadList,
} DEBUG_COMMAND_TYPE, *PDEBUG_COMMAND_TYPE;

typedef enum _THREAD_SIGNAL_PENDING_TYPE {
    ThreadNoSignalPending,
    ThreadChildSignalPending,
    ThreadSignalPending,
} THREAD_SIGNAL_PENDING_TYPE, *PTHREAD_SIGNAL_PENDING_TYPE;

typedef enum _PS_INFORMATION_TYPE {
    PsInformationInvalid,
    PsInformationProcess,
    PsInformationProcessIdList,
    PsInformationHostName,
    PsInformationDomainName,
} PS_INFORMATION_TYPE, *PPS_INFORMATION_TYPE;

typedef enum _PROCESS_ID_TYPE {
    ProcessIdInvalid,
    ProcessIdProcess,
    ProcessIdThread,
    ProcessIdParentProcess,
    ProcessIdProcessGroup,
    ProcessIdSession,
} PROCESS_ID_TYPE, *PPROCESS_ID_TYPE;

typedef enum _SCHEDULER_ENTRY_TYPE {
    SchedulerEntryInvalid,
    SchedulerEntryThread,
    SchedulerEntryGroup,
} SCHEDULER_ENTRY_TYPE, *PSCHEDULER_ENTRY_TYPE;

typedef enum _USER_LOCK_OPERATION {
    UserLockInvalid,
    UserLockWait,
    UserLockWake,
} USER_LOCK_OPERATION, *PUSER_LOCK_OPERATION;

//
// Define the different types of resource limits. These line up with the
// RLIMIT_* definitions.
// TODO: Implement ResourceLimitCore.
// TODO: Implement ResourceLimitCpuTime.
// TODO: Implement ResourceLimitData.
// TODO: Implement ResourceLimitFileSize.
// TODO: Implement ResourceLimitFileCount.
// TODO: Implement ResourceLimitAddressSpace.
// TODO: Implement ResourceLimitProcessCount.
// TODO: Implement ResourceLimitSignals.
// TODO: Implement ResourceLimitNice.
//

typedef enum _RESOURCE_LIMIT_TYPE {
    ResourceLimitCore,
    ResourceLimitCpuTime,
    ResourceLimitData,
    ResourceLimitFileSize,
    ResourceLimitFileCount,
    ResourceLimitStack,
    ResourceLimitAddressSpace,
    ResourceLimitProcessCount,
    ResourceLimitSignals,
    ResourceLimitNice,
    ResourceLimitCount
} RESOURCE_LIMIT_TYPE, *PRESOURCE_LIMIT_TYPE;

typedef LONG PROCESS_ID, *PPROCESS_ID;
typedef PROCESS_ID THREAD_ID, *PTHREAD_ID;
typedef PROCESS_ID PROCESS_GROUP_ID, *PPROCESS_GROUP_ID;
typedef PROCESS_ID SESSION_ID, *PSESSION_ID;
typedef struct _SIGNAL_QUEUE_ENTRY SIGNAL_QUEUE_ENTRY, *PSIGNAL_QUEUE_ENTRY;
typedef struct _KPROCESS KPROCESS, *PKPROCESS;
typedef struct _KTHREAD KTHREAD, *PKTHREAD;
typedef struct _SUPPLEMENTARY_GROUPS
    SUPPLEMENTARY_GROUPS, *PSUPPLEMENTARY_GROUPS;

typedef struct _UTS_REALM UTS_REALM, *PUTS_REALM;

//
// Define the user and group ID types.
//

typedef ULONG USER_ID, *PUSER_ID;
typedef ULONG GROUP_ID, *PGROUP_ID;
typedef ULONGLONG PERMISSION_SET, *PPERMISSION_SET;

/*++

Structure Description:

    This structure stores information about a group of processes that
    interact with their controlling terminal as a unit.

Members:

    ListEntry - Stores pointers to the next and previous process groups in the
        global list.

    ReferenceCount - Stores the number of outstanding references to this
        process group. This structure will be automatically destroyed when the
        reference count hits zero.

    Identifier - Stores the identifier for this process group, which is usually
        the identifier of the process that created the process group.

    ProcessListHead - Stores the head of the list of processes in the group.

    SessionId - Stores the session identifier the process group belongs to.

    OutsideParents - Stores the number of processes with living parents outside
        the process group. When the number of processes in a process group with
        parents outside the process group (but inside the session) drops to
        zero, the process is considered orphaned as there is no one to do job
        control on it.

--*/

typedef struct _PROCESS_GROUP {
    LIST_ENTRY ListEntry;
    volatile ULONG ReferenceCount;
    PROCESS_GROUP_ID Identifier;
    LIST_ENTRY ProcessListHead;
    SESSION_ID SessionId;
    ULONG OutsideParents;
} PROCESS_GROUP, *PPROCESS_GROUP;

/*++

Structure Description:

    This structure defines the current user and group identity of a thread.

Members:

    RealUserId - Stores the user identifier of the user that created the
        process.

    EffectiveUserId - Stores the user identifier actively used in file and
        permission checks.

    SavedUserId - Stores the real user ID if the process executed was not a
        set-UID program, or the user ID of the executable if the executable had
        the set-UID file permission set.

    RealGroupId - Stores the group identifier of the user that created the
        process.

    EffectiveGroupId - Stores the group ID actively used in file and permission
        checks.

    SavedGroupId - Stores the real group ID if the process executed was not a
        sed-GID program, or the group ID of the executable if the executable
        had the set-GID file permission set.

--*/

typedef struct _THREAD_IDENTITY {
    USER_ID RealUserId;
    USER_ID EffectiveUserId;
    USER_ID SavedUserId;
    GROUP_ID RealGroupId;
    GROUP_ID EffectiveGroupId;
    GROUP_ID SavedGroupId;
} THREAD_IDENTITY, *PTHREAD_IDENTITY;

/*++

Structure Description:

    This structure is passed from the kernel to a newly starting application,
    and contains useful values. This structure is provided as a mechanism to
    improve performance, as it obviates the need to make various system calls
    during program load.

Members:

    Version - Stores the structure version. This is set by the kernel to
        PROCESS_START_DATA_VERSION. Newer versions will be backwards compatible
        with older versions.

    PageSize - Stores the system page size.

    Identity - Stores the thread identity of the new process.

    Base - Stores the base address of the interpreter or executable.

    InterpreterBase - Stores the base address of the program interpreter, or
        0 if no intepreter was requested.

    OsLibraryBase - Stores the base address of the Minoca OS library, loaded
        into every address space.

    ExecutableBase - Stores the base address of the executable.

    EntryPoint - Stores the initial entry point, either into the interpreter
        if there was one or into the executable if not.

    StackBase - Stores the base of the stack.

    IgnoredSignals - Stores a mask of which signals are ignored.

--*/

typedef struct _PROCESS_START_DATA {
    ULONG Version;
    UINTN PageSize;
    THREAD_IDENTITY Identity;
    UCHAR Random[PROCESS_START_DATA_RANDOM_SIZE];
    PVOID InterpreterBase;
    PVOID OsLibraryBase;
    PVOID ExecutableBase;
    PVOID EntryPoint;
    PVOID StackBase;
    SIGNAL_SET IgnoredSignals;
} PROCESS_START_DATA, *PPROCESS_START_DATA;

/*++

Structure Description:

    This structure defines the initial environment for a process.

Members:

    ImageName - Stores a pointer to a string containing the path to the image
        being executed.

    ImageNameLength - Stores the length of the image name buffer, in bytes,
        including the null terminator.

    Arguments - Stores a pointer to an array of pointers to null terminated
        strings representing the arguments to be passed to the main function
        (in the argv parameter). The first argument will be the same as the
        image name pointer.

    ArgumentCount - Stores the element count of the arguments array (which
        can be passed directly as the argc parameter to the main function).

    ArgumentsBuffer - Stores a pointer to the buffer containing the image name
        and arguments strings.

    ArgumentsBufferLength - Stores the size of the arguments buffer, in bytes.

    Environment - Stores a pointer to an array of null terminated strings
        representing the execution environment of the process. This array can
        be assigned directly to the environ C library variable.

    EnvironmentCount - Stores the number of environment variable definitions.
        This does not include the null terminating entry.

    EnvironmentBuffer - Stores a pointer to the buffer used to hold environment
        variable strings.

    EnvironmentBufferLength - Stores the length of the environment buffer, in
        bytes.

    StartData - Stores a pointer to additional data used to help the program
        load faster. This is ignored when user mode is passing a process
        environment to the kernel.

--*/

typedef struct _PROCESS_ENVIRONMENT {
    PSTR ImageName;
    ULONG ImageNameLength;
    PSTR *Arguments;
    ULONG ArgumentCount;
    PVOID ArgumentsBuffer;
    ULONG ArgumentsBufferLength;
    PSTR *Environment;
    ULONG EnvironmentCount;
    PVOID EnvironmentBuffer;
    ULONG EnvironmentBufferLength;
    PPROCESS_START_DATA StartData;
} PROCESS_ENVIRONMENT, *PPROCESS_ENVIRONMENT;

typedef
VOID
(*PTHREAD_ENTRY_ROUTINE) (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine is the entry point prototype for a new thread.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread.

Return Value:

    None.

--*/

typedef
VOID
(*PSIGNAL_COMPLETION_ROUTINE) (
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    );

/*++

Routine Description:

    This routine is called when a queued signal was successfully completed
    in usermode. It is responsible for doing what it will with the signal queue
    entry memory, as the system is done with it.

Arguments:

    SignalQueueEntry - Supplies a pointer to the signal queue entry that was
        successfully sent to user mode.

Return Value:

    None.

--*/

typedef
BOOL
(*PPROCESS_ITERATOR_ROUTINE) (
    PVOID Context,
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine describes the prototype for the process list iterator. This
    routine is called with the process list lock held.

Arguments:

    Context - Supplies a pointer's worth of context passed into the iterate
        routine.

    Process - Supplies the process to examine.

Return Value:

    TRUE if the iteration should stop.

    FALSE if the iteration should continue.

--*/

/*++

Structure Description:

    This structure defines the signal information structure.

Members:

    ListEntry - Stores pointers to the next and previous signals in the process
        queue.

    Parameters - Stores the parameters of the signal to send.

    CompletionRoutine - Stores a pointer to a function that gets called by the
        system when the signal is successfully sent to user mode.

--*/

struct _SIGNAL_QUEUE_ENTRY {
    LIST_ENTRY ListEntry;
    SIGNAL_PARAMETERS Parameters;
    PSIGNAL_COMPLETION_ROUTINE CompletionRoutine;
};

/*++

Structure Description:

    This structure defines information about a process' paths.

Members:

    Root - Stores the root path point for this process.

    CurrentDirectory - Stores the current directory path point for this process.

    SharedMemoryDirectory - Stores the shared memory directory path point for
        this process.

    Lock - Stores a pointer to a queued lock synchronizing accesses with
        changes.

--*/

typedef struct _PROCESS_PATHS {
    struct {
        PVOID PathEntry;
        PVOID MountPoint;
    } Root;

    struct {
        PVOID PathEntry;
        PVOID MountPoint;
    } CurrentDirectory;

    struct {
        PVOID PathEntry;
        PVOID MountPoint;
    } SharedMemoryDirectory;

    PVOID Lock;
} PROCESS_PATHS, *PPROCESS_PATHS;

/*++

Structure Description:

    This structure defines the debug request structure.

Members:

    Command - Stores the debug command that the application would like to
        perform.

    PreviousCommand - Stores the previous debug command executed by the
        application.

    Address - Stores the address parameter of the command, used for commands
        like read and write.

    Thread - Stores the thread ID for the switch thread command.

    Data - Stores a pointer to a buffer containing either the data to write or
        the location to return the read data.

    Size - Stores the amount of data to be read or written. The data buffer
        must be at least that size.

    SignalToDeliver - Stores the signal number to deliver to the debugged
        process for step or continue operations. Supply zero to not deliver
        any signal.

    Status - Stores the result of the operation as returned by the kernel.

--*/

typedef struct _PROCESS_DEBUG_COMMAND {
    DEBUG_COMMAND_TYPE Command;
    DEBUG_COMMAND_TYPE PreviousCommand;
    union {
        PVOID Address;
        THREAD_ID Thread;
    } U;

    PVOID Data;
    ULONG Size;
    ULONG SignalToDeliver;
    KSTATUS Status;
} PROCESS_DEBUG_COMMAND, *PPROCESS_DEBUG_COMMAND;

/*++

Structure Description:

    This structure defines a breakpoint range for a usermode process.

Members:

    BreakRangeStart - Stores a pointer to the first byte of memory that
        qualifies as being in the break range.

    BreakRangeEnd - Stores a pointer to the first byte of memory that does not
        qualify as being in the break range.

    RangeHoleStart - Stores a pointer to the first byte within the range that
        does not generate a break (a "hole" in the break range).

    RangeHoleEnd - Stores a pointer to the first byte within the range that does
        not fall in the range hole (the first byte that again qualifies as
        within the break range).
--*/

typedef struct _PROCESS_DEBUG_BREAK_RANGE {
    PVOID BreakRangeStart;
    PVOID BreakRangeEnd;
    PVOID RangeHoleStart;
    PVOID RangeHoleEnd;
} PROCESS_DEBUG_BREAK_RANGE, *PPROCESS_DEBUG_BREAK_RANGE;

/*++

Structure Description:

    This structure defines a module change notification for the kernel from
    user mode.

Members:

    Version - Supplies the version of the module change notification structure.
        Set this to PROCESS_DEBUG_MODULE_CHANGE_VERSION.

    Load - Supplies a boolean indicating whether this is a module load (TRUE)
        or a module unload (FALSE).

    Image - Supplies a pointer to the module being loaded or unloaded.

    BinaryNameSize - Supplies the length of the binary name string, in bytes,
        including the null terminator.

--*/

typedef struct _PROCESS_DEBUG_MODULE_CHANGE {
    ULONG Version;
    BOOL Load;
    PLOADED_IMAGE Image;
    UINTN BinaryNameSize;
} PROCESS_DEBUG_MODULE_CHANGE, *PPROCESS_DEBUG_MODULE_CHANGE;

/*++

Structure Description:

    This structure defines fields used when a process is being debugged by
    another process.

Members:

    Process - Stores a pointer back to the process that owns this debug data.

    TracingProcess - Stores an optional pointer to the process that is
        tracing (debugging) this process. The tracing process will be notified
        and this process stopped on every signal and system call. The tracee
        does not have a reference on its tracing process. To freely use the
        tracer outside of the tracee process' queued lock, the tracee must
        acquire its lock, check to make sure the tracing process is not NULL,
        take a reference, and then release the lock. This synchronizes with the
        tracer's destruction, as it will acquire the tracee's process lock and
        NULL this pointer.

    TraceeListHead - Stores the list of processes this process traces.

    TracerListEntry - Stores pointers to the next and previous entries in the
        list of processes that also wait on the same tracer process as this one.

    TracerStopRequested - Stores a boolean indicating if the process wants to
        stop for a tracer event.

    TracerSignalInformation - Stores a pointer to the signal information of the
        signal that stopped this process. The tracer process may change this
        information.

    TracerLock - Stores a lock that serializes access to the tracer stop
        requested and tracer signal information members.

    AllStoppedEvent - Stores a pointer to the event that is signaled when all
        threads have responded to a stop request.

    DebugCommand - Stores a the current debug command.

    DebugCommandCompleteEvent - Stores a pointer to the event signaled by this
        process when the command is complete.

    DebugLeaderThread - Stores a pointer to the thread currently acting as the
        debug leader.

    DebugSingleStepAddress - Stores the address of the single step destination,
        or NULL if there is no single step address currently. This is only
        used on some architectures (like ARM).

    DebugSingleStepOriginalContents - Stores the original contents of the
        instruction stream at the given address. This is only used on some
        architectures (like ARM).

    BreakRange - Stores the range information for a range step, a single step
        that is looking to step over one or more instructions.

--*/

typedef struct _PROCESS_DEBUG_DATA {
    PKPROCESS Process;
    PKPROCESS TracingProcess;
    LIST_ENTRY TraceeListHead;
    LIST_ENTRY TracerListEntry;
    BOOL TracerStopRequested;
    SIGNAL_PARAMETERS TracerSignalInformation;
    KSPIN_LOCK TracerLock;
    PVOID AllStoppedEvent;
    PROCESS_DEBUG_COMMAND DebugCommand;
    PVOID DebugCommandCompleteEvent;
    PVOID DebugLeaderThread;
    PVOID DebugSingleStepAddress;
    ULONG DebugSingleStepOriginalContents;
    PROCESS_DEBUG_BREAK_RANGE BreakRange;
} PROCESS_DEBUG_DATA, *PPROCESS_DEBUG_DATA;

/*++

Structure Description:

    This structure maintains system resource usage information for a given
    process or thread.

Members:

    UserCycles - Stores the number of accumulated cycles in user mode.

    KernelCycles - Stores the number of accumulated cycles in kernel mode.

    Preemptions - Stores the number of times this thread or process has been
        forcibly descheduled.

    Yields - Stores the number of times this thread has voluntarily
        relinquished control to the scheduler.

    PageFaults - Stores the number of page faults that have occurred, including
        both page faults that required no I/O, and page faults that required
        I/O.

    HardPageFaults - Stores the number of hard page faults, which are page
        faults that ultimately generated I/O.

    BytesRead - Stores the number of bytes read from a device. Reads from
        volumes (that don't generate subsequent device reads) do not count here.

    BytesWritten - Stores the number of bytes written to a device. Writes to
        volumes do not count here.

    DeviceReads - Stores the count of device read operations that constituted
        the bytes written value.

    DeviceWrites - Stores the number of device write operations that
        constituted the bytes written value.

    MaxResidentSet - Store the maximum number of pages that have ever been
        mapped in the process address space. This is zero for threads, as
        threads share a single process address space.

--*/

typedef struct _RESOURCE_USAGE {
    ULONGLONG UserCycles;
    ULONGLONG KernelCycles;
    ULONGLONG Preemptions;
    ULONGLONG Yields;
    ULONGLONG PageFaults;
    ULONGLONG HardPageFaults;
    ULONGLONG BytesRead;
    ULONGLONG BytesWritten;
    ULONGLONG DeviceReads;
    ULONGLONG DeviceWrites;
    UINTN MaxResidentSet;
} RESOURCE_USAGE, *PRESOURCE_USAGE;

/*++

Structure Description:

    This structure stores the soft and hard values for a resource limit.

Members:

    Current - Stores the currently enforced limit, also known as the soft
        limit. Set to RESOURCE_LIMIT_INFINITY if no limit is in effect.

    Max - Stores the maximum value the currently enforced limit can be set to.
        Set to RESOURCE_LIMIT_INFINITY if no maximum limit is enforced.

--*/

typedef struct _RESOURCE_LIMIT {
    UINTN Current;
    UINTN Max;
} RESOURCE_LIMIT, *PRESOURCE_LIMIT;

/*++

Structure Description:

    This structure defines the set of IDs for a process.

Members:

    ProcessId - Stores the process' system-unique identifier.

    ParentProcessId - Stores the process identifier of the parent process.

    ProcessGroupId - Stores the process group identifier of this process.

    SessionId - Stores the process group session identifier of this process.

--*/

typedef struct _PROCESS_IDENTIFIERS {
    PROCESS_ID ProcessId;
    PROCESS_ID ParentProcessId;
    PROCESS_GROUP_ID ProcessGroupId;
    SESSION_ID SessionId;
} PROCESS_IDENTIFIERS, *PPROCESS_IDENTIFIERS;

/*++

Structure Description:

    This structure defines the set of realms a process can belong to.

Members:

    Uts - Stores a pointer to the UTS realm.

--*/

typedef struct _PROCESS_REALMS {
    PUTS_REALM Uts;
} PROCESS_REALMS, *PPROCESS_REALMS;

/*++

Structure Description:

    This structure defines system or user process.

Members:

    Header - Stores the object header.

    BinaryName - Stores a pointer to the name of the binary that created this
        process.

    BinaryNameSize - Stores the size of the binary name buffer in bytes,
        including the null terminator.

    Flags - Stores a bitmask of flags protected by the process queued lock.
        See PROCESS_FLAG_* for definitions.

    QueuedLock - Stores a pointer to a queued lock protecting simultaneous
        access to this structure. A (potentially partial) list of things
        protected by this lock:
        * Thread's supplementary groups
        * Process group membership
        * Child process list
        * Environment and binary name
        * Process parent pointer and ID
        * Signal masks and function pointers
        * Exit status
        * Debug data creation
        * Debug data tracer list and tracing process pointer
        * Loaded Image list
        * Thread list
        * Resource usage
        * Timer list

    ListEntry - Stores pointers to the next and previous processes in the
        system.

    ProcessGroupListEntry - Stores pointers to the next and previous processes
        in the process group.

    ThreadListHead - Stores the head of a list of KTHREAD structures whose
        list entries are the member ProcessEntry.

    SiblingListEntry - Stores pointers to the next and previous processes that
        share the parent process.

    ChildListHead - Stores the list of child processes that inherit from this
        process.

    Parent - Stores a pointer to the parent process if it is still alive. A
        child does not have a reference on the parent. To freely use the
        parent outside of its queued lock, a child must acquire the lock, check
        to see if the parent is not NULL, take a reference, and then release
        the lock. This synchronizes with the parent's destruction, as the
        parent will acquire the child's lock and NULL the parent point.

    ThreadCount - Stores the number of threads that belong to this process.

    Identifiers - Stores the ID information for this process, including the
        process ID, process group ID, and session ID.

    ProcessGroup - Stores a pointer directly to the process group this process
        belongs to.

    HandleTable - Stores a pointer to the handle table for this process.

    Paths - Stores the path root information for this process.

    Environment - Stores a pointer to the kernel mode copy of the process
        environment.

    ImageCount - Stores the number of loaded image elements in the image list.

    ImageListHead - Stores the head of list of images loaded for this process.

    ImageListSignature - Stores the sum of all the timestamps and loaded
        lowest addresses of the loaded images. The debugger uses this as a
        heuristic to determine if its list of loaded modules is in sync.

    ImageListQueuedLock - Stores a pointer to a queued lock protecting the
        image list.

    PendingSignals - Stores a bitfield of signals pending for the process as
        a whole.

    IgnoredSignals - Stores a bitfield of signals that the user has marked
        as ignored.

    HandledSignals - Stores a bitfield of signals that have a handler
        installed. If no handler is installed, the default action is performed.

    SignalHandlerRoutine - Stores a pointer to the user mode signal handling
        routine.

    SignalListHead - Stores the head of the list of signals that are currently
        queued for the process. The type of elements on this list will be
        SIGNAL_QUEUE_ENTRY structures. This list is protected by the process
        lock.

    UnreapedChildList - Stores the head of the list of child signal entries.
        that have not yet been waited for.

    ChildSignal - Stores the required memory needed for this process to send a
        child signal to the parent and/or tracer process on exits, stops, and
        continues.

    ChildSignalDestination - Stores the destination process where the child
        signal is currently queued.

    ChildSignalLock - Stores the spin lock serializing access to the child
        signal structure.

    ExitStatus - Stores the exit status of the process. This is either the
        code the process exited with or the signal that terminated the process.

    ExitReason - Stores the exit reason. See CHILD_SIGNAL_REASON_* definitions.

    DebugData - Stores a pointer to the debug data if this process is being
        debugged.

    StopEvent - Stores a pointer to the event that stopped threads wait on to
        continue.

    StoppedThreadCount - Stores the number of threads currently waiting on the
        stopped event.

    TimerList - Stores the list of the currently active timers.

    StartTime - Stores the process start time as a system time.

    ResourceUsage - Stores the resource usage for this process. This is
        protected by the queued lock.

    ChildResourceUsage - Stores the resource usage for terminated and waited
        for children of this process. This is protected by the queued lock.

    Umask - Stores the user file creation permission bit mask for the process.

    ControllingTerminal - Stores an opaque pointer to the process' controlling
        terminal. This is a file object, but it's a file object that this
        process doesn't necessarily have a reference to. This pointer should
        not be touched without the terminal list lock held.

    Realm - Stores the set of realms the process belongs to.

--*/

struct _KPROCESS {
    OBJECT_HEADER Header;
    PCSTR BinaryName;
    ULONG BinaryNameSize;
    ULONG Flags;
    PVOID QueuedLock;
    LIST_ENTRY ListEntry;
    LIST_ENTRY ProcessGroupListEntry;
    LIST_ENTRY ThreadListHead;
    LIST_ENTRY SiblingListEntry;
    LIST_ENTRY ChildListHead;
    PKPROCESS Parent;
    ULONG ThreadCount;
    PROCESS_IDENTIFIERS Identifiers;
    PPROCESS_GROUP ProcessGroup;
    PADDRESS_SPACE AddressSpace;
    PHANDLE_TABLE HandleTable;
    PROCESS_PATHS Paths;
    PPROCESS_ENVIRONMENT Environment;
    ULONG ImageCount;
    LIST_ENTRY ImageListHead;
    ULONGLONG ImageListSignature;
    PVOID ImageListQueuedLock;
    SIGNAL_SET PendingSignals;
    SIGNAL_SET IgnoredSignals;
    SIGNAL_SET HandledSignals;
    PVOID SignalHandlerRoutine;
    LIST_ENTRY SignalListHead;
    LIST_ENTRY UnreapedChildList;
    SIGNAL_QUEUE_ENTRY ChildSignal;
    PKPROCESS ChildSignalDestination;
    KSPIN_LOCK ChildSignalLock;
    UINTN ExitStatus;
    USHORT ExitReason;
    PPROCESS_DEBUG_DATA DebugData;
    PVOID StopEvent;
    volatile ULONG StoppedThreadCount;
    LIST_ENTRY TimerList;
    ULONGLONG StartTime;
    RESOURCE_USAGE ResourceUsage;
    RESOURCE_USAGE ChildResourceUsage;
    ULONG Umask;
    PVOID ControllingTerminal;
    PROCESS_REALMS Realm;
};

/*++

Structure Description:

    This structure defines the current set of privileged permissions afforded
    to a thread.

Members:

    Behavior - Stores the bitfield of flags governing the behavior of the
        permission sets when the user ID is manipulated. See the
        PERMISSION_BEHAVIOR_* definitions.

    Limit - Stores the maximum set of permissions this thread and its
        descendents can have, regardless of superuser or file attributes.
        Removing bits from this set is a way of deflating the power of the
        superuser.

    Permitted - Stores the set of permissions that the thread is allowed to
        assume. It is also the maximum set that may be added to the thread's
        inheritable set for a thread that does not have permission to set
        additional permissions.

    Inheritable - Stores the set of permissions preserved across an exec call.

    Effective - Stores the set of the thread's current permissions, used in
        all permission checks.

--*/

typedef struct _THREAD_PERMISSIONS {
    ULONG Behavior;
    PERMISSION_SET Limit;
    PERMISSION_SET Permitted;
    PERMISSION_SET Inheritable;
    PERMISSION_SET Effective;
} THREAD_PERMISSIONS, *PTHREAD_PERMISSIONS;

/*++

Structure Description:

    This structure defines a chunk of supplementary group IDs for a thread.

Members:

    Capacity - Stores the number of elements in this array allocation.

    Count - Stores the number of valid elements in this array allocation.

    Groups - Stores a pointer to the array of supplementary groups this thread
        belongs to.

    Next - Stores a pointer to the next chunk of supplementary group IDs.

--*/

struct _SUPPLEMENTARY_GROUPS {
    ULONG Capacity;
    ULONG Count;
    PGROUP_ID Groups;
    PSUPPLEMENTARY_GROUPS Next;
};

/*++

Structure Description:

    This structure defines an entry within the scheduler. This may either be a
    thread or group.

Members:

    Type - Stores the entry type.

    Parent - Stores the parent group this entry belongs to.

    ListEntry - Stores pointers to the next and previous threads in the
        ready list.

--*/

typedef struct _SCHEDULER_ENTRY SCHEDULER_ENTRY, *PSCHEDULER_ENTRY;
struct _SCHEDULER_ENTRY {
    SCHEDULER_ENTRY_TYPE Type;
    PSCHEDULER_ENTRY Parent;
    LIST_ENTRY ListEntry;
};

/*++

Structure Description:

    This structure defines information about a timer that tracks CPU time.

Members:

    DueTime - Stores the due time in processor counter ticks.

    Period - Stores the interval in processor counter ticks.

--*/

typedef struct _RUNTIME_TIMER {
    ULONGLONG DueTime;
    ULONGLONG Period;
} RUNTIME_TIMER, *PRUNTIME_TIMER;

/*++

Structure Description:

    This structure defines system or user thread of execution.

Members:

    Header - Stores object manager information about this thread.

    OwningProcess - Stores a pointer to the process that owns this thread.

    KernelStackPointer - Stores a pointer to the current stack location in the
        kernel stack.

    ThreadPointer - Stores the architecture-specific encoding of the thread
        pointer. On x86 this is a GDT_ENTRY, on ARM this is just the pointer
        itself.

    State - Stores the executable state of the thread.

    SignalPending - Stores a value indicating if a signal is pending on this
        thread or perhaps on the process. The synchronization of this variable
        is this: the variable is only ever set to non-zero values with the
        process lock held, but can be reset to zero without the lock held.

    Flags - Stores the bitfield of flags the thread was created with. See
        THREAD_FLAG_* definitions.

    FpuFlags - Stores the bitfield of floating point unit flags governing the
        thread. This is separate from the flags member because it is altered
        during context switch, which can land on top of other kernel level flag
        manipulation.

    SchedulerEntry - Stores the scheduler information for this thread.

    BuiltinTimer - Stores a pointer to the thread's default timeout timer.

    BuiltinWaitBlock - Stores a pointer to the built-in wait block that comes
        with every thread.

    WaitBlock - Stores a pointer to the wait block this thread is currently
        blocking on.

    PendingSignals - Stores a bitfield of signals pending for the current
        thread.

    BlockedSignals - Stores a bitfield of signals that are blocked by the
        thread.

    RestoreSignals - Stores a bitfield of signals that are to be restored to
        the blocked signals set after a signal is dispatched.

    SignalListHead - Stores the head of the list of signals that are currently
        queued for the process. The type of elements on this list will be
        SIGNAL_QUEUE_ENTRY structures. This list is protected by the process
        lock.

    ResourceUsage - Stores the resource usage of the thread.

    FpuContext - Stores a pointer to the saved extended state if user mode
        is currently using the floating point unit.

    Identity - Stores the thread's user and group identity.

    SupplementaryGroups - Stores a pointer to the first block of the thread's
        supplementary groups.

    Permissions - Stores the thread's permission masks.

    ThreadRoutine - Stores the location to jump to on first execution of this
        thread.

    ThreadParameter - Stores the parameter given to the thread routine.

    KernelStack - Stores the base (limit) of the kernel stack for this thread.

    KernelStackSize - Stores the size, in bytes, of the kernel stack.

    UserStack - Stores the base (limit) of the user-mode stack for this thread.

    UserStackSize - Stores the size of the user-mode stack for this thread.

    ThreadId - Stores the thread's system-unique ID.

    ThreadIdPointer - Stores an optional pointer in user mode. If a thread is
        terminated, zero will be written to this value by the kernel and a
        UserLockWake operation will be performed to wake one thread waiting on
        that address. This is used so that threads can reliably wait for
        other threads termination.

    ProcessEntry - Stores pointers to the previous and next threads in the
        process that owns this thread.

    TrapFrame - Stores a pointer to the user mode trap frame saved for this
        thread by the system call handler.

    RealTimer - Stores an optional pointer to the real-time interval timer.

    UserTimer - Stores the per-thread timer that tracks user mode execution
        time.

    ProfileTimer - Stores the per-thread timer that tracks user plus kernel
        execution time.

    Limits - Stores the resource limits associated with the thread.

--*/

struct _KTHREAD {
    OBJECT_HEADER Header;
    PKPROCESS OwningProcess;
    PVOID KernelStackPointer;
    ULONGLONG ThreadPointer;
    volatile THREAD_STATE State;
    THREAD_SIGNAL_PENDING_TYPE SignalPending;
    USHORT Flags;
    USHORT FpuFlags;
    SCHEDULER_ENTRY SchedulerEntry;
    PVOID BuiltinTimer;
    PWAIT_BLOCK BuiltinWaitBlock;
    PWAIT_BLOCK WaitBlock;
    SIGNAL_SET PendingSignals;
    SIGNAL_SET BlockedSignals;
    SIGNAL_SET RestoreSignals;
    LIST_ENTRY SignalListHead;
    RESOURCE_USAGE ResourceUsage;
    PFPU_CONTEXT FpuContext;
    THREAD_IDENTITY Identity;
    PSUPPLEMENTARY_GROUPS SupplementaryGroups;
    THREAD_PERMISSIONS Permissions;
    PTHREAD_ENTRY_ROUTINE ThreadRoutine;
    PVOID ThreadParameter;
    PVOID KernelStack;
    ULONG KernelStackSize;
    PVOID UserStack;
    ULONG UserStackSize;
    THREAD_ID ThreadId;
    PTHREAD_ID ThreadIdPointer;
    PTRAP_FRAME TrapFrame;
    LIST_ENTRY ProcessEntry;
    PVOID RealTimer;
    RUNTIME_TIMER UserTimer;
    RUNTIME_TIMER ProfileTimer;
    RESOURCE_LIMIT Limits[ResourceLimitCount];
};

/*++

Structure Description:

    This structure defines information about an active process.

Members:

    Version - Stores the version of the structure. When querying process
        information, this should be set to PROCESS_INFORMATION_VERSION.

    StructureSize - Stores the size of the structure, in bytes, including the
        name and arguments buffers.

    ProcessId - Stores the process identifier.

    ParentProcessId - Stores the parent process's identifier.

    ProcessGroupId - Stores the process's group identifier.

    SessionId - Stores the process's session identifier.

    RealUserId - Stores the real user ID of the process owner.

    EffectiveUserId - Stores the effective user ID of the process owner.

    RealGroupId - Stores the real group ID of the process owner.

    EffectiveGroupId - Stores the effective group ID of the process owner.

    Priority - Stores the priority of the process.

    NiceValue - Stores the nice value of the process.

    Flags - Stores the process flags.

    State - Stores the process state.

    ImageSize - Stores the size, in bytes, of the process's main image.

    StartTime - Stores the process start time as a system time.

    ResourceUsage - Stores the resource usage of the process.

    ChildResourceUsage - Stores the resource usage of terminated and waited-for
        children of the process.

    Frequency - Stores the frequency of the CPU to be used in conjunction with
        the resource usage data for calculated time.

    NameOffset - Stores the offset, in bytes, to the start of the process name.
        The offset is from the beginning of this structure.

    NameLength - Stores the length of the process name, in characters,
        including the null terminator.

    ArgumentsBufferOffset - Stores the offset, in bytes, to the start of the
        image name and argument strings. The offset is from the beginning of
        this structure.

    ArgumentsBufferSize - Stores the size of the arguments buffer in bytes.

--*/

typedef struct _PROCESS_INFORMATION {
    ULONG Version;
    ULONG StructureSize;
    PROCESS_ID ProcessId;
    PROCESS_ID ParentProcessId;
    PROCESS_GROUP_ID ProcessGroupId;
    SESSION_ID SessionId;
    USER_ID RealUserId;
    USER_ID EffectiveUserId;
    GROUP_ID RealGroupId;
    GROUP_ID EffectiveGroupId;
    ULONG Priority;
    ULONG NiceValue;
    ULONG Flags;
    PROCESS_STATE State;
    UINTN ImageSize;
    ULONGLONG StartTime;
    RESOURCE_USAGE ResourceUsage;
    RESOURCE_USAGE ChildResourceUsage;
    ULONGLONG Frequency;
    UINTN NameOffset;
    ULONG NameLength;
    UINTN ArgumentsBufferOffset;
    ULONG ArgumentsBufferSize;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION;

/*++

Structure Description:

    This structure defines information about an active thread.

Members:

    StructureSize - Stores the size of the structure plus the null terminated
        name.

    ThreadId - Stores the thread identifier.

    ResourceUsage - Stores the resource usage information for the thread.

    Name - Stores the null terminated name of the thread.

--*/

typedef struct _THREAD_INFORMATION {
    ULONG StructureSize;
    THREAD_ID ThreadId;
    RESOURCE_USAGE ResourceUsage;
    CHAR Name[ANYSIZE_ARRAY];
} THREAD_INFORMATION, *PTHREAD_INFORMATION;

/*++

Structure Description:

    This structure defines a request to set (or get) the current thread's
    identity.

Members:

    FieldsToSet - Stores a bitfield of which fields should be changed. See
        THREAD_IDENTITY_FIELD_* definitions. Supply 0 to simply get the current
        thread's identity.

    Identity - Stores the thread identity to set on input. On output,
        contains the new complete thread identity.

--*/

typedef struct _SET_THREAD_IDENTITY {
    ULONG FieldsToSet;
    THREAD_IDENTITY Identity;
} SET_THREAD_IDENTITY, *PSET_THREAD_IDENTITY;

/*++

Structure Description:

    This structure defines a request to set (or get) the current thread's
    permissions.

Members:

    FieldsToSet - Stores a bitfield of which fields should be changed. See
        THREAD_PERMISSION_FIELD_* definitions. Supply 0 to simply get the
        current thread's permission masks.

    Permissions - Stores the thread permissions to set on input. On output,
        contains the new complete thread permission set.

--*/

typedef struct _SET_THREAD_PERMISSIONS {
    ULONG FieldsToSet;
    THREAD_PERMISSIONS Permissions;
} SET_THREAD_PERMISSIONS, *PSET_THREAD_PERMISSIONS;

/*++

Structure Description:

    This structure defines the parameters passed to create a new thread.

Members:

    Process - Stores an optional pointer to the process to create the thread
        in. Supply NULL to create the thread in the current process.

    Name - Stores a pointer to a kernel mode buffer containing the name of
        the thread.

    NameSize - Stores the size of the name buffer in bytes, including the
        null terminator.

    ThreadRoutine - Stores a pointer to the routine to call for the thread.

    Parameter - Stores a pointer's worth of context to pass to the thread
        routine.

    UserStack - Stores an optional pointer to the user stack base to use.

    StackSize - Stores an optional stack size, either the user stack size for a
        user mode thread or a kernel stack size for a kernel mode thread.
        Supply 0 to use a default value.

    Flags - Stores thread flags. See THREAD_FLAG_* definitions.

    ThreadPointer - Stores the value to set as the thread pointer.

    ThreadIdPointer - Stores a pointer where the thread ID will be returned. If
        this is a user mode address, this will also be set as the thread ID
        pointer value for the thread.

    Environment - Stores an optional kernel mode pointer to an environment to
        copy over to the stack of the created thread. This is only used when
        creating a process directly from kernel mode.

--*/

typedef struct _THREAD_CREATION_PARAMETERS {
    PKPROCESS Process;
    PCSTR Name;
    UINTN NameSize;
    PTHREAD_ENTRY_ROUTINE ThreadRoutine;
    PVOID Parameter;
    PVOID UserStack;
    ULONG StackSize;
    ULONG Flags;
    PVOID ThreadPointer;
    PTHREAD_ID ThreadIdPointer;
    PPROCESS_ENVIRONMENT Environment;
} THREAD_CREATION_PARAMETERS, *PTHREAD_CREATION_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
KSTATUS
PsCreateKernelThread (
    PTHREAD_ENTRY_ROUTINE ThreadRoutine,
    PVOID ThreadParameter,
    PCSTR Name
    );

/*++

Routine Description:

    This routine creates and launches a new kernel thread with default
    parameters.

Arguments:

    ThreadRoutine - Supplies the entry point to the thread.

    ThreadParameter - Supplies the parameter to pass to the entry point
        routine.

    Name - Supplies an optional name to identify the thread.

Return Value:

    Returns a pointer to the new thread on success, or NULL on failure.

--*/

KERNEL_API
KSTATUS
PsCreateThread (
    PTHREAD_CREATION_PARAMETERS Parameters
    );

/*++

Routine Description:

    This routine creates and initializes a new thread, and adds it to the
    ready list for execution.

Arguments:

    Parameters - Supplies a pointer to the thread creation parameters.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
PsCheckPermission (
    ULONG Permission
    );

/*++

Routine Description:

    This routine checks to see if the calling thread currently has the given
    permission.

Arguments:

    Permission - Supplies the permission number to check. See PERMISSION_*
        definitions.

Return Value:

    STATUS_SUCCESS if the current thread has the given permission.

    STATUS_PERMISSION_DENIED if the thread does not have the given permission.

--*/

BOOL
PsIsUserInGroup (
    GROUP_ID Group
    );

/*++

Routine Description:

    This routine determines if the given group ID matches the effective
    group ID or any of the supplementary group IDs of the calling thread. The
    current thread must not be a kernel thread.

Arguments:

    Group - Supplies the group ID to check against.

Return Value:

    TRUE if the calling thread is a member of the given group.

    FALSE if the calling thread is not a member of the given group.

--*/

KSTATUS
PsGetThreadList (
    PROCESS_ID ProcessId,
    ULONG AllocationTag,
    PVOID *Buffer,
    PULONG BufferSize
    );

/*++

Routine Description:

    This routine returns information about the active processes in the system.

Arguments:

    ProcessId - Supplies the identifier of the process to get thread
        information for.

    AllocationTag - Supplies the allocation tag to use for the allocation
        this routine will make on behalf of the caller.

    Buffer - Supplies a pointer where a non-paged pool buffer will be returned
        containing the array of thread information. The caller is responsible
        for freeing this memory from non-paged pool. The type returned here
        will be an array (where each element may be a different size) of
        THREAD_INFORMATION structures.

    BufferSize - Supplies a pointer where the size of the buffer in bytes
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    information buffer.

    STATUS_BUFFER_TOO_SMALL if the thread list is so volatile that it cannot
    be sized. This is only returned in extremely rare cases, as this routine
    makes multiple attempts.

--*/

KSTATUS
PsGetThreadInformation (
    PROCESS_ID ProcessId,
    THREAD_ID ThreadId,
    PTHREAD_INFORMATION Buffer,
    PULONG BufferSize
    );

/*++

Routine Description:

    This routine returns information about a given thread.

Arguments:

    ProcessId - Supplies the process ID owning the thread.

    ThreadId - Supplies the ID of the thread to get information about.

    Buffer - Supplies an optional pointer to a buffer to write the data into.
        This must be non-paged memory if the thread requested belongs to the
        kernel process.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the size needed to contain the data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if no process with the given identifier exists.

    STATUS_NO_SUCH_THREAD if the no thread with the given identifier exists.

    STATUS_BUFFER_TOO_SMALL if a buffer was supplied but was not big enough to
    contain all the information.

--*/

VOID
PsSetSignalMask (
    PSIGNAL_SET NewMask,
    PSIGNAL_SET OriginalMask
    );

/*++

Routine Description:

    This routine sets the blocked signal mask for the current thread.

Arguments:

    NewMask - Supplies a pointer to the new mask to set.

    OriginalMask - Supplies an optional pointer to the previous mask.

Return Value:

    None.

--*/

INTN
PsSysCreateThread (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine creates a new thread for the current process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysExitThread (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine terminates the current thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    Does not return. Eventually exits by killing the thread.

--*/

INTN
PsSysSetThreadPointer (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine sets the thread pointer for the current thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This is supplies the thread pointer directly, which is
        passed from user mode via a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetThreadIdPointer (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine sets the thread ID pointer for the current thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This is supplies the thread ID pointer directly, which
        is passed from user mode via a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetSignalHandler (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine sets the user mode signal handler for the given thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysRestoreContext (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine restores the original user mode thread context for the thread
    before the signal was invoked.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. The parameter is a user mode pointer to the signal
        context to restore.

Return Value:

    Returns the architecture-specific return register from the thread context.
    The architecture-specific system call assembly routines do not restore the
    return register out of the trap frame in order to allow a system call to
    return a value via a register. The restore context system call, however,
    must restore the old context, including the return register.

--*/

INTN
PsSysSendSignal (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call that allows usermode processes and
    threads to send signals to one another.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetSignalBehavior (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call that allows a thread to set its
    varios signal behavior masks.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysWaitForChildProcess (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call that suspends the current thread
    until a child process exits.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSuspendExecution (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call that suspends the current thread
    until a signal comes in.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysQueryTimeCounter (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for getting the current time
    counter value.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysTimerControl (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine performs timer control operations.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetITimer (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine performs gets or sets a thread interval timer.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetThreadIdentity (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the get/set thread identity system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetThreadPermissions (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the get/set thread permissions system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetSupplementaryGroups (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the get/set supplementary groups system call.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetResourceLimit (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call that gets or sets a resource limit
    for the current thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysUserLock (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for user mode locking.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

VOID
PsEvaluateRuntimeTimers (
    PKTHREAD Thread
    );

/*++

Routine Description:

    This routine checks the runtime timers for expiration on the current thread.

Arguments:

    Thread - Supplies a pointer to the current thread.

Return Value:

    None.

--*/

VOID
PsSignalThread (
    PKTHREAD Thread,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry,
    BOOL Force
    );

/*++

Routine Description:

    This routine sends a signal to the given thread.

Arguments:

    Thread - Supplies a pointer to the thread to send the signal to.

    SignalNumber - Supplies the signal number to send.

    SignalQueueEntry - Supplies an optional pointer to a queue entry to place
        on the thread's queue.

    Force - Supplies a boolean that if set indicates the thread cannot block
        or ignore this signal.

Return Value:

    None.

--*/

VOID
PsSignalProcess (
    PKPROCESS Process,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    );

/*++

Routine Description:

    This routine sends a signal to the given process.

Arguments:

    Process - Supplies a pointer to the process to send the signal to.

    SignalNumber - Supplies the signal number to send.

    SignalQueueEntry - Supplies an optional pointer to a queue entry to place
        on the process' queue.

Return Value:

    None.

--*/

KSTATUS
PsSignalProcessId (
    PROCESS_ID ProcessId,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    );

/*++

Routine Description:

    This routine sends a signal to the given process.

Arguments:

    ProcessId - Supplies the identifier of the process to send the signal to.

    SignalNumber - Supplies the signal number to send.

    SignalQueueEntry - Supplies an optional pointer to a queue entry to place
        on the process' queue.

Return Value:

    None.

--*/

KSTATUS
PsSignalAllProcesses (
    BOOL FromKernel,
    ULONG SignalNumber,
    PSIGNAL_QUEUE_ENTRY QueueEntry
    );

/*++

Routine Description:

    This routine sends a signal to every process currently in the system
    (except the kernel process). Processes created during the execution of this
    call may not receive the signal. This routine is used mainly during system
    shutdown.

Arguments:

    FromKernel - Supplies a boolean indicating whether the origin of the signal
        is the the kernel or not. Permissions are not checked if the origin
        is the kernel.

    SignalNumber - Supplies the signal number to send.

    QueueEntry - Supplies an optional pointer to the queue structure to send.
        A copy of this memory will be made in paged pool for each process a
        signal is sent to.

Return Value:

    STATUS_SUCCESS if some processes were signaled.

    STATUS_PERMISSION_DENIED if the caller did not have permission to signal
        some of the processes.

    STATUS_INSUFFICIENT_RESOURCES if there was not enough memory to enumerate
    all the processes in the system.

--*/

BOOL
PsIsThreadAcceptingSignal (
    PKTHREAD Thread,
    ULONG SignalNumber
    );

/*++

Routine Description:

    This routine determines if the given thread is currently accepting a given
    signal, or if it is being either blocked or ignored.

Arguments:

    Thread - Supplies a pointer to the process to query. If NULL is supplied
        the current thread will be used.

    SignalNumber - Supplies the signal number to check.

Return Value:

    TRUE if the process has the signal action set to either default or a
    handler.

    FALSE if the signal is currently blocked or ignored.

--*/

VOID
PsDefaultSignalCompletionRoutine (
    PSIGNAL_QUEUE_ENTRY SignalQueueEntry
    );

/*++

Routine Description:

    This routine implements the default signal completion routine, which
    simply frees the signal queue entry from paged pool. The caller should
    not touch the signal queue entry after this routine has returned, as it's
    gone back to the pool.

Arguments:

    SignalQueueEntry - Supplies a pointer to the signal queue entry that just
        completed.

Return Value:

    None.

--*/

BOOL
PsDispatchPendingSignalsOnCurrentThread (
    PTRAP_FRAME TrapFrame,
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine dispatches any pending signals that should be run on the
    current thread.

Arguments:

    TrapFrame - Supplies a pointer to the current trap frame. If this trap frame
        is not destined for user mode, this function exits immediately.

    SystemCallNumber - Supplies the number of the system call that is
        attempting to dispatch a pending signal. Supply SystemCallInvalid if
        the caller is not a system call.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call that is attempting to dispatch a signal. Supply NULL if
        the caller is not a system call.

Return Value:

    FALSE if no signals are pending.

    TRUE if a signal was applied.

--*/

ULONG
PsDequeuePendingSignal (
    PSIGNAL_PARAMETERS SignalParameters,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine dequeues the first signal in the thread or process signal mask
    of the current thread that is not handled by any default processing.

Arguments:

    SignalParameters - Supplies a pointer to a caller-allocated structure where
        the signal parameter information might get returned.

    TrapFrame - Supplies a pointer to the current trap frame. If this trap frame
        is not destined for user mode, this function exits immediately.

Return Value:

    Returns a signal number if a signal was queued.

    -1 if no signal was dispatched.

--*/

VOID
PsApplySynchronousSignal (
    PTRAP_FRAME TrapFrame,
    PSIGNAL_PARAMETERS SignalParameters,
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine applies the given signal onto the current thread. It is
    required that no signal is already in progress, nor will any other signals
    be applied for the duration of the system call.

Arguments:

    TrapFrame - Supplies a pointer to the current trap frame. This trap frame
        must be destined for user mode.

    SignalParameters - Supplies a pointer to the signal information to apply.

    SystemCallNumber - Supplies the number of the system call that is
        attempting to dispatch a pending signal. Supply SystemCallInvalid if
        the caller is not a system call.

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call that is attempting to dispatch a signal. Supply NULL if
        the caller is not a system call.

Return Value:

    None.

--*/

VOID
PsVolumeArrival (
    PCSTR VolumeName,
    ULONG VolumeNameLength,
    BOOL SystemVolume
    );

/*++

Routine Description:

    This routine implements actions that the process library takes in response
    to a new volume arrival.

Arguments:

    VolumeName - Supplies the full path to the new volume.

    VolumeNameLength - Supplies the length of the volume name buffer, including
        the null terminator, in bytes.

    SystemVolume - Supplies a boolean indicating whether or not this is the
        system volume.

Return Value:

    None.

--*/

VOID
PsGetProcessGroup (
    PKPROCESS Process,
    PPROCESS_GROUP_ID ProcessGroupId,
    PSESSION_ID SessionId
    );

/*++

Routine Description:

    This routine returns the process group and session ID for the given process.

Arguments:

    Process - Supplies a pointer to the process whose process group and session
        IDs are desired. Supply NULL and the current process will be used.

    ProcessGroupId - Supplies an optional pointer where the process group ID of
        the requested process will be returned.

    SessionId - Supplies an optional pointer where the session ID of the
        requested process will be returned.

Return Value:

    None.

--*/

BOOL
PsIsProcessGroupOrphaned (
    PROCESS_GROUP_ID ProcessGroupId
    );

/*++

Routine Description:

    This routine determines if a process group is orphaned.

Arguments:

    ProcessGroupId - Supplies the ID of the process group to query.

Return Value:

    TRUE if the process group is orphaned or does not exist.

    FALSE if the process group has at least one parent within the session but
    outside the process group.

--*/

BOOL
PsIsProcessGroupInSession (
    PROCESS_GROUP_ID ProcessGroupId,
    SESSION_ID SessionId
    );

/*++

Routine Description:

    This routine determines whether or not the given process group belongs to
    the given session.

Arguments:

    ProcessGroupId - Supplies the ID of the process group to be tested.

    SessionId - Supplies the ID of the session to be searched for the given
        process group.

Return Value:

    Returns TRUE if the process group is in the given session.

--*/

KSTATUS
PsSignalProcessGroup (
    PROCESS_GROUP_ID ProcessGroupId,
    ULONG SignalNumber
    );

/*++

Routine Description:

    This routine sends a signal to every process in the given process group.

Arguments:

    ProcessGroupId - Supplies the ID of the process group to send the signal to.

    SignalNumber - Supplies the signal to send to each process in the group.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if there was no such process group.

--*/

KSTATUS
PsGetAllProcessInformation (
    ULONG AllocationTag,
    PVOID *Buffer,
    PUINTN BufferSize
    );

/*++

Routine Description:

    This routine returns information about the active processes in the system.

Arguments:

    AllocationTag - Supplies the allocation tag to use for the allocation
        this routine will make on behalf of the caller.

    Buffer - Supplies a pointer where a non-paged pool buffer will be returned
        containing the array of process information. The caller is responsible
        for freeing this memory from non-paged pool. The type returned here
        will be an array (where each element may be a different size) of
        PROCESS_INFORMATION structures.

    BufferSize - Supplies a pointer where the size of the buffer in bytes
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    information buffer.

    STATUS_BUFFER_TOO_SMALL if the process list is so volatile that it cannot
    be sized. This is only returned in extremely rare cases, as this routine
    makes multiple attempts.

--*/

KSTATUS
PsGetProcessInformation (
    PROCESS_ID ProcessId,
    PPROCESS_INFORMATION Buffer,
    PUINTN BufferSize
    );

/*++

Routine Description:

    This routine returns information about a given process.

Arguments:

    ProcessId - Supplies the ID of the process to get the information for.

    Buffer - Supplies an optional pointer to a buffer to write the data into.
        This buffer must be non-paged.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the size needed to contain the data.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if the given process ID does not correspond to any
    known process.

    STATUS_BUFFER_TOO_SMALL if a buffer was supplied but was not big enough to
    contain all the information.

--*/

KSTATUS
PsGetProcessIdentity (
    PROCESS_ID ProcessId,
    PTHREAD_IDENTITY Identity
    );

/*++

Routine Description:

    This routine gets the identity of the process, which is simply that of
    an arbitrary thread in the process.

Arguments:

    ProcessId - Supplies the ID of the process to get the information for.

    Identity - Supplies a pointer where the process identity will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_PROCESS if the given process ID does not correspond to any
    known process.

--*/

INTN
PsSysForkProcess (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine duplicates the current process, including all allocated
    address space and open file handles. Only the current thread's execution
    continues in the new process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    Process ID of the child on success (a positive integer).

    Error status code on failure (a negative integer).

--*/

INTN
PsSysExecuteImage (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine duplicates the current process, including all allocated
    address space and open file handles. Only the current thread's execution
    continues in the new process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    The architecture-specific return register from the reset thread context on
    success. This is necessary because the architecture-specific system call
    assembly routines do not restore the return register out of the trap frame
    in order to allow a system call to return a value via a register. If an
    architecture does not need to pass anything to the new thread in its return
    register, then it should return 0.

    Error status code on failure.

--*/

INTN
PsSysGetSetProcessId (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine gets or sets identifiers associated with the calling process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or the requested process ID on success.

    Error status code on failure.

--*/

INTN
PsSysDebug (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the user mode debug interface.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysExitProcess (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine terminates the current process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This stores the exit status for the process. It is
        passed to the kernel in a register.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysGetResourceUsage (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine returns the resource usage for a process or thread.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
PsSysSetUmask (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine sets the file permission mask for the current process.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

PKPROCESS
PsCreateProcess (
    PCSTR CommandLine,
    ULONG CommandLineSize,
    PVOID RootDirectoryPathPoint,
    PVOID WorkingDirectoryPathPoint,
    PVOID SharedMemoryDirectoryPathPoint
    );

/*++

Routine Description:

    This routine creates a new process and executes the given binary image.
    This routine must be called at low level.

Arguments:

    CommandLine - Supplies a pointer to a string containing the command line to
        invoke (the executable and any arguments).

    CommandLineSize - Supplies the size of the command line in bytes, including
        the null terminator.

    RootDirectoryPathPoint - Supplies an optional pointer to the path point of
        the root directory to set for the new process.

    WorkingDirectoryPathPoint - Supplies an optional pointer to the path point
        of the working directory to set for the process.

    SharedMemoryDirectoryPathPoint - Supplies an optional pointer to the path
        point of the shared memory object directory to set for the process.

Return Value:

    Returns a pointer to the new process, or NULL if the process could not be
    created. This process will contain a reference that the caller must
    explicitly release.

--*/

PKPROCESS
PsGetCurrentProcess (
    VOID
    );

/*++

Routine Description:

    This routine returns the currently running process.

Arguments:

    None.

Return Value:

    Returns a pointer to the current process.

--*/

PKPROCESS
PsGetKernelProcess (
    VOID
    );

/*++

Routine Description:

    This routine returns a pointer to the system process.

Arguments:

    None.

Return Value:

    Returns a pointer to the system process.

--*/

ULONG
PsGetProcessCount (
    VOID
    );

/*++

Routine Description:

    This routine returns the number of active processes in the system. This
    count includes the kernel process (and therefore is never zero). This
    information is stale as soon as it is received, and as such is only useful
    in limited scenarios.

Arguments:

    None.

Return Value:

    Returns the number of active processes in the system.

--*/

VOID
PsIterateProcess (
    PROCESS_ID_TYPE Type,
    PROCESS_ID Match,
    PPROCESS_ITERATOR_ROUTINE IteratorFunction,
    PVOID Context
    );

/*++

Routine Description:

    This routine iterates over all processes in the process ID list.

Arguments:

    Type - Supplies the type of identifier to match on. Valid values are
        process ID, process group, or session.

    Match - Supplies the process, process group, or session ID to match on.
        Supply -1 to iterate over all processes.

    IteratorFunction - Supplies a pointer to the function to call for each
        matching process.

    Context - Supplies an opaque pointer that will be passed into the iterator
        function on each iteration.

Return Value:

    None.

--*/

VOID
PsHandleUserModeFault (
    PVOID VirtualAddress,
    ULONG FaultFlags,
    PTRAP_FRAME TrapFrame,
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine handles a user mode fault where no image section seems to back
    the faulting address or a write attempt was made to a read-only image
    section.

Arguments:

    VirtualAddress - Supplies the virtual address that caused the fault.

    FaultFlags - Supplies the fault information.

    TrapFrame - Supplies a pointer to the trap frame.

    Process - Supplies the process that caused the fault.

Return Value:

    None.

--*/

KSTATUS
PsIncreaseAllThreadIrpCount (
    ULONG OldPagingIrpCount,
    ULONG NewPagingIrpCount
    );

/*++

Routine Description:

    This routine allocates more IRPs for each thread to handle a new page file
    coming online whose stack requires additional IRPs to complete page file
    I/O.

Arguments:

    OldPagingIrpCount - Supplies the number of IRPs previously required to
        complete paging I/O.

    NewPagingIrpCount - Supplies the number of IRPs required to complete paging
        I/O.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if new IRPs could not be allocated.

--*/

VOID
PsQueueThreadCleanup (
    PKTHREAD Thread
    );

/*++

Routine Description:

    This routine queues the work item that cleans up a dead thread. This routine
    must not be executed by the thread being destroyed! This routine must be
    called at dispatch level.

Arguments:

    Thread - Supplies a pointer to the thread to clean up.

Return Value:

    None.

--*/

KSTATUS
PsCopyEnvironment (
    PPROCESS_ENVIRONMENT Source,
    PPROCESS_ENVIRONMENT *Destination,
    BOOL FromUserMode,
    PKTHREAD DestinationThread,
    PSTR OverrideImageName,
    UINTN OverrideImageNameSize
    );

/*++

Routine Description:

    This routine creates a copy of a pre-existing environment.

Arguments:

    Source - Supplies a pointer to the source environment to copy.

    Destination - Supplies a pointer where a pointer to the newly created
        environment will be returned.

    FromUserMode - Supplies a boolean indicating whether the environment exists
        in user mode or not.

    DestinationThread - Supplies an optional pointer to the user mode thread
        to copy the environment into. Supply NULL to copy the environment to
        a new kernel mode buffer.

    OverrideImageName - Supplies an optional pointer to an image name to use as
        an override of the image name in the source environment.

    OverrideImageNameSize - Supplies the size of the override image name,
        including the null terminator.

Return Value:

    Status code.

--*/

KSTATUS
PsCreateEnvironment (
    PCSTR CommandLine,
    ULONG CommandLineSize,
    PSTR *EnvironmentVariables,
    ULONG EnvironmentVariableCount,
    PPROCESS_ENVIRONMENT *NewEnvironment
    );

/*++

Routine Description:

    This routine creates a new environment based on a command line.

Arguments:

    CommandLine - Supplies a pointer to a string containing the command line,
        including the image and any arguments.

    CommandLineSize - Supplies the size of the command line buffer in bytes,
        including the null terminator character.

    EnvironmentVariables - Supplies an optional pointer to an array of
        environment variables in the form name=value.

    EnvironmentVariableCount - Supplies the number of valid entries in the
        environment variables array. For example, 1 if there is a single
        environment variable.

    NewEnvironment - Supplies a pointer where a pointer to the newly created
        environment will be returned on success. The caller is responsible
        for freeing this memory from paged pool.

Return Value:

    Status code.

--*/

VOID
PsDestroyEnvironment (
    PPROCESS_ENVIRONMENT Environment
    );

/*++

Routine Description:

    This routine destroys an environment and frees all resources associated with
    it. This routine can only be called on environments created in kernel space.

Arguments:

    Environment - Supplies a pointer to the environment to tear down.

Return Value:

    None.

--*/

KSTATUS
PsGetSetSystemInformation (
    BOOL FromKernelMode,
    PS_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets system information.

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

