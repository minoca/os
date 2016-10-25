/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    part.h

Abstract:

    This header contains definitions for the partition device information
    structure.

Author:

    Evan Green 10-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define PARTITION_DEVICE_INFORMATION_UUID \
    {{0x104D5242, 0x9DCD44B7, 0xA51B760F, 0x0D6810B8}}

#define PARTITION_DEVICE_INFORMATION_VERSION 0x00010000

//
// Define the size of a disk identifier (which happens to be large enough to
// hold a GPT GUID).
//

#define DISK_IDENTIFIER_SIZE 16
#define PARTITION_IDENTIFIER_SIZE DISK_IDENTIFIER_SIZE
#define PARTITION_TYPE_SIZE 16

//
// Define partition information flags.
//

//
// This flag is set if the partition is marked as "active". This is usually the
// entry that gets booted by default. Only one partition on a disk should have
// this flag set.
//

#define PARTITION_FLAG_BOOT 0x00000001

//
// This flag is set if this is a primary partition (its partition entry was
// found directly in the MBR).
//

#define PARTITION_FLAG_PRIMARY 0x00000002

//
// This flag is set if this is an extended partition (its partition entry was
// found directly in the MBR and it points to logical partitions.
//

#define PARTITION_FLAG_EXTENDED 0x00000004

//
// This flag is set if this is a logical partition (its partition entry was
// found chained in an extended partition).
//

#define PARTITION_FLAG_LOGICAL 0x00000008

//
// Set this flag if this is not actually a partition at all but the raw disk
// itself.
//

#define PARTITION_FLAG_RAW_DISK 0x00000010

//
// Define recognized partition system ID byte values. Some super old values
// that will probably never come up are simply ignored.
//

#define PARTITION_ID_EMPTY               0x00
#define PARTITION_ID_DOS_FAT12           0x01
#define PARTITION_ID_DOS_PRIMARY_FAT16   0x04
#define PARTITION_ID_DOS_EXTENDED        0x05
#define PARTITION_ID_NTFS                0x07
#define PARTITION_ID_WINDOWS95_FAT32     0x0B
#define PARTITION_ID_WINDOWS95_FAT32_LBA 0x0C
#define PARTITION_ID_DOS_EXTENDED_FAT16  0x0E
#define PARTITION_ID_DOS_EXTENDED_LBA    0x0F
#define PARTITION_ID_WINDOWS_RE          0x27
#define PARTITION_ID_PLAN9               0x39
#define PARTITION_ID_SYSTEMV_MACH_HURD   0x63
#define PARTITION_ID_MINOCA              0x6B
#define PARTITION_ID_MINIX_13            0x80
#define PARTITION_ID_MINIX_14            0x81
#define PARTITION_ID_LINUX_SWAP          0x82
#define PARTITION_ID_LINUX               0x83
#define PARTITION_ID_LINUX_EXTENDED      0x85
#define PARTITION_ID_LINUX_LVM           0x8E
#define PARTITION_ID_BSD                 0x9F
#define PARTITION_ID_FREEBSD             0xA5
#define PARTITION_ID_OPENBSD             0xA6
#define PARTITION_ID_NEXTSTEP            0xA7
#define PARTITION_ID_MAC_OS_X            0xA8
#define PARTITION_ID_NETBSD              0xA9
#define PARTITION_ID_MAC_OS_X_BOOT       0xAB
#define PARTITION_ID_MAX_OS_X_HFS        0xAF
#define PARTITION_ID_EFI_GPT             0xEE
#define PARTITION_ID_EFI_SYSTEM          0xEF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PARTITION_FORMAT {
    PartitionFormatInvalid,
    PartitionFormatNone,
    PartitionFormatMbr,
    PartitionFormatGpt,
} PARTITION_FORMAT, *PPARTITION_FORMAT;

typedef enum _PARTITION_TYPE {
    PartitionTypeInvalid,
    PartitionTypeNone,
    PartitionTypeUnknown,
    PartitionTypeEmpty,
    PartitionTypeDosFat12,
    PartitionTypeDosPrimaryFat16,
    PartitionTypeDosExtended,
    PartitionTypeNtfs,
    PartitionTypeWindows95Fat32,
    PartitionTypeWindows95Fat32Lba,
    PartitionTypeDosExtendedFat16,
    PartitionTypeDosExtendedLba,
    PartitionTypeWindowsRecovery,
    PartitionTypePlan9,
    PartitionTypeSystemVMachHurd,
    PartitionTypeMinoca,
    PartitionTypeMinix13,
    PartitionTypeMinix14,
    PartitionTypeLinuxSwap,
    PartitionTypeLinux,
    PartitionTypeLinuxExtended,
    PartitionTypeLinuxLvm,
    PartitionTypeBsd,
    PartitionTypeFreeBsd,
    PartitionTypeOpenBsd,
    PartitionTypeNextStep,
    PartitionTypeMacOsX,
    PartitionTypeNetBsd,
    PartitionTypeMaxOsXBoot,
    PartitionTypeMaxOsXHfs,
    PartitionTypeEfiGpt,
    PartitionTypeEfiSystem,
} PARTITION_TYPE, *PPARTITION_TYPE;

/*++

Structure Description:

    This structure stores the partition device information published by
    partition devices.

Members:

    Version - Stores the table version. Future revisions will be backwards
        compatible. Set to PARTITION_DEVICE_INFORMATION_VERSION.

    Format - Stores the partition format, type PARTITION_FORMAT.

    PartitionType - Stores the partition type, type PARTITION_TYPE.

    Flags - Stores a bitfield of flags. See PARTITION_FLAG_* definitions.

    BlockSize - Stores the size of a block on the underlying disk.

    Number - Stores the partition number.

    ParentNumber - Stores the number of the parent extended partition if this
        is a logical partition.

    FirstBlock - Stores the first logical block of the partition, inclusive.

    LastBlock - Stores the last logical block of the partition, inclusive.

    PartitionId - Stores the partition unique identifier.

    PartitionTypeId - Stores the partition type identifier.

    DiskId - Stores the disk identifier.

--*/

typedef struct _PARTITION_DEVICE_INFORMATION {
    ULONG Version;
    ULONG PartitionFormat;
    ULONG PartitionType;
    ULONG Flags;
    ULONG BlockSize;
    ULONG Number;
    ULONG ParentNumber;
    ULONGLONG FirstBlock;
    ULONGLONG LastBlock;
    UCHAR PartitionId[PARTITION_IDENTIFIER_SIZE];
    UCHAR PartitionTypeId[PARTITION_TYPE_SIZE];
    UCHAR DiskId[DISK_IDENTIFIER_SIZE];
} PARTITION_DEVICE_INFORMATION, *PPARTITION_DEVICE_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
