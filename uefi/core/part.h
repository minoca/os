/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    part.h

Abstract:

    This header contains internal definitions for the UEFI partition driver.

Author:

    Evan Green 19-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "partfmt.h"
#include <minoca/uefi/protocol/blockio.h>
#include <minoca/uefi/protocol/diskio.h>
#include <minoca/uefi/protocol/drvbind.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the partition data given a pointer to the
// block I/O protocol instance.
//

#define EFI_PARTITION_DATA_FROM_THIS(_BlockIo) \
    PARENT_STRUCTURE(_BlockIo, EFI_PARTITION_DATA, BlockIo)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_PARTITION_DATA_MAGIC 0x74726150 // 'traP'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
EFI_STATUS
(*EFI_PARTITION_DETECT_ROUTINE) (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE Handle,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

/*++

Routine Description:

    This routine attempts to detect a partitioned disk, and exposes child
    block devices for each partition it finds.

Arguments:

    This - Supplies the driver binding protocol instance.

    Handle - Supplies the new controller handle.

    DiskIo - Supplies a pointer to the disk I/O protocol.

    BlockIo - Supplies a pointer to the block I/O protocol.

    DevicePath - Supplies a pointer to the device path.

Return Value:

    EFI status code.

--*/

/*++

Structure Description:

    This structure defines the internal data stored for a partition device.

Members:

    Magic - Stores the magic constant EFI_PARTITION_DATA_MAGIC.

    Handle - Stores the device handle.

    DevicePath - Stores the device path.

    BlockIo - Stores the block I/O protocol.

    Media - Stores the media information.

    ParentDiskIo - Stores a pointer to the disk I/O protocol of the parent.

    ParentBlockIo - Stores a pointer to the block I/O protocol of the parent
        disk.

    Start - Stores the start offset of the logical partition.

    End - Stores the end offset of the logical partition.

    BlockSize - Stores the size of a block on the partition.

    EspGuid - Stores a pointer to the EFI System Partition GUID.

--*/

typedef struct _EFI_PARTITION_DATA {
    UINT64 Magic;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_BLOCK_IO_PROTOCOL BlockIo;
    EFI_BLOCK_IO_MEDIA Media;
    EFI_DISK_IO_PROTOCOL *ParentDiskIo;
    EFI_BLOCK_IO_PROTOCOL *ParentBlockIo;
    UINT64 Start;
    UINT64 End;
    UINT32 BlockSize;
    EFI_GUID *EspGuid;
} EFI_PARTITION_DATA, *PEFI_PARTITION_DATA;

/*++

Structure Description:

    This structure defines the validity status of a GPT partition entry.

Members:

    OutOfRange - Stores a boolean indicating if the GPT partition goes outside
        the valid boundaries of the disk.

    Overlap - Stores a boolean indicating if the GPT partition overlaps with
        another GPT partition.

    OsSpecific - Stores a boolean indicating if the OS-specific attribute (bit
        1) is set and therefore the partition should not be enumerated by
        firmware.

--*/

typedef struct _EFI_PARTITION_ENTRY_STATUS {
    BOOLEAN OutOfRange;
    BOOLEAN Overlap;
    BOOLEAN OsSpecific;
} EFI_PARTITION_ENTRY_STATUS, *PEFI_PARTITION_ENTRY_STATUS;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfiPartitionDetectGpt (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE Handle,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

/*++

Routine Description:

    This routine attempts to detect a GPT partitioned disk, and exposes child
    block devices for each partition it finds.

Arguments:

    This - Supplies the driver binding protocol instance.

    Handle - Supplies the new controller handle.

    DiskIo - Supplies a pointer to the disk I/O protocol.

    BlockIo - Supplies a pointer to the block I/O protocol.

    DevicePath - Supplies a pointer to the device path.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfiPartitionDetectElTorito (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE Handle,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

/*++

Routine Description:

    This routine attempts to detect an El Torito partitioned disk, and exposes
    child block devices for each partition it finds.

Arguments:

    This - Supplies the driver binding protocol instance.

    Handle - Supplies the new controller handle.

    DiskIo - Supplies a pointer to the disk I/O protocol.

    BlockIo - Supplies a pointer to the block I/O protocol.

    DevicePath - Supplies a pointer to the device path.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfiPartitionDetectMbr (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE Handle,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

/*++

Routine Description:

    This routine attempts to detect an El Torito partitioned disk, and exposes
    child block devices for each partition it finds.

Arguments:

    This - Supplies the driver binding protocol instance.

    Handle - Supplies the new controller handle.

    DiskIo - Supplies a pointer to the disk I/O protocol.

    BlockIo - Supplies a pointer to the block I/O protocol.

    DevicePath - Supplies a pointer to the device path.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfiPartitionInstallChildHandle (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ParentHandle,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DEVICE_PATH_PROTOCOL *ParentDevicePath,
    EFI_DEVICE_PATH_PROTOCOL *DevicePathNode,
    EFI_LBA Start,
    EFI_LBA End,
    UINT32 BlockSize,
    BOOLEAN EfiSystemPartition
    );

/*++

Routine Description:

    This routine creates a new partition child handle for a logical block
    device that represents a partition.

Arguments:

    This - Supplies a pointer to the driver binding protocol instance.

    ParentHandle - Supplies the parent handle for the new child.

    DiskIo - Supplies a pointer to the parent disk I/O protocol.

    BlockIo - Supplies a pointer to the block I/O protocol.

    ParentDevicePath - Supplies a pointer to the parent device path.

    DevicePathNode - Supplies the child device path node.

    Start - Supplies the starting LBA of the partition.

    End - Supplies the ending LBA of the partition, inclusive.

    BlockSize - Supplies the disk block size.

    EfiSystemPartition - Supplies a boolean indicating if this is an EFI
        system partition.

Return Value:

    EFI status code.

--*/

