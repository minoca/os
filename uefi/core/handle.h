/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    handle.h

Abstract:

    This header contains definitions for UEFI core handles.

Author:

    Evan Green 5-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define magic constants in the various handle structures.
//

#define EFI_HANDLE_MAGIC 0x646E6148 // 'dnaH'
#define EFI_PROTOCOL_ENTRY_MAGIC 0x746F7250 // 'torP'
#define EFI_PROTOCOL_INTERFACE_MAGIC 0x72746E49 // 'rtnI'
#define EFI_OPEN_PROTOCOL_MAGIC 0x6E65704F // 'nepO'
#define EFI_PROTOCOL_NOTIFY_MAGIC 0x69746F4E // 'itoN'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal structure of an EFI handle.

Members:

    Magic - Stores the constant EFI_HANDLE_MAGIC.

    ListEntry - Stores pointers to the next and previous system handles in the
        global list of all handles.

    ProtocolList - Stores the head of the list of protocols supported on this
        handle.

    LocateRequest - Stores the locate request.

    Key - Stores the handle database key when this handle was last created or
        modified.

--*/

typedef struct _EFI_HANDLE_DATA {
    UINTN Magic;
    LIST_ENTRY ListEntry;
    LIST_ENTRY ProtocolList;
    UINTN LocateRequest;
    UINT64 Key;
} EFI_HANDLE_DATA, *PEFI_HANDLE_DATA;

/*++

Structure Description:

    This structure stores information about an EFI protocol. It represents a
    protocol with a specific GUID. Each handler that supports this protocol is
    listed, along with a list of registered notifies.

Members:

    Magic - Stores the constant EFI_PROTOCOL_ENTRY_MAGIC.

    ListEntry - Stores pointers to the next and previous protocols in the
        global protocol database.

    ProtocolList - Stores the head of the list of protocol interfaces for this
        protocol ID.

    NotifyList - Stores the list of registered notification handlers.

    ProtocolId - Stores the GUID of the protocol.

--*/

typedef struct _EFI_PROTOCOL_ENTRY {
    UINTN Magic;
    LIST_ENTRY ListEntry;
    LIST_ENTRY ProtocolList;
    LIST_ENTRY NotifyList;
    EFI_GUID ProtocolId;
} EFI_PROTOCOL_ENTRY, *PEFI_PROTOCOL_ENTRY;

/*++

Structure Description:

    This structure stores information about a protocol interface, which tracks
    a protocol installed on a handle.

Members:

    Magic - Stores the constant EFI_PROTOCOL_INTERFACE_MAGIC.

    ListEntry - Stores pointers to the next and previous protocol interfaces
        on the handle's protocol list.

    Handle - Stores a pointer back to the handle this interface is bound to.

    ProtocolListEntry - Stores pointers to the next and previous protocol
        interfaces with the same protocol GUID.

    Protocol - Stores a pointer to the protocol this interface belongs to.

    Interface - Stores the interface value.

    OpenList - Stores the list of open protocol data structures.

    OpenCount - Stores the number of open protocol entry structures on the
        open list.

--*/

typedef struct _EFI_PROTOCOL_INTERFACE {
    UINTN Magic;
    LIST_ENTRY ListEntry;
    PEFI_HANDLE_DATA Handle;
    LIST_ENTRY ProtocolListEntry;
    PEFI_PROTOCOL_ENTRY Protocol;
    VOID *Interface;
    LIST_ENTRY OpenList;
    UINTN OpenCount;
} EFI_PROTOCOL_INTERFACE, *PEFI_PROTOCOL_INTERFACE;

/*++

Structure Description:

    This structure stores information about an open protocol interface.

Members:

    Magic - Stores the constant EFI_OPEN_PROTOCOL_MAGIC.

    ListEntry - Stores pointers to the next and previous open protocol data
        structures in the protocol interface's open list.

    AgentHandle - Stores the agent opening the protocol interface.

    ControllerHandle - Stores the optional controller associated with the open.

    Attributes - Stores attributes about the open.

    OpenCount - Stores the number of times the protocol interface has been
        opened with these parameters.

--*/

typedef struct _EFI_OPEN_PROTOCOL_DATA {
    UINTN Magic;
    LIST_ENTRY ListEntry;
    EFI_HANDLE AgentHandle;
    EFI_HANDLE ControllerHandle;
    UINT32 Attributes;
    UINT32 OpenCount;
} EFI_OPEN_PROTOCOL_DATA, *PEFI_OPEN_PROTOCOL_DATA;

/*++

Structure Description:

    This structure stores information about a protocol notification
    registration.

Members:

    Magic - Stores the constant EFI_PROTOCOL_NOTIFY_MAGIC.

    ListEntry - Stores pointers to the next and previous notification
        registrations in the protocol entry.

    Protocol - Stores a pointer back to the protocol this registration is
        bound to.

    Event - Stores a pointer to the event to notify.

    Position - Stores the last position notified.

--*/

typedef struct _EFI_PROTOCOL_NOTIFY {
    UINTN Magic;
    LIST_ENTRY ListEntry;
    PEFI_PROTOCOL_ENTRY Protocol;
    EFI_EVENT Event;
    PLIST_ENTRY Position;
} EFI_PROTOCOL_NOTIFY, *PEFI_PROTOCOL_NOTIFY;

//
// -------------------------------------------------------------------- Globals
//

extern EFI_LOCK EfiProtocolDatabaseLock;
extern LIST_ENTRY EfiHandleList;
extern LIST_ENTRY EfiProtocolDatabase;
extern UINTN EfiHandleDatabaseKey;

//
// -------------------------------------------------------- Function Prototypes
//

EFIAPI
EFI_STATUS
EfiCoreProtocolsPerHandle (
    EFI_HANDLE Handle,
    EFI_GUID ***ProtocolBuffer,
    UINTN *ProtocolBufferCount
    );

/*++

Routine Description:

    This routine retrieves the list of protocol interface GUIDs that are
    installed on a handle in a buffer allocated from pool.

Arguments:

    Handle - Supplies the handle from which to retrieve the list of protocol
        interface GUIDs.

    ProtocolBuffer - Supplies a pointer to the list of protocol interface GUID
        pointers that are installed on the given handle.

    ProtocolBufferCount - Supplies a pointer to the number of GUID pointers
        present in the protocol buffer.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_INVALID_PARAMETER if the handle is NULL or invalid, or the protocol
    buffer or count is NULL.

--*/

EFIAPI
EFI_STATUS
EfiCoreOpenProtocolInformation (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer,
    UINTN *EntryCount
    );

/*++

Routine Description:

    This routine retrieves a list of agents that currently have a protocol
    interface opened.

Arguments:

    Handle - Supplies the handle for the protocol interface being queried.

    Protocol - Supplies the published unique identifier of the protocol.

    EntryBuffer - Supplies a pointer where a pointer to a buffer of open
        protocol information in the form of EFI_OPEN_PROTOCOL_INFORMATION_ENTRY
        structures will be returned.

    EntryCount - Supplies a pointer that receives the number of entries in the
        buffer.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_NOT_FOUND if the handle does not support the given protocol.

--*/

EFIAPI
EFI_STATUS
EfiCoreOpenProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface,
    EFI_HANDLE AgentHandle,
    EFI_HANDLE ControllerHandle,
    UINT32 Attributes
    );

/*++

Routine Description:

    This routine queries a handle to determine if it supports a specified
    protocol. If the protocol is supported by the handle, it opens the protocol
    on behalf of the calling agent.

Arguments:

    Handle - Supplies the handle for the protocol interface that is being
        opened.

    Protocol - Supplies the published unique identifier of the protocol.

    Interface - Supplies the address where a pointer to the corresponding
        protocol interface is returned.

    AgentHandle - Supplies the handle of the agent that is opening the protocol
        interface specified by the protocol and interface.

    ControllerHandle - Supplies the controller handle that requires the
        protocl interface if the caller is a driver that follows the UEFI
        driver model. If the caller does not follow the UEFI Driver Model, then
        this parameter is optional.

    Attributes - Supplies the open mode of the protocol interface specified by
        the given handle and protocol.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_UNSUPPORTED if the handle not support the specified protocol.

    EFI_INVALID_PARAMETER if a parameter is invalid.

    EFI_ACCESS_DENIED if the required attributes can't be supported in the
    current environment.

    EFI_ALREADY_STARTED if the item on the open list already has required
    attributes whose agent handle is the same as the given one.

--*/

EFIAPI
EFI_STATUS
EfiCoreCloseProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    EFI_HANDLE AgentHandle,
    EFI_HANDLE ControllerHandle
    );

/*++

Routine Description:

    This routine closes a protocol on a handle that was previously opened.

Arguments:

    Handle - Supplies the handle for the protocol interface was previously
        opened.

    Protocol - Supplies the published unique identifier of the protocol.

    AgentHandle - Supplies the handle of the agent that is closing the
        protocol interface.

    ControllerHandle - Supplies the controller handle that required the
        protocl interface if the caller is a driver that follows the UEFI
        driver model. If the caller does not follow the UEFI Driver Model, then
        this parameter is optional.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_INVALID_PARAMETER if the handle, agent, or protocol is NULL, or if the
    controller handle is not NULL and the controller handle is not valid.

    EFI_NOT_FOUND if the handle does not support the given protocol, or the
    protocol interface is not currently open by the agent and controller
    handles.

--*/

EFIAPI
EFI_STATUS
EfiCoreHandleProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface
    );

/*++

Routine Description:

    This routine queries a handle to determine if it supports a specified
    protocol.

Arguments:

    Handle - Supplies the handle being queried.

    Protocol - Supplies the published unique identifier of the protocol.

    Interface - Supplies the address where a pointer to the corresponding
        protocol interface is returned.

Return Value:

    EFI_SUCCESS if the interface information was returned.

    EFI_UNSUPPORTED if the device not support the specified protocol.

    EFI_INVALID_PARAMETER if the handle, protocol, or interface is NULL.

--*/

EFIAPI
EFI_STATUS
EfiCoreInstallProtocolInterface (
    EFI_HANDLE *Handle,
    EFI_GUID *Protocol,
    EFI_INTERFACE_TYPE InterfaceType,
    VOID *Interface
    );

/*++

Routine Description:

    This routine installs a protocol interface on a device handle. If the
    handle does not exist, it is created and added to the list of handles in
    the system. InstallMultipleProtocolInterfaces performs more error checking
    than this routine, so it is recommended to be used in place of this
    routine.

Arguments:

    Handle - Supplies a pointer to the EFI handle on which the interface is to
        be installed.

    Protocol - Supplies a pointer to the numeric ID of the protocol interface.

    InterfaceType - Supplies the interface type.

    Interface - Supplies a pointer to the protocol interface.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

    EFI_INVALID_PARAMETER if the handle or protocol is NULL, the interface type
    is not native, or the protocol is already install on the given handle.

--*/

EFIAPI
EFI_STATUS
EfiCoreInstallMultipleProtocolInterfaces (
    EFI_HANDLE *Handle,
    ...
    );

/*++

Routine Description:

    This routine installs one or more protocol interface into the boot
    services environment.

Arguments:

    Handle - Supplies a pointer to the EFI handle on which the interface is to
        be installed, or a pointer to NULL if a new handle is to be allocated.

    ... - Supplies a variable argument list containing pairs of protocol GUIDs
        and protocol interfaces.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

    EFI_ALREADY_STARTED if a device path protocol instance was passed in that
    is already present in the handle database.

    EFI_INVALID_PARAMETER if the handle is NULL or the protocol is already
    installed on the given handle.

--*/

EFIAPI
EFI_STATUS
EfiCoreReinstallProtocolInterface (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID *OldInterface,
    VOID *NewInterface
    );

/*++

Routine Description:

    This routine reinstalls a protocol interface on a device handle.

Arguments:

    Handle - Supplies the device handle on which the interface is to be
        reinstalled.

    Protocol - Supplies a pointer to the numeric ID of the interface.

    OldInterface - Supplies a pointer to the old interface. NULL can be used if
        a structure is not associated with the protocol.

    NewInterface - Supplies a pointer to the new interface.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the old interface was not found.

    EFI_ACCESS_DENIED if the protocl interface could not be reinstalled because
    the old interface is still being used by a driver that will not release it.

    EFI_INVALID_PARAMETER if the handle or protocol is NULL.

--*/

EFI_STATUS
EfiCoreUninstallProtocolInterface (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID *Interface
    );

/*++

Routine Description:

    This routine removes a protocol interface from a device handle. It is
    recommended that UninstallMultipleProtocolInterfaces be used in place of
    this routine.

Arguments:

    Handle - Supplies the device handle on which the interface is to be
        removed.

    Protocol - Supplies a pointer to the numeric ID of the interface.

    Interface - Supplies a pointer to the interface.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the old interface was not found.

    EFI_ACCESS_DENIED if the protocl interface could not be reinstalled because
    the old interface is still being used by a driver that will not release it.

    EFI_INVALID_PARAMETER if the handle or protocol is NULL.

--*/

EFIAPI
EFI_STATUS
EfiCoreUninstallMultipleProtocolInterfaces (
    EFI_HANDLE Handle,
    ...
    );

/*++

Routine Description:

    This routine removes one or more protocol interfaces into the boot services
    environment.

Arguments:

    Handle - Supplies the device handle on which the interface is to be
        removed.

    ... - Supplies a variable argument list containing pairs of protocol GUIDs
        and protocol interfaces.

Return Value:

    EFI_SUCCESS if all of the requested protocol interfaces were removed.

    EFI_INVALID_PARAMETER if one of the protocol interfaces was not previously
        installed on the given.

--*/

EFIAPI
EFI_STATUS
EfiCoreRegisterProtocolNotify (
    EFI_GUID *Protocol,
    EFI_EVENT Event,
    VOID **Registration
    );

/*++

Routine Description:

    This routine creates an event that is to be signaled whenever an interface
    is installed for a specified protocol.

Arguments:

    Protocol - Supplies the numeric ID of the protocol for which the event is
        to be registered.

    Event - Supplies the event that is to be signaled whenever a protocol
        interface is registered for the given protocol.

    Registration - Supplies a pointer to a memory location to receive the
        registration value.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_INVALID_PARAMETER if the protocol, event, or registration is NULL.

--*/

VOID
EfiCoreInitializeHandleDatabase (
    VOID
    );

/*++

Routine Description:

    This routine initializes EFI handle and protocol support.

Arguments:

    None.

Return Value:

    None.

--*/

EFI_STATUS
EfipCoreInstallProtocolInterfaceNotify (
    EFI_HANDLE *EfiHandle,
    EFI_GUID *Protocol,
    EFI_INTERFACE_TYPE InterfaceType,
    VOID *Interface,
    BOOLEAN Notify
    );

/*++

Routine Description:

    This routine installs a protocol interface into the boot services
    environment.

Arguments:

    EfiHandle - Supplies a pointer to the handle to install the protocol
        handler on. If this points to NULL, a new handle will be allocated and
        the protocol is added to the handle.

    Protocol - Supplies a pointer to the GUID of the protocol to add.

    InterfaceType - Supplies the interface type.

    Interface - Supplies the interface value.

    Notify - Supplies a boolean indicating whether ot notify the notification
        list for this protocol.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipCoreUnregisterProtocolNotify (
    EFI_EVENT Event
    );

/*++

Routine Description:

    This routine removes all the events in the protocol database that match
    the given event.

Arguments:

    Event - Supplies the event that is being destroyed.

Return Value:

    EFI status code.

--*/

PEFI_PROTOCOL_ENTRY
EfipCoreFindProtocolEntry (
    EFI_GUID *Protocol,
    BOOLEAN Create
    );

/*++

Routine Description:

    This routine findst the protocol entry for the given protocol ID. This
    routine assumes the protocol database lock is already held.

Arguments:

    Protocol - Supplies a pointer to the protocol GUID to find.

    Create - Supplies a boolean indicating if an entry should be created if not
        found.

Return Value:

    Returns a pointer to the protocol entry on success.

    NULL on failure.

--*/

EFI_STATUS
EfipCoreValidateHandle (
    EFI_HANDLE Handle
    );

/*++

Routine Description:

    This routine validates that the given handle is a valid EFI_HANDLE.

Arguments:

    Handle - Supplies the handle to validate.

Return Value:

    EFI_SUCCESS if on success.

    EFI_INVALID_PARAMETER if the handle is not in fact valid.

--*/

UINT64
EfipCoreGetHandleDatabaseKey (
    VOID
    );

/*++

Routine Description:

    This routine returns the current handle database key.

Arguments:

    None.

Return Value:

    Returns the current handle database key.

--*/

VOID
EfipCoreConnectHandlesByKey (
    UINT64 Key
    );

/*++

Routine Description:

    This routine connects any handles that were created or modified while an
    image executed.

Arguments:

    Key - Supplies the database key snapped when the image was started.

Return Value:

    None.

--*/

//
// Locate functions
//

EFIAPI
EFI_STATUS
EfiCoreLocateDevicePath (
    EFI_GUID *Protocol,
    EFI_DEVICE_PATH_PROTOCOL **DevicePath,
    EFI_HANDLE *Device
    );

/*++

Routine Description:

    This routine attempts to locate the handle to a device on the device path
    that supports the specified protocol.

Arguments:

    Protocol - Supplies a pointer to the protocol to search for.

    DevicePath - Supplies a pointer that on input contains a pointer to the
        device path. On output, the path pointer is modified to point to the
        remaining part of the device path.

    Device - Supplies a pointer where the handle of the device will be
        returned.

Return Value:

    EFI_SUCCESS if a handle was returned.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the protocol is NULL, device path is NULL, or a
    handle matched and the device is NULL.

--*/

EFIAPI
EFI_STATUS
EfiCoreLocateHandleBuffer (
    EFI_LOCATE_SEARCH_TYPE SearchType,
    EFI_GUID *Protocol,
    VOID *SearchKey,
    UINTN *HandleCount,
    EFI_HANDLE **Buffer
    );

/*++

Routine Description:

    This routine returns an array of handles that support the requested
    protocol in a buffer allocated from pool.

Arguments:

    SearchType - Supplies the search behavior.

    Protocol - Supplies a pointer to the protocol to search by.

    SearchKey - Supplies a pointer to the search key.

    HandleCount - Supplies a pointer where the number of handles will be
        returned.

    Buffer - Supplies a pointer where an array will be returned containing the
        requested handles.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no handles match the search.

    EFI_INVALID_PARAMETER if the handle count or buffer is NULL.

    EFI_OUT_OF_RESOURCES if an allocation failed.

--*/

EFIAPI
EFI_STATUS
EfiCoreLocateHandle (
    EFI_LOCATE_SEARCH_TYPE SearchType,
    EFI_GUID *Protocol,
    VOID *SearchKey,
    UINTN *BufferSize,
    EFI_HANDLE *Buffer
    );

/*++

Routine Description:

    This routine returns an array of handles that support a specified protocol.

Arguments:

    SearchType - Supplies which handle(s) are to be returned.

    Protocol - Supplies an optional pointer to the protocols to search by.

    SearchKey - Supplies an optional pointer to the search key.

    BufferSize - Supplies a pointer that on input contains the size of the
        result buffer in bytes. On output, the size of the result array will be
        returned (even if the buffer was too small).

    Buffer - Supplies a pointer where the results will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no handles match the search.

    EFI_BUFFER_TOO_SMALL if the given buffer wasn't big enough to hold all the
    results.

    EFI_INVALID_PARAMETER if the serach type is invalid, one of the parameters
    required by the given search type was NULL, one or more matches are found
    and the buffer size is NULL, or the buffer size is large enough and the
    buffer is NULL.

--*/

EFIAPI
EFI_STATUS
EfiCoreLocateProtocol (
    EFI_GUID *Protocol,
    VOID *Registration,
    VOID **Interface
    );

/*++

Routine Description:

    This routine returns the first protocol instance that matches the given
    protocol.

Arguments:

    Protocol - Supplies a pointer to the protocol to search by.

    Registration - Supplies a pointer to an optional registration key
        returned from RegisterProtocolNotify.

    Interface - Supplies a pointer where a pointer to the first interface that
        matches will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if no protocol instances matched the search.

    EFI_INVALID_PARAMETER if the interface is NULL.

--*/
