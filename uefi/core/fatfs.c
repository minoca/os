/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatfs.c

Abstract:

    This module implements FAT simple file system support.

Author:

    Evan Green 21-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/fat/fat.h>
#include <uefifw.h>
#include <minoca/uefi/protocol/diskio.h>
#include <minoca/uefi/protocol/blockio.h>
#include <minoca/uefi/protocol/sfilesys.h>
#include <minoca/uefi/protocol/drvbind.h>
#include "fatfs.h"
#include "fileinfo.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfiFatSupported (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

EFIAPI
EFI_STATUS
EfiFatStart (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

EFIAPI
EFI_STATUS
EfiFatStop (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    UINTN NumberOfChildren,
    EFI_HANDLE *ChildHandleBuffer
    );

EFIAPI
EFI_STATUS
EfiFatOpenVolume (
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    EFI_FILE_PROTOCOL **Root
    );

EFIAPI
EFI_STATUS
EfiFatOpen (
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_PROTOCOL **NewHandle,
    CHAR16 *FileName,
    UINT64 OpenMode,
    UINT64 Attributes
    );

EFIAPI
EFI_STATUS
EfiFatClose (
    EFI_FILE_PROTOCOL *This
    );

EFIAPI
EFI_STATUS
EfiFatDelete (
    EFI_FILE_PROTOCOL *This
    );

EFIAPI
EFI_STATUS
EfiFatRead (
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfiFatWrite (
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfiFatSetPosition (
    EFI_FILE_PROTOCOL *This,
    UINT64 Position
    );

EFIAPI
EFI_STATUS
EfiFatGetPosition (
    EFI_FILE_PROTOCOL *This,
    UINT64 *Position
    );

EFIAPI
EFI_STATUS
EfiFatGetInformation (
    EFI_FILE_PROTOCOL *This,
    EFI_GUID *InformationType,
    UINTN *BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfiFatSetInformation (
    EFI_FILE_PROTOCOL *This,
    EFI_GUID *InformationType,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfiFatFlush (
    EFI_FILE_PROTOCOL *This
    );

CHAR8 *
EfipFatCopyPath (
    CHAR16 *InputPath,
    BOOLEAN *StartsAtRoot
    );

INTN
EfipFatStringCompare (
    CHAR8 *String1,
    CHAR8 *String2
    );

UINTN
EfipFatStringLength (
    CHAR8 *String
    );

EFI_FILE_INFO *
EfipFatConvertDirectoryEntryToFileInfo (
    PEFI_FAT_FILE File,
    PDIRECTORY_ENTRY DirectoryEntry
    );

EFI_FILE_INFO *
EfipFatConvertFilePropertiesToFileInfo (
    PFILE_PROPERTIES Properties,
    CHAR8 *FileName,
    UINTN FileNameSize
    );

BOOLEAN
EfipFatCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_FAT_FILE EfiFatFileTemplate = {
    EFI_FAT_FILE_MAGIC,
    0xFFFFFFFF,
    {
        EFI_FILE_PROTOCOL_REVISION,
        EfiFatOpen,
        EfiFatClose,
        EfiFatDelete,
        EfiFatRead,
        EfiFatWrite,
        EfiFatGetPosition,
        EfiFatSetPosition,
        EfiFatGetInformation,
        EfiFatSetInformation,
        EfiFatFlush,
        NULL,
        NULL,
        NULL,
        NULL
    },

    NULL
};

EFI_DRIVER_BINDING_PROTOCOL EfiFatDriverBinding = {
    EfiFatSupported,
    EfiFatStart,
    EfiFatStop,
    0x9,
    NULL,
    NULL
};

EFI_GUID EfiFileInformationGuid = EFI_FILE_INFO_ID;
EFI_GUID EfiSimpleFileSystemProtocolGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

extern EFI_GUID EfiDiskIoProtocolGuid;
extern EFI_GUID EfiBlockIoProtocolGuid;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiFatDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine is the entry point into the disk I/O driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    EfiFatDriverBinding.ImageHandle = ImageHandle;
    EfiFatDriverBinding.DriverBindingHandle = ImageHandle;
    Status = EfiInstallMultipleProtocolInterfaces(
                                   &(EfiFatDriverBinding.DriverBindingHandle),
                                   &EfiDriverBindingProtocolGuid,
                                   &EfiFatDriverBinding,
                                   NULL);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfiFatSupported (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    )

/*++

Routine Description:

    This routine tests to see if the FAT driver supports this new
    controller handle. Any controller handle that contains a block I/O and
    disk I/O protocol is supported.

Arguments:

    This - Supplies a pointer to the driver binding instance.

    ControllerHandle - Supplies the new controller handle to test.

    RemainingDevicePath - Supplies an optional parameter to pick a specific
        child device to start.

Return Value:

    EFI status code.

--*/

{

    EFI_DISK_IO_PROTOCOL *DiskIo;
    EFI_DEV_PATH *Node;
    EFI_DEVICE_PATH_PROTOCOL *ParentDevicePath;
    EFI_STATUS Status;

    if (RemainingDevicePath != NULL) {
        Node = (EFI_DEV_PATH *)RemainingDevicePath;
        if ((Node->DevPath.Type != MEDIA_DEVICE_PATH) ||
            (Node->DevPath.SubType != MEDIA_HARDDRIVE_DP)) {

            return EFI_UNSUPPORTED;
        }
    }

    //
    // Try to open the abstractions needed to support the simple file system.
    // Start by opening the disk I/O protocol, the least common.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiDiskIoProtocolGuid,
                             (VOID **)&DiskIo,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (Status == EFI_ALREADY_STARTED) {
        return EFI_SUCCESS;
    }

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCloseProtocol(ControllerHandle,
                     &EfiDiskIoProtocolGuid,
                     This->DriverBindingHandle,
                     ControllerHandle);

    //
    // Also open up the device path protocol.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiDevicePathProtocolGuid,
                             (VOID **)&ParentDevicePath,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (Status == EFI_ALREADY_STARTED) {
        return EFI_SUCCESS;
    }

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCloseProtocol(ControllerHandle,
                     &EfiDevicePathProtocolGuid,
                     This->DriverBindingHandle,
                     ControllerHandle);

    //
    // Open Block I/O.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiBlockIoProtocolGuid,
                             NULL,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_TEST_PROTOCOL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiFatStart (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    )

/*++

Routine Description:

    This routine starts a partition driver on a raw Block I/O device.

Arguments:

    This - Supplies a pointer to the driver binding protocol instance.

    ControllerHandle - Supplies the handle of the controller to start. This
        handle must support a protocol interface that supplies an I/O
        abstraction to the driver.

    RemainingDevicePath - Supplies an optional pointer to the remaining
        portion of a device path.

Return Value:

    EFI_SUCCESS if the device was started.

    EFI_DEVICE_ERROR if the device could not be started due to a device error.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    Other error codes if the driver failed to start the device.

--*/

{

    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_DISK_IO_PROTOCOL *DiskIo;
    BOOLEAN DiskIoOpened;
    EFI_FILE_PROTOCOL *File;
    EFI_TPL OldTpl;
    EFI_STATUS Status;
    PEFI_FAT_VOLUME Volume;

    DiskIoOpened = FALSE;
    Volume = NULL;
    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    if (RemainingDevicePath != NULL) {
        if ((RemainingDevicePath->Type == END_DEVICE_PATH_TYPE) &&
            (RemainingDevicePath->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE)) {

            Status = EFI_SUCCESS;
            goto FatStartEnd;
        }
    }

    //
    // Open up Block I/O.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiBlockIoProtocolGuid,
                             (VOID **)&BlockIo,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(Status)) {
        goto FatStartEnd;
    }

    //
    // Open Disk I/O.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiDiskIoProtocolGuid,
                             (VOID **)&DiskIo,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if ((EFI_ERROR(Status)) && (Status != EFI_ALREADY_STARTED)) {
        goto FatStartEnd;
    }

    DiskIoOpened = TRUE;

    //
    // Create a volume structure.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_FAT_VOLUME),
                             (VOID **)&Volume);

    if (EFI_ERROR(Status)) {
        goto FatStartEnd;
    }

    EfiSetMem(Volume, sizeof(EFI_FAT_VOLUME), 0);
    Volume->Magic = EFI_FAT_VOLUME_MAGIC;
    Volume->Handle = ControllerHandle;
    Volume->DiskIo = DiskIo;
    Volume->BlockIo = BlockIo;
    Volume->SimpleFileSystem.Revision =
                                      EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION;

    Volume->SimpleFileSystem.OpenVolume = EfiFatOpenVolume;

    //
    // Try to open the volume.
    //

    Status = EfiFatOpenVolume(&(Volume->SimpleFileSystem), &File);
    if (EFI_ERROR(Status)) {
        Status = EFI_UNSUPPORTED;
        goto FatStartEnd;
    }

    Status = File->Close(File);
    if (EFI_ERROR(Status)) {
        goto FatStartEnd;
    }

    //
    // Install the simple file system interface, open for business.
    //

    Status = EfiInstallMultipleProtocolInterfaces(
                                              &ControllerHandle,
                                              &EfiSimpleFileSystemProtocolGuid,
                                              &(Volume->SimpleFileSystem),
                                              NULL);

    if (EFI_ERROR(Status)) {
        goto FatStartEnd;
    }

    Status = EFI_SUCCESS;

FatStartEnd:
    if (EFI_ERROR(Status)) {
        if (DiskIoOpened != FALSE) {
            EfiCloseProtocol(ControllerHandle,
                             &EfiDiskIoProtocolGuid,
                             This->DriverBindingHandle,
                             ControllerHandle);
        }

        if (Volume != NULL) {
            EfiFreePool(Volume);
        }
    }

    EfiRestoreTPL(OldTpl);
    return Status;
}

EFIAPI
EFI_STATUS
EfiFatStop (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    UINTN NumberOfChildren,
    EFI_HANDLE *ChildHandleBuffer
    )

/*++

Routine Description:

    This routine stops a disk I/O driver device, stopping any child handles
    created by this driver.

Arguments:

    This - Supplies a pointer to the driver binding protocol instance.

    ControllerHandle - Supplies the handle of the device being stopped. The
        handle must support a bus specific I/O protocol for the driver to use
        to stop the device.

    NumberOfChildren - Supplies the number of child devices in the child handle
        buffer.

    ChildHandleBuffer - Supplies an optional array of child device handles to
        be freed. This can be NULL if the number of children specified is zero.

Return Value:

    EFI_SUCCESS if the device was stopped.

    EFI_DEVICE_ERROR if the device could not be stopped due to a device error.

--*/

{

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    PEFI_FAT_VOLUME Instance;
    EFI_STATUS Status;

    //
    // Get the context back.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiSimpleFileSystemProtocolGuid,
                             (VOID **)&FileSystem,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Instance = EFI_FAT_VOLUME_FROM_THIS(FileSystem);
    if (Instance->OpenFiles != 0) {
        return EFI_DEVICE_ERROR;
    }

    Status = EfiUninstallMultipleProtocolInterfaces(
                                              ControllerHandle,
                                              &EfiSimpleFileSystemProtocolGuid,
                                              &(Instance->SimpleFileSystem),
                                              NULL);

    if (!EFI_ERROR(Status)) {
        if (Instance->FatVolume != NULL) {
            FatUnmount(Instance->FatVolume);
        }

        Status = EfiCloseProtocol(ControllerHandle,
                                  &EfiDiskIoProtocolGuid,
                                  This->DriverBindingHandle,
                                  ControllerHandle);

        ASSERT(!EFI_ERROR(Status));

        EfiFreePool(Instance);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiFatOpenVolume (
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
    EFI_FILE_PROTOCOL **Root
    )

/*++

Routine Description:

    This routine opens the root directory on a volume.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Root - Supplies a pointer where the opened file handle will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the volume does not support the requested file system
    type.

    EFI_NO_MEDIA if the device has no medium.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_ACCESS_DENIED if the service denied access to the file.

    EFI_OUT_OF_RESOURCES if resources could not be allocated.

    EFI_MEDIA_CHANGED if the device has a different medium in it or the medium
    is no longer supported. Any existing file handles for this volume are no
    longer valid. The volume must be reopened.

--*/

{

    BLOCK_DEVICE_PARAMETERS BlockDeviceParameters;
    KSTATUS FatStatus;
    PEFI_FAT_FILE File;
    EFI_TPL OldTpl;
    EFI_STATUS Status;
    PEFI_FAT_VOLUME Volume;

    Status = EFI_UNSUPPORTED;
    if ((This == NULL) || (Root == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    File = NULL;
    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    Volume = EFI_FAT_VOLUME_FROM_THIS(This);

    //
    // If this is the first file being opened and the volume isn't mounted or
    // the media's changed, unmount and remount the volume.
    //

    if ((Volume->OpenFiles == 0) &&
        ((Volume->FatVolume == NULL) ||
         (Volume->MediaId != Volume->BlockIo->Media->MediaId))) {

        if (Volume->FatVolume != NULL) {
            FatUnmount(Volume->FatVolume);
        }

        Volume->FatVolume = NULL;
        Volume->BlockSize = Volume->BlockIo->Media->BlockSize;
        Volume->MediaId = Volume->BlockIo->Media->MediaId;
        EfiSetMem(&BlockDeviceParameters, sizeof(BLOCK_DEVICE_PARAMETERS), 0);
        BlockDeviceParameters.DeviceToken = Volume;
        BlockDeviceParameters.BlockSize = Volume->BlockIo->Media->BlockSize;
        BlockDeviceParameters.BlockCount =
                                         Volume->BlockIo->Media->LastBlock + 1;

        FatStatus = FatMount(&BlockDeviceParameters, 0, &(Volume->FatVolume));
        if (!KSUCCESS(FatStatus)) {
            Status = EFI_UNSUPPORTED;
            goto FatOpenVolumeEnd;
        }
    }

    //
    // If the media appears to have changed, fail.
    //

    if (Volume->MediaId != Volume->BlockIo->Media->MediaId) {
        Status = EFI_MEDIA_CHANGED;
        goto FatOpenVolumeEnd;
    }

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_FAT_FILE),
                             (VOID **)&File);

    if (EFI_ERROR(Status)) {
        goto FatOpenVolumeEnd;
    }

    EfiCopyMem(File, &EfiFatFileTemplate, sizeof(EFI_FAT_FILE));
    File->MediaId = Volume->MediaId;
    File->Volume = Volume;
    FatStatus = FatLookup(Volume->FatVolume,
                          TRUE,
                          0,
                          NULL,
                          0,
                          &(File->Properties));

    if (!KSUCCESS(FatStatus)) {
        Status = EFI_VOLUME_CORRUPTED;
        goto FatOpenVolumeEnd;
    }

    File->IsRoot = TRUE;
    File->IsOpenForRead = TRUE;
    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(""),
                             (VOID **)&(File->FileName));

    if (EFI_ERROR(Status)) {
        goto FatOpenVolumeEnd;
    }

    EfiCopyMem(File->FileName, "", sizeof(""));
    FatStatus = FatOpenFileId(Volume->FatVolume,
                              File->Properties.FileId,
                              IO_ACCESS_READ | IO_ACCESS_WRITE,
                              0,
                              &(File->FatFile));

    Volume->OpenFiles += 1;
    Volume->RootDirectoryId = File->Properties.FileId;
    Status = EFI_SUCCESS;

FatOpenVolumeEnd:
    if (EFI_ERROR(Status)) {
        if (File != NULL) {
            if (File->FatFile != NULL) {
                FatCloseFile(File->FatFile);
            }

            if (File->FileName != NULL) {
                EfiFreePool(File->FileName);
            }

            EfiFreePool(File);
            File = NULL;
        }
    }

    EfiRestoreTPL(OldTpl);
    *Root = &(File->FileProtocol);
    return Status;
}

EFIAPI
EFI_STATUS
EfiFatOpen (
    EFI_FILE_PROTOCOL *This,
    EFI_FILE_PROTOCOL **NewHandle,
    CHAR16 *FileName,
    UINT64 OpenMode,
    UINT64 Attributes
    )

/*++

Routine Description:

    This routine opens a file relative to the source file's location.

Arguments:

    This - Supplies a pointer to the protocol instance.

    NewHandle - Supplies a pointer where the new handl will be returned on
        success.

    FileName - Supplies a pointer to a null-terminated string containing the
        name of the file to open. The file name may contain the path modifiers
        "\", ".", and "..".

    OpenMode - Supplies the open mode of the file. The only valid combinations
        are Read, Read/Write, or Create/Read/Write. See EFI_FILE_MODE_*
        definitions.

    Attributes - Supplies the attributes to create the file with, which are
        only valid if the EFI_FILE_MODE_CREATE flag is set. See EFI_FILE_*
        definitions.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the file could not be found on the device.

    EFI_NO_MEDIA if the device has no medium.

    EFI_MEDIA_CHANGED if the device has a different medium in it or the medium
    is no longer supported.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_ACCESS_DENIED if the service denied access to the file.

    EFI_OUT_OF_RESOURCES if resources could not be allocated.

    EFI_VOLUME_FULL if the volume is full.

--*/

{

    CHAR8 *CurrentPath;
    UINTN CurrentPathLength;
    UINT32 DesiredAccess;
    FILE_ID DirectoryFileId;
    KSTATUS FatStatus;
    PEFI_FAT_FILE File;
    BOOLEAN FileOpened;
    UINT64 NewDirectorySize;
    PEFI_FAT_FILE NewFatFile;
    EFI_FILE_PROTOCOL *NewFile;
    FILE_PROPERTIES NewProperties;
    CHAR8 *OpenedFileName;
    UINTN OpenedFileNameLength;
    CHAR8 *Path;
    FILE_PROPERTIES Properties;
    BOOLEAN StartsAtRoot;
    EFI_STATUS Status;

    DirectoryFileId = 0;
    FileOpened = FALSE;
    NewFatFile = NULL;
    OpenedFileName = NULL;
    if ((This == NULL) || (NewHandle == NULL) || (FileName == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    switch (OpenMode) {
    case EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE:
        if ((Attributes & ~EFI_FILE_VALID_ATTR) != 0) {
            return EFI_INVALID_PARAMETER;
        }

        if ((Attributes & EFI_FILE_READ_ONLY) != 0) {
            return EFI_INVALID_PARAMETER;
        }

        //
        // Fall through.
        //

    case EFI_FILE_MODE_READ:
    case EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE:
        break;

    default:
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    Path = EfipFatCopyPath(FileName, &StartsAtRoot);
    if (Path == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    //
    // The copy path routine returns whether or not the path started with a
    // slash. In addition, if the first component is a "." and the current
    // directory is the root, then set the starting at root flag.
    //

    CurrentPath = Path;
    if ((EfipFatStringCompare(Path, ".") == 0) && (File->IsRoot != FALSE)) {
        StartsAtRoot = TRUE;
        CurrentPath += EfipFatStringLength(Path) + 1;
    }

    CurrentPathLength = EfipFatStringLength(CurrentPath);

    //
    // If the file path starts at the root and this node is not the root, open
    // the root.
    //

    if ((StartsAtRoot != FALSE) && (File->IsRoot == FALSE)) {
        Status = File->Volume->SimpleFileSystem.OpenVolume(
                                             &(File->Volume->SimpleFileSystem),
                                             &NewFile);

        if (EFI_ERROR(Status)) {
            goto FatOpenEnd;
        }

        File = EFI_FAT_FILE_FROM_THIS(NewFile);

        ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

        FileOpened = TRUE;
        NewFile = NULL;
    }

    //
    // Set the starting file to be the current properties.
    //

    EfiCopyMem(&Properties, &(File->Properties), sizeof(FILE_PROPERTIES));
    OpenedFileName = File->FileName;
    OpenedFileNameLength = EfipFatStringLength(OpenedFileName);

    //
    // Loop opening the next component in the path.
    //

    Status = EFI_SUCCESS;
    while (TRUE) {
        if (CurrentPathLength == 0) {
            break;
        }

        DirectoryFileId = Properties.FileId;
        OpenedFileName = CurrentPath;
        OpenedFileNameLength = CurrentPathLength;
        FatStatus = FatLookup(File->Volume->FatVolume,
                              FALSE,
                              Properties.FileId,
                              CurrentPath,
                              CurrentPathLength + 1,
                              &Properties);

        //
        // If the file was not found, stop.
        //

        if ((FatStatus == STATUS_NO_SUCH_FILE) ||
            (FatStatus == STATUS_NOT_FOUND) ||
            (FatStatus == STATUS_PATH_NOT_FOUND)) {

            Status = EFI_NOT_FOUND;
            break;

        //
        // If some wackier error occured, fail the whole function.
        //

        } else if (!KSUCCESS(FatStatus)) {
            Status = EFI_VOLUME_CORRUPTED;
            goto FatOpenEnd;
        }

        //
        // This file was found, move to the next path component.
        //

        CurrentPath += CurrentPathLength + 1;
        CurrentPathLength = EfipFatStringLength(CurrentPath);

        //
        // If the file was not a directory, nothing more can be looked up
        // underneath this, so stop.
        //

        if (Properties.Type != IoObjectRegularDirectory) {
            break;
        }
    }

    ASSERT((Status == EFI_SUCCESS) || (Status == EFI_NOT_FOUND));

    //
    // Okay, either the path ended, the file was not found, or the file was
    // not a directory. If the file was not found, maybe create it.
    //

    if (Status == EFI_NOT_FOUND) {

        //
        // If the file doesn't exist and the caller doesn't want to create it,
        // then return not found.
        //

        if ((OpenMode & EFI_FILE_MODE_CREATE) == 0) {
            goto FatOpenEnd;
        }

        //
        // Fail if the volume or directory is read-only.
        //

        if ((File->Volume->ReadOnly != FALSE) ||
            ((Properties.Permissions & FILE_PERMISSION_USER_WRITE) == 0)) {

            Status = EFI_WRITE_PROTECTED;
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

        if (EfipFatStringLength(CurrentPath + CurrentPathLength + 1) != 0) {
            goto FatOpenEnd;
        }

        //
        // Create the new file or directory.
        //

        EfiCopyMem(&NewProperties, &Properties, sizeof(FILE_PROPERTIES));
        NewProperties.Type = IoObjectRegularFile;
        if ((Attributes & EFI_FILE_DIRECTORY) != 0) {
            NewProperties.Type = IoObjectRegularDirectory;
        }

        NewProperties.Permissions = FILE_PERMISSION_USER_ALL;
        if ((Attributes & EFI_FILE_READ_ONLY) != 0) {
            NewProperties.Permissions &= ~FILE_PERMISSION_USER_WRITE;
        }

        NewProperties.FileId = 0;
        FatGetCurrentSystemTime(&(NewProperties.StatusChangeTime));
        NewProperties.Size = 0;
        OpenedFileName = CurrentPath;
        OpenedFileNameLength = CurrentPathLength;
        FatStatus = FatCreate(File->Volume->FatVolume,
                              Properties.FileId,
                              OpenedFileName,
                              OpenedFileNameLength + 1,
                              &NewDirectorySize,
                              &NewProperties);

        if (!KSUCCESS(FatStatus)) {
            Status = EFI_VOLUME_CORRUPTED;
            goto FatOpenEnd;
        }

        //
        // Update the directory properties, as that new file may have made the
        // directory bigger.
        //

        Properties.Size = NewDirectorySize;
        FatStatus = FatWriteFileProperties(File->Volume->FatVolume,
                                           &Properties,
                                           0);

        if (!KSUCCESS(FatStatus)) {
            Status = EFI_VOLUME_CORRUPTED;
            goto FatOpenEnd;
        }

        //
        // Make it look like this new file was successfully looked up by the
        // above loop.
        //

        CurrentPathLength = 0;
        EfiCopyMem(&Properties, &NewProperties, sizeof(FILE_PROPERTIES));
    }

    //
    // If there are more components to the path, then this lookup failed.
    //

    if (CurrentPathLength != 0) {
        Status = EFI_NOT_FOUND;
        goto FatOpenEnd;
    }

    //
    // Create and initialize the file structure.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_FAT_FILE),
                             (VOID **)&NewFatFile);

    if (EFI_ERROR(Status)) {
        goto FatOpenEnd;
    }

    EfiCopyMem(NewFatFile, File, sizeof(EFI_FAT_FILE));
    EfiCopyMem(&(NewFatFile->Properties), &Properties, sizeof(FILE_PROPERTIES));
    EfiSetMem(&(NewFatFile->SeekInformation), sizeof(FAT_SEEK_INFORMATION), 0);
    NewFatFile->CurrentOffset = 0;
    NewFatFile->IsRoot = FALSE;
    if (Properties.FileId == File->Volume->RootDirectoryId) {
        NewFatFile->IsRoot = TRUE;
    }

    NewFatFile->FatFile = NULL;
    NewFatFile->IsOpenForRead = TRUE;
    DesiredAccess = 0;
    if ((OpenMode & EFI_FILE_MODE_READ) != 0) {
        DesiredAccess |= IO_ACCESS_READ;
    }

    if ((OpenMode & EFI_FILE_MODE_WRITE) != 0) {
        DesiredAccess |= IO_ACCESS_WRITE;
        NewFatFile->IsOpenForRead = FALSE;
    }

    NewFatFile->DirectoryFileId = DirectoryFileId;
    Status = EfiAllocatePool(EfiBootServicesData,
                             OpenedFileNameLength + 1,
                             (VOID **)&(NewFatFile->FileName));

    if (EFI_ERROR(Status)) {
        goto FatOpenEnd;
    }

    EfiCopyMem(NewFatFile->FileName, OpenedFileName, OpenedFileNameLength + 1);
    FatStatus = FatOpenFileId(File->Volume->FatVolume,
                              Properties.FileId,
                              DesiredAccess,
                              0,
                              &(NewFatFile->FatFile));

    if (!KSUCCESS(FatStatus)) {
        Status = EFI_VOLUME_CORRUPTED;
        goto FatOpenEnd;
    }

    NewFatFile->Volume->OpenFiles += 1;
    Status = EFI_SUCCESS;

FatOpenEnd:
    if (Path == NULL) {
        EfiFreePool(Path);
    }

    if (FileOpened != FALSE) {
        File->FileProtocol.Close(&(File->FileProtocol));
    }

    if (EFI_ERROR(Status)) {
        if (NewFatFile != NULL) {
            if (NewFatFile->FatFile != NULL) {
                FatCloseFile(NewFatFile->FatFile);
            }

            if (NewFatFile->FileName != NULL) {
                EfiFreePool(NewFatFile->FileName);
            }

            EfiFreePool(NewFatFile);
            NewFatFile = NULL;
        }
    }

    *NewHandle = NULL;
    if (NewFatFile != NULL) {
        *NewHandle = &(NewFatFile->FileProtocol);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiFatClose (
    EFI_FILE_PROTOCOL *This
    )

/*++

Routine Description:

    This routine closes an open file.

Arguments:

    This - Supplies a pointer to the protocol instance, the handle to close.

Return Value:

    EFI_SUCCESS on success.

--*/

{

    PEFI_FAT_FILE File;
    EFI_TPL OldTpl;

    if (This == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    if (File->FatFile != NULL) {
        FatCloseFile(File->FatFile);
    }

    if (File->IsDirty != FALSE) {
        FatWriteFileProperties(File->Volume->FatVolume, &(File->Properties), 0);
    }

    if (File->FileName != NULL) {
        EfiFreePool(File->FileName);
    }

    File->Magic = 0;

    ASSERT(File->Volume->OpenFiles != 0);

    File->Volume->OpenFiles -= 1;
    EfiFreePool(File);
    EfiRestoreTPL(OldTpl);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiFatDelete (
    EFI_FILE_PROTOCOL *This
    )

/*++

Routine Description:

    This routine deletes an open file handle. This also closes the handle.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_WARN_DELETE_FAILURE if the handle was closed but the file was not
    deleted.

--*/

{

    KSTATUS FatStatus;
    PEFI_FAT_FILE File;
    EFI_TPL OldTpl;
    EFI_STATUS Status;
    BOOL Unlinked;

    if (This == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    Status = EFI_SUCCESS;
    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    FatStatus = FatUnlink(File->Volume->FatVolume,
                          File->DirectoryFileId,
                          File->FileName,
                          EfipFatStringLength(File->FileName) + 1,
                          File->Properties.FileId,
                          &Unlinked);

    if (Unlinked == FALSE) {
        Status = EFI_WARN_DELETE_FAILURE;
    }

    if (KSUCCESS(FatStatus)) {

        ASSERT(File->FatFile != NULL);

        FatDeleteFileBlocks(File->Volume->FatVolume,
                            File->FatFile,
                            File->Properties.FileId,
                            0,
                            FALSE);
    }

    File->FileProtocol.Close(This);
    EfiRestoreTPL(OldTpl);
    return Status;
}

EFIAPI
EFI_STATUS
EfiFatRead (
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine reads data from a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer in bytes. On output, the number of bytes successfully read will
        be returned.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NO_MEDIA if the device has no medium.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or an attempt was made to read from a deleted file.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_BUFFER_TOO_SMALL if the buffer size is too small to read the current
    directory entry. The buffer size will be updated with the needed size.

--*/

{

    UINTN BytesComplete;
    PDIRECTORY_ENTRY DirectoryEntry;
    ULONG ElementsRead;
    KSTATUS FatStatus;
    PEFI_FAT_FILE File;
    EFI_FILE_INFO *FileInformation;
    PFAT_IO_BUFFER IoBuffer;
    EFI_TPL OldTpl;
    EFI_STATUS Status;

    if ((This == NULL) || (BufferSize == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    if ((*BufferSize != 0) && (Buffer == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    BytesComplete = 0;
    DirectoryEntry = NULL;
    FileInformation = NULL;
    IoBuffer = NULL;
    OldTpl = EfiRaiseTPL(TPL_CALLBACK);

    //
    // A directory read returns the files in the directory.
    //

    if (File->Properties.Type == IoObjectRegularDirectory) {
        Status = EfiAllocatePool(EfiBootServicesData,
                                 EFI_FAT_DIRECTORY_ENTRY_SIZE,
                                 (VOID **)&DirectoryEntry);

        if (EFI_ERROR(Status)) {
            goto FatReadEnd;
        }

        IoBuffer = FatCreateIoBuffer(DirectoryEntry,
                                     EFI_FAT_DIRECTORY_ENTRY_SIZE);

        if (IoBuffer == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto FatReadEnd;
        }

        FatStatus = FatEnumerateDirectory(
                               File->FatFile,
                               File->CurrentOffset + DIRECTORY_CONTENTS_OFFSET,
                               IoBuffer,
                               EFI_FAT_DIRECTORY_ENTRY_SIZE,
                               TRUE,
                               TRUE,
                               NULL,
                               &BytesComplete,
                               &ElementsRead);

        if (!KSUCCESS(FatStatus)) {
            Status = EFI_VOLUME_CORRUPTED;
            goto FatReadEnd;
        }

        FileInformation =
                  EfipFatConvertDirectoryEntryToFileInfo(File, DirectoryEntry);

        if (FileInformation == NULL) {
            Status = EFI_VOLUME_CORRUPTED;
            goto FatReadEnd;
        }

        if (*BufferSize < FileInformation->Size) {
            *BufferSize = FileInformation->Size;
            Status = EFI_BUFFER_TOO_SMALL;
            goto FatReadEnd;
        }

        *BufferSize = FileInformation->Size;
        EfiCopyMem(Buffer, FileInformation, *BufferSize);
        File->CurrentOffset += ElementsRead;
        Status = EFI_SUCCESS;

    //
    // Perform a normal file read.
    //

    } else {

        ASSERT(File->Properties.Type == IoObjectRegularFile);

        IoBuffer = FatCreateIoBuffer(Buffer, *BufferSize);
        if (IoBuffer == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }

        BytesComplete = 0;
        FatStatus = FatReadFile(File->FatFile,
                                &(File->SeekInformation),
                                IoBuffer,
                                *BufferSize,
                                0,
                                NULL,
                                &BytesComplete);

        ASSERT(BytesComplete <= *BufferSize);

        File->CurrentOffset += BytesComplete;
        Status = EFI_SUCCESS;
        if (!KSUCCESS(Status)) {
            FatStatus = EFI_VOLUME_CORRUPTED;
        }

        *BufferSize = (UINTN)BytesComplete;
    }

FatReadEnd:
    EfiRestoreTPL(OldTpl);
    if (IoBuffer != NULL) {
        FatFreeIoBuffer(IoBuffer);
    }

    if (DirectoryEntry != NULL) {
        EfiFreePool(DirectoryEntry);
    }

    if (FileInformation != NULL) {
        EfiFreePool(FileInformation);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiFatWrite (
    EFI_FILE_PROTOCOL *This,
    UINTN *BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine writes data to a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    BufferSize - Supplies a pointer that on input contains the size of the
        buffer in bytes. On output, the number of bytes successfully written
        will be returned.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the open handle is a directory.

    EFI_NO_MEDIA if the device has no medium.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or an attempt was made to write to a deleted file.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_WRITE_PROTECTED if the file or medium is write-protected.

    EFI_ACCESS_DENIED if the file was opened read only.

    EFI_VOLUME_FULL if the volume was full.

--*/

{

    UINTN BytesComplete;
    KSTATUS FatStatus;
    PEFI_FAT_FILE File;
    UINT64 FileSize;
    PFAT_IO_BUFFER IoBuffer;
    EFI_TPL OldTpl;
    EFI_STATUS Status;

    if ((This == NULL) || (BufferSize == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    if ((*BufferSize != 0) && (Buffer == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    if ((File->Volume->ReadOnly != FALSE) ||
        ((File->Properties.Permissions & FILE_PERMISSION_USER_WRITE) == 0)) {

        return EFI_WRITE_PROTECTED;
    }

    if (File->Properties.Type != IoObjectRegularFile) {
        return EFI_UNSUPPORTED;
    }

    if (File->IsOpenForRead != FALSE) {
        return EFI_ACCESS_DENIED;
    }

    IoBuffer = FatCreateIoBuffer(Buffer, *BufferSize);
    if (IoBuffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    BytesComplete = 0;
    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    FileSize = File->Properties.Size;
    FatStatus = FatWriteFile(File->FatFile,
                             &(File->SeekInformation),
                             IoBuffer,
                             *BufferSize,
                             0,
                             NULL,
                             &BytesComplete);

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

    EfiRestoreTPL(OldTpl);
    Status = EFI_SUCCESS;
    if (!KSUCCESS(FatStatus)) {
        Status = EFI_VOLUME_CORRUPTED;
    }

    *BufferSize = (UINTN)BytesComplete;
    return Status;
}

EFIAPI
EFI_STATUS
EfiFatSetPosition (
    EFI_FILE_PROTOCOL *This,
    UINT64 Position
    )

/*++

Routine Description:

    This routine sets the file position of an open file handle.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Position - Supplies the new position in bytes to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the open handle is a directory.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or the file was deleted.

--*/

{

    KSTATUS FatStatus;
    PEFI_FAT_FILE File;
    EFI_TPL OldTpl;
    EFI_STATUS Status;

    if (This == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    if (File->Properties.Type == IoObjectRegularDirectory) {
        if (Position != 0) {
            return EFI_UNSUPPORTED;
        }

        File->CurrentOffset = 0;
        return EFI_SUCCESS;
    }

    Status = EFI_SUCCESS;
    OldTpl = EfiRaiseTPL(TPL_CALLBACK);

    //
    // Seek to the end of the file if -1 is passed in.
    //

    if (Position == (UINT64)-1) {
        Position = File->Properties.Size;
    }

    FatStatus = FatFileSeek(File->FatFile,
                            NULL,
                            0,
                            SeekCommandFromBeginning,
                            Position,
                            &(File->SeekInformation));

    if (!KSUCCESS(FatStatus)) {
        Status = EFI_DEVICE_ERROR;

    } else {
        File->CurrentOffset = Position;
    }

    EfiRestoreTPL(OldTpl);
    return Status;
}

EFIAPI
EFI_STATUS
EfiFatGetPosition (
    EFI_FILE_PROTOCOL *This,
    UINT64 *Position
    )

/*++

Routine Description:

    This routine gets the file position for an open file handle.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Position - Supplies a pointer where the position in bytes from the
        beginning of the file is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the open handle is a directory.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request, or the file was deleted.

--*/

{

    PEFI_FAT_FILE File;
    EFI_TPL OldTpl;

    if (This == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    if (File->Properties.Type != IoObjectRegularFile) {
        return EFI_UNSUPPORTED;
    }

    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    *Position = File->CurrentOffset;
    EfiRestoreTPL(OldTpl);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiFatGetInformation (
    EFI_FILE_PROTOCOL *This,
    EFI_GUID *InformationType,
    UINTN *BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine gets information about a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    InformationType - Supplies a pointer to the GUID identifying the
        information being requested.

    BufferSize - Supplies a pointer that on input contains the size of the
        supplied buffer in bytes. On output, the size of the data returned will
        be returned.

    Buffer - Supplies a pointer where the data is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the information type is not known.

    EFI_NO_MEDIA if the device has no media.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_BUFFER_TOO_SMALL if the supplied buffer was not large enough. The size
    needed will be returned in the size parameter.

--*/

{

    PEFI_FAT_FILE File;
    EFI_FILE_INFO *FileInformation;
    EFI_TPL OldTpl;
    EFI_STATUS Status;

    FileInformation = NULL;
    if ((This == NULL) || (InformationType == NULL) || (BufferSize == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    Status = EFI_UNSUPPORTED;
    if (EfipFatCompareGuids(InformationType, &EfiFileInformationGuid) !=
        FALSE) {

        FileInformation = EfipFatConvertFilePropertiesToFileInfo(
                                      &(File->Properties),
                                      File->FileName,
                                      EfipFatStringLength(File->FileName) + 1);

        if (FileInformation == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto FatGetInformationEnd;
        }

        if (*BufferSize < FileInformation->Size) {
            *BufferSize = FileInformation->Size;
            Status = EFI_BUFFER_TOO_SMALL;
            goto FatGetInformationEnd;
        }

        *BufferSize = FileInformation->Size;
        EfiCopyMem(Buffer, FileInformation, *BufferSize);
        Status = EFI_SUCCESS;
    }

FatGetInformationEnd:
    EfiRestoreTPL(OldTpl);
    if (FileInformation != NULL) {
        EfiFreePool(FileInformation);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiFatSetInformation (
    EFI_FILE_PROTOCOL *This,
    EFI_GUID *InformationType,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine sets information about a file.

Arguments:

    This - Supplies a pointer to the protocol instance.

    InformationType - Supplies a pointer to the GUID identifying the
        information being set.

    BufferSize - Supplies the size of the data buffer.

    Buffer - Supplies a pointer to the data, whose type is defined by the
        information type.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the information type is not known.

    EFI_NO_MEDIA if the device has no media.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_WRITE_PROTECTED if the information type is EFI_FILE_INFO_ID,
    EFI_FILE_PROTOCOL_SYSTEM_INFO_ID, or EFI_FILE_SYSTEM_VOLUME_LABEL_ID and
    the media is read-only.

    EFI_ACCESS_DENIED if an attempt is made to change the name of a file to a
    file that already exists, an attempt is made to change the
    EFI_FILE_DIRECTORY attribute, an attempt is made to change the size of a
    directory, or the information type is EFI_FILE_INFO_ID, the file was opened
    read-only, and attempt is being made to modify a field other than Attribute.

    EFI_VOLUME_FULL if the volume is full.

    EFI_BAD_BUFFER_SIZE if the buffer size is smaller than the size required by
    the type.

--*/

{

    PEFI_FAT_FILE File;
    EFI_FILE_INFO *FileInformation;
    EFI_TPL OldTpl;
    EFI_STATUS Status;

    FileInformation = NULL;
    if ((This == NULL) || (InformationType == NULL) || (BufferSize == 0) ||
        (Buffer == NULL)) {

        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    Status = EFI_UNSUPPORTED;
    if (EfipFatCompareGuids(InformationType, &EfiFileInformationGuid) !=
        FALSE) {

        FileInformation = Buffer;
        if ((BufferSize < sizeof(EFI_FILE_INFO)) ||
            (FileInformation->Size < sizeof(EFI_FILE_INFO)) ||
            ((FileInformation->Attribute &= ~EFI_FILE_VALID_ATTR) != 0) ||
            ((sizeof(UINTN) == 4) && (FileInformation->Size >= 0xFFFFFFFF))) {

            Status = EFI_INVALID_PARAMETER;
            goto FatSetInformationEnd;
        }

        //
        // For now, this is not supported.
        //

        Status = EFI_UNSUPPORTED;
    }

FatSetInformationEnd:
    EfiRestoreTPL(OldTpl);
    if (FileInformation != NULL) {
        EfiFreePool(FileInformation);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiFatFlush (
    EFI_FILE_PROTOCOL *This
    )

/*++

Routine Description:

    This routine flushes all modified data associated with a file to a device.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_NO_MEDIA if the device has no media.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_VOLUME_CORRUPTED if the file system structures are corrupted.

    EFI_WRITE_PROTECTED if the file or medium is write-protected.

    EFI_ACCESS_DENIED if the file is opened read-only.

    EFI_VOLUME_FULL if the volume is full.

--*/

{

    PEFI_FAT_FILE File;
    EFI_TPL OldTpl;

    if (This == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    File = EFI_FAT_FILE_FROM_THIS(This);

    ASSERT(File->Magic == EFI_FAT_FILE_MAGIC);

    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    if (File->IsDirty != FALSE) {
        FatWriteFileProperties(File->Volume->FatVolume, &(File->Properties), 0);
        File->IsDirty = FALSE;
    }

    EfiFreePool(File);
    EfiRestoreTPL(OldTpl);
    return EFI_SUCCESS;
}

CHAR8 *
EfipFatCopyPath (
    CHAR16 *InputPath,
    BOOLEAN *StartsAtRoot
    )

/*++

Routine Description:

    This routine creates a copy of the given path, converting it to ASCII and
    separating backslashes with terminators along the way.

Arguments:

    InputPath - Supplies a pointer to the input path.

    StartsAtRoot - Supplies a pointer where a boolean is returned indicating
        if the path starts at the root of the drive.

Return Value:

    Returns a pointer to the separated path, terminated with an additional
    NULL terminator.

    NULL on allocation failure.

--*/

{

    CHAR16 *CurrentInput;
    CHAR8 *CurrentOutput;
    UINTN Length;
    CHAR8 *NewPath;
    EFI_STATUS Status;

    *StartsAtRoot = FALSE;
    while (*InputPath == L'\\') {
        *StartsAtRoot = TRUE;
        InputPath += 1;
    }

    CurrentInput = InputPath;
    Length = 2;
    while (*CurrentInput != L'\0') {
        Length += 1;
        CurrentInput += 1;
    }

    Status = EfiAllocatePool(EfiBootServicesData, Length, (VOID **)&NewPath);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    CurrentInput = InputPath;
    CurrentOutput = NewPath;
    while (*CurrentInput != L'\0') {

        //
        // If it's a backslash, then terminate the current output and get past
        // the backslash (and any additional consecutive ones).
        //

        if (*CurrentInput == L'\\') {
            *CurrentOutput = '\0';
            CurrentOutput += 1;
            while (*CurrentInput == L'\\') {
                CurrentInput += 1;
            }

            continue;
        }

        *CurrentOutput = (CHAR8)*CurrentInput;
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

INTN
EfipFatStringCompare (
    CHAR8 *String1,
    CHAR8 *String2
    )

/*++

Routine Description:

    This routine compares two ASCII strings.

Arguments:

    String1 - Supplies a pointer to the first string to compare.

    String2 - Supplies a pointer to the second string to compare.

Return Value:

    0 if the strings are identical.

    Returns the difference between the first characters that are different if
    the strings are not identical.

--*/

{

    while (TRUE) {
        if (*String1 != *String2) {
            return *String1 - *String2;
        }

        if (*String1 == '\0') {
            break;
        }

        String1 += 1;
        String2 += 1;
    }

    return 0;
}

UINTN
EfipFatStringLength (
    CHAR8 *String
    )

/*++

Routine Description:

    This routine returns the length of an ASCII string.

Arguments:

    String - Supplies a pointer to the string to get the length of.

Return Value:

    Returns the length of the string, not including the null terminator.

--*/

{

    UINTN Length;

    Length = 0;
    while (*String != '\0') {
        Length += 1;
        String += 1;
    }

    return Length;
}

EFI_FILE_INFO *
EfipFatConvertDirectoryEntryToFileInfo (
    PEFI_FAT_FILE File,
    PDIRECTORY_ENTRY DirectoryEntry
    )

/*++

Routine Description:

    This routine converts a directory entry into a file information structure.

Arguments:

    File - Supplies a pointer to the open file.

    DirectoryEntry - Supplies a pointer to the read directory entry.

Return Value:

    Returns the file information structure on success. The caller is
    responsible for freeing this memory from pool.

    NULL on failure.

--*/

{

    KSTATUS FatStatus;
    EFI_FILE_INFO *FileInformation;
    CHAR8 *FileName;
    ULONG NameSize;
    FILE_PROPERTIES Properties;

    FileName = (CHAR8 *)(DirectoryEntry + 1);

    ASSERT(File->Properties.Type == IoObjectRegularDirectory);

    NameSize = DirectoryEntry->Size - sizeof(DIRECTORY_ENTRY);
    FatStatus = FatLookup(File->Volume->FatVolume,
                          FALSE,
                          File->Properties.FileId,
                          FileName,
                          NameSize,
                          &Properties);

    if (!KSUCCESS(FatStatus)) {
        return NULL;
    }

    FileInformation = EfipFatConvertFilePropertiesToFileInfo(&Properties,
                                                             FileName,
                                                             NameSize);

    return FileInformation;
}

EFI_FILE_INFO *
EfipFatConvertFilePropertiesToFileInfo (
    PFILE_PROPERTIES Properties,
    CHAR8 *FileName,
    UINTN FileNameSize
    )

/*++

Routine Description:

    This routine converts a file properties structure into a file information
    structure.

Arguments:

    Properties - Supplies a pointer to the file properties.

    FileName - Supplies a pointer to the file name.

    FileNameSize - Supplies the size of the file name in bytes including the
        null terminator.

Return Value:

    Returns the file information structure on success. The caller is
    responsible for freeing this memory from pool.

    NULL on failure.

--*/

{

    UINTN AllocationSize;
    EFI_FILE_INFO *FileInformation;
    CHAR16 *FileName16;
    EFI_STATUS Status;

    AllocationSize = sizeof(EFI_FILE_INFO) +
                     (FileNameSize * sizeof(CHAR16));

    Status = EfiAllocatePool(EfiBootServicesData,
                             AllocationSize,
                             (VOID **)&FileInformation);

    if (EFI_ERROR(Status)) {
        return NULL;
    }

    EfiSetMem(FileInformation, sizeof(EFI_FILE_INFO), 0);
    FileInformation->Size = AllocationSize;
    if (Properties->Type == IoObjectRegularDirectory) {
        FileInformation->FileSize = EFI_FAT_DIRECTORY_ENTRY_SIZE;

    } else {
        FileInformation->FileSize = Properties->Size;
    }

    FileInformation->PhysicalSize =
                                Properties->BlockCount * Properties->BlockSize;

    if ((Properties->Permissions & FILE_PERMISSION_USER_READ) == 0) {
        FileInformation->Attribute |= EFI_FILE_READ_ONLY;
    }

    if (Properties->Type == IoObjectRegularDirectory) {
        FileInformation->Attribute |= EFI_FILE_DIRECTORY;
    }

    FileName16 = FileInformation->FileName;
    while (TRUE) {
        *FileName16 = *FileName;
        if (*FileName == '\0') {
            break;
        }

        FileName += 1;
        FileName16 += 1;
    }

    return FileInformation;
}

BOOLEAN
EfipFatCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    )

/*++

Routine Description:

    This routine compares two GUIDs.

Arguments:

    FirstGuid - Supplies a pointer to the first GUID.

    SecondGuid - Supplies a pointer to the second GUID.

Return Value:

    TRUE if the GUIDs are equal.

    FALSE if the GUIDs are different.

--*/

{

    UINT32 *FirstPointer;
    UINT32 *SecondPointer;

    //
    // Compare GUIDs 32 bits at a time.
    //

    FirstPointer = (UINT32 *)FirstGuid;
    SecondPointer = (UINT32 *)SecondGuid;
    if ((FirstPointer[0] == SecondPointer[0]) &&
        (FirstPointer[1] == SecondPointer[1]) &&
        (FirstPointer[2] == SecondPointer[2]) &&
        (FirstPointer[3] == SecondPointer[3])) {

        return TRUE;
    }

    return FALSE;
}

