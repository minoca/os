/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rk3i2c.c

Abstract:

    This module implements support for the RockChip RK3xxx I2C driver.

Author:

    Evan Green 1-Apr-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/spb/spbhost.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/soc/rk32xx.h>

//
// --------------------------------------------------------------------- Macros
//

#define RK3_READ_I2C(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define RK3_WRITE_I2C(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define RK3_I2C_ALLOCATION_TAG 0x32493352

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an RK3xxx I2C controller.

Members:

    OsDevice - Stores a pointer to the OS device object.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    SlaveAddress - Stores the device slave address.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    ControllerBase - Stores the virtual address of the memory mapping to the
        I2C controller registers.

    SpbController - Stores a pointer to the library Simple Peripheral Bus
        controller.

    Transfer - Stores the current transfer being worked on.

    TransferDirection - Stores the direction of the current transfer. This is
        used in the ISR and thus cannot jube be accessed through the paged
        transfer itself.

    PendingInterrupts - Stores a bitfield of pending interrupts.

    Lock - Stores a pointer to a lock serializing access to the controller.

    FifoDepth - Stores the depth of the transmit and receive FIFOs.

    FifoThreshold - Stores the FIFO threshold value that causes TX/RX ready
        interrupts to fire.

    InterruptMask - Stores the current interrupt mask.

    Control - Stores the control register value.

--*/

typedef struct _RK3_I2C_CONTROLLER {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    USHORT SlaveAddress;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PSPB_CONTROLLER SpbController;
    PSPB_TRANSFER Transfer;
    SPB_TRANSFER_DIRECTION TransferDirection;
    ULONG PendingInterrupts;
    PQUEUED_LOCK Lock;
    ULONG InterruptMask;
    ULONG Control;
} RK3_I2C_CONTROLLER, *PRK3_I2C_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Rk3I2cAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Rk3I2cDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk3I2cDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk3I2cDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk3I2cDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk3I2cDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
Rk3I2cInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
Rk3I2cInterruptServiceWorker (
    PVOID Context
    );

KSTATUS
Rk3I2cProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Rk3I2cStartDevice (
    PIRP Irp,
    PRK3_I2C_CONTROLLER Device
    );

KSTATUS
Rk3I2cInitializeController (
    PRK3_I2C_CONTROLLER Controller
    );

KSTATUS
Rk3I2cConfigureBus (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    );

KSTATUS
Rk3I2cSubmitTransfer (
    PVOID Context,
    PSPB_TRANSFER Transfer
    );

KSTATUS
Rk3I2cSetupTransfer (
    PRK3_I2C_CONTROLLER Controller,
    PSPB_TRANSFER Transfer
    );

KSTATUS
Rk3I2cTransferData (
    PRK3_I2C_CONTROLLER Controller,
    PSPB_TRANSFER Transfer,
    ULONG InterruptStatus
    );

VOID
Rk3I2cSendStop (
    PRK3_I2C_CONTROLLER Controller
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Rk3I2cDriver;

SPB_FUNCTION_TABLE Rk3I2cFunctionTableTemplate = {
    Rk3I2cConfigureBus,
    Rk3I2cSubmitTransfer,
    NULL,
    NULL
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

    This routine is the entry point for the RK3xxx I2C driver. It registers
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

    Rk3I2cDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Rk3I2cAddDevice;
    FunctionTable.DispatchStateChange = Rk3I2cDispatchStateChange;
    FunctionTable.DispatchOpen = Rk3I2cDispatchOpen;
    FunctionTable.DispatchClose = Rk3I2cDispatchClose;
    FunctionTable.DispatchIo = Rk3I2cDispatchIo;
    FunctionTable.DispatchSystemControl = Rk3I2cDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Rk3I2cAddDevice (
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

    PRK3_I2C_CONTROLLER Controller;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(RK3_I2C_CONTROLLER),
                                        RK3_I2C_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(RK3_I2C_CONTROLLER));
    Controller->OsDevice = DeviceToken;
    Controller->InterruptHandle = INVALID_HANDLE;
    Controller->Lock = KeCreateQueuedLock();
    if (Controller->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Controller);

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
Rk3I2cDispatchStateChange (
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
            Status = Rk3I2cProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Rk3I2cDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Rk3I2cStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Rk3I2cDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Rk3I2cDispatchOpen (
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
Rk3I2cDispatchClose (
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
Rk3I2cDispatchIo (
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
Rk3I2cDispatchSystemControl (
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
Rk3I2cInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the interrupt service routine for the RK3xxx I2C
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

    UCHAR Address;
    PRK3_I2C_CONTROLLER Controller;
    ULONG Status;

    Controller = Context;
    Status = RK3_READ_I2C(Controller, Rk32I2cInterruptPending);
    Status &= Controller->InterruptMask;
    if (Status != 0) {
        RK3_WRITE_I2C(Controller, Rk32I2cInterruptPending, Status);

        //
        // Real quick, if the start just finished, turn around and send the
        // device address.
        //

        if ((Status & RK32_I2C_INTERRUPT_START) != 0) {
            Controller->Control &= ~RK32_I2C_CONTROL_START;
            RK3_WRITE_I2C(Controller, Rk32I2cControl, Controller->Control);
            Address = Controller->SlaveAddress << 1;
            if (Controller->TransferDirection == SpbTransferDirectionIn) {
                Address |= 0x1;
            }

            RK3_WRITE_I2C(Controller, Rk32I2cTransmitData0, Address);
            RK3_WRITE_I2C(Controller, Rk32I2cMasterTransmitCount, 1);
            Status &= ~RK32_I2C_INTERRUPT_START;
        }

        if (Status != 0) {
            RtlAtomicOr32(&(Controller->PendingInterrupts), Status);
        }

        return InterruptStatusClaimed;
    }

    return InterruptStatusNotClaimed;
}

INTERRUPT_STATUS
Rk3I2cInterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the low level interrupt service routine for the
    RK3xxx I2c controller.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PRK3_I2C_CONTROLLER Controller;
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

    if ((InterruptBits & RK32_I2C_INTERRUPT_NAK) != 0) {
        RtlDebugPrint("RK3 I2C: Error 0x%08x\n", InterruptBits);
        Status = STATUS_DEVICE_IO_ERROR;

    //
    // Transfer more data. If the transfer fills the FIFOs, then
    // break out and wait for the interrupt to fire to put more data in.
    //

    } else {
        Status = Rk3I2cTransferData(Controller, Transfer, InterruptBits);
        if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
            goto InterruptServiceWorkerEnd;
        }
    }

    //
    // If this was the last transfer, send the stop.
    //

    if ((Transfer->Flags & SPB_TRANSFER_FLAG_LAST) != 0) {
        Rk3I2cSendStop(Controller);
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
        Rk3I2cSetupTransfer(Controller, Transfer);

    } else {
        Controller->InterruptMask = 0;
        RK3_WRITE_I2C(Controller, Rk32I2cInterruptEnable, 0);
    }

InterruptServiceWorkerEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Rk3I2cProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an RK3xxx I2C controller. It adds an interrupt vector requirement
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
Rk3I2cStartDevice (
    PIRP Irp,
    PRK3_I2C_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the RK3xxx I2C device.

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
                      &Rk3I2cFunctionTableTemplate,
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
        Connect.InterruptServiceRoutine = Rk3I2cInterruptService;
        Connect.LowLevelServiceRoutine = Rk3I2cInterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Status = Rk3I2cInitializeController(Device);
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
Rk3I2cInitializeController (
    PRK3_I2C_CONTROLLER Controller
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

    RK3_WRITE_I2C(Controller, Rk32I2cInterruptEnable, 0);
    RK3_WRITE_I2C(Controller, Rk32I2cControl, 0);
    return STATUS_SUCCESS;
}

KSTATUS
Rk3I2cConfigureBus (
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

    ULONG Address;
    PRK3_I2C_CONTROLLER Controller;
    PRESOURCE_SPB_I2C I2c;

    Controller = Context;
    if (Configuration->BusType != ResourceSpbBusI2c) {
        return STATUS_INVALID_PARAMETER;
    }

    I2c = PARENT_STRUCTURE(Configuration, RESOURCE_SPB_I2C, Header);
    if ((Configuration->Flags & RESOURCE_SPB_DATA_SLAVE) != 0) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Currently 10 bit addressing is not supported in this controller because
    // the device address is transmitted directly, which only has byte
    // granularity.
    //

    if ((I2c->Flags & RESOURCE_SPB_I2C_10_BIT_ADDRESSING) != 0) {
        return STATUS_NOT_SUPPORTED;
    }

    KeAcquireQueuedLock(Controller->Lock);
    Address = ((I2c->SlaveAddress <<
                RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_SHIFT) &
               RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_MASK) |
              RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_LOW_BYTE_VALID;

    if ((I2c->Flags & RESOURCE_SPB_I2C_10_BIT_ADDRESSING) != 0) {
        Address |= RK32_I2C_MASTER_RECEIVE_SLAVE_ADDRESS_MIDDLE_BYTE_VALID;
    }

    RK3_WRITE_I2C(Controller, Rk32I2cMasterReceiveSlaveAddress, Address);
    Controller->SlaveAddress = I2c->SlaveAddress;
    KeReleaseQueuedLock(Controller->Lock);
    return STATUS_SUCCESS;
}

KSTATUS
Rk3I2cSubmitTransfer (
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

    PRK3_I2C_CONTROLLER Controller;
    KSTATUS Status;

    Controller = Context;
    KeAcquireQueuedLock(Controller->Lock);
    Status = Rk3I2cSetupTransfer(Controller, Transfer);
    if (!KSUCCESS(Status)) {
        goto SubmitTransferEnd;
    }

SubmitTransferEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return Status;
}

KSTATUS
Rk3I2cSetupTransfer (
    PRK3_I2C_CONTROLLER Controller,
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

    ULONG Control;
    ULONG Mask;

    ASSERT(Controller->Transfer == NULL);

    Controller->Transfer = Transfer;
    Controller->TransferDirection = Transfer->Direction;
    Transfer->ReceiveSizeCompleted = 0;
    Transfer->TransmitSizeCompleted = 0;
    Mask = RK32_I2C_INTERRUPT_NAK | RK32_I2C_INTERRUPT_START |
           RK32_I2C_INTERRUPT_MASTER_TRANSMIT_FINISHED |
           RK32_I2C_INTERRUPT_MASTER_RECEIVE_FINISHED;

    //
    // Clear any old interrupts.
    //

    RK3_WRITE_I2C(Controller, Rk32I2cControl, 0);
    RK3_WRITE_I2C(Controller, Rk32I2cInterruptEnable, 0);
    RK3_WRITE_I2C(Controller, Rk32I2cInterruptPending, 0);
    Control = RK32_I2C_CONTROL_START | RK32_I2C_CONTROL_ENABLE |
              RK32_I2C_CONTROL_STOP_ON_NAK | RK32_I2C_CONTROL_MODE_TRANSMIT;

    Controller->Control = Control;
    RK3_WRITE_I2C(Controller, Rk32I2cControl, Control);

    //
    // Delay if needed.
    //

    if (Transfer->MicrosecondDelay != 0) {
        KeDelayExecution(FALSE, FALSE, Transfer->MicrosecondDelay);
    }

    //
    // Enable the interrupts.
    //

    Controller->InterruptMask = Mask;
    RK3_WRITE_I2C(Controller, Rk32I2cInterruptEnable, Mask);
    return STATUS_SUCCESS;
}

KSTATUS
Rk3I2cTransferData (
    PRK3_I2C_CONTROLLER Controller,
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

    ULONG Buffer[RK32_I2C_BUFFER_SIZE / sizeof(ULONG)];
    ULONG Control;
    SPB_TRANSFER_DIRECTION Direction;
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
        if ((InterruptStatus &
             RK32_I2C_INTERRUPT_MASTER_TRANSMIT_FINISHED) != 0) {

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

            for (Index = 0; Index < Size; Index += sizeof(ULONG)) {
                RK3_WRITE_I2C(Controller,
                              Rk32I2cTransmitData0 + Index,
                              Buffer[Index / sizeof(ULONG)]);
            }

            Transfer->TransmitSizeCompleted += Size;

            //
            // Kick off the next write.
            //

            RK3_WRITE_I2C(Controller, Rk32I2cMasterTransmitCount, Size);

        //
        // No unexpected interrupts should be coming in.
        //

        } else {

            ASSERT(FALSE);
        }

    } else {

        ASSERT(Direction == SpbTransferDirectionIn);

        //
        // If some actual receive data came in, grab it.
        //

        if ((InterruptStatus &
             RK32_I2C_INTERRUPT_MASTER_RECEIVE_FINISHED) != 0) {

            if (Size > Transfer->Size - Transfer->ReceiveSizeCompleted) {
                Size = Transfer->Size - Transfer->ReceiveSizeCompleted;
            }

            for (Index = 0; Index < Size; Index += sizeof(ULONG)) {
                Buffer[Index / sizeof(ULONG)] =
                         RK3_READ_I2C(Controller, Rk32I2cReceiveData0 + Index);
            }

            Offset = Transfer->Offset + Transfer->ReceiveSizeCompleted;
            Status = MmCopyIoBufferData(Transfer->IoBuffer,
                                        Buffer,
                                        Offset,
                                        Size,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                TransferDone = TRUE;
                goto TransferDataEnd;
            }

            Transfer->ReceiveSizeCompleted += Size;
            if (Transfer->ReceiveSizeCompleted >= Transfer->Size) {
                TransferDone = TRUE;
                goto TransferDataEnd;
            }

        //
        // Switch into receive mode if the device address was just transmitted.
        //

        } else if ((InterruptStatus &
                    RK32_I2C_INTERRUPT_MASTER_TRANSMIT_FINISHED) != 0) {

            Control = RK32_I2C_CONTROL_ENABLE |
                      RK32_I2C_CONTROL_STOP_ON_NAK |
                      RK32_I2C_CONTROL_MODE_RECEIVE;

            Controller->Control = Control;
            RK3_WRITE_I2C(Controller, Rk32I2cControl, Control);
        }

        //
        // If the initial transmit of the device address finished or a previous
        // receive finished, ask for more.
        //

        if ((InterruptStatus &
             (RK32_I2C_INTERRUPT_MASTER_TRANSMIT_FINISHED |
              RK32_I2C_INTERRUPT_MASTER_RECEIVE_FINISHED)) != 0) {

            //
            // Now ask the controller to go get the next batch of bytes.
            //

            Size = sizeof(Buffer);
            if (Size > Transfer->Size - Transfer->ReceiveSizeCompleted) {
                Size = Transfer->Size - Transfer->ReceiveSizeCompleted;
            }

            //
            // If this is the last set of bytes, end it with a nak instaed of
            // an ack.
            //

            if (Transfer->ReceiveSizeCompleted + Size >= Transfer->Size) {
                Controller->Control |= RK32_I2C_CONTROL_SEND_NAK;
                RK3_WRITE_I2C(Controller, Rk32I2cControl, Controller->Control);
            }

            RK3_WRITE_I2C(Controller, Rk32I2cMasterReceiveCount, Size);

        //
        // No unexpected interrupts should be coming in.
        //

        } else {

            ASSERT(FALSE);
        }
    }

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
Rk3I2cSendStop (
    PRK3_I2C_CONTROLLER Controller
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

    ULONG Control;
    ULONG Status;

    Control = Controller->Control;
    Control |= RK32_I2C_CONTROL_STOP | RK32_I2C_CONTROL_ENABLE;
    RK3_WRITE_I2C(Controller, Rk32I2cControl, Control);
    do {
        Status = RK3_READ_I2C(Controller, Rk32I2cInterruptPending);

    } while ((Status & RK32_I2C_INTERRUPT_STOP) == 0);

    RK3_WRITE_I2C(Controller, Rk32I2cInterruptPending, RK32_I2C_INTERRUPT_STOP);
    Controller->Control = 0;
    RK3_WRITE_I2C(Controller, Rk32I2cControl, 0);
    return;
}

