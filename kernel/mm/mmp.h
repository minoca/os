/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mmp.h

Abstract:

    This header contains private definitions for the memory managment library.

Author:

    Evan Green 1-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define MM_PAGE_DIRECTORY_BLOCK_ALLOCATION_TAG 0x6C426450 // 'lBdP'

//
// Define the block expansion count for the page directory block allocator.
// This is defined in number of blocks.
//

#define MM_PAGE_DIRECTORY_BLOCK_ALLOCATOR_EXPANSION_COUNT 4

//
// Define paging entry flags.
//

#define PAGING_ENTRY_FLAG_PAGING_OUT 0x0001
#define PAGING_ENTRY_FLAG_FREED      0x0002

//
// Define flags for flushing image sections.
//

#define IMAGE_SECTION_FLUSH_FLAG_ASYNC 0x00000001

//
// Define the set of unmap flags.
//

#define UNMAP_FLAG_SEND_INVALIDATE_IPI 0x00000001
#define UNMAP_FLAG_FREE_PHYSICAL_PAGES 0x00000002

//
// This flag indicates that the underlying physical memory being described was
// created with this structure. When the structure is destroyed, the memory
// will be freed as well.
//

#define IO_BUFFER_INTERNAL_FLAG_PA_OWNED 0x00000001

//
// This flag is set when the structure was not allocated by these routines.
//

#define IO_BUFFER_INTERNAL_FLAG_STRUCTURE_NOT_OWNED 0x00000002

//
// This flag is set when the I/O buffer's memory is locked.
//

#define IO_BUFFER_INTERNAL_FLAG_MEMORY_LOCKED 0x00000004

//
// This flag is set when the I/O buffer meta-data is non-paged.
//

#define IO_BUFFER_INTERNAL_FLAG_NON_PAGED 0x00000008

//
// This flag is set if the buffer is meant to be filled with physical pages
// from page cache entries.
//

#define IO_BUFFER_INTERNAL_FLAG_CACHE_BACKED 0x00000010

//
// This flag is set if the I/O buffer represents a region in user mode.
//

#define IO_BUFFER_INTERNAL_FLAG_USER_MODE 0x00000020

//
// This flag is set if the I/O buffer is completely mapped. It does not have to
// be virtually contiguous.
//

#define IO_BUFFER_INTERNAL_FLAG_MAPPED 0x00000040

//
// This flag is set if the I/O buffer is mapped virtually contiguous.
//

#define IO_BUFFER_INTERNAL_FLAG_VA_CONTIGUOUS 0x00000080

//
// This flag is set if the I/O buffer needs to be unmapped on free. An I/O
// buffer may have valid virtual addresses, but only needs to be unmapped if
// the virtual addresses were allocated by I/O buffer routines.
//

#define IO_BUFFER_INTERNAL_FLAG_VA_OWNED 0x00000100

//
// This flag is set if the I/O buffer can be extended by appending physical
// pages, page cache entries, or by allocating new physical memory.
//

#define IO_BUFFER_INTERNAL_FLAG_EXTENDABLE 0x00000200

//
// This flag is set when the I/O buffer's memory by the I/O buffer internals
// and thus needs to be unlocked when the buffer is released.
//

#define IO_BUFFER_INTERNAL_FLAG_LOCK_OWNED 0x00000400

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines the index into an image section's bitmap array for a
// given page offset.
//

#define IMAGE_SECTION_BITMAP_INDEX(_PageOffset) \
    ((_PageOffset) / (sizeof(ULONG) * BITS_PER_BYTE))

//
// This macro determines in the mask for a particular page within an image
// section's bitmap.
//

#define IMAGE_SECTION_BITMAP_MASK(_PageOffset) \
    (1 << ((_PageOffset) % (sizeof(ULONG) * BITS_PER_BYTE)))

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a section of memory.

Members:

    ReferenceCount - Stores the reference count of the image section.

    Flags - Stores flags regarding the image section. See IMAGE_SECTION_*
        definitions.

    AddressListEntry - Stores pointers to the next and previous sections in the
        address space.

    ImageListEntry - Stores pointers to the next and previous sections that
        also inherit page cache pages from the same backing image.

    CopyListEntry - Stores pointers to the next and previous sections also
        inheriting from the parent section.

    Parent - Stores a pointer to the parent section this one inherits from.

    ChildList - Stores the list of image sections inheriting from this one.

    AddressSpace - Stores a pointer to the address space this section belongs
        to.

    VirtualAddress - Stores the virtual address this section is mapped to.

    Lock - Stores a pointer to the image section lock.

    PagingInIrp - Stores a pointer to the IRP used to swap pages in from the
        page file.

    SwapSpace - Stores 1 page of free VA space that can be used as swap space
        while pages are being paged in or out.

    DirtyPageBitmap - Stores a pointer to a bitmap describing which pages are
        clean (and can thus be backed by an image) and which pages are
        dirty (and therefore must be backed by the page file).

    InheritPageBitmap - Stores a pointer to a bitmap describing which pages
        should be inherited from the parent.

    Size - Stores the size of the section, in bytes.

    TruncateCount - Stores the number of times pages from this image section
        have been unmapped due to truncation. This is used to detect evictions
        during page in while the lock is released.

    PageFileBacking - Stores the page file backing, if needed.

    ImageBacking - Stores the image file backing, if needed.

    ImageBackingReferenceCount - Stores the reference count for the image
        backing handle specifically. This is kept separately so that it can be
        closed earlier, preventing the paging thread from holding the bag of
        closing this handle (which is paged).

    MinTouched - Stores the minimum address that has been accessed.

    MaxTouched - Stores the maximum address that has been accessed.

    MapFlags - Stores an additional bitmask of MAP_FLAG_* definitions to OR in
        to any mappings of this section.

--*/

typedef struct _IMAGE_SECTION IMAGE_SECTION, *PIMAGE_SECTION;
struct _IMAGE_SECTION {
    volatile ULONG ReferenceCount;
    ULONG Flags;
    LIST_ENTRY AddressListEntry;
    LIST_ENTRY ImageListEntry;
    LIST_ENTRY CopyListEntry;
    PIMAGE_SECTION Parent;
    LIST_ENTRY ChildList;
    PADDRESS_SPACE AddressSpace;
    PVOID VirtualAddress;
    PQUEUED_LOCK Lock;
    PVOID PagingInIrp;
    PMEMORY_RESERVATION SwapSpace;
    PULONG DirtyPageBitmap;
    PULONG InheritPageBitmap;
    UINTN Size;
    volatile ULONG TruncateCount;
    IMAGE_BACKING PageFileBacking;
    IMAGE_BACKING ImageBacking;
    UINTN ImageBackingReferenceCount;
    PVOID MinTouched;
    PVOID MaxTouched;
    ULONG MapFlags;
};

/*++

Structure Description:

    This structure defines all the data necessary for a physical page to
    participate in paging.

Members:

    Section - Stores a pointer to the image section this page is mapped into.

    SectionOffset - Stores the number of pages from the beginning of the
        section to the virtual address corresponding to this physical page.

    LockCount - Stores the number of concurrent requests to lock the page in
        memory. It is protected by the physical page lock.

    Flags - Stores a bitmask of flags for the paging entry. See
        PAGING_ENTRY_FLAG_* for definitions. This is only modified by the
        paging thread.

    ListEntry - Stores a pointer to the next and previous paging entries in
        a list of paging entries ready for destruction.

--*/

typedef struct _PAGING_ENTRY {
    PIMAGE_SECTION Section;
    union {
        struct {
            UINTN SectionOffset;
            USHORT LockCount;
            USHORT Flags;
        };

        LIST_ENTRY ListEntry;
    } U;

} PAGING_ENTRY, *PPAGING_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// Stores the number of physical pages of memory in the system.
//

extern UINTN MmTotalPhysicalPages;

//
// Stores the number of allocated pages.
//

extern UINTN MmTotalAllocatedPhysicalPages;

//
// Stores the the minimum number of free physical pages to be maintained by
// the system.
//

extern UINTN MmMinimumFreePhysicalPages;

//
// Store the maximum physical address that can be reached. This should be
// removed when PAE is supported.
//

extern PHYSICAL_ADDRESS MmMaximumPhysicalAddress;

//
// Stores the lock protecting access to physical page data structures.
//

extern PQUEUED_LOCK MmPhysicalPageLock;

//
// Store a boolean indicating whether or not physical page zero is available.
//
//

extern BOOL MmPhysicalPageZeroAvailable;

//
// Stores the event used to signal a memory warnings when there is a warning
// level change in the number of allocated physical pages.
//

extern PKEVENT MmPhysicalMemoryWarningEvent;

//
// Stores the event used to signal a virtual memory notification when there is
// a significant change in the amount of allocated virtual memory.
//

PKEVENT MmVirtualMemoryWarningEvent;

//
// The virtual allocator keeps track of which VA space is in use and which is
// free.
//

extern MEMORY_ACCOUNTING MmKernelVirtualSpace;

//
// Store a pointer to the kernel's address space context.
//

extern PADDRESS_SPACE MmKernelAddressSpace;

//
// Stores the locks that serialize access to the pools.
//

extern KSPIN_LOCK MmNonPagedPoolLock;
extern PQUEUED_LOCK MmPagedPoolLock;

//
// Store a list of the available paging devices.
//

extern LIST_ENTRY MmPageFileListHead;
extern PQUEUED_LOCK MmPageFileListLock;

//
// Store the paging thread and paging related events.
//

extern PKTHREAD MmPagingThread;
extern PKEVENT MmPagingEvent;
extern PKEVENT MmPagingFreePagesEvent;

//
// This lock serializes TLB invaldation IPIs.
//

extern KSPIN_LOCK MmInvalidateIpiLock;

//
// Define cache line sizes for the CPU L1 caches.
//

extern ULONG MmDataCacheLineSize;
extern ULONG MmInstructionCacheLineSize;
extern BOOL MmVirtuallyIndexedInstructionCache;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
MmpInitializePhysicalPageAllocator (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    PVOID *InitMemory,
    PUINTN InitMemorySize
    );

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

VOID
MmpGetPhysicalPageStatistics (
    PMM_STATISTICS Statistics
    );

/*++

Routine Description:

    This routine fills out the physical memory portion of the given memory
    statistics structure.

Arguments:

    Statistics - Supplies a pointer to the statistics to fill in.

Return Value:

    None.

--*/

PHYSICAL_ADDRESS
MmpAllocatePhysicalPages (
    UINTN PageCount,
    UINTN Alignment
    );

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

PHYSICAL_ADDRESS
MmpAllocateIdentityMappablePhysicalPages (
    UINTN PageCount,
    UINTN Alignment
    );

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

KSTATUS
MmpAllocateScatteredPhysicalPages (
    PHYSICAL_ADDRESS MinPhysical,
    PHYSICAL_ADDRESS MaxPhysical,
    PPHYSICAL_ADDRESS Pages,
    UINTN PageCount
    );

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

KSTATUS
MmpEarlyAllocatePhysicalMemory (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    UINTN PageCount,
    UINTN Alignment,
    ALLOCATION_STRATEGY Strategy,
    PPHYSICAL_ADDRESS Allocation
    );

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

VOID
MmpEnablePagingOnPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN PageCount,
    PPAGING_ENTRY *PagingEntries,
    BOOL LockPages
    );

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

KSTATUS
MmpLockPhysicalPages (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN PageCount
    );

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

VOID
MmpUnlockPhysicalPages (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN PageCount
    );

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

PPAGE_CACHE_ENTRY
MmpGetPageCacheEntryForPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress
    );

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

VOID
MmpMigratePagingEntries (
    PIMAGE_SECTION OldSection,
    PIMAGE_SECTION NewSection,
    PVOID Address,
    UINTN PageCount
    );

/*++

Routine Description:

    This routine migrates all existing paging entries in the given virtual
    addre space over to a new image section.

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

UINTN
MmpPageOutPhysicalPages (
    UINTN FreePagesTarget,
    PIO_BUFFER IoBuffer,
    PMEMORY_RESERVATION SwapRegion
    );

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

PADDRESS_SPACE
MmpArchCreateAddressSpace (
    VOID
    );

/*++

Routine Description:

    This routine creates a new address space context. This routine allocates
    the structure, zeros at least the common portion, and initializes any
    architecture specific members after the common potion.

Arguments:

    None.

Return Value:

    Returns a pointer to the new address space on success.

    NULL on allocation failure.

--*/

VOID
MmpArchDestroyAddressSpace (
    PADDRESS_SPACE AddressSpace
    );

/*++

Routine Description:

    This routine destroys an address space, freeing this structure and all
    architecture-specific content. The common portion of the structure will
    already have been taken care of.

Arguments:

    AddressSpace - Supplies a pointer to the address space to destroy.

Return Value:

    None.

--*/

BOOL
MmpCheckDirectoryUpdates (
    PVOID FaultingAddress
    );

/*++

Routine Description:

    This routine determines if a page fault occurred because a process' page
    directory is out of date. If so, it updates the directory entry.

Arguments:

    FaultingAddress - Supplies the address to verify for up-to-date PDE entries.

Return Value:

    Returns TRUE if the update resolved the page fault, or FALSE if the fault
    requires further attention.

--*/

VOID
MmpMapPage (
    PHYSICAL_ADDRESS PhysicalAddress,
    PVOID VirtualAddress,
    ULONG Flags
    );

/*++

Routine Description:

    This routine maps a physical page of memory into virtual address space.

Arguments:

    PhysicalAddress - Supplies the physical address to back the mapping with.

    VirtualAddress - Supplies the virtual address to map the physical page to.

    Flags - Supplies a bitfield of flags governing the options of the mapping.
        See MAP_FLAG_* definitions.

Return Value:

    None.

--*/

VOID
MmpUnmapPages (
    PVOID VirtualAddress,
    ULONG PageCount,
    ULONG UnmapFlags,
    PBOOL PageWasDirty
    );

/*++

Routine Description:

    This routine unmaps a portion of virtual address space.

Arguments:

    VirtualAddress - Supplies the address to unmap.

    PageCount - Supplies the number of pages to unmap.

    UnmapFlags - Supplies a bitmask of flags for the unmap operation. See
        UNMAP_FLAG_* for definitions. In the default case, this should contain
        UNMAP_FLAG_SEND_INVALIDATE_IPI. There are specific situations where
        it's known that this memory could not exist in another processor's TLB.

    PageWasDirty - Supplies an optional pointer where a boolean will be
        returned indicating if any of the pages were dirty.

Return Value:

    None.

--*/

PHYSICAL_ADDRESS
MmpVirtualToPhysical (
    PVOID VirtualAddress,
    PULONG Attributes
    );

/*++

Routine Description:

    This routine returns the physical address corresponding to the given
    virtual address.

Arguments:

    VirtualAddress - Supplies the address to translate to a physical address.

    Attributes - Supplies an optional pointer where a bitfield of attributes
        will be returned. See MAP_FLAG_* definitions.

Return Value:

    Returns the physical address corresponding to the virtual address, or NULL
    if no mapping could be found.

--*/

PHYSICAL_ADDRESS
MmpVirtualToPhysicalInOtherProcess (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress
    );

/*++

Routine Description:

    This routine returns the physical address corresponding to the given
    virtual address that belongs to another process.

Arguments:

    AddressSpace - Supplies a pointer to the address space.

    VirtualAddress - Supplies the address to translate to a physical address.

Return Value:

    Returns the physical address corresponding to the virtual address, or NULL
    if no mapping could be found.

--*/

VOID
MmpUnmapPageInOtherProcess (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    ULONG UnmapFlags,
    PBOOL PageWasDirty
    );

/*++

Routine Description:

    This routine unmaps a page of VA space from this process or another.

Arguments:

    AddressSpace - Supplies a pointer to the address space to unmap the page
        from.

    VirtualAddress - Supplies the virtual address of the page to unmap.

    UnmapFlags - Supplies a bitmask of flags for the unmap operation. See
        UNMAP_FLAG_* for definitions. This routine will always send an IPI
        after doing the unmap.

    PageWasDirty - Supplies an optional pointer where a boolean will be
        returned indicating if the pages were dirty.

Return Value:

    None.

--*/

VOID
MmpMapPageInOtherProcess (
    PADDRESS_SPACE AddressSpace,
    PHYSICAL_ADDRESS PhysicalAddress,
    PVOID VirtualAddress,
    ULONG MapFlags,
    BOOL SendTlbInvalidateIpi
    );

/*++

Routine Description:

    This routine maps a physical page of memory into the virtual address space
    of another process.

Arguments:

    AddressSpace - Supplies a pointer to the address space to map the page in.

    PhysicalAddress - Supplies the physical address to back the mapping with.

    VirtualAddress - Supplies the virtual address to map the physical page to.

    MapFlags - Supplies a bitfield of flags governing the mapping. See
        MAP_FLAG_* definitions.

    SendTlbInvalidateIpi - Supplies a boolean indicating whether a TLB
        invalidate IPI needs to be sent out for this mapping. If in doubt,
        specify TRUE.

Return Value:

    None.

--*/

VOID
MmpChangeMemoryRegionAccess (
    PVOID VirtualAddress,
    ULONG PageCount,
    ULONG MapFlags,
    ULONG MapFlagsMask
    );

/*++

Routine Description:

    This routine changes whether or not writes are allowed in the given VA
    range. This routine skips any pages in the range that are not mapped.

Arguments:

    VirtualAddress - Supplies the page-aligned beginning of the virtual address
        range to change.

    PageCount - Supplies the number of pages in the range to change.

    MapFlags - Supplies the bitfield of MAP_FLAG_* values to set. Only
        present, read-only, and execute can be changed.

    MapFlagsMask - Supplies the bitfield of supplied MAP_FLAG_* values that are
        valid. If in doubt, use MAP_FLAG_ALL_MASK to make all values valid.

Return Value:

    None.

--*/

KSTATUS
MmpPreallocatePageTables (
    PADDRESS_SPACE SourceAddressSpace,
    PADDRESS_SPACE DestinationAddressSpace
    );

/*++

Routine Description:

    This routine allocates, but does not initialize nor fully map the page
    tables for a process that is being forked. It is needed because physical
    page allocations are not allowed while an image section lock is held.

Arguments:

    SourceAddressSpace - Supplies a pointer to the address space to prepare to
        copy. A page table is allocated but not initialized for every missing
        page table in the destination.

    DestinationAddressSpace - Supplies a pointer to the destination to create
        the page tables in.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on failure.

--*/

KSTATUS
MmpCopyAndChangeSectionMappings (
    PADDRESS_SPACE Destination,
    PADDRESS_SPACE Source,
    PVOID VirtualAddress,
    UINTN Size
    );

/*++

Routine Description:

    This routine converts all the mappings of the given virtual address region
    to read-only, and copies those read-only mappings to another process.

Arguments:

    Destination - Supplies a pointer to the destination address space.

    Source - Supplies a pointer to the source address space.

    VirtualAddress - Supplies the starting virtual address of the memory range.

    Size - Supplies the size of the virtual address region, in bytes.

Return Value:

    None.

--*/

VOID
MmpCreatePageTables (
    PVOID VirtualAddress,
    UINTN Size
    );

/*++

Routine Description:

    This routine ensures that all page tables are present for the given virtual
    address range.

Arguments:

    VirtualAddress - Supplies the starting virtual address of the memory range.

    Size - Supplies the size of the virtual address region, in bytes.

Return Value:

    None.

--*/

KSTATUS
MmpAddAccountingDescriptor (
    PMEMORY_ACCOUNTING Accountant,
    PMEMORY_DESCRIPTOR Descriptor
    );

/*++

Routine Description:

    This routine adds the given descriptor to the accounting information. The
    caller must he holding the accounting lock.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Descriptor - Supplies a pointer to the descriptor to add. Note that the
        descriptor being passed in does not have to be permanent. A copy of the
        descriptor will be made.

Return Value:

    Status code.

--*/

KSTATUS
MmpAllocateFromAccountant (
    PMEMORY_ACCOUNTING Accountant,
    PVM_ALLOCATION_PARAMETERS Request
    );

/*++

Routine Description:

    This routine allocates a piece of free memory from the given memory
    accountant's memory list and marks it as the given memory type.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Request - Supplies a pointer to the allocation request. The allocated
        address is also returned here.

Return Value:

    Status code.

--*/

KSTATUS
MmpFreeAccountingRange (
    PADDRESS_SPACE AddressSpace,
    PVOID Allocation,
    UINTN SizeInBytes,
    BOOL LockHeld,
    ULONG UnmapFlags
    );

/*++

Routine Description:

    This routine frees the previously allocated memory range.

Arguments:

    AddressSpace - Supplies a pointer to the address space containing the
        allocated range. If NULL is supplied, the kernel address space will
        be used.

    Allocation - Supplies the allocation to free.

    SizeInBytes - Supplies the length of space, in bytes, to release.

    LockHeld - Supplies a boolean indicating whether or not the accountant's
        lock is already held exclusively.

    UnmapFlags - Supplies a bitmask of flags for the unmap operation. See
        UNMAP_FLAG_* for definitions. In the default case, this should contain
        UNMAP_FLAG_SEND_INVALIDATE_IPI. There are specific situations where
        it's known that this memory could not exist in another processor's TLB.

Return Value:

    Status code.

--*/

KSTATUS
MmpRemoveAccountingRange (
    PMEMORY_ACCOUNTING Accountant,
    ULONGLONG StartAddress,
    ULONGLONG EndAddress
    );

/*++

Routine Description:

    This routine removes the given address range from the memory accountant.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    StartAddress - Supplies the starting address of the range to remove.

    EndAddress - Supplies the first address beyond the region being removed.
        That is, the non-inclusive end address of the range.

Return Value:

    Status code.

--*/

KSTATUS
MmpAllocateAddressRange (
    PMEMORY_ACCOUNTING Accountant,
    PVM_ALLOCATION_PARAMETERS Request,
    BOOL LockHeld
    );

/*++

Routine Description:

    This routine finds an address range of a certain size in the given memory
    space.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Request - Supplies a pointer to the allocation request. The resulting
        allocation is also returned here.

    LockHeld - Supplies a boolean indicating whether or not the accountant's
        lock is already held exclusively.

Return Value:

    Status code.

--*/

KSTATUS
MmpAllocateAddressRanges (
    PMEMORY_ACCOUNTING Accountant,
    UINTN Size,
    UINTN Count,
    MEMORY_TYPE MemoryType,
    PVOID *Allocations
    );

/*++

Routine Description:

    This routine allocates multiple potentially discontiguous address ranges
    of a given size.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Size - Supplies the size of each allocation, in bytes. This will also be
        the alignment of each allocation.

    Count - Supplies the number of allocations to make. This is the number of
        elements assumed to be in the return array.

    MemoryType - Supplies a the type of memory this allocation should be marked
        as. Do not specify MemoryTypeFree for this parameter.

    Allocations - Supplies a pointer where the addresses are returned on
        success. The caller is responsible for freeing each of these.

Return Value:

    Status code.

--*/

KSTATUS
MmpMapRange (
    PVOID RangeAddress,
    UINTN RangeSize,
    UINTN PhysicalRunAlignment,
    UINTN PhysicalRunSize,
    BOOL WriteThrough,
    BOOL NonCached
    );

/*++

Routine Description:

    This routine maps the given memory region after allocating physical pages
    to back the region. The pages will be allocated in sets of physically
    contiguous pages according to the given physical run size. Each set of
    physical pages will be aligned to the given physical run alignment.

Arguments:

    RangeAddress - Supplies the starting virtual address of the range to map.

    RangeSize - Supplies the size of the virtual range to map, in bytes.

    PhysicalRunAlignment - Supplies the required alignment of the runs of
        physical pages.

    PhysicalRunSize - Supplies the size of each run of physically contiguous
        pages.

    WriteThrough - Supplies a boolean indicating if the virtual addresses
        should be mapped write through (TRUE) or the default write back (FALSE).

    NonCached - Supplies a boolean indicating if the virtual addresses should
        be mapped non-cached (TRUE) or the default, which is to map is as
        normal cached memory (FALSE).

Return Value:

    Status code.

--*/

VOID
MmpLockAccountant (
    PMEMORY_ACCOUNTING Accountant,
    BOOL Exclusive
    );

/*++

Routine Description:

    This routine acquires the memory accounting lock, preventing changes to the
    virtual address space of the given process.

Arguments:

    Accountant - Supplies a pointer to the memory accountant.

    Exclusive - Supplies a boolean indicating whether to acquire the lock
        shared (FALSE) if the caller just wants to make sure the VA layout
        doesn't change or exclusive (TRUE) if the caller wants to change the
        VA layout.

Return Value:

    None.

--*/

VOID
MmpUnlockAccountant (
    PMEMORY_ACCOUNTING Accountant,
    BOOL Exclusive
    );

/*++

Routine Description:

    This routine releases the memory accounting lock.

Arguments:

    Accountant - Supplies a pointer to the memory accountant.

    Exclusive - Supplies a boolean indicating whether the lock was held
        shared (FALSE) or exclusive (TRUE).

Return Value:

    None.

--*/

BOOL
MmpIsAccountingRangeFree (
    PMEMORY_ACCOUNTING Accountant,
    PVOID Address,
    ULONGLONG SizeInBytes
    );

/*++

Routine Description:

    This routine determines whether the given address range is free according
    to the accountant. This routine assumes the accounting lock is already
    held.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure.

    Address - Supplies the address to find the corresponding descriptor for.

    SizeInBytes - Supplies the size, in bytes, of the range in question.

Return Value:

    TRUE if the given range is free.

    FALSE if at least part of the given range is in use.

--*/

BOOL
MmpIsAccountingRangeInUse (
    PMEMORY_ACCOUNTING Accountant,
    PVOID Address,
    ULONG SizeInBytes
    );

/*++

Routine Description:

    This routine determines whether or not any portion of the supplied range
    is in use.

Arguments:

    Accountant - Supplies a pointer to a memory accounting structure.

    Address - Supplies the base address of the range.

    SizeInBytes - Supplies the size of the range, in bytes.

Return Value:

    Returns TRUE if a portion of the range is in use or FALSE otherwise.

--*/

BOOL
MmpIsAccountingRangeAllocated (
    PMEMORY_ACCOUNTING Accountant,
    PVOID Address,
    ULONG SizeInBytes
    );

/*++

Routine Description:

    This routine determines whether or not the supplied range is currently
    allocated in the given memory accountant.

Arguments:

    Accountant - Supplies a pointer to a memory accounting structure.

    Address - Supplies the base address of the range.

    SizeInBytes - Supplies the size of the range, in bytes.

Return Value:

    Returns TRUE if the range is completely allocated for a single memory type
    or FALSE otherwise.

--*/

KSTATUS
MmpLookupSection (
    PVOID VirtualAddress,
    PADDRESS_SPACE AddressSpace,
    PIMAGE_SECTION *Section,
    PUINTN PageOffset
    );

/*++

Routine Description:

    This routine looks up the image section corresponding to the given
    virtual address. This routine must be called at low level. If the section
    is found, a reference is added to the section.

Arguments:

    VirtualAddress - Supplies the virtual address to query for.

    AddressSpace - Supplies the address space to look up the section in.

    Section - Supplies a pointer where a pointer to the image section will be
        returned.

    PageOffset - Supplies a pointer where the offset in pages from the
        beginning of the section will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if the virtual address does not map to an image section
        for this process.

--*/

KSTATUS
MmpAddImageSection (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    UINTN Size,
    ULONG Flags,
    HANDLE ImageHandle,
    IO_OFFSET ImageOffset
    );

/*++

Routine Description:

    This routine creates an image section for the given process so that page
    faults can be recognized and handled appropriately. This routine must be
    called at low level.

Arguments:

    AddressSpace - Supplies a pointer to the address space to add the section
        under.

    VirtualAddress - Supplies the virtual address of the section.

    Size - Supplies the size of the section, in bytes.

    Flags - Supplies a bitfield of flags governing this image section. See
        IMAGE_SECTION_* definitions.

    ImageHandle - Supplies an open handle to the backing image, if the section
        is backed by a file. Supply INVALID_HANDLE here for pure memory
        allocations.

    ImageOffset - Supplies the offset, in bytes, to the beginning of the
        backing with the image.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated to create
        the image or there is no more room in the page file.

--*/

KSTATUS
MmpCopyImageSection (
    PIMAGE_SECTION SectionToCopy,
    PADDRESS_SPACE DestinationAddressSpace
    );

/*++

Routine Description:

    This routine copies an image section to another process.

Arguments:

    SectionToCopy - Supplies a pointer to the section to copy.

    DestinationAddressSpace - Supplies a pointer to the address space to copy
        the image section to.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated to create
        the image or there is no more room in the page file.

--*/

KSTATUS
MmpUnmapImageRegion (
    PADDRESS_SPACE AddressSpace,
    PVOID SectionAddress,
    UINTN Size
    );

/*++

Routine Description:

    This routine unmaps and destroys any image sections at the given address.
    This routine must be called at low level. For kernel mode, this must
    specify a single whole image section.

Arguments:

    AddressSpace - Supplies a pointer to the address space to unmap from.

    SectionAddress - Supplies the virtual address of the section.

    Size - Supplies the size of the section in bytes.

Return Value:

    Status code.

--*/

KSTATUS
MmpFlushImageSectionRegion (
    PIMAGE_SECTION Section,
    ULONG PageOffset,
    ULONG PageCount,
    ULONG Flags
    );

/*++

Routine Description:

    This routine flushes the specified region of the given image section to
    its backing image.

Arguments:

    Section - Supplies a pointer to the image section to flush.

    PageOffset - Supplies the offset, in pages, to the start of the region that
        is to be flushed.

    PageCount - Supplies the number of pages to flush.

    Flags - Supplies a bitmask of flags. See IMAGE_SECTION_FLUSH_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

VOID
MmpImageSectionAddReference (
    PIMAGE_SECTION ImageSection
    );

/*++

Routine Description:

    This routine increases the reference count on an image section.

Arguments:

    ImageSection - Supplies a pointer to the image section to add the reference
        to.

Return Value:

    None.

--*/

VOID
MmpImageSectionReleaseReference (
    PIMAGE_SECTION ImageSection
    );

/*++

Routine Description:

    This routine decreases the reference count on an image section. If this was
    the last reference on the image section, then the section is destroyed.

Arguments:

    ImageSection - Supplies a pointer to the image section to release the
        reference from.

Return Value:

    None.

--*/

VOID
MmpImageSectionAddImageBackingReference (
    PIMAGE_SECTION ImageSection
    );

/*++

Routine Description:

    This routine increases the reference count on the image backing portion of
    an image section. This routine assumes the section lock is already held.

Arguments:

    ImageSection - Supplies a pointer to the image section to add the reference
        to.

Return Value:

    None.

--*/

VOID
MmpImageSectionReleaseImageBackingReference (
    PIMAGE_SECTION ImageSection
    );

/*++

Routine Description:

    This routine decreases the reference count on the image backing handle of
    an image section. If this was the last reference on the handle, then the
    handle is set to INVALID_HANDLE and closed. This routine must NOT be called
    with the image section lock held.

Arguments:

    ImageSection - Supplies a pointer to the image section to release the
        backing image reference from.

Return Value:

    None.

--*/

PIMAGE_SECTION
MmpGetOwningSection (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset
    );

/*++

Routine Description:

    This routine returns the image section that owns the given page, based off
    of inheritance. It assumes the section lock is held and it takes a
    reference on the owning section.

Arguments:

    ImageSection - Supplies a pointer to the initial image section.

    PageOffset - Supplies the page's offset within the image section.

Return Value:

    Returns a pointer to the image section that currently owns the page.

--*/

PIMAGE_SECTION
MmpGetRootSection (
    PIMAGE_SECTION ImageSection
    );

/*++

Routine Description:

    This routine returns the root of the image section tree to which the given
    section belongs. It assumes the shared section lock is held and it takes a
    reference on the root section.

Arguments:

    ImageSection - Supplies a pointer to the initial image section.

Return Value:

    Returns a pointer to the image section tree's root image section.

--*/

KSTATUS
MmpIsolateImageSection (
    PIMAGE_SECTION Section,
    UINTN PageOffset
    );

/*++

Routine Description:

    This routine isolates the page mapped in the given image section by
    breaking the section's inheritance from a parent or the page cache. It also
    breaks the inheritance of any children that map the same physical page as
    the given section.

Arguments:

    Section - Supplies a pointer to the image section that contains page that
        needs to be isolated.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    Status code.

--*/

KSTATUS
MmpClipImageSections (
    PLIST_ENTRY SectionListHead,
    PVOID Address,
    UINTN Size,
    PLIST_ENTRY *ListEntryBefore
    );

/*++

Routine Description:

    This routine wipes out any image sections covering the given VA range. It
    assumes the address space lock is already held. This routine does not
    change any accountant mappings.

Arguments:

    SectionListHead - Supplies a pointer to the head of the list of image
        sections for the address space.

    Address - Supplies the first address (inclusive) to remove image sections
        for.

    Size - Supplies the size in bytes of the region to clear.

    ListEntryBefore - Supplies an optional pointer to the list entry
        immediately before where the given address range starts.

Return Value:

    Status code.

--*/

PVOID
MmpMapPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN SizeInBytes,
    BOOL Writable,
    BOOL WriteThrough,
    BOOL CacheDisabled,
    MEMORY_TYPE MemoryType
    );

/*++

Routine Description:

    This routine maps a physical address into kernel VA space.

Arguments:

    PhysicalAddress - Supplies a pointer to the physical address. This address
        must be page aligned.

    SizeInBytes - Supplies the size in bytes of the mapping. This will be
        rounded up to the nearest page size.

    Writable - Supplies a boolean indicating if the memory is to be marked
        writable (TRUE) or read-only (FALSE).

    WriteThrough - Supplies a boolean indicating if the memory is to be marked
        write-through (TRUE) or write-back (FALSE).

    CacheDisabled - Supplies a boolean indicating if the memory is to be mapped
        uncached.

    MemoryType - Supplies the memory type to allocate this as.

Return Value:

    Returns a pointer to the virtual address of the mapping on success, or
    NULL on failure.

--*/

VOID
MmpCopyPage (
    PIMAGE_SECTION Section,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress
    );

/*++

Routine Description:

    This routine copies the page at the given virtual address. It temporarily
    maps the physical address at the given temporary virtual address in order
    to perform the copy.

Arguments:

    Section - Supplies a pointer to the image section to which the virtual
        address belongs.

    VirtualAddress - Supplies the page-aligned virtual address to use as the
        initial contents of the page.

    PhysicalAddress - Supplies the physical address of the destination page
        where the data is to be copied.

Return Value:

    None.

--*/

VOID
MmpZeroPage (
    PHYSICAL_ADDRESS PhysicalAddress
    );

/*++

Routine Description:

    This routine zeros the page specified by the physical address. It maps the
    page temporarily in order to zero it out.

Arguments:

    PhysicalAddress - Supplies the physical address of the page to be filled
        with zero.

Return Value:

    None.

--*/

VOID
MmpUpdateResidentSetCounter (
    PADDRESS_SPACE AddressSpace,
    INTN Addition
    );

/*++

Routine Description:

    This routine adjusts the process resident set counter. This should only
    be done for user mode addresses.

Arguments:

    AddressSpace - Supplies a pointer to the address space to update.

    Addition - Supplies the number of pages to add or subtract from the counter.

Return Value:

    None.

--*/

VOID
MmpAddPageZeroDescriptorsToMdl (
    PMEMORY_ACCOUNTING Accountant
    );

/*++

Routine Description:

    This routine maps page zero and adds it to be used as memory descriptors
    for the given memory accountant. It is assume that page zero was already
    reserved by some means.

Arguments:

    Accountant - Supplies a pointer to the memory accontant to receive the
        memory descriptors from page zero.

Return Value:

    None.

--*/

KSTATUS
MmpInitializeNonPagedPool (
    VOID
    );

/*++

Routine Description:

    This routine initializes the kernel's nonpaged pool.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
MmpInitializePagedPool (
    VOID
    );

/*++

Routine Description:

    This routine initializes the kernel's paged pool.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
MmpSendTlbInvalidateIpi (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    ULONG PageCount
    );

/*++

Routine Description:

    This routine invalidates the given TLB entry on all active processors.

Arguments:

    AddressSpace - Supplies a pointer to the address space to invalidate for.

    VirtualAddress - Supplies the virtual address to invalidate.

    PageCount - Supplies the number of pages to invalidate.

Return Value:

    None.

--*/

KSTATUS
MmpInitializePaging (
    VOID
    );

/*++

Routine Description:

    This routine initializes the paging infrastructure, preparing for the
    arrival of a page file.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
MmpPageIn (
    PIMAGE_SECTION ImageSection,
    UINTN PageOffset,
    PIO_BUFFER LockedIoBuffer
    );

/*++

Routine Description:

    This routine pages a physical page in from disk or allocates a new free
    physical page. This routine must be called at low level.

Arguments:

    ImageSection - Supplies a pointer to the image section within the process
        to page in.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

    LockedIoBuffer - Supplies an optional pointer to an uninitialized I/O
        buffer that will be initialized with the the paged in page, effectively
        locking the page until the I/O buffer is released.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_LATE if the given image section is destroyed.

    STATUS_TRY_AGAIN if the given image section is no longer large enough to
    cover the given page offset.

    Other status codes on other errors.

--*/

KSTATUS
MmpPageInAndLock (
    PIMAGE_SECTION Section,
    UINTN PageOffset
    );

/*++

Routine Description:

    This routine attempts to page in the given page and lock the image section
    as to prevent the page from being paged back out, unmapped, or destroyed by
    the owning section.

Arguments:

    Section - Supplies a pointer to the image section that contains the
        address.

    PageOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    Status code.

--*/

KSTATUS
MmpPageOut (
    PPAGING_ENTRY PagingEntry,
    PIMAGE_SECTION Section,
    UINTN PageOffset,
    PHYSICAL_ADDRESS PhysicalAddress,
    PIO_BUFFER IoBuffer,
    PMEMORY_RESERVATION SwapRegion,
    PUINTN PagesPaged
    );

/*++

Routine Description:

    This routine pages a physical page out to disk. It assumes the page has
    been flagged for paging out. This routine will attempt to batch writes
    and page out several physical pages at this offset.

Arguments:

    PagingEntry - Supplies a pointer to the physical page's paging entry.

    Section - Supplies a pointer to the image section, snapped from the paging
        entry while the physical page lock was still held.

    PageOffset - Supplies the offset into the section in pages where this page
        resides, snapped form the paging entry while the physical page lock was
        still held.

    PhysicalAddress - Supplies the address of the physical page to swap out.

    IoBuffer - Supplies a pointer to an allocated but uninitialized I/O buffer
        to use during page out I/O.

    SwapRegion - Supplies a pointer to a region of VA space to use during
        paging.

    PagesPaged - Supplies a pointer where the count of pages removed will
        be returned.

Return Value:

    Status code.

--*/

VOID
MmpModifySectionMapping (
    PIMAGE_SECTION OwningSection,
    UINTN PageOffset,
    PHYSICAL_ADDRESS PhysicalAddress,
    BOOL CreateMapping,
    PBOOL PageWasDirty,
    BOOL SendTlbInvalidateIpi
    );

/*++

Routine Description:

    This routine maps or unmaps a freshly paged-in physical page to or from its
    location in all appropriate processes.

Arguments:

    OwningSection - Supplies a pointer to the parent section that owns this
        page. The page will be mapped in this section and all children who
        inherit this page from the owning section.

    PageOffset - Supplies the offset in pages from the beginning of the section
        where this page belongs.

    PhysicalAddress - Supplies the physical address of the freshly initialized
        page. If the mapping is not being created (ie being unmapped) then this
        parameter is ignored.

    CreateMapping - Supplies a boolean whether the page should be mapped (TRUE)
        or unmapped (FALSE).

    PageWasDirty - Supplies a pointer where a boolean will be returned
        indicating if the page that was unmapped was dirty. If the create
        mapping parameter was TRUE, this value is ignored.

    SendTlbInvalidateIpi - Supplies a boolean indicating whether a TLB
        invalidate IPI needs to be sent out for this mapping. If in doubt,
        specify TRUE.

Return Value:

    None.

--*/

PPAGING_ENTRY
MmpCreatePagingEntry (
    PIMAGE_SECTION ImageSection,
    ULONGLONG SectionOffset
    );

/*++

Routine Description:

    This routine creates a paging entry based on the provided image section and
    page offset.

Arguments:

    ImageSection - Supplies a pointer to an image section.

    SectionOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    Returns a pointer to a new paging entry on success or NULL on failure.

--*/

VOID
MmpInitializePagingEntry (
    PPAGING_ENTRY PagingEntry,
    PIMAGE_SECTION ImageSection,
    ULONGLONG SectionOffset
    );

/*++

Routine Description:

    This routine initializes the given paging entry based on the provided image
    section and page offset.

Arguments:

    PagingEntry - Supplies a pointer to a paging entry to initialize.

    ImageSection - Supplies a pointer to an image section.

    SectionOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    None.

--*/

VOID
MmpReinitializePagingEntry (
    PPAGING_ENTRY PagingEntry,
    PIMAGE_SECTION ImageSection,
    ULONGLONG SectionOffset
    );

/*++

Routine Description:

    This routine re-initializes the given paging entry based on the provided
    image section and page offset. If there is an existing section, it will be
    dereferenced and overwritten.

Arguments:

    PagingEntry - Supplies a pointer to a paging entry to re-initialize.

    ImageSection - Supplies a pointer to an image section.

    SectionOffset - Supplies the offset, in pages, from the beginning of the
        section.

Return Value:

    None.

--*/

VOID
MmpDestroyPagingEntry (
    PPAGING_ENTRY PagingEntry
    );

/*++

Routine Description:

    This routine destroys a paging entry.

Arguments:

    PagingEntry - Supplies a pointer to a paging entry.

Return Value:

    None.

--*/

BOOL
MmpCheckUserModeCopyRoutines (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine determines if a given fault occurred inside a user mode memory
    manipulation function, and adjusts the instruction pointer if so.

Arguments:

    TrapFrame - Supplies a pointer to the state of the machine when the page
        fault occurred.

Return Value:

    None.

--*/

BOOL
MmpCopyUserModeMemory (
    PVOID Destination,
    PCVOID Source,
    ULONG ByteCount
    );

/*++

Routine Description:

    This routine copies a section of memory to or from user mode.

Arguments:

    Destination - Supplies a pointer to the buffer where the memory will be
        copied to.

    Source - Supplies a pointer to the buffer to be copied.

    ByteCount - Supplies the number of bytes to copy.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
MmpZeroUserModeMemory (
    PVOID Buffer,
    ULONG ByteCount
    );

/*++

Routine Description:

    This routine zeroes out a section of user mode memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to clear.

    ByteCount - Supplies the number of bytes to zero out.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
MmpSetUserModeMemory (
    PVOID Buffer,
    INT Byte,
    UINTN Count
    );

/*++

Routine Description:

    This routine writes the given byte value repeatedly into a region of
    user mode memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to set.

    Byte - Supplies the byte to set.

    Count - Supplies the number of bytes to set.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
MmpCompareUserModeMemory (
    PVOID FirstBuffer,
    PVOID SecondBuffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine compares two buffers for equality.

Arguments:

    FirstBuffer - Supplies a pointer to the first buffer to compare.

    SecondBuffer - Supplies a pointer to the second buffer to compare.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are equal.

    FALSE if the buffers are not equal or on failure.

--*/

BOOL
MmpTouchUserModeMemoryForRead (
    PVOID Buffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine touches each page of a user mode buffer to ensure it can be
    read from.

Arguments:

    Buffer - Supplies a pointer to the buffer to probe.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are valid.

    FALSE if the buffers are not valid.

--*/

BOOL
MmpTouchUserModeMemoryForWrite (
    PVOID Buffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine touches each page of a user mode buffer to ensure it can be
    written to.

Arguments:

    Buffer - Supplies a pointer to the buffer to probe.

    Size - Supplies the number of bytes to compare.

Return Value:

    TRUE if the buffers are valid.

    FALSE if the buffers are not valid.

--*/

VOID
MmpInitializeCpuCaches (
    VOID
    );

/*++

Routine Description:

    This routine initializes the system's processor cache infrastructure.

Arguments:

    None.

Return Value:

    None.

--*/

BOOL
MmpInvalidateCacheLine (
    PVOID Address
    );

/*++

Routine Description:

    This routine invalidates the cache line associated with the given virtual
    address. Note that if there was dirty data in the cache line, it will be
    destroyed.

Arguments:

    Address - Supplies the address whose associated cache line will be
        invalidated.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

BOOL
MmpCleanCacheLine (
    PVOID Address
    );

/*++

Routine Description:

    This routine flushes a cache line, writing any dirty bits back to the next
    level cache.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

BOOL
MmpCleanInvalidateCacheLine (
    PVOID Address
    );

/*++

Routine Description:

    This routine cleans a cache line to the point of coherency and invalidates
    the cache line associated with this address.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned and invalidated.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

BOOL
MmpInvalidateInstructionCacheLine (
    PVOID Address
    );

/*++

Routine Description:

    This routine invalidates a line in the instruction cache by virtual address.

Arguments:

    Address - Supplies the address whose associated instruction cache line will
        be invalidated.

Return Value:

    TRUE on success.

    FALSE if the address was a user mode one and accessing it caused a bad
    fault.

--*/

VOID
MmpSyncSwapPage (
    PVOID SwapPage,
    ULONG PageSize
    );

/*++

Routine Description:

    This routine cleans the data cache but does not invalidate the instruction
    cache for the given kernel region. It is used by the paging code for a
    temporary mapping that is going to get marked executable, but this mapping
    itself does not need an instruction cache flush.

Arguments:

    SwapPage - Supplies a pointer to the swap page.

    PageSize - Supplies the size of a page.

Return Value:

    None.

--*/

BOOL
MmpInvalidateInstructionCacheRegion (
    PVOID Address,
    ULONG Size
    );

/*++

Routine Description:

    This routine invalidates the given region of virtual address space in the
    instruction cache.

Arguments:

    Address - Supplies the virtual address of the region to invalidate.

    Size - Supplies the number of bytes to invalidate.

Return Value:

    TRUE on success.

    FALSE if one of the addresses in the region caused a bad page fault.

--*/

BOOL
MmpCleanCacheRegion (
    PVOID Address,
    UINTN Size
    );

/*++

Routine Description:

    This routine cleans the given region of virtual address space in the first
    level data cache.

Arguments:

    Address - Supplies the virtual address of the region to clean.

    Size - Supplies the number of bytes to clean.

Return Value:

    TRUE on success.

    FALSE if one of the addresses in the region caused a bad page fault.

--*/

