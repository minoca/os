/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sd.c

Abstract:

    This module implements the SD/MMC driver.

Author:

    Evan Green 27-Feb-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define away the API decorator.
//

#define SD_API

#include <minoca/kernel/driver.h>
#include <minoca/intrface/disk.h>
#include <minoca/sd/sd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of slots that can be on one device. On current
// implementations this is limited by the number of PCI BARs, where each
// slot gets a BAR.
//

#define MAX_SD_SLOTS 6

//
// Define the amount of time in microseconds to wait after an insertion event
// to allow the card to simmer down in the slot.
//

#define SD_INSERTION_SETTLE_DELAY 50000

//
// Define the set of flags for an SD disk.
//

#define SD_DISK_FLAG_DMA_SUPPORTED     0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SD_DEVICE_TYPE {
    SdDeviceInvalid,
    SdDeviceBus,
    SdDeviceSlot,
    SdDeviceDisk
} SD_DEVICE_TYPE, *PSD_DEVICE_TYPE;

typedef struct _SD_BUS SD_BUS, *PSD_BUS;
typedef struct _SD_SLOT SD_SLOT, *PSD_SLOT;

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

    ControllerLock - Stores a pointer to the lock used to serialize access to
        the controller. This is owned by the parent slot.

    Irp - Stores a pointer to the current IRP running on this disk.

    Flags - Stores a bitmask of flags describing the disk state. See
        SD_DISK_FLAG_* for definitions;

    BlockShift - Stores the block size shift of the disk.

    BlockCount - Stores the number of blocks on the disk.

    DiskInterface - Stores the disk interface presented to the system.

--*/

typedef struct _SD_DISK {
    SD_DEVICE_TYPE Type;
    volatile ULONG ReferenceCount;
    PDEVICE Device;
    PSD_SLOT Parent;
    PSD_CONTROLLER Controller;
    PQUEUED_LOCK ControllerLock;
    PIRP Irp;
    ULONG Flags;
    ULONG BlockShift;
    ULONGLONG BlockCount;
    DISK_INTERFACE DiskInterface;
} SD_DISK, *PSD_DISK;

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

    ChildIndex - Stores the child index of this device.

    Parent - Stores a pointer back to the parent.

    Disk - Stores a pointer to the child disk context.

    Lock - Stores a pointer to a lock used to serialize access to the
        controller.

--*/

struct _SD_SLOT {
    SD_DEVICE_TYPE Type;
    PDEVICE Device;
    PSD_CONTROLLER Controller;
    PVOID ControllerBase;
    PRESOURCE_ALLOCATION Resource;
    UINTN ChildIndex;
    PSD_BUS Parent;
    PSD_DISK Disk;
    PQUEUED_LOCK Lock;
};

/*++

Structure Description:

    This structure describes an SD/MMC driver context (the function driver
    context for the SD bus controller).

Members:

    Type - Stores the type identifying this as an SD controller.

    Slots - Stores the array of SD slots.

    Handle - Stores the connected interrupt handle.

    InterruptLine - Stores ths interrupt line of the controller.

    InterruptVector - Stores the interrupt vector of the controller.

    InterruptResourcesFound - Stores a boolean indicating whether or not
        interrupt resources were located for this device.

--*/

struct _SD_BUS {
    SD_DEVICE_TYPE Type;
    SD_SLOT Slots[MAX_SD_SLOTS];
    HANDLE InterruptHandle;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
SdAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
SdDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
SdDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
SdBusInterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
SdBusInterruptServiceDispatch (
    PVOID Context
    );

VOID
SdpBusDispatchStateChange (
    PIRP Irp,
    PSD_BUS Bus
    );

VOID
SdpSlotDispatchStateChange (
    PIRP Irp,
    PSD_SLOT Slot
    );

VOID
SdpDiskDispatchStateChange (
    PIRP Irp,
    PSD_DISK Disk
    );

KSTATUS
SdpBusProcessResourceRequirements (
    PIRP Irp,
    PSD_BUS Bus
    );

KSTATUS
SdpBusStartDevice (
    PIRP Irp,
    PSD_BUS Bus
    );

KSTATUS
SdpBusQueryChildren (
    PIRP Irp,
    PSD_BUS Context
    );

KSTATUS
SdpSlotStartDevice (
    PIRP Irp,
    PSD_SLOT Slot
    );

KSTATUS
SdpSlotQueryChildren (
    PIRP Irp,
    PSD_SLOT Slot
    );

PSD_DISK
SdpCreateDisk (
    PSD_SLOT Slot
    );

VOID
SdpDestroyDisk (
    PSD_DISK Disk
    );

VOID
SdpDiskAddReference (
    PSD_DISK Disk
    );

VOID
SdpDiskReleaseReference (
    PSD_DISK Disk
    );

VOID
SdpDmaCompletion (
    PSD_CONTROLLER Controller,
    PVOID Context,
    UINTN BytesTransferred,
    KSTATUS Status
    );

KSTATUS
SdpDiskBlockIoReset (
    PVOID DiskToken
    );

KSTATUS
SdpDiskBlockIoRead (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
SdpDiskBlockIoWrite (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
SdpPerformIoPolled (
    PIRP_READ_WRITE IrpReadWrite,
    PSD_DISK Disk,
    BOOL Write,
    BOOL LockRequired
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER SdDriver = NULL;
UUID SdDiskInterfaceUuid = UUID_DISK_INTERFACE;

DISK_INTERFACE SdDiskInterfaceTemplate = {
    DISK_INTERFACE_VERSION,
    NULL,
    0,
    0,
    NULL,
    SdpDiskBlockIoReset,
    SdpDiskBlockIoRead,
    SdpDiskBlockIoWrite
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

    SdDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = SdAddDevice;
    FunctionTable.DispatchStateChange = SdDispatchStateChange;
    FunctionTable.DispatchOpen = SdDispatchOpen;
    FunctionTable.DispatchClose = SdDispatchClose;
    FunctionTable.DispatchIo = SdDispatchIo;
    FunctionTable.DispatchSystemControl = SdDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
SdAddDevice (
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

    PSD_BUS Context;
    PSD_SLOT Slot;
    UINTN SlotIndex;
    KSTATUS Status;

    Context = MmAllocateNonPagedPool(sizeof(SD_BUS), SD_ALLOCATION_TAG);
    if (Context == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Context, sizeof(SD_BUS));
    Context->Type = SdDeviceBus;
    Context->InterruptHandle = INVALID_HANDLE;
    for (SlotIndex = 0; SlotIndex < MAX_SD_SLOTS; SlotIndex += 1) {
        Slot = &(Context->Slots[SlotIndex]);
        Slot->Type = SdDeviceSlot;
        Slot->ChildIndex = SlotIndex;
        Slot->Parent = Context;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Context);
    if (!KSUCCESS(Status)) {
        MmFreeNonPagedPool(Context);
    }

    return Status;
}

VOID
SdDispatchStateChange (
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

    PSD_BUS Context;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Context = (PSD_BUS)DeviceContext;
    switch (Context->Type) {
    case SdDeviceBus:
        SdpBusDispatchStateChange(Irp, Context);
        break;

    case SdDeviceSlot:
        SdpSlotDispatchStateChange(Irp, (PSD_SLOT)Context);
        break;

    case SdDeviceDisk:
        SdpDiskDispatchStateChange(Irp, (PSD_DISK)Context);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
SdDispatchOpen (
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

    PSD_DISK Disk;

    Disk = DeviceContext;
    if (Disk->Type != SdDeviceDisk) {
        return;
    }

    SdpDiskAddReference(Disk);
    IoCompleteIrp(SdDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
SdDispatchClose (
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

    PSD_DISK Disk;

    Disk = DeviceContext;
    if (Disk->Type != SdDeviceDisk) {
        return;
    }

    SdpDiskReleaseReference(Disk);
    IoCompleteIrp(SdDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
SdDispatchIo (
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
    BOOL CompleteIrp;
    PSD_CONTROLLER Controller;
    PSD_DISK Disk;
    IO_OFFSET IoOffset;
    ULONG IrpReadWriteFlags;
    KSTATUS IrpStatus;
    KSTATUS Status;
    BOOL Write;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Disk = DeviceContext;
    if (Disk->Type != SdDeviceDisk) {

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

    if ((Disk->Flags & SD_DISK_FLAG_DMA_SUPPORTED) == 0) {

        ASSERT(Irp->Direction == IrpDown);

        Status = SdpPerformIoPolled(&(Irp->U.ReadWrite), Disk, Write, TRUE);
        goto DispatchIoEnd;
    }

    //
    // Set the IRP read/write flags for the preparation and completion steps.
    //

    IrpReadWriteFlags = IRP_READ_WRITE_FLAG_DMA;
    if (Write != FALSE) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    Controller = Disk->Controller;
    if (Irp->Direction == IrpDown) {
        Controller->Try = 0;
    }

    //
    // If the IRP is on the way up, then clean up after the DMA as this IRP is
    // still sitting in the channel. An IRP going up is already complete.
    //

    if (Irp->Direction == IrpUp) {

        ASSERT (Irp == Disk->Irp);

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

    BytesToComplete = Irp->U.ReadWrite.IoSizeInBytes;
    IoOffset = Irp->U.ReadWrite.IoOffset;
    Irp->U.ReadWrite.IoBytesCompleted = 0;

    ASSERT((Disk->BlockCount != 0) && (Disk->BlockShift != 0));
    ASSERT(Irp->U.ReadWrite.IoBuffer != NULL);
    ASSERT(IS_ALIGNED(IoOffset, 1 << Disk->BlockShift) != FALSE);
    ASSERT(IS_ALIGNED(BytesToComplete, 1 << Disk->BlockShift) != FALSE);

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

    //
    // Pend the IRP and fire up the DMA.
    //

    Irp->U.ReadWrite.NewIoOffset = Irp->U.ReadWrite.IoOffset;
    Disk->Irp = Irp;
    BlockOffset = IoOffset >> Disk->BlockShift;
    BlockCount = BytesToComplete >> Disk->BlockShift;
    CompleteIrp = FALSE;
    IoPendIrp(SdDriver, Irp);

    //
    // Make sure the system isn't trying to do I/O off the end of the disk.
    //

    ASSERT(BlockOffset < Disk->BlockCount);
    ASSERT(BlockCount >= 1);

    SdStandardBlockIoDma(Controller,
                         BlockOffset,
                         BlockCount,
                         Irp->U.ReadWrite.IoBuffer,
                         0,
                         Write,
                         SdpDmaCompletion,
                         Disk);

    //
    // DMA transfers are self perpetuating, so after kicking off this first
    // transfer, return. This returns with the lock held because I/O is
    // still in progress.
    //

    ASSERT(KeIsQueuedLockHeld(Disk->ControllerLock) != FALSE);
    ASSERT(CompleteIrp == FALSE);

DispatchIoEnd:
    if (CompleteIrp != FALSE) {
        IoCompleteIrp(SdDriver, Irp, Status);
    }

    return;
}

VOID
SdDispatchSystemControl (
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
    PSD_DISK Disk;
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

    if (Disk->Type != SdDeviceDisk) {
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

        IoCompleteIrp(SdDriver, Irp, Status);
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

        IoCompleteIrp(SdDriver, Irp, Status);
        break;

    //
    // Do not support hard disk device truncation.
    //

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(SdDriver, Irp, STATUS_NOT_SUPPORTED);
        break;

    //
    // Gather and return device information.
    //

    case IrpMinorSystemControlDeviceInformation:
        break;

    case IrpMinorSystemControlSynchronize:
        IoCompleteIrp(SdDriver, Irp, STATUS_SUCCESS);
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
SdBusInterruptService (
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

    PSD_BUS Bus;
    PSD_SLOT Slot;
    UINTN SlotIndex;
    INTERRUPT_STATUS Status;
    INTERRUPT_STATUS TotalStatus;

    Bus = Context;
    TotalStatus = InterruptStatusNotClaimed;
    for (SlotIndex = 0; SlotIndex < MAX_SD_SLOTS; SlotIndex += 1) {
        Slot = &(Bus->Slots[SlotIndex]);
        if (Slot->Controller == NULL) {
            break;
        }

        Status = SdStandardInterruptService(Slot->Controller);
        if (Status != InterruptStatusNotClaimed) {
            TotalStatus = Status;
        }
    }

    return TotalStatus;
}

INTERRUPT_STATUS
SdBusInterruptServiceDispatch (
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

    PSD_BUS Bus;
    PSD_SLOT Slot;
    UINTN SlotIndex;
    INTERRUPT_STATUS Status;
    INTERRUPT_STATUS TotalStatus;

    Bus = Context;
    TotalStatus = InterruptStatusNotClaimed;
    for (SlotIndex = 0; SlotIndex < MAX_SD_SLOTS; SlotIndex += 1) {
        Slot = &(Bus->Slots[SlotIndex]);
        if (Slot->Controller == NULL) {
            break;
        }

        Status = SdStandardInterruptServiceDispatch(Slot->Controller);
        if (Status != InterruptStatusNotClaimed) {
            TotalStatus = Status;
        }
    }

    return TotalStatus;
}

VOID
SdpBusDispatchStateChange (
    PIRP Irp,
    PSD_BUS Bus
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
            Status = SdpBusProcessResourceRequirements(Irp, Bus);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdDriver, Irp, Status);
            }

            break;

        case IrpMinorStartDevice:
            Status = SdpBusStartDevice(Irp, Bus);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdDriver, Irp, Status);
            }

            break;

        case IrpMinorQueryChildren:
            Status = SdpBusQueryChildren(Irp, Bus);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(SdDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
SdpSlotDispatchStateChange (
    PIRP Irp,
    PSD_SLOT Slot
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
            Status = SdpSlotStartDevice(Irp, Slot);
            IoCompleteIrp(SdDriver, Irp, Status);
            break;

        case IrpMinorQueryResources:
            IoCompleteIrp(SdDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorQueryChildren:
            Status = SdpSlotQueryChildren(Irp, Slot);
            IoCompleteIrp(SdDriver, Irp, Status);
            break;

        default:
            break;
        }
    }

    return;
}

VOID
SdpDiskDispatchStateChange (
    PIRP Irp,
    PSD_DISK Disk
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
                              &SdDiskInterfaceTemplate,
                              sizeof(DISK_INTERFACE));

                Disk->DiskInterface.DiskToken = Disk;
                Disk->DiskInterface.BlockSize = 1 << Disk->BlockShift;
                Disk->DiskInterface.BlockCount = Disk->BlockCount;
                Status = IoCreateInterface(&SdDiskInterfaceUuid,
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
                Status = IoDestroyInterface(&SdDiskInterfaceUuid,
                                            Disk->Device,
                                            &(Disk->DiskInterface));

                ASSERT(KSUCCESS(Status));

                Disk->DiskInterface.DiskToken = NULL;
            }

            SdpDiskReleaseReference(Disk);
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
            IoCompleteIrp(SdDriver, Irp, Status);
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
SdpBusProcessResourceRequirements (
    PIRP Irp,
    PSD_BUS Bus
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
SdpBusStartDevice (
    PIRP Irp,
    PSD_BUS Bus
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
    UINTN SlotIndex;
    KSTATUS Status;

    for (SlotIndex = 0; SlotIndex < MAX_SD_SLOTS; SlotIndex += 1) {
        Bus->Slots[SlotIndex].Resource = NULL;

        ASSERT(Bus->Slots[SlotIndex].Controller == NULL);

    }

    SlotIndex = 0;

    //
    // Loop through the allocated resources to get the controller base and the
    // interrupt.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;

    ASSERT(AllocationList != NULL);

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
            if ((SlotIndex < MAX_SD_SLOTS) && (Allocation->Length > 0)) {
                Bus->Slots[SlotIndex].Resource = Allocation;
                SlotIndex += 1;
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
        Connect.InterruptServiceRoutine = SdBusInterruptService;
        Connect.DispatchServiceRoutine = SdBusInterruptServiceDispatch;
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
SdpBusQueryChildren (
    PIRP Irp,
    PSD_BUS Context
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

    UINTN ChildCount;
    UINTN ChildIndex;
    PDEVICE Children[MAX_SD_SLOTS];
    PSD_SLOT Slot;
    KSTATUS Status;

    ChildCount = 0;
    for (ChildIndex = 0; ChildIndex < MAX_SD_SLOTS; ChildIndex += 1) {
        Slot = &(Context->Slots[ChildIndex]);
        if (Slot->Resource != NULL) {
            if (Slot->Device == NULL) {
                Status = IoCreateDevice(SdDriver,
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

            Children[ChildCount] = Slot->Device;
            ChildCount += 1;
        }
    }

    if (ChildCount != 0) {
        Status = IoMergeChildArrays(Irp,
                                    Children,
                                    ChildCount,
                                    SD_ALLOCATION_TAG);

        if (!KSUCCESS(Status)) {
            goto BusQueryChildrenEnd;
        }
    }

    Status = STATUS_SUCCESS;

BusQueryChildrenEnd:
    return Status;
}

KSTATUS
SdpSlotStartDevice (
    PIRP Irp,
    PSD_SLOT Slot
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
    // Initialize the standard SD controller.
    //

    if (Slot->Controller == NULL) {
        RtlZeroMemory(&Parameters, sizeof(SD_INITIALIZATION_BLOCK));
        Parameters.ConsumerContext = Slot;
        Parameters.StandardControllerBase = Slot->ControllerBase;
        Parameters.HostCapabilities = SD_MODE_AUTO_CMD12 |
                                      SD_MODE_4BIT |
                                      SD_MODE_RESPONSE136_SHIFTED |
                                      SD_MODE_CMD23;

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
SdpSlotQueryChildren (
    PIRP Irp,
    PSD_SLOT Slot
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
    PSD_DISK NewDisk;
    ULONG OldFlags;
    KSTATUS Status;

    NewDisk = NULL;

    //
    // Collect the current pending status.
    //

    FlagsMask = ~(SD_CONTROLLER_FLAG_INSERTION_PENDING |
                  SD_CONTROLLER_FLAG_REMOVAL_PENDING);

    OldFlags = RtlAtomicAnd32(&(Slot->Controller->Flags), FlagsMask);

    //
    // If either removal or insertion is pending, remove the existing disk. In
    // theory, an insertion should always follow a removal, but this does not
    // appear to be the case in practice when cards are quickly removed and
    // inserted.
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
    // Check to see if there's an insertion pending, re-initialize the
    // controller and create a new disk if there is one present.
    //

    if ((OldFlags & SD_CONTROLLER_FLAG_INSERTION_PENDING) != 0) {

        ASSERT(Slot->Disk == NULL);

        KeDelayExecution(FALSE, FALSE, SD_INSERTION_SETTLE_DELAY);
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
        // Allocate a new disk context for the slot. The disk was at least
        // present long enough to be enumerated.
        //

        NewDisk = SdpCreateDisk(Slot);
        if (NewDisk == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto SlotQueryChildrenEnd;
        }

        //
        // The slot just got a new disk, set the block size and count. Ignore
        // cases where the card immediately got removed. Act like it was never
        // seen.
        //

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
        // Initialize DMA support, but it's okay if it doesn't succeed. Again,
        // don't bother reporting the disk if it got removed.
        //

        Status = SdStandardInitializeDma(NewDisk->Controller);
        if (KSUCCESS(Status)) {
            NewDisk->Flags |= SD_DISK_FLAG_DMA_SUPPORTED;

        } else if (Status == STATUS_NO_MEDIA) {
            Status = STATUS_SUCCESS;
            goto SlotQueryChildrenEnd;
        }

        //
        // Create the OS device for the disk.
        //

        DeviceId = SD_MMC_DEVICE_ID;
        if (SD_IS_CARD_SD(NewDisk->Controller)) {
            DeviceId = SD_CARD_DEVICE_ID;
        }

        Status = IoCreateDevice(SdDriver,
                                NewDisk,
                                Irp->Device,
                                DeviceId,
                                DISK_CLASS_ID,
                                NULL,
                                &(NewDisk->Device));

        if (!KSUCCESS(Status)) {
            goto SlotQueryChildrenEnd;
        }

        //
        // The disk for the slot is all set to go.
        //

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

        SdpDiskReleaseReference(NewDisk);
    }

    return Status;
}

PSD_DISK
SdpCreateDisk (
    PSD_SLOT Slot
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

    PSD_DISK Disk;

    Disk = MmAllocateNonPagedPool(sizeof(SD_DISK), SD_ALLOCATION_TAG);
    if (Disk == NULL) {
        return NULL;
    }

    RtlZeroMemory(Disk, sizeof(SD_DISK));
    Disk->Type = SdDeviceDisk;
    Disk->Parent = Slot;
    Disk->Controller = Slot->Controller;
    Disk->ControllerLock = Slot->Lock;
    Disk->ReferenceCount = 1;
    return Disk;
}

VOID
SdpDestroyDisk (
    PSD_DISK Disk
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
    ASSERT(Disk->Irp == NULL);

    MmFreeNonPagedPool(Disk);
    return;
}

VOID
SdpDiskAddReference (
    PSD_DISK Disk
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
SdpDiskReleaseReference (
    PSD_DISK Disk
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
        SdpDestroyDisk(Disk);
    }

    return;
}

VOID
SdpDmaCompletion (
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
    PSD_DISK Disk;
    ULONGLONG IoOffset;
    UINTN IoSize;
    PIRP Irp;
    BOOL Write;

    Disk = Context;
    Irp = Disk->Irp;

    ASSERT(Irp != NULL);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("SD Failed: %d 0x%I64x 0x%x 0x%x\n",
                      Status,
                      Irp->U.ReadWrite.IoOffset,
                      Irp->U.ReadWrite.IoSizeInBytes,
                      Irp->MinorCode);

        IoCompleteIrp(SdDriver, Irp, Status);
        return;
    }

    Irp->U.ReadWrite.IoBytesCompleted += BytesTransferred;
    Irp->U.ReadWrite.NewIoOffset += BytesTransferred;

    //
    // If this transfer's over, unlock and complete the IRP.
    //

    if (Irp->U.ReadWrite.IoBytesCompleted ==
        Irp->U.ReadWrite.IoSizeInBytes) {

        IoCompleteIrp(SdDriver, Irp, Status);
        return;
    }

    IoOffset = Irp->U.ReadWrite.NewIoOffset;

    ASSERT(IoOffset ==
           (Irp->U.ReadWrite.IoOffset + Irp->U.ReadWrite.IoBytesCompleted));

    BlockOffset = IoOffset >> Disk->BlockShift;
    IoSize = Irp->U.ReadWrite.IoSizeInBytes - Irp->U.ReadWrite.IoBytesCompleted;
    BlockCount = IoSize >> Disk->BlockShift;
    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    SdStandardBlockIoDma(Disk->Controller,
                         BlockOffset,
                         BlockCount,
                         Irp->U.ReadWrite.IoBuffer,
                         Irp->U.ReadWrite.IoBytesCompleted,
                         Write,
                         SdpDmaCompletion,
                         Disk);

    return;
}

KSTATUS
SdpDiskBlockIoReset (
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

    PSD_DISK Disk;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Disk = (PSD_DISK)DiskToken;

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
SdpDiskBlockIoRead (
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

    PSD_DISK Disk;
    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Disk = (PSD_DISK)DiskToken;
    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress << Disk->BlockShift;
    IrpReadWrite.IoSizeInBytes = BlockCount << Disk->BlockShift;

    //
    // As this read routine is meant for critical code paths (crash dump),
    // indicate that the channel should not be locked when performing the I/O.
    // It may be that some other thread holds the lock, which would cause a
    // dead lock as all other processors and threads are likely frozen.
    //

    Status = SdpPerformIoPolled(&IrpReadWrite, Disk, FALSE, FALSE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted >> Disk->BlockShift;
    return Status;
}

KSTATUS
SdpDiskBlockIoWrite (
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

    PSD_DISK Disk;
    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Disk = (PSD_DISK)DiskToken;
    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress << Disk->BlockShift;
    IrpReadWrite.IoSizeInBytes = BlockCount << Disk->BlockShift;

    //
    // As this write routine is meant for critical code paths (crash dump),
    // indicate that the channel should not be locked when performing the I/O.
    // It may be that some other thread holds the lock, which would cause a
    // dead lock as all other processors and threads are likely frozen.
    //

    Status = SdpPerformIoPolled(&IrpReadWrite, Disk, TRUE, FALSE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted >> Disk->BlockShift;
    return Status;
}

KSTATUS
SdpPerformIoPolled (
    PIRP_READ_WRITE IrpReadWrite,
    PSD_DISK Disk,
    BOOL Write,
    BOOL LockRequired
    )

/*++

Routine Description:

    This routine performs polled I/O data transfers.

Arguments:

    IrpReadWrite - Supplies a pointer to an IRP read/write context.

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
        goto PerformIoPolledEnd;
    }

    ReadWriteIrpPrepared = TRUE;

    //
    // Make sure the I/O buffer is mapped before use. SD depends on the buffer
    // being mapped.
    //

    IoBuffer = IrpReadWrite->IoBuffer;
    Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE);
    if (!KSUCCESS(Status)) {
        goto PerformIoPolledEnd;
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

    Controller = Disk->Controller;
    if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_CHANGED) != 0) {
        Status = STATUS_MEDIA_CHANGED;
        goto PerformIoPolledEnd;

    } else if ((Controller->Flags & SD_CONTROLLER_FLAG_MEDIA_PRESENT) == 0) {
        Status = STATUS_NO_MEDIA;
        goto PerformIoPolledEnd;
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
            goto PerformIoPolledEnd;
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

PerformIoPolledEnd:
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

