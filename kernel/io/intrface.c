/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intrface.c

Abstract:

    This module implements support for device interfaces.

Author:

    Evan Green 19-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an interface directory, which contains device
    interface instances and listeners.

Members:

    Header - Supplies the standard object manager header.

    Uuid - Supplies the UUID of the interface.

--*/

typedef struct _INTERFACE_DIRECTORY {
    OBJECT_HEADER Header;
    UUID Uuid;
} INTERFACE_DIRECTORY, *PINTERFACE_DIRECTORY;

/*++

Structure Description:

    This structure defines a device interface instance.

Members:

    Header - Stores the object manager header.

    Device - Stores a pointer to the device this interface is attached to.

    InterfaceBuffer - Stores a pointer to the interface buffer.

    InterfaceBufferSize - Stores the size of the interface buffer.

--*/

typedef struct _DEVICE_INTERFACE_INSTANCE {
    OBJECT_HEADER Header;
    PDEVICE Device;
    PVOID InterfaceBuffer;
    ULONG InterfaceBufferSize;
} DEVICE_INTERFACE_INSTANCE, *PDEVICE_INTERFACE_INSTANCE;

/*++

Structure Description:

    This structure defines an interface listener.

Members:

    Header - Stores the standard object manager header.

    CallbackRoutine - Stores a pointer to the listener's callback routine.

    Device - Stores an optional pointer to a specific device to listen to.

    Context - Stores a pointer supplied by and passed back to the interface
        listener during notifications.

--*/

typedef struct _INTERFACE_LISTENER {
    OBJECT_HEADER Header;
    PINTERFACE_NOTIFICATION_CALLBACK CallbackRoutine;
    PDEVICE Device;
    PVOID Context;
} INTERFACE_LISTENER, *PINTERFACE_LISTENER;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopNotifyInterfaceListeners (
    PINTERFACE_DIRECTORY InterfaceDirectory,
    PDEVICE_INTERFACE_INSTANCE InterfaceInstance,
    PDEVICE Device,
    BOOL Arrival
    );

VOID
IopPrintUuidToString (
    PSTR String,
    ULONG StringSize,
    PUUID Uuid
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the directory of all exposed interfaces.
//

POBJECT_HEADER IoInterfaceDirectory = NULL;
PQUEUED_LOCK IoInterfaceLock = NULL;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoCreateInterface (
    PUUID InterfaceUuid,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize
    )

/*++

Routine Description:

    This routine creates a device interface. Interfaces start out disabled. The
    Interface/device pair must be unique, there cannot be two interfaces for
    the same UUID and device.

Arguments:

    InterfaceUuid - Supplies a pointer to the UUID identifying the interface.

    Device - Supplies a pointer to the device exposing the interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer.

    InterfaceBufferSize - Supplies the size of the interface buffer, in bytes.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the interface or device were not specified.

    STATUS_NO_MEMORY if allocations could not be made.

    STATUS_DUPLICATE_ENTRY if an interface already exists for this device.

--*/

{

    PINTERFACE_DIRECTORY InterfaceDirectory;
    PDEVICE_INTERFACE_INSTANCE InterfaceInstance;
    CHAR Name[UUID_STRING_LENGTH + 1];
    ULONG NameLength;
    KSTATUS Status;

    if ((InterfaceUuid == NULL) || (Device == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    IopPrintUuidToString(Name, UUID_STRING_LENGTH + 1, InterfaceUuid);
    KeAcquireQueuedLock(IoInterfaceLock);

    //
    // Look up the interface directory. Create it if it does not exist.
    //

    InterfaceDirectory = ObFindObject(Name,
                                      UUID_STRING_LENGTH + 1,
                                      IoInterfaceDirectory);

    if (InterfaceDirectory == NULL) {
        InterfaceDirectory = ObCreateObject(ObjectInterface,
                                            IoInterfaceDirectory,
                                            Name,
                                            sizeof(Name),
                                            sizeof(INTERFACE_DIRECTORY),
                                            NULL,
                                            0,
                                            DEVICE_INTERFACE_ALLOCATION_TAG);

        if (InterfaceDirectory == NULL) {
            Status = STATUS_NO_MEMORY;
            goto CreateInterfaceEnd;
        }

        InterfaceDirectory->Uuid = *InterfaceUuid;
    }

    ASSERT(UUID_STRING_LENGTH + 1 > ((sizeof(UINTN) * 2) + 1));

    //
    // Attempt to open the interface instance for this device. If this succeeds,
    // fail this function.
    //

    NameLength = RtlPrintToString(Name,
                                  UUID_STRING_LENGTH + 1,
                                  CharacterEncodingDefault,
                                  "%08x",
                                  Device);

    if (NameLength > UUID_STRING_LENGTH + 1) {
        NameLength = UUID_STRING_LENGTH + 1;
    }

    InterfaceInstance = ObFindObject(Name,
                                     NameLength,
                                     (POBJECT_HEADER)InterfaceDirectory);

    if (InterfaceInstance != NULL) {
        ObReleaseReference(InterfaceInstance);
        Status = STATUS_DUPLICATE_ENTRY;
        goto CreateInterfaceEnd;
    }

    InterfaceInstance = ObCreateObject(ObjectInterfaceInstance,
                                       InterfaceDirectory,
                                       Name,
                                       sizeof(Name),
                                       sizeof(DEVICE_INTERFACE_INSTANCE),
                                       NULL,
                                       0,
                                       DEVICE_INTERFACE_ALLOCATION_TAG);

    if (InterfaceInstance == NULL) {
        Status = STATUS_NO_MEMORY;
        goto CreateInterfaceEnd;
    }

    InterfaceInstance->Device = Device;
    InterfaceInstance->InterfaceBuffer = InterfaceBuffer;
    InterfaceInstance->InterfaceBufferSize = InterfaceBufferSize;

    //
    // Notify listeners of the interface arrival.
    //

    IopNotifyInterfaceListeners(InterfaceDirectory,
                                InterfaceInstance,
                                Device,
                                TRUE);

    Status = STATUS_SUCCESS;

CreateInterfaceEnd:
    KeReleaseQueuedLock(IoInterfaceLock);
    if (InterfaceDirectory != NULL) {
        ObReleaseReference(InterfaceDirectory);
    }

    return Status;
}

KERNEL_API
KSTATUS
IoDestroyInterface (
    PUUID InterfaceUuid,
    PDEVICE Device,
    PVOID InterfaceBuffer
    )

/*++

Routine Description:

    This routine destroys a previously created interface. All parties
    registered for notifications on this interface will be notified that the
    interface is going down.

Arguments:

    InterfaceUuid - Supplies a pointer to the UUID identifying the interface.

    Device - Supplies a pointer to the device supplied on registration.

    InterfaceBuffer - Supplies a pointer to the interface buffer for the
        specific interface to tear down. This needs to match the interface
        buffer used when the interface was created.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the interface or device is invalid.

    STATUS_NOT_FOUND if the interface could not be found.

--*/

{

    PINTERFACE_DIRECTORY InterfaceDirectory;
    PDEVICE_INTERFACE_INSTANCE InterfaceInstance;
    CHAR Name[UUID_STRING_LENGTH + 1];
    ULONG NameLength;
    KSTATUS Status;

    if (InterfaceUuid == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    IopPrintUuidToString(Name, UUID_STRING_LENGTH + 1, InterfaceUuid);
    KeAcquireQueuedLock(IoInterfaceLock);

    //
    // Look up the interface directory.
    //

    InterfaceDirectory = ObFindObject(Name,
                                      UUID_STRING_LENGTH + 1,
                                      IoInterfaceDirectory);

    if (InterfaceDirectory == NULL) {
        Status = STATUS_NOT_FOUND;
        goto DeleteInterfaceEnd;
    }

    //
    // Look up the interface instance.
    //

    NameLength = RtlPrintToString(Name,
                                  UUID_STRING_LENGTH + 1,
                                  CharacterEncodingDefault,
                                  "%08x",
                                  Device);

    if (NameLength > UUID_STRING_LENGTH + 1) {
        NameLength = UUID_STRING_LENGTH + 1;
    }

    InterfaceInstance = ObFindObject(Name,
                                     NameLength,
                                     (POBJECT_HEADER)InterfaceDirectory);

    if (InterfaceInstance == NULL) {
        Status = STATUS_NOT_FOUND;
        goto DeleteInterfaceEnd;
    }

    //
    // TODO: Rework the data structures for interfaces. There's no need to
    // use the object manager.
    //

    ASSERT(InterfaceInstance->InterfaceBuffer == InterfaceBuffer);

    //
    // Notify listeners that the interface is going down.
    //

    IopNotifyInterfaceListeners(InterfaceDirectory,
                                InterfaceInstance,
                                Device,
                                FALSE);

    //
    // Remove the reference from the interface, once because of the find and
    // another because of the initial creation.
    //

    ObReleaseReference(InterfaceInstance);
    ObReleaseReference(InterfaceInstance);
    Status = STATUS_SUCCESS;

DeleteInterfaceEnd:

    //
    // If necessary, release the pointer to the interface directory added by
    // finding it.
    //

    if (InterfaceDirectory != NULL) {
        ObReleaseReference(InterfaceDirectory);
    }

    KeReleaseQueuedLock(IoInterfaceLock);
    return Status;
}

KERNEL_API
KSTATUS
IoRegisterForInterfaceNotifications (
    PUUID Interface,
    PINTERFACE_NOTIFICATION_CALLBACK CallbackRoutine,
    PDEVICE Device,
    PVOID Context,
    BOOL NotifyForExisting
    )

/*++

Routine Description:

    This routine registers the given handler to be notified when the given
    interface arrives or disappears. Callers are notified of both events.
    Callers will be notified for all interface arrivals and removals of the
    given interface.

Arguments:

    Interface - Supplies a pointer to the UUID identifying the interface.

    CallbackRoutine - Supplies a pointer to the callback routine to notify
        when an interface arrives or is removed.

    Device - Supplies an optional pointer to a device. If this device is
        non-NULL, then interface notifications will be restricted to the given
        device.

    Context - Supplies an optional pointer to a context that will be passed
        back to the caller during notifications.

    NotifyForExisting - Supplies TRUE if the caller would like an arrival
        notification for every pre-existing interface, or FALSE if the caller
        only wants to be notified about future arrivals. The caller will be
        notified about ALL removals no matter the state of this flag.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid interface or callback routine was
        supplied.

    STATUS_NO_MEMORY if memory could not be allocated to satisfy the request.

    STATUS_INSUFFICIENT_RESOURCES if general resources could not be allocated to
        satisfy the request.

    STATUS_DUPLICATE_ENTRY if the given routine is already registered for
        notification on the device.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEVICE_INTERFACE_INSTANCE CurrentInterface;
    PINTERFACE_DIRECTORY InterfaceDirectory;
    PINTERFACE_LISTENER InterfaceListener;
    CHAR Name[UUID_STRING_LENGTH + 1];
    KSTATUS Status;

    if ((Interface == NULL) || (CallbackRoutine == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    IopPrintUuidToString(Name, UUID_STRING_LENGTH + 1, Interface);
    KeAcquireQueuedLock(IoInterfaceLock);

    //
    // Look up the interface directory. Create it if it doesn't exist.
    //

    InterfaceDirectory = ObFindObject(Name,
                                      UUID_STRING_LENGTH + 1,
                                      IoInterfaceDirectory);

    if (InterfaceDirectory == NULL) {
        InterfaceDirectory = ObCreateObject(ObjectInterface,
                                            IoInterfaceDirectory,
                                            Name,
                                            sizeof(Name),
                                            sizeof(INTERFACE_DIRECTORY),
                                            NULL,
                                            0,
                                            DEVICE_INTERFACE_ALLOCATION_TAG);

        if (InterfaceDirectory == NULL) {
            Status = STATUS_NO_MEMORY;
            goto RegisterForInterfaceNotificationsEnd;
        }
    }

    ASSERT(InterfaceDirectory != NULL);

    //
    // Create the interface listener object.
    //

    InterfaceListener = ObCreateObject(ObjectInterfaceListener,
                                       InterfaceDirectory,
                                       NULL,
                                       0,
                                       sizeof(INTERFACE_LISTENER),
                                       NULL,
                                       0,
                                       DEVICE_INTERFACE_ALLOCATION_TAG);

    if (InterfaceListener == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RegisterForInterfaceNotificationsEnd;
    }

    InterfaceListener->CallbackRoutine = CallbackRoutine;
    InterfaceListener->Device = Device;
    InterfaceListener->Context = Context;

    //
    // If the caller would like to be notified about existing interfaces,
    // notify them now.
    //

    if (NotifyForExisting != FALSE) {
        CurrentEntry = InterfaceDirectory->Header.ChildListHead.Next;
        while (CurrentEntry != &(InterfaceDirectory->Header.ChildListHead)) {
            CurrentInterface =
                          (PDEVICE_INTERFACE_INSTANCE)LIST_VALUE(CurrentEntry,
                                                                 OBJECT_HEADER,
                                                                 SiblingEntry);

            CurrentEntry = CurrentEntry->Next;
            if (CurrentInterface->Header.Type != ObjectInterfaceInstance) {
                continue;
            }

            //
            // Notify the listener.
            //

            if ((Device == NULL) || (CurrentInterface->Device == Device)) {
                CallbackRoutine(InterfaceListener->Context,
                                CurrentInterface->Device,
                                CurrentInterface->InterfaceBuffer,
                                CurrentInterface->InterfaceBufferSize,
                                TRUE);
            }
        }
    }

    Status = STATUS_SUCCESS;

RegisterForInterfaceNotificationsEnd:
    KeReleaseQueuedLock(IoInterfaceLock);
    if (InterfaceDirectory != NULL) {
        ObReleaseReference(InterfaceDirectory);
    }

    return Status;
}

KERNEL_API
KSTATUS
IoUnregisterForInterfaceNotifications (
    PUUID Interface,
    PINTERFACE_NOTIFICATION_CALLBACK CallbackRoutine,
    PDEVICE Device,
    PVOID Context
    )

/*++

Routine Description:

    This routine de-registers the given handler from receiving device interface
    notifications. Once this routine returns, the given handler will not
    receive notifications for the given interface.

Arguments:

    Interface - Supplies a pointer to the UUID identifying the interface.

    CallbackRoutine - Supplies a pointer to the callback routine supplied on
        registration for notifications.

    Device - Supplies a pointer to the device supplied upon registration.

    Context - Supplies a pointer to the context supplied upon registration.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid interface or callback routine was
        supplied.

    STATUS_NOT_FOUND if no interface listener was found for the given callback
        routine on the given UUID.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PINTERFACE_LISTENER CurrentListener;
    PINTERFACE_DIRECTORY InterfaceDirectory;
    CHAR Name[UUID_STRING_LENGTH + 1];
    KSTATUS Status;

    if ((Interface == NULL) || (CallbackRoutine == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    IopPrintUuidToString(Name, UUID_STRING_LENGTH + 1, Interface);
    KeAcquireQueuedLock(IoInterfaceLock);

    //
    // Look up the interface directory. Create it if it doesn't exist.
    //

    InterfaceDirectory = ObFindObject(Name,
                                      UUID_STRING_LENGTH + 1,
                                      IoInterfaceDirectory);

    if (InterfaceDirectory == NULL) {
        Status = STATUS_NOT_FOUND;
        goto UnregisterForInterfaceNotificationsEnd;
    }

    //
    // Loop through the interface listeners searching for the one matching the
    // given callback routine.
    //

    Status = STATUS_NOT_FOUND;
    CurrentEntry = InterfaceDirectory->Header.ChildListHead.Next;
    while (CurrentEntry != &(InterfaceDirectory->Header.ChildListHead)) {
        CurrentListener = (PINTERFACE_LISTENER)LIST_VALUE(CurrentEntry,
                                                          OBJECT_HEADER,
                                                          SiblingEntry);

        CurrentEntry = CurrentEntry->Next;
        if (CurrentListener->Header.Type != ObjectInterfaceListener) {
            continue;
        }

        if ((CurrentListener->CallbackRoutine == CallbackRoutine) &&
            (CurrentListener->Device == Device) &&
            (CurrentListener->Context == Context)) {

            ObReleaseReference(CurrentListener);
            Status = STATUS_SUCCESS;
            break;
        }
    }

UnregisterForInterfaceNotificationsEnd:
    if (InterfaceDirectory != NULL) {
        ObReleaseReference(InterfaceDirectory);
    }

    KeReleaseQueuedLock(IoInterfaceLock);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
IopNotifyInterfaceListeners (
    PINTERFACE_DIRECTORY InterfaceDirectory,
    PDEVICE_INTERFACE_INSTANCE InterfaceInstance,
    PDEVICE Device,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine notifies all parties registered to receive device interface
    notifications. This routine MUST be called with the interface lock held.

Arguments:

    InterfaceDirectory - Supplies a pointer to the interface undergoing the
        transition.

    InterfaceInstance - Supplies a pointer to the interface instance undergoing
        transition.

    Device - Supplies a pointer to the device changing its interface.

    Arrival - Supplies TRUE if the interface is arriving, or FALSE if it is
        departing.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PINTERFACE_LISTENER CurrentListener;

    CurrentEntry = InterfaceDirectory->Header.ChildListHead.Next;
    while (CurrentEntry != &(InterfaceDirectory->Header.ChildListHead)) {
        CurrentListener = (PINTERFACE_LISTENER)LIST_VALUE(CurrentEntry,
                                                          OBJECT_HEADER,
                                                          SiblingEntry);

        CurrentEntry = CurrentEntry->Next;
        if (CurrentListener->Header.Type != ObjectInterfaceListener) {
            continue;
        }

        ASSERT(CurrentListener->CallbackRoutine != NULL);

        if ((CurrentListener->Device == NULL) ||
            (Device == CurrentListener->Device)) {

            CurrentListener->CallbackRoutine(
                                        CurrentListener->Context,
                                        Device,
                                        InterfaceInstance->InterfaceBuffer,
                                        InterfaceInstance->InterfaceBufferSize,
                                        Arrival);
        }
    }

    return;
}

VOID
IopPrintUuidToString (
    PSTR String,
    ULONG StringSize,
    PUUID Uuid
    )

/*++

Routine Description:

    This routine prints the given UUID out to a string.

Arguments:

    String - Supplies a pointer to the string where the UUID will be written.

    StringSize - Supplies the size of the supplied string buffer.

    Uuid - Supplies a pointer to the UUID to print.

Return Value:

    None.

--*/

{

    RtlPrintToString(String,
                     StringSize,
                     CharacterEncodingDefault,
                     "%08X-%08X-%08X-%08X",
                     Uuid->Data[0],
                     Uuid->Data[1],
                     Uuid->Data[2],
                     Uuid->Data[3]);

    return;
}

