/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uhci.c

Abstract:

    This module implements support for the UHCI USB Host controller.

Author:

    Evan Green 13-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/pci.h>
#include <minoca/usb/usbhost.h>
#include "uhci.h"

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

    This structure stores context about a UHCI Host Controller.

Members:

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    Controller - Stores a pointer to the UHCI controller.

    PciConfigInterface - Stores the interface to access PCI configuration space.

    PciConfigInterfaceAvailable - Stores a boolean indicating if the PCI
        config interface is actively available.

    RegisteredForPciConfigInterfaces - Stores a boolean indicating whether or
        not the driver has regsistered for PCI Configuration Space interface
        access.

--*/

typedef struct _UHCI_CONTROLLER_CONTEXT {
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PUHCI_CONTROLLER Controller;
    INTERFACE_PCI_CONFIG_ACCESS PciConfigInterface;
    BOOL PciConfigInterfaceAvailable;
    BOOL RegisteredForPciConfigInterfaces;
} UHCI_CONTROLLER_CONTEXT, *PUHCI_CONTROLLER_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UhciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
UhciDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UhciDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UhciDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UhciDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UhciDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
UhcipProcessResourceRequirements (
    PIRP Irp,
    PUHCI_CONTROLLER_CONTEXT Device
    );

KSTATUS
UhcipStartDevice (
    PIRP Irp,
    PUHCI_CONTROLLER_CONTEXT Device
    );

VOID
UhcipEnumerateChildren (
    PIRP Irp,
    PUHCI_CONTROLLER_CONTEXT Device
    );

VOID
UhcipProcessPciConfigInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

KSTATUS
UhcipDisableLegacyInterrupts (
    PUHCI_CONTROLLER_CONTEXT ControllerContext
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER UhciDriver = NULL;
UUID UhciPciConfigurationInterfaceUuid = UUID_PCI_CONFIG_ACCESS;

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

    This routine is the entry point for the UHCI driver. It registers its other
    dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    UhciDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = UhciAddDevice;
    FunctionTable.DispatchStateChange = UhciDispatchStateChange;
    FunctionTable.DispatchOpen = UhciDispatchOpen;
    FunctionTable.DispatchClose = UhciDispatchClose;
    FunctionTable.DispatchIo = UhciDispatchIo;
    FunctionTable.DispatchSystemControl = UhciDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UhciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the UHCI driver
    acts as the function driver. The driver will attach itself to the stack.

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

    PUHCI_CONTROLLER_CONTEXT NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocateNonPagedPool(sizeof(UHCI_CONTROLLER_CONTEXT),
                                       UHCI_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(UHCI_CONTROLLER_CONTEXT));
    NewDevice->InterruptHandle = INVALID_HANDLE;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);
    return Status;
}

VOID
UhciDispatchStateChange (
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

    PUHCI_CONTROLLER_CONTEXT Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PUHCI_CONTROLLER_CONTEXT)DeviceContext;

    //
    // If there is no controller context, then UHCI is acting as the bus driver
    // for the root hub. Simply complete standard IRPs.
    //

    if (Device == NULL) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
        case IrpMinorStartDevice:
        case IrpMinorQueryChildren:
            IoCompleteIrp(UhciDriver, Irp, STATUS_SUCCESS);
            break;

        default:
            break;
        }

        return;
    }

    if ((Irp->Direction == IrpUp) && (!KSUCCESS(IoGetIrpStatus(Irp)))) {
        return;
    }

    switch (Irp->MinorCode) {
    case IrpMinorQueryResources:

        //
        // On the way up, filter the resource requirements to add interrupt
        // vectors to any lines.
        //

        if (Irp->Direction == IrpUp) {
            Status = UhcipProcessResourceRequirements(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(UhciDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorStartDevice:

        //
        // Attempt to fire the thing up if the bus has already started it.
        //

        if (Irp->Direction == IrpUp) {
            Status = UhcipStartDevice(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(UhciDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorQueryChildren:
        if (Irp->Direction == IrpUp) {
            UhcipEnumerateChildren(Irp, Device);
        }

        break;

    case IrpMinorRemoveDevice:

        ASSERT(FALSE);

        break;

    //
    // For all other IRPs, do nothing.
    //

    default:
        break;
    }

    return;
}

VOID
UhciDispatchOpen (
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
UhciDispatchClose (
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
UhciDispatchIo (
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
UhciDispatchSystemControl (
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
UhcipProcessResourceRequirements (
    PIRP Irp,
    PUHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a UHCI Host controller. It adds an interrupt vector requirement for
    any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this UHCI device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_CONFIGURATION_LIST Requirements;
    KSTATUS Status;
    RESOURCE_REQUIREMENT VectorRequirement;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Initialize a nice interrupt vector requirement in preparation.
    //

    RtlZeroMemory(&VectorRequirement, sizeof(RESOURCE_REQUIREMENT));
    VectorRequirement.Type = ResourceTypeInterruptVector;
    VectorRequirement.Minimum = 0;
    VectorRequirement.Maximum = -1;
    VectorRequirement.Length = 1;

    //
    // Loop through all configuration lists, creating a vector for each line.
    //

    Requirements = Irp->U.QueryResources.ResourceRequirements;
    Status = IoCreateAndAddInterruptVectorsForLines(Requirements,
                                                    &VectorRequirement);

    if (!KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

ProcessResourceRequirementsEnd:
    return Status;
}

KSTATUS
UhcipStartDevice (
    PIRP Irp,
    PUHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine starts up the UHCI controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this UHCI device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PUHCI_CONTROLLER Controller;
    PRESOURCE_ALLOCATION ControllerBase;
    PRESOURCE_ALLOCATION LineAllocation;
    KSTATUS Status;

    Controller = NULL;
    ControllerBase = NULL;

    //
    // Start listening for a PCI config interface.
    //

    if (Device->RegisteredForPciConfigInterfaces == FALSE) {
        Status = IoRegisterForInterfaceNotifications(
                              &UhciPciConfigurationInterfaceUuid,
                              UhcipProcessPciConfigInterfaceChangeNotification,
                              Irp->Device,
                              Device,
                              TRUE);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->RegisteredForPciConfigInterfaces = TRUE;
    }

    //
    // If there is a PCI configuation interface, shut off the legacy interrupt
    // redirection to SMI land.
    //

    Status = UhcipDisableLegacyInterrupts(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // If the resource is an interrupt vector, then it should have an
        // owning interrupt line allocation.
        //

        if (Allocation->Type == ResourceTypeInterruptVector) {

            //
            // Currently only one interrupt resource is expected.
            //

            ASSERT(Device->InterruptResourcesFound == FALSE);
            ASSERT(Allocation->OwningAllocation != NULL);

            //
            // Save the line and vector number.
            //

            LineAllocation = Allocation->OwningAllocation;
            Device->InterruptLine = LineAllocation->Allocation;
            Device->InterruptVector = Allocation->Allocation;
            Device->InterruptResourcesFound = TRUE;

        } else if (Allocation->Type == ResourceTypeIoPort) {

            ASSERT(ControllerBase == NULL);

            ControllerBase = Allocation;
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Fail to start if the controller base was not found.
    //

    if (ControllerBase == NULL) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Allocate the controller structures.
    //

    Controller = UhcipInitializeControllerState(
                                            (ULONG)ControllerBase->Allocation);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto StartDeviceEnd;
    }

    Device->Controller = Controller;

    //
    // Start up the controller.
    //

    Status = UhcipResetController(Controller);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Register the device with the USB core. The interrupt service interacts
    // with the USB core, so the controller must register itself first.
    //

    Status = UhcipRegisterController(Controller, Irp->Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Attempt to connect the interrupt.
    //

    ASSERT(Device->InterruptHandle == INVALID_HANDLE);

    RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
    Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
    Connect.Device = Irp->Device;
    Connect.LineNumber = Device->InterruptLine;
    Connect.Vector = Device->InterruptVector;
    Connect.InterruptServiceRoutine = UhcipInterruptService;
    Connect.DispatchServiceRoutine = UhcipInterruptServiceDpc;
    Connect.Context = Device->Controller;
    Connect.Interrupt = &(Device->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    UhcipSetInterruptHandle(Controller, Device->InterruptHandle);

    //
    // Start polling for port changes.
    //

    Status = UhcipInitializePortChangeDetection(Controller);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->InterruptHandle);
            Device->InterruptHandle = INVALID_HANDLE;
        }

        if (Controller != NULL) {
            UhcipDestroyControllerState(Controller);
        }
    }

    return Status;
}

VOID
UhcipEnumerateChildren (
    PIRP Irp,
    PUHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine enumerates the root hub of a UHCI controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this UHCI device.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Forward this on to the USB core to figure out.
    //

    Status = UsbHostQueryChildren(Irp, Device->Controller->UsbCoreHandle);
    IoCompleteIrp(UhciDriver, Irp, Status);
    return;
}

VOID
UhcipProcessPciConfigInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine is called when a PCI configuration space access interface
    changes in availability.

Arguments:

    Context - Supplies the caller's context pointer, supplied when the caller
        requested interface notifications.

    Device - Supplies a pointer to the device exposing or deleting the
        interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer of the
        interface.

    InterfaceBufferSize - Supplies the buffer size.

    Arrival - Supplies TRUE if a new interface is arriving, or FALSE if an
        interface is departing.

Return Value:

    None.

--*/

{

    PUHCI_CONTROLLER_CONTEXT ControllerContext;

    ControllerContext = (PUHCI_CONTROLLER_CONTEXT)Context;
    if (Arrival != FALSE) {
        if (InterfaceBufferSize >= sizeof(INTERFACE_PCI_CONFIG_ACCESS)) {

            ASSERT(ControllerContext->PciConfigInterfaceAvailable == FALSE);

            RtlCopyMemory(&(ControllerContext->PciConfigInterface),
                          InterfaceBuffer,
                          sizeof(INTERFACE_PCI_CONFIG_ACCESS));

            ControllerContext->PciConfigInterfaceAvailable = TRUE;
        }

    } else {
        ControllerContext->PciConfigInterfaceAvailable = FALSE;
    }

    return;
}

KSTATUS
UhcipDisableLegacyInterrupts (
    PUHCI_CONTROLLER_CONTEXT ControllerContext
    )

/*++

Routine Description:

    This routine disables routing of UHCI interrupts to SMI land (which is used
    to emulate a PS/2 keyboard when a USB keyboard is connected). Without this,
    UHCI interrupts would never come in.

Arguments:

    ControllerContext - Supplies a pointer to the UHCI controller context.

Return Value:

    Status code.

--*/

{

    PVOID PciDeviceToken;
    KSTATUS Status;
    PWRITE_PCI_CONFIG WritePciConfig;

    //
    // If no interface is available, nothing can be done. At this point, UHCI
    // is only supported on the PC platform under PCI, so it's always expected
    // that the interface will be available.
    //

    if (ControllerContext->PciConfigInterfaceAvailable == FALSE) {

        ASSERT(FALSE);

        Status = STATUS_SUCCESS;
        goto DisableLegacyInterruptsEnd;
    }

    //
    // Write the handoff value to enable UHCI interrupts.
    //

    WritePciConfig = ControllerContext->PciConfigInterface.WritePciConfig;
    PciDeviceToken = ControllerContext->PciConfigInterface.DeviceToken;
    Status = WritePciConfig(PciDeviceToken,
                            UHCI_LEGACY_SUPPORT_REGISTER_OFFSET,
                            sizeof(USHORT),
                            UHCI_LEGACY_SUPPORT_ENABLE_USB_INTERRUPTS);

    if (!KSUCCESS(Status)) {
        goto DisableLegacyInterruptsEnd;
    }

DisableLegacyInterruptsEnd:
    return Status;
}

