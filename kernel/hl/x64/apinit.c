/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    apinit.c

Abstract:

    This module implements support for application processor initialization.

Author:

    Evan Green 11-Jun-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include <minoca/kernel/x64.h>
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
// Define the trampoline code labels.
//

extern CHAR HlpTrampolineCode;
extern CHAR HlKernelStart;
extern CHAR HlpTrampolineCodeEnd;

//
// Define the trampoline code variable pointers.
//

extern PVOID *HlTrampolineCr3;
extern PVOID *HlKernelStartPointer;

//
// Store a pointer to the virtual address (and physical address) of the
// identity mapped region used to bootstrap initializing and resuming
// processors.
//

PVOID HlIdentityStub = (PVOID)-1;

//
// Store a pointer to the processor context for the processor currently
// starting up. Having a single global pointer like this means x86 must
// serialize bringing processors online. Fortunately this is natural for x86,
// since on both boot and resume the BSP comes up and then it brings the APs up.
//

PPROCESSOR_CONTEXT HlProcessorStartContext;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
UINTN
HlDisableMmu (
    PHL_PHYSICAL_CALLBACK PhysicalFunction,
    UINTN Argument
    )

/*++

Routine Description:

    This routine temporarily disables the MMU and calls then given callback
    function.

Arguments:

    PhysicalFunction - Supplies the physical address of a function to call
        with the MMU disabled. Interrupts will also be disabled during this
        call.

    Argument - Supplies an argument to pass to the function.

Return Value:

    Returns the value returned by the callback function.

--*/

{

    //
    // So far this function is not needed on x86. If implemented, the temporary
    // GDT and IDT would need to be loaded in the assembly version to prevent
    // NMIs from triple faulting.
    //

    ASSERT(FALSE);

    return 0;
}

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
    PVOID *KernelStartPointer;
    ULONG Offset;
    PVOID *PageDirectoryPointer;
    KSTATUS Status;
    PVOID TrampolineCode;
    ULONG TrampolineCodeSize;

    if (HlIdentityStub != (PVOID)-1) {
        return STATUS_SUCCESS;
    }

    TrampolineCode = NULL;

    //
    // Allocate and identity map one page of physical memory for the trampoline
    // code.
    //

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

    Offset = (UINTN)(&HlTrampolineCr3) - (UINTN)(&HlpTrampolineCode);
    PageDirectoryPointer = (PVOID *)((PUCHAR)TrampolineCode + Offset);
    Offset = (UINTN)(&HlKernelStartPointer) - (UINTN)(&HlpTrampolineCode);
    KernelStartPointer = (PVOID *)((PUCHAR)TrampolineCode + Offset);

    //
    // Copy the trampoline code to the allocation.
    //

    RtlCopyMemory(TrampolineCode, &HlpTrampolineCode, TrampolineCodeSize);

    //
    // Set up the page directory parameter.
    //

    *PageDirectoryPointer = CurrentPageDirectory;
    *KernelStartPointer = &HlKernelStart;
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
    HlIdentityStub = NULL;
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

    PPROCESSOR_CONTEXT ProcessorContext;
    PVOID *StackPointer;

    //
    // If there is no start block, this is just P0 initializing its page.
    //

    if (StartBlock == NULL) {
        return STATUS_SUCCESS;
    }

    //
    // Save the current processor context. Processors will not restore to here
    // however. Use the edge of the stack as a region to store the context.
    //

    ProcessorContext = StartBlock->StackBase;
    ArSaveProcessorContext(ProcessorContext);

    //
    // Set a dummy return address and the start block as the one argument.
    //

    StackPointer = StartBlock->StackBase + StartBlock->StackSize;
    StackPointer -= 2;
    StackPointer[0] = NULL;
    StackPointer[1] = StartBlock;
    StartBlock->StackPointer = StackPointer;

    //
    // Now modify the processor context to "restore" to the initialization
    // routine.
    //

    ProcessorContext->Rsp = (UINTN)StackPointer;
    ProcessorContext->Rip = (UINTN)StartRoutine;
    ProcessorContext->Rbp = 0;
    ProcessorContext->Rbx = 0;
    ProcessorContext->Rax = 0;
    ProcessorContext->R12 = 0;
    ProcessorContext->R13 = 0;
    ProcessorContext->R14 = 0;
    ProcessorContext->R15 = 0;
    ProcessorContext->Fsbase = 0;
    ProcessorContext->Gsbase = 0;
    HlProcessorStartContext = ProcessorContext;
    *PhysicalStart = (UINTN)HlIdentityStub;
    return STATUS_SUCCESS;
}

KSTATUS
HlpInterruptPrepareForProcessorResume (
    ULONG ProcessorIndex,
    PPROCESSOR_CONTEXT *ProcessorContextPointer,
    PPHYSICAL_ADDRESS ResumeAddress,
    BOOL Abort
    )

/*++

Routine Description:

    This routine performs any per-processor preparations necessary to resume
    the given processor from a context-destructive state.

Arguments:

    ProcessorIndex - Supplies the processor index to save context for.

    ProcessorContextPointer - Supplies a pointer where a pointer to the
        processor's resume context should be saved. This routine cannot do the
        saving since once the context is saved the routine is not allowed to
        return until it's restored.

    ResumeAddress - Supplies a pointer where the physical address of the
        resume code for this processor will be returned.

    Abort - Supplies a boolean that if set undoes the effects of this function.

Return Value:

    Status code.

--*/

{

    //
    // At some point, consider implementing resume.
    //

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

//
// --------------------------------------------------------- Internal Functions
//

