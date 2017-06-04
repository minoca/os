/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include <minoca/kernel/x86.h>
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

KSTATUS
MmpCreatePageDirectory (
    PADDRESS_SPACE_X86 AddressSpace
    );

VOID
MmpDestroyPageDirectory (
    PADDRESS_SPACE_X86 AddressSpace
    );

VOID
MmpCreatePageTable (
    PADDRESS_SPACE_X86 AddressSpace,
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
    ULONG MapFlags;

    //
    // Allocate pages starting at address 0x1000.
    //

    *Allocation = (PVOID)(UINTN)IDENTITY_STUB_ADDRESS;
    CurrentAddress = *Allocation;
    MapFlags = MAP_FLAG_PRESENT | MAP_FLAG_EXECUTE;
    if (CurrentAddress >= KERNEL_VA_START) {
        MapFlags |= MAP_FLAG_GLOBAL;
    }

    for (CurrentPage = 0; CurrentPage < PageCount; CurrentPage += 1) {
        MmpMapPage((UINTN)CurrentAddress, CurrentAddress, MapFlags);
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

    ASSERT((UINTN)Allocation == IDENTITY_STUB_ADDRESS);

    //
    // Unmap the pages. Don't "free" the physical pages because they were
    // never recognized as memory.
    //

    MmpUnmapPages(Allocation, PageCount, UNMAP_FLAG_SEND_INVALIDATE_IPI, NULL);
    return;
}

VOID
MmUpdatePageDirectory (
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    UINTN Size
    )

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

{

    ULONG EndIndex;
    PPTE Entry;
    ULONG Index;
    PPTE PageDirectory;
    PADDRESS_SPACE_X86 Space;

    Space = (PADDRESS_SPACE_X86)AddressSpace;
    PageDirectory = Space->PageDirectory;

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

VOID
MmSwitchAddressSpace (
    PVOID Processor,
    PVOID CurrentStack,
    PADDRESS_SPACE AddressSpace
    )

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

{

    ULONG DirectoryIndex;
    PPROCESSOR_BLOCK ProcessorBlock;
    PADDRESS_SPACE_X86 Space;
    PTSS Tss;

    Space = (PADDRESS_SPACE_X86)AddressSpace;

    //
    // Make sure the current stack is visible. It might not be if this current
    // thread is new and its stack pushed out into a new page table not in the
    // destination context.
    //

    DirectoryIndex = (UINTN)CurrentStack >> PAGE_DIRECTORY_SHIFT;
    Space->PageDirectory[DirectoryIndex] =
                                         MmKernelPageDirectory[DirectoryIndex];

    ProcessorBlock = Processor;
    Tss = ProcessorBlock->Tss;

    //
    // Set the CR3 first because an NMI can come in any time and change CR3 to
    // whatever is in the TSS.
    //

    Tss->Cr3 = Space->PageDirectoryPhysical;
    ArSetCurrentPageDirectory(Space->PageDirectoryPhysical);
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
    PMEMORY_DESCRIPTOR Descriptor;
    volatile PTE *Directory;
    ULONG DirectoryIndex;
    ULONG Flags;
    BOOL FreePageTable;
    MEMORY_DESCRIPTOR NewDescriptor;
    volatile PTE *PageTable;
    PHYSICAL_ADDRESS PhysicalAddress;
    PPROCESSOR_BLOCK ProcessorBlock;
    PHYSICAL_ADDRESS RunPhysicalAddress;
    UINTN RunSize;
    KSTATUS Status;
    ULONG TableIndex;

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
            // Take over the first page of physical memory.
            //

            Descriptor = MmMdLookupDescriptor(
                                            Parameters->MemoryMap,
                                            IDENTITY_STUB_ADDRESS,
                                            IDENTITY_STUB_ADDRESS + PAGE_SIZE);

            ASSERT((Descriptor == NULL) ||
                   (Descriptor->Type == MemoryTypeFree));

            MmMdInitDescriptor(&NewDescriptor,
                               IDENTITY_STUB_ADDRESS,
                               IDENTITY_STUB_ADDRESS + PAGE_SIZE,
                               MemoryTypeReserved);

            MmMdAddDescriptorToList(Parameters->MemoryMap, &NewDescriptor);
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
        // By now, all boot mappings should have been unmapped. Loop over the
        // kernel page table's user mode space looking for entries. If there
        // are non-zero entries on a page table, keep the first and second
        // level mappings and the page table. If the page table is entirely
        // clean, free it and destroy the first level entry.
        //

        RunSize = 0;
        RunPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
        Directory = MmKernelPageDirectory;
        for (DirectoryIndex = 0;
             DirectoryIndex < (UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT;
             DirectoryIndex += 1) {

            if (Directory[DirectoryIndex].Entry == 0) {

                ASSERT(Directory[DirectoryIndex].Present == 0);

                continue;
            }

            //
            // A second level table is present, check to see if it is all zeros.
            //

            FreePageTable = TRUE;
            PageTable = GET_PAGE_TABLE(DirectoryIndex);
            for (TableIndex = 0;
                 TableIndex < PAGE_SIZE / sizeof(PTE);
                 TableIndex += 1) {

                if (PageTable[TableIndex].Entry != 0) {
                    FreePageTable = FALSE;
                    break;
                }

                ASSERT(PageTable[TableIndex].Present == 0);
            }

            //
            // Move to the next directory entry if this page table is in use.
            //

            if (FreePageTable == FALSE) {
                continue;
            }

            //
            // Otherwise, update the directory entry and free the page table.
            //

            PhysicalAddress = (ULONG)(Directory[DirectoryIndex].Entry <<
                                      PAGE_SHIFT);

            *((PULONG)(&(Directory[DirectoryIndex]))) = 0;
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

        if (RunSize != 0) {
            MmFreePhysicalPages(RunPhysicalAddress, RunSize >> PAGE_SHIFT);
        }

        Status = STATUS_SUCCESS;

    } else {

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto ArchInitializeEnd;
    }

ArchInitializeEnd:
    return Status;
}

PADDRESS_SPACE
MmpArchCreateAddressSpace (
    VOID
    )

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

{

    PADDRESS_SPACE_X86 Space;
    KSTATUS Status;

    Space = MmAllocateNonPagedPool(sizeof(ADDRESS_SPACE_X86),
                                   MM_ADDRESS_SPACE_ALLOCATION_TAG);

    if (Space == NULL) {
        return NULL;
    }

    RtlZeroMemory(Space, sizeof(ADDRESS_SPACE_X86));
    Status = MmpCreatePageDirectory(Space);
    if (!KSUCCESS(Status)) {
        goto ArchCreateAddressSpaceEnd;
    }

ArchCreateAddressSpaceEnd:
    if (!KSUCCESS(Status)) {
        if (Space != NULL) {
            MmpDestroyPageDirectory(Space);
            MmFreeNonPagedPool(Space);
            Space = NULL;
        }
    }

    return (PADDRESS_SPACE)Space;
}

VOID
MmpArchDestroyAddressSpace (
    PADDRESS_SPACE AddressSpace
    )

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

{

    PADDRESS_SPACE_X86 Space;

    Space = (PADDRESS_SPACE_X86)AddressSpace;
    MmpDestroyPageDirectory(Space);
    MmFreeNonPagedPool(Space);
    return;
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

    PADDRESS_SPACE_X86 AddressSpace;
    PPTE CurrentPageDirectory;
    PKPROCESS CurrentProcess;
    ULONG DirectoryIndex;
    PPTE PageTable;
    ULONG TableIndex;

    //
    // This check only applies to kernel-mode addresses.
    //

    if (FaultingAddress < KERNEL_VA_START) {
        return FALSE;
    }

    CurrentProcess = PsGetCurrentProcess();
    AddressSpace = (PADDRESS_SPACE_X86)(CurrentProcess->AddressSpace);
    CurrentPageDirectory = AddressSpace->PageDirectory;
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

    PADDRESS_SPACE_X86 AddressSpace;
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
        AddressSpace = NULL;

    } else {
        Process = CurrentThread->OwningProcess;
        AddressSpace = (PADDRESS_SPACE_X86)(Process->AddressSpace);
        Directory = AddressSpace->PageDirectory;
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
        MmpCreatePageTable(AddressSpace, Directory, VirtualAddress);
    }

    ASSERT(Directory[DirectoryIndex].Present != 0);
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

    } else if ((Flags & MAP_FLAG_GLOBAL) != 0) {
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
        MmpUpdateResidentSetCounter(&(AddressSpace->Common), 1);
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

    PADDRESS_SPACE_X86 AddressSpace;
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
        AddressSpace = NULL;

    } else {
        Process = Thread->OwningProcess;
        AddressSpace = (PADDRESS_SPACE_X86)(Process->AddressSpace);
        Directory = AddressSpace->PageDirectory;

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

        if ((Directory[DirectoryIndex].Present == 0) &&
            (Directory[DirectoryIndex].Entry == 0)) {

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

        MmpSendTlbInvalidateIpi(&(AddressSpace->Common),
                                VirtualAddress,
                                PageCount);
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
        MmpUpdateResidentSetCounter(&(AddressSpace->Common), -MappedCount);
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

    PADDRESS_SPACE_X86 AddressSpace;
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
        AddressSpace = (PADDRESS_SPACE_X86)(Process->AddressSpace);
        ProcessPageDirectory = AddressSpace->PageDirectory;
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

        Directory = ProcessPageDirectory;
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
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine returns the physical address corresponding to the given
    virtual address that belongs to another process.

Arguments:

    AddressSpace - Supplies a pointer to the address space.

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
    PADDRESS_SPACE_X86 Space;

    Space = (PADDRESS_SPACE_X86)AddressSpace;
    ProcessPageDirectory = Space->PageDirectory;
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
               MAP_FLAG_PRESENT | MAP_FLAG_READ_ONLY | MAP_FLAG_GLOBAL);

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
    PADDRESS_SPACE AddressSpace,
    PVOID VirtualAddress,
    ULONG UnmapFlags,
    PBOOL PageWasDirty
    )

/*++

Routine Description:

    This routine unmaps a page of VA space from this process or another.

Arguments:

    AddressSpace - Supplies a pointer to the address space to unmap the page
        from.

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
    PADDRESS_SPACE_X86 Space;

    //
    // Assert that this is called at low level. If it ever needs to be called
    // at dispatch, then all acquisitions of the page table spin lock will need
    // to be changed to raise to dispatch before acquiring.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (PageWasDirty != NULL) {
        *PageWasDirty = FALSE;
    }

    Space = (PADDRESS_SPACE_X86)AddressSpace;
    Directory = Space->PageDirectory;
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
    MmpMapPage(PageTablePhysical,
               ProcessorBlock->SwapPage,
               MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL);

    PageTable = (volatile PTE *)(ProcessorBlock->SwapPage);
    if (PageTable[PageTableIndex].Entry != 0) {

        //
        // Invalidate the TLB everywhere before reading the page table entry,
        // as the PTE could become dirty at any time if the mapping is valid.
        //

        if (PageTable[PageTableIndex].Present != 0) {
            PageTable[PageTableIndex].Present = 0;
            MmpSendTlbInvalidateIpi(&(Space->Common), VirtualAddress, 1);
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

    MmpUpdateResidentSetCounter(&(Space->Common), -1);

UnmapPageInOtherProcessEnd:
    return;
}

VOID
MmpMapPageInOtherProcess (
    PADDRESS_SPACE AddressSpace,
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

    AddressSpace - Supplies a pointer to the address space to map the page in.

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
    PADDRESS_SPACE_X86 Space;

    //
    // Assert that this is called at low level. If it ever needs to be called
    // at dispatch, then all acquisitions of the page table spin lock will need
    // to be changed to raise to dispatch before acquiring.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Space = (PADDRESS_SPACE_X86)AddressSpace;
    Directory = Space->PageDirectory;
    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;

    //
    // Create a page table if nothing is there.
    //

    if (Directory[DirectoryIndex].Present == 0) {
        MmpCreatePageTable(Space, Directory, VirtualAddress);
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
    MmpMapPage(PageTablePhysical,
               ProcessorBlock->SwapPage,
               MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL);

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

    } else if ((MapFlags & MAP_FLAG_GLOBAL) != 0) {
        PageTable[PageTableIndex].Global = 1;
    }

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
        MmpSendTlbInvalidateIpi(&(Space->Common), VirtualAddress, 1);
    }

    ASSERT(VirtualAddress < KERNEL_VA_START);

    if (MappedCount != 0) {
        MmpUpdateResidentSetCounter(&(Space->Common), MappedCount);
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

    PADDRESS_SPACE_X86 AddressSpace;
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
    AddressSpace = (PADDRESS_SPACE_X86)(Process->AddressSpace);
    ProcessPageDirectory = AddressSpace->PageDirectory;
    SendInvalidateIpi = TRUE;
    if (VirtualAddress >= KERNEL_VA_START) {
        Process = PsGetKernelProcess();
        AddressSpace = (PADDRESS_SPACE_X86)(Process->AddressSpace);
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

        MmpSendTlbInvalidateIpi(&(AddressSpace->Common),
                                VirtualAddress,
                                PageCount);
    }

    return;
}

KSTATUS
MmpPreallocatePageTables (
    PADDRESS_SPACE SourceAddressSpace,
    PADDRESS_SPACE DestinationAddressSpace
    )

/*++

Routine Description:

    This routine allocates, but does not initialize nor fully map the page
    tables for a process that is being forked. It is needed because physical
    page allocations are not allowed while an image section lock is held.

Arguments:

    SourceAddressSpace - Supplies a pointer to the address space to prepare to
        copy. A page table is allocated but not initialized for every missing
        page table in the destination.

    DestinationAddressSpace - Supplies a pointer to the destination to create
        the page tables in.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on failure.

--*/

{

    ULONG DeleteIndex;
    PPTE Destination;
    PADDRESS_SPACE_X86 DestinationSpace;
    ULONG DirectoryIndex;
    PHYSICAL_ADDRESS Physical;
    PPTE Source;
    PADDRESS_SPACE_X86 SourceSpace;
    ULONG Total;

    DestinationSpace = (PADDRESS_SPACE_X86)DestinationAddressSpace;
    SourceSpace = (PADDRESS_SPACE_X86)SourceAddressSpace;
    Destination = DestinationSpace->PageDirectory;
    Source = SourceSpace->PageDirectory;
    Total = 0;
    for (DirectoryIndex = 0;
         DirectoryIndex < ((UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT);
         DirectoryIndex += 1) {

        if (Source[DirectoryIndex].Entry == 0) {
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
                    Physical = (ULONG)(Destination[DeleteIndex].Entry <<
                                       PAGE_SHIFT);

                    Destination[DeleteIndex].Entry = 0;
                    MmFreePhysicalPage(Physical);
                }
            }

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Destination[DirectoryIndex].Entry = (ULONG)Physical >> PAGE_SHIFT;
        Total += 1;
    }

    DestinationSpace->PageTableCount = Total;
    return STATUS_SUCCESS;
}

KSTATUS
MmpCopyAndChangeSectionMappings (
    PADDRESS_SPACE Destination,
    PADDRESS_SPACE Source,
    PVOID VirtualAddress,
    UINTN Size
    )

/*++

Routine Description:

    This routine converts all the mappings of the given virtual address region
    to read-only, and copies those read-only mappings to another process.

Arguments:

    Destination - Supplies a pointer to the destination address space.

    Source - Supplies a pointer to the source address space.

    VirtualAddress - Supplies the starting virtual address of the memory range.

    Size - Supplies the size of the virtual address region, in bytes.

Return Value:

    None.

--*/

{

    PVOID CurrentVirtual;
    PPTE DestinationDirectory;
    PADDRESS_SPACE_X86 DestinationSpace;
    PPTE DestinationTable;
    UINTN DirectoryIndex;
    ULONG DirectoryIndexEnd;
    ULONG DirectoryIndexStart;
    INTN MappedCount;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PageTable;
    PPROCESSOR_BLOCK ProcessorBlock;
    PPTE SourceDirectory;
    PADDRESS_SPACE_X86 SourceSpace;
    PPTE SourceTable;
    ULONG TableIndex;
    ULONG TableIndexEnd;
    ULONG TableIndexStart;
    PVOID VirtualEnd;

    DestinationSpace = (PADDRESS_SPACE_X86)Destination;
    DestinationDirectory = DestinationSpace->PageDirectory;
    SourceSpace = (PADDRESS_SPACE_X86)Source;
    SourceDirectory = SourceSpace->PageDirectory;
    VirtualEnd = VirtualAddress + Size;

    ASSERT(VirtualEnd > VirtualAddress);

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
    DirectoryIndexEnd = (UINTN)ALIGN_POINTER_UP(VirtualEnd,
                                                1 << PAGE_DIRECTORY_SHIFT);

    DirectoryIndexEnd >>= PAGE_DIRECTORY_SHIFT;
    DirectoryIndexStart = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    for (DirectoryIndex = DirectoryIndexStart;
         DirectoryIndex < DirectoryIndexEnd;
         DirectoryIndex += 1) {

        //
        // Determine the start and end page table indices that will need to be
        // synchronized.
        //

        TableIndexStart = ((UINTN)CurrentVirtual & PTE_INDEX_MASK) >>
                          PAGE_SHIFT;

        CurrentVirtual = (PVOID)((DirectoryIndex + 1) << PAGE_DIRECTORY_SHIFT);
        if (CurrentVirtual > VirtualEnd) {
            CurrentVirtual = VirtualEnd;
        }

        //
        // If the source directory does not have this page table, then skip it.
        //

        if (SourceDirectory[DirectoryIndex].Present == 0) {
            continue;
        }

        TableIndexEnd = ((UINTN)CurrentVirtual & PTE_INDEX_MASK) >>
                        PAGE_SHIFT;

        if (TableIndexEnd == 0) {
            TableIndexEnd = PAGE_SIZE / sizeof(PTE);
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

            PageTable = (ULONG)(DestinationDirectory[DirectoryIndex].Entry <<
                                PAGE_SHIFT);

            ASSERT(PageTable != 0);

            SourceTable = GET_PAGE_TABLE(DirectoryIndex);
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            ProcessorBlock = KeGetCurrentProcessorBlock();
            DestinationTable = ProcessorBlock->SwapPage;
            MmpMapPage(PageTable,
                       DestinationTable,
                       MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL);

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
                    *((PULONG)&(SourceTable[TableIndex])) &= ~X86_PTE_WRITABLE;
                    *((PULONG)&(DestinationTable[TableIndex])) =
                       *((PULONG)&(SourceTable[TableIndex])) & ~X86_PTE_DIRTY;

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
            PageTable = (ULONG)(DestinationDirectory[DirectoryIndex].Entry <<
                                PAGE_SHIFT);

            ASSERT(PageTable != INVALID_PHYSICAL_ADDRESS);

            SourceTable = GET_PAGE_TABLE(DirectoryIndex);
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            ProcessorBlock = KeGetCurrentProcessorBlock();
            DestinationTable = ProcessorBlock->SwapPage;
            MmpMapPage(PageTable,
                       DestinationTable,
                       MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL);

            for (TableIndex = TableIndexStart;
                 TableIndex < TableIndexEnd;
                 TableIndex += 1) {

                if (SourceTable[TableIndex].Entry != 0) {
                    MappedCount += 1;
                    *((PULONG)&(SourceTable[TableIndex])) &= ~X86_PTE_WRITABLE;
                    *((PULONG)&(DestinationTable[TableIndex])) =
                       *((PULONG)&(SourceTable[TableIndex])) & ~X86_PTE_DIRTY;
                }
            }

            MmpUnmapPages(DestinationTable, 1, 0, NULL);
            KeLowerRunLevel(OldRunLevel);
        }
    }

    ASSERT(VirtualAddress < KERNEL_VA_START);

    if (MappedCount != 0) {
        MmpUpdateResidentSetCounter(&(DestinationSpace->Common), MappedCount);
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

    PADDRESS_SPACE_X86 AddressSpace;
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

        AddressSpace = NULL;

    } else {
        AddressSpace =
              (PADDRESS_SPACE_X86)(CurrentThread->OwningProcess->AddressSpace);

        Directory = AddressSpace->PageDirectory;
    }

    DirectoryIndex = (UINTN)VirtualAddress >> PAGE_DIRECTORY_SHIFT;
    DirectoryEndIndex = ((UINTN)VirtualAddress + Size - 1) >>
                        PAGE_DIRECTORY_SHIFT;

    ASSERT(DirectoryIndex <= DirectoryEndIndex);

    while (DirectoryIndex <= DirectoryEndIndex) {
        if (Directory[DirectoryIndex].Present == 0) {
            MmpCreatePageTable(AddressSpace,
                               Directory,
                               (PVOID)(DirectoryIndex << PAGE_DIRECTORY_SHIFT));
        }

        DirectoryIndex += 1;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
MmpCreatePageDirectory (
    PADDRESS_SPACE_X86 AddressSpace
    )

/*++

Routine Description:

    This routine creates a new page directory for a new address space, and
    initializes it with kernel address space.

Arguments:

    AddressSpace - Supplies a pointer to the address space to create a page
        directory for.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_MEMORY if memory could not be allocated for the page table.

--*/

{

    PBLOCK_ALLOCATOR Allocator;
    ULONG CopySize;
    ULONG DirectoryIndex;
    ULONG KernelIndex;
    PPTE PageDirectory;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONG ZeroSize;

    Allocator = MmPageDirectoryBlockAllocator;

    //
    // This must be the kernel if there is no page directory block allocator
    // yet.
    //

    if (Allocator == NULL) {

        ASSERT(MmPageTableLock == NULL);

        AddressSpace->PageDirectory = (PPTE)MmKernelPageDirectory;
        AddressSpace->PageDirectoryPhysical =
                       MmpVirtualToPhysical(AddressSpace->PageDirectory, NULL);

        return STATUS_SUCCESS;
    }

    PageDirectory = MmAllocateBlock(Allocator, &PhysicalAddress);
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
    AddressSpace->PageDirectoryPhysical = PhysicalAddress;
    Status = STATUS_SUCCESS;

CreatePageDirectoryEnd:
    if (!KSUCCESS(Status)) {
        if (PageDirectory != NULL) {
            MmFreeBlock(Allocator, (PVOID)PageDirectory);
            PageDirectory = NULL;
        }
    }

    AddressSpace->PageDirectory = PageDirectory;
    return Status;
}

VOID
MmpDestroyPageDirectory (
    PADDRESS_SPACE_X86 AddressSpace
    )

/*++

Routine Description:

    This routine destroys a page directory upon address space destruction.

Arguments:

    AddressSpace - Supplies a pointer to the address space being torn down.

Return Value:

    None.

--*/

{

    PPTE Directory;
    ULONG DirectoryIndex;
    PHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS RunPhysicalAddress;
    UINTN RunSize;
    ULONG Total;

    //
    // Loop through and free every allocated page table in user mode.
    //

    RunSize = 0;
    RunPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    Total = 0;
    Directory = AddressSpace->PageDirectory;
    if (Directory == NULL) {
        return;
    }

    for (DirectoryIndex = 0;
         DirectoryIndex < ((UINTN)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT);
         DirectoryIndex += 1) {

        if (Directory[DirectoryIndex].Entry != 0) {
            Total += 1;
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

    //
    // Assert if page tables were leaked somewhere.
    //

    ASSERT(Total == AddressSpace->PageTableCount);

    AddressSpace->PageTableCount -= Total;
    MmFreeBlock(MmPageDirectoryBlockAllocator, Directory);
    AddressSpace->PageDirectory = NULL;
    AddressSpace->PageDirectoryPhysical = INVALID_PHYSICAL_ADDRESS;
    return;
}

VOID
MmpCreatePageTable (
    PADDRESS_SPACE_X86 AddressSpace,
    volatile PTE *Directory,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine creates a page table for the given directory and virtual
    address.

Arguments:

    AddressSpace - Supplies a pointer to the address space.

    Directory - Supplies a pointer to the page directory, in case this is an
        early call and there is no address space yet.

    VirtualAddress - Supplies the virtual address that the page table will
        eventually service.

Return Value:

    None.

--*/

{

    ULONG DirectoryIndex;
    ULONG NewCount;
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
        (((PULONG)MmKernelPageDirectory)[DirectoryIndex] !=
         ((PULONG)Directory)[DirectoryIndex])) {

        ASSERT(Directory[DirectoryIndex].Entry == 0);

        Directory[DirectoryIndex] = MmKernelPageDirectory[DirectoryIndex];
    }

    //
    // If the page table entry is now present, then return immediately.
    //

    if (Directory[DirectoryIndex].Present != 0) {
        RtlMemoryBarrier();
        return;
    }

    //
    // If there is no kernel page directory entry, then a new page table needs
    // to be allocated. Create calls that require more than just
    // synchronization better be called at low level.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if ((VirtualAddress < KERNEL_VA_START) &&
        (Directory[DirectoryIndex].Entry != 0)) {

        NewPageTable = (ULONG)(Directory[DirectoryIndex].Entry << PAGE_SHIFT);
        NewCount = 0;

    } else {
        NewPageTable = MmpAllocatePhysicalPages(1, 0);
        NewCount = 1;
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
        MmpMapPage(NewPageTable,
                   ProcessorBlock->SwapPage,
                   MAP_FLAG_PRESENT | MAP_FLAG_GLOBAL);

        RtlZeroMemory(ProcessorBlock->SwapPage, PAGE_SIZE);
        MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
        Directory[DirectoryIndex].Entry = (ULONG)NewPageTable >> PAGE_SHIFT;
        Directory[DirectoryIndex].Writable = 1;
        if (VirtualAddress >= KERNEL_VA_START) {

            ASSERT(MmKernelPageDirectory[DirectoryIndex].Present == 0);

            Directory[DirectoryIndex].Global = 1;
            MmKernelPageDirectory[DirectoryIndex] = Directory[DirectoryIndex];
            MmKernelPageDirectory[DirectoryIndex].Present = 1;

        } else {
            Directory[DirectoryIndex].User = 1;
            AddressSpace->PageTableCount += NewCount;
        }

        Directory[DirectoryIndex].Present = 1;
        RtlMemoryBarrier();
        KeLowerRunLevel(OldRunLevel);

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

