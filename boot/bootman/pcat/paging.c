/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    paging.c

Abstract:

    This module sets up PAE paging for a 64-bit OS loader.

Author:

    Evan Green 31-May-2017

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x64.h>
#include "firmware.h"
#include "bootlib.h"
#include "bios.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the iteration context for mapping all the boot
    manager allocations.

Members:

    PagesMapped - Stores the number of pages that were successfully mapped.

    Status - Stores the overall status code.

--*/

typedef struct _BOOTMAN_MAPPING_CONTEXT {
    UINTN PagesMapped;
    KSTATUS Status;
} BOOTMAN_MAPPING_CONTEXT, *PBOOTMAN_MAPPING_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
BmpFwBootMappingIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

KSTATUS
BmpFwIdentityMapPages (
    ULONGLONG Address,
    UINTN Size,
    PUINTN PagesMapped
    );

KSTATUS
BmpFwIdentityMapPage (
    ULONGLONG Address,
    PUINTN PagesMapped
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the PML4 table address.
//

PPTE FwPml4Table;

//
// Define the PML4 self map index.
//

ULONG FwSelfMapIndex;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BmpFwCreatePageTables (
    PBOOT_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine sets up the page tables used by a 64-bit boot application.

Arguments:

    Parameters - Supplies a pointer to the boot initialization block.

Return Value:

    Status code.

--*/

{

    BOOTMAN_MAPPING_CONTEXT Context;
    ULONGLONG Page;
    KSTATUS Status;

    Context.PagesMapped = 0;
    Context.Status = STATUS_SUCCESS;

    //
    // Allocate and initialize a PML4.
    //

    if (FwPml4Table == INVALID_PHYSICAL_ADDRESS) {
        Status = FwAllocatePages(&Page,
                                 PAGE_SIZE,
                                 PAGE_SIZE,
                                 MemoryTypeLoaderTemporary);

        if (!KSUCCESS(Status)) {
            goto FwCreatePageTablesEnd;
        }

        ASSERT(Page == (UINTN)Page);

        FwPml4Table = (PVOID)(UINTN)Page;
        RtlZeroMemory(FwPml4Table, PAGE_SIZE);

        //
        // Just use the highest value as the self map index. This conveniently
        // keeps it out of a range where the self map might collide with real
        // physical pages.
        //

        FwSelfMapIndex = X64_PTE_COUNT;
        FwPml4Table[FwSelfMapIndex] =
                              X86_ENTRY_PTE((UINTN)FwPml4Table >> PAGE_SHIFT) |
                              X86_PTE_PRESENT |
                              X86_PTE_WRITABLE;
    }

    MmMdIterate(&BoMemoryMap,
                BmpFwBootMappingIterationRoutine,
                &Context);

    if (!KSUCCESS(Context.Status)) {
        goto FwCreatePageTablesEnd;
    }

    Parameters->PageDirectory = (UINTN)FwPml4Table;
    Parameters->PageTables = ((ULONGLONG)FwSelfMapIndex << X64_PML4E_SHIFT) |
                             X64_CANONICAL_HIGH;

FwCreatePageTablesEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
BmpFwBootMappingIterationRoutine (
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

    PBOOTMAN_MAPPING_CONTEXT IterationContext;
    UINTN PagesMapped;
    KSTATUS Status;

    IterationContext = Context;
    if (!KSUCCESS(IterationContext->Status)) {
        return;
    }

    //
    // Skip all except interesting descriptors.
    //

    if ((Descriptor->Type == MemoryTypeFirmwareTemporary) ||
        (Descriptor->Type == MemoryTypeLoaderTemporary) ||
        (Descriptor->Type == MemoryTypeLoaderPermanent)) {

        PagesMapped = 0;
        Status = BmpFwIdentityMapPages(Descriptor->BaseAddress,
                                       Descriptor->Size,
                                       &PagesMapped);

        IterationContext->PagesMapped += PagesMapped;
        if (!KSUCCESS(Status)) {
            IterationContext->Status = Status;
        }
    }

    return;
}

KSTATUS
BmpFwIdentityMapPages (
    ULONGLONG Address,
    UINTN Size,
    PUINTN PagesMapped
    )

/*++

Routine Description:

    This routine identity maps a region of memory in preparation for switching
    64-bit paging on.

Arguments:

    Address - Supplies the address to identity map.

    Size - Supplies the size to map.

    PagesMapped - Supplies a pointer where the number of pages successfully
        mapped will be incremented. Pages already mapped do not count.

Return Value:

    Status code.

--*/

{

    ULONGLONG Current;
    UINTN Index;
    UINTN PageCount;
    KSTATUS Status;

    Current = ALIGN_RANGE_DOWN(Address, PAGE_SIZE);
    PageCount = (ALIGN_RANGE_UP(Address + Size, PAGE_SIZE) - Current) >>
                PAGE_SHIFT;

    for (Index = 0; Index < PageCount; Index += 1) {
        Status = BmpFwIdentityMapPage(Current, PagesMapped);
        if (!KSUCCESS(Status)) {
            return Status;
        }

        Current += PAGE_SIZE;
    }

    return STATUS_SUCCESS;
}

KSTATUS
BmpFwIdentityMapPage (
    ULONGLONG Address,
    PUINTN PagesMapped
    )

/*++

Routine Description:

    This routine identity maps a page of memory in preparation for switching
    64-bit paging on.

Arguments:

    Address - Supplies the address of the page to identity map.

    PagesMapped - Supplies a pointer whose value will be incremented if a new
        page was mapped.

Return Value:

    Status code.

--*/

{

    ULONG EntryIndex;
    ULONG Index;
    ULONGLONG NewPage;
    ULONG Shift;
    KSTATUS Status;
    PPTE Table;

    //
    // Walk the page tables, creating any needed pages along the way.
    //

    Table = FwPml4Table;
    Shift = X64_PML4E_SHIFT;
    for (Index = 0; Index < X64_PAGE_LEVEL - 1; Index += 1) {
        EntryIndex = (Address >> Shift) & X64_PT_MASK;
        if (X86_PTE_ENTRY(Table[EntryIndex]) == 0) {
            Status = FwAllocatePages(&NewPage,
                                     PAGE_SIZE,
                                     PAGE_SIZE,
                                     MemoryTypeLoaderTemporary);

            if (!KSUCCESS(Status)) {
                return Status;
            }

            ASSERT(NewPage == (UINTN)NewPage);

            RtlZeroMemory((PVOID)(UINTN)NewPage, PAGE_SIZE);
            Table[EntryIndex] = X86_ENTRY_PTE(NewPage >> PAGE_SHIFT) |
                                X86_PTE_PRESENT |
                                X86_PTE_WRITABLE;
        }

        ASSERT(X86_PTE_ENTRY(Table[EntryIndex]) ==
               (UINTN)(X86_PTE_ENTRY(Table[EntryIndex])));

        Table = (PPTE)(UINTN)(X86_PTE_ENTRY(Table[EntryIndex]) << PAGE_SHIFT);
        Shift -= X64_PTE_BITS;
    }

    ASSERT(Shift == PAGE_SHIFT);

    EntryIndex = (Address >> PAGE_SHIFT) & X64_PT_MASK;
    if ((Table[EntryIndex] & X86_PTE_PRESENT) != 0) {
        if (X86_PTE_ENTRY(Table[EntryIndex]) != (Address >> PAGE_SHIFT)) {

            //
            // Some page is already mapped here, and it's not the right one.
            //

            ASSERT(FALSE);

            return STATUS_MEMORY_CONFLICT;
        }

        return STATUS_SUCCESS;
    }

    //
    // Set the PTE to point at the page itself.
    //

    Table[EntryIndex] = X86_ENTRY_PTE(Address >> PAGE_SHIFT) |
                        X86_PTE_PRESENT |
                        X86_PTE_WRITABLE;

    *PagesMapped += 1;
    return STATUS_SUCCESS;
}

