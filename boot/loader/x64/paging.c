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

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
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

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
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

    ASSERT(FALSE);

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

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
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

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

//
// --------------------------------------------------------- Internal Functions
//

