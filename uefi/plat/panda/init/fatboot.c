/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatboot.c

Abstract:

    This module implements support for the FAT boot code. It's a small
    section of code that is loaded directly by the first stage loader, and
    knows only how to load and execute the firmware.

Author:

    Evan Green 14-Oct-2013

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/fat/fatlib.h>
#include <uefifw.h>
#include <dev/tirom.h>
#include "util.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FAT_DIRECTORY_ENTRIES_PER_BLOCK \
    (SECTOR_SIZE / sizeof(FAT_DIRECTORY_ENTRY))

//
// Define the address of a scratch buffer to hold a sector.
//

#define FAT_BOOT_SCRATCH (PVOID)0x81FFE000
#define FAT_BOOT_FAT12_REGION (PVOID)0x81FFC000

#define SECTOR_SIZE 512

//
// Define the number of clusters (which are 32 bits in FAT32) that can fit on
// a sector in the FAT. Note this is not the number of actual clusters that
// can fit in a block, just the count of cluster numbers that can fit on a
// block.
//

#define FAT16_CLUSTERS_PER_BLOCK (SECTOR_SIZE / FAT16_CLUSTER_WIDTH)
#define FAT32_CLUSTERS_PER_BLOCK (SECTOR_SIZE / FAT32_CLUSTER_WIDTH)

//
// Define some MBR values.
//

#define MBR_SIGNATURE_OFFSET 0x1FE
#define MBR_SIGNATURE 0xAA55
#define MBR_PARTITION_ENTRY_OFFSET 0x1BE
#define MBR_PARTITION_ENTRY_COUNT 4

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

    EndingHead - Stores the head number of the last sector of the partition
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

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
EfipTiGetActivePartition (
    PVOID Mbr,
    PULONG PartitionOffset
    );

BOOL
EfipFatMatchDirectoryEntry (
    PFAT_DIRECTORY_ENTRY Entry,
    PSTR Name,
    PLONG State
    );

KSTATUS
EfipFatGetNextCluster (
    PTI_ROM_MEM_HANDLE Handle,
    FAT_FORMAT Format,
    PVOID ScratchBuffer,
    PULONG Cluster
    );

UCHAR
EfipFatChecksumDirectoryEntry (
    PFAT_DIRECTORY_ENTRY Entry
    );

KSTATUS
EfipReadSectors (
    PTI_ROM_MEM_HANDLE Handle,
    PVOID Buffer,
    ULONG AbsoluteSector,
    ULONG SectorCount
    );

PVOID
EfipInitCopyMemory (
    PVOID Destination,
    PVOID Source,
    ULONG ByteCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the boot disk information and geometry.
//

ULONG EfiBootPartitionStart;

//
// Store the basic FAT file system information.
//

ULONG EfiFatSectorsPerCluster;
ULONG EfiFatFatBlockOffset;
ULONG EfiFatClustersBlockOffset;
ULONG EfiFatSectorsPerFat;

//
// Store a volatile variable for debugging indicating how far the code got
// before dying.
//

UCHAR EfiFatStepNumber;

//
// Store some more debugging variables.
//

ULONG EfiDirectoryEntriesExamined;
ULONG EfiLoaderCluster;

//
// Store the pointer where the entire FAT12 FAT is read in, to avoid trying to
// read a cluster that spans a sector.
//

PVOID EfiFat12FatRegion;

//
// ------------------------------------------------------------------ Functions
//

INTN
EfipTiLoadFirmwareFromFat (
    PTI_ROM_MEM_HANDLE Handle,
    CHAR8 *FileName,
    VOID *LoadAddress,
    UINT32 *Length
    )

/*++

Routine Description:

    This routine loads the firmware from a FAT file system.

Arguments:

    Handle - Supplies a pointer to the connection to the block device.

    FileName - Supplies a pointer to the null terminated name of the file.

    LoadAddress - Supplies the address where the image should be loaded.

    Length - Supplies a pointer where the length will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG BlockIndex;
    PFAT_BOOT_SECTOR BootSector;
    USHORT BytesPerSector;
    ULONG ClusterBlock;
    ULONG ClusterCount;
    ULONG ClusterShift;
    ULONG DataSectorCount;
    PFAT_DIRECTORY_ENTRY DirectoryEntry;
    ULONG DirectoryEntryIndex;
    ULONG FileSize;
    FAT_FORMAT Format;
    ULONGLONG Identifier;
    ULONG LoadedSize;
    PVOID Loader;
    ULONG LoaderCluster;
    INT Match;
    LONG MatchState;
    INTN Result;
    ULONG RootBlocks;
    ULONG RootDirectoryCluster;
    ULONG RootDirectoryCount;
    PVOID Scratch;
    KSTATUS Status;
    ULONG TotalSectors;

    EfiBootPartitionStart = 0;
    EfiDirectoryEntriesExamined = 0;
    EfiFatStepNumber = 1;
    EfiFat12FatRegion = NULL;

    //
    // Read the MBR to figure out where the active partition is.
    //

    Scratch = FAT_BOOT_SCRATCH;
    Status = EfipReadSectors(Handle, Scratch, 0, 1);
    if (!KSUCCESS(Status)) {
        goto TiLoadFirmwareFromFatEnd;
    }

    EfiFatStepNumber += 1;
    Status = EfipTiGetActivePartition(Scratch, &EfiBootPartitionStart);
    if (!KSUCCESS(Status)) {
        goto TiLoadFirmwareFromFatEnd;
    }

    //
    // Read the first sector of the partition to validate that this is a FAT
    // drive and find out where basic structures lie.
    //

    Scratch = FAT_BOOT_SCRATCH;
    Status = EfipReadSectors(Handle, Scratch, 0, 1);
    if (!KSUCCESS(Status)) {
        goto TiLoadFirmwareFromFatEnd;
    }

    EfiFatStepNumber += 1;
    BootSector = Scratch;
    if (BootSector->FatParameters.Signature != FAT_BOOT_SIGNATURE) {
        Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
        goto TiLoadFirmwareFromFatEnd;
    }

    if ((BootSector->Fat32Parameters.ExtendedBootSignature ==
         FAT_EXTENDED_BOOT_SIGNATURE) ||
        (BootSector->Fat32Parameters.ExtendedBootSignature ==
         FAT_EXTENDED_BOOT_SIGNATURE2)) {

        EfiFatSectorsPerFat =
                         BootSector->Fat32Parameters.SectorsPerAllocationTable;

        RootDirectoryCluster = BootSector->Fat32Parameters.RootDirectoryCluster;
        RootDirectoryCount = 0;
        EfipInitCopyMemory(&Identifier,
                           BootSector->Fat32Parameters.FatType,
                           sizeof(ULONGLONG));

    } else {
        EfiFatSectorsPerFat = BootSector->SectorsPerFileAllocationTable;
        RootDirectoryCluster = 0;
        RootDirectoryCount =
                           READ_UNALIGNED16(&(BootSector->RootDirectoryCount));

        EfipInitCopyMemory(&Identifier,
                           BootSector->FatParameters.FatType,
                           sizeof(ULONGLONG));
    }

    if ((Identifier != FAT32_IDENTIFIER) &&
        (Identifier != FAT16_IDENTIFIER) &&
        (Identifier != FAT12_IDENTIFIER) &&
        (Identifier != FAT_IDENTIFIER)) {

        Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
        goto TiLoadFirmwareFromFatEnd;
    }

    EfiFatStepNumber += 1;

    //
    // This code assumes that FAT's concept of the sector size is the same as
    // the old school BIOS 512 byte sectors.
    //

    BytesPerSector = READ_UNALIGNED16(&(BootSector->BytesPerSector));
    if (BytesPerSector != SECTOR_SIZE) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto TiLoadFirmwareFromFatEnd;
    }

    TotalSectors = READ_UNALIGNED16(&(BootSector->SmallTotalSectors));
    if (TotalSectors == 0) {
        TotalSectors = BootSector->BigTotalSectors;
    }

    EfiFatStepNumber += 1;
    EfiFatSectorsPerCluster = BootSector->SectorsPerCluster;
    EfiFatFatBlockOffset = READ_UNALIGNED16(&(BootSector->ReservedSectorCount));
    RootBlocks = RootDirectoryCount * sizeof(FAT_DIRECTORY_ENTRY);
    RootBlocks = ALIGN_RANGE_UP(RootBlocks, BytesPerSector) / BytesPerSector;
    EfiFatClustersBlockOffset = EfiFatFatBlockOffset +
                                (EfiFatSectorsPerFat *
                                 BootSector->AllocationTableCount) +
                                RootBlocks;

    FileSize = 0;
    LoaderCluster = 0;
    LoadedSize = 0;
    EfiFatStepNumber += 1;

    //
    // Figure out the total number of clusters, and therefore the FAT format.
    //

    DataSectorCount = TotalSectors - EfiFatClustersBlockOffset;
    ClusterShift = 0;
    while (BootSector->SectorsPerCluster != 0) {
        ClusterShift += 1;
        BootSector->SectorsPerCluster >>= 1;
    }

    ClusterShift -= 1;
    ClusterCount = (DataSectorCount >> ClusterShift) + FAT_CLUSTER_BEGIN;
    if (ClusterCount < FAT12_CLUSTER_CUTOFF) {
        Format = Fat12Format;

    } else if (ClusterCount < FAT16_CLUSTER_CUTOFF) {
        Format = Fat16Format;

    } else {
        Format = Fat32Format;
    }

    EfiFatStepNumber += 1;

    //
    // If the format is FAT12, read the entire FAT in.
    //

    if (Format == Fat12Format) {
        EfiFat12FatRegion = FAT_BOOT_FAT12_REGION;
        Status = EfipReadSectors(Handle,
                                 EfiFat12FatRegion,
                                 EfiFatFatBlockOffset,
                                 EfiFatSectorsPerFat);

        if (!KSUCCESS(Status)) {
            goto TiLoadFirmwareFromFatEnd;
        }
    }

    EfiFatStepNumber += 1;

    //
    // Loop across all clusters or blocks in the root directory.
    //

    if (RootDirectoryCluster != 0) {
        ClusterBlock = EfiFatClustersBlockOffset +
                       ((RootDirectoryCluster - FAT_CLUSTER_BEGIN) *
                        EfiFatSectorsPerCluster);

    } else {
        ClusterBlock = EfiFatClustersBlockOffset - RootBlocks;
    }

    Match = FALSE;
    MatchState = 0;
    DirectoryEntry = NULL;
    while (TRUE) {

        //
        // Loop over every block in the cluster.
        //

        for (BlockIndex = 0;
             BlockIndex < EfiFatSectorsPerCluster;
             BlockIndex += 1) {

            Status = EfipReadSectors(Handle,
                                     Scratch,
                                     ClusterBlock + BlockIndex,
                                     1);

            if (!KSUCCESS(Status)) {
                goto TiLoadFirmwareFromFatEnd;
            }

            //
            // Loop over every directory entry in the block.
            //

            DirectoryEntry = Scratch;
            for (DirectoryEntryIndex = 0;
                 DirectoryEntryIndex < FAT_DIRECTORY_ENTRIES_PER_BLOCK;
                 DirectoryEntryIndex += 1) {

                EfiDirectoryEntriesExamined += 1;

                //
                // If the directory ended, fail sadly.
                //

                if (DirectoryEntry->DosName[0] == FAT_DIRECTORY_ENTRY_END) {
                    Status = STATUS_PATH_NOT_FOUND;
                    goto TiLoadFirmwareFromFatEnd;
                }

                Match = EfipFatMatchDirectoryEntry(DirectoryEntry,
                                                   FileName,
                                                   &MatchState);

                if (Match != FALSE) {
                    LoaderCluster = (DirectoryEntry->ClusterHigh << 16) |
                                    DirectoryEntry->ClusterLow;

                    break;
                }

                DirectoryEntry += 1;
            }

            if (Match != FALSE) {
                break;
            }
        }

        if (Match != FALSE) {
            break;
        }

        //
        // Get the next cluster of the directory entry. If this is the root
        // directory of FAT12/16, just advance to the next block.
        //

        if (RootBlocks != 0) {
            if (RootBlocks <= BlockIndex) {
                break;
            }

            RootBlocks -= BlockIndex;
            ClusterBlock += BlockIndex;

        //
        // For directories in the main data area, fetch the next cluster of the
        // directory.
        //

        } else {
            Status = EfipFatGetNextCluster(Handle,
                                           Format,
                                           Scratch,
                                           &RootDirectoryCluster);

            if (!KSUCCESS(Status)) {
                goto TiLoadFirmwareFromFatEnd;
            }

            ClusterBlock = EfiFatClustersBlockOffset +
                           ((RootDirectoryCluster - FAT_CLUSTER_BEGIN) *
                            EfiFatSectorsPerCluster);
        }
    }

    EfiFatStepNumber += 1;
    FileSize = DirectoryEntry->FileSizeInBytes;
    if (FileSize == 0) {
        Status = STATUS_INVALID_ADDRESS;
        goto TiLoadFirmwareFromFatEnd;
    }

    EfiFatStepNumber += 1;
    EfiLoaderCluster = LoaderCluster;

    //
    // Loop through every cluster in the loader.
    //

    Loader = LoadAddress;
    while (TRUE) {
        ClusterBlock = EfiFatClustersBlockOffset +
                       ((LoaderCluster - FAT_CLUSTER_BEGIN) *
                        EfiFatSectorsPerCluster);

        Status = EfipReadSectors(Handle,
                                 Loader,
                                 ClusterBlock,
                                 EfiFatSectorsPerCluster);

        if (!KSUCCESS(Status)) {
            goto TiLoadFirmwareFromFatEnd;
        }

        Loader += EfiFatSectorsPerCluster * SECTOR_SIZE;
        LoadedSize += EfiFatSectorsPerCluster * SECTOR_SIZE;
        if (LoadedSize >= FileSize) {
            break;
        }

        Status = EfipFatGetNextCluster(Handle, Format, Scratch, &LoaderCluster);
        if (!KSUCCESS(Status)) {
            goto TiLoadFirmwareFromFatEnd;
        }
    }

    EfiFatStepNumber += 1;
    Status = STATUS_SUCCESS;

    //
    // Jump into the loader. This is not expected to return.
    //

    *Length = FileSize;
    Status = STATUS_SUCCESS;

TiLoadFirmwareFromFatEnd:
    Result = 0;
    if (!KSUCCESS(Status)) {
        EfipSerialPrintString("Failed to find UEFI firmware. Status ");
        EfipSerialPrintHexInteger(Status);
        EfipSerialPrintString(" Step ");
        EfipSerialPrintHexInteger(EfiFatStepNumber);
        EfipSerialPrintString(".\n");
        Result = 1;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
EfipTiGetActivePartition (
    PVOID Mbr,
    PULONG PartitionOffset
    )

/*++

Routine Description:

    This routine determines the partition offset of the active partition.

Arguments:

    Mbr - Supplies a pointer to the MBR.

    PartitionOffset - Supplies a pointer where the offset in sectors to the
        active partition will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DUPLICATE_ENTRY if there is more than one active partition.

    STATUS_NOT_FOUND if the MBR is not valid, the partition information is
    not valid, or there is not one active partition.

--*/

{

    PPARTITION_TABLE_ENTRY Entries;
    UINTN Index;
    UINT32 SectorCount;
    UINT16 Signature;
    UINT32 StartingLba;

    Signature = READ_UNALIGNED16(Mbr + MBR_SIGNATURE_OFFSET);
    if (Signature != MBR_SIGNATURE) {
        return STATUS_NOT_FOUND;
    }

    *PartitionOffset = 0;
    Entries = Mbr + MBR_PARTITION_ENTRY_OFFSET;
    for (Index = 0; Index < MBR_PARTITION_ENTRY_COUNT; Index += 1) {
        if (Entries[Index].BootIndicator == 0) {
            continue;
        }

        if (Entries[Index].BootIndicator != 0x80) {
            return STATUS_NOT_FOUND;
        }

        StartingLba = READ_UNALIGNED32(&(Entries[Index].StartingLba));
        SectorCount = READ_UNALIGNED32(&(Entries[Index].SectorCount));
        if ((StartingLba == 0) || (SectorCount == 0)) {
            continue;
        }

        if (*PartitionOffset != 0) {
            return STATUS_DUPLICATE_ENTRY;
        }

        *PartitionOffset = StartingLba;
    }

    return STATUS_SUCCESS;
}

BOOL
EfipFatMatchDirectoryEntry (
    PFAT_DIRECTORY_ENTRY Entry,
    PSTR Name,
    PLONG State
    )

/*++

Routine Description:

    This routine compares the given directory entry against the desired
    loader directory entry.

Arguments:

    Entry - Supplies a pointer to the directory entry.

    Name - Supplies a pointer to the eleven byte short format name (containing
        no period and space for padding on the right). This name must be all
        lower-case, the directory entry name will be converted to lower case.

    State - Supplies a pointer to the match state used by this routine.
        Initialize this to zero for the first directory entry.

Return Value:

    TRUE if directory entry matches the desired name.

    FALSE if the directory entry does not match.

--*/

{

    CHAR Character;
    ULONG CharacterIndex;
    UCHAR ComputedChecksum;
    PSTR EntryName;
    PFAT_LONG_DIRECTORY_ENTRY LongEntry;
    UCHAR LongEntryChecksum;
    ULONG NameIndex;
    PUCHAR Region;
    ULONG RegionIndex;
    ULONG RegionSize;
    UCHAR Sequence;

    if (Entry->FileAttributes == FAT_LONG_FILE_NAME_ATTRIBUTES) {
        *State = 0;
        LongEntry = (PFAT_LONG_DIRECTORY_ENTRY)Entry;
        if (LongEntry->SequenceNumber == FAT_DIRECTORY_ENTRY_ERASED) {
            return FALSE;
        }

        //
        // The terminating entry comes first, so there should be more long file
        // name entries on the way.
        //

        if ((LongEntry->SequenceNumber & FAT_LONG_DIRECTORY_ENTRY_END) != 0) {
            Sequence = LongEntry->SequenceNumber &
                       FAT_LONG_DIRECTORY_ENTRY_SEQUENCE_MASK;

            //
            // This routine currently only supports matching a single long
            // entry.
            //

            if (Sequence != 1) {
                return FALSE;
            }

            NameIndex = *State;
            for (RegionIndex = 0; RegionIndex < 3; RegionIndex += 1) {
                if (RegionIndex == 0) {
                    Region = (PUCHAR)&(LongEntry->Name1);
                    RegionSize = sizeof(LongEntry->Name1);

                } else if (RegionIndex == 1) {
                    Region = (PUCHAR)&(LongEntry->Name2);
                    RegionSize = sizeof(LongEntry->Name2);

                } else {
                    Region = (PUCHAR)&(LongEntry->Name3);
                    RegionSize = sizeof(LongEntry->Name3);
                }

                for (CharacterIndex = 0;
                     CharacterIndex < RegionSize;
                     CharacterIndex += sizeof(USHORT)) {

                    if (Name[NameIndex] == '\0') {
                        break;
                    }

                    if (READ_UNALIGNED16(&(Region[CharacterIndex])) !=
                        Name[NameIndex]) {

                        return FALSE;
                    }

                    NameIndex += 1;
                }
            }

            //
            // This long entry matches. The next short entry is the one.
            //

            *State = NameIndex |
                     (LongEntry->ShortFileNameChecksum << BITS_PER_BYTE);

            return FALSE;
        }

    } else if ((Entry->FileAttributes & FAT_VOLUME_LABEL) != 0) {
        *State = 0;
        return FALSE;
    }

    //
    // If the previous long entry matched the entire name, then compare the
    // checksums and return this short entry if they match.
    //

    NameIndex = *State & 0xFF;
    if (Name[NameIndex] == '\0') {
        LongEntryChecksum = (*State >> BITS_PER_BYTE) & 0xFF;
        ComputedChecksum = EfipFatChecksumDirectoryEntry(Entry);
        if (ComputedChecksum == LongEntryChecksum) {
            return TRUE;
        }
    }

    //
    // Compare the short entry directly against the file name.
    //

    *State = 0;
    EntryName = (PSTR)(Entry->DosName);
    NameIndex = 0;
    for (CharacterIndex = 0;
         CharacterIndex < FAT_NAME_SIZE;
         CharacterIndex += 1) {

        //
        // Index through the name knowing that the extension comes right after
        // it. This is code no one is ever supposed to see.
        //

        Character = EntryName[CharacterIndex];

        //
        // If the name ended, it better be spaces all the way to the end.
        //

        if (Name[NameIndex] == '\0') {
            if (Character != ' ') {
                return FALSE;
            }

            continue;

        //
        // If it's a dot and the current character is still in the DOS name
        // portion, it had better be a blanking space. If it's at the extension
        // boundary, advance past the dot to compare the extension.
        //

        } else if (Name[NameIndex] == '.') {
            if (CharacterIndex < FAT_FILE_LENGTH) {
                if (Character != ' ') {
                    return FALSE;
                }

                continue;

            } else if (CharacterIndex == FAT_FILE_LENGTH) {
                NameIndex += 1;
            }
        }

        //
        // Lowercase the character.
        //

        if ((Character >= 'A') && (Character <= 'Z')) {
            Character = Character - 'A' + 'a';
        }

        if (Character != Name[NameIndex]) {
            return FALSE;
        }

        NameIndex += 1;
    }

    return TRUE;
}

KSTATUS
EfipFatGetNextCluster (
    PTI_ROM_MEM_HANDLE Handle,
    FAT_FORMAT Format,
    PVOID ScratchBuffer,
    PULONG Cluster
    )

/*++

Routine Description:

    This routine finds the next cluster given a current cluster.

Arguments:

    Handle - Supplies the handle to the device.

    Format - Supplies the FAT formatting (the cluster number size).

    ScratchBuffer - Supplies a pointer to a sector sized buffer this routine
        can use for scratch.

    Cluster - Supplies a pointer to a pointer that on input contains the
        current cluster. On successful output, contains the next cluster.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_END_OF_FILE if there is no next cluster.

    STATUS_VOLUME_CORRUPT if the current cluster is greater than the size of
    the FAT.

--*/

{

    PVOID Fat;
    ULONG FatOffset;
    ULONG NextCluster;
    KSTATUS Status;

    if (Format == Fat12Format) {
        *Cluster = FAT12_READ_CLUSTER(EfiFat12FatRegion, *Cluster);
        return STATUS_SUCCESS;
    }

    if (Format == Fat16Format) {
        FatOffset = *Cluster / FAT16_CLUSTERS_PER_BLOCK;

    } else {
        FatOffset = *Cluster / FAT32_CLUSTERS_PER_BLOCK;
    }

    if (FatOffset >= EfiFatSectorsPerFat) {
        return STATUS_VOLUME_CORRUPT;
    }

    Status = EfipReadSectors(Handle,
                             ScratchBuffer,
                             EfiFatFatBlockOffset + FatOffset,
                             1);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Fat = ScratchBuffer;
    if (Format == Fat16Format) {
        NextCluster = ((PUSHORT)Fat)[*Cluster % FAT16_CLUSTERS_PER_BLOCK];
        if (NextCluster >= FAT16_CLUSTER_BAD) {
            return STATUS_END_OF_FILE;
        }

    } else {
        NextCluster = ((PULONG)Fat)[*Cluster % FAT32_CLUSTERS_PER_BLOCK];
        if (NextCluster >= FAT32_CLUSTER_BAD) {
            return STATUS_END_OF_FILE;
        }
    }

    if (NextCluster < FAT_CLUSTER_BEGIN) {
        return STATUS_VOLUME_CORRUPT;
    }

    *Cluster = NextCluster;
    return STATUS_SUCCESS;
}

UCHAR
EfipFatChecksumDirectoryEntry (
    PFAT_DIRECTORY_ENTRY Entry
    )

/*++

Routine Description:

    This routine returns the checksum of the given fat short directory entry
    based on the file name.

Arguments:

    Entry - Supplies a pointer to the directory entry.

Return Value:

    Returns the checksum of the directory entry.

--*/

{

    ULONG Index;
    UCHAR Sum;

    Sum = 0;
    for (Index = 0; Index < FAT_FILE_LENGTH; Index += 1) {
        Sum = ((Sum & 0x1) << 0x7) + (Sum >> 1) + Entry->DosName[Index];
    }

    for (Index = 0; Index < FAT_FILE_EXTENSION_LENGTH; Index += 1) {
        Sum = ((Sum & 0x1) << 0x7) + (Sum >> 1) + Entry->DosExtension[Index];
    }

    return Sum;
}

KSTATUS
EfipReadSectors (
    PTI_ROM_MEM_HANDLE Handle,
    PVOID Buffer,
    ULONG AbsoluteSector,
    ULONG SectorCount
    )

/*++

Routine Description:

    This routine reads sectors from the SD card using the ROM.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Buffer - Supplies the buffer where the read data will be returned.

    AbsoluteSector - Supplies the zero-based sector number to read from.

    SectorCount - Supplies the number of sectors to read. The supplied buffer
        must be at least this large.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_DEVICE_IO_ERROR if the ROM code failed the operation.

--*/

{

    INT Result;

    Result = EfipTiMemRead(Handle,
                           AbsoluteSector + EfiBootPartitionStart,
                           SectorCount,
                           Buffer);

    if (Result != 0) {
        EfipSerialPrintString("Failed to read from SD: ");
        EfipSerialPrintHexInteger(Result);
        EfipSerialPrintString(".\n");
        return STATUS_DEVICE_IO_ERROR;
    }

    return STATUS_SUCCESS;
}

PVOID
EfipInitCopyMemory (
    PVOID Destination,
    PVOID Source,
    ULONG ByteCount
    )

/*++

Routine Description:

    This routine copies a section of memory.

Arguments:

    Destination - Supplies a pointer to the buffer where the memory will be
        copied to.

    Source - Supplies a pointer to the buffer to be copied.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    Returns the destination pointer.

--*/

{

    PUCHAR From;
    PUCHAR To;

    From = (PUCHAR)Source;
    To = (PUCHAR)Destination;
    while (ByteCount > 0) {
        *To = *From;
        To += 1;
        From += 1;
        ByteCount -= 1;
    }

    return Destination;
}

