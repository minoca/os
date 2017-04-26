/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rk32spi.c

Abstract:

    This module implements support for the SPI controllers on the RockChip
    RK3288 SoC.

Author:

    Evan Green 24-Aug-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/spb/spbhost.h>

//
// --------------------------------------------------------------------- Macros
//

#define RK32_READ_SPI(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define RK32_WRITE_SPI(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define RK32_SPI_ALLOCATION_TAG 0x53336B52
#define RK32_SPI_INPUT_CLOCK 99000000
#define RK32_SPI_FIFO_DEPTH 32

//
// Define control register 0 bits.
//

#define RK32_SPI_CONTROL0_DATA_FRAME_4 (0x0 << 0)
#define RK32_SPI_CONTROL0_DATA_FRAME_8 (0x1 << 0)
#define RK32_SPI_CONTROL0_DATA_FRAME_16 (0x2 << 0)
#define RK32_SPI_CONTROL0_CONTROL_FRAME_SIZE_MASK (0xF << 2)
#define RK32_SPI_CONTROL0_CLOCK_PHASE (1 << 6)
#define RK32_SPI_CONTROL0_CLOCK_INACTIVE_HIGH (1 << 7)
#define RK32_SPI_CONTROL0_CHIP_SELECT_KEEP_LOW (0x0 << 8)
#define RK32_SPI_CONTROL0_CHIP_SELECT_HIGH_HALF (0x1 << 8)
#define RK32_SPI_CONTROL0_CHIP_SELECT_HIGH_FULL (0x2 << 8)
#define RK32_SPI_CONTROL0_SS_CLK_DELAY_FULL_CLOCK (1 << 10)
#define RK32_SPI_CONTROL0_BIG_ENDIAN (1 << 11)
#define RK32_SPI_CONTROL0_LSB_FIRST (1 << 12)
#define RK32_SPI_CONTROL0_APB_8BIT (1 << 13)
#define RK32_SPI_CONTROL0_DELAY_CYCLE_SHIFT 14
#define RK32_SPI_CONTROL0_FRAME_MOTOROLA (0x0 << 16)
#define RK32_SPI_CONTROL0_FRAME_TI_SSP (0x1 << 16)
#define RK32_SPI_CONTROL0_FRAME_NS_MICROWIRE (0x2 << 16)
#define RK32_SPI_CONTROL0_TRANSMIT_AND_RECEIVE (0x0 << 18)
#define RK32_SPI_CONTROL0_TRANSMIT_ONLY (0x1 << 18)
#define RK32_SPI_CONTROL0_RECEIVE_ONLY (0x2 << 18)
#define RK32_SPI_CONTROL0_TRANSCEIVE_MASK (0x3 << 18)
#define RK32_SPI_CONTROL0_SLAVE_MODE (1 << 20)
#define RK32_SPI_CONTROL0_MICROWIRE_SEQUENTIAL (1 << 21)

//
// Define SPI enable register bits.
//

#define RK32_SPI_ENABLE (1 << 0)

//
// Define SPI status register bits.
//

#define RK32_SPI_STATUS_SPI_BUSY (1 << 0)
#define RK32_SPI_STATUS_TX_FIFO_FULL (1 << 1)
#define RK32_SPI_STATUS_TX_FIFO_EMPTY (1 << 2)
#define RK32_SPI_STATUS_RX_FIFO_EMPTY (1 << 3)
#define RK32_SPI_STATUS_RX_FIFO_FULL (1 << 4)

//
// Define SPI interrupt polarity bits.
//

#define RK32_SPI_INTERRUPT_POLARITY_LOW (1 << 0)

//
// Define SPI interrupt register bits.
//

#define RK32_SPI_INTERRUPT_TX_EMPTY (1 << 0)
#define RK32_SPI_INTERRUPT_TX_OVERFLOW (1 << 1)
#define RK32_SPI_INTERRUPT_RX_UNDERFLOW (1 << 2)
#define RK32_SPI_INTERRUPT_RX_OVERFLOW (1 << 3)
#define RK32_SPI_INTERRUPT_RX_FULL (1 << 4)
#define RK32_SPI_INTERRUPT_MASK                                             \
    (RK32_SPI_INTERRUPT_TX_EMPTY | RK32_SPI_INTERRUPT_TX_OVERFLOW |         \
     RK32_SPI_INTERRUPT_RX_UNDERFLOW | RK32_SPI_INTERRUPT_RX_OVERFLOW |     \
     RK32_SPI_INTERRUPT_RX_FULL)

#define RK32_SPI_INTERRUPT_ERROR_MASK                                       \
    (RK32_SPI_INTERRUPT_TX_OVERFLOW | RK32_SPI_INTERRUPT_RX_UNDERFLOW |     \
     RK32_SPI_INTERRUPT_RX_OVERFLOW)

#define RK32_SPI_INTERRUPT_DEFAULT_MASK                                     \
    (RK32_SPI_INTERRUPT_TX_OVERFLOW | RK32_SPI_INTERRUPT_RX_UNDERFLOW |     \
     RK32_SPI_INTERRUPT_RX_OVERFLOW | RK32_SPI_INTERRUPT_RX_FULL)

//
// Define DMA control register bits.
//

#define RK32_SPI_DMA_RX_ENABLE (1 << 0)
#define RK32_SPI_DMA_TX_ENABLE (1 << 1)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RK32_SPI_REGISTER {
    Rk32SpiControl0 = 0x00,
    Rk32SpiControl1 = 0x04,
    Rk32SpiEnable = 0x08,
    Rk32SpiSlaveEnable = 0x0C,
    Rk32SpiBaudRateSelect = 0x10,
    Rk32SpiTxFifoThreshold = 0x14,
    Rk32SpiRxFifoThreshold = 0x18,
    Rk32SpiTxFifoLevel = 0x1C,
    Rk32SpiRxFifoLevel = 0x20,
    Rk32SpiSpiStatus = 0x24,
    Rk32SpiInterruptPolarity = 0x28,
    Rk32SpiInterruptMask = 0x2C,
    Rk32SpiInterruptStatus = 0x30,
    Rk32SpiRawInterruptStatus = 0x34,
    Rk32SpiInterruptClear = 0x38,
    Rk32SpiDmaControl = 0x3C,
    Rk32SpiDmaTxDataLevel = 0x40,
    Rk32SpiDmaRxDataLevel = 0x44,
    Rk32SpiTxFifoData = 0x400,
    Rk32SpiRxFifoData = 0x800
} RK32_SPI_REGISTER, *PRK32_SPI_REGISTER;

/*++

Structure Description:

    This structure defines the context for an RK32 SPI controller.

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
        SPI controller registers.

    SpbController - Stores a pointer to the library Simple Peripheral Bus
        controller.

    Control - Stores a shadow copy of the current control 0 register.

    Transfer - Stores the current transfer being worked on.

    PendingInterrupts - Stores a bitfield of pending interrupts.

    InterruptMask - Stores a shadow copy of the current interrupt mask.

    Lock - Stores a pointer to a lock serializing access to the controller.

--*/

typedef struct _RK32_SPI_CONTROLLER {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PSPB_CONTROLLER SpbController;
    ULONG Control;
    PSPB_TRANSFER Transfer;
    ULONG PendingInterrupts;
    ULONG InterruptMask;
    PQUEUED_LOCK Lock;
} RK32_SPI_CONTROLLER, *PRK32_SPI_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Rk32SpiAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Rk32SpiDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk32SpiDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk32SpiDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk32SpiDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Rk32SpiDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
Rk32SpiInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
Rk32SpiInterruptServiceWorker (
    PVOID Context
    );

KSTATUS
Rk32SpiProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
Rk32SpiStartDevice (
    PIRP Irp,
    PRK32_SPI_CONTROLLER Device
    );

KSTATUS
Rk32SpiConfigureBus (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    );

KSTATUS
Rk32SpiSubmitTransfer (
    PVOID Context,
    PSPB_TRANSFER Transfer
    );

VOID
Rk32SpiLockBus (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    );

VOID
Rk32SpiUnlockBus (
    PVOID Context
    );

KSTATUS
Rk32SpiSetupTransfer (
    PRK32_SPI_CONTROLLER Controller,
    PSPB_TRANSFER Transfer
    );

KSTATUS
Rk32SpiTransferData (
    PRK32_SPI_CONTROLLER Controller,
    PSPB_TRANSFER Transfer
    );

VOID
Rk32SpiEnableController (
    PRK32_SPI_CONTROLLER Controller,
    BOOL Enable
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Rk32SpiDriver;

SPB_FUNCTION_TABLE Rk32SpiFunctionTableTemplate = {
    Rk32SpiConfigureBus,
    Rk32SpiSubmitTransfer,
    Rk32SpiLockBus,
    Rk32SpiUnlockBus
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

    This routine is the entry point for the RK32 SPI driver. It registers
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

    Rk32SpiDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Rk32SpiAddDevice;
    FunctionTable.DispatchStateChange = Rk32SpiDispatchStateChange;
    FunctionTable.DispatchOpen = Rk32SpiDispatchOpen;
    FunctionTable.DispatchClose = Rk32SpiDispatchClose;
    FunctionTable.DispatchIo = Rk32SpiDispatchIo;
    FunctionTable.DispatchSystemControl = Rk32SpiDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Rk32SpiAddDevice (
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

    PRK32_SPI_CONTROLLER Controller;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(RK32_SPI_CONTROLLER),
                                        RK32_SPI_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(RK32_SPI_CONTROLLER));
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
Rk32SpiDispatchStateChange (
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
            Status = Rk32SpiProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Rk32SpiDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = Rk32SpiStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Rk32SpiDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Rk32SpiDispatchOpen (
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
Rk32SpiDispatchClose (
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
Rk32SpiDispatchIo (
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
Rk32SpiDispatchSystemControl (
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
Rk32SpiInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the interrupt service routine for the RK32 SPI
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

    PRK32_SPI_CONTROLLER Controller;
    ULONG Mask;
    ULONG Status;

    Controller = Context;
    Status = RK32_READ_SPI(Controller, Rk32SpiInterruptStatus);
    if (Status != 0) {

        //
        // Clear the bits out of the mask to avoid an interrupt storm.
        //

        Mask = RK32_READ_SPI(Controller, Rk32SpiInterruptMask);
        Mask &= ~Status;
        RK32_WRITE_SPI(Controller, Rk32SpiInterruptMask, Mask);
        RK32_WRITE_SPI(Controller, Rk32SpiInterruptClear, Status);
        RtlAtomicOr32(&(Controller->PendingInterrupts), Status);
        return InterruptStatusClaimed;
    }

    return InterruptStatusNotClaimed;
}

INTERRUPT_STATUS
Rk32SpiInterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents the low level interrupt service routine for the
    RK32 SPI controller.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PRK32_SPI_CONTROLLER Controller;
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
    // Loop processing transfers.
    //

    while (TRUE) {

        //
        // If an error came in, fail the current transfer.
        //

        if ((InterruptBits & RK32_SPI_INTERRUPT_ERROR_MASK) != 0) {
            RtlDebugPrint("RK32 SPI: Error 0x%08x\n", InterruptBits);
            Status = STATUS_DEVICE_IO_ERROR;

        //
        // Transfer more data. If the transfer fills the FIFOs, then
        // break out and wait for the interrupt to fire to put more data in.
        //

        } else {
            Status = Rk32SpiTransferData(Controller, Transfer);
            if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
                break;
            }
        }

        //
        // The interrupt bits apply only to the first iteration of the loop.
        //

        InterruptBits = 0;

        //
        // The transfer completed entirely, so complete this one and go get a
        // new one.
        //

        Transfer = SpbTransferCompletion(Controller->SpbController,
                                         Transfer,
                                         Status);

        if (Transfer == NULL) {
            break;
        }

        Rk32SpiSetupTransfer(Controller, Transfer);
    }

InterruptServiceWorkerEnd:
    KeReleaseQueuedLock(Controller->Lock);
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Rk32SpiProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an RK32 SPI controller. It adds an interrupt vector requirement
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
Rk32SpiStartDevice (
    PIRP Irp,
    PRK32_SPI_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the RK32 SPI device.

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
        Registration.MaxFrequency = RK32_SPI_INPUT_CLOCK / 2;
        Registration.BusType = ResourceSpbBusSpi;
        RtlCopyMemory(&(Registration.FunctionTable),
                      &Rk32SpiFunctionTableTemplate,
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
        Connect.InterruptServiceRoutine = Rk32SpiInterruptService;
        Connect.LowLevelServiceRoutine = Rk32SpiInterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
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
Rk32SpiConfigureBus (
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

    ULONG Control;
    PRK32_SPI_CONTROLLER Controller;
    ULONG Divisor;
    PRESOURCE_SPB_SPI Spi;
    ULONG Value;

    Controller = Context;
    if (Configuration->BusType != ResourceSpbBusSpi) {
        return STATUS_INVALID_PARAMETER;
    }

    Spi = PARENT_STRUCTURE(Configuration, RESOURCE_SPB_SPI, Header);
    Control = RK32_SPI_CONTROL0_SS_CLK_DELAY_FULL_CLOCK |
              RK32_SPI_CONTROL0_APB_8BIT;

    if (Spi->WordSize == 4) {
        Control |= RK32_SPI_CONTROL0_DATA_FRAME_4;

    } else if (Spi->WordSize == 8) {
        Control |= RK32_SPI_CONTROL0_DATA_FRAME_8;

    } else if (Spi->WordSize == 16) {
        Control |= RK32_SPI_CONTROL0_DATA_FRAME_16;

    } else {
        return STATUS_INVALID_CONFIGURATION;
    }

    if ((Spi->Flags & RESOURCE_SPB_SPI_SECOND_PHASE) != 0) {
        Control |= RK32_SPI_CONTROL0_CLOCK_PHASE;
    }

    if ((Spi->Flags & RESOURCE_SPB_SPI_START_HIGH) != 0) {
        Control |= RK32_SPI_CONTROL0_CLOCK_INACTIVE_HIGH;
    }

    if ((Spi->Header.Flags & RESOURCE_SPB_DATA_SLAVE) != 0) {
        Control |= RK32_SPI_CONTROL0_SLAVE_MODE;
    }

    KeAcquireQueuedLock(Controller->Lock);
    Rk32SpiEnableController(Controller, FALSE);
    RK32_WRITE_SPI(Controller, Rk32SpiControl0, Control);
    Controller->Control = Control;
    Value = (RK32_SPI_FIFO_DEPTH / 2) - 1;
    RK32_WRITE_SPI(Controller, Rk32SpiTxFifoThreshold, Value);

    //
    // Trigger an interrupt as soon as there is any data in the RX FIFO.
    //

    RK32_WRITE_SPI(Controller, Rk32SpiRxFifoThreshold, 0);

    //
    // Determine the divisor. Make sure it's even by rounding up, as devices
    // can usually handle speeds that are a little too slow, but not a little
    // too fast.
    //

    Divisor = RK32_SPI_INPUT_CLOCK / Spi->Speed;
    Divisor = (Divisor + 1) & ~0x1;
    RK32_WRITE_SPI(Controller, Rk32SpiBaudRateSelect, Divisor);
    RK32_WRITE_SPI(Controller, Rk32SpiSlaveEnable, Spi->DeviceSelect);
    KeReleaseQueuedLock(Controller->Lock);
    return STATUS_SUCCESS;
}

KSTATUS
Rk32SpiSubmitTransfer (
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

    PRK32_SPI_CONTROLLER Controller;
    KSTATUS Status;
    KSTATUS TotalStatus;

    Controller = Context;
    KeAcquireQueuedLock(Controller->Lock);
    TotalStatus = STATUS_SUCCESS;
    do {
        Rk32SpiSetupTransfer(Controller, Transfer);

        //
        // Begin transferring data. If the transfer fills the FIFOs, then
        // break out and wait for the interrupt to fire to put more data in.
        //

        Status = Rk32SpiTransferData(Controller, Transfer);
        if (Status == STATUS_MORE_PROCESSING_REQUIRED) {
            break;
        }

        ASSERT(Controller->Transfer == NULL);

        //
        // The transfer completed entirely, so complete this one and go get a
        // new one.
        //

        Transfer = SpbTransferCompletion(Controller->SpbController,
                                         Transfer,
                                         Status);

        if (!KSUCCESS(Status)) {
            if (KSUCCESS(TotalStatus)) {
                TotalStatus = Status;
            }
        }

    } while (Transfer != NULL);

    KeReleaseQueuedLock(Controller->Lock);
    return TotalStatus;
}

VOID
Rk32SpiLockBus (
    PVOID Context,
    PRESOURCE_SPB_DATA Configuration
    )

/*++

Routine Description:

    This routine is called when the bus is being locked for a particular
    transfer set or directly via the interface. The software synchronization
    portion of locking the bus is handled by the SPB library, this routine
    only needs to do hardware-specific actions (like selecting or deselecting
    device lines).

Arguments:

    Context - Supplies the host controller context.

    Configuration - Supplies a pointer to the configuration of the handle that
        locked this bus. The configure bus function will still be called, this
        is only passed for reference if bus-specific actions need to be
        performed (like selecting or deselecting the device).

Return Value:

    None.

--*/

{

    PRK32_SPI_CONTROLLER Controller;
    PRESOURCE_SPB_SPI SpiData;

    Controller = Context;
    SpiData = PARENT_STRUCTURE(Configuration, RESOURCE_SPB_SPI, Header);

    ASSERT(SpiData->Header.BusType == ResourceSpbBusSpi);

    //
    // Select the device.
    //

    RK32_WRITE_SPI(Controller, Rk32SpiSlaveEnable, SpiData->DeviceSelect);
    return;
}

VOID
Rk32SpiUnlockBus (
    PVOID Context
    )

/*++

Routine Description:

    This routine is called when the bus is being unlocked.

Arguments:

    Context - Supplies the host controller context.

Return Value:

    None.

--*/

{

    PRK32_SPI_CONTROLLER Controller;

    Controller = Context;

    //
    // Deselect the device.
    //

    RK32_WRITE_SPI(Controller, Rk32SpiSlaveEnable, 0);
    return;
}

KSTATUS
Rk32SpiSetupTransfer (
    PRK32_SPI_CONTROLLER Controller,
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

    Transfer->ReceiveSizeCompleted = 0;
    Transfer->TransmitSizeCompleted = 0;
    Rk32SpiEnableController(Controller, FALSE);

    //
    // Set up the transfer direction.
    //

    Control = Controller->Control;
    Control &= ~RK32_SPI_CONTROL0_TRANSCEIVE_MASK;
    switch (Transfer->Direction) {
    case SpbTransferDirectionIn:
        Control |= RK32_SPI_CONTROL0_RECEIVE_ONLY;
        break;

    case SpbTransferDirectionOut:
        Control |= RK32_SPI_CONTROL0_TRANSMIT_ONLY;
        break;

    case SpbTransferDirectionBoth:
        Control |= RK32_SPI_CONTROL0_TRANSMIT_AND_RECEIVE;
        break;

    default:

        ASSERT(FALSE);

        return STATUS_INVALID_PARAMETER;
    }

    if (Control != Controller->Control) {
        RK32_WRITE_SPI(Controller, Rk32SpiControl0, Control);
        Controller->Control = Control;
    }

    RK32_WRITE_SPI(Controller, Rk32SpiControl1, Transfer->Size - 1);

    ASSERT(Controller->Transfer == NULL);

    Controller->Transfer = Transfer;
    Rk32SpiEnableController(Controller, TRUE);
    if (Transfer->MicrosecondDelay != 0) {
        KeDelayExecution(FALSE, FALSE, Transfer->MicrosecondDelay);
    }

    return STATUS_SUCCESS;
}

KSTATUS
Rk32SpiTransferData (
    PRK32_SPI_CONTROLLER Controller,
    PSPB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine transfers data to and from the SPI controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the transfer to execute.

Return Value:

    STATUS_MORE_PROCESSING_REQUIRED if more data needs to be sent before the
    transfer is complete.

    STATUS_SUCCESS if the data was transferred successfully.

    Other status codes if the transfer failed.

--*/

{

    UCHAR Buffer[RK32_SPI_FIFO_DEPTH];
    SPB_TRANSFER_DIRECTION Direction;
    ULONG Index;
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

    if ((Direction == SpbTransferDirectionOut) ||
        (Direction == SpbTransferDirectionBoth)) {

        Size = RK32_SPI_FIFO_DEPTH -
               RK32_READ_SPI(Controller, Rk32SpiTxFifoLevel);

        //
        // If the transfer has completed, finish it now that everything is out
        // on the wire.
        //

        if ((Transfer->TransmitSizeCompleted == Transfer->Size) &&
            (Size == RK32_SPI_FIFO_DEPTH)) {

            if (Direction == SpbTransferDirectionOut) {
                TransferDone = TRUE;
                goto TransferDataEnd;
            }
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

        for (Index = 0; Index < Size; Index += 1) {
            RK32_WRITE_SPI(Controller,
                           Rk32SpiTxFifoData,
                           Buffer[Transfer->TransmitSizeCompleted + Index]);
        }

        Transfer->TransmitSizeCompleted += Size;

        //
        // Fire an interrupt when the transmit queue is empty again, as
        // more things need to be sent.
        //

        Controller->InterruptMask |= RK32_SPI_INTERRUPT_TX_EMPTY;
    }

    //
    // Receive some data if needed.
    //

    if ((Direction == SpbTransferDirectionIn) ||
        (Direction == SpbTransferDirectionBoth)) {

        Size = RK32_READ_SPI(Controller, Rk32SpiRxFifoLevel);
        while (Size != 0) {
            if (Size > RK32_SPI_FIFO_DEPTH) {
                Size = RK32_SPI_FIFO_DEPTH;
            }

            if (Size > Transfer->Size - Transfer->ReceiveSizeCompleted) {
                Size = Transfer->Size - Transfer->ReceiveSizeCompleted;
            }

            for (Index = 0; Index < Size; Index += 1) {
                Buffer[Index] = RK32_READ_SPI(Controller, Rk32SpiRxFifoData);
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

            Size = RK32_READ_SPI(Controller, Rk32SpiRxFifoLevel);
        }
    }

TransferDataEnd:
    if (TransferDone != FALSE) {

        //
        // Disable the TX-empty interrupt, otherwise it would just keep firing.
        //

        Controller->InterruptMask &= ~RK32_SPI_INTERRUPT_TX_EMPTY;
        Controller->Transfer = NULL;
    }

    //
    // Refresh the mask, as the ISR disables interrupts in the mask.
    //

    RK32_WRITE_SPI(Controller,
                   Rk32SpiInterruptMask,
                   Controller->InterruptMask);

    if ((KSUCCESS(Status)) && (TransferDone == FALSE)) {
        Status = STATUS_MORE_PROCESSING_REQUIRED;
    }

    return Status;
}

VOID
Rk32SpiEnableController (
    PRK32_SPI_CONTROLLER Controller,
    BOOL Enable
    )

/*++

Routine Description:

    This routine makes sure that the SPI controller is enabled and active.

Arguments:

    Controller - Supplies a pointer to the controller to initialize.

    Enable - Supplies a boolean indicating whether to enable (TRUE) or disable
        (FALSE) SPI bus access.

Return Value:

    None.

--*/

{

    RK32_WRITE_SPI(Controller, Rk32SpiEnable, Enable);
    if (Enable != FALSE) {
        Controller->InterruptMask = RK32_SPI_INTERRUPT_DEFAULT_MASK;

    } else {
        Controller->InterruptMask = 0;
    }

    RK32_WRITE_SPI(Controller, Rk32SpiInterruptMask, Controller->InterruptMask);
    return;
}

