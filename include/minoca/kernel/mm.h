/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mm.h

Abstract:

    This header contains definitions for virtual memory management.

Author:

    Evan Green 27-Jul-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the catch-all allocation tag used the the memory manager: Mm!!.
//

#define MM_ALLOCATION_TAG 0x21216D4D

//
// Define the allocation tag used for I/O buffers: MmIo
//

#define MM_IO_ALLOCATION_TAG 0x6F496D4D

//
// Define the allocation tag used for MM address space allocations: MmAd
//

#define MM_ADDRESS_SPACE_ALLOCATION_TAG 0x64416D4D

//
// Define the allocation tag used by image sections.
//

#define MM_IMAGE_SECTION_ALLOCATION_TAG 0x6D496D4D

//
// Define the pool magic values for non-paged pool (NonP) and paged-pool (PagP).
//

#define NON_PAGED_POOL_MAGIC 0x506E6F4E
#define PAGED_POOL_MAGIC 0x50676150

//
// Define the kernel address space. For 64-bit mode, leave a page at the end
// to avoid rollover issues and to keep the space immediately underflowing NULL
// clear.
//

#if __SIZEOF_LONG__ == 8

#define KERNEL_VA_START (PVOID)0xFFFF800000000000
#define KERNEL_VA_END 0xFFFFFFFFFFFFF000

#else

#define KERNEL_VA_START (PVOID)0x80000000
#define KERNEL_VA_END 0x100000000ULL

#endif

#define SWAP_VA_PAGES 1

#define INVALID_PHYSICAL_ADDRESS 0

//
// Define the minimum size to reserve for user mode stack expansion. Calls to
// map memory will not use this region.
//

#define USER_STACK_HEADROOM (128 * _1MB)
#define USER_STACK_MAX (((UINTN)MAX_USER_ADDRESS + 1) * 3 / 4)
#define MM_STATISTICS_VERSION 1
#define MM_STATISTICS_MAX_VERSION 0x10000000

//
// Define flags for memory accounting systems.
//

#define MEMORY_ACCOUNTING_FLAG_INITIALIZED 0x00000001
#define MEMORY_ACCOUNTING_FLAG_SYSTEM      0x00000002
#define MEMORY_ACCOUNTING_FLAG_NO_MAP      0x00000004
#define MEMORY_ACCOUNTING_FLAG_MASK        0x00000006

//
// Define flags used for MDLs.
//

#define DESCRIPTOR_FLAG_USED     0x00000001
#define DESCRIPTOR_FLAG_FREEABLE 0x00000002

//
// Define the number of bins MDLs keep for free descriptors.
//

#define MDL_BIN_COUNT 8

//
// Define the minimum amount of free system descriptors that need to be
// available before a new system descriptor is added.
//

#define FREE_SYSTEM_DESCRIPTORS_MIN 2

//
// Define the number of free system descriptors that need to be available for
// a descriptor refill to succeed.
//

#define FREE_SYSTEM_DESCRIPTORS_REQUIRED_FOR_REFILL 3

//
// Define flags used for image sections.
//

#define IMAGE_SECTION_READABLE          0x00000001
#define IMAGE_SECTION_WRITABLE          0x00000002
#define IMAGE_SECTION_EXECUTABLE        0x00000004
#define IMAGE_SECTION_NON_PAGED         0x00000008
#define IMAGE_SECTION_SHARED            0x00000010
#define IMAGE_SECTION_MAP_SYSTEM_CALL   0x00000020
#define IMAGE_SECTION_BACKED            0x00000040
#define IMAGE_SECTION_NO_IMAGE_BACKING  0x00000080
#define IMAGE_SECTION_DESTROYING        0x00000100
#define IMAGE_SECTION_DESTROYED         0x00000200
#define IMAGE_SECTION_WAS_WRITABLE      0x00000400
#define IMAGE_SECTION_PAGE_CACHE_BACKED 0x00000800

//
// Define a mask of image section flags that should be transfered when an image
// section is copied. For internal use only.
//

#define IMAGE_SECTION_COPY_MASK                             \
    (IMAGE_SECTION_ACCESS_MASK | IMAGE_SECTION_NON_PAGED |  \
     IMAGE_SECTION_SHARED | IMAGE_SECTION_MAP_SYSTEM_CALL | \
     IMAGE_SECTION_WAS_WRITABLE)

//
// Define a mask of image section access flags.
//

#define IMAGE_SECTION_ACCESS_MASK \
    (IMAGE_SECTION_READABLE | IMAGE_SECTION_WRITABLE | IMAGE_SECTION_EXECUTABLE)

//
// Define the mask of flags that is internal and should not be specified by
// outside callers.
//

#define IMAGE_SECTION_INTERNAL_MASK \
    (IMAGE_SECTION_BACKED | IMAGE_SECTION_NO_IMAGE_BACKING | \
     IMAGE_SECTION_PAGE_CACHE_BACKED)

//
// Define flags used for unmapping image sections.
//

#define IMAGE_SECTION_UNMAP_FLAG_TRUNCATE        0x00000001
#define IMAGE_SECTION_UNMAP_FLAG_PAGE_CACHE_ONLY 0x00000002

//
// Define flags that describe properties of a fault.
//

#define FAULT_FLAG_PAGE_NOT_PRESENT    0x00000001
#define FAULT_FLAG_WRITE               0x00000002
#define FAULT_FLAG_PROTECTION_FAULT    0x00000004
#define FAULT_FLAG_PERMISSION_ERROR    0x00000008
#define FAULT_FLAG_OUT_OF_BOUNDS       0x00000010

//
// Define mapping flags.
//

#define MAP_FLAG_PRESENT            0x00000001
#define MAP_FLAG_READ_ONLY          0x00000002
#define MAP_FLAG_EXECUTE            0x00000004
#define MAP_FLAG_USER_MODE          0x00000008
#define MAP_FLAG_WRITE_THROUGH      0x00000010
#define MAP_FLAG_CACHE_DISABLE      0x00000020
#define MAP_FLAG_GLOBAL             0x00000040
#define MAP_FLAG_LARGE_PAGE         0x00000080
#define MAP_FLAG_PAGABLE            0x00000100
#define MAP_FLAG_DIRTY              0x00000200
#define MAP_FLAG_PROTECT_MASK       0xFFFF
#define MAP_FLAG_PROTECT_SHIFT      16

#define MAP_FLAG_ALL_MASK     \
    (MAP_FLAG_PRESENT |       \
     MAP_FLAG_READ_ONLY |     \
     MAP_FLAG_EXECUTE |       \
     MAP_FLAG_USER_MODE |     \
     MAP_FLAG_WRITE_THROUGH | \
     MAP_FLAG_CACHE_DISABLE | \
     MAP_FLAG_GLOBAL |        \
     MAP_FLAG_PAGABLE |       \
     MAP_FLAG_DIRTY)

//
// Define flags used for creating block allocators.
//

#define BLOCK_ALLOCATOR_FLAG_NON_PAGED             0x00000001
#define BLOCK_ALLOCATOR_FLAG_NON_CACHED            0x00000002
#define BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS 0x00000004
#define BLOCK_ALLOCATOR_FLAG_TRIM                  0x00000008
#define BLOCK_ALLOCATOR_FLAG_NO_EXPANSION          0x00000010

//
// Define user mode virtual address for the user shared data page.
//

#define USER_SHARED_DATA_USER_ADDRESS ((PVOID)0x7FFFF000)

//
// Define the maximum number of I/O vector elements that will be tolerated from
// user-mode.
//

#define MAX_IO_VECTOR_COUNT 1024

//
// Define the native sized user write function.
//

#if __SIZEOF_LONG__ == 8

#define MmUserWrite MmUserWrite64
#define MmUserRead MmUserRead64

#else

#define MmUserWrite MmUserWrite32
#define MmUserRead MmUserRead32

#endif

//
// Define the bitmask of flags used to initialize or allocate an I/O buffer.
//

#define IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS 0x00000001
#define IO_BUFFER_FLAG_MAP_NON_CACHED        0x00000002
#define IO_BUFFER_FLAG_MAP_WRITE_THROUGH     0x00000004
#define IO_BUFFER_FLAG_MEMORY_LOCKED         0x00000008
#define IO_BUFFER_FLAG_KERNEL_MODE_DATA      0x00000010

//
// --------------------------------------------------------------------- Macros
//

#define IS_MEMORY_FREE_TYPE(_Type) ((_Type) == MemoryTypeFree)

//
// Define macros for pool allocations.
//

#define MmAllocateNonPagedPool(_Size, _Tag) \
    MmAllocatePool(PoolTypeNonPaged, _Size, _Tag)

#define MmAllocatePagedPool(_Size, _Tag) \
    MmAllocatePool(PoolTypePaged, _Size, _Tag)

#define MmFreeNonPagedPool(_Allocation) \
    MmFreePool(PoolTypeNonPaged, _Allocation)

#define MmFreePagedPool(_Allocation) \
    MmFreePool(PoolTypePaged, _Allocation)

#define MmFreePhysicalPage(_PhysicalAddress) \
    MmFreePhysicalPages((_PhysicalAddress), 1)

//
// These macros acquire the address space lock.
//

#define MmAcquireAddressSpaceLock(_AddressSpace) \
    KeAcquireQueuedLock((_AddressSpace)->Lock)

#define MmReleaseAddressSpaceLock(_AddressSpace) \
    KeReleaseQueuedLock((_AddressSpace)->Lock)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef LONGLONG IO_OFFSET, *PIO_OFFSET;
typedef struct _IMAGE_SECTION_LIST IMAGE_SECTION_LIST, *PIMAGE_SECTION_LIST;

typedef enum _POOL_CORRUPTION_DETAIL {
    PoolCorruptionNone,
    PoolCorruptionDoubleFree,
    PoolCorruptionBufferOverrun
} POOL_CORRUPTION_DETAIL, *PPOOL_CORRUPTION_DETAIL;

typedef enum _MEMORY_TYPE {
    MemoryTypeInvalid,
    MemoryTypeReserved,
    MemoryTypeFree,
    MemoryTypeFirmwareTemporary,
    MemoryTypeFirmwarePermanent,
    MemoryTypeAcpiTables,
    MemoryTypeAcpiNvStorage,
    MemoryTypeBad,
    MemoryTypeLoaderTemporary,
    MemoryTypeLoaderPermanent,
    MemoryTypePageTables,
    MemoryTypeBootPageTables,
    MemoryTypeMmStructures,
    MemoryTypeNonPagedPool,
    MemoryTypePagedPool,
    MemoryTypeHardware,
    MemoryTypeIoBuffer,
    MaxMemoryTypes
} MEMORY_TYPE, *PMEMORY_TYPE;

typedef enum _MDL_ALLOCATION_SOURCE {
    MdlAllocationSourceInvalid,
    MdlAllocationSourceNone,
    MdlAllocationSourceNonPagedPool,
    MdlAllocationSourcePagedPool
} MDL_ALLOCATION_SOURCE, *PMDL_ALLOCATION_SOURCE;

typedef enum _ALLOCATION_STRATEGY {
    AllocationStrategyInvalid,
    AllocationStrategyLowestAddress,
    AllocationStrategyAnyAddress,
    AllocationStrategyHighestAddress,
    AllocationStrategyFixedAddress,
    AllocationStrategyFixedAddressClobber,
} ALLOCATION_STRATEGY, *PALLOCATION_STRATEGY;

typedef enum _POOL_TYPE {
    PoolTypeInvalid,
    PoolTypeNonPaged,
    PoolTypePaged,
    PoolTypeCount
} POOL_TYPE, *PPOOL_TYPE;

typedef enum _MEMORY_WARNING_LEVEL {
    MemoryWarningLevelNone,
    MemoryWarningLevel1,
    MemoryWarningLevel2,
    MaxMemoryWarningLevels
} MEMORY_WARNING_LEVEL, *PMEMORY_WARNING_LEVEL;

typedef enum _MM_INFORMATION_TYPE {
    MmInformationInvalid,
    MmInformationSystemMemory,
} MM_INFORMATION_TYPE, *PMM_INFORMATION_TYPE;

/*++

Structure Description:

    This structure defines a list of memory descriptors.

Members:

    Tree - Stores the tree of the memory map.

    FreeLists - Stores the array of lists of free regions within the descriptor
        list.

    DescriptorCount - Stores the number of descriptors in the list.

    AllocationSource - Stores the policy on where the MDL should acquire new
        descriptors from.

    UnusedListHead - Stores the head of the list of descriptors that are
        currently not active in the MDL but are available for use.

    UnusedDescriptorCount - Stores the number of descriptors in the unused list
        that are immediately available.

    TotalSpace - Stores the total number of bytes described by this descriptor
        list.

    FreeSpace - Stores the total free descriptor bytes in this descriptor list.

--*/

typedef struct _MEMORY_DESCRIPTOR_LIST {
    RED_BLACK_TREE Tree;
    LIST_ENTRY FreeLists[MDL_BIN_COUNT];
    ULONG DescriptorCount;
    MDL_ALLOCATION_SOURCE AllocationSource;
    LIST_ENTRY UnusedListHead;
    ULONG UnusedDescriptorCount;
    ULONGLONG TotalSpace;
    ULONGLONG FreeSpace;
} MEMORY_DESCRIPTOR_LIST, *PMEMORY_DESCRIPTOR_LIST;

/*++

Structure Description:

    This structure defines a contiguous piece of physical memory.

Members:

    TreeNode - Stores the red-black tree membership information for this
        descriptor.

    FreeListEntry - Stores links to the next and previous memory descriptors if
        this descriptor represents a free area.

    BaseAddress - Stores the address of the beginning of the descriptor.

    Size - Stores the size of the region, in bytes.

    Type - Stores the type of memory that this descriptor represents.

    Flags - Stores various state of the descriptor. See DESCRIPTOR_FLAG_*
        definitions.

--*/

typedef struct _MEMORY_DESCRIPTOR {
    RED_BLACK_TREE_NODE TreeNode;
    LIST_ENTRY FreeListEntry;
    ULONGLONG BaseAddress;
    ULONGLONG Size;
    MEMORY_TYPE Type;
    ULONG Flags;
} MEMORY_DESCRIPTOR, *PMEMORY_DESCRIPTOR;

typedef
VOID
(*PMEMORY_DESCRIPTOR_LIST_ITERATION_ROUTINE) (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

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

/*++

Structure Description:

    This structure defines a memory accountant. This structure can be passed
    into support routines that keep track of which memory for a given region
    is in use.

Members:

    Flags - Stores characteristics about the memory accounting. See the
        MEMORY_ACCOUNTING_FLAG_* flags.

    Lock - Stores a pointer to the shared/exclusive lock that synchronizes
        access to the accounting structures (type SHARED_EXCLUSIVE_LOCK).

    Mdl - Stores the memory descriptor list containing which areas are free
        and which are in use.

--*/

typedef struct _MEMORY_ACCOUNTING {
    ULONG Flags;
    PVOID Lock;
    MEMORY_DESCRIPTOR_LIST Mdl;
} MEMORY_ACCOUNTING, *PMEMORY_ACCOUNTING;

/*++

Structure Description:

    This structure stores image backing information for a section of memory.

Members:

    DeviceHandle - Stores a pointer to the device or file backing the
        allocation.

    Offset - Stores the offset from the beginning of the file where the backing
        starts, in bytes.

--*/

typedef struct _IMAGE_BACKING {
    HANDLE DeviceHandle;
    IO_OFFSET Offset;
} IMAGE_BACKING, *PIMAGE_BACKING;

/*++

Structure Description:

    This structure defines a virtual address space reservation.

Members:

    Process - Stores a pointer to the process owning the reservation.

    VirtualBase - Stores the base virtual address of the reservation.

    Size - Stores the size, in bytes, of the reservation.

--*/

typedef struct _MEMORY_RESERVATION {
    PVOID Process;
    PVOID VirtualBase;
    UINTN Size;
} MEMORY_RESERVATION, *PMEMORY_RESERVATION;

/*++

Structure Description:

    This structure defines an I/O buffer fragment, a region of memory that is
    physically and virtually contiguous.

Members:

    PhysicalAddress - Stores the physical address of the memory region.

    VirtualAddress - Stores the virtual address of the memory region.

    Size - Stores the size of the region, in bytes.

--*/

typedef struct _IO_BUFFER_FRAGMENT {
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID VirtualAddress;
    UINTN Size;
} IO_BUFFER_FRAGMENT, *PIO_BUFFER_FRAGMENT;

/*++

Structure Description:

    This structure defines an I/O buffer.

Members:

    Flags - Stores a bitfield of flags reserved for use interally by
        the memory management system.

    CurrentOffset - Stores the byte offset into the buffer at which all I/O
        or copies will begin.

    MaxFragmentCount - Stores the maximum number of fragments that this I/O
        buffer can hold.

    TotalSize - Stores the total size of the I/O buffer.

    PageCacheEntryCount - Store the maximum number of page cache entries that
        the I/O buffer can contain.

    PageCacheEntry - Stores a pointer to a page cache entry used for
        stack-allocated I/O buffers that only require one page.

    PageCacheEntries - Stores an array of page cache entries associated with
        this I/O buffer.

    MapFlags - Stores any additional mapping flags mandated by the file object
        for this I/O buffer. See MAP_FLAG_* definitions.

    Fragment - Stores an I/O buffer fragment structure used for stack-allocated
        I/O buffers that only require one fragment.

--*/

typedef struct _IO_BUFFER_INTERNAL {
    ULONG Flags;
    UINTN CurrentOffset;
    UINTN MaxFragmentCount;
    UINTN TotalSize;
    UINTN PageCacheEntryCount;
    PVOID PageCacheEntry;
    PVOID *PageCacheEntries;
    ULONG MapFlags;
    IO_BUFFER_FRAGMENT Fragment;
} IO_BUFFER_INTERNAL, *PIO_BUFFER_INTERNAL;

/*++

Structure Description:

    This structure defines an I/O buffer.

Members:

    Fragment - Stores an array of memory fragments that make up the I/O
        buffer, sorted by virtual address.

    FragmentCount - Stores the number of fragments in the fragment array.

    Internal - Stores I/O buffer information that is internal to the system.

--*/

typedef struct _IO_BUFFER {
    IO_BUFFER_FRAGMENT *Fragment;
    UINTN FragmentCount;
    IO_BUFFER_INTERNAL Internal;
} IO_BUFFER, *PIO_BUFFER;

typedef struct _BLOCK_ALLOCATOR BLOCK_ALLOCATOR, *PBLOCK_ALLOCATOR;

/*++

Structure Description:

    This structure defines an I/O buffer.

Members:

    Version - Stores the structure version number. Set this to
        MM_STATISTICS_VERSION.

    PageSize - Stores the size of a page in the system.

    NonPagedPool - Stores memory heap statistics for non-paged pool.

    PagedPool - Stores memory heap statistics for paged pool.

    PhysicalPages - Stores the number of physical pages in the system.

    AllocatedPhysicalPages - Stores the number of physical pages currently in
        use by the system.

    NonPagedPhysicalPages - Stores the number of physical pages that are
        pinned in memory and cannot be paged out to disk.

--*/

typedef struct _MM_STATISTICS {
    ULONG Version;
    ULONG PageSize;
    MEMORY_HEAP_STATISTICS NonPagedPool;
    MEMORY_HEAP_STATISTICS PagedPool;
    UINTN PhysicalPages;
    UINTN AllocatedPhysicalPages;
    UINTN NonPagedPhysicalPages;
} MM_STATISTICS, *PMM_STATISTICS;

/*++

Structure Description:

    This structure defines an I/O vector, a structure used in kernel mode that
    lines up with struct iovec in the C library.

Members:

    Data - Stores a pointer to the data.

    Length - Stores a the length of the data.

--*/

typedef struct _IO_VECTOR {
    PVOID Data;
    UINTN Length;
} IO_VECTOR, *PIO_VECTOR;

/*++

Structure Description:

    This structure defines an address space context.

Members:

    Lock - Stores a pointer to the queued lock serializing access to the
        image section list.

    SectionListHead - Stores the head of the list of image sections mapped
        into this process.

    Accountant - Stores a pointer to the address tracking information for this
        space.

    ResidentSet - Stores the number of pages currently mapped in the process.

    MaxResidentSet - Stores the maximum resident set ever mapped into the
        process.

    MaxMemoryMap - Stores the maximum address that map/unmap system calls
        should return.

    BreakStart - Stores the start address of the program break.

    BreakEnd - Stores the end address of the program break.

--*/

typedef struct _ADDRESS_SPACE {
    PVOID Lock;
    LIST_ENTRY SectionListHead;
    PMEMORY_ACCOUNTING Accountant;
    volatile UINTN ResidentSet;
    volatile UINTN MaxResidentSet;
    PVOID MaxMemoryMap;
    PVOID BreakStart;
    PVOID BreakEnd;
} ADDRESS_SPACE, *PADDRESS_SPACE;

/*++

Structure Description:

    This structure defines the usual set of parameters for a virtual memory
    allocation request.

Members:

    Address - Stores the preferred or demanded address on input, and the
        returned address on output.

    Size - Stores the size of the allocation.

    Alignment - Stores the required alignment of the allocation.

    Min - Stores the minimum address to allocate.

    Max - Stores the maximum address to allocate.

    MemoryType - Stores the requested memory type.

    Strategy - Stores the memory allocation strategy to use.

--*/

typedef struct _VM_ALLOCATION_PARAMETERS {
    PVOID Address;
    UINTN Size;
    ULONG Alignment;
    PVOID Min;
    PVOID Max;
    MEMORY_TYPE MemoryType;
    ALLOCATION_STRATEGY Strategy;
} VM_ALLOCATION_PARAMETERS, *PVM_ALLOCATION_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
PVOID
MmAllocatePool (
    POOL_TYPE PoolType,
    UINTN Size,
    ULONG Tag
    );

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

KERNEL_API
PVOID
MmReallocatePool (
    POOL_TYPE PoolType,
    PVOID Memory,
    UINTN NewSize,
    UINTN AllocationTag
    );

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

KERNEL_API
VOID
MmFreePool (
    POOL_TYPE PoolType,
    PVOID Allocation
    );

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

KSTATUS
MmGetPoolProfilerStatistics (
    PVOID *Buffer,
    PULONG BufferSize,
    ULONG Tag
    );

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

KERNEL_API
VOID
MmDebugPrintPoolStatistics (
    VOID
    );

/*++

Routine Description:

    This routine prints pool statistics to the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

KERNEL_API
PIO_BUFFER
MmAllocateNonPagedIoBuffer (
    PHYSICAL_ADDRESS MinimumPhysicalAddress,
    PHYSICAL_ADDRESS MaximumPhysicalAddress,
    UINTN Alignment,
    UINTN Size,
    ULONG Flags
    );

/*++

Routine Description:

    This routine allocates memory for use as an I/O buffer. This memory will
    remain mapped in memory until the buffer is destroyed.

Arguments:

    MinimumPhysicalAddress - Supplies the minimum physical address of the
        allocation.

    MaximumPhysicalAddress - Supplies the maximum physical address of the
        allocation.

    Alignment - Supplies the required physical alignment of the buffer, in
        bytes.

    Size - Supplies the minimum size of the buffer, in bytes.

    Flags - Supplies a bitmask of flags used to allocate the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

Return Value:

    Returns a pointer to the I/O buffer on success, or NULL on failure.

--*/

KERNEL_API
PIO_BUFFER
MmAllocatePagedIoBuffer (
    UINTN Size,
    ULONG Flags
    );

/*++

Routine Description:

    This routine allocates memory for use as a pageable I/O buffer.

Arguments:

    Size - Supplies the minimum size of the buffer, in bytes.

    Flags - Supplies a bitmask of flags used to allocate the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

Return Value:

    Returns a pointer to the I/O buffer on success, or NULL on failure.

--*/

KERNEL_API
PIO_BUFFER
MmAllocateUninitializedIoBuffer (
    UINTN Size,
    ULONG Flags
    );

/*++

Routine Description:

    This routine allocates an uninitialized I/O buffer that the caller will
    fill in with pages. It simply allocates the structures for the given
    size, assuming a buffer fragment may be required for each page.

Arguments:

    Size - Supplies the minimum size of the buffer, in bytes. This size is
        rounded up (always) to a page, but does assume page alignment.

    Flags - Supplies a bitmask of flags used to allocate the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

Return Value:

    Returns a pointer to the I/O buffer on success, or NULL on failure.

--*/

KERNEL_API
KSTATUS
MmCreateIoBuffer (
    PVOID Buffer,
    UINTN SizeInBytes,
    ULONG Flags,
    PIO_BUFFER *NewIoBuffer
    );

/*++

Routine Description:

    This routine creates an I/O buffer from an existing memory buffer. This
    routine must be called at low level.

Arguments:

    Buffer - Supplies a pointer to the memory buffer on which to base the I/O
        buffer.

    SizeInBytes - Supplies the size of the buffer, in bytes.

    Flags - Supplies a bitmask of flags used to allocate the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

    NewIoBuffer - Supplies a pointer where a pointer to the new I/O buffer
        will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
MmCreateIoBufferFromVector (
    PIO_VECTOR Vector,
    BOOL VectorInKernelMode,
    UINTN VectorCount,
    PIO_BUFFER *NewIoBuffer
    );

/*++

Routine Description:

    This routine creates a paged usermode I/O buffer based on an I/O vector
    array. This is generally used to support vectored I/O functions in the C
    library.

Arguments:

    Vector - Supplies a pointer to the I/O vector array.

    VectorInKernelMode - Supplies a boolean indicating if the given I/O vector
        array comes directly from kernel mode.

    VectorCount - Supplies the number of elements in the vector array.

    NewIoBuffer - Supplies a pointer where a pointer to the newly created I/O
        buffer will be returned on success. The caller is responsible for
        releasing this buffer.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the vector count is invalid.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_ACCESS_VIOLATION if the given vector array was from user-mode and
    was not valid.

--*/

KSTATUS
MmInitializeIoBuffer (
    PIO_BUFFER IoBuffer,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN SizeInBytes,
    ULONG Flags
    );

/*++

Routine Description:

    This routine initializes an I/O buffer based on the given virtual and
    phsyical address and the size. If a physical address is supplied, it is
    assumed that the range of bytes is both virtually and physically contiguous
    so that it can be contained in one fragment.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to initialize.

    VirtualAddress - Supplies the starting virtual address of the I/O buffer.

    PhysicalAddress - Supplies the starting physical address of the I/O buffer.

    SizeInBytes - Supplies the size of the I/O buffer, in bytes.

    Flags - Supplies a bitmask of flags used to initialize the I/O buffer. See
        IO_BUFFER_FLAG_* for definitions.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
MmAppendIoBufferData (
    PIO_BUFFER IoBuffer,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN SizeInBytes
    );

/*++

Routine Description:

    This routine appends a fragment to and I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer on which to append.

    VirtualAddress - Supplies the starting virtual address of the data to
        append.

    PhysicalAddress - Supplies the starting physical address of the data to
        append.

    SizeInBytes - Supplies the size of the data to append, in bytes.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
MmAppendIoBuffer (
    PIO_BUFFER IoBuffer,
    PIO_BUFFER AppendBuffer,
    UINTN AppendOffset,
    UINTN SizeInBytes
    );

/*++

Routine Description:

    This routine appends one I/O buffer on another.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer on which to append.

    AppendBuffer - Supplies a pointer to the I/O buffer that owns the data to
        append.

    AppendOffset - Supplies the offset into the append buffer where the data to
        append starts.

    SizeInBytes - Supplies the size of the data to append, in bytes.

Return Value:

    Status code.

--*/

KERNEL_API
VOID
MmFreeIoBuffer (
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine destroys an I/O buffer. If the memory was allocated when the
    I/O buffer was created, then the memory will be released at this time as
    well.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to release.

Return Value:

    None.

--*/

VOID
MmResetIoBuffer (
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine resets an I/O buffer for re-use, unmapping any memory and
    releasing any associated page cache entries.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
MmMapIoBuffer (
    PIO_BUFFER IoBuffer,
    BOOL WriteThrough,
    BOOL NonCached,
    BOOL VirtuallyContiguous
    );

/*++

Routine Description:

    This routine maps the given I/O buffer into memory. If the caller requests
    that the I/O buffer be mapped virtually contiguous, then all fragments will
    be updated with the virtually contiguous mappings. If the I/O buffer does
    not need to be virtually contiguous, then this routine just ensure that
    each fragment is mapped.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    WriteThrough - Supplies a boolean indicating if the virtual addresses
        should be mapped write through (TRUE) or the default write back (FALSE).

    NonCached - Supplies a boolean indicating if the virtual addresses should
        be mapped non-cached (TRUE) or the default, which is to map is as
        normal cached memory (FALSE).

    VirtuallyContiguous - Supplies a boolean indicating whether or not the
        caller needs the I/O buffer to be mapped virtually contiguous (TRUE) or
        not (FALSE). In the latter case, each I/O buffer fragment will at least
        be virtually contiguous.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
MmCopyIoBuffer (
    PIO_BUFFER Destination,
    UINTN DestinationOffset,
    PIO_BUFFER Source,
    UINTN SourceOffset,
    UINTN ByteCount
    );

/*++

Routine Description:

    This routine copies the contents of the source I/O buffer starting at the
    source offset to the destination I/O buffer starting at the destination
    offset. It assumes that the arguments are correct such that the copy can
    succeed.

Arguments:

    Destination - Supplies a pointer to the destination I/O buffer that is to
        be copied into.

    DestinationOffset - Supplies the offset into the destination I/O buffer
        where the copy should begin.

    Source - Supplies a pointer to the source I/O buffer whose contents will be
        copied to the destination.

    SourceOffset - Supplies the offset into the source I/O buffer where the
        copy should begin.

    ByteCount - Supplies the size of the requested copy in bytes.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
MmZeroIoBuffer (
    PIO_BUFFER IoBuffer,
    UINTN Offset,
    UINTN ByteCount
    );

/*++

Routine Description:

    This routine zeroes the contents of the I/O buffer starting at the offset
    for the given number of bytes.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer that is to be zeroed.

    Offset - Supplies the offset into the I/O buffer where the zeroing
        should begin.

    ByteCount - Supplies the number of bytes to zero.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
MmCopyIoBufferData (
    PIO_BUFFER IoBuffer,
    PVOID Buffer,
    UINTN Offset,
    UINTN Size,
    BOOL ToIoBuffer
    );

/*++

Routine Description:

    This routine copies from a buffer into the given I/O buffer or out of the
    given I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to the I/O buffer to copy in or out of.

    Buffer - Supplies a pointer to the regular linear buffer to copy to or from.
        This must be a kernel mode address.

    Offset - Supplies an offset in bytes from the beginning of the I/O buffer
        to copy to or from.

    Size - Supplies the number of bytes to copy.

    ToIoBuffer - Supplies a boolean indicating whether data is copied into the
        I/O buffer (TRUE) or out of the I/O buffer (FALSE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INCORRECT_BUFFER_SIZE if the copy goes outside the I/O buffer.

    Other error codes if the I/O buffer could not be mapped.

--*/

KERNEL_API
ULONG
MmGetIoBufferAlignment (
    VOID
    );

/*++

Routine Description:

    This routine returns the required alignment for all flush operations.

Arguments:

    None.

Return Value:

    Returns the size of a data cache line, in bytes.

--*/

KSTATUS
MmValidateIoBuffer (
    PHYSICAL_ADDRESS MinimumPhysicalAddress,
    PHYSICAL_ADDRESS MaximumPhysicalAddress,
    UINTN Alignment,
    UINTN SizeInBytes,
    BOOL PhysicallyContiguous,
    PIO_BUFFER *IoBuffer,
    PBOOL LockedCopy
    );

/*++

Routine Description:

    This routine validates an I/O buffer for use by a device. If the I/O buffer
    does not meet the given requirements, then a new I/O buffer that meets the
    requirements will be returned. This new I/O buffer will not contain the
    same data as the originally supplied I/O buffer. It is up to the caller to
    decide which further actions need to be taken if a different buffer is
    returned. The exception is if the locked parameter is returned as true. In
    that case a new I/O buffer was created, but is backed by the same physical
    pages, now locked in memory.

Arguments:

    MinimumPhysicalAddress - Supplies the minimum allowed physical address for
        the I/O buffer.

    MaximumPhysicalAddress - Supplies the maximum allowed physical address for
        the I/O buffer.

    Alignment - Supplies the required physical alignment of the I/O buffer.

    SizeInBytes - Supplies the minimum required size of the buffer, in bytes.

    PhysicallyContiguous - Supplies a boolean indicating whether or not the
        I/O buffer should be physically contiguous.

    IoBuffer - Supplies a pointer to a pointer to an I/O buffer. On entry, this
        contains a pointer to the I/O buffer to be validated. On exit, it may
        point to a newly allocated I/O buffer that the caller must free.

    LockedCopy - Supplies a pointer to a boolean that receives whether or not
        the validated I/O buffer is a locked copy of the original.

Return Value:

    Status code.

--*/

KSTATUS
MmValidateIoBufferForCachedIo (
    PIO_BUFFER *IoBuffer,
    UINTN SizeInBytes,
    UINTN Alignment
    );

/*++

Routine Description:

    This routine validates an I/O buffer for a cached I/O operation,
    potentially returning a new I/O buffer.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer pointer. On entry, it stores
        the pointer to the I/O buffer to evaluate. On exit, it stores a pointer
        to a valid I/O buffer, that may actually be a new I/O buffer.

    SizeInBytes - Supplies the required size of the I/O buffer.

    Alignment - Supplies the required alignment of the I/O buffer.

Return Value:

    Status code.

--*/

VOID
MmIoBufferAppendPage (
    PIO_BUFFER IoBuffer,
    PVOID PageCacheEntry,
    PVOID VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress
    );

/*++

Routine Description:

    This routine appends a page, as described by its VA/PA or page cache entry,
    to the end of the given I/O buffer. The caller should either supply a page
    cache entry or a physical address (with an optional virtual address), but
    not both.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    PageCacheEntry - Supplies an optional pointer to the page cache entry whose
        data will be appended to the I/O buffer.

    VirtualAddress - Supplies an optional virtual address for the range.

    PhysicalAddress - Supplies the optional physical address of the data that
        is to be set in the I/O buffer at the given offset. Use
        INVALID_PHYSICAL_ADDRESS when supplying a page cache entry.

Return Value:

    None.

--*/

VOID
MmSetIoBufferPageCacheEntry (
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset,
    PVOID PageCacheEntry
    );

/*++

Routine Description:

    This routine sets the given page cache entry in the I/O buffer at the given
    offset. The physical address of the page cache entry should match that of
    the I/O buffer at the given offset.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    IoBufferOffset - Supplies an offset into the given I/O buffer.

    PageCacheEntry - Supplies a pointer to the page cache entry to set.

Return Value:

    None.

--*/

PVOID
MmGetIoBufferPageCacheEntry (
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset
    );

/*++

Routine Description:

    This routine returns the page cache entry associated with the given I/O
    buffer at the given offset into the buffer.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    IoBufferOffset - Supplies an offset into the I/O buffer, in bytes.

Return Value:

    Returns a pointer to a page cache entry if the physical page at the given
    offset has been cached, or NULL otherwise.

--*/

KERNEL_API
UINTN
MmGetIoBufferSize (
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine returns the size of the I/O buffer, in bytes.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

Return Value:

    Returns the size of the I/O buffer, in bytes.

--*/

KERNEL_API
UINTN
MmGetIoBufferCurrentOffset (
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine returns the given I/O buffer's current offset. The offset is
    the point at which all I/O should begin.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

Return Value:

    Returns the I/O buffers current offset.

--*/

KERNEL_API
VOID
MmSetIoBufferCurrentOffset (
    PIO_BUFFER IoBuffer,
    UINTN Offset
    );

/*++

Routine Description:

    This routine sets the given I/O buffer's current offset. The offset is
    the point at which all I/O should begin.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    Offset - Supplies the new offset for the I/O buffer.

Return Value:

    None.

--*/

KERNEL_API
VOID
MmIoBufferIncrementOffset (
    PIO_BUFFER IoBuffer,
    UINTN OffsetIncrement
    );

/*++

Routine Description:

    This routine increments the I/O buffer's current offset by the given amount.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    OffsetIncrement - Supplies the number of bytes by which the offset will be
        incremented.

Return Value:

    None.

--*/

KERNEL_API
VOID
MmIoBufferDecrementOffset (
    PIO_BUFFER IoBuffer,
    UINTN OffsetDecrement
    );

/*++

Routine Description:

    This routine decrements the I/O buffer's current offset by the given amount.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    OffsetDecrement - Supplies the number of bytes by which the offset will be
        incremented.

Return Value:

    None.

--*/

PHYSICAL_ADDRESS
MmGetIoBufferPhysicalAddress (
    PIO_BUFFER IoBuffer,
    UINTN IoBufferOffset
    );

/*++

Routine Description:

    This routine returns the physical address at a given offset within an I/O
    buffer.

Arguments:

    IoBuffer - Supplies a pointer to an I/O buffer.

    IoBufferOffset - Supplies a byte offset into the I/O buffer.

Return Value:

    Returns the physical address of the memory at the given offset within the
    I/O buffer.

--*/

KERNEL_API
PVOID
MmGetVirtualMemoryWarningEvent (
    VOID
    );

/*++

Routine Description:

    This routine returns the memory manager's system virtual memory warning
    event. This event is signaled whenever there is a change in system virtual
    memory's warning level.

Arguments:

    None.

Return Value:

    Returns a pointer to the virutal memory warning event.

--*/

KERNEL_API
MEMORY_WARNING_LEVEL
MmGetVirtualMemoryWarningLevel (
    VOID
    );

/*++

Routine Description:

    This routine returns the current system virtual memory warning level.

Arguments:

    None.

Return Value:

    Returns the current virtual memory warning level.

--*/

UINTN
MmGetTotalVirtualMemory (
    VOID
    );

/*++

Routine Description:

    This routine returns the size of the kernel virtual address space, in bytes.

Arguments:

    None.

Return Value:

    Returns the total number of bytes in the kernel virtual address space.

--*/

UINTN
MmGetFreeVirtualMemory (
    VOID
    );

/*++

Routine Description:

    This routine returns the number of unallocated bytes in the kernel virtual
    address space.

Arguments:

    None.

Return Value:

    Returns the total amount of free kernel virtual memory, in bytes.

--*/

KERNEL_API
PVOID
MmMapPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN SizeInBytes,
    BOOL Writable,
    BOOL WriteThrough,
    BOOL CacheDisabled
    );

/*++

Routine Description:

    This routine maps a physical address into kernel VA space. It is meant so
    that system components can access memory mapped hardware.

Arguments:

    PhysicalAddress - Supplies a pointer to the physical address.

    SizeInBytes - Supplies the size in bytes of the mapping. This will be
        rounded up to the nearest page size.

    Writable - Supplies a boolean indicating if the memory is to be marked
        writable (TRUE) or read-only (FALSE).

    WriteThrough - Supplies a boolean indicating if the memory is to be marked
        write-through (TRUE) or write-back (FALSE).

    CacheDisabled - Supplies a boolean indicating if the memory is to be mapped
        uncached.

Return Value:

    Returns a pointer to the virtual address of the mapping on success, or
    NULL on failure.

--*/

KERNEL_API
VOID
MmUnmapAddress (
    PVOID VirtualAddress,
    UINTN SizeInBytes
    );

/*++

Routine Description:

    This routine unmaps memory mapped with MmMapPhysicalMemory.

Arguments:

    VirtualAddress - Supplies the virtual address to unmap.

    SizeInBytes - Supplies the number of bytes to unmap.

Return Value:

    None.

--*/

KERNEL_API
ULONG
MmPageSize (
    VOID
    );

/*++

Routine Description:

    This routine returns the size of a page of memory.

Arguments:

    None.

Return Value:

    Returns the size of one page of memory (ie the minimum mapping granularity).

--*/

KERNEL_API
ULONG
MmPageShift (
    VOID
    );

/*++

Routine Description:

    This routine returns the amount to shift by to truncate an address to a
    page number.

Arguments:

    None.

Return Value:

    Returns the amount to shift to reach page granularity.

--*/

KERNEL_API
PBLOCK_ALLOCATOR
MmCreateBlockAllocator (
    ULONG BlockSize,
    ULONG Alignment,
    ULONG ExpansionCount,
    ULONG Flags,
    ULONG Tag
    );

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

KERNEL_API
VOID
MmDestroyBlockAllocator (
    PBLOCK_ALLOCATOR Allocator
    );

/*++

Routine Description:

    This routine destroys a block allocator, freeing all of its allocations
    and releasing all memory associated with it.

Arguments:

    Allocator - Supplies a pointer to the allocator to release.

Return Value:

    None.

--*/

KERNEL_API
PVOID
MmAllocateBlock (
    PBLOCK_ALLOCATOR Allocator,
    PPHYSICAL_ADDRESS AllocationPhysicalAddress
    );

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

KERNEL_API
VOID
MmFreeBlock (
    PBLOCK_ALLOCATOR Allocator,
    PVOID Allocation
    );

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

VOID
MmHandleFault (
    ULONG FaultFlags,
    PVOID FaultingAddress,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine handles access faults for the kernel.

Arguments:

    FaultFlags - Supplies a bitfield of flags regarding the fault. See
        FAULT_FLAG_* definitions.

    FaultingAddress - Supplies the address that caused the page fault.

    TrapFrame - Supplies a pointer to the state of the machine when the page
        fault occurred.

Return Value:

    None.

--*/

KSTATUS
MmGetMemoryStatistics (
    PMM_STATISTICS Statistics
    );

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

PVOID
MmAllocateKernelStack (
    UINTN Size
    );

/*++

Routine Description:

    This routine allocates memory to be used as a kernel stack.

Arguments:

    Size - Supplies the size of the kernel stack to allocate, in bytes.

Return Value:

    Returns a pointer to the base of the stack on success, or NULL on failure.

--*/

VOID
MmFreeKernelStack (
    PVOID StackBase,
    UINTN Size
    );

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

VOID
MmMdInitDescriptorList (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    MDL_ALLOCATION_SOURCE AllocationSource
    );

/*++

Routine Description:

    This routine initializes a memory descriptor list.

Arguments:

    Mdl - Supplies a pointer to the MDL to initialize.

    AllocationSource - Supplies the way that additional descriptors should be
        allocated.

Return Value:

    None.

--*/

VOID
MmMdDestroyDescriptorList (
    PMEMORY_DESCRIPTOR_LIST Mdl
    );

/*++

Routine Description:

    This routine destroys a memory descriptor list. It frees all descriptors.

Arguments:

    Mdl - Supplies a pointer to the MDL to destroy.

Return Value:

    None.

--*/

VOID
MmMdInitDescriptor (
    PMEMORY_DESCRIPTOR Descriptor,
    ULONGLONG MinimumAddress,
    ULONGLONG MaximumAddress,
    MEMORY_TYPE Type
    );

/*++

Routine Description:

    This routine initializes a memory descriptor. Unaligned addresses are
    expanded out to page boundaries.

Arguments:

    Descriptor - Supplies a pointer to the uninitialized descriptor.

    MinimumAddress - Supplies the base address of the descriptor.

    MaximumAddress - Supplies the top address of the descriptor. This is the
        first address NOT described by the descriptor.

    Type - Supplies the memory type of the descriptor.

Return Value:

    None.

--*/

KSTATUS
MmMdAddDescriptorToList (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    PMEMORY_DESCRIPTOR NewDescriptor
    );

/*++

Routine Description:

    This routine adds the given descriptor to the descriptor list, regardless
    of what other descriptors are currently describing that region. This
    routine is useful for overriding regions described incorrectly by the
    firmware.

Arguments:

    Mdl - Supplies a pointer to the destination descriptor list the descriptor
        should be added to.

    NewDescriptor - Supplies a pointer to the descriptor to be added.

Return Value:

    Status code.

--*/

PMEMORY_DESCRIPTOR
MmMdLookupDescriptor (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    ULONGLONG StartAddress,
    ULONGLONG EndAddress
    );

/*++

Routine Description:

    This routine finds the memory descriptor corresponding to the given address.

Arguments:

    Mdl - Supplies a pointer to the descriptor list to search through.

    StartAddress - Supplies the first valid address of the region being
        queried for.

    EndAddress - Supplies the first address beyond the region being queried.
        In other words, the end address is not inclusive.

Return Value:

    Returns a pointer to the descriptor that covers the given addres, or NULL
    if the address is not described by the list.

--*/

PMEMORY_DESCRIPTOR
MmMdIsRangeFree (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    ULONGLONG StartAddress,
    ULONGLONG EndAddress
    );

/*++

Routine Description:

    This routine determines if the given memory range is marked as free.

Arguments:

    Mdl - Supplies a pointer to the descriptor list to search through.

    StartAddress - Supplies the first valid address of the region being
        queried for.

    EndAddress - Supplies the first address beyond the region being queried.
        In other words, the end address is not inclusive.

Return Value:

    Returns a pointer to the descriptor with the free memory type that covers
    the given address range.

    NULL if entire specified range is not free.

--*/

KSTATUS
MmMdRemoveRangeFromList (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    ULONGLONG StartAddress,
    ULONGLONG EndAddress
    );

/*++

Routine Description:

    This routine removes all descriptors from the given list that are within
    the given memory range. Overlapping descriptors are truncated.

Arguments:

    Mdl - Supplies a pointer to the descriptor list to remove from.

    StartAddress - Supplies the first valid address of the region being removed.

    EndAddress - Supplies the first address beyond the region being removed.
        In other words, the end address is not inclusive.

Return Value:

    Status code.

--*/

VOID
MmMdRemoveDescriptorFromList (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    PMEMORY_DESCRIPTOR Descriptor
    );

/*++

Routine Description:

    This routine removes the given memory descriptor from the descriptor list.

Arguments:

    Mdl - Supplies a pointer to the descriptor list to remove from.

    Descriptor - Supplies a pointer to the descriptor to remove.

Return Value:

    None.

--*/

VOID
MmMdPrintMdl (
    PMEMORY_DESCRIPTOR_LIST Mdl
    );

/*++

Routine Description:

    This routine prints a memory descriptor list into a readable format.

Arguments:

    Mdl - Supplies a pointer to the descriptor list to print.

Return Value:

    None.

--*/

KSTATUS
MmMdAllocateFromMdl (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    PULONGLONG Address,
    ULONGLONG Size,
    ULONG Alignment,
    ULONGLONG Min,
    ULONGLONG Max,
    MEMORY_TYPE MemoryType,
    ALLOCATION_STRATEGY Strategy
    );

/*++

Routine Description:

    This routine allocates a piece of free memory from the given descriptor
    list, and marks it as the given type in the list.

Arguments:

    Mdl - Supplies a pointer to the descriptor list to allocate memory from.

    Address - Supplies a pointer to where the allocation will be returned.

    Size - Supplies the size of the required space.

    Alignment - Supplies the alignment requirement for the allocation, in bytes.
        Valid values are powers of 2. Set to 1 or 0 to specify no alignment
        requirement.

    Min - Supplies the minimum address to allocate.

    Max - Supplies the maximum address to allocate.

    MemoryType - Supplies the type of memory to mark the allocation as.

    Strategy - Supplies the memory allocation strategy for this allocation.

Return Value:

    STATUS_SUCCESS if the allocation was successful.

    STATUS_INVALID_PARAMETER if a page count of 0 was passed or the address
        parameter was not filled out.

    STATUS_NO_MEMORY if the allocation request could not be filled.

--*/

KSTATUS
MmMdAllocateMultiple (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    ULONGLONG Size,
    ULONGLONG Count,
    MEMORY_TYPE MemoryType,
    PUINTN Addresses
    );

/*++

Routine Description:

    This routine allocates multiple native sized addresses from an MDL in a
    single pass.

Arguments:

    Mdl - Supplies a pointer to the descriptor list to allocate memory from.

    Size - Supplies the required size of each individual allocation. This must
        be a power of two. This is also assumed to be the alignment requirement.

    Count - Supplies the number of allocations required.

    MemoryType - Supplies the type of memory to mark the allocation as.

    Addresses - Supplies a pointer where the addresses will be returned on
        success.

Return Value:

    STATUS_SUCCESS if the allocation was successful.

    STATUS_NO_MEMORY if the allocation request could not be filled.

--*/

VOID
MmMdAddFreeDescriptorsToMdl (
    PMEMORY_DESCRIPTOR_LIST Mdl,
    PMEMORY_DESCRIPTOR NewDescriptor,
    ULONG Size
    );

/*++

Routine Description:

    This routine adds new free descriptors to the given memory descriptor list.

Arguments:

    Mdl - Supplies a pointer to the descriptor list to add free descriptors to.

    NewDescriptor - Supplies an array of new descriptors.

    Size - Supplies the size of the descriptor array, in bytes.

Return Value:

    None.

--*/

VOID
MmMdIterate (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR_LIST_ITERATION_ROUTINE IterationRoutine,
    PVOID Context
    );

/*++

Routine Description:

    This routine iterates over all the descriptors in the given list, calling
    the iteration routine for each one.

Arguments:

    DescriptorList - Supplies a pointer to the list to iterate over.

    IterationRoutine - Supplies a pointer to the routine to call for each
        descriptor in the list.

    Context - Supplies an optional opaque context passed to the iteration
        routine.

Return Value:

    None.

--*/

INTN
MmSysMapOrUnmapMemory (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to map a
    file object or unmap a region of the current process' address space.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
MmSysSetMemoryProtection (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to change
    memory region attributes.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
MmSysFlushMemory (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to flush a
    region of memory in the current process' to permanent storage.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
MmSysSetBreak (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine implements the system call for getting or modifying the
    program break.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

KSTATUS
MmCreateCopyOfUserModeString (
    PCSTR UserModeString,
    ULONG UserModeStringBufferLength,
    ULONG AllocationTag,
    PSTR *CreatedCopy
    );

/*++

Routine Description:

    This routine is a convenience method that captures a string from user mode
    and creates a paged-pool copy in kernel mode. The caller can be sure that
    the string pointer was properly sanitized and the resulting buffer is null
    terminated. The caller is responsible for freeing the memory returned by
    this function on success.

Arguments:

    UserModeString - Supplies the user mode pointer to the string.

    UserModeStringBufferLength - Supplies the size of the buffer containing the
        user mode string.

    AllocationTag - Supplies the allocation tag that should be used when
        creating the kernel buffer.

    CreatedCopy - Supplies a pointer where the paged pool allocation will be
        returned.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
MmCopyFromUserMode (
    PVOID KernelModePointer,
    PCVOID UserModePointer,
    UINTN Size
    );

/*++

Routine Description:

    This routine copies memory from user mode to kernel mode.

Arguments:

    KernelModePointer - Supplies the kernel mode pointer, the destination of
        the copy.

    UserModePointer - Supplies the untrusted user mode pointer, the source of
        the copy.

    Size - Supplies the number of bytes to copy.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the user mode memory is invalid or corrupt.

--*/

KERNEL_API
KSTATUS
MmCopyToUserMode (
    PVOID UserModePointer,
    PCVOID KernelModePointer,
    UINTN Size
    );

/*++

Routine Description:

    This routine copies memory to user mode from kernel mode.

Arguments:

    UserModePointer - Supplies the untrusted user mode pointer, the destination
        of the copy.

    KernelModePointer - Supplies the kernel mode pointer, the source of the
        copy.

    Size - Supplies the number of bytes to copy.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the user mode memory is invalid or corrupt.

--*/

KERNEL_API
KSTATUS
MmTouchUserModeBuffer (
    PVOID Buffer,
    UINTN Size,
    BOOL Write
    );

/*++

Routine Description:

    This routine touches a user mode buffer, validating it either for reading
    or writing. Note that the caller must also have the process VA space
    locked, or else this data is immediately stale.

Arguments:

    Buffer - Supplies a pointer to the buffer to probe.

    Size - Supplies the number of bytes to copy.

    Write - Supplies a boolean indicating whether to probe the memory for
        reading or writing.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the user mode memory is invalid.

--*/

BOOL
MmUserRead8 (
    PVOID Buffer,
    PUCHAR Value
    );

/*++

Routine Description:

    This routine performs a 8-bit read from user mode.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

BOOL
MmUserWrite8 (
    PVOID Buffer,
    UCHAR Value
    );

/*++

Routine Description:

    This routine performs a 8-bit write to user mode.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

BOOL
MmUserRead16 (
    PVOID Buffer,
    PUSHORT Value
    );

/*++

Routine Description:

    This routine performs a 16-bit read from user mode. This is assumed to be
    two-byte aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

BOOL
MmUserWrite16 (
    PVOID Buffer,
    USHORT Value
    );

/*++

Routine Description:

    This routine performs a 16-bit write to user mode. This is assumed to be
    two-byte aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

BOOL
MmUserRead32 (
    PVOID Buffer,
    PULONG Value
    );

/*++

Routine Description:

    This routine performs a 32-bit read from user mode. This is assumed to be
    naturally aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

BOOL
MmUserWrite32 (
    PVOID Buffer,
    ULONG Value
    );

/*++

Routine Description:

    This routine performs a 32-bit write to user mode. This is assumed to be
    naturally aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

BOOL
MmUserRead64 (
    PVOID Buffer,
    PULONGLONG Value
    );

/*++

Routine Description:

    This routine performs a 32-bit read from user mode. This is assumed to be
    naturally aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to read.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    TRUE if the read succeeded.

    FALSE if the read failed.

--*/

BOOL
MmUserWrite64 (
    PVOID Buffer,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine performs a 32-bit write to user mode. This is assumed to be
    naturally aligned.

Arguments:

    Buffer - Supplies a pointer to the buffer to write to.

    Value - Supplies the value to write.

Return Value:

    TRUE if the write succeeded.

    FALSE if the write failed.

--*/

PMEMORY_RESERVATION
MmCreateMemoryReservation (
    PVOID PreferredVirtualAddress,
    UINTN Size,
    PVOID Min,
    PVOID Max,
    ALLOCATION_STRATEGY FallbackStrategy,
    BOOL KernelMode
    );

/*++

Routine Description:

    This routine creates a virtual address reservation for the current process.

Arguments:

    PreferredVirtualAddress - Supplies the preferred virtual address of the
        reservation. Supply NULL to indicate no preference.

    Size - Supplies the size of the requested reservation, in bytes.

    Min - Supplies the minimum virtual address to allocate.

    Max - Supplies the maximum virtual address to allocate.

    FallbackStrategy - Supplies the fallback memory allocation strategy in
        case the preferred address isn't available (or wasn't supplied).

    KernelMode - Supplies a boolean indicating whether the VA reservation must
        be in kernel mode (TRUE) or user mode (FALSE).

Return Value:

    Returns a pointer to the reservation structure on success.

    NULL on failure.

--*/

VOID
MmFreeMemoryReservation (
    PMEMORY_RESERVATION Reservation
    );

/*++

Routine Description:

    This routine destroys a memory reservation. All memory must be unmapped
    and freed prior to this call.

Arguments:

    Reservation - Supplies a pointer to the reservation structure returned when
        the reservation was made.

Return Value:

    None.

--*/

KSTATUS
MmInitializeMemoryAccounting (
    PMEMORY_ACCOUNTING Accountant,
    ULONG Flags
    );

/*++

Routine Description:

    This routine initializes a memory accounting structure.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure to
        initialize.

    Flags - Supplies flags to control the behavior of the accounting. See the
        MEMORY_ACCOUNTING_FLAG_* definitions for valid flags.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid flag was passed.

--*/

KSTATUS
MmReinitializeUserAccounting (
    PMEMORY_ACCOUNTING Accountant
    );

/*++

Routine Description:

    This routine resets the memory reservations on a user memory accounting
    structure to those of a clean process.

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure to
        initialize.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if an invalid flag was passed.

--*/

VOID
MmDestroyMemoryAccounting (
    PMEMORY_ACCOUNTING Accountant
    );

/*++

Routine Description:

    This routine destroys a memory accounting structure, freeing all memory
    associated with it (except the MEMORY_ACCOUNTING structure itself, which
    was provided to the initialize function separately).

Arguments:

    Accountant - Supplies a pointer to the memory accounting structure to
        destroy.

Return Value:

    None.

--*/

KSTATUS
MmCloneAddressSpace (
    PADDRESS_SPACE Source,
    PADDRESS_SPACE Destination
    );

/*++

Routine Description:

    This routine makes a clone of one process' entire address space into
    another process. The copy is not shared memory, the destination segments
    are marked copy on write. This includes copying the mapping for the user
    shared data page.

Arguments:

    Source - Supplies a pointer to the source address space to copy.

    Destination - Supplies a pointer to the newly created destination to copy
        the sections to.

Return Value:

    Status code.

--*/

VOID
MmIdentityMapStartupStub (
    ULONG PageCount,
    PVOID *Allocation,
    PVOID *PageDirectory
    );

/*++

Routine Description:

    This routine allocates and identity maps pages for use by application
    processor startup code.

Arguments:

    PageCount - Supplies the number of pages to allocate and map.

    Allocation - Supplies a pointer where the virtual/physical address will
        be returned on success.

    PageDirectory - Supplies a pointer where the current page directory will be
        returned.

Return Value:

    None.

--*/

VOID
MmUnmapStartupStub (
    PVOID Allocation,
    ULONG PageCount
    );

/*++

Routine Description:

    This routine unmaps memory allocated and identity mapped for use by the
    AP startup stub.

Arguments:

    Allocation - Supplies the allocation.

    PageCount - Supplies the number of pages in the allocation.

Return Value:

    None.

--*/

VOID
MmUpdatePageDirectory (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    UINTN Size
    );

/*++

Routine Description:

    This routine updates the kernel mode entries in the given page directory
    for the given virtual address range so that they're current.

Arguments:

    AddressSpace - Supplies a pointer to the address space.

    VirtualAddress - Supplies the base virtual address of the range to be
        synchronized.

    Size - Supplies the size of the virtual address range to synchronize.

Return Value:

    None.

--*/

ULONG
MmValidateMemoryAccessForDebugger (
    PVOID Address,
    ULONG Length,
    PBOOL Writable
    );

/*++

Routine Description:

    This routine validates that access to a specified location in memory will
    not cause a page fault. It is intended to be called only by the debugger.

Arguments:

    Address - Supplies the virtual address of the memory that will be read or
        written.

    Length - Supplies how many bytes at that location the caller would like to
        read or write.

    Writable - Supplies an optional pointer that receives a boolean indicating
        whether or not the memory range is mapped writable.

Return Value:

    Returns the number of bytes from the beginning of the address that are
    accessible. If the memory is completely available, the return value will be
    equal to the Length parameter. If the memory is completely paged out, 0
    will be returned.

--*/

VOID
MmModifyAddressMappingForDebugger (
    PVOID Address,
    BOOL Writable,
    PBOOL WasWritable
    );

/*++

Routine Description:

    This routine modifies the mapping properties for the page that contains the
    given address.

Arguments:

    Address - Supplies the virtual address of the memory whose mapping
        properties are to be changed.

    Writable - Supplies a boolean indicating whether or not to make the page
        containing the address writable (TRUE) or read-only (FALSE).

    WasWritable - Supplies a pointer that receives a boolean indicating whether
        or not the page was writable (TRUE) or read-only (FALSE) before any
        modifications.

Return Value:

    None.

--*/

VOID
MmSwitchAddressSpace (
    PVOID Processor,
    PVOID CurrentStack,
    PADDRESS_SPACE AddressSpace
    );

/*++

Routine Description:

    This routine switches to the given address space.

Arguments:

    Processor - Supplies a pointer to the current processor block.

    CurrentStack - Supplies the address of the current thread's kernel stack.
        This routine will ensure this address is visible in the address space
        being switched to. Stacks must not cross page directory boundaries.

    AddressSpace - Supplies a pointer to the address space to switch to.

Return Value:

    None.

--*/

VOID
MmVolumeArrival (
    PCSTR VolumeName,
    ULONG VolumeNameLength,
    BOOL SystemVolume
    );

/*++

Routine Description:

    This routine implements the memory manager's response to a new volume in
    the system.

Arguments:

    VolumeName - Supplies the full path to the new volume.

    VolumeNameLength - Supplies the length of the volume name buffer, including
        the null terminator, in bytes.

    SystemVolume - Supplies a boolean indicating if this is the system volume
        or not.

Return Value:

    None.

--*/

BOOL
MmRequestPagingOut (
    UINTN FreePageTarget
    );

/*++

Routine Description:

    This routine schedules the backgroung paging thread to run, hopefully
    freeing up some memory. This must be called at low level. This routine is
    asynchronous, it will signal the paging thread and return immediately. The
    request may be ignored or coalesced with other paging out requests.

Arguments:

    FreePageTarget - Supplies the target number of free physical pages the
        caller would like to see in the system.

Return Value:

    Returns TRUE if a request was submitted or FALSE otherwise (e.g. paging is
    not enabled).

--*/

KSTATUS
MmVolumeRemoval (
    PVOID Device
    );

/*++

Routine Description:

    This routine implements the memory manager's response to a volume being
    removed from the system.

Arguments:

    Device - Supplies a pointer to the device (volume) being removed.

Return Value:

    Status code.

--*/

KSTATUS
MmAllocatePageFileSpace (
    PIMAGE_BACKING ImageBacking,
    UINTN Size
    );

/*++

Routine Description:

    This routine allocates space from a page file.

Arguments:

    ImageBacking - Supplies a pointer to an image backing structure that
        recevies the allocated page file space.

    Size - Supplies the size of the page file space to allocate, in bytes.

Return Value:

    STATUS_SUCCESS on success. In this case the image backing structure
    parameterwill be filled out.

    STATUS_INSUFFICIENT_RESOURCES if the request could not be satisified.

--*/

VOID
MmFreePageFileSpace (
    PIMAGE_BACKING ImageBacking,
    UINTN Size
    );

/*++

Routine Description:

    This routine frees space from a page file.

Arguments:

    ImageBacking - Supplies a pointer to the page file image backing to release.

    Size - Supplies the size of the image backing.

Return Value:

    None.

--*/

VOID
MmFreePartialPageFileSpace (
    PIMAGE_BACKING ImageBacking,
    UINTN PageOffset,
    UINTN PageCount
    );

/*++

Routine Description:

    This routine frees a portion of the original space allocated in the page
    file.

Arguments:

    ImageBacking - Supplies a pointer to the image backing taking up page file
        space.

    PageOffset - Supplies the offset in pages to the beginning of the region
        that should be freed.

    PageCount - Supplies the number of pages to free.

Return Value:

    None.

--*/

KSTATUS
MmPageFilePerformIo (
    PIMAGE_BACKING ImageBacking,
    PIO_BUFFER IoBuffer,
    UINTN Offset,
    UINTN SizeInBytes,
    ULONG Flags,
    ULONG TimeoutInMilliseconds,
    BOOL Write,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine performs I/O on the page file region described by the given
    image backing.

Arguments:

    ImageBacking - Supplies a pointer to the image backing that holds a device
        handle and offset for the page file region.

    IoBuffer - Supplies a pointer to an I/O buffer to use for the read or write.

    Offset - Supplies the offset from the beginning of the file or device where
        the I/O should be done.

    SizeInBytes - Supplies the number of bytes to read or write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    BytesCompleted - Supplies the a pointer where the number of bytes actually
        read or written will be returned.

Return Value:

    Status code.

--*/

KSTATUS
MmMapFileSection (
    HANDLE FileHandle,
    IO_OFFSET FileOffset,
    PVM_ALLOCATION_PARAMETERS VaRequest,
    ULONG Flags,
    BOOL KernelSpace,
    PMEMORY_RESERVATION Reservation
    );

/*++

Routine Description:

    This routine maps a file or a portion of a file into virtual memory space
    of the current process. This routine must be called below dispatch level.

Arguments:

    FileHandle - Supplies the open file handle.

    FileOffset - Supplies the offset, in bytes, from the start of the file
        where the mapping should begin.

    VaRequest - Supplies a pointer to the virtual address allocation
        parameters. If the supplied size is zero, then this routine will
        attempt to map until the end of the file. The alignment will be set
        to a page size, and the memory type will be set to reserved.

    Flags - Supplies flags governing the mapping of the section. See
        IMAGE_SECTION_* definitions.

    KernelSpace - Supplies a boolean indicating whether to map the section in
        kernel space or user space.

    Reservation - Supplies an optional pointer to a memory reservation for the
        desired mapping. A reservation is required only if several mappings
        need to be allocated in the same range together for any one mapping to
        be useful.

Return Value:

    Status code.

--*/

KSTATUS
MmUnmapFileSection (
    PVOID Process,
    PVOID FileMapping,
    UINTN Size,
    PMEMORY_RESERVATION Reservation
    );

/*++

Routine Description:

    This routine unmaps a file section. This routine must be called at low
    level. For kernel mode, this must specify a single whole image section.

Arguments:

    Process - Supplies a pointer to the process containing the section to
        unmap. Supply NULL to unmap from the current process.

    FileMapping - Supplies a pointer to the file mapping.

    Size - Supplies the size in bytes of the region to unmap.

    Reservation - Supplies an optional pointer to the reservation that the
        section was mapped under. If the mapping was not done under a
        memory reservation, supply NULL. If the mapping was done under a
        memory reservation, that reservation MUST be supplied here.

Return Value:

    Status code.

--*/

VOID
MmCleanUpProcessMemory (
    PVOID ExitedProcess
    );

/*++

Routine Description:

    This routine cleans up any leftover allocations made under the given
    process.

Arguments:

    ExitedProcess - Supplies a pointer to the process to clean up.

Return Value:

    None.

--*/

KSTATUS
MmMapUserSharedData (
    PADDRESS_SPACE AddressSpace
    );

/*++

Routine Description:

    This routine maps the user shared data at a fixed address in a new
    process' address space.

Arguments:

    AddressSpace - Supplies the address space to map the user shared data page
        into.

Return Value:

    Status code.

--*/

PVOID
MmGetUserSharedData (
    VOID
    );

/*++

Routine Description:

    This routine returns the kernel virtual address of the user shared data
    area.

Arguments:

    None.

Return Value:

    The kernel mode address of the user shared data page.

--*/

PADDRESS_SPACE
MmCreateAddressSpace (
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
MmDestroyAddressSpace (
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

PIMAGE_SECTION_LIST
MmCreateImageSectionList (
    VOID
    );

/*++

Routine Description:

    This routine creates an image section list.

Arguments:

    None.

Return Value:

    Returns a pointer to the new image section list.

--*/

VOID
MmDestroyImageSectionList (
    PIMAGE_SECTION_LIST ImageSectionList
    );

/*++

Routine Description:

    This routine destroys an image section list.

Arguments:

    ImageSectionList - Supplies a pointer to the image section list to destroy.

Return Value:

    None.

--*/

KSTATUS
MmUnmapImageSectionList (
    PIMAGE_SECTION_LIST ImageSectionList,
    IO_OFFSET Offset,
    ULONGLONG Size,
    ULONG Flags
    );

/*++

Routine Description:

    This routine unmaps all pages in each image section in the given image
    section list starting at the given offset and for the supplied size.

Arguments:

    ImageSectionList - Supplies a pointer to an image section list.

    Offset - Supplies the start offset beyond which all mappings in each image
        section will be unmapped. The offset should be page aligned.

    Size - Supplies the size of the region to unmap, in bytes. The size should
        be page aligned.

    Flags - Supplies a bitmask of flags for the unmap. See
        IMAGE_SECTION_UNMAP_FLAG_* for definitions.

    PageWasDirty - Supplies a pointer where a boolean will be returned
        indicating if any page that was unmapped was dirty. This parameter is
        optional.

Return Value:

    Status code.

--*/

KSTATUS
MmChangeImageSectionRegionAccess (
    PVOID Address,
    UINTN Size,
    ULONG NewAccess
    );

/*++

Routine Description:

    This routine sets the memory region protection for the given address range.

Arguments:

    Address - Supplies the starting address of the region to change.

    Size - Supplies the size of the region to change.

    NewAccess - Supplies the new access permissions to set. See IMAGE_SECTION_*
        definitions. Only the read, write, and execute flags can be changed.

Return Value:

    Status code.

--*/

PVOID
MmGetObjectForAddress (
    PVOID Address,
    PUINTN Offset,
    PBOOL Shared
    );

/*++

Routine Description:

    This routine returns a pointer to the object backing the memory at the
    given user mode address. This is an opaque object with an increased
    reference count on it.

Arguments:

    Address - Supplies the user mode address to look up.

    Offset - Supplies a pointer where the offset in bytes from the base of the
        object's virtual region will be returned.

    Shared - Supplies a pointer indicating whether the memory is a shared file
        mapping (TRUE) or either a private file mapping or just general
        memory (FALSE).

Return Value:

    Returns a pointer to the object that owns this user mode address for the
    current process. The caller must release the reference held on this object.

    NULL if the address passed in is invalid or not mapped.

--*/

VOID
MmReleaseObjectReference (
    PVOID Object,
    BOOL Shared
    );

/*++

Routine Description:

    This routine releases the reference acquired by getting the object for a
    user mode address.

Arguments:

    Object - Supplies a pointer to the object returned when the address was
        looked up.

    Shared - Supplies the shared boolean that was returned when the address was
        looked up. This is needed to know how to release the object.

Return Value:

    None.

--*/

KSTATUS
MmUserModeDebuggerWrite (
    PVOID KernelBuffer,
    PVOID UserDestination,
    UINTN Size
    );

/*++

Routine Description:

    This routine performs a user mode debugger write to the current
    process memory. This routine may convert a read-only image section it
    finds to a writable section.

Arguments:

    KernelBuffer - Supplies a pointer to the kernel-mode buffer containing
        the data to write.

    UserDestination - Supplies the destination buffer where the contents
        should be written.

    Size - Supplies the number of bytes to write.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the buffer is not valid.

    STATUS_ACCESS_DENIED if an attempt was made to write to a read-only
    shared section.

--*/

KERNEL_API
PVOID
MmGetPhysicalMemoryWarningEvent (
    VOID
    );

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

KERNEL_API
MEMORY_WARNING_LEVEL
MmGetPhysicalMemoryWarningLevel (
    VOID
    );

/*++

Routine Description:

    This routine returns the current physical memory warning level.

Arguments:

    None.

Return Value:

    Returns the current physical memory warning level.

--*/

UINTN
MmGetTotalPhysicalPages (
    VOID
    );

/*++

Routine Description:

    This routine gets the total physical pages in the system.

Arguments:

    None.

Return Value:

    Returns the total number of physical pages present in the system.

--*/

UINTN
MmGetTotalFreePhysicalPages (
    VOID
    );

/*++

Routine Description:

    This routine returns the total number of free physical pages in the system.

Arguments:

    None.

Return Value:

    Returns the total number of free physical pages in the system.

--*/

VOID
MmFreePhysicalPages (
    PHYSICAL_ADDRESS PhysicalAddress,
    UINTN PageCount
    );

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

VOID
MmSetPageCacheEntryForPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    PVOID PageCacheEntry
    );

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

KERNEL_API
KSTATUS
MmFlushBufferForDataIn (
    PVOID Buffer,
    UINTN SizeInBytes
    );

/*++

Routine Description:

    This routine flushes a buffer in preparation for incoming I/O from a device.

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the region was user mode and an address in the
    region was not valid. Kernel mode addresses are always expected to be
    valid.

--*/

KERNEL_API
KSTATUS
MmFlushBufferForDataOut (
    PVOID Buffer,
    UINTN SizeInBytes
    );

/*++

Routine Description:

    This routine flushes a buffer in preparation for outgoing I/O to a device.

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the region was user mode and an address in the
    region was not valid. Kernel mode addresses are always expected to be
    valid.

--*/

KERNEL_API
KSTATUS
MmFlushBufferForDataIo (
    PVOID Buffer,
    UINTN SizeInBytes
    );

/*++

Routine Description:

    This routine flushes a buffer in preparation for data that is both
    incoming and outgoing (ie the buffer is read from and written to by an
    external device).

Arguments:

    Buffer - Supplies the virtual address of the buffer to flush. This buffer
        must be cache-line aligned.

    SizeInBytes - Supplies the size of the buffer to flush, in bytes. This
        size must also be cache-line aligned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if the region was user mode and an address in the
    region was not valid. Kernel mode addresses are always expected to be
    valid.

--*/

KERNEL_API
KSTATUS
MmSyncCacheRegion (
    PVOID Address,
    UINTN Size
    );

/*++

Routine Description:

    This routine unifies the instruction and data caches for the given region,
    probably after a region of executable code was modified. This does not
    necessarily flush data to the point where it's observable to device DMA
    (called the point of coherency).

Arguments:

    Address - Supplies the address to flush.

    Size - Supplies the number of bytes in the region to flush.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_ACCESS_VIOLATION if one of the addresses in the given range was not
    valid.

--*/

INTN
MmSysFlushCache (
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine responds to system calls from user mode requesting to
    invalidate the instruction cache after changing a code region.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

KSTATUS
MmGetSetSystemInformation (
    BOOL FromKernelMode,
    MM_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets system information.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

