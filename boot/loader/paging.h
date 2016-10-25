/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    paging.h

Abstract:

    This header contains definitions for enabling paging in the boot loader.

Author:

    Evan Green 30-Jul-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// --------------------------------------------------------------------- Macros
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
BoInitializePagingStructures (
    PPHYSICAL_ADDRESS PageDirectory
    );

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

KSTATUS
BoMapPhysicalAddress (
    PVOID *VirtualAddress,
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONG Size,
    ULONG Attributes,
    MEMORY_TYPE MemoryType
    );

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

KSTATUS
BoUnmapPhysicalAddress (
    PVOID VirtualAddress,
    ULONG PageCount
    );

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

VOID
BoChangeMappingAttributes (
    PVOID VirtualAddress,
    UINTN Size,
    ULONG NewAttributes
    );

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

KSTATUS
BoMapPagingStructures (
    PHYSICAL_ADDRESS PageDirectoryPhysical,
    PVOID *PageDirectoryVirtual,
    PVOID *PageTablesVirtual
    );

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

KSTATUS
BoCreatePageTableStage (
    PHYSICAL_ADDRESS PageDirectoryPhysical,
    PVOID *PageTableStage
    );

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

VOID
BoEnablePaging (
    VOID
    );

/*++

Routine Description:

    This routine turns paging on in the processor.

Arguments:

    None.

Return Value:

    None.

--*/

