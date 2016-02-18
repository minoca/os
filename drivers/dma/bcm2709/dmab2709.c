/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    dmab2709.c

Abstract:

    This module implements the Broadcom 2709 DMA controller driver.

Author:

    Chris Stevens 12-Feb-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
#include <minoca/dma/dmahost.h>
#include <minoca/dma/dmab2709.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write to registers in the global region.
//

#define DMA_BCM2709_READ(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define DMA_BCM2709_WRITE(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// These macros read and write to channel registers.
//

#define DMA_BCM2709_CHANNEL_READ(_Controller, _Channel, _Register)      \
    HlReadRegister32((_Controller)->ControllerBase +                    \
                     DMA_BCM2709_CHANNEL_REGISTER(_Channel, _Register))

#define DMA_BCM2709_CHANNEL_WRITE(_Controller, _Channel, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase +                       \
                      DMA_BCM2709_CHANNEL_REGISTER(_Channel, _Register),    \
                      (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define DMA_BCM2709_ALLOCATION_TAG 0x616D4442

//
// Define the size of the control block table.
//

#define DMA_BCM2709_CONTROL_BLOCK_COUNT 0x100
#define DMA_BCM2709_CONTROL_BLOCK_TABLE_SIZE \
    (DMA_BCM2709_CONTROL_BLOCK_COUNT * sizeof(DMA_BCM2709_CONTROL_BLOCK))

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for a BCM2709 DMA channel.

Members:

    InterruptVector - Stores the interrupt vector that this channel's
        interrupts come in on.

    InterruptLine - Stores the interrupt line that this channel's interrupt
        comes in on.

    InterruptHandle - Stores a pointer to the handle received when the
        channel's interrupt was connected.

    ControlBlockTable - Store a pointers to an I/O buffer that contains the
        control block table for this channel.

--*/

typedef struct _DMA_BCM2709_CHANNEL {
    ULONGLONG InterruptVector;
    ULONGLONG InterruptLine;
    HANDLE InterruptHandle;
    volatile PIO_BUFFER ControlBlockTable;
} DMA_BCM2709_CHANNEL, *PDMA_BCM2709_CHANNEL;

/*++

Structure Description:

    This structure defines the context for an BCM2709 DMA controller.

Members:

    OsDevice - Stores a pointer to the OS device object.

    InterruptsConnected - Stores a boolean indicating whether or not the
        interrupts have been connected for each channel.

    ControllerBase - Stores the virtual address of the memory mapping to the
        BCM2709 DMA registers.

    DmaController - Stores a pointer to the library DMA controller.

    Lock - Stores the lock serializing access to the sensitive parts of the
        structure.

    TransferList - Stores the head of the list of transfers.

    FreeList - Stores the head of the list of transfer structures that are
        allocated but not currently used.

    PendingInterrupts - Stores the pending interrupt flags.

--*/

typedef struct _DMA_BCM2709_CONTROLLER {
    PDEVICE OsDevice;
    BOOL InterruptsConnected;
    DMA_BCM2709_CHANNEL Channel[DMA_BCM2709_CHANNEL_COUNT];
    PVOID ControllerBase;
    PDMA_CONTROLLER DmaController;
    KSPIN_LOCK Lock;
    LIST_ENTRY TransferList;
    LIST_ENTRY FreeList;
    volatile ULONG PendingInterrupts;
} DMA_BCM2709_CONTROLLER, *PDMA_BCM2709_CONTROLLER;

/*++

Structure Description:

    This structure defines the context for an BCM2709 DMA transfer.

Members:

    ListEntry - Stores pointers to the next and previous transfers on the
        transfer list or free list.

    Transfer - Stores a pointer to the DMA transfer.

    ControlBlockAddress - Stores the physical address of the first control
        block.

    BytesPending - Stores the size of the currently outstanding request.

--*/

typedef struct _DMA_BCM2709_TRANSFER {
    LIST_ENTRY ListEntry;
    PDMA_TRANSFER Transfer;
    PHYSICAL_ADDRESS ControlBlockAddress;
    UINTN BytesPending;
} DMA_BCM2709_TRANSFER, *PDMA_BCM2709_TRANSFER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
DmaBcm2709AddDevice (
    PVOID Driver,
    PSTR DeviceId,
    PSTR ClassId,
    PSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
DmaBcm2709DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DmaBcm2709DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DmaBcm2709DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DmaBcm2709DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DmaBcm2709DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
DmaBcm2709InterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
DmaBcm2709InterruptServiceDispatch (
    PVOID Context
    );

KSTATUS
DmaBcm2709ProcessResourceRequirements (
    PIRP Irp
    );

KSTATUS
DmaBcm2709StartDevice (
    PIRP Irp,
    PDMA_BCM2709_CONTROLLER Device
    );

KSTATUS
DmaBcm2709Submit (
    PVOID Context,
    PDMA_TRANSFER Transfer
    );

KSTATUS
DmaBcm2709Cancel (
    PVOID Context,
    PDMA_TRANSFER Transfer
    );

VOID
DmaBcm2709pControllerReset (
    PDMA_BCM2709_CONTROLLER Controller
    );

KSTATUS
DmaBcm2709pPrepareTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_BCM2709_TRANSFER Transfer
    );

KSTATUS
DmaBcm2709pSubmitTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_BCM2709_TRANSFER Transfer
    );

KSTATUS
DmaBcm2709pSetupControlBlock (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_BCM2709_TRANSFER Transfer,
    PDMA_BCM2709_CONTROL_BLOCK ControlBlock,
    PHYSICAL_ADDRESS MemoryAddress,
    PHYSICAL_ADDRESS DeviceAddress,
    ULONG Size,
    BOOL LastOne
    );

VOID
DmaBcm2709pProcessCompletedTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    ULONG Channel
    );

VOID
DmaBcm2709pTearDownChannel (
    PDMA_BCM2709_CONTROLLER Controller,
    ULONG Channel
    );

PDMA_BCM2709_TRANSFER
DmaBcm2709pAllocateTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_TRANSFER DmaTransfer
    );

VOID
DmaBcm2709pFreeTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_BCM2709_TRANSFER Transfer
    );

RUNLEVEL
DmaBcm2709pAcquireLock (
    PDMA_BCM2709_CONTROLLER Controller
    );

VOID
DmaBcm2709pReleaseLock (
    PDMA_BCM2709_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER DmaBcm2709Driver = NULL;

DMA_FUNCTION_TABLE DmaBcm2709FunctionTableTemplate = {
    DmaBcm2709Submit,
    DmaBcm2709Cancel,
    NULL
};

DMA_INFORMATION DmaBcm2709InformationTemplate = {
    DMA_INFORMATION_VERSION,
    UUID_DMA_BCM2709_CONTROLLER,
    0,
    NULL,
    0,
    DMA_BCM2709_CHANNEL_COUNT,
    0,
    0xFFFFFFFF
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

    This routine is the entry point for the RK32 GPIO driver. It registers
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

    DmaBcm2709Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = DmaBcm2709AddDevice;
    FunctionTable.DispatchStateChange = DmaBcm2709DispatchStateChange;
    FunctionTable.DispatchOpen = DmaBcm2709DispatchOpen;
    FunctionTable.DispatchClose = DmaBcm2709DispatchClose;
    FunctionTable.DispatchIo = DmaBcm2709DispatchIo;
    FunctionTable.DispatchSystemControl = DmaBcm2709DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
DmaBcm2709AddDevice (
    PVOID Driver,
    PSTR DeviceId,
    PSTR ClassId,
    PSTR CompatibleIds,
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

    ULONG Channel;
    PDMA_BCM2709_CONTROLLER Controller;
    KSTATUS Status;

    Controller = MmAllocateNonPagedPool(sizeof(DMA_BCM2709_CONTROLLER),
                                        DMA_BCM2709_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(DMA_BCM2709_CONTROLLER));
    Controller->OsDevice = DeviceToken;
    INITIALIZE_LIST_HEAD(&(Controller->TransferList));
    INITIALIZE_LIST_HEAD(&(Controller->FreeList));
    KeInitializeSpinLock(&(Controller->Lock));
    for (Channel = 0; Channel < DMA_BCM2709_CHANNEL_COUNT; Channel += 1) {
        Controller->Channel[Channel].InterruptHandle = INVALID_HANDLE;
    }

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
DmaBcm2709DispatchStateChange (
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
            Status = DmaBcm2709ProcessResourceRequirements(Irp);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(DmaBcm2709Driver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = DmaBcm2709StartDevice(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(DmaBcm2709Driver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
DmaBcm2709DispatchOpen (
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
DmaBcm2709DispatchClose (
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
DmaBcm2709DispatchIo (
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
DmaBcm2709DispatchSystemControl (
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
DmaBcm2709InterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine processes a channel interrupt.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    ULONG Channel;
    PDMA_BCM2709_CONTROLLER Controller;
    INTERRUPT_STATUS Status;
    ULONG Value;

    Controller = Context;
    Status = InterruptStatusNotClaimed;
    Value = DMA_BCM2709_READ(Controller, DmaBcm2709InterruptStatus);
    if (Value != 0) {
        RtlAtomicOr32(&(Controller->PendingInterrupts), Value);

        //
        // The interrupt must be acknowledged for each channel or else it will
        // keep interrupting.
        //

        while (Value != 0) {
            Channel = RtlCountTrailingZeros32(Value);
            Value &= ~(1ULL << Channel);
            DMA_BCM2709_CHANNEL_WRITE(Controller,
                                      Channel,
                                      DmaBcm2709ChannelStatus,
                                      DMA_BCM2709_CHANNEL_STATUS_INTERRUPT);
        }

        DMA_BCM2709_WRITE(Controller, DmaBcm2709InterruptStatus, Value);
        Status = InterruptStatusClaimed;
    }

    return Status;
}

INTERRUPT_STATUS
DmaBcm2709InterruptServiceDispatch (
    PVOID Context
    )

/*++

Routine Description:

    This routine handles interrupts for the DMA_BCM2709 controller at dispatch
    level.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    ULONG Channel;
    PDMA_BCM2709_CONTROLLER Controller;
    ULONG Interrupts;

    Controller = Context;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);

    //
    // Handle completion interrupts.
    //

    KeAcquireSpinLock(&(Controller->Lock));
    Interrupts = RtlAtomicExchange32(&(Controller->PendingInterrupts), 0);
    while (Interrupts != 0) {
        Channel = RtlCountTrailingZeros32(Interrupts);
        Interrupts &= ~(1ULL << Channel);
        DmaBcm2709pProcessCompletedTransfer(Controller, Channel);
    }

    KeReleaseSpinLock(&(Controller->Lock));
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
DmaBcm2709ProcessResourceRequirements (
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
DmaBcm2709StartDevice (
    PIRP Irp,
    PDMA_BCM2709_CONTROLLER Device
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
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION ControllerBase;
    PHYSICAL_ADDRESS EndAddress;
    ULONG Index;
    PRESOURCE_ALLOCATION Interrupts[DMA_BCM2709_CHANNEL_COUNT];
    ULONGLONG LineNumber;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    DMA_CONTROLLER_INFORMATION Registration;
    ULONG Size;
    KSTATUS Status;
    ULONGLONG Vector;

    ControllerBase = NULL;
    Size = 0;
    Index = 0;
    RtlZeroMemory(&Interrupts,
                  sizeof(PRESOURCE_ALLOCATION) * DMA_BCM2709_CHANNEL_COUNT);

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

            if (Index < DMA_BCM2709_CHANNEL_COUNT) {
                Interrupts[Index] = Allocation;
                Index += 1;
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
    // Fail to start if the controller base was not found or not enough
    // interrupt vectors.
    //

    if ((ControllerBase == NULL) || (Index != DMA_BCM2709_CHANNEL_COUNT)) {
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
                      &DmaBcm2709InformationTemplate,
                      sizeof(DMA_INFORMATION));

        RtlCopyMemory(&(Registration.FunctionTable),
                      &DmaBcm2709FunctionTableTemplate,
                      sizeof(DMA_FUNCTION_TABLE));

        Status = DmaCreateController(&Registration, &(Device->DmaController));
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    DmaBcm2709pControllerReset(Device);

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

    if (Device->InterruptsConnected == FALSE) {
        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.InterruptServiceRoutine = DmaBcm2709InterruptService;
        Connect.DispatchServiceRoutine = DmaBcm2709InterruptServiceDispatch;
        Connect.Context = Device;
        Connect.Device = Irp->Device;
        for (Index = 0; Index < DMA_BCM2709_CHANNEL_COUNT; Index += 1) {
            Vector = Interrupts[Index]->Allocation;
            LineNumber = Interrupts[Index]->OwningAllocation->Allocation;
            Device->Channel[Index].InterruptVector = Vector;
            Device->Channel[Index].InterruptLine = LineNumber;
            Connect.Vector = Vector;
            Connect.LineNumber = LineNumber;
            Connect.Interrupt = &(Device->Channel[Index].InterruptHandle);
            Status = IoConnectInterrupt(&Connect);
            if (!KSUCCESS(Status)) {
                return Status;
            }
        }
    }

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        for (Index = 0; Index < DMA_BCM2709_CHANNEL_COUNT; Index += 1) {
            if (Device->Channel[Index].InterruptHandle != INVALID_HANDLE) {
                IoDisconnectInterrupt(Device->Channel[Index].InterruptHandle);
                Device->Channel[Index].InterruptHandle = INVALID_HANDLE;
            }
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
DmaBcm2709Submit (
    PVOID Context,
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called to execute a transfer on the BCM2709 DMA controller.

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

    PDMA_BCM2709_CHANNEL Channel;
    PIO_BUFFER ControlBlockTable;
    PDMA_BCM2709_CONTROLLER Controller;
    PDMA_BCM2709_TRANSFER DmaBcm2709Transfer;
    ULONG IoBufferFlags;
    BOOL LockHeld;
    RUNLEVEL OldRunLevel;
    PIO_BUFFER OriginalTable;
    KSTATUS Status;

    Controller = Context;
    DmaBcm2709Transfer = NULL;
    LockHeld = FALSE;

    //
    // If this channel does not have a control block table yet, then allocate
    // it now.
    //

    Channel = &(Controller->Channel[Transfer->Allocation->Allocation]);
    if (Channel->ControlBlockTable == NULL) {
        IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS |
                        IO_BUFFER_FLAG_MAP_NON_CACHED;

        ControlBlockTable = MmAllocateNonPagedIoBuffer(
                                          0,
                                          MAX_ULONG,
                                          DMA_BCM2709_CONTROL_BLOCK_ALIGNMENT,
                                          DMA_BCM2709_CONTROL_BLOCK_TABLE_SIZE,
                                          IoBufferFlags);

        if (ControlBlockTable == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SubmitEnd;
        }

        OriginalTable = (PVOID)RtlAtomicCompareExchange(
                               (volatile UINTN *)&(Channel->ControlBlockTable),
                               (UINTN)ControlBlockTable,
                               (UINTN)NULL);

        if (OriginalTable != NULL) {
            MmFreeIoBuffer(ControlBlockTable);
        }
    }

    OldRunLevel = DmaBcm2709pAcquireLock(Controller);
    LockHeld = TRUE;
    DmaBcm2709Transfer = DmaBcm2709pAllocateTransfer(Controller, Transfer);
    if (DmaBcm2709Transfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SubmitEnd;
    }

    Status = DmaBcm2709pPrepareTransfer(Controller, DmaBcm2709Transfer);
    if (!KSUCCESS(Status)) {
        goto SubmitEnd;
    }

    Status = DmaBcm2709pSubmitTransfer(Controller, DmaBcm2709Transfer);
    if (!KSUCCESS(Status)) {
        goto SubmitEnd;
    }

    Status = STATUS_SUCCESS;

SubmitEnd:
    if (!KSUCCESS(Status)) {
        if (DmaBcm2709Transfer != NULL) {
            DmaBcm2709pFreeTransfer(Controller, DmaBcm2709Transfer);
        }
    }

    if (LockHeld != FALSE) {
        DmaBcm2709pReleaseLock(Controller, OldRunLevel);
    }

    return Status;
}

KSTATUS
DmaBcm2709Cancel (
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

    PDMA_BCM2709_CONTROLLER Controller;
    PLIST_ENTRY CurrentEntry;
    PDMA_BCM2709_TRANSFER DmaBcm2709Transfer;
    RUNLEVEL OldRunLevel;
    KSTATUS Status;

    Controller = Context;

    //
    // Grab the lock to synchronize with completion, and then look for the
    // transfer in the transfer list.
    //

    OldRunLevel = DmaBcm2709pAcquireLock(Controller);
    CurrentEntry = Controller->TransferList.Next;
    while (CurrentEntry != &(Controller->TransferList)) {
        DmaBcm2709Transfer = LIST_VALUE(CurrentEntry,
                                        DMA_BCM2709_TRANSFER,
                                        ListEntry);

        if (DmaBcm2709Transfer->Transfer == Transfer) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // If the transfer was not found, it must already have been completed.
    //

    if (CurrentEntry == &(Controller->TransferList)) {
        Status = STATUS_TOO_LATE;
        goto CancelEnd;
    }

    //
    // Tear down the channel to stop any transfer that might be in progress.
    //

    DmaBcm2709pTearDownChannel(Controller, Transfer->Allocation->Allocation);
    Status = STATUS_SUCCESS;

CancelEnd:
    DmaBcm2709pReleaseLock(Controller, OldRunLevel);
    return Status;
}

VOID
DmaBcm2709pControllerReset (
    PDMA_BCM2709_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine resets and initializes the BCM2709 DMA controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    None.

--*/

{

    ULONG Channel;
    ULONG ChannelMask;

    //
    // Reset all the channels.
    //

    ChannelMask = 0;
    for (Channel = 0; Channel < DMA_BCM2709_CHANNEL_COUNT; Channel += 1) {
        DMA_BCM2709_CHANNEL_WRITE(Controller,
                                  Channel,
                                  DmaBcm2709ChannelStatus,
                                  DMA_BCM2709_CHANNEL_STATUS_RESET);

        ChannelMask |= (1 << Channel);
    }

    //
    // Enable all DMA channels in this controller's region.
    //

    DMA_BCM2709_WRITE(Controller, DmaBcm2709Enable, ChannelMask);
    return;
}

KSTATUS
DmaBcm2709pPrepareTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_BCM2709_TRANSFER Transfer
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
    ULONG Channel;
    PDMA_BCM2709_CONTROL_BLOCK ControlBlock;
    ULONG ControlBlockCount;
    PHYSICAL_ADDRESS ControlBlockPhysical;
    PIO_BUFFER ControlBlockTable;
    PHYSICAL_ADDRESS DeviceAddress;
    PDMA_TRANSFER DmaTransfer;
    PIO_BUFFER_FRAGMENT Fragment;
    ULONG FragmentIndex;
    UINTN FragmentOffset;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    UINTN MaxSize;
    PHYSICAL_ADDRESS MemoryAddress;
    PHYSICAL_ADDRESS PreviousAddress;
    PDMA_BCM2709_CONTROL_BLOCK PreviousControlBlock;
    UINTN Remaining;
    UINTN Size;
    KSTATUS Status;
    UINTN TransferSize;

    DmaTransfer = Transfer->Transfer;
    IoBuffer = DmaTransfer->Memory;
    if (DmaTransfer->Completed >= DmaTransfer->Size) {
        Status = STATUS_SUCCESS;
        goto PrepareTransferEnd;
    }

    Channel = DmaTransfer->Allocation->Allocation;
    if (Channel >= DMA_BCM2709_LITE_CHANNEL_START) {
        MaxSize = DMA_BCM2709_MAX_LITE_TRANSFER_SIZE;

    } else {
        MaxSize = DMA_BCM2709_MAX_TRANSFER_SIZE;
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
    // Now loop filling out control blocks.
    //

    Transfer->BytesPending = 0;
    Remaining = DmaTransfer->Size - DmaTransfer->Completed;
    PreviousAddress = IoBuffer->Fragment[FragmentIndex].PhysicalAddress +
                      FragmentOffset;

    MemoryAddress = PreviousAddress;
    ControlBlockTable = Controller->Channel[Channel].ControlBlockTable;
    ControlBlock = ControlBlockTable->Fragment[0].VirtualAddress;
    ControlBlockPhysical = ControlBlockTable->Fragment[0].PhysicalAddress;
    Transfer->ControlBlockAddress = ControlBlockPhysical;
    PreviousControlBlock = NULL;
    ControlBlockCount = 0;
    TransferSize = 0;
    while ((Remaining != 0) &&
           (ControlBlockCount + 1 < DMA_BCM2709_CONTROL_BLOCK_COUNT)) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);

        //
        // If the last address is not contiguous, or the current run is too
        // big, start a new control block.
        //

        if ((Fragment->PhysicalAddress + FragmentOffset != PreviousAddress) ||
            (TransferSize == MaxSize)) {

            Status = DmaBcm2709pSetupControlBlock(Controller,
                                                  Transfer,
                                                  ControlBlock,
                                                  MemoryAddress,
                                                  DeviceAddress,
                                                  TransferSize,
                                                  FALSE);

            if (!KSUCCESS(Status)) {
                goto PrepareTransferEnd;
            }

            Transfer->BytesPending += TransferSize;
            if (PreviousControlBlock != NULL) {
                PreviousControlBlock->NextAddress = ControlBlockPhysical;
            }

            PreviousControlBlock = ControlBlock;
            ControlBlockCount += 1;
            ControlBlock += 1;
            ControlBlockPhysical += sizeof(DMA_BCM2709_CONTROL_BLOCK);
            TransferSize = 0;
            MemoryAddress = Fragment->PhysicalAddress + FragmentOffset;
            PreviousAddress = MemoryAddress;
            if ((DmaTransfer->Flags & DMA_TRANSFER_ADVANCE_DEVICE) != 0) {
                DeviceAddress += TransferSize;
            }
        }

        BytesThisRound = Fragment->Size - FragmentOffset;
        if (BytesThisRound > Remaining) {
            BytesThisRound = Remaining;
        }

        if (BytesThisRound > MaxSize - TransferSize) {
            BytesThisRound = MaxSize - TransferSize;
        }

        FragmentOffset += BytesThisRound;

        ASSERT(FragmentOffset <= Fragment->Size);

        if (FragmentOffset == Fragment->Size) {
            FragmentIndex += 1;
            FragmentOffset = 0;
        }

        Size += BytesThisRound;
        TransferSize += BytesThisRound;
        Remaining -= BytesThisRound;
        PreviousAddress += BytesThisRound;
    }

    if (TransferSize != 0) {
        Status = DmaBcm2709pSetupControlBlock(Controller,
                                              Transfer,
                                              ControlBlock,
                                              MemoryAddress,
                                              DeviceAddress,
                                              TransferSize,
                                              TRUE);

        if (!KSUCCESS(Status)) {
            goto PrepareTransferEnd;
        }

        if (PreviousControlBlock != NULL) {
            PreviousControlBlock->NextAddress = ControlBlockPhysical;
        }

        Transfer->BytesPending += TransferSize;
        ControlBlockCount += 1;
        ControlBlock += 1;
        ControlBlockPhysical += sizeof(DMA_BCM2709_CONTROL_BLOCK);
    }

    Status = STATUS_SUCCESS;

PrepareTransferEnd:
    return Status;
}

KSTATUS
DmaBcm2709pSubmitTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_BCM2709_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine submits a transfer to the BCM2709 DMA controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the DMA_BCM2709 transfer.

Return Value:

    Status code.

--*/

{

    ULONG Channel;
    PDMA_TRANSFER DmaTransfer;

    DmaTransfer = Transfer->Transfer;
    Channel = DmaTransfer->Allocation->Allocation;

    //
    // Program the channel to point at the first control block.
    //

    DMA_BCM2709_CHANNEL_WRITE(Controller,
                              Channel,
                              DmaBcm2709ChannelControlBlockAddress,
                              Transfer->ControlBlockAddress);

    //
    // Fire off the transfer.
    //

    DMA_BCM2709_CHANNEL_WRITE(Controller,
                              Channel,
                              DmaBcm2709ChannelStatus,
                              DMA_BCM2709_CHANNEL_STATUS_ACTIVE);

    return STATUS_SUCCESS;
}

KSTATUS
DmaBcm2709pSetupControlBlock (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_BCM2709_TRANSFER Transfer,
    PDMA_BCM2709_CONTROL_BLOCK ControlBlock,
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

    Transfer - Supplies a pointer to the DMA_BCM2709 transfer.

    ControlBlock - Supplies a pointer to the control block to fill out.

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

    PRESOURCE_DMA_DATA DmaData;
    PDMA_TRANSFER DmaTransfer;
    ULONG TransferInformation;

    DmaTransfer = Transfer->Transfer;

    ASSERT(DmaTransfer->Allocation->DataSize >= sizeof(RESOURCE_DMA_DATA));

    DmaData = (PRESOURCE_DMA_DATA)DmaTransfer->Allocation->Data;
    TransferInformation = 0;
    TransferInformation |= DMA_BCM2709_TRANSFER_INFORMATION_WAIT_FOR_RESPONSE;
    TransferInformation |=
                      (DmaData->Request <<
                       DMA_BCM2709_TRANSFER_INFORMATION_PERIPHERAL_MAP_SHIFT) &
                      DMA_BCM2709_TRANSFER_INFORMATION_PERIPHERAL_MAP_MASK;

    ControlBlock->TransferLength = Size;
    ControlBlock->Stride = 0;
    if (LastOne != FALSE) {
        TransferInformation |=
                             DMA_BCM2709_TRANSFER_INFORMATION_INTERRUPT_ENABLE;

        ControlBlock->NextAddress = 0;
    }

    ASSERT((ULONG)DeviceAddress == DeviceAddress);
    ASSERT((ULONG)MemoryAddress == MemoryAddress);

    if (DmaTransfer->Direction == DmaTransferFromDevice) {
        ControlBlock->SourceAddress = DeviceAddress;
        ControlBlock->DestinationAddress = MemoryAddress;
        if ((DmaTransfer->Flags & DMA_TRANSFER_ADVANCE_DEVICE) != 0) {
            TransferInformation |=
                            DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_WIDTH_128 |
                            DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_INCREMENT;
        }

        TransferInformation |=
                       DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_WIDTH_128 |
                       DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_INCREMENT |
                       DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_DATA_REQUEST;

    } else {

        ASSERT((DmaTransfer->Direction == DmaTransferToDevice) ||
               (DmaTransfer->Direction == DmaTransferMemoryToMemory));

        ControlBlock->SourceAddress = MemoryAddress;
        ControlBlock->DestinationAddress = DeviceAddress;
        if ((DmaTransfer->Direction == DmaTransferMemoryToMemory) ||
            ((DmaTransfer->Flags & DMA_TRANSFER_ADVANCE_DEVICE) != 0)) {

            TransferInformation |=
                       DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_WIDTH_128 |
                       DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_INCREMENT;
        }

        TransferInformation |=
                     DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_WIDTH_128 |
                     DMA_BCM2709_TRANSFER_INFORMATION_SOURCE_INCREMENT |
                     DMA_BCM2709_TRANSFER_INFORMATION_DESTINATION_DATA_REQUEST;
    }

    ControlBlock->TransferInformation = TransferInformation;
    return STATUS_SUCCESS;
}

VOID
DmaBcm2709pProcessCompletedTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    ULONG Channel
    )

/*++

Routine Description:

    This routine processes a completed transfer.

Arguments:

    Controller - Supplies a pointer to the controller.

    Channel - Supplies the channel that completed.

Return Value:

    None.

--*/

{

    ULONG ChannelStatus;
    BOOL CompleteTransfer;
    PLIST_ENTRY CurrentEntry;
    PDMA_TRANSFER DmaTransfer;
    KSTATUS Status;
    PDMA_BCM2709_TRANSFER Transfer;

    Status = STATUS_SUCCESS;

    //
    // Before checking for a transfer, read and clear the error state.
    //

    ChannelStatus = DMA_BCM2709_CHANNEL_READ(Controller,
                                             Channel,
                                             DmaBcm2709ChannelStatus);

    if ((ChannelStatus & DMA_BCM2709_CHANNEL_STATUS_ERROR) != 0) {
        Status = STATUS_DEVICE_IO_ERROR;
        DMA_BCM2709_CHANNEL_WRITE(Controller,
                                  Channel,
                                  DmaBcm2709ChannelDebug,
                                  DMA_BCM2709_DEBUG_ERROR_MASK);
    }

    //
    // Clear the interrupt and end state for this channel.
    //

    ChannelStatus |= DMA_BCM2709_CHANNEL_STATUS_ERROR |
                     DMA_BCM2709_CHANNEL_STATUS_END;

    DMA_BCM2709_CHANNEL_WRITE(Controller,
                              Channel,
                              DmaBcm2709ChannelStatus,
                              ChannelStatus);

    //
    // Ok. Carry on processing this channel interrupt to see if a transfer just
    // completed.
    //

    CurrentEntry = Controller->TransferList.Next;
    while (CurrentEntry != &(Controller->TransferList)) {
        Transfer = LIST_VALUE(CurrentEntry, DMA_BCM2709_TRANSFER, ListEntry);
        if (Transfer->Transfer->Allocation->Allocation == Channel) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // If the transfer is gone, ignore it. It may have come in while a transfer
    // was being canceled.
    //

    if (CurrentEntry == &(Controller->TransferList)) {
        return;
    }

    DmaTransfer = Transfer->Transfer;

    //
    // Tear down the channel, since either way this transfer is over.
    //

    DmaBcm2709pTearDownChannel(Controller, Channel);
    CompleteTransfer = TRUE;

    //
    // If an error was found above, bail now and report the error on completion.
    //

    if (!KSUCCESS(Status)) {
        goto ProcessCompletedTransferEnd;
    }

    DmaTransfer->Completed += Transfer->BytesPending;

    ASSERT((Transfer->BytesPending != 0) &&
           (DmaTransfer->Completed <= DmaTransfer->Size));

    //
    // Continue the DMA transfer if there's more to do.
    //

    if (DmaTransfer->Completed < DmaTransfer->Size) {
        Status = DmaBcm2709pPrepareTransfer(Controller, Transfer);
        if (!KSUCCESS(Status)) {
            goto ProcessCompletedTransferEnd;
        }

        Status = DmaBcm2709pSubmitTransfer(Controller, Transfer);
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
        LIST_REMOVE(&(Transfer->ListEntry));
        Transfer->ListEntry.Next = NULL;
        DmaBcm2709pFreeTransfer(Controller, Transfer);
        DmaTransferCompletion(Controller->DmaController, DmaTransfer);
    }

    return;
}

VOID
DmaBcm2709pTearDownChannel (
    PDMA_BCM2709_CONTROLLER Controller,
    ULONG Channel
    )

/*++

Routine Description:

    This routine tears down an initialized DMA channel.

Arguments:

    Controller - Supplies a pointer to the controller.

    Channel - Supplies the channel/event number to tear down.

Return Value:

    Returns a pointer to the allocated DMA_BCM2709 transfer on success.

    NULL on allocation failure.

--*/

{

    ULONG ChannelStatus;
    ULONG TransferInformation;

    //
    // There is nothing to do if the active bit is not set. Otherwise pause the
    // channel by unsetting the active bit.
    //

    ChannelStatus = DMA_BCM2709_CHANNEL_READ(Controller,
                                             Channel,
                                             DmaBcm2709ChannelStatus);

    if ((ChannelStatus & DMA_BCM2709_CHANNEL_STATUS_ACTIVE) == 0) {
        return;
    }

    ChannelStatus &= ~DMA_BCM2709_CHANNEL_STATUS_ACTIVE;
    DMA_BCM2709_CHANNEL_WRITE(Controller,
                              Channel,
                              DmaBcm2709ChannelStatus,
                              ChannelStatus);

    while (TRUE) {
        ChannelStatus = DMA_BCM2709_CHANNEL_READ(Controller,
                                                 Channel,
                                                 DmaBcm2709ChannelStatus);

        if ((ChannelStatus & DMA_BCM2709_CHANNEL_STATUS_PAUSED) != 0) {
            break;
        }
    }

    //
    // Now that it is paused, the control block next address can be modified.
    //

    DMA_BCM2709_CHANNEL_WRITE(Controller,
                              Channel,
                              DmaBcm2709ChannelNextControlBlockAddress,
                              0);

    //
    // Also make sure that the current control block will interrupt on
    // completion.
    //

    TransferInformation = DMA_BCM2709_CHANNEL_READ(
                                         Controller,
                                         Channel,
                                         DmaBcm2709ChannelTransferInformation);

    TransferInformation |= DMA_BCM2709_TRANSFER_INFORMATION_INTERRUPT_ENABLE;

    //
    // Unpause the channel and abort the transfer. It should "complete".
    //

    ChannelStatus |= DMA_BCM2709_CHANNEL_STATUS_ACTIVE |
                     DMA_BCM2709_CHANNEL_STATUS_ABORT;

    DMA_BCM2709_CHANNEL_WRITE(Controller,
                              Channel,
                              DmaBcm2709ChannelStatus,
                              ChannelStatus);

    return;
}

PDMA_BCM2709_TRANSFER
DmaBcm2709pAllocateTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_TRANSFER DmaTransfer
    )

/*++

Routine Description:

    This routine allocates an BCM2709 DMA transfer structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    DmaTransfer - Supplies a pointer to the DMA transfer.

Return Value:

    Returns a pointer to the allocated DMA_BCM2709 transfer on success.

    NULL on allocation failure.

--*/

{

    PDMA_BCM2709_TRANSFER Transfer;

    if (!LIST_EMPTY(&(Controller->FreeList))) {
        Transfer = LIST_VALUE(Controller->FreeList.Next,
                              DMA_BCM2709_TRANSFER,
                              ListEntry);

        LIST_REMOVE(&(Transfer->ListEntry));

    } else {
        Transfer = MmAllocateNonPagedPool(sizeof(DMA_BCM2709_TRANSFER),
                                          DMA_BCM2709_ALLOCATION_TAG);

        if (Transfer == NULL) {
            return NULL;
        }
    }

    Transfer->ListEntry.Next = NULL;
    Transfer->Transfer = DmaTransfer;
    INSERT_AFTER(&(Transfer->ListEntry), &(Controller->TransferList));
    return Transfer;
}

VOID
DmaBcm2709pFreeTransfer (
    PDMA_BCM2709_CONTROLLER Controller,
    PDMA_BCM2709_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine frees an DMA_BCM2709 transfer.

Arguments:

    Controller - Supplies a pointer to the controller.

    Transfer - Supplies a pointer to the DMA_BCM2709 transfer.

Return Value:

    None.

--*/

{

    ASSERT(Transfer->ListEntry.Next == NULL);

    INSERT_AFTER(&(Transfer->ListEntry), &(Controller->FreeList));
    return;
}

RUNLEVEL
DmaBcm2709pAcquireLock (
    PDMA_BCM2709_CONTROLLER Controller
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
DmaBcm2709pReleaseLock (
    PDMA_BCM2709_CONTROLLER Controller,
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

