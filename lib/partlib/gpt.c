/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gpt.c

Abstract:

    This module contains GPT support for the partition library.

Author:

    Evan Green 6-Feb-2014

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "partlibp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PartpGptReadEntries (
    PPARTITION_CONTEXT Context,
    PVOID Block,
    PGPT_PARTITION_ENTRY *Entries,
    PVOID *EntriesAllocation,
    PULONG EntryCount,
    PULONG ValidCount
    );

KSTATUS
PartpGptWriteBlocks (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    ULONG BlockCount,
    PVOID Buffer
    );

KSTATUS
PartpGptConvertPartitionTypeToGuid (
    PARTITION_TYPE PartitionType,
    UCHAR TypeGuid[GPT_GUID_SIZE]
    );

BOOL
PartpGptIsGuidEmpty (
    UCHAR Guid[GPT_GUID_SIZE]
    );

BOOL
PartpGptAreGuidsEqual (
    UCHAR FirstGuid[GPT_GUID_SIZE],
    UCHAR SecondGuid[GPT_GUID_SIZE]
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the mapping of known partition types. The first entry here is
// assumed to be the empty GUID.
//

PARTITION_TYPE_GUID_MAPPING PartTypeGuidToPartitionTypeTable[] = {
    {GPT_PARTITION_TYPE_EMPTY, PartitionTypeEmpty},
    {GPT_PARTITION_TYPE_EFI_SYSTEM, PartitionTypeEfiSystem},
    {GPT_PARTITION_TYPE_MINOCA, PartitionTypeMinoca},
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
PartpGptIsProtectiveMbr (
    PARTITION_TABLE_ENTRY Entry[PARTITION_TABLE_SIZE]
    )

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

{

    BOOL FoundEfiEntry;
    ULONG Index;

    FoundEfiEntry = FALSE;
    for (Index = 0; Index < PARTITION_TABLE_SIZE; Index += 1) {
        if (Entry[Index].SystemId == PARTITION_ID_EMPTY) {
            continue;
        }

        if ((Entry[Index].SystemId == PARTITION_ID_EFI_GPT) &&
            (Entry[Index].StartingLba == 1)) {

            FoundEfiEntry = TRUE;

        //
        // Anything other than empty, GPT, or EFI system partitions mean it's
        // not a GPT disk.
        //

        } else {
            return FALSE;
        }
    }

    return FoundEfiEntry;
}

KSTATUS
PartpGptEnumeratePartitions (
    PPARTITION_CONTEXT Context
    )

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

{

    ULONG AllocationSize;
    PVOID Block;
    PVOID BlockAllocation;
    ULONG BlockSize;
    ULONG EntryCount;
    ULONG EntryIndex;
    PGPT_PARTITION_ENTRY GptEntry;
    ULONGLONG GptHeaderLba;
    PGPT_HEADER Header;
    PPARTITION_INFORMATION Information;
    ULONG InformationIndex;
    PPARTITION_INFORMATION Partition;
    PGPT_PARTITION_ENTRY PartitionEntries;
    PVOID PartitionEntriesAllocation;
    KSTATUS Status;
    ULONG ValidCount;

    BlockSize = Context->BlockSize;
    Information = NULL;
    PartitionEntriesAllocation = NULL;
    BlockAllocation = PartpAllocateIo(Context, BlockSize, &Block);
    if (BlockAllocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GptEnumeratePartitionsEnd;
    }

    //
    // Read LBA 1 to get the GPT header.
    //

    GptHeaderLba = 1;
    Status = Context->ReadFunction(Context, GptHeaderLba, Block);
    if (!KSUCCESS(Status)) {
        goto GptEnumeratePartitionsEnd;
    }

    //
    // Validate the header here. If it wasn't valid, try the header that's at
    // the last sector of the disk.
    //

    Status = PartpGptReadEntries(Context,
                                 Block,
                                 &PartitionEntries,
                                 &PartitionEntriesAllocation,
                                 &EntryCount,
                                 &ValidCount);

    if ((!KSUCCESS(Status)) && (Context->BlockCount != 0)) {
        GptHeaderLba = Context->BlockCount - 1;
        Status = Context->ReadFunction(Context, GptHeaderLba, Block);
        if (!KSUCCESS(Status)) {
            goto GptEnumeratePartitionsEnd;
        }

        Status = PartpGptReadEntries(Context,
                                     Block,
                                     &PartitionEntries,
                                     &PartitionEntriesAllocation,
                                     &EntryCount,
                                     &ValidCount);
    }

    //
    // If neither header is valid, abort.
    //

    if (!KSUCCESS(Status)) {
        goto GptEnumeratePartitionsEnd;
    }

    Header = (PGPT_HEADER)Block;

    ASSERT(sizeof(Context->DiskIdentifier) >= sizeof(Header->DiskGuid));

    RtlCopyMemory(Context->DiskIdentifier,
                  Header->DiskGuid,
                  sizeof(Context->DiskIdentifier));

    if (ValidCount == 0) {
        Status = STATUS_SUCCESS;
        goto GptEnumeratePartitionsEnd;
    }

    //
    // Allocate space for the partition information structures.
    //

    AllocationSize = sizeof(PARTITION_INFORMATION) * ValidCount;
    Information = Context->AllocateFunction(AllocationSize);
    if (Information == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GptEnumeratePartitionsEnd;
    }

    RtlZeroMemory(Information, AllocationSize);

    //
    // Loop through and convert all the GPT entries to partition information
    // structures.
    //

    InformationIndex = 0;
    for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
        GptEntry = &(PartitionEntries[EntryIndex]);
        if ((GptEntry->FirstLba == 0) ||
            (GptEntry->LastLba == 0)) {

            continue;
        }

        if (PartpGptIsGuidEmpty(GptEntry->TypeGuid) != FALSE) {
            continue;
        }

        ASSERT(InformationIndex < ValidCount);

        Partition = &(Information[InformationIndex]);
        Partition->StartOffset = GptEntry->FirstLba;
        Partition->EndOffset = GptEntry->LastLba + 1;
        Partition->Number = EntryIndex + 1;

        ASSERT(PARTITION_IDENTIFIER_SIZE >= GPT_GUID_SIZE);

        RtlCopyMemory(Partition->TypeIdentifier,
                      GptEntry->TypeGuid,
                      sizeof(Partition->TypeIdentifier));

        RtlCopyMemory(Partition->Identifier,
                      GptEntry->Guid,
                      sizeof(Partition->TypeIdentifier));

        Partition->PartitionType =
                    PartpGptConvertTypeGuidToPartitionType(GptEntry->TypeGuid);

        InformationIndex += 1;
    }

    ASSERT(InformationIndex == ValidCount);

    Context->Partitions = Information;
    Context->PartitionCount = InformationIndex;
    Information = NULL;
    Status = STATUS_SUCCESS;

GptEnumeratePartitionsEnd:
    if (PartitionEntriesAllocation != NULL) {
        Context->FreeFunction(PartitionEntriesAllocation);
    }

    if (Information != NULL) {
        Context->FreeFunction(Information);
    }

    if (BlockAllocation != NULL) {
        Context->FreeFunction(BlockAllocation);
    }

    return Status;
}

KSTATUS
PartpGptWritePartitionLayout (
    PPARTITION_CONTEXT Context,
    PPARTITION_INFORMATION Partitions,
    ULONG PartitionCount,
    BOOL CleanMbr
    )

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

{

    ULONG BlockSize;
    ULONG EntriesBlockCount;
    ULONG EntriesSize;
    ULONG EntryCount;
    ULONGLONG FirstUsableBlock;
    PGPT_PARTITION_ENTRY GptEntries;
    PGPT_PARTITION_ENTRY GptEntry;
    PGPT_HEADER Header;
    ULONGLONG LastUsableBlock;
    PPARTITION_TABLE_ENTRY MbrEntry;
    PPARTITION_INFORMATION Partition;
    ULONG PartitionIndex;
    PUSHORT Pointer16;
    KSTATUS Status;
    PVOID Table;
    PVOID TableAllocation;

    TableAllocation = NULL;
    if (Context->BlockCount < 12) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto GptWritePartitionLayoutEnd;
    }

    if (Context->FillRandomFunction == NULL) {
        Status = STATUS_NOT_INITIALIZED;
        goto GptWritePartitionLayoutEnd;
    }

    BlockSize = Context->BlockSize;

    ASSERT(POWER_OF_2(BlockSize));

    EntriesSize = PartitionCount * sizeof(GPT_PARTITION_ENTRY);
    if (EntriesSize < GPT_MINIMUM_PARTITION_ENTRIES_SIZE) {
        EntriesSize = GPT_MINIMUM_PARTITION_ENTRIES_SIZE;
    }

    EntriesSize = ALIGN_RANGE_UP(EntriesSize, BlockSize);
    EntriesBlockCount = (EntriesSize / BlockSize);
    EntryCount = EntriesSize / sizeof(GPT_PARTITION_ENTRY);

    //
    // Make sure the first usable block is aligned to 4KB so that disks that
    // internally use 4KB sectors but report 512 byte sectors don't have
    // performance issues doing read/modify/writes all the time within the
    // partition. Remember that the first two blocks are reserved for the
    // protective MBR and the GPT header.
    //

    FirstUsableBlock = 2 + EntriesBlockCount;
    FirstUsableBlock = ALIGN_RANGE_UP(FirstUsableBlock * BlockSize,
                                      GPT_PARTITION_ALIGNMENT);

    FirstUsableBlock /= BlockSize;

    //
    // Allocate space for the entire table.
    //

    TableAllocation = PartpAllocateIo(Context,
                                      FirstUsableBlock * BlockSize,
                                      &Table);

    if (TableAllocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GptWritePartitionLayoutEnd;
    }

    RtlZeroMemory(Table, FirstUsableBlock * BlockSize);
    Header = (PGPT_HEADER)(Table + BlockSize);
    LastUsableBlock = (Context->BlockCount - 1) - (EntriesBlockCount + 1);

    //
    // First fill out the partition entries array.
    //

    GptEntries = (PGPT_PARTITION_ENTRY)(Table + (2 * BlockSize));
    for (PartitionIndex = 0;
         PartitionIndex < PartitionCount;
         PartitionIndex += 1) {

        GptEntry = &(GptEntries[PartitionIndex]);
        Partition = &(Partitions[PartitionIndex]);

        //
        // Convert the type enum if it's set, otherwise, just copy the type
        // identifier GUID.
        //

        if ((Partition->PartitionType != PartitionTypeInvalid) &&
            (Partition->PartitionType != PartitionTypeUnknown)) {

            Status = PartpGptConvertPartitionTypeToGuid(
                                                      Partition->PartitionType,
                                                      GptEntry->TypeGuid);

            if (!KSUCCESS(Status)) {
                goto GptWritePartitionLayoutEnd;
            }

        } else {

            ASSERT(sizeof(GptEntry->TypeGuid) <=
                   sizeof(Partition->TypeIdentifier));

            RtlCopyMemory(GptEntry->TypeGuid,
                          Partition->TypeIdentifier,
                          sizeof(GptEntry->TypeGuid));
        }

        //
        // Copy the GUID if it's not the empty one. If it is, create a random
        // one.
        //

        if (PartpGptIsGuidEmpty(Partition->Identifier) != FALSE) {
            Context->FillRandomFunction(Context,
                                        GptEntry->Guid,
                                        sizeof(GptEntry->Guid));

        } else {

            ASSERT(sizeof(GptEntry->Guid) <= sizeof(Partition->Identifier));

            RtlCopyMemory(GptEntry->Guid,
                          Partition->Identifier,
                          sizeof(GptEntry->Guid));
        }

        GptEntry->Attributes = Partition->Attributes;
        GptEntry->FirstLba = Partition->StartOffset;
        if (Partition->EndOffset != 0) {
            GptEntry->LastLba = Partition->EndOffset - 1;
        }

        if ((GptEntry->FirstLba != GptEntry->LastLba) &&
            ((GptEntry->FirstLba < FirstUsableBlock) ||
             (GptEntry->FirstLba > LastUsableBlock) ||
             (GptEntry->LastLba < FirstUsableBlock) ||
             (GptEntry->LastLba > LastUsableBlock))) {

            Status = STATUS_INVALID_CONFIGURATION;
            goto GptWritePartitionLayoutEnd;
        }
    }

    //
    // Create the backup copy first as requested by the specification.
    //

    Header->Signature = GPT_HEADER_SIGNATURE;
    Header->Revision = GPT_HEADER_REVISION_1;
    Header->HeaderSize = sizeof(GPT_HEADER);
    Header->CurrentLba = Context->BlockCount - 1;
    Header->BackupLba = 1;
    Header->FirstUsableLba = FirstUsableBlock;
    Header->LastUsableLba = LastUsableBlock;
    if (PartpGptIsGuidEmpty(Context->DiskIdentifier) != FALSE) {
        Context->FillRandomFunction(Context,
                                    Header->DiskGuid,
                                    sizeof(Header->DiskGuid));

    } else {
        RtlCopyMemory(Header->DiskGuid,
                      Context->DiskIdentifier,
                      sizeof(Header->DiskGuid));
    }

    Header->PartitionEntriesLba = Header->CurrentLba - EntriesBlockCount;
    Header->PartitionEntryCount = EntryCount;
    Header->PartitionEntrySize = sizeof(GPT_PARTITION_ENTRY);
    Header->PartitionArrayCrc32 = RtlComputeCrc32(
                                     0,
                                     GptEntries,
                                     EntryCount * sizeof(GPT_PARTITION_ENTRY));

    Header->HeaderCrc32 = RtlComputeCrc32(0, Header, Header->HeaderSize);

    //
    // Write out the header, then the partition entries array.
    //

    Status = PartpGptWriteBlocks(Context, Header->CurrentLba, 1, Header);
    if (!KSUCCESS(Status)) {
        goto GptWritePartitionLayoutEnd;
    }

    Status = PartpGptWriteBlocks(Context,
                                 Header->PartitionEntriesLba,
                                 EntriesBlockCount,
                                 GptEntries);

    if (!KSUCCESS(Status)) {
        goto GptWritePartitionLayoutEnd;
    }

    //
    // Create the protective MBR.
    //

    Pointer16 = (PUSHORT)(Table + PARTITION_SIGNATURE_OFFSET);
    *Pointer16 = PARTITION_SIGNATURE;
    MbrEntry = (PPARTITION_TABLE_ENTRY)(Table + PARTITION_TABLE_OFFSET);
    MbrEntry->StartingSector = 1;
    MbrEntry->SystemId = PARTITION_ID_EFI_GPT;
    MbrEntry->EndingHead = 0xFE;
    MbrEntry->EndingSector = 0xFF;
    MbrEntry->EndingCylinder = 0xFF;
    MbrEntry->StartingLba = 1;
    if (Context->BlockCount - 1 > MAX_ULONG) {
        MbrEntry->SectorCount = MAX_ULONG;

    } else {
        MbrEntry->SectorCount = Context->BlockCount - 1;
    }

    //
    // Fix up the GPT header for the beginning of the disk.
    //

    Header->HeaderCrc32 = 0;
    Header->BackupLba = Header->CurrentLba;
    Header->CurrentLba = 1;
    Header->PartitionEntriesLba = 2;
    Header->HeaderCrc32 = RtlComputeCrc32(0, Header, Header->HeaderSize);

    //
    // Finally, write out the MBR, GPT header, and entries all at once.
    //

    Status = PartpGptWriteBlocks(Context, 0, FirstUsableBlock, Table);
    if (!KSUCCESS(Status)) {
        goto GptWritePartitionLayoutEnd;
    }

GptWritePartitionLayoutEnd:
    if (TableAllocation != NULL) {
        Context->FreeFunction(TableAllocation);
    }

    return Status;
}

PARTITION_TYPE
PartpGptConvertTypeGuidToPartitionType (
    UCHAR TypeGuid[GPT_GUID_SIZE]
    )

/*++

Routine Description:

    This routine converts a partition type GUID into a partition type to the
    best of its abilities.

Arguments:

    TypeGuid - Supplies a pointer to the partition type GUID.

Return Value:

    Returns a partition type for this type GUID.

--*/

{

    ULONG EntryCount;
    ULONG EntryIndex;
    BOOL Match;

    EntryCount = sizeof(PartTypeGuidToPartitionTypeTable) /
                 sizeof(PartTypeGuidToPartitionTypeTable[0]);

    for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
        Match = PartpGptAreGuidsEqual(
                         PartTypeGuidToPartitionTypeTable[EntryIndex].TypeGuid,
                         TypeGuid);

        if (Match != FALSE) {
            return PartTypeGuidToPartitionTypeTable[EntryIndex].PartitionType;
        }
    }

    return PartitionTypeUnknown;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PartpGptReadEntries (
    PPARTITION_CONTEXT Context,
    PVOID Block,
    PGPT_PARTITION_ENTRY *Entries,
    PVOID *EntriesAllocation,
    PULONG EntryCount,
    PULONG ValidCount
    )

/*++

Routine Description:

    This routine determines if the GPT header is valid, and reads the
    partition entries if so.

Arguments:

    Context - Supplies a pointer to the initialized context.

    Block - Supplies a pointer to the block containing the GPT header.

    Entries - Supplies a pointer where a pointer will be returned to the
        GPT partition entries on success.

    EntriesAllocation - Supplies a pointer where a pointer to the actual
        entries allocation will be returned. This is what should be passed to
        the free function.

    EntryCount - Supplies a pointer where the number of GPT entries in the
        array will be returned.

    ValidCount - Supplies a pointer where the number of non-empty partitions
        will be returned on success.

Return Value:

    STATUS_SUCCESS if the partition information could be determined. There
    could still be zero partitions in this case.

    STATUS_NO_ELIGIBLE_DEVICES if the header is invalid.

    Other errors on read or allocation failures.

--*/

{

    ULONG AllocationSize;
    ULONG BlockCount;
    ULONG BlockIndex;
    ULONG BlockSize;
    ULONG ComputedCrc;
    PGPT_PARTITION_ENTRY Entry;
    ULONG EntryIndex;
    PGPT_HEADER Header;
    ULONG HeaderCrc;
    PGPT_PARTITION_ENTRY PartitionEntries;
    PVOID PartitionEntriesAllocation;
    ULONG PartitionEntryCount;
    KSTATUS Status;
    ULONG ValidPartitionCount;

    Status = STATUS_NO_ELIGIBLE_DEVICES;
    BlockSize = Context->BlockSize;
    Header = (PGPT_HEADER)Block;
    PartitionEntriesAllocation = NULL;
    PartitionEntryCount = 0;
    ValidPartitionCount = 0;
    if (Header->Signature != GPT_HEADER_SIGNATURE) {
        goto GptValidateHeaderEnd;
    }

    if (Header->Revision < GPT_HEADER_REVISION_1) {
        goto GptValidateHeaderEnd;
    }

    //
    // Validate that the reported sizes of the header and partition entry are
    // reasonable.
    //

    if ((Header->HeaderSize < sizeof(GPT_HEADER)) ||
        (Header->HeaderSize > Context->BlockSize)) {

        goto GptValidateHeaderEnd;
    }

    if ((Header->PartitionEntrySize < sizeof(GPT_PARTITION_ENTRY)) ||
        (Header->PartitionEntrySize > Context->BlockSize)) {

        goto GptValidateHeaderEnd;
    }

    //
    // Validate that the partition entries live outside the usable data.
    //

    if ((Header->PartitionEntriesLba >= Header->FirstUsableLba) &&
        (Header->PartitionEntriesLba <= Header->LastUsableLba)) {

        goto GptValidateHeaderEnd;
    }

    HeaderCrc = Header->HeaderCrc32;
    Header->HeaderCrc32 = 0;
    ComputedCrc = RtlComputeCrc32(0, Header, Header->HeaderSize);
    Header->HeaderCrc32 = HeaderCrc;
    if (ComputedCrc != HeaderCrc) {
        goto GptValidateHeaderEnd;
    }

    //
    // Allocate space for the partition entries.
    //

    PartitionEntryCount = Header->PartitionEntryCount;
    AllocationSize = PartitionEntryCount * Header->PartitionEntrySize;
    if (AllocationSize == 0) {
        Status = STATUS_SUCCESS;
        goto GptValidateHeaderEnd;
    }

    AllocationSize = ALIGN_RANGE_UP(AllocationSize, BlockSize);
    PartitionEntriesAllocation = PartpAllocateIo(Context,
                                                 AllocationSize,
                                                 (PVOID *)&PartitionEntries);

    if (PartitionEntriesAllocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GptValidateHeaderEnd;
    }

    BlockCount = AllocationSize / BlockSize;
    for (BlockIndex = 0; BlockIndex < BlockCount; BlockIndex += 1) {
        Status = Context->ReadFunction(
                          Context,
                          Header->PartitionEntriesLba + BlockIndex,
                          (PUCHAR)PartitionEntries + (BlockIndex * BlockSize));

        if (!KSUCCESS(Status)) {
            goto GptValidateHeaderEnd;
        }
    }

    //
    // Validate the CRC for the partition entries.
    //

    ComputedCrc = RtlComputeCrc32(
                     0,
                     PartitionEntries,
                     Header->PartitionEntryCount * Header->PartitionEntrySize);

    if (ComputedCrc != Header->PartitionArrayCrc32) {
        Status = STATUS_NO_ELIGIBLE_DEVICES;
        goto GptValidateHeaderEnd;
    }

    //
    // Loop through and count the valid entries.
    //

    for (EntryIndex = 0; EntryIndex < PartitionEntryCount; EntryIndex += 1) {
        Entry = &(PartitionEntries[EntryIndex]);
        if ((Entry->FirstLba == 0) || (Entry->LastLba == 0)) {
            continue;
        }

        if (PartpGptIsGuidEmpty(Entry->TypeGuid) != FALSE) {
            continue;
        }

        ValidPartitionCount += 1;
    }

    Status = STATUS_SUCCESS;

GptValidateHeaderEnd:
    if (!KSUCCESS(Status)) {
        if (PartitionEntriesAllocation != NULL) {
            Context->FreeFunction(PartitionEntriesAllocation);
            PartitionEntries = NULL;
            PartitionEntriesAllocation = NULL;
        }

        PartitionEntryCount = 0;
        ValidPartitionCount = 0;
    }

    *Entries = PartitionEntries;
    *EntriesAllocation = PartitionEntriesAllocation;
    *EntryCount = PartitionEntryCount;
    *ValidCount = ValidPartitionCount;
    return Status;
}

KSTATUS
PartpGptWriteBlocks (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    ULONG BlockCount,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine writes multiple blocks to the disk.

Arguments:

    Context - Supplies a pointer to the partition context.

    BlockAddress - Supplies the block address to write to.

    BlockCount - Supplies the number of blocks to write.

    Buffer - Supplies a pointer to the buffer containing the data to write.

Return Value:

    Status code.

--*/

{

    ULONG BlockIndex;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    for (BlockIndex = 0; BlockIndex < BlockCount; BlockIndex += 1) {
        Status = Context->WriteFunction(Context, BlockAddress, Buffer);
        if (!KSUCCESS(Status)) {
            goto GptWriteBlocksEnd;
        }

        Buffer += Context->BlockSize;
        BlockAddress += 1;
    }

GptWriteBlocksEnd:
    return Status;
}

KSTATUS
PartpGptConvertPartitionTypeToGuid (
    PARTITION_TYPE PartitionType,
    UCHAR TypeGuid[GPT_GUID_SIZE]
    )

/*++

Routine Description:

    This routine converts a partition type enum into its corresponding GPT GUID.

Arguments:

    PartitionType - Supplies the partition type.

    TypeGuid - Supplies a pointer where the GUID will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the partition type could not be converted.

--*/

{

    ULONG EntryCount;
    ULONG EntryIndex;

    EntryCount = sizeof(PartTypeGuidToPartitionTypeTable) /
                 sizeof(PartTypeGuidToPartitionTypeTable[0]);

    for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
        if (PartTypeGuidToPartitionTypeTable[EntryIndex].PartitionType ==
            PartitionType) {

            RtlCopyMemory(TypeGuid,
                          PartTypeGuidToPartitionTypeTable[EntryIndex].TypeGuid,
                          GPT_GUID_SIZE);

            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_PARAMETER;
}

BOOL
PartpGptIsGuidEmpty (
    UCHAR Guid[GPT_GUID_SIZE]
    )

/*++

Routine Description:

    This routine determines if the given GUID is all zero.

Arguments:

    Guid - Supplies the guid to compare.

Return Value:

    TRUE if the GUID is all zero.

    FALSE if the GUID has something in there.

--*/

{

    BOOL Match;

    //
    // The first entry is known to be the empty entry.
    //

    Match = PartpGptAreGuidsEqual(Guid,
                                  PartTypeGuidToPartitionTypeTable[0].TypeGuid);

    return Match;
}

BOOL
PartpGptAreGuidsEqual (
    UCHAR FirstGuid[GPT_GUID_SIZE],
    UCHAR SecondGuid[GPT_GUID_SIZE]
    )

/*++

Routine Description:

    This routine compares two GPT GUIDs.

Arguments:

    FirstGuid - Supplies the first GUID to compare.

    SecondGuid - Supplies the second GUID to compare.

Return Value:

    TRUE if the GUIDs are equal.

    FALSE if the GUIDs are not equal.

--*/

{

    ULONG Index;

    for (Index = 0; Index < GPT_GUID_SIZE; Index += 1) {
        if (FirstGuid[Index] != SecondGuid[Index]) {
            return FALSE;
        }
    }

    return TRUE;
}

