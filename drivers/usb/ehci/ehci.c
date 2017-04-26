/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ehci.c

Abstract:

    This module implements support for the EHCI USB 2.0 Host controller.

Author:

    Evan Green 18-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/intrface/pci.h>
#include <minoca/usb/usbhost.h>
#include "ehci.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the wait time in seconds for the legacy bit to flip.
//

#define EHCI_LEGACY_SWITCH_TIMEOUT 5

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores context about a EHCI Host Controller.

Members:

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    Controller - Stores a pointer to the EHCI controller.

    PciConfigInterface - Stores the interface to access PCI configuration space.

    PciConfigInterfaceAvailable - Stores a boolean indicating if the PCI
        config interface is actively available.

    RegisteredForPciConfigInterfaces - Stores a boolean indicating whether or
        not the driver has regsistered for PCI Configuration Space interface
        access.

    RegisterBasePhysical - Stores the physical memory address where the EHCI
        registers are located.

    RegisterBase - Stores a pointer to the virtual address where the EHCI
        registers are located.

    OperationalOffset - Stores the offset from the register base where the
        operational registers begin.

    PortCount - Stores the number of ports in this controller.

    ExtendedCapabilitiesOffset - Stores the offset in PCI configuration space
        where the extended capabilities begin. This value must be greater than
        0x40 to be valid (or else it would clash with the PCI spec).

--*/

typedef struct _EHCI_CONTROLLER_CONTEXT {
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PEHCI_CONTROLLER Controller;
    INTERFACE_PCI_CONFIG_ACCESS PciConfigInterface;
    BOOL PciConfigInterfaceAvailable;
    BOOL RegisteredForPciConfigInterfaces;
    PHYSICAL_ADDRESS RegisterBasePhysical;
    PVOID RegisterBase;
    ULONG OperationalOffset;
    ULONG PortCount;
    UCHAR ExtendedCapabilitiesOffset;
} EHCI_CONTROLLER_CONTEXT, *PEHCI_CONTROLLER_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
EhciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
EhciDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
EhciDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
EhciDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
EhciDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
EhciDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
EhcipProcessResourceRequirements (
    PIRP Irp,
    PEHCI_CONTROLLER_CONTEXT Device
    );

KSTATUS
EhcipStartDevice (
    PIRP Irp,
    PEHCI_CONTROLLER_CONTEXT Device
    );

VOID
EhcipEnumerateChildren (
    PIRP Irp,
    PEHCI_CONTROLLER_CONTEXT Device
    );

VOID
EhcipProcessPciConfigInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

KSTATUS
EhcipDisableLegacyInterrupts (
    PEHCI_CONTROLLER_CONTEXT ControllerContext
    );

KSTATUS
EhcipGatherControllerParameters (
    PEHCI_CONTROLLER_CONTEXT ControllerContext,
    PRESOURCE_ALLOCATION ControllerBase
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this flag to avoid bringing up the EHCI driver if there's debug data.
// This is helpful when debugging other drivers that come up at the same time
// as EHCI.
//

BOOL EhciLeaveDebuggerAlone = FALSE;

PDRIVER EhciDriver = NULL;
UUID EhciPciConfigurationInterfaceUuid = UUID_PCI_CONFIG_ACCESS;

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

    This routine is the entry point for the EHCI driver. It registers its other
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

    EhciDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = EhciAddDevice;
    FunctionTable.DispatchStateChange = EhciDispatchStateChange;
    FunctionTable.DispatchOpen = EhciDispatchOpen;
    FunctionTable.DispatchClose = EhciDispatchClose;
    FunctionTable.DispatchIo = EhciDispatchIo;
    FunctionTable.DispatchSystemControl = EhciDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
EhciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the EHCI driver
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

    PEHCI_CONTROLLER_CONTEXT NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocateNonPagedPool(sizeof(EHCI_CONTROLLER_CONTEXT),
                                       EHCI_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(EHCI_CONTROLLER_CONTEXT));
    NewDevice->InterruptHandle = INVALID_HANDLE;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);
    return Status;
}

VOID
EhciDispatchStateChange (
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

    PEHCI_CONTROLLER_CONTEXT Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PEHCI_CONTROLLER_CONTEXT)DeviceContext;

    //
    // If there is no controller context, then EHCI is acting as the bus driver
    // for the root hub. Simply complete standard IRPs.
    //

    if (Device == NULL) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
        case IrpMinorStartDevice:
        case IrpMinorQueryChildren:
            IoCompleteIrp(EhciDriver, Irp, STATUS_SUCCESS);
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
            Status = EhcipProcessResourceRequirements(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(EhciDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorStartDevice:

        //
        // Attempt to fire the thing up if the bus has already started it.
        //

        if (Irp->Direction == IrpUp) {
            Status = EhcipStartDevice(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(EhciDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorQueryChildren:
        if (Irp->Direction == IrpUp) {
            EhcipEnumerateChildren(Irp, Device);
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
EhciDispatchOpen (
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
EhciDispatchClose (
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
EhciDispatchIo (
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
EhciDispatchSystemControl (
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
EhcipProcessResourceRequirements (
    PIRP Irp,
    PEHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a EHCI Host controller. It adds an interrupt vector requirement for
    any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this EHCI device.

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
EhcipStartDevice (
    PIRP Irp,
    PEHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine starts up the EHCI controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this EHCI device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PEHCI_CONTROLLER Controller;
    PRESOURCE_ALLOCATION ControllerBase;
    PDEBUG_HANDOFF_DATA HandoffData;
    PRESOURCE_ALLOCATION LineAllocation;
    KSTATUS Status;
    PDEBUG_USB_HANDOFF_DATA UsbHandoffData;

    Controller = NULL;
    ControllerBase = NULL;

    //
    // Start listening for a PCI config interface.
    //

    if (Device->RegisteredForPciConfigInterfaces == FALSE) {
        Status = IoRegisterForInterfaceNotifications(
                              &EhciPciConfigurationInterfaceUuid,
                              EhcipProcessPciConfigInterfaceChangeNotification,
                              Irp->Device,
                              Device,
                              TRUE);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->RegisteredForPciConfigInterfaces = TRUE;
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
    // Fail to start if the controller base was not found.
    //

    if (ControllerBase == NULL) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Get EHCI register parameters, including the register base and port count.
    //

    Status = EhcipGatherControllerParameters(Device, ControllerBase);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // If there is a PCI configuation interface, shut off the legacy interrupt
    // redirection to SMI land.
    //

    Status = EhcipDisableLegacyInterrupts(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Also fail if the allocation provided by the OS doesn't line up with
    // what's in the registers.
    //

    if (ControllerBase->Allocation != Device->RegisterBasePhysical) {

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Look for handoff data to see if the debugger is using this controller.
    //

    HandoffData = NULL;
    Status = KdGetDeviceInformation(&HandoffData);
    if (KSUCCESS(Status)) {
        if ((HandoffData == NULL) ||
            (HandoffData->PortType != DEBUG_PORT_TYPE_USB) ||
            (HandoffData->PortSubType != DEBUG_PORT_USB_EHCI) ||
            (HandoffData->Identifier != Device->RegisterBasePhysical)) {

            HandoffData = NULL;
        }

    } else {
        HandoffData = NULL;
    }

    if ((HandoffData != NULL) && (EhciLeaveDebuggerAlone != FALSE)) {
        RtlDebugPrint("EHCI: Not starting due to debug device.\n");
        Status = STATUS_RESOURCE_IN_USE;
        goto StartDeviceEnd;
    }

    //
    // Allocate the controller structures.
    //

    UsbHandoffData = NULL;
    if (HandoffData != NULL) {
        UsbHandoffData = &(HandoffData->U.Usb);
    }

    Controller = EhcipInitializeControllerState(
                              Device->RegisterBase + Device->OperationalOffset,
                              Device->RegisterBasePhysical,
                              Device->PortCount,
                              UsbHandoffData);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto StartDeviceEnd;
    }

    Device->Controller = Controller;

    //
    // Start up the controller.
    //

    Status = EhcipResetController(Controller);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Register the device with the USB core. This is required before enabling
    // the interrupt.
    //

    Status = EhcipRegisterController(Controller, Irp->Device);
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
    Connect.InterruptServiceRoutine = EhcipInterruptService;
    Connect.DispatchServiceRoutine = EhcipInterruptServiceDpc;
    Connect.Context = Device->Controller;
    Connect.Interrupt = &(Device->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    EhcipSetInterruptHandle(Controller, Device->InterruptHandle);

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->InterruptHandle);
            Device->InterruptHandle = INVALID_HANDLE;
        }

        if (Controller != NULL) {
            EhcipDestroyControllerState(Controller);
        }
    }

    return Status;
}

VOID
EhcipEnumerateChildren (
    PIRP Irp,
    PEHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine enumerates the root hub of a EHCI controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this EHCI device.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Forward this on to the USB core to figure out.
    //

    Status = UsbHostQueryChildren(Irp, Device->Controller->UsbCoreHandle);
    IoCompleteIrp(EhciDriver, Irp, Status);
    return;
}

VOID
EhcipProcessPciConfigInterfaceChangeNotification (
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

    PEHCI_CONTROLLER_CONTEXT ControllerContext;

    ControllerContext = (PEHCI_CONTROLLER_CONTEXT)Context;
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
EhcipDisableLegacyInterrupts (
    PEHCI_CONTROLLER_CONTEXT ControllerContext
    )

/*++

Routine Description:

    This routine disables routing of EHCI interrupts to SMI land (which is used
    to emulate a PS/2 keyboard when a USB keyboard is connected).
    Without running this, the BIOS would continue to think it owned the EHCI
    controller, causing both register clashes and the OS not to get interrupts.

Arguments:

    ControllerContext - Supplies a pointer to the EHCI controller context.

Return Value:

    Status code.

--*/

{

    ULONGLONG LegacyControl;
    ULONG LegacyControlRegister;
    PVOID PciDeviceToken;
    PREAD_PCI_CONFIG ReadPciConfig;
    KSTATUS Status;
    BOOL TimedOut;
    ULONGLONG Timeout;
    PWRITE_PCI_CONFIG WritePciConfig;

    //
    // If no PCI config interface is available, then this must not be a legacy
    // platform.
    //

    if ((ControllerContext->PciConfigInterfaceAvailable == FALSE) ||
        (ControllerContext->ExtendedCapabilitiesOffset == 0)) {

        Status = STATUS_SUCCESS;
        goto DisableLegacyInterruptsEnd;
    }

    ReadPciConfig = ControllerContext->PciConfigInterface.ReadPciConfig;
    WritePciConfig = ControllerContext->PciConfigInterface.WritePciConfig;
    PciDeviceToken = ControllerContext->PciConfigInterface.DeviceToken;

    //
    // Check to see if the EHCI controller is owned by the OS. If it is still
    // owned by the BIOS, claim ownership, and wait for the BIOS to agree.
    //

    LegacyControlRegister = ControllerContext->ExtendedCapabilitiesOffset +
                            EHCI_EECP_LEGACY_SUPPORT_REGISTER;

    Status = ReadPciConfig(PciDeviceToken,
                           LegacyControlRegister,
                           sizeof(ULONG),
                           &LegacyControl);

    if (!KSUCCESS(Status)) {
        goto DisableLegacyInterruptsEnd;
    }

    if ((LegacyControl & EHCI_LEGACY_SUPPORT_BIOS_OWNED) != 0) {

        //
        // If both the OS and BIOS owned bits are set, this is an indication
        // something more serious is wrong, or these are not really EHCI
        // registers.
        //

        ASSERT((LegacyControl & EHCI_LEGACY_SUPPORT_OS_OWNED) == 0);

        //
        // Write the "OS owned" bit to request that the BIOS stop trying to be
        // helpful and get out of the way.
        //

        LegacyControl |= EHCI_LEGACY_SUPPORT_OS_OWNED;
        Status = WritePciConfig(PciDeviceToken,
                                LegacyControlRegister,
                                sizeof(ULONG),
                                LegacyControl);

        //
        // Now loop waiting for the BIOS to give it up.
        //

        Timeout = KeGetRecentTimeCounter() +
                  (HlQueryTimeCounterFrequency() * EHCI_LEGACY_SWITCH_TIMEOUT);

        TimedOut = TRUE;
        do {
            Status = ReadPciConfig(PciDeviceToken,
                                   LegacyControlRegister,
                                   sizeof(ULONG),
                                   &LegacyControl);

            if (!KSUCCESS(Status)) {
                goto DisableLegacyInterruptsEnd;
            }

            if ((LegacyControl & EHCI_LEGACY_SUPPORT_BIOS_OWNED) == 0) {
                Status = STATUS_SUCCESS;
                TimedOut = FALSE;
                break;
            }

        } while (KeGetRecentTimeCounter() <= Timeout);

        if (TimedOut != FALSE) {
            RtlDebugPrint("EHCI: BIOS failed to relinquish control: 0x%I64x\n",
                          LegacyControl);

            Status = STATUS_TIMEOUT;
        }

        if (!KSUCCESS(Status)) {
            goto DisableLegacyInterruptsEnd;
        }
    }

    Status = STATUS_SUCCESS;

DisableLegacyInterruptsEnd:
    return Status;
}

KSTATUS
EhcipGatherControllerParameters (
    PEHCI_CONTROLLER_CONTEXT ControllerContext,
    PRESOURCE_ALLOCATION ControllerBase
    )

/*++

Routine Description:

    This routine pokes around and collects various pieces of needed information
    for the controller, such as the register base, operational offset, and
    port count.

Arguments:

    ControllerContext - Supplies a pointer to the EHCI controller context.

    ControllerBase - Supplies a pointer to the resource allocation defining the
        location of the controller's registers.

Return Value:

    Status code.

--*/

{

    ULONG AlignmentOffset;
    ULONG Capabilities;
    PVOID CapabilitiesRegister;
    PHYSICAL_ADDRESS EndAddress;
    PVOID LengthRegister;
    ULONG PageSize;
    ULONG Parameters;
    PVOID ParametersRegister;
    PVOID PciDeviceToken;
    PHYSICAL_ADDRESS PhysicalAddress;
    PREAD_PCI_CONFIG ReadPciConfig;
    ULONG Size;
    KSTATUS Status;
    ULONGLONG Value;
    PVOID VirtualAddress;

    //
    // If a PCI config interface is available, verify the base address.
    //

    if (ControllerContext->PciConfigInterfaceAvailable != FALSE) {

        //
        // Read the register base register to find out where all the other
        // registers begin in memory.
        //

        ReadPciConfig = ControllerContext->PciConfigInterface.ReadPciConfig;
        PciDeviceToken = ControllerContext->PciConfigInterface.DeviceToken;
        if (ControllerContext->RegisterBasePhysical == 0) {
            Status = ReadPciConfig(PciDeviceToken,
                                   EHCI_USB_REGISTER_BASE_REGISTER,
                                   sizeof(ULONG),
                                   &Value);

            if (!KSUCCESS(Status)) {
                goto GatherControllerParametersEnd;
            }

            PhysicalAddress = (ULONG)Value &
                              EHCI_USB_REGISTER_BASE_ADDRESS_MASK;

            ASSERT(PhysicalAddress == ControllerBase->Allocation);

            ControllerContext->RegisterBasePhysical = PhysicalAddress;
        }

    } else {
        ControllerContext->RegisterBasePhysical = ControllerBase->Allocation;
    }

    //
    // Map those registers if needed.
    //

    ASSERT(ControllerContext->RegisterBasePhysical != 0);

    if (ControllerContext->RegisterBase == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();

        ASSERT(ControllerContext->RegisterBasePhysical ==
               ControllerBase->Allocation);

        PhysicalAddress = ControllerContext->RegisterBasePhysical;
        EndAddress = PhysicalAddress + ControllerBase->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = ControllerContext->RegisterBasePhysical -
                          PhysicalAddress;

        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);
        VirtualAddress = MmMapPhysicalAddress(PhysicalAddress,
                                              Size,
                                              TRUE,
                                              FALSE,
                                              TRUE);

        if (VirtualAddress == NULL) {
            Status = STATUS_NO_MEMORY;
            goto GatherControllerParametersEnd;
        }

        ControllerContext->RegisterBase = VirtualAddress + AlignmentOffset;
    }

    ASSERT(ControllerContext->RegisterBase != NULL);

    //
    // Read the operational offset if needed.
    //

    if (ControllerContext->OperationalOffset == 0) {
        LengthRegister = ControllerContext->RegisterBase +
                         EHCI_CAPABILITY_LENGTH_REGISTER;

        ControllerContext->OperationalOffset = HlReadRegister8(LengthRegister);
    }

    //
    // Read the port count and other structural parameters if needed.
    //

    if (ControllerContext->PortCount == 0) {
        ParametersRegister = ControllerContext->RegisterBase +
                             EHCI_CAPABILITY_PARAMETERS_REGISTER;

        Parameters = HlReadRegister32(ParametersRegister);
        ControllerContext->PortCount =
                       Parameters & EHCI_CAPABILITY_PARAMETERS_PORT_COUNT_MASK;
    }

    if (ControllerContext->PortCount == 0) {

        ASSERT(FALSE);

        Status = STATUS_NO_SUCH_DEVICE;
        goto GatherControllerParametersEnd;
    }

    //
    // Grab the extended capabilities offset.
    //

    if (ControllerContext->ExtendedCapabilitiesOffset == 0) {
        CapabilitiesRegister = ControllerContext->RegisterBase +
                               EHCI_CAPABILITY_CAPABILITIES_REGISTER;

        Capabilities = HlReadRegister32(CapabilitiesRegister);
        ControllerContext->ExtendedCapabilitiesOffset =
                   (Capabilities &
                    EHCI_CAPABILITY_CAPABILITIES_EXTENDED_CAPABILITIES_MASK) >>
                   EHCI_CAPABILITY_CAPABILITIES_EXTENDED_CAPABILITIES_SHIFT;
    }

    Status = STATUS_SUCCESS;

GatherControllerParametersEnd:
    return Status;
}

