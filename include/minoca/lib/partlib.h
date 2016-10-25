/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    partlib.h

Abstract:

    This header contains definitions for using the partition support library.

Author:

    Evan Green 30-Jan-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/devinfo/part.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a particular partition.

Members:

    StartOffset - Stores the starting offset of the partition (the address of
        the first block of the partition).

    EndOffset - Stores the ending offset of the partition, exclusive (the
        first address just beyond the end of the partition).

    Attributes - Stores a bitfield of attributes about the partition.

    Number - Stores a partition number, starting at 1.

    ParentNumber - Stores the partition number of the extended partition this
        logical partition is in. This parameter is unused for anything other
        than logical partitions.

    Flags - Stores a bitfield of flags about the partition. See
        PARTITION_FLAG_* definitions.

    PartitionType - Stores a recognized partition type.

    TypeIdentifier - Stores the partition type identifier. For MBR disks this
        is just the system ID byte, for GPT disks this is the partition type
        GUID.

    Identifier - Stores the partition identifier. For MBR disks this contains
        the partition number and disk signature (a cobbled together identifier).
        For GPT disks this contains the unique partition GUID.

--*/

typedef struct _PARTITION_INFORMATION {
    ULONGLONG StartOffset;
    ULONGLONG EndOffset;
    ULONGLONG Attributes;
    ULONG Number;
    ULONG ParentNumber;
    ULONG Flags;
    PARTITION_TYPE PartitionType;
    UCHAR TypeIdentifier[PARTITION_TYPE_SIZE];
    UCHAR Identifier[PARTITION_IDENTIFIER_SIZE];
} PARTITION_INFORMATION, *PPARTITION_INFORMATION;

typedef struct _PARTITION_CONTEXT PARTITION_CONTEXT, *PPARTITION_CONTEXT;

//
// Define functions called by the partition library.
//

typedef
PVOID
(*PPARTITION_ALLOCATE) (
    UINTN Size
    );

/*++

Routine Description:

    This routine is called when the partition library needs to allocate memory.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

typedef
VOID
(*PPARTITION_FREE) (
    PVOID Memory
    );

/*++

Routine Description:

    This routine is called when the partition library needs to free allocated
    memory.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PPARTITION_READ) (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    );

/*++

Routine Description:

    This routine is called when the partition library needs to read a sector
    from the disk.

Arguments:

    Context - Supplies the partition context identifying the disk.

    BlockAddress - Supplies the block address to read.

    Buffer - Supplies a pointer where the data will be returned on success.
        This buffer is expected to be one block in size (as specified in the
        partition context).

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PPARTITION_WRITE) (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    );

/*++

Routine Description:

    This routine is called when the partition library needs to write a sector
    to the disk.

Arguments:

    Context - Supplies the partition context identifying the disk.

    BlockAddress - Supplies the block address to read.

    Buffer - Supplies a pointer where the data will be returned on success.
        This buffer is expected to be one block in size (as specified in the
        partition context).

Return Value:

    Status code.

--*/

typedef
VOID
(*PPARTITION_FILL_RANDOM) (
    PPARTITION_CONTEXT Context,
    PUCHAR Buffer,
    ULONG BufferSize
    );

/*++

Routine Description:

    This routine is called when the partition library needs to fill a buffer
    with random bytes.

Arguments:

    Context - Supplies the partition context identifying the disk.

    Buffer - Supplies a pointer to the buffer to fill with random bytes.

    BufferSize - Supplies the size of the buffer in bytes.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores information about a partition manager context to a
    disk.

Members:

    AllocateFunction - Stores a pointer to a function the partition library
        uses to allocate memory.

    FreeFunction - Stores a pointer to a function the partition library uses to
        free previously allocated memory.

    ReadFunction - Stores a pointer to a function the partition library uses to
        read a block from the disk.

    WriteFunction - Stores a pointer to a function the partition library uses
        to write a block to the disk. This is optional if editing partitions
        is not needed.

    FillRandomFunction - Stores a pointer to a function the partition library
        uses to fill a buffer with random data.

    BlockSize - Stores the block size of the disk.

    BlockShift - Stores the power of two of the block size.

    Alignment - Stores the required alignment of buffers that do I/O. Zero or
        one specifies no alignment requirement.

    BlockCount - Stores the number of blocks in the disk (one beyond the last
        valid LBA).

    SectorsPerHead - Stores the maximum valid sector number in legacy CHS
        geometry. This value probably shouldn't be greater than 63.

    HeadsPerCylinder - Stores the number of heads per cylinder for legacy CHS
        geometry.

    Format - Stores the partition format of this disk.

    DiskIdentifier - Stores the disk identifier information.

    PartitionCount - Stores the number of partitions on this disk.

    Partitions - Stores a pointer to the array of partition information
        structures describing this disk.

--*/

struct _PARTITION_CONTEXT {
    PPARTITION_ALLOCATE AllocateFunction;
    PPARTITION_FREE FreeFunction;
    PPARTITION_READ ReadFunction;
    PPARTITION_WRITE WriteFunction;
    PPARTITION_FILL_RANDOM FillRandomFunction;
    ULONG BlockSize;
    ULONG BlockShift;
    ULONG Alignment;
    ULONGLONG BlockCount;
    ULONG SectorsPerHead;
    ULONG HeadsPerCylinder;
    PARTITION_FORMAT Format;
    UCHAR DiskIdentifier[DISK_IDENTIFIER_SIZE];
    ULONG PartitionCount;
    PPARTITION_INFORMATION Partitions;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
PartInitialize (
    PPARTITION_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes a partition context. The caller is expected to
    have filled in pointers to the allocate, free, and read sector functions.
    The caller is also expected to have filled in the block size and disk
    geometry information (if needed).

Arguments:

    Context - Supplies a pointer to the context, partially initialized by the
        caller.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the context was not initialized properly by
    the caller.

--*/

VOID
PartDestroy (
    PPARTITION_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys a partition context.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

KSTATUS
PartEnumeratePartitions (
    PPARTITION_CONTEXT Context
    );

/*++

Routine Description:

    This routine is called to read the partition information from the disk
    and enumerate the list of partitions. The caller must have just called
    the initialize context function.

Arguments:

    Context - Supplies a pointer to the initialized context.

Return Value:

    STATUS_SUCCESS if the partition information could be determined. There
    could still be zero partitions in this case.

    STATUS_NO_ELIGIBLE_DEVICES if the partition table is invalid.

    Error codes on device read or allocation failure.

--*/

KSTATUS
PartWritePartitionLayout (
    PPARTITION_CONTEXT Context,
    PARTITION_FORMAT Format,
    PPARTITION_INFORMATION Partitions,
    ULONG PartitionCount,
    BOOL CleanMbr
    );

/*++

Routine Description:

    This routine writes a partition layout to the disk. This usually wipes out
    all data on the disk.

Arguments:

    Context - Supplies a pointer to the partition context.

    Format - Supplies the partition format to use.

    Partitions - Supplies a pointer to the new partition layout.

    PartitionCount - Supplies the number of partitions in the new layout.

    CleanMbr - Supplies a boolean indicating if only the partition entries of
        the MBR should be modified (FALSE) or if the whole MBR should be
        zeroed before being written (TRUE).

Return Value:

    STATUS_SUCCESS if the valid block count is non-zero.

    STATUS_OUT_OF_BOUNDS if the block address is beyond the end of the
    partition.

--*/

KSTATUS
PartTranslateIo (
    PPARTITION_INFORMATION Partition,
    PULONGLONG BlockAddress,
    PULONGLONG BlockCount
    );

/*++

Routine Description:

    This routine performs a translation from a partition-relative offset to a
    global disk offset.

Arguments:

    Partition - Supplies a pointer to the partition to translate for.

    BlockAddress - Supplies a pointer that on input contains the
        partition-relative block address. On successful output, this will
        contain the globla address.

    BlockCount - Supplies a pointer that on input contains the number of blocks
        to read or write. On output, the number of valid blocks will be
        returned. This number may be reduced on output if the caller tried to
        do I/O off the end of the partition.

Return Value:

    STATUS_SUCCESS if the valid block count is non-zero.

    STATUS_OUT_OF_BOUNDS if the block address is beyond the end of the
    partition.

--*/

PARTITION_TYPE
PartConvertToPartitionType (
    PARTITION_FORMAT Format,
    UCHAR PartitionTypeId[PARTITION_TYPE_SIZE]
    );

/*++

Routine Description:

    This routine converts a partition type ID into a known partition type.

Arguments:

    Format - Supplies the format. Valid values are MBR and GPT.

    PartitionTypeId - Supplies the partition type ID bytes.

Return Value:

    Returns the partition type that corresponds with the given partition
    type ID.

    PartitionTypeInvalid if the format is invalid.

    PartitionTypeUnknown if the partition type ID is unknown.

--*/
