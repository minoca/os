/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    drvsup.c

Abstract:

    This module implements UEFI core driver support routines.

Author:

    Evan Green 5-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/uefi/protocol/drvbind.h>
#include <minoca/uefi/protocol/drvplato.h>
#include <minoca/uefi/protocol/drvfamov.h>
#include <minoca/uefi/protocol/drvbusov.h>

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
EfipCoreConnectSingleController (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE *ContextDriverImageHandles,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

VOID
EfipCoreAddSortedDriverBindingProtocol (
    EFI_HANDLE DriverBindingHandle,
    UINTN *NumberOfSortedDriverBindingProtocols,
    EFI_DRIVER_BINDING_PROTOCOL **SortedDriverBindingProtocols,
    UINTN DriverBindingHandleCount,
    EFI_HANDLE *DriverBindingHandleBuffer,
    BOOLEAN IsImageHandle
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_GUID EfiDriverBindingProtocolGuid = EFI_DRIVER_BINDING_PROTOCOL_GUID;
EFI_GUID EfiPlatformDriverOverrideProtocolGuid =
                                    EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL_GUID;

EFI_GUID EfiDriverFamilyOverrideProtocolGuid =
                                      EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL_GUID;

EFI_GUID EfiBusSpecificDriverOverrideProtocolGuid =
                                EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreConnectController (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE *DriverImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath,
    BOOLEAN Recursive
    )

/*++

Routine Description:

    This routine connects one or more drivers to a controller.

Arguments:

    ControllerHandle - Supplies the handle of the controller which driver(s)
        are connecting to.

    DriverImageHandle - Supplies a pointer to an ordered list of handles that
        support the EFI_DRIVER_BINDING_PROTOCOL.

    RemainingDevicePath - Supplies an optional pointer to the device path that
        specifies a child of the controller specified by the controller handle.

    Recursive - Supplies a boolean indicating if this routine should be called
        recursively until the entire tree of controllers below the specified
        controller has been connected. If FALSE, then the tree of controllers
        is only expanded one level.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the controller handle is NULL.

    EFI_NOT_FOUND if either there are no EFI_DRIVER_BINDING_PROTOCOL instances
    present in the system, or no drivers were connected to the controller
    handle.

    EFI_SECURITY_VIOLATION if the user has no permission to start UEFI device
    drivers on the device associated with the controller handle or specified
    by the remaining device path.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *AlignedRemainingDevicePath;
    EFI_HANDLE *ChildHandleBuffer;
    UINTN ChildHandleCount;
    PLIST_ENTRY CurrentEntry;
    PEFI_HANDLE_DATA Handle;
    UINTN Index;
    PEFI_OPEN_PROTOCOL_DATA OpenData;
    PLIST_ENTRY OpenEntry;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    EFI_STATUS ReturnStatus;
    EFI_STATUS Status;

    Status = EfipCoreValidateHandle(ControllerHandle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Handle = ControllerHandle;

    //
    // Make a copy of the device path to ensure it is aligned.
    //

    AlignedRemainingDevicePath = NULL;
    if (RemainingDevicePath != NULL) {
        AlignedRemainingDevicePath = EfiCoreDuplicateDevicePath(
                                                          RemainingDevicePath);

        if (AlignedRemainingDevicePath == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }
    }

    //
    // Connect all drivers to the controller handle. If the connection routine
    // returns EFI_NOT_READY, then the number of driver binding protocols in
    // the handle database has increased during the call so the connect
    // operation must be restarted.
    //

    do {
        ReturnStatus = EfipCoreConnectSingleController(
                                                   ControllerHandle,
                                                   DriverImageHandle,
                                                   AlignedRemainingDevicePath);

    } while (ReturnStatus == EFI_NOT_READY);

    if (AlignedRemainingDevicePath != NULL) {
        EfiCoreFreePool(AlignedRemainingDevicePath);
    }

    //
    // If recursive, then connect all drivers to all of the controller handle's
    // children.
    //

    if (Recursive != FALSE) {
        EfiCoreAcquireLock(&EfiProtocolDatabaseLock);

        //
        // Make sure the driver binding handle is valid.
        //

        Status = EfipCoreValidateHandle(ControllerHandle);
        if (EFI_ERROR(Status)) {
            EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
            return Status;
        }

        //
        // Count the controller handle's children.
        //

        ChildHandleCount = 0;
        CurrentEntry = Handle->ProtocolList.Next;
        while (CurrentEntry != &(Handle->ProtocolList)) {
            ProtocolInterface = LIST_VALUE(CurrentEntry,
                                           EFI_PROTOCOL_INTERFACE,
                                           ListEntry);

            ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

            CurrentEntry = CurrentEntry->Next;
            OpenEntry = ProtocolInterface->OpenList.Next;
            while (OpenEntry != &(ProtocolInterface->OpenList)) {
                OpenData = LIST_VALUE(OpenEntry,
                                      EFI_OPEN_PROTOCOL_DATA,
                                      ListEntry);

                ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

                OpenEntry = OpenEntry->Next;
                if ((OpenData->Attributes &
                     EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) != 0) {

                    ChildHandleCount += 1;
                }
            }
        }

        //
        // Allocate an array for the controller handle's children.
        //

        ChildHandleBuffer = EfiCoreAllocateBootPool(
                                        ChildHandleCount * sizeof(EFI_HANDLE));

        if (ChildHandleBuffer == NULL) {
            EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
            return EFI_OUT_OF_RESOURCES;
        }

        //
        // Fill in the handle buffer with the controller handle's children.
        //

        ChildHandleCount = 0;
        CurrentEntry = Handle->ProtocolList.Next;
        while (CurrentEntry != &(Handle->ProtocolList)) {
            ProtocolInterface = LIST_VALUE(CurrentEntry,
                                           EFI_PROTOCOL_INTERFACE,
                                           ListEntry);

            ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

            CurrentEntry = CurrentEntry->Next;
            OpenEntry = ProtocolInterface->OpenList.Next;
            while (OpenEntry != &(ProtocolInterface->OpenList)) {
                OpenData = LIST_VALUE(OpenEntry,
                                      EFI_OPEN_PROTOCOL_DATA,
                                      ListEntry);

                ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

                OpenEntry = OpenEntry->Next;
                if ((OpenData->Attributes &
                     EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) != 0) {

                    ChildHandleBuffer[ChildHandleCount] =
                                                    OpenData->ControllerHandle;

                    ChildHandleCount += 1;
                }
            }
        }

        EfiCoreReleaseLock(&EfiProtocolDatabaseLock);

        //
        // Recursively connect each child.
        //

        for (Index = 0; Index < ChildHandleCount; Index += 1) {
            EfiCoreConnectController(ChildHandleBuffer[Index],
                                     NULL,
                                     NULL,
                                     TRUE);
        }

        EfiCoreFreePool(ChildHandleBuffer);
    }

    return ReturnStatus;
}

EFIAPI
EFI_STATUS
EfiCoreDisconnectController (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE DriverImageHandle,
    EFI_HANDLE ChildHandle
    )

/*++

Routine Description:

    This routine disconnects one or more drivers to a controller.

Arguments:

    ControllerHandle - Supplies the handle of the controller which driver(s)
        are disconnecting from.

    DriverImageHandle - Supplies an optional pointer to the driver to
        disconnect from the controller. If NULL, all drivers are disconnected.

    ChildHandle - Supplies an optional pointer to the handle of the child to
        destroy.

Return Value:

    EFI_SUCCESS if one or more drivers were disconnected, no drivers are
    managing the handle, or a driver image handle was supplied and it is not
    controlling the given handle.

    EFI_INVALID_PARAMETER if the controller handle or driver handle is not a
    valid EFI handle, or the driver image handle doesn't support the
    EFI_DRIVER_BINDING_PROTOCOL.

    EFI_OUT_OF_RESOURCES if there are not enough resources are available to
    disconnect the controller(s).

    EFI_DEVICE_ERROR if the controller could not be disconnected because of a
    device error.

--*/

{

    EFI_HANDLE *ChildBuffer;
    UINTN ChildBufferCount;
    BOOLEAN ChildHandleValid;
    UINTN ChildrenToStop;
    PLIST_ENTRY CurrentEntry;
    EFI_DRIVER_BINDING_PROTOCOL *DriverBinding;
    EFI_HANDLE *DriverImageHandleBuffer;
    UINTN DriverImageHandleCount;
    BOOLEAN DriverImageHandleValid;
    BOOLEAN Duplicate;
    PEFI_HANDLE_DATA Handle;
    UINTN HandleIndex;
    UINTN Index;
    PEFI_OPEN_PROTOCOL_DATA OpenData;
    PLIST_ENTRY OpenEntry;
    PEFI_PROTOCOL_INTERFACE ProtocolInterface;
    EFI_STATUS Status;
    UINTN StopCount;

    Status = EfipCoreValidateHandle(ControllerHandle);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Make sure the child handle is valid if supplied.
    //

    if (ChildHandle != NULL) {
        Status = EfipCoreValidateHandle(ChildHandle);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    Handle = ControllerHandle;

    //
    // Get a list of drivers managing the controller handle.
    //

    DriverImageHandleBuffer = NULL;
    DriverImageHandleCount = 0;
    if (DriverImageHandle == NULL) {
        DriverImageHandleCount = 0;
        EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
        CurrentEntry = Handle->ProtocolList.Next;
        while (CurrentEntry != &(Handle->ProtocolList)) {
            ProtocolInterface = LIST_VALUE(CurrentEntry,
                                           EFI_PROTOCOL_INTERFACE,
                                           ListEntry);

            ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

            OpenEntry = ProtocolInterface->OpenList.Next;
            while (OpenEntry != &(ProtocolInterface->OpenList)) {
                OpenData = LIST_VALUE(OpenEntry,
                                      EFI_OPEN_PROTOCOL_DATA,
                                      ListEntry);

                ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

                OpenEntry = OpenEntry->Next;
                if ((OpenData->Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) != 0) {
                    DriverImageHandleCount += 1;
                }
            }

            CurrentEntry = CurrentEntry->Next;
        }

        EfiCoreReleaseLock(&EfiProtocolDatabaseLock);

        //
        // If there are no drivers managing this controller, then there's no
        // work to do.
        //

        if (DriverImageHandleCount == 0) {
            Status = EFI_SUCCESS;
            goto CoreDisconnectControllerEnd;
        }

        DriverImageHandleBuffer = EfiCoreAllocateBootPool(
                                  DriverImageHandleCount * sizeof(EFI_HANDLE));

        if (DriverImageHandleBuffer == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto CoreDisconnectControllerEnd;
        }

        DriverImageHandleCount = 0;
        EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
        CurrentEntry = Handle->ProtocolList.Next;
        while (CurrentEntry != &(Handle->ProtocolList)) {
            ProtocolInterface = LIST_VALUE(CurrentEntry,
                                           EFI_PROTOCOL_INTERFACE,
                                           ListEntry);

            ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

            OpenEntry = ProtocolInterface->OpenList.Next;
            while (OpenEntry != &(ProtocolInterface->OpenList)) {
                OpenData = LIST_VALUE(OpenEntry,
                                      EFI_OPEN_PROTOCOL_DATA,
                                      ListEntry);

                OpenEntry = OpenEntry->Next;
                if ((OpenData->Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) != 0) {
                    Duplicate = FALSE;
                    for (Index = 0;
                         Index < DriverImageHandleCount;
                         Index += 1) {

                        if (DriverImageHandleBuffer[Index] ==
                            OpenData->AgentHandle) {

                            Duplicate = TRUE;
                            break;
                        }
                    }

                    if (Duplicate == FALSE) {
                        DriverImageHandleBuffer[DriverImageHandleCount] =
                                                         OpenData->AgentHandle;

                        DriverImageHandleCount += 1;
                    }
                }
            }

            CurrentEntry = CurrentEntry->Next;
        }

        EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
    }

    //
    // Loop through each driver that has this controller open.
    //

    StopCount = 0;
    for (HandleIndex = 0;
         HandleIndex < DriverImageHandleCount;
         HandleIndex += 1) {

        if (DriverImageHandleBuffer != NULL) {
            DriverImageHandle = DriverImageHandleBuffer[HandleIndex];
        }

        //
        // Get the driver binding protocol of the driver managing this
        // controller.
        //

        Status = EfiCoreHandleProtocol(DriverImageHandle,
                                       &EfiDriverBindingProtocolGuid,
                                       (VOID **)&DriverBinding);

        if ((EFI_ERROR(Status)) || (DriverBinding == NULL)) {
            Status = EFI_INVALID_PARAMETER;
            goto CoreDisconnectControllerEnd;
        }

        //
        // Look at each protocol interface for a match.
        //

        DriverImageHandleValid = FALSE;
        ChildBufferCount = 0;
        EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
        CurrentEntry = Handle->ProtocolList.Next;
        while (CurrentEntry != &(Handle->ProtocolList)) {
            ProtocolInterface = LIST_VALUE(CurrentEntry,
                                           EFI_PROTOCOL_INTERFACE,
                                           ListEntry);

            ASSERT(ProtocolInterface->Magic == EFI_PROTOCOL_INTERFACE_MAGIC);

            OpenEntry = ProtocolInterface->OpenList.Next;
            while (OpenEntry != &(ProtocolInterface->OpenList)) {
                OpenData = LIST_VALUE(OpenEntry,
                                      EFI_OPEN_PROTOCOL_DATA,
                                      ListEntry);

                OpenEntry = OpenEntry->Next;

                ASSERT(OpenData->Magic == EFI_OPEN_PROTOCOL_MAGIC);

                if (OpenData->AgentHandle == DriverImageHandle) {
                    if ((OpenData->Attributes &
                         EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) != 0) {

                        ChildBufferCount += 1;
                    }

                    if ((OpenData->Attributes &
                         EFI_OPEN_PROTOCOL_BY_DRIVER) != 0) {

                        //
                        // The driver really does have the controller open.
                        //

                        DriverImageHandleValid = TRUE;
                    }
                }
            }

            CurrentEntry = CurrentEntry->Next;
        }

        EfiCoreReleaseLock(&EfiProtocolDatabaseLock);

        //
        // If the driver really has the controller open, stop it.
        //

        if (DriverImageHandleValid != FALSE) {
            ChildHandleValid = FALSE;
            ChildBuffer = NULL;
            if (ChildBufferCount != 0) {
                ChildBuffer = EfiCoreAllocateBootPool(
                                        ChildBufferCount * sizeof(EFI_HANDLE));

                if (ChildBuffer == NULL) {
                    Status = EFI_OUT_OF_RESOURCES;
                    goto CoreDisconnectControllerEnd;
                }

                ChildBufferCount = 0;
                EfiCoreAcquireLock(&EfiProtocolDatabaseLock);
                CurrentEntry = Handle->ProtocolList.Next;
                while (CurrentEntry != &(Handle->ProtocolList)) {
                    ProtocolInterface = LIST_VALUE(CurrentEntry,
                                                   EFI_PROTOCOL_INTERFACE,
                                                   ListEntry);

                    ASSERT(ProtocolInterface->Magic ==
                           EFI_PROTOCOL_INTERFACE_MAGIC);

                    OpenEntry = ProtocolInterface->OpenList.Next;
                    while (OpenEntry != &(ProtocolInterface->OpenList)) {
                        OpenData = LIST_VALUE(OpenEntry,
                                              EFI_OPEN_PROTOCOL_DATA,
                                              ListEntry);

                        OpenEntry = OpenEntry->Next;
                        if ((OpenData->AgentHandle == DriverImageHandle) &&
                            ((OpenData->Attributes &
                              EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) != 0)) {

                            Duplicate = FALSE;
                            for (Index = 0;
                                 Index < ChildBufferCount;
                                 Index += 1) {

                                if (ChildBuffer[Index] ==
                                    OpenData->ControllerHandle) {

                                    Duplicate = TRUE;
                                    break;
                                }
                            }

                            if (Duplicate == FALSE) {
                                ChildBuffer[ChildBufferCount] =
                                                    OpenData->ControllerHandle;

                                if (ChildHandle ==
                                    ChildBuffer[ChildBufferCount]) {

                                    ChildHandleValid = TRUE;
                                }

                                ChildBufferCount += 1;
                            }
                        }
                    }

                    CurrentEntry = CurrentEntry->Next;
                }

                EfiCoreReleaseLock(&EfiProtocolDatabaseLock);
            }

            if ((ChildHandle == NULL) || (ChildHandleValid != FALSE)) {
                ChildrenToStop = 0;
                Status = EFI_SUCCESS;
                if (ChildBufferCount > 0) {
                    if (ChildHandle != NULL) {
                        ChildrenToStop = 1;
                        Status = DriverBinding->Stop(DriverBinding,
                                                     ControllerHandle,
                                                     ChildrenToStop,
                                                     &ChildHandle);

                    } else {
                        ChildrenToStop = ChildBufferCount;
                        Status = DriverBinding->Stop(DriverBinding,
                                                     ControllerHandle,
                                                     ChildrenToStop,
                                                     ChildBuffer);
                    }
                }

                if ((!EFI_ERROR(Status)) &&
                    ((ChildHandle == NULL) ||
                     (ChildBufferCount == ChildrenToStop))) {

                    Status = DriverBinding->Stop(DriverBinding,
                                                 ControllerHandle,
                                                 0,
                                                 NULL);
                }

                if (!EFI_ERROR(Status)) {
                    StopCount += 1;
                }
            }

            if (ChildBuffer != NULL) {
                EfiCoreFreePool(ChildBuffer);
            }
        }
    }

    if (StopCount > 0) {
        Status = EFI_SUCCESS;

    } else {
        Status = EFI_NOT_FOUND;
    }

CoreDisconnectControllerEnd:
    if (DriverImageHandleBuffer != NULL) {
        EfiCoreFreePool(DriverImageHandleBuffer);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipCoreConnectSingleController (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE *ContextDriverImageHandles,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    )

/*++

Routine Description:

    This routine connects one controller to a driver.

Arguments:

    ControllerHandle - Supplies the handle of the controller to connect.

    ContextDriverImageHandles - Supplies a pointer to an ordered list of driver
        image handles.

    RemainingDevicePath - Supplies a pointer to the device path that specifies
        a child of the controller.

Return Value:

    EFI status code.

--*/

{

    EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL *BusSpecificDriverOverride;
    EFI_DRIVER_BINDING_PROTOCOL *DriverBinding;
    EFI_HANDLE *DriverBindingHandleBuffer;
    UINTN DriverBindingHandleCount;
    EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL *DriverFamilyOverride;
    UINT32 DriverFamilyOverrideVersion;
    BOOLEAN DriverFound;
    EFI_HANDLE DriverImageHandle;
    UINTN HighestIndex;
    UINT32 HighestVersion;
    UINTN Index;
    EFI_HANDLE *NewDriverBindingHandleBuffer;
    UINTN NewDriverBindingHandleCount;
    UINTN NumberOfSortedDriverBindingProtocols;
    BOOLEAN OneStarted;
    EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL *PlatformDriverOverride;
    EFI_DRIVER_BINDING_PROTOCOL **SortedDriverBindingProtocols;
    UINTN SortIndex;
    EFI_STATUS Status;

    DriverBindingHandleCount = 0;
    DriverBindingHandleBuffer = NULL;
    NumberOfSortedDriverBindingProtocols = 0;
    SortedDriverBindingProtocols = NULL;

    //
    // Get a list of all driver binding protocol instances.
    //

    Status = EfiCoreLocateHandleBuffer(ByProtocol,
                                       &EfiDriverBindingProtocolGuid,
                                       NULL,
                                       &DriverBindingHandleCount,
                                       &DriverBindingHandleBuffer);

    if ((EFI_ERROR(Status)) || (DriverBindingHandleCount == 0)) {
        return EFI_NOT_FOUND;
    }

    //
    // Allocate a duplicate array for the sorted driver binding protocol
    // instances.
    //

    SortedDriverBindingProtocols = EfiCoreAllocateBootPool(
                                DriverBindingHandleCount * sizeof(EFI_HANDLE));

    if (SortedDriverBindingProtocols == NULL) {
        EfiCoreFreePool(DriverBindingHandleBuffer);
        return EFI_OUT_OF_RESOURCES;
    }

    //
    // Add Driver Binding Protocols from Context Driver Image Handles first.
    //

    if (ContextDriverImageHandles != NULL) {
        Index = 0;
        while (ContextDriverImageHandles[Index] != NULL) {
            EfipCoreAddSortedDriverBindingProtocol(
                                         ContextDriverImageHandles[Index],
                                         &NumberOfSortedDriverBindingProtocols,
                                         SortedDriverBindingProtocols,
                                         DriverBindingHandleCount,
                                         DriverBindingHandleBuffer,
                                         FALSE);

            Index += 1;
        }
    }

    //
    // Add the Platform Driver Override Protocol drivers for the controller
    // handle next.
    //

    Status = EfiCoreLocateProtocol(&EfiPlatformDriverOverrideProtocolGuid,
                                   NULL,
                                   (VOID **)&PlatformDriverOverride);

    if ((!EFI_ERROR(Status)) && (PlatformDriverOverride != NULL)) {
        DriverImageHandle = NULL;
        do {
            Status = PlatformDriverOverride->GetDriver(PlatformDriverOverride,
                                                       ControllerHandle,
                                                       &DriverImageHandle);

            if (!EFI_ERROR(Status)) {
                EfipCoreAddSortedDriverBindingProtocol(
                                         DriverImageHandle,
                                         &NumberOfSortedDriverBindingProtocols,
                                         SortedDriverBindingProtocols,
                                         DriverBindingHandleCount,
                                         DriverBindingHandleBuffer,
                                         TRUE);

            }

        } while (!EFI_ERROR(Status));
    }

    //
    // Add the Driver Family Override Protocol drivers fot he controller handle.
    //

    while (TRUE) {
        HighestIndex = DriverBindingHandleCount;
        HighestVersion = 0;
        for (Index = 0; Index < DriverBindingHandleCount; Index += 1) {
            Status = EfiCoreHandleProtocol(DriverBindingHandleBuffer[Index],
                                           &EfiDriverFamilyOverrideProtocolGuid,
                                           (VOID **)&DriverFamilyOverride);

            if ((!EFI_ERROR(Status)) && (DriverFamilyOverride != NULL)) {
                DriverFamilyOverrideVersion = DriverFamilyOverride->GetVersion(
                                                         DriverFamilyOverride);

                if ((HighestIndex == DriverBindingHandleCount) ||
                    (DriverFamilyOverrideVersion > HighestVersion)) {

                    HighestVersion = DriverFamilyOverrideVersion;
                    HighestIndex = Index;
                }
            }
        }

        if (HighestIndex == DriverBindingHandleCount) {
            break;
        }

        EfipCoreAddSortedDriverBindingProtocol(
                                       DriverBindingHandleBuffer[HighestIndex],
                                       &NumberOfSortedDriverBindingProtocols,
                                       SortedDriverBindingProtocols,
                                       DriverBindingHandleCount,
                                       DriverBindingHandleBuffer,
                                       FALSE);
    }

    //
    // Get the Bus Specific Driver Override Protocol instance on the controller
    // handle.
    //

    Status = EfiCoreHandleProtocol(ControllerHandle,
                                   &EfiBusSpecificDriverOverrideProtocolGuid,
                                   (VOID **)&BusSpecificDriverOverride);

    if ((!EFI_ERROR(Status)) && (BusSpecificDriverOverride != NULL)) {
        DriverImageHandle = NULL;
        do {
            Status = BusSpecificDriverOverride->GetDriver(
                                                     BusSpecificDriverOverride,
                                                     &DriverImageHandle);

            if (!EFI_ERROR(Status)) {
                EfipCoreAddSortedDriverBindingProtocol(
                                         DriverImageHandle,
                                         &NumberOfSortedDriverBindingProtocols,
                                         SortedDriverBindingProtocols,
                                         DriverBindingHandleCount,
                                         DriverBindingHandleBuffer,
                                         TRUE);

            }

        } while (!EFI_ERROR(Status));
    }

    //
    // Finally, add all remaining Driver Binding Protocols.
    //

    SortIndex = NumberOfSortedDriverBindingProtocols;
    for (Index = 0; Index < DriverBindingHandleCount; Index += 1) {
        EfipCoreAddSortedDriverBindingProtocol(
                                         DriverBindingHandleBuffer[Index],
                                         &NumberOfSortedDriverBindingProtocols,
                                         SortedDriverBindingProtocols,
                                         DriverBindingHandleCount,
                                         DriverBindingHandleBuffer,
                                         FALSE);
    }

    EfiCoreFreePool(DriverBindingHandleBuffer);

    //
    // If the number of Driver Binding Protocols has increased since this
    // function started, return "not ready" so it will be restarted.
    //

    Status = EfiCoreLocateHandleBuffer(ByProtocol,
                                       &EfiDriverBindingProtocolGuid,
                                       NULL,
                                       &NewDriverBindingHandleCount,
                                       &NewDriverBindingHandleBuffer);

    EfiCoreFreePool(NewDriverBindingHandleBuffer);
    if (NewDriverBindingHandleCount > DriverBindingHandleCount) {
        EfiCoreFreePool(SortedDriverBindingProtocols);
        return EFI_NOT_READY;
    }

    //
    // Sort the remaining driver binding protocols based on their version field
    // from highest to lowest.
    //

    while (SortIndex < NumberOfSortedDriverBindingProtocols) {
        HighestVersion = SortedDriverBindingProtocols[SortIndex]->Version;
        HighestIndex = SortIndex;
        for (Index = SortIndex + 1;
             Index < NumberOfSortedDriverBindingProtocols;
             Index += 1) {

            if (SortedDriverBindingProtocols[Index]->Version > HighestVersion) {
                HighestVersion = SortedDriverBindingProtocols[Index]->Version;
                HighestIndex   = Index;
            }
        }

        if (SortIndex != HighestIndex) {
            DriverBinding = SortedDriverBindingProtocols[SortIndex];
            SortedDriverBindingProtocols[SortIndex] =
                                    SortedDriverBindingProtocols[HighestIndex];

            SortedDriverBindingProtocols[HighestIndex] = DriverBinding;
        }

        SortIndex += 1;
    }

    //
    // Loop until no more drivers can be started on the controller handle.
    //

    OneStarted = FALSE;
    do {

        //
        // Loop through the sorted driver binding protocol instance in order,
        // and see if any of the driver binding protocols support the
        // controller.
        //

        DriverBinding = NULL;
        DriverFound = FALSE;
        for (Index = 0;
             Index < NumberOfSortedDriverBindingProtocols;
             Index += 1) {

            if (DriverFound != FALSE) {
                break;
            }

            if (SortedDriverBindingProtocols[Index] != NULL) {
                DriverBinding = SortedDriverBindingProtocols[Index];
                Status = DriverBinding->Supported(DriverBinding,
                                                  ControllerHandle,
                                                  RemainingDevicePath);

                if (!EFI_ERROR(Status)) {
                    SortedDriverBindingProtocols[Index] = NULL;
                    DriverFound = TRUE;

                    //
                    // A driver was found that claims to support the controller,
                    // so start the driver on the controller.
                    //

                    Status = DriverBinding->Start(DriverBinding,
                                                  ControllerHandle,
                                                  RemainingDevicePath);

                    if (!EFI_ERROR(Status)) {
                        OneStarted = TRUE;
                    }
                }
            }
        }

    } while (DriverFound != FALSE);

    EfiCoreFreePool(SortedDriverBindingProtocols);

    //
    // If at least one driver started, declare success.
    //

    if (OneStarted != FALSE) {
        return EFI_SUCCESS;
    }

    //
    // If no drivers started and the remaining device path is an end device
    // node, return success.
    //

    if (RemainingDevicePath != NULL) {
        if (EfiCoreIsDevicePathEnd(RemainingDevicePath) != FALSE) {
            return EFI_SUCCESS;
        }
    }

    //
    // No drivers were started on the controller.
    //

    return EFI_NOT_FOUND;
}

VOID
EfipCoreAddSortedDriverBindingProtocol (
    EFI_HANDLE DriverBindingHandle,
    UINTN *NumberOfSortedDriverBindingProtocols,
    EFI_DRIVER_BINDING_PROTOCOL **SortedDriverBindingProtocols,
    UINTN DriverBindingHandleCount,
    EFI_HANDLE *DriverBindingHandleBuffer,
    BOOLEAN IsImageHandle
    )

/*++

Routine Description:

    This routine adds a driver binding protocol to a sorted driver binding
    protocol list.

Arguments:

    DriverBindingHandle - Supplies the handle of the driver binding protcol.

    NumberOfSortedDriverBindingProtocols - Supplies a pointer containing the
        number os sorted driver binding protocols. This will be incremented by
        this function.

    SortedDriverBindingProtocols - Supplies the sorted protocol list.

    DriverBindingHandleCount - Supplies the number of handles in the driver
        binding handle buffer.

    DriverBindingHandleBuffer - Supplies the buffer of driver binding handles
        to be modified.

    IsImageHandle - Supplies a boolean indicating if the driver binding handle
        is an image handle.

Return Value:

    None.

--*/

{

    EFI_DRIVER_BINDING_PROTOCOL *DriverBinding;
    UINTN Index;
    EFI_STATUS Status;

    Status = EfipCoreValidateHandle(DriverBindingHandle);
    if (EFI_ERROR(Status)) {

        ASSERT(FALSE);

        return;
    }

    //
    // If the handle is an image handle, find all the driver binding handles
    // associated with that image handle and add them to the sorted list.
    //

    if (IsImageHandle != FALSE) {
        for (Index = 0; Index < DriverBindingHandleCount; Index += 1) {
            Status = EfiCoreHandleProtocol(DriverBindingHandleBuffer[Index],
                                           &EfiDriverBindingProtocolGuid,
                                           (VOID **)&DriverBinding);

            if ((EFI_ERROR(Status)) || (DriverBinding == NULL)) {
                continue;
            }

            //
            // If the image handle associated with the driver binding matches
            // the driver binding handle, then add the driver binding to the
            // list.
            //

            if (DriverBinding->ImageHandle == DriverBindingHandle) {
                EfipCoreAddSortedDriverBindingProtocol(
                                          DriverBindingHandleBuffer[Index],
                                          NumberOfSortedDriverBindingProtocols,
                                          SortedDriverBindingProtocols,
                                          DriverBindingHandleCount,
                                          DriverBindingHandleBuffer,
                                          FALSE);
            }
        }

        return;
    }

    Status = EfiCoreHandleProtocol(DriverBindingHandle,
                                   &EfiDriverBindingProtocolGuid,
                                   (VOID **)&DriverBinding);

    if ((EFI_ERROR(Status)) || (DriverBinding == NULL)) {
        return;
    }

    //
    // See if the driver binding is already on the list.
    //

    for (Index = 0;
         Index < *NumberOfSortedDriverBindingProtocols;
         Index += 1) {

        if (Index >= DriverBindingHandleCount) {
            break;
        }

        if (DriverBinding == SortedDriverBindingProtocols[Index]) {
            return;
        }
    }

    //
    // Add the driver binding to the end of the list.
    //

    if (*NumberOfSortedDriverBindingProtocols < DriverBindingHandleCount) {
        SortedDriverBindingProtocols[*NumberOfSortedDriverBindingProtocols] =
                                                                 DriverBinding;
    }

    *NumberOfSortedDriverBindingProtocols += 1;

    //
    // Mark the corresponding handle in the driver binding handle buffer as
    // used.
    //

    for (Index = 0; Index < DriverBindingHandleCount; Index += 1) {
        if (DriverBindingHandleBuffer[Index] == DriverBindingHandle) {
            DriverBindingHandleBuffer[Index] = NULL;
        }
    }

    return;
}

