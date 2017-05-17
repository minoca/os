/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    edma3.c

Abstract:

    This module implements the TI EDMA3 controller driver.

Author:

    Evan Green 2-Feb-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/dma/dmahost.h>
#include <minoca/dma/edma3.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write to registers in the global region.
//

#define EDMA_READ(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define EDMA_WRITE(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

#define EDMA_READ64(_Controller, _Register)                 \
    ((ULONGLONG)(EDMA_READ((_Controller), (_Register))) |   \
     ((ULONGLONG)(EDMA_READ((_Controller), (_Register) + 4)) << 32))

#define EDMA_WRITE64(_Controller, _Register, _Value)            \
    EDMA_WRITE((_Controller), (_Register), (ULONG)(_Value)),    \
    EDMA_WRITE((_Controller), (_Register) + 4, (ULONG)((_Value) >> 32))

//
// These macros read and write to registers in the shadow regions.
//

#define EDMA_REGION_READ(_Controller, _Register)    \
    EDMA_READ((_Controller),                        \
              ((_Register) + 0x1000 + (0x200 * (_Controller)->Region)))

#define EDMA_REGION_WRITE(_Controller, _Register, _Value)                   \
    EDMA_WRITE((_Controller),                                               \
               ((_Register) + 0x1000 + (0x200 * (_Controller)->Region)),    \
               (_Value))

#define EDMA_REGION_READ64(_Controller, _Register)                  \
    ((ULONGLONG)(EDMA_REGION_READ((_Controller), (_Register))) |    \
     ((ULONGLONG)(EDMA_REGION_READ((_Controller), (_Register) + 4)) << 32))

#define EDMA_REGION_WRITE64(_Controller, _Register, _Value)            \
    EDMA_REGION_WRITE((_Controller), (_Register), (ULONG)(_Value)),    \
    EDMA_REGION_WRITE((_Controller), (_Register) + 4, (ULONG)((_Value) >> 32))

//
// ---------------------------------------------------------------- Definitions
//

#define EDMA_ALLOCATION_TAG 0x616D4445

#define EDMA_PARAM_WORDS (EDMA_PARAM_COUNT / (sizeof(UINTN) * BITS_PER_BYTE))
#define EDMA_TRANSFER_PARAMS 32

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an EDMA3 transfer.

Members:

    Transfer - Stores a pointer to the DMA transfer.

    Params - Stores the array of PaRAM slots allocated for this transfer.

    ParamCount - Stores the number of valid entries in the PaRAMs array.

    BytesPending - Stores the size of the currently outstanding request.

--*/

typedef struct _EDMA_TRANSFER {
    PDMA_TRANSFER Transfer;
    UCHAR Params[EDMA_TRANSFER_PARAMS];
    ULONG ParamCount;
    UINTN BytesPending;
} EDMA_TRANSFER, *PEDMA_TRANSFER;

/*++

Structure Description:

    This structure defines the set of pending interrupts in the controller.

Members:

    CompletionLow - Stores the pending completion interrupts for first 32
        channels.

    CompletionHigh - Stores the pending completion interrupts for the upper
        32 channels.

    MissedLow - Stores the pending missed event interrupts for the lower 32
        channels.

    MissedHigh - Stores the pending missed event interrupts for the upper
        32 channels.

    MissedQuick - Stores the pending missed quick event DMA interrupts.

    Error - Stores the pending error interrupts.

--*/

typedef struct _EDMA_PENDING_INTERRUPTS {
    ULONG CompletionLow;
    ULONG CompletionHigh;
    ULONG MissedLow;
    ULONG MissedHigh;
    ULONG MissedQuick;
    ULONG Error;
} EDMA_PENDING_INTERRUPTS, *PEDMA_PENDING_INTERRUPTS;

/*++

Structure Description:

    This structure defines the context for an EDMA3 controller.

Members:

    OsDevice - Stores a pointer to the OS device object.

    CompletionInterruptLine - Stores the interrupt line that this controller's
        completion interrupt comes in on.

    CompletionInterruptVector - Stores the interrupt vector that this
        controller's completion interrupt comes in on.

    CompletionInterruptHandle - Stores a pointer to the handle received when the
        completion interrupt was connected.

    ErrorInterruptLine - Stores the interrupt line that this controller's
        error interrupt comes in on.

    ErrorInterruptVector - Stores the interrupt vector that this controller's
        error interrupt comes in on.

    ErrorInterruptHandle - Stores a pointer to the handle received when the
        error interrupt was connected.

    ControllerBase - Stores the virtual address of the memory mapping to the
        EDMA3 registers.

    DmaController - Stores a pointer to the library DMA controller.

    Lock - Stores the lock serializing access to the sensitive parts of the
        structure.

    TransferList - Stores the head of the list of transfers.

    FreeList - Stores the head of the list of transfer structures that are
        allocated but not currently used.

    Params - Stores the bitmap of allocated PaRAM entries.

    Pending - Stores the pending interrupt flags.

    Region - Stores the shadow region identifier that the processor is
        connected to.

    Transfers - Stores an array of points to EDMA transfers. One for each
        channel.

--*/

typedef struct _EDMA_CONTROLLER {
    PDEVICE OsDevice;
    ULONGLONG CompletionInterruptLine;
    ULONGLONG CompletionInterruptVector;
    HANDLE CompletionInterruptHandle;
    ULONGLONG ErrorInterruptLine;
    ULONGLONG ErrorInterruptVector;
    HANDLE ErrorInterruptHandle;
    PVOID ControllerBase;
    PDMA_CONTROLLER DmaController;
    KSPIN_LOCK Lock;
    UINTN Params[EDMA_PARAM_WORDS];
    EDMA_PENDING_INTERRUPTS Pending;
    UCHAR Region;
    PEDMA_TRANSFER Transfers[EDMA_CHANNEL_COUNT];
} EDMA_CONTROLLER, *PEDMA_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
EdmaAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
EdmaDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
EdmaDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
EdmaDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
EdmaDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
EdmaDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
EdmaCompletionInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
EdmaErrorInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
EdmaInterruptServiceDispatch (
    PVOID Context
    );

KSTATUS
EdmaProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
EdmaStartDevice (
    PIRP Irp,
    PEDMA_CONTROLLER Device
    );

KSTATUS
EdmaSubmit (
    PVOID Context,
    PDMA_TRANSFER Transfer
    );

KSTATUS
EdmaCancel (
    PVOID Context,
    PDMA_TRANSFER Transfer
    );

VOID
EdmapControllerReset (
    PEDMA_CONTROLLER Controller
    );

KSTATUS
EdmapPrepareAndSubmitTransfer (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer
    );

KSTATUS
EdmapPrepareTransfer (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer
    );

KSTATUS
EdmapSubmitTransfer (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer
    );

KSTATUS
EdmapSetupParam (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer,
    UCHAR ParamIndex,
    PHYSICAL_ADDRESS MemoryAddress,
    PHYSICAL_ADDRESS DeviceAddress,
    ULONG Size,
    BOOL LastOne
    );

VOID
EdmapProcessCompletedTransfer (
    PEDMA_CONTROLLER Controller,
    ULONG Channel,
    BOOL MissedEvent
    );

VOID
EdmapTearDownChannel (
    PEDMA_CONTROLLER Controller,
    ULONG Channel
    );

VOID
EdmapResetTransfer (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer
    );

UCHAR
EdmapAllocateParam (
    PEDMA_CONTROLLER Controller
    );

VOID
EdmapFreeParam (
    PEDMA_CONTROLLER Controller,
    UCHAR Param
    );

VOID
EdmapGetParam (
    PEDMA_CONTROLLER Controller,
    UCHAR Index,
    PEDMA_PARAM Param
    );

VOID
EdmapSetParam (
    PEDMA_CONTROLLER Controller,
    UCHAR Index,
    PEDMA_PARAM Param
    );

VOID
EdmapClearMissEvent (
    PEDMA_CONTROLLER Controller,
    ULONG Channel
    );

RUNLEVEL
EdmapAcquireLock (
    PEDMA_CONTROLLER Controller
    );

VOID
EdmapReleaseLock (
    PEDMA_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER EdmaDriver = NULL;

DMA_FUNCTION_TABLE EdmaFunctionTableTemplate = {
    EdmaSubmit,
    EdmaCancel,
    NULL
};

DMA_INFORMATION EdmaInformationTemplate = {
    DMA_INFORMATION_VERSION,
    UUID_EDMA_CONTROLLER,
    0,
    0,
    NULL,
    0,
    EDMA_CHANNEL_COUNT,
    0,
    0xFFFFFFFF
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

    This routine is the entry point for the EDMA3 driver. It registers
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

    EdmaDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = EdmaAddDevice;
    FunctionTable.DispatchStateChange = EdmaDispatchStateChange;
    FunctionTable.DispatchOpen = EdmaDispatchOpen;
    FunctionTable.DispatchClose = EdmaDispatchClose;
    FunctionTable.DispatchIo = EdmaDispatchIo;
    FunctionTable.DispatchSystemControl = EdmaDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
EdmaAddDevice (
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

    PEDMA_CONTROLLER Controller;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(EDMA_CONTROLLER),
                                        EDMA_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(EDMA_CONTROLLER));
    Controller->OsDevice = DeviceToken;
    Controller->CompletionInterruptHandle = INVALID_HANDLE;
    Controller->ErrorInterruptHandle = INVALID_HANDLE;
    KeInitializeSpinLock(&(Controller->Lock));

    //
    // PaRAM zero is reserved for a null entry at all times.
    //

    Controller->Params[0] = 1;
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
EdmaDispatchStateChange (
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
            Status = EdmaProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(EdmaDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = EdmaStartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(EdmaDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
EdmaDispatchOpen (
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
EdmaDispatchClose (
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
EdmaDispatchIo (
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
EdmaDispatchSystemControl (
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
EdmaCompletionInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine processes a transfer completion interrupt.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PEDMA_CONTROLLER Controller;
    INTERRUPT_STATUS Status;
    ULONG Value;

    Controller = Context;
    Status = InterruptStatusNotClaimed;
    Value = EDMA_REGION_READ(Controller, EdmaInterruptPendingLow);
    if (Value != 0) {
        RtlAtomicOr32(&(Controller->Pending.CompletionLow), Value);
        EDMA_REGION_WRITE(Controller, EdmaInterruptClearLow, Value);
        Status = InterruptStatusClaimed;
    }

    Value = EDMA_REGION_READ(Controller, EdmaInterruptPendingHigh);
    if (Value != 0) {
        RtlAtomicOr32(&(Controller->Pending.CompletionHigh), Value);
        EDMA_REGION_WRITE(Controller, EdmaInterruptClearHigh, Value);
        Status = InterruptStatusClaimed;
    }

    return Status;
}

INTERRUPT_STATUS
EdmaErrorInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine processes a transfer error interrupt.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    PEDMA_CONTROLLER Controller;
    INTERRUPT_STATUS Status;
    ULONG Value;

    Controller = Context;
    Status = InterruptStatusNotClaimed;
    Value = EDMA_READ(Controller, EdmaEventMissedLow);
    if (Value != 0) {
        RtlAtomicOr32(&(Controller->Pending.MissedLow), Value);
        EDMA_WRITE(Controller, EdmaEventMissedClearLow, Value);
        EDMA_WRITE(Controller, EdmaSecondaryEventClearLow, Value);
        Status = InterruptStatusClaimed;
    }

    Value = EDMA_READ(Controller, EdmaEventMissedHigh);
    if (Value != 0) {
        RtlAtomicOr32(&(Controller->Pending.MissedHigh), Value);
        EDMA_WRITE(Controller, EdmaEventMissedClearHigh, Value);
        EDMA_WRITE(Controller, EdmaSecondaryEventClearHigh, Value);
        Status = InterruptStatusClaimed;
    }

    Value = EDMA_READ(Controller, EdmaQDmaEventMissed);
    if (Value != 0) {
        RtlAtomicOr32(&(Controller->Pending.MissedQuick), Value);
        EDMA_WRITE(Controller, EdmaQDmaEventMissedClear, Value);
        EDMA_WRITE(Controller, EdmaQDmaSecondaryEventClear, Value);
        Status = InterruptStatusClaimed;
    }

    Value = EDMA_READ(Controller, EdmaCcError);
    if (Value != 0) {
        RtlAtomicOr32(&(Controller->Pending.Error), Value);
        EDMA_WRITE(Controller, EdmaCcErrorClear, Value);
        Status = InterruptStatusClaimed;
    }

    if (Status == InterruptStatusClaimed) {
        RtlDebugPrint("EDMA: Error 0x%x 0x%x 0x%x 0x%x\n",
                      Controller->Pending.MissedLow,
                      Controller->Pending.MissedHigh,
                      Controller->Pending.MissedQuick,
                      Controller->Pending.Error);

        EDMA_WRITE(Controller, EdmaErrorEvaluate, 1);
    }

    return Status;
}

INTERRUPT_STATUS
EdmaInterruptServiceDispatch (
    PVOID Context
    )

/*++

Routine Description:

    This routine handles interrupts for the EDMA controller at dispatch level.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    ULONGLONG Bits;
    ULONG Channel;
    PEDMA_CONTROLLER Controller;
    ULONG Value;

    Controller = Context;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);

    //
    // Handle completion interrupts.
    //

    KeAcquireSpinLock(&(Controller->Lock));
    Bits = RtlAtomicExchange32(&(Controller->Pending.CompletionLow), 0);
    Value = RtlAtomicExchange32(&(Controller->Pending.CompletionHigh), 0);
    Bits |= (ULONGLONG)Value << 32;
    while (Bits != 0) {
        Channel = RtlCountTrailingZeros64(Bits);
        Bits &= ~(1ULL << Channel);
        EdmapProcessCompletedTransfer(Controller, Channel, FALSE);
    }

    if ((Controller->Pending.MissedLow != 0) ||
        (Controller->Pending.MissedHigh != 0)) {

        Bits = RtlAtomicExchange32(&(Controller->Pending.MissedLow), 0);
        Value = RtlAtomicExchange32(&(Controller->Pending.MissedHigh), 0);
        Bits |= (ULONGLONG)Value << 32;
        while (Bits != 0) {
            Channel = RtlCountTrailingZeros64(Bits);
            Bits &= ~(1ULL << Channel);
            EdmapProcessCompletedTransfer(Controller, Channel, TRUE);
        }
    }

    if ((Controller->Pending.MissedQuick != 0) ||
        (Controller->Pending.Error != 0)) {

        Value = RtlAtomicExchange32(&(Controller->Pending.MissedQuick), 0);
        if (Value != 0) {
            RtlDebugPrint("EDMA: Missed quick DMA events 0x%x\n", Value);
        }

        Value = RtlAtomicExchange32(&(Controller->Pending.Error), 0);
        if (Value != 0) {
            RtlDebugPrint("EDMA: Error event 0x%x\n", Value);
        }
    }

    KeReleaseSpinLock(&(Controller->Lock));
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
EdmaProcessResourceRequirements (
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for an RK32 GPIO controller. It adds an interrupt vector requirement
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
EdmaStartDevice (
    PIRP Irp,
    PEDMA_CONTROLLER Device
    )

/*++

Routine Description:

    This routine starts the RK32 GPIO device.

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
    PRESOURCE_ALLOCATION CompletionInterrupt;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION ControllerBase;
    PHYSICAL_ADDRESS EndAddress;
    PRESOURCE_ALLOCATION ErrorInterrupt;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PRESOURCE_ALLOCATION ProtectionInterrupt;
    DMA_CONTROLLER_INFORMATION Registration;
    ULONG Size;
    KSTATUS Status;

    ControllerBase = NULL;
    CompletionInterrupt = NULL;
    ErrorInterrupt = NULL;
    ProtectionInterrupt = NULL;
    Size = 0;

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

            if (CompletionInterrupt == NULL) {
                CompletionInterrupt = Allocation;

            } else if (ProtectionInterrupt == NULL) {
                ProtectionInterrupt = Allocation;

            } else if (ErrorInterrupt == NULL) {
                ErrorInterrupt = Allocation;
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

    if ((ControllerBase == NULL) ||
        (CompletionInterrupt == NULL) ||
        (ErrorInterrupt == NULL)) {

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
    // Allocate the controller structures.
    //

    if (Device->DmaController == NULL) {
        RtlZeroMemory(&Registration, sizeof(DMA_CONTROLLER_INFORMATION));
        Registration.Version = DMA_CONTROLLER_INFORMATION_VERSION;
        Registration.Context = Device;
        Registration.Device = Device->OsDevice;
        RtlCopyMemory(&(Registration.Information),
                      &EdmaInformationTemplate,
                      sizeof(DMA_INFORMATION));

        RtlCopyMemory(&(Registration.FunctionTable),
                      &EdmaFunctionTableTemplate,
                      sizeof(DMA_FUNCTION_TABLE));

        Status = DmaCreateController(&Registration, &(Device->DmaController));
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    EdmapControllerReset(Device);

    //
    // Start up the controller.
    //

    Status = DmaStartController(Device->DmaController);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    //
    // Connect the completion interrupt.
    //

    if (Device->CompletionInterruptHandle == INVALID_HANDLE) {
        Device->CompletionInterruptVector = CompletionInterrupt->Allocation;
        Device->CompletionInterruptLine =
                             CompletionInterrupt->OwningAllocation->Allocation;

        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Device->CompletionInterruptLine;
        Connect.Vector = Device->CompletionInterruptVector;
        Connect.InterruptServiceRoutine = EdmaCompletionInterruptService;
        Connect.DispatchServiceRoutine = EdmaInterruptServiceDispatch;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->CompletionInterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Connect the error interrupt.
    //

    if (Device->ErrorInterruptHandle == INVALID_HANDLE) {
        Device->ErrorInterruptVector = ErrorInterrupt->Allocation;
        Device->ErrorInterruptLine =
                                  ErrorInterrupt->OwningAllocation->Allocation;

        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Device->ErrorInterruptLine;
        Connect.Vector = Device->ErrorInterruptVector;
        Connect.InterruptServiceRoutine = EdmaErrorInterruptService;
        Connect.DispatchServiceRoutine = EdmaInterruptServiceDispatch;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->ErrorInterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->CompletionInterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->CompletionInterruptHandle);
            Device->CompletionInterruptHandle = INVALID_HANDLE;
        }

        if (Device->ErrorInterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->ErrorInterruptHandle);
            Device->ErrorInterruptHandle  = INVALID_HANDLE;
        }

        if (Device->ControllerBase != NULL) {
            MmUnmapAddress(Device->ControllerBase, Size);
            Device->ControllerBase = NULL;
        }

        if (Device->DmaController != NULL) {
            DmaDestroyController(Device->DmaController);
            Device->DmaController = NULL;
        }
    }

    return Status;
}

KSTATUS
EdmaSubmit (
    PVOID Context,
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called to execute a transfer on the EDMA3 controller.

Arguments:

    Context - Supplies the host controller context.

    Transfer - Supplies a pointer to the transfer to begin executing. The
        controller can return immediately, and should call
        DmaProcessCompletedTransfer when the transfer completes.

Return Value:

    Status code indicating whether or not the transfer was successfully
    started.

--*/

{

    ULONG Channel;
    PEDMA_CONTROLLER Controller;
    PEDMA_TRANSFER EdmaTransfer;
    BOOL LockHeld;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    Controller = Context;
    LockHeld = FALSE;

    //
    // Allocate a transfer for this channel if necessary. This is serialized by
    // the DMA core that only submits one transfer to a channel at a time.
    //

    Channel = Transfer->Allocation->Allocation;
    EdmaTransfer = Controller->Transfers[Channel];
    if (EdmaTransfer == NULL) {
        EdmaTransfer = MmAllocateNonPagedPool(sizeof(EDMA_TRANSFER),
                                              EDMA_ALLOCATION_TAG);

        if (EdmaTransfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SubmitEnd;
        }

        EdmaTransfer->Transfer = NULL;
        EdmaTransfer->ParamCount = 0;
        Controller->Transfers[Channel] = EdmaTransfer;
    }

    OldRunLevel = EdmapAcquireLock(Controller);
    LockHeld = TRUE;

    ASSERT(EdmaTransfer->Transfer == NULL);

    EdmaTransfer->Transfer = Transfer;
    Status = EdmapPrepareAndSubmitTransfer(Controller, EdmaTransfer);
    if (!KSUCCESS(Status)) {
        goto SubmitEnd;
    }

    Status = STATUS_SUCCESS;

SubmitEnd:
    if (!KSUCCESS(Status)) {
        if (EdmaTransfer != NULL) {
            EdmapResetTransfer(Controller, EdmaTransfer);
        }
    }

    if (LockHeld != FALSE) {
        EdmapReleaseLock(Controller, OldRunLevel);
    }

    return Status;
}

KSTATUS
EdmaCancel (
    PVOID Context,
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called to cancel an in-progress transfer. Once this routine
    returns, the transfer should be all the way out of the DMA controller and
    the controller should no longer interrupt because of this transfer. This
    routine is called at dispatch level.

Arguments:

    Context - Supplies the host controller context.

    Transfer - Supplies a pointer to the transfer to cancel.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_LATE if the transfer is already complete.

    Other errors on other failures.

--*/

{

    ULONG Channel;
    PEDMA_CONTROLLER Controller;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    Controller = Context;
    Channel = Transfer->Allocation->Allocation;

    //
    // If there is no transfer for this channel, then something is wrong.
    //

    if (Controller->Transfers[Channel] == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Do a quick check to see if the transfer is still in the channel. If it
    // is not, then it's too late.
    //

    if (Controller->Transfers[Channel]->Transfer != Transfer) {
        return STATUS_TOO_LATE;
    }

    //
    // Grab the lock to synchronize with completion, and then look again.
    //

    OldRunLevel = EdmapAcquireLock(Controller);
    if (Controller->Transfers[Channel]->Transfer != Transfer) {
        Status = STATUS_TOO_LATE;
        goto CancelEnd;
    }

    //
    // Tear down the channel to stop any transfer that might be in progress.
    //

    EdmapTearDownChannel(Controller, Channel);
    EdmapResetTransfer(Controller, Controller->Transfers[Channel]);
    Status = STATUS_SUCCESS;

CancelEnd:
    EdmapReleaseLock(Controller, OldRunLevel);
    return Status;
}

VOID
EdmapControllerReset (
    PEDMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine resets and initializes the EDMA controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    ULONG Channel;
    EDMA_PARAM Param;

    EDMA_WRITE64(Controller, EdmaEventMissedClearLow, -1ULL);
    EDMA_WRITE(Controller, EdmaQDmaEventMissedClear, -1);
    EDMA_WRITE(Controller, EdmaCcErrorClear, -1);

    //
    // Create a null entry. PaRAM slot zero is reserved to always be a null
    // entry.
    //

    RtlZeroMemory(&Param, sizeof(EDMA_PARAM));
    EdmapSetParam(Controller, 0, &Param);

    //
    // Initially set all events to point at the null entry.
    //

    for (Channel = 0; Channel < EDMA_CHANNEL_COUNT; Channel += 1) {
        EDMA_WRITE(Controller, EDMA_DMA_CHANNEL_MAP(Channel), 0);
    }

    //
    // Enable all DMA channels in this controller's region.
    //

    EDMA_WRITE64(Controller,
                 EDMA_DMA_REGION_ACCESS(Controller->Region),
                 -1ULL);

    EDMA_WRITE64(Controller,
                 EDMA_QDMA_REGION_ACCESS(Controller->Region),
                 -1ULL);

    //
    // Disable all interrupts.
    //

    EDMA_REGION_WRITE64(Controller, EdmaInterruptEnableClearLow, -1ULL);
    return;
}

KSTATUS
EdmapPrepareAndSubmitTransfer (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine prepares and submits an EDMA transfer.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the transfer to prepare and submit.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = EdmapPrepareTransfer(Controller, Transfer);
    if (!KSUCCESS(Status)) {
        goto PrepareAndSubmitTransfer;
    }

    Status = EdmapSubmitTransfer(Controller, Transfer);
    if (!KSUCCESS(Status)) {
        goto PrepareAndSubmitTransfer;
    }

PrepareAndSubmitTransfer:
    return Status;
}

KSTATUS
EdmapPrepareTransfer (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine prepares for a DMA transfer, filling out as many PaRAM
    entries as possible.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the transfer to set up.

Return Value:

    Status code.

--*/

{

    UINTN BytesThisRound;
    PEDMA_CONFIGURATION Configuration;
    PHYSICAL_ADDRESS DeviceAddress;
    PDMA_TRANSFER DmaTransfer;
    PIO_BUFFER_FRAGMENT Fragment;
    ULONG FragmentIndex;
    UINTN FragmentOffset;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    PHYSICAL_ADDRESS MemoryAddress;
    ULONG ParamIndex;
    UINTN ParamSize;
    PHYSICAL_ADDRESS PreviousAddress;
    UINTN Remaining;
    KSTATUS Status;

    DmaTransfer = Transfer->Transfer;
    ParamIndex = 0;
    IoBuffer = DmaTransfer->Memory;
    if (DmaTransfer->Completed >= DmaTransfer->Size) {
        Status = STATUS_SUCCESS;
        goto PrepareTransferEnd;
    }

    //
    // Memory to memory transfers require some reorganization of the loop in
    // this function.
    //

    ASSERT(DmaTransfer->Direction != DmaTransferMemoryToMemory);

    DeviceAddress = DmaTransfer->Device.Address;
    if ((DmaTransfer->Flags & DMA_TRANSFER_ADVANCE_DEVICE) != 0) {
        DeviceAddress += DmaTransfer->Completed;
    }

    //
    // Get past the already completed portion.
    //

    IoBufferOffset = MmGetIoBufferCurrentOffset(IoBuffer) +
                     DmaTransfer->Completed;

    FragmentIndex = 0;
    FragmentOffset = 0;
    while (IoBufferOffset != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        if (IoBufferOffset < Fragment->Size) {
            FragmentOffset = IoBufferOffset;
            break;
        }

        IoBufferOffset -= Fragment->Size;
        FragmentIndex += 1;
    }

    //
    // Now loop filling out PaRAM entries.
    //

    Transfer->BytesPending = 0;
    Remaining = DmaTransfer->Size - DmaTransfer->Completed;
    PreviousAddress = IoBuffer->Fragment[FragmentIndex].PhysicalAddress +
                      FragmentOffset;

    MemoryAddress = PreviousAddress;
    ParamSize = 0;
    while ((Remaining != 0) && (ParamIndex + 1 < EDMA_TRANSFER_PARAMS)) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);

        //
        // If the last address is not contiguous, or the current run is too
        // big, start a new PaRAM.
        //

        if ((Fragment->PhysicalAddress + FragmentOffset != PreviousAddress) ||
            (ParamSize == EDMA_MAX_TRANSFER_SIZE)) {

            Status = EdmapSetupParam(Controller,
                                     Transfer,
                                     ParamIndex,
                                     MemoryAddress,
                                     DeviceAddress,
                                     ParamSize,
                                     FALSE);

            if (!KSUCCESS(Status)) {
                goto PrepareTransferEnd;
            }

            Transfer->BytesPending += ParamSize;
            ParamIndex += 1;
            ParamSize = 0;
            MemoryAddress = Fragment->PhysicalAddress + FragmentOffset;
            PreviousAddress = MemoryAddress;
            if ((DmaTransfer->Flags & DMA_TRANSFER_ADVANCE_DEVICE) != 0) {
                DeviceAddress += ParamSize;
            }
        }

        BytesThisRound = Fragment->Size - FragmentOffset;
        if (BytesThisRound > Remaining) {
            BytesThisRound = Remaining;
        }

        if (BytesThisRound > EDMA_MAX_TRANSFER_SIZE - ParamSize) {
            BytesThisRound = EDMA_MAX_TRANSFER_SIZE - ParamSize;
        }

        FragmentOffset += BytesThisRound;

        ASSERT(FragmentOffset <= Fragment->Size);

        if (FragmentOffset == Fragment->Size) {
            FragmentIndex += 1;
            FragmentOffset = 0;
        }

        ParamSize += BytesThisRound;
        Remaining -= BytesThisRound;
        PreviousAddress += BytesThisRound;
    }

    if (ParamSize != 0) {
        Status = EdmapSetupParam(Controller,
                                 Transfer,
                                 ParamIndex,
                                 MemoryAddress,
                                 DeviceAddress,
                                 ParamSize,
                                 TRUE);

        if (!KSUCCESS(Status)) {
            goto PrepareTransferEnd;
        }

        Transfer->BytesPending += ParamSize;
        ParamIndex += 1;
    }

    //
    // If this is an event based transaction, limit the DMA transfer to what
    // could be achieved this round. Otherwise, the caller may set up a larger
    // transfer, resulting in missed events.
    //

    Configuration = DmaTransfer->Configuration;
    if ((Configuration != NULL) &&
        (DmaTransfer->ConfigurationSize >= sizeof(EDMA_CONFIGURATION))) {

        if (Configuration->Mode == EdmaTriggerModeEvent) {
            Transfer->Transfer->Size = Transfer->BytesPending +
                                       Transfer->Transfer->Completed;
        }
    }

    Status = STATUS_SUCCESS;

PrepareTransferEnd:
    return Status;
}

KSTATUS
EdmapSubmitTransfer (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine submits a transfer to the EDMA controller. It assumes all
    PaRAMs are set up and ready to go.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the EDMA transfer.

Return Value:

    Status code.

--*/

{

    ULONG Channel;
    ULONG ChannelMask;
    PEDMA_CONFIGURATION Configuration;
    PDMA_TRANSFER DmaTransfer;
    EDMA3_TRIGGER_MODE Mode;
    ULONG Offset;
    ULONG Queue;
    ULONG Register;
    ULONG Shift;
    ULONG Value;

    DmaTransfer = Transfer->Transfer;
    Configuration = DmaTransfer->Configuration;
    if (DmaTransfer->ConfigurationSize < sizeof(EDMA_CONFIGURATION)) {
        Configuration = NULL;
    }

    Channel = DmaTransfer->Allocation->Allocation;
    Offset = 0;
    if (Channel >= 32) {
        ChannelMask = 1 << (Channel - 32);
        Offset = 4;

    } else {
        ChannelMask = 1 << Channel;
    }

    ASSERT(Transfer->ParamCount != 0);

    //
    // Set the channel to the first PaRAM entry.
    //

    Register = EDMA_DMA_CHANNEL_MAP(Channel);
    EDMA_WRITE(Controller, Register, Transfer->Params[0] * sizeof(EDMA_PARAM));

    //
    // Shove everything on queue zero unless the caller wants something
    // different.
    //

    Queue = 0;
    if (Configuration != NULL) {
        Queue = Configuration->Queue;
    }

    Shift = EDMA_CHANNEL_QUEUE_SHIFT(Channel);
    Register = EDMA_CHANNEL_QUEUE_REGISTER(Channel);
    Value = EDMA_READ(Controller, Register);
    Value &= ~(EDMA_QUEUE_NUMBER_MASK << Shift);
    Value |= Queue << Shift;
    EDMA_WRITE(Controller, Register, Value);

    //
    // Enable the channel interrupt.
    //

    EDMA_REGION_WRITE(Controller,
                      EdmaInterruptEnableSetLow + Offset,
                      ChannelMask);

    //
    // Kick off the transfer.
    //

    Mode = EdmaTriggerModeManual;
    if (Configuration != NULL) {
        Mode = Configuration->Mode;
    }

    switch (Mode) {

    //
    // For manual mode, just set the event.
    //

    case EdmaTriggerModeManual:
        EDMA_REGION_WRITE(Controller, EdmaEventSetLow + Offset, ChannelMask);
        break;

    //
    // For event mode, clear the secondary event and event miss registers, then
    // enable the event.
    //

    case EdmaTriggerModeEvent:
        EdmapClearMissEvent(Controller, Channel);
        EDMA_REGION_WRITE(Controller,
                          EdmaEventEnableSetLow + Offset,
                          ChannelMask);

        break;

    default:
        return STATUS_INVALID_CONFIGURATION;
    }

    return STATUS_SUCCESS;
}

KSTATUS
EdmapSetupParam (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer,
    UCHAR ParamIndex,
    PHYSICAL_ADDRESS MemoryAddress,
    PHYSICAL_ADDRESS DeviceAddress,
    ULONG Size,
    BOOL LastOne
    )

/*++

Routine Description:

    This routine fills out a PaRAM entry.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the EDMA transfer.

    ParamIndex - Supplies the index to fill out.

    MemoryAddress - Supplies the physical memory address to set.

    DeviceAddress - Supplies the physical device address to set.

    Size - Supplies the size of the PaRAM transfer in bytes.

    LastOne - Supplies a boolean indicating if this is the last transfer or not.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if a PaRAM entry could not be allocated.

    STATUS_INVALID_CONFIGURATION if the settings in the existing configuration
    conflict with the request.

--*/

{

    ULONG BlockSize;
    ULONG Channel;
    PEDMA_CONFIGURATION Configuration;
    PDMA_TRANSFER DmaTransfer;
    EDMA_PARAM Param;

    DmaTransfer = Transfer->Transfer;

    ASSERT(ParamIndex <= Transfer->ParamCount);
    ASSERT(ParamIndex < EDMA_TRANSFER_PARAMS);

    if (ParamIndex == Transfer->ParamCount) {
        Transfer->Params[ParamIndex] = EdmapAllocateParam(Controller);
        if (Transfer->Params[ParamIndex] == 0) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Transfer->ParamCount += 1;
    }

    //
    // Use the supplied configuration if there is one.
    //

    Configuration = DmaTransfer->Configuration;
    if ((Configuration != NULL) &&
        (DmaTransfer->ConfigurationSize >= sizeof(EDMA_CONFIGURATION))) {

        RtlCopyMemory(&Param, &(Configuration->Param), sizeof(EDMA_PARAM));

        //
        // Figure out how many blocks are in this transfer depending on whether
        // or not there's a third dimension set.
        //

        BlockSize = Param.SourceCIndex;
        if (BlockSize < Param.DestinationCIndex) {
            BlockSize = Param.DestinationCIndex;
        }

        if (BlockSize != 0) {

            //
            // If there's a stride in the third dimension, there had better
            // be a count in the second.
            //

            ASSERT(Param.BCount != 0);

            Param.CCount = Size / BlockSize;
            if ((Size % BlockSize) != 0) {

                ASSERT(FALSE);

                return STATUS_INVALID_CONFIGURATION;
            }

        } else {

            //
            // If there's no stride in the third dimension, there had better
            // not be a count either.
            //

            ASSERT(Param.CCount == 0);

            BlockSize = Param.SourceBIndex;
            if (BlockSize < Param.DestinationBIndex) {
                BlockSize = Param.DestinationBIndex;
            }

            if (BlockSize != 0) {
                Param.BCount = Size / BlockSize;
            }
        }

    } else {
        Configuration = NULL;
        RtlZeroMemory(&Param, sizeof(EDMA_PARAM));
        Param.ACount = DmaTransfer->Width / BITS_PER_BYTE;
        if ((Param.ACount == 0) ||
            ((DmaTransfer->Width % BITS_PER_BYTE) != 0)) {

            ASSERT(FALSE);

            return STATUS_INVALID_CONFIGURATION;
        }

        Param.BCount = Size / Param.ACount;
        if ((Param.BCount == 0) || ((Size % Param.ACount) != 0)) {

            ASSERT(FALSE);

            return STATUS_INVALID_CONFIGURATION;
        }

        Param.SourceBIndex = Param.ACount;
        Param.DestinationBIndex = Param.ACount;
        Channel = DmaTransfer->Allocation->Allocation;
        Param.Options = EDMA_TRANSFER_AB_SYNCHRONIZED;
        Param.Options |= (Channel << EDMA_TRANSFER_COMPLETION_CODE_SHIFT) &
                         EDMA_TRANSFER_COMPLETION_CODE_MASK;

        if ((DmaTransfer->Flags & DMA_TRANSFER_ADVANCE_DEVICE) == 0) {
            switch (DmaTransfer->Width) {
            case 256:
                Param.Options |= EDMA_TRANSFER_FIFO_WIDTH_256;
                break;

            case 128:
                Param.Options |= EDMA_TRANSFER_FIFO_WIDTH_128;
                break;

            case 64:
                Param.Options |= EDMA_TRANSFER_FIFO_WIDTH_64;
                break;

            case 32:
                Param.Options |= EDMA_TRANSFER_FIFO_WIDTH_32;
                break;

            case 16:
                Param.Options |= EDMA_TRANSFER_FIFO_WIDTH_16;
                break;

            case 8:
                Param.Options |= EDMA_TRANSFER_FIFO_WIDTH_8;
                break;

            default:

                ASSERT(FALSE);

                return STATUS_INVALID_CONFIGURATION;
            }
        }
    }

    //
    // Link to the next PaRAM entry.
    //

    if (LastOne != FALSE) {
        Param.Link = EDMA_LINK_TERMINATE;
        Param.Options |= EDMA_TRANSFER_COMPLETION_INTERRUPT;

    } else {

        ASSERT(ParamIndex + 1 <= Transfer->ParamCount);
        ASSERT(ParamIndex + 1 < EDMA_TRANSFER_PARAMS);

        if (ParamIndex + 1 == Transfer->ParamCount) {
            Transfer->Params[ParamIndex + 1] = EdmapAllocateParam(Controller);
            if (Transfer->Params[ParamIndex + 1] == 0) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            Transfer->ParamCount += 1;
        }

        Param.Link = Transfer->Params[ParamIndex + 1] * sizeof(EDMA_PARAM);
    }

    ASSERT((ULONG)DeviceAddress == DeviceAddress);
    ASSERT((ULONG)MemoryAddress == MemoryAddress);

    if (DmaTransfer->Direction == DmaTransferFromDevice) {
        Param.Source = DeviceAddress;
        Param.Destination = MemoryAddress;
        if (Configuration == NULL) {
            if ((DmaTransfer->Flags & DMA_TRANSFER_ADVANCE_DEVICE) == 0) {
                Param.SourceBIndex = 0;
                Param.Options |= EDMA_TRANSFER_SOURCE_FIFO;
            }
        }

    } else {

        ASSERT((DmaTransfer->Direction == DmaTransferToDevice) ||
               (DmaTransfer->Direction == DmaTransferMemoryToMemory));

        Param.Source = MemoryAddress;
        Param.Destination = DeviceAddress;
        if (Configuration == NULL) {
            if ((DmaTransfer->Flags & DMA_TRANSFER_ADVANCE_DEVICE) == 0) {
                Param.DestinationBIndex = 0;
                Param.Options |= EDMA_TRANSFER_DESTINATION_FIFO;
            }
        }
    }

    EdmapSetParam(Controller, Transfer->Params[ParamIndex], &Param);
    return STATUS_SUCCESS;
}

VOID
EdmapProcessCompletedTransfer (
    PEDMA_CONTROLLER Controller,
    ULONG Channel,
    BOOL MissedEvent
    )

/*++

Routine Description:

    This routine processes a completed transfer.

Arguments:

    Controller - Supplies a pointer to the controller.

    Channel - Supplies the channel that completed.

    MissedEvent - Supplies a boolean indicating whether the transfer missed an
        an event or not.

Return Value:

    None.

--*/

{

    BOOL CompleteTransfer;
    PDMA_TRANSFER DmaTransfer;
    EDMA_PARAM Param;
    KSTATUS Status;
    PEDMA_TRANSFER Transfer;

    //
    // If the channel does not have a transfer allocated, then there is nothing
    // that can be done for this interrupt.
    //

    if (Controller->Transfers[Channel] == NULL) {
        return;
    }

    //
    // If the transfer is gone, ignore it. It may have come in while a transfer
    // was being canceled.
    //

    Transfer = Controller->Transfers[Channel];
    if (Transfer->Transfer == NULL) {
        return;
    }

    //
    // Read the channel's current PaRAM to make sure the transfer is actually
    // complete. When a NULL link is encountered, the NULL PaRAM set is written
    // to the current PaRAM set. A NULL PaRAM set has all three count fields
    // set to 0 and the NULL link set.
    //

    EdmapGetParam(Controller, Transfer->Params[0], &Param);
    if ((Param.Link != EDMA_LINK_TERMINATE) ||
        (Param.ACount != 0) ||
        (Param.BCount != 0) ||
        (Param.CCount != 0)) {

        return;
    }

    DmaTransfer = Transfer->Transfer;

    //
    // Tear down the channel, since either way this transfer is over.
    //

    EdmapTearDownChannel(Controller, Channel);
    CompleteTransfer = TRUE;
    if (MissedEvent != FALSE) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto ProcessCompletedTransferEnd;
    }

    DmaTransfer->Completed += Transfer->BytesPending;

    ASSERT((Transfer->BytesPending != 0) &&
           (DmaTransfer->Completed <= DmaTransfer->Size));

    //
    // Continue the DMA transfer if there's more to do.
    //

    if (DmaTransfer->Completed < DmaTransfer->Size) {
        Status = EdmapPrepareAndSubmitTransfer(Controller, Transfer);
        if (!KSUCCESS(Status)) {
            goto ProcessCompletedTransferEnd;
        }

        CompleteTransfer = FALSE;

    } else {
        Status = STATUS_SUCCESS;
    }

ProcessCompletedTransferEnd:
    if (CompleteTransfer != FALSE) {
        DmaTransfer->Status = Status;
        EdmapResetTransfer(Controller, Transfer);
        KeReleaseSpinLock(&(Controller->Lock));
        DmaTransfer = DmaTransferCompletion(Controller->DmaController,
                                            DmaTransfer);

        KeAcquireSpinLock(&(Controller->Lock));
        if (DmaTransfer != NULL) {
            Transfer->Transfer = DmaTransfer;
            EdmapPrepareAndSubmitTransfer(Controller, Transfer);
        }
    }

    return;
}

VOID
EdmapTearDownChannel (
    PEDMA_CONTROLLER Controller,
    ULONG Channel
    )

/*++

Routine Description:

    This routine tears down an initialized DMA channel.

Arguments:

    Controller - Supplies a pointer to the controller.

    Channel - Supplies the channel/event number to tear down.

Return Value:

    None.

--*/

{

    ULONG ChannelMask;
    ULONG Offset;

    ChannelMask = 1 << Channel;
    Offset = 0;
    if (Channel >= 32) {
        ChannelMask = 1 << (Channel - 32);
        Offset = 4;
    }

    EDMA_REGION_WRITE(Controller,
                      EdmaInterruptEnableClearLow + Offset,
                      ChannelMask);

    EDMA_REGION_WRITE(Controller,
                      EdmaEventEnableClearLow + Offset,
                      ChannelMask);

    EDMA_REGION_WRITE(Controller,
                      EdmaSecondaryEventClearLow + Offset,
                      ChannelMask);

    EdmapClearMissEvent(Controller, Channel);
    EDMA_REGION_WRITE(Controller, EdmaEventClearLow + Offset, ChannelMask);

    //
    // Set the PaRAM address back to the null entry.
    //

    EDMA_WRITE(Controller, EDMA_DMA_CHANNEL_MAP(Channel), 0);
    return;
}

VOID
EdmapResetTransfer (
    PEDMA_CONTROLLER Controller,
    PEDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine resets an EDMA transfer.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the EDMA transfer.

Return Value:

    None.

--*/

{

    ULONG ParamIndex;

    for (ParamIndex = 0; ParamIndex < Transfer->ParamCount; ParamIndex += 1) {
        EdmapFreeParam(Controller, Transfer->Params[ParamIndex]);
    }

    Transfer->ParamCount = 0;
    Transfer->Transfer = NULL;
    return;
}

UCHAR
EdmapAllocateParam (
    PEDMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine allocates a PaRAM entry.

Arguments:

    Controller - Supplies a pointer to the controller to allocate from.

Return Value:

    Returns the PaRAM index on success.

    0 on failure (0 is reserved for a permanently null entry).

--*/

{

    ULONG BitIndex;
    UINTN Block;
    ULONG BlockIndex;

    for (BlockIndex = 0; BlockIndex < EDMA_PARAM_WORDS; BlockIndex += 1) {
        Block = ~(Controller->Params[BlockIndex]);
        if (Block == 0) {
            continue;
        }

        BitIndex = RtlCountTrailingZeros(Block);
        Controller->Params[BlockIndex] |= 1 << BitIndex;
        return (BlockIndex * (sizeof(UINTN) * BITS_PER_BYTE)) + BitIndex;
    }

    return 0;
}

VOID
EdmapFreeParam (
    PEDMA_CONTROLLER Controller,
    UCHAR Param
    )

/*++

Routine Description:

    This routine frees a PaRAM entry.

Arguments:

    Controller - Supplies a pointer to the controller to free to.

    Param - Supplies a pointer to the param to free. This must not be zero.

Return Value:

    None.

--*/

{

    ULONG BitIndex;
    ULONG BlockIndex;
    ULONG Mask;

    ASSERT(Param != 0);

    BlockIndex = Param / (sizeof(UINTN) * BITS_PER_BYTE);
    BitIndex = Param % (sizeof(UINTN) * BITS_PER_BYTE);
    Mask = 1 << BitIndex;

    ASSERT((Controller->Params[BlockIndex] & Mask) != 0);

    Controller->Params[BlockIndex] &= ~Mask;
    return;
}

VOID
EdmapGetParam (
    PEDMA_CONTROLLER Controller,
    UCHAR Index,
    PEDMA_PARAM Param
    )

/*++

Routine Description:

    This routine reads an entry from PaRAM.

Arguments:

    Controller - Supplies a pointer to the controller.

    Index - Supplies the PaRAM number to get.

    Param - Supplies a pointer where the PaRAM will be returned on success.

Return Value:

    None.

--*/

{

    ULONG Register;
    ULONG WordIndex;
    PULONG Words;

    Register = EDMA_GET_PARAM(Controller, Index);
    Words = (PULONG)Param;
    for (WordIndex = 0;
         WordIndex < (sizeof(EDMA_PARAM) / sizeof(ULONG));
         WordIndex += 1) {

        *Words = EDMA_READ(Controller, Register);
        Words += 1;
        Register += sizeof(ULONG);
    }

    return;
}

VOID
EdmapSetParam (
    PEDMA_CONTROLLER Controller,
    UCHAR Index,
    PEDMA_PARAM Param
    )

/*++

Routine Description:

    This routine writes an entry to PaRAM.

Arguments:

    Controller - Supplies a pointer to the controller.

    Index - Supplies the PaRAM number to set.

    Param - Supplies a pointer to the PaRAM data to set.

Return Value:

    None.

--*/

{

    ULONG Register;
    ULONG WordIndex;
    PULONG Words;

    Register = EDMA_GET_PARAM(Controller, Index);
    Words = (PULONG)Param;
    for (WordIndex = 0;
         WordIndex < (sizeof(EDMA_PARAM) / sizeof(ULONG));
         WordIndex += 1) {

        EDMA_WRITE(Controller, Register, *Words);
        Words += 1;
        Register += sizeof(ULONG);
    }

    return;
}

VOID
EdmapClearMissEvent (
    PEDMA_CONTROLLER Controller,
    ULONG Channel
    )

/*++

Routine Description:

    This routine clears any missed events in the controller for a particular
    channel.

Arguments:

    Controller - Supplies a pointer to the controller.

    Channel - Supplies the channel to clear.

Return Value:

    None.

--*/

{

    ULONG Offset;

    Offset = 0;
    if (Channel >= 32) {
        Channel -= 32;
        Offset = 4;
    }

    EDMA_REGION_WRITE(Controller,
                      EdmaSecondaryEventClearLow + Offset,
                      1 << Channel);

    EDMA_WRITE(Controller, EdmaEventMissedClearLow + Offset, 1 << Channel);
    return;
}

RUNLEVEL
EdmapAcquireLock (
    PEDMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine raises to dispatch and acquires the DMA controller's lock.

Arguments:

    Controller - Supplies a pointer to the controller to lock.

Return Value:

    Returns the previous runlevel, which should be passed into the release
    function.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Controller->Lock));
    return OldRunLevel;
}

VOID
EdmapReleaseLock (
    PEDMA_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the DMA controller's lock and lowers to the runlevel
    the system was at before the acquire.

Arguments:

    Controller - Supplies a pointer to the controller to unlock.

    OldRunLevel - Supplies the runlevel returned by the acquire function.

Return Value:

    None.

--*/

{

    KeReleaseSpinLock(&(Controller->Lock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

