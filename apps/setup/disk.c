/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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

#include "setup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_MAX_PARTITION_COUNT 3

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

    ULONGLONG BlockCount;
    ULONG BlockIndex;
    ULONG DiskPadding;
    ULONGLONG MainSize;
    PPARTITION_CONTEXT PartitionContext;
    ULONG PartitionCount;
    ULONG PartitionIndex;
    PPARTITION_INFORMATION Partitions;
    INT Result;
    CHAR SizeString[20];
    KSTATUS Status;
    PVOID ZeroBuffer;

    assert((Context->Disk == NULL) && (Context->DiskPath != NULL));

    Partitions = NULL;

    //
    // Open up the disk.
    //

    Context->Disk = SetupOpenDestination(Context->DiskPath, O_RDWR, 0);
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

    PartitionContext->BlockCount /= SETUP_BLOCK_SIZE;
    PartitionContext->Format = Context->DiskFormat;
    Status = PartInitialize(PartitionContext);
    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "Error: Failed to initialize partition library: %x\n",
                Status);

        Result = -1;
        goto FormatDiskEnd;
    }

    //
    // Initialize the partition layout.
    //

    DiskPadding = 0;
    if (PartitionContext->Format == PartitionFormatGpt) {
        DiskPadding = 40;
    }

    Partitions = malloc(
                    sizeof(PARTITION_INFORMATION) * SETUP_MAX_PARTITION_COUNT);

    if (Partitions == NULL) {
        Result = ENOMEM;
        goto FormatDiskEnd;
    }

    memset(Partitions,
           0,
           sizeof(PARTITION_INFORMATION) * SETUP_MAX_PARTITION_COUNT);

    PartitionCount = 2;

    //
    // Create the boot partition.
    //

    Partitions[0].StartOffset = 1 + DiskPadding;
    if ((Context->Flags & SETUP_FLAG_1MB_PARTITION_ALIGNMENT) != 0) {
        Partitions[0].StartOffset = ALIGN_RANGE_UP(Partitions[0].StartOffset,
                                                   _1MB / SETUP_BLOCK_SIZE);
    }

    Partitions[0].EndOffset = Partitions[0].StartOffset +
                              SETUP_BOOT_PARTITION_SIZE;

    if ((Context->Flags & SETUP_FLAG_1MB_PARTITION_ALIGNMENT) != 0) {
        Partitions[0].EndOffset = ALIGN_RANGE_UP(Partitions[0].EndOffset,
                                                 _1MB / SETUP_BLOCK_SIZE);
    }

    Partitions[0].Number = 1;
    Partitions[0].Flags = PARTITION_FLAG_PRIMARY | PARTITION_FLAG_BOOT;
    Partitions[0].PartitionType = PartitionTypeEfiSystem;
    Context->BootPartitionOffset = Partitions[0].StartOffset;
    Context->BootPartitionSize = Partitions[0].EndOffset -
                                 Partitions[0].StartOffset;

    //
    // Create the one or two primary partitions.
    //

    if (Partitions[0].EndOffset + DiskPadding + 2 >=
        PartitionContext->BlockCount) {

        SetupPrintSize(
                   SizeString,
                   sizeof(SizeString),
                   PartitionContext->BlockCount * PartitionContext->BlockSize);

        fprintf(stderr, "Error: Disk is too small: %s.\n", SizeString);
        Result = EINVAL;
        goto FormatDiskEnd;
    }

    MainSize = PartitionContext->BlockCount -
               (Partitions[0].EndOffset + DiskPadding);

    if ((Context->Flags & SETUP_FLAG_TWO_PARTITIONS) != 0) {
        PartitionCount += 1;
        MainSize /= 2;
    }

    if ((Context->Flags & SETUP_FLAG_1MB_PARTITION_ALIGNMENT) != 0) {
        MainSize = ALIGN_RANGE_DOWN(MainSize, _1MB / SETUP_BLOCK_SIZE);
        if (MainSize == 0) {
            fprintf(stderr, "Error: Disk is too small!\n");
            Result = EINVAL;
            goto FormatDiskEnd;
        }
    }

    Partitions[1].StartOffset = Partitions[0].EndOffset;
    Partitions[1].EndOffset = Partitions[1].StartOffset + MainSize;
    Partitions[1].Number = 2;
    Partitions[1].Flags = PARTITION_FLAG_PRIMARY;
    Partitions[1].PartitionType = PartitionTypeMinoca;
    if ((Context->Flags & SETUP_FLAG_TWO_PARTITIONS) != 0) {
        Partitions[2].StartOffset = Partitions[1].EndOffset;
        Partitions[2].EndOffset = Partitions[2].StartOffset + MainSize;
        Partitions[2].Number = 3;
        Partitions[2].Flags = PARTITION_FLAG_PRIMARY;
        Partitions[2].PartitionType = PartitionTypeMinoca;
    }

    Status = PartWritePartitionLayout(PartitionContext,
                                      Context->DiskFormat,
                                      Partitions,
                                      PartitionCount,
                                      TRUE);

    if (!KSUCCESS(Status)) {
        Result = -1;
        fprintf(stderr,
                "Error: Failed to write partition layout: %x\n",
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
                "Error: Failed to reenumerate partitions: %x\n",
                Status);

        Result = -1;
        goto FormatDiskEnd;
    }

    ASSERT((PartitionContext->PartitionCount == PartitionCount) &&
           (SETUP_MAX_PARTITION_COUNT > 1));

    Partitions = PartitionContext->Partitions;
    ZeroBuffer = malloc(PartitionContext->BlockSize);
    if (ZeroBuffer == NULL) {
        Result = ENOMEM;
        goto FormatDiskEnd;
    }

    memset(ZeroBuffer, 0, PartitionContext->BlockSize);

    //
    // Clear out the first 16kB of each partition to remove any file systems
    // tha may have happened to exist there.
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
                        "Error: Failed to clear %I64x: %x\n",
                        Partitions[PartitionIndex].StartOffset + BlockIndex,
                        Status);
            }
        }
    }

    free(ZeroBuffer);

    //
    // Fill in the install partition information.
    //

    Context->InstallPartition.Version = PARTITION_DEVICE_INFORMATION_VERSION;
    Context->InstallPartition.PartitionFormat = PartitionContext->Format;
    Context->InstallPartition.PartitionType = Partitions[1].PartitionType;
    Context->InstallPartition.Flags = Partitions[1].Flags;
    Context->InstallPartition.BlockSize = SETUP_BLOCK_SIZE;
    Context->InstallPartition.Number = Partitions[1].Number;
    Context->InstallPartition.FirstBlock = Partitions[1].StartOffset;
    Context->InstallPartition.LastBlock = Partitions[1].EndOffset - 1;
    RtlCopyMemory(&(Context->InstallPartition.PartitionId),
                  &(Partitions[1].Identifier),
                  sizeof(Context->InstallPartition.PartitionId));

    RtlCopyMemory(&(Context->InstallPartition.PartitionTypeId),
                  &(Partitions[1].TypeIdentifier),
                  sizeof(Context->InstallPartition.PartitionTypeId));

    RtlCopyMemory(&(Context->InstallPartition.DiskId),
                  &(PartitionContext->DiskIdentifier),
                  sizeof(Context->InstallPartition.DiskId));

FormatDiskEnd:
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
    ULONGLONG BytesRead;
    ULONGLONG Result;

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
    ULONGLONG BytesWritten;
    ULONGLONG Result;

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

