/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbhub.c

Abstract:

    This module implements support for the USB Hub driver.

Author:

    Evan Green 16-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usbhost.h>
#include "usbhub.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores context about a USB Hub.

Members:

    UsbCoreHandle - Stores the handle to the hub as identified by the USB core
        library.

    UsbHub - Stores an opaque pointer to the USB hub-specific context.

--*/

typedef struct _USB_HUB_DRIVER_CONTEXT {
    HANDLE UsbCoreHandle;
    PUSB_HUB UsbHub;
} USB_HUB_DRIVER_CONTEXT, *PUSB_HUB_DRIVER_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UsbHubAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
UsbHubDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbHubDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbHubDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbHubDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbHubDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
UsbHubpStartDevice (
    PIRP Irp,
    PUSB_HUB_DRIVER_CONTEXT Device
    );

VOID
UsbHubpEnumerateChildren (
    PIRP Irp,
    PUSB_HUB_DRIVER_CONTEXT Device
    );

VOID
UsbHubpRemoveDevice (
    PIRP Irp,
    PUSB_HUB_DRIVER_CONTEXT Device
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER UsbHubDriver = NULL;

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

    This routine is the entry point for the USB Hub driver. It registers its
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

    UsbHubDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = UsbHubAddDevice;
    FunctionTable.DispatchStateChange = UsbHubDispatchStateChange;
    FunctionTable.DispatchOpen = UsbHubDispatchOpen;
    FunctionTable.DispatchClose = UsbHubDispatchClose;
    FunctionTable.DispatchIo = UsbHubDispatchIo;
    FunctionTable.DispatchSystemControl = UsbHubDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UsbHubAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the USB Hub
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

    PUSB_HUB_DRIVER_CONTEXT NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocateNonPagedPool(sizeof(USB_HUB_DRIVER_CONTEXT),
                                       USB_HUB_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(USB_HUB_DRIVER_CONTEXT));
    NewDevice->UsbCoreHandle = INVALID_HANDLE;

    //
    // Attempt to attach to the USB core.
    //

    Status = UsbDriverAttach(DeviceToken,
                             UsbHubDriver,
                             &(NewDevice->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    ASSERT(NewDevice->UsbCoreHandle != INVALID_HANDLE);

    //
    // Allow the USB core to create some hub context with this device.
    //

    Status = UsbCreateHub(NewDevice->UsbCoreHandle, &(NewDevice->UsbHub));
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (NewDevice != NULL) {
            if (NewDevice->UsbCoreHandle != INVALID_HANDLE) {
                UsbDeviceClose(NewDevice->UsbCoreHandle);
            }

            MmFreeNonPagedPool(NewDevice);
        }
    }

    return Status;
}

VOID
UsbHubDispatchStateChange (
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

    PUSB_HUB_DRIVER_CONTEXT Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PUSB_HUB_DRIVER_CONTEXT)DeviceContext;

    //
    // If the device structure is NULL, then the USB hub driver is acting as a
    // bus driver. Complete some IRPs by default.
    //

    if (Device == NULL) {
        switch (Irp->MinorCode) {
        case IrpMinorRemoveDevice:
        case IrpMinorQueryResources:
        case IrpMinorStartDevice:
        case IrpMinorQueryChildren:
            if (Irp->Direction == IrpUp) {
                IoCompleteIrp(UsbHubDriver, Irp, STATUS_SUCCESS);
            }

            break;

        default:
            break;
        }

    //
    // If the device is non-NULL, the driver is acting as a function driver for
    // the USB hub.
    //

    } else {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            if (Irp->Direction == IrpUp) {
                IoCompleteIrp(UsbHubDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorStartDevice:

            //
            // Attempt to fire the thing up if the bus has already started it.
            //

            if (Irp->Direction == IrpUp) {
                Status = UsbHubpStartDevice(Irp, Device);
                if (!KSUCCESS(Status)) {
                    IoCompleteIrp(UsbHubDriver, Irp, Status);
                }
            }

            break;

        case IrpMinorQueryChildren:
            if (Irp->Direction == IrpUp) {
                UsbHubpEnumerateChildren(Irp, Device);
            }

            break;

        case IrpMinorRemoveDevice:
            if (Irp->Direction == IrpUp) {
                UsbHubpRemoveDevice(Irp, Device);
            }

            break;

        //
        // For all other IRPs, do nothing.
        //

        default:
            break;
        }
    }

    return;
}

VOID
UsbHubDispatchOpen (
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
UsbHubDispatchClose (
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
UsbHubDispatchIo (
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
UsbHubDispatchSystemControl (
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
UsbHubpStartDevice (
    PIRP Irp,
    PUSB_HUB_DRIVER_CONTEXT Device
    )

/*++

Routine Description:

    This routine starts up the USB Hub.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this hub device.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = UsbStartHub(Device->UsbHub);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    return Status;
}

VOID
UsbHubpEnumerateChildren (
    PIRP Irp,
    PUSB_HUB_DRIVER_CONTEXT Device
    )

/*++

Routine Description:

    This routine enumerates the USB Hub's children.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this hub device.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Forward this on to the USB core to figure out.
    //

    Status = UsbHubQueryChildren(Irp, Device->UsbHub);
    IoCompleteIrp(UsbHubDriver, Irp, Status);
    return;
}

VOID
UsbHubpRemoveDevice (
    PIRP Irp,
    PUSB_HUB_DRIVER_CONTEXT Device
    )

/*++

Routine Description:

    This routine removes the USB hub device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this hub device.

Return Value:

    None.

--*/

{

    //
    // Set the device for removal in USB core. This will mark the device as
    // disconnected and cancel all of its transfers.
    //

    UsbDetachDevice(Device->UsbCoreHandle);

    //
    // Destroy the hub. This will remove all of the device's children and
    // destroy any hub-specific state.
    //

    UsbDestroyHub(Device->UsbHub);

    //
    // Release the reference on the USB core handle that was taken when the hub
    // device was added.
    //

    UsbDeviceClose(Device->UsbCoreHandle);

    //
    // Free the hub context.
    //

    MmFreeNonPagedPool(Device);
    return;
}
