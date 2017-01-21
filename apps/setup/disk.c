/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    disk.c

Abstract:

    This module implements support for working with the disk directly in
    the setup app.

Author:

    Evan Green 11-Apr-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "setup.h"
#include "sconf.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the boot partition size in blocks.
//

#define SETUP_BOOT_PARTITION_SIZE ((_1MB * 10) / SETUP_BLOCK_SIZE)

//
// Define the amount to clear of the beginning of each partition.
//

#define SETUP_PARTITION_CLEAR_SIZE (1024 * 16)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
SetupPartitionLibraryAllocate (
    UINTN Size
    );

VOID
SetupPartitionLibraryFree (
    PVOID Memory
    );

KSTATUS
SetupPartitionLibraryRead (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    );

KSTATUS
SetupPartitionLibraryWrite (
    PPARTITION_CONTEXT Context,
    ULONGLONG BlockAddress,
    PVOID Buffer
    );

VOID
SetupPartitionLibraryFillRandom (
    PPARTITION_CONTEXT Context,
    PUCHAR Buffer,
    ULONG BufferSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SetupFormatDisk (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine partitions a disk.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINTN AllocationSize;
    ULONGLONG BlockCount;
    ULONG BlockIndex;
    ULONGLONG BlockOffset;
    PSETUP_DISK_CONFIGURATION DiskConfiguration;
    ULONGLONG FreeSize;
    PSETUP_PARTITION_CONFIGURATION PartitionConfig;
    PPARTITION_CONTEXT PartitionContext;
    ULONG PartitionCount;
    ULONG PartitionDataStart;
    ULONG PartitionIndex;
    PPARTITION_INFORMATION PartitionInfo;
    PPARTITION_INFORMATION Partitions;
    INT Result;
    ULONGLONG Size;
    ULONG SplitCount;
    ULONGLONG Start;
    KSTATUS Status;
    ULONG SystemIndex;
    PVOID ZeroBuffer;

    assert((Context->Disk == NULL) && (Context->DiskPath != NULL));

    DiskConfiguration = &(Context->Configuration->Disk);
    Partitions = NULL;
    ZeroBuffer = NULL;

    //
    // Open up the disk.
    //

    Context->Disk = SetupOpenDestination(Context->DiskPath,
                                         O_CREAT | O_RDWR,
                                         0664);

    if (Context->Disk == NULL) {
        printf("Error: Failed to open ");
        SetupPrintDestination(Context->DiskPath);
        Result = errno;
        if (errno == 0) {
            Result = -1;
        }

        printf(": %s\n", strerror(Result));
        return Result;
    }

    //
    // Set up the partition context.
    //

    PartitionContext = &(Context->PartitionContext);
    PartitionContext->AllocateFunction = SetupPartitionLibraryAllocate;
    PartitionContext->FreeFunction = SetupPartitionLibraryFree;
    PartitionContext->ReadFunction = SetupPartitionLibraryRead;
    PartitionContext->WriteFunction = SetupPartitionLibraryWrite;
    PartitionContext->FillRandomFunction = SetupPartitionLibraryFillRandom;
    PartitionContext->BlockSize = SETUP_BLOCK_SIZE;
    Result = SetupFstat(Context->Disk,
                        &(PartitionContext->BlockCount),
                        NULL,
                        NULL);

    if (Result != 0) {
        goto FormatDiskEnd;
    }

    //
    // Use the override if one was specified.
    //

    if (Context->DiskSize != 0) {
        PartitionContext->BlockCount = Context->DiskSize;
    }

    PartitionContext->BlockCount /= SETUP_BLOCK_SIZE;
    if (PartitionContext->BlockCount == 0) {
        fprintf(stderr, "Error: Disk has zero size.\n");
        Result = ERANGE;
        goto FormatDiskEnd;
    }

    PartitionContext->Format = DiskConfiguration->PartitionFormat;
    Status = PartInitialize(PartitionContext);
    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "Error: Failed to initialize partition library: %d\n",
                Status);

        Result = -1;
        goto FormatDiskEnd;
    }

    //
    // Initialize the partition layout.
    //

    switch (PartitionContext->Format) {
    case PartitionFormatNone:
        PartitionDataStart = 0;
        break;

    case PartitionFormatMbr:
        PartitionDataStart = 1;
        break;

    case PartitionFormatGpt:
        PartitionDataStart = 41;
        break;

    default:

        assert(FALSE);

        Result = EINVAL;
        goto FormatDiskEnd;
    }

    AllocationSize = sizeof(PARTITION_INFORMATION) *
                     DiskConfiguration->PartitionCount;

    Partitions = malloc(AllocationSize);
    if (Partitions == NULL) {
        Result = ENOMEM;
        goto FormatDiskEnd;
    }

    memset(Partitions, 0, AllocationSize);

    //
    // Go through once to calculate the main partition size and how many
    // partitions to split it between.
    //

    BlockOffset = PartitionDataStart;
    SplitCount = 0;
    for (PartitionIndex = 0;
         PartitionIndex < DiskConfiguration->PartitionCount;
         PartitionIndex += 1) {

        PartitionConfig = &(DiskConfiguration->Partitions[PartitionIndex]);
        Start = BlockOffset;
        if (PartitionConfig->Alignment > SETUP_BLOCK_SIZE) {
            Start =
                ALIGN_RANGE_UP(BlockOffset,
                               PartitionConfig->Alignment / SETUP_BLOCK_SIZE);
        }

        BlockOffset = Start;
        if (PartitionConfig->Size == -1ULL) {
            SplitCount += 1;
            continue;
        }

        Size = ALIGN_RANGE_UP(PartitionConfig->Size, SETUP_BLOCK_SIZE) /
               SETUP_BLOCK_SIZE;

        BlockOffset += Size;
    }

    if (BlockOffset > PartitionContext->BlockCount) {
        fprintf(stderr,
                "Error: Partition specification goes out to block 0x%llx, "
                "but disk size is only 0x%llx.\n",
                BlockOffset,
                PartitionContext->BlockCount);

        Result = ERANGE;
        goto FormatDiskEnd;
    }

    assert(BlockOffset <= PartitionContext->BlockCount);

    FreeSize = PartitionContext->BlockCount - BlockOffset;
    if (PartitionContext->Format == PartitionFormatGpt) {
        if (FreeSize < 40) {
            fprintf(stderr, "Error: Disk too small for GPT footer.\n");
            Result = ERANGE;
            goto FormatDiskEnd;
        }

        FreeSize -= 40;
    }

    if (SplitCount > 1) {
        FreeSize /= SplitCount;
    }

    //
    // Initialize the partition structures.
    //

    PartitionCount = DiskConfiguration->PartitionCount;
    SystemIndex = PartitionCount;
    BlockOffset = PartitionDataStart;
    for (PartitionIndex = 0;
         PartitionIndex < DiskConfiguration->PartitionCount;
         PartitionIndex += 1) {

        PartitionInfo = &(Partitions[PartitionIndex]);
        PartitionConfig = &(DiskConfiguration->Partitions[PartitionIndex]);
        Start = BlockOffset;
        if (PartitionConfig->Alignment > SETUP_BLOCK_SIZE) {
            Start =
                ALIGN_RANGE_UP(BlockOffset,
                               PartitionConfig->Alignment / SETUP_BLOCK_SIZE);
        }

        PartitionInfo->StartOffset = Start;
        PartitionConfig->Offset = Start * SETUP_BLOCK_SIZE;

        //
        // If the partition size is -1, use as much space as is available.
        // Watch out for differences due to alignment requirements between
        // this loop and the earlier one.
        //

        if (PartitionConfig->Size == -1ULL) {
            if (FreeSize > PartitionContext->BlockCount) {
                Size = PartitionContext->BlockCount - Start;

            } else {
                Size = FreeSize;
            }

        } else {
            Size = ALIGN_RANGE_UP(PartitionConfig->Size, SETUP_BLOCK_SIZE) /
                   SETUP_BLOCK_SIZE;
        }

        PartitionConfig->Size = Size * SETUP_BLOCK_SIZE;
        PartitionInfo->EndOffset = Start + Size;
        BlockOffset = PartitionInfo->EndOffset;
        if ((PartitionInfo->EndOffset > PartitionContext->BlockCount) ||
            (Start + Size < Start)) {

            fprintf(stderr,
                    "Error: Partition blocks 0x%llx + 0x%llx bigger than "
                    "disk size 0x%llx (%lldMB).\n",
                    Start,
                    Size,
                    PartitionContext->BlockCount,
                    PartitionContext->BlockCount * SETUP_BLOCK_SIZE / _1MB);

            Result = EINVAL;
            goto FormatDiskEnd;
        }

        PartitionInfo->Number = PartitionIndex + 1;
        PartitionInfo->Flags = PARTITION_FLAG_PRIMARY;
        if ((PartitionConfig->Flags & SETUP_PARTITION_FLAG_BOOT) != 0) {
            PartitionInfo->Flags |= PARTITION_FLAG_BOOT;
        }

        if ((PartitionConfig->Flags & SETUP_PARTITION_FLAG_SYSTEM) != 0) {
            SystemIndex = PartitionIndex;
        }

        if (DiskConfiguration->PartitionFormat == PartitionFormatGpt) {
            memcpy(PartitionInfo->TypeIdentifier,
                   PartitionConfig->PartitionType,
                   PARTITION_TYPE_SIZE);

        } else if (DiskConfiguration->PartitionFormat == PartitionFormatMbr) {
            PartitionInfo->TypeIdentifier[0] = PartitionConfig->MbrType;
        }

        PartitionInfo->Attributes = PartitionConfig->Attributes;
    }

    //
    // If the disk is actually not partitioned, end now.
    //

    if (DiskConfiguration->PartitionFormat == PartitionFormatNone) {
        Status = 0;
        goto FormatDiskEnd;
    }

    if (SystemIndex == PartitionCount) {
        fprintf(stderr,
                "Error: One partition must be designated the system "
                "partition.\n");

        Status = EINVAL;
        goto FormatDiskEnd;
    }

    Status = PartWritePartitionLayout(PartitionContext,
                                      DiskConfiguration->PartitionFormat,
                                      Partitions,
                                      PartitionCount,
                                      TRUE);

    if (!KSUCCESS(Status)) {
        Result = -1;
        fprintf(stderr,
                "Error: Failed to write partition layout: %d\n",
                Status);

        goto FormatDiskEnd;
    }

    //
    // Re-read the partition information to get the randomly assigned partition
    // and disk IDs.
    //

    Status = PartEnumeratePartitions(PartitionContext);
    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "Error: Failed to reenumerate partitions: %d\n",
                Status);

        Result = -1;
        goto FormatDiskEnd;
    }

    assert(PartitionContext->PartitionCount == PartitionCount);

    Partitions = PartitionContext->Partitions;
    ZeroBuffer = malloc(PartitionContext->BlockSize);
    if (ZeroBuffer == NULL) {
        Result = ENOMEM;
        goto FormatDiskEnd;
    }

    memset(ZeroBuffer, 0, PartitionContext->BlockSize);

    //
    // Clear out the space before the first partition.
    //

    BlockCount = Partitions[0].StartOffset;
    for (BlockIndex = PartitionDataStart;
         BlockIndex < BlockCount;
         BlockIndex += 1) {

        Status = SetupPartitionLibraryWrite(PartitionContext,
                                            BlockIndex,
                                            ZeroBuffer);

        if (!KSUCCESS(Status)) {
            fprintf(stderr,
                    "Error: Failed to clear %llx: %d\n",
                    Partitions[PartitionIndex].StartOffset + BlockIndex,
                    Status);

            Result = -1;
            goto FormatDiskEnd;
        }
    }

    //
    // Clear out the first 16kB of each partition to remove any file systems
    // that may have happened to exist there.
    //

    for (PartitionIndex = 0;
         PartitionIndex < PartitionCount;
         PartitionIndex += 1) {

        BlockCount = SETUP_PARTITION_CLEAR_SIZE / PartitionContext->BlockSize;
        if (BlockCount >
            (Partitions[PartitionIndex].EndOffset -
             Partitions[PartitionIndex].StartOffset)) {

            BlockCount = Partitions[PartitionIndex].EndOffset -
                         Partitions[PartitionIndex].StartOffset;
        }

        for (BlockIndex = 0; BlockIndex < BlockCount; BlockIndex += 1) {
            Status = SetupPartitionLibraryWrite(
                          PartitionContext,
                          Partitions[PartitionIndex].StartOffset + BlockIndex,
                          ZeroBuffer);

            if (!KSUCCESS(Status)) {
                fprintf(stderr,
                        "Error: Failed to clear %llx: %d\n",
                        Partitions[PartitionIndex].StartOffset + BlockIndex,
                        Status);

                Result = -1;
                goto FormatDiskEnd;
            }
        }
    }

    //
    // Read and write the last sector on the disk, to ensure that for disks
    // that are actually files that the file size is correct.
    //

    Status = SetupPartitionLibraryRead(PartitionContext,
                                       PartitionContext->BlockCount - 1,
                                       ZeroBuffer);

    if (Status != 0) {
        fprintf(stderr, "Error: Failed to read the last sector.\n");
        Result = -1;
        goto FormatDiskEnd;
    }

    Status = SetupPartitionLibraryWrite(PartitionContext,
                                        PartitionContext->BlockCount - 1,
                                        ZeroBuffer);

    if (Status != 0) {
        fprintf(stderr, "Error: Failed to write the last sector.\n");
        Result = -1;
        goto FormatDiskEnd;
    }

    //
    // Convert the actual partition data back into the configuration
    // structures.
    //

    for (PartitionIndex = 0;
         PartitionIndex < DiskConfiguration->PartitionCount;
         PartitionIndex += 1) {

        PartitionInfo = &(Partitions[PartitionIndex]);
        PartitionConfig = &(DiskConfiguration->Partitions[PartitionIndex]);
        PartitionConfig->Index = PartitionInfo->Number - 1;
        PartitionConfig->Offset = PartitionInfo->StartOffset * SETUP_BLOCK_SIZE;
        Size = (PartitionInfo->EndOffset - PartitionInfo->StartOffset) *
               SETUP_BLOCK_SIZE;

        assert(Size == PartitionConfig->Size);

        PartitionConfig->Size = Size;
        RtlCopyMemory(&(PartitionConfig->PartitionId),
                      &(PartitionInfo->Identifier),
                      PARTITION_IDENTIFIER_SIZE);

        RtlCopyMemory(&(PartitionConfig->PartitionType),
                      &(PartitionInfo->TypeIdentifier),
                      PARTITION_TYPE_SIZE);

        if (DiskConfiguration->PartitionFormat == PartitionFormatMbr) {

            assert(PartitionConfig->MbrType ==
                   PartitionInfo->TypeIdentifier[0]);
        }
    }

FormatDiskEnd:
    if (ZeroBuffer != NULL) {
        free(ZeroBuffer);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
SetupPartitionLibraryAllocate (
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
SetupPartitionLibraryFree (
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
SetupPartitionLibraryRead (
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

    PSETUP_CONTEXT ApplicationContext;
    LONGLONG BytesRead;
    LONGLONG Result;

    ApplicationContext = PARENT_STRUCTURE(Context,
                                          SETUP_CONTEXT,
                                          PartitionContext);

    Result = SetupSeek(ApplicationContext->Disk,
                       BlockAddress * SETUP_BLOCK_SIZE);

    if (Result != BlockAddress * SETUP_BLOCK_SIZE) {
        return STATUS_UNSUCCESSFUL;
    }

    BytesRead = SetupRead(ApplicationContext->Disk, Buffer, SETUP_BLOCK_SIZE);
    if (BytesRead != SETUP_BLOCK_SIZE) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
SetupPartitionLibraryWrite (
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

    PSETUP_CONTEXT ApplicationContext;
    LONGLONG BytesWritten;
    LONGLONG Result;

    ApplicationContext = PARENT_STRUCTURE(Context,
                                          SETUP_CONTEXT,
                                          PartitionContext);

    Result = SetupSeek(ApplicationContext->Disk,
                       BlockAddress * SETUP_BLOCK_SIZE);

    if (Result != BlockAddress * SETUP_BLOCK_SIZE) {
        return STATUS_UNSUCCESSFUL;
    }

    BytesWritten = SetupWrite(ApplicationContext->Disk,
                              Buffer,
                              SETUP_BLOCK_SIZE);

    if (BytesWritten != SETUP_BLOCK_SIZE) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

VOID
SetupPartitionLibraryFillRandom (
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

