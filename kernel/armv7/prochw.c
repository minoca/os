/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    prochw.c

Abstract:

    This module implements support functionality for hardware that is specific
    to the ARM architecture.

Author:

    Evan Green 11-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ArpInitializeInterrupts (
    BOOL PhysicalMode,
    BOOL BootProcessor,
    PVOID Idt
    );

VOID
ArpSetProcessorFeatures (
    PPROCESSOR_BLOCK ProcessorBlock
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store globals for the per-processor data structures used by P0.
//

PVOID ArP0InterruptTable[MAXIMUM_VECTOR - MINIMUM_VECTOR + 1] = {NULL};
PROCESSOR_BLOCK ArP0ProcessorBlock;
ULONG ArP0ExceptionStacks[EXCEPTION_STACK_COUNT * EXCEPTION_STACK_SIZE];

//
// Remember whether the processor was initialized with translation enabled or
// not.
//

BOOL ArTranslationEnabled = FALSE;

//
// Global containing a partially initialized interrupt table. This table will
// be copied to the real location, either 0 or 0xFFFF0000.
//

extern ARM_INTERRUPT_TABLE ArArmInterruptTable;

//
// ------------------------------------------------------------------ Functions
//

VOID
ArInitializeProcessor (
    BOOL PhysicalMode,
    PVOID ProcessorStructures
    )

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

{

    BOOL BootProcessor;
    PVOID ExceptionStacks;
    PVOID InterruptTable;
    PPROCESSOR_BLOCK ProcessorBlock;

    BootProcessor = TRUE;
    if (PhysicalMode == FALSE) {
        ArTranslationEnabled = TRUE;
    }

    ExceptionStacks = ArP0ExceptionStacks;
    InterruptTable = ArP0InterruptTable;
    if (PhysicalMode == FALSE) {

        //
        // Use the globals if this is the boot processor because the memory
        // subsystem is not yet online.
        //

        if (ProcessorStructures == NULL) {
            ProcessorBlock = &ArP0ProcessorBlock;

        } else {
            BootProcessor = FALSE;
            ProcessorBlock = (PPROCESSOR_BLOCK)ProcessorStructures;
            ExceptionStacks = (PVOID)(ProcessorBlock + 1);
        }

    //
    // The processor is executing in physical mode.
    //

    } else {
        ProcessorBlock = &ArP0ProcessorBlock;
    }

    //
    // Initialize the exception stacks.
    //

    ArpInitializeExceptionStacks(ExceptionStacks, EXCEPTION_STACK_SIZE);

    //
    // Initialize the pointer to the processor block.
    //

    ProcessorBlock->Self = ProcessorBlock;
    ProcessorBlock->InterruptTable = InterruptTable;
    ArSetProcessorBlockRegister(ProcessorBlock);
    ArpInitializeInterrupts(PhysicalMode, BootProcessor, NULL);
    ArpSetProcessorFeatures(ProcessorBlock);

    //
    // Initialize the performance monitor.
    //

    ArpInitializePerformanceMonitor();
    return;
}

KSTATUS
ArFinishBootProcessorInitialization (
    VOID
    )

/*++

Routine Description:

    This routine performs additional initialization steps for processor 0 that
    were put off in pre-debugger initialization.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

PVOID
ArAllocateProcessorStructures (
    ULONG ProcessorNumber
    )

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

{

    PVOID Allocation;
    ULONG AllocationSize;
    PPROCESSOR_BLOCK ProcessorBlock;

    AllocationSize = sizeof(PROCESSOR_BLOCK) +
                     (EXCEPTION_STACK_SIZE * EXCEPTION_STACK_COUNT);

    Allocation = MmAllocateNonPagedPool(AllocationSize, ARCH_POOL_TAG);
    if (Allocation == NULL) {
        return NULL;
    }

    RtlZeroMemory(Allocation, AllocationSize);
    ProcessorBlock = (PPROCESSOR_BLOCK)Allocation;
    ProcessorBlock->Self = ProcessorBlock;
    ProcessorBlock->ProcessorNumber = ProcessorNumber;
    return Allocation;
}

VOID
ArFreeProcessorStructures (
    PVOID ProcessorStructures
    )

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

{

    MmFreeNonPagedPool(ProcessorStructures);
    return;
}

BOOL
ArIsTranslationEnabled (
    VOID
    )

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

{

    return ArTranslationEnabled;
}

ULONG
ArGetIoPortCount (
    VOID
    )

/*++

Routine Description:

    This routine returns the number of I/O port addresses architecturally
    available.

Arguments:

    None.

Return Value:

    Returns the number of I/O port address supported by the architecture.

--*/

{

    return IO_PORT_COUNT;
}

ULONG
ArGetInterruptVectorCount (
    VOID
    )

/*++

Routine Description:

    This routine returns the number of interrupt vectors in the system, either
    architecturally defined or artificially created.

Arguments:

    None.

Return Value:

    Returns the number of interrupt vectors in use by the system.

--*/

{

    return INTERRUPT_VECTOR_COUNT;
}

ULONG
ArGetMinimumDeviceVector (
    VOID
    )

/*++

Routine Description:

    This routine returns the first interrupt vector that can be used by
    devices.

Arguments:

    None.

Return Value:

    Returns the minimum interrupt vector available for use by devices.

--*/

{

    return MINIMUM_VECTOR;
}

ULONG
ArGetMaximumDeviceVector (
    VOID
    )

/*++

Routine Description:

    This routine returns the last interrupt vector that can be used by
    devices.

Arguments:

    None.

Return Value:

    Returns the maximum interrupt vector available for use by devices.

--*/

{

    return MAXIMUM_DEVICE_VECTOR;
}

ULONG
ArGetTrapFrameSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of the trap frame structure, in bytes.

Arguments:

    None.

Return Value:

    Returns the size of the trap frame structure, in bytes.

--*/

{

    return sizeof(TRAP_FRAME);
}

PVOID
ArGetInstructionPointer (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine returns the instruction pointer out of the trap frame.

Arguments:

    TrapFrame - Supplies the trap frame from which the instruction pointer
        will be returned.

Return Value:

    Returns the instruction pointer the trap frame is pointing to.

--*/

{

    ULONG Pc;

    Pc = TrapFrame->Pc;
    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        Pc |= ARM_THUMB_BIT;
    }

    return (PVOID)Pc;
}

BOOL
ArIsTrapFrameFromPrivilegedMode (
    PTRAP_FRAME TrapFrame
    )

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

{

    return IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(TrapFrame);
}

BOOL
ArIsTrapFrameComplete (
    PTRAP_FRAME TrapFrame
    )

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

{

    return IS_TRAP_FRAME_COMPLETE(TrapFrame);
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ArpInitializeInterrupts (
    BOOL PhysicalMode,
    BOOL BootProcessor,
    PVOID Idt
    )

/*++

Routine Description:

    This routine initializes and enables interrupts.

Arguments:

    PhysicalMode - Supplies a flag indicating that the processor is running
        with translation disabled. This is unused on x86.

    BootProcessor - Supplies a flag indicating whether this is processor 0 or
        an AP.

    Idt - Supplies a pointer to the Interrrupt Descriptor Table for this
        processor.

Return Value:

    None.

--*/

{

    ARM_CPUID CpuInformation;
    ULONG SystemControl;

    if (BootProcessor != FALSE) {

        //
        // The interrupt table must be 32-byte aligned to make it into VBAR.
        //

        ASSERT(((UINTN)&ArArmInterruptTable & 0x0000001F) == 0);
    }

    //
    // Get the CPU information to determine if the processor supports security
    // extensions. If security extensions are supported, then the interrupt
    // table can be remapped to another address using the VBAR register.
    //

    SystemControl = ArGetSystemControlRegister();
    ArCpuid(&CpuInformation);
    if ((CpuInformation.ProcessorFeatures[1] &
         CPUID_PROCESSOR1_SECURITY_EXTENSION_MASK) !=
        CPUID_PROCESSOR1_SECURITY_EXTENSION_UNSUPPORTED) {

        //
        // Security extensions are supported, so turn off the high vectors and
        // set the address using VBAR.
        //

        SystemControl &= ~MMU_HIGH_EXCEPTION_VECTORS;
        ArSetVectorBaseAddress(&ArArmInterruptTable);

    //
    // Security extensions are not supported, so the vectors will have to go
    // at 0 or 0xFFFF0000, as VBAR may not work.
    //

    } else {
        if (PhysicalMode == FALSE) {

            //
            // If address translation is enabled, copy the vectors to the
            // "hivecs" address, and enable high vectors in the system control
            // register.
            //

            RtlCopyMemory((PVOID)EXCEPTION_VECTOR_ADDRESS,
                          &ArArmInterruptTable,
                          sizeof(ARM_INTERRUPT_TABLE));

            SystemControl |= MMU_HIGH_EXCEPTION_VECTORS;

        } else {

            //
            // In physical mode, copy the exception table over the firmware's,
            // whether it be at the low or high address.
            //

            if ((SystemControl & MMU_HIGH_EXCEPTION_VECTORS) != 0) {
                RtlCopyMemory((PVOID)EXCEPTION_VECTOR_ADDRESS,
                              &ArArmInterruptTable,
                              sizeof(ARM_INTERRUPT_TABLE));

            } else {
                RtlCopyMemory((PVOID)EXCEPTION_VECTOR_LOW_ADDRESS,
                              &ArArmInterruptTable,
                              sizeof(ARM_INTERRUPT_TABLE));
            }
        }
    }

    if ((((UINTN)ArpUndefinedInstructionEntry) & ARM_THUMB_BIT) != 0) {
        SystemControl |= MMU_THUMB_EXCEPTIONS;
    }

    ArSetSystemControlRegister(SystemControl);
    return;
}

VOID
ArpSetProcessorFeatures (
    PPROCESSOR_BLOCK ProcessorBlock
    )

/*++

Routine Description:

    This routine reads processor features.

Arguments:

    ProcessorBlock - Supplies a pointer to the processor block.

Return Value:

    None.

--*/

{

    PPROCESSOR_IDENTIFICATION Identification;
    ULONG MainId;

    MainId = ArGetMainIdRegister();
    Identification = &(ProcessorBlock->CpuVersion);
    Identification->Vendor = (MainId & ARM_MAIN_ID_IMPLEMENTOR_MASK) >>
                             ARM_MAIN_ID_IMPLEMENTER_SHIFT;

    Identification->Family = (MainId & ARM_MAIN_ID_PART_MASK) >>
                             ARM_MAIN_ID_PART_SHIFT;

    Identification->Model = (MainId & ARM_MAIN_ID_VARIANT_MASK) >>
                            ARM_MAIN_ID_VARIANT_SHIFT;

    Identification->Stepping = MainId & ARM_MAIN_ID_REVISION_MASK;
    return;
}

