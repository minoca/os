/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    device.c

Abstract:

    This module implements functions that interact with devices in the system.

Author:

    Evan Green 16-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"
#include "pmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define values for the device ID suffix that is used to make device IDs
// unique.
//

#define DEVICE_ID_SUFFIX_FORMAT "#%d"
#define DEVICE_ID_SUFFIX_FORMAT_LENGTH 4
#define DEVICE_ID_SUFFIX_ALTERNATE_FORMAT "%d"
#define DEVICE_ID_SUFFIX_ALTERNATE_FORMAT_LENGTH 3
#define DEVICE_ID_SUFFIX_START_CHARACTER '#'
#define DEVICE_ID_SUFFIX_LENGTH_MAX 5

//
// Define the maximum number of sibling devices that can conflict with the same
// name.
//

#define MAX_CONFLICTING_DEVICES 10000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopDeviceWorker (
    PVOID Parameter
    );

VOID
IopProcessWorkEntry (
    PDEVICE Device,
    PDEVICE_WORK_ENTRY Work
    );

VOID
IopStartDevice (
    PDEVICE Device
    );

KSTATUS
IopAddDrivers (
    PDEVICE Device
    );

PCSTR
IopFindDriverForDevice (
    PDEVICE Device
    );

VOID
IopQueryChildren (
    PDEVICE Device
    );

VOID
IopProcessReportedChildren (
    PDEVICE Device,
    PIRP_QUERY_CHILDREN Result
    );

PSTR
IopGetUniqueDeviceId (
    PDEVICE ParentDevice,
    PCSTR DeviceId
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the device work queue.
//

PWORK_QUEUE IoDeviceWorkQueue;

//
// Define the object that roots the device tree.
//

PDEVICE IoRootDevice;
LIST_ENTRY IoDeviceList;
PQUEUED_LOCK IoDeviceListLock;

//
// Store the number of active work items flowing around.
//

UINTN IoDeviceWorkItemsQueued;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoCreateDevice (
    PDRIVER BusDriver,
    PVOID BusDriverContext,
    PDEVICE ParentDevice,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PDEVICE *NewDevice
    )

/*++

Routine Description:

    This routine creates a new device in the system. This device can be used in
    subsequent calls to Query Children.

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

    NewDevice - Supplies a pointer where the new device will be returned on
        success.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = IopCreateDevice(BusDriver,
                             BusDriverContext,
                             ParentDevice,
                             DeviceId,
                             ClassId,
                             CompatibleIds,
                             ObjectDevice,
                             sizeof(DEVICE),
                             NewDevice);

    return Status;
}

KERNEL_API
KSTATUS
IoRemoveUnreportedDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine removes a device that was created but never reported. Devices
    created on enumerable busses must be removed by not reporting them in
    a query children request. This routine must only be called on devices whose
    parent device is the root.

Arguments:

    Device - Supplies a pointer to the device to remove.

Return Value:

    Status code.

--*/

{

    ULONG Flags;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // It is an error for a driver to yank out an enumerated device.
    //

    if ((Device->State != DeviceUnreported) &&
        (Device->State != DeviceInitialized) &&
        (Device->ParentDevice != IoRootDevice)) {

        KeCrashSystem(CRASH_DRIVER_ERROR,
                      DriverErrorRemovingEnumeratedDevice,
                      (UINTN)Device,
                      Device->State,
                      (UINTN)(Device->ParentDevice));
    }

    Flags = DEVICE_ACTION_SEND_TO_SUBTREE | DEVICE_ACTION_OPEN_QUEUE;
    Status = IopQueueDeviceWork(Device,
                                DeviceActionPrepareRemove,
                                NULL,
                                Flags);

    //
    // If the action failed to queue for a reason other than that the device
    // was already awaiting removal, set the problem state. Do not call the
    // queue failure handler as that can roll back the parent's device state,
    // but in this case the parent isn't expecting an answer from the child's
    // removal process.
    //

    if (!KSUCCESS(Status) &&
        (Status != STATUS_DEVICE_QUEUE_CLOSING)) {

        IopSetDeviceProblem(Device,
                            DeviceProblemFailedToQueuePrepareRemove,
                            Status);
    }

    return Status;
}

KERNEL_API
VOID
IoDeviceAddReference (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine increments the reference count on a device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ObAddReference(Device);
    return;
}

KERNEL_API
VOID
IoDeviceReleaseReference (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine decrements the reference count on a device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ObReleaseReference(Device);
    return;
}

KERNEL_API
KSTATUS
IoSetTargetDevice (
    PDEVICE Device,
    PDEVICE TargetDevice
    )

/*++

Routine Description:

    This routine sets the target device for a given device. IRPs flow through
    a device and then through its target device (if not completed by an
    earlier driver). Target devices allow the piling of stacks on one another.
    Target device relations must be set either before the device is reported
    by the bus, or during AddDevice. They cannot be changed after that. This
    routine is not thread safe, as it's only expected to be called by drivers
    on the device during early device initialization.

Arguments:

    Device - Supplies a pointer to the device to set a target device for.

    TargetDevice - Supplies a pointer to the target device.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_LATE if the device is already too far through its initialization
    to have a target device added to it.

--*/

{

    if (Device->State > DeviceInitialized) {
        return STATUS_TOO_LATE;
    }

    if (Device->TargetDevice != NULL) {
        ObReleaseReference(Device->TargetDevice);
    }

    if (TargetDevice != NULL) {
        ObAddReference(TargetDevice);
    }

    Device->TargetDevice = TargetDevice;
    return STATUS_SUCCESS;
}

KERNEL_API
PDEVICE
IoGetTargetDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine returns the target device for the given device, if any.

Arguments:

    Device - Supplies a pointer to the device to get the target device for.

Return Value:

    Returns a pointer to the target device.

    NULL if there is no target device.

--*/

{

    return Device->TargetDevice;
}

PDEVICE
IoGetDiskDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine returns the underlying disk device for a given device.

Arguments:

    Device - Supplies a pointer to a device.

Return Value:

    Returns a pointer to the disk device that backs the device.

    NULL if the given device does not have a disk backing it.

--*/

{

    if (Device->Header.Type != ObjectVolume) {
        return NULL;
    }

    while (Device->TargetDevice != NULL) {
        Device = Device->TargetDevice;
    }

    return Device;
}

KERNEL_API
VOID
IoSetDeviceMountable (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine indicates that the given device is mountable. A device cannot
    be unmarked as mountable. This routine is not thread safe.

Arguments:

    Device - Supplies a pointer to the device that the system could potentially
        mount.

Return Value:

    None.

--*/

{

    Device->Flags |= DEVICE_FLAG_MOUNTABLE;

    //
    // If the device is not yet fully enumerated, return. The enumeration will
    // take care of creating the volume.
    //

    if (Device->State < DeviceStarted) {
        return;
    }

    //
    // This device is being marked mountable after it's fully started. Create
    // the volume for it now.
    //

    if ((Device->Flags & DEVICE_FLAG_MOUNTED) == 0) {
        IoCreateVolume(Device, NULL);
    }

    return;
}

KERNEL_API
BOOL
IoAreDeviceIdsEqual (
    PCSTR DeviceIdOne,
    PCSTR DeviceIdTwo
    )

/*++

Routine Description:

    This routine determines if the given device IDs match. This routine always
    truncates the given device IDs at the last '#' character, if it exists. If
    one of the supplied device IDs naturally has a '#' character within it,
    then caller should append a second '#' character to the device ID.

Arguments:

    DeviceIdOne - Supplies the first device ID.

    DeviceIdTwo - Supplies the second device ID.

Return Value:

    Returns TRUE if the given device IDs match. FALSE otherwise.

--*/

{

    ULONG DeviceIdOneLength;
    ULONG DeviceIdTwoLength;
    PCSTR LastHash;
    BOOL Result;

    //
    // Find the lengths of the two device IDs. If there is a '#' in the device
    // ID, then that is treated as the last character.
    //

    DeviceIdOneLength = RtlStringLength(DeviceIdOne) + 1;
    LastHash = RtlStringFindCharacterRight(DeviceIdOne, '#', DeviceIdOneLength);
    if (LastHash != NULL) {
        DeviceIdOneLength = (UINTN)LastHash - (UINTN)DeviceIdOne + 1;
    }

    DeviceIdTwoLength = RtlStringLength(DeviceIdTwo) + 1;
    LastHash = RtlStringFindCharacterRight(DeviceIdTwo, '#', DeviceIdTwoLength);
    if (LastHash != NULL) {
        DeviceIdTwoLength = (UINTN)LastHash - (UINTN)DeviceIdTwo + 1;
    }

    //
    // If the device IDs are not the same length, then they cannot match.
    //

    if (DeviceIdOneLength != DeviceIdTwoLength) {
        return FALSE;
    }

    //
    // Compare the device IDs up to the last character. The last characters may
    // be NULL terminators or '#' characters, but not necessarily the same.
    //

    ASSERT((DeviceIdOne[DeviceIdOneLength - 1] == STRING_TERMINATOR) ||
           (DeviceIdOne[DeviceIdOneLength - 1] == '#'));

    ASSERT((DeviceIdTwo[DeviceIdTwoLength - 1] == STRING_TERMINATOR) ||
           (DeviceIdTwo[DeviceIdTwoLength - 1] == '#'));

    Result = RtlAreStringsEqual(DeviceIdOne,
                                DeviceIdTwo,
                                DeviceIdOneLength - 1);

    return Result;
}

KERNEL_API
PCSTR
IoGetDeviceId (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine returns the device ID of the given system device.

Arguments:

    Device - Supplies the device to get the ID of.

Return Value:

    Returns a pionter to a string representing the device's Identifier.

--*/

{

    return Device->Header.Name;
}

KERNEL_API
PCSTR
IoGetCompatibleDeviceIds (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine returns a semilcolon-delimited list of device IDs that this
    device is compatible with.

Arguments:

    Device - Supplies the device to get the compatible IDs of.

Return Value:

    Returns a pointer to a semicolon-delimited string of device IDs that this
    device is compatible with, not including the actual device ID itself.

    NULL if the compatible ID list is empty.

--*/

{

    return Device->CompatibleIds;
}

KERNEL_API
PCSTR
IoGetDeviceClassId (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine returns the class ID of the given device.

Arguments:

    Device - Supplies the device to get the class ID of.

Return Value:

    Returns the class ID of the given device.

    NULL if the device was not created with a class ID.

--*/

{

    return Device->ClassId;
}

KERNEL_API
BOOL
IoIsDeviceIdInCompatibleIdList (
    PCSTR DeviceId,
    PDEVICE Device
    )

/*++

Routine Description:

    This routine determines if the given device ID is present in the semicolon-
    delimited list of compatible device IDs of the given device, or matches
    the device ID itself.

    This routine must be called at Low level.

Arguments:

    DeviceId - Supplies the device ID in question.

    Device - Supplies the device whose compatible IDs should be queried.

Return Value:

    TRUE if the given device ID string is present in the device's compatible ID
        list.

    FALSE if the device ID string is not present in the compatible ID list.

--*/

{

    PSTR CurrentId;
    ULONG DeviceIdSize;
    BOOL Match;
    PSTR MutableCopy;
    BOOL Result;
    PSTR Semicolon;
    ULONG StringSize;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Match = IoAreDeviceIdsEqual(IoGetDeviceId(Device), DeviceId);
    if (Match != FALSE) {
        return TRUE;
    }

    if (Device->CompatibleIds == NULL) {
        return FALSE;
    }

    Result = FALSE;

    //
    // Make a mutable copy of the compatible ID string.
    //

    StringSize = RtlStringLength(Device->CompatibleIds) + 1;
    MutableCopy = MmAllocatePagedPool(StringSize, IO_ALLOCATION_TAG);
    if (MutableCopy == NULL) {
        goto IoIsDeviceIdInCompatibleIdListEnd;
    }

    RtlCopyMemory(MutableCopy, Device->CompatibleIds, StringSize);

    //
    // Loop through every compatible ID.
    //

    DeviceIdSize = RtlStringLength(DeviceId) + 1;
    CurrentId = MutableCopy;
    while (TRUE) {
        if (*CurrentId == '\0') {
            break;
        }

        //
        // Find the next semicolon and mark it as a null terminator.
        //

        Semicolon = RtlStringFindCharacter(CurrentId,
                                           COMPATIBLE_ID_DELIMITER,
                                           StringSize);

        if (Semicolon != NULL) {
            *Semicolon = '\0';
            Semicolon += 1;
        }

        //
        // Compare the IDs.
        //

        Match = RtlAreStringsEqual(DeviceId, CurrentId, DeviceIdSize);
        if (Match != FALSE) {
            Result = TRUE;
            break;
        }

        //
        // Advance the string.
        //

        if (Semicolon == NULL) {
            break;
        }

        StringSize -= (UINTN)Semicolon - (UINTN)CurrentId;
        if (StringSize == 0) {
            break;
        }

        CurrentId = Semicolon;
    }

IoIsDeviceIdInCompatibleIdListEnd:
    if (MutableCopy != NULL) {
        MmFreePagedPool(MutableCopy);
    }

    return Result;
}

KERNEL_API
DEVICE_ID
IoGetDeviceNumericId (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine gets the numeric device ID for the given device.

Arguments:

    Device - Supplies a pointer to the device whose numeric ID is being queried.

Return Value:

    Returns the numeric device ID for the device.

--*/

{

    return Device->DeviceId;
}

KERNEL_API
PDEVICE
IoGetDeviceByNumericId (
    DEVICE_ID DeviceId
    )

/*++

Routine Description:

    This routine looks up a device given its numeric device ID. This routine
    will increment the reference count of the device returned, it is the
    caller's responsibility to release that reference. Only devices that are in
    the started state will be returned. This routine must be called at low
    level.

Arguments:

    DeviceId - Supplies the numeric device ID of the device to get.

Return Value:

    Returns a pointer to the device with the given numeric device ID on
    success. This routine will add a reference to the device returned, it is
    the caller's responsibility to release this reference when finished with
    the device.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEVICE Device;
    PDEVICE FoundDevice;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    FoundDevice = NULL;
    KeAcquireQueuedLock(IoDeviceListLock);
    CurrentEntry = IoDeviceList.Next;
    while (CurrentEntry != &IoDeviceList) {
        Device = LIST_VALUE(CurrentEntry, DEVICE, ListEntry);
        if (Device->DeviceId == DeviceId) {
            if (Device->State == DeviceStarted) {
                ObAddReference(Device);
                FoundDevice = Device;
            }

            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseQueuedLock(IoDeviceListLock);
    return FoundDevice;
}

KERNEL_API
KSTATUS
IoMergeChildArrays (
    PIRP QueryChildrenIrp,
    PDEVICE *Children,
    ULONG ChildCount,
    ULONG AllocationTag
    )

/*++

Routine Description:

    This routine merges a device's enumerated children with the array that is
    already present in the Query Children IRP. If needed, a new array containing
    the merged list will be created and stored in the IRP, and the old list
    will be freed. If the IRP has no list yet, a copy of the array passed in
    will be created and set in the IRP.

Arguments:

    QueryChildrenIrp - Supplies a pointer to the Query Children IRP.

    Children - Supplies a pointer to the device children. This array will not be
        used in the IRP, so this array can be temporarily allocated.

    ChildCount - Supplies the number of elements in the pointer array.

    AllocationTag - Supplies the allocate tag to use for the newly created
        array.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if the new array could not be allocated.

--*/

{

    ULONG AllocationSize;
    PDEVICE *IrpArray;
    ULONG IrpArrayCount;
    ULONG IrpIndex;
    PDEVICE *NewArray;
    ULONG NewCount;
    ULONG NewIndex;
    KSTATUS Status;

    if ((QueryChildrenIrp == NULL) || (Children == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    if (ChildCount == 0) {
        return STATUS_SUCCESS;
    }

    ASSERT(QueryChildrenIrp->MajorCode == IrpMajorStateChange);
    ASSERT(QueryChildrenIrp->MinorCode == IrpMinorQueryChildren);

    IrpArray = QueryChildrenIrp->U.QueryChildren.Children;
    IrpArrayCount = QueryChildrenIrp->U.QueryChildren.ChildCount;

    //
    // First look to see if all devices in the child array are already in the
    // existing IRP.
    //

    if ((IrpArray != NULL) && (IrpArrayCount != 0)) {
        for (NewIndex = 0; NewIndex < ChildCount; NewIndex += 1) {

            //
            // Look through the IRP array for the given child.
            //

            for (IrpIndex = 0; IrpIndex < IrpArrayCount; IrpIndex += 1) {
                if (IrpArray[IrpIndex] == Children[NewIndex]) {
                    break;
                }
            }

            //
            // If the child was not found, stop looping.
            //

            if (IrpIndex == IrpArrayCount) {
                break;
            }
        }

        //
        // If every device in the new array was already in the existing array,
        // then there's nothing to do, the existing list is fine.
        //

        if (NewIndex == ChildCount) {
            Status = STATUS_SUCCESS;
            goto MergeChildArraysEnd;
        }
    }

    //
    // Make a pessimistically sized array assuming nothing will merge.
    //

    AllocationSize = (IrpArrayCount + ChildCount) * sizeof(PDEVICE);
    NewArray = MmAllocatePagedPool(AllocationSize, AllocationTag);
    if (NewArray == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MergeChildArraysEnd;
    }

    RtlZeroMemory(NewArray, AllocationSize);

    //
    // If there is no existing array, just copy and finish!
    //

    if ((IrpArray == NULL) || (IrpArrayCount == 0)) {
        RtlCopyMemory(NewArray, Children, ChildCount * sizeof(PDEVICE));
        NewCount = ChildCount;
        QueryChildrenIrp->U.QueryChildren.Children = NewArray;
        QueryChildrenIrp->U.QueryChildren.ChildCount = NewCount;
        Status = STATUS_SUCCESS;
        goto MergeChildArraysEnd;
    }

    //
    // An existing array definitely exists. Start by copying in all the stuff
    // that's already there.
    //

    RtlCopyMemory(NewArray, IrpArray, IrpArrayCount * sizeof(PDEVICE));
    NewCount = IrpArrayCount;

    //
    // Go through every child again, and if it's not already in the list, add
    // it to the list.
    //

    for (NewIndex = 0; NewIndex < ChildCount; NewIndex += 1) {

        //
        // Look to see if it is already in the destination list.
        //

        for (IrpIndex = 0; IrpIndex < NewCount; IrpIndex += 1) {
            if (NewArray[IrpIndex] == Children[NewIndex]) {
                break;
            }
        }

        //
        // If the child was not found, add it to the end of the array. This
        // will not overflow because the array was allocated assuming nothing
        // would merge.
        //

        if (IrpIndex == NewCount) {
            NewArray[NewCount] = Children[NewIndex];
            NewCount += 1;
        }
    }

    //
    // Free the old array and replace it with this great one.
    //

    MmFreePagedPool(QueryChildrenIrp->U.QueryChildren.Children);
    QueryChildrenIrp->U.QueryChildren.Children = NewArray;
    QueryChildrenIrp->U.QueryChildren.ChildCount = NewCount;
    Status = STATUS_SUCCESS;

MergeChildArraysEnd:
    return Status;
}

KERNEL_API
KSTATUS
IoNotifyDeviceTopologyChange (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine notifies the system that the device topology has changed for
    the given device. This routine is meant to be called by a device driver
    when it notices a child device is missing or when a new device arrives.

Arguments:

    Device - Supplies a pointer to the device whose topology has changed.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Device != NULL);

    //
    // Queue up a work item to handle this, allowing the driver to finish
    // processing.
    //

    Status = IopQueueDeviceWork(Device, DeviceActionQueryChildren, NULL, 0);
    if (!KSUCCESS(Status)) {
        goto NotifyDeviceTopologyChangeEnd;
    }

NotifyDeviceTopologyChangeEnd:
    return Status;
}

KERNEL_API
BOOL
IoIsDeviceStarted (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine returns whether or not the device is in the started state.

Arguments:

    Device - Supplies a pointer to the device in question.

Return Value:

    Returns TRUE if the device is in the started state, or FALSE otherwise.

--*/

{

    //
    // This is a simple spot check and does not wait on the device to signal.
    //

    if (Device->State == DeviceStarted) {
        return TRUE;
    }

    return FALSE;
}

KERNEL_API
VOID
IoSetDeviceDriverErrorEx (
    PDEVICE Device,
    KSTATUS Status,
    PDRIVER Driver,
    ULONG DriverError,
    PCSTR SourceFile,
    ULONG LineNumber
    )

/*++

Routine Description:

    This routine sets a driver specific error code on a given device. This
    problem is preventing a device from making forward progress. Avoid calling
    this function directly, use the non-Ex version.

Arguments:

    Device - Supplies a pointer to the device with the error.

    Status - Supplies the failure status generated by the error.

    Driver - Supplies a pointer to the driver reporting the error.

    DriverError - Supplies an optional driver specific error code.

    SourceFile - Supplies a pointer to the source file where the problem
        occurred. This is usually automatically generated by the compiler.

    LineNumber - Supplies the line number in the source file where the problem
        occurred. This is usually automatically generated by the compiler.

Return Value:

    None.

--*/

{

    IopSetDeviceProblemEx(Device,
                          DeviceProblemDriverError,
                          Status,
                          Driver,
                          DriverError,
                          SourceFile,
                          LineNumber);

    return;
}

KERNEL_API
KSTATUS
IoClearDeviceProblem (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine clears any problem code associated with a device, and attempts
    to start the device if it is not already started.

Arguments:

    Device - Supplies a pointer to the device to clear.

Return Value:

    STATUS_SUCCESS if the problem was successfully cleared and the start device
    work item was successfully queued (if needed).

    Error code if the start work item needed to be queued and had a problem.

--*/

{

    KSTATUS Status;

    Device->ProblemState.Problem = DeviceProblemNone;
    Device->ProblemState.Driver = NULL;

    //
    // Signal anyone waiting on the device. They were queued up waiting for it
    // to complete a state transition. It failed to do so; let them check the
    // status.
    //

    ObSignalObject(Device, SignalOptionUnsignal);
    Status = STATUS_SUCCESS;
    if (Device->State != DeviceStarted) {
        Status = IopQueueDeviceWork(Device, DeviceActionStart, NULL, 0);
        if (!KSUCCESS(Status)) {
            IopSetDeviceProblem(Device,
                                DeviceProblemFailedToQueueStart,
                                Status);
        }
    }

    return Status;
}

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
    )

/*++

Routine Description:

    This routine creates a new device or volume. This routine must be called at
    low level.

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

{

    ULONG AllocationSize;
    PSTR ClassIdCopy;
    PSTR CompatibleIdsCopy;
    PDEVICE Device;
    KSTATUS Status;
    PSTR StringBuffer;
    ULONG StringLength;
    PSTR UniqueDeviceId;

    ASSERT((DeviceType == ObjectDevice) || (DeviceType == ObjectVolume));
    ASSERT((DeviceSize == sizeof(DEVICE)) || (DeviceSize == sizeof(VOLUME)));
    ASSERT(KeGetRunLevel() == RunLevelLow);

    ClassIdCopy = NULL;
    CompatibleIdsCopy = NULL;
    Device = NULL;
    StringBuffer = NULL;
    UniqueDeviceId = NULL;
    if (NewDevice != NULL) {
        *NewDevice = NULL;
    }

    //
    // At least a device ID must be supplied.
    //

    if (DeviceId == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    StringLength = RtlStringLength(DeviceId) + 1;
    if (StringLength > MAX_DEVICE_ID) {
        Status = STATUS_NAME_TOO_LONG;
        goto CreateDeviceEnd;
    }

    //
    // Make sure the device ID is unique.
    //

    UniqueDeviceId = IopGetUniqueDeviceId(ParentDevice, DeviceId);
    if (UniqueDeviceId == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDeviceEnd;
    }

    //
    // Determine the allocation size by adding all the optional strings
    // together. The device ID will get copied during object creation.
    //

    AllocationSize = 0;
    if (ClassId != NULL) {
        StringLength = RtlStringLength(ClassId) + 1;
        if (StringLength > MAX_DEVICE_ID) {
            Status = STATUS_NAME_TOO_LONG;
            goto CreateDeviceEnd;
        }

        AllocationSize += StringLength;
    }

    if (CompatibleIds != NULL) {
        StringLength = RtlStringLength(CompatibleIds) + 1;
        if (AllocationSize + StringLength < AllocationSize) {
            Status = STATUS_NAME_TOO_LONG;
            goto CreateDeviceEnd;
        }

        AllocationSize += StringLength;
    }

    //
    // Allocate the optional strings at once and copy them over.
    //

    if (AllocationSize != 0) {
        StringBuffer = MmAllocatePagedPool(AllocationSize,
                                           DEVICE_ALLOCATION_TAG);

        if (StringBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateDeviceEnd;
        }

        StringLength = 0;
        if (ClassId != NULL) {
            ClassIdCopy = StringBuffer;
            StringLength = RtlStringCopy(ClassIdCopy, ClassId, AllocationSize);
            AllocationSize -= StringLength;
        }

        if (CompatibleIds != NULL) {
            CompatibleIdsCopy = StringBuffer + StringLength;
            AllocationSize -= RtlStringCopy(CompatibleIdsCopy,
                                            CompatibleIds,
                                            AllocationSize);
        }
    }

    ASSERT(AllocationSize == 0);

    //
    // If no parent device was supplied, the device is created under the root.
    //

    if (ParentDevice == NULL) {
        ParentDevice = IoRootDevice;
    }

    //
    // Create the device object.
    //

    Device = ObCreateObject(DeviceType,
                            ParentDevice,
                            UniqueDeviceId,
                            RtlStringLength(UniqueDeviceId) + 1,
                            DeviceSize,
                            IopDestroyDevice,
                            0,
                            DEVICE_ALLOCATION_TAG);

    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDeviceEnd;
    }

    Device->DeviceId = IopGetNextDeviceId();
    Device->Lock = KeCreateSharedExclusiveLock();
    if (Device->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDeviceEnd;
    }

    Device->QueueLock = KeCreateQueuedLock();
    if (Device->QueueLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDeviceEnd;
    }

    //
    // Initialize the active child list.
    //

    INITIALIZE_LIST_HEAD(&(Device->ActiveChildListHead));

    ASSERT(Device->ActiveListEntry.Next == NULL);

    //
    // Initialize the arbiter lists.
    //

    INITIALIZE_LIST_HEAD(&(Device->ArbiterListHead));
    INITIALIZE_LIST_HEAD(&(Device->ArbiterAllocationListHead));

    //
    // Initialize string pointers.
    //

    Device->ClassId = ClassIdCopy;
    Device->CompatibleIds = CompatibleIdsCopy;

    //
    // Store the parent device. A reference does not need to be taken here
    // because the object manager already took a reference on the parent.
    //

    Device->ParentDevice = ParentDevice;

    //
    // Initialize the device queue.
    //

    INITIALIZE_LIST_HEAD(&(Device->WorkQueue));

    ASSERT(Device->DriverStackSize == 0);

    INITIALIZE_LIST_HEAD(&(Device->DriverStackHead));
    IopSetDeviceState(Device, DeviceUnreported);
    Device->QueueState = DeviceQueueClosed;

    //
    // Attach the bus driver if present.
    //

    if (BusDriver != NULL) {
        Status = IoAttachDriverToDevice(BusDriver, Device, BusDriverContext);
        if (!KSUCCESS(Status)) {
            goto CreateDeviceEnd;
        }
    }

    //
    // If the device was enumerated by something, then it needs to be reported
    // as well. If this was an unenumerable device, set the state straight to
    // initialized.
    //

    if ((ParentDevice == IoRootDevice) ||
        (ParentDevice == (PDEVICE)IoVolumeDirectory)) {

        IopSetDeviceState(Device, DeviceInitialized);
        Device->QueueState = DeviceQueueOpen;
    }

    //
    // With success on the horizon, add this element to the parent device's
    // active child list, unless the parent is the volume directory.
    //

    if ((ParentDevice != NULL) &&
        (ParentDevice != (PDEVICE)IoVolumeDirectory)) {

        //
        // Acquire the parent device's lock exclusivley and make sure that the
        // parent isn't in the process of being removed.
        //

        KeAcquireSharedExclusiveLockExclusive(ParentDevice->Lock);
        if (ParentDevice->State == DeviceAwaitingRemoval) {
            KeReleaseSharedExclusiveLockExclusive(ParentDevice->Lock);
            Status = STATUS_PARENT_AWAITING_REMOVAL;
            goto CreateDeviceEnd;
        }

        //
        // Device creation should never happen with a removed parent. A device
        // in the removed state has received the remove IRP and should not be
        // creating new devices.
        //

        ASSERT(ParentDevice->State != DeviceRemoved);

        INSERT_BEFORE(&(Device->ActiveListEntry),
                      &(ParentDevice->ActiveChildListHead));

        KeReleaseSharedExclusiveLockExclusive(ParentDevice->Lock);
    }

    //
    // Add this device to the global list.
    //

    KeAcquireQueuedLock(IoDeviceListLock);
    INSERT_BEFORE(&(Device->ListEntry), &IoDeviceList);
    KeReleaseQueuedLock(IoDeviceListLock);

    //
    // If this is an unenumerable device, kick off the start action.
    //

    if (ParentDevice == IoRootDevice) {
        Status = IopQueueDeviceWork(Device, DeviceActionStart, NULL, 0);
        if (!KSUCCESS(Status)) {
            IopSetDeviceProblem(Device,
                                DeviceProblemFailedToQueueStart,
                                Status);

            goto CreateDeviceEnd;
        }
    }

    Status = STATUS_SUCCESS;

CreateDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device != NULL) {

            //
            // If the device's parent is the root, then it may have failed
            // after being placed on the active list. Remove it and then
            // destroy the device.
            //

            if ((ParentDevice == IoRootDevice) &&
                (Device->ActiveListEntry.Next != NULL)) {

                KeAcquireSharedExclusiveLockExclusive(ParentDevice->Lock);
                if (Device->ActiveListEntry.Next != NULL) {
                    LIST_REMOVE(&(Device->ActiveListEntry));
                }

                KeReleaseSharedExclusiveLockExclusive(ParentDevice->Lock);
            }

            ObReleaseReference(Device);

        } else {
            if (StringBuffer != NULL) {
                MmFreePagedPool(StringBuffer);
            }
        }

    } else {
        if (NewDevice != NULL) {
            *NewDevice = Device;
        }

        RtlDebugPrint("New Device: %s, 0x%x\n", Device->Header.Name, Device);
    }

    if ((UniqueDeviceId != DeviceId) && (UniqueDeviceId != NULL)) {
        MmFreePagedPool(UniqueDeviceId);
    }

    return Status;
}

VOID
IopSetDeviceState (
    PDEVICE Device,
    DEVICE_STATE NewState
    )

/*++

Routine Description:

    This routine sets the device to a new state.

Arguments:

    Device - Supplies a pointer to the device whose state is to be changed.

    NewState - Supplies a pointer to the new device state.

Return Value:

    None.

--*/

{

    Device->StateHistory[Device->StateHistoryNextIndex] = Device->State;
    Device->StateHistoryNextIndex += 1;
    if (Device->StateHistoryNextIndex == DEVICE_STATE_HISTORY) {
        Device->StateHistoryNextIndex = 0;
    }

    Device->State = NewState;
    return;
}

KSTATUS
IopQueueDeviceWork (
    PDEVICE Device,
    DEVICE_ACTION Action,
    PVOID Parameter,
    ULONG Flags
    )

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

{

    PDEVICE_WORK_ENTRY NewEntry;
    BOOL NewWorkItemNeeded;
    DEVICE_QUEUE_STATE OldQueueState;
    BOOL Result;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Attempts to queue the remove action should also close the queue.
    //

    ASSERT((Action != DeviceActionRemove) ||
           ((Flags & DEVICE_ACTION_CLOSE_QUEUE) != 0));

    NewEntry = NULL;
    Status = STATUS_SUCCESS;
    if (Device->QueueState == DeviceQueueActiveClosing) {
        Status = STATUS_DEVICE_QUEUE_CLOSING;
        goto QueueDeviceWorkEnd;
    }

    //
    // Do not queue work to a device that is in an invalid state or whose queue
    // is closed unless the open queue flag is supplied.
    //

    if (((Flags & DEVICE_ACTION_OPEN_QUEUE) == 0) &&
        ((!IO_IS_DEVICE_ALIVE(Device)) || (!IO_IS_DEVICE_QUEUE_OPEN(Device)))) {

        Status = STATUS_NO_ELIGIBLE_DEVICES;
        goto QueueDeviceWorkEnd;
    }

    //
    // Determine if a test hook is requesting this call to fail.
    //

    Result = IopIsTestHookSet(IO_FAIL_QUEUE_DEVICE_WORK);
    if (Result != FALSE) {
        Status = STATUS_UNSUCCESSFUL;
        goto QueueDeviceWorkEnd;
    }

    //
    // Allocate the work item entry.
    //

    NewEntry = MmAllocatePagedPool(sizeof(DEVICE_WORK_ENTRY),
                                   DEVICE_WORK_ALLOCATION_TAG);

    if (NewEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto QueueDeviceWorkEnd;
    }

    NewEntry->Action = Action;
    NewEntry->Flags = Flags;
    NewEntry->Parameter = Parameter;

    //
    // Acquire the lock and insert the item onto the queue. While the lock is
    // held, determine if a work item is already in flight for this device.
    // There's also a chance that the work queue closed or was marked as
    // closing since it was last checked.
    //

    NewWorkItemNeeded = TRUE;
    KeAcquireQueuedLock(Device->QueueLock);
    OldQueueState = Device->QueueState;
    if (Device->QueueState == DeviceQueueActiveClosing) {
        Status = STATUS_DEVICE_QUEUE_CLOSING;

    } else if (IO_IS_DEVICE_QUEUE_OPEN(Device) ||
               ((Flags & DEVICE_ACTION_OPEN_QUEUE) != 0)) {

        ASSERT(Device->State != DeviceRemoved);

        if (Device->QueueState == DeviceQueueActive) {
            NewWorkItemNeeded = FALSE;

        } else {

            ASSERT(LIST_EMPTY(&(Device->WorkQueue)) != FALSE);

            Device->QueueState = DeviceQueueActive;
        }

        INSERT_BEFORE(&(NewEntry->ListEntry), &(Device->WorkQueue));

        //
        // Mark the queue as closing if requested.
        //

        if ((Flags & DEVICE_ACTION_CLOSE_QUEUE) != 0) {
            Device->QueueState = DeviceQueueActiveClosing;
        }

    } else {
        Status = STATUS_NO_ELIGIBLE_DEVICES;
    }

    KeReleaseQueuedLock(Device->QueueLock);
    if (!KSUCCESS(Status)) {
        goto QueueDeviceWorkEnd;
    }

    //
    // If the device queue was not actively processing items, queue a work
    // item to process the new work.
    //

    if (NewWorkItemNeeded != FALSE) {
        RtlAtomicAdd(&IoDeviceWorkItemsQueued, 1);
        Status = KeCreateAndQueueWorkItem(IoDeviceWorkQueue,
                                          WorkPriorityNormal,
                                          IopDeviceWorker,
                                          Device);

        if (!KSUCCESS(Status)) {
            RtlAtomicAdd(&IoDeviceWorkItemsQueued, -1);

            //
            // Bad news if a work item could not be queued. Mark the queue as
            // open but not active so that the next request will try again to
            // create a work item.
            //

            KeAcquireQueuedLock(Device->QueueLock);

            //
            // If the queue was active and work item creation failed, revert to
            // the old queue state so the next request can try to create a work
            // item. No work item could have been allocated before the failure.
            //

            ASSERT((Device->QueueState == DeviceQueueActive) ||
                   (Device->QueueState == DeviceQueueActiveClosing));

            Device->QueueState = OldQueueState;

            //
            // Remove this item from the list.
            //

            LIST_REMOVE(&(NewEntry->ListEntry));
            KeReleaseQueuedLock(Device->QueueLock);
            goto QueueDeviceWorkEnd;
        }
    }

QueueDeviceWorkEnd:
    if (!KSUCCESS(Status)) {
        if (NewEntry != NULL) {
            MmFreePagedPool(NewEntry);
        }
    }

    return Status;
}

VOID
IopHandleDeviceQueueFailure (
    PDEVICE Device,
    DEVICE_ACTION Action
    )

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

{

    switch (Action) {

    //
    // A prepare remove action may not have been able to be queued for a few
    // reasons:
    //
    // 1. Another device tree removal process already scheduled the remove
    //    action on the given device, but it is yet to run. This case should
    //    be handled by the caller by ignoring the "queue closing" failure
    //    status from the attempt to queue the work item.
    //
    // 2. An allocation failed, in which case the device tree state must be
    //    rolled back because a parent device is expecting this child to be
    //    removed. This queue failure handler should only be called if the
    //    parent device expects a response from the prepare remove action. That
    //    is, do not call it when the root of a removal tree fails to queue the
    //    action.
    //

    case DeviceActionPrepareRemove:

        //
        // The device should not be in the removed state when queing fails.
        //

        ASSERT(Device->State != DeviceRemoved);

        //
        // The prepare remove work item can fail because it is in the awaiting
        // removal state and the remove work item has already been queued. This
        // case, however, should be handled by the queuer; assert here to make
        // sure the case does not enter this code path.
        //

        ASSERT((Device->State != DeviceAwaitingRemoval) ||
               (Device->QueueState != DeviceQueueActiveClosing));

        IopAbortDeviceRemoval(Device,
                              DeviceProblemFailedToQueuePrepareRemove,
                              FALSE);

        break;

    //
    // A device remove action can only be triggered from the prepare remove
    // work item or from a child's removal work item. Any device that has
    // children should be able to queue work items. Additionally, once a device
    // is in the awaiting removal state, there should only ever be one attempt
    // at queuing a removal work item. This means that the only reason for a
    // removal queuing to fail is due to allocation failure. The only recourse
    // to a failed allocation in this code path is to roll back the removal
    // process.
    //

    case DeviceActionRemove:

        //
        // The device should be awaiting removal, meaning that the device is
        // active enough to receive work items. But further assert that it is
        // active, meaning there is no reason the device state kept it from
        // receiving a work item.
        //

        ASSERT(Device->State == DeviceAwaitingRemoval);
        ASSERT(IO_IS_DEVICE_ALIVE(Device) != FALSE);

        //
        // The removal action should not fail to be appended to the work queue
        // because the queue is closed or closing.
        //

        ASSERT(IO_IS_DEVICE_QUEUE_OPEN(Device) != FALSE);

        //
        // Abort the removal process, reverting the actions of the prepare
        // removal work item.
        //

        IopAbortDeviceRemoval(Device, DeviceProblemFailedToQueueRemove, TRUE);
        break;

    case DeviceActionStart:
    case DeviceActionQueryChildren:
    case DeviceActionInvalid:
    default:
        break;
    }

    return;
}

VOID
IopSetDeviceProblemEx (
    PDEVICE Device,
    DEVICE_PROBLEM Problem,
    KSTATUS Status,
    PDRIVER Driver,
    ULONG DriverCode,
    PCSTR SourceFile,
    ULONG LineNumber
    )

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

{

    Device->ProblemState.Problem = Problem;
    Device->ProblemState.Driver = Driver;
    Device->ProblemState.Status = Status;
    Device->ProblemState.DriverCode = DriverCode;
    Device->ProblemState.File = SourceFile;
    Device->ProblemState.Line = LineNumber;

    //
    // Signal anyone waiting on the device. They were queued up waiting for it
    // to complete a state transition. It failed to do so; let them check the
    // status.
    //

    ObSignalObject(Device, SignalOptionSignalAll);
    return;
}

VOID
IopClearDeviceProblem (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine clears any problem code associated with a device.

Arguments:

    Device - Supplies a pointer to the device whose problem code should be
        cleared.

Return Value:

    None.

--*/

{

    RtlZeroMemory(&(Device->ProblemState), sizeof(DEVICE_PROBLEM_STATE));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
IopDeviceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the work item that performs actions on devices.

Arguments:

    Parameter - Supplies the work item parameter, which in this case is a
        pointer to the device to operate on.

Return Value:

    None.

--*/

{

    PDEVICE ChildDevice;
    PLIST_ENTRY CurrentEntry;
    PDEVICE Device;
    PDEVICE FailedDevice;
    ULONG NewFlags;
    UINTN OldWorkItemCount;
    BOOL QueueClosed;
    KSTATUS Status;
    PDEVICE_WORK_ENTRY Work;

    Device = (PDEVICE)Parameter;

    //
    // Loop processing items in the queue.
    //

    while (TRUE) {
        Work = NULL;

        //
        // Dequeue an item.
        //

        QueueClosed = FALSE;
        KeAcquireQueuedLock(Device->QueueLock);

        //
        // If the queue is empty, this work item is finished.
        //

        if (LIST_EMPTY(&(Device->WorkQueue)) != FALSE) {

            ASSERT(Device->QueueState == DeviceQueueActive);

            Device->QueueState = DeviceQueueOpen;

        //
        // This list is not empty, so get an item.
        //

        } else {

            ASSERT((Device->QueueState == DeviceQueueActive) ||
                   (Device->QueueState == DeviceQueueActiveClosing));

            Work = LIST_VALUE(Device->WorkQueue.Next,
                              DEVICE_WORK_ENTRY,
                              ListEntry);

            LIST_REMOVE(&(Work->ListEntry));

            //
            // If the queue is in the active closing state and this is the last
            // item on the list, indicate that the queue should be closed
            // immediately after the work item is executed.
            //
            // N.B. This requires the queue empty check because this could be
            //      a work item on the queue in front of the remove action.
            //

            if ((Device->QueueState == DeviceQueueActiveClosing) &&
                (LIST_EMPTY(&(Device->WorkQueue)) != FALSE)) {

                ASSERT((Work->Flags & DEVICE_ACTION_CLOSE_QUEUE) != 0);
                ASSERT(Work->Action == DeviceActionRemove);

                QueueClosed = TRUE;
            }
        }

        KeReleaseQueuedLock(Device->QueueLock);

        //
        // If no work was found, end this work item.
        //

        if (Work == NULL) {
            goto DeviceWorkerEnd;
        }

        //
        // Do the work, except skip the root device itself.
        //

        if (Device != IoRootDevice) {
            IopProcessWorkEntry(Device, Work);
        }

        //
        // If the device queue was closed above it means that the device worker
        // just processed a remove work item. The remove work item can release
        // the last reference on a device, meaning that this routine can no
        // longer safely touch the device structure. In this case, exit
        // immediately and do not process any children.
        //

        if (QueueClosed != FALSE) {

            ASSERT((Work->Flags & DEVICE_ACTION_SEND_TO_SUBTREE) == 0);
            ASSERT((Work->Flags & DEVICE_ACTION_SEND_TO_CHILDREN) == 0);
            ASSERT((Work->Flags & DEVICE_ACTION_CLOSE_QUEUE) != 0);
            ASSERT(Work->Action == DeviceActionRemove);

            MmFreePagedPool(Work);
            goto DeviceWorkerEnd;
        }

        //
        // If this request is to be propagated to the children, queue those
        // requests now. Acquire the device's lock shared while traversing the
        // children as it would be bad if the list changed in the middle of the
        // loop.
        //

        if (((Work->Flags & DEVICE_ACTION_SEND_TO_SUBTREE) != 0) ||
            ((Work->Flags & DEVICE_ACTION_SEND_TO_CHILDREN) != 0)) {

            FailedDevice = NULL;
            NewFlags = Work->Flags & ~DEVICE_ACTION_SEND_TO_CHILDREN;
            KeAcquireSharedExclusiveLockShared(Device->Lock);
            CurrentEntry = Device->ActiveChildListHead.Next;
            while (CurrentEntry != &(Device->ActiveChildListHead)) {
                ChildDevice = LIST_VALUE(CurrentEntry, DEVICE, ActiveListEntry);
                CurrentEntry = CurrentEntry->Next;

                //
                // Queue the same work item for the child device. It is
                // important that the device's queue lock is NOT held at this
                // point because this routine will modify the queue.
                //

                Status = IopQueueDeviceWork(ChildDevice,
                                            Work->Action,
                                            Work->Parameter,
                                            NewFlags);

                if (!KSUCCESS(Status) &&
                    (Status != STATUS_DEVICE_QUEUE_CLOSING)) {

                    FailedDevice = ChildDevice;
                    ObAddReference(FailedDevice);
                    break;
                }
            }

            KeReleaseSharedExclusiveLockShared(Device->Lock);

            //
            // Handle any failures outside of the loop.
            //

            if (FailedDevice != NULL) {
                IopHandleDeviceQueueFailure(FailedDevice, Work->Action);
                ObReleaseReference(FailedDevice);
            }
        }

        //
        // Free this work entry.
        //

        MmFreePagedPool(Work);
    }

DeviceWorkerEnd:
    OldWorkItemCount = RtlAtomicAdd(&IoDeviceWorkItemsQueued, -1);
    if (OldWorkItemCount == 1) {
        IopQueueDelayedResourceAssignment();
    }

    return;
}

VOID
IopProcessWorkEntry (
    PDEVICE Device,
    PDEVICE_WORK_ENTRY Work
    )

/*++

Routine Description:

    This routine processes a device work request.

Arguments:

    Device - Supplies a pointer to the device to operate on.

    Work - Supplies a pointer to the work request.

Return Value:

    None.

--*/

{

    switch (Work->Action) {
    case DeviceActionStart:
        IopStartDevice(Device);
        break;

    case DeviceActionQueryChildren:
        IopQueryChildren(Device);
        break;

    case DeviceActionPrepareRemove:
        IopPrepareRemoveDevice(Device, Work);
        break;

    case DeviceActionRemove:
        IopRemoveDevice(Device, Work);
        break;

    case DeviceActionPowerTransition:
        PmpDevicePowerTransition(Device);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
IopStartDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine attempts to process a device from initialized to started, or
    as far as possible until started.

Arguments:

    Device - Supplies a pointer to the device to start.

Return Value:

    None.

--*/

{

    BOOL Breakout;
    IRP_QUERY_RESOURCES QueryResources;
    IRP_START_DEVICE StartDevice;
    KSTATUS Status;

    //
    // Loop until a resting state is achieved.
    //

    Breakout = FALSE;
    while (Breakout == FALSE) {
        switch (Device->State) {

        //
        // The device has been initialized. Add drivers to the stack.
        //

        case DeviceInitialized:
            if (Device->Header.Type == ObjectVolume) {
                Status = IopAddFileSystem(Device);

            } else {
                Status = IopAddDrivers(Device);
            }

            if (KSUCCESS(Status)) {
                IopSetDeviceState(Device, DeviceDriversAdded);

            } else {
                Breakout = TRUE;
            }

            break;

        //
        // The driver stack has been built. Ask the device about resources.
        //

        case DeviceDriversAdded:
            QueryResources.ResourceRequirements = NULL;
            QueryResources.BootAllocation = NULL;
            Status = IopSendStateChangeIrp(Device,
                                           IrpMinorQueryResources,
                                           &QueryResources,
                                           sizeof(QueryResources));

            if (!KSUCCESS(Status)) {
                IopSetDeviceProblem(Device,
                                    DeviceProblemFailedQueryResources,
                                    Status);

                Breakout = TRUE;
                break;
            }

            Device->ResourceRequirements = QueryResources.ResourceRequirements;
            Device->BootResources = QueryResources.BootAllocation;
            IopSetDeviceState(Device, DeviceResourcesQueried);
            break;

        //
        // Queue the resource assignment.
        //

        case DeviceResourcesQueried:
            Status = IopQueueResourceAssignment(Device);
            if (!KSUCCESS(Status)) {
                Breakout = TRUE;
            }

            break;

        //
        // While the resource assignment is in the queue, there's nothing to
        // do but wait.
        //

        case DeviceResourceAssignmentQueued:
            Breakout = TRUE;
            break;

        //
        // Start the device.
        //

        case DeviceResourcesAssigned:
            StartDevice.ProcessorLocalResources =
                                               Device->ProcessorLocalResources;

            StartDevice.BusLocalResources = Device->BusLocalResources;
            Status = IopSendStateChangeIrp(Device,
                                           IrpMinorStartDevice,
                                           &StartDevice,
                                           sizeof(StartDevice));

            if (!KSUCCESS(Status)) {
                IopSetDeviceProblem(Device, DeviceProblemFailedStart, Status);
                Breakout = TRUE;
                break;
            }

            //
            // Set the device state to awaiting enumeration and queue child
            // enumeration.
            //

            IopSetDeviceState(Device, DeviceAwaitingEnumeration);
            Status = IopQueueDeviceWork(Device,
                                        DeviceActionQueryChildren,
                                        NULL,
                                        0);

            if (!KSUCCESS(Status)) {
                IopSetDeviceProblem(Device,
                                    DeviceProblemFailedToQueueQueryChildren,
                                    Status);

                Breakout = TRUE;
                break;
            }

            break;

        //
        // If the device enumeration is in the queue, there's nothing to do but
        // wait.
        //

        case DeviceAwaitingEnumeration:
            Breakout = TRUE;
            break;

        //
        // If enumeration completed, roll the device to the started state and
        // if it is a new disk device, alert the file system.
        //

        case DeviceEnumerated:
            IopSetDeviceState(Device, DeviceStarted);
            if (((Device->Flags & DEVICE_FLAG_MOUNTABLE) != 0) &&
                ((Device->Flags & DEVICE_FLAG_MOUNTED) == 0)) {

                IoCreateVolume(Device, NULL);
            }

            //
            // If the device is a volume, perform volume arrival actions. As
            // this operation does not happen on the device's work queue, there
            // is nothing preventing device removal from releasing the original
            // reference on the volume. Take another that volume arrival will
            // release.
            //

            if (Device->Header.Type == ObjectVolume) {
                ObAddReference(Device);
                Status = KeCreateAndQueueWorkItem(IoDeviceWorkQueue,
                                                  WorkPriorityNormal,
                                                  IopVolumeArrival,
                                                  Device);

                if (!KSUCCESS(Status)) {
                    ObReleaseReference(Device);
                    ObSignalObject(Device, SignalOptionSignalAll);
                }

            //
            // Otherwise signal the device now that is has reached the start
            // state.
            //

            } else {
                ObSignalObject(Device, SignalOptionSignalAll);
            }

            Breakout = TRUE;
            break;

        //
        // If the device is already started, then there's nothing to do.
        //

        case DeviceStarted:
            Breakout = TRUE;
            break;

        //
        // If the device is awaiting removal, do not proceed with the start
        // sequence.
        //

        case DeviceAwaitingRemoval:
            Breakout = TRUE;
            break;

        //
        // The device should not be found in this state.
        //

        default:

            ASSERT(FALSE);

            IopSetDeviceProblem(Device,
                                DeviceProblemInvalidState,
                                STATUS_UNSUCCESSFUL);

            Breakout = TRUE;
            break;
        }
    }

    return;
}

KSTATUS
IopAddDrivers (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine builds the driver stack for the device. If the device stack is
    partially built, this routine will attempt to finish it.

Arguments:

    Device - Supplies a pointer to the device to add drivers for.

Return Value:

    Status code.

--*/

{

    PDRIVER_ADD_DEVICE AddDevice;
    PDRIVER FunctionDriver;
    PCSTR FunctionDriverName;
    ULONG OriginalStackSize;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Device->State == DeviceInitialized);

    FunctionDriver = NULL;

    //
    // Clear a previous driver load device problem before trying a second time.
    //

    if (Device->ProblemState.Problem == DeviceProblemFailedDriverLoad) {
        IopClearDeviceProblem(Device);
    }

    //
    // Find and load a functional driver.
    //

    OriginalStackSize = Device->DriverStackSize;
    FunctionDriverName = IopFindDriverForDevice(Device);
    if (FunctionDriverName != NULL) {
        Status = IoLoadDriver(FunctionDriverName, &FunctionDriver);
        if (!KSUCCESS(Status)) {
            IopSetDeviceProblem(Device,
                                DeviceProblemFailedDriverLoad,
                                Status);

            goto AddDriversEnd;
        }

        //
        // Call the driver's AddDevice.
        //

        if ((FunctionDriver->Flags & DRIVER_FLAG_FAILED_DRIVER_ENTRY) == 0) {
            if (FunctionDriver->FunctionTable.AddDevice != NULL) {
                AddDevice = FunctionDriver->FunctionTable.AddDevice;
                Status = AddDevice(FunctionDriver,
                                   IoGetDeviceId(Device),
                                   Device->ClassId,
                                   Device->CompatibleIds,
                                   Device);

                if (!KSUCCESS(Status)) {
                    IopSetDeviceProblem(Device,
                                        DeviceProblemFailedAddDevice,
                                        Status);

                    goto AddDriversEnd;
                }

            } else {
                Status = STATUS_DRIVER_FUNCTION_MISSING;
                IopSetDeviceProblem(Device, DeviceProblemNoAddDevice, Status);
                goto AddDriversEnd;
            }
        }

        //
        // Release the reference on the driver added from the load call.
        //

        IoDriverReleaseReference(FunctionDriver);
        FunctionDriver = NULL;
    }

    //
    // Make sure the stack has added some drivers.
    //

    if (Device->DriverStackSize == OriginalStackSize) {
        Status = STATUS_NO_DRIVERS;
        IopSetDeviceProblem(Device, DeviceProblemNoDrivers, Status);
        goto AddDriversEnd;
    }

    Status = STATUS_SUCCESS;

AddDriversEnd:
    if (FunctionDriver != NULL) {
        IoDriverReleaseReference(FunctionDriver);
    }

    return Status;
}

PCSTR
IopFindDriverForDevice (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine attempts to find a functional driver for the given device.

Arguments:

    Device - Supplies a pointer to the device to match with a functional driver.

Return Value:

    Returns a pointer to the loaded driver on success, or NULL if no driver
    could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDEVICE_DATABASE_ENTRY DatabaseEntry;
    PCSTR Driver;
    BOOL Match;

    Driver = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Search through the database to match the device's ID to a driver.
    //

    KeAcquireQueuedLock(IoDeviceDatabaseLock);
    CurrentEntry = IoDeviceDatabaseHead.Next;
    while (CurrentEntry != &IoDeviceDatabaseHead) {
        DatabaseEntry = LIST_VALUE(CurrentEntry,
                                   DEVICE_DATABASE_ENTRY,
                                   ListEntry);

        CurrentEntry = CurrentEntry->Next;
        Match = IoAreDeviceIdsEqual(IoGetDeviceId(Device),
                                    DatabaseEntry->U.DeviceId);

        if (Match != FALSE) {
            Driver = DatabaseEntry->DriverName;
            goto FindDriverForDeviceEnd;
        }
    }

    //
    // Attempt to find a match with the device class.
    //

    if (Device->ClassId != NULL) {
        CurrentEntry = IoDeviceClassDatabaseHead.Next;
        while (CurrentEntry != &IoDeviceClassDatabaseHead) {
            DatabaseEntry = LIST_VALUE(CurrentEntry,
                                       DEVICE_DATABASE_ENTRY,
                                       ListEntry);

            CurrentEntry = CurrentEntry->Next;
            Match = RtlAreStringsEqual(DatabaseEntry->U.ClassId,
                                       Device->ClassId,
                                       MAX_DEVICE_ID);

            if (Match != FALSE) {
                Driver = DatabaseEntry->DriverName;
                goto FindDriverForDeviceEnd;
            }
        }
    }

FindDriverForDeviceEnd:
    KeReleaseQueuedLock(IoDeviceDatabaseLock);
    return Driver;
}

VOID
IopQueryChildren (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine queries the given device's children, processing any changes.

Arguments:

    Device - Supplies a pointer to the device whose children are to be queried.

Return Value:

    None.

--*/

{

    IRP_QUERY_CHILDREN QueryChildren;
    KSTATUS Status;

    RtlZeroMemory(&QueryChildren, sizeof(QueryChildren));
    Status = IopSendStateChangeIrp(Device,
                                   IrpMinorQueryChildren,
                                   &QueryChildren,
                                   sizeof(QueryChildren));

    if (!KSUCCESS(Status)) {
        IopSetDeviceProblem(Device, DeviceProblemFailedQueryChildren, Status);
        goto QueryChildrenEnd;
    }

    //
    // Process the children, then free the child list and destroy the
    // IRP.
    //

    IopProcessReportedChildren(Device, &QueryChildren);
    if (QueryChildren.Children != NULL) {
        MmFreePagedPool(QueryChildren.Children);
    }

    //
    // On success, if the device was awaiting enumeration, move it to the
    // started state and queue the start work item to finish any additional
    // initialization.
    //

    if (Device->State == DeviceAwaitingEnumeration) {
        IopSetDeviceState(Device, DeviceEnumerated);
        Status = IopQueueDeviceWork(Device, DeviceActionStart, NULL, 0);
        if (!KSUCCESS(Status)) {
            IopSetDeviceProblem(Device,
                                DeviceProblemFailedToQueueStart,
                                Status);
        }
    }

QueryChildrenEnd:
    return;
}

VOID
IopProcessReportedChildren (
    PDEVICE Device,
    PIRP_QUERY_CHILDREN Result
    )

/*++

Routine Description:

    This routine processes the list of children reported by a Query Children
    IRP. It will queue removals for any devices that were no longer reported
    and queue starts for any new devices.

Arguments:

    Device - Supplies a pointer to the device that just completed the
        enumeration.

    Result - Supplies a pointer to the query children data.

Return Value:

    None.

--*/

{

    PDEVICE *ChildArray;
    PDEVICE ChildDevice;
    ULONG ChildIndex;
    PLIST_ENTRY CurrentEntry;
    ULONG Flags;
    KSTATUS Status;

    //
    // Looping over a device's active children requires the device lock in
    // shared mode. Without locks, if a child were added during this routine
    // but it is not in the query IRP's list, it would immediately get marked
    // for removal. Without locks, if a child were deleted during this routine
    // then corruption could occur while looping over the children. This
    // requires a device to be removed from its parent's active child list
    // while the parent's lock is held exclusively.
    //

    KeAcquireSharedExclusiveLockShared(Device->Lock);

    //
    // Loop through all active children of this device and clear their
    // enumerated flag.
    //

    CurrentEntry = Device->ActiveChildListHead.Next;
    while (CurrentEntry != &(Device->ActiveChildListHead)) {
        ChildDevice = LIST_VALUE(CurrentEntry, DEVICE, ActiveListEntry);
        ChildDevice->Flags &= ~DEVICE_FLAG_ENUMERATED;
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Loop through the array of children the device returned looking for
    // brand new children.
    //

    ChildArray = Result->Children;
    for (ChildIndex = 0; ChildIndex < Result->ChildCount; ChildIndex += 1) {
        if ((ChildArray != NULL) && (ChildArray[ChildIndex] != NULL)) {

            //
            // Mark the child as enumerated so it does not get torn down later
            // during this routine. If the device appears to be previously
            // unreported, set the state to Initialized and queue work to
            // start the device.
            //

            ChildArray[ChildIndex]->Flags |= DEVICE_FLAG_ENUMERATED;
            if (ChildArray[ChildIndex]->State == DeviceUnreported) {
                IopSetDeviceState(ChildArray[ChildIndex], DeviceInitialized);
                Status = IopQueueDeviceWork(ChildArray[ChildIndex],
                                            DeviceActionStart,
                                            NULL,
                                            DEVICE_ACTION_OPEN_QUEUE);

                if (!KSUCCESS(Status)) {
                    IopSetDeviceProblem(ChildArray[ChildIndex],
                                        DeviceProblemFailedToQueueStart,
                                        Status);
                }
            }
        }
    }

    //
    // Loop through the active children again. If a device does not have the
    // enumerated flag, the bus didn't report it this time. Queue removals for
    // these devices.
    //

    CurrentEntry = Device->ActiveChildListHead.Next;
    while (CurrentEntry != &(Device->ActiveChildListHead)) {
        ChildDevice = LIST_VALUE(CurrentEntry, DEVICE, ActiveListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((ChildDevice->Flags & DEVICE_FLAG_ENUMERATED) == 0) {
            Flags = DEVICE_ACTION_SEND_TO_SUBTREE | DEVICE_ACTION_OPEN_QUEUE;
            Status = IopQueueDeviceWork(ChildDevice,
                                        DeviceActionPrepareRemove,
                                        NULL,
                                        Flags);

            //
            // If the action failed to queue for a reason other than that the
            // device was already awaiting removal, set the problem state. Do
            // not call the queue failure handler as that can roll back the
            // parent's device state, but in this case the parent isn't
            // expecting an answer from the child's removal process.
            //

            if (!KSUCCESS(Status) &&
                (Status != STATUS_DEVICE_QUEUE_CLOSING)) {

                IopSetDeviceProblem(ChildDevice,
                                    DeviceProblemFailedToQueuePrepareRemove,
                                    Status);

                break;
            }
        }
    }

    KeReleaseSharedExclusiveLockShared(Device->Lock);
    return;
}

PSTR
IopGetUniqueDeviceId (
    PDEVICE ParentDevice,
    PCSTR DeviceId
    )

/*++

Routine Description:

    This routine converts the given device ID into a device ID that is unique
    amongst the children of the given parent device.

Arguments:

    ParentDevice - Supplies a pointer to the parent of the device whose ID is
        supplied.

    DeviceId - Supplies the device ID that needs to be made unique.

Return Value:

    Returns a unique device ID string. If the result is different than
    DeviceId, the caller is responsible for releasing the memory.

--*/

{

    ULONG BytesCopied;
    ULONG DeviceIdLength;
    ULONG DeviceIndex;
    POBJECT_HEADER ExistingDevice;
    PSTR FormatString;
    ULONG FormatStringLength;
    PSTR NewDeviceId;
    ULONG NewDeviceIdLength;

    //
    // If there is no parent device or the parent device is the volume
    // directory, then just return the device ID.
    //

    if ((ParentDevice == NULL) ||
        (ParentDevice == (PDEVICE)IoVolumeDirectory)) {

        return (PSTR)DeviceId;
    }

    //
    // If this is the first time this device ID has been used, then just use
    // the given device ID.
    //

    DeviceIdLength = RtlStringLength(DeviceId) + 1;
    ExistingDevice = ObFindObject(DeviceId,
                                  DeviceIdLength,
                                  (POBJECT_HEADER)ParentDevice);

    if (ExistingDevice == NULL) {
        return (PSTR)DeviceId;
    }

    ObReleaseReference(ExistingDevice);
    ExistingDevice = NULL;

    //
    // Otherwise, append a unique value to the device ID. Start by creating a
    // format string. The format string is the original device ID appended with
    // "#%d", unless the current device ID already ends with a '#' character.
    //

    if (DeviceId[DeviceIdLength - 2] == DEVICE_ID_SUFFIX_START_CHARACTER) {
        FormatStringLength = DeviceIdLength +
                             DEVICE_ID_SUFFIX_ALTERNATE_FORMAT_LENGTH -
                             1;

    } else {
        FormatStringLength = DeviceIdLength +
                             DEVICE_ID_SUFFIX_FORMAT_LENGTH -
                             1;
    }

    NewDeviceId = NULL;
    FormatString = MmAllocatePagedPool(FormatStringLength,
                                       DEVICE_ALLOCATION_TAG);

    if (FormatString == NULL) {
        goto GetUniqueDeviceIdEnd;
    }

    BytesCopied = RtlStringCopy(FormatString, DeviceId, FormatStringLength);

    ASSERT(BytesCopied == DeviceIdLength);

    //
    // Do not copy a second '#' character to start the suffix if the device ID
    // already ends in one.
    //

    if (DeviceId[DeviceIdLength - 2] == DEVICE_ID_SUFFIX_START_CHARACTER) {
        BytesCopied = RtlStringCopy(FormatString + (DeviceIdLength - 1),
                                    DEVICE_ID_SUFFIX_ALTERNATE_FORMAT,
                                    DEVICE_ID_SUFFIX_ALTERNATE_FORMAT_LENGTH);

        ASSERT(BytesCopied == DEVICE_ID_SUFFIX_ALTERNATE_FORMAT_LENGTH);

    } else {
        BytesCopied = RtlStringCopy(FormatString + (DeviceIdLength - 1),
                                    DEVICE_ID_SUFFIX_FORMAT,
                                    DEVICE_ID_SUFFIX_FORMAT_LENGTH);

        ASSERT(BytesCopied == DEVICE_ID_SUFFIX_FORMAT_LENGTH);
    }

    //
    // Allocate a buffer to use for creating the new device ID string.
    //

    DeviceIdLength += DEVICE_ID_SUFFIX_LENGTH_MAX;
    NewDeviceId = MmAllocatePagedPool(DeviceIdLength, DEVICE_ALLOCATION_TAG);
    if (NewDeviceId == NULL) {
        goto GetUniqueDeviceIdEnd;
    }

    //
    // Create the possible device IDs and compare them to existing device IDs
    // amongst the parent device's children. Use the first available.
    //

    for (DeviceIndex = 1;
         DeviceIndex < MAX_CONFLICTING_DEVICES;
         DeviceIndex += 1) {

        NewDeviceIdLength = RtlPrintToString(NewDeviceId,
                                             DeviceIdLength,
                                             CharacterEncodingDefault,
                                             FormatString,
                                             DeviceIndex);

        if (NewDeviceIdLength > DeviceIdLength) {
            NewDeviceIdLength = DeviceIdLength;
        }

        ExistingDevice = ObFindObject(NewDeviceId,
                                      NewDeviceIdLength,
                                      (POBJECT_HEADER)ParentDevice);

        if (ExistingDevice == NULL) {
            goto GetUniqueDeviceIdEnd;
        }

        ObReleaseReference(ExistingDevice);
        ExistingDevice = NULL;
    }

    //
    // This device has too many children with the same device ID. Give up.
    //

    MmFreePagedPool(NewDeviceId);
    NewDeviceId = NULL;

GetUniqueDeviceIdEnd:
    if (FormatString != NULL) {
        MmFreePagedPool(FormatString);
    }

    return NewDeviceId;
}

