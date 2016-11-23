/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ob.h

Abstract:

    This header contains definitions for the kernel Object Manager.

Author:

    Evan Green 4-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro signals the given object. The first parameter is a pointer to an
// object (which always begins with an OBJECT_HEADER), and the second parameter
// is a SIGNAL_OPTION. See the signal queue function.
//

#define ObSignalObject(_Object, _SignalOption) \
    ObSignalQueue(&(((POBJECT_HEADER)(_Object))->WaitQueue), (_SignalOption))

//
// This macro waits on the given object. The first parameter is a pointer to
// an object header. The other parameters follow the "wait on queue" function
// parameters.
//

#define ObWaitOnObject(_Object, _Flags, _TimeoutInMilliseconds) \
    ObWaitOnQueue(&(((POBJECT_HEADER)(_Object))->WaitQueue),    \
                  (_Flags),                                     \
                  (_TimeoutInMilliseconds))

//
// ---------------------------------------------------------------- Definitions
//

#define OBJECT_MANAGER_POOL_TAG 0x216A624F // '!jbO'

//
// This character represents the object directory separator.
//

#define OBJECT_PATH_SEPARATOR '/'

//
// Set this flag if the object manager should use the name parameter passed in
// directly as the object's name buffer rather than allocating a copy. This
// saves some memory for hardcoded strings.
//

#define OBJECT_FLAG_USE_NAME_DIRECTLY 0x00000001

//
// Set this flag if all queues must be signaled before the wait is satisfied.
//

#define WAIT_FLAG_ALL 0x00000001

//
// Set this flag if the wait can be interrupted by an asynchronous signal.
//

#define WAIT_FLAG_INTERRUPTIBLE 0x00000002

//
// Define the number of built in wait block entries.
//

#define BUILTIN_WAIT_BLOCK_ENTRY_COUNT 8

//
// Define a constant that can be passed to wait routines to indicate that the
// wait should never time out.
//

#define WAIT_TIME_INDEFINITE MAX_ULONG

//
// Define the bitmask of usable flags in each handle table entry.
//

#define HANDLE_FLAG_MASK 0x7FFFFFFF

//
// Define the maximum number of handles. This is fairly arbitrary, and it should
// be possible to raise this so long as it doesn't collide with INVALID_HANDLE.
//

#define OB_MAX_HANDLES 0x1000

typedef enum _OBJECT_TYPE {
    ObjectInvalid,
    ObjectDirectory,
    ObjectQueuedLock,
    ObjectEvent,
    ObjectProcess,
    ObjectThread,
    ObjectDriver,
    ObjectDevice,
    ObjectIrp,
    ObjectInterface,
    ObjectInterfaceInstance,
    ObjectInterfaceListener,
    ObjectVolume,
    ObjectImageSection,
    ObjectPipe,
    ObjectTimer,
    ObjectTerminalMaster,
    ObjectTerminalSlave,
    ObjectSharedMemoryObject,
    ObjectMaxTypes
} OBJECT_TYPE, *POBJECT_TYPE;

typedef enum _SIGNAL_STATE {
    InvalidSignalState,
    NotSignaledWithWaiters,
    NotSignaled,
    SignaledForOne,
    Signaled,
} SIGNAL_STATE, *PSIGNAL_STATE;

typedef enum _SIGNAL_OPTION {
    SignalOptionInvalid,
    SignalOptionSignalAll,
    SignalOptionSignalOne,
    SignalOptionPulse,
    SignalOptionUnsignal
} SIGNAL_OPTION, *PSIGNAL_OPTION;

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _HANDLE_TABLE HANDLE_TABLE, *PHANDLE_TABLE;

typedef
VOID
(*PDESTROY_OBJECT_ROUTINE) (
    PVOID Object
    );

/*++

Routine Description:

    This routine is called when an object's reference count drops to zero. It
    is responsible for cleaning up any auxiliary state inside the object. The
    object itself will be freed by the object manager.

Arguments:

    Object - Supplies a pointer to the object being destroyed.

Return Value:

    None.

--*/

typedef
VOID
(*PHANDLE_TABLE_ITERATE_ROUTINE) (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    ULONG Flags,
    PVOID HandleValue,
    PVOID Context
    );

/*++

Routine Description:

    This routine is called on each handle in the handle table for which it was
    invoked. The handle table will be locked during this call, so this call
    must not make any calls that would require accessing the handle table.

Arguments:

    HandleTable - Supplies a pointer to the handle table being iterated through.

    Descriptor - Supplies the handle descriptor for the current handle.

    Flags - Supplies the flags associated with this handle.

    HandleValue - Supplies the handle value for the current handle.

    Context - Supplies an opaque pointer of context that was provided when the
        iteration was requested.

Return Value:

    None.

--*/

typedef
VOID
(*PHANDLE_TABLE_LOOKUP_CALLBACK) (
    PHANDLE_TABLE HandleTable,
    HANDLE Descriptor,
    PVOID HandleValue
    );

/*++

Routine Description:

    This routine is called whenever a handle is looked up. It is called with
    the handle table lock still held.

Arguments:

    HandleTable - Supplies a pointer to the handle table being iterated through.

    Descriptor - Supplies the handle descriptor for the current handle.

    HandleValue - Supplies the handle value for the current handle.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a scheduler wait queue, upon which threads can
    block.

Members:

    Lock - Stores the spin lock used to synchronize access to the structure.

    State - Stores the signaling state of the object. This is of type
        SIGNAL_STATE.

    Waiters - Stores a list of wait blocks waiting on this object.

--*/

typedef struct _WAIT_QUEUE {
    KSPIN_LOCK Lock;
    volatile ULONG State;
    LIST_ENTRY Waiters;
} WAIT_QUEUE, *PWAIT_QUEUE;

/*++

Structure Description:

    This structure defines a generic kernel object.

Members:

    Type - Stores the object type.

    NameLength - Stores the length of the name buffer in bytes, including the
        null terminator.

    Name - Stores an optional pointer to the pool allocated name.

    Parent - Stores a pointer to the parent object.

    SiblingEntry - Stores the list entry for its sibling objects.

    ChildListHead - Stores the list head for its child objects.

    Flags - Stores state flags regarding the object.

    ReferenceCount - Stores the reference count of the object, managed by the
        Object Manager.

    DestroyRoutine - Stores an optional pointer to a function to be called when
        the reference count drops to zero immediately before the object is
        deallocated.

--*/

typedef struct _OBJECT_HEADER OBJECT_HEADER, *POBJECT_HEADER;
struct _OBJECT_HEADER {
    OBJECT_TYPE Type;
    ULONG NameLength;
    PCSTR Name;
    POBJECT_HEADER Parent;
    LIST_ENTRY SiblingEntry;
    LIST_ENTRY ChildListHead;
    ULONG Flags;
    WAIT_QUEUE WaitQueue;
    volatile ULONG ReferenceCount;
    PDESTROY_OBJECT_ROUTINE DestroyRoutine;
};

typedef struct _WAIT_BLOCK WAIT_BLOCK, *PWAIT_BLOCK;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
ObInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the Object Manager. It requires that the MM pools
    are online.

Arguments:

    None.

Return Value:

    Status code.

--*/

PVOID
ObGetRootObject (
    VOID
    );

/*++

Routine Description:

    This routine returns the root object of the system.

Arguments:

    None.

Return Value:

    Returns a pointer to the root object.

--*/

VOID
ObInitializeWaitQueue (
    PWAIT_QUEUE WaitQueue,
    SIGNAL_STATE InitialState
    );

/*++

Routine Description:

    This routine initializes a wait queue structure.

Arguments:

    WaitQueue - Supplies a pointer to the wait queue to initialize.

    InitialState - Supplies the initial state to set the queue to.

Return Value:

    None.

--*/

PVOID
ObCreateObject (
    OBJECT_TYPE Type,
    PVOID Parent,
    PCSTR ObjectName,
    ULONG NameLength,
    ULONG DataSize,
    PDESTROY_OBJECT_ROUTINE DestroyRoutine,
    ULONG Flags,
    ULONG Tag
    );

/*++

Routine Description:

    This routine creates a new system object.

Arguments:

    Type - Supplies the type of object being created.

    Parent - Supplies a pointer to the object that this object is a child under.
        Supply NULL to create an object off the root node.

    ObjectName - Supplies an optional name for the object. A copy of this
        string will be made unless the flags specify otherwise.

    NameLength - Supplies the length of the name string in bytes, including
        the null terminator.

    DataSize - Supplies the size of the object body, *including* the object
        header.

    DestroyRoutine - Supplies an optional pointer to a function to be called
        when the reference count of the object drops to zero (immediately before
        the object is deallocated).

    Flags - Supplies optional flags indicating various properties of the object.
        See OBJECT_FLAG_* definitions.

    Tag - Supplies the pool tag that should be used for the memory allocation.

Return Value:

    Returns a pointer to the new object, on success. The returned structure is
        assumed to start with an OBJECT_HEADER structure.

    NULL if the object could not be allocated, the object already exists, or
        an invalid parameter was passed in.

--*/

KERNEL_API
VOID
ObAddReference (
    PVOID Object
    );

/*++

Routine Description:

    This routine increases the reference count on an object by 1.

Arguments:

    Object - Supplies a pointer to the object to add a reference to.

Return Value:

    None.

--*/

KERNEL_API
VOID
ObReleaseReference (
    PVOID Object
    );

/*++

Routine Description:

    This routine decreases the reference count of an object by 1. If this
    causes the reference count of the object to drop to 0, the object will be
    freed. This may cascade up the tree.

Arguments:

    Object - Supplies a pointer to the object to subtract a reference from.
        This structure is presumed to begin with an OBJECT_HEADER structure.

Return Value:

    None.

--*/

KSTATUS
ObUnlinkObject (
    PVOID Object
    );

/*++

Routine Description:

    This routine unlinks an object.

Arguments:

    Object - Supplies a pointer to the object to unlink.

Return Value:

    Status code.

--*/

KSTATUS
ObNameObject (
    PVOID Object,
    PCSTR Name,
    ULONG NameLength,
    ULONG Tag,
    BOOL UseNameDirectly
    );

/*++

Routine Description:

    This routine names an object.

Arguments:

    Object - Supplies a pointer to the object to name.

    Name - Supplies a pointer to the object name.

    NameLength - Supplies the new name's length.

    Tag - Supplies the pool tag to use to allocate the name.

    UseNameDirectly - Supplies the new value of the "Use name directly" flag
        in the object.

Return Value:

    Status code.

--*/

PWAIT_BLOCK
ObCreateWaitBlock (
    ULONG Capacity
    );

/*++

Routine Description:

    This routine creates a wait block. While this can be done on the fly,
    creating a wait block ahead of time is potentially faster if the number of
    elements being waited on is fairly large (greater than approximately 7).

Arguments:

    Capacity - Supplies the maximum number of queues that will be waited on
        with this wait block.

Return Value:

    Returns a pointer to the wait block on success.

    NULL on allocation failure.

--*/

VOID
ObDestroyWaitBlock (
    PWAIT_BLOCK WaitBlock
    );

/*++

Routine Description:

    This routine destroys an explicitly created wait block. The wait block must
    not be actively waiting on anything.

Arguments:

    WaitBlock - Supplies a pointer to the wait block.

Return Value:

    None.

--*/

KSTATUS
ObWait (
    PWAIT_BLOCK WaitBlock,
    ULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine executes a wait block, waiting on the given list of wait
    queues for the specified amount of time.

Arguments:

    WaitBlock - Supplies a pointer to the wait block to wait for.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        queues should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever.

Return Value:

    STATUS_SUCCESS if the wait completed successfully.

    STATUS_TIMEOUT if the wait timed out.

    STATUS_INTERRUPTED if the wait timed out early due to a signal.

--*/

KERNEL_API
KSTATUS
ObWaitOnQueue (
    PWAIT_QUEUE Queue,
    ULONG Flags,
    ULONG TimeoutInMilliseconds
    );

/*++

Routine Description:

    This routine waits on a given wait queue. It is assumed that the caller can
    ensure externally that the wait queue will remain allocated.

Arguments:

    Queue - Supplies a pointer to the queue to wait on.

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See WAIT_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        queues should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
ObWaitOnObjects (
    PVOID *ObjectArray,
    ULONG ObjectCount,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PWAIT_BLOCK PreallocatedWaitBlock,
    PVOID *SignalingObject
    );

/*++

Routine Description:

    This routine waits on multiple objects until one (or all in some cases) is
    signaled. The caller is responsible for maintaining references to these
    objects.

Arguments:

    ObjectArray - Supplies an array of object pointers containing the objects
        to wait on. Each object must only be on the list once.

    ObjectCount - Supplies the number of elements in the array.

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See WAIT_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        objects should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever on these objects.

    PreallocatedWaitBlock - Supplies an optional pointer to a pre-allocated
        wait block to use for the wait. This is optional, however if there are
        a large number of objects to wait on and this is a common operation, it
        is a little faster to pre-allocate a wait block and reuse it.

    SignalingObject - Supplies an optional pointer where the object that
        satisfied the wait will be returned on success. If the wait was
        interrupted, this returns NULL. If the WAIT_FLAG_ALL_OBJECTS flag was
        not specified, the first object to be signaled will be returned. If the
        WAIT_FLAG_ALL_OBJECTS was specified, the caller should not depend on
        a particular object being returned here.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
ObWaitOnQueues (
    PWAIT_QUEUE *QueueArray,
    ULONG Count,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    PWAIT_BLOCK PreallocatedWaitBlock,
    PWAIT_QUEUE *SignalingQueue
    );

/*++

Routine Description:

    This routine waits on multiple wait queues until one (or all in some cases)
    is signaled. The caller is responsible for ensuring externally that these
    wait queues will not somehow be deallocated over the course of the wait.

Arguments:

    QueueArray - Supplies an array of wait queue pointers containing the queues
        to wait on. Each queue must only be on the list once.

    Count - Supplies the number of elements in the array.

    Flags - Supplies a bitfield of flags governing the behavior of the wait.
        See WAIT_FLAG_* definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the given
        queues should be waited on before timing out. Use WAIT_TIME_INDEFINITE
        to wait forever.

    PreallocatedWaitBlock - Supplies an optional pointer to a pre-allocated
        wait block to use for the wait. This is optional, however if there are
        a large number of queues to wait on and this is a common operation, it
        is a little faster to pre-allocate a wait block and reuse it.

    SignalingQueue - Supplies an optional pointer where the queue that
        satisfied the wait will be returned on success. If the wait was
        interrupted, this returns NULL. If the WAIT_FLAG_ALL_OBJECTS flag was
        not specified, the first queue to be signaled will be returned. If the
        WAIT_FLAG_ALL_OBJECTS was specified, the caller should not depend on
        a particular queue being returned here.

Return Value:

    Status code.

--*/

VOID
ObSignalQueue (
    PWAIT_QUEUE Queue,
    SIGNAL_OPTION Signal
    );

/*++

Routine Description:

    This routine signals (or unsignals) a wait queue, potentially releasing
    threads blocking on this object.

Arguments:

    Queue - Supplies a pointer to the wait queue to signal.

    Signal - Supplies the type of signaling to provide on the queue. Valid
        values are:

        SignalOptionSignalAll - Sets the queue to a signaled state and leaves
            it that way. All threads waiting on this queue will continue.

        SignalOptionSignalOne - Wakes up one thread waiting on the queue.
            If no threads are waiting on the queue, the state will be
            signaled until one thread waits on the queue, at which point it
            will go back to being unsignaled.

        SignalOptionPulse - Satisfies all waiters currently waiting on the
            queue, but does not set the state to signaled.

        SignalOptionUnsignal - Sets the queue's state to unsignaled.

Return Value:

    None.

--*/

BOOL
ObWakeBlockedThread (
    PVOID ThreadToWake,
    BOOL OnlyWakeSuspendedThreads
    );

/*++

Routine Description:

    This routine wakes up a blocked or suspended thread, interrupting any wait
    it may have been performing. If the thread was not blocked or suspended or
    the wait is not interruptible, then this routine does nothing.

Arguments:

    ThreadToWake - Supplies a pointer to the thread to poke.

    OnlyWakeSuspendedThreads - Supplies a boolean indicating that the thread
        should only be poked if it's in the suspended state (not if it's in
        the blocked state).

Return Value:

    TRUE if the thread was actually pulled out of a blocked or suspended state.

    FALSE if no action was performed because the thread was not stopped.

--*/

BOOL
ObWakeBlockingThread (
    PVOID ThreadToWake
    );

/*++

Routine Description:

    This routine wakes up a blocking or suspending thread, interrupting any
    wait it may have been performing. This routine assumes that the thread is
    either blocking or suspending. It should not be in another state.

Arguments:

    ThreadToWake - Supplies a pointer to the thread to poke.

Return Value:

    TRUE if the thread was actually pulled out of a blocking or suspending
    state.

    FALSE if no action was performed because the thread had already been awoken.

--*/

PVOID
ObFindObject (
    PCSTR ObjectName,
    ULONG BufferLength,
    POBJECT_HEADER ParentObject
    );

/*++

Routine Description:

    This routine locates an object by name. The found object will be returned
    with an incremented reference count as to ensure it does not disappear
    while the caller is handling it. It is the caller's responsibility to
    release this reference.

Arguments:

    ObjectName - Supplies the name of the object to find, either
        fully-specified or relative. Fully specified names begin with a /.

    BufferLength - Supplies the length of the name buffer.

    ParentObject - Supplies the parent object for relative names.

Return Value:

    Returns a pointer to the object with the specified name, or NULL if the
    object could not be found.

--*/

PSTR
ObGetFullPath (
    PVOID Object,
    ULONG AllocationTag
    );

/*++

Routine Description:

    This routine returns the full path of the given object. The return value
    will be allocated from paged pool, and it is the caller's responsibility
    to free it. The object path must not have any unnamed objects anywhere in
    its parent chain.

Arguments:

    Object - Supplies a pointer to the object whose path to return.

    AllocationTag - Supplies the allocation tag to use when allocating memory
        for the return value.

Return Value:

    Returns a pointer to the full path of the object, allocated from paged
    pool. It is the caller's responsibility to free this memory.

    NULL on failure, usually a memory allocation failure or an unnamed object
    somewhere in the path.

--*/

PWAIT_QUEUE
ObGetBlockingQueue (
    PVOID Thread
    );

/*++

Routine Description:

    This routine returns one of the wait queues the given thread is blocking on.
    The caller is not guaranteed that the queue returned has a reference on it
    and won't disappear before being accessed. Generally this routine is only
    used by the scheduler for profiling.

Arguments:

    Thread - Supplies a pointer to the thread.

Return Value:

    Returns a pointer to the first wait queue the thread is blocking on, if any.
    The built-in thread timer does not count.

    NULL if the thread's wait block is empty.

--*/

//
// Handle Table routines.
//

PHANDLE_TABLE
ObCreateHandleTable (
    PVOID Process,
    PHANDLE_TABLE_LOOKUP_CALLBACK LookupCallbackRoutine
    );

/*++

Routine Description:

    This routine creates a new handle table. This routine must be called at low
    level.

Arguments:

    Process - Supplies an optional pointer to the process that owns the handle
        table. When in doubt, supply NULL.

    LookupCallbackRoutine - Supplies an optional pointer that if supplied
        points to a function that will get called whenever a handle value is
        looked up (but not on iterates).

Return Value:

    Returns a pointer to the new handle table on success.

    NULL on insufficient resource conditions.

--*/

VOID
ObDestroyHandleTable (
    PHANDLE_TABLE HandleTable
    );

/*++

Routine Description:

    This routine destroys a handle table. This routine must be called at low
    level.

Arguments:

    HandleTable - Supplies a pointer to the handle table to destroy.

Return Value:

    None.

--*/

KSTATUS
ObEnableHandleTableLocking (
    PHANDLE_TABLE HandleTable
    );

/*++

Routine Description:

    This routine enables locking on the given handle table.

Arguments:

    HandleTable - Supplies a pointer to the handle table to enable locking for.

Return Value:

    Status code.

--*/

KSTATUS
ObCreateHandle (
    PHANDLE_TABLE Table,
    PVOID HandleValue,
    ULONG Flags,
    PHANDLE NewHandle
    );

/*++

Routine Description:

    This routine creates a new handle table entry. This routine must be called
    at low level.

Arguments:

    Table - Supplies a pointer to the handle table.

    HandleValue - Supplies the value to be associated with the handle.

    Flags - Supplies a bitfield of flags to set with the handle. This value
        will be ANDed with HANDLE_FLAG_MASK, so bits set outside of that range
        will not stick.

    NewHandle - Supplies a pointer where the handle will be returned. On input,
        contains the minimum required value for the handle. Supply
        INVALID_HANDLE as the initial contents to let the system decide (which
        should be almost always).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    handle table entry.

--*/

VOID
ObDestroyHandle (
    PHANDLE_TABLE Table,
    HANDLE Handle
    );

/*++

Routine Description:

    This routine destroys a handle.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle returned when the handle was created.

Return Value:

    None.

--*/

KSTATUS
ObReplaceHandleValue (
    PHANDLE_TABLE Table,
    HANDLE Handle,
    PVOID NewHandleValue,
    ULONG NewFlags,
    PVOID *OldHandleValue,
    PULONG OldFlags
    );

/*++

Routine Description:

    This routine replaces a handle table entry, or creates a handle if none was
    there before. This routine must be called at low level.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle to replace or create.

    NewHandleValue - Supplies the value to be associated with the handle.

    NewFlags - Supplies the new handle flags to set.

    OldHandleValue - Supplies an optional pointer where the original handle
        value will be returned.

    OldFlags - Supplies an optional pointer where the original handle flags
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
    handle table entry.

    STATUS_TOO_MANY_HANDLES if the given minimum handle value was too high.

--*/

PVOID
ObGetHandleValue (
    PHANDLE_TABLE Table,
    HANDLE Handle,
    PULONG Flags
    );

/*++

Routine Description:

    This routine looks up the given handle and returns the value associated
    with that handle.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle returned when the handle was created.

    Flags - Supplies an optional pointer that receives value of the handle's
        flags.

Return Value:

    Returns the value associated with that handle upon success.

    NULL if the given handle is invalid.

--*/

KSTATUS
ObGetSetHandleFlags (
    PHANDLE_TABLE Table,
    HANDLE Handle,
    BOOL Set,
    PULONG Flags
    );

/*++

Routine Description:

    This routine sets and/or returns the flags associated with a handle. The
    lookup callback routine initialized with the handle table is not called
    during this operation.

Arguments:

    Table - Supplies a pointer to the handle table.

    Handle - Supplies the handle whose flags should be retrieved.

    Set - Supplies a boolean indicating if the value in the flags parameter
        should be set as the new value.

    Flags - Supplies a pointer that on input contains the value of the flags
        to set if the set parameter is TRUE. This value will be ANDed with
        HANDLE_FLAG_MASK, so bits set outside of that mask will not stick.
        On output, contains the original value of the flags before the set was
        performed.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if no such handle could be found.

--*/

HANDLE
ObGetHighestHandle (
    PHANDLE_TABLE Table
    );

/*++

Routine Description:

    This routine returns the highest allocated handle.

Arguments:

    Table - Supplies a pointer to the handle table.

Return Value:

    Returns the highest handle number (not the handle value).

    INVALID_HANDLE if the table is empty.

--*/

VOID
ObHandleTableIterate (
    PHANDLE_TABLE Table,
    PHANDLE_TABLE_ITERATE_ROUTINE IterateRoutine,
    PVOID IterateRoutineContext
    );

/*++

Routine Description:

    This routine iterates through all handles in the given handle table, and
    calls the given handle table for each one. The table will be locked when the
    iterate routine is called, so the iterate routine must not make any calls
    that would require use of the handle table.

Arguments:

    Table - Supplies a pointer to the handle table to iterate through.

    IterateRoutine - Supplies a pointer to the routine to be called for each
        handle in the table.

    IterateRoutineContext - Supplies an opaque context pointer that will get
        passed to the iterate routine each time it is called.

Return Value:

    None.

--*/

