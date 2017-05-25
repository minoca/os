/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hda.c

Abstract:

    This module implements the Intel High Definition Audio driver.

Author:

    Chris Stevens 3-Apr-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/pci.h>
#include "hda.h"

//
// --------------------------------------------------------------------- Macros
//

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
HdaAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
HdaDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
HdaDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
HdaDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
HdaDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
HdaDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
HdaDispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
HdapProcessResourceRequirements (
    PIRP Irp,
    PHDA_CONTROLLER Controller
    );

KSTATUS
HdapStartController (
    PIRP Irp,
    PHDA_CONTROLLER Controller
    );

VOID
HdapProcessPciMsiInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER HdaDriver = NULL;
UUID HdaPciMsiInterfaceUuid = UUID_PCI_MESSAGE_SIGNALED_INTERRUPTS;

//
// Define the set of enabled HD Audio debug flags.
//

ULONG HdaDebugFlags = 0x0;

//
// Store the sound core interface function table.
//

SOUND_FUNCTION_TABLE HdaSoundFunctionTable = {
    HdaSoundAllocateDmaBuffer,
    HdaSoundFreeDmaBuffer,
    HdaSoundGetSetInformation,
    NULL
};

//
// Store the list of legacy Intel devices that use the old stream
// synchronization register. All are assumed to have an Intel vendor ID of
// 0x8086.
//

ULONG HdaLegacyIntelDevices[] = {
    0x2668, 0x27D8, 0x269A, 0x284B, 0x293E, 0x293F, 0x3A3E, 0x3A6E
};

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

    This routine is the entry point for the Intel HDA driver. It registers
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

    HdaDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = HdaAddDevice;
    FunctionTable.DispatchStateChange = HdaDispatchStateChange;
    FunctionTable.DispatchOpen = HdaDispatchOpen;
    FunctionTable.DispatchClose = HdaDispatchClose;
    FunctionTable.DispatchIo = HdaDispatchIo;
    FunctionTable.DispatchSystemControl = HdaDispatchSystemControl;
    FunctionTable.DispatchUserControl = HdaDispatchUserControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
HdaAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which this driver
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

    PHDA_CONTROLLER Controller;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    ULONG DeviceNumber;
    ULONG ItemsScanned;
    HDA_REGISTER Register;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(HDA_CONTROLLER),
                                        HDA_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(HDA_CONTROLLER));

    //
    // Check to see if this is one of the older Intel devices that uses the
    // legacy stream synchronization register.
    //

    Register = HdaRegisterStreamSynchronization;
    Status = RtlStringScan(DeviceId,
                           RtlStringLength(DeviceId) + 1,
                           "VEN_8086&DEV_%x",
                           sizeof("VEN_8086&DEV_%x"),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           &DeviceNumber);

    if (KSUCCESS(Status) && (ItemsScanned == 1)) {
        DeviceCount = sizeof(HdaLegacyIntelDevices) /
                      sizeof(HdaLegacyIntelDevices[0]);

        for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
            if (HdaLegacyIntelDevices[DeviceIndex] == DeviceNumber) {
                Register = HdaRegisterLegacyStreamSynchronization;
                break;
            }
        }
    }

    Controller->StreamSynchronizationRegister = Register;
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, Controller);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            MmFreeNonPagedPool(Controller);
        }
    }

    return Status;
}

VOID
HdaDispatchStateChange (
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
            Status = HdapProcessResourceRequirements(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(HdaDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = HdapStartController(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(HdaDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
HdaDispatchOpen (
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

    PHDA_CONTROLLER Controller;
    PSOUND_DEVICE_HANDLE SoundHandle;
    KSTATUS Status;

    Controller = (PHDA_CONTROLLER)DeviceContext;
    Status = SoundOpenDevice(Controller->SoundController,
                             Irp->U.Open.FileProperties,
                             Irp->U.Open.DesiredAccess,
                             Irp->U.Open.OpenFlags,
                             Irp->U.Open.IoState,
                             &SoundHandle);

    if (!KSUCCESS(Status)) {
        goto DispatchOpenEnd;
    }

    Irp->U.Open.DeviceContext = SoundHandle;

DispatchOpenEnd:
    IoCompleteIrp(HdaDriver, Irp, Status);
    return;
}

VOID
HdaDispatchClose (
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

    PSOUND_DEVICE_HANDLE SoundHandle;

    SoundHandle = (PSOUND_DEVICE_HANDLE)Irp->U.Close.DeviceContext;
    SoundCloseDevice(SoundHandle);
    IoCompleteIrp(HdaDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
HdaDispatchIo (
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

    IO_OFFSET IoOffset;
    PSOUND_DEVICE_HANDLE SoundHandle;
    KSTATUS Status;
    BOOL Write;

    SoundHandle = (PSOUND_DEVICE_HANDLE)Irp->U.ReadWrite.DeviceContext;
    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    IoOffset = Irp->U.ReadWrite.IoOffset;
    Status = SoundPerformIo(SoundHandle,
                            Irp->U.ReadWrite.IoBuffer,
                            &IoOffset,
                            Irp->U.ReadWrite.IoSizeInBytes,
                            Irp->U.ReadWrite.IoFlags,
                            Irp->U.ReadWrite.TimeoutInMilliseconds,
                            Write,
                            &(Irp->U.ReadWrite.IoBytesCompleted));

    Irp->U.ReadWrite.NewIoOffset = IoOffset;
    if (!KSUCCESS(Status)) {
        goto DispatchIoEnd;
    }

DispatchIoEnd:
    IoCompleteIrp(HdaDriver, Irp, Status);
    return;
}

VOID
HdaDispatchSystemControl (
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

    PVOID Context;
    PHDA_CONTROLLER Controller;
    PSYSTEM_CONTROL_DEVICE_INFORMATION DeviceInformationRequest;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    KSTATUS Status;

    Controller = (PHDA_CONTROLLER)DeviceContext;
    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Status = SoundLookupDevice(Controller->SoundController, Lookup);
        IoCompleteIrp(HdaDriver, Irp, Status);
        break;

    //
    // Succeed for the basics.
    //

    case IrpMinorSystemControlWriteFileProperties:
    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(HdaDriver, Irp, STATUS_SUCCESS);
        break;

    case IrpMinorSystemControlDeviceInformation:
        DeviceInformationRequest = Irp->U.SystemControl.SystemContext;
        Status = SoundGetSetDeviceInformation(
                                         Controller->SoundController,
                                         &(DeviceInformationRequest->Uuid),
                                         DeviceInformationRequest->Data,
                                         &(DeviceInformationRequest->DataSize),
                                         DeviceInformationRequest->Set);

        IoCompleteIrp(HdaDriver, Irp, Status);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

    }

    return;
}

VOID
HdaDispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles User Control IRPs.

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

    PSOUND_DEVICE_HANDLE SoundHandle;
    KSTATUS Status;

    SoundHandle = (PSOUND_DEVICE_HANDLE)Irp->U.UserControl.DeviceContext;
    Status = SoundUserControl(SoundHandle,
                              Irp->U.UserControl.FromKernelMode,
                              (ULONG)Irp->MinorCode,
                              Irp->U.UserControl.UserBuffer,
                              Irp->U.UserControl.UserBufferSize);

    if (!KSUCCESS(Status)) {
        goto DispatchUserControlEnd;
    }

DispatchUserControlEnd:
    IoCompleteIrp(HdaDriver, Irp, Status);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HdapProcessResourceRequirements (
    PIRP Irp,
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an HD Audio controller. It adds an interrupt vector requirement
    for any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Controller - Supplies a pointer to the controller information.

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

    if ((Controller->PciMsiFlags &
         HDA_PCI_MSI_FLAG_INTERFACE_REGISTERED) == 0) {

        Status = IoRegisterForInterfaceNotifications(
                                &HdaPciMsiInterfaceUuid,
                                HdapProcessPciMsiInterfaceChangeNotification,
                                Irp->Device,
                                Controller,
                                TRUE);

        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        Controller->PciMsiFlags |= HDA_PCI_MSI_FLAG_INTERFACE_REGISTERED;
    }

    //
    // If the MSI interface is ever going to be present, then it should have
    // been registered immediately. Prepare the device to prefer MSI interrupts.
    //

    ConfigurationList = Irp->U.QueryResources.ResourceRequirements;
    if ((Controller->PciMsiFlags & HDA_PCI_MSI_FLAG_INTERFACE_AVAILABLE) != 0) {

        //
        // The HD Audio devices only ever need one interrupt vector. Create one
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

        Controller->PciMsiFlags |= HDA_PCI_MSI_FLAG_RESOURCES_REQUESTED;

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
HdapStartController (
    PIRP Irp,
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine starts the Intel HD Audio controller.

Arguments:

    Irp - Supplies a pointer to the start IRP.

    Controller - Supplies a pointer to the controller information.

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
    PCI_MSI_INFORMATION MsiInformation;
    PINTERFACE_PCI_MSI MsiInterface;
    PCI_MSI_TYPE MsiType;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PROCESSOR_SET ProcessorSet;
    SOUND_CONTROLLER_INFORMATION Registration;
    UINTN Size;
    KSTATUS Status;

    ControllerBase = NULL;
    Size = 0;

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    ASSERT(Controller->InterruptHandle == INVALID_HANDLE);

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

                ASSERT((Controller->PciMsiFlags &
                        HDA_PCI_MSI_FLAG_RESOURCES_REQUESTED) != 0);

                ASSERT(Allocation->Characteristics ==
                       INTERRUPT_VECTOR_EDGE_TRIGGERED);

                Controller->InterruptLine = INVALID_INTERRUPT_LINE;
                Controller->PciMsiFlags |= HDA_PCI_MSI_FLAG_RESOURCES_ALLOCATED;

            } else {

                ASSERT(LineAllocation->Type == ResourceTypeInterruptLine);

                Controller->InterruptLine = LineAllocation->Allocation;
            }

            Controller->InterruptVector = Allocation->Allocation;

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
        goto StartControllerEnd;
    }

    //
    // Map the controller.
    //

    if (Controller->ControllerBase == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = ControllerBase->Allocation;
        EndAddress = PhysicalAddress + ControllerBase->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = ControllerBase->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (UINTN)(EndAddress - PhysicalAddress);
        Controller->ControllerBase = MmMapPhysicalAddress(PhysicalAddress,
                                                          Size,
                                                          TRUE,
                                                          FALSE,
                                                          TRUE);

        if (Controller->ControllerBase == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartControllerEnd;
        }

        Controller->ControllerBase += AlignmentOffset;
    }

    ASSERT(Controller->ControllerBase != NULL);

    //
    // Allocate the controller structures.
    //

    Status = HdapInitializeDeviceStructures(Controller);
    if (!KSUCCESS(Status)) {
        goto StartControllerEnd;
    }

    //
    // Connect the interrupt. The command/response buffers are interrupt driven
    // during initialization. This must be done first.
    //

    if (Controller->InterruptHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Controller->InterruptLine;
        Connect.Vector = Controller->InterruptVector;
        Connect.InterruptServiceRoutine = HdaInterruptService;
        Connect.DispatchServiceRoutine = HdaInterruptServiceDpc;
        Connect.LowLevelServiceRoutine = HdaInterruptServiceWorker;
        Connect.Context = Controller;
        Connect.Interrupt = &(Controller->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    //
    // If MSI/MSI-X resources were allocated, then those additionally need to
    // be enabled through the PCI interface. Prefer MSI and fall back to MSI-X.
    //

    if (Controller->InterruptLine == INVALID_INTERRUPT_LINE) {

        ASSERT((Controller->PciMsiFlags &
                HDA_PCI_MSI_FLAG_RESOURCES_ALLOCATED) != 0);

        ProcessorSet.Target = ProcessorTargetAny;
        MsiType = PciMsiTypeBasic;
        MsiInterface = &(Controller->PciMsiInterface);
        Status = MsiInterface->SetVectors(MsiInterface->DeviceToken,
                                          MsiType,
                                          Controller->InterruptVector,
                                          0,
                                          1,
                                          &ProcessorSet);

        if (!KSUCCESS(Status)) {
            MsiType = PciMsiTypeExtended;
            Status = MsiInterface->SetVectors(MsiInterface->DeviceToken,
                                              MsiType,
                                              Controller->InterruptVector,
                                              0,
                                              1,
                                              &ProcessorSet);

            if (!KSUCCESS(Status)) {
                goto StartControllerEnd;
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
            goto StartControllerEnd;
        }
    }

    //
    // Initialize the controller, which includes enumerating the codecs.
    //

    Status = HdapInitializeController(Controller);
    if (!KSUCCESS(Status)) {
        goto StartControllerEnd;
    }

    //
    // Register with the sound core library.
    //

    if (Controller->SoundController == NULL) {
        RtlZeroMemory(&Registration, sizeof(SOUND_CONTROLLER_INFORMATION));
        Registration.Version = SOUND_CONTROLLER_INFORMATION_VERSION;
        Registration.Context = Controller;
        Registration.OsDevice = Controller->OsDevice;
        Registration.Flags = SOUND_CONTROLLER_FLAG_NON_CACHED_DMA_BUFFER |
                             SOUND_CONTROLLER_FLAG_NON_PAGED_SOUND_BUFFER;

        Registration.FunctionTable = &HdaSoundFunctionTable;
        Registration.MinFragmentCount =
                                HDA_BUFFER_DESCRIPTOR_LIST_ENTRY_COUNT_MINIMUM;

        Registration.MaxFragmentCount =
                                HDA_BUFFER_DESCRIPTOR_LIST_ENTRY_COUNT_DEFAULT;

        Registration.MinFragmentSize = HDA_DMA_BUFFER_ALIGNMENT;
        Registration.MaxFragmentSize = HDA_SOUND_BUFFER_MAX_FRAGMENT_SIZE;
        Registration.MaxBufferSize = HDA_SOUND_BUFFER_MAX_SIZE;
        Registration.DeviceCount = Controller->DeviceCount;
        Registration.Devices = Controller->Devices;
        Status = SoundCreateController(&Registration,
                                       &(Controller->SoundController));

        if (!KSUCCESS(Status)) {
            goto StartControllerEnd;
        }
    }

StartControllerEnd:
    if (!KSUCCESS(Status)) {
        if (Controller->ControllerBase != NULL) {
            MmUnmapAddress(Controller->ControllerBase, Size);
            Controller->ControllerBase = NULL;
        }

        if (Controller->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Controller->InterruptHandle);
            Controller->InterruptHandle = INVALID_HANDLE;
        }

        if (Controller->SoundController != NULL) {
            SoundDestroyController(Controller->SoundController);
            Controller->SoundController = NULL;
        }

        HdapDestroyDeviceStructures(Controller);
    }

    return Status;
}

VOID
HdapProcessPciMsiInterfaceChangeNotification (
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

    PHDA_CONTROLLER Controller;

    Controller = (PHDA_CONTROLLER)Context;
    if (Arrival != FALSE) {
        if (InterfaceBufferSize >= sizeof(INTERFACE_PCI_MSI)) {

            ASSERT((Controller->PciMsiFlags &
                    HDA_PCI_MSI_FLAG_INTERFACE_AVAILABLE) == 0);

            RtlCopyMemory(&(Controller->PciMsiInterface),
                          InterfaceBuffer,
                          sizeof(INTERFACE_PCI_MSI));

            Controller->PciMsiFlags |= HDA_PCI_MSI_FLAG_INTERFACE_AVAILABLE;
        }

    } else {
        Controller->PciMsiFlags &= ~HDA_PCI_MSI_FLAG_INTERFACE_AVAILABLE;
    }

    return;
}

