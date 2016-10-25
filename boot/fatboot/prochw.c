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

//
// ---------------------------------------------------------------- Definitions
//

#define BOOT_GDT_ENTRIES 3
#define BOOT_IDT_SIZE (VECTOR_PROTECTION_FAULT + 1)

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
BoProtectionFaultHandlerAsm (
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
    UCHAR Type,
    UCHAR Privilege
    );

VOID
BopCreateSegmentDescriptor (
    PGDT_ENTRY GdtEntry,
    PVOID Base,
    ULONG Limit,
    GDT_GRANULARITY Granularity,
    GDT_SEGMENT_TYPE Access,
    UCHAR PrivilegeLevel,
    BOOL System
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
                               GdtKilobyteGranularity,
                               GdtCodeExecuteOnly,
                               0,
                               FALSE);

    //
    // Initialize the kernel data segment. Initialize the entry to cover
    // all 4GB of memory, with read/write permissions, and only on ring 0. This
    // is not a system segment.
    //

    BopCreateSegmentDescriptor(&(GdtTable[KERNEL_DS / sizeof(GDT_ENTRY)]),
                               NULL,
                               MAX_GDT_LIMIT,
                               GdtKilobyteGranularity,
                               GdtDataReadWrite,
                               0,
                               FALSE);

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

    BopCreateGate(IdtTable + VECTOR_BREAKPOINT,
                  BoBreakExceptionHandlerAsm,
                  KERNEL_CS,
                  INTERRUPT_GATE_TYPE,
                  3);

    BopCreateGate(IdtTable + VECTOR_DEBUG,
                  BoSingleStepExceptionHandlerAsm,
                  KERNEL_CS,
                  INTERRUPT_GATE_TYPE,
                  0);

    BopCreateGate(IdtTable + VECTOR_PROTECTION_FAULT,
                  BoProtectionFaultHandlerAsm,
                  KERNEL_CS,
                  INTERRUPT_GATE_TYPE,
                  0);

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
    UCHAR Type,
    UCHAR Privilege
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

    Type - Supplies the type of the gate. Set this to CALL_GATE_TYPE,
        INTERRUPT_GATE_TYPE, TASK_GATE_TYPE, or TRAP_GATE_TYPE.

    Privilege - Supplies the privilege level this gate should run in. 0 is the
        most privileged level, and 3 is the least privileged.

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

    //
    // Access is programmed as follows:
    //     Bit 7: Present. Set to 1 to indicate that this gate is present.
    //     Bits 5-6: Privilege level.
    //     Bit 4: Set to 0 to indicate it's a system gate.
    //     Bits 3-0: Type.
    //

    Gate->Access = Type | (Privilege << 5) | (1 << 7);
    return;
}

VOID
BopCreateSegmentDescriptor (
    PGDT_ENTRY GdtEntry,
    PVOID Base,
    ULONG Limit,
    GDT_GRANULARITY Granularity,
    GDT_SEGMENT_TYPE Access,
    UCHAR PrivilegeLevel,
    BOOL System
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

    System - Supplies a flag indicating whether this is a system segment (TRUE)
        or a code/data segment.

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
    GdtEntry->Access = DEFAULT_GDT_ACCESS |
                       ((PrivilegeLevel & 0x3) << 5) |
                       (Access & 0xF);

    if (System != FALSE) {
        GdtEntry->Access |= GDT_SYSTEM_SEGMENT;

    } else {
        GdtEntry->Access |= GDT_CODE_DATA_SEGMENT;
    }

    GdtEntry->Granularity = DEFAULT_GDT_GRANULARITY |
                            Granularity |
                            ((Limit >> 16) & 0xF);

    GdtEntry->BaseHigh = ((ULONG)Base >> 24) & 0xFF;
    return;
}

