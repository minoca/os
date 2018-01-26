/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    partlibp.h

Abstract:

    This header contains internal definitions for the partition library.

Author:

    Evan Green 6-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/lib/partlib.h>

//
// ---------------------------------------------------------------- Definitions
//

#define PARTITION_SIGNATURE 0xAA55
#define PARTITION_SIGNATURE_OFFSET 0x1FE

//
// Define the minimum block size for a disk.
//

#define MINIMUM_BLOCK_SIZE 512

//
// Define the MBR disk identifier offset.
//

#define MBR_DISK_ID_OFFSET 0x1B8
#define MBR_DISK_ID_SIZE 4

//
// Define the offset of the partition table.
//

#define PARTITION_TABLE_OFFSET 0x1BE

//
// Define the number of entries in a partition table.
//

#define PARTITION_TABLE_SIZE 4

//
// Define the initial allocation size for the partition information array.
//

#define INITIAL_PARTITION_INFORMATION_CAPACITY 4

//
// Define the boot flag for MBR style partitions.
//

#define MBR_PARTITION_BOOT 0x80

//
// Define the maximum cylinder number.
//

#define MBR_MAX_CYLINDER 0x3FF

//
// Define the GPT header signature.
//

#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL
#define GPT_HEADER_REVISION_1 0x00010000

//
// Define the size of a GPT GUID.
//

#define GPT_GUID_SIZE 16

//
// Define the minimum size of the GPT partition entries array.
//

#define GPT_MINIMUM_PARTITION_ENTRIES_SIZE (16 * 1024)

//
// Define the desired alignment for partition start values.
//

#define GPT_PARTITION_ALIGNMENT (4 * 1024)

//
// Define some well known GPT disk GUIDs.
//

#define GPT_PARTITION_TYPE_EMPTY \
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

#define GPT_PARTITION_TYPE_EFI_SYSTEM \
    {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, \
     0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}

#define GPT_PARTITION_TYPE_MINOCA \
    {0xCC, 0x07, 0xA3, 0xCE, 0xBD, 0x78, 0x40, 0x6E, \
     0x81, 0x62, 0x60, 0x20, 0xAF, 0xB8, 0x8D, 0x17}

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the standard partition table entry format for MBR
    formatted disks.

Members:

    BootIndicator - Stores either 0 (not the boot partition) or 0x80 (the
        active/boot partition).

    StartingHead - Stores the head number of the first sector of the partition
        in legacy CHS geometry.

    StartingSector - Stores the sector number of the first sector of the
        partition in legacy CHS geometry. Actually bits 0-5 stores the
        sector number, bits 6 and 7 are the high bits of the starting cylinder
        number.

    StartingCylinder - Stores the cylinder number of the first sector of the
        partition in legacy CHS geometry. Actually this really stores the low
        8 bits, the high 2 bits are in the upper bits of the starting sector.

    SystemId - Stores the wild west system ID byte. No standard ever came for
        this byte.

    EndingHead - Stores the head number of hte last sector of the partition
        (inclusive) in legacy CHS geometry.

    EndingSector - Stores the sector number of the last sector of the partition
        (inclusive) in legacy CHS geometry. Bits 0-5 store the sector, bits
        6 and 7 stores the lowest 2 bits of the ending cylinder.

    EndingCylinder - Stores the cylinder number of the last cylinder of the
        partition (inclusive) in legacy CHS geometry. This is actually the
        low 8 bits of the value, the highest two bits are in the upper bits of
        the ending sector.

    StartingLba - Stores the Logical Block Address of the first sector of the
        disk. This is the value everybody uses, and it's also the one that
        limits MBR disks to 2TB. This value is relative, but what it's relative
        to depends on which table and entry it is.

    SectorCount - Stores the number of sectors in the partition. This is the
        value everybody uses, but again is limited to 2TB.

--*/

#pragma pack(push, 1)

typedef struct _PARTITION_TABLE_ENTRY {
    UCHAR BootIndicator;
    UCHAR StartingHead;
    UCHAR StartingSector;
    UCHAR StartingCylinder;
    UCHAR SystemId;
    UCHAR EndingHead;
    UCHAR EndingSector;
    UCHAR EndingCylinder;
    ULONG StartingLba;
    ULONG SectorCount;
} PACKED PARTITION_TABLE_ENTRY, *PPARTITION_TABLE_ENTRY;

/*++

Structure Description:

    This structure defines the header format for GPT disks. Two copies of this
    structure exist, one at LBA 1, and one at the last LBA of the disk. LBA
    stands for Logical Block Address, it is the indexing used by the disk to
    address data.

Members:

    Signature - Stores a constant value, set to GPT_HEADER_SIGNATURE
        ("EFI PART").

    Revision - Stores the revision of the format.

    HeaderSize - Stores the size of this structure (usually 0x5C).

    HeaderCrc32 - Stores the CRC32 of this header structure (up to header size
        bytes). This field is zeroed during calculation.

    Reserved - Stores a reserved value, must be set to zero.

    CurrentLba - Stores the LBA of this header copy.

    BackupLba - Stores the LBA of the other header copy.

    FirstUsableLba - Stores the LBA of the first data sector. This is after all
        the partition entries.

    LastUsableLba - Stores the LBA of the last data sector, inclusive. This is
        before all the backup partition entries.

    DiskGuid - Stores the unique identifier of the disk.

    PartitionEntriesLba - Stores the LBA of the partition entries. This is
        always 2 in the primary copy.

    PartitionEntryCount - Stores the maximum number of entries in the partition
        array.

    PartitionEntrySize - Stores the size of one partition entry element. This
        is usually 128.

    PartitionArrayCrc32 - Stores the CRC32 of the partition array, up to
        PartitionEntryCount * PartitionEntrySize bytes. Additional bytes due
        to the remainder of the block are ignored.

--*/

typedef struct _GPT_HEADER {
    ULONGLONG Signature;
    ULONG Revision;
    ULONG HeaderSize;
    ULONG HeaderCrc32;
    ULONG Reserved;
    ULONGLONG CurrentLba;
    ULONGLONG BackupLba;
    ULONGLONG FirstUsableLba;
    ULONGLONG LastUsableLba;
    UCHAR DiskGuid[GPT_GUID_SIZE];
    ULONGLONG PartitionEntriesLba;
    ULONG PartitionEntryCount;
    ULONG PartitionEntrySize;
    ULONG PartitionArrayCrc32;
} PACKED GPT_HEADER, *PGPT_HEADER;

/*++

Structure Description:

    This structure defines the format of a partition entry in a GPT disk.

Members:

    TypeGuid - Stores the GUID describing the type of the partition.

    Guid - Stores the unique GUID for this partition.

    FirstLba - Stores the first valid logical block address for this partition
        (in little endian format).

    LastLba - Stores the last valid logical block address for this partition
        (in little endian format), inclusive.

    Attributes - Stores a bitfield of attributes about the partition.

    Name - Stores a UTF-16LE human readable name for the partition.

--*/

typedef struct _GPT_PARTITION_ENTRY {
    UCHAR TypeGuid[GPT_GUID_SIZE];
    UCHAR Guid[GPT_GUID_SIZE];
    ULONGLONG FirstLba;
    ULONGLONG LastLba;
    ULONGLONG Attributes;
    USHORT Name[36];
} PACKED GPT_PARTITION_ENTRY, *PGPT_PARTITION_ENTRY;

#pragma pack(pop)

/*++

Structure Description:

    This structure defines a mapping between a system ID byte and a partition
    type.

Members:

    SystemId - Stores the system ID byte.

    PartitionType - Stores the partition type corresponding to this system ID
        value.

--*/

typedef struct _PARTITION_SYSTEM_ID_MAPPING {
    UCHAR SystemId;
    PARTITION_TYPE PartitionType;
} PARTITION_SYSTEM_ID_MAPPING, *PPARTITION_SYSTEM_ID_MAPPING;

/*++

Structure Description:

    This structure defines a mapping between a partition type ID and the type
    enum.

Members:

    TypeGuid - Stores the partition type GUID.

    PartitionType - Stores the partition type corresponding to this partition
        type GUID.

--*/

typedef struct _PARTITION_TYPE_GUID_MAPPING {
    UCHAR TypeGuid[GPT_GUID_SIZE];
    PARTITION_TYPE PartitionType;
} PARTITION_TYPE_GUID_MAPPING, *PPARTITION_TYPE_GUID_MAPPING;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

PVOID
PartpAllocateIo (
    PPARTITION_CONTEXT Context,
    UINTN Size,
    PVOID *AlignedAllocation
    );

/*++

Routine Description:

    This routine allocates a region that will be used for I/O.

Arguments:

    Context - Supplies a pointer to the initialized partition context.

    Size - Supplies the required size of the allocation.

    AlignedAllocation - Supplies a pointer where the aligned buffer will be
        returned.

Return Value:

    Returns the actual buffer to be passed to the free function on success.

    NULL on failure.

--*/

BOOL
PartpGptIsProtectiveMbr (
    PARTITION_TABLE_ENTRY Entry[PARTITION_TABLE_SIZE]
    );

/*++

Routine Description:

    This routine determines if the given partition table is a protective MBR
    for a GPT disk.

Arguments:

    Entry - Supplies the array of MBR partition table entries.

Return Value:

    TRUE if this is a GPT disk.

    FALSE if this is not a GPT disk.

--*/

KSTATUS
PartpGptEnumeratePartitions (
    PPARTITION_CONTEXT Context
    );

/*++

Routine Description:

    This routine is called to read the partition information from the
    GPT-formatted disk and enumerate the list of partitions.

Arguments:

    Context - Supplies a pointer to the initialized context.

Return Value:

    STATUS_SUCCESS if the partition information could be determined. There
    could still be zero partitions in this case.

    STATUS_NO_ELIGIBLE_DEVICES if the partition table is invalid.

    Error codes on device read or allocation failure.

--*/

KSTATUS
PartpGptWritePartitionLayout (
    PPARTITION_CONTEXT Context,
    PPARTITION_INFORMATION Partitions,
    ULONG PartitionCount,
    BOOL CleanMbr
    );

/*++

Routine Description:

    This routine writes a GPT partition layout to the disk. This usually wipes
    out all data on the disk.

Arguments:

    Context - Supplies a pointer to the partition context.

    Partitions - Supplies a pointer to the new partition layout.

    PartitionCount - Supplies the number of partitions in the new layout.

    CleanMbr - Supplies a boolean indicating if only the partition entries of
        the MBR should be modified (FALSE) or if the whole MBR should be
        zeroed before being written (TRUE).

Return Value:

    Status code.

--*/

PARTITION_TYPE
PartpGptConvertTypeGuidToPartitionType (
    UCHAR TypeGuid[GPT_GUID_SIZE]
    );

/*++

Routine Description:

    This routine converts a partition type GUID into a partition type to the
    best of its abilities.

Arguments:

    TypeGuid - Supplies a pointer to the partition type GUID.

Return Value:

    Returns a partition type for this type GUID.

--*/
