/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am3eth.c

Abstract:

    This module implements the CPSW Ethernet Controller on TI AM335x SoCs.

Author:

    Evan Green 20-Mar-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include "am3eth.h"

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
A3eAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
A3eDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
A3eDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
A3eDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
A3eDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
A3eDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
A3eDestroyLink (
    PVOID DeviceContext
    );

KSTATUS
A3epProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
A3epStartDevice (
    PIRP Irp,
    PA3E_DEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER A3eDriver = NULL;

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

    This routine is the entry point for the AM3 ethernet driver. It registers
    its other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    A3eDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = A3eAddDevice;
    FunctionTable.DispatchStateChange = A3eDispatchStateChange;
    FunctionTable.DispatchOpen = A3eDispatchOpen;
    FunctionTable.DispatchClose = A3eDispatchClose;
    FunctionTable.DispatchIo = A3eDispatchIo;
    FunctionTable.DispatchSystemControl = A3eDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
A3eAddDevice (
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

    PA3E_DEVICE Device;
    KSTATUS Status;

    Device = MmAllocateNonPagedPool(sizeof(A3E_DEVICE), A3E_ALLOCATION_TAG);
    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Device, sizeof(A3E_DEVICE));
    Device->TxInterruptHandle = INVALID_HANDLE;
    Device->RxInterruptHandle = INVALID_HANDLE;
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
A3eDispatchStateChange (
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
            Status = A3epProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(A3eDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = A3epStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(A3eDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
A3eDispatchOpen (
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
A3eDispatchClose (
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
A3eDispatchIo (
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
A3eDispatchSystemControl (
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

    PA3E_DEVICE Device;
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

            IoCompleteIrp(A3eDriver, Irp, Status);
            break;

        default:
            break;
        }
    }

    return;
}

KSTATUS
A3epAddNetworkDevice (
    PA3E_DEVICE Device
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

    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation;
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
    Properties.TransmitAlignment = Device->DataAlignment;
    Properties.Device = Device->OsDevice;
    Properties.DeviceContext = Device;
    PacketSizeInformation = &(Properties.PacketSizeInformation);
    PacketSizeInformation->MaxPacketSize = Device->ReceiveFrameDataSize;
    PacketSizeInformation->MinPacketSize = A3E_TRANSMIT_MINIMUM_PACKET_SIZE;
    Properties.DataLinkType = NetDomainEthernet;
    Properties.MaxPhysicalAddress = MAX_ULONG;
    Properties.PhysicalAddress.Domain = NetDomainEthernet;
    Properties.Capabilities = Device->SupportedCapabilities;

    ASSERT(Device->MacAddressAssigned != FALSE);

    RtlCopyMemory(&(Properties.PhysicalAddress.Address),
                  &(Device->MacAddress),
                  sizeof(Device->MacAddress));

    Properties.Interface.Send = A3eSend;
    Properties.Interface.GetSetInformation = A3eGetSetInformation;
    Properties.Interface.DestroyLink = A3eDestroyLink;
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
A3eDestroyLink (
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
A3epProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an e100 LAN controller. It adds an interrupt vector requirement for
    any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

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
A3epStartDevice (
    PIRP Irp,
    PA3E_DEVICE Device
    )

/*++

Routine Description:

    This routine starts the TI AM335x CPSW Ethernet device.

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
    HANDLE InterruptHandles[2];
    PRESOURCE_ALLOCATION LineAllocation;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
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

            ASSERT(Allocation->OwningAllocation != NULL);

            //
            // Save the line and vector number.
            //

            LineAllocation = Allocation->OwningAllocation;
            if (Device->InterruptResourcesFound == 0) {
                Device->TxInterruptLine = LineAllocation->Allocation;
                Device->TxInterruptVector = Allocation->Allocation;
                Device->InterruptResourcesFound += 1;

            } else if (Device->InterruptResourcesFound == 1) {
                Device->RxInterruptLine = LineAllocation->Allocation;
                Device->RxInterruptVector = Allocation->Allocation;
                Device->InterruptResourcesFound += 1;
            }

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

        ASSERT(ControllerBase->Length >= A3E_REGISTERS_SIZE);

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
        Device->ControllerBasePhysical = ControllerBase->Allocation;
    }

    ASSERT(Device->ControllerBase != NULL);

    //
    // Allocate the controller structures.
    //

    Status = A3epInitializeDeviceStructures(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    if (Device->InterruptResourcesFound != 2) {
        RtlDebugPrint("A3E: Missing interrupt resources\n");
        Status = STATUS_NOT_READY;
        goto StartDeviceEnd;
    }

    //
    // Attempt to connect the interrupts.
    //

    Device->InterruptRunLevel = RunLevelMaxDevice;

    ASSERT(Device->TxInterruptHandle == INVALID_HANDLE);

    RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
    Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
    Connect.Device = Irp->Device;
    Connect.LineNumber = Device->TxInterruptLine;
    Connect.Vector = Device->TxInterruptVector;
    Connect.InterruptServiceRoutine = A3epTxInterruptService;
    Connect.LowLevelServiceRoutine = A3epInterruptServiceWorker;
    Connect.Context = Device;
    Connect.Interrupt = &(Device->TxInterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    ASSERT(Device->RxInterruptHandle == INVALID_HANDLE);

    Connect.LineNumber = Device->RxInterruptLine;
    Connect.Vector = Device->RxInterruptVector;
    Connect.InterruptServiceRoutine = A3epRxInterruptService;
    Connect.Interrupt = &(Device->RxInterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    InterruptHandles[0] = Device->TxInterruptHandle;
    InterruptHandles[1] = Device->RxInterruptHandle;
    Device->InterruptRunLevel = IoGetInterruptRunLevel(InterruptHandles, 2);

    //
    // Start up the controller.
    //

    Status = A3epResetDevice(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    ASSERT(Device->NetworkLink != NULL);

StartDeviceEnd:
    return Status;
}

