/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    partgpt.c

Abstract:

    This module implements UEFI GPT partition support.

Author:

    Evan Green 19-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "part.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOLEAN
EfipPartitionValidGptTable (
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_LBA Lba,
    EFI_PARTITION_TABLE_HEADER *PartitionHeader
    );

BOOLEAN
EfipPartitionCheckCrc (
    UINTN MaxSize,
    EFI_TABLE_HEADER *Header
    );

BOOLEAN
EfipPartitionCheckPartitionEntriesCrc (
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_PARTITION_TABLE_HEADER *PartitionHeader
    );

VOID
EfipPartitionCheckGptEntries (
    EFI_PARTITION_TABLE_HEADER *Header,
    EFI_PARTITION_ENTRY *Entries,
    EFI_PARTITION_ENTRY_STATUS *EntryStatus
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_GUID EfiPartitionTypeUnusedGuid = EFI_PARTITION_TYPE_UNUSED_GUID;
EFI_GUID EfiPartitionTypeSystemPartitionGuid =
                                            EFI_PARTITION_TYPE_EFI_SYSTEM_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiPartitionDetectGpt (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE Handle,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    )

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

{

    PEFI_PARTITION_TABLE_HEADER BackupHeader;
    UINT32 BlockSize;
    HARDDRIVE_DEVICE_PATH DrivePath;
    UINT32 EntriesSize;
    PEFI_PARTITION_ENTRY Entry;
    EFI_STATUS GptValidStatus;
    UINTN Index;
    EFI_LBA LastBlock;
    BOOLEAN Match;
    UINT32 MediaId;
    PEFI_PARTITION_ENTRY PartitionEntry;
    PEFI_PARTITION_ENTRY_STATUS PartitionEntryStatus;
    UINTN PartitionEntryStatusSize;
    PEFI_PARTITION_TABLE_HEADER PrimaryHeader;
    PEFI_MASTER_BOOT_RECORD ProtectiveMbr;
    EFI_STATUS Status;
    BOOLEAN SystemPartition;
    BOOLEAN Valid;

    ProtectiveMbr = NULL;
    PrimaryHeader = NULL;
    BackupHeader = NULL;
    PartitionEntry = NULL;
    PartitionEntryStatus = NULL;
    BlockSize = BlockIo->Media->BlockSize;
    LastBlock = BlockIo->Media->LastBlock;
    MediaId = BlockIo->Media->MediaId;
    GptValidStatus = EFI_NOT_FOUND;
    ProtectiveMbr = EfiCoreAllocateBootPool(BlockSize);
    if (ProtectiveMbr == NULL) {
        return EFI_NOT_FOUND;
    }

    //
    // Read the protective MBR from LBA zero.
    //

    Status = DiskIo->ReadDisk(DiskIo,
                              MediaId,
                              0,
                              BlockSize,
                              ProtectiveMbr);

    if (EFI_ERROR(Status)) {
        GptValidStatus = Status;
        goto PartitionDetectGptEnd;
    }

    //
    // Verify that the protective MBR is valid.
    //

    for (Index = 0; Index < EFI_MAX_MBR_PARTITIONS; Index += 1) {
        if ((ProtectiveMbr->Partition[Index].BootIndicator == 0x00) &&
            (ProtectiveMbr->Partition[Index].OsIndicator ==
             EFI_PROTECTIVE_MBR_PARTITION) &&
            (EFI_UNPACK_UINT32(ProtectiveMbr->Partition[Index].StartingLba) ==
             1)) {

            break;
        }
    }

    if (Index == EFI_MAX_MBR_PARTITIONS) {
        goto PartitionDetectGptEnd;
    }

    //
    // Allocate the GPT structures.
    //

    PrimaryHeader = EfiCoreAllocateBootPool(sizeof(EFI_PARTITION_TABLE_HEADER));
    if (PrimaryHeader == NULL) {
        goto PartitionDetectGptEnd;
    }

    BackupHeader = EfiCoreAllocateBootPool(sizeof(EFI_PARTITION_TABLE_HEADER));
    if (BackupHeader == NULL) {
        goto PartitionDetectGptEnd;
    }

    //
    // Check the primary and backup headers.
    //

    Valid = EfipPartitionValidGptTable(BlockIo,
                                       DiskIo,
                                       EFI_PRIMARY_PARTITION_HEADER_LBA,
                                       PrimaryHeader);

    if (Valid == FALSE) {
        Valid = EfipPartitionValidGptTable(BlockIo,
                                           DiskIo,
                                           LastBlock,
                                           BackupHeader);

        //
        // End now if neither the primary nor the backup header was valid.
        //

        if (Valid == FALSE) {
            goto PartitionDetectGptEnd;

        //
        // The primary header was bad but the backup header is valid.
        //

        } else {
            RtlDebugPrint("Warning: Primary GPT header was bad, using backup "
                          "header.\n");

            EfiCopyMem(PrimaryHeader,
                       BackupHeader,
                       sizeof(EFI_PARTITION_TABLE_HEADER));
        }

    //
    // The primary partition header is valid. Check the backup header.
    //

    } else {
        Valid = EfipPartitionValidGptTable(BlockIo,
                                           DiskIo,
                                           PrimaryHeader->AlternateLba,
                                           BackupHeader);

        if (Valid == FALSE) {
            RtlDebugPrint("Warning: Backup GPT header is invalid!\n");
        }
    }

    //
    // Read the EFI partition entries.
    //

    EntriesSize = PrimaryHeader->NumberOfPartitionEntries *
                  PrimaryHeader->SizeOfPartitionEntry;

    PartitionEntry = EfiCoreAllocateBootPool(EntriesSize);
    if (PartitionEntry == NULL) {
        goto PartitionDetectGptEnd;
    }

    Status = DiskIo->ReadDisk(DiskIo,
                              MediaId,
                              PrimaryHeader->PartitionEntryLba * BlockSize,
                              EntriesSize,
                              PartitionEntry);

    if (EFI_ERROR(Status)) {
        GptValidStatus = Status;
        goto PartitionDetectGptEnd;
    }

    PartitionEntryStatusSize = PrimaryHeader->NumberOfPartitionEntries *
                               sizeof(EFI_PARTITION_ENTRY_STATUS);

    PartitionEntryStatus = EfiCoreAllocateBootPool(PartitionEntryStatusSize);
    if (PartitionEntryStatus == NULL) {
        goto PartitionDetectGptEnd;
    }

    EfiSetMem(PartitionEntryStatus, PartitionEntryStatusSize, 0);

    //
    // Check the integrity of the partition entries.
    //

    EfipPartitionCheckGptEntries(PrimaryHeader,
                                 PartitionEntry,
                                 PartitionEntryStatus);

    //
    // Everything looks pretty valid.
    //

    GptValidStatus = EFI_SUCCESS;

    //
    // Create child device handles.
    //

    for (Index = 0;
         Index < PrimaryHeader->NumberOfPartitionEntries;
         Index += 1) {

        Entry = (EFI_PARTITION_ENTRY *)((UINT8 *)PartitionEntry +
                                        (Index *
                                         PrimaryHeader->SizeOfPartitionEntry));

        Match = EfiCoreCompareGuids(&(Entry->PartitionTypeGuid),
                                    &EfiPartitionTypeUnusedGuid);

        if ((Match != FALSE) ||
            (PartitionEntryStatus[Index].OutOfRange != FALSE) ||
            (PartitionEntryStatus[Index].Overlap != FALSE) ||
            (PartitionEntryStatus[Index].OsSpecific != FALSE)) {

            //
            // Don't use null entries, invalid entries, or OS-specific entries.
            //

            continue;
        }

        EfiSetMem(&DrivePath, sizeof(DrivePath), 0);
        DrivePath.Header.Type = MEDIA_DEVICE_PATH;
        DrivePath.Header.SubType = MEDIA_HARDDRIVE_DP;
        EfiCoreSetDevicePathNodeLength(&(DrivePath.Header), sizeof(DrivePath));
        DrivePath.PartitionNumber = (UINT32)Index + 1;
        DrivePath.MBRType = MBR_TYPE_EFI_PARTITION_TABLE_HEADER;
        DrivePath.SignatureType = SIGNATURE_TYPE_GUID;
        DrivePath.PartitionStart = Entry->StartingLba;
        DrivePath.PartitionSize = Entry->EndingLba - Entry->StartingLba + 1;
        EfiCopyMem(&(DrivePath.Signature),
                   &(Entry->UniquePartitionGuid), sizeof(EFI_GUID));

        SystemPartition =
                     EfiCoreCompareGuids(&(Entry->PartitionTypeGuid),
                                         &EfiPartitionTypeSystemPartitionGuid);

        Status = EfiPartitionInstallChildHandle(
                                        This,
                                        Handle,
                                        DiskIo,
                                        BlockIo,
                                        DevicePath,
                                        (EFI_DEVICE_PATH_PROTOCOL *)&DrivePath,
                                        Entry->StartingLba,
                                        Entry->EndingLba,
                                        BlockSize,
                                        SystemPartition);
    }

PartitionDetectGptEnd:
    if (ProtectiveMbr != NULL) {
        EfiFreePool(ProtectiveMbr);
    }

    if (PrimaryHeader != NULL) {
        EfiFreePool(PrimaryHeader);
    }

    if (BackupHeader != NULL) {
        EfiFreePool(BackupHeader);
    }

    if (PartitionEntry != NULL) {
        EfiFreePool(PartitionEntry);
    }

    if (PartitionEntryStatus != NULL) {
        EfiFreePool(PartitionEntryStatus);
    }

    return GptValidStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOLEAN
EfipPartitionValidGptTable (
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_LBA Lba,
    EFI_PARTITION_TABLE_HEADER *PartitionHeader
    )

/*++

Routine Description:

    This routine determines if the given partition table header is valid.

Arguments:

    BlockIo - Supplies a pointer to the block I/O protocol.

    DiskIo - Supplies a pointer to the disk I/O protocol.

    Lba - Supplies the LBA to read.

    PartitionHeader - Supplies a pointer where the partition table will be read
        and returned.

Return Value:

    TRUE if the header is valid.

    FALSE if the header is invalid.

--*/

{

    UINT32 BlockSize;
    EFI_PARTITION_TABLE_HEADER *Header;
    UINT32 MediaId;
    EFI_STATUS Status;
    BOOLEAN Valid;

    BlockSize = BlockIo->Media->BlockSize;
    MediaId = BlockIo->Media->MediaId;
    Header = EfiCoreAllocateBootPool(BlockSize);
    if (Header == NULL) {
        return FALSE;
    }

    EfiSetMem(Header, BlockSize, 0);
    Status = DiskIo->ReadDisk(DiskIo,
                              MediaId,
                              Lba * BlockSize,
                              BlockSize,
                              Header);

    if (EFI_ERROR(Status)) {
        EfiFreePool(Header);
        return FALSE;
    }

    if ((Header->Header.Signature != EFI_GPT_HEADER_SIGNATURE) ||
        (EfipPartitionCheckCrc(BlockSize, &(Header->Header)) == FALSE) ||
        (Header->MyLba != Lba) ||
        (Header->SizeOfPartitionEntry < sizeof(EFI_PARTITION_ENTRY))) {

        EfiFreePool(Header);
        return FALSE;
    }

    EfiCopyMem(PartitionHeader, Header, sizeof(EFI_PARTITION_TABLE_HEADER));
    Valid = EfipPartitionCheckPartitionEntriesCrc(BlockIo,
                                                  DiskIo,
                                                  PartitionHeader);

    EfiFreePool(Header);
    return Valid;
}

BOOLEAN
EfipPartitionCheckCrc (
    UINTN MaxSize,
    EFI_TABLE_HEADER *Header
    )

/*++

Routine Description:

    This routine validates the CRC of the partition header.

Arguments:

    MaxSize - Supplies the maximum size of the buffer.

    Header - Supplies a pointer to the header to validate.

Return Value:

    TRUE if the CRC is valid.

    FALSE if the CRC is invalid.

--*/

{

    UINT32 Crc;
    UINT32 OriginalCrc;
    UINTN Size;

    Size = Header->HeaderSize;
    Crc = 0;
    if (Size == 0) {
        return FALSE;
    }

    if ((MaxSize != 0) && (Size > MaxSize)) {
        return FALSE;
    }

    OriginalCrc = Header->CRC32;
    Header->CRC32 = 0;
    EfiCalculateCrc32((UINT8 *)Header, Size, &Crc);
    Header->CRC32 = Crc;
    if (Crc != OriginalCrc) {
        return FALSE;
    }

    return TRUE;
}

BOOLEAN
EfipPartitionCheckPartitionEntriesCrc (
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_PARTITION_TABLE_HEADER *PartitionHeader
    )

/*++

Routine Description:

    This routine validates the CRC of the partition entries.

Arguments:

    BlockIo - Supplies a pointer to the block I/O protocol.

    DiskIo - Supplies a pointer to the disk I/O protocol.

    PartitionHeader - Supplies a pointer to the GPT header.

Return Value:

    TRUE if the CRC is valid.

    FALSE if the CRC is invalid.

--*/

{

    UINT8 *Buffer;
    UINT32 Crc;
    UINTN EntriesSize;
    UINT64 Offset;
    EFI_STATUS Status;
    BOOLEAN Valid;

    EntriesSize = PartitionHeader->NumberOfPartitionEntries *
                  PartitionHeader->SizeOfPartitionEntry;

    Buffer = EfiCoreAllocateBootPool(EntriesSize);
    if (Buffer == NULL) {
        return FALSE;
    }

    Offset = PartitionHeader->PartitionEntryLba *
             BlockIo->Media->BlockSize;

    Status = DiskIo->ReadDisk(DiskIo,
                              BlockIo->Media->MediaId,
                              Offset,
                              EntriesSize,
                              Buffer);

    if (EFI_ERROR(Status)) {
        EfiFreePool(Buffer);
        return FALSE;
    }

    Valid = FALSE;
    Status = EfiCalculateCrc32(Buffer, EntriesSize, &Crc);
    if (EFI_ERROR(Status)) {
        RtlDebugPrint("GPT: Needed CRC and it wasn't there!\n");

    } else if (PartitionHeader->PartitionEntryArrayCrc32 == Crc) {
        Valid = TRUE;
    }

    EfiFreePool(Buffer);
    return Valid;
}

VOID
EfipPartitionCheckGptEntries (
    EFI_PARTITION_TABLE_HEADER *Header,
    EFI_PARTITION_ENTRY *Entries,
    EFI_PARTITION_ENTRY_STATUS *EntryStatus
    )

/*++

Routine Description:

    This routine checks the validity of the partition entry array.

Arguments:

    Header - Supplies a pointer to the partition entry header.

    Entries - Supplies a pointer to the partition entries to validate.

    EntryStatus - Supplies a pointer where the validity information of each
        partition will be returned.

Return Value:

    None.

--*/

{

    UINTN CompareIndex;
    EFI_LBA EndingLba;
    EFI_PARTITION_ENTRY *Entry;
    UINTN EntryIndex;
    BOOLEAN Match;
    EFI_LBA StartingLba;

    for (EntryIndex = 0;
         EntryIndex < Header->NumberOfPartitionEntries;
         EntryIndex += 1) {

        Entry = (EFI_PARTITION_ENTRY *)((UINT8 *)Entries +
                                        (EntryIndex *
                                         Header->SizeOfPartitionEntry));

        Match = EfiCoreCompareGuids(&(Entry->PartitionTypeGuid),
                                    &EfiPartitionTypeUnusedGuid);

        if (Match != FALSE) {
            continue;
        }

        StartingLba = Entry->StartingLba;
        EndingLba = Entry->EndingLba;
        if ((StartingLba > EndingLba) ||
            (StartingLba < Header->FirstUsableLba) ||
            (EndingLba < Header->FirstUsableLba) ||
            (EndingLba > Header->LastUsableLba)) {

            EntryStatus[EntryIndex].OutOfRange = TRUE;
            continue;
        }

        if ((Entry->Attributes & EFI_GPT_ATTRIBUTE_OS_SPECIFIC) != 0) {
            EntryStatus[EntryIndex].OsSpecific = TRUE;
        }

        for (CompareIndex = EntryIndex + 1;
             CompareIndex < Header->NumberOfPartitionEntries;
             CompareIndex += 1) {

            Entry = (EFI_PARTITION_ENTRY *)((UINT8 *)Entries +
                                            (CompareIndex *
                                             Header->SizeOfPartitionEntry));

            Match = EfiCoreCompareGuids(&(Entry->PartitionTypeGuid),
                                        &EfiPartitionTypeUnusedGuid);

            if (Match != FALSE) {
                continue;
            }

            if ((Entry->EndingLba >= StartingLba) &&
                (Entry->StartingLba <= EndingLba)) {

                EntryStatus[CompareIndex].Overlap = TRUE;
                EntryStatus[EntryIndex].Overlap = TRUE;
            }
        }
    }

    return;
}

