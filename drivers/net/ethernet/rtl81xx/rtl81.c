/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtl81.c

Abstract:

    This module implements support for the driver portion of the Realtek
    RTL81xx family of Ethernet controllers.

Author:

    Chris Stevens 20-Jun-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include "rtl81.h"

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
Rtl81AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Rtl81DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rtl81DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rtl81DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rtl81DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rtl81DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rtl81DestroyLink (
    PVOID DeviceContext
    );

KSTATUS
Rtl81pProcessResourceRequirements (
    PIRP Irp,
    PRTL81_DEVICE Device
    );

KSTATUS
Rtl81pStartDevice (
    PIRP Irp,
    PRTL81_DEVICE Device
    );

VOID
Rtl81pProcessPciMsiInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Rtl81Driver = NULL;
UUID Rtl81PciMsiInterfaceUuid = UUID_PCI_MESSAGE_SIGNALED_INTERRUPTS;

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

    This routine is the entry point for the RTL81xx driver. It registers its
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

    Rtl81Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Rtl81AddDevice;
    FunctionTable.DispatchStateChange = Rtl81DispatchStateChange;
    FunctionTable.DispatchOpen = Rtl81DispatchOpen;
    FunctionTable.DispatchClose = Rtl81DispatchClose;
    FunctionTable.DispatchIo = Rtl81DispatchIo;
    FunctionTable.DispatchSystemControl = Rtl81DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Rtl81AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the RTL81xx
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

    PRTL81_DEVICE Device;
    KSTATUS Status;

    Device = MmAllocateNonPagedPool(sizeof(RTL81_DEVICE), RTL81_ALLOCATION_TAG);
    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Device, sizeof(RTL81_DEVICE));
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
Rtl81DispatchStateChange (
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
            Status = Rtl81pProcessResourceRequirements(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Rtl81Driver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Rtl81pStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Rtl81Driver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Rtl81DispatchOpen (
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
Rtl81DispatchClose (
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
Rtl81DispatchIo (
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
Rtl81DispatchSystemControl (
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

    PRTL81_DEVICE Device;
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

            IoCompleteIrp(Rtl81Driver, Irp, Status);
            break;

        default:
            break;
        }
    }

    return;
}

KSTATUS
Rtl81pAddNetworkDevice (
    PRTL81_DEVICE Device
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
    Properties.TransmitAlignment = RTL81_TRANSMIT_ALIGNMENT;
    Properties.Device = Device->OsDevice;
    Properties.DeviceContext = Device;
    PacketSizeInformation = &(Properties.PacketSizeInformation);
    PacketSizeInformation->MaxPacketSize = RTL81_MAX_TRANSMIT_PACKET_SIZE;
    if ((Device->Flags & RTL81_FLAG_TRANSMIT_MODE_LEGACY) != 0) {
        PacketSizeInformation->MinPacketSize = RTL81_MINIMUM_PACKET_LENGTH;
    }

    Properties.DataLinkType = NetDomainEthernet;
    Properties.MaxPhysicalAddress = MAX_ULONG;
    Properties.PhysicalAddress.Domain = NetDomainEthernet;
    Properties.Capabilities = Device->SupportedCapabilities;
    RtlCopyMemory(&(Properties.PhysicalAddress.Address),
                  &(Device->MacAddress),
                  sizeof(Device->MacAddress));

    Properties.Interface.Send = Rtl81Send;
    Properties.Interface.GetSetInformation = Rtl81GetSetInformation;
    Properties.Interface.DestroyLink = Rtl81DestroyLink;
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
Rtl81DestroyLink (
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
Rtl81pProcessResourceRequirements (
    PIRP Irp,
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an RTL81xx LAN controller. It adds an interrupt vector requirement
    for any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to the device information.

Return Value:

    Status code.

--*/

{

    PRESOURCE_CONFIGURATION_LIST ConfigurationList;
    ULONGLONG EdgeTriggered;
    ULONGLONG LineCharacteristics;
    PRESOURCE_REQUIREMENT NextRequirement;
    PRESOURCE_REQUIREMENT Requirement;
    PRESOURCE_REQUIREMENT_LIST RequirementList;
    KSTATUS Status;
    ULONGLONG VectorCharacteristics;
    PRESOURCE_REQUIREMENT VectorRequirement;
    RESOURCE_REQUIREMENT VectorTemplate;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Initialize a nice interrupt vector requirement in preparation.
    //

    RtlZeroMemory(&VectorTemplate, sizeof(RESOURCE_REQUIREMENT));
    VectorTemplate.Type = ResourceTypeInterruptVector;
    VectorTemplate.Minimum = 0;
    VectorTemplate.Maximum = -1;
    VectorTemplate.Length = 1;

    //
    // Some RTL81xx devices support MSI/MSI-X. If this device does, then prefer
    // MSIs over legacy interrupts.
    //

    if ((Device->PciMsiFlags & RTL81_PCI_MSI_FLAG_INTERFACE_REGISTERED) == 0) {
        Status = IoRegisterForInterfaceNotifications(
                                &Rtl81PciMsiInterfaceUuid,
                                Rtl81pProcessPciMsiInterfaceChangeNotification,
                                Irp->Device,
                                Device,
                                TRUE);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        Device->PciMsiFlags |= RTL81_PCI_MSI_FLAG_INTERFACE_REGISTERED;
    }

    //
    // If the MSI interface is ever going to be present, then it should have
    // been registered immediately. Prepare the device to prefer MSI interrupts.
    //

    ConfigurationList = Irp->U.QueryResources.ResourceRequirements;
    if ((Device->PciMsiFlags & RTL81_PCI_MSI_FLAG_INTERFACE_AVAILABLE) != 0) {

        //
        // The RTl81xx devices only ever need one interrupt vector. Create one
        // for every configuration.
        //

        RequirementList = IoGetNextResourceConfiguration(ConfigurationList,
                                                         NULL);

        while (RequirementList != NULL) {
            VectorTemplate.Characteristics = INTERRUPT_VECTOR_EDGE_TRIGGERED;
            VectorTemplate.OwningRequirement = NULL;
            Status = IoCreateAndAddResourceRequirement(&VectorTemplate,
                                                       RequirementList,
                                                       &VectorRequirement);

            if (!KSUCCESS(Status)) {
                goto ProcessResourceRequirementsEnd;
            }

            //
            // Now, just in case the above vector allocation fails, prepare to
            // fall back to legacy interrupts by allocating an alternative
            // vector for each interrupt in the requirement list.
            //

            Requirement = IoGetNextResourceRequirement(RequirementList, NULL);
            while (Requirement != NULL) {
                NextRequirement = IoGetNextResourceRequirement(RequirementList,
                                                               Requirement);

                if (Requirement->Type != ResourceTypeInterruptLine) {
                    Requirement = NextRequirement;
                    continue;
                }

                VectorCharacteristics = 0;
                LineCharacteristics = Requirement->Characteristics;
                if ((LineCharacteristics & INTERRUPT_LINE_ACTIVE_LOW) != 0) {
                    VectorCharacteristics |= INTERRUPT_VECTOR_ACTIVE_LOW;
                }

                if ((LineCharacteristics & INTERRUPT_LINE_ACTIVE_HIGH) != 0) {
                    VectorCharacteristics |= INTERRUPT_VECTOR_ACTIVE_HIGH;
                }

                EdgeTriggered = LineCharacteristics &
                                INTERRUPT_LINE_EDGE_TRIGGERED;

                if (EdgeTriggered != 0) {
                    VectorCharacteristics |= INTERRUPT_VECTOR_EDGE_TRIGGERED;
                }

                VectorTemplate.Characteristics = VectorCharacteristics;
                VectorTemplate.OwningRequirement = Requirement;
                Status = IoCreateAndAddResourceRequirementAlternative(
                                                            &VectorTemplate,
                                                            VectorRequirement);

                if (!KSUCCESS(Status)) {
                    goto ProcessResourceRequirementsEnd;
                }

                Requirement = NextRequirement;
            }

            RequirementList = IoGetNextResourceConfiguration(ConfigurationList,
                                                             RequirementList);
        }

        Device->PciMsiFlags |= RTL81_PCI_MSI_FLAG_RESOURCES_REQUESTED;

    //
    // Otherwise stick with the good, old legacy interrupt setup.
    //

    } else {

        //
        // Loop through all configuration lists and add vectors for each line.
        //

        Status = IoCreateAndAddInterruptVectorsForLines(ConfigurationList,
                                                        &VectorTemplate);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }
    }

ProcessResourceRequirementsEnd:
    return Status;
}

KSTATUS
Rtl81pStartDevice (
    PIRP Irp,
    PRTL81_DEVICE Device
    )

/*++

Routine Description:

    This routine starts the RTL81xx LAN device.

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
    BOOL Initialized;
    PRESOURCE_ALLOCATION LineAllocation;
    PCI_MSI_INFORMATION MsiInformation;
    PINTERFACE_PCI_MSI MsiInterface;
    PCI_MSI_TYPE MsiType;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PROCESSOR_SET ProcessorSet;
    ULONG Size;
    KSTATUS Status;

    ControllerBase = NULL;
    Initialized = FALSE;

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // If the resource is an interrupt vector the presense of an owning
        // interrupt line allocation will dictate whether or not MSI/MSI-X
        // is used versus legacy interrupts.
        //

        if (Allocation->Type == ResourceTypeInterruptVector) {
            LineAllocation = Allocation->OwningAllocation;
            if (LineAllocation == NULL) {

                ASSERT((Device->PciMsiFlags &
                        RTL81_PCI_MSI_FLAG_RESOURCES_REQUESTED) != 0);

                ASSERT(Allocation->Characteristics ==
                       INTERRUPT_VECTOR_EDGE_TRIGGERED);

                Device->InterruptLine = INVALID_INTERRUPT_LINE;
                Device->PciMsiFlags |= RTL81_PCI_MSI_FLAG_RESOURCES_ALLOCATED;

            } else {

                ASSERT(LineAllocation->Type == ResourceTypeInterruptLine);

                Device->InterruptLine = LineAllocation->Allocation;
            }

            Device->InterruptVector = Allocation->Allocation;
            Device->InterruptResourcesFound = TRUE;

        //
        // Look for the first physical address reservation, the registers.
        //

        } else if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            if ((ControllerBase == NULL) && (Allocation->Length != 0)) {
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
    // Initialize the controller structures.
    //

    Status = Rtl81pInitializeDeviceStructures(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Start up the controller.
    //

    Status = Rtl81pInitialize(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    Initialized = TRUE;

    //
    // Attempt to connect the interrupt.
    //

    ASSERT(Device->InterruptHandle == INVALID_HANDLE);

    RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
    Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
    Connect.Device = Irp->Device;
    Connect.LineNumber = Device->InterruptLine;
    Connect.Vector = Device->InterruptVector;
    Connect.InterruptServiceRoutine = Rtl81pInterruptService;
    Connect.LowLevelServiceRoutine = Rtl81pInterruptServiceWorker;
    Connect.Context = Device;
    Connect.Interrupt = &(Device->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // If MSI/MSI-X resources were allocated, then those additionally need to
    // be enabled through the PCI interface. Prefer MSI and fall back to MSI-X.
    //

    if (Device->InterruptLine == INVALID_INTERRUPT_LINE) {

        ASSERT((Device->PciMsiFlags &
                RTL81_PCI_MSI_FLAG_RESOURCES_ALLOCATED) != 0);

        ProcessorSet.Target = ProcessorTargetAny;
        MsiType = PciMsiTypeBasic;
        MsiInterface = &(Device->PciMsiInterface);
        Status = MsiInterface->SetVectors(MsiInterface->DeviceToken,
                                          MsiType,
                                          Device->InterruptVector,
                                          0,
                                          1,
                                          &ProcessorSet);

        if (!KSUCCESS(Status)) {
            MsiType = PciMsiTypeExtended;
            Status = MsiInterface->SetVectors(MsiInterface->DeviceToken,
                                              MsiType,
                                              Device->InterruptVector,
                                              0,
                                              1,
                                              &ProcessorSet);

            if (!KSUCCESS(Status)) {
                goto StartDeviceEnd;
            }
        }

        RtlZeroMemory(&MsiInformation, sizeof(PCI_MSI_INFORMATION));
        MsiInformation.Version = PCI_MSI_INTERFACE_INFORMATION_VERSION;
        MsiInformation.MsiType = MsiType;
        MsiInformation.Flags = PCI_MSI_INTERFACE_FLAG_ENABLED;
        MsiInformation.VectorCount = 1;
        Status = MsiInterface->GetSetInformation(MsiInterface->DeviceToken,
                                                 &MsiInformation,
                                                 TRUE);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Initialized != FALSE) {

            ASSERT(Device->NetworkLink != NULL);

            NetRemoveLink(Device->NetworkLink);
            Device->NetworkLink = NULL;
        }

        Rtl81pDestroyDeviceStructures(Device);
    }

    return Status;
}

VOID
Rtl81pProcessPciMsiInterfaceChangeNotification (
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

    PRTL81_DEVICE Rtl81Device;

    Rtl81Device = (PRTL81_DEVICE)Context;
    if (Arrival != FALSE) {
        if (InterfaceBufferSize >= sizeof(INTERFACE_PCI_MSI)) {

            ASSERT((Rtl81Device->PciMsiFlags &
                    RTL81_PCI_MSI_FLAG_INTERFACE_AVAILABLE) == 0);

            RtlCopyMemory(&(Rtl81Device->PciMsiInterface),
                          InterfaceBuffer,
                          sizeof(INTERFACE_PCI_MSI));

            Rtl81Device->PciMsiFlags |= RTL81_PCI_MSI_FLAG_INTERFACE_AVAILABLE;
        }

    } else {
        Rtl81Device->PciMsiFlags &= ~RTL81_PCI_MSI_FLAG_INTERFACE_AVAILABLE;
    }

    return;
}

