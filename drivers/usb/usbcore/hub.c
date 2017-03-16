/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hub.c

Abstract:

    This module implements support for interacting with standard USB Hubs.

Author:

    Evan Green 19-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "usbcore.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores internal USB hub context.

Members:

    DeviceHandle - Stores a pointer to the USB device handle.

    IoBuffer - Stores a pointer to the I/O buffer used to back the hub data
        transfers.

    ControlTransfer - Stores a pointer to the control transfer used to
        communicate with the hub.

    ControlTransferLock - Stores a pointer to the lock that synchronizes
        access to the control transfer.

    InterruptTransfer - Stores a pointer to the interrupt transfer used for
        hub status notifications.

    PortCount - Stores the number of downstream ports in the hub.

    PowerDelayIn2ms - Stores the time, in 2ms intervals, from the time the
        power-on sequence begins on a port until the power is good on that
        port. Software uses this value to determine how long to wait before
        accessing a powered-on port.

    HasIndicators - Stores a boolean indicating whether or not the hub has
        port indicator LEDs.

    HubStatus - Stores the status of each of the hub's ports.

    Interface - Stores a pointer to the hub interface description.

    InterruptWorkItem - Stores a pointer to the work item queued when the
        interrupt transfer completes.

    ChangedPorts - Stores the result of the interrupt transfer, the bitfield
        of changed ports.

--*/

struct _USB_HUB {
    HANDLE DeviceHandle;
    PIO_BUFFER IoBuffer;
    PUSB_TRANSFER ControlTransfer;
    PQUEUED_LOCK ControlTransferLock;
    PUSB_TRANSFER InterruptTransfer;
    UCHAR PortCount;
    UCHAR PowerUpDelayIn2ms;
    BOOL HasIndicators;
    USB_HUB_STATUS HubStatus;
    PUSB_INTERFACE_DESCRIPTION Interface;
    PWORK_ITEM InterruptWorkItem;
    USHORT ChangedPorts;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UsbpGetHubStatus (
    PUSB_HUB Hub,
    BOOL ForceRefresh
    );

KSTATUS
UsbpGetRootHubStatus (
    PUSB_HUB RootHub
    );

KSTATUS
UsbpSetHubStatus (
    PUSB_HUB Hub
    );

VOID
UsbpHubUpdatePortStatus (
    PUSB_HUB Hub,
    ULONG PortIndex,
    ULONG HardwareStatus
    );

KSTATUS
UsbpResetHub (
    PUSB_HUB Hub
    );

KSTATUS
UsbpReadHubDescriptor (
    PUSB_HUB Hub
    );

KSTATUS
UsbpHubSendControlTransfer (
    PUSB_HUB Hub,
    PULONG LengthTransferred
    );

KSTATUS
UsbpHubGetHubStatus (
    PUSB_HUB Hub,
    PULONG HubStatus
    );

KSTATUS
UsbpHubGetPortStatus (
    PUSB_HUB Hub,
    ULONG PortNumber,
    PULONG PortStatus
    );

KSTATUS
UsbpHubSetOrClearFeature (
    PUSB_HUB Hub,
    BOOL SetFeature,
    USHORT Feature,
    USHORT Port
    );

VOID
UsbpHubInterruptTransferCompletion (
    PUSB_TRANSFER Transfer
    );

VOID
UsbpHubInterruptTransferCompletionWorker (
    PVOID Parameter
    );

KSTATUS
UsbpHubClearPortChangeBits (
    PUSB_HUB Hub,
    ULONG PortNumber,
    ULONG PortStatus
    );

VOID
UsbpHubAddDevice (
    PUSB_HUB Hub,
    ULONG PortIndex
    );

KSTATUS
UsbpHubEnablePortPower (
    PUSB_HUB Hub,
    ULONG PortIndex
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

USB_API
KSTATUS
UsbCreateHub (
    HANDLE DeviceHandle,
    PUSB_HUB *Hub
    )

/*++

Routine Description:

    This routine creates a new USB hub device. This routine must be called at
    low level.

Arguments:

    DeviceHandle - Supplies the open device handle to the hub.

    Hub - Supplies a pointer where a pointer to the hub context will be
        returned.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    UINTN BufferAlignment;
    UINTN BufferSize;
    PUSB_DEVICE Device;
    PVOID HubStatus;
    ULONG IoBufferFlags;
    ULONG MaxControlTransferSize;
    ULONG MaxInterruptSize;
    PUSB_HUB NewHub;
    KSTATUS Status;

    ASSERT(Hub != NULL);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    HubStatus = NULL;
    NewHub = MmAllocatePagedPool(sizeof(USB_HUB), USB_CORE_ALLOCATION_TAG);
    if (NewHub == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateHubEnd;
    }

    RtlZeroMemory(NewHub, sizeof(USB_HUB));
    NewHub->DeviceHandle = DeviceHandle;

    //
    // Create an I/O buffer for both control and interrupt transfers. Since the
    // I/O buffer allocation rounds up to a page anyway, this allocation
    // accounts for the maximum possible number of ports on a hub: 127.
    //

    BufferAlignment = MmGetIoBufferAlignment();
    MaxControlTransferSize = ALIGN_RANGE_UP(USB_HUB_MAX_CONTROL_TRANSFER_SIZE,
                                            BufferAlignment);

    MaxInterruptSize = ALIGN_RANGE_UP(USB_HUB_MAX_INTERRUPT_SIZE,
                                      BufferAlignment);

    BufferSize = MaxControlTransferSize + MaxInterruptSize;
    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    NewHub->IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                  MAX_ULONG,
                                                  BufferAlignment,
                                                  BufferSize,
                                                  IoBufferFlags);

    if (NewHub->IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateHubEnd;
    }

    ASSERT(NewHub->IoBuffer->FragmentCount == 1);

    //
    // Create a control transfer.
    //

    NewHub->ControlTransfer = UsbAllocateTransfer(
                                            NewHub->DeviceHandle,
                                            0,
                                            USB_HUB_MAX_CONTROL_TRANSFER_SIZE,
                                            0);

    if (NewHub->ControlTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateHubEnd;
    }

    NewHub->ControlTransfer->Buffer =
                                  NewHub->IoBuffer->Fragment[0].VirtualAddress;

    NewHub->ControlTransfer->BufferPhysicalAddress =
                                 NewHub->IoBuffer->Fragment[0].PhysicalAddress;

    NewHub->ControlTransfer->BufferActualLength = MaxControlTransferSize;
    NewHub->ControlTransferLock = KeCreateQueuedLock();
    if (NewHub->ControlTransferLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateHubEnd;
    }

    //
    // Create the interrupt work item.
    //

    NewHub->InterruptWorkItem = KeCreateWorkItem(
                                      UsbCoreWorkQueue,
                                      WorkPriorityNormal,
                                      UsbpHubInterruptTransferCompletionWorker,
                                      NewHub,
                                      USB_CORE_ALLOCATION_TAG);

    if (NewHub->InterruptWorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateHubEnd;
    }

    //
    // Get the number of ports for this hub and finish creating the hub's port
    // count dependent structures.
    //

    Device = (PUSB_DEVICE)DeviceHandle;
    if (Device->Type == UsbDeviceTypeRootHub) {
        NewHub->PortCount = Device->Controller->Device.RootHubPortCount;

    } else {
        Status = UsbpReadHubDescriptor(NewHub);
        if (!KSUCCESS(Status)) {
            goto CreateHubEnd;
        }
    }

    //
    // Allocate space for the hub status arrays.
    //

    AllocationSize = (sizeof(USB_PORT_STATUS) * NewHub->PortCount) +
                     (sizeof(USB_DEVICE_SPEED) * NewHub->PortCount);

    HubStatus = MmAllocatePagedPool(AllocationSize, USB_CORE_ALLOCATION_TAG);
    if (HubStatus == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateHubEnd;
    }

    RtlZeroMemory(HubStatus, AllocationSize);
    NewHub->HubStatus.PortStatus = HubStatus;
    HubStatus += (sizeof(USB_PORT_STATUS) * NewHub->PortCount);
    NewHub->HubStatus.PortDeviceSpeed = HubStatus;

    //
    // If this is the root hub, link it up with the host controller.
    //

    if (Device->Type == UsbDeviceTypeRootHub) {
        Device->Controller->RootHub = NewHub;
    }

    Status = STATUS_SUCCESS;

CreateHubEnd:
    if (!KSUCCESS(Status)) {
        if (NewHub != NULL) {
            if (NewHub->InterruptWorkItem != NULL) {
                KeDestroyWorkItem(NewHub->InterruptWorkItem);
            }

            if (NewHub->ControlTransferLock != NULL) {
                KeDestroyQueuedLock(NewHub->ControlTransferLock);
            }

            if (NewHub->ControlTransfer != NULL) {
                UsbDestroyTransfer(NewHub->ControlTransfer);
            }

            if (NewHub->IoBuffer != NULL) {
                MmFreeIoBuffer(NewHub->IoBuffer);
            }

            if (HubStatus != NULL) {
                MmFreePagedPool(HubStatus);
            }

            MmFreePagedPool(NewHub);
            NewHub = NULL;
        }
    }

    *Hub = NewHub;
    return Status;
}

USB_API
VOID
UsbDestroyHub (
    PUSB_HUB Hub
    )

/*++

Routine Description:

    This routine destroys a USB hub context. This should only be called once
    all of the hub's transfers have completed.

Arguments:

    Hub - Supplies a pointer to the hub to tear down.

Return Value:

    None.

--*/

{

    PUSB_DEVICE Child;
    PUSB_DEVICE HubDevice;

    ASSERT(Hub != NULL);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Get the hub's USB device. It should be disconnected.
    //

    HubDevice = (PUSB_DEVICE)Hub->DeviceHandle;

    ASSERT(HubDevice->Connected == FALSE);

    //
    // Clean up the hub's children, who should all be disconnected. The list
    // will be empty after this.
    //

    KeAcquireQueuedLock(HubDevice->ChildLock);
    while (LIST_EMPTY(&(HubDevice->ChildList)) == FALSE) {
        Child = (PUSB_DEVICE)LIST_VALUE(HubDevice->ChildList.Next,
                                        USB_DEVICE,
                                        ListEntry);

        //
        // Assert that the child is disconnected or that there is only one
        // reference on the child. It is not enough to assert that it is
        // disconnected, because devices whose functional driver never came
        // online will not get the remove IRP to disconnect themselves.
        // They may still be "connected". Devices that were connected and are
        // now disconnected may have more than 1 reference if something in the
        // system still holds a handle to the device.
        //

        ASSERT((Child->Connected == FALSE) || (Child->ReferenceCount == 1));

        UsbpRemoveDevice(Child);
    }

    KeReleaseQueuedLock(HubDevice->ChildLock);

    //
    // The hub's interrupt transfer callback queues a work item, which then
    // attempts to re-submit the transfer. Re-submission will fail at this
    // point, so flush the work item, destroy it, and then the transfer can
    // be safely destroyed. The work item is guaranteed to have been queued
    // because the transfer is currently in the inactive state, not in the
    // callback state.
    //
    // The destroy routine attempts to cancel the work item and then flush if
    // the cancel was too late.
    //

    KeDestroyWorkItem(Hub->InterruptWorkItem);

    //
    // There is no guarantee the interrupt transfer was allocated in cases
    // where the hub never got the start IRP. Only release what is necessary.
    //

    if (Hub->InterruptTransfer != NULL) {
        UsbDestroyTransfer(Hub->InterruptTransfer);
    }

    //
    // Release the interface used for the transfer, if necessary.
    //

    if (Hub->Interface != NULL) {
        UsbReleaseInterface(Hub->DeviceHandle,
                            Hub->Interface->Descriptor.InterfaceNumber);
    }

    //
    // The control transfer is only used during start, query children, and
    // interrupt callback operations. Given that the interrupt transfer has
    // been destroyed, it is safe to destroy the control transfer and lock.
    //

    UsbDestroyTransfer(Hub->ControlTransfer);
    KeDestroyQueuedLock(Hub->ControlTransferLock);

    //
    // Destroy remaining data and the hub itself.
    //

    MmFreeIoBuffer(Hub->IoBuffer);
    MmFreePagedPool(Hub->HubStatus.PortStatus);
    MmFreePagedPool(Hub);
    return;
}

USB_API
KSTATUS
UsbStartHub (
    PUSB_HUB Hub
    )

/*++

Routine Description:

    This routine starts a USB hub.

Arguments:

    Hub - Supplies a pointer to the hub to start.

Return Value:

    None.

--*/

{

    PUSB_DEVICE Device;
    BOOL LockHeld;
    KSTATUS Status;

    LockHeld = FALSE;
    Device = (PUSB_DEVICE)(Hub->DeviceHandle);

    //
    // If this is not the root hub, reset the hub. This consists of turning the
    // power on for each port, collecting the hub status, and starting the
    // change notification interrupts.
    //

    if (Device->Type != UsbDeviceTypeRootHub) {

        ASSERT(Device->Type == UsbDeviceTypeHub);

        Status = UsbpResetHub(Hub);
        if (!KSUCCESS(Status)) {
            goto StartHubEnd;
        }

    //
    // Otherwise, just read the port status information out of the hub.
    // Synchronize this with port status change notifications that may also
    // modify the hub's software status.
    //

    } else {
        KeAcquireQueuedLock(Device->ChildLock);
        LockHeld = TRUE;
        Status = UsbpGetHubStatus(Hub, TRUE);
        if (!KSUCCESS(Status)) {
            goto StartHubEnd;
        }
    }

StartHubEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Device->ChildLock);
    }

    return Status;
}

USB_API
KSTATUS
UsbHubQueryChildren (
    PIRP Irp,
    PUSB_HUB Hub
    )

/*++

Routine Description:

    This routine responds to the Query Children IRP for a USB Hub. This routine
    must be called at low level.

Arguments:

    Irp - Supplies a pointer to the Query Children IRP.

    Hub - Supplies a pointer to the hub to query.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PUSB_DEVICE Child;
    ULONG ChildCount;
    ULONG ChildIndex;
    PDEVICE *Children;
    PLIST_ENTRY CurrentEntry;
    PUSB_DEVICE Device;
    BOOL DeviceLockHeld;
    UCHAR PortIndex;
    PUSB_PORT_STATUS PortStatus;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Children = NULL;
    Device = (PUSB_DEVICE)(Hub->DeviceHandle);

    //
    // Loop over all possible ports in the hub.
    //

    KeAcquireQueuedLock(Device->ChildLock);
    DeviceLockHeld = TRUE;

    ASSERT(Hub->PortCount != 0);
    ASSERT(Hub->HubStatus.PortStatus != NULL);

    for (PortIndex = 0; PortIndex < Hub->PortCount; PortIndex += 1) {

        //
        // Loop over all children of this device to find one corresponding to
        // this port.
        //

        CurrentEntry = Device->ChildList.Next;
        while (CurrentEntry != &(Device->ChildList)) {
            Child = LIST_VALUE(CurrentEntry, USB_DEVICE, ListEntry);
            if (Child->PortNumber == PortIndex + 1) {
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (CurrentEntry == &(Device->ChildList)) {
            Child = NULL;
        }

        //
        // Handle cases where the port status changed.
        //

        PortStatus = &(Hub->HubStatus.PortStatus[PortIndex]);
        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_CONNECTED) != 0) {

            //
            // If there had previously been a child at the current port, then
            // remove it from the list. The port is either empty or the child
            // was replaced. It will be reported as missing when this call
            // completes, triggering the removal process.
            //

            if (Child != NULL) {
                UsbpRemoveDevice(Child);
            }

            //
            // If there is a device present, then it's new. Create the new
            // device. Ignore failures here to allow other devices to be
            // enumerated.
            //

            if ((PortStatus->Status & USB_PORT_STATUS_CONNECTED) != 0) {
                UsbpHubAddDevice(Hub, PortIndex);
            }

            //
            // Clear the changed status in the port.
            //

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_CONNECTED;
        }
    }

    //
    // Loop once to determine how many children there are. A child should only
    // not have an OS device if it is the debug device.
    //

    ChildCount = 0;
    CurrentEntry = Device->ChildList.Next;
    while (CurrentEntry != &(Device->ChildList)) {
        Child = LIST_VALUE(CurrentEntry, USB_DEVICE, ListEntry);

        ASSERT((Child->Device != NULL) || (Child->DebugDevice != FALSE));

        if (Child->Device != NULL) {
            ChildCount += 1;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (ChildCount == 0) {
        Status = STATUS_SUCCESS;
        goto HubEnumerateChildrenEnd;
    }

    //
    // Create the array of OS device objects to report the children.
    //

    AllocationSize = sizeof(PDEVICE) * ChildCount;
    Children = MmAllocatePagedPool(AllocationSize, USB_CORE_ALLOCATION_TAG);
    if (Children == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto HubEnumerateChildrenEnd;
    }

    RtlZeroMemory(Children, AllocationSize);
    ChildIndex = 0;
    CurrentEntry = Device->ChildList.Next;
    while (CurrentEntry != &(Device->ChildList)) {
        Child = LIST_VALUE(CurrentEntry, USB_DEVICE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Child->Device != NULL) {
            Children[ChildIndex] = Child->Device;
            ChildIndex += 1;
        }
    }

    ASSERT(ChildIndex == ChildCount);

    //
    // Merge this child array with the children already in the IRP. This
    // routine allocates a new array, so release the array allocated here
    // upon the completion of query children.
    //

    Status = IoMergeChildArrays(Irp,
                                Children,
                                ChildCount,
                                USB_CORE_ALLOCATION_TAG);

    if (!KSUCCESS(Status)) {
        goto HubEnumerateChildrenEnd;
    }

    Status = STATUS_SUCCESS;

HubEnumerateChildrenEnd:
    if (Children != NULL) {
        MmFreePagedPool(Children);
    }

    if (DeviceLockHeld != FALSE) {
        KeReleaseQueuedLock(Device->ChildLock);
    }

    return Status;
}

VOID
UsbpNotifyRootHubStatusChange (
    PUSB_HUB RootHub
    )

/*++

Routine Description:

    This routine handles notifications from the host controller indicating that
    a port on the root hub has changed. It queries the port status for the hub
    and notifies the system if something is different.

Arguments:

    RootHub - Supplies a pointer to the USB root hub.

Return Value:

    None.

--*/

{

    BOOL LockHeld;
    BOOL PortChanged;
    ULONG PortCount;
    ULONG PortIndex;
    PUSB_PORT_STATUS PortStatus;
    PUSB_DEVICE RootDevice;
    KSTATUS Status;

    ASSERT(RootHub != NULL);
    ASSERT(RootHub->HubStatus.PortStatus != NULL);

    //
    // Acquire the device's child lock to synchronize with other accesses to
    // the ports.
    //

    RootDevice = (PUSB_DEVICE)RootHub->DeviceHandle;
    KeAcquireQueuedLock(RootDevice->ChildLock);
    LockHeld = TRUE;

    //
    // Get the status for the root hub.
    //

    Status = UsbpGetRootHubStatus(RootHub);
    if (!KSUCCESS(Status)) {
        goto NotifyRootHubStatusChange;
    }

    //
    // Search through the ports for change notifications.
    //

    PortChanged = FALSE;
    PortCount = RootHub->PortCount;
    for (PortIndex = 0; PortIndex < PortCount; PortIndex += 1) {
        PortStatus = &(RootHub->HubStatus.PortStatus[PortIndex]);

        //
        // Run through the over-current reset sequence as defined in section
        // 11.12.5 of the USB 2.0 Specification.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_OVER_CURRENT) != 0) {

            //
            // Wait until the over current bit is clear.
            //

            while ((PortStatus->Status & USB_PORT_STATUS_OVER_CURRENT) != 0) {
                Status = UsbpGetRootHubStatus(RootHub);
                if (!KSUCCESS(Status)) {
                    goto NotifyRootHubStatusChange;
                }
            }

            //
            // Now wipe the port status and reset the port. There is no
            // mechanism to power on a root port, so settle for a reset. The
            // USB specification is not clear on what to do for the root hub's
            // ports.
            //

            RtlZeroMemory(PortStatus, sizeof(USB_PORT_STATUS));
            RootHub->HubStatus.PortDeviceSpeed[PortIndex] =
                                                         UsbDeviceSpeedInvalid;

            Status = UsbpResetHubPort(RootHub, PortIndex);
            if (!KSUCCESS(Status)) {
                continue;
            }

            //
            // Collect the status one more time after the power on. If there is
            // something behind the port then the connection changed bit should
            // get set.
            //

            Status = UsbpGetRootHubStatus(RootHub);
            if (!KSUCCESS(Status)) {
                goto NotifyRootHubStatusChange;
            }
        }

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_CONNECTED) != 0) {
            PortChanged = TRUE;
            break;
        }
    }

    KeReleaseQueuedLock(RootDevice->ChildLock);
    LockHeld = FALSE;

    //
    // A change was found. Notify the system.
    //

    if (PortChanged != FALSE) {
        IoNotifyDeviceTopologyChange(RootDevice->Device);
    }

NotifyRootHubStatusChange:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(RootDevice->ChildLock);
    }

    return;
}

KSTATUS
UsbpResetHubPort (
    PUSB_HUB Hub,
    UCHAR PortIndex
    )

/*++

Routine Description:

    This routine resets the device behind the given port. The controller lock
    must be held.

Arguments:

    Hub - Supplies a pointer to the context of the hub whose port is to be
        reset.

    PortIndex - Supplies the zero-based port index on the hub to reset.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE HubDevice;
    PUSB_PORT_STATUS PortStatus;
    KSTATUS Status;

    ASSERT(Hub->HubStatus.PortStatus != NULL);

    HubDevice = (PUSB_DEVICE)Hub->DeviceHandle;

    ASSERT(KeIsQueuedLockHeld(HubDevice->ChildLock) != FALSE);

    //
    // Reset the port in question.
    //

    PortStatus = &(Hub->HubStatus.PortStatus[PortIndex]);
    PortStatus->Status |= USB_PORT_STATUS_RESET;
    PortStatus->Status &= ~USB_PORT_STATUS_ENABLED;
    PortStatus->Change |= (USB_PORT_STATUS_CHANGE_RESET |
                           USB_PORT_STATUS_CHANGE_ENABLED);

    Status = UsbpSetHubStatus(Hub);
    if (!KSUCCESS(Status)) {
        goto ResetHubPortEnd;
    }

    //
    // Stall for 10ms per section 7.1.7.5 of the USB specification (TDRST).
    // This is reduced because around 10ms devices start to suspend themselves
    // and stop responding to requests.
    //

    HlBusySpin(5 * MICROSECONDS_PER_MILLISECOND);

    //
    // Now enable the port.
    //

    PortStatus->Status &= ~USB_PORT_STATUS_RESET;
    PortStatus->Status |= USB_PORT_STATUS_ENABLED;
    PortStatus->Change |= (USB_PORT_STATUS_CHANGE_RESET |
                           USB_PORT_STATUS_CHANGE_ENABLED);

    Status = UsbpSetHubStatus(Hub);
    if (!KSUCCESS(Status)) {
        goto ResetHubPortEnd;
    }

    //
    // Stall for 10ms per section 7.1.7.5 of the USB specification (TRSTRCY).
    //

    HlBusySpin(25 * MICROSECONDS_PER_MILLISECOND);

    //
    // Get the status of the port now (actively request it, don't rely on the
    // interrupt transfer, as it's blocked waiting to hold the hub lock.
    //

    Status = UsbpGetHubStatus(Hub, TRUE);
    if (!KSUCCESS(Status)) {
        goto ResetHubPortEnd;
    }

    //
    // If the reset did not enable the port, then clear the changed bit.
    //

    if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_ENABLED) != 0) {

        ASSERT((PortStatus->Status & USB_PORT_STATUS_ENABLED) == 0);

        PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_ENABLED;
    }

    //
    // If the device is not present, then exit claiming success. It may have
    // been removed during the reset.
    //

    if ((PortStatus->Status & USB_PORT_STATUS_CONNECTED) == 0) {
        Status = STATUS_SUCCESS;
        goto ResetHubPortEnd;
    }

    //
    // If the port got disabled, fail the reset. Note that a device might still
    // be in the connected state even though it is in the disabled state, so
    // this must fail. See Section 11.24.2.7.1 PORT_CONNECTION of the USB 2.0
    // Specification.
    //

    if ((PortStatus->Status & USB_PORT_STATUS_ENABLED) == 0) {
        Status = STATUS_NOT_READY;
        goto ResetHubPortEnd;
    }

    ASSERT(Hub->HubStatus.PortDeviceSpeed[PortIndex] != UsbDeviceSpeedInvalid);

    //
    // Stall again to allow the device time to initialize.
    //

    HlBusySpin(20 * MICROSECONDS_PER_MILLISECOND);
    Status = STATUS_SUCCESS;

ResetHubPortEnd:
    if (((UsbDebugFlags & (USB_DEBUG_HUB | USB_DEBUG_ENUMERATION)) != 0) ||
        ((!KSUCCESS(Status)) && ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

        RtlDebugPrint("USB: Hub 0x%x reset port %d, status %d.\n",
                      Hub,
                      PortIndex,
                      Status);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UsbpGetHubStatus (
    PUSB_HUB Hub,
    BOOL ForceRefresh
    )

/*++

Routine Description:

    This routine gets the current hub and port status out of a USB hub.

Arguments:

    Hub - Supplies the pointer to the hub context for this device.

    ForceRefresh - Supplies a boolean indicating if all ports should be
        re-queried.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    ULONG PortIndex;
    ULONG PortStatus;
    KSTATUS Status;

    Device = (PUSB_DEVICE)(Hub->DeviceHandle);

    ASSERT(KeIsQueuedLockHeld(Device->ChildLock) != FALSE);

    //
    // For root hubs, just farm off the question to the host controller.
    //

    if (Device->Type == UsbDeviceTypeRootHub) {
        Status = UsbpGetRootHubStatus(Hub);
        goto GetHubStatusEnd;
    }

    ASSERT(Device->Type == UsbDeviceTypeHub);

    //
    // If no refresh is required, just return what's already found. An interrupt
    // transfer will automatically update these values when they change.
    //

    if (ForceRefresh == FALSE) {
        Status = STATUS_SUCCESS;
        goto GetHubStatusEnd;
    }

    //
    // If a refresh is requested, get each port's status.
    //

    for (PortIndex = 0; PortIndex < Hub->PortCount; PortIndex += 1) {
        Status = UsbpHubGetPortStatus(Hub, PortIndex + 1, &PortStatus);
        if (!KSUCCESS(Status)) {
            goto GetHubStatusEnd;
        }

        //
        // Set the software bits based on the hardware bits.
        //

        UsbpHubUpdatePortStatus(Hub, PortIndex, PortStatus);

        //
        // Clear out any change bits.
        //

        Status = UsbpHubClearPortChangeBits(Hub, PortIndex + 1, PortStatus);
        if (!KSUCCESS(Status)) {
            goto GetHubStatusEnd;
        }
    }

    Status = STATUS_SUCCESS;

GetHubStatusEnd:
    return Status;
}

KSTATUS
UsbpGetRootHubStatus (
    PUSB_HUB RootHub
    )

/*++

Routine Description:

    This routine gets the root hub's port status out of the USB host controller.

Arguments:

    RootHub - Supplies a pointer to hub context for the root USB hub.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    PUSB_HOST_GET_ROOT_HUB_STATUS GetRootHubStatus;
    PVOID HostControllerContext;
    ULONG PortIndex;
    PUSB_PORT_STATUS PortStatus;
    KSTATUS Status;

    Device = (PUSB_DEVICE)RootHub->DeviceHandle;

    ASSERT(Device->Type == UsbDeviceTypeRootHub);
    ASSERT(RootHub->HubStatus.PortStatus != NULL);

    //
    // Farm the question off to the host controller.
    //

    HostControllerContext = Device->Controller->Device.HostControllerContext;
    GetRootHubStatus = Device->Controller->Device.GetRootHubStatus;
    Status = GetRootHubStatus(HostControllerContext, &(RootHub->HubStatus));
    if (((UsbDebugFlags & USB_DEBUG_HUB) != 0) ||
        ((!KSUCCESS(Status)) && ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

        for (PortIndex = 0; PortIndex < RootHub->PortCount; PortIndex += 1) {
            PortStatus = &(RootHub->HubStatus.PortStatus[PortIndex]);
            RtlDebugPrint(
                 "USB: Root Hub 0x%x Port %d SoftwareStatus 0x%x, "
                 "SoftwareChange 0x%x Status %d.\n"
                 "USB: Speed %d Enabled %d Suspended %d OverCurrent %d "
                 "Present %d\n",
                 RootHub,
                 PortIndex,
                 PortStatus->Status,
                 PortStatus->Change,
                 Status,
                 RootHub->HubStatus.PortDeviceSpeed[PortIndex],
                 (PortStatus->Status & USB_PORT_STATUS_ENABLED) != 0,
                 (PortStatus->Status & USB_PORT_STATUS_SUSPENDED) != 0,
                 (PortStatus->Status & USB_PORT_STATUS_OVER_CURRENT) != 0,
                 (PortStatus->Status & USB_PORT_STATUS_CONNECTED) != 0);
        }
    }

    return Status;
}

KSTATUS
UsbpSetHubStatus (
    PUSB_HUB Hub
    )

/*++

Routine Description:

    This routine sets the current hub and port status out of a USB hub.

Arguments:

    Hub - Supplies a pointer to the hub context.

Return Value:

    Status code.

--*/

{

    PUSB_HOST_CONTROLLER Controller;
    PUSB_DEVICE Device;
    PVOID HostControllerContext;
    ULONG PortIndex;
    PUSB_PORT_STATUS PortStatus;
    BOOL SetFeature;
    PUSB_HOST_SET_ROOT_HUB_STATUS SetRootHubStatus;
    KSTATUS Status;

    ASSERT(Hub->HubStatus.PortStatus != NULL);

    Device = (PUSB_DEVICE)(Hub->DeviceHandle);

    //
    // For root hubs, just farm off the work to the host controller.
    //

    if (Device->Type == UsbDeviceTypeRootHub) {
        Controller = Device->Controller;
        HostControllerContext = Controller->Device.HostControllerContext;
        SetRootHubStatus = Controller->Device.SetRootHubStatus;
        Status = SetRootHubStatus(HostControllerContext, &(Hub->HubStatus));
        goto SetHubStatusEnd;
    }

    ASSERT(Device->Type == UsbDeviceTypeHub);

    //
    // Loop through each port looking for a change.
    //

    for (PortIndex = 0; PortIndex < Hub->PortCount; PortIndex += 1) {

        //
        // Determine what changed between the previous status and the current
        // status, and act on those bits.
        //

        PortStatus = &(Hub->HubStatus.PortStatus[PortIndex]);

        //
        // If no bits changed, then there is nothing to do really.
        //

        if (PortStatus->Change == 0) {
            continue;
        }

        //
        // Handle port enabled change events.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_ENABLED) != 0) {

            //
            // Disable the port if it changed and is no longer enabled.
            // Enabling a port directly is not allowed. This must be done
            // through a reset.
            //

            if ((PortStatus->Status & USB_PORT_STATUS_ENABLED) == 0) {
                Status = UsbpHubSetOrClearFeature(Hub,
                                                  FALSE,
                                                  USB_HUB_FEATURE_PORT_ENABLE,
                                                  PortIndex + 1);

                if (!KSUCCESS(Status)) {
                    goto SetHubStatusEnd;
                }
            }

            //
            // Clear the change bit now that it has been handled.
            //

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_ENABLED;
        }

        //
        // Handle port reset changes.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_RESET) != 0) {

            //
            // If the port is to be reset, then issue a reset. Note that a port
            // cannot be "un-reset", the hardware handles this.
            //

            if ((PortStatus->Status & USB_PORT_STATUS_RESET) != 0) {
                Status = UsbpHubSetOrClearFeature(Hub,
                                                  TRUE,
                                                  USB_HUB_FEATURE_PORT_RESET,
                                                  PortIndex + 1);

                if (!KSUCCESS(Status)) {
                    goto SetHubStatusEnd;
                }
            }

            //
            // Clear the change bit now that it has been handled.
            //

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_RESET;
        }

        //
        // Handle port suspend changes.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_SUSPENDED) != 0) {
            if ((PortStatus->Status & USB_PORT_STATUS_SUSPENDED) != 0) {
                SetFeature = TRUE;

            } else {
                SetFeature = FALSE;
            }

            Status = UsbpHubSetOrClearFeature(Hub,
                                              SetFeature,
                                              USB_HUB_FEATURE_PORT_SUSPEND,
                                              PortIndex + 1);

            if (!KSUCCESS(Status)) {
                goto SetHubStatusEnd;
            }

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_SUSPENDED;
        }
    }

    Status = STATUS_SUCCESS;

SetHubStatusEnd:
    return Status;
}

VOID
UsbpHubUpdatePortStatus (
    PUSB_HUB Hub,
    ULONG PortIndex,
    ULONG HardwareStatus
    )

/*++

Routine Description:

    This routine converts the given hardware port status to software status and
    updates the port status of the given hub at the given index. It saves the
    old port status in the process.

Arguments:

    Hub - Supplies a pointer to the USB hub device whose port status is to be
        updated.

    PortIndex - Supplies the index of the port whose status is to be updated.
        This is zero-indexed.

    HardwareStatus - Supplies the port's hardware status that needs to be
        converted to software status.

Return Value:

    None.

--*/

{

    USHORT ChangeBits;
    PUSB_PORT_STATUS PortStatus;
    USHORT SoftwareStatus;
    USB_DEVICE_SPEED Speed;

    ASSERT(Hub->HubStatus.PortStatus != NULL);

    Speed = UsbDeviceSpeedInvalid;
    SoftwareStatus = 0;
    if ((HardwareStatus & USB_HUB_PORT_STATUS_DEVICE_CONNECTED) != 0) {
        SoftwareStatus |= USB_PORT_STATUS_CONNECTED;
        if ((HardwareStatus & USB_HUB_PORT_STATUS_HIGH_SPEED) != 0) {
            Speed = UsbDeviceSpeedHigh;

        } else if ((HardwareStatus & USB_HUB_PORT_STATUS_LOW_SPEED) != 0) {
            Speed = UsbDeviceSpeedLow;

        } else {
            Speed = UsbDeviceSpeedFull;
        }
    }

    if ((HardwareStatus & USB_HUB_PORT_STATUS_ENABLED) != 0) {
        SoftwareStatus |= USB_PORT_STATUS_ENABLED;
    }

    if ((HardwareStatus & USB_HUB_PORT_STATUS_SUSPENDED) != 0) {
        SoftwareStatus |= USB_PORT_STATUS_SUSPENDED;
    }

    if ((HardwareStatus & USB_HUB_PORT_STATUS_OVER_CURRENT) != 0) {
        SoftwareStatus |= USB_PORT_STATUS_OVER_CURRENT;
    }

    //
    // If the new status does not match the current status, then mark the
    // appropriate fields as changed and set the new status.
    //

    PortStatus = &(Hub->HubStatus.PortStatus[PortIndex]);
    if (SoftwareStatus != PortStatus->Status) {
        ChangeBits = SoftwareStatus ^ PortStatus->Status;

        //
        // Because the port status bits and changed bits match 1-to-1, just OR
        // in the changed bits.
        //

        PortStatus->Change |= ChangeBits;
        PortStatus->Status = SoftwareStatus;
    }

    if ((UsbDebugFlags & USB_DEBUG_HUB) != 0) {
        RtlDebugPrint(
                 "USB: Hub 0x%x Port %d HardwareStatus 0x%x, SoftwareStatus "
                 "0x%x, SoftwareChange 0x%x\n"
                 "USB: Speed %d Enabled %d Suspended %d OverCurrent %d "
                 "Present %d\n",
                 Hub,
                 PortIndex,
                 HardwareStatus,
                 PortStatus->Status,
                 PortStatus->Change,
                 Speed,
                 (HardwareStatus & USB_HUB_PORT_STATUS_ENABLED) != 0,
                 (HardwareStatus & USB_HUB_PORT_STATUS_SUSPENDED) != 0,
                 (HardwareStatus & USB_HUB_PORT_STATUS_OVER_CURRENT) != 0,
                 (HardwareStatus & USB_HUB_PORT_STATUS_DEVICE_CONNECTED) != 0);
    }

    //
    // Save the new speed.
    //

    Hub->HubStatus.PortDeviceSpeed[PortIndex] = Speed;
    return;
}

KSTATUS
UsbpResetHub (
    PUSB_HUB Hub
    )

/*++

Routine Description:

    This routine resets a USB hub.

Arguments:

    Hub - Supplies a pointer to the hub to reset.

Return Value:

    Status code.

--*/

{

    ULONG BufferAlignment;
    PUSB_CONFIGURATION_DESCRIPTION Configuration;
    PUSB_DEVICE Device;
    PUSB_ENDPOINT_DESCRIPTION Endpoint;
    UCHAR EndpointNumber;
    PUSB_INTERFACE_DESCRIPTION Interface;
    BOOL LockHeld;
    ULONG MaxControlSize;
    ULONG MaxInterruptSize;
    ULONG PortIndex;
    KSTATUS Status;
    ULONG TransferLength;

    ASSERT(Hub->PortCount != 0);

    Device = (PUSB_DEVICE)Hub->DeviceHandle;
    LockHeld = FALSE;

    //
    // Send the SET_CONFIGURATION request to the port.
    //

    Status = UsbSetConfiguration(Hub->DeviceHandle, 0, TRUE);
    if (!KSUCCESS(Status)) {
        goto ResetHubEnd;
    }

    if (Hub->Interface == NULL) {

        //
        // Get the only configuration.
        //

        Status = UsbGetConfiguration(Hub->DeviceHandle,
                                     0,
                                     TRUE,
                                     &Configuration);

        if (!KSUCCESS(Status)) {
            goto ResetHubEnd;
        }

        //
        // Find and claim the only interface.
        //

        if (LIST_EMPTY(&(Configuration->InterfaceListHead)) != FALSE) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto ResetHubEnd;
        }

        Interface = LIST_VALUE(Configuration->InterfaceListHead.Next,
                               USB_INTERFACE_DESCRIPTION,
                               ListEntry);

        Status = UsbClaimInterface(Hub->DeviceHandle,
                                   Interface->Descriptor.InterfaceNumber);

        if (!KSUCCESS(Status)) {
            goto ResetHubEnd;
        }

        //
        // Get the interrupt endpoint.
        //

        if (LIST_EMPTY(&(Interface->EndpointListHead)) != FALSE) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto ResetHubEnd;
        }

        Endpoint = LIST_VALUE(Interface->EndpointListHead.Next,
                              USB_ENDPOINT_DESCRIPTION,
                              ListEntry);

        if ((Endpoint->Descriptor.Attributes &
             USB_ENDPOINT_ATTRIBUTES_TYPE_MASK) !=
            USB_ENDPOINT_ATTRIBUTES_TYPE_INTERRUPT) {

            Status = STATUS_INVALID_CONFIGURATION;
            goto ResetHubEnd;
        }

        //
        // Create the interrupt transfer that goes on the status change
        // endpoint.
        //

        BufferAlignment = MmGetIoBufferAlignment();

        ASSERT(POWER_OF_2(BufferAlignment) != FALSE);
        ASSERT(Hub->InterruptTransfer == NULL);

        EndpointNumber = Endpoint->Descriptor.EndpointAddress;
        TransferLength = ALIGN_RANGE_UP(Hub->PortCount + 1, BITS_PER_BYTE) /
                         BITS_PER_BYTE;

        Hub->InterruptTransfer = UsbAllocateTransfer(Hub->DeviceHandle,
                                                     EndpointNumber,
                                                     TransferLength,
                                                     0);

        if (Hub->InterruptTransfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ResetHubEnd;
        }

        MaxControlSize = ALIGN_RANGE_UP(USB_HUB_MAX_CONTROL_TRANSFER_SIZE,
                                        BufferAlignment);

        MaxInterruptSize = ALIGN_RANGE_UP(USB_HUB_MAX_INTERRUPT_SIZE,
                                          BufferAlignment);

        ASSERT(Hub->IoBuffer->Fragment[0].Size >=
               (MaxControlSize + MaxInterruptSize));

        Hub->InterruptTransfer->Buffer =
                                    Hub->IoBuffer->Fragment[0].VirtualAddress +
                                    MaxControlSize;

        Hub->InterruptTransfer->BufferPhysicalAddress =
                                   Hub->IoBuffer->Fragment[0].PhysicalAddress +
                                   MaxControlSize;

        Hub->InterruptTransfer->BufferActualLength = MaxInterruptSize;
        Hub->InterruptTransfer->Direction = UsbTransferDirectionIn;
        Hub->InterruptTransfer->Length = TransferLength;
        Hub->InterruptTransfer->CallbackRoutine =
                                            UsbpHubInterruptTransferCompletion;

        Hub->InterruptTransfer->UserData = Hub;
        Hub->Interface = Interface;

    //
    // This is not the first time the hub has been reset.
    //

    } else {

        //
        // Attempt to cancel the interrupt transfer. If the transfer is on the
        // hardware queue, then the cancel will succeed. Otherwise, it is too
        // late to cancel it. Since the interrupt transfer's callback resubmits
        // the transfer, it should get cancelled if this keeps trying.
        //

        while (TRUE) {

            //
            // Cancel the transfer, which tries to cancel and just waits until
            // the transfer is in the inactive state. It returns successfully
            // only if the transfer was actually pulled off the hardware queue.
            // If this fails with status too early, then the transfer is not in
            // the hardware queue and not in the callback. This means that the
            // hub status change worker is queued or running. It is likely the
            // one requesting a reset. Let it go through.
            //

            Status = UsbCancelTransfer(Hub->InterruptTransfer, TRUE);
            if (KSUCCESS(Status) || (Status == STATUS_TOO_EARLY)) {
                break;
            }

            //
            // If the device has been disconnected, the transfer might not go
            // around again and might have missed the cancel. Just exit.
            //
            // N.B. This case is currently not possible since hub reset is only
            //      called during the hub start IRP. This needs to be here,
            //      however, if the system tried to reset the hub in parallel
            //      with a removal IRP.
            //

            if (Device->Connected == FALSE) {
                Status = STATUS_SUCCESS;
                goto ResetHubEnd;
            }

            //
            // Rest a bit to let stuff progress. This may not be fruitful or
            // necessary since the cancel will do some yielding.
            //

            KeYield();
        }
    }

    //
    // Acquire the hub's child lock so no state changes during the reset.
    //

    KeAcquireQueuedLock(Device->ChildLock);
    LockHeld = TRUE;

    //
    // Reset the state for every port. That is, zero out the state ignoring any
    // change bits.
    //

    RtlZeroMemory(Hub->HubStatus.PortStatus,
                  (sizeof(USB_PORT_STATUS) * Hub->PortCount));

    RtlZeroMemory(Hub->HubStatus.PortDeviceSpeed,
                  (sizeof(USB_DEVICE_SPEED) * Hub->PortCount));

    //
    // Loop through and power on each port.
    //

    for (PortIndex = 0; PortIndex < Hub->PortCount; PortIndex += 1) {
        Status = UsbpHubSetOrClearFeature(Hub,
                                          TRUE,
                                          USB_HUB_FEATURE_PORT_POWER,
                                          PortIndex + 1);

        if (!KSUCCESS(Status)) {
            goto ResetHubEnd;
        }
    }

    //
    // Set the port indicators to auto. The set power feature set them to the
    // off state.
    //

    if (Hub->HasIndicators != FALSE) {
        for (PortIndex = 0; PortIndex < Hub->PortCount; PortIndex += 1) {
            Status = UsbpHubSetOrClearFeature(
                                Hub,
                                TRUE,
                                USB_HUB_FEATURE_PORT_INDICATOR,
                                (PortIndex + 1) | USB_HUB_INDICATOR_AUTOMATIC);

            if (!KSUCCESS(Status)) {
                goto ResetHubEnd;
            }
        }
    }

    //
    // Now that the ports have been powered up, delay for the appropriate
    // amount of time before accessing them again.
    //

    KeDelayExecution(FALSE,
                     FALSE,
                     Hub->PowerUpDelayIn2ms * 2 * MICROSECONDS_PER_MILLISECOND);

    //
    // After waiting for the ports to power up, get the current status.
    //

    Status = UsbpGetHubStatus(Hub, TRUE);
    if (!KSUCCESS(Status)) {
        goto ResetHubEnd;
    }

    KeReleaseQueuedLock(Device->ChildLock);
    LockHeld = FALSE;

    //
    // Submit the interrupt transfer.
    //

    Status = UsbSubmitTransfer(Hub->InterruptTransfer);
    if (!KSUCCESS(Status)) {
        goto ResetHubEnd;
    }

ResetHubEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Device->ChildLock);
    }

    return Status;
}

KSTATUS
UsbpReadHubDescriptor (
    PUSB_HUB Hub
    )

/*++

Routine Description:

    This routine sends a request to read in the hub descriptor, and sets the
    various fields of the hub structure according to the result.

Arguments:

    Hub - Supplies a pointer to the hub.

Return Value:

    Status code.

--*/

{

    PUSB_HUB_DESCRIPTOR HubDescriptor;
    ULONG LengthTransferred;
    PUSB_SETUP_PACKET Setup;
    KSTATUS Status;

    Setup = (PUSB_SETUP_PACKET)Hub->ControlTransfer->Buffer;
    KeAcquireQueuedLock(Hub->ControlTransferLock);
    Hub->ControlTransfer->Direction = UsbTransferDirectionIn;

    //
    // Send the GET_DESCRIPTOR request.
    //

    Setup->RequestType = USB_SETUP_REQUEST_TO_HOST |
                         USB_SETUP_REQUEST_CLASS |
                         USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup->Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    Setup->Value = UsbDescriptorTypeHub << 8;
    Setup->Index = 0;
    Setup->Length = USB_HUB_DESCRIPTOR_MAX_SIZE;
    Hub->ControlTransfer->Length = sizeof(USB_SETUP_PACKET) +
                                   USB_HUB_DESCRIPTOR_MAX_SIZE;

    Status = UsbpHubSendControlTransfer(Hub, &LengthTransferred);
    if (!KSUCCESS(Status)) {
        goto ReadHubDescriptorEnd;
    }

    if (LengthTransferred < sizeof(USB_HUB_DESCRIPTOR)) {
        Status = STATUS_NOT_SUPPORTED;
        goto ReadHubDescriptorEnd;
    }

    HubDescriptor = (PUSB_HUB_DESCRIPTOR)(Setup + 1);
    if ((HubDescriptor->DescriptorType != UsbDescriptorTypeHub) ||
        (HubDescriptor->Length < sizeof(USB_HUB_DESCRIPTOR))) {

        Status = STATUS_NOT_SUPPORTED;
        goto ReadHubDescriptorEnd;
    }

    Hub->PortCount = HubDescriptor->PortCount;
    Hub->PowerUpDelayIn2ms = HubDescriptor->PowerUpDelayIn2ms;
    if ((HubDescriptor->HubCharacteristics &
         USB_HUB_CHARACTERISTIC_INDICATORS_SUPPORTED) != 0) {

        Hub->HasIndicators = TRUE;
    }

    Status = STATUS_SUCCESS;

ReadHubDescriptorEnd:
    KeReleaseQueuedLock(Hub->ControlTransferLock);
    return Status;
}

KSTATUS
UsbpHubSendControlTransfer (
    PUSB_HUB Hub,
    PULONG LengthTransferred
    )

/*++

Routine Description:

    This routine sends a synchronous control transfer. It assumes that the hub's
    buffer is already all set up and ready to go.

Arguments:

    Hub - Supplies a pointer to the hub.

    LengthTransferred - Supplies a pointer to the length to transfer.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONG TransferCount;

    ASSERT(Hub->ControlTransfer->Direction != UsbTransferDirectionInvalid);

    TransferCount = 0;
    Status = UsbSubmitSynchronousTransfer(Hub->ControlTransfer);
    if (!KSUCCESS(Status)) {
        goto HubSendControlTransferEnd;
    }

    ASSERT(KSUCCESS(Hub->ControlTransfer->Status));

    TransferCount = Hub->ControlTransfer->LengthTransferred -
                    sizeof(USB_SETUP_PACKET);

    Hub->ControlTransfer->Direction = UsbTransferDirectionInvalid;
    Status = STATUS_SUCCESS;

HubSendControlTransferEnd:
    if (LengthTransferred != NULL) {
        *LengthTransferred = TransferCount;
    }

    return Status;
}

KSTATUS
UsbpHubGetHubStatus (
    PUSB_HUB Hub,
    PULONG HubStatus
    )

/*++

Routine Description:

    This routine performs a control transfer to get the current status of the
    given USB hub.

Arguments:

    Hub - Supplies a pointer to the hub to query.

    HubStatus - Supplies a pointer where the hub status will be returned upon
        success.

Return Value:

    Status code.

--*/

{

    ULONG LengthTransferred;
    PUSB_SETUP_PACKET Setup;
    KSTATUS Status;

    Setup = (PUSB_SETUP_PACKET)Hub->ControlTransfer->Buffer;
    KeAcquireQueuedLock(Hub->ControlTransferLock);
    Setup->RequestType = USB_SETUP_REQUEST_TO_HOST |
                         USB_SETUP_REQUEST_CLASS |
                         USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup->Request = USB_DEVICE_REQUEST_GET_STATUS;
    Setup->Value = 0;
    Setup->Index = 0;
    Setup->Length = sizeof(ULONG);
    Hub->ControlTransfer->Direction = UsbTransferDirectionIn;
    Hub->ControlTransfer->Length = sizeof(USB_SETUP_PACKET) + sizeof(ULONG);
    Status = UsbpHubSendControlTransfer(Hub, &LengthTransferred);
    if (!KSUCCESS(Status)) {
        goto HubGetHubStatusEnd;
    }

    if (LengthTransferred != sizeof(ULONG)) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto HubGetHubStatusEnd;
    }

HubGetHubStatusEnd:
    KeReleaseQueuedLock(Hub->ControlTransferLock);
    if (!KSUCCESS(Status)) {
        *HubStatus = 0;

    } else {
        *HubStatus = *((PULONG)(Setup + 1));
    }

    return Status;
}

KSTATUS
UsbpHubGetPortStatus (
    PUSB_HUB Hub,
    ULONG PortNumber,
    PULONG PortStatus
    )

/*++

Routine Description:

    This routine performs a control transfer to get the current status of the
    given USB hub port.

Arguments:

    Hub - Supplies a pointer to the hub to query.

    PortNumber - Supplies the one-indexed port number to query.

    PortStatus - Supplies a pointer where the port status will be returned upon
        success.

Return Value:

    Status code.

--*/

{

    ULONG LengthTransferred;
    PUSB_SETUP_PACKET Setup;
    KSTATUS Status;

    ASSERT(PortNumber != 0);
    ASSERT(PortNumber <= Hub->PortCount);

    Setup = (PUSB_SETUP_PACKET)Hub->ControlTransfer->Buffer;
    KeAcquireQueuedLock(Hub->ControlTransferLock);
    Setup->RequestType = USB_SETUP_REQUEST_TO_HOST |
                         USB_SETUP_REQUEST_CLASS |
                         USB_SETUP_REQUEST_OTHER_RECIPIENT;

    Setup->Request = USB_DEVICE_REQUEST_GET_STATUS;
    Setup->Value = 0;
    Setup->Index = PortNumber;
    Setup->Length = sizeof(ULONG);
    Hub->ControlTransfer->Direction = UsbTransferDirectionIn;
    Hub->ControlTransfer->Length = sizeof(USB_SETUP_PACKET) + sizeof(ULONG);
    Status = UsbpHubSendControlTransfer(Hub, &LengthTransferred);
    if (!KSUCCESS(Status)) {
        goto HubGetPortStatusEnd;
    }

    if (LengthTransferred != sizeof(ULONG)) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto HubGetPortStatusEnd;
    }

HubGetPortStatusEnd:
    KeReleaseQueuedLock(Hub->ControlTransferLock);
    if (!KSUCCESS(Status)) {
        *PortStatus = 0;

    } else {
        *PortStatus = *((PULONG)(Setup + 1));
    }

    return Status;
}

KSTATUS
UsbpHubSetOrClearFeature (
    PUSB_HUB Hub,
    BOOL SetFeature,
    USHORT Feature,
    USHORT Port
    )

/*++

Routine Description:

    This routine sends a set feature or clear feature request to the hub.

Arguments:

    Hub - Supplies a pointer to the hub to send the transfer to.

    SetFeature - Supplies a boolean indicating whether this is a SET_FEATURE
        request (TRUE) or a CLEAR_FEATURE request (FALSE).

    Feature - Supplies the feature selector to set or clear. This is the value
        that goes in the Value field of the setup packet.

    Port - Supplies the port number to set or clear. This is the value that
        goes in the Index field of the setup packet. The first port is port 1.
        Supply 0 to set or clear a hub feature.

Return Value:

    Status code.

--*/

{

    ULONG LengthTransferred;
    PUSB_SETUP_PACKET Setup;
    KSTATUS Status;

    Setup = (PUSB_SETUP_PACKET)Hub->ControlTransfer->Buffer;
    KeAcquireQueuedLock(Hub->ControlTransferLock);
    Setup->RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                         USB_SETUP_REQUEST_CLASS;

    //
    // Treat port 0 as the hub itself.
    //

    if (Port == 0) {
        Setup->RequestType |= USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    } else {

        ASSERT(Port <= Hub->PortCount);

        Setup->RequestType |= USB_SETUP_REQUEST_OTHER_RECIPIENT;
    }

    if (SetFeature != FALSE) {
        Setup->Request = USB_DEVICE_REQUEST_SET_FEATURE;

    } else {
        Setup->Request = USB_DEVICE_REQUEST_CLEAR_FEATURE;
    }

    Setup->Value = Feature;
    Setup->Index = Port;
    Setup->Length = 0;
    Hub->ControlTransfer->Direction = UsbTransferDirectionOut;
    Hub->ControlTransfer->Length = sizeof(USB_SETUP_PACKET);
    Status = UsbpHubSendControlTransfer(Hub, &LengthTransferred);
    KeReleaseQueuedLock(Hub->ControlTransferLock);
    return Status;
}

VOID
UsbpHubInterruptTransferCompletion (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when the interrupt transfer on the hub's status
    change endpoint completes.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    USHORT ChangedPorts;
    PVOID DeviceToken;
    PUSB_HUB Hub;
    PUSB_TRANSFER_INTERNAL InternalTransfer;
    KSTATUS Status;
    BOOL SubmitTransfer;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Hub = (PUSB_HUB)Transfer->UserData;

    ASSERT(Transfer == Hub->InterruptTransfer);

    //
    // Handle errors.
    //

    SubmitTransfer = FALSE;
    if (!KSUCCESS(Transfer->Status)) {

        //
        // Exit on cancelled transfers. Something else will restart the
        // transfer if necessary.
        //

        if (Transfer->Status == STATUS_OPERATION_CANCELLED) {

            ASSERT(Transfer->Error == UsbErrorTransferCancelled);

        //
        // On IO errors, do not queue the work item, just re-submit.
        //

        } else if (Transfer->Status == STATUS_DEVICE_IO_ERROR) {

            //
            // If the endpoint halted, try to clear the halted feature bit.
            //

            if (Transfer->Error == UsbErrorTransferStalled) {
                InternalTransfer = (PUSB_TRANSFER_INTERNAL)Transfer;
                Status = UsbClearFeature(Hub->DeviceHandle,
                                         USB_SETUP_REQUEST_ENDPOINT_RECIPIENT,
                                         USB_FEATURE_ENDPOINT_HALT,
                                         InternalTransfer->EndpointNumber);

                if (!KSUCCESS(Status)) {
                    if ((UsbDebugFlags &
                         (USB_DEBUG_HUB | USB_DEBUG_ERRORS)) != 0) {

                        RtlDebugPrint("USB HUB: status change transfer "
                                      "(0x%08x) on hub 0x%08x stalled. Failed "
                                      "to clear HALT feature on endpoint with "
                                      "status %d.\n",
                                      Transfer,
                                      Hub,
                                      Status);
                    }

                    DeviceToken = UsbGetDeviceToken(Hub->DeviceHandle);
                    IoSetDeviceDriverError(DeviceToken,
                                           UsbCoreDriver,
                                           Status,
                                           USB_CORE_ERROR_ENDPOINT_HALTED);

                    goto HubInterruptCompletionEnd;
                }
            }

            SubmitTransfer = TRUE;

        //
        // On all other errors, notify the debugger and try again.
        //

        } else {
            RtlDebugPrint("USB HUB: Unexpected error for hub (0x%08x) status "
                          "change transfer (0x%08x): status %d, error %d.\n",
                          Hub,
                          Transfer,
                          Transfer->Status,
                          Transfer->Error);

            SubmitTransfer = TRUE;
        }

        goto HubInterruptCompletionEnd;
    }

    //
    // If the length transferred is correct, read in the changed port data.
    //

    ChangedPorts = 0;
    if (Transfer->LengthTransferred == Transfer->Length) {
        ChangedPorts = *((PUSHORT)Transfer->Buffer);
    }

    Hub->ChangedPorts = ChangedPorts;

    //
    // If something changed, queue the interrupt work item to get off of the
    // callback routine. While running in the callback, the control transfers
    // kicked off here won't complete.
    //

    if (ChangedPorts != 0) {
        Status = KeQueueWorkItem(Hub->InterruptWorkItem);

        ASSERT(KSUCCESS(Status));

    } else {
        SubmitTransfer = TRUE;
    }

HubInterruptCompletionEnd:
    if (SubmitTransfer != FALSE) {
        UsbSubmitTransfer(Hub->InterruptTransfer);
    }

    return;
}

VOID
UsbpHubInterruptTransferCompletionWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is a work item routine called when the interrupt transfer on
    the hub's status change endpoint completes.

Arguments:

    Parameter - Supplies a pointer to the work item parameter, which in this
        case is a pointer to the USB hub.

Return Value:

    None.

--*/

{

    USHORT ChangedPorts;
    BOOL ChildLockHeld;
    PUSB_DEVICE Device;
    ULONG HardwareStatus;
    PUSB_HUB Hub;
    USHORT HubChange;
    ULONG HubStatus;
    BOOL PortChanged;
    ULONG PortIndex;
    PUSB_PORT_STATUS PortStatus;
    KSTATUS Status;
    BOOL SubmitTransfer;
    BOOL TopologyChanged;
    PUSB_DEVICE UsbDevice;

    Hub = (PUSB_HUB)Parameter;
    Device = (PUSB_DEVICE)Hub->DeviceHandle;
    ChangedPorts = Hub->ChangedPorts;
    ChildLockHeld = FALSE;
    SubmitTransfer = TRUE;
    TopologyChanged = FALSE;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Hub->HubStatus.PortStatus != NULL);

    //
    // Bit zero is the hub's index.
    //

    if ((ChangedPorts & 0x0001) != 0) {
        Status = UsbpHubGetHubStatus(Hub, &HubStatus);
        if (KSUCCESS(Status)) {
            HubChange = HubStatus >> USB_HUB_HUB_STATUS_CHANGE_SHIFT;

            //
            // Just clear the local power status.
            //

            if ((HubChange & USB_HUB_HUB_STATUS_LOCAL_POWER) != 0) {
                UsbpHubSetOrClearFeature(Hub,
                                         FALSE,
                                         USB_HUB_FEATURE_C_HUB_LOCAL_POWER,
                                         0);
            }

            //
            // Handle over current changes according to section 11.12.5 of the
            // USB 2.0 Specification.
            //

            if ((HubChange & USB_HUB_HUB_STATUS_OVER_CURRENT) != 0) {

                //
                // Wait for the hub's over current status bit to go to zero.
                // Assumably, this is to wait for the hub to power off.
                //

                while ((HubStatus & USB_HUB_HUB_STATUS_OVER_CURRENT) != 0) {
                    Status = UsbpHubGetHubStatus(Hub, &HubStatus);
                    if (!KSUCCESS(Status)) {
                        goto HubInterruptTransferCompletionWorkerEnd;
                    }
                }

                //
                // Clear the over current change bit.
                //

                Status = UsbpHubSetOrClearFeature(
                                            Hub,
                                            FALSE,
                                            USB_HUB_FEATURE_C_HUB_OVER_CURRENT,
                                            0);

                if (!KSUCCESS(Status)) {
                    goto HubInterruptTransferCompletionWorkerEnd;
                }

                //
                // Reset the hub. If this succeeds, then it will have
                // re-submitted the interrupt transfer.
                //

                Status = UsbpResetHub(Hub);
                if (!KSUCCESS(Status)) {
                    goto HubInterruptTransferCompletionWorkerEnd;
                }

                SubmitTransfer = FALSE;

                //
                // Mark that the topology changed so that the system
                // re-enumerates all the ports on this hub.
                //

                TopologyChanged = TRUE;

                //
                // Exit without checking the individual port status. The whole
                // hub just got reset.
                //

                goto HubInterruptTransferCompletionWorkerEnd;
            }
        }
    }

    ChangedPorts = ChangedPorts >> 1;

    ASSERT(Hub->PortCount != 0);

    for (PortIndex = 0; PortIndex < Hub->PortCount; PortIndex += 1) {

        //
        // Determine if the port changed. If it didn't, move on.
        //

        PortChanged = FALSE;
        if ((ChangedPorts & 0x1) != 0) {
            PortChanged = TRUE;
        }

        ChangedPorts = ChangedPorts >> 1;
        if (PortChanged == FALSE) {
            continue;
        }

        //
        // If the port changed, read its status. Synchronize this with any
        // other port status changes.
        //

        KeAcquireQueuedLock(Device->ChildLock);
        ChildLockHeld = TRUE;
        Status = UsbpHubGetPortStatus(Hub, PortIndex + 1, &HardwareStatus);
        if (!KSUCCESS(Status)) {
            goto HubInterruptTransferCompletionWorkerEnd;
        }

        //
        // Update the software status stored in the hub.
        //

        UsbpHubUpdatePortStatus(Hub, PortIndex, HardwareStatus);
        if ((UsbDebugFlags & USB_DEBUG_HUB) != 0) {
            RtlDebugPrint("USB: Hub 0x%x Port %d Hardware Status 0x%x.\n",
                          Hub,
                          PortIndex,
                          HardwareStatus);
        }

        //
        // Handle over current change notifications.
        //

        PortStatus = &(Hub->HubStatus.PortStatus[PortIndex]);
        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_OVER_CURRENT) != 0) {

            //
            // Wait until the over current bit is clear.
            //

            while ((PortStatus->Status & USB_PORT_STATUS_OVER_CURRENT) != 0) {
                Status = UsbpHubGetPortStatus(Hub,
                                              PortIndex + 1,
                                              &HardwareStatus);

                if (!KSUCCESS(Status)) {
                    goto HubInterruptTransferCompletionWorkerEnd;
                }

                UsbpHubUpdatePortStatus(Hub, PortIndex, HardwareStatus);
            }

            //
            // Now wipe the port status and enable the power on the port.
            //

            RtlZeroMemory(PortStatus, sizeof(USB_PORT_STATUS));
            Hub->HubStatus.PortDeviceSpeed[PortIndex] = UsbDeviceSpeedInvalid;
            Status = UsbpHubEnablePortPower(Hub, PortIndex);
            if (!KSUCCESS(Status)) {
                goto HubInterruptTransferCompletionWorkerEnd;
            }

            //
            // Collect the status one more time after the power on. If there is
            // something behind the port then the connection changed bit should
            // get set.
            //

            Status = UsbpHubGetPortStatus(Hub, PortIndex + 1, &HardwareStatus);
            if (!KSUCCESS(Status)) {
                goto HubInterruptTransferCompletionWorkerEnd;
            }

            UsbpHubUpdatePortStatus(Hub, PortIndex, HardwareStatus);
        }

        //
        // Attempt to clear out any change bits.
        //

        Status = UsbpHubClearPortChangeBits(Hub,
                                            PortIndex + 1,
                                            HardwareStatus);

        if (!KSUCCESS(Status)) {
            goto HubInterruptTransferCompletionWorkerEnd;
        }

        //
        // If the connection status has changed, then notify the system of a
        // topology change.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_CONNECTED) != 0) {
            TopologyChanged = TRUE;
        }

        KeReleaseQueuedLock(Device->ChildLock);
        ChildLockHeld = FALSE;
    }

HubInterruptTransferCompletionWorkerEnd:
    if (ChildLockHeld != FALSE) {
        KeReleaseQueuedLock(Device->ChildLock);
    }

    //
    // If there was a topology change on the hub, notify the system.
    //

    if (TopologyChanged != FALSE) {
        UsbDevice = (PUSB_DEVICE)Hub->DeviceHandle;
        IoNotifyDeviceTopologyChange(UsbDevice->Device);
    }

    //
    // Resubmit the transfer even if this routine failed.
    //

    if (SubmitTransfer != FALSE) {
        Status = UsbSubmitTransfer(Hub->InterruptTransfer);

        ASSERT(KSUCCESS(Status));
    }

    return;
}

KSTATUS
UsbpHubClearPortChangeBits (
    PUSB_HUB Hub,
    ULONG PortNumber,
    ULONG PortStatus
    )

/*++

Routine Description:

    This routine communicates with the given hub to clear any change status
    bits set in the port status.

Arguments:

    Hub - Supplies a pointer to the hub that owns the port.

    PortNumber - Supplies the one-indexed port number (zero is not a valid
        value here).

    PortStatus - Supplies the port status bits. Any change bits set here will
        be cleared.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT(PortNumber != 0);

    //
    // Clear out any change bits.
    //

    PortStatus = PortStatus >> USB_HUB_PORT_STATUS_CHANGE_SHIFT;
    if ((PortStatus & USB_HUB_PORT_STATUS_DEVICE_CONNECTED) != 0) {
        Status = UsbpHubSetOrClearFeature(Hub,
                                          FALSE,
                                          USB_HUB_FEATURE_C_PORT_CONNECTION,
                                          PortNumber);

        if (!KSUCCESS(Status)) {
            goto HubClearPortChangeBits;
        }
    }

    if ((PortStatus & USB_HUB_PORT_STATUS_ENABLED) != 0) {
        Status = UsbpHubSetOrClearFeature(Hub,
                                          FALSE,
                                          USB_HUB_FEATURE_C_PORT_ENABLE,
                                          PortNumber);

        if (!KSUCCESS(Status)) {
            goto HubClearPortChangeBits;
        }
    }

    if ((PortStatus & USB_HUB_PORT_STATUS_SUSPENDED) != 0) {
        Status = UsbpHubSetOrClearFeature(Hub,
                                          FALSE,
                                          USB_HUB_FEATURE_C_PORT_SUSPEND,
                                          PortNumber);

        if (!KSUCCESS(Status)) {
            goto HubClearPortChangeBits;
        }
    }

    if ((PortStatus & USB_HUB_PORT_STATUS_OVER_CURRENT) != 0) {
        Status = UsbpHubSetOrClearFeature(
                                       Hub,
                                       FALSE,
                                       USB_HUB_FEATURE_C_PORT_OVER_CURRENT,
                                       PortNumber);

        if (!KSUCCESS(Status)) {
            goto HubClearPortChangeBits;
        }
    }

    if ((PortStatus & USB_HUB_PORT_STATUS_RESET) != 0) {
        Status = UsbpHubSetOrClearFeature(Hub,
                                          FALSE,
                                          USB_HUB_FEATURE_C_PORT_RESET,
                                          PortNumber);

        if (!KSUCCESS(Status)) {
            goto HubClearPortChangeBits;
        }
    }

    Status = STATUS_SUCCESS;

HubClearPortChangeBits:
    return Status;
}

VOID
UsbpHubAddDevice (
    PUSB_HUB Hub,
    ULONG PortIndex
    )

/*++

Routine Description:

    This routine attempts to add a device to the given hub at the given port
    index. It resets the port and then tries to enumerate a device.

Arguments:

    Hub - Supplies a pointer to the USB hub that is adding the new device.

    PortIndex - Supplies the index of the hub port at which the new device is
        to be added.

Return Value:

    None.

--*/

{

    PUSB_DEVICE Child;
    PUSB_DEVICE Device;
    PUSB_PORT_STATUS PortStatus;
    KSTATUS Status;

    Child = NULL;
    Device = (PUSB_DEVICE)Hub->DeviceHandle;

    ASSERT(KeIsQueuedLockHeld(Device->ChildLock) != FALSE);
    ASSERT(Hub->HubStatus.PortStatus != NULL);

    //
    // When the system last checked, there was a device present on this port.
    // Wait the minimum debounce interval according to section 7.1.7.3 of the
    // USB specification, and then recheck the state and proceed only if the
    // device is still present.
    //

    KeDelayExecution(FALSE, FALSE, 100 * MICROSECONDS_PER_MILLISECOND);

    //
    // Get the current hub status.
    //

    Status = UsbpGetHubStatus(Hub, TRUE);
    if (!KSUCCESS(Status)) {
        goto HubAddDevice;
    }

    //
    // If the device is not present, exit.
    //

    PortStatus = &(Hub->HubStatus.PortStatus[PortIndex]);

    ASSERT((PortStatus->Change & USB_PORT_STATUS_CHANGE_CONNECTED) != 0);

    if ((PortStatus->Status & USB_PORT_STATUS_CONNECTED) == 0) {
        Status = STATUS_SUCCESS;
        goto HubAddDevice;
    }

    //
    // Reset the port. If the device is still there after the reset, then
    // create a device.
    //

    Status = UsbpResetHubPort(Hub, PortIndex);
    if (!KSUCCESS(Status)) {
        goto HubAddDevice;
    }

    if ((PortStatus->Status & USB_PORT_STATUS_CONNECTED) != 0) {
        Status = UsbpEnumerateDevice(Hub,
                                     Device,
                                     PortIndex + 1,
                                     Hub->HubStatus.PortDeviceSpeed[PortIndex],
                                     (PVOID)&Child);

        if (!KSUCCESS(Status)) {
            goto HubAddDevice;
        }

        ASSERT(Child != NULL);

    }

HubAddDevice:
    return;
}

KSTATUS
UsbpHubEnablePortPower (
    PUSB_HUB Hub,
    ULONG PortIndex
    )

/*++

Routine Description:

    This routine enables power on a hub port.

Arguments:

    Hub - Supplies a pointer to a hub.

    PortIndex - Supplies the zero-based index of the port to be powered on.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = UsbpHubSetOrClearFeature(Hub,
                                      TRUE,
                                      USB_HUB_FEATURE_PORT_POWER,
                                      PortIndex + 1);

    if (!KSUCCESS(Status)) {
        goto HubEnablePortPowerEnd;
    }

    if (Hub->HasIndicators != FALSE) {
        Status = UsbpHubSetOrClearFeature(
                                Hub,
                                TRUE,
                                USB_HUB_FEATURE_PORT_INDICATOR,
                                (PortIndex + 1) | USB_HUB_INDICATOR_AUTOMATIC);

        if (!KSUCCESS(Status)) {
            goto HubEnablePortPowerEnd;
        }
    }

    //
    // Now that the port has been powered up, delay for the appropriate
    // amount of time before accessing it again.
    //

    KeDelayExecution(FALSE,
                     FALSE,
                     Hub->PowerUpDelayIn2ms * 2 * MICROSECONDS_PER_MILLISECOND);

HubEnablePortPowerEnd:
    return Status;
}

