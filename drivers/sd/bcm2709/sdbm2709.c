/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sdbm2709.c

Abstract:

    This module implements the SD/MMC driver for BCM2709 SoCs.

Author:

    Chris Stevens 10-Dec-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/disk.h>
#include <minoca/sd/sd.h>
#include <minoca/dma/dma.h>
#include <minoca/dma/dmab2709.h>
#include "emmc.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the set of flags for the SD disk.
//

#define SD_BCM2709_DISK_FLAG_DMA_SUPPORTED 0x00000001

//
// Define the mask and value for the upper byte of the physical addresses that
// must be supplied to the DMA controller.
//

#define SD_BCM2709_DEVICE_ADDRESS_MASK  0xFF000000
#define SD_BCM2709_DEVICE_ADDRESS_VALUE 0x7E000000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SD_BCM2385_DEVICE_TYPE {
    SdBcm2709DeviceInvalid,
    SdBcm2709DeviceBus,
    SdBcm2709DeviceSlot,
    SdBcm2709DeviceDisk
} SD_BCM2709_DEVICE_TYPE, *PSD_BCM2709_DEVICE_TYPE;

typedef struct _SD_BCM2709_BUS SD_BCM2709_BUS, *PSD_BCM2709_BUS;
typedef struct _SD_BCM2709_SLOT SD_BCM2709_SLOT, *PSD_BCM2709_SLOT;

/*++

Structure Description:

    This structure describes an SD/MMC disk context (the context used by the
    bus driver for the disk device).

Members:

    Type - Stores the type identifying this as an SD disk structure.

    ReferenceCount - Stores a reference count for the disk.

    Device - Stores a pointer to the OS device for the disk.

    Parent - Stores a pointer to the parent slot.

    Controller - Stores a pointer to the SD controller structure.

    ControllerLock - Stores a pointer to a lock used to serialize access to the
        controller.

    Irp - Stores a pointer to the current IRP being processed.

    Flags - Stores a bitmask of flags. See SD_BCM2709_DISK_FLAG_* for
        definitions.

    BlockShift - Stores the block size shift of the disk.

    BlockCount - Stores the number of blocks on the disk.

    DiskInterface - Stores the disk interface presented to the system.

    RemainingInterrupts - Stores the count of remaining interrupts expected to
        come in before the transfer is complete.

--*/

typedef struct _SD_BCM2709_DISK {
    SD_BCM2709_DEVICE_TYPE Type;
    volatile ULONG ReferenceCount;
    PDEVICE Device;
    PSD_BCM2709_SLOT Parent;
    PSD_CONTROLLER Controller;
    PQUEUED_LOCK ControllerLock;
    PIRP Irp;
    ULONG Flags;
    ULONG BlockShift;
    ULONGLONG BlockCount;
    DISK_INTERFACE DiskInterface;
    volatile ULONG RemainingInterrupts;
} SD_BCM2709_DISK, *PSD_BCM2709_DISK;

/*++

Structure Description:

    This structure describes an SD/MMC slot (the context used by the bus driver
    for the individual SD slot).

Members:

    Type - Stores the type identifying this as an SD slot.

    Device - Stores a pointer to the OS device for the slot.

    Controller - Stores a pointer to the SD controller structure.

    ControllerBase - Stores the virtual address of the base of the controller
        registers.

    Resource - Stores a pointer to the resource describing the location of the
        controller.

    Parent - Stores a pointer back to the parent.

    Disk - Stores a pointer to the child disk context.

    Lock - Stores a pointer to a lock used to serialize access to the
        controller.

    DmaResource - Stores a pointer to the DMA resource.

    Dma - Stores a pointer to the DMA interface.

    DmaTransfer - Stores a pointer to the DMA transfer used on I/O.

--*/

struct _SD_BCM2709_SLOT {
    SD_BCM2709_DEVICE_TYPE Type;
    PDEVICE Device;
    PSD_CONTROLLER Controller;
    PVOID ControllerBase;
    PRESOURCE_ALLOCATION Resource;
    PSD_BCM2709_BUS Parent;
    PSD_BCM2709_DISK Disk;
    PQUEUED_LOCK Lock;
    PRESOURCE_ALLOCATION DmaResource;
    PDMA_INTERFACE Dma;
    PDMA_TRANSFER DmaTransfer;
};

/*++

Structure Description:

    This structure describes an SD/MMC driver context (the function driver
    context for the SD bus controller).

Members:

    Type - Stores the type identifying this as an SD controller.

    Slots - Stores the array of SD slots.

    Handle - Stores the connected interrupt handle.

    InterruptLine - Stores the interrupt line of the controller.

    InterruptVector - Stores the interrupt vector of the controller.

    InterruptResourcesFound - Stores a boolean indicating whether or not
        interrupt resources were located for this device.

--*/

struct _SD_BCM2709_BUS {
    SD_BCM2709_DEVICE_TYPE Type;
    SD_BCM2709_SLOT Slot;
    HANDLE InterruptHandle;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
SdBcm2709AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
SdBcm2709DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdBcm2709DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdBcm2709DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdBcm2709DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdBcm2709DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
SdBcm2709BusInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
SdBcm2709BusInterruptServiceDispatch (
    PVOID Context
    );

VOID
SdBcm2709pBusDispatchStateChange (
    PIRP Irp,
    PSD_BCM2709_BUS Bus
    );

VOID
SdBcm2709pSlotDispatchStateChange (
    PIRP Irp,
    PSD_BCM2709_SLOT Slot
    );

VOID
SdBcm2709pDiskDispatchStateChange (
    PIRP Irp,
    PSD_BCM2709_DISK Disk
    );

KSTATUS
SdBcm2709pBusProcessResourceRequirements (
    PIRP Irp,
    PSD_BCM2709_BUS Bus
    );

KSTATUS
SdBcm2709pBusStartDevice (
    PIRP Irp,
    PSD_BCM2709_BUS Bus
    );

KSTATUS
SdBcm2709pBusQueryChildren (
    PIRP Irp,
    PSD_BCM2709_BUS Context
    );

KSTATUS
SdBcm2709pSlotStartDevice (
    PIRP Irp,
    PSD_BCM2709_SLOT Slot
    );

KSTATUS
SdBcm2709pSlotQueryChildren (
    PIRP Irp,
    PSD_BCM2709_SLOT Slot
    );

PSD_BCM2709_DISK
SdBcm2709pCreateDisk (
    PSD_BCM2709_SLOT Slot
    );

VOID
SdBcm2709pDestroyDisk (
    PSD_BCM2709_DISK Disk
    );

VOID
SdBcm2709pDiskAddReference (
    PSD_BCM2709_DISK Disk
    );

VOID
SdBcm2709pDiskReleaseReference (
    PSD_BCM2709_DISK Disk
    );

KSTATUS
SdBcm2709pDiskBlockIoReset (
    PVOID DiskToken
    );

KSTATUS
SdBcm2709pDiskBlockIoRead (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
SdBcm2709pDiskBlockIoWrite (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
SdBcm2709pPerformIoPolled (
    PIRP_READ_WRITE IrpReadWrite,
    PSD_BCM2709_DISK Disk,
    BOOL Write,
    BOOL LockRequired
    );

KSTATUS
SdBcm2709pInitializeDma (
    PSD_BCM2709_SLOT Slot
    );

VOID
SdBcm2709pDmaInterfaceCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

VOID
SdBcm2709pPerformDmaIo (
    PSD_BCM2709_DISK Disk,
    PIRP Irp
    );

VOID
SdBcm2709pSdDmaCompletion (
    PSD_CONTROLLER Controller,
    PVOID Context,
    UINTN BytesTransferred,
    KSTATUS Status
    );

KSTATUS
SdBcm2709pSetupSystemDma (
    PSD_BCM2709_DISK Child
    );

VOID
SdBcm2709pSystemDmaCompletion (
    PDMA_TRANSFER Transfer
    );

VOID
SdBcm2709pDmaCompletion (
    PSD_CONTROLLER Controller,
    PVOID Context,
    UINTN BytesTransferred,
    KSTATUS Status
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER SdBcm2709Driver = NULL;
UUID SdBcm2709DiskInterfaceUuid = UUID_DISK_INTERFACE;
UUID SdBcm2709DmaUuid = UUID_DMA_INTERFACE;
UUID SdBcm2709DmaBcm2709Uuid = UUID_DMA_BCM2709_CONTROLLER;

DISK_INTERFACE SdBcm2709DiskInterfaceTemplate = {
    DISK_INTERFACE_VERSION,
    NULL,
    0,
    0,
    NULL,
    SdBcm2709pDiskBlockIoReset,
    SdBcm2709pDiskBlockIoRead,
    SdBcm2709pDiskBlockIoWrite
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

    SdBcm2709Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = SdBcm2709AddDevice;
    FunctionTable.DispatchStateChange = SdBcm2709DispatchStateChange;
    FunctionTable.DispatchOpen = SdBcm2709DispatchOpen;
    FunctionTable.DispatchClose = SdBcm2709DispatchClose;
    FunctionTable.DispatchIo = SdBcm2709DispatchIo;
    FunctionTable.DispatchSystemControl = SdBcm2709DispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
SdBcm2709AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
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

    PSD_BCM2709_BUS Context;
    PSD_BCM2709_SLOT Slot;
    KSTATUS Status;

    Context = MmAllocateNonPagedPool(sizeof(SD_BCM2709_BUS), SD_ALLOCATION_TAG);
    if (Context == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Context, sizeof(SD_BCM2709_BUS));
    Context->Type = SdBcm2709DeviceBus;
    Context->InterruptHandle = INVALID_HANDLE;
    Slot = &(Context->Slot);
    Slot->Type = SdBcm2709DeviceSlot;
    Slot->Parent = Context;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, Context);
    if (!KSUCCESS(Status)) {
        MmFreeNonPagedPool(Context);
    }

    return Status;
}

VOID
SdBcm2709DispatchStateChange (
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

    PSD_BCM2709_BUS Context;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Context = (PSD_BCM2709_BUS)DeviceContext;
    switch (Context->Type) {
    case SdBcm2709DeviceBus:
        SdBcm2709pBusDispatchStateChange(Irp, Context);
        break;

    case SdBcm2709DeviceSlot:
        SdBcm2709pSlotDispatchStateChange(Irp, (PSD_BCM2709_SLOT)Context);
        break;

    case SdBcm2709DeviceDisk:
        SdBcm2709pDiskDispatchStateChange(Irp, (PSD_BCM2709_DISK)Context);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
SdBcm2709DispatchOpen (
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

    PSD_BCM2709_DISK Disk;

    Disk = DeviceContext;
    if (Disk->Type != SdBcm2709DeviceDisk) {
        return;
    }

    SdBcm2709pDiskAddReference(Disk);
    IoCompleteIrp(SdBcm2709Driver, Irp, STATUS_SUCCESS);
    return;
}

VOID
SdBcm2709DispatchClose (
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

    PSD_BCM2709_DISK Disk;

    Disk = DeviceContext;
    if (Disk->Type != SdBcm2709DeviceDisk) {
        return;
    }

    SdBcm2709pDiskReleaseReference(Disk);
    IoCompleteIrp(SdBcm2709Driver, Irp, STATUS_SUCCESS);
    return;
}

VOID
SdBcm2709DispatchIo (
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
    PSD_CONTROLLER Controller;
    PSD_BCM2709_DISK Disk;
    ULONG IrpReadWriteFlags;
    KSTATUS IrpStatus;
    KSTATUS Status;
    BOOL Write;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Disk = DeviceContext;
    if (Disk->Type != SdBcm2709DeviceDisk) {

        ASSERT(FALSE);

        return;
    }

    Controller = Disk->Controller;
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

    if ((Disk->Flags & SD_BCM2709_DISK_FLAG_DMA_SUPPORTED) == 0) {

        ASSERT(Irp->Direction == IrpDown);

        Status = SdBcm2709pPerformIoPolled(&(Irp->U.ReadWrite),
                                           Disk,
                                           Write,
                                           TRUE);

        goto DispatchIoEnd;
    }

    //
    // Set the IRP read/write flags for the preparation and completion steps.
    //

    IrpReadWriteFlags = IRP_READ_WRITE_FLAG_DMA;
    if (Write != FALSE) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    if (Irp->Direction == IrpDown) {
        Controller->Try = 0;
    }

    //
    // If the IRP is on the way up, then clean up after the DMA as this IRP is
    // still sitting in the channel. An IRP going up is already complete.
    //

    if (Irp->Direction == IrpUp) {

        ASSERT(Irp == Disk->Irp);

        Disk->Irp = NULL;

        //
        // Try to recover on failure.
        //

        IrpStatus = IoGetIrpStatus(Irp);
        if (!KSUCCESS(IrpStatus)) {
            Status = SdErrorRecovery(Controller);
            if (!KSUCCESS(Status)) {
                IrpStatus = Status;
                IoUpdateIrpStatus(Irp, IrpStatus);
            }

            //
            // Do not make further attempts if the media is gone or enough
            // attempts have been made.
            //

            if (((Controller->Flags &
                  SD_CONTROLLER_FLAG_MEDIA_CHANGED) != 0) ||
                ((Controller->Flags &
                  SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) ||
                (Controller->Try >= SD_MAX_IO_RETRIES)) {

                IrpStatus = STATUS_SUCCESS;

            } else {
                Controller->Try += 1;
            }
        }

        KeReleaseQueuedLock(Disk->ControllerLock);
        Status = IoCompleteReadWriteIrp(&(Irp->U.ReadWrite),
                                        IrpReadWriteFlags);

        if (!KSUCCESS(Status)) {
            IoUpdateIrpStatus(Irp, Status);
        }

        //
        // Potentially return the completed IRP.
        //

        if (KSUCCESS(IrpStatus)) {
            CompleteIrp = FALSE;
            goto DispatchIoEnd;
        }
    }

    //
    // Start the DMA on the way down.
    //

    Irp->U.ReadWrite.IoBytesCompleted = 0;
    Irp->U.ReadWrite.NewIoOffset = Irp->U.ReadWrite.IoOffset;

    ASSERT(Irp->U.ReadWrite.IoBuffer != NULL);
    ASSERT((Disk->BlockCount != 0) && (Disk->BlockShift != 0));
    ASSERT(IS_ALIGNED(Irp->U.ReadWrite.IoOffset,
                      1 << Disk->BlockShift) != FALSE);

    ASSERT(IS_ALIGNED(Irp->U.ReadWrite.IoSizeInBytes,
                      1 << Disk->BlockShift) != FALSE);

    //
    // Before acquiring the controller's lock and starting the DMA, prepare
    // the I/O context for SD (i.e. it must use physical addresses that
    // are less than 4GB and be sector size aligned).
    //

    Status = IoPrepareReadWriteIrp(&(Irp->U.ReadWrite),
                                   1 << Disk->BlockShift,
                                   0,
                                   MAX_ULONG,
                                   IrpReadWriteFlags);

    if (!KSUCCESS(Status)) {
        goto DispatchIoEnd;
    }

    //
    // Lock the controller to serialize access to the hardware.
    //

    KeAcquireQueuedLock(Disk->ControllerLock);
    if (((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) ||
        ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_CHANGED) != 0)) {

        Status = STATUS_NO_MEDIA;
        if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_CHANGED) != 0) {
            Status = STATUS_MEDIA_CHANGED;
        }

        KeReleaseQueuedLock(Disk->ControllerLock);
        IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
        goto DispatchIoEnd;
    }

    Disk->Irp = Irp;
    CompleteIrp = FALSE;
    IoPendIrp(SdBcm2709Driver, Irp);
    SdBcm2709pPerformDmaIo(Disk, Irp);

    //
    // DMA transfers are self perpetuating, so after kicking off this
    // first transfer, return. This returns with the lock held because
    // I/O is still in progress.
    //

    ASSERT(KeIsQueuedLockHeld(Disk->ControllerLock) != FALSE);

DispatchIoEnd:
    if (CompleteIrp != FALSE) {
        IoCompleteIrp(SdBcm2709Driver, Irp, Status);
    }

    return;
}

VOID
SdBcm2709DispatchSystemControl (
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
    PSD_BCM2709_DISK Disk;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    ULONGLONG PropertiesFileSize;
    KSTATUS Status;

    Context = Irp->U.SystemControl.SystemContext;
    Disk = DeviceContext;

    //
    // Only disk devices are supported.
    //

    if (Disk->Type != SdBcm2709DeviceDisk) {
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
            Properties->BlockCount = Disk->BlockCount;
            Properties->BlockSize = 1 << Disk->BlockShift;
            Properties->Size = Disk->BlockCount << Disk->BlockShift;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(SdBcm2709Driver, Irp, Status);
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
            (Properties->BlockSize != (1 << Disk->BlockShift)) ||
            (Properties->BlockCount != Disk->BlockCount) ||
            (PropertiesFileSize != (Disk->BlockCount << Disk->BlockShift))) {

            Status = STATUS_NOT_SUPPORTED;

        } else {
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(SdBcm2709Driver, Irp, Status);
        break;

    //
    // Do not support hard disk device truncation.
    //

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(SdBcm2709Driver, Irp, STATUS_NOT_SUPPORTED);
        break;

    //
    // Gather and return device information.
    //

    case IrpMinorSystemControlDeviceInformation:
        break;

    case IrpMinorSystemControlSynchronize:
        IoCompleteIrp(SdBcm2709Driver, Irp, STATUS_SUCCESS);
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

INTERRUPT_STATUS
SdBcm2709BusInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the interrupt service routine for an SD bus.

Arguments:

    Context - Supplies a pointer to the device context.

Return Value:

    Returns whether or not the SD controller caused the interrupt.

--*/

{

    PSD_BCM2709_BUS Bus;
    PSD_BCM2709_SLOT Slot;
    INTERRUPT_STATUS Status;

    Bus = Context;
    Slot = &(Bus->Slot);
    Status = InterruptStatusNotClaimed;
    if (Slot->Controller != NULL) {
        Status = SdStandardInterruptService(Slot->Controller);
    }

    return Status;
}

INTERRUPT_STATUS
SdBcm2709BusInterruptServiceDispatch (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the dispatch level interrupt service routine for an
    SD bus.

Arguments:

    Context - Supplies a pointer to the device context.

Return Value:

    Returns whether or not the SD controller caused the interrupt.

--*/

{

    PSD_BCM2709_BUS Bus;
    PSD_BCM2709_SLOT Slot;
    INTERRUPT_STATUS Status;

    Bus = Context;
    Slot = &(Bus->Slot);
    Status = InterruptStatusNotClaimed;
    if (Slot->Controller != NULL) {
        Status = SdStandardInterruptServiceDispatch(Slot->Controller);
    }

    return Status;
}

VOID
SdBcm2709pBusDispatchStateChange (
    PIRP Irp,
    PSD_BCM2709_BUS Bus
    )

/*++

Routine Description:

    This routine handles State Change IRPs for the SD bus device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Bus - Supplies a pointer to the SD bus.

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
            Status = SdBcm2709pBusProcessResourceRequirements(Irp, Bus);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdBcm2709Driver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = SdBcm2709pBusStartDevice(Irp, Bus);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdBcm2709Driver, Irp, Status);
            }

            break;

        case IrpMinorQueryChildren:
            Status = SdBcm2709pBusQueryChildren(Irp, Bus);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdBcm2709Driver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
SdBcm2709pSlotDispatchStateChange (
    PIRP Irp,
    PSD_BCM2709_SLOT Slot
    )

/*++

Routine Description:

    This routine handles State Change IRPs for the SD bus device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Slot - Supplies a pointer to the SD slot.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Actively handle IRPs as the bus driver for the slot.
    //

    if (Irp->Direction == IrpDown) {
        switch (Irp->MinorCode) {
        case IrpMinorStartDevice:
            Status = SdBcm2709pSlotStartDevice(Irp, Slot);
            IoCompleteIrp(SdBcm2709Driver, Irp, Status);
            break;

        case IrpMinorQueryResources:
            IoCompleteIrp(SdBcm2709Driver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorQueryChildren:
            Status = SdBcm2709pSlotQueryChildren(Irp, Slot);
            IoCompleteIrp(SdBcm2709Driver, Irp, Status);
            break;

        default:
            break;
        }
    }

    return;
}

VOID
SdBcm2709pDiskDispatchStateChange (
    PIRP Irp,
    PSD_BCM2709_DISK Disk
    )

/*++

Routine Description:

    This routine handles State Change IRPs for a parent device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Disk - Supplies a pointer to the SD disk.

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

            //
            // Publish the disk interface.
            //

            Status = STATUS_SUCCESS;
            if (Disk->DiskInterface.DiskToken == NULL) {
                RtlCopyMemory(&(Disk->DiskInterface),
                              &SdBcm2709DiskInterfaceTemplate,
                              sizeof(DISK_INTERFACE));

                Disk->DiskInterface.DiskToken = Disk;
                Disk->DiskInterface.BlockSize = 1 << Disk->BlockShift;
                Disk->DiskInterface.BlockCount = Disk->BlockCount;
                Status = IoCreateInterface(&SdBcm2709DiskInterfaceUuid,
                                           Disk->Device,
                                           &(Disk->DiskInterface),
                                           sizeof(DISK_INTERFACE));

                if (!KSUCCESS(Status)) {
                    Disk->DiskInterface.DiskToken = NULL;
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
            if (Disk->DiskInterface.DiskToken != NULL) {
                Status = IoDestroyInterface(&SdBcm2709DiskInterfaceUuid,
                                            Disk->Device,
                                            &(Disk->DiskInterface));

                ASSERT(KSUCCESS(Status));

                Disk->DiskInterface.DiskToken = NULL;
            }

            SdBcm2709pDiskReleaseReference(Disk);
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
            IoCompleteIrp(SdBcm2709Driver, Irp, Status);
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
SdBcm2709pBusProcessResourceRequirements (
    PIRP Irp,
    PSD_BCM2709_BUS Bus
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a SD Bus controller. It adds an interrupt vector requirement
    for any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Bus - Supplies a pointer to the bus context.

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
        goto BusProcessResourceRequirementsEnd;
    }

BusProcessResourceRequirementsEnd:
    return Status;
}

KSTATUS
SdBcm2709pBusStartDevice (
    PIRP Irp,
    PSD_BCM2709_BUS Bus
    )

/*++

Routine Description:

    This routine starts an SD bus device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Bus - Supplies a pointer to the bus context.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION LineAllocation;
    KSTATUS Status;

    ASSERT(Bus->Slot.Controller == NULL);
    ASSERT(Bus->Slot.Resource == NULL);
    ASSERT(Bus->Slot.DmaResource == NULL);

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

            ASSERT(Bus->InterruptResourcesFound == FALSE);
            ASSERT(Allocation->OwningAllocation != NULL);

            //
            // Save the line and vector number.
            //

            LineAllocation = Allocation->OwningAllocation;
            Bus->InterruptLine = LineAllocation->Allocation;
            Bus->InterruptVector = Allocation->Allocation;
            Bus->InterruptResourcesFound = TRUE;

        } else if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            if ((Bus->Slot.Resource == NULL) && (Allocation->Length > 0)) {
                Bus->Slot.Resource = Allocation;
            }

        } else if (Allocation->Type == ResourceTypeDmaChannel) {
            if (Bus->Slot.DmaResource == NULL) {
                Bus->Slot.DmaResource = Allocation;
            }
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Attempt to connect the interrupt.
    //

    if (Bus->InterruptHandle == INVALID_HANDLE) {
        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Bus->InterruptLine;
        Connect.Vector = Bus->InterruptVector;
        Connect.InterruptServiceRoutine = SdBcm2709BusInterruptService;
        Connect.DispatchServiceRoutine = SdBcm2709BusInterruptServiceDispatch;
        Connect.Context = Bus;
        Connect.Interrupt = &(Bus->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    Status = STATUS_SUCCESS;

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Bus->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Bus->InterruptHandle);
            Bus->InterruptHandle = INVALID_HANDLE;
        }
    }

    return Status;
}

KSTATUS
SdBcm2709pBusQueryChildren (
    PIRP Irp,
    PSD_BCM2709_BUS Context
    )

/*++

Routine Description:

    This routine handles State Change IRPs for the SD bus device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Context - Supplies a pointer to the bus context.

Return Value:

    Status code.

--*/

{

    PSD_BCM2709_SLOT Slot;
    KSTATUS Status;

    Slot = &(Context->Slot);
    if (Slot->Resource == NULL) {
        return STATUS_SUCCESS;
    }

    if (Slot->Device == NULL) {
        Status = IoCreateDevice(SdBcm2709Driver,
                                Slot,
                                Irp->Device,
                                SD_SLOT_DEVICE_ID,
                                NULL,
                                NULL,
                                &(Slot->Device));

        if (!KSUCCESS(Status)) {
            goto BusQueryChildrenEnd;
        }
    }

    ASSERT(Slot->Device != NULL);

    Status = IoMergeChildArrays(Irp,
                                &(Slot->Device),
                                1,
                                SD_ALLOCATION_TAG);

    if (!KSUCCESS(Status)) {
        goto BusQueryChildrenEnd;
    }

BusQueryChildrenEnd:
    return Status;
}

KSTATUS
SdBcm2709pSlotStartDevice (
    PIRP Irp,
    PSD_BCM2709_SLOT Slot
    )

/*++

Routine Description:

    This routine starts an SD slot device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Slot - Supplies a pointer to the slot context.

Return Value:

    Status code.

--*/

{

    ULONG Frequency;
    SD_INITIALIZATION_BLOCK Parameters;
    KSTATUS Status;

    ASSERT(Slot->Resource != NULL);

    //
    // Initialize the controller base.
    //

    if (Slot->ControllerBase == NULL) {
        Slot->ControllerBase = MmMapPhysicalAddress(Slot->Resource->Allocation,
                                                    Slot->Resource->Length,
                                                    TRUE,
                                                    FALSE,
                                                    TRUE);

        if (Slot->ControllerBase == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartDeviceEnd;
        }
    }

    if (Slot->Lock == NULL) {
        Slot->Lock = KeCreateQueuedLock();
        if (Slot->Lock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartDeviceEnd;
        }
    }

    //
    // Try to fire up system DMA.
    //

    if ((Slot->DmaResource != NULL) && (Slot->Dma == NULL)) {
        Status = SdBcm2709pInitializeDma(Slot);
        if (!KSUCCESS(Status)) {
            Slot->DmaResource = NULL;
        }
    }

    //
    // Initialize the standard SD controller.
    //

    if (Slot->Controller == NULL) {

        //
        // Power on the BCM2709's Emmc.
        //

        Status = Bcm2709EmmcInitialize();
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Status = Bcm2709EmmcGetClockFrequency(&Frequency);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        RtlZeroMemory(&Parameters, sizeof(SD_INITIALIZATION_BLOCK));
        Parameters.ConsumerContext = Slot;
        Parameters.StandardControllerBase = Slot->ControllerBase;
        Parameters.Voltages = SD_VOLTAGE_32_33 | SD_VOLTAGE_33_34;
        Parameters.HostCapabilities = SD_MODE_AUTO_CMD12 |
                                      SD_MODE_4BIT |
                                      SD_MODE_RESPONSE136_SHIFTED |
                                      SD_MODE_HIGH_SPEED |
                                      SD_MODE_HIGH_SPEED_52MHZ |
                                      SD_MODE_CMD23;

        if (Slot->Dma != NULL) {
            Parameters.HostCapabilities |= SD_MODE_SYSTEM_DMA;
        }

        Parameters.FundamentalClock = Frequency;
        Parameters.OsDevice = Slot->Device;
        Slot->Controller = SdCreateController(&Parameters);
        if (Slot->Controller == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartDeviceEnd;
        }

        Slot->Controller->InterruptHandle = Slot->Parent->InterruptHandle;
    }

    Status = STATUS_SUCCESS;

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Slot->Lock != NULL) {
            KeDestroyQueuedLock(Slot->Lock);
        }

        if (Slot->Controller != NULL) {
            SdDestroyController(Slot->Controller);
            Slot->Controller = NULL;
        }
    }

    return Status;
}

KSTATUS
SdBcm2709pSlotQueryChildren (
    PIRP Irp,
    PSD_BCM2709_SLOT Slot
    )

/*++

Routine Description:

    This routine potentially enumerates an SD card in a given slot.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Slot - Supplies a pointer to the SD slot that may contain the card.

Return Value:

    Status code.

--*/

{

    ULONG BlockSize;
    PSTR DeviceId;
    ULONG FlagsMask;
    PSD_BCM2709_DISK NewDisk;
    ULONG OldFlags;
    KSTATUS Status;

    NewDisk = NULL;

    //
    // The Broadcom SD chip does not currently support device insertion and
    // removal, but at least handle it here for the initial query.
    //

    FlagsMask = ~(SD_CONTROLLER_FLAG_INSERTION_PENDING |
                  SD_CONTROLLER_FLAG_REMOVAL_PENDING);

    OldFlags = RtlAtomicAnd32(&(Slot->Controller->Flags), FlagsMask);

    //
    // If either insertion or removal is pending, remove the existing disk. In
    // practice, an insertion can occur without the previous removal.
    //

    FlagsMask = SD_CONTROLLER_FLAG_INSERTION_PENDING |
                SD_CONTROLLER_FLAG_REMOVAL_PENDING;

    if ((OldFlags & FlagsMask) != 0) {
        if (Slot->Disk != NULL) {
            KeAcquireQueuedLock(Slot->Lock);
            RtlAtomicAnd32(&(Slot->Disk->Controller->Flags),
                           ~SD_CONTROLLER_FLAG_MEDIA_PRESENT);

            KeReleaseQueuedLock(Slot->Lock);
            Slot->Disk = NULL;
        }
    }

    //
    // If an insertion is pending, try to enumerate the new disk.
    //

    if ((OldFlags & SD_CONTROLLER_FLAG_INSERTION_PENDING) != 0) {

        ASSERT(Slot->Disk == NULL);

        //
        // Initialize the controller to see if a disk is actually present.
        //

        RtlAtomicAnd32(&(Slot->Controller->Flags),
                       ~SD_CONTROLLER_FLAG_MEDIA_CHANGED);

        Status = SdInitializeController(Slot->Controller, TRUE);
        if (!KSUCCESS(Status)) {
            if (Status == STATUS_TIMEOUT) {
                Status = STATUS_SUCCESS;
            }

            goto SlotQueryChildrenEnd;
        }

        //
        // A disk was found to be present. Create state for it.
        //

        NewDisk = SdBcm2709pCreateDisk(Slot);
        if (NewDisk == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SlotQueryChildrenEnd;
        }

        BlockSize = 0;
        Status = SdGetMediaParameters(NewDisk->Controller,
                                      &(NewDisk->BlockCount),
                                      &BlockSize);

        if (!KSUCCESS(Status)) {
            if (Status == STATUS_NO_MEDIA) {
                Status = STATUS_SUCCESS;
            }

            goto SlotQueryChildrenEnd;
        }

        ASSERT(POWER_OF_2(BlockSize) != FALSE);

        NewDisk->BlockShift = RtlCountTrailingZeros32(BlockSize);

        //
        // Initialize DMA is there is system DMA available.
        //

        if (Slot->Dma != NULL) {
            Status = SdStandardInitializeDma(NewDisk->Controller);
            if (KSUCCESS(Status)) {
                NewDisk->Flags |= SD_BCM2709_DISK_FLAG_DMA_SUPPORTED;

            } else if (Status == STATUS_NO_MEDIA) {
                Status = STATUS_SUCCESS;
                goto SlotQueryChildrenEnd;
            }
        }

        //
        // Create the child device.
        //

        DeviceId = SD_MMC_DEVICE_ID;
        if (SD_IS_CARD_SD(NewDisk->Controller)) {
            DeviceId = SD_CARD_DEVICE_ID;
        }

        Status = IoCreateDevice(SdBcm2709Driver,
                                NewDisk,
                                Irp->Device,
                                DeviceId,
                                DISK_CLASS_ID,
                                NULL,
                                &(NewDisk->Device));

        if (!KSUCCESS(Status)) {
            goto SlotQueryChildrenEnd;
        }

        Slot->Disk = NewDisk;
        NewDisk = NULL;
    }

    //
    // If there's no disk, don't enumerate it.
    //

    if (Slot->Disk == NULL) {
        Status = STATUS_SUCCESS;
        goto SlotQueryChildrenEnd;
    }

    ASSERT((Slot->Disk != NULL) && (Slot->Disk->Device != NULL));

    //
    // Enumerate the one child.
    //

    Status = IoMergeChildArrays(Irp,
                                &(Slot->Disk->Device),
                                1,
                                SD_ALLOCATION_TAG);

    if (!KSUCCESS(Status)) {
        goto SlotQueryChildrenEnd;
    }

SlotQueryChildrenEnd:
    if (NewDisk != NULL) {

        ASSERT(NewDisk->Device == NULL);

        SdBcm2709pDiskReleaseReference(NewDisk);
    }

    return Status;
}

PSD_BCM2709_DISK
SdBcm2709pCreateDisk (
    PSD_BCM2709_SLOT Slot
    )

/*++

Routine Description:

    This routine creates an SD disk context.

Arguments:

    Slot - Supplies a pointer to the SD slot to which the disk belongs.

Return Value:

    Returns a pointer to the new SD disk on success or NULL on failure.

--*/

{

    PSD_BCM2709_DISK Disk;

    Disk = MmAllocateNonPagedPool(sizeof(SD_BCM2709_DISK), SD_ALLOCATION_TAG);
    if (Disk == NULL) {
        return NULL;
    }

    RtlZeroMemory(Disk, sizeof(SD_BCM2709_DISK));
    Disk->Type = SdBcm2709DeviceDisk;
    Disk->Parent = Slot;
    Disk->Controller = Slot->Controller;
    Disk->ControllerLock = Slot->Lock;
    Disk->ReferenceCount = 1;
    return Disk;
}

VOID
SdBcm2709pDestroyDisk (
    PSD_BCM2709_DISK Disk
    )

/*++

Routine Description:

    This routine destroys the given SD disk.

Arguments:

    Disk - Supplies a pointer to the SD disk to destroy.

Return Value:

    None.

--*/

{

    ASSERT(Disk->DiskInterface.DiskToken == NULL);

    MmFreeNonPagedPool(Disk);
    return;
}

VOID
SdBcm2709pDiskAddReference (
    PSD_BCM2709_DISK Disk
    )

/*++

Routine Description:

    This routine adds a reference to SD disk.

Arguments:

    Disk - Supplies a pointer to the SD disk.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Disk->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
SdBcm2709pDiskReleaseReference (
    PSD_BCM2709_DISK Disk
    )

/*++

Routine Description:

    This routine releases a reference from the SD disk.

Arguments:

    Disk - Supplies a pointer to the SD disk.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Disk->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        SdBcm2709pDestroyDisk(Disk);
    }

    return;
}

KSTATUS
SdBcm2709pDiskBlockIoReset (
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

    PSD_BCM2709_DISK Disk;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Disk = (PSD_BCM2709_DISK)DiskToken;

    //
    // Put the SD controller into critical execution mode.
    //

    SdSetCriticalMode(Disk->Controller, TRUE);

    //
    // Abort any current transaction that might have been left incomplete
    // when the crash occurred.
    //

    Status = SdAbortTransaction(Disk->Controller, FALSE);
    return Status;
}

KSTATUS
SdBcm2709pDiskBlockIoRead (
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

    PSD_BCM2709_DISK Disk;
    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Disk = (PSD_BCM2709_DISK)DiskToken;
    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress << Disk->BlockShift;
    IrpReadWrite.IoSizeInBytes = BlockCount << Disk->BlockShift;

    //
    // As this read routine is meant for critical code paths (crash dump),
    // indicate that the channel should not be locked when performing the I/O.
    // It may be that some other thread holds the lock, which would cause a
    // dead lock as all other processors and threads are likely frozen.
    //

    Status = SdBcm2709pPerformIoPolled(&IrpReadWrite, Disk, FALSE, FALSE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted >> Disk->BlockShift;
    return Status;
}

KSTATUS
SdBcm2709pDiskBlockIoWrite (
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

    PSD_BCM2709_DISK Disk;
    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Disk = (PSD_BCM2709_DISK)DiskToken;
    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress << Disk->BlockShift;
    IrpReadWrite.IoSizeInBytes = BlockCount << Disk->BlockShift;

    //
    // As this write routine is meant for critical code paths (crash dump),
    // indicate that the channel should not be locked when performing the I/O.
    // It may be that some other thread holds the lock, which would cause a
    // dead lock as all other processors and threads are likely frozen.
    //

    Status = SdBcm2709pPerformIoPolled(&IrpReadWrite, Disk, TRUE, FALSE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted >> Disk->BlockShift;
    return Status;
}

KSTATUS
SdBcm2709pPerformIoPolled (
    PIRP_READ_WRITE IrpReadWrite,
    PSD_BCM2709_DISK Disk,
    BOOL Write,
    BOOL LockRequired
    )

/*++

Routine Description:

    This routine performs polled I/O data transfers.

Arguments:

    IrpReadWrite - Supplies a pointer to the IRP read/write context.

    Disk - Supplies a pointer to the SD disk device.

    Write - Supplies a boolean indicating if this is a read operation (TRUE) or
        a write operation (FALSE).

    LockRequired - Supplies a boolean indicating if the controller lock needs
        to be acquired (TRUE) or it does not (FALSE).

Return Value:

    None.

--*/

{

    UINTN BlockCount;
    ULONGLONG BlockOffset;
    UINTN BytesRemaining;
    UINTN BytesThisRound;
    KSTATUS CompletionStatus;
    PSD_CONTROLLER Controller;
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

    Controller = Disk->Controller;
    IrpReadWrite->IoBytesCompleted = 0;
    LockHeld = FALSE;
    ReadWriteIrpPrepared = FALSE;

    ASSERT(IrpReadWrite->IoBuffer != NULL);
    ASSERT((Disk->BlockCount != 0) && (Disk->BlockShift != 0));

    //
    // Validate the supplied I/O buffer is aligned and big enough.
    //

    IrpReadWriteFlags = IRP_READ_WRITE_FLAG_POLLED;
    if (Write != FALSE) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    Status = IoPrepareReadWriteIrp(IrpReadWrite,
                                   1 << Disk->BlockShift,
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
        KeAcquireQueuedLock(Disk->ControllerLock);
        LockHeld = TRUE;
    }

    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_CHANGED) != 0) {
        Status = STATUS_MEDIA_CHANGED;
        goto PerformBlockIoPolledEnd;

    } else if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        Status = STATUS_NO_MEDIA;
        goto PerformBlockIoPolledEnd;
    }

    //
    // Loop reading in or writing out each fragment in the I/O buffer.
    //

    BytesRemaining = IrpReadWrite->IoSizeInBytes;

    ASSERT(IS_ALIGNED(BytesRemaining, 1 << Disk->BlockShift) != FALSE);
    ASSERT(IS_ALIGNED(IrpReadWrite->IoOffset, 1 << Disk->BlockShift) != FALSE);

    BlockOffset = IrpReadWrite->IoOffset >> Disk->BlockShift;
    while (BytesRemaining != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = (PIO_BUFFER_FRAGMENT)&(IoBuffer->Fragment[FragmentIndex]);
        VirtualAddress = Fragment->VirtualAddress + FragmentOffset;
        BytesThisRound = Fragment->Size - FragmentOffset;
        if (BytesRemaining < BytesThisRound) {
            BytesThisRound = BytesRemaining;
        }

        ASSERT(IS_ALIGNED(BytesThisRound, (1 << Disk->BlockShift)) != FALSE);

        BlockCount = BytesThisRound >> Disk->BlockShift;

        //
        // Make sure the system isn't trying to do I/O off the end of the disk.
        //

        ASSERT(BlockOffset < Disk->BlockCount);
        ASSERT(BlockCount >= 1);

        Status = SdBlockIoPolled(Disk->Controller,
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
        KeReleaseQueuedLock(Disk->ControllerLock);
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
SdBcm2709pInitializeDma (
    PSD_BCM2709_SLOT Slot
    )

/*++

Routine Description:

    This routine attempts to wire up the BCM2709 DMA controller to the SD
    controller.

Arguments:

    Slot - Supplies a pointer to this SD BCM2709 slot context.

Return Value:

    Status code.

--*/

{

    BOOL Equal;
    DMA_INFORMATION Information;
    PRESOURCE_ALLOCATION Resource;
    KSTATUS Status;
    PDMA_TRANSFER Transfer;

    Resource = Slot->DmaResource;

    ASSERT(Resource != NULL);

    Status = IoRegisterForInterfaceNotifications(&SdBcm2709DmaUuid,
                                                 SdBcm2709pDmaInterfaceCallback,
                                                 Resource->Provider,
                                                 Slot,
                                                 TRUE);

    if (!KSUCCESS(Status)) {
        goto InitializeDmaEnd;
    }

    if (Slot->Dma == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto InitializeDmaEnd;
    }

    RtlZeroMemory(&Information, sizeof(DMA_INFORMATION));
    Information.Version = DMA_INFORMATION_VERSION;
    Status = Slot->Dma->GetInformation(Slot->Dma, &Information);
    if (!KSUCCESS(Status)) {
        goto InitializeDmaEnd;
    }

    Equal = RtlAreUuidsEqual(&(Information.ControllerUuid),
                             &SdBcm2709DmaBcm2709Uuid);

    if (Equal == FALSE) {
        Status = STATUS_NOT_SUPPORTED;
        goto InitializeDmaEnd;
    }

    if (Slot->DmaTransfer == NULL) {
        Status = Slot->Dma->AllocateTransfer(Slot->Dma, &Transfer);
        if (!KSUCCESS(Status)) {
            goto InitializeDmaEnd;
        }

        Slot->DmaTransfer = Transfer;

        //
        // Fill in some of the fields that will never change transfer to
        // transfer.
        //

        Transfer->Configuration = NULL;
        Transfer->ConfigurationSize = 0;
        Transfer->CompletionCallback = SdBcm2709pSystemDmaCompletion;
        Transfer->Width = 32;
        Transfer->Device.Address = Slot->Resource->Allocation +
                                   SdRegisterBufferDataPort;

        Transfer->Device.Address &= ~SD_BCM2709_DEVICE_ADDRESS_MASK;
        Transfer->Device.Address |= SD_BCM2709_DEVICE_ADDRESS_VALUE;
    }

InitializeDmaEnd:
    if (!KSUCCESS(Status)) {
        if (Slot->DmaTransfer != NULL) {
            Slot->Dma->FreeTransfer(Slot->Dma, Slot->DmaTransfer);
            Slot->DmaTransfer = NULL;
        }

        IoUnregisterForInterfaceNotifications(&SdBcm2709DmaUuid,
                                              SdBcm2709pDmaInterfaceCallback,
                                              Resource->Provider,
                                              Slot);
    }

    return Status;
}

VOID
SdBcm2709pDmaInterfaceCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine is called to notify listeners that an interface has arrived
    or departed.

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

    PSD_BCM2709_SLOT Slot;

    Slot = Context;

    ASSERT(InterfaceBufferSize >= sizeof(DMA_INTERFACE));
    ASSERT((Slot->Dma == NULL) || (Slot->Dma == InterfaceBuffer));

    if (Arrival != FALSE) {
        Slot->Dma = InterfaceBuffer;

    } else {
        Slot->Dma = NULL;
    }

    return;
}

VOID
SdBcm2709pPerformDmaIo (
    PSD_BCM2709_DISK Disk,
    PIRP Irp
    )

/*++

Routine Description:

    This routine performs DMA-based I/O for the OMAP SD controller.

Arguments:

    Disk - Supplies a pointer to the slot's disk.

    Irp - Supplies a pointer to the partially completed IRP.

Return Value:

    None.

--*/

{

    UINTN BlockCount;
    ULONGLONG BlockOffset;
    PDMA_INTERFACE Dma;
    IO_OFFSET IoOffset;
    ULONGLONG IoSize;
    KSTATUS Status;
    BOOL Write;

    IoOffset = Irp->U.ReadWrite.IoOffset + Irp->U.ReadWrite.IoBytesCompleted;
    BlockOffset = IoOffset >> Disk->BlockShift;
    IoSize = Irp->U.ReadWrite.IoSizeInBytes - Irp->U.ReadWrite.IoBytesCompleted;
    BlockCount = IoSize >> Disk->BlockShift;
    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    ASSERT(BlockOffset < Disk->BlockCount);
    ASSERT(BlockCount >= 1);

    //
    // The expected intrrupt count has to be set up now because SD might
    // complete it immediately.
    //

    Dma = Disk->Parent->Dma;
    if (Dma != NULL) {

        ASSERT(Disk->RemainingInterrupts == 0);

        Disk->RemainingInterrupts = 2;
    }

    SdStandardBlockIoDma(Disk->Controller,
                         BlockOffset,
                         BlockCount,
                         Irp->U.ReadWrite.IoBuffer,
                         Irp->U.ReadWrite.IoBytesCompleted,
                         Write,
                         SdBcm2709pSdDmaCompletion,
                         Disk);

    //
    // Fire off the system DMA transfer if necessary.
    //

    if (Dma != NULL) {
        Status = SdBcm2709pSetupSystemDma(Disk);
        if (!KSUCCESS(Status)) {
            IoCompleteIrp(SdBcm2709Driver, Irp, Status);
            return;
        }
    }

    return;
}

VOID
SdBcm2709pSdDmaCompletion (
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

    PSD_BCM2709_DISK Disk;

    Disk = Context;
    if (!KSUCCESS(Status) || (Disk->Parent->Dma == NULL)) {
        if (Disk->Parent->Dma != NULL) {
            Disk->Parent->Dma->Cancel(Disk->Parent->Dma,
                                      Disk->Parent->DmaTransfer);
        }

        RtlAtomicAdd32(&(Disk->RemainingInterrupts), -1);
        SdBcm2709pDmaCompletion(Controller, Context, BytesTransferred, Status);

    //
    // If this is an SD interrupt coming in and system DMA is enabled, only
    // complete the transfer if SD came in last.
    //

    } else if (RtlAtomicAdd32(&(Disk->RemainingInterrupts), -1) == 1) {
        SdBcm2709pDmaCompletion(Controller, Context, 0, Status);
    }

    return;
}

KSTATUS
SdBcm2709pSetupSystemDma (
    PSD_BCM2709_DISK Disk
    )

/*++

Routine Description:

    This routine submits a system DMA request on behalf of the SD controller.

Arguments:

    Disk - Supplies a pointer to the slot's disk.

Return Value:

    Status code.

--*/

{

    PDMA_INTERFACE Dma;
    PDMA_TRANSFER DmaTransfer;
    PIRP Irp;
    KSTATUS Status;

    Dma = Disk->Parent->Dma;
    DmaTransfer = Disk->Parent->DmaTransfer;
    Irp = Disk->Irp;
    DmaTransfer->Memory = Irp->U.ReadWrite.IoBuffer;
    DmaTransfer->Completed = Irp->U.ReadWrite.IoBytesCompleted;
    DmaTransfer->Size = Irp->U.ReadWrite.IoSizeInBytes;
    DmaTransfer->UserContext = Disk;
    DmaTransfer->Allocation = Disk->Parent->DmaResource;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        DmaTransfer->Direction = DmaTransferToDevice;

    } else {
        DmaTransfer->Direction = DmaTransferFromDevice;
    }

    Status = Dma->Submit(Dma, DmaTransfer);
    return Status;
}

VOID
SdBcm2709pSystemDmaCompletion (
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when a transfer set has completed or errored out.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    PSD_BCM2709_DISK Disk;
    KSTATUS Status;

    Disk = Transfer->UserContext;
    Status = Transfer->Status;
    SdBcm2709pDmaCompletion(Disk->Controller,
                            Disk,
                            Transfer->Completed,
                            Status);

    return;
}

VOID
SdBcm2709pDmaCompletion (
    PSD_CONTROLLER Controller,
    PVOID Context,
    UINTN BytesTransferred,
    KSTATUS Status
    )

/*++

Routine Description:

    This routine is called indirectly by either the system DMA code or the SD
    library code once the transfer has actually completed. It either completes
    the IRP or fires up a new transfer.

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

    PSD_BCM2709_DISK Disk;
    PIRP Irp;

    Disk = Context;
    Irp = Disk->Irp;

    ASSERT(Irp != NULL);

    if (!KSUCCESS(Status)) {
        RtlAtomicAdd32(&(Disk->RemainingInterrupts), -1);
        RtlDebugPrint("SD BCM2709 Failed: %d\n", Status);
        SdAbortTransaction(Controller, FALSE);
        IoCompleteIrp(SdBcm2709Driver, Irp, Status);
        return;
    }

    if (BytesTransferred != 0) {
        Irp->U.ReadWrite.IoBytesCompleted += BytesTransferred;
        Irp->U.ReadWrite.NewIoOffset += BytesTransferred;

        //
        // If more interrupts are expected, don't complete just yet.
        //

        if (RtlAtomicAdd32(&(Disk->RemainingInterrupts), -1) != 1) {
            return;
        }

    //
    // Otherwise if this is SD and it was the last remaining interrupt, the
    // DMA portion better be complete already.
    //

    } else {

        ASSERT(Disk->RemainingInterrupts == 0);
    }

    //
    // If this transfer's over, complete the IRP.
    //

    if (Irp->U.ReadWrite.IoBytesCompleted ==
        Irp->U.ReadWrite.IoSizeInBytes) {

        IoCompleteIrp(SdBcm2709Driver, Irp, Status);
        return;
    }

    SdBcm2709pPerformDmaIo(Disk, Irp);
    return;
}

