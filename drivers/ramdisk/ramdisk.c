/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ramdisk.c

Abstract:

    This module implements driver support for RAM disks. This RAM disk driver
    serves as an excellent simple example for Open, Close, I/O, and system
    control IRPs. It is fairly unusual (and therefore probably not a good
    example) in relation to its DriverEntry, AddDevice, and StateChange
    handling. Be aware of this if using this driver as a template to write
    your own.

Author:

    Evan Green 17-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/kernel/sysres.h>

//
// ---------------------------------------------------------------- Definitions
//

#define RAM_DISK_ALLOCATION_TAG 0x444D4152 // 'DMAR'
#define RAM_DISK_SECTOR_SIZE 0x200

//
// --------------------------------------------------------------------- Macros
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines state associated with a RAM disk.

Members:

    PhysicalAddress - Stores the physical address of the buffer.

    Buffer - Stores a pointer to the buffer of the raw RAM disk.

    Size - Stores the total size of the RAM disk, in bytes.

--*/

typedef struct _RAM_DISK_DEVICE {
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID Buffer;
    ULONGLONG Size;
} RAM_DISK_DEVICE, *PRAM_DISK_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
RamDiskAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
RamDiskDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
RamDiskDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
RamDiskDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
RamDiskDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
RamDiskDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the driver object.
//

PDRIVER RamDiskDriver = NULL;

//
// Store the next identifier.
//

volatile ULONG RamDiskNextIdentifier = 0;

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

    This routine is the entry point for the RAM disk driver. It registers its
    other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    volatile ULONG DeviceId;
    CHAR DeviceIdString[11];
    DRIVER_FUNCTION_TABLE FunctionTable;
    PSYSTEM_RESOURCE_HEADER GenericHeader;
    PRAM_DISK_DEVICE RamDiskDevice;
    PSYSTEM_RESOURCE_RAM_DISK RamDiskResource;
    KSTATUS Status;

    RamDiskDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = RamDiskAddDevice;
    FunctionTable.DispatchStateChange = RamDiskDispatchStateChange;
    FunctionTable.DispatchOpen = RamDiskDispatchOpen;
    FunctionTable.DispatchClose = RamDiskDispatchClose;
    FunctionTable.DispatchIo = RamDiskDispatchIo;
    FunctionTable.DispatchSystemControl = RamDiskDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Get all RAM disks from the boot environment. This is not normally how
    // devices are created or enumerated. The RAM disk is special in that its
    // devices and resources are essentially born out of the boot environment.
    // Don't copy this loop if using this driver as a template.
    //

    while (TRUE) {
        GenericHeader = KeAcquireSystemResource(SystemResourceRamDisk);
        if (GenericHeader == NULL) {
            break;
        }

        RamDiskResource = (PSYSTEM_RESOURCE_RAM_DISK)GenericHeader;

        //
        // Allocate the internal data structure.
        //

        RamDiskDevice = MmAllocateNonPagedPool(sizeof(RAM_DISK_DEVICE),
                                               RAM_DISK_ALLOCATION_TAG);

        if (RamDiskDevice == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto DriverEntryEnd;
        }

        RtlZeroMemory(RamDiskDevice, sizeof(RAM_DISK_DEVICE));
        RamDiskDevice->PhysicalAddress =
                                       RamDiskResource->Header.PhysicalAddress;

        RamDiskDevice->Buffer = RamDiskResource->Header.VirtualAddress;
        RamDiskDevice->Size = RamDiskResource->Header.Size;
        DeviceId = RtlAtomicAdd32(&RamDiskNextIdentifier, 1);
        RtlPrintToString(DeviceIdString,
                         11,
                         CharacterEncodingDefault,
                         "RamDisk%x",
                         DeviceId);

        //
        // Create the RAM disk device.
        //

        Status = IoCreateDevice(RamDiskDriver,
                                RamDiskDevice,
                                NULL,
                                DeviceIdString,
                                DISK_CLASS_ID,
                                NULL,
                                NULL);

        if (!KSUCCESS(Status)) {
            goto DriverEntryEnd;
        }
    }

DriverEntryEnd:
    return Status;
}

KSTATUS
RamDiskAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a RAM disk is detected.

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

    //
    // The RAM disk is not a real device, so it is not expected to be
    // attaching to emerging stacks. A proper driver should examine the device
    // ID and call IoAttachDriverToDevice to connect their driver to a newly
    // enumerated device.
    //

    return STATUS_NOT_IMPLEMENTED;
}

VOID
RamDiskDispatchStateChange (
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
            break;

        case IrpMinorQueryChildren:
            Irp->U.QueryChildren.Children = NULL;
            Irp->U.QueryChildren.ChildCount = 0;
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
        // Complete the IRP unless there's a reason not to. Normal drivers
        // should only complete the IRP if they're a bus driver or an error
        // occurred. The RAM disk is special as it created itself (and so it is
        // its own bus driver).
        //

        if (CompleteIrp != FALSE) {
            IoCompleteIrp(RamDiskDriver, Irp, Status);
        }

    //
    // The IRP is completed and is on its way back up. In normal device
    // drivers, this would be where to process the IRP, as by this point the
    // bus driver has performed necessary work (like enabling access to the
    // device on the bus in the case of start IRPs).
    //

    } else {

        ASSERT(Irp->Direction == IrpUp);
    }

    return;
}

VOID
RamDiskDispatchOpen (
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

    PRAM_DISK_DEVICE Disk;
    PRAM_DISK_DEVICE DiskCopy;

    Disk = (PRAM_DISK_DEVICE)DeviceContext;
    DiskCopy = MmAllocatePagedPool(sizeof(RAM_DISK_DEVICE),
                                   RAM_DISK_ALLOCATION_TAG);

    if (DiskCopy == NULL) {
        IoCompleteIrp(RamDiskDriver, Irp, STATUS_INSUFFICIENT_RESOURCES);
        return;
    }

    RtlCopyMemory(DiskCopy, Disk, sizeof(RAM_DISK_DEVICE));
    Irp->U.Open.DeviceContext = DiskCopy;
    IoCompleteIrp(RamDiskDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
RamDiskDispatchClose (
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

    MmFreePagedPool(Irp->U.Close.DeviceContext);
    IoCompleteIrp(RamDiskDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
RamDiskDispatchIo (
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

    UINTN BytesToComplete;
    KSTATUS CompletionStatus;
    PRAM_DISK_DEVICE Disk;
    IO_OFFSET IoOffset;
    ULONG IrpReadWriteFlags;
    BOOL ReadWriteIrpPrepared;
    KSTATUS Status;
    BOOL ToIoBuffer;

    ASSERT(Irp->Direction == IrpDown);

    Disk = Irp->U.ReadWrite.DeviceContext;
    ReadWriteIrpPrepared = FALSE;

    ASSERT(IS_ALIGNED(Irp->U.ReadWrite.IoOffset, RAM_DISK_SECTOR_SIZE));
    ASSERT(IS_ALIGNED(Irp->U.ReadWrite.IoSizeInBytes, RAM_DISK_SECTOR_SIZE));
    ASSERT(Irp->U.ReadWrite.IoBuffer != NULL);

    Irp->U.ReadWrite.IoBytesCompleted = 0;
    IoOffset = Irp->U.ReadWrite.IoOffset;
    if (IoOffset >= Disk->Size) {
        Status = STATUS_OUT_OF_BOUNDS;
        goto DispatchIoEnd;
    }

    BytesToComplete = Irp->U.ReadWrite.IoSizeInBytes;
    if ((IoOffset + BytesToComplete) > Disk->Size) {
        BytesToComplete = Disk->Size - Irp->U.ReadWrite.IoOffset;
    }

    ToIoBuffer = TRUE;
    IrpReadWriteFlags = IRP_READ_WRITE_FLAG_POLLED;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        ToIoBuffer = FALSE;
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    //
    // Prepare the I/O buffer for polled I/O.
    //

    Status = IoPrepareReadWriteIrp(&(Irp->U.ReadWrite),
                                   1,
                                   0,
                                   MAX_ULONGLONG,
                                   IrpReadWriteFlags);

    if (!KSUCCESS(Status)) {
        goto DispatchIoEnd;
    }

    ReadWriteIrpPrepared = TRUE;

    //
    // Transfer the data between the disk and I/O buffer.
    //

    Status = MmCopyIoBufferData(Irp->U.ReadWrite.IoBuffer,
                                (PUCHAR)Disk->Buffer + IoOffset,
                                0,
                                BytesToComplete,
                                ToIoBuffer);

    if (!KSUCCESS(Status)) {
        goto DispatchIoEnd;
    }

    Irp->U.ReadWrite.IoBytesCompleted = BytesToComplete;

DispatchIoEnd:
    if (ReadWriteIrpPrepared != FALSE) {
        CompletionStatus = IoCompleteReadWriteIrp(&(Irp->U.ReadWrite),
                                                  IrpReadWriteFlags);

        if (!KSUCCESS(CompletionStatus) && KSUCCESS(Status)) {
            Status = CompletionStatus;
        }
    }

    Irp->U.ReadWrite.NewIoOffset = IoOffset + Irp->U.ReadWrite.IoBytesCompleted;
    IoCompleteIrp(RamDiskDriver, Irp, Status);
    return;
}

VOID
RamDiskDispatchSystemControl (
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
    PRAM_DISK_DEVICE Disk;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    ULONGLONG PropertiesFileSize;
    KSTATUS Status;

    Context = Irp->U.SystemControl.SystemContext;
    Disk = DeviceContext;
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
            Properties->BlockSize = RAM_DISK_SECTOR_SIZE;
            Properties->BlockCount = Disk->Size / RAM_DISK_SECTOR_SIZE;
            Properties->Size = Disk->Size;
            Lookup->Flags = LOOKUP_FLAG_NO_PAGE_CACHE;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(RamDiskDriver, Irp, Status);
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
            (Properties->BlockSize != RAM_DISK_SECTOR_SIZE) ||
            (Properties->BlockCount != (Disk->Size / RAM_DISK_SECTOR_SIZE)) ||
            (PropertiesFileSize != Disk->Size)) {

            Status = STATUS_NOT_SUPPORTED;

        } else {
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(RamDiskDriver, Irp, Status);
        break;

    //
    // Do not support ramdisk device truncation.
    //

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(RamDiskDriver, Irp, STATUS_NOT_SUPPORTED);
        break;

    //
    // Gather and return device information.
    //

    case IrpMinorSystemControlDeviceInformation:
        IoCompleteIrp(RamDiskDriver, Irp, STATUS_NOT_SUPPORTED);
        break;

    case IrpMinorSystemControlSynchronize:
        IoCompleteIrp(RamDiskDriver, Irp, STATUS_SUCCESS);
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

//
// --------------------------------------------------------- Internal Functions
//

