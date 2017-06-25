/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    paging.c

Abstract:

    This module implements ARM page table support for the boot loader.

Author:

    Evan Green 15-Aug-2012

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include "firmware.h"
#include "paging.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// An arbitrary virtual address chosen for the initial page table stage and its
// page table.
//

#define INITIAL_PAGE_TABLE_STAGE (PVOID)(MAX_ULONG - PAGE_SIZE + 1)
#define INITIAL_STAGE_PAGE_TABLE \
    (PVOID)((ULONG)INITIAL_PAGE_TABLE_STAGE - PAGE_SIZE)

//
// Define the maximum number of descriptors in the virtual map.
//

#define BO_VIRTUAL_MAP_DESCRIPTOR_COUNT 100

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

PFIRST_LEVEL_TABLE BoFirstLevelTable;
MEMORY_DESCRIPTOR_LIST BoVirtualMap;
MEMORY_DESCRIPTOR BoVirtualMapDescriptors[BO_VIRTUAL_MAP_DESCRIPTOR_COUNT];
PSECOND_LEVEL_TABLE BoSelfMapPageTable;
SECOND_LEVEL_TABLE BoSecondLevelInitialValue;
ULONG BoTtbrCacheAttributes;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BoInitializePagingStructures (
    PPHYSICAL_ADDRESS PageDirectory
    )

/*++

Routine Description:

    This routine initializes and returns a page directory for the kernel.

Arguments:

    PageDirectory - Supplies a pointer where the location of the page
        directory will be returned. This is a physical address, even though
        it's a pointer type.

Return Value:

    Status code.

--*/

{

    MEMORY_DESCRIPTOR KernelSpace;
    ULONG MultiprocessorIdRegister;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    //
    // Set the TTBR cache attributes based on the availability of the
    // multiprocessor extensions.
    //

    MultiprocessorIdRegister = ArGetMultiprocessorIdRegister();
    if ((MultiprocessorIdRegister & MPIDR_MP_EXTENSIONS_ENABLED) != 0) {
        BoSecondLevelInitialValue.Shared = 1;
        BoTtbrCacheAttributes = TTBR_MP_KERNEL_MASK;

    } else {
        BoTtbrCacheAttributes = TTBR_NO_MP_KERNEL_MASK;
    }

    //
    // Initialize the virtual memory map.
    //

    MmMdInitDescriptorList(&BoVirtualMap, MdlAllocationSourceNone);
    MmMdAddFreeDescriptorsToMdl(&BoVirtualMap,
                                BoVirtualMapDescriptors,
                                sizeof(BoVirtualMapDescriptors));

    MmMdInitDescriptor(&KernelSpace,
                       (ULONG)KERNEL_VA_START,
                       KERNEL_VA_END,
                       MemoryTypeFree);

    Status = MmMdAddDescriptorToList(&BoVirtualMap, &KernelSpace);
    if (!KSUCCESS(Status)) {
        goto InitializePagingStructuresEnd;
    }

    //
    // Allocate a first level table and self map.
    //

    Status = FwAllocatePages(&PhysicalAddress,
                             FLT_SIZE + PAGE_SIZE,
                             FLT_ALIGNMENT,
                             MemoryTypePageTables);

    if (!KSUCCESS(Status)) {
        goto InitializePagingStructuresEnd;
    }

    ASSERT((UINTN)PhysicalAddress == PhysicalAddress);
    ASSERT(PhysicalAddress == ALIGN_RANGE_DOWN(PhysicalAddress, FLT_ALIGNMENT));

    *PageDirectory = PhysicalAddress;
    BoFirstLevelTable = (PVOID)(UINTN)*PageDirectory;
    RtlZeroMemory(BoFirstLevelTable, FLT_SIZE);

    //
    // Initialize the self map page tables.
    //

    PhysicalAddress += FLT_SIZE;

    ASSERT((UINTN)PhysicalAddress == PhysicalAddress);
    ASSERT(PhysicalAddress == ALIGN_RANGE_DOWN(PhysicalAddress, PAGE_SIZE));

    BoSelfMapPageTable = (PVOID)(UINTN)PhysicalAddress;
    RtlZeroMemory(BoSelfMapPageTable, PAGE_SIZE);
    Status = STATUS_SUCCESS;

InitializePagingStructuresEnd:
    return Status;
}

KSTATUS
BoMapPhysicalAddress (
    PVOID *VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONG Size,
    ULONG Attributes,
    MEMORY_TYPE MemoryType
    )

/*++

Routine Description:

    This routine maps a physical address into the kernel's page table.

Arguments:

    VirtualAddress - Supplies a pointer to the virtual address to use on input.
        To allow the allocator to choose any virtual address, set the value of
        this pointer to -1. Upon return, contains the virtual address where
        the memory was mapped.

    PhysicalAddress - Supplies the physical address to map.

    Size - Supplies the size, in bytes, of the memory range.

    Attributes - Supplies the attributes to use when mapping this flag. See
        MAP_FLAG_* definitions.

    MemoryType - Supplies the type of memory to save this as in the virtual
        memory map.

Return Value:

    STATUS_SUCCESS if the mapping was successful.

    STATUS_NOT_INITIALIZED if the page directory has not been initialized.

    STATUS_INVALID_PARAMETER if the addresses do not share the same page
        offset.

    STATUS_MEMORY_CONFLICT if there is already a mapping at the desired
        virtual address.

    STATUS_NO_MEMORY if no free virtual space could be found.

--*/

{

    UINTN CurrentVirtual;
    ULONGLONG EndAddress;
    PMEMORY_DESCRIPTOR ExistingDescriptor;
    ULONG FirstLevelIndex;
    ULONG LoopIndex;
    ULONGLONG MappedAddress;
    ULONG Offset;
    ULONG PageCount;
    ULONG PageOffset;
    ULONG PagesMapped;
    PSECOND_LEVEL_TABLE PageTable;
    ULONG PageTableIndex;
    MEMORY_TYPE PageTableMemoryType;
    PHYSICAL_ADDRESS PageTablePhysical;
    ULONG SelfMapIndex;
    ULONGLONG StartAddress;
    KSTATUS Status;
    ALLOCATION_STRATEGY Strategy;
    MEMORY_DESCRIPTOR VirtualSpace;
    BOOL VirtualSpaceAllocated;

    PageCount = 0;
    PageOffset = (ULONG)((UINTN)PhysicalAddress & PAGE_MASK);
    Size += PageOffset;
    PageCount = ALIGN_RANGE_UP(Size, PAGE_SIZE) / PAGE_SIZE;
    VirtualSpaceAllocated = FALSE;
    if (BoFirstLevelTable == NULL) {
        return STATUS_NOT_INITIALIZED;
    }

    if ((VirtualAddress != NULL) &&
        (*VirtualAddress != (PVOID)-1) &&
        (((ULONG)*VirtualAddress & PAGE_MASK) !=
         ((ULONG)PhysicalAddress & PAGE_MASK))) {

        return STATUS_INVALID_PARAMETER;
    }

    Strategy = AllocationStrategyAnyAddress;
    if (MemoryType == MemoryTypeLoaderTemporary) {
        Strategy = AllocationStrategyHighestAddress;
    }

    //
    // Get the requested address, or find a virtual address if one was not
    // supplied.
    //

    if ((VirtualAddress == NULL) || (*VirtualAddress == (PVOID)-1)) {
        MappedAddress = 0;
        Status = MmMdAllocateFromMdl(&BoVirtualMap,
                                     (PVOID)&MappedAddress,
                                     PageCount << PAGE_SHIFT,
                                     PAGE_SIZE,
                                     0,
                                     MAX_UINTN,
                                     MemoryType,
                                     Strategy);

        if (!KSUCCESS(Status)) {
            Status = STATUS_NO_MEMORY;
            goto MapPhysicalAddressEnd;
        }

        if (VirtualAddress != NULL) {
            *VirtualAddress = (PVOID)(UINTN)MappedAddress;
        }

        StartAddress = MappedAddress;
        EndAddress = StartAddress + Size;

    } else {
        MappedAddress = (UINTN)*VirtualAddress;

        //
        // Check to see if this region is occupied already, and fail if it is.
        //

        StartAddress = (ULONG)*VirtualAddress;
        EndAddress = StartAddress + Size;
        ExistingDescriptor = MmMdLookupDescriptor(&BoVirtualMap,
                                                  StartAddress,
                                                  EndAddress);

        if ((ExistingDescriptor != NULL) &&
            (ExistingDescriptor->Type != MemoryTypeFree)) {

            Status = STATUS_MEMORY_CONFLICT;
            goto MapPhysicalAddressEnd;
        }

        //
        // Add the descriptor to the virtual memory map to account for its use.
        //

        MmMdInitDescriptor(&VirtualSpace,
                           MappedAddress,
                           EndAddress,
                           MemoryType);

        Status = MmMdAddDescriptorToList(&BoVirtualMap, &VirtualSpace);
        if (!KSUCCESS(Status)) {
            goto MapPhysicalAddressEnd;
        }
    }

    VirtualSpaceAllocated = TRUE;
    if ((VirtualAddress != NULL) && (*VirtualAddress != NULL)) {
        *VirtualAddress = (PVOID)((UINTN)(*VirtualAddress) + PageOffset);
    }

    //
    // Ensure the space is big enough.
    //

    if (EndAddress < MappedAddress) {
        Status = STATUS_INVALID_PARAMETER;
        goto MapPhysicalAddressEnd;
    }

    CurrentVirtual = (ULONG)MappedAddress;
    PagesMapped = 0;
    while (PageCount != PagesMapped) {

        //
        // Look up the entry in the page directory.
        //

        FirstLevelIndex = FLT_INDEX(CurrentVirtual);
        if (BoFirstLevelTable[FirstLevelIndex].Format == FLT_UNMAPPED) {

            //
            // The page table corresponding to this page does not exist.
            // Create one now.
            //

            PageTableMemoryType = MemoryTypePageTables;
            if (CurrentVirtual < (UINTN)KERNEL_VA_START) {

                ASSERT(MemoryType == MemoryTypeLoaderTemporary);

                PageTableMemoryType = MemoryTypeBootPageTables;
            }

            Status = FwAllocatePages(&PageTablePhysical,
                                     PAGE_SIZE,
                                     PAGE_SIZE,
                                     PageTableMemoryType);

            if (!KSUCCESS(Status)) {
                goto MapPhysicalAddressEnd;
            }

            ASSERT((UINTN)PageTablePhysical == PageTablePhysical);

            PageTable = (PSECOND_LEVEL_TABLE)(UINTN)PageTablePhysical;
            RtlZeroMemory(PageTable, PAGE_SIZE);

            //
            // Map all 4 page tables at once, since page tables can only be
            // allocated 4 at a time.
            //

            FirstLevelIndex = ALIGN_RANGE_DOWN(FirstLevelIndex, 4);
            for (LoopIndex = 0; LoopIndex < 4; LoopIndex += 1) {
                BoFirstLevelTable[FirstLevelIndex + LoopIndex].Format =
                                                         FLT_COARSE_PAGE_TABLE;

                BoFirstLevelTable[FirstLevelIndex + LoopIndex].Entry =
                              ((ULONG)PageTable >> SLT_ALIGNMENT) + LoopIndex;
            }

            //
            // Also set the corresponding entry in the self map page table so
            // this new page table can be accessed.
            //

            SelfMapIndex = FirstLevelIndex >> 2;
            BoSelfMapPageTable[SelfMapIndex] = BoSecondLevelInitialValue;
            BoSelfMapPageTable[SelfMapIndex].Entry =
                                                (ULONG)PageTable >> PAGE_SHIFT;

            BoSelfMapPageTable[SelfMapIndex].NotGlobal = 0;
            BoSelfMapPageTable[SelfMapIndex].AccessExtension = 0;
            BoSelfMapPageTable[SelfMapIndex].CacheTypeExtension = 1;
            BoSelfMapPageTable[SelfMapIndex].Access = SLT_ACCESS_SUPERVISOR;
            BoSelfMapPageTable[SelfMapIndex].CacheAttributes = SLT_WRITE_BACK;
            BoSelfMapPageTable[SelfMapIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;

            //
            // Since the page just allocated for page tables is always aligned
            // to 4 tables, the actual page table to be modified may be
            // somewhere in the middle of the page. Take the offset from the
            // rounded down index to get the current page table.
            //

            Offset = (FLT_INDEX(CurrentVirtual) - FirstLevelIndex) * SLT_SIZE;
            PageTable = (PSECOND_LEVEL_TABLE)((ULONG)PageTable + Offset);

        } else {
            PageTable =
               (PSECOND_LEVEL_TABLE)(BoFirstLevelTable[FirstLevelIndex].Entry <<
                                     SLT_ALIGNMENT);
        }

        //
        // Look up the entry in the page table.
        //

        PageTableIndex = SLT_INDEX(CurrentVirtual);

        ASSERT(*((PULONG)&(PageTable[PageTableIndex])) == 0);

        PageTable[PageTableIndex] = BoSecondLevelInitialValue;

        //
        // Set the access attributes.
        //

        if ((Attributes & MAP_FLAG_READ_ONLY) != 0) {
            PageTable[PageTableIndex].AccessExtension = 1;
            if ((Attributes & MAP_FLAG_USER_MODE) != 0) {
                PageTable[PageTableIndex].Access =
                                               SLT_XACCESS_READ_ONLY_ALL_MODES;

            } else {
                PageTable[PageTableIndex].Access =
                                              SLT_XACCESS_SUPERVISOR_READ_ONLY;
            }

        } else {
            PageTable[PageTableIndex].AccessExtension = 0;
            if ((Attributes & MAP_FLAG_USER_MODE) != 0) {
                PageTable[PageTableIndex].Access = SLT_ACCESS_USER_FULL;

            } else {
                PageTable[PageTableIndex].Access = SLT_ACCESS_SUPERVISOR;
            }
        }

        //
        // Set the cache attributes.
        //

        if ((Attributes & MAP_FLAG_WRITE_THROUGH) != 0) {
            PageTable[PageTableIndex].CacheAttributes = SLT_WRITE_THROUGH;

        } else if ((Attributes & MAP_FLAG_CACHE_DISABLE) != 0) {
            PageTable[PageTableIndex].CacheAttributes = SLT_UNCACHED;

        } else {
            PageTable[PageTableIndex].CacheTypeExtension = 1;
            PageTable[PageTableIndex].CacheAttributes = SLT_WRITE_BACK;
        }

        //
        // Large pages are currently unsupported.
        //

        ASSERT((Attributes & MAP_FLAG_LARGE_PAGE) == 0);

        //
        // Set the global or non-global attributes.
        //

        if ((Attributes & MAP_FLAG_GLOBAL) != 0) {
            PageTable[PageTableIndex].NotGlobal = 0;

        } else {
            PageTable[PageTableIndex].NotGlobal = 1;
        }

        PageTable[PageTableIndex].Entry =
                                     ((ULONG)PhysicalAddress >> PAGE_SHIFT);

        if ((Attributes & MAP_FLAG_EXECUTE) != 0) {
            PageTable[PageTableIndex].Format = SLT_SMALL_PAGE;

        } else {
            PageTable[PageTableIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;
        }

        PagesMapped += 1;
        PhysicalAddress += PAGE_SIZE;
        CurrentVirtual += PAGE_SIZE;
    }

    Status = STATUS_SUCCESS;

MapPhysicalAddressEnd:
    if ((!KSUCCESS(Status)) && (VirtualSpaceAllocated != FALSE)) {
        BoUnmapPhysicalAddress((PVOID)(UINTN)MappedAddress, PageCount);
    }

    return Status;
}

KSTATUS
BoUnmapPhysicalAddress (
    PVOID VirtualAddress,
    ULONG PageCount
    )

/*++

Routine Description:

    This routine unmaps a region of virtual address space from the kernel's
    address space.

Arguments:

    VirtualAddress - Supplies the virtual address to unmap.

    PageCount - Supplies the number of pages to unmap.

Return Value:

    Status code.

--*/

{

    UINTN CurrentVirtual;
    ULONGLONG EndAddress;
    ULONG EntryPhysical;
    ULONG FirstLevelIndex;
    ULONG PagesUnmapped;
    PSECOND_LEVEL_TABLE PageTable;
    ULONG PageTableIndex;
    KSTATUS Status;
    MEMORY_DESCRIPTOR VirtualSpace;

    if (BoFirstLevelTable == NULL) {
        return STATUS_NOT_INITIALIZED;
    }

    EndAddress = (ULONGLONG)(UINTN)VirtualAddress +
                 ((ULONGLONG)PageCount << PAGE_SHIFT);

    MmMdInitDescriptor(&VirtualSpace,
                       (UINTN)VirtualAddress,
                       EndAddress,
                       MemoryTypeFree);

    Status = MmMdAddDescriptorToList(&BoVirtualMap, &VirtualSpace);
    CurrentVirtual = (ULONG)VirtualAddress;
    PagesUnmapped = 0;
    while (PageCount != PagesUnmapped) {

        //
        // Look up the entry in the page directory.
        //

        FirstLevelIndex = FLT_INDEX(CurrentVirtual);
        if (BoFirstLevelTable[FirstLevelIndex].Format != FLT_UNMAPPED) {
            EntryPhysical = BoFirstLevelTable[FirstLevelIndex].Entry;
            PageTable = (PSECOND_LEVEL_TABLE)(EntryPhysical << SLT_ALIGNMENT);

            //
            // Look up the entry in the page table.
            //

            PageTableIndex = SLT_INDEX(CurrentVirtual);
            *((PULONG)&(PageTable[PageTableIndex])) = 0;
        }

        PagesUnmapped += 1;
        CurrentVirtual += PAGE_SIZE;
    }

    return Status;
}

VOID
BoChangeMappingAttributes (
    PVOID VirtualAddress,
    UINTN Size,
    ULONG NewAttributes
    )

/*++

Routine Description:

    This routine changes the mapping attributes for a region of VA space.

Arguments:

    VirtualAddress - Supplies the virtual address of the region to change.

    Size - Supplies the size of the region in bytes to change.

    NewAttributes - Supplies the new attributes to set. The lower 16-bits
        provide the new attributes and the upper 16-bits indicate which of
        those attributes are masked for modification.

Return Value:

    None.

--*/

{

    UINTN CurrentVirtual;
    ULONG FirstLevelIndex;
    ULONG NewAttributesMask;
    UINTN PageCount;
    UINTN PagesComplete;
    PSECOND_LEVEL_TABLE PageTable;
    ULONG PageTableIndex;

    NewAttributesMask = (NewAttributes >> MAP_FLAG_PROTECT_SHIFT) &
                        MAP_FLAG_PROTECT_MASK;

    CurrentVirtual = (UINTN)VirtualAddress;
    PageCount = ALIGN_RANGE_UP(Size, PAGE_SIZE) / PAGE_SIZE;
    PagesComplete = 0;
    while (PageCount != PagesComplete) {
        FirstLevelIndex = FLT_INDEX(CurrentVirtual);
        PageTableIndex = SLT_INDEX(CurrentVirtual);
        CurrentVirtual += PAGE_SIZE;
        PagesComplete += 1;

        //
        // Look up the entry in the page directory.
        //

        if (BoFirstLevelTable[FirstLevelIndex].Format == FLT_UNMAPPED) {
            continue;

        } else {
            PageTable =
               (PSECOND_LEVEL_TABLE)(BoFirstLevelTable[FirstLevelIndex].Entry <<
                                     SLT_ALIGNMENT);
        }

        //
        // Look up the entry in the page table.
        //

        ASSERT(PageTable[PageTableIndex].Format != SLT_UNMAPPED);

        //
        // Set the access attributes.
        //

        if ((NewAttributesMask & MAP_FLAG_READ_ONLY) != 0) {
            if ((NewAttributes & MAP_FLAG_READ_ONLY) != 0) {
                PageTable[PageTableIndex].AccessExtension = 1;
                if (PageTable[PageTableIndex].Access == SLT_ACCESS_USER_FULL) {
                    PageTable[PageTableIndex].Access =
                                               SLT_XACCESS_READ_ONLY_ALL_MODES;

                } else if (PageTable[PageTableIndex].Access ==
                           SLT_ACCESS_SUPERVISOR) {

                    PageTable[PageTableIndex].Access =
                                              SLT_XACCESS_SUPERVISOR_READ_ONLY;
                }

            } else {
                PageTable[PageTableIndex].AccessExtension = 0;
                if (PageTable[PageTableIndex].Access ==
                    SLT_XACCESS_READ_ONLY_ALL_MODES) {

                    PageTable[PageTableIndex].Access = SLT_ACCESS_USER_FULL;

                } else if (PageTable[PageTableIndex].Access ==
                           SLT_XACCESS_SUPERVISOR_READ_ONLY) {

                    PageTable[PageTableIndex].Access = SLT_ACCESS_SUPERVISOR;
                }
            }
        }

        if ((NewAttributesMask & MAP_FLAG_USER_MODE) != 0) {
            if (PageTable[PageTableIndex].AccessExtension == 1) {
                if ((NewAttributes & MAP_FLAG_USER_MODE) != 0) {
                    PageTable[PageTableIndex].Access =
                                               SLT_XACCESS_READ_ONLY_ALL_MODES;

                } else {
                    PageTable[PageTableIndex].Access =
                                              SLT_XACCESS_SUPERVISOR_READ_ONLY;
                }

            } else {
                if ((NewAttributes & MAP_FLAG_USER_MODE) != 0) {
                    PageTable[PageTableIndex].Access = SLT_ACCESS_USER_FULL;

                } else {
                    PageTable[PageTableIndex].Access = SLT_ACCESS_SUPERVISOR;
                }
            }
        }

        //
        // Set the cache attributes.
        //

        if (((NewAttributesMask & MAP_FLAG_WRITE_THROUGH) != 0) ||
            ((NewAttributesMask & MAP_FLAG_CACHE_DISABLE) != 0)) {

            if ((NewAttributes & MAP_FLAG_WRITE_THROUGH) != 0) {
                PageTable[PageTableIndex].CacheAttributes = SLT_WRITE_THROUGH;

            } else if ((NewAttributes & MAP_FLAG_CACHE_DISABLE) != 0) {
                PageTable[PageTableIndex].CacheAttributes = SLT_UNCACHED;

            } else {
                PageTable[PageTableIndex].CacheTypeExtension = 1;
                PageTable[PageTableIndex].CacheAttributes = SLT_WRITE_BACK;
            }
        }

        //
        // Large pages are currently unsupported.
        //

        ASSERT((NewAttributes & MAP_FLAG_LARGE_PAGE) == 0);

        //
        // Set the global or non-global attributes.
        //

        if ((NewAttributesMask & MAP_FLAG_GLOBAL) != 0) {
            if ((NewAttributes & MAP_FLAG_GLOBAL) != 0) {
                PageTable[PageTableIndex].NotGlobal = 0;

            } else {
                PageTable[PageTableIndex].NotGlobal = 1;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_EXECUTE) != 0) {
            if ((NewAttributes & MAP_FLAG_EXECUTE) != 0) {
                PageTable[PageTableIndex].Format = SLT_SMALL_PAGE;

            } else {
                PageTable[PageTableIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;
            }
        }
    }

    return;
}

KSTATUS
BoMapPagingStructures (
    PHYSICAL_ADDRESS PageDirectoryPhysical,
    PVOID *PageDirectoryVirtual,
    PVOID *PageTablesVirtual
    )

/*++

Routine Description:

    This routine maps the page directory, page tables, and any other paging
    related structures needed by MM into the kernel virtual address space.

Arguments:

    PageDirectoryPhysical - Supplies the physical address of the page directory.

    PageDirectoryVirtual - Supplies a pointer where the virtual address of the
        page directory will be stored.

    PageTablesVirtual - Supplies a pointer where the virtual address of the
        page tables will be stored.

    SelfMapPageTableVirtual - Supplies a pointer where the virtual address of
        the page table used to map all other page tables will be returned. This
        is only used on architectures where the page directory itself cannot
        be used as a page table (ie ARM).

Return Value:

    Status code.

--*/

{

    ULONGLONG Address;
    ULONG Entry;
    ULONG FirstLevelIndex;
    ULONG LoopIndex;
    ULONG SelfMapIndex;
    ULONG SelfMapSize;
    KSTATUS Status;

    //
    // Map the page directory and the appended self map.
    //

    *PageDirectoryVirtual = (PVOID)-1;
    Status = BoMapPhysicalAddress(PageDirectoryVirtual,
                                  PageDirectoryPhysical,
                                  FLT_SIZE + PAGE_SIZE,
                                  MAP_FLAG_GLOBAL,
                                  MemoryTypePageTables);

    if (!KSUCCESS(Status)) {
        goto MapPagingStructuresEnd;
    }

    //
    // Allocate space for the self map. It must be aligned to take up a natural
    // slot of 4 first level table entries.
    //

    *PageTablesVirtual = (PVOID)-1;
    SelfMapSize = PAGE_SIZE * (PAGE_SIZE / sizeof(SECOND_LEVEL_TABLE));
    Status = MmMdAllocateFromMdl(&BoVirtualMap,
                                 &Address,
                                 SelfMapSize,
                                 SelfMapSize,
                                 0,
                                 MAX_UINTN,
                                 MemoryTypeMmStructures,
                                 AllocationStrategyAnyAddress);

    if (!KSUCCESS(Status)) {
        Status = STATUS_NO_MEMORY;
        goto MapPagingStructuresEnd;
    }

    ASSERT((UINTN)Address == Address);

    *PageTablesVirtual = (PVOID)(UINTN)Address;

    //
    // The page tables had better be allocated in a group of 4 first level table
    // entries.
    //

    ASSERT(ALIGN_RANGE_DOWN(FLT_INDEX(*PageTablesVirtual), 4) ==
           FLT_INDEX(*PageTablesVirtual));

    FirstLevelIndex = FLT_INDEX(*PageTablesVirtual);
    for (LoopIndex = 0; LoopIndex < 4; LoopIndex += 1) {
        Entry = (((ULONG)BoSelfMapPageTable) >> SLT_ALIGNMENT) + LoopIndex;
        BoFirstLevelTable[FirstLevelIndex + LoopIndex].Entry = Entry;
        BoFirstLevelTable[FirstLevelIndex + LoopIndex].Format =
                                                 FLT_COARSE_PAGE_TABLE;
    }

    //
    // Make sure that the self map references back to itself. So the result of
    // "get page table" of the self map equals the self map page table.
    //

    SelfMapIndex = FirstLevelIndex >> 2;
    Entry = (((ULONG)BoSelfMapPageTable) >> PAGE_SHIFT);
    BoSelfMapPageTable[SelfMapIndex] = BoSecondLevelInitialValue;
    BoSelfMapPageTable[SelfMapIndex].Entry = Entry;
    BoSelfMapPageTable[SelfMapIndex].NotGlobal = 0;
    BoSelfMapPageTable[SelfMapIndex].AccessExtension = 0;
    BoSelfMapPageTable[SelfMapIndex].CacheTypeExtension = 1;
    BoSelfMapPageTable[SelfMapIndex].Access = SLT_ACCESS_SUPERVISOR;
    BoSelfMapPageTable[SelfMapIndex].CacheAttributes = SLT_WRITE_BACK;
    BoSelfMapPageTable[SelfMapIndex].Format = SLT_SMALL_PAGE_NO_EXECUTE;
    Status = STATUS_SUCCESS;

MapPagingStructuresEnd:
    return Status;
}

KSTATUS
BoCreatePageTableStage (
    PHYSICAL_ADDRESS PageDirectoryPhysical,
    PVOID *PageTableStage
    )

/*++

Routine Description:

    This routine sets up a page table staging area, a page of virtual memory
    available for mapping new page tables into. The virtual address where the
    stage resides will have a valid page table, so that in attempting to map
    a page table, one does not have to be created.

Arguments:

    PageDirectoryPhysical - Supplies the physical address of the page directory.

    PageTableStage - Supplies a pointer where the virtual address of the page
        table stage will reside.

Return Value:

    Status code.

--*/

{

    ULONG FirstIndex;
    PFIRST_LEVEL_TABLE FirstLevelTable;
    ULONG Offset;
    UINTN PageTablePhysical;
    ULONG SecondIndex;
    PSECOND_LEVEL_TABLE SecondLevelTable;
    PVOID StageVirtual;
    KSTATUS Status;

    ASSERT((UINTN)PageDirectoryPhysical == PageDirectoryPhysical);

    FirstLevelTable = (PVOID)(UINTN)PageDirectoryPhysical;

    //
    // "Map" the page table stage, which is really just done to set up a
    // page table for it.
    //

    StageVirtual = INITIAL_PAGE_TABLE_STAGE;
    Status = BoMapPhysicalAddress(&StageVirtual,
                                  0,
                                  SWAP_VA_PAGES * PAGE_SIZE,
                                  MAP_FLAG_READ_ONLY,
                                  MemoryTypeMmStructures);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    FirstIndex = FLT_INDEX(StageVirtual);

    ASSERT(FirstLevelTable[FirstIndex].Format != FLT_UNMAPPED);

    PageTablePhysical = (ULONG)(FirstLevelTable[FirstIndex].Entry <<
                                SLT_ALIGNMENT) & (~PAGE_MASK);

    //
    // Unmap the page table stage.
    //

    FirstIndex = FLT_INDEX(StageVirtual);
    FirstIndex = ALIGN_RANGE_DOWN(FirstIndex, 4);
    Offset = (FLT_INDEX(StageVirtual) - FirstIndex) * SLT_SIZE;
    SecondLevelTable = (PVOID)(PageTablePhysical + Offset);
    SecondIndex = SLT_INDEX(StageVirtual);
    *((PULONG)&(SecondLevelTable[SecondIndex])) = 0;
    *PageTableStage = StageVirtual;
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//
