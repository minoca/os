/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module contains functions necessary to initialize the memory manager
    subsystem.

Author:

    Evan Green 1-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the stack size for the processor initialization and idle thread.
//

#define DEFAULT_IDLE_STACK_SIZE 0x3000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the iteration context when creating an array of
    boot descriptors.

Members:

    Count - Stores the currently counted number of descriptors.

    AllocatedCount - Stores the number of elements in the allocated array.

    Array - Stores the destination array of descriptors.

--*/

typedef struct _BOOT_DESCRIPTOR_ITERATOR_CONTEXT {
    UINTN Count;
    UINTN AllocatedCount;
    PMEMORY_DESCRIPTOR Array;
} BOOT_DESCRIPTOR_ITERATOR_CONTEXT, *PBOOT_DESCRIPTOR_ITERATOR_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
MmpInitializeKernelVa (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

KSTATUS
MmpArchInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Phase
    );

KSTATUS
MmpInitializeUserSharedData (
    VOID
    );

KSTATUS
MmpFreeBootMappings (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

VOID
MmpFreeBootMappingsIpiRoutine (
    PVOID Context
    );

KSTATUS
MmpCreateBootMemoryDescriptorArray (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    PMEMORY_DESCRIPTOR *Descriptors,
    PULONG DescriptorCount
    );

VOID
MmpBootMemoryDescriptorIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
MmInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    PPROCESSOR_START_BLOCK StartBlock,
    ULONG Phase
    )

/*++

Routine Description:

    This routine initializes the kernel Memory Manager.

Arguments:

    Parameters - Supplies a pointer to the initialization block from the loader.

    StartBlock - Supplies a pointer to the processor start block if this is an
        application processor.

    Phase - Supplies the phase of initialization. Valid values are 0 through 4.

Return Value:

    Status code.

--*/

{

    PPROCESSOR_BLOCK ProcessorBlock;
    KSTATUS Status;

    //
    // Phase 0 is executed on the boot processor before the debugger comes
    // online.
    //

    if (Phase == 0) {

        //
        // Perform phase 0 architecture specific initialization.
        //

        Status = MmpArchInitialize(Parameters, 0);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

    //
    // Phase 1 is executed on all processors.
    //

    } else if (Phase == 1) {

        //
        // Set the swap virtual address used by this processor.
        //

        ProcessorBlock = KeGetCurrentProcessorBlock();
        if (Parameters != NULL) {
            ProcessorBlock->SwapPage = Parameters->PageTableStage;

        } else {
            ProcessorBlock->SwapPage = StartBlock->SwapPage;
        }

        ASSERT(ProcessorBlock->SwapPage != NULL);

        //
        // Perform phase 1 architecture specific initialization.
        //

        Status = MmpArchInitialize(Parameters, 1);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        //
        // If the system is just booting, initialize MM data structures.
        //

        if (KeGetCurrentProcessorNumber() == 0) {
            KeInitializeSpinLock(&MmInvalidateIpiLock);
            KeInitializeSpinLock(&MmNonPagedPoolLock);

            //
            // Initialize the physical memory allocator.
            //

            Status = MmpInitializePhysicalPageAllocator(
                                            Parameters->MemoryMap,
                                            &(Parameters->MmInitMemory.Buffer),
                                            &(Parameters->MmInitMemory.Size));

            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            //
            // Initialize structures for kernel VA space. After this routine
            // completes the system is ready to use real allocation routines.
            //

            Status = MmpInitializeKernelVa(Parameters);
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            //
            // Initialize the non-paged pool. This will cause an initial pool
            // expansion.
            //

            Status = MmpInitializeNonPagedPool();
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            //
            // Initialize the user shared data page in the kernel VA space.
            //

            Status = MmpInitializeUserSharedData();
            if (!KSUCCESS(Status)) {
                goto InitializeEnd;
            }

            //
            // Initialize the paged pool. No memory gets mapped for the paged
            // pool initialization, page faults bring it in as needed.
            //

            MmpInitializePagedPool();
        }

    //
    // In phase 2, lock down memory structures in preparation for
    // multi-threaded access. This is only executed on processor 0.
    //

    } else if (Phase == 2) {

        ASSERT(KeGetCurrentProcessorNumber() == 0);

        MmPagedPoolLock = KeCreateQueuedLock();
        if (MmPagedPoolLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        //
        // Create the kernel's VA lock, which was deferred becaues the Object
        // Manager was not online.
        //

        MmKernelVirtualSpace.Lock = KeCreateSharedExclusiveLock();
        if (MmKernelVirtualSpace.Lock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        //
        // Create the kernel's VA memory warning event.
        //

        MmVirtualMemoryWarningEvent = KeCreateEvent(NULL);
        if (MmVirtualMemoryWarningEvent == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        //
        // Create the physical address lock.
        //

        MmPhysicalPageLock = KeCreateQueuedLock();
        if (MmPhysicalPageLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        //
        // Create an event that signals whenever there is a change in the
        // physical memory warning level.
        //

        MmPhysicalMemoryWarningEvent = KeCreateEvent(NULL);
        if (MmPhysicalMemoryWarningEvent == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        //
        // Initialize the paging infrastructure. Some things need to be set up
        // even if a page file will never arrive. This must be done before the
        // first paged pool allocation.
        //

        Status = MmpInitializePaging();
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status = MmpArchInitialize(Parameters, 2);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status = STATUS_SUCCESS;

    //
    // In phase 3, free all loader temporary space, the kernel is on its own
    // now.
    //

    } else {

        ASSERT(Phase == 3);

        Status = MmpFreeBootMappings(Parameters);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        //
        // If physical page zero exists, it was removed from the memory map
        // during physical page initialization. If it was free or a temporary
        // boot allocation, it is now available for wise reuse.
        //

        if (MmPhysicalPageZeroAvailable != FALSE) {
            MmpAddPageZeroDescriptorsToMdl(&MmKernelVirtualSpace);
        }
    }

InitializeEnd:
    return Status;
}

KSTATUS
MmPrepareForProcessorLaunch (
    PPROCESSOR_START_BLOCK StartBlock
    )

/*++

Routine Description:

    This routine initializes a processor start block in preparation for
    launching a new processor.

Arguments:

    StartBlock - Supplies a pointer to the start block that will be passed to
        the new core.

Return Value:

    Status code.

--*/

{

    UINTN PageSize;
    KSTATUS Status;
    VM_ALLOCATION_PARAMETERS VaRequest;

    PageSize = MmPageSize();
    StartBlock->StackBase = MmAllocateKernelStack(DEFAULT_IDLE_STACK_SIZE);
    if (StartBlock->StackBase == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PrepareForProcessorLaunchEnd;
    }

    StartBlock->StackSize = DEFAULT_IDLE_STACK_SIZE;

    //
    // Allocate a space the processor can use for temporary mappings. Note that
    // processors do actual TLB fills on speculative data accesses, so other
    // processors may accumulate stale mappings to this VA. As long as this
    // address is only ever used by the processor that owns it, it's all fine.
    //

    VaRequest.Address = NULL;
    VaRequest.Alignment = PageSize;
    VaRequest.Size = SWAP_VA_PAGES * PageSize;
    VaRequest.Min = 0;
    VaRequest.Max = MAX_ADDRESS;
    VaRequest.MemoryType = MemoryTypeReserved;
    VaRequest.Strategy = AllocationStrategyAnyAddress;
    Status = MmpAllocateAddressRange(&MmKernelVirtualSpace, &VaRequest, FALSE);
    if (!KSUCCESS(Status)) {
        goto PrepareForProcessorLaunchEnd;
    }

    StartBlock->SwapPage = VaRequest.Address;
    Status = STATUS_SUCCESS;

PrepareForProcessorLaunchEnd:
    if (!KSUCCESS(Status)) {
        MmDestroyProcessorStartBlock(StartBlock);
    }

    return Status;
}

VOID
MmDestroyProcessorStartBlock (
    PPROCESSOR_START_BLOCK StartBlock
    )

/*++

Routine Description:

    This routine destroys structures initialized by MM in preparation for a
    (now failed) processor launch.

Arguments:

    StartBlock - Supplies a pointer to the start block.

Return Value:

    None.

--*/

{

    UINTN PageSize;

    if (StartBlock->StackBase != NULL) {
        MmFreeKernelStack(StartBlock->StackBase, DEFAULT_IDLE_STACK_SIZE);
        StartBlock->StackBase = NULL;
    }

    if (StartBlock->SwapPage != NULL) {
        PageSize = MmPageSize();
        MmpFreeAccountingRange(NULL,
                               StartBlock->SwapPage,
                               SWAP_VA_PAGES * PageSize,
                               FALSE,
                               0);

        StartBlock->SwapPage = NULL;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
MmpFreeBootMappings (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine unmaps and releases the physical memory associated with
    temporary boot memory. After this routine, the kernel initialization block
    is no longer touchable.

Arguments:

    Parameters - Supplies a pointer to the initialization block from the loader.

Return Value:

    Status code.

--*/

{

    PMEMORY_DESCRIPTOR CurrentDescriptor;
    ULONG DescriptorIndex;
    ULONGLONG PageCount;
    ULONG PageShift;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG PhysicalDescriptorCount;
    PMEMORY_DESCRIPTOR PhysicalDescriptors;
    PROCESSOR_SET ProcessorSet;
    KSTATUS Status;
    PVOID VirtualAddress;
    ULONG VirtualDescriptorCount;
    PMEMORY_DESCRIPTOR VirtualDescriptors;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    PageShift = MmPageShift();
    PhysicalDescriptors = NULL;
    VirtualDescriptors = NULL;

    //
    // Create an array of the virtual boot memory descriptors.
    //

    Status = MmpCreateBootMemoryDescriptorArray(Parameters->VirtualMap,
                                                &VirtualDescriptors,
                                                &VirtualDescriptorCount);

    if (!KSUCCESS(Status)) {
        goto FreeBootMappingsEnd;
    }

    //
    // Create an array of the physical boot memory descriptors.
    //

    Status = MmpCreateBootMemoryDescriptorArray(Parameters->MemoryMap,
                                                &PhysicalDescriptors,
                                                &PhysicalDescriptorCount);

    if (!KSUCCESS(Status)) {
        goto FreeBootMappingsEnd;
    }

    //
    // Once the unmapping completes, the parameters structure will not be
    // accessible. Set it to NULL to make sure it is no longer used.
    //

    Parameters = NULL;

    //
    // Loop through the virtual descriptors, unmapping the region in every
    // descriptor.
    //

    for (DescriptorIndex = 0;
         DescriptorIndex < VirtualDescriptorCount;
         DescriptorIndex += 1) {

        CurrentDescriptor = &(VirtualDescriptors[DescriptorIndex]);
        VirtualAddress = (PVOID)(UINTN)CurrentDescriptor->BaseAddress;
        PageCount = CurrentDescriptor->Size >> PageShift;

        ASSERT((PageCount << PageShift) == CurrentDescriptor->Size);

        //
        // Releasing the accounting range for user mode regions will decrement
        // the resident set counter, but it never accounted for these pages
        // as the descriptors were created before the kernel process was
        // present.
        //

        if (VirtualAddress < KERNEL_VA_START) {
            MmpUpdateResidentSetCounter(PsGetKernelProcess()->AddressSpace,
                                        PageCount);
        }

        Status = MmpFreeAccountingRange(NULL,
                                        VirtualAddress,
                                        PageCount << PageShift,
                                        FALSE,
                                        0);

        if (!KSUCCESS(Status)) {
            goto FreeBootMappingsEnd;
        }
    }

    //
    // Make sure all user mode descriptors are removed from the kernel virtual
    // space.
    //

    Status = MmpRemoveAccountingRange(&MmKernelVirtualSpace,
                                      0,
                                      (UINTN)KERNEL_VA_START);

    if (!KSUCCESS(Status)) {
        goto FreeBootMappingsEnd;
    }

    //
    // Perform architecture specific work, including releasing boot page tables
    // that are no longer in use.
    //

    Status = MmpArchInitialize(NULL, 3);
    if (!KSUCCESS(Status)) {
        goto FreeBootMappingsEnd;
    }

    //
    // Invalidate the entire TLB on all processors.
    //

    ProcessorSet.Target = ProcessorTargetAll;
    Status = KeSendIpi(MmpFreeBootMappingsIpiRoutine, NULL, &ProcessorSet);
    if (!KSUCCESS(Status)) {
        goto FreeBootMappingsEnd;
    }

    //
    // Now that the physical pages have been unmapped and removed from the
    // page tables and TLB, loop through the physical descriptors and free
    // every region.
    //

    for (DescriptorIndex = 0;
         DescriptorIndex < PhysicalDescriptorCount;
         DescriptorIndex += 1) {

        CurrentDescriptor = &(PhysicalDescriptors[DescriptorIndex]);
        PhysicalAddress = CurrentDescriptor->BaseAddress;
        PageCount = CurrentDescriptor->Size >> PageShift;
        while (PageCount > MAX_UINTN) {
            MmFreePhysicalPages(PhysicalAddress, MAX_UINTN);
            PhysicalAddress += MAX_UINTN << PageShift;
            PageCount -= MAX_UINTN;
        }

        if (PageCount != 0) {
            MmFreePhysicalPages(PhysicalAddress, PageCount);
        }
    }

    Status = STATUS_SUCCESS;

FreeBootMappingsEnd:
    if (VirtualDescriptors != NULL) {
        MmFreeNonPagedPool(VirtualDescriptors);
    }

    if (PhysicalDescriptors != NULL) {
        MmFreeNonPagedPool(PhysicalDescriptors);
    }

    return Status;
}

VOID
MmpFreeBootMappingsIpiRoutine (
    PVOID Context
    )

/*++

Routine Description:

    This routine is an IPI routine that runs once all boot allocations are
    freed. It flushes the entire TLB on the current processor.

Arguments:

    Context - Supplies an unused context parameter.

Return Value:

    None.

--*/

{

    ArInvalidateEntireTlb();
    return;
}

KSTATUS
MmpCreateBootMemoryDescriptorArray (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    PMEMORY_DESCRIPTOR *Descriptors,
    PULONG DescriptorCount
    )

/*++

Routine Description:

    This routine creates an array of boot memory descriptors based on the given
    memory map.

Arguments:

    MemoryMap - Supplies a pointer to the memory map whose descriptors are to
        be put in an array.

    Descriptors - Supplies a pointer that receives an array of memory
        descriptors.

    DescriptorCount - Supplies a pointer that receives the number of memory
        descriptors in the allocated array.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    BOOT_DESCRIPTOR_ITERATOR_CONTEXT Context;
    PMEMORY_DESCRIPTOR DescriptorArray;
    KSTATUS Status;

    //
    // Determine how many boot descriptors are in the memory map.
    //

    RtlZeroMemory(&Context, sizeof(BOOT_DESCRIPTOR_ITERATOR_CONTEXT));
    MmMdIterate(MemoryMap, MmpBootMemoryDescriptorIterationRoutine, &Context);

    //
    // Allocate an array of descriptors and copy the descriptors from the
    // initialization block into this temporary array. This must be done
    // because one of the things being unmapped and freed is this memory list.
    //

    AllocationSize = Context.Count * sizeof(MEMORY_DESCRIPTOR);
    DescriptorArray = MmAllocateNonPagedPool(AllocationSize, MM_ALLOCATION_TAG);
    if (DescriptorArray == NULL) {
        Status = STATUS_NO_MEMORY;
        goto CreateMemoryDescriptorArrayEnd;
    }

    Context.Array = DescriptorArray;
    Context.AllocatedCount = Context.Count;
    Context.Count = 0;

    //
    // Loop through the list again, copying the descriptors into the new space.
    //

    MmMdIterate(MemoryMap, MmpBootMemoryDescriptorIterationRoutine, &Context);
    *Descriptors = DescriptorArray;
    *DescriptorCount = Context.Count;
    Status = STATUS_SUCCESS;

CreateMemoryDescriptorArrayEnd:
    return Status;
}

VOID
MmpBootMemoryDescriptorIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    )

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

{

    PBOOT_DESCRIPTOR_ITERATOR_CONTEXT IteratorContext;

    IteratorContext = Context;
    if ((Descriptor->Size != 0) &&
        ((Descriptor->Type == MemoryTypeLoaderTemporary) ||
         (Descriptor->Type == MemoryTypeFirmwareTemporary))) {

        if (IteratorContext->Array != NULL) {

            ASSERT(IteratorContext->Count < IteratorContext->AllocatedCount);

            RtlCopyMemory(&(IteratorContext->Array[IteratorContext->Count]),
                          Descriptor,
                          sizeof(MEMORY_DESCRIPTOR));
        }

        IteratorContext->Count += 1;
    }

    return;
}

