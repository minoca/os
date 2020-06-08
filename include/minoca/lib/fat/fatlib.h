/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatlib.h

Abstract:

    This header contains definitions for the File Allocation Table file system.

Author:

    Evan Green 23-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API __DLLEXPORT

#include <minoca/kernel/driver.h>

//
// --------------------------------------------------------------------- Macros
//

//
// FAT12 contain 12 bits per cluster number, so three bytes contains 2 clusters.
// They're packed like this: xxxxxxxx yyyyXXXX YYYYYYYY, where capitals are
// the higher order bits, and x and y are the two clusters. This is strictly
// little endian.
//

#define FAT12_READ_CLUSTER(_Buffer, _Cluster) \
    ((_Cluster) & 0x1) ? \
    (FAT12_READ_SHORT(_Buffer, _Cluster) >> 4) : \
    (FAT12_READ_SHORT(_Buffer, _Cluster) & 0x0FFF)

//
// This macro sets the FAT cluster number to the given value. See the read
// cluster description for the details on FAT12 cluster packing.
//

#define FAT12_WRITE_CLUSTER(_Buffer, _Cluster, _Value)                         \
    if (((_Cluster) & 0x1) != 0) {                                             \
        WRITE_UNALIGNED16(                                                     \
            FAT12_CLUSTER_BYTE(_Buffer, _Cluster),                             \
            (FAT12_READ_SHORT(_Buffer, _Cluster) & 0x000F) | ((_Value) << 4)); \
                                                                               \
    } else {                                                                   \
        WRITE_UNALIGNED16(                                                     \
            FAT12_CLUSTER_BYTE(_Buffer, _Cluster),                             \
            (FAT12_READ_SHORT(_Buffer, _Cluster) & 0xF000) |                   \
             ((_Value) & 0x0FFF));                                             \
    }

//
// This macro reads the two-byte value where the given cluster resides. This
// needs to be potentially shifted and definitely masked to get the right
// 12-bit value.
//

#define FAT12_READ_SHORT(_Buffer, _Cluster) \
    (READ_UNALIGNED16(FAT12_CLUSTER_BYTE(_Buffer, _Cluster)))

//
// This macro computes the byte offset closest to the given cluster in the FAT.
// This could be off by 0 or 4 bits, as it multiplies the cluster number by
// 1.5.
//

#define FAT12_CLUSTER_BYTE(_Buffer, _Cluster) \
    (((PUCHAR)_Buffer) + (_Cluster) + ((_Cluster) >> 1))

//
// ---------------------------------------------------------------- Definitions
//

#define FAT_DEFAULT_ALIGNMENT 4096
#define FAT_MINIMUM_BLOCK_COUNT 36

#define FAT_MEDIA_FLOPPY 0xF0
#define FAT_MEDIA_DISK   0xF8

#define FAT_FIRST_JUMP_BYTE 0xEB
#define FAT_THIRD_JUMP_BYTE 0x90

//
// Define sector cutoffs for determining which FAT format should be used. The
// comparisons should be strictly less than (not less than or equal to).
//

#define FAT12_CLUSTER_CUTOFF 0x0FF5
#define FAT16_CLUSTER_CUTOFF 0xFFF5
#define FAT32_CLUSTER_CUTOFF 0xFFFFFF5

//
// Define the size of the entire FAT12 FAT, aligned up to a 4k boundary.
//

#define FAT12_MAX_FAT_SIZE 8192

//
// Define the value that must be at the end of the boot sector. This isn't
// really specific to FAT but it is required by FAT.
//

#define FAT_BOOT_SIGNATURE 0xAA55

//
// Define the values in the FAT32 Extended Bios Parameter Block that indicate
// it's a FAT32-style EBPB. This doesn't actually prove FAT32, just that the
// FAT32-style EPBP is in use.
//

#define FAT_EXTENDED_BOOT_SIGNATURE 0x29
#define FAT_EXTENDED_BOOT_SIGNATURE2 0x28

//
// Define the signatures in the FS information block.
//

#define FAT32_SIGNATURE1   0x41615252 // 'RRaA'
#define FAT32_SIGNATURE2   0x61417272 // 'rrAa'
#define FAT32_VERSION 0

//
// Define the identifiers in the FAT type.
//

#define FAT_IDENTIFIER   0x2020202020544146 // "FAT     "
#define FAT12_IDENTIFIER 0x2020203231544146 // "FAT12   "
#define FAT16_IDENTIFIER 0x2020203631544146 // "FAT16   "
#define FAT32_IDENTIFIER 0x2020203233544146 // "FAT32   "

//
// Define attributes of a cluster number itself.
//

#define FAT16_CLUSTER_WIDTH_SHIFT 1
#define FAT32_CLUSTER_WIDTH_SHIFT 2

#define FAT16_CLUSTER_WIDTH 2
#define FAT32_CLUSTER_WIDTH 4

//
// File Allocation Entry definitions.
//

#define FAT_CLUSTER_FREE        0x00000000
#define FAT_CLUSTER_RESERVED    0x00000001
#define FAT_CLUSTER_BEGIN       0x00000002

#define FAT12_CLUSTER_BAD       0x0FF7
#define FAT12_CLUSTER_END       0x0FF8
#define FAT12_CLUSTER_END_STAMP 0x0FFF

#define FAT16_CLUSTER_BAD       0xFFF7
#define FAT16_CLUSTER_END       0xFFF8
#define FAT16_CLUSTER_END_STAMP 0xFFFF

#define FAT32_CLUSTER_BAD       0x0FFFFFF7
#define FAT32_CLUSTER_END       0x0FFFFFF8
#define FAT32_CLUSTER_END_STAMP 0x0FFFFFFF

//
// Directory Entry definitions.
//

#define FAT_DIRECTORY_ENTRY_END    0x00
#define FAT_DIRECTORY_ENTRY_E5     0x05
#define FAT_DIRECTORY_ENTRY_ERASED 0xE5

#define FAT_LONG_DIRECTORY_ENTRY_SEQUENCE_MASK 0x1F
#define FAT_LONG_DIRECTORY_ENTRY_END 0x40
#define FAT_LONG_DIRECTORY_ENTRY_NAME1_SIZE 5
#define FAT_LONG_DIRECTORY_ENTRY_NAME2_SIZE 6
#define FAT_LONG_DIRECTORY_ENTRY_NAME3_SIZE 2

//
// File attributes.
//

#define FAT_READ_ONLY    0x01
#define FAT_HIDDEN       0x02
#define FAT_SYSTEM       0x04
#define FAT_VOLUME_LABEL 0x08
#define FAT_SUBDIRECTORY 0x10
#define FAT_ARCHIVE      0x20
#define FAT_LONG_FILE_NAME_ATTRIBUTES \
    (FAT_READ_ONLY | FAT_HIDDEN | FAT_SYSTEM | FAT_VOLUME_LABEL)

//
// Case attributes.
//

#define FAT_CASE_BASENAME_LOWER 0x08
#define FAT_CASE_EXTENSION_LOWER 0x10

//
// Define the length of a short 8.3 entry.
//

#define FAT_FILE_LENGTH 8
#define FAT_FILE_EXTENSION_LENGTH 3
#define FAT_NAME_SIZE (FAT_FILE_LENGTH + FAT_FILE_EXTENSION_LENGTH)

//
// Define the maximum size of a FAT long file name.
//

#define FAT_MAX_LONG_FILE_LENGTH 255

//
// Define the number of characters that fit in a single long file name entry.
//

#define FAT_CHARACTERS_PER_LONG_NAME_ENTRY 13

//
// FAT date and time bits.
//

#define FAT_EPOCH_YEAR                  1980
#define FAT_DATE_YEAR_MASK              0xFE00
#define FAT_DATE_YEAR_SHIFT             9
#define FAT_DATE_MONTH_MASK             0x01E0
#define FAT_DATE_MONTH_SHIFT            5
#define FAT_DATE_DAY_MASK               0x001F
#define FAT_TIME_HOUR_MASK              0xF800
#define FAT_TIME_HOUR_SHIFT             11
#define FAT_TIME_MINUTE_MASK            0x07E0
#define FAT_TIME_MINUTE_SHIFT           5
#define FAT_TIME_SECOND_OVER_TWO_MASK   0x001F

#define FAT_10MS_PER_SECOND 100
#define FAT_NANOSECONDS_PER_10MS \
    (NANOSECONDS_PER_SECOND / FAT_10MS_PER_SECOND)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _FAT_FORMAT {
    Fat12Format,
    Fat16Format,
    Fat32Format
} FAT_FORMAT, *PFAT_FORMAT;

/*++

Structure Description:

    This structure describes the extended BIOS Parameter Block used in FAT12/16.

Members:

    PhysicalDriveNumber - Stores the first field of the extended BIOS paramter
        block. Valid values are 0x00 for removable media, or 0x80 for hard
        disks.

    CurrentHead - Stores a reserved field (rumor has it that NT uses bit 0 of
        this field to determine clean shutdown).

    ExtendedBootSignature - Stores the value 0x29 to indicate the following
        three entries are valid.

    SerialNumber - Stores the ID of the volume.

    VolumeLabel - Stores the volume label, padded with spaces.

    FatType - Stores a string describing the type of FAT file system. Values
        are usually something like "FAT12   ", "FAT16   ", or "FAT32   ".

    BootCode - Stores additional operating system boot code.

    Signature - Stores the value 0xAA55, indicating to system firmware that
        this is a valid boot device.

--*/

#pragma pack(push, 1)

typedef struct _FAT_EXTENDED_BIOS_PARAMETERS {
    BYTE PhysicalDriveNumber;
    BYTE CurrentHead;
    BYTE ExtendedBootSignature;
    ULONG SerialNumber;
    BYTE VolumeLabel[11];
    BYTE FatType[8];
    BYTE BootCode[448];
    USHORT Signature;
} PACKED FAT_EXTENDED_BIOS_PARAMETERS, *PFAT_EXTENDED_BIOS_PARAMETERS;

/*++

Structure Description:

    This structure describes the replacement for the extended BIOS parameter
    block, used by FAT32.

Members:

    SectorsPerFileAllocationTable - Stores the number of sectors in one FAT.

    FatFlags - Stores FAT flags, which are only used during a conversion from
        FAT12/16 to FAT32.

    Version - Stores the version number, defined as 0.

    RootDirectoryCluster - Stores the cluster number of the root directory
        start.

    InformationSector - Stores the sector number of the FS information sector.

    BootSectorCopy - Stores the sector number of a copy of this boot sector, or
        0 if no copy was created.

    Reserved1 - Stores 12 reserved bytes.

    PhysicalDriveNumber - Stores the first field of the extended BIOS paramter
        block. Valid values are 0x00 for removable media, or 0x80 for hard
        disks.

    Reserved2 - Stores a reserved byte (see the CurrentHead field in the
        FAT12/16 BIOS parameter block).

    ExtendedBootSignature - Stores the value 0x29 to indicate the following
        three entries are valid.

    SerialNumber - Stores the ID of the volume.

    VolumeLabel - Stores the volume label, padded with spaces.

    FatType - Stores a string describing the type of FAT file system. This
        should be "FAT32   ".

    BootCode - Stores additional operating system boot code.

    Signature - Stores the value 0xAA55, indicating to system firmware that
        this is a valid boot device.

--*/

typedef struct _FAT32_EXTENDED_PARAMETERS {
    ULONG SectorsPerAllocationTable;
    USHORT FatFlags;
    USHORT Version;
    ULONG RootDirectoryCluster;
    USHORT InformationSector;
    USHORT BootSectorCopy;
    BYTE Reserved1[12];
    BYTE PhysicalDriveNumber;
    BYTE Reserved2;
    BYTE ExtendedBootSignature;
    ULONG SerialNumber;
    BYTE VolumeLabel[11];
    BYTE FatType[8];
    BYTE BootCode[420];
    USHORT Signature;
} PACKED FAT32_EXTENDED_PARAMETERS, *PFAT32_EXTENDED_PARAMETERS;

/*++

Structure Description:

    This structure describes the first sector in a FAT partition. Fields
    greater than offset 0x20 are actually defined in the extended BIOS
    parameter block, but for convience are all being defined here.

Members:

    Jump - Stores the machine code for a jump instruction that jumps over the
        rest of the non-instruction portion of the structure.

    OemName - Stores the Original Equipment Manufacturer name, padded with
        spaces. This value determines in which system the disk was formatted.
        Common values are IBM  3.3, MSDOS5.0, MSWIN4.1, and mkdosfs.

    BytesPerSector - Stores the number of bytes in one sector of the disk. This
        value is commonly 512, especially for IDE devices.

    SectorsPerCluster - Stores the number of sectors in one cluster.

    ReservedSectorCount - Stores the number of sectors before the first FAT in
        the file system image. This should be 1 for FAT12/16, and is usually 32
        for FAT32.

    AllocationTableCount - Stores the number of file allocation tables. This is
        usually 2.

    RootDirectoryCount - Stores the maximum number of root directory entries.
        This is only used in FAT12/16 where the root directory is handled
        specially. This should be 0 for FAT32.

    SmallTotalSectors - Stores the total number of sectors. If this is zero,
        use the BigTotalSectors field.

    MediaDescriptor - Describes the physical media this file system image
        resides on. Most values describe floppy disk parameters, 0xF8 signifies
        a fixed hard disk.

    SectorsPerFileAllocationTable - Stores the number of sectors per File
        Allocation Table for FAT12/16.

    SectorsPerTrack - Stores the number of sectors per track.

    HeadCount - Stores the number of head in the drive.

    HiddenSectors - Stores the number of hidden sectors preceding the partition
        that contains this FAT volume. This field should always be zero on
        media that are not partitioned.

    BigTotalSectors - Stores the number of total sectors in the device, if it
        is greater that 65535. Otherwise, the number of total sectors is stored
        in the SmallTotalSectors field.

--*/

typedef struct FAT_BOOT_SECTOR {
    BYTE Jump[3];
    BYTE OemName[8];
    USHORT BytesPerSector;
    BYTE SectorsPerCluster;
    USHORT ReservedSectorCount;
    BYTE AllocationTableCount;
    USHORT RootDirectoryCount;
    USHORT SmallTotalSectors;
    BYTE MediaDescriptor;
    USHORT SectorsPerFileAllocationTable;
    USHORT SectorsPerTrack;
    USHORT HeadCount;
    ULONG HiddenSectors;
    ULONG BigTotalSectors;
    union {
        FAT_EXTENDED_BIOS_PARAMETERS FatParameters;
        FAT32_EXTENDED_PARAMETERS Fat32Parameters;
    };
} PACKED FAT_BOOT_SECTOR, *PFAT_BOOT_SECTOR;

/*++

Structure Description:

    This structure describes additional information stored for FAT32 volumes.

Members:

    Signature1 - Stores a signature validating that this information is valid.

    Reserved1 - Stores reserved bytes, which should be set to 0.

    Signature2 - Stores another signature validating that this information is
        really valid.

    FreeClusters - Stores the number of free clusters on the volume.

    LastClusterAllocated - Stores the number of the most recently allocated
        cluster.

    Reserved2 - Stores reserved bytes, which should be set to 0.

    BootSignature - Stores the classic 0xAA55 boot signature.

--*/

typedef struct _FAT32_INFORMATION_SECTOR {
    ULONG Signature1;
    BYTE Reserved1[480];
    ULONG Signature2;
    ULONG FreeClusters;
    ULONG LastClusterAllocated;
    BYTE Reserved2[14];
    USHORT BootSignature;
} PACKED FAT32_INFORMATION_SECTOR, *PFAT32_INFORMATION_SECTOR;

/*++

Structure Description:

    This structure describes a traditional short FAT directory entry.

Members:

    DosName - Stores the DOS file name, padded with spaces. The first byte has
        special meaning, potentially describing the entry as available (and no
        subsequent entry is in use), previously erased, or a dot entry (. or
        ..).

    DosExtension - Stores the three character DOS file name extension, padded
        with spaces.

    FileAttributes - Stores attributes of the file, such as read-only, hidden,
        system, directory, or long file name.

    CaseInformation - Stores a reserved field, Wikipedia says that two bits are
        used by NT to encode case information.

    CreationTime10ms - Stores the 10ms offset from the creation time when this
        file was actually created. Valid values are 0 to 199.

    CreationTime - Stores the hour, minute, and second of the file creation
        time. Bits 0-4 store seconds/2 (0-29), bits 5-10 store the minutes
        (0-59), and bits 11-15 store the hours (0-23).

    CreationDate - Stores the day, month, and year of the file creation time.
        Bits 0-4 store the day of the month (0-31), bits 5-8 store the month
        (1-12), and bits 9-15 store the year (0-127, translating to 1980-2107).

    LastAccessDate - Store the day, month, and year of the file's last access.

    ClusterHigh - Stores the EA-index (used by OS/2 and NT), or the high 2
        bytes of the cluster number in FAT32.

    LastModifiedTime - Stores the hour, minute, and second of the last time the
        file was modified.

    LastModifiedDate - Stores the day, month, and year of the last time the file
        was modified.

    ClusterLow - Stores the index of the first cluster in FAT12/16, or the low
        2 bytes of the first cluster in FAT32. Entries with the Volume Label
        flag, subdirectory ".." pointing to root, and empty files with size 0
        should have first cluster 0.

    FileSizeInBytes - Stores the size of the file, in bytes. Entries with the
        volume Label or Subdirectory flag set should have a size of 0.

--*/

typedef struct _FAT_DIRECTORY_ENTRY {
    BYTE DosName[FAT_FILE_LENGTH];
    BYTE DosExtension[FAT_FILE_EXTENSION_LENGTH];
    BYTE FileAttributes;
    BYTE CaseInformation;
    BYTE CreationTime10ms;
    USHORT CreationTime;
    USHORT CreationDate;
    USHORT LastAccessDate;
    USHORT ClusterHigh;
    USHORT LastModifiedTime;
    USHORT LastModifiedDate;
    USHORT ClusterLow;
    ULONG FileSizeInBytes;
} PACKED ALIGNED32 FAT_DIRECTORY_ENTRY, *PFAT_DIRECTORY_ENTRY;

/*++

Structure Description:

    This structure describes a long FAT directory entry.

Members:

    SequenceNumber - Stores the sequence number of the long file name. The
        last entry has the termination bit set.

    Name1 - Stores the first span of long file name characters.

    FileAttributes - Stores attributes of the file, which for long file names
        is always hidden, system, read-only, and volume label.

    Type - Stores the type of special entry, which in this case is zero for
        long file names.

    ShortFileNameChecksum - Stores the checksum of the short file name entry
        following this span of long ones. This ensures that a non long filename
        aware OS hadn't deleted the short file and replaced it with a different
        one.

    Name2 - Stores the second span of long file name characters.

    Cluster - Stores the starting cluster for this entry, which is always set
        to zero to avoid confusing old disk checking utilities.

    Name3 - Stores the third span of name characters.

--*/

typedef struct _FAT_LONG_DIRECTORY_ENTRY {
    BYTE SequenceNumber;
    USHORT Name1[FAT_LONG_DIRECTORY_ENTRY_NAME1_SIZE];
    BYTE FileAttributes;
    BYTE Type;
    BYTE ShortFileNameChecksum;
    USHORT Name2[FAT_LONG_DIRECTORY_ENTRY_NAME2_SIZE];
    USHORT Cluster;
    USHORT Name3[FAT_LONG_DIRECTORY_ENTRY_NAME3_SIZE];
} PACKED ALIGNED32 FAT_LONG_DIRECTORY_ENTRY, *PFAT_LONG_DIRECTORY_ENTRY;

#pragma pack(pop)

//
// -------------------------------------------------------- Function Prototypes
//

