/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    file.c

Abstract:

    This module implements file system support for the boot library.

Author:

    Evan Green 19-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/fat/fat.h>
#include "firmware.h"
#include "bootlibp.h"

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
BopOpenVolume (
    HANDLE DiskHandle,
    PBOOT_VOLUME *VolumeHandle
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BoOpenBootVolume (
    ULONG BootDriveNumber,
    ULONGLONG PartitionOffset,
    PBOOT_ENTRY BootEntry,
    PBOOT_VOLUME *VolumeHandle
    )

/*++

Routine Description:

    This routine opens a handle to the boot volume device, which is the device
    this boot application was loaded from.

Arguments:

    BootDriveNumber - Supplies the drive number of the boot device, for PC/AT
        systems.

    PartitionOffset - Supplies the offset in sectors to the start of the boot
        partition, for PC/AT systems.

    BootEntry - Supplies an optional pointer to the boot entry, for EFI systems.

    VolumeHandle - Supplies a pointer where a handle to the open volume will
        be returned on success.

Return Value:

    Status code.

--*/

{

    HANDLE DiskHandle;
    KSTATUS Status;

    *VolumeHandle = NULL;
    DiskHandle = NULL;
    Status = FwOpenBootDisk(BootDriveNumber,
                            PartitionOffset,
                            BootEntry,
                            &DiskHandle);

    if (!KSUCCESS(Status)) {
        goto OpenBootVolumeEnd;
    }

    Status = BopOpenVolume(DiskHandle, VolumeHandle);
    if (!KSUCCESS(Status)) {
        goto OpenBootVolumeEnd;
    }

OpenBootVolumeEnd:
    if (!KSUCCESS(Status)) {
        if (DiskHandle != NULL) {
            FwCloseDisk(DiskHandle);
        }
    }

    return Status;
}

KSTATUS
BoCloseVolume (
    PBOOT_VOLUME VolumeHandle
    )

/*++

Routine Description:

    This routine closes a disk handle.

Arguments:

    VolumeHandle - Supplies the volume handle returned when the volume was
        opened.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = FatUnmount(VolumeHandle->FileSystemHandle);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    FwCloseDisk(VolumeHandle->DiskHandle);
    BoFreeMemory(VolumeHandle);
    return STATUS_SUCCESS;
}

KSTATUS
BoOpenVolume (
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE],
    PBOOT_VOLUME *Volume
    )

/*++

Routine Description:

    This routine closes a disk handle.

Arguments:

    PartitionId - Supplies the ID of the partition to open.

    Volume - Supplies a pointer where a handle to the open volume will
        be returned on success.

Return Value:

    Status code.

--*/

{

    HANDLE DiskHandle;
    KSTATUS Status;

    *Volume = NULL;
    DiskHandle = NULL;
    Status = FwOpenPartition(PartitionId, &DiskHandle);
    if (!KSUCCESS(Status)) {
        goto OpenVolumeEnd;
    }

    Status = BopOpenVolume(DiskHandle, Volume);
    if (!KSUCCESS(Status)) {
        goto OpenVolumeEnd;
    }

OpenVolumeEnd:
    if (!KSUCCESS(Status)) {
        if (DiskHandle != NULL) {
            FwCloseDisk(DiskHandle);
        }
    }

    return Status;
}

KSTATUS
BoLookupPath (
    PBOOT_VOLUME Volume,
    PFILE_ID StartingDirectory,
    PCSTR Path,
    PFILE_PROPERTIES FileProperties
    )

/*++

Routine Description:

    This routine attempts to look up the given file path.

Arguments:

    Volume - Supplies a pointer to the volume token.

    StartingDirectory - Supplies an optional pointer to a file ID containing
        the directory to start path traversal from. If NULL, path lookup will
        start with the root of the volume.

    Path - Supplies a pointer to the path string to look up.

    FileProperties - Supplies a pointer where the properties for the file will
        be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PATH_NOT_FOUND if the given file path does not exist.

    Other error codes on other failures.

--*/

{

    ULONG ComponentSize;
    PSTR CurrentComponent;
    FILE_ID DirectoryId;
    PSTR NextComponent;
    PSTR PathCopy;
    ULONG PathLength;
    KSTATUS Status;

    PathCopy = NULL;
    PathLength = RtlStringLength(Path);
    if (PathLength == 0) {
        Status = STATUS_INVALID_PARAMETER;
        goto LookupPathEnd;
    }

    PathLength += 1;
    PathCopy = BoAllocateMemory(PathLength);
    if (PathCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LookupPathEnd;
    }

    RtlCopyMemory(PathCopy, Path, PathLength);

    //
    // Start with the root directory.
    //

    if (StartingDirectory != NULL) {
        DirectoryId = *StartingDirectory;
        RtlZeroMemory(FileProperties, sizeof(FILE_PROPERTIES));

    } else {
        Status = FatLookup(Volume->FileSystemHandle,
                           TRUE,
                           0,
                           NULL,
                           0,
                           FileProperties);

        if (!KSUCCESS(Status)) {
            goto LookupPathEnd;
        }

        DirectoryId = FileProperties->FileId;
    }

    //
    // Remove any leading slashes.
    //

    CurrentComponent = PathCopy;
    while (*CurrentComponent == '/') {
        CurrentComponent += 1;
        PathLength -= 1;
    }

    //
    // Loop looking up directory entries.
    //

    while (TRUE) {
        NextComponent = RtlStringFindCharacter(CurrentComponent,
                                               '/',
                                               PathLength);

        if (NextComponent != NULL) {
            *NextComponent = '\0';
            NextComponent += 1;
            ComponentSize = (UINTN)NextComponent - (UINTN)CurrentComponent;

        } else {
            ComponentSize = PathLength;
        }

        if (ComponentSize > 1) {
            Status = FatLookup(Volume->FileSystemHandle,
                               FALSE,
                               DirectoryId,
                               CurrentComponent,
                               ComponentSize,
                               FileProperties);

            if (!KSUCCESS(Status)) {
                goto LookupPathEnd;
            }
        }

        if (NextComponent == NULL) {
            break;
        }

        DirectoryId = FileProperties->FileId;
        PathLength -= ComponentSize;
        CurrentComponent = NextComponent;
    }

LookupPathEnd:
    if (PathCopy != NULL) {
        BoFreeMemory(PathCopy);
    }

    return Status;
}

KSTATUS
BoLoadFile (
    PBOOT_VOLUME Volume,
    PFILE_ID Directory,
    PSTR FileName,
    PVOID *FilePhysical,
    PUINTN FileSize,
    PULONGLONG ModificationDate
    )

/*++

Routine Description:

    This routine loads a file from disk into memory.

Arguments:

    Volume - Supplies a pointer to the mounted volume to read the file from.

    Directory - Supplies an optional pointer to the ID of the directory to
        start path traversal from. If NULL, the root of the volume will be used.

    FileName - Supplies the name of the file to load.

    FilePhysical - Supplies a pointer where the file buffer's physical address
        will be returned. This routine will allocate the buffer to hold the
        file. If this parameter is NULL, the status code will reflect whether
        or not the file could be opened, but the file contents will not be
        loaded.

    FileSize - Supplies a pointer where the size of the file in bytes will be
        returned.

    ModificationDate - Supplies an optional pointer where the modification
        date of the file will be returned on success.

Return Value:

    Status code.

--*/

{

    UINTN AlignedSize;
    UINTN BytesRead;
    FAT_SEEK_INFORMATION FatSeekInformation;
    PVOID File;
    FILE_PROPERTIES FileProperties;
    PFAT_IO_BUFFER IoBuffer;
    ULONGLONG LocalFileSize;
    ULONG PageSize;
    PVOID PhysicalBuffer;
    KSTATUS Status;

    File = NULL;
    IoBuffer = NULL;
    LocalFileSize = 0;
    PhysicalBuffer = NULL;
    PageSize = MmPageSize();
    Status = BoLookupPath(Volume, Directory, FileName, &FileProperties);
    if (!KSUCCESS(Status)) {
        goto LoadFileEnd;
    }

    Status = FatOpenFileId(Volume->FileSystemHandle,
                           FileProperties.FileId,
                           IO_ACCESS_READ,
                           0,
                           &File);

    if (!KSUCCESS(Status)) {
        goto LoadFileEnd;
    }

    LocalFileSize = FileProperties.Size;

    ASSERT((UINTN)LocalFileSize == LocalFileSize);

    //
    // If the caller doesn't actually want the data, the work is done.
    //

    if (FilePhysical == NULL) {
        Status = STATUS_SUCCESS;
        goto LoadFileEnd;
    }

    //
    // Round the file size up to the nearest page.
    //

    AlignedSize = ALIGN_RANGE_UP(LocalFileSize + 1, PageSize);
    PhysicalBuffer = BoAllocateMemory(AlignedSize);
    if (PhysicalBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadFileEnd;
    }

    IoBuffer = FatCreateIoBuffer(PhysicalBuffer, AlignedSize);
    if (IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto LoadFileEnd;
    }

    RtlZeroMemory(&FatSeekInformation, sizeof(FAT_SEEK_INFORMATION));
    Status = FatReadFile(File,
                         &FatSeekInformation,
                         IoBuffer,
                         (UINTN)LocalFileSize,
                         0,
                         NULL,
                         &BytesRead);

    if (!KSUCCESS(Status)) {
        goto LoadFileEnd;
    }

    if (BytesRead != LocalFileSize) {
        Status = STATUS_FILE_CORRUPT;
        goto LoadFileEnd;
    }

    //
    // NULL terminate the file just in case someone tries to read off the end
    // of it.
    //

    *((PUCHAR)PhysicalBuffer + BytesRead) = '\0';

LoadFileEnd:
    if (FilePhysical != NULL) {
        *FilePhysical = PhysicalBuffer;
    }

    if (FileSize != NULL) {
        *FileSize = LocalFileSize;
    }

    if (ModificationDate != NULL) {
        *ModificationDate = FileProperties.ModifiedTime.Seconds;
    }

    if (IoBuffer != NULL) {
        FatFreeIoBuffer(IoBuffer);
    }

    if (File != NULL) {
        FatCloseFile(File);
    }

    return Status;
}

KSTATUS
BoStoreFile (
    PBOOT_VOLUME Volume,
    FILE_ID Directory,
    PSTR FileName,
    ULONG FileNameLength,
    PVOID FilePhysical,
    UINTN FileSize,
    ULONGLONG ModificationDate
    )

/*++

Routine Description:

    This routine stores a file buffer to disk.

Arguments:

    Volume - Supplies a pointer to the mounted volume to read the file from.

    Directory - Supplies the file ID of the directory the file resides in.

    FileName - Supplies the name of the file to store.

    FileNameLength - Supplies the length of the file name buffer in bytes,
        including the null terminator.

    FilePhysical - Supplies a pointer to the buffer containing the file
        contents.

    FileSize - Supplies the size of the file buffer in bytes. The file will be
        truncated to this size if it previously existed and was larger.

    ModificationDate - Supplies the modification date to set.

Return Value:

    Status code.

--*/

{

    UINTN BytesWritten;
    ULONG DesiredAccess;
    FILE_PROPERTIES DirectoryProperties;
    ULONGLONG DirectorySize;
    FAT_SEEK_INFORMATION FatSeekInformation;
    PVOID File;
    FILE_PROPERTIES FileProperties;
    PFAT_IO_BUFFER IoBuffer;
    ULONGLONG NewDirectorySize;
    KSTATUS Status;

    File = NULL;
    IoBuffer = NULL;
    RtlZeroMemory(&FileProperties, sizeof(FILE_PROPERTIES));
    FileProperties.Size = 0;

    ASSERT(Directory != 0);

    //
    // Load the file into memory.
    //

    DesiredAccess = IO_ACCESS_WRITE;
    Status = FatLookup(Volume->FileSystemHandle,
                       FALSE,
                       Directory,
                       FileName,
                       FileNameLength,
                       &FileProperties);

    if (KSUCCESS(Status)) {
        Status = FatDeleteFileBlocks(Volume->FileSystemHandle,
                                     NULL,
                                     FileProperties.FileId,
                                     0,
                                     TRUE);

        if (!KSUCCESS(Status)) {
            goto StoreFileEnd;
        }

    //
    // The file did not exist before. Create it.
    //

    } else if (Status == STATUS_PATH_NOT_FOUND) {

        //
        // Look up the directory.
        //

        RtlZeroMemory(&DirectoryProperties, sizeof(FILE_PROPERTIES));
        Status = FatLookup(Volume->FileSystemHandle,
                           FALSE,
                           Directory,
                           ".",
                           sizeof("."),
                           &DirectoryProperties);

        if (!KSUCCESS(Status)) {
            goto StoreFileEnd;
        }

        FileProperties.Type = IoObjectRegularFile;
        FileProperties.Permissions = FILE_PERMISSION_USER_READ |
                                     FILE_PERMISSION_USER_WRITE |
                                     FILE_PERMISSION_GROUP_READ |
                                     FILE_PERMISSION_GROUP_WRITE |
                                     FILE_PERMISSION_OTHER_READ;

        FatGetCurrentSystemTime(&(FileProperties.StatusChangeTime));
        Status = FatCreate(Volume->FileSystemHandle,
                           Directory,
                           FileName,
                           FileNameLength,
                           &NewDirectorySize,
                           &FileProperties);

        DirectorySize = DirectoryProperties.Size;
        if (NewDirectorySize > DirectorySize) {
            DirectoryProperties.Size = NewDirectorySize;
            Status = FatWriteFileProperties(Volume->FileSystemHandle,
                                            &DirectoryProperties,
                                            0);

            if (!KSUCCESS(Status)) {
                goto StoreFileEnd;
            }
        }

    //
    // Some other error occurred, bail out.
    //

    } else if (!KSUCCESS(Status)) {
        goto StoreFileEnd;
    }

    //
    // Open up the now-empty file.
    //

    Status = FatOpenFileId(Volume->FileSystemHandle,
                           FileProperties.FileId,
                           DesiredAccess,
                           0,
                           &File);

    if (!KSUCCESS(Status)) {
        goto StoreFileEnd;
    }

    //
    // Create an I/O buffer and write the data out.
    //

    IoBuffer = FatCreateIoBuffer(FilePhysical, FileSize);
    if (IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto StoreFileEnd;
    }

    RtlZeroMemory(&FatSeekInformation, sizeof(FAT_SEEK_INFORMATION));
    Status = FatWriteFile(File,
                          &FatSeekInformation,
                          IoBuffer,
                          FileSize,
                          0,
                          NULL,
                          &BytesWritten);

    if (!KSUCCESS(Status)) {
        goto StoreFileEnd;
    }

    if (BytesWritten != FileSize) {
        Status = STATUS_FILE_CORRUPT;
        goto StoreFileEnd;
    }

    //
    // Update the metadata.
    //

    FileProperties.Size = FileSize;
    FileProperties.ModifiedTime.Seconds = ModificationDate;
    FileProperties.AccessTime.Seconds = ModificationDate;
    Status = FatWriteFileProperties(Volume->FileSystemHandle,
                                    &FileProperties,
                                    0);

    if (!KSUCCESS(Status)) {
        goto StoreFileEnd;
    }

StoreFileEnd:
    if (IoBuffer != NULL) {
        FatFreeIoBuffer(IoBuffer);
    }

    if (File != NULL) {
        FatCloseFile(File);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BopOpenVolume (
    HANDLE DiskHandle,
    PBOOT_VOLUME *VolumeHandle
    )

/*++

Routine Description:

    This routine mounts a volume on an open disk handle and creates a volume
    handle representing that connection.

Arguments:

    DiskHandle - Supplies the open disk or partition handle from the firmware.

    PartitionOffset - Supplies the offset in sectors to the start of the
        partition.

    VolumeHandle - Supplies a pointer where a handle to the open volume will
        be returned on success.

Return Value:

    Status code.

--*/

{

    PBOOT_VOLUME BootVolume;
    KSTATUS Status;

    BootVolume = BoAllocateMemory(sizeof(BOOT_VOLUME));
    if (BootVolume == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OpenVolumeEnd;
    }

    RtlZeroMemory(BootVolume, sizeof(BOOT_VOLUME));
    BootVolume->DiskHandle = DiskHandle;

    //
    // Attempt to mount the device.
    //

    BootVolume->Parameters.DeviceToken = BootVolume;
    BootVolume->Parameters.BlockSize = FwGetDiskSectorSize(DiskHandle);
    BootVolume->Parameters.BlockCount = FwGetDiskSectorCount(DiskHandle);
    Status = FatMount(&(BootVolume->Parameters),
                      0,
                      &(BootVolume->FileSystemHandle));

    if (!KSUCCESS(Status)) {
        goto OpenVolumeEnd;
    }

OpenVolumeEnd:
    if (!KSUCCESS(Status)) {
        if (BootVolume != NULL) {

            ASSERT(BootVolume->FileSystemHandle == NULL);

            BoFreeMemory(BootVolume);
            BootVolume = NULL;
        }
    }

    *VolumeHandle = BootVolume;
    return Status;
}

