/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    mapping.c

Abstract:

    This module implements memory mapping and unmapping functionality.

Author:

    Evan Green 1-Aug-2012

Environment:

    Kernel (x86)

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/bootload.h>
#include <minoca/x86.h>
#include "../mmp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// This macro uses the self-mappings to retrieve the page table for the given
// page directory index.
//

#define GET_PAGE_TABLE(_DirectoryIndex) \
    (PPTE)((PVOID)MmKernelPageTables + (PAGE_SIZE * _DirectoryIndex))

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
MmpCreatePageTable (
    volatile PTE *Directory,
    PVOID VirtualAddress
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Stores a pointer to the kernel's top level page table structure.
//

volatile PTE *MmKernelPageDirectory;

//
// Stores a pointer to the self-mappings that point to page tables.
//

PPTE MmKernelPageTables;

//
// Synchronizes access to creating or destroying page tables.
//

PQUEUED_LOCK MmPageTableLock;

//
// Keep a page below 1MB for identity mapping.
//

PHYSICAL_ADDRESS MmFirstMegabyteFreePage;

//
// Stores a pointer to the page directory block allocator.
//

PBLOCK_ALLOCATOR MmPageDirectoryBlockAllocator;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
ULONG
MmPageSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of a page of memory.

Arguments:

    None.

Return Value:

    Returns the size of one page of memory (ie the minimum mapping granularity).

--*/

{

    return PAGE_SIZE;
}

KERNEL_API
ULONG
MmPageShift (
    VOID
    )

/*++

Routine Description:

    This routine returns the amount to shift by to truncate an address to a
    page number.

Arguments:

    None.

Return Value:

    Returns the amount to shift to reach page granularity.

--*/

{

    return PAGE_SHIFT;
}

KSTATUS
MmCreatePageDirectory (
    PVOID *NewPageDirectory,
    PPHYSICAL_ADDRESS NewPageDirectoryPhysical,
    BOOL KernelProcessPageDirectory
    )

/*++

Routine Description:

    This routine creates a new page directory for a new process, and
    initializes it with kernel address space.

Arguments:

    NewPageDirectory - Supplies a pointer that will receive the new page
        directory.

    NewPageDirectoryPhysical - Supplies a pointer where the physical address
        of the new page directory will be returned.

    KernelProcessPageDirectory - Supplies a boolean indicating whether or not
        the the page directory is for the kernel process.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_MEMORY if memory could not be allocated for the page table.

--*/

{

    ULONG CopySize;
    ULONG DirectoryIndex;
    ULONG KernelIndex;
    volatile PTE *PageDirectory;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONG ZeroSize;

    PageDirectory = NULL;

    //
    // If this is a request for the kernel process, then just use the global
    // page directory. The self map should already be initialized.
    //

    if (KernelProcessPageDirectory != FALSE) {
        PageDirectory = MmKernelPageDirectory;
        *NewPageDirectoryPhysical = MmpVirtualToPhysical((PVOID)PageDirectory,
                                                         NULL);

        Status = STATUS_SUCCESS;
        goto CreatePageDirectoryEnd;
    }

    ASSERT(MmPageDirectoryBlockAllocator != NULL);

    PageDirectory = MmAllocateBlock(MmPageDirectoryBlockAllocator,
                                    &PhysicalAddress);

    if (PageDirectory == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePageDirectoryEnd;
    }

    //
    // Zero the user mode portion and copy the kernel portion from the kernel
    // page directory.
    //

    KernelIndex = (UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT;
    ZeroSize = KernelIndex * sizeof(PTE);
    CopySize = PAGE_SIZE - ZeroSize;
    RtlZeroMemory((PVOID)PageDirectory, ZeroSize);
    RtlCopyMemory((PVOID)&(PageDirectory[KernelIndex]),
                  (PVOID)&(MmKernelPageDirectory[KernelIndex]),
                  CopySize);

    //
    // Make the self mappings point to this page directory.
    //

    DirectoryIndex = (UINTN)MmKernelPageTables >> PAGE_DIRECTORY_SHIFT;
    PageDirectory[DirectoryIndex].Entry = (ULONG)PhysicalAddress >> PAGE_SHIFT;
    PageDirectory[DirectoryIndex].Writable = 1;
    PageDirectory[DirectoryIndex].Present = 1;
    *NewPageDirectoryPhysical = PhysicalAddress;
    Status = STATUS_SUCCESS;

CreatePageDirectoryEnd:
    if (!KSUCCESS(Status)) {
        if (PageDirectory != NULL) {
            MmFreeBlock(MmPageDirectoryBlockAllocator, (PVOID)PageDirectory);
            PageDirectory = NULL;
        }
    }

    *NewPageDirectory = (PVOID)PageDirectory;
    return Status;
}

VOID
MmDestroyPageDirectory (
    PVOID PageDirectory
    )

/*++

Routine Description:

    This routine destroys a page directory upon process destruction.

Arguments:

    PageDirectory - Supplies a pointer to the page directory to destroy.

Return Value:

    None.

--*/

{

    PPTE Directory;
    ULONG DirectoryIndex;
    PHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS RunPhysicalAddress;
    UINTN RunSize;

    //
    // Do nothing if this is the global page directory.
    //

    if (PageDirectory == MmKernelPageDirectory) {
        return;
    }

    //
    // Loop through and free every allocated page table in user mode.
    //

    RunSize = 0;
    RunPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    Directory = PageDirectory;
    for (DirectoryIndex = 0;
         DirectoryIndex < ((UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT);
         DirectoryIndex += 1) {

        if (Directory[DirectoryIndex].Entry != 0) {
            PhysicalAddress = (ULONG)(Directory[DirectoryIndex].Entry <<
                                      PAGE_SHIFT);

            if (RunSize != 0) {
                if ((RunPhysicalAddress + RunSize) == PhysicalAddress) {
                    RunSize += PAGE_SIZE;

                } else {
                    MmFreePhysicalPages(RunPhysicalAddress,
                                        RunSize >> PAGE_SHIFT);

                    RunPhysicalAddress = PhysicalAddress;
                    RunSize = PAGE_SIZE;
                }

            } else {
                RunPhysicalAddress = PhysicalAddress;
                RunSize = PAGE_SIZE;
            }
        }
    }

    if (RunSize != 0) {
        MmFreePhysicalPages(RunPhysicalAddress, RunSize >> PAGE_SHIFT);
    }

    MmFreeBlock(MmPageDirectoryBlockAllocator, PageDirectory);
    return;
}

VOID
MmIdentityMapStartupStub (
    ULONG PageCount,
    PVOID *Allocation,
    PVOID *PageDirectory
    )

/*++

Routine Description:

    This routine allocates and identity maps pages in the first 1MB of physical
    memory for use by application processor startup code.

Arguments:

    PageCount - Supplies the number of pages to allocate and map.

    Allocation - Supplies a pointer where the virtual/physical address will
        be returned on success.

    PageDirectory - Supplies a pointer where the current page directory will be
        returned.

Return Value:

    None.

--*/

{

    PVOID CurrentAddress;
    ULONG CurrentPage;

    //
    // Allocate pages starting at address 0x1000.
    //

    *Allocation = (PVOID)(UINTN)MmFirstMegabyteFreePage;

    ASSERT(*Allocation != (PVOID)MAX_ULONG);

    MmFirstMegabyteFreePage = MAX_ULONG;
    CurrentAddress = *Allocation;
    for (CurrentPage = 0; CurrentPage < PageCount; CurrentPage += 1) {
        MmpMapPage((UINTN)CurrentAddress,
                   CurrentAddress,
                   MAP_FLAG_PRESENT | MAP_FLAG_EXECUTE);

        CurrentAddress += PAGE_SIZE;
    }

    *PageDirectory = (PVOID)ArGetCurrentPageDirectory();
    return;
}

VOID
MmUnmapStartupStub (
    PVOID Allocation,
    ULONG PageCount
    )

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

{

    ASSERT(Allocation != (PVOID)MAX_ULONG);

    MmFirstMegabyteFreePage = (UINTN)Allocation;

    //
    // Unmap the pages. Don't "free" the physical pages because they were
    // never recognized as memory.
    //

    MmpUnmapPages(Allocation, PageCount, UNMAP_FLAG_SEND_INVALIDATE_IPI, NULL);
    return;
}

VOID
MmUpdatePageDirectory (
    PVOID PageDirectory,
    PVOID VirtualAddress,
    UINTN Size
    )

/*++

Routine Description:

    This routine updates the kernel mode entries in the given page directory
    for the given virtual address range so that they're current.

Arguments:

    PageDirectory - Supplies a pointer to the virtual address of the page
        directory to update.

    VirtualAddress - Supplies the base virtual address of the range to be
        synchronized.

    Size - Supplies the size of the virtual address range to synchronize.

Return Value:

    None.

--*/

{

    ULONG EndIndex;
    PPTE Entry;
    ULONG Index;

    //
    // Do nothing if this is the global page directory.
    //

    if (PageDirectory == MmKernelPageDirectory) {
        return;
    }

    Entry = PageDirectory;
    Index = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    EndIndex = (UINTN)(VirtualAddress + (Size - 1)) >> PAGE_DIRECTORY_SHIFT;
    while (Index <= EndIndex) {

        //
        // The supplied VA range should never include the self map directory
        // entries.
        //

        ASSERT(Index != ((UINTN)MmKernelPageTables >> PAGE_DIRECTORY_SHIFT));

        Entry[Index] = MmKernelPageDirectory[Index];
        Index += 1;
    }

    return;
}

ULONG
MmValidateMemoryAccessForDebugger (
    PVOID Address,
    ULONG Length,
    PBOOL Writable
    )

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

{

    ULONG ByteOffset;
    ULONG BytesMapped;
    ULONG BytesRemaining;
    ULONG BytesThisRound;
    ULONG DirectoryIndex;
    volatile PTE *PageDirectory;
    volatile PTE *PageTable;
    ULONG SelfMapIndex;
    ULONG TableIndex;

    //
    // Assume that all pages are writable until proven otherwise.
    //

    if (Writable != NULL) {
        *Writable = TRUE;
    }

    //
    // If the memory manager has not been initialized yet, just assume the
    // entire region is valid.
    //

    if (MmKernelPageTables == NULL) {
        if (Writable != NULL) {
            *Writable = FALSE;
        }

        return Length;
    }

    //
    // Get the page directory by using the self-map.
    //

    SelfMapIndex = (UINTN)MmKernelPageTables >> PAGE_DIRECTORY_SHIFT;
    PageDirectory = GET_PAGE_TABLE(SelfMapIndex);

    //
    // For each page in the address range, determine if it is mapped.
    //

    BytesMapped = 0;
    BytesRemaining = Length;
    while (BytesRemaining != 0) {
        DirectoryIndex = (UINTN)Address >> PAGE_DIRECTORY_SHIFT;
        if (PageDirectory[DirectoryIndex].Present == 0) {
            break;
        }

        PageTable = GET_PAGE_TABLE(DirectoryIndex);
        TableIndex = ((UINTN)Address & PTE_INDEX_MASK) >> PAGE_SHIFT;
        if (PageTable[TableIndex].Present == 0) {
            break;
        }

        if ((Writable != NULL) && (PageTable[TableIndex].Writable == 0)) {
            *Writable = FALSE;
        }

        ByteOffset = (UINTN)Address & PAGE_MASK;
        BytesThisRound = PAGE_SIZE - ByteOffset;
        if (BytesThisRound > BytesRemaining) {
            BytesThisRound = BytesRemaining;
        }

        BytesRemaining -= BytesThisRound;
        Address += BytesThisRound;
        BytesMapped += BytesThisRound;
    }

    return BytesMapped;
}

VOID
MmModifyAddressMappingForDebugger (
    PVOID Address,
    BOOL Writable,
    PBOOL WasWritable
    )

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

{

    ULONG DirectoryIndex;
    volatile PTE *PageDirectory;
    volatile PTE *PageTable;
    ULONG SelfMapIndex;
    ULONG TableIndex;

    //
    // Assume that the page was writable and do no more if the memory manager
    // is not yet initialized.
    //

    *WasWritable = TRUE;
    if (MmKernelPageTables == NULL) {
        return;
    }

    //
    // Get the page directory by using the self-map.
    //

    SelfMapIndex = (UINTN)MmKernelPageTables >> PAGE_DIRECTORY_SHIFT;
    PageDirectory = GET_PAGE_TABLE(SelfMapIndex);

    //
    // For the page containing the address, modify its mapping properties. It
    // should be mapped.
    //

    DirectoryIndex = (UINTN)Address >> PAGE_DIRECTORY_SHIFT;

    ASSERT(PageDirectory[DirectoryIndex].Present != 0);

    PageTable = GET_PAGE_TABLE(DirectoryIndex);
    TableIndex = ((UINTN)Address & PTE_INDEX_MASK) >> PAGE_SHIFT;

    ASSERT(PageTable[TableIndex].Present != 0);

    //
    // Record if the page was not actually writable and modify the mapping if
    // necessary.
    //

    if (PageTable[TableIndex].Writable == 0) {
        *WasWritable = FALSE;
        if (Writable != FALSE) {
            PageTable[TableIndex].Writable = 1;
        }

    } else {
        if (Writable == FALSE) {
            PageTable[TableIndex].Writable = 0;
        }
    }

    ArInvalidateTlbEntry(Address);
    return;
}

KSTATUS
MmpArchInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Phase
    )

/*++

Routine Description:

    This routine performs architecture-specific initialization.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization parameters
        from the loader.

    Phase - Supplies the initialization phase.

Return Value:

    Status code.

--*/

{

    PBLOCK_ALLOCATOR BlockAllocator;
    ULONG Flags;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SizeToZero;
    KSTATUS Status;

    //
    // Phase 0 runs on the boot processor before the debugger is online.
    //

    if (Phase == 0) {
        if ((Parameters->PageDirectory == NULL) ||
            (Parameters->PageTables == NULL) ||
            (Parameters->PageTableStage == NULL)) {

            Status = STATUS_NOT_INITIALIZED;
            goto ArchInitializeEnd;
        }

        MmKernelPageDirectory = Parameters->PageDirectory;
        MmKernelPageTables = Parameters->PageTables;
        ProcessorBlock = KeGetCurrentProcessorBlock();
        ProcessorBlock->SwapPage = Parameters->PageTableStage;
        Status = STATUS_SUCCESS;

    //
    // Phase 1 initialization runs on all processors.
    //

    } else if (Phase == 1) {

        //
        // Initialize basic globals if this is the boot processor.
        //

        if (KeGetCurrentProcessorNumber() == 0) {

            //
            // There should be a free page in the first megabyte somewhere,
            // needed for processor startup.
            //

            Status = MmpEarlyAllocatePhysicalMemory(
                                               Parameters->MemoryMap,
                                               1,
                                               0,
                                               AllocationStrategyLowestAddress,
                                               &MmFirstMegabyteFreePage);

            if (!KSUCCESS(Status)) {
                goto ArchInitializeEnd;
            }

            if (MmFirstMegabyteFreePage >= _1MB) {
                RtlDebugPrint("No free page in the first 1MB.\n");
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto ArchInitializeEnd;
            }
        }

        Status = STATUS_SUCCESS;

    //
    // Phase 2 initialization only runs on the boot processor in order to
    // prepare for multi-threaded execution.
    //

    } else if (Phase == 2) {

        //
        // Create a queued lock to synchronize leaf page table creation and
        // insertion.
        //

        MmPageTableLock = KeCreateQueuedLock();
        if (MmPageTableLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ArchInitializeEnd;
        }

        //
        // Create a block allocator for page directories. This prevents the
        // need to allocate and map a page directory for every new process and
        // the need to unmap and free for every dying process. The IPIs get
        // expensive on unmap.
        //

        Flags = BLOCK_ALLOCATOR_FLAG_NON_PAGED |
                BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS |
                BLOCK_ALLOCATOR_FLAG_TRIM;

        BlockAllocator = MmCreateBlockAllocator(
                             PAGE_SIZE,
                             PAGE_SIZE,
                             TRUE,
                             MM_PAGE_DIRECTORY_BLOCK_ALLOCATOR_EXPANSION_COUNT,
                             Flags,
                             MM_PAGE_DIRECTORY_BLOCK_ALLOCATION_TAG);

        if (BlockAllocator == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ArchInitializeEnd;
        }

        MmPageDirectoryBlockAllocator = BlockAllocator;
        Status = STATUS_SUCCESS;

    //
    // Phase 3 runs once after the scheduler is active.
    //

    } else if (Phase == 3) {

        //
        // Zero out the usermode section of the page directory, wiping out all
        // page tables in user mode that came from the boot environment.
        //
        // N.B. The page tables mapped in the user mode portion of the page
        //      directory do not need to be released. They were marked as
        //      loader temporary memory and get released by other means.
        //

        SizeToZero = ((UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT) *
                     sizeof(PTE);

        RtlZeroMemory((PVOID)MmKernelPageDirectory, SizeToZero);
        Status = STATUS_SUCCESS;

    } else {

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto ArchInitializeEnd;
    }

ArchInitializeEnd:
    return Status;
}

BOOL
MmpCheckDirectoryUpdates (
    PVOID FaultingAddress
    )

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

{

    volatile PTE *CurrentPageDirectory;
    PKPROCESS CurrentProcess;
    ULONG DirectoryIndex;
    volatile PTE *PageTable;
    ULONG TableIndex;

    //
    // This check only applies to kernel-mode addresses.
    //

    if (FaultingAddress < KERNEL_VA_START) {
        return FALSE;
    }

    CurrentProcess = PsGetCurrentProcess();
    CurrentPageDirectory = CurrentProcess->PageDirectory;
    DirectoryIndex = (UINTN)FaultingAddress >> PAGE_DIRECTORY_SHIFT;

    //
    // Check to see if the kernel page directory has an entry and the current
    // page directory doesn't. If so, add the entry.
    //

    if ((MmKernelPageDirectory[DirectoryIndex].Present == 1) &&
        (CurrentPageDirectory[DirectoryIndex].Present == 0)) {

        CurrentPageDirectory[DirectoryIndex] =
                                        MmKernelPageDirectory[DirectoryIndex];

        //
        // See if the page fault is resolved by this entry.
        //

        PageTable = GET_PAGE_TABLE(DirectoryIndex);
        TableIndex = ((UINTN)FaultingAddress & PTE_INDEX_MASK) >> PAGE_SHIFT;
        if (PageTable[TableIndex].Present == 1) {
            return TRUE;
        }
    }

    return FALSE;
}

PVOID
MmpEarlyAllocateMemory (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    ULONG PageCount,
    ULONGLONG PhysicalAlignment
    )

/*++

Routine Description:

    This routine allocates memory for MM init routines. It is used when MM is
    bootstrapping and the facilities required for allocating memory need memory
    for themselves.

Arguments:

    MemoryMap - Supplies a pointer to the system memory map.

    PageCount - Supplies the number of pages needed.

    PhysicalAlignment - Supplies the required alignment of the physical memory
        allocation, in bytes. Valid values are powers of 2. Values of 0 or 1
        indicate no physical alignment requirement.

Return Value:

    Returns a pointer to the memory on success, or NULL on failure.

--*/

{

    ULONG DirectoryIndex;
    ULONG FreePages;
    UINTN NewAllocation;
    ULONG PagesMapped;
    volatile PTE *PageTable;
    PHYSICAL_ADDRESS PageTablePhysical;
    PHYSICAL_ADDRESS PhysicalAllocation;
    ULONG PotentialDirectoryIndex;
    ULONG PotentialTableIndex;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SelfMappingIndex;
    ULONG StagingDirectoryIndex;
    volatile PTE *StagingPageTable;
    ULONG StagingTableIndex;
    KSTATUS Status;
    ULONG TableIndex;

    PhysicalAllocation = INVALID_PHYSICAL_ADDRESS;

    //
    // If MM doesn't even know where the page directory or page tables are,
    // there's not really much that can be done.
    //

    if ((MmKernelPageDirectory == NULL) ||
        (MmKernelPageTables == NULL)) {

        return NULL;
    }

    //
    // Find the physical memory for the allocation. Defer this procedure if the
    // physical page allocator is up.
    //

    if (MmTotalPhysicalPages == 0) {
        Status = MmpEarlyAllocatePhysicalMemory(MemoryMap,
                                                PageCount,
                                                PhysicalAlignment,
                                                AllocationStrategyAnyAddress,
                                                &PhysicalAllocation);

        if (!KSUCCESS(Status)) {
            return NULL;
        }

        ASSERT((PhysicalAllocation & PAGE_MASK) == 0);
    }

    ProcessorBlock = KeGetCurrentProcessorBlock();

    //
    // Find the self mappings.
    //

    SelfMappingIndex = ((UINTN)MmKernelPageTables & PDE_INDEX_MASK) >>
                        PAGE_DIRECTORY_SHIFT;

    //
    // Find a virtual address that is large enough for the allocation.
    //

    FreePages = 0;
    PotentialDirectoryIndex = 0;
    PotentialTableIndex = 0;
    for (DirectoryIndex = (UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT;
         DirectoryIndex < MAX_ULONG >> PAGE_DIRECTORY_SHIFT;
         DirectoryIndex += 1) {

        //
        // Skip the self mappings.
        //

        if (DirectoryIndex == SelfMappingIndex) {
            FreePages = 0;
            PotentialDirectoryIndex = 0;
            PotentialTableIndex = 0;
            continue;
        }

        //
        // Check to see if the PDE is present, and create a page table if it
        // is not.
        //

        if (MmKernelPageDirectory[DirectoryIndex].Present == 0) {

            //
            // Allocate a physical page for the page table.
            //

            if (MmTotalPhysicalPages == 0) {
                Status = MmpEarlyAllocatePhysicalMemory(
                                                  MemoryMap,
                                                  1,
                                                  0,
                                                  AllocationStrategyAnyAddress,
                                                  &PageTablePhysical);

                if (!KSUCCESS(Status)) {
                    return NULL;
                }

            } else {
                PageTablePhysical = MmpAllocatePhysicalPages(1, 0);
                if (PageTablePhysical == INVALID_PHYSICAL_ADDRESS) {
                    return NULL;
                }
            }

            //
            // Map the page table to the staging area. This is required to zero
            // the page table before actually making it a live page table.
            //

            ASSERT(((UINTN)(ProcessorBlock->SwapPage) & PAGE_MASK) == 0);

            StagingDirectoryIndex = ((UINTN)(ProcessorBlock->SwapPage) &
                                     PDE_INDEX_MASK) >> PAGE_DIRECTORY_SHIFT;

            StagingPageTable = GET_PAGE_TABLE(StagingDirectoryIndex);
            StagingTableIndex = ((UINTN)(ProcessorBlock->SwapPage) &
                                 PTE_INDEX_MASK) >> PAGE_SHIFT;

            StagingPageTable[StagingTableIndex].Entry =
                                        (ULONG)PageTablePhysical >> PAGE_SHIFT;

            StagingPageTable[StagingTableIndex].Writable = 1;
            StagingPageTable[StagingTableIndex].Present = 1;
            ArInvalidateTlbEntry(ProcessorBlock->SwapPage);

            //
            // Zero the page table, unmap it from the staging area, then
            // actually add the directory entry for it.
            //

            RtlZeroMemory(ProcessorBlock->SwapPage, PAGE_SIZE);
            *((PULONG)&(StagingPageTable[StagingTableIndex])) = 0;
            ArInvalidateTlbEntry(ProcessorBlock->SwapPage);
            MmKernelPageDirectory[DirectoryIndex].Writable = 1;
            MmKernelPageDirectory[DirectoryIndex].Entry =
                                        (ULONG)PageTablePhysical >> PAGE_SHIFT;

            MmKernelPageDirectory[DirectoryIndex].Present = 1;
        }

        PageTable = GET_PAGE_TABLE(DirectoryIndex);

        //
        // Scan through the PTEs looking for a run that is large enough.
        //

        for (TableIndex = 0;
             TableIndex < PAGE_SIZE / sizeof(PTE);
             TableIndex += 1) {

            //
            // If it's present, this breaks the run. Start over.
            //

            if (PageTable[TableIndex].Present == 1) {
                FreePages = 0;
                PotentialDirectoryIndex = 0;
                PotentialTableIndex = 0;
                continue;
            }

            if (PotentialDirectoryIndex == 0) {
                PotentialDirectoryIndex = DirectoryIndex;
                PotentialTableIndex = TableIndex;
            }

            //
            // This page is also free. If this makes the run big enough,
            // allocate this space.
            //

            FreePages += 1;
            if (FreePages == PageCount) {

                //
                // Map the allocation. All page tables are already present.
                //

                DirectoryIndex = PotentialDirectoryIndex;
                TableIndex = PotentialTableIndex;
                PageTable = GET_PAGE_TABLE(DirectoryIndex);
                PagesMapped = 0;

                //
                // If the physical page allocator is initialized, the
                // physical allocation was deferred earlier, so allocate
                // now.
                //

                if (MmTotalPhysicalPages != 0) {
                    PhysicalAllocation = MmpAllocatePhysicalPages(
                                                            PageCount,
                                                            PhysicalAlignment);

                    if (PhysicalAllocation == INVALID_PHYSICAL_ADDRESS) {
                        return NULL;
                    }
                }

                while (PagesMapped != PageCount) {
                    PageTable[TableIndex].Writable = 1;
                    PageTable[TableIndex].Entry =
                                        (ULONG)PhysicalAllocation >> PAGE_SHIFT;

                    PageTable[TableIndex].Present = 1;

                    //
                    // If the allocation spans page tables, advance to the next
                    // page table.
                    //

                    PagesMapped += 1;
                    TableIndex += 1;
                    PhysicalAllocation += PAGE_SIZE;
                    if (TableIndex == (PAGE_SIZE / sizeof(PTE))) {

                        ASSERT(DirectoryIndex + 1 > DirectoryIndex);
                        ASSERT(MmKernelPageDirectory[DirectoryIndex].Present !=
                               0);

                        DirectoryIndex += 1;
                        PageTable = GET_PAGE_TABLE(DirectoryIndex);
                        TableIndex = 0;
                    }
                }

                //
                // Create the virtual address and return it.
                //

                NewAllocation = PotentialDirectoryIndex << PAGE_DIRECTORY_SHIFT;
                NewAllocation |= (PotentialTableIndex << PAGE_SHIFT);

                //
                // The virtual allocator should not be up. If it is then the
                // system should have transitioned to the non-early allocation
                // routines.
                //

                ASSERT(MmKernelVirtualSpace.Mdl.DescriptorCount == 0);

                return (PVOID)NewAllocation;
            }
        }
    }

    return NULL;
}

KSTATUS
MmpReserveCurrentMappings (
    PMEMORY_ACCOUNTING Accountant
    )

/*++

Routine Description:

    This routine is called during MM initialization. It scans through the
    page tables and marks everything currently mapped as reserved, syncing the
    virtual allocator to the current state of the machine.

Arguments:

    Accountant - Supplies a pointer to the structure that keeps track of the
        current VA usage.

Return Value:

    Status code.

--*/

{

    MEMORY_DESCRIPTOR Descriptor;
    ULONG DirectoryIndex;
    volatile PTE *PageTable;
    BOOL RangeFree;
    ULONG SelfMappingEnd;
    ULONG SelfMappingIndex;
    KSTATUS Status;
    ULONG TableIndex;
    UINTN VirtualAddress;

    if ((MmKernelPageDirectory == NULL) || (MmKernelPageTables == NULL)) {
        return STATUS_NOT_INITIALIZED;
    }

    SelfMappingIndex = (UINTN)MmKernelPageTables >> PAGE_DIRECTORY_SHIFT;
    SelfMappingEnd = (UINTN)MmKernelPageTables + (1 << PAGE_DIRECTORY_SHIFT);

    //
    // Reserve the entire self-map region. Attempting to traverse into the
    // "page tables" would reserve only currently mapped page tables.
    //

    MmMdInitDescriptor(&Descriptor,
                       (UINTN)MmKernelPageTables,
                       SelfMappingEnd,
                       MemoryTypePageTables);

    Status = MmpAddAccountingDescriptor(Accountant, &Descriptor);
    if (!KSUCCESS(Status)) {
        goto ReserveCurrentMappingsEnd;
    }

    for (DirectoryIndex = (UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT;
         DirectoryIndex < MAX_ULONG >> PAGE_DIRECTORY_SHIFT;
         DirectoryIndex += 1) {

        //
        // Skip the self mappings, which were already marked as reserved.
        //

        if (DirectoryIndex == SelfMappingIndex) {
            continue;
        }

        if (MmKernelPageDirectory[DirectoryIndex].Present == 0) {
            continue;
        }

        //
        // Traverse through the page table pointed to by this directory entry.
        //

        PageTable = GET_PAGE_TABLE(DirectoryIndex);
        for (TableIndex = 0;
             TableIndex < PAGE_SIZE / sizeof(PTE);
             TableIndex += 1) {

            //
            // Skip pages that aren't mapped.
            //

            if (PageTable[TableIndex].Present == 0) {
                continue;
            }

            VirtualAddress = (DirectoryIndex << PAGE_DIRECTORY_SHIFT) +
                             (TableIndex << PAGE_SHIFT);

            //
            // Skip this if the virtual address manager already knows about it.
            //

            RangeFree = MmpIsAccountingRangeFree(&MmKernelVirtualSpace,
                                                 (PVOID)VirtualAddress,
                                                 PAGE_SIZE);

            if (RangeFree == FALSE) {
                continue;
            }

            //
            // Reserve this page, as it is currently mapped.
            //

            MmMdInitDescriptor(&Descriptor,
                               VirtualAddress,
                               VirtualAddress + PAGE_SIZE,
                               MemoryTypeReserved);

            Status = MmpAddAccountingDescriptor(Accountant, &Descriptor);
            if (!KSUCCESS(Status)) {
                goto ReserveCurrentMappingsEnd;
            }
        }
    }

ReserveCurrentMappingsEnd:
    return Status;
}

VOID
MmpMapPage (
    PHYSICAL_ADDRESS PhysicalAddress,
    PVOID VirtualAddress,
    ULONG Flags
    )

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

{

    PKTHREAD CurrentThread;
    volatile PTE *Directory;
    ULONG DirectoryIndex;
    volatile PTE *PageTable;
    PKPROCESS Process;
    ULONG TableIndex;

    CurrentThread = KeGetCurrentThread();
    if (CurrentThread == NULL) {

        ASSERT(VirtualAddress >= KERNEL_VA_START);

        Directory = MmKernelPageDirectory;
        Process = NULL;

    } else {
        Process = CurrentThread->OwningProcess;
        Directory = Process->PageDirectory;
    }

    ASSERT(Directory != NULL);
    ASSERT((UINTN)VirtualAddress + PAGE_SIZE - 1 > (UINTN)VirtualAddress);

    //
    // Assert that the addresses are page aligned.
    //

    ASSERT((PhysicalAddress & PAGE_MASK) == 0);
    ASSERT(((UINTN)VirtualAddress & PAGE_MASK) == 0);

    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    PageTable = GET_PAGE_TABLE(DirectoryIndex);
    TableIndex = ((UINTN)VirtualAddress & PTE_INDEX_MASK) >> PAGE_SHIFT;

    //
    // If no page table exists for this entry, allocate and initialize one.
    //

    if (Directory[DirectoryIndex].Present == 0) {
        MmpCreatePageTable(Directory, VirtualAddress);
    }

    ASSERT((PageTable[TableIndex].Present == 0) &&
           (PageTable[TableIndex].Entry == 0));

    *((PULONG)(&(PageTable[TableIndex]))) = 0;
    PageTable[TableIndex].Entry = (ULONG)PhysicalAddress >> PAGE_SHIFT;
    if ((Flags & MAP_FLAG_READ_ONLY) == 0) {
        PageTable[TableIndex].Writable = 1;
    }

    if ((Flags & MAP_FLAG_CACHE_DISABLE) != 0) {

        ASSERT((Flags & MAP_FLAG_WRITE_THROUGH) == 0);

        PageTable[TableIndex].CacheDisabled = 1;

    } else if ((Flags & MAP_FLAG_WRITE_THROUGH) != 0) {
        PageTable[TableIndex].WriteThrough = 1;
    }

    if ((Flags & MAP_FLAG_LARGE_PAGE) != 0) {
        PageTable[TableIndex].LargePage = 1;
    }

    if ((Flags & MAP_FLAG_USER_MODE) != 0) {

        ASSERT(VirtualAddress < KERNEL_VA_START);

        PageTable[TableIndex].User = 1;

    } else {
        PageTable[TableIndex].Global = 1;
    }

    if ((Flags & MAP_FLAG_DIRTY) != 0) {
        PageTable[TableIndex].Dirty = 1;
    }

    //
    // TLB entry invalidation is not required when transitioning a PTE's
    // present bit from 0 to 1 as long as it was invalidated the last time it
    // went from 1 to 0. The invalidation on a 1 to 0 transition is, however,
    // required as the physical page may be immediately re-used.
    //

    if ((Flags & MAP_FLAG_PRESENT) != 0) {
        PageTable[TableIndex].Present = 1;
    }

    if (VirtualAddress < KERNEL_VA_START) {
        MmpUpdateResidentSetCounter(Process, 1);
    }

    return;
}

VOID
MmpUnmapPages (
    PVOID VirtualAddress,
    ULONG PageCount,
    ULONG UnmapFlags,
    PBOOL PageWasDirty
    )

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

{

    BOOL ChangedSomething;
    PVOID CurrentVirtual;
    volatile PTE *Directory;
    ULONG DirectoryIndex;
    BOOL InvalidateTlb;
    INTN MappedCount;
    ULONG PageNumber;
    volatile PTE *PageTable;
    BOOL PageWasPresent;
    PHYSICAL_ADDRESS PhysicalPage;
    PKPROCESS Process;
    PHYSICAL_ADDRESS RunPhysicalPage;
    UINTN RunSize;
    ULONG TableIndex;
    PKTHREAD Thread;

    ChangedSomething = FALSE;
    InvalidateTlb = TRUE;
    Thread = KeGetCurrentThread();
    if (Thread == NULL) {

        ASSERT(VirtualAddress >= KERNEL_VA_START);
        ASSERT(((UINTN)VirtualAddress + (PageCount << MmPageShift())) - 1 >
               (UINTN)VirtualAddress);

        Directory = MmKernelPageDirectory;
        Process = NULL;

    } else {
        Process = Thread->OwningProcess;
        Directory = Process->PageDirectory;

        //
        // If there's only one thread in the process and this is not a kernel
        // mode address, then there's no need to send a TLB invalidate IPI.
        //

        if ((Process->ThreadCount <= 1) && (VirtualAddress < KERNEL_VA_START)) {
            UnmapFlags &= ~UNMAP_FLAG_SEND_INVALIDATE_IPI;
            if (Process->ThreadCount == 0) {
                InvalidateTlb = FALSE;
            }
        }
    }

    ASSERT(((UINTN)VirtualAddress & PAGE_MASK) == 0);

    //
    // Loop through once to turn them all off. Other processors may still have
    // TLB mappings to them, so the page is technically still in use.
    //

    MappedCount = 0;
    CurrentVirtual = VirtualAddress;
    for (PageNumber = 0; PageNumber < PageCount; PageNumber += 1) {
        DirectoryIndex = (UINTN)CurrentVirtual >> PAGE_DIRECTORY_SHIFT;

        //
        // There's a chance that this routine is unmapping some memory set up
        // in another process that this process has never touched. Check to see
        // if the kernel page directory has an entry, and update this directory
        // if so.
        //

        if (Directory[DirectoryIndex].Present == 0) {
            Directory[DirectoryIndex] = MmKernelPageDirectory[DirectoryIndex];
        }

        //
        // Skip it if there's still no page table there.
        //

        if (Directory[DirectoryIndex].Present == 0) {
            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        PageTable = GET_PAGE_TABLE(DirectoryIndex);
        TableIndex = ((UINTN)CurrentVirtual & PTE_INDEX_MASK) >> PAGE_SHIFT;

        //
        // If the page was not present or physical pages aren't being freed,
        // just wipe the whole PTE out.
        //

        if (PageTable[TableIndex].Entry != 0) {
            PageWasPresent = FALSE;
            if (PageTable[TableIndex].Present != 0) {
                ChangedSomething = TRUE;
                PageWasPresent = TRUE;
            }

            MappedCount += 1;
            if (((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) == 0) &&
                (PageWasDirty == NULL)) {

                *((PULONG)(&(PageTable[TableIndex]))) = 0;

            //
            // Otherwise, preserve the entry so the physical page can be freed
            // below.
            //

            } else {
                PageTable[TableIndex].Present = 0;
            }

            //
            // If an IPI is not going to be sent, clear the TLB entries on this
            // processor as they're unmapped, unless this is a user mode
            // address for a dying process (i.e. a process with no threads) or
            // the page was not actually mapped.
            //

            if ((PageWasPresent != FALSE) &&
                (InvalidateTlb != FALSE) &&
                ((UnmapFlags & UNMAP_FLAG_SEND_INVALIDATE_IPI) == 0)) {

                ArInvalidateTlbEntry(CurrentVirtual);
            }

        } else {

            ASSERT(PageTable[TableIndex].Present == 0);
        }

        CurrentVirtual += PAGE_SIZE;
    }

    //
    // Send the invalidate IPI to get everyone faulting. After this the pages
    // can be taken offline.
    //

    if ((ChangedSomething != FALSE) &&
        ((UnmapFlags & UNMAP_FLAG_SEND_INVALIDATE_IPI) != 0)) {

        MmpSendTlbInvalidateIpi((PVOID)Directory, VirtualAddress, PageCount);
    }

    //
    // Loop through again to free the physical pages or check if things were
    // dirty or writable.
    //

    if ((PageWasDirty != NULL) ||
        ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0)) {

        if (PageWasDirty != NULL) {
            *PageWasDirty = FALSE;
        }

        RunSize = 0;
        RunPhysicalPage = INVALID_PHYSICAL_ADDRESS;
        CurrentVirtual = VirtualAddress;
        for (PageNumber = 0; PageNumber < PageCount; PageNumber += 1) {
            DirectoryIndex = (UINTN)CurrentVirtual >> PAGE_DIRECTORY_SHIFT;
            if (Directory[DirectoryIndex].Present == 0) {
                CurrentVirtual += PAGE_SIZE;
                continue;
            }

            PageTable = GET_PAGE_TABLE(DirectoryIndex);
            TableIndex = ((UINTN)CurrentVirtual & PTE_INDEX_MASK) >> PAGE_SHIFT;
            if (PageTable[TableIndex].Entry == 0) {
                CurrentVirtual += PAGE_SIZE;
                continue;
            }

            if ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0) {
                PhysicalPage =
                            (ULONG)(PageTable[TableIndex].Entry << PAGE_SHIFT);

                if (RunSize != 0) {
                    if ((RunPhysicalPage + RunSize) == PhysicalPage) {
                        RunSize += PAGE_SIZE;

                    } else {
                        MmFreePhysicalPages(RunPhysicalPage,
                                            RunSize >> PAGE_SHIFT);

                        RunPhysicalPage = PhysicalPage;
                        RunSize = PAGE_SIZE;
                    }

                } else {
                    RunPhysicalPage = PhysicalPage;
                    RunSize = PAGE_SIZE;
                }
            }

            if ((PageWasDirty != NULL) && (PageTable[TableIndex].Dirty != 0)) {
                *PageWasDirty = TRUE;
            }

            *((PULONG)(&(PageTable[TableIndex]))) = 0;
            CurrentVirtual += PAGE_SIZE;
        }

        if (RunSize != 0) {
            MmFreePhysicalPages(RunPhysicalPage, RunSize >> PAGE_SHIFT);
        }
    }

    if (VirtualAddress < KERNEL_VA_START) {
        MmpUpdateResidentSetCounter(Process, -MappedCount);
    }

    return;
}

PHYSICAL_ADDRESS
MmpVirtualToPhysical (
    PVOID VirtualAddress,
    PULONG Attributes
    )

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

{

    volatile PTE *Directory;
    ULONG DirectoryIndex;
    volatile PTE *PageTable;
    PHYSICAL_ADDRESS PhysicalAddress;
    PKPROCESS Process;
    volatile PTE *ProcessPageDirectory;
    ULONG TableIndex;

    Process = PsGetCurrentProcess();
    ProcessPageDirectory = NULL;
    if (Process != NULL) {
        ProcessPageDirectory = Process->PageDirectory;
    }

    if (Attributes != NULL) {
        *Attributes = 0;
    }

    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    if (VirtualAddress >= KERNEL_VA_START) {
        Directory = MmKernelPageDirectory;

        //
        // Sync the current page directory to the kernel page directory.
        //

        if (ProcessPageDirectory != NULL) {
            ProcessPageDirectory[DirectoryIndex] =
                                         MmKernelPageDirectory[DirectoryIndex];
        }

    } else {
        if (Process == NULL) {
            return INVALID_PHYSICAL_ADDRESS;
        }

        Directory = Process->PageDirectory;
    }

    if (Directory[DirectoryIndex].Present == 0) {
        return INVALID_PHYSICAL_ADDRESS;
    }

    PageTable = GET_PAGE_TABLE(DirectoryIndex);
    TableIndex = ((UINTN)VirtualAddress & PTE_INDEX_MASK) >> PAGE_SHIFT;
    if (PageTable[TableIndex].Entry == 0) {

        ASSERT(PageTable[TableIndex].Present == 0);

        return INVALID_PHYSICAL_ADDRESS;
    }

    PhysicalAddress = (UINTN)(PageTable[TableIndex].Entry << PAGE_SHIFT) +
                             ((UINTN)VirtualAddress & PAGE_MASK);

    if (Attributes != NULL) {
        if (PageTable[TableIndex].Present != 0) {
            *Attributes |= MAP_FLAG_PRESENT | MAP_FLAG_EXECUTE;
        }

        if (PageTable[TableIndex].Writable == 0) {
            *Attributes |= MAP_FLAG_READ_ONLY;
        }

        if (PageTable[TableIndex].Dirty != 0) {
            *Attributes |= MAP_FLAG_DIRTY;
        }
    }

    return PhysicalAddress;
}

PHYSICAL_ADDRESS
MmpVirtualToPhysicalInOtherProcess (
    PVOID PageDirectory,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine returns the physical address corresponding to the given
    virtual address that belongs to another process.

Arguments:

    PageDirectory - Supplies a pointer to the top level page directory for the
        process.

    VirtualAddress - Supplies the address to translate to a physical address.

Return Value:

    Returns the physical address corresponding to the virtual address, or NULL
    if no mapping could be found.

--*/

{

    volatile PTE *Directory;
    ULONG DirectoryIndex;
    RUNLEVEL OldRunLevel;
    volatile PTE *PageTable;
    ULONG PageTableIndex;
    PHYSICAL_ADDRESS PageTablePhysical;
    PHYSICAL_ADDRESS PhysicalAddress;
    PPROCESSOR_BLOCK ProcessorBlock;
    volatile PTE *ProcessPageDirectory;

    ProcessPageDirectory = PageDirectory;
    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    if (VirtualAddress >= KERNEL_VA_START) {
        Directory = MmKernelPageDirectory;

        //
        // Sync the current page directory to the kernel page directory.
        //

        if (ProcessPageDirectory != NULL) {
            ProcessPageDirectory[DirectoryIndex] =
                                         MmKernelPageDirectory[DirectoryIndex];
        }

    } else {
        Directory = ProcessPageDirectory;
    }

    if (Directory[DirectoryIndex].Present == 0) {
        return INVALID_PHYSICAL_ADDRESS;
    }

    PageTablePhysical = (ULONG)(Directory[DirectoryIndex].Entry << PAGE_SHIFT);
    PageTableIndex = ((UINTN)VirtualAddress & PTE_INDEX_MASK) >> PAGE_SHIFT;

    //
    // Map the page table at dispatch level to avoid bouncing around to
    // different processors and creating TLB entries that will have to be
    // IPIed out.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    MmpMapPage(PageTablePhysical,
               ProcessorBlock->SwapPage,
               MAP_FLAG_PRESENT | MAP_FLAG_READ_ONLY);

    PageTable = (volatile PTE *)(ProcessorBlock->SwapPage);
    if (PageTable[PageTableIndex].Entry == 0) {
        PhysicalAddress = INVALID_PHYSICAL_ADDRESS;

    } else {
        PhysicalAddress =
                       (UINTN)(PageTable[PageTableIndex].Entry << PAGE_SHIFT) +
                       ((UINTN)VirtualAddress & PAGE_MASK);
    }

    MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
    KeLowerRunLevel(OldRunLevel);
    return PhysicalAddress;
}

VOID
MmpUnmapPageInOtherProcess (
    PVOID Process,
    PVOID VirtualAddress,
    ULONG UnmapFlags,
    PBOOL PageWasDirty
    )

/*++

Routine Description:

    This routine unmaps a page of VA space from this process or another.

Arguments:

    Process - Supplies a pointer to the process to unmap the page in.

    VirtualAddress - Supplies the virtual address of the page to unmap.

    UnmapFlags - Supplies a bitmask of flags for the unmap operation. See
        UNMAP_FLAG_* for definitions. This routine will always send an IPI
        after doing the unmap.

    PageWasDirty - Supplies an optional pointer where a boolean will be
        returned indicating if the pages were dirty.

Return Value:

    None.

--*/

{

    PPTE Directory;
    ULONG DirectoryIndex;
    RUNLEVEL OldRunLevel;
    volatile PTE *PageTable;
    PTE PageTableEntry;
    ULONG PageTableIndex;
    PHYSICAL_ADDRESS PageTablePhysical;
    PHYSICAL_ADDRESS PhysicalAddress;
    PPROCESSOR_BLOCK ProcessorBlock;
    PKPROCESS TypedProcess;

    //
    // Assert that this is called at low level. If it ever needs to be called
    // at dispatch, then all acquisitions of the page table spin lock will need
    // to be changed to raise to dispatch before acquiring.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (PageWasDirty != NULL) {
        *PageWasDirty = FALSE;
    }

    TypedProcess = Process;
    Directory = TypedProcess->PageDirectory;
    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    if (Directory[DirectoryIndex].Present == 0) {
        goto UnmapPageInOtherProcessEnd;
    }

    PageTablePhysical = (UINTN)(Directory[DirectoryIndex].Entry << PAGE_SHIFT);
    PageTableIndex = ((UINTN)VirtualAddress & PTE_INDEX_MASK) >> PAGE_SHIFT;

    //
    // Map the page table at dispatch level to avoid bouncing around to
    // different processors and creating TLB entries that will have to be
    // IPIed out.
    //

    PageTableEntry.Entry = 0;
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    MmpMapPage(PageTablePhysical, ProcessorBlock->SwapPage, MAP_FLAG_PRESENT);
    PageTable = (volatile PTE *)(ProcessorBlock->SwapPage);
    if (PageTable[PageTableIndex].Entry != 0) {

        //
        // Invalidate the TLB everywhere before reading the page table entry,
        // as the PTE could become dirty at any time if the mapping is valid.
        //

        if (PageTable[PageTableIndex].Present != 0) {
            PageTable[PageTableIndex].Present = 0;
            MmpSendTlbInvalidateIpi(Directory, VirtualAddress, 1);
        }

        PageTableEntry = PageTable[PageTableIndex];
        *((PULONG)&(PageTable[PageTableIndex])) = 0;

    } else {

        ASSERT(PageTable[PageTableIndex].Present == 0);

    }

    MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
    KeLowerRunLevel(OldRunLevel);

    //
    // Exit immediately if there was no entry. There is no page to release.
    //

    if (PageTableEntry.Entry == 0) {
        goto UnmapPageInOtherProcessEnd;
    }

    if ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0) {
        PhysicalAddress = (ULONG)(PageTableEntry.Entry << PAGE_SHIFT);
        MmFreePhysicalPage(PhysicalAddress);
    }

    if ((PageWasDirty != NULL) && (PageTableEntry.Dirty != 0)) {
        *PageWasDirty = TRUE;
    }

    ASSERT(VirtualAddress < KERNEL_VA_START);

    MmpUpdateResidentSetCounter(Process, -1);

UnmapPageInOtherProcessEnd:
    return;
}

VOID
MmpMapPageInOtherProcess (
    PVOID Process,
    PHYSICAL_ADDRESS PhysicalAddress,
    PVOID VirtualAddress,
    ULONG MapFlags,
    BOOL SendTlbInvalidateIpi
    )

/*++

Routine Description:

    This routine maps a physical page of memory into the virtual address space
    of another process.

Arguments:

    Process - Supplies a pointer to the process to map the page in.

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

{

    PPTE Directory;
    ULONG DirectoryIndex;
    INTN MappedCount;
    RUNLEVEL OldRunLevel;
    volatile PTE *PageTable;
    ULONG PageTableIndex;
    PHYSICAL_ADDRESS PageTablePhysical;
    PPROCESSOR_BLOCK ProcessorBlock;
    PKPROCESS TypedProcess;

    //
    // Assert that this is called at low level. If it ever needs to be called
    // at dispatch, then all acquisitions of the page table spin lock will need
    // to be changed to raise to dispatch before acquiring.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    TypedProcess = Process;
    Directory = TypedProcess->PageDirectory;
    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;

    //
    // Create a page table if nothing is there.
    //

    if (Directory[DirectoryIndex].Present == 0) {
        MmpCreatePageTable(Directory, VirtualAddress);
    }

    PageTablePhysical = (UINTN)(Directory[DirectoryIndex].Entry << PAGE_SHIFT);
    PageTableIndex = ((UINTN)VirtualAddress & PTE_INDEX_MASK) >> PAGE_SHIFT;

    //
    // Map the page table at dispatch level to avoid bouncing around to
    // different processors and creating TLB entries that will have to be
    // IPIed out.
    //

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    MmpMapPage(PageTablePhysical, ProcessorBlock->SwapPage, MAP_FLAG_PRESENT);
    PageTable = (volatile PTE *)(ProcessorBlock->SwapPage);

    //
    // This VA better be unmapped unless the caller requested an TLB
    // invalidation.
    //

    if (PageTable[PageTableIndex].Entry != 0) {
        MappedCount = 0;

        ASSERT(SendTlbInvalidateIpi != FALSE);

        if (PageTable[PageTableIndex].Present == 0) {
            SendTlbInvalidateIpi = FALSE;
        }

    } else {
        MappedCount = 1;
        SendTlbInvalidateIpi = FALSE;

        ASSERT(PageTable[PageTableIndex].Present == 0);
    }

    *((PULONG)&(PageTable[PageTableIndex])) = 0;
    PageTable[PageTableIndex].Entry = (ULONG)PhysicalAddress >> PAGE_SHIFT;
    if ((MapFlags & MAP_FLAG_READ_ONLY) == 0) {
        PageTable[PageTableIndex].Writable = 1;
    }

    if ((MapFlags & MAP_FLAG_WRITE_THROUGH) != 0) {
        PageTable[PageTableIndex].WriteThrough = 1;
    }

    if ((MapFlags & MAP_FLAG_CACHE_DISABLE) != 0) {
        PageTable[PageTableIndex].CacheDisabled = 1;
    }

    if ((MapFlags & MAP_FLAG_LARGE_PAGE) != 0) {
        PageTable[PageTableIndex].LargePage = 1;
    }

    ASSERT(((MapFlags & MAP_FLAG_USER_MODE) == 0) ||
           (VirtualAddress < KERNEL_VA_START));

    if ((MapFlags & MAP_FLAG_USER_MODE) != 0) {
        PageTable[PageTableIndex].User = 1;

    } else {

        ASSERT((MapFlags & MAP_FLAG_GLOBAL) != 0);

        PageTable[PageTableIndex].Global = 1;
    }

    //
    // When mapping something in another process, assume that it's dirty. You
    // don't know where that page came from.
    //

    PageTable[PageTableIndex].Dirty = 1;
    if ((MapFlags & MAP_FLAG_PRESENT) != 0) {
        PageTable[PageTableIndex].Present = 1;
    }

    MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
    KeLowerRunLevel(OldRunLevel);

    //
    // If requested, send a TLB invalidate IPI. This routine can be used for
    // remap, in which case the virtual address never got invalidated.
    //

    if (SendTlbInvalidateIpi != FALSE) {
        MmpSendTlbInvalidateIpi(Directory, VirtualAddress, 1);
    }

    ASSERT(VirtualAddress < KERNEL_VA_START);

    if (MappedCount != 0) {
        MmpUpdateResidentSetCounter(Process, MappedCount);
    }

    return;
}

VOID
MmpChangeMemoryRegionAccess (
    PVOID VirtualAddress,
    ULONG PageCount,
    ULONG MapFlags,
    ULONG MapFlagsMask
    )

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

{

    BOOL ChangedSomething;
    BOOL ChangedSomethingThisRound;
    PVOID CurrentVirtual;
    volatile PTE *Directory;
    ULONG DirectoryIndex;
    BOOL InvalidateTlb;
    ULONG PageIndex;
    PPTE PageTable;
    ULONG PageTableIndex;
    BOOL Present;
    PKPROCESS Process;
    volatile PTE *ProcessPageDirectory;
    BOOL SendInvalidateIpi;
    BOOL Writable;

    InvalidateTlb = TRUE;
    Process = PsGetCurrentProcess();
    ProcessPageDirectory = Process->PageDirectory;
    SendInvalidateIpi = TRUE;
    if (VirtualAddress >= KERNEL_VA_START) {
        Directory = MmKernelPageDirectory;

    } else {
        Directory = ProcessPageDirectory;

        //
        // If there's only one thread in the process, then there's no need to
        // send a TLB invalidate IPI for this user mode address.
        //

        if (Process->ThreadCount <= 1) {
            SendInvalidateIpi = FALSE;
            if (Process->ThreadCount == 0) {
                InvalidateTlb = FALSE;
            }
        }
    }

    ChangedSomething = FALSE;
    Writable = ((MapFlags & MAP_FLAG_READ_ONLY) == 0);
    Present = ((MapFlags & MAP_FLAG_PRESENT) != 0);
    CurrentVirtual = VirtualAddress;
    for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
        DirectoryIndex = (UINTN)CurrentVirtual >> PAGE_DIRECTORY_SHIFT;

        //
        // Sync the current directory entry to the kernel.
        //

        if (CurrentVirtual >= KERNEL_VA_START) {
            ProcessPageDirectory[DirectoryIndex] =
                                         MmKernelPageDirectory[DirectoryIndex];
        }

        PageTableIndex = ((UINTN)CurrentVirtual & PTE_INDEX_MASK) >> PAGE_SHIFT;
        if (Directory[DirectoryIndex].Present == 0) {
            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        PageTable = GET_PAGE_TABLE(DirectoryIndex);
        if (PageTable[PageTableIndex].Entry == 0) {

            ASSERT(PageTable[PageTableIndex].Present == 0);

            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        //
        // Set the new attributes.
        //

        ChangedSomethingThisRound = FALSE;
        if (((MapFlagsMask & MAP_FLAG_READ_ONLY) != 0) &&
            (PageTable[PageTableIndex].Writable != Writable)) {

            ChangedSomethingThisRound = TRUE;
            PageTable[PageTableIndex].Writable = Writable;
        }

        if (((MapFlagsMask & MAP_FLAG_PRESENT) != 0) &&
            (PageTable[PageTableIndex].Present != Present)) {

            //
            // Negative PTEs aren't cached, so only going from present to not
            // present counts as a change.
            //

            if (Present == FALSE) {
                ChangedSomethingThisRound = TRUE;
            }

            PageTable[PageTableIndex].Present = Present;
        }

        if (ChangedSomethingThisRound != FALSE) {
            if (SendInvalidateIpi == FALSE) {
                if (InvalidateTlb != FALSE) {
                    ArInvalidateTlbEntry(CurrentVirtual);
                }

            } else {
                ChangedSomething = TRUE;
            }
        }

        CurrentVirtual += PAGE_SIZE;
    }

    //
    // Send an invalidate IPI if any mappings were changed.
    //

    if (ChangedSomething != FALSE) {

        ASSERT(SendInvalidateIpi != FALSE);

        MmpSendTlbInvalidateIpi((PVOID)Directory, VirtualAddress, PageCount);
    }

    return;
}

KSTATUS
MmpPreallocatePageTables (
    PVOID SourcePageDirectory,
    PVOID DestinationPageDirectory
    )

/*++

Routine Description:

    This routine allocates, but does not initialize nor fully map the page
    tables for a process that is being forked. It is needed because physical
    page allocations are not allowed while an image section lock is held.

Arguments:

    SourcePageDirectory - Supplies a pointer to the page directory to scan. A
        page table is allocated but not initialized for every missing page
        table in the destination.

    DestinationPageDirectory - Supplies a pointer to the page directory that
        will get page tables filled in.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on failure.

--*/

{

    ULONG DeleteIndex;
    PPTE Destination;
    ULONG DirectoryIndex;
    PHYSICAL_ADDRESS Physical;
    PPTE Source;

    Destination = DestinationPageDirectory;
    Source = SourcePageDirectory;
    for (DirectoryIndex = 0;
         DirectoryIndex < ((UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT);
         DirectoryIndex += 1) {

        if (Source[DirectoryIndex].Present == 0) {
            continue;
        }

        ASSERT(Destination[DirectoryIndex].Present == 0);

        Physical = MmpAllocatePhysicalPages(1, 0);
        if (Physical == INVALID_PHYSICAL_ADDRESS) {

            //
            // Clean up and fail.
            //

            for (DeleteIndex = 0;
                 DeleteIndex < DirectoryIndex;
                 DeleteIndex += 1) {

                if (Source[DeleteIndex].Present != 0) {
                    Physical = Destination[DeleteIndex].Entry << PAGE_SHIFT;
                    Destination[DeleteIndex].Entry = 0;
                    MmFreePhysicalPage(Physical);
                }
            }

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Destination[DirectoryIndex].Entry = Physical >> PAGE_SHIFT;
    }

    return STATUS_SUCCESS;
}

KSTATUS
MmpCopyAndChangeSectionMappings (
    PKPROCESS DestinationProcess,
    PVOID SourcePageDirectory,
    PVOID VirtualAddress,
    UINTN Size
    )

/*++

Routine Description:

    This routine converts all the mappings of the given virtual address region
    to read-only, and copies those read-only mappings to another process.

Arguments:

    DestinationProcess - Supplies a pointer to the process where the mappings
        should be copied to.

    SourcePageDirectory - Supplies the top level page table of the current
        process.

    VirtualAddress - Supplies the starting virtual address of the memory range.

    Size - Supplies the size of the virtual address region, in bytes.

Return Value:

    None.

--*/

{

    PVOID CurrentVirtual;
    PPTE DestinationDirectory;
    PPTE DestinationTable;
    ULONG DirectoryIndex;
    ULONG DirectoryIndexEnd;
    ULONG DirectoryIndexStart;
    INTN MappedCount;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PageTable;
    PPROCESSOR_BLOCK ProcessorBlock;
    PPTE SourceDirectory;
    PPTE SourceTable;
    ULONG TableIndex;
    ULONG TableIndexEnd;
    ULONG TableIndexStart;
    PVOID VirtualEnd;

    DestinationDirectory = DestinationProcess->PageDirectory;
    SourceDirectory = SourcePageDirectory;
    VirtualEnd = VirtualAddress + Size;

    ASSERT(VirtualEnd > VirtualAddress);
    ASSERT(DestinationProcess->ThreadCount == 0);

    //
    // It is assumed that all image sections are page aligned in base address
    // and size.
    //

    ASSERT(IS_POINTER_ALIGNED(VirtualAddress, PAGE_SIZE) != FALSE);
    ASSERT(IS_POINTER_ALIGNED(VirtualEnd, PAGE_SIZE) != FALSE);

    //
    // Iterate over the source directory looking for valid entries. For each
    // valid entry, create a page table in the destination (if necessary), and
    // copy the page table entries for the given virtual address region.
    //

    MappedCount = 0;
    CurrentVirtual = VirtualAddress;
    DirectoryIndexEnd = (UINTN)VirtualEnd >> PAGE_DIRECTORY_SHIFT;
    DirectoryIndexStart = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    for (DirectoryIndex = DirectoryIndexStart;
         DirectoryIndex <= DirectoryIndexEnd;
         DirectoryIndex += 1) {

        //
        // If the source directory does not have this page table, then skip it.
        //

        if (SourceDirectory[DirectoryIndex].Present == 0) {
            continue;
        }

        //
        // Determine the start and end page table indices that will need to be
        // synchronized.
        //

        TableIndexStart = ((UINTN)CurrentVirtual & PTE_INDEX_MASK) >>
                          PAGE_SHIFT;

        //
        // If this is not the last page directory entry, then this VA
        // region should run to the last index of this PTE.
        //

        if (DirectoryIndex < DirectoryIndexEnd) {
            TableIndexEnd = PAGE_SIZE / sizeof(PTE);

            //
            // As this is not the last time around the loop, up the current
            // virtual address.
            //

            CurrentVirtual += (TableIndexEnd - TableIndexStart) << PAGE_SHIFT;

        //
        // Otherwise the PTE index of the end virtual address should do.
        //

        } else {
            TableIndexEnd = ((UINTN)VirtualEnd & PTE_INDEX_MASK) >> PAGE_SHIFT;
        }

        //
        // If the destination has not encountered this directory entry yet,
        // allocate a page table for the destination. Then proceed to copy the
        // contents of the source page table over.
        //

        if (DestinationDirectory[DirectoryIndex].Present == 0) {

            //
            // The preallocation step better have set up a page table to use.
            // Allocations are not possible in this routine because an image
            // section lock is held, which means the paging out thread could
            // be blocked trying to get it, and there could be no free pages
            // left.
            //

            PageTable = DestinationDirectory[DirectoryIndex].Entry <<
                        PAGE_SHIFT;

            ASSERT(PageTable != 0);

            SourceTable = GET_PAGE_TABLE(DirectoryIndex);
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            ProcessorBlock = KeGetCurrentProcessorBlock();
            DestinationTable = ProcessorBlock->SwapPage;
            MmpMapPage(PageTable, DestinationTable, MAP_FLAG_PRESENT);
            if (TableIndexStart != 0) {
                RtlZeroMemory(DestinationTable, TableIndexStart * sizeof(PTE));
            }

            //
            // Copy the contents for the given VA region from the source to
            // the destination, while modifying the source to be read only.
            // Do not flush the TLB during modifications. One giant TLB flush
            // is executed at the end of copying all these VA regions.
            //

            for (TableIndex = TableIndexStart;
                 TableIndex < TableIndexEnd;
                 TableIndex += 1) {

                if (SourceTable[TableIndex].Entry != 0) {
                    MappedCount += 1;
                    *((PULONG)&(SourceTable[TableIndex])) &= ~PTE_FLAG_WRITABLE;
                    *((PULONG)&(DestinationTable[TableIndex])) =
                       *((PULONG)&(SourceTable[TableIndex])) & ~PTE_FLAG_DIRTY;

                } else {
                    *((PULONG)&(DestinationTable[TableIndex])) = 0;
                }
            }

            if (TableIndexEnd != (PAGE_SIZE / sizeof(PTE))) {
                RtlZeroMemory(&(DestinationTable[TableIndexEnd]),
                              (PAGE_SIZE - (TableIndexEnd * sizeof(PTE))));
            }

            MmpUnmapPages(DestinationTable, 1, 0, NULL);
            KeLowerRunLevel(OldRunLevel);

            //
            // Insert the newly initialized page table into the page directory.
            //

            DestinationDirectory[DirectoryIndex].Entry =
                                                (ULONG)PageTable >> PAGE_SHIFT;

            DestinationDirectory[DirectoryIndex].Writable = 1;
            DestinationDirectory[DirectoryIndex].User = 1;
            DestinationDirectory[DirectoryIndex].Present = 1;

        //
        // If the destination already has a page table allocated at this
        // location, then just synchronize the source and destination tables
        // for the given region.
        //

        } else {
            PageTable = DestinationDirectory[DirectoryIndex].Entry <<
                        PAGE_SHIFT;

            ASSERT(PageTable != INVALID_PHYSICAL_ADDRESS);

            SourceTable = GET_PAGE_TABLE(DirectoryIndex);
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            ProcessorBlock = KeGetCurrentProcessorBlock();
            DestinationTable = ProcessorBlock->SwapPage;
            MmpMapPage(PageTable, DestinationTable, MAP_FLAG_PRESENT);
            for (TableIndex = TableIndexStart;
                 TableIndex < TableIndexEnd;
                 TableIndex += 1) {

                if (SourceTable[TableIndex].Entry != 0) {
                    MappedCount += 1;
                    *((PULONG)&(SourceTable[TableIndex])) &= ~PTE_FLAG_WRITABLE;
                    *((PULONG)&(DestinationTable[TableIndex])) =
                       *((PULONG)&(SourceTable[TableIndex])) & ~PTE_FLAG_DIRTY;
                }
            }

            MmpUnmapPages(DestinationTable, 1, 0, NULL);
            KeLowerRunLevel(OldRunLevel);
        }
    }

    ASSERT(VirtualAddress < KERNEL_VA_START);

    if (MappedCount != 0) {
        MmpUpdateResidentSetCounter(DestinationProcess, MappedCount);
    }

    return STATUS_SUCCESS;
}

VOID
MmpCreatePageTables (
    PVOID VirtualAddress,
    UINTN Size
    )

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

{

    PKTHREAD CurrentThread;
    volatile PTE *Directory;
    ULONG DirectoryEndIndex;
    UINTN DirectoryIndex;

    CurrentThread = KeGetCurrentThread();
    if (CurrentThread == NULL) {
        Directory = MmKernelPageDirectory;
        if (Directory == NULL) {
            return;
        }

    } else {
        Directory = CurrentThread->OwningProcess->PageDirectory;
    }

    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    DirectoryEndIndex = ((UINTN)VirtualAddress + Size - 1) >>
                        PAGE_DIRECTORY_SHIFT;

    ASSERT(DirectoryIndex <= DirectoryEndIndex);

    while (DirectoryIndex <= DirectoryEndIndex) {
        if (Directory[DirectoryIndex].Present == 0) {
            MmpCreatePageTable(Directory,
                               (PVOID)(DirectoryIndex << PAGE_DIRECTORY_SHIFT));
        }

        DirectoryIndex += 1;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
MmpCreatePageTable (
    volatile PTE *Directory,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine creates a page table for the given directory and virtual
    address.

Arguments:

    Directory - Supplies a pointer to the page directory that will own the
        page table.

    VirtualAddress - Supplies the virtual address that the page table will
        eventually service.

Return Value:

    None.

--*/

{

    ULONG DirectoryIndex;
    PHYSICAL_ADDRESS NewPageTable;
    BOOL NewPageTableUsed;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    NewPageTableUsed = FALSE;

    //
    // Sync the current page directory with the kernel page directory.
    //

    if ((VirtualAddress >= KERNEL_VA_START) &&
        (MmKernelPageDirectory[DirectoryIndex].Entry !=
         Directory[DirectoryIndex].Entry)) {

        ASSERT(Directory[DirectoryIndex].Entry == 0);

        Directory[DirectoryIndex] = MmKernelPageDirectory[DirectoryIndex];
    }

    //
    // If the page table entry is now present, then return immediately.
    //

    if (Directory[DirectoryIndex].Present != 0) {
        return;
    }

    //
    // If there is no kernel page directory entry, then a new page table needs
    // to be allocated. Create calls that require more than just
    // synchronization better be called at low level.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (Directory[DirectoryIndex].Entry != 0) {
        NewPageTable = Directory[DirectoryIndex].Entry << PAGE_SHIFT;
        Directory[DirectoryIndex].Entry = 0;

    } else {
        NewPageTable = MmpAllocatePhysicalPages(1, 0);
    }

    ASSERT(NewPageTable != INVALID_PHYSICAL_ADDRESS);

    //
    // Acquire the lock and check the status of the directory entry again.
    //

    if (MmPageTableLock != NULL) {
        KeAcquireQueuedLock(MmPageTableLock);
    }

    //
    // With the lock acquired, sync with the kernel page directory again.
    //

    if ((VirtualAddress >= KERNEL_VA_START) &&
        (MmKernelPageDirectory[DirectoryIndex].Entry !=
         Directory[DirectoryIndex].Entry)) {

        Directory[DirectoryIndex] = MmKernelPageDirectory[DirectoryIndex];
    }

    //
    // If it still is not present, then action needs to be taken. Zero out the
    // page table page and then insert it into the directory. Additionally
    // insert it into the kernel page directory if it is a page table for
    // kernel VA.
    //

    if (Directory[DirectoryIndex].Present == 0) {

        ASSERT((VirtualAddress < KERNEL_VA_START) ||
               (MmKernelPageDirectory[DirectoryIndex].Present == 0));

        //
        // Map the new page table to the staging area and zero it out.
        // Raise to dispatch to avoid creating TLB entries in a bunch of
        // processors that will then have to be IPIed out.
        //

        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        ProcessorBlock = KeGetCurrentProcessorBlock();
        MmpMapPage(NewPageTable, ProcessorBlock->SwapPage, MAP_FLAG_PRESENT);
        RtlZeroMemory(ProcessorBlock->SwapPage, PAGE_SIZE);
        MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
        KeLowerRunLevel(OldRunLevel);
        Directory[DirectoryIndex].Entry = (ULONG)NewPageTable >> PAGE_SHIFT;
        Directory[DirectoryIndex].Writable = 1;
        if (VirtualAddress >= KERNEL_VA_START) {

            ASSERT(MmKernelPageDirectory[DirectoryIndex].Present == 0);

            Directory[DirectoryIndex].Global = 1;
            MmKernelPageDirectory[DirectoryIndex] = Directory[DirectoryIndex];
            MmKernelPageDirectory[DirectoryIndex].Present = 1;

        } else {
            Directory[DirectoryIndex].User = 1;
        }

        Directory[DirectoryIndex].Present = 1;

        //
        // As this is a present bit transition from 0 to 1, for both the
        // PDE and PTE (via self-map) versions of this entry, no TLB
        // invalidation is necessary.
        //
        // Mark the page table as used so it does not get released below.
        //

        NewPageTableUsed = TRUE;
    }

    if (MmPageTableLock != NULL) {
        KeReleaseQueuedLock(MmPageTableLock);
    }

    //
    // If a page table was allocated but not used, then free it.
    //

    if ((NewPageTableUsed == FALSE) &&
        (NewPageTable != INVALID_PHYSICAL_ADDRESS)) {

        MmFreePhysicalPage(NewPageTable);
    }

    return;
}

