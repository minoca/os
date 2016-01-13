/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    apinit.c

Abstract:

    This module implements support for application processor initialization.

Author:

    Evan Green 31-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/arm.h>
#include "../intrupt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of pages needed for the startup trampoline.
//

#define TRAMPOLINE_PAGE_COUNT 1

#define ARM_PARKED_PROCESSOR_ID_OFFSET 0
#define ARM_PARKED_PROCESSOR_JUMP_ADDRESS_OFFSET 8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Extern the address of the trampoline code, the variables it uses, and its
// end.
//

extern PVOID HlpTrampolineCode;
extern PVOID HlTrampolineTtbr0;
extern PVOID HlTrampolineSystemControlRegister;
extern PVOID HlpTrampolineCodeEnd;

//
// -------------------------------------------------------------------- Globals
//

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
    ULONG PageSize;
    KSTATUS Status;
    ULONG SystemControlOffset;
    PULONG SystemControlPointer;
    PVOID TrampolineCode;
    ULONG TrampolineCodeSize;

    PageSize = MmPageSize();
    TrampolineCode = NULL;

    //
    // Allocate and identity map space for the trampoline code.
    //

    MmIdentityMapStartupStub(TRAMPOLINE_PAGE_COUNT,
                             &TrampolineCode,
                             &CurrentPageDirectory);

    TrampolineCodeSize = (UINTN)(&HlpTrampolineCodeEnd) -
                         (UINTN)(&HlpTrampolineCode);

    ASSERT(TrampolineCodeSize < PageSize);

    //
    // Determine the offsets from the start of the trampoline code to write
    // the page directory, stack pointer, and system control register.
    //

    PageDirectoryOffset = (UINTN)(&HlTrampolineTtbr0) -
                          (UINTN)(&HlpTrampolineCode);

    PageDirectoryPointer = (PVOID *)((PUCHAR)TrampolineCode +
                                     PageDirectoryOffset);

    SystemControlOffset = (UINTN)(&HlTrampolineSystemControlRegister) -
                          (UINTN)(&HlpTrampolineCode);

    SystemControlPointer =
                       (PULONG)((PUCHAR)TrampolineCode + SystemControlOffset);

    //
    // Copy the trampoline code to the allocation.
    //

    RtlCopyMemory(TrampolineCode, &HlpTrampolineCode, TrampolineCodeSize);

    //
    // Set up the page directory parameters.
    //

    *PageDirectoryPointer = CurrentPageDirectory;

    //
    // Set up the system control register pointer with the current value from
    // this processor.
    //

    *SystemControlPointer = ArGetSystemControlRegister();

    //
    // Return the needed information.
    //

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

    PULONGLONG JumpAddressRegister;
    PULONG ProcessorIdRegister;

    ASSERT(ParkedAddressMapping != NULL);

    //
    // Assert that this thread isn't wandering around processors while this
    // cache flush is happening.
    //

    ASSERT((KeGetRunLevel() >= RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    //
    // Clean the cache so that everything is current in memory before the
    // processor sees data that can unpark it.
    //

    ArSerializeExecution();
    ArCleanEntireCache();
    HlFlushCache(HL_CACHE_FLAG_CLEAN);

    //
    // Write the jump address first, then the processor number.
    //

    JumpAddressRegister = ParkedAddressMapping +
                          ARM_PARKED_PROCESSOR_JUMP_ADDRESS_OFFSET;

    *JumpAddressRegister = PhysicalJumpAddress;
    RtlMemoryBarrier();
    ProcessorIdRegister = ParkedAddressMapping + ARM_PARKED_PROCESSOR_ID_OFFSET;
    *ProcessorIdRegister = ProcessorPhysicalIdentifier;
    RtlMemoryBarrier();
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

