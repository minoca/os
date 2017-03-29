/*++

Copyright (c) 2017 Minoca Corp. All Rights Reserved

Module Name:

    bcm27i2c.c

Abstract:

    This module implements support for the Broadcom 27xx I2C driver.

Author:

    Chris Stevens 18-Jan-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/spb/spbhost.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/soc/bcm2709.h>

//
// --------------------------------------------------------------------- Macros
//

#define BCM27_READ_I2C(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define BCM27_WRITE_I2C(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define BCM27_I2C_ALLOCATION_TAG 0x32493242 // '2I2B'

//
// Define a set of BCM2709 I2C controller flags.
//

#define BCM27_I2C_CONTROLLER_FLAG_10_BIT_ADDRESS 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an BCM27xx I2C controller.

Members:

    OsDevice - Stores a pointer to the OS device object.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    SlaveAddress - Stores the device slave address.

    Flags - Stores a bitmask of controller flags. See
        BCM27_I2C_CONTROLLER_FLAG_* for details.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    ControllerBase - Stores the virtual address of the memory mapping to the
        I2C controller registers.

    SpbController - Stores a pointer to the library Simple Peripheral Bus
        controller.

    Transfer - Stores the current transfer being worked on.

    PendingInterrupts - Stores a bitfield of pending interrupts.

    Lock - Stores a pointer to a lock serializing access to the controller.

    FifoDepth - Stores the depth of the transmit and receive FIFOs.

    FifoThreshold - Stores the FIFO threshold value that causes TX/RX ready
        interrupts to fire.

    InterruptMask - Stores the current interrupt mask.

    Control - Stores the control register value.

--*/

typedef struct _BCM27_I2C_CONTROLLER {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    USHORT SlaveAddress;
    ULONG Flags;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PSPB_CONTROLLER SpbController;
    PSPB_TRANSFER Transfer;
    ULONG PendingInterrupts;
    PQUEUED_LOCK Lock;
    ULONG InterruptMask;
    ULONG Control;
} BCM27_I2C_CONTROLLER, *PBCM27_I2C_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Bcm27I2cAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Bcm27I2cDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27I2cDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27I2cDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27I2cDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27I2cDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
Bcm27I2cInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
Bcm27I2cInterruptServiceWorker (
    PVOID Context
    );

KSTATUS
Bcm27I2cProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Bcm27I2cStartDevice (
    PIRP Irp,
    PBCM27_I2C_CONTROLLER Device
    );

KSTATUS
Bcm27I2cInitializeController (
    PBCM27_I2C_CONTROLLER Controller
    );

KSTATUS
Bcm27I2cConfigureBus (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    );

KSTATUS
Bcm27I2cSubmitTransfer (
    PVOID Context,
    PSPB_TRANSFER Transfer
    );

KSTATUS
Bcm27I2cSetupTransfer (
    PBCM27_I2C_CONTROLLER Controller,
    PSPB_TRANSFER Transfer
    );

KSTATUS
Bcm27I2cTransferData (
    PBCM27_I2C_CONTROLLER Controller,
    PSPB_TRANSFER Transfer,
    ULONG InterruptStatus
    );

VOID
Bcm27I2cSendStop (
    PBCM27_I2C_CONTROLLER Controller
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Bcm27I2cDriver;

SPB_FUNCTION_TABLE Bcm27I2cFunctionTableTemplate = {
    Bcm27I2cConfigureBus,
    Bcm27I2cSubmitTransfer,
    NULL,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the BCM27xx I2C driver. It registers
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

    Bcm27I2cDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Bcm27I2cAddDevice;
    FunctionTable.DispatchStateChange = Bcm27I2cDispatchStateChange;
    FunctionTable.DispatchOpen = Bcm27I2cDispatchOpen;
    FunctionTable.DispatchClose = Bcm27I2cDispatchClose;
    FunctionTable.DispatchIo = Bcm27I2cDispatchIo;
    FunctionTable.DispatchSystemControl = Bcm27I2cDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Bcm27I2cAddDevice (
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

    PBCM27_I2C_CONTROLLER Controller;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(BCM27_I2C_CONTROLLER),
                                        BCM27_I2C_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(BCM27_I2C_CONTROLLER));
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Controller->Lock = KeCreateQueuedLock();
    if (Controller->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Controller);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            if (Controller->Lock != NULL) {
                KeDestroyQueuedLock(Controller->Lock);
            }

            MmFreeNonPagedPool(Controller);
        }
    }

    return Status;
}

VOID
Bcm27I2cDispatchStateChange (
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
            Status = Bcm27I2cProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Bcm27I2cDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Bcm27I2cStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Bcm27I2cDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Bcm27I2cDispatchOpen (
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
Bcm27I2cDispatchClose (
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
Bcm27I2cDispatchIo (
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
Bcm27I2cDispatchSystemControl (
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

INTERRUPT_STATUS
Bcm27I2cInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the interrupt service routine for the BCM27xx I2C
    controller.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    ULONG Control;
    PBCM27_I2C_CONTROLLER Controller;
    ULONG Status;

    Controller = Context;
    Status = BCM27_READ_I2C(Controller, Bcm2709I2cStatus);
    Status &= Controller->InterruptMask;
    if (Status != 0) {

        //
        // Disable the transmit or receive interrupts if they fired.
        //

        Control = Controller->Control;
        if ((Status & BCM2709_I2C_STATUS_TRANSMIT_FIFO_WRITING) != 0) {
            Control &= ~BCM2709_I2C_CONTROL_INTERRUPT_TRANSMIT;
        }

        if ((Status & BCM2709_I2C_STATUS_RECEIVE_FIFO_READING) != 0) {
            Control &= ~BCM2709_I2C_CONTROL_INTERRUPT_RECEIVE;
        }

        if (Control != Controller->Control) {
            BCM27_WRITE_I2C(Controller, Bcm2709I2cControl, Control);
        }

        BCM27_WRITE_I2C(Controller, Bcm2709I2cStatus, Status);
        RtlAtomicOr32(&(Controller->PendingInterrupts), Status);
        return InterruptStatusClaimed;
    }

    return InterruptStatusNotClaimed;
}

INTERRUPT_STATUS
Bcm27I2cInterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the low level interrupt service routine for the
    BCM27xxx I2c controller.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PBCM27_I2C_CONTROLLER Controller;
    ULONG InterruptBits;
    KSTATUS Status;
    PSPB_TRANSFER Transfer;

    Controller = Context;
    InterruptBits = RtlAtomicExchange32(&(Controller->PendingInterrupts), 0);
    if (InterruptBits == 0) {
        return InterruptStatusClaimed;
    }

    KeAcquireQueuedLock(Controller->Lock);
    Transfer = Controller->Transfer;
    if (Transfer == NULL) {
        goto InterruptServiceWorkerEnd;
    }

    //
    // If an error came in, fail the current transfer.
    //

    if ((InterruptBits & BCM2709_I2C_STATUS_ACK_ERROR) != 0) {
        RtlDebugPrint("BCM27 I2C: Error 0x%08x\n", InterruptBits);
        Status = STATUS_DEVICE_IO_ERROR;

    //
    // Transfer more data. If the transfer fills the FIFOs, then
    // break out and wait for the interrupt to fire to put more data in.
    //

    } else {
        Status = Bcm27I2cTransferData(Controller, Transfer, InterruptBits);
        if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
            goto InterruptServiceWorkerEnd;
        }
    }

    //
    // If this was the last transfer, send the stop.
    //

    if ((Transfer->Flags & SPB_TRANSFER_FLAG_LAST) != 0) {
        Bcm27I2cSendStop(Controller);
    }

    //
    // The transfer completed entirely, so complete this one and go get a
    // new one.
    //

    Controller->Transfer = NULL;
    Transfer = SpbTransferCompletion(Controller->SpbController,
                                     Transfer,
                                     Status);

    if (Transfer != NULL) {
        Bcm27I2cSetupTransfer(Controller, Transfer);

    } else {
        Controller->InterruptMask = 0;
        BCM27_WRITE_I2C(Controller, Bcm2709I2cControl, 0);
    }

InterruptServiceWorkerEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Bcm27I2cProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an BCM27xxx I2C controller. It adds an interrupt vector requirement
    for any interrupt line requested.

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
Bcm27I2cStartDevice (
    PIRP Irp,
    PBCM27_I2C_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the BCM27xxx I2C device.

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
    SPB_CONTROLLER_INFORMATION Registration;
    ULONG Size;
    KSTATUS Status;

    ControllerBase = NULL;

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    ASSERT(Device->InterruptHandle == INVALID_HANDLE);

    Device->InterruptResourcesFound = FALSE;
    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // If the resource is an interrupt vector, then it should have an
        // owning interrupt line allocation.
        //

        if (Allocation->Type == ResourceTypeInterruptVector) {
            LineAllocation = Allocation->OwningAllocation;
            if (Device->InterruptResourcesFound == FALSE) {

                ASSERT(Allocation->OwningAllocation != NULL);

                //
                // Save the line and vector number.
                //

                Device->InterruptLine = LineAllocation->Allocation;
                Device->InterruptVector = Allocation->Allocation;
                Device->InterruptResourcesFound = TRUE;

            } else {

                ASSERT((Device->InterruptLine == LineAllocation->Allocation) &&
                       (Device->InterruptVector == Allocation->Allocation));
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
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = ControllerBase->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a page, then the failure code at the bottom needs
        // to be fancier.
        //

        ASSERT(Size == PageSize);

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
    // Allocate the controller structures.
    //

    if (Device->SpbController == NULL) {
        RtlZeroMemory(&Registration, sizeof(SPB_CONTROLLER_INFORMATION));
        Registration.Version = SPB_CONTROLLER_INFORMATION_VERSION;
        Registration.Context = Device;
        Registration.Device = Device->OsDevice;
        Registration.MaxFrequency = 400000;
        Registration.BusType = ResourceSpbBusI2c;
        RtlCopyMemory(&(Registration.FunctionTable),
                      &Bcm27I2cFunctionTableTemplate,
                      sizeof(SPB_FUNCTION_TABLE));

        Status = SpbCreateController(&Registration, &(Device->SpbController));
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    //
    // Start up the controller.
    //

    Status = SpbStartController(Device->SpbController);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Connect the interrupt.
    //

    if (Device->InterruptHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Device->InterruptLine;
        Connect.Vector = Device->InterruptVector;
        Connect.InterruptServiceRoutine = Bcm27I2cInterruptService;
        Connect.LowLevelServiceRoutine = Bcm27I2cInterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Status = Bcm27I2cInitializeController(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->ControllerBase != NULL) {
            MmUnmapAddress(Device->ControllerBase, MmPageSize());
            Device->ControllerBase = NULL;
        }

        if (Device->SpbController != NULL) {
            SpbDestroyController(Device->SpbController);
            Device->SpbController = NULL;
        }
    }

    return Status;
}

KSTATUS
Bcm27I2cInitializeController (
    PBCM27_I2C_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine resets and initializes the given I2C controller.

Arguments:

    Controller - Supplies a pointer to the controller to reset.

Return Value:

    Status code.

--*/

{

    BCM27_WRITE_I2C(Controller, Bcm2709I2cControl, 0);
    return STATUS_SUCCESS;
}

KSTATUS
Bcm27I2cConfigureBus (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    )

/*++

Routine Description:

    This routine configures the given Simple Peripheral Bus controller.

Arguments:

    Context - Supplies the host controller context.

    Configuration - Supplies a pointer to the new configuration to set.

Return Value:

    Status code.

--*/

{

    USHORT Address;
    USHORT AddressHigh;
    PBCM27_I2C_CONTROLLER Controller;
    PRESOURCE_SPB_I2C I2c;

    Controller = Context;
    if (Configuration->BusType != ResourceSpbBusI2c) {
        return STATUS_INVALID_PARAMETER;
    }

    I2c = PARENT_STRUCTURE(Configuration, RESOURCE_SPB_I2C, Header);
    if ((Configuration->Flags & RESOURCE_SPB_DATA_SLAVE) != 0) {
        return STATUS_NOT_SUPPORTED;
    }

    Controller->Flags &= ~BCM27_I2C_CONTROLLER_FLAG_10_BIT_ADDRESS;
    if ((I2c->Flags & RESOURCE_SPB_I2C_10_BIT_ADDRESSING) != 0) {
        Controller->Flags |= BCM27_I2C_CONTROLLER_FLAG_10_BIT_ADDRESS;
    }

    KeAcquireQueuedLock(Controller->Lock);
    Address = I2c->SlaveAddress;

    //
    // If this is a 10-bit address, then the slave address register gets
    // written with a well-known 5-bit address header and then the two most
    // significant bits of the 10-bit address.
    //

    if ((Controller->Flags & BCM27_I2C_CONTROLLER_FLAG_10_BIT_ADDRESS) != 0) {
        AddressHigh = (Address & BCM2709_I2C_10_BIT_ADDRESS_HIGH_MASK) >>
                      BCM2709_I2C_10_BIT_ADDRESS_HIGH_SHIFT;

        Address = (AddressHigh << BCM2709_I2C_SLAVE_ADDRESS_10_BIT_HIGH_SHIFT) &
                  BCM2709_I2C_SLAVE_ADDRESS_10_BIT_HIGH_MASK;

        Address |= BCM2709_I2C_SLAVE_ADDRESS_10_BIT_HEADER;

    //
    // Otherwise just take the lower 7-bits of the supplied address.
    //

    } else {
        Address = (Address << BCM2709_I2C_SLAVE_ADDRESS_SHIFT) &
                  BCM2709_I2C_SLAVE_ADDRESS_MASK;
    }

    BCM27_WRITE_I2C(Controller,
                    Bcm2709I2cControl,
                    BCM2709_I2C_CONTROL_CLEAR_FIFO);

    BCM27_WRITE_I2C(Controller, Bcm2709I2cSlaveAddress, Address);
    Controller->SlaveAddress = I2c->SlaveAddress;
    KeReleaseQueuedLock(Controller->Lock);
    return STATUS_SUCCESS;
}

KSTATUS
Bcm27I2cSubmitTransfer (
    PVOID Context,
    PSPB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called to execute a single transfer on the Simple
    Peripheral Bus. The host controller is responsible for implementing the
    delay set in the transfer.

Arguments:

    Context - Supplies the host controller context.

    Transfer - Supplies a pointer to the transfer to begin executing. The
        controller can return immediately, and should call
        SpbProcessCompletedTransfer when the transfer completes.

Return Value:

    Status code indicating whether or not the transfer was successfully
    started.

--*/

{

    PBCM27_I2C_CONTROLLER Controller;
    KSTATUS Status;

    Controller = Context;
    KeAcquireQueuedLock(Controller->Lock);
    Status = Bcm27I2cSetupTransfer(Controller, Transfer);
    if (!KSUCCESS(Status)) {
        goto SubmitTransferEnd;
    }

SubmitTransferEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return Status;
}

KSTATUS
Bcm27I2cSetupTransfer (
    PBCM27_I2C_CONTROLLER Controller,
    PSPB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called to execute a single transfer on the Simple
    Peripheral Bus. The host controller is responsible for implementing the
    delay set in the transfer.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the transfer to begin executing. The
        controller can return immediately, and should call
        SpbProcessCompletedTransfer when the transfer completes.

Return Value:

    Status code indicating whether or not the transfer was successfully
    started.

--*/

{

    USHORT Address;
    ULONG Control;
    ULONG Control10Bit;
    ULONG Flags;
    ULONG Mask;
    UINTN MaxSize;
    UINTN Size;
    ULONG Status;

    ASSERT(Controller->Transfer == NULL);

    Controller->Transfer = Transfer;
    Flags = Controller->Flags;
    Transfer->ReceiveSizeCompleted = 0;
    Transfer->TransmitSizeCompleted = 0;
    Mask = BCM2709_I2C_STATUS_ACK_ERROR |
           BCM2709_I2C_STATUS_CLOCK_STRETCH_TIMEOUT |
           BCM2709_I2C_STATUS_TRANSFER_DONE;

    Control = BCM2709_I2C_CONTROL_START_TRANSFER |
              BCM2709_I2C_CONTROL_ENABLE |
              BCM2709_I2C_CONTROL_INTERRUPT_DONE;

    //
    // Setup the transfer based on the direction.
    //

    MaxSize = BCM2709_I2C_DATA_LENGTH_MAX;
    switch (Transfer->Direction) {
    case SpbTransferDirectionIn:
        Control |= BCM2709_I2C_CONTROL_READ_TRANSFER |
                   BCM2709_I2C_CONTROL_INTERRUPT_RECEIVE;

        break;

    case SpbTransferDirectionOut:
        if ((Flags & BCM27_I2C_CONTROLLER_FLAG_10_BIT_ADDRESS) != 0) {
            MaxSize = BCM2709_I2C_DATA_LENGTH_MAX - 1;
        }

        Control |= BCM2709_I2C_CONTROL_INTERRUPT_TRANSMIT;
        break;

    default:

        ASSERT(FALSE);

        return STATUS_INVALID_PARAMETER;
    }

    //
    // Clear any old interrupts.
    //

    BCM27_WRITE_I2C(Controller, Bcm2709I2cControl, 0);
    BCM27_WRITE_I2C(Controller, Bcm2709I2cStatus, Mask);
    Controller->InterruptMask = Mask;

    //
    // Scrub the transfer size.
    //

    if (Transfer->Size > MaxSize) {
        return STATUS_INVALID_PARAMETER;
    }

    Size = Transfer->Size;

    //
    // If this is 10-bit addressing, write the lower 8-bits of the address to
    // the FIFO.
    //

    if ((Flags & BCM27_I2C_CONTROLLER_FLAG_10_BIT_ADDRESS) != 0) {
        if (Transfer->Direction == SpbTransferDirectionIn) {
            BCM27_WRITE_I2C(Controller, Bcm2709I2cDataLength, 1);

        } else {
            BCM27_WRITE_I2C(Controller, Bcm2709I2cDataLength, Size + 1);
        }

        Address = Controller->SlaveAddress;
        Address = (Address & BCM2709_I2C_10_BIT_ADDRESS_LOW_MASK) >>
                  BCM2709_I2C_10_BIT_ADDRESS_LOW_SHIFT;

        BCM27_WRITE_I2C(Controller, Bcm2709I2cDataFifo, Address);

        //
        // If this is a read transfer, then trigger a write to set the lower
        // 8-bits of the slave address and wait for the transfer to become
        // active.
        //

        if (Transfer->Direction == SpbTransferDirectionIn) {
            Control10Bit = BCM2709_I2C_CONTROL_ENABLE |
                           BCM2709_I2C_CONTROL_START_TRANSFER;

            BCM27_WRITE_I2C(Controller, Bcm2709I2cControl, Control10Bit);
            do {
                Status = BCM27_READ_I2C(Controller, Bcm2709I2cStatus);

            } while ((Status & BCM2709_I2C_STATUS_TRANSFER_ACTIVE) == 0);

            BCM27_WRITE_I2C(Controller, Bcm2709I2cDataLength, Size);
        }

    } else {
        BCM27_WRITE_I2C(Controller, Bcm2709I2cDataLength, Size);
    }

    //
    // Fire off the transfer.
    //

    Controller->Control = Control;
    BCM27_WRITE_I2C(Controller, Bcm2709I2cControl, Control);

    //
    // Delay if needed.
    //

    if (Transfer->MicrosecondDelay != 0) {
        KeDelayExecution(FALSE, FALSE, Transfer->MicrosecondDelay);
    }

    return STATUS_SUCCESS;
}

KSTATUS
Bcm27I2cTransferData (
    PBCM27_I2C_CONTROLLER Controller,
    PSPB_TRANSFER Transfer,
    ULONG InterruptStatus
    )

/*++

Routine Description:

    This routine transfers data to and from the I2C controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the transfer to execute.

    InterruptStatus - Supplies the current interrupt status.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED if more data needs to be sent before the
    transfer is complete.

    STATUS_SUCCESS if the data was transferred successfully.

    Other status codes if the transfer failed.

--*/

{

    UCHAR Buffer[BCM2709_I2C_BUFFER_SIZE];
    SPB_TRANSFER_DIRECTION Direction;
    ULONG FifoStatus;
    UINTN Index;
    UINTN Offset;
    ULONG Size;
    KSTATUS Status;
    BOOL TransferDone;

    Direction = Transfer->Direction;
    TransferDone = FALSE;
    Status = STATUS_SUCCESS;
    Size = sizeof(Buffer);
    if (Direction == SpbTransferDirectionOut) {
        if (((InterruptStatus & BCM2709_I2C_STATUS_TRANSFER_DONE) != 0) ||
            ((InterruptStatus &
              BCM2709_I2C_STATUS_TRANSMIT_FIFO_WRITING) != 0)) {

            if (Transfer->TransmitSizeCompleted == Transfer->Size) {
                TransferDone = TRUE;
                goto TransferDataEnd;
            }

            if (Size > Transfer->Size - Transfer->TransmitSizeCompleted) {
                Size = Transfer->Size - Transfer->TransmitSizeCompleted;
            }

            Offset = Transfer->Offset + Transfer->TransmitSizeCompleted;
            Status = MmCopyIoBufferData(Transfer->IoBuffer,
                                        Buffer,
                                        Offset,
                                        Size,
                                        FALSE);

            if (!KSUCCESS(Status)) {
                TransferDone = TRUE;
                goto TransferDataEnd;
            }

            //
            // Attempt to write the whole buffer, but be careful. There is no
            // way to query the FIFO size ahead of time, so just keep reading
            // the status and write while it's got free space.
            //

            for (Index = 0; Index < Size; Index += sizeof(ULONG)) {
                FifoStatus = BCM27_READ_I2C(Controller, Bcm2709I2cStatus);
                if ((FifoStatus & BCM2709_I2C_STATUS_TRANSMIT_FIFO_DATA) == 0) {
                    break;
                }

                BCM27_WRITE_I2C(Controller, Bcm2709I2cDataFifo, Buffer[Index]);
            }

            Transfer->TransmitSizeCompleted += Index;
        }

    } else {

        ASSERT(Direction == SpbTransferDirectionIn);

        //
        // If some actual receive data came in, grab it.
        //

        if (((InterruptStatus & BCM2709_I2C_STATUS_TRANSFER_DONE) != 0) ||
            ((InterruptStatus &
              BCM2709_I2C_STATUS_RECEIVE_FIFO_READING) != 0)) {

            if (Size > Transfer->Size - Transfer->ReceiveSizeCompleted) {
                Size = Transfer->Size - Transfer->ReceiveSizeCompleted;
            }

            for (Index = 0; Index < Size; Index += sizeof(ULONG)) {
                FifoStatus = BCM27_READ_I2C(Controller, Bcm2709I2cStatus);
                if ((FifoStatus & BCM2709_I2C_STATUS_RECEIVE_FIFO_DATA) == 0) {
                    break;
                }

                Buffer[Index] = BCM27_READ_I2C(Controller, Bcm2709I2cDataFifo);
            }

            Offset = Transfer->Offset + Transfer->ReceiveSizeCompleted;
            Status = MmCopyIoBufferData(Transfer->IoBuffer,
                                        Buffer,
                                        Offset,
                                        Index,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                TransferDone = TRUE;
                goto TransferDataEnd;
            }

            Transfer->ReceiveSizeCompleted += Index;
            if (Transfer->ReceiveSizeCompleted >= Transfer->Size) {
                TransferDone = TRUE;
                goto TransferDataEnd;
            }
        }
    }

    //
    // Reset to the initial control state. This re-enables the transmit and
    // receive FIFO read/write interrupts.
    //

    BCM27_WRITE_I2C(Controller, Bcm2709I2cControl, Controller->Control);

TransferDataEnd:
    if (TransferDone != FALSE) {
        Controller->Transfer = NULL;
    }

    if ((KSUCCESS(Status)) && (TransferDone == FALSE)) {
        Status = STATUS_MORE_PROCESSING_REQUIRED;
    }

    return Status;
}

VOID
Bcm27I2cSendStop (
    PBCM27_I2C_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine sends a stop condition out on the I2C bus.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    Controller->Control = 0;
    BCM27_WRITE_I2C(Controller, Bcm2709I2cControl, 0);
    return;
}

