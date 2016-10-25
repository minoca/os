/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tblock.c

Abstract:

    This module implements the kernel block allocator tests.

Author:

    Chris Stevens 14-Nov-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "ktestdrv.h"
#include "testsup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define KTEST_BLOCK_DEFAULT_ITERATIONS 500000
#define KTEST_BLOCK_DEFAULT_THREAD_COUNT 5
#define KTEST_BLOCK_DEFAULT_ALLOCATION_COUNT 500
#define KTEST_BLOCK_DEFAULT_BLOCK_SIZE 1024
#define KTEST_BLOCK_DEFAULT_INITIAL_CAPACITY 100
#define KTEST_BLOCK_DEFAULT_ALIGNMENT 1

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KTestBlockStressRoutine (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KTestBlockStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    )

/*++

Routine Description:

    This routine starts a new invocation of the block allocator stress tests.

Arguments:

    Command - Supplies a pointer to the start command.

    Test - Supplies a pointer to the active test structure to initialize.

Return Value:

    Status code.

--*/

{

    PKTEST_PARAMETERS Parameters;
    KSTATUS Status;
    ULONG ThreadIndex;

    Parameters = &(Test->Parameters);
    RtlCopyMemory(Parameters, &(Command->Parameters), sizeof(KTEST_PARAMETERS));
    if (Parameters->Iterations == 0) {
        Parameters->Iterations = KTEST_BLOCK_DEFAULT_ITERATIONS;
    }

    if (Parameters->Threads == 0) {
        Parameters->Threads = KTEST_BLOCK_DEFAULT_THREAD_COUNT;
    }

    if (Parameters->Parameters[0] == 0) {
        Parameters->Parameters[0] = KTEST_BLOCK_DEFAULT_ALLOCATION_COUNT;
    }

    if (Parameters->Parameters[1] == 0) {
        Parameters->Parameters[1] = KTEST_BLOCK_DEFAULT_BLOCK_SIZE;
    }

    if (Parameters->Parameters[2] == 0) {
        Parameters->Parameters[2] = KTEST_BLOCK_DEFAULT_INITIAL_CAPACITY;
    }

    if (Parameters->Parameters[3] == 0) {
        Parameters->Parameters[3] = KTEST_BLOCK_DEFAULT_ALIGNMENT;
    }

    Test->Total = Test->Parameters.Iterations;
    Test->Results.Status = STATUS_SUCCESS;
    Test->Results.Failures = 0;
    for (ThreadIndex = 0;
         ThreadIndex < Test->Parameters.Threads;
         ThreadIndex += 1) {

        Status = PsCreateKernelThread(KTestBlockStressRoutine,
                                      Test,
                                      "KTestBlockStressRoutine");

        if (!KSUCCESS(Status)) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto PoolStressStartEnd;
        }
    }

    Status = STATUS_SUCCESS;

PoolStressStartEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KTestBlockStressRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the block allocator stress test.

Arguments:

    Parameter - Supplies a pointer to the thread parameter, which in this
        case is a pointer to the active test structure.

Return Value:

    None.

--*/

{

    UINTN Alignment;
    UINTN AllocatedMemory;
    PUCHAR Allocation;
    UINTN AllocationCount;
    PVOID *Array;
    UINTN ArraySize;
    PBLOCK_ALLOCATOR BlockAllocator;
    UINTN BlockSize;
    UINTN ExpansionCount;
    ULONG Failures;
    UINTN Flags;
    UINTN Index;
    PKTEST_ACTIVE_TEST Information;
    UINTN Iteration;
    UINTN MaxAllocatedMemory;
    UINTN MaxAllocationCount;
    PKTEST_PARAMETERS Parameters;
    PPHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS PhysicalAddressBuffer;
    ULONG Random;
    KSTATUS Status;
    ULONG ThreadNumber;
    UINTN WriteIndex;

    AllocatedMemory = 0;
    AllocationCount = 0;
    Array = NULL;
    Failures = 0;
    MaxAllocatedMemory = 0;
    MaxAllocationCount = 0;
    Information = Parameter;
    Parameters = &(Information->Parameters);
    ArraySize = Parameters->Parameters[0];
    BlockSize = Parameters->Parameters[1];
    ExpansionCount = Parameters->Parameters[2];
    Alignment = Parameters->Parameters[3];
    ThreadNumber = RtlAtomicAdd32(&(Information->ThreadsStarted), 1);
    if (BlockSize < sizeof(UINTN)) {
        BlockSize = sizeof(UINTN);
    }

    //
    // Create the block allocator.
    //

    Flags = 0;
    PhysicalAddress = NULL;
    if (Parameters->TestType == KTestNonPagedBlockStress) {
        Flags |= BLOCK_ALLOCATOR_FLAG_NON_PAGED;
        Flags |= BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;
        PhysicalAddress = &PhysicalAddressBuffer;
    }

    BlockAllocator = MmCreateBlockAllocator(BlockSize,
                                            Alignment,
                                            ExpansionCount,
                                            Flags,
                                            KTEST_ALLOCATION_TAG);

    if (BlockAllocator == NULL) {
        Failures += 1;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto BlockStressRoutineEnd;
    }

    //
    // Create the array that holds the allocations.
    //

    Array = MmAllocatePagedPool(ArraySize * sizeof(PVOID),
                                KTEST_ALLOCATION_TAG);

    if (Array == NULL) {
        Failures += 1;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto BlockStressRoutineEnd;
    }

    RtlZeroMemory(Array, ArraySize * sizeof(PVOID));

    //
    // Loop simply making and freeing allocations randomly.
    //

    for (Iteration = 0; Iteration < Parameters->Iterations; Iteration += 1) {
        if (Information->Cancel != FALSE) {
            Status = STATUS_SUCCESS;
            goto BlockStressRoutineEnd;
        }

        Index = KTestGetRandomValue() % ArraySize;
        if (ThreadNumber == 0) {
            Information->Progress += 1;
        }

        //
        // If the lowest bit is set, attempt to allocate. Otherwise, attempt to
        // free. If there's nothing to free, allocate.
        //

        Random = KTestGetRandomValue();
        if (Array[Index] == NULL) {
            Random |= 1;
        }

        if ((Random & 1) != 0) {
            Allocation = MmAllocateBlock(BlockAllocator, PhysicalAddress);
            if (Allocation == NULL) {
                Failures += 1;
                continue;
            }

            if (IS_ALIGNED((UINTN)Allocation, Alignment) == FALSE) {
                RtlDebugPrint("KTEST: Block allocator return unaligned "
                              "block: block virtual address 0x%0x, "
                              "alignment: 0x%x\n",
                              Allocation,
                              Alignment);

                Failures += 1;
                Status = STATUS_UNSUCCESSFUL;
                goto BlockStressRoutineEnd;
            }

            if ((PhysicalAddress != NULL) &&
                (IS_ALIGNED(*PhysicalAddress, Alignment) == FALSE)) {

                RtlDebugPrint("KTEST: Block allocator return unaligned "
                              "block: block physical address 0x%I64x, "
                              "alignment: 0x%x\n",
                              *PhysicalAddress,
                              Alignment);

                Failures += 1;
                Status = STATUS_UNSUCCESSFUL;
                goto BlockStressRoutineEnd;
            }

            AllocatedMemory += BlockSize;

            //
            // Initialize the memory to something. Put the size at the
            // beginning.
            //

            ASSERT(BlockSize >= sizeof(UINTN));

            *((PUINTN)Allocation) = Random;
            for (WriteIndex = sizeof(UINTN);
                 WriteIndex < BlockSize;
                 WriteIndex += 1) {

                Allocation[WriteIndex] = WriteIndex + 0x80;
            }

            //
            // Free the old array.
            //

            if (Array[Index] != NULL) {
                AllocatedMemory -= BlockSize;
                MmFreeBlock(BlockAllocator, Array[Index]);
                AllocationCount -= 1;
            }

            Array[Index] = Allocation;
            AllocationCount += 1;
            if (AllocationCount > MaxAllocationCount) {
                MaxAllocationCount = AllocationCount;
            }

            if (AllocatedMemory > MaxAllocatedMemory) {
                MaxAllocatedMemory = AllocatedMemory;
            }

        } else {
            AllocatedMemory -= BlockSize;
            AllocationCount -= 1;
            MmFreeBlock(BlockAllocator, Array[Index]);
            Array[Index] = NULL;
        }
    }

    Status = STATUS_SUCCESS;

BlockStressRoutineEnd:

    //
    // Clean up the block allocator.
    //

    if (BlockAllocator != NULL) {
        MmDestroyBlockAllocator(BlockAllocator);
    }

    if (Array != NULL) {
        MmFreePagedPool(Array);
    }

    //
    // Save the results.
    //

    if (!KSUCCESS(Status)) {
        Information->Results.Status = Status;
    }

    Information->Results.Failures += Failures;
    if (ThreadNumber == 0) {
        Information->Results.Results[0] = MaxAllocationCount;
        Information->Results.Results[1] = MaxAllocatedMemory;
    }

    RtlAtomicAdd32(&(Information->ThreadsFinished), 1);
    return;
}

