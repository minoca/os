/*++

Copyright (c) 2013 Minoca Corp.

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

    Evan Green 13-Aug-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/arm.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Internal assembly routines
//

VOID
BoInitializeExceptionStacks (
    PVOID ExceptionStacksBase,
    ULONG ExceptionStackSize
    );

VOID
BoUndefinedInstructionEntry (
    VOID
    );

VOID
BoSoftwareInterruptEntry (
    VOID
    );

VOID
BoPrefetchAbortEntry (
    VOID
    );

VOID
BoDataAbortEntry (
    VOID
    );

VOID
BoIrqEntry (
    VOID
    );

VOID
BoFiqEntry (
    VOID
    );

BOOL
BoDisableInterrupts (
    VOID
    );

VOID
BoEnableInterrupts (
    VOID
    );

BOOL
BoAreInterruptsEnabled (
    VOID
    );

VOID
BoCpuid (
    PARM_CPUID Features
    );

//
// Internal C routines
//

VOID
BopInitializeInterrupts (
    VOID
    );

VOID
BopDispatchException (
    PTRAP_FRAME TrapFrame,
    BOOL PrefetchAbort
    );

VOID
BopDispatchUndefinedInstructionException (
    PTRAP_FRAME TrapFrame
    );

VOID
BopDoubleFaultHandler (
    PTRAP_FRAME TrapFrame
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

ULONG BoExceptionStacks[EXCEPTION_STACK_COUNT * EXCEPTION_STACK_SIZE];

//
// Global containing a partially initialized interrupt table. This table will
// be copied to the real location, either 0 or 0xFFFF0000.
//

extern ARM_INTERRUPT_TABLE BoArmInterruptTable;

//
// ------------------------------------------------------------------ Functions
//

ULONG
MmPageSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of a page of memory.

Arguments:

    None.

Return Value:

    Returns the size of one page of memory (ie the minimum mapping granularity).

--*/

{

    return PAGE_SIZE;
}

ULONG
MmPageShift (
    VOID
    )

/*++

Routine Description:

    This routine returns the amount to shift by to truncate an address to a
    page number.

Arguments:

    None.

Return Value:

    Returns the amount to shift to reach page granularity.

--*/

{

    return PAGE_SHIFT;
}

VOID
BoInitializeProcessor (
    VOID
    )

/*++

Routine Description:

    This routine initializes processor-specific structures.

Arguments:

    None.

Return Value:

    None.

--*/

{

    BoInitializeExceptionStacks(BoExceptionStacks, EXCEPTION_STACK_SIZE);
    BopInitializeInterrupts();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
BopInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine initializes and enables interrupts.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ARM_CPUID CpuInformation;
    ULONG SystemControl;

    //
    // The interrupt table must be 32-byte aligned to make it into VBAR.
    //

    ASSERT(((UINTN)&BoArmInterruptTable & 0x0000001F) == 0);

    //
    // Initialize the vectors to jump to for each type of interrupt.
    //

    BoArmInterruptTable.UndefinedInstructionVector =
                                                   BoUndefinedInstructionEntry;

    BoArmInterruptTable.SoftwareInterruptVector = BoSoftwareInterruptEntry;
    BoArmInterruptTable.PrefetchAbortVector = BoPrefetchAbortEntry;
    BoArmInterruptTable.DataAbortVector = BoDataAbortEntry;
    BoArmInterruptTable.IrqVector = BoIrqEntry;
    BoArmInterruptTable.FiqVector = BoFiqEntry;

    //
    // Get the CPU information to determine if the processor supports security
    // extensions. If security extensions are supported, then the interrupt
    // table can be remapped to another address using the VBAR register.
    //

    SystemControl = ArGetSystemControlRegister();
    BoCpuid(&CpuInformation);
    if ((CpuInformation.ProcessorFeatures[1] &
         CPUID_PROCESSOR1_SECURITY_EXTENSION_MASK) !=
        CPUID_PROCESSOR1_SECURITY_EXTENSION_UNSUPPORTED) {

        //
        // Security extensions are supported, so turn off the high vectors and
        // set the address using VBAR.
        //

        SystemControl &= ~MMU_HIGH_EXCEPTION_VECTORS;
        ArSetVectorBaseAddress(&BoArmInterruptTable);

    //
    // Security extensions are not supported, so the vectors will have to go
    // at 0 or 0xFFFF0000, as VBAR may not work.
    //

    } else {

        //
        // In physical mode, copy the exception table over the firmware's,
        // whether it be at the low or high address.
        //

        if ((SystemControl & MMU_HIGH_EXCEPTION_VECTORS) != 0) {
            RtlCopyMemory((PVOID)EXCEPTION_VECTOR_ADDRESS,
                          &BoArmInterruptTable,
                          sizeof(ARM_INTERRUPT_TABLE));

        } else {
            RtlCopyMemory((PVOID)EXCEPTION_VECTOR_LOW_ADDRESS,
                          &BoArmInterruptTable,
                          sizeof(ARM_INTERRUPT_TABLE));
        }
    }

    if ((((UINTN)BoUndefinedInstructionEntry) & ARM_THUMB_BIT) != 0) {
        SystemControl |= MMU_THUMB_EXCEPTIONS;
    }

    ArSetSystemControlRegister(SystemControl);
    return;
}

VOID
BopDispatchException (
    PTRAP_FRAME TrapFrame,
    BOOL PrefetchAbort
    )

/*++

Routine Description:

    This routine receives a generic exception and dispatches it to the correct
    handler based on the type of exception and the previous execution mode.

Arguments:

    TrapFrame - Supplies a pointer to the state immediately before the
        exception.

    PrefetchAbort - Supplies a boolean indicating if this is a prefetch abort
        or data abort. For non-aborts, this parameter is undefined.

Return Value:

    None.

--*/

{

    PVOID FaultingAddress;
    ULONG FaultStatus;

    ASSERT(BoAreInterruptsEnabled() == FALSE);

    //
    // The SVC mode stack pointer is wrong because it has the trap frame on it.
    // "Add" that off to get the real stack pointer.
    //

    TrapFrame->SvcSp += sizeof(TRAP_FRAME);

    //
    // Dispatch the exception according to which mode it came from.
    //

    switch (TrapFrame->ExceptionCpsr & ARM_MODE_MASK) {
    case ARM_MODE_FIQ:
    case ARM_MODE_IRQ:
        TrapFrame->Pc -= 4;
        KdDebugExceptionHandler(EXCEPTION_UNHANDLED_INTERRUPT, NULL, TrapFrame);
        break;

    case ARM_MODE_ABORT:

        //
        // The trap handlers set the overflow flag of the exception-mode PSR for
        // prefetch (instruction) aborts. This helps determine which Fault
        // Address Register to read.
        //

        if (PrefetchAbort != FALSE) {
            FaultingAddress = ArGetInstructionFaultingAddress();
            FaultStatus = ArGetInstructionFaultStatus();

        } else {
            FaultingAddress = ArGetDataFaultingAddress();
            FaultStatus = ArGetDataFaultStatus();
        }

        //
        // Translate the fault status register a bit.
        //

        RtlDebugPrint(" *** Page Fault: Faulting Address 0x%08x, "
                      "Instruction 0x%08x",
                      FaultingAddress,
                      TrapFrame->Pc);

        if (IS_ARM_PAGE_FAULT(FaultStatus)) {
            RtlDebugPrint(",  Page Not Present");

        } else {
            RtlDebugPrint(", Protection Violation");
        }

        if ((FaultStatus & ARM_FAULT_STATUS_WRITE) != 0) {
            RtlDebugPrint(", Write ***\n");

        } else {
            RtlDebugPrint(", Read ***\n");
        }

        KdDebugExceptionHandler(EXCEPTION_ACCESS_VIOLATION, NULL, TrapFrame);
        break;

    case ARM_MODE_UNDEF:
        KdDebugExceptionHandler(EXCEPTION_UNDEFINED_INSTRUCTION,
                                NULL,
                                TrapFrame);

        break;

    default:
        KdDebugExceptionHandler(EXCEPTION_ACCESS_VIOLATION, NULL, TrapFrame);
        break;
    }

    //
    // Re-adjust the SVC stack pointer. If it was changed since the first
    // adjustment, the routine changing it *must* copy the trap frame over.
    //

    TrapFrame->SvcSp -= sizeof(TRAP_FRAME);
    return;
}

VOID
BopDispatchUndefinedInstructionException (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine is called from the assembly trap handlers to handle the
    undefined instruction exception, which is usually an intentional debug
    break.

Arguments:

    TrapFrame - Supplies a pointer to the state immediately before the
        exception.

Return Value:

    None.

--*/

{

    PVOID Address;
    ULONG Exception;
    ULONG Instruction;
    PVOID Parameter;

    //
    // The SVC mode stack pointer is wrong because it has the trap frame on it.
    // "Add" that off to get the real stack pointer.
    //

    TrapFrame->SvcSp += sizeof(TRAP_FRAME);

    //
    // Since this is an undefined instruction entry and not a data abort,
    // the memory at PC must be valid. If this is a debug service
    // exception, get parameters.
    //

    Exception = EXCEPTION_UNDEFINED_INSTRUCTION;
    Parameter = NULL;
    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        Address = (PVOID)REMOVE_THUMB_BIT(TrapFrame->Pc) -
                  THUMB16_INSTRUCTION_LENGTH;

        Instruction = *((PUSHORT)Address);
        if (Instruction == THUMB_DEBUG_SERVICE_INSTRUCTION) {
            Exception = TrapFrame->R0;
            Parameter = (PVOID)TrapFrame->R1;
        }

    } else {
        Instruction = *((PULONG)(TrapFrame->Pc - ARM_INSTRUCTION_LENGTH));
        if (Instruction == ARM_DEBUG_SERVICE_INSTRUCTION) {
            Exception = TrapFrame->R0;
            Parameter = (PVOID)TrapFrame->R1;
        }
    }

    //
    // Dispatch the exception according to which mode it came from.
    //

    KdDebugExceptionHandler(Exception, Parameter, TrapFrame);

    //
    // Re-adjust the SVC stack pointer. If it was changed since the first
    // adjustment, the routine changing it *must* copy the trap frame over.
    //

    TrapFrame->SvcSp -= sizeof(TRAP_FRAME);
    return;
}

VOID
BopDoubleFaultHandler (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine is called when a stack exception is taken by the trap handlers.
    It attmepts to take the system down gracefully.

Arguments:

    TrapFrame - Supplies a pointer to the state immediately before the
        exception.

Return Value:

    This routine does not return.

--*/

{

    //
    // First enter the debugger with this context, then crash.
    //

    KdDebugExceptionHandler(EXCEPTION_DOUBLE_FAULT, NULL, TrapFrame);
    return;
}

