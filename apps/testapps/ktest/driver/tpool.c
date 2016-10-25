/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tpool.c

Abstract:

    This module implements the kernel pool tests.

Author:

    Evan Green 5-Nov-2013

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

#define KTEST_POOL_DEFAULT_ITERATIONS 500000
#define KTEST_POOL_DEFAULT_THREAD_COUNT 5
#define KTEST_POOL_DEFAULT_ALLOCATION_COUNT 500
#define KTEST_POOL_DEFAULT_ALLOCATION_SIZE 4096

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KTestPoolStressRoutine (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KTestPoolStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    )

/*++

Routine Description:

    This routine starts a new invocation of the paged and non-paged pool stress
    test.

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
        Parameters->Iterations = KTEST_POOL_DEFAULT_ITERATIONS;
    }

    if (Parameters->Threads == 0) {
        Parameters->Threads = KTEST_POOL_DEFAULT_THREAD_COUNT;
    }

    if (Parameters->Parameters[0] == 0) {
        Parameters->Parameters[0] = KTEST_POOL_DEFAULT_ALLOCATION_COUNT;
    }

    if (Parameters->Parameters[1] == 0) {
        Parameters->Parameters[1] = KTEST_POOL_DEFAULT_ALLOCATION_SIZE;
    }

    Test->Total = Test->Parameters.Iterations;
    Test->Results.Status = STATUS_SUCCESS;
    Test->Results.Failures = 0;
    for (ThreadIndex = 0;
         ThreadIndex < Test->Parameters.Threads;
         ThreadIndex += 1) {

        Status = PsCreateKernelThread(KTestPoolStressRoutine,
                                      Test,
                                      "KTestPoolStressRoutine");

        if (!KSUCCESS(Status)) {
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
KTestPoolStressRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the paged pool stress test.

Arguments:

    Parameter - Supplies a pointer to the thread parameter, which in this
        case is a pointer to the active test structure.

Return Value:

    None.

--*/

{

    UINTN AllocatedMemory;
    PUCHAR Allocation;
    UINTN AllocationCount;
    PVOID *Array;
    UINTN ArraySize;
    ULONG Failures;
    UINTN Index;
    PKTEST_ACTIVE_TEST Information;
    UINTN Iteration;
    UINTN MaxAllocatedMemory;
    UINTN MaxAllocationCount;
    UINTN MaxAllocationSize;
    UINTN MaxAllocationSizeSeen;
    PKTEST_PARAMETERS Parameters;
    POOL_TYPE PoolType;
    ULONG Random;
    KSTATUS Status;
    ULONG ThreadNumber;
    UINTN WriteIndex;

    AllocationCount = 0;
    Failures = 0;
    MaxAllocatedMemory = 0;
    MaxAllocationCount = 0;
    MaxAllocationSizeSeen = 0;
    Information = Parameter;
    Parameters = &(Information->Parameters);
    ArraySize = Parameters->Parameters[0];
    MaxAllocationSize = Parameters->Parameters[1];
    ThreadNumber = RtlAtomicAdd32(&(Information->ThreadsStarted), 1);
    if (Parameters->TestType == KTestPagedPoolStress) {
        PoolType = PoolTypePaged;

    } else {

        ASSERT(Parameters->TestType == KTestNonPagedPoolStress);

        PoolType = PoolTypeNonPaged;
    }

    //
    // Create the array that holds the other allocations.
    //

    Array = MmAllocatePool(PoolType,
                           ArraySize * sizeof(PVOID),
                           KTEST_ALLOCATION_TAG);

    if (Array == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PoolStressRoutineEnd;
    }

    RtlZeroMemory(Array, ArraySize * sizeof(PVOID));
    AllocatedMemory = ArraySize * sizeof(PVOID);
    MaxAllocatedMemory = AllocatedMemory;

    //
    // Loop simply making and freeing allocations randomly.
    //

    for (Iteration = 0; Iteration < Parameters->Iterations; Iteration += 1) {
        if (Information->Cancel != FALSE) {
            Status = STATUS_SUCCESS;
            goto PoolStressRoutineEnd;
        }

        Random = (KTestGetRandomValue() % MaxAllocationSize) + sizeof(UINTN);
        Index = KTestGetRandomValue() % ArraySize;
        if (ThreadNumber == 0) {
            Information->Progress += 1;
        }

        //
        // If the lowest bit is set, attempt to allocate. Otherwise, attempt to
        // free. If there's nothing to free, allocate.
        //

        if (Array[Index] == NULL) {
            Random |= 1;
        }

        if ((Random & 1) != 0) {
            Allocation = MmAllocatePool(PoolType, Random, KTEST_ALLOCATION_TAG);
            if (Allocation == NULL) {
                Failures += 1;
                continue;
            }

            AllocatedMemory += Random;

            //
            // Initialize the memory to something. Put the size at the
            // beginning.
            //

            *((PUINTN)Allocation) = Random;
            for (WriteIndex = sizeof(UINTN);
                 WriteIndex < Random;
                 WriteIndex += 1) {

                Allocation[WriteIndex] = WriteIndex + 0x80;
            }

            //
            // Free the old array.
            //

            if (Array[Index] != NULL) {
                AllocatedMemory -= *((PUINTN)(Array[Index]));
                MmFreePool(PoolType, Array[Index]);
                AllocationCount -= 1;
            }

            Array[Index] = Allocation;
            AllocationCount += 1;
            if (AllocationCount > MaxAllocationCount) {
                MaxAllocationCount = AllocationCount;
            }

            if (Random > MaxAllocationSizeSeen) {
                MaxAllocationSizeSeen = Random;
            }

            if (AllocatedMemory > MaxAllocatedMemory) {
                MaxAllocatedMemory = AllocatedMemory;
            }

        } else {
            AllocatedMemory -= *((PUINTN)(Array[Index]));
            AllocationCount -= 1;
            MmFreePool(PoolType, Array[Index]);
            Array[Index] = NULL;
        }
    }

    Status = STATUS_SUCCESS;

PoolStressRoutineEnd:

    //
    // Clean up the array.
    //

    if (Array != NULL) {
        for (Index = 0; Index < ArraySize; Index += 1) {
            if (Array[Index] != NULL) {
                MmFreePool(PoolType, Array[Index]);
            }
        }

        MmFreePool(PoolType, Array);
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
        Information->Results.Results[1] = MaxAllocationSizeSeen;
        Information->Results.Results[2] = MaxAllocatedMemory;
    }

    RtlAtomicAdd32(&(Information->ThreadsFinished), 1);
    return;
}

