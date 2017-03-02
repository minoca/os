/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fileio.c

Abstract:

    This module implements support for file I/O in the setup program.

Author:

    Evan Green 11-Apr-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "setup.h"
#include <minoca/lib/fat/fat.h>

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_DIRECTORY_ENTRY_SIZE 300

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a file handle in the setup app.

Members:

    Volume - Stores a pointer to the volume this file belongs to.

    Handle - Stores the handle returned from the layer below.

    Properties - Stores the file properties.

    SeekInformation - Stores the seek information for the file.

    CurrentOffset - Stores the current file offset.

    FatFile - Stores a pointer to the file systems internal context for the
        open file.

    IsDirty - Stores a boolean indicating if the file is dirty or not.

--*/

typedef struct _SETUP_FILE {
    PSETUP_VOLUME Volume;
    PVOID Handle;
    FILE_PROPERTIES Properties;
    FAT_SEEK_INFORMATION SeekInformation;
    ULONGLONG CurrentOffset;
    PVOID FatFile;
    ULONGLONG DirectoryFileId;
    BOOL IsDirty;
} SETUP_FILE, *PSETUP_FILE;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupFatOpen (
    PSETUP_VOLUME Volume,
    PSETUP_FILE NewFile,
    PCSTR Path,
    INT Flags,
    INT CreatePermissions,
    BOOL Directory
    );

PSTR
SetupFatCopyPath (
    PCSTR InputPath
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PVOID
SetupVolumeOpen (
    PSETUP_CONTEXT Context,
    PSETUP_DESTINATION Destination,
    SETUP_VOLUME_FORMAT_CHOICE Format,
    BOOL CompatibilityMode
    )

/*++

Routine Description:

    This routine opens a handle to a given volume.

Arguments:

    Context - Supplies a pointer to the application context.

    Destination - Supplies a pointer to the destination to open.

    Format - Supplies the disposition for formatting the volume.

    CompatibilityMode - Supplies a boolean indicating whether to run the
        file system in the most compatible way possible.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

{

    BLOCK_DEVICE_PARAMETERS BlockParameters;
    ULONG MountFlags;
    INT Result;
    KSTATUS Status;
    PSETUP_VOLUME Volume;

    memset(&BlockParameters, 0, sizeof(BLOCK_DEVICE_PARAMETERS));
    Volume = malloc(sizeof(SETUP_VOLUME));
    if (Volume == NULL) {
        Result = ENOMEM;
        goto OpenVolumeEnd;
    }

    memset(Volume, 0, sizeof(SETUP_VOLUME));
    Volume->Context = Context;
    Volume->DestinationType = Destination->Type;

    //
    // If this is an image, disk, or partition, open the native interface.
    //

    if ((Destination->Type == SetupDestinationDisk) ||
        (Destination->Type == SetupDestinationPartition) ||
        (Destination->Type == SetupDestinationImage)) {

        if ((Destination->Type == SetupDestinationPartition) ||
            (Destination->Type == SetupDestinationDisk)) {

            Volume->BlockHandle = Context->Disk;

            assert((Context->Disk != NULL) &&
                   (Context->CurrentPartitionSize != 0));

        } else {
            Volume->BlockHandle = SetupOpenDestination(Destination, O_RDWR, 0);
        }

        if (Volume->BlockHandle == NULL) {
            printf("Error: Failed to open: ");
            SetupPrintDestination(Destination);
            Result = errno;
            if (Result > 0) {
                printf(": %s\n", strerror(errno));

            } else {
                Result = -1;
                printf("\n");
            }

            goto OpenVolumeEnd;
        }

        //
        // Fill out the block device parameters.
        //

        BlockParameters.DeviceToken = Volume;
        BlockParameters.BlockSize = SETUP_BLOCK_SIZE;
        if ((Destination->Type == SetupDestinationPartition) ||
            (Destination->Type == SetupDestinationDisk)) {

            BlockParameters.BlockCount = Context->CurrentPartitionSize;

        } else {
            Result = SetupFstat(Volume->BlockHandle,
                                &(BlockParameters.BlockCount),
                                NULL,
                                NULL);

            if (Result != 0) {
                goto OpenVolumeEnd;
            }

            BlockParameters.BlockCount /= SETUP_BLOCK_SIZE;
        }

        MountFlags = 0;
        if (CompatibilityMode != FALSE) {
            MountFlags |= FAT_MOUNT_FLAG_COMPATIBILITY_MODE;
        }

        //
        // Potentially try to mount the volume without formatting it.
        //

        Status = STATUS_NOT_STARTED;
        if (Format == SetupVolumeFormatIfIncompatible) {
            Status = FatMount(&BlockParameters,
                              MountFlags,
                              &(Volume->VolumeToken));
        }

        //
        // Format the volume if needed.
        //

        if ((Format == SetupVolumeFormatAlways) ||
            ((Format == SetupVolumeFormatIfIncompatible) &&
             (!KSUCCESS(Status)))) {

            Status = FatFormat(&BlockParameters, 0, 0);
            if (!KSUCCESS(Status)) {
                printf("Error: Failed to format ");
                SetupPrintDestination(Destination);
                printf(": %d\n", Status);
                Result = -1;
                goto OpenVolumeEnd;
            }
        }

        Status = FatMount(&BlockParameters, MountFlags, &(Volume->VolumeToken));
        if (!KSUCCESS(Status)) {
            printf("Error: Failed to mount ");
            SetupPrintDestination(Destination);
            printf(": %d\n", Status);
            Result = -1;
            goto OpenVolumeEnd;
        }

    //
    // This is a directory, just copy the prefix over.
    //

    } else {

        assert(Destination->Type == SetupDestinationDirectory);

        if (Destination->Path == NULL) {
            fprintf(stderr,
                    "Error: Installations to a directory need a path-based "
                    "destination.\n");

            Result = -1;
            goto OpenVolumeEnd;
        }

        Volume->PathPrefix = strdup(Destination->Path);
        if (Volume->PathPrefix == NULL) {
            Result = ENOMEM;
            goto OpenVolumeEnd;
        }
    }

    Result = 0;

OpenVolumeEnd:
    if (Result != 0) {
        if (Volume != NULL) {
            SetupVolumeClose(Context, Volume);
            Volume = NULL;
        }
    }

    return Volume;
}

VOID
SetupVolumeClose (
    PSETUP_CONTEXT Context,
    PVOID Handle
    )

/*++

Routine Description:

    This routine closes a volume.

Arguments:

    Context - Supplies a pointer to the application context.

    Handle - Supplies a pointer to the open volume handle.

Return Value:

    None.

--*/

{

    PSETUP_VOLUME Volume;

    Volume = Handle;
    if (Volume->VolumeToken != NULL) {
        FatUnmount(Volume->VolumeToken);
    }

    if ((Volume->BlockHandle != NULL) &&
        (Volume->BlockHandle != Context->Disk)) {

        SetupClose(Volume->BlockHandle);
    }

    if (Volume->PathPrefix != NULL) {
        free(Volume->PathPrefix);
    }

    free(Volume);
    return;
}

INT
SetupFileReadLink (
    PVOID Handle,
    PCSTR Path,
    PSTR *LinkTarget,
    INT *LinkTargetSize
    )

/*++

Routine Description:

    This routine attempts to read a symbolic link.

Arguments:

    Handle - Supplies the volume handle.

    Path - Supplies a pointer to the path to open.

    LinkTarget - Supplies a pointer where an allocated link target will be
        returned on success. The caller is responsible for freeing this memory.

    LinkTargetSize - Supplies a pointer where the size of the link target will
        be returned on success.

Return Value:

    Returns the link size on success.

    -1 on failure.

--*/

{

    PVOID File;
    PSTR FinalPath;
    mode_t Mode;
    INT Result;
    ULONGLONG Size;
    PSETUP_VOLUME Volume;

    File = NULL;
    *LinkTarget = NULL;
    Volume = Handle;
    FinalPath = SetupAppendPaths(Volume->PathPrefix, Path);
    if (FinalPath == NULL) {
        Result = ENOMEM;
        goto FileReadLinkEnd;
    }

    Result = -1;

    //
    // If it's the native interface, append the path and send the request
    // down directly.
    //

    if (Volume->DestinationType == SetupDestinationDirectory) {
        Result = SetupOsReadLink(FinalPath, LinkTarget, LinkTargetSize);

    //
    // Route this through the file system code.
    //

    } else {
        File = SetupFileOpen(Handle, Path, O_RDONLY, 0);
        if (File == NULL) {
            goto FileReadLinkEnd;
        }

        Result = SetupFileFileStat(File, &Size, NULL, &Mode);
        if ((Result != 0) || (S_ISLNK(Mode) == 0)) {
            Result = -1;
            goto FileReadLinkEnd;
        }

        *LinkTarget = malloc(Size + 1);
        if (*LinkTarget == NULL) {
            Result = -1;
            goto FileReadLinkEnd;
        }

        Result = SetupFileRead(File, *LinkTarget, Size);
        if (Result != Size) {
            Result = -1;
            goto FileReadLinkEnd;
        }

        (*LinkTarget)[Size] = '\0';
        *LinkTargetSize = Size;
        Result = 0;
    }

FileReadLinkEnd:
    if (Result != 0) {
        if (*LinkTarget != NULL) {
            free(*LinkTarget);
            *LinkTarget = NULL;
        }
    }

    if (File != NULL) {
        SetupFileClose(File);
        File = NULL;
    }

    if (FinalPath != NULL) {
        free(FinalPath);
    }

    return Result;
}

INT
SetupFileSymlink (
    PVOID Handle,
    PCSTR Path,
    PSTR LinkTarget,
    INT LinkTargetSize
    )

/*++

Routine Description:

    This routine creates a symbolic link.

Arguments:

    Handle - Supplies the volume handle.

    Path - Supplies a pointer to the path of the symbolic link to create.

    LinkTarget - Supplies a pointer to the target of the link.

    LinkTargetSize - Supplies a the size of the link target buffer in bytes.

Return Value:

    Returns the link size on success.

    -1 on failure.

--*/

{

    PSETUP_FILE File;
    PSTR FinalPath;
    INT Result;
    PSETUP_VOLUME Volume;

    File = NULL;
    Volume = Handle;
    FinalPath = SetupAppendPaths(Volume->PathPrefix, Path);
    if (FinalPath == NULL) {
        Result = ENOMEM;
        goto FileSymlinkEnd;
    }

    Result = -1;

    //
    // If it's the native interface, append the path and send the request
    // down directly.
    //

    if (Volume->DestinationType == SetupDestinationDirectory) {
        Result = SetupOsSymlink(FinalPath, LinkTarget, LinkTargetSize);

    //
    // Route this through the file system code.
    //

    } else {
        File = SetupFileOpen(Handle,
                             Path,
                             O_WRONLY | O_CREAT | O_TRUNC,
                             FILE_PERMISSION_ALL);

        if (File == NULL) {
            goto FileSymlinkEnd;
        }

        Result = SetupFileWrite(File, LinkTarget, LinkTargetSize);
        if (Result != LinkTargetSize) {
            Result = -1;
            goto FileSymlinkEnd;
        }

        File->Properties.Permissions |= FILE_PERMISSION_ALL;
        File->Properties.Type = IoObjectSymbolicLink;
        File->IsDirty = TRUE;
        Result = 0;
    }

FileSymlinkEnd:
    if (File != NULL) {
        SetupFileClose(File);
        File = NULL;
    }

    if (FinalPath != NULL) {
        free(FinalPath);
    }

    return Result;
}

PVOID
SetupFileOpen (
    PVOID Handle,
    PCSTR Path,
    INT Flags,
    INT CreatePermissions
    )

/*++

Routine Description:

    This routine opens a handle to a file in a volume.

Arguments:

    Handle - Supplies the volume handle.

    Path - Supplies a pointer to the path to open.

    Flags - Supplies open flags. See O_* definitions.

    CreatePermissions - Supplies optional create permissions.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

{

    PSETUP_DESTINATION Destination;
    PSETUP_FILE File;
    PSTR FinalPath;
    INT Result;
    PSETUP_VOLUME Volume;

    File = NULL;
    Volume = Handle;
    FinalPath = SetupAppendPaths(Volume->PathPrefix, Path);
    if (FinalPath == NULL) {
        Result = ENOMEM;
        goto OpenFileEnd;
    }

    File = malloc(sizeof(SETUP_FILE));
    if (File == NULL) {
        Result = ENOMEM;
        goto OpenFileEnd;
    }

    memset(File, 0, sizeof(SETUP_FILE));
    File->Volume = Volume;

    //
    // If it's the native interface, append the path and send the request
    // down directly.
    //

    if (Volume->DestinationType == SetupDestinationDirectory) {
        Destination = SetupCreateDestination(SetupDestinationFile,
                                             FinalPath,
                                             0);

        if (Destination == NULL) {
            Result = ENOMEM;
            goto OpenFileEnd;
        }

        File->Handle = SetupOpenDestination(Destination,
                                            Flags,
                                            CreatePermissions);

        SetupDestroyDestination(Destination);
        if (File->Handle == NULL) {
            Result = errno;
            goto OpenFileEnd;
        }

    //
    // Route this through the file system code.
    //

    } else {

        assert((Volume->DestinationType == SetupDestinationDisk) ||
               (Volume->DestinationType == SetupDestinationPartition) ||
               (Volume->DestinationType == SetupDestinationImage));

        Result = SetupFatOpen(Volume,
                              File,
                              FinalPath,
                              Flags,
                              CreatePermissions,
                              FALSE);

        if (Result != 0) {
            goto OpenFileEnd;
        }
    }

    Result = 0;

OpenFileEnd:
    if (Result != 0) {
        if (File != NULL) {
            SetupFileClose(File);
            File = NULL;
        }
    }

    if (FinalPath != NULL) {
        free(FinalPath);
    }

    return File;
}

VOID
SetupFileClose (
    PVOID Handle
    )

/*++

Routine Description:

    This routine closes a file.

Arguments:

    Handle - Supplies the handle to close.

Return Value:

    None.

--*/

{

    PSETUP_FILE File;

    File = Handle;
    if (File->FatFile != NULL) {
        FatCloseFile(File->FatFile);
    }

    if (File->IsDirty != FALSE) {
        FatWriteFileProperties(File->Volume->VolumeToken,
                               &(File->Properties),
                               0);
    }

    if (File->Handle != NULL) {
        SetupClose(File->Handle);
    }

    File->Volume->OpenFiles -= 1;
    free(File);
    return;
}

ssize_t
SetupFileRead (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine reads from a file.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read on success.

--*/

{

    UINTN BytesComplete;
    PSETUP_FILE File;
    PFAT_IO_BUFFER IoBuffer;
    KSTATUS Status;

    File = Handle;

    //
    // Pass directly to the native OS interface if the destination is native.
    //

    if (File->Volume->DestinationType == SetupDestinationDirectory) {
        return SetupRead(File->Handle, Buffer, ByteCount);
    }

    assert((File->Volume->DestinationType == SetupDestinationDisk) ||
           (File->Volume->DestinationType == SetupDestinationPartition) ||
           (File->Volume->DestinationType == SetupDestinationImage));

    if ((File->Properties.Type != IoObjectRegularFile) &&
        (File->Properties.Type != IoObjectSymbolicLink)) {

        return -1;
    }

    IoBuffer = FatCreateIoBuffer(Buffer, ByteCount);
    if (IoBuffer == NULL) {
        return -1;
    }

    BytesComplete = 0;
    Status = FatReadFile(File->FatFile,
                         &(File->SeekInformation),
                         IoBuffer,
                         ByteCount,
                         0,
                         NULL,
                         &BytesComplete);

    ASSERT(BytesComplete <= ByteCount);

    File->CurrentOffset += BytesComplete;
    if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        fprintf(stderr, "FatReadFile Error: %d\n", Status);
        BytesComplete = 0;
    }

    if (IoBuffer != NULL) {
        FatFreeIoBuffer(IoBuffer);
    }

    return (ssize_t)BytesComplete;
}

ssize_t
SetupFileWrite (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine writes data to an open file handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the bytes to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

    -1 on failure.

--*/

{

    UINTN BytesComplete;
    PSETUP_FILE File;
    ULONGLONG FileSize;
    PFAT_IO_BUFFER IoBuffer;
    KSTATUS Status;

    File = Handle;

    //
    // Pass directly to the native OS interface if the destination is native.
    //

    if (File->Volume->DestinationType == SetupDestinationDirectory) {
        return SetupWrite(File->Handle, Buffer, ByteCount);
    }

    assert((File->Volume->DestinationType == SetupDestinationDisk) ||
           (File->Volume->DestinationType == SetupDestinationPartition) ||
           (File->Volume->DestinationType == SetupDestinationImage));

    if ((File->Properties.Type != IoObjectRegularFile) &&
        (File->Properties.Type != IoObjectSymbolicLink)) {

        errno = EISDIR;
        return -1;
    }

    IoBuffer = FatCreateIoBuffer(Buffer, ByteCount);
    if (IoBuffer == NULL) {
        return -1;
    }

    BytesComplete = 0;
    FileSize = File->Properties.Size;
    Status = FatWriteFile(File->FatFile,
                          &(File->SeekInformation),
                          IoBuffer,
                          ByteCount,
                          0,
                          NULL,
                          &BytesComplete);

    ASSERT(BytesComplete <= ByteCount);

    //
    // Advance the current position. Mark the file dirty and update the size
    // if the write made the file bigger.
    //

    File->CurrentOffset += BytesComplete;
    if (File->CurrentOffset > FileSize) {
        FileSize = File->CurrentOffset;
        File->Properties.Size = FileSize;
        File->IsDirty = TRUE;
    }

    if (!KSUCCESS(Status)) {
        fprintf(stderr, "FatWriteFile Error: %d\n", Status);
        if (Status == STATUS_VOLUME_FULL) {
            errno = ENOSPC;
        }

        BytesComplete = 0;
    }

    if (IoBuffer != NULL) {
        FatFreeIoBuffer(IoBuffer);
    }

    return (ssize_t)BytesComplete;
}

LONGLONG
SetupFileSeek (
    PVOID Handle,
    LONGLONG Offset
    )

/*++

Routine Description:

    This routine seeks in the given file.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the new offset to set.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

{

    PSETUP_FILE File;
    KSTATUS Status;

    File = Handle;

    //
    // Pass directly to the native OS interface if the destination is native.
    //

    if (File->Volume->DestinationType == SetupDestinationDirectory) {
        return SetupSeek(File->Handle, Offset);
    }

    assert((File->Volume->DestinationType == SetupDestinationDisk) ||
           (File->Volume->DestinationType == SetupDestinationPartition) ||
           (File->Volume->DestinationType == SetupDestinationImage));

    if (File->Properties.Type != IoObjectRegularFile) {
        return -1;
    }

    Status = FatFileSeek(File->FatFile,
                         NULL,
                         0,
                         SeekCommandFromBeginning,
                         Offset,
                         &(File->SeekInformation));

    if (!KSUCCESS(Status)) {
        fprintf(stderr, "FatFileSeek Error: %d\n", Status);
        Offset = -1;

    } else {
        File->CurrentOffset = Offset;
    }

    return Offset;
}

INT
SetupFileFileStat (
    PVOID Handle,
    PULONGLONG FileSize,
    time_t *ModificationDate,
    mode_t *Mode
    )

/*++

Routine Description:

    This routine gets details for the given open file.

Arguments:

    Handle - Supplies the handle.

    FileSize - Supplies an optional pointer where the file size will be
        returned on success.

    ModificationDate - Supplies an optional pointer where the file's
        modification date will be returned on success.

    Mode - Supplies an optional pointer where the file's mode information will
        be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_FILE File;

    File = Handle;
    if (File->Volume->DestinationType == SetupDestinationDirectory) {
        return SetupFstat(File->Handle, FileSize, ModificationDate, Mode);
    }

    if (FileSize != NULL) {
        *FileSize = File->Properties.Size;
    }

    if (ModificationDate != NULL) {
        *ModificationDate = File->Properties.ModifiedTime.Seconds +
                            SYSTEM_TIME_TO_EPOCH_DELTA;
    }

    if (Mode != NULL) {
        *Mode = 0;
        if (File->Properties.Type == IoObjectRegularDirectory) {
            *Mode |= S_IFDIR;

        } else if (File->Properties.Type == IoObjectSymbolicLink) {
            *Mode |= S_IFLNK;

        } else {
            *Mode |= S_IFREG;
        }

        *Mode |= File->Properties.Permissions;
    }

    return 0;
}

INT
SetupFileFileTruncate (
    PVOID Handle,
    ULONGLONG NewSize
    )

/*++

Routine Description:

    This routine sets the file size of the given file.

Arguments:

    Handle - Supplies the handle.

    NewSize - Supplies the new file size.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONGLONG CurrentSize;
    PSETUP_FILE File;
    KSTATUS Status;

    File = Handle;
    if (File->Volume->DestinationType == SetupDestinationDirectory) {
        return SetupFtruncate(File->Handle, NewSize);
    }

    CurrentSize = File->Properties.Size;
    if (CurrentSize == NewSize) {
        return 0;

    } else if (NewSize < CurrentSize) {
        Status = FatDeleteFileBlocks(File->Volume->VolumeToken,
                                     File->Handle,
                                     File->Properties.FileId,
                                     NewSize,
                                     TRUE);

    } else {
        Status = FatAllocateFileClusters(File->Volume->VolumeToken,
                                         File->Properties.FileId,
                                         NewSize);
    }

    if (!KSUCCESS(Status)) {
        fprintf(stderr, "FatTruncate Error: %d\n", Status);
        return -1;
    }

    File->Properties.Size = NewSize;
    File->IsDirty = TRUE;
    return 0;
}

INT
SetupFileEnumerateDirectory (
    PVOID VolumeHandle,
    PCSTR DirectoryPath,
    PSTR *Enumeration
    )

/*++

Routine Description:

    This routine enumerates the contents of a given directory.

Arguments:

    VolumeHandle - Supplies the open volume handle.

    DirectoryPath - Supplies a pointer to a string containing the path to the
        directory to enumerate.

    Enumeration - Supplies a pointer where a pointer to a sequence of
        strings will be returned containing the files in the directory. The
        sequence will be terminated by an empty string. The caller is
        responsible for freeing this memory when done.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Array;
    size_t ArrayCapacity;
    UINTN BytesRead;
    PDIRECTORY_ENTRY DirectoryEntry;
    ULONG ElementsRead;
    ULONGLONG EntryOffset;
    SETUP_FILE File;
    PSTR FinalPath;
    PFAT_IO_BUFFER IoBuffer;
    PSTR Name;
    size_t NameSize;
    PVOID NewBuffer;
    size_t NewCapacity;
    INT Result;
    KSTATUS Status;
    size_t UsedSize;
    PSETUP_VOLUME Volume;

    DirectoryEntry = NULL;
    IoBuffer = NULL;
    Name = NULL;
    Volume = VolumeHandle;
    memset(&File, 0, sizeof(SETUP_FILE));
    File.Volume = VolumeHandle;
    if (Volume->DestinationType == SetupDestinationDirectory) {
        FinalPath = SetupAppendPaths(Volume->PathPrefix, DirectoryPath);
        if (FinalPath == NULL) {
            Result = ENOMEM;
            goto EnumerateFileDirectoryEnd;
        }

        Result = SetupEnumerateDirectory(File.Volume,
                                         FinalPath,
                                         Enumeration);

        free(FinalPath);
        return Result;
    }

    Result = SetupFatOpen(VolumeHandle,
                          &File,
                          DirectoryPath,
                          0,
                          0,
                          TRUE);

    if (Result != 0) {
        return Result;
    }

    Array = NULL;
    ArrayCapacity = 0;
    UsedSize = 0;
    EntryOffset = DIRECTORY_CONTENTS_OFFSET;
    DirectoryEntry = malloc(SETUP_DIRECTORY_ENTRY_SIZE);
    if (DirectoryEntry == NULL) {
        goto EnumerateFileDirectoryEnd;
    }

    IoBuffer = FatCreateIoBuffer(DirectoryEntry, SETUP_DIRECTORY_ENTRY_SIZE);
    if (IoBuffer == NULL) {
        goto EnumerateFileDirectoryEnd;
    }

    //
    // Loop reading directory entries.
    //

    while (TRUE) {
        BytesRead = 0;
        Status = FatEnumerateDirectory(File.FatFile,
                                       EntryOffset,
                                       IoBuffer,
                                       SETUP_DIRECTORY_ENTRY_SIZE,
                                       TRUE,
                                       FALSE,
                                       NULL,
                                       &BytesRead,
                                       &ElementsRead);

        if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
            fprintf(stderr, "FatEnumerateDirectory Error: %d\n", Status);
            Result = -1;
            goto EnumerateFileDirectoryEnd;
        }

        NameSize = 1;
        if (Status != STATUS_END_OF_FILE) {
            Name = (PVOID)(DirectoryEntry + 1);
            NameSize = strlen(Name) + 1;
        }

        //
        // Reallocate the array if needed.
        //

        if (ArrayCapacity - UsedSize < NameSize) {
            NewCapacity = ArrayCapacity;
            if (NewCapacity == 0) {
                NewCapacity = 2;
            }

            while (NewCapacity - UsedSize < NameSize) {
                NewCapacity *= 2;
            }

            NewBuffer = realloc(Array, NewCapacity);
            if (NewBuffer == NULL) {
                Result = ENOMEM;
                goto EnumerateFileDirectoryEnd;
            }

            Array = NewBuffer;
            ArrayCapacity = NewCapacity;
        }

        //
        // Copy the entry (or an empty file if this is the end).
        //

        if (Status == STATUS_END_OF_FILE) {
            strcpy(Array + UsedSize, "");
            UsedSize += 1;
            Status = STATUS_SUCCESS;
            break;

        } else {
            strcpy(Array + UsedSize, Name);
            UsedSize += NameSize;
        }

        assert(ElementsRead != 0);

        EntryOffset += ElementsRead;
    }

    Result = 0;

EnumerateFileDirectoryEnd:
    if (DirectoryEntry != NULL) {
        free(DirectoryEntry);
    }

    if (IoBuffer != NULL) {
        FatFreeIoBuffer(IoBuffer);
    }

    if (File.FatFile != NULL) {
        FatCloseFile(File.FatFile);
    }

    if (Result != 0) {
        if (Array != NULL) {
            free(Array);
            Array = NULL;
        }
    }

    *Enumeration = Array;
    return Result;
}

INT
SetupFileCreateDirectory (
    PVOID VolumeHandle,
    PCSTR Path,
    mode_t Permissions
    )

/*++

Routine Description:

    This routine creates a new directory.

Arguments:

    VolumeHandle - Supplies a pointer to the volume handle.

    Path - Supplies the path string of the directory to create.

    Permissions - Supplies the permission bits to create the file with.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    SETUP_FILE File;
    PSTR FinalPath;
    INT Result;
    PSETUP_VOLUME Volume;

    Volume = VolumeHandle;
    if (Volume->DestinationType == SetupDestinationDirectory) {
        FinalPath = SetupAppendPaths(Volume->PathPrefix, Path);
        if (FinalPath == NULL) {
            Result = ENOMEM;
            return Result;
        }

        Result = SetupOsCreateDirectory(FinalPath, Permissions);
        if (Result == EEXIST) {
            Result = 0;
        }

        free(FinalPath);
        return Result;
    }

    memset(&File, 0, sizeof(SETUP_FILE));
    File.Volume = VolumeHandle;
    Result = SetupFatOpen(VolumeHandle,
                          &File,
                          Path,
                          O_CREAT,
                          Permissions,
                          TRUE);

    if (Result != 0) {
        return Result;
    }

    if (File.FatFile != NULL) {
        FatCloseFile(File.FatFile);
    }

    return Result;
}

INT
SetupFileSetAttributes (
    PVOID VolumeHandle,
    PCSTR Path,
    time_t ModificationDate,
    mode_t Permissions
    )

/*++

Routine Description:

    This routine sets attributes on a given path.

Arguments:

    VolumeHandle - Supplies a pointer to the volume handle.

    Path - Supplies the path string of the file to modify.

    ModificationDate - Supplies the new modification date to set.

    Permissions - Supplies the new permissions to set.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    SETUP_FILE File;
    PSTR FinalPath;
    BOOL IsDirectory;
    INT Result;
    PSETUP_VOLUME Volume;

    Volume = VolumeHandle;
    if (Volume->DestinationType == SetupDestinationDirectory) {
        FinalPath = SetupAppendPaths(Volume->PathPrefix, Path);
        if (FinalPath == NULL) {
            return ENOMEM;
        }

        Result = SetupOsSetAttributes(FinalPath, ModificationDate, Permissions);
        free(FinalPath);
        return Result;
    }

    memset(&File, 0, sizeof(SETUP_FILE));
    File.Volume = VolumeHandle;
    IsDirectory = FALSE;
    if (S_ISDIR(Permissions) != 0) {
        IsDirectory = TRUE;
    }

    Result = SetupFatOpen(VolumeHandle,
                          &File,
                          Path,
                          0,
                          0,
                          IsDirectory);

    if (Result != 0) {
        return Result;
    }

    File.Properties.AccessTime.Seconds =
                                       time(NULL) - SYSTEM_TIME_TO_EPOCH_DELTA;

    File.Properties.AccessTime.Nanoseconds = 0;
    File.Properties.ModifiedTime.Seconds = ModificationDate -
                                           SYSTEM_TIME_TO_EPOCH_DELTA;

    File.Properties.ModifiedTime.Nanoseconds = 0;
    File.Properties.Permissions = Permissions & FILE_PERMISSION_MASK;
    if (S_ISDIR(Permissions)) {
        File.Properties.Type = IoObjectRegularDirectory;

    } else if (S_ISLNK(Permissions)) {
        File.Properties.Type = IoObjectSymbolicLink;

    } else {
        File.Properties.Type = IoObjectRegularFile;
    }

    FatCloseFile(File.FatFile);
    FatWriteFileProperties(Volume->VolumeToken, &(File.Properties), 0);
    return 0;
}

VOID
SetupFileDetermineExecuteBit (
    PVOID Handle,
    PCSTR Path,
    mode_t *Mode
    )

/*++

Routine Description:

    This routine determines whether the open file is executable.

Arguments:

    Handle - Supplies the open file handle.

    Path - Supplies the path the file was opened from (sometimes the file name
        is used as a hint).

    Mode - Supplies a pointer to the current mode bits. This routine may add
        the executable bit to user/group/other if it determines this file is
        executable.

Return Value:

    None.

--*/

{

    PSETUP_FILE File;

    File = Handle;

    //
    // Pass directly to the native OS interface if the destination is native.
    //

    if (File->Volume->DestinationType == SetupDestinationDirectory) {
        return SetupDetermineExecuteBit(File->Handle, Path, Mode);
    }

    assert((File->Volume->DestinationType == SetupDestinationDisk) ||
           (File->Volume->DestinationType == SetupDestinationPartition) ||
           (File->Volume->DestinationType == SetupDestinationImage));

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupFatOpen (
    PSETUP_VOLUME Volume,
    PSETUP_FILE NewFile,
    PCSTR Path,
    INT Flags,
    INT CreatePermissions,
    BOOL Directory
    )

/*++

Routine Description:

    This routine opens a file in a FAT image.

Arguments:

    Volume - Supplies a pointer to the volume.

    NewFile - Supplies a pointer to the new file being opened.

    Path - Supplies a pointer to the path to open.

    Flags - Supplies open flags. See O_* definitions.

    CreatePermissions - Supplies optional create permissions.

    Directory - Supplies a boolean indicating if this is a directory open or
        file open.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR CurrentPath;
    size_t CurrentPathLength;
    ULONG DesiredAccess;
    FILE_ID DirectoryFileId;
    ULONG FatOpenFlags;
    ULONGLONG NewDirectorySize;
    FILE_PROPERTIES NewProperties;
    PSTR OpenedFileName;
    size_t OpenedFileNameLength;
    PSTR PathCopy;
    FILE_PROPERTIES Properties;
    KSTATUS Status;

    DirectoryFileId = 0;
    PathCopy = NULL;

    //
    // Start at the root.
    //

    Status = FatLookup(Volume->VolumeToken, TRUE, 0, NULL, 0, &Properties);
    if (!KSUCCESS(Status)) {
        goto FatOpenEnd;
    }

    PathCopy = SetupFatCopyPath(Path);
    if (PathCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto FatOpenEnd;
    }

    if (*PathCopy == '\0') {
        Status = STATUS_NOT_FOUND;
        goto FatOpenEnd;
    }

    //
    // Loop opening the next component in the path.
    //

    CurrentPath = PathCopy;
    CurrentPathLength = strlen(CurrentPath);
    Status = STATUS_SUCCESS;
    while (TRUE) {
        if (CurrentPathLength == 0) {
            break;
        }

        DirectoryFileId = Properties.FileId;
        OpenedFileName = CurrentPath;
        OpenedFileNameLength = CurrentPathLength;
        Status = FatLookup(Volume->VolumeToken,
                           FALSE,
                           Properties.FileId,
                           CurrentPath,
                           CurrentPathLength + 1,
                           &Properties);

        //
        // If the file was not found, stop.
        //

        if ((Status == STATUS_NO_SUCH_FILE) ||
            (Status == STATUS_NOT_FOUND) ||
            (Status == STATUS_PATH_NOT_FOUND)) {

            Status = STATUS_NOT_FOUND;
            break;

        //
        // If some wackier error occured, fail the whole function.
        //

        } else if (!KSUCCESS(Status)) {
            goto FatOpenEnd;
        }

        //
        // This file was found, move to the next path component.
        //

        CurrentPath += CurrentPathLength + 1;
        CurrentPathLength = strlen(CurrentPath);

        //
        // If the file was not a directory, nothing more can be looked up
        // underneath this, so stop.
        //

        if (Properties.Type != IoObjectRegularDirectory) {
            break;
        }
    }

    ASSERT((Status == STATUS_SUCCESS) || (Status == STATUS_NOT_FOUND));

    //
    // Okay, either the path ended, the file was not found, or the file was
    // not a directory. If the file was found, but an exclusive open was
    // requested, fail.
    //

    if (Status == STATUS_SUCCESS) {
        if ((CurrentPathLength == 0) &&
            ((Flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))) {

            Status = STATUS_FILE_EXISTS;
            goto FatOpenEnd;
        }

    //
    // If the file was not found, maybe create it.
    //

    } else if (Status == STATUS_NOT_FOUND) {

        //
        // If the file doesn't exist and the caller doesn't want to create it,
        // then return not found.
        //

        if ((Flags & O_CREAT) == 0) {
            goto FatOpenEnd;
        }

        //
        // Fail if the volume is read-only.
        //

        if (Volume->DestinationType == SetupDestinationImage) {
            Status = STATUS_ACCESS_DENIED;
            goto FatOpenEnd;
        }

        //
        // The caller wants to create a file or directory. If the last
        // successful lookup wasn't a directory, fail.
        //

        if (Properties.Type != IoObjectRegularDirectory) {
            goto FatOpenEnd;
        }

        //
        // If this isn't the last component, also fail.
        //

        if (CurrentPath[CurrentPathLength + 1] != '\0') {
            goto FatOpenEnd;
        }

        //
        // Create the new file or directory.
        //

        memcpy(&NewProperties, &Properties, sizeof(FILE_PROPERTIES));
        NewProperties.Type = IoObjectRegularFile;
        if (Directory != FALSE) {
            NewProperties.Type = IoObjectRegularDirectory;
        }

        NewProperties.Permissions = CreatePermissions;
        NewProperties.FileId = 0;
        NewProperties.Size = 0;
        FatGetCurrentSystemTime(&(NewProperties.StatusChangeTime));
        OpenedFileName = CurrentPath;
        OpenedFileNameLength = CurrentPathLength;
        Status = FatCreate(Volume->VolumeToken,
                           Properties.FileId,
                           OpenedFileName,
                           OpenedFileNameLength + 1,
                           &NewDirectorySize,
                           &NewProperties);

        if (!KSUCCESS(Status)) {
            goto FatOpenEnd;
        }

        //
        // Update the directory properties, as that new file may have made the
        // directory bigger.
        //

        Properties.Size = NewDirectorySize;
        Status = FatWriteFileProperties(Volume->VolumeToken, &Properties, 0);
        if (!KSUCCESS(Status)) {
            goto FatOpenEnd;
        }

        //
        // Make it look like this new file was successfully looked up by the
        // above loop.
        //

        CurrentPathLength = 0;
        memcpy(&Properties, &NewProperties, sizeof(FILE_PROPERTIES));
    }

    //
    // If there are more components to the path, then this lookup failed.
    //

    if (CurrentPathLength != 0) {
        Status = STATUS_PATH_NOT_FOUND;
        goto FatOpenEnd;
    }

    //
    // If the file is a symbolic link, don't open it if the caller specified
    // the "no follow" flag.
    //

    if ((Properties.Type == IoObjectSymbolicLink) &&
        ((Flags & O_NOFOLLOW) != 0)) {

        Status = STATUS_UNEXPECTED_TYPE;
        goto FatOpenEnd;
    }

    memcpy(&(NewFile->Properties), &Properties, sizeof(FILE_PROPERTIES));
    memset(&(NewFile->SeekInformation), 0, sizeof(FAT_SEEK_INFORMATION));
    NewFile->CurrentOffset = 0;
    NewFile->FatFile = NULL;
    NewFile->DirectoryFileId = DirectoryFileId;
    DesiredAccess = 0;
    switch (Flags & O_ACCMODE) {
    case O_RDONLY:
        DesiredAccess = IO_ACCESS_READ;
        break;

    case O_WRONLY:
        DesiredAccess = IO_ACCESS_WRITE;
        break;

    case O_RDWR:
        DesiredAccess = IO_ACCESS_READ | IO_ACCESS_WRITE;
        break;
    }

    //
    // Truncate the file if desired.
    //

    if ((Flags & O_TRUNC) != 0) {

        assert(Directory == FALSE);

        Status = FatDeleteFileBlocks(Volume->VolumeToken,
                                     NULL,
                                     Properties.FileId,
                                     0,
                                     TRUE);

        if (!KSUCCESS(Status)) {
            goto FatOpenEnd;
        }

        NewFile->Properties.Size = 0;
    }

    FatOpenFlags = 0;
    if (Directory != FALSE) {
        FatOpenFlags |= OPEN_FLAG_DIRECTORY;
    }

    Status = FatOpenFileId(Volume->VolumeToken,
                           Properties.FileId,
                           DesiredAccess,
                           FatOpenFlags,
                           &(NewFile->FatFile));

    if (!KSUCCESS(Status)) {
        goto FatOpenEnd;
    }

    Volume->OpenFiles += 1;
    Status = STATUS_SUCCESS;

FatOpenEnd:
    if (PathCopy != NULL) {
        free(PathCopy);
    }

    if (!KSUCCESS(Status)) {
        if ((Status == STATUS_NOT_FOUND) || (Status == STATUS_PATH_NOT_FOUND)) {
            errno = ENOENT;

        } else if (Status == STATUS_VOLUME_FULL) {
            errno = ENOSPC;
        }

        if ((Status != STATUS_NOT_FOUND) &&
            (Status != STATUS_UNEXPECTED_TYPE)) {

            fprintf(stderr, "FatOpenFile Error %s: %d\n", Path, Status);
            errno = EINVAL;
        }

        return -1;
    }

    return 0;
}

PSTR
SetupFatCopyPath (
    PCSTR InputPath
    )

/*++

Routine Description:

    This routine creates a copy of the given path, separating slashes with
    terminators along the way.

Arguments:

    InputPath - Supplies a pointer to the input path.

Return Value:

    Returns a pointer to the separated path, terminated with an additional
    NULL terminator.

    NULL on allocation failure.

--*/

{

    PCSTR CurrentInput;
    PSTR CurrentOutput;
    UINTN Length;
    PSTR NewPath;

    while (*InputPath == '/') {
        InputPath += 1;
    }

    CurrentInput = InputPath;
    Length = 2;
    while (*CurrentInput != '\0') {
        Length += 1;
        CurrentInput += 1;
    }

    NewPath = malloc(Length);
    if (NewPath == NULL) {
        return NULL;
    }

    CurrentInput = InputPath;
    CurrentOutput = NewPath;
    while (*CurrentInput != '\0') {

        //
        // If it's a slash, then terminate the current output and get past
        // the backslash (and any additional consecutive ones).
        //

        if (*CurrentInput == '/') {
            *CurrentOutput = '\0';
            CurrentOutput += 1;
            while (*CurrentInput == '/') {
                CurrentInput += 1;
            }

            continue;
        }

        *CurrentOutput = *CurrentInput;
        CurrentOutput += 1;
        CurrentInput += 1;
    }

    //
    // Double terminate the string.
    //

    *CurrentOutput = '\0';
    CurrentOutput += 1;
    *CurrentOutput = '\0';
    return NewPath;
}

