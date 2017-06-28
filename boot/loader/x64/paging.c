/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    paging.c

Abstract:

    This module implements page table support for the boot loader.

Author:

    Evan Green 6-Jun-2017

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x64.h>
#include "firmware.h"
#include "paging.h"
#include "bootlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of descriptors in the virtual map.
//

#define BO_VIRTUAL_MAP_DESCRIPTOR_COUNT 100

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
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

    //
    // Set up the self map.
    //

    BoPageDirectory[X64_SELF_MAP_INDEX] =
             PhysicalAddress | X86_PTE_PRESENT | X86_PTE_WRITABLE | X86_PTE_NX;

    MmMdInitDescriptor(
              &KernelSpace,
              X64_CANONICAL_HIGH | (X64_SELF_MAP_INDEX << X64_PML4E_SHIFT),
              X64_CANONICAL_HIGH | (X64_SELF_MAP_INDEX + 1) << X64_PML4E_SHIFT,
              MemoryTypePageTables);

    Status = MmMdAddDescriptorToList(&BoVirtualMap, &KernelSpace);
    if (!KSUCCESS(Status)) {
        goto InitializePagingStructuresEnd;
    }

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

    UINTN AlignedSize;
    UINTN CurrentVirtual;
    ULONG EntryShift;
    PMEMORY_DESCRIPTOR ExistingDescriptor;
    ULONG Level;
    ULONGLONG MappedAddress;
    UINTN PageCount;
    ULONG PageOffset;
    UINTN PagesMapped;
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
        (((UINTN)*VirtualAddress & PAGE_MASK) !=
         ((UINTN)PhysicalAddress & PAGE_MASK))) {

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
            *VirtualAddress = (PVOID)MappedAddress;
        }

    } else {
        MappedAddress = (UINTN)*VirtualAddress;

        //
        // Check to see if this region is occupied already, and fail if it is.
        //

        ExistingDescriptor = MmMdLookupDescriptor(
                                                &BoVirtualMap,
                                                (UINTN)*VirtualAddress,
                                                (UINTN)*VirtualAddress + Size);

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

    CurrentVirtual = (UINTN)MappedAddress;
    PageCount = ALIGN_RANGE_UP(Size, PAGE_SIZE) / PAGE_SIZE;
    PagesMapped = 0;
    while (PageCount != PagesMapped) {

        //
        // Get to the lowest level page table, allocating page tables along the
        // way.
        //

        PageTable = BoPageDirectory;
        EntryShift = X64_PML4E_SHIFT;
        for (Level = 0; Level < X64_PAGE_LEVEL - 1; Level += 1) {
            PageTableIndex = (CurrentVirtual >> EntryShift) & X64_PT_MASK;
            PageTable += PageTableIndex;
            EntryShift -= X64_PTE_BITS;
            if ((*PageTable & X86_PTE_PRESENT) == 0) {
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

                RtlZeroMemory((PVOID)PageTablePhysical, PAGE_SIZE);
                *PageTable =
                        PageTablePhysical | X86_PTE_PRESENT | X86_PTE_WRITABLE;
            }

            PageTable = (PPTE)(X86_PTE_ENTRY(*PageTable));
        }

        //
        // Set up the page.
        //

        PageTableIndex = (CurrentVirtual & X64_PTE_MASK) >> PAGE_SHIFT;
        PageTable += PageTableIndex;

        ASSERT(*PageTable == 0);

        *PageTable = PhysicalAddress | X86_PTE_PRESENT;
        if ((Attributes & MAP_FLAG_READ_ONLY) == 0) {
            *PageTable |= X86_PTE_WRITABLE;
        }

        if ((Attributes & MAP_FLAG_USER_MODE) != 0) {
            *PageTable |= X86_PTE_USER_MODE;
        }

        if ((Attributes & MAP_FLAG_WRITE_THROUGH) != 0) {
            *PageTable |= X86_PTE_WRITE_THROUGH;
        }

        if ((Attributes & MAP_FLAG_CACHE_DISABLE) != 0) {
            *PageTable |= X86_PTE_CACHE_DISABLED;
        }

        if ((Attributes & MAP_FLAG_GLOBAL) != 0) {
            *PageTable |= X86_PTE_GLOBAL;
        }

        if ((Attributes & MAP_FLAG_EXECUTE) == 0) {
            *PageTable |= X86_PTE_NX;
        }

        ASSERT((Attributes & MAP_FLAG_LARGE_PAGE) == 0);

        PagesMapped += 1;
        PhysicalAddress += PAGE_SIZE;
        CurrentVirtual += PAGE_SIZE;
    }

    Status = STATUS_SUCCESS;

MapPhysicalAddressEnd:
    if ((!KSUCCESS(Status)) && (VirtualSpaceAllocated != FALSE)) {
        BoUnmapPhysicalAddress((PVOID)MappedAddress, PageCount);
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
    CurrentVirtual = (UINTN)VirtualAddress;
    while (CurrentVirtual < EndAddress) {

        //
        // Get down to the lowest level page directory.
        //

        PageTable = BoPageDirectory;
        PageTableIndex = (CurrentVirtual >> X64_PML4E_SHIFT) & X64_PT_MASK;
        PageTable += PageTableIndex;
        if ((*PageTable & X86_PTE_PRESENT) == 0) {
            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        PageTable = (PPTE)(X86_PTE_ENTRY(*PageTable));
        PageTableIndex = (CurrentVirtual >> X64_PDPE_SHIFT) & X64_PT_MASK;
        PageTable += PageTableIndex;
        if ((*PageTable & X86_PTE_PRESENT) == 0) {
            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        PageTable = (PPTE)(X86_PTE_ENTRY(*PageTable));
        PageTableIndex = (CurrentVirtual >> X64_PDE_SHIFT) & X64_PT_MASK;
        PageTable += PageTableIndex;
        if ((*PageTable & X86_PTE_PRESENT) == 0) {
            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        if ((*PageTable & X86_PTE_LARGE) != 0) {

            ASSERT((CurrentVirtual & (_2MB - 1)) == 0);
            ASSERT((EndAddress - CurrentVirtual) >= _2MB);

            *PageTable = 0;
            CurrentVirtual += _2MB;
            continue;
        }

        PageTable = (PPTE)(X86_PTE_ENTRY(*PageTable));
        PageTableIndex = (CurrentVirtual >> X64_PTE_SHIFT) & X64_PT_MASK;
        PageTable[PageTableIndex] = 0;
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
    ULONGLONG EndAddress;
    ULONG EntryShift;
    ULONG Level;
    ULONG NewAttributesMask;
    PPTE PageTable;
    UINTN PageTableIndex;

    NewAttributesMask = (NewAttributes >> MAP_FLAG_PROTECT_SHIFT) &
                        MAP_FLAG_PROTECT_MASK;

    CurrentVirtual = (UINTN)VirtualAddress;
    EndAddress = CurrentVirtual + Size;
    while (CurrentVirtual < EndAddress) {

        //
        // Get down to the lowest level page table.
        //

        PageTable = BoPageDirectory;
        EntryShift = X64_PML4E_SHIFT;
        for (Level = 0; Level < X64_PAGE_LEVEL - 1; Level += 1) {
            PageTableIndex = (CurrentVirtual >> EntryShift) & X64_PT_MASK;
            PageTable += PageTableIndex;
            EntryShift -= X64_PTE_BITS;
            if ((*PageTable & X86_PTE_PRESENT) == 0) {
                PageTable = NULL;
                break;
            }

            //
            // Also stop if a huge page was found. Consider adding some code
            // to break apart a huge page if only part of it has attributes
            // being modified.
            //

            if ((*PageTable & X86_PTE_LARGE) != 0) {

                ASSERT(Level == X64_PAGE_LEVEL - 2);

                if (((EndAddress - CurrentVirtual) < _2MB) ||
                    ((CurrentVirtual & (_2MB - 1)) != 0)) {

                    RtlDebugPrint("Skipping modification of huge page at "
                                  "0x%llx because modification is only "
                                  "0x%llx bytes.\n",
                                  CurrentVirtual,
                                  EndAddress - CurrentVirtual);

                    PageTable = NULL;
                }

                break;
            }

            PageTable = (PPTE)(X86_PTE_ENTRY(*PageTable));
        }

        if (PageTable == NULL) {
            CurrentVirtual += PAGE_SIZE;
            continue;
        }

        PageTableIndex = (CurrentVirtual >> X64_PTE_SHIFT) & X64_PT_MASK;
        PageTable += PageTableIndex;

        //
        // Look up the entry in the page table.
        //

        ASSERT((*PageTable & X86_PTE_PRESENT) != 0);

        //
        // Set the various attributes and set the entry.
        //

        if ((NewAttributesMask & MAP_FLAG_READ_ONLY) != 0) {
            *PageTable &= ~X86_PTE_WRITABLE;
            if ((NewAttributes & MAP_FLAG_READ_ONLY) == 0) {
                *PageTable |= X86_PTE_WRITABLE;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_USER_MODE) != 0) {
            *PageTable &= ~X86_PTE_USER_MODE;
            if ((NewAttributes & MAP_FLAG_USER_MODE) != 0) {
                *PageTable |= X86_PTE_USER_MODE;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_WRITE_THROUGH) != 0) {
            *PageTable &= ~X86_PTE_WRITE_THROUGH;
            if ((NewAttributes & MAP_FLAG_WRITE_THROUGH) != 0) {
                *PageTable |= X86_PTE_WRITE_THROUGH;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_CACHE_DISABLE) != 0) {
            *PageTable &= ~X86_PTE_CACHE_DISABLED;
            if ((NewAttributes & MAP_FLAG_CACHE_DISABLE) != 0) {
                *PageTable |= X86_PTE_CACHE_DISABLED;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_GLOBAL) != 0) {
            *PageTable &= ~X86_PTE_GLOBAL;
            if ((NewAttributes & MAP_FLAG_GLOBAL) != 0) {
                *PageTable |= X86_PTE_GLOBAL;
            }
        }

        if ((NewAttributesMask & MAP_FLAG_EXECUTE) != 0) {
            *PageTable &= ~X86_PTE_NX;
            if ((NewAttributes & MAP_FLAG_EXECUTE) == 0) {
                *PageTable |= X86_PTE_NX;
            }
        }

        CurrentVirtual += PAGE_SIZE;
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

    KSTATUS Status;

    //
    // Map the kernel PML4 to a separate location since it needs to be visible
    // for syncing with other PML4s.
    //

    *PageDirectoryVirtual = (PVOID)-1;
    Status = BoMapPhysicalAddress(PageDirectoryVirtual,
                                  PageDirectoryPhysical,
                                  PAGE_SIZE,
                                  0,
                                  MemoryTypePageTables);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // The self map location is hardcoded and already set up, so these aren't
    // needed.
    //

    *PageTablesVirtual = NULL;
    return STATUS_SUCCESS;
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

    ULONGLONG Address;
    KSTATUS Status;
    PPTE Table;
    ULONG TableIndex;

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
    // Manually unmap the page. Don't use the unmap routine because that frees
    // the region in the MDL, which isn't cool.
    //

    Address = (UINTN)*PageTableStage;
    Table = BoPageDirectory;
    TableIndex = (Address >> X64_PML4E_SHIFT) & X64_PT_MASK;
    Table = (PPTE)(X86_PTE_ENTRY(Table[TableIndex]));
    TableIndex = (Address >> X64_PDPE_SHIFT) & X64_PT_MASK;
    Table = (PPTE)(X86_PTE_ENTRY(Table[TableIndex]));
    TableIndex = (Address >> X64_PDE_SHIFT) & X64_PT_MASK;
    Table = (PPTE)(X86_PTE_ENTRY(Table[TableIndex]));
    TableIndex = (Address >> X64_PTE_SHIFT) & X64_PT_MASK;
    Table[TableIndex] = 0;
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

