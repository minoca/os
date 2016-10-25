/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    partfmt.h

Abstract:

    This header contains definitions for the various partition formats
    supported by UEFI.

Author:

    Evan Green 19-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// GPT partition definitions
//

#define EFI_PARTITION_TYPE_UNUSED_GUID                      \
    {                                                       \
        0x00000000, 0x0000, 0x0000,                         \
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}    \
    }

#define EFI_PARTITION_TYPE_EFI_SYSTEM_GUID                  \
    {                                                       \
        0xC12A7328, 0xF81F, 0x11D2,                         \
        {0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}    \
    }

#define EFI_PARTITION_TYPE_LEGACY_MBR_GUID                  \
    {                                                       \
        0x024DEE41, 0x33E7, 0x11D3,                         \
        {0x9D, 0x69, 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F}    \
    }

//
// The primary GPT header must be at LBA 1 (the second logical block).
//

#define EFI_PRIMARY_PARTITION_HEADER_LBA 1

//
// Define the EFI GPT header signature, "EFI PART".
//

#define EFI_GPT_HEADER_SIGNATURE 0x5452415020494645ULL

#define EFI_GPT_ATTRIBUTE_OS_SPECIFIC (1 << 1)

//
// El Torito definitions
//

#define EFI_CD_BLOCK_SIZE 2048
#define EFI_CD_VOLUME_RECORD_LBA 16

#define EFI_CD_VOLUME_TYPE_STANDARD 0x0
#define EFI_CD_VOLUME_TYPE_CODED    0x1
#define EFI_CD_VOLUME_TYPE_END      0xFF

#define EFI_CD_VOLUME_ID  "CD001"
#define EFI_CD_VOLUME_ELTORITO_ID "EL TORITO SPECIFICATION"

//
// Indicator types
//

#define EFI_ELTORITO_ID_CATALOG               0x01
#define EFI_ELTORITO_ID_SECTION_BOOTABLE      0x88
#define EFI_ELTORITO_ID_SECTION_NOT_BOOTABLE  0x00
#define EFI_ELTORITO_ID_SECTION_HEADER        0x90
#define EFI_ELTORITO_ID_SECTION_HEADER_FINAL  0x91

//
// Boot media types.
//

#define EFI_ELTORITO_NO_EMULATION 0x00
#define EFI_ELTORITO_12_DISKETTE  0x01
#define EFI_ELTORITO_14_DISKETTE  0x02
#define EFI_ELTORITO_28_DISKETTE  0x03
#define EFI_ELTORITO_HARD_DISK    0x04

//
// MBR partition definitions
//

#define EFI_MBR_SIGNATURE               0xAA55
#define EFI_EXTENDED_DOS_PARTITION      0x05
#define EFI_EXTENDED_WINDOWS_PARTITION  0x0F
#define EFI_PROTECTIVE_MBR_PARTITION    0xEE
#define EFI_PARTITION                   0xEF
#define EFI_MAX_MBR_PARTITIONS          4
#define EFI_MBR_SIZE                    512

//
// ------------------------------------------------------ Data Type Definitions
//

//
// GPT partition structures
//

/*++

Structure Description:

    This structure defines a GPT partition table header.

Members:

    Header - Stores the common header portion of any EFI table.

    MyLba - Stores the LBA that contains this structure.

    AlternateLba - Stores the LBA of the other copy of this structure.

    FirstUsableLba - Stores the LBA of the first usable block that may be used
        by a partition described in a partition entry.

    LastUsableLba - Stores the last usable block that may be used by a
        partition described in a partition entry.

    DiskGuid - Stores a GUID that can be used to uniquely identify the disk.

    PartitionEntryLba - Stores the starting LBA of the partition entry array.

    NumberOfPartitionEntries - Stores the number of entries in the partition
        entry array.

    SizeOfPartitionEntry - Stores the size in bytes of each GUID partition
        entry structure. This must be a multiple of 128.

    PartitionEntryArrayCrc32 - Stores the CRC32 of the partition entry array.
        This starts at the partition entry LBA and is computed over a length
        of the number of partition entries times the size of a partition
        entry.

--*/

typedef struct _EFI_PARTITION_TABLE_HEADER {
    EFI_TABLE_HEADER Header;
    EFI_LBA MyLba;
    EFI_LBA AlternateLba;
    EFI_LBA FirstUsableLba;
    EFI_LBA LastUsableLba;
    EFI_GUID DiskGuid;
    EFI_LBA PartitionEntryLba;
    UINT32 NumberOfPartitionEntries;
    UINT32 SizeOfPartitionEntry;
    UINT32 PartitionEntryArrayCrc32;
} PACKED EFI_PARTITION_TABLE_HEADER, *PEFI_PARTITION_TABLE_HEADER;

/*++

Structure Description:

    This structure defines a GPT partition entry.

Members:

    PartitionTypeGuid - Stores a unique ID that identifies the purpose and type
        of this partition. A valid of zero indicates the partition entry is
        not being used.

    UniquePartitionGuid - Stores a unique identifier for each partition entry.
        Every partition ever created will have a unique ID. This GUID is
        assigned when the partition entry is created.

    StartingLba - Stores the starting LBA of the partition.

    EndingLba - Stores the ending LBA of the partition, inclusive.

    Attributes - Stores attribute bits.

    PartitionName - Stores a null-terminated human readable name for the
        partition.

--*/

typedef struct _EFI_PARTITION_ENTRY {
    EFI_GUID PartitionTypeGuid;
    EFI_GUID UniquePartitionGuid;
    EFI_LBA StartingLba;
    EFI_LBA EndingLba;
    UINT64 Attributes;
    CHAR16 PartitionName[36];
} PACKED EFI_PARTITION_ENTRY, *PEFI_PARTITION_ENTRY;

//
// El Torito data structures
//

/*++

Structure Description:

    This structure defines the boot record volume descriptor, defined in the
    "El Torito" specification. What a great name, El Torrrrrrrito!

Members:

    Type - Stores the value zero for this type.

    Id - Stores the ASCII string "CD001".

    Version - Stores the constant one.

    SystemId - Stores the ASCII string "EL TORITO SPECIFICATION".

    Unused - Stores unused bytes that should be zero.

    Catalog - Stores the absolute first sector of the Boot Catalog.

    Unused2 - Stores more unused bytes that should be zero.

--*/

typedef struct _EFI_CDROM_BOOT_VOLUME_DESCRIPTOR {
    UINT8 Type;
    CHAR8 Id[5];
    UINT8 Version;
    CHAR8 SystemId[32];
    CHAR8 Unused[32];
    UINT8 Catalog[4];
    CHAR8 Unused2[13];
} PACKED EFI_CDROM_BOOT_VOLUME_DESCRIPTOR, *PEFI_CDROM_BOOT_VOLUME_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the primary volume descriptor, defined in ISO 9660,
    a much more boring name.

Members:

    Type - Stores the volume type.

    Id - Stores the ASCII string "CD001".

    Unused - Stores an unused value that should be zero.

    SystemId - Stores the system ID string.

    VolumeId - Stores the volume ID string.

    Unused2 - Stores more bytes that should be zero.

    VolumeSize - Stores the number of logical blocks in the volume.

--*/

typedef struct _EFI_CDROM_PRIMARY_VOLUME_DESCRIPTOR {
    UINT8 Type;
    CHAR8 Id[5];
    UINT8 Version;
    UINT8 Unused;
    CHAR8 SystemId[32];
    CHAR8 VolumeId[32];
    UINT8 Unused2[8];
    UINT32 VolumeSize[2];
} PACKED EFI_CDROM_PRIMARY_VOLUME_DESCRIPTOR,
                                         *PEFI_CDROM_PRIMARY_VOLUME_DESCRIPTOR;

/*++

Structure Description:

    This union defines the format of a CD-ROM volume descriptor.

Members:

    BootRecordVolume - Stores the boot record volume.

    PrimaryVolume - Stores the primary volume record.

--*/

typedef union _EFI_CDROM_VOLUME_DESCRIPTOR {
    EFI_CDROM_BOOT_VOLUME_DESCRIPTOR BootRecordVolume;
    EFI_CDROM_PRIMARY_VOLUME_DESCRIPTOR PrimaryVolume;
} EFI_CDROM_VOLUME_DESCRIPTOR, *PEFI_CDROM_VOLUME_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the El Torito catalog validation entry.

Members:

    Type - Stores the constant one.

    PlatformId - Stores a platform identifier byte.

    Reserved - Stores an unused value.

    ManufacturerId - Stores the string identifier of the manufacturer.

    Checksum - Stores the checksum of the catalog.

    Id55AA - Stores the constant values 0x55 and 0xAA.

--*/

typedef struct _EFI_ELTORITO_CATALOG_DATA {
    UINT8 Indicator;
    UINT8 PlatformId;
    UINT16 Reserved;
    CHAR8 ManufacturerId[24];
    UINT16 Checksum;
    UINT16 Id55AA;
} PACKED EFI_ELTORITO_CATALOG_DATA, *PEFI_ELTORITO_CATALOG_DATA;

/*++

Structure Description:

    This structure defines the El Torito initial/default entry.

Members:

    Indicator - Stores the value 0x88 for a bootable volume, or 0 for a
        non-bootable volume.

    MediaType - Stores the media type.

    LoadSegment - Stores the load segment.

    SystemType - Stores the system type.

    Reserved2 - Stores a reserved value that must be zero.

    Lba - Stores the LBA of the boot data.

--*/

typedef struct _EFI_ELTORITO_BOOT_DATA {
    UINT8 Indicator;
    UINT8 MediaType;
    UINT16 LoadSegment;
    UINT8 SystemType;
    UINT8 Reserved2;
    UINT16 SectorCount;
    UINT32 Lba;
} PACKED EFI_ELTORITO_BOOT_DATA, *PEFI_ELTORITO_BOOT_DATA;

/*++

Structure Description:

    This structure defines the El Torito section header entry.

Members:

    Indicator - Stores 0x90 for a header where more data follows, or 0x91 for
        the final header.

    PlatformId - Stores the platform ID byte.

    SectionEntries - Stores the number of section entries following this header.

    Id - Stores the identifier string of this section.

--*/

typedef struct _EFI_ELTORITO_SECTION {
    UINT8 Indicator;
    UINT8 PlatformId;
    UINT16 SectionEntries;
    CHAR8 Id[28];
} PACKED EFI_ELTORITO_SECTION, *PEFI_ELTORITO_SECTION;

/*++

Structure Description:

    This union defines an El Torito catalog entry.

Members:

    Catalog - Stores the catalog validation entry (catalog header).

    Boot - Stores the initial/default entry.

    Section - Stores the section header entry.

--*/

typedef union _EFI_ELTORITO_CATALOG {
    EFI_ELTORITO_CATALOG_DATA Catalog;
    EFI_ELTORITO_BOOT_DATA Boot;
    EFI_ELTORITO_SECTION Section;
} EFI_ELTORITO_CATALOG, *PEFI_ELTORITO_CATALOG;

//
// MBR data structures
//

/*++

Structure Description:

    This structure defines the format of an MBR partition table entry.

Members:

    BootIndicator - Stores 0x00 for an inactive entry or 0x80 for an active
        entry.

    StartHead - Stores the starting head number of the partition.

    StartSector - Stores the starting sector number of the partition.

    StartTrack - Stores the starting track number of the partition.

    OsIndicator - Stores a wild west byte that tries to define the OS and/or
        file system on the partition.

    EndHead - Stores the ending head of the partition, inclusive.

    EndSector - Stores the ending sector of the partition, inclusive.

    EndTrack - Stores the ending track of the partition, inclusive.

    StartingLba - Stores the starting logical block address of the partition.

    SizeInLba - Stores the number of logical block in the partition.

--*/

typedef struct _EFI_MBR_PARTITION_RECORD {
    UINT8 BootIndicator;
    UINT8 StartHead;
    UINT8 StartSector;
    UINT8 StartTrack;
    UINT8 OsIndicator;
    UINT8 EndHead;
    UINT8 EndSector;
    UINT8 EndTrack;
    UINT8 StartingLba[4];
    UINT8 SizeInLba[4];
} PACKED EFI_MBR_PARTITION_RECORD, *PEFI_MBR_PARTITION_RECORD;

/*++

Structure Description:

    This structure defines the first 512 bytes of an MBR partitioned disk. GPT
    disks also follow this format.

Members:

    BootStrapCode - Stores 440 bytes of undefined data, usually x86 bootstrap
        code.

    UniqueMbrSignature - Stores the signature word of the MBR.

    Unknown - Stores two undefined bytes.

    Partition - Stores the partition records.

    Signature - Stores the constant 0xAA55.

--*/

typedef struct _EFI_MASTER_BOOT_RECORD {
    UINT8 BootStrapCode[440];
    UINT8 UniqueMbrSignature[4];
    UINT8 Unknown[2];
    EFI_MBR_PARTITION_RECORD Partition[EFI_MAX_MBR_PARTITIONS];
    UINT16 Signature;
} PACKED EFI_MASTER_BOOT_RECORD, *PEFI_MASTER_BOOT_RECORD;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
