/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    heap.c

Abstract:

    This module contains a dynamic memory allocation implementation based on
    Doug Lea's dlmalloc implementation, version 2.8.6.

Author:

    Evan Green 20-May-2014

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro isolates the least bit set of a bitmap.
//

#define HEAP_LEAST_BIT(_Value) ((_Value) & -(_Value))

//
// This macro returns a mask with all the bits to the left of the least bit
// of the given value on.
//

#define HEAP_LEFT_BITS(_Value) (((_Value) << 1) | -((_Value) << 1))

//
// This macro returns a mask with all the bits to the left of or equal to the
// least significant bit of the given value on.
//

#define HEAP_SAME_OR_LEFT_BITS(_Value) ((_Value) | -(_Value))

//
// This macro computes the index corresponding to the given bit.
//

#define HEAP_COMPUTE_BIT_INDEX(_Value) \
    (HEAP_BINDEX)RtlCountTrailingZeros(_Value)

//
// This macro determines if the given value is aligned.
//

#define HEAP_IS_ALIGNED(_Value) \
    (((UINTN)(_Value) & HEAP_CHUNK_ALIGN_MASK) == 0)

//
// This macro returns the offset needed to align the given value.
//

#define HEAP_ALIGNMENT_OFFSET(_Value)                                   \
    ((((UINTN)(_Value) & HEAP_CHUNK_ALIGN_MASK) == 0) ? 0 :             \
     ((HEAP_ALIGNMENT - ((UINTN)(_Value) & HEAP_CHUNK_ALIGN_MASK)) &    \
      HEAP_CHUNK_ALIGN_MASK))

//
// This macro determines if the heap is initialized by looking at the top chunk.
//

#define HEAP_IS_INITIALIZED(_Heap) ((_Heap)->Top != NULL)

//
// This macro returns non-zero if the given segment was handed to the heap, and
// not allocated by it.
//

#define HEAP_IS_EXTERNAL_SEGMENT(_Segment) \
    (((_Segment)->Flags & HEAP_SEGMENT_FLAG_EXTERNAL) != 0)

//
// This macro returns non-zero if the given segment contains the given address.
//

#define HEAP_SEGMENT_HOLDS(_Segment, _Address)                  \
    (((PCHAR)(_Address) >= (_Segment)->Base) &&                 \
     ((PCHAR)(_Address) < (_Segment)->Base + (_Segment)->Size))

//
// This macro determines if the given size goes in the small bins.
//

#define HEAP_IS_SMALL(_Size) \
    (((_Size) >> HEAP_SMALL_BIN_SHIFT) < HEAP_SMALL_BIN_COUNT)

//
// This macro returns a pointer to the bin at the given index.
//

#define HEAP_SMALL_BIN_AT(_Heap, _Index)                            \
    ((PHEAP_CHUNK)(((PCHAR)&((_Heap)->SmallBins[(_Index) << 1])) -  \
                   FIELD_OFFSET(HEAP_CHUNK, Next)))

//
// This macro returns a pointer to the root of the tree for the given bin.
//

#define HEAP_TREE_BIN_AT(_Heap, _Index) (&((_Heap)->TreeBins[_Index]))

//
// These macros convert between allocation pointers returned to callers and
// chunk pointers.
//

#define HEAP_CHUNK_TO_MEMORY(_Chunk) \
    ((PVOID)((PCHAR)(_Chunk) + FIELD_OFFSET(HEAP_CHUNK, Next)))

#define HEAP_MEMORY_TO_CHUNK(_Memory) \
    ((PHEAP_CHUNK)((PCHAR)(_Memory) - FIELD_OFFSET(HEAP_CHUNK, Next)))

//
// This macro aligns the given address and casts it to a chunk.
//

#define HEAP_ALIGN_AS_CHUNK(_Address)       \
    ((PHEAP_CHUNK)((_Address) +             \
                   HEAP_ALIGNMENT_OFFSET(HEAP_CHUNK_TO_MEMORY(_Address))))

//
// This macro pads a request size to a usable size.
//

#define HEAP_PAD_REQUEST(_Request)                                  \
    (((_Request) + HEAP_CHUNK_OVERHEAD + HEAP_CHUNK_ALIGN_MASK) &   \
     ~HEAP_CHUNK_ALIGN_MASK)

//
// This macro converts a request size into a chunk size.
//

#define HEAP_REQUEST_TO_SIZE(_RequestSize) \
    (((_RequestSize) < HEAP_MIN_REQUEST) ? HEAP_MIN_CHUNK_SIZE : \
     HEAP_PAD_REQUEST(_RequestSize))

//
// This macro returns the overhead for the given chunk.
//

#define HEAP_OVERHEAD_FOR(_Chunk)                               \
    (HEAP_CHUNK_IS_MMAPPED(_Chunk) ? HEAP_MMAP_CHUNK_OVERHEAD : \
     HEAP_CHUNK_OVERHEAD)

//
// This macro gets the bin index for a small bin.
//

#define HEAP_SMALL_INDEX(_Size) (HEAP_BINDEX)((_Size) >> HEAP_SMALL_BIN_SHIFT)

//
// This macro returns the chunk size for a given chunk.
//

#define HEAP_CHUNK_SIZE(_Chunk) ((_Chunk)->Header & ~HEAP_CHUNK_FLAGS)

//
// This macro converts a small bin index back into a size.
//

#define HEAP_SMALL_INDEX_TO_SIZE(_Index) ((_Index) << HEAP_SMALL_BIN_SHIFT)

//
// These macros manipulate the small map within the heap.
//

#define HEAP_MARK_SMALL_MAP(_Heap, _Index) \
    ((_Heap)->SmallMap |= HEAP_INDEX_TO_BIT(_Index))

#define HEAP_CLEAR_SMALL_MAP(_Heap, _Index) \
    ((_Heap)->SmallMap &= ~HEAP_INDEX_TO_BIT(_Index))

#define HEAP_IS_SMALL_MAP_MARKED(_Heap, _Index) \
    (((_Heap)->SmallMap & HEAP_INDEX_TO_BIT(_Index)) != 0)

//
// These macros manipulate the tree map within the heap.
//

#define HEAP_MARK_TREE_MAP(_Heap, _Index) \
    ((_Heap)->TreeMap |= HEAP_INDEX_TO_BIT(_Index))

#define HEAP_CLEAR_TREE_MAP(_Heap, _Index) \
    ((_Heap)->TreeMap &= ~HEAP_INDEX_TO_BIT(_Index))

#define HEAP_IS_TREE_MAP_MARKED(_Heap, _Index) \
    (((_Heap)->TreeMap & HEAP_INDEX_TO_BIT(_Index)) != 0)

//
// This macro determines if the given address is a valid heap address.
//

#define HEAP_OK_ADDRESS(_Heap, _Address) \
    ((PCHAR)(_Address) >= (_Heap)->LeastAddress)

//
// This macro detremines if the address of the next chunk is higher than the
// given base chunk.
//

#define HEAP_OK_NEXT(_Chunk, _Next) ((PCHAR)(_Chunk) < (PCHAR)(_Next))

//
// This macro clears the given bit in the small map.
//

#define HEAP_INDEX_TO_BIT(_Index) ((HEAP_BINMAP)(1) << (_Index))
#define HEAP_CLEAR_SMALL_MAP(_Heap, _Index) \
    ((_Heap)->SmallMap &= ~HEAP_INDEX_TO_BIT(_Index))

//
// These macros get the next and previous chunks.
//

#define HEAP_NEXT_CHUNK(_Chunk) \
    ((PHEAP_CHUNK)(((PCHAR)(_Chunk)) + ((_Chunk)->Header & ~HEAP_CHUNK_FLAGS)))

#define HEAP_PREVIOUS_CHUNK(_Chunk) \
    ((PHEAP_CHUNK)(((PCHAR)(_Chunk)) - ((_Chunk)->PreviousFooter)))

//
// This macro gets the leftmost child of a tree node.
//

#define HEAP_TREE_LEFTMOST_CHILD(_Node) \
    ((_Node)->Child[0] != NULL ? (_Node)->Child[0] : (_Node)->Child[1])

//
// This macro returns the bit representing the maximum resolved size in a tree
// bin at the given index.
//

#define HEAP_BIT_FOR_TREE_INDEX(_Index) \
    ((_Index) == HEAP_TREE_BIN_COUNT - 1) ? (sizeof(UINTN) * BITS_PER_BYTE) : \
    (((_Index) >> 1) + HEAP_TREE_BIN_SHIFT - 2)

//
// This macro returns the left shift needed to place the maximum resolved bit
// in a tree bin at the given index as the sign bit.
//

#define HEAP_LEFT_SHIFT_FOR_TREE_INDEX(_Index)          \
    (((_Index) == HEAP_TREE_BIN_COUNT - 1) ? 0 :        \
     (((sizeof(UINTN) * BITS_PER_BYTE) - 1U) -          \
      (((_Index) >> 1) + HEAP_TREE_BIN_SHIFT - 2)))

//
// This macro returns the size of the smallest chunk held in the tree bin
// with the given index.
//

#define HEAP_MIN_SIZE_FOR_TREE_INDEX(_Index)                    \
    (((UINTN)1) << (((_Index) >> 1) + HEAP_TREE_BIN_SHIFT) |    \
     (((UINTN)((_Index) & ((UINTN)1))) <<                       \
      (((_Index) >> 1) + HEAP_TREE_BIN_SHIFT - 1)))

//
// These macros extract bits out of the chunk header.
//

#define HEAP_CHUNK_IS_CURRENT_IN_USE(_Chunk) \
    ((_Chunk)->Header & HEAP_CHUNK_IN_USE)

#define HEAP_CHUNK_IS_PREVIOUS_IN_USE(_Chunk) \
    ((_Chunk)->Header & HEAP_CHUNK_PREVIOUS_IN_USE)

#define HEAP_CHUNK_IS_IN_USE(_Chunk) \
    (((_Chunk)->Header & HEAP_CHUNK_IN_USE_MASK) != HEAP_CHUNK_PREVIOUS_IN_USE)

#define HEAP_CHUNK_IS_MMAPPED(_Chunk) \
    (((_Chunk)->Header & HEAP_CHUNK_IN_USE_MASK) == 0)

#define HEAP_CHUNK_NEXT_PREVIOUS_IN_USE(_Chunk) \
    HEAP_CHUNK_IS_PREVIOUS_IN_USE(HEAP_NEXT_CHUNK(_Chunk))

#define HEAP_CHUNK_CLEAR_PREVIOUS_IN_USE(_Chunk) \
    ((_Chunk)->Header &= ~HEAP_CHUNK_PREVIOUS_IN_USE)

//
// This macro returns a pointer to a chunk at a given pointer plus an offset.
//

#define HEAP_CHUNK_PLUS_OFFSET(_Chunk, _Size) \
    ((PHEAP_CHUNK)(((PCHAR)(_Chunk)) + (_Size)))

#define HEAP_CHUNK_MINUS_OFFSET(_Chunk, _Size) \
    ((PHEAP_CHUNK)(((PCHAR)(_Chunk)) - (_Size)))

//
// This macro determines whether or not the trim function should be called.
// It's called when boatloads of space accumulate in the heap.
//

#define HEAP_SHOULD_TRIM(_Heap, _TopSize)           \
    ((((_TopSize) >= (_Heap)->TrimCheck)) &&        \
     ((_Heap)->FreeFunction != NULL) &&             \
     (((_Heap)->Flags & MEMORY_HEAP_FLAG_NO_PARTIAL_FREES) == 0))

//
// This macro marks the given chunk as in use and sets up the footer as well.
//

#define HEAP_CHUNK_SET_IN_USE(_Heap, _Chunk, _Size)                            \
    ((_Chunk)->Header = (((_Chunk)->Header & HEAP_CHUNK_PREVIOUS_IN_USE) |     \
                        (_Size) | HEAP_CHUNK_IN_USE),                          \
                                                                               \
     (((PHEAP_CHUNK)(((PCHAR)(_Chunk)) + (_Size)))->Header |=                  \
                                                HEAP_CHUNK_PREVIOUS_IN_USE),   \
                                                                               \
     HEAP_MARK_IN_USE_FOOTER(_Heap, _Chunk, _Size))

//
// This macro marks the footer of an in use chunk to be the exclusive or of
// the magic and the heap itself and the magic value.
//

#define HEAP_MARK_IN_USE_FOOTER(_Heap, _Chunk, _Size)               \
    (((PHEAP_CHUNK)((PCHAR)(_Chunk) + (_Size)))->PreviousFooter =   \
        ((UINTN)(_Heap) ^ (_Heap)->AllocationTag))

//
// This macro decodes the footer back into a pointer to the heap.
//

#define HEAP_DECODE_FOOTER_MAGIC(_Heap, _Chunk)                     \
    ((PMEMORY_HEAP)(((PHEAP_CHUNK)((PCHAR)(_Chunk) +                \
                     (HEAP_CHUNK_SIZE(_Chunk))))->PreviousFooter ^  \
                    (_Heap)->AllocationTag))

//
// This macro calls the corruption routine, if present.
//

#define HEAP_HANDLE_CORRUPTION(_Heap, _Code, _Parameter)                \
    if ((_Heap)->CorruptionFunction != NULL) {                          \
        (_Heap)->CorruptionFunction((_Heap), (_Code), (_Parameter));    \
    }

//
// These macros get and set the footer values.
//

#define HEAP_GET_FOOTER(_Chunk, _Size) \
    (((PHEAP_CHUNK)((PCHAR)(_Chunk) + (_Size)))->PreviousFooter)

#define HEAP_SET_FOOTER(_Chunk, _Size) \
    (((PHEAP_CHUNK)((PCHAR)(_Chunk) + (_Size)))->PreviousFooter = (_Size))

//
// This macro set the current in use and previous in use flags in the chunk.
//

#define HEAP_SET_CURRENT_PREVIOUS_IN_USE(_Heap, _Chunk, _Size)                 \
    ((_Chunk)->Header = ((_Size) | HEAP_CHUNK_PREVIOUS_IN_USE |                \
                         HEAP_CHUNK_IN_USE),                                   \
    (((PHEAP_CHUNK)(((PCHAR)(_Chunk)) + (_Size)))->Header |=                   \
                                                 HEAP_CHUNK_PREVIOUS_IN_USE,   \
                                                                               \
    HEAP_MARK_IN_USE_FOOTER((_Heap), (_Chunk), (_Size))))

//
// This macro sets the size, current in use, and previous in use flags in the
// chunk.
//

#define HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(_Heap, _Chunk, _Size) \
    ((_Chunk)->Header = ((_Size) | HEAP_CHUNK_PREVIOUS_IN_USE |                \
                         HEAP_CHUNK_IN_USE),                                   \
    HEAP_MARK_IN_USE_FOOTER((_Heap), (_Chunk), (_Size)))

//
// This macro sets the size, previous in use flag, and footer of the given
// free chunk.
//

#define HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(_Chunk, _Size)         \
    ((_Chunk)->Header = ((_Size) | HEAP_CHUNK_PREVIOUS_IN_USE),     \
    HEAP_SET_FOOTER((_Chunk), (_Size)))

//
// This macro sets the size, previous in use bit, footer, and clears the next
// previous in use bit.
//

#define HEAP_SET_FREE_PREVIOUS_IN_USE(_Chunk, _Size, _Next)     \
    (HEAP_CHUNK_CLEAR_PREVIOUS_IN_USE(_Next),                   \
     HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(_Chunk, _Size))

//
// This macro inserts either a small chunk or a large chunk. Either way, it
// gets inserted.
//

#define HEAP_INSERT_CHUNK(_Heap, _Chunk, _Size)                 \
    if (HEAP_IS_SMALL(_Size)) {                                 \
        RtlpHeapInsertSmallChunk((_Heap), (_Chunk), (_Size));   \
                                                                \
    } else {                                                    \
        RtlpHeapInsertLargeChunk((_Heap), (PHEAP_TREE_CHUNK)(_Chunk), (_Size));\
    }

//
// This macro unlinks either a small or large chunk, depending on the size.
//

#define HEAP_UNLINK_CHUNK(_Heap, _Chunk, _Size)                                \
    if (HEAP_IS_SMALL(_Size)) {                                                \
        RtlpHeapUnlinkSmallChunk((_Heap), (_Chunk), (_Size));                  \
                                                                               \
    } else {                                                                   \
        RtlpHeapUnlinkLargeChunk((_Heap), (PHEAP_TREE_CHUNK)(_Chunk));         \
    }

//
// This macro unlinks the first chunk from a small bin.
//

#define HEAP_UNLINK_FIRST_SMALL_CHUNK(_Heap, _Base, _Chunk, _Index, _First)    \
    {                                                                          \
                                                                               \
        (_First) = (_Chunk)->Next;                                             \
                                                                               \
        ASSERT((_Chunk) != (_Base));                                           \
        ASSERT((_Chunk) != (_First));                                          \
        ASSERT(HEAP_CHUNK_SIZE(_Chunk) == HEAP_SMALL_INDEX_TO_SIZE(_Index));   \
                                                                               \
        if ((_Base) == (_First)) {                                             \
            HEAP_CLEAR_SMALL_MAP((_Heap), (_Index));                           \
                                                                               \
        } else if ((HEAP_OK_ADDRESS((_Heap), (_First))) &&                     \
                   ((_First)->Previous == (_Chunk))) {                         \
                                                                               \
            (_First)->Previous = (_Base);                                      \
            (_Base)->Next = (_First);                                          \
                                                                               \
        } else {                                                               \
            HEAP_HANDLE_CORRUPTION(_Heap,                                      \
                                   HeapCorruptionCorruptStructures,            \
                                   _First);                                    \
        }                                                                      \
    }

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of bits of the allocation size to shave off before
// indexing into a small bin.
//

#define HEAP_SMALL_BIN_SHIFT 3U

//
// Define how often (in terms of number of free calls) to try and release
// unused segments back to the system. This is a costly operation, so it isn't
// done too often.
//

#define HEAP_MAX_RELEASE_CHECK_RATE 4095U

//
// Define the alignment heap allocations are always returned to.
//

#define HEAP_ALIGNMENT (2U * sizeof(PVOID))

//
// Define the bitmask associated with the allocation alignment.
//

#define HEAP_CHUNK_ALIGN_MASK (HEAP_ALIGNMENT - ((UINTN)1))

//
// Define the maximum amount of unused top-most memory to keep before releasing
// in the heap free routine.
//

#define HEAP_DEFAULT_TRIM_THRESHOLD (1024U * 1024U * 2U)

//
// Define the default direct allocation threshold.
//

#define HEAP_DEFAULT_DIRECT_ALLOCATION_THRESHOLD (1024U * 256U)

//
// Define to zero to disable segment traversal.
//

#define HEAP_SEGMENT_TRAVERSAL 0

//
// Define the overhead of a heap allocation.
//

#define HEAP_CHUNK_OVERHEAD FIELD_OFFSET(HEAP_CHUNK, Next)

//
// Define the amount of padding needed at the end of a segment, including space
// that may be needed to place segment records and fenceposts when new
// non-contiguous segments are added.
//

#define HEAP_TOP_FOOTER_SIZE                                        \
    (HEAP_ALIGNMENT_OFFSET(HEAP_CHUNK_TO_MEMORY(NULL)) +            \
     HEAP_PAD_REQUEST(sizeof(HEAP_SEGMENT)) + HEAP_MIN_CHUNK_SIZE)

//
// Define the amount of extra space needed for overhead when allocating a new
// segment.
//

#define HEAP_EXPANSION_PADDING (HEAP_TOP_FOOTER_SIZE + HEAP_ALIGNMENT)

//
// Define the number of bits to shift to get to a tree bin index.
//

#define HEAP_TREE_BIN_SHIFT 8U

//
// Define the minimum size for a tree-based allocation.
//

#define HEAP_MIN_LARGE_SIZE (((UINTN)1) << HEAP_TREE_BIN_SHIFT)

//
// Define the maximum small allocation size, including overhead.
//

#define HEAP_MAX_SMALL_SIZE (HEAP_MIN_LARGE_SIZE - ((UINTN)1))

//
// Define the maximum small request size.
//

#define HEAP_MAX_SMALL_REQUEST \
    (HEAP_MAX_SMALL_SIZE - HEAP_CHUNK_ALIGN_MASK - HEAP_CHUNK_OVERHEAD)

//
// Define the boundaries on request sizes.
//

#define HEAP_MAX_REQUEST ((-HEAP_MIN_CHUNK_SIZE) << 2)
#define HEAP_MIN_REQUEST (HEAP_MIN_CHUNK_SIZE - HEAP_CHUNK_OVERHEAD - 1U)
#define HEAP_MIN_CHUNK_SIZE \
    ((sizeof(HEAP_CHUNK) + HEAP_CHUNK_ALIGN_MASK) & ~HEAP_CHUNK_ALIGN_MASK)

//
// Define heap chunk flags.
//

#define HEAP_CHUNK_PREVIOUS_IN_USE 0x00000001
#define HEAP_CHUNK_IN_USE 0x00000002
#define HEAP_CHUNK_IN_USE_MASK \
    (HEAP_CHUNK_PREVIOUS_IN_USE | HEAP_CHUNK_IN_USE)

#define HEAP_CHUNK_FLAGS 0x00000007

//
// Define heap segment flags.
//

//
// This flag is set if the heap did not allocate this segment (and it therefore
// should not be freed).
//

#define HEAP_SEGMENT_FLAG_EXTERNAL 0x00000001

//
// Define the header value for fenceposts.
//

#define HEAP_FENCEPOST_HEADER (HEAP_CHUNK_IN_USE_MASK | sizeof(UINTN))

//
// Define mmapped segment overhead.
//

#define HEAP_MMAP_CHUNK_OVERHEAD (2U * sizeof(UINTN))
#define HEAP_MMAP_FOOTER_PAD (5U * sizeof(UINTN))

//
// Define magic constants.
//

#define HEAP_MAGIC 0x6C6F6F50 // 'looP'
#define HEAP_FREE_MAGIC 0x65657246 // 'eerF'

//
// Define the tag used for allocations relating to the statistics structures.
//

#define MEMORY_HEAP_STATISTICS_TAG 0x74536D4D // 'tSmM'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the bookkeeping for an allocated or freeheap chunk.
    This view is a bit misleading, as it is actually a view into two overlapping
    ranges. The size of a chunk is stored at the beginning of the chunk, and
    the first field is only valid if the previous chunk is not allocated.

Members:

    PreviousFooter - Stores the previous chunk's footer value, if there is a
        previous chunk.

    Header - Stores the size and flags of this chunk.

    Tag - Stores the heap tag used to identify the allocation.

    Next - Stores a pointer to the next chunk. This is only valid if the chunk
        is free.

    Previous - Stores a pointer to the previous chunk. This is only valid if
        the chunk is free.

--*/

struct _HEAP_CHUNK {
    UINTN PreviousFooter;
    UINTN Header;
    UINTN Tag;
    PHEAP_CHUNK Next;
    PHEAP_CHUNK Previous;
};

/*++

Structure Description:

    This structure defines the bookkeeping for a free heap tree chunk. When
    chunks are not in use, they are treated as nodes of either lists or trees.
    Small chunks are stored in circular doubly-linked lists, larger chunks are
    stored in tries based on chunk sizes. Each element in the tree is a unique
    chunk size. Chunks of the same size are arranged in a circularly linked
    list, with only the oldest chunk actually in the tree. Tree members are
    distinguished by a non-null parent pointer. The first four fields must be
    compatible with the heap chunk structure.

Members:

    PreviousFooter - Stores the previous chunk's footer value, as in the heap
        chunk structure.

    Header - Stores the size and flags of this chunk, as in the heap chunk
        structure.

    Tag - Stores the tag for this allocation, as in the heap chunk structure.

    Next - Stores a pointer to the next chunk, as in the heap chunk structure.

    Previous - Stores a pointer to the previous chunk, as in the heap chunk
        structure.

    Child - Stores an array of pointers to the left and right children in the
        tree.

    Parent - Stores a pointer to the parent node. This member is non-NULL if
        the node is actually a member in the tree.

    Index - Stores the bin index of this node.

--*/

struct _HEAP_TREE_CHUNK {
    UINTN PreviousFooter;
    UINTN Header;
    ULONG Tag;
    PHEAP_TREE_CHUNK Next;
    PHEAP_TREE_CHUNK Previous;
    PHEAP_TREE_CHUNK Child[2];
    PHEAP_TREE_CHUNK Parent;
    HEAP_BINDEX Index;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
RtlpHeapExpandAndAllocate (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

PVOID
RtlpHeapAllocateDirect (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

PVOID
RtlpHeapPrependAllocate (
    PMEMORY_HEAP Heap,
    PCHAR NewBase,
    PCHAR OldBase,
    UINTN Size,
    UINTN Tag
    );

VOID
RtlpHeapAddSegment (
    PMEMORY_HEAP Heap,
    PCHAR Base,
    UINTN Size
    );

PHEAP_CHUNK
RtlpHeapTryToReallocateChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size,
    BOOL CanMove
    );

VOID
RtlpHeapDisposeOfChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN ChunkSize
    );

BOOL
RtlpHeapTrim (
    PMEMORY_HEAP Heap,
    UINTN Padding
    );

UINTN
RtlpHeapReleaseUnusedSegments (
    PMEMORY_HEAP Heap
    );

PVOID
RtlpHeapTreeAllocateLarge (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

PVOID
RtlpHeapTreeAllocateSmall (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

VOID
RtlpHeapReplaceDesignatedVictim (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size
    );

VOID
RtlpHeapInsertSmallChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size
    );

VOID
RtlpHeapInsertLargeChunk (
    PMEMORY_HEAP Heap,
    PHEAP_TREE_CHUNK Chunk,
    UINTN Size
    );

VOID
RtlpHeapUnlinkSmallChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size
    );

VOID
RtlpHeapUnlinkLargeChunk (
    PMEMORY_HEAP Heap,
    PHEAP_TREE_CHUNK Node
    );

HEAP_BINDEX
RtlpHeapComputeTreeIndex (
    UINTN Size
    );

VOID
RtlpHeapInitializeBins (
    PMEMORY_HEAP Heap
    );

VOID
RtlpHeapInitializeTop (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size
    );

UINTN
RtlpHeapTraverseAndCheck (
    PMEMORY_HEAP Heap
    );

VOID
RtlpHeapCheckSmallBin (
    PMEMORY_HEAP Heap,
    HEAP_BINDEX Index
    );

VOID
RtlpHeapCheckTreeBin (
    PMEMORY_HEAP Heap,
    HEAP_BINDEX Index
    );

VOID
RtlpHeapCheckTree (
    PMEMORY_HEAP Heap,
    PHEAP_TREE_CHUNK Tree
    );

VOID
RtlpHeapCheckTopChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    );

VOID
RtlpHeapCheckAllocatedChunk (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    );

VOID
RtlpHeapCheckInUseChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    );

VOID
RtlpHeapCheckChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    );

VOID
RtlpHeapCheckMmappedChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    );

VOID
RtlpHeapCheckFreeChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    );

BOOL
RtlpHeapFindInBins (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    );

PHEAP_SEGMENT
RtlpHeapSegmentHolding (
    PMEMORY_HEAP Heap,
    PCHAR Address
    );

BOOL
RtlpHeapHasSegmentLink (
    PMEMORY_HEAP Heap,
    PHEAP_SEGMENT Segment
    );

COMPARISON_RESULT
RtlpCompareHeapStatisticNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

VOID
RtlpCollectTagStatistics (
    PMEMORY_HEAP Heap,
    ULONG Tag,
    ULONG AllocationSize,
    BOOL Allocate
    );

VOID
RtlpPrintMemoryHeapTagStatistic (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
VOID
RtlHeapInitialize (
    PMEMORY_HEAP Heap,
    PHEAP_ALLOCATE AllocateFunction,
    PHEAP_FREE FreeFunction,
    PHEAP_CORRUPTION_ROUTINE CorruptionFunction,
    UINTN MinimumExpansionSize,
    UINTN ExpansionGranularity,
    UINTN AllocationTag,
    ULONG Flags
    )

/*++

Routine Description:

    This routine initializes a memory heap. It does not initialize emergency
    resources.

Arguments:

    Heap - Supplies the heap to initialize.

    AllocateFunction - Supplies a pointer to a function the heap calls when it
        wants to expand and needs more memory.

    FreeFunction - Supplies a pointer to a function the heap calls when it
        wants to free a previously allocated segment.

    CorruptionFunction - Supplies a pointer to a function to call if heap
        corruption is detected.

    MinimumExpansionSize - Supplies the minimum number of bytes to request
        when expanding the heap.

    ExpansionGranularity - Supplies the granularity of expansions, in bytes.
        This must be a power of two.

    AllocationTag - Supplies the magic number to put in each allocation. This
        is also the tag supplied when the allocation function above is called.

    Flags - Supplies a bitfield of flags governing the heap's behavior. See
        MEMORY_HEAP_FLAG_* definitions.

Return Value:

    None.

--*/

{

    if (ExpansionGranularity == 0) {
        ExpansionGranularity = 1;
    }

    RtlZeroMemory(Heap, sizeof(MEMORY_HEAP));
    Heap->Magic = HEAP_MAGIC;
    Heap->Flags = Flags;
    Heap->AllocateFunction = AllocateFunction;
    Heap->FreeFunction = FreeFunction;
    Heap->CorruptionFunction = CorruptionFunction;
    Heap->MinimumExpansionSize = MinimumExpansionSize;
    Heap->ExpansionGranularity = ExpansionGranularity;
    Heap->AllocationTag = AllocationTag;
    RtlRedBlackTreeInitialize(&(Heap->TagStatistics.Tree),
                              0,
                              RtlpCompareHeapStatisticNodes);

    //
    // Pre-insert the statistics tag entry to avoid infinite recursion.
    //

    Heap->TagStatistics.StatisticEntry.Tag = MEMORY_HEAP_STATISTICS_TAG;
    RtlRedBlackTreeInsert(&(Heap->TagStatistics.Tree),
                          &(Heap->TagStatistics.StatisticEntry.Node));

    ASSERT(Heap->TagStatistics.TagCount == 0);

    Heap->TagStatistics.TagCount = 1;

    //
    // Initialize allocator state.
    //

    Heap->ReleaseChecks = HEAP_MAX_RELEASE_CHECK_RATE;
    RtlpHeapInitializeBins(Heap);
    Heap->TrimCheck = HEAP_DEFAULT_TRIM_THRESHOLD;
    Heap->DirectAllocationThreshold = HEAP_DEFAULT_DIRECT_ALLOCATION_THRESHOLD;
    return;
}

RTL_API
VOID
RtlHeapDestroy (
    PMEMORY_HEAP Heap
    )

/*++

Routine Description:

    This routine destroys a memory heap, releasing all resources it was
    managing. The structure itself is owned by the caller, so that isn't freed.

Arguments:

    Heap - Supplies the heap to destroy.

Return Value:

    None. The heap and all its allocations are no longer usable.

--*/

{

    PVOID Base;
    HEAP_CORRUPTION_CODE CorruptionCode;
    PHEAP_SEGMENT NextSegment;
    PHEAP_SEGMENT Segment;
    UINTN Size;

    if (Heap->Magic != HEAP_MAGIC) {
        CorruptionCode = HeapCorruptionCorruptStructures;
        if (Heap->Magic == 0) {
            CorruptionCode = HeapCorruptionDoubleDestroy;
        }

        HEAP_HANDLE_CORRUPTION(Heap, CorruptionCode, NULL);
        return;
    }

    Segment = &(Heap->Segment);
    while (Segment != NULL) {
        Base = Segment->Base;
        Size = Segment->Size;
        NextSegment = Segment->Next;
        if ((!HEAP_IS_EXTERNAL_SEGMENT(Segment)) &&
            (Heap->FreeFunction != NULL)) {

            Heap->FreeFunction(Heap, Base, Size);
        }

        Segment = NextSegment;
    }

    Heap->Magic = 0;
    return;
}

RTL_API
PVOID
RtlHeapAllocate (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine allocates memory from a given heap.

Arguments:

    Heap - Supplies the heap to allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identification tag to mark this allocation with.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    PHEAP_CHUNK Base;
    PHEAP_CHUNK Chunk;
    PHEAP_CHUNK First;
    HEAP_BINDEX Index;
    HEAP_BINMAP LeastBit;
    HEAP_BINMAP LeftBits;
    PVOID Memory;
    PHEAP_CHUNK Remainder;
    UINTN RemainderSize;
    HEAP_BINMAP SmallBits;

    if (Heap->Magic != HEAP_MAGIC) {
        HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionCorruptStructures, NULL);
        return NULL;
    }

    if ((Tag == 0) || (Tag == -1)) {

        ASSERT(FALSE);

        return NULL;
    }

    Heap->Statistics.TotalAllocationCalls += 1;
    Memory = NULL;
    if (Size <= HEAP_MAX_SMALL_REQUEST) {
        Size = HEAP_REQUEST_TO_SIZE(Size);
        Index = HEAP_SMALL_INDEX(Size);
        SmallBits = Heap->SmallMap >> Index;

        //
        // Handle a remainderless fit into a small bin.
        //

        if ((SmallBits & 0x3U) != 0) {

            //
            // Use the next bin if the given index is empty.
            //

            Index += ~SmallBits & 0x1;
            Base = HEAP_SMALL_BIN_AT(Heap, Index);
            Chunk = Base->Next;

            ASSERT(HEAP_CHUNK_SIZE(Chunk) == HEAP_SMALL_INDEX_TO_SIZE(Index));

            HEAP_UNLINK_FIRST_SMALL_CHUNK(Heap, Base, Chunk, Index, First);
            HEAP_SET_CURRENT_PREVIOUS_IN_USE(Heap,
                                             Chunk,
                                             HEAP_SMALL_INDEX_TO_SIZE(Index));

            Chunk->Tag = Tag;
            Memory = HEAP_CHUNK_TO_MEMORY(Chunk);
            Heap->Statistics.FreeListSize -= HEAP_SMALL_INDEX_TO_SIZE(Index);
            RtlpHeapCheckAllocatedChunk(Heap, Memory, Size);
            goto HeapAllocateEnd;

        } else if (Size > Heap->DesignatedVictimSize) {

            //
            // If there are more small bins, use the chunk from the next
            // non-empty small bin.
            //

            if (SmallBits != 0) {
                LeftBits = (SmallBits << Index) &
                           HEAP_LEFT_BITS(HEAP_INDEX_TO_BIT(Index));

                LeastBit = HEAP_LEAST_BIT(LeftBits);
                Index = HEAP_COMPUTE_BIT_INDEX(LeastBit);
                Base = HEAP_SMALL_BIN_AT(Heap, Index);
                Chunk = Base->Next;

                ASSERT(HEAP_CHUNK_SIZE(Chunk) ==
                       HEAP_SMALL_INDEX_TO_SIZE(Index));

                HEAP_UNLINK_FIRST_SMALL_CHUNK(Heap, Base, Chunk, Index, First);
                RemainderSize = HEAP_SMALL_INDEX_TO_SIZE(Index) - Size;
                if (RemainderSize < HEAP_MIN_CHUNK_SIZE) {
                    Size = HEAP_SMALL_INDEX_TO_SIZE(Index);
                    HEAP_SET_CURRENT_PREVIOUS_IN_USE(Heap,
                                                     Chunk,
                                                     Size);

                } else {
                    HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(Heap, Chunk, Size);
                    Remainder = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
                    HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Remainder,
                                                         RemainderSize);

                    RtlpHeapReplaceDesignatedVictim(Heap,
                                                    Remainder,
                                                    RemainderSize);
                }

                Chunk->Tag = Tag;
                Memory = HEAP_CHUNK_TO_MEMORY(Chunk);
                Heap->Statistics.FreeListSize -= Size;
                RtlpHeapCheckAllocatedChunk(Heap, Memory, Size);
                goto HeapAllocateEnd;

            } else if (Heap->TreeMap != 0) {
                Memory = RtlpHeapTreeAllocateSmall(Heap, Size, Tag);
                if (Memory != NULL) {
                    RtlpHeapCheckAllocatedChunk(Heap, Memory, Size);
                    goto HeapAllocateEnd;
                }
            }
        }

    //
    // If the allocation is impossible, force a failure later.
    //

    } else if (Size >= HEAP_MAX_REQUEST) {
        Size = (UINTN)-1;

    } else {
        Size = HEAP_PAD_REQUEST(Size);
        if (Heap->TreeMap != 0) {
            Memory = RtlpHeapTreeAllocateLarge(Heap, Size, Tag);
            if (Memory != NULL) {
                RtlpHeapCheckAllocatedChunk(Heap, Memory, Size);
                goto HeapAllocateEnd;
            }
        }
    }

    //
    // See if the designated victim can satisfy this allocation.
    //

    if (Size <= Heap->DesignatedVictimSize) {
        RemainderSize = Heap->DesignatedVictimSize - Size;
        Chunk = Heap->DesignatedVictim;

        //
        // Split the designated victim if it's still got a remainder.
        //

        if (RemainderSize >= HEAP_MIN_CHUNK_SIZE) {
            Remainder = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
            Heap->DesignatedVictim = Remainder;
            Heap->DesignatedVictimSize = RemainderSize;
            HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Remainder, RemainderSize);
            HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(Heap, Chunk, Size);

        //
        // Use the designated victim in its entirety.
        //

        } else {
            Size = Heap->DesignatedVictimSize;
            Heap->DesignatedVictimSize = 0;
            Heap->DesignatedVictim = NULL;
            HEAP_SET_CURRENT_PREVIOUS_IN_USE(Heap, Chunk, Size);
        }

        Chunk->Tag = Tag;
        Heap->Statistics.FreeListSize -= Size;
        Memory = HEAP_CHUNK_TO_MEMORY(Chunk);
        RtlpHeapCheckAllocatedChunk(Heap, Memory, Size);
        goto HeapAllocateEnd;

    //
    // See if the top can satisfy this allocation.
    //

    } else if (Size < Heap->TopSize) {
        Heap->TopSize -= Size;
        RemainderSize = Heap->TopSize;
        Chunk = Heap->Top;
        Remainder = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
        Heap->Top = Remainder;
        Remainder->Header = RemainderSize | HEAP_CHUNK_PREVIOUS_IN_USE;
        HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(Heap, Chunk, Size);
        Chunk->Tag = Tag;
        Heap->Statistics.FreeListSize -= Size;
        Memory = HEAP_CHUNK_TO_MEMORY(Chunk);
        RtlpHeapCheckTopChunk(Heap, Heap->Top);
        RtlpHeapCheckAllocatedChunk(Heap, Memory, Size);
        goto HeapAllocateEnd;
    }

    Memory = RtlpHeapExpandAndAllocate(Heap, Size, Tag);

HeapAllocateEnd:
    if (Memory != NULL) {
        Heap->Statistics.Allocations += 1;
        if ((Heap->Flags & MEMORY_HEAP_FLAG_COLLECT_TAG_STATISTICS) != 0) {
            RtlpCollectTagStatistics(
                                 Heap,
                                 Tag,
                                 HEAP_CHUNK_SIZE(HEAP_MEMORY_TO_CHUNK(Memory)),
                                 TRUE);
        }

    } else {
        Heap->Statistics.FailedAllocations += 1;
    }

    return Memory;
}

RTL_API
PVOID
RtlHeapReallocate (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN NewSize,
    UINTN AllocationTag
    )

/*++

Routine Description:

    This routine resizes the given allocation, potentially creating a new
    buffer and copying the old contents in.

Arguments:

    Heap - Supplies a pointer to the heap to work with.

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

    UINTN AdjustedSize;
    UINTN CopySize;
    PMEMORY_HEAP FooterHeap;
    PHEAP_CHUNK NewChunk;
    PVOID NewMemory;
    PHEAP_CHUNK OldChunk;

    NewMemory = NULL;
    if (Memory == NULL) {
        return RtlHeapAllocate(Heap, NewSize, AllocationTag);

    } else if (NewSize == 0) {
        RtlHeapFree(Heap, Memory);
        return NULL;

    } else if (NewSize >= HEAP_MAX_REQUEST) {
        goto HeapReallocateEnd;
    }

    AdjustedSize = HEAP_REQUEST_TO_SIZE(NewSize);
    OldChunk = HEAP_MEMORY_TO_CHUNK(Memory);
    FooterHeap = HEAP_DECODE_FOOTER_MAGIC(Heap, OldChunk);
    if (FooterHeap != Heap) {
        HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionBufferOverrun, OldChunk);
        goto HeapReallocateEnd;
    }

    NewChunk = RtlpHeapTryToReallocateChunk(Heap, OldChunk, AdjustedSize, TRUE);
    if (NewChunk != NULL) {
        RtlpHeapCheckInUseChunk(Heap, NewChunk);
        NewMemory = HEAP_CHUNK_TO_MEMORY(NewChunk);

    } else {
        NewMemory = RtlHeapAllocate(Heap, NewSize, AllocationTag);
        if (NewMemory == NULL) {
            return NULL;
        }

        CopySize = HEAP_CHUNK_SIZE(OldChunk) - HEAP_OVERHEAD_FOR(OldChunk);
        if (NewSize < CopySize) {
            CopySize = NewSize;
        }

        RtlCopyMemory(NewMemory, Memory, CopySize);
        RtlHeapFree(Heap, Memory);
    }

HeapReallocateEnd:
    if (NewMemory == NULL) {
        Heap->Statistics.FailedAllocations += 1;
    }

    return NewMemory;
}

RTL_API
KSTATUS
RtlHeapAlignedAllocate (
    PMEMORY_HEAP Heap,
    PVOID *Memory,
    UINTN Alignment,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine allocates aligned memory from a given heap.

Arguments:

    Heap - Supplies the heap to allocate from.

    Memory - Supplies a pointer that receives the pointer to the aligned
        allocation on success.

    Alignment - Supplies the requested alignment for the allocation, in bytes.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identification tag to mark this allocation with.

Return Value:

    Status code.

--*/

{

    PHEAP_CHUNK AlignedChunk;
    PVOID AlignedMemory;
    UINTN AlignedOffset;
    UINTN AlignedSize;
    UINTN AllocationSize;
    PHEAP_CHUNK NewChunk;
    UINTN NewChunkSize;
    PVOID NewMemory;
    PHEAP_CHUNK RemainderChunk;
    UINTN RemainderChunkSize;
    UINTN Shift;
    KSTATUS Status;

    NewMemory = NULL;

    //
    // Make sure the alignment is big enough and a power of 2.
    //

    if (Alignment < HEAP_MIN_CHUNK_SIZE) {
        Alignment = HEAP_MIN_CHUNK_SIZE;
    }

    if (!POWER_OF_2(Alignment)) {
        Shift = RtlCountLeadingZeros(Alignment);
        if (Shift == 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto HeapAlignedAllocateEnd;
        }

        Shift = (sizeof(UINTN) * BITS_PER_BYTE) - Shift;
        Alignment = 1L << Shift;

        ASSERT(POWER_OF_2(Alignment));
    }

    //
    // Validate that the request can be aligned.
    //

    if (Size >= (HEAP_MAX_REQUEST - Alignment)) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto HeapAlignedAllocateEnd;
    }

    //
    // Pad the allocation request so that it can be aligned up in case it does
    // not come back aligned.
    //

    Size = HEAP_REQUEST_TO_SIZE(Size);
    AllocationSize = Size +
                     Alignment +
                     HEAP_MIN_CHUNK_SIZE -
                     HEAP_CHUNK_OVERHEAD;

    NewMemory = RtlHeapAllocate(Heap, AllocationSize, Tag);
    if (NewMemory == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto HeapAlignedAllocateEnd;
    }

    NewChunk = HEAP_MEMORY_TO_CHUNK(NewMemory);
    AllocationSize = HEAP_CHUNK_SIZE(NewChunk);

    //
    // If the base of the allocation is not aligned, align it up to the
    // requested alignment. There should be enough room to release the
    // beginning of the allocation back to the heap.
    //

    if (!IS_POINTER_ALIGNED(NewMemory, Alignment)) {
        AlignedMemory = ALIGN_POINTER_UP(NewMemory, Alignment);
        AlignedChunk = HEAP_MEMORY_TO_CHUNK(AlignedMemory);
        if (((PVOID)AlignedChunk - (PVOID)NewChunk) < HEAP_MIN_CHUNK_SIZE) {
            AlignedChunk = HEAP_CHUNK_PLUS_OFFSET(AlignedChunk, Alignment);
        }

        AlignedOffset = (UINTN)((PVOID)AlignedChunk - (PVOID)NewChunk);
        AlignedSize = HEAP_CHUNK_SIZE(NewChunk) - AlignedOffset;
        if (HEAP_CHUNK_IS_MMAPPED(NewChunk)) {
            AlignedChunk->PreviousFooter = NewChunk->PreviousFooter +
                                           AlignedOffset;

            AlignedChunk->Header = AlignedSize;

        } else {
            HEAP_CHUNK_SET_IN_USE(Heap, AlignedChunk, AlignedSize);
            HEAP_CHUNK_SET_IN_USE(Heap, NewChunk, AlignedOffset);
            NewChunk->Tag = HEAP_FREE_MAGIC;
            RtlpHeapDisposeOfChunk(Heap, NewChunk, AlignedOffset);
            Heap->Statistics.FreeListSize += AlignedOffset;
        }

        AlignedChunk->Tag = Tag;
        NewChunk = AlignedChunk;
    }

    //
    // If the trailing space is big enough to fit a minimum heap chunk, give
    // it back. The trailing space never had the chunk tag set, so it does not
    // need to be modified.
    //

    if (!HEAP_CHUNK_IS_MMAPPED(NewChunk)) {
        NewChunkSize = HEAP_CHUNK_SIZE(NewChunk);
        if (NewChunkSize > (Size + HEAP_MIN_CHUNK_SIZE)) {
            RemainderChunkSize = NewChunkSize - Size;
            RemainderChunk = HEAP_CHUNK_PLUS_OFFSET(NewChunk, Size);
            HEAP_CHUNK_SET_IN_USE(Heap, NewChunk, Size);
            HEAP_CHUNK_SET_IN_USE(Heap, RemainderChunk, RemainderChunkSize);
            RtlpHeapDisposeOfChunk(Heap, RemainderChunk, RemainderChunkSize);
            Heap->Statistics.FreeListSize += RemainderChunkSize;
        }
    }

    NewMemory = HEAP_CHUNK_TO_MEMORY(NewChunk);

    ASSERT(HEAP_CHUNK_SIZE(NewChunk) >= Size);
    ASSERT(IS_POINTER_ALIGNED(NewMemory, Alignment));

    RtlpHeapCheckInUseChunk(Heap, NewChunk);

    //
    // If the final chunk is not the same size as the allocated chunk, adjust
    // the statistics.
    //

    if (((Heap->Flags & MEMORY_HEAP_FLAG_COLLECT_TAG_STATISTICS) != 0) &&
        (HEAP_CHUNK_SIZE(NewChunk) != AllocationSize)) {

        RtlpCollectTagStatistics(Heap, Tag, AllocationSize, FALSE);
        RtlpCollectTagStatistics(Heap, Tag, HEAP_CHUNK_SIZE(NewChunk), TRUE);
    }

    Status = STATUS_SUCCESS;

HeapAlignedAllocateEnd:
    if (!KSUCCESS(Status)) {
        if (NewMemory != NULL) {
            RtlHeapFree(Heap, NewMemory);
            NewMemory = NULL;
        }
    }

    *Memory = NewMemory;
    return Status;
}

RTL_API
VOID
RtlHeapFree (
    PMEMORY_HEAP Heap,
    PVOID Memory
    )

/*++

Routine Description:

    This routine frees memory, making it available for other users in the heap.
    This routine may potentially contract the heap periodically.

Arguments:

    Heap - Supplies the heap to free the memory back to.

    Memory - Supplies the allocation created by the heap allocation routine.

Return Value:

    None.

--*/

{

    PHEAP_CHUNK Chunk;
    UINTN ChunkSize;
    UINTN DesignatedVictimSize;
    PMEMORY_HEAP FooterMagic;
    PHEAP_CHUNK Next;
    UINTN NextSize;
    PHEAP_CHUNK Previous;
    UINTN PreviousSize;
    BOOL Success;
    UINTN TopSize;

    if (Memory == NULL) {
        return;
    }

    Heap->Statistics.TotalFreeCalls += 1;
    Chunk = HEAP_MEMORY_TO_CHUNK(Memory);
    FooterMagic = HEAP_DECODE_FOOTER_MAGIC(Heap, Chunk);
    if (FooterMagic != Heap) {
        HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionBufferOverrun, Chunk);
        goto HeapFreeEnd;
    }

    RtlpHeapCheckInUseChunk(Heap, Chunk);
    if ((!HEAP_CHUNK_IS_IN_USE(Chunk)) || (Chunk->Tag == HEAP_FREE_MAGIC)) {
        HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionDoubleFree, Chunk);
        goto HeapFreeEnd;
    }

    if (!HEAP_OK_ADDRESS(Heap, Chunk)) {
        HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionCorruptStructures, Chunk);
        goto HeapFreeEnd;
    }

    ChunkSize = HEAP_CHUNK_SIZE(Chunk);
    if ((Heap->Flags & MEMORY_HEAP_FLAG_COLLECT_TAG_STATISTICS) != 0) {
        RtlpCollectTagStatistics(Heap,
                                 Chunk->Tag,
                                 ChunkSize,
                                 FALSE);
    }

    Next = HEAP_CHUNK_PLUS_OFFSET(Chunk, ChunkSize);
    Chunk->Tag = HEAP_FREE_MAGIC;
    Heap->Statistics.FreeListSize += ChunkSize;

    //
    // Potentially consolidate backwards.
    //

    if (!HEAP_CHUNK_IS_PREVIOUS_IN_USE(Chunk)) {
        PreviousSize = Chunk->PreviousFooter;

        //
        // If the chunk was directly allocated, then directly free it. The
        // previous chunk is just a little stub at the beginning.
        //

        if (HEAP_CHUNK_IS_MMAPPED(Chunk)) {

            //
            // Whoops, the free list should not have been adjusted, as this
            // didn't come out of it. Put that back.
            //

            Heap->Statistics.FreeListSize -= ChunkSize;
            ChunkSize += PreviousSize + HEAP_MMAP_FOOTER_PAD;
            if (Heap->FreeFunction != NULL) {
                Heap->Statistics.TotalHeapSize -= ChunkSize;
                Heap->Statistics.DirectAllocationSize -= ChunkSize;
                Success = Heap->FreeFunction(Heap,
                                             (PVOID)Chunk - PreviousSize,
                                             ChunkSize);

                //
                // If the free failed, put things back.
                //

                if (Success == FALSE) {
                    Heap->Statistics.TotalHeapSize += ChunkSize;
                    Heap->Statistics.DirectAllocationSize += ChunkSize;
                }
            }

            goto HeapFreeEnd;

        } else {
            Previous = HEAP_CHUNK_MINUS_OFFSET(Chunk, PreviousSize);
            ChunkSize += PreviousSize;
            if (!HEAP_OK_ADDRESS(Heap, Previous)) {
                HEAP_HANDLE_CORRUPTION(Heap,
                                       HeapCorruptionCorruptStructures,
                                       Chunk);

                goto HeapFreeEnd;
            }

            Chunk = Previous;
            if (Chunk != Heap->DesignatedVictim) {
                HEAP_UNLINK_CHUNK(Heap, Chunk, PreviousSize);

            } else if ((Next->Header & HEAP_CHUNK_IN_USE_MASK) ==
                       HEAP_CHUNK_IN_USE_MASK) {

                Heap->DesignatedVictimSize = ChunkSize;
                HEAP_SET_FREE_PREVIOUS_IN_USE(Chunk, ChunkSize, Next);
                goto HeapFreeEnd;
            }
        }
    }

    if ((!HEAP_OK_NEXT(Chunk, Next)) ||
        (!HEAP_CHUNK_IS_PREVIOUS_IN_USE(Chunk))) {

        HEAP_HANDLE_CORRUPTION(Heap,
                               HeapCorruptionCorruptStructures,
                               Chunk);

        goto HeapFreeEnd;
    }

    //
    // Potentially consolidate forward.
    //

    if (!HEAP_CHUNK_IS_CURRENT_IN_USE(Next)) {
        if (Next == Heap->Top) {
            Heap->TopSize += ChunkSize;
            TopSize = Heap->TopSize;
            Heap->Top = Chunk;
            Chunk->Header = TopSize | HEAP_CHUNK_PREVIOUS_IN_USE;
            if (Chunk == Heap->DesignatedVictim) {
                Heap->DesignatedVictim = NULL;
                Heap->DesignatedVictimSize = 0;
            }

            if (HEAP_SHOULD_TRIM(Heap, TopSize)) {
                RtlpHeapTrim(Heap, 0);
            }

            goto HeapFreeEnd;

        } else if (Next == Heap->DesignatedVictim) {
            Heap->DesignatedVictimSize += ChunkSize;
            DesignatedVictimSize = Heap->DesignatedVictimSize;
            Heap->DesignatedVictim = Chunk;
            HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Chunk, DesignatedVictimSize);
            goto HeapFreeEnd;

        } else {
            NextSize = HEAP_CHUNK_SIZE(Next);
            ChunkSize += NextSize;
            HEAP_UNLINK_CHUNK(Heap, Next, NextSize);
            HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Chunk, ChunkSize);
            if (Chunk == Heap->DesignatedVictim) {
                Heap->DesignatedVictimSize = ChunkSize;
                goto HeapFreeEnd;
            }
        }

    } else {
        HEAP_SET_FREE_PREVIOUS_IN_USE(Chunk, ChunkSize, Next);
    }

    //
    // Stick the free chunk on a list or tree.
    //

    if (HEAP_IS_SMALL(ChunkSize)) {
        RtlpHeapInsertSmallChunk(Heap, Chunk, ChunkSize);
        RtlpHeapCheckFreeChunk(Heap, Chunk);

    } else {
        RtlpHeapInsertLargeChunk(Heap, (PHEAP_TREE_CHUNK)Chunk, ChunkSize);
        RtlpHeapCheckFreeChunk(Heap, Chunk);

        //
        // Periodically release segments that are entirely free.
        //

        Heap->ReleaseChecks -= 1;
        if (Heap->ReleaseChecks == 0) {
            RtlpHeapReleaseUnusedSegments(Heap);
        }
    }

HeapFreeEnd:
    Heap->Statistics.Allocations -= 1;
    return;
}

RTL_API
VOID
RtlValidateHeap (
    PMEMORY_HEAP Heap,
    PHEAP_CORRUPTION_ROUTINE CorruptionRoutine
    )

/*++

Routine Description:

    This routine validates a memory heap for consistency, ensuring that no
    corruption or other errors are present in the heap.

Arguments:

    Heap - Supplies a pointer to the heap to validate.

    CorruptionRoutine - Supplies an optional pointer to a routine to call if
        corruption is detected. If not supplied, the internal one supplied when
        the heap was initialized will be used.

Return Value:

    None.

--*/

{

    HEAP_BINDEX Index;
    PHEAP_CORRUPTION_ROUTINE OriginalRoutine;
    UINTN Total;

    OriginalRoutine = Heap->CorruptionFunction;
    Heap->CorruptionFunction = CorruptionRoutine;

    //
    // Check all the small bins and all the tree bins.
    //

    for (Index = 0; Index < HEAP_SMALL_BIN_COUNT; Index += 1) {
        RtlpHeapCheckSmallBin(Heap, Index);
    }

    for (Index = 0; Index < HEAP_TREE_BIN_COUNT; Index += 1) {
        RtlpHeapCheckTreeBin(Heap, Index);
    }

    //
    // Check the designated victim if it's valid.
    //

    if (Heap->DesignatedVictimSize != 0) {
        RtlpHeapCheckChunk(Heap, Heap->DesignatedVictim);

        ASSERT(Heap->DesignatedVictimSize ==
               HEAP_CHUNK_SIZE(Heap->DesignatedVictim));

        ASSERT(Heap->DesignatedVictimSize >= HEAP_MIN_CHUNK_SIZE);
        ASSERT(RtlpHeapFindInBins(Heap, Heap->DesignatedVictim) == FALSE);
    }

    //
    // Check the top if it's valid.
    //

    if (Heap->Top != NULL) {
        RtlpHeapCheckTopChunk(Heap, Heap->Top);

        ASSERT(Heap->TopSize > 0);
        ASSERT(RtlpHeapFindInBins(Heap, Heap->Top) == FALSE);
    }

    Total = RtlpHeapTraverseAndCheck(Heap);

    ASSERT(Total <= Heap->Statistics.TotalHeapSize);
    ASSERT(Heap->Statistics.TotalHeapSize <= Heap->Statistics.MaxHeapSize);

    Heap->CorruptionFunction = OriginalRoutine;
    return;
}

RTL_API
VOID
RtlHeapDebugPrintStatistics (
    PMEMORY_HEAP Heap
    )

/*++

Routine Description:

    This routine prints current heap statistics to the debugger.

Arguments:

    Heap - Supplies a pointer to the heap to print.

Return Value:

    None.

--*/

{

    ULONG FreePercentage;

    FreePercentage = Heap->Statistics.FreeListSize * 100 /
                     Heap->Statistics.TotalHeapSize;

    RtlDebugPrint("Heap 0x%x, Size %I64d, %d%% free, %I64d allocation calls, "
                  "%I64d free calls %I64d failed.\n",
                  Heap,
                  (ULONGLONG)Heap->Statistics.TotalHeapSize,
                  FreePercentage,
                  (ULONGLONG)Heap->Statistics.TotalAllocationCalls,
                  (ULONGLONG)Heap->Statistics.TotalFreeCalls,
                  (ULONGLONG)Heap->Statistics.FailedAllocations);

    RtlDebugPrint("     Largest                                    Active   "
                  "Max Active\n"
                  "Tag  Alloc    Active Bytes     Max Active Bytes Count    "
                  "Count      Lifetime Alloc\n"
                  "---------------------------------------------------------"
                  "---------------------------\n");

    RtlRedBlackTreeIterate(&(Heap->TagStatistics.Tree),
                           RtlpPrintMemoryHeapTagStatistic,
                           NULL);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
RtlpHeapExpandAndAllocate (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine expands the heap in an attempt to satisfy a given allocation
    request.

Arguments:

    Heap - Supplies the heap to expand and allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identification tag to mark this allocation with.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    UINTN AlignedSize;
    PHEAP_CHUNK Chunk;
    UINTN DoublePrevious;
    UINTN Footprint;
    PCHAR Memory;
    UINTN Minimum;
    PCHAR OldBase;
    PHEAP_CHUNK Replacement;
    UINTN ReplacementSize;
    PHEAP_SEGMENT Segment;

    if (Heap->AllocateFunction == NULL) {
        return NULL;
    }

    //
    // Directly allocate large chunks, but only if the heap is already
    // initialized.
    //

    if ((Size >= Heap->DirectAllocationThreshold) && (Heap->TopSize != 0)) {
        Memory = RtlpHeapAllocateDirect(Heap, Size, Tag);
        if (Memory != NULL) {
            return Memory;
        }
    }

    AlignedSize = Size + HEAP_EXPANSION_PADDING;
    if (AlignedSize < Heap->MinimumExpansionSize) {
        AlignedSize = Heap->MinimumExpansionSize;
    }

    //
    // Watch out for overflow.
    //

    if (AlignedSize < Size) {
        return NULL;
    }

    //
    // Attempt to use double the previous size to avoid lots of small
    // expansions. Don't go over the footprint limit that way though.
    //

    Minimum = ALIGN_RANGE_UP(AlignedSize, Heap->ExpansionGranularity);
    DoublePrevious = Heap->PreviousExpansionSize << 1;
    if ((AlignedSize < DoublePrevious) &&
        ((Heap->FootprintLimit == 0) ||
         (DoublePrevious < Heap->FootprintLimit))) {

        AlignedSize = DoublePrevious;
    }

    AlignedSize = ALIGN_RANGE_UP(AlignedSize, Heap->ExpansionGranularity);

    //
    // Avoid exceeding the footprint limit.
    //

    if (Heap->FootprintLimit != 0) {
        Footprint = Heap->Statistics.TotalHeapSize + AlignedSize;
        if ((Footprint < Heap->Statistics.TotalHeapSize) ||
            (Footprint > Heap->FootprintLimit)) {

            return NULL;
        }
    }

    //
    // Ask the system for more memory. If the doubling effort failed, divide by
    // 2 until something is found or the minimum fails as well.
    //

    Memory = NULL;
    while (TRUE) {

        ASSERT(AlignedSize >= Minimum);

        Memory = Heap->AllocateFunction(Heap, AlignedSize, Heap->AllocationTag);
        if (Memory != NULL) {
            break;
        }

        if (AlignedSize <= Minimum) {
            break;
        }

        AlignedSize >>= 1;
        AlignedSize = ALIGN_RANGE_UP(AlignedSize,
                                     Heap->ExpansionGranularity);

        if (AlignedSize < Minimum) {
            AlignedSize = Minimum;
        }
    }

    if (Memory != NULL) {
        Heap->Statistics.TotalHeapSize += AlignedSize;
        Heap->PreviousExpansionSize = AlignedSize;

        //
        // Trim the heap if the top gets to be 5/4 of the previous expansion
        // size (ie the expansion plus half the previous expansion).
        //

        Heap->TrimCheck = AlignedSize + (AlignedSize >> 2);

        //
        // Add the whole segment to the free list size, but know that the add
        // segment routine adjusts that down if segment and chunk structures
        // are carved out of it.
        //

        Heap->Statistics.FreeListSize += AlignedSize;
        if (Heap->Statistics.TotalHeapSize > Heap->Statistics.MaxHeapSize) {
            Heap->Statistics.MaxHeapSize = Heap->Statistics.TotalHeapSize;
        }

        //
        // If this is the first allocation ever, set up the top chunk.
        //

        if (!HEAP_IS_INITIALIZED(Heap)) {
            if ((Heap->LeastAddress == NULL) ||
                (Memory < Heap->LeastAddress)) {

                Heap->LeastAddress = Memory;
            }

            Heap->Segment.Base = Memory;
            Heap->Segment.Size = AlignedSize;
            Heap->Segment.Flags = 0;
            Heap->ReleaseChecks = HEAP_MAX_RELEASE_CHECK_RATE;
            RtlpHeapInitializeBins(Heap);
            RtlpHeapInitializeTop(Heap,
                                  (PHEAP_CHUNK)Memory,
                                  AlignedSize - HEAP_TOP_FOOTER_SIZE);

        //
        // This is not the first allocation. If the heap cannot do partial
        // frees, then just add the segment.
        //

        } else if ((Heap->Flags & MEMORY_HEAP_FLAG_NO_PARTIAL_FREES) != 0) {
            if (Memory < Heap->LeastAddress) {
                Heap->LeastAddress = Memory;
            }

            RtlpHeapAddSegment(Heap, Memory, AlignedSize);

        //
        // The heap is allowed to free only portions of this new memory. Try to
        // merge this new memory with an existing segment.
        //

        } else {
            Segment = &(Heap->Segment);
            while ((Segment != NULL) &&
                   (Memory != (Segment->Base + Segment->Size))) {

                if (HEAP_SEGMENT_TRAVERSAL != 0) {
                    Segment = Segment->Next;

                } else {
                    Segment = NULL;
                }
            }

            //
            // Append the segment if it's contiguous.
            //

            if ((Segment != NULL) &&
                (!HEAP_IS_EXTERNAL_SEGMENT(Segment)) &&
                (HEAP_SEGMENT_HOLDS(Segment, Heap->Top))) {

                Segment->Size += AlignedSize;
                RtlpHeapInitializeTop(Heap,
                                      Heap->Top,
                                      Heap->TopSize + AlignedSize);

            } else {
                if (Memory < Heap->LeastAddress) {
                    Heap->LeastAddress = Memory;
                }

                //
                // See if it can be merged onto the beginning of an existing
                // segment.
                //

                Segment = &(Heap->Segment);
                while ((Segment != NULL) &&
                       (Segment->Base != (Segment->Base + Segment->Size))) {

                    if (HEAP_SEGMENT_TRAVERSAL != 0) {
                        Segment = Segment->Next;

                    } else {
                        Segment = NULL;
                    }
                }

                if ((Segment != NULL) &&
                    (!HEAP_IS_EXTERNAL_SEGMENT(Segment))) {

                    OldBase = Segment->Base;
                    Segment->Base = Memory;
                    Segment->Size += AlignedSize;
                    Memory = RtlpHeapPrependAllocate(Heap,
                                                     Memory,
                                                     OldBase,
                                                     Size,
                                                     Tag);

                    return Memory;

                } else {
                    RtlpHeapAddSegment(Heap, Memory, AlignedSize);
                }
            }
        }

        //
        // Attempt to allocate from new or extended top space.
        //

        if (Size < Heap->TopSize) {
            Heap->TopSize -= Size;
            ReplacementSize = Heap->TopSize;
            Chunk = Heap->Top;
            Replacement = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
            Heap->Top = Replacement;
            Replacement->Header = ReplacementSize | HEAP_CHUNK_PREVIOUS_IN_USE;
            HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(Heap, Chunk, Size);
            Chunk->Tag = Tag;
            Heap->Statistics.FreeListSize -= Size;
            RtlpHeapCheckTopChunk(Heap, Heap->Top);
            RtlpHeapCheckAllocatedChunk(Heap,
                                        HEAP_CHUNK_TO_MEMORY(Chunk),
                                        Size);

            return HEAP_CHUNK_TO_MEMORY(Chunk);
        }
    }

    return NULL;
}

PVOID
RtlpHeapAllocateDirect (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine staisfies the allocation request by calling directly into
    the underlying allocate function.

Arguments:

    Heap - Supplies a pointer to the heap.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identification tag to mark this allocation with.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    UINTN AlignedSize;
    PCHAR Allocation;
    PHEAP_CHUNK Chunk;
    UINTN Footprint;
    UINTN Offset;
    UINTN PadSize;

    PadSize = Size + HEAP_MMAP_CHUNK_OVERHEAD + HEAP_MMAP_FOOTER_PAD +
              HEAP_CHUNK_ALIGN_MASK;

    AlignedSize = ALIGN_RANGE_UP(PadSize,
                                 Heap->ExpansionGranularity);

    if (AlignedSize < Heap->MinimumExpansionSize) {
        AlignedSize = Heap->MinimumExpansionSize;
    }

    //
    // Check against the footprint limit.
    //

    if (Heap->FootprintLimit != 0) {
        Footprint = Heap->Statistics.TotalHeapSize + AlignedSize;
        if ((Footprint < Heap->Statistics.TotalHeapSize) ||
            (Footprint > Heap->FootprintLimit)) {

            return NULL;
        }
    }

    if (AlignedSize > Size) {
        Allocation = Heap->AllocateFunction(Heap, AlignedSize, Tag);
        if (Allocation != NULL) {
            Offset = HEAP_ALIGNMENT_OFFSET(HEAP_CHUNK_TO_MEMORY(Allocation));
            PadSize = AlignedSize - Offset - HEAP_MMAP_FOOTER_PAD;
            Chunk = (PHEAP_CHUNK)(Allocation + Offset);
            Chunk->PreviousFooter = Offset;
            Chunk->Header = PadSize;
            HEAP_MARK_IN_USE_FOOTER(Heap, Chunk, PadSize);
            HEAP_CHUNK_PLUS_OFFSET(Chunk, PadSize)->Header =
                                                         HEAP_FENCEPOST_HEADER;

            HEAP_CHUNK_PLUS_OFFSET(Chunk, PadSize + sizeof(UINTN))->Header = 0;
            if ((Heap->LeastAddress == NULL) ||
                (Allocation < Heap->LeastAddress)) {

                Heap->LeastAddress = Allocation;
            }

            Chunk->Tag = Tag;

            //
            // Adjust the statistics. In this case, the free list just grew by
            // the allocation, but was then immediately allocated, resulting in
            // no net change.
            //

            Heap->Statistics.TotalHeapSize += AlignedSize;
            if (Heap->Statistics.TotalHeapSize > Heap->Statistics.MaxHeapSize) {
                Heap->Statistics.MaxHeapSize = Heap->Statistics.TotalHeapSize;
            }

            Heap->Statistics.DirectAllocationSize += AlignedSize;

            ASSERT(HEAP_IS_ALIGNED(HEAP_CHUNK_TO_MEMORY(Chunk)));

            RtlpHeapCheckMmappedChunk(Heap, Chunk);
            return HEAP_CHUNK_TO_MEMORY(Chunk);
        }
    }

    return NULL;
}

PVOID
RtlpHeapPrependAllocate (
    PMEMORY_HEAP Heap,
    PCHAR NewBase,
    PCHAR OldBase,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine allocates a chunk prepends the remainder with the chunk in
    the successor.

Arguments:

    Heap - Supplies a pointer to the heap.

    NewBase - Supplies a pointer to the new lower base.

    OldBase - Supplies a pointer to the original chunk base address.

    Size - Supplies the size of the requested allocation.

    Tag - Supplies the tag associated with the allocation.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on allocation failure.

--*/

{

    PHEAP_CHUNK Chunk;
    UINTN ChunkSize;
    UINTN DesignatedVictimSize;
    PHEAP_CHUNK Next;
    UINTN NextSize;
    PHEAP_CHUNK OldFirst;
    UINTN OldFirstSize;
    UINTN TopSize;

    Chunk = HEAP_ALIGN_AS_CHUNK(NewBase);
    OldFirst = HEAP_ALIGN_AS_CHUNK(OldBase);
    ChunkSize = (PCHAR)OldFirst - (PCHAR)Chunk;
    Next = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
    NextSize = ChunkSize - Size;
    HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(Heap, Chunk, Size);
    Chunk->Tag = Tag;
    Heap->Statistics.FreeListSize -= Size;

    ASSERT((PCHAR)OldFirst > (PCHAR)Next);
    ASSERT(HEAP_CHUNK_IS_IN_USE(OldFirst));
    ASSERT(NextSize >= HEAP_MIN_CHUNK_SIZE);

    //
    // Consolidate the remainder with the first chunk of the old base.
    //

    if (OldFirst == Heap->Top) {
        Heap->TopSize += NextSize;
        TopSize = Heap->TopSize;
        Heap->Top = Next;
        Next->Header = TopSize | HEAP_CHUNK_IN_USE;
        RtlpHeapCheckTopChunk(Heap, Next);

    } else if (OldFirst == Heap->DesignatedVictim) {
        Heap->DesignatedVictimSize += NextSize;
        DesignatedVictimSize = Heap->DesignatedVictimSize;
        Heap->DesignatedVictim = Next;
        HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Next, DesignatedVictimSize);

    } else {
        if (!HEAP_CHUNK_IS_IN_USE(OldFirst)) {
            OldFirstSize = HEAP_CHUNK_SIZE(OldFirst);
            HEAP_UNLINK_CHUNK(Heap, OldFirst, OldFirstSize);
            OldFirst = HEAP_CHUNK_PLUS_OFFSET(OldFirst, OldFirstSize);
            NextSize += OldFirstSize;
        }

        HEAP_SET_FREE_PREVIOUS_IN_USE(Next, NextSize, OldFirst);
        HEAP_INSERT_CHUNK(Heap, Next, NextSize);
        RtlpHeapCheckFreeChunk(Heap, Next);
    }

    RtlpHeapCheckAllocatedChunk(Heap, HEAP_CHUNK_TO_MEMORY(Chunk), Size);
    return HEAP_CHUNK_TO_MEMORY(Chunk);
}

VOID
RtlpHeapAddSegment (
    PMEMORY_HEAP Heap,
    PCHAR Base,
    UINTN Size
    )

/*++

Routine Description:

    This routine adds a new segment of memory to the heap.

Arguments:

    Heap - Supplies a pointer to the heap.

    Base - Supplies a pointer to the base of the allocation.

    Size - Supplies the size in bytes of the allocation.

Return Value:

    None.

--*/

{

    PCHAR AfterSegment;
    PHEAP_CHUNK Chunk;
    PCHAR CurrentSegment;
    ULONG FenceCount;
    PHEAP_CHUNK Next;
    UINTN Offset;
    PCHAR OldEnd;
    PHEAP_SEGMENT OldSegment;
    PCHAR OldTop;
    PCHAR RawSegment;
    PHEAP_CHUNK Remainder;
    PHEAP_CHUNK RemainderNext;
    UINTN RemainderSize;
    PHEAP_SEGMENT Segment;
    PHEAP_CHUNK SegmentChunk;
    UINTN SegmentSize;

    OldTop = (PCHAR)(Heap->Top);
    OldSegment = RtlpHeapSegmentHolding(Heap, OldTop);
    OldEnd = OldSegment->Base + OldSegment->Size;
    SegmentSize = HEAP_PAD_REQUEST(sizeof(HEAP_SEGMENT));
    RawSegment = OldEnd -
                 (SegmentSize + (4 * sizeof(UINTN)) + HEAP_CHUNK_ALIGN_MASK);

    Offset = HEAP_ALIGNMENT_OFFSET(HEAP_CHUNK_TO_MEMORY(RawSegment));
    AfterSegment = RawSegment + Offset;
    CurrentSegment = AfterSegment;
    if (AfterSegment < OldTop + HEAP_MIN_CHUNK_SIZE) {
        CurrentSegment = OldTop;
    }

    SegmentChunk = (PHEAP_CHUNK)CurrentSegment;
    Segment = HEAP_CHUNK_TO_MEMORY(SegmentChunk);
    Next = HEAP_CHUNK_PLUS_OFFSET(SegmentChunk, SegmentSize);
    Chunk = Next;
    FenceCount = 0;

    //
    // Reset the top to the new space.
    //

    RtlpHeapInitializeTop(Heap, (PHEAP_CHUNK)Base, Size - HEAP_TOP_FOOTER_SIZE);

    //
    // Set up the segment record.
    //

    ASSERT(HEAP_IS_ALIGNED(Segment));

    HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(Heap, SegmentChunk, SegmentSize);

    //
    // Push the current record.
    //

    RtlCopyMemory(Segment, &(Heap->Segment), sizeof(HEAP_SEGMENT));
    Heap->Segment.Base = Base;
    Heap->Segment.Size = Size;
    Heap->Segment.Flags = 0;
    Heap->Segment.Next = Segment;

    //
    // Insert trailing fenceposts.
    //

    while (TRUE) {
        Next = HEAP_CHUNK_PLUS_OFFSET(Chunk, sizeof(UINTN));
        Chunk->Header = HEAP_FENCEPOST_HEADER;
        FenceCount += 1;
        if ((PCHAR)(&(Next->Header)) < OldEnd) {
            Chunk = Next;

        } else {
            break;
        }
    }

    ASSERT(FenceCount >= 2);

    //
    // Adjust the free list size due to these fenceposts.
    //

    Heap->Statistics.FreeListSize -= SegmentSize +
                                     (sizeof(UINTN) * (FenceCount + 1));

    //
    // Insert the rest of the old top into a bin as an ordinary free chunk.
    //

    if (CurrentSegment != OldTop) {
        Remainder = (PHEAP_CHUNK)OldTop;
        RemainderSize = CurrentSegment - OldTop;
        RemainderNext = HEAP_CHUNK_PLUS_OFFSET(Remainder, RemainderSize);
        HEAP_SET_FREE_PREVIOUS_IN_USE(Remainder, RemainderSize, RemainderNext);
        HEAP_INSERT_CHUNK(Heap, Remainder, RemainderSize);
    }

    RtlpHeapCheckTopChunk(Heap, Heap->Top);
    return;
}

PHEAP_CHUNK
RtlpHeapTryToReallocateChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size,
    BOOL CanMove
    )

/*++

Routine Description:

    This routine attempts to reallocate a chunk. It will only do in-place
    reallocations unless the can move flag parameter is set.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the original chunk to be reallocated.

    Size - Supplies the new required size in bytes.

    CanMove - Supplies a boolean indicating if the allocation base can be
        moved or not.

Return Value:

    Returns a pointer to the new heap chunk on success.

    NULL on allocation failure.

--*/

{

    UINTN DesignatedVictimSize;
    PHEAP_CHUNK NewChunk;
    UINTN NewSize;
    PHEAP_CHUNK NewTop;
    UINTN NewTopSize;
    PHEAP_CHUNK NextChunk;
    UINTN NextSize;
    UINTN OldSize;
    PHEAP_CHUNK Replacement;
    UINTN ReplacementSize;

    NewChunk = NULL;
    OldSize = HEAP_CHUNK_SIZE(Chunk);
    NextChunk = HEAP_CHUNK_PLUS_OFFSET(Chunk, OldSize);
    if (!HEAP_CHUNK_IS_IN_USE(Chunk)) {
        HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionDoubleFree, Chunk);
        return NULL;
    }

    if ((!HEAP_OK_ADDRESS(Heap, Chunk)) ||
        (!HEAP_OK_ADDRESS(Heap, NextChunk)) ||
        (!HEAP_CHUNK_IS_PREVIOUS_IN_USE(NextChunk))) {

        HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionCorruptStructures, Chunk);
        return NULL;
    }

    //
    // Memory mapped chunks can't be resized.
    //

    if (HEAP_CHUNK_IS_MMAPPED(Chunk)) {
        NewChunk = NULL;

    //
    // If the old chunk is already big enough, just return it.
    //

    } else if (OldSize >= Size) {
        ReplacementSize = OldSize - Size;
        if (ReplacementSize >= HEAP_MIN_CHUNK_SIZE) {
            Replacement = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
            HEAP_CHUNK_SET_IN_USE(Heap, Chunk, Size);
            HEAP_CHUNK_SET_IN_USE(Heap, Replacement, ReplacementSize);
            RtlpHeapDisposeOfChunk(Heap, Replacement, ReplacementSize);
            Heap->Statistics.FreeListSize += ReplacementSize;
        }

        NewChunk = Chunk;

    //
    // If the next chunk is the top, extend into it.
    //

    } else if (NextChunk == Heap->Top) {
        if (OldSize + Heap->TopSize > Size) {
            NewSize = OldSize + Heap->TopSize;
            NewTopSize = NewSize - Size;
            NewTop = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
            HEAP_CHUNK_SET_IN_USE(Heap, Chunk, Size);
            Heap->Statistics.FreeListSize -= Size - OldSize;
            NewTop->Header = NewTopSize | HEAP_CHUNK_PREVIOUS_IN_USE;
            Heap->Top = NewTop;
            Heap->TopSize = NewTopSize;
            NewChunk = Chunk;
        }

    //
    // If the next chunk is the designated victim, extend into that.
    //

    } else if (NextChunk == Heap->DesignatedVictim) {
        DesignatedVictimSize = Heap->DesignatedVictimSize;
        if (OldSize + DesignatedVictimSize >= Size) {
            ReplacementSize = OldSize + DesignatedVictimSize - Size;
            if (ReplacementSize >= HEAP_MIN_CHUNK_SIZE) {
                Replacement = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
                NextChunk = HEAP_CHUNK_PLUS_OFFSET(Replacement,
                                                   ReplacementSize);

                HEAP_CHUNK_SET_IN_USE(Heap, Chunk, Size);
                HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Replacement,
                                                     ReplacementSize);

                HEAP_CHUNK_CLEAR_PREVIOUS_IN_USE(NextChunk);
                Heap->DesignatedVictim = Replacement;
                Heap->DesignatedVictimSize = ReplacementSize;

            //
            // Exhaust the designated victim.
            //

            } else {
                Size = OldSize + DesignatedVictimSize;
                HEAP_CHUNK_SET_IN_USE(Heap, Chunk, Size);
                Heap->DesignatedVictim = NULL;
                Heap->DesignatedVictimSize = 0;
            }

            Heap->Statistics.FreeListSize -= Size - OldSize;
            NewChunk = Chunk;
        }

    //
    // If the next chunk is free, extend into that.
    //

    } else if (!HEAP_CHUNK_IS_CURRENT_IN_USE(NextChunk)) {
        NextSize = HEAP_CHUNK_SIZE(NextChunk);
        if (OldSize + NextSize >= Size) {
            ReplacementSize = OldSize + NextSize - Size;
            HEAP_UNLINK_CHUNK(Heap, NextChunk, NextSize);
            if (ReplacementSize >= HEAP_MIN_CHUNK_SIZE) {
                Replacement = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
                HEAP_CHUNK_SET_IN_USE(Heap, Chunk, Size);
                HEAP_CHUNK_SET_IN_USE(Heap, Replacement, ReplacementSize);
                RtlpHeapDisposeOfChunk(Heap, Replacement, ReplacementSize);

            //
            // Use the whole next chunk.
            //

            } else {
                Size = OldSize + NextSize;
                HEAP_CHUNK_SET_IN_USE(Heap, Chunk, Size);
            }

            Heap->Statistics.FreeListSize -= Size - OldSize;
            NewChunk = Chunk;
        }
    }

    if ((NewChunk != NULL) &&
        ((Heap->Flags & MEMORY_HEAP_FLAG_COLLECT_TAG_STATISTICS) != 0)) {

        RtlpCollectTagStatistics(Heap,
                                 Chunk->Tag,
                                 OldSize,
                                 FALSE);

        RtlpCollectTagStatistics(Heap,
                                 Chunk->Tag,
                                 HEAP_CHUNK_SIZE(NewChunk),
                                 TRUE);
    }

    return NewChunk;
}

VOID
RtlpHeapDisposeOfChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN ChunkSize
    )

/*++

Routine Description:

    This routine frees a chunk that wasn't necessarily marked as allocated.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the chunk to release.

    ChunkSize - Supplies the size of the given chunk in bytes.

Return Value:

    None.

--*/

{

    UINTN DesignatedVictimSize;
    PHEAP_CHUNK Next;
    UINTN NextSize;
    PHEAP_CHUNK Previous;
    UINTN PreviousSize;
    BOOL Success;
    UINTN TopSize;

    Next = HEAP_CHUNK_PLUS_OFFSET(Chunk, ChunkSize);
    if (!HEAP_CHUNK_IS_PREVIOUS_IN_USE(Chunk)) {
        PreviousSize = Chunk->PreviousFooter;
        if (HEAP_CHUNK_IS_MMAPPED(Chunk)) {
            ChunkSize += PreviousSize + HEAP_MMAP_FOOTER_PAD;
            if (Heap->FreeFunction != NULL) {
                Heap->Statistics.TotalHeapSize -= ChunkSize;
                Success = Heap->FreeFunction(Heap,
                                             (PCHAR)Chunk - PreviousSize,
                                             ChunkSize);

                //
                // If the free failed, put things back.
                //

                if (Success == FALSE) {
                    Heap->Statistics.TotalHeapSize += ChunkSize;
                }
            }

            return;
        }

        Previous = HEAP_CHUNK_MINUS_OFFSET(Chunk, PreviousSize);
        ChunkSize += PreviousSize;
        Chunk = Previous;

        //
        // Consolidate backward.
        //

        if (HEAP_OK_ADDRESS(Heap, Previous)) {
            if (Chunk != Heap->DesignatedVictim) {
                HEAP_UNLINK_CHUNK(Heap, Chunk, PreviousSize);

            } else if ((Next->Header & HEAP_CHUNK_IN_USE_MASK) ==
                       HEAP_CHUNK_IN_USE_MASK) {

                Heap->DesignatedVictimSize = ChunkSize;
                HEAP_SET_FREE_PREVIOUS_IN_USE(Chunk, ChunkSize, Next);
                return;
            }

        } else {
            HEAP_HANDLE_CORRUPTION(Heap,
                                   HeapCorruptionCorruptStructures,
                                   Chunk);

            return;
        }
    }

    if (!HEAP_OK_ADDRESS(Heap, Next)) {
        HEAP_HANDLE_CORRUPTION(Heap,
                               HeapCorruptionCorruptStructures,
                               Chunk);

        return;
    }

    //
    // Consolidate forward.
    //

    if (!HEAP_CHUNK_IS_CURRENT_IN_USE(Next)) {
        if (Next == Heap->Top) {
            Heap->TopSize += ChunkSize;
            TopSize = Heap->TopSize;
            Heap->Top = Chunk;
            Chunk->Header = TopSize | HEAP_CHUNK_PREVIOUS_IN_USE;
            if (Chunk == Heap->DesignatedVictim) {
                Heap->DesignatedVictim = NULL;
                Heap->DesignatedVictimSize = 0;
            }

            return;

        } else if (Next == Heap->DesignatedVictim) {
            Heap->DesignatedVictimSize += ChunkSize;
            DesignatedVictimSize = Heap->DesignatedVictimSize;
            Heap->DesignatedVictim = Chunk;
            HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Chunk, DesignatedVictimSize);
            return;

        } else {
            NextSize = HEAP_CHUNK_SIZE(Next);
            ChunkSize += NextSize;
            HEAP_UNLINK_CHUNK(Heap, Next, NextSize);
            HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Chunk, ChunkSize);
            if (Chunk == Heap->DesignatedVictim) {
                Heap->DesignatedVictimSize = ChunkSize;
                return;
            }
        }

    } else {
        HEAP_SET_FREE_PREVIOUS_IN_USE(Chunk, ChunkSize, Next);
    }

    //
    // The chunk was not consolidated, so add it into the free structures.
    //

    HEAP_INSERT_CHUNK(Heap, Chunk, ChunkSize);
    return;
}

BOOL
RtlpHeapTrim (
    PMEMORY_HEAP Heap,
    UINTN Padding
    )

/*++

Routine Description:

    This routine trims the top wilderness area of the heap if it is getting
    too large.

Arguments:

    Heap - Supplies a pointer to the heap.

    Padding - Supplies the number of bytes of padding needed on the top chunk.

Return Value:

    TRUE if the heap shrunk.

    FALSE if nothing changed.

--*/

{

    UINTN Extra;
    UINTN MemoryReleased;
    UINTN NewSize;
    UINTN OriginalTopSize;
    PHEAP_SEGMENT Segment;
    BOOL Success;
    UINTN Unit;

    MemoryReleased = 0;
    if ((Padding < HEAP_MAX_REQUEST) && (HEAP_IS_INITIALIZED(Heap))) {
        Padding += HEAP_TOP_FOOTER_SIZE;
        if (Heap->TopSize > Padding) {

            //
            // Shrink the top space in granularity sized units, keeping at
            // least one.
            //

            Unit = Heap->ExpansionGranularity;
            Extra = (((Heap->TopSize - Padding + (Unit - 1)) / Unit) - 1) *
                    Unit;

            Segment = RtlpHeapSegmentHolding(Heap, (PCHAR)(Heap->Top));
            if (!HEAP_IS_EXTERNAL_SEGMENT(Segment)) {

                //
                // If this is not the complete segment and no other segments
                // point at it, then free it.
                //

                if ((Segment->Size >= Extra) &&
                    (RtlpHeapHasSegmentLink(Heap, Segment) == FALSE)) {

                    OriginalTopSize = Heap->TopSize;
                    NewSize = Segment->Size - Extra;
                    MemoryReleased = Extra;
                    Segment->Size -= MemoryReleased;
                    Heap->Statistics.TotalHeapSize -= MemoryReleased;
                    Heap->Statistics.FreeListSize -= MemoryReleased;
                    RtlpHeapInitializeTop(Heap,
                                          Heap->Top,
                                          Heap->TopSize - MemoryReleased);

                    RtlpHeapCheckTopChunk(Heap, Heap->Top);
                    Success = Heap->FreeFunction(Heap,
                                                 Segment->Base + NewSize,
                                                 Extra);

                    //
                    // On success, knock the previous expansion down by one so
                    // the expansion doesn't double if memory was trimmed.
                    // Without this, the heap tends to expand some huge region,
                    // trim most of it, then expand again with double the huge
                    // region, etc.
                    //

                    if (Success != FALSE) {

                        //
                        // Trim the heap again when it frees 5/4 of the
                        // previous expansion size.
                        //

                        Heap->TrimCheck = Heap->PreviousExpansionSize +
                                          (Heap->PreviousExpansionSize >> 2);

                        Heap->PreviousExpansionSize >>= 1;

                    //
                    // If the free failed, put everything back.
                    //

                    } else {
                        Segment->Size += MemoryReleased;
                        Heap->Statistics.TotalHeapSize += MemoryReleased;
                        Heap->Statistics.FreeListSize += MemoryReleased;
                        RtlpHeapInitializeTop(Heap,
                                              Heap->Top,
                                              OriginalTopSize);

                        RtlpHeapCheckTopChunk(Heap, Heap->Top);
                        MemoryReleased = 0;
                    }
                }
            }
        }

        MemoryReleased += RtlpHeapReleaseUnusedSegments(Heap);

        //
        // On failure, disable trimming to avoid repeated failed future calls.
        //

        if ((MemoryReleased == 0) && (Heap->TopSize > Heap->TrimCheck)) {
            Heap->TrimCheck = -1;
        }
    }

    if (MemoryReleased != 0) {
        return TRUE;
    }

    return FALSE;
}

UINTN
RtlpHeapReleaseUnusedSegments (
    PMEMORY_HEAP Heap
    )

/*++

Routine Description:

    This routine releases unused segments that don't contain any used chunks.

Arguments:

    Heap - Supplies a pointer to the heap.

Return Value:

    Returns the number of bytes released.

--*/

{

    PCHAR Base;
    UINTN BytesReleased;
    PHEAP_CHUNK Chunk;
    UINTN ChunkSize;
    PHEAP_SEGMENT NextSegment;
    PHEAP_SEGMENT PreviousSegment;
    PHEAP_SEGMENT Segment;
    UINTN SegmentCount;
    UINTN Size;
    BOOL Success;

    //
    // If memory cannot be freed from the heap, then don't bother.
    //

    if (Heap->FreeFunction == NULL) {
        return 0;
    }

    BytesReleased = 0;
    SegmentCount = 0;
    PreviousSegment = &(Heap->Segment);
    Segment = PreviousSegment->Next;
    while (Segment != NULL) {
        Base = Segment->Base;
        Size = Segment->Size;
        NextSegment = Segment->Next;
        SegmentCount += 1;
        if (!HEAP_IS_EXTERNAL_SEGMENT(Segment)) {
            Chunk = HEAP_ALIGN_AS_CHUNK(Base);
            ChunkSize = HEAP_CHUNK_SIZE(Chunk);

            //
            // Call the free function if the first chunk holds the entire
            // segment.
            //

            if ((!HEAP_CHUNK_IS_IN_USE(Chunk)) &&
                ((PCHAR)Chunk + ChunkSize >=
                 Base + Size - HEAP_TOP_FOOTER_SIZE)) {

                ASSERT(HEAP_SEGMENT_HOLDS(Segment, Segment));

                if (Heap->DesignatedVictim == Chunk) {
                    Heap->DesignatedVictim = NULL;
                    Heap->DesignatedVictimSize = 0;

                } else {
                    HEAP_UNLINK_CHUNK(Heap, Chunk, ChunkSize);
                }

                BytesReleased += Size;
                Heap->Statistics.TotalHeapSize -= Size;
                Heap->Statistics.FreeListSize -= ChunkSize;

                //
                // Unlink the destroyed record, and call free.
                //

                PreviousSegment->Next = NextSegment;
                Success = Heap->FreeFunction(Heap, Base, Size);

                //
                // If the free failed, put things back, re-link the segment,
                // and insert the chunk.
                //

                if (Success == FALSE) {
                    BytesReleased -= Size;
                    Heap->Statistics.TotalHeapSize += Size;
                    Heap->Statistics.FreeListSize += ChunkSize;

                    //
                    // Unlink the destroyed record.
                    //

                    PreviousSegment->Next = Segment;
                    RtlpHeapInsertLargeChunk(Heap,
                                             (PHEAP_TREE_CHUNK)Chunk,
                                             ChunkSize);

                //
                // The free succeeded, the segment released is gone, the new
                // current is the previous.
                //

                } else {
                    Segment = PreviousSegment;
                }
            }
        }

        if (HEAP_SEGMENT_TRAVERSAL == 0) {
            break;
        }

        PreviousSegment = Segment;
        Segment = NextSegment;
    }

    //
    // Reset the release checks counter.
    //

    Heap->ReleaseChecks = HEAP_MAX_RELEASE_CHECK_RATE;
    if (SegmentCount > Heap->ReleaseChecks) {
        Heap->ReleaseChecks = SegmentCount;
    }

    return BytesReleased;
}

PVOID
RtlpHeapTreeAllocateLarge (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine allocates a large request from the best fitting chunk in a
    tree bin.

Arguments:

    Heap - Supplies a pointer to the heap.

    Size - Supplies the size of the requested allocation.

    Tag - Supplies the tag associated with the allocation.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on allocation failure.

--*/

{

    UINTN ChildIndex;
    HEAP_BINDEX Index;
    HEAP_BINMAP LeastBit;
    HEAP_BINMAP LeftBits;
    PHEAP_TREE_CHUNK Node;
    PHEAP_CHUNK Remainder;
    UINTN RemainderSize;
    PHEAP_TREE_CHUNK Right;
    PHEAP_TREE_CHUNK RightSubTree;
    UINTN SizeBits;
    PHEAP_TREE_CHUNK Tree;
    UINTN TreeRemainder;

    //
    // Use unsigned negation.
    //

    RemainderSize = -Size;
    Node = NULL;
    Index = RtlpHeapComputeTreeIndex(Size);
    Tree = *HEAP_TREE_BIN_AT(Heap, Index);
    if (Tree != NULL) {

        //
        // Traverse the tree for this bin looking for a node with the requested
        // size.
        //

        SizeBits = Size << HEAP_LEFT_SHIFT_FOR_TREE_INDEX(Index);
        RightSubTree = NULL;
        while (TRUE) {
            TreeRemainder = HEAP_CHUNK_SIZE(Tree) - Size;
            if (TreeRemainder < RemainderSize) {
                Node = Tree;
                RemainderSize = TreeRemainder;
                if (RemainderSize == 0) {
                    break;
                }
            }

            Right = Tree->Child[1];
            ChildIndex = (SizeBits >> ((sizeof(UINTN) * BITS_PER_BYTE) - 1)) &
                         0x1;

            Tree = Tree->Child[ChildIndex];
            if ((Right != NULL) && (Right != Tree)) {
                RightSubTree = Right;
            }

            //
            // If the tree went too far, set the tree to the least subtree
            // holding sizes greater than the requested size.
            //

            if (Tree == NULL) {
                Tree = RightSubTree;
                break;
            }

            SizeBits <<= 1;
        }
    }

    //
    // If nothing was found, set the tree to the root of the next non-empty
    // tree bin.
    //

    if ((Tree == NULL) && (Node == NULL)) {
        LeftBits = HEAP_LEFT_BITS(HEAP_INDEX_TO_BIT(Index)) & Heap->TreeMap;
        if (LeftBits != 0) {
            LeastBit = HEAP_LEAST_BIT(LeftBits);
            Index = HEAP_COMPUTE_BIT_INDEX(LeastBit);
            Tree = *HEAP_TREE_BIN_AT(Heap, Index);
        }
    }

    //
    // Find the smallest of the tree or its subtree.
    //

    while (Tree != NULL) {
        TreeRemainder = HEAP_CHUNK_SIZE(Tree) - Size;
        if (TreeRemainder < RemainderSize) {
            RemainderSize = TreeRemainder;
            Node = Tree;
        }

        Tree = HEAP_TREE_LEFTMOST_CHILD(Tree);
    }

    //
    // Return the found chunk, unless the designated victim is a better fit.
    //

    if ((Node != NULL) && (RemainderSize < Heap->DesignatedVictimSize - Size)) {
        Remainder = HEAP_CHUNK_PLUS_OFFSET(Node, Size);
        if ((HEAP_OK_ADDRESS(Heap, Node)) &&
            (HEAP_OK_NEXT(Node, Remainder))) {

            ASSERT(HEAP_CHUNK_SIZE(Node) == Size + RemainderSize);

            RtlpHeapUnlinkLargeChunk(Heap, Node);
            if (RemainderSize < HEAP_MIN_CHUNK_SIZE) {
                Size += RemainderSize;
                HEAP_SET_CURRENT_PREVIOUS_IN_USE(Heap, Node, Size);

            } else {
                HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(Heap, Node, Size);
                HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Remainder, RemainderSize);
                HEAP_INSERT_CHUNK(Heap, Remainder, RemainderSize);
            }

            Node->Tag = Tag;
            Heap->Statistics.FreeListSize -= Size;
            return HEAP_CHUNK_TO_MEMORY(Node);

        } else {
            HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionCorruptStructures, Node);
        }
    }

    return NULL;
}

PVOID
RtlpHeapTreeAllocateSmall (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine allocates a small request from the best fitting chunk in a
    tree bin.

Arguments:

    Heap - Supplies a pointer to the heap.

    Size - Supplies the size of the requested allocation.

    Tag - Supplies the tag associated with the allocation.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on allocation failure.

--*/

{

    PHEAP_TREE_CHUNK Child;
    HEAP_BINDEX Index;
    HEAP_BINMAP LeastBit;
    PHEAP_TREE_CHUNK Node;
    PHEAP_CHUNK Remainder;
    UINTN RemainderSize;
    UINTN TreeRemainder;

    LeastBit = HEAP_LEAST_BIT(Heap->TreeMap);
    Index = HEAP_COMPUTE_BIT_INDEX(LeastBit);
    Node = *HEAP_TREE_BIN_AT(Heap, Index);
    Child = Node;
    RemainderSize = HEAP_CHUNK_SIZE(Child) - Size;

    //
    // Find the child with the best fit (smallest remainder).
    //

    while (TRUE) {
        Child = HEAP_TREE_LEFTMOST_CHILD(Child);
        if (Child == NULL) {
            break;
        }

        TreeRemainder = HEAP_CHUNK_SIZE(Child) - Size;
        if (TreeRemainder < RemainderSize) {
            RemainderSize = TreeRemainder;
            Node = Child;
        }
    }

    if (HEAP_OK_ADDRESS(Heap, Node)) {
        Remainder = HEAP_CHUNK_PLUS_OFFSET(Node, Size);

        ASSERT(HEAP_CHUNK_SIZE(Node) == RemainderSize + Size);

        if (HEAP_OK_NEXT(Node, Remainder)) {
            RtlpHeapUnlinkLargeChunk(Heap, Node);
            if (RemainderSize < HEAP_MIN_CHUNK_SIZE) {
                Size += RemainderSize;
                HEAP_SET_CURRENT_PREVIOUS_IN_USE(Heap, Node, Size);

            } else {
                HEAP_SET_SIZE_PREVIOUS_OF_IN_USE_CHUNK(Heap, Node, Size);
                HEAP_SET_SIZE_PREVIOUS_OF_FREE_CHUNK(Remainder, RemainderSize);
                RtlpHeapReplaceDesignatedVictim(Heap, Remainder, RemainderSize);
            }

            Node->Tag = Tag;
            Heap->Statistics.FreeListSize -= Size;
            return HEAP_CHUNK_TO_MEMORY(Node);
        }
    }

    HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionCorruptStructures, NULL);
    return NULL;
}

VOID
RtlpHeapReplaceDesignatedVictim (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size
    )

/*++

Routine Description:

    This routine replaces the designated victim with the given new victim.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the new victim.

    Size - Supplies the new victim's size.

Return Value:

    None.

--*/

{

    PHEAP_CHUNK Original;
    UINTN OriginalSize;

    OriginalSize = Heap->DesignatedVictimSize;

    ASSERT(HEAP_IS_SMALL(OriginalSize));

    if (OriginalSize != 0) {
        Original = Heap->DesignatedVictim;
        RtlpHeapInsertSmallChunk(Heap, Original, OriginalSize);
    }

    Heap->DesignatedVictim = Chunk;
    Heap->DesignatedVictimSize = Size;
    return;
}

VOID
RtlpHeapInsertSmallChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size
    )

/*++

Routine Description:

    This routine links a free chunk into a small bin.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the newly free chunk.

    Size - Supplies the size of the chunk.

Return Value:

    None.

--*/

{

    PHEAP_CHUNK Bin;
    HEAP_BINDEX Index;
    PHEAP_CHUNK Next;

    Index = HEAP_SMALL_INDEX(Size);
    Bin = HEAP_SMALL_BIN_AT(Heap, Index);
    Next = Bin;

    ASSERT(Size >= HEAP_MIN_CHUNK_SIZE);

    if (!HEAP_IS_SMALL_MAP_MARKED(Heap, Index)) {
        HEAP_MARK_SMALL_MAP(Heap, Index);

    } else if (HEAP_OK_ADDRESS(Heap, Bin->Next)) {
        Next = Bin->Next;

    } else {
        HEAP_HANDLE_CORRUPTION(Heap,
                               HeapCorruptionCorruptStructures,
                               Chunk);
    }

    Bin->Next = Chunk;
    Next->Previous = Chunk;
    Chunk->Next = Next;
    Chunk->Previous = Bin;
    return;
}

VOID
RtlpHeapInsertLargeChunk (
    PMEMORY_HEAP Heap,
    PHEAP_TREE_CHUNK Chunk,
    UINTN Size
    )

/*++

Routine Description:

    This routine links a free chunk into a large tree.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the newly free chunk.

    Size - Supplies the size of the chunk.

Return Value:

    None.

--*/

{

    PHEAP_TREE_CHUNK *Child;
    UINTN ChildIndex;
    PHEAP_TREE_CHUNK First;
    PHEAP_TREE_CHUNK *Head;
    HEAP_BINDEX Index;
    UINTN SizeBits;
    PHEAP_TREE_CHUNK Tree;

    Index = RtlpHeapComputeTreeIndex(Size);
    Head = HEAP_TREE_BIN_AT(Heap, Index);
    Chunk->Index = Index;
    Chunk->Child[0] = NULL;
    Chunk->Child[1] = NULL;
    if (!HEAP_IS_TREE_MAP_MARKED(Heap, Index)) {
        HEAP_MARK_TREE_MAP(Heap, Index);
        *Head = Chunk;
        Chunk->Parent = (PHEAP_TREE_CHUNK)Head;
        Chunk->Next = Chunk;
        Chunk->Previous = Chunk;

    } else {
        Tree = *Head;
        SizeBits = Size << HEAP_LEFT_SHIFT_FOR_TREE_INDEX(Index);
        while (TRUE) {
            if (HEAP_CHUNK_SIZE(Tree) != Size) {
                ChildIndex = (SizeBits >>
                              ((sizeof(UINTN) * BITS_PER_BYTE) - 1)) &
                             0x1;

                Child = &(Tree->Child[ChildIndex]);
                SizeBits <<= 1;
                if (*Child != NULL) {
                    Tree = *Child;

                } else if (HEAP_OK_ADDRESS(Heap, Child)) {
                    *Child = Chunk;
                    Chunk->Parent = Tree;
                    Chunk->Next = Chunk;
                    Chunk->Previous = Chunk;
                    break;

                } else {
                    HEAP_HANDLE_CORRUPTION(Heap,
                                           HeapCorruptionCorruptStructures,
                                           Child);

                    break;
                }

            } else {
                First = Tree->Next;
                if ((HEAP_OK_ADDRESS(Heap, Tree)) &&
                    (HEAP_OK_ADDRESS(Heap, First))) {

                    Tree->Next = Chunk;
                    First->Previous = Chunk;
                    Chunk->Next = First;
                    Chunk->Previous = Tree;
                    Chunk->Parent = NULL;
                    break;

                } else {
                    HEAP_HANDLE_CORRUPTION(Heap,
                                           HeapCorruptionCorruptStructures,
                                           Tree);
                }

                break;
            }
        }
    }

    return;
}

VOID
RtlpHeapUnlinkSmallChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size
    )

/*++

Routine Description:

    This routine unlinks a small chunk from the lists.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the chunk to unlink.

    Size - Supplies the size of the chunk.

Return Value:

    None.

--*/

{

    HEAP_BINDEX Index;
    PHEAP_CHUNK Next;
    PHEAP_CHUNK Previous;

    Next = Chunk->Next;
    Previous = Chunk->Previous;
    Index = HEAP_SMALL_INDEX(Size);

    ASSERT(Chunk != Next);
    ASSERT(Chunk != Previous);
    ASSERT(HEAP_CHUNK_SIZE(Chunk) == HEAP_SMALL_INDEX_TO_SIZE(Index));

    if ((Next == HEAP_SMALL_BIN_AT(Heap, Index)) ||
        ((HEAP_OK_ADDRESS(Heap, Next)) && (Next->Previous == Chunk))) {

        if (Previous == Next) {
            HEAP_CLEAR_SMALL_MAP(Heap, Index);

        } else if ((Previous == HEAP_SMALL_BIN_AT(Heap, Index)) ||
                   ((HEAP_OK_ADDRESS(Heap, Previous)) &&
                    (Previous->Next == Chunk))) {

            Next->Previous = Previous;
            Previous->Next = Next;

        } else {
            HEAP_HANDLE_CORRUPTION(Heap,
                                   HeapCorruptionCorruptStructures,
                                   Chunk);
        }

    } else {
        HEAP_HANDLE_CORRUPTION(Heap,
                               HeapCorruptionCorruptStructures,
                               Chunk);
    }

    return;
}

VOID
RtlpHeapUnlinkLargeChunk (
    PMEMORY_HEAP Heap,
    PHEAP_TREE_CHUNK Node
    )

/*++

Routine Description:

    This routine unlinks a large chunk from the tree.

Arguments:

    Heap - Supplies a pointer to the heap.

    Node - Supplies a pointer to the node to unlink from the tree.

Return Value:

    None.

--*/

{

    PHEAP_TREE_CHUNK *Bin;
    PHEAP_TREE_CHUNK *ChildPointer;
    PHEAP_TREE_CHUNK LeftChild;
    PHEAP_TREE_CHUNK Next;
    PHEAP_TREE_CHUNK Parent;
    PHEAP_TREE_CHUNK Replacement;
    PHEAP_TREE_CHUNK *ReplacementPointer;
    PHEAP_TREE_CHUNK RightChild;

    Parent = Node->Parent;

    //
    // If the list of same-sized entries for this node is not empty, remove
    // this node from the list, and chose the previous node as the replacement.
    //

    if (Node->Previous != Node) {
        Next = Node->Next;
        Replacement = Node->Previous;
        if ((HEAP_OK_ADDRESS(Heap, Next)) &&
            (Next->Previous == Node) && (Replacement->Next == Node)) {

            Next->Previous = Replacement;
            Replacement->Next = Next;

        } else {
            HEAP_HANDLE_CORRUPTION(Heap,
                                   HeapCorruptionCorruptStructures,
                                   Node);
        }

    //
    // This node is the last of its kind.
    //

    } else {

        //
        // Find the right mode leaf node to replace it.
        //

        ReplacementPointer = &(Node->Child[1]);
        Replacement = *ReplacementPointer;
        if (Replacement == NULL) {
            ReplacementPointer = &(Node->Child[0]);
            Replacement = *ReplacementPointer;
        }

        if (Replacement != NULL) {
            while (TRUE) {
                ChildPointer = &(Replacement->Child[1]);
                if (*ChildPointer == NULL) {
                    ChildPointer = &(Replacement->Child[0]);
                }

                if (*ChildPointer == NULL) {
                    break;
                }

                ReplacementPointer = ChildPointer;
                Replacement = *ReplacementPointer;
            }

            //
            // Wipe out the pointer to the replacement, unlinking it from its
            // parent.
            //

            if (HEAP_OK_ADDRESS(Heap, ReplacementPointer)) {
                *ReplacementPointer = NULL;

            } else {
                HEAP_HANDLE_CORRUPTION(Heap,
                                       HeapCorruptionCorruptStructures,
                                       Replacement);
            }
        }
    }

    //
    // If the node is the base of a chain (ie it has parent links), then relink
    // its parent and children to the replacement (or NULL if none).
    //

    if (Parent != NULL) {
        Bin = HEAP_TREE_BIN_AT(Heap, Node->Index);
        if (Node == *Bin) {
            *Bin = Replacement;
            if (*Bin == NULL) {
                HEAP_CLEAR_TREE_MAP(Heap, Node->Index);
            }

        //
        // Move the replacement underneath the parent.
        //

        } else if (HEAP_OK_ADDRESS(Heap, Parent)) {
            if (Parent->Child[0] == Node) {
                Parent->Child[0] = Replacement;

            } else {

                ASSERT(Parent->Child[1] == Node);

                Parent->Child[1] = Replacement;
            }

        } else {
            HEAP_HANDLE_CORRUPTION(Heap, HeapCorruptionCorruptStructures, Node);
        }

        //
        // Fix up the children of the original node to point up at the
        // replacement.
        //

        if (Replacement != NULL) {
            if (HEAP_OK_ADDRESS(Heap, Replacement)) {
                Replacement->Parent = Parent;
                LeftChild = Node->Child[0];
                if (LeftChild != NULL) {
                    if (HEAP_OK_ADDRESS(Heap, LeftChild)) {
                        Replacement->Child[0] = LeftChild;
                        LeftChild->Parent = Replacement;

                    } else {
                        HEAP_HANDLE_CORRUPTION(Heap,
                                               HeapCorruptionCorruptStructures,
                                               Node);
                    }
                }

                RightChild = Node->Child[1];
                if (RightChild != NULL) {
                    if (HEAP_OK_ADDRESS(Heap, RightChild)) {
                        Replacement->Child[1] = RightChild;
                        RightChild->Parent = Replacement;

                    } else {
                        HEAP_HANDLE_CORRUPTION(Heap,
                                               HeapCorruptionCorruptStructures,
                                               Node);
                    }
                }

            } else {
                HEAP_HANDLE_CORRUPTION(Heap,
                                       HeapCorruptionCorruptStructures,
                                       Node);
            }
        }
    }

    return;
}

HEAP_BINDEX
RtlpHeapComputeTreeIndex (
    UINTN Size
    )

/*++

Routine Description:

    This routine returns the tree index for the given size.

Arguments:

    Size - Supplies the allocation size to get the corresponding tree for.

Return Value:

    Returns the tree index for the given allocation size.

--*/

{

    UINTN Index;
    HEAP_BINDEX Result;
    UINTN ShiftedSize;

    ShiftedSize = Size >> HEAP_TREE_BIN_SHIFT;
    if (ShiftedSize == 0) {
        return 0;

    } else if (ShiftedSize > 0xFFFF) {
        return HEAP_TREE_BIN_COUNT - 1;
    }

    Index = (UINTN)sizeof(UINTN) * BITS_PER_BYTE - 1 -
            RtlCountLeadingZeros(ShiftedSize);

    Result = (HEAP_BINDEX)((Index << 1) +
                           (Size >> (Index + (HEAP_TREE_BIN_SHIFT - 1)) & 0x1));

    return Result;
}

VOID
RtlpHeapInitializeBins (
    PMEMORY_HEAP Heap
    )

/*++

Routine Description:

    This routine initializes the small bins in a heap.

Arguments:

    Heap - Supplies the heap to initialize.

Return Value:

    None.

--*/

{

    PHEAP_CHUNK Bin;
    HEAP_BINDEX Index;

    for (Index = 0; Index < HEAP_SMALL_BIN_COUNT; Index += 1) {
        Bin = HEAP_SMALL_BIN_AT(Heap, Index);
        Bin->Next = Bin;
        Bin->Previous = Bin;
    }

    return;
}

VOID
RtlpHeapInitializeTop (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk,
    UINTN Size
    )

/*++

Routine Description:

    This routine initializes the top chunk of memory.

Arguments:

    Heap - Supplies the heap to initialize.

    Chunk - Supplies a pointer to the top chunk to use.

    Size - Supplies the size of the top chunk.

Return Value:

    None.

--*/

{

    UINTN Offset;

    Offset = HEAP_ALIGNMENT_OFFSET(HEAP_CHUNK_TO_MEMORY(Chunk));

    //
    // The free list size was adjusted assuming the whole chunk would make it
    // in. Make an adjustment if the beginning is being sliced off.
    //

    Heap->Statistics.FreeListSize -= Offset;
    Chunk = HEAP_CHUNK_PLUS_OFFSET(Chunk, Offset);
    Size -= Offset;
    Heap->Top = Chunk;
    Heap->TopSize = Size;
    Chunk->Header = Size | HEAP_CHUNK_PREVIOUS_IN_USE;

    //
    // Set the size of the fake trailing chunk holding the overhead once.
    //

    HEAP_CHUNK_PLUS_OFFSET(Chunk, Size)->Header = HEAP_TOP_FOOTER_SIZE;
    Heap->TrimCheck = HEAP_DEFAULT_TRIM_THRESHOLD;
    return;
}

UINTN
RtlpHeapTraverseAndCheck (
    PMEMORY_HEAP Heap
    )

/*++

Routine Description:

    This routine traverses each chunk and checks it.

Arguments:

    Heap - Supplies a pointer to the heap.

Return Value:

    Returns the total size of the heap.

--*/

{

    PHEAP_CHUNK Chunk;
    UINTN FreeListSize;
    PHEAP_CHUNK PreviousChunk;
    PHEAP_SEGMENT Segment;
    UINTN Sum;

    FreeListSize = 0;
    Sum = 0;
    if (HEAP_IS_INITIALIZED(Heap)) {
        Segment = &(Heap->Segment);
        Sum += Heap->TopSize + HEAP_TOP_FOOTER_SIZE;
        FreeListSize += Heap->TopSize + HEAP_TOP_FOOTER_SIZE;
        while (Segment != NULL) {
            Chunk = HEAP_ALIGN_AS_CHUNK(Segment->Base);
            PreviousChunk = NULL;

            ASSERT(HEAP_CHUNK_IS_PREVIOUS_IN_USE(Chunk));

            while ((HEAP_SEGMENT_HOLDS(Segment, Chunk)) &&
                   (Chunk != Heap->Top) &&
                   (Chunk->Header != HEAP_FENCEPOST_HEADER)) {

                Sum += HEAP_CHUNK_SIZE(Chunk);
                if (HEAP_CHUNK_IS_IN_USE(Chunk)) {

                    ASSERT(RtlpHeapFindInBins(Heap, Chunk) == FALSE);

                    RtlpHeapCheckInUseChunk(Heap, Chunk);

                } else {

                    ASSERT((Chunk == Heap->DesignatedVictim) ||
                           (RtlpHeapFindInBins(Heap, Chunk) != FALSE));

                    //
                    // There should not be two consecutive free chunks.
                    //

                    ASSERT((PreviousChunk == NULL) ||
                           (HEAP_CHUNK_IS_IN_USE(PreviousChunk)));

                    RtlpHeapCheckFreeChunk(Heap, Chunk);
                    FreeListSize += HEAP_CHUNK_SIZE(Chunk);
                }

                PreviousChunk = Chunk;
                Chunk = HEAP_NEXT_CHUNK(Chunk);
            }

            Segment = Segment->Next;
        }
    }

    ASSERT(FreeListSize == Heap->Statistics.FreeListSize);

    return Sum;
}

VOID
RtlpHeapCheckSmallBin (
    PMEMORY_HEAP Heap,
    HEAP_BINDEX Index
    )

/*++

Routine Description:

    This routine validates a heap small bin.

Arguments:

    Heap - Supplies a pointer to the heap.

    Index - Supplies the bin index to check.

Return Value:

    None.

--*/

{

    PHEAP_CHUNK Bin;
    PHEAP_CHUNK Chunk;
    BOOL Empty;
    PHEAP_CHUNK Next;
    UINTN Size;

    Bin = HEAP_SMALL_BIN_AT(Heap, Index);
    Chunk = Bin->Previous;
    Empty = FALSE;
    if ((Heap->SmallMap & (1U << Index)) == 0) {
        Empty = TRUE;
    }

    if (Chunk == Bin) {

        ASSERT(Empty != FALSE);

    }

    if (Empty == FALSE) {
        while (Chunk != Bin) {
            Size = HEAP_CHUNK_SIZE(Chunk);

            //
            // Validate each (free) chunk.
            //

            RtlpHeapCheckFreeChunk(Heap, Chunk);

            //
            // Validate that the chunk belongs in the bin.
            //

            ASSERT(HEAP_SMALL_INDEX(Size) == Index);
            ASSERT((Chunk->Previous == Bin) ||
                   (HEAP_CHUNK_SIZE(Chunk->Previous) ==
                    HEAP_CHUNK_SIZE(Chunk)));

            //
            // Validate that the chunk is followed by an in-use chunk.
            //

            Next = HEAP_NEXT_CHUNK(Chunk);
            if (Next->Header != HEAP_FENCEPOST_HEADER) {
                RtlpHeapCheckInUseChunk(Heap, Next);
            }

            //
            // Move to the next chunk.
            //

            Chunk = Chunk->Previous;
        }
    }

    return;
}

VOID
RtlpHeapCheckTreeBin (
    PMEMORY_HEAP Heap,
    HEAP_BINDEX Index
    )

/*++

Routine Description:

    This routine validates a heap tree bin.

Arguments:

    Heap - Supplies a pointer to the heap.

    Index - Supplies the bin index to check.

Return Value:

    None.

--*/

{

    PHEAP_TREE_CHUNK Chunk;
    BOOL Empty;

    Chunk = *HEAP_TREE_BIN_AT(Heap, Index);
    Empty = FALSE;
    if ((Heap->TreeMap & (1U << Index)) == 0) {
        Empty = TRUE;
    }

    if (Chunk == NULL) {

        ASSERT(Empty != FALSE);
    }

    if (Empty == FALSE) {
        RtlpHeapCheckTree(Heap, Chunk);
    }

    return;
}

VOID
RtlpHeapCheckTree (
    PMEMORY_HEAP Heap,
    PHEAP_TREE_CHUNK Tree
    )

/*++

Routine Description:

    This routine validates a heap tree and its subtrees.

Arguments:

    Heap - Supplies a pointer to the heap.

    Tree - Supplies the root of the tree to validate.

Return Value:

    None.

--*/

{

    PHEAP_TREE_CHUNK Head;
    HEAP_BINDEX Index;
    PHEAP_TREE_CHUNK Sibling;
    HEAP_BINDEX TreeIndex;
    UINTN TreeSize;

    Head = NULL;
    TreeSize = HEAP_CHUNK_SIZE(Tree);
    TreeIndex = Tree->Index;
    Index = RtlpHeapComputeTreeIndex(TreeSize);

    ASSERT(TreeIndex == Index);
    ASSERT(TreeSize >= HEAP_MIN_LARGE_SIZE);
    ASSERT(TreeSize >= HEAP_MIN_SIZE_FOR_TREE_INDEX(Index));
    ASSERT((Index == HEAP_TREE_BIN_COUNT - 1) ||
           (TreeSize < HEAP_MIN_SIZE_FOR_TREE_INDEX(Index + 1)));

    //
    // Traverse through the chain of same-sized nodes.
    //

    Sibling = Tree;
    do {
        RtlpHeapCheckChunk(Heap, (PHEAP_CHUNK)Sibling);

        ASSERT(Sibling->Index == TreeIndex);
        ASSERT(HEAP_CHUNK_SIZE(Sibling) == TreeSize);
        ASSERT(!HEAP_CHUNK_IS_IN_USE(Sibling));
        ASSERT(!HEAP_CHUNK_NEXT_PREVIOUS_IN_USE(Sibling));
        ASSERT(Sibling->Next->Previous == Sibling);
        ASSERT(Sibling->Previous->Next == Sibling);

        if (Sibling->Parent == NULL) {

            ASSERT(Sibling->Child[0] == NULL);
            ASSERT(Sibling->Child[1] == NULL);

        } else {

            //
            // Only one node on the chain should be in the tree.
            //

            ASSERT(Head == NULL);

            Head = Sibling;

            ASSERT(Sibling->Parent != Sibling);
            ASSERT((Sibling->Parent->Child[0] == Sibling) ||
                   (Sibling->Parent->Child[1] == Sibling) ||
                   (*((PHEAP_TREE_CHUNK *)(Sibling->Parent)) == Sibling));

            if (Sibling->Child[0] != NULL) {

                ASSERT(Sibling->Child[0]->Parent == Sibling);
                ASSERT(Sibling->Child[0] != Sibling);

                RtlpHeapCheckTree(Heap, Sibling->Child[0]);
            }

            if (Sibling->Child[1] != NULL) {

                ASSERT(Sibling->Child[1]->Parent == Sibling);
                ASSERT(Sibling->Child[1] != Sibling);

                RtlpHeapCheckTree(Heap, Sibling->Child[1]);
            }

            if ((Sibling->Child[0] != NULL) && (Sibling->Child[1] != NULL)) {

                ASSERT(HEAP_CHUNK_SIZE(Sibling->Child[0]) <
                       HEAP_CHUNK_SIZE(Sibling->Child[1]));
            }
        }

        Sibling = Sibling->Next;

    } while (Sibling != Tree);

    ASSERT(Head != NULL);

    return;
}

VOID
RtlpHeapCheckTopChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    )

/*++

Routine Description:

    This routine validates the top chunk.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the heap allocation chunk.

Return Value:

    None.

--*/

{

    PHEAP_SEGMENT Segment;
    UINTN Size;

    Segment = RtlpHeapSegmentHolding(Heap, (PCHAR)Chunk);
    Size = HEAP_CHUNK_SIZE(Chunk);

    ASSERT(Segment != NULL);
    ASSERT((HEAP_IS_ALIGNED(HEAP_CHUNK_TO_MEMORY(Chunk))) ||
           (Chunk->Header == HEAP_FENCEPOST_HEADER));

    ASSERT(HEAP_OK_ADDRESS(Heap, Chunk));
    ASSERT(Size == Heap->TopSize);
    ASSERT(Size > 0);
    ASSERT(Size ==
           ((Segment->Base + Segment->Size) - (PCHAR)Chunk) -
           HEAP_TOP_FOOTER_SIZE);

    ASSERT(HEAP_CHUNK_IS_PREVIOUS_IN_USE(Chunk));
    ASSERT(!HEAP_CHUNK_IS_PREVIOUS_IN_USE(HEAP_CHUNK_PLUS_OFFSET(Chunk, Size)));

    return;
}

VOID
RtlpHeapCheckAllocatedChunk (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    )

/*++

Routine Description:

    This routine validates an allocated chunk at the point at which it is
    validated.

Arguments:

    Heap - Supplies a pointer to the heap.

    Memory - Supplies the allocation pointer.

    Size - Supplies the size of the allocation.

Return Value:

    None.

--*/

{

    PHEAP_CHUNK Chunk;
    UINTN HeaderSize;

    if (Memory == NULL) {
        return;
    }

    Chunk = HEAP_MEMORY_TO_CHUNK(Memory);
    HeaderSize = HEAP_CHUNK_SIZE(Chunk);
    RtlpHeapCheckInUseChunk(Heap, Chunk);

    ASSERT((HeaderSize & HEAP_CHUNK_ALIGN_MASK) == 0);
    ASSERT(HeaderSize >= HEAP_MIN_CHUNK_SIZE);
    ASSERT(HeaderSize >= Size);
    ASSERT((HEAP_CHUNK_IS_MMAPPED(Chunk)) ||
           (HeaderSize < (Size + HEAP_MIN_CHUNK_SIZE)));

    return;
}

VOID
RtlpHeapCheckInUseChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    )

/*++

Routine Description:

    This routine validates a chunk that is in use.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the heap allocation chunk.

Return Value:

    None.

--*/

{

    RtlpHeapCheckChunk(Heap, Chunk);

    ASSERT(HEAP_CHUNK_IS_IN_USE(Chunk));
    ASSERT(HEAP_CHUNK_NEXT_PREVIOUS_IN_USE(Chunk));
    ASSERT(Chunk->Tag != HEAP_FREE_MAGIC);

    //
    // Check that if the previous chunk is not in use and not mmapped, then the
    // previous chunk has a good offset.
    //

    ASSERT((HEAP_CHUNK_IS_MMAPPED(Chunk)) ||
           (HEAP_CHUNK_IS_PREVIOUS_IN_USE(Chunk)) ||
           (HEAP_NEXT_CHUNK(HEAP_PREVIOUS_CHUNK(Chunk)) == Chunk));

    if (HEAP_CHUNK_IS_MMAPPED(Chunk)) {
        RtlpHeapCheckMmappedChunk(Heap, Chunk);
    }

    return;
}

VOID
RtlpHeapCheckChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    )

/*++

Routine Description:

    This routine validates a generic chunk (not much else is guaranteed about
    it).

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the heap allocation chunk.

Return Value:

    None.

--*/

{

    ASSERT((HEAP_IS_ALIGNED(HEAP_CHUNK_TO_MEMORY(Chunk))) ||
           (Chunk->Header == HEAP_FENCEPOST_HEADER));

    ASSERT(HEAP_OK_ADDRESS(Heap, Chunk));

    return;
}

VOID
RtlpHeapCheckMmappedChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    )

/*++

Routine Description:

    This routine validates an mmapped chunk.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the heap allocation chunk.

Return Value:

    None.

--*/

{

    UINTN Length;
    UINTN Size;

    Size = HEAP_CHUNK_SIZE(Chunk);
    Length = Size + Chunk->PreviousFooter + HEAP_MMAP_FOOTER_PAD;

    ASSERT(HEAP_CHUNK_IS_MMAPPED(Chunk));
    ASSERT((HEAP_IS_ALIGNED(HEAP_CHUNK_TO_MEMORY(Chunk))) ||
           (Chunk->Header == HEAP_FENCEPOST_HEADER));

    ASSERT(HEAP_OK_ADDRESS(Heap, Chunk));
    ASSERT(!HEAP_IS_SMALL(Size));
    ASSERT((Length & (Heap->ExpansionGranularity - 1)) == 0);
    ASSERT(HEAP_CHUNK_PLUS_OFFSET(Chunk, Size)->Header ==
           HEAP_FENCEPOST_HEADER);

    ASSERT(HEAP_CHUNK_PLUS_OFFSET(Chunk, Size + sizeof(UINTN))->Header == 0);

    return;
}

VOID
RtlpHeapCheckFreeChunk (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    )

/*++

Routine Description:

    This routine validates a free chunk.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the heap allocation chunk.

Return Value:

    None.

--*/

{

    PHEAP_CHUNK Next;
    UINTN Size;

    Size = HEAP_CHUNK_SIZE(Chunk);
    Next = HEAP_CHUNK_PLUS_OFFSET(Chunk, Size);
    RtlpHeapCheckChunk(Heap, Chunk);

    ASSERT(!HEAP_CHUNK_IS_IN_USE(Chunk));
    ASSERT(!HEAP_CHUNK_NEXT_PREVIOUS_IN_USE(Chunk));
    ASSERT(!HEAP_CHUNK_IS_MMAPPED(Chunk));

    if ((Chunk != Heap->DesignatedVictim) && (Chunk != Heap->Top)) {
        if (Size >= HEAP_MIN_CHUNK_SIZE) {

            ASSERT((Size & HEAP_CHUNK_ALIGN_MASK) == 0);
            ASSERT(HEAP_IS_ALIGNED(HEAP_CHUNK_TO_MEMORY(Chunk)));
            ASSERT(Next->PreviousFooter == Size);
            ASSERT(HEAP_CHUNK_IS_PREVIOUS_IN_USE(Chunk));
            ASSERT((Next == Heap->Top) || (HEAP_CHUNK_IS_IN_USE(Next)));
            ASSERT(Chunk->Next->Previous == Chunk);
            ASSERT(Chunk->Previous->Next == Chunk);

        } else {

            //
            // This is a marker, which should always be the native size.
            //

            ASSERT(Size == sizeof(UINTN));
        }
    }

    return;
}

BOOL
RtlpHeapFindInBins (
    PMEMORY_HEAP Heap,
    PHEAP_CHUNK Chunk
    )

/*++

Routine Description:

    This routine attempts to find the given chunk in the bins somewhere.

Arguments:

    Heap - Supplies a pointer to the heap.

    Chunk - Supplies a pointer to the chunk to locate.

Return Value:

    TRUE if the chunk was located in the bins somewhere.

    FALSE if the chunk does not seem to exist in the bins.

--*/

{

    PHEAP_CHUNK Bin;
    UINTN ChildIndex;
    PHEAP_CHUNK Search;
    PHEAP_TREE_CHUNK Sibling;
    UINTN Size;
    UINTN SizeBits;
    HEAP_BINDEX SmallIndex;
    PHEAP_TREE_CHUNK Tree;
    HEAP_BINDEX TreeIndex;

    Size = HEAP_CHUNK_SIZE(Chunk);
    if (HEAP_IS_SMALL(Size)) {
        SmallIndex = HEAP_SMALL_INDEX(Size);
        Bin = HEAP_SMALL_BIN_AT(Heap, SmallIndex);
        if (HEAP_IS_SMALL_MAP_MARKED(Heap, SmallIndex)) {
            Search = Bin;
            do {
                if (Search == Chunk) {
                    return TRUE;
                }

                Search = Search->Next;

            } while (Search != Bin);
        }

    } else {
        TreeIndex = RtlpHeapComputeTreeIndex(Size);
        if (HEAP_IS_TREE_MAP_MARKED(Heap, TreeIndex)) {
            Tree = *HEAP_TREE_BIN_AT(Heap, TreeIndex);
            SizeBits = Size << HEAP_LEFT_SHIFT_FOR_TREE_INDEX(TreeIndex);
            while ((Tree != NULL) && (HEAP_CHUNK_SIZE(Tree) != Size)) {
                ChildIndex = (SizeBits >>
                              ((sizeof(UINTN) * BITS_PER_BYTE) - 1)) &
                             0x1;

                Tree = Tree->Child[ChildIndex];
                SizeBits <<= 1;
            }

            if (Tree != NULL) {
                Sibling = Tree;
                do {
                    if (Sibling == (PHEAP_TREE_CHUNK)Chunk) {
                        return TRUE;
                    }

                    Sibling = Sibling->Next;

                } while (Sibling != Tree);
            }
        }
    }

    return FALSE;
}

PHEAP_SEGMENT
RtlpHeapSegmentHolding (
    PMEMORY_HEAP Heap,
    PCHAR Address
    )

/*++

Routine Description:

    This routine returns a pointer to the segment holding the given address.

Arguments:

    Heap - Supplies a pointer to the heap.

    Address - Supplies the address in question.

Return Value:

    Returns a pointer to the segment containing the address on success.

    NULL if no known segment holds the given address.

--*/

{

    PHEAP_SEGMENT Segment;

    Segment = &(Heap->Segment);
    while (TRUE) {
        if ((Address >= Segment->Base) &&
            (Address < Segment->Base + Segment->Size)) {

            return Segment;
        }

        Segment = Segment->Next;
        if (Segment == NULL) {
            break;
        }
    }

    return NULL;
}

BOOL
RtlpHeapHasSegmentLink (
    PMEMORY_HEAP Heap,
    PHEAP_SEGMENT Segment
    )

/*++

Routine Description:

    This routine determines if this segment is linked in the segment list.

Arguments:

    Heap - Supplies a pointer to the heap.

    Segment - Supplies the segment in question.

Return Value:

    TRUE if the structure of any segment in the heap's segment list lies within
    the boundaries described by the given segment.

    FALSE if no segment pointer in the list falls within this segment's
    boundaries.

--*/

{

    PHEAP_SEGMENT Search;

    Search = &(Heap->Segment);
    while (Search != NULL) {
        if (((PCHAR)Search >= Segment->Base) &&
            ((PCHAR)Search < Segment->Base + Segment->Size)) {

            return TRUE;
        }

        Search = Search->Next;
    }

    return FALSE;
}

COMPARISON_RESULT
RtlpCompareHeapStatisticNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes.

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

    PMEMORY_HEAP_TAG_STATISTIC FirstEntry;
    PMEMORY_HEAP_TAG_STATISTIC SecondEntry;

    FirstEntry = RED_BLACK_TREE_VALUE(FirstNode,
                                      MEMORY_HEAP_TAG_STATISTIC,
                                      Node);

    SecondEntry = RED_BLACK_TREE_VALUE(SecondNode,
                                       MEMORY_HEAP_TAG_STATISTIC,
                                       Node);

    if (FirstEntry->Tag < SecondEntry->Tag) {
        return ComparisonResultAscending;
    }

    if (FirstEntry->Tag > SecondEntry->Tag) {
        return ComparisonResultDescending;
    }

    return ComparisonResultSame;
}

VOID
RtlpCollectTagStatistics (
    PMEMORY_HEAP Heap,
    ULONG Tag,
    ULONG AllocationSize,
    BOOL Allocate
    )

/*++

Routine Description:

    This routine updates the statistics on an allocation tag. This routine
    assumes the heap lock is already held.

Arguments:

    Heap - Supplies a pointer to the heap owning the allocation.

    Tag - Supplies a pointer to the tag in question.

    AllocationSize - Supplies the size of the allocate or free operation.

    Allocate - Supplies a boolean indicating whether this is an allocation
        operation (TRUE) or a free operation (FALSE).

Return Value:

    None.

--*/

{

    MEMORY_HEAP_TAG_STATISTIC SearchValue;
    PMEMORY_HEAP_TAG_STATISTIC Statistic;
    PRED_BLACK_TREE_NODE TreeNode;

    //
    // Search for the statistic on this allocation tag.
    //

    SearchValue.Tag = Tag;
    TreeNode = RtlRedBlackTreeSearch(&(Heap->TagStatistics.Tree),
                                     &(SearchValue.Node));

    //
    // If no statistic was found, create one.
    //

    if (TreeNode == NULL) {

        ASSERT(Allocate != FALSE);

        //
        // While this does recurse back to heap allocate, it only recurses once
        // because the statistics tag always has an entry in the tree, so this
        // specific code path doesn't get hit again.
        //

        Statistic = RtlHeapAllocate(Heap,
                                    sizeof(MEMORY_HEAP_TAG_STATISTIC),
                                    MEMORY_HEAP_STATISTICS_TAG);

        if (Statistic == NULL) {
            return;
        }

        RtlZeroMemory(Statistic, sizeof(MEMORY_HEAP_TAG_STATISTIC));
        Statistic->Tag = Tag;
        RtlRedBlackTreeInsert(&(Heap->TagStatistics.Tree), &(Statistic->Node));

        //
        // A new tag was found, increment the total count in the heap.
        //

        Heap->TagStatistics.TagCount += 1;

    } else {
        Statistic = RED_BLACK_TREE_VALUE(TreeNode,
                                         MEMORY_HEAP_TAG_STATISTIC,
                                         Node);
    }

    //
    // Update the statistics for a new allocation by this tag.
    //

    if (Allocate != FALSE) {
        if (AllocationSize > Statistic->LargestAllocation) {
            Statistic->LargestAllocation = AllocationSize;
        }

        Statistic->ActiveSize += AllocationSize;
        if (Statistic->ActiveSize > Statistic->LargestActiveSize) {
            Statistic->LargestActiveSize = Statistic->ActiveSize;
        }

        Statistic->LifetimeAllocationSize += AllocationSize;
        Statistic->ActiveAllocationCount += 1;
        if (Statistic->ActiveAllocationCount >
            Statistic->LargestActiveAllocationCount) {

            Statistic->LargestActiveAllocationCount =
                                              Statistic->ActiveAllocationCount;
        }

    //
    // Update the statistics for a free by this tag.
    //

    } else {

        ASSERT(Statistic->ActiveSize >= AllocationSize);
        ASSERT(Statistic->ActiveAllocationCount != 0);

        Statistic->ActiveSize -= AllocationSize;
        Statistic->ActiveAllocationCount -= 1;
    }

    return;
}

VOID
RtlpPrintMemoryHeapTagStatistic (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each node in the tree (via an in order
    traversal). It prints one heap statistic line.

Arguments:

    Tree - Supplies a pointer to the tree being enumerated.

    Node - Supplies a pointer to the node.

    Level - Supplies the depth into the tree that this node exists at. 0 is
        the root.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested. This parameter is not used.

Return Value:

    None.

--*/

{

    PMEMORY_HEAP_TAG_STATISTIC Statistic;

    Statistic = RED_BLACK_TREE_VALUE(Node, MEMORY_HEAP_TAG_STATISTIC, Node);
    RtlDebugPrint("%c%c%c%c 0x%8x %16I64d %16I64d %8d %8d %16I64d\n",
                  (UCHAR)(Statistic->Tag),
                  (UCHAR)(Statistic->Tag >> 8),
                  (UCHAR)(Statistic->Tag >> 16),
                  (UCHAR)(Statistic->Tag >> 24),
                  Statistic->LargestAllocation,
                  Statistic->ActiveSize,
                  Statistic->LargestActiveSize,
                  Statistic->ActiveAllocationCount,
                  Statistic->LargestActiveAllocationCount,
                  Statistic->LifetimeAllocationSize);

    return;
}

