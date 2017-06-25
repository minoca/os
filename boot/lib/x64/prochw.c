/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    prochw.c

Abstract:

    This module processor architecture specific support for the boot loader.

Author:

    Evan Green 6-Jun-2017

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include <minoca/kernel/x64.h>

//
// ---------------------------------------------------------------- Definitions
//

#define BOOT_GDT_ENTRIES 4
#define BOOT_IDT_SIZE (VECTOR_DEBUG_SERVICE + 1)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Assembly routines
//

VOID
BoBreakExceptionHandlerAsm (
    VOID
    );

VOID
BoSingleStepExceptionHandlerAsm (
    VOID
    );

VOID
BoDebugServiceHandlerAsm (
    VOID
    );

VOID
BoDivideByZeroExceptionHandlerAsm (
    VOID
    );

VOID
BoProtectionFaultHandlerAsm (
    VOID
    );

VOID
BoPageFaultHandlerAsm (
    VOID
    );

VOID
BoLoadBootDataSegments (
    VOID
    );

//
// C routines
//

VOID
BopInitializeGdt (
    PGDT_ENTRY GdtTable
    );

VOID
BopInitializeInterrupts (
    PVOID Idt
    );

VOID
BopCreateGate (
    PPROCESSOR_GATE Gate,
    PVOID HandlerRoutine,
    USHORT Selector,
    UCHAR StackIndex,
    UCHAR Access
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store global processor structures.
//

PROCESSOR_GATE BoIdt[BOOT_IDT_SIZE];

//
// Create the GDT, which has the following entries:
// 0x00 - Null entry, required.
// 0x08 - KERNEL_CS, flat long mode code segment.
// 0x10 - KERNEL_DS, flat data segment.
// 0x18 - KERNEL64_TRANSITION_CS, flat 32-bit code segment.
//

GDT_ENTRY BoGdt[BOOT_GDT_ENTRIES] = {
    {0},

    {
        MAX_USHORT,
        0,
        0,
        GATE_ACCESS_PRESENT | GDT_TYPE_CODE,
        GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_64BIT |
            (MAX_GDT_LIMIT >> 16),

        0
    },

    {
        MAX_USHORT,
        0,
        0,
        GATE_ACCESS_PRESENT | GDT_TYPE_DATA_WRITE,
        GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_64BIT |
            (MAX_GDT_LIMIT >> 16),

        0
    },

    {
        MAX_USHORT,
        0,
        0,
        GATE_ACCESS_PRESENT | GDT_TYPE_CODE,
        GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_32BIT |
            (MAX_GDT_LIMIT >> 16),

        0
    }
};

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

    This routine initializes processor-specific structures. In the case of x86,
    it initializes the GDT and TSS.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Initialize and load the GDT and Tasks.
    //

    BopInitializeGdt(BoGdt);
    BopInitializeInterrupts(BoIdt);
    return;
}

VOID
BoDivideByZeroHandler (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine responds to a divide by zero exception.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame.

Return Value:

    None.

--*/

{

    RtlDebugPrint(" *** Divide by zero ***\n");
    KdDebugExceptionHandler(EXCEPTION_DIVIDE_BY_ZERO, NULL, TrapFrame);
    return;
}

VOID
BoPageFaultHandler (
    PVOID FaultingAddress,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles page faults, or rather doesn't handle them.

Arguments:

    FaultingAddress - Supplies the address that caused the fault.

    TrapFrame - Supplies a pointer to the trap frame of the fault.

Return Value:

    None.

--*/

{

    RtlDebugPrint(" *** Page Fault: Faulting Address 0x%08llx, "
                  "Instruction 0x%08llx",
                  FaultingAddress,
                  TrapFrame->Rip);

    if ((TrapFrame->ErrorCode & X86_FAULT_FLAG_PROTECTION_VIOLATION) != 0) {
        RtlDebugPrint(", Protection Violation");

    } else {
        RtlDebugPrint(",  Page Not Present");
    }

    if ((TrapFrame->ErrorCode & X86_FAULT_ERROR_CODE_WRITE) != 0) {
        RtlDebugPrint(", Write ***\n");

    } else {
        RtlDebugPrint(", Read ***\n");
    }

    KdDebugExceptionHandler(EXCEPTION_ACCESS_VIOLATION, NULL, TrapFrame);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
BopInitializeGdt (
    PGDT_ENTRY GdtTable
    )

/*++

Routine Description:

    This routine initializes and loads the system's Global Descriptor Table
    (GDT).

Arguments:

    GdtTable - Supplies a pointer to the global descriptor table to use. It is
        assumed this table contains enough entries to hold all the segment
        descriptors.

    Task - Supplies a pointer to the main task.

Return Value:

    None.

--*/

{

    TABLE_REGISTER Gdt;

    //
    // Install the new GDT table.
    //

    Gdt.Limit = sizeof(GDT_ENTRY) * BOOT_GDT_ENTRIES;
    Gdt.Base = (UINTN)GdtTable;
    ArLoadGdtr(&Gdt);
    BoLoadBootDataSegments();
    return;
}

VOID
BopInitializeInterrupts (
    PVOID Idt
    )

/*++

Routine Description:

    This routine initializes and enables interrupts.

Arguments:

    Idt - Supplies a pointer to the Interrrupt Descriptor Table for this
        processor.

Return Value:

    None.

--*/

{

    TABLE_REGISTER IdtRegister;
    PPROCESSOR_GATE IdtTable;

    IdtTable = Idt;

    //
    // Set up the debug trap handlers.
    //

    BopCreateGate(IdtTable + VECTOR_DIVIDE_ERROR,
                  BoDivideByZeroExceptionHandlerAsm,
                  KERNEL_CS,
                  0,
                  GATE_ACCESS_USER | GATE_TYPE_TRAP);

    BopCreateGate(IdtTable + VECTOR_BREAKPOINT,
                  BoBreakExceptionHandlerAsm,
                  KERNEL_CS,
                  0,
                  GATE_ACCESS_USER | GATE_TYPE_INTERRUPT);

    BopCreateGate(IdtTable + VECTOR_DEBUG,
                  BoSingleStepExceptionHandlerAsm,
                  KERNEL_CS,
                  0,
                  GATE_TYPE_INTERRUPT);

    BopCreateGate(IdtTable + VECTOR_DEBUG_SERVICE,
                  BoDebugServiceHandlerAsm,
                  KERNEL_CS,
                  0,
                  GATE_TYPE_INTERRUPT);

    BopCreateGate(IdtTable + VECTOR_PROTECTION_FAULT,
                  BoProtectionFaultHandlerAsm,
                  KERNEL_CS,
                  0,
                  GATE_TYPE_INTERRUPT);

    //
    // Set up the page fault handler.
    //

    BopCreateGate(IdtTable + VECTOR_PAGE_FAULT,
                  BoPageFaultHandlerAsm,
                  KERNEL_CS,
                  0,
                  GATE_TYPE_INTERRUPT);

    BopCreateGate(IdtTable + VECTOR_STACK_EXCEPTION,
                  BoPageFaultHandlerAsm,
                  KERNEL_CS,
                  0,
                  GATE_TYPE_INTERRUPT);

    //
    // Load the IDT register with our interrupt descriptor table.
    //

    IdtRegister.Limit = (BOOT_IDT_SIZE * sizeof(PROCESSOR_GATE)) - 1;
    IdtRegister.Base = (UINTN)IdtTable;
    ArLoadIdtr(&IdtRegister);
    return;
}

VOID
BopCreateGate (
    PPROCESSOR_GATE Gate,
    PVOID HandlerRoutine,
    USHORT Selector,
    UCHAR StackIndex,
    UCHAR Access
    )

/*++

Routine Description:

    This routine initializes a task, call, trap, or interrupt gate with the
    given values.

Arguments:

    Gate - Supplies a pointer to the structure where the gate will be returned.
        It is assumed this structure is already allocated.

    HandlerRoutine - Supplies a pointer to the destination routine of this gate.

    Selector - Supplies the code selector this gate should run in.

    StackIndex - Supplies the interrupt stack index to use. Supply to to not
        switch stacks.

    Access - Supplies the gate access bits, similar to the GDT access bits.

Return Value:

    None.

--*/

{

    Gate->LowOffset = (USHORT)((UINTN)HandlerRoutine & 0xFFFF);
    Gate->MidOffset = (USHORT)((UINTN)HandlerRoutine >> 16);
    Gate->HighWord = (ULONG)((UINTN)HandlerRoutine >> 32);
    Gate->Selector = Selector;
    Gate->Ist = StackIndex;
    Gate->Access = Access | GATE_ACCESS_PRESENT;
    Gate->Reserved = 0;
    return;
}

