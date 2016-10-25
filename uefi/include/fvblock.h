/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fvblock.h

Abstract:

    This header contains definitions for the UEFI Firmware Volume Block
    Protocol.

Author:

    Evan Green 11-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// This defines the version 2 GUID.
//

#define EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL_GUID             \
    {                                                       \
        0x8F644FA9, 0xE850, 0x4DB1,                         \
        {0x9C, 0xE2, 0x0B, 0x44, 0x69, 0x8E, 0x8D, 0xA4}    \
    }

#define EFI_LBA_LIST_TERMINATOR   0xFFFFFFFFFFFFFFFFULL

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL
    EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_FVB_GET_ATTRIBUTES) (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_FVB_ATTRIBUTES *Attributes
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_FVB_SET_ATTRIBUTES) (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_FVB_ATTRIBUTES *Attributes
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_FVB_GET_PHYSICAL_ADDRESS) (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_PHYSICAL_ADDRESS *Address
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_FVB_GET_BLOCK_SIZE) (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN *BlockSize,
    UINTN *NumberOfBlocks
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_FVB_READ) (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN Offset,
    UINTN *ByteCount,
    UINT8 *Buffer
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_FVB_WRITE) (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    EFI_LBA Lba,
    UINTN Offset,
    UINTN *ByteCount,
    UINT8 *Buffer
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_FVB_ERASE_BLOCKS) (
    CONST EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL *This,
    ...
    );

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

/*++

Structure Description:

    This structure defines the Firmware Volume Block Protocol. It is a
    low-level interface to a firmware volume. File-level access to a firmware
    volume should not be done using the Firmware Volume Block Protocol. Normal
    access to a firmware volume must use the Firmware Volume Protocol.
    Typically, only the file system driver that produces the Firmware Volume
    Protocol will bind to the Firmware Volume Block Protocol.

Members:

    GetAttributes - Stores a pointer to a function used to get block device
        attributes.

    SetAttributes - Stores a pointer to a function used to set block device
        attributes.

    GetPhysicalAddress - Stores a pointer to a function used to get the
        physical memory address of a memory-mapped firmware block device.

    GetBlockSize - Stores a pointer to a function used to get the size of a
        block at a given LBA.

    Read - Stores a pointer to a function used to read blocks from the device.

    Write - Stores a pointer to a function used to write blocks to the device.

    EraseBlocks - Stores a pointer to a function used to erase blocks on the
        device.

    ParentHandle - Stores the handle of the parent firmware volume.

--*/

struct _EFI_FIRMWARE_VOLUME_BLOCK_PROTOCOL {
    EFI_FVB_GET_ATTRIBUTES GetAttributes;
    EFI_FVB_SET_ATTRIBUTES SetAttributes;
    EFI_FVB_GET_PHYSICAL_ADDRESS GetPhysicalAddress;
    EFI_FVB_GET_BLOCK_SIZE GetBlockSize;
    EFI_FVB_READ Read;
    EFI_FVB_WRITE Write;
    EFI_FVB_ERASE_BLOCKS EraseBlocks;
    EFI_HANDLE ParentHandle;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
