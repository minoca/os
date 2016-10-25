/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    devinfo.c

Abstract:

    This module implements support for device information requests.

Author:

    Evan Green 9-Apr-2014

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

    This structure defines a device information entry.

Members:

    ListEntry - Stores pointers to the next and previous information entries in
        the global list.

    Uuid - Stores the universally unique identifier of the device information
        type.

    DeviceId - Stores the device ID of the device that registered the
        information.

    Device - Stores a pointer to the device that registered the information.

--*/

typedef struct _DEVICE_INFORMATION_ENTRY {
    LIST_ENTRY ListEntry;
    UUID Uuid;
    DEVICE_ID DeviceId;
    PDEVICE Device;
} DEVICE_INFORMATION_ENTRY, *PDEVICE_INFORMATION_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

UINTN IoNextDeviceId;

LIST_ENTRY IoDeviceInformationList;
PSHARED_EXCLUSIVE_LOCK IoDeviceInformationLock;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoLocateDeviceInformation (
    PUUID Uuid,
    PDEVICE Device,
    PDEVICE_ID DeviceId,
    PDEVICE_INFORMATION_RESULT Results,
    PULONG ResultCount
    )

/*++

Routine Description:

    This routine returns instances of devices enumerating information. Callers
    can get all devices enumerating the given information type, or all
    information types enumerated by a given device. This routine must be called
    at low level.

Arguments:

    Uuid - Supplies an optional pointer to the information identifier to
        filter on. If NULL, any information type will match.

    Device - Supplies an optional pointer to the device to match against. If
        NULL (and the device ID parameter is NULL), then any device will match.

    DeviceId - Supplies an optional pointer to the device ID to match against.
        If NULL (and the device ID parameter is NULL), then any device will
        match.

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

{

    ULONG BufferSize;
    PLIST_ENTRY CurrentEntry;
    PDEVICE_INFORMATION_ENTRY Entry;
    ULONG MatchCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    BufferSize = *ResultCount;

    ASSERT((BufferSize == 0) || (Results != NULL));

    MatchCount = 0;

    //
    // Loop through and look for elements that match.
    //

    KeAcquireSharedExclusiveLockShared(IoDeviceInformationLock);
    CurrentEntry = IoDeviceInformationList.Next;
    while (CurrentEntry != &IoDeviceInformationList) {
        Entry = LIST_VALUE(CurrentEntry, DEVICE_INFORMATION_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Skip anything that doesn't match.
        //

        if (Device != NULL) {
            if (Entry->Device != Device) {
                continue;
            }
        }

        if (DeviceId != NULL) {
            if (Entry->DeviceId != *DeviceId) {
                continue;
            }
        }

        if (Uuid != NULL) {
            if (RtlAreUuidsEqual(Uuid, &(Entry->Uuid)) == FALSE) {
                continue;
            }
        }

        //
        // This matches. Copy it into the results if there's space.
        //

        if (MatchCount < BufferSize) {
            RtlCopyMemory(&(Results->Uuid), &(Entry->Uuid), sizeof(UUID));
            Results->DeviceId = Entry->DeviceId;
            Results += 1;
        }

        MatchCount += 1;
    }

    KeReleaseSharedExclusiveLockShared(IoDeviceInformationLock);
    Status = STATUS_SUCCESS;
    if (MatchCount > BufferSize) {
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    *ResultCount = MatchCount;
    return Status;
}

KERNEL_API
KSTATUS
IoGetSetDeviceInformation (
    DEVICE_ID DeviceId,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

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

{

    PLIST_ENTRY CurrentEntry;
    PDEVICE Device;
    PDEVICE_INFORMATION_ENTRY Entry;
    SYSTEM_CONTROL_DEVICE_INFORMATION Request;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Loop through and find the information entry to convert from a device ID
    // back to a device.
    //

    Device = NULL;
    KeAcquireSharedExclusiveLockShared(IoDeviceInformationLock);
    CurrentEntry = IoDeviceInformationList.Next;
    while (CurrentEntry != &IoDeviceInformationList) {
        Entry = LIST_VALUE(CurrentEntry, DEVICE_INFORMATION_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Entry->DeviceId == DeviceId) {
            Device = Entry->Device;
            break;
        }
    }

    KeReleaseSharedExclusiveLockShared(IoDeviceInformationLock);
    if (Device == NULL) {
        *DataSize = 0;
        return STATUS_NO_INTERFACE;
    }

    RtlZeroMemory(&Request, sizeof(SYSTEM_CONTROL_DEVICE_INFORMATION));
    RtlCopyMemory(&(Request.Uuid), Uuid, sizeof(UUID));
    Request.Data = Data;
    Request.DataSize = *DataSize;
    Request.Set = Set;
    Status = IopSendSystemControlIrp(Device,
                                     IrpMinorSystemControlDeviceInformation,
                                     &Request);

    *DataSize = Request.DataSize;
    return Status;
}

KERNEL_API
KSTATUS
IoRegisterDeviceInformation (
    PDEVICE Device,
    PUUID Uuid,
    BOOL Register
    )

/*++

Routine Description:

    This routine registers or deregisters a device to respond to information
    requests of the given universally unique identifier. This routine must be
    called at low level.

Arguments:

    Device - Supplies a pointer to the device.

    Uuid - Supplies a pointer to the device information identifier.

    Register - Supplies a boolean indcating if the device is registering (TRUE)
        or de-registering (FALSE) for the information type.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEVICE_INFORMATION_ENTRY Entry;
    PDEVICE_INFORMATION_ENTRY ExistingEntry;
    PDEVICE_INFORMATION_ENTRY NewEntry;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    ExistingEntry = NULL;
    NewEntry = NULL;
    Status = STATUS_SUCCESS;

    //
    // Allocate and initialize the entry before acquiring the lock.
    //

    if (Register != FALSE) {
        NewEntry = MmAllocatePagedPool(sizeof(DEVICE_INFORMATION_ENTRY),
                                       DEVICE_INFORMATION_ALLOCATION_TAG);

        if (NewEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RegisterDeviceInformationEnd;
        }

        NewEntry->ListEntry.Next = NULL;
        RtlCopyMemory(&(NewEntry->Uuid), Uuid, sizeof(UUID));
        NewEntry->Device = Device;
        NewEntry->DeviceId = Device->DeviceId;
    }

    //
    // Look for an existing entry matching the device and UUID.
    //

    KeAcquireSharedExclusiveLockExclusive(IoDeviceInformationLock);
    CurrentEntry = IoDeviceInformationList.Next;
    while (CurrentEntry != &IoDeviceInformationList) {
        Entry = LIST_VALUE(CurrentEntry, DEVICE_INFORMATION_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Entry->Device == Device) &&
            (RtlAreUuidsEqual(&(Entry->Uuid), Uuid) != FALSE)) {

            ExistingEntry = Entry;
            break;
        }
    }

    //
    // If there was an existing entry, remove and free it if necessary.
    //

    if (ExistingEntry != NULL) {
        if (Register == FALSE) {
            LIST_REMOVE(&(ExistingEntry->ListEntry));
            ExistingEntry->ListEntry.Next = NULL;

        //
        // The entry exists and the caller is trying to register it again.
        // Clear out the pointer so it's not freed at the end of this routine.
        //

        } else {
            ExistingEntry = NULL;
        }

    //
    // There was no previous entry, add the new one if needed.
    //

    } else {
        if (Register != FALSE) {
            INSERT_AFTER(&(NewEntry->ListEntry), &IoDeviceInformationList);
            NewEntry = NULL;
        }
    }

RegisterDeviceInformationEnd:
    KeReleaseSharedExclusiveLockExclusive(IoDeviceInformationLock);
    if (ExistingEntry != NULL) {

        ASSERT(ExistingEntry->ListEntry.Next == NULL);

        MmFreePagedPool(ExistingEntry);
    }

    if (NewEntry != NULL) {

        ASSERT(NewEntry->ListEntry.Next == NULL);

        MmFreePagedPool(NewEntry);
    }

    return Status;
}

INTN
IoSysLocateDeviceInformation (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the user mode system call for locating device
    information registrations by UUID or device ID.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    UINTN AllocationSize;
    ULONG CopyCount;
    KSTATUS CopyStatus;
    PDEVICE_ID DeviceIdPointer;
    PSYSTEM_CALL_LOCATE_DEVICE_INFORMATION Request;
    PDEVICE_INFORMATION_RESULT Results;
    KSTATUS Status;
    PUUID UuidPointer;

    CopyCount = 0;
    Request = SystemCallParameter;

    //
    // Create a paged pool buffer to hold the results.
    //

    Results = NULL;
    if (Request->ResultCount != 0) {
        CopyCount = Request->ResultCount;
        AllocationSize = sizeof(DEVICE_INFORMATION_RESULT) * CopyCount;
        Results = MmAllocatePagedPool(
                                    AllocationSize,
                                    DEVICE_INFORMATION_REQUEST_ALLOCATION_TAG);

        if (Results == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SysLocateDeviceInformationEnd;
        }
    }

    DeviceIdPointer = NULL;
    UuidPointer = NULL;
    if (Request->ByDeviceId != FALSE) {
        DeviceIdPointer = &(Request->DeviceId);
    }

    if (Request->ByUuid != FALSE) {
        UuidPointer = &(Request->Uuid);
    }

    Status = IoLocateDeviceInformation(UuidPointer,
                                       NULL,
                                       DeviceIdPointer,
                                       Results,
                                       &(Request->ResultCount));

    //
    // Copy the results back into user mode.
    //

    if (Request->ResultCount < CopyCount) {
        CopyCount = Request->ResultCount;
    }

    if (CopyCount != 0) {
        CopyStatus = MmCopyToUserMode(
                                Request->Results,
                                Results,
                                CopyCount * sizeof(DEVICE_INFORMATION_RESULT));

        if ((KSUCCESS(Status)) && (!KSUCCESS(CopyStatus))) {
            Status = CopyStatus;
        }
    }

SysLocateDeviceInformationEnd:
    if (Results != NULL) {
        MmFreePagedPool(Results);
    }

    return Status;
}

INTN
IoSysGetSetDeviceInformation (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the user mode system call for getting and setting
    device information.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    PVOID Buffer;
    UINTN CopySize;
    KSTATUS CopyStatus;
    PSYSTEM_CALL_GET_SET_DEVICE_INFORMATION Request;
    KSTATUS Status;

    Buffer = NULL;
    Request = SystemCallParameter;

    //
    // Create a paged pool buffer to hold the data.
    //

    CopySize = 0;
    if (Request->DataSize != 0) {
        Buffer = MmAllocatePagedPool(
                                    Request->DataSize,
                                    DEVICE_INFORMATION_REQUEST_ALLOCATION_TAG);

        if (Buffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SysGetSetDeviceInformationEnd;
        }

        CopySize = Request->DataSize;

        //
        // Copy the data into the kernel mode buffer.
        //

        Status = MmCopyFromUserMode(Buffer,
                                    Request->Data,
                                    Request->DataSize);

        if (!KSUCCESS(Status)) {
            goto SysGetSetDeviceInformationEnd;
        }
    }

    Status = IoGetSetDeviceInformation(Request->DeviceId,
                                       &(Request->Uuid),
                                       Buffer,
                                       &(Request->DataSize),
                                       Request->Set);

    //
    // Copy the data back into user mode, even on set operations.
    //

    if (CopySize > Request->DataSize) {
        CopySize = Request->DataSize;
    }

    if (CopySize != 0) {
        CopyStatus = MmCopyToUserMode(Request->Data, Buffer, CopySize);
        if ((KSUCCESS(Status)) && (!KSUCCESS(CopyStatus))) {
            Status = CopyStatus;
        }
    }

SysGetSetDeviceInformationEnd:
    if (Buffer != NULL) {
        MmFreePagedPool(Buffer);
    }

    return Status;
}

DEVICE_ID
IopGetNextDeviceId (
    VOID
    )

/*++

Routine Description:

    This routine allocates and returns a device ID.

Arguments:

    None.

Return Value:

    Returns a unique device ID number.

--*/

{

    DEVICE_ID DeviceId;

    DeviceId = RtlAtomicAdd(&IoNextDeviceId, 1);
    return DeviceId;
}

KSTATUS
IopInitializeDeviceInformationSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes device information support.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    IoNextDeviceId = OBJECT_MANAGER_DEVICE_ID + 1;
    INITIALIZE_LIST_HEAD(&IoDeviceInformationList);
    IoDeviceInformationLock = KeCreateSharedExclusiveLock();
    if (IoDeviceInformationLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceInformationSupportEnd;
    }

    Status = STATUS_SUCCESS;

InitializeDeviceInformationSupportEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

