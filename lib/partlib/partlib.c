/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    partlib.c

Abstract:

    This module implements the partition support library.

Author:

    Evan Green 30-Jan-2014

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
PartpWriteMbrPartitionLayout (
    PPARTITION_CONTEXT Context,
    PPARTITION_INFORMATION Partitions,
    ULONG PartitionCount,
    BOOL CleanMbr
    );

KSTATUS
PartpParseMbrPartitionEntry (
    PPARTITION_CONTEXT Context,
    PPARTITION_TABLE_ENTRY TableEntry,
    BOOL Primary,
    ULONG Parent,
    ULONGLONG ExtendedEnd,
    ULONGLONG ExtendedRecordStart,
    PPARTITION_INFORMATION Information
    );

VOID
PartpConvertToMbrPartitionEntry (
    PPARTITION_CONTEXT Context,
    PPARTITION_INFORMATION Partition,
    ULONG StartOffset,
    ULONG Length,
    PPARTITION_TABLE_ENTRY TableEntry
    );

PPARTITION_INFORMATION
PartpReallocateArray (
    PPARTITION_CONTEXT Context,
    PPARTITION_INFORMATION Information,
    PULONG Capacity
    );

PARTITION_TYPE
PartpConvertSystemIdToPartitionType (
    UCHAR SystemId
    );

UCHAR
PartpConvertPartitionTypeToSystemId (
    PARTITION_TYPE Type
    );

VOID
PartpConvertLbaToChs (
    PPARTITION_CONTEXT Context,
    ULONG Lba,
    PULONG Cylinder,
    PULONG Head,
    PULONG Sector
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the mapping between partition system ID bytes and the partition type
// enum.
//

PARTITION_SYSTEM_ID_MAPPING PartSystemIdToPartitionTypeTable[] = {
    {PARTITION_ID_EMPTY, PartitionTypeEmpty},
    {PARTITION_ID_MINOCA, PartitionTypeMinoca},
    {PARTITION_ID_DOS_FAT12, PartitionTypeDosFat12},
    {PARTITION_ID_DOS_PRIMARY_FAT16, PartitionTypeDosPrimaryFat16},
    {PARTITION_ID_DOS_EXTENDED, PartitionTypeDosExtended},
    {PARTITION_ID_NTFS, PartitionTypeNtfs},
    {PARTITION_ID_WINDOWS95_FAT32, PartitionTypeWindows95Fat32},
    {PARTITION_ID_WINDOWS95_FAT32_LBA, PartitionTypeWindows95Fat32Lba},
    {PARTITION_ID_DOS_EXTENDED_FAT16, PartitionTypeDosExtendedFat16},
    {PARTITION_ID_DOS_EXTENDED_LBA, PartitionTypeDosExtendedLba},
    {PARTITION_ID_WINDOWS_RE, PartitionTypeWindowsRecovery},
    {PARTITION_ID_PLAN9, PartitionTypePlan9},
    {PARTITION_ID_SYSTEMV_MACH_HURD, PartitionTypeSystemVMachHurd},
    {PARTITION_ID_MINIX_13, PartitionTypeMinix13},
    {PARTITION_ID_MINIX_14, PartitionTypeMinix14},
    {PARTITION_ID_LINUX_SWAP, PartitionTypeLinuxSwap},
    {PARTITION_ID_LINUX, PartitionTypeLinux},
    {PARTITION_ID_LINUX_EXTENDED, PartitionTypeLinuxExtended},
    {PARTITION_ID_LINUX_LVM, PartitionTypeLinuxLvm},
    {PARTITION_ID_BSD, PartitionTypeBsd},
    {PARTITION_ID_FREEBSD, PartitionTypeFreeBsd},
    {PARTITION_ID_OPENBSD, PartitionTypeOpenBsd},
    {PARTITION_ID_NEXTSTEP, PartitionTypeNextStep},
    {PARTITION_ID_MAC_OS_X, PartitionTypeMacOsX},
    {PARTITION_ID_NETBSD, PartitionTypeNetBsd},
    {PARTITION_ID_MAC_OS_X_BOOT, PartitionTypeMaxOsXBoot},
    {PARTITION_ID_MAX_OS_X_HFS, PartitionTypeMaxOsXHfs},
    {PARTITION_ID_EFI_GPT, PartitionTypeEfiGpt},
    {PARTITION_ID_EFI_SYSTEM, PartitionTypeEfiSystem},
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
PartInitialize (
    PPARTITION_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a partition context. The caller is expected to
    have filled in pointers to the allocate, free, and read sector functions.
    The caller is also expected to have filled in the block size, disk
    geometry information, and alignment (if needed).

Arguments:

    Context - Supplies a pointer to the context, partially initialized by the
        caller.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the context was not initialized properly by
    the caller.

--*/

{

    if (Context->Alignment == 1) {
        Context->Alignment = 0;
    }

    if ((Context->AllocateFunction == NULL) ||
        (Context->FreeFunction == NULL) ||
        (Context->ReadFunction == NULL) ||
        (Context->BlockSize < MINIMUM_BLOCK_SIZE) ||
        (!POWER_OF_2(Context->BlockSize)) ||
        ((Context->Alignment != 0) && (!POWER_OF_2(Context->Alignment)))) {

        return STATUS_INVALID_PARAMETER;
    }

    Context->BlockShift = RtlCountTrailingZeros32(Context->BlockSize);
    RtlZeroMemory(&(Context->DiskIdentifier), sizeof(Context->DiskIdentifier));
    Context->PartitionCount = 0;
    Context->Partitions = NULL;
    return STATUS_SUCCESS;
}

VOID
PartDestroy (
    PPARTITION_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys a partition context.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

{

    if (Context->Partitions != NULL) {

        ASSERT(Context->FreeFunction != NULL);

        Context->FreeFunction(Context->Partitions);
        Context->Partitions = NULL;
    }

    Context->PartitionCount = 0;
    return;
}

KSTATUS
PartEnumeratePartitions (
    PPARTITION_CONTEXT Context
    )

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

{

    PUCHAR Block;
    PVOID BlockAllocation;
    ULONG Capacity;
    PPARTITION_TABLE_ENTRY Entry;
    ULONG EntryIndex;
    ULONGLONG ExtendedEnd;
    ULONGLONG ExtendedRecordOffset;
    ULONGLONG ExtendedStart;
    PPARTITION_INFORMATION Information;
    PPARTITION_INFORMATION NewInformation;
    ULONG NextExtendedRecord;
    PUSHORT Pointer16;
    ULONG PrimaryCount;
    KSTATUS Status;

    ASSERT((Context->BlockSize != 0) &&
           ((1 << Context->BlockShift) == Context->BlockSize) &&
           (Context->AllocateFunction != NULL) &&
           (Context->FreeFunction != NULL) &&
           (Context->ReadFunction != NULL) &&
           (Context->PartitionCount == 0) &&
           (Context->Partitions == NULL));

    Capacity = 0;
    Information = NULL;

    //
    // Allocate a block for reading.
    //

    BlockAllocation = PartpAllocateIo(Context,
                                      Context->BlockSize,
                                      (PVOID *)&Block);

    if (BlockAllocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumeratePartitionsEnd;
    }

    //
    // Read the first block.
    //

    Status = Context->ReadFunction(Context, 0, Block);
    if (!KSUCCESS(Status)) {
        goto EnumeratePartitionsEnd;
    }

    Context->Format = PartitionFormatNone;

    //
    // Check the MBR signature.
    //

    Pointer16 = (PUSHORT)(Block + PARTITION_SIGNATURE_OFFSET);
    if (*Pointer16 != PARTITION_SIGNATURE) {
        Status = STATUS_NO_ELIGIBLE_DEVICES;
        goto EnumeratePartitionsEnd;
    }

    //
    // Parse differently if this is a GPT disk.
    //

    Entry = (PPARTITION_TABLE_ENTRY)(Block + PARTITION_TABLE_OFFSET);
    if (PartpGptIsProtectiveMbr(Entry) != FALSE) {
        Status = PartpGptEnumeratePartitions(Context);
        if (!KSUCCESS(Status)) {
            goto EnumeratePartitionsEnd;
        }

        Context->Format = PartitionFormatGpt;
        Information = Context->Partitions;
        goto EnumeratePartitionsEnd;
    }

    //
    // This is an MBR disk. Save the disk ID.
    //

    RtlCopyMemory(&(Context->DiskIdentifier),
                  Block + MBR_DISK_ID_OFFSET,
                  MBR_DISK_ID_SIZE);

    //
    // Loop over each entry and create the partition information.
    //

    for (EntryIndex = 0;
         EntryIndex < PARTITION_TABLE_SIZE;
         EntryIndex += 1) {

        //
        // Expand the array if needed.
        //

        if (Context->PartitionCount == Capacity) {
            NewInformation = PartpReallocateArray(Context,
                                                  Information,
                                                  &Capacity);

            if (NewInformation == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto EnumeratePartitionsEnd;
            }

            Information = NewInformation;
        }

        ASSERT(Context->PartitionCount < Capacity);

        Status = PartpParseMbrPartitionEntry(
                                      Context,
                                      &(Entry[EntryIndex]),
                                      TRUE,
                                      0,
                                      0,
                                      0,
                                      &(Information[Context->PartitionCount]));

        if (!KSUCCESS(Status)) {
            Status = STATUS_NO_ELIGIBLE_DEVICES;
            goto EnumeratePartitionsEnd;
        }
    }

    //
    // Now go through each of the primary partitions and parse any logical
    // partitions out of any extended partitions.
    //

    PrimaryCount = Context->PartitionCount;
    for (EntryIndex = 0; EntryIndex < PrimaryCount; EntryIndex += 1) {
        if ((Information[EntryIndex].Flags & PARTITION_FLAG_EXTENDED) == 0) {
            continue;
        }

        ExtendedStart = Information[EntryIndex].StartOffset;
        ExtendedEnd = Information[EntryIndex].EndOffset;
        ExtendedRecordOffset = ExtendedStart;

        //
        // Loop over the singly linked list of logical partitions within the
        // extended partition.
        //

        while (TRUE) {

            //
            // Read the extended boot record.
            //

            Status = Context->ReadFunction(Context,
                                           ExtendedRecordOffset,
                                           Block);

            if (!KSUCCESS(Status)) {
                goto EnumeratePartitionsEnd;
            }

            //
            // Check the signature.
            //

            Pointer16 = (PUSHORT)(Block + PARTITION_SIGNATURE_OFFSET);
            if (*Pointer16 != PARTITION_SIGNATURE) {
                continue;
            }

            //
            // Expand the array if needed.
            //

            if (Context->PartitionCount == Capacity) {
                NewInformation = PartpReallocateArray(Context,
                                                      Information,
                                                      &Capacity);

                if (NewInformation == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto EnumeratePartitionsEnd;
                }

                Information = NewInformation;
            }

            ASSERT(Context->PartitionCount < Capacity);

            //
            // The first entry has information about the logical partition, the
            // second entry has a pointer to the next EBR. Parse the first
            // entry.
            //

            Entry = (PPARTITION_TABLE_ENTRY)(Block + PARTITION_TABLE_OFFSET);
            PartpParseMbrPartitionEntry(
                                      Context,
                                      &(Entry[0]),
                                      FALSE,
                                      Information[EntryIndex].Number,
                                      ExtendedEnd,
                                      ExtendedRecordOffset,
                                      &(Information[Context->PartitionCount]));

            //
            // The offset for the second entry is relative to the start of the
            // extended partition as a whole (which is different than the first
            // entry, whose offset was relative to this EBR's offset). If it's
            // zero, take that to mean the end.
            //

            NextExtendedRecord = Entry[1].StartingLba;
            if (NextExtendedRecord == 0) {
                break;
            }

            //
            // Also quietly stop if the next record tries to go off the end of
            // the extended partition.
            //

            if (NextExtendedRecord + ExtendedStart >= ExtendedEnd) {
                break;
            }

            ExtendedRecordOffset = NextExtendedRecord + ExtendedStart;
        }
    }

    //
    // Trim off any empty partitions on the end.
    //

    while ((Context->PartitionCount != 0) &&
           (Information[Context->PartitionCount - 1].PartitionType ==
            PartitionTypeEmpty)) {

        Context->PartitionCount -= 1;
    }

    Context->Format = PartitionFormatMbr;
    Status = STATUS_SUCCESS;

EnumeratePartitionsEnd:
    if (!KSUCCESS(Status)) {
        if (Information != NULL) {
            Context->FreeFunction(Information);
            Information = NULL;
        }

        Context->PartitionCount = 0;
    }

    Context->Partitions = Information;
    if (BlockAllocation != NULL) {
        Context->FreeFunction(BlockAllocation);
    }

    return Status;
}

KSTATUS
PartWritePartitionLayout (
    PPARTITION_CONTEXT Context,
    PARTITION_FORMAT Format,
    PPARTITION_INFORMATION Partitions,
    ULONG PartitionCount,
    BOOL CleanMbr
    )

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

{

    KSTATUS Status;

    if (Format == PartitionFormatMbr) {
        Status = PartpWriteMbrPartitionLayout(Context,
                                              Partitions,
                                              PartitionCount,
                                              CleanMbr);

    } else if (Format == PartitionFormatGpt) {
        Status = PartpGptWritePartitionLayout(Context,
                                              Partitions,
                                              PartitionCount,
                                              CleanMbr);

    } else {

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

KSTATUS
PartTranslateIo (
    PPARTITION_INFORMATION Partition,
    PULONGLONG BlockAddress,
    PULONGLONG BlockCount
    )

/*++

Routine Description:

    This routine performs a translation from a partition-relative offset to a
    global disk offset.

Arguments:

    Partition - Supplies a pointer to the partition to translate for.

    BlockAddress - Supplies a pointer that on input contains the
        partition-relative block address. On successful output, this will
        contain the global address.

    BlockCount - Supplies a pointer that on input contains the number of blocks
        to read or write. On output, the number of valid blocks will be
        returned. This number may be reduced on output if the caller tried to
        do I/O off the end of the partition.

Return Value:

    STATUS_SUCCESS if the valid block count is non-zero.

    STATUS_OUT_OF_BOUNDS if the block address is beyond the end of the
    partition.

--*/

{

    ULONGLONG Length;

    Length = Partition->EndOffset - Partition->StartOffset;
    if (*BlockAddress >= Length) {
        return STATUS_OUT_OF_BOUNDS;
    }

    if (BlockCount != NULL) {
        if (*BlockAddress + *BlockCount < *BlockAddress) {
            return STATUS_OUT_OF_BOUNDS;
        }

        if (*BlockAddress + *BlockCount > Length) {
            *BlockCount = Length - *BlockAddress;
        }
    }

    *BlockAddress += Partition->StartOffset;
    return STATUS_SUCCESS;
}

PARTITION_TYPE
PartConvertToPartitionType (
    PARTITION_FORMAT Format,
    UCHAR PartitionTypeId[PARTITION_TYPE_SIZE]
    )

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

{

    PARTITION_TYPE PartitionType;

    if (Format == PartitionFormatMbr) {
        PartitionType = PartpConvertSystemIdToPartitionType(PartitionTypeId[0]);

    } else if (Format == PartitionFormatGpt) {
        PartitionType = PartpGptConvertTypeGuidToPartitionType(PartitionTypeId);

    } else {
        PartitionType = PartitionTypeInvalid;
    }

    return PartitionType;
}

PVOID
PartpAllocateIo (
    PPARTITION_CONTEXT Context,
    UINTN Size,
    PVOID *AlignedAllocation
    )

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

{

    PVOID Allocation;

    Allocation = Context->AllocateFunction(Size + Context->Alignment);
    if (Allocation == NULL) {
        return NULL;
    }

    if (Context->Alignment == 0) {
        *AlignedAllocation = Allocation;

    } else {
        *AlignedAllocation = (PVOID)(UINTN)ALIGN_RANGE_UP((UINTN)Allocation,
                                                          Context->Alignment);
    }

    return Allocation;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PartpWriteMbrPartitionLayout (
    PPARTITION_CONTEXT Context,
    PPARTITION_INFORMATION Partitions,
    ULONG PartitionCount,
    BOOL CleanMbr
    )

/*++

Routine Description:

    This routine writes an MBR partition layout to the disk. This usually wipes
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

    PVOID Block;
    PVOID BlockAllocation;
    ULONG BlockIndex;
    ULONG FirstPartition;
    PARTITION_TABLE_ENTRY MbrEntries[PARTITION_TABLE_SIZE];
    ULONG MbrEntryCount;
    PPARTITION_INFORMATION Partition;
    ULONG PartitionIndex;
    KSTATUS Status;

    BlockAllocation = NULL;
    RtlZeroMemory(MbrEntries, sizeof(MbrEntries));

    //
    // Loop over the partitions to fill in the primary MBR entries.
    //

    FirstPartition = 0;
    MbrEntryCount = 0;
    for (PartitionIndex = 0;
         PartitionIndex < PartitionCount;
         PartitionIndex += 1) {

        Partition = &(Partitions[PartitionIndex]);
        if (PartitionIndex == 0) {
            FirstPartition = Partition->StartOffset;
        }

        ASSERT(Partition->EndOffset >= Partition->StartOffset);

        //
        // Find a slot in the MBR if this is a primary or extended partition.
        //

        if ((Partition->Flags &
            (PARTITION_FLAG_PRIMARY | PARTITION_FLAG_EXTENDED)) != 0) {

            if (MbrEntryCount == PARTITION_TABLE_SIZE) {
                Status = STATUS_INVALID_CONFIGURATION;
                goto WriteMbrPartitionLayoutEnd;
            }

            ASSERT((Partition->Number == 0) ||
                   (Partition->Number == MbrEntryCount + 1));

            ASSERT(((Partition->Flags & PARTITION_FLAG_EXTENDED) == 0) ||
                   (Partition->TypeIdentifier[0] ==
                    PARTITION_ID_DOS_EXTENDED) ||
                   (Partition->TypeIdentifier[0] ==
                    PARTITION_ID_DOS_EXTENDED_LBA));

            PartpConvertToMbrPartitionEntry(
                                 Context,
                                 Partition,
                                 Partition->StartOffset,
                                 Partition->EndOffset - Partition->StartOffset,
                                 &(MbrEntries[MbrEntryCount]));

            MbrEntryCount += 1;
        }

        //
        // Logical partitions are currently not supported.
        //

        if ((Partition->Flags & PARTITION_FLAG_LOGICAL) != 0) {
            Status = STATUS_NOT_SUPPORTED;
            goto WriteMbrPartitionLayoutEnd;
        }
    }

    //
    // Allocate space for the MBR block to be read in.
    //

    if (Context->BlockSize < MINIMUM_BLOCK_SIZE) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto WriteMbrPartitionLayoutEnd;
    }

    BlockAllocation = PartpAllocateIo(Context, Context->BlockSize, &Block);
    if (BlockAllocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto WriteMbrPartitionLayoutEnd;
    }

    //
    // Read in the MBR, or zero out the buffer.
    //

    if (CleanMbr != FALSE) {
        RtlZeroMemory(Block, Context->BlockSize);

    } else {
        Status = Context->ReadFunction(Context, 0, Block);
        if (!KSUCCESS(Status)) {
            goto WriteMbrPartitionLayoutEnd;
        }
    }

    //
    // Copy the new partition tables over.
    //

    RtlCopyMemory(Block + PARTITION_TABLE_OFFSET,
                  MbrEntries,
                  sizeof(MbrEntries));

    //
    // If there's a random function, create a random disk ID.
    //

    if (Context->FillRandomFunction != NULL) {
        Context->FillRandomFunction(Context,
                                    Block + MBR_DISK_ID_OFFSET,
                                    MBR_DISK_ID_SIZE);
    }

    ASSERT(Context->WriteFunction != NULL);

    //
    // Apply the signature.
    //

    *((PUSHORT)((PUCHAR)Block + PARTITION_SIGNATURE_OFFSET)) =
                                                           PARTITION_SIGNATURE;

    //
    // Write the MBR back out.
    //

    Status = Context->WriteFunction(Context, 0, Block);
    if (!KSUCCESS(Status)) {
        goto WriteMbrPartitionLayoutEnd;
    }

    //
    // Zero out the space between the MBR and the first partition.
    //

    RtlZeroMemory(Block, Context->BlockSize);
    BlockIndex = 1;
    while (BlockIndex < FirstPartition) {
        Status = Context->WriteFunction(Context, BlockIndex, Block);
        if (!KSUCCESS(Status)) {
            goto WriteMbrPartitionLayoutEnd;
        }

        BlockIndex += 1;
    }

WriteMbrPartitionLayoutEnd:
    if (BlockAllocation != NULL) {
        Context->FreeFunction(BlockAllocation);
    }

    return Status;
}

KSTATUS
PartpParseMbrPartitionEntry (
    PPARTITION_CONTEXT Context,
    PPARTITION_TABLE_ENTRY TableEntry,
    BOOL Primary,
    ULONG Parent,
    ULONGLONG ExtendedEnd,
    ULONGLONG ExtendedRecordStart,
    PPARTITION_INFORMATION Information
    )

/*++

Routine Description:

    This routine parses an MBR-style partition table entry and converts it to
    a partition information structure.

Arguments:

    Context - Supplies a pointer to the partition context.

    TableEntry - Supplies a pointer to the MBR-style partition table entry.

    Primary - Supplies a boolean indicating if this is a primary partition
        (TRUE) or a logical partition (FALSE).

    Parent - Supplies the parent partition number (used only for logical
        partitions).

    ExtendedEnd - Supplies the end block address (exclusive) for the extended
        partition this entry resides in. This is the ending sector for the
        entire extended partition, not just this EBR. This value is ignored for
        primary partitions.

    ExtendedRecordStart - Supplies the block address that this extended boot
        record resides in. This is the offset of this EBR. This is ignored for
        primary partitions.

    Information - Supplies a pointer where the partition information will be
        returned on success.

Return Value:

    STATUS_SUCCESS if a partition was parsed out.

    STATUS_INVALID_CONFIGURATION if the partition table entry is not valid.

--*/

{

    if ((TableEntry->BootIndicator != 0) &&
        (TableEntry->BootIndicator != MBR_PARTITION_BOOT)) {

        return STATUS_INVALID_CONFIGURATION;
    }

    if (Primary != FALSE) {
        ExtendedEnd = 0;
        ExtendedRecordStart = 0;
    }

    //
    // Fail if the logical partition goes outside of its parent extended
    // partition.
    //

    if ((Primary == FALSE) &&
        (TableEntry->StartingLba + ExtendedRecordStart > ExtendedEnd)) {

        return STATUS_BUFFER_OVERRUN;
    }

    //
    // The starting offset for the first entry in the extended boot record
    // is the relative offset from this EBR. For primary partitions, this value
    // is 0. The second entry is a link, and isn't handled by this routine.
    //

    Information->StartOffset = TableEntry->StartingLba + ExtendedRecordStart;
    Information->EndOffset = Information->StartOffset + TableEntry->SectorCount;
    Information->Number = Context->PartitionCount + 1;
    Information->ParentNumber = Parent;
    Context->PartitionCount += 1;
    Information->Flags = 0;
    if (TableEntry->BootIndicator == MBR_PARTITION_BOOT) {
        Information->Flags |= PARTITION_FLAG_BOOT;
    }

    Information->TypeIdentifier[0] = TableEntry->SystemId;
    Information->PartitionType =
                     PartpConvertSystemIdToPartitionType(TableEntry->SystemId);

    if (Primary != FALSE) {
        if ((Information->PartitionType == PartitionTypeDosExtended) ||
            (Information->PartitionType == PartitionTypeDosExtendedLba)) {

            Information->Flags |= PARTITION_FLAG_EXTENDED;

        } else {
            Information->Flags |= PARTITION_FLAG_PRIMARY;
        }

    } else {
        Information->Flags |= PARTITION_FLAG_LOGICAL;
    }

    //
    // Create a partition signature by cobbling together the partition number
    // and the disk ID.
    //

    RtlCopyMemory(&(Information->Identifier[0]),
                  &(Context->DiskIdentifier),
                  MBR_DISK_ID_SIZE);

    RtlCopyMemory(&(Information->Identifier[MBR_DISK_ID_SIZE]),
                  &(Information->Number),
                  sizeof(Information->Number));

    return STATUS_SUCCESS;
}

VOID
PartpConvertToMbrPartitionEntry (
    PPARTITION_CONTEXT Context,
    PPARTITION_INFORMATION Partition,
    ULONG StartOffset,
    ULONG Length,
    PPARTITION_TABLE_ENTRY TableEntry
    )

/*++

Routine Description:

    This routine initializes an MBR-style partition entry from a partition
    information structure.

Arguments:

    Context - Supplies a pointer to the partition context.

    Partition - Supplies a pointer to the partition information.

    StartOffset - Supplies the start offset to set in the partition table.

    Length - Supplies the length to use in the partition table.

    TableEntry - Supplies a pointer where table entry is returned.

Return Value:

    None.

--*/

{

    ULONG Cylinder;
    ULONG Head;
    ULONG Sector;

    RtlZeroMemory(TableEntry, sizeof(PARTITION_TABLE_ENTRY));
    if (Length == 0) {
        return;
    }

    if ((Partition->Flags & PARTITION_FLAG_BOOT) != 0) {
        TableEntry->BootIndicator = MBR_PARTITION_BOOT;
    }

    PartpConvertLbaToChs(Context, StartOffset, &Cylinder, &Head, &Sector);
    TableEntry->StartingHead = Head;
    TableEntry->StartingSector = Sector | ((Cylinder >> 2) & 0xC0);
    TableEntry->StartingCylinder = Cylinder & 0xFF;
    PartpConvertLbaToChs(Context,
                         StartOffset + Length - 1,
                         &Cylinder,
                         &Head,
                         &Sector);

    TableEntry->EndingHead = Head;
    TableEntry->EndingSector = Sector | ((Cylinder >> 2) & 0xC0);
    TableEntry->EndingCylinder = Cylinder & 0xFF;
    if (Partition->PartitionType != PartitionTypeInvalid) {
        TableEntry->SystemId =
                 PartpConvertPartitionTypeToSystemId(Partition->PartitionType);

    } else {
        TableEntry->SystemId = Partition->TypeIdentifier[0];
    }

    TableEntry->StartingLba = StartOffset;
    TableEntry->SectorCount = Length;
    return;
}

PPARTITION_INFORMATION
PartpReallocateArray (
    PPARTITION_CONTEXT Context,
    PPARTITION_INFORMATION Information,
    PULONG Capacity
    )

/*++

Routine Description:

    This routine allocates or reallocates the partition information array.

Arguments:

    Context - Supplies a pointer to the partition context.

    Information - Supplies an optional pointer to the current array.

    Capacity - Supplies a pointer that on input contains the capacity of the
        current array. On output, this will be updated to the new capacity.

Return Value:

    Returns a pointer to the new array on success.

    NULL on allocation failure, and the old array will not be freed.

--*/

{

    PPARTITION_INFORMATION NewBuffer;
    ULONG NewCapacity;

    if (*Capacity == 0) {
        NewCapacity = INITIAL_PARTITION_INFORMATION_CAPACITY;

    } else {
        NewCapacity = *Capacity * 2;
    }

    if (NewCapacity <= *Capacity) {
        return NULL;
    }

    NewBuffer = Context->AllocateFunction(
                                  NewCapacity * sizeof(PARTITION_INFORMATION));

    if (NewBuffer == NULL) {
        return NULL;
    }

    //
    // Copy the old buffer over.
    //

    if (*Capacity != 0) {
        RtlCopyMemory(NewBuffer,
                      Information,
                      *Capacity * sizeof(PARTITION_INFORMATION));
    }

    //
    // Zero out the new stuff.
    //

    RtlZeroMemory(NewBuffer + *Capacity,
                  (NewCapacity - *Capacity) * sizeof(PARTITION_INFORMATION));

    //
    // Free the old buffer and return the new.
    //

    if (Information != NULL) {
        Context->FreeFunction(Information);
    }

    *Capacity = NewCapacity;
    return NewBuffer;
}

PARTITION_TYPE
PartpConvertSystemIdToPartitionType (
    UCHAR SystemId
    )

/*++

Routine Description:

    This routine converts a system ID byte into a partition type to the best of
    its abilities.

Arguments:

    SystemId - Supplies the system ID byte.

Return Value:

    Returns a partition type for this system ID byte.

--*/

{

    ULONG EntryCount;
    ULONG EntryIndex;

    EntryCount = sizeof(PartSystemIdToPartitionTypeTable) /
                 sizeof(PartSystemIdToPartitionTypeTable[0]);

    for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
        if (PartSystemIdToPartitionTypeTable[EntryIndex].SystemId == SystemId) {
            return PartSystemIdToPartitionTypeTable[EntryIndex].PartitionType;
        }
    }

    return PartitionTypeUnknown;
}

UCHAR
PartpConvertPartitionTypeToSystemId (
    PARTITION_TYPE Type
    )

/*++

Routine Description:

    This routine converts a partition type value into a system ID byte.

Arguments:

    Type - Supplies the partition type.

Return Value:

    Returns a partition type for this system ID byte.

--*/

{

    ULONG EntryCount;
    ULONG EntryIndex;

    EntryCount = sizeof(PartSystemIdToPartitionTypeTable) /
                 sizeof(PartSystemIdToPartitionTypeTable[0]);

    for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
        if (PartSystemIdToPartitionTypeTable[EntryIndex].PartitionType ==
            Type) {

            return PartSystemIdToPartitionTypeTable[EntryIndex].SystemId;
        }
    }

    ASSERT(FALSE);

    return PARTITION_ID_DOS_FAT12;
}

VOID
PartpConvertLbaToChs (
    PPARTITION_CONTEXT Context,
    ULONG Lba,
    PULONG Cylinder,
    PULONG Head,
    PULONG Sector
    )

/*++

Routine Description:

    This routine converts a LBA address (linear offset) into a Cylinder-Head-
    Sector geometry address. If the LBA address is too high, then the maximum
    CHS values will be set.

Arguments:

    Context - Supplies a pointer to the initialized partition context.

    Lba - Supplies the LBA to convert.

    Cylinder - Supplies a pointer where the cylinder will be returned.

    Head - Supplies a pointer where the head will be returned.

    Sector - Supplies a pointer where the sector will be returned.

Return Value:

    None.

--*/

{

    ULONG TotalHead;

    if ((Context->SectorsPerHead == 0) || (Context->HeadsPerCylinder == 0)) {
        *Cylinder = 0xFF;
        *Head = 0xFE;
        *Sector = 0xFF;
        return;
    }

    TotalHead = Lba / Context->SectorsPerHead;
    *Sector = (Lba % Context->SectorsPerHead) + 1;
    *Cylinder = TotalHead / Context->HeadsPerCylinder;
    *Head = TotalHead % Context->HeadsPerCylinder;
    if (*Cylinder > MBR_MAX_CYLINDER) {
        *Cylinder = MBR_MAX_CYLINDER;
        *Head = Context->HeadsPerCylinder - 1;
        *Sector = Context->SectorsPerHead;
    }

    return;
}

