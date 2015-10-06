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
#include <minoca/bootload.h>
#include <minoca/arm.h>
#include "../intrupt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of pages needed for the startup trampoline.
//

#define TRAMPOLINE_PAGE_COUNT 1

//
// Define the size of the OS code region of the processor parked page. The OS
// has the first half of the page (2k, minus the MP protocol defined regions,
// and minus the saved processor context).
//

#define ARM_PARKED_PAGE_OS_CODE_SIZE 0x100

//
// Define the total size of the OS portion of the parked page.
//

#define ARM_PARKED_PAGE_OS_SIZE 0x800

/*++

Structure Description:

    This structure describes the format of the page that ARM secondary
    processors are parked on. Some of this is defined with the firmware, and
    some of it is OS-specific. Many if not all of these structure members are
    accessed directly by assembly code.

Members:

    ProcessorId - Stores the processor ID of the processor. This is defined by
        the MP parking protocol, which uses this as an identifier to indicate
        if the jump address is valid.

    Reserved - Stores a reserved byte, for alignment and possible expansion of
        the processor ID field.

    JumpAddress - Stores the physical address to jump to when the processor is
        coming out of the parking protocol. This is defined by the MP parking
        protocol.

    IdentityPagePhysical - Stores the physical address of the identity mapped
        page to jump to. This is OS-specific, and is used directly by assembly
        code.

    ContextVirtual - Stores the virtual address of the processor context
        structure below.

    ProcessorContext - Stores the processor context saved for this processor
        when it was going down. This is OS-specific. This is OS-specific, and
        is used directly by assembly code.

    OsCode - Stores the OS region for bootstrap code. This is OS-specific.

--*/

typedef struct _ARM_PARKED_PAGE {
    ULONG ProcessorId;
    ULONG Reserved;
    ULONGLONG JumpAddress;
    ULONG IdentityPagePhysical;
    PVOID ContextVirtual;
    PROCESSOR_CONTEXT ProcessorContext;
    UCHAR OsCode[ARM_PARKED_PAGE_OS_CODE_SIZE];
} ARM_PARKED_PAGE, *PARM_PARKED_PAGE;

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

extern PVOID HlpProcessorStartup;
extern PVOID HlpProcessorStartupEnd;
extern PVOID HlpTrampolineCode;
extern PVOID HlTrampolineTtbr0;
extern PVOID HlTrampolineSystemControlRegister;
extern PVOID HlpTrampolineCodeEnd;

VOID
HlpInterruptArmReleaseParkedProcessor (
    PARM_PARKED_PAGE ParkedPage,
    ULONG PhysicalJumpAddress,
    ULONG ProcessorIdentifier
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the virtual address (and physical address) of the
// identity mapped region used to bootstrap initializing and resuming
// processors.
//

PVOID HlIdentityStub = (PVOID)-1;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpInterruptPrepareIdentityStub (
    VOID
    )

/*++

Routine Description:

    This routine prepares the identity mapped trampoline, used to bootstrap
    initializing and resuming processors coming from physical mode.

Arguments:

    None.

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

    if (HlIdentityStub != (PVOID)-1) {
        return STATUS_SUCCESS;
    }

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

    HlIdentityStub = TrampolineCode;
    Status = STATUS_SUCCESS;
    return Status;
}

VOID
HlpInterruptDestroyIdentityStub (
    VOID
    )

/*++

Routine Description:

    This routine destroys the startup stub trampoline, freeing all allocated
    resources.

Arguments:

    None.

Return Value:

    None.

--*/

{

    MmUnmapStartupStub(HlIdentityStub, TRAMPOLINE_PAGE_COUNT);
    return;
}

KSTATUS
HlpInterruptPrepareForProcessorStart (
    ULONG ProcessorIndex,
    PPROCESSOR_START_BLOCK StartBlock,
    PPROCESSOR_START_ROUTINE StartRoutine,
    PPHYSICAL_ADDRESS PhysicalStart
    )

/*++

Routine Description:

    This routine performs any per-processor preparations necessary to start the
    given processor.

Arguments:

    ProcessorIndex - Supplies the index of the processor to start.

    StartBlock - Supplies a pointer to the processor start block.

    StartRoutine - Supplies a pointer to the routine to call on the new
        processor.

    PhysicalStart - Supplies a pointer where the physical address the processor
        should jump to upon initialization will be returned.

Return Value:

    Status code.

--*/

{

    UINTN CopyEnd;
    UINTN CopySize;
    UINTN CopyStart;
    PARM_PARKED_PAGE ParkedPage;
    ULONG ParkedPagePhysical;

    ParkedPage = HlProcessorTargets[ProcessorIndex].ParkedVirtualAddress;
    ParkedPagePhysical =
                      HlProcessorTargets[ProcessorIndex].ParkedPhysicalAddress;

    ASSERT(ParkedPage != NULL);

    //
    // Save the current processor context, although the secondary processor
    // will not restore back to here.
    //

    StartBlock->StackPointer = StartBlock->StackBase + StartBlock->StackSize;
    ArSaveProcessorContext(&(ParkedPage->ProcessorContext));
    ParkedPage->ProcessorContext.Sp = (UINTN)(StartBlock->StackPointer);
    ParkedPage->ProcessorContext.Pc = (UINTN)StartRoutine;
    ParkedPage->ProcessorContext.R0 = (UINTN)StartBlock;
    ParkedPage->ProcessorContext.R4 = ProcessorIndex;
    ParkedPage->ProcessorContext.R5 = 0xDEADBEEF;
    ParkedPage->ProcessorContext.R11 = 0;
    ParkedPage->ProcessorContext.Tpidrprw = 0;
    ParkedPage->ProcessorContext.Pmccntr = 0;
    *PhysicalStart = ParkedPagePhysical + FIELD_OFFSET(ARM_PARKED_PAGE, OsCode);

    //
    // Copy the small amount of code into the parked page.
    //

    CopyStart = (UINTN)&HlpProcessorStartup & ~ARM_THUMB_BIT;
    CopyEnd = (UINTN)&HlpProcessorStartupEnd & ~ARM_THUMB_BIT;
    CopySize = CopyEnd - CopyStart;

    ASSERT(CopySize <= ARM_PARKED_PAGE_OS_CODE_SIZE);
    ASSERT(sizeof(ARM_PARKED_PAGE) <= ARM_PARKED_PAGE_OS_SIZE);

    RtlCopyMemory(&(ParkedPage->OsCode), (PVOID)CopyStart, CopySize);
    ParkedPage->IdentityPagePhysical = (UINTN)HlIdentityStub;
    ParkedPage->ContextVirtual = &(ParkedPage->ProcessorContext);

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
    // Make the core jump to the spot of code within the page itself (which
    // then jumps to the identity mapped page for real initialization).
    //

    HlpInterruptArmReleaseParkedProcessor(
                                ParkedPage,
                                *PhysicalStart,
                                HlProcessorTargets[ProcessorIndex].PhysicalId);

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
HlpInterruptArmReleaseParkedProcessor (
    PARM_PARKED_PAGE ParkedPage,
    ULONG PhysicalJumpAddress,
    ULONG ProcessorIdentifier
    )

/*++

Routine Description:

    This routine performs the ARM parking protocol ceremony to release a
    parked processor.

Arguments:

    ParkedPage - Supplies the virtual address of the parked page mapping for
        the desired processor.

    PhysicalJumpAddress - Supplies the 32-bit physical address to jump to.

    ProcessorIdentifier - Supplies the processor identifier of the processor
        to boot.

Return Value:

    None.

--*/

{

    //
    // Write the jump address first, then the processor number.
    //

    ParkedPage->JumpAddress = PhysicalJumpAddress;
    ArSerializeExecution();
    ParkedPage->ProcessorId = ProcessorIdentifier;
    RtlMemoryBarrier();
    return;
}

