/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    heaptest.c

Abstract:

    This module implements the tests for the runtime library heap.

Author:

    Evan Green 30-Sep-2013

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#define TEST_HEAP_ALLOCATION_COUNT 5000
#define TEST_HEAP_MAX_ALLOCATION_SIZE 0x1800
#define TEST_HEAP_ITERATIONS 20000
#define TEST_HEAP_MAX_ALIGNMENT 0x100000

#define TEST_HEAP_TAG 0x74736554

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
TestExpandUpperHeap (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

BOOL
TestContractUpperHeap (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    );

PVOID
TestExpandLowerHeap (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

BOOL
TestContractLowerHeap (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    );

VOID
TestHeapHandleCorruption (
    PMEMORY_HEAP Heap,
    HEAP_CORRUPTION_CODE Code,
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// There are two heaps. The upper heap is the one under test. It gets its
// memory from a lower heap. This ensures the upper heap keeps track of its
// extents correctly. The lower heap is serviced by malloc and free.
//

MEMORY_HEAP TestUpperHeap;
MEMORY_HEAP TestLowerHeap;
ULONG TestHeapCorruptions = 0;

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestHeaps (
    BOOL Quiet
    )

/*++

Routine Description:

    This routine tests memory heaps.

Arguments:

    Quiet - Supplies a boolean indicating whether the test should be run
        without printouts (TRUE) or with debug output (FALSE).

Return Value:

    Returns the number of tests that failed.

--*/

{

    ULONG Alignment;
    PVOID *Allocations;
    ULONG Failures;
    ULONG Flags;
    ULONG Index;
    ULONG Iteration;
    PVOID OriginalAllocation;
    ULONG Size;
    KSTATUS Status;

    Failures = 0;

    //
    // Fire up the heaps.
    //

    Flags = MEMORY_HEAP_FLAG_PERIODIC_VALIDATION;
    RtlHeapInitialize(&TestLowerHeap,
                      TestExpandLowerHeap,
                      TestContractLowerHeap,
                      TestHeapHandleCorruption,
                      0x1000,
                      0x1000,
                      0xAAAAAAAA,
                      Flags);

    Flags = MEMORY_HEAP_FLAG_PERIODIC_VALIDATION |
            MEMORY_HEAP_FLAG_NO_PARTIAL_FREES;

    RtlHeapInitialize(&TestUpperHeap,
                      TestExpandUpperHeap,
                      TestContractUpperHeap,
                      TestHeapHandleCorruption,
                      0x1000,
                      0x1000,
                      0xBBBBBBBB,
                      Flags);

    TestUpperHeap.DirectAllocationThreshold =
                                         TEST_HEAP_MAX_ALLOCATION_SIZE - 0x100;

    //
    // Allocate space for the array of allocations.
    //

    Allocations = RtlHeapAllocate(&TestUpperHeap,
                                  TEST_HEAP_ALLOCATION_COUNT * sizeof(PVOID),
                                  TEST_HEAP_TAG);

    if (Allocations == NULL) {
        Failures += 1;
    }

    memset(Allocations, 0, TEST_HEAP_ALLOCATION_COUNT * sizeof(PVOID));
    for (Iteration = 0; Iteration < TEST_HEAP_ITERATIONS; Iteration += 1) {
        Index = rand() % TEST_HEAP_ALLOCATION_COUNT;
        Size = rand() % TEST_HEAP_MAX_ALLOCATION_SIZE;
        if (((Size & 0x1) != 0) || (Allocations[Index] == NULL)) {
            if ((Size & 0x2) != 0) {
                Allocations[Index] = RtlHeapReallocate(&TestUpperHeap,
                                                       Allocations[Index],
                                                       Size,
                                                       TEST_HEAP_TAG);

            } else {
                OriginalAllocation = Allocations[Index];
                Alignment = rand() % TEST_HEAP_MAX_ALIGNMENT;
                if ((Alignment & 0x1) != 0) {
                    Allocations[Index] = RtlHeapAllocate(&TestUpperHeap,
                                                         Size,
                                                         TEST_HEAP_TAG);

                } else {
                    Status = RtlHeapAlignedAllocate(&TestUpperHeap,
                                                    &(Allocations[Index]),
                                                    Alignment,
                                                    Size,
                                                    TEST_HEAP_TAG);

                    if (!KSUCCESS(Status)) {
                        printf("Aligned heap allocation failure: Status %d, "
                               "Alignment 0x%x, Size 0x%x\n",
                               Status,
                               Alignment,
                               Size);
                    }
                }

                if (OriginalAllocation != NULL) {
                    RtlHeapFree(&TestUpperHeap, OriginalAllocation);
                }
            }

            if (Allocations[Index] == NULL) {
                printf("Heap allocation failure: %x\n", Size);
                Failures += 1;

            } else {
                if (((UINTN)(Allocations[Index]) & 0x7) != 0) {
                    printf("Error: Heap returned unaligned allocation %p\n",
                           Allocations[Index]);

                    Failures += 1;
                }

                memset(Allocations[Index], 0xAB, Size);
            }

        } else {
            RtlHeapFree(&TestUpperHeap, Allocations[Index]);
            Allocations[Index] = NULL;
        }

        RtlValidateHeap(&TestUpperHeap, NULL);
    }

    //
    // Free everything.
    //

    for (Index = 0; Index < TEST_HEAP_ALLOCATION_COUNT; Index += 1) {
        if (Allocations[Index] != NULL) {
            RtlHeapFree(&TestUpperHeap, Allocations[Index]);
            Allocations[Index] = NULL;
        }
    }

    RtlHeapFree(&TestUpperHeap, Allocations);
    RtlValidateHeap(&TestUpperHeap, NULL);

    //
    // Make sure there are no more allocations on the upper heap.
    //

    if ((TestUpperHeap.Statistics.DirectAllocationSize != 0) ||
        (TestUpperHeap.Statistics.Allocations != 0) ||
        (TestUpperHeap.Statistics.TotalAllocationCalls !=
         TestUpperHeap.Statistics.TotalFreeCalls)) {

        printf("Error: Empty upper heap still has %lu allocations, %lu "
               "direct allocations, or %lu != %lu alloc/free calls.\n",
               TestUpperHeap.Statistics.Allocations,
               TestUpperHeap.Statistics.DirectAllocationSize,
               TestUpperHeap.Statistics.TotalAllocationCalls,
               TestUpperHeap.Statistics.TotalFreeCalls);

        Failures += 1;
    }

    //
    // Destroy the heap and make sure there are no more allocations on the
    // lower heap.
    //

    RtlValidateHeap(&TestLowerHeap, NULL);
    RtlHeapDestroy(&TestUpperHeap);
    RtlValidateHeap(&TestLowerHeap, NULL);
    if ((TestLowerHeap.Statistics.DirectAllocationSize != 0) ||
        (TestLowerHeap.Statistics.Allocations != 0) ||
        (TestLowerHeap.Statistics.TotalAllocationCalls !=
         TestLowerHeap.Statistics.TotalFreeCalls)) {

        printf("Error: Empty lower heap still has %lu allocations, %lu "
               "direct allocations, or %lu != %lu alloc/free calls.\n",
               TestLowerHeap.Statistics.Allocations,
               TestLowerHeap.Statistics.DirectAllocationSize,
               TestLowerHeap.Statistics.TotalAllocationCalls,
               TestLowerHeap.Statistics.TotalFreeCalls);

        Failures += 1;
    }

    RtlHeapDestroy(&TestLowerHeap);
    Failures += TestHeapCorruptions;
    if (Failures != 0) {
        printf("%d failures in heap test.\n", Failures);
    }

    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
TestExpandUpperHeap (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine is called when the heap wants to expand and get more space.

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

    return RtlHeapAllocate(&TestLowerHeap, Size, Tag);
}

BOOL
TestContractUpperHeap (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    )

/*++

Routine Description:

    This routine is called when the heap wants to release space it had
    previously been allocated.

Arguments:

    Heap - Supplies a pointer to the heap the memory was originally allocated
        from.

    Memory - Supplies the allocation returned by the allocation routine.

    Size - Supplies the size to free.

Return Value:

    TRUE if the memory was successfully freed.

    FALSE if the memory could not be freed at this time.

--*/

{

    RtlHeapFree(&TestLowerHeap, Memory);
    return TRUE;
}

PVOID
TestExpandLowerHeap (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine is called when the heap wants to expand and get more space.

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

    PVOID Allocation;

    Allocation = malloc(Size);
    return Allocation;
}

BOOL
TestContractLowerHeap (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    )

/*++

Routine Description:

    This routine is called when the heap wants to release space it had
    previously been allocated.

Arguments:

    Heap - Supplies a pointer to the heap the memory was originally allocated
        from.

    Memory - Supplies the allocation returned by the allocation routine.

    Size - Supplies the size to free.

Return Value:

    TRUE if the memory was successfully freed.

    FALSE if the memory could not be freed at this time.

--*/

{

    //
    // The lower heap is set to allow the heap to trim, in order to test heap
    // trimming. As such, the heap may free in the middle of a previous
    // allocation (or really only at the end). Unless data structures are
    // kept to know which is the middle and which is the end, this routine
    // cannot know whether or not to call free. So it just leaks instead.
    //

    return TRUE;
}

VOID
TestHeapHandleCorruption (
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

    fprintf(stderr,
            "Error: Heap corruption in heap %p, Code %d, Parameter %p\n",
            Heap,
            Code,
            Parameter);

    TestHeapCorruptions += 1;
    return;
}

