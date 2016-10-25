/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ramdisk.c

Abstract:

    This module implements support for creating a Block I/O protocol from a
    RAM Disk device.

Author:

    Evan Green 3-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/uefi/protocol/ramdisk.h>
#include <minoca/uefi/protocol/blockio.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts from a block I/O protocol to the RAM disk context.
//

#define EFI_RAM_DISK_FROM_THIS(_BlockIo)                  \
        PARENT_STRUCTURE(_BlockIo, EFI_RAM_DISK_CONTEXT, BlockIo);

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_RAM_DISK_MAGIC 0x444D4152 // 'DMAR'

#define EFI_RAM_DISK_BLOCK_SIZE 512

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the RAM disk device context.

Members:

    Magic - Stores the magic constand EFI_RAM_DISK_MAGIC.

    Handle - Stores the handle to the block I/O device.

    DevicePath - Stores a pointer to the device path.

    BlockCount - Stores the cached block count of the media.

    RamDisk - Stores the instance of the RAM disk protocol.

    BlockIo - Stores the block I/O protocol.

    Media - Stores the block I/O media information.

--*/

typedef struct _EFI_RAM_DISK_CONTEXT {
    UINT32 Magic;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINT64 BlockCount;
    EFI_RAM_DISK_PROTOCOL RamDisk;
    EFI_BLOCK_IO_PROTOCOL BlockIo;
    EFI_BLOCK_IO_MEDIA Media;
} EFI_RAM_DISK_CONTEXT, *PEFI_RAM_DISK_CONTEXT;

/*++

Structure Description:

    This structure defines the SD OMAP4 block I/O device path.

Members:

    DevicePath - Stores the standard vendor-specific device path.

    Base - Stores the base physical address of the RAM disk.

--*/

typedef struct _EFI_RAM_DISK_DEVICE_PATH_NODE {
    VENDOR_DEVICE_PATH DevicePath;
    EFI_PHYSICAL_ADDRESS Base;
} EFI_RAM_DISK_DEVICE_PATH_NODE, *PEFI_RAM_DISK_DEVICE_PATH_NODE;

/*++

Structure Description:

    This structure defines the RAM disk device path format.

Members:

    Disk - Stores the RAM disk device path node.

    End - Stores the end device path node.

--*/

typedef struct _EFI_RAM_DISK_DEVICE_PATH {
    EFI_RAM_DISK_DEVICE_PATH_NODE Disk;
    EFI_DEVICE_PATH_PROTOCOL End;
} PACKED EFI_RAM_DISK_DEVICE_PATH, *PEFI_RAM_DISK_DEVICE_PATH;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfipRamDiskReset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

EFIAPI
EFI_STATUS
EfipRamDiskReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipRamDiskWriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfipRamDiskFlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_RAM_DISK_DEVICE_PATH EfiRamDiskDevicePathTemplate = {
    {
        {
            {
                HARDWARE_DEVICE_PATH,
                HW_VENDOR_DP,
                sizeof(EFI_RAM_DISK_DEVICE_PATH_NODE)
            },

            EFI_RAM_DISK_PROTOCOL_GUID,
        },

        0
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

EFI_GUID EfiRamDiskProtocolGuid = EFI_RAM_DISK_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiCoreEnumerateRamDisk (
    EFI_PHYSICAL_ADDRESS Base,
    UINT64 Size
    )

/*++

Routine Description:

    This routine enumerates a RAM disk at the given address.

Arguments:

    Base - Supplies the base physical address of the RAM disk.

    Size - Supplies the size of the RAM disk.

Return Value:

    EFI Status code.

--*/

{

    PEFI_RAM_DISK_CONTEXT Context;
    EFI_RAM_DISK_DEVICE_PATH *DevicePath;
    EFI_STATUS Status;

    Context = NULL;
    DevicePath = NULL;

    //
    // Allocate the context structure.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_RAM_DISK_CONTEXT),
                             (VOID **)&Context);

    if (EFI_ERROR(Status)) {
        goto EnumerateRamDiskEnd;
    }

    EfiSetMem(Context, sizeof(EFI_RAM_DISK_CONTEXT), 0);
    Context->Magic = EFI_RAM_DISK_MAGIC;

    //
    // Allocate the device path.
    //

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_RAM_DISK_DEVICE_PATH),
                             (VOID **)(&DevicePath));

    if (EFI_ERROR(Status)) {
        goto EnumerateRamDiskEnd;
    }

    EfiCopyMem(DevicePath,
               &EfiRamDiskDevicePathTemplate,
               sizeof(EFI_RAM_DISK_DEVICE_PATH));

    DevicePath->Disk.Base = Base;
    Context->DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
    Context->BlockCount = (Size + (EFI_RAM_DISK_BLOCK_SIZE - 1)) /
                          EFI_RAM_DISK_BLOCK_SIZE;

    Context->RamDisk.Revision = EFI_RAM_DISK_PROTOCOL_REVISION;
    Context->RamDisk.Base = Base;
    Context->RamDisk.Length = Size;
    Context->BlockIo.Revision = EFI_BLOCK_IO_PROTOCOL_REVISION3;
    Context->BlockIo.Media = &(Context->Media);
    Context->BlockIo.Reset = EfipRamDiskReset;
    Context->BlockIo.ReadBlocks = EfipRamDiskReadBlocks;
    Context->BlockIo.WriteBlocks = EfipRamDiskWriteBlocks;
    Context->BlockIo.FlushBlocks = EfipRamDiskFlushBlocks;
    Context->Media.MediaId = 1;
    Context->Media.MediaPresent = 1;
    Context->Media.BlockSize = EFI_RAM_DISK_BLOCK_SIZE;
    Context->Media.LastBlock = Context->BlockCount - 1;
    Status = EfiInstallMultipleProtocolInterfaces(&(Context->Handle),
                                                  &EfiBlockIoProtocolGuid,
                                                  &(Context->BlockIo),
                                                  &EfiDevicePathProtocolGuid,
                                                  Context->DevicePath,
                                                  &EfiRamDiskProtocolGuid,
                                                  &(Context->RamDisk),
                                                  NULL);

EnumerateRamDiskEnd:
    if (EFI_ERROR(Status)) {
        if (Context != NULL) {
            EfiFreePool(Context);
        }

        if (DevicePath != NULL) {
            EfiFreePool(DevicePath);
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfipRamDiskReset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    )

/*++

Routine Description:

    This routine resets the block device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ExtendedVerification - Supplies a boolean indicating whether or not the
        driver should perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

--*/

{

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipRamDiskReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a block I/O read from the device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    MediaId - Supplies the media identifier, which changes each time the media
        is replaced.

    Lba - Supplies the logical block address of the read.

    BufferSize - Supplies the size of the buffer in bytes.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the media ID does not match the current device.

    EFI_BAD_BUFFER_SIZE if the buffer was not a multiple of the device block
    size.

    EFI_INVALID_PARAMETER if the read request contains LBAs that are not valid,
    or the buffer is not properly aligned.

--*/

{

    EFI_RAM_DISK_CONTEXT *Context;
    VOID *DiskBuffer;

    Context = EFI_RAM_DISK_FROM_THIS(This);
    if (Lba + (BufferSize / EFI_RAM_DISK_BLOCK_SIZE) > Context->BlockCount) {
        return EFI_INVALID_PARAMETER;
    }

    if ((BufferSize % EFI_RAM_DISK_BLOCK_SIZE) != 0) {
        return EFI_BAD_BUFFER_SIZE;
    }

    DiskBuffer = (VOID *)(UINTN)(Context->RamDisk.Base +
                                 (Lba * EFI_RAM_DISK_BLOCK_SIZE));

    EfiCopyMem(Buffer, DiskBuffer, BufferSize);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipRamDiskWriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a block I/O write to the device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    MediaId - Supplies the media identifier, which changes each time the media
        is replaced.

    Lba - Supplies the logical block address of the write.

    BufferSize - Supplies the size of the buffer in bytes.

    Buffer - Supplies the buffer containing the data to write.

Return Value:

    EFI_SUCCESS on success.

    EFI_WRITE_PROTECTED if the device cannot be written to.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the media ID does not match the current device.

    EFI_BAD_BUFFER_SIZE if the buffer was not a multiple of the device block
    size.

    EFI_INVALID_PARAMETER if the read request contains LBAs that are not valid,
    or the buffer is not properly aligned.

--*/

{

    EFI_RAM_DISK_CONTEXT *Context;
    VOID *DiskBuffer;

    Context = EFI_RAM_DISK_FROM_THIS(This);
    if (Lba + (BufferSize / EFI_RAM_DISK_BLOCK_SIZE) >= Context->BlockCount) {
        return EFI_INVALID_PARAMETER;
    }

    if ((BufferSize % EFI_RAM_DISK_BLOCK_SIZE) != 0) {
        return EFI_BAD_BUFFER_SIZE;
    }

    DiskBuffer = (VOID *)(UINTN)(Context->RamDisk.Base +
                                 (Lba * EFI_RAM_DISK_BLOCK_SIZE));

    EfiCopyMem(DiskBuffer, Buffer, BufferSize);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipRamDiskFlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    )

/*++

Routine Description:

    This routine flushes the block device.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

--*/

{

    return EFI_SUCCESS;
}

