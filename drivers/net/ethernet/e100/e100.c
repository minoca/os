/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    e100.c

Abstract:

    This module implements the Intel e100 integrated LAN driver.

Author:

    Evan Green 4-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/intrface/pci.h>
#include "e100.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// PCI Configuration Space definitions.
//

#define PCI_REVISION_ID_OFFSET 0x8
#define PCI_REVISION_ID_MASK 0x000000FF

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
E100AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
E100DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
E100DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
E100DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
E100DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
E100DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
E100DestroyLink (
    PVOID DeviceContext
    );

KSTATUS
E100pProcessResourceRequirements (
    PIRP Irp,
    PE100_DEVICE Device
    );

KSTATUS
E100pStartDevice (
    PIRP Irp,
    PE100_DEVICE Device
    );

VOID
E100pProcessPciConfigInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER E100Driver = NULL;
UUID E100PciConfigurationInterfaceUuid = UUID_PCI_CONFIG_ACCESS;

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

    This routine is the entry point for the e100 driver. It registers its other
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

    E100Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = E100AddDevice;
    FunctionTable.DispatchStateChange = E100DispatchStateChange;
    FunctionTable.DispatchOpen = E100DispatchOpen;
    FunctionTable.DispatchClose = E100DispatchClose;
    FunctionTable.DispatchIo = E100DispatchIo;
    FunctionTable.DispatchSystemControl = E100DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
E100AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the e100 driver
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

    PE100_DEVICE Device;
    KSTATUS Status;

    Device = MmAllocateNonPagedPool(sizeof(E100_DEVICE), E100_ALLOCATION_TAG);
    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Device, sizeof(E100_DEVICE));
    Device->InterruptHandle = INVALID_HANDLE;
    Device->OsDevice = DeviceToken;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, Device);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device != NULL) {
            MmFreeNonPagedPool(Device);
            Device = NULL;
        }
    }

    return Status;
}

VOID
E100DispatchStateChange (
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
            Status = E100pProcessResourceRequirements(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(E100Driver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = E100pStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(E100Driver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
E100DispatchOpen (
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
E100DispatchClose (
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
E100DispatchIo (
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
E100DispatchSystemControl (
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

    PE100_DEVICE Device;
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

            IoCompleteIrp(E100Driver, Irp, Status);
            break;

        default:
            break;
        }
    }

    return;
}

KSTATUS
E100pAddNetworkDevice (
    PE100_DEVICE Device
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
    Properties.TransmitAlignment = 1;
    Properties.Device = Device->OsDevice;
    Properties.DeviceContext = Device;
    Properties.PacketSizeInformation.MaxPacketSize = RECEIVE_FRAME_DATA_SIZE;
    Properties.DataLinkType = NetDomainEthernet;
    Properties.MaxPhysicalAddress = MAX_ULONG;
    Properties.PhysicalAddress.Domain = NetDomainEthernet;
    Properties.Capabilities = Device->SupportedCapabilities;
    RtlCopyMemory(&(Properties.PhysicalAddress.Address),
                  &(Device->EepromMacAddress),
                  sizeof(Device->EepromMacAddress));

    Properties.Interface.Send = E100Send;
    Properties.Interface.GetSetInformation = E100GetSetInformation;
    Properties.Interface.DestroyLink = E100DestroyLink;
    Status = NetAddLink(&Properties, &(Device->NetworkLink));
    if (!KSUCCESS(Status)) {
        goto AddNetworkDeviceEnd;
    }

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
E100DestroyLink (
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

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
E100pProcessResourceRequirements (
    PIRP Irp,
    PE100_DEVICE Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an e100 LAN controller. It adds an interrupt vector requirement for
    any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to the device information.

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
    // Start listening for a PCI config interface.
    //

    if (Device->RegisteredForPciConfigInterfaces == FALSE) {
        Status = IoRegisterForInterfaceNotifications(
                              &E100PciConfigurationInterfaceUuid,
                              E100pProcessPciConfigInterfaceChangeNotification,
                              Irp->Device,
                              Device,
                              TRUE);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        Device->RegisteredForPciConfigInterfaces = TRUE;
    }

    //
    // A PCI interface should have been found.
    //

    if (Device->PciConfigInterfaceAvailable == FALSE) {
        Status = STATUS_NOT_CONFIGURED;
        goto ProcessResourceRequirementsEnd;
    }

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
E100pStartDevice (
    PIRP Irp,
    PE100_DEVICE Device
    )

/*++

Routine Description:

    This routine starts the E100 LAN device.

Arguments:

    Irp - Supplies a pointer to the start IRP.

    Device - Supplies a pointer to the device information.

Return Value:

    Status code.

--*/

{

    ULONG AlignmentOffset;
    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION ControllerBase;
    PHYSICAL_ADDRESS EndAddress;
    PRESOURCE_ALLOCATION LineAllocation;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONGLONG Revision;
    ULONG Size;
    KSTATUS Status;

    ControllerBase = NULL;

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

        //
        // Look for the first physical address reservation, the registers.
        //

        } else if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            if (ControllerBase == NULL) {
                ControllerBase = Allocation;
            }
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
    // Map the controller.
    //

    if (Device->ControllerBase == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = ControllerBase->Allocation;
        EndAddress = PhysicalAddress + ControllerBase->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = ControllerBase->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);
        Device->ControllerBase = MmMapPhysicalAddress(PhysicalAddress,
                                                      Size,
                                                      TRUE,
                                                      FALSE,
                                                      TRUE);

        if (Device->ControllerBase == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartDeviceEnd;
        }

        Device->ControllerBase += AlignmentOffset;
    }

    ASSERT(Device->ControllerBase != NULL);

    //
    // Read the revision from PCI config space.
    //

    Status = Device->PciConfigInterface.ReadPciConfig(
                                        Device->PciConfigInterface.DeviceToken,
                                        PCI_REVISION_ID_OFFSET,
                                        sizeof(ULONG),
                                        &Revision);

    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    Device->Revision = (ULONG)Revision & PCI_REVISION_ID_MASK;

    //
    // Allocate the controller structures.
    //

    Status = E100pInitializeDeviceStructures(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Attempt to connect the interrupt.
    //

    ASSERT(Device->InterruptHandle == INVALID_HANDLE);

    RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
    Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
    Connect.Device = Device->OsDevice;
    Connect.LineNumber = Device->InterruptLine;
    Connect.Vector = Device->InterruptVector;
    Connect.InterruptServiceRoutine = E100pInterruptService;
    Connect.LowLevelServiceRoutine = E100pInterruptServiceWorker;
    Connect.Context = Device;
    Connect.Interrupt = &(Device->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Start up the controller.
    //

    Status = E100pResetDevice(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    ASSERT(Device->NetworkLink != NULL);

StartDeviceEnd:
    return Status;
}

VOID
E100pProcessPciConfigInterfaceChangeNotification (
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

    PE100_DEVICE DeviceContext;

    DeviceContext = (PE100_DEVICE)Context;
    if (Arrival != FALSE) {
        if (InterfaceBufferSize >= sizeof(INTERFACE_PCI_CONFIG_ACCESS)) {

            ASSERT(DeviceContext->PciConfigInterfaceAvailable == FALSE);

            RtlCopyMemory(&(DeviceContext->PciConfigInterface),
                          InterfaceBuffer,
                          sizeof(INTERFACE_PCI_CONFIG_ACCESS));

            DeviceContext->PciConfigInterfaceAvailable = TRUE;
        }

    } else {
        DeviceContext->PciConfigInterfaceAvailable = FALSE;
    }

    return;
}

