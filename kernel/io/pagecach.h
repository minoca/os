/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pagecach.h

Abstract:

    This header contains definitions for integrating the page cache throughout
    the rest of I/O.

Author:

    Evan Green 29-Jan-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of pages to clean if a random write stumbles into a page
// cache that is too dirty.
//

#define PAGE_CACHE_DIRTY_PENANCE_PAGES 128

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global list of dirty file objects.
//

extern LIST_ENTRY IoFileObjectsDirtyList;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
IopInitializePageCache (
    VOID
    );

/*++

Routine Description:

    This routine initializes the page cache.

Arguments:

    None.

Return Value:

    Status code.

--*/

PPAGE_CACHE_ENTRY
IopLookupPageCacheEntry (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset
    );

/*++

Routine Description:

    This routine searches for a page cache entry based on the file object and
    offset. If found, this routine takes a reference on the page cache entry.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file.

    Offset - Supplies an offset into the file or device.

Return Value:

    Returns a pointer to the found page cache entry on success, or NULL on
    failure.

--*/

PPAGE_CACHE_ENTRY
IopCreateOrLookupPageCacheEntry (
    PFILE_OBJECT FileObject,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    IO_OFFSET Offset,
    PPAGE_CACHE_ENTRY LinkEntry,
    PBOOL EntryCreated
    );

/*++

Routine Description:

    This routine creates a page cache entry and inserts it into the cache. Or,
    if a page cache entry already exists for the supplied file object and
    offset, it returns the existing entry. The file object lock must be held
    exclusive already.

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

    EntryCreated - Supplies an optional pointer that receives a boolean
        indicating whether or not a new page cache entry was created.

Return Value:

    Returns a pointer to a page cache entry on success, or NULL on failure.

--*/

PPAGE_CACHE_ENTRY
IopCreateAndInsertPageCacheEntry (
    PFILE_OBJECT FileObject,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    IO_OFFSET Offset,
    PPAGE_CACHE_ENTRY LinkEntry
    );

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

Return Value:

    Returns a pointer to a page cache entry on success, or NULL on failure.

--*/

KSTATUS
IopCopyAndCacheIoBuffer (
    PFILE_OBJECT FileObject,
    IO_OFFSET FileOffset,
    PIO_BUFFER Destination,
    UINTN CopySize,
    PIO_BUFFER Source,
    UINTN SourceSize,
    UINTN SourceCopyOffset,
    PUINTN BytesCopied
    );

/*++

Routine Description:

    This routine iterates over the source buffer, caching each page and copying
    the pages to the destination buffer starting at the given copy offsets and
    up to the given copy size. The file object lock must be held exclusive
    already.

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

    BytesCopied - Supplies a pointer that receives the number of bytes copied
        to the destination buffer.

Return Value:

    Status code.

--*/

KSTATUS
IopFlushPageCacheEntries (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset,
    ULONGLONG Size,
    ULONG Flags,
    PUINTN PageCount
    );

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

    PageCount - Supplies an optional pointer describing how many pages to flush.
        On output this value will be decreased by the number of pages actually
        flushed. Supply NULL to flush all pages in the size range.

Return Value:

    Status code.

--*/

VOID
IopEvictPageCacheEntries (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset,
    ULONG Flags
    );

/*++

Routine Description:

    This routine attempts to evict the page cache entries for a given file or
    device, as specified by the file object. The flags specify how aggressive
    this routine should be. The file object lock must already be held
    exclusively and this routine assumes that the file object has been unmapped
    from all image sections starting at the offset.

Arguments:

    FileObject - Supplies a pointer to a file object for the device or file.

    Offset - Supplies the starting offset into the file or device after which
        all page cache entries should be evicted.

    Flags - Supplies a bitmask of eviction flags. See EVICTION_FLAG_* for
        definitions.

Return Value:

    None.

--*/

BOOL
IopIsIoBufferPageCacheBacked (
    PFILE_OBJECT FileObject,
    PIO_BUFFER IoBuffer,
    IO_OFFSET Offset,
    UINTN SizeInBytes
    );

/*++

Routine Description:

    This routine determines whether or not the given I/O buffer with data
    targeting the given file object at the given offset is currently backed by
    the page cache. The caller is expected to synchronize with eviction via
    truncate.

Arguments:

    FileObject - Supplies a pointer to a file object.

    IoBuffer - Supplies a pointer to an I/O buffer.

    Offset - Supplies an offset into the file or device object.

    SizeInBytes - Supplied the number of bytes in the I/O buffer that should be
        cache backed.

Return Value:

    Returns TRUE if the I/O buffer is backed by valid page cache entries, or
    FALSE otherwise.

--*/

VOID
IopSchedulePageCacheThread (
    VOID
    );

/*++

Routine Description:

    This routine schedules a cleaning of the page cache for some time in the
    future.

Arguments:

    None.

Return Value:

    None.

--*/

IO_OFFSET
IopGetPageCacheEntryOffset (
    PPAGE_CACHE_ENTRY Entry
    );

/*++

Routine Description:

    This routine gets the file or device offset of the given page cache entry.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

Return Value:

    Returns the file or device offset of the given page cache entry.

--*/

BOOL
IopMarkPageCacheEntryClean (
    PPAGE_CACHE_ENTRY Entry,
    BOOL MoveToCleanList
    );

/*++

Routine Description:

    This routine marks the given page cache entry as clean.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

    MoveToCleanList - Supplies a boolean indicating if the page cache entry
        should be moved to the list of clean page cache entries.

Return Value:

    Returns TRUE if it marked the entry clean or FALSE if the entry was already
    clean.

--*/

BOOL
IopMarkPageCacheEntryDirty (
    PPAGE_CACHE_ENTRY Entry
    );

/*++

Routine Description:

    This routine marks the given page cache entry as dirty. The file object
    lock must already be held.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

Return Value:

    Returns TRUE if it marked the entry dirty or FALSE if the entry was already
    dirty.

--*/

KSTATUS
IopCopyIoBufferToPageCacheEntry (
    PPAGE_CACHE_ENTRY Entry,
    ULONG PageOffset,
    PIO_BUFFER SourceBuffer,
    UINTN SourceOffset,
    ULONG ByteCount
    );

/*++

Routine Description:

    This routine copies up to a page from the given source buffer to the given
    page cache entry.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

    PageOffset - Supplies an offset into the page where the copy should begin.

    SourceBuffer - Supplies a pointer to the source buffer where the data
        originates.

    SourceOffset - Supplies an offset into the the source buffer where the data
        copy should begin.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    Status code.

--*/

BOOL
IopCanLinkPageCacheEntry (
    PPAGE_CACHE_ENTRY Entry,
    PFILE_OBJECT FileObject
    );

/*++

Routine Description:

    This routine determines if the given page cache entry could link with a
    page cache entry for the given file object.

Arguments:

    Entry - Supplies a pointer to a page cache entry.

    FileObject - Supplied a pointer to a file object.

Return Value:

    Returns TRUE if the page cache entry could be linked to a page cache entry
    with the given file object or FALSE otherwise.

--*/

BOOL
IopLinkPageCacheEntries (
    PPAGE_CACHE_ENTRY LowerEntry,
    PPAGE_CACHE_ENTRY UpperEntry
    );

/*++

Routine Description:

    This routine attempts to link the given link entry to the page cache entry
    so that they can begin sharing a physical page that is currently used by
    the link entry.

Arguments:

    LowerEntry - Supplies a pointer to the lower (disk) level page cache entry
        whose physical address is to be modified. The caller should ensure that
        its reference on this entry does not come from an I/O buffer or else
        the physical address in the I/O buffer would be invalid. The file
        object lock for this entry must already be held exclusive.

    UpperEntry - Supplies a pointer to the upper (file) page cache entry
        that currently owns the physical page to be shared.

Return Value:

    Returns TRUE if the two page cache entries are already connected or if the
    routine is successful. It returns FALSE otherwise and both page cache
    entries should continue to use their own physical pages.

--*/

VOID
IopTrimPageCache (
    BOOL TimidEffort
    );

/*++

Routine Description:

    This routine removes as many clean page cache entries as is necessary to
    bring the size of the page cache back down to a reasonable level. It evicts
    the page cache entries in LRU order.

Arguments:

    TimidEffort - Supplies a boolean indicating whether or not this function
        should only try once to acquire a file object lock before moving on.
        Set this to TRUE if this thread might already be holding file object
        locks.

Return Value:

    None.

--*/

BOOL
IopIsPageCacheTooDirty (
    VOID
    );

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

COMPARISON_RESULT
IopComparePageCacheEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

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

