/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smsc95.c

Abstract:

    This module implements support for the driver portion of the SMSC95xx
    family of USB Ethernet Controllers.

Author:

    Evan Green 7-Nov-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/usb/usb.h>
#include "smsc95.h"

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
Sm95AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Sm95DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Sm95DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Sm95DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Sm95DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Sm95DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Sm95DestroyLink (
    PVOID DeviceContext
    );

KSTATUS
Sm95pInitializeDeviceStructures (
    PVOID OsDevice,
    PSM95_DEVICE *NewDevice
    );

VOID
Sm95pDestroyDeviceStructures (
    PSM95_DEVICE Device
    );

VOID
Sm95pDeviceAddReference (
    PSM95_DEVICE Device
    );

VOID
Sm95pDeviceReleaseReference (
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pSetUpUsbDevice (
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pStartDevice (
    PIRP Irp,
    PSM95_DEVICE Device
    );

KSTATUS
Sm95pStopDevice (
    PIRP Irp,
    PSM95_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Sm95Driver = NULL;

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the SMSC95xx driver. It registers its
    other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    Sm95Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Sm95AddDevice;
    FunctionTable.DispatchStateChange = Sm95DispatchStateChange;
    FunctionTable.DispatchOpen = Sm95DispatchOpen;
    FunctionTable.DispatchClose = Sm95DispatchClose;
    FunctionTable.DispatchIo = Sm95DispatchIo;
    FunctionTable.DispatchSystemControl = Sm95DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Sm95AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the SMSC95xx
    driver acts as the function driver. The driver will attach itself to the
    stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    PSM95_DEVICE Device;
    KSTATUS Status;

    Status = Sm95pInitializeDeviceStructures(DeviceToken, &Device);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Device);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device != NULL) {
            Sm95pDeviceReleaseReference(Device);
        }
    }

    return Status;
}

VOID
Sm95DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    if (Irp->Direction == IrpUp) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
        case IrpMinorQueryChildren:
            IoCompleteIrp(Sm95Driver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorStartDevice:
            Status = Sm95pStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Sm95Driver, Irp, Status);
            }

            break;

        case IrpMinorRemoveDevice:
            Status = Sm95pStopDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Sm95Driver, Irp, Status);
                break;
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Sm95DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
Sm95DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
Sm95DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    return;
}

VOID
Sm95DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PSM95_DEVICE Device;
    PSYSTEM_CONTROL_DEVICE_INFORMATION DeviceInformationRequest;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    Device = DeviceContext;
    if (Irp->Direction == IrpDown) {
        switch (Irp->MinorCode) {
        case IrpMinorSystemControlDeviceInformation:
            DeviceInformationRequest = Irp->U.SystemControl.SystemContext;
            Status = NetGetSetLinkDeviceInformation(
                                         Device->NetworkLink,
                                         &(DeviceInformationRequest->Uuid),
                                         DeviceInformationRequest->Data,
                                         &(DeviceInformationRequest->DataSize),
                                         DeviceInformationRequest->Set);

            IoCompleteIrp(Sm95Driver, Irp, Status);
            break;

        default:
            break;
        }
    }

    return;
}

KSTATUS
Sm95pAddNetworkDevice (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

{

    NET_LINK_PROPERTIES Properties;
    KSTATUS Status;

    if (Device->NetworkLink != NULL) {
        Status = STATUS_SUCCESS;
        goto AddNetworkDeviceEnd;
    }

    //
    // Add a link to the core networking library.
    //

    RtlZeroMemory(&Properties, sizeof(NET_LINK_PROPERTIES));
    Properties.Version = NET_LINK_PROPERTIES_VERSION;
    Properties.TransmitAlignment = MmGetIoBufferAlignment();
    Properties.Device = Device->OsDevice;
    Properties.DeviceContext = Device;
    Properties.PacketSizeInformation.MaxPacketSize = SM95_MAX_PACKET_SIZE;
    Properties.PacketSizeInformation.HeaderSize = SM95_TRANSMIT_HEADER_SIZE;
    Properties.DataLinkType = NetDomainEthernet;
    Properties.MaxPhysicalAddress = MAX_ULONG;
    Properties.PhysicalAddress.Domain = NetDomainEthernet;
    Properties.Capabilities = Device->SupportedCapabilities;
    RtlCopyMemory(&(Properties.PhysicalAddress.Address),
                  &(Device->MacAddress),
                  sizeof(Device->MacAddress));

    Properties.Interface.Send = Sm95Send;
    Properties.Interface.GetSetInformation = Sm95GetSetInformation;
    Properties.Interface.DestroyLink = Sm95DestroyLink;
    Status = NetAddLink(&Properties, &(Device->NetworkLink));
    if (!KSUCCESS(Status)) {
        goto AddNetworkDeviceEnd;
    }

    //
    // The networking core now references the device structure. Add a
    // reference for it.
    //

    Sm95pDeviceAddReference(Device);

AddNetworkDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->NetworkLink != NULL) {
            NetRemoveLink(Device->NetworkLink);
            Device->NetworkLink = NULL;
        }
    }

    return Status;
}

VOID
Sm95DestroyLink (
    PVOID DeviceContext
    )

/*++

Routine Description:

    This routine notifies the device layer that the networking core is in the
    process of destroying the link and will no longer call into the device for
    this link. This allows the device layer to release any context that was
    supporting the device link interface.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link being destroyed.

Return Value:

    None.

--*/

{

    Sm95pDeviceReleaseReference(DeviceContext);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Sm95pInitializeDeviceStructures (
    PVOID OsDevice,
    PSM95_DEVICE *NewDevice
    )

/*++

Routine Description:

    This routine initializes an SMSC95xx device.

Arguments:

    OsDevice - Supplies a pointer to the system token that represents this
        device.

    NewDevice - Supplies a pointer where the new structure will be returned.

Return Value:

    Status code.

--*/

{

    ULONG BufferAlignment;
    PSM95_DEVICE Device;
    ULONG Index;
    ULONG IoBufferFlags;
    UINTN IoBufferSize;
    ULONG MaxControlSize;
    ULONG MaxHighSpeedBurstSize;
    ULONG MaxInterruptSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    PUSB_TRANSFER UsbTransfer;
    PVOID VirtualAddress;

    Device = MmAllocatePagedPool(sizeof(SM95_DEVICE), SM95_ALLOCATION_TAG);
    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    RtlZeroMemory(Device, sizeof(SM95_DEVICE));
    Device->OsDevice = OsDevice;
    Device->UsbCoreHandle = INVALID_HANDLE;
    Device->ReferenceCount = 1;
    INITIALIZE_LIST_HEAD(&(Device->BulkOutFreeTransferList));
    Device->BulkOutListLock = KeCreateQueuedLock();
    if (Device->BulkOutListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->ConfigurationLock = KeCreateQueuedLock();
    if (Device->ConfigurationLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Attempt to attach to the USB core.
    //

    Status = UsbDriverAttach(OsDevice, Sm95Driver, &(Device->UsbCoreHandle));
    if (!KSUCCESS(Status)) {
        goto InitializeDeviceStructuresEnd;
    }

    Status = Sm95pSetUpUsbDevice(Device);
    if (!KSUCCESS(Status)) {
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Create an I/O buffer for the control and receive transfers.
    //

    BufferAlignment = MmGetIoBufferAlignment();
    MaxHighSpeedBurstSize = ALIGN_RANGE_UP(SM95_HIGH_SPEED_BURST_SIZE,
                                           BufferAlignment);

    MaxControlSize = ALIGN_RANGE_UP(SM95_MAX_CONTROL_TRANSFER_SIZE,
                                    BufferAlignment);

    MaxInterruptSize = ALIGN_RANGE_UP(SM95_MAX_INTERRUPT_TRANSFER_SIZE,
                                      BufferAlignment);

    IoBufferSize = (MaxHighSpeedBurstSize * SM95_BULK_IN_TRANSFER_COUNT) +
                   MaxControlSize + MaxInterruptSize;

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Device->IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                  MAX_ULONG,
                                                  BufferAlignment,
                                                  IoBufferSize,
                                                  IoBufferFlags);

    if (Device->IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(Device->IoBuffer->FragmentCount == 1);
    ASSERT(Device->IoBuffer->Fragment[0].VirtualAddress != NULL);

    PhysicalAddress = Device->IoBuffer->Fragment[0].PhysicalAddress;
    VirtualAddress = Device->IoBuffer->Fragment[0].VirtualAddress;

    //
    // Set up the bulk in transfers that are used to receive packets.
    //

    for (Index = 0; Index < SM95_BULK_IN_TRANSFER_COUNT; Index += 1) {
        UsbTransfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                          Device->BulkInEndpoint,
                                          SM95_HIGH_SPEED_BURST_SIZE,
                                          0);

        if (UsbTransfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeDeviceStructuresEnd;
        }

        UsbTransfer->Buffer = VirtualAddress;
        UsbTransfer->BufferPhysicalAddress = PhysicalAddress;
        UsbTransfer->Direction = UsbTransferDirectionIn;
        UsbTransfer->Length = SM95_HIGH_SPEED_BURST_SIZE;
        UsbTransfer->BufferActualLength = MaxHighSpeedBurstSize;
        UsbTransfer->UserData = Device;
        UsbTransfer->CallbackRoutine = Sm95BulkInTransferCompletion;
        Device->BulkInTransfer[Index] = UsbTransfer;
        PhysicalAddress += MaxHighSpeedBurstSize;
        VirtualAddress += MaxHighSpeedBurstSize;
    }

    //
    // Set up the control transfer that's used for register reads and writes.
    //

    Device->ControlTransfer = UsbAllocateTransfer(
                                               Device->UsbCoreHandle,
                                               0,
                                               SM95_MAX_CONTROL_TRANSFER_SIZE,
                                               0);

    if (Device->ControlTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->ControlTransfer->Buffer = VirtualAddress;
    Device->ControlTransfer->BufferPhysicalAddress = PhysicalAddress;
    Device->ControlTransfer->BufferActualLength = MaxControlSize;
    VirtualAddress += MaxControlSize;
    PhysicalAddress += MaxControlSize;

    //
    // Set up the interrupt transfer that's used for link change notifications.
    //

    Device->InterruptTransfer = UsbAllocateTransfer(
                                             Device->UsbCoreHandle,
                                             Device->InterruptEndpoint,
                                             SM95_MAX_INTERRUPT_TRANSFER_SIZE,
                                             0);

    if (Device->InterruptTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Device->InterruptTransfer->Buffer = VirtualAddress;
    Device->InterruptTransfer->BufferPhysicalAddress = PhysicalAddress;
    Device->InterruptTransfer->BufferActualLength = MaxInterruptSize;
    Device->InterruptTransfer->Direction = UsbTransferDirectionIn;
    Device->InterruptTransfer->Length = sizeof(ULONG);
    Device->InterruptTransfer->UserData = Device;
    Device->InterruptTransfer->CallbackRoutine =
                                               Sm95InterruptTransferCompletion;

    //
    // Advertise the supported capabilties and set the capabilities enabled by
    // default.
    //

    Device->SupportedCapabilities |= NET_LINK_CAPABILITY_PROMISCUOUS_MODE;
    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    if (!KSUCCESS(Status)) {
        if (Device != NULL) {
            Sm95pDeviceReleaseReference(Device);
            Device = NULL;
        }
    }

    *NewDevice = Device;
    return Status;
}

VOID
Sm95pDestroyDeviceStructures (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine destroys an SMSC95xx device structure.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    ULONG Index;

    //
    // Destroy all the allocated transfers. For good measure, make sure they
    // are cancelled.
    //

    for (Index = 0; Index < SM95_BULK_IN_TRANSFER_COUNT; Index += 1) {
        if (Device->BulkInTransfer[Index] != NULL) {
            UsbCancelTransfer(Device->BulkInTransfer[Index], TRUE);
            UsbDestroyTransfer(Device->BulkInTransfer[Index]);
        }
    }

    if (Device->ControlTransfer != NULL) {
        UsbCancelTransfer(Device->ControlTransfer, TRUE);
        UsbDestroyTransfer(Device->ControlTransfer);
    }

    if (Device->InterruptTransfer != NULL) {
        UsbCancelTransfer(Device->InterruptTransfer, TRUE);
        UsbDestroyTransfer(Device->InterruptTransfer);
    }

    if (Device->IoBuffer != NULL) {
        MmFreeIoBuffer(Device->IoBuffer);
    }

    //
    // There should be no active bulk out transfers, so destroy all the free
    // transfers.
    //

    Sm95pDestroyBulkOutTransfers(Device);
    if (Device->BulkOutListLock != NULL) {
        KeDestroyQueuedLock(Device->BulkOutListLock);
    }

    if (Device->ConfigurationLock != NULL) {
        KeDestroyQueuedLock(Device->ConfigurationLock);
    }

    MmFreePagedPool(Device);
    return;
}

VOID
Sm95pDeviceAddReference (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine increments the reference count of the given SM95 device.

Arguments:

    Device - Supplies a pointer to the SM95 device.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Device->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) & (OldReferenceCount < 0x20000000));

    return;
}

VOID
Sm95pDeviceReleaseReference (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine decrements the reference count of the given SM95 device.

Arguments:

    Device - Supplies a pointer to the SM95 device.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Device->ReferenceCount), -1);

    ASSERT(OldReferenceCount != 0);

    if (OldReferenceCount == 1) {
        Sm95pDestroyDeviceStructures(Device);
    }

    return;
}

KSTATUS
Sm95pSetUpUsbDevice (
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine claims the proper interface for the device and finds the
    bulk in, bulk out, and interrupt endpoints.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION_DESCRIPTION Configuration;
    PLIST_ENTRY CurrentEntry;
    USB_TRANSFER_DIRECTION Direction;
    PUSB_ENDPOINT_DESCRIPTION Endpoint;
    UCHAR EndpointType;
    PUSB_INTERFACE_DESCRIPTION Interface;
    KSTATUS Status;

    if (Device->InterfaceClaimed != FALSE) {

        ASSERT((Device->BulkInEndpoint != 0) &&
               (Device->BulkOutEndpoint != 0) &&
               (Device->InterruptEndpoint != 0));

        Status = STATUS_SUCCESS;
        goto SetUpUsbDeviceEnd;
    }

    //
    // If the configuration isn't yet set, set the first one.
    //

    Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);
    if (Configuration == NULL) {
        Status = UsbSetConfiguration(Device->UsbCoreHandle, 0, TRUE);
        if (!KSUCCESS(Status)) {
            goto SetUpUsbDeviceEnd;
        }

        Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);

        ASSERT(Configuration != NULL);

    }

    //
    // Get and verify the interface.
    //

    Interface = UsbGetDesignatedInterface(Device->OsDevice,
                                          Device->UsbCoreHandle);

    if (Interface == NULL) {
        Status = STATUS_NO_INTERFACE;
        goto SetUpUsbDeviceEnd;
    }

    if (Interface->Descriptor.Class != UsbInterfaceClassVendor) {
        Status = STATUS_NO_INTERFACE;
        goto SetUpUsbDeviceEnd;
    }

    //
    // Locate the IN and OUT bulk endpoints, and the interrupt endpoint.
    //

    CurrentEntry = Interface->EndpointListHead.Next;
    while (CurrentEntry != &(Interface->EndpointListHead)) {
        if ((Device->BulkInEndpoint != 0) &&
            (Device->BulkOutEndpoint != 0) &&
            (Device->InterruptEndpoint != 0)) {

            break;
        }

        Endpoint = LIST_VALUE(CurrentEntry,
                              USB_ENDPOINT_DESCRIPTION,
                              ListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Deconstruct the components of the endpoint descriptor.
        //

        EndpointType = Endpoint->Descriptor.Attributes &
                       USB_ENDPOINT_ATTRIBUTES_TYPE_MASK;

        if ((Endpoint->Descriptor.EndpointAddress &
             USB_ENDPOINT_ADDRESS_DIRECTION_IN) != 0) {

            Direction = UsbTransferDirectionIn;

        } else {
            Direction = UsbTransferDirectionOut;
        }

        //
        // Look to match the endpoint up to one of the required ones.
        //

        if (EndpointType == USB_ENDPOINT_ATTRIBUTES_TYPE_BULK) {
            if ((Device->BulkInEndpoint == 0) &&
                (Direction == UsbTransferDirectionIn)) {

                Device->BulkInEndpoint = Endpoint->Descriptor.EndpointAddress;

            } else if ((Device->BulkOutEndpoint == 0) &&
                       (Direction == UsbTransferDirectionOut)) {

                Device->BulkOutEndpoint = Endpoint->Descriptor.EndpointAddress;
            }

        } else if (EndpointType == USB_ENDPOINT_ATTRIBUTES_TYPE_INTERRUPT) {
            if ((Device->InterruptEndpoint == 0) &&
                (Direction == UsbTransferDirectionIn)) {

                Device->InterruptEndpoint =
                                          Endpoint->Descriptor.EndpointAddress;
            }
        }
    }

    if ((Device->BulkInEndpoint == 0) ||
        (Device->BulkOutEndpoint == 0) ||
        (Device->InterruptEndpoint == 0)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto SetUpUsbDeviceEnd;
    }

    //
    // Everything's all ready, claim the interface.
    //

    Status = UsbClaimInterface(Device->UsbCoreHandle,
                               Interface->Descriptor.InterfaceNumber);

    if (!KSUCCESS(Status)) {
        goto SetUpUsbDeviceEnd;
    }

    Device->InterfaceNumber = Interface->Descriptor.InterfaceNumber;
    Device->InterfaceClaimed = TRUE;
    Status = STATUS_SUCCESS;

SetUpUsbDeviceEnd:
    return Status;
}

KSTATUS
Sm95pStartDevice (
    PIRP Irp,
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine starts the SMSC95xx LAN device.

Arguments:

    Irp - Supplies a pointer to the start IRP.

    Device - Supplies a pointer to the device information.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Start up the controller.
    //

    Status = Sm95pInitialize(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    return Status;
}

KSTATUS
Sm95pStopDevice (
    PIRP Irp,
    PSM95_DEVICE Device
    )

/*++

Routine Description:

    This routine stops the SMSC95xx LAN device.

Arguments:

    Irp - Supplies a pointer to the removal IRP.

    Device - Supplies a pointer to the device information.

Return Value:

    Status code.

--*/

{

    //
    // Detach the device from USB. This will cancel all transfer attached to
    // the device, including the in-flight bulk out transfers that this driver
    // does not track.
    //

    if (Device->UsbCoreHandle != INVALID_HANDLE) {
        UsbDetachDevice(Device->UsbCoreHandle);
    }

    if (Device->InterfaceClaimed != FALSE) {
        UsbReleaseInterface(Device->UsbCoreHandle, Device->InterfaceNumber);
    }

    if (Device->UsbCoreHandle != INVALID_HANDLE) {
        UsbDeviceClose(Device->UsbCoreHandle);
    }

    //
    // The device is gone, notify the networking core that the link has bene
    // removed.
    //

    if (Device->NetworkLink != NULL) {
        NetRemoveLink(Device->NetworkLink);
        Device->NetworkLink = NULL;
    }

    Sm95pDeviceReleaseReference(Device);
    return STATUS_SUCCESS;
}

