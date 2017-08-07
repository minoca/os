/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    arch.h

Abstract:

    This header contains definitions for architecture dependent but universally
    required functionality.

Author:

    Evan Green 10-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define ARCH_POOL_TAG 0x68637241 // 'hcrA'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _TRAP_FRAME TRAP_FRAME, *PTRAP_FRAME;
typedef struct _PROCESSOR_CONTEXT PROCESSOR_CONTEXT, *PPROCESSOR_CONTEXT;
typedef struct _FPU_CONTEXT FPU_CONTEXT, *PFPU_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
ArInitializeProcessor (
    BOOL PhysicalMode,
    PVOID ProcessorStructures
    );

/*++

Routine Description:

    This routine initializes processor-specific structures.

Arguments:

    PhysicalMode - Supplies a boolean indicating whether or not the processor
        is operating in physical mode.

    ProcessorStructures - Supplies a pointer to the memory to use for basic
        processor structures, as returned by the allocate processor structures
        routine. For the boot processor, supply NULL here to use this routine's
        internal resources.

Return Value:

    None.

--*/

KSTATUS
ArFinishBootProcessorInitialization (
    VOID
    );

/*++

Routine Description:

    This routine performs additional initialization steps for processor 0 that
    were put off in pre-debugger initialization.

Arguments:

    None.

Return Value:

    Status code.

--*/

PVOID
ArAllocateProcessorStructures (
    ULONG ProcessorNumber
    );

/*++

Routine Description:

    This routine attempts to allocate and initialize early structures needed by
    a new processor.

Arguments:

    ProcessorNumber - Supplies the number of the processor that these resources
        will go to.

Return Value:

    Returns a pointer to the new processor resources on success.

    NULL on failure.

--*/

VOID
ArFreeProcessorStructures (
    PVOID ProcessorStructures
    );

/*++

Routine Description:

    This routine destroys a set of processor structures that have been
    allocated. It should go without saying, but obviously a processor must not
    be actively using these resources.

Arguments:

    ProcessorStructures - Supplies the pointer returned by the allocation
        routine.

Return Value:

    None.

--*/

BOOL
ArIsTranslationEnabled (
    VOID
    );

/*++

Routine Description:

    This routine determines if the processor was initialized with virtual-to-
    physical address translation enabled or not.

Arguments:

    None.

Return Value:

    TRUE if the processor is using a layer of translation between CPU accessible
    addresses and physical memory.

    FALSE if the processor was initialized in physical mode.

--*/

ULONG
ArGetIoPortCount (
    VOID
    );

/*++

Routine Description:

    This routine returns the number of I/O port addresses architecturally
    available.

Arguments:

    None.

Return Value:

    Returns the number of I/O port address supported by the architecture.

--*/

ULONG
ArGetInterruptVectorCount (
    VOID
    );

/*++

Routine Description:

    This routine returns the number of interrupt vectors in the system, either
    architecturally defined or artificially created.

Arguments:

    None.

Return Value:

    Returns the number of interrupt vectors in use by the system.

--*/

ULONG
ArGetMinimumDeviceVector (
    VOID
    );

/*++

Routine Description:

    This routine returns the first interrupt vector that can be used by
    devices.

Arguments:

    None.

Return Value:

    Returns the minimum interrupt vector available for use by devices.

--*/

ULONG
ArGetMaximumDeviceVector (
    VOID
    );

/*++

Routine Description:

    This routine returns the last interrupt vector that can be used by
    devices.

Arguments:

    None.

Return Value:

    Returns the maximum interrupt vector available for use by devices.

--*/

ULONG
ArGetTrapFrameSize (
    VOID
    );

/*++

Routine Description:

    This routine returns the size of the trap frame structure, in bytes.

Arguments:

    None.

Return Value:

    Returns the size of the trap frame structure, in bytes.

--*/

PVOID
ArGetInstructionPointer (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine returns the instruction pointer out of the trap frame.

Arguments:

    TrapFrame - Supplies the trap frame from which the instruction pointer
        will be returned.

Return Value:

    Returns the instruction pointer the trap frame is pointing to.

--*/

BOOL
ArIsTrapFrameFromPrivilegedMode (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine determines if the given trap frame occurred in a privileged
    environment or not.

Arguments:

    TrapFrame - Supplies the trap frame.

Return Value:

    TRUE if the execution environment of the trap frame is privileged.

    FALSE if the execution environment of the trap frame is not privileged.

--*/

BOOL
ArIsTrapFrameComplete (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine determines if the given trap frame contains the full context
    or only partial context as saved by the system call handler.

Arguments:

    TrapFrame - Supplies the trap frame.

Return Value:

    TRUE if the trap frame has all registers filled out.

    FALSE if the the trap frame is largely uninitialized as left by the system
    call handler.

--*/

KERNEL_API
BOOL
ArAreInterruptsEnabled (
    VOID
    );

/*++

Routine Description:

    This routine determines whether or not interrupts are currently enabled
    on the processor.

Arguments:

    None.

Return Value:

    TRUE if interrupts are enabled in the processor.

    FALSE if interrupts are globally disabled.

--*/

KERNEL_API
BOOL
ArDisableInterrupts (
    VOID
    );

/*++

Routine Description:

    This routine disables all interrupts on the current processor.

Arguments:

    None.

Return Value:

    TRUE if interrupts were previously enabled.

    FALSE if interrupts were not previously enabled.

--*/

KERNEL_API
VOID
ArEnableInterrupts (
    VOID
    );

/*++

Routine Description:

    This routine enables interrupts on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/

ULONG
ArGetProcessorFlags (
    VOID
    );

/*++

Routine Description:

    This routine gets the current processor's flags register.

Arguments:

    None.

Return Value:

    Returns the current flags.

--*/

VOID
ArCleanEntireCache (
    VOID
    );

/*++

Routine Description:

    This routine cleans the entire data cache.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArInvalidateTlbEntry (
    volatile VOID *Address
    );

/*++

Routine Description:

    This routine invalidates one TLB entry corresponding to the given virtual
    address.

Arguments:

    Address - Supplies the virtual address whose associated TLB entry will be
        invalidated.

Return Value:

    None.

--*/

VOID
ArInvalidateEntireTlb (
    VOID
    );

/*++

Routine Description:

    This routine invalidates the entire TLB.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArProcessorYield (
    VOID
    );

/*++

Routine Description:

    This routine executes a short processor yield in hardware.

Arguments:

    None.

Return Value:

    None.

--*/

KERNEL_API
VOID
ArWaitForInterrupt (
    VOID
    );

/*++

Routine Description:

    This routine halts the processor until the next interrupt comes in. This
    routine should be called with interrupts disabled, and will return with
    interrupts enabled.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArSerializeExecution (
    VOID
    );

/*++

Routine Description:

    This routine acts a serializing instruction, preventing the processor
    from speculatively executing beyond this point.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArInvalidateInstructionCache (
    VOID
    );

/*++

Routine Description:

    This routine invalidate the processor's instruction only cache, indicating
    that a page containing code has changed.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArSetUpUserSharedDataFeatures (
    VOID
    );

/*++

Routine Description:

    This routine initialize the user shared data processor specific features.

Arguments:

    None.

Return Value:

    None.

--*/

PFPU_CONTEXT
ArAllocateFpuContext (
    ULONG AllocationTag
    );

/*++

Routine Description:

    This routine allocates a buffer that can be used for FPU context.

Arguments:

    AllocationTag - Supplies the pool allocation tag to use for the allocation.

Return Value:

    Returns a pointer to the newly allocated FPU context on success.

    NULL on allocation failure.

--*/

VOID
ArDestroyFpuContext (
    PFPU_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys a previously allocated FPU context buffer.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

VOID
ArSetThreadPointer (
    PVOID Thread,
    PVOID NewThreadPointer
    );

/*++

Routine Description:

    This routine sets the new thread pointer value.

Arguments:

    Thread - Supplies a pointer to the thread to set the thread pointer for.

    NewThreadPointer - Supplies the new thread pointer value to set.

Return Value:

    None.

--*/

UINTN
ArSaveProcessorContext (
    PPROCESSOR_CONTEXT Context
    );

/*++

Routine Description:

    This routine saves the current processor context, including the
    non-volatile general registers and the system level control registers. This
    function appears to return twice, once when the context is saved and then
    again when the context is restored. Because the stack pointer is restored,
    the caller of this function may not return without either abandoning the
    context or calling restore. Returning and then calling restore would almost
    certainly result in stack corruption.

Arguments:

    Context - Supplies a pointer to the context area to save into.

Return Value:

    Returns 0 after the context was successfully saved (first time).

    Returns the value in the context return address register when the restore
    function is called (the second time). By default this value is 1, though it
    can be manipulated after the initial save is complete.

--*/

VOID
ArRestoreProcessorContext (
    PPROCESSOR_CONTEXT Context
    );

/*++

Routine Description:

    This routine restores the current processor context, including the
    non-volatile general registers and the system level control registers. This
    function does not return, but instead jumps to the return address from
    the caller of the save context function.

Arguments:

    Context - Supplies a pointer to the context to restore.

Return Value:

    Does not return, at least not conventionally.

--*/

