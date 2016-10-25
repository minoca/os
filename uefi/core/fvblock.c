/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fvblock.c

Abstract:

    This module implements UEFI Firmware Volume Block Protocol support.

Author:

    Evan Green 11-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "fwvol.h"
#include "fvblock.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the firmware block device given a pointer
// to the firmware block device protocol instance.
//

#define EFI_FIRMWARE_BLOCK_DEVICE_FROM_THIS(_This) \
    PARENT_STRUCTURE(_This, EFI_FIRMWARE_BLOCK_DEVICE, BlockProtocol)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_FIRMWARE_BLOCK_DEVICE_MAGIC 0x6C427646 // 'lBvF'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the device path structures for the two variants of device path that
// go on a memory firmware volume block device. The memmap device path is
// generated if the firmware volume doesn't have a name GUID. The media
// firmware volume device path is generated if the firmware volume exposes a
// name GUID in its extended header.
//

typedef struct _EFI_FIRMWARE_BLOCK_MEMMAP_DEVICE_PATH {
    MEMMAP_DEVICE_PATH MemMapDevicePath;
    EFI_DEVICE_PATH_PROTOCOL EndDevicePath;
} EFI_FIRMWARE_BLOCK_MEMMAP_DEVICE_PATH,
    *PEFI_FIRMWARE_BLOCK_MEMMAP_DEVICE_PATH;

typedef struct _EFI_FIRMWARE_BLOCK_MEDIA_DEVICE_PATH {
    MEDIA_FW_VOL_DEVICE_PATH MediaDevicePath;
    EFI_DEVICE_PATH_PROTOCOL EndDevicePath;
} EFI_FIRMWARE_BLOCK_MEDIA_DEVICE_PATH, *PEFI_FIRMWARE_BLOCK_MEDIA_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a Logical Block Address cache entry.

Members:

    Base - Stores the base block address.

    Length - Stores the length of that block.

--*/

typedef struct _EFI_LBA_CACHE {
    UINTN Base;
    UINTN Length;
} EFI_LBA_CACHE, *PEFI_LBA_CACHE;

/*++

Structure Description:

    This structure defines EFI firmware volume block I/O protocol data.

Members:

    Magic - Stores the magic value EFI_FIRMWARE_BLOCK_DEVICE_MAGIC.

    Handle - Stores the handle that the block I/O protocol is on.

    DevicePath - Stores a pointer to the device path of the block I/O protocol.

    BlockProtocol - Stores the block protocol value.

    BlockCount - Stores the number of blocks in the volume.

    LbaCache - Stores a pointer to the Logical Block Address cache entries.

    Attributes - Stores the volume attributes.

    BaseAddress - Stores the base physical address of the firmware volume.

    AuthenticationStatus - Stores the authentication status of the firmware
        volume.

--*/

typedef struct _EFI_FIRMWARE_BLOCK_DEVICE {
    ULONG Magic;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL BlockProtocol;
    UINTN BlockCount;
    PEFI_LBA_CACHE LbaCache;
    UINT32 Attributes;
    EFI_PHYSICAL_ADDRESS BaseAddress;
    UINT32 AuthenticationStatus;
} EFI_FIRMWARE_BLOCK_DEVICE, *PEFI_FIRMWARE_BLOCK_DEVICE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfiFvBlockGetAttributes (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_FVB_ATTRIBUTES *Attributes
    );

EFIAPI
EFI_STATUS
EfiFvBlockSetAttributes (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_FVB_ATTRIBUTES *Attributes
    );

EFIAPI
EFI_STATUS
EfiFvBlockGetPhysicalAddress (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_PHYSICAL_ADDRESS *Address
    );

EFIAPI
EFI_STATUS
EfiFvBlockGetBlockSize (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN *BlockSize,
    UINTN *NumberOfBlocks
    );

EFIAPI
EFI_STATUS
EfiFvBlockRead (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN Offset,
    UINTN *ByteCount,
    UINT8 *Buffer
    );

EFIAPI
EFI_STATUS
EfiFvBlockWrite (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN Offset,
    UINTN *ByteCount,
    UINT8 *Buffer
    );

EFIAPI
EFI_STATUS
EfiFvBlockErase (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    ...
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_FIRMWARE_BLOCK_DEVICE EfiFirmwareBlockDeviceTemplate = {
    EFI_FIRMWARE_BLOCK_DEVICE_MAGIC,
    NULL,
    NULL,
    {
        EfiFvBlockGetAttributes,
        EfiFvBlockSetAttributes,
        EfiFvBlockGetPhysicalAddress,
        EfiFvBlockGetBlockSize,
        EfiFvBlockRead,
        EfiFvBlockWrite,
        EfiFvBlockErase,
        NULL
    },

    0,
    NULL,
    0,
    0,
    0
};

EFI_FIRMWARE_BLOCK_MEMMAP_DEVICE_PATH
    EfiFirmwareBlockMemMapDevicePathTemplate = {

    {
        {
            HARDWARE_DEVICE_PATH,
            HW_MEMMAP_DP,
            sizeof(MEMMAP_DEVICE_PATH),
        },

        EfiMemoryMappedIO,
        0,
        0
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

EFI_FIRMWARE_BLOCK_MEDIA_DEVICE_PATH EfiFirmwareBlockMediaDevicePathTemplate = {
    {
        {
            MEDIA_DEVICE_PATH,
            MEDIA_PIWG_FW_VOL_DP,
            sizeof(MEDIA_FW_VOL_DEVICE_PATH)
        },

        {0}
    },

    {
        END_DEVICE_PATH_TYPE,
        END_ENTIRE_DEVICE_PATH_SUBTYPE,
        END_DEVICE_PATH_LENGTH
    }
};

EFI_GUID EfiFirmwareVolumeBlockProtocolGuid =
                                       EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiFvInitializeBlockSupport (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine initializes the Firmware Volume Block I/O support module.

Arguments:

    ImageHandle - Supplies a pointer to the image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

{

    return EFI_SUCCESS;
}

EFI_STATUS
EfiCreateFirmwareVolume (
    EFI_PHYSICAL_ADDRESS BaseAddress,
    UINT64 Length,
    EFI_HANDLE ParentHandle,
    UINT32 AuthenticationStatus,
    EFI_HANDLE *BlockIoProtocol
    )

/*++

Routine Description:

    This routine creates a firmware volume out of the given memory buffer.
    Specifically, this function creates a handle and adds the Firmware Block
    I/O protocol and the Device Path protocol to it. The firmware volume
    protocol will then attach after noticing the block I/O protocol instance.

Arguments:

    BaseAddress - Supplies the physical address of the firmware volume buffer.

    Length - Supplies the length of the firmware volume buffer in bytes.

    ParentHandle - Supplies an optional handle to a parent firmware volume this
        volume is being enumerated from.

    AuthenticationStatus - Supplies the authentication status of this volume if
        this volume came from another file and volume.

    BlockIoProtocol - Supplies an optional pointer where the created handle
        will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_VOLUME_CORRUPTED if the volume was not valid.

    EFI_OUT_OF_RESOURCES on allocation failure.

--*/

{

    UINT32 Alignment;
    UINTN BlockIndex;
    EFI_FV_BLOCK_MAP_ENTRY *BlockMap;
    UINTN BlockRangeIndex;
    PEFI_FIRMWARE_BLOCK_DEVICE Device;
    UINTN LinearOffset;
    PEFI_FIRMWARE_BLOCK_MEDIA_DEVICE_PATH MediaDevicePath;
    PEFI_FIRMWARE_BLOCK_MEMMAP_DEVICE_PATH MemMapDevicePath;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader;
    EFI_FIRMWARE_VOLUME_EXT_HEADER *VolumeHeaderExt;

    Alignment = 0;
    VolumeHeader = (EFI_FIRMWARE_VOLUME_HEADER *)(UINTN)BaseAddress;
    if (VolumeHeader->Signature != EFI_FVH_SIGNATURE) {
        return EFI_VOLUME_CORRUPTED;
    }

    //
    // If the weak alignment bit is set then the first byte of the volume
    // can be aligned on any power of two boundary. A weakly aligned volume
    // cannot be moved from its initial linked location and maintain its
    // alignment.
    //

    if ((VolumeHeader->Attributes & EFI_FVB2_WEAK_ALIGNMENT) == 0) {
        Alignment =
                  1 << ((VolumeHeader->Attributes & EFI_FVB2_ALIGNMENT) >> 16);

        if (Alignment < 8) {
            Alignment = 8;
        }

        if (((UINTN)BaseAddress % Alignment) != 0) {
            RtlDebugPrint("Firmware Volume Base Address 0x%I64x is not "
                          "aligned to 0x%x.\n",
                          BaseAddress,
                          Alignment);

            return EFI_VOLUME_CORRUPTED;
        }
    }

    Device = EfiCoreAllocateBootPool(sizeof(EFI_FIRMWARE_BLOCK_DEVICE));
    if (Device == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EfiCoreCopyMemory(Device,
                      &EfiFirmwareBlockDeviceTemplate,
                      sizeof(EFI_FIRMWARE_BLOCK_DEVICE));

    Device->BaseAddress = BaseAddress;
    Device->Attributes = VolumeHeader->Attributes;
    Device->BlockProtocol.ParentHandle = ParentHandle;
    if (ParentHandle != NULL) {
        Device->AuthenticationStatus = AuthenticationStatus;
    }

    //
    // Count the number of blocks in the volume.
    //

    Device->BlockCount = 0;
    BlockMap = VolumeHeader->BlockMap;
    while (BlockMap->BlockCount != 0) {
        Device->BlockCount += BlockMap->BlockCount;
        BlockMap += 1;
    }

    //
    // Allocate the cache.
    //

    if (Device->BlockCount >= (MAX_ADDRESS / sizeof(EFI_LBA_CACHE))) {
        EfiCoreFreePool(Device);
        return EFI_OUT_OF_RESOURCES;
    }

    Device->LbaCache = EfiCoreAllocateBootPool(
                                   Device->BlockCount * sizeof(EFI_LBA_CACHE));

    if (Device->LbaCache == NULL) {
        EfiCoreFreePool(Device);
        return EFI_OUT_OF_RESOURCES;
    }

    //
    // Fill in the cache with the linear address of the blocks.
    //

    BlockIndex = 0;
    LinearOffset = 0;
    BlockMap = VolumeHeader->BlockMap;
    while (BlockMap->BlockCount != 0) {
        for (BlockRangeIndex = 0;
             BlockRangeIndex < BlockMap->BlockCount;
             BlockRangeIndex += 1) {

            Device->LbaCache[BlockIndex].Base = LinearOffset;
            Device->LbaCache[BlockIndex].Length = BlockMap->BlockLength;
            LinearOffset += BlockMap->BlockLength;
            BlockIndex += 1;
        }

        BlockMap += 1;
    }

    //
    // Judge whether or not the firmware volume name GUID comes from the
    // extended header.
    //

    if (VolumeHeader->ExtHeaderOffset == 0) {

        //
        // The firmware volume does not contain a name GUID, so produce a
        // MEMMAP_DEVICE_PATH.
        //

        Device->DevicePath = EfiCoreAllocateBootPool(
                                sizeof(EFI_FIRMWARE_BLOCK_MEMMAP_DEVICE_PATH));

        if (Device->DevicePath == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }

        EfiCoreCopyMemory(Device->DevicePath,
                          &EfiFirmwareBlockMemMapDevicePathTemplate,
                          sizeof(EFI_FIRMWARE_BLOCK_MEMMAP_DEVICE_PATH));

        MemMapDevicePath =
                  (PEFI_FIRMWARE_BLOCK_MEMMAP_DEVICE_PATH)(Device->DevicePath);

        MemMapDevicePath->MemMapDevicePath.StartingAddress = BaseAddress;
        MemMapDevicePath->MemMapDevicePath.EndingAddress =
                                        BaseAddress + VolumeHeader->Length - 1;

    //
    // If the firmware volume contains an extension header, then expose a
    // media firmware volume device path.
    //

    } else {
        Device->DevicePath = EfiCoreAllocateBootPool(
                                 sizeof(EFI_FIRMWARE_BLOCK_MEDIA_DEVICE_PATH));

        if (Device->DevicePath == NULL) {
            return EFI_OUT_OF_RESOURCES;
        }

        EfiCoreCopyMemory(Device->DevicePath,
                          &EfiFirmwareBlockMediaDevicePathTemplate,
                          sizeof(EFI_FIRMWARE_BLOCK_MEDIA_DEVICE_PATH));

        MediaDevicePath =
                   (PEFI_FIRMWARE_BLOCK_MEDIA_DEVICE_PATH)(Device->DevicePath);

        VolumeHeaderExt =
             (EFI_FIRMWARE_VOLUME_EXT_HEADER *)((UINT8 *)VolumeHeader +
                                                VolumeHeader->ExtHeaderOffset);

        EfiCoreCopyMemory(&(MediaDevicePath->MediaDevicePath.FvName),
                          &(VolumeHeaderExt->FvName),
                          sizeof(EFI_GUID));
    }

    //
    // Attach the block I/O protocol to a new handle.
    //

    ASSERT(Device->Handle == NULL);

    Status = EfiCoreInstallMultipleProtocolInterfaces(
                                           &(Device->Handle),
                                           &EfiFirmwareVolumeBlockProtocolGuid,
                                           &(Device->BlockProtocol),
                                           &EfiDevicePathProtocolGuid,
                                           Device->DevicePath,
                                           NULL);

    if (BlockIoProtocol != NULL) {
        *BlockIoProtocol = Device->Handle;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfiFvBlockGetAttributes (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_FVB_ATTRIBUTES *Attributes
    )

/*++

Routine Description:

    This routine retrieves the attributes and current settings of the block
    device.

Arguments:

    This - Supplies the protocol instance.

    Attributes - Supplies a pointer where the attributes will be returne.

Return Value:

    EFI Status code.

--*/

{

    PEFI_FIRMWARE_BLOCK_DEVICE Device;

    if (Attributes == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Return attributes from the in-memory copy, and report it as not writable.
    //

    Device = EFI_FIRMWARE_BLOCK_DEVICE_FROM_THIS(This);
    *Attributes = Device->Attributes & (~EFI_FVB_WRITE_STATUS);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiFvBlockSetAttributes (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_FVB_ATTRIBUTES *Attributes
    )

/*++

Routine Description:

    This routine sets configurable firmware volume attributes and returns the
    new settings.

Arguments:

    This - Supplies the protocol instance.

    Attributes - Supplies a pointer that on input contains the attributes to
        set. On output, it contains the new settings.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the requested attributes are in conflict with the
    capabilities as declared in the firmware volume header.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiFvBlockGetPhysicalAddress (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_PHYSICAL_ADDRESS *Address
    )

/*++

Routine Description:

    This routine retrieves the base address of a memory-mapped firmware volume.
    This function should only be called for memory-mapped firmware volumes.

Arguments:

    This - Supplies the protocol instance.

    Address - Supplies a pointer where the physical address of the firmware
        volume will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the firmware volume is not memory mapped.

--*/

{

    PEFI_FIRMWARE_BLOCK_DEVICE Device;

    Device = EFI_FIRMWARE_BLOCK_DEVICE_FROM_THIS(This);
    if ((Device->Attributes & EFI_FVB_MEMORY_MAPPED) != 0) {
        *Address = Device->BaseAddress;
        return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiFvBlockGetBlockSize (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN *BlockSize,
    UINTN *NumberOfBlocks
    )

/*++

Routine Description:

    This routine retrieves the size of the requested block. It also returns the
    number of additional blocks with the identical size. This routine is used
    to retrieve the block map.

Arguments:

    This - Supplies the protocol instance.

    Lba - Supplies the block for which to return the size.

    BlockSize - Supplies a pointer where the size of the block is returned on
        success.

    NumberOfBlocks - Supplies a pointer where the number of consecutive blocks
        with the same block size is returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the requested block address is out of range.

--*/

{

    EFI_FV_BLOCK_MAP_ENTRY *BlockMap;
    PEFI_FIRMWARE_BLOCK_DEVICE Device;
    UINTN TotalBlocks;
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader;

    Device = EFI_FIRMWARE_BLOCK_DEVICE_FROM_THIS(This);
    if (Lba >= Device->BlockCount) {
        return EFI_INVALID_PARAMETER;
    }

    VolumeHeader = (EFI_FIRMWARE_VOLUME_HEADER *)((UINTN)(Device->BaseAddress));
    BlockMap = VolumeHeader->BlockMap;

    //
    // Search the block map for the given block.
    //

    TotalBlocks = 0;
    while ((BlockMap->BlockCount != 0) || (BlockMap->BlockLength != 0)) {
        TotalBlocks += BlockMap->BlockCount;
        if (Lba < TotalBlocks) {
            break;
        }

        BlockMap += 1;
    }

    *BlockSize = BlockMap->BlockLength;
    *NumberOfBlocks = TotalBlocks - (UINTN)Lba;
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiFvBlockRead (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN Offset,
    UINTN *ByteCount,
    UINT8 *Buffer
    )

/*++

Routine Description:

    This routine reads the requested number of bytes from the requested block
    and stores them in the provided buffer. Implementations should be mindful
    that the firmware volume might be in the ReadDisabled state. If it is in
    this state, this function must return the status code EFI_ACCESS_DENIED
    without modifying the contents of the buffer. This function must also
    prevent spanning block boundaries. If a read is requested that would span a
    block boundary, the read must read up to the boundary but not beyond. The
    output byte count parameter must be set to correctly indicate the number of
    bytes actually read. The caller must be aware that a read may be partially
    completed.

Arguments:

    This - Supplies the protocol instance.

    Lba - Supplies the starting block address from which to read.

    Offset - Supplies the offset into the block at which to begin reading.

    ByteCount - Supplies a pointer that on input contains the size of the
        read buffer in bytes. On output, will contain the number of bytes read.

    Buffer - Supplies a pointer to the caller-allocated buffer where the read
        data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_BAD_BUFFER_SIZE if the read attempted to cross an LBA boundary. On
    output, the byte count will contain the number of bytes returned in the
    buffer.

    EFI_ACCESS_DENIED if the firmware volume is in the ReadDisabled state.

    EFI_DEVICE_ERROR if the block device is not functioning properly.

--*/

{

    UINTN BytesRead;
    PEFI_FIRMWARE_BLOCK_DEVICE Device;
    UINTN LbaIndex;
    UINT8 *LbaOffset;
    UINTN LbaStart;
    EFI_FIRMWARE_VOLUME_HEADER *VolumeHeader;

    Device = EFI_FIRMWARE_BLOCK_DEVICE_FROM_THIS(This);
    if ((Device->Attributes & EFI_FVB_READ_STATUS) == 0) {
        return EFI_ACCESS_DENIED;
    }

    LbaIndex = (UINTN)Lba;
    if (LbaIndex >= Device->BlockCount) {
        *ByteCount = 0;
        return EFI_BAD_BUFFER_SIZE;
    }

    if (Offset > Device->LbaCache[LbaIndex].Length) {
        *ByteCount = 0;
        return EFI_BAD_BUFFER_SIZE;
    }

    BytesRead = *ByteCount;

    //
    // If the read partially exceeds the block boundary, read from the current
    // position to the end of the block.
    //

    if (Offset + BytesRead > Device->LbaCache[LbaIndex].Length) {
        BytesRead = Device->LbaCache[LbaIndex].Length - Offset;
    }

    LbaStart = Device->LbaCache[LbaIndex].Base;
    VolumeHeader = (EFI_FIRMWARE_VOLUME_HEADER *)((UINTN)(Device->BaseAddress));
    LbaOffset = (UINT8 *)VolumeHeader + LbaStart + Offset;
    EfiCoreCopyMemory(Buffer, LbaOffset, BytesRead);
    if (BytesRead == *ByteCount) {
        return EFI_SUCCESS;
    }

    *ByteCount = BytesRead;
    return EFI_BAD_BUFFER_SIZE;
}

EFIAPI
EFI_STATUS
EfiFvBlockWrite (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN Offset,
    UINTN *ByteCount,
    UINT8 *Buffer
    )

/*++

Routine Description:

    This routine writes the specified number of bytes from the provided buffer
    to the specified block and offset. If the firmware volume is sticky write,
    the caller must ensure that all the bits of the specified range to write
    are in the EFI_FVB_ERASE_POLARITY state before calling the write function,
    or else the result will be unpredictable. This unpredictability arises
    because, for a sticky-write firmware volume, a write may negate a bit in
    the EFI_FVB_ERASE_POLARITY state but cannot flip it back again.  Before
    calling the write function, it is recommended for the caller to first call
    the erase blocks function to erase the specified block to write. A block
    erase cycle will transition bits from the (NOT)EFI_FVB_ERASE_POLARITY state
    back to the EFI_FVB_ERASE_POLARITY state. Implementations should be mindful
    that the firmware volume might be in the WriteDisabled state. If it is in
    this state, this function must return the status code EFI_ACCESS_DENIED
    without modifying the contents of the firmware volume. The write function
    must also prevent spanning block boundaries. If a write is requested that
    spans a block boundary, the write must store up to the boundary but not
    beyond. The output byte count parameter must be set to correctly indicate
    the number of bytes actually written. The caller must be aware that a write
    may be partially completed. All writes, partial or otherwise, must be fully
    flushed to the hardware before the write routine returns.

Arguments:

    This - Supplies the protocol instance.

    Lba - Supplies the starting block address from which to write.

    Offset - Supplies the offset into the block at which to begin writing.

    ByteCount - Supplies a pointer that on input contains the size of the
        write buffer in bytes. On output, will contain the number of bytes
        written.

    Buffer - Supplies a pointer to the caller-allocated buffer containing the
        data to write.

Return Value:

    EFI_SUCCESS on success.

    EFI_BAD_BUFFER_SIZE if the write attempted to cross an LBA boundary. On
    output, the byte count will contain the number of bytes written.

    EFI_ACCESS_DENIED if the firmware volume is in the WriteDisabled state.

    EFI_DEVICE_ERROR if the block device is not functioning properly.

--*/

{

    return EFI_UNSUPPORTED;
}

EFIAPI
EFI_STATUS
EfiFvBlockErase (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    ...
    )

/*++

Routine Description:

    This routine erases one or more blocks as denoted by the variable argument
    list. The entire parameter list of blocks must be verified before erasing
    any blocks. If a block is requested that does not exist within the
    associated firmware volume (it has a larger index than the last block of
    the firmware volume), this function must return the status code
    EFI_INVALID_PARAMETER without modifying the contents of the firmware
    volume. Implementations should be mindful that the firmware volume might be
    in the WriteDisabled state. If it is in this state, this function must
    return the status code EFI_ACCESS_DENIED without modifying the contents of
    the firmware volume. All calls to erase blocks must be fully flushed to the
    hardware before this routine returns.

Arguments:

    This - Supplies the protocol instance.

    ... - Supplies the variable argument list, which is a list of tuples. Each
        tuple describes a range of LBAs to erase and consists of the following:
        1) an EFI_LBA that indicates the starting LBA, and 2) a UINTN that
        indicates the number of blocks to erase. The list is terminated with
        an EFI_LBA_LIST_TERMINATOR. For example, to erase blocks 5-7 and
        10-11, this list would contain (5, 3, 10, 2, EFI_LBA_LIST_TERMINATOR).

Return Value:

    EFI_SUCCESS on success.

    EFI_ACCESS_DENIED if the firmware volume is in the WriteDisabled state.

    EFI_INVALID_PARAMETER if one or more of the LBAs listed in the variable
    argument list do not exist on the firmware block device.

    EFI_DEVICE_ERROR if the block device is not functioning properly.

--*/

{

    return EFI_UNSUPPORTED;
}

