/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    prochw.c

Abstract:

    This module implements support functionality for hardware that is specific
    to the AMD64 architecture.

Author:

    Evan Green 8-Jun-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x64.h>
#include <minoca/kernel/ioport.h>
#include <minoca/kernel/kdebug.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number and size of alternate stacks.
//

#define ALTERNATE_STACK_COUNT 2
#define ALTERNATE_STACK_SIZE 8192

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Declare any still-undefined built in interrupt handlers.
//

VOID
ArBreakExceptionHandlerAsm (
    VOID
    );

VOID
KdNmiHandlerAsm (
    VOID
    );

VOID
ArSingleStepExceptionHandlerAsm (
    VOID
    );

VOID
KdDebugServiceHandlerAsm (
    VOID
    );

VOID
ArDivideByZeroExceptionHandlerAsm (
    VOID
    );

VOID
ArFpuAccessExceptionHandlerAsm (
    VOID
    );

VOID
ArDoubleFaultHandlerAsm (
    VOID
    );

VOID
ArProtectionFaultHandlerAsm (
    VOID
    );

VOID
ArMathFaultHandlerAsm (
    VOID
    );

INTN
ArSystemCallHandlerAsm (
    VOID
    );

VOID
ArpPageFaultHandlerAsm (
    VOID
    );

VOID
HlSpuriousInterruptHandlerAsm (
    VOID
    );

VOID
ArpCreateGate (
    PPROCESSOR_GATE Gate,
    PVOID HandlerRoutine,
    UCHAR Ist,
    UCHAR Access
    );

VOID
ArpInitializeTss (
    PTSS64 Task,
    PVOID NmiStack,
    PVOID DoubleFaultStack
    );

VOID
ArpInitializeGdt (
    PGDT_ENTRY GdtTable,
    PPROCESSOR_BLOCK ProcessorBlock,
    PTSS64 Tss
    );

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
// Store pointers to functions used to save and restore floating point state.
//

PAR_SAVE_RESTORE_FPU_CONTEXT ArSaveFpuState = ArFxSave;
PAR_SAVE_RESTORE_FPU_CONTEXT ArRestoreFpuState = ArFxRestore;

//
// Store globals for the per-processor data structures used by the boot
// processor.
//

//
// Define the boot processor's TSS.
//

TSS64 ArP0Tss;

//
// Create the GDT, which has the following entries:
// 0x00 - Null entry, required.
// 0x08 - KERNEL_CS, flat 64-bit code segment.
// 0x10 - KERNEL_DS, flat 64-bit data segment.
// 0x18 - USER_CS, flat user-mode 64-bit code segment.
// 0x20 - USER_DS, flat user-mode 64-bit data segment.
// 0x28 - KERNEL_TSS, long mode kernel TSS segment (double sized).
//

GDT_ENTRY ArP0Gdt[X64_GDT_ENTRIES] = {
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
        GATE_ACCESS_USER | GATE_ACCESS_PRESENT | GDT_TYPE_CODE,
        GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_32BIT |
            (MAX_GDT_LIMIT >> 16),

        0
    },

    {
        MAX_USHORT,
        0,
        0,
        GATE_ACCESS_USER | GATE_ACCESS_PRESENT | GDT_TYPE_DATA_WRITE,
        GDT_GRANULARITY_KILOBYTE | GDT_GRANULARITY_64BIT |
            (MAX_GDT_LIMIT >> 16),

        0
    },

    {
        sizeof(TSS64) - 1,
        0,
        0,
        GATE_ACCESS_PRESENT | GDT_TYPE_TSS,
        0,
        0
    },

    {0}
};

PROCESSOR_GATE ArP0Idt[IDT_SIZE];
PROCESSOR_BLOCK ArP0ProcessorBlock;
PVOID ArP0InterruptTable[MAXIMUM_VECTOR - MINIMUM_VECTOR + 1] = {NULL};

//
// Pointers to the interrupt dispatch code, which is repeated from the minimum
// to maximum device IDT entries.
//

extern UCHAR HlVectorStart;
extern UCHAR HlVectorEnd;

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

    This routine initializes processor-specific structures. In the case of x86,
    it initializes the GDT and TSS.

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

    UINTN Address;
    BOOL BootProcessor;
    UINTN Cr0;
    PVOID DoubleFaultStack;
    PGDT_ENTRY Gdt;
    PPROCESSOR_GATE Idt;
    PVOID InterruptTable;
    PVOID NmiStack;
    UINTN PageSize;
    PPROCESSOR_BLOCK ProcessorBlock;
    PTSS64 Tss;

    BootProcessor = TRUE;
    DoubleFaultStack = NULL;
    NmiStack = NULL;

    //
    // Physical mode implies P0.
    //

    if (PhysicalMode != FALSE) {
        Gdt = ArP0Gdt;
        Idt = ArP0Idt;
        InterruptTable = ArP0InterruptTable;
        ProcessorBlock = &ArP0ProcessorBlock;
        Tss = &ArP0Tss;

    } else {

        //
        // Use the globals if this is the boot processor because the memory
        // subsystem is not yet online.
        //

        if (ProcessorStructures == NULL) {
            Gdt = ArP0Gdt;
            Idt = ArP0Idt;
            InterruptTable = ArP0InterruptTable;
            ProcessorBlock = &ArP0ProcessorBlock;
            Tss = &ArP0Tss;

        } else {
            BootProcessor = FALSE;
            PageSize = MmPageSize();
            Address = ALIGN_RANGE_UP((UINTN)ProcessorStructures, PageSize);
            Address += ALTERNATE_STACK_SIZE;
            NmiStack = (PVOID)Address;
            Address += ALTERNATE_STACK_SIZE;
            DoubleFaultStack = (PVOID)Address;
            Tss = (PVOID)Address;
            Address += sizeof(ArP0Tss);
            Gdt = (PVOID)Address;

            ASSERT(ALIGN_RANGE_DOWN((UINTN)Gdt, 8) == (UINTN)Gdt);

            Address += sizeof(ArP0Gdt);
            ProcessorBlock = (PVOID)Address;

            //
            // Use a global IDT space.
            //

            Idt = ArP0Idt;
            InterruptTable = ArP0InterruptTable;
        }
    }

    //
    // Initialize the pointer to the processor block.
    //

    ProcessorBlock->Self = ProcessorBlock;
    ProcessorBlock->Idt = Idt;
    ProcessorBlock->InterruptTable = InterruptTable;
    ProcessorBlock->Tss = Tss;
    ProcessorBlock->Gdt = Gdt;

    //
    // Initialize and load the GDT and Tasks.
    //

    ArpInitializeTss(Tss, NmiStack, DoubleFaultStack);
    ArpInitializeGdt(Gdt, ProcessorBlock, Tss);
    ArLoadTr(KERNEL_TSS);
    ArpInitializeInterrupts(PhysicalMode, BootProcessor, Idt);
    ArpSetProcessorFeatures(ProcessorBlock);
    ArWriteMsr(X86_MSR_FSBASE, 0);
    ArWriteMsr(X86_MSR_GSBASE, (UINTN)ProcessorBlock);

    //
    // Initialize the FPU, then disable access to it again.
    //

    Cr0 = ArGetControlRegister0();
    ArEnableFpu();
    ArInitializeFpu();
    ArSetControlRegister0(Cr0);
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

    UINTN Address;
    PVOID Allocation;
    UINTN AllocationSize;
    PPROCESSOR_BLOCK ProcessorBlock;
    PVOID Stack;
    PTSS64 Tss;

    ProcessorBlock = KeGetCurrentProcessorBlock();
    Tss = ProcessorBlock->Tss;

    //
    // Allocate and initialize double fault and NMI stacks now that MM is up
    // and running.
    //

    AllocationSize = ALTERNATE_STACK_SIZE * ALTERNATE_STACK_COUNT;
    Allocation = MmAllocateNonPagedPool(AllocationSize, ARCH_POOL_TAG);
    if (Allocation == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Initialize the double fault stack.
    //

    Address = (UINTN)Allocation + ALTERNATE_STACK_SIZE;
    Stack = (PVOID)(ALIGN_RANGE_DOWN(Address, 16));
    Tss->Ist[X64_IST_DOUBLE_FAULT] = (UINTN)Stack;

    //
    // Initialize the NMI stack (separate stack needed to avoid vulnerable
    // window during/before sysret instruction).
    //

    Address += ALTERNATE_STACK_SIZE;
    Stack = (PVOID)(ALIGN_RANGE_DOWN(Address, 16));
    Tss->Ist[X64_IST_NMI] = (UINTN)Stack;
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

    UINTN Address;
    PVOID Allocation;
    UINTN AllocationSize;
    UINTN PageSize;
    PPROCESSOR_BLOCK ProcessorBlock;

    //
    // Allocate an extra page for alignment purposes, as TSS structures are
    // not supposed to cross page boundaries.
    //

    PageSize = MmPageSize();
    AllocationSize = ALTERNATE_STACK_COUNT * ALTERNATE_STACK_SIZE;
    AllocationSize += sizeof(ArP0Gdt) + sizeof(PROCESSOR_BLOCK) +
                      sizeof(ArP0Tss) + PageSize;

    Allocation = MmAllocateNonPagedPool(AllocationSize, ARCH_POOL_TAG);
    if (Allocation == NULL) {
        return NULL;
    }

    RtlZeroMemory(Allocation, AllocationSize);
    Address = ALIGN_RANGE_UP((UINTN)Allocation, PageSize);
    ProcessorBlock = (PPROCESSOR_BLOCK)((PUCHAR)Address +
                                        (ALTERNATE_STACK_COUNT *
                                         ALTERNATE_STACK_SIZE) +
                                        sizeof(ArP0Gdt));

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

    //
    // Translation is architecturally always enabled in long mode.
    //

    return TRUE;
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

    return (PVOID)TrapFrame->Rip;
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

VOID
ArClearTssBusyBit (
    USHORT TssSegment
    )

/*++

Routine Description:

    This routine clears the busy bit in the GDT for the given segment. It is
    assumed this segment is used on the current processor.

Arguments:

    TssSegment - Supplies the TSS segment for the busy bit to clear.

Return Value:

    None.

--*/

{

    PGDT_ENTRY Gdt;
    PPROCESSOR_BLOCK ProcessorBlock;

    ProcessorBlock = KeGetCurrentProcessorBlock();
    Gdt = ProcessorBlock->Gdt;
    Gdt += TssSegment / sizeof(GDT_ENTRY);

    ASSERT((Gdt->Access & ~GDT_TSS_BUSY) ==
           (GATE_ACCESS_PRESENT | GDT_TYPE_TSS));

    Gdt->Access &= ~GDT_TSS_BUSY;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ArpHandleDoubleFault (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine handles double faults as gracefully as possible.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame.

Return Value:

    This routine does not return, double faults are not recoverable.

--*/

{

    KdDebugExceptionHandler(EXCEPTION_DOUBLE_FAULT, NULL, TrapFrame);
    KeCrashSystem(CRASH_KERNEL_STACK_EXCEPTION, (UINTN)TrapFrame, 0, 0, 0);
    return;
}

VOID
ArpCreateGate (
    PPROCESSOR_GATE Gate,
    PVOID HandlerRoutine,
    UCHAR Ist,
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

    Ist - Supplies the interrupt stack index to run on. Supply 0 to not change
        stacks when the interrupt comes in.

    Access - Supplies the gate access bits, similar to the GDT access bits.

Return Value:

    None.

--*/

{

    Gate->LowOffset = (USHORT)((UINTN)HandlerRoutine & 0xFFFF);
    Gate->MidOffset = (USHORT)((UINTN)HandlerRoutine >> 16);
    Gate->HighWord = (UINTN)HandlerRoutine >> 32;
    Gate->Selector = KERNEL_CS;
    Gate->Ist = Ist;
    Gate->Access = GATE_ACCESS_PRESENT | Access;
    return;
}

VOID
ArpInitializeTss (
    PTSS64 Task,
    PVOID NmiStack,
    PVOID DoubleFaultStack
    )

/*++

Routine Description:

    This routine initializes and loads the kernel Task State Segment (TSS).

Arguments:

    Task - Supplies a pointer to the task to initialize and load.

    NmiStack - Supplies a pointer to the NMI stack to use.

    DoubleFaultStack - Supplies a pointer to the double fault stack to use.

Return Value:

    None.

--*/

{

    RtlZeroMemory(Task, sizeof(TSS64));
    Task->Ist[X64_IST_NMI] = (UINTN)NmiStack;
    Task->Ist[X64_IST_DOUBLE_FAULT] = (UINTN)DoubleFaultStack;
    Task->IoMapBase = sizeof(TSS64);
    return;
}

VOID
ArpInitializeGdt (
    PGDT_ENTRY GdtTable,
    PPROCESSOR_BLOCK ProcessorBlock,
    PTSS64 Tss
    )

/*++

Routine Description:

    This routine initializes and loads the kernel's Global Descriptor Table
    (GDT).

Arguments:

    GdtTable - Supplies a pointer to the global descriptor table to use. It is
        assumed this table contains enough entries to hold all the segment
        descriptors.

    ProcessorBlock - Supplies a pointer to the processor block to use for this
        processor.

    Tss - Supplies a pointer to the main kernel task.

Return Value:

    None.

--*/

{

    PGDT64_ENTRY Entry64;
    TABLE_REGISTER Gdt;

    //
    // Set the pointer to the kernel TSS, which is really the only thing that's
    // different between processors in the GDT. The limit, type, access, and
    // granularity are already set up correctly, it's just the base address
    // that needs fixing.
    //

    Entry64 = (PGDT64_ENTRY)(&(GdtTable[KERNEL_TSS / sizeof(GDT_ENTRY)]));
    Entry64->BaseLow = (USHORT)(UINTN)Tss;
    Entry64->BaseMiddle = (UCHAR)((UINTN)Tss >> 16);
    Entry64->BaseHigh = (UCHAR)((UINTN)Tss >> 24);
    Entry64->BaseHigh32 = (ULONG)((UINTN)Tss >> 32);

    //
    // Install the new GDT table.
    //

    Gdt.Limit = sizeof(GDT_ENTRY) * X64_GDT_ENTRIES;
    Gdt.Base = (UINTN)GdtTable;
    ArLoadGdtr(&Gdt);
    ArLoadKernelDataSegments();
    return;
}

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
        with translation disabled.

    BootProcessor - Supplies a flag indicating whether this is processor 0 or
        an AP.

    Idt - Supplies a pointer to the Interrrupt Descriptor Table for this
        processor.

Return Value:

    None.

--*/

{

    ULONG DispatchCodeLength;
    ULONG IdtIndex;
    TABLE_REGISTER IdtRegister;
    PPROCESSOR_GATE IdtTable;
    PVOID ServiceRoutine;

    IdtTable = Idt;
    if (BootProcessor != FALSE) {

        //
        // Initialize the device vectors of the IDT. The vector dispatch code
        // is a bunch of copies of the same code, the only difference is which
        // vector number they push as a parameter.
        //

        DispatchCodeLength = (ULONG)(&HlVectorEnd - &HlVectorStart) /
                             (MAXIMUM_VECTOR - MINIMUM_VECTOR);

        for (IdtIndex = MINIMUM_VECTOR;
             IdtIndex < MAXIMUM_VECTOR;
             IdtIndex += 1) {

            ServiceRoutine = &HlVectorStart + ((IdtIndex - MINIMUM_VECTOR) *
                                               DispatchCodeLength);

            ArpCreateGate(IdtTable + IdtIndex,
                          ServiceRoutine,
                          0,
                          GATE_TYPE_INTERRUPT);
        }

        //
        // Set up the debug trap handlers.
        //

        ArpCreateGate(IdtTable + VECTOR_DIVIDE_ERROR,
                      ArDivideByZeroExceptionHandlerAsm,
                      0,
                      GATE_ACCESS_USER | GATE_TYPE_TRAP);

        ArpCreateGate(IdtTable + VECTOR_NMI,
                      KdNmiHandlerAsm,
                      X64_IST_NMI,
                      GATE_TYPE_INTERRUPT);

        ArpCreateGate(IdtTable + VECTOR_BREAKPOINT,
                      ArBreakExceptionHandlerAsm,
                      0,
                      GATE_ACCESS_USER | GATE_TYPE_INTERRUPT);

        ArpCreateGate(IdtTable + VECTOR_DEBUG,
                      ArSingleStepExceptionHandlerAsm,
                      0,
                      GATE_TYPE_INTERRUPT);

        ArpCreateGate(IdtTable + VECTOR_DEBUG_SERVICE,
                      KdDebugServiceHandlerAsm,
                      0,
                      GATE_TYPE_INTERRUPT);

        //
        // Set up the double fault and general protection fault handlers.
        //

        ArpCreateGate(IdtTable + VECTOR_DOUBLE_FAULT,
                      ArDoubleFaultHandlerAsm,
                      X64_IST_DOUBLE_FAULT,
                      GATE_TYPE_INTERRUPT);

        ArpCreateGate(IdtTable + VECTOR_PROTECTION_FAULT,
                      ArProtectionFaultHandlerAsm,
                      0,
                      GATE_ACCESS_USER | GATE_TYPE_INTERRUPT);

        ArpCreateGate(IdtTable + VECTOR_MATH_FAULT,
                      ArMathFaultHandlerAsm,
                      0,
                      GATE_TYPE_INTERRUPT);

        //
        // Set up the system call handler.
        //

        ArpCreateGate(IdtTable + VECTOR_SYSTEM_CALL,
                      ArSystemCallHandlerAsm,
                      0,
                      GATE_ACCESS_USER | GATE_TYPE_TRAP);

        //
        // Set up the spurious interrupt vector.
        //

        ArpCreateGate(IdtTable + VECTOR_SPURIOUS_INTERRUPT,
                      HlSpuriousInterruptHandlerAsm,
                      0,
                      GATE_TYPE_INTERRUPT);

        //
        // Set up the page fault handler.
        //

        ArpCreateGate(IdtTable + VECTOR_PAGE_FAULT,
                      ArpPageFaultHandlerAsm,
                      0,
                      GATE_TYPE_INTERRUPT);

        ArpCreateGate(IdtTable + VECTOR_STACK_EXCEPTION,
                      ArpPageFaultHandlerAsm,
                      0,
                      GATE_TYPE_INTERRUPT);

        //
        // Set up floating point access handlers.
        //

        ArpCreateGate(IdtTable + VECTOR_DEVICE_NOT_AVAILABLE,
                      ArFpuAccessExceptionHandlerAsm,
                      0,
                      GATE_TYPE_TRAP);
    }

    //
    // Load the IDT register with our interrupt descriptor table.
    //

    IdtRegister.Limit = (IDT_SIZE * 8) - 1;
    IdtRegister.Base = (UINTN)IdtTable;
    ArLoadIdtr(&IdtRegister);
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

    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    ULONG ExtendedFamily;
    ULONG ExtendedModel;
    ULONG Family;
    PPROCESSOR_IDENTIFICATION Identification;
    ULONG Model;

    Identification = &(ProcessorBlock->CpuVersion);

    //
    // First call CPUID to find out the highest supported value.
    //

    Eax = X86_CPUID_IDENTIFICATION;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);
    Identification->Vendor = Ebx;
    if (Eax < X86_CPUID_BASIC_INFORMATION) {
        return;
    }

    Eax = X86_CPUID_BASIC_INFORMATION;
    ArCpuid(&Eax, &Ebx, &Ecx, &Edx);

    //
    // Tease out the family, model, and stepping information.
    //

    Family = (Eax & X86_CPUID_BASIC_EAX_BASE_FAMILY_MASK) >>
             X86_CPUID_BASIC_EAX_BASE_FAMILY_SHIFT;

    Model = (Eax & X86_CPUID_BASIC_EAX_BASE_MODEL_MASK) >>
            X86_CPUID_BASIC_EAX_BASE_MODEL_SHIFT;

    ExtendedFamily = (Eax & X86_CPUID_BASIC_EAX_EXTENDED_FAMILY_MASK) >>
                     X86_CPUID_BASIC_EAX_EXTENDED_FAMILY_SHIFT;

    ExtendedModel = (Eax & X86_CPUID_BASIC_EAX_EXTENDED_MODEL_MASK) >>
                    X86_CPUID_BASIC_EAX_EXTENDED_MODEL_SHIFT;

    Identification->Family = Family;
    Identification->Model = Model;
    Identification->Stepping = Eax & X86_CPUID_BASIC_EAX_STEPPING_MASK;

    //
    // Certain well-known vendors have minor quirks about how their family and
    // model values are computed.
    //

    if (Identification->Vendor == X86_VENDOR_INTEL) {
        if (Family == 0xF) {
            Identification->Family = Family + ExtendedFamily;
        }

        if ((Family == 0xF) || (Family == 0x6)) {
            Identification->Model = (ExtendedModel << 4) + Model;
        }

    } else if (Identification->Vendor == X86_VENDOR_AMD) {
        Identification->Family = Family + ExtendedFamily;
        if (Model == 0xF) {
            Identification->Model = (ExtendedModel << 4) + Model;
        }
    }

    return;
}

