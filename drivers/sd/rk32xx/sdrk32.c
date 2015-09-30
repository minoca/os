/*++

Copyright (c) 2015 Minoca Corp. All rights reserved.

Module Name:

    sdrk32.c

Abstract:

    This module implements the SD/MMC driver for Rk32xx SoCs. The Rockchip SD
    controller is based on the Synopsis DesignWare controller.

Author:

    Chris Stevens 29-Jul-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
#include <minoca/acpi.h>
#include "sdrk32.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write SD controller registers.
//

#define SD_DWC_READ_REGISTER(_Device, _Register) \
    HlReadRegister32((_Device)->ControllerBase + (_Register))

#define SD_DWC_WRITE_REGISTER(_Device, _Register, _Value) \
    HlWriteRegister32((_Device)->ControllerBase + (_Register), (_Value))

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
SdRk32AddDevice (
    PVOID Driver,
    PSTR DeviceId,
    PSTR ClassId,
    PSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
SdRk32DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdRk32DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdRk32DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdRk32DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdRk32DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdRk32ParentDispatchStateChange (
    PIRP Irp,
    PSD_RK32_CONTEXT Context,
    PVOID IrpContext
    );

VOID
SdRk32ChildDispatchStateChange (
    PIRP Irp,
    PSD_RK32_CHILD Child,
    PVOID IrpContext
    );

KSTATUS
SdRk32ParentProcessResourceRequirements (
    PIRP Irp,
    PSD_RK32_CONTEXT Device
    );

KSTATUS
SdRk32ParentStartDevice (
    PIRP Irp,
    PSD_RK32_CONTEXT Device
    );

KSTATUS
SdRk32ParentQueryChildren (
    PIRP Irp,
    PSD_RK32_CONTEXT Device
    );

KSTATUS
SdRk32HardResetController (
    PSD_RK32_CONTEXT Device
    );

KSTATUS
SdRk32InitializeFundamentalClock (
    PSD_RK32_CONTEXT Device
    );

INTERRUPT_STATUS
SdRk32InterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
SdRk32InterruptServiceDispatch (
    PVOID Context
    );

VOID
SdRk32DmaCompletion (
    PSD_CONTROLLER Controller,
    PVOID Context,
    UINTN BytesTransferred,
    KSTATUS Status
    );

PSD_RK32_CHILD
SdRk32pCreateChild (
    PSD_RK32_CONTEXT Device
    );

VOID
SdRk32pDestroyChild (
    PSD_RK32_CHILD Child
    );

VOID
SdRk32pChildAddReference (
    PSD_RK32_CHILD Child
    );

VOID
SdRk32pChildReleaseReference (
    PSD_RK32_CHILD Child
    );

KSTATUS
SdRk32ChildBlockIoReset (
    PVOID DiskToken
    );

KSTATUS
SdRk32ChildBlockIoRead (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
SdRk32ChildBlockIoWrite (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
SdRk32PerformIoPolled (
    PIRP_READ_WRITE IrpReadWrite,
    PSD_RK32_CHILD Child,
    BOOL Write,
    BOOL LockRequired
    );

KSTATUS
SdRk32InitializeDma (
    PSD_RK32_CONTEXT Device
    );

VOID
SdRk32BlockIoDma (
    PSD_RK32_CONTEXT Device,
    ULONGLONG BlockOffset,
    UINTN BlockCount,
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset,
    BOOL Write,
    PSD_IO_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
    );

KSTATUS
SdRk32InitializeController (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Phase
    );

KSTATUS
SdRk32ResetController (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Flags
    );

KSTATUS
SdRk32SendCommand (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PSD_COMMAND Command
    );

KSTATUS
SdRk32GetSetBusWidth (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    );

KSTATUS
SdRk32GetSetClockSpeed (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    );

KSTATUS
SdRk32StopDataTransfer (
    PSD_CONTROLLER Controller,
    PVOID Context
    );

KSTATUS
SdRk32ReadData (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

KSTATUS
SdRk32WriteData (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

KSTATUS
SdRk32SetClockSpeed (
    PSD_RK32_CONTEXT Device,
    SD_CLOCK_SPEED ClockSpeed
    );

VOID
SdRk32SetDmaInterrupts (
    PSD_CONTROLLER Controller,
    PSD_RK32_CONTEXT Device,
    BOOL Enable
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER SdRk32Driver = NULL;
UUID SdRk32DiskInterfaceUuid = UUID_DISK_INTERFACE;

DISK_INTERFACE SdRk32DiskInterfaceTemplate = {
    DISK_INTERFACE_VERSION,
    NULL,
    0,
    0,
    NULL,
    SdRk32ChildBlockIoReset,
    SdRk32ChildBlockIoRead,
    SdRk32ChildBlockIoWrite
};

SD_FUNCTION_TABLE SdRk32FunctionTable = {
    SdRk32InitializeController,
    SdRk32ResetController,
    SdRk32SendCommand,
    SdRk32GetSetBusWidth,
    SdRk32GetSetClockSpeed,
    SdRk32StopDataTransfer,
    NULL,
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

    This routine is the entry point for the SD/MMC driver. It registers its
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

    SdRk32Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = SdRk32AddDevice;
    FunctionTable.DispatchStateChange = SdRk32DispatchStateChange;
    FunctionTable.DispatchOpen = SdRk32DispatchOpen;
    FunctionTable.DispatchClose = SdRk32DispatchClose;
    FunctionTable.DispatchIo = SdRk32DispatchIo;
    FunctionTable.DispatchSystemControl = SdRk32DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
SdRk32AddDevice (
    PVOID Driver,
    PSTR DeviceId,
    PSTR ClassId,
    PSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the SD/MMC driver
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

    PSD_RK32_CONTEXT Context;
    KSTATUS Status;

    //
    // Allocate non-paged pool because this device could be the paging device.
    //

    Context = MmAllocateNonPagedPool(sizeof(SD_RK32_CONTEXT),
                                     SD_ALLOCATION_TAG);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Context, sizeof(SD_RK32_CONTEXT));
    Context->Type = SdRk32Parent;
    Context->Flags = SD_RK32_DEVICE_FLAG_INSERTION_PENDING;
    Context->InterruptHandle = INVALID_HANDLE;
    Context->Lock = KeCreateQueuedLock();
    if (Context->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Context);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Context != NULL) {
            MmFreeNonPagedPool(Context);
        }
    }

    return Status;
}

VOID
SdRk32DispatchStateChange (
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

    PSD_RK32_CONTEXT Context;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Context = DeviceContext;
    switch (Context->Type) {
    case SdRk32Parent:
        SdRk32ParentDispatchStateChange(Irp, Context, IrpContext);
        break;

    case SdRk32Child:
        SdRk32ChildDispatchStateChange(Irp,
                                       (PSD_RK32_CHILD)Context,
                                       IrpContext);

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
SdRk32DispatchOpen (
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

    PSD_RK32_CHILD Child;

    Child = DeviceContext;

    //
    // Only the child can be opened or closed.
    //

    if (Child->Type != SdRk32Child) {
        return;
    }

    SdRk32pChildAddReference(Child);
    IoCompleteIrp(SdRk32Driver, Irp, STATUS_SUCCESS);
    return;
}

VOID
SdRk32DispatchClose (
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

    PSD_RK32_CHILD Child;

    Child = DeviceContext;
    if (Child->Type != SdRk32Child) {
        return;
    }

    SdRk32pChildReleaseReference(Child);
    IoCompleteIrp(SdRk32Driver, Irp, STATUS_SUCCESS);
    return;
}

VOID
SdRk32DispatchIo (
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

    UINTN BlockCount;
    ULONGLONG BlockOffset;
    UINTN BytesToComplete;
    PSD_RK32_CHILD Child;
    BOOL CompleteIrp;
    ULONGLONG IoOffset;
    ULONG IrpReadWriteFlags;
    KSTATUS Status;
    ULONG Value;
    BOOL Write;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Child = DeviceContext;
    if (Child->Type != SdRk32Child) {

        ASSERT(FALSE);

        return;
    }

    CompleteIrp = TRUE;
    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    //
    // Polled I/O is shared by a few code paths and prepares the IRP for I/O
    // further down the stack. It should also only be hit in the down direction
    // path as it always completes the IRP.
    //

    if ((Child->Flags & SD_RK32_CHILD_FLAG_DMA_SUPPORTED) == 0) {

        ASSERT(Irp->Direction == IrpDown);

        CompleteIrp = TRUE;
        Status = SdRk32PerformIoPolled(&(Irp->U.ReadWrite), Child, Write, TRUE);
        goto DispatchIoEnd;
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

        ASSERT(Irp == Child->Irp);

        //
        // Disable DMA mode.
        //

        Value = SD_DWC_READ_REGISTER(Child->Parent, SdDwcControl);
        Value &= ~SD_DWC_CONTROL_USE_INTERNAL_DMAC;
        SD_DWC_WRITE_REGISTER(Child->Parent, SdDwcControl, Value);

        //
        // Release the hold on the controller and complete any buffer
        // operations related to the completed transfer.
        //

        Child->Irp = NULL;
        KeReleaseQueuedLock(Child->ControllerLock);
        Status = IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
        if (!KSUCCESS(Status)) {
            IoUpdateIrpStatus(Irp, Status);
        }

    //
    // Start the DMA on the way down.
    //

    } else {
        BytesToComplete = Irp->U.ReadWrite.IoSizeInBytes;
        IoOffset = Irp->U.ReadWrite.IoOffset;
        Irp->U.ReadWrite.IoBytesCompleted = 0;

        ASSERT(Irp->U.ReadWrite.IoBuffer != NULL);
        ASSERT((Child->BlockCount != 0) && (Child->BlockShift != 0));
        ASSERT(IS_ALIGNED(IoOffset, 1 << Child->BlockShift) != FALSE);
        ASSERT(IS_ALIGNED(BytesToComplete, 1 << Child->BlockShift) != FALSE);

        //
        // Before acquiring the controller's lock and starting the DMA, prepare
        // the I/O context for SD (i.e. it must use physical addresses that
        // are less than 4GB and be sector size aligned).
        //

        Status = IoPrepareReadWriteIrp(&(Irp->U.ReadWrite),
                                       1 << Child->BlockShift,
                                       0,
                                       MAX_ULONG,
                                       IrpReadWriteFlags);

        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        //
        // Lock the controller to serialize access to the hardware.
        //

        KeAcquireQueuedLock(Child->ControllerLock);
        if ((Child->Flags & SD_RK32_CHILD_FLAG_MEDIA_PRESENT) == 0) {
            KeReleaseQueuedLock(Child->ControllerLock);
            IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
            Status = STATUS_NO_MEDIA;
            goto DispatchIoEnd;
        }

        //
        // If it's DMA, just send it on through.
        //

        Irp->U.ReadWrite.NewIoOffset = IoOffset;
        Child->Irp = Irp;
        BlockOffset = IoOffset >> Child->BlockShift;
        BlockCount = BytesToComplete >> Child->BlockShift;
        CompleteIrp = FALSE;
        IoPendIrp(SdRk32Driver, Irp);

        //
        // Set the controller into DMA mode.
        //

        Value = SD_DWC_READ_REGISTER(Child->Parent, SdDwcControl);
        Value |= SD_DWC_CONTROL_USE_INTERNAL_DMAC;
        SD_DWC_WRITE_REGISTER(Child->Parent, SdDwcControl, Value);

        //
        // Make sure the system isn't trying to do I/O off the end of the
        // disk.
        //

        ASSERT(BlockOffset < Child->BlockCount);
        ASSERT(BlockCount >= 1);

        SdRk32BlockIoDma(Child->Parent,
                         BlockOffset,
                         BlockCount,
                         Irp->U.ReadWrite.IoBuffer,
                         0,
                         Write,
                         SdRk32DmaCompletion,
                         Child);

        //
        // DMA transfers are self perpetuating, so after kicking off this
        // first transfer, return. This returns with the lock held because
        // I/O is still in progress.
        //

        ASSERT(KeIsQueuedLockHeld(Child->ControllerLock) != FALSE);
        ASSERT(CompleteIrp == FALSE);
    }

DispatchIoEnd:
    if (CompleteIrp != FALSE) {
        IoCompleteIrp(SdRk32Driver, Irp, Status);
    }

    return;
}

VOID
SdRk32DispatchSystemControl (
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

    PSD_RK32_CHILD Child;
    PVOID Context;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    ULONGLONG PropertiesFileSize;
    KSTATUS Status;

    Context = Irp->U.SystemControl.SystemContext;
    Child = DeviceContext;

    //
    // Only child devices are supported.
    //

    if (Child->Type != SdRk32Child) {

        ASSERT(Child->Type == SdRk32Parent);

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

            Properties = &(Lookup->Properties);
            Properties->FileId = 0;
            Properties->Type = IoObjectBlockDevice;
            Properties->HardLinkCount = 1;
            Properties->BlockCount = Child->BlockCount;
            Properties->BlockSize = 1 << Child->BlockShift;
            WRITE_INT64_SYNC(&(Properties->FileSize),
                             Child->BlockCount << Child->BlockShift);

            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(SdRk32Driver, Irp, Status);
        break;

    //
    // Writes to the disk's properties are not allowed. Fail if the data
    // has changed.
    //

    case IrpMinorSystemControlWriteFileProperties:
        FileOperation = (PSYSTEM_CONTROL_FILE_OPERATION)Context;
        Properties = FileOperation->FileProperties;
        READ_INT64_SYNC(&(Properties->FileSize), &PropertiesFileSize);
        if ((Properties->FileId != 0) ||
            (Properties->Type != IoObjectBlockDevice) ||
            (Properties->HardLinkCount != 1) ||
            (Properties->BlockSize != (1 << Child->BlockShift)) ||
            (Properties->BlockCount != Child->BlockCount) ||
            (PropertiesFileSize != (Child->BlockCount << Child->BlockShift))) {

            Status = STATUS_NOT_SUPPORTED;

        } else {
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(SdRk32Driver, Irp, Status);
        break;

    //
    // Do not support hard disk device truncation.
    //

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(SdRk32Driver, Irp, STATUS_NOT_SUPPORTED);
        break;

    //
    // Gather and return device information.
    //

    case IrpMinorSystemControlDeviceInformation:
        break;

    case IrpMinorSystemControlSynchronize:
        IoCompleteIrp(SdRk32Driver, Irp, STATUS_SUCCESS);
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

VOID
SdRk32ParentDispatchStateChange (
    PIRP Irp,
    PSD_RK32_CONTEXT Context,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs for a parent device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Context - Supplies a pointer to the controller information.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    if (Irp->Direction == IrpUp) {
        if (!KSUCCESS(IoGetIrpStatus(Irp))) {
            return;
        }

        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            Status = SdRk32ParentProcessResourceRequirements(Irp, Context);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdRk32Driver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = SdRk32ParentStartDevice(Irp, Context);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdRk32Driver, Irp, Status);
            }

            break;

        case IrpMinorQueryChildren:
            Status = SdRk32ParentQueryChildren(Irp, Context);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdRk32Driver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
SdRk32ChildDispatchStateChange (
    PIRP Irp,
    PSD_RK32_CHILD Child,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs for a parent device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Child - Supplies a pointer to the child device information.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    BOOL CompleteIrp;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    //
    // The IRP is on its way down the stack. Do most processing here.
    //

    if (Irp->Direction == IrpDown) {
        Status = STATUS_NOT_SUPPORTED;
        CompleteIrp = TRUE;
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            Status = STATUS_SUCCESS;
            break;

        case IrpMinorStartDevice:
            Status = STATUS_SUCCESS;
            if (Child->DiskInterface.DiskToken == NULL) {
                RtlCopyMemory(&(Child->DiskInterface),
                              &SdRk32DiskInterfaceTemplate,
                              sizeof(DISK_INTERFACE));

                Child->DiskInterface.BlockSize = 1 << Child->BlockShift;
                Child->DiskInterface.BlockCount = Child->BlockCount;
                Child->DiskInterface.DiskToken = Child;
                Status = IoCreateInterface(&SdRk32DiskInterfaceUuid,
                                           Child->Device,
                                           &(Child->DiskInterface),
                                           sizeof(DISK_INTERFACE));

                if (!KSUCCESS(Status)) {
                    Child->DiskInterface.DiskToken = NULL;
                }
            }

            break;

        case IrpMinorQueryChildren:
            Irp->U.QueryChildren.Children = NULL;
            Irp->U.QueryChildren.ChildCount = 0;
            Status = STATUS_SUCCESS;
            break;

        case IrpMinorQueryInterface:
            break;

        case IrpMinorRemoveDevice:
            if (Child->DiskInterface.DiskToken != NULL) {
                Status = IoDestroyInterface(&SdRk32DiskInterfaceUuid,
                                            Child->Device,
                                            &(Child->DiskInterface));

                ASSERT(KSUCCESS(Status));

                Child->DiskInterface.DiskToken = NULL;
            }

            SdRk32pChildReleaseReference(Child);
            Status = STATUS_SUCCESS;
            break;

        //
        // Pass all other IRPs down.
        //

        default:
            CompleteIrp = FALSE;
            break;
        }

        //
        // Complete the IRP unless there's a reason not to.
        //

        if (CompleteIrp != FALSE) {
            IoCompleteIrp(SdRk32Driver, Irp, Status);
        }

    //
    // The IRP is completed and is on its way back up.
    //

    } else {

        ASSERT(Irp->Direction == IrpUp);
    }

    return;
}

KSTATUS
SdRk32ParentProcessResourceRequirements (
    PIRP Irp,
    PSD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a SD RK32xx Host controller. It adds an interrupt vector
    requirement for any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this device.

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
SdRk32ParentStartDevice (
    PIRP Irp,
    PSD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine starts up the RK32xx SD controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this SD RK32xx device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION ControllerBase;
    PRESOURCE_ALLOCATION LineAllocation;
    SD_INITIALIZATION_BLOCK Parameters;
    KSTATUS Status;

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

            ASSERT((Device->Flags &
                    SD_RK32_DEVICE_FLAG_INTERRUPT_RESOURCES_FOUND) == 0);

            ASSERT(Allocation->OwningAllocation != NULL);

            //
            // Save the line and vector number.
            //

            LineAllocation = Allocation->OwningAllocation;
            Device->InterruptLine = LineAllocation->Allocation;
            Device->InterruptVector = Allocation->Allocation;
            RtlAtomicOr32(&(Device->Flags),
                          SD_RK32_DEVICE_FLAG_INTERRUPT_RESOURCES_FOUND);

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

    if ((ControllerBase == NULL) ||
        (ControllerBase->Length < SD_RK32_CONTROLLER_LENGTH)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto StartDeviceEnd;
    }

    //
    // Initialize RK32xx specific stuff.
    //

    if (Device->ControllerBase == NULL) {
        Device->ControllerBase = MmMapPhysicalAddress(
                                                    ControllerBase->Allocation,
                                                    ControllerBase->Length,
                                                    TRUE,
                                                    FALSE,
                                                    TRUE);

        if (Device->ControllerBase == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartDeviceEnd;
        }
    }

    Status = SdRk32InitializeFundamentalClock(Device);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SdRk32InitializeFundamentalClock Failed: %x\n", Status);
        goto StartDeviceEnd;
    }

    Status = SdRk32HardResetController(Device);
    if (Status == STATUS_NO_MEDIA) {
        Status = STATUS_SUCCESS;
        goto StartDeviceEnd;

    } else if (!KSUCCESS(Status)) {
        RtlDebugPrint("SdRk32ResetController Failed: %x\n", Status);
        goto StartDeviceEnd;
    }

    //
    // Initialize the standard SD controller.
    //

    if (Device->Controller == NULL) {
        RtlZeroMemory(&Parameters, sizeof(SD_INITIALIZATION_BLOCK));
        Parameters.Voltages = SD_VOLTAGE_32_33 |
                              SD_VOLTAGE_33_34 |
                              SD_VOLTAGE_165_195;

        Parameters.HostCapabilities = SD_MODE_4BIT |
                                      SD_MODE_HIGH_SPEED |
                                      SD_MODE_AUTO_CMD12;

        Parameters.FundamentalClock = Device->FundamentalClock;
        Parameters.ConsumerContext = Device;
        RtlCopyMemory(&(Parameters.FunctionTable),
                      &SdRk32FunctionTable,
                      sizeof(SD_FUNCTION_TABLE));

        Device->Controller = SdCreateController(&Parameters);
        if (Device->Controller == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartDeviceEnd;
        }
    }

    //
    // Attempt to connect the interrupt before initializing the controller. The
    // initialization process may trigger some interrupts.
    //

    if (Device->InterruptHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Device->InterruptLine;
        Connect.Vector = Device->InterruptVector;
        Connect.InterruptServiceRoutine = SdRk32InterruptService;
        Connect.DispatchServiceRoutine = SdRk32InterruptServiceDispatch;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->Controller->InterruptHandle = Device->InterruptHandle;
    }

    Status = STATUS_SUCCESS;

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->InterruptHandle);
            Device->InterruptHandle = INVALID_HANDLE;
        }

        if (Device->Controller != NULL) {
            SdDestroyController(Device->Controller);
            Device->Controller = NULL;
        }
    }

    return Status;
}

KSTATUS
SdRk32ParentQueryChildren (
    PIRP Irp,
    PSD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine potentially enumerates the disk device for the SD RK32xx
    controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this device.

Return Value:

    Status code.

--*/

{

    ULONG BlockSize;
    ULONG FlagsMask;
    PSD_RK32_CHILD NewChild;
    ULONG OldFlags;
    KSTATUS Status;

    NewChild = NULL;

    //
    // Check to see if any changes to the children are pending.
    //

    FlagsMask = ~(SD_RK32_DEVICE_FLAG_INSERTION_PENDING |
                  SD_RK32_DEVICE_FLAG_REMOVAL_PENDING);

    OldFlags = RtlAtomicAnd32(&(Device->Flags), FlagsMask);

    //
    // If either a removal or insertion is pending, clean out the old child.
    // In practice, not all removals interrupt, meaning that two insertions can
    // arrive in a row.
    //

    FlagsMask = SD_RK32_DEVICE_FLAG_INSERTION_PENDING |
                SD_RK32_DEVICE_FLAG_REMOVAL_PENDING;

    if ((OldFlags & FlagsMask) != 0) {
        if (Device->Child != NULL) {
            KeAcquireQueuedLock(Device->Lock);
            Device->Child->Flags &= ~SD_RK32_CHILD_FLAG_MEDIA_PRESENT;
            KeReleaseQueuedLock(Device->Lock);
            Device->Child = NULL;
        }
    }

    //
    // If an insertion is pending, try to enumerate the child.
    //

    if ((OldFlags & SD_RK32_DEVICE_FLAG_INSERTION_PENDING) != 0) {

        ASSERT(Device->Child == NULL);

        Status = SdInitializeController(Device->Controller, FALSE);
        if (!KSUCCESS(Status)) {
            if (Status == STATUS_TIMEOUT) {
                Status = STATUS_SUCCESS;

            } else {
                RtlDebugPrint("SdInitializeController failed: %x\n", Status);
            }

            goto ParentQueryChildrenEnd;
        }

        NewChild = SdRk32pCreateChild(Device);
        if (NewChild == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ParentQueryChildrenEnd;
        }

        BlockSize = 0;
        Status = SdGetMediaParameters(NewChild->Controller,
                                      &(NewChild->BlockCount),
                                      &BlockSize);

        if (!KSUCCESS(Status)) {
            if (Status == STATUS_NO_MEDIA) {
                Status = STATUS_SUCCESS;
            }

            goto ParentQueryChildrenEnd;
        }

        ASSERT(POWER_OF_2(BlockSize) != FALSE);

        NewChild->BlockShift = RtlCountTrailingZeros32(BlockSize);

        //
        // Try to enable DMA, but it's okay if it doesn't succeed.
        //

        Status = SdRk32InitializeDma(Device);
        if (KSUCCESS(Status)) {
            NewChild->Flags |= SD_RK32_CHILD_FLAG_DMA_SUPPORTED;

        } else if (Status == STATUS_NO_MEDIA) {
            Status = STATUS_SUCCESS;
            goto ParentQueryChildrenEnd;
        }

        NewChild->Flags |= SD_RK32_CHILD_FLAG_MEDIA_PRESENT;
        Status = IoCreateDevice(SdRk32Driver,
                                NewChild,
                                Irp->Device,
                                "SdCard",
                                DISK_CLASS_ID,
                                NULL,
                                &(NewChild->Device));

        if (!KSUCCESS(Status)) {
            return Status;
        }

        Device->Child = NewChild;
        NewChild = NULL;
    }

    //
    // If there's no child present, don't enumerate it.
    //

    if (Device->Child == NULL) {
        return STATUS_SUCCESS;
    }

    ASSERT((Device->Child != NULL) && (Device->Child->Device != NULL));

    //
    // Enumerate the one child.
    //

    Status = IoMergeChildArrays(Irp,
                                &(Device->Child->Device),
                                1,
                                SD_ALLOCATION_TAG);

ParentQueryChildrenEnd:
    if (NewChild != NULL) {

        ASSERT(NewChild->Device == NULL);

        SdRk32pChildReleaseReference(NewChild);
    }

    return Status;
}

KSTATUS
SdRk32HardResetController (
    PSD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine hard resets the RK32xx SD controller and card.

Arguments:

    Device - Supplies a pointer to this SD RK32xx device.

Return Value:

    Status code.

--*/

{

    ULONGLONG Frequency;
    ULONG ResetMask;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Frequency = HlQueryTimeCounterFrequency();

    //
    // First perform a hardware reset on the SD card.
    //

    SD_DWC_WRITE_REGISTER(Device, SdDwcPower, SD_DWC_POWER_DISABLE);
    SD_DWC_WRITE_REGISTER(Device, SdDwcResetN, SD_DWC_RESET_ENABLE);
    HlBusySpin(5000);
    SD_DWC_WRITE_REGISTER(Device, SdDwcPower, SD_DWC_POWER_ENABLE);
    SD_DWC_WRITE_REGISTER(Device, SdDwcResetN, 0);
    HlBusySpin(1000);

    //
    // Perform a complete controller reset and wait for it to complete.
    //

    ResetMask = SD_DWC_CONTROL_FIFO_RESET |
                SD_DWC_CONTROL_DMA_RESET |
                SD_DWC_CONTROL_CONTROLLER_RESET;

    SD_DWC_WRITE_REGISTER(Device, SdDwcControl, ResetMask);
    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
        if ((Value & ResetMask) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        goto HardResetControllerEnd;
    }

    //
    // Reset the internal DMA.
    //

    Value = SD_DWC_READ_REGISTER(Device, SdDwcBusMode);
    Value |= SD_DWC_BUS_MODE_INTERNAL_DMA_RESET;
    SD_DWC_WRITE_REGISTER(Device, SdDwcBusMode, Value);
    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcBusMode);
        if ((Value & SD_DWC_BUS_MODE_INTERNAL_DMA_RESET) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Clear interrupts.
    //

    SD_DWC_WRITE_REGISTER(Device,
                          SdDwcInterruptStatus,
                          SD_DWC_INTERRUPT_STATUS_ALL_MASK);

    //
    // Set 3v3 volts in the UHS register.
    //

    SD_DWC_WRITE_REGISTER(Device, SdDwcUhs, SD_DWC_UHS_VOLTAGE_3V3);

    //
    // Set the clock to 400kHz in preparation for sending CMD0 with the
    // initialization bit set.
    //

    Status = SdRk32SetClockSpeed(Device, SdClock400kHz);
    if (!KSUCCESS(Status)) {
        goto HardResetControllerEnd;
    }

    //
    // Reset the card by sending the CMD0 reset command with the initialization
    // bit set.
    //

    Value = SD_DWC_COMMAND_START |
            SD_DWC_COMMAND_USE_HOLD_REGISTER |
            SD_DWC_COMMAND_SEND_INITIALIZATION;

    SD_DWC_WRITE_REGISTER(Device, SdDwcCommand, Value);

    //
    // Wait for the command to complete.
    //

    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        goto HardResetControllerEnd;
    }

    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcInterruptStatus);
        if (Value != 0) {
            if ((Value & SD_DWC_INTERRUPT_STATUS_COMMAND_DONE) != 0) {
                Status = STATUS_SUCCESS;

            } else if ((Value &
                        SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE_TIMEOUT) != 0) {

                Status = STATUS_NO_MEDIA;

            } else {
                Status = STATUS_DEVICE_IO_ERROR;
            }

            SD_DWC_WRITE_REGISTER(Device, SdDwcInterruptStatus, Value);
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        goto HardResetControllerEnd;
    }

HardResetControllerEnd:
    return Status;
}

KSTATUS
SdRk32InitializeFundamentalClock (
    PSD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine gets the fundamental clock frequency to use for the SD
    controller. It initializes the device context with the value.

Arguments:

    Device - Supplies a pointer to the SD RK32 device.

Return Value:

    Status code.

--*/

{

    ULONG ClockSource;
    PVOID CruBase;
    ULONG Divisor;
    ULONG Frequency;
    ULONG Mode;
    ULONG Nf;
    ULONG No;
    ULONG Nr;
    ULONG PageSize;
    PRK32XX_TABLE Rk32xxTable;
    KSTATUS Status;
    ULONG Value;

    //
    // Find the RK32xx ACPI table in order to retrieve the physical addresses
    // for the CRU and GRF.
    //

    Rk32xxTable = AcpiFindTable(RK32XX_SIGNATURE, NULL);
    if (Rk32xxTable == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    PageSize = MmPageSize();
    CruBase = MmMapPhysicalAddress(Rk32xxTable->CruBase,
                                   PageSize,
                                   TRUE,
                                   FALSE,
                                   TRUE);

    if (CruBase == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Read the current MMC0 clock source.
    //

    Value = HlReadRegister32(CruBase + Rk32CruClockSelect11);
    ClockSource = (Value & RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_MASK) >>
                  RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_SHIFT;

    Divisor = (Value & RK32_CRU_CLOCK_SELECT11_MMC0_DIVIDER_MASK) >>
              RK32_CRU_CLOCK_SELECT11_MMC0_DIVIDER_SHIFT;

    Divisor += 1;

    //
    // Get the fundamental clock frequency base on the source.
    //

    switch (ClockSource) {
    case RK32_CRU_CLOCK_SELECT11_MMC0_CODEC_PLL:
        Mode = HlReadRegister32(CruBase + Rk32CruModeControl);
        Mode = (Mode & RK32_CRU_MODE_CONTROL_CODEC_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_CODEC_PLL_MODE_SHIFT;

        if (Mode == RK32_CRU_MODE_CONTROL_SLOW_MODE) {
            Frequency = RK32_CRU_PLL_SLOW_MODE_FREQUENCY;

        } else if (Mode == RK32_CRU_MODE_CONTROL_NORMAL_MODE) {

            //
            // Calculate the clock speed based on the formula described in
            // section 3.9 of the RK3288 TRM.
            //

            Value = HlReadRegister32(CruBase + Rk32CruCodecPllConfiguration0);
            No = (Value & RK32_CRU_CODEC_PLL_CONTROL0_CLKOD_MASK) >>
                 RK32_CRU_CODEC_PLL_CONTROL0_CLKOD_SHIFT;

            No += 1;
            Nr = (Value & RK32_CRU_CODEC_PLL_CONTROL0_CLKR_MASK) >>
                 RK32_CRU_CODEC_PLL_CONTROL0_CLKR_SHIFT;

            Nr += 1;
            Value = HlReadRegister32(CruBase + Rk32CruCodecPllConfiguration1);
            Nf = (Value & RK32_CRU_CODEC_PLL_CONTROL1_CLKF_MASK) >>
                 RK32_CRU_CODEC_PLL_CONTROL1_CLKF_SHIFT;

            Nf += 1;
            Frequency = RK32_CRU_PLL_COMPUTE_CLOCK_FREQUENCY(Nf, Nr, No);

        } else if (Mode == RK32_CRU_MODE_CONTROL_DEEP_SLOW_MODE) {
            Frequency = RK32_CRU_PLL_DEEP_SLOW_MODE_FREQUENCY;

        } else {
            Status = STATUS_DEVICE_IO_ERROR;
            goto GetFundamentalClockEnd;
        }

        break;

    case RK32_CRU_CLOCK_SELECT11_MMC0_GENERAL_PLL:
        Mode = HlReadRegister32(CruBase + Rk32CruModeControl);
        Mode = (Mode & RK32_CRU_MODE_CONTROL_GENERAL_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_GENERAL_PLL_MODE_SHIFT;

        if (Mode == RK32_CRU_MODE_CONTROL_SLOW_MODE) {
            Frequency = RK32_CRU_PLL_SLOW_MODE_FREQUENCY;

        } else if (Mode == RK32_CRU_MODE_CONTROL_NORMAL_MODE) {

            //
            // Calculate the clock speed based on the formula described in
            // section 3.9 of the RK3288 TRM.
            //

            Value = HlReadRegister32(CruBase + Rk32CruGeneralPllConfiguration0);
            No = (Value & RK32_CRU_GENERAL_PLL_CONTROL0_CLKOD_MASK) >>
                 RK32_CRU_GENERAL_PLL_CONTROL0_CLKOD_SHIFT;

            No += 1;
            Nr = (Value & RK32_CRU_GENERAL_PLL_CONTROL0_CLKR_MASK) >>
                 RK32_CRU_GENERAL_PLL_CONTROL0_CLKR_SHIFT;

            Nr += 1;
            Value = HlReadRegister32(CruBase + Rk32CruGeneralPllConfiguration1);
            Nf = (Value & RK32_CRU_GENERAL_PLL_CONTROL1_CLKF_MASK) >>
                 RK32_CRU_GENERAL_PLL_CONTROL1_CLKF_SHIFT;

            Nf += 1;
            Frequency = RK32_CRU_PLL_COMPUTE_CLOCK_FREQUENCY(Nf, Nr, No);

        } else if (Mode == RK32_CRU_MODE_CONTROL_DEEP_SLOW_MODE) {
            Frequency = RK32_CRU_PLL_DEEP_SLOW_MODE_FREQUENCY;

        } else {
            Status = STATUS_DEVICE_IO_ERROR;
            goto GetFundamentalClockEnd;
        }

        break;

    case RK32_CRU_CLOCK_SELECT11_MMC0_24MHZ:
        Frequency = RK32_SDMMC_FREQUENCY_24MHZ;
        break;

    default:
        Status = STATUS_DEVICE_IO_ERROR;
        goto GetFundamentalClockEnd;
    }

    //
    // To get the MMC0 clock speed, the clock source frequency must be divided
    // by the divisor.
    //

    Device->FundamentalClock = Frequency / Divisor;
    Status = STATUS_SUCCESS;

GetFundamentalClockEnd:
    MmUnmapAddress(CruBase, PageSize);
    return Status;
}

INTERRUPT_STATUS
SdRk32InterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the RK32xx SD interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the RK32xx SD
        controller.

Return Value:

    Interrupt status.

--*/

{

    PSD_CONTROLLER Controller;
    PSD_RK32_CONTEXT Device;
    ULONG MaskedStatus;

    Device = (PSD_RK32_CONTEXT)Context;
    MaskedStatus = SD_DWC_READ_REGISTER(Device, SdDwcMaskedInterruptStatus);
    if (MaskedStatus == 0) {
        return InterruptStatusNotClaimed;
    }

    Controller = Device->Controller;
    SD_DWC_WRITE_REGISTER(Device, SdDwcInterruptStatus, MaskedStatus);
    RtlAtomicOr32(&(Controller->PendingStatusBits), MaskedStatus);
    return InterruptStatusClaimed;
}

INTERRUPT_STATUS
SdRk32InterruptServiceDispatch (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the RK32xx SD dispatch level interrupt service
    routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the RK32xx SD
        controller.

Return Value:

    Interrupt status.

--*/

{

    PVOID CompletionContext;
    PSD_IO_COMPLETION_ROUTINE CompletionRoutine;
    PSD_CONTROLLER Controller;
    PSD_RK32_CONTEXT Device;
    BOOL Inserted;
    ULONG PendingBits;
    BOOL Removed;
    KSTATUS Status;

    Device = (PSD_RK32_CONTEXT)Context;
    Controller = Device->Controller;
    PendingBits = RtlAtomicExchange32(&(Controller->PendingStatusBits), 0);
    if (PendingBits == 0) {
        return InterruptStatusNotClaimed;
    }

    //
    // Process a media change.
    //

    Status = STATUS_DEVICE_IO_ERROR;
    Inserted = FALSE;
    Removed = FALSE;
    if ((PendingBits & SD_DWC_INTERRUPT_STATUS_CARD_DETECT) != 0) {

        //
        // TODO: Hanndle RK32xx SD/MMC insertion and removal.
        //

        ASSERT(FALSE);
    }

    //
    // Process the I/O completion. The only other interrupt bits that are sent
    // to the DPC are the error bits and the transfer complete bit.
    //

    if ((PendingBits & SD_DWC_INTERRUPT_ERROR_MASK) != 0) {
        RtlDebugPrint("SD RK32xx: Error status 0x%x\n", PendingBits);
        SdErrorRecovery(Controller);
        Status = STATUS_DEVICE_IO_ERROR;

    } else if ((PendingBits &
                SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {

        Status = STATUS_SUCCESS;
    }

    if (Controller->IoCompletionRoutine != NULL) {
        CompletionRoutine = Controller->IoCompletionRoutine;
        CompletionContext = Controller->IoCompletionContext;
        Controller->IoCompletionRoutine = NULL;
        Controller->IoCompletionContext = NULL;
        CompletionRoutine(Controller,
                          CompletionContext,
                          Controller->IoRequestSize,
                          Status);
    }

    if (((Inserted != FALSE) || (Removed != FALSE)) &&
        (Controller->FunctionTable.MediaChangeCallback != NULL)) {

        Controller->FunctionTable.MediaChangeCallback(
                                                   Controller,
                                                   Controller->ConsumerContext,
                                                   Removed,
                                                   Inserted);
    }

    return InterruptStatusClaimed;
}

VOID
SdRk32DmaCompletion (
    PSD_CONTROLLER Controller,
    PVOID Context,
    UINTN BytesTransferred,
    KSTATUS Status
    )

/*++

Routine Description:

    This routine is called by the SD library when a DMA transfer completes.
    This routine is called from a DPC and, as a result, can get called back
    at dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the library when the DMA
        request was issued.

    BytesTransferred - Supplies the number of bytes transferred in the request.

    Status - Supplies the status code representing the completion of the I/O.

Return Value:

    None.

--*/

{

    UINTN BlockCount;
    ULONGLONG BlockOffset;
    PSD_RK32_CHILD Child;
    ULONGLONG IoOffset;
    UINTN IoSize;
    PIRP Irp;
    BOOL Write;

    Child = Context;
    Irp = Child->Irp;

    ASSERT(Irp != NULL);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SD RK32xx Failed: %x\n", Status);
        IoCompleteIrp(SdRk32Driver, Irp, Status);
        return;
    }

    Irp->U.ReadWrite.IoBytesCompleted += BytesTransferred;
    Irp->U.ReadWrite.NewIoOffset += BytesTransferred;

    //
    // If this transfer's over, unlock and complete the IRP.
    //

    if (Irp->U.ReadWrite.IoBytesCompleted ==
        Irp->U.ReadWrite.IoSizeInBytes) {

        IoCompleteIrp(SdRk32Driver, Irp, Status);
        return;
    }

    IoOffset = Irp->U.ReadWrite.IoOffset + Irp->U.ReadWrite.IoBytesCompleted;
    BlockOffset = IoOffset >> Child->BlockShift;
    IoSize = Irp->U.ReadWrite.IoSizeInBytes -
             Irp->U.ReadWrite.IoBytesCompleted;

    BlockCount = IoSize >> Child->BlockShift;
    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    SdRk32BlockIoDma(Child->Parent,
                     BlockOffset,
                     BlockCount,
                     Irp->U.ReadWrite.IoBuffer,
                     Irp->U.ReadWrite.IoBytesCompleted,
                     Write,
                     SdRk32DmaCompletion,
                     Child);

    return;
}

PSD_RK32_CHILD
SdRk32pCreateChild (
    PSD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine creates an SD child context.

Arguments:

    Device - Supplies a pointer to the parent device to which the child belongs.

Return Value:

    Returns a pointer to the new child on success or NULL on failure.

--*/

{

    PSD_RK32_CHILD Child;

    Child = MmAllocateNonPagedPool(sizeof(SD_RK32_CHILD), SD_ALLOCATION_TAG);
    if (Child == NULL) {
        return NULL;
    }

    RtlZeroMemory(Child, sizeof(SD_RK32_CHILD));
    Child->Type = SdRk32Child;
    Child->Parent = Device;
    Child->Controller = Device->Controller;
    Child->ControllerLock = Device->Lock;
    Child->ReferenceCount = 1;
    return Child;
}

VOID
SdRk32pDestroyChild (
    PSD_RK32_CHILD Child
    )

/*++

Routine Description:

    This routine destroys the given SD child device.

Arguments:

    Child - Supplies a pointer to the SD child device to destroy.

Return Value:

    None.

--*/

{

    ASSERT(((Child->Flags & SD_RK32_CHILD_FLAG_MEDIA_PRESENT) == 0) ||
           (Child->Device == NULL));

    ASSERT(Child->DiskInterface.DiskToken == NULL);
    ASSERT(Child->Irp == NULL);

    MmFreeNonPagedPool(Child);
    return;
}

VOID
SdRk32pChildAddReference (
    PSD_RK32_CHILD Child
    )

/*++

Routine Description:

    This routine adds a reference to SD child device.

Arguments:

    Child - Supplies a pointer to the SD child device.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Child->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
SdRk32pChildReleaseReference (
    PSD_RK32_CHILD Child
    )

/*++

Routine Description:

    This routine releases a reference from the SD child.

Arguments:

    Child - Supplies a pointer to the SD child.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Child->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        SdRk32pDestroyChild(Child);
    }

    return;
}

KSTATUS
SdRk32ChildBlockIoReset (
    PVOID DiskToken
    )

/*++

Routine Description:

    This routine must be called immediately before using the block read and
    write routines in order to allow the disk to reset any I/O channels in
    preparation for imminent block I/O. This routine is called at high run
    level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

Return Value:

    Status code.

--*/

{

    PSD_RK32_CHILD Child;
    KSTATUS Status;
    ULONG Value;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Child = (PSD_RK32_CHILD)DiskToken;

    //
    // Put the SD controller into critical execution mode.
    //

    SdSetCriticalMode(Child->Controller, TRUE);

    //
    // Abort any current transaction that might have been left incomplete
    // when the crash occurred.
    //

    Status = SdAbortTransaction(Child->Controller, FALSE);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Make sure the controller is not stuck in DMA transfer mode.
    //

    //
    // Make sure DMA mode is disabled.
    //

    Value = SD_DWC_READ_REGISTER(Child->Parent, SdDwcControl);
    if ((Value & SD_DWC_CONTROL_USE_INTERNAL_DMAC) != 0) {
        Value &= ~SD_DWC_CONTROL_USE_INTERNAL_DMAC;
        SD_DWC_WRITE_REGISTER(Child->Parent, SdDwcControl, Value);
    }

    return Status;
}

KSTATUS
SdRk32ChildBlockIoRead (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    )

/*++

Routine Description:

    This routine reads the block contents from the disk into the given I/O
    buffer using polled I/O. It does so without acquiring any locks or
    allocating any resources, as this routine is used for crash dump support
    when the system is in a very fragile state. This routine must be called at
    high level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

    IoBuffer - Supplies a pointer to the I/O buffer where the data will be read.

    BlockAddress - Supplies the block index to read (for physical disk, this is
        the LBA).

    BlockCount - Supplies the number of blocks to read.

    BlocksCompleted - Supplies a pointer that receives the total number of
        blocks read.

Return Value:

    Status code.

--*/

{

    PSD_RK32_CHILD Child;
    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Child = (PSD_RK32_CHILD)DiskToken;
    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress << Child->BlockShift;
    IrpReadWrite.IoSizeInBytes = BlockCount << Child->BlockShift;

    //
    // As this read routine is meant for critical code paths (crash dump),
    // indicate that the channel should not be locked when performing the I/O.
    // It may be that some other thread holds the lock, which would cause a
    // dead lock as all other processors and threads are likely frozen.
    //

    Status = SdRk32PerformIoPolled(&IrpReadWrite, Child, FALSE, FALSE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted >> Child->BlockShift;
    return Status;
}

KSTATUS
SdRk32ChildBlockIoWrite (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    )

/*++

Routine Description:

    This routine writes the contents of the given I/O buffer to the disk using
    polled I/O. It does so without acquiring any locks or allocating any
    resources, as this routine is used for crash dump support when the system
    is in a very fragile state. This routine must be called at high level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        write.

    BlockAddress - Supplies the block index to write to (for physical disk,
        this is the LBA).

    BlockCount - Supplies the number of blocks to write.

    BlocksCompleted - Supplies a pointer that receives the total number of
        blocks written.

Return Value:

    Status code.

--*/

{

    PSD_RK32_CHILD Child;
    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Child = (PSD_RK32_CHILD)DiskToken;
    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress << Child->BlockShift;
    IrpReadWrite.IoSizeInBytes = BlockCount << Child->BlockShift;

    //
    // As this write routine is meant for critical code paths (crash dump),
    // indicate that the channel should not be locked when performing the I/O.
    // It may be that some other thread holds the lock, which would cause a
    // dead lock as all other processors and threads are likely frozen.
    //

    Status = SdRk32PerformIoPolled(&IrpReadWrite, Child, TRUE, FALSE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted >> Child->BlockShift;
    return Status;
}

KSTATUS
SdRk32PerformIoPolled (
    PIRP_READ_WRITE IrpReadWrite,
    PSD_RK32_CHILD Child,
    BOOL Write,
    BOOL LockRequired
    )

/*++

Routine Description:

    This routine performs polled I/O data transfers.

Arguments:

    IrpReadWrite - Supplies a pointer to the IRP read/write context.

    Child - Supplies a pointer to the SD child device.

    Write - Supplies a boolean indicating if this is a read operation (TRUE) or
        a write operation (FALSE).

    LockRequired - Supplies a boolean indicating if this operation requires the
        child's controller lock to be acquired (TRUE) or not (FALSE).

Return Value:

    None.

--*/

{

    UINTN BlockCount;
    ULONGLONG BlockOffset;
    UINTN BytesRemaining;
    UINTN BytesThisRound;
    KSTATUS CompletionStatus;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    ULONG IrpReadWriteFlags;
    BOOL LockHeld;
    BOOL ReadWriteIrpPrepared;
    KSTATUS Status;
    PVOID VirtualAddress;

    IrpReadWrite->IoBytesCompleted = 0;
    LockHeld = FALSE;
    ReadWriteIrpPrepared = FALSE;

    ASSERT(IrpReadWrite->IoBuffer != NULL);
    ASSERT(Child->Type == SdRk32Child);
    ASSERT((Child->BlockCount != 0) && (Child->BlockShift != 0));

    //
    // Validate the supplied I/O buffer is aligned and big enough.
    //

    IrpReadWriteFlags = IRP_READ_WRITE_FLAG_POLLED;
    if (Write != FALSE) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    Status = IoPrepareReadWriteIrp(IrpReadWrite,
                                   1 << Child->BlockShift,
                                   0,
                                   MAX_ULONGLONG,
                                   IrpReadWriteFlags);

    if (!KSUCCESS(Status)) {
        goto PerformBlockIoPolledEnd;
    }

    ReadWriteIrpPrepared = TRUE;

    //
    // Make sure the I/O buffer is mapped before use. SD depends on the buffer
    // being mapped.
    //

    IoBuffer = IrpReadWrite->IoBuffer;
    Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE);
    if (!KSUCCESS(Status)) {
        goto PerformBlockIoPolledEnd;
    }

    //
    // Find the starting fragment based on the current offset.
    //

    IoBufferOffset = MmGetIoBufferCurrentOffset(IoBuffer);
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

    if (LockRequired != FALSE) {
        KeAcquireQueuedLock(Child->ControllerLock);
        LockHeld = TRUE;
    }

    if ((Child->Flags & SD_RK32_CHILD_FLAG_MEDIA_PRESENT) == 0) {
        Status = STATUS_NO_MEDIA;
        goto PerformBlockIoPolledEnd;
    }

    //
    // Loop reading in or writing out each fragment in the I/O buffer.
    //

    BytesRemaining = IrpReadWrite->IoSizeInBytes;

    ASSERT(IS_ALIGNED(BytesRemaining, 1 << Child->BlockShift) != FALSE);
    ASSERT(IS_ALIGNED(IrpReadWrite->IoOffset, 1 << Child->BlockShift) != FALSE);

    BlockOffset = IrpReadWrite->IoOffset >> Child->BlockShift;
    while (BytesRemaining != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        VirtualAddress = Fragment->VirtualAddress + FragmentOffset;
        BytesThisRound = Fragment->Size - FragmentOffset;
        if (BytesRemaining < BytesThisRound) {
            BytesThisRound = BytesRemaining;
        }

        ASSERT(IS_ALIGNED(BytesThisRound, (1 << Child->BlockShift)) != FALSE);

        BlockCount = BytesThisRound >> Child->BlockShift;

        //
        // Make sure the system isn't trying to do I/O off the end of the disk.
        //

        ASSERT(BlockOffset < Child->BlockCount);
        ASSERT(BlockCount >= 1);

        Status = SdBlockIoPolled(Child->Controller,
                                 BlockOffset,
                                 BlockCount,
                                 VirtualAddress,
                                 Write);

        if (!KSUCCESS(Status)) {
            goto PerformBlockIoPolledEnd;
        }

        BlockOffset += BlockCount;
        BytesRemaining -= BytesThisRound;
        IrpReadWrite->IoBytesCompleted += BytesThisRound;
        FragmentOffset += BytesThisRound;
        if (FragmentOffset >= Fragment->Size) {
            FragmentIndex += 1;
            FragmentOffset = 0;
        }
    }

    Status = STATUS_SUCCESS;

PerformBlockIoPolledEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Child->ControllerLock);
    }

    if (ReadWriteIrpPrepared != FALSE) {
        CompletionStatus = IoCompleteReadWriteIrp(IrpReadWrite,
                                                  IrpReadWriteFlags);

        if (!KSUCCESS(CompletionStatus) && KSUCCESS(Status)) {
            Status = CompletionStatus;
        }
    }

    IrpReadWrite->NewIoOffset = IrpReadWrite->IoOffset +
                                IrpReadWrite->IoBytesCompleted;

    return Status;
}

KSTATUS
SdRk32InitializeDma (
    PSD_RK32_CONTEXT Device
    )

/*++

Routine Description:

    This routine initializes DMA support in the RK32 host controller.

Arguments:

    Device - Supplies a pointer to the RK32xx SD device.

Return Value:

    Status code.

--*/

{

    PSD_CONTROLLER Controller;
    PSD_DWC_DMA_DESCRIPTOR Descriptor;
    ULONG IoBufferFlags;
    ULONG Value;

    Controller = Device->Controller;
    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        return STATUS_NO_MEDIA;
    }

    if ((Controller->HostCapabilities & SD_MODE_AUTO_CMD12) == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Create the DMA descriptor table if not already done.
    //

    if (Controller->DmaDescriptorTable == NULL) {
        IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS |
                        IO_BUFFER_FLAG_MAP_NON_CACHED;

        Controller->DmaDescriptorTable = MmAllocateNonPagedIoBuffer(
                                             0,
                                             MAX_ULONG,
                                             4,
                                             SD_RK32_DMA_DESCRIPTOR_TABLE_SIZE,
                                             IoBufferFlags);

        if (Controller->DmaDescriptorTable == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        ASSERT(Controller->DmaDescriptorTable->FragmentCount == 1);
    }

    Descriptor = Controller->DmaDescriptorTable->Fragment[0].VirtualAddress;
    RtlZeroMemory(Descriptor, SD_RK32_DMA_DESCRIPTOR_TABLE_SIZE);

    //
    // Enable DMA in the control register.
    //

    Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
    Value |= SD_DWC_CONTROL_DMA_ENABLE;
    SD_DWC_WRITE_REGISTER(Device, SdDwcControl, Value);

    //
    // Read it to make sure the write stuck.
    //

    Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
    if ((Value & SD_DWC_CONTROL_DMA_ENABLE) == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Enable internal DMA in the bus mode register.
    //

    Value = SD_DWC_READ_REGISTER(Device, SdDwcBusMode);
    Value |= SD_DWC_BUS_MODE_IDMAC_ENABLE;
    SD_DWC_WRITE_REGISTER(Device, SdDwcBusMode, Value);

    //
    // Read it to make sure the write stuck.
    //

    Value = SD_DWC_READ_REGISTER(Device, SdDwcBusMode);
    if ((Value & SD_DWC_BUS_MODE_IDMAC_ENABLE) == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

VOID
SdRk32BlockIoDma (
    PSD_RK32_CONTEXT Device,
    ULONGLONG BlockOffset,
    UINTN BlockCount,
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset,
    BOOL Write,
    PSD_IO_COMPLETION_ROUTINE CompletionRoutine,
    PVOID CompletionContext
    )

/*++

Routine Description:

    This routine performs a block I/O read or write using standard ADMA2.

Arguments:

    Device - Supplies a pointer to the RK32xx controller device.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    IoBuffer - Supplies a pointer to the buffer containing the data to write
        or where the read data should be returned.

    IoBufferOffset - Supplies the offset from the beginning of the I/O buffer
        where this I/O should begin. This is relative to the I/O buffer's
        current offset.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation.

    CompletionRoutine - Supplies a pointer to a function to call when the I/O
        completes.

    CompletionContext - Supplies a context pointer to pass as a parameter to
        the completion routine.

Return Value:

    None. The status of the operation is returned when the completion routine
    is called, which may be during the execution of this function in the case
    of an early failure.

--*/

{

    ULONG BlockLength;
    SD_COMMAND Command;
    PSD_CONTROLLER Controller;
    ULONG DescriptorCount;
    PHYSICAL_ADDRESS DescriptorPhysical;
    UINTN DescriptorSize;
    PSD_DWC_DMA_DESCRIPTOR DmaDescriptor;
    PIO_BUFFER DmaDescriptorTable;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONG TableAddress;
    UINTN TransferSize;
    UINTN TransferSizeRemaining;

    ASSERT(BlockCount != 0);

    Controller = Device->Controller;
    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        Status = STATUS_NO_MEDIA;
        goto BlockIoDmaEnd;
    }

    if (Write != FALSE) {
        if (BlockCount > 1) {
            Command.Command = SdCommandWriteMultipleBlocks;

        } else {
            Command.Command = SdCommandWriteSingleBlock;
        }

        BlockLength = Controller->WriteBlockLength;
        TransferSize = BlockCount * BlockLength;

        ASSERT(TransferSize != 0);

    } else {
        if (BlockCount > 1) {
            Command.Command = SdCommandReadMultipleBlocks;

        } else {
            Command.Command = SdCommandReadSingleBlock;
        }

        BlockLength = Controller->ReadBlockLength;
        TransferSize = BlockCount * BlockLength;

        ASSERT(TransferSize != 0);

    }

    //
    // Get to the correct spot in the I/O buffer.
    //

    IoBufferOffset += MmGetIoBufferCurrentOffset(IoBuffer);
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
    // Fill out the DMA descriptors.
    //

    DmaDescriptorTable = Controller->DmaDescriptorTable;
    DmaDescriptor = DmaDescriptorTable->Fragment[0].VirtualAddress;
    DescriptorPhysical = DmaDescriptorTable->Fragment[0].PhysicalAddress;
    DescriptorCount = 0;
    TransferSizeRemaining = TransferSize;
    while ((TransferSizeRemaining != 0) &&
           (DescriptorCount < SD_RK32_DMA_DESCRIPTOR_COUNT - 1)) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);

        //
        // This descriptor size is going to the the minimum of the total
        // remaining size, the size that can fit in a DMA descriptor, and the
        // remaining size of the fragment.
        //

        DescriptorSize = TransferSizeRemaining;
        if (DescriptorSize > SD_DWC_DMA_DESCRIPTOR_MAX_BUFFER_SIZE) {
            DescriptorSize = SD_DWC_DMA_DESCRIPTOR_MAX_BUFFER_SIZE;
        }

        if (DescriptorSize > (Fragment->Size - FragmentOffset)) {
            DescriptorSize = Fragment->Size - FragmentOffset;
        }

        TransferSizeRemaining -= DescriptorSize;
        PhysicalAddress = Fragment->PhysicalAddress + FragmentOffset;

        //
        // Assert that the buffer is within the first 4GB.
        //

        ASSERT(((ULONG)PhysicalAddress == PhysicalAddress) &&
               ((ULONG)(PhysicalAddress + DescriptorSize) ==
                PhysicalAddress + DescriptorSize));

        DmaDescriptor->Address = PhysicalAddress;
        DmaDescriptor->Size = DescriptorSize;
        DmaDescriptor->Control =
                 SD_DWC_DMA_DESCRIPTOR_CONTROL_OWN |
                 SD_DWC_DMA_DESCRIPTOR_CONTROL_SECOND_ADDRESS_CHAINED |
                 SD_DWC_DMA_DESCRIPTOR_CONTROL_DISABLE_INTERRUPT_ON_COMPLETION;

        if (DescriptorCount == 0) {
            DmaDescriptor->Control |=
                                SD_DWC_DMA_DESCRIPTOR_CONTROL_FIRST_DESCRIPTOR;
        }

        DescriptorPhysical += sizeof(SD_DWC_DMA_DESCRIPTOR);
        DmaDescriptor->NextDescriptor = DescriptorPhysical;
        DmaDescriptor += 1;
        DescriptorCount += 1;
        FragmentOffset += DescriptorSize;
        if (FragmentOffset >= Fragment->Size) {
            FragmentIndex += 1;
            FragmentOffset = 0;
        }
    }

    //
    // Mark the last DMA descriptor as the end of the transfer.
    //

    DmaDescriptor -= 1;
    DmaDescriptor->Control &=
              ~(SD_DWC_DMA_DESCRIPTOR_CONTROL_SECOND_ADDRESS_CHAINED |
                SD_DWC_DMA_DESCRIPTOR_CONTROL_DISABLE_INTERRUPT_ON_COMPLETION);

    DmaDescriptor->Control |= SD_DWC_DMA_DESCRIPTOR_CONTROL_LAST_DESCRIPTOR;
    DmaDescriptor->NextDescriptor = 0;
    RtlMemoryBarrier();
    Command.ResponseType = SD_RESPONSE_R1;
    if ((Controller->Flags & SD_CONTROLLER_FLAG_HIGH_CAPACITY) != 0) {
        Command.CommandArgument = BlockOffset;

    } else {
        Command.CommandArgument = BlockOffset * BlockLength;
    }

    ASSERT((TransferSize - TransferSizeRemaining) <= MAX_ULONG);

    Command.BufferSize = (ULONG)(TransferSize - TransferSizeRemaining);
    Command.BufferVirtual = NULL;
    Command.BufferPhysical = INVALID_PHYSICAL_ADDRESS;
    Command.Write = Write;
    Command.Dma = TRUE;
    Controller->IoCompletionRoutine = CompletionRoutine;
    Controller->IoCompletionContext = CompletionContext;
    Controller->IoRequestSize = Command.BufferSize;
    TableAddress = (ULONG)(DmaDescriptorTable->Fragment[0].PhysicalAddress);
    SD_DWC_WRITE_REGISTER(Device, SdDwcDescriptorBaseAddress, TableAddress);
    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    if (!KSUCCESS(Status)) {
        SdErrorRecovery(Controller);
        Controller->IoCompletionRoutine = NULL;
        Controller->IoCompletionContext = NULL;
        Controller->IoRequestSize = 0;
        goto BlockIoDmaEnd;
    }

    Status = STATUS_SUCCESS;

BlockIoDmaEnd:

    //
    // If this routine failed, call the completion routine back immediately.
    //

    if (!KSUCCESS(Status)) {
        CompletionRoutine(Controller, CompletionContext, 0, Status);
    }

    return;
}

KSTATUS
SdRk32InitializeController (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Phase
    )

/*++

Routine Description:

    This routine performs any controller specific initialization steps.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Phase - Supplies the phase of initialization. Phase 0 happens after the
        initial software reset and Phase 1 happens after the bus width has been
        set to 1 and the speed to 400KHz.

Return Value:

    Status code.

--*/

{

    PSD_RK32_CONTEXT Device;
    ULONG Mask;
    KSTATUS Status;
    ULONG Value;
    ULONG Voltage;

    Device = (PSD_RK32_CONTEXT)Context;

    //
    // Phase 0 is an early initialization phase that happens after the
    // controller has been reset. It is used to gather capabilities and set
    // certain parameters in the hardware.
    //

    if (Phase == 0) {
        Mask = SD_DWC_CONTROL_FIFO_RESET | SD_DWC_CONTROL_CONTROLLER_RESET;
        SD_DWC_WRITE_REGISTER(Device, SdDwcControl, Mask);
        do {
            Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);

        } while ((Value & Mask) != 0);

        //
        // Set the default burst length.
        //

        Value = (SD_DWC_BUS_MODE_BURST_LENGTH_16 <<
                 SD_DWC_BUS_MODE_BURST_LENGTH_SHIFT) |
                SD_DWC_BUS_MODE_FIXED_BURST;

        SD_DWC_WRITE_REGISTER(Device, SdDwcBusMode, Value);

        //
        // Set the default FIFO threshold.
        //

        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcFifoThreshold,
                              SD_DWC_FIFO_THRESHOLD_DEFAULT);

        //
        // Set the default timeout.
        //

        SD_DWC_WRITE_REGISTER(Device, SdDwcTimeout, SD_DWC_TIMEOUT_DEFAULT);

        //
        // Set the voltages based on the supported values supplied when the
        // controller was created.
        //

        Voltage = SD_DWC_READ_REGISTER(Device, SdDwcUhs);
        Voltage &= ~SD_DWC_UHS_VOLTAGE_MASK;
        if ((Controller->Voltages & (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) ==
            (SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34)) {

            Voltage |= SD_DWC_UHS_VOLTAGE_3V3;

        } else if ((Controller->Voltages & SD_VOLTAGE_165_195) ==
                   SD_VOLTAGE_165_195) {

            Voltage |= SD_DWC_UHS_VOLTAGE_1V8;

        } else {
            Status = STATUS_DEVICE_NOT_CONNECTED;
            goto InitializeControllerEnd;
        }

        SD_DWC_WRITE_REGISTER(Device, SdDwcUhs, Voltage);

    //
    // Phase 1 happens right before the initialization command sequence is
    // about to begin. The clock and bus width have been program and the device
    // is just about read to go.
    //

    } else if (Phase == 1) {

        //
        // Turn on the power.
        //

        SD_DWC_WRITE_REGISTER(Device, SdDwcPower, SD_DWC_POWER_ENABLE);

        //
        // Set the interrupt mask, clear any pending state, and enable the
        // interrupts.
        //

        Controller->EnabledInterrupts = SD_DWC_INTERRUPT_DEFAULT_MASK;
        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcInterruptMask,
                              SD_DWC_INTERRUPT_DEFAULT_MASK);

        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_ALL_MASK);

        Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
        Value |= SD_DWC_CONTROL_INTERRUPT_ENABLE;
        SD_DWC_WRITE_REGISTER(Device, SdDwcControl, Value);
    }

    Status = STATUS_SUCCESS;

InitializeControllerEnd:
    return Status;
}

KSTATUS
SdRk32ResetController (
    PSD_CONTROLLER Controller,
    PVOID Context,
    ULONG Flags
    )

/*++

Routine Description:

    This routine performs a soft reset of the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Flags - Supplies a bitmask of reset flags. See SD_RESET_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

{

    PSD_RK32_CONTEXT Device;
    ULONGLONG Frequency;
    ULONG ResetMask;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Device = (PSD_RK32_CONTEXT)Context;
    Frequency = HlQueryTimeCounterFrequency();
    ResetMask = SD_DWC_CONTROL_FIFO_RESET |
                SD_DWC_CONTROL_DMA_RESET;

    if ((Flags & SD_RESET_FLAG_ALL) != 0) {
        ResetMask |= SD_DWC_CONTROL_CONTROLLER_RESET;
    }

    Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
    Value |= ResetMask;
    SD_DWC_WRITE_REGISTER(Device, SdDwcControl, Value);
    Status = STATUS_TIMEOUT;
    Timeout = SdQueryTimeCounter(Controller) + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
        if ((Value & ResetMask) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Wait for the DMA status to clear.
    //

    Status = STATUS_TIMEOUT;
    Timeout = SdQueryTimeCounter(Controller) + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcStatus);
        if ((Value & SD_DWC_STATUS_DMA_REQUEST) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Reset the FIFO again.
    //

    ResetMask = SD_DWC_CONTROL_FIFO_RESET;
    Device = (PSD_RK32_CONTEXT)Context;
    Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
    Value |= ResetMask;
    SD_DWC_WRITE_REGISTER(Device, SdDwcControl, Value);
    Status = STATUS_TIMEOUT;
    Timeout = SdQueryTimeCounter(Controller) + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
        if ((Value & ResetMask) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Reset the internal DMA.
    //

    Value = SD_DWC_READ_REGISTER(Device, SdDwcBusMode);
    Value |= SD_DWC_BUS_MODE_INTERNAL_DMA_RESET;
    SD_DWC_WRITE_REGISTER(Device, SdDwcBusMode, Value);
    Status = STATUS_TIMEOUT;
    Timeout = SdQueryTimeCounter(Controller) + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcBusMode);
        if ((Value & SD_DWC_BUS_MODE_INTERNAL_DMA_RESET) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdRk32SendCommand (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PSD_COMMAND Command
    )

/*++

Routine Description:

    This routine sends the given command to the card.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Command - Supplies a pointer to the command parameters.

Return Value:

    Status code.

--*/

{

    ULONG CommandValue;
    PSD_RK32_CONTEXT Device;
    ULONG Flags;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    Device = (PSD_RK32_CONTEXT)Context;
    SdRk32SetDmaInterrupts(Controller, Device, Command->Dma);

    //
    // If the stop command is being sent, add the flag to make sure the current
    // data transfer stops and that this command does not wait for the previous
    // data to complete. Otherwise, wait for the previous data to complete.
    //

    if (Command->Command == SdCommandStopTransmission) {
        Flags = SD_DWC_COMMAND_STOP_ABORT;

    } else {
        Flags = SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;
        if (Command->Command == SdCommandReset) {
            Flags |= SD_DWC_COMMAND_SEND_INITIALIZATION;
        }

        //
        // Wait for the FIFO to become empty command to complete.
        //

        Timeout = 0;
        Value = SD_DWC_READ_REGISTER(Device, SdDwcStatus);
        if ((Value & SD_DWC_STATUS_FIFO_EMPTY) == 0) {
            Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
            Value |= SD_DWC_CONTROL_FIFO_RESET;
            SD_DWC_WRITE_REGISTER(Device, SdDwcControl, Value);
            Status = STATUS_TIMEOUT;
            do {
                Value = SD_DWC_READ_REGISTER(Device, SdDwcControl);
                if ((Value & SD_DWC_CONTROL_FIFO_RESET) == 0) {
                    Status = STATUS_SUCCESS;
                    break;

                } else if (Timeout == 0) {
                    Timeout = SdQueryTimeCounter(Controller) +
                              Controller->Timeout;
                }

            } while (SdQueryTimeCounter(Controller) <= Timeout);

            if (!KSUCCESS(Status)) {
                goto SendCommandEnd;
            }
        }

        //
        // Also wait for the controller to stop being busy from the last
        // command. This comes into play on writes that use internal DMA. The
        // state machine remains busy despite the transfer completion
        // interrupt.
        //

        if ((Value & SD_DWC_STATUS_DATA_BUSY) != 0) {
            Status = STATUS_TIMEOUT;
            Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
            do {
                Value = SD_DWC_READ_REGISTER(Device, SdDwcStatus);
                if ((Value & SD_DWC_STATUS_DATA_BUSY) == 0) {
                    Status = STATUS_SUCCESS;
                    break;
                }

            } while (SdQueryTimeCounter(Controller) <= Timeout);

            if (!KSUCCESS(Status)) {
                goto SendCommandEnd;
            }
        }
    }

    //
    // Clear any old interrupt status.
    //

    SD_DWC_WRITE_REGISTER(Device,
                          SdDwcInterruptStatus,
                          SD_DWC_INTERRUPT_STATUS_ALL_MASK);

    //
    // Set up the response flags.
    //

    if ((Command->ResponseType & SD_RESPONSE_PRESENT) != 0) {
        if ((Command->ResponseType & SD_RESPONSE_136_BIT) != 0) {
            Flags |= SD_DWC_COMMAND_LONG_RESPONSE;
        }

        Flags |= SD_DWC_COMMAND_RESPONSE_EXPECTED;
    }

    //
    // Set up the remainder of the command flags.
    //

    if ((Command->ResponseType & SD_RESPONSE_VALID_CRC) != 0) {
        Flags |= SD_DWC_COMMAND_CHECK_RESPONSE_CRC;
    }

    //
    // If there's a data buffer, program the block count.
    //

    if (Command->BufferSize != 0) {
        Flags |= SD_DWC_COMMAND_DATA_EXPECTED;
        if (Command->Write != FALSE) {
            Flags |= SD_DWC_COMMAND_WRITE;

        } else {
            Flags |= SD_DWC_COMMAND_READ;
        }

        //
        // If reading or writing multiple blocks, the block size register
        // should be set to the default block size and the byte count should be
        // a multiple of the block size.
        //

        if ((Command->Command == SdCommandReadMultipleBlocks) ||
            (Command->Command == SdCommandWriteMultipleBlocks)) {

            if ((Controller->HostCapabilities & SD_MODE_AUTO_CMD12) != 0) {
                Flags |= SD_DWC_COMMAND_SEND_AUTO_STOP;
            }

            SD_DWC_WRITE_REGISTER(Device, SdDwcBlockSize, SD_RK32_BLOCK_SIZE);
            SD_DWC_WRITE_REGISTER(Device, SdDwcByteCount, Command->BufferSize);

        //
        // Otherwise set the block size to total number of bytes to be
        // processed.
        //

        } else {
            SD_DWC_WRITE_REGISTER(Device, SdDwcBlockSize, Command->BufferSize);
            SD_DWC_WRITE_REGISTER(Device, SdDwcByteCount, Command->BufferSize);
        }
    }

    //
    // Internal DMA better be enabled if this is a DMA command.
    //

    ASSERT((Command->Dma == FALSE) ||
           (((SD_DWC_READ_REGISTER(Device, SdDwcBusMode) &
              SD_DWC_BUS_MODE_IDMAC_ENABLE) != 0) &&
            ((SD_DWC_READ_REGISTER(Device, SdDwcControl) &
              SD_DWC_CONTROL_USE_INTERNAL_DMAC) != 0)));

    //
    // Write the command argument.
    //

    SD_DWC_WRITE_REGISTER(Device,
                          SdDwcCommandArgument,
                          Command->CommandArgument);

    //
    // Set the command and wait for it to be accepted.
    //

    CommandValue = (Command->Command << SD_DWC_COMMAND_INDEX_SHIFT) &
                   SD_DWC_COMMAND_INDEX_MASK;

    CommandValue |= SD_DWC_COMMAND_START |
                    SD_DWC_COMMAND_USE_HOLD_REGISTER |
                    Flags;

    SD_DWC_WRITE_REGISTER(Device, SdDwcCommand, CommandValue);

    //
    // If this was a DMA command, just let it sail away.
    //

    if (Command->Dma != FALSE) {
        Status = STATUS_SUCCESS;
        goto SendCommandEnd;
    }

    ASSERT(Controller->EnabledInterrupts == SD_DWC_INTERRUPT_DEFAULT_MASK);

    Status = STATUS_TIMEOUT;
    Timeout = 0;
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
            Status = STATUS_SUCCESS;
            break;

        } else if (Timeout == 0) {
            Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    //
    // Check the interrupt status.
    //

    Status = STATUS_TIMEOUT;
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcInterruptStatus);
        if ((Value & SD_DWC_INTERRUPT_STATUS_COMMAND_DONE) != 0) {
            Status = STATUS_SUCCESS;
            break;

        } else if (Timeout == 0) {
            Timeout = SdQueryTimeCounter(Controller) + Controller->Timeout;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    if ((Value & SD_DWC_INTERRUPT_STATUS_ERROR_RESPONSE_TIMEOUT) != 0) {
        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_ALL_MASK);

        SdRk32ResetController(Controller,
                              Context,
                              SD_RESET_FLAG_COMMAND_LINE);

        Status = STATUS_TIMEOUT;
        goto SendCommandEnd;

    } else if ((Value & SD_DWC_INTERRUPT_STATUS_COMMAND_ERROR_MASK) != 0) {
        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_ALL_MASK);

        Status = STATUS_DEVICE_IO_ERROR;
        goto SendCommandEnd;
    }

    //
    // Acknowledge the completed command.
    //

    SD_DWC_WRITE_REGISTER(Device,
                          SdDwcInterruptStatus,
                          SD_DWC_INTERRUPT_STATUS_COMMAND_DONE);

    //
    // Get the response if there is one.
    //

    if ((Command->ResponseType & SD_RESPONSE_PRESENT) != 0) {
        if ((Command->ResponseType & SD_RESPONSE_136_BIT) != 0) {
            Command->Response[3] = SD_DWC_READ_REGISTER(Device, SdDwcResponse0);
            Command->Response[2] = SD_DWC_READ_REGISTER(Device, SdDwcResponse1);
            Command->Response[1] = SD_DWC_READ_REGISTER(Device, SdDwcResponse2);
            Command->Response[0] = SD_DWC_READ_REGISTER(Device, SdDwcResponse3);
            if ((Controller->HostCapabilities &
                 SD_MODE_RESPONSE136_SHIFTED) != 0) {

                Command->Response[0] = (Command->Response[0] << 8) |
                                       ((Command->Response[1] >> 24) & 0xFF);

                Command->Response[1] = (Command->Response[1] << 8) |
                                       ((Command->Response[2] >> 24) & 0xFF);

                Command->Response[2] = (Command->Response[2] << 8) |
                                       ((Command->Response[3] >> 24) & 0xFF);

                Command->Response[3] = Command->Response[3] << 8;
            }

        } else {
            Command->Response[0] = SD_DWC_READ_REGISTER(Device, SdDwcResponse0);
        }
    }

    //
    // Read/write the data.
    //

    if (Command->BufferSize != 0) {
        if (Command->Write != FALSE) {
            Status = SdRk32WriteData(Controller,
                                     Context,
                                     Command->BufferVirtual,
                                     Command->BufferSize);

        } else {
            Status = SdRk32ReadData(Controller,
                                    Context,
                                    Command->BufferVirtual,
                                    Command->BufferSize);
        }

        if (!KSUCCESS(Status)) {
            goto SendCommandEnd;
        }
    }

    Status = STATUS_SUCCESS;

SendCommandEnd:
    return Status;
}

KSTATUS
SdRk32GetSetBusWidth (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the controller's bus width. The bus width is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

{

    PSD_RK32_CONTEXT Device;
    ULONG Value;

    Device = (PSD_RK32_CONTEXT)Context;
    if (Set != FALSE) {
        switch (Controller->BusWidth) {
        case 1:
            Value = SD_DWC_CARD_TYPE_1_BIT_WIDTH;
            break;

        case 4:
            Value = SD_DWC_CARD_TYPE_4_BIT_WIDTH;
            break;

        case 8:
            Value = SD_DWC_CARD_TYPE_8_BIT_WIDTH;
            break;

        default:
            RtlDebugPrint("SDRK32: Invalid bus width %d.\n",
                          Controller->BusWidth);

            ASSERT(FALSE);

            return STATUS_INVALID_CONFIGURATION;
        }

        SD_DWC_WRITE_REGISTER(Device, SdDwcCardType, Value);

    } else {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcCardType);
        if ((Value & SD_DWC_CARD_TYPE_8_BIT_WIDTH) != 0) {
            Controller->BusWidth = 8;

        } else if ((Value & SD_DWC_CARD_TYPE_4_BIT_WIDTH) != 0) {
            Controller->BusWidth = 4;

        } else {
            Controller->BusWidth = 1;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdRk32GetSetClockSpeed (
    PSD_CONTROLLER Controller,
    PVOID Context,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the controller's clock speed. The clock speed is
    stored in the controller structure.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Set - Supplies a boolean indicating whether the bus width should be queried
        or set.

Return Value:

    Status code.

--*/

{

    PSD_RK32_CONTEXT Device;

    Device = (PSD_RK32_CONTEXT)Context;
    if (Device->FundamentalClock == 0) {
        return STATUS_INVALID_CONFIGURATION;
    }

    //
    // Getting the clock speed is not implemented as the divisor math might not
    // work out precisely in reverse.
    //

    if (Set == FALSE) {
        return STATUS_NOT_SUPPORTED;
    }

    return SdRk32SetClockSpeed(Device, Controller->ClockSpeed);
}

KSTATUS
SdRk32StopDataTransfer (
    PSD_CONTROLLER Controller,
    PVOID Context
    )

/*++

Routine Description:

    This routine stops any current data transfer on the controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

Return Value:

    Status code.

--*/

{

    SD_COMMAND Command;
    KSTATUS Status;

    //
    // Just send a stop command. This will stop the data transfer by adding in
    // the stop/abort bit into the command register.
    //

    RtlZeroMemory(&Command, sizeof(SD_COMMAND));
    Command.Command = SdCommandStopTransmission;
    Command.ResponseType = SD_RESPONSE_NONE;

    //
    // Attempt to send the abort command until the card enters the transfer
    // state.
    //

    Status = Controller->FunctionTable.SendCommand(Controller,
                                                   Controller->ConsumerContext,
                                                   &Command);

    return Status;
}

KSTATUS
SdRk32ReadData (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine reads polled data from the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Data - Supplies a pointer to the buffer where the data will be read into.

    Size - Supplies the size in bytes. This must be a multiple of four bytes.

Return Value:

    Status code.

--*/

{

    PULONG Buffer32;
    ULONG BusyMask;
    ULONG Count;
    ULONG DataReadyMask;
    BOOL DataTransferOver;
    PSD_RK32_CONTEXT Device;
    ULONG Interrupts;
    ULONG IoIndex;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONGLONG TimeoutTicks;
    ULONG Value;

    ASSERT(IS_ALIGNED(Size, sizeof(ULONG)) != FALSE);

    Device = (PSD_RK32_CONTEXT)Context;
    DataTransferOver = FALSE;
    Buffer32 = (PULONG)Data;
    Size /= sizeof(ULONG);
    TimeoutTicks = HlQueryTimeCounterFrequency() * SD_RK32_TIMEOUT;
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Status = STATUS_SUCCESS;
        Timeout = SdQueryTimeCounter(Controller) + TimeoutTicks;
        do {
            Interrupts = SD_DWC_READ_REGISTER(Device, SdDwcInterruptStatus);
            if (Interrupts != 0) {
                Status = STATUS_SUCCESS;
                break;
            }

        } while (SdQueryTimeCounter(Controller) <= Timeout);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Reset the controller if any error bits are set.
        //

        if ((Interrupts & SD_DWC_INTERRUPT_STATUS_DATA_ERROR_MASK) != 0) {
            SdRk32ResetController(Controller, Context, SD_RESET_FLAG_DATA_LINE);
            return STATUS_DEVICE_IO_ERROR;
        }

        //
        // Check for received data status. If data is ready, the status
        // register holds the number of 32-bit elements to be read.
        //

        DataReadyMask = SD_DWC_INTERRUPT_STATUS_RECEIVE_FIFO_DATA_REQUEST;
        if ((Interrupts & DataReadyMask) != 0) {
            Count = SD_DWC_READ_REGISTER(Device, SdDwcStatus);
            Count = (Count & SD_DWC_STATUS_FIFO_COUNT_MASK) >>
                    SD_DWC_STATUS_FIFO_COUNT_SHIFT;

            if (Count > Size) {
                Count = Size;
            }

            for (IoIndex = 0; IoIndex < Count; IoIndex += 1) {
                *Buffer32 = SD_DWC_READ_REGISTER(Device, SdDwcFifoBase);
                Buffer32 += 1;
            }

            Size -= Count;
            SD_DWC_WRITE_REGISTER(Device,
                                  SdDwcInterruptStatus,
                                  DataReadyMask);
        }

        //
        // Check for the transfer over bit. If it is set, then read the rest of
        // the bytes from the FIFO.
        //

        if ((Interrupts & SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {
            for (IoIndex = 0; IoIndex < Size; IoIndex += 1) {
                *Buffer32 = SD_DWC_READ_REGISTER(Device, SdDwcFifoBase);
                Buffer32 += 1;
            }

            SD_DWC_WRITE_REGISTER(Device,
                                  SdDwcInterruptStatus,
                                  SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER);

            Size = 0;
            DataTransferOver = TRUE;
            break;
        }
    }

    //
    // If the data transfer over interrupt has not yet been seen, wait for it
    // to be asserted.
    //

    if (DataTransferOver == FALSE) {
        Status = STATUS_SUCCESS;
        Timeout = SdQueryTimeCounter(Controller) + TimeoutTicks;
        do {
            Interrupts = SD_DWC_READ_REGISTER(Device, SdDwcInterruptStatus);
            if ((Interrupts &
                 SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {

                Status = STATUS_SUCCESS;
                break;
            }

        } while (SdQueryTimeCounter(Controller) <= Timeout);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER);
    }

    //
    // Wait until the state machine and data stop being busy.
    //

    BusyMask = SD_DWC_STATUS_DATA_STATE_MACHINE_BUSY |
               SD_DWC_STATUS_DATA_BUSY;

    Status = STATUS_SUCCESS;
    Timeout = SdQueryTimeCounter(Controller) + TimeoutTicks;
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcStatus);
        if ((Value & BusyMask) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdRk32WriteData (
    PSD_CONTROLLER Controller,
    PVOID Context,
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine writes polled data to the SD controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Context - Supplies a context pointer passed to the SD/MMC library upon
        creation of the controller.

    Data - Supplies a pointer to the buffer containing the data to write.

    Size - Supplies the size in bytes. This must be a multiple of 4 bytes.

Return Value:

    Status code.

--*/

{

    PULONG Buffer32;
    ULONG BusyMask;
    ULONG Count;
    ULONG DataRequestMask;
    BOOL DataTransferOver;
    PSD_RK32_CONTEXT Device;
    ULONG Interrupts;
    ULONG IoIndex;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONGLONG TimeoutTicks;
    ULONG Value;

    ASSERT(IS_ALIGNED(Size, sizeof(ULONG)) != FALSE);

    Device = (PSD_RK32_CONTEXT)Context;
    DataTransferOver = FALSE;
    Buffer32 = (PULONG)Data;
    Size /= sizeof(ULONG);
    TimeoutTicks = HlQueryTimeCounterFrequency() * SD_RK32_TIMEOUT;
    while (Size != 0) {

        //
        // Get the interrupt status register.
        //

        Status = STATUS_SUCCESS;
        Timeout = SdQueryTimeCounter(Controller) + TimeoutTicks;
        do {
            Interrupts = SD_DWC_READ_REGISTER(Device, SdDwcInterruptStatus);
            if (Interrupts != 0) {
                Status = STATUS_SUCCESS;
                break;
            }

        } while (SdQueryTimeCounter(Controller) <= Timeout);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // Reset the controller if any error bits are set.
        //

        if ((Interrupts & SD_DWC_INTERRUPT_STATUS_DATA_ERROR_MASK) != 0) {
            SdRk32ResetController(Controller, Context, SD_RESET_FLAG_DATA_LINE);
            return STATUS_DEVICE_IO_ERROR;
        }

        //
        // If the controller is ready for data to be written, the number of
        // 4-byte elements consumed in the FIFO is stored in the status
        // register. The available bytes is the total FIFO size minus that
        // amount.
        //

        DataRequestMask = SD_DWC_INTERRUPT_STATUS_TRANSMIT_FIFO_DATA_REQUEST;
        if ((Interrupts & DataRequestMask) != 0) {
            Count = SD_DWC_READ_REGISTER(Device, SdDwcStatus);
            Count = (Count & SD_DWC_STATUS_FIFO_COUNT_MASK) >>
                    SD_DWC_STATUS_FIFO_COUNT_SHIFT;

            Count = (SD_DWC_FIFO_DEPTH / sizeof(ULONG)) - Count;
            if (Count > Size) {
                Count = Size;
            }

            for (IoIndex = 0; IoIndex < Count; IoIndex += 1) {
                SD_DWC_WRITE_REGISTER(Device, SdDwcFifoBase, *Buffer32);
                Buffer32 += 1;
            }

            Size -= Count;
            SD_DWC_WRITE_REGISTER(Device,
                                  SdDwcInterruptStatus,
                                  DataRequestMask);
        }

        //
        // Check for the transfer over bit. If it is set, then exit.
        //

        if ((Interrupts & SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {
            SD_DWC_WRITE_REGISTER(Device,
                                  SdDwcInterruptStatus,
                                  SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER);

            Size = 0;
            DataTransferOver = TRUE;
            break;
        }
    }

    //
    // If the data transfer over interrupt has not yet been seen, wait for it
    // to be asserted.
    //

    if (DataTransferOver == FALSE) {
        Status = STATUS_SUCCESS;
        Timeout = SdQueryTimeCounter(Controller) + TimeoutTicks;
        do {
            Interrupts = SD_DWC_READ_REGISTER(Device, SdDwcInterruptStatus);
            if ((Interrupts &
                 SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER) != 0) {

                Status = STATUS_SUCCESS;
                break;
            }

        } while (SdQueryTimeCounter(Controller) <= Timeout);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcInterruptStatus,
                              SD_DWC_INTERRUPT_STATUS_DATA_TRANSFER_OVER);
    }

    //
    // Wait until the state machine and data stop being busy.
    //

    BusyMask = SD_DWC_STATUS_DATA_STATE_MACHINE_BUSY |
               SD_DWC_STATUS_DATA_BUSY;

    Status = STATUS_SUCCESS;
    Timeout = SdQueryTimeCounter(Controller) + TimeoutTicks;
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcStatus);
        if ((Value & BusyMask) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (SdQueryTimeCounter(Controller) <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

KSTATUS
SdRk32SetClockSpeed (
    PSD_RK32_CONTEXT Device,
    SD_CLOCK_SPEED ClockSpeed
    )

/*++

Routine Description:

    This routine sets the controller's clock speed.

Arguments:

    Device - Supplies a pointer to this SD RK32xx device.

    ClockSpeed - Supplies the desired clock speed in Hertz.

Return Value:

    Status code.

--*/

{

    ULONG Divisor;
    ULONGLONG Frequency;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Value;

    if (Device->FundamentalClock == 0) {
        return STATUS_INVALID_CONFIGURATION;
    }

    Frequency = HlQueryTimeCounterFrequency();

    //
    // Wait for the card to not be busy.
    //

    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcStatus);
        if ((Value & SD_DWC_STATUS_DATA_BUSY) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Disable all clocks.
    //

    SD_DWC_WRITE_REGISTER(Device, SdDwcClockEnable, 0);

    //
    // Send the command to indicate that the clock enable register is being
    // updated.
    //

    Value = SD_DWC_COMMAND_START |
            SD_DWC_COMMAND_UPDATE_CLOCK_REGISTERS |
            SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_DWC_WRITE_REGISTER(Device, SdDwcCommand, Value);
    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Get the appropriate divisor without going over the desired clock speed.
    //

    if (ClockSpeed >= Device->FundamentalClock) {
        Divisor = 0;

    } else {
        Divisor = 2;
        while (Divisor < SD_DWC_MAX_DIVISOR) {
            if ((Device->FundamentalClock / Divisor) <= ClockSpeed) {
                break;
            }

            Divisor += 2;
        }

        Divisor >>= 1;
    }

    SD_DWC_WRITE_REGISTER(Device, SdDwcClockDivider, Divisor);
    SD_DWC_WRITE_REGISTER(Device,
                          SdDwcClockSource,
                          SD_DWC_CLOCK_SOURCE_DIVIDER_0);

    //
    // Send the command to indicate that the clock source and divider are is
    // being updated.
    //

    Value = SD_DWC_COMMAND_START |
            SD_DWC_COMMAND_UPDATE_CLOCK_REGISTERS |
            SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_DWC_WRITE_REGISTER(Device, SdDwcCommand, Value);
    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Enable the clocks in lower power mode.
    //

    SD_DWC_WRITE_REGISTER(Device,
                          SdDwcClockEnable,
                          (SD_DWC_CLOCK_ENABLE_LOW_POWER |
                           SD_DWC_CLOCK_ENABLE_ON));

    //
    // Send the command to indicate that the clock is enable register being
    // updated.
    //

    Value = SD_DWC_COMMAND_START |
            SD_DWC_COMMAND_UPDATE_CLOCK_REGISTERS |
            SD_DWC_COMMAND_WAIT_PREVIOUS_DATA_COMPLETE;

    SD_DWC_WRITE_REGISTER(Device, SdDwcCommand, Value);
    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + (Frequency * SD_RK32_TIMEOUT);
    do {
        Value = SD_DWC_READ_REGISTER(Device, SdDwcCommand);
        if ((Value & SD_DWC_COMMAND_START) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

VOID
SdRk32SetDmaInterrupts (
    PSD_CONTROLLER Controller,
    PSD_RK32_CONTEXT Device,
    BOOL Enable
    )

/*++

Routine Description:

    This routine enables or disables interrupts necessary to perform block I/O
    via DMA. It is assumed that the caller has synchronized disk access on this
    controller and there are currently no DMA or polled operations in flight.

Arguments:

    Controller - Supplies a pointer to the controller.

    Device - Supplies a pointer to the RK32xx SD device context.

    Enable - Supplies a boolean indicating if the DMA interrupts are to be
        enabled (TRUE) or disabled (FALSE).

Return Value:

    None.

--*/

{

    ULONG Flags;

    Flags = Controller->Flags;

    //
    // Enable the interrupts for transfer completion so that DMA operations
    // can complete asynchronously. Unless, of course, the DMA interrupts are
    // already enabled.
    //

    if (Enable != FALSE) {
        if ((Flags & SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED) != 0) {
            return;
        }

        Controller->EnabledInterrupts |=
                                     SD_DWC_INTERRUPT_MASK_DATA_TRANSFER_OVER |
                                     SD_DWC_INTERRUPT_ERROR_MASK;

        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcInterruptMask,
                              Controller->EnabledInterrupts);

        RtlAtomicOr32(&(Controller->Flags),
                      SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED);

    //
    // Disable the DMA interrupts so that they do not interfere with polled I/O
    // attempts to check the transfer status. Do nothing if the DMA interrupts
    // are disabled.
    //

    } else {
        if ((Flags & SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED) == 0) {
            return;
        }

        Controller->EnabledInterrupts &=
                                   ~(SD_DWC_INTERRUPT_MASK_DATA_TRANSFER_OVER |
                                     SD_DWC_INTERRUPT_ERROR_MASK);

        SD_DWC_WRITE_REGISTER(Device,
                              SdDwcInterruptMask,
                              Controller->EnabledInterrupts);

        RtlAtomicAnd32(&(Controller->Flags),
                       ~SD_CONTROLLER_FLAG_DMA_INTERRUPTS_ENABLED);
    }

    return;
}

