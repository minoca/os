/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kpools.c

Abstract:

    This module implements kernel pool API.

Author:

    Evan Green 1-Aug-2012

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

#define MINIMUM_POOL_EXPANSION_PAGES 0x10

//
// Define the initial non-paged pool size needed to successfully bootstrap the
// system. This is required because as other processors perform their early
// initialization they must not cause pool expansion.
//

#define INITIAL_NON_PAGED_POOL_SIZE (512 * 1024)

//
// Define the number of default-sized kernel stacks to keep around.
//

#define KERNEL_STACK_CACHE_SIZE 10

//
// Do not collect pool tag statistics on non-debug builds.
//

#if DEBUG

#define DEFAULT_NON_PAGED_POOL_MEMORY_HEAP_FLAGS \
    (MEMORY_HEAP_FLAG_COLLECT_TAG_STATISTICS | \
     MEMORY_HEAP_FLAG_NO_PARTIAL_FREES)

#define DEFAULT_PAGED_POOL_MEMORY_HEAP_FLAGS \
    (MEMORY_HEAP_FLAG_COLLECT_TAG_STATISTICS | \
     MEMORY_HEAP_FLAG_NO_PARTIAL_FREES)

#else

#define DEFAULT_NON_PAGED_POOL_MEMORY_HEAP_FLAGS \
    (MEMORY_HEAP_FLAG_NO_PARTIAL_FREES)

#define DEFAULT_PAGED_POOL_MEMORY_HEAP_FLAGS \
    (MEMORY_HEAP_FLAG_NO_PARTIAL_FREES)

#endif

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
MmpExpandNonPagedPool (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

BOOL
MmpContractNonPagedPool (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    );

PVOID
MmpExpandPagedPool (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

BOOL
MmpContractPagedPool (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    );

VOID
MmpHandlePoolCorruption (
    PMEMORY_HEAP Heap,
    HEAP_CORRUPTION_CODE Code,
    PVOID Parameter
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// The lock order between these two locks is that the paged pool lock must be
// acquired first if they ever both need to be acquired.
//

MEMORY_HEAP MmNonPagedPool;
KSPIN_LOCK MmNonPagedPoolLock;
RUNLEVEL MmNonPagedPoolOldRunLevel;
MEMORY_HEAP MmPagedPool;
PQUEUED_LOCK MmPagedPoolLock = NULL;

//
// Keep a little cache of kernel stacks to avoid the constant mapping and
// unmapping associated with thread creation.
// TODO: Maintain the kernel stack cache so it doesn't just keep growing.
//

KSPIN_LOCK MmFreeKernelStackLock;
LIST_ENTRY MmFreeKernelStackList;
ULONG MmFreeKernelStackCount;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PVOID
MmAllocatePool (
    POOL_TYPE PoolType,
    UINTN Size,
    ULONG Tag
    )

/*++

Routine Description:

    This routine allocates memory from a kernel pool.

Arguments:

    PoolType - Supplies the type of pool to allocate from. Valid choices are:

        PoolTypeNonPaged - This type of memory will never be paged out. It is a
        scarce resource, and should only be allocated if paged pool is not
        an option. This memory is marked no-execute.

        PoolTypePaged - This is normal memory that may be transparently paged if
        memory gets tight. The caller may not touch paged pool at run-levels at
        or above dispatch, and is not suitable for DMA (as its physical address
        may change unexpectedly.) This pool type should be used for most normal
        allocations. This memory is marked no-execute.

    Size - Supplies the size of the allocation, in bytes.

    Tag - Supplies an identifier to associate with the allocation, useful for
        debugging and leak detection.

Return Value:

    Returns the allocated memory if successful, or NULL on failure.

--*/

{

    PVOID Allocation;
    RUNLEVEL OldRunLevel;

    ASSERT((Size != 0) && (Tag != 0) && (Tag != 0xFFFFFFFF));

    if (PoolType == PoolTypeNonPaged) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&MmNonPagedPoolLock);
        MmNonPagedPoolOldRunLevel = OldRunLevel;
        Allocation = RtlHeapAllocate(&MmNonPagedPool, Size, Tag);
        KeReleaseSpinLock(&MmNonPagedPoolLock);
        KeLowerRunLevel(OldRunLevel);

    } else if (PoolType == PoolTypePaged) {

        ASSERT(KeGetRunLevel() == RunLevelLow);

        if (MmPagedPoolLock != NULL) {
            KeAcquireQueuedLock(MmPagedPoolLock);
        }

        Allocation = RtlHeapAllocate(&MmPagedPool, Size, Tag);
        if (MmPagedPoolLock != NULL) {
            KeReleaseQueuedLock(MmPagedPoolLock);
        }

    } else {
        RtlDebugPrint("Unsupported pool type %d.\n", PoolType);
        Allocation = NULL;
    }

    return Allocation;
}

KERNEL_API
PVOID
MmReallocatePool (
    POOL_TYPE PoolType,
    PVOID Memory,
    UINTN NewSize,
    UINTN AllocationTag
    )

/*++

Routine Description:

    This routine resizes the given allocation, potentially creating a new
    buffer and copying the old contents in.

Arguments:

    PoolType - Supplies the type of pool the memory was allocated from. This
        must agree with the type of pool the allocation originated from, or
        the system will become unstable.

    Memory - Supplies the original active allocation. If this parameter is
        NULL, this routine will simply allocate memory.

    NewSize - Supplies the new required size of the allocation. If this is
        0, then the original allocation will simply be freed.

    AllocationTag - Supplies an identifier for this allocation.

Return Value:

    Returns a pointer to a buffer with the new size (and original contents) on
    success. This may be a new buffer or the same one.

    NULL on failure or if the new size supplied was zero.

--*/

{

    RUNLEVEL OldRunLevel;

    if (PoolType == PoolTypeNonPaged) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&MmNonPagedPoolLock);
        Memory = RtlHeapReallocate(&MmNonPagedPool,
                                   Memory,
                                   NewSize,
                                   AllocationTag);

        KeReleaseSpinLock(&MmNonPagedPoolLock);
        KeLowerRunLevel(OldRunLevel);

    } else if (PoolType == PoolTypePaged) {

        ASSERT(KeGetRunLevel() == RunLevelLow);

        if (MmPagedPoolLock != NULL) {
            KeAcquireQueuedLock(MmPagedPoolLock);
        }

        Memory = RtlHeapReallocate(&MmPagedPool,
                                   Memory,
                                   NewSize,
                                   AllocationTag);

        if (MmPagedPoolLock != NULL) {
            KeReleaseQueuedLock(MmPagedPoolLock);
        }

    } else {

        ASSERT(FALSE);

        Memory = NULL;
    }

    return Memory;
}

KERNEL_API
VOID
MmFreePool (
    POOL_TYPE PoolType,
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees memory allocated from a kernel pool.

Arguments:

    PoolType - Supplies the type of pool the memory was allocated from. This
        must agree with the type of pool the allocation originated from, or
        the system will become unstable.

    Allocation - Supplies a pointer to the allocation to free. This pointer
        may not be referenced after this function completes.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;

    if (PoolType == PoolTypeNonPaged) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&MmNonPagedPoolLock);
        RtlHeapFree(&MmNonPagedPool, Allocation);
        KeReleaseSpinLock(&MmNonPagedPoolLock);
        KeLowerRunLevel(OldRunLevel);

    } else if (PoolType == PoolTypePaged) {

        ASSERT(KeGetRunLevel() == RunLevelLow);

        if (MmPagedPoolLock != NULL) {
            KeAcquireQueuedLock(MmPagedPoolLock);
        }

        RtlHeapFree(&MmPagedPool, Allocation);
        if (MmPagedPoolLock != NULL) {
            KeReleaseQueuedLock(MmPagedPoolLock);
        }

    } else {

        //
        // The caller should not be freeing an unknown pool type since no
        // allocations were ever handed out of an unknown pool type.
        //

        ASSERT(Allocation == NULL);
    }

    return;
}

KSTATUS
MmGetPoolProfilerStatistics (
    PVOID *Buffer,
    PULONG BufferSize,
    ULONG Tag
    )

/*++

Routine Description:

    This routine allocates a buffer and fills it with the pool statistics.

Arguments:

    Buffer - Supplies a pointer that receives a buffer full of pool statistics.

    BufferSize - Supplies a pointer that receives the size of the buffer, in
        bytes.

    Tag - Supplies an identifier to associate with the allocation, useful for
        debugging and leak detection.

Return Value:

    Status code.

--*/

{

    PVOID NonPagedPoolBuffer;
    BOOL NonPagedPoolLockHeld;
    ULONG NonPagedPoolSize;
    RUNLEVEL OldRunLevel;
    PVOID PagedPoolBuffer;
    BOOL PagedPoolLockHeld;
    ULONG PagedPoolSize;
    PPROFILER_MEMORY_POOL ProfilerMemoryPool;
    KSTATUS Status;
    ULONGLONG TagCount;
    PVOID TotalBuffer;
    ULONG TotalSize;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    NonPagedPoolBuffer = NULL;
    PagedPoolBuffer = NULL;
    PagedPoolLockHeld = FALSE;
    TotalBuffer = NULL;

    //
    // Lock non-paged pool in order to collect the current statistics.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&MmNonPagedPoolLock);
    NonPagedPoolLockHeld = TRUE;

    //
    // Determine the size of the non-paged pool statistics, which is based on
    // the number of unique tags, and then allocate a buffer to hold the
    // statistics. Note that the usual non-paged pool allocation API has to be
    // skipped here.
    //

    TagCount = MmNonPagedPool.TagStatistics.TagCount;
    NonPagedPoolSize = sizeof(PROFILER_MEMORY_POOL);
    NonPagedPoolSize += (TagCount * sizeof(PROFILER_MEMORY_POOL_TAG_STATISTIC));
    NonPagedPoolBuffer = RtlHeapAllocate(&MmNonPagedPool,
                                         NonPagedPoolSize,
                                         Tag);

    if (NonPagedPoolBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetPoolStatisticsEnd;
    }

    //
    // Make sure the tag count did not change. The module calling this should
    // have at least have some memory allocated before calling this routine.
    // This assumption is easily removed with a retry.
    //

    ASSERT(TagCount == MmNonPagedPool.TagStatistics.TagCount);

    //
    // Collect the statistics.
    //

    RtlHeapProfilerGetStatistics(&MmNonPagedPool,
                                 NonPagedPoolBuffer,
                                 NonPagedPoolSize);

    KeReleaseSpinLock(&MmNonPagedPoolLock);
    KeLowerRunLevel(OldRunLevel);
    NonPagedPoolLockHeld = FALSE;
    ProfilerMemoryPool = NonPagedPoolBuffer;
    ProfilerMemoryPool->ProfilerMemoryType = ProfilerMemoryTypeNonPagedPool;

    //
    // Lock paged pool in order to collect the current statistics.
    //

    if (MmPagedPoolLock != NULL) {
        KeAcquireQueuedLock(MmPagedPoolLock);
        PagedPoolLockHeld = TRUE;
    }

    //
    // Determine the size of the paged pool statistics, which is based on the
    // number of unique tags, and then allocate a buffer to hold the statistics.
    //

    TagCount = MmPagedPool.TagStatistics.TagCount;
    PagedPoolSize = sizeof(PROFILER_MEMORY_POOL);
    PagedPoolSize += (TagCount * sizeof(PROFILER_MEMORY_POOL_TAG_STATISTIC));
    PagedPoolBuffer = MmAllocateNonPagedPool(PagedPoolSize, Tag);
    if (PagedPoolBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetPoolStatisticsEnd;
    }

    //
    // Make sure the tag count did not change. The module calling this should
    // have at least have some memory allocated before calling this routine.
    // This assumption is easily removed with a retry.
    //

    ASSERT(TagCount == MmPagedPool.TagStatistics.TagCount);

    //
    // Collect the statistics.
    //

    RtlHeapProfilerGetStatistics(&MmPagedPool, PagedPoolBuffer, PagedPoolSize);
    if (MmPagedPoolLock != NULL) {
        KeReleaseQueuedLock(MmPagedPoolLock);
        PagedPoolLockHeld = FALSE;
    }

    ProfilerMemoryPool = PagedPoolBuffer;
    ProfilerMemoryPool->ProfilerMemoryType = ProfilerMemoryTypePagedPool;

    //
    // Allocate a new buffer for the merged statistics. The buffers could be
    // allocated together, but this minimizes the amount of time the pool
    // locks are held to keep the profiler out of the way.
    //

    TotalSize = NonPagedPoolSize + PagedPoolSize;
    TotalBuffer = MmAllocateNonPagedPool(TotalSize, Tag);
    if (TotalBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetPoolStatisticsEnd;
    }

    RtlCopyMemory(TotalBuffer, NonPagedPoolBuffer, NonPagedPoolSize);
    RtlCopyMemory((PBYTE)TotalBuffer + NonPagedPoolSize,
                  PagedPoolBuffer,
                  PagedPoolSize);

    //
    // Free the temporary per-pool buffers and return the combined buffer.
    //

    MmFreeNonPagedPool(NonPagedPoolBuffer);
    MmFreeNonPagedPool(PagedPoolBuffer);
    *Buffer = TotalBuffer;
    *BufferSize = TotalSize;
    Status = STATUS_SUCCESS;

GetPoolStatisticsEnd:
    if (!KSUCCESS(Status)) {
        if (NonPagedPoolLockHeld != FALSE) {
            KeReleaseSpinLock(&MmNonPagedPoolLock);
            KeLowerRunLevel(OldRunLevel);
        }

        if (PagedPoolLockHeld != FALSE) {

            ASSERT(MmPagedPoolLock != NULL);

            KeReleaseQueuedLock(MmPagedPoolLock);
        }

        if (NonPagedPoolBuffer != NULL) {
            MmFreeNonPagedPool(NonPagedPoolBuffer);
        }

        if (PagedPoolBuffer != NULL) {
            MmFreeNonPagedPool(PagedPoolBuffer);
        }

        if (TotalBuffer != NULL) {
            MmFreeNonPagedPool(TotalBuffer);
        }
    }

    return Status;
}

KERNEL_API
VOID
MmDebugPrintPoolStatistics (
    VOID
    )

/*++

Routine Description:

    This routine prints pool statistics to the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&MmNonPagedPoolLock);
    RtlDebugPrint("Non-Paged Pool:\n");
    RtlHeapDebugPrintStatistics(&MmNonPagedPool);
    KeReleaseSpinLock(&MmNonPagedPoolLock);
    KeLowerRunLevel(OldRunLevel);
    if (MmPagedPoolLock != NULL) {
        KeAcquireQueuedLock(MmPagedPoolLock);
    }

    RtlDebugPrint("\nPaged Pool:\n");
    RtlHeapDebugPrintStatistics(&MmPagedPool);
    if (MmPagedPoolLock != NULL) {
        KeReleaseQueuedLock(MmPagedPoolLock);
    }

    return;
}

KSTATUS
MmGetMemoryStatistics (
    PMM_STATISTICS Statistics
    )

/*++

Routine Description:

    This routine collects general memory statistics about the system as a whole.
    This routine must be called at low level.

Arguments:

    Statistics - Supplies a pointer where the statistics will be returned on
        success. The caller should zero this buffer beforehand and set the
        version member to MM_STATISTICS_VERSION. Failure to zero the structure
        beforehand may result in uninitialized data when a driver built for a
        newer OS is run on an older OS.

Return Value:

    Status code.

--*/

{

    RUNLEVEL OldRunLevel;

    if (Statistics->Version < MM_STATISTICS_VERSION) {
        return STATUS_VERSION_MISMATCH;
    }

    Statistics->PageSize = MmPageSize();
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);

    ASSERT(OldRunLevel == RunLevelLow);

    KeAcquireSpinLock(&MmNonPagedPoolLock);
    RtlCopyMemory(&(Statistics->NonPagedPool),
                  &(MmNonPagedPool.Statistics),
                  sizeof(MEMORY_HEAP_STATISTICS));

    KeReleaseSpinLock(&MmNonPagedPoolLock);
    KeLowerRunLevel(OldRunLevel);
    KeAcquireQueuedLock(MmPagedPoolLock);
    RtlCopyMemory(&(Statistics->PagedPool),
                  &(MmPagedPool.Statistics),
                  sizeof(MEMORY_HEAP_STATISTICS));

    KeReleaseQueuedLock(MmPagedPoolLock);
    MmpGetPhysicalPageStatistics(Statistics);
    return STATUS_SUCCESS;
}

PVOID
MmAllocateKernelStack (
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates memory to be used as a kernel stack.

Arguments:

    Size - Supplies the size of the kernel stack to allocate, in bytes.

Return Value:

    Returns a pointer to the base of the stack on success, or NULL on failure.

--*/

{

    UINTN Alignment;
    PLIST_ENTRY Entry;
    RUNLEVEL OldRunLevel;
    ULONG PageSize;
    BOOL RangeAllocated;
    PVOID Stack;
    KSTATUS Status;
    ULONG UnmapFlags;
    VM_ALLOCATION_PARAMETERS VaRequest;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    VaRequest.Address = NULL;
    PageSize = MmPageSize();
    RangeAllocated = FALSE;
    Stack = NULL;
    Size = ALIGN_RANGE_UP(Size, PageSize);

    //
    // If the stack size requested is the default (it always is), then look in
    // the cache for a previously allocated kernel stack.
    //

    if (Size == DEFAULT_KERNEL_STACK_SIZE) {
        Alignment = DEFAULT_KERNEL_STACK_SIZE_ALIGNMENT;
        if (MmFreeKernelStackCount != 0) {
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            KeAcquireSpinLock(&MmFreeKernelStackLock);
            if (!LIST_EMPTY(&MmFreeKernelStackList)) {

                ASSERT(MmFreeKernelStackCount != 0);

                MmFreeKernelStackCount -= 1;
                Entry = MmFreeKernelStackList.Next;
                LIST_REMOVE(Entry);
                VaRequest.Address = Entry;
            }

            KeReleaseSpinLock(&MmFreeKernelStackLock);
            KeLowerRunLevel(OldRunLevel);
        }

    //
    // The alignment is the size (rounded up to the next power of 2) to ensure
    // that kernel stacks don't span page directory entries, which would cause
    // trouble for the context swap code that probes the stack before switching.
    //

    } else {
        if (POWER_OF_2(Size)) {
            Alignment = Size;

        } else {
            Alignment = 1L << ((sizeof(UINTN) * BITS_PER_BYTE) - 1 -
                               RtlCountLeadingZeros(Size));
        }
    }

    if (VaRequest.Address == NULL) {

        //
        // Allocate space, plus an extra guard page.
        //

        VaRequest.Size = Size + PageSize;
        VaRequest.Alignment = Alignment;
        VaRequest.Min = 0;
        VaRequest.Max = MAX_ADDRESS;
        VaRequest.MemoryType = MemoryTypeReserved;
        VaRequest.Strategy = AllocationStrategyAnyAddress;
        Status = MmpAllocateAddressRange(&MmKernelVirtualSpace,
                                         &VaRequest,
                                         FALSE);

        if (!KSUCCESS(Status)) {
            goto AllocateKernelStackEnd;
        }

        RangeAllocated = TRUE;

        //
        // Map everything but the guard page.
        //

        Stack = VaRequest.Address + PageSize;
        Status = MmpMapRange(Stack, Size, PageSize, PageSize, FALSE, FALSE);
        if (!KSUCCESS(Status)) {
            goto AllocateKernelStackEnd;
        }

    } else {
        Stack = VaRequest.Address;
    }

    Status = STATUS_SUCCESS;

AllocateKernelStackEnd:
    if (!KSUCCESS(Status)) {
        if ((Stack != NULL) && (RangeAllocated != FALSE)) {
            UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                         UNMAP_FLAG_SEND_INVALIDATE_IPI;

            MmpFreeAccountingRange(NULL,
                                   Stack - PageSize,
                                   Size + PageSize,
                                   FALSE,
                                   UnmapFlags);

            Stack = NULL;
        }
    }

    return Stack;
}

VOID
MmFreeKernelStack (
    PVOID StackBase,
    UINTN Size
    )

/*++

Routine Description:

    This routine frees a kernel stack.

Arguments:

    StackBase - Supplies the base of the stack (the lowest address in the
        allocation).

    Size - Supplies the number of bytes allocated for the stack.

Return Value:

    None.

--*/

{

    PLIST_ENTRY Entry;
    RUNLEVEL OldRunLevel;
    ULONG PageSize;
    ULONG UnmapFlags;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // If there's room, put the stack back onto the cached list of stacks for
    // the next thread to use. This first check of the count is unprotected
    // by the lock and could be wrong, but it's really just a best effort and
    // avoids doing the heavy lock acquire all the time.
    //

    Entry = NULL;
    if (MmFreeKernelStackCount < KERNEL_STACK_CACHE_SIZE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&MmFreeKernelStackLock);
        if (MmFreeKernelStackCount < KERNEL_STACK_CACHE_SIZE) {
            MmFreeKernelStackCount += 1;
            Entry = StackBase;
            INSERT_AFTER(Entry, &MmFreeKernelStackList);
        }

        KeReleaseSpinLock(&MmFreeKernelStackLock);
        KeLowerRunLevel(OldRunLevel);
        if (Entry != NULL) {
            return;
        }
    }

    //
    // Actually do the work of freeing the stack, the cache is full. Remember
    // that there is a guard page there as well to release.
    //

    PageSize = MmPageSize();
    UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                 UNMAP_FLAG_SEND_INVALIDATE_IPI;

    MmpFreeAccountingRange(NULL,
                           StackBase - PageSize,
                           Size + PageSize,
                           FALSE,
                           UnmapFlags);

    return;
}

KSTATUS
MmpInitializeNonPagedPool (
    VOID
    )

/*++

Routine Description:

    This routine initializes the kernel's nonpaged pool.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PVOID AllocationToCauseExpansion;
    ULONG Flags;
    UINTN MinimumExpansionSize;
    ULONG PageSize;
    KSTATUS Status;

    KeInitializeSpinLock(&MmFreeKernelStackLock);
    INITIALIZE_LIST_HEAD(&MmFreeKernelStackList);

    //
    // Initialize the non-paged pool heap.
    //

    PageSize = MmPageSize();
    MinimumExpansionSize = MINIMUM_POOL_EXPANSION_PAGES * PageSize;
    Flags = DEFAULT_NON_PAGED_POOL_MEMORY_HEAP_FLAGS;
    RtlHeapInitialize(&MmNonPagedPool,
                      MmpExpandNonPagedPool,
                      MmpContractNonPagedPool,
                      MmpHandlePoolCorruption,
                      MinimumExpansionSize,
                      PageSize,
                      0,
                      Flags);

    //
    // Force an initial expansion of the pool to appropriate levels. Use the
    // internal routine so that the expansion does not happen at dispatch level.
    //

    AllocationToCauseExpansion = MmAllocateNonPagedPool(
                                                   INITIAL_NON_PAGED_POOL_SIZE,
                                                   MM_ALLOCATION_TAG);

    if (AllocationToCauseExpansion == NULL) {
        Status = STATUS_NO_MEMORY;
        goto InitializeNonPagedPoolEnd;
    }

    MmFreeNonPagedPool(AllocationToCauseExpansion);
    Status = STATUS_SUCCESS;

InitializeNonPagedPoolEnd:
    return Status;
}

VOID
MmpInitializePagedPool (
    VOID
    )

/*++

Routine Description:

    This routine initializes the kernel's paged pool.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG Flags;
    UINTN MinimumExpansionSize;
    ULONG PageSize;

    PageSize = MmPageSize();
    MinimumExpansionSize = MINIMUM_POOL_EXPANSION_PAGES * PageSize;

    //
    // The paged pool does not support partial frees because image sections
    // cannot be partially freed.
    //

    Flags = DEFAULT_PAGED_POOL_MEMORY_HEAP_FLAGS |
            MEMORY_HEAP_FLAG_NO_PARTIAL_FREES;

    RtlHeapInitialize(&MmPagedPool,
                      MmpExpandPagedPool,
                      MmpContractPagedPool,
                      MmpHandlePoolCorruption,
                      MinimumExpansionSize,
                      PageSize,
                      0,
                      Flags);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
MmpExpandNonPagedPool (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine is called to expand non-paged pool.

Arguments:

    Heap - Supplies a pointer to the heap to allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies a 32-bit tag to associate with this allocation for debugging
        purposes. These are usually four ASCII characters so as to stand out
        when a poor developer is looking at a raw memory dump. It could also be
        a return address.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    BOOL LockHeld;
    RUNLEVEL OldRunLevel;
    ULONG PageSize;
    KSTATUS Status;
    ULONG UnmapFlags;
    VM_ALLOCATION_PARAMETERS VaRequest;

    VaRequest.Address = NULL;
    LockHeld = TRUE;
    PageSize = MmPageSize();

    ASSERT(ALIGN_RANGE_DOWN(Size, PageSize) == Size);

    //
    // Free ranges must be allocated at low level. If the previous runlevel was
    // low then release the lock and lower back down to try the allocation.
    // Several parties might do this, which results in a pool that expanded
    // multiple times. This isn't the end of the world. Ideally a work item
    // to expand the pool would kick off before things get this desperate.
    //

    if (MmNonPagedPoolOldRunLevel != RunLevelLow) {
        Status = STATUS_OPERATION_WOULD_BLOCK;
        goto ExpandNonPagedPoolEnd;
    }

    OldRunLevel = MmNonPagedPoolOldRunLevel;
    KeReleaseSpinLock(&MmNonPagedPoolLock);
    KeLowerRunLevel(OldRunLevel);
    LockHeld = FALSE;
    VaRequest.Size = Size;
    VaRequest.Alignment = PageSize;
    VaRequest.Min = 0;
    VaRequest.Max = MAX_ADDRESS;
    VaRequest.MemoryType = MemoryTypeNonPagedPool;
    VaRequest.Strategy = AllocationStrategyAnyAddress;
    Status = MmpAllocateAddressRange(&MmKernelVirtualSpace, &VaRequest, FALSE);
    if (!KSUCCESS(Status)) {
        goto ExpandNonPagedPoolEnd;
    }

    Status = MmpMapRange(VaRequest.Address,
                         Size,
                         PageSize,
                         PageSize,
                         FALSE,
                         FALSE);

    if (!KSUCCESS(Status)) {
        goto ExpandNonPagedPoolEnd;
    }

ExpandNonPagedPoolEnd:
    if (!KSUCCESS(Status)) {
        if (VaRequest.Address != NULL) {
            UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                         UNMAP_FLAG_SEND_INVALIDATE_IPI;

            MmpFreeAccountingRange(NULL,
                                   VaRequest.Address,
                                   Size,
                                   FALSE,
                                   UnmapFlags);

            VaRequest.Address = NULL;
        }
    }

    if (LockHeld == FALSE) {
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        KeAcquireSpinLock(&MmNonPagedPoolLock);
        MmNonPagedPoolOldRunLevel = OldRunLevel;
    }

    return VaRequest.Address;
}

BOOL
MmpContractNonPagedPool (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    )

/*++

Routine Description:

    This routine is called to contract non-paged pool.

Arguments:

    Heap - Supplies a pointer to the heap the memory was originally allocated
        from.

    Memory - Supplies the allocation returned by the allocation routine.

    Size - Supplies the number of bytes to release.

Return Value:

    TRUE if the memory was successfully freed.

    FALSE if the memory could not be freed at this time.

--*/

{

    RUNLEVEL OldRunLevel;
    UINTN PageSize;
    KSTATUS Status;
    ULONG UnmapFlags;

    PageSize = MmPageSize();

    ASSERT(ALIGN_RANGE_DOWN(Size, PageSize) == Size);

    //
    // Free ranges must be allocated at low level. If the previous runlevel was
    // low then release the lock and lower back down to try the allocation.
    // The heap should have been left in a consistent state before calling this
    // function.
    //

    if (MmNonPagedPoolOldRunLevel != RunLevelLow) {
        Status = STATUS_OPERATION_WOULD_BLOCK;
        goto ContractNonPagedPoolEnd;
    }

    OldRunLevel = MmNonPagedPoolOldRunLevel;
    KeReleaseSpinLock(&MmNonPagedPoolLock);
    KeLowerRunLevel(OldRunLevel);
    UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                 UNMAP_FLAG_SEND_INVALIDATE_IPI;

    Status = MmpFreeAccountingRange(NULL,
                                    Memory,
                                    Size,
                                    FALSE,
                                    UnmapFlags);

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&MmNonPagedPoolLock);
    MmNonPagedPoolOldRunLevel = OldRunLevel;
    if (!KSUCCESS(Status)) {
        goto ContractNonPagedPoolEnd;
    }

ContractNonPagedPoolEnd:
    if (!KSUCCESS(Status)) {
        return FALSE;
    }

    return TRUE;
}

PVOID
MmpExpandPagedPool (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine is called to expand paged pool.

Arguments:

    Heap - Supplies a pointer to the heap to allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies a 32-bit tag to associate with this allocation for debugging
        purposes. These are usually four ASCII characters so as to stand out
        when a poor developer is looking at a raw memory dump. It could also be
        a return address.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    PKPROCESS KernelProcess;
    ULONG PageSize;
    ULONG SectionFlags;
    KSTATUS Status;
    ULONG UnmapFlags;
    VM_ALLOCATION_PARAMETERS VaRequest;

    KernelProcess = PsGetKernelProcess();
    PageSize = MmPageSize();

    ASSERT(ALIGN_RANGE_DOWN(Size, PageSize) == Size);

    VaRequest.Address = NULL;
    VaRequest.Size = Size;
    VaRequest.Alignment = PageSize;
    VaRequest.Min = 0;
    VaRequest.Max = MAX_ADDRESS;
    VaRequest.MemoryType = MemoryTypePagedPool;
    VaRequest.Strategy = AllocationStrategyAnyAddress;
    Status = MmpAllocateAddressRange(&MmKernelVirtualSpace, &VaRequest, FALSE);
    if (!KSUCCESS(Status)) {
        goto ExpandPagedPoolEnd;
    }

    SectionFlags = IMAGE_SECTION_READABLE | IMAGE_SECTION_WRITABLE;
    Status = MmpAddImageSection(KernelProcess->AddressSpace,
                                VaRequest.Address,
                                Size,
                                SectionFlags,
                                INVALID_HANDLE,
                                0);

    if (!KSUCCESS(Status)) {
        goto ExpandPagedPoolEnd;
    }

ExpandPagedPoolEnd:
    if (!KSUCCESS(Status)) {
        if (VaRequest.Address != NULL) {
            UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                         UNMAP_FLAG_SEND_INVALIDATE_IPI;

            MmpFreeAccountingRange(NULL,
                                   VaRequest.Address,
                                   Size,
                                   FALSE,
                                   UnmapFlags);
        }

        return NULL;
    }

    return VaRequest.Address;
}

BOOL
MmpContractPagedPool (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    )

/*++

Routine Description:

    This routine is called to release paged pool resources.

Arguments:

    Heap - Supplies a pointer to the heap the memory was originally allocated
        from.

    Memory - Supplies the allocation returned by the allocation routine.

    Size - Supplies the size of the allocation to free.

Return Value:

    TRUE if the memory was successfully freed.

    FALSE if the memory could not be freed.

--*/

{

    PKPROCESS Process;
    KSTATUS Status;
    ULONG UnmapFlags;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Process = PsGetKernelProcess();
    Status = MmpUnmapImageRegion(Process->AddressSpace, Memory, Size);

    ASSERT(KSUCCESS(Status));

    UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                 UNMAP_FLAG_SEND_INVALIDATE_IPI;

    Status = MmpFreeAccountingRange(NULL,
                                    Memory,
                                    Size,
                                    FALSE,
                                    UnmapFlags);

    ASSERT(KSUCCESS(Status));

    return TRUE;
}

VOID
MmpHandlePoolCorruption (
    PMEMORY_HEAP Heap,
    HEAP_CORRUPTION_CODE Code,
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is called when the heap detects internal corruption.

Arguments:

    Heap - Supplies a pointer to the heap containing the corruption.

    Code - Supplies the code detailing the problem.

    Parameter - Supplies an optional parameter pointing at a problem area.

Return Value:

    None. This routine probably shouldn't return.

--*/

{

    KeCrashSystem(CRASH_POOL_CORRUPTION,
                  (UINTN)Heap,
                  Code,
                  (UINTN)Parameter,
                  0);

    return;
}

