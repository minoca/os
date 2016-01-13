/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    apinit.c

Abstract:

    This module implements support for application processor initialization.

Author:

    Evan Green 21-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/x86.h>
#include "../intrupt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of pages needed for the startup trampoline.
//

#define TRAMPOLINE_PAGE_COUNT 1

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
// Define the location of the trampoline code.
//

extern CHAR HlpTrampolineCode;
extern CHAR HlpTrampolineCodeEnd;
extern PVOID *HlTrampolineCr3;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpInterruptPrepareStartupStub (
    PPHYSICAL_ADDRESS JumpAddressPhysical,
    PVOID *JumpAddressVirtual,
    PULONG PagesAllocated
    )

/*++

Routine Description:

    This routine prepares the startup stub trampoline, used to bootstrap
    embryonic processors into the kernel.

Arguments:

    JumpAddressPhysical - Supplies a pointer that will receive the physical
        address the new processor(s) should jump to.

    JumpAddressVirtual - Supplies a pointer that will receive the virtual
        address the new processor(s) should jump to.

    PagesAllocated - Supplies a pointer that will receive the number of pages
        needed to create the startup stub.

Return Value:

    Status code.

--*/

{

    PVOID CurrentPageDirectory;
    ULONG PageDirectoryOffset;
    PVOID *PageDirectoryPointer;
    KSTATUS Status;
    PVOID TrampolineCode;
    ULONG TrampolineCodeSize;

    TrampolineCode = NULL;

    //
    // Allocate and identity map two pages of physical memory, one for the code
    // and one for the loaner stack (and processor block). Assert if the
    // processor block ever gets too big so that someone knows to clean this
    // up.
    //

    ASSERT(sizeof(PROCESSOR_BLOCK) <= 0x800);

    MmIdentityMapStartupStub(TRAMPOLINE_PAGE_COUNT,
                             &TrampolineCode,
                             &CurrentPageDirectory);

    TrampolineCodeSize = (UINTN)(&HlpTrampolineCodeEnd) -
                         (UINTN)(&HlpTrampolineCode);

    ASSERT(TrampolineCodeSize < (TRAMPOLINE_PAGE_COUNT * MmPageSize()));

    //
    // Determine the offsets from the start of the trampoline code to write
    // the page directory.
    //

    PageDirectoryOffset = (UINTN)(&HlTrampolineCr3) -
                          (UINTN)(&HlpTrampolineCode);

    PageDirectoryPointer = (PVOID *)((PUCHAR)TrampolineCode +
                                     PageDirectoryOffset);

    //
    // Copy the trampoline code to the allocation.
    //

    RtlCopyMemory(TrampolineCode, &HlpTrampolineCode, TrampolineCodeSize);

    //
    // Set up the page directory parameter.
    //

    *PageDirectoryPointer = CurrentPageDirectory;
    *JumpAddressPhysical = (PHYSICAL_ADDRESS)(UINTN)TrampolineCode;
    *JumpAddressVirtual = TrampolineCode;
    *PagesAllocated = TRAMPOLINE_PAGE_COUNT;
    Status = STATUS_SUCCESS;
    return Status;
}

VOID
HlpInterruptDestroyStartupStub (
    PHYSICAL_ADDRESS JumpAddressPhysical,
    PVOID JumpAddressVirtual,
    ULONG PageCount
    )

/*++

Routine Description:

    This routine destroys the startup stub trampoline, freeing all allocated
    resources.

Arguments:

    JumpAddressPhysical - Supplies the physical address of the startup stub.

    JumpAddressVirtual - Supplies the virtual address of the startup stub.

    PageCount - Supplies the number of pages in the startup stub.

Return Value:

    None.

--*/

{

    MmUnmapStartupStub(JumpAddressVirtual, PageCount);
    return;
}

KSTATUS
HlpInterruptPrepareForProcessorStart (
    ULONG ProcessorPhysicalIdentifier,
    PVOID ParkedAddressMapping,
    PHYSICAL_ADDRESS PhysicalJumpAddress,
    PVOID VirtualJumpAddress
    )

/*++

Routine Description:

    This routine performs any per-processor preparations necessary to start the
    given processor.

Arguments:

    ProcessorPhysicalIdentifier - Supplies the physical ID of the processor that
        is about to be started.

    ParkedAddressMapping - Supplies a pointer to the mapping to the processor's
        parked physical address.

    PhysicalJumpAddress - Supplies the physical address of the boot code this
        processor should jump to.

    VirtualJumpAddress - Supplies the virtual address of the boot code this
        processor should jump to.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

