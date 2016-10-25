/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testuva.c

Abstract:

    This module contains tests for the user VA memory accounting.

Author:

    Evan Green 6-Aug-2012

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "../mmp.h"
#include "testmm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define NON_PAGED_MEMORY 0x10
#define PAGED_MEMORY 0x10
#define MINIMUM_POOL_GROWTH (0x20 * 0x1000)
#define POOL_GRANULARITY 0x1000
#define TEST_TAG 0x74736554 // 'tseT'
#define TEST_ALLOCATION_COUNT 100
#define TEST_ALLOCATION_MAGIC 0x54534554 // 'TSET'

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
UvaTestAllocate (
    PKPROCESS Process,
    PVOID RequestedAddress,
    ULONG Size,
    BOOL ExpectedSuccess,
    PULONG Failures
    );

PVOID
TestExpandPool (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

BOOL
TestContractPool (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern MEMORY_HEAP MmPagedPool;
extern MEMORY_HEAP MmNonPagedPool;

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestUserVa (
    VOID
    )

/*++

Routine Description:

    This routine tests the user virtual allocator functionality.

Arguments:

    None.

Return Value:

    Returns the number of test failures.

--*/

{

    ADDRESS_SPACE AddressSpace;
    MEMORY_DESCRIPTOR Descriptor;
    ULONG EndAddress;
    ULONG Failures;
    PMEMORY_DESCRIPTOR FreeDescriptors;
    ULONG FreeDescriptorSize;
    ULONG HeapFlags;
    ULONG Index;
    ULONG PageShift;
    ULONG PageSize;
    PVOID PreviousAllocation;
    PVOID RawMemory;
    ULONG StartAddress;
    KSTATUS Status;
    PVOID TestAllocation;
    KPROCESS UserProcess;

    Failures = 0;
    PageShift = MmPageShift();
    PageSize = MmPageSize();

    //
    // Allocate some memory for the accounting descriptors.
    //

    FreeDescriptorSize = PageSize;
    FreeDescriptors = malloc(FreeDescriptorSize);
    if (FreeDescriptors == NULL) {
        printf("Infrastructure Error: Could not allocate memory from host OS "
               "for the allocator descriptors.\n");

        Failures += 1;
        goto TestUserVaEnd;
    }

    //
    // Allocate space for the accountant.
    //

    RawMemory = malloc(NON_PAGED_MEMORY << PageShift);
    if (RawMemory == NULL) {
        printf("Infrastructure error: Could not allocate memory from host OS "
               "to initialize the allocator.\n");

        Failures += 1;
        goto TestUserVaEnd;
    }

    //
    // Initialize the accountant for the non-paged pool.
    //

    Status = MmInitializeMemoryAccounting(&MmKernelVirtualSpace,
                                          MEMORY_ACCOUNTING_FLAG_SYSTEM);

    if (!KSUCCESS(Status)) {
        printf("Error: Unable to initialize memory accounting. Status: %d\n",
               Status);

        Failures += 1;
        goto TestUserVaEnd;
    }

    //
    // The system memory list is backed by physically allocated descriptors.
    // This test cannot perform physical allocations, so seed the memory
    // descriptor list with the allocated descriptors.
    //

    MmMdAddFreeDescriptorsToMdl(&(MmKernelVirtualSpace.Mdl),
                                FreeDescriptors,
                                FreeDescriptorSize);

    //
    // Make the accountant aware of the memory available. The purpose of doing
    // the allocation in two steps is to create a small hole the allocator
    // has to deal with. Ensure that the allocation is on page boundaries.
    //

    StartAddress = ALIGN_RANGE_UP((UINTN)RawMemory, PageSize);
    EndAddress = (UINTN)RawMemory + (NON_PAGED_MEMORY << PageShift);
    EndAddress = ALIGN_RANGE_DOWN(EndAddress, PageSize);
    MmMdInitDescriptor(&Descriptor, StartAddress, EndAddress, MemoryTypeFree);
    Status = MmpAddAccountingDescriptor(&MmKernelVirtualSpace, &Descriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to add accounting descriptor. Status: %d\n",
               Status);

        Failures += 1;
        goto TestUserVaEnd;
    }

    //
    // Bring up the non-paged pool, and swap out its accountant and memory type
    // before it attempts to do anything.
    //

    HeapFlags = MEMORY_HEAP_FLAG_PERIODIC_VALIDATION |
                MEMORY_HEAP_FLAG_NO_PARTIAL_FREES;

    RtlHeapInitialize(&MmNonPagedPool,
                      TestExpandPool,
                      TestContractPool,
                      NULL,
                      MINIMUM_POOL_GROWTH,
                      POOL_GRANULARITY,
                      0,
                      HeapFlags);

    //
    // Do a test allocation to make sure the non-paged pool works.
    //

    TestAllocation = MmAllocateNonPagedPool(PageSize, TEST_TAG);
    if (TestAllocation == NULL) {
        printf("Error: Unable to get non-paged pool up!\n");
        Failures += 1;
        goto TestUserVaEnd;

    } else {
        MmFreeNonPagedPool(TestAllocation);
    }

    //
    // Allocate space for the paged pool.
    //

    RawMemory = malloc(PAGED_MEMORY << PageShift);
    if (RawMemory == NULL) {
        printf("Infrastructure error: Could not allocate memory from host OS "
               "to initialize the allocator.\n");

        Failures += 1;
        goto TestUserVaEnd;
    }

    //
    // Make the accountant aware of the memory available.
    //

    StartAddress = ALIGN_RANGE_UP((UINTN)RawMemory, PageSize);
    EndAddress = (UINTN)RawMemory + (PAGED_MEMORY << PageShift);
    EndAddress = ALIGN_RANGE_DOWN(EndAddress, PageSize);
    MmMdInitDescriptor(&Descriptor, StartAddress, EndAddress, MemoryTypeFree);
    Status = MmpAddAccountingDescriptor(&MmKernelVirtualSpace, &Descriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to add accounting descriptor. Status: %d\n",
               Status);

        Failures += 1;
        goto TestUserVaEnd;
    }

    //
    // Initialize and test the paged pool.
    //

    HeapFlags = MEMORY_HEAP_FLAG_PERIODIC_VALIDATION |
                MEMORY_HEAP_FLAG_NO_PARTIAL_FREES;

    RtlHeapInitialize(&MmPagedPool,
                      TestExpandPool,
                      TestContractPool,
                      NULL,
                      MINIMUM_POOL_GROWTH,
                      POOL_GRANULARITY,
                      0,
                      HeapFlags);

    TestAllocation = MmAllocatePagedPool(PageSize, TEST_TAG);
    if (TestAllocation == NULL) {
        printf("Error: Unable to get paged pool up!\n");
        Failures += 1;
        goto TestUserVaEnd;

    } else {
        MmFreePagedPool(TestAllocation);
    }

    //
    // Create the User space accountant.
    //

    RtlZeroMemory(&UserProcess, sizeof(KPROCESS));
    RtlZeroMemory(&AddressSpace, sizeof(ADDRESS_SPACE));
    UserProcess.AddressSpace = &AddressSpace;
    INITIALIZE_LIST_HEAD(&(UserProcess.ImageListHead));
    INITIALIZE_LIST_HEAD(&(AddressSpace.SectionListHead));
    AddressSpace.Accountant = malloc(sizeof(MEMORY_ACCOUNTING));

    assert(AddressSpace.Accountant != NULL);

    Status = MmInitializeMemoryAccounting(AddressSpace.Accountant,
                                          MEMORY_ACCOUNTING_FLAG_NO_MAP);

    if (!KSUCCESS(Status)) {
        printf("Error: Unable to initialize User VA accountant. Status = %d.\n",
               Status);

        Failures += 1;
        goto TestUserVaEnd;
    }

    MmMdInitDescriptor(&Descriptor,
                       PageSize,
                       (UINTN)KERNEL_VA_START,
                       MemoryTypeFree);

    Status = MmpAddAccountingDescriptor(AddressSpace.Accountant,
                                        &Descriptor);

    if (!KSUCCESS(Status)) {
        printf("Error: Unable to add initial descriptor to user VA accountant. "
               "Status = %d.\n",
               Status);

        Failures += 1;
        goto TestUserVaEnd;
    }

    //
    // Attempt to allocate stuff from kernel space.
    //

    UvaTestAllocate(&UserProcess,
                    KERNEL_VA_START,
                    1,
                    FALSE,
                    &Failures);

    //
    // Attempt to allocate something that is in user space but spills into
    // kernel space.
    //

    UvaTestAllocate(&UserProcess,
                    (PVOID)((UINTN)KERNEL_VA_START - 0x1000),
                    0x2000,
                    FALSE,
                    &Failures);

    //
    // Attempt to allocate something that is in user space but overflows.
    //

    UvaTestAllocate(&UserProcess,
                    (PVOID)((UINTN)KERNEL_VA_START - 0x1000),
                    MAX_ULONG,
                    FALSE,
                    &Failures);

    //
    // Make a nice normal allocation.
    //

    UvaTestAllocate(&UserProcess,
                    (PVOID)0x10000,
                    0xF0000,
                    TRUE,
                    &Failures);

    //
    // Attempt to allocate that occupied space again.
    //

    UvaTestAllocate(&UserProcess, (PVOID)0x20000, 0x1000, FALSE, &Failures);
    UvaTestAllocate(&UserProcess, (PVOID)0x10000, 0xF0000, FALSE, &Failures);

    //
    // Free the normal allocation.
    //

    MmpFreeAccountingRange(&AddressSpace,
                           (PVOID)0x10000,
                           0xF0000,
                           TRUE,
                           0);

    //
    // Now attempt to reallocate that space.
    //

    UvaTestAllocate(&UserProcess, (PVOID)0x10000, 0xF0000, TRUE, &Failures);

    //
    // Free it again.
    //

    MmpFreeAccountingRange(&AddressSpace,
                           (PVOID)0x10000,
                           0xF0000,
                           TRUE,
                           0);

    //
    // Make an allocation from who-cares where, and then free it.
    //

    TestAllocation = UvaTestAllocate(&UserProcess,
                                     NULL,
                                     0x20000,
                                     TRUE,
                                     &Failures);

    if (TestAllocation != NULL) {
        Status = MmpFreeAccountingRange(&AddressSpace,
                                        TestAllocation,
                                        0x20000,
                                        TRUE,
                                        0);

        if (!KSUCCESS(Status)) {
            printf("Error freeing allocation %p. Status = %d.\n",
                   TestAllocation,
                   Status);

            Failures += 1;
        }
    }

    //
    // Make a bunch of outstanding allocations, and then destroy the allocator.
    //

    PreviousAllocation = NULL;
    for (Index = 1; Index < 200; Index += 1) {
        TestAllocation = UvaTestAllocate(&UserProcess,
                                         NULL,
                                         Index * PageSize,
                                         TRUE,
                                         &Failures);

        if (PreviousAllocation != NULL) {
            Status = MmpFreeAccountingRange(&AddressSpace,
                                            PreviousAllocation,
                                            (Index - 1) * PageSize,
                                            TRUE,
                                            0);

            if (!KSUCCESS(Status)) {
                printf("Error freeing allocation %p. Status = %d.\n",
                       PreviousAllocation,
                       Status);

                Failures += 1;
            }
        }

        PreviousAllocation = TestAllocation;
    }

    //
    // Free the last allocation.
    //

    if (PreviousAllocation != NULL) {
        Status = MmpFreeAccountingRange(&AddressSpace,
                                        PreviousAllocation,
                                        (Index - 1) * PageSize,
                                        TRUE,
                                        0);

        if (!KSUCCESS(Status)) {
            printf("Error freeing allocation %p. Status = %d.\n",
                   PreviousAllocation,
                   Status);

            Failures += 1;
        }
    }

    MmDestroyMemoryAccounting(AddressSpace.Accountant);
    free(AddressSpace.Accountant);

    //
    // If the outstanding allocations are not from the pool tags, then
    // something was not properly released. The number of allocations should be
    // one less than the number of tags.
    //

    if (MmNonPagedPool.Statistics.Allocations !=
                                 (MmNonPagedPool.TagStatistics.TagCount - 1)) {

        printf("Error: %ld outstanding non-paged pool allocations.\n",
               MmNonPagedPool.Statistics.Allocations);

        Failures += 1;
    }

TestUserVaEnd:
    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
UvaTestAllocate (
    PKPROCESS Process,
    PVOID RequestedAddress,
    ULONG Size,
    BOOL ExpectedSuccess,
    PULONG Failures
    )

/*++

Routine Description:

    This routine attempts to allocate memory from the given pool.

Arguments:

    Process - Supplies a pointer to the process containing the memory accounting
        structure.

    RequestedAddress - Supplies the requested address to pass into the
        allocator.

    Size - Supplies the size of the allocation to make, in bytes.

    ExpectedSuccess - Supplies TRUE if the allocation should succeed, or FALSE
        if the allocation was expected to fail.

    Failures - Supplies a pointer to a variable to increment if the allocation
        success or failure is unexpected.

Return Value:

    Returns the result of the allocation.

--*/

{

    PVOID Allocation;
    KSTATUS Status;
    ALLOCATION_STRATEGY Strategy;
    BOOL Valid;
    VM_ALLOCATION_PARAMETERS VaRequest;

    Strategy = AllocationStrategyAnyAddress;
    if (RequestedAddress != NULL) {
        Strategy = AllocationStrategyFixedAddress;
    }

    VaRequest.Address = RequestedAddress;
    VaRequest.Size = Size;
    VaRequest.Alignment = 0;
    VaRequest.Min = 0;
    VaRequest.Max = MAX_ADDRESS;
    VaRequest.MemoryType = MemoryTypeReserved;
    VaRequest.Strategy = Strategy;
    Status = MmpAllocateAddressRange(Process->AddressSpace->Accountant,
                                     &VaRequest,
                                     TRUE);

    Allocation = VaRequest.Address;
    if ((!KSUCCESS(Status)) && (ExpectedSuccess != FALSE)) {
        printf("Error: Allocation Failed: size %d, Requested address: "
               "%p, Status = %d.\n",
               Size,
               RequestedAddress,
               Status);

        *Failures += 1;

    } else if ((KSUCCESS(Status)) && (ExpectedSuccess == FALSE)) {
        printf("Error: Allocation succeeded that shouldn't have. Size %d, "
               "Requested address: %p.\n",
               Size,
               RequestedAddress);

        *Failures += 1;
    }

    if (KSUCCESS(Status)) {
        if ((RequestedAddress != NULL) && (Allocation != RequestedAddress)) {
            printf("Error: Requested address %p, but got %p\n",
                   RequestedAddress,
                   Allocation);

            *Failures += 1;
        }
    }

    Valid = ValidateMdl(&(Process->AddressSpace->Accountant->Mdl));
    if (Valid == FALSE) {
        printf("MDL not valid after allocating %p.\n",
               Allocation);

        *Failures += 1;
    }

    return Allocation;
}

PVOID
TestExpandPool (
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

    return malloc(Size);
}

BOOL
TestContractPool (
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

    Size - Supplies the size of the allocation to free.

Return Value:

    TRUE if the memory was successfully freed.

    FALSE if the memory could not be freed at this time.

--*/

{

    free(Memory);
    return TRUE;
}

