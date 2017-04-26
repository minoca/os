/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am3usb.c

Abstract:

    This module implements support for the USB controller in the TI AM33xx SoCs.

Author:

    Evan Green 11-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usbhost.h>
#include "am3usb.h"

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

    This structure stores context about an AM33xx USB Host Controller.

Members:

    Controller - Stores a pointer to the AM33xx USB controller.

    RegisterBase - Stores a pointer to the virtual address where the AM33xx USB
        registers are located.

    PhysicalBase - Stores the physical address of the register base.

--*/

typedef struct _AM3_USB_CONTROLLER_CONTEXT {
    AM3_USB_CONTROLLER Controller;
    PVOID RegisterBase;
    PHYSICAL_ADDRESS PhysicalBase;
} AM3_USB_CONTROLLER_CONTEXT, *PAM3_USB_CONTROLLER_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Am3UsbAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Am3UsbDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3UsbDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3UsbDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3UsbDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3UsbDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
Am3UsbpProcessResourceRequirements (
    PIRP Irp,
    PAM3_USB_CONTROLLER_CONTEXT Device
    );

KSTATUS
Am3UsbpStartDevice (
    PIRP Irp,
    PAM3_USB_CONTROLLER_CONTEXT Device
    );

VOID
Am3UsbpEnumerateChildren (
    PIRP Irp,
    PAM3_USB_CONTROLLER_CONTEXT Device
    );

KSTATUS
Am3UsbpInitializeControllerState (
    PAM3_USB_CONTROLLER Controller,
    PVOID RegisterBase,
    PHYSICAL_ADDRESS PhysicalBase
    );

VOID
Am3UsbpDestroyControllerState (
    PAM3_USB_CONTROLLER Controller
    );

KSTATUS
Am3UsbpResetController (
    PAM3_USB_CONTROLLER Controller
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Am3UsbDriver = NULL;

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

    This routine is the entry point for the AM33xx USB driver. It registers its
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

    Am3UsbDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Am3UsbAddDevice;
    FunctionTable.DispatchStateChange = Am3UsbDispatchStateChange;
    FunctionTable.DispatchOpen = Am3UsbDispatchOpen;
    FunctionTable.DispatchClose = Am3UsbDispatchClose;
    FunctionTable.DispatchIo = Am3UsbDispatchIo;
    FunctionTable.DispatchSystemControl = Am3UsbDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Am3UsbAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the AM33xx USB
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

    PAM3_USB_CONTROLLER_CONTEXT NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocateNonPagedPool(sizeof(AM3_USB_CONTROLLER_CONTEXT),
                                       AM3_USB_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(AM3_USB_CONTROLLER_CONTEXT));
    NewDevice->Controller.UsbSs.InterruptLine = -1ULL;
    NewDevice->Controller.UsbSs.InterruptVector = -1ULL;
    NewDevice->Controller.UsbSs.InterruptHandle = INVALID_HANDLE;
    NewDevice->Controller.Usb[0].InterruptLine = -1ULL;
    NewDevice->Controller.Usb[0].InterruptVector = -1ULL;
    NewDevice->Controller.Usb[0].InterruptHandle = INVALID_HANDLE;
    NewDevice->Controller.Usb[1].InterruptLine = -1ULL;
    NewDevice->Controller.Usb[1].InterruptVector = -1ULL;
    NewDevice->Controller.Usb[1].InterruptHandle = INVALID_HANDLE;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);
    return Status;
}

VOID
Am3UsbDispatchStateChange (
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

    PAM3_USB_CONTROLLER_CONTEXT Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PAM3_USB_CONTROLLER_CONTEXT)DeviceContext;

    //
    // If there is no controller context, then AM33xx USB is acting as the bus
    // driver for a root hub. Simply complete standard IRPs.
    //

    if (Device == NULL) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
        case IrpMinorStartDevice:
        case IrpMinorQueryChildren:
            IoCompleteIrp(Am3UsbDriver, Irp, STATUS_SUCCESS);
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
            Status = Am3UsbpProcessResourceRequirements(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Am3UsbDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorStartDevice:

        //
        // Attempt to fire the thing up if the bus has already started it.
        //

        if (Irp->Direction == IrpUp) {
            Status = Am3UsbpStartDevice(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Am3UsbDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorQueryChildren:
        if (Irp->Direction == IrpUp) {
            Am3UsbpEnumerateChildren(Irp, Device);
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
Am3UsbDispatchOpen (
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
Am3UsbDispatchClose (
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
Am3UsbDispatchIo (
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
Am3UsbDispatchSystemControl (
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
Am3UsbpProcessResourceRequirements (
    PIRP Irp,
    PAM3_USB_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a AM33xx USB Host controller. It adds an interrupt vector
    requirement for any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this AM33xx USB device.

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
Am3UsbpStartDevice (
    PIRP Irp,
    PAM3_USB_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine starts up the AM33xx USB controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this AM33xx USB device.

Return Value:

    Status code.

--*/

{

    ULONG AlignmentOffset;
    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PAM3_USB_CONTROLLER Controller;
    PRESOURCE_ALLOCATION ControllerBase;
    PHYSICAL_ADDRESS EndAddress;
    UINTN PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Size;
    KSTATUS Status;
    PAM3_USB_CONTROL Usb0;
    PAM3_USB_CONTROL Usb1;
    PAM3_USBSS_CONTROLLER UsbSs;
    PVOID VirtualAddress;

    Controller = &(Device->Controller);
    ControllerBase = NULL;
    Usb0 = &(Device->Controller.Usb[0]);
    Usb1 = &(Device->Controller.Usb[1]);
    UsbSs = &(Device->Controller.UsbSs);

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {
        if (Allocation->Type == ResourceTypeInterruptVector) {

            ASSERT(Allocation->OwningAllocation != NULL);

            if (UsbSs->InterruptVector == -1ULL) {
                UsbSs->InterruptVector = Allocation->Allocation;
                UsbSs->InterruptLine = Allocation->OwningAllocation->Allocation;

            } else if (Usb0->InterruptVector == -1ULL) {
                Usb0->InterruptVector = Allocation->Allocation;
                Usb0->InterruptLine = Allocation->OwningAllocation->Allocation;

            } else if (Usb1->InterruptVector == -1ULL) {
                Usb1->InterruptVector = Allocation->Allocation;
                Usb1->InterruptLine = Allocation->OwningAllocation->Allocation;
            }

        } else if (Allocation->Type == ResourceTypePhysicalAddressSpace) {

            ASSERT(ControllerBase == NULL);

            ControllerBase = Allocation;
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Fail to start if the controller base was not found or is not big enough.
    //

    if ((ControllerBase == NULL) ||
        (ControllerBase->Length < AM335_USB_REGION_SIZE)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    if ((UsbSs->InterruptVector == -1ULL) ||
        (Usb0->InterruptVector == -1ULL) ||
        (Usb1->InterruptVector == -1ULL)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Map the region.
    //

    if (Device->RegisterBase == NULL) {

        ASSERT(ControllerBase->Length == AM335_USB_REGION_SIZE);

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = ControllerBase->Allocation;
        EndAddress = PhysicalAddress + ControllerBase->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        Device->PhysicalBase = PhysicalAddress;
        AlignmentOffset = ControllerBase->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);
        VirtualAddress = MmMapPhysicalAddress(PhysicalAddress,
                                              Size,
                                              TRUE,
                                              FALSE,
                                              TRUE);

        if (VirtualAddress == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->RegisterBase = VirtualAddress + AlignmentOffset;
    }

    //
    // Allocate the controller structures.
    //

    Status = Am3UsbpInitializeControllerState(Controller,
                                              Device->RegisterBase,
                                              Device->PhysicalBase);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Start up the controller.
    //

    Status = Am3UsbpResetController(Controller);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Register the devices with the USB core. This is required before enabling
    // the interrupt.
    //

    Status = MusbRegisterController(&(Controller->Usb[0].MentorUsb),
                                    Irp->Device);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    Status = MusbRegisterController(&(Controller->Usb[1].MentorUsb),
                                    Irp->Device);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Attempt to connect the interrupts.
    //

    ASSERT(UsbSs->InterruptHandle == INVALID_HANDLE);

    RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
    Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
    Connect.Device = Irp->Device;
    Connect.LineNumber = UsbSs->InterruptLine;
    Connect.Vector = UsbSs->InterruptVector;
    Connect.InterruptServiceRoutine = Am3UsbssInterruptService;
    Connect.DispatchServiceRoutine = Am3UsbssInterruptServiceDpc;
    Connect.Context = UsbSs;
    Connect.Interrupt = &(UsbSs->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    ASSERT(Usb0->InterruptHandle == INVALID_HANDLE);

    Connect.LineNumber = Usb0->InterruptLine;
    Connect.Vector = Usb0->InterruptVector;
    Connect.InterruptServiceRoutine = Am3UsbInterruptService;
    Connect.DispatchServiceRoutine = Am3UsbInterruptServiceDpc;
    Connect.Context = Usb0;
    Connect.Interrupt = &(Usb0->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    ASSERT(Usb1->InterruptHandle == INVALID_HANDLE);

    Connect.LineNumber = Usb1->InterruptLine;
    Connect.Vector = Usb1->InterruptVector;
    Connect.Context = Usb1;
    Connect.Interrupt = &(Usb1->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (UsbSs->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(UsbSs->InterruptHandle);
            UsbSs->InterruptHandle = INVALID_HANDLE;
        }

        if (Usb0->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Usb0->InterruptHandle);
            Usb0->InterruptHandle = INVALID_HANDLE;
        }

        if (Usb1->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Usb1->InterruptHandle);
            Usb1->InterruptHandle = INVALID_HANDLE;
        }

        if (Device->RegisterBase != NULL) {
            MmUnmapAddress(Device->RegisterBase, AM335_USB_REGION_SIZE);
        }

        if (Controller != NULL) {
            Am3UsbpDestroyControllerState(Controller);
        }
    }

    return Status;
}

VOID
Am3UsbpEnumerateChildren (
    PIRP Irp,
    PAM3_USB_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine enumerates the root hub of a AM33xx USB controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this AM33xx USB device.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    Status = STATUS_NOT_READY;

    //
    // Forward this on to the USB core to figure out.
    //

    if (Device->Controller.Usb[0].MentorUsb.UsbCoreHandle != NULL) {
        Status = UsbHostQueryChildren(
                            Irp,
                            Device->Controller.Usb[0].MentorUsb.UsbCoreHandle);

        if (!KSUCCESS(Status)) {
            goto EnumerateChildrenEnd;
        }
    }

    if (Device->Controller.Usb[1].MentorUsb.UsbCoreHandle != NULL) {
        Status = UsbHostQueryChildren(
                            Irp,
                            Device->Controller.Usb[1].MentorUsb.UsbCoreHandle);

        if (!KSUCCESS(Status)) {
            goto EnumerateChildrenEnd;
        }
    }

EnumerateChildrenEnd:
    IoCompleteIrp(Am3UsbDriver, Irp, Status);
    return;
}

KSTATUS
Am3UsbpInitializeControllerState (
    PAM3_USB_CONTROLLER Controller,
    PVOID RegisterBase,
    PHYSICAL_ADDRESS PhysicalBase
    )

/*++

Routine Description:

    This routine initializes data structures for the AM335 USB controllers.

Arguments:

    Controller - Supplies a pointer to the controller structure, which has
        already been zeroed.

    RegisterBase - Supplies the virtual address of the registers for the
        device.

    PhysicalBase - Supplies the physical address of the register base.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = CppiInitializeControllerState(
                                       &(Controller->CppiDma),
                                       RegisterBase + AM3_USB_CPPI_DMA_OFFSET);

    if (!KSUCCESS(Status)) {
        goto InitializeControllerStateEnd;
    }

    Status = Am3UsbssInitializeControllerState(
                                          &(Controller->UsbSs),
                                          RegisterBase + AM3_USB_USBSS_OFFSET,
                                          &(Controller->CppiDma));

    if (!KSUCCESS(Status)) {
        goto InitializeControllerStateEnd;
    }

    Controller->Usb[0].ControllerBase = RegisterBase + AM3_USB_USB0_OFFSET;
    Status = MusbInitializeControllerState(
                                      &(Controller->Usb[0].MentorUsb),
                                      RegisterBase + AM3_USB_USB0_CORE_OFFSET,
                                      Am3UsbDriver,
                                      PhysicalBase + AM3_USB_USB0_CORE_OFFSET,
                                      &(Controller->CppiDma),
                                      0);

    if (!KSUCCESS(Status)) {
        goto InitializeControllerStateEnd;
    }

    Controller->Usb[1].ControllerBase = RegisterBase + AM3_USB_USB1_OFFSET;
    Status = MusbInitializeControllerState(
                                      &(Controller->Usb[1].MentorUsb),
                                      RegisterBase + AM3_USB_USB1_CORE_OFFSET,
                                      Am3UsbDriver,
                                      PhysicalBase + AM3_USB_USB1_CORE_OFFSET,
                                      &(Controller->CppiDma),
                                      1);

    if (!KSUCCESS(Status)) {
        goto InitializeControllerStateEnd;
    }

InitializeControllerStateEnd:
    return Status;
}

VOID
Am3UsbpDestroyControllerState (
    PAM3_USB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine tears down all state associated with the given AM33xx USB
    controller. The structure itself is not freed since it was passed in on
    initialize.

Arguments:

    Controller - Supplies a pointer to the controller structure to tear down.

Return Value:

    None.

--*/

{

    MusbDestroyControllerState(&(Controller->Usb[0].MentorUsb));
    MusbDestroyControllerState(&(Controller->Usb[1].MentorUsb));
    CppiDestroyControllerState(&(Controller->CppiDma));
    Am3UsbssDestroyControllerState(&(Controller->UsbSs));
    return;
}

KSTATUS
Am3UsbpResetController (
    PAM3_USB_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine performs a hardware reset and initialization of the given
    controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = Am3UsbssResetController(&(Controller->UsbSs));
    if (!KSUCCESS(Status)) {
        goto ResetControllerEnd;
    }

    Status = CppiResetController(&(Controller->CppiDma));
    if (!KSUCCESS(Status)) {
        goto ResetControllerEnd;
    }

    Status = Am3UsbControlReset(&(Controller->Usb[0]));
    if (!KSUCCESS(Status)) {
        goto ResetControllerEnd;
    }

    Status = Am3UsbControlReset(&(Controller->Usb[1]));
    if (!KSUCCESS(Status)) {
        goto ResetControllerEnd;
    }

ResetControllerEnd:
    return Status;
}

