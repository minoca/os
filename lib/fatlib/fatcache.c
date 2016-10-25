/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatcache.c

Abstract:

    This module implements the cache of the File Allocation Table.

Author:

    Chris Stevens 17-Oct-2013

Environment:

    Kernel, Boot, Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/fat/fatlib.h>
#include <minoca/lib/fat/fat.h>
#include "fatlibp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the minimum FAT window size. This must be at least 8k to completely
// capture a FAT12 FAT. If the FAT12 FAT is not completely captured, then there
// are potential problems with a cluster spanning two windows.
//

#define FAT_CACHE_MINIMUM_WINDOW_SIZE _128KB

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
FatpFatCacheReadWindow (
    PFAT_VOLUME Volume,
    BOOL VolumeLockHeld,
    ULONG WindowIndex
    );

KSTATUS
FatpFatCacheWriteWindow (
    PFAT_VOLUME Volume,
    ULONG IoFlags,
    ULONG WindowIndex
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
FatpCreateFatCache (
    PFAT_VOLUME Volume
    )

/*++

Routine Description:

    This routine creates the FAT cache for the given volume.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PVOID DeviceToken;
    ULONGLONG FatSize;
    KSTATUS Status;
    ULONG WindowCount;
    ULONG WindowIndex;
    ULONG WindowSize;

    //
    // Determine the appropriate size of each FAT cache window. Prefer making
    // each window the size of a system cache entry's data. This way, if the
    // FAT's offset is cache-aligned, then reads and writes will be more
    // efficient. That said, align the window size up to a block size. Any disk
    // reads for the FAT need to be at least the size of a block.
    //

    WindowSize = FatGetIoCacheEntryDataSize();
    if (WindowSize == 0) {
        WindowSize = FAT_CACHE_MINIMUM_WINDOW_SIZE;
    }

    WindowSize = ALIGN_RANGE_UP(WindowSize, Volume->Device.BlockSize);
    if (WindowSize < FAT_CACHE_MINIMUM_WINDOW_SIZE) {
        WindowSize = FAT_CACHE_MINIMUM_WINDOW_SIZE;
    }

    ASSERT(POWER_OF_2(WindowSize));

    //
    // Determine the number of FAT cache windows required.
    //

    ASSERT((Volume->FatByteStart != 0) && (Volume->FatSize != 0));
    ASSERT((Volume->Format == Fat12Format) ||
           (Volume->FatSize >=
            (Volume->ClusterCount << Volume->ClusterWidthShift)));

    FatSize = Volume->FatSize;
    FatSize = ALIGN_RANGE_UP(FatSize, WindowSize);
    WindowCount = FatSize / WindowSize;

    //
    // Allocate the window array and initialize it to have no present windows.
    //

    AllocationSize = (WindowCount * sizeof(PFAT_IO_BUFFER)) +
                     (WindowCount * sizeof(PVOID)) +
                     (WindowCount * sizeof(FAT_WINDOW_DIRTY_REGION));

    DeviceToken = Volume->Device.DeviceToken;
    Volume->FatCache.WindowBuffers = FatAllocateNonPagedMemory(DeviceToken,
                                                               AllocationSize);

    if (Volume->FatCache.WindowBuffers == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeFatCacheEnd;
    }

    RtlZeroMemory(Volume->FatCache.WindowBuffers, AllocationSize);
    Volume->FatCache.Windows = (PVOID *)(Volume->FatCache.WindowBuffers +
                                         WindowCount);

    Volume->FatCache.Dirty =
            (PFAT_WINDOW_DIRTY_REGION)(Volume->FatCache.Windows + WindowCount);

    for (WindowIndex = 0; WindowIndex < WindowCount; WindowIndex += 1) {
        Volume->FatCache.Dirty[WindowIndex].Min = WindowSize;
    }

    Volume->FatCache.DirtyStart = MAX_ULONG;
    Volume->FatCache.DirtyEnd = 0;
    Volume->FatCache.WindowSize = WindowSize;

    ASSERT(POWER_OF_2(WindowSize));

    Volume->FatCache.WindowShift = RtlCountTrailingZeros32(WindowSize);
    Volume->FatCache.WindowCount = WindowCount;
    Status = STATUS_SUCCESS;

InitializeFatCacheEnd:
    return Status;
}

VOID
FatpDestroyFatCache (
    PFAT_VOLUME Volume
    )

/*++

Routine Description:

    This routine destroys the FAT cache for the given volume.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

Return Value:

    None.

--*/

{

    ULONG Index;

    //
    // The FAT windows are flushed immediately on writes, so this routine only
    // needs to free FAT I/O buffers and the window array. Freeing a FAT I/O
    // buffer will also unmap it.
    //

    for (Index = 0; Index < Volume->FatCache.WindowCount; Index += 1) {
        if (Volume->FatCache.WindowBuffers[Index] != NULL) {
            FatFreeIoBuffer(Volume->FatCache.WindowBuffers[Index]);
        }
    }

    FatFreeNonPagedMemory(Volume->Device.DeviceToken,
                          Volume->FatCache.WindowBuffers);

    return;
}

BOOL
FatpFatCacheIsClusterEntryPresent (
    PFAT_VOLUME Volume,
    ULONG Cluster
    )

/*++

Routine Description:

    This routine determines whether or not the FAT cache entry for the given
    cluster is present in the cache.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the cluster whose FAT entry is in question.

Return Value:

    Returns TRUE if the cluster's FAT cache entry is present, or FALSE
    otherwise.

--*/

{

    ULONG WindowIndex;

    //
    // The FAT cache may be larger than the actual FAT. Assert that the cluster
    // is within the bounds of the FAT.
    //

    ASSERT(Cluster < Volume->ClusterCount);

    //
    // If the FAT window for this cluster is not present, then read it in.
    //

    WindowIndex = FAT_WINDOW_INDEX(Volume, Cluster);

    ASSERT(WindowIndex < Volume->FatCache.WindowCount);

    if (Volume->FatCache.Windows[WindowIndex] != NULL) {
        return TRUE;
    }

    return FALSE;
}

KSTATUS
FatpFatCacheReadClusterEntry (
    PFAT_VOLUME Volume,
    BOOL VolumeLockHeld,
    ULONG Cluster,
    PULONG Value
    )

/*++

Routine Description:

    This routine reads the FAT cache to get the next cluster for the given
    cluster.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    VolumeLockHeld - Supplies a boolean indicating whether or not the volume
        lock is already held.

    Cluster - Supplies the cluster whose FAT entry is being read.

    Value - Supplies a pointer where the value of the cluster entry will be
        return.

Return Value:

    Status code.

--*/

{

    PVOID FatWindow;
    KSTATUS Status;
    ULONG WindowIndex;
    ULONG WindowOffset;

    ASSERT(Volume->FatCache.Windows != NULL);

    //
    // The FAT cache may be larger than the actual FAT. Assert that the cluster
    // is within the bounds of the FAT.
    //

    ASSERT(Cluster < Volume->ClusterCount);

    //
    // If the FAT window for this cluster is not present, then read it in.
    //

    WindowIndex = FAT_WINDOW_INDEX(Volume, Cluster);

    ASSERT(WindowIndex < Volume->FatCache.WindowCount);

    if (Volume->FatCache.Windows[WindowIndex] == NULL) {
        Status = FatpFatCacheReadWindow(Volume, VolumeLockHeld, WindowIndex);
        if (!KSUCCESS(Status)) {
            goto FatCacheReadNextClusterEnd;
        }
    }

    //
    // The appropriate FAT cache window should be in place. Get the mod by
    // shifting back out and subtracting.
    //

    ASSERT(Volume->FatCache.Windows[WindowIndex] != NULL);

    FatWindow = Volume->FatCache.Windows[WindowIndex];
    if (Volume->Format == Fat12Format) {
        *Value = FAT12_READ_CLUSTER(FatWindow, Cluster);

    } else {
        WindowOffset = Cluster - FAT_WINDOW_INDEX_TO_CLUSTER(Volume,
                                                             WindowIndex);

        if (Volume->Format == Fat16Format) {
            *Value = ((PUSHORT)FatWindow)[WindowOffset];

        } else {
            *Value = ((PULONG)FatWindow)[WindowOffset];
        }
    }

    Status = STATUS_SUCCESS;

FatCacheReadNextClusterEnd:
    return Status;
}

KSTATUS
FatpFatCacheGetFatWindow (
    PFAT_VOLUME Volume,
    BOOL VolumeLockHeld,
    ULONG Cluster,
    PVOID *Window,
    PULONG WindowOffset
    )

/*++

Routine Description:

    This routine returns a portion of the FAT.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    VolumeLockHeld - Supplies a boolean indicating whether or not the volume
        lock is already held.

    Cluster - Supplies the cluster to get the containing window for.

    Window - Supplies a pointer where the window will be returned.

    WindowOffset - Supplies a pointer where the offset into the window where
        the desired cluster resides will be returned.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONG WindowIndex;

    ASSERT(Volume->FatCache.Windows != NULL);

    //
    // The FAT cache may be larger than the actual FAT. Assert that the cluster
    // is within the bounds of the FAT.
    //

    ASSERT(Cluster < Volume->ClusterCount);

    //
    // If the FAT window for this cluster is not present, then read it in.
    //

    WindowIndex = FAT_WINDOW_INDEX(Volume, Cluster);

    ASSERT(WindowIndex < Volume->FatCache.WindowCount);

    if (Volume->FatCache.Windows[WindowIndex] == NULL) {
        Status = FatpFatCacheReadWindow(Volume, VolumeLockHeld, WindowIndex);
        if (!KSUCCESS(Status)) {
            goto FatCacheGetFatWindow;
        }
    }

    //
    // The appropriate FAT cache window should be in place. Get the mod by
    // shifting back out and subtracting.
    //

    ASSERT(Volume->FatCache.Windows[WindowIndex] != NULL);

    *WindowOffset = Cluster - FAT_WINDOW_INDEX_TO_CLUSTER(Volume, WindowIndex);
    *Window = Volume->FatCache.Windows[WindowIndex];
    Status = STATUS_SUCCESS;

FatCacheGetFatWindow:
    return Status;
}

KSTATUS
FatpFatCacheWriteClusterEntry (
    PFAT_VOLUME Volume,
    ULONG Cluster,
    ULONG NewValue,
    PULONG OldValue
    )

/*++

Routine Description:

    This routine writes the FAT cache to set the next cluster for the given
    cluster. This routine assumes that the volume lock is held. It will
    optionally return the previous contents of the entry.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    Cluster - Supplies the cluster whose FAT entry is being written.

    NewValue - Supplies the new value to be written to the given cluster's
        FAT entry.

    OldValue - Supplies an optional pointer that receives the old value of the
        given cluster's FAT entry.

Return Value:

    Status code.

--*/

{

    ULONG EndOffset;
    PFAT_CACHE FatCache;
    PVOID FatWindow;
    ULONG Original;
    ULONG StartOffset;
    KSTATUS Status;
    ULONG WindowIndex;
    ULONG WindowOffset;

    FatCache = &(Volume->FatCache);

    ASSERT(FatCache->Windows != NULL);

    //
    // The FAT cache may be larger than the actual FAT. Assert that the cluster
    // is within the bounds of the FAT.
    //

    ASSERT(Cluster < Volume->ClusterCount);

    //
    // If the FAT window for this cluster is not present, then read it in.
    //

    WindowIndex = FAT_WINDOW_INDEX(Volume, Cluster);

    ASSERT(WindowIndex < FatCache->WindowCount);

    if (FatCache->Windows[WindowIndex] == NULL) {
        Status = FatpFatCacheReadWindow(Volume, TRUE, WindowIndex);
        if (!KSUCCESS(Status)) {
            goto FatCacheWriteNextClusterEnd;
        }
    }

    //
    // The appropriate FAT cache window should be in place. Update it.
    //

    ASSERT(FatCache->Windows[WindowIndex] != NULL);

    FatWindow = FatCache->Windows[WindowIndex];
    WindowOffset = 0;
    if (Volume->Format == Fat12Format) {
        Original = FAT12_READ_CLUSTER(FatWindow, Cluster);
        StartOffset = (UINTN)FAT12_CLUSTER_BYTE(0, Cluster);
        EndOffset = (UINTN)FAT12_CLUSTER_BYTE(0, Cluster + 1);

    } else {
        WindowOffset = Cluster - FAT_WINDOW_INDEX_TO_CLUSTER(Volume,
                                                             WindowIndex);

        if (Volume->Format == Fat16Format) {
            Original = ((PUSHORT)FatWindow)[WindowOffset];
            StartOffset = WindowOffset * sizeof(USHORT);
            EndOffset = StartOffset + sizeof(USHORT);

        } else {
            Original = ((PULONG)FatWindow)[WindowOffset];
            StartOffset = WindowOffset * sizeof(ULONG);
            EndOffset = StartOffset + sizeof(ULONG);
        }
    }

    if (OldValue != NULL) {
        *OldValue = Original;
    }

    //
    // If this cluster is being marked free then it better have been allocated
    // and not marked free already.
    //

    if ((NewValue == FAT_CLUSTER_FREE) && (Original == FAT_CLUSTER_FREE)) {
        RtlDebugPrint("FAT: Cluster 0x%x was already free!\n", Cluster);
    }

    //
    // Skip the write below if the new value is the same as the old value.
    //

    if (Original == NewValue) {
        Status = STATUS_SUCCESS;
        goto FatCacheWriteNextClusterEnd;
    }

    if (Volume->Format == Fat12Format) {
        FAT12_WRITE_CLUSTER(FatWindow, Cluster, NewValue);

    } else if (Volume->Format == Fat16Format) {
        ((PUSHORT)FatWindow)[WindowOffset] = NewValue;

    } else {
        ((PULONG)FatWindow)[WindowOffset] = NewValue;
    }

    //
    // Mark the region in the window that's dirty.
    //

    if (FatCache->Dirty[WindowIndex].Min > StartOffset) {
        FatCache->Dirty[WindowIndex].Min = StartOffset;
    }

    if (FatCache->Dirty[WindowIndex].Max < EndOffset) {
        FatCache->Dirty[WindowIndex].Max = EndOffset;
    }

    //
    // Potentially expand the set of windows that need to be flushed.
    //

    if (FatCache->DirtyStart > WindowIndex) {
        FatCache->DirtyStart = WindowIndex;
    }

    if (FatCache->DirtyEnd < WindowIndex + 1) {
        FatCache->DirtyEnd = WindowIndex + 1;
    }

    Status = STATUS_SUCCESS;

FatCacheWriteNextClusterEnd:
    return Status;
}

KSTATUS
FatpFatCacheFlush (
    PFAT_VOLUME Volume,
    ULONG IoFlags
    )

/*++

Routine Description:

    This routine flushes the FATs down to the disk. This routine assumes the
    volume lock is already held.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

Return Value:

    Status code.

--*/

{

    PFAT_CACHE FatCache;
    KSTATUS Status;
    KSTATUS TotalStatus;
    ULONG WindowIndex;

    TotalStatus = STATUS_SUCCESS;
    FatCache = &(Volume->FatCache);
    for (WindowIndex = FatCache->DirtyStart;
         WindowIndex < FatCache->DirtyEnd;
         WindowIndex += 1) {

        if (FatCache->Dirty[WindowIndex].Min >
            FatCache->Dirty[WindowIndex].Max) {

            continue;
        }

        //
        // This is metadata, so don't write it synchronized unless the
        // caller wants metadata flushed.
        //

        if ((IoFlags & IO_FLAG_METADATA_SYNCHRONIZED) == 0) {
            IoFlags &= ~IO_FLAG_DATA_SYNCHRONIZED;
        }

        Status = FatpFatCacheWriteWindow(Volume, IoFlags, WindowIndex);
        if (!KSUCCESS(Status)) {
            TotalStatus = Status;

        } else {
            FatCache->Dirty[WindowIndex].Min = FatCache->WindowSize;
            FatCache->Dirty[WindowIndex].Max = 0;
        }
    }

    if (KSUCCESS(TotalStatus)) {
        FatCache->DirtyStart = MAX_ULONG;
        FatCache->DirtyEnd = 0;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
FatpFatCacheReadWindow (
    PFAT_VOLUME Volume,
    BOOL VolumeLockHeld,
    ULONG WindowIndex
    )

/*++

Routine Description:

    This routine reads in a window of the File Allocation Table.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    VolumeLockHeld - Supplies a boolean indicating whether or not the volume
        lock is already held.

    WindowIndex - Supplies the index of the FAT window that is to be read.

Return Value:

    Status code.

--*/

{

    ULONGLONG BlockAddress;
    ULONGLONG BlockCount;
    ULONG BlockShift;
    PFAT_IO_BUFFER FatIoBuffer;
    KSTATUS Status;
    PVOID Window;
    ULONGLONG WindowByteOffset;
    ULONG WindowSize;

    BlockShift = Volume->BlockShift;
    FatIoBuffer = NULL;
    WindowSize = Volume->FatCache.WindowSize;
    WindowByteOffset = Volume->FatByteStart + (WindowIndex * WindowSize);
    BlockAddress = WindowByteOffset >> BlockShift;

    ASSERT(IS_ALIGNED(WindowSize, Volume->Device.BlockSize));

    FatIoBuffer = FatAllocateIoBuffer(Volume->Device.DeviceToken, WindowSize);
    if (FatIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto FatCacheReadWindowEnd;
    }

    BlockCount = WindowSize >> BlockShift;
    Status = FatReadDevice(Volume->Device.DeviceToken,
                           BlockAddress,
                           BlockCount,
                           IO_FLAG_FS_DATA | IO_FLAG_FS_METADATA,
                           NULL,
                           FatIoBuffer);

    if (!KSUCCESS(Status)) {
        goto FatCacheReadWindowEnd;
    }

    //
    // Map the I/O buffer so it is ready to go once it is in place.
    //

    Window = FatMapIoBuffer(FatIoBuffer);
    if (Window == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto FatCacheReadWindowEnd;
    }

    //
    // With this window of the FAT read, try to add it to the cache. If
    // something else beats this to it, then release the buffer.
    //

    if (VolumeLockHeld == FALSE) {
        FatAcquireLock(Volume->Lock);
    }

    if (Volume->FatCache.WindowBuffers[WindowIndex] == NULL) {

        ASSERT(Volume->FatCache.Windows[WindowIndex] == NULL);

        Volume->FatCache.WindowBuffers[WindowIndex] = FatIoBuffer;
        Volume->FatCache.Windows[WindowIndex] = Window;
        FatIoBuffer = NULL;
    }

    if (VolumeLockHeld == FALSE) {
        FatReleaseLock(Volume->Lock);
    }

    Status = STATUS_SUCCESS;

FatCacheReadWindowEnd:
    if (FatIoBuffer != NULL) {
        FatFreeIoBuffer(FatIoBuffer);
    }

    return Status;
}

KSTATUS
FatpFatCacheWriteWindow (
    PFAT_VOLUME Volume,
    ULONG IoFlags,
    ULONG WindowIndex
    )

/*++

Routine Description:

    This routine writes out a window of the File Allocation Table.

Arguments:

    Volume - Supplies a pointer to the FAT volume structure.

    IoFlags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    WindowIndex - Supplies the index of the FAT window that is to be written.

Return Value:

    Status code.

--*/

{

    ULONGLONG BlockAddress;
    UINTN BlockCount;
    ULONG BlockShift;
    PFAT_WINDOW_DIRTY_REGION Dirty;
    ULONGLONG DirtyEnd;
    ULONGLONG DirtyStart;
    PFAT_CACHE FatCache;
    ULONG FatIndex;
    PFAT_IO_BUFFER FatIoBuffer;
    ULONGLONG FatStart;
    KSTATUS Status;

    BlockShift = Volume->BlockShift;
    FatIoBuffer = Volume->FatCache.WindowBuffers[WindowIndex];

    ASSERT(FatIoBuffer != NULL);

    FatCache = &(Volume->FatCache);
    Dirty = &(FatCache->Dirty[WindowIndex]);

    ASSERT((Dirty->Min < Dirty->Max) && (Dirty->Max <= FatCache->WindowSize));

    IoFlags |= IO_FLAG_FS_DATA | IO_FLAG_FS_METADATA;
    DirtyStart = ((WindowIndex << FatCache->WindowShift) + Dirty->Min) >>
                 BlockShift;

    DirtyEnd = (WindowIndex << FatCache->WindowShift) + Dirty->Max;
    DirtyEnd = ALIGN_RANGE_UP(DirtyEnd, Volume->Device.BlockSize);

    ASSERT(DirtyEnd <= Volume->FatSize);

    DirtyEnd >>= BlockShift;
    BlockCount = DirtyEnd - DirtyStart;

    ASSERT(BlockCount != 0);

    FatIoBufferSetOffset(FatIoBuffer, (Dirty->Min >> BlockShift) << BlockShift);

    //
    // Write the result out to each FAT.
    //

    Status = STATUS_VOLUME_CORRUPT;
    for (FatIndex = 0; FatIndex < Volume->FatCount; FatIndex += 1) {
        FatStart = Volume->FatByteStart + (FatIndex * Volume->FatSize);

        ASSERT(IS_ALIGNED(FatStart, Volume->Device.BlockSize));

        BlockAddress = (FatStart >> BlockShift) + DirtyStart;
        Status = FatWriteDevice(Volume->Device.DeviceToken,
                                BlockAddress,
                                BlockCount,
                                IoFlags,
                                NULL,
                                FatIoBuffer);

        if (!KSUCCESS(Status)) {
            goto FatCacheReadWindowEnd;
        }
    }

FatCacheReadWindowEnd:
    return Status;
}

