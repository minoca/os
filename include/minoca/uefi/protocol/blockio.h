/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    blockio.h

Abstract:

    This header contains definitions for the UEFI Block I/O Protocol.

Author:

    Evan Green 8-Feb-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_BLOCK_IO_PROTOCOL_GUID                          \
    {                                                       \
        0x964E5B21, 0x6459, 0x11D2,                         \
        {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}    \
    }

//
// Protocol GUID name defined in EFI1.1.
//

#define BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL_GUID

#define EFI_BLOCK_IO_PROTOCOL_REVISION  0x00010000
#define EFI_BLOCK_IO_PROTOCOL_REVISION2 0x00020001
#define EFI_BLOCK_IO_PROTOCOL_REVISION3 0x00020031

//
// Revision defined in EFI1.1.
//

#define EFI_BLOCK_IO_INTERFACE_REVISION EFI_BLOCK_IO_PROTOCOL_REVISION

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;

//
// Protocol defined in EFI1.1.
//

typedef EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO;

typedef
EFI_STATUS
(EFIAPI *EFI_BLOCK_RESET) (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_BLOCK_READ) (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_BLOCK_WRITE) (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

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

typedef
EFI_STATUS
(EFIAPI *EFI_BLOCK_FLUSH) (
    EFI_BLOCK_IO_PROTOCOL *This
    );

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

/*++

Structure Description:

    This structure defines the Block I/O Media information structure.

Members:

    MediaId - Stores the current media identifier. This changes each time
        media is inserted.

    RemovableMedia - Stores a boolean indicating if the media is removable.

    MediaPresent - Stores a boolean indicating if there is media in the device.
        This shows media present status as of the most recent read or write
        blocks call.

    LogicalPartition - Stores a boolean indicating if LBA zero is the first
        block of a partition. This is TRUE for media with only one partition.

    ReadOnly - Stores a boolean indicating if the media cannot be written to.

    WriteCaching - Stores a boolean indicating if the write block function
        caches data.

    BlockSize - Stores the intrinsic block size of the device. This field is
        updated if the media changes.

    IoAlign - Stores the alignment required for any I/O buffer.

    LastBlock - Stores the last block on the device. If the media changes, this
        field is updated.

    LowestAlignedLba - Stores the first LBA aligned to a physical block
        boundary. This is only present if the revision is
        EFI_BLOCK_IO_PROTOCOL_REVISION2 or higher.

    LogicalBlocksPerPhysicalBlock - Stores the number of logical blocks per
        physical block. This is only present if the revision is
        EFI_BLOCK_IO_PROTOCOL_REVISION2 or higher.

    OptimalTransferLengthGranularity - Stores the optimal transfer length
        granularity as a number of transfer blocks. This is only present if the
        revision is EFI_BLOCK_IO_PROTOCOL_REVISION2 or higher.

--*/

typedef struct {
    UINT32 MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCaching;
    UINT32 BlockSize;
    UINT32 IoAlign;
    EFI_LBA LastBlock;
    EFI_LBA LowestAlignedLba;
    UINT32 LogicalBlocksPerPhysicalBlock;
    UINT32 OptimalTransferLengthGranularity;
} EFI_BLOCK_IO_MEDIA;

/*++

Structure Description:

    This structure defines the Block I/O Protocol structure.

Members:

    Revision - Stores the protocol revision number. All future revisions are
        backwards compatible.

    Media - Stores a pointer to the media information.

    Reset - Stores a pointer to a function used to reset the device.

    ReadBlocks - Stores a pointer to a function used to read blocks from the
        device.

    WriteBlocks - Stores a pointer to a function used to write blocks to the
        device.

    FlushBlocks - Stores a pointer to a function used to flush blocks to the
        device.

--*/

struct _EFI_BLOCK_IO_PROTOCOL {
    UINT64 Revision;
    EFI_BLOCK_IO_MEDIA *Media;
    EFI_BLOCK_RESET Reset;
    EFI_BLOCK_READ ReadBlocks;
    EFI_BLOCK_WRITE WriteBlocks;
    EFI_BLOCK_FLUSH FlushBlocks;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

