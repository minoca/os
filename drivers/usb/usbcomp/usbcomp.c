/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbcomp.c

Abstract:

    This module implements support for USB compound devices (devices with
    multiple interfaces).

Author:

    Evan Green 20-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usb.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used throughout the USB compound device driver.
//

#define USB_COMPOUND_ALLOCATION_TAG 0x43627355 // 'CbsU'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores context about a USB compound device.

Members:

    UsbCoreHandle - Stores the handle to the device as identified by the USB
        core library.

    InterfaceCount - Stores the number of interfaces this device has.

    Children - Stores the array of pointers to child devices, one for each
        exposed interface.

--*/

typedef struct _USB_COMPOUND_DEVICE {
    HANDLE UsbCoreHandle;
    ULONG InterfaceCount;
    PDEVICE *Children;
} USB_COMPOUND_DEVICE, *PUSB_COMPOUND_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UsbCmpAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
UsbCmpDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbCmpDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbCmpDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbCmpDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbCmpDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
UsbCmppStartDevice (
    PIRP Irp,
    PUSB_COMPOUND_DEVICE Device
    );

KSTATUS
UsbCmppEnumerateChildren (
    PIRP Irp,
    PUSB_COMPOUND_DEVICE Device
    );

VOID
UsbCmppRemoveDevice (
    PIRP Irp,
    PUSB_COMPOUND_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER UsbCmpDriver = NULL;

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

    This routine is the entry point for the USB compound device driver. It
    registers the other dispatch functions, and performs driver-wide
    initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    UsbCmpDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = UsbCmpAddDevice;
    FunctionTable.DispatchStateChange = UsbCmpDispatchStateChange;
    FunctionTable.DispatchOpen = UsbCmpDispatchOpen;
    FunctionTable.DispatchClose = UsbCmpDispatchClose;
    FunctionTable.DispatchIo = UsbCmpDispatchIo;
    FunctionTable.DispatchSystemControl = UsbCmpDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UsbCmpAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the USB compound
    device driver acts as the function driver. The driver will attach itself to
    the stack.

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

    PUSB_COMPOUND_DEVICE NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocatePagedPool(sizeof(USB_COMPOUND_DEVICE),
                                    USB_COMPOUND_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(USB_COMPOUND_DEVICE));
    NewDevice->UsbCoreHandle = INVALID_HANDLE;

    //
    // Attempt to attach to the USB core.
    //

    Status = UsbDriverAttach(DeviceToken,
                             UsbCmpDriver,
                             &(NewDevice->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    ASSERT(NewDevice->UsbCoreHandle != INVALID_HANDLE);

    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (NewDevice->UsbCoreHandle != INVALID_HANDLE) {
            UsbDeviceClose(NewDevice->UsbCoreHandle);
        }

        if (NewDevice != NULL) {
            MmFreePagedPool(NewDevice);
        }
    }

    return Status;
}

VOID
UsbCmpDispatchStateChange (
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

    PUSB_COMPOUND_DEVICE Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PUSB_COMPOUND_DEVICE)DeviceContext;

    //
    // If this is the parent device, enumerate children.
    //

    if (Device != NULL) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            if (Irp->Direction == IrpUp) {
                IoCompleteIrp(UsbCmpDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorStartDevice:

            //
            // Attempt to fire the thing up if the bus has already started it.
            //

            if (Irp->Direction == IrpUp) {
                Status = UsbCmppStartDevice(Irp, Device);
                IoCompleteIrp(UsbCmpDriver, Irp, Status);
            }

            break;

        case IrpMinorQueryChildren:
            if (Irp->Direction == IrpUp) {
                Status = UsbCmppEnumerateChildren(Irp, Device);
                IoCompleteIrp(UsbCmpDriver, Irp, Status);
            }

            break;

        case IrpMinorRemoveDevice:
            if (Irp->Direction == IrpUp) {
                UsbCmppRemoveDevice(Irp, Device);
            }

            break;

        //
        // For all other IRPs, do nothing.
        //

        default:
            break;
        }

    //
    // If this driver is acting as the bus driver for the child device, then
    // simply complete things as a happy bus.
    //

    } else {
        switch (Irp->MinorCode) {
        case IrpMinorRemoveDevice:
        case IrpMinorQueryResources:
        case IrpMinorStartDevice:
        case IrpMinorQueryChildren:
            IoCompleteIrp(UsbCmpDriver, Irp, STATUS_SUCCESS);
            break;

        default:
            break;
        }
    }

    return;
}

VOID
UsbCmpDispatchOpen (
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
UsbCmpDispatchClose (
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
UsbCmpDispatchIo (
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
UsbCmpDispatchSystemControl (
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

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    //
    // Do no processing on any IRPs. Let them flow.
    //

    return;
}

KSTATUS
UsbCmppStartDevice (
    PIRP Irp,
    PUSB_COMPOUND_DEVICE Device
    )

/*++

Routine Description:

    This routine starts up the USB compound device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB compound device.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PUSB_CONFIGURATION_DESCRIPTION Configuration;
    PLIST_ENTRY CurrentEntry;
    ULONG InterfaceCount;
    KSTATUS Status;

    //
    // If the configuration isn't yet set, set the first one.
    //

    Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);
    if (Configuration == NULL) {
        Status = UsbSetConfiguration(Device->UsbCoreHandle, 0, TRUE);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);

        ASSERT(Configuration != NULL);

    }

    if (Device->InterfaceCount == 0) {

        //
        // Loop through once counting the number of interfaces.
        //

        InterfaceCount = 0;
        CurrentEntry = Configuration->InterfaceListHead.Next;
        while (CurrentEntry != &(Configuration->InterfaceListHead)) {
            CurrentEntry = CurrentEntry->Next;
            InterfaceCount += 1;
        }

        if (InterfaceCount == 0) {

            ASSERT(FALSE);

            Status = STATUS_NO_INTERFACE;
            goto StartDeviceEnd;
        }

        //
        // Allocate the device pointer list.
        //

        AllocationSize = InterfaceCount * sizeof(PDEVICE);
        Device->Children = MmAllocatePagedPool(AllocationSize,
                                               USB_COMPOUND_ALLOCATION_TAG);

        if (Device->Children == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartDeviceEnd;
        }

        RtlZeroMemory(Device->Children, AllocationSize);
        Device->InterfaceCount = InterfaceCount;
    }

    Status = STATUS_SUCCESS;

StartDeviceEnd:
    return Status;
}

KSTATUS
UsbCmppEnumerateChildren (
    PIRP Irp,
    PUSB_COMPOUND_DEVICE Device
    )

/*++

Routine Description:

    This routine enumerates the children of the given USB compound device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB compound device.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION_DESCRIPTION Configuration;
    PLIST_ENTRY CurrentEntry;
    PUSB_INTERFACE_DESCRIPTION Interface;
    ULONG InterfaceIndex;
    KSTATUS Status;

    Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);
    if (Configuration == NULL) {
        Status = STATUS_NOT_CONFIGURED;
        goto EnumerateChildrenEnd;
    }

    //
    // Loop through each child.
    //

    CurrentEntry = Configuration->InterfaceListHead.Next;
    for (InterfaceIndex = 0;
         InterfaceIndex < Device->InterfaceCount;
         InterfaceIndex += 1) {

        if (CurrentEntry == &(Configuration->InterfaceListHead)) {

            ASSERT(FALSE);

            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto EnumerateChildrenEnd;
        }

        Interface = LIST_VALUE(CurrentEntry,
                               USB_INTERFACE_DESCRIPTION,
                               ListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Ask the USB core to enumerate a device for this interface.
        //

        Status = UsbEnumerateDeviceForInterface(
                                          Device->UsbCoreHandle,
                                          Interface,
                                          &(Device->Children[InterfaceIndex]));

        if (!KSUCCESS(Status)) {
            goto EnumerateChildrenEnd;
        }
    }

    Status = IoMergeChildArrays(Irp,
                                Device->Children,
                                Device->InterfaceCount,
                                USB_COMPOUND_ALLOCATION_TAG);

    if (!KSUCCESS(Status)) {
        goto EnumerateChildrenEnd;
    }

EnumerateChildrenEnd:
    return Status;
}

VOID
UsbCmppRemoveDevice (
    PIRP Irp,
    PUSB_COMPOUND_DEVICE Device
    )

/*++

Routine Description:

    This routine removes the USB compound device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this USB compound device.

Return Value:

    Status code.

--*/

{

    //
    // Detach the device from USB core's grasp. This marks it as disconnected.
    //

    UsbDetachDevice(Device->UsbCoreHandle);

    //
    // Destroy the interface device list. By the time the removal IRP reaches
    // the compound device driver, all of the children have already been
    // released. Do not interate over the pointers in this array because they
    // are invalid.
    //

    if (Device->Children != NULL) {
        MmFreePagedPool(Device->Children);
    }

    //
    // Release the reference taken on the USB core handle. This will clean up
    // the cached configurations.
    //

    UsbDeviceClose(Device->UsbCoreHandle);

    //
    // Release the USB compound device.
    //

    MmFreePagedPool(Device);
    return;
}

