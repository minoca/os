/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwhci.c

Abstract:

    This module implements support for the DesignWare High-Speed USB 2.0
    On-The-Go (HS OTG) host controller.

Author:

    Chris Stevens 28-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usbhost.h>
#include "dwhci.h"

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

    This structure stores context about a DWHCI Host Controller.

Members:

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    Controller - Stores a pointer to the OTG controller.

    RegisterBasePhysical - Stores the physical memory address where the DWHCI
        registers are located.

    RegisterBase - Stores a pointer to the virtual address where the DWHCI
        registers are located.

    ChannelCount - Stores the number of channels for this host controller.

    Speed - Stores the speed of the DWHCI host controller.

    MaxTransferSize - Stores the maximum tranfer size for the DWHCI host
        controller.

    MaxPacketCount - Store the maximum packet count for the DWHCI host
        controller.

    Revision - Stores the revision of the DWHCI host controller.

--*/

typedef struct _DWHCI_CONTROLLER_CONTEXT {
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PDWHCI_CONTROLLER Controller;
    PHYSICAL_ADDRESS RegisterBasePhysical;
    PVOID RegisterBase;
    ULONG ChannelCount;
    USB_DEVICE_SPEED Speed;
    ULONG MaxTransferSize;
    ULONG MaxPacketCount;
    ULONG Revision;
} DWHCI_CONTROLLER_CONTEXT, *PDWHCI_CONTROLLER_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
DwhciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
DwhciDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DwhciDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DwhciDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DwhciDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DwhciDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
DwhcipProcessResourceRequirements (
    PIRP Irp,
    PDWHCI_CONTROLLER_CONTEXT Device
    );

KSTATUS
DwhcipStartDevice (
    PIRP Irp,
    PDWHCI_CONTROLLER_CONTEXT Device
    );

VOID
DwhcipEnumerateChildren (
    PIRP Irp,
    PDWHCI_CONTROLLER_CONTEXT Device
    );

KSTATUS
DwhcipGatherControllerParameters (
    PDWHCI_CONTROLLER_CONTEXT ControllerContext,
    PRESOURCE_ALLOCATION ControllerBase
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER DwhciDriver = NULL;

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

    This routine is the entry point for the DWHCI driver. It registers its
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

    DwhciDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = DwhciAddDevice;
    FunctionTable.DispatchStateChange = DwhciDispatchStateChange;
    FunctionTable.DispatchOpen = DwhciDispatchOpen;
    FunctionTable.DispatchClose = DwhciDispatchClose;
    FunctionTable.DispatchIo = DwhciDispatchIo;
    FunctionTable.DispatchSystemControl = DwhciDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
DwhciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the DWHCI driver
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

    PDWHCI_CONTROLLER_CONTEXT NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocateNonPagedPool(sizeof(DWHCI_CONTROLLER_CONTEXT),
                                       DWHCI_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(DWHCI_CONTROLLER_CONTEXT));
    NewDevice->InterruptHandle = INVALID_HANDLE;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);
    return Status;
}

VOID
DwhciDispatchStateChange (
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

    PDWHCI_CONTROLLER_CONTEXT Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PDWHCI_CONTROLLER_CONTEXT)DeviceContext;

    //
    // If there is no controller context, then DWHCI is acting as the bus
    // driver for the root hub. Simply complete standard IRPs.
    //

    if (Device == NULL) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
        case IrpMinorStartDevice:
        case IrpMinorQueryChildren:
            IoCompleteIrp(DwhciDriver, Irp, STATUS_SUCCESS);
            break;

        default:
            break;
        }

        return;
    }

    switch (Irp->MinorCode) {
    case IrpMinorQueryResources:

        //
        // On the way up, filter the resource requirements to add interrupt
        // vectors to any lines.
        //

        if (Irp->Direction == IrpUp) {
            Status = DwhcipProcessResourceRequirements(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(DwhciDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorStartDevice:

        //
        // Attempt to fire the thing up if the bus has already started it.
        //

        if (Irp->Direction == IrpUp) {
            Status = DwhcipStartDevice(Irp, Device);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(DwhciDriver, Irp, Status);
            }
        }

        break;

    case IrpMinorQueryChildren:
        if (Irp->Direction == IrpUp) {
            DwhcipEnumerateChildren(Irp, Device);
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
DwhciDispatchOpen (
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
DwhciDispatchClose (
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
DwhciDispatchIo (
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
DwhciDispatchSystemControl (
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
DwhcipProcessResourceRequirements (
    PIRP Irp,
    PDWHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a DWHCI Host controller. It adds an interrupt vector requirement
    for any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this DWHCI device.

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
DwhcipStartDevice (
    PIRP Irp,
    PDWHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine starts up the DWHCI controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this DWHCI device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PDWHCI_CONTROLLER Controller;
    PRESOURCE_ALLOCATION ControllerBase;
    PRESOURCE_ALLOCATION LineAllocation;
    KSTATUS Status;

    Controller = NULL;
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
    // Get DWHCI register parameters, including the register base and port
    // count.
    //

    Status = DwhcipGatherControllerParameters(Device, ControllerBase);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    ASSERT(ControllerBase->Allocation == Device->RegisterBasePhysical);

    //
    // Allocate the controller structures.
    //

    Controller = DwhcipInitializeControllerState(Device->RegisterBase,
                                                 Device->ChannelCount,
                                                 Device->Speed,
                                                 Device->MaxTransferSize,
                                                 Device->MaxPacketCount,
                                                 Device->Revision);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto StartDeviceEnd;
    }

    Device->Controller = Controller;

    //
    // Start up the controller.
    //

    Status = DwhcipInitializeController(Controller);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Register the device with the USB core. This is required before enabling
    // the interrupt.
    //

    Status = DwhcipRegisterController(Controller, Irp->Device);
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
    Connect.InterruptServiceRoutine = DwhcipInterruptService;
    Connect.Context = Device->Controller;
    Connect.Interrupt = &(Device->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    DwhcipSetInterruptHandle(Controller, Device->InterruptHandle);

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->InterruptHandle);
            Device->InterruptHandle = INVALID_HANDLE;
        }

        if (Controller != NULL) {
            DwhcipDestroyControllerState(Controller);
        }
    }

    return Status;
}

VOID
DwhcipEnumerateChildren (
    PIRP Irp,
    PDWHCI_CONTROLLER_CONTEXT Device
    )

/*++

Routine Description:

    This routine enumerates the root hub of a DWHCI controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this DWHCI device.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Forward this on to the USB core to figure out.
    //

    Status = UsbHostQueryChildren(Irp, Device->Controller->UsbCoreHandle);
    IoCompleteIrp(DwhciDriver, Irp, Status);
    return;
}

KSTATUS
DwhcipGatherControllerParameters (
    PDWHCI_CONTROLLER_CONTEXT ControllerContext,
    PRESOURCE_ALLOCATION ControllerBase
    )

/*++

Routine Description:

    This routine pokes around and collects various pieces of needed information
    for the controller, such as the register base and port count.

Arguments:

    ControllerContext - Supplies a pointer to the DWHCI controller context.

    ControllerBase - Supplies a pointer to the resource allocation defining the
        location of the controller's registers.

Return Value:

    Status code.

--*/

{

    ULONG AlignmentOffset;
    ULONG ChannelCount;
    PHYSICAL_ADDRESS EndAddress;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID RegisterAddress;
    ULONG RegisterValue;
    ULONG Size;
    KSTATUS Status;
    PVOID VirtualAddress;
    ULONG Width;

    PageSize = MmPageSize();
    ControllerContext->RegisterBasePhysical = ControllerBase->Allocation;

    //
    // Initialize and map the DWHCI registers.
    //

    if (ControllerContext->RegisterBase == NULL) {
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

    //
    // Read the host channel count. The stored value is one less than the
    // actual number of channels.
    //

    if (ControllerContext->ChannelCount == 0) {
        RegisterAddress = ControllerContext->RegisterBase +
                          DwhciRegisterHardware2;

        RegisterValue = HlReadRegister32(RegisterAddress);
        ChannelCount = 1 + ((RegisterValue &
                             DWHCI_HARDWARE2_HOST_CHANNEL_COUNT_MASK) >>
                             DWHCI_HARDWARE2_HOST_CHANNEL_COUNT_SHIFT);

        ControllerContext->ChannelCount = ChannelCount;
    }

    if (ControllerContext->ChannelCount == 0) {

        ASSERT(FALSE);

        Status = STATUS_NO_SUCH_DEVICE;
        goto GatherControllerParametersEnd;
    }

    //
    // Determine the speed of the DWHCI host controller.
    //

    if (ControllerContext->Speed == UsbDeviceSpeedInvalid) {
        RegisterAddress = ControllerContext->RegisterBase +
                          DwhciRegisterHardware2;

        RegisterValue = HlReadRegister32(RegisterAddress);
        if ((RegisterValue & DWHCI_HARDWARE2_HIGH_SPEED_MASK) ==
            DWHCI_HARDWARE2_HIGH_SPEED_NOT_SUPPORTED) {

            ControllerContext->Speed = UsbDeviceSpeedFull;

        } else {
            ControllerContext->Speed = UsbDeviceSpeedHigh;
        }
    }

    //
    // Determine the maximum transfer size and the maximum packet count for the
    // DWHCI host controller.
    //

    if ((ControllerContext->MaxTransferSize == 0) ||
        (ControllerContext->MaxPacketCount == 0)) {

        RegisterAddress = ControllerContext->RegisterBase +
                          DwhciRegisterHardware3;

        RegisterValue = HlReadRegister32(RegisterAddress);
        Width = (RegisterValue & DWHCI_HARDWARE3_TRANSFER_SIZE_WIDTH_MASK) >>
                DWHCI_HARDWARE3_TRANSFER_SIZE_WIDTH_SHIFT;

        Width += DWHCI_HARDWARE3_TRANSFER_SIZE_WIDTH_OFFSET;
        ControllerContext->MaxTransferSize = (1 << Width) - 1;
        Width = (RegisterValue & DWHCI_HARDWARE3_PACKET_COUNT_WIDTH_MASK) >>
                DWHCI_HARDWARE3_PACKET_COUNT_WIDTH_SHIFT;

        Width += DWHCI_HARDWARE3_PACKET_COUNT_WIDTH_OFFSET;
        ControllerContext->MaxPacketCount = (1 << Width) - 1;
    }

    if ((ControllerContext->MaxTransferSize == 0) ||
        (ControllerContext->MaxPacketCount == 0)) {

        ASSERT(FALSE);

        Status = STATUS_NO_SUCH_DEVICE;
        goto GatherControllerParametersEnd;
    }

    //
    // Query the revision.
    //

    RegisterAddress = ControllerContext->RegisterBase +
                      DwhciRegisterCoreId;

    ControllerContext->Revision = HlReadRegister32(RegisterAddress);
    Status = STATUS_SUCCESS;

GatherControllerParametersEnd:
    return Status;
}

