/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    pagecach.c

Abstract:

    This module implements support for the I/O page cache.

Author:

    Chris Stevens 11-Sep-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PAGE_CACHE_ALLOCATION_TAG 0x68436750 // 'hCgP'

//
// Define the percentage of the total system physical pages the page cache
// tries to keep free.
//

#define PAGE_CACHE_MEMORY_HEADROOM_PERCENT_TRIGGER 10
#define PAGE_CACHE_MEMORY_HEADROOM_PERCENT_RETREAT 15

//
// Define the target size in percent of total system physical memory that the
// page cache aims for. Below this size, paging out begins in addition to
// shrinking the page cache.
//

#define PAGE_CACHE_MINIMUM_MEMORY_TARGET_PERCENT 33

//
// Define the size in percent of total system physical memory that the page
// cache feels it's entitled to even when memory is tight. Performance simply
// suffers too much if the page cache shrinks to nothing.
//

#define PAGE_CACHE_MINIMUM_MEMORY_PERCENT 7

//
// Define the number of system virtual memory the page cache aims to keep free
// by unmapping page cache entries. This is stored in bytes. There are
// different values for system with small (<4GB) and large (64-bit) system
// virtual memory resources.
//

#define PAGE_CACHE_SMALL_VIRTUAL_HEADROOM_TRIGGER_BYTES (512 * _1MB)
#define PAGE_CACHE_SMALL_VIRTUAL_HEADROOM_RETREAT_BYTES (896 * _1MB)
#define PAGE_CACHE_LARGE_VIRTUAL_HEADROOM_TRIGGER_BYTES (1 * (UINTN)_1GB)
#define PAGE_CACHE_LARGE_VIRTUAL_HEADROOM_RETREAT_BYTES (3 * (UINTN)_1GB)

//
// Set this flag if the page cache entry contains dirty data.
//

#define PAGE_CACHE_ENTRY_FLAG_DIRTY 0x00000001

//
// Set this flag if the page cache entry has been evicted.
//

#define PAGE_CACHE_ENTRY_FLAG_EVICTED 0x00000002

//
// Set this flag if the page cache entry is busy and cannot be removed from the
// page cache tree, typically because it is being used as part of tree
// iteration.
//

#define PAGE_CACHE_ENTRY_FLAG_BUSY 0x00000004

//
// Set this flag if the page cache entry owns the physical page it uses.
//

#define PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER 0x00000008

//
// Define page cache debug flags.
//

#define PAGE_CACHE_DEBUG_INSERTION         0x00000001
#define PAGE_CACHE_DEBUG_LOOKUP            0x00000002
#define PAGE_CACHE_DEBUG_EVICTION          0x00000004
#define PAGE_CACHE_DEBUG_FLUSH             0x00000008
#define PAGE_CACHE_DEBUG_SIZE_MANAGEMENT   0x00000010
#define PAGE_CACHE_DEBUG_MAPPED_MANAGEMENT 0x00000020

//
// Define parameters to help coalesce flushes.
//

#define PAGE_CACHE_FLUSH_MAX _128KB

//
// Define the maximum streak of clean pages the page cache encounters while
// flushing before breaking up a write.
//

#define PAGE_CACHE_FLUSH_MAX_CLEAN_STREAK 4

//
// Define the block expansion count for the page cache entry block allocator.
//

#define PAGE_CACHE_BLOCK_ALLOCATOR_EXPANSION_COUNT 0x40

//
// Define the maximum number of pages that can be used as the minimum number of
// free pages necessary to require page cache flushes to give up in favor of
// removing entries in a low memory situation.
//

#define PAGE_CACHE_LOW_MEMORY_CLEAN_PAGE_MAXIMUM 256

//
// Define the percentage of total physical pages that need to be free before
// the page cache stops cleaning entries to evict entries.
//

#define PAGE_CACHE_LOW_MEMORY_CLEAN_PAGE_MINIMUM_PERCENTAGE 10

//
// Define the portion of the page cache that should be dirty (maximum) as a
// shift.
//

#define PAGE_CACHE_MAX_DIRTY_SHIFT 1

//
// --------------------------------------------------------------------- Macros
//

#define IS_IO_OBJECT_TYPE_LINKABLE(_IoObjectType)     \
    ((_IoObjectType == IoObjectRegularFile) ||        \
     (_IoObjectType == IoObjectSymbolicLink) ||       \
     (_IoObjectType == IoObjectSharedMemoryObject) || \
     (_IoObjectType == IoObjectBlockDevice))

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PAGE_CACHE_STATE {
    PageCacheStateInvalid,
    PageCacheStateClean,
    PageCacheStateDirty,
    PageCacheStateWorkerQueued,
    PageCacheStateWorkerBusy
} PAGE_CACHE_STATE, *PPAGE_CACHE_STATE;

/*++

Structure Description:

    This structure defines a page cache entry.

Members:

    Node - Stores the Red-Black tree node information for this page cache
        entry.

    ListEntry - Stores this page cache entry's list entry in the LRU list and,
        once evicted, the list of entries to destroy.

    FileObject - Stores a pointer to the file object for the device or file to
        which the page cache entry belongs.

    Offset - Stores the offset into the file or device of the cached page.

    PhysicalAddress - Stores the physical address of the page containing the
        cached data.

    VirtualAddress - Stores the virtual address of the page containing the
        cached data.

    BackingEntry - Stores a pointer to a page cache entry that owns the
        physical page used by this page cache entry.

    ReferenceCount - Stores the number of references on this page cache entry.

    Flags - Stores a bitmask of page cache entry flags. See
        PAGE_CACHE_ENTRY_FLAG_* for definitions.

--*/

struct _PAGE_CACHE_ENTRY {
    RED_BLACK_TREE_NODE Node;
    LIST_ENTRY ListEntry;
    PFILE_OBJECT FileObject;
    ULONGLONG Offset;
    PHYSICAL_ADDRESS PhysicalAddress;
    volatile PVOID VirtualAddress;
    PPAGE_CACHE_ENTRY BackingEntry;
    volatile ULONG ReferenceCount;
    volatile ULONG Flags;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

PPAGE_CACHE_ENTRY
IopCreatePageCacheEntry (
    PFILE_OBJECT FileObject,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONGLONG Offset
    );

VOID
IopDestroyPageCacheEntries (
    PLIST_ENTRY ListHead,
    BOOL FailIfLastFileObjectReference
    );

KSTATUS
IopDestroyPageCacheEntry (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    BOOL FailIfLastFileObjectReference
    );

VOID
IopInsertPageCacheEntry (
    PPAGE_CACHE_ENTRY NewEntry,
    PPAGE_CACHE_ENTRY LinkEntry
    );

PPAGE_CACHE_ENTRY
IopLookupPageCacheEntryHelper (
    PFILE_OBJECT FileObject,
    ULONGLONG Offset
    );

VOID
IopPageCacheThread (
    PVOID Parameter
    );

KSTATUS
IopFlushPageCacheBuffer (
    PIO_BUFFER FlushBuffer,
    UINTN FlushSize,
    ULONG Flags
    );

VOID
IopTrimLruPageCacheList (
    BOOL AvoidDestroyingFileObjects
    );

VOID
IopTrimRemovalPageCacheList (
    VOID
    );

VOID
IopRemovePageCacheEntriesFromList (
    PLIST_ENTRY PageCacheListHead,
    PLIST_ENTRY DestroyListHead,
    PUINTN TargetRemoveCount
    );

VOID
IopUnmapLruPageCacheList (
    VOID
    );

BOOL
IopIsIoBufferPageCacheBackedHelper (
    PFILE_OBJECT FileObject,
    PIO_BUFFER IoBuffer,
    ULONGLONG Offset,
    UINTN SizeInBytes
    );

KSTATUS
IopUnmapPageCacheEntry (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    PBOOL PageWasDirty
    );

VOID
IopRemovePageCacheEntryFromTree (
    PPAGE_CACHE_ENTRY PageCacheEntry
    );

VOID
IopRemovePageCacheEntryFromList (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    BOOL Eviction,
    PLIST_ENTRY DestroyListHead
    );

VOID
IopUpdatePageCacheEntryList (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    BOOL Created,
    BOOL Write
    );

BOOL
IopIsPageCacheTooBig (
    PUINTN FreePhysicalPages
    );

BOOL
IopIsPageCacheTooMapped (
    PUINTN FreeVirtualPages
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Stores the tree of page cache entries and the lock to go with it.
//

PSHARED_EXCLUSIVE_LOCK IoPageCacheTreeLock;

//
// Stores the list head for the page cache entries that are ordered from least
// to most recently used. This will mostly contain clean entries, but could
// have a few dirty entries on it.
//

LIST_ENTRY IoPageCacheLruListHead;

//
// Stores the list head for the list of page cache entries that are ready to be
// removed from the cache. These could be evicted entries or entries that
// belong to a deleted file. A deleted file's entries are not necessarily
// marked evicted, thus the evicted count is not always equal to the size of
// this list.
//

LIST_ENTRY IoPageCacheRemovalListHead;

//
// Stores a lock to protect access to the lists of page cache entries.
//

PQUEUED_LOCK IoPageCacheListLock;

//
// The count tracks the current number of entries in the cache's tree. This is
// protected by the tree lock.
//

UINTN IoPageCacheEntryCount = 0;

//
// The evicted count tracks the number of entries that are marked evicted but
// have not been destroyed. They are on the evicted list, but may or may not be
// in the tree. This variable is protected by the list lock.
//

UINTN IoPageCacheEvictedCount = 0;

//
// Store the target number of free pages in the system the page cache shoots
// for once low-memory eviction of page cache entries kicks in.
//

UINTN IoPageCacheHeadroomPagesRetreat = 0;

//
// Store the number of free physical pages in the system at (or below) which
// the page cache will start evicting entries.
//

UINTN IoPageCacheHeadroomPagesTrigger = 0;

//
// Store the size of the page cache (in pages) below which the page cache will
// ask for pages to be paged out in an effort to keep the working set in memory.
//

UINTN IoPageCacheMinimumPagesTarget = 0;

//
// Store the minimum size (in pages) below which the page cache will not
// attempt to shrink.
//

UINTN IoPageCacheMinimumPages = 0;

//
// Store the minimum number of pages that must be clean in a low memory
// scenario before the page cache worker stops flushing entries in favor or
// removing clean entries.
//

UINTN IoPageCacheLowMemoryCleanPageMinimum = 0;

//
// The physical page count tracks the current number of physical pages in use
// by the cache. This includes pages that are active in the tree and pages that
// are not in the tree, awaiting destruction.
//

volatile UINTN IoPageCachePhysicalPageCount = 0;

//
// Stores the number of pages in the cache that are dirty.
//

volatile UINTN IoPageCacheDirtyPageCount = 0;

//
// Stores the number of page cache pages that are currently mapped.
//

volatile UINTN IoPageCacheMappedPageCount = 0;

//
// Stores the number of dirty page cache entries that are currently mapped.
//

volatile UINTN IoPageCacheMappedDirtyPageCount = 0;

//
// Store the target number of free virtual pages in the system the page cache
// shoots for once low-memory unmapping of page cache entries kicks in.
//

UINTN IoPageCacheHeadroomVirtualPagesRetreat = 0;

//
// Store the number of free virtual pages in the system at (or below) which
// the page cache will start unmapping entries.
//

UINTN IoPageCacheHeadroomVirtualPagesTrigger = 0;

//
// Store the timer used to trigger the page cache worker.
//

PKTIMER IoPageCacheWorkTimer;

//
// The page cache state records the current state of the cleaning process.
// This is of type PAGE_CACHE_STATE.
//

volatile ULONG IoPageCacheState;

//
// The next due time records the next due time the page cache should be
// scheduled to run.
//

ULONGLONG IoPageCacheDueTime;
KSPIN_LOCK IoPageCacheDueTimeLock;

//
// This boolean stores whether or not the page cache cleaner should flush dirty
// page cache entries.
//

volatile ULONG IoPageCacheFlushDirtyEntries;

//
// This boolean stores whether or not the page cache cleaner should also flush
// file properties. It always flushes dirty pages.
//

volatile ULONG IoPageCacheFlushProperties;

//
// This boolean indicates that there is a deleted file to remove from the page
// cache.
//

volatile ULONG IoPageCacheDeletedFiles;

//
// This stores the last time the page cache was cleaned.
//

INT64_SYNC IoLastPageCacheCleanTime;

//
// Store a bitfield of enabled page cache debug flags. See PAGE_CACHE_DEBUG_*
// for definitions.
//

ULONG IoPageCacheDebugFlags = 0x0;

//
// Store the global page cache entry block allocator.
//

PBLOCK_ALLOCATOR IoPageCacheBlockAllocator;

//
// Store a pointer to the page cache thread itself.
//

PKTHREAD IoPageCacheThread;

//
// Stores a boolean that can be used to disable page cache entries from storing
// virtual addresses.
//

BOOL IoPageCacheDisableVirtulAddresses = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
ULONG
IoGetCacheEntryDataSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of data stored in each cache entry.

Arguments:

    None.

Return Value:

    Returns the size of the data stored in each cache entry.

--*/

{

    return MmPageSize();
}

KSTATUS
IoGetCacheStatistics (
    PIO_CACHE_STATISTICS Statistics
    )

/*++

Routine Description:

    This routine collects the cache statistics and returns them to the caller.

Arguments:

    Statistics - Supplies a pointer that receives the cache statistics. The
        caller should zero this buffer beforehand and set the version member to
        IO_CACHE_STATISTICS_VERSION. Failure to zero the structure beforehand
        may result in uninitialized data when a driver built for a newer OS is
        run on an older OS.

Return Value:

    Status code.

--*/

{

    ULONGLONG LastCleanTime;

    if (Statistics->Version < IO_CACHE_STATISTICS_VERSION) {
        return STATUS_INVALID_PARAMETER;
    }

    READ_INT64_SYNC(&IoLastPageCacheCleanTime, &LastCleanTime);
    Statistics->EntryCount = IoPageCacheEntryCount;
    Statistics->HeadroomPagesTrigger = IoPageCacheHeadroomPagesTrigger;
    Statistics->HeadroomPagesRetreat = IoPageCacheHeadroomPagesRetreat;
    Statistics->MinimumPagesTarget = IoPageCacheMinimumPagesTarget;
    Statistics->PhysicalPageCount = IoPageCachePhysicalPageCount;
    Statistics->DirtyPageCount = IoPageCacheDirtyPageCount;
    Statistics->LastCleanTime = LastCleanTime;
    return STATUS_SUCCESS;
}

VOID
IoPageCacheEntryAddReference (
    PPAGE_CACHE_ENTRY PageCacheEntry
    )

/*++

Routine Description:

    This routine increments the reference count on the given page cache entry.
    It is assumed that callers of this routine either hold the page cache lock
    or already hold a reference on the given page cache entry.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry whose reference
        count should be incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(PageCacheEntry->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x1000);

    return;
}

VOID
IoPageCacheEntryReleaseReference (
    PPAGE_CACHE_ENTRY PageCacheEntry
    )

/*++

Routine Description:

    This routine decrements the reference count on the given page cache entry.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry whose reference
        count should be incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(PageCacheEntry->ReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x1000));

    return;
}

PHYSICAL_ADDRESS
IoGetPageCacheEntryPhysicalAddress (
    PPAGE_CACHE_ENTRY PageCacheEntry
    )

/*++

Routine Description:

    This routine returns the physical address of the page cache entry.

Arguments:

    PageCacheEntry - Supplies a pointer to a page cache entry.

Return Value:

    Returns the physical address of the given page cache entry.

--*/

{

    return PageCacheEntry->PhysicalAddress;
}

PVOID
IoGetPageCacheEntryVirtualAddress (
    PPAGE_CACHE_ENTRY PageCacheEntry
    )

/*++

Routine Description:

    This routine gets the given page cache entry's virtual address.

Arguments:

    PageCacheEntry - Supplies a pointer to a page cache entry.

Return Value:

    Returns the virtual address of the given page cache entry.

--*/

{

    PPAGE_CACHE_ENTRY BackingEntry;
    PVOID VirtualAddress;

    //
    // If this page cache entry's virtual address is NULL, but it has a mapped
    // backing entry, then synchronize the two.
    //

    VirtualAddress = PageCacheEntry->VirtualAddress;
    BackingEntry = PageCacheEntry->BackingEntry;

    ASSERT((VirtualAddress == NULL) ||
           (BackingEntry == NULL) ||
           (VirtualAddress == BackingEntry->VirtualAddress));

    if ((VirtualAddress == NULL) && (BackingEntry != NULL)) {

        ASSERT((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) == 0);
        ASSERT((BackingEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0);

        //
        // Updating the virtual address in the non-backing entry does not need
        // to be atomic because any race would be to set it to the same value.
        // As only backing entries can be set.
        //

        VirtualAddress = PageCacheEntry->BackingEntry->VirtualAddress;
        PageCacheEntry->VirtualAddress = VirtualAddress;
    }

    return VirtualAddress;
}

BOOL
IoSetPageCacheEntryVirtualAddress (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine attempts to set the virtual address in the given page cache
    entry. It is assumed that the page cache entry's physical address is mapped
    at the given virtual address.

Arguments:

    PageCacheEntry - Supplies as pointer to the page cache entry.

    VirtualAddress - Supplies the virtual address to set in the page cache
        entry.

Return Value:

    Returns TRUE if the set succeeds or FALSE if another virtual address is
    already set for the page cache entry.

--*/

{

    MEMORY_WARNING_LEVEL MemoryWarningLevel;
    PVOID Original;
    BOOL Set;
    PPAGE_CACHE_ENTRY UnmappedEntry;

    ASSERT(IS_POINTER_ALIGNED(VirtualAddress, MmPageSize()) != FALSE);

    if ((PageCacheEntry->VirtualAddress != NULL) ||
        (IoPageCacheDisableVirtulAddresses != FALSE)) {

        return FALSE;
    }

    UnmappedEntry = PageCacheEntry;
    if (UnmappedEntry->BackingEntry != NULL) {
        UnmappedEntry = UnmappedEntry->BackingEntry;
    }

    //
    // If the unmapped entry is still not mapped, then attempt to set the given
    // virtual address.
    //

    Set = FALSE;
    if (UnmappedEntry->VirtualAddress == NULL) {

        ASSERT((UnmappedEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0);

        Original = (PVOID)RtlAtomicCompareExchange(
                           (volatile UINTN *)&(UnmappedEntry->VirtualAddress),
                           (UINTN)VirtualAddress,
                           (UINTN)NULL);

        if (Original == NULL) {
            Set = TRUE;
            RtlAtomicAdd(&IoPageCacheMappedPageCount, 1);

            //
            // Another page cache entry was successfully mapped. Make sure the
            // mapping thread cannot map too many entries. Make it do some
            // cleanup work to unmap clean LRU entries if there is a memory
            // warning level.
            //

            MemoryWarningLevel = MmGetVirtualMemoryWarningLevel();
            if (MemoryWarningLevel != MemoryWarningLevelNone) {
                IopUnmapLruPageCacheList();
            }
        }
    }

    ASSERT(UnmappedEntry->VirtualAddress != NULL);

    //
    // Always set the newly mapped page cache entry's virtual address in the
    // given page cache entry. This is either a write to the same memory or
    // synchronizing a non-backing entry with the newly mapped (or already
    // mapped) backing entry. This update does not need to be atomic because
    // any races will be setting the same value (that of the backing entry).
    //

    PageCacheEntry->VirtualAddress = UnmappedEntry->VirtualAddress;
    return Set;
}

BOOL
IoMarkPageCacheEntryDirty (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    ULONG DirtyOffset,
    ULONG DirtyBytes,
    BOOL MoveToDirtyList
    )

/*++

Routine Description:

    This routine marks the given page cache entry as dirty and extends the
    owning file's size if the page cache entry down not own the page. Supply 0
    for dirty bytes to not alter the file size.

Arguments:

    PageCacheEntry - Supplies a pointer to a page cache entry.

    DirtyOffset - Supplies the offset into the page where the dirty bytes
        start.

    DirtyBytes - Supplies the number of dirty bytes.

    MoveToDirtyList - Supplies a boolean indicating if the page cache entry
        should be moved to the list of dirty page cache entries. This should
        only be set to TRUE in special circumstances where the page was marked
        clean and then failed to be flushed or if the page was found to be
        dirty only after it was unmapped. Normal behavior is that the page
        cache entry migrates to the dirty list during lookup for write
        operations.

Return Value:

    Returns TRUE if it marked the entry dirty or FALSE if the entry was already
    dirty.

--*/

{

    PPAGE_CACHE_ENTRY DirtyEntry;
    ULONGLONG FileSize;
    BOOL MarkedDirty;
    ULONG OldFlags;

    //
    // If this page cache entry does not own the physical page then directly
    // mark the backing entry dirty. This causes the system to skip the flush
    // at this page cache entry's layer. As such, the file size for the owning
    // file object is not appropriately updated. Do it now.
    //

    if ((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) == 0) {
        DirtyEntry = PageCacheEntry->BackingEntry;
        if (DirtyBytes != 0) {
            FileSize = PageCacheEntry->Offset + DirtyOffset + DirtyBytes;
            IopUpdateFileObjectFileSize(PageCacheEntry->FileObject,
                                        FileSize,
                                        TRUE,
                                        TRUE);
        }

    } else {

        ASSERT((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0);

        DirtyEntry = PageCacheEntry;
    }

    OldFlags = RtlAtomicOr32(&(DirtyEntry->Flags), PAGE_CACHE_ENTRY_FLAG_DIRTY);
    if ((OldFlags & PAGE_CACHE_ENTRY_FLAG_DIRTY) == 0) {

        ASSERT((OldFlags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0);

        RtlAtomicAdd(&IoPageCacheDirtyPageCount, 1);
        if (PageCacheEntry->VirtualAddress != NULL) {
            RtlAtomicAdd(&IoPageCacheMappedDirtyPageCount, 1);
        }

        MarkedDirty = TRUE;

        //
        // If requested, move the page to the back of the dirty "list". There
        // is no actual list. Dirty pages are just identified by a NULL next
        // pointer.
        //

        if (MoveToDirtyList != FALSE) {
            KeAcquireQueuedLock(IoPageCacheListLock);

            //
            // Only modify the page cache entry's list entry if it is on a list
            // and not evicted.
            //

            if (((DirtyEntry->Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0) &&
                (DirtyEntry->ListEntry.Next != NULL)) {

                LIST_REMOVE(&(DirtyEntry->ListEntry));
                DirtyEntry->ListEntry.Next = NULL;
            }

            KeReleaseQueuedLock(IoPageCacheListLock);
        }

        IopMarkFileObjectDirty(DirtyEntry->FileObject);

    } else {
        MarkedDirty = FALSE;
    }

    return MarkedDirty;
}

KSTATUS
IopInitializePageCache (
    VOID
    )

/*++

Routine Description:

    This routine initializes the page cache.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PBLOCK_ALLOCATOR BlockAllocator;
    ULONGLONG CurrentTime;
    ULONG PageShift;
    UINTN PhysicalPages;
    KSTATUS Status;
    UINTN TotalPhysicalPages;
    UINTN TotalVirtualMemory;

    INITIALIZE_LIST_HEAD(&IoPageCacheLruListHead);
    INITIALIZE_LIST_HEAD(&IoPageCacheRemovalListHead);
    IoPageCacheListLock = KeCreateQueuedLock();
    if (IoPageCacheListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePageCacheEnd;
    }

    IoPageCacheTreeLock = KeCreateSharedExclusiveLock();
    if (IoPageCacheTreeLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePageCacheEnd;
    }

    //
    // Create a timer to schedule the page cache worker.
    //

    IoPageCacheWorkTimer = KeCreateTimer(PAGE_CACHE_ALLOCATION_TAG);
    if (IoPageCacheWorkTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePageCacheEnd;
    }

    //
    // Create the block allocator for the page cache entry structures.
    //

    BlockAllocator = MmCreateBlockAllocator(
                                    sizeof(PAGE_CACHE_ENTRY),
                                    0,
                                    TRUE,
                                    PAGE_CACHE_BLOCK_ALLOCATOR_EXPANSION_COUNT,
                                    BLOCK_ALLOCATOR_FLAG_TRIM,
                                    PAGE_CACHE_ALLOCATION_TAG);

    if (BlockAllocator == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePageCacheEnd;
    }

    IoPageCacheBlockAllocator = BlockAllocator;

    //
    // Determine an appropriate limit on the size of the page cache based on
    // the total number of physical pages.
    //

    TotalPhysicalPages = MmGetTotalPhysicalPages();
    PhysicalPages = (TotalPhysicalPages *
                     PAGE_CACHE_MEMORY_HEADROOM_PERCENT_RETREAT) /
                    100;

    if (PhysicalPages > MAX_UINTN) {
        PhysicalPages = MAX_UINTN;
    }

    IoPageCacheHeadroomPagesRetreat = PhysicalPages;

    ASSERT(IoPageCacheHeadroomPagesRetreat > 0);

    PhysicalPages = (TotalPhysicalPages *
                     PAGE_CACHE_MEMORY_HEADROOM_PERCENT_TRIGGER) /
                    100;

    if (PhysicalPages > MAX_UINTN) {
        PhysicalPages = MAX_UINTN;
    }

    IoPageCacheHeadroomPagesTrigger = PhysicalPages;

    ASSERT(IoPageCacheHeadroomPagesTrigger > 0);

    PhysicalPages = (TotalPhysicalPages *
                     PAGE_CACHE_MINIMUM_MEMORY_TARGET_PERCENT) /
                    100;

    if (PhysicalPages > MAX_UINTN) {
        PhysicalPages = MAX_UINTN;
    }

    IoPageCacheMinimumPagesTarget = PhysicalPages;
    PhysicalPages = (TotalPhysicalPages * PAGE_CACHE_MINIMUM_MEMORY_PERCENT) /
                    100;

    if (PhysicalPages > MAX_UINTN) {
        PhysicalPages = MAX_UINTN;
    }

    IoPageCacheMinimumPages = PhysicalPages;
    PhysicalPages = (TotalPhysicalPages *
                     PAGE_CACHE_LOW_MEMORY_CLEAN_PAGE_MINIMUM_PERCENTAGE) /
                    100;

    if (PhysicalPages > MAX_UINTN) {
        PhysicalPages = MAX_UINTN;
    }

    IoPageCacheLowMemoryCleanPageMinimum = PhysicalPages;

    ASSERT(IoPageCacheLowMemoryCleanPageMinimum > 0);

    if (IoPageCacheLowMemoryCleanPageMinimum >
        PAGE_CACHE_LOW_MEMORY_CLEAN_PAGE_MAXIMUM) {

        IoPageCacheLowMemoryCleanPageMinimum =
                                      PAGE_CACHE_LOW_MEMORY_CLEAN_PAGE_MAXIMUM;
    }

    //
    // Determine an appropriate limit on the amount of virtual memory the page
    // cache is allowed to consume based on the total amount of system virtual
    // memory.
    //

    PageShift = MmPageShift();
    TotalVirtualMemory = MmGetTotalVirtualMemory();
    if (TotalVirtualMemory < MAX_ULONG) {
        IoPageCacheHeadroomVirtualPagesTrigger =
                  PAGE_CACHE_SMALL_VIRTUAL_HEADROOM_TRIGGER_BYTES >> PageShift;

        IoPageCacheHeadroomVirtualPagesRetreat =
                  PAGE_CACHE_SMALL_VIRTUAL_HEADROOM_RETREAT_BYTES >> PageShift;

    } else {
        IoPageCacheHeadroomVirtualPagesTrigger =
                  PAGE_CACHE_LARGE_VIRTUAL_HEADROOM_TRIGGER_BYTES >> PageShift;

        IoPageCacheHeadroomVirtualPagesRetreat =
                  PAGE_CACHE_LARGE_VIRTUAL_HEADROOM_RETREAT_BYTES >> PageShift;
    }

    IoPageCacheState = PageCacheStateClean;
    IoPageCacheFlushDirtyEntries = FALSE;
    IoPageCacheFlushProperties = FALSE;
    IoPageCacheDeletedFiles = FALSE;
    IoPageCacheDueTime = 0;
    KeInitializeSpinLock(&IoPageCacheDueTimeLock);
    CurrentTime = HlQueryTimeCounter();
    WRITE_INT64_SYNC(&IoLastPageCacheCleanTime, CurrentTime);

    //
    // With success on the horizon, create a thread to handle the background
    // page cache entry removal and flushing work.
    //

    Status = PsCreateKernelThread(IopPageCacheThread,
                                  NULL,
                                  "IopPageCacheThread");

    if (!KSUCCESS(Status)) {
        goto InitializePageCacheEnd;
    }

    Status = STATUS_SUCCESS;

InitializePageCacheEnd:
    if (!KSUCCESS(Status)) {
        if (IoPageCacheListLock != NULL) {
            KeDestroyQueuedLock(IoPageCacheListLock);
            IoPageCacheListLock = NULL;
        }

        if (IoPageCacheTreeLock != NULL) {
            KeDestroySharedExclusiveLock(IoPageCacheTreeLock);
            IoPageCacheTreeLock = NULL;
        }

        if (IoPageCacheWorkTimer != NULL) {
            KeDestroyTimer(IoPageCacheWorkTimer);
            IoPageCacheWorkTimer = NULL;
        }

        if (IoPageCacheBlockAllocator != NULL) {
            MmDestroyBlockAllocator(IoPageCacheBlockAllocator);
            IoPageCacheBlockAllocator = NULL;
        }
    }

    return Status;
}

PPAGE_CACHE_ENTRY
IopLookupPageCacheEntry (
    PFILE_OBJECT FileObject,
    ULONGLONG Offset,
    BOOL Write
    )

/*++

Routine Description:

    This routine searches for a page cache entry based on the file object and
    offset. If found, this routine takes a reference on the page cache entry.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file.

    Offset - Supplies an offset into the file or device.

    Write - Supplies a boolean indicating if the lookup is for a write
        operation (TRUE) or a read operation (FALSE).

Return Value:

    Returns a pointer to the found page cache entry on success, or NULL on
    failure.

--*/

{

    PPAGE_CACHE_ENTRY FoundEntry;

    //
    // Acquire the page cache lock shared to allow multiple look-ups at the
    // same time.
    //

    KeAcquireSharedExclusiveLockShared(IoPageCacheTreeLock);
    FoundEntry = IopLookupPageCacheEntryHelper(FileObject, Offset);
    KeReleaseSharedExclusiveLockShared(IoPageCacheTreeLock);

    //
    // If the entry was found for a write operation, then update its list
    // information.
    //

    if (FoundEntry != NULL) {
        IopUpdatePageCacheEntryList(FoundEntry, FALSE, Write);
    }

    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_LOOKUP) != 0) {
        if (FoundEntry != NULL) {
            RtlDebugPrint("PAGE CACHE: Lookup for file object (0x%08x) at "
                          "offset 0x%I64x succeeded: cache entry 0x%08x, "
                          "physical address 0x%I64x, reference count %d, "
                          "flags 0x%08x.\n",
                          FileObject,
                          Offset,
                          FoundEntry,
                          FoundEntry->PhysicalAddress,
                          FoundEntry->ReferenceCount,
                          FoundEntry->Flags);

        } else {
            RtlDebugPrint("PAGE CACHE: Lookup for file object (0x08%x) at "
                          "offset 0x%I64x failed.\n",
                          FileObject,
                          Offset);
        }
    }

    return FoundEntry;
}

PPAGE_CACHE_ENTRY
IopCreateOrLookupPageCacheEntry (
    PFILE_OBJECT FileObject,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONGLONG Offset,
    PPAGE_CACHE_ENTRY LinkEntry,
    BOOL Write,
    PBOOL EntryCreated
    )

/*++

Routine Description:

    This routine creates a page cache entry and inserts it into the cache. Or,
    if a page cache entry already exists for the supplied file object and
    offset, it returns the existing entry.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file
        that owns the contents of the physical page.

    VirtualAddress - Supplies an optional virtual address of the page.

    PhysicalAddress - Supplies the physical address of the page.

    Offset - Supplies the offset into the file or device where the page is
        from.

    LinkEntry - Supplies an optional pointer to a page cache entry that is
        to share the physical address with this new page cache entry if it gets
        inserted.

    Write - Supplies a boolean indicating if the page cache entry is being
        queried/created for a write operation (TRUE) or a read operation
        (FALSE).

    EntryCreated - Supplies an optional pointer that receives a boolean
        indicating whether or not a new page cache entry was created.

Return Value:

    Returns a pointer to a page cache entry on success, or NULL on failure.

--*/

{

    BOOL Created;
    PPAGE_CACHE_ENTRY ExistingCacheEntry;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    KSTATUS Status;

    ASSERT((LinkEntry == NULL) ||
           (LinkEntry->PhysicalAddress == PhysicalAddress));

    Created = FALSE;

    //
    // Allocate and initialize a new page cache entry.
    //

    PageCacheEntry = IopCreatePageCacheEntry(FileObject,
                                             VirtualAddress,
                                             PhysicalAddress,
                                             Offset);

    if (PageCacheEntry == NULL) {
        goto CreateOrLookupPageCacheEntryEnd;
    }

    //
    // Try to insert the entry. If someone else beat this to the punch, then
    // use the existing cache entry.
    //

    KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);
    ExistingCacheEntry = IopLookupPageCacheEntryHelper(FileObject, Offset);
    if (ExistingCacheEntry == NULL) {
        IopInsertPageCacheEntry(PageCacheEntry, LinkEntry);
        Created = TRUE;
    }

    KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // If an existing entry was found, then release the allocated entry.
    //

    if (Created == FALSE) {
        Status = IopDestroyPageCacheEntry(PageCacheEntry, FALSE);

        //
        // This has to succeed because there's another page cache entry for
        // this file object, so it must not be the last reference and therefore
        // couldn't fail.
        //

        ASSERT(KSUCCESS(Status));

        PageCacheEntry = ExistingCacheEntry;
    }

    //
    // Put the page cache entry on the appropriate list.
    //

    IopUpdatePageCacheEntryList(PageCacheEntry, Created, Write);
    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_INSERTION) != 0) {
        if (Created != FALSE) {
            RtlDebugPrint("PAGE CACHE: Inserted new entry for file object "
                          "(0x%08x) at offset 0x%I64x: cache entry 0x%08x, "
                          "physical address 0x%I64x, reference count %d, "
                          "flags 0x%08x.\n",
                          FileObject,
                          Offset,
                          PageCacheEntry,
                          PageCacheEntry->PhysicalAddress,
                          PageCacheEntry->ReferenceCount,
                          PageCacheEntry->Flags);

        } else {
            RtlDebugPrint("PAGE CACHE: Insert found existing entry for file "
                          "object (0x%08x) at offset 0x%I64x: cache entry "
                          "0x%08x, physical address 0x%I64x, reference count "
                          "%d, flags 0x%08x.\n",
                          FileObject,
                          Offset,
                          PageCacheEntry,
                          PageCacheEntry->PhysicalAddress,
                          PageCacheEntry->ReferenceCount,
                          PageCacheEntry->Flags);
        }
    }

CreateOrLookupPageCacheEntryEnd:
    if (EntryCreated != NULL) {
        *EntryCreated = Created;
    }

    return PageCacheEntry;
}

PPAGE_CACHE_ENTRY
IopCreateAndInsertPageCacheEntry (
    PFILE_OBJECT FileObject,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONGLONG Offset,
    PPAGE_CACHE_ENTRY LinkEntry,
    BOOL Write
    )

/*++

Routine Description:

    This routine creates a page cache entry and inserts it into the cache. The
    caller should be certain that there is not another entry in the cache for
    the same file object and offset and that nothing else is in contention to
    create the same entry.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file
        that owns the contents of the physical page.

    VirtualAddress - Supplies an optional virtual address of the page.

    PhysicalAddress - Supplies the physical address of the page.

    Offset - Supplies the offset into the file or device where the page is
        from.

    LinkEntry - Supplies an optional pointer to a page cache entry that is to
        share the physical address with the new page cache entry.

    Write - Supplies a boolean indicating if the page cache entry is being
        created for a write operation (TRUE) or a read operation (FALSE).

Return Value:

    Returns a pointer to a page cache entry on success, or NULL on failure.

--*/

{

    PPAGE_CACHE_ENTRY PageCacheEntry;

    ASSERT(FileObject->Lock != NULL);
    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE);
    ASSERT((LinkEntry == NULL) ||
           (LinkEntry->PhysicalAddress == PhysicalAddress));

    //
    // Allocate and initialize a new page cache entry.
    //

    PageCacheEntry = IopCreatePageCacheEntry(FileObject,
                                             VirtualAddress,
                                             PhysicalAddress,
                                             Offset);

    if (PageCacheEntry == NULL) {
        goto CreateAndInsertPageCacheEntryEnd;
    }

    //
    // Insert the entry. Nothing should beat this to the punch.
    //

    KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    ASSERT(IopLookupPageCacheEntryHelper(FileObject, Offset) == NULL);

    IopInsertPageCacheEntry(PageCacheEntry, LinkEntry);
    KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // Add the newly created page cach entry to the appropriate list.
    //

    IopUpdatePageCacheEntryList(PageCacheEntry, TRUE, Write);
    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_INSERTION) != 0) {
        RtlDebugPrint("PAGE CACHE: Inserted new entry for file object "
                      "(0x%08x) at offset 0x%I64x: cache entry 0x%08x, "
                      "physical address 0x%I64x, reference count %d, "
                      "flags 0x%08x.\n",
                      FileObject,
                      Offset,
                      PageCacheEntry,
                      PageCacheEntry->PhysicalAddress,
                      PageCacheEntry->ReferenceCount,
                      PageCacheEntry->Flags);
    }

CreateAndInsertPageCacheEntryEnd:
    return PageCacheEntry;
}

KSTATUS
IopCopyAndCacheIoBuffer (
    PFILE_OBJECT FileObject,
    ULONGLONG FileOffset,
    PIO_BUFFER Destination,
    UINTN CopySize,
    PIO_BUFFER Source,
    UINTN SourceSize,
    UINTN SourceCopyOffset,
    BOOL Write,
    PUINTN BytesCopied
    )

/*++

Routine Description:

    This routine iterates over the source buffer, caching each page and copying
    the pages to the destination buffer starting at the given copy offsets and
    up to the given copy size.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file
        that owns the data.

    FileOffset - Supplies an offset into the file that corresponds to the
        beginning of the source I/O buffer.

    Destination - Supplies a pointer to the destination I/O buffer.

    CopySize - Supplies the maximum number of bytes that can be copied
        to the destination.

    Source - Supplies a pointer to the source I/O buffer.

    SourceSize - Supplies the number of bytes in the source that should be
        cached.

    SourceCopyOffset - Supplies the offset into the source buffer where the
        copy to the destination should start.

    Write - Supplies a boolean indicating if the I/O buffer is being cached for
        a write operation (TRUE) or a read operation (FALSE).

    BytesCopied - Supplies a pointer that receives the number of bytes copied
        to the destination buffer.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    BOOL LocalWrite;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PPAGE_CACHE_ENTRY SourceEntry;
    UINTN SourceOffset;
    KSTATUS Status;
    PVOID VirtualAddress;

    *BytesCopied = 0;
    PageSize = MmPageSize();

    ASSERT(IS_ALIGNED(SourceSize, PageSize) != FALSE);
    ASSERT(IS_ALIGNED(CopySize, PageSize) != FALSE);

    Fragment = Source->Fragment;
    FragmentIndex = 0;
    FragmentOffset = 0;
    LocalWrite = FALSE;
    SourceOffset = 0;
    while (SourceSize != 0) {

        ASSERT(FragmentIndex < Source->FragmentCount);
        ASSERT(IS_ALIGNED(Fragment->Size, PageSize) != FALSE);

        //
        // If the current page is meant to be copied for a write operation,
        // then create or lookup the page cache entry for write.
        //

        if ((Write != FALSE) &&
            (SourceOffset == SourceCopyOffset) &&
            (CopySize != 0)) {

            LocalWrite = TRUE;

        } else {
            LocalWrite = FALSE;
        }

        //
        // If the source buffer is already backed by a page cache entry at the
        // current offset, then this new page cache entry should try to
        // reference that entry. Otherwise, it will directly own the physical
        // page.
        //

        SourceEntry = MmGetIoBufferPageCacheEntry(Source, SourceOffset);
        PhysicalAddress = Fragment->PhysicalAddress + FragmentOffset;

        ASSERT((SourceEntry == NULL) ||
               (SourceEntry->PhysicalAddress == PhysicalAddress));

        //
        // Find a virtual address for the page cache entry that is about to be
        // created. Prefer the address in the source's page cache entry, but
        // also use the source I/O buffer's virtual address if present.
        //

        VirtualAddress = NULL;
        if (SourceEntry != NULL) {
            VirtualAddress = SourceEntry->VirtualAddress;
        }

        if ((VirtualAddress == NULL) && (Fragment->VirtualAddress != NULL)) {
            VirtualAddress = Fragment->VirtualAddress + FragmentOffset;

            //
            // If there is a source page cache entry and it had no VA, it is
            // current mapped at the determined VA. Transfer ownership to the
            // page cache entry.
            //

            if (SourceEntry != NULL) {
                IoSetPageCacheEntryVirtualAddress(SourceEntry, VirtualAddress);
            }
        }

        //
        // Try to create a page cache entry for this fragment of the source.
        //

        PageCacheEntry = IopCreateOrLookupPageCacheEntry(FileObject,
                                                         VirtualAddress,
                                                         PhysicalAddress,
                                                         FileOffset,
                                                         SourceEntry,
                                                         LocalWrite,
                                                         &Created);

        if (PageCacheEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CopyAndCacheIoBufferEnd;
        }

        //
        // If a cache entry was created for this physical page and the source
        // was not already backed by the page cache, then the source buffer
        // needs to take a reference on it. Otherwise the source buffer will
        // incorrectly free this physical page. Initialize the source buffer at
        // this offset with the created page cache entry.
        //

        if ((Created != FALSE) && (SourceEntry == NULL)) {
            MmSetIoBufferPageCacheEntry(Source, SourceOffset, PageCacheEntry);
        }

        //
        // If the source offset equals the copy offset, and there is more
        // to "copy", initialize the destination buffer with this page
        // cache entry.
        //

        if ((SourceOffset == SourceCopyOffset) && (CopySize != 0)) {
            MmIoBufferAppendPage(Destination,
                                 PageCacheEntry,
                                 NULL,
                                 INVALID_PHYSICAL_ADDRESS);

            SourceCopyOffset += PageSize;
            CopySize -= PageSize;
            *BytesCopied += PageSize;
        }

        //
        // Always release the reference taken by create or lookup. The I/O
        // buffer initialization routines took the necessary references.
        //

        IoPageCacheEntryReleaseReference(PageCacheEntry);
        FileOffset += PageSize;
        SourceOffset += PageSize;
        SourceSize -= PageSize;
        FragmentOffset += PageSize;

        //
        // If the end of this fragment has been reached, moved to the next.
        //

        if (FragmentOffset == Fragment->Size) {
            Fragment += 1;
            FragmentIndex += 1;
            FragmentOffset = 0;
        }
    }

    Status = STATUS_SUCCESS;

CopyAndCacheIoBufferEnd:
    return Status;
}

KSTATUS
IopFlushPageCacheEntries (
    PFILE_OBJECT FileObject,
    ULONGLONG Offset,
    ULONGLONG Size,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes the page cache entries for the given file object
    starting at the given offset for the requested size. This routine does not
    return until all file data has successfully been written to disk. It does
    not guarantee that file meta-data has been flushed to disk.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file.

    Offset - Supplies the offset from the beginning of the file or device where
        the flush should be done.

    Size - Supplies the size, in bytes, of the region to flush. Supply a value
        of -1 to flush from the given offset to the end of the file.

    Flags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    BOOL AddCacheEntry;
    PPAGE_CACHE_ENTRY BackingEntry;
    BOOL BytesFlushed;
    PPAGE_CACHE_ENTRY CacheEntry;
    BOOL CacheEntryProcessed;
    UINTN CleanStreak;
    LIST_ENTRY DestroyListHead;
    PIO_BUFFER FlushBuffer;
    ULONGLONG FlushNextOffset;
    UINTN FlushSize;
    BOOL LockHeld;
    PRED_BLACK_TREE_NODE Node;
    BOOL PageCacheThread;
    ULONG PageShift;
    ULONG PageSize;
    PAGE_CACHE_ENTRY SearchEntry;
    BOOL SkipEntry;
    KSTATUS Status;
    BOOL ThisEntryInCleanStreak;

    PageCacheThread = FALSE;
    BytesFlushed = FALSE;
    CacheEntry = NULL;
    CleanStreak = 0;
    INITIALIZE_LIST_HEAD(&DestroyListHead);
    FlushBuffer = NULL;
    LockHeld = FALSE;
    PageShift = MmPageShift();
    Status = STATUS_SUCCESS;

    //
    // Only allow the page cache thread to perform writes while holding the
    // file object lock shared.
    //

    if (KeGetCurrentThread() == IoPageCacheThread) {
        PageCacheThread = TRUE;
    }

    ASSERT((Size == -1) || ((Offset + Size) > Offset));

    if (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) == FALSE) {
        goto FlushPageCacheEntriesEnd;
    }

    //
    // Quickly exit if there is nothing to flush.
    //

    if (RED_BLACK_TREE_EMPTY(&(FileObject->PageCacheEntryTree)) != FALSE) {
        goto FlushPageCacheEntriesEnd;
    }

    //
    // Allocate a buffer to support the maximum allowed flush size.
    //

    FlushBuffer = MmAllocateUninitializedIoBuffer(PAGE_CACHE_FLUSH_MAX, 0);
    if (FlushBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto FlushPageCacheEntriesEnd;
    }

    //
    // Loop over the page cache entries for the given file.
    //

    CacheEntryProcessed = FALSE;
    FlushNextOffset = 0;
    FlushSize = 0;
    PageSize = MmPageSize();
    KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // Determine which page cache entry the flush should start on.
    //

    SearchEntry.FileObject = FileObject;
    SearchEntry.Offset = Offset;
    SearchEntry.Flags = 0;
    Node = RtlRedBlackTreeSearchClosest(&(FileObject->PageCacheEntryTree),
                                        &(SearchEntry.Node),
                                        TRUE);

    while (Node != NULL) {
        CacheEntry = RED_BLACK_TREE_VALUE(Node, PAGE_CACHE_ENTRY, Node);
        if ((Size != -1) && (CacheEntry->Offset >= (Offset + Size))) {
            break;
        }

        //
        // Determine if the current entry can be skipped.
        //

        SkipEntry = FALSE;
        BackingEntry = CacheEntry->BackingEntry;
        ThisEntryInCleanStreak = FALSE;

        ASSERT(CacheEntry->FileObject == FileObject);

        //
        // If the file object has been deleted, then just remove the entry and
        // continue.
        //

        if ((FileObject->Properties.HardLinkCount == 0) &&
            (FileObject->PathEntryCount == 0)) {

            Node = RtlRedBlackTreeGetNextNode(&(FileObject->PageCacheEntryTree),
                                              FALSE,
                                              Node);

            IopRemovePageCacheEntryFromTree(CacheEntry);
            IopMarkPageCacheEntryClean(CacheEntry, FALSE);
            IopRemovePageCacheEntryFromList(CacheEntry,
                                            FALSE,
                                            &DestroyListHead);

            continue;
        }

        //
        // If the entry has been evicted, it can be skipped.
        //

        if ((CacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0) {
            SkipEntry = TRUE;

        //
        // If the entry is clean, then it can probably be skipped.
        //

        } else if ((CacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) == 0) {
            SkipEntry = TRUE;

            //
            // If this is a synchronized flush and the backing entry is dirty,
            // then write it out.
            //

            if (((Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) &&
                (BackingEntry != NULL) &&
                ((BackingEntry->Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) != 0)) {

                SkipEntry = FALSE;
            }

            //
            // A certain number of clean pages will be tolerated to batch up
            // writes.
            //

            if ((SkipEntry != FALSE) &&
                (FlushSize != 0) &&
                (CleanStreak < PAGE_CACHE_FLUSH_MAX_CLEAN_STREAK)) {

                CleanStreak += 1;
                SkipEntry = FALSE;
                ThisEntryInCleanStreak = TRUE;
            }

            //
            // If a reference is taken on the clean page then it could become
            // dirty at any time, but non-referenced clean pages should be on
            // a list.
            //

            ASSERT((CacheEntry->ReferenceCount != 0) ||
                   (CacheEntry->ListEntry.Next != NULL));

        //
        // If the entry is not within the bounds of the provided offset and
        // size then it can be skipped.
        //

        } else {
            if ((CacheEntry->Offset + PageSize) <= Offset) {
                SkipEntry = TRUE;

            } else if ((Size != -1) &&
                       (CacheEntry->Offset >= (Offset + Size))) {

                SkipEntry = TRUE;
            }

            //
            // If it's dirty and it counts, then reset any clean streaks.
            //

            if (SkipEntry == FALSE) {
                CleanStreak = 0;
            }
        }

        //
        // If this entry is to be skipped, either loop around or proceed to
        // flush the data that has collected in the flush buffer.
        //

        if (SkipEntry != FALSE) {
            if (FlushSize == 0) {
                Node = RtlRedBlackTreeGetNextNode(
                                             &(FileObject->PageCacheEntryTree),
                                             FALSE,
                                             Node);

                continue;
            }

            //
            // There is a set of dirty pages to flush, go do it, but note that
            // the current page cache entry has already been accounted for.
            //

            CacheEntryProcessed = TRUE;

        //
        // Otherwise it needs to be flushed. Add it to the flush buffer if it
        // is contiguous with what is currently there. Or, flush what is in the
        // buffer and prepare to flush the current entry next time.
        //

        } else {
            if (FlushSize == 0) {
                AddCacheEntry = TRUE;
                CacheEntryProcessed = TRUE;
                FlushNextOffset = CacheEntry->Offset;

            } else if (CacheEntry->Offset == FlushNextOffset) {
                AddCacheEntry = TRUE;
                CacheEntryProcessed = TRUE;

            } else {

                ASSERT(CacheEntryProcessed == FALSE);

                AddCacheEntry = FALSE;
            }

            //
            // Add the cache entry to the flush buffer if necessary,
            // potentially looping again to try to add more pages.
            //

            if (AddCacheEntry != FALSE) {
                MmIoBufferAppendPage(FlushBuffer,
                                     CacheEntry,
                                     NULL,
                                     INVALID_PHYSICAL_ADDRESS);

                FlushSize += PageSize;
                if (FlushSize < PAGE_CACHE_FLUSH_MAX) {
                    FlushNextOffset += PageSize;
                    Node = RtlRedBlackTreeGetNextNode(
                                             &(FileObject->PageCacheEntryTree),
                                             FALSE,
                                             Node);

                    CacheEntryProcessed = FALSE;
                    continue;
                }
            }
        }

        ASSERT(FlushSize != 0);

        //
        // Take a reference on the current entry and release the lock to do
        // the required flushing. Note that it is busy so that it does not get
        // removed from the tree.
        //

        RtlAtomicOr32(&(CacheEntry->Flags), PAGE_CACHE_ENTRY_FLAG_BUSY);
        IoPageCacheEntryAddReference(CacheEntry);
        KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);

        //
        // No need to flush any trailing clean entries on the end. If this
        // cache entry was part of the clean streak but is not in the entries
        // being flushed, then it's not really part of the clean streak.
        //

        if ((CacheEntryProcessed == FALSE) &&
            (ThisEntryInCleanStreak != FALSE)) {

            ASSERT(CleanStreak != 0);

            CleanStreak -= 1;
        }

        ASSERT(FlushSize > (CleanStreak << PageShift));

        FlushSize -= CleanStreak << PageShift;

        //
        // Acquire the file object lock to prevent writes from occurring during
        // this flush operation. The only thread that should be acquiring the
        // lock shared is the page cache thread as asserted above.
        //

        if (FileObject->Lock != NULL) {
            LockHeld = TRUE;
            if (PageCacheThread != FALSE) {
                KeAcquireSharedExclusiveLockShared(FileObject->Lock);

            } else {
                KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
            }
        }

        Status = IopFlushPageCacheBuffer(FlushBuffer, FlushSize, Flags);
        if (LockHeld != FALSE) {
            if (PageCacheThread != FALSE) {
                KeReleaseSharedExclusiveLockShared(FileObject->Lock);

            } else {
                KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
            }

            LockHeld = FALSE;
        }

        if (!KSUCCESS(Status)) {
            if (PageCacheThread == FALSE) {
                goto FlushPageCacheEntriesEnd;
            }

        } else {
            BytesFlushed = TRUE;
        }

        //
        // Prepare the flush buffer to be used again. Do this while the page
        // cache lock is released.
        //

        MmResetIoBuffer(FlushBuffer);
        FlushSize = 0;
        FlushNextOffset = 0;
        CleanStreak = 0;

        //
        // Re-acquire the page cache lock to move to the next entry.
        //

        KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);

        //
        // Clear the busy flag in the cache entry with the current focus.
        //

        RtlAtomicAnd32(&(CacheEntry->Flags), ~PAGE_CACHE_ENTRY_FLAG_BUSY);
        IoPageCacheEntryReleaseReference(CacheEntry);
        CacheEntry = NULL;

        //
        // Determine whether to move on to the next cache entry or not.
        //

        if (CacheEntryProcessed != FALSE) {
            Node = RtlRedBlackTreeGetNextNode(&(FileObject->PageCacheEntryTree),
                                              FALSE,
                                              Node);

            CacheEntryProcessed = FALSE;
        }

        //
        // If this is an attempt to flush the entire cache, check on the memory
        // warning level, it may be necessary to stop the flush and evict some
        // entries. Only do this if the minimum number of pages have been
        // cleaned.
        //

        if (PageCacheThread != FALSE) {
            if ((IopIsPageCacheTooBig(NULL) != FALSE) &&
                ((IoPageCachePhysicalPageCount -
                  IoPageCacheDirtyPageCount) >
                 IoPageCacheLowMemoryCleanPageMinimum)) {

                KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);
                Status = STATUS_TRY_AGAIN;
                goto FlushPageCacheEntriesEnd;
            }
        }
    }

    KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);
    CacheEntry = NULL;

    //
    // If the loop completed and there was something left to flush, do it now.
    //

    if (FlushSize != 0) {
        if (FileObject->Lock != NULL) {
            LockHeld = TRUE;
            if (PageCacheThread != FALSE) {
                KeAcquireSharedExclusiveLockShared(FileObject->Lock);

            } else {
                KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
            }
        }

        Status = IopFlushPageCacheBuffer(FlushBuffer, FlushSize, Flags);
        if (LockHeld != FALSE) {
            if (PageCacheThread != FALSE) {
                KeReleaseSharedExclusiveLockShared(FileObject->Lock);

            } else {
                KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
            }

            LockHeld = FALSE;
        }

        if (!KSUCCESS(Status)) {
            if (PageCacheThread == FALSE) {
                goto FlushPageCacheEntriesEnd;
            }

        } else {
            BytesFlushed = TRUE;
        }
    }

    Status = STATUS_SUCCESS;

FlushPageCacheEntriesEnd:

    ASSERT(LockHeld == FALSE);

    //
    // If writing to a disk and the synchronized flag is not set, then
    // a sync operation will need to be performed on this disk.
    //

    if ((BytesFlushed != FALSE) &&
        (FileObject->Properties.Type == IoObjectBlockDevice) &&
        ((Flags & IO_FLAG_DATA_SYNCHRONIZED) == 0)) {

        IopSynchronizeBlockDevice(FileObject->Device);
    }

    if (CacheEntry != NULL) {
        RtlAtomicAnd32(&(CacheEntry->Flags), ~PAGE_CACHE_ENTRY_FLAG_BUSY);
        IoPageCacheEntryReleaseReference(CacheEntry);
    }

    if (FlushBuffer != NULL) {
        MmFreeIoBuffer(FlushBuffer);
    }

    //
    // Destroy any page cache entries that were removed from the tree and
    // placed on the destroyed list. The flush routine should not be called by
    // any lower layer I/O routines, so it should always be safe to write out
    // file properties if a file objects last reference is released.
    //

    IopDestroyPageCacheEntries(&DestroyListHead, FALSE);
    return Status;
}

VOID
IopEvictPageCacheEntries (
    PFILE_OBJECT FileObject,
    ULONGLONG Offset,
    ULONG Flags
    )

/*++

Routine Description:

    This routine attempts to evict the page cache entries for a given file or
    device, as specified by the file object. The flags specify how aggressive
    this routine should be.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file.

    Flags - Supplies a bitmask of eviction flags. See
        PAGE_CACHE_EVICTION_FLAG_* for definitions.

    Offset - Supplies the starting offset into the file or device after which
        all page cache entries should be evicted.

Return Value:

    None.

--*/

{

    PPAGE_CACHE_ENTRY CacheEntry;
    LIST_ENTRY DestroyListHead;
    PRED_BLACK_TREE_NODE Node;
    ULONG PreviousFlags;
    PAGE_CACHE_ENTRY SearchEntry;

    if (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) == FALSE) {
        return;
    }

    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_EVICTION) != 0) {
        RtlDebugPrint("PAGE CACHE: Evicting entries for file object "
                      "(0x%08x): type %d, reference count %d, path count "
                      "%d, offset 0x%I64x.\n",
                      FileObject,
                      FileObject->Properties.Type,
                      FileObject->ReferenceCount,
                      FileObject->PathEntryCount,
                      Offset);
    }

    //
    // If this is a truncate, then the file object's lock must be held
    // exclusively. It also doesn't make sense to truncate a device.
    //

    ASSERT(((Flags & PAGE_CACHE_EVICTION_FLAG_TRUNCATE) == 0) ||
           ((FileObject->Lock != NULL) &&
            (KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock))));

    //
    // Quickly exit if there is nothing to evict.
    //

    if (RED_BLACK_TREE_EMPTY(&(FileObject->PageCacheEntryTree)) != FALSE) {
        return;
    }

    //
    // Iterate over the file object's tree of page cache entries.
    //

    INITIALIZE_LIST_HEAD(&DestroyListHead);
    KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // Find the page cache entry in the file object's tree that is closest (but
    // greater than or equal) to the given eviction offset.
    //

    SearchEntry.FileObject = FileObject;
    SearchEntry.Offset = Offset;
    SearchEntry.Flags = 0;
    Node = RtlRedBlackTreeSearchClosest(&(FileObject->PageCacheEntryTree),
                                        &(SearchEntry.Node),
                                        TRUE);

    while (Node != NULL) {
        CacheEntry = LIST_VALUE(Node, PAGE_CACHE_ENTRY, Node);
        Node = RtlRedBlackTreeGetNextNode(&(FileObject->PageCacheEntryTree),
                                          FALSE,
                                          Node);

        //
        // Assert this is a cache entry after the eviction offset.
        //

        ASSERT(CacheEntry->Offset >= Offset);

        //
        // If no flags were provided to indicate more forceful behavior, just
        // do a best effort. If there is a reference on the cache entry, that
        // is not from the flush worker (i.e. the busy flag), then skip it.
        //

        if ((Flags == 0) &&
            (CacheEntry->ReferenceCount != 0) &&
            (((CacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_BUSY) == 0) ||
             (CacheEntry->ReferenceCount > 1))) {

            if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_EVICTION) != 0) {
                RtlDebugPrint("PAGE CACHE: Skip evicting entry 0x%08x: file "
                              "object 0x%08x, offset 0x%I64x, physical "
                              "address 0x%I64x, reference count %d, flags "
                              "0x%08x.\n",
                              CacheEntry,
                              CacheEntry->FileObject,
                              CacheEntry->Offset,
                              CacheEntry->PhysicalAddress,
                              CacheEntry->ReferenceCount,
                              CacheEntry->Flags);
            }

            continue;
        }

        //
        // Otherwise mark it as evicted. This does not invalidate the Red-Black
        // tree because there should only ever be one valid cache entry for a
        // given file and offset pair. So, at most this will make this entry
        // equal to other evicted entries for its file and offset pair.
        //

        PreviousFlags = RtlAtomicOr32(&(CacheEntry->Flags),
                                      PAGE_CACHE_ENTRY_FLAG_EVICTED);

        if ((PreviousFlags & PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0) {
            IoPageCacheEvictedCount += 1;
        }

        //
        // Clean the page to keep the statistics accurate. It's been evicted
        // and will not be written out. Don't move it to the clean list, as
        // this routine will place it on either the evicted list or the local
        // destroy list.
        //

        IopMarkPageCacheEntryClean(CacheEntry, FALSE);

        //
        // If the entry is in the middle of a flush iteration (i.e. its busy
        // flag is set), it cannot be removed from the tree as those routines
        // need to iterate to the next entry. Just move it to evicted list and
        // it will get removed the next time the worker runs.
        //

        if ((CacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_BUSY) != 0) {
            IopRemovePageCacheEntryFromList(CacheEntry, TRUE, NULL);
            if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_EVICTION) != 0) {
                RtlDebugPrint("PAGE CACHE: Skip removing entry 0x%08x: file "
                              "object 0x%08x, offset 0x%I64x, physical "
                              "address 0x%I64x, reference count %d, flags "
                              "0x%08x.\n",
                              CacheEntry,
                              CacheEntry->FileObject,
                              CacheEntry->Offset,
                              CacheEntry->PhysicalAddress,
                              CacheEntry->ReferenceCount,
                              CacheEntry->Flags);
            }

            continue;
        }

        //
        // If this is a delete operation, then there should not be any open
        // handles for this file object. Therefore there should be no I/O
        // buffers with references to this file object's page cache entries.
        // Truncate is different, as there may be outstanding handles.
        //

        ASSERT(((Flags & PAGE_CACHE_EVICTION_FLAG_DELETE) == 0) ||
               (CacheEntry->ReferenceCount == 0));

        //
        // Remove the node from the page cache tree. It should not be found on
        // look-up again.
        //

        ASSERT(CacheEntry->Node.Parent != NULL);

        IopRemovePageCacheEntryFromTree(CacheEntry);

        //
        // Remove the cache entry from its current list.
        //

        IopRemovePageCacheEntryFromList(CacheEntry, TRUE, &DestroyListHead);

        //
        // If the reference count is zero, then it was added to the local
        // destroy list. Decrement the evicted count if it was incremented
        // above.
        //

        if ((CacheEntry->ReferenceCount == 0) &&
            ((PreviousFlags & PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0)) {

            IoPageCacheEvictedCount -= 1;
        }
    }

    KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // With the evicted page cache entries removed from the cache, loop through
    // and destroy them. This gets called by truncate and device removal, so
    // releasing the last file object reference and generating additional I/O
    // here should be okay (this should not be in a recursive I/O path).
    //

    IopDestroyPageCacheEntries(&DestroyListHead, FALSE);
    return;
}

BOOL
IopIsIoBufferPageCacheBacked (
    PFILE_OBJECT FileObject,
    PIO_BUFFER IoBuffer,
    ULONGLONG Offset,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine determines whether or not the given I/O buffer with data
    targeting the given file object at the given offset is currently backed by
    the page cache, up to the given size. The caller is expected to synchronize
    with eviction via truncate.

Arguments:

    FileObject - Supplies a pointer to a file object.

    IoBuffer - Supplies a pointer to an I/O buffer.

    Offset - Supplies an offset into the file or device object.

    SizeInBytes - Supplies the number of bytes in the I/O buffer that should be
        cache backed.

Return Value:

    Returns TRUE if the I/O buffer is backed by valid page cache entries, or
    FALSE otherwise.

--*/

{

    BOOL Backed;
    ULONG PageSize;

    ASSERT(IoBuffer->FragmentCount != 0);

    //
    // It is assumed that if the first page of the I/O buffer is backed by the
    // page cache then all pages are backed by the page cache.
    //

    PageSize = MmPageSize();
    Backed = IopIsIoBufferPageCacheBackedHelper(FileObject,
                                                IoBuffer,
                                                Offset,
                                                PageSize);

    //
    // Assert that the assumption above is correct.
    //

    ASSERT((Backed == FALSE) ||
           (IopIsIoBufferPageCacheBackedHelper(FileObject,
                                               IoBuffer,
                                               Offset,
                                               ALIGN_RANGE_UP(SizeInBytes,
                                                              PageSize))));

    return Backed;
}

VOID
IopNotifyPageCacheFilePropertiesUpdate (
    VOID
    )

/*++

Routine Description:

    This routine is used to notify the page cache subsystem that a change to
    file properties occurred. It decides what actions to take.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BOOL OldValue;

    //
    // Try to alert the page cache system that some file properties are dirty.
    // This ensures that the next time the page cache cleaner runs it will
    // flush file properties as well as pages.
    //

    OldValue = RtlAtomicExchange32(&IoPageCacheFlushProperties, TRUE);

    //
    // If this thread toggled the status, then it needs to make sure a cleaning
    // is scheduled.
    //

    if (OldValue == FALSE) {
        IopSchedulePageCacheCleaning(PAGE_CACHE_CLEAN_DELAY_MIN * 2);
    }

    return;
}

VOID
IopNotifyPageCacheFileDeleted (
    VOID
    )

/*++

Routine Description:

    This routine notifies the page cache that a file has been deleted and that
    it can clean up any cached entries for the file.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BOOL OldValue;

    //
    // Attempt to mark that a cacheable file object has been deleted.
    //

    OldValue = RtlAtomicExchange32(&IoPageCacheDeletedFiles, TRUE);

    //
    // If this thread won the race to flip the deleted file status, then it
    // is responsible to clean the cache.
    //

    if (OldValue == FALSE) {
        IopSchedulePageCacheCleaning(PAGE_CACHE_CLEAN_DELAY_MIN);
    }

    return;
}

VOID
IopNotifyPageCacheWrite (
    VOID
    )

/*++

Routine Description:

    This routine is used to notify the page cache subsystem that a write to the
    page cache occurred. It decides what actions to take.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BOOL OldValue;

    //
    // Attempt to mark that there are dirty page cache entries.
    //

    OldValue = RtlAtomicExchange32(&IoPageCacheFlushDirtyEntries, TRUE);

    //
    // If this thread succeeded it toggling the state, then it has to schedule
    // a cleaning.
    //

    if (OldValue == FALSE) {
        IopSchedulePageCacheCleaning(PAGE_CACHE_CLEAN_DELAY_MIN);
    }

    return;
}

VOID
IopSchedulePageCacheCleaning (
    ULONGLONG Delay
    )

/*++

Routine Description:

    This routine schedules a cleaning of the page cache or waits until it is
    sure a cleaning is about to be scheduled by another caller. The only
    guarantee is that it will get cleaned. It may not run exactly within the
    requested delay period.

Arguments:

    Delay - Supplies the desired delay time before the cleaning begins,
        in microseconds.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentDueTime;
    ULONGLONG DueTime;
    PAGE_CACHE_STATE OldState;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // The delay should be at least the minimum.
    //

    if (Delay < PAGE_CACHE_CLEAN_DELAY_MIN) {
        Delay = PAGE_CACHE_CLEAN_DELAY_MIN;
    }

    //
    // Calculate the desired due time.
    //

    DueTime = KeGetRecentTimeCounter();
    DueTime += KeConvertMicrosecondsToTimeTicks(Delay);

    //
    // Try to update the global due time if this due time is earlier than the
    // previously recorded due time.
    //

    KeAcquireSpinLock(&IoPageCacheDueTimeLock);
    if ((IoPageCacheDueTime == 0) || (DueTime < IoPageCacheDueTime)) {
        IoPageCacheDueTime = DueTime;
    }

    KeReleaseSpinLock(&IoPageCacheDueTimeLock);

    //
    // Try to queue the page cache work item by transitioning the state from
    // clean to worker queued.
    //

    while (TRUE) {
        OldState = RtlAtomicCompareExchange32(&IoPageCacheState,
                                              PageCacheStateWorkerQueued,
                                              PageCacheStateClean);

        //
        // If it used to be clean, queue the work item.
        //

        if (OldState == PageCacheStateClean) {

            //
            // Queue the timer using the global due time.
            //

            KeAcquireSpinLock(&IoPageCacheDueTimeLock);
            KeQueueTimer(IoPageCacheWorkTimer,
                         TimerQueueSoftWake,
                         IoPageCacheDueTime,
                         0,
                         0,
                         NULL);

            IoPageCacheDueTime = 0;
            KeReleaseSpinLock(&IoPageCacheDueTimeLock);
            break;

        //
        // If it was marked busy, that means that the worker is in the middle
        // of running. This write or file properties update may have been too
        // late, so try to notify the worker to run again by marking the cache
        // dirty.
        //

        } else if (OldState == PageCacheStateWorkerBusy) {
            OldState = RtlAtomicCompareExchange32(&IoPageCacheState,
                                                  PageCacheStateDirty,
                                                  PageCacheStateWorkerBusy);

            //
            // If the state got back to being clean again. This update needs to
            // try to queue the work item. Loop back around.
            //

            if (OldState == PageCacheStateClean) {
                continue;

            //
            // If the state used to be busy, then the state is now dirty. The
            // worker will see this when it's done and re-queue the work item.
            //

            } else if (OldState == PageCacheStateWorkerBusy) {
                break;

            //
            // If the page cache state is now already dirty or the worker is
            // about to run, then it will flush the update. Exit. In this case
            // do not try to cancel and requeue the timer for earlier. The page
            // cache worker will reschedule work for the minimum delay period.
            //

            } else {
                break;
            }

        //
        // If the worker is already queued, then potentially cancel the timer
        // if the current due time is greater than the requested due time.
        //

        } else if (OldState == PageCacheStateWorkerQueued) {

            //
            // Collect the current due time. If it is less than the requested
            // due time, just exit. If it is yet to be queued, then the queuer
            // will pick up the global due time and satisfy this caller's
            // request. If it is currently queued, but has an earlier due time,
            // then this callers request is satisfied. If it just popped off
            // the queue (and returned 0 for due time), it is about to run, but
            // this caller's request was in time because it saw the state as
            // queued and not busy.
            //

            KeAcquireSpinLock(&IoPageCacheDueTimeLock);
            CurrentDueTime = KeGetTimerDueTime(IoPageCacheWorkTimer);
            if (CurrentDueTime <= DueTime) {
                KeReleaseSpinLock(&IoPageCacheDueTimeLock);
                break;
            }

            //
            // Reaching here means that the timer was still on the queue with
            // a due time that was greater than the requested due time. Try to
            // cancel and reschedule it for earlier. If this fails, then the
            // timer expired. The due time and the requested due time must have
            // been extremely close together. It's good enough.
            //

            Status = KeCancelTimer(IoPageCacheWorkTimer);
            if (!KSUCCESS(Status)) {
                IoPageCacheDueTime = 0;
                KeReleaseSpinLock(&IoPageCacheDueTimeLock);
                break;
            }

            //
            // Schedule the timer for the next due time. No need to change the
            // page cache state.
            //

            KeQueueTimer(IoPageCacheWorkTimer,
                         TimerQueueSoftWake,
                         IoPageCacheDueTime,
                         0,
                         0,
                         NULL);

            IoPageCacheDueTime = 0;
            KeReleaseSpinLock(&IoPageCacheDueTimeLock);
            break;

        //
        // If the state is dirty then the worker is about to re-queue the work
        // item and this caller got its chance to update the global due time.
        //

        } else {

            ASSERT(OldState == PageCacheStateDirty);

            break;
        }
    }

    return;
}

ULONGLONG
IopGetPageCacheEntryOffset (
    PPAGE_CACHE_ENTRY PageCacheEntry
    )

/*++

Routine Description:

    This routine gets the file or device offset of the given page cache entry.

Arguments:

    PageCacheEntry - Supplies a pointer to a page cache entry.

Return Value:

    Returns the file or device offset of the given page cache entry.

--*/

{

    return PageCacheEntry->Offset;
}

BOOL
IopMarkPageCacheEntryClean (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    BOOL MoveToCleanList
    )

/*++

Routine Description:

    This routine marks the given page cache entry as clean.

Arguments:

    PageCacheEntry - Supplies a pointer to a page cache entry.

    MoveToCleanList - Supplies a boolean indicating if the page cache entry
        should be moved to the list of clean page cache entries.

Return Value:

    Returns TRUE if it marked the entry clean or FALSE if the entry was already
    clean.

--*/

{

    BOOL MarkedClean;
    ULONG OldFlags;

    //
    // Marking a page cache entry clean requires having a reference on the
    // entry or holding the tree lock exclusively.
    //

    ASSERT((PageCacheEntry->ReferenceCount != 0) ||
           (KeIsSharedExclusiveLockHeldExclusive(IoPageCacheTreeLock) !=
            FALSE));

    OldFlags = RtlAtomicAnd32(&(PageCacheEntry->Flags),
                              ~PAGE_CACHE_ENTRY_FLAG_DIRTY);

    //
    // Return that this routine marked the page clean based on the old value.
    // Additional decrement the dirty page count if this entry owns the page.
    //

    if ((OldFlags & PAGE_CACHE_ENTRY_FLAG_DIRTY) != 0) {
        if ((OldFlags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0) {
            RtlAtomicAdd(&IoPageCacheDirtyPageCount, (UINTN)-1);
            if (PageCacheEntry->VirtualAddress != NULL) {
                RtlAtomicAdd(&IoPageCacheMappedDirtyPageCount, (UINTN)-1);
            }
        }

        MarkedClean = TRUE;

        //
        // If requested, move the page cache entry to the back of the LRU list;
        // assume that this page has been fairly recently used on account of it
        // having been dirty. If the page is already on a list, then leave it
        // as its current location.
        //

        if (MoveToCleanList != FALSE) {
            KeAcquireQueuedLock(IoPageCacheListLock);
            if (PageCacheEntry->ListEntry.Next == NULL) {
                INSERT_BEFORE(&(PageCacheEntry->ListEntry),
                              &IoPageCacheLruListHead);
            }

            KeReleaseQueuedLock(IoPageCacheListLock);
        }

    } else {
        MarkedClean = FALSE;
    }

    return MarkedClean;
}

KSTATUS
IopCopyIoBufferToPageCacheEntry (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    ULONG PageOffset,
    PIO_BUFFER SourceBuffer,
    UINTN SourceOffset,
    ULONG ByteCount
    )

/*++

Routine Description:

    This routine copies up to a page from the given source buffer to the given
    page cache entry.

Arguments:

    PageCacheEntry - Supplies a pointer to a page cache entry.

    PageOffset - Supplies an offset into the page where the copy should begin.

    SourceBuffer - Supplies a pointer to the source buffer where the data
        originates.

    SourceOffset - Supplies an offset into the the source buffer where the data
        copy should begin.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    Status code.

--*/

{

    IO_BUFFER PageCacheBuffer;
    KSTATUS Status;

    //
    // Initialize the I/O buffer with the page cache entry. This takes an
    // additional reference on the page cache entry.
    //

    Status = MmInitializeIoBuffer(&PageCacheBuffer,
                                  NULL,
                                  INVALID_PHYSICAL_ADDRESS,
                                  0,
                                  IO_BUFFER_FLAG_KERNEL_MODE_DATA);

    if (!KSUCCESS(Status)) {
        goto CopyIoBufferToPageCacheEntryEnd;
    }

    MmIoBufferAppendPage(&PageCacheBuffer,
                         PageCacheEntry,
                         NULL,
                         INVALID_PHYSICAL_ADDRESS);

    //
    // Copy the contents of the source to the page cache entry.
    //

    Status = MmCopyIoBuffer(&PageCacheBuffer,
                            PageOffset,
                            SourceBuffer,
                            SourceOffset,
                            ByteCount);

    if (!KSUCCESS(Status)) {
        goto CopyIoBufferToPageCacheEntryEnd;
    }

    IoMarkPageCacheEntryDirty(PageCacheEntry, PageOffset, ByteCount, FALSE);

CopyIoBufferToPageCacheEntryEnd:
    MmFreeIoBuffer(&PageCacheBuffer);
    return Status;
}

BOOL
IopCanLinkPageCacheEntry (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine determines if the given page cache entry could link with a
    page cache entry for the given file object.

Arguments:

    PageCacheEntry - Supplies a pointer to a page cache entry.

    FileObject - Supplies a pointer to a file object.

Return Value:

    Returns TRUE if the page cache entry could be linked to a page cache entry
    with the given file object or FALSE otherwise.

--*/

{

    IO_OBJECT_TYPE PageCacheType;

    ASSERT(IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE);

    PageCacheType = PageCacheEntry->FileObject->Properties.Type;
    if (IS_IO_OBJECT_TYPE_LINKABLE(PageCacheType) == FALSE) {
        return FALSE;
    }

    if (FileObject->Properties.Type == PageCacheType) {
        return FALSE;
    }

    return TRUE;
}

BOOL
IopLinkPageCacheEntries (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    PPAGE_CACHE_ENTRY LinkEntry
    )

/*++

Routine Description:

    This routine attempts to link the given link entry to the page cache entry
    so that they can begin sharing a physical page that is currently used by
    the link entry.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry whose physical
        address is to be modified. The caller should ensure that its reference
        on this entry does not come from an I/O buffer or else the physical
        address in the I/O buffer would be invalid.

    LinkEntry - Supplies a pointer to page cache entry that currently owns the
        physical page that is to be shared.

Return Value:

    Returns TRUE if the two page cache entries are already connected or if the
    routine is successful. It returns FALSE otherwise and both page cache
    entries should continue to use their own physical pages.

--*/

{

    IO_OBJECT_TYPE EntryType;
    IO_OBJECT_TYPE LinkType;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL Result;
    PVOID VirtualAddress;

    ASSERT(PageCacheEntry->ReferenceCount > 0);
    ASSERT(LinkEntry->ReferenceCount > 0);

    EntryType = PageCacheEntry->FileObject->Properties.Type;
    LinkType = LinkEntry->FileObject->Properties.Type;

    //
    // Page cache entries with the same I/O type are not allowed to be linked.
    //

    if (EntryType == LinkType) {
        return FALSE;
    }

    //
    // Weed out any page cache entries that cannot be linked.
    //

    if ((IS_IO_OBJECT_TYPE_LINKABLE(EntryType) == FALSE) ||
        (IS_IO_OBJECT_TYPE_LINKABLE(LinkType) == FALSE)) {

        return FALSE;
    }

    //
    // If the two entries are already linked, do nothing.
    //

    if ((EntryType == IoObjectBlockDevice) &&
        ((LinkType == IoObjectRegularFile) ||
         (LinkType == IoObjectSymbolicLink) ||
         (LinkType == IoObjectSharedMemoryObject))) {

        if (LinkEntry->BackingEntry == PageCacheEntry) {
            return TRUE;
        }

    } else {

        ASSERT(((EntryType == IoObjectRegularFile) ||
                (EntryType == IoObjectSymbolicLink) ||
                (EntryType == IoObjectSharedMemoryObject)) &&
               (LinkType == IoObjectBlockDevice));

        if (PageCacheEntry->BackingEntry == LinkEntry) {
            return TRUE;
        }
    }

    //
    // If the page cache entry that is to be updated has more than one
    // reference then this cannot proceed.
    //

    if (PageCacheEntry->ReferenceCount != 1) {
        return FALSE;
    }

    //
    // Acquire the page cache tree lock exclusively so that no more references
    // can be taken on the page cache entries then check the reference again to
    // make sure it is OK to proceed.
    //

    PhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    VirtualAddress = NULL;
    KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);
    if (PageCacheEntry->ReferenceCount != 1) {
        Result = FALSE;
        goto LinkPageCacheEntries;
    }

    ASSERT((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0);
    ASSERT((LinkEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0);

    //
    // They should not both be dirty, otherwise dirty page accounting is wrong.
    //

    ASSERT(((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) == 0) ||
           ((LinkEntry->Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) == 0));

    //
    // Save the address of the physical page that is to be released and update
    // the entries to share the link entry's page.
    //

    PhysicalAddress = PageCacheEntry->PhysicalAddress;
    VirtualAddress = PageCacheEntry->VirtualAddress;
    PageCacheEntry->PhysicalAddress = LinkEntry->PhysicalAddress;
    PageCacheEntry->VirtualAddress = LinkEntry->VirtualAddress;

    //
    // Now link the two entries based on their types. Note that nothing should
    // have been able to sneak in and link them since the caller has a
    // reference on both entries.
    //

    if ((EntryType == IoObjectBlockDevice) &&
        ((LinkType == IoObjectRegularFile) ||
         (LinkType == IoObjectSymbolicLink) ||
         (LinkType == IoObjectSharedMemoryObject))) {

        ASSERT(LinkEntry->BackingEntry == NULL);

        IoPageCacheEntryAddReference(PageCacheEntry);
        LinkEntry->BackingEntry = PageCacheEntry;
        RtlAtomicAnd32(&(LinkEntry->Flags), ~PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER);

    } else {

        ASSERT(PageCacheEntry->BackingEntry == NULL);
        ASSERT(((EntryType == IoObjectRegularFile) ||
                (EntryType == IoObjectSymbolicLink) ||
                (EntryType == IoObjectSharedMemoryObject)) &&
               (LinkType == IoObjectBlockDevice));

        IoPageCacheEntryAddReference(LinkEntry);
        PageCacheEntry->BackingEntry = LinkEntry;
        RtlAtomicAnd32(&(PageCacheEntry->Flags),
                       ~PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER);
    }

    Result = TRUE;

LinkPageCacheEntries:
    KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // If the physical page removed from the entry was mapped, unmap it.
    //

    if (VirtualAddress != NULL) {
        PageSize = MmPageSize();
        MmUnmapAddress(VirtualAddress, PageSize);
        RtlAtomicAdd(&IoPageCacheMappedPageCount, (UINTN)-1);
    }

    //
    // If a physical page was removed from the entry, free it.
    //

    if (PhysicalAddress != INVALID_PHYSICAL_ADDRESS) {
        MmFreePhysicalPage(PhysicalAddress);
        RtlAtomicAdd(&IoPageCachePhysicalPageCount, (UINTN)-1);
    }

    return Result;
}

BOOL
IopIsPageCacheTooDirty (
    VOID
    )

/*++

Routine Description:

    This routine determines if the page cache has an uncomfortable number of
    entries in it that are dirty. Dirty entries are dangerous because they
    prevent the page cache from shrinking if memory gets tight.

Arguments:

    None.

Return Value:

    TRUE if the page cache has too many dirty entries and adding new ones
    should generally be avoided.

    FALSE if the page cache is relatively clean.

--*/

{

    UINTN FreePages;
    UINTN IdealSize;
    UINTN MaxDirty;
    BOOL TooDirty;

    //
    // The page cache thread is allowed to make all the pages dirty.
    //

    TooDirty = FALSE;
    if (KeGetCurrentThread() == IoPageCacheThread) {
        TooDirty = IopIsPageCacheTooBig(NULL);
        goto IsPageCacheTooDirtyEnd;
    }

    //
    // Determine the ideal page cache size.
    //

    FreePages = MmGetTotalFreePhysicalPages();
    if (FreePages < IoPageCacheHeadroomPagesRetreat) {
        IdealSize = IoPageCachePhysicalPageCount -
                    (IoPageCacheHeadroomPagesRetreat - FreePages);

    } else {
        IdealSize = IoPageCachePhysicalPageCount +
                    (FreePages - IoPageCacheHeadroomPagesRetreat);
    }

    //
    // Only a portion of that ideal size should be dirty.
    //

    MaxDirty = IdealSize >> PAGE_CACHE_MAX_DIRTY_SHIFT;
    if (IoPageCacheDirtyPageCount >= MaxDirty) {
        TooDirty = TRUE;
        goto IsPageCacheTooDirtyEnd;
    }

IsPageCacheTooDirtyEnd:
    if (TooDirty == FALSE) {
        TooDirty = IopIsPageCacheTooMapped(NULL);
    }

    return TooDirty;
}

COMPARISON_RESULT
IopComparePageCacheEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes contained inside file
    objects.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PPAGE_CACHE_ENTRY FirstEntry;
    ULONG FirstFlags;
    PPAGE_CACHE_ENTRY SecondEntry;
    ULONG SecondFlags;

    FirstEntry = RED_BLACK_TREE_VALUE(FirstNode, PAGE_CACHE_ENTRY, Node);
    SecondEntry = RED_BLACK_TREE_VALUE(SecondNode, PAGE_CACHE_ENTRY, Node);

    ASSERT(FirstEntry->FileObject == SecondEntry->FileObject);

    //
    // If the offsets match, then further compare their flags. One might be
    // evicted. Consider an evicted entry higher.
    //

    if (FirstEntry->Offset == SecondEntry->Offset) {
        FirstFlags = FirstEntry->Flags;
        SecondFlags = SecondEntry->Flags;
        if ((FirstFlags & PAGE_CACHE_ENTRY_FLAG_EVICTED) ==
            (SecondFlags & PAGE_CACHE_ENTRY_FLAG_EVICTED)) {

            return ComparisonResultSame;

        } else if (((FirstFlags & PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0) &&
                   ((SecondFlags & PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0)) {

            return ComparisonResultAscending;
        }

        ASSERT(((FirstFlags & PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0) &&
               ((SecondFlags & PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0));

    //
    // If the first entry has a lower offset, then return ascending.
    //

    } else if (FirstEntry->Offset < SecondEntry->Offset) {
        return ComparisonResultAscending;
    }

    //
    // The second entry must be less than the first. Return descending.
    //

    return ComparisonResultDescending;
}

//
// --------------------------------------------------------- Internal Functions
//

PPAGE_CACHE_ENTRY
IopCreatePageCacheEntry (
    PFILE_OBJECT FileObject,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONGLONG Offset
    )

/*++

Routine Description:

    This routine creates a page cache entry.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file
        that owns the page.

    VirtualAddress - Supplies an optional virtual address for the page.

    PhysicalAddress - Supplies the physical address of the page.

    Offset - Supplies the offset into the file or device where the page is
        from.

Return Value:

    Returns a pointer to a page cache entry on success, or NULL on failure.

--*/

{

    MEMORY_WARNING_LEVEL MemoryWarningLevel;
    PPAGE_CACHE_ENTRY PageCacheEntry;

    ASSERT(IS_ALIGNED(PhysicalAddress, MmPageSize()) != FALSE);
    ASSERT((FileObject->Properties.Type != IoObjectBlockDevice) ||
           (Offset < (FileObject->Properties.BlockSize *
                      FileObject->Properties.BlockCount)));

    //
    // Before allowing the current thread to create a new page cache entry,
    // check the memory warning level and force this thread to do a bit of
    // cleanup work (throttling threads that are madly generating page cache
    // entries). This may be a recursive I/O operation (ie a disk I/O running
    // inside of a file I/O operation). Don't destroy any file objects, as that
    // may cause additional I/O to the same object as is already being written
    // to, causing a deadlock. An example of this is a directory read doing a
    // disk read, which here might release the last reference on a file within
    // that directory, which tries to do a directory write but can't acquire
    // the lock as it's already held further up in this thread.
    //

    MemoryWarningLevel = MmGetPhysicalMemoryWarningLevel();
    if (MemoryWarningLevel != MemoryWarningLevelNone) {
        IopTrimLruPageCacheList(TRUE);
    }

    //
    // By the same logic, if there is a virtual memory warning and the new page
    // cache entry will be mapped, do a bit of cleanup work.
    //

    if (VirtualAddress != NULL) {
        MemoryWarningLevel = MmGetVirtualMemoryWarningLevel();
        if (MemoryWarningLevel != MemoryWarningLevelNone) {
            IopUnmapLruPageCacheList();
        }
    }

    //
    // Allocate and initialize a new page cache entry.
    //

    PageCacheEntry = MmAllocateBlock(IoPageCacheBlockAllocator, NULL);
    if (PageCacheEntry == NULL) {
        goto CreatePageCacheEntryEnd;
    }

    RtlZeroMemory(PageCacheEntry, sizeof(PAGE_CACHE_ENTRY));
    IopFileObjectAddReference(FileObject);
    PageCacheEntry->FileObject = FileObject;
    PageCacheEntry->Offset = Offset;
    PageCacheEntry->PhysicalAddress = PhysicalAddress;
    if (IoPageCacheDisableVirtulAddresses == FALSE) {
        PageCacheEntry->VirtualAddress = VirtualAddress;
    }

    PageCacheEntry->ReferenceCount = 1;

    ASSERT(PageCacheEntry->BackingEntry == NULL);
    ASSERT(PageCacheEntry->Flags == 0);
    ASSERT(PageCacheEntry->Node.Parent == NULL);
    ASSERT(PageCacheEntry->ListEntry.Next == NULL);

CreatePageCacheEntryEnd:
    return PageCacheEntry;
}

VOID
IopDestroyPageCacheEntries (
    PLIST_ENTRY ListHead,
    BOOL FailIfLastFileObjectReference
    )

/*++

Routine Description:

    This routine destroys (or attempts to destroy) a list of page cache entries.
    Page cache entries that are not successfully destroyed will be marked
    evicted and put back on the global removal list for destruction later.

Arguments:

    ListHead - Supplies a pointer to the head of the list of entries to
        destroy.

    FailIfLastFileObjectReference - Supplies a boolean indicating that this
        operation may be inside of recursive I/O, and therefore shouldn't
        release the last reference on a file object (which might generate more
        I/O and deadlock).

Return Value:

    None. All page cache entries on this list are removed and either destroyed
    or put back on the global removal list for destruction later.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN FailCount;
    LIST_ENTRY FailedListHead;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    UINTN RemovedCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    FailCount = 0;
    RemovedCount = 0;
    INITIALIZE_LIST_HEAD(&FailedListHead);
    while (LIST_EMPTY(ListHead) == FALSE) {
        CurrentEntry = ListHead->Next;
        PageCacheEntry = LIST_VALUE(CurrentEntry, PAGE_CACHE_ENTRY, ListEntry);
        LIST_REMOVE(CurrentEntry);

        ASSERT(PageCacheEntry->ReferenceCount == 0);
        ASSERT(PageCacheEntry->Node.Parent == NULL);

        if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_EVICTION) != 0) {
            RtlDebugPrint("PAGE CACHE: Destroy entry 0x%08x: file object "
                          "0x%08x, offset 0x%I64x, physical address 0x%I64x, "
                          "reference count %d, flags 0x%08x.\n",
                          PageCacheEntry,
                          PageCacheEntry->FileObject,
                          PageCacheEntry->Offset,
                          PageCacheEntry->PhysicalAddress,
                          PageCacheEntry->ReferenceCount,
                          PageCacheEntry->Flags);
        }

        Status = IopDestroyPageCacheEntry(PageCacheEntry,
                                          FailIfLastFileObjectReference);

        if (!KSUCCESS(Status)) {
            if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_EVICTION) != 0) {
                RtlDebugPrint("PAGE CACHE: Failed to destroy entry 0x%08x: "
                              "%x.\n",
                              PageCacheEntry,
                              Status);
            }

            FailCount += 1;
            INSERT_BEFORE(&(PageCacheEntry->ListEntry), &FailedListHead);

        } else {
            RemovedCount += 1;
        }
    }

    //
    // Notify the debugger if any page cache entries were destroyed or failed
    // to be destroyed.
    //

    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_SIZE_MANAGEMENT) != 0) {
        if (FailCount != 0) {
            RtlDebugPrint("PAGE CACHE: Failed to destroy %d entries.\n",
                          FailCount);
        }

        if (RemovedCount != 0) {
            RtlDebugPrint("PAGE CACHE: Removed %d entries.\n", RemovedCount);
        }
    }

    //
    // Stick any page cache entries that failed back onto removal list. The
    // page cache thread will pick them up next time around.
    //

    if (LIST_EMPTY(&FailedListHead) == FALSE) {
        KeAcquireQueuedLock(IoPageCacheListLock);
        while (LIST_EMPTY(&FailedListHead) == FALSE) {
            PageCacheEntry = LIST_VALUE(FailedListHead.Next,
                                        PAGE_CACHE_ENTRY,
                                        ListEntry);

            //
            // Failed page cache entries are always evicted.
            //

            ASSERT((PageCacheEntry->Flags &
                    PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0);

            IoPageCacheEvictedCount += 1;

            ASSERT((PageCacheEntry->Flags &
                    PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) == 0);

            LIST_REMOVE(&(PageCacheEntry->ListEntry));
            INSERT_BEFORE(&(PageCacheEntry->ListEntry),
                          &IoPageCacheRemovalListHead);
        }

        KeReleaseQueuedLock(IoPageCacheListLock);
    }

    return;
}

KSTATUS
IopDestroyPageCacheEntry (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    BOOL FailIfLastFileObjectReference
    )

/*++

Routine Description:

    This routine destroys the given page cache entry. It is assumed that the
    page cache entry has already been removed from the cache and that is it
    not dirty.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry.

    FailIfLastFileObjectReference - Supplies a boolean indicating that this
        may be a recursive I/O operation and that the page cache entry should
        not release the final file object reference (as that may cause
        additional I/O which might deadlock).

Return Value:

    Status code indicating if the file object reference (and therefore the page
    cache entry itself) was successfully released or not.

--*/

{

    PPAGE_CACHE_ENTRY BackingEntry;
    PFILE_OBJECT FileObject;
    ULONG PageSize;
    KSTATUS Status;

    FileObject = PageCacheEntry->FileObject;

    ASSERT(((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) == 0) ||
           ((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0));

    ASSERT((PageCacheEntry->ReferenceCount == 0) ||
           (PageCacheEntry->Node.Parent == NULL));

    //
    // If this is the page owner, then free the physical page.
    //

    if ((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0) {
        if (PageCacheEntry->VirtualAddress != NULL) {
            PageSize = MmPageSize();
            MmUnmapAddress(PageCacheEntry->VirtualAddress, PageSize);
            RtlAtomicAdd(&IoPageCacheMappedPageCount, (UINTN)-1);
            PageCacheEntry->VirtualAddress = NULL;
        }

        MmFreePhysicalPage(PageCacheEntry->PhysicalAddress);
        RtlAtomicAdd(&IoPageCachePhysicalPageCount, (UINTN)-1);
        PageCacheEntry->PhysicalAddress = INVALID_PHYSICAL_ADDRESS;

    //
    // Otherwise release the reference on the page cache owner if it exists.
    //

    } else if (PageCacheEntry->BackingEntry != NULL) {
        BackingEntry = PageCacheEntry->BackingEntry;

        //
        // The virtual address must either be NULL or match the backing entry's
        // virtual address. It should never be the case that the backing entry
        // is not mapped while the non-backing entry is mapped.
        //

        ASSERT((PageCacheEntry->VirtualAddress == NULL) ||
               (PageCacheEntry->VirtualAddress ==
                BackingEntry->VirtualAddress));

        ASSERT(PageCacheEntry->PhysicalAddress ==
               BackingEntry->PhysicalAddress);

        IoPageCacheEntryReleaseReference(BackingEntry);
        PageCacheEntry->BackingEntry = NULL;
    }

    //
    // Mark it as evicted.
    //

    if ((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0) {
        RtlAtomicOr32(&(PageCacheEntry->Flags), PAGE_CACHE_ENTRY_FLAG_EVICTED);
    }

    //
    // Release the reference on the system's I/O handle.
    //

    Status = IopFileObjectReleaseReference(FileObject,
                                           FailIfLastFileObjectReference);

    if (!KSUCCESS(Status)) {

        //
        // Mark this page as evicted and not the page owner, since the physical
        // page is gone.
        //

        RtlAtomicOr32(&(PageCacheEntry->Flags), PAGE_CACHE_ENTRY_FLAG_EVICTED);
        RtlAtomicAnd32(&(PageCacheEntry->Flags),
                       ~PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER);

        return Status;
    }

    //
    // With the final reference gone, free the page cache entry.
    //

    MmFreeBlock(IoPageCacheBlockAllocator, PageCacheEntry);
    return Status;
}

VOID
IopInsertPageCacheEntry (
    PPAGE_CACHE_ENTRY NewEntry,
    PPAGE_CACHE_ENTRY LinkEntry
    )

/*++

Routine Description:

    This routine inserts the new page cache entry into the page cache and links
    it to the link entry once it is inserted. This routine assumes that the
    page cache tree lock is held exclusively and that there is not already an
    entry for the same file and offset in the tree.

Arguments:

    NewEntry - Supplies a pointer to the new entry to insert into the page
        cache.

    LinkEntry - Supplies an optional pointer to an existing page cache entry
        to link to the page cache entry.

Return Value:

    None.

--*/

{

    IO_OBJECT_TYPE LinkType;
    IO_OBJECT_TYPE NewType;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoPageCacheTreeLock) != FALSE);
    ASSERT(NewEntry->Flags == 0);

    //
    // Insert the new entry into its file object's tree.
    //

    RtlRedBlackTreeInsert(&(NewEntry->FileObject->PageCacheEntryTree),
                          &(NewEntry->Node));

    IoPageCacheEntryCount += 1;

    //
    // Now link the new entry to the supplied link entry based on their I/O
    // types.
    //

    if (LinkEntry != NULL) {
        LinkType = LinkEntry->FileObject->Properties.Type;
        NewType = NewEntry->FileObject->Properties.Type;

        ASSERT(LinkType != NewType);
        ASSERT(IS_IO_OBJECT_TYPE_LINKABLE(LinkType) != FALSE);
        ASSERT(IS_IO_OBJECT_TYPE_LINKABLE(NewType) != FALSE);
        ASSERT((LinkEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0);
        ASSERT(LinkEntry->PhysicalAddress == NewEntry->PhysicalAddress);
        ASSERT(LinkEntry->VirtualAddress == NewEntry->VirtualAddress);

        if ((LinkType == IoObjectBlockDevice) &&
            ((NewType == IoObjectRegularFile) ||
             (NewType == IoObjectSymbolicLink) ||
             (NewType == IoObjectSharedMemoryObject))) {

            IoPageCacheEntryAddReference(LinkEntry);
            NewEntry->BackingEntry = LinkEntry;

        } else {

            ASSERT(((LinkType == IoObjectRegularFile) ||
                    (LinkType == IoObjectSymbolicLink) ||
                    (LinkType == IoObjectSharedMemoryObject)) &&
                   (NewType == IoObjectBlockDevice));

            IoPageCacheEntryAddReference(NewEntry);
            LinkEntry->BackingEntry = NewEntry;
            RtlAtomicAnd32(&(LinkEntry->Flags),
                           ~PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER);

            NewEntry->Flags = PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER;
        }

    } else {
        if (NewEntry->VirtualAddress != NULL) {
            RtlAtomicAdd(&IoPageCacheMappedPageCount, 1);
        }

        RtlAtomicAdd(&IoPageCachePhysicalPageCount, 1);
        NewEntry->Flags = PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER;
        MmSetPageCacheEntryForPhysicalAddress(NewEntry->PhysicalAddress,
                                              NewEntry);
    }

    return;
}

PPAGE_CACHE_ENTRY
IopLookupPageCacheEntryHelper (
    PFILE_OBJECT FileObject,
    ULONGLONG Offset
    )

/*++

Routine Description:

    This routine searches for a page cache entry based on the file object and
    offset. This routine assumes the page cache lock is held. If found, this
    routine takes a reference on the page cache entry.

Arguments:

    FileObject - Supplies a pointer to the file object for the file or device.

    Offset - Supplies an offset into the file or device.

Return Value:

    Returns a pointer to the found page cache entry on success, or NULL on
    failure.

--*/

{

    PPAGE_CACHE_ENTRY FoundEntry;
    PRED_BLACK_TREE_NODE FoundNode;
    PAGE_CACHE_ENTRY SearchEntry;

    SearchEntry.FileObject = FileObject;
    SearchEntry.Offset = Offset;
    SearchEntry.Flags = 0;
    FoundNode = RtlRedBlackTreeSearch(&(FileObject->PageCacheEntryTree),
                                      &(SearchEntry.Node));

    if (FoundNode == NULL) {
        return NULL;
    }

    FoundEntry = RED_BLACK_TREE_VALUE(FoundNode, PAGE_CACHE_ENTRY, Node);
    IoPageCacheEntryAddReference(FoundEntry);
    return FoundEntry;
}

VOID
IopPageCacheThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine cleans cached pages and removes clean pages if the cache is
    consuming too much memory.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. This
        parameter is not used.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentTime;
    BOOL DeletedEntries;
    ULONGLONG DueTime;
    BOOL FlushDirtyEntries;
    BOOL FlushProperties;
    PAGE_CACHE_STATE NextState;
    PAGE_CACHE_STATE OldState;
    PKEVENT PhysicalMemoryWarningEvent;
    MEMORY_WARNING_LEVEL PhysicalMemoryWarningLevel;
    PVOID SignalingObject;
    KSTATUS Status;
    PKEVENT VirtualMemoryWarningEvent;
    MEMORY_WARNING_LEVEL VirtualMemoryWarningLevel;
    PVOID WaitObjectArray[3];

    Status = STATUS_SUCCESS;
    IoPageCacheThread = KeGetCurrentThread();

    //
    // Get the memory warning events from the memory manager.
    //

    PhysicalMemoryWarningEvent = MmGetPhysicalMemoryWarningEvent();
    VirtualMemoryWarningEvent = MmGetVirtualMemoryWarningEvent();

    ASSERT(PhysicalMemoryWarningEvent != NULL);
    ASSERT(VirtualMemoryWarningEvent != NULL);

    //
    // There are only three objects to wait for and as this is less than the
    // thread's built-in wait blocks, do not pre-allocate a wait block.
    //

    ASSERT(3 < BUILTIN_WAIT_BLOCK_ENTRY_COUNT);

    WaitObjectArray[0] = IoPageCacheWorkTimer;
    WaitObjectArray[1] = PhysicalMemoryWarningEvent;
    WaitObjectArray[2] = VirtualMemoryWarningEvent;

    //
    // Loop forever waiting for either the page cache timer or the memory
    // manager's warning event.
    //

    while (TRUE) {
        Status = ObWaitOnObjects(WaitObjectArray,
                                 3,
                                 0,
                                 WAIT_TIME_INDEFINITE,
                                 NULL,
                                 &SignalingObject);

        ASSERT(KSUCCESS(Status));

        //
        // If the memory manager signaled but not for the warning levels the
        // page cache is interested in, loop back and wait. If one signaled,
        // always check the state of both in case the non-signaling signal was
        // also pulsed.
        //

        if ((SignalingObject == PhysicalMemoryWarningEvent) ||
            (SignalingObject == VirtualMemoryWarningEvent)) {

            PhysicalMemoryWarningLevel = MmGetPhysicalMemoryWarningLevel();
            VirtualMemoryWarningLevel = MmGetVirtualMemoryWarningLevel();
            if ((PhysicalMemoryWarningLevel != MemoryWarningLevel1) &&
                (PhysicalMemoryWarningLevel != MemoryWarningLevel2) &&
                (VirtualMemoryWarningLevel != MemoryWarningLevel1)) {

                continue;
            }

            //
            // The cache's state never reached the queued state. Try to
            // transition it from clean to queued so that it can be safely
            // transitioned to the busy state below.
            //

            OldState = RtlAtomicExchange32(&IoPageCacheState,
                                           PageCacheStateWorkerQueued);

            ASSERT((OldState == PageCacheStateWorkerQueued) ||
                   (OldState == PageCacheStateClean));
        }

        //
        // The page cache worker is about to clean the page cache. Mark it busy
        // now so writes that arrive during this routine's execution get
        // noticed at the end of the routine. They might be missed and the
        // worker needs to run again to make sure they get to disk.
        //

        OldState = RtlAtomicExchange32(&IoPageCacheState,
                                       PageCacheStateWorkerBusy);

        ASSERT(OldState == PageCacheStateWorkerQueued);

        //
        // Always cancel the page cache timer in order to acknowledge it.
        // Even if it did not wake up the thread, it may be queued. It needs
        // to be cancelled now as this thread may queue it or another might
        // queue it once the cache state is transitioned to clean.
        //

        KeCancelTimer(IoPageCacheWorkTimer);

        //
        // The page cache cleaning is about to start. Mark down the current
        // time as the last time the cleaning ran. The leaves a record that an
        // attempt was made to flush any writes that occurred before this time.
        //

        CurrentTime = KeGetRecentTimeCounter();
        WRITE_INT64_SYNC(&IoLastPageCacheCleanTime, CurrentTime);

        //
        // Loop over the process of removing excess entries and flushing
        // dirty entries. The flush code may decided to loop back and remove
        // more excess entries.
        //

        while (TRUE) {
            Status = STATUS_SUCCESS;

            //
            // Blast away the list of page cache entries that are ready for
            // removal.
            //

            IopTrimRemovalPageCacheList();

            //
            // Attempt to trim out some clean page cache entries from the LRU
            // list. This routine should only do any work if memory is tight.
            // This is the root of the page cache thread, so there's never
            // recursive I/O to worry about (so go ahead and destroy file
            // objects).
            //

            IopTrimLruPageCacheList(FALSE);

            //
            // Now that some page cache entries may have been destroyed, and
            // thus unmapped, attempt to unmap page cache entries to keep the
            // virtual address usage in check.
            //

            IopUnmapLruPageCacheList();

            //
            // Check to see if dirty page cache entries need to be flushed or
            // there are files to delete.
            //

            FlushDirtyEntries = RtlAtomicExchange32(
                                                 &IoPageCacheFlushDirtyEntries,
                                                 FALSE);

            DeletedEntries = RtlAtomicExchange32(&IoPageCacheDeletedFiles,
                                                 FALSE);

            FlushProperties = RtlAtomicExchange32(&IoPageCacheFlushProperties,
                                                  FALSE);

            if ((FlushDirtyEntries != FALSE) ||
                (DeletedEntries != FALSE) ||
                (FlushProperties != FALSE)) {

                //
                // Flush the dirty page cache entries. If this fails, mark the
                // page cache dirty again or that there are deleted files.
                //

                Status = IopFlushFileObjects(0, 0);
                if (!KSUCCESS(Status)) {
                    if (FlushDirtyEntries != FALSE) {
                        RtlAtomicExchange32(&IoPageCacheFlushDirtyEntries,
                                            TRUE);
                    }

                    if (DeletedEntries != FALSE) {
                        RtlAtomicExchange32(&IoPageCacheDeletedFiles, TRUE);
                    }

                    if (FlushProperties != FALSE) {
                        RtlAtomicExchange32(&IoPageCacheFlushProperties, TRUE);
                    }

                    if (Status == STATUS_TRY_AGAIN) {
                        continue;
                    }
                }
            }

            break;
        }

        //
        // If this routine failed, then things might still be dirty, try to
        // transition back to dirty.
        //

        if (!KSUCCESS(Status)) {
            NextState = PageCacheStateDirty;

        //
        // Otherwise try to transition to clean. If new writes arrived while
        // this routine was flushing, then they switched the state to dirty and
        // this routine needs to schedule work.
        //

        } else {
            NextState = PageCacheStateClean;
        }

        OldState = RtlAtomicCompareExchange32(&IoPageCacheState,
                                              NextState,
                                              PageCacheStateWorkerBusy);

        ASSERT(OldState != PageCacheStateClean);
        ASSERT(OldState != PageCacheStateWorkerQueued);

        if ((OldState == PageCacheStateDirty) ||
            (NextState == PageCacheStateDirty)) {

            OldState = RtlAtomicExchange32(&IoPageCacheState,
                                           PageCacheStateWorkerQueued);

            ASSERT(OldState == PageCacheStateDirty);

            //
            // Since something is dirty again, clean the cache again after the
            // minimum delay unless the request was for some longer wait period.
            //

            DueTime = KeGetRecentTimeCounter();
            DueTime += KeConvertMicrosecondsToTimeTicks(
                                                   PAGE_CACHE_CLEAN_DELAY_MIN);

            KeAcquireSpinLock(&IoPageCacheDueTimeLock);
            if ((IoPageCacheDueTime != 0) && (DueTime < IoPageCacheDueTime)) {
                DueTime = IoPageCacheDueTime;
            }

            KeQueueTimer(IoPageCacheWorkTimer,
                         TimerQueueSoftWake,
                         DueTime,
                         0,
                         0,
                         NULL);

            IoPageCacheDueTime = 0;
            KeReleaseSpinLock(&IoPageCacheDueTimeLock);
        }
    }

    return;
}

KSTATUS
IopFlushPageCacheBuffer (
    PIO_BUFFER FlushBuffer,
    UINTN FlushSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine flushes the given buffer to the owning file or device. This
    routine assumes that the lock of the file object that owns the page cache
    entries is held in the appropriate mode.

Arguments:

    FlushBuffer - Supplies a pointer to a cache-backed I/O buffer to flush.

    FlushSize - Supplies the number of bytes to flush.

    Flags - Supplies a bitmaks of I/O flags for the flush. See IO_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

{

    UINTN BufferOffset;
    UINTN BytesToWrite;
    PPAGE_CACHE_ENTRY CacheEntry;
    BOOL Clean;
    PFILE_OBJECT FileObject;
    ULONGLONG FileOffset;
    IO_CONTEXT IoContext;
    BOOL MarkedClean;
    ULONG OldFlags;
    ULONG PageSize;
    KSTATUS Status;

    CacheEntry = MmGetIoBufferPageCacheEntry(FlushBuffer, 0);
    FileObject = CacheEntry->FileObject;
    FileOffset = CacheEntry->Offset;
    PageSize = MmPageSize();

    ASSERT(FlushSize <= PAGE_CACHE_FLUSH_MAX);
    ASSERT((FileObject->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeld(FileObject->Lock) != FALSE));

    //
    // Try to mark all the pages clean. If they are all already clean, then
    // just exit. Something is already performing the I/O. Also make sure that
    // all the supplied page cache entries are still in the cache. If an
    // evicted entry is found, do not write any data from that page or further;
    // the file was truncated.
    //

    BufferOffset = 0;
    BytesToWrite = 0;
    Clean = TRUE;
    while (BufferOffset < FlushSize) {
        CacheEntry = MmGetIoBufferPageCacheEntry(FlushBuffer, BufferOffset);
        if ((CacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0) {
            break;
        }

        MarkedClean = IopMarkPageCacheEntryClean(CacheEntry, TRUE);
        if (MarkedClean != FALSE) {
            Clean = FALSE;
        }

        BytesToWrite += PageSize;
        BufferOffset += PageSize;
    }

    //
    // If there are no bytes to write, because all the pages got evicted, then
    // exit now.
    //

    if (BytesToWrite == 0) {
        Status = STATUS_SUCCESS;
        goto FlushPageCacheBufferEnd;
    }

    //
    // Exit now if it was already clean, unless this is synchronized I/O. It
    // could be that the backing entries are what require flushing and this
    // layer does not have jurisdiction to mark them clean.
    //

    if ((Clean != FALSE) && ((Flags & IO_FLAG_DATA_SYNCHRONIZED) == 0)) {
        Status = STATUS_SUCCESS;
        goto FlushPageCacheBufferEnd;
    }

    IoContext.IoBuffer = FlushBuffer;
    IoContext.Offset = FileOffset;
    IoContext.SizeInBytes = BytesToWrite;
    IoContext.Flags = Flags;
    IoContext.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    IoContext.Write = TRUE;
    Status = IopPerformNonCachedWrite(FileObject, &IoContext, NULL, FALSE);
    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_FLUSH) != 0) {
        if ((!KSUCCESS(Status)) || (Flags != 0) ||
            (IoContext.BytesCompleted != BytesToWrite)) {

            RtlDebugPrint("PAGE CACHE: Flushed FILE_OBJECT 0x%08x "
                          "with status 0x%08x: flags 0x%x, file offset "
                          "0x%I64x, bytes attempted 0x%x, bytes completed "
                          "0x%x.\n",
                          FileObject,
                          Status,
                          Flags,
                          FileOffset,
                          BytesToWrite,
                          IoContext.BytesCompleted);

        } else {
            RtlDebugPrint("PAGE CACHE: Flushed FILE_OBJECT %x "
                          "Offset 0x%I64x Size 0x%x\n",
                          FileObject,
                          FileOffset,
                          BytesToWrite);
        }
    }

    if (!KSUCCESS(Status))  {
        goto FlushPageCacheBufferEnd;
    }

    if (IoContext.BytesCompleted != BytesToWrite) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto FlushPageCacheBufferEnd;
    }

    Status = STATUS_SUCCESS;

FlushPageCacheBufferEnd:
    if (!KSUCCESS(Status)) {

        //
        // Mark the non-written pages as dirty. Do this directly as opposed to
        // using the helper function because the write may need to go through
        // the upper layer again, for instance to increase the file size in the
        // file system. The helper function would have just marked the lowest
        // layer dirty, leaving the file system with a "clean" page beyond the
        // end of the file.
        //

        BufferOffset = ALIGN_RANGE_DOWN(IoContext.BytesCompleted, PageSize);
        while (BufferOffset < BytesToWrite) {
            CacheEntry = MmGetIoBufferPageCacheEntry(FlushBuffer, BufferOffset);
            OldFlags = RtlAtomicOr32(&(CacheEntry->Flags),
                                     PAGE_CACHE_ENTRY_FLAG_DIRTY);

            if ((OldFlags & PAGE_CACHE_ENTRY_FLAG_DIRTY) == 0) {
                if ((OldFlags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0) {
                    RtlAtomicAdd(&IoPageCacheDirtyPageCount, 1);
                    if (CacheEntry->VirtualAddress != NULL) {
                        RtlAtomicAdd(&IoPageCacheMappedDirtyPageCount, 1);
                    }
                }

                //
                // Carefully try to remove the page cache entry from the LRU
                // list as it is now dirty again. But, if it got evicted during
                // the write, leave it be; another thread is moving it towards
                // destruction.
                //

                KeAcquireQueuedLock(IoPageCacheListLock);
                if (((CacheEntry->Flags &
                      PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0) &&
                    (CacheEntry->ListEntry.Next != NULL)) {

                    LIST_REMOVE(&(CacheEntry->ListEntry));
                    CacheEntry->ListEntry.Next = NULL;
                }

                KeReleaseQueuedLock(IoPageCacheListLock);
            }

            BufferOffset += PageSize;
        }

        if (IoContext.BytesCompleted != BytesToWrite) {
            IopMarkFileObjectDirty(CacheEntry->FileObject);
            IopNotifyPageCacheWrite();
        }
    }

    return Status;
}

VOID
IopTrimLruPageCacheList (
    BOOL AvoidDestroyingFileObjects
    )

/*++

Routine Description:

    This routine removes as many clean page cache entries as is necessary to
    bring the size of the page cache back down to a reasonable level. It evicts
    the page cache entries in LRU order.

Arguments:

    AvoidDestroyingFileObjects - Supplies a boolean indicating whether to
        avoid releasing the last reference on any file object as a result of
        trying to destroy page cache entries. Callers that may be doing I/O
        recursively should set this flag. The root page cache thread does not
        set this flag, so gutted page cache entries always get cleaned up
        there.

Return Value:

    None.

--*/

{

    LIST_ENTRY DestroyListHead;
    UINTN FreePageTarget;
    UINTN FreePhysicalPages;
    UINTN PageOutCount;
    UINTN TargetRemoveCount;

    TargetRemoveCount = 0;
    FreePhysicalPages = -1;
    if (IopIsPageCacheTooBig(&FreePhysicalPages) == FALSE) {
        goto TrimLruPageCacheListEnd;
    }

    //
    // The page cache is not leaving enough free physical pages; determine how
    // many entries must be evicted.
    //

    ASSERT(FreePhysicalPages < IoPageCacheHeadroomPagesRetreat);

    TargetRemoveCount = IoPageCacheHeadroomPagesRetreat -
                        FreePhysicalPages;

    if (TargetRemoveCount > IoPageCachePhysicalPageCount) {
        TargetRemoveCount = IoPageCachePhysicalPageCount;
    }

    if (IoPageCachePhysicalPageCount - TargetRemoveCount <
        IoPageCacheMinimumPages) {

        TargetRemoveCount = IoPageCachePhysicalPageCount -
                            IoPageCacheMinimumPages;
    }

    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_SIZE_MANAGEMENT) != 0) {
        RtlDebugPrint("PAGE CACHE: Attempt to remove at least %lu entries.\n",
                      TargetRemoveCount);
    }

    //
    // Iterate over the clean LRU page cache list trying to find which page
    // cache entries can be removed. Stop as soon as the target count has been
    // reached.
    //

    INITIALIZE_LIST_HEAD(&DestroyListHead);
    KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);
    IopRemovePageCacheEntriesFromList(&IoPageCacheLruListHead,
                                      &DestroyListHead,
                                      &TargetRemoveCount);

    KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // Destroy the evicted page cache entries. This will reduce the page
    // cache's physical page count for any page that it ends up releasing.
    //

    IopDestroyPageCacheEntries(&DestroyListHead, AvoidDestroyingFileObjects);

TrimLruPageCacheListEnd:

    //
    // If the page cache is smaller than its target, ask MM to page out some
    // things so the page cache can grow back up to its target. This throws
    // pageable data into the mix, so if a process allocates a boatload of
    // memory, the page cache doesn't shrink to a dot and constantly lose the
    // working set of the process.
    //

    if ((TargetRemoveCount != 0) &&
        (IoPageCachePhysicalPageCount < IoPageCacheMinimumPagesTarget)) {

        PageOutCount = IoPageCacheMinimumPagesTarget -
                       IoPageCachePhysicalPageCount;

        FreePageTarget = FreePhysicalPages + PageOutCount;
        if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_SIZE_MANAGEMENT) != 0) {
            RtlDebugPrint("PAGE CACHE: Requesting page out: 0x%I64x\n",
                          PageOutCount);
        }

        MmRequestPagingOut(FreePageTarget);
    }

    return;
}

VOID
IopTrimRemovalPageCacheList (
    VOID
    )

/*++

Routine Description:

    This routine removes the page cache entries from the list of page cache
    entries that are ready for removal.

Arguments:

    None.

Return Value:

    None.

--*/

{

    LIST_ENTRY DestroyListHead;

    if (LIST_EMPTY(&(IoPageCacheRemovalListHead)) != FALSE) {
        return;
    }

    INITIALIZE_LIST_HEAD(&DestroyListHead);
    KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);
    IopRemovePageCacheEntriesFromList(&IoPageCacheRemovalListHead,
                                      &DestroyListHead,
                                      NULL);

    KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // Destroy the evicted page cache entries. This will reduce the page
    // cache's physical page count for any page that it ends up releasing.
    //

    IopDestroyPageCacheEntries(&DestroyListHead, FALSE);
    return;
}

VOID
IopRemovePageCacheEntriesFromList (
    PLIST_ENTRY PageCacheListHead,
    PLIST_ENTRY DestroyListHead,
    PUINTN TargetRemoveCount
    )

/*++

Routine Description:

    This routine processes page cache entries in the given list, removing them
    from the tree and the list, if possible. If a target remove count is
    supplied, then the removal process will stop as soon as the removal count
    reaches 0 or the end of the list is reached. This routine assumes that the
    page cache tree lock is held exclusively.

Arguments:

    PageCacheListHead - Supplies a pointer to the head of the page cache list.

    DestroyListHead - Supplies a pointer to the head of the list of page cache
        entries that can be destroyed as a result of the removal process.

    TargetRemoveCount - Supplies an optional pointer to the number of page
        cache entries the caller wishes to remove from the list. On return, it
        will store the difference between the target and the actual number of
        page cache entries removed. If not supplied, then the routine will
        process the entire list looking for page cache entries to remove.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PFILE_OBJECT FileObject;
    PPAGE_CACHE_ENTRY FirstWithReference;
    ULONG Flags;
    BOOL MarkedDirty;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    BOOL PageWasDirty;
    KSTATUS Status;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoPageCacheTreeLock) != FALSE);

    FirstWithReference = NULL;
    KeAcquireQueuedLock(IoPageCacheListLock);
    CurrentEntry = PageCacheListHead->Next;
    while ((CurrentEntry != PageCacheListHead) &&
           ((TargetRemoveCount == NULL) || (*TargetRemoveCount != 0))) {

        PageCacheEntry = LIST_VALUE(CurrentEntry, PAGE_CACHE_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FileObject = PageCacheEntry->FileObject;
        Flags = PageCacheEntry->Flags;

        //
        // Skip over all page cache entries with references, moving them to the
        // back of the list. They cannot be removed at the moment. Break out if
        // the first page cache entry with a reference has been seen twice.
        //

        if (PageCacheEntry == FirstWithReference) {
            break;
        }

        if (PageCacheEntry->ReferenceCount != 0) {
            if (FirstWithReference == NULL) {
                FirstWithReference = PageCacheEntry;
            }

            LIST_REMOVE(&(PageCacheEntry->ListEntry));
            INSERT_BEFORE(&(PageCacheEntry->ListEntry), PageCacheListHead);
            continue;
        }

        //
        // Skip over the page cache entry if it is dirty, belongs to a live
        // file (not deleted), and it has not been evicted.
        //

        if (((Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) != 0) &&
            ((FileObject->Properties.HardLinkCount != 0) ||
             (FileObject->PathEntryCount != 0)) &&
            ((Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0)) {

            //
            // This page cache entry really shouldn't be on a list. Remove it.
            //

            ASSERT(PageCacheListHead == &IoPageCacheLruListHead);
            ASSERT((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) != 0);

            LIST_REMOVE(&(PageCacheEntry->ListEntry));
            PageCacheEntry->ListEntry.Next = NULL;
            continue;
        }

        ASSERT(PageCacheEntry->ReferenceCount == 0);

        //
        // Unmap this page cache entry from any image sections that may be
        // mapping it. If the mappings note that the page is dirty, then mark
        // it dirty and skip removing the page if it became dirty. As the list
        // lock is already held, the helper routine cannot move it to the dirty
        // list. This routine must do it on its own, but make sure to move the
        // page cache entry that was marked dirty (i.e. the page owner).
        //
        // N.B. Keep hold of the page cache locks or else another image section
        //      could start using this page in the middle of the unmap process.
        //
        // Evicted page cache entries and entries for deleted file objects
        // should not have any mappings.
        //

        Flags = PageCacheEntry->Flags;
        if (((FileObject->Properties.HardLinkCount != 0) ||
             (FileObject->PathEntryCount != 0)) &&
            ((Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0)) {

            //
            // Unmapping a page cache entry can fail if a non-paged image
            // section maps it.
            //

            Status = IopUnmapPageCacheEntry(PageCacheEntry, &PageWasDirty);
            if (!KSUCCESS(Status)) {
                continue;
            }

            if (PageWasDirty != FALSE) {
                MarkedDirty = IoMarkPageCacheEntryDirty(PageCacheEntry,
                                                        0,
                                                        0,
                                                        FALSE);

                if (MarkedDirty != FALSE) {
                    if (PageCacheEntry->BackingEntry != NULL) {
                        PageCacheEntry = PageCacheEntry->BackingEntry;
                    }

                    if (((PageCacheEntry->Flags &
                          PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0) &&
                        (PageCacheEntry->ListEntry.Next != NULL)) {

                        //
                        // Watch out for the next pointer being this backing
                        // entry, it would be bad to loop through this now
                        // defunct entry.
                        //

                        if (CurrentEntry == &(PageCacheEntry->ListEntry)) {
                            CurrentEntry = CurrentEntry->Next;
                        }

                        LIST_REMOVE(&(PageCacheEntry->ListEntry));
                        PageCacheEntry->ListEntry.Next = NULL;

                        //
                        // Watch out for the loop searching for an entry
                        // that is no longer on the list.
                        //

                        if (PageCacheEntry == FirstWithReference) {
                            FirstWithReference = NULL;
                        }
                    }

                    continue;
                }

                ASSERT((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) ==
                       0);
            }
        }

        //
        // Make sure the cache entry is clean to keep the metrics correct.
        //

        IopMarkPageCacheEntryClean(PageCacheEntry, FALSE);

        //
        // Remove the node for the page cache entry tree if necessary.
        //

        if (PageCacheEntry->Node.Parent != NULL) {
            IopRemovePageCacheEntryFromTree(PageCacheEntry);
        }

        //
        // If this page cache entry owns its physical page, then it counts
        // towards the removal count.
        //

        if ((Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0) {
            if (TargetRemoveCount != NULL) {
                *TargetRemoveCount -= 1;
            }
        }

        //
        // If this entry had been evicted, it is about to get destroyed so
        // decrement the evicted count.
        //

        if ((Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0) {
            IoPageCacheEvictedCount -= 1;
        }

        //
        // Remove the entry from the list and add it to the destroy list.
        //

        LIST_REMOVE(&(PageCacheEntry->ListEntry));
        INSERT_BEFORE(&(PageCacheEntry->ListEntry), DestroyListHead);
    }

    KeReleaseQueuedLock(IoPageCacheListLock);
    return;
}

VOID
IopUnmapLruPageCacheList (
    VOID
    )

/*++

Routine Description:

    This routine unmounts as many clean page cache entries as is necessary to
    bring the number of mapped page cache entries back down to a reasonable
    level. It unmaps page cache entires in LRU order.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PPAGE_CACHE_ENTRY BackingEntry;
    PLIST_ENTRY CurrentEntry;
    PPAGE_CACHE_ENTRY FirstWithReference;
    UINTN FreeVirtualPages;
    UINTN MappedCleanPageCount;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageSize;
    UINTN TargetUnmapCount;
    UINTN UnmapCount;
    UINTN UnmapSize;
    PVOID UnmapStart;
    PVOID VirtualAddress;

    TargetUnmapCount = 0;
    FreeVirtualPages= -1;
    if (IopIsPageCacheTooMapped(&FreeVirtualPages) == FALSE) {
        return;
    }

    //
    // The page cache is not leaving enough free virtual memory; determine how
    // many entries must be unmapped.
    //

    ASSERT(FreeVirtualPages < IoPageCacheHeadroomVirtualPagesRetreat);

    TargetUnmapCount = IoPageCacheHeadroomVirtualPagesRetreat -
                       FreeVirtualPages;

    MappedCleanPageCount = IoPageCacheMappedPageCount -
                           IoPageCacheMappedDirtyPageCount;

    if (TargetUnmapCount > MappedCleanPageCount) {
        TargetUnmapCount = MappedCleanPageCount;
        if (TargetUnmapCount == 0) {
            return;
        }
    }

    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_MAPPED_MANAGEMENT) != 0) {
        RtlDebugPrint("PAGE CACHE: Attempt to unmap at least %lu entries.\n",
                      TargetUnmapCount);
    }

    //
    // Iterate over the clean LRU page cache list trying to unmap page cache
    // entries. Stop as soon as the target count has been reached.
    //

    UnmapStart = NULL;
    UnmapSize = 0;
    UnmapCount = 0;
    PageSize = MmPageSize();
    FirstWithReference = NULL;
    KeAcquireSharedExclusiveLockExclusive(IoPageCacheTreeLock);
    KeAcquireQueuedLock(IoPageCacheListLock);
    CurrentEntry = IoPageCacheLruListHead.Next;
    while ((CurrentEntry != &IoPageCacheLruListHead) &&
           (TargetUnmapCount != UnmapCount)) {

        PageCacheEntry = LIST_VALUE(CurrentEntry, PAGE_CACHE_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Skip over all page cache entries with references, moving them to the
        // back of the list. They cannot be unmapped at the moment. Break out
        // if the first page cache entry with a reference has been seen twice.
        //

        if (PageCacheEntry == FirstWithReference) {
            break;
        }

        if (PageCacheEntry->ReferenceCount != 0) {
            if (FirstWithReference == NULL) {
                FirstWithReference = PageCacheEntry;
            }

            LIST_REMOVE(&(PageCacheEntry->ListEntry));
            INSERT_BEFORE(&(PageCacheEntry->ListEntry),
                          &IoPageCacheLruListHead);

            continue;
        }

        //
        // Skip the page cache entry if it is not mapped.
        //

        if (PageCacheEntry->VirtualAddress == NULL) {
            continue;
        }

        //
        // Skip over the page cache entry if it is dirty. The cleaning process
        // will likely need it to be mapped shortly.
        //

        if ((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_DIRTY) != 0) {
            continue;
        }

        //
        // If this page cache entry owns the physical page, then it is not
        // serving as a backing entry to any other page cache entry (as it has
        // no refernences). Freely unmap it.
        //

        ASSERT(PageCacheEntry->ReferenceCount == 0);

        if ((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0) {
            VirtualAddress = PageCacheEntry->VirtualAddress;
            PageCacheEntry->VirtualAddress = NULL;
            UnmapCount += 1;

        //
        // The page cache entry is not the owner, but it may be eligible for
        // unmap if the owner only has 1 reference. A page cache entry should
        // not back more than one other page cache entry.
        //

        } else {
            BackingEntry = PageCacheEntry->BackingEntry;

            ASSERT(BackingEntry != NULL);
            ASSERT(BackingEntry->VirtualAddress ==
                   PageCacheEntry->VirtualAddress);

            ASSERT((BackingEntry->Flags &
                    PAGE_CACHE_ENTRY_FLAG_PAGE_OWNER) != 0);

            if (BackingEntry->ReferenceCount != 1) {
                continue;
            }

            VirtualAddress = BackingEntry->VirtualAddress;
            BackingEntry->VirtualAddress = NULL;
            PageCacheEntry->VirtualAddress = NULL;
            UnmapCount += 1;
        }

        //
        // If this page is not contiguous with the previous run, unmap the
        // previous run.
        //

        if ((UnmapStart != NULL) &&
            (VirtualAddress != (UnmapStart + UnmapSize))) {

            MmUnmapAddress(UnmapStart, UnmapSize);
            UnmapStart = NULL;
            UnmapSize = 0;
        }

        //
        // Either start a new run or append it to the previous run.
        //

        if (UnmapStart == NULL) {
            UnmapStart = VirtualAddress;
        }

        UnmapSize += PageSize;
    }

    KeReleaseQueuedLock(IoPageCacheListLock);
    KeReleaseSharedExclusiveLockExclusive(IoPageCacheTreeLock);

    //
    // If there is a remaining region of contiguous virtual memory that needs
    // to be unmapped, it can be done after releasing the lock as all of the
    // page cache entries have already been updated to reflect being unmapped.
    //

    if (UnmapStart != NULL) {
        MmUnmapAddress(UnmapStart, UnmapSize);
    }

    RtlAtomicAdd(&IoPageCacheMappedPageCount, -UnmapCount);
    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_MAPPED_MANAGEMENT) != 0) {
        RtlDebugPrint("PAGE CACHE: Unmapped %lu entries.\n", UnmapCount);
    }

    return;
}

BOOL
IopIsIoBufferPageCacheBackedHelper (
    PFILE_OBJECT FileObject,
    PIO_BUFFER IoBuffer,
    ULONGLONG Offset,
    UINTN SizeInBytes
    )

/*++

Routine Description:

    This routine determines whether or not the given I/O buffer with data
    targeting the given file object at the given offset is currently backed by
    the page cache, up to the given size. The caller is expected to synchronize
    with eviction via truncate.

Arguments:

    FileObject - Supplies a pointer to a file object.

    IoBuffer - Supplies a pointer to an I/O buffer.

    Offset - Supplies an offset into the file or device object.

    SizeInBytes - Supplies the number of bytes in the I/O buffer that should be
        cache backed.

Return Value:

    Returns TRUE if the I/O buffer is backed by valid page cache entries, or
    FALSE otherwise.

--*/

{

    UINTN BufferOffset;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageSize;

    PageSize = MmPageSize();

    ASSERT(IS_ALIGNED(SizeInBytes, PageSize) != FALSE);

    BufferOffset = 0;
    while (SizeInBytes != 0) {

        //
        // If this page in the buffer is not backed by a page cache entry or
        // not backed by the correct page cache entry, then return FALSE. Also
        // return FALSE if the offsets do not agree.
        //

        PageCacheEntry = MmGetIoBufferPageCacheEntry(IoBuffer, BufferOffset);
        if ((PageCacheEntry == NULL) ||
            (PageCacheEntry->FileObject != FileObject) ||
            ((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0) ||
            (PageCacheEntry->Offset != Offset)) {

            return FALSE;
        }

        SizeInBytes -= PageSize;
        BufferOffset += PageSize;
        Offset += PageSize;
    }

    return TRUE;
}

KSTATUS
IopUnmapPageCacheEntry (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    PBOOL PageWasDirty
    )

/*++

Routine Description:

    This routine unmaps the physical page owned by the given page cache entry
    from all the image sections that may have it mapped.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry to be unmapped.

    PageWasDirty - Supplies a pointer where a boolean will be returned
        indicating if the page that was unmapped was dirty. This parameter is
        optional.

Return Value:

    Status code.

--*/

{

    ULONG Flags;
    PIMAGE_SECTION_LIST ImageSectionList;
    KSTATUS Status;

    ASSERT(PageCacheEntry->ReferenceCount == 0);

    Status = STATUS_SUCCESS;
    *PageWasDirty = FALSE;
    ImageSectionList = PageCacheEntry->FileObject->ImageSectionList;
    if (ImageSectionList != NULL) {
        Flags = IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY;
        Status = MmUnmapImageSectionList(ImageSectionList,
                                         PageCacheEntry->Offset,
                                         MmPageSize(),
                                         Flags,
                                         PageWasDirty);
    }

    return Status;
}

VOID
IopRemovePageCacheEntryFromTree (
    PPAGE_CACHE_ENTRY PageCacheEntry
    )

/*++

Routine Description:

    This routine removes a page cache entry from the page cache tree. This
    routine assumes that the page cache's tree lock is held exclusively.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry to be removed.

Return Value:

    None.

--*/

{

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoPageCacheTreeLock) != FALSE);
    ASSERT((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_BUSY) == 0);
    ASSERT(PageCacheEntry->Node.Parent != NULL);

    //
    // If a backing entry exists, then MM needs to know that the backing entry
    // now owns the page. It may have always been the owner, but just make sure.
    //

    if (PageCacheEntry->BackingEntry != NULL) {
        MmSetPageCacheEntryForPhysicalAddress(PageCacheEntry->PhysicalAddress,
                                              PageCacheEntry->BackingEntry);
    }

    RtlRedBlackTreeRemove(&(PageCacheEntry->FileObject->PageCacheEntryTree),
                          &(PageCacheEntry->Node));

    PageCacheEntry->Node.Parent = NULL;
    IoPageCacheEntryCount -= 1;
    if ((IoPageCacheDebugFlags & PAGE_CACHE_DEBUG_EVICTION) != 0) {
        RtlDebugPrint("PAGE CACHE: Remove entry 0x%08x: file object "
                      "0x%08x, offset 0x%I64x, physical address "
                      "0x%I64x, reference count %d, flags 0x%08x.\n",
                      PageCacheEntry,
                      PageCacheEntry->FileObject,
                      PageCacheEntry->Offset,
                      PageCacheEntry->PhysicalAddress,
                      PageCacheEntry->ReferenceCount,
                      PageCacheEntry->Flags);
    }

    return;
}

VOID
IopRemovePageCacheEntryFromList (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    BOOL Eviction,
    PLIST_ENTRY DestroyListHead
    )

/*++

Routine Description:

    This routine removes the given page cache entry from its current list (if
    necessary). If it has no remaining references, then it is moved to the
    supplied destroy list for the caller to destroy, as long as such a list is
    supplied. Otherwise it puts the entry on the global remove list so that the
    page cache can remove it once all references are gone. This routine assumes
    that the page cache's tree lock is held exclusively.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry that is to be
        removed from its list.

    Eviction - Supplies a boolean indicating if the page cache entry is being
        removed from its list for eviction.

    DestroyListHead - Supplies an optional pointer to the head of a list of
        page cache entries that the caller plans to destroy when ready.

Return Value:

    Returns TRUE f the entry was added to the supplied destroy list and FALSE
    otherwise.

--*/

{

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoPageCacheTreeLock) != FALSE);

    KeAcquireQueuedLock(IoPageCacheListLock);
    if ((Eviction == FALSE) &&
        ((PageCacheEntry->Flags & PAGE_CACHE_ENTRY_FLAG_EVICTED) != 0)) {

        goto RemovePageCacheEntryFromListEnd;
    }

    if (PageCacheEntry->ListEntry.Next != NULL) {
        LIST_REMOVE(&(PageCacheEntry->ListEntry));
    }

    if ((PageCacheEntry->ReferenceCount == 0) && (DestroyListHead != NULL)) {
        INSERT_BEFORE(&(PageCacheEntry->ListEntry), DestroyListHead);

    } else {
        INSERT_BEFORE(&(PageCacheEntry->ListEntry),
                      &IoPageCacheRemovalListHead);
    }

RemovePageCacheEntryFromListEnd:
    KeReleaseQueuedLock(IoPageCacheListLock);
    return;
}

VOID
IopUpdatePageCacheEntryList (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    BOOL Created,
    BOOL Write
    )

/*++

Routine Description:

    This routine updates a page cache entry's list entry by putting it on the
    appropriate list. This should be used when a page cache entry is looked up
    or when it is created.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry whose list
        entry needs to be updated.

    Created - Supplies a boolean indicating if the page cache entry was just
        created or not.

    Write - Supplies a boolean indicating if the page cache entry was looked up
        or created on behalf of a write operation.

Return Value:

    None.

--*/

{

    PPAGE_CACHE_ENTRY DirtyEntry;

    if ((Created == FALSE) ||
        (Write == FALSE) ||
        (PageCacheEntry->BackingEntry != NULL)) {

        KeAcquireQueuedLock(IoPageCacheListLock);

        //
        // If the page cache entry is not new, then it might already be on a
        // list. If so, then either remove it from all lists (write operation
        // on a clean page) or move it to the back of the clean LRU list (read
        // operation on a clean page). If it is not currently on a list, then
        // it is dirty and remains dirty.
        //

        if (Created == FALSE) {
            if (Write != FALSE) {

                //
                // In the write case, the backing entry is what will eventually
                // get marked dirty, so only remove that from the list. Keep
                // the page cache entry in its current location.
                //

                if (PageCacheEntry->BackingEntry != NULL) {
                    DirtyEntry = PageCacheEntry->BackingEntry;

                } else {
                    DirtyEntry = PageCacheEntry;
                }

                if ((DirtyEntry->ListEntry.Next != NULL) &&
                    ((DirtyEntry->Flags &
                      PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0)) {

                    LIST_REMOVE(&(DirtyEntry->ListEntry));
                    DirtyEntry->ListEntry.Next = NULL;
                }

            } else {
                if ((PageCacheEntry->ListEntry.Next != NULL) &&
                    ((PageCacheEntry->Flags &
                      PAGE_CACHE_ENTRY_FLAG_EVICTED) == 0)) {

                    LIST_REMOVE(&(PageCacheEntry->ListEntry));
                    INSERT_BEFORE(&(PageCacheEntry->ListEntry),
                                  &IoPageCacheLruListHead);
                }
            }

        //
        // New pages do not start on a list. If it has a backing entry and this
        // is a create for write, then make sure the backing entry isn't on a
        // list and put the new entry on the LRU "clean" list. On creates for
        // read, just add the new page cache entry to the LRU "clean" list.
        //

        } else {

            ASSERT(PageCacheEntry->ListEntry.Next == NULL);

            if (Write != FALSE) {

                ASSERT(PageCacheEntry->BackingEntry != NULL);

                DirtyEntry = PageCacheEntry->BackingEntry;
                if (DirtyEntry->ListEntry.Next != NULL) {
                    LIST_REMOVE(&(DirtyEntry->ListEntry));
                    DirtyEntry->ListEntry.Next = NULL;
                }
            }

            INSERT_BEFORE(&(PageCacheEntry->ListEntry),
                          &IoPageCacheLruListHead);
        }

        KeReleaseQueuedLock(IoPageCacheListLock);
    }

    return;
}

BOOL
IopIsPageCacheTooBig (
    PUINTN FreePhysicalPages
    )

/*++

Routine Description:

    This routine determines if the page cache is too large given current
    memory constraints.

Arguments:

    FreePhysicalPages - Supplies an optional pointer where the number of free
        physical pages used at the time of computation will be returned. This
        will only be returned if the page cache is reported to be too big.

Return Value:

    TRUE if the page cache is too big and should shrink.

    FALSE if the page cache is too small or just right.

--*/

{

    UINTN FreePages;

    //
    // Don't let the page cache shrink too much. If it's already below the
    // minimum just skip it (but leave the target remove count set so that
    // paging out is requested). Otherwise, clip the remove count to avoid
    // going below the minimum.
    //

    if (IoPageCachePhysicalPageCount <= IoPageCacheMinimumPages) {
        return FALSE;
    }

    //
    // Get the current number of free pages in the system, and determine if the
    // page cache still has room to grow.
    //

    FreePages = MmGetTotalFreePhysicalPages();
    if (FreePages > IoPageCacheHeadroomPagesTrigger) {
        return FALSE;
    }

    if (FreePhysicalPages != NULL) {
        *FreePhysicalPages = FreePages;
    }

    return TRUE;
}

BOOL
IopIsPageCacheTooMapped (
    PUINTN FreeVirtualPages
    )

/*++

Routine Description:

    This routine determines if the page cache has too many mapped entries given
    current memory constraints.

Arguments:

    FreeVirtualPages - Supplies an optional pointer where the number of free
        virtual pages at the time of computation will be returned. This will
        only be returned if the page cache is determined to have too many
        entries mapped.

Return Value:

    TRUE if the page cache has too many mapped entries and some should be
    unmapped.

    FALSE if the page cache does not have too many entries mapped.

--*/

{

    UINTN FreePages;

    //
    // Check to make sure at least a single page cache entry is mapped.
    //

    if (IoPageCacheMappedPageCount == 0) {
        return FALSE;
    }

    //
    // Get the current number of free virtual pages in system memory and
    // determine if the page cache still has room to grow.
    //

    FreePages = MmGetTotalFreeVirtualMemory() >> MmPageShift();
    if (FreePages > IoPageCacheHeadroomVirtualPagesTrigger) {
        return FALSE;
    }

    if (FreeVirtualPages != NULL) {
        *FreeVirtualPages = FreePages;
    }

    return TRUE;
}

