/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    prochw.c

Abstract:

    This module processor architecture specific support for the boot loader.

Author:

    Evan Green 1-Aug-2013

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include <minoca/kernel/x86.h>
#include <minoca/kernel/ioport.h>

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
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

VOID
BoSingleStepExceptionHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

VOID
BoDebugServiceHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

VOID
BoDivideByZeroExceptionHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

VOID
BoProtectionFaultHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

VOID
BoPageFaultHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
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
    UCHAR Access
    );

VOID
BopCreateSegmentDescriptor (
    PGDT_ENTRY GdtEntry,
    PVOID Base,
    ULONG Limit,
    UCHAR Granularity,
    UCHAR Access
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store global processor structures.
//

GDT_ENTRY BoGdt[BOOT_GDT_ENTRIES];
PROCESSOR_GATE BoIdt[BOOT_IDT_SIZE];

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

    RtlDebugPrint(" *** Page Fault: Faulting Address 0x%08x, "
                  "Instruction 0x%08x",
                  FaultingAddress,
                  TrapFrame->Eip);

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
    // The first segment descriptor must be unused. Set it to zero.
    //

    GdtTable[0].LimitLow = 0;
    GdtTable[0].BaseLow = 0;
    GdtTable[0].BaseMiddle = 0;
    GdtTable[0].Access = 0;
    GdtTable[0].Granularity = 0;
    GdtTable[0].BaseHigh = 0;

    //
    // Initialize the kernel code segment. Initialize the entry to cover all
    // 4GB of memory, with read/write permissions, and only on ring 0. This is
    // not a system segment.
    //

    BopCreateSegmentDescriptor(&(GdtTable[KERNEL_CS / sizeof(GDT_ENTRY)]),
                               NULL,
                               MAX_GDT_LIMIT,
                               GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_32BIT,
                               GDT_TYPE_CODE);

    //
    // Initialize the kernel data segment. Initialize the entry to cover
    // all 4GB of memory, with read/write permissions, and only on ring 0. This
    // is not a system segment.
    //

    BopCreateSegmentDescriptor(&(GdtTable[KERNEL_DS / sizeof(GDT_ENTRY)]),
                               NULL,
                               MAX_GDT_LIMIT,
                               GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_32BIT,
                               GDT_TYPE_DATA_WRITE);

    //
    // Create a 64-bit transition code segment.
    //

    BopCreateSegmentDescriptor(
                       &(GdtTable[KERNEL64_TRANSITION_CS / sizeof(GDT_ENTRY)]),
                       NULL,
                       MAX_GDT_LIMIT,
                       GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_64BIT,
                       GDT_TYPE_CODE);

    //
    // Install the new GDT table.
    //

    Gdt.Limit = sizeof(GDT_ENTRY) * BOOT_GDT_ENTRIES;
    Gdt.Base = (ULONG)GdtTable;
    ArLoadGdtr(Gdt);
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
                  GATE_ACCESS_USER | GATE_TYPE_TRAP);

    BopCreateGate(IdtTable + VECTOR_BREAKPOINT,
                  BoBreakExceptionHandlerAsm,
                  KERNEL_CS,
                  GATE_ACCESS_USER | GATE_TYPE_INTERRUPT);

    BopCreateGate(IdtTable + VECTOR_DEBUG,
                  BoSingleStepExceptionHandlerAsm,
                  KERNEL_CS,
                  GATE_TYPE_INTERRUPT);

    BopCreateGate(IdtTable + VECTOR_DEBUG_SERVICE,
                  BoDebugServiceHandlerAsm,
                  KERNEL_CS,
                  GATE_TYPE_INTERRUPT);

    BopCreateGate(IdtTable + VECTOR_PROTECTION_FAULT,
                  BoProtectionFaultHandlerAsm,
                  KERNEL_CS,
                  GATE_TYPE_INTERRUPT);

    //
    // Set up the page fault handler.
    //

    BopCreateGate(IdtTable + VECTOR_PAGE_FAULT,
                  BoPageFaultHandlerAsm,
                  KERNEL_CS,
                  GATE_TYPE_INTERRUPT);

    BopCreateGate(IdtTable + VECTOR_STACK_EXCEPTION,
                  BoPageFaultHandlerAsm,
                  KERNEL_CS,
                  GATE_TYPE_INTERRUPT);

    //
    // Load the IDT register with our interrupt descriptor table.
    //

    IdtRegister.Limit = (BOOT_IDT_SIZE * 8) - 1;
    IdtRegister.Base = (ULONG)IdtTable;
    ArLoadIdtr(&IdtRegister);
    return;
}

VOID
BopCreateGate (
    PPROCESSOR_GATE Gate,
    PVOID HandlerRoutine,
    USHORT Selector,
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

    Access - Supplies the gate access bits, similar to the GDT access bits.

Return Value:

    None.

--*/

{

    Gate->LowOffset = (USHORT)((ULONG)HandlerRoutine & 0xFFFF);
    Gate->HighOffset = (USHORT)((ULONG)HandlerRoutine >> 16);
    Gate->Selector = Selector;

    //
    // Set bit 5-7 of the count to 0. Bits 4-0 are reserved and need to be set
    // to 0 as well.
    //

    Gate->Count = 0;
    Gate->Access = GATE_ACCESS_PRESENT | Access;
    return;
}

VOID
BopCreateSegmentDescriptor (
    PGDT_ENTRY GdtEntry,
    PVOID Base,
    ULONG Limit,
    UCHAR Granularity,
    UCHAR Access
    )

/*++

Routine Description:

    This routine initializes a GDT entry given the parameters.

Arguments:

    GdtEntry - Supplies a pointer to the GDT entry that will be initialized.

    Base - Supplies the base address where this segment begins.

    Limit - Supplies the size of the segment, either in bytes or kilobytes,
        depending on the Granularity parameter.

    Granularity - Supplies the granularity of the segment. Valid values are byte
        granularity or kilobyte granularity.

    Access - Supplies the access permissions on the segment.

    PrivilegeLevel - Supplies the privilege level that this segment requires.
        Valid values are 0 (most privileged, kernel) to 3 (user mode, least
        privileged).

Return Value:

    None.

--*/

{

    //
    // If all these magic numbers seem cryptic, see the comment above the
    // definition for the GDT_ENTRY structure.
    //

    GdtEntry->LimitLow = Limit & 0xFFFF;
    GdtEntry->BaseLow = (ULONG)Base & 0xFFFF;
    GdtEntry->BaseMiddle = ((ULONG)Base >> 16) & 0xFF;
    GdtEntry->Access = GATE_ACCESS_PRESENT | Access;
    GdtEntry->Granularity = Granularity | ((Limit >> 16) & 0xF);
    GdtEntry->BaseHigh = ((ULONG)Base >> 24) & 0xFF;
    return;
}

