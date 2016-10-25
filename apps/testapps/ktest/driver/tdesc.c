/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tdesc.c

Abstract:

    This module implements the kernel memory descriptor tests.

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

#define KTEST_DESCRIPTOR_DEFAULT_ITERATIONS 100
#define KTEST_DESCRIPTOR_DEFAULT_THREAD_COUNT 5
#define KTEST_DESCRIPTOR_DEFAULT_BLOCK_SIZE 4096

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KTestDescriptorStressRoutine (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KTestDescriptorStressStart (
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
        Parameters->Iterations = KTEST_DESCRIPTOR_DEFAULT_ITERATIONS;
    }

    if (Parameters->Threads == 0) {
        Parameters->Threads = KTEST_DESCRIPTOR_DEFAULT_THREAD_COUNT;
    }

    if (Parameters->Parameters[0] == 0) {
        Parameters->Parameters[0] = KTEST_DESCRIPTOR_DEFAULT_BLOCK_SIZE;
    }

    Test->Total = Test->Parameters.Iterations;
    Test->Results.Status = STATUS_SUCCESS;
    Test->Results.Failures = 0;
    for (ThreadIndex = 0;
         ThreadIndex < Test->Parameters.Threads;
         ThreadIndex += 1) {

        Status = PsCreateKernelThread(KTestDescriptorStressRoutine,
                                      Test,
                                      "KTestDescriptorStressRoutine");

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
KTestDescriptorStressRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the memory descriptor stress test.

Arguments:

    Parameter - Supplies a pointer to the thread parameter, which in this
        case is a pointer to the active test structure.

Return Value:

    None.

--*/

{

    UINTN BlockSize;
    ULONG Failures;
    ULONG Flags;
    UINTN Index;
    PKTEST_ACTIVE_TEST Information;
    UINTN IoBufferCount;
    PIO_BUFFER *IoBuffers;
    UINTN Iteration;
    PBLOCK_ALLOCATOR NonPagedAllocator;
    PKTEST_PARAMETERS Parameters;
    KSTATUS Status;
    ULONG ThreadNumber;

    Failures = 0;
    Information = Parameter;
    IoBufferCount = 0;
    IoBuffers = NULL;
    NonPagedAllocator = NULL;
    Parameters = &(Information->Parameters);
    BlockSize = Parameters->Parameters[0];
    ThreadNumber = RtlAtomicAdd32(&(Information->ThreadsStarted), 1);

    //
    // Create a block allocator.
    //

    Flags = BLOCK_ALLOCATOR_FLAG_NON_PAGED |
            BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;

    NonPagedAllocator = MmCreateBlockAllocator(BlockSize,
                                               1,
                                               1,
                                               Flags,
                                               KTEST_ALLOCATION_TAG);

    if (NonPagedAllocator == NULL) {
        Failures += 1;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DescriptorStressRoutineEnd;
    }

    //
    // Create an array to hold I/O buffers.
    //

    IoBuffers = MmAllocatePagedPool(sizeof(PIO_BUFFER) * Parameters->Iterations,
                                    KTEST_ALLOCATION_TAG);

    if (IoBuffers == NULL) {
        Failures += 1;
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DescriptorStressRoutineEnd;
    }

    //
    // Loop simply alternating block allocations and I/O buffer allocations.
    // The goal of this is to have each allocation expand the block allocators
    // reserves, triggering a new allocation, and interleave those expansions
    // with I/O buffer allocations. This will force interleaving amongst the
    // virtual descriptors in the kernel's memory map, driving the descriptor
    // count up.
    //

    for (Iteration = 0; Iteration < Parameters->Iterations; Iteration += 1) {
        if (Information->Cancel != FALSE) {
            Status = STATUS_SUCCESS;
            goto DescriptorStressRoutineEnd;
        }

        if (ThreadNumber == 0) {
            Information->Progress += 1;
        }

        MmAllocateBlock(NonPagedAllocator, NULL);
        IoBuffers[Iteration] = MmAllocateNonPagedIoBuffer(0,
                                                          MAX_ULONGLONG,
                                                          BlockSize,
                                                          BlockSize,
                                                          0);

        if (IoBuffers[Iteration] == NULL) {
            Failures += 1;
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto DescriptorStressRoutineEnd;
        }

        IoBufferCount += 1;
    }

    Status = STATUS_SUCCESS;

DescriptorStressRoutineEnd:

    //
    // Destroy the block allocator.
    //

    if (NonPagedAllocator != NULL) {
        MmDestroyBlockAllocator(NonPagedAllocator);
    }

    //
    // Destroy the I/O buffers.
    //

    for (Index = 0; Index < IoBufferCount; Index += 1) {
        MmFreeIoBuffer(IoBuffers[Index]);
    }

    //
    // Save the results.
    //

    if (!KSUCCESS(Status)) {
        Information->Results.Status = Status;
    }

    Information->Results.Failures += Failures;
    RtlAtomicAdd32(&(Information->ThreadsFinished), 1);
    return;
}

