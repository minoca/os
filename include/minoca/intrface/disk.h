/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    disk.h

Abstract:

    This header contains definitions for the disk block device access interface.

Author:

    Chris Stevens 25-Aug-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define UUID_DISK_INTERFACE \
    {{0xC0C4064E, 0x11E42CAA, 0x7401B591, 0x04010FDD}}

#define DISK_INTERFACE_VERSION 0x00010000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
KSTATUS
(*PDISK_BLOCK_IO_INITIALIZE) (
    PVOID DiskToken
    );

/*++

Routine Description:

    This routine must be called before using the block read and write routines
    in order to allow the disk to prepare for block I/O. This must be called at
    low level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PDISK_BLOCK_IO_RESET) (
    PVOID DiskToken
    );

/*++

Routine Description:

    This routine must be called immediately before using the block read and
    write routines in order to allow the disk to reset any I/O channels in
    preparation for imminent block I/O. This routine is called at high run
    level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PDISK_BLOCK_IO_READ) (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

/*++

Routine Description:

    This routine reads the block contents from the disk into the given I/O
    buffer using polled I/O. It does so without acquiring any locks or
    allocating any resources, as this routine is used for crash dump support
    when the system is in a very fragile state. It must be called at high level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

    IoBuffer - Supplies a pointer to the I/O buffer where the data will be read.

    BlockAddress - Supplies the block index to read (for physical disk, this is
        the LBA).

    BlockCount - Supplies the number of blocks to read.

    BlocksCompleted - Supplies a pointer that receives the total number of
        blocks read.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PDISK_BLOCK_IO_WRITE) (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

/*++

Routine Description:

    This routine writes the contents of the given I/O buffer to the disk using
    polled I/O. It does so without acquiring any locks or allocating any
    resources, as this routine is used for crash dump support when the system
    is in a very fragile state. It must be called at high level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        write.

    BlockAddress - Supplies the block index to write to (for physical disk,
        this is the LBA).

    BlockCount - Supplies the number of blocks to write.

    BlocksCompleted - Supplies a pointer that receives the total number of
        blocks written.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure stores the disk device interface published by disk devices.

Members:

    Version - Stores the table version. Future revisions will be backwards
        compatible. Set to DISK_INTERFACE_VERSION.

    DiskToken - Stores an opaque token to disk device context.

    BlockSize - Stores the size of each block on the disk.

    BlockCount - Stores the total number of blocks on the disk.

    BlockIoInitialize - Stores a pointer to a routine used to prepare for
        block-level I/O to the disk.

    BlockIoReset - Stores a pointer to a routine that allows the device to
        reset any I/O paths in preparation for imminent block I/O.

    BlockIoRead - Stores a pointer to a routine that can do direct block-level
        reads from the disk in a limited resource environment, such as when the
        system crashes.

    BlockIoWrite - Stores a pointer to a routine that can do direct block-level
        writes to the disk in a limited resource environment, such as when the
        system crashes.

--*/

typedef struct _DISK_INTERFACE {
    ULONG Version;
    PVOID DiskToken;
    ULONG BlockSize;
    ULONGLONG BlockCount;
    PDISK_BLOCK_IO_INITIALIZE BlockIoInitialize;
    PDISK_BLOCK_IO_RESET BlockIoReset;
    PDISK_BLOCK_IO_READ BlockIoRead;
    PDISK_BLOCK_IO_WRITE BlockIoWrite;
} DISK_INTERFACE, *PDISK_INTERFACE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

