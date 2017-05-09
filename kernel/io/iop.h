/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    iop.h

Abstract:

    This header contains private definitions for the I/O Subsystem.

Author:

    Evan Green 16-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro evaluates to non-zero if the given object is a device or a
// volume.
//

#define IS_DEVICE_OR_VOLUME(_Object)                        \
    ((((POBJECT_HEADER)(_Object))->Type == ObjectDevice) || \
     (((POBJECT_HEADER)(_Object))->Type == ObjectVolume))

//
// This macro evaluates to non-zero if the given file object has no dirty data
// and no dirty properties.
//

#define IS_FILE_OBJECT_CLEAN(_FileObject)   \
    (((_FileObject)->Flags &                \
     (FILE_OBJECT_FLAG_DIRTY_DATA | FILE_OBJECT_FLAG_DIRTY_PROPERTIES)) == 0)

//
// ---------------------------------------------------------------- Definitions
//

#define IO_ALLOCATION_TAG 0x21216F49 // '!!oI'
#define FI_ALLOCATION_TAG 0x656C6946 // 'eliF'
#define DEVICE_ALLOCATION_TAG 0x21766544 // '!veD'
#define DEVICE_WORK_ALLOCATION_TAG 0x57766544 // 'WveD'
#define IRP_ALLOCATION_TAG 0x21707249 // '!prI'
#define DEVICE_INTERFACE_ALLOCATION_TAG 0x49766544 // 'IveD'
#define DEVICE_INFORMATION_ALLOCATION_TAG 0x666E4944 // 'fnID'
#define DEVICE_INFORMATION_REQUEST_ALLOCATION_TAG 0x526E4944 // 'RnID'
#define PATH_ALLOCATION_TAG 0x68746150 // 'htaP'
#define FILE_LOCK_ALLOCATION_TAG 0x6B434C46 // 'kcLF'
#define SOCKET_INFORMATION_ALLOCATION_TAG 0x666E4953 // 'fnIS'
#define UNIX_SOCKET_ALLOCATION_TAG 0x6F536E55 // 'oSnU'

#define IRP_MAGIC_VALUE (USHORT)IRP_ALLOCATION_TAG

//
// This flag is set once the DriverEntry routine has been called for a driver.
//

#define DRIVER_FLAG_ENTRY_CALLED 0x00000001

//
// This flag is set if a driver returns a failing status code to its
// DriverEntry routine.
//

#define DRIVER_FLAG_FAILED_DRIVER_ENTRY 0x00000002

//
// This flag is set if a driver was loaded by the boot environment.
//

#define DRIVER_FLAG_LOADED_AT_BOOT 0x00000004

//
// This flag is set on a device action if the action is to be sent down to the
// entire subtree below this device. This performs a pre-order traversal.
//

#define DEVICE_ACTION_SEND_TO_SUBTREE 0x00000001

//
// This flag is set on a device action if the action is to be sent to the device
// and its children. Only the children will receive the action, not
// grandchildren.
//

#define DEVICE_ACTION_SEND_TO_CHILDREN 0x00000002

//
// This flag is set on a device action if the action should open the queue.
//

#define DEVICE_ACTION_OPEN_QUEUE 0x00000004

//
// This flag is set on a device action if the action should close the queue.
//

#define DEVICE_ACTION_CLOSE_QUEUE 0x00000008

//
// This flag is set when an IRP has been marked as complete.
//

#define IRP_COMPLETE 0x00000001

//
// This flag is set in an IRP when it's been marked as pending.
//

#define IRP_PENDING 0x00000002

//
// This flag is set in an IRP when it's active.
//

#define IRP_ACTIVE 0x00000004

//
// This flag is used during processing Query Children to mark pre-existing
// devices and notice missing ones.
//

#define DEVICE_FLAG_ENUMERATED 0x00000001

//
// This flag is used to indicate that the device represents a volume that
// should be mounted by a file system.
//

#define DEVICE_FLAG_MOUNTABLE 0x00000002

//
// This flag is set when a file system has successfully been mounted on the
// device.
//

#define DEVICE_FLAG_MOUNTED 0x00000004

//
// This flag is set when a device is set to act as the paging device.
//

#define DEVICE_FLAG_PAGING_DEVICE 0x00000008

//
// This flag is set when a device isn't using its boot resources, or it has
// no boot resources.
//

#define DEVICE_FLAG_NOT_USING_BOOT_RESOURCES 0x00000010

//
// This flag is set when a volume is in the process of being removed.
//

#define VOLUME_FLAG_UNMOUNTING 0x00000001

//
// This flag is set in the file object if it is closing.
//

#define FILE_OBJECT_FLAG_CLOSING 0x00000001

//
// This flag is set in the file object if it failed to close.
//

#define FILE_OBJECT_FLAG_CLOSE_FAILED 0x00000002

//
// This flag is set in the file object if it has been opened.
//

#define FILE_OBJECT_FLAG_OPEN 0x00000004

//
// This flag is set in the file object if its properties are dirty.
//

#define FILE_OBJECT_FLAG_DIRTY_PROPERTIES 0x00000008

//
// This flag is set in the file object if its data should not be cached in the
// page cache.
//

#define FILE_OBJECT_FLAG_NO_PAGE_CACHE 0x00000010

//
// This flag is set if the file object gets its I/O state from elsewhere, and
// should not try to free it.
//

#define FILE_OBJECT_FLAG_EXTERNAL_IO_STATE 0x00000020

//
// This flag is set if the file object has any dirty page cache entries.
//

#define FILE_OBJECT_FLAG_DIRTY_DATA 0x00000040

//
// This flag indicates that a hard flush must be preformed before the file
// object's contents are preserved on a backing store.
//

#define FILE_OBJECT_FLAG_HARD_FLUSH_REQUIRED 0x00000080

//
// This flag is set if the file object's I/O state needs to be allocated from
// non-paged pool.
//

#define FILE_OBJECT_FLAG_NON_PAGED_IO_STATE 0x00000100

//
// The resource allocation work is currently assigned to the system work queue.
//

#define IoResourceAllocationWorkQueue NULL

//
// Define the size of read-aheads.
//

#define IO_READ_AHEAD_SIZE _128KB

//
// This flag is set to indicate that the eviction operation is executing as a
// result of a truncate. All image sections should be unmapped and all page
// cache entries should be removed.
//

#define EVICTION_FLAG_TRUNCATE 0x00000001

//
// This flag is set to indicate that the eviction operation is executing as a
// result of a device removal. There may be outstanding references on the
// device or file, but all of its page cache entries should be aggressively
// removed and image sections unmapped.
//

#define EVICTION_FLAG_REMOVE 0x00000002

//
// --------------------------------------------------------------------- Macros
//

//
// This macro sets a problem code on a device, automatically generating the
// source file and line number parameters. The first parameter is a PDEVICE,
// the second parameter is a DEVICE_PROBLEM, and the third parameter is a
// KSTATUS.
//

#define IopSetDeviceProblem(_Device, _Problem, _Status) \
    IopSetDeviceProblemEx((_Device),                    \
                          (_Problem),                   \
                          (_Status),                    \
                          NULL,                         \
                          0,                            \
                          __FILE__,                     \
                          __LINE__)                     \

//
// This macro determines whether or not the device is in a state where it is
// able to accept work. The parameter should be of type PDEVICE.
//

#define IO_IS_DEVICE_ALIVE(_Device)                            \
    (((_Device)->State != DeviceStateInvalid) &&            \
     ((_Device)->State != DeviceUnreported) &&              \
     ((_Device)->State != DeviceRemoved))

//
// This macro determines whether or not the device queue is in a state to be
// accepting new work. The parameter should be of type PDEVICE.
//

#define IO_IS_DEVICE_QUEUE_OPEN(_Device)                       \
    (((_Device)->QueueState == DeviceQueueOpen) ||          \
     ((_Device)->QueueState == DeviceQueueActive))

//
// This macro determines if the given path point is a mount point. This is the
// case if the path entry is equal to the owning mount point's target path
// entry.
//

#define IO_IS_MOUNT_POINT(_PathPoint) \
    ((_PathPoint)->PathEntry == (_PathPoint)->MountPoint->TargetEntry)

//
// This macro determines whether this is a cacheable file-ish object. It
// excludes block and character devices.
//

#define IO_IS_CACHEABLE_FILE(_IoObjectType)         \
    ((_IoObjectType == IoObjectRegularFile) ||      \
     (_IoObjectType == IoObjectSymbolicLink) ||     \
     (_IoObjectType == IoObjectSharedMemoryObject))

//
// This macro determines whether or not a file type is cacheable.
//

#define IO_IS_CACHEABLE_TYPE(_IoObjectType)         \
    ((_IoObjectType == IoObjectBlockDevice) ||      \
     IO_IS_CACHEABLE_FILE(_IoObjectType))

//
// This macro determines whether or not a file object is cacheable.
//

#define IO_IS_FILE_OBJECT_CACHEABLE(_FileObject)                             \
    ((IO_IS_CACHEABLE_TYPE(_FileObject->Properties.Type) != FALSE) &&        \
     ((_FileObject->Flags & FILE_OBJECT_FLAG_NO_PAGE_CACHE) == 0))

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _FILE_OBJECT_TIME_TYPE {
    FileObjectAccessTime,
    FileObjectModifiedTime,
    FileObjectStatusTime,
} FILE_OBJECT_TIME_TYPE, *PFILE_OBJECT_TIME_TYPE;

typedef struct _DEVICE_POWER DEVICE_POWER, *PDEVICE_POWER;

/*++

Structure Description:

    This structure defines a file object.

Members:

    TreeEntry - Stores the Red-Black tree node information for this file object,
        used internally. Never access these members directly.

    ListEntry - Stores an entry into the list of file objects.

    PageCacheTree - Stores a tree root for the page cache entries that
        belong to this file object.

    DirtyPageList - Stores the head of the list of dirty page cache entries
        in this file object. This list is synchronized by the global page
        cache list lock.

    ReferenceCount - Stores the memory reference count on this structure, used
        internally. Never manipulate this member directly.

    PathEntryCount - Stores the count of path entries that are using this file
        object. This accounts for all ways that a file object whose hard link
        count has gone to zero can still be accessed. If a path entry is using
        it, I/O can still be done on the file object.

    Device - Stores a pointer to the device or volume that owns the file serial
        number.

    Directory - Stores a pointer to an open handle to the file's directory,
        which is used to synchronize deletes, opens, and metadata updates.

    Lock - Stores a pointer to the lock that serializes I/O operations on
        this file object and child path entry lookup, creation, and insertion.

    IoState - Stores a pointer to the I/O object state for this file object.

    SpecialIo - Stores a pointer to the context needed to do I/O if this is a
        special object (like a pipe, terminal, socket, or shared memory object).

    ReadyEvent - Stores a pointer to an event that must be waited on before
        using this file object.

    ImageSectionList - Stores a pointer to a list of image setions that map
        portions of this file object.

    DeviceContext - Stores a pointer to context supplied by the device with the
        file was opened.

    Flags - Stores a set of flags describing the file object state. See
        FILE_OBJECT_FLAG_* for definitions. This must be modified with atomic
        operations.

    MapFlags - Stores a set of additional mapping flags that should be set
        when mapping contents from this file object.

    Properties - Stores the characteristics for this file.

    FileLockList - Stores the head of the list of file locks held on this
        file object. This is a user mode thing.

    FileLockEvent - Stores a pointer to the event that's signalled when a file
        object lock is released.

--*/

typedef struct _FILE_OBJECT FILE_OBJECT, *PFILE_OBJECT;
struct _FILE_OBJECT {
    RED_BLACK_TREE_NODE TreeEntry;
    LIST_ENTRY ListEntry;
    RED_BLACK_TREE PageCacheTree;
    LIST_ENTRY DirtyPageList;
    volatile ULONG ReferenceCount;
    volatile ULONG PathEntryCount;
    PDEVICE Device;
    PSHARED_EXCLUSIVE_LOCK Lock;
    PIO_OBJECT_STATE IoState;
    PVOID SpecialIo;
    PKEVENT ReadyEvent;
    volatile PIMAGE_SECTION_LIST ImageSectionList;
    volatile PVOID DeviceContext;
    volatile ULONG Flags;
    ULONG MapFlags;
    FILE_PROPERTIES Properties;
    LIST_ENTRY FileLockList;
    PKEVENT FileLockEvent;
};

/*++

Structure Description:

    This structure defines a path entry.

Members:

    SiblingListEntry - Stores pointers to the next and previous entries in
        the parent directory.

    CacheListEntry - Stores pointers to the next and previous entries in the
        LRU list of the path entry cache.

    ReferenceCount - Stores the reference count of the entry.

    MountCount - Stores the number of mount points mounted on this path entry.

    MountFlags - Stores a bitmaks of mount related flags that get inherited
        from a path entry's parent.

    Negative - Stores a boolean indicating that is is a negative path entry,
        which caches the lack of a file here.

    DoNotCache - Stores a boolean indicating that this path entry should not
        be cached.

    Name - Stores a pointer to the name of the path entry, allocated in paged
        pool.

    NameSize - Stores the size of the name buffer in bytes including the null
        terminator.

    Hash - Stores a hash of the name, used for quick negative comparisons.

    SequenceNumber - Stores a sequence number to keep track of whether or not
        the path entry is in sync with the underlying file object.

    Parent - Stores a pointer to the parent node.

    ChildList - Stores the list of children for this node.

    FileObject - Stores a pointer to the file object backing this path entry.

--*/

struct _PATH_ENTRY {
    LIST_ENTRY SiblingListEntry;
    LIST_ENTRY CacheListEntry;
    volatile ULONG ReferenceCount;
    volatile ULONG MountCount;
    BOOL Negative;
    BOOL DoNotCache;
    PSTR Name;
    ULONG NameSize;
    ULONG Hash;
    PPATH_ENTRY Parent;
    LIST_ENTRY ChildList;
    PFILE_OBJECT FileObject;
};

/*++

Struction Description:

    This structure defines a mount point.

Members:

    SiblingListEntry - Stores pointers to the next and previous entries in
        the parent mount's list of children.

    ChildListHead- Stores the list head for its child mount points.

    Parent - Stores a pointer to the parent mount point. If set to NULL, then
        the parent has been unmounted.

    MountEntry - Stores a pointer to the path entry that the mount point is
        mounted on.

    TargetEntry - Stores a pointer to the path entry that is the target path
        entry to traverse for this mount point.

    TargetPath - Store a string to the original target path specified during
        the mount request.

    ReferenceCount - Stores the reference count of the mount point.

    Flags - Stores a bitmask of flags for this mount point. See MOUNT_FLAG_*
        for definitions.

--*/

struct _MOUNT_POINT {
    LIST_ENTRY SiblingListEntry;
    LIST_ENTRY ChildListHead;
    PMOUNT_POINT Parent;
    PPATH_ENTRY MountEntry;
    PPATH_ENTRY TargetEntry;
    PSTR TargetPath;
    volatile ULONG ReferenceCount;
    ULONG Flags;
};

/*++

Enumeration Description:

    This enumeration describes the various I/O handle types.

Values:

    IoHandleTypeDefault - Indicates a default I/O handle.

    IoHandleTypePaging - Indicates a paging I/O handle.

--*/

typedef enum _IO_HANDLE_TYPE {
    IoHandleTypeDefault,
    IoHandleTypePaging
} IO_HANDLE_TYPE, *PIO_HANDLE_TYPE;

/*++

Structure Description:

    This structure defines the context behind a generic I/O handle.

Members:

    HandleType - Stores the type of I/O handle. All I/O handle types must begin
        with a member of this type.

    Type - Stores the type of I/O object this handle represents.

    OpenFlags - Stores the flags the handle was opened with.

    Access - Stores the access permissions for the handle.

    ReferenceCount - Stores the current reference count on the I/O handle.
        Never manipulate this value directly, use the APIs provided to add
        or release a reference.

    Device - Stores a pointer to the underlying device or object that performs
        the I/O.

    DeviceContext - Stores a context pointer supplied by the device when the
        handle was opened.

    PathPoint - Stores the path context (path entry and mount point) for the
        file or object.

    FileObject - Stores a pointer to the file object to interact with for I/O
        purposes.

    CurrentOffset - Stores the current file pointer.

    Async - Stores an optional pointer to the asynchronous receiver state.

--*/

struct _IO_HANDLE {
    IO_HANDLE_TYPE HandleType;
    ULONG OpenFlags;
    ULONG Access;
    volatile ULONG ReferenceCount;
    PVOID DeviceContext;
    PATH_POINT PathPoint;
    PFILE_OBJECT FileObject;
    IO_OFFSET CurrentOffset;
    PASYNC_IO_RECEIVER Async;
};

/*++

Structure Description:

    This structure defines the stripped down basic paging I/O handle. There
    is no locking, no reference counting, more or less just enough information
    to pass requests directly to the file system or block device.

Members:

    HandleType - Stores the type of I/O handle. All I/O handle types must begin
        with a member of this type.

    IoHandle - Stores a pointer to the normal I/O handle.

    Device - Stores a pointer to the device this I/O handle points to.

    DeviceContext - Stores the context pointer returned by the file system or
        device when the object was opened.

    Capacity - Stores the total size of the file or block device, in bytes.

    OffsetAlignment - Stores the required alignment of all I/O offsets.

    SizeAlignment - Stores the required physical alignment of all I/O buffers.

--*/

typedef struct _PAGING_IO_HANDLE {
    IO_HANDLE_TYPE HandleType;
    PIO_HANDLE IoHandle;
    PDEVICE Device;
    PVOID DeviceContext;
    ULONGLONG Capacity;
    ULONG OffsetAlignment;
    ULONG SizeAlignment;
} PAGING_IO_HANDLE, *PPAGING_IO_HANDLE;

/*++

Structure Description:

    This structure defines the context used for an I/O operation.

Members:

    Offset - Stores the offset from the beginning of the file or device where
        the I/O should be done.

    IoBuffer - Stores a pointer to an I/O buffer that either contains the data
        to write or will contain the read data.

    SizeInBytes - Stores the number of bytes to read or write.

    BytesCompleted - Stores the number of bytes of I/O actually performed.

    Flags - Stores the flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Stores the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Write - Stores a boolean value indicating if the I/O operation is a write
        (TRUE) or a read (FALSE).

--*/

typedef struct _IO_CONTEXT {
    IO_OFFSET Offset;
    PIO_BUFFER IoBuffer;
    UINTN SizeInBytes;
    UINTN BytesCompleted;
    ULONG Flags;
    ULONG TimeoutInMilliseconds;
    BOOL Write;
} IO_CONTEXT, *PIO_CONTEXT;

/*++

Structure Description:

    This structure defines an entry in the device database, which associates
    devices or device classes with drivers. These structures are generally
    paged.

Members:

    ListEntry - Supplies pointers to the next and previous entries in the
        database.

    DeviceId - Supplies a string containing the device ID, in the case of a
        device to driver association.

    ClassId - Supplies a string containing the class ID, in the case of a
        device class to driver association.

    DriverName - Supplies a string containing the driver associated with this
        device or device class.

--*/

typedef struct _DEVICE_DATABASE_ENTRY {
    LIST_ENTRY ListEntry;
    union {
        PSTR DeviceId;
        PSTR ClassId;
    } U;

    PSTR DriverName;
} DEVICE_DATABASE_ENTRY, *PDEVICE_DATABASE_ENTRY;

/*++

Enumeration Description:

    This enumeration describes the various device actions.

Values:

    DeviceActionInvalid - Indicates an invalid device action.

    DeviceActionStart - Indicates that the device should be started.

    DeviceActionQueryChildren - Indicates that the devices children should be
        queried.

    DeviceActionPrepareRemove - Indicates that the device should prepare for
        removal.

    DeviceActionRemove - Indicates that the device should be removed.

    DeviceActionPowerTransition - Indicates that the device will undergo a
        power state change.

--*/

typedef enum _DEVICE_ACTION {
    DeviceActionInvalid,
    DeviceActionStart,
    DeviceActionQueryChildren,
    DeviceActionPrepareRemove,
    DeviceActionRemove,
    DeviceActionPowerTransition
} DEVICE_ACTION, *PDEVICE_ACTION;

/*++

Structure Description:

    This structure defines a unit of work on a device. A queue of these work
    items are queued on a per-device basis.

Members:

    ListEntry - Stores pointers to the next and previous entries in the
        queue.

    Action - Stores the action to perform on the device.

    Flags - Stores properties and options for the action. See DEVICE_ACTION_*
        definitions.

    Parameter - Stores a caller-supplied parameter that has different meanings
        depending on the type of work requested.

--*/

typedef struct _DEVICE_WORK_ENTRY {
    LIST_ENTRY ListEntry;
    DEVICE_ACTION Action;
    ULONG Flags;
    PVOID Parameter;
} DEVICE_WORK_ENTRY, *PDEVICE_WORK_ENTRY;

/*++

Structure Description:

    This structure defines an entry in the driver stack for a device. A device
    contains one or more drivers from the functional driver to various filters,
    with a bus driver at the bottom.

Members:

    ListEntry - Stores pointers to the next and previous entries in the
        stack.

    Driver - Stores a pointer to the driver associated with this driver
        stack entry.

    DriverContext - Stores a pointer supplied by the driver on AddDevice.
        This pointer will be passed to the driver each time it is asked to
        operate on this device. It is typically used to store device context.

    Flags - Stores a set of flags associated with this stack entry. See
        DRIVER_STACK_* definitions.

--*/

typedef struct _DRIVER_STACK_ENTRY {
    LIST_ENTRY ListEntry;
    PDRIVER Driver;
    PVOID DriverContext;
    ULONG Flags;
} DRIVER_STACK_ENTRY, *PDRIVER_STACK_ENTRY;

typedef enum _DEVICE_STATE {
    DeviceStateInvalid,
    DeviceUnreported,
    DeviceInitialized,
    DeviceDriversAdded,
    DeviceResourcesQueried,
    DeviceResourceAssignmentQueued,
    DeviceResourcesAssigned,
    DeviceAwaitingEnumeration,
    DeviceEnumerated,
    DeviceStarted,
    DeviceAwaitingRemoval,
    DeviceRemoved
} DEVICE_STATE, *PDEVICE_STATE;

typedef enum _DEVICE_QUEUE_STATE {
    DeviceQueueInvalid,
    DeviceQueueOpen,
    DeviceQueueActive,
    DeviceQueueActiveClosing,
    DeviceQueueClosed
} DEVICE_QUEUE_STATE, *PDEVICE_QUEUE_STATE;

typedef enum _DEVICE_PROBLEM {
    DeviceProblemNone,
    DeviceProblemNoDrivers,
    DeviceProblemFailedDriverLoad,
    DeviceProblemNoAddDevice,
    DeviceProblemNoFileSystem,
    DeviceProblemFailedAddDevice,
    DeviceProblemInvalidState,
    DeviceProblemFailedToQueueResourceAssignmentWork,
    DeviceProblemFailedQueryResources,
    DeviceProblemResourceConflict,
    DeviceProblemFailedStart,
    DeviceProblemFailedQueryChildren,
    DeviceProblemFailedToQueueStart,
    DeviceProblemFailedToQueueQueryChildren,
    DeviceProblemFailedToQueuePrepareRemove,
    DeviceProblemFailedToQueueRemove,
    DeviceProblemFailedToSendRemoveIrp,
    DeviceProblemFailedVolumeArrival,
    DeviceProblemFailedVolumeRemoval,
    DeviceProblemFailedPathRemoval,
    DeviceProblemDriverError
} DEVICE_PROBLEM, *PDEVICE_PROBLEM;

/*++

Structure Description:

    This structure defines a device problem state.

Members:

    Problem - Stores a device problem code.

    Status - Stores the failure status associated with the device problem.

    DriverCode - Stores a driver-specific error code.

    ProblemLine - Stores the line number of the source file where the problem
        was set.

    ProblemFile - Stores a pointer to a string containing the name of the
        source file where the problem was set.

    Driver - Stores a pointer to the driver that reported the device problem.
        The field is NULL if the system reported the problem.

--*/

typedef struct _DEVICE_PROBLEM_STATE {
    DEVICE_PROBLEM Problem;
    KSTATUS Status;
    ULONG DriverCode;
    ULONG Line;
    PCSTR File;
    PDRIVER Driver;
} DEVICE_PROBLEM_STATE, *PDEVICE_PROBLEM_STATE;

/*++

Structure Description:

    This structure defines a device.

Members:

    Header - Stores the object header for this device, including the device's
        name.

    ListEntry - Stores pointers to the next and previous devices in the global
        list.

    State - Stores the current state of the device.

    StateHistoryNextIndex - Stores the index of where the next device state
        should be written to. The state history is a circular buffer.

    StateHistory - Stores a log containing the history of the last few device
        states.

    DeviceId - Stores the numeric identifier for the device.

    ActiveChildList - Store the head of the list of this device's active
        children.

    ActiveListEntry - Stores the list entry for the device's place in its
        parent's list of active children.

    Lock - Stores a pointer to a shared exclusive lock that synchronizes
        device removal with IRPs. Lock order is always parent, then child.

    ParentDevice - Stores a pointer to the device that created this device, aka
        the device's parent. Usually this points to the parent bus. This can
        be NULL for unenumerable devices.

    TargetDevice - Stores a pointer to a device that IRPs continue through
        if they've not been completed by this device stack.

    ClassId - Stores a pointer to a string containing the class ID for the
        device.

    CompatibleIds - Stores a pointer to a string containing the compatible IDs
        for this device.

    QueueLock - Stores a pointer to a queued lock that protects the work
        queue's state and list.

    QueueState - Stores the state of the work queue, describing whether or not
        it is accepting new requests. Writes of this variable are protected by
        the QueueLock.

    WorkQueue - Stores the list head of the device work queue. Access to this
        list is protected by the QueueLock. See the DEVICE_WORK_ENTRY
        definition.

    DriverStackHead - Stores the list head for the driver stack. The Next link
        of this head points to the top of the driver stack (the functional
        driver or uppermost filter).

    DriverStackSize - Stores the number of drivers in the driver stack.

    Flags - Stores device flags. See DEVICE_FLAG_* definitions.

    ProblemState - Stores device problem information reported by the system or
        a device driver.

    ArbiterListHead - Stores the head of the list of arbiters this device is
        responsible for.

    ResourceRequirements - Stores a pointer to set of possible resource
        configurations for the device.

    ArbiterAllocationListHead - Stores the head of the list of allocations
        assigned to the device by the arbiter.

    SelectedConfiguration - Stores a pointer to the configuration that was
        selected in the device's resource configuration list.

    BusLocalResources - Stores a pointer to the device's committed resources, as
        seen from the point of view of the device itself. These
        are the resources that the bus driver is likely use when programming
        things like the device's Base Address Registers.

    ProcessorLocalResources - Stores a pointer to the device's committed
        resources, as seen from the CPU complex. These are the resources the
        device driver would use to communicate with the device.

    BootResources - Stores a pointer to the resources the BIOS assigned to the
        device at boot.

    Power - Stores the power management information for the device.

--*/

struct _DEVICE {
    OBJECT_HEADER Header;
    LIST_ENTRY ListEntry;
    DEVICE_STATE State;
    ULONG StateHistoryNextIndex;
    DEVICE_STATE StateHistory[DEVICE_STATE_HISTORY];
    DEVICE_ID DeviceId;
    LIST_ENTRY ActiveChildListHead;
    LIST_ENTRY ActiveListEntry;
    PSHARED_EXCLUSIVE_LOCK Lock;
    PDEVICE ParentDevice;
    PDEVICE TargetDevice;
    PSTR ClassId;
    PSTR CompatibleIds;
    PQUEUED_LOCK QueueLock;
    DEVICE_QUEUE_STATE QueueState;
    LIST_ENTRY WorkQueue;
    LIST_ENTRY DriverStackHead;
    ULONG DriverStackSize;
    ULONG Flags;
    DEVICE_PROBLEM_STATE ProblemState;
    LIST_ENTRY ArbiterListHead;
    PRESOURCE_CONFIGURATION_LIST ResourceRequirements;
    LIST_ENTRY ArbiterAllocationListHead;
    PRESOURCE_REQUIREMENT_LIST SelectedConfiguration;
    PRESOURCE_ALLOCATION_LIST BusLocalResources;
    PRESOURCE_ALLOCATION_LIST ProcessorLocalResources;
    PRESOURCE_ALLOCATION_LIST BootResources;
    PDEVICE_POWER Power;
};

/*++

Structure Description:

    This structure defines a volume device.

Members:

    Device - Stores the data required for a standard device.

    Flags - Stores a bitmask of volume specific flags.

    ReferenceCount - Stores the number of references taken on the volume. This
        is used to track the number of mount points that target the volume.
        As such, volume creation does not set a reference count of 1.

    PathEntry - Stores a pointer to the anonymous path entry associated with
        the volume.

--*/

struct _VOLUME {
    DEVICE Device;
    ULONG Flags;
    volatile ULONG ReferenceCount;
    PPATH_ENTRY PathEntry;
};

/*++

Structure Description:

    This structure defines a driver object.

Members:

    Image - Stores a pointer to the driver's image.

    FunctionTable - Stores the driver's registered function pointers.

    Flags - Stores various state of the driver. See DRIVER_FLAG_* definitions.

--*/

struct _DRIVER {
    PVOID Image;
    DRIVER_FUNCTION_TABLE FunctionTable;
    ULONG Flags;
};

typedef
KSTATUS
(*PFILE_OBJECT_ITERATION_ROUTINE) (
    PFILE_OBJECT FileObject,
    PVOID Context
    );

/*++

Routine Description:

    This routine is called once for every file object in the file object list.

Arguments:

    FileObject - Supplies a pointer to the current file object.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines the parameters associated with a creation request.

Members:

    Type - Stores the type of object to create.

    Context - Stores a pointer that contains additional context specific to a
        given device type.

    Permissions - Stores the creation permissions to assign.

    Created - Stores a boolean that will get set to TRUE if the file was
        created.

--*/

typedef struct _CREATE_PARAMETERS {
    IO_OBJECT_TYPE Type;
    PVOID Context;
    FILE_PERMISSIONS Permissions;
    BOOL Created;
} CREATE_PARAMETERS, *PCREATE_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// Store list heads to the device databases. These list entries are of type
// DEVICE_DATABASE_ENTRY, and store the mappings between devices and drivers
// or device classes and drivers. All memory in these databases is paged.
//

extern LIST_ENTRY IoDeviceDatabaseHead;
extern LIST_ENTRY IoDeviceClassDatabaseHead;
extern PQUEUED_LOCK IoDeviceDatabaseLock;

//
// Store a pointer to the device work queue.
//

extern PWORK_QUEUE IoDeviceWorkQueue;

//
// Define the object that roots the device tree.
//

extern PDEVICE IoRootDevice;
extern LIST_ENTRY IoDeviceList;
extern PQUEUED_LOCK IoDeviceListLock;

//
// Store the number of active work items flowing around.
//

extern UINTN IoDeviceWorkItemsQueued;

//
// Store the array of devices that were delayed until the initial enumeration
// was complete.
//

extern PDEVICE *IoDelayedDevices;
extern UINTN IoDelayedDeviceCount;

//
// Store a pointer to the directory of all exposed interfaces.
//

extern POBJECT_HEADER IoInterfaceDirectory;
extern PQUEUED_LOCK IoInterfaceLock;

//
// Keep the list of registered file systems.
//

extern LIST_ENTRY IoFileSystemList;

//
// This lock synchronizes access to the list of file systems.
//

extern PQUEUED_LOCK IoFileSystemListLock;

//
// Store a pointer to the volumes directory and the number of volumes in the
// system.
//

extern POBJECT_HEADER IoVolumeDirectory;

//
// Store a pointer to the parent object of all IRPs.
//

extern POBJECT_HEADER IoIrpDirectory;

//
// Store a pointer to the pipes directory.
//

extern POBJECT_HEADER IoPipeDirectory;

//
// Store the saved boot information.
//

extern IO_BOOT_INFORMATION IoBootInformation;

//
// Store the global I/O statistics.
//

extern IO_GLOBAL_STATISTICS IoGlobalStatistics;

//
// Store the root path point.
//

extern PATH_POINT IoPathPointRoot;

//
// Store the shared memory object root path point.
//

extern PATH_POINT IoSharedMemoryRoot;

//
// Store a pointer to the mount lock.
//

extern PSHARED_EXCLUSIVE_LOCK IoMountLock;

//
// Store the path to the system directory on the system volume.
//

extern PSTR IoSystemDirectoryPath;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
IopInitializeDriver (
    PVOID LoadedImage
    );

/*++

Routine Description:

    This routine is called to initialize a newly loaded driver. This routine
    should only be called internally by the system.

Arguments:

    LoadedImage - Supplies a pointer to the image associated with the driver.

Return Value:

    Status code.

--*/

KSTATUS
IopCreateDevice (
    PDRIVER BusDriver,
    PVOID BusDriverContext,
    PDEVICE ParentDevice,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    OBJECT_TYPE DeviceType,
    ULONG DeviceSize,
    PDEVICE *NewDevice
    );

/*++

Routine Description:

    This routine creates a new device or volume.

Arguments:

    BusDriver - Supplies a pointer to the driver reporting this device.

    BusDriverContext - Supplies the context pointer that will be passed to the
        bus driver when IRPs are sent to the device.

    ParentDevice - Supplies a pointer to the device enumerating this device.
        Most devices are enumerated off of a bus, so this parameter will
        contain a pointer to that bus device. For unenumerable devices, this
        parameter can be NULL, in which case the device will be enumerated off
        of the root device.

    DeviceId - Supplies a pointer to a null terminated string identifying the
        device. This memory does not have to be retained, a copy of it will be
        created during this call.

    ClassId - Supplies a pointer to a null terminated string identifying the
        device class. This memory does not have to be retained, a copy of it
        will be created during this call.

    CompatibleIds - Supplies a semicolon-delimited list of device IDs that this
        device is compatible with.

    DeviceType - Supplies the type of the new device.

    DeviceSize - Supplies the size of the new device's data structure.

    NewDevice - Supplies a pointer where the new device or volume will be
        returned on success.

Return Value:

    Status code.

--*/

VOID
IopSetDeviceState (
    PDEVICE Device,
    DEVICE_STATE NewState
    );

/*++

Routine Description:

    This routine sets the device to a new state.

Arguments:

    Device - Supplies a pointer to the device whose state is to be changed.

    NewState - Supplies a pointer to the new device state.

Return Value:

    None.

--*/

KSTATUS
IopQueueDeviceWork (
    PDEVICE Device,
    DEVICE_ACTION Action,
    PVOID Parameter,
    ULONG Flags
    );

/*++

Routine Description:

    This routine queues work on a device.

Arguments:

    Device - Supplies a pointer to the device to queue work on.

    Action - Supplies the work to be done on that device.

    Parameter - Supplies a parameter that accompanies the action. The meaning
        of this parameter changes with the type of work queued.

    Flags - Supplies a set of flags and options about the work.
        See DEVICE_ACTION_* definitions.

Return Value:

    STATUS_SUCCESS if the request was queued on at least one device.

    STATUS_NO_ELIGIBLE_DEVICES if the request could not be queued because the
        devices are not accepting work.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

    Other error codes on other failures.

--*/

VOID
IopHandleDeviceQueueFailure (
    PDEVICE Device,
    DEVICE_ACTION Action
    );

/*++

Routine Description:

    This routine handles a failure to add a work item to a device queue.

Arguments:

    Device - Supplies a pointer to the device that could not accept the action.

    Action - Supplies the action that failed to be added to the given device's
        work queue.

Return Value:

    None.

--*/

VOID
IopSetDeviceProblemEx (
    PDEVICE Device,
    DEVICE_PROBLEM Problem,
    KSTATUS Status,
    PDRIVER Driver,
    ULONG DriverCode,
    PCSTR SourceFile,
    ULONG LineNumber
    );

/*++

Routine Description:

    This routine sets a device problem code on a given device. This problem is
    usually preventing a device from starting or otherwise making forward
    progress. Avoid calling this function directly, use the non-Ex version.

Arguments:

    Device - Supplies a pointer to the device with the problem.

    Problem - Supplies the problem with the device.

    Status - Supplies the failure status generated by the error.

    Driver - Supplies a pointer to the driver reporting the error. This
        parameter is optional.

    DriverCode - Supplies an optional problem driver-specific error code.

    SourceFile - Supplies a pointer to the source file where the problem
        occurred. This is usually automatically generated by the compiler.

    LineNumber - Supplies the line number in the source file where the problem
        occurred. This is usually automatically generated by the compiler.

Return Value:

    None.

--*/

VOID
IopClearDeviceProblem (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine clears any problem code associated with a device.

Arguments:

    Device - Supplies a pointer to the device whose problem code should be
        cleared.

Return Value:

    None.

--*/

VOID
IopPrepareRemoveDevice (
    PDEVICE Device,
    PDEVICE_WORK_ENTRY Work
    );

/*++

Routine Description:

    This routine prepares a device for removal. It puts the device in the
    awaiting removal state. If it has no children, it queues the removal work
    item on itself. If this routine discovers that the device is already in the
    awaiting removal state, it exits.

Arguments:

    Device - Supplies a pointer to the device that is preparing to be removed.

    Work - Supplies a pointer to the work request.

Return Value:

    None.

--*/

VOID
IopRemoveDevice (
    PDEVICE Device,
    PDEVICE_WORK_ENTRY Work
    );

/*++

Routine Description:

    This routine removes a device by sending a removal IRP and then removing
    the device reference added during device creation. The removal IRP allows
    the driver to clean up any necessary state that cannot be cleaned up by the
    object manager's destruction call-back.

Arguments:

    Device - Supplies a pointer to the device that is to be removed.

    Work - Supplies a pointer to the work request.

Return Value:

    None.

--*/

VOID
IopAbortDeviceRemoval (
    PDEVICE Device,
    DEVICE_PROBLEM DeviceProblem,
    BOOL RollbackDevice
    );

/*++

Routine Description:

    This routine aborts the device removal process for the given device. It
    also walks back up the device tree reverting the removal process for any
    ancestor devices that were awaiting the given device's removal.

Arguments:

    Device - Supplies a pointer to the device that failed the removal process
        and requires rollback.

    DeviceProblem - Supplies the devices problem (i.e. the reason for the
        abort).

    RollbackDevice - Supplies a boolean indicating whether or not the supplied
        device should be included in the rollback.

Return Value:

    None.

--*/

VOID
IopDestroyDevice (
    PVOID Object
    );

/*++

Routine Description:

    This routine destroys a device and its resources. The object manager will
    clean up the object header, leaving this routine to clean up the device
    specific elements of the object. This routine is meant only as a callback
    for the object manager.

Arguments:

    Object - Supplies a pointer to the object to be destroyed.

Return Value:

    None.

--*/

DEVICE_ID
IopGetNextDeviceId (
    VOID
    );

/*++

Routine Description:

    This routine allocates and returns a device ID.

Arguments:

    None.

Return Value:

    Returns a unique device ID number.

--*/

KSTATUS
IopInitializeDeviceInformationSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes device information support.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
IopAddFileSystem (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine adds a file system to the given volume.

Arguments:

    Device - Supplies a pointer to the volume to attach a file system to.

Return Value:

    Status code.

--*/

KSTATUS
IopCreateOrLookupVolume (
    PDEVICE Device,
    PVOLUME *Volume
    );

/*++

Routine Description:

    This routine returns the volume associated with the given device, if such
    a volume exists. A reference is taken on the volume, which the caller is
    expected to release.

Arguments:

    Device - Supplies a pointer to the device whose volume is to be returned.

    Volume - Supplies a pointer that receives a pointer to the created or found
        volume.

Return Value:

    Status code.

--*/

VOID
IopVolumeArrival (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine performs work associated with a new volume coming online.

Arguments:

    Parameter - Supplies a pointer to the arriving volume.

Return Value:

    None.

--*/

KSTATUS
IopRemoveDevicePaths (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine takes the device's paths offline.

Arguments:

    Device - Supplies a pointer to the departing device.

Return Value:

    Status code.

--*/

KSTATUS
IopInitializeMountPointSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes the support for mount points.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
IopGetSetMountPointInformation (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets mount point information.

Arguments:

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
IopRemoveMountPoints (
    PPATH_POINT RootPath
    );

/*++

Routine Description:

    This routine lazily unmounts all the mount points that exist under the
    given root path point, including itself.

Arguments:

    RootPath - Supplies a pointer to the root path point, under which all
        mount points will be removed.

Return Value:

    None.

--*/

PMOUNT_POINT
IopFindMountPoint (
    PMOUNT_POINT Parent,
    PPATH_ENTRY PathEntry
    );

/*++

Routine Description:

    This routine searches for a child mount point of the given parent whose
    mount path entry matches the given path entry. If found, a reference is
    taken on the returned mount point.

Arguments:

    Parent - Supplies a pointer to a parent mount point whose children are
        searched for a mount point that matches the path entry.

    PathEntry - Supplies a pointer to a path entry to search for.

Return Value:

    Returns a pointer to the found mount point on success, or NULL on failure.

--*/

PMOUNT_POINT
IopGetMountPointParent (
    PMOUNT_POINT MountPoint
    );

/*++

Routine Description:

    This routine gets a mount point's parent. The parent can disappear at any
    moment with a lazy unmount, so this routine acquires the mount lock in
    shared mode to check the parent.

Arguments:

    MountPoint - Supplies a pointer to a mount point.

Return Value:

    Returns a pointer to the parent mount point on success, or NULL otherwise.

--*/

KSTATUS
IopOpen (
    BOOL FromKernelMode,
    PIO_HANDLE Directory,
    PCSTR Path,
    ULONG PathLength,
    ULONG Access,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PIO_HANDLE *Handle
    );

/*++

Routine Description:

    This routine opens a file, device, pipe, or other I/O object.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Directory - Supplies an optional pointer to a handle to a directory to use
        if the given path is relative. Supply NULL to use the current working
        directory.

    Path - Supplies a pointer to the path to open.

    PathLength - Supplies the length of the path buffer in bytes, including the
        null terminator.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Create - Supplies an optional pointer to the creation information. If the
        OPEN_FLAG_CREATE is supplied in the flags, then this field is required.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopOpenPathPoint (
    PPATH_POINT PathPoint,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle
    );

/*++

Routine Description:

    This routine opens a path point object. This routine must be called
    carefully by internal functions, as it skips all permission checks.

Arguments:

    PathPoint - Supplies a pointer to the path point to open. Upon success this
        routine will add a reference to the path point's path entry and mount
        point.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopOpenDevice (
    PDEVICE Device,
    ULONG Access,
    ULONG Flags,
    PIO_HANDLE *Handle
    );

/*++

Routine Description:

    This routine opens a device or volume.

Arguments:

    Device - Supplies a pointer to a device to open.

    Access - Supplies the desired access permissions to the object. See
        IO_ACCESS_* definitions.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Handle - Supplies a pointer where a pointer to the open I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopCreateSpecialIoObject (
    BOOL FromKernelMode,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    );

/*++

Routine Description:

    This routine creates a special file object.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where a pointer to the new file object
        will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopClose (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine shuts down an I/O handle that is about to be destroyed.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle returned when the file was
        opened.

Return Value:

    Status code.

--*/

KSTATUS
IopDeleteByHandle (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    ULONG Flags
    );

/*++

Routine Description:

    This routine attempts to delete the the object open by the given I/O
    handle. This does not close or invalidate the handle, but it does attempt
    to unlink the object so future path walks will not find it at that location.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Handle - Supplies the open handle to the device.

    Flags - Supplies a bitfield of flags. See DELETE_FLAG_* definitions.

Return Value:

    Status code.

--*/

KSTATUS
IopDeletePathPoint (
    BOOL FromKernelMode,
    PPATH_POINT PathPoint,
    ULONG Flags
    );

/*++

Routine Description:

    This routine attempts to delete the object at the given path. If the path
    points to a directory, the directory must be empty. If the path point is
    a file object or shared memory object, its hard link count is decremented.
    If the hard link count reaches zero and no processes have the object open,
    the contents of the object are destroyed. If processes have open handles to
    the object, the destruction of the object contents are deferred until the
    last handle to the old file is closed.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    PathPoint - Supplies a pointer to the path point to delete. The caller
        should already have a reference on this path point, which will need to
        be released by the caller when finished.

    Flags - Supplies a bitfield of flags. See DELETE_FLAG_* definitions.

Return Value:

    Status code.

--*/

KSTATUS
IopSendFileOperationIrp (
    IRP_MINOR_CODE MinorCode,
    PFILE_OBJECT FileObject,
    PVOID DeviceContext,
    ULONG Flags
    );

/*++

Routine Description:

    This routine sends a file operation IRP.

Arguments:

    MinorCode - Supplies the minor code of the IRP to send.

    FileObject - Supplies a pointer to the file object of the file being
        operated on.

    DeviceContext - Supplies a pointer to the device context to send down.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

Return Value:

    Status code.

--*/

KSTATUS
IopSendLookupRequest (
    PDEVICE Device,
    PFILE_OBJECT Directory,
    PCSTR FileName,
    ULONG FileNameSize,
    PFILE_PROPERTIES Properties,
    PULONG Flags,
    PULONG MapFlags
    );

/*++

Routine Description:

    This routine sends a lookup request IRP. This routine assumes that the
    directory's lock is held exclusively.

Arguments:

    Device - Supplies a pointer to the device to send the request to.

    Directory - Supplies a pointer to the file object of the directory to
        search in.

    FileName - Supplies a pointer to the name of the file, which may not be
        null terminated.

    FileNameSize - Supplies the size of the file name buffer including space
        for a null terminator (which may be a null terminator or may be a
        garbage character). Supply 0 to perform a root lookup request.

    Properties - Supplies a pointer where the file properties will be returned
        if the file was found.

    Flags - Supplies a pointer where the translated file object flags will be
        returned. See FILE_OBJECT_FLAG_* definitions.

    MapFlags - Supplies a pointer where the required map flags associated with
        this file object will be returned. See MAP_FLAG_* definitions.

Return Value:

    Status code.

--*/

KSTATUS
IopSendCreateRequest (
    PDEVICE Device,
    PFILE_OBJECT Directory,
    PCSTR Name,
    ULONG NameSize,
    PFILE_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine sends a creation request to the device.

Arguments:

    Device - Supplies a pointer to the device to send the request to.

    Directory - Supplies a pointer to the file object of the directory to
        create the file in.

    Name - Supplies a pointer to the name of the file or directory to create,
        which may not be null terminated.

    NameSize - Supplies the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a  garbage
        character).

    Properties - Supplies a pointer to the file properties of the created file
        on success. The permissions, object type, user ID, group ID, and access
        times are all valid from the system.

Return Value:

    Status code.

--*/

KSTATUS
IopSendUnlinkRequest (
    PDEVICE Device,
    PFILE_OBJECT FileObject,
    PFILE_OBJECT DirectoryObject,
    PCSTR Name,
    ULONG NameSize,
    PBOOL Unlinked
    );

/*++

Routine Description:

    This routine sends an unlink request to the device. This routine assumes
    that the directory's lock is held exclusively.

Arguments:

    Device - Supplies a pointer to the device to send the request to.

    FileObject - Supplies a pointer to the file object of the file that is to
        be unlinked.

    DirectoryObject - Supplies a pointer to the file object of the directory
        from which the file will be unlinked.

    Name - Supplies a pointer to the name of the file or directory to create,
        which may not be null terminated.

    NameSize - Supplies the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a  garbage
        character).

    Unlinked - Supplies a boolean that receives whether or not the file was
        unlinked. The file may be unlinked even if the call fails.

Return Value:

    Status code.

--*/

KSTATUS
IopPerformCacheableIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    );

/*++

Routine Description:

    This routine reads from or writes to the given handle. The I/O object type
    of the given handle must be cacheable.

Arguments:

    Handle - Supplies a pointer to the I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

KSTATUS
IopPerformNonCachedRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext
    );

/*++

Routine Description:

    This routine performs a non-cached read from a cacheable file object. It is
    assumed that the file lock is held.

Arguments:

    FileObject - Supplies a pointer to a cacheable file object.

    IoContext - Supplies a pointer to the I/O context.

    DeviceContext - Supplies a pointer to the device context to use when
        writing to the backing device.

Return Value:

    Status code.

--*/

KSTATUS
IopPerformNonCachedWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext
    );

/*++

Routine Description:

    This routine performs a non-cached write to a cacheable file object. It is
    assumed that the file lock is held. This routine will always modify the
    file size in the the file properties and conditionally modify the file size
    in the file object.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

    DeviceContext - Supplies a pointer to the device context to use when
        writing to the backing device.

Return Value:

    Status code.

--*/

KSTATUS
IopPerformObjectIoOperation (
    PIO_HANDLE IoHandle,
    PIO_CONTEXT IoContext
    );

/*++

Routine Description:

    This routine reads from an object directory.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

KSTATUS
IopCreateIoHandle (
    PIO_HANDLE *Handle
    );

/*++

Routine Description:

    This routine creates a new I/O handle with a reference count of one.

Arguments:

    Handle - Supplies a pointer where a pointer to the new I/O handle will be
        returned on success.

Return Value:

    Status code.

--*/

VOID
IopOverwriteIoHandle (
    PIO_HANDLE Destination,
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine overwrites the file object of the given handle. I/O actions
    performed on the destination handle go to the given file object. This
    routine is not thread safe.

Arguments:

    Destination - Supplies a pointer to the I/O handle that should magically
        redirect elsewhere.

    FileObject - Supplies a pointer to the file object to place in the handle.

Return Value:

    None.

--*/

KSTATUS
IopInitializeFileObjectSupport (
    VOID
    );

/*++

Routine Description:

    This routine performs global initialization for file object support.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
IopCreateOrLookupFileObject (
    PFILE_PROPERTIES Properties,
    PDEVICE Device,
    ULONG Flags,
    ULONG MapFlags,
    PFILE_OBJECT *FileObject,
    PBOOL ObjectCreated
    );

/*++

Routine Description:

    This routine attempts to look up a file object with the given properties
    (specifically the I-Node number and volume). If one does not exist, it
    is created and inserted in the global list. If a special file object is
    created, the ready event is left unsignaled so the remainder of the state
    can be created.

Arguments:

    Properties - Supplies a pointer to the file object properties.

    Device - Supplies a pointer to the device that owns the file serial number.
        This may also be a volume or an object directory.

    Flags - Supplies a bitmask of file object flags. See FILE_OBJECT_FLAG_* for
        definitions.

    MapFlags - Supplies the additional map flags associated with this file
        object. See MAP_FLAG_* definitions.

    FileObject - Supplies a pointer where the file object will be returned on
        success.

    ObjectCreated - Supplies a pointer where a boolean will be returned
        indicating whether or not the object was just created. If it was just
        created, the caller is responsible for signaling the ready event when
        the object is fully set up.

Return Value:

    Status code.

--*/

ULONG
IopFileObjectAddReference (
    PFILE_OBJECT Object
    );

/*++

Routine Description:

    This routine increments the reference count on a file object.

Arguments:

    Object - Supplies a pointer to the object to retain.

Return Value:

    Returns the reference count before the addition.

--*/

KSTATUS
IopFileObjectReleaseReference (
    PFILE_OBJECT Object
    );

/*++

Routine Description:

    This routine decrements the reference count on a file object. If the
    reference count hits zero, then the file object will be destroyed.

Arguments:

    Object - Supplies a pointer to the object to release.

    FailIfLastReference - Supplies a boolean that if set causes the reference
        count not to be decremented if this would involve releasing the very
        last reference on the file object. Callers that set this flag must be
        able to take responsibility for the reference they continue to own in
        the failure case. Set this to FALSE.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure to write out the file properties to the file
    system or device.

--*/

VOID
IopFileObjectAddPathEntryReference (
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine increments the path entry reference count on a file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    None.

--*/

VOID
IopFileObjectReleasePathEntryReference (
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine decrements the path entry reference count on a file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    None.

--*/

KSTATUS
IopFlushFileObject (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset,
    ULONGLONG Size,
    ULONG Flags,
    BOOL FlushExclusive,
    PUINTN PageCount
    );

/*++

Routine Description:

    This routine flushes all file object data to the next lowest cache layer.
    If the flags request synchronized I/O, then all file data and meta-data
    will be flushed to the backing media.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file.

    Offset - Supplies the offset from the beginning of the file or device where
        the flush should be done.

    Size - Supplies the size, in bytes, of the region to flush. Supply a value
        of -1 to flush from the given offset to the end of the file.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

    FlushExclusive - Supplies a boolean indicating if this was an explicit
        flush. If so, then the flush lock is acquired exclusively to prevent
        partial flushes due to dirty page cache entries being on a local list.

    PageCount - Supplies an optional pointer describing how many pages to flush.
        On output this value will be decreased by the number of pages actually
        flushed. Supply NULL to flush all pages in the size range.

Return Value:

    Status code.

--*/

KSTATUS
IopFlushFileObjects (
    DEVICE_ID DeviceId,
    ULONG Flags,
    PUINTN PageCount
    );

/*++

Routine Description:

    This routine iterates over file objects in the global dirty file objects
    list, flushing each one that belongs to the given device or to all entries
    if a device ID of 0 is specified.

Arguments:

    DeviceId - Supplies an optional device ID filter. Supply 0 to iterate over
        dirty file objects for all devices.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

    PageCount - Supplies an optional pointer describing how many pages to flush.
        On output this value will be decreased by the number of pages actually
        flushed. Supply NULL to flush all pages.

Return Value:

    STATUS_SUCCESS if all file object were successfully iterated.

    STATUS_TRY_AGAIN if the iteration quit early for some reason (i.e. the page
    cache was found to be too dirty when flushing file objects).

    Other status codes for other errors.

--*/

VOID
IopEvictFileObject (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset,
    ULONG Flags
    );

/*++

Routine Description:

    This routine evicts the tail of a file object from the system. It unmaps
    all page cache entries used by image sections after the given offset and
    evicts all page cache entries after the given offset. If the remove or
    truncate flags are specified, this routine actually unmaps all mappings for
    the image sections after the given offset, not just the mapped page cache
    entries. This routine assumes the file object's lock is held exclusively.

Arguments:

    FileObject - Supplies a pointer to the file object to evict.

    Offset - Supplies the starting offset into the file or device after which
        all page cache entries should be evicted and all image sections should
        be unmapped.

    Flags - Supplies a bitmask of eviction flags. See EVICTION_FLAG_* for
        definitions.

Return Value:

    None.

--*/

VOID
IopEvictFileObjects (
    DEVICE_ID DeviceId,
    ULONG Flags
    );

/*++

Routine Description:

    This routine iterates over all file objects evicting page cache entries for
    each one that belongs to the given device.

Arguments:

    DeviceId - Supplies an optional device ID filter. Supply 0 to iterate over
        file objects for all devices.

    Flags - Supplies a bitmask of eviction flags. See EVICTION_FLAG_* for
        definitions.

Return Value:

    None.

--*/

KSTATUS
IopFlushAllFileObjectsProperties (
    BOOL StopIfTooDirty
    );

/*++

Routine Description:

    This routine flushes all dirty file object properties.

Arguments:

    StopIfTooDirty - Supplies a boolean that if set indicates this routine
        should stop flushing file object properties if the page cache is too
        dirty.

Return Value:

    STATUS_SUCCESS if all file object properties were successfully flushed, or
    the device ID was zero.

    STATUS_TRY_AGAIN if the page cache became too dirty and the function quit
    early.

    Other status codes for other errors.

--*/

KSTATUS
IopFlushFileObjectPropertiesByDevice (
    DEVICE_ID DeviceId,
    BOOL StopIfTooDirty
    );

/*++

Routine Description:

    This routine flushes the file properties for all the file objects that
    belong to a given device. If no device ID is specified, it flushes the file
    objects for all devices.

Arguments:

    DeviceId - Supplies the ID of the device whose file objects' properties are
        to be flushed.

    StopIfTooDirty - Supplies a boolean that if set indicates this routine
        should stop flushing file object properties if the page cache is too
        dirty.

Return Value:

    STATUS_SUCCESS if all file object properties were successfully flushed, or
    the device ID was zero.

    STATUS_TRY_AGAIN if the page cache became too dirty and the function quit
    early.

--*/

VOID
IopUpdateFileObjectTime (
    PFILE_OBJECT FileObject,
    FILE_OBJECT_TIME_TYPE TimeType
    );

/*++

Routine Description:

    This routine updates the given file object's access and modified times. The
    latter is only updated upon request.

Arguments:

    FileObject - Supplies a pointer to a file object.

    TimeType - Supplies the type of time to update. Updating modified time also
        updates status change time.

Return Value:

    None.

--*/

VOID
IopUpdateFileObjectFileSize (
    PFILE_OBJECT FileObject,
    ULONGLONG NewSize
    );

/*++

Routine Description:

    This routine will make sure the file object file size is at least the
    given size. If it is not, it will be set to the given size. If it is, no
    change will be performed. Use the modify file object size function to
    forcibly set a new size (eg for truncate).

Arguments:

    FileObject - Supplies a pointer to a file object.

    NewSize - Supplies the new file size.

Return Value:

    None.

--*/

KSTATUS
IopModifyFileObjectSize (
    PFILE_OBJECT FileObject,
    PVOID DeviceContext,
    ULONGLONG NewFileSize
    );

/*++

Routine Description:

    This routine modifies the given file object's size. It will either increase
    or decrease the file size. If the size is decreased then the file object's
    driver will be notified, any existing page cache entries for the file will
    be evicted and any image sections that map the file will be unmapped.

Arguments:

    FileObject - Supplies a pointer to the file object whose size will be
        modified.

    DeviceContext - Supplies an optional pointer to the device context to use
        when doing file operations. Not every file object has a built-in device
        context.

    NewFileSize - Supplies the desired new size of the file object.

Return Value:

    Status code.

--*/

VOID
IopFileObjectIncrementHardLinkCount (
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine decrements the hard link count for a file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    None.

--*/

VOID
IopFileObjectDecrementHardLinkCount (
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine decrements the hard link count for a file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    None.

--*/

VOID
IopCleanupFileObjects (
    VOID
    );

/*++

Routine Description:

    This routine releases any lingering file objects that were left around as a
    result of I/O failures during the orignal release attempt.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
IopAcquireFileObjectLocksExclusive (
    PFILE_OBJECT Object1,
    PFILE_OBJECT Object2
    );

/*++

Routine Description:

    This routine acquires two file object locks exclusive in the right order.
    The order is to sort first by file object type, then by file object pointer.

Arguments:

    Object1 - Supplies a pointer to the first file object.

    Object2 - Supplies a pointer to the second file object.

Return Value:

    None.

--*/

PIMAGE_SECTION_LIST
IopGetImageSectionListFromFileObject (
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine gets the image section for the given file object.

Arguments:

    FileObject - Supplies a pointer to a file object.

Return Value:

    Returns a pointer to the file object's image section list or NULL on
    failure.

--*/

VOID
IopMarkFileObjectDirty (
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine marks the given file object as dirty, moving it to the list of
    dirty file objects if it is not already on a list.

Arguments:

    FileObject - Supplies a pointer to the dirty file object.

Return Value:

    None.

--*/

VOID
IopMarkFileObjectPropertiesDirty (
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine marks that the given file object's properties are dirty.

Arguments:

    FileObject - Supplies a pointer to the file object whose properties are
        dirty.

Return Value:

    None.

--*/

VOID
IopCheckDirtyFileObjectsList (
    VOID
    );

/*++

Routine Description:

    This routine iterates over all file objects, checking to make sure they're
    properly marked dirty and in the dirty list if they have dirty entries.
    This routine is slow and should only be used while actively debugging
    dirty data that won't flush.

Arguments:

    None.

Return Value:

    None.

--*/

PIO_ASYNC_STATE
IopGetAsyncState (
    PIO_OBJECT_STATE State
    );

/*++

Routine Description:

    This routine returns or attempts to create the asynchronous state for an
    I/O object state.

Arguments:

    State - Supplies a pointer to the I/O object state.

Return Value:

    Returns a pointer to the async state on success. This may have just been
    created.

    NULL if no async state exists and none could be created.

--*/

POBJECT_HEADER
IopGetPipeDirectory (
    VOID
    );

/*++

Routine Description:

    This routine returns the pipe root directory in the object system. This is
    the only place in the object system pipe creation is allowed.

Arguments:

    None.

Return Value:

    Returns a pointer to the pipe directory.

--*/

KSTATUS
IopCreatePipe (
    PCSTR Name,
    ULONG NameSize,
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    );

/*++

Routine Description:

    This routine actually creates a new pipe.

Arguments:

    Name - Supplies an optional pointer to the pipe name. This is only used for
        named pipes created in the pipe directory.

    NameSize - Supplies the size of the name in bytes including the null
        terminator.

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where a pointer to a newly created pipe
        file object will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopUnlinkPipe (
    PFILE_OBJECT FileObject,
    PBOOL Unlinked
    );

/*++

Routine Description:

    This routine unlinks a pipe from the accessible namespace.

Arguments:

    FileObject - Supplies a pointer to the pipe's file object.

    Unlinked - Supplies a pointer to a boolean that receives whether or not the
        terminal was successfully unlinked.

Return Value:

    Status code.

--*/

KSTATUS
IopOpenPipe (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine is called when a pipe is opened.

Arguments:

    IoHandle - Supplies a pointer to the new I/O handle.

Return Value:

    Status code.

--*/

KSTATUS
IopClosePipe (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine is called when a pipe is closed.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle being closed.

Return Value:

    Status code.

--*/

KSTATUS
IopPerformPipeIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    );

/*++

Routine Description:

    This routine reads from or writes to a pipe.

Arguments:

    Handle - Supplies a pointer to the pipe I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

KSTATUS
IopInitializeTerminalSupport (
    VOID
    );

/*++

Routine Description:

    This routine is called during system initialization to set up support for
    terminals.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
IopTerminalOpenMaster (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine is called when a master terminal was just opened.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle for the terminal.

Return Value:

    Status code.

--*/

KSTATUS
IopTerminalCloseMaster (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine is called when a master terminal was just closed.

Arguments:

    IoHandle - Supplies a pointer to the handle to close.

Return Value:

    Status code.

--*/

KSTATUS
IopTerminalOpenSlave (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine opens the slave side of a terminal object.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle for the terminal.

Return Value:

    Status code.

--*/

KSTATUS
IopTerminalCloseSlave (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine is called when a master terminal was just closed.

Arguments:

    IoHandle - Supplies a pointer to the handle to close.

Return Value:

    Status code.

--*/

KSTATUS
IopPerformTerminalMasterIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    );

/*++

Routine Description:

    This routine reads from or writes to the master end of a terminal.

Arguments:

    Handle - Supplies a pointer to the master terminal I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

KSTATUS
IopPerformTerminalSlaveIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    );

/*++

Routine Description:

    This routine reads from or writes to the slave end of a terminal.

Arguments:

    Handle - Supplies a pointer to the slave terminal I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

KSTATUS
IopCreateTerminal (
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    );

/*++

Routine Description:

    This routine creates a terminal master or slave.

Arguments:

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where a pointer to the new file object
        will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopUnlinkTerminal (
    PFILE_OBJECT FileObject,
    PBOOL Unlinked
    );

/*++

Routine Description:

    This routine unlinks a terminal from the accessible namespace.

Arguments:

    FileObject - Supplies a pointer to the terminal's file object.

    Unlinked - Supplies a pointer to a boolean that receives whether or not the
        terminal was successfully unlinked.

Return Value:

    Status code.

--*/

KSTATUS
IopTerminalUserControl (
    PIO_HANDLE Handle,
    TERMINAL_USER_CONTROL_CODE CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

/*++

Routine Description:

    This routine handles user control requests destined for a terminal object.

Arguments:

    Handle - Supplies the open file handle.

    CodeNumber - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

KSTATUS
IopTerminalFlush (
    PFILE_OBJECT FileObject,
    ULONG Flags
    );

/*++

Routine Description:

    This routine flushes a terminal object, discarding unwritten and unread
    data.

Arguments:

    FileObject - Supplies a pointer to the terminal to flush.

    Flags - Supplies the flags governing the flush operation. See FLUSH_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

KSTATUS
IopCreateSocket (
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    );

/*++

Routine Description:

    This routine allocates resources associated with a new socket.

Arguments:

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where the new file object representing the
        socket will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopPerformSocketIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    );

/*++

Routine Description:

    This routine reads from or writes to a socket.

Arguments:

    Handle - Supplies a pointer to the socket I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

KSTATUS
IopOpenSocket (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine opens a socket connection.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle for the socket being opened.

Return Value:

    Status code.

--*/

KSTATUS
IopCloseSocket (
    PIO_HANDLE IoHandle
    );

/*++

Routine Description:

    This routine closes a socket connection.

Arguments:

    IoHandle - Supplies a pointer to the socket handle to close.

Return Value:

    Status code.

--*/

KSTATUS
IopInitializeSharedMemoryObjectSupport (
    VOID
    );

/*++

Routine Description:

    This routine is called during system initialization to set up support for
    shared memory objects.

Arguments:

    None.

Return Value:

    Status code.

--*/

PPATH_POINT
IopGetSharedMemoryDirectory (
    BOOL FromKernelMode
    );

/*++

Routine Description:

    This routine returns the current process' shared memory directory. This is
    the only place the current process is allowed to create shared memory
    objects.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

Return Value:

    Returns a pointer to the process' shared memory directory.

--*/

KSTATUS
IopCreateSharedMemoryObject (
    BOOL FromKernelMode,
    PCSTR Name,
    ULONG NameSize,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    );

/*++

Routine Description:

    This routine actually creates a new shared memory object.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

    Name - Supplies an optional pointer to the shared memory object name. This
        is only used for shared memory objects created in the shared memory
        directory.

    NameSize - Supplies the size of the name in bytes including the null
        terminator.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where a pointer to a newly created pipe
        file object will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopTruncateSharedMemoryObject (
    PFILE_OBJECT FileObject,
    ULONGLONG NewSize
    );

/*++

Routine Description:

    This routine truncates a shared memory object. It assumes that the file's
    lock is held exclusively.

Arguments:

    FileObject - Supplies a pointer to the file object that owns the shared
        memory object.

    NewSize - Supplies the new size to set.

Return Value:

    Status code.

--*/

KSTATUS
IopUnlinkSharedMemoryObject (
    PFILE_OBJECT FileObject,
    PBOOL Unlinked
    );

/*++

Routine Description:

    This routine unlinks a shared memory object from the accessible namespace.

Arguments:

    FileObject - Supplies a pointer to the file object that owns the shared
        memory object.

    Unlinked - Supplies a pointer to a boolean that receives whether or not the
        shared memory object was successfully unlinked.

Return Value:

    Status code.

--*/

KSTATUS
IopPerformSharedMemoryIoOperation (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

/*++

Routine Description:

    This routine performs a non-cached I/O operation on a shared memory object.
    It is assumed that the file lock is held. This routine will always modify
    the file size in the the file properties and conditionally modify the file
    size in the file object.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code.

--*/

KSTATUS
IopSharedMemoryNotifyFileMapping (
    PFILE_OBJECT FileObject,
    BOOL Mapping
    );

/*++

Routine Description:

    This routine is called to notify a shared memory object that it is being
    mapped into memory or unmapped.

Arguments:

    FileObject - Supplies a pointer to the file object being mapped.

    Mapping - Supplies a boolean indicating if a new mapping is being created
        (TRUE) or an old mapping is being destroyed (FALSE).

Return Value:

    Status code.

--*/

KSTATUS
IopSharedMemoryUserControl (
    PIO_HANDLE Handle,
    SHARED_MEMORY_COMMAND CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

/*++

Routine Description:

    This routine handles user control requests destined for a shared memory
    object.

Arguments:

    Handle - Supplies the open file handle.

    CodeNumber - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

KSTATUS
IopInitializePathSupport (
    VOID
    );

/*++

Routine Description:

    This routine is called at system initialization time to initialize support
    for path traversal. It connects the root of the object manager to the root
    of the path system.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
IopPathWalk (
    BOOL FromKernelMode,
    PPATH_POINT Directory,
    PCSTR *Path,
    PULONG PathSize,
    ULONG OpenFlags,
    PCREATE_PARAMETERS Create,
    PPATH_POINT Result
    );

/*++

Routine Description:

    This routine attempts to walk the given path.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        is coming directly from kernel mode (and should use the kernel's root).

    Directory - Supplies an optional pointer to a path point containing the
        directory to start from if the path is relative. Supply NULL to use the
        current working directory.

    Path - Supplies a pointer that on input contains a pointer to the string
        of the path to walk. This pointer will be advanced beyond the portion
        of the path that was successfully walked.

    PathSize - Supplies a pointer that on input contains the size of the
        input path string in bytes. This value will be updated to reflect the
        new size of the updated path string.

    OpenFlags - Supplies a bitfield of flags governing the behavior of the
        handle. See OPEN_FLAG_* definitions.

    Create - Supplies an optional pointer to the creation parameters.

    Result - Supplies a pointer to a path point that receives the resulting
        path entry and mount point on success. The path entry and mount point
        will come with an extra reference on them, so the caller must be sure
        to release the references when finished.

Return Value:

    Status code.

--*/

VOID
IopFillOutFilePropertiesForObject (
    PFILE_PROPERTIES Properties,
    POBJECT_HEADER Object
    );

/*++

Routine Description:

    This routine fills out the file properties structure for an object manager
    object.

Arguments:

    Properties - Supplies a pointer to the file properties.

    Object - Supplies a pointer to the object.

Return Value:

    TRUE if the paths are equals.

    FALSE if the paths differ in some way.

--*/

PPATH_ENTRY
IopCreateAnonymousPathEntry (
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine creates a new path entry structure that is not connected to
    the global path tree.

Arguments:

    FileObject - Supplies a pointer to the file object backing this entry. This
        routine takes ownership of an assumed reference on the file object.

Return Value:

    Returns a pointer to the path entry on success.

    NULL on allocation failure.

--*/

KSTATUS
IopPathSplit (
    PCSTR Path,
    ULONG PathSize,
    PSTR *DirectoryComponent,
    PULONG DirectoryComponentSize,
    PSTR *LastComponent,
    PULONG LastComponentSize
    );

/*++

Routine Description:

    This routine creates a new string containing the last component of the
    given path.

Arguments:

    Path - Supplies a pointer to the null terminated path string.

    PathSize - Supplies the size of the path string in bytes including the
        null terminator.

    DirectoryComponent - Supplies a pointer where a newly allocated string
        containing only the directory component will be returned on success.
        The caller is responsible for freeing this memory from paged pool.

    DirectoryComponentSize - Supplies a pointer where the size of the directory
        component buffer in bytes including the null terminator will be
        returned.

    LastComponent - Supplies a pointer where a newly allocated string
        containing only the last component will be returned on success. The
        caller is responsible for freeing this memory from paged pool.

    LastComponentSize - Supplies a pointer where the size of the last component
        buffer in bytes including the null terminator will be returned.

Return Value:

    Status code.

--*/

PPATH_ENTRY
IopCreatePathEntry (
    PCSTR Name,
    ULONG NameSize,
    ULONG Hash,
    PPATH_ENTRY Parent,
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine creates a new path entry structure.

Arguments:

    Name - Supplies an optional pointer to the name to give this path entry. A
        copy of this name will be made.

    NameSize - Supplies the size of the name buffer in bytes including the
        null terminator.

    Hash - Supplies the hash of the name string.

    Parent - Supplies a pointer to the parent of this entry.

    FileObject - Supplies an optional pointer to the file object backing this
        entry. This routine takes ownership of an assumed reference on the file
        object.

Return Value:

    Returns a pointer to the path entry on success.

    NULL on allocation failure.

--*/

ULONG
IopHashPathString (
    PCSTR String,
    ULONG StringSize
    );

/*++

Routine Description:

    This routine generates the hash associated with a path name. This hash is
    used to speed up comparisons.

Arguments:

    String - Supplies a pointer to the string to hash.

    StringSize - Supplies the size of the string, including the null terminator.

Return Value:

    Returns the hash of the given string.

--*/

BOOL
IopIsDescendantPath (
    PPATH_ENTRY Ancestor,
    PPATH_ENTRY DescendantEntry
    );

/*++

Routine Description:

    This routine determines whether or not the given descendant path entry is a
    descendent of the given path entry. This does not take mount points into
    account.

Arguments:

    Ancestor - Supplies a pointer to the possible ancestor path entry.

    DescendantEntry - Supplies a pointer to the possible descendant path entry.

Return Value:

    Returns TRUE if it is a descendant, or FALSE otherwise.

--*/

VOID
IopPathUnlink (
    PPATH_ENTRY Entry
    );

/*++

Routine Description:

    This routine unlinks the given path entry from the path hierarchy. This
    assumes the caller hold both the path entry's file object lock (if it
    exists) and the parent path entry's file object lock exclusively.

Arguments:

    Entry - Supplies a pointer to the path entry that is to be unlinked from
        the path hierarchy.

Return Value:

    None.

--*/

KSTATUS
IopGetUserFilePath (
    PPATH_POINT Entry,
    PPATH_POINT Root,
    PSTR UserBuffer,
    PUINTN UserBufferSize
    );

/*++

Routine Description:

    This routine copies the full path of the given path entry (as seen from
    the given root) into the given user mode buffer.

Arguments:

    Entry - Supplies a pointer to the path point to get the full path of.

    Root - Supplies a pointer to the user's root.

    UserBuffer - Supplies a pointer to the user mode buffer where the full path
        should be returned.

    UserBufferSize - Supplies a pointer that on success contains the size of
        the user mode buffer. Returns the actual size of the file path, even if
        the supplied buffer was too small.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PATH_NOT_FOUND if the path entry has no path.

    STATUS_ACCESS_VIOLATION if the buffer was invalid.

    STATUS_BUFFER_TOO_SMALL if the buffer was too small.

--*/

KSTATUS
IopGetPathFromRoot (
    PPATH_POINT Entry,
    PPATH_POINT Root,
    PSTR *Path,
    PUINTN PathSize
    );

/*++

Routine Description:

    This routine creates a string representing the path from the given root to
    the given entry. If the entry is not a descendent of the given root, then
    the full path is printed.

Arguments:

    Entry - Supplies a pointer to the path point where to stop the string.

    Root - Supplies a optional pointer to the path point to treat as root.

    Path - Supplies a pointer that receives the full path string.

    PathSize - Supplies a pointer that receives the size of the full path
        string, in bytes.

Return Value:

    Status code.

--*/

KSTATUS
IopGetPathFromRootUnlocked (
    PPATH_POINT Entry,
    PPATH_POINT Root,
    PSTR *Path,
    PUINTN PathSize
    );

/*++

Routine Description:

    This routine creates a string representing the path from the given root to
    the given entry. If the entry is not a descendent of the given root, then
    the full path is printed. This routine assumes that the mount lock is held
    in shared mode.

Arguments:

    Entry - Supplies a pointer to the path point where to stop the string.

    Root - Supplies a optional pointer to the path point to treat as root.

    Path - Supplies a pointer that receives the full path string.

    PathSize - Supplies a pointer that receives the size of the full path
        string, in bytes.

Return Value:

    Status code.

--*/

KSTATUS
IopPathLookup (
    BOOL FromKernelMode,
    PPATH_POINT Root,
    PPATH_POINT Directory,
    BOOL DirectoryLockHeld,
    PCSTR Name,
    ULONG NameSize,
    ULONG OpenFlags,
    PCREATE_PARAMETERS Create,
    PPATH_POINT Result
    );

/*++

Routine Description:

    This routine attempts to look up a child with the given name in a directory.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether this request is
        originating from kernel mode (TRUE) or user mode (FALSE). Kernel mode
        requests are not subjected to permission checks.

    Root - Supplies a pointer to the caller's root path point.

    Directory - Supplies a pointer to the path point to search.

    DirectoryLockHeld - Supplies a boolean indicating whether or not the caller
        had already acquired the directory's lock (exclusively).

    Name - Supplies a pointer to the name string.

    NameSize - Supplies a pointer to the size of the string in bytes
        including an assumed null terminator.

    OpenFlags - Supplies a bitfield of flags governing the behavior of the
        handle. See OPEN_FLAG_* definitions.

    Create - Supplies an optional pointer to the creation parameters.

    Result - Supplies a pointer to a path point that receives the resulting
        path entry and mount point on success. The path entry and mount point
        will come with an extra reference on them, so the caller must be sure
        to release the references when finished. This routine may return a
        path entry even on failing status codes, such as a negative path entry.

Return Value:

    Status code.

--*/

VOID
IopPathCleanCache (
    PPATH_ENTRY RootPath
    );

/*++

Routine Description:

    This routine attempts to destroy any cached path entries below the given
    root path.

Arguments:

    RootPath - Supplies a pointer to the root path entry.

    DoNotCache - Supplies a boolean indicating whether or not to mark open
        path entries as non-cacheable or not.

Return Value:

    None.

--*/

VOID
IopPathEntryIncrementMountCount (
    PPATH_ENTRY PathEntry
    );

/*++

Routine Description:

    This routine increments the mount count for the given path entry.

Arguments:

    PathEntry - Supplies a pointer to a path entry.

Return Value:

    None.

--*/

VOID
IopPathEntryDecrementMountCount (
    PPATH_ENTRY PathEntry
    );

/*++

Routine Description:

    This routine decrements the mount count for the given path entry.

Arguments:

    PathEntry - Supplies a pointer to a path entry.

Return Value:

    None.

--*/

VOID
IopGetParentPathPoint (
    PPATH_POINT Root,
    PPATH_POINT PathPoint,
    PPATH_POINT ParentPathPoint
    );

/*++

Routine Description:

    This routine gets the parent path point of the given path point, correctly
    traversing mount points. This routine takes references on the parent path
    point's path entry and mount point.

Arguments:

    Root - Supplies an optional pointer to the caller's path point root. If
        supplied, then the parent will never be lower in the path tree than
        the root.

    PathPoint - Supplies a pointer to the path point whose parent is being
        queried.

    ParentPathPoint - Supplies a pointer to a path point that receives the
        parent path point's information.

Return Value:

    None.

--*/

KSTATUS
IopCheckPermissions (
    BOOL FromKernelMode,
    PPATH_POINT PathPoint,
    ULONG Access
    );

/*++

Routine Description:

    This routine performs a permission check for the current user at the given
    path point.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request actually
        originates from kernel mode or not.

    PathPoint - Supplies a pointer to the path point to check.

    Access - Supplies the desired access the user needs.

Return Value:

    STATUS_SUCCESS if the user has permission to access the given object in
    the requested way.

    STATUS_ACCESS_DENIED if the permission was not granted.

--*/

KSTATUS
IopCheckDeletePermission (
    BOOL FromKernelMode,
    PPATH_POINT DirectoryPathPoint,
    PPATH_POINT FilePathPoint
    );

/*++

Routine Description:

    This routine performs a permission check for the current user at the given
    path point, in preparation for removing a directory entry during a rename
    or unlink operation.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this
        request actually originated from kernel mode.

    DirectoryPathPoint - Supplies a pointer to the directory path point the
        file resides in.

    FilePathPoint - Supplies a pointer to the file path point being deleted or
        renamed.

Return Value:

    STATUS_SUCCESS if the user has permission to access the given object in
    the requested way.

    STATUS_ACCESS_DENIED if the permission was not granted.

--*/

KSTATUS
IopSendStateChangeIrp (
    PDEVICE Device,
    IRP_MINOR_CODE MinorCode,
    PVOID IrpBody,
    UINTN IrpBodySize
    );

/*++

Routine Description:

    This routine sends a state change IRP.

Arguments:

    Device - Supplies a pointer to the device to send the IRP to.

    MinorCode - Supplies the IRP minor code.

    IrpBody - Supplies a pointer to a buffer that will be copied into the IRP
        data union on input. On output, this buffer will receive the altered
        data.

    IrpBodySize - Supplies the size of the IRP body in bytes.

Return Value:

    Status code.

--*/

KSTATUS
IopSendOpenIrp (
    PDEVICE Device,
    PIRP_OPEN OpenRequest
    );

/*++

Routine Description:

    This routine sends an open IRP.

Arguments:

    Device - Supplies a pointer to the device to send the IRP to.

    OpenRequest - Supplies a pointer that on input contains the open request
        parameters. The contents of the IRP will also be copied here upon
        returning.

Return Value:

    Status code.

--*/

KSTATUS
IopSendCloseIrp (
    PDEVICE Device,
    PIRP_CLOSE CloseRequest
    );

/*++

Routine Description:

    This routine sends a close IRP to the given device.

Arguments:

    Device - Supplies a pointer to the device to send the close IRP to.

    CloseRequest - Supplies a pointer to the IRP close context.

Return Value:

    Status code.

--*/

KSTATUS
IopSendIoIrp (
    PDEVICE Device,
    IRP_MINOR_CODE MinorCodeNumber,
    PIRP_READ_WRITE Request
    );

/*++

Routine Description:

    This routine sends an I/O IRP.

Arguments:

    Device - Supplies a pointer to the device to send the IRP to.

    MinorCodeNumber - Supplies the minor code number to send to the IRP.

    Request - Supplies a pointer that on input contains the I/O request
        parameters.

Return Value:

    Status code.

--*/

KSTATUS
IopSendIoReadIrp (
    PDEVICE Device,
    PIRP_READ_WRITE Request
    );

/*++

Routine Description:

    This routine sends an I/O read IRP to the given device. It makes sure that
    the bytes completed that are returned do not extend beyond the file size.
    Here the file size is that which is currently on the device and not in the
    system's cached view of the world.

Arguments:

    Device - Supplies a pointer to the device to send the IRP to.

    Request - Supplies a pointer that on input contains the I/O request
        parameters.

Return Value:

    Status code.

--*/

KSTATUS
IopSendSystemControlIrp (
    PDEVICE Device,
    IRP_MINOR_CODE ControlNumber,
    PVOID SystemContext
    );

/*++

Routine Description:

    This routine sends a system control request to the given device. This
    routine must be called at low level.

Arguments:

    Device - Supplies a pointer to the device to send the system control
        request to.

    ControlNumber - Supplies the system control number to send.

    SystemContext - Supplies a pointer to the request details, which are
        dependent on the control number.

Return Value:

    Status code.

--*/

KSTATUS
IopSendUserControlIrp (
    PIO_HANDLE Handle,
    ULONG MinorCode,
    BOOL FromKernelMode,
    PVOID UserContext,
    UINTN UserContextSize
    );

/*++

Routine Description:

    This routine sends a user control request to the device associated with
    the given handle. This routine must be called at low level.

Arguments:

    Handle - Supplies the open file handle.

    MinorCode - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    UserContext - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    UserContextSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

KSTATUS
IopQueueResourceAssignment (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine puts this device in the resource assignment queue.

Arguments:

    Device - Supplies a pointer to the device to queue for resource assignment.

Return Value:

    Status code indicating whether or not the device was successfully queued.
    (Not that it successfully made it through the queue or was processed in
    any way).

--*/

KSTATUS
IopQueueDelayedResourceAssignment (
    VOID
    );

/*++

Routine Description:

    This routine queues resource assignment for devices that were delayed to
    allow devices with boot resources to go first.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
IopProcessResourceRequirements (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine attempts to find the best set of resources for a given device.

Arguments:

    Device - Supplies a pointer to the device that will be receiving the
        resource allocation.

Return Value:

    Status code.

--*/

KSTATUS
IopArchInitializeKnownArbiterRegions (
    VOID
    );

/*++

Routine Description:

    This routine performs any architecture-specific initialization of the
    resource arbiters.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
IopDestroyArbiterList (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine destroys the arbiter list of the given device.

Arguments:

    Device - Supplies a pointer to a device whose arbiter list is to be
        destroyed.

Return Value:

    None.

--*/

BOOL
IopIsTestHookSet (
    ULONG TestHookMask
    );

/*++

Routine Description:

    This routine checks to see if the given test hook field is currently set in
    the test hook bitmask.

Arguments:

    TestHookMask - Supplies the test hook field this routine will check the
        test hook bitmask against.

Return Value:

    Returns TRUE if the test hook is set, or FALSE otherwise.

--*/

KSTATUS
IopGetFileLock (
    PIO_HANDLE IoHandle,
    PFILE_LOCK Lock
    );

/*++

Routine Description:

    This routine gets information about a file lock. Existing locks are not
    reported if they are compatible with making a new lock in the given region.
    So set the lock type to write if both read and write locks should be
    reported.

Arguments:

    IoHandle - Supplies a pointer to the open I/O handle.

    Lock - Supplies a pointer to the lock information.

Return Value:

    Status code.

--*/

KSTATUS
IopSetFileLock (
    PIO_HANDLE IoHandle,
    PFILE_LOCK Lock,
    BOOL Blocking
    );

/*++

Routine Description:

    This routine locks or unlocks a portion of a file. If the process already
    has a lock on any part of the region, the old lock is replaced with this
    new region. Remove a lock by specifying a lock type of unlock.

Arguments:

    IoHandle - Supplies a pointer to the open I/O handle.

    Lock - Supplies a pointer to the lock information.

    Blocking - Supplies a boolean indicating if this should block until a
        determination is made.

Return Value:

    Status code.

--*/

VOID
IopRemoveFileLocks (
    PIO_HANDLE IoHandle,
    PKPROCESS Process
    );

/*++

Routine Description:

    This routine destroys any locks the given process has on the file object
    pointed to by the given I/O handle. This routine is called anytime any
    file descriptor is closed by a process, even if other file descriptors
    to the same file in the process remain open.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle being closed.

    Process - Supplies the process closing the handle.

Return Value:

    None.

--*/

KSTATUS
IopSynchronizeBlockDevice (
    PDEVICE Device
    );

/*++

Routine Description:

    This routine sends a sync request to a block device to ensure all data is
    written out to permanent storage.

Arguments:

    Device - Supplies a pointer to the device to send the synchronize request
        to.

Return Value:

    Status code.

--*/

