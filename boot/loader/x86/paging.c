/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    paging.c

Abstract:

    This module implements page table support for the boot loader.

Author:

    Evan Green 30-Jul-2012

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>
#include "firmware.h"
#include "paging.h"
#include "bootlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// An arbitrary virtual address chosen for the initial page table stage and its
// page table.
//

#define INITIAL_PAGE_TABLE_STAGE (PVOID)(MAX_ULONG - PAGE_SIZE + 1)

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

PPTE BoPageDirectory;
MEMORY_DESCRIPTOR_LIST BoVirtualMap;
MEMORY_DESCRIPTOR BoVirtualMapDescriptors[BO_VIRTUAL_MAP_DESCRIPTOR_COUNT];

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
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    MmMdInitDescriptorList(&BoVirtualMap, MdlAllocationSourceNone);
    MmMdAddFreeDescriptorsToMdl(&BoVirtualMap,
                                BoVirtualMapDescriptors,
                                sizeof(BoVirtualMapDescriptors));

    MmMdInitDescriptor(&KernelSpace,
                       (UINTN)KERNEL_VA_START,
                       KERNEL_VA_END,
                       MemoryTypeFree);

    Status = MmMdAddDescriptorToList(&BoVirtualMap, &KernelSpace);
    if (!KSUCCESS(Status)) {
        goto InitializePagingStructuresEnd;
    }

    PageSize = MmPageSize();
    Status = FwAllocatePages(&PhysicalAddress,
                             PageSize,
                             PageSize,
                             MemoryTypePageTables);

    if (!KSUCCESS(Status)) {
        goto InitializePagingStructuresEnd;
    }

    ASSERT((UINTN)PhysicalAddress == PhysicalAddress);

    *PageDirectory = PhysicalAddress;
    BoPageDirectory = (PVOID)(UINTN)*PageDirectory;
    RtlZeroMemory(BoPageDirectory, PAGE_SIZE);
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

    ULONG AlignedSize;
    ULONG CurrentVirtual;
    ULONG DirectoryIndex;
    PMEMORY_DESCRIPTOR ExistingDescriptor;
    ULONGLONG MappedAddress;
    ULONG PageCount;
    ULONG PageOffset;
    ULONG PagesMapped;
    PPTE PageTable;
    ULONG PageTableIndex;
    MEMORY_TYPE PageTableMemoryType;
    PHYSICAL_ADDRESS PageTablePhysical;
    KSTATUS Status;
    ALLOCATION_STRATEGY Strategy;
    MEMORY_DESCRIPTOR VirtualSpace;
    BOOL VirtualSpaceAllocated;

    VirtualSpaceAllocated = FALSE;
    PageCount = 0;
    PageOffset = (ULONG)((UINTN)PhysicalAddress & PAGE_MASK);
    Size += PageOffset;
    if (BoPageDirectory == NULL) {
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
        AlignedSize = ALIGN_RANGE_UP(Size, PAGE_SIZE);
        MappedAddress = 0;
        Status = MmMdAllocateFromMdl(&BoVirtualMap,
                                     &MappedAddress,
                                     AlignedSize,
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
            *VirtualAddress = (PVOID)(ULONG)MappedAddress;
        }

    } else {
        MappedAddress = (ULONG)*VirtualAddress;

        //
        // Check to see if this region is occupied already, and fail if it is.
        //

        ExistingDescriptor = MmMdLookupDescriptor(
                                                &BoVirtualMap,
                                                (ULONG)*VirtualAddress,
                                                (ULONG)*VirtualAddress + Size);

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
                           MappedAddress + Size,
                           MemoryType);

        Status = MmMdAddDescriptorToList(&BoVirtualMap, &VirtualSpace);
        if (!KSUCCESS(Status)) {
            goto MapPhysicalAddressEnd;
        }
    }

    VirtualSpaceAllocated = TRUE;
    if ((VirtualAddress != NULL) && (*VirtualAddress != NULL)) {
        *VirtualAddress = (PVOID)((UINTN)*VirtualAddress + PageOffset);
    }

    //
    // Ensure the space is big enough.
    //

    if (MappedAddress + Size < MappedAddress) {
        Status = STATUS_INVALID_PARAMETER;
        goto MapPhysicalAddressEnd;
    }

    CurrentVirtual = (ULONG)MappedAddress;
    PageCount = ALIGN_RANGE_UP(Size, PAGE_SIZE) / PAGE_SIZE;
    PagesMapped = 0;
    while (PageCount != PagesMapped) {

        //
        // Look up the entry in the page directory.
        //

        DirectoryIndex = (CurrentVirtual & PDE_INDEX_MASK) >>
                         PAGE_DIRECTORY_SHIFT;

        if (BoPageDirectory[DirectoryIndex].Present == 0) {

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

            PageTable = (PPTE)(UINTN)PageTablePhysical;
            RtlZeroMemory(PageTable, PAGE_SIZE);
            BoPageDirectory[DirectoryIndex].Writable = 1;
            BoPageDirectory[DirectoryIndex].Entry =
                                              ((ULONG)PageTable) >> PAGE_SHIFT;

            BoPageDirectory[DirectoryIndex].Present = 1;

        } else {
            PageTable = (PPTE)(BoPageDirectory[DirectoryIndex].Entry <<
                               PAGE_SHIFT);
        }

        //
        // Look up the entry in the page table.
        //

        PageTableIndex = (CurrentVirtual & PTE_INDEX_MASK) >> PAGE_SHIFT;

        //
        // Set the various attributes and set the entry.
        //

        if ((Attributes & MAP_FLAG_READ_ONLY) != 0) {
            PageTable[PageTableIndex].Writable = 0;

        } else {
            PageTable[PageTableIndex].Writable = 1;
        }

        if ((Attributes & MAP_FLAG_USER_MODE) != 0) {
            PageTable[PageTableIndex].User = 1;
        }

        if ((Attributes & MAP_FLAG_WRITE_THROUGH) != 0) {
            PageTable[PageTableIndex].WriteThrough = 1;
        }

        if ((Attributes & MAP_FLAG_CACHE_DISABLE) != 0) {
            PageTable[PageTableIndex].CacheDisabled = 1;
        }

        if ((Attributes & MAP_FLAG_LARGE_PAGE) != 0) {
            PageTable[PageTableIndex].LargePage = 1;
        }

        if ((Attributes & MAP_FLAG_GLOBAL) != 0) {
            PageTable[PageTableIndex].Global = 1;
        }

        PageTable[PageTableIndex].Entry =
                                        ((ULONG)PhysicalAddress >> PAGE_SHIFT);

        PageTable[PageTableIndex].Present = 1;
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

    ULONG CurrentVirtual;
    ULONG DirectoryIndex;
    ULONGLONG EndAddress;
    ULONG PagesUnmapped;
    PPTE PageTable;
    ULONG PageTableIndex;
    KSTATUS Status;
    MEMORY_DESCRIPTOR VirtualSpace;

    if (BoPageDirectory == NULL) {
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

        DirectoryIndex = (CurrentVirtual & PDE_INDEX_MASK) >>
                         PAGE_DIRECTORY_SHIFT;

        if (BoPageDirectory[DirectoryIndex].Present != 0) {
            PageTable = (PPTE)(BoPageDirectory[DirectoryIndex].Entry <<
                               PAGE_SHIFT);

            //
            // Look up the entry in the page table, and clear the entry.
            //

            PageTableIndex = (CurrentVirtual & PTE_INDEX_MASK) >> PAGE_SHIFT;
            *((PULONG)(&(PageTable[PageTableIndex]))) = 0;
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
    ULONG DirectoryIndex;
    ULONG NewAttributesMask;
    UINTN PageCount;
    UINTN PagesChanged;
    PPTE PageTable;
    UINTN PageTableIndex;

    NewAttributesMask = (NewAttributes >> MAP_FLAG_PROTECT_SHIFT) &
                        MAP_FLAG_PROTECT_MASK;

    CurrentVirtual = (UINTN)VirtualAddress;
    PageCount = ALIGN_RANGE_UP(Size, PAGE_SIZE) / PAGE_SIZE;
    PagesChanged = 0;
    while (PageCount != PagesChanged) {
        DirectoryIndex = (CurrentVirtual & PDE_INDEX_MASK) >>
                         PAGE_DIRECTORY_SHIFT;

        PageTableIndex = (CurrentVirtual & PTE_INDEX_MASK) >> PAGE_SHIFT;
        PagesChanged += 1;
        CurrentVirtual += PAGE_SIZE;

        //
        // Look up the entry in the page directory.
        //

        if (BoPageDirectory[DirectoryIndex].Present == 0) {
            continue;
        }

        PageTable = (PPTE)(BoPageDirectory[DirectoryIndex].Entry <<
                           PAGE_SHIFT);

        //
        // Look up the entry in the page table.
        //

        ASSERT(PageTable[PageTableIndex].Present != 0);

        //
        // Set the various attributes and set the entry.
        //

        if ((NewAttributesMask & MAP_FLAG_READ_ONLY) != 0) {
            PageTable[PageTableIndex].Writable = 0;
            if ((NewAttributes & MAP_FLAG_READ_ONLY) == 0) {
                PageTable[PageTableIndex].Writable = 1;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_USER_MODE) != 0) {
            PageTable[PageTableIndex].User = 0;
            if ((NewAttributes & MAP_FLAG_USER_MODE) != 0) {
                PageTable[PageTableIndex].User = 1;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_WRITE_THROUGH) != 0) {
            PageTable[PageTableIndex].WriteThrough = 0;
            if ((NewAttributes & MAP_FLAG_WRITE_THROUGH) != 0) {
                PageTable[PageTableIndex].WriteThrough = 1;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_CACHE_DISABLE) != 0) {
            PageTable[PageTableIndex].CacheDisabled = 0;
            if ((NewAttributes & MAP_FLAG_CACHE_DISABLE) != 0) {
                PageTable[PageTableIndex].CacheDisabled = 1;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_GLOBAL) != 0) {
            PageTable[PageTableIndex].Global = 0;
            if ((NewAttributes & MAP_FLAG_GLOBAL) != 0) {
                PageTable[PageTableIndex].Global = 1;
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

Return Value:

    Status code.

--*/

{

    ULONG DirectoryIndex;
    MEMORY_DESCRIPTOR SelfMapDescriptor;
    ULONGLONG SelfMapEndAddress;
    KSTATUS Status;

    //
    // Map the page directory.
    //

    *PageDirectoryVirtual = (PVOID)-1;
    Status = BoMapPhysicalAddress(PageDirectoryVirtual,
                                  PageDirectoryPhysical,
                                  PAGE_SIZE,
                                  MAP_FLAG_GLOBAL,
                                  MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto MapPagingStructuresEnd;
    }

    //
    // Self map the page tables. By pointing one entry of the page directory to
    // itself, all page tables are automatically mapped into one place. This
    // requires an entire PDE entry.
    //

    for (DirectoryIndex = ((ULONG)KERNEL_VA_START >> PAGE_DIRECTORY_SHIFT);
         DirectoryIndex < (MAX_ULONG >> PAGE_DIRECTORY_SHIFT);
         DirectoryIndex += 1) {

        if (BoPageDirectory[DirectoryIndex].Present == 0) {

            //
            // An empty PDE was found. Mark the region as reserved in the MDL
            // for virtual memory.
            //

            SelfMapEndAddress = (DirectoryIndex + 1) << PAGE_DIRECTORY_SHIFT;
            MmMdInitDescriptor(&SelfMapDescriptor,
                               DirectoryIndex << PAGE_DIRECTORY_SHIFT,
                               SelfMapEndAddress,
                               MemoryTypePageTables);

            Status = MmMdAddDescriptorToList(&BoVirtualMap, &SelfMapDescriptor);
            if (!KSUCCESS(Status)) {
                goto MapPagingStructuresEnd;
            }

            //
            // Create the mapping by pointing the PDE entry to the page
            // directory. This results in accesses to addresses here to point
            // to page tables.
            //

            BoPageDirectory[DirectoryIndex].Writable = 1;
            BoPageDirectory[DirectoryIndex].Entry = (ULONG)BoPageDirectory >>
                                                           PAGE_SHIFT;

            BoPageDirectory[DirectoryIndex].Present = 1;
            *PageTablesVirtual =
                               (PVOID)(DirectoryIndex << PAGE_DIRECTORY_SHIFT);

            Status = STATUS_SUCCESS;
            goto MapPagingStructuresEnd;
        }
    }

    Status = STATUS_NO_MEMORY;

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

    StagesPageTable - Supplies a pointer where the page table corresponding to
        the stage's virtual address will be returned. This is useful so the
        stage's page table can be manipulating it without having to map it
        somewhere.

Return Value:

    Status code.

--*/

{

    ULONG DirectoryIndex;
    PPTE PageTable;
    ULONG PageTableIndex;
    KSTATUS Status;
    UINTN VirtualAddress;

    *PageTableStage = NULL;

    //
    // "Map" the page table stage, which is really just done to set up a
    // page table for it.
    //

    *PageTableStage = (PVOID)-1;
    Status = BoMapPhysicalAddress(PageTableStage,
                                  0,
                                  SWAP_VA_PAGES * PAGE_SIZE,
                                  MAP_FLAG_READ_ONLY,
                                  MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Unmap the page itself.
    //

    VirtualAddress = (UINTN)(*PageTableStage);
    DirectoryIndex = (VirtualAddress & PDE_INDEX_MASK) >> PAGE_DIRECTORY_SHIFT;
    PageTable = (PPTE)(BoPageDirectory[DirectoryIndex].Entry << PAGE_SHIFT);
    PageTableIndex = (VirtualAddress & PTE_INDEX_MASK) >> PAGE_SHIFT;
    *((PULONG)(&(PageTable[PageTableIndex]))) = 0;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//
