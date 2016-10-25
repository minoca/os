/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    image.c

Abstract:

    This module implements UEFI core image services.

Author:

    Evan Green 10-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "fileinfo.h"
#include "hii.h"
#include "imagep.h"
#include "efiimg.h"
#include "fv2.h"
#include <minoca/uefi/protocol/loadfil.h>
#include <minoca/uefi/protocol/loadfil2.h>
#include <minoca/uefi/protocol/sfilesys.h>
#include <minoca/kernel/hmod.h>
#include <minoca/kernel/kdebug.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipCoreLoadImage (
    BOOLEAN BootPolicy,
    EFI_HANDLE ParentImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *FilePath,
    VOID *SourceBuffer,
    UINTN SourceSize,
    EFI_PHYSICAL_ADDRESS DestinationBuffer,
    UINTN *PageCount,
    EFI_HANDLE *ImageHandle,
    EFI_PHYSICAL_ADDRESS *EntryPoint,
    UINT32 Attributes
    );

EFIAPI
VOID *
EfipCoreGetFileBufferByFilePath (
    BOOLEAN BootPolicy,
    CONST EFI_DEVICE_PATH_PROTOCOL *FilePath,
    CHAR16 **FileName,
    UINTN *FileSize,
    UINT32 *AuthenticationStatus
    );

EFI_STATUS
EfipCoreLoadPeImage (
    BOOLEAN BootPolicy,
    VOID *PeHandle,
    PEFI_IMAGE_DATA Image,
    EFI_PHYSICAL_ADDRESS DestinationBuffer,
    EFI_PHYSICAL_ADDRESS *EntryPoint,
    UINT32 Attribute
    );

VOID
EfipCoreUnloadAndCloseImage (
    PEFI_IMAGE_DATA Image,
    BOOLEAN FreePages
    );

PEFI_IMAGE_DATA
EfipCoreGetImageDataFromHandle (
    EFI_HANDLE ImageHandle
    );

EFIAPI
EFI_STATUS
EfipCoreReadImageFile (
    VOID *FileHandle,
    UINTN FileOffset,
    UINTN *ReadSize,
    VOID *Buffer
    );

CHAR8 *
EfipCoreConvertFileNameToAscii (
    CHAR16 *FileName,
    UINTN *AsciiNameSize
    );

//
// -------------------------------------------------------------------- Globals
//

PEFI_IMAGE_DATA EfiCurrentImage;

EFI_IMAGE_DATA EfiFirmwareLoadedImage = {
    EFI_IMAGE_DATA_MAGIC,
    NULL,
    EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER,
    TRUE,
    (EFI_IMAGE_ENTRY_POINT)EfiCoreMain,
    {
        EFI_LOADED_IMAGE_INFORMATION_REVISION,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        NULL,
        NULL,
        0,
        EfiBootServicesCode,
        EfiBootServicesData
    },
    0,
    0,
    NULL,
    0,
    EFI_SUCCESS,
    0,
    NULL,
    NULL,
    NULL,
    0,
    NULL,
    NULL,
    {0},
    EFI_SUCCESS
};

EFI_GUID EfiLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
EFI_GUID EfiLoadFile2ProtocolGuid = EFI_LOAD_FILE2_PROTOCOL_GUID;
EFI_GUID EfiLoadFileProtocolGuid = EFI_LOAD_FILE_PROTOCOL_GUID;
EFI_GUID EfiLoadedImageDevicePathProtocolGuid =
                                    EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID;

EFI_GUID EfiHiiPackageListProtocolGuid = EFI_HII_PACKAGE_LIST_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreLoadImage (
    BOOLEAN BootPolicy,
    EFI_HANDLE ParentImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    VOID *SourceBuffer,
    UINTN SourceSize,
    EFI_HANDLE *ImageHandle
    )

/*++

Routine Description:

    This routine loads an EFI image into memory.

Arguments:

    BootPolicy - Supplies a boolean indicating that the request originates
        from the boot manager, and that the boot manager is attempting to load
        the given file path as a boot selection. This is ignored if the source
        buffer is NULL.

    ParentImageHandle - Supplies the caller's image handle.

    DevicePath - Supplies a pointer to the device path from which the image is
        loaded.

    SourceBuffer - Supplies an optional pointer to the memory location
        containing a copy of the image to be loaded.

    SourceSize - Supplies the size in bytes of the source buffer.

    ImageHandle - Supplies a pointer where the loaded image handle will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if both the source buffer and device path are NULL.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_UNSUPPORTED if the image type is unsupported.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    EFI_LOAD_ERROR if the image format was corrupt or not understood.

    EFI_DEVICE_ERROR if the underlying device returned a read error.

    EFI_ACCESS_DENIED if the platform policy prohibits the image from being
    loaded.

    EFI_SECURITY_VIOLATION if the image was successfully loaded, but the
    platform policy indicates the image should not be started.

--*/

{

    UINT32 Attributes;
    EFI_STATUS Status;

    Attributes =
               EFI_LOAD_PE_IMAGE_ATTRIBUTE_RUNTIME_REGISTRATION |
               EFI_LOAD_PE_IMAGE_ATTRIBUTE_DEBUG_IMAGE_INFO_TABLE_REGISTRATION;

    Status = EfipCoreLoadImage(BootPolicy,
                               ParentImageHandle,
                               DevicePath,
                               SourceBuffer,
                               SourceSize,
                               (EFI_PHYSICAL_ADDRESS)(UINTN)NULL,
                               NULL,
                               ImageHandle,
                               NULL,
                               Attributes);

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreUnloadImage (
    EFI_HANDLE ImageHandle
    )

/*++

Routine Description:

    This routine unloads an image.

Arguments:

    ImageHandle - Supplies the handle of the image to unload.

    ExitStatus - Supplies the exit code.

    ExitDataSize - Supplies the size of the exit data. This is ignored if the
        exit status code is EFI_SUCCESS.

    ExitData - Supplies an optional pointer where a pointer will be returned
        that includes a null-terminated string describing the reason the
        application exited, optionally followed by additional binary data. This
        buffer must be allocated from AllocatePool.

Return Value:

    EFI_SUCCESS if the image was unloaded.

    EFI_INVALID_PARAMETER if the image handle is not valid.

--*/

{

    PEFI_IMAGE_DATA Image;
    EFI_STATUS Status;

    Image = EfipCoreGetImageDataFromHandle(ImageHandle);
    if (Image == NULL) {
        Status = EFI_INVALID_PARAMETER;
        goto CoreUnloadImageEnd;
    }

    //
    // If the image has been started, request that it unload.
    //

    if (Image->Started != FALSE) {
        Status = EFI_UNSUPPORTED;
        if (Image->Information.Unload != NULL) {
            Status = Image->Information.Unload(ImageHandle);
        }

    //
    // The image has not been started, so unloading it is always okay.
    //

    } else {
        Status = EFI_SUCCESS;
    }

    if (!EFI_ERROR(Status)) {
        EfipCoreUnloadAndCloseImage(Image, TRUE);
    }

CoreUnloadImageEnd:
    return Status;
}

EFI_STATUS
EfiCoreStartImage (
    EFI_HANDLE ImageHandle,
    UINTN *ExitDataSize,
    CHAR16 **ExitData
    )

/*++

Routine Description:

    This routine transfers control to a loaded image's entry point.

Arguments:

    ImageHandle - Supplies the handle of the image to run.

    ExitDataSize - Supplies a pointer to the size, in bytes, of the exit data.

    ExitData - Supplies an optional pointer where a pointer will be returned
        that includes a null-terminated string, optionally followed by
        additional binary data.

Return Value:

    EFI_INVALID_PARAMETER if the image handle is invalid or the image has
    already been started.

    EFI_SECURITY_VIOLATION if the platform policy specifies the image should
    not be started.

    Otherwise, returns the exit code from the image.

--*/

{

    UINT64 HandleDatabaseKey;
    PEFI_IMAGE_DATA Image;
    PEFI_IMAGE_DATA LastImage;
    UINTN SetJumpFlag;
    EFI_STATUS Status;

    Image = EfipCoreGetImageDataFromHandle(ImageHandle);
    if ((Image == NULL) || (Image->Started != FALSE)) {
        return EFI_INVALID_PARAMETER;
    }

    if (EFI_ERROR(Image->LoadImageStatus)) {
        return Image->LoadImageStatus;
    }

    if (!EFI_IMAGE_MACHINE_TYPE_SUPPORTED(Image->Machine)) {
        return EFI_UNSUPPORTED;
    }

    //
    // Push the current start image context, and link the current image to the
    // head. This is the only image that can call exit.
    //

    HandleDatabaseKey = EfipCoreGetHandleDatabaseKey();
    LastImage = EfiCurrentImage;
    EfiCurrentImage = Image;
    Image->Tpl = EfiCurrentTpl;

    //
    // Allocate the jump buffer and set the jump target. This is needed because
    // the caller may call Exit several functions in on the stack and
    // exit needs to get back here.
    //

    Image->JumpBuffer = EfiCoreAllocateBootPool(
                          sizeof(EFI_JUMP_BUFFER) + EFI_JUMP_BUFFER_ALIGNMENT);

    if (Image->JumpBuffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    Image->JumpContext = ALIGN_POINTER(Image->JumpBuffer,
                                       EFI_JUMP_BUFFER_ALIGNMENT);

    SetJumpFlag = EfipArchSetJump(Image->JumpContext);

    //
    // The initial call to set jump always returns zero. Subsequent calls to
    // long jump cause a non-zero value to be returned here. The return
    // value of the set jump function is where exit jumps to.
    //

    if (SetJumpFlag == 0) {
        Image->Started = TRUE;
        Image->Status = Image->EntryPoint(ImageHandle,
                                          Image->Information.SystemTable);

        //
        // If the image returned, call exit for it.
        //

        EfiCoreExit(ImageHandle, Image->Status, 0, NULL);
    }

    //
    // The image has exited. Verify the TPL is the same.
    //

    ASSERT(Image->Tpl == EfiCurrentTpl);

    EfiCoreRestoreTpl(Image->Tpl);
    EfiCoreFreePool(Image->JumpBuffer);
    EfiCurrentImage = LastImage;
    EfipCoreConnectHandlesByKey(HandleDatabaseKey);

    //
    // Return the exit data to the caller, or discard it.
    //

    if ((ExitData != NULL) && (ExitDataSize != NULL)) {
        *ExitDataSize = Image->ExitDataSize;
        *ExitData = Image->ExitData;

    } else {
        EfiCoreFreePool(Image->ExitData);
        Image->ExitData = NULL;
        Image->ExitDataSize = 0;
    }

    //
    // If the image returned an error or the image is an application, unload it.
    //

    Status = Image->Status;
    if ((EFI_ERROR(Image->Status)) ||
        (Image->Type == EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION)) {

        EfipCoreUnloadAndCloseImage(Image, TRUE);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreExit (
    EFI_HANDLE ImageHandle,
    EFI_STATUS ExitStatus,
    UINTN ExitDataSize,
    CHAR16 *ExitData
    )

/*++

Routine Description:

    This routine terminates an loaded EFI image and returns control to boot
    services.

Arguments:

    ImageHandle - Supplies the handle of the image passed upon entry.

    ExitStatus - Supplies the exit code.

    ExitDataSize - Supplies the size of the exit data. This is ignored if the
        exit status code is EFI_SUCCESS.

    ExitData - Supplies an optional pointer where a pointer will be returned
        that includes a null-terminated string describing the reason the
        application exited, optionally followed by additional binary data. This
        buffer must be allocated from AllocatePool.

Return Value:

    EFI_SUCCESS if the image was unloaded.

    EFI_INVALID_PARAMETER if the image has been loaded and started with
    LoadImage and StartImage, but the image is not currently executing.

--*/

{

    PEFI_IMAGE_DATA Image;
    EFI_TPL OldTpl;

    //
    // Prevent possible reentrance to this function for the same image handle.
    //

    OldTpl = EfiCoreRaiseTpl(TPL_NOTIFY);
    Image = EfipCoreGetImageDataFromHandle(ImageHandle);
    if (Image == NULL) {
        ExitStatus = EFI_INVALID_PARAMETER;
        goto CoreExitEnd;
    }

    //
    // If the image has not yet been started, just free its resources.
    //

    if (Image->Started == FALSE) {
        EfipCoreUnloadAndCloseImage(Image, TRUE);
        ExitStatus = EFI_SUCCESS;
        goto CoreExitEnd;
    }

    //
    // If the image has been started, verify it can exit.
    //

    if (Image != EfiCurrentImage) {
        RtlDebugPrint("Error: Image cannot exit while in the middle of "
                      "starting another image.\n");

        ExitStatus = EFI_INVALID_PARAMETER;
        goto CoreExitEnd;
    }

    Image->Status = ExitStatus;
    if (ExitData != NULL) {
        Image->ExitDataSize = ExitDataSize;
        Image->ExitData = EfiCoreAllocateBootPool(Image->ExitDataSize);
        if (Image->ExitData == NULL) {
            ExitStatus = EFI_OUT_OF_RESOURCES;
            goto CoreExitEnd;
        }

        EfiCoreCopyMemory(Image->ExitData, ExitData, Image->ExitDataSize);
    }

    EfiCoreRestoreTpl(OldTpl);

    //
    // Return to the set jump in start image.
    //

    EfipArchLongJump(Image->JumpContext, -1);

    //
    // There should be no way to return from a long jump.
    //

    ASSERT(FALSE);

    ExitStatus = EFI_ACCESS_DENIED;

CoreExitEnd:

    //
    // Something bizarre happened, return from the exit.
    //

    EfiCoreRestoreTpl(OldTpl);
    return ExitStatus;
}

EFI_STATUS
EfiCoreInitializeImageServices (
    VOID *FirmwareBaseAddress,
    VOID *FirmwareLowestAddress,
    UINTN FirmwareSize
    )

/*++

Routine Description:

    This routine initializes image service support for the UEFI core.

Arguments:

    FirmwareBaseAddress - Supplies the base address where the firmware was
        loaded into memory. Supply -1 to indicate that the image is loaded at
        its preferred base address and was not relocated.

    FirmwareLowestAddress - Supplies the lowest address where the firmware was
        loaded into memory.

    FirmwareSize - Supplies the size of the firmware image in memory, in bytes.

Return Value:

    EFI Status code.

--*/

{

    PEFI_IMAGE_DATA Image;
    EFI_STATUS Status;

    //
    // Initialize the firmware image data.
    //

    Image = &EfiFirmwareLoadedImage;
    Image->ImageBasePage = (EFI_PHYSICAL_ADDRESS)(UINTN)FirmwareLowestAddress;
    Image->ImagePageCount = EFI_SIZE_TO_PAGES(FirmwareSize);
    Image->Tpl = EfiCurrentTpl;
    Image->Information.SystemTable = EfiSystemTable;
    Image->Information.ImageBase = FirmwareLowestAddress;
    Image->Information.ImageSize = FirmwareSize;

    //
    // Install the loaded image protocol on a new handle representing the
    // firmware image.
    //

    Image->Handle = NULL;
    Status = EfiCoreInstallProtocolInterface(&(Image->Handle),
                                             &EfiLoadedImageProtocolGuid,
                                             EFI_NATIVE_INTERFACE,
                                             &(Image->Information));

    if (EFI_ERROR(Status)) {

        ASSERT(FALSE);

        return Status;
    }

    EfiFirmwareImageHandle = Image->Handle;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipCoreLoadImage (
    BOOLEAN BootPolicy,
    EFI_HANDLE ParentImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *FilePath,
    VOID *SourceBuffer,
    UINTN SourceSize,
    EFI_PHYSICAL_ADDRESS DestinationBuffer,
    UINTN *PageCount,
    EFI_HANDLE *ImageHandle,
    EFI_PHYSICAL_ADDRESS *EntryPoint,
    UINT32 Attributes
    )

/*++

Routine Description:

    This routine loads an EFI image into memory.

Arguments:

    BootPolicy - Supplies a boolean indicating that the request originates
        from the boot manager, and that the boot manager is attempting to load
        the given file path as a boot selection. This is ignored if the source
        buffer is NULL.

    ParentImageHandle - Supplies the caller's image handle.

    FilePath - Supplies a pointer to the device path from which the image is
        loaded.

    SourceBuffer - Supplies an optional pointer to the memory location
        containing a copy of the image to be loaded.

    SourceSize - Supplies the size in bytes of the source buffer.

    DestinationBuffer - Supplies an optional address to load the image at.

    PageCount - Supplies a pointer that on input contains the size of the
        destination buffer in pages. On output, will contain the number of
        pages in the loaded image.

    ImageHandle - Supplies a pointer where the loaded image handle will be
        returned on success.

    EntryPoint - Supplies a pointer where the image entry point will be
        returned.

    Attributes - Supplies a bitfield of flags governing the behavior of the
        load. See EFI_LOAD_PE_IMAGE_ATTRIBUTE_* definitions.

Return Value:

    EFI Status code.

--*/

{

    UINTN AllocationSize;
    CHAR8 *AsciiFileName;
    UINTN AsciiFileNameSize;
    UINT32 AuthenticationStatus;
    PDEBUG_MODULE DebuggerModule;
    EFI_HANDLE DeviceHandle;
    EFI_IMAGE_FILE_HANDLE FileHandle;
    CHAR16 *FileName;
    UINTN FilePathSize;
    BOOLEAN FreePage;
    EFI_DEVICE_PATH_PROTOCOL *HandleFilePath;
    PEFI_IMAGE_DATA Image;
    EFI_DEVICE_PATH_PROTOCOL *OriginalFilePath;
    PEFI_IMAGE_DATA ParentImage;
    EFI_STATUS Status;

    ASSERT(EfiCurrentTpl < TPL_NOTIFY);

    ParentImage = NULL;
    if ((ImageHandle == NULL) || (ParentImageHandle == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    ParentImage = EfipCoreGetImageDataFromHandle(ParentImageHandle);
    if (ParentImage == NULL) {
        RtlDebugPrint("LoadImage: Invalid Parent image handle.\n");
        return EFI_INVALID_PARAMETER;
    }

    EfiCoreSetMemory(&FileHandle, 0, sizeof(EFI_IMAGE_FILE_HANDLE));
    FileHandle.Magic = EFI_IMAGE_FILE_HANDLE_MAGIC;
    FileName = NULL;
    OriginalFilePath = FilePath;
    HandleFilePath = FilePath;
    DeviceHandle = NULL;
    Status = EFI_SUCCESS;

    //
    // If the caller passed a copy of the file, then just use it.
    //

    if (SourceBuffer != NULL) {
        FileHandle.Source = SourceBuffer;
        FileHandle.SourceSize = SourceSize;
        Status = EfiCoreLocateDevicePath(&EfiDevicePathProtocolGuid,
                                         &HandleFilePath,
                                         &DeviceHandle);

        if (EFI_ERROR(Status)) {
            DeviceHandle = NULL;
        }

        if (SourceSize > 0) {
            Status = EFI_SUCCESS;

        } else {
            Status = EFI_LOAD_ERROR;
        }

    //
    // An image source was not supplied, go find it.
    //

    } else {
        if (FilePath == NULL) {
            return EFI_INVALID_PARAMETER;
        }

        FileHandle.Source = EfipCoreGetFileBufferByFilePath(
                                                      BootPolicy,
                                                      FilePath,
                                                      &FileName,
                                                      &(FileHandle.SourceSize),
                                                      &AuthenticationStatus);

        if (FileHandle.Source == NULL) {
            Status = EFI_NOT_FOUND;

        } else {
            FileHandle.FreeBuffer = TRUE;
            Status = EfiCoreLocateDevicePath(&EfiFirmwareVolume2ProtocolGuid,
                                             &HandleFilePath,
                                             &DeviceHandle);

            if (EFI_ERROR(Status)) {
                HandleFilePath = FilePath;
                Status = EfiCoreLocateDevicePath(
                                              &EfiSimpleFileSystemProtocolGuid,
                                              &HandleFilePath,
                                              &DeviceHandle);

                if (EFI_ERROR(Status)) {
                    if (BootPolicy == FALSE) {
                        Status = EfiCoreLocateDevicePath(
                                                     &EfiLoadFile2ProtocolGuid,
                                                     &HandleFilePath,
                                                     &DeviceHandle);
                    }

                    if (EFI_ERROR(Status)) {
                        HandleFilePath = FilePath;
                        Status = EfiCoreLocateDevicePath(
                                                      &EfiLoadFileProtocolGuid,
                                                      &HandleFilePath,
                                                      &DeviceHandle);
                    }
                }
            }
        }
    }

    if (EFI_ERROR(Status)) {
        Image = NULL;
        goto CoreLoadImageEnd;
    }

    //
    // Allocate a new image data structure.
    //

    Image = EfiCoreAllocateBootPool(sizeof(EFI_IMAGE_DATA));
    if (Image == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CoreLoadImageEnd;
    }

    EfiCoreSetMemory(Image, sizeof(EFI_IMAGE_DATA), 0);

    //
    // Pull out just the file portion of the device path for the loaded image
    // file path.
    //

    FilePath = OriginalFilePath;
    if (DeviceHandle != NULL) {
        Status = EfiCoreHandleProtocol(DeviceHandle,
                                       &EfiDevicePathProtocolGuid,
                                       (VOID **)&HandleFilePath);

        if (!EFI_ERROR(Status)) {
            FilePathSize = EfiCoreGetDevicePathSize(HandleFilePath) -
                           sizeof(EFI_DEVICE_PATH_PROTOCOL);

            FilePath = (EFI_DEVICE_PATH_PROTOCOL *)(((UINT8 *)FilePath) +
                                                    FilePathSize);
        }
    }

    Image->Magic = EFI_IMAGE_DATA_MAGIC;
    Image->Information.SystemTable = EfiSystemTable;
    Image->Information.DeviceHandle = DeviceHandle;
    Image->Information.Revision = EFI_LOADED_IMAGE_PROTOCOL_REVISION;
    Image->Information.FilePath = EfiCoreDuplicateDevicePath(FilePath);
    Image->Information.ParentHandle = ParentImageHandle;
    if (PageCount != NULL) {
        Image->ImagePageCount = *PageCount;

    } else {
        Image->ImagePageCount = 0;
    }

    //
    // Install the protocol interfaces for this image, but don't fire the
    // notifications just yet.
    //

    Status = EfipCoreInstallProtocolInterfaceNotify(&(Image->Handle),
                                                    &EfiLoadedImageProtocolGuid,
                                                    EFI_NATIVE_INTERFACE,
                                                    &(Image->Information),
                                                    FALSE);

    if (EFI_ERROR(Status)) {
        goto CoreLoadImageEnd;
    }

    //
    // Load up the image.
    //

    Status = EfipCoreLoadPeImage(BootPolicy,
                                 &FileHandle,
                                 Image,
                                 DestinationBuffer,
                                 EntryPoint,
                                 Attributes);

    if (EFI_ERROR(Status)) {
        if ((Status == EFI_BUFFER_TOO_SMALL) ||
            (Status == EFI_OUT_OF_RESOURCES)) {

            if (PageCount != NULL) {
                *PageCount = Image->ImagePageCount;
            }
        }

        goto CoreLoadImageEnd;
    }

    if (PageCount != NULL) {
        *PageCount = Image->ImagePageCount;
    }

    //
    // Register the image with the debugger unless asked not to.
    //

    if (((Attributes &
          EFI_LOAD_PE_IMAGE_ATTRIBUTE_DEBUG_IMAGE_INFO_TABLE_REGISTRATION) !=
         0) &&
        (FileName != NULL)) {

        AsciiFileName = EfipCoreConvertFileNameToAscii(FileName,
                                                       &AsciiFileNameSize);

        if (AsciiFileName != NULL) {
            AllocationSize = sizeof(DEBUG_MODULE) + AsciiFileNameSize;
            DebuggerModule = EfiCoreAllocateBootPool(AllocationSize);
            if (DebuggerModule != NULL) {
                EfiSetMem(DebuggerModule, AllocationSize, 0);
                DebuggerModule->StructureSize = AllocationSize;
                DebuggerModule->LowestAddress =
                                            (PVOID)(UINTN)Image->ImageBasePage;

                DebuggerModule->Size = Image->ImagePageCount << EFI_PAGE_SHIFT;
                DebuggerModule->EntryPoint = Image->EntryPoint;
                RtlStringCopy(DebuggerModule->BinaryName,
                              AsciiFileName,
                              AsciiFileNameSize);

                Image->DebuggerData = DebuggerModule;
                KdReportModuleChange(DebuggerModule, TRUE);
            }

            EfiCoreFreePool(AsciiFileName);
        }
    }

    //
    // Reinstall the loaded image protocol to fire any notifications.
    //

    Status = EfiCoreReinstallProtocolInterface(Image->Handle,
                                               &EfiLoadedImageProtocolGuid,
                                               &(Image->Information),
                                               &(Image->Information));

    if (EFI_ERROR(Status)) {
        goto CoreLoadImageEnd;
    }

    //
    // If the device path parameter is not NULL, make a copy of the device
    // path. Otherwise the loaded image device path protocol is installed with
    // a NULL interface pointer.
    //

    if (OriginalFilePath != NULL) {
        Image->LoadedImageDevicePath =
                                  EfiCoreDuplicateDevicePath(OriginalFilePath);
    }

    //
    // Install the loaded image device path protocol.
    //

    Status = EfiCoreInstallProtocolInterface(
                                         &(Image->Handle),
                                         &EfiLoadedImageDevicePathProtocolGuid,
                                         EFI_NATIVE_INTERFACE,
                                         Image->LoadedImageDevicePath);

    if (EFI_ERROR(Status)) {
        goto CoreLoadImageEnd;
    }

    //
    // Install the HII package list protocol onto the image handle.
    //

    if (Image->ImageContext.HiiResourceData != 0) {
        Status = EfiCoreInstallProtocolInterface(
                           &(Image->Handle),
                           &EfiHiiPackageListProtocolGuid,
                           EFI_NATIVE_INTERFACE,
                           (VOID *)(UINTN)Image->ImageContext.HiiResourceData);

        if (EFI_ERROR(Status)) {
            goto CoreLoadImageEnd;
        }
    }

    *ImageHandle = Image->Handle;

CoreLoadImageEnd:
    if (FileHandle.FreeBuffer != FALSE) {
        EfiCoreFreePool(FileHandle.Source);
    }

    if (FileName != NULL) {
        EfiCoreFreePool(FileName);
    }

    if (EFI_ERROR(Status)) {
        if (Image != NULL) {
            FreePage = FALSE;
            if (DestinationBuffer == 0) {
                FreePage = TRUE;
            }

            EfipCoreUnloadAndCloseImage(Image, FreePage);
            Image = NULL;
        }
    }

    //
    // Track the return status from this call.
    //

    if (Image != NULL) {
        Image->LoadImageStatus = Status;
    }

    return Status;
}

EFIAPI
VOID *
EfipCoreGetFileBufferByFilePath (
    BOOLEAN BootPolicy,
    CONST EFI_DEVICE_PATH_PROTOCOL *FilePath,
    CHAR16 **FileName,
    UINTN *FileSize,
    UINT32 *AuthenticationStatus
    )

/*++

Routine Description:

    This routine loads a file either from a firmware image, file system
    interface, or from the load file interface.

Arguments:

    BootPolicy - Supplies the boot policy. if TRUE, indicates that the request
        originates from a boot manager trying to make a boot selection. If
        FALSE, the file path must match exactly with the file to be loaded.

    FilePath - Supplies a pointer to the device path of the file to load.

    FileName - Supplies a pointer where a pointer to the file name will be
        returned. The caller is responsible for freeing this buffer.

    FileSize - Supplies a pointer where the size of the loaded file buffer will
        be returned on success.

    AuthenticationStatus - Supplies a pointer to the authentication status.

Return Value:

    Returns a pointer to the image contents. The caller is responsible for
    freeing this memory from pool.

--*/

{

    EFI_FV_FILE_ATTRIBUTES Attributes;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathNode;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathNodeCopy;
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO *FileInformation;
    UINTN FileInformationSize;
    UINTN FileNameSize;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FirmwareVolume;
    EFI_HANDLE Handle;
    UINT8 *ImageBuffer;
    UINTN ImageBufferSize;
    EFI_FILE_HANDLE LastHandle;
    EFI_LOAD_FILE_PROTOCOL *LoadFile;
    EFI_LOAD_FILE2_PROTOCOL *LoadFile2;
    EFI_GUID *NameGuid;
    EFI_DEVICE_PATH_PROTOCOL *OriginalDevicePathNode;
    EFI_SECTION_TYPE SectionType;
    EFI_STATUS Status;
    EFI_FV_FILETYPE Type;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Volume;

    if ((FilePath == NULL) || (FileSize == NULL) ||
        (AuthenticationStatus == NULL)) {

        return NULL;
    }

    DevicePathNodeCopy = NULL;
    NameGuid = NULL;
    FileInformation = NULL;
    FileHandle = NULL;
    ImageBuffer = NULL;
    ImageBufferSize = 0;
    *AuthenticationStatus = 0;
    *FileName = NULL;
    OriginalDevicePathNode = EfiCoreDuplicateDevicePath(FilePath);
    if (OriginalDevicePathNode == NULL) {
        return NULL;
    }

    //
    // See if the device path supports the Firmware Volume 2 protocol.
    //

    DevicePathNode = OriginalDevicePathNode;
    Status = EfiLocateDevicePath(&EfiFirmwareVolume2ProtocolGuid,
                                 &DevicePathNode,
                                 &Handle);

    if (!EFI_ERROR(Status)) {
        NameGuid = EfiCoreGetNameGuidFromFirmwareVolumeDevicePathNode(
                          (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)DevicePathNode);

        if (NameGuid == NULL) {
            Status = EFI_INVALID_PARAMETER;

        } else {

            //
            // Read the image from the firmware file.
            //

            Status = EfiHandleProtocol(Handle,
                                       &EfiFirmwareVolume2ProtocolGuid,
                                       (VOID **)&FirmwareVolume);

            if (!EFI_ERROR(Status)) {
                SectionType = EFI_SECTION_PE32;
                ImageBuffer = NULL;
                Status = FirmwareVolume->ReadSection(FirmwareVolume,
                                                     NameGuid,
                                                     SectionType,
                                                     0,
                                                     (VOID **)&ImageBuffer,
                                                     &ImageBufferSize,
                                                     AuthenticationStatus);

                //
                // If that succeeded, try to read the UI description out as
                // well.
                //

                if (!EFI_ERROR(Status)) {
                    FirmwareVolume->ReadSection(FirmwareVolume,
                                                NameGuid,
                                                EFI_SECTION_USER_INTERFACE,
                                                0,
                                                (VOID **)FileName,
                                                &FileNameSize,
                                                AuthenticationStatus);

                    //
                    // Null terminate the string just to be safe.
                    //

                    if (FileNameSize / sizeof(CHAR16) != 0) {
                        (*FileName)[(FileNameSize / sizeof(CHAR16)) - 1] =
                                                                         L'\0';
                    }

                //
                // If reading the PE32 section failed, try a raw file type.
                //

                } else {
                    if (ImageBuffer != NULL) {
                        EfiCoreFreePool(ImageBuffer);
                        *AuthenticationStatus = 0;
                        ImageBuffer = NULL;
                    }

                    Status = FirmwareVolume->ReadFile(FirmwareVolume,
                                                      NameGuid,
                                                      (VOID **)&ImageBuffer,
                                                      &ImageBufferSize,
                                                      &Type,
                                                      &Attributes,
                                                      AuthenticationStatus);

                }
            }
        }

        if (!EFI_ERROR(Status)) {
            goto CoreGetFileBufferByFilePathEnd;
        }
    }

    //
    // Try to access the file via a file system interface.
    //

    DevicePathNode = OriginalDevicePathNode;
    Status = EfiLocateDevicePath(&EfiSimpleFileSystemProtocolGuid,
                                 &DevicePathNode,
                                 &Handle);

    if (!EFI_ERROR(Status)) {
        Status = EfiHandleProtocol(Handle,
                                   &EfiSimpleFileSystemProtocolGuid,
                                   (VOID **)&Volume);

        if (!EFI_ERROR(Status)) {

            //
            // Open the volume to get the file system handle.
            //

            Status = Volume->OpenVolume(Volume, &FileHandle);
            if (!EFI_ERROR(Status)) {

                //
                // Duplicate the device path to avoid access to an unaligned
                // device path node.
                //

                DevicePathNodeCopy = EfiCoreDuplicateDevicePath(DevicePathNode);
                if (DevicePathNodeCopy == NULL) {
                    FileHandle->Close(FileHandle);
                    Status = EFI_OUT_OF_RESOURCES;
                }

                DevicePathNode = DevicePathNodeCopy;
                while ((!EFI_ERROR(Status)) &&
                       (EfiCoreIsDevicePathEnd(DevicePathNode) == FALSE)) {

                    if ((EfiCoreGetDevicePathType(DevicePathNode) !=
                         MEDIA_DEVICE_PATH) ||
                        (EfiCoreGetDevicePathSubType(DevicePathNode) !=
                         MEDIA_FILEPATH_DP)) {

                        Status = EFI_UNSUPPORTED;
                        break;
                    }

                    LastHandle = FileHandle;
                    FileHandle = NULL;
                    Status = LastHandle->Open(
                            LastHandle,
                            &FileHandle,
                            ((FILEPATH_DEVICE_PATH *)DevicePathNode)->PathName,
                            EFI_FILE_MODE_READ,
                            0);

                    LastHandle->Close(LastHandle);
                    DevicePathNode =
                                  EfiCoreGetNextDevicePathNode(DevicePathNode);
                }

                //
                // If no error occurred, then the file was found. Load the
                // file.
                //

                if (!EFI_ERROR(Status)) {
                    FileInformation = NULL;
                    FileInformationSize = 0;
                    Status = FileHandle->GetInfo(FileHandle,
                                                 &EfiFileInformationGuid,
                                                 &FileInformationSize,
                                                 FileInformation);

                    if (Status == EFI_BUFFER_TOO_SMALL) {
                        FileInformation =
                                  EfiCoreAllocateBootPool(FileInformationSize);

                        if (FileInformation == NULL) {
                            Status = EFI_OUT_OF_RESOURCES;

                        } else {
                            Status = FileHandle->GetInfo(
                                                       FileHandle,
                                                       &EfiFileInformationGuid,
                                                       &FileInformationSize,
                                                       FileInformation);
                        }
                    }

                    if ((!EFI_ERROR(Status)) && (FileInformation != NULL)) {

                        //
                        // Fail if it's a directory.
                        //

                        if ((FileInformation->Attribute & EFI_FILE_DIRECTORY) !=
                            0) {

                            Status = EFI_LOAD_ERROR;

                        //
                        // Allocate space for the file and read it in.
                        //

                        } else {
                            ImageBuffer = EfiCoreAllocateBootPool(
                                           (UINTN)(FileInformation->FileSize));

                            if (ImageBuffer == NULL) {
                                Status = EFI_OUT_OF_RESOURCES;

                            } else {
                                ImageBufferSize = FileInformation->FileSize;
                                Status = FileHandle->Read(FileHandle,
                                                          &ImageBufferSize,
                                                          ImageBuffer);

                                if (!EFI_ERROR(Status)) {

                                    //
                                    // Also read in the file name.
                                    //

                                    FileNameSize = EfiCoreStringLength(
                                                    FileInformation->FileName);

                                    FileNameSize = (FileNameSize + 1) *
                                                   sizeof(CHAR16);

                                    *FileName = EfiCoreAllocateBootPool(
                                                                 FileNameSize);

                                    if (*FileName != NULL) {
                                        EfiCopyMem(*FileName,
                                                   FileInformation->FileName,
                                                   FileNameSize);
                                    }
                                }
                            }
                        }
                    }
                }

                if (FileInformation != NULL) {
                    EfiCoreFreePool(FileInformation);
                }

                if (FileHandle != NULL) {
                    FileHandle->Close(FileHandle);
                }

                if (DevicePathNodeCopy != NULL) {
                    EfiCoreFreePool(DevicePathNodeCopy);
                }
            }
        }

        if (!EFI_ERROR(Status)) {
            goto CoreGetFileBufferByFilePathEnd;
        }
    }

    //
    // Attempt to access the file using the Load File 2 protocol.
    //

    if (BootPolicy == FALSE) {
        DevicePathNode = OriginalDevicePathNode;
        Status = EfiLocateDevicePath(&EfiLoadFile2ProtocolGuid,
                                     &DevicePathNode,
                                     &Handle);

        if (!EFI_ERROR(Status)) {
            Status = EfiHandleProtocol(Handle,
                                       &EfiLoadFile2ProtocolGuid,
                                       (VOID **)&LoadFile2);

            if (!EFI_ERROR(Status)) {

                //
                // Call once to figure out the buffer size.
                //

                ImageBufferSize = 0;
                ImageBuffer = NULL;
                Status = LoadFile2->LoadFile(LoadFile2,
                                             DevicePathNode,
                                             FALSE,
                                             &ImageBufferSize,
                                             ImageBuffer);

                if (Status == EFI_BUFFER_TOO_SMALL) {
                    ImageBuffer = EfiCoreAllocateBootPool(ImageBufferSize);
                    if (ImageBuffer == NULL) {
                        Status = EFI_OUT_OF_RESOURCES;

                    } else {
                        Status = LoadFile2->LoadFile(LoadFile2,
                                                     DevicePathNode,
                                                     FALSE,
                                                     &ImageBufferSize,
                                                     ImageBuffer);
                    }
                }
            }
        }

        if (!EFI_ERROR(Status)) {
            goto CoreGetFileBufferByFilePathEnd;
        }
    }

    //
    // Attempt to access the file using the Load File protocol.
    //

    if (BootPolicy == FALSE) {
        DevicePathNode = OriginalDevicePathNode;
        Status = EfiLocateDevicePath(&EfiLoadFileProtocolGuid,
                                     &DevicePathNode,
                                     &Handle);

        if (!EFI_ERROR(Status)) {
            Status = EfiHandleProtocol(Handle,
                                       &EfiLoadFileProtocolGuid,
                                       (VOID **)&LoadFile);

            if (!EFI_ERROR(Status)) {

                //
                // Call once to figure out the buffer size.
                //

                ImageBufferSize = 0;
                ImageBuffer = NULL;
                Status = LoadFile->LoadFile(LoadFile,
                                            DevicePathNode,
                                            FALSE,
                                            &ImageBufferSize,
                                            ImageBuffer);

                if (Status == EFI_BUFFER_TOO_SMALL) {
                    ImageBuffer = EfiCoreAllocateBootPool(ImageBufferSize);
                    if (ImageBuffer == NULL) {
                        Status = EFI_OUT_OF_RESOURCES;

                    } else {
                        Status = LoadFile->LoadFile(LoadFile,
                                                    DevicePathNode,
                                                    FALSE,
                                                    &ImageBufferSize,
                                                    ImageBuffer);
                    }
                }
            }
        }

        if (!EFI_ERROR(Status)) {
            goto CoreGetFileBufferByFilePathEnd;
        }
    }

CoreGetFileBufferByFilePathEnd:
    if (EFI_ERROR(Status)) {
        if (ImageBuffer != NULL) {
            EfiCoreFreePool(ImageBuffer);
            ImageBuffer = NULL;
        }

        ImageBufferSize = 0;
    }

    *FileSize = ImageBufferSize;
    EfiCoreFreePool(OriginalDevicePathNode);
    return ImageBuffer;
}

EFI_STATUS
EfipCoreLoadPeImage (
    BOOLEAN BootPolicy,
    VOID *PeHandle,
    PEFI_IMAGE_DATA Image,
    EFI_PHYSICAL_ADDRESS DestinationBuffer,
    EFI_PHYSICAL_ADDRESS *EntryPoint,
    UINT32 Attribute
    )

/*++

Routine Description:

    This routine loads, relocates, and invokes a PE/COFF image.

Arguments:

    BootPolicy - Supplies the boot policy. if TRUE, indicates that the request
        originates from a boot manager trying to make a boot selection. If
        FALSE, the file path must match exactly with the file to be loaded.

    PeHandle - Supplies the handle of the PE image.

    Image - Supplies the PE image to be loaded.

    DestinationBuffer - Supplies the buffer to store the image at.

    EntryPoint - Supplies a pointer where the entry point of the image will
        be returned.

    Attribute - Supplies the bitmask of attributes governing the load process.

Return Value:

    EFI_SUCCESS if the image was loaded, relocated, and invoked.

    EFI_OUT_OF_RESOURCES on allocation failure.

    EFI_INVALID_PARAMETER on invalid parameter.

    EFI_BUFFER_TOO_SMALL if the buffer for the image is too small.

--*/

{

    BOOLEAN DestinationAllocated;
    UINTN NeededPages;
    UINTN Size;
    EFI_STATUS Status;

    EfiCoreSetMemory(&(Image->ImageContext), sizeof(Image->ImageContext), 0);
    Image->ImageContext.Handle = PeHandle;
    Image->ImageContext.ImageRead = EfipCoreReadImageFile;

    //
    // Get information about the image being loaded.
    //

    Status = EfiPeLoaderGetImageInfo(&(Image->ImageContext));
    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (!EFI_IMAGE_MACHINE_TYPE_SUPPORTED(Image->ImageContext.Machine)) {
        RtlDebugPrint("Image Type 0x%x can't be loaded.\n",
                      Image->ImageContext.Machine);

        return EFI_UNSUPPORTED;
    }

    //
    // Set the memory type based on the image type.
    //

    switch (Image->ImageContext.ImageType) {
    case EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION:
        Image->ImageContext.ImageCodeMemoryType = EfiLoaderCode;
        Image->ImageContext.ImageDataMemoryType = EfiLoaderData;
        break;

    case EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER:
        Image->ImageContext.ImageCodeMemoryType = EfiBootServicesCode;
        Image->ImageContext.ImageDataMemoryType = EfiBootServicesData;
        break;

    case EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER:
    case EFI_IMAGE_SUBSYSTEM_SAL_RUNTIME_DRIVER:
        Image->ImageContext.ImageCodeMemoryType = EfiRuntimeServicesCode;
        Image->ImageContext.ImageDataMemoryType = EfiRuntimeServicesData;
        break;

    default:
        Image->ImageContext.ImageError = IMAGE_ERROR_INVALID_SUBSYSTEM;
        return EFI_UNSUPPORTED;
    }

    //
    // Allocate memory of the correct type aligned on the required image
    // boundary.
    //

    DestinationAllocated = FALSE;
    if (DestinationBuffer == 0) {
        Size = (UINTN)Image->ImageContext.ImageSize;
        if (Image->ImageContext.SectionAlignment > EFI_PAGE_SIZE) {
            Size += Image->ImageContext.SectionAlignment;
        }

        Image->ImagePageCount = EFI_SIZE_TO_PAGES(Size);

        //
        // If image relocations have not been stripped, then load at any
        // address. Otherwise, load at the address the image was linked at.
        //

        Status = EFI_OUT_OF_RESOURCES;
        if (Image->ImageContext.RelocationsStripped != FALSE) {
            Status = EfiCoreAllocatePages(
                    AllocateAddress,
                    (EFI_MEMORY_TYPE)(Image->ImageContext.ImageCodeMemoryType),
                    Image->ImagePageCount,
                    &(Image->ImageContext.ImageAddress));

        }

        if ((EFI_ERROR(Status)) &&
            (Image->ImageContext.RelocationsStripped == FALSE)) {

            Status = EfiCoreAllocatePages(
                    AllocateAnyPages,
                    (EFI_MEMORY_TYPE)(Image->ImageContext.ImageCodeMemoryType),
                    Image->ImagePageCount,
                    &(Image->ImageContext.ImageAddress));
        }

        if (EFI_ERROR(Status)) {
            return Status;
        }

        DestinationAllocated = TRUE;

    //
    // The caller provided a destination buffer.
    //

    } else {
        if ((Image->ImageContext.RelocationsStripped != FALSE) &&
            (Image->ImageContext.ImageAddress != DestinationBuffer)) {

            RtlDebugPrint("Image must be loaded at 0x%x.\n",
                          (UINTN)Image->ImageContext.ImageAddress);

            return EFI_INVALID_PARAMETER;
        }

        Size = (UINTN)Image->ImageContext.ImageSize +
               Image->ImageContext.SectionAlignment;

        NeededPages = EFI_SIZE_TO_PAGES(Size);
        if ((Image->ImagePageCount != 0) &&
            (Image->ImagePageCount < NeededPages)) {

            return EFI_BUFFER_TOO_SMALL;
        }

        Image->ImagePageCount = NeededPages;
        Image->ImageContext.ImageAddress = DestinationBuffer;
    }

    Image->ImageBasePage = Image->ImageContext.ImageAddress;
    if (Image->ImageContext.IsTeImage == FALSE) {
        Image->ImageContext.ImageAddress =
                             ALIGN_VALUE(Image->ImageContext.ImageAddress,
                                         Image->ImageContext.SectionAlignment);
    }

    //
    // Load the image from the file.
    //

    Status = EfiPeLoaderLoadImage(&(Image->ImageContext));
    if (EFI_ERROR(Status)) {
        goto CoreLoadPeImageEnd;
    }

    //
    // If this is a runtime driver, allocate memory for the fixup data used to
    // relocate the image when SetVirtualAddressMap is called.
    //

    if (((Attribute & EFI_LOAD_PE_IMAGE_ATTRIBUTE_RUNTIME_REGISTRATION) != 0) &&
        (Image->ImageContext.ImageType ==
         EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER)) {

        Image->ImageContext.FixupData = EfiCoreAllocateRuntimePool(
                                     (UINTN)Image->ImageContext.FixupDataSize);

        if (Image->ImageContext.FixupData == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto CoreLoadPeImageEnd;
        }
    }

    //
    // Relocate the image in memory.
    //

    Status = EfiPeLoaderRelocateImage(&(Image->ImageContext));
    if (EFI_ERROR(Status)) {
        goto CoreLoadPeImageEnd;
    }

    EfiCoreInvalidateInstructionCacheRange(
                             (VOID *)(UINTN)(Image->ImageContext.ImageAddress),
                             (UINTN)(Image->ImageContext.ImageSize));

    Image->Machine = Image->ImageContext.Machine;

    //
    // Get the image entry point.
    //

    Image->EntryPoint =
                (EFI_IMAGE_ENTRY_POINT)(UINTN)(Image->ImageContext.EntryPoint);

    //
    // Fill in the image information for the Loaded Image Protocol.
    //

    Image->Type = Image->ImageContext.ImageType;
    Image->Information.ImageBase =
                             (VOID *)(UINTN)(Image->ImageContext.ImageAddress);

    Image->Information.ImageSize = Image->ImageContext.ImageSize;
    Image->Information.ImageCodeType =
                    (EFI_MEMORY_TYPE)(Image->ImageContext.ImageCodeMemoryType);

    Image->Information.ImageDataType =
                    (EFI_MEMORY_TYPE)(Image->ImageContext.ImageDataMemoryType);

    //
    // Create the runtime image entry as well if needed.
    //

    if (((Attribute & EFI_LOAD_PE_IMAGE_ATTRIBUTE_RUNTIME_REGISTRATION) != 0) &&
        (Image->ImageContext.ImageType ==
         EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER)) {

        Image->RuntimeData = EfiCoreAllocateRuntimePool(
                                              sizeof(EFI_RUNTIME_IMAGE_ENTRY));

        if (Image->RuntimeData == NULL) {
            Status = EFI_OUT_OF_RESOURCES;
            goto CoreLoadPeImageEnd;
        }

        Image->RuntimeData->ImageBase = Image->Information.ImageBase;
        Image->RuntimeData->ImageSize = Image->Information.ImageSize;
        Image->RuntimeData->RelocationData = Image->ImageContext.FixupData;
        Image->RuntimeData->Handle = Image->Handle;
        INSERT_BEFORE(&(Image->RuntimeData->ListEntry),
                      &(EfiRuntimeProtocol->ImageListHead));
    }

    if (EntryPoint != NULL) {
        *EntryPoint = Image->ImageContext.EntryPoint;
    }

    Status = EFI_SUCCESS;

CoreLoadPeImageEnd:
    if (EFI_ERROR(Status)) {
        if (DestinationAllocated != FALSE) {
            EfiCoreFreePages(Image->ImageContext.ImageAddress,
                             Image->ImagePageCount);
        }

        if (Image->ImageContext.FixupData != NULL) {
            EfiCoreFreePool(Image->ImageContext.FixupData);
        }
    }

    return Status;
}

VOID
EfipCoreUnloadAndCloseImage (
    PEFI_IMAGE_DATA Image,
    BOOLEAN FreePages
    )

/*++

Routine Description:

    This routine unloads an EFI image from memory.

Arguments:

    Image - Supplies a pointer to the image data.

    FreePages - Supplies a boolean indicating whether or not to free the
        allocated pages.

Return Value:

    None.

--*/

{

    UINTN ArrayCount;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN HandleIndex;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *OpenInformation;
    UINTN OpenInformationCount;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *OpenInformationEntry;
    UINTN OpenInformationIndex;
    EFI_GUID **ProtocolGuidArray;
    UINTN ProtocolIndex;
    EFI_STATUS Status;

    ASSERT(Image->Magic == EFI_IMAGE_DATA_MAGIC);

    EfiPeLoaderUnloadImage(&(Image->ImageContext));

    //
    // Free references to the image handle.
    //

    if (Image->Handle != NULL) {
        Status = EfiCoreLocateHandleBuffer(AllHandles,
                                           NULL,
                                           NULL,
                                           &HandleCount,
                                           &HandleBuffer);

        if (!EFI_ERROR(Status)) {
            for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex += 1) {
                Status = EfiCoreProtocolsPerHandle(HandleBuffer[HandleIndex],
                                                   &ProtocolGuidArray,
                                                   &ArrayCount);

                if (!EFI_ERROR(Status)) {
                    for (ProtocolIndex = 0;
                         ProtocolIndex < ArrayCount;
                         ProtocolIndex += 1) {

                        Status = EfiCoreOpenProtocolInformation(
                                              HandleBuffer[HandleIndex],
                                              ProtocolGuidArray[ProtocolIndex],
                                              &OpenInformation,
                                              &OpenInformationCount);

                        if (!EFI_ERROR(Status)) {
                            for (OpenInformationIndex = 0;
                                 OpenInformationIndex < OpenInformationCount;
                                 OpenInformationIndex += 1) {

                                OpenInformationEntry =
                                      &(OpenInformation[OpenInformationIndex]);

                                if (OpenInformationEntry->AgentHandle ==
                                    Image->Handle) {

                                    EfiCoreCloseProtocol(
                                        HandleBuffer[HandleIndex],
                                        ProtocolGuidArray[ProtocolIndex],
                                        Image->Handle,
                                        OpenInformationEntry->ControllerHandle);
                                }
                            }

                            if (OpenInformation != NULL) {
                                EfiCoreFreePool(OpenInformation);
                            }
                        }
                    }

                    if (ProtocolGuidArray != NULL) {
                        EfiCoreFreePool(ProtocolGuidArray);
                    }
                }
            }

            if (HandleBuffer != NULL) {
                EfiCoreFreePool(HandleBuffer);
            }
        }

        //
        // Let the debugger know the image is being unloaded.
        //

        if (Image->DebuggerData != NULL) {
            KdReportModuleChange(Image->DebuggerData, FALSE);
            EfiCoreFreePool(Image->DebuggerData);
        }

        EfiCoreUninstallProtocolInterface(Image->Handle,
                                          &EfiLoadedImageDevicePathProtocolGuid,
                                          Image->LoadedImageDevicePath);

        EfiCoreUninstallProtocolInterface(Image->Handle,
                                          &EfiLoadedImageProtocolGuid,
                                          &(Image->Information));

        if (Image->ImageContext.HiiResourceData != 0) {
            EfiCoreUninstallProtocolInterface(
                           Image->Handle,
                           &EfiHiiPackageListProtocolGuid,
                           (VOID *)(UINTN)Image->ImageContext.HiiResourceData);

        }
    }

    if (Image->RuntimeData != NULL) {
        if (Image->RuntimeData->ListEntry.Next != NULL) {
            LIST_REMOVE(&(Image->RuntimeData->ListEntry));
        }

        EfiCoreFreePool(Image->RuntimeData);
    }

    //
    // Free the image from memory.
    //

    if ((Image->ImageBasePage != 0) && (FreePages != FALSE)) {
        EfiCoreFreePages(Image->ImageBasePage, Image->ImagePageCount);
    }

    if (Image->Information.FilePath != NULL) {
        EfiCoreFreePool(Image->Information.FilePath);
    }

    if (Image->LoadedImageDevicePath != NULL) {
        EfiCoreFreePool(Image->LoadedImageDevicePath);
    }

    if (Image->FixupData != NULL) {
        EfiCoreFreePool(Image->FixupData);
    }

    Image->Magic = 0;
    EfiCoreFreePool(Image);
    return;
}

PEFI_IMAGE_DATA
EfipCoreGetImageDataFromHandle (
    EFI_HANDLE ImageHandle
    )

/*++

Routine Description:

    This routine returns the private image data from a handle.

Arguments:

    ImageHandle - Supplies the image handle.

Return Value:

    Returns a pointer to the private image data.

--*/

{

    PEFI_IMAGE_DATA Image;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_STATUS Status;

    Status = EfiCoreHandleProtocol(ImageHandle,
                                   &EfiLoadedImageProtocolGuid,
                                   (VOID **)&LoadedImage);

    if (!EFI_ERROR(Status)) {
        Image = PARENT_STRUCTURE(LoadedImage, EFI_IMAGE_DATA, Information);

        ASSERT(Image->Magic == EFI_IMAGE_DATA_MAGIC);

    } else {

        ASSERT(FALSE);

        Image = NULL;
    }

    return Image;
}

EFIAPI
EFI_STATUS
EfipCoreReadImageFile (
    VOID *FileHandle,
    UINTN FileOffset,
    UINTN *ReadSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine reads contents of the PE/COFF image file.

Arguments:

    FileHandle - Supplies a pointer to the file handle to read from.

    FileOffset - Supplies an offset in bytes from the beginning of the file to
        read.

    ReadSize - Supplies a pointer that on input contains the number of bytes to
        read. On output, returns the number of bytes read.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI status code.

--*/

{

    UINTN EndPosition;
    PEFI_IMAGE_FILE_HANDLE ImageHandle;

    if ((FileHandle == NULL) || (ReadSize == NULL) || (Buffer == NULL)) {
        return EFI_INVALID_PARAMETER;
    }

    if (MAX_ADDRESS - FileOffset < *ReadSize) {
        return EFI_INVALID_PARAMETER;
    }

    ImageHandle = (EFI_IMAGE_FILE_HANDLE *)FileHandle;

    ASSERT(ImageHandle->Magic == EFI_IMAGE_FILE_HANDLE_MAGIC);

    EndPosition = FileOffset + *ReadSize;
    if (EndPosition > ImageHandle->SourceSize) {
        *ReadSize = (UINT32)(ImageHandle->SourceSize - FileOffset);
    }

    if (FileOffset >= ImageHandle->SourceSize) {
        *ReadSize = 0;
    }

    if (*ReadSize != 0) {
        EfiCoreCopyMemory(Buffer,
                          (CHAR8 *)ImageHandle->Source + FileOffset,
                          *ReadSize);
    }

    return EFI_SUCCESS;
}

CHAR8 *
EfipCoreConvertFileNameToAscii (
    CHAR16 *FileName,
    UINTN *AsciiNameSize
    )

/*++

Routine Description:

    This routine creates an ASCII version of the given wide string.

Arguments:

    FileName - Supplies a pointer to the wide string.

    AsciiNameSize - Supplies a pointer where the size of the ASCII string in
        bytes including the NULL terminator will be returned.

Return Value:

    Returns a pointer to the ASCII string. The caller is responsible for
    freeing this memory.

    NULL on allocation failure.

--*/

{

    CHAR8 *AsciiCharacter;
    CHAR8 *AsciiString;
    UINTN StringSize;

    StringSize = EfiCoreStringLength(FileName) + 1;
    AsciiString = EfiCoreAllocateBootPool(StringSize);
    if (AsciiString == NULL) {
        *AsciiNameSize = 0;
        return NULL;
    }

    AsciiCharacter = AsciiString;
    while (TRUE) {
        *AsciiCharacter = *FileName;
        if (*AsciiCharacter == '\0') {
            break;
        }

        FileName += 1;
        AsciiCharacter += 1;
    }

    *AsciiNameSize = StringSize;
    return AsciiString;
}

