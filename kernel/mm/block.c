/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    block.c

Abstract:

    This module implements support for the MM block allocator, which supports
    private pools of allocations with fixed sizes.

Author:

    Evan Green 14-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the block allocator tag.
//

#define BLOCK_ALLOCATOR_ALLOCATION_TAG 0x6B426D4D // 'kBmM'

//
// Define the headroom for the segment array.
//

#define SEGMENT_ARRAY_HEAD_ROOM 0x10

//
// Defines the divisor use to calculate whether or not a free segment can be
// trimmed from the block allocator. A free segment can be removed if the
// number of free blocks outside the segment is greater than the total blocks
// in the free segment divided by the trim divisor. For example, if the trim
// divisor is 4, there must be more than a quarter of the free segment's total
// block count worth of free blocks in other segments.
//

#define BLOCK_ALLOCATOR_TRIM_DIVISOR 4

//
// Define the all ones value.
//

#define BLOCK_FULL ((UINTN)-1L)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a segment used in a block allocator.

Members:

    VirtualAddress - Stores the virtual address of the allocation segment.

    Bitmap - Stores a pointer to a bitmap indicating whether each block is
        allocated.

    Size - Stores the total segment data size in bytes.

    TotalBlocks - Stores the number of blocks in this segment.

    FreeBlocks - Stores the number of free blocks in this segment.

--*/

typedef struct _BLOCK_ALLOCATOR_SEGMENT {
    PVOID VirtualAddress;
    PUINTN Bitmap;
    UINTN Size;
    UINTN TotalBlocks;
    UINTN FreeBlocks;
} BLOCK_ALLOCATOR_SEGMENT, *PBLOCK_ALLOCATOR_SEGMENT;

/*++

Structure Description:

    This structure stores internal information about a memory management block
    allocator.

Members:

    Flags - Stores a bitfield of flags governing the behavior and properties of
        the block allocator.

    BlockSize - Stores the size of the fixed allocations, in bytes.

    Lock - Stores a pointer to the lock used to serialize access.

    Segments - Stores an array of pointers to segments, sorted by virtual
        address.

    SegmentCount - Stores the number of segments in the segment list. One
        segment is created on each expansion.

    SegmentCapacity - Stores the number of segments that can be put in the
        segment array before the array has to be reallocated.

    SearchStartSegmentIndex - Stores the segment index to start the search in
        for free segments.

    SearchStartBlockIndex - Stores the block index to start the search for free
        segments.

    ExpansionBlockCount - Stores the number of blocks to expand the allocator
        by when expanding it.

    PreviousExpansionBlockCount - Stores the expansion size in blocks the last
        time the block allocator was expanded. This is doubled each time.

    FreeBlocks - Stores the total number of blocks available in all the
        segments of this allocator.

    Alignment - Stores the requested address alignment, in bytes, for each
        allocated block.

    Tag - Stores an identifier to associate with the block allocations, useful
        for debugging and leak detection.

--*/

struct _BLOCK_ALLOCATOR {
    ULONG Flags;
    ULONG BlockSize;
    PQUEUED_LOCK Lock;
    PBLOCK_ALLOCATOR_SEGMENT *Segments;
    UINTN SegmentCount;
    UINTN SegmentCapacity;
    UINTN SearchStartSegmentIndex;
    UINTN SearchStartBlockIndex;
    UINTN ExpansionBlockCount;
    UINTN PreviousExpansionBlockCount;
    UINTN FreeBlocks;
    ULONG Alignment;
    ULONG Tag;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
MmpExpandBlockAllocator (
    PBLOCK_ALLOCATOR Allocator,
    BOOL AllocatorLockHeld
    );

KSTATUS
MmpExpandBlockAllocatorBySize (
    PBLOCK_ALLOCATOR Allocator,
    BOOL AllocatorLockHeld,
    UINTN ExpansionBlockCount
    );

VOID
MmpDestroyBlockAllocatorSegment (
    PBLOCK_ALLOCATOR Allocator,
    PBLOCK_ALLOCATOR_SEGMENT Segment
    );

UINTN
MmpBlockAllocatorFindSegment (
    PBLOCK_ALLOCATOR Allocator,
    PVOID Address
    );

KSTATUS
MmpBlockAllocatorInsertSegment (
    PBLOCK_ALLOCATOR Allocator,
    PBLOCK_ALLOCATOR_SEGMENT NewSegment
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PBLOCK_ALLOCATOR
MmCreateBlockAllocator (
    ULONG BlockSize,
    ULONG Alignment,
    ULONG ExpansionCount,
    ULONG Flags,
    ULONG Tag
    )

/*++

Routine Description:

    This routine creates a memory block allocator. This routine must be called
    at low level.

Arguments:

    BlockSize - Supplies the size of allocations that this block allocator
        doles out.

    Alignment - Supplies the required address alignment, in bytes, for each
        allocation. Valid values are powers of 2. Set to 1 or 0 to specify no
        alignment requirement.

    ExpansionCount - Supplies the number of blocks to expand the pool by when
        out of free blocks.

    Flags - Supplies a bitfield of flags governing the creation and behavior of
        the block allocator. See BLOCK_ALLOCATOR_FLAG_* definitions.

    Tag - Supplies an identifier to associate with the block allocations,
        useful for debugging and leak detection.

Return Value:

    Supplies an opaque pointer to the block allocator on success.

    NULL on failure.

--*/

{

    ULONG AlignedBlockSize;
    PBLOCK_ALLOCATOR Allocator;
    ULONG NonPagedFlags;
    KSTATUS Status;

    Allocator = NULL;

    //
    // Validate the alignment.
    //

    if (!POWER_OF_2(Alignment)) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateBlockAllocatorEnd;
    }

    if (Alignment == 0) {
        Alignment = 1;
    }

    //
    // Validate that the expansion regions are not too big.
    //

    AlignedBlockSize = ALIGN_RANGE_UP(BlockSize, Alignment);
    if (AlignedBlockSize < BlockSize) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateBlockAllocatorEnd;
    }

    if (ExpansionCount == 0) {
        ExpansionCount = 1;
    }

    if ((ExpansionCount * BlockSize) < BlockSize) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateBlockAllocatorEnd;
    }

    //
    // Allocate and initialize the block allocator structure.
    //

    NonPagedFlags = BLOCK_ALLOCATOR_FLAG_NON_PAGED |
                    BLOCK_ALLOCATOR_FLAG_NON_CACHED |
                    BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;

    if ((Flags & NonPagedFlags) != 0) {
        Allocator = MmAllocateNonPagedPool(sizeof(BLOCK_ALLOCATOR),
                                           BLOCK_ALLOCATOR_ALLOCATION_TAG);

    } else {
        Allocator = MmAllocatePagedPool(sizeof(BLOCK_ALLOCATOR),
                                        BLOCK_ALLOCATOR_ALLOCATION_TAG);
    }

    if (Allocator == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateBlockAllocatorEnd;
    }

    RtlZeroMemory(Allocator, sizeof(BLOCK_ALLOCATOR));
    Allocator->Flags = Flags;
    Allocator->BlockSize = AlignedBlockSize;
    Allocator->Alignment = Alignment;
    Allocator->ExpansionBlockCount = ExpansionCount;
    Allocator->Tag = Tag;
    Allocator->Lock = KeCreateQueuedLock();
    if (Allocator->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateBlockAllocatorEnd;
    }

    //
    // Fill the allocator with the an initial allocation.
    //

    Allocator->Flags &= ~BLOCK_ALLOCATOR_FLAG_NO_EXPANSION;
    Status = MmpExpandBlockAllocator(Allocator, FALSE);
    Allocator->Flags = Flags;
    if (!KSUCCESS(Status)) {
        goto CreateBlockAllocatorEnd;
    }

    Status = STATUS_SUCCESS;

CreateBlockAllocatorEnd:
    if (!KSUCCESS(Status)) {
        if (Allocator != NULL) {
            if (Allocator->Lock != NULL) {
                KeDestroyQueuedLock(Allocator->Lock);
            }

            if ((Allocator->Flags & NonPagedFlags) != 0) {
                MmFreeNonPagedPool(Allocator);

            } else {
                MmFreePagedPool(Allocator);
            }

            Allocator = NULL;
        }
    }

    return Allocator;
}

KERNEL_API
VOID
MmDestroyBlockAllocator (
    PBLOCK_ALLOCATOR Allocator
    )

/*++

Routine Description:

    This routine destroys a block allocator, freeing all of its allocations
    and releasing all memory associated with it.

Arguments:

    Allocator - Supplies a pointer to the allocator to release.

Return Value:

    None.

--*/

{

    ULONG NonPagedFlags;
    PBLOCK_ALLOCATOR_SEGMENT Segment;
    UINTN SegmentIndex;

    for (SegmentIndex = 0;
         SegmentIndex < Allocator->SegmentCount;
         SegmentIndex += 1) {

        Segment = Allocator->Segments[SegmentIndex];
        MmpDestroyBlockAllocatorSegment(Allocator, Segment);
    }

    KeDestroyQueuedLock(Allocator->Lock);
    NonPagedFlags = BLOCK_ALLOCATOR_FLAG_NON_PAGED |
                    BLOCK_ALLOCATOR_FLAG_NON_CACHED |
                    BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;

    if ((Allocator->Flags & NonPagedFlags) != 0) {
        if (Allocator->Segments != NULL) {
            MmFreeNonPagedPool(Allocator->Segments);
        }

        MmFreeNonPagedPool(Allocator);

    } else {
        if (Allocator->Segments != NULL) {
            MmFreePagedPool(Allocator->Segments);
        }

        MmFreePagedPool(Allocator);
    }

    return;
}

KERNEL_API
PVOID
MmAllocateBlock (
    PBLOCK_ALLOCATOR Allocator,
    PPHYSICAL_ADDRESS AllocationPhysicalAddress
    )

/*++

Routine Description:

    This routine attempts to allocate a block from the given block allocator.

Arguments:

    Allocator - Supplies a pointer to the allocator to allocate the block of
        memory from.

    AllocationPhysicalAddress - Supplies an optional pointer where the physical
        address of the allocation will be returned. If this parameter is
        non-null, then the block allocator must have been created with the
        physically contiguous flag. Otherwise blocks are not guaranteed to be
        contiguous, making the starting physical address of a block meaningless.

Return Value:

    Returns an allocation of fixed size (defined when the block allocator was
    created) on success.

    NULL on failure.

--*/

{

    ULONG AlignedBlockSize;
    PVOID Allocation;
    ULONG BlockIndex;
    ULONG BlocksPerInteger;
    ULONG BlocksPerPage;
    ULONG Index;
    ULONG IntegerIndex;
    UINTN Mask;
    ULONG MaxIndex;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG PhysicallyContiguousFlags;
    PBLOCK_ALLOCATOR_SEGMENT Segment;
    UINTN SegmentIndex;
    UINTN SegmentLoopIndex;
    UINTN StartIndex;
    KSTATUS Status;
    UINTN TotalOffset;

    //
    // Fail if the physical address parameter was supplied to a non-physically
    // contiguous block allocator.
    //

    PhysicallyContiguousFlags = BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;
    if ((AllocationPhysicalAddress != NULL) &&
        ((Allocator->Flags & PhysicallyContiguousFlags) == 0)) {

        ASSERT(FALSE);

        return NULL;
    }

    Allocation = NULL;
    Index = 0;
    IntegerIndex = 0;
    KeAcquireQueuedLock(Allocator->Lock);
    while (TRUE) {

        //
        // Loop through all segments looking for free blocks.
        //

        SegmentIndex = Allocator->SearchStartSegmentIndex;
        StartIndex = Allocator->SearchStartBlockIndex;
        for (SegmentLoopIndex = 0;
             SegmentLoopIndex < Allocator->SegmentCount;
             SegmentLoopIndex += 1) {

            if (SegmentIndex >= Allocator->SegmentCount) {
                SegmentIndex = 0;
            }

            Segment = Allocator->Segments[SegmentIndex];
            if (Segment->FreeBlocks == 0) {
                SegmentIndex += 1;
                StartIndex = 0;
                continue;
            }

            //
            // Loop over all integers looking for one with free bits.
            //

            BlocksPerInteger = sizeof(UINTN) * BITS_PER_BYTE;
            MaxIndex = ALIGN_RANGE_UP(Segment->TotalBlocks, BlocksPerInteger) /
                       BlocksPerInteger;

            ASSERT(StartIndex < MaxIndex);

            for (IntegerIndex = StartIndex;
                 IntegerIndex < MaxIndex;
                 IntegerIndex += 1) {

                if (Segment->Bitmap[IntegerIndex] != BLOCK_FULL) {
                    break;
                }
            }

            //
            // Check the beginning of the block too if the scan didn't start
            // at the beginning.
            //

            if ((IntegerIndex == MaxIndex) && (StartIndex != 0)) {
                for (IntegerIndex = 0;
                     IntegerIndex < StartIndex;
                     IntegerIndex += 1) {

                    if (Segment->Bitmap[IntegerIndex] != BLOCK_FULL) {
                        break;
                    }
                }

                ASSERT(IntegerIndex != StartIndex);
            }

            StartIndex = 0;

            ASSERT(IntegerIndex < MaxIndex);
            ASSERT(Segment->Bitmap[IntegerIndex] != BLOCK_FULL);

            //
            // Loop over all bits to find the exact index of the free one. This
            // should not go beyond the last valid bit in the last integer
            // because the free blocks count guarantees that something is free
            // in this block.
            //

            if (Segment->Bitmap[IntegerIndex] == 0) {
                Index = 0;

            } else {
                Index = RtlCountTrailingZeros(~(Segment->Bitmap[IntegerIndex]));
            }

            Mask = 1L << Index;

            ASSERT(Mask != 0);

            //
            // Mark the allocation as taken.
            //

            Segment->Bitmap[IntegerIndex] |= Mask;
            Segment->FreeBlocks -= 1;
            Allocator->FreeBlocks -= 1;
            Allocator->SearchStartSegmentIndex = SegmentIndex;
            Allocator->SearchStartBlockIndex = IntegerIndex;
            Status = STATUS_SUCCESS;
            goto AllocateBlockEnd;
        }

        //
        // Sadly, there is no more free space in the allocator. Attempt to
        // expand it!
        //

        Status = MmpExpandBlockAllocator(Allocator, TRUE);
        if (!KSUCCESS(Status)) {
            break;
        }

        Allocator->SearchStartSegmentIndex = 0;
        Allocator->SearchStartBlockIndex = 0;
    }

    Status = STATUS_INSUFFICIENT_RESOURCES;

AllocateBlockEnd:
    KeReleaseQueuedLock(Allocator->Lock);

    //
    // Upon success, the virtual address and physical address (if requested)
    // can be calculated outside of the lock.
    //

    if (KSUCCESS(Status)) {
        BlockIndex = (IntegerIndex * BITS_PER_BYTE * sizeof(UINTN)) + Index;
        if ((Allocator->Flags & PhysicallyContiguousFlags) != 0) {
            PageSize = MmPageSize();
            if (Allocator->BlockSize >= PageSize) {
                AlignedBlockSize = ALIGN_RANGE_UP(Allocator->BlockSize,
                                                  PageSize);

                TotalOffset = AlignedBlockSize * BlockIndex;

            } else {
                BlocksPerPage = PageSize / Allocator->BlockSize;
                TotalOffset = (BlockIndex / BlocksPerPage) * PageSize;
                TotalOffset += (BlockIndex % BlocksPerPage) *
                               Allocator->BlockSize;
            }

        } else {
            TotalOffset = Allocator->BlockSize * BlockIndex;
        }

        Allocation = Segment->VirtualAddress + TotalOffset;

        ASSERT(IS_ALIGNED((UINTN)Allocation, Allocator->Alignment));
        ASSERT(TotalOffset < Segment->Size);

        //
        // The physical addresses may not be contiguous within a segment. They
        // are only guaranteed to be contiguous within a block. Look up the
        // physical address, if requested.
        //

        if (AllocationPhysicalAddress != NULL) {
            PhysicalAddress = MmpVirtualToPhysical(Allocation, NULL);

            ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);
            ASSERT(IS_ALIGNED(PhysicalAddress, Allocator->Alignment));
            ASSERT((PhysicalAddress + Allocator->BlockSize - 1) ==
                   MmpVirtualToPhysical(
                         (Allocation + Allocator->BlockSize - 1), NULL));

            *AllocationPhysicalAddress = PhysicalAddress;
        }
    }

    return Allocation;
}

KERNEL_API
VOID
MmFreeBlock (
    PBLOCK_ALLOCATOR Allocator,
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees an allocated block back into the block allocator.

Arguments:

    Allocator - Supplies a pointer to the allocator that originally doled out
        the allocation.

    Allocation - Supplies a pointer to the allocation to free.

Return Value:

    None.

--*/

{

    ULONG AlignedBlockSize;
    ULONG BitIndex;
    UINTN BlockIndex;
    ULONG BlocksPerPage;
    UINTN CopySize;
    UINTN IntegerIndex;
    UINTN Offset;
    ULONG PageSize;
    ULONG PhysicallyContiguousFlags;
    PBLOCK_ALLOCATOR_SEGMENT Segment;
    UINTN SegmentIndex;
    PBLOCK_ALLOCATOR_SEGMENT SegmentToDestroy;

    ASSERT(IS_ALIGNED((UINTN)Allocation, Allocator->Alignment) != FALSE);

    //
    // Loop through all segments looking for the segment that owns this
    // allocation.
    //

    SegmentToDestroy = NULL;
    KeAcquireQueuedLock(Allocator->Lock);
    SegmentIndex = MmpBlockAllocatorFindSegment(Allocator, Allocation);
    if (SegmentIndex == BLOCK_FULL) {

        ASSERT(FALSE);

        goto FreeBlockEnd;
    }

    Segment = Allocator->Segments[SegmentIndex];

    //
    // Calculate the block index. This gets tricky because of slack space
    // that might be lurking at the end of a page for physically contiguous
    // blocks.
    //

    Offset = (UINTN)Allocation - (UINTN)Segment->VirtualAddress;
    PhysicallyContiguousFlags = BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;
    if ((Allocator->Flags & PhysicallyContiguousFlags) != 0) {
        PageSize = MmPageSize();
        if (Allocator->BlockSize >= PageSize) {
            AlignedBlockSize = ALIGN_RANGE_UP(Allocator->BlockSize,
                                              PageSize);

            ASSERT((Offset % AlignedBlockSize) == 0);

            BlockIndex = Offset / AlignedBlockSize;

        } else {
            BlocksPerPage = PageSize / Allocator->BlockSize;
            BlockIndex = (Offset / PageSize) * BlocksPerPage;

            ASSERT(((Offset % PageSize) % Allocator->BlockSize) == 0);

            BlockIndex += (Offset % PageSize) / Allocator->BlockSize;
        }

    } else {

        ASSERT((Offset % Allocator->BlockSize) == 0);

        BlockIndex = Offset / Allocator->BlockSize;
    }

    IntegerIndex = BlockIndex / (BITS_PER_BYTE * sizeof(UINTN));
    BitIndex = BlockIndex % (BITS_PER_BYTE * sizeof(UINTN));

    ASSERT((Segment->Bitmap[IntegerIndex] & (1L << BitIndex)) != 0);

    Segment->Bitmap[IntegerIndex] &= ~(1L << BitIndex);
    Segment->FreeBlocks += 1;
    Allocator->FreeBlocks += 1;

    ASSERT(Segment->FreeBlocks <= Segment->TotalBlocks);

    //
    // If the allocator is set to trim itself and this segment becomes free
    // and the total number of free blocks outside of this segment is greater
    // than a quarter of the number of blocks in this segment, then remove it.
    //

    if (((Allocator->Flags & BLOCK_ALLOCATOR_FLAG_TRIM) != 0) &&
        (Segment->FreeBlocks == Segment->TotalBlocks) &&
        ((Allocator->FreeBlocks - Segment->FreeBlocks) >
         (Segment->TotalBlocks / BLOCK_ALLOCATOR_TRIM_DIVISOR))) {

        CopySize = (Allocator->SegmentCount - (SegmentIndex + 1)) *
                   sizeof(PVOID);

        if (CopySize != 0) {
            RtlCopyMemory(&(Allocator->Segments[SegmentIndex]),
                          &(Allocator->Segments[SegmentIndex + 1]),
                          CopySize);
        }

        Allocator->SegmentCount -= 1;
        Allocator->FreeBlocks -= Segment->FreeBlocks;
        SegmentToDestroy = Segment;

        //
        // The array is all moved around, so reset the search start position.
        //

        Allocator->SearchStartSegmentIndex = 0;
        Allocator->SearchStartBlockIndex = 0;

        //
        // Shift the previous expansion size down to keep the doubling effect
        // from running away.
        //

        Allocator->PreviousExpansionBlockCount >>= 1;
    }

    //
    // There's a free block in this index, move the search start down if
    // possible.
    //

    if (SegmentIndex < Allocator->SearchStartSegmentIndex) {
        Allocator->SearchStartSegmentIndex = SegmentIndex;
    }

    if (Allocator->SearchStartSegmentIndex == SegmentIndex) {
        if (IntegerIndex < Allocator->SearchStartBlockIndex) {
            Allocator->SearchStartBlockIndex = IntegerIndex;
        }
    }

FreeBlockEnd:
    KeReleaseQueuedLock(Allocator->Lock);

    //
    // If a segment got removed, then release it outside the lock.
    //

    if (SegmentToDestroy != NULL) {

        ASSERT(SegmentToDestroy->FreeBlocks == SegmentToDestroy->TotalBlocks);

        MmpDestroyBlockAllocatorSegment(Allocator, SegmentToDestroy);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
MmpExpandBlockAllocator (
    PBLOCK_ALLOCATOR Allocator,
    BOOL AllocatorLockHeld
    )

/*++

Routine Description:

    This routine expands the allocation capacity of a block allocator. This
    routine assumes the block allocator's lock is already held unless it is
    the first expansion.

Arguments:

    Allocator - Supplies a pointer to the allocator to expand.

    AllocatorLockHeld - Supplies a boolean indicating whether or not the
        allocator's lock is held by the caller.

Return Value:

    Status code.

--*/

{

    UINTN ExpansionSize;
    KSTATUS Status;

    //
    // If the block allocator is prevented from expanding, just fail.
    //

    if ((Allocator->Flags & BLOCK_ALLOCATOR_FLAG_NO_EXPANSION) != 0) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Try doubling the previous expansion. Don't go below the minimum size.
    // Keep dividing by two until either the something is found or the minimum
    // is hit.
    //

    ExpansionSize = Allocator->PreviousExpansionBlockCount << 1;
    if (ExpansionSize < Allocator->ExpansionBlockCount) {
        ExpansionSize = Allocator->ExpansionBlockCount;
    }

    Status = STATUS_INVALID_PARAMETER;
    while (ExpansionSize >= Allocator->ExpansionBlockCount) {
        Status = MmpExpandBlockAllocatorBySize(Allocator,
                                               AllocatorLockHeld,
                                               ExpansionSize);

        if (KSUCCESS(Status)) {
            break;
        }

        ExpansionSize >>= 1;
    }

    Allocator->PreviousExpansionBlockCount = ExpansionSize;
    return Status;
}

KSTATUS
MmpExpandBlockAllocatorBySize (
    PBLOCK_ALLOCATOR Allocator,
    BOOL AllocatorLockHeld,
    UINTN ExpansionBlockCount
    )

/*++

Routine Description:

    This routine expands the allocation capacity of a block allocator. This
    routine assumes the block allocator's lock is already held unless it is
    the first expansion.

Arguments:

    Allocator - Supplies a pointer to the allocator to expand.

    AllocatorLockHeld - Supplies a boolean indicating whether or not the
        allocator's lock is held by the caller.

    ExpansionBlockCount - Supplies the number of blocks by which to expand the
        block allocator's pool of blocks.

Return Value:

    Status code.

--*/

{

    ULONG AlignedBlockSize;
    ULONG AllocationSize;
    ULONG BitmapSize;
    ULONG BlockSize;
    ULONG BlocksPerPage;
    UINTN LastBitmapBit;
    UINTN LastBitmapIndex;
    UINTN LastBitmapMask;
    BOOL LockHeld;
    BOOL NonCached;
    BOOL NonPaged;
    ULONG NonPagedFlags;
    ULONG PageCount;
    ULONG PageSize;
    BOOL PhysicallyContiguous;
    ULONGLONG PhysicalRunSize;
    ULONG RequiredBits;
    PBLOCK_ALLOCATOR_SEGMENT Segment;
    ULONG SegmentSize;
    KSTATUS Status;
    VM_ALLOCATION_PARAMETERS VaRequest;
    PVOID VirtualAddress;

    ASSERT(Allocator->ExpansionBlockCount != 0);

    //
    // Release the lock to perform the actual allocations. It does not matter
    // if the lock is not held during the creation expansion.
    //

    LockHeld = AllocatorLockHeld;
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Allocator->Lock);
        LockHeld = FALSE;
    }

    BlockSize = Allocator->BlockSize;
    NonPagedFlags = BLOCK_ALLOCATOR_FLAG_NON_PAGED |
                    BLOCK_ALLOCATOR_FLAG_NON_CACHED |
                    BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;

    NonPaged = ((Allocator->Flags & NonPagedFlags) != 0);
    NonCached = ((Allocator->Flags & BLOCK_ALLOCATOR_FLAG_NON_CACHED) != 0);
    if ((Allocator->Flags & BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS) != 0) {
        PhysicallyContiguous = TRUE;

    } else {
        PhysicallyContiguous = FALSE;
    }

    PageSize = MmPageSize();
    Segment = NULL;
    SegmentSize = 0;
    PhysicalRunSize = 0;

    //
    // Physically contiguous blocks cannot use pool, so carefully calculate
    // the segment information.
    //

    if (PhysicallyContiguous != FALSE) {

        ASSERT(NonPaged != FALSE);

        //
        // If blocks are greater than or equal to a page in size, then create
        // one virtually contiguous segment that has runs of physically
        // contiguous pages equal to the page-aligned size of the block. For
        // example, if a block is 2.5 pages, then allocate the buffer in runs
        // of 3 physically contiguous pages.
        //

        if (BlockSize >= PageSize) {
            AlignedBlockSize = ALIGN_RANGE_UP(BlockSize, PageSize);
            PhysicalRunSize = AlignedBlockSize;
            SegmentSize = AlignedBlockSize * ExpansionBlockCount;

        //
        // If blocks are smaller than a page, determine how many blocks can fit
        // in a page and how many pages are needed to fulfill the expansion
        // requirement. The individual pages of the buffer do not need to be
        // physically contiguous.
        //

        } else {
            BlocksPerPage = PageSize / BlockSize;
            PageCount = ExpansionBlockCount / BlocksPerPage;
            if ((ExpansionBlockCount % BlocksPerPage) != 0) {
                PageCount += 1;
            }

            PhysicalRunSize = PageSize;
            SegmentSize = PageCount * PageSize;
            ExpansionBlockCount = PageCount * BlocksPerPage;
        }

    //
    // Non-cached blocks cannot be allocated out of pool. These do not need to
    // be physically contiguous, so just allocate the exact size, rounded up
    // to a page.
    //

    } else if (NonCached != FALSE) {

        ASSERT(NonPaged != FALSE);

        PhysicalRunSize = 0;
        SegmentSize = BlockSize * ExpansionBlockCount;
        SegmentSize = ALIGN_RANGE_UP(SegmentSize, PageSize);
        ExpansionBlockCount = SegmentSize / BlockSize;

    //
    // For paged memory or normal non-paged memory, allocate the exact amount
    // of memory.
    //

    } else {
        SegmentSize = BlockSize * ExpansionBlockCount;
    }

    ASSERT(SegmentSize != 0);

    //
    // Prepare the necessary variables to be used during segment creation.
    //

    RequiredBits = ALIGN_RANGE_UP(ExpansionBlockCount, BITS_PER_BYTE);
    BitmapSize = ALIGN_RANGE_UP((RequiredBits / BITS_PER_BYTE), sizeof(UINTN));
    AllocationSize = sizeof(BLOCK_ALLOCATOR_SEGMENT) + BitmapSize;

    ASSERT(AllocationSize > sizeof(BLOCK_ALLOCATOR_SEGMENT));

    //
    // For pool segments, pad the allocation size so that the segments
    // starting virtual address can have the correct alignment. The pool
    // allocators only returns addresses that are 8 byte aligned.
    //

    if ((PhysicallyContiguous == FALSE) && (NonCached == FALSE)) {
        if (Allocator->Alignment > sizeof(ULONGLONG)) {
            AllocationSize += ALIGN_RANGE_UP(BitmapSize, sizeof(ULONGLONG)) +
                              (Allocator->Alignment - sizeof(ULONGLONG));
        }
    }

    //
    // Physically contiguous and non-cached blocks cannot be allocated from
    // pool.
    //

    if ((PhysicallyContiguous != FALSE) || (NonCached != FALSE)) {
        Segment = MmAllocateNonPagedPool(AllocationSize, Allocator->Tag);
        if (Segment == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ExpandBlockAllocatorEnd;
        }

        RtlZeroMemory(Segment, AllocationSize);
        Segment->Bitmap = (PUINTN)(Segment + 1);
        Segment->Size = SegmentSize;
        Segment->TotalBlocks = ExpansionBlockCount;
        Segment->FreeBlocks = Segment->TotalBlocks;
        VaRequest.Address = NULL;
        VaRequest.Size = SegmentSize;
        VaRequest.Alignment = ALIGN_RANGE_UP(Allocator->Alignment, PageSize);
        VaRequest.Min = 0;
        VaRequest.Max = MAX_ADDRESS;
        VaRequest.MemoryType = MemoryTypeReserved;
        VaRequest.Strategy = AllocationStrategyAnyAddress;
        Status = MmpAllocateAddressRange(&MmKernelVirtualSpace,
                                         &VaRequest,
                                         FALSE);

        if (!KSUCCESS(Status)) {
            goto ExpandBlockAllocatorEnd;
        }

        Segment->VirtualAddress = VaRequest.Address;
        Status = MmpMapRange(Segment->VirtualAddress,
                             SegmentSize,
                             VaRequest.Alignment,
                             PhysicalRunSize,
                             FALSE,
                             NonCached);

        if (!KSUCCESS(Status)) {
            goto ExpandBlockAllocatorEnd;
        }

    //
    // Otherwise allocate the segment structure, bitmap, and data out of the
    // appropriate pool.
    //

    } else {
        if (NonPaged != FALSE) {
            Segment = MmAllocateNonPagedPool(AllocationSize + SegmentSize,
                                             Allocator->Tag);

        } else {
            Segment = MmAllocatePagedPool(AllocationSize + SegmentSize,
                                          Allocator->Tag);
        }

        if (Segment == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ExpandBlockAllocatorEnd;
        }

        //
        // Find the aligned starting address for the blocks.
        //

        VirtualAddress = (PVOID)((UINTN)(Segment + 1) + BitmapSize);
        VirtualAddress = (PVOID)(UINTN)ALIGN_RANGE_UP((UINTN)VirtualAddress,
                                                      Allocator->Alignment);

        ASSERT(((UINTN)VirtualAddress + SegmentSize) <=
               ((UINTN)Segment + AllocationSize + SegmentSize));

        RtlZeroMemory(Segment, AllocationSize);
        Segment->Bitmap = (PUINTN)(Segment + 1);
        Segment->Size = SegmentSize;
        Segment->VirtualAddress = VirtualAddress;
        Segment->TotalBlocks = ExpansionBlockCount;
        Segment->FreeBlocks = Segment->TotalBlocks;
    }

    //
    // If the expansion does not evenly fit into the bitmap blocks, mark the
    // remainder as "allocated" to avoid going off the end of the segment.
    //

    if ((ExpansionBlockCount % (BITS_PER_BYTE * sizeof(UINTN))) != 0) {

        ASSERT(BitmapSize >= sizeof(UINTN));

        LastBitmapIndex = (BitmapSize / sizeof(UINTN)) - 1;
        LastBitmapBit = 1L <<
                        (ExpansionBlockCount % (BITS_PER_BYTE * sizeof(UINTN)));

        LastBitmapMask = ~(LastBitmapBit - 1);
        Segment->Bitmap[LastBitmapIndex] = LastBitmapMask;
    }

    //
    // With success on the horizon, reacquire the lock if it was held on entry.
    //

    ASSERT((AllocatorLockHeld != FALSE) || (Allocator->SegmentCount == 0));

    if (AllocatorLockHeld != FALSE) {

        ASSERT(LockHeld == FALSE);

        KeAcquireQueuedLock(Allocator->Lock);
        LockHeld = TRUE;
    }

    //
    // Add the segment to the allocator's list and increment the segment count.
    //

    Status = MmpBlockAllocatorInsertSegment(Allocator, Segment);
    if (!KSUCCESS(Status)) {
        goto ExpandBlockAllocatorEnd;
    }

    Segment = NULL;

ExpandBlockAllocatorEnd:
    if (Segment != NULL) {
        MmpDestroyBlockAllocatorSegment(Allocator, Segment);
    }

    if ((AllocatorLockHeld != FALSE) && (LockHeld == FALSE)) {
        KeAcquireQueuedLock(Allocator->Lock);
    }

    return Status;
}

VOID
MmpDestroyBlockAllocatorSegment (
    PBLOCK_ALLOCATOR Allocator,
    PBLOCK_ALLOCATOR_SEGMENT Segment
    )

/*++

Routine Description:

    This routine destroys a block allocator segment. It is assumed that the
    segment has already been removed from the block allocator segment list.

Arguments:

    Allocator - Supplies a pointer to the block allocator that owned the
        segment.

    Segment - Supplies a pointer to the segment to destroy.

Return Value:

    None.

--*/

{

    ULONG NonPagedFlags;
    ULONG NonPoolFlags;
    KSTATUS Status;
    ULONG UnmapFlags;

    NonPagedFlags = BLOCK_ALLOCATOR_FLAG_NON_PAGED |
                    BLOCK_ALLOCATOR_FLAG_NON_CACHED |
                    BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;

    if ((Allocator->Flags & NonPagedFlags) != 0) {
        NonPoolFlags = BLOCK_ALLOCATOR_FLAG_NON_CACHED |
                       BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;

        if ((Segment->VirtualAddress != NULL) &&
            ((Allocator->Flags & NonPoolFlags) != 0)) {

            UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                         UNMAP_FLAG_SEND_INVALIDATE_IPI;

            Status = MmpFreeAccountingRange(NULL,
                                            Segment->VirtualAddress,
                                            Segment->Size,
                                            FALSE,
                                            UnmapFlags);

            ASSERT(KSUCCESS(Status));
        }

        MmFreeNonPagedPool(Segment);

    } else {
        MmFreePagedPool(Segment);
    }

    return;
}

UINTN
MmpBlockAllocatorFindSegment (
    PBLOCK_ALLOCATOR Allocator,
    PVOID Address
    )

/*++

Routine Description:

    This routine returns the segment containing the given block allocator.

Arguments:

    Allocator - Supplies a pointer to the block allocator that owned the
        segment.

    Address - Supplies the address of the block.

Return Value:

    Returns the segment index of the segment containing the allocation.

    BLOCK_FULL if no segment contains the allocation.

--*/

{

    INTN CompareIndex;
    INTN Distance;
    INTN Maximum;
    INTN Minimum;
    PBLOCK_ALLOCATOR_SEGMENT Segment;

    if (Allocator->SegmentCount == 0) {

        ASSERT(FALSE);

        return BLOCK_FULL;
    }

    //
    // Perform a binary search to find the segment.
    //

    Minimum = 0;
    Maximum = Allocator->SegmentCount;

    //
    // Loop as long as the indices don't cross. The maximum index is exclusive
    // (so 0,1 includes only 0).
    //

    while (Minimum < Maximum) {
        Distance = (Maximum - Minimum) / 2;
        CompareIndex = Minimum + Distance;
        Segment = Allocator->Segments[CompareIndex];
        if ((Address >= Segment->VirtualAddress) &&
            (Address < Segment->VirtualAddress + Segment->Size)) {

            return CompareIndex;

        //
        // If the segment starts after the address, then go lower.
        //

        } else if (Segment->VirtualAddress > Address) {
            Maximum = CompareIndex;

        } else {
            Minimum = CompareIndex + 1;
        }
    }

    ASSERT(FALSE);

    return BLOCK_FULL;
}

KSTATUS
MmpBlockAllocatorInsertSegment (
    PBLOCK_ALLOCATOR Allocator,
    PBLOCK_ALLOCATOR_SEGMENT NewSegment
    )

/*++

Routine Description:

    This routine inserts a block allocator segment in the correct order on the
    allocator segment array. This routine assumes the allocator lock is
    already held.

Arguments:

    Allocator - Supplies a pointer to the block allocator.

    NewSegment - Supplies a pointer to the segment to insert.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on failure.

--*/

{

    UINTN AllocationSize;
    INTN CompareIndex;
    INTN Distance;
    UINTN InsertIndex;
    INTN Maximum;
    INTN Minimum;
    INTN MoveIndex;
    PVOID NewBuffer;
    UINTN NewCapacity;
    PBLOCK_ALLOCATOR_SEGMENT Segment;

    //
    // Reallocate the array if needed.
    //

    ASSERT(Allocator->SegmentCount <= Allocator->SegmentCapacity);

    if (Allocator->SegmentCount == Allocator->SegmentCapacity) {
        NewCapacity = Allocator->SegmentCapacity + SEGMENT_ARRAY_HEAD_ROOM;
        AllocationSize = NewCapacity * sizeof(PVOID);
        if ((Allocator->Flags & BLOCK_ALLOCATOR_FLAG_NON_PAGED) != 0) {
            NewBuffer = MmAllocateNonPagedPool(AllocationSize, Allocator->Tag);
            if (NewBuffer == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            if (Allocator->SegmentCount != 0) {
                RtlCopyMemory(NewBuffer,
                              Allocator->Segments,
                              Allocator->SegmentCount * sizeof(PVOID));

                MmFreeNonPagedPool(Allocator->Segments);
            }

        } else {
            NewBuffer = MmAllocatePagedPool(AllocationSize, Allocator->Tag);
            if (NewBuffer == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            if (Allocator->SegmentCount != 0) {
                RtlCopyMemory(NewBuffer,
                              Allocator->Segments,
                              Allocator->SegmentCount * sizeof(PVOID));

                MmFreePagedPool(Allocator->Segments);
            }
        }

        Allocator->Segments = NewBuffer;
        Allocator->SegmentCapacity = NewCapacity;
    }

    if (Allocator->SegmentCount == 0) {
        Allocator->Segments[0] = NewSegment;

    } else {

        //
        // Find the right spot to insert this segment in. Loop as long as the
        // indices don't cross. The maximum index is exclusive (so 0,1 includes
        // only 0).
        //

        Minimum = 0;
        Maximum = Allocator->SegmentCount;
        CompareIndex = 0;
        while (Minimum < Maximum) {
            Distance = (Maximum - Minimum) / 2;
            CompareIndex = Minimum + Distance;
            Segment = Allocator->Segments[CompareIndex];

            ASSERT(Segment->VirtualAddress != NewSegment->VirtualAddress);

            if (Segment->VirtualAddress < NewSegment->VirtualAddress) {
                Minimum = CompareIndex + 1;

            } else {
                Maximum = CompareIndex;
            }
        }

        InsertIndex = CompareIndex;

        ASSERT(CompareIndex < Allocator->SegmentCount);

        if (Allocator->Segments[CompareIndex]->VirtualAddress <
            NewSegment->VirtualAddress) {

            InsertIndex = CompareIndex + 1;
        }

        ASSERT((InsertIndex == 0) ||
               (Allocator->Segments[InsertIndex - 1]->VirtualAddress <
                NewSegment->VirtualAddress));

        ASSERT((InsertIndex == Allocator->SegmentCount) ||
               (Allocator->Segments[InsertIndex]->VirtualAddress >
                NewSegment->VirtualAddress));

        for (MoveIndex = Allocator->SegmentCount;
             MoveIndex > InsertIndex;
             MoveIndex -= 1) {

            Allocator->Segments[MoveIndex] = Allocator->Segments[MoveIndex - 1];
        }

        Allocator->Segments[InsertIndex] = NewSegment;
    }

    Allocator->SegmentCount += 1;

    //
    // Add the segment's free blocks to the total free block count.
    //

    Allocator->FreeBlocks += NewSegment->FreeBlocks;
    return STATUS_SUCCESS;
}

