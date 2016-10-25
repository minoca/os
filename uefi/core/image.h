/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    image.h

Abstract:

    This header contains definitions for UEFI core image services.

Author:

    Evan Green 13-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
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
    );

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

EFIAPI
EFI_STATUS
EfiCoreUnloadImage (
    EFI_HANDLE ImageHandle
    );

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

EFI_STATUS
EfiCoreStartImage (
    EFI_HANDLE ImageHandle,
    UINTN *ExitDataSize,
    CHAR16 **ExitData
    );

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

EFIAPI
EFI_STATUS
EfiCoreExit (
    EFI_HANDLE ImageHandle,
    EFI_STATUS ExitStatus,
    UINTN ExitDataSize,
    CHAR16 *ExitData
    );

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

EFI_STATUS
EfiCoreInitializeImageServices (
    VOID *FirmwareBaseAddress,
    VOID *FirmwareLowestAddress,
    UINTN FirmwareSize
    );

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
