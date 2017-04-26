/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am3i2c.c

Abstract:

    This module implements support for the TI AM335x I2C controller driver.

Author:

    Evan Green 7-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/spb/spbhost.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/soc/am335x.h>

//
// --------------------------------------------------------------------- Macros
//

#define AM3_READ_I2C(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define AM3_WRITE_I2C(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define AM335_I2C_ALLOCATION_TAG 0x32493341

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an AM335x I2C controller.

Members:

    OsDevice - Stores a pointer to the OS device object.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    ControllerBase - Stores the virtual address of the memory mapping to the
        I2C controller registers.

    SpbController - Stores a pointer to the library Simple Peripheral Bus
        controller.

    Transfer - Stores the current transfer being worked on.

    PendingInterrupts - Stores a bitfield of pending interrupts.

    Control - Stores a shadow copy of the control register.

    Lock - Stores a pointer to a lock serializing access to the controller.

    FifoDepth - Stores the depth of the transmit and receive FIFOs.

    FifoThreshold - Stores the FIFO threshold value that causes TX/RX ready
        interrupts to fire.

--*/

typedef struct _AM3_I2C_CONTROLLER {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PSPB_CONTROLLER SpbController;
    PSPB_TRANSFER Transfer;
    ULONG PendingInterrupts;
    ULONG Control;
    PQUEUED_LOCK Lock;
    ULONG FifoDepth;
    ULONG FifoThreshold;
} AM3_I2C_CONTROLLER, *PAM3_I2C_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Am3I2cAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Am3I2cDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3I2cDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3I2cDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3I2cDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Am3I2cDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
Am3I2cInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
Am3I2cInterruptServiceWorker (
    PVOID Context
    );

KSTATUS
Am3I2cProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Am3I2cStartDevice (
    PIRP Irp,
    PAM3_I2C_CONTROLLER Device
    );

KSTATUS
Am3I2cInitializeController (
    PAM3_I2C_CONTROLLER Controller
    );

KSTATUS
Am3I2cConfigureBus (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    );

KSTATUS
Am3I2cSubmitTransfer (
    PVOID Context,
    PSPB_TRANSFER Transfer
    );

KSTATUS
Am3I2cSetupTransfer (
    PAM3_I2C_CONTROLLER Controller,
    PSPB_TRANSFER Transfer
    );

KSTATUS
Am3I2cTransferData (
    PAM3_I2C_CONTROLLER Controller,
    PSPB_TRANSFER Transfer,
    ULONG InterruptStatus
    );

VOID
Am3I2cEnableController (
    PAM3_I2C_CONTROLLER Controller,
    BOOL Enable
    );

VOID
Am3I2cSendStop (
    PAM3_I2C_CONTROLLER Controller
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Am3I2cDriver;

SPB_FUNCTION_TABLE Am3I2cFunctionTableTemplate = {
    Am3I2cConfigureBus,
    Am3I2cSubmitTransfer,
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

    This routine is the entry point for the AM335x I2C driver. It registers
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

    Am3I2cDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Am3I2cAddDevice;
    FunctionTable.DispatchStateChange = Am3I2cDispatchStateChange;
    FunctionTable.DispatchOpen = Am3I2cDispatchOpen;
    FunctionTable.DispatchClose = Am3I2cDispatchClose;
    FunctionTable.DispatchIo = Am3I2cDispatchIo;
    FunctionTable.DispatchSystemControl = Am3I2cDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Am3I2cAddDevice (
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

    PAM3_I2C_CONTROLLER Controller;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(AM3_I2C_CONTROLLER),
                                        AM335_I2C_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(AM3_I2C_CONTROLLER));
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
Am3I2cDispatchStateChange (
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
            Status = Am3I2cProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Am3I2cDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Am3I2cStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Am3I2cDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Am3I2cDispatchOpen (
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
Am3I2cDispatchClose (
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
Am3I2cDispatchIo (
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
Am3I2cDispatchSystemControl (
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
Am3I2cInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the interrupt service routine for the AM335x I2C
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

    PAM3_I2C_CONTROLLER Controller;
    ULONG ReadyStatus;
    ULONG Status;

    Controller = Context;
    Status = AM3_READ_I2C(Controller, Am3I2cInterruptStatus);
    if (Status != 0) {

        //
        // Disable receive interrupts since they would just keep firing
        // until the FIFO was handled. The transfer data function will always
        // re-enable them if needed.
        //

        ReadyStatus = Status & (AM335_I2C_INTERRUPT_RX_READY |
                                AM335_I2C_INTERRUPT_RX_DRAIN);

        if (ReadyStatus != 0) {
            AM3_WRITE_I2C(Controller, Am3I2cInterruptEnableClear, ReadyStatus);
        }

        AM3_WRITE_I2C(Controller, Am3I2cInterruptStatus, Status);
        RtlAtomicOr32(&(Controller->PendingInterrupts), Status);
        return InterruptStatusClaimed;
    }

    return InterruptStatusNotClaimed;
}

INTERRUPT_STATUS
Am3I2cInterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the low level interrupt service routine for the
    AM335x I2c controller.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PAM3_I2C_CONTROLLER Controller;
    ULONG InterruptBits;
    KSTATUS Status;
    PSPB_TRANSFER Transfer;

    Controller = Context;
    InterruptBits = RtlAtomicExchange32(&(Controller->PendingInterrupts), 0);
    if (InterruptBits == 0) {
        return InterruptStatusNotClaimed;
    }

    KeAcquireQueuedLock(Controller->Lock);
    Transfer = Controller->Transfer;
    if (Transfer == NULL) {
        goto InterruptServiceWorkerEnd;
    }

    //
    // If an error came in, fail the current transfer.
    //

    if ((InterruptBits & AM335_I2C_INTERRUPT_ERROR_MASK) != 0) {
        RtlDebugPrint("AM3 I2C: Error 0x%08x\n", InterruptBits);
        Status = STATUS_DEVICE_IO_ERROR;

    //
    // Transfer more data. If the transfer fills the FIFOs, then
    // break out and wait for the interrupt to fire to put more data in.
    //

    } else {
        Status = Am3I2cTransferData(Controller, Transfer, InterruptBits);
        if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
            goto InterruptServiceWorkerEnd;
        }
    }

    //
    // If this was the last transfer, send the stop.
    //

    if ((Transfer->Flags & SPB_TRANSFER_FLAG_LAST) != 0) {
        Am3I2cSendStop(Controller);
    }

    //
    // The transfer completed entirely, so complete this one and go get a
    // new one.
    //

    Transfer = SpbTransferCompletion(Controller->SpbController,
                                     Transfer,
                                     Status);

    if (Transfer != NULL) {
        Am3I2cSetupTransfer(Controller, Transfer);
    }

InterruptServiceWorkerEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Am3I2cProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an AM335x I2C controller. It adds an interrupt vector requirement
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
Am3I2cStartDevice (
    PIRP Irp,
    PAM3_I2C_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the AM335x I2C device.

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
        Registration.MaxFrequency = AM335_I2C_INTERNAL_CLOCK_SPEED;
        Registration.BusType = ResourceSpbBusI2c;
        RtlCopyMemory(&(Registration.FunctionTable),
                      &Am3I2cFunctionTableTemplate,
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
        Connect.InterruptServiceRoutine = Am3I2cInterruptService;
        Connect.LowLevelServiceRoutine = Am3I2cInterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    Status = Am3I2cInitializeController(Device);
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
Am3I2cInitializeController (
    PAM3_I2C_CONTROLLER Controller
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

    ULONG BufferStatus;
    ULONG Prescaler;
    ULONG Value;

    //
    // Disable the I2C controller.
    //

    Value = AM3_READ_I2C(Controller, Am3I2cControl);
    Value &= ~AM335_I2C_CONTROL_ENABLE;
    AM3_WRITE_I2C(Controller, Am3I2cControl, Value);

    //
    // Reset the controller.
    //

    Value = AM3_READ_I2C(Controller, Am3I2cSysControl);
    Value |= AM335_I2C_SYSTEM_CONTROL_SOFT_RESET;
    AM3_WRITE_I2C(Controller, Am3I2cSysControl, Value);
    do {
        Value = AM3_READ_I2C(Controller, Am3I2cSysControl);

    } while ((Value & AM335_I2C_SYSTEM_CONTROL_SOFT_RESET) != 0);

    //
    // Enable auto idle.
    //

    Value &= ~AM335_I2C_SYSTEM_CONTROL_AUTO_IDLE;
    AM3_WRITE_I2C(Controller, Am3I2cSysControl, Value);

    //
    // Compute the prescaler value.
    //

    Prescaler = (AM335_I2C_SYSTEM_CLOCK_SPEED /
                 AM335_I2C_INTERNAL_CLOCK_SPEED) - 1;

    AM3_WRITE_I2C(Controller, Am3I2cPrescale, Prescaler);

    //
    // Figure out the FIFO size.
    //

    BufferStatus = AM3_READ_I2C(Controller, Am3I2cBufferStatus);
    switch (BufferStatus & AM335_I2C_BUFFER_STATUS_DEPTH_MASK) {
    case AM335_I2C_BUFFER_STATUS_DEPTH_8:
        Controller->FifoDepth = 8;
        break;

    case AM335_I2C_BUFFER_STATUS_DEPTH_16:
        Controller->FifoDepth = 16;
        break;

    case AM335_I2C_BUFFER_STATUS_DEPTH_32:
        Controller->FifoDepth = 32;
        break;

    case AM335_I2C_BUFFER_STATUS_DEPTH_64:
        Controller->FifoDepth = 64;
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    //
    // Disable all interrupts.
    //

    AM3_WRITE_I2C(Controller, Am3I2cInterruptEnableClear, 0xFFFFFFFF);
    return STATUS_SUCCESS;
}

KSTATUS
Am3I2cConfigureBus (
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
    ULONG BitTime;
    ULONG Control;
    PAM3_I2C_CONTROLLER Controller;
    PRESOURCE_SPB_I2C I2c;
    ULONG Threshold;
    ULONG Value;

    Controller = Context;
    if (Configuration->BusType != ResourceSpbBusI2c) {
        return STATUS_INVALID_PARAMETER;
    }

    I2c = PARENT_STRUCTURE(Configuration, RESOURCE_SPB_I2C, Header);
    Control = 0;
    if ((I2c->Flags & RESOURCE_SPB_I2C_10_BIT_ADDRESSING) != 0) {
        Control |= AM335_I2C_CONTROL_EXPAND_SLAVE_ADDRESS |
                   AM335_I2C_CONTROL_EXPAND_OWN_ADDRESS_0 |
                   AM335_I2C_CONTROL_EXPAND_OWN_ADDRESS_1 |
                   AM335_I2C_CONTROL_EXPAND_OWN_ADDRESS_2 |
                   AM335_I2C_CONTROL_EXPAND_OWN_ADDRESS_3;
    }

    KeAcquireQueuedLock(Controller->Lock);

    //
    // The controller must be disabled while reconfiguring.
    //

    Am3I2cEnableController(Controller, FALSE);
    Address = I2c->SlaveAddress;
    if ((I2c->Header.Flags & RESOURCE_SPB_DATA_SLAVE) == 0) {
        Control |= AM335_I2C_CONTROL_MASTER;
        AM3_WRITE_I2C(Controller, Am3I2cSlaveAddress, Address);

    } else {
        AM3_WRITE_I2C(Controller, Am3I2cOwnAddress, Address);
    }

    AM3_WRITE_I2C(Controller, Am3I2cControl, Control);
    Controller->Control = Control;

    //
    // Set the FIFO thresholds and clear the FIFO as well.
    //

    Threshold = Controller->FifoDepth / 2;;
    Controller->FifoThreshold = Threshold;
    Value = ((Threshold - 1) << AM335_I2C_BUFFER_RX_THRESHOLD_SHIFT) |
            ((Threshold - 1) << AM335_I2C_BUFFER_TX_THRESHOLD_SHIFT) |
            AM335_I2C_BUFFER_RX_FIFO_CLEAR |
            AM335_I2C_BUFFER_TX_FIFO_CLEAR;

    AM3_WRITE_I2C(Controller, Am3I2cBuffer, Value);

    //
    // Configure the low and high bit times.
    //

    BitTime = (AM335_I2C_INTERNAL_CLOCK_SPEED / I2c->Speed) / 2;
    AM3_WRITE_I2C(Controller, Am3I2cSclLowTime, BitTime - 7);
    AM3_WRITE_I2C(Controller, Am3I2cSclHighTime, BitTime - 5);
    Am3I2cEnableController(Controller, TRUE);
    KeReleaseQueuedLock(Controller->Lock);
    return STATUS_SUCCESS;
}

KSTATUS
Am3I2cSubmitTransfer (
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

    PAM3_I2C_CONTROLLER Controller;
    KSTATUS Status;

    Controller = Context;
    KeAcquireQueuedLock(Controller->Lock);
    Status = Am3I2cSetupTransfer(Controller, Transfer);
    if (!KSUCCESS(Status)) {
        goto SubmitTransferEnd;
    }

SubmitTransferEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return Status;
}

KSTATUS
Am3I2cSetupTransfer (
    PAM3_I2C_CONTROLLER Controller,
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
    ULONG RawStatus;
    ULONG Size;

    Transfer->ReceiveSizeCompleted = 0;
    Transfer->TransmitSizeCompleted = 0;
    Am3I2cEnableController(Controller, FALSE);
    Mask = AM335_I2C_INTERRUPT_DEFAULT_MASK;

    //
    // Set up the transfer direction.
    //

    Control = Controller->Control;
    Control &= ~AM335_I2C_CONTROL_TRANSMIT;
    switch (Transfer->Direction) {
    case SpbTransferDirectionIn:
        Mask |= AM335_I2C_INTERRUPT_RX_READY |
                AM335_I2C_INTERRUPT_RX_DRAIN;

        break;

    case SpbTransferDirectionOut:
        Mask |= AM335_I2C_INTERRUPT_TX_READY |
                AM335_I2C_INTERRUPT_TX_DRAIN;

        Control |= AM335_I2C_CONTROL_TRANSMIT;
        break;

    default:

        ASSERT(FALSE);

        return STATUS_INVALID_PARAMETER;
    }

    AM3_WRITE_I2C(Controller, Am3I2cControl, Control);
    Controller->Control = Control;
    Size = Transfer->Size;
    if (Size == 0x10000) {
        Size = 0;

    } else if ((Size == 0) || (Size > 0x10000)) {
        return STATUS_INVALID_PARAMETER;
    }

    AM3_WRITE_I2C(Controller, Am3I2cCount, Size & 0x0000FFFF);

    ASSERT(Controller->Transfer == NULL);

    //
    // Clear any old interrupts.
    //

    AM3_WRITE_I2C(Controller, Am3I2cInterruptStatus, 0xFFFFFFFF);
    Controller->Transfer = Transfer;
    Am3I2cEnableController(Controller, TRUE);

    //
    // Send the start.
    //

    Control = AM3_READ_I2C(Controller, Am3I2cControl);
    Control |= AM335_I2C_CONTROL_START;
    AM3_WRITE_I2C(Controller, Am3I2cControl, Control);
    do {
        RawStatus = AM3_READ_I2C(Controller, Am3I2cInterruptStatusRaw);

    } while ((RawStatus & AM335_I2C_INTERRUPT_BUS_BUSY) == 0);

    //
    // Delay if needed.
    //

    if (Transfer->MicrosecondDelay != 0) {
        KeDelayExecution(FALSE, FALSE, Transfer->MicrosecondDelay);
    }

    //
    // Enable the interrupts.
    //

    Mask |= AM335_I2C_INTERRUPT_ACCESS_READY | AM335_I2C_INTERRUPT_ACCESS_ERROR;
    AM3_WRITE_I2C(Controller, Am3I2cInterruptEnableSet, Mask);
    return STATUS_SUCCESS;
}

KSTATUS
Am3I2cTransferData (
    PAM3_I2C_CONTROLLER Controller,
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

    UCHAR Buffer[AM335_I2C_MAX_FIFO_DEPTH];
    ULONG BufferStatus;
    SPB_TRANSFER_DIRECTION Direction;
    ULONG Index;
    ULONG Mask;
    UINTN Offset;
    ULONG Size;
    KSTATUS Status;
    BOOL TransferDone;

    Direction = Transfer->Direction;
    TransferDone = FALSE;
    Status = STATUS_SUCCESS;

    //
    // Send some data if needed.
    //

    if (Direction == SpbTransferDirectionOut) {

        //
        // If the TX ready interrupt is set, then it's known how many bytes are
        // free in the buffer. Otherwise if the drain interrupt is set, find
        // out how many remaining bytes to fill.
        //

        if ((InterruptStatus & AM335_I2C_INTERRUPT_TX_DRAIN) != 0) {
            Size = AM3_READ_I2C(Controller, Am3I2cBufferStatus);
            Size = (Size & AM335_I2C_BUFFER_STATUS_TX_MASK) >>
                   AM335_I2C_BUFFER_STATUS_TX_SHIFT;

        } else if ((InterruptStatus & AM335_I2C_INTERRUPT_TX_READY) != 0) {
            Size = Controller->FifoDepth - Controller->FifoThreshold;

        } else {

            //
            // If an access ready interrupt occurred, the transfer is probably
            // done.
            //

            if ((InterruptStatus & AM335_I2C_INTERRUPT_ACCESS_READY) != 0) {
                if (Transfer->TransmitSizeCompleted == Transfer->Size) {
                    TransferDone = TRUE;
                    goto TransferDataEnd;
                }
            }

            goto TransferDataEnd;
        }

        ASSERT(Size <= AM335_I2C_MAX_FIFO_DEPTH);

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

        for (Index = 0; Index < Size; Index += 1) {
            AM3_WRITE_I2C(Controller,
                          Am3I2cData,
                          Buffer[Transfer->TransmitSizeCompleted + Index]);
        }

        Transfer->TransmitSizeCompleted += Size;

    //
    // Receive some data.
    //

    } else {

        ASSERT(Direction == SpbTransferDirectionIn);

        BufferStatus = AM3_READ_I2C(Controller, Am3I2cBufferStatus);
        Size = (BufferStatus & AM335_I2C_BUFFER_STATUS_RX_MASK) >>
               AM335_I2C_BUFFER_STATUS_RX_SHIFT;

        ASSERT(Size < AM335_I2C_MAX_FIFO_DEPTH);

        if (Size > Transfer->Size - Transfer->ReceiveSizeCompleted) {
            Size = Transfer->Size - Transfer->ReceiveSizeCompleted;
        }

        for (Index = 0; Index < Size; Index += 1) {
            Buffer[Index] = AM3_READ_I2C(Controller, Am3I2cData);
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

            //
            // If all the data has been transferred and the access ready
            // interrupt occurred, then the transfer is complete.
            //

            if ((InterruptStatus & AM335_I2C_INTERRUPT_ACCESS_READY) != 0) {
                TransferDone = TRUE;
            }

            goto TransferDataEnd;
        }

        //
        // There are more bytes to receive, so clear and enable the RX ready
        // and RX drain interrupts.
        //

        Mask = AM335_I2C_INTERRUPT_RX_READY |
               AM335_I2C_INTERRUPT_RX_DRAIN;

        AM3_WRITE_I2C(Controller, Am3I2cInterruptStatus, Mask);
        AM3_WRITE_I2C(Controller, Am3I2cInterruptEnableSet, Mask);
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
Am3I2cEnableController (
    PAM3_I2C_CONTROLLER Controller,
    BOOL Enable
    )

/*++

Routine Description:

    This routine makes sure that the I2C controller is enabled and active.

Arguments:

    Controller - Supplies a pointer to the controller.

    Enable - Supplies a boolean indicating whether to enable (TRUE) or disable
        (FALSE) I2C bus access.

Return Value:

    None.

--*/

{

    ULONG Value;

    Value = AM3_READ_I2C(Controller, Am3I2cControl);
    Value &= ~AM335_I2C_CONTROL_ENABLE;
    if (Enable != FALSE) {
        Value |= AM335_I2C_CONTROL_ENABLE;
    }

    AM3_WRITE_I2C(Controller, Am3I2cControl, Value);
    if (Enable == FALSE) {
        AM3_WRITE_I2C(Controller, Am3I2cInterruptEnableClear, 0xFFFFFFFF);
        AM3_WRITE_I2C(Controller, Am3I2cInterruptStatus, 0xFFFFFFFF);
    }

    return;
}

VOID
Am3I2cSendStop (
    PAM3_I2C_CONTROLLER Controller
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
    ULONG RawStatus;

    Control = AM3_READ_I2C(Controller, Am3I2cControl);

    //
    // The master sends the stop. If this is slave, do nothing.
    //

    if ((Control & AM335_I2C_CONTROL_MASTER) == 0) {
        return;
    }

    Control |= AM335_I2C_CONTROL_STOP;
    AM3_WRITE_I2C(Controller, Am3I2cControl, Control);
    do {
        RawStatus = AM3_READ_I2C(Controller, Am3I2cInterruptStatusRaw);

    } while ((RawStatus & AM335_I2C_INTERRUPT_BUS_FREE) == 0);

    return;
}

