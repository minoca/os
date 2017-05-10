/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatboot.c

Abstract:

    This module implements support for the FAT BIOS boot code. It's a small
    section of code that is loaded directly by the MBR, and knows only how to
    load and execute the boot manager.

Author:

    Evan Green 14-Oct-2013

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/fat/fatlib.h>
#include "firmware.h"
#include "bios.h"
#include <minoca/kernel/x86.h>
#include "realmode.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Set this to biosfw.bin to load the UEFI layer that sits on top of BIOS.
//

#if 0

#define BOOT_MANAGER_NAME "biosfw.bin"

#else

#define BOOT_MANAGER_NAME "bootman.bin"

#endif

#define BOOT_MANAGER_ADDRESS (PVOID)0x100000

#define FAT_DIRECTORY_ENTRIES_PER_BLOCK \
    (SECTOR_SIZE / sizeof(FAT_DIRECTORY_ENTRY))

#define TEXT_VIDEO_ATTRIBUTE (0x1F << 8)

//
// Define the address of a scratch buffer to hold a sector.
//

#define FAT_BOOT_SCRATCH (PVOID)0x4000
#define SECTOR_SIZE 512

//
// Limit the maximum number of sectors that can be read at a time to a page,
// since the real mode context data area is only a page.
//

#define MAX_READ_SECTORS (0x1000 / SECTOR_SIZE)

//
// Define a region that can hold all 8k of the FAT12 FAT.
//

#define FAT_BOOT_FAT12_REGION (PVOID)0x5000

//
// Define the number of clusters (which are 32 bits in FAT32) that can fit on
// a sector in the FAT. Note this is not the number of actual clusters that
// can fit in a block, just the count of cluster numbers that can fit on a
// block.
//

#define FAT16_CLUSTERS_PER_BLOCK (SECTOR_SIZE / FAT16_CLUSTER_WIDTH)
#define FAT32_CLUSTERS_PER_BLOCK (SECTOR_SIZE / FAT32_CLUSTER_WIDTH)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
INT
(*PBOOT_APPLICATION_MAIN) (
    PVOID TopOfStack,
    ULONG StackSize,
    ULONGLONG PartitionOffset,
    ULONG BootDriveNumber
    );

/*++

Routine Description:

    This routine is the entry point for the boot loader program.

Arguments:

    StartOfLoader - Supplies the address where the loader begins in memory.

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

    PartitionOffset - Supplies the offset, in sectors, from the start of the
        disk where this boot partition resides.

    BootDriveNumber - Supplies the drive number for the device that booted
        the system.

Return Value:

    Returns the step number that failed.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
BoInitializeProcessor (
    VOID
    );

VOID
BopPrintHexInteger (
    ULONG Column,
    ULONG Line,
    ULONG Integer
    );

VOID
BopPrintString (
    ULONG Column,
    ULONG Line,
    PSTR String
    );

KSTATUS
BopReadSectors (
    PVOID Buffer,
    ULONG AbsoluteSector,
    ULONG SectorCount
    );

BOOL
BopMatchDirectoryEntry (
    PFAT_DIRECTORY_ENTRY Entry,
    PSTR Name,
    PLONG State
    );

KSTATUS
BopFatGetNextCluster (
    FAT_FORMAT Format,
    PVOID ScratchBuffer,
    PULONG Cluster
    );

UCHAR
BopFatChecksumDirectoryEntry (
    PFAT_DIRECTORY_ENTRY Entry
    );

VOID
RtlZeroMemory (
    PVOID Buffer,
    UINTN ByteCount
    );

PVOID
RtlCopyMemory (
    PVOID Destination,
    PCVOID Source,
    UINTN ByteCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the boot disk information and geometry.
//

UCHAR BoBootDriveNumber;
ULONG BoBootPartitionStart;

//
// Store the basic FAT file system information.
//

ULONG BoFatSectorsPerCluster;
ULONG BoFatFatBlockOffset;
ULONG BoFatClustersBlockOffset;
ULONG BoFatSectorsPerFat;

//
// Store a volatile variable for debugging indicating how far the code got
// before dying.
//

UCHAR BoStepNumber;

//
// Store some more debugging variables.
//

ULONG BoDirectoryEntriesExamined;
ULONG BoLoaderCluster;
ULONG BoLoaderClusterCount;
ULONG BoLoaderClustersRead;

//
// Store the pointer where the entire FAT12 FAT is read in, to avoid trying to
// read a cluster that spans a sector.
//

PVOID BoFat12FatRegion;

//
// ------------------------------------------------------------------ Functions
//

VOID
BoMain (
    PVOID TopOfStack,
    ULONG StackSize,
    ULONGLONG PartitionOffset,
    ULONG BootDriveNumber
    )

/*++

Routine Description:

    This routine is the entry point for the boot loader program.

Arguments:

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

    PartitionOffset - Supplies the offset, in sectors, from the start of the
        disk where this boot partition resides.

    BootDriveNumber - Supplies the drive number for the device that booted
        the system.

Return Value:

    This function does not return.

--*/

{

    ULONG BlockIndex;
    ULONG BlocksThisRound;
    PFAT_BOOT_SECTOR BootSector;
    ULONG ClusterBlock;
    ULONG ClusterCount;
    ULONG DataSectorCount;
    PFAT_DIRECTORY_ENTRY DirectoryEntry;
    ULONG DirectoryEntryIndex;
    FAT_FORMAT Format;
    ULONGLONG Identifier;
    PVOID Loader;
    ULONG LoaderCluster;
    ULONG LoaderClusterCount;
    PBOOT_APPLICATION_MAIN MainFunction;
    BOOL Match;
    LONG MatchState;
    INT ReturnValue;
    ULONG RootBlocks;
    ULONG RootDirectoryCluster;
    ULONG RootDirectoryCount;
    PVOID Scratch;
    KSTATUS Status;
    ULONG TotalSectors;

    BoStepNumber = 1;
    BoInitializeProcessor();
    BoStepNumber += 1;
    BoBootDriveNumber = BootDriveNumber;
    BoBootPartitionStart = PartitionOffset;
    BoFat12FatRegion = NULL;
    BopPrintString(0, 0, "VBR");

    //
    // Read the boot sector to validate that this is a FAT drive and find out
    // where basic structures lie.
    //

    Scratch = FAT_BOOT_SCRATCH;
    Status = BopReadSectors(Scratch, 0, 1);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    BoStepNumber += 1;
    BootSector = Scratch;
    if (BootSector->FatParameters.Signature != FAT_BOOT_SIGNATURE) {
        Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
        goto MainEnd;
    }

    if ((BootSector->Fat32Parameters.ExtendedBootSignature ==
         FAT_EXTENDED_BOOT_SIGNATURE) ||
        (BootSector->Fat32Parameters.ExtendedBootSignature ==
         FAT_EXTENDED_BOOT_SIGNATURE2)) {

        BoFatSectorsPerFat =
                         BootSector->Fat32Parameters.SectorsPerAllocationTable;

        RootDirectoryCluster = BootSector->Fat32Parameters.RootDirectoryCluster;
        RootDirectoryCount = 0;
        RtlCopyMemory(&Identifier,
                      BootSector->Fat32Parameters.FatType,
                      sizeof(ULONGLONG));

    } else {
        BoFatSectorsPerFat = BootSector->SectorsPerFileAllocationTable;
        RootDirectoryCluster = 0;
        RootDirectoryCount = BootSector->RootDirectoryCount;
        RtlCopyMemory(&Identifier,
                      BootSector->FatParameters.FatType,
                      sizeof(ULONGLONG));
    }

    if ((Identifier != FAT32_IDENTIFIER) &&
        (Identifier != FAT16_IDENTIFIER) &&
        (Identifier != FAT12_IDENTIFIER) &&
        (Identifier != FAT_IDENTIFIER)) {

        Status = STATUS_UNRECOGNIZED_FILE_SYSTEM;
        goto MainEnd;
    }

    BoStepNumber += 1;

    //
    // This code assumes that FAT's concept of the sector size is the same as
    // the old school BIOS 512 byte sectors.
    //

    if (BootSector->BytesPerSector != SECTOR_SIZE) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto MainEnd;
    }

    TotalSectors = BootSector->SmallTotalSectors;
    if (TotalSectors == 0) {
        TotalSectors = BootSector->BigTotalSectors;
    }

    BoStepNumber += 1;
    BoFatSectorsPerCluster = BootSector->SectorsPerCluster;
    BoFatFatBlockOffset = BootSector->ReservedSectorCount;
    RootBlocks = RootDirectoryCount * sizeof(FAT_DIRECTORY_ENTRY);
    RootBlocks = ALIGN_RANGE_UP(RootBlocks, BootSector->BytesPerSector) /
                 BootSector->BytesPerSector;

    BoFatClustersBlockOffset = BoFatFatBlockOffset +
                               (BoFatSectorsPerFat *
                                BootSector->AllocationTableCount) +
                               RootBlocks;

    LoaderCluster = 0;
    LoaderClusterCount = 0;
    BoStepNumber += 1;

    //
    // Figure out the total number of clusters, and therefore the FAT format.
    //

    DataSectorCount = TotalSectors - BoFatClustersBlockOffset;
    ClusterCount = (DataSectorCount / BootSector->SectorsPerCluster) +
                   FAT_CLUSTER_BEGIN;

    if (ClusterCount < FAT12_CLUSTER_CUTOFF) {
        Format = Fat12Format;

    } else if (ClusterCount < FAT16_CLUSTER_CUTOFF) {
        Format = Fat16Format;

    } else {
        Format = Fat32Format;
    }

    BoStepNumber += 1;

    //
    // If the format is FAT12, read the entire FAT in.
    //

    if (Format == Fat12Format) {
        BoFat12FatRegion = FAT_BOOT_FAT12_REGION;
        Status = BopReadSectors(BoFat12FatRegion,
                                BoFatFatBlockOffset,
                                BoFatSectorsPerFat);

        if (!KSUCCESS(Status)) {
            goto MainEnd;
        }
    }

    BoStepNumber += 1;

    //
    // Loop across all clusters or blocks in the root directory.
    //

    if (RootDirectoryCluster != 0) {
        ClusterBlock = BoFatClustersBlockOffset +
                       ((RootDirectoryCluster - FAT_CLUSTER_BEGIN) *
                        BoFatSectorsPerCluster);

    } else {
        ClusterBlock = BoFatClustersBlockOffset - RootBlocks;
    }

    Match = FALSE;
    MatchState = 0;
    while (TRUE) {

        //
        // Loop over every block in the cluster.
        //

        for (BlockIndex = 0;
             BlockIndex < BoFatSectorsPerCluster;
             BlockIndex += 1) {

            Status = BopReadSectors(Scratch, ClusterBlock + BlockIndex, 1);
            if (!KSUCCESS(Status)) {
                goto MainEnd;
            }

            //
            // Loop over every directory entry in the block.
            //

            DirectoryEntry = Scratch;
            for (DirectoryEntryIndex = 0;
                 DirectoryEntryIndex < FAT_DIRECTORY_ENTRIES_PER_BLOCK;
                 DirectoryEntryIndex += 1) {

                BoDirectoryEntriesExamined += 1;

                //
                // If the directory ended, fail sadly.
                //

                if (DirectoryEntry->DosName[0] == FAT_DIRECTORY_ENTRY_END) {
                    Status = STATUS_PATH_NOT_FOUND;
                    goto MainEnd;
                }

                Match = BopMatchDirectoryEntry(DirectoryEntry,
                                               BOOT_MANAGER_NAME,
                                               &MatchState);

                if (Match != FALSE) {
                    LoaderCluster = (DirectoryEntry->ClusterHigh << 16) |
                                    DirectoryEntry->ClusterLow;

                    //
                    // Round up to an integral number of clusters.
                    //

                    LoaderClusterCount =
                                ALIGN_RANGE_UP(DirectoryEntry->FileSizeInBytes,
                                               SECTOR_SIZE) / SECTOR_SIZE;

                    LoaderClusterCount = (LoaderClusterCount +
                                          BoFatSectorsPerCluster - 1) /
                                         BoFatSectorsPerCluster;

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
            Status = BopFatGetNextCluster(Format,
                                          Scratch,
                                          &RootDirectoryCluster);

            if (!KSUCCESS(Status)) {
                goto MainEnd;
            }

            ClusterBlock = BoFatClustersBlockOffset +
                           ((RootDirectoryCluster - FAT_CLUSTER_BEGIN) *
                            BoFatSectorsPerCluster);
        }
    }

    BoStepNumber += 1;
    if (LoaderClusterCount == 0) {
        Status = STATUS_INVALID_ADDRESS;
        goto MainEnd;
    }

    BoStepNumber += 1;
    BoLoaderCluster = LoaderCluster;
    BoLoaderClusterCount = LoaderClusterCount;

    //
    // Loop through every cluster in the loader.
    //

    Loader = BOOT_MANAGER_ADDRESS;
    while (TRUE) {
        ClusterBlock = BoFatClustersBlockOffset +
                       ((LoaderCluster - FAT_CLUSTER_BEGIN) *
                        BoFatSectorsPerCluster);

        BlockIndex = 0;
        while (BlockIndex < BoFatSectorsPerCluster) {
            BlocksThisRound = BoFatSectorsPerCluster - BlockIndex;
            if (BlocksThisRound > MAX_READ_SECTORS) {
                BlocksThisRound = MAX_READ_SECTORS;
            }

            Status = BopReadSectors(Loader, ClusterBlock, BlocksThisRound);
            if (!KSUCCESS(Status)) {
                goto MainEnd;
            }

            Loader += BlocksThisRound * SECTOR_SIZE;
            BlockIndex += BlocksThisRound;
            ClusterBlock += BlocksThisRound;
        }

        BoLoaderClustersRead += 1;
        LoaderClusterCount -= 1;
        if (LoaderClusterCount == 0) {
            break;
        }

        Status = BopFatGetNextCluster(Format, Scratch, &LoaderCluster);
        if (!KSUCCESS(Status)) {
            goto MainEnd;
        }
    }

    BoStepNumber += 1;

    //
    // Jump into the loader. This is not expected to return.
    //

    MainFunction = BOOT_MANAGER_ADDRESS;
    BopPrintString(0, 0, "Launch");
    ReturnValue = MainFunction(TopOfStack,
                               StackSize,
                               PartitionOffset,
                               BootDriveNumber);

    BopPrintString(0, 4, "Return");
    BopPrintHexInteger(7, 4, ReturnValue);
    Status = STATUS_DRIVER_FUNCTION_MISSING;

MainEnd:
    BopPrintString(0, 2, "Error ");
    BopPrintHexInteger(6, 2, Status);
    BopPrintString(0, 3, "Step ");
    BopPrintHexInteger(5, 3, BoStepNumber);
    while (TRUE) {
        NOTHING;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
BopPrintHexInteger (
    ULONG Column,
    ULONG Line,
    ULONG Integer
    )

/*++

Routine Description:

    This routine prints a hex integer on the second line of the screen.

Arguments:

    Column - Supplies the column to print at.

    Line - Supplies the line to print.

    Integer - Supplies the integer to print.

Return Value:

    Returns the destination pointer.

--*/

{

    CHAR Digit;
    ULONG Index;
    PUSHORT Screen;

    Screen = (PUSHORT)BIOS_TEXT_VIDEO_BASE +
             (BIOS_TEXT_VIDEO_COLUMNS * Line) + Column;

    for (Index = 0; Index < 8; Index += 1) {
        Digit = (Integer >> 28) & 0x0F;
        if (Digit < 10) {
            Digit += '0';

        } else {
            Digit += 'A' - 10;
        }

        Screen[Index] = TEXT_VIDEO_ATTRIBUTE | Digit;
        Integer <<= 4;
    }

    return;
}

VOID
BopPrintString (
    ULONG Column,
    ULONG Line,
    PSTR String
    )

/*++

Routine Description:

    This routine prints a string to the first line of the screen.

Arguments:

    Column - Supplies the column to print at.

    Line - Supplies the line to print.

    String - Supplies a pointer to the null terminated string to print.

Return Value:

    None.

--*/

{

    PUSHORT Screen;

    Screen = (PUSHORT)BIOS_TEXT_VIDEO_BASE +
             (BIOS_TEXT_VIDEO_COLUMNS * Line) + Column;

    while (*String != '\0') {
        *Screen = TEXT_VIDEO_ATTRIBUTE | *String;
        Screen += 1;
        String += 1;
    }

    return;
}

KSTATUS
BopReadSectors (
    PVOID Buffer,
    ULONG AbsoluteSector,
    ULONG SectorCount
    )

/*++

Routine Description:

    This routine uses the BIOS to read sectors off of the boot disk.

Arguments:

    Buffer - Supplies the buffer where the read data will be returned.

    AbsoluteSector - Supplies the zero-based sector number to read from.

    SectorCount - Supplies the number of sectors to read. The supplied buffer
        must be at least this large.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the BIOS returned an error.

    Other error codes.

--*/

{

    REAL_MODE_CONTEXT RealModeContext;
    PINT13_DISK_ACCESS_PACKET Request;
    KSTATUS Status;

    //
    // Create a standard BIOS call context.
    //

    Status = FwpRealModeCreateBiosCallContext(&RealModeContext, 0x13);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Create the disk access packet on the stack.
    //

    Request = (PINT13_DISK_ACCESS_PACKET)(RealModeContext.Esp -
                                          sizeof(INT13_DISK_ACCESS_PACKET));

    Request->PacketSize = sizeof(INT13_DISK_ACCESS_PACKET);
    Request->Reserved = 0;
    Request->BlockCount = SectorCount;
    Request->TransferBuffer = RealModeContext.DataPage.RealModeAddress;
    Request->BlockAddress = AbsoluteSector + BoBootPartitionStart;
    RealModeContext.Eax = INT13_EXTENDED_READ << BITS_PER_BYTE;
    RealModeContext.Edx = BoBootDriveNumber;
    RealModeContext.Esp = (UINTN)Request;
    RealModeContext.Esi = (UINTN)Request;

    //
    // Execute the firmware call.
    //

    FwpRealModeExecute(&RealModeContext);

    //
    // Check for an error (carry flag set). The status code is in Ah.
    //

    if (((RealModeContext.Eax & 0xFF00) != 0) ||
        ((RealModeContext.Eflags & IA32_EFLAG_CF) != 0)) {

        Status = STATUS_FIRMWARE_ERROR;
        goto BlockOperationEnd;
    }

    //
    // Copy the data over from the real mode data page to the caller's buffer.
    //

    RtlCopyMemory(Buffer,
                  RealModeContext.DataPage.Page,
                  SectorCount * SECTOR_SIZE);

    Status = STATUS_SUCCESS;

BlockOperationEnd:
    FwpRealModeDestroyBiosCallContext(&RealModeContext);
    return Status;
}

BOOL
BopMatchDirectoryEntry (
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
    PUSHORT Region;
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
                    Region = LongEntry->Name1;
                    RegionSize = FAT_LONG_DIRECTORY_ENTRY_NAME1_SIZE;

                } else if (RegionIndex == 1) {
                    Region = LongEntry->Name2;
                    RegionSize = FAT_LONG_DIRECTORY_ENTRY_NAME2_SIZE;

                } else {
                    Region = LongEntry->Name3;
                    RegionSize = FAT_LONG_DIRECTORY_ENTRY_NAME3_SIZE;
                }

                for (CharacterIndex = 0;
                     CharacterIndex < RegionSize;
                     CharacterIndex += 1) {

                    if (Name[NameIndex] == '\0') {
                        break;
                    }

                    if (Region[CharacterIndex] != Name[NameIndex]) {
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
        ComputedChecksum = BopFatChecksumDirectoryEntry(Entry);
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
BopFatGetNextCluster (
    FAT_FORMAT Format,
    PVOID ScratchBuffer,
    PULONG Cluster
    )

/*++

Routine Description:

    This routine finds the next cluster given a current cluster.

Arguments:

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
        *Cluster = FAT12_READ_CLUSTER(BoFat12FatRegion, *Cluster);
        return STATUS_SUCCESS;
    }

    if (Format == Fat16Format) {
        FatOffset = *Cluster / FAT16_CLUSTERS_PER_BLOCK;

    } else {
        FatOffset = *Cluster / FAT32_CLUSTERS_PER_BLOCK;
    }

    if (FatOffset >= BoFatSectorsPerFat) {
        return STATUS_VOLUME_CORRUPT;
    }

    Status = BopReadSectors(ScratchBuffer, BoFatFatBlockOffset + FatOffset, 1);
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
BopFatChecksumDirectoryEntry (
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

VOID
KdDebugExceptionHandler (
    ULONG Exception,
    PVOID Parameter,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles the debug break exception. It is usually called by an
    assembly routine responding to an exception.

Arguments:

    Exception - Supplies the type of exception that this function is handling.
        See EXCEPTION_* definitions for valid values.

    Parameter - Supplies a pointer to a parameter supplying additional
        context in the case of a debug service request.

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred. Also returns the possibly modified
        machine state.

Return Value:

    None.

--*/

{

    BopPrintString(0, 0, "Exception: ");
    BopPrintHexInteger(11, 0, Exception);
    BopPrintString(0, 1, "Step ");
    BopPrintHexInteger(5, 1, BoStepNumber);
    BopPrintString(0, 2, "eip ");
    BopPrintHexInteger(4, 2, TrapFrame->Eip);
    while (TRUE) {
        NOTHING;
    }

    return;
}

VOID
BoPageFaultHandler (
    PVOID FaultingAddress,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles page faults, or rather doesn't handle them.

Arguments:

    FaultingAddress - Supplies the address that caused the fault.

    TrapFrame - Supplies a pointer to the trap frame of the fault.

Return Value:

    None.

--*/

{

    BopPrintString(0, 0, "PageFault");
    while (TRUE) {
        NOTHING;
    }

    return;
}

VOID
BoDivideByZeroHandler (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine responds to a divide by zero exception.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame.

Return Value:

    None.

--*/

{

    BopPrintString(0, 0, "Div0");
    while (TRUE) {
        NOTHING;
    }

    return;
}

VOID
RtlZeroMemory (
    PVOID Buffer,
    UINTN ByteCount
    )

/*++

Routine Description:

    This routine zeroes out a section of memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to clear.

    ByteCount - Supplies the number of bytes to zero out.

Return Value:

    None.

--*/

{

    PUCHAR CurrentByte;

    CurrentByte = Buffer;
    while (ByteCount > 0) {
        *CurrentByte = 0;
        CurrentByte += 1;
        ByteCount -= 1;
    }

    return;
}

PVOID
RtlCopyMemory (
    PVOID Destination,
    PCVOID Source,
    UINTN ByteCount
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

