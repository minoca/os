/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    physical.c

Abstract:

    This module implements the physical page allocator routines.

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

//
// Define the number of concurrent lock requests that can exist before callers
// start getting rejected.
//

#define MAX_PHYSICAL_PAGE_LOCK_COUNT 15

//
// Define the flags for the physical page array.
//

#define PHYSICAL_PAGE_FLAG_NON_PAGED 0x1

//
// Define the free page value. A physical page is free if its 'free' member is
// set to zero.
//

#define PHYSICAL_PAGE_FREE 0

//
// Define the percentage of physical pages that should remain free.
//

#define MIN_FREE_PHYSICAL_PAGES_PERCENT 5

//
// Define the physical memory percentages for each memory warning level.
//

#define MEMORY_WARNING_LEVEL_1_HIGH_PERCENT 97
#define MEMORY_WARNING_LEVEL_1_LOW_PERCENT 95
#define MEMORY_WARNING_LEVEL_2_HIGH_PERCENT 90
#define MEMORY_WARNING_LEVEL_2_LOW_PERCENT 87

//
// Define the percentage of physical pages to use for the memory warning count
// mask.
//

#define MEMORY_WARNING_COUNT_MASK_PERCENT 1

//
// Define the amount of time to wait in seconds before declaring that the
// system is truly out of memory.
//

#define PHYSICAL_MEMORY_ALLOCATION_TIMEOUT 180

//
// Defines the maximum number of page out failures allowed before the paging
// out of physical pages gives up.
//

#define PHYSICAL_MEMORY_MAX_PAGE_OUT_FAILURE_COUNT 10

//
// Define how many pages must be paged out before the paging event is
// signaled and all threads trying to allocate are re-woken. Too few pages and
// work is wasted as the allocations aren't satisfied. Too many pages and
// threads wait unnecessarily.
//

#define PAGING_EVENT_SIGNAL_PAGE_COUNT 0x10

//
// --------------------------------------------------------------------- Macros
//

#define IS_PHYSICAL_MEMORY_TYPE(_Type)                          \
    (((_Type) == MemoryTypeFree) ||                             \
     ((_Type) == MemoryTypeAcpiTables) ||                       \
     ((_Type) == MemoryTypeLoaderTemporary) ||                  \
     ((_Type) == MemoryTypeLoaderPermanent) ||                  \
     ((_Type) == MemoryTypeFirmwareTemporary) ||                \
     ((_Type) == MemoryTypePageTables) ||                       \
     ((_Type) == MemoryTypeBootPageTables) ||                   \
     ((_Type) == MemoryTypeMmStructures))

#define IS_BOOT_TEMPORARY_MEMORY_TYPE(_Type)                    \
    (((_Type) == MemoryTypeLoaderTemporary) ||                  \
     ((_Type) == MemoryTypeFirmwareTemporary) ||                \
     ((_Type) == MemoryTypeBootPageTables))

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PHYSICAL_MEMORY_SEARCH_TYPE {
    PhysicalMemoryFindInvalid,
    PhysicalMemoryFindFree,
    PhysicalMemoryFindPagable,
    PhysicalMemoryFindIdentityMappable
} PHYSICAL_MEMORY_SEARCH_TYPE, *PPHYSICAL_MEMORY_SEARCH_TYPE;

/*++

Structure Description:

    This structure stores information about one physical page of memory.

Members:

    Free - Stores PHYSICAL_PAGE_FREE if the page is free.

    Flags - Stores a bitmask of flags for the physical page. See
        PHYSICAL_PAGE_FLAG_* for definitions.

    PagingEntry - Stores a pointer to a paging entry.

    PageCacheEntry - Stores a pointer to page cache entry.

--*/

typedef struct _PHYSICAL_PAGE {
    union {
        UINTN Free;
        UINTN Flags;
        PPAGING_ENTRY PagingEntry;
        PPAGE_CACHE_ENTRY PageCacheEntry;
    } U;

} PHYSICAL_PAGE, *PPHYSICAL_PAGE;

/*++

Structure Description:

    This structure stores information about a physical segment of memory.

Members:

    ListEntry - Stores pointers to the next and previous segments, which are in
        no particular order.

    StartAddress - Stores the start address of the segment.

    EndAddress - Stores the end address of the segment.

    FreePages - Stores the number of unallocated pages in the segment.

--*/

typedef struct _PHYSICAL_MEMORY_SEGMENT {
    LIST_ENTRY ListEntry;
    PHYSICAL_ADDRESS StartAddress;
    PHYSICAL_ADDRESS EndAddress;
    UINTN FreePages;
} PHYSICAL_MEMORY_SEGMENT, *PPHYSICAL_MEMORY_SEGMENT;

/*++

Structure Description:

    This structure defines the iteration context when initializing the physical
    page segments.

Members:

    TotalMemoryBytes - Stores the running total of bytes of memory in the
        system.

    TotalSegments - Stores the running total of physical memory segments in
        the system.

    LastEnd - Stores the physical address of the previous segment's end.

    CurrentPage - Stores the current page being worked on.

    CurrentSegment - Stores the currents segment being initialized.

    PagesInitialized - Stores the number of pages that have been initialized.

    TotalMemoryPages - Stores the maximum number of pages to initialize.

--*/

typedef struct _INIT_PHYSICAL_MEMORY_ITERATOR {
    ULONGLONG TotalMemoryBytes;
    UINTN TotalSegments;
    PHYSICAL_ADDRESS LastEnd;
    PPHYSICAL_PAGE CurrentPage;
    PPHYSICAL_MEMORY_SEGMENT CurrentSegment;
    UINTN PagesInitialized;
    UINTN TotalMemoryPages;
} INIT_PHYSICAL_MEMORY_ITERATOR, *PINIT_PHYSICAL_MEMORY_ITERATOR;

//
// ----------------------------------------------- Internal Function Prototypes
//

PPHYSICAL_MEMORY_SEGMENT
MmpFindPhysicalPages (
    UINTN PageCount,
    UINTN PageAlignment,
    PHYSICAL_MEMORY_SEARCH_TYPE SearchType,
    PUINTN SelectedPageOffset,
    PUINTN PagesFound
    );

VOID
MmpInitializePhysicalAllocatorIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

BOOL
MmpUpdatePhysicalMemoryStatistics (
    UINTN PageCount,
    BOOL Allocation
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Artificially limits the number of physical pages available in the system. A
// value of 0 indicates no limit on the number of physical pages.
//

UINTN MmLimitTotalPhysicalPages = 0;

//
// Stores the number of physical pages of memory in the system.
//

UINTN MmTotalPhysicalPages;

//
// Stores the number of allocated pages.
//

UINTN MmTotalAllocatedPhysicalPages;

//
// Stores the minimum number of free physical pages to be maintained by the
// system.
//

UINTN MmMinimumFreePhysicalPages;

//
// Stores the number of non-paged physical pages.
//

UINTN MmNonPagedPhysicalPages;

//
// Store the maximum physical address that can be reached. This should be
// removed when PAE is supported.
//

PHYSICAL_ADDRESS MmMaximumPhysicalAddress = 0x100000000ULL;

//
// Store the last pages allocated, so that in general allocating pages sweeps
// across memory instead of always picking the same pages.
//

PPHYSICAL_MEMORY_SEGMENT MmLastAllocatedSegment;
UINTN MmLastAllocatedSegmentOffset;

//
// Store the last pages paged out, so that in general selecting pages to be
// paged out sweeps across memory instead of always picking the same pages.
//

PPHYSICAL_MEMORY_SEGMENT MmLastPagedSegment;
UINTN MmLastPagedSegmentOffset;

//
// Stores the lock protecting access to physical page data structures.
//

PQUEUED_LOCK MmPhysicalPageLock = NULL;

//
// Store the lowest physical page to use.
//

PHYSICAL_ADDRESS MmLowestPhysicalPage = 0;

//
// Store the list head of the physical page segments.
//

LIST_ENTRY MmPhysicalSegmentListHead;

//
// Stores the event used to signal a physical memory notification when there is
// a significant change in the number of allocated physical pages.
//

PKEVENT MmPhysicalMemoryWarningEvent;

//
// Stores the current physical memory warning level.
//

MEMORY_WARNING_LEVEL MmPhysicalMemoryWarningLevel;

//
// Store the number physical pages for each warning level's threshold.
//

UINTN MmPhysicalMemoryWarningLevel1HighPages;
UINTN MmPhysicalMemoryWarningLevel1LowPages;
UINTN MmPhysicalMemoryWarningLevel2HighPages;
UINTN MmPhysicalMemoryWarningLevel2LowPages;

//
// Store the mask that determines how often physical warning levels are checked.
//

UINTN MmPhysicalMemoryWarningCountMask;

//
// Store counters that track how many times allocate and free are called. It is
// OK for these values to wrap.
//

UINTN MmPhysicalMemoryAllocationCount;
UINTN MmPhysicalMemoryFreeCount;

//
// Store a boolean indicating whether or not physical page zero is available.
//

BOOL MmPhysicalPageZeroAvailable = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PVOID
MmGetPhysicalMemoryWarningEvent (
    VOID
    )

/*++

Routine Description:

    This routine returns the memory manager's physical memory warning event.
    This event is signaled whenever there is a change in physical memory's
    warning level.

Arguments:

    None.

Return Value:

    Returns a pointer to the physical memory warning event.

--*/

{

    ASSERT(MmPhysicalMemoryWarningEvent != NULL);

    return MmPhysicalMemoryWarningEvent;
}

KERNEL_API
MEMORY_WARNING_LEVEL
MmGetPhysicalMemoryWarningLevel (
    VOID
    )

/*++

Routine Description:

    This routine returns the current physical memory warning level.

Arguments:

    None.

Return Value:

    Returns the current physical memory warning level.

--*/

{

    return MmPhysicalMemoryWarningLevel;
}

UINTN
MmGetTotalPhysicalPages (
    VOID
    )

/*++

Routine Description:

    This routine gets the total physical pages in the system.

Arguments:

    None.

Return Value:

    Returns the total number of physical pages present in the system.

--*/

{

    return MmTotalPhysicalPages;
}

UINTN
MmGetTotalFreePhysicalPages (
    VOID
    )

/*++

Routine Description:

    This routine returns the total number of free physical pages in the system.

Arguments:

    None.

Return Value:

    Returns the total number of free physical pages in the system.

--*/

{

    return MmTotalPhysicalPages - MmTotalAllocatedPhysicalPages;
}

VOID
MmFreePhysicalPages (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN PageCount
    )

/*++

Routine Description:

    This routine frees a contiguous run of physical memory pages, making the
    pages available to the system again.

Arguments:

    PhysicalAddress - Supplies the base physical address of the pages to free.

    PageCount - Supplies the number of contiguous physical pages to free.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN Index;
    UINTN Offset;
    ULONG PageShift;
    PPAGING_ENTRY PagingEntry;
    LIST_ENTRY PagingEntryList;
    PPHYSICAL_PAGE PhysicalPage;
    UINTN ReleasedCount;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    BOOL SignalEvent;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    PageShift = MmPageShift();
    PagingEntry = NULL;
    INITIALIZE_LIST_HEAD(&PagingEntryList);
    ReleasedCount = 0;
    SignalEvent = FALSE;
    if (MmPhysicalPageLock != NULL) {
        KeAcquireQueuedLock(MmPhysicalPageLock);
    }

    CurrentEntry = MmPhysicalSegmentListHead.Next;
    while (CurrentEntry != &MmPhysicalSegmentListHead) {
        Segment = LIST_VALUE(CurrentEntry, PHYSICAL_MEMORY_SEGMENT, ListEntry);
        if ((PhysicalAddress < Segment->StartAddress) ||
            (PhysicalAddress >= Segment->EndAddress)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        //
        // Find the first physical page in the run.
        //

        Offset = (PhysicalAddress - Segment->StartAddress) >> PageShift;
        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        PhysicalPage += Offset;

        //
        // Any contiguous memory should be contained in the same memory segment.
        //

        ASSERT((PhysicalAddress + (PageCount << PageShift)) <=
               Segment->EndAddress);

        //
        // Release each page in the contiguous run.
        //

        for (Index = 0; Index < PageCount; Index += 1) {

            ASSERT(PhysicalPage->U.Free != PHYSICAL_PAGE_FREE);

            //
            // Directly mark non-paged physical pages as free.
            //

            if ((PhysicalPage->U.Flags & PHYSICAL_PAGE_FLAG_NON_PAGED) != 0) {
                PhysicalPage->U.Free = PHYSICAL_PAGE_FREE;
                MmNonPagedPhysicalPages -= 1;
                ReleasedCount += 1;

            //
            // For physical pages that might be paged, check the paging entry
            // for the paging out flag. If the flag is set then the paging
            // process now owns the page and will release it when appropriate.
            //

            } else {
                PagingEntry = PhysicalPage->U.PagingEntry;

                ASSERT(KeIsQueuedLockHeld(PagingEntry->Section->Lock) != FALSE);

                if ((PagingEntry->U.Flags &
                     PAGING_ENTRY_FLAG_PAGING_OUT) == 0) {

                    if (PagingEntry->U.LockCount == 0) {
                        PhysicalPage->U.Free = PHYSICAL_PAGE_FREE;
                        ReleasedCount += 1;
                        INSERT_BEFORE(&(PagingEntry->U.ListEntry),
                                      &PagingEntryList);

                    } else {
                        PagingEntry->U.Flags |= PAGING_ENTRY_FLAG_FREED;
                    }
                }
            }

            PhysicalPage += 1;
        }

        //
        // If any pages were set free, then update the appropriate metrics.
        //

        if (ReleasedCount != 0) {
            Segment->FreePages += ReleasedCount;
            SignalEvent = MmpUpdatePhysicalMemoryStatistics(ReleasedCount,
                                                            FALSE);
        }

        goto FreePhysicalPageEnd;
    }

    //
    // The page was not found. If the total number of physical pages was
    // truncated, then these may be boot allocations being released. Trust that
    // these were valid pages. If no truncation was possible, then report the
    // missing page and assert.
    //

    if ((MmLimitTotalPhysicalPages == 0) &&
        (MmLowestPhysicalPage == 0) &&
        (MmMaximumPhysicalAddress > PhysicalAddress)) {

        RtlDebugPrint("Error: Attempt to free non-existant physical page "
                      "0x%I64x.\n",
                      PhysicalAddress);

        ASSERT(FALSE);
    }

FreePhysicalPageEnd:
    if (MmPhysicalPageLock != NULL) {
        KeReleaseQueuedLock(MmPhysicalPageLock);
    }

    while (LIST_EMPTY(&PagingEntryList) == FALSE) {
        PagingEntry = LIST_VALUE(PagingEntryList.Next,
                                 PAGING_ENTRY,
                                 U.ListEntry);

        LIST_REMOVE(&(PagingEntry->U.ListEntry));
        MmpDestroyPagingEntry(PagingEntry);
    }

    if (SignalEvent != FALSE) {

        ASSERT(MmPhysicalMemoryWarningEvent != NULL);

        KeSignalEvent(MmPhysicalMemoryWarningEvent, SignalOptionPulse);
    }

    return;
}

VOID
MmSetPageCacheEntryForPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    PVOID PageCacheEntry
    )

/*++

Routine Description:

    This routine sets the page cache entry for the given physical address.

Arguments:

    PhysicalAddress - Supplies the address of the physical page whose page
        cache entry is to be set.

    PageCacheEntry - Supplies a pointer to the page cache entry to be set for
        the given physical page.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN Offset;
    ULONG PageShift;
    PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;

    PageShift = MmPageShift();

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(MmPhysicalPageLock);
    CurrentEntry = MmPhysicalSegmentListHead.Next;
    while (CurrentEntry != &MmPhysicalSegmentListHead) {
        Segment = LIST_VALUE(CurrentEntry, PHYSICAL_MEMORY_SEGMENT, ListEntry);
        if ((PhysicalAddress < Segment->StartAddress) ||
            (PhysicalAddress >= Segment->EndAddress)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        Offset = (PhysicalAddress - Segment->StartAddress) >> PageShift;
        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        PhysicalPage += Offset;

        //
        // This request should only be on a non-paged physical page.
        //

        ASSERT((PhysicalPage->U.Flags & PHYSICAL_PAGE_FLAG_NON_PAGED) != 0);
        ASSERT(((UINTN)PageCacheEntry & PHYSICAL_PAGE_FLAG_NON_PAGED) == 0);

        PageCacheEntry = (PVOID)((UINTN)PageCacheEntry |
                                 PHYSICAL_PAGE_FLAG_NON_PAGED);

        PhysicalPage->U.PageCacheEntry = PageCacheEntry;
        goto SetPageCacheEntryForPhysicalAddressEnd;
    }

    //
    // The page was not found. This probably indicates a serious memory
    // corruption. Consider crashing the system altogether.
    //

    ASSERT(FALSE);

SetPageCacheEntryForPhysicalAddressEnd:
    KeReleaseQueuedLock(MmPhysicalPageLock);
    return;
}

KSTATUS
MmpInitializePhysicalPageAllocator (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    PVOID *InitMemory,
    PUINTN InitMemorySize
    )

/*++

Routine Description:

    This routine initializes the physical page allocator, given the system
    memory map. It carves off as many pages as it needs for its own purposes,
    and initializes the rest in the physical page allocator.

Arguments:

    MemoryMap - Supplies a pointer to the current memory layout of the system.

    InitMemory - Supplies a pointer where a pointer to the initialization
        memory provided by the loader is given on input. On output, this
        pointer is advanced beyond what this routine allocated from it.

    InitMemorySize - Supplies a pointer that on input contains the size of the
        init memory region. On output, this will be advanced beyond what was
        allocated by this function.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the memory map is invalid.

    STATUS_NO_MEMORY if not enough memory is present to initialize the physical
        memory allocator.

--*/

{

    UINTN AllocationSize;
    INIT_PHYSICAL_MEMORY_ITERATOR Context;
    UINTN Count;
    ULONG LastBitIndex;
    ULONG LeadingZeros;
    ULONG PageShift;
    PUCHAR RawBuffer;
    KSTATUS Status;

    PageShift = MmPageShift();
    Status = STATUS_SUCCESS;
    INITIALIZE_LIST_HEAD(&MmPhysicalSegmentListHead);

    //
    // Loop through the descriptors once to determine the number of segments
    // and total physical memory.
    //

    RtlZeroMemory(&Context, sizeof(INIT_PHYSICAL_MEMORY_ITERATOR));
    MmMdIterate(MemoryMap,
                MmpInitializePhysicalAllocatorIterationRoutine,
                &Context);

    //
    // Allocate space for the memory structures.
    //

    ASSERT((Context.TotalMemoryBytes >> PageShift) <= MAX_UINTN);

    Context.TotalMemoryPages = (UINTN)(Context.TotalMemoryBytes >> PageShift);
    if ((MmLimitTotalPhysicalPages != 0) &&
        (Context.TotalMemoryPages > MmLimitTotalPhysicalPages)) {

        Context.TotalMemoryPages = MmLimitTotalPhysicalPages;
    }

    AllocationSize = (Context.TotalMemoryPages * sizeof(PHYSICAL_PAGE)) +
                     (Context.TotalSegments * sizeof(PHYSICAL_MEMORY_SEGMENT));

    if (*InitMemorySize < AllocationSize) {
        Status = STATUS_NO_MEMORY;
        goto InitializePhysicalPageAllocatorEnd;
    }

    RawBuffer = *InitMemory;
    *InitMemory += AllocationSize;
    *InitMemorySize -= AllocationSize;

    //
    // Loop through the descriptors again and set up the physical memory
    // structures.
    //

    Context.CurrentPage = (PPHYSICAL_PAGE)RawBuffer;
    Context.TotalSegments = 0;
    Context.TotalMemoryBytes = 0;
    Context.LastEnd = 0;
    MmMdIterate(MemoryMap,
                MmpInitializePhysicalAllocatorIterationRoutine,
                &Context);

    //
    // Now that the memory map has been truncated by both the maximum physical
    // address and the physical page limit, the context's last end is the
    // maximum physical page.
    //

    if (MmLimitTotalPhysicalPages != 0) {
        MmMaximumPhysicalAddress = Context.LastEnd;
    }

    MmLastAllocatedSegment = LIST_VALUE(MmPhysicalSegmentListHead.Next,
                                        PHYSICAL_MEMORY_SEGMENT,
                                        ListEntry);

    MmLastAllocatedSegmentOffset = 0;
    MmLastPagedSegment = MmLastAllocatedSegment;
    MmLastPagedSegmentOffset = 0;
    MmTotalPhysicalPages = Context.TotalMemoryPages;
    MmMinimumFreePhysicalPages =
                (MmTotalPhysicalPages * MIN_FREE_PHYSICAL_PAGES_PERCENT) / 100;

    ASSERT(MmMinimumFreePhysicalPages > 0);

    //
    // Initialize the physical memory warning levels.
    //

    MmPhysicalMemoryWarningLevel = MemoryWarningLevelNone;
    MmPhysicalMemoryWarningLevel1HighPages =
            (MmTotalPhysicalPages * MEMORY_WARNING_LEVEL_1_HIGH_PERCENT) / 100;

    MmPhysicalMemoryWarningLevel1LowPages =
             (MmTotalPhysicalPages * MEMORY_WARNING_LEVEL_1_LOW_PERCENT) / 100;

    MmPhysicalMemoryWarningLevel2HighPages =
            (MmTotalPhysicalPages * MEMORY_WARNING_LEVEL_2_HIGH_PERCENT) / 100;

    MmPhysicalMemoryWarningLevel2LowPages =
             (MmTotalPhysicalPages * MEMORY_WARNING_LEVEL_2_LOW_PERCENT) / 100;

    //
    // Compute the mask for the allocate and free counters. Get the percentage
    // and round it up to the nearest power of 2.
    //

    Count = (MmTotalPhysicalPages * MEMORY_WARNING_COUNT_MASK_PERCENT) / 100;
    if (Count == 0) {
        Count = 1;
    }

    LeadingZeros = RtlCountLeadingZeros(Count);
    LastBitIndex = (sizeof(UINTN) * BITS_PER_BYTE) - LeadingZeros;
    MmPhysicalMemoryWarningCountMask = (UINTN)(1 << LastBitIndex) - 1;
    Status = STATUS_SUCCESS;

InitializePhysicalPageAllocatorEnd:
    return Status;
}

VOID
MmpGetPhysicalPageStatistics (
    PMM_STATISTICS Statistics
    )

/*++

Routine Description:

    This routine fills out the physical memory portion of the given memory
    statistics structure.

Arguments:

    Statistics - Supplies a pointer to the statistics to fill in.

Return Value:

    None.

--*/

{

    Statistics->PhysicalPages = MmTotalPhysicalPages;
    Statistics->AllocatedPhysicalPages = MmTotalAllocatedPhysicalPages;
    Statistics->NonPagedPhysicalPages = MmNonPagedPhysicalPages;
    return;
}

PHYSICAL_ADDRESS
MmpAllocatePhysicalPages (
    UINTN PageCount,
    UINTN Alignment
    )

/*++

Routine Description:

    This routine allocates a physical page of memory. If necessary, it will
    notify the system that free physical memory is low and wake up the page out
    worker thread. All allocate pages start out as non-paged and must be
    made pagable.

Arguments:

    PageCount - Supplies the number of consecutive physical pages required.

    Alignment - Supplies the alignment requirement of the allocation, in pages.
        Valid values are powers of 2. Values of 1 or 0 indicate no alignment
        requirement.

Return Value:

    Returns the physical address of the first page of allocated memory on
    success, or INVALID_PHYSICAL_ADDRESS on failure.

--*/

{

    UINTN FreePageTarget;
    BOOL LockHeld;
    UINTN PageIndex;
    ULONG PageShift;
    volatile PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    UINTN SegmentOffset;
    BOOL SignalEvent;
    ULONGLONG Timeout;
    PHYSICAL_ADDRESS WorkingAllocation;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((MmPagingThread == NULL) ||
           (KeGetCurrentThread() != MmPagingThread));

    LockHeld = FALSE;
    PageShift = MmPageShift();
    SignalEvent = FALSE;
    WorkingAllocation = INVALID_PHYSICAL_ADDRESS;
    if (Alignment == 0) {
        Alignment = 1;
    }

    //
    // Loop continuously looking for free pages.
    //

    Timeout = 0;
    while (TRUE) {
        if (MmPhysicalPageLock != NULL) {
            KeAcquireQueuedLock(MmPhysicalPageLock);
            LockHeld = TRUE;
        }

        //
        // Attempt to find some free pages.
        //

        Segment = MmpFindPhysicalPages(PageCount,
                                       Alignment,
                                       PhysicalMemoryFindFree,
                                       &SegmentOffset,
                                       NULL);

        //
        // If a section of free memory was available, grab it up!
        //

        if (Segment != NULL) {
            WorkingAllocation = Segment->StartAddress +
                                (SegmentOffset << PageShift);

            PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
            PhysicalPage += SegmentOffset;
            for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {

                ASSERT(PhysicalPage->U.Free == PHYSICAL_PAGE_FREE);

                PhysicalPage->U.Flags = PHYSICAL_PAGE_FLAG_NON_PAGED;
                PhysicalPage += 1;
            }

            Segment->FreePages -= PageCount;
            SignalEvent = MmpUpdatePhysicalMemoryStatistics(PageCount, TRUE);
            goto AllocatePhysicalPagesEnd;
        }

        //
        // Page out to try to get back to the minimum free count, or at least
        // enough to hopefully satisfy the request.
        //

        FreePageTarget = MmMinimumFreePhysicalPages;
        if (FreePageTarget < (PageCount + Alignment)) {
            FreePageTarget = PageCount + Alignment;
        }

        if (LockHeld != FALSE) {
            KeReleaseQueuedLock(MmPhysicalPageLock);
            LockHeld = FALSE;
        }

        //
        // Not enough free memory could be found laying around. Schedule the
        // paging worker to notify it that memory is a little tight. If it gets
        // scheduled, wait for it to free some pages.
        //

        if (MmRequestPagingOut(FreePageTarget) != FALSE) {
            KeWaitForEvent(MmPagingFreePagesEvent, FALSE, WAIT_TIME_INDEFINITE);
        }

        //
        // If this is the first time around, set the timeout timer to decide
        // when to give up.
        //

        if (Timeout == 0) {
            Timeout = KeGetRecentTimeCounter() +
                      (HlQueryTimeCounterFrequency() *
                       PHYSICAL_MEMORY_ALLOCATION_TIMEOUT);

        } else {

            //
            // If it's been quite awhile and still there is no free physical
            // page, it's time to assume forward progress will never be made
            // and throw in the towel.
            //

            if (KeGetRecentTimeCounter() >= Timeout) {
                KeCrashSystem(CRASH_OUT_OF_MEMORY, PageCount, Alignment, 0, 0);
            }
        }
    }

AllocatePhysicalPagesEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(MmPhysicalPageLock);
    }

    //
    // This allocation was successful.
    //

    ASSERT(WorkingAllocation != INVALID_PHYSICAL_ADDRESS);

    //
    // Signal the physical memory change event if it was determined above.
    //

    if (SignalEvent != FALSE) {

        ASSERT(MmPhysicalMemoryWarningEvent != NULL);

        KeSignalEvent(MmPhysicalMemoryWarningEvent, SignalOptionPulse);
    }

    return WorkingAllocation;
}

PHYSICAL_ADDRESS
MmpAllocateIdentityMappablePhysicalPages (
    UINTN PageCount,
    UINTN Alignment
    )

/*++

Routine Description:

    This routine allocates physical memory that can be identity mapped to
    the same virtual address as the physical address returned. This routine
    does not ensure that the virtual address range stays free, so this routine
    must only be used internally and in a very controlled environment.

Arguments:

    PageCount - Supplies the number of consecutive physical pages required.

    Alignment - Supplies the alignment requirement of the allocation, in pages.
        Valid values are powers of 2. Values of 1 or 0 indicate no alignment
        requirement.

Return Value:

    Returns a physical pointer to the memory on success, or NULL on failure.

--*/

{

    UINTN PageIndex;
    ULONG PageShift;
    PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    UINTN SegmentOffset;
    PHYSICAL_ADDRESS WorkingAllocation;

    PageShift = MmPageShift();
    WorkingAllocation = INVALID_PHYSICAL_ADDRESS;
    if (Alignment == 0) {
        Alignment = 1;
    }

    if (MmPhysicalPageLock != NULL) {
        KeAcquireQueuedLock(MmPhysicalPageLock);
    }

    //
    // Attempt to find some free pages.
    //

    Segment = MmpFindPhysicalPages(PageCount,
                                   Alignment,
                                   PhysicalMemoryFindIdentityMappable,
                                   &SegmentOffset,
                                   NULL);

    if (Segment != NULL) {
        WorkingAllocation = Segment->StartAddress +
                            (SegmentOffset << PageShift);
    }

    if (WorkingAllocation != INVALID_PHYSICAL_ADDRESS) {
        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        PhysicalPage += SegmentOffset;
        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {

            ASSERT(PhysicalPage->U.Free == PHYSICAL_PAGE_FREE);

            Segment->FreePages -= 1;
            MmTotalAllocatedPhysicalPages += 1;
            MmNonPagedPhysicalPages += 1;

            ASSERT(MmTotalAllocatedPhysicalPages <= MmTotalPhysicalPages);

            PhysicalPage->U.Flags = PHYSICAL_PAGE_FLAG_NON_PAGED;
            PhysicalPage += 1;
        }
    }

    if (MmPhysicalPageLock != NULL) {
        KeReleaseQueuedLock(MmPhysicalPageLock);
    }

    return WorkingAllocation;
}

KSTATUS
MmpAllocateScatteredPhysicalPages (
    PHYSICAL_ADDRESS MinPhysical,
    PHYSICAL_ADDRESS MaxPhysical,
    PPHYSICAL_ADDRESS Pages,
    UINTN PageCount
    )

/*++

Routine Description:

    This routine allocates a set of any physical pages.

Arguments:

    MinPhysical - Supplies the minimum physical address for the allocations,
        inclusive.

    MaxPhysical - Supplies the maximum physical address to allocate, exclusive.

    Pages - Supplies a pointer to an array where the physical addresses
        allocated will be returned.

    PageCount - Supplies the number of pages to allocate.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_MEMORY on failure.

--*/

{

    PHYSICAL_ADDRESS EndAddress;
    UINTN EndOffset;
    BOOL FirstIteration;
    PPHYSICAL_MEMORY_SEGMENT LastSegment;
    UINTN LastSegmentOffset;
    UINTN Offset;
    UINTN PageIndex;
    ULONG PageShift;
    PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    BOOL SignalEvent;
    PHYSICAL_ADDRESS StartAddress;

    FirstIteration = TRUE;
    PageShift = MmPageShift();

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(MmPhysicalPageLock);
    LastSegment = MmLastAllocatedSegment;
    LastSegmentOffset = MmLastAllocatedSegmentOffset;
    Segment = LastSegment;

    //
    // Adjust the offset to the min/max. If the segment is completely out of
    // range, then the offset should end up at or beyond the end offset to
    // trigger moving to the next segment.
    //

    EndAddress = Segment->EndAddress;
    if (EndAddress > MaxPhysical) {
        EndAddress = MaxPhysical;
    }

    StartAddress = Segment->StartAddress;
    if (StartAddress < MinPhysical) {
        StartAddress = MinPhysical;
    }

    EndOffset = 0;
    if (EndAddress >= StartAddress) {
        EndOffset = (EndAddress - Segment->StartAddress) >> PageShift;
    }

    Offset = LastSegmentOffset;
    if (Segment->StartAddress + (Offset << PageShift) < StartAddress) {
        Offset = (StartAddress - Segment->StartAddress) >> PageShift;
    }

    PageIndex = 0;
    while (PageIndex < PageCount) {

        //
        // See if it's time to move to a new segment.
        //

        if ((Offset >= EndOffset) || (Segment->FreePages == 0)) {
            if ((Segment == LastSegment) && (FirstIteration == FALSE)) {
                break;
            }

            FirstIteration = FALSE;
            if (Segment->ListEntry.Next == &MmPhysicalSegmentListHead) {
                Segment = LIST_VALUE(MmPhysicalSegmentListHead.Next,
                                     PHYSICAL_MEMORY_SEGMENT,
                                     ListEntry);

            } else {
                Segment = LIST_VALUE(Segment->ListEntry.Next,
                                     PHYSICAL_MEMORY_SEGMENT,
                                     ListEntry);
            }

            EndAddress = Segment->EndAddress;
            if (EndAddress > MaxPhysical) {
                EndAddress = MaxPhysical;
            }

            Offset = 0;
            StartAddress = Segment->StartAddress;
            if (StartAddress < MinPhysical) {
                StartAddress = MinPhysical;
                Offset = (StartAddress - Segment->StartAddress) >> PageShift;
            }

            EndOffset = 0;
            if (EndAddress >= StartAddress) {
                EndOffset = (EndAddress - Segment->StartAddress) >> PageShift;
            }
        }

        //
        // Suck up all the pages in this segment.
        //

        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        while ((Offset < EndOffset) && (Segment->FreePages != 0)) {
            if (PhysicalPage[Offset].U.Free == PHYSICAL_PAGE_FREE) {
                PhysicalPage[Offset].U.Flags = PHYSICAL_PAGE_FLAG_NON_PAGED;
                Pages[PageIndex] = Segment->StartAddress +
                                   (Offset << PageShift);

                ASSERT(Segment->FreePages != 0);

                Segment->FreePages -= 1;
                PageIndex += 1;
                if (PageIndex == PageCount) {
                    MmLastAllocatedSegment = Segment;
                    MmLastAllocatedSegmentOffset = Offset;
                    break;
                }
            }

            Offset += 1;
        }
    }

    SignalEvent = MmpUpdatePhysicalMemoryStatistics(PageCount, TRUE);
    KeReleaseQueuedLock(MmPhysicalPageLock);
    if (SignalEvent != FALSE) {
        KeSignalEvent(MmPhysicalMemoryWarningEvent, SignalOptionPulse);
    }

    //
    // Space seems to be limited, since not all spots were allocated and all of
    // physical memory was traversed. Allocate the slow way, with delays and
    // attempted page outs.
    //

    while (PageIndex < PageCount) {
        Pages[PageIndex] = MmpAllocatePhysicalPages(1, 0);
        if (Pages[PageIndex] == INVALID_PHYSICAL_ADDRESS) {

            //
            // Ick. Free everything allocated so far and give up.
            //

            PageCount = PageIndex;
            for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
                MmFreePhysicalPage(Pages[PageIndex]);
            }

            return STATUS_NO_MEMORY;
        }

        PageIndex += 1;
    }

    return STATUS_SUCCESS;
}

KSTATUS
MmpEarlyAllocatePhysicalMemory (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    UINTN PageCount,
    UINTN Alignment,
    ALLOCATION_STRATEGY Strategy,
    PPHYSICAL_ADDRESS Allocation
    )

/*++

Routine Description:

    This routine allocates physical memory for MM init routines. It should only
    be used during early MM initialization. If the physical page allocator is
    up, this routine will attempt to use that, otherwise it will carve memory
    directly off the memory map.

Arguments:

    MemoryMap - Supplies a pointer to the system memory map.

    PageCount - Supplies the number of physical pages needed.

    Alignment - Supplies the required alignment of the allocation, in pagse.
        Valid values are powers of 2. Supply 0 or 1 for no alignment
        requirement.

    Strategy - Supplies the memory allocation strategy to employ.

    Allocation - Supplies a pointer where the allocated address will be
        returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_MEMORY on failure.

    STATUS_TOO_LATE if the real physical memory allocator is already online.

--*/

{

    ULONG PageShift;
    KSTATUS Status;

    *Allocation = INVALID_PHYSICAL_ADDRESS;
    PageShift = MmPageShift();

    //
    // This routine should not be used if the real physical allocator has been
    // initialized.
    //

    ASSERT(MmTotalPhysicalPages == 0);

    if (MmTotalPhysicalPages != 0) {
        Status = STATUS_TOO_LATE;
        goto EarlyAllocatePhysicalMemoryEnd;
    }

    if (Alignment == 0) {
        Alignment = 1;
    }

    Status = MmMdAllocateFromMdl(MemoryMap,
                                 Allocation,
                                 PageCount << PageShift,
                                 Alignment << PageShift,
                                 0,
                                 MAX_ULONGLONG,
                                 MemoryTypeMmStructures,
                                 Strategy);

    if (!KSUCCESS(Status)) {
        goto EarlyAllocatePhysicalMemoryEnd;
    }

EarlyAllocatePhysicalMemoryEnd:
    return Status;
}

VOID
MmpEnablePagingOnPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN PageCount,
    PPAGING_ENTRY *PagingEntries,
    BOOL LockPages
    )

/*++

Routine Description:

    This routine sets one or more physical pages to be pagable. This is done
    as a separate step from the allocation to prevent situations where a
    thread tries to page out a page that is currently in the process of being
    paged in.

Arguments:

    PhysicalAddress - Supplies the physical address to make pagable.

    PageCount - Supplies the length of the range, in pages, of the physical
        space.

    PagingEntries - Supplies an array of paging entries for each page.

    LockPages - Supplies a boolean indicating if these pageable pages should
        start locked.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN PageIndex;
    UINTN PageOffset;
    ULONG PageShift;
    ULONG PageSize;
    PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;

    PageShift = MmPageShift();
    PageSize = MmPageSize();

    ASSERT(IS_ALIGNED(PhysicalAddress, PageSize) != FALSE);

    if (MmPhysicalPageLock != NULL) {
        KeAcquireQueuedLock(MmPhysicalPageLock);
    }

    CurrentEntry = MmPhysicalSegmentListHead.Next;
    while (CurrentEntry != &MmPhysicalSegmentListHead) {
        Segment = LIST_VALUE(CurrentEntry, PHYSICAL_MEMORY_SEGMENT, ListEntry);
        if ((PhysicalAddress < Segment->StartAddress) ||
            (PhysicalAddress >= Segment->EndAddress)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        //
        // Any contiguous memory should be contained in the same memory segment.
        //

        ASSERT((PhysicalAddress + (PageCount << PageShift)) <=
               Segment->EndAddress);

        //
        // Mark each page in the segment as pagable by adding in the supplied
        // paging entry.
        //

        PageOffset = (PhysicalAddress - Segment->StartAddress) >> PageShift;
        PhysicalPage = ((PPHYSICAL_PAGE)(Segment + 1)) + PageOffset;
        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {

            ASSERT(PhysicalPage->U.Flags == PHYSICAL_PAGE_FLAG_NON_PAGED);
            ASSERT(((UINTN)PagingEntries[PageIndex] &
                    PHYSICAL_PAGE_FLAG_NON_PAGED) == 0);

            PhysicalPage->U.PagingEntry = PagingEntries[PageIndex];

            ASSERT(PagingEntries[PageIndex]->Section != NULL);
            ASSERT((PagingEntries[PageIndex]->Section->Flags &
                    IMAGE_SECTION_DESTROYED) == 0);

            if (LockPages != FALSE) {

                ASSERT(PhysicalPage->U.PagingEntry->U.LockCount == 0);

                PhysicalPage->U.PagingEntry->U.LockCount = 1;

            } else {
                MmNonPagedPhysicalPages -= 1;
            }

            PhysicalPage += 1;
        }

        break;
    }

    if (MmPhysicalPageLock != NULL) {
        KeReleaseQueuedLock(MmPhysicalPageLock);
    }

    return;
}

KSTATUS
MmpLockPhysicalPages (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN PageCount
    )

/*++

Routine Description:

    This routine locks a set of physical pages in memory.

Arguments:

    PhysicalAddress - Supplies the physical address to lock.

    PageCount - Supplies the number of consecutive physical pages to lock. 0 is
        not a valid value.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN Flags;
    UINTN MaxOffset;
    UINTN Offset;
    UINTN PageIndex;
    ULONG PageShift;
    PPAGING_ENTRY PagingEntry;
    PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    KSTATUS Status;

    PageIndex = 0;
    PageShift = MmPageShift();

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (MmPhysicalPageLock != NULL) {
        KeAcquireQueuedLock(MmPhysicalPageLock);
    }

    //
    // Loop through every segment looking for the one that owns these pages.
    //

    CurrentEntry = MmPhysicalSegmentListHead.Next;
    while (CurrentEntry != &MmPhysicalSegmentListHead) {
        Segment = LIST_VALUE(CurrentEntry, PHYSICAL_MEMORY_SEGMENT, ListEntry);
        if ((PhysicalAddress < Segment->StartAddress) ||
            (PhysicalAddress >= Segment->EndAddress)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        Offset = (PhysicalAddress - Segment->StartAddress) >> PageShift;
        MaxOffset = (Segment->EndAddress - Segment->StartAddress) >> PageShift;
        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        PhysicalPage += Offset;

        //
        // Loop through the number of contiguous pages requested, and mark each
        // one as locked if it was marked as pagable.
        //

        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {

            ASSERT((Offset + PageIndex) < MaxOffset);
            ASSERT(PhysicalPage[PageIndex].U.Free != PHYSICAL_PAGE_FREE);

            //
            // If there is no paging entry and this is just a non-paged
            // allocation, then it is already locked down.
            //

            Flags = PhysicalPage[PageIndex].U.Flags;
            if ((Flags & PHYSICAL_PAGE_FLAG_NON_PAGED) != 0) {
                continue;
            }

            PagingEntry = PhysicalPage[PageIndex].U.PagingEntry;

            ASSERT(PagingEntry != NULL);

            //
            // Locking a pageable page should only happen with the section's
            // lock held.
            //

            ASSERT(KeIsQueuedLockHeld(PagingEntry->Section->Lock) != FALSE);

            //
            // Fail if too many callers have attempted to lock this page.
            //

            if (PagingEntry->U.LockCount == MAX_PHYSICAL_PAGE_LOCK_COUNT) {
                Status = STATUS_RESOURCE_IN_USE;
                goto LockPhysicalPagesEnd;
            }

            //
            // If this is the first request to lock the page, then increment
            // the non-paged physical page count.
            //

            if (PagingEntry->U.LockCount == 0) {
                MmNonPagedPhysicalPages += 1;
            }

            PagingEntry->U.LockCount += 1;
        }

        Status = STATUS_SUCCESS;
        goto LockPhysicalPagesEnd;
    }

    //
    // The page was not found. This probably indicates a serious memory
    // corruption. Consider crashing the system altogether.
    //

    ASSERT(FALSE);

    Status = STATUS_NOT_FOUND;

LockPhysicalPagesEnd:
    if (MmPhysicalPageLock != NULL) {
        KeReleaseQueuedLock(MmPhysicalPageLock);
    }

    //
    // Undo what was done on failure.
    //

    if (!KSUCCESS(Status)) {
        if (PageIndex != 0) {
            MmpUnlockPhysicalPages(PhysicalAddress, PageIndex);
        }
    }

    return Status;
}

VOID
MmpUnlockPhysicalPages (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN PageCount
    )

/*++

Routine Description:

    This routine unlocks a set of physical pages in memory.

Arguments:

    PhysicalAddress - Supplies the physical address to unlock.

    PageCount - Supplies the number of consecutive physical pages to unlock.
        Zero is not a valid value.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN Flags;
    UINTN MaxOffset;
    UINTN Offset;
    UINTN PageIndex;
    ULONG PageShift;
    PPAGING_ENTRY PagingEntry;
    LIST_ENTRY PagingEntryList;
    PPHYSICAL_PAGE PhysicalPage;
    UINTN ReleasedCount;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    BOOL SignalEvent;

    PageShift = MmPageShift();
    INITIALIZE_LIST_HEAD(&PagingEntryList);
    ReleasedCount = 0;
    SignalEvent = FALSE;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(MmPhysicalPageLock);
    CurrentEntry = MmPhysicalSegmentListHead.Next;
    while (CurrentEntry != &MmPhysicalSegmentListHead) {
        Segment = LIST_VALUE(CurrentEntry, PHYSICAL_MEMORY_SEGMENT, ListEntry);
        if ((PhysicalAddress < Segment->StartAddress) ||
            (PhysicalAddress >= Segment->EndAddress)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        Offset = (PhysicalAddress - Segment->StartAddress) >> PageShift;
        MaxOffset = (Segment->EndAddress - Segment->StartAddress) >> PageShift;
        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        PhysicalPage += Offset;

        //
        // Loop through and unlock the number of contiguous pages requested.
        //

        for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {

            ASSERT((Offset + PageIndex) < MaxOffset);
            ASSERT(PhysicalPage[PageIndex].U.Free != PHYSICAL_PAGE_FREE);

            //
            // If this is a non-paged physical page, then skip it.
            //

            Flags = PhysicalPage[PageIndex].U.Flags;
            if ((Flags & PHYSICAL_PAGE_FLAG_NON_PAGED) != 0) {
                continue;
            }

            PagingEntry = PhysicalPage[PageIndex].U.PagingEntry;

            ASSERT(PagingEntry != NULL);
            ASSERT(PagingEntry->U.LockCount != 0);

            PagingEntry->U.LockCount -= 1;
            if (PagingEntry->U.LockCount == 0) {
                MmNonPagedPhysicalPages -= 1;
                if ((PagingEntry->U.Flags & PAGING_ENTRY_FLAG_FREED) != 0) {
                    PhysicalPage[PageIndex].U.Free = PHYSICAL_PAGE_FREE;
                    ReleasedCount += 1;
                    INSERT_BEFORE(&(PagingEntry->U.ListEntry),
                                  &PagingEntryList);
                }
            }
        }

        if (ReleasedCount != 0) {
            Segment->FreePages += ReleasedCount;
            SignalEvent = MmpUpdatePhysicalMemoryStatistics(ReleasedCount,
                                                            FALSE);
        }

        goto UnlockPhysicalPageEnd;
    }

    //
    // The page was not found. This probably indicates a serious memory
    // corruption. Consider crashing the system altogether.
    //

    ASSERT(FALSE);

UnlockPhysicalPageEnd:
    KeReleaseQueuedLock(MmPhysicalPageLock);
    while (LIST_EMPTY(&PagingEntryList) == FALSE) {
        PagingEntry = LIST_VALUE(PagingEntryList.Next,
                                 PAGING_ENTRY,
                                 U.ListEntry);

        LIST_REMOVE(&(PagingEntry->U.ListEntry));
        MmpDestroyPagingEntry(PagingEntry);
    }

    if (SignalEvent != FALSE) {

        ASSERT(MmPhysicalMemoryWarningEvent != NULL);

        KeSignalEvent(MmPhysicalMemoryWarningEvent, SignalOptionPulse);
    }

    return;
}

PPAGE_CACHE_ENTRY
MmpGetPageCacheEntryForPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine gets the page cache entry for the given physical address.

Arguments:

    PhysicalAddress - Supplies the address of the physical page whose page
        cache entry is to be returned.

Return Value:

    Returns a pointer to a page cache entry on success or NULL if there is
    no page cache entry for the specified address.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN Offset;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageShift;
    PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;

    PageCacheEntry = NULL;
    PageShift = MmPageShift();

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(MmPhysicalPageLock);
    CurrentEntry = MmPhysicalSegmentListHead.Next;
    while (CurrentEntry != &MmPhysicalSegmentListHead) {
        Segment = LIST_VALUE(CurrentEntry, PHYSICAL_MEMORY_SEGMENT, ListEntry);
        if ((PhysicalAddress < Segment->StartAddress) ||
            (PhysicalAddress >= Segment->EndAddress)) {

            CurrentEntry = CurrentEntry->Next;
            continue;
        }

        Offset = (PhysicalAddress - Segment->StartAddress) >> PageShift;
        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        PhysicalPage += Offset;

        //
        // If the physical address is a non-paged entry, then get the
        // associated page cache entry, if any. This might just be a
        // non-paged physical page without a page cache entry but returning
        // NULL in that case is expected.
        //

        if ((PhysicalPage->U.Flags & PHYSICAL_PAGE_FLAG_NON_PAGED) != 0) {
            PageCacheEntry = PhysicalPage->U.PageCacheEntry;
            PageCacheEntry = (PVOID)((UINTN)PageCacheEntry &
                                     ~PHYSICAL_PAGE_FLAG_NON_PAGED);
        }

        goto GetPageCacheEntryForPhysicalAddressEnd;
    }

    //
    // The page was not found. This probably indicates a serious memory
    // corruption. Consider crashing the system altogether.
    //

    ASSERT(FALSE);

GetPageCacheEntryForPhysicalAddressEnd:
    KeReleaseQueuedLock(MmPhysicalPageLock);
    return PageCacheEntry;
}

VOID
MmpMigratePagingEntries (
    PIMAGE_SECTION OldSection,
    PIMAGE_SECTION NewSection,
    PVOID Address,
    UINTN PageCount
    )

/*++

Routine Description:

    This routine migrates all existing paging entries in the given virtual
    address space over to a new image section.

Arguments:

    OldSection - Supplies a pointer to the old image section losing entries.
        This section must have at least one extra reference held on it, this
        routine cannot be left releasing the last reference.

    NewSection - Supplies a pointer to the new section taking ownership of the
        region.

    Address - Supplies the address to start at.

    PageCount - Supplies the number of pages to convert.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN PageIndex;
    UINTN PageOffset;
    UINTN PageShift;
    UINTN PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    UINTN SegmentOffset;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    PageShift = MmPageShift();
    PageSize = MmPageSize();
    PageOffset = ((UINTN)Address - (UINTN)(NewSection->VirtualAddress)) >>
                 PageShift;

    ASSERT(((PageOffset + PageCount) << PageShift) <= NewSection->Size);

    //
    // Loop through editing paging entries with the physical lock held.
    //

    Segment = NULL;
    KeAcquireQueuedLock(MmPhysicalPageLock);
    for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
        PhysicalAddress = MmpVirtualToPhysical(Address, NULL);
        if (PhysicalAddress != INVALID_PHYSICAL_ADDRESS) {

            //
            // Locate the segment this page resides in if the current segment
            // isn't it. There's a very high likelihood they'll all be in the
            // same segment.
            //

            if ((Segment == NULL) ||
                (!((Segment->StartAddress <= PhysicalAddress) &&
                   (Segment->EndAddress > PhysicalAddress)))) {

                CurrentEntry = MmPhysicalSegmentListHead.Next;
                while (CurrentEntry != &MmPhysicalSegmentListHead) {
                    Segment = LIST_VALUE(CurrentEntry,
                                         PHYSICAL_MEMORY_SEGMENT,
                                         ListEntry);

                    if ((Segment->StartAddress <= PhysicalAddress) &&
                        (Segment->EndAddress > PhysicalAddress)) {

                        break;
                    }

                    CurrentEntry = CurrentEntry->Next;
                }

                if (CurrentEntry == &MmPhysicalSegmentListHead) {

                    //
                    // An unknown physical address was mapped.
                    //

                    ASSERT(FALSE);

                    Segment = NULL;
                    continue;
                }
            }

            PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
            SegmentOffset = (PhysicalAddress - Segment->StartAddress) >>
                            PageShift;

            PhysicalPage += SegmentOffset;

            ASSERT(PhysicalPage->U.Flags != PHYSICAL_PAGE_FREE);

            //
            // If it's a page cache entry, just leave it alone. Otherwise, it
            // had better point to the old section.
            //

            if ((PhysicalPage->U.Flags & PHYSICAL_PAGE_FLAG_NON_PAGED) == 0) {

                ASSERT(PhysicalPage->U.PagingEntry->Section == OldSection);

                MmpImageSectionReleaseReference(
                                         PhysicalPage->U.PagingEntry->Section);

                MmpImageSectionAddReference(NewSection);
                PhysicalPage->U.PagingEntry->Section = NewSection;
                PhysicalPage->U.PagingEntry->U.SectionOffset =
                                                        PageOffset + PageIndex;
            }
        }

        Address += PageSize;
    }

    KeReleaseQueuedLock(MmPhysicalPageLock);
    return;
}

UINTN
MmpPageOutPhysicalPages (
    UINTN FreePagesTarget,
    PIO_BUFFER IoBuffer,
    PMEMORY_RESERVATION SwapRegion
    )

/*++

Routine Description:

    This routine pages out physical pages to the backing store.

Arguments:

    FreePagesTarget - Supplies the target number of free pages the system
        should have.

    IoBuffer - Supplies a pointer to an allocated but uninitialized I/O buffer
        to use during page out I/O.

    SwapRegion - Supplies a pointer to a region of VA space to use during page
        out I/O.

Return Value:

    Returns the number of physical pages that were able to be paged out.

--*/

{

    BOOL Failure;
    ULONG FailureCount;
    UINTN FreePages;
    BOOL LockHeld;
    UINTN PageCountSinceEvent;
    UINTN PagesFound;
    ULONG PageShift;
    UINTN PagesPaged;
    PPAGING_ENTRY PagingEntry;
    PHYSICAL_ADDRESS PhysicalAddress;
    PPHYSICAL_PAGE PhysicalPage;
    PIMAGE_SECTION Section;
    UINTN SectionOffset;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    UINTN SegmentOffset;
    KSTATUS Status;
    UINTN TotalPagesPaged;

    LockHeld = FALSE;
    PageShift = MmPageShift();

    //
    // Now attempt to swap pages out to the backing store. This algorithm can
    // get more intelligent with time, but for now just round robin the
    // evictions.
    //

    FailureCount = 0;
    PageCountSinceEvent = 0;
    TotalPagesPaged = 0;
    while (TRUE) {
        if (MmPhysicalPageLock != NULL) {
            KeAcquireQueuedLock(MmPhysicalPageLock);
            LockHeld = TRUE;
        }

        //
        // Keep the goal realistic.
        //

        if (FreePagesTarget > MmTotalPhysicalPages - MmNonPagedPhysicalPages) {
            FreePagesTarget = MmTotalPhysicalPages - MmNonPagedPhysicalPages;
        }

        //
        // If the pager hit its goal (either by its own steam or with the help
        // of outside forces), then break out. Consider it hitting the goal if
        // either free memory rises above the desired line, or the pager has
        // paged enough pages that ought to get to the desired line. Without
        // the second part, the pager may just keep going forever if the goal
        // is too ambitious (with page in paging everything right back in).
        //

        FreePages = MmTotalPhysicalPages - MmTotalAllocatedPhysicalPages;
        if ((FreePages >= FreePagesTarget) ||
            (TotalPagesPaged >= FreePagesTarget)) {

            break;
        }

        //
        // Find a single physical page that can be paged out.
        //

        Segment = MmpFindPhysicalPages(1,
                                       1,
                                       PhysicalMemoryFindPagable,
                                       &SegmentOffset,
                                       &PagesFound);

        if (Segment == NULL) {
            break;
        }

        ASSERT(PagesFound == 1);

        Failure = FALSE;
        PagesPaged = 0;
        PhysicalAddress = Segment->StartAddress + (SegmentOffset << PageShift);
        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        PhysicalPage += SegmentOffset;
        PagingEntry = PhysicalPage->U.PagingEntry;

        ASSERT(PagingEntry != NULL);
        ASSERT(((UINTN)PagingEntry & PHYSICAL_PAGE_FLAG_NON_PAGED) == 0);

        //
        // Snap the image section and offset while the lock is still held to
        // avoid racing with the migrate paging entries function.
        //

        Section = PagingEntry->Section;
        SectionOffset = PagingEntry->U.SectionOffset;
        if (LockHeld != FALSE) {
            KeReleaseQueuedLock(MmPhysicalPageLock);
            LockHeld = FALSE;
        }

        //
        // Try to page this memory out.
        //

        Status = MmpPageOut(PagingEntry,
                            Section,
                            SectionOffset,
                            PhysicalAddress,
                            IoBuffer,
                            SwapRegion,
                            &PagesPaged);

        if (KSUCCESS(Status)) {
            PageCountSinceEvent += PagesPaged;

            //
            // If a reasonable number of pages have been freed up, let
            // everyone try their allocations again.
            //

            if (PageCountSinceEvent >= PAGING_EVENT_SIGNAL_PAGE_COUNT) {
                PageCountSinceEvent = 0;
                KeSignalEvent(MmPagingFreePagesEvent, SignalOptionSignalAll);
            }

        } else if (Status != STATUS_RESOURCE_IN_USE) {
            Failure = TRUE;
        }

        TotalPagesPaged += PagesPaged;

        //
        // If the physical page run failed to be completely paged out, then
        // increment the failure count and stop if too many runs have failed.
        //

        if (Failure != FALSE) {
            FailureCount += 1;
            if (FailureCount >= PHYSICAL_MEMORY_MAX_PAGE_OUT_FAILURE_COUNT) {
                break;
            }
        }
    }

    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(MmPhysicalPageLock);
    }

    //
    // Signal the event if there are any remainders that were paged out.
    //

    if ((PageCountSinceEvent != 0) || (TotalPagesPaged == 0)) {
        KeSignalEvent(MmPagingFreePagesEvent, SignalOptionSignalAll);
    }

    return TotalPagesPaged;
}

//
// --------------------------------------------------------- Internal Functions
//

PPHYSICAL_MEMORY_SEGMENT
MmpFindPhysicalPages (
    UINTN PageCount,
    UINTN PageAlignment,
    PHYSICAL_MEMORY_SEARCH_TYPE SearchType,
    PUINTN SelectedPageOffset,
    PUINTN PagesFound
    )

/*++

Routine Description:

    This routine attempts to find a set of physical pages matching a set of
    criteria.

Arguments:

    PageCount - Supplies the number of consecutive pages needed to be found.

    PageAlignment - Supplies the alignment of the physical allocation, in pages.

    SearchType - Supplies the type of physical memory to search for.

    SelectedPageOffset - Supplies a pointer where the index into the physical
        page database marking the beginning of the allocation will be returned
        on success. Said differently, this will contain an index into the
        physical page array on success.

    PagesFound - Supplies an optional pointer that receives the number of
        suitable pages found.

Return Value:

    Returns a pointer to the memory segment containing a swath of pages
    matching the given criteria.

    NULL if there is not enough contiguous memory to satisfy the request.

--*/

{

    UINTN AlignedSegmentStartPage;
    BOOL ExitCheck;
    BOOL FirstIteration;
    UINTN FirstOffset;
    UINTN Flags;
    PPHYSICAL_MEMORY_SEGMENT LastSegment;
    UINTN LastSegmentOffset;
    UINTN Offset;
    ULONG PageShift;
    PPAGING_ENTRY PagingEntry;
    PPHYSICAL_PAGE PhysicalPage;
    PPHYSICAL_MEMORY_SEGMENT Segment;
    UINTN SegmentPageCount;
    UINTN SpanCount;
    UINTN SpanPageCount;
    PVOID VirtualAddress;
    BOOL VirtualAddressInUse;

    ASSERT(PageAlignment != 0);

    //
    // The caller must hold the physical page lock if it exists.
    //

    ASSERT((MmPhysicalPageLock == NULL) ||
           (KeIsQueuedLockHeld(MmPhysicalPageLock) != FALSE));

    PageShift = MmPageShift();
    if (SearchType == PhysicalMemoryFindPagable) {
        LastSegment = MmLastPagedSegment;
        LastSegmentOffset = MmLastPagedSegmentOffset;

    } else {
        LastSegment = MmLastAllocatedSegment;
        LastSegmentOffset = MmLastAllocatedSegmentOffset;
    }

    Segment = LastSegment;

    //
    // Memory segments had better describe pages, otherwise the alignment here
    // is off.
    //

    ASSERT(((Segment->StartAddress >> PageShift) << PageShift) ==
           Segment->StartAddress);

    //
    // Start from the current page, but align up to the physical address
    // according to the alignment requirements.
    //

    AlignedSegmentStartPage = (Segment->StartAddress >> PageShift) +
                              LastSegmentOffset;

    AlignedSegmentStartPage = ALIGN_RANGE_UP(AlignedSegmentStartPage,
                                             PageAlignment);

    Offset = AlignedSegmentStartPage - (Segment->StartAddress >> PageShift);
    FirstOffset = Offset;
    SegmentPageCount = (Segment->EndAddress - Segment->StartAddress) >>
                       PageShift;

    //
    // Loop while not back at the start.
    //

    FirstIteration = TRUE;
    do {

        //
        // Check to see if it's time to advance to the next segment, either
        // due to walking off of this one or there not being enough space
        // left.
        //

        if ((Offset >= SegmentPageCount) ||
            ((SearchType != PhysicalMemoryFindPagable) &&
             (Offset + PageCount > SegmentPageCount)) ||
            ((SearchType == PhysicalMemoryFindFree) &&
             (Segment->FreePages < PageCount))) {

            //
            // If this segment itself is too small, this is the first segment
            // searched, and the loop has been here before, then stop looking.
            //

            if ((Segment == LastSegment) && (FirstIteration == FALSE)) {
                break;
            }

            FirstIteration = FALSE;
            if (Segment->ListEntry.Next == &MmPhysicalSegmentListHead) {
                Segment = LIST_VALUE(MmPhysicalSegmentListHead.Next,
                                     PHYSICAL_MEMORY_SEGMENT,
                                     ListEntry);

            } else {
                Segment = LIST_VALUE(Segment->ListEntry.Next,
                                     PHYSICAL_MEMORY_SEGMENT,
                                     ListEntry);
            }

            ASSERT(((Segment->StartAddress >> PageShift) << PageShift) ==
                   Segment->StartAddress);

            //
            // Determine the segment page count and aligned offset to start at.
            //

            SegmentPageCount = (Segment->EndAddress - Segment->StartAddress) >>
                               PageShift;

            AlignedSegmentStartPage =
                            ALIGN_RANGE_UP((Segment->StartAddress >> PageShift),
                                           PageAlignment);

            Offset = AlignedSegmentStartPage -
                     (Segment->StartAddress >> PageShift);

            //
            // Do all this checking again, as the next segment may be too small
            // or the alignment may have gone off the end of the segment.
            //

            continue;
        }

        PhysicalPage = (PPHYSICAL_PAGE)(Segment + 1);
        PhysicalPage += Offset;

        //
        // Try to collect the desired number of pages from the current segment.
        // When searching for pagable pages, take as many as are available.
        //

        if (SearchType == PhysicalMemoryFindPagable) {
            SpanPageCount = SegmentPageCount - Offset;
            if (PageCount < SpanPageCount) {
                SpanPageCount = PageCount;
            }

        } else {

            ASSERT(PageCount <= (SegmentPageCount - Offset));

            SpanPageCount = PageCount;
        }

        ExitCheck = FALSE;
        for (SpanCount = 0; SpanCount < SpanPageCount; SpanCount += 1) {
            switch (SearchType) {
            case PhysicalMemoryFindFree:

                //
                // The page isn't suitable if it's allocated.
                //

                if (PhysicalPage->U.Free != PHYSICAL_PAGE_FREE) {
                    ExitCheck = TRUE;
                }

                break;

            case PhysicalMemoryFindPagable:
                Flags = PhysicalPage->U.Flags;

                //
                // Free or non-pagable pages cannot be paged out.
                //

                if ((PhysicalPage->U.Free == PHYSICAL_PAGE_FREE) ||
                    ((Flags & PHYSICAL_PAGE_FLAG_NON_PAGED) != 0)) {

                    ExitCheck = TRUE;

                } else {
                    PagingEntry = PhysicalPage->U.PagingEntry;

                    ASSERT((PagingEntry->Section->Flags &
                            IMAGE_SECTION_DESTROYED) == 0);

                    //
                    // If the paging entry is locked, it cannot be paged out.
                    //

                    if (PagingEntry->U.LockCount != 0) {
                        ExitCheck = TRUE;

                    //
                    // Otherwise mark that the page is being paged out so that
                    // it does not get released in the middle of use.
                    //

                    } else {
                        PagingEntry->U.Flags |= PAGING_ENTRY_FLAG_PAGING_OUT;
                    }
                }

                break;

            //
            // Search for a piece of physical memory that is both free and
            // free in the virtual space. This routine does not make sure that
            // the VA range found to be free will continue to be free, so this
            // type of search can only be perfomed in very controlled
            // environments.
            //

            case PhysicalMemoryFindIdentityMappable:
                if (PhysicalPage->U.Free != PHYSICAL_PAGE_FREE) {
                    ExitCheck = TRUE;

                } else {
                    VirtualAddress = (PVOID)(UINTN)(Segment->StartAddress +
                                                    (Offset << PageShift));

                    VirtualAddressInUse = MmpIsAccountingRangeInUse(
                                                         &MmKernelVirtualSpace,
                                                         VirtualAddress,
                                                         1 << PageShift);

                    if (VirtualAddressInUse != FALSE) {
                        ExitCheck = TRUE;
                    }
                }

                break;

            default:

                ASSERT(FALSE);

                return NULL;
            }

            if (ExitCheck != FALSE) {
                break;
            }

            PhysicalPage += 1;
        }

        //
        // If the right number of pages are available, or this is a search for
        // pagable pages and at least one was found, then get excited and
        // return it. Update the globals for the next search too.
        //

        if ((SpanCount == PageCount) ||
            ((SpanCount != 0) &&
             (SearchType == PhysicalMemoryFindPagable))) {

            //
            // Update the global last segment trackers. It's okay if the offset
            // points off the end of the array because remember the beginning
            // of this loop always checks for its validity before assuming
            // anything (it has to because it may have bumped itself over
            // during alignment).
            //

            if (SearchType == PhysicalMemoryFindPagable) {
                MmLastPagedSegment = Segment;
                MmLastPagedSegmentOffset = Offset + SpanCount;

            } else {
                MmLastAllocatedSegment = Segment;
                MmLastAllocatedSegmentOffset = Offset + SpanCount;
            }

            *SelectedPageOffset = Offset;
            if (PagesFound != NULL) {
                *PagesFound = SpanCount;
            }

            return Segment;
        }

        //
        // If this is a search for pagable memory, skip to the next page. The
        // search above didn't find any free pages, so the span count should be
        // at zero.
        //

        if (SearchType == PhysicalMemoryFindPagable) {

            ASSERT(SpanCount == 0);

            Offset += 1;

        //
        // Otherwise advance to the next (aligned) page in the segment.
        //

        } else {
            Offset += PageAlignment;
        }

    } while ((Segment != LastSegment) || (Offset != FirstOffset));

    return NULL;
}

VOID
MmpInitializePhysicalAllocatorIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each descriptor in the memory descriptor
    list.

Arguments:

    DescriptorList - Supplies a pointer to the descriptor list being iterated
        over.

    Descriptor - Supplies a pointer to the current descriptor.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    None.

--*/

{

    ULONGLONG BaseAddress;
    PPHYSICAL_MEMORY_SEGMENT CurrentSegment;
    BOOL FreePage;
    PHYSICAL_ADDRESS LowestPhysicalAddress;
    PINIT_PHYSICAL_MEMORY_ITERATOR MemoryContext;
    UINTN OutOfBoundsAllocatedPageCount;
    UINTN PageCount;
    UINTN PageShift;
    UINTN PageSize;
    ULONGLONG TrimmedSize;
    UINTN TruncatePageCount;

    LowestPhysicalAddress = 0;
    MemoryContext = Context;
    PageSize = MmPageSize();
    PageShift = MmPageShift();
    if (!IS_PHYSICAL_MEMORY_TYPE(Descriptor->Type)) {
        return;
    }

    //
    // Remove page zero from the memory map if it exists. This needs to be done
    // ASAP to prevent the early memory allocator from grabbing it. If there
    // are no memory constraints and it is a free or temporary mapping, mark it
    // as available. The memory manager will use it once the boot allocations
    // are release.
    //

    if (Descriptor->BaseAddress == 0) {

        ASSERT(MmPhysicalPageZeroAvailable == FALSE);

        if ((MmLowestPhysicalPage == 0) &&
            (IS_MEMORY_FREE_TYPE(Descriptor->Type) ||
             IS_BOOT_TEMPORARY_MEMORY_TYPE(Descriptor->Type))) {

            MmPhysicalPageZeroAvailable = TRUE;
        }

        Descriptor->BaseAddress += PageSize;
        Descriptor->Size -= PageSize;
        DescriptorList->TotalSpace -= PageSize;
        if (IS_MEMORY_FREE_TYPE(Descriptor->Type)) {
            DescriptorList->FreeSpace -= PageSize;
        }
    }

    //
    // If the descriptor has no size, skip it. This could be the original page
    // zero descriptor, or some bogus descriptor.
    //

    if (Descriptor->Size == 0) {
        return;
    }

    //
    // If the total memory pages is valid and that many pages have been
    // initialized, don't go any further.
    //

    OutOfBoundsAllocatedPageCount = 0;
    if ((MemoryContext->TotalMemoryPages != 0) &&
        (MemoryContext->PagesInitialized == MemoryContext->TotalMemoryPages)) {

        //
        // Memory was artificially limited. Record any allocated, non-temporary
        // descriptors that are fully out of bounds.
        //

        if (!IS_MEMORY_FREE_TYPE(Descriptor->Type) &&
            !IS_BOOT_TEMPORARY_MEMORY_TYPE(Descriptor->Type)) {

            ASSERT((Descriptor->Size >> PageShift) <= MAX_UINTN);

            OutOfBoundsAllocatedPageCount += Descriptor->Size >> PageShift;
        }

        goto InitializePhysicalAllocatorIterationRoutineEnd;
    }

    //
    // Record the descriptor size and potentially trim it due to maximum
    // physical memory constraints.
    //

    BaseAddress = Descriptor->BaseAddress;
    TrimmedSize = Descriptor->Size;
    if (MmMaximumPhysicalAddress != 0) {

        //
        // Skip this descriptor if it starts above the maximum physical
        // address.
        //

        if (BaseAddress >= MmMaximumPhysicalAddress) {

            //
            // If the total memory pages is valid, the routine should not have
            // made it this far.
            //

            ASSERT(MemoryContext->TotalMemoryPages == 0);

            goto InitializePhysicalAllocatorIterationRoutineEnd;
        }

        //
        // Trim the descriptor size if it goes above the maximum address.
        //

        if ((BaseAddress + TrimmedSize) > MmMaximumPhysicalAddress) {
            TrimmedSize = MmMaximumPhysicalAddress - BaseAddress;
        }
    }

    //
    // If memory is clipped on the low end, potentially trim the size further.
    //

    if (MmLowestPhysicalPage != 0) {
        LowestPhysicalAddress = MmLowestPhysicalPage << PageShift;

        //
        // Skip the descriptor entirely if it is below the lower bound.
        //

        if ((Descriptor->BaseAddress + TrimmedSize) < LowestPhysicalAddress) {

            //
            // Memory was artificially limited. Record any non-temporary,
            // allocated descriptors that are fully out of bounds.
            //

            if (!IS_MEMORY_FREE_TYPE(Descriptor->Type) &&
                !IS_BOOT_TEMPORARY_MEMORY_TYPE(Descriptor->Type)) {

                ASSERT((TrimmedSize >> PageShift) <= MAX_UINTN);

                OutOfBoundsAllocatedPageCount += TrimmedSize >> PageShift;
            }

            goto InitializePhysicalAllocatorIterationRoutineEnd;
        }

        //
        // Trim the size if this descriptor straddles the low bound.
        //

        if (BaseAddress < LowestPhysicalAddress) {
            TrimmedSize -= LowestPhysicalAddress - BaseAddress;
            BaseAddress = LowestPhysicalAddress;
        }
    }

    //
    // Descriptors had better be page aligned because the total memory
    // is simply summed up, so non-page alignment could cause rounding
    // issues when allocating physical page accounting structures.
    //

    ASSERT(IS_ALIGNED(Descriptor->BaseAddress, PageSize) != FALSE);
    ASSERT(IS_ALIGNED(Descriptor->Size, PageSize) != FALSE);
    ASSERT(IS_ALIGNED(BaseAddress, PageSize) != FALSE);
    ASSERT(IS_ALIGNED(TrimmedSize, PageSize) != FALSE);

    MemoryContext->TotalMemoryBytes += TrimmedSize;

    //
    // If the last memory descriptor and this one are not contiguous,
    // then a new segment is required.
    //

    if ((MemoryContext->LastEnd == 0) ||
        (MemoryContext->LastEnd != BaseAddress)) {

        MemoryContext->TotalSegments += 1;
        if (MemoryContext->CurrentPage != NULL) {
            CurrentSegment =
                        (PPHYSICAL_MEMORY_SEGMENT)(MemoryContext->CurrentPage);

            INSERT_BEFORE(&(CurrentSegment->ListEntry),
                          &(MmPhysicalSegmentListHead));

            CurrentSegment->StartAddress = BaseAddress;
            CurrentSegment->EndAddress = CurrentSegment->StartAddress;
            CurrentSegment->FreePages = 0;
            MemoryContext->CurrentSegment = CurrentSegment;
            MemoryContext->CurrentPage = (PPHYSICAL_PAGE)(CurrentSegment + 1);
        }
    }

    //
    // If the current page is set up, add the descriptor to the physical memory
    // segment. Use the real (non trimmed) descriptor size here. The recorded
    // total memory pages will make sure that only the trimmed amount actually
    // get set in the segments, but the remainder potentially needs to get
    // marked as allocated. Also use the original base address as allocated
    // pages truncated on the low end need to be accounted for.
    //

    if (MemoryContext->CurrentPage != NULL) {
        CurrentSegment = MemoryContext->CurrentSegment;

        ASSERT((Descriptor->Size >> PageShift) <= MAX_UINTN);

        PageCount = (UINTN)(Descriptor->Size >> PageShift);
        FreePage = FALSE;
        if (IS_MEMORY_FREE_TYPE(Descriptor->Type)) {
            FreePage = TRUE;
        }

        if (Descriptor->BaseAddress < LowestPhysicalAddress) {
            CurrentSegment->StartAddress = LowestPhysicalAddress;
            CurrentSegment->EndAddress = CurrentSegment->StartAddress;
            TruncatePageCount = (LowestPhysicalAddress -
                                 Descriptor->BaseAddress) >> PageShift;

            PageCount -= TruncatePageCount;

            //
            // Physical memory must be artificially limited. If the descriptor
            // is not free, add these truncated pages to the total memory
            // counts.
            //

            if ((FreePage == FALSE) &&
                !IS_BOOT_TEMPORARY_MEMORY_TYPE(Descriptor->Type)) {

                OutOfBoundsAllocatedPageCount += TruncatePageCount;
            }
        }

        //
        // Initialize each page in the segment.
        //

        while ((PageCount != 0) &&
               (MemoryContext->PagesInitialized <
                MemoryContext->TotalMemoryPages)) {

            //
            // If the page is not free, mark it as non-paged.
            //

            if (FreePage == FALSE) {
                MemoryContext->CurrentPage->U.Flags =
                                                  PHYSICAL_PAGE_FLAG_NON_PAGED;

                MmTotalAllocatedPhysicalPages += 1;

                ASSERT(MmTotalAllocatedPhysicalPages <=
                       MemoryContext->TotalMemoryPages);

                MmNonPagedPhysicalPages += 1;

            } else {
                MemoryContext->CurrentPage->U.Free = PHYSICAL_PAGE_FREE;
                CurrentSegment->FreePages += 1;
            }

            CurrentSegment->EndAddress += PageSize;
            MemoryContext->CurrentPage += 1;
            PageCount -= 1;
            MemoryContext->PagesInitialized += 1;
        }

        ASSERT(CurrentSegment->EndAddress <= MmMaximumPhysicalAddress);

        //
        // If total physical memory was limited and there are pages left over,
        // add them to the total number of allocated pages if the descriptor is
        // not free.
        //

        if ((PageCount != 0) &&
            (FreePage == FALSE) &&
            !IS_BOOT_TEMPORARY_MEMORY_TYPE(Descriptor->Type)) {

            OutOfBoundsAllocatedPageCount += PageCount;
        }

        MemoryContext->LastEnd = CurrentSegment->EndAddress;

    //
    // Otherwise use the trimmed size and updated base address as only the
    // trimmed amount is really being reported back to the caller.
    //

    } else {
        MemoryContext->LastEnd = BaseAddress + TrimmedSize;
    }

InitializePhysicalAllocatorIterationRoutineEnd:

    //
    // Record any allocated pages that were found to be beyond the bounds of
    // artificial memory limits. Only do this when the total memory page count
    // is valid.
    //

    if ((OutOfBoundsAllocatedPageCount != 0) &&
        (MemoryContext->TotalMemoryPages != 0)) {

        MmTotalAllocatedPhysicalPages += OutOfBoundsAllocatedPageCount;
        MmNonPagedPhysicalPages += OutOfBoundsAllocatedPageCount;
        MemoryContext->TotalMemoryPages += OutOfBoundsAllocatedPageCount;
        MemoryContext->PagesInitialized += OutOfBoundsAllocatedPageCount;
    }

    return;
}

BOOL
MmpUpdatePhysicalMemoryStatistics (
    UINTN PageCount,
    BOOL Allocation
    )

/*++

Routine Description:

    This routine updates the physical memory allocation statistics. The
    physical page lock must be held.

Arguments:

    PageCount - Supplies the number of pages allocated or freed during the
        update period.

    Allocation - Supplies a boolean indicating whether or not to update the
        statistics for an allocation (TRUE) or a free (FALSE).

Return Value:

    Returns TRUE if a significant memory update occurred that requires the
    physical memory event to be signaled. Returns FALSE otherwise.

--*/

{

    BOOL SignalEvent;

    SignalEvent = FALSE;
    if (Allocation != FALSE) {
        MmTotalAllocatedPhysicalPages += PageCount;
        MmNonPagedPhysicalPages += PageCount;

        ASSERT(MmTotalAllocatedPhysicalPages <= MmTotalPhysicalPages);

        //
        // Periodically check to see if memory warnings should be signaled.
        //

        MmPhysicalMemoryAllocationCount += PageCount;
        if (((MmPhysicalMemoryAllocationCount &
              MmPhysicalMemoryWarningCountMask) == 0) ||
            (PageCount >= MmPhysicalMemoryWarningCountMask)) {

            //
            // Check the levels from highest page count to the lowest.
            //

            if ((MmPhysicalMemoryWarningLevel != MemoryWarningLevel1) &&
                (MmTotalAllocatedPhysicalPages >=
                 MmPhysicalMemoryWarningLevel1HighPages)) {

                MmPhysicalMemoryWarningLevel = MemoryWarningLevel1;
                SignalEvent = TRUE;

            } else if ((MmPhysicalMemoryWarningLevel !=
                        MemoryWarningLevel2) &&
                       (MmTotalAllocatedPhysicalPages >=
                        MmPhysicalMemoryWarningLevel2HighPages)) {

                MmPhysicalMemoryWarningLevel = MemoryWarningLevel2;
                SignalEvent = TRUE;
            }
        }

    } else {
        MmTotalAllocatedPhysicalPages -= PageCount;

        ASSERT(MmTotalAllocatedPhysicalPages <= MmTotalPhysicalPages);

        //
        // Periodically check to see if memory warnings should be
        // signaled.
        //

        MmPhysicalMemoryFreeCount += PageCount;
        if (((MmPhysicalMemoryFreeCount &
              MmPhysicalMemoryWarningCountMask) == 0) ||
            (PageCount >= MmPhysicalMemoryWarningCountMask)) {

            //
            // Check levels from the lowest page count to the highest.
            //

            if ((MmPhysicalMemoryWarningLevel == MemoryWarningLevel2) &&
                (MmTotalAllocatedPhysicalPages <
                 MmPhysicalMemoryWarningLevel2LowPages)) {

                SignalEvent = TRUE;
                MmPhysicalMemoryWarningLevel = MemoryWarningLevelNone;

            } else if ((MmPhysicalMemoryWarningLevel ==
                        MemoryWarningLevel1) &&
                       (MmTotalAllocatedPhysicalPages <
                        MmPhysicalMemoryWarningLevel1LowPages)) {

                SignalEvent = TRUE;
                MmPhysicalMemoryWarningLevel = MemoryWarningLevel2;
            }
        }
    }

    return SignalEvent;
}

