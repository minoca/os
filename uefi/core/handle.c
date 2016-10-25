/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    handle.c

Abstract:

    This module implements handle and protocol support for the UEFI core.

Author:

    Evan Green 4-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipCoreDisconnectControllersUsingProtocolInterface (
    EFI_HANDLE EfiHandle,
    PEFI_PROTOCOL_INTERFACE ProtocolInterface
    );

EFI_STATUS
EfipCoreUnregisterProtocolNotifyEvent (
    EFI_EVENT Event
    );

PEFI_PROTOCOL_INTERFACE
EfipCoreRemoveInterfaceFromProtocol (
    PEFI_HANDLE_DATA Handle,
    EFI_GUID *Protocol,
    VOID *Interface
    );

PEFI_PROTOCOL_INTERFACE
EfipCoreGetProtocolInterface (
    EFI_HANDLE EfiHandle,
    EFI_GUID *Protocol
    );

PEFI_PROTOCOL_INTERFACE
EfipCoreFindProtocolInterface (
    PEFI_HANDLE_DATA Handle,
    EFI_GUID *Protocol,
    VOID *Interface
    );

VOID
EfipCoreNotifyProtocolEntry (
    PEFI_PROTOCOL_ENTRY ProtocolEntry
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_LOCK EfiProtocolDatabaseLock;
LIST_ENTRY EfiHandleList;
LIST_ENTRY EfiProtocolDatabase;
UINTN EfiHandleDatabaseKey;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreProtocolsPerHandle (
    EFI_HANDLE Handle,
    EFI_GUID ***ProtocolBuffer,
    UINTN *ProtocolBufferCount
    )

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

{

    EFI_GUID **Buffer;
    PLIST_ENTRY CurrentEntry;
    PEFI_HANDLE_DATA HandleData;
    UINTN ProtocolCount;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    EFI_STATUS Status;

    HandleData = Handle;
    Status = EfipCoreValidateHandle(Handle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if ((ProtocolBuffer == NULL) || (ProtocolBufferCount == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    *ProtocolBufferCount = 0;
    ProtocolCount = 0;
    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
    CurrentEntry = HandleData->ProtocolList.Next;
    while (CurrentEntry != &(HandleData->ProtocolList)) {
        ProtocolCount += 1;
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // If there are no protocol interfaces installed on the handle, then the
    // caller asked for something invalid.
    //

    if (ProtocolCount == 0) {
        Status = EFI_INVALID_PARAMETER;
        goto CoreProtocolsPerHandleEnd;
    }

    Buffer = EfiCoreAllocateBootPool(sizeof(EFI_GUID *) * ProtocolCount);
    if (Buffer == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CoreProtocolsPerHandleEnd;
    }

    *ProtocolBuffer = Buffer;
    *ProtocolBufferCount = ProtocolCount;
    ProtocolCount = 0;
    CurrentEntry = HandleData->ProtocolList.Next;
    while (CurrentEntry != &(HandleData->ProtocolList)) {
        ProtocolInterface = LIST_VALUE(CurrentEntry,
                                       EFI_PROTOCOL_INTERFACE,
                                       ListEntry);

        CurrentEntry = CurrentEntry->Next;

        ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

        Buffer[ProtocolCount] = &(ProtocolInterface->Protocol->ProtocolId);
        ProtocolCount += 1;
    }

    Status = EFI_SUCCESS;

CoreProtocolsPerHandleEnd:
    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreOpenProtocolInformation (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer,
    UINTN *EntryCount
    )

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

{

    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *Buffer;
    UINTN Count;
    PLIST_ENTRY CurrentEntry;
    PEFI_OPEN_PROTOCOL_DATA OpenData;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    UINTN Size;
    EFI_STATUS Status;

    *EntryBuffer = NULL;
    *EntryCount = 0;
    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);

    //
    // Look at each protocol interface for a match.
    //

    Status = EFI_NOT_FOUND;
    ProtocolInterface = EfipCoreGetProtocolInterface(Handle, Protocol);
    if (ProtocolInterface == NULL) {
        goto CoreOpenProtocolInformationEnd;
    }

    //
    // Count the number of open entries.
    //

    Count = 0;
    CurrentEntry = ProtocolInterface->OpenList.Next;
    while (CurrentEntry != &(ProtocolInterface->OpenList)) {
        CurrentEntry = CurrentEntry->Next;
        Count += 1;
    }

    ASSERT(Count == ProtocolInterface->OpenCount);

    if (Count == 0) {
        Size = sizeof(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY);

    } else {
        Size = Count * sizeof(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY);
    }

    Buffer = EfiCoreAllocateBootPool(Size);
    if (Buffer == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CoreOpenProtocolInformationEnd;
    }

    //
    // Now loop through again and fill in the information.
    //

    Count = 0;
    CurrentEntry = ProtocolInterface->OpenList.Next;
    while (CurrentEntry != &(ProtocolInterface->OpenList)) {
        OpenData = LIST_VALUE(CurrentEntry, EFI_OPEN_PROTOCOL_DATA, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

        Buffer[Count].AgentHandle = OpenData->AgentHandle;
        Buffer[Count].ControllerHandle = OpenData->ControllerHandle;
        Buffer[Count].Attributes = OpenData->Attributes;
        Buffer[Count].OpenCount = OpenData->OpenCount;
        Count += 1;
    }

    *EntryBuffer = Buffer;
    *EntryCount = Count;
    Status = EFI_SUCCESS;

CoreOpenProtocolInformationEnd:
    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreOpenProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface,
    EFI_HANDLE AgentHandle,
    EFI_HANDLE ControllerHandle,
    UINT32 Attributes
    )

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

{

    BOOLEAN ByDriver;
    PLIST_ENTRY CurrentEntry;
    BOOLEAN Disconnect;
    BOOLEAN ExactMatch;
    BOOLEAN Exclusive;
    PEFI_OPEN_PROTOCOL_DATA OpenData;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    EFI_STATUS Status;

    if (Protocol == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (Attributes != EFI_OPEN_PROTOCOL_TEST_PROTOCOL) {
        if (Interface == NULL) {
            return EFI_INVALID_PARAMETER;
        }

        *Interface = NULL;
    }

    Status = EfipCoreValidateHandle(Handle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Check for invalid attributes.
    //

    switch (Attributes) {
    case EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER:
        Status = EfipCoreValidateHandle(AgentHandle);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipCoreValidateHandle(ControllerHandle);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        if (Handle == ControllerHandle) {
            return EFI_INVALID_PARAMETER;
        }

        break;

    case EFI_OPEN_PROTOCOL_BY_DRIVER:
    case EFI_OPEN_PROTOCOL_BY_DRIVER | EFI_OPEN_PROTOCOL_EXCLUSIVE:
        Status = EfipCoreValidateHandle(AgentHandle);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipCoreValidateHandle(ControllerHandle);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        break;

    case EFI_OPEN_PROTOCOL_EXCLUSIVE:
        Status = EfipCoreValidateHandle(AgentHandle);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        break;

    case EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL:
    case EFI_OPEN_PROTOCOL_GET_PROTOCOL:
    case EFI_OPEN_PROTOCOL_TEST_PROTOCOL:
        break;

    default:
        return EFI_INVALID_PARAMETER;
    }

    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);

    //
    // Get the interface for this protocol on this handle.
    //

    ProtocolInterface = EfipCoreGetProtocolInterface(Handle, Protocol);
    if (ProtocolInterface == NULL) {
        Status = EFI_UNSUPPORTED;
        goto OpenProtocolEnd;
    }

    if (Attributes != EFI_OPEN_PROTOCOL_TEST_PROTOCOL) {
        *Interface = ProtocolInterface->Interface;
    }

    ByDriver = FALSE;
    Exclusive = FALSE;
    CurrentEntry = ProtocolInterface->OpenList.Next;
    while (CurrentEntry != &(ProtocolInterface->OpenList)) {
        OpenData = LIST_VALUE(CurrentEntry, EFI_OPEN_PROTOCOL_DATA, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

        ExactMatch = FALSE;
        if ((OpenData->AgentHandle == AgentHandle) &&
            (OpenData->Attributes == Attributes) &&
            (OpenData->ControllerHandle == ControllerHandle)) {

            ExactMatch = TRUE;
        }

        if ((OpenData->Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) != 0) {
            ByDriver = TRUE;
            if (ExactMatch != FALSE) {
                Status = EFI_ALREADY_STARTED;
                goto OpenProtocolEnd;
            }
        }

        if ((OpenData->Attributes & EFI_OPEN_PROTOCOL_EXCLUSIVE) != 0) {
            Exclusive = TRUE;

        } else if (ExactMatch != FALSE) {
            OpenData->OpenCount += 1;
            Status = EFI_SUCCESS;
            goto OpenProtocolEnd;
        }
    }

    //
    // Validate the attributes with what was found.
    //

    switch (Attributes) {
    case EFI_OPEN_PROTOCOL_BY_DRIVER:
        if ((Exclusive != FALSE) || (ByDriver != FALSE)) {
            Status = EFI_ACCESS_DENIED;
            goto OpenProtocolEnd;
        }

        break;

    case EFI_OPEN_PROTOCOL_BY_DRIVER | EFI_OPEN_PROTOCOL_EXCLUSIVE:
    case EFI_OPEN_PROTOCOL_EXCLUSIVE:
        if (Exclusive != FALSE) {
            Status = EFI_ACCESS_DENIED;
            goto OpenProtocolEnd;
        }

        if (ByDriver != FALSE) {
            do {
                Disconnect = FALSE;
                CurrentEntry = ProtocolInterface->OpenList.Next;
                while (CurrentEntry != &(ProtocolInterface->OpenList)) {
                    OpenData = LIST_VALUE(CurrentEntry,
                                          EFI_OPEN_PROTOCOL_DATA,
                                          ListEntry);

                    CurrentEntry = CurrentEntry->Next;
                    if ((OpenData->Attributes &
                         EFI_OPEN_PROTOCOL_BY_DRIVER) != 0) {

                        Disconnect = TRUE;
                        EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
                        Status = EfiCoreDisconnectController(
                                                         Handle,
                                                         OpenData->AgentHandle,
                                                         NULL);

                        EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
                        if (EFI_ERROR(Status)) {
                            Status = EFI_ACCESS_DENIED;
                            goto OpenProtocolEnd;
                        }
                    }
                }

            } while (Disconnect != FALSE);
        }

        break;

    case EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER:
    case EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL:
    case EFI_OPEN_PROTOCOL_GET_PROTOCOL:
    case EFI_OPEN_PROTOCOL_TEST_PROTOCOL:
    default:
        break;
    }

    if (AgentHandle == NULL) {
        Status = EFI_SUCCESS;
        goto OpenProtocolEnd;
    }

    //
    // Create a new open protocol entry.
    //

    OpenData = EfiCoreAllocateBootPool(sizeof(EFI_OPEN_PROTOCOL_DATA));
    if (OpenData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto OpenProtocolEnd;
    }

    OpenData->Magic = EFI_OPEN_PROTOCOL_MAGIC;
    OpenData->AgentHandle = AgentHandle;
    OpenData->ControllerHandle = ControllerHandle;
    OpenData->Attributes = Attributes;
    OpenData->OpenCount = 1;
    INSERT_BEFORE(&(OpenData->ListEntry), &(ProtocolInterface->OpenList));
    ProtocolInterface->OpenCount += 1;
    Status = EFI_SUCCESS;

OpenProtocolEnd:
    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreCloseProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    EFI_HANDLE AgentHandle,
    EFI_HANDLE ControllerHandle
    )

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

{

    PLIST_ENTRY CurrentEntry;
    PEFI_OPEN_PROTOCOL_DATA OpenData;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    EFI_STATUS Status;

    Status = EfipCoreValidateHandle(Handle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipCoreValidateHandle(AgentHandle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (ControllerHandle != NULL) {
        Status = EfipCoreValidateHandle(ControllerHandle);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    if (Protocol == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
    Status = EFI_NOT_FOUND;
    ProtocolInterface = EfipCoreGetProtocolInterface(Handle, Protocol);
    if (ProtocolInterface == NULL) {
        goto CoreCloseProtocolEnd;
    }

    //
    // Loop through the open data list looking for the agent handle.
    //

    CurrentEntry = ProtocolInterface->OpenList.Next;
    while (CurrentEntry != &(ProtocolInterface->OpenList)) {
        OpenData = LIST_VALUE(CurrentEntry, EFI_OPEN_PROTOCOL_DATA, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

        if ((OpenData->AgentHandle == AgentHandle) &&
            (OpenData->ControllerHandle == ControllerHandle)) {

            LIST_REMOVE(&(OpenData->ListEntry));
            ProtocolInterface->OpenCount -= 1;
            EfiCoreFreePool(OpenData);
            Status = EFI_SUCCESS;
        }
    }

CoreCloseProtocolEnd:
    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreHandleProtocol (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID **Interface
    )

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

{

    EFI_STATUS Status;

    Status = EfiCoreOpenProtocol(Handle,
                                 Protocol,
                                 Interface,
                                 EfiFirmwareImageHandle,
                                 NULL,
                                 EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreInstallProtocolInterface (
    EFI_HANDLE *Handle,
    EFI_GUID *Protocol,
    EFI_INTERFACE_TYPE InterfaceType,
    VOID *Interface
    )

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

{

    EFI_STATUS Status;

    Status = EfipCoreInstallProtocolInterfaceNotify(Handle,
                                                    Protocol,
                                                    InterfaceType,
                                                    Interface,
                                                    TRUE);

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreInstallMultipleProtocolInterfaces (
    EFI_HANDLE *Handle,
    ...
    )

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

{

    VA_LIST Arguments;
    EFI_HANDLE DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINTN Index;
    VOID *Interface;
    EFI_HANDLE OldHandle;
    EFI_TPL OldTpl;
    EFI_GUID *Protocol;
    EFI_STATUS Status;

    if (Handle == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Synchronize with notifications.
    //

    OldTpl = EfiCoreRaiseTpl(TPL_NOTIFY);
    OldHandle = *Handle;

    //
    // Check for a duplicate device path and install the protocol interfaces.
    //

    VA_START(Arguments, Handle);
    Status = EFI_SUCCESS;
    Index = 0;
    while (!EFI_ERROR(Status)) {

        //
        // The end of the list is marked with a NULL entry.
        //

        Protocol = VA_ARG(Arguments, EFI_GUID *);
        if (Protocol == NULL) {
            break;
        }

        Interface = VA_ARG(Arguments, VOID *);

        //
        // Make sure this is being installed on a something with a valid
        // device path.
        //

        if (EfiCoreCompareGuids(Protocol, &EfiDevicePathProtocolGuid) !=
            FALSE) {

            DeviceHandle = NULL;
            DevicePath = Interface;
            Status = EfiCoreLocateDevicePath(&EfiDevicePathProtocolGuid,
                                             &DevicePath,
                                             &DeviceHandle);

            if ((!EFI_ERROR(Status)) &&
                (DeviceHandle != NULL) &&
                (EfiCoreIsDevicePathEnd(DevicePath) != FALSE)) {

                Status = EFI_ALREADY_STARTED;
                continue;
            }
        }

        Status = EfiCoreInstallProtocolInterface(Handle,
                                                 Protocol,
                                                 EFI_NATIVE_INTERFACE,
                                                 Interface);

        Index += 1;
    }

    VA_END(Arguments);

    //
    // If there was an error, remove all the interfaces that were installed
    // without errors.
    //

    if (EFI_ERROR(Status)) {
        VA_START(Arguments, Handle);
        while (Index > 1) {
            Protocol = VA_ARG(Arguments, EFI_GUID *);
            Interface = VA_ARG(Arguments, VOID *);
            EfiCoreUninstallProtocolInterface(*Handle, Protocol, Interface);
            Index -= 1;
        }

        VA_END(Arguments);
        *Handle = OldHandle;
    }

    EfiCoreRestoreTpl(OldTpl);
    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreReinstallProtocolInterface (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID *OldInterface,
    VOID *NewInterface
    )

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

{

    PEFI_HANDLE_DATA HandleData;
    PEFI_PROTOCOL_ENTRY ProtocolEntry;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    EFI_STATUS Status;

    Status = EfipCoreValidateHandle(Handle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (Protocol == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    HandleData = (PEFI_HANDLE_DATA)Handle;
    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);

    //
    // Find the alleged protocol interface.
    //

    ProtocolInterface = EfipCoreFindProtocolInterface(Handle,
                                                      Protocol,
                                                      OldInterface);

    if (ProtocolInterface == NULL) {
        Status = EFI_NOT_FOUND;
        goto CoreReinstallProtocolInterfaceEnd;
    }

    //
    // Disconnect everybody using this protocol interface.
    //

    Status = EfipCoreDisconnectControllersUsingProtocolInterface(
                                                            Handle,
                                                            ProtocolInterface);

    if (EFI_ERROR(Status)) {
        goto CoreReinstallProtocolInterfaceEnd;
    }

    ProtocolInterface = EfipCoreRemoveInterfaceFromProtocol(HandleData,
                                                            Protocol,
                                                            OldInterface);

    if (ProtocolInterface == NULL) {
        Status = EFI_NOT_FOUND;
        goto CoreReinstallProtocolInterfaceEnd;
    }

    ProtocolEntry = ProtocolInterface->Protocol;

    //
    // Update the interface on the protocol, and re-add it to the end of the
    // protocol entry list
    //

    ProtocolInterface->Interface = NewInterface;
    INSERT_BEFORE(&(ProtocolInterface->ProtocolListEntry),
                  &(ProtocolEntry->ProtocolList));

    EfiHandleDatabaseKey += 1;
    HandleData->Key = EfiHandleDatabaseKey;

    //
    // Reconnect the controller. The return code is ignored intentionally.
    //

    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    EfiCoreConnectController(Handle, NULL, NULL, TRUE);
    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
    Status = EFI_SUCCESS;

CoreReinstallProtocolInterfaceEnd:
    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return Status;
}

EFI_STATUS
EfiCoreUninstallProtocolInterface (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    VOID *Interface
    )

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

{

    PEFI_HANDLE_DATA HandleData;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    EFI_STATUS Status;

    if (Protocol == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfipCoreValidateHandle(Handle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);

    //
    // Check to see if the protocol exists on the given handle, and that the
    // interface matches the one given.
    //

    ProtocolInterface = EfipCoreFindProtocolInterface(Handle,
                                                      Protocol,
                                                      Interface);

    if (ProtocolInterface == NULL) {
        Status = EFI_NOT_FOUND;
        goto CoreUninstallProtocolInterfaceEnd;
    }

    //
    // Attempt to disconnect all drivers using the protocol interface that is
    // about to be removed.
    //

    Status = EfipCoreDisconnectControllersUsingProtocolInterface(
                                                            Handle,
                                                            ProtocolInterface);

    if (EFI_ERROR(Status)) {
        goto CoreUninstallProtocolInterfaceEnd;
    }

    //
    // Remove the protocol interface from the protocol.
    //

    Status = EFI_NOT_FOUND;
    HandleData = (PEFI_HANDLE_DATA)Handle;
    ProtocolInterface = EfipCoreRemoveInterfaceFromProtocol(HandleData,
                                                            Protocol,
                                                            Interface);

    if (ProtocolInterface != NULL) {
        EfiHandleDatabaseKey += 1;
        HandleData->Key = EfiHandleDatabaseKey;
        LIST_REMOVE(&(ProtocolInterface->ListEntry));
        ProtocolInterface->Magic = 0;
        EfiCoreFreePool(ProtocolInterface);
        Status = EFI_SUCCESS;
    }

    //
    // If there are no more handlers for the handle, destroy the handle.
    //

    if (LIST_EMPTY(&(HandleData->ProtocolList)) != FALSE) {
        HandleData->Magic = 0;
        LIST_REMOVE(&(HandleData->ListEntry));
        EfiCoreFreePool(HandleData);
    }

CoreUninstallProtocolInterfaceEnd:
    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreUninstallMultipleProtocolInterfaces (
    EFI_HANDLE Handle,
    ...
    )

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

{

    VA_LIST Arguments;
    UINTN Index;
    VOID *Interface;
    EFI_GUID *Protocol;
    EFI_STATUS Status;

    VA_START(Arguments, Handle);
    Index = 0;
    Status = EFI_SUCCESS;
    while (!EFI_ERROR(Status)) {

        //
        // The arguments are terminated with a NULL entry.
        //

        Protocol = VA_ARG(Arguments, EFI_GUID *);
        if (Protocol == NULL) {
            break;
        }

        Interface = VA_ARG(Arguments, VOID *);
        Status = EfiCoreUninstallProtocolInterface(Handle, Protocol, Interface);
        Index += 1;
    }

    VA_END(Arguments);

    //
    // If there was an error, reinstall all the interfaces that were
    // uninstalled without error.
    //

    if (EFI_ERROR(Status)) {
        VA_START(Arguments, Handle);
        while (Index > 1) {
            Protocol = VA_ARG(Arguments, EFI_GUID *);
            Interface = VA_ARG(Arguments, VOID *);
            EfiCoreInstallProtocolInterface(&Handle,
                                            Protocol,
                                            EFI_NATIVE_INTERFACE,
                                            Interface);

            Index -= 1;
        }

        VA_END(Arguments);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreRegisterProtocolNotify (
    EFI_GUID *Protocol,
    EFI_EVENT Event,
    VOID **Registration
    )

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

{

    PEFI_PROTOCOL_ENTRY ProtocolEntry;
    PEFI_PROTOCOL_NOTIFY ProtocolNotify;
    EFI_STATUS Status;

    if ((Protocol == NULL) || (Event == NULL) || (Registration == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
    ProtocolNotify = NULL;
    ProtocolEntry = EfipCoreFindProtocolEntry(Protocol, TRUE);
    if (ProtocolEntry != NULL) {
        ProtocolNotify = EfiCoreAllocateBootPool(sizeof(EFI_PROTOCOL_NOTIFY));
        if (ProtocolNotify != NULL) {
            ProtocolNotify->Magic = EFI_PROTOCOL_NOTIFY_MAGIC;
            ProtocolNotify->Protocol = ProtocolEntry;
            ProtocolNotify->Event = Event;
            ProtocolNotify->Position = &(ProtocolEntry->ProtocolList);
            INSERT_BEFORE(&(ProtocolNotify->ListEntry),
                          &(ProtocolEntry->NotifyList));
        }
    }

    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    Status = EFI_OUT_OF_RESOURCES;
    if (ProtocolNotify != NULL) {
        *Registration = ProtocolNotify;
        Status = EFI_SUCCESS;
    }

    return Status;
}

VOID
EfiCoreInitializeHandleDatabase (
    VOID
    )

/*++

Routine Description:

    This routine initializes EFI handle and protocol support.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfiCoreInitializeLock(&EfiProtocolDatabaseLock, TPL_NOTIFY);
    INITIALIZE_LIST_HEAD(&EfiProtocolDatabase);
    INITIALIZE_LIST_HEAD(&EfiHandleList);
    EfiHandleDatabaseKey = 0;
    return;
}

EFI_STATUS
EfipCoreInstallProtocolInterfaceNotify (
    EFI_HANDLE *EfiHandle,
    EFI_GUID *Protocol,
    EFI_INTERFACE_TYPE InterfaceType,
    VOID *Interface,
    BOOLEAN Notify
    )

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

{

    VOID *ExistingInterface;
    PEFI_HANDLE_DATA Handle;
    PEFI_PROTOCOL_ENTRY ProtocolEntry;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    EFI_STATUS Status;

    if ((EfiHandle == NULL) || (Protocol == NULL) ||
        (InterfaceType != EFI_NATIVE_INTERFACE)) {

        return EFI_INVALID_PARAMETER;
    }

    ProtocolEntry = NULL;
    ProtocolInterface = NULL;
    Handle = NULL;
    if (*EfiHandle != NULL) {
        Status = EfiCoreHandleProtocol(*EfiHandle,
                                       Protocol,
                                       &ExistingInterface);

        if (!EFI_ERROR(Status)) {
            return EFI_INVALID_PARAMETER;
        }
    }

    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
    ProtocolEntry = EfipCoreFindProtocolEntry(Protocol, TRUE);
    if (ProtocolEntry == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CoreInstallProtocolInterfaceNotifyEnd;
    }

    ProtocolInterface = EfiCoreAllocateBootPool(sizeof(EFI_PROTOCOL_INTERFACE));
    if (ProtocolInterface == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CoreInstallProtocolInterfaceNotifyEnd;
    }

    EfiCoreSetMemory(ProtocolInterface, sizeof(EFI_PROTOCOL_INTERFACE), 0);

    //
    // Create a handle if one was not supplied.
    //

    Handle = *EfiHandle;
    if (Handle == NULL) {
        Handle = EfiCoreAllocateBootPool(sizeof(EFI_HANDLE_DATA));
        if (Handle == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto CoreInstallProtocolInterfaceNotifyEnd;
        }

        EfiCoreSetMemory(Handle, sizeof(EFI_HANDLE_DATA), 0);
        Handle->Magic = EFI_HANDLE_MAGIC;
        INITIALIZE_LIST_HEAD(&(Handle->ProtocolList));
        EfiHandleDatabaseKey += 1;
        Handle->Key = EfiHandleDatabaseKey;
        INSERT_BEFORE(&(Handle->ListEntry), &EfiHandleList);
    }

    Status = EfipCoreValidateHandle(Handle);
    if (EFI_ERROR(Status)) {
        goto CoreInstallProtocolInterfaceNotifyEnd;
    }

    //
    // Each added interface must be unique.
    //

    ASSERT(EfipCoreFindProtocolInterface(Handle, Protocol, Interface) == NULL);

    //
    // Initialize the protocol interface structure.
    //

    ProtocolInterface->Magic = EFI_PROTOCOL_INTERFACE_MAGIC;
    ProtocolInterface->Handle = Handle;
    ProtocolInterface->Protocol = ProtocolEntry;
    ProtocolInterface->Interface = Interface;
    INITIALIZE_LIST_HEAD(&(ProtocolInterface->OpenList));
    ProtocolInterface->OpenCount = 0;

    //
    // Add this protocol interface to the head of the supported protocol list
    // for this handle.
    //

    INSERT_AFTER(&(ProtocolInterface->ListEntry), &(Handle->ProtocolList));

    //
    // Add this protocol interface to the end of the list for the protocol
    // entry.
    //

    INSERT_BEFORE(&(ProtocolInterface->ProtocolListEntry),
                  &(ProtocolEntry->ProtocolList));

    //
    // Notify anybody listening for this protocol.
    //

    if (Notify != FALSE) {
        EfipCoreNotifyProtocolEntry(ProtocolEntry);
    }

    Status = EFI_SUCCESS;

CoreInstallProtocolInterfaceNotifyEnd:
    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    if (!EFI_ERROR(Status)) {
        *EfiHandle = Handle;

    } else {
        if (ProtocolInterface != NULL) {
            EfiCoreFreePool(ProtocolInterface);
        }
    }

    return Status;
}

EFI_STATUS
EfipCoreUnregisterProtocolNotify (
    EFI_EVENT Event
    )

/*++

Routine Description:

    This routine removes all the events in the protocol database that match
    the given event.

Arguments:

    Event - Supplies the event that is being destroyed.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    do {
        Status = EfipCoreUnregisterProtocolNotifyEvent(Event);

    } while (!EFI_ERROR(Status));

    return EFI_SUCCESS;
}

PEFI_PROTOCOL_ENTRY
EfipCoreFindProtocolEntry (
    EFI_GUID *Protocol,
    BOOLEAN Create
    )

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

{

    PLIST_ENTRY CurrentEntry;
    PEFI_PROTOCOL_ENTRY Item;
    PEFI_PROTOCOL_ENTRY ProtocolEntry;

    ASSERT(EfiCoreIsLockHeld(&EfiProtocolDatabaseLock) != FALSE);

    //
    // Search the database for the matching GUID.
    //

    ProtocolEntry = NULL;
    CurrentEntry = EfiProtocolDatabase.Next;
    while (CurrentEntry != &EfiProtocolDatabase) {
        Item = LIST_VALUE(CurrentEntry, EFI_PROTOCOL_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(Item->Magic == EFI_PROTOCOL_ENTRY_MAGIC);

        if (EfiCoreCompareGuids(&(Item->ProtocolId), Protocol) != FALSE) {
            ProtocolEntry = Item;
            break;
        }
    }

    if ((ProtocolEntry == NULL) && (Create != FALSE)) {
        ProtocolEntry = EfiCoreAllocateBootPool(sizeof(EFI_PROTOCOL_ENTRY));
        if (ProtocolEntry != NULL) {
            ProtocolEntry->Magic = EFI_PROTOCOL_ENTRY_MAGIC;
            EfiCoreCopyMemory(&(ProtocolEntry->ProtocolId),
                              Protocol,
                              sizeof(EFI_GUID));

            INITIALIZE_LIST_HEAD(&(ProtocolEntry->ProtocolList));
            INITIALIZE_LIST_HEAD(&(ProtocolEntry->NotifyList));
            INSERT_BEFORE(&(ProtocolEntry->ListEntry), &EfiProtocolDatabase);
        }
    }

    return ProtocolEntry;
}

EFI_STATUS
EfipCoreValidateHandle (
    EFI_HANDLE Handle
    )

/*++

Routine Description:

    This routine validates that the given handle is a valid EFI_HANDLE.

Arguments:

    Handle - Supplies the handle to validate.

Return Value:

    EFI_SUCCESS if on success.

    EFI_INVALID_PARAMETER if the handle is not in fact valid.

--*/

{

    PEFI_HANDLE_DATA HandleData;

    HandleData = Handle;
    if ((HandleData == NULL) || (HandleData->Magic != EFI_HANDLE_MAGIC)) {
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

UINT64
EfipCoreGetHandleDatabaseKey (
    VOID
    )

/*++

Routine Description:

    This routine returns the current handle database key.

Arguments:

    None.

Return Value:

    Returns the current handle database key.

--*/

{

    return EfiHandleDatabaseKey;
}

VOID
EfipCoreConnectHandlesByKey (
    UINT64 Key
    )

/*++

Routine Description:

    This routine connects any handles that were created or modified while an
    image executed.

Arguments:

    Key - Supplies the database key snapped when the image was started.

Return Value:

    None.

--*/

{

    UINTN Count;
    PLIST_ENTRY CurrentEntry;
    PEFI_HANDLE_DATA Handle;
    EFI_HANDLE *HandleBuffer;
    UINTN Index;

    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);

    //
    // Loop through once to get the number of handles created after the
    // given key.
    //

    Count = 0;
    CurrentEntry = EfiHandleList.Next;
    while (CurrentEntry != &EfiHandleList) {
        Handle = LIST_VALUE(CurrentEntry, EFI_HANDLE_DATA, ListEntry);

        ASSERT(Handle->Magic == EFI_HANDLE_MAGIC);

        if (Handle->Key > Key) {
            Count += 1;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Create a buffer to hold all those handles.
    //

    HandleBuffer = EfiCoreAllocateBootPool(Count * sizeof(EFI_HANDLE));
    if (HandleBuffer == NULL) {
        EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
        return;
    }

    //
    // Loop through again to populate the array.
    //

    Count = 0;
    CurrentEntry = EfiHandleList.Next;
    while (CurrentEntry != &EfiHandleList) {
        Handle = LIST_VALUE(CurrentEntry, EFI_HANDLE_DATA, ListEntry);

        ASSERT(Handle->Magic == EFI_HANDLE_MAGIC);

        if (Handle->Key > Key) {
            HandleBuffer[Count] = Handle;
            Count += 1;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);

    //
    // Now, with the protocol lock not held, go through and connect the
    // controllers of the handles.
    //

    for (Index = 0; Index < Count; Index += 1) {
        EfiCoreConnectController(HandleBuffer[Index], NULL, NULL, TRUE);
    }

    EfiCoreFreePool(HandleBuffer);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipCoreDisconnectControllersUsingProtocolInterface (
    EFI_HANDLE EfiHandle,
    PEFI_PROTOCOL_INTERFACE ProtocolInterface
    )

/*++

Routine Description:

    This routine attempts to disconnect all drivers using the given protocol
    interface. If this fails, it will attempt to reconnect any drivers it
    disconnected. This routine assumes the protocol database lock is already
    held. It may briefly release it and reacquire it.

Arguments:

    EfiHandle - Supplies the handle.

    ProtocolInterface - Supplies a pointer to the interface being removed.

Return Value:

    EFI status code.

--*/

{

    UINT32 Attributes;
    PLIST_ENTRY CurrentEntry;
    BOOLEAN ItemFound;
    PEFI_OPEN_PROTOCOL_DATA OpenData;
    EFI_STATUS Status;

    Attributes = EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL |
                 EFI_OPEN_PROTOCOL_GET_PROTOCOL |
                 EFI_OPEN_PROTOCOL_TEST_PROTOCOL;

    Status = EFI_SUCCESS;

    //
    // Attempt to disconnect all drivers from this protocol interface.
    //

    do {
        ItemFound = FALSE;
        CurrentEntry = ProtocolInterface->OpenList.Next;
        while ((CurrentEntry != &(ProtocolInterface->OpenList)) &&
               (ItemFound == FALSE)) {

            OpenData = LIST_VALUE(CurrentEntry,
                                  EFI_OPEN_PROTOCOL_DATA,
                                  ListEntry);

            ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

            if ((OpenData->Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) != 0) {
                ItemFound = TRUE;
                EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
                Status = EfiCoreDisconnectController(EfiHandle,
                                                     OpenData->AgentHandle,
                                                     NULL);

                EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
                if (EFI_ERROR(Status)) {
                    ItemFound = FALSE;
                    break;
                }
            }

            CurrentEntry = CurrentEntry->Next;
        }

    } while (ItemFound != FALSE);

    //
    // Attempt to remove BY_HANDLE_PROTOCOL, GET_PROTOCOL, and TEST_PROTOCOL
    // open list entries.
    //

    if (!EFI_ERROR(Status)) {
        do {
            ItemFound = FALSE;
            while ((CurrentEntry != &(ProtocolInterface->OpenList)) &&
                   (ItemFound == FALSE)) {

                OpenData = LIST_VALUE(CurrentEntry,
                                      EFI_OPEN_PROTOCOL_DATA,
                                      ListEntry);

                ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

                CurrentEntry = CurrentEntry->Next;
                if ((OpenData->Attributes & Attributes) != 0) {
                    ItemFound = TRUE;
                    LIST_REMOVE(&(OpenData->ListEntry));
                    ProtocolInterface->OpenCount -= 1;
                    EfiCoreFreePool(OpenData);
                }
            }

        } while (ItemFound != FALSE);
    }

    //
    // If there are errors or the protocol interface still has open items,
    // reconnect the drivers and return an error.
    //

    if ((EFI_ERROR(Status)) || (ProtocolInterface->OpenCount > 0)) {
        EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
        EfiCoreConnectController(EfiHandle, NULL, NULL, TRUE);
        EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
        Status = EFI_ACCESS_DENIED;
    }

    return Status;
}

EFI_STATUS
EfipCoreUnregisterProtocolNotifyEvent (
    EFI_EVENT Event
    )

/*++

Routine Description:

    This routine removes an every from a register protocol notify list on a
    protocol.

Arguments:

    Event - Supplies the event that is being destroyed.

Return Value:

    EFI status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY NotifyEntry;
    PEFI_PROTOCOL_ENTRY ProtocolEntry;
    PEFI_PROTOCOL_NOTIFY ProtocolNotify;

    EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
    CurrentEntry = EfiProtocolDatabase.Next;
    while (CurrentEntry != &EfiProtocolDatabase) {
        ProtocolEntry = LIST_VALUE(CurrentEntry, EFI_PROTOCOL_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(ProtocolEntry->Magic == EFI_PROTOCOL_ENTRY_MAGIC);

        NotifyEntry = ProtocolEntry->NotifyList.Next;
        while (NotifyEntry != &(ProtocolEntry->NotifyList)) {
            ProtocolNotify = LIST_VALUE(NotifyEntry,
                                        EFI_PROTOCOL_NOTIFY,
                                        ListEntry);

            NotifyEntry = NotifyEntry->Next;

            ASSERT(ProtocolNotify->Magic == EFI_PROTOCOL_NOTIFY_MAGIC);

            if (ProtocolNotify->Event == Event) {
                LIST_REMOVE(&(ProtocolNotify->ListEntry));
                EfiCoreFreePool(ProtocolNotify);
                EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
                return EFI_SUCCESS;
            }
        }
    }

    EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    return EFI_NOT_FOUND;
}

PEFI_PROTOCOL_INTERFACE
EfipCoreRemoveInterfaceFromProtocol (
    PEFI_HANDLE_DATA Handle,
    EFI_GUID *Protocol,
    VOID *Interface
    )

/*++

Routine Description:

    This routine removes the given protocol from the protocol list (but not
    the handle list). This routine assumes the protocol database lock is
    already held.

Arguments:

    Handle - Supplies the handle to remove the protocol on.

    Protocol - Supplies a pointer to the protocol GUID to be removed.

    Interface - Supplies the interface value of the protocol.

Return Value:

    Returns a pointer to the protocol interface on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_PROTOCOL_ENTRY ProtocolEntry;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    PEFI_PROTOCOL_NOTIFY ProtocolNotify;

    ASSERT(EfiCoreIsLockHeld(&EfiProtocolDatabaseLock) != FALSE);

    ProtocolInterface = EfipCoreFindProtocolInterface(Handle,
                                                      Protocol,
                                                      Interface);

    if (ProtocolInterface != NULL) {
        ProtocolEntry = ProtocolInterface->Protocol;

        //
        // If there's a protocol notify location pointing to this entry, back
        // it up one.
        //

        CurrentEntry = ProtocolEntry->NotifyList.Next;
        while (CurrentEntry != &(ProtocolEntry->NotifyList)) {
            ProtocolNotify = LIST_VALUE(CurrentEntry,
                                        EFI_PROTOCOL_NOTIFY,
                                        ListEntry);

            ASSERT(ProtocolNotify->Magic == EFI_PROTOCOL_NOTIFY_MAGIC);

            if (ProtocolNotify->Position ==
                &(ProtocolInterface->ProtocolListEntry)) {

                ProtocolNotify->Position =
                                 ProtocolInterface->ProtocolListEntry.Previous;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        LIST_REMOVE(&(ProtocolInterface->ProtocolListEntry));
    }

    return ProtocolInterface;
}

PEFI_PROTOCOL_INTERFACE
EfipCoreGetProtocolInterface (
    EFI_HANDLE EfiHandle,
    EFI_GUID *Protocol
    )

/*++

Routine Description:

    This routine returns the protocol interface for the given protocol GUID on
    the given handle.

Arguments:

    EfiHandle - Supplies the handle.

    Protocol - Supplies a pointer to the protocol GUID to find within the
        handle.

Return Value:

    Returns a pointer to the protocol interface on success.

    NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_HANDLE_DATA Handle;
    PEFI_PROTOCOL_INTERFACE Interface;
    PEFI_PROTOCOL_ENTRY ProtocolEntry;
    EFI_STATUS Status;

    Status = EfipCoreValidateHandle(EfiHandle);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    Handle = EfiHandle;
    CurrentEntry = Handle->ProtocolList.Next;
    while (CurrentEntry != &(Handle->ProtocolList)) {
        Interface = LIST_VALUE(CurrentEntry, EFI_PROTOCOL_INTERFACE, ListEntry);

        ASSERT(Interface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

        ProtocolEntry = Interface->Protocol;
        if (EfiCoreCompareGuids(&(ProtocolEntry->ProtocolId), Protocol) !=
            FALSE) {

            return Interface;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

PEFI_PROTOCOL_INTERFACE
EfipCoreFindProtocolInterface (
    PEFI_HANDLE_DATA Handle,
    EFI_GUID *Protocol,
    VOID *Interface
    )

/*++

Routine Description:

    This routine attempts to find a protocol interface for the given handle
    and protocol.

Arguments:

    Handle - Supplies the handle to search on.

    Protocol - Supplies a pointer to the protocol's GUID.

    Interface - Supplies the interface value for the protocol being searched.

Return Value:

    Returns a pointer to the protocol interface if one matches.

    NULL if nothing was found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_PROTOCOL_ENTRY ProtocolEntry;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;

    ASSERT(EfiCoreIsLockHeld(&EfiProtocolDatabaseLock) != FALSE);

    ProtocolInterface = NULL;
    ProtocolEntry = EfipCoreFindProtocolEntry(Protocol, FALSE);
    if (ProtocolEntry != NULL) {
        CurrentEntry = Handle->ProtocolList.Next;
        while (CurrentEntry != &(Handle->ProtocolList)) {
            ProtocolInterface = LIST_VALUE(CurrentEntry,
                                           EFI_PROTOCOL_INTERFACE,
                                           ListEntry);

            CurrentEntry = CurrentEntry->Next;

            ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

            if ((ProtocolInterface->Interface == Interface) &&
                (ProtocolInterface->Protocol == ProtocolEntry)) {

                break;
            }

            ProtocolInterface = NULL;
        }
    }

    return ProtocolInterface;
}

VOID
EfipCoreNotifyProtocolEntry (
    PEFI_PROTOCOL_ENTRY ProtocolEntry
    )

/*++

Routine Description:

    This routine signals the event for every protocol in the given protocol
    entry. This routine assumes the protocol database lock is held.

Arguments:

    ProtocolEntry - Supplies a pointer to the protocol entry to signal.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_PROTOCOL_NOTIFY Notify;

    ASSERT(EfiCoreIsLockHeld(&EfiProtocolDatabaseLock) != FALSE);

    CurrentEntry = ProtocolEntry->NotifyList.Next;
    while (CurrentEntry != &(ProtocolEntry->NotifyList)) {
        Notify = LIST_VALUE(CurrentEntry, EFI_PROTOCOL_NOTIFY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(Notify->Magic == EFI_PROTOCOL_NOTIFY_MAGIC);

        EfiCoreSignalEvent(Notify->Event);
    }

    return;
}

