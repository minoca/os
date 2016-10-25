/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    enum.c

Abstract:

    This module implements device enumeration for the USB core.

Author:

    Evan Green 16-Jan-2013

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
// Define the format of device identifiers that USB presents devices with.
//

#define USB_DEVICE_ID_LENGTH 20
#define USB_DEVICE_ID_FORMAT "VID_%04X&PID_%04X"
#define USB_DEVICE_ID_WITH_INTERFACE_FORMAT "VID_%04X&PID_%04X_%02X"

//
// Define the number of times an enumeration request will be made before
// declaring that it really doesn't work.
//

#define USB_ENUMERATION_TRANSFER_TRY_COUNT 5

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UsbpEnumerateRootHub (
    PUSB_HOST_CONTROLLER Controller
    );

PUSB_DEVICE
UsbpCreateDevice (
    USB_DEVICE_SPEED DeviceSpeed,
    PUSB_DEVICE ParentDevice,
    UCHAR PortNumber,
    PUSB_HOST_CONTROLLER ParentController
    );

VOID
UsbpDestroyDevice (
    PUSB_DEVICE Device
    );

KSTATUS
UsbpGetDeviceDescriptor (
    PUSB_DEVICE Device,
    PUSB_DEVICE_DESCRIPTOR DeviceDescriptor,
    BOOL FirstEightBytesOnly
    );

KSTATUS
UsbpAssignDeviceAddress (
    PUSB_DEVICE Device
    );

VOID
UsbpUnassignDeviceAddress (
    PUSB_DEVICE Device
    );

KSTATUS
UsbpReadDeviceStrings (
    PUSB_DEVICE Device,
    PUSB_DEVICE_DESCRIPTOR DeviceDescriptor
    );

PSTR
UsbpCreateAnsiStringFromStringDescriptor (
    PUSB_STRING_DESCRIPTOR StringDescriptor
    );

VOID
UsbpGetDeviceClass (
    PUSB_DEVICE Device,
    PUCHAR Class,
    PUCHAR Subclass,
    PUCHAR Protocol
    );

KSTATUS
UsbpCreateOsDevice (
    PUSB_DEVICE Device,
    UCHAR Class,
    UCHAR Subclass,
    UCHAR Protocol,
    UCHAR Interface,
    BOOL InterfaceDevice,
    PDEVICE *CreatedDevice
    );

UCHAR
UsbpGetReservedDeviceAddress (
    PUSB_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

USB_API
KSTATUS
UsbHostQueryChildren (
    PIRP Irp,
    HANDLE UsbDeviceHandle
    )

/*++

Routine Description:

    This routine responds to the Query Children IRP for a USB Host controller.

Arguments:

    Irp - Supplies a pointer to the Query Children IRP.

    UsbDeviceHandle - Supplies a pointer to the USB Host controller handle.

Return Value:

    Status code.

--*/

{

    PUSB_HOST_CONTROLLER Controller;
    KSTATUS Status;

    Controller = (PUSB_HOST_CONTROLLER)UsbDeviceHandle;

    ASSERT(Controller != NULL);

    //
    // If the root hub's device has never before been created, create it now.
    //

    if (Controller->RootDevice == NULL) {
        Status = UsbpEnumerateRootHub(Controller);
        if (!KSUCCESS(Status)) {
            goto HostQueryChildrenEnd;
        }
    }

    ASSERT((Controller->RootDevice != NULL) &&
           (Controller->RootDevice->Device != NULL));

    //
    // Merge whatever is in the IRP with the enumeration of this root hub.
    //

    Status = IoMergeChildArrays(Irp,
                                &(Controller->RootDevice->Device),
                                1,
                                USB_CORE_ALLOCATION_TAG);

    if (!KSUCCESS(Status)) {
        goto HostQueryChildrenEnd;
    }

HostQueryChildrenEnd:
    return Status;
}

USB_API
KSTATUS
UsbDriverAttach (
    PDEVICE Device,
    PDRIVER Driver,
    PHANDLE UsbCoreHandle
    )

/*++

Routine Description:

    This routine attaches a USB driver to a USB device, and returns a USB
    core handle to the device, used for all USB communications. This routine
    must be called at low level.

Arguments:

    Device - Supplies a pointer to the OS device object representation of the
        USB device.

    Driver - Supplies a pointer to the driver that will take ownership of the
        device.

    UsbCoreHandle - Supplies a pointer where the USB Core device handle will
        be returned.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE CurrentDevice;
    PLIST_ENTRY CurrentEntry;
    PUSB_DEVICE FoundDevice;
    PUSB_INTERFACE Interface;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    FoundDevice = NULL;
    *UsbCoreHandle = INVALID_HANDLE;
    if (Driver == NULL) {
        return STATUS_ARGUMENT_EXPECTED;
    }

    //
    // Loop through all USB controllers.
    //

    Status = STATUS_NOT_FOUND;
    KeAcquireQueuedLock(UsbDeviceListLock);
    CurrentEntry = UsbDeviceList.Next;
    while (CurrentEntry != &UsbDeviceList) {
        CurrentDevice = LIST_VALUE(CurrentEntry, USB_DEVICE, GlobalListEntry);

        ASSERT(CurrentEntry != NULL);

        CurrentEntry = CurrentEntry->Next;

        //
        // Check to see if the driver is attaching to the current device.
        //

        if (CurrentDevice->Device == Device) {
            FoundDevice = CurrentDevice;
            if (FoundDevice->Driver == NULL) {
                FoundDevice->Driver = Driver;
                Status = STATUS_SUCCESS;
            }

            break;
        }

        //
        // Check all the interfaces of the current configuration to see if the
        // driver is actually just attaching to an interface.
        //

        Interface = (PUSB_INTERFACE)UsbGetDesignatedInterface(Device,
                                                              CurrentDevice);

        if (Interface != NULL) {
            FoundDevice = CurrentDevice;

            ASSERT(Interface->Driver == NULL);

            Interface->Driver = Driver;
            Status = STATUS_SUCCESS;
            break;
        }
    }

    KeReleaseQueuedLock(UsbDeviceListLock);

    //
    // Only a device's removel IRP marks the device as disconnected. Since the
    // removal IRP is the last action a device can take, it is safe to assume
    // that the attempt to open the device here will succeed.
    //

    if (FoundDevice != NULL) {

        ASSERT(FoundDevice->Connected != FALSE);

        *UsbCoreHandle = UsbDeviceOpen(FoundDevice);

        ASSERT(*UsbCoreHandle != INVALID_HANDLE);

    }

    return Status;
}

USB_API
KSTATUS
UsbEnumerateDeviceForInterface (
    HANDLE UsbCoreHandle,
    PUSB_INTERFACE_DESCRIPTION InterfaceDescription,
    PDEVICE *ChildDevice
    )

/*++

Routine Description:

    This routine enumerates a child OS device on the requested device and
    interface combination. With this interface multiple drivers can
    independently operate interfaces of a shared USB device.

Arguments:

    UsbCoreHandle - Supplies the core handle to the device containing the
        interface to share.

    InterfaceDescription - Supplies a pointer to the interface to enumerate a
        device for.

    ChildDevice - Supplies a pointer to an OS device that will come up to
        claim the given interface. This device should be returned in Query
        Children calls sent to the parent device so the device can properly
        enumerate.

Return Value:

    Status code.

--*/

{

    UCHAR Class;
    PUSB_DEVICE Device;
    PUSB_INTERFACE Interface;
    UCHAR InterfaceNumber;
    UCHAR Protocol;
    KSTATUS Status;
    UCHAR Subclass;

    Device = (PUSB_DEVICE)UsbCoreHandle;
    Interface = (PUSB_INTERFACE)InterfaceDescription;
    if (Interface->Device != NULL) {
        *ChildDevice = Interface->Device;
        Status = STATUS_SUCCESS;
        goto EnumerateDeviceForInterfaceEnd;
    }

    Class = InterfaceDescription->Descriptor.Class;
    Subclass = InterfaceDescription->Descriptor.Subclass;
    Protocol = InterfaceDescription->Descriptor.Protocol;
    InterfaceNumber = InterfaceDescription->Descriptor.InterfaceNumber;

    ASSERT(Device->DebugDevice == FALSE);

    Status = UsbpCreateOsDevice(Device,
                                Class,
                                Subclass,
                                Protocol,
                                InterfaceNumber,
                                TRUE,
                                ChildDevice);

    if (!KSUCCESS(Status)) {
        goto EnumerateDeviceForInterfaceEnd;
    }

    Interface->Device = *ChildDevice;
    Status = STATUS_SUCCESS;

EnumerateDeviceForInterfaceEnd:
    if (!KSUCCESS(Status)) {
        *ChildDevice = NULL;
    }

    return Status;
}

USB_API
PUSB_INTERFACE_DESCRIPTION
UsbGetDesignatedInterface (
    PDEVICE Device,
    HANDLE UsbCoreHandle
    )

/*++

Routine Description:

    This routine returns the interface for which the given pseudo-device was
    enumerated. This routine is used by general class drivers (like Hub or
    Mass Storage) that can interact with an interface without necessarily
    taking responsibility for the entire device.

Arguments:

    Device - Supplies a pointer to the OS device object representation of the
        USB device.

    UsbCoreHandle - Supplies the core handle to the device.

Return Value:

    Returns a pointer to the interface this pseudo-device is supposed to take
    ownership of. If the device only has one interface, then that interface is
    returned.

    NULL if the OS device was not enumerated for any one particular interface.

--*/

{

    PUSB_CONFIGURATION Configuration;
    PUSB_INTERFACE FoundInterface;
    PUSB_INTERFACE Interface;
    PLIST_ENTRY InterfaceEntry;
    PLIST_ENTRY InterfaceListHead;
    PUSB_DEVICE UsbDevice;

    UsbDevice = (PUSB_DEVICE)UsbCoreHandle;
    if (UsbCoreHandle == INVALID_HANDLE) {
        return NULL;
    }

    FoundInterface = NULL;
    Configuration = UsbDevice->ActiveConfiguration;
    if (Configuration == NULL) {
        return NULL;
    }

    //
    // If there's only one interface, return that one.
    //

    InterfaceListHead = &(Configuration->Description.InterfaceListHead);

    ASSERT(LIST_EMPTY(InterfaceListHead) == FALSE);

    InterfaceEntry = InterfaceListHead->Next;

    //
    // If this is the main device attached to the USB device, just give it the
    // first interface.
    //

    if (UsbDevice->Device == Device) {
        FoundInterface = LIST_VALUE(InterfaceEntry,
                                    USB_INTERFACE,
                                    Description.ListEntry);

        return (PUSB_INTERFACE_DESCRIPTION)&(FoundInterface->Description);
    }

    //
    // Loop through all the interfaces looking for the one associated with this
    // device.
    //

    while (InterfaceEntry != InterfaceListHead) {
        Interface = LIST_VALUE(InterfaceEntry,
                               USB_INTERFACE,
                               Description.ListEntry);

        if (Interface->Device == Device) {
            FoundInterface = Interface;
            break;
        }

        InterfaceEntry = InterfaceEntry->Next;
    }

    return (PUSB_INTERFACE_DESCRIPTION)&(FoundInterface->Description);
}

USB_API
KSTATUS
UsbGetDeviceSpeed (
    PUSB_DEVICE Device,
    PUSB_DEVICE_SPEED Speed
    )

/*++

Routine Description:

    This routine returns the connected speed of the given USB device.

Arguments:

    Device - Supplies a pointer to the device.

    Speed - Supplies a pointer where the device speed will be returned.

Return Value:

    Status code.

--*/

{

    *Speed = Device->Speed;
    return STATUS_SUCCESS;
}

USB_API
VOID
UsbDetachDevice (
    HANDLE UsbCoreHandle
    )

/*++

Routine Description:

    This routine detaches a USB device from the USB core by marking it as
    disconnected, and cancelling all active transfers belonging to the device.
    It does not close the device.

Arguments:

    UsbCoreHandle - Supplies the core handle to the device that is to be
        removed.

Return Value:

    None.

--*/

{

    PUSB_DEVICE Device;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(UsbCoreHandle != INVALID_HANDLE);

    Device = (PUSB_DEVICE)UsbCoreHandle;

    //
    // Acquire the device's lock that protects the status and transfer list in
    // order to synchronize with transfer submission and deletion.
    //

    KeAcquireQueuedLock(Device->Lock);

    //
    // Mark the device as disconnected. Mark this before cancelling the
    // transfer so that no new transfers cannot be submitted.
    //

    Device->Connected = FALSE;
    KeReleaseQueuedLock(Device->Lock);

    //
    // Cancel all of the device's transfers.
    //

    UsbpCancelAllTransfers(UsbCoreHandle);
    return;
}

USB_API
KSTATUS
UsbReadDeviceString (
    PUSB_DEVICE Device,
    UCHAR StringNumber,
    USHORT Language,
    PUSB_STRING_DESCRIPTOR Buffer
    )

/*++

Routine Description:

    This routine reads a string descriptor from a USB device.

Arguments:

    Device - Supplies a pointer to the device to read from.

    StringNumber - Supplies the string descriptor index of the string to read.

    Language - Supplies the language code.

    Buffer - Supplies a pointer where the string descriptor and data will be
        returned. This buffer must be the size of the maximum string descriptor,
        which is 256 bytes.

Return Value:

    Status code.

--*/

{

    ULONG LengthTransferred;
    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;
    ULONG Try;

    //
    // Initialize the setup packet. Send the request once with just a single
    // letter's worth of space to get the real size. Some devices don't like
    // it when the length is greater than the actual string they want to send.
    //

    RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
    SetupPacket.RequestType = USB_SETUP_REQUEST_TO_HOST |
                              USB_SETUP_REQUEST_STANDARD |
                              USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    SetupPacket.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    SetupPacket.Index = Language;
    SetupPacket.Length = sizeof(USB_STRING_DESCRIPTOR) + 2;
    SetupPacket.Value = (UsbDescriptorTypeString << 8) | StringNumber;
    for (Try = 0; Try < USB_ENUMERATION_TRANSFER_TRY_COUNT; Try += 1) {
        Status = UsbSendControlTransfer(Device,
                                        UsbTransferDirectionIn,
                                        &SetupPacket,
                                        Buffer,
                                        SetupPacket.Length,
                                        &LengthTransferred);

        if (!KSUCCESS(Status)) {
            if ((UsbDebugFlags &
                 (USB_DEBUG_ENUMERATION | USB_DEBUG_ERRORS)) != 0) {

                RtlDebugPrint("USB: Failed to read (small) string %d "
                              "(language 0x%x) from device 0x%x: status %d,"
                              "try %d.\n",
                              StringNumber,
                              Language,
                              Device,
                              Status,
                              Try + 1);
            }
        }

        if (KSUCCESS(Status)) {
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        if ((UsbDebugFlags & (USB_DEBUG_ENUMERATION | USB_DEBUG_ERRORS)) != 0) {
            RtlDebugPrint("USB: ReadDeviceString Giving up.\n");
        }

        goto ReadDeviceStringEnd;
    }

    //
    // If the string descriptor header was not fully read, exit.
    //

    if (LengthTransferred < sizeof(USB_STRING_DESCRIPTOR)) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ReadDeviceStringEnd;
    }

    //
    // Now read it for real with the correct size.
    //

    SetupPacket.Length = Buffer->Length;
    for (Try = 0; Try < USB_ENUMERATION_TRANSFER_TRY_COUNT; Try += 1) {
        Status = UsbSendControlTransfer(Device,
                                        UsbTransferDirectionIn,
                                        &SetupPacket,
                                        Buffer,
                                        SetupPacket.Length,
                                        &LengthTransferred);

        if (!KSUCCESS(Status)) {
            if ((UsbDebugFlags &
                 (USB_DEBUG_ENUMERATION | USB_DEBUG_ERRORS)) != 0) {

                RtlDebugPrint("USB: Failed to read string %d (language 0x%x) "
                              "from device 0x%x: status %d, try %d\n",
                              StringNumber,
                              Language,
                              Device,
                              Status,
                              Try);
            }
        }

        if (KSUCCESS(Status)) {
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        if ((UsbDebugFlags & (USB_DEBUG_ENUMERATION | USB_DEBUG_ERRORS)) != 0) {
            RtlDebugPrint("USB: ReadDeviceString Giving up.\n");
        }

        goto ReadDeviceStringEnd;
    }

ReadDeviceStringEnd:
    return Status;
}

VOID
UsbpDeviceAddReference (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine increments the reference count on the given device.

Arguments:

    Device - Supplies a pointer to the device whose reference count should be
        incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Device->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x1000));

    return;
}

VOID
UsbpDeviceReleaseReference (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine decrements the reference count on the given device, and
    destroys it if it hits zero.

Arguments:

    Device - Supplies a pointer to the device whose reference count should be
        decremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Device->ReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x1000));

    if (OldReferenceCount == 1) {
        UsbpDestroyDevice(Device);
    }

    return;
}

KSTATUS
UsbpEnumerateDevice (
    PUSB_HUB ParentHub,
    PUSB_DEVICE ParentHubDevice,
    UCHAR PortNumber,
    USB_DEVICE_SPEED DeviceSpeed,
    PHANDLE DeviceHandle
    )

/*++

Routine Description:

    This routine creates a new USB device in the system. This routine must be
    called at low level, and must be called with the parent hub's child lock
    held.

Arguments:

    ParentHub - Supplies a pointer to the parent hub object.

    ParentHubDevice - Supplies the handle of the parent USB hub device
        enumerating the new device.

    PortNumber - Supplies the parent hub's one-based port number where this
        device exists.

    DeviceSpeed - Supplies the speed of the device being enumerated.

    DeviceHandle - Supplies a pointer where a handle representing the device
        will be returned upon success.

Return Value:

    Status code.

--*/

{

    UCHAR Class;
    UCHAR Configuration;
    PUSB_DEVICE Device;
    USB_DEVICE_DESCRIPTOR DeviceDescriptor;
    UCHAR Protocol;
    KSTATUS Status;
    UCHAR Subclass;
    ULONG Try;

    Device = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(KeIsQueuedLockHeld(ParentHubDevice->ChildLock) != FALSE);

    //
    // Acquire the parent device's controller lock to synchronize access to
    // address zero.
    //

    KeAcquireQueuedLock(ParentHubDevice->Controller->Lock);

    //
    // Create the child device.
    //

    if ((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) {
        RtlDebugPrint("USB: Creating device on hub 0x%x port %d.\n",
                      ParentHubDevice,
                      PortNumber);
    }

    Device = UsbpCreateDevice(DeviceSpeed,
                              ParentHubDevice,
                              PortNumber,
                              ParentHubDevice->Controller);

    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumerateDeviceEnd;
    }

    //
    // Attempt to establish communication with the device by asking for the
    // first 8 bytes of the device descriptor, which contain the maximum
    // packet size.
    //

    for (Try = 0; Try < USB_ENUMERATION_TRANSFER_TRY_COUNT; Try += 1) {
        RtlZeroMemory(&DeviceDescriptor, sizeof(USB_DEVICE_DESCRIPTOR));
        Status = UsbpGetDeviceDescriptor(Device, &DeviceDescriptor, TRUE);
        if (((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) ||
            ((!KSUCCESS(Status)) &&
             ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

            RtlDebugPrint("USB: GetDeviceDescriptor try %d on device 0x%x, "
                          "Status %d.\n",
                          Try + 1,
                          Device,
                          Status);
        }

        if (KSUCCESS(Status)) {
            break;
        }

        HlBusySpin(50 * MICROSECONDS_PER_MILLISECOND);
    }

    if (!KSUCCESS(Status)) {
        goto EnumerateDeviceEnd;
    }

    //
    // Reset the device again.
    //

    Status = UsbpResetHubPort(ParentHub, PortNumber - 1);
    if (!KSUCCESS(Status)) {
        if ((UsbDebugFlags & (USB_DEBUG_ENUMERATION | USB_DEBUG_ERRORS)) != 0) {
            RtlDebugPrint("USB: Hub 0x%x Port %d failed to reset.\n",
                          ParentHubDevice,
                          PortNumber);
        }

        goto EnumerateDeviceEnd;
    }

    //
    // Reset the endpoint to get the newly found max packet size all the way
    // down into the host controller.
    //

    UsbpResetEndpoint(Device, Device->EndpointZero);

    //
    // Request the entire device descriptor.
    //

    for (Try = 0; Try < USB_ENUMERATION_TRANSFER_TRY_COUNT; Try += 1) {
        Status = UsbpGetDeviceDescriptor(Device, &DeviceDescriptor, FALSE);
        if (((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) ||
            ((!KSUCCESS(Status)) &&
             ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

            RtlDebugPrint("USB: GetDeviceDescriptor2 Try %d on device %x, "
                          "Status %d.\n",
                          Try + 1,
                          Device,
                          Status);
        }

        if (KSUCCESS(Status)) {
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        goto EnumerateDeviceEnd;
    }

    //
    // Assign the device an address.
    //

    Status = UsbpAssignDeviceAddress(Device);
    if (((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) ||
        ((!KSUCCESS(Status)) &&
         ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

        RtlDebugPrint("USB: AssignDeviceAddress on device 0x%x, Status %d.\n",
                      Device,
                      Status);
    }

    if (!KSUCCESS(Status)) {
        goto EnumerateDeviceEnd;
    }

    //
    // Remember if the device is a hub.
    //

    if (DeviceDescriptor.Class == UsbDeviceClassHub) {
        Device->Type = UsbDeviceTypeHub;
    }

    Device->ConfigurationCount = DeviceDescriptor.ConfigurationCount;

    //
    // Attempt to read the interesting device strings.
    //

    Status = UsbpReadDeviceStrings(Device, &DeviceDescriptor);
    if (((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) ||
        ((!KSUCCESS(Status)) &&
         ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

        RtlDebugPrint("USB: ReadDeviceStrings on device 0x%x, Status %d.\n",
                      Device,
                      Status);
    }

    if (!KSUCCESS(Status)) {
        goto EnumerateDeviceEnd;
    }

    //
    // Read the configuration descriptors.
    //

    for (Try = 0; Try < USB_ENUMERATION_TRANSFER_TRY_COUNT; Try += 1) {
        Status = UsbpReadConfigurationDescriptors(Device, &DeviceDescriptor);
        if (((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) ||
            ((!KSUCCESS(Status)) &&
             ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

            RtlDebugPrint("USB: ReadConfigurationDescriptors on device 0x%x, "
                          "Status %d.\n",
                          Device,
                          Status);
        }

        if (KSUCCESS(Status)) {
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        goto EnumerateDeviceEnd;
    }

    //
    // If this is the debug device, avoid exposing it to the operating system,
    // as the debugger is using it.
    //

    if (Device->DebugDevice != FALSE) {
        Configuration = Device->Controller->HandoffData->U.Usb.Configuration;
        Status = UsbSetConfiguration(Device, Configuration, FALSE);
        if (((UsbDebugFlags & USB_DEBUG_DEBUGGER_HANDOFF) != 0) ||
            ((!KSUCCESS(Status)) &&
             ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

            RtlDebugPrint("USB: Set configuration %d for debug device 0x%x: "
                          "%d.\n",
                          Configuration,
                          Device,
                          Status);
        }

        if (!KSUCCESS(Status)) {
            goto EnumerateDeviceEnd;
        }

        //
        // The debug device is back online, reconnect!
        //

        KdConnect();

    } else {

        //
        // Now that the device is properly enumerated, expose it to the
        // operating system.
        //

        UsbpGetDeviceClass(Device, &Class, &Subclass, &Protocol);
        Status = UsbpCreateOsDevice(Device,
                                    Class,
                                    Subclass,
                                    Protocol,
                                    0,
                                    FALSE,
                                    &(Device->Device));

        if (((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) ||
            ((!KSUCCESS(Status)) &&
             ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

            RtlDebugPrint("USB: CreateOsDevice on device 0x%x, Status %d.\n",
                          Device,
                          Status);
        }

        if (!KSUCCESS(Status)) {
            goto EnumerateDeviceEnd;
        }
    }

    //
    // Add the device to the list of children.
    //

    INSERT_BEFORE(&(Device->ListEntry), &(ParentHubDevice->ChildList));
    Status = STATUS_SUCCESS;
    if ((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) {
        RtlDebugPrint("USB: Enumeration complete for device 0x%x.\n", Device);
    }

EnumerateDeviceEnd:
    KeReleaseQueuedLock(ParentHubDevice->Controller->Lock);
    if (!KSUCCESS(Status)) {
        if (Device != NULL) {

            //
            // Remove the device.
            //

            ASSERT(Device->ReferenceCount == 1);

            UsbpRemoveDevice(Device);
            Device = NULL;
        }
    }

    if (Device == NULL) {
        *DeviceHandle = INVALID_HANDLE;

    } else {
        *DeviceHandle = (HANDLE)Device;
    }

    return Status;
}

VOID
UsbpRemoveDevice (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine removes a device from its parent hub. The parent USB device's
    child lock should be held.

Arguments:

    Device - Supplies a pointer to the device that is to be removed.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Device->Parent->Type != UsbDeviceTypeNonHub);
    ASSERT(KeIsQueuedLockHeld(Device->Parent->ChildLock) != FALSE);

    //
    // Remove the device from the parent's list.
    //

    if (Device->ListEntry.Next != NULL) {
        LIST_REMOVE(&(Device->ListEntry));
        Device->ListEntry.Next = NULL;
    }

    //
    // Remove the device from the global list.
    //

    if (Device->GlobalListEntry.Next != NULL) {
        KeAcquireQueuedLock(UsbDeviceListLock);
        LIST_REMOVE(&(Device->GlobalListEntry));
        KeReleaseQueuedLock(UsbDeviceListLock);
        Device->GlobalListEntry.Next = NULL;
    }

    //
    // Release the reference on the device that the hub took during
    // enumeration.
    //

    UsbpDeviceReleaseReference(Device);
    return;
}

KSTATUS
UsbpReserveDeviceAddress (
    PUSB_HOST_CONTROLLER Controller,
    PUSB_DEVICE Device,
    UCHAR Address
    )

/*++

Routine Description:

    This routine assigns the given device to a specific address.

Arguments:

    Controller - Supplies a pointer to the controller the device lives on.

    Device - Supplies a pointer to the USB device reserving the address. This
        can be NULL.

    Address - Supplies the address to reserve.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

    STATUS_RESOURCE_IN_USE if the address is already assigned.

--*/

{

    ULONG AllocationSize;
    PUSB_DEVICE *Segment;
    ULONG SegmentIndex;
    ULONG SegmentOffset;

    SegmentIndex = Address / USB_HOST_ADDRESSES_PER_SEGMENT;
    if (SegmentIndex >= USB_HOST_ADDRESS_SEGMENT_COUNT) {
        return STATUS_INVALID_PARAMETER;
    }

    Segment = Controller->ChildrenByAddress[SegmentIndex];

    //
    // If the segment is not yet allocated, allocate it now.
    //

    if (Segment == NULL) {
        AllocationSize = sizeof(PUSB_DEVICE) * USB_HOST_ADDRESSES_PER_SEGMENT;
        Segment = MmAllocateNonPagedPool(AllocationSize,
                                         USB_CORE_ALLOCATION_TAG);

        if (Segment == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(Segment, AllocationSize);
        Controller->ChildrenByAddress[SegmentIndex] = Segment;
    }

    //
    // Fail if there's already something valid in that slot.
    //

    SegmentOffset = Address % USB_HOST_ADDRESSES_PER_SEGMENT;
    if ((Segment[SegmentOffset] != NULL) &&
        (Segment[SegmentOffset] != (PUSB_DEVICE)-1)) {

        return STATUS_RESOURCE_IN_USE;
    }

    //
    // Reserve it.
    //

    if (Device == NULL) {
        Segment[SegmentOffset] = (PUSB_DEVICE)-1;

    } else {
        Segment[SegmentOffset] = Device;
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UsbpEnumerateRootHub (
    PUSB_HOST_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine enumerates a root hub off of a host controller.

Arguments:

    Controller - Supplies a pointer to the host controller.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE RootDevice;
    KSTATUS Status;

    ASSERT(Controller->RootDevice == NULL);

    //
    // Create a USB device structure.
    //

    RootDevice = UsbpCreateDevice(Controller->Device.Speed,
                                  NULL,
                                  0,
                                  Controller);

    if (RootDevice == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto EnumerateRootHubEnd;
    }

    RootDevice->Type = UsbDeviceTypeRootHub;
    Controller->RootDevice = RootDevice;

    //
    // Create the OS device to go with the USB device.
    //

    Status = IoCreateDevice(Controller->Device.DriverObject,
                            NULL,
                            Controller->Device.DeviceObject,
                            USB_ROOT_HUB_DEVICE_ID,
                            NULL,
                            NULL,
                            &(RootDevice->Device));

    if (!KSUCCESS(Status)) {
        goto EnumerateRootHubEnd;
    }

    Status = STATUS_SUCCESS;

EnumerateRootHubEnd:
    if (!KSUCCESS(Status)) {
        if (RootDevice != NULL) {

            ASSERT(RootDevice->ReferenceCount == 1);

            UsbpRemoveDevice(RootDevice);
        }
    }

    return Status;
}

PUSB_DEVICE
UsbpCreateDevice (
    USB_DEVICE_SPEED DeviceSpeed,
    PUSB_DEVICE ParentDevice,
    UCHAR PortNumber,
    PUSB_HOST_CONTROLLER ParentController
    )

/*++

Routine Description:

    This routine allocates and initializes a new USB device structure. This
    routine must be called at low level.

Arguments:

    DeviceSpeed - Supplies the speed of the device being enumerated.

    ParentDevice - Supplies an optional pointer to the hub device enumerating
        this device.

    PortNumber - Supplies the parent hub's one-based port number where this
        device exists.

    ParentController - Supplies a pointer to the host controller that this
        device descends from.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    ULONG MaxPacketSize;
    KSTATUS Status;

    Device = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(DeviceSpeed != UsbDeviceSpeedInvalid);
    ASSERT((ParentDevice == NULL) || (PortNumber != 0));

    //
    // It is illegal to enumerate a child object with a different parent host
    // controller.
    //

    ASSERT((ParentDevice == NULL) ||
           (ParentDevice->Controller == ParentController));

    //
    // Create a device structure.
    //

    Device = MmAllocateNonPagedPool(sizeof(USB_DEVICE),
                                    USB_CORE_ALLOCATION_TAG);

    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDeviceEnd;
    }

    RtlZeroMemory(Device, sizeof(USB_DEVICE));
    INITIALIZE_LIST_HEAD(&(Device->ChildList));
    INITIALIZE_LIST_HEAD(&(Device->ConfigurationList));
    INITIALIZE_LIST_HEAD(&(Device->TransferList));
    Device->ReferenceCount = 1;
    Device->Speed = DeviceSpeed;
    Device->Controller = ParentController;
    Device->Parent = ParentDevice;
    Device->PortNumber = PortNumber;
    Device->Depth = 0;
    if (ParentDevice != NULL) {
        Device->Depth = ParentDevice->Depth + 1;
    }

    Device->ChildLock = KeCreateQueuedLock();
    if (Device->ChildLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDeviceEnd;
    }

    Device->ConfigurationLock = KeCreateQueuedLock();
    if (Device->ConfigurationLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDeviceEnd;
    }

    Device->Lock = KeCreateQueuedLock();
    if (Device->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDeviceEnd;
    }

    ASSERT(Device->ListEntry.Next == NULL);
    ASSERT(Device->GlobalListEntry.Next == NULL);

    //
    // Create the default control endpoint.
    //

    MaxPacketSize = 8;
    Status = UsbpCreateEndpoint(Device,
                                0,
                                UsbTransferBidirectional,
                                UsbTransferTypeControl,
                                MaxPacketSize,
                                0,
                                &(Device->EndpointZero));

    if (!KSUCCESS(Status)) {
        goto CreateDeviceEnd;
    }

    //
    // Mark the device as connected before adding it to the global list. The
    // device needs to be marked connected for transfers to be submitted.
    //

    Device->Connected = TRUE;

    //
    // Insert the device onto the global list.
    //

    KeAcquireQueuedLock(UsbDeviceListLock);
    INSERT_AFTER(&(Device->GlobalListEntry), &UsbDeviceList);
    KeReleaseQueuedLock(UsbDeviceListLock);
    Status = STATUS_SUCCESS;

CreateDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device != NULL) {
            if (Device->EndpointZero != NULL) {

                ASSERT(Device->EndpointZero->ReferenceCount == 1);

                UsbpEndpointReleaseReference(Device, Device->EndpointZero);
            }

            if (Device->ChildLock != NULL) {
                KeDestroyQueuedLock(Device->ChildLock);
            }

            if (Device->ConfigurationLock != NULL) {
                KeDestroyQueuedLock(Device->ConfigurationLock);
            }

            if (Device->Lock != NULL) {
                KeDestroyQueuedLock(Device->Lock);
            }

            MmFreeNonPagedPool(Device);
            Device = NULL;
        }
    }

    return Device;
}

VOID
UsbpDestroyDevice (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine releases the memory associated with a USB device. It is
    assumed that the device is already pulled off of all lists to which it
    belonged. This routine must be called at low level.

Arguments:

    Device - Supplies a pointer to the device to release.

Return Value:

    None.

--*/

{

    PUSB_CONFIGURATION Configuration;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Assert that all references have been released, the device has no more
    // children and that it has no more transfers.
    //

    ASSERT(Device->ReferenceCount == 0);
    ASSERT(LIST_EMPTY(&(Device->ChildList)) != FALSE);
    ASSERT(LIST_EMPTY(&(Device->TransferList)) != FALSE);

    //
    // Unassign the device's bus address.
    //

    if (Device->Type != UsbDeviceTypeRootHub) {
        UsbpUnassignDeviceAddress(Device);
    }

    //
    // Release the reference taken on the endpoint.
    //

    UsbpEndpointReleaseReference(Device, Device->EndpointZero);

    //
    // Release all cached configurations.
    //

    while (LIST_EMPTY(&(Device->ConfigurationList)) == FALSE) {
        Configuration = LIST_VALUE(Device->ConfigurationList.Next,
                                   USB_CONFIGURATION,
                                   ListEntry);

        LIST_REMOVE(&(Configuration->ListEntry));
        MmFreePagedPool(Configuration);
    }

    //
    // Destroy all other structures.
    //

    KeDestroyQueuedLock(Device->Lock);
    KeDestroyQueuedLock(Device->ConfigurationLock);
    KeDestroyQueuedLock(Device->ChildLock);
    if (Device->Manufacturer != NULL) {
        MmFreePagedPool(Device->Manufacturer);
    }

    if (Device->ProductName != NULL) {
        MmFreePagedPool(Device->ProductName);
    }

    if (Device->SerialNumber != NULL) {
        MmFreePagedPool(Device->SerialNumber);
    }

    MmFreeNonPagedPool(Device);
    return;
}

KSTATUS
UsbpGetDeviceDescriptor (
    PUSB_DEVICE Device,
    PUSB_DEVICE_DESCRIPTOR DeviceDescriptor,
    BOOL FirstEightBytesOnly
    )

/*++

Routine Description:

    This routine attempts to get the device descriptor out of a new USB device.

Arguments:

    Device - Supplies a pointer to the device to query.

    DeviceDescriptor - Supplies a pointer where the device descriptor will be
        returned.

    FirstEightBytesOnly - Supplies a boolean indicating if only the first 8
        bytes of the device descriptor should be retrieved.

Return Value:

    Status code.

--*/

{

    ULONG LengthTransferred;
    USB_SETUP_PACKET Setup;
    KSTATUS Status;

    //
    // Create the setup packet to get the device descriptor.
    //

    RtlZeroMemory(&Setup, sizeof(USB_SETUP_PACKET));
    Setup.RequestType = USB_SETUP_REQUEST_TO_HOST |
                        USB_SETUP_REQUEST_STANDARD |
                        USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    Setup.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    Setup.Value = UsbDescriptorTypeDevice << 8;
    Setup.Index = 0;
    if (FirstEightBytesOnly != FALSE) {
        Setup.Length = 8;

    } else {
        Setup.Length = sizeof(USB_DEVICE_DESCRIPTOR);
    }

    Status = UsbSendControlTransfer(Device,
                                    UsbTransferDirectionIn,
                                    &Setup,
                                    DeviceDescriptor,
                                    Setup.Length,
                                    &LengthTransferred);

    if (!KSUCCESS(Status)) {
        goto GetDeviceDescriptorEnd;
    }

    if (LengthTransferred != Setup.Length) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto GetDeviceDescriptorEnd;
    }

    //
    // Save the values just grabbed into the device if they were retrieved.
    // If only the first 8 bytes were grabbed, that's enough to determine the
    // max packet size so that the rest of the device descriptor can be
    // requested next.
    //

    Device->EndpointZero->MaxPacketSize = DeviceDescriptor->MaxPacketSize;
    if (FirstEightBytesOnly == FALSE) {
        Device->VendorId = DeviceDescriptor->VendorId;
        Device->ProductId = DeviceDescriptor->ProductId;
        Device->ClassCode = DeviceDescriptor->Class;
        Device->SubclassCode = DeviceDescriptor->Subclass;
        Device->ProtocolCode = DeviceDescriptor->Protocol;
    }

    Status = STATUS_SUCCESS;

GetDeviceDescriptorEnd:
    return Status;
}

KSTATUS
UsbpAssignDeviceAddress (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine assigns a new address to the USB device. This routine must be
    called at low level, and assumes the controller lock is held.

Arguments:

    Device - Supplies a pointer to the device to assign an address to.

Return Value:

    Status code. On success, the new device address will be returned inside the
    device, and this routine will send a SET_ADDRESS command to the device.

--*/

{

    UCHAR AddressIndex;
    BOOL AddressLockHeld;
    PUSB_HOST_CONTROLLER Controller;
    UCHAR FoundAddress;
    ULONG LengthTransferred;
    UCHAR ReservedAddress;
    PUSB_DEVICE *Segment;
    ULONG SegmentIndex;
    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;

    AddressLockHeld = FALSE;
    FoundAddress = 0;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Device->BusAddress == 0);
    ASSERT(KeIsQueuedLockHeld(Device->Controller->Lock) != FALSE);

    //
    // Acquire the controller's address lock in order to find an address.
    //

    Controller = Device->Controller;
    KeAcquireQueuedLock(Controller->AddressLock);
    AddressLockHeld = TRUE;
    ReservedAddress = UsbpGetReservedDeviceAddress(Device);
    if (ReservedAddress != 0) {

        //
        // Make the association between the address (which is just reserved)
        // to the actual device pointer.
        //

        Status = UsbpReserveDeviceAddress(Controller, Device, ReservedAddress);

        ASSERT(KSUCCESS(Status));

        FoundAddress = ReservedAddress;

    //
    // This device is not special, go allocate an address.
    //

    } else {
        if (Controller->ControllerFull != FALSE) {
            Status = STATUS_RESOURCE_IN_USE;
            goto AssignDeviceAddressEnd;
        }

        //
        // Loop through every segment of addresses. Segmentation of the 128
        // addresses is done to cut down on wasted memory allocations.
        //

        for (SegmentIndex = 0;
             SegmentIndex < USB_HOST_ADDRESS_SEGMENT_COUNT;
             SegmentIndex += 1) {

            Segment = Controller->ChildrenByAddress[SegmentIndex];
            for (AddressIndex = 0;
                 AddressIndex < USB_HOST_ADDRESSES_PER_SEGMENT;
                 AddressIndex += 1) {

                //
                // Skip address zero.
                //

                if ((SegmentIndex == 0) && (AddressIndex == 0)) {
                    continue;
                }

                //
                // If there is no segment or the index is free, try to reserve
                // it.
                //

                if ((Segment == NULL) || (Segment[AddressIndex] == NULL)) {
                    FoundAddress =
                              (SegmentIndex * USB_HOST_ADDRESSES_PER_SEGMENT) +
                              AddressIndex;

                    Status = UsbpReserveDeviceAddress(Controller,
                                                      Device,
                                                      FoundAddress);

                    if (KSUCCESS(Status)) {
                        break;
                    }

                    FoundAddress = 0;
                }
            }

            if (FoundAddress != 0) {
                break;
            }
        }
    }

    //
    // If an address could not be allocated, the bus is full of devices!
    //

    if (FoundAddress == 0) {
        Controller->ControllerFull = TRUE;
        Status = STATUS_RESOURCE_IN_USE;
        goto AssignDeviceAddressEnd;
    }

    //
    // Now that an address has been acquired, release the address lock.
    //

    KeReleaseQueuedLock(Controller->AddressLock);
    AddressLockHeld = FALSE;

    //
    // Send a SET_ADDRESS command to the device to get it off of address zero.
    //

    RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
    SetupPacket.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                              USB_SETUP_REQUEST_STANDARD |
                              USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    SetupPacket.Request = USB_DEVICE_REQUEST_SET_ADDRESS;
    SetupPacket.Value = FoundAddress;
    SetupPacket.Index = 0;
    SetupPacket.Length = 0;
    Status = UsbSendControlTransfer(Device,
                                    UsbTransferDirectionOut,
                                    &SetupPacket,
                                    NULL,
                                    0,
                                    &LengthTransferred);

    if (!KSUCCESS(Status)) {
        goto AssignDeviceAddressEnd;
    }

    //
    // Wait 2ms for the set address request to settle (see section 9.2.6.3 of
    // the USB 2.0 specification).
    //

    HlBusySpin(2 * 1000);
    Device->BusAddress = FoundAddress;
    Status = STATUS_SUCCESS;

AssignDeviceAddressEnd:

    //
    // Release the address lock first before unassigning the address. That
    // routine also acquires the lock.
    //

    if (AddressLockHeld != FALSE) {
        KeReleaseQueuedLock(Controller->AddressLock);
    }

    if (!KSUCCESS(Status)) {

        //
        // Unassign the bus address if it was assigned.
        //

        if (FoundAddress != 0) {
            UsbpUnassignDeviceAddress(Device);
        }
    }

    return Status;
}

VOID
UsbpUnassignDeviceAddress (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine unassigns a USB device's address.

Arguments:

    Device - Supplies a pointer to the USB device whose address is to be
        unassigned.

Return Value:

    None.

--*/

{

    ULONG AddressIndex;
    PUSB_HOST_CONTROLLER Controller;
    PUSB_DEVICE *Segment;
    BOOL SegmentEmpty;
    ULONG SegmentIndex;

    //
    // There's nothing to do if the device never received an address.
    //

    if (Device->BusAddress == 0) {
        return;
    }

    //
    // Acquire the controller's address lock before releasing the address.
    //

    Controller = Device->Controller;
    KeAcquireQueuedLock(Controller->AddressLock);

    //
    // Release the address and mark that the controller is not full.
    //

    SegmentIndex = Device->BusAddress / USB_HOST_ADDRESSES_PER_SEGMENT;
    Segment = Controller->ChildrenByAddress[SegmentIndex];
    Segment[Device->BusAddress % USB_HOST_ADDRESSES_PER_SEGMENT] = NULL;
    Device->BusAddress = 0;
    Controller->ControllerFull = FALSE;

    //
    // Loop through the segment addresses to determine if it is empty.
    //

    SegmentEmpty = TRUE;
    for (AddressIndex = 0;
         AddressIndex < USB_HOST_ADDRESSES_PER_SEGMENT;
         AddressIndex += 1) {

        //
        // Skip address zero.
        //

        if ((SegmentIndex == 0) && (AddressIndex == 0)) {
            continue;
        }

        //
        // If the space is not free, declare that the segment is not empty.
        //

        if (Segment[AddressIndex] != NULL) {
            SegmentEmpty = FALSE;
            break;
        }
    }

    //
    // If the segment is empty, null it out.
    //

    if (SegmentEmpty != FALSE) {
        Controller->ChildrenByAddress[SegmentIndex] = NULL;
    }

    //
    // Release the address lock.
    //

    KeReleaseQueuedLock(Controller->AddressLock);

    //
    // With the lock released, free the segment if it was empty.
    //

    if (SegmentEmpty != FALSE) {
        MmFreeNonPagedPool(Segment);
    }

    return;
}

KSTATUS
UsbpReadDeviceStrings (
    PUSB_DEVICE Device,
    PUSB_DEVICE_DESCRIPTOR DeviceDescriptor
    )

/*++

Routine Description:

    This routine attempts to read the manufacturer, product, and serial number
    strings from the device, if they exist.

Arguments:

    Device - Supplies a pointer to the device to assign an address to.

    DeviceDescriptor - Supplies a pointer to the device descriptor (which
        contains the string descriptor indices).

Return Value:

    Status code. On success, the strings will be allocated and filled into the
    device.

--*/

{

    ULONG LanguageCount;
    PUSHORT LanguageId;
    ULONG LanguageIndex;
    KSTATUS Status;
    PUSB_STRING_DESCRIPTOR StringDescriptor;
    BOOL UsEnglishSupported;

    StringDescriptor = NULL;

    //
    // If none of the strings being sought exist, just exit.
    //

    if ((DeviceDescriptor->ManufacturerStringIndex == 0) &&
        (DeviceDescriptor->ProductStringIndex == 0) &&
        (DeviceDescriptor->SerialNumberStringIndex == 0)) {

        Status = STATUS_SUCCESS;
        goto ReadDeviceStringsEnd;
    }

    //
    // Create a temporary string descriptor of the maximum possible size.
    //

    StringDescriptor = MmAllocateNonPagedPool(USB_STRING_DESCRIPTOR_MAX_SIZE,
                                              USB_CORE_ALLOCATION_TAG);

    if (StringDescriptor == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReadDeviceStringsEnd;
    }

    //
    // Attempt to read string 0, which returns the list of supported languages.
    //

    Status = UsbReadDeviceString(Device, 0, 0, StringDescriptor);
    if (!KSUCCESS(Status)) {
        if ((UsbDebugFlags & (USB_DEBUG_ENUMERATION | USB_DEBUG_ERRORS)) != 0) {
            RtlDebugPrint("USB: Device 0x%x failed to read language ID string "
                          "0.\n",
                          Device);
        }

        goto ReadDeviceStringsEnd;
    }

    LanguageCount = StringDescriptor->Length / 2;
    LanguageId = (PUSHORT)(StringDescriptor + 1);
    UsEnglishSupported = FALSE;
    for (LanguageIndex = 0; LanguageIndex < LanguageCount; LanguageIndex += 1) {
        if (LanguageId[LanguageIndex] == USB_LANGUAGE_ENGLISH_US) {
            UsEnglishSupported = TRUE;
        }
    }

    if (UsEnglishSupported == FALSE) {
        if ((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) {
            RtlDebugPrint("USB: Device 0x%x supports %d languages but US "
                          "English (0x0409) is not one of them. Skipping "
                          "device strings.\n",
                          Device,
                          LanguageCount);

            Status = STATUS_SUCCESS;
            goto ReadDeviceStringsEnd;
        }
    }

    //
    // Attempt to get the manufacturer string descriptor.
    //

    if (DeviceDescriptor->ManufacturerStringIndex != 0) {
        Status = UsbReadDeviceString(Device,
                                     DeviceDescriptor->ManufacturerStringIndex,
                                     USB_LANGUAGE_ENGLISH_US,
                                     StringDescriptor);

        if (!KSUCCESS(Status)) {
            goto ReadDeviceStringsEnd;
        }

        if (Device->Manufacturer != NULL) {
            MmFreePagedPool(Device->Manufacturer);
        }

        Device->Manufacturer =
                    UsbpCreateAnsiStringFromStringDescriptor(StringDescriptor);

        if (Device->Manufacturer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReadDeviceStringsEnd;
        }
    }

    //
    // Attempt to get the product name string descriptor.
    //

    if (DeviceDescriptor->ProductStringIndex != 0) {
        Status = UsbReadDeviceString(Device,
                                     DeviceDescriptor->ProductStringIndex,
                                     USB_LANGUAGE_ENGLISH_US,
                                     StringDescriptor);

        if (!KSUCCESS(Status)) {
            goto ReadDeviceStringsEnd;
        }

        if (Device->ProductName != NULL) {
            MmFreePagedPool(Device->ProductName);
        }

        Device->ProductName =
                    UsbpCreateAnsiStringFromStringDescriptor(StringDescriptor);

        if (Device->ProductName == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReadDeviceStringsEnd;
        }
    }

    //
    // Attempt to get the serial number string descriptor.
    //

    if (DeviceDescriptor->SerialNumberStringIndex != 0) {
        Status = UsbReadDeviceString(Device,
                                     DeviceDescriptor->SerialNumberStringIndex,
                                     USB_LANGUAGE_ENGLISH_US,
                                     StringDescriptor);

        if (!KSUCCESS(Status)) {
            goto ReadDeviceStringsEnd;
        }

        if (Device->SerialNumber != NULL) {
            MmFreePagedPool(Device->SerialNumber);
        }

        Device->SerialNumber =
                    UsbpCreateAnsiStringFromStringDescriptor(StringDescriptor);

        if (Device->SerialNumber == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ReadDeviceStringsEnd;
        }
    }

    Status = STATUS_SUCCESS;

ReadDeviceStringsEnd:
    if (KSUCCESS(Status)) {
        if ((UsbDebugFlags & USB_DEBUG_ENUMERATION) != 0) {
            RtlDebugPrint("USB: New Device VID: %04x, PID %04x, Class %d, "
                          "Address %d\nUSB: Manufacturer: \"%s\" Product Name: "
                          "\"%s\" Serial Number: \"%s\".\n",
                          DeviceDescriptor->VendorId,
                          DeviceDescriptor->ProductId,
                          DeviceDescriptor->Class,
                          Device->BusAddress,
                          Device->Manufacturer,
                          Device->ProductName,
                          Device->SerialNumber);
        }
    }

    if (StringDescriptor != NULL) {
        MmFreeNonPagedPool(StringDescriptor);
    }

    return Status;
}

PSTR
UsbpCreateAnsiStringFromStringDescriptor (
    PUSB_STRING_DESCRIPTOR StringDescriptor
    )

/*++

Routine Description:

    This routine converts a unicode string descriptor into an ANSI string.

Arguments:

    StringDescriptor - Supplies a pointer to the string descriptor to convert.

Return Value:

    Returns a pointer to the string on success. The caller is responsible for
    freeing this new string from paged pool.

    NULL on failure.

--*/

{

    ULONG Index;
    ULONG Length;
    PSTR NewString;
    PSTR UnicodeString;

    if ((StringDescriptor->Length < 2) ||
        ((StringDescriptor->Length & 0x1) != 0)) {

        return NULL;
    }

    Length = (StringDescriptor->Length / 2) - 1;
    NewString = MmAllocatePagedPool(Length + 1, USB_CORE_ALLOCATION_TAG);
    if (NewString == NULL) {
        return NULL;
    }

    Index = 0;
    UnicodeString = (PSTR)(StringDescriptor + 1);
    while (Index < Length) {
        NewString[Index] = *UnicodeString;
        Index += 1;
        UnicodeString += 2;
    }

    NewString[Index] = '\0';
    return NewString;
}

VOID
UsbpGetDeviceClass (
    PUSB_DEVICE Device,
    PUCHAR Class,
    PUCHAR Subclass,
    PUCHAR Protocol
    )

/*++

Routine Description:

    This routine returns the given device's effective class, subclass, and
    protocol identifiers. It will return information from the device descriptor
    if it is filled in, or the first interface if there is only one
    configuration and one interface.

Arguments:

    Device - Supplies a pointer to the device to enumerate.

    Class - Supplies a pointer where the class code will be returned.

    Subclass - Supplies a pointer where the sub-class code will be returned.

    Protocol - Supplies a pointer where the protocol code will be returned.

Return Value:

    Status code.

--*/

{

    UCHAR ClassCode;
    PUSB_CONFIGURATION Configuration;
    PUSB_INTERFACE_DESCRIPTION Interface;
    PLIST_ENTRY InterfaceListHead;
    UCHAR ProtocolCode;
    UCHAR SubclassCode;

    //
    // If the class code in the device descriptor is set, use it. If it defers
    // to the interfaces, look to see if there is only one interface. If so,
    // use that one.
    //

    ClassCode = Device->ClassCode;
    SubclassCode = Device->SubclassCode;
    ProtocolCode = Device->ProtocolCode;
    if (ClassCode == UsbDeviceClassUseInterface) {

        ASSERT(LIST_EMPTY(&(Device->ConfigurationList)) == FALSE);

        Configuration = LIST_VALUE(Device->ConfigurationList.Next,
                                   USB_CONFIGURATION,
                                   ListEntry);

        InterfaceListHead = &(Configuration->Description.InterfaceListHead);

        ASSERT(LIST_EMPTY(InterfaceListHead) == FALSE);

        //
        // If there's only one interface on the list, use it.
        //

        if (InterfaceListHead->Next == InterfaceListHead->Previous) {
            Interface = LIST_VALUE(InterfaceListHead->Next,
                                   USB_INTERFACE_DESCRIPTION,
                                   ListEntry);

            ClassCode = Interface->Descriptor.Class;
            SubclassCode = Interface->Descriptor.Subclass;
            ProtocolCode = Interface->Descriptor.Protocol;

            //
            // Also save these back directly into the device.
            //

            Device->ClassCode = ClassCode;
            Device->SubclassCode = SubclassCode;
            Device->ProtocolCode = ProtocolCode;
        }
    }

    *Class = ClassCode;
    *Subclass = SubclassCode;
    *Protocol = ProtocolCode;
    return;
}

KSTATUS
UsbpCreateOsDevice (
    PUSB_DEVICE Device,
    UCHAR Class,
    UCHAR Subclass,
    UCHAR Protocol,
    UCHAR Interface,
    BOOL InterfaceDevice,
    PDEVICE *CreatedDevice
    )

/*++

Routine Description:

    This routine creates an operating system device object for the given USB
    object.

Arguments:

    Device - Supplies a pointer to the USB device corresponding to the soon-to-
        be-created OS device.

    Class - Supplies the USB device class code of the new device.

    Subclass - Supplies the USB subclass code of the new device.

    Protocol - Supplies the USB protocol code of the new device.

    Interface - Supplies an optional interface number for the device. If the
        interface device boolean flag is false, this parameter is ignored.

    InterfaceDevice - Supplies a boolean indicating if this device is just
        enumerating an interface off a pre-existing device, or if it's
        enumerating the device itself. For interface devies, the interface
        number is tacked onto the device ID.

    CreatedDevice - Supplies a pointer where the pointer to the fresh OS
        device will be returned.

Return Value:

    Status code.

--*/

{

    PSTR DeviceClass;
    CHAR DeviceId[USB_DEVICE_ID_LENGTH + 1];
    PDRIVER Driver;
    PDEVICE Parent;
    KSTATUS Status;

    *CreatedDevice = NULL;

    //
    // Create the device ID string.
    //

    if (InterfaceDevice != FALSE) {
        RtlPrintToString(DeviceId,
                         USB_DEVICE_ID_LENGTH + 1,
                         CharacterEncodingDefault,
                         USB_DEVICE_ID_WITH_INTERFACE_FORMAT,
                         Device->VendorId,
                         Device->ProductId,
                         Interface);

    } else {
        RtlPrintToString(DeviceId,
                         USB_DEVICE_ID_LENGTH + 1,
                         CharacterEncodingDefault,
                         USB_DEVICE_ID_FORMAT,
                         Device->VendorId,
                         Device->ProductId);
    }

    //
    // Set the class ID if applicable.
    //

    switch (Class) {
    case UsbDeviceClassUseInterface:
        DeviceClass = USB_COMPOUND_DEVICE_CLASS_ID;
        break;

    case UsbDeviceClassHid:
        DeviceClass = USB_HID_CLASS_ID;
        if ((Subclass == USB_HID_BOOT_INTERFACE_SUBCLASS) &&
            (Protocol == USB_HID_BOOT_KEYBOARD_PROTOCOL)) {

            DeviceClass = USB_BOOT_KEYBOARD_CLASS_ID;

        } else if ((Subclass == USB_HID_BOOT_INTERFACE_SUBCLASS) &&
                   (Protocol == USB_HID_BOOT_MOUSE_PROTOCOL)) {

            DeviceClass = USB_BOOT_MOUSE_CLASS_ID;
        }

        break;

    case UsbInterfaceClassMassStorage:
        DeviceClass = USB_MASS_STORAGE_CLASS_ID;
        break;

    case UsbDeviceClassHub:
        DeviceClass = USB_HUB_CLASS_ID;
        break;

    default:
        DeviceClass = NULL;
        break;
    }

    //
    // For interface devices, the device itself is the parent.
    //

    if (InterfaceDevice != FALSE) {
        Driver = Device->Driver;
        Parent = Device->Device;

    } else {

        ASSERT(Device->Parent != NULL);

        Driver = Device->Parent->Driver;
        Parent = Device->Parent->Device;
    }

    ASSERT((Driver != NULL) && (Parent != NULL));

    //
    // Create the OS device object, making the device visible to the system.
    //

    Status = IoCreateDevice(Driver,
                            NULL,
                            Parent,
                            DeviceId,
                            DeviceClass,
                            NULL,
                            CreatedDevice);

    if (!KSUCCESS(Status)) {
        goto CreateOsDeviceEnd;
    }

    Status = STATUS_SUCCESS;

CreateOsDeviceEnd:
    return Status;
}

UCHAR
UsbpGetReservedDeviceAddress (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine returns the device's reserved address if it is a special
    device. The debug device and debug device hub both have reserved addresses
    since they're being used up in debugger land.

Arguments:

    Device - Supplies a pointer to the device whose address is being assigned.

Return Value:

    Returns the device's reserved address on success.

    0 if the device does not have a reserved address.

--*/

{

    PUSB_DEVICE CheckDevice;
    ULONG CheckIndex;
    PUSB_HOST_CONTROLLER Controller;
    PDEBUG_USB_HANDOFF_DATA HandoffData;
    ULONG PathIndex;

    Controller = Device->Controller;
    if (Controller->HandoffData == NULL) {
        return 0;
    }

    HandoffData = &(Controller->HandoffData->U.Usb);

    //
    // If this is not the debug device itself or the hub of the debug device,
    // then the device is not special.
    //

    ASSERT(Device->Depth != 0);

    PathIndex = Device->Depth - 1;
    if ((PathIndex != HandoffData->DevicePathSize - 1) &&
        (PathIndex != HandoffData->DevicePathSize - 2)) {

        return 0;
    }

    if ((UsbDebugFlags & USB_DEBUG_DEBUGGER_HANDOFF) != 0) {
        RtlDebugPrint("USB: Checking device %04X:%04X against debugger "
                      "device path.\n",
                      Device->VendorId,
                      Device->ProductId);
    }

    CheckIndex = PathIndex;
    CheckDevice = Device;
    while (TRUE) {

        //
        // If the device's hub address is not equal to the debug device path,
        // exit.
        //

        if (CheckDevice->PortNumber != HandoffData->DevicePath[CheckIndex]) {
            return 0;
        }

        if (CheckIndex == 0) {
            break;
        }

        CheckIndex -= 1;
        CheckDevice = CheckDevice->Parent;
    }

    //
    // The path lines up, this is either the debug device itself or the hub.
    //

    if (PathIndex == HandoffData->DevicePathSize - 1) {
        if ((UsbDebugFlags & USB_DEBUG_DEBUGGER_HANDOFF) != 0) {
            RtlDebugPrint("USB: Found debugger device 0x%x! Assigning "
                          "address 0x%x\n",
                          Device,
                          HandoffData->DeviceAddress);
        }

        if ((Device->VendorId != HandoffData->VendorId) ||
            (Device->ProductId != HandoffData->ProductId)) {

            RtlDebugPrint("USB: Found VID:PID %04X:%04X at debug device path, "
                          "expected %04X:%04x.\n",
                          Device->VendorId,
                          Device->ProductId,
                          HandoffData->VendorId,
                          HandoffData->ProductId);

            return 0;
        }

        Device->DebugDevice = TRUE;
        return HandoffData->DeviceAddress;
    }

    //
    // If there's a hub address, return that.
    //

    if ((UsbDebugFlags & USB_DEBUG_DEBUGGER_HANDOFF) != 0) {
        RtlDebugPrint("USB: Found debugger hub 0x%x. Assigning "
                      "address 0x%x\n",
                      Device,
                      HandoffData->HubAddress);
    }

    return HandoffData->HubAddress;
}

