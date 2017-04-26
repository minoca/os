/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ahci.c

Abstract:

    This module implements driver support for the Advanced Host Controller
    Interface (AHCI).

Author:

    Evan Green 15-Nov-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/storage/ata.h>
#include "ahci.h"

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
AhciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
AhciDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AhciDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AhciDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AhciDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AhciDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AhcipDispatchControllerStateChange (
    PIRP Irp,
    PAHCI_CONTROLLER Controller
    );

VOID
AhcipDispatchPortStateChange (
    PIRP Irp,
    PAHCI_PORT Port
    );

VOID
AhcipDispatchPortSystemControl (
    PIRP Irp,
    PAHCI_PORT Device
    );

KSTATUS
AhcipProcessResourceRequirements (
    PIRP Irp,
    PAHCI_CONTROLLER Controller
    );

KSTATUS
AhcipStartController (
    PIRP Irp,
    PAHCI_CONTROLLER Controller
    );

VOID
AhcipEnumeratePorts (
    PIRP Irp,
    PAHCI_CONTROLLER Controller
    );

VOID
AhcipStartPort (
    PIRP Irp,
    PAHCI_PORT Port
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER AhciDriver = NULL;

DRIVER_FUNCTION_TABLE AhciDriverFunctionTable = {
    DRIVER_FUNCTION_TABLE_VERSION,
    NULL,
    AhciAddDevice,
    NULL,
    NULL,
    AhciDispatchStateChange,
    AhciDispatchOpen,
    AhciDispatchClose,
    AhciDispatchIo,
    AhciDispatchSystemControl,
    NULL
};

//
// Store how long it took to do the enumeration of all drives, in milliseconds.
//

ULONG AhciEnumerationMilliseconds;

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

    This routine is the entry point for the AHCI driver. It registers its other
    dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    KSTATUS Status;

    AhciDriver = Driver;
    Status = IoRegisterDriverFunctions(Driver, &AhciDriverFunctionTable);
    return Status;
}

KSTATUS
AhciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the AHCI device
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

    PAHCI_CONTROLLER Controller;
    UINTN Index;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(AHCI_CONTROLLER),
                                        AHCI_ALLOCATION_TAG);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Controller, sizeof(AHCI_CONTROLLER));
    Controller->Type = AhciContextController;
    Controller->InterruptHandle = INVALID_HANDLE;
    Controller->InterruptVector = INVALID_INTERRUPT_VECTOR;
    Controller->InterruptLine = INVALID_INTERRUPT_LINE;
    for (Index = 0; Index < AHCI_PORT_COUNT; Index += 1) {
        Controller->Ports[Index].Controller = Controller;
        KeInitializeSpinLock(&(Controller->Ports[Index].DpcLock));
        INITIALIZE_LIST_HEAD(&(Controller->Ports[Index].IrpQueue));
        Controller->Ports[Index].Type = AhciContextPort;
    }

    Controller->OsDevice = DeviceToken;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, Controller);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    Status = STATUS_SUCCESS;

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            MmFreeNonPagedPool(Controller);
        }
    }

    return Status;
}

VOID
AhciDispatchStateChange (
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

    PAHCI_CONTROLLER Controller;

    Controller = DeviceContext;
    switch (Controller->Type) {
    case AhciContextController:
        AhcipDispatchControllerStateChange(Irp, Controller);
        break;

    case AhciContextPort:
        AhcipDispatchPortStateChange(Irp, (PAHCI_PORT)Controller);
        break;

    default:

        ASSERT(FALSE);

        IoCompleteIrp(AhciDriver, Irp, STATUS_INVALID_CONFIGURATION);
        break;
    }

    return;
}

VOID
AhciDispatchOpen (
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

    PAHCI_PORT Disk;

    //
    // Only the disk can be opened or closed.
    //

    Disk = (PAHCI_PORT)DeviceContext;
    if (Disk->Type != AhciContextPort) {
        return;
    }

    Irp->U.Open.DeviceContext = Disk;
    IoCompleteIrp(AhciDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
AhciDispatchClose (
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

    PAHCI_PORT Disk;

    //
    // Only the disk can be opened or closed.
    //

    Disk = (PAHCI_PORT)DeviceContext;
    if (Disk->Type != AhciContextPort) {
        return;
    }

    Irp->U.Open.DeviceContext = Disk;
    IoCompleteIrp(AhciDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
AhciDispatchIo (
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

    BOOL CompleteIrp;
    PAHCI_PORT Device;
    ULONG IrpReadWriteFlags;
    BOOL PmReferenceAdded;
    KSTATUS Status;
    BOOL Write;

    Device = (PAHCI_PORT)Irp->U.ReadWrite.DeviceContext;
    if (Device->Type != AhciContextPort) {
        return;
    }

    CompleteIrp = TRUE;
    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    //
    // If this IRP is on the way down, always add a power management reference.
    //

    PmReferenceAdded = FALSE;
    if (Irp->Direction == IrpDown) {
        Status = PmDeviceAddReference(Device->OsDevice);
        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        PmReferenceAdded = TRUE;
    }

    //
    // Set the IRP read/write flags for the preparation and completion steps.
    //

    IrpReadWriteFlags = IRP_READ_WRITE_FLAG_DMA;
    if (Write != FALSE) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    //
    // If the IRP is on the way up, then clean up after the DMA as this IRP is
    // still sitting in the channel. An IRP going up is already complete.
    //

    if (Irp->Direction == IrpUp) {
        CompleteIrp = FALSE;
        PmDeviceReleaseReference(Device->OsDevice);
        Status = IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
        if (!KSUCCESS(Status)) {
            IoUpdateIrpStatus(Irp, Status);
        }

    //
    // Start the DMA on the way down.
    //

    } else {
        Irp->U.ReadWrite.NewIoOffset = Irp->U.ReadWrite.IoOffset;

        //
        // Before acquiring the channel's lock and starting the DMA, prepare
        // the I/O context for AHCI (i.e. it must use physical addresses that
        // are less than 4GB and be sector size aligned).
        //

        Status = IoPrepareReadWriteIrp(&(Irp->U.ReadWrite),
                                       ATA_SECTOR_SIZE,
                                       0,
                                       Device->Controller->MaxPhysical,
                                       IrpReadWriteFlags);

        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        CompleteIrp = FALSE;
        Status = AhcipEnqueueIrp(Device, Irp);
        if (!KSUCCESS(Status)) {
            IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
            CompleteIrp = TRUE;
        }
    }

DispatchIoEnd:
    if (CompleteIrp != FALSE) {
        if (PmReferenceAdded != FALSE) {
            PmDeviceReleaseReference(Device->OsDevice);
        }

        IoCompleteIrp(AhciDriver, Irp, Status);
    }

    return;
}

VOID
AhciDispatchSystemControl (
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

    PAHCI_PORT Child;

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    Child = (PAHCI_PORT)DeviceContext;
    if (Child->Type == AhciContextPort) {
        AhcipDispatchPortSystemControl(Irp, Child);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
AhcipDispatchControllerStateChange (
    PIRP Irp,
    PAHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine handles state change IRPs for an AHCI controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Controller - Supplies a pointer to the controller context.

Return Value:

    None. The routine completes the IRP if appropriate.

--*/

{

    KSTATUS Status;

    if (Irp->Direction == IrpUp) {
        if (!KSUCCESS(IoGetIrpStatus(Irp))) {
            return;
        }

        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            Status = AhcipProcessResourceRequirements(Irp, Controller);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(AhciDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = AhcipStartController(Irp, Controller);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(AhciDriver, Irp, Status);
            }

            break;

        case IrpMinorQueryChildren:
            AhcipEnumeratePorts(Irp, Controller);
            break;

        case IrpMinorIdle:
        case IrpMinorSuspend:
        case IrpMinorResume:
        default:
            break;
        }
    }

    return;
}

VOID
AhcipDispatchPortStateChange (
    PIRP Irp,
    PAHCI_PORT Port
    )

/*++

Routine Description:

    This routine handles state change IRPs for an AHCI port device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Port - Supplies a pointer to the AHCI port.

Return Value:

    None. The routine completes the IRP if appropriate.

--*/

{

    KSTATUS Status;

    if (Irp->Direction == IrpDown) {
        switch (Irp->MinorCode) {
        case IrpMinorStartDevice:

            ASSERT(Port->OsDevice == Irp->Device);

            Status = PmInitialize(Irp->Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(AhciDriver, Irp, Status);
                break;
            }

            AhcipStartPort(Irp, Port);
            break;

        case IrpMinorQueryResources:
        case IrpMinorQueryChildren:
            IoCompleteIrp(AhciDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorIdle:
            IoCompleteIrp(AhciDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorSuspend:
            IoCompleteIrp(AhciDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorResume:
            IoCompleteIrp(AhciDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorRemoveDevice:

            //
            // In the case where it's just the device that's disappearing, this
            // cleanup call has already happened and will end up being a no-op.
            // But if the AHCI controller disappeared, then this call won't
            // have happened yet, and the disk needs to be cleaned up without
            // touching the port registers. They are all dead.
            //

            AhcipProcessPortRemoval(Port, FALSE);
            IoCompleteIrp(AhciDriver, Irp, STATUS_SUCCESS);
            break;

        default:
            break;
        }
    }

    return;
}

VOID
AhcipDispatchPortSystemControl (
    PIRP Irp,
    PAHCI_PORT Device
    )

/*++

Routine Description:

    This routine handles System Control IRPs for an AHCI child device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    PVOID Context;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    ULONGLONG PropertiesFileSize;
    KSTATUS Status;

    Context = Irp->U.SystemControl.SystemContext;
    if (Irp->Direction == IrpUp) {

        ASSERT(Irp->MinorCode == IrpMinorSystemControlSynchronize);

        PmDeviceReleaseReference(Device->OsDevice);
        return;
    }

    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Status = STATUS_PATH_NOT_FOUND;
        if (Lookup->Root != FALSE) {

            //
            // Enable opening of the root as a single file.
            //

            Properties = Lookup->Properties;
            Properties->FileId = 0;
            Properties->Type = IoObjectBlockDevice;
            Properties->HardLinkCount = 1;
            Properties->BlockSize = ATA_SECTOR_SIZE;
            Properties->BlockCount = Device->TotalSectors;
            Properties->Size = Device->TotalSectors * ATA_SECTOR_SIZE;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(AhciDriver, Irp, Status);
        break;

    //
    // Writes to the disk's properties are not allowed. Fail if the data
    // has changed.
    //

    case IrpMinorSystemControlWriteFileProperties:
        FileOperation = (PSYSTEM_CONTROL_FILE_OPERATION)Context;
        Properties = FileOperation->FileProperties;
        PropertiesFileSize = Properties->Size;
        if ((Properties->FileId != 0) ||
            (Properties->Type != IoObjectBlockDevice) ||
            (Properties->HardLinkCount != 1) ||
            (Properties->BlockSize != ATA_SECTOR_SIZE) ||
            (Properties->BlockCount != Device->TotalSectors) ||
            (PropertiesFileSize != (Device->TotalSectors * ATA_SECTOR_SIZE))) {

            Status = STATUS_NOT_SUPPORTED;

        } else {
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(AhciDriver, Irp, Status);
        break;

    //
    // Do not support hard disk device truncation.
    //

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(AhciDriver, Irp, STATUS_NOT_SUPPORTED);
        break;

    //
    // Gather and return device information.
    //

    case IrpMinorSystemControlDeviceInformation:
        break;

    //
    // Send a cache flush command to the device upon getting a synchronize
    // request.
    //

    case IrpMinorSystemControlSynchronize:
        Status = PmDeviceAddReference(Device->OsDevice);
        if (!KSUCCESS(Status)) {
            IoCompleteIrp(AhciDriver, Irp, Status);
            break;
        }

        IoPendIrp(AhciDriver, Irp);
        Status = AhcipEnqueueIrp(Device, Irp);
        if (!KSUCCESS(Status)) {
            PmDeviceReleaseReference(Device->OsDevice);
            IoCompleteIrp(AhciDriver, Irp, Status);
        }

        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
AhcipProcessResourceRequirements (
    PIRP Irp,
    PAHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an AHCI controller. It adds an interrupt vector requirement for
    any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Controller - Supplies a pointer to the AHCI controller.

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
AhcipStartController (
    PIRP Irp,
    PAHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine starts an AHCI controller device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Controller - Supplies a pointer to the AHCI controller.

Return Value:

    Status code.

--*/

{

    UINTN AlignmentOffset;
    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    ULONG BarCount;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION ControllerBase;
    PHYSICAL_ADDRESS EndAddress;
    PRESOURCE_ALLOCATION LineAllocation;
    UINTN PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    UINTN Size;
    KSTATUS Status;
    PVOID VirtualAddress;

    ControllerBase = NULL;
    LineAllocation = NULL;
    Status = PmInitialize(Irp->Device);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = PmDeviceAddReference(Irp->Device);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    BarCount = 0;
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

            ASSERT((Controller->InterruptVector == INVALID_INTERRUPT_VECTOR) ||
                   (Controller->InterruptVector == Allocation->Allocation));

            ASSERT(Allocation->OwningAllocation != NULL);

            //
            // Save the line and vector number.
            //

            LineAllocation = Allocation->OwningAllocation;
            Controller->InterruptLine = LineAllocation->Allocation;
            Controller->InterruptVector = Allocation->Allocation;

        } else if ((Allocation->Type == ResourceTypePhysicalAddressSpace) ||
                   (Allocation->Type == ResourceTypeIoPort)) {

            BarCount += 1;
            if ((BarCount == 6) &&
                (Allocation->Type == ResourceTypePhysicalAddressSpace) &&
                (Allocation->Length != 0)) {

                ASSERT(ControllerBase == NULL);

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

    if ((ControllerBase == NULL) ||
        (Controller->InterruptVector == INVALID_INTERRUPT_VECTOR)) {

        RtlDebugPrint("AHCI: Missing resources.\n");
        Status = STATUS_INVALID_CONFIGURATION;
        goto StartControllerEnd;
    }

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
        Size = (ULONG)(EndAddress - PhysicalAddress);
        VirtualAddress = MmMapPhysicalAddress(PhysicalAddress,
                                              Size,
                                              TRUE,
                                              FALSE,
                                              TRUE);

        if (VirtualAddress == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartControllerEnd;
        }

        Controller->ControllerBase = VirtualAddress + AlignmentOffset;
    }

    ASSERT(Controller->ControllerBase != NULL);

    //
    // Put the controller into a known state.
    //

    Status = AhcipResetController(Controller);
    if (!KSUCCESS(Status)) {
        goto StartControllerEnd;
    }

    if (Controller->InterruptHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.InterruptServiceRoutine = AhciInterruptService;
        Connect.DispatchServiceRoutine = AhciInterruptServiceDpc;
        Connect.Context = Controller;
        Connect.LineNumber = Controller->InterruptLine;
        Connect.Vector = Controller->InterruptVector;
        Connect.Interrupt = &(Controller->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            goto StartControllerEnd;
        }
    }

    Status = STATUS_SUCCESS;

StartControllerEnd:
    PmDeviceReleaseReference(Irp->Device);
    return Status;
}

VOID
AhcipEnumeratePorts (
    PIRP Irp,
    PAHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine enumerates all active ports on the AHCI controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Controller - Supplies a pointer to the AHCI controller.

Return Value:

    None. The IRP is completed with the appropriate status.

--*/

{

    ULONG ChildCount;
    PDEVICE Children[AHCI_PORT_COUNT];
    ULONGLONG Duration;
    PAHCI_PORT Port;
    ULONG PortIndex;
    ULONGLONG Start;
    KSTATUS Status;

    Status = PmDeviceAddReference(Irp->Device);
    if (!KSUCCESS(Status)) {
        IoCompleteIrp(AhciDriver, Irp, Status);
        return;
    }

    Start = HlQueryTimeCounter();
    ChildCount = 0;
    for (PortIndex = 0; PortIndex < AHCI_PORT_COUNT; PortIndex += 1) {
        Port = &(Controller->Ports[PortIndex]);
        Status = AhcipProbePort(Controller, PortIndex);
        if (!KSUCCESS(Status)) {
            if (Status == STATUS_NO_MEDIA) {
                if (Port->OsDevice != NULL) {
                    RtlDebugPrint("AHCI: Port %d device gone.\n", PortIndex);

                    //
                    // The device disappeared, but AHCI is still around, so
                    // port registers can be touched.
                    //

                    AhcipProcessPortRemoval(&(Controller->Ports[PortIndex]),
                                            TRUE);
                }

                continue;
            }

            RtlDebugPrint("AHCI: Probe port %d failed: %d\n",
                          PortIndex,
                          Status);

            goto EnumeratePortsEnd;
        }

        //
        // Create a new device if there was not one there before.
        //

        Port = &(Controller->Ports[PortIndex]);
        if (Port->OsDevice == NULL) {
            Status = IoCreateDevice(AhciDriver,
                                    Port,
                                    Irp->Device,
                                    "Disk",
                                    DISK_CLASS_ID,
                                    NULL,
                                    &(Port->OsDevice));

            if (!KSUCCESS(Status)) {
                goto EnumeratePortsEnd;
            }
        }

        Children[ChildCount] = Port->OsDevice;
        ChildCount += 1;
    }

    Duration = HlQueryTimeCounter() - Start;
    AhciEnumerationMilliseconds = (Duration * MILLISECONDS_PER_SECOND) /
                                  HlQueryTimeCounterFrequency();

    if (ChildCount != 0) {
        Status = IoMergeChildArrays(Irp,
                                    Children,
                                    ChildCount,
                                    AHCI_ALLOCATION_TAG);

        if (!KSUCCESS(Status)) {
            goto EnumeratePortsEnd;
        }
    }

EnumeratePortsEnd:
    PmDeviceReleaseReference(Irp->Device);
    IoCompleteIrp(AhciDriver, Irp, Status);
    return;
}

VOID
AhcipStartPort (
    PIRP Irp,
    PAHCI_PORT Port
    )

/*++

Routine Description:

    This routine starts the AHCI port device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Port - Supplies a pointer to the AHCI port to start.

Return Value:

    None. The IRP is completed with the appropriate status.

--*/

{

    KSTATUS Status;

    Status = PmDeviceAddReference(Irp->Device);
    if (!KSUCCESS(Status)) {
        IoCompleteIrp(AhciDriver, Irp, Status);
        return;
    }

    Status = STATUS_SUCCESS;
    if (Port->TotalSectors == 0) {
        Status = AhcipEnumeratePort(Port);
        if (!KSUCCESS(Status)) {
            goto StartPortEnd;
        }
    }

StartPortEnd:
    PmDeviceReleaseReference(Irp->Device);
    IoCompleteIrp(AhciDriver, Irp, Status);
    return;
}

