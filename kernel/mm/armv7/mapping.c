/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    mapping.c

Abstract:

    This module implements memory mapping and unmapping functionality.

Author:

    Evan Green 20-Aug-2012

Environment:

    Kernel (ARM)

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/bootload.h>
#include <minoca/arm.h>
#include "../mmp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro uses the self-mappings to retrieve the page table for the given
// first level index.
//

#define GET_PAGE_TABLE(_FirstIndex) \
    ((PVOID)MmPageTables + ((_FirstIndex) * SLT_SIZE))

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
MmpCreatePageTable (
    volatile FIRST_LEVEL_TABLE *FirstLevelTable,
    PVOID VirtualAddress
    );

PSECOND_LEVEL_TABLE
MmpMapPageTable (
    PHYSICAL_ADDRESS PhysicalAddress
    );

VOID
MmpUnmapPageTable (
    volatile SECOND_LEVEL_TABLE *SecondLevelTable
    );

VOID
MmpSyncKernelPageDirectory (
    volatile FIRST_LEVEL_TABLE *ProcessFirstLevelTable,
    PVOID VirtualAddress
    );

VOID
MmpCleanPageTableCacheRegion (
    PVOID PageTable,
    ULONG Size
    );

VOID
MmpCleanPageTableCacheLine (
    PVOID PageTableEntry
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

PFIRST_LEVEL_TABLE MmKernelFirstLevelTable;

//
// Store a pointer to the mapped page tables.
//

PSECOND_LEVEL_TABLE MmPageTables;

//
// Store the first level index of the self map.
//

ULONG MmPageTablesFirstIndex;

//
// Synchronizes access to creating or destroying page tables.
//

PQUEUED_LOCK MmPageTableLock;

//
// Stores the mask of cache attributes to use when loading the TTBR0.
//

ULONG MmTtbrCacheAttributes;

//
// Stores the initial value to use for all second level page table entries.
//

SECOND_LEVEL_TABLE MmSecondLevelInitialValue;

//
// Store whether or not the multiprocessing extensions are supported.
//

BOOL MmMultiprocessingExtensions;

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
    ULONG Entry;
    PFIRST_LEVEL_TABLE FirstLevelTable;
    ULONG KernelIndex;
    ULONG LoopIndex;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG SelfMapIndex;
    PVOID SelfMapPageTable;
    PHYSICAL_ADDRESS SelfMapPageTablePhysical;
    KSTATUS Status;
    ULONG ZeroSize;

    FirstLevelTable = NULL;

    //
    // If this is a request to create the kernel's page directory, then just
    // use the global page directory that was supplied from boot.
    //

    if (KernelProcessPageDirectory != FALSE) {
        FirstLevelTable = MmKernelFirstLevelTable;
        *NewPageDirectoryPhysical = MmpVirtualToPhysical(FirstLevelTable, NULL);

        ASSERT((*NewPageDirectoryPhysical & TTBR_ADDRESS_MASK) == 0);
        ASSERT(*NewPageDirectoryPhysical != INVALID_PHYSICAL_ADDRESS);

        *NewPageDirectoryPhysical |= MmTtbrCacheAttributes;
        Status = STATUS_SUCCESS;
        goto CreatePageDirectoryEnd;
    }

    ASSERT(MmPageDirectoryBlockAllocator != NULL);

    //
    // Attempt to allocate space for the new page directory and the page
    // needed for the user-mode half of the self map page table.
    //

    FirstLevelTable = MmAllocateBlock(MmPageDirectoryBlockAllocator,
                                      &PhysicalAddress);

    if (FirstLevelTable == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePageDirectoryEnd;
    }

    //
    // Zero user-mode and copy the kernel portions from the kernel first level
    // table.
    //

    KernelIndex = FLT_INDEX(KERNEL_VA_START);
    ZeroSize = KernelIndex * sizeof(FIRST_LEVEL_TABLE);
    CopySize = FLT_SIZE - ZeroSize;
    RtlZeroMemory(FirstLevelTable, ZeroSize);
    RtlCopyMemory(&(FirstLevelTable[KernelIndex]),
                  &(MmKernelFirstLevelTable[KernelIndex]),
                  CopySize);

    *NewPageDirectoryPhysical = PhysicalAddress;

    ASSERT((*NewPageDirectoryPhysical & TTBR_ADDRESS_MASK) == 0);
    ASSERT(*NewPageDirectoryPhysical != INVALID_PHYSICAL_ADDRESS);

    *NewPageDirectoryPhysical |= MmTtbrCacheAttributes;

    //
    // Set up the self map page table. Normally first level table entries are
    // set up in groups of 4 so that the page table size is the same as the page
    // size. By changing the first two directory entries of the self map page
    // table, the first half of the self map region (the user mode part) will
    // point at the per-process user mode page tables, and the second half will
    // point at the standard kernel self map page table.
    //

    SelfMapPageTable = (PVOID)FirstLevelTable + FLT_SIZE;
    RtlZeroMemory(SelfMapPageTable, PAGE_SIZE);
    MmpCleanPageTableCacheRegion(SelfMapPageTable, PAGE_SIZE);
    SelfMapPageTablePhysical = PhysicalAddress + FLT_SIZE;

    ASSERT(SelfMapPageTablePhysical != INVALID_PHYSICAL_ADDRESS);

    SelfMapIndex = FLT_INDEX(MmPageTables);

    //
    // Fix up the user mode mappings. Since there are 4 first level entries that
    // can be manipulated, the kernel address space better start on one of those
    // boundaries.
    //

    ASSERT(ALIGN_RANGE_DOWN((UINTN)KERNEL_VA_START, (1 << (32 - 2))) ==
           (UINTN)KERNEL_VA_START);

    for (LoopIndex = 0;
         LoopIndex < ((UINTN)KERNEL_VA_START >> (32 - 2));
         LoopIndex += 1) {

        Entry = ((ULONG)SelfMapPageTablePhysical >> SLT_ALIGNMENT) + LoopIndex;
        FirstLevelTable[SelfMapIndex + LoopIndex].Entry = Entry;
    }

    //
    // Serialization is not needed here as this new page directory will not be
    // live for page table walks until after a context switch, which serializes
    // execution.
    //

    MmpCleanPageTableCacheRegion(FirstLevelTable, FLT_SIZE);
    Status = STATUS_SUCCESS;

CreatePageDirectoryEnd:
    if (!KSUCCESS(Status)) {
        if (FirstLevelTable != NULL) {
            MmFreeBlock(MmPageDirectoryBlockAllocator, FirstLevelTable);
        }
    }

    *NewPageDirectory = FirstLevelTable;
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

    ULONG FirstIndex;
    PFIRST_LEVEL_TABLE FirstLevelTable;
    PHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS RunPhysicalAddress;
    UINTN RunSize;

    //
    // Do nothing if this is the global first level table.
    //

    if (PageDirectory == MmKernelFirstLevelTable) {
        return;
    }

    //
    // Free every attached page table in user mode. Don't bother to clean the
    // cache for these updates. The page directory should never be put back
    // in the TTBR and it certainly should not be in service now.
    //

    RunSize = 0;
    RunPhysicalAddress = INVALID_PHYSICAL_ADDRESS;
    FirstLevelTable = PageDirectory;
    for (FirstIndex = 0;
         FirstIndex < FLT_INDEX(KERNEL_VA_START);
         FirstIndex += 4) {

        if (FirstLevelTable[FirstIndex].Entry != 0) {
            PhysicalAddress =
                   (ULONG)(FirstLevelTable[FirstIndex].Entry << SLT_ALIGNMENT);

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

    This routine allocates and identity maps pages for use by application
    processor startup code.

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
    ULONG Flags;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    PhysicalAddress = MmpAllocateIdentityMappablePhysicalPages(PageCount, 0);

    ASSERT(PhysicalAddress != INVALID_PHYSICAL_ADDRESS);
    ASSERT(PhysicalAddress == (UINTN)PhysicalAddress);

    //
    // If the physical address will be identity mapped in the kernel VA range,
    // then reserve it while it is in use.
    //

    *Allocation = (PVOID)(UINTN)PhysicalAddress;
    if (*Allocation >= KERNEL_VA_START) {
        Status = MmpAllocateAddressRange(&MmKernelVirtualSpace,
                                         PageCount << PAGE_SHIFT,
                                         PAGE_SIZE,
                                         MemoryTypeReserved,
                                         AllocationStrategyFixedAddress,
                                         FALSE,
                                         Allocation);

        ASSERT(KSUCCESS(Status));

    } else {

        ASSERT(MmpIsAccountingRangeInUse(&MmKernelVirtualSpace,
                                         *Allocation,
                                         PAGE_SIZE) == FALSE);

    }

    //
    // Map the pages received.
    //

    Flags = MAP_FLAG_PRESENT | MAP_FLAG_EXECUTE;
    CurrentAddress = *Allocation;
    for (CurrentPage = 0; CurrentPage < PageCount; CurrentPage += 1) {
        MmpMapPage((UINTN)CurrentAddress, CurrentAddress, Flags);
        CurrentAddress += PAGE_SIZE;
    }

    *PageDirectory = (PVOID)ArGetTranslationTableBaseRegister0();
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

    ULONG UnmapFlags;

    UnmapFlags = UNMAP_FLAG_FREE_PHYSICAL_PAGES |
                 UNMAP_FLAG_SEND_INVALIDATE_IPI;

    //
    // If the allocation was in the kernel VA space, then free the accounting
    // range. Otherwise just directly unmap it.
    //

    if (Allocation >= KERNEL_VA_START) {
        MmpFreeAccountingRange(PsGetKernelProcess(),
                               &MmKernelVirtualSpace,
                               Allocation,
                               PageCount << PAGE_SHIFT,
                               FALSE,
                               UnmapFlags);

    } else {
        MmpUnmapPages(Allocation, PageCount, UnmapFlags, NULL);
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
    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    ULONG LoopIndex;
    PKPROCESS Process;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;
    ULONG SelfMapIndex;
    BOOL UserModeSelfMap;

    //
    // Assume the range is writable.
    //

    if (Writable != NULL) {
        *Writable = TRUE;
    }

    //
    // If the memory manager is not yet initialized, just assume the whole
    // region is valid.
    //

    if (MmPageTables == NULL) {
        return Length;
    }

    //
    // The self-map cannot be used on ARM because there is no way to
    // self-reference the user mode portions of it. The VA range of the self
    // map is in kernel VA space, meaning that the one second level self map
    // page table entry that would point to the self map is in kernel self map
    // space. Each process's user-mode self map second level page table
    // entries, therefore, cannot be accessed.
    //
    // So, cruise the processor block for the current first level page table.
    //

    ProcessorBlock = KeGetCurrentProcessorBlockForDebugger();
    if ((ProcessorBlock == NULL) ||
        (ProcessorBlock->RunningThread == NULL) ||
        (ProcessorBlock->RunningThread->OwningProcess == NULL) ||
        (ProcessorBlock->RunningThread->OwningProcess->PageDirectory == NULL)) {

        if (Address < KERNEL_VA_START) {
            return Length;
        }

        FirstLevelTable = MmKernelFirstLevelTable;

    } else {
        Process = ProcessorBlock->RunningThread->OwningProcess;
        FirstLevelTable = Process->PageDirectory;
    }

    SelfMapIndex = FLT_INDEX(MmPageTables);

    //
    // For each page in the address range, determine if it is mapped.
    //

    BytesMapped = 0;
    BytesRemaining = Length;
    while (BytesRemaining != 0) {
        FirstIndex = FLT_INDEX(Address);
        if (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) {
            break;
        }

        //
        // If the virtual address falls into the user mode self-map, then using
        // the self map is not possible. GET_PAGE_TABLE will return a pointer
        // to the kernel's self map, which does not have this process's user
        // mode self map mapped. Directly fetch the process's self map. It's
        // dangling off the end of the page directory. Don't do this for the
        // kernel process.
        //

        UserModeSelfMap = FALSE;
        if ((FirstLevelTable != MmKernelFirstLevelTable) &&
            (ALIGN_RANGE_DOWN(FirstIndex, 4) == SelfMapIndex)) {

            for (LoopIndex = 0;
                 LoopIndex < ((UINTN)KERNEL_VA_START >> (32 - 2));
                 LoopIndex += 1) {

                if (FirstIndex == (SelfMapIndex + LoopIndex)) {
                    UserModeSelfMap = TRUE;
                    break;
                }
            }
        }

        if (UserModeSelfMap != FALSE) {
            SecondLevelTable = ((PVOID)FirstLevelTable + FLT_SIZE) +
                               ((FirstIndex - SelfMapIndex) * SLT_SIZE);

        } else {
            SecondLevelTable = GET_PAGE_TABLE(FirstIndex);
        }

        SecondIndex = SLT_INDEX(Address);
        if (SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED) {
            break;
        }

        //
        // If the page is read only, then note that the whole region is not
        // writable.
        //

        if ((Writable != NULL) &&
            (SecondLevelTable[SecondIndex].AccessExtension == 1)) {

            ASSERT((SecondLevelTable[SecondIndex].Access ==
                    SLT_XACCESS_SUPERVISOR_READ_ONLY) ||
                   (SecondLevelTable[SecondIndex].Access ==
                    SLT_XACCESS_READ_ONLY_ALL_MODES));

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

    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    PKPROCESS Process;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;

    //
    // Assume that the page was writable and do no more if the memory manager
    // is not yet initialized.
    //

    *WasWritable = TRUE;
    if (MmPageTables == NULL) {
        return;
    }

    //
    // The self-map cannot be used on ARM because there is no way to
    // self-reference the user mode portions of it. The VA range of the self
    // map is in kernel VA space, meaning that the one second level self map
    // page table entry that would point to the self map is in kernel self map
    // space. Each process's user-mode self map second level page table
    // entries, therefore, cannot be accessed.
    //
    // So, cruise the processor block for the current first level page table.
    //

    ProcessorBlock = KeGetCurrentProcessorBlockForDebugger();
    if ((ProcessorBlock == NULL) ||
        (ProcessorBlock->RunningThread == NULL) ||
        (ProcessorBlock->RunningThread->OwningProcess == NULL) ||
        (ProcessorBlock->RunningThread->OwningProcess->PageDirectory == NULL)) {

        if (Address < KERNEL_VA_START) {
            return;
        }

        FirstLevelTable = MmKernelFirstLevelTable;

    } else {
        Process = ProcessorBlock->RunningThread->OwningProcess;
        FirstLevelTable = Process->PageDirectory;
    }

    Address = (PVOID)(UINTN)ALIGN_RANGE_DOWN((UINTN)Address, PAGE_SIZE);

    //
    // For the page containing the address, modify its mapping properties. It
    // should be mapped.
    //

    FirstIndex = FLT_INDEX(Address);

    ASSERT(FirstLevelTable[FirstIndex].Format != FLT_UNMAPPED);

    SecondLevelTable = GET_PAGE_TABLE(FirstIndex);
    SecondIndex = SLT_INDEX(Address);

    ASSERT(SecondLevelTable[SecondIndex].Format != SLT_UNMAPPED);

    //
    // Note if the page was actually read-only and then modify the mapping if
    // necessary.
    //

    if (SecondLevelTable[SecondIndex].AccessExtension == 1) {

        ASSERT((SecondLevelTable[SecondIndex].Access ==
                SLT_XACCESS_SUPERVISOR_READ_ONLY) ||
               (SecondLevelTable[SecondIndex].Access ==
                SLT_XACCESS_READ_ONLY_ALL_MODES));

        *WasWritable = FALSE;
        if (Writable != FALSE) {
            SecondLevelTable[SecondIndex].AccessExtension = 0;
            if (Address >= KERNEL_VA_START) {
                SecondLevelTable[SecondIndex].Access = SLT_ACCESS_SUPERVISOR;

            } else {
                SecondLevelTable[SecondIndex].Access = SLT_ACCESS_USER_FULL;
            }

            //
            // Clean the cache line for the modified page table entry.
            //

            MmpCleanPageTableCacheLine((PVOID)&(SecondLevelTable[SecondIndex]));
        }

    } else {

        ASSERT((SecondLevelTable[SecondIndex].Access ==
                SLT_ACCESS_SUPERVISOR) ||
               (SecondLevelTable[SecondIndex].Access ==
                SLT_ACCESS_USER_FULL));

        if (Writable == FALSE) {
            SecondLevelTable[SecondIndex].AccessExtension = 1;
            if (Address >= KERNEL_VA_START) {
                SecondLevelTable[SecondIndex].Access =
                                              SLT_XACCESS_SUPERVISOR_READ_ONLY;

            } else {
                SecondLevelTable[SecondIndex].Access =
                                               SLT_XACCESS_READ_ONLY_ALL_MODES;
            }

            //
            // Clean the cache line for the modified page table entry.
            //

            MmpCleanPageTableCacheLine((PVOID)&(SecondLevelTable[SecondIndex]));
        }
    }

    //
    // Invalidation of the TLB also serializes execution, guaranteeing the
    // completion of the page table modifications.
    //

    ArInvalidateTlbEntry(Address);
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

    PVOID CleanEnd;
    ULONG CleanSize;
    PVOID CleanStart;
    ULONG EndIndex;
    PULONG Entry;
    ULONG Index;
    ULONG OfficialValue;

    //
    // Exit immediately if the page directory is the kernel's page directory.
    //

    if (PageDirectory == MmKernelFirstLevelTable) {
        return;
    }

    ASSERT(sizeof(FIRST_LEVEL_TABLE) == sizeof(ULONG));

    Entry = PageDirectory;
    Index = ALIGN_RANGE_DOWN(FLT_INDEX(VirtualAddress), 4);
    EndIndex = ALIGN_RANGE_DOWN(FLT_INDEX(VirtualAddress + (Size - 1)), 4);
    CleanStart = NULL;
    CleanEnd = NULL;
    while (Index <= EndIndex) {

        //
        // The supplied VA range should never include the self map entries.
        //

        ASSERT(Index != FLT_INDEX(MmPageTables));

        OfficialValue = ((PULONG)MmKernelFirstLevelTable)[Index];
        if (Entry[Index] != OfficialValue) {
            Entry[Index] = ((PULONG)MmKernelFirstLevelTable)[Index];
            Entry[Index + 1] = ((PULONG)MmKernelFirstLevelTable)[Index + 1];
            Entry[Index + 2] = ((PULONG)MmKernelFirstLevelTable)[Index + 2];
            Entry[Index + 3] = ((PULONG)MmKernelFirstLevelTable)[Index + 3];
            if (CleanStart == NULL) {
                CleanStart = &(Entry[Index]);
            }

            CleanEnd = &(Entry[Index + 3]);
        }

        Index += 4;
    }

    if (CleanStart != NULL) {

        ASSERT(CleanEnd > CleanStart);

        CleanSize = (CleanEnd + sizeof(FIRST_LEVEL_TABLE)) - CleanStart;
        MmpCleanPageTableCacheRegion(CleanStart, CleanSize);
        ArSerializeExecution();
    }

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
    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    ULONG Flags;
    BOOL FreePageTable;
    ULONG MultiprocessorIdRegister;
    PHYSICAL_ADDRESS PhysicalAddress;
    PHYSICAL_ADDRESS RunPhysicalAddress;
    UINTN RunSize;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;
    ULONG SelfMapIndex;
    volatile SECOND_LEVEL_TABLE *SelfMapPageTable;
    KSTATUS Status;

    //
    // Phase 0 runs on the boot processor before the debugger is online.
    //

    if (Phase == 0) {
        if ((Parameters->PageDirectory == NULL) ||
            (Parameters->PageTableStage == NULL) ||
            (Parameters->PageTables == NULL)) {

            Status = STATUS_NOT_INITIALIZED;
            goto ArchInitializeEnd;
        }

        //
        // Initialize basic globals.
        //

        MmKernelFirstLevelTable = Parameters->PageDirectory;
        MmPageTables = Parameters->PageTables;
        MmPageTablesFirstIndex = FLT_INDEX(MmPageTables);

        //
        // Set the TTBR cache attributes based on the availability of the
        // multiprocessor extensions.
        //

        MultiprocessorIdRegister = ArGetMultiprocessorIdRegister();
        if ((MultiprocessorIdRegister & MPIDR_MP_EXTENSIONS_ENABLED) != 0) {
            MmMultiprocessingExtensions = TRUE;
            MmSecondLevelInitialValue.Shared = 1;
            MmTtbrCacheAttributes = TTBR_MP_KERNEL_MASK;

        } else {
            MmMultiprocessingExtensions = FALSE;
            MmTtbrCacheAttributes = TTBR_NO_MP_KERNEL_MASK;
        }

        MmpInitializeCpuCaches();
        Status = STATUS_SUCCESS;

    //
    // Phase 1 runs on all processors.
    //

    } else if (Phase == 1) {
        Status = STATUS_SUCCESS;

    //
    // Phase 2 initialization only runs on the boot processor in order to
    // prepare for multi-threaded execution.
    //

    } else if (Phase == 2) {
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
                             FLT_SIZE + PAGE_SIZE,
                             FLT_ALIGNMENT,
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
        FirstLevelTable  = MmKernelFirstLevelTable;
        for (FirstIndex = 0;
             FirstIndex < FLT_INDEX(KERNEL_VA_START);
             FirstIndex += 4) {

            if (FirstLevelTable[FirstIndex].Entry == 0) {

                ASSERT(FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED);

                continue;
            }

            //
            // A second level table is present, check to see if it is all zeros.
            // Be sure to check all four second level tables that use the same
            // physical page.
            //

            FreePageTable = TRUE;
            SecondLevelTable = GET_PAGE_TABLE(FirstIndex);
            for (SecondIndex = 0;
                 SecondIndex < ((SLT_SIZE * 4) / sizeof(SECOND_LEVEL_TABLE));
                 SecondIndex += 1) {

                if (SecondLevelTable[SecondIndex].Entry != 0) {
                    FreePageTable = FALSE;
                    break;
                }

                ASSERT(SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED);
            }

            //
            // Move to the next set of first level entries if this page table
            // is in use.
            //

            if (FreePageTable == FALSE) {
                continue;
            }

            //
            // Otherwise, update the first level entries, the self map, and
            // free the page table.
            //

            PhysicalAddress =
                   (ULONG)(FirstLevelTable[FirstIndex].Entry << SLT_ALIGNMENT);

            RtlZeroMemory((PVOID)&(FirstLevelTable[FirstIndex]),
                          sizeof(FIRST_LEVEL_TABLE) * 4);

            MmpCleanPageTableCacheRegion((PVOID)&(FirstLevelTable[FirstIndex]),
                                         sizeof(FIRST_LEVEL_TABLE) * 4);

            SelfMapPageTable = (PSECOND_LEVEL_TABLE)((PVOID)FirstLevelTable +
                                                     FLT_SIZE);

            SelfMapIndex = FirstIndex >> 2;
            RtlZeroMemory((PVOID)&(SelfMapPageTable[SelfMapIndex]),
                          sizeof(SECOND_LEVEL_TABLE));

            MmpCleanPageTableCacheLine(
                                     (PVOID)&(SelfMapPageTable[SelfMapIndex]));

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

        ArSerializeExecution();
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

    volatile FIRST_LEVEL_TABLE *CurrentFirstLevelTable;
    PKPROCESS CurrentProcess;
    ULONG FirstIndex;
    BOOL Result;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;

    Result = FALSE;

    //
    // This check only applies to kernel-mode addresses.
    //

    if (FaultingAddress < KERNEL_VA_START) {
        return FALSE;
    }

    CurrentProcess = PsGetCurrentProcess();
    CurrentFirstLevelTable = CurrentProcess->PageDirectory;
    FirstIndex = FLT_INDEX(FaultingAddress);

    //
    // Check to see if the kernel page directory has an entry and the current
    // page directory doesn't. If so, add the entry.
    //

    if (MmKernelFirstLevelTable[FirstIndex].Format !=
        CurrentFirstLevelTable[FirstIndex].Format) {

        SecondLevelTable = GET_PAGE_TABLE(FirstIndex);
        FirstIndex = ALIGN_RANGE_DOWN(FirstIndex, 4);
        CurrentFirstLevelTable[FirstIndex] =
                                           MmKernelFirstLevelTable[FirstIndex];

        CurrentFirstLevelTable[FirstIndex + 1] =
                                       MmKernelFirstLevelTable[FirstIndex + 1];

        CurrentFirstLevelTable[FirstIndex + 2] =
                                       MmKernelFirstLevelTable[FirstIndex + 2];

        CurrentFirstLevelTable[FirstIndex + 3] =
                                       MmKernelFirstLevelTable[FirstIndex + 3];

        MmpCleanPageTableCacheRegion(
                                  (PVOID)&(CurrentFirstLevelTable[FirstIndex]),
                                  sizeof(FIRST_LEVEL_TABLE) * 4);

        ArSerializeExecution();

        //
        // See if the page fault is resolved by this entry.
        //

        SecondIndex = SLT_INDEX(FaultingAddress);
        if (SecondLevelTable[SecondIndex].Format != SLT_UNMAPPED) {
            Result = TRUE;
        }
    }

    return Result;
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
    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    PKPROCESS Process;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;

    CurrentThread = KeGetCurrentThread();
    if (CurrentThread == NULL) {

        ASSERT(VirtualAddress >= KERNEL_VA_START);

        FirstLevelTable = MmKernelFirstLevelTable;
        Process = NULL;

    } else {
        Process = CurrentThread->OwningProcess;
        FirstLevelTable = Process->PageDirectory;
    }

    ASSERT(FirstLevelTable != NULL);
    ASSERT((UINTN)VirtualAddress + PAGE_SIZE - 1 > (UINTN)VirtualAddress);

    //
    // Assert that the addresses are page aligned.
    //

    ASSERT((PhysicalAddress & PAGE_MASK) == 0);
    ASSERT(((ULONG)VirtualAddress & PAGE_MASK) == 0);

    FirstIndex = FLT_INDEX(VirtualAddress);

    ASSERT(FirstIndex != MmPageTablesFirstIndex);

    //
    // Create a page table if the first level table entry is not there.
    //

    if (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) {
        MmpCreatePageTable(FirstLevelTable, VirtualAddress);
    }

    SecondLevelTable = GET_PAGE_TABLE(FirstIndex);

    //
    // Look up the entry in the page table, and zero out the entry.
    //

    SecondIndex = SLT_INDEX(VirtualAddress);

    ASSERT((SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED) &&
           (SecondLevelTable[SecondIndex].Entry == 0));

    SecondLevelTable[SecondIndex] = MmSecondLevelInitialValue;
    SecondLevelTable[SecondIndex].CacheTypeExtension = 0;
    SecondLevelTable[SecondIndex].NotGlobal = 0;

    //
    // Set the access attributes.
    //

    ASSERT(((Flags & MAP_FLAG_USER_MODE) == 0) ||
           (VirtualAddress < KERNEL_VA_START));

    if ((Flags & MAP_FLAG_READ_ONLY) != 0) {
        SecondLevelTable[SecondIndex].AccessExtension = 1;
        if ((Flags & MAP_FLAG_USER_MODE) == 0) {
            SecondLevelTable[SecondIndex].Access =
                                              SLT_XACCESS_SUPERVISOR_READ_ONLY;

        } else {
            SecondLevelTable[SecondIndex].Access =
                                              SLT_XACCESS_READ_ONLY_ALL_MODES;

            SecondLevelTable[SecondIndex].NotGlobal = 1;
        }

    } else {
        SecondLevelTable[SecondIndex].AccessExtension = 0;
        if ((Flags & MAP_FLAG_USER_MODE) == 0) {
            SecondLevelTable[SecondIndex].Access = SLT_ACCESS_SUPERVISOR;

        } else {
            SecondLevelTable[SecondIndex].Access = SLT_ACCESS_USER_FULL;
            SecondLevelTable[SecondIndex].NotGlobal = 1;
        }
    }

    //
    // Set the cache attributes.
    //

    if ((Flags & MAP_FLAG_CACHE_DISABLE) != 0) {

        ASSERT((Flags & MAP_FLAG_WRITE_THROUGH) == 0);

        SecondLevelTable[SecondIndex].CacheAttributes = SLT_UNCACHED;

    } else if ((Flags & MAP_FLAG_WRITE_THROUGH) != 0) {
        SecondLevelTable[SecondIndex].CacheAttributes = SLT_WRITE_THROUGH;

    } else {
        SecondLevelTable[SecondIndex].CacheTypeExtension = 1;
        SecondLevelTable[SecondIndex].CacheAttributes = SLT_WRITE_BACK;
    }

    //
    // Large pages are currently unsupported.
    //

    ASSERT((Flags & MAP_FLAG_LARGE_PAGE) == 0);

    SecondLevelTable[SecondIndex].Entry =
                                        ((ULONG)PhysicalAddress >> PAGE_SHIFT);

    if ((Flags & MAP_FLAG_PRESENT) != 0) {
        if ((Flags & MAP_FLAG_EXECUTE) != 0) {
            SecondLevelTable[SecondIndex].Format = SLT_SMALL_PAGE;

        } else {
            SecondLevelTable[SecondIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;
        }

    } else {
        SecondLevelTable[SecondIndex].Format = SLT_UNMAPPED;
    }

    //
    // Clean the cache line for the modified page table entry. This also
    // provides the proper memory barrier for future page table walks.
    //

    MmpCleanPageTableCacheLine((PVOID)&(SecondLevelTable[SecondIndex]));

    //
    // Page table modifications to user mode addresses do not need
    // serialization as returns from exceptions have the same effect.
    //

    if (VirtualAddress < KERNEL_VA_START) {
        MmpUpdateResidentSetCounter(Process, 1);

    } else {
        ArSerializeExecution();
    }

    //
    // If this may be executed, invalidate that region of the instruction
    // cache. Do this for both virtually and physically indexed caches. It may
    // be that a physical page was reused without the instruction cache being
    // invalidated when the new data reached the point of unification.
    //

    if ((Flags & MAP_FLAG_EXECUTE) != 0) {
        MmpInvalidateInstructionCacheRegion(VirtualAddress, PAGE_SIZE);
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
    BOOL CleanInvalidate;
    PVOID CurrentVirtual;
    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    INTN MappedCount;
    ULONG PageNumber;
    BOOL PageWasPresent;
    PHYSICAL_ADDRESS PhysicalPage;
    PKPROCESS Process;
    PHYSICAL_ADDRESS RunPhysicalPage;
    UINTN RunSize;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelEntry;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;
    BOOL SerializeExecution;
    PKTHREAD Thread;

    ChangedSomething = FALSE;
    CleanInvalidate = TRUE;
    PhysicalPage = INVALID_PHYSICAL_ADDRESS;
    Thread = KeGetCurrentThread();
    if (Thread == NULL) {

        ASSERT(VirtualAddress >= KERNEL_VA_START);
        ASSERT(((UINTN)VirtualAddress + (PageCount << MmPageShift())) - 1 >
               (UINTN)VirtualAddress);

        FirstLevelTable = MmKernelFirstLevelTable;
        Process = NULL;

    } else {
        Process = Thread->OwningProcess;
        FirstLevelTable = Process->PageDirectory;

        //
        // If there's only one thread in the process and this is not a kernel
        // mode address, then there's no need to send a TLB invalidate IPI.
        //

        if ((Process->ThreadCount <= 1) && (VirtualAddress < KERNEL_VA_START)) {
            UnmapFlags &= ~UNMAP_FLAG_SEND_INVALIDATE_IPI;
            if (Process->ThreadCount == 0) {
                CleanInvalidate = FALSE;
            }
        }
    }

    ASSERT(((ULONG)VirtualAddress & PAGE_MASK) == 0);

    CurrentVirtual = VirtualAddress;

    //
    // Loop through and unmap each page.
    //

    MappedCount = 0;
    for (PageNumber = 0; PageNumber < PageCount; PageNumber += 1) {
        FirstIndex = FLT_INDEX(CurrentVirtual);
        if ((FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) &&
            (MmKernelFirstLevelTable[FirstIndex].Format == FLT_UNMAPPED)) {

            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        SecondLevelTable = GET_PAGE_TABLE(FirstIndex);
        SecondIndex = SLT_INDEX(CurrentVirtual);

        //
        // If the page was mapped before, then preserve the entry so that
        // physical pages can potentially be freed. If it wasn't mapped, then
        // wipe out the whole entry as an indicator to the loop that frees
        // physical pages and determines write status.
        //

        if (SecondLevelTable[SecondIndex].Entry != 0) {
            PageWasPresent = FALSE;
            if (SecondLevelTable[SecondIndex].Format != SLT_UNMAPPED) {
                ChangedSomething = TRUE;
                PageWasPresent = TRUE;
            }

            MappedCount += 1;
            if ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) == 0) {
                *((PULONG)(&(SecondLevelTable[SecondIndex]))) = 0;

            } else {
                SecondLevelTable[SecondIndex].Format = SLT_UNMAPPED;
            }

            if (CleanInvalidate != FALSE) {
                MmpCleanPageTableCacheLine(
                                      (PVOID)&(SecondLevelTable[SecondIndex]));

                //
                // Just invalidate this processor's TLB entry if no IPI is to be
                // sent. Note that the TLB entry invalidation routine also
                // serializes execution.
                //

                if ((PageWasPresent != FALSE) &&
                    ((UnmapFlags & UNMAP_FLAG_SEND_INVALIDATE_IPI) == 0)) {

                    ArInvalidateTlbEntry(CurrentVirtual);
                }
            }

        } else {

            ASSERT(SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED);
        }

        CurrentVirtual += PAGE_SIZE;
    }

    //
    // Send the invalidate IPI if requested. Note that the TLB entry
    // invalidation routine also serializes execution.
    //

    if ((ChangedSomething != FALSE) &&
        ((UnmapFlags & UNMAP_FLAG_SEND_INVALIDATE_IPI) != 0)) {

        MmpSendTlbInvalidateIpi((PVOID)FirstLevelTable,
                                VirtualAddress,
                                PageCount);
    }

    if (PageWasDirty != NULL) {
        *PageWasDirty = TRUE;
    }

    //
    // Loop again and free physical pages now that all processors are not
    // touching them. It's important that this happens only after the TLB
    // invalidate IPI went out.
    //

    SerializeExecution = FALSE;
    if ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0) {
        RunSize = 0;
        RunPhysicalPage = INVALID_PHYSICAL_ADDRESS;
        CurrentVirtual = VirtualAddress;
        for (PageNumber = 0; PageNumber < PageCount; PageNumber += 1) {
            FirstIndex = FLT_INDEX(CurrentVirtual);
            if ((FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) &&
                (MmKernelFirstLevelTable[FirstIndex].Format == FLT_UNMAPPED)) {

                CurrentVirtual += PAGE_SIZE;
                continue;
            }

            SecondLevelTable = GET_PAGE_TABLE(FirstIndex);
            SecondIndex = SLT_INDEX(CurrentVirtual);
            SecondLevelEntry = &(SecondLevelTable[SecondIndex]);

            //
            // Skip the entry if it's zero. It was never mapped and there is no
            // reason to clean the page table cache line.
            //

            if (SecondLevelEntry->Entry == 0) {
                CurrentVirtual += PAGE_SIZE;
                continue;
            }

            //
            // Free the physical page. That's the only reason this loop is
            // executing.
            //

            PhysicalPage = (ULONG)(SecondLevelEntry->Entry << PAGE_SHIFT);
            if (RunSize != 0) {
                if ((RunPhysicalPage + RunSize) == PhysicalPage) {
                    RunSize += PAGE_SIZE;

                } else {
                    MmFreePhysicalPages(RunPhysicalPage, RunSize >> PAGE_SHIFT);
                    RunPhysicalPage = PhysicalPage;
                    RunSize = PAGE_SIZE;
                }

            } else {
                RunPhysicalPage = PhysicalPage;
                RunSize = PAGE_SIZE;
            }

            *((PULONG)SecondLevelEntry) = 0;
            if (CleanInvalidate != FALSE) {
                MmpCleanPageTableCacheLine((PVOID)SecondLevelEntry);
                SerializeExecution = TRUE;
            }

            CurrentVirtual += PAGE_SIZE;
        }

        if (RunSize != 0) {
            MmFreePhysicalPages(RunPhysicalPage, RunSize >> PAGE_SHIFT);
        }
    }

    //
    // Page table modifications to user mode addresses do not need
    // serialization as returns from exceptions have the same effect.
    //

    if (VirtualAddress < KERNEL_VA_START) {
        MmpUpdateResidentSetCounter(Process, -MappedCount);

    } else if (SerializeExecution != FALSE) {
        ArSerializeExecution();
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

    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    PHYSICAL_ADDRESS PhysicalAddress;
    PKPROCESS Process;
    volatile FIRST_LEVEL_TABLE *ProcessFirstLevelTable;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelEntry;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;

    if (Attributes != NULL) {
        *Attributes = MAP_FLAG_DIRTY;
    }

    Process = PsGetCurrentProcess();
    ProcessFirstLevelTable = NULL;
    if (Process != NULL) {
        ProcessFirstLevelTable = Process->PageDirectory;
    }

    FirstIndex = FLT_INDEX(VirtualAddress);
    if (VirtualAddress >= KERNEL_VA_START) {
        FirstLevelTable = MmKernelFirstLevelTable;

    } else {
        FirstLevelTable = ProcessFirstLevelTable;
    }

    if ((FirstIndex == MmPageTablesFirstIndex) ||
        (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED)) {

        return INVALID_PHYSICAL_ADDRESS;
    }

    SecondLevelTable = GET_PAGE_TABLE(FirstIndex);
    SecondIndex = SLT_INDEX(VirtualAddress);
    if (SecondLevelTable[SecondIndex].Entry == 0) {

        ASSERT(SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED);

        return INVALID_PHYSICAL_ADDRESS;
    }

    SecondLevelEntry = &(SecondLevelTable[SecondIndex]);
    PhysicalAddress = (ULONG)(SecondLevelEntry->Entry << PAGE_SHIFT) +
                             ((ULONG)VirtualAddress & PAGE_MASK);

    if (Attributes != NULL) {
        if (SecondLevelTable[SecondIndex].Format != SLT_UNMAPPED) {
            *Attributes |= MAP_FLAG_PRESENT;
            if (SecondLevelTable[SecondIndex].Format == SLT_SMALL_PAGE) {
                *Attributes |= MAP_FLAG_EXECUTE;
            }
        }

        if (!((SecondLevelEntry->AccessExtension == 0) &&
              ((SecondLevelEntry->Access == SLT_ACCESS_SUPERVISOR) ||
               (SecondLevelEntry->Access == SLT_ACCESS_USER_FULL)))) {

            *Attributes |= MAP_FLAG_READ_ONLY;
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

    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    ULONG Offset;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PageTablePhysical;
    PHYSICAL_ADDRESS PhysicalAddress;
    volatile FIRST_LEVEL_TABLE *ProcessFirstLevelTable;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;

    //
    // Assert that this is called at low level. If it ever needs to be called
    // at dispatch, then all acquisitions of the page table spin lock will need
    // to be changed to raise to dispatch before acquiring.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    ProcessFirstLevelTable = PageDirectory;
    FirstIndex = FLT_INDEX(VirtualAddress);
    if (VirtualAddress >= KERNEL_VA_START) {
        FirstLevelTable = MmKernelFirstLevelTable;

    } else {
        FirstLevelTable = ProcessFirstLevelTable;
    }

    if (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) {
        return INVALID_PHYSICAL_ADDRESS;
    }

    PageTablePhysical = (ULONG)(FirstLevelTable[FirstIndex].Entry <<
                                SLT_ALIGNMENT) & (~PAGE_MASK);

    //
    // Use the processor's swap page for this.
    //

    FirstIndex = ALIGN_RANGE_DOWN(FirstIndex, 4);
    SecondIndex = SLT_INDEX(VirtualAddress);
    Offset = (FLT_INDEX(VirtualAddress) - FirstIndex) * SLT_SIZE;
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    MmpMapPage(PageTablePhysical,
               ProcessorBlock->SwapPage,
               MAP_FLAG_PRESENT | MAP_FLAG_READ_ONLY);

    SecondLevelTable = (PSECOND_LEVEL_TABLE)(ProcessorBlock->SwapPage);
    SecondLevelTable = (PSECOND_LEVEL_TABLE)((UINTN)SecondLevelTable + Offset);
    if (SecondLevelTable[SecondIndex].Entry == 0) {
        PhysicalAddress = INVALID_PHYSICAL_ADDRESS;

    } else {
        PhysicalAddress =
                    (ULONG)(SecondLevelTable[SecondIndex].Entry << PAGE_SHIFT) +
                    ((ULONG)VirtualAddress & PAGE_MASK);
    }

    //
    // Unmap the page table.
    //

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

    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    ULONG Offset;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PageTablePhysical;
    PHYSICAL_ADDRESS PhysicalAddress;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SecondIndex;
    SECOND_LEVEL_TABLE SecondLevelEntry;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;
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
    FirstLevelTable = (PFIRST_LEVEL_TABLE)(TypedProcess->PageDirectory);
    FirstIndex = FLT_INDEX(VirtualAddress);
    if (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) {
        goto UnmapPageInOtherProcessEnd;
    }

    PageTablePhysical = (ULONG)(FirstLevelTable[FirstIndex].Entry <<
                                SLT_ALIGNMENT) & (~PAGE_MASK);

    FirstIndex = ALIGN_RANGE_DOWN(FirstIndex, 4);
    SecondIndex = SLT_INDEX(VirtualAddress);
    Offset = (FLT_INDEX(VirtualAddress) - FirstIndex) * SLT_SIZE;
    SecondLevelEntry.Entry = 0;
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    MmpMapPage(PageTablePhysical, ProcessorBlock->SwapPage, MAP_FLAG_PRESENT);
    SecondLevelTable = (PSECOND_LEVEL_TABLE)(ProcessorBlock->SwapPage);
    SecondLevelTable = (PSECOND_LEVEL_TABLE)((UINTN)SecondLevelTable + Offset);
    if (SecondLevelTable[SecondIndex].Entry != 0) {
        SecondLevelEntry = SecondLevelTable[SecondIndex];
        *((PULONG)&(SecondLevelTable[SecondIndex])) = 0;

        //
        // Serialization is not required because the unmap of the second level
        // page table will a serialize exeuction. It's guaranteed that cache
        // operations complete before page table changes become visible,
        // meaning that this clean on a VA that is about to be unmapped is safe.
        //

        MmpCleanPageTableCacheLine((PVOID)&(SecondLevelTable[SecondIndex]));
        if (SecondLevelEntry.Format != SLT_UNMAPPED) {
            MmpSendTlbInvalidateIpi((PVOID)FirstLevelTable, VirtualAddress, 1);
        }

    } else {

        ASSERT(SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED);

    }

    //
    // Unmap the page table.
    //

    MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
    KeLowerRunLevel(OldRunLevel);

    //
    // If there was nothing there, then there is nothing to free.
    //

    if (SecondLevelEntry.Entry == 0) {
        goto UnmapPageInOtherProcessEnd;
    }

    //
    // Free the physical page if requested.
    //

    if ((UnmapFlags & UNMAP_FLAG_FREE_PHYSICAL_PAGES) != 0) {
        PhysicalAddress = (ULONG)(SecondLevelEntry.Entry << PAGE_SHIFT);
        MmFreePhysicalPage(PhysicalAddress);
    }

    //
    // There is no dirty bit, so just assume it was dirty.
    //

    if (PageWasDirty != NULL) {
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

    ULONG FirstIndex;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;
    INTN MappedCount;
    ULONG Offset;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PageTablePhysical;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SecondIndex;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;
    PKPROCESS TypedProcess;

    //
    // Assert that this is called at low level. If it ever needs to be called
    // at dispatch, then all acquisitions of the page table spin lock will need
    // to be changed to raise to dispatch before acquiring.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    TypedProcess = Process;
    FirstLevelTable = (PFIRST_LEVEL_TABLE)(TypedProcess->PageDirectory);
    FirstIndex = FLT_INDEX(VirtualAddress);

    //
    // Create a page table if one is needed.
    //

    if (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) {
        MmpCreatePageTable(FirstLevelTable, VirtualAddress);
    }

    PageTablePhysical = (ULONG)(FirstLevelTable[FirstIndex].Entry <<
                                SLT_ALIGNMENT) & (~PAGE_MASK);

    FirstIndex = ALIGN_RANGE_DOWN(FirstIndex, 4);
    SecondIndex = SLT_INDEX(VirtualAddress);
    Offset = (FLT_INDEX(VirtualAddress) - FirstIndex) * SLT_SIZE;
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    MmpMapPage(PageTablePhysical, ProcessorBlock->SwapPage, MAP_FLAG_PRESENT);
    SecondLevelTable = (PSECOND_LEVEL_TABLE)(ProcessorBlock->SwapPage);
    SecondLevelTable = (PSECOND_LEVEL_TABLE)((UINTN)SecondLevelTable + Offset);

    //
    // This VA better be unmapped unless the caller requested an TLB
    // invalidation.
    //

    if (SecondLevelTable[SecondIndex].Entry != 0) {
        MappedCount = 0;

        ASSERT(SendTlbInvalidateIpi != FALSE);

        if (SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED) {
            SendTlbInvalidateIpi = FALSE;
        }

    } else {
        MappedCount = 1;
        SendTlbInvalidateIpi = FALSE;

        ASSERT(SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED);
    }

    SecondLevelTable[SecondIndex] = MmSecondLevelInitialValue;
    SecondLevelTable[SecondIndex].CacheTypeExtension = 0;
    SecondLevelTable[SecondIndex].NotGlobal = 0;

    //
    // Set the access attributes.
    //

    ASSERT(((MapFlags & MAP_FLAG_USER_MODE) == 0) ||
           (VirtualAddress < KERNEL_VA_START));

    if ((MapFlags & MAP_FLAG_READ_ONLY) != 0) {
        SecondLevelTable[SecondIndex].AccessExtension = 1;
        if ((MapFlags & MAP_FLAG_USER_MODE) == 0) {
            SecondLevelTable[SecondIndex].Access =
                                              SLT_XACCESS_SUPERVISOR_READ_ONLY;

        } else {
            SecondLevelTable[SecondIndex].Access =
                                              SLT_XACCESS_READ_ONLY_ALL_MODES;

            SecondLevelTable[SecondIndex].NotGlobal = 1;
        }

    } else {
        SecondLevelTable[SecondIndex].AccessExtension = 0;
        if ((MapFlags & MAP_FLAG_USER_MODE) == 0) {

            ASSERT((MapFlags & MAP_FLAG_GLOBAL) != 0);

            SecondLevelTable[SecondIndex].Access = SLT_ACCESS_SUPERVISOR;

        } else {
            SecondLevelTable[SecondIndex].Access = SLT_ACCESS_USER_FULL;
            SecondLevelTable[SecondIndex].NotGlobal = 1;
        }
    }

    //
    // Set the cache attributes.
    //

    if ((MapFlags & MAP_FLAG_CACHE_DISABLE) != 0) {
        SecondLevelTable[SecondIndex].CacheAttributes = SLT_UNCACHED;

    } else if ((MapFlags & MAP_FLAG_WRITE_THROUGH) != 0) {
        SecondLevelTable[SecondIndex].CacheAttributes = SLT_WRITE_THROUGH;

    } else {
        SecondLevelTable[SecondIndex].CacheTypeExtension = 1;
        SecondLevelTable[SecondIndex].CacheAttributes = SLT_WRITE_BACK;
    }

    //
    // Large pages are currently unsupported.
    //

    ASSERT((MapFlags & MAP_FLAG_LARGE_PAGE) == 0);

    SecondLevelTable[SecondIndex].Entry =
                                        ((ULONG)PhysicalAddress >> PAGE_SHIFT);

    if ((MapFlags & MAP_FLAG_PRESENT) != 0) {
        if ((MapFlags & MAP_FLAG_EXECUTE) != 0) {
            SecondLevelTable[SecondIndex].Format = SLT_SMALL_PAGE;

        } else {
            SecondLevelTable[SecondIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;
        }

    } else {
        SecondLevelTable[SecondIndex].Format = SLT_UNMAPPED;
    }

    //
    // Clean the cache line for the page table entry that was just modified.
    // Normally execution needs to be explicity serialized after a page table
    // is modified and cleaned, but the unmapping of the swap page will also
    // modify page tables and serialize execution. There is no need to do it
    // here. It is also guaranteed that cache operations complete before page
    // table changes become visible, meaning that this clean on a VA that is
    // about to be unmapped is safe.
    //

    MmpCleanPageTableCacheLine((PVOID)&(SecondLevelTable[SecondIndex]));

    //
    // Unmap the page table.
    //

    MmpUnmapPages(ProcessorBlock->SwapPage, 1, 0, NULL);
    KeLowerRunLevel(OldRunLevel);

    //
    // If requested, send a TLB invalidate IPI. This routine can be used for
    // remap, in which case the virtual address never got invalidated. TLB
    // invalidate serializes execution.
    //

    if (SendTlbInvalidateIpi != FALSE) {
        MmpSendTlbInvalidateIpi((PVOID)FirstLevelTable, VirtualAddress, 1);
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

    BOOL AllowWrites;
    BOOL ChangedSomething;
    BOOL ChangedSomethingThisRound;
    PVOID CleanEnd;
    BOOL CleanInvalidate;
    ULONG CleanSize;
    PVOID CleanStart;
    PVOID CurrentVirtual;
    ULONG FirstIndex;
    PFIRST_LEVEL_TABLE FirstLevelTable;
    ULONG Format;
    ULONG PageIndex;
    PKPROCESS Process;
    PFIRST_LEVEL_TABLE ProcessFirstLevelTable;
    ULONG SecondIndex;
    PSECOND_LEVEL_TABLE SecondLevelTable;
    BOOL SendInvalidateIpi;
    PSECOND_LEVEL_TABLE TableToClean;

    ChangedSomething = FALSE;
    CurrentVirtual = VirtualAddress;
    CleanInvalidate = TRUE;
    Process = PsGetCurrentProcess();
    SendInvalidateIpi = TRUE;

    ASSERT((Process != NULL) && (Process->PageDirectory != NULL));

    ProcessFirstLevelTable = Process->PageDirectory;
    FirstIndex = FLT_INDEX(VirtualAddress);
    if (VirtualAddress >= KERNEL_VA_START) {
        FirstLevelTable = MmKernelFirstLevelTable;

    } else {
        FirstLevelTable = ProcessFirstLevelTable;

        //
        // If there's only one thread in the process, then there's no need to
        // send a TLB invalidate IPI for this user mode address.
        //

        if (Process->ThreadCount <= 1) {
            SendInvalidateIpi = FALSE;
            if (Process->ThreadCount == 0) {
                CleanInvalidate = FALSE;
            }
        }
    }

    Format = SLT_UNMAPPED;
    if ((MapFlags & MAP_FLAG_PRESENT) != 0) {
        if ((MapFlags & MAP_FLAG_EXECUTE) != 0) {
            Format = SLT_SMALL_PAGE;

        } else {
            Format = SLT_SMALL_PAGE_NO_EXECUTE;
        }
    }

    CleanStart = NULL;
    CleanEnd = NULL;
    TableToClean = NULL;
    AllowWrites = ((MapFlags & MAP_FLAG_READ_ONLY) == 0);
    for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
        FirstIndex = FLT_INDEX(CurrentVirtual);
        SecondIndex = SLT_INDEX(CurrentVirtual);
        CurrentVirtual += PAGE_SIZE;
        if (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) {
            continue;
        }

        SecondLevelTable = GET_PAGE_TABLE(FirstIndex);

        //
        // If this moved to a new page table, then clean the last one if
        // necessary.
        //

        if ((TableToClean != NULL) && (SecondLevelTable != TableToClean)) {

            ASSERT(CleanEnd > CleanStart);

            CleanSize = CleanEnd - CleanStart;
            MmpCleanPageTableCacheRegion(CleanStart, CleanSize);
            TableToClean = NULL;
        }

        if (SecondLevelTable[SecondIndex].Entry == 0) {

            ASSERT(SecondLevelTable[SecondIndex].Format == SLT_UNMAPPED);

            continue;
        }

        ChangedSomethingThisRound = FALSE;
        if (((MapFlagsMask & MAP_FLAG_PRESENT) != 0) &&
            (SecondLevelTable[SecondIndex].Format != Format)) {

            ChangedSomethingThisRound = TRUE;
            SecondLevelTable[SecondIndex].Format = Format;
        }

        //
        // Use the access extension bit as an indicator as to whether or not the
        // page is mapped as read-only or not.
        //

        if (((MapFlagsMask & MAP_FLAG_READ_ONLY) != 0) &&
            (SecondLevelTable[SecondIndex].AccessExtension != !AllowWrites)) {

            ChangedSomethingThisRound = TRUE;
            if (AllowWrites == FALSE) {
                SecondLevelTable[SecondIndex].AccessExtension = 1;
                if (VirtualAddress >= KERNEL_VA_START) {
                    SecondLevelTable[SecondIndex].Access =
                                              SLT_XACCESS_SUPERVISOR_READ_ONLY;

                } else {
                    SecondLevelTable[SecondIndex].Access =
                                               SLT_XACCESS_READ_ONLY_ALL_MODES;

                    SecondLevelTable[SecondIndex].NotGlobal = 1;
                }

            } else {
                SecondLevelTable[SecondIndex].AccessExtension = 0;
                if (VirtualAddress >= KERNEL_VA_START) {
                    SecondLevelTable[SecondIndex].Access =
                                                         SLT_ACCESS_SUPERVISOR;

                } else {
                    SecondLevelTable[SecondIndex].Access = SLT_ACCESS_USER_FULL;
                    SecondLevelTable[SecondIndex].NotGlobal = 1;
                }
            }
        }

        if ((ChangedSomethingThisRound != FALSE) &&
            (CleanInvalidate != FALSE)) {

            if (TableToClean == NULL) {
                TableToClean = SecondLevelTable;
                CleanStart = &(SecondLevelTable[SecondIndex]);
            }

            CleanEnd = &(SecondLevelTable[SecondIndex]) + 1;
            ChangedSomething = TRUE;
        }
    }

    //
    // Clean the last page table if necessary.
    //

    if (TableToClean != NULL) {

        ASSERT(CleanEnd > CleanStart);

        CleanSize = CleanEnd - CleanStart;
        MmpCleanPageTableCacheRegion(CleanStart, CleanSize);
    }

    //
    // Invalidate the TLB if any mappings were changed. This also serializes
    // execution to make the page table updates visible to page table walks.
    // These TLB invalidations must happen after the page table cache cleans.
    //

    if (ChangedSomething != FALSE) {
        if (SendInvalidateIpi != FALSE) {
            MmpSendTlbInvalidateIpi(FirstLevelTable, VirtualAddress, PageCount);

        } else {
            CurrentVirtual = VirtualAddress;
            for (PageIndex = 0; PageIndex < PageCount; PageIndex += 1) {
                ArInvalidateTlbEntry(CurrentVirtual);
                CurrentVirtual += PAGE_SIZE;
            }
        }
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
    PFIRST_LEVEL_TABLE Destination;
    ULONG Index;
    PHYSICAL_ADDRESS Physical;
    PFIRST_LEVEL_TABLE Source;

    Destination = DestinationPageDirectory;
    Source = SourcePageDirectory;
    for (Index = 0; Index < FLT_INDEX(KERNEL_VA_START); Index += 4) {
        if (Source[Index].Format != FLT_COARSE_PAGE_TABLE) {
            continue;
        }

        ASSERT(Destination[Index].Format != FLT_COARSE_PAGE_TABLE);

        Physical = MmpAllocatePhysicalPages(1, 0);
        if (Physical == INVALID_PHYSICAL_ADDRESS) {

            //
            // Clean up and fail.
            //

            for (DeleteIndex = 0;
                 DeleteIndex < Index;
                 DeleteIndex += 4) {

                if (Source[DeleteIndex].Format == FLT_COARSE_PAGE_TABLE) {
                    Physical = (ULONG)(Destination[DeleteIndex].Entry <<
                                       SLT_ALIGNMENT);

                    Destination[DeleteIndex].Entry = 0;
                    MmFreePhysicalPage(Physical);
                }
            }

            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Destination[Index].Entry = (ULONG)Physical >> SLT_ALIGNMENT;
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

    PVOID CleanEnd;
    ULONG CleanSize;
    PVOID CleanStart;
    PVOID CurrentVirtual;
    PFIRST_LEVEL_TABLE DestinationDirectory;
    PSECOND_LEVEL_TABLE DestinationTable;
    ULONG Entry;
    ULONG FirstIndex;
    ULONG FirstIndexEnd;
    ULONG FirstIndexStart;
    ULONG LoopIndex;
    INTN MappedCount;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PageTable;
    PPROCESSOR_BLOCK ProcessorBlock;
    UINTN Remainder;
    ULONG SelfMapIndex;
    volatile SECOND_LEVEL_TABLE *SelfMapPageTable;
    PFIRST_LEVEL_TABLE SourceDirectory;
    PSECOND_LEVEL_TABLE SourceTable;
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

    CleanStart = NULL;
    CleanEnd = NULL;
    MappedCount = 0;
    CurrentVirtual = VirtualAddress;
    FirstIndexEnd = ALIGN_RANGE_DOWN(FLT_INDEX(VirtualEnd), 4);
    FirstIndexStart = ALIGN_RANGE_DOWN(FLT_INDEX(VirtualAddress), 4);
    for (FirstIndex = FirstIndexStart;
         FirstIndex <= FirstIndexEnd;
         FirstIndex += 4) {

        //
        // If the source directory does not have this section of four first
        // level entries mapped, skip it. It is assumed that the other three
        // are also not mapped.
        //

        if (SourceDirectory[FirstIndex].Format == FLT_UNMAPPED) {
            continue;
        }

        //
        // Determine the start end end page table indices that will need to be
        // synchronized.
        //

        TableIndexStart = SLT_INDEX(CurrentVirtual) +
                          ((FLT_INDEX(CurrentVirtual) - FirstIndex) *
                           (SLT_SIZE / sizeof(SECOND_LEVEL_TABLE)));

        if (FirstIndex < FirstIndexEnd) {
            TableIndexEnd = (SLT_SIZE * 4) / sizeof(SECOND_LEVEL_TABLE);

            //
            // As this is not the last time around the loop, up the current
            // virtual address.
            //

            CurrentVirtual += (TableIndexEnd - TableIndexStart) << PAGE_SHIFT;

        } else {
            TableIndexEnd = SLT_INDEX(VirtualEnd) +
                            ((FLT_INDEX(VirtualEnd) - FirstIndex) *
                             (SLT_SIZE / sizeof(SECOND_LEVEL_TABLE)));
        }

        //
        // If the destination has not encountered this first level entry yet,
        // allocate a page table for the destination. Then proceed to copy the
        // contents of the source page table over.
        //

        if (DestinationDirectory[FirstIndex].Format == FLT_UNMAPPED) {

            //
            // The preallocation step better have set up a page table to use.
            // Allocations are not possible in this routine because an image
            // section lock is held, which means the paging out thread could
            // be blocked trying to get it, and there could be no free pages
            // left.
            //

            PageTable = (ULONG)(DestinationDirectory[FirstIndex].Entry <<
                                SLT_ALIGNMENT);

            ASSERT(PageTable != 0);

            SourceTable = GET_PAGE_TABLE(FirstIndex);
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            ProcessorBlock = KeGetCurrentProcessorBlock();
            DestinationTable = ProcessorBlock->SwapPage;
            MmpMapPage(PageTable, DestinationTable, MAP_FLAG_PRESENT);
            if (TableIndexStart != 0) {
                RtlZeroMemory(DestinationTable,
                              TableIndexStart * sizeof(SECOND_LEVEL_TABLE));
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
                    if (SourceTable[TableIndex].AccessExtension == 0) {

                        //
                        // The "user full" and "read only all modes" values are
                        // identical, so there's no need to worry about an
                        // intermediate state where access extension is set but
                        // access is the stale old value. These mappings should
                        // also only be user mode mappings.
                        //

                        SourceTable[TableIndex].AccessExtension = 1;

                        ASSERT(SourceTable[TableIndex].Access ==
                               SLT_ACCESS_USER_FULL);

                        SourceTable[TableIndex].Access =
                                               SLT_XACCESS_READ_ONLY_ALL_MODES;

                        //
                        // Record that there was a modification if this is the
                        // first for this source page table. Always move the
                        // cache clean end forward by an entry.
                        //

                        if (CleanStart == NULL) {
                            CleanStart = &(SourceTable[TableIndex]);
                        }

                        CleanEnd = &(SourceTable[TableIndex]);
                    }
                }

                DestinationTable[TableIndex] = SourceTable[TableIndex];
            }

            if (TableIndexEnd !=
                ((SLT_SIZE * 4) / sizeof(SECOND_LEVEL_TABLE))) {

                Remainder = (SLT_SIZE * 4) -
                            (TableIndexEnd * sizeof(SECOND_LEVEL_TABLE));

                RtlZeroMemory(&(DestinationTable[TableIndexEnd]), Remainder);
            }

            MmpUnmapPages(DestinationTable, 1, 0, NULL);
            KeLowerRunLevel(OldRunLevel);

            //
            // Insert the new page table in the appropriate first level entries
            // and the self map.
            //

            for (LoopIndex = FirstIndex;
                 LoopIndex < FirstIndex + 4;
                 LoopIndex += 1) {

                ASSERT(DestinationDirectory[LoopIndex].Domain == 0);

                Entry = (((ULONG)PageTable) >> SLT_ALIGNMENT) +
                        (LoopIndex - FirstIndex);

                DestinationDirectory[LoopIndex].Entry = Entry;
                DestinationDirectory[LoopIndex].Format = FLT_COARSE_PAGE_TABLE;
            }

            MmpCleanPageTableCacheRegion(
                                    (PVOID)&(DestinationDirectory[FirstIndex]),
                                    4 * sizeof(FIRST_LEVEL_TABLE));

            SelfMapPageTable = (PVOID)DestinationDirectory + FLT_SIZE;
            SelfMapIndex = FirstIndex >> 2;
            Entry = (ULONG)PageTable >> PAGE_SHIFT;
            SelfMapPageTable[SelfMapIndex] = MmSecondLevelInitialValue;
            SelfMapPageTable[SelfMapIndex].Entry = Entry;
            SelfMapPageTable[SelfMapIndex].NotGlobal = 1;
            SelfMapPageTable[SelfMapIndex].AccessExtension = 0;
            SelfMapPageTable[SelfMapIndex].CacheTypeExtension = 1;
            SelfMapPageTable[SelfMapIndex].Access = SLT_ACCESS_SUPERVISOR;
            SelfMapPageTable[SelfMapIndex].CacheAttributes = SLT_WRITE_BACK;
            SelfMapPageTable[SelfMapIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;
            MmpCleanPageTableCacheLine(
                                     (PVOID)&(SelfMapPageTable[SelfMapIndex]));

        //
        // If the destination already has first level entries allocated at this
        // location, then just synchronize the source and destination tables
        // for the given VA region.
        //

        } else {
            PageTable = (ULONG)(DestinationDirectory[FirstIndex].Entry <<
                                SLT_ALIGNMENT);

            ASSERT(PageTable != INVALID_PHYSICAL_ADDRESS);

            SourceTable = GET_PAGE_TABLE(FirstIndex);
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            ProcessorBlock = KeGetCurrentProcessorBlock();
            DestinationTable = ProcessorBlock->SwapPage;
            MmpMapPage(PageTable, DestinationTable, MAP_FLAG_PRESENT);
            for (TableIndex = TableIndexStart;
                 TableIndex < TableIndexEnd;
                 TableIndex += 1) {

                if (SourceTable[TableIndex].Entry != 0) {
                    MappedCount += 1;
                    if (SourceTable[TableIndex].AccessExtension == 0) {

                        //
                        // The "user full" and "read only all modes" values are
                        // identical, so there's no need to worry about an
                        // intermediate state where access extension is set but
                        // access is the stale old value. These mappings should
                        // also only be user mode mappings.
                        //

                        SourceTable[TableIndex].AccessExtension = 1;

                        ASSERT(SourceTable[TableIndex].Access ==
                               SLT_ACCESS_USER_FULL);

                        SourceTable[TableIndex].Access =
                                               SLT_XACCESS_READ_ONLY_ALL_MODES;

                        //
                        // Record that there was a modification if this is the
                        // first for this source page table. Always move the
                        // cache clean end forward by an entry.
                        //

                        if (CleanStart == NULL) {
                            CleanStart = &(SourceTable[TableIndex]);
                        }

                        CleanEnd = &(SourceTable[TableIndex]);
                    }

                    DestinationTable[TableIndex] = SourceTable[TableIndex];
                }
            }

            MmpUnmapPages(DestinationTable, 1, 0, NULL);
            KeLowerRunLevel(OldRunLevel);
        }
    }

    //
    // If page tables in the source were modified, then they need to be
    // flushed. Do them all at once now. The address of the first entry
    // modified was saved, along with the address of the last. Serialization is
    // not necessary as these are user mode page table entries.
    //

    if (CleanStart != NULL) {

        ASSERT(CleanEnd >= CleanStart);

        CleanSize = (CleanEnd + sizeof(SECOND_LEVEL_TABLE)) - CleanStart;
        MmpCleanPageTableCacheRegion(CleanStart, CleanSize);
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
    ULONG FirstIndex;
    ULONG FirstIndexEnd;
    volatile FIRST_LEVEL_TABLE *FirstLevelTable;

    CurrentThread = KeGetCurrentThread();
    if (CurrentThread == NULL) {
        FirstLevelTable = MmKernelFirstLevelTable;
        if (FirstLevelTable == NULL) {
            return;
        }

    } else {
        FirstLevelTable = CurrentThread->OwningProcess->PageDirectory;
    }

    FirstIndex = FLT_INDEX(VirtualAddress);
    FirstIndexEnd = FLT_INDEX(VirtualAddress + Size - 1);

    ASSERT(FirstIndex <= FirstIndexEnd);

    while (FirstIndex <= FirstIndexEnd) {
        if (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) {
            MmpCreatePageTable(FirstLevelTable,
                               (PVOID)(FirstIndex << FLT_INDEX_SHIFT));
        }

        FirstIndex += 1;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
MmpCreatePageTable (
    volatile FIRST_LEVEL_TABLE *FirstLevelTable,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine creates a page table for the given directory and virtual
    address. This routine must be called at low level.

Arguments:

    FirstLevelTable - Supplies a pointer to the first level table that will own
        the page table.

    VirtualAddress - Supplies the virtual address that the page table will
        eventually service.

Return Value:

    None.

--*/

{

    ULONG Entry;
    ULONG FirstIndex;
    ULONG FirstIndexDown;
    ULONG LoopIndex;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PageTablePhysical;
    volatile SECOND_LEVEL_TABLE *SecondLevelTable;
    ULONG SelfMapIndex;
    volatile SECOND_LEVEL_TABLE *SelfMapPageTable;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    FirstIndex = FLT_INDEX(VirtualAddress);
    OldRunLevel = RunLevelCount;
    PageTablePhysical = INVALID_PHYSICAL_ADDRESS;
    MmpSyncKernelPageDirectory(FirstLevelTable, VirtualAddress);

    //
    // If the first entry table exists, just exit.
    //

    if (FirstLevelTable[FirstIndex].Format != FLT_UNMAPPED) {
        return;
    }

    //
    // Allocate a page outside of the lock. Create calls that are not just
    // synchronization calls need to be at low level.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (FirstLevelTable[FirstIndex].Entry != 0) {
        PageTablePhysical = (ULONG)(FirstLevelTable[FirstIndex].Entry <<
                                    SLT_ALIGNMENT);

        FirstLevelTable[FirstIndex].Entry = 0;

    } else {
        PageTablePhysical = MmpAllocatePhysicalPages(1, 0);
    }

    ASSERT(PageTablePhysical != INVALID_PHYSICAL_ADDRESS);

    //
    // Acquire the lock and check the first level entry again.
    //

    if (MmPageTableLock != NULL) {
        KeAcquireQueuedLock(MmPageTableLock);
    }

    //
    // Sync the kernel page directory again in case someone snuck in since the
    // last check.
    //

    MmpSyncKernelPageDirectory(FirstLevelTable, VirtualAddress);

    //
    // Check again to see if perhaps another thread created a page table while
    // this thread was allocating a page and acquiring the lock. If it still
    // unmapped, then action needs to be taken.
    //

    if (FirstLevelTable[FirstIndex].Format == FLT_UNMAPPED) {

        //
        // If this is a kernel VA, then the kernel's first level table should
        // not be mapped either.
        //

        ASSERT((VirtualAddress < KERNEL_VA_START) ||
               (MmKernelFirstLevelTable[FirstIndex].Format == FLT_UNMAPPED));

        //
        // Zero out the new page and set up the mappings in the first level
        // table. If the page tables are in the kernel VA region, then update
        // the kernel's first level table as well. It is guaranteed that cache
        // operations complete before page table changes become visible,
        // meaning it is safe to clean the page table region and then proceed
        // to unmap it without an intervening serialization.
        //

        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        SecondLevelTable = MmpMapPageTable(PageTablePhysical);
        RtlZeroMemory((PVOID)SecondLevelTable, PAGE_SIZE);
        MmpCleanPageTableCacheRegion((PVOID)SecondLevelTable, PAGE_SIZE);
        MmpUnmapPageTable(SecondLevelTable);
        KeLowerRunLevel(OldRunLevel);

        //
        // Map all 4 page tables at once, since at minimum page tables can
        // only be allocated in groups of 4.
        //

        FirstIndexDown = ALIGN_RANGE_DOWN(FirstIndex, 4);
        for (LoopIndex = FirstIndexDown;
             LoopIndex < FirstIndexDown + 4;
             LoopIndex += 1) {

            ASSERT(FirstLevelTable[LoopIndex].Format == FLT_UNMAPPED);
            ASSERT(FirstLevelTable[LoopIndex].Domain == 0);

            Entry = (((ULONG)PageTablePhysical) >> SLT_ALIGNMENT) +
                    (LoopIndex - FirstIndexDown);

            FirstLevelTable[LoopIndex].Entry = Entry;
            FirstLevelTable[LoopIndex].Format = FLT_COARSE_PAGE_TABLE;
            if (VirtualAddress >= KERNEL_VA_START) {
                MmKernelFirstLevelTable[LoopIndex] = FirstLevelTable[LoopIndex];
            }
        }

        MmpCleanPageTableCacheRegion((PVOID)&(FirstLevelTable[FirstIndexDown]),
                                     4 * sizeof(FIRST_LEVEL_TABLE));

        //
        // Clean the cache line containing the first level table entry.
        //

        if (VirtualAddress >= KERNEL_VA_START) {
            MmpCleanPageTableCacheRegion(
                                   &(MmKernelFirstLevelTable[FirstIndexDown]),
                                   4 * sizeof(FIRST_LEVEL_TABLE));
        }

        //
        // Also fix up the self map. In user mode, the self map page table
        // is allocated right after the page directory.
        //

        if (VirtualAddress >= KERNEL_VA_START) {
            SelfMapPageTable = GET_PAGE_TABLE(MmPageTablesFirstIndex);

        } else {
            SelfMapPageTable =
                      (PSECOND_LEVEL_TABLE)((PVOID)FirstLevelTable + FLT_SIZE);
        }

        SelfMapIndex = FirstIndex >> 2;
        Entry = (ULONG)PageTablePhysical >> PAGE_SHIFT;
        SelfMapPageTable[SelfMapIndex] = MmSecondLevelInitialValue;
        SelfMapPageTable[SelfMapIndex].Entry = Entry;
        if (VirtualAddress >= KERNEL_VA_START) {
            SelfMapPageTable[SelfMapIndex].NotGlobal = 0;

        } else {
            SelfMapPageTable[SelfMapIndex].NotGlobal = 1;
        }

        SelfMapPageTable[SelfMapIndex].AccessExtension = 0;
        SelfMapPageTable[SelfMapIndex].CacheTypeExtension = 1;
        SelfMapPageTable[SelfMapIndex].Access = SLT_ACCESS_SUPERVISOR;
        SelfMapPageTable[SelfMapIndex].CacheAttributes = SLT_WRITE_BACK;
        SelfMapPageTable[SelfMapIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;
        MmpCleanPageTableCacheLine((PVOID)&(SelfMapPageTable[SelfMapIndex]));
        ArSerializeExecution();
        PageTablePhysical = INVALID_PHYSICAL_ADDRESS;
    }

    if (MmPageTableLock != NULL) {
        KeReleaseQueuedLock(MmPageTableLock);
    }

    //
    // If a page table was allocated but not used, then free it.
    //

    if (PageTablePhysical != INVALID_PHYSICAL_ADDRESS) {
        MmFreePhysicalPage(PageTablePhysical);
    }

    return;
}

PSECOND_LEVEL_TABLE
MmpMapPageTable (
    PHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine temporarily maps a page table so it can be initialized or
    modified.

Arguments:

    PhysicalAddress - Supplies the physical address of the page table to be
        mapped.

Return Value:

    Returns a pointer to the virtual address that now points to that physical
    page.

--*/

{

    PSECOND_LEVEL_TABLE PageTable;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SecondIndex;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);
    ASSERT((MmPageTableLock == NULL) ||
           (KeIsQueuedLockHeld(MmPageTableLock) != FALSE));

    ProcessorBlock = KeGetCurrentProcessorBlock();
    SecondIndex = SLT_INDEX(ProcessorBlock->SwapPage);
    PageTable = GET_PAGE_TABLE(FLT_INDEX(ProcessorBlock->SwapPage));

    ASSERT(PageTable[SecondIndex].Format == SLT_UNMAPPED);

    //
    // Simply set the entry up and invalidate the TLB.
    //

    PageTable[SecondIndex] = MmSecondLevelInitialValue;
    PageTable[SecondIndex].Entry = (ULONG)PhysicalAddress >> PAGE_SHIFT;
    PageTable[SecondIndex].NotGlobal = 0;
    PageTable[SecondIndex].AccessExtension = 0;
    PageTable[SecondIndex].CacheTypeExtension = 1;
    PageTable[SecondIndex].Access = SLT_ACCESS_SUPERVISOR;
    PageTable[SecondIndex].CacheAttributes = SLT_WRITE_BACK;
    PageTable[SecondIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;
    MmpCleanPageTableCacheLine(&(PageTable[SecondIndex]));

    ASSERT(((UINTN)(ProcessorBlock->SwapPage) & PAGE_MASK) == 0);

    ArSerializeExecution();
    return ProcessorBlock->SwapPage;
}

VOID
MmpUnmapPageTable (
    volatile SECOND_LEVEL_TABLE *SecondLevelTable
    )

/*++

Routine Description:

    This routine unmaps a page mapped temporarily for manipulating page tables.

Arguments:

    SecondLevelTable - Supplies the second level page table to be unmapped.

Return Value:

    None.

--*/

{

    PSECOND_LEVEL_TABLE PageTable;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG SecondIndex;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);
    ASSERT((MmPageTableLock == NULL) ||
           (KeIsQueuedLockHeld(MmPageTableLock) != FALSE));

    ProcessorBlock = KeGetCurrentProcessorBlock();

    ASSERT((PVOID)((UINTN)SecondLevelTable & ~PAGE_MASK) ==
           ProcessorBlock->SwapPage);

    SecondIndex = SLT_INDEX(ProcessorBlock->SwapPage);
    PageTable = GET_PAGE_TABLE(FLT_INDEX(ProcessorBlock->SwapPage));

    ASSERT(PageTable[SecondIndex].Format != SLT_UNMAPPED);

    //
    // Unmap the page table from the stage, clean that page table to actually
    // flush the table, and then invalidate the stage's TLB entry.
    //

    *(PULONG)&(PageTable[SecondIndex]) = 0;
    MmpCleanPageTableCacheLine(&(PageTable[SecondIndex]));

    ASSERT(((UINTN)(ProcessorBlock->SwapPage) & PAGE_MASK) == 0);

    //
    // The TLB entry invalidation routine also serializes execution.
    //

    ArInvalidateTlbEntry(ProcessorBlock->SwapPage);
    return;
}

VOID
MmpSyncKernelPageDirectory (
    volatile FIRST_LEVEL_TABLE *ProcessFirstLevelTable,
    PVOID VirtualAddress
    )

/*++

Routine Description:

    This routine syncs the first level table entries of the given process page
    directory with the kernel first level tables. This only occurs for kernel
    addresses.

Arguments:

    ProcessFirstLevelTable - Supplies a pointer to the process first level to
        sync.

    VirtualAddress - Supplies the virtual address to sync.

Return Value:

    None.

--*/

{

    ULONG FirstIndex;
    ULONG FirstIndexDown;
    ULONG LoopIndex;

    if (VirtualAddress < KERNEL_VA_START) {
        return;
    }

    FirstIndex = FLT_INDEX(VirtualAddress);
    if (MmKernelFirstLevelTable[FirstIndex].Entry !=
        ProcessFirstLevelTable[FirstIndex].Entry) {

        ASSERT(ProcessFirstLevelTable[FirstIndex].Entry == 0);

        FirstIndexDown = ALIGN_RANGE_DOWN(FirstIndex, 4);
        for (LoopIndex = FirstIndexDown;
             LoopIndex < FirstIndexDown + 4;
             LoopIndex += 1) {

            ProcessFirstLevelTable[LoopIndex] =
                                            MmKernelFirstLevelTable[LoopIndex];
        }

        MmpCleanPageTableCacheRegion(
                              (PVOID)&(ProcessFirstLevelTable[FirstIndexDown]),
                              sizeof(FIRST_LEVEL_TABLE) * 4);

        ArSerializeExecution();
    }

    return;
}

VOID
MmpCleanPageTableCacheRegion (
    PVOID PageTable,
    ULONG Size
    )

/*++

Routine Description:

    This routine cleans the given region of page table space from the cache.

Arguments:

    PageTable - Supplies a pointer to a page table to clean from the cache.

    Size - Supplies the size of the page table region to clean, in bytes.

Return Value:

    None.

--*/

{

    ULONG CacheLineSize;

    //
    // If MP extensions are enabled, the data cache does not need to be cleaned.
    //

    if (MmMultiprocessingExtensions != FALSE) {
        return;
    }

    //
    // Align the given range up and down to cache line boundaries.
    //

    CacheLineSize = MmDataCacheLineSize;
    if (CacheLineSize == 0) {
        return;
    }

    Size += REMAINDER((UINTN)PageTable, CacheLineSize);
    PageTable = (PVOID)(UINTN)ALIGN_RANGE_DOWN((UINTN)PageTable, CacheLineSize);
    Size = ALIGN_RANGE_UP(Size, CacheLineSize);
    MmpCleanCacheRegion(PageTable, Size);
    return;
}

VOID
MmpCleanPageTableCacheLine (
    PVOID PageTableEntry
    )

/*++

Routine Description:

    This routine cleans the given page table entry's cache line if necessary.

Arguments:

    PageTableEntry - Supplies a pointer to a page table entry whose cache line
        needs to be cleaned.

Return Value:

    None.

--*/

{

    //
    // If MP extensions are enabled, the data cache does not need to be cleaned.
    //

    if (MmMultiprocessingExtensions != FALSE) {
        return;
    }

    MmpCleanCacheLine(PageTableEntry);
    return;
}

