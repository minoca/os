/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    part.c

Abstract:

    This module implements the partition manager driver.

Author:

    Evan Green 30-Jan-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/lib/partlib.h>

//
// ---------------------------------------------------------------- Definitions
//

#define PARTITION_ALLOCATION_TAG 0x74726150 // 'traP'

#define PARTITION_STRING_SIZE sizeof("PartitionXXXXX")
#define PARTITION_STRING_FORMAT "Partition%d"
#define PARTITION_RAW_DISK_ID "RawDisk"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PARTITION_OBJECT_TYPE {
    PartitionObjectInvalid,
    PartitionObjectParent,
    PartitionObjectChild
} PARTITION_OBJECT_TYPE, *PPARTITION_OBJECT_TYPE;

/*++

Structure Description:

    This structure stores the common header for a partition manager object.

Members:

    Type - Stores the partition object type.

    ReferenceCount - Stores the reference count on the object.

--*/

typedef struct _PARTITION_OBJECT {
    PARTITION_OBJECT_TYPE Type;
    ULONG ReferenceCount;
} PARTITION_OBJECT, *PPARTITION_OBJECT;

/*++

Structure Description:

    This structure stores information about a partition parent context.

Members:

    Header - Store the common partition object header.

    Device - Stores the OS device this device belongs to.

    IoHandle - Stores an open I/O handle to the disk, active when the partition
        manager is reading the device.

    PartitionContext - Stores the partition context for this disk.

    Children - Stores an array of pointers to devices representing the device
        partitions. The raw disk is not included here.

    RawDisk - Stores a pointer to the raw disk device.

--*/

typedef struct _PARTITION_PARENT {
    PARTITION_OBJECT Header;
    PDEVICE Device;
    PIO_HANDLE IoHandle;
    PARTITION_CONTEXT PartitionContext;
    PDEVICE *Children;
    PDEVICE RawDisk;
} PARTITION_PARENT, *PPARTITION_PARENT;

/*++

Structure Description:

    This structure stores information about a particular partition.

Members:

    Header - Stores the common partition object header.

    Parent - Stores a pointer to the parent structure.

    Index - Stores the index into the partition information for this partition.

--*/

typedef struct _PARTITION_CHILD {
    PARTITION_OBJECT Header;
    PPARTITION_PARENT Parent;
    ULONG Index;
} PARTITION_CHILD, *PPARTITION_CHILD;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PartAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
PartDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
PartDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
PartDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
PartDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
PartDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
PartpEnumerateChildren (
    PIRP Irp,
    PPARTITION_PARENT Parent
    );

KSTATUS
PartpReadPartitionStructures (
    PPARTITION_PARENT Parent
    );

VOID
PartpHandleDeviceInformationRequest (
    PIRP Irp,
    PPARTITION_CHILD Child
    );

VOID
PartpHandleBlockInformationRequest (
    PIRP Irp,
    PPARTITION_CHILD Child
    );

PVOID
PartpAllocate (
    UINTN Size
    );

VOID
PartpFree (
    PVOID Memory
    );

KSTATUS
PartpRead (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    );

PPARTITION_CHILD
PartpCreateChild (
    PPARTITION_PARENT Parent,
    ULONG Index
    );

VOID
PartpAddReference (
    PPARTITION_OBJECT Object
    );

VOID
PartpReleaseReference (
    PPARTITION_OBJECT Object
    );

VOID
PartpDestroyDevice (
    PPARTITION_OBJECT Object
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER PartDriver;

UUID PartPartitionDeviceInformationUuid = PARTITION_DEVICE_INFORMATION_UUID;

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

    This routine implements the initial entry point of the partition
    library, called when the library is first loaded.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    Status code.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    PartDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = PartAddDevice;
    FunctionTable.DispatchStateChange = PartDispatchStateChange;
    FunctionTable.DispatchOpen = PartDispatchOpen;
    FunctionTable.DispatchClose = PartDispatchClose;
    FunctionTable.DispatchIo = PartDispatchIo;
    FunctionTable.DispatchSystemControl = PartDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
PartAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a disk is detected. The partition manager
    attaches to the disk.

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

    PPARTITION_PARENT Context;
    KSTATUS Status;

    Context = PartpAllocate(sizeof(PARTITION_PARENT));
    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Context, sizeof(PARTITION_PARENT));
    Context->Header.Type = PartitionObjectParent;
    Context->Header.ReferenceCount = 1;
    Context->Device = DeviceToken;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, Context);

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Context != NULL) {
            PartpFree(Context);
            Context = NULL;
        }
    }

    return Status;
}

VOID
PartDispatchStateChange (
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

    PPARTITION_CHILD Child;
    PPARTITION_OBJECT Object;
    PPARTITION_PARENT Parent;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Object = (PPARTITION_OBJECT)DeviceContext;
    switch (Object->Type) {

    //
    // If this is the functional driver for the disk itself, usurp the query
    // children IRP but don't alter any other IRP paths.
    //

    case PartitionObjectParent:
        Parent = PARENT_STRUCTURE(Object, PARTITION_PARENT, Header);
        switch (Irp->MinorCode) {
        case IrpMinorQueryChildren:
            Status = PartpEnumerateChildren(Irp, Parent);
            IoCompleteIrp(PartDriver, Irp, Status);
            break;

        case IrpMinorRemoveDevice:
            if (Irp->Direction == IrpUp) {
                PartpReleaseReference(Object);
            }

            break;

        //
        // For all other IRPs, do nothing.
        //

        default:
            break;
        }

        break;

    //
    // If this is a child, then this driver is being called as the bus driver.
    // Complete state change IRPs so they don't make it down to the disk.
    //

    case PartitionObjectChild:
        Child = PARENT_STRUCTURE(Object, PARTITION_CHILD, Header);
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            IoCompleteIrp(PartDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorStartDevice:

            //
            // Publish the partition information device type.
            //

            Status = IoRegisterDeviceInformation(
                                           Irp->Device,
                                           &PartPartitionDeviceInformationUuid,
                                           TRUE);

            IoCompleteIrp(PartDriver, Irp, Status);
            break;

        case IrpMinorQueryChildren:
            Status = STATUS_SUCCESS;

            //
            // If this is the raw disk coming up, then read off the partition
            // information.
            //

            if (Child->Index == -1) {
                Status = PartpReadPartitionStructures(Child->Parent);
            }

            IoCompleteIrp(PartDriver, Irp, Status);
            break;

        case IrpMinorRemoveDevice:
            IoRegisterDeviceInformation(Irp->Device,
                                        &PartPartitionDeviceInformationUuid,
                                        FALSE);

            PartpReleaseReference(Object);
            IoCompleteIrp(PartDriver, Irp, STATUS_SUCCESS);
            break;

        //
        // For all other IRPs, do nothing.
        //

        default:
            break;
        }

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
PartDispatchOpen (
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

    //
    // Let the IRP head down and be handled by the disk directly.
    //

    return;
}

VOID
PartDispatchClose (
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

    //
    // Let the IRP be handled by the disk.
    //

    return;
}

VOID
PartDispatchIo (
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

    ULONGLONG BlockAddress;
    ULONGLONG BlockCount;
    ULONG BlockShift;
    ULONG BlockSize;
    PPARTITION_CHILD Child;
    PPARTITION_OBJECT Object;
    ULONGLONG OriginalBlockCount;
    PPARTITION_INFORMATION Partition;
    PIRP_READ_WRITE ReadWrite;
    KSTATUS Status;

    Object = (PPARTITION_OBJECT)DeviceContext;

    //
    // Don't process I/O as the parent (bus driver), let that head down to the
    // disk.
    //

    if (Object->Type != PartitionObjectChild) {
        return;
    }

    ASSERT(Irp->MajorCode == IrpMajorIo);

    Child = PARENT_STRUCTURE(Object, PARTITION_CHILD, Header);

    //
    // If this is the raw disk partition, let the IRP continue down to the disk
    // unmolested.
    //

    if (Child->Index == -1) {
        return;
    }

    BlockShift = Child->Parent->PartitionContext.BlockShift;
    BlockSize = Child->Parent->PartitionContext.BlockSize;

    ASSERT(BlockSize != 0);

    ReadWrite = &(Irp->U.ReadWrite);
    Partition = &(Child->Parent->PartitionContext.Partitions[Child->Index]);
    if (Irp->Direction == IrpDown) {

        //
        // On the way down, convert the offset into a block address, translate
        // the partition-relative block address into a disk block address, and
        // then convert back into bytes.
        //

        ASSERT((IS_ALIGNED(ReadWrite->IoOffset, BlockSize)) &&
               (IS_ALIGNED(ReadWrite->IoSizeInBytes, BlockSize)));

        BlockAddress = ReadWrite->IoOffset >> BlockShift;
        BlockCount = ReadWrite->IoSizeInBytes >> BlockShift;
        OriginalBlockCount = BlockCount;
        Status = PartTranslateIo(Partition, &BlockAddress, &BlockCount);
        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        ReadWrite->IoOffset = BlockAddress << BlockShift;
        ReadWrite->NewIoOffset += Partition->StartOffset << BlockShift;
        if (BlockCount != OriginalBlockCount) {
            ReadWrite->IoSizeInBytes = BlockCount << BlockShift;
        }

    //
    // On the way back up, re-adjust the I/O offset and new I/O offset.
    //

    } else {

        ASSERT(Irp->Direction == IrpUp);
        ASSERT(ReadWrite->IoOffset >= (Partition->StartOffset << BlockShift));

        ReadWrite->IoOffset -= Partition->StartOffset << BlockShift;

        ASSERT(ReadWrite->NewIoOffset >= Partition->StartOffset << BlockShift);

        ReadWrite->NewIoOffset -= Partition->StartOffset << BlockShift;
    }

    Status = STATUS_SUCCESS;

DispatchIoEnd:

    //
    // If something bad happened, don't let this get down to the disk.
    // Otherwise, let it flow to the disk.
    //

    if (!KSUCCESS(Status)) {
        IoCompleteIrp(PartDriver, Irp, Status);
    }

    return;
}

VOID
PartDispatchSystemControl (
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

    ULONGLONG BlockCount;
    ULONG BlockSize;
    PPARTITION_CHILD Child;
    PVOID Context;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    ULONGLONG FileSize;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PPARTITION_OBJECT Object;
    PPARTITION_PARENT Parent;
    PPARTITION_INFORMATION Partition;
    PPARTITION_CONTEXT PartitionContext;
    PFILE_PROPERTIES Properties;
    ULONGLONG PropertiesFileSize;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    Object = (PPARTITION_OBJECT)DeviceContext;
    Context = Irp->U.SystemControl.SystemContext;
    if (Irp->Direction != IrpDown) {
        return;
    }

    if (Object->Type == PartitionObjectParent) {
        Parent = PARENT_STRUCTURE(Object, PARTITION_PARENT, Header);
        switch (Irp->MinorCode) {

        //
        // If the IRP is destined for the disk, then explicitly complete it as
        // "not supported" so the object manager will enumerate children. If
        // this is IRP was actually sent to the raw disk child, then let it
        // flow down to the disk.
        //

        case IrpMinorSystemControlLookup:
            if (Irp->Device != Parent->RawDisk) {
                IoCompleteIrp(PartDriver, Irp, STATUS_NOT_HANDLED);
            }

            break;

        default:
            break;
        }

    } else {

        ASSERT(Object->Type == PartitionObjectChild);

        Child = PARENT_STRUCTURE(Object, PARTITION_CHILD, Header);
        PartitionContext = &(Child->Parent->PartitionContext);
        BlockSize = PartitionContext->BlockSize;
        Partition = NULL;
        BlockCount = 0;
        if (Child->Index != -1) {
            Partition = &(PartitionContext->Partitions[Child->Index]);
            BlockCount = Partition->EndOffset - Partition->StartOffset;
        }

        FileSize = BlockCount << PartitionContext->BlockShift;
        switch (Irp->MinorCode) {
        case IrpMinorSystemControlLookup:

            //
            // Let the IRP pass down to the disk if this is the raw disk child.
            //

            if (Child->Index == -1) {
                break;
            }

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
                Properties->BlockSize = BlockSize;
                Properties->BlockCount = BlockCount;
                Properties->Size = FileSize;
                Status = STATUS_SUCCESS;
            }

            IoCompleteIrp(PartDriver, Irp, Status);
            break;

        //
        // Writes to the disk's properties are not allowed. Fail if the data
        // has changed.
        //

        case IrpMinorSystemControlWriteFileProperties:
            if (Child->Index == -1) {
                break;
            }

            FileOperation = (PSYSTEM_CONTROL_FILE_OPERATION)Context;
            Properties = FileOperation->FileProperties;
            PropertiesFileSize = Properties->Size;
            if ((Properties->FileId != 0) ||
                (Properties->Type != IoObjectBlockDevice) ||
                (Properties->HardLinkCount != 1) ||
                (Properties->BlockSize != BlockSize) ||
                (Properties->BlockCount != BlockCount) ||
                (PropertiesFileSize != FileSize)) {

                Status = STATUS_NOT_SUPPORTED;

            } else {
                Status = STATUS_SUCCESS;
            }

            IoCompleteIrp(PartDriver, Irp, Status);
            break;

        //
        // Handle get/set device information requests.
        //

        case IrpMinorSystemControlDeviceInformation:
            PartpHandleDeviceInformationRequest(Irp, Child);
            break;

        case IrpMinorSystemControlGetBlockInformation:
            PartpHandleBlockInformationRequest(Irp, Child);
            break;

        //
        // Let synchronize requests go down to the disk.
        //

        case IrpMinorSystemControlSynchronize:
            break;

        //
        // Other operations are not supported.
        //

        default:
            IoCompleteIrp(PartDriver, Irp, STATUS_NOT_SUPPORTED);
            break;
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PartpEnumerateChildren (
    PIRP Irp,
    PPARTITION_PARENT Parent
    )

/*++

Routine Description:

    This routine is called to respond to enumeration requests of the parent.
    On the very first iteration, it enumerates only the raw disk. When the
    raw disk comes up, it reads the partition information and re-enumerates
    the parent. This routine is then called again an enumreates both the raw
    disk and all the partitions.

Arguments:

    Irp - Supplies a pointer to the query children IRP.

    Parent - Supplies a pointer to the partition parent.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PPARTITION_CHILD Child;
    CHAR DeviceId[PARTITION_STRING_SIZE];
    PPARTITION_INFORMATION Information;
    PPARTITION_CONTEXT PartitionContext;
    ULONG PartitionIndex;
    PARTITION_TYPE PartitionType;
    KSTATUS Status;

    Child = NULL;

    ASSERT(Irp->MinorCode == IrpMinorQueryChildren);

    //
    // If the raw disk has yet to be created, do that now.
    //

    if (Parent->RawDisk == NULL) {
        Child = PartpCreateChild(Parent, -1);
        if (Child == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto EnumerateChildrenEnd;
        }

        Status = IoCreateDevice(PartDriver,
                                Child,
                                Parent->Device,
                                PARTITION_RAW_DISK_ID,
                                PARTITION_CLASS_ID,
                                NULL,
                                &(Parent->RawDisk));

        if (!KSUCCESS(Status)) {
            goto EnumerateChildrenEnd;
        }

        Child = NULL;
        Status = IoSetTargetDevice(Parent->RawDisk, Parent->Device);
        if (!KSUCCESS(Status)) {
            goto EnumerateChildrenEnd;
        }
    }

    PartitionContext = &(Parent->PartitionContext);

    //
    // Allocate the array of children if needed.
    //

    if ((Parent->Children == NULL) && (PartitionContext->BlockSize != 0) &&
        (PartitionContext->PartitionCount != 0)) {

        AllocationSize = sizeof(PDEVICE) * PartitionContext->PartitionCount;
        Parent->Children = MmAllocatePagedPool(AllocationSize,
                                               PARTITION_ALLOCATION_TAG);

        if (Parent->Children == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto EnumerateChildrenEnd;
        }

        RtlZeroMemory(Parent->Children, AllocationSize);
    }

    //
    // Create a child device for every partition if needed.
    //

    for (PartitionIndex = 0;
         PartitionIndex < PartitionContext->PartitionCount;
         PartitionIndex += 1) {

        Information = &(PartitionContext->Partitions[PartitionIndex]);

        //
        // Skip empty and extended partitions.
        //

        PartitionType = Information->PartitionType;
        if ((PartitionType == PartitionTypeInvalid) ||
            (PartitionType == PartitionTypeEmpty) ||
            (PartitionType == PartitionTypeDosExtended) ||
            (PartitionType == PartitionTypeDosExtendedLba)) {

            continue;
        }

        if (Parent->Children[PartitionIndex] != NULL) {
            continue;
        }

        Child = PartpCreateChild(Parent, PartitionIndex);
        if (Child == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto EnumerateChildrenEnd;
        }

        RtlPrintToString(DeviceId,
                         sizeof(DeviceId),
                         CharacterEncodingDefault,
                         PARTITION_STRING_FORMAT,
                         Information->Number);

        Status = IoCreateDevice(PartDriver,
                                Child,
                                Parent->Device,
                                DeviceId,
                                PARTITION_CLASS_ID,
                                NULL,
                                &(Parent->Children[PartitionIndex]));

        if (!KSUCCESS(Status)) {
            goto EnumerateChildrenEnd;
        }

        IoSetDeviceMountable(Parent->Children[PartitionIndex]);
        Child = NULL;
        Status = IoSetTargetDevice(Parent->Children[PartitionIndex],
                                   Parent->Device);

        if (!KSUCCESS(Status)) {
            goto EnumerateChildrenEnd;
        }
    }

    if (PartitionContext->PartitionCount != 0) {
        Status = IoMergeChildArrays(Irp,
                                    Parent->Children,
                                    PartitionContext->PartitionCount,
                                    PARTITION_ALLOCATION_TAG);

        if (!KSUCCESS(Status)) {
            goto EnumerateChildrenEnd;
        }
    }

    ASSERT(Parent->RawDisk != NULL);

    Status = IoMergeChildArrays(Irp,
                                &(Parent->RawDisk),
                                1,
                                PARTITION_ALLOCATION_TAG);

    if (!KSUCCESS(Status)) {
        goto EnumerateChildrenEnd;
    }

EnumerateChildrenEnd:
    if (Child != NULL) {
        PartpReleaseReference(&(Child->Header));
    }

    return Status;
}

KSTATUS
PartpReadPartitionStructures (
    PPARTITION_PARENT Parent
    )

/*++

Routine Description:

    This routine is called to enumerate the partitions in a disk. This routine
    is called by a disk device upon starting.

Arguments:

    Irp - Supplies a pointer to the query children IRP.

    Parent - Supplies a pointer to the partition parent.

Return Value:

    Status code.

--*/

{

    ULONGLONG DiskCapacity;
    ULONG DiskOffsetAlignment;
    ULONG DiskSizeAlignment;
    PPARTITION_CONTEXT PartitionContext;
    KSTATUS Status;

    PartitionContext = &(Parent->PartitionContext);
    Status = STATUS_SUCCESS;

    //
    // Do nothing if the information has already been gathered.
    //

    if (PartitionContext->BlockSize != 0) {
        goto ReadPartitionStructuresEnd;
    }

    Status = IoOpenDevice(Parent->RawDisk,
                          IO_ACCESS_READ,
                          0,
                          &(Parent->IoHandle),
                          &DiskOffsetAlignment,
                          &DiskSizeAlignment,
                          &DiskCapacity);

    if (!KSUCCESS(Status)) {
        goto ReadPartitionStructuresEnd;
    }

    PartitionContext->AllocateFunction = PartpAllocate;
    PartitionContext->FreeFunction = PartpFree;
    PartitionContext->ReadFunction = PartpRead;
    PartitionContext->BlockSize = DiskOffsetAlignment;
    PartitionContext->BlockCount = DiskCapacity / PartitionContext->BlockSize;
    PartitionContext->Alignment = MmGetIoBufferAlignment();
    Status = PartInitialize(PartitionContext);
    if (!KSUCCESS(Status)) {
        PartitionContext->BlockSize = 0;
        goto ReadPartitionStructuresEnd;
    }

    Status = PartEnumeratePartitions(PartitionContext);

    //
    // If the partition table isn't valid or no partitions enumreate, make the
    // entire disk mountable, maybe there's just a raw file system here.
    //

    if ((Status == STATUS_NO_ELIGIBLE_DEVICES) ||
        ((KSUCCESS(Status)) && (PartitionContext->PartitionCount == 0))) {

        IoSetDeviceMountable(Parent->RawDisk);
        Status = STATUS_SUCCESS;

    } else if (!KSUCCESS(Status)) {

        //
        // For other failures, clear the block size so it tries again next
        // time around.
        //

        PartitionContext->BlockSize = 0;
        goto ReadPartitionStructuresEnd;
    }

    //
    // Poke the system to re-enumerate the parent. Failure is not fatal.
    //

    IoNotifyDeviceTopologyChange(Parent->Device);

ReadPartitionStructuresEnd:
    if (Parent->IoHandle != NULL) {
        IoClose(Parent->IoHandle);
        Parent->IoHandle = NULL;
    }

    return Status;
}

VOID
PartpHandleDeviceInformationRequest (
    PIRP Irp,
    PPARTITION_CHILD Child
    )

/*++

Routine Description:

    This routine handles requests to get and set device information for the
    partition.

Arguments:

    Irp - Supplies a pointer to the IRP making the request.

    Child - Supplies a pointer to the partition context.

Return Value:

    None. Any completion status is set in the IRP.

--*/

{

    PPARTITION_DEVICE_INFORMATION Information;
    BOOL Match;
    PPARTITION_PARENT Parent;
    PPARTITION_CONTEXT PartitionContext;
    PPARTITION_INFORMATION PartitionInformation;
    PSYSTEM_CONTROL_DEVICE_INFORMATION Request;
    KSTATUS Status;

    Request = Irp->U.SystemControl.SystemContext;

    //
    // If this is not a request for the partition device information, ignore it.
    //

    Match = RtlAreUuidsEqual(&(Request->Uuid),
                             &PartPartitionDeviceInformationUuid);

    if (Match == FALSE) {
        return;
    }

    //
    // Setting partition information is not supported.
    //

    if (Request->Set != FALSE) {
        Status = STATUS_ACCESS_DENIED;
        goto HandleDeviceInformationRequestEnd;
    }

    //
    // Make sure the size is large enough.
    //

    if (Request->DataSize < sizeof(PARTITION_DEVICE_INFORMATION)) {
        Request->DataSize = sizeof(PARTITION_DEVICE_INFORMATION);
        Status = STATUS_BUFFER_TOO_SMALL;
        goto HandleDeviceInformationRequestEnd;
    }

    ASSERT(Child->Header.Type == PartitionObjectChild);

    Parent = Child->Parent;
    PartitionContext = &(Parent->PartitionContext);
    Request->DataSize = sizeof(PARTITION_DEVICE_INFORMATION);
    Information = Request->Data;
    RtlZeroMemory(Information, sizeof(PARTITION_DEVICE_INFORMATION));
    Information->Version = PARTITION_DEVICE_INFORMATION_VERSION;
    Information->PartitionFormat = PartitionContext->Format;
    Information->BlockSize = PartitionContext->BlockSize;
    RtlCopyMemory(&(Information->DiskId),
                  &(PartitionContext->DiskIdentifier),
                  sizeof(Information->DiskId));

    //
    // If the index is -1, fill out the information for the parent disk itself.
    //

    if (Child->Index == -1) {
        Information->PartitionType = PartitionTypeNone;
        Information->Flags = PARTITION_FLAG_RAW_DISK;
        Information->FirstBlock = 0;
        Information->LastBlock = PartitionContext->BlockCount - 1;

    //
    // Fill out the information for the specific partition.
    //

    } else {

        ASSERT(Child->Index < PartitionContext->PartitionCount);

        PartitionInformation = &(PartitionContext->Partitions[Child->Index]);
        Information->PartitionType = PartitionInformation->PartitionType;
        Information->Flags = PartitionInformation->Flags;
        Information->FirstBlock = PartitionInformation->StartOffset;
        Information->LastBlock = PartitionInformation->EndOffset - 1;
        Information->Number = PartitionInformation->Number;
        Information->ParentNumber = PartitionInformation->ParentNumber;
        RtlCopyMemory(&(Information->PartitionId),
                      &(PartitionInformation->Identifier),
                      sizeof(Information->PartitionId));

        RtlCopyMemory(&(Information->PartitionTypeId),
                      &(PartitionInformation->TypeIdentifier),
                      sizeof(Information->PartitionTypeId));
    }

    Status = STATUS_SUCCESS;

HandleDeviceInformationRequestEnd:
    IoCompleteIrp(PartDriver, Irp, Status);
    return;
}

VOID
PartpHandleBlockInformationRequest (
    PIRP Irp,
    PPARTITION_CHILD Child
    )

/*++

Routine Description:

    This routine handles requests to get block information for the partition.

Arguments:

    Irp - Supplies a pointer to the IRP making the request.

    Child - Supplies a pointer to the partition context.

Return Value:

    None. Any completion status is set in the IRP.

--*/

{

    ULONGLONG BlockCount;
    PFILE_BLOCK_ENTRY BlockEntry;
    PFILE_BLOCK_INFORMATION BlockInformation;
    PLIST_ENTRY CurrentEntry;
    PPARTITION_PARENT Parent;
    PPARTITION_CONTEXT PartitionContext;
    PPARTITION_INFORMATION PartitionInformation;
    PPARTITION_INFORMATION Partitions;
    PSYSTEM_CONTROL_GET_BLOCK_INFORMATION Request;
    KSTATUS Status;

    ASSERT(Child->Header.Type == PartitionObjectChild);

    Request = Irp->U.SystemControl.SystemContext;
    BlockInformation = Request->FileBlockInformation;
    Parent = Child->Parent;
    PartitionContext = &(Parent->PartitionContext);

    //
    // If the request already contains non-empty file information, then the
    // partition is being requested to convert relative block offsets into
    // absolute block offsets.
    //

    if ((BlockInformation != NULL) &&
        (LIST_EMPTY(&(BlockInformation->BlockList)) == FALSE)) {

        //
        // If the index is -1, then the offsets are already accurate as there
        // are no partitions in the way.
        //

        if (Child->Index == -1) {
            Status = STATUS_SUCCESS;
            goto HandleBlockInformationRequestEnd;
        }

        ASSERT(Child->Index < PartitionContext->PartitionCount);

        Partitions = PartitionContext->Partitions;
        PartitionInformation = &(Partitions[Child->Index]);

        //
        // Otherwise, iterate over the list and convert the addresses.
        //

        CurrentEntry = BlockInformation->BlockList.Next;
        while (CurrentEntry != &(BlockInformation->BlockList)) {
            BlockEntry = LIST_VALUE(CurrentEntry, FILE_BLOCK_ENTRY, ListEntry);
            Status = PartTranslateIo(PartitionInformation,
                                     &(BlockEntry->Address),
                                     &(BlockEntry->Count));

            if (!KSUCCESS(Status)) {
                goto HandleBlockInformationRequestEnd;
            }

            CurrentEntry = CurrentEntry->Next;
        }

    //
    // Otherwise this is a request for the absolute block offsets and size of
    // the partition.
    //

    } else {
        if (BlockInformation == NULL) {
            BlockInformation = MmAllocateNonPagedPool(
                                                sizeof(FILE_BLOCK_INFORMATION),
                                                PARTITION_ALLOCATION_TAG);

            if (BlockInformation == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto HandleBlockInformationRequestEnd;
            }

            INITIALIZE_LIST_HEAD(&(BlockInformation->BlockList));
        }

        BlockEntry = MmAllocateNonPagedPool(sizeof(FILE_BLOCK_ENTRY),
                                            PARTITION_ALLOCATION_TAG);

        if (BlockEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto HandleBlockInformationRequestEnd;
        }

        //
        // If the index is -1, then get the information for the whole parent
        // disk.
        //

        if (Child->Index == -1) {
            BlockEntry->Address = 0;
            BlockEntry->Count = PartitionContext->BlockCount;

        //
        // Otherwise return the information for the particular partition.
        //

        } else {

            ASSERT(Child->Index < PartitionContext->PartitionCount);

            Partitions = PartitionContext->Partitions;
            PartitionInformation = &(Partitions[Child->Index]);
            BlockEntry->Address = PartitionInformation->StartOffset;
            BlockCount = PartitionInformation->EndOffset -
                         PartitionInformation->StartOffset;

            BlockEntry->Count = BlockCount;
        }

        INSERT_BEFORE(&(BlockEntry->ListEntry), &(BlockInformation->BlockList));

        //
        // Fill the block information back into the request, in case it was
        // allocated.
        //

        Request->FileBlockInformation = BlockInformation;
    }

    Status = STATUS_SUCCESS;

HandleBlockInformationRequestEnd:
    if (!KSUCCESS(Status)) {
        if ((BlockInformation != NULL) &&
            (BlockInformation != Request->FileBlockInformation)) {

            while (LIST_EMPTY(&(BlockInformation->BlockList)) == FALSE) {
                BlockEntry = LIST_VALUE(BlockInformation->BlockList.Next,
                                        FILE_BLOCK_ENTRY,
                                        ListEntry);

                LIST_REMOVE(&(BlockEntry->ListEntry));
                MmFreeNonPagedPool(BlockEntry);
            }

            MmFreeNonPagedPool(BlockInformation);
        }
    }

    IoCompleteIrp(PartDriver, Irp, Status);
    return;
}

PVOID
PartpAllocate (
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates memory for the partition manager.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    return MmAllocateNonPagedPool(Size, PARTITION_ALLOCATION_TAG);
}

VOID
PartpFree (
    PVOID Memory
    )

/*++

Routine Description:

    This routine frees allocated memory for the partition manager.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

{

    MmFreeNonPagedPool(Memory);
    return;
}

KSTATUS
PartpRead (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine reads from the underlying disk.

Arguments:

    Context - Supplies the partition context identifying the disk.

    BlockAddress - Supplies the block address to read.

    Buffer - Supplies a pointer where the data will be returned on success.
        This buffer is expected to be one block in size (as specified in the
        partition context).

Return Value:

    Status code.

--*/

{

    UINTN BytesCompleted;
    PIO_BUFFER IoBuffer;
    PPARTITION_PARENT Parent;
    KSTATUS Status;

    Parent = PARENT_STRUCTURE(Context, PARTITION_PARENT, PartitionContext);

    ASSERT((Parent->Header.Type == PartitionObjectParent) &&
           (Parent->IoHandle != NULL) &&
           (Context->BlockSize != 0));

    Status = MmCreateIoBuffer(Buffer,
                              Context->BlockSize,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &IoBuffer);

    if (!KSUCCESS(Status)) {
        goto ReadEnd;
    }

    Status = IoReadAtOffset(Parent->IoHandle,
                            IoBuffer,
                            BlockAddress << Context->BlockShift,
                            Context->BlockSize,
                            0,
                            WAIT_TIME_INDEFINITE,
                            &BytesCompleted,
                            NULL);

    if (!KSUCCESS(Status)) {
        goto ReadEnd;
    }

ReadEnd:
    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return Status;
}

PPARTITION_CHILD
PartpCreateChild (
    PPARTITION_PARENT Parent,
    ULONG Index
    )

/*++

Routine Description:

    This routine creates a partition child structure.

Arguments:

    Parent - Supplies a pointer to the parent structure.

    Index - Supplies the child index for this child.

Return Value:

    Returns a pointer to the new child structure on success.

    NULL on allocation failure.

--*/

{

    PPARTITION_CHILD Child;

    Child = PartpAllocate(sizeof(PARTITION_CHILD));
    if (Child == NULL) {
        return NULL;
    }

    RtlZeroMemory(Child, sizeof(PARTITION_CHILD));
    Child->Header.Type = PartitionObjectChild;
    Child->Header.ReferenceCount = 1;
    Child->Parent = Parent;
    PartpAddReference(&(Parent->Header));
    Child->Index = Index;
    return Child;
}

VOID
PartpAddReference (
    PPARTITION_OBJECT Object
    )

/*++

Routine Description:

    This routine adds a reference on the given partition manager context.

Arguments:

    Object - Supplies a pointer to the partition manager object.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Object->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    return;
}

VOID
PartpReleaseReference (
    PPARTITION_OBJECT Object
    )

/*++

Routine Description:

    This routine releases a reference on a partition object.

Arguments:

    Object - Supplies a pointer to the partition manager object.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Object->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        PartpDestroyDevice(Object);
    }

    return;
}

VOID
PartpDestroyDevice (
    PPARTITION_OBJECT Object
    )

/*++

Routine Description:

    This routine destroys a partition manager object.

Arguments:

    Object - Supplies a pointer to the partition manager object.

Return Value:

    None.

--*/

{

    PPARTITION_CHILD Child;
    PPARTITION_PARENT Parent;

    switch (Object->Type) {
    case PartitionObjectParent:
        Parent = PARENT_STRUCTURE(Object, PARTITION_PARENT, Header);
        if (Parent->PartitionContext.BlockSize != 0) {
            PartDestroy(&(Parent->PartitionContext));
            Parent->PartitionContext.BlockSize = 0;
        }

        if (Parent->Children != NULL) {
            MmFreePagedPool(Parent->Children);
        }

        ASSERT(Parent->IoHandle == NULL);

        Parent->Header.Type = PartitionObjectInvalid;
        PartpFree(Parent);
        break;

    case PartitionObjectChild:
        Child = PARENT_STRUCTURE(Object, PARTITION_CHILD, Header);
        PartpReleaseReference(&(Child->Parent->Header));
        Child->Header.Type = PartitionObjectInvalid;
        PartpFree(Child);
        break;

    default:

        ASSERT(FALSE);

        return;
    }

    return;
}

