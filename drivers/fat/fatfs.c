/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatfs.c

Abstract:

    This module implements the File Allocation Table (FAT) file system driver.

Author:

    Evan Green 25-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/lib/fat/fat.h>

//
// ---------------------------------------------------------------- Definitions
//

#define FAT_VOLUME_ALLOCATION_TAG 0x56746146 // 'VtaF'
#define FAT_TRANSFER_ALLOCATION_TAG 0x54746146 // 'TtaF'
#define FAT_FILE_ALLOCATION_TAG 0x46746146 // 'FtaF'
#define FAT_BUFFER_ALLOCATION_TAG 0x42746146 // 'BtaF'
#define FAT_DIRECTORY_ALLOCATION_TAG 0x44746146 // 'DtaF'

//
// Define Fat FS file flags.
//

#define FATFS_FLAG_DIRECTORY 0x00000001
#define FATFS_FLAG_PAGE_FILE 0x00000002

//
// Try to zero fill files in big chunks
//

#define FAT_ZERO_BUFFER_SIZE (512 * 1024)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a FAT volume.

Members:

    MediaHandle - Stores the handle to the underlying device.

    VolumeToken - Stores the volume token supplied by the FAT library.

    Attached - Stores a boolean indicating whether the volume is attached.

    DirectoryTree - Stores the tree of fat FS directory file objects.

    DirectoryTreeLock - Stores the lock that protects the directory file tree.

    RootDirectoryCluster - Stores the cluster number of the root directory.

    ReferenceCount - Stores the reference count of the FAT FS volume.

--*/

typedef struct _FATFS_VOLUME {
    HANDLE MediaHandle;
    PVOID VolumeToken;
    BOOL Attached;
    RED_BLACK_TREE DirectoryTree;
    KSPIN_LOCK DirectoryTreeLock;
    ULONG RootDirectoryCluster;
    volatile ULONG ReferenceCount;
} FATFS_VOLUME, *PFATFS_VOLUME;

/*++

Structure Description:

    This structure stores information about a FAT directory object.

Members:

    TreeNode - Stores the information for the file object within the FAT FS
        volume file tree.

    ReferenceCount - Stores the number of references taken on the directory
        file object.

    Cluster - Stores the cluster number of this file.

    DirectoryLock - Stores a pointer to the lock that synchronizes access to
        this directory.

--*/

typedef struct _FATFS_DIRECTORY_OBJECT {
    RED_BLACK_TREE_NODE TreeNode;
    volatile ULONG ReferenceCount;
    ULONG Cluster;
    PQUEUED_LOCK Lock;
} FATFS_DIRECTORY_OBJECT, *PFATFS_DIRECTORY_OBJECT;

/*++

Structure Description:

    This structure stores information about a FAT file.

Members:

    FileToken - Stores the token provided by the underlying FAT library.

    OpenFlags - Stores the flags regarding the file. See FATFS_FLAG_*
        definitions.

--*/

typedef struct _FATFS_FILE {
    PVOID FileToken;
    ULONG Flags;
} FATFS_FILE, *PFATFS_FILE;

/*++

Structure Description:

    This structure stores information about a FAT file transfer.

Members:

    Volume - Stores a pointer to the FAT volume.

    DeviceIrp - Stores a pointer to the IRP to use to access the underlying
        block device.

--*/

typedef struct _FATFS_TRANSFER {
    PFATFS_VOLUME Volume;
    PIRP DeviceIrp;
} FATFS_TRANSFER, *PFATFS_TRANSFER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FatAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

KSTATUS
FatCreateIrp (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID *IrpContext,
    ULONG Flags
    );

VOID
FatDestroyIrp (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
FatDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
FatDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
FatDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
FatDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
FatDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
FatpRemoveDevice (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
FatpDestroyVolume (
    PFATFS_VOLUME Volume
    );

VOID
FatpVolumeAddReference (
    PFATFS_VOLUME Volume
    );

VOID
FatpVolumeReleaseReference (
    PFATFS_VOLUME Volume
    );

KSTATUS
FatpTruncateFile (
    PFATFS_VOLUME Volume,
    PFILE_PROPERTIES FileProperties,
    ULONGLONG NewSize,
    PVOID FileToken
    );

KSTATUS
FatpRenameFile (
    PFATFS_VOLUME Volume,
    PFILE_PROPERTIES SourceDirectory,
    PFILE_PROPERTIES SourceFile,
    PFILE_PROPERTIES DestinationDirectory,
    PFILE_PROPERTIES DestinationFile,
    PULONG SourceFileHardLinkDelta,
    PBOOL DestinationFileUnlinked,
    PULONGLONG DestinationDirectorySize,
    PSTR Name,
    ULONG NameSize
    );

PFATFS_DIRECTORY_OBJECT
FatpCreateDirectoryObject (
    PFATFS_VOLUME Volume,
    FILE_ID DirectoryFileId
    );

VOID
FatpDestroyDirectoryObject (
    PFATFS_DIRECTORY_OBJECT DirectoryObject
    );

VOID
FatpDirectoryObjectAddReference (
    PFATFS_VOLUME Volume,
    PFATFS_DIRECTORY_OBJECT DirectoryObject
    );

VOID
FatpDirectoryObjectReleaseReference (
    PFATFS_VOLUME Volume,
    PFATFS_DIRECTORY_OBJECT DirectoryObject
    );

KSTATUS
FatpGetDirectoryObject (
    PFATFS_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PFATFS_DIRECTORY_OBJECT *DirectoryObject
    );

KSTATUS
FatpGetParentDirectoryObject (
    PFATFS_VOLUME Volume,
    FILE_ID FileId,
    PFATFS_DIRECTORY_OBJECT *ParentDirectoryObject
    );

KSTATUS
FatpCreateOrLookupDirectoryObject (
    PFATFS_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PFATFS_DIRECTORY_OBJECT *DirectoryObject
    );

COMPARISON_RESULT
FatpCompareDirectoryObjectNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER FatDriver = NULL;

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

    This routine is the entry point for the FAT driver. It registers its other
    dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    FatDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = FatAddDevice;
    FunctionTable.CreateIrp = FatCreateIrp;
    FunctionTable.DestroyIrp = FatDestroyIrp;
    FunctionTable.DispatchStateChange = FatDispatchStateChange;
    FunctionTable.DispatchOpen = FatDispatchOpen;
    FunctionTable.DispatchClose = FatDispatchClose;
    FunctionTable.DispatchIo = FatDispatchIo;
    FunctionTable.DispatchSystemControl = FatDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    Status = IoRegisterFileSystem(Driver);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

DriverEntryEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FatAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a volume is detected. This is the FAT file
    system's opportunity to attach itself to the device stack.

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

    BLOCK_DEVICE_PARAMETERS BlockDeviceParameters;
    BOOL DeviceOpen;
    ULONG DiskAccess;
    PIO_HANDLE DiskHandle;
    PFATFS_VOLUME FatVolume;
    ULONG Flags;
    ULONGLONG IoCapacity;
    ULONG IoOffsetAlignment;
    ULONG IoSizeAlignment;
    BOOL PagingDevice;
    FILE_PROPERTIES RootProperties;
    KSTATUS Status;
    PDEVICE TargetDevice;
    KSTATUS UnmountStatus;

    DeviceOpen = FALSE;
    DiskHandle = INVALID_HANDLE;
    FatVolume = NULL;
    Flags = 0;
    RtlZeroMemory(&BlockDeviceParameters, sizeof(BLOCK_DEVICE_PARAMETERS));
    PagingDevice = IoIsPagingDevice(DeviceToken);
    if (PagingDevice != FALSE) {
        Flags |= OPEN_FLAG_PAGING_DEVICE;
    }

    TargetDevice = IoGetTargetDevice(DeviceToken);

    //
    // All volumes are backed by an I/O device of some kind.
    //

    ASSERT(TargetDevice != NULL);

    //
    // Open the underlying disk/partition supporting this volume.
    //

    DiskAccess = IO_ACCESS_READ | IO_ACCESS_WRITE;
    Status = IoOpenDevice(TargetDevice,
                          DiskAccess,
                          Flags,
                          &DiskHandle,
                          &IoOffsetAlignment,
                          &IoSizeAlignment,
                          &IoCapacity);

    //
    // If the disk could not be opened with write permissions, try for just
    // read.
    //

    if (!KSUCCESS(Status)) {
        DiskAccess = IO_ACCESS_READ;
        Status = IoOpenDevice(TargetDevice,
                              DiskAccess,
                              Flags,
                              &DiskHandle,
                              &IoOffsetAlignment,
                              &IoSizeAlignment,
                              &IoCapacity);

        if (!KSUCCESS(Status)) {
            goto AddDeviceEnd;
        }

        //
        // A read only disk should not be in the paging path.
        //

        ASSERT(PagingDevice == FALSE);
    }

    //
    // Allocate space for the driver data.
    //

    if (PagingDevice != FALSE) {
        FatVolume = MmAllocateNonPagedPool(sizeof(FATFS_VOLUME),
                                           FAT_VOLUME_ALLOCATION_TAG);

    } else {
        FatVolume = MmAllocatePagedPool(sizeof(FATFS_VOLUME),
                                        FAT_VOLUME_ALLOCATION_TAG);
    }

    if (FatVolume == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(FatVolume, sizeof(FATFS_VOLUME));
    KeInitializeSpinLock(&(FatVolume->DirectoryTreeLock));
    RtlRedBlackTreeInitialize(&(FatVolume->DirectoryTree),
                              0,
                              FatpCompareDirectoryObjectNodes);

    FatVolume->MediaHandle = DiskHandle;

    ASSERT(IoOffsetAlignment == IoSizeAlignment);

    RtlZeroMemory(&BlockDeviceParameters, sizeof(BLOCK_DEVICE_PARAMETERS));
    BlockDeviceParameters.DeviceToken = DiskHandle;
    BlockDeviceParameters.BlockSize = IoSizeAlignment;
    BlockDeviceParameters.BlockCount = IoCapacity / IoSizeAlignment;

    //
    // Attempt to mount the volume.
    //

    Status = FatOpenDevice(&BlockDeviceParameters);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    DeviceOpen = TRUE;
    Status = FatMount(&BlockDeviceParameters, 0, &(FatVolume->VolumeToken));
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    Status = FatLookup(FatVolume->VolumeToken,
                       TRUE,
                       0,
                       NULL,
                       0,
                       &RootProperties);

    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    FatVolume->RootDirectoryCluster = (ULONG)(RootProperties.FileId);

    //
    // The volume was successfully mounted, add the device.
    //

    Status = IoAttachDriverToDevice(Driver, DeviceToken, FatVolume);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    //
    // Now that it has been fully initialized, mark it as attached and give it
    // a reference of 1.
    //

    FatVolume->ReferenceCount = 1;
    FatVolume->Attached = TRUE;

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (FatVolume != NULL) {
            if (FatVolume->VolumeToken != NULL) {
                UnmountStatus = FatUnmount(FatVolume->VolumeToken);

                ASSERT(KSUCCESS(UnmountStatus));
            }

            MmFreeNonPagedPool(FatVolume);
        }

        if (DiskHandle != INVALID_HANDLE) {
            IoClose(DiskHandle);
        }

        if (DeviceOpen != FALSE) {
            FatCloseDevice(BlockDeviceParameters.DeviceToken);
        }
    }

    return Status;
}

KSTATUS
FatCreateIrp (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID *IrpContext,
    ULONG Flags
    )

/*++

Routine Description:

    This routine is called when an IRP is being created. It gives the driver a
    chance to allocate any additional state it may need to associate with the
    IRP.

Arguments:

    Irp - Supplies a pointer to the I/O request packet. The only variables the
        driver can count on staying constant are the device and the IRP Major
        Code. All other fields are subject to change throughout the lifetime of
        the IRP.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies a pointer where the driver can provide context
        associated with this specific IRP.

    Flags - Supplies a bitmask of IRP creation flags. See IRP_CREATE_FLAG_* for
        definitions.

Return Value:

    None.

--*/

{

    PDEVICE DiskDevice;
    KSTATUS Status;
    PFATFS_TRANSFER Transfer;
    PFATFS_VOLUME Volume;

    ASSERT(DeviceContext != NULL);

    Transfer = NULL;
    Volume = (PFATFS_VOLUME)DeviceContext;

    //
    // If this is going to be a no-allocate IRP, then this means that the
    // sender of the IRP is going to read from the backing device in a critical
    // code path that cannot handle allocations. Create a transfer and allocate
    // an IRP for the underlying device transfer.
    //

    if ((Flags & IRP_CREATE_FLAG_NO_ALLOCATE) != 0) {
        Transfer = MmAllocateNonPagedPool(sizeof(FATFS_TRANSFER),
                                          FAT_TRANSFER_ALLOCATION_TAG);

        if (Transfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateIrpEnd;
        }

        RtlZeroMemory(Transfer, sizeof(FATFS_TRANSFER));
        Transfer->Volume = Volume;
        Status = IoGetDevice(Volume->MediaHandle, &DiskDevice);
        if (!KSUCCESS(Status)) {
            goto CreateIrpEnd;
        }

        Transfer->DeviceIrp = IoCreateIrp(DiskDevice,
                                          IrpMajorIo,
                                          IRP_CREATE_FLAG_NO_ALLOCATE);

        if (Transfer->DeviceIrp == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateIrpEnd;
        }
    }

    Status = STATUS_SUCCESS;

CreateIrpEnd:
    if (!KSUCCESS(Status)) {
        if (Transfer != NULL) {
            if (Transfer->DeviceIrp != NULL) {
                IoDestroyIrp(Transfer->DeviceIrp);
            }

            MmFreeNonPagedPool(Transfer);
            Transfer = NULL;
        }
    }

    *IrpContext = Transfer;
    return Status;
}

VOID
FatDestroyIrp (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine destroys device context associated with an IRP.

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

    PFATFS_TRANSFER Transfer;

    Transfer = (PFATFS_TRANSFER)IrpContext;
    if (Transfer == NULL) {
        return;
    }

    if (Transfer->DeviceIrp != NULL) {
        IoDestroyIrp(Transfer->DeviceIrp);
    }

    MmFreeNonPagedPool(Transfer);
    return;
}

VOID
FatDispatchOpen (
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

    PFATFS_FILE FatFile;
    ULONG FatFsFlags;
    PFATFS_VOLUME FatVolume;
    PVOID FileToken;
    BOOL NonPaged;
    ULONG OpenFlags;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorOpen);
    ASSERT(Irp->MinorCode == IrpMinorOpen);
    ASSERT(DeviceContext != NULL);

    FatFile = NULL;
    FatFsFlags = 0;
    FileToken = NULL;
    NonPaged = FALSE;

    //
    // Make a passive effort to do nothing if the device is not connected.
    //

    FatVolume = (PFATFS_VOLUME)DeviceContext;

    ASSERT(FatVolume->Attached != FALSE);

    FatpVolumeAddReference(FatVolume);

    //
    // If this is a page file open request, then the backing device better
    // support page file access. Fail if this is not the case.
    //

    OpenFlags = Irp->U.Open.OpenFlags;
    if ((OpenFlags & OPEN_FLAG_PAGE_FILE) != 0) {
        FatFsFlags |= FATFS_FLAG_PAGE_FILE;
        if (IoIsPageFileAccessSupported(FatVolume->MediaHandle) == FALSE) {
            Status = STATUS_NO_ELIGIBLE_DEVICES;
            goto DispatchOpenEnd;
        }

        NonPaged = TRUE;
    }

    if (Irp->U.Open.FileProperties->Type == IoObjectRegularDirectory) {
        FatFsFlags |= FATFS_FLAG_DIRECTORY;
        OpenFlags |= OPEN_FLAG_DIRECTORY;
    }

    Status = FatOpenFileId(FatVolume->VolumeToken,
                           Irp->U.Open.FileProperties->FileId,
                           Irp->U.Open.DesiredAccess,
                           OpenFlags,
                           &FileToken);

    if (!KSUCCESS(Status)) {
        goto DispatchOpenEnd;
    }

    //
    // Allocate the FAT file system information.
    //

    if (NonPaged != FALSE) {
        FatFile = MmAllocateNonPagedPool(sizeof(FATFS_FILE),
                                         FAT_FILE_ALLOCATION_TAG);

    } else {
        FatFile = MmAllocatePagedPool(sizeof(FATFS_FILE),
                                      FAT_FILE_ALLOCATION_TAG);
    }

    if (FatFile == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DispatchOpenEnd;
    }

    RtlZeroMemory(FatFile, sizeof(FATFS_FILE));
    FatFile->FileToken = FileToken;
    FatFile->Flags = FatFsFlags;
    Irp->U.Open.DeviceContext = FatFile;
    Status = STATUS_SUCCESS;

DispatchOpenEnd:
    if (!KSUCCESS(Status)) {
        if (FatFile != NULL) {
            if (NonPaged != FALSE) {
                MmFreeNonPagedPool(FatFile);

            } else {
                MmFreePagedPool(FatFile);
            }
        }

        if (FileToken != NULL) {
            FatCloseFile(FileToken);
        }

        FatpVolumeReleaseReference(FatVolume);
    }

    IoCompleteIrp(FatDriver, Irp, Status);
    return;
}

VOID
FatDispatchClose (
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

    PFATFS_FILE FatFile;
    PFATFS_VOLUME FatVolume;

    ASSERT(Irp->MajorCode == IrpMajorClose);
    ASSERT(Irp->MinorCode == IrpMinorClose);

    FatFile = (PFATFS_FILE)Irp->U.Close.DeviceContext;
    FatVolume = (PFATFS_VOLUME)DeviceContext;
    FatCloseFile(FatFile->FileToken);
    if ((FatFile->Flags & FATFS_FLAG_PAGE_FILE) != 0) {
        MmFreeNonPagedPool(FatFile);

    } else {
        MmFreePagedPool(FatFile);
    }

    FatpVolumeReleaseReference(FatVolume);
    IoCompleteIrp(FatDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
FatDispatchIo (
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

    UINTN BytesCompleted;
    PFATFS_DIRECTORY_OBJECT DirectoryObject;
    PIRP DiskIrp;
    ULONG ElementsRead;
    PFATFS_FILE FatFile;
    FAT_SEEK_INFORMATION FatSeekInformation;
    PFATFS_VOLUME FatVolume;
    PFILE_PROPERTIES FileProperties;
    ULONGLONG FileSize;
    PIO_BUFFER IoBuffer;
    ULONGLONG IoOffset;
    KSTATUS Status;
    PFATFS_TRANSFER Transfer;

    ASSERT(Irp->Direction == IrpDown);
    ASSERT(Irp->MajorCode == IrpMajorIo);
    ASSERT(DeviceContext != NULL);

    //
    // Make a passive effort to do nothing if the device is not connected.
    //

    FatVolume = (PFATFS_VOLUME)DeviceContext;
    if (FatVolume->Attached == FALSE) {
        IoCompleteIrp(FatDriver, Irp,  STATUS_DEVICE_NOT_CONNECTED);
        return;
    }

    DiskIrp = NULL;
    FileProperties = NULL;
    FatFile = (PFATFS_FILE)(Irp->U.ReadWrite.DeviceContext);
    if (((FatFile->Flags & FATFS_FLAG_PAGE_FILE) == 0) ||
        ((Irp->U.ReadWrite.IoFlags & IO_FLAG_NO_ALLOCATE) == 0)) {

        FileProperties = Irp->U.ReadWrite.FileProperties;
    }

    Transfer = (PFATFS_TRANSFER)IrpContext;
    if (Transfer != NULL) {
        DiskIrp = Transfer->DeviceIrp;
    }

    IoBuffer = Irp->U.ReadWrite.IoBuffer;
    IoOffset = Irp->U.ReadWrite.IoOffset;

    //
    // All requests must supply an I/O buffer.
    //

    ASSERT(IoBuffer != NULL);

    //
    // Directory I/O is handled a little differently.
    //

    if ((FatFile->Flags & FATFS_FLAG_DIRECTORY) != 0) {

        //
        // Directories cannot be written to directly.
        //

        if (Irp->MinorCode == IrpMinorIoWrite) {
            Status = STATUS_ACCESS_DENIED;
            goto DispatchIoEnd;
        }

        ASSERT(FileProperties != NULL);

        //
        // Get the directory's object to synchronize access to the directory.
        // This can fail an allocation.
        //

        Status = FatpGetDirectoryObject(FatVolume,
                                        FileProperties->FileId,
                                        &DirectoryObject);

        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        //
        // Synchronize access to the directory.
        //

        KeAcquireQueuedLock(DirectoryObject->Lock);

        ASSERT(IoOffset >= DIRECTORY_CONTENTS_OFFSET);

        Status = FatEnumerateDirectory(FatFile->FileToken,
                                       IoOffset,
                                       IoBuffer,
                                       Irp->U.ReadWrite.IoSizeInBytes,
                                       FALSE,
                                       FALSE,
                                       DiskIrp,
                                       &(Irp->U.ReadWrite.IoBytesCompleted),
                                       &ElementsRead);

        KeReleaseQueuedLock(DirectoryObject->Lock);
        FatpDirectoryObjectReleaseReference(FatVolume, DirectoryObject);
        Irp->U.ReadWrite.NewIoOffset = IoOffset + ElementsRead;
        goto DispatchIoEnd;
    }

    //
    // If the seek didn't get all the way to the desired offset, write some
    // zeroes.
    //

    if ((Irp->MinorCode == IrpMinorIoWrite) && (FileProperties != NULL)) {
        FileSize = FileProperties->Size;
        if (FileSize < IoOffset) {
            Status = FatTruncate(FatVolume->VolumeToken,
                                 FatFile->FileToken,
                                 FileProperties->FileId,
                                 FileSize,
                                 IoOffset);

            if (!KSUCCESS(Status)) {
                goto DispatchIoEnd;
            }
        }
    }

    //
    // Seek to the desired offset within the file. If the seek reaches the end
    // of file and this is not a write operation, fail.
    //

    RtlZeroMemory(&FatSeekInformation, sizeof(FAT_SEEK_INFORMATION));
    Status = FatFileSeek(FatFile->FileToken,
                         DiskIrp,
                         Irp->U.ReadWrite.IoFlags,
                         SeekCommandFromBeginning,
                         IoOffset,
                         &FatSeekInformation);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_OUT_OF_BOUNDS) {
            Status = STATUS_END_OF_FILE;
        }

        goto DispatchIoEnd;
    }

    //
    // Read or write the requested region of the file.
    //

    BytesCompleted = 0;
    if (Irp->MinorCode == IrpMinorIoRead) {
        Status = FatReadFile(FatFile->FileToken,
                             &FatSeekInformation,
                             (PFAT_IO_BUFFER)IoBuffer,
                             Irp->U.ReadWrite.IoSizeInBytes,
                             Irp->U.ReadWrite.IoFlags,
                             DiskIrp,
                             &BytesCompleted);

    } else {

        ASSERT(Irp->MinorCode == IrpMinorIoWrite);

        Status = FatWriteFile(FatFile->FileToken,
                              &FatSeekInformation,
                              (PFAT_IO_BUFFER)IoBuffer,
                              Irp->U.ReadWrite.IoSizeInBytes,
                              Irp->U.ReadWrite.IoFlags,
                              DiskIrp,
                              &BytesCompleted);

    }

    Irp->U.ReadWrite.IoBytesCompleted = BytesCompleted;
    Irp->U.ReadWrite.NewIoOffset = IoOffset + BytesCompleted;

DispatchIoEnd:
    IoCompleteIrp(FatDriver, Irp, Status);
    return;
}

VOID
FatDispatchStateChange (
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

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    if (Irp->Direction == IrpDown) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            IoCompleteIrp(FatDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorStartDevice:
            IoCompleteIrp(FatDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorQueryChildren:
            Irp->U.QueryChildren.ChildCount = 0;
            Irp->U.QueryChildren.Children = NULL;
            IoCompleteIrp(FatDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorRemoveDevice:
            FatpRemoveDevice(Irp, DeviceContext, IrpContext);
            IoCompleteIrp(FatDriver, Irp, STATUS_SUCCESS);
            break;

        default:

            ASSERT(FALSE);

            IoCompleteIrp(FatDriver, Irp, STATUS_NOT_SUPPORTED);
            break;
        }
    }

    return;
}

VOID
FatDispatchSystemControl (
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

    PSYSTEM_CONTROL_GET_BLOCK_INFORMATION BlockInformation;
    PVOID Context;
    PSYSTEM_CONTROL_CREATE Create;
    FILE_ID DirectoryFileId;
    PFATFS_DIRECTORY_OBJECT DirectoryObject;
    PFATFS_FILE File;
    PFILE_BLOCK_INFORMATION *FileBlockInformation;
    FILE_ID FileId;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PSYSTEM_CONTROL_RENAME Rename;
    KSTATUS Status;
    PSYSTEM_CONTROL_TRUNCATE Truncate;
    PSYSTEM_CONTROL_UNLINK Unlink;
    PFATFS_VOLUME Volume;

    Volume = (PFATFS_VOLUME)DeviceContext;

    ASSERT(Volume != NULL);
    ASSERT(Volume->Attached != FALSE);

    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {

    //
    // Search for a file within a directory.
    //

    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        DirectoryFileId = 0;
        DirectoryObject = NULL;
        if (Lookup->DirectoryProperties != NULL) {
            DirectoryFileId = Lookup->DirectoryProperties->FileId;
            Status = FatpGetDirectoryObject(Volume,
                                            DirectoryFileId,
                                            &DirectoryObject);

            if (!KSUCCESS(Status)) {
                IoCompleteIrp(FatDriver, Irp, Status);
                break;
            }

            //
            // Lock the directory to prevent updates from racing with lookup.
            //

            KeAcquireQueuedLock(DirectoryObject->Lock);

            //
            // The system shouldn't be allowing look-ups on directories that
            // don't have any hard links.
            //

            ASSERT(Lookup->DirectoryProperties->HardLinkCount != 0);
        }

        Status = FatLookup(Volume->VolumeToken,
                           Lookup->Root,
                           DirectoryFileId,
                           Lookup->FileName,
                           Lookup->FileNameSize,
                           Lookup->Properties);

        if (Lookup->DirectoryProperties != NULL) {

            ASSERT(DirectoryObject != NULL);

            KeReleaseQueuedLock(DirectoryObject->Lock);
            FatpDirectoryObjectReleaseReference(Volume, DirectoryObject);
        }

        IoCompleteIrp(FatDriver, Irp, Status);
        break;

    //
    // Create a new file.
    //

    case IrpMinorSystemControlCreate:
        Create = (PSYSTEM_CONTROL_CREATE)Context;
        DirectoryFileId = Create->DirectoryProperties->FileId;
        Status = FatpGetDirectoryObject(Volume,
                                        DirectoryFileId,
                                        &DirectoryObject);

        if (!KSUCCESS(Status)) {
            IoCompleteIrp(FatDriver, Irp, Status);
            break;
        }

        //
        // Acquire the directory lock, as create will issue writes.
        //

        KeAcquireQueuedLock(DirectoryObject->Lock);

        //
        // The system should have prevented any create requests when the
        // directory hard link count is zero.
        //

        ASSERT(Create->DirectoryProperties->HardLinkCount != 0);

        Status = FatCreate(Volume->VolumeToken,
                           DirectoryFileId,
                           Create->Name,
                           Create->NameSize,
                           &(Create->DirectorySize),
                           &(Create->FileProperties));

        KeReleaseQueuedLock(DirectoryObject->Lock);
        FatpDirectoryObjectReleaseReference(Volume, DirectoryObject);
        IoCompleteIrp(FatDriver, Irp, Status);
        break;

    //
    // Delete all the file blocks and let the system reclaim the file ID.
    //

    case IrpMinorSystemControlDelete:
        FileOperation = (PSYSTEM_CONTROL_FILE_OPERATION)Context;

        ASSERT(FileOperation->FileProperties->HardLinkCount == 0);
        ASSERT(FileOperation->FileProperties->FileId !=
               Volume->RootDirectoryCluster);

        Status = FatDeleteFileBlocks(Volume->VolumeToken,
                                     NULL,
                                     FileOperation->FileProperties->FileId,
                                     0,
                                     FALSE);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Fat: failed to delete file blocks for file "
                          "%I64d on volume 0x%08x. Status: %d\n",
                          FileOperation->FileProperties->FileId,
                          Volume->VolumeToken,
                          Status);
        }

        IoCompleteIrp(FatDriver, Irp, Status);
        break;

    //
    // Write out the file properties. Properties can't be written out on the
    // root directory, but just pretend it went well.
    //

    case IrpMinorSystemControlWriteFileProperties:
        FileOperation = (PSYSTEM_CONTROL_FILE_OPERATION)Context;
        FileId = FileOperation->FileProperties->FileId;
        if (FileId == Volume->RootDirectoryCluster) {
            Status = STATUS_SUCCESS;

        } else {

            ASSERT(FileOperation->FileProperties->HardLinkCount != 0);

            Status = FatpGetParentDirectoryObject(Volume,
                                                  FileId,
                                                  &DirectoryObject);

            if (!KSUCCESS(Status)) {
                IoCompleteIrp(FatDriver, Irp, Status);
                break;
            }

            //
            // FAT stores file properties in the parent directory, acquire the
            // parent directory's lock to synchronize with other reads and
            // writes.
            //

            KeAcquireQueuedLock(DirectoryObject->Lock);
            Status = FatWriteFileProperties(Volume->VolumeToken,
                                            FileOperation->FileProperties,
                                            FileOperation->Flags);

            KeReleaseQueuedLock(DirectoryObject->Lock);
            FatpDirectoryObjectReleaseReference(Volume, DirectoryObject);
        }

        IoCompleteIrp(FatDriver, Irp, Status);
        break;

    //
    // Delete the given file or empty directory.
    //

    case IrpMinorSystemControlUnlink:
        Unlink = (PSYSTEM_CONTROL_UNLINK)Context;
        DirectoryFileId = Unlink->DirectoryProperties->FileId;
        FileId = Unlink->FileProperties->FileId;
        Status = FatpGetDirectoryObject(Volume,
                                        DirectoryFileId,
                                        &DirectoryObject);

        if (!KSUCCESS(Status)) {
            IoCompleteIrp(FatDriver, Irp, Status);
            break;
        }

        ASSERT(FileId != Volume->RootDirectoryCluster);
        ASSERT(FileId != DirectoryFileId);

        //
        // On fat the file being unlinked should only have a hard link count of
        // one.
        //

        ASSERT(Unlink->FileProperties->HardLinkCount == 1);

        //
        // Acquire the directory lock, as it will be written to in order to
        // unlink the file. If the file being unlinked is a sub-directory, its
        // lock does not need to be held. The unlink routine will do a read
        // from the sub-directory to determine if it is empty, but the system's
        // synchronization is sufficient. The worst case is that it is not
        // empty and the read races with a write properties call. The
        // sub-directory will still be determined to be non-empty, regardless
        // of the data written to the file properties.
        //

        KeAcquireQueuedLock(DirectoryObject->Lock);
        Status = FatUnlink(Volume->VolumeToken,
                           DirectoryFileId,
                           Unlink->Name,
                           Unlink->NameSize,
                           FileId,
                           &(Unlink->Unlinked));

        KeReleaseQueuedLock(DirectoryObject->Lock);
        FatpDirectoryObjectReleaseReference(Volume, DirectoryObject);
        IoCompleteIrp(FatDriver, Irp, Status);
        break;

    //
    // Rename a file or directory.
    //

    case IrpMinorSystemControlRename:
        Rename = (PSYSTEM_CONTROL_RENAME)Context;
        Status = FatpRenameFile(Volume,
                                Rename->SourceDirectoryProperties,
                                Rename->SourceFileProperties,
                                Rename->DestinationDirectoryProperties,
                                Rename->DestinationFileProperties,
                                &(Rename->SourceFileHardLinkDelta),
                                &(Rename->DestinationFileUnlinked),
                                &(Rename->DestinationDirectorySize),
                                Rename->Name,
                                Rename->NameSize);

        IoCompleteIrp(FatDriver, Irp, Status);
        break;

    //
    // Truncate the file. The system shouldn't pass directories down for
    // truncation.
    //

    case IrpMinorSystemControlTruncate:
        Truncate = (PSYSTEM_CONTROL_TRUNCATE)Context;

        ASSERT(Truncate->FileProperties->Type == IoObjectRegularFile);
        ASSERT(Truncate->DeviceContext != NULL);

        File = (PFATFS_FILE)(Truncate->DeviceContext);
        Status = FatpTruncateFile(Volume,
                                  Truncate->FileProperties,
                                  Truncate->NewSize,
                                  File->FileToken);

        IoCompleteIrp(FatDriver, Irp, Status);
        break;

    //
    // Get the array of block offsets and lengths for the given file.
    //

    case IrpMinorSystemControlGetBlockInformation:
        BlockInformation = (PSYSTEM_CONTROL_GET_BLOCK_INFORMATION)Context;
        FileId = BlockInformation->FileProperties->FileId;
        FileBlockInformation = &(BlockInformation->FileBlockInformation);
        Status = FatGetFileBlockInformation(Volume->VolumeToken,
                                            FileId,
                                            FileBlockInformation);

        IoCompleteIrp(FatDriver, Irp, Status);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:
        break;
    }

    return;
}

VOID
FatpRemoveDevice (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine removes a FAT FS volume from the system.

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

    PFATFS_VOLUME Volume;

    Volume = (PFATFS_VOLUME)DeviceContext;

    ASSERT(Volume != NULL);
    ASSERT(Volume->Attached != FALSE);

    //
    // Mark the volume as detached and release the original reference taken
    // when the volume was added. This may not be the last reference on the
    // volume as the system may be holding on to a path. If so, then the last
    // reference on the volume will be released when the system closes the
    // volume's root path.
    //

    Volume->Attached = FALSE;
    FatpVolumeReleaseReference(Volume);
    return;
}

VOID
FatpDestroyVolume (
    PFATFS_VOLUME Volume
    )

/*++

Routine Description:

    This routine destroys the given volume.

Arguments:

    Volume - Supplies a pointer to the volume that is to be destroyed.

Return Value:

    None.

--*/

{

    BLOCK_DEVICE_PARAMETERS BlockDeviceParameters;
    KSTATUS Status;

    ASSERT(Volume != NULL);
    ASSERT(Volume->VolumeToken != NULL);
    ASSERT(Volume->MediaHandle != INVALID_HANDLE);
    ASSERT(Volume->Attached == FALSE);

    //
    // Get the block device information before unmounting the volume.
    //

    FatGetDeviceInformation(Volume->VolumeToken, &BlockDeviceParameters);

    //
    // Unmount the volume, destroying the volume token.
    //

    Status = FatUnmount(Volume->VolumeToken);

    ASSERT(KSUCCESS(Status));

    //
    // Close the media device handle.
    //

    IoClose(Volume->MediaHandle);

    //
    // Close the fat device.
    //

    FatCloseDevice(BlockDeviceParameters.DeviceToken);

    //
    // Destroy the volume's resources.
    //

    MmFreeNonPagedPool(Volume);
    return;
}

VOID
FatpVolumeAddReference (
    PFATFS_VOLUME Volume
    )

/*++

Routine Description:

    This routine increments the reference count on the given volume.

Arguments:

    Volume - Supplies a pointer to the volume whose reference count should be
        incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Volume->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x30000000));

    return;
}

VOID
FatpVolumeReleaseReference (
    PFATFS_VOLUME Volume
    )

/*++

Routine Description:

    This routine decrements the reference count on the given volume, and
    destroys it if it hits zero.

Arguments:

    Volume - Supplies a pointer to the volume whose reference count should be
        decremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Volume->ReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x30000000));

    if (OldReferenceCount == 1) {
        FatpDestroyVolume(Volume);
    }

    return;
}

KSTATUS
FatpTruncateFile (
    PFATFS_VOLUME Volume,
    PFILE_PROPERTIES FileProperties,
    ULONGLONG NewSize,
    PVOID FileToken
    )

/*++

Routine Description:

    This routine truncates a file to the current file size.

Arguments:

    Volume - Supplies a pointer to the volume.

    FileProperties - Supplies a pointer to the properties of the file being
        destroyed.

    NewSize - Supplies the new file size to set.

    FileToken - Supplies an optional open file token.

Return Value:

    None.

--*/

{

    ULONGLONG OldSize;
    KSTATUS Status;

    OldSize = FileProperties->Size;
    Status = FatTruncate(Volume->VolumeToken,
                         FileToken,
                         FileProperties->FileId,
                         OldSize,
                         NewSize);

    if (KSUCCESS(Status)) {
        FileProperties->Size = NewSize;
    }

    return Status;
}

KSTATUS
FatpRenameFile (
    PFATFS_VOLUME Volume,
    PFILE_PROPERTIES SourceDirectory,
    PFILE_PROPERTIES SourceFile,
    PFILE_PROPERTIES DestinationDirectory,
    PFILE_PROPERTIES DestinationFile,
    PULONG SourceFileHardLinkDelta,
    PBOOL DestinationFileUnlinked,
    PULONGLONG DestinationDirectorySize,
    PSTR Name,
    ULONG NameSize
    )

/*++

Routine Description:

    This routine truncates a file to zero size.

Arguments:

    Volume - Supplies a pointer to the volume.

    SourceDirectory - Supplies a pointer to the properties of the directory
        containing the file to rename.

    SourceFile - Supplies a pointer to the properties of the file to rename.

    DestinationDirectory - Supplies a pointer to the properties of the
        directory where the named file will reside.

    DestinationFile - Supplies an optional pointer to the properties of the
        file or directory currently sitting at the destination (that will need
        to be unlinked). If there is no file or directory at the destination,
        then this will be NULL.

    SourceFileHardLinkDelta - Supplies a pointer that receives a value
        indicating the delta in hard links caused by this rename operation. It
        should be -1 or 0.

    DestinationFileUnlinked - Supplies a pointer to a boolean that receives
        TRUE if the destination file was successfully unlinked (if necessary),
        or FALE otherwise.

    DestinationDirectorySize - Supplies a pointer where the new size of the
        destination directory will be returned.

    Name - Supplies a pointer to the string containing the destination
        file/directory name, which may not be null terminated.

    NameSize - Supplies the size of the name buffer including space for a null
        terminator (which may be a null terminator or may be a garbage
        character).

Return Value:

    Status code.

--*/

{

    BOOL DestinationCreated;
    PFATFS_DIRECTORY_OBJECT DestinationDirectoryObject;
    BOOL LocksHeld;
    PFATFS_DIRECTORY_OBJECT SourceDirectoryObject;
    BOOL SourceErased;
    KSTATUS Status;

    //
    // The system should have handled the case of renaming to the same file.
    //

    ASSERT(SourceFile != DestinationFile);
    ASSERT(*DestinationFileUnlinked == FALSE);

    DestinationDirectoryObject = NULL;
    LocksHeld = FALSE;
    SourceDirectoryObject = NULL;
    *SourceFileHardLinkDelta = 0;

    //
    // Lock the source and destination directories in the appropriate order.
    //

    Status = FatpGetDirectoryObject(Volume,
                                    SourceDirectory->FileId,
                                    &SourceDirectoryObject);

    if (!KSUCCESS(Status)) {
        goto RenameFileEnd;
    }

    Status = FatpGetDirectoryObject(Volume,
                                    DestinationDirectory->FileId,
                                    &DestinationDirectoryObject);

    if (!KSUCCESS(Status)) {
        goto RenameFileEnd;
    }

    if (SourceDirectoryObject->Cluster < DestinationDirectoryObject->Cluster) {
        KeAcquireQueuedLock(SourceDirectoryObject->Lock);
        KeAcquireQueuedLock(DestinationDirectoryObject->Lock);

    } else if (SourceDirectoryObject->Cluster >
               DestinationDirectoryObject->Cluster) {

        KeAcquireQueuedLock(DestinationDirectoryObject->Lock);
        KeAcquireQueuedLock(SourceDirectoryObject->Lock);

    } else {

        ASSERT(SourceDirectory == DestinationDirectory);
        ASSERT(SourceDirectoryObject == DestinationDirectoryObject);

        KeAcquireQueuedLock(SourceDirectoryObject->Lock);
    }

    LocksHeld = TRUE;

    //
    // The system should not have allowed a renames into a directory that has
    // been unlinked.
    //

    ASSERT(DestinationDirectory->HardLinkCount != 0);

    //
    // If the old file exists, unlink it. Just like the unlink system control
    // operation, this unlink does not need to acquire the directory lock if
    // the destination file is a sub-directory. The system's synchronization is
    // sufficient. The worse case is that this unlink's read of the
    // sub-directory races with a file properties write to the sub-directory.
    // Such a write cannot change whether or not the directory is empty, which
    // is what unlink tries to determine.
    //

    if (DestinationFile != NULL) {

        ASSERT(DestinationFile->HardLinkCount == 1);

        Status = FatUnlink(Volume->VolumeToken,
                           DestinationDirectory->FileId,
                           Name,
                           NameSize,
                           DestinationFile->FileId,
                           DestinationFileUnlinked);

        if (!KSUCCESS(Status)) {
            goto RenameFileEnd;
        }
    }

    //
    // Perform the rename operation.
    //

    Status = FatRename(Volume->VolumeToken,
                       SourceDirectory->FileId,
                       SourceFile->FileId,
                       &SourceErased,
                       DestinationDirectory->FileId,
                       &DestinationCreated,
                       DestinationDirectorySize,
                       Name,
                       NameSize);

    if (SourceErased != DestinationCreated) {

        ASSERT((SourceErased != FALSE) && (DestinationCreated == FALSE));

        *SourceFileHardLinkDelta = (ULONG)-1;
    }

    if (!KSUCCESS(Status)) {
        goto RenameFileEnd;
    }

    Status = STATUS_SUCCESS;

RenameFileEnd:

    //
    // Unlock the directories in the reverse order.
    //

    if (LocksHeld != FALSE) {
        if (SourceDirectoryObject->Cluster <
            DestinationDirectoryObject->Cluster) {

            KeReleaseQueuedLock(DestinationDirectoryObject->Lock);
            KeReleaseQueuedLock(SourceDirectoryObject->Lock);

        } else if (SourceDirectoryObject->Cluster >
                   DestinationDirectoryObject->Cluster) {

            KeReleaseQueuedLock(SourceDirectoryObject->Lock);
            KeReleaseQueuedLock(DestinationDirectoryObject->Lock);

        } else {

            ASSERT(SourceDirectoryObject == DestinationDirectoryObject);

            KeReleaseQueuedLock(SourceDirectoryObject->Lock);
        }
    }

    if (SourceDirectoryObject != NULL) {
        FatpDirectoryObjectReleaseReference(Volume, SourceDirectoryObject);
    }

    if (DestinationDirectoryObject != NULL) {
        FatpDirectoryObjectReleaseReference(Volume, DestinationDirectoryObject);
    }

    return Status;
}

PFATFS_DIRECTORY_OBJECT
FatpCreateDirectoryObject (
    PFATFS_VOLUME Volume,
    FILE_ID DirectoryFileId
    )

/*++

Routine Description:

    This routine handles requests to create a new directory objects.

Arguments:

    Volume - Supplies a pointer to the FAT FS volume structure.

    DirectoryFileId - Supplies the file ID of the directory.

Return Value:

    Returns a pointer to a directory object on success, or NULL on failure.

--*/

{

    PFATFS_DIRECTORY_OBJECT DirectoryObject;
    KSTATUS Status;

    DirectoryObject = MmAllocatePagedPool(sizeof(FATFS_DIRECTORY_OBJECT),
                                          FAT_DIRECTORY_ALLOCATION_TAG);

    if (DirectoryObject == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateFileObjectEnd;
    }

    RtlZeroMemory(DirectoryObject, sizeof(FATFS_DIRECTORY_OBJECT));
    DirectoryObject->Cluster = (ULONG)DirectoryFileId;

    ASSERT(DirectoryObject->Cluster == DirectoryFileId);

    DirectoryObject->Lock = KeCreateQueuedLock();
    if (DirectoryObject->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateFileObjectEnd;
    }

    DirectoryObject->ReferenceCount = 1;
    Status = STATUS_SUCCESS;

CreateFileObjectEnd:
    if (!KSUCCESS(Status)) {
        if (DirectoryObject != NULL) {
            FatpDestroyDirectoryObject(DirectoryObject);
            DirectoryObject = NULL;
        }
    }

    return DirectoryObject;
}

VOID
FatpDestroyDirectoryObject (
    PFATFS_DIRECTORY_OBJECT DirectoryObject
    )

/*++

Routine Description:

    This routine handles requests to destroy a file object.

Arguments:

    DirectoryObject - Supplies a pointer to a directory object.

Return Value:

    None.

--*/

{

    if (DirectoryObject->Lock != NULL) {
        KeDestroyQueuedLock(DirectoryObject->Lock);
    }

    MmFreePagedPool(DirectoryObject);
    return;
}

VOID
FatpDirectoryObjectAddReference (
    PFATFS_VOLUME Volume,
    PFATFS_DIRECTORY_OBJECT DirectoryObject
    )

/*++

Routine Description:

    This routine adds a reference to the directory file object for the given
    volume.

Arguments:

    Volume - Supplies a pointer to the FAT FS volume structure.

    DirectoryObject - Supplies a pointer to a directory object.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(DirectoryObject->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
FatpDirectoryObjectReleaseReference (
    PFATFS_VOLUME Volume,
    PFATFS_DIRECTORY_OBJECT DirectoryObject
    )

/*++

Routine Description:

    This routine adds a reference to the directory file object for the given
    volume.

Arguments:

    Volume - Supplies a pointer to the FAT FS volume structure.

    DirectoryObject - Supplies a pointer to a directory object.

Return Value:

    None.

--*/

{

    BOOL LockHeld;
    ULONG OldReferenceCount;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Acquire the directory tree lock in case this is the last reference to
    // prevent any races with the create and lookup operation.
    //

    KeAcquireSpinLock(&(Volume->DirectoryTreeLock));
    LockHeld = TRUE;
    OldReferenceCount = RtlAtomicAdd32(&(DirectoryObject->ReferenceCount),
                                       (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {

        //
        // Remove this file object from the tree.
        //

        RtlRedBlackTreeRemove(&(Volume->DirectoryTree),
                              &(DirectoryObject->TreeNode));

        KeReleaseSpinLock(&(Volume->DirectoryTreeLock));
        LockHeld = FALSE;

        //
        // Destroy it with the lock released.
        //

        FatpDestroyDirectoryObject(DirectoryObject);
    }

    if (LockHeld != FALSE) {
        KeReleaseSpinLock(&(Volume->DirectoryTreeLock));
    }

    return;
}

KSTATUS
FatpGetDirectoryObject (
    PFATFS_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PFATFS_DIRECTORY_OBJECT *DirectoryObject
    )

/*++

Routine Description:

    This routine returns the directory object for the given directory,
    specified by its file ID.

Arguments:

    Volume - Supplies a pointer to the FAT FS volume structure.

    DirectoryFileId - Supplies the ID of the directory whose FAT FS object
        should be looked up.

    DirectoryObject - Supplies a pointer that receives a pointer to the
        directory object.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = FatpCreateOrLookupDirectoryObject(Volume,
                                               DirectoryFileId,
                                               DirectoryObject);

    return Status;
}

KSTATUS
FatpGetParentDirectoryObject (
    PFATFS_VOLUME Volume,
    FILE_ID FileId,
    PFATFS_DIRECTORY_OBJECT *ParentDirectoryObject
    )

/*++

Routine Description:

    This routine returns the parent directory object for the given file,
    specified by its file ID.

Arguments:

    Volume - Supplies a pointer to the FAT FS volume structure.

    FileId - Supplies the ID of the file whose parent directory's directory
        object should be looked up.

    ParentDirectoryObject - Supplies a pointer that receives a pointer to the
        parent directory object.

Return Value:

    Status code.

--*/

{

    FILE_ID DirectoryFileId;
    KSTATUS Status;

    //
    // Get the cluster number of the file's directory.
    //

    Status = FatGetFileDirectory(Volume->VolumeToken, FileId, &DirectoryFileId);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Find the FAT FS directory object structure for the directory cluster.
    //

    Status = FatpCreateOrLookupDirectoryObject(Volume,
                                               DirectoryFileId,
                                               ParentDirectoryObject);

    return Status;
}

KSTATUS
FatpCreateOrLookupDirectoryObject (
    PFATFS_VOLUME Volume,
    FILE_ID DirectoryFileId,
    PFATFS_DIRECTORY_OBJECT *DirectoryObject
    )

/*++

Routine Description:

    This routine creates a new directory object, inserts it into the tree and
    returns it with a reference, or it finds an existing object for the given
    directory and takes a reference on it.

Arguments:

    Volume - Supplies a pointer to the FAT FS volume structure.

    DirectoryFileId - Supplies the ID of the directory file whose object is to
        be looked up.

    DirectoryObject - Supplies a pointer that receives a pointer to the
        directory object.

Return Value:

    Status code.

--*/

{

    PRED_BLACK_TREE_NODE FoundNode;
    PFATFS_DIRECTORY_OBJECT FoundObject;
    PFATFS_DIRECTORY_OBJECT NewObject;
    FATFS_DIRECTORY_OBJECT SearchObject;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // First see if an object already exists for this directory.
    //

    FoundObject = NULL;
    SearchObject.Cluster = (ULONG)DirectoryFileId;
    KeAcquireSpinLock(&(Volume->DirectoryTreeLock));
    FoundNode = RtlRedBlackTreeSearch(&(Volume->DirectoryTree),
                                      &(SearchObject.TreeNode));

    if (FoundNode != NULL) {
        FoundObject = RED_BLACK_TREE_VALUE(FoundNode,
                                           FATFS_DIRECTORY_OBJECT,
                                           TreeNode);

        FatpDirectoryObjectAddReference(Volume, FoundObject);
    }

    KeReleaseSpinLock(&(Volume->DirectoryTreeLock));

    //
    // If an object was found, just exit.
    //

    if (FoundObject != NULL) {
        Status = STATUS_SUCCESS;
        goto CreateOrLookupDirectoryObject;
    }

    //
    // Otherwise, create a directory object and try to insert it into the
    // tree.
    //

    NewObject = FatpCreateDirectoryObject(Volume, DirectoryFileId);
    if (NewObject == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateOrLookupDirectoryObject;
    }

    //
    // After acquiring the lock, do another lookup to see if another directory
    // lookup created a object for this directory.
    //

    KeAcquireSpinLock(&(Volume->DirectoryTreeLock));
    FoundNode = RtlRedBlackTreeSearch(&(Volume->DirectoryTree),
                                      &(SearchObject.TreeNode));

    if (FoundNode != NULL) {
        FoundObject = RED_BLACK_TREE_VALUE(FoundNode,
                                           FATFS_DIRECTORY_OBJECT,
                                           TreeNode);

        FatpDirectoryObjectAddReference(Volume, FoundObject);

    } else {
        RtlRedBlackTreeInsert(&(Volume->DirectoryTree),
                              &(NewObject->TreeNode));
    }

    KeReleaseSpinLock(&(Volume->DirectoryTreeLock));

    //
    // If an existing node was found, use that directory object.
    //

    if (FoundObject != NULL) {
        FatpDestroyDirectoryObject(NewObject);

    //
    // Otherwise the new object is in action, set it as found.
    //

    } else {
        FoundObject = NewObject;
    }

    Status = STATUS_SUCCESS;

CreateOrLookupDirectoryObject:
    *DirectoryObject = FoundObject;
    return Status;
}

COMPARISON_RESULT
FatpCompareDirectoryObjectNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares FAT directory object nodes by their cluster numbers.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PFATFS_DIRECTORY_OBJECT First;
    PFATFS_DIRECTORY_OBJECT Second;

    First = RED_BLACK_TREE_VALUE(FirstNode, FATFS_DIRECTORY_OBJECT, TreeNode);
    Second = RED_BLACK_TREE_VALUE(SecondNode, FATFS_DIRECTORY_OBJECT, TreeNode);
    if (First->Cluster > Second->Cluster) {
        return ComparisonResultDescending;
    }

    if (First->Cluster < Second->Cluster) {
        return ComparisonResultAscending;
    }

    return ComparisonResultSame;
}

