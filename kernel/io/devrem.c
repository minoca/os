/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    devrem.c

Abstract:

    This module implements device removal functionality.

Author:

    Chris Stevens 17-Jun-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"
#include "pmp.h"
#include "pagecach.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopSendRemovalIrp (
    PDEVICE Device
    );

VOID
IopDeviceDestroyDriverStack (
    PDEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
IopPrepareRemoveDevice (
    PDEVICE Device,
    PDEVICE_WORK_ENTRY Work
    )

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

{

    BOOL QueueRemoval;
    KSTATUS Status;
    PVOLUME Volume;

    ASSERT(Work->Action == DeviceActionPrepareRemove);
    ASSERT((Work->Flags & DEVICE_ACTION_SEND_TO_SUBTREE) != 0);

    //
    // This device should not already be marked removed. If it was removed,
    // then no additional work items should have been scheduled on its queue.
    //

    ASSERT(Device->State != DeviceRemoved);

    //
    // If the device is already awaiting removal, that means that some process
    // already scheduled this prepare removal work item on this device. It
    // should get signaled for removal by its children. In this case, this
    // work entry no longer needs to traverse this device's subtree.
    //

    if (Device->State == DeviceAwaitingRemoval) {
        Work->Flags &= ~DEVICE_ACTION_SEND_TO_SUBTREE;
        goto PrepareRemoveDeviceEnd;
    }

    //
    // Acquire the lock here to synchronize with child device creation. By the
    // time this lock is acquired, any concurrently active device creations
    // are finished and any future attempts at creation should fail because the
    // device is now awaiting removal. This also synchronizes with removal so
    // a device does not get two removal work items queued - one by this
    // routine and another when the last child gets removed.
    //

    KeAcquireSharedExclusiveLockExclusive(Device->Lock);

    //
    // The state check above is safe because a device's queue items are
    // processed sequentially and only this work item moves the state to
    // awaiting removal. Assert this though for good measure.
    //

    ASSERT(Device->State != DeviceAwaitingRemoval);

    //
    // Mark the device as awaiting removal.
    //

    IopSetDeviceState(Device, DeviceAwaitingRemoval);

    //
    // Unsignal the device so that anyone waiting on it will have to let the
    // removal finish.
    //

    ObSignalObject(Device, SignalOptionUnsignal);

    //
    // If this is a volume, make sure that it is marked that it is in the
    // process of being unmounted.
    //

    if (Device->Header.Type == ObjectVolume) {
        Volume = (PVOLUME)Device;
        Volume->Flags |= VOLUME_FLAG_UNMOUNTING;
    }

    //
    // Queue removal on the device if it has no active children.
    //

    QueueRemoval = FALSE;
    if (LIST_EMPTY(&(Device->ActiveChildListHead)) != FALSE) {
        QueueRemoval = TRUE;
    }

    KeReleaseSharedExclusiveLockExclusive(Device->Lock);

    //
    // Queue the removal work item on this device if necessary. If this fails,
    // handle the queue failure, which will roll back the state of any device
    // waiting on this device's removal process.
    //
    // N.B. There could be another prepare to remove work item in this device's
    //      queue that could end up succeeding removal, making the rollback
    //      unnecessary. Don't bother to check, however. A parent that gets
    //      rolled back can attempt removal again.
    //

    if (QueueRemoval != FALSE) {
        Status = IopQueueDeviceWork(Device,
                                    DeviceActionRemove,
                                    NULL,
                                    DEVICE_ACTION_CLOSE_QUEUE);

        if (!KSUCCESS(Status)) {
            IopHandleDeviceQueueFailure(Device, DeviceActionRemove);
        }
    }

PrepareRemoveDeviceEnd:
    return;
}

VOID
IopRemoveDevice (
    PDEVICE Device,
    PDEVICE_WORK_ENTRY Work
    )

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

{

    PDEVICE ParentDevice;
    BOOL RemoveParent;
    KSTATUS Status;

    ASSERT(Device->State == DeviceAwaitingRemoval);
    ASSERT((Work->Flags & DEVICE_ACTION_CLOSE_QUEUE) != 0);
    ASSERT(Device->QueueState == DeviceQueueActiveClosing);
    ASSERT(LIST_EMPTY(&(Device->ActiveChildListHead)) != FALSE);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Attempt to remove any device paths that belong to the device. Do this
    // before the remove IRP in case something fails. It will be done again
    // after the remove IRP, ingorning failures.
    //

    Status = IopRemoveDevicePaths(Device);
    if (!KSUCCESS(Status)) {
        IopAbortDeviceRemoval(Device, DeviceProblemFailedPathRemoval, TRUE);
        goto RemoveDeviceEnd;
    }

    //
    // Acquire the device lock exclusive for the remove IRP to synchronize with
    // I/O opens, system control IRPs, and user control IRPs.
    //

    KeAcquireSharedExclusiveLockExclusive(Device->Lock);

    //
    // Clean up the power management state.
    //

    PmpRemoveDevice(Device);

    //
    // Send the removal IRP to the device. If this fails, the removal process
    // must be rolled back for this branch of the device tree.
    //

    Status = IopSendRemovalIrp(Device);
    if (!KSUCCESS(Status)) {
        KeReleaseSharedExclusiveLockExclusive(Device->Lock);
        IopAbortDeviceRemoval(Device, DeviceProblemFailedToSendRemoveIrp, TRUE);
        goto RemoveDeviceEnd;
    }

    //
    // With the removal IRP sent, rolling back should not happen anymore. Thus,
    // without further ado, declare this device removed.
    //

    IopSetDeviceState(Device, DeviceRemoved);
    ObSignalObject(Device, SignalOptionSignalAll);

    //
    // Remove the device from the global list so that racy folks trying to
    // look up the device by device ID must finish now. It would be bad to get
    // all the way down to releasing the last reference only to have that
    // lookup function try to re-add one.
    //

    KeAcquireQueuedLock(IoDeviceListLock);
    LIST_REMOVE(&(Device->ListEntry));
    Device->ListEntry.Next = NULL;
    KeReleaseQueuedLock(IoDeviceListLock);

    //
    // Officially close the work queue.
    //

    KeAcquireQueuedLock(Device->QueueLock);
    Device->QueueState = DeviceQueueClosed;
    KeReleaseQueuedLock(Device->QueueLock);

    //
    // Release the device lock to let everyone else waiting on the state see
    // that it has now switched to removed.
    //

    KeReleaseSharedExclusiveLockExclusive(Device->Lock);

    //
    // Clean up the device paths again, ignoring failures this time.
    //

    IopRemoveDevicePaths(Device);

    //
    // Evict any lingering file object entries in the page cache. Clean removal
    // should have flushed block devices by this point.
    //

    IopEvictFileObjects(Device->DeviceId, EVICTION_FLAG_REMOVE);

    //
    // Flush the file objects for the device. Eviction should have removed all
    // the page cache entries, but the file object properties may be dirty and
    // keeping the file objects for this device in the dirty file objects list.
    // The writes will fail, but the flush should do the job to moving them all
    // out of the list.
    //

    IopFlushFileObjects(
                    Device->DeviceId,
                    IO_FLAG_DATA_SYNCHRONIZED | IO_FLAG_METADATA_SYNCHRONIZED,
                    NULL);

    //
    // Release any lingering file objects that may be stuck open for this
    // device. Be nice, and do it for other devices as well.
    //

    IopCleanupFileObjects();

    //
    // Determine which device to consider the parent. A volume's effective
    // "parent" is the target device.
    //

    if (Device->Header.Type == ObjectVolume) {
        ParentDevice = Device->TargetDevice;

    } else {
        ParentDevice = Device->ParentDevice;
    }

    ASSERT(ParentDevice->Header.ReferenceCount >= 2);

    //
    // Acquire the parent's device lock exclusively to free its active child
    // list and its state. This needs to happen under the lock to synchronize
    // with the parent's own prepare remove work item which can schedule the
    // remove work item on a device with no children.
    //

    KeAcquireSharedExclusiveLockExclusive(ParentDevice->Lock);

    //
    // With the device officially removed, remove it from its parent's list of
    // active children.
    //

    LIST_REMOVE(&(Device->ActiveListEntry));
    Device->ActiveListEntry.Next = NULL;

    //
    // Handle the special case where the device is a volume and it's "parent"
    // is its target device.
    //

    if (Device->Header.Type == ObjectVolume) {

        //
        // If the parent has no more children, then nothing is mounted.
        //

        if (LIST_EMPTY(&(ParentDevice->ActiveChildListHead)) != FALSE) {
            ParentDevice->Flags &= ~DEVICE_FLAG_MOUNTED;
        }
    }

    //
    // If the parent device is awaiting removal, determine if the given device
    // is its last active child.
    //

    RemoveParent = FALSE;
    if ((ParentDevice->State == DeviceAwaitingRemoval) &&
        (LIST_EMPTY(&(ParentDevice->ActiveChildListHead)) != FALSE)) {

        ObAddReference(ParentDevice);
        RemoveParent = TRUE;
    }

    KeReleaseSharedExclusiveLockExclusive(ParentDevice->Lock);

    //
    // Release the initial volume reference.
    //

    if (Device->Header.Type == ObjectVolume) {
        IoVolumeReleaseReference((PVOLUME)Device);
    }

    //
    // Release the reference taken by the object manager. This is not
    // necessarily the device's last reference.
    //

    ObReleaseReference(Device);

    //
    // Queue the removal of the parent if it has no more children.
    //

    if (RemoveParent != FALSE) {
        Status = IopQueueDeviceWork(ParentDevice,
                                    DeviceActionRemove,
                                    NULL,
                                    DEVICE_ACTION_CLOSE_QUEUE);

        if (!KSUCCESS(Status)) {
            IopHandleDeviceQueueFailure(ParentDevice, DeviceActionRemove);
        }

        ObReleaseReference(ParentDevice);
    }

RemoveDeviceEnd:
    return;
}

VOID
IopAbortDeviceRemoval (
    PDEVICE Device,
    DEVICE_PROBLEM DeviceProblem,
    BOOL RollbackDevice
    )

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

{

    PDEVICE CurrentDevice;
    PDEVICE ParentDevice;
    DEVICE_STATE PreviousState;
    ULONG PreviousStateIndex;
    PVOLUME Volume;

    ASSERT(Device->Header.ReferenceCount >= 1);

    //
    // This routine could be called when the given device is not marked for
    // removal. In this case, just start with the parent device. In the case of
    // volumes, the "parent" device is the target device.
    //

    if (RollbackDevice == FALSE) {

        ASSERT(Device->State != DeviceAwaitingRemoval);

        if (Device->Header.Type == ObjectVolume) {

            ASSERT(Device->TargetDevice != NULL);

            CurrentDevice = Device->TargetDevice;

        } else {
            CurrentDevice = Device->ParentDevice;
        }

    } else {

        ASSERT(Device->State == DeviceAwaitingRemoval);

        CurrentDevice = Device;
    }

    //
    // Look back up the device tree reverting all the device's ancestors out of
    // the awaiting removal state. Since the caller must have a reference on
    // the supplied device, this routine does not need to worry about devices
    // disappearing; every device holds a reference to its parent (including
    // volumes and their target device).
    //

    KeAcquireSharedExclusiveLockExclusive(CurrentDevice->Lock);
    while (CurrentDevice->State == DeviceAwaitingRemoval) {

        ASSERT(CurrentDevice->Header.ReferenceCount >= 1);

        PreviousStateIndex = CurrentDevice->StateHistoryNextIndex - 1;
        if (PreviousStateIndex == MAX_ULONG) {
            PreviousStateIndex = DEVICE_STATE_HISTORY - 1;
        }

        PreviousState = CurrentDevice->StateHistory[PreviousStateIndex];
        IopSetDeviceState(CurrentDevice, PreviousState);

        //
        // Modify the device's queue back to the correct state. This depends on
        // the current queue state and the previous device state.
        //

        KeAcquireQueuedLock(Device->QueueLock);

        //
        // Devices with closed queues should never need to be rolled back.
        //

        ASSERT(CurrentDevice->QueueState != DeviceQueueClosed);

        //
        // The only queue state that needs rolling back is the active closing
        // state. All other device removal aborts come from failing to queue an
        // action, which already rolls back the queue state correctly.
        //

        if (CurrentDevice->QueueState == DeviceQueueActiveClosing) {

            ASSERT(LIST_EMPTY(&(CurrentDevice->WorkQueue)) != FALSE);

            //
            // If the previous state was unreported, then the queue should be
            // marked closed. Otherwise, it is open.
            //

            if (PreviousState == DeviceUnreported) {
                CurrentDevice->QueueState = DeviceQueueClosed;

            } else {
                CurrentDevice->QueueState = DeviceQueueOpen;
            }
        }

        KeReleaseQueuedLock(Device->QueueLock);

        //
        // Signal anyone waiting on this device's removal state. It will no
        // longer reach that signal.
        //

        ObSignalObject(CurrentDevice, SignalOptionSignalAll);

        //
        // Move backwards up the tree. For a volume, the effective parent is
        // the target device.
        //

        if (CurrentDevice->Header.Type == ObjectVolume) {
            Volume = (PVOLUME)CurrentDevice;

            //
            // Also make sure that the volume is no longer marked as
            // "unmounting".
            //

            Volume->Flags &= ~VOLUME_FLAG_UNMOUNTING;

            ASSERT(Device->TargetDevice != NULL);

            ParentDevice = Device->TargetDevice;

        } else {
            ParentDevice = CurrentDevice->ParentDevice;
        }

        //
        // Release the current device's lock before gettings the parent's lock.
        //

        KeReleaseSharedExclusiveLockExclusive(CurrentDevice->Lock);

        //
        // Move up to the parent device and acquire its lock.
        //

        CurrentDevice = ParentDevice;
        KeAcquireSharedExclusiveLockExclusive(CurrentDevice->Lock);
    }

    KeReleaseSharedExclusiveLockExclusive(CurrentDevice->Lock);

    //
    // Set the device problem state on the orignal device to  record that this
    // device is the origin of the removal failure.
    //

    IopSetDeviceProblem(Device, DeviceProblem, STATUS_UNSUCCESSFUL);
    return;
}

VOID
IopDestroyDevice (
    PVOID Object
    )

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

{

    PDEVICE Device;

    Device = (PDEVICE)Object;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Device != NULL);
    ASSERT((Device->State == DeviceRemoved) ||
           (Device->State == DeviceUnreported));

    //
    // Remove the device from the global list if not already done.
    //

    if (Device->ListEntry.Next != NULL) {

        ASSERT(Device->State == DeviceUnreported);

        KeAcquireQueuedLock(IoDeviceListLock);
        LIST_REMOVE(&(Device->ListEntry));
        Device->ListEntry.Next = NULL;
        KeReleaseQueuedLock(IoDeviceListLock);
    }

    //
    // If there's a target device, release the reference on it.
    //

    if (Device->TargetDevice != NULL) {
        ObReleaseReference(Device->TargetDevice);
    }

    //
    // The device's work queue should be empty.
    //

    ASSERT(LIST_EMPTY(&(Device->WorkQueue)) != FALSE);

    //
    // Detached the drivers from the device.
    //

    IopDeviceDestroyDriverStack(Device);

    //
    // Assert all the children are gone, there are no active children, and this
    // device is not an active child to anyone.
    //

    ASSERT(LIST_EMPTY(&(Device->Header.ChildListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Device->ActiveChildListHead)) != FALSE);
    ASSERT(Device->ActiveListEntry.Next == NULL);

    //
    // Clean up the power management state.
    //

    PmpDestroyDevice(Device);

    //
    // Delete the arbiter list and the various resource lists.
    //

    IopDestroyArbiterList(Device);
    if (Device->ResourceRequirements != NULL) {
        IoDestroyResourceConfigurationList(Device->ResourceRequirements);
    }

    if (Device->SelectedConfiguration != NULL) {
        IoDestroyResourceRequirementList(Device->SelectedConfiguration);
    }

    if (Device->BusLocalResources != NULL) {
        IoDestroyResourceAllocationList(Device->BusLocalResources);
    }

    if (Device->ProcessorLocalResources != NULL) {
        IoDestroyResourceAllocationList(Device->ProcessorLocalResources);
    }

    if (Device->BootResources != NULL) {
        IoDestroyResourceAllocationList(Device->BootResources);
    }

    if (Device->Lock != NULL) {
        KeDestroySharedExclusiveLock(Device->Lock);
    }

    //
    // Deallocate the class ID, and compatible IDs. The object manager will
    // free the device ID (i.e. the name).
    //

    ASSERT((Device->Header.Flags & OBJECT_FLAG_USE_NAME_DIRECTLY) == 0);

    if (Device->ClassId != NULL) {
        MmFreePagedPool(Device->ClassId);

    } else if (Device->CompatibleIds != NULL) {
        MmFreePagedPool(Device->CompatibleIds);
    }

    RtlDebugPrint("Destroyed Device: %s, 0x%x\n", Device->Header.Name, Device);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopSendRemovalIrp (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine sends a removal IRP to a device, allowing device drivers to
    clean up any resources for the given device.

Arguments:

    Device - Supplies a pointer to the device that is to receive a removal IRP.

Return Value:

    Status code.

--*/

{

    PIRP RemovalIrp;
    KSTATUS Status;

    RemovalIrp = NULL;
    Status = STATUS_SUCCESS;

    //
    // The system should only send removal IRPs to devices awaiting removal.
    //

    ASSERT(Device->State == DeviceAwaitingRemoval);

    //
    // The device should have no active children.
    //

    ASSERT(LIST_EMPTY(&(Device->ActiveChildListHead)) != FALSE);

    //
    // The device's work queue should be closing and the work queue should be
    // empty.
    //

    ASSERT(Device->QueueState == DeviceQueueActiveClosing);
    ASSERT(LIST_EMPTY(&(Device->WorkQueue)) != FALSE);

    //
    // Skip to the end if there are no drivers.
    //

    if (Device->DriverStackSize == 0) {

        ASSERT(LIST_EMPTY(&(Device->DriverStackHead)) != FALSE);

       goto SendRemovalIrpEnd;
    }

    //
    // Allocate a removal IRP.
    //

    RemovalIrp = IoCreateIrp(Device, IrpMajorStateChange, 0);
    if (RemovalIrp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendRemovalIrpEnd;
    }

    RemovalIrp->MinorCode = IrpMinorRemoveDevice;

    //
    // Send the removal IRP to the device. Release the topology lock while
    // calling the driver.
    //

    Status = IoSendSynchronousIrp(RemovalIrp);
    if (!KSUCCESS(Status)) {

        ASSERT(KSUCCESS(Status));

        goto SendRemovalIrpEnd;
    }

    Status = IoGetIrpStatus(RemovalIrp);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("IO: Remove IRP failed for device 0x%08x with status "
                      "%d\n",
                      Device,
                      Status);

        goto SendRemovalIrpEnd;
    }

SendRemovalIrpEnd:
    if (RemovalIrp != NULL) {
        IoDestroyIrp(RemovalIrp);
    }

    return Status;
}

VOID
IopDeviceDestroyDriverStack (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine destroys the driver stack for the given device.

Arguments:

    Device - Supplies a pointer to the device whose driver stack is to be
        destroyed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY DriverListEntry;
    PDRIVER_STACK_ENTRY DriverStackEntry;
    PLIST_ENTRY RemovalEntry;

    ASSERT(Device != NULL);
    ASSERT((Device->State == DeviceRemoved) ||
           (Device->State == DeviceUnreported));

    //
    // Detached the drivers from the device.
    //

    DriverListEntry = Device->DriverStackHead.Next;
    while (DriverListEntry != &(Device->DriverStackHead)) {
        DriverStackEntry = (PDRIVER_STACK_ENTRY)LIST_VALUE(DriverListEntry,
                                                           DRIVER_STACK_ENTRY,
                                                           ListEntry);

        RemovalEntry = DriverListEntry;
        DriverListEntry = DriverListEntry->Next;
        LIST_REMOVE(RemovalEntry);
        IoDriverReleaseReference(DriverStackEntry->Driver);
        MmFreeNonPagedPool(DriverStackEntry);
        Device->DriverStackSize -= 1;
    }

    ASSERT(Device->DriverStackSize == 0);

    return;
}

