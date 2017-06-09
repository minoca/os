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

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the PML4 table address.
//

PPTE FwPml4Table;

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

    ULONG Count;
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    BOOL HugePages;
    ULONG Index;
    ULONGLONG Page;
    ULONG PageCount;
    KSTATUS Status;
    PPTE Table;

    //
    // Allocate and initialize a PML4, then like lazy slobs just identity
    // map the first 8GB and call it a day. The PCAT memory allocation has a
    // governer in there as well to avoid accidentally allocating pages greater
    // than that.
    //

    if (FwPml4Table == INVALID_PHYSICAL_ADDRESS) {

        //
        // First find out if the processor supports 1GB pages. This leaf must
        // be supported because it's the one that determined the machine was
        // long-mode capable.
        //

        Eax = X86_CPUID_EXTENDED_INFORMATION;
        ArCpuid(&Eax, &Ebx, &Ecx, &Edx);

        //
        // With 1GB pages, just a PML4T and PDPT are needed (4 entries).
        //

        if ((Edx & X86_CPUID_EXTENDED_INFORMATION_EDX_1GB_PAGES) != 0) {
            HugePages = TRUE;
            PageCount = 2;

        //
        // With only 2MB pages, 4 PDTs are needed too.
        //

        } else {
            HugePages = FALSE;
            PageCount = 2 + 4;
        }

        Status = FwAllocatePages(&Page,
                                 PageCount * PAGE_SIZE,
                                 PAGE_SIZE,
                                 MemoryTypeLoaderTemporary);

        if (!KSUCCESS(Status)) {
            goto FwCreatePageTablesEnd;
        }

        ASSERT(Page == (UINTN)Page);

        FwPml4Table = (PVOID)(UINTN)Page;
        Page += PAGE_SIZE;

        //
        // Zero out the PML4T and the PDPT. Point the PML4T at the PDPT.
        //

        RtlZeroMemory(FwPml4Table, PAGE_SIZE * 2);
        FwPml4Table[0] = Page | X86_PTE_PRESENT | X86_PTE_WRITABLE;

        //
        // If 1GB pages are supported, just fill in the four entries for the
        // PDPT.
        //

        Table = (PPTE)(UINTN)Page;
        Page += PAGE_SIZE;
        if (HugePages != FALSE) {
            Table[0] = X86_PTE_PRESENT | X86_PTE_WRITABLE | X86_PTE_LARGE;
            Table[1] = (2ULL * _1GB) |
                       X86_PTE_PRESENT | X86_PTE_WRITABLE | X86_PTE_LARGE;

            Table[2] = (4ULL * _1GB) |
                       X86_PTE_PRESENT | X86_PTE_WRITABLE | X86_PTE_LARGE;

            Table[3] = (6ULL * _1GB) |
                       X86_PTE_PRESENT | X86_PTE_WRITABLE | X86_PTE_LARGE;

        //
        // 1GB pages are not supported, so map 2MB pages.
        //

        } else {

            //
            // Fill in the PDPT.
            //

            for (Index = 0; Index < 4; Index += 1) {
                Table[Index] = Page | X86_PTE_PRESENT | X86_PTE_WRITABLE;
                Page += PAGE_SIZE;
            }

            //
            // Now fill in all the little 2MB pages up to 8GB. 4096 entries in
            // all. The PDTs are contiguous so this loop spans all 4 of them.
            //

            Table += X64_PTE_COUNT;
            Count = (8LL * _1GB) / (2LL * _1MB);
            Page = 0;
            for (Index = 0; Index < Count; Index += 1) {
                Table[Index] =
                     Page | X86_PTE_PRESENT | X86_PTE_WRITABLE | X86_PTE_LARGE;

                Page += 2 * _1MB;
            }
        }
    }

    Parameters->PageDirectory = (UINTN)FwPml4Table;
    Status = STATUS_SUCCESS;

FwCreatePageTablesEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

