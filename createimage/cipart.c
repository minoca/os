/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    cipart.c

Abstract:

    This module implements partition support for the createimage app.

Author:

    Evan Green 2-Feb-2014

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <minoca/kernel.h>
#include <minoca/partlib.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "createimage.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
CipPartitionAllocate (
    UINTN Size
    );

VOID
CipPartitionFree (
    PVOID Memory
    );

KSTATUS
CipValidatePartitionLayout (
    PCREATEIMAGE_CONTEXT Context
    );

KSTATUS
CipPartitionRead (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    );

KSTATUS
CipPartitionWrite (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    );

VOID
CipPartitionFillRandom (
    PPARTITION_CONTEXT Context,
    PUCHAR Buffer,
    ULONG BufferSize
    );

VOID
CipGetHumanSize (
    ULONGLONG Bytes,
    PULONG Size,
    PUCHAR Suffix
    );

KSTATUS
CipConvertStringToGuid (
    PSTR GuidString,
    PSTR *StringAfterScan,
    UCHAR *GuidBuffer
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
CiInitializePartitionSupport (
    PCREATEIMAGE_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the partition context.

Arguments:

    Context - Supplies a pointer to the createimage context.

Return Value:

    Status code.

--*/

{

    PPARTITION_CONTEXT PartitionContext;
    KSTATUS Status;

    PartitionContext = &(Context->PartitionContext);
    PartitionContext->AllocateFunction = CipPartitionAllocate;
    PartitionContext->FreeFunction = CipPartitionFree;
    PartitionContext->ReadFunction = CipPartitionRead;
    PartitionContext->WriteFunction = CipPartitionWrite;
    PartitionContext->FillRandomFunction = CipPartitionFillRandom;
    PartitionContext->BlockSize = CREATEIMAGE_SECTOR_SIZE;
    Status = PartInitialize(PartitionContext);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    return Status;
}

VOID
CiDestroyPartitionSupport (
    PCREATEIMAGE_CONTEXT Context
    )

/*++

Routine Description:

    This routine rears down the partition support in createimage.

Arguments:

    Context - Supplies a pointer to the createimage context.

Return Value:

    None.

--*/

{

    PartDestroy(&(Context->PartitionContext));
    if (Context->CreatePartitions != NULL) {
        free(Context->CreatePartitions);
    }

    return;
}

KSTATUS
CiParsePartitionLayout (
    PCREATEIMAGE_CONTEXT Context,
    PSTR Argument
    )

/*++

Routine Description:

    This routine initializes parses the partition layout specified on the
    command line.

Arguments:

    Context - Supplies a pointer to the createimage context.

    Argument - Supplies the partition layout argument.

Return Value:

    Status code.

--*/

{

    PSTR AfterScan;
    ULONG AllocationSize;
    ULONGLONG Attributes;
    ULONG BlockSize;
    CHAR Character;
    INT CharacterIndex;
    CHAR Flavor;
    ULONGLONG Length;
    PVOID NewBuffer;
    PARTITION_INFORMATION Partition;
    KSTATUS Status;
    BOOL TypeSet;

    BlockSize = Context->PartitionContext.BlockSize;

    assert(BlockSize != 0);

    CharacterIndex = 0;
    while (TRUE) {
        memset(&Partition, 0, sizeof(Partition));
        Partition.StartOffset = -1;
        Partition.EndOffset = -1;
        Character = Argument[CharacterIndex];
        if (Character == '\0') {
            break;
        }

        //
        // Parse the partition type.
        //

        Partition.TypeIdentifier[0] = PARTITION_ID_MINOCA;
        Flavor = Character;
        switch (Flavor) {
        case 'p':
            Partition.Flags |= PARTITION_FLAG_PRIMARY;
            break;

        case 'e':
            Partition.Flags |= PARTITION_FLAG_EXTENDED;
            Partition.TypeIdentifier[0] = PARTITION_ID_DOS_EXTENDED_LBA;
            break;

        case 'l':
            Partition.Flags |= PARTITION_FLAG_LOGICAL;
            break;

        case 'b':
            Partition.PartitionType = PartitionTypeEmpty;
            Partition.TypeIdentifier[0] = PARTITION_ID_EMPTY;
            break;

        default:
            printf("createimage: Expected partition type (p for primary, "
                   "e for extended, l for logical) at character %d. Got %c.\n",
                   CharacterIndex + 1,
                   Character);

            Status = STATUS_INVALID_PARAMETER;
            goto ParsePartitionLayoutEnd;
        }

        CharacterIndex += 1;

        //
        // Scan the optional start offset.
        //

        if (isdigit(Argument[CharacterIndex])) {
            Partition.StartOffset = strtoull(Argument + CharacterIndex,
                                             &AfterScan,
                                             0);

            if (AfterScan == Argument + CharacterIndex) {
                printf("createimage: Unable to scan partition offset at "
                       "character %d.\n",
                       CharacterIndex + 1);

                Status = STATUS_INVALID_PARAMETER;
                goto ParsePartitionLayoutEnd;
            }

            CharacterIndex = (UINTN)AfterScan - (UINTN)Argument;
            switch (Argument[CharacterIndex]) {
            case 't':
            case 'T':
                Partition.StartOffset *= 1024ULL;

            case 'g':
            case 'G':
                Partition.StartOffset *= 1024ULL;

            case 'm':
            case 'M':
                Partition.StartOffset *= 1024ULL;

            case 'k':
            case 'K':
                Partition.StartOffset *= 1024ULL;
                CharacterIndex += 1;
                break;

            default:
                break;
            }
        }

        if (Argument[CharacterIndex] != ':') {
            printf("createimage: Expected : at character %d, got %c.\n",
                   CharacterIndex + 1,
                   Argument[CharacterIndex]);

            Status = STATUS_INVALID_PARAMETER;
            goto ParsePartitionLayoutEnd;
        }

        CharacterIndex += 1;

        //
        // Scan the length.
        //

        if (isdigit(Argument[CharacterIndex])) {
            Length = strtoull(Argument + CharacterIndex, &AfterScan, 0);
            if (AfterScan == Argument + CharacterIndex) {
                printf("createimage: Unable to scan partition offset at "
                       "character %d.\n",
                       CharacterIndex + 1);

                Status = STATUS_INVALID_PARAMETER;
                goto ParsePartitionLayoutEnd;
            }

            CharacterIndex = (UINTN)AfterScan - (UINTN)Argument;
            switch (Argument[CharacterIndex]) {
            case 't':
            case 'T':
                Length *= 1024ULL;

            case 'g':
            case 'G':
                Length *= 1024ULL;

            case 'm':
            case 'M':
                Length *= 1024ULL;

            case 'k':
            case 'K':
                Length *= 1024ULL;
                CharacterIndex += 1;
                break;

            default:
                break;
            }

            //
            // If the start offset is specified, then fully specify the end
            // offset too. Otherwise, store the length in the end offset, and
            // fix it up when the real offsets are known.
            //

            if (Partition.StartOffset != -1) {
                Partition.EndOffset = Partition.StartOffset + Length;

            } else {
                Partition.EndOffset = Length;
            }
        }

        //
        // Divide the byte offsets into blocks if they're set.
        //

        if (Partition.StartOffset != -1) {
            Partition.StartOffset /= BlockSize;
        }

        if (Partition.EndOffset != -1) {
            Partition.EndOffset /= BlockSize;
        }

        //
        // Parse an optional * for the active partition.
        //

        if (Argument[CharacterIndex] == '*') {
            CharacterIndex += 1;
            Partition.Flags |= PARTITION_FLAG_BOOT;
        }

        //
        // Parse the optional system ID/partition type identifier. A couple of
        // standard names are supported.
        //

        TypeSet = FALSE;
        if (Argument[CharacterIndex] == ':') {
            CharacterIndex += 1;
            if (Argument[CharacterIndex] == '{') {
                Status = CipConvertStringToGuid(Argument + CharacterIndex,
                                                &AfterScan,
                                                Partition.TypeIdentifier);

                if (!KSUCCESS(Status)) {
                    goto ParsePartitionLayoutEnd;
                }

                TypeSet = TRUE;
                CharacterIndex = (UINTN)AfterScan - (UINTN)Argument;

            } else if (isdigit(Argument[CharacterIndex])) {
                Partition.TypeIdentifier[0] = strtoul(Argument + CharacterIndex,
                                                      &AfterScan,
                                                      0);

                if (AfterScan == (Argument + CharacterIndex)) {
                    printf("createimage: Unable to scan system ID at character "
                           "%d.\n",
                           CharacterIndex + 1);

                    Status = STATUS_INVALID_PARAMETER;
                    goto ParsePartitionLayoutEnd;
                }

                TypeSet = TRUE;
                CharacterIndex = (UINTN)AfterScan - (UINTN)Argument;

            } else if ((Argument[CharacterIndex] != ':') &&
                       (Argument[CharacterIndex] != ',') &&
                       (Argument[CharacterIndex] != '\0')) {

                TypeSet = TRUE;
                switch (Argument[CharacterIndex]) {
                case 'e':
                    Partition.PartitionType = PartitionTypeEfiSystem;
                    break;

                case 'm':
                    Partition.PartitionType = PartitionTypeMinoca;
                    break;

                case 'd':
                    Partition.PartitionType = PartitionTypeDosFat12;
                    break;

                case 'f':
                    Partition.PartitionType = PartitionTypeDosPrimaryFat16;
                    break;

                case 'F':
                    Partition.PartitionType = PartitionTypeWindows95Fat32;
                    break;

                default:
                    printf("createimage: Unknown partition type ID %c.\n",
                           Argument[CharacterIndex]);

                    Status = STATUS_INVALID_PARAMETER;
                    goto ParsePartitionLayoutEnd;
                }

                CharacterIndex += 1;
            }
        }

        if (TypeSet == FALSE) {
            Partition.PartitionType = PartitionTypeMinoca;
        }

        //
        // Parse the optional attributes override.
        //

        Attributes = 0;
        if (Argument[CharacterIndex] == ':') {
            CharacterIndex += 1;
            if (isdigit(Argument[CharacterIndex])) {
                Attributes = strtoull(Argument + CharacterIndex, &AfterScan, 0);
                if (AfterScan == (Argument + CharacterIndex)) {
                    printf("createimage: Unable to scan partition attributes "
                           "at character '%c'.\n",
                           Argument[CharacterIndex + 1]);

                    Status = STATUS_INVALID_PARAMETER;
                    goto ParsePartitionLayoutEnd;
                }

                CharacterIndex = (UINTN)AfterScan - (UINTN)Argument;

            } else if ((Argument[CharacterIndex] != ',') &&
                       (Argument[CharacterIndex] != '\0')) {

                printf("createimage: Invalid attributes at character '%c'.\n",
                       Argument[CharacterIndex]);

                Status = STATUS_INVALID_PARAMETER;
                goto ParsePartitionLayoutEnd;
            }
        }

        Partition.Attributes = Attributes;

        //
        // The next character can either be a comma and another entry, or the
        // end.
        //

        if ((Argument[CharacterIndex] != '\0') &&
            (Argument[CharacterIndex] != ',')) {

            printf("createimage: Unexpected junk at end of argument: %s.\n",
                   Argument + CharacterIndex);

            Status = STATUS_INVALID_PARAMETER;
            goto ParsePartitionLayoutEnd;
        }

        if (Argument[CharacterIndex] == ',') {
            CharacterIndex += 1;
        }

        //
        // Add this partition to the layout.
        //

        AllocationSize = (Context->CreatePartitionCount + 1) *
                         sizeof(PARTITION_INFORMATION);

        NewBuffer = realloc(Context->CreatePartitions, AllocationSize);
        if (NewBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ParsePartitionLayoutEnd;
        }

        Context->CreatePartitions = NewBuffer;
        memcpy(&(Context->CreatePartitions[Context->CreatePartitionCount]),
               &Partition,
               sizeof(PARTITION_INFORMATION));

        Context->CreatePartitionCount += 1;

        //
        // Bump up the image size if needed.
        //

        if ((Partition.StartOffset != -1) &&
            (Partition.EndOffset != -1)) {

            if (Context->DiskSize < Partition.EndOffset) {
                Context->DiskSize = Partition.EndOffset;
            }
        }
    }

    Status = STATUS_SUCCESS;

ParsePartitionLayoutEnd:
    return Status;
}

KSTATUS
CiWritePartitionLayout (
    PCREATEIMAGE_CONTEXT Context,
    ULONGLONG MainPartitionSize
    )

/*++

Routine Description:

    This routine writes the partition layout to the output image. This erases
    everything on the disk.

Arguments:

    Context - Supplies a pointer to the createimage context.

    MainPartitionSize - Supplies the size of the main partition, which is
        used for any unsized partitions.

Return Value:

    Status code.

--*/

{

    ULONGLONG DiskEnd;
    ULONGLONG DiskFooterBlocks;
    PPARTITION_INFORMATION Partition;
    PARTITION_FORMAT PartitionFormat;
    ULONGLONG PartitionIndex;
    PPARTITION_INFORMATION PreviousPartition;
    KSTATUS Status;

    //
    // GPT disks need extra space at the end for the backup copy of the
    // partition table.
    //

    DiskFooterBlocks = 0;
    if ((Context->Options & CREATEIMAGE_OPTION_GPT) != 0) {
        DiskFooterBlocks = 40;
    }

    //
    // Loop through and assign space for any partitions whose offsets and sizes
    // are not fully pinned down.
    //

    DiskEnd = MainPartitionSize;
    PreviousPartition = NULL;
    for (PartitionIndex = 0;
         PartitionIndex < Context->CreatePartitionCount;
         PartitionIndex += 1) {

        Partition = &(Context->CreatePartitions[PartitionIndex]);

        //
        // If the partition's start offset is not set, then put it on the end
        // of the previous partition.
        //

        if (Partition->StartOffset == -1) {
            if (PreviousPartition == NULL) {
                if ((Context->Options & CREATEIMAGE_OPTION_GPT) != 0) {

                    //
                    // GPT formatted disks reserve the protective MBR, GPT
                    // header, and at least 16KB for partition table entries.
                    //

                    assert(Context->CreatePartitionCount < 128);

                    Partition->StartOffset = 40;

                } else {
                    Partition->StartOffset = 1;
                }

            } else {
                Partition->StartOffset = PreviousPartition->EndOffset;
            }

            if ((Context->Options & CREATEIMAGE_OPTION_ALIGN_PARTITIONS) != 0) {
                Partition->StartOffset =
                                ALIGN_RANGE_UP(Partition->StartOffset,
                                               _1MB / CREATEIMAGE_SECTOR_SIZE);
            }

            //
            // Take the end offset to be a length, and fix it up now.
            //

            if (Partition->EndOffset != -1) {
                Partition->EndOffset += Partition->StartOffset;
            }
        }

        //
        // If the end offset is not set, use the main partition size.
        //

        if (Partition->EndOffset == -1) {
            Partition->EndOffset = Partition->StartOffset + MainPartitionSize;
        }

        if (Partition->EndOffset + DiskFooterBlocks > DiskEnd) {
            DiskEnd = Partition->EndOffset + DiskFooterBlocks;
        }

        PreviousPartition = Partition;
    }

    //
    // Set the disk size if none was specified.
    //

    if (Context->DiskSize == 0) {
        Context->DiskSize = DiskEnd;
    }

    if (Context->PartitionContext.BlockCount == 0) {
        Context->PartitionContext.BlockCount = DiskEnd;
    }

    Status = CipValidatePartitionLayout(Context);
    if (!KSUCCESS(Status)) {
        goto WritePartitionLayoutEnd;
    }

    if (Context->CreatePartitionCount != 0) {
        PartitionFormat = PartitionFormatMbr;
        if ((Context->Options & CREATEIMAGE_OPTION_GPT) != 0) {
            PartitionFormat = PartitionFormatGpt;
        }

        Status = PartWritePartitionLayout(&(Context->PartitionContext),
                                          PartitionFormat,
                                          Context->CreatePartitions,
                                          Context->CreatePartitionCount,
                                          TRUE);

        if (!KSUCCESS(Status)) {
            goto WritePartitionLayoutEnd;
        }
    }

WritePartitionLayoutEnd:
    return Status;
}

KSTATUS
CiBindToPartitions (
    PCREATEIMAGE_CONTEXT Context,
    ULONGLONG DiskSize
    )

/*++

Routine Description:

    This routine binds to the partitions to install to.

Arguments:

    Context - Supplies a pointer to the createimage context.

    DiskSize - Supplies the size of the disk in blocks.

Return Value:

    Status code.

--*/

{

    ULONG FileIndex;
    ULONG Index;
    PPARTITION_INFORMATION Partition;
    PPARTITION_CONTEXT PartitionContext;
    PCREATEIMAGE_RAW_FILE RawFile;
    KSTATUS Status;

    if ((Context->InstallPartitionNumber == -1) &&
        (Context->BootPartitionNumber == -1) &&
        (Context->CreatePartitionCount == 0)) {

        Status = STATUS_SUCCESS;
        goto BindToPartitionsEnd;
    }

    PartitionContext = &(Context->PartitionContext);
    PartitionContext->BlockCount = DiskSize;
    Status = PartEnumeratePartitions(PartitionContext);
    if (!KSUCCESS(Status)) {
        printf("Error: Unable to enumerate partitions: %x\n", Status);
        goto BindToPartitionsEnd;
    }

    if ((PartitionContext->PartitionCount != 0) &&
        (Context->InstallPartitionNumber == -1)) {

        printf("Defaulting to partition 1.\n");
        Context->InstallPartitionNumber = 1;
    }

    //
    // Find the install partition.
    //

    Context->InstallPartition = NULL;
    for (Index = 0; Index < PartitionContext->PartitionCount; Index += 1) {
        if (PartitionContext->Partitions[Index].Number ==
            Context->InstallPartitionNumber) {

            Context->InstallPartition = &(PartitionContext->Partitions[Index]);
            break;
        }
    }

    if (Context->InstallPartition == NULL) {
        printf("Error: Install partition %d could not be found. %d partitions "
               "exist.\n",
               Context->InstallPartitionNumber,
               PartitionContext->PartitionCount);

        Status = STATUS_INVALID_PARAMETER;
        goto BindToPartitionsEnd;
    }

    //
    // Find the boot partition.
    //

    Context->BootPartition = NULL;
    if (Context->BootPartitionNumber == -1) {
        Context->BootPartitionNumber = Context->InstallPartitionNumber;
    }

    if (Context->BootPartitionNumber != -1) {
        for (Index = 0; Index < PartitionContext->PartitionCount; Index += 1) {
            if (PartitionContext->Partitions[Index].Number ==
                Context->BootPartitionNumber) {

                Context->BootPartition = &(PartitionContext->Partitions[Index]);
                break;
            }
        }

        if (Context->BootPartition == NULL) {
            printf("Error: Boot partition %d could not be found. %d partitions "
                   "exist.\n",
                   Context->BootPartitionNumber,
                   PartitionContext->PartitionCount);

            Status = STATUS_INVALID_PARAMETER;
            goto BindToPartitionsEnd;
        }
    }

    //
    // Update the raw file partitions with the correct index into the context
    // array.
    //

    if (Context->RawFiles != NULL) {
        for (FileIndex = 0; FileIndex < Context->RawFileCount; FileIndex += 1) {
            RawFile = &(Context->RawFiles[FileIndex]);
            RawFile->Partition = NULL;
            for (Index = 0;
                 Index < PartitionContext->PartitionCount;
                 Index += 1) {

                Partition = &(PartitionContext->Partitions[Index]);
                if (Partition->Number == RawFile->PartitionNumber) {
                    RawFile->Partition = Partition;
                    break;
                }
            }

            if (RawFile->Partition == NULL) {
                printf("Error: Raw file partition %d could not be found. "
                       "%d partitions exist.\n",
                       RawFile->PartitionNumber,
                       PartitionContext->PartitionCount);

                Status = STATUS_INVALID_PARAMETER;
                goto BindToPartitionsEnd;
            }
        }
    }

BindToPartitionsEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
CipValidatePartitionLayout (
    PCREATEIMAGE_CONTEXT Context
    )

/*++

Routine Description:

    This routine validates the created partition information before trying to
    write it out to disk.

Arguments:

    Context - Supplies a pointer to the createimage context.

Return Value:

    Status code.

--*/

{

    PPARTITION_INFORMATION Extended;
    BOOL FoundBoot;
    ULONG HumanSize;
    ULONGLONG LastEnd;
    ULONGLONG Length;
    PPARTITION_INFORMATION Partition;
    ULONG PartitionIndex;
    ULONG PrimaryCount;
    UCHAR Suffix;

    Extended = NULL;
    FoundBoot = FALSE;
    LastEnd = 0;
    PrimaryCount = 0;
    for (PartitionIndex = 0;
         PartitionIndex < Context->CreatePartitionCount;
         PartitionIndex += 1) {

        Partition = &(Context->CreatePartitions[PartitionIndex]);
        if ((Context->Options & CREATEIMAGE_OPTION_VERBOSE) != 0) {
            CipGetHumanSize(Partition->StartOffset * CREATEIMAGE_SECTOR_SIZE,
                            &HumanSize,
                            &Suffix);

            printf("Partition: %d%c", HumanSize, Suffix);
            CipGetHumanSize(Partition->EndOffset * CREATEIMAGE_SECTOR_SIZE,
                            &HumanSize,
                            &Suffix);

            printf(" - %d%c", HumanSize, Suffix);
            Length = (Partition->EndOffset - Partition->StartOffset) *
                     CREATEIMAGE_SECTOR_SIZE;

            CipGetHumanSize(Length, &HumanSize, &Suffix);
            printf(", Length %d%c", HumanSize, Suffix);
            if ((Partition->Flags & PARTITION_FLAG_PRIMARY) != 0) {
                printf(", primary");
            }

            if ((Partition->Flags & PARTITION_FLAG_EXTENDED) != 0) {
                printf(", extended");
            }

            if ((Partition->Flags & PARTITION_FLAG_LOGICAL) != 0) {
                printf(", logical");
            }

            if ((Partition->Flags & PARTITION_FLAG_BOOT) != 0) {
                printf(", boot");
            }

            printf("\n");
        }

        if (Partition->StartOffset == 0) {
            printf("Error: Partition start cannot be zero.\n");
            return STATUS_INVALID_CONFIGURATION;
        }

        if (Partition->EndOffset < Partition->StartOffset) {
            printf("Error: Partition end %I64x is less than start %I64x.\n",
                   Partition->EndOffset,
                   Partition->StartOffset);

            return STATUS_INVALID_CONFIGURATION;
        }

        if (Partition->StartOffset < LastEnd) {
            printf("Error: Partition start %I64x is less than last partition "
                   "end %I64x.\n",
                   Partition->StartOffset,
                   LastEnd);

            return STATUS_INVALID_CONFIGURATION;
        }

        if (Partition->EndOffset > Context->DiskSize) {
            printf("Error: Partition end offset %I64x goes off the end of "
                   "the disk (disk block count %I64x).\n",
                   Partition->EndOffset,
                   Context->DiskSize);

            return STATUS_INVALID_CONFIGURATION;
        }

        Partition = &(Context->CreatePartitions[PartitionIndex]);
        if ((Partition->Flags & PARTITION_FLAG_BOOT) != 0) {
            if (FoundBoot == FALSE) {
                FoundBoot = TRUE;

            } else {
                printf("Error: Multiple active/boot partitions.\n");
                return STATUS_INVALID_CONFIGURATION;
            }
        }

        if ((Partition->Flags & PARTITION_FLAG_PRIMARY) != 0) {
            Extended = NULL;
        }

        if ((Partition->Flags &
             (PARTITION_FLAG_PRIMARY | PARTITION_FLAG_EXTENDED)) != 0) {

            PrimaryCount += 1;
            if (PrimaryCount > 4) {
                printf("Error: Too many primary/extended partitions (max is "
                       "4).\n");

                return STATUS_INVALID_CONFIGURATION;
            }
        }

        if ((Partition->Flags & PARTITION_FLAG_EXTENDED) != 0) {
            Extended = Partition;
        }

        if ((Partition->Flags & PARTITION_FLAG_LOGICAL) != 0) {
            if (Extended == NULL) {
                printf("Error: Logical partitions must be inside an extended "
                       "partition.\n");

                return STATUS_INVALID_CONFIGURATION;
            }

            if ((Partition->StartOffset < Extended->StartOffset) ||
                (Partition->EndOffset > Extended->EndOffset)) {

                printf("Error: Logical partition (%I64x, %I64x) falls outside "
                       "its parent extended partition (%I64x, %I64x).\n",
                       Partition->StartOffset,
                       Partition->EndOffset,
                       Extended->StartOffset,
                       Extended->EndOffset);

                return STATUS_INVALID_CONFIGURATION;
            }
        }

        LastEnd = Partition->EndOffset;
    }

    if ((FoundBoot == FALSE) && (Context->CreatePartitionCount != 0) &&
        ((Context->Options & CREATEIMAGE_OPTION_GPT) == 0)) {

        printf("Warning: No active partition was specified.\n");
    }

    return STATUS_SUCCESS;
}

PVOID
CipPartitionAllocate (
    UINTN Size
    )

/*++

Routine Description:

    This routine is called when the partition library needs to allocate memory.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    return malloc(Size);
}

VOID
CipPartitionFree (
    PVOID Memory
    )

/*++

Routine Description:

    This routine is called when the partition library needs to free allocated
    memory.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

{

    free(Memory);
    return;
}

KSTATUS
CipPartitionRead (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    )

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

{

    PCREATEIMAGE_CONTEXT ApplicationContext;
    ssize_t BytesRead;
    FILE *File;

    ApplicationContext = PARENT_STRUCTURE(Context,
                                          CREATEIMAGE_CONTEXT,
                                          PartitionContext);

    File = ApplicationContext->OutputFile;

    assert(File != NULL);

    fseeko64(File, BlockAddress * CREATEIMAGE_SECTOR_SIZE, SEEK_SET);
    BytesRead = fread(Buffer, 1, CREATEIMAGE_SECTOR_SIZE, File);
    if (BytesRead != CREATEIMAGE_SECTOR_SIZE) {
        printf("createimage: Unable to read.\n");
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
CipPartitionWrite (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    )

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

{

    PCREATEIMAGE_CONTEXT ApplicationContext;
    ssize_t BytesWritten;
    FILE *File;

    ApplicationContext = PARENT_STRUCTURE(Context,
                                          CREATEIMAGE_CONTEXT,
                                          PartitionContext);

    File = ApplicationContext->OutputFile;

    assert(File != NULL);

    fseeko64(File, BlockAddress * CREATEIMAGE_SECTOR_SIZE, SEEK_SET);
    BytesWritten = fwrite(Buffer, 1, CREATEIMAGE_SECTOR_SIZE, File);
    if (BytesWritten != CREATEIMAGE_SECTOR_SIZE) {
        printf("createimage: Unable to write.\n");
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

VOID
CipPartitionFillRandom (
    PPARTITION_CONTEXT Context,
    PUCHAR Buffer,
    ULONG BufferSize
    )

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

{

    INT Value;

    while (BufferSize != 0) {
        Value = rand();
        if (BufferSize >= sizeof(USHORT)) {
            RtlCopyMemory(Buffer, &Value, sizeof(USHORT));
            Buffer += sizeof(USHORT);
            BufferSize -= sizeof(USHORT);

        } else {
            RtlCopyMemory(Buffer, &Value, BufferSize);
            break;
        }
    }

    return;
}

VOID
CipGetHumanSize (
    ULONGLONG Bytes,
    PULONG Size,
    PUCHAR Suffix
    )

/*++

Routine Description:

    This routine converts a byte count into something people enjoy looking at
    more.

Arguments:

    Bytes - Supplies the raw byte count.

    Size - Supplies a pointer where the modified size will be returned.

    Suffix - Supplies a pointer where the suffix byte will be returned.

Return Value:

    None.

--*/

{

    ULONGLONG HumanSize;

    HumanSize = Bytes;
    *Suffix = 0;
    if (HumanSize > 1024) {
        *Suffix = 'K';
        HumanSize /= 1024;
    }

    if (HumanSize > 1024) {
        *Suffix = 'M';
        HumanSize /= 1024;
    }

    if (HumanSize > 1024) {
        *Suffix = 'G';
        HumanSize /= 1024;
    }

    if (HumanSize > 1024) {
        *Suffix = 'T';
        HumanSize /= 1024;
    }

    *Size = HumanSize;
    return;
}

KSTATUS
CipConvertStringToGuid (
    PSTR GuidString,
    PSTR *StringAfterScan,
    UCHAR *GuidBuffer
    )

/*++

Routine Description:

    This routine converts a string to an GUID type identifier The string must
    be in the {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format.

Arguments:

    GuidString - Supplies a pointer to ascii string.

    StringAfterScan - Supplies a pointer that receives the location in the
        string after the can for the GUID has been completed.

    GuidBuffer - Supplies a pointer where the GUID will be returned on
        success.

Return Value:

    Status code.

--*/

{

    ULONG Data1;
    ULONG Data2;
    ULONG Data3;
    ULONG Data4[8];
    INT Index;
    INT ItemsScanned;

    *StringAfterScan = GuidString;
    if ((GuidString == NULL) || (GuidBuffer == NULL)) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Check that the Guid Format is strictly
    // {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}.
    //

    for (Index = 0;
         (GuidString[Index] != '\0') && (Index < 38);
         Index += 1) {

        if (Index == 0) {
            if (GuidString[Index] != '{') {
                break;
            }

        } else if (Index == 37) {
            if (GuidString[Index] != '}') {
                break;
            }

        } else if ((Index == 9) ||
                   (Index == 14) ||
                   (Index == 19) ||
                   (Index == 24)) {

            if (GuidString[Index] != '-') {
                break;
            }

        } else {
            if (((GuidString[Index] >= '0') &&
                 (GuidString[Index] <= '9')) ||
                ((GuidString[Index] >= 'a') &&
                 (GuidString[Index] <= 'f')) ||
                ((GuidString[Index] >= 'A') &&
                 (GuidString[Index] <= 'F'))) {

                continue;

            } else {
                break;
            }
        }
    }

    if (Index < 38) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Scan the guid string into the local variables.
    //

    ItemsScanned = sscanf(GuidString,
                          "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                          &Data1,
                          &Data2,
                          &Data3,
                          &Data4[0],
                          &Data4[1],
                          &Data4[2],
                          &Data4[3],
                          &Data4[4],
                          &Data4[5],
                          &Data4[6],
                          &Data4[7]);

    //
    // Verify the correct number of items were scanned.
    //

    if (ItemsScanned != 11) {
        return STATUS_UNSUCCESSFUL;
    }

    *StringAfterScan = &(GuidString[Index]);

    //
    // Copy the data into the GUID buffer.
    //

    GuidBuffer[0] = (UCHAR)Data1;
    GuidBuffer[1] = (UCHAR)(Data1 >> 8);
    GuidBuffer[2] = (UCHAR)(Data1 >> 16);
    GuidBuffer[3] = (UCHAR)(Data1 >> 24);
    GuidBuffer[4] = (UCHAR)Data2;
    GuidBuffer[5] = (UCHAR)(Data2 >> 8);
    GuidBuffer[6] = (UCHAR)Data3;
    GuidBuffer[7] = (UCHAR)(Data3 >> 8);
    GuidBuffer[8] = (UCHAR)Data4[0];
    GuidBuffer[9] = (UCHAR)Data4[1];
    GuidBuffer[10] = (UCHAR)Data4[2];
    GuidBuffer[11] = (UCHAR)Data4[3];
    GuidBuffer[12] = (UCHAR)Data4[4];
    GuidBuffer[13] = (UCHAR)Data4[5];
    GuidBuffer[14] = (UCHAR)Data4[6];
    GuidBuffer[15] = (UCHAR)Data4[7];
    return STATUS_SUCCESS;
}

