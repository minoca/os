/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    heapprof.c

Abstract:

    This module implements Minoca specific heap profiling support.

Author:

    Evan Green 2-Mar-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"
#include <minoca/debug/spproto.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context used to collect heap statistics when
    iterating through a heap's tree of heap tag statistics.

Members:

    Buffer - Supplies a pointer to a buffer that is used to collect the heap's
        statistics for the profiler.

    BufferSize - Supplies the size of the buffer, in bytes.

--*/

typedef struct _HEAP_PROFILER_CONTEXT {
    PVOID Buffer;
    UINTN BufferSize;
} HEAP_PROFILER_CONTEXT, *PHEAP_PROFILER_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
RtlpGetProfilerMemoryHeapTagStatistic (
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
RtlHeapProfilerGetStatistics (
    PMEMORY_HEAP Heap,
    PVOID Buffer,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine fills the given buffer with the current heap statistics.

Arguments:

    Heap - Supplies a pointer to the heap.

    Buffer - Supplies the buffer to fill with heap statistics.

    BufferSize - Supplies the size of the buffer.

Return Value:

    None.

--*/

{

    HEAP_PROFILER_CONTEXT ProfilerContext;
    PPROFILER_MEMORY_POOL ProfilerHeap;

    //
    // Cast the start of the buffer as a pointer to a profiler heap.
    //

    ASSERT(BufferSize > sizeof(PROFILER_MEMORY_POOL));

    ProfilerHeap = (PPROFILER_MEMORY_POOL)Buffer;

    //
    // Re-package the basic heap statistics into the profiler's format, writing
    // to the buffer.
    //

    ProfilerHeap->Magic = PROFILER_POOL_MAGIC;
    ProfilerHeap->TagCount = Heap->TagStatistics.TagCount;
    ProfilerHeap->TotalPoolSize = Heap->Statistics.TotalHeapSize;
    ProfilerHeap->FreeListSize = Heap->Statistics.FreeListSize;
    ProfilerHeap->TotalAllocationCalls = Heap->Statistics.TotalAllocationCalls;
    ProfilerHeap->FailedAllocations = Heap->Statistics.FailedAllocations;
    ProfilerHeap->TotalFreeCalls = Heap->Statistics.TotalFreeCalls;

    //
    // Now get the statistics for each unique tag in the heap, filling in the
    // remainder of the supplied buffer.
    //

    ProfilerContext.Buffer = (PBYTE)Buffer + sizeof(PROFILER_MEMORY_POOL);
    ProfilerContext.BufferSize = BufferSize - sizeof(PROFILER_MEMORY_POOL);
    RtlRedBlackTreeIterate(&(Heap->TagStatistics.Tree),
                           RtlpGetProfilerMemoryHeapTagStatistic,
                           &ProfilerContext);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
RtlpGetProfilerMemoryHeapTagStatistic (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE Node,
    ULONG Level,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each node in the tree (via an in order
    traversal). It dumps the the statistics in the heap's profiler buffer.

Arguments:

    Tree - Supplies a pointer to the tree being enumerated.

    Node - Supplies a pointer to the node.

    Level - Supplies the depth into the tree that this node exists at. 0 is
        the root.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested. In this case it supplies a context
        to be filled with heap tag data.

Return Value:

    None.

--*/

{

    ULONG CopySize;
    PHEAP_PROFILER_CONTEXT ProfilerContext;
    PPROFILER_MEMORY_POOL_TAG_STATISTIC ProfilerStatistic;
    PMEMORY_HEAP_TAG_STATISTIC TagStatistic;

    ASSERT(Context != NULL);

    ProfilerContext = (PHEAP_PROFILER_CONTEXT)Context;
    CopySize = sizeof(PROFILER_MEMORY_POOL_TAG_STATISTIC);

    //
    // Cast the start of the heap's profiler buffer as a pointer to a profiler
    // tag statistic.
    //

    ASSERT(ProfilerContext->Buffer != NULL);
    ASSERT(ProfilerContext->BufferSize >= CopySize);

    ProfilerStatistic =
                  (PPROFILER_MEMORY_POOL_TAG_STATISTIC)ProfilerContext->Buffer;

    //
    // Convert the memory heap tag statistics to that of the profiler.
    //

    TagStatistic = RED_BLACK_TREE_VALUE(Node, MEMORY_HEAP_TAG_STATISTIC, Node);
    ProfilerStatistic->Tag = TagStatistic->Tag;
    ProfilerStatistic->LargestAllocation = TagStatistic->LargestAllocation;
    ProfilerStatistic->ActiveSize = TagStatistic->ActiveSize;
    ProfilerStatistic->LargestActiveSize = TagStatistic->LargestActiveSize;
    ProfilerStatistic->LifetimeAllocationSize =
                                          TagStatistic->LifetimeAllocationSize;

    ProfilerStatistic->ActiveAllocationCount =
                                           TagStatistic->ActiveAllocationCount;

    ProfilerStatistic->LargestActiveAllocationCount =
                                    TagStatistic->LargestActiveAllocationCount;

    //
    // Move the profiler buffer forward and decrease the size.
    //

    ProfilerContext->Buffer = (PBYTE)ProfilerContext->Buffer + CopySize;
    ProfilerContext->BufferSize -= CopySize;
    return;
}

