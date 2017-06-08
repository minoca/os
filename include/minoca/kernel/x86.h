/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    x86.h

Abstract:

    This header contains definitions for aspects of the system that are specific
    to the x86 architecture.

Author:

    Evan Green 3-Jul-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x86defs.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// --------------------------------------------------------------------- Macros
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the format of a task, interrupt, or call gate
    descriptor. This structure must not be padded, since the hardware relies on
    this exact format.

Members:

    LowOffset - Stores the lower 16 bits of the gate's destination address.

    Selector - Stores the code segment selector the gate code should run in.

    Count - Must be 0 for entries in the IDT.

    Access - Stores various properties of the gate.
        Bit 7: Present. 1 if the gate is present, 0 if not present.
        Bits 6-5: DPL. Sets the ring number this handler executes in. Zero is
            the most privileged ring, 3 is least privileged.
        Bit 4: Reserved (set to 0).
        Bits 3-0: The gate type. Set to CALL_GATE_TYPE, INTERRUPT_GATE_TYPE,
            TASK_GATE_TYPE, or TRAP_GATE_TYPE.

    HighOffset - Stores the upper 16 bits of the interrupt handler's address.

--*/

typedef struct _PROCESSOR_GATE {
    USHORT LowOffset;
    USHORT Selector;
    BYTE Count;
    BYTE Access;
    USHORT HighOffset;
} PACKED PROCESSOR_GATE, *PPROCESSOR_GATE;

/*++

Structure Description:

    This structure defines the format of the GDTR, IDTR, or TR. This structure
    must be packed since it represents a hardware construct.

Members:

    Limit - Stores the last valid byte of the table, essentially size - 1.

    Base - Stores a pointer to the Global Descriptor Table, Interrupt
        Descriptor Table, or Task Table.

--*/

typedef struct _TABLE_REGISTER {
    USHORT Limit;
    ULONG Base;
} PACKED TABLE_REGISTER, *PTABLE_REGISTER;

/*++

Structure Description:

    This structure defines the x86 Task State Segment. It represents a complete
    task state as understood by the hardware.

Members:

    BackLink - Stores a pointer to the previous executing task. This value is
        written by the processor.

    Esp0-2 - Stores the stack pointer to load for each of the privilege levels.

    Ss0-2 - Stores the stack segment to load for each of the privilege levels.

    Pad0-9 - Stores padding in the structure. The processor does not use these
        fields, but they should not be modified.

    Cr3 - Stores the value of CR3 used by the task.

    Eip - Stores the currently executing instruction pointer.

    Eflags through Edi - Stores the state of the general registers when this
        task was last run.

    Es through Gs - Stores the state of the segment registers when this task
        was last run.

    LdtSelector - Stores the selector of the Local Descriptor Table when this
        task was last run.

    DebugTrap - Stores information only relevant when doing on-chip debugging.

    IoMapBase - Stores the 16 bit offset from the TSS base to the 8192 byte I/O
        Bitmap.

--*/

typedef struct _TSS {
    ULONG BackLink;
    ULONG Esp0;
    USHORT Ss0;
    USHORT Pad0;
    ULONG Esp1;
    USHORT Ss1;
    USHORT Pad1;
    ULONG Esp2;
    USHORT Ss2;
    USHORT Pad2;
    ULONG Cr3;
    ULONG Eip;
    ULONG Eflags;
    ULONG Eax;
    ULONG Ecx;
    ULONG Edx;
    ULONG Ebx;
    ULONG Esp;
    ULONG Ebp;
    ULONG Esi;
    ULONG Edi;
    USHORT Es;
    USHORT Pad3;
    USHORT Cs;
    USHORT Pad4;
    USHORT Ss;
    USHORT Pad5;
    USHORT Ds;
    USHORT Pad6;
    USHORT Fs;
    USHORT Pad7;
    USHORT Gs;
    USHORT Pad8;
    USHORT LdtSelector;
    USHORT Pad9;
    USHORT DebugTrap;
    USHORT IoMapBase;
} PACKED TSS, *PTSS;

/*++

Structure Description:

    This structure define a Global Descriptor Table entry. The GDT table sets
    up the segmentation features of the processor and privilege levels.

Members:

    LimitLow - Stores the lower 16 bits of the descriptor limit.

    BaseLow - Stores the lower 16 bits of the descriptor base.

    BaseMiddle - Stores the next 8 bits of the base.

    Access - Stores the access flags. The access byte has the following format:

             |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
             |     |           |     |                       |
             |  P  |    DPL    |  S  |         Type          |

             P - Is segment present (1 = Yes)

             DPL - Descriptor privilege level: Ring 0-3. Zero is the highest
                   privilege, 3 is the lowest (least privileged).

             S - System flag. Set to 0 if it's a system segment, or 1 if it's a
                 code/data segment.

             Type - Segment type: code segment / data segment. The Type field
                has the following definition:

                Bit 3 - Set to 1 for Code, or 0 for Data.

                Bit 2 - Expansion direction. Set to 0 for expand-up, or 1 for
                    expand-down.

                Bit 1 - Write-Enable. Set to 0 for Read Only, or 1 for
                    Read/Write.

                Bit 0 - Accessed. This bit is set by the processor when memory
                    in this segment is accessed. It is never cleared by
                    hardware.

    Granularity - Stores the granularity for the descriptor. The granularity
        byte has the following format:

             |  7  |  6  |  5  |  4  |  3  |  2  |  1  |  0  |
             |     |     |     |     |                       |
             |  G  |  D  |  L  |  A  | Segment length 19:16  |

             G - Granularity. 0 = 1 byte, 1 = 1 KByte.

             D - Operand Size. 0 = 16/64 bit, 1 = 32 bit.

             L - Long mode (64 bit).

             A - Available for system use (always zero).

    BaseHigh - Stores the high 8 bits of the base address.

--*/

typedef struct _GDT_ENTRY {
    USHORT LimitLow;
    USHORT BaseLow;
    UCHAR BaseMiddle;
    UCHAR Access;
    UCHAR Granularity;
    UCHAR BaseHigh;
} PACKED GDT_ENTRY, *PGDT_ENTRY;

/*++

Structure Description:

    This structure defines the format of an entry in a page table or directory.

Members:

    Present - Stores whether or not the page is present in memory.

    Writable - Stores whether or not this page is read-only (0) or writable (1).

    User - Stores whether or not this page is accessible by user mode (1) or
        only by kernel mode (0).

    WriteThrough - Stores whether or not write-through caching is enabled (1)
        or write-back caching (0).

    CacheDisabled - Stores whether or not to use caching. If this bit is set,
        the page will not be cached.

    Accessed - Stores whether or not the page has been accessed. This bit is
        set automatically by the processor, but will never be cleared by the
        processor.

    Dirty - Stores whether or not this page has been written to. This bit is
        set automatically by the processor, but must be cleared by software.

    LargePage - Stores whether or not large 4MB pages are in use (1) or 4kB
        pages (0).

    Global - Stores whether or not the TLB should avoid flushing this address
        if CR3 is changed. If this bit is set, then the TLB entry for this page
        will not be invalidated when CR3 is reset.

    Unused - These bits are unused by both the processor and the OS.

    Entry - Stores a pointer to the 4kB aligned page.

--*/

typedef struct _PTE {
    ULONG Present:1;
    ULONG Writable:1;
    ULONG User:1;
    ULONG WriteThrough:1;
    ULONG CacheDisabled:1;
    ULONG Accessed:1;
    ULONG Dirty:1;
    ULONG LargePage:1;
    ULONG Global:1;
    ULONG Unused:3;
    ULONG Entry:20;
} PACKED PTE, *PPTE;

/*++

Structure Description:

    This structure defines the extended state of the x86 architecture. This
    structure is architecturally defined by the FXSAVE and FXRSTOR instructions.

Members:

    Registers - Stores the extended processor state.

--*/

struct _FPU_CONTEXT {
    USHORT Fcw;
    USHORT Fsw;
    USHORT Ftw;
    USHORT Fop;
    ULONG FpuIp;
    USHORT Cs;
    USHORT Reserved1;
    ULONG FpuDp;
    USHORT Ds;
    USHORT Reserved2;
    ULONG Mxcsr;
    ULONG MxcsrMask;
    UCHAR St0Mm0[16];
    UCHAR St1Mm1[16];
    UCHAR St2Mm2[16];
    UCHAR St3Mm3[16];
    UCHAR St4Mm4[16];
    UCHAR St5Mm5[16];
    UCHAR St6Mm6[16];
    UCHAR St7Mm7[16];
    UCHAR Xmm0[16];
    UCHAR Xmm1[16];
    UCHAR Xmm2[16];
    UCHAR Xmm3[16];
    UCHAR Xmm4[16];
    UCHAR Xmm5[16];
    UCHAR Xmm6[16];
    UCHAR Xmm7[16];
    UCHAR Xmm8[16];
    UCHAR Xmm9[16];
    UCHAR Xmm10[16];
    UCHAR Xmm11[16];
    UCHAR Xmm12[16];
    UCHAR Xmm13[16];
    UCHAR Xmm14[16];
    UCHAR Xmm15[16];
    UCHAR Padding[96];
} PACKED ALIGNED64;

/*++

Structure Description:

    This structure outlines a trap frame that will be generated during most
    interrupts and exceptions.

Members:

    Registers - Stores the current state of the machine's registers. These
        values will be restored upon completion of the interrupt or exception.

--*/

struct _TRAP_FRAME {
    ULONG Ds;
    ULONG Es;
    ULONG Fs;
    ULONG Gs;
    ULONG Ss;
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
    ULONG Esi;
    ULONG Edi;
    ULONG Ebp;
    ULONG ErrorCode;
    ULONG Eip;
    ULONG Cs;
    ULONG Eflags;
    ULONG Esp;
} PACKED;

/*++

Structure Description:

    This structure outlines the register state saved by the kernel when a
    user mode signal is dispatched. This generally contains 1) control
    registers which are clobbered by switching to the signal handler, and
    2) volatile registers.

Members:

    Common - Stores the common signal context information.

    TrapFrame - Stores the general register state.

    FpuContext - Stores the FPU state.

--*/

typedef struct _SIGNAL_CONTEXT_X86 {
    SIGNAL_CONTEXT Common;
    TRAP_FRAME TrapFrame;
    FPU_CONTEXT FpuContext;
} PACKED SIGNAL_CONTEXT_X86, *PSIGNAL_CONTEXT_X86;

/*++

Structure Description:

    This structure contains the state of the processor, including both the
    non-volatile general registers and the system registers configured by the
    kernel. This structure is used in a manner similar to the C library
    setjmp/longjmp routines, the save context function appears to return
    twice. It returns once after the saving is complete, and then again with
    a different return value after restoring. Be careful when modifying this
    structure, as its offsets are used directly in assembly by the save/restore
    routines.

Members:

    Eax - Stores the value to return when restoring.

    Eip - Stores the instruction pointer to jump back to on restore. By default
        this is initialized to the return from whoever called save.

    Cs - Stores the code segment.

    Eflags - Stores the eflags register.

    Ebx - Stores a non-volatile general register.

    Esi - Stores a non-volatile general register.

    Edi - Stores a non-volatile general register.

    Ebp - Stores a non-volatile general register.

    Esp - Stores the stack pointer. This should be restored after the final
        page tables are in place to avoid NMIs having an invalid stack.

    Dr7 - Stores a debug register. This should be restored last of the debug
        registers.

    Dr6 - Stores a debug register.

    Dr0 - Stores a debug register.

    Dr1 - Stores a debug register.

    Dr2 - Stores a debug register.

    Dr3 - Stores a debug register.

    VirtualAddress - Stores the virtual address of this structure member, which
        is used in case the restore of CR0 that just happened enabled paging
        suddenly.

    Cr0 - Stores the CR0 control register value.

    Cr2 - Stores the CR2 control register value (faulting address).

    Cr3 - Stores the CR3 control register value (top level page directory).

    Cr4 - Stores the CR4 control register value.

    Tr - Stores the task register (must be restored after the GDT).

    Idt - Stores the interrupt descriptor table. The stack should be restored
        before this because once this is restored NMIs could come in and use
        stack (rather than the stub function they may currently be on).

    Gdt - Stores the global descriptor table.

--*/

struct _PROCESSOR_CONTEXT {
    ULONG Eax;
    ULONG Eip;
    ULONG Cs;
    ULONG Eflags;
    ULONG Ebx;
    ULONG Esi;
    ULONG Edi;
    ULONG Ebp;
    ULONG Esp;
    ULONG Dr7;
    ULONG Dr6;
    ULONG Dr0;
    ULONG Dr1;
    ULONG Dr2;
    ULONG Dr3;
    ULONG VirtualAddress;
    ULONG Cr0;
    ULONG Cr2;
    ULONG Cr3;
    ULONG Cr4;
    ULONG Tr;
    TABLE_REGISTER Idt;
    TABLE_REGISTER Gdt;
} PACKED;

typedef
VOID
(*PAR_SAVE_RESTORE_FPU_CONTEXT) (
    PFPU_CONTEXT Buffer
    );

/*++

Routine Description:

    This routine saves or restores floating point context from the processor.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        saved to or loaded from. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the architecture specific form of an address space
    structure.

Members:

    Common - Stores the common address space information.

    PageDirectory - Stores the virtual address of the top level page directory.

    PageDirectoryPhysical - Stores the physical address of the top level page
        directory.

    PageTableCount - Stores the number of page tables that were allocated on
        behalf of this process (user mode only).

--*/

typedef struct _ADDRESS_SPACE_X86 {
    ADDRESS_SPACE Common;
    PPTE PageDirectory;
    ULONG PageDirectoryPhysical;
    ULONG PageTableCount;
} ADDRESS_SPACE_X86, *PADDRESS_SPACE_X86;

//
// -------------------------------------------------------------------- Globals
//

//
// Store pointers to functions used to save and restore floating point state.
//

extern PAR_SAVE_RESTORE_FPU_CONTEXT ArSaveFpuState;
extern PAR_SAVE_RESTORE_FPU_CONTEXT ArRestoreFpuState;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
ArLoadKernelDataSegments (
    VOID
    );

/*++

Routine Description:

    This routine switches the data segments DS and ES to the kernel data
    segment selectors.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArLoadTr (
    USHORT TssSegment
    );

/*++

Routine Description:

    This routine loads a TSS (Task Selector State).

Arguments:

    TssSegment - Supplies the segment selector in the GDT that describes the
        TSS.

Return Value:

    None.

--*/

VOID
ArStoreTr (
    PULONG TssSegment
    );

/*++

Routine Description:

    This routine retrieves the current TSS (Task Selector State) register.

Arguments:

    TssSegment - Supplies a pointer where the current TSS segment register will
        be returned.

Return Value:

    None.

--*/

VOID
ArLoadIdtr (
    PVOID IdtBase
    );

/*++

Routine Description:

    This routine loads the given Interrupt Descriptor Table.

Arguments:

    IdtBase - Supplies a pointer to the base of the IDT.

Return Value:

    None.

--*/

VOID
ArStoreIdtr (
    PTABLE_REGISTER IdtRegister
    );

/*++

Routine Description:

    This routine stores the interrupt descriptor table register into the given
    value.

Arguments:

    IdtRegister - Supplies a pointer that will receive the value.

Return Value:

    None.

--*/

VOID
ArLoadGdtr (
    TABLE_REGISTER Gdt
    );

/*++

Routine Description:

    This routine loads a global descriptor table.

Arguments:

    Gdt - Supplies a pointer to the Gdt pointer, which contains the base and
        limit for the GDT.

Return Value:

    None.

--*/

VOID
ArStoreGdtr (
    PTABLE_REGISTER GdtRegister
    );

/*++

Routine Description:

    This routine stores the GDT register into the given value.

Arguments:

    GdtRegister - Supplies a pointer that will receive the value.

Return Value:

    None.

--*/

PVOID
ArGetFaultingAddress (
    );

/*++

Routine Description:

    This routine determines which address caused a page fault.

Arguments:

    None.

Return Value:

    Returns the faulting address.

--*/

VOID
ArSetFaultingAddress (
    PVOID Value
    );

/*++

Routine Description:

    This routine sets the CR2 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

UINTN
ArGetCurrentPageDirectory (
    VOID
    );

/*++

Routine Description:

    This routine returns the active page directory.

Arguments:

    None.

Return Value:

    Returns the page directory currently in use by the system.

--*/

VOID
ArSetCurrentPageDirectory (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the CR3 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

VOID
ArDoubleFaultHandlerAsm (
    );

/*++

Routine Description:

    This routine is entered via an IDT entry when a double fault exception
    occurs. Double faults are non-recoverable. This machine loops attempting
    to enter the debugger indefinitely.

Arguments:

    None.

Return Value:

    None, this routine does not return.

--*/

VOID
ArProtectionFaultHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

/*++

Routine Description:

    This routine is called directly when a general protection fault occurs.
    It's job is to prepare the trap frame, call the appropriate handler, and
    then restore the trap frame.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

VOID
ArMathFaultHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

/*++

Routine Description:

    This routine is called directly when a x87 FPU fault occurs.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

VOID
ArTrapSystemCallHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

/*++

Routine Description:

    This routine is entered when the sysenter routine is entered with the TF
    flag set. It performs a normal save and sets the TF.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

INTN
ArSystemCallHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

/*++

Routine Description:

    This routine is entered via an IDT entry to service a user mode request.
    Ecx contains the system call number, and Edx contains the argument.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

INTN
ArSysenterHandlerAsm (
    VOID
    );

/*++

Routine Description:

    This routine is executed when user mode invokes the SYSENTER instruction.
    Upon entry, CS, EIP, and ESP are set to predefined values set in MSRs.

Arguments:

    None.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

VOID
ArCpuid (
    PULONG Eax,
    PULONG Ebx,
    PULONG Ecx,
    PULONG Edx
    );

/*++

Routine Description:

    This routine executes the CPUID instruction to get processor architecture
    information.

Arguments:

    Eax - Supplies a pointer to the value that EAX should be set to when the
        CPUID instruction is executed. On output, contains the contents of
        EAX immediately after the CPUID instruction.

    Ebx - Supplies a pointer to the value that EBX should be set to when the
        CPUID instruction is executed. On output, contains the contents of
        EAX immediately after the CPUID instruction.

    Ecx - Supplies a pointer to the value that ECX should be set to when the
        CPUID instruction is executed. On output, contains the contents of
        EAX immediately after the CPUID instruction.

    Edx - Supplies a pointer to the value that EDX should be set to when the
        CPUID instruction is executed. On output, contains the contents of
        EAX immediately after the CPUID instruction.

Return Value:

    None.

--*/

ULONG
ArGetControlRegister0 (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of CR0.

Arguments:

    None.

Return Value:

    Returns CR0.

--*/

VOID
ArSetControlRegister0 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the CR0 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetControlRegister4 (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of CR4.

Arguments:

    None.

Return Value:

    Returns CR4.

--*/

VOID
ArSetControlRegister4 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the CR4 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetDebugRegister0 (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of DR0.

Arguments:

    None.

Return Value:

    Returns DR0.

--*/

VOID
ArSetDebugRegister0 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the DR0 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetDebugRegister1 (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of DR1.

Arguments:

    None.

Return Value:

    Returns DR1.

--*/

VOID
ArSetDebugRegister1 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the DR1 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetDebugRegister2 (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of DR2.

Arguments:

    None.

Return Value:

    Returns DR2.

--*/

VOID
ArSetDebugRegister2 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the DR2 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetDebugRegister3 (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of DR3.

Arguments:

    None.

Return Value:

    Returns DR3.

--*/

VOID
ArSetDebugRegister3 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the DR3 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetDebugRegister6 (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of DR6.

Arguments:

    None.

Return Value:

    Returns DR6.

--*/

VOID
ArSetDebugRegister6 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the DR6 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetDebugRegister7 (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of DR7.

Arguments:

    None.

Return Value:

    Returns DR7.

--*/

VOID
ArSetDebugRegister7 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the DR7 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

VOID
ArFxSave (
    PFPU_CONTEXT Buffer
    );

/*++

Routine Description:

    This routine saves the current x87 FPU, MMX, XMM, and MXCSR registers to a
    512 byte memory location.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        saved. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

VOID
ArFxRestore (
    PFPU_CONTEXT Buffer
    );

/*++

Routine Description:

    This routine restores the current x87 FPU, MMX, XMM, and MXCSR registers
    from a 512 byte memory location.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        loaded from. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

VOID
ArSaveX87State (
    PFPU_CONTEXT Buffer
    );

/*++

Routine Description:

    This routine saves the current x87 FPU (floating point unit) state.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        saved. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

VOID
ArRestoreX87State (
    PFPU_CONTEXT Buffer
    );

/*++

Routine Description:

    This routine restores the x87 FPU (floating point unit) state.

Arguments:

    Buffer - Supplies a pointer to the buffer where the information will be
        loaded from. This buffer must be 16-byte aligned.

Return Value:

    None.

--*/

VOID
ArEnableFpu (
    VOID
    );

/*++

Routine Description:

    This routine clears the TS bit of CR0, allowing access to the FPU.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArDisableFpu (
    VOID
    );

/*++

Routine Description:

    This routine sets the TS bit of CR0, disallowing access to the FPU.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArInitializeFpu (
    VOID
    );

/*++

Routine Description:

    This routine resets the FPU state.

Arguments:

    None.

Return Value:

    None.

--*/

ULONGLONG
ArReadTimeStampCounter (
    VOID
    );

/*++

Routine Description:

    This routine reads the time stamp counter from the current processor. It
    is essential that callers of this function understand that this returns
    instruction cycles, which does not always translate directly into units
    of time. For example, some processors halt the timestamp counter during
    performance and CPU idle state transitions. In other cases, the timestamp
    counters of all processors are not in sync, so as execution of a thread
    bounces unpredictably from one core to another, different timelines may be
    observed. Additionally, one must understand that this intrinsic is not a
    serializing instruction to the hardware, so the processor may decide to
    execute any number of instructions after this one before actually snapping
    the timestamp counter. To all those who choose to continue to use this
    primitive to measure time, you have been warned.

Arguments:

    None.

Return Value:

    Returns the current instruction cycle count since the processor was started.

--*/

ULONGLONG
ArReadMsr (
    ULONG Msr
    );

/*++

Routine Description:

    This routine reads the requested Model Specific Register.

Arguments:

    Msr - Supplies the MSR to read.

Return Value:

    Returns the 64-bit MSR value.

--*/

VOID
ArWriteMsr (
    ULONG Msr,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine writes the requested Model Specific Register.

Arguments:

    Msr - Supplies the MSR to write.

    Value - Supplies the 64-bit value to write.

Return Value:

    None.

--*/

VOID
ArReloadThreadSegment (
    VOID
    );

/*++

Routine Description:

    This routine reloads the thread segment register.

Arguments:

    None.

Return Value:

    None.

--*/

KERNEL_API
VOID
ArMonitor (
    PVOID Address,
    UINTN Ecx,
    UINTN Edx
    );

/*++

Routine Description:

    This routine arms the monitoring hardware in preparation for an mwait
    instruction.

Arguments:

    Address - Supplies the address pointer to monitor.

    Ecx - Supplies the contents to load into the ECX (RCX in 64-bit) register
        when executing the monitor instruction. These are defined as hints.

    Edx - Supplies the contents to load into the EDX/RDX register. These are
        also hints.

Return Value:

    None.

--*/

KERNEL_API
VOID
ArMwait (
    UINTN Eax,
    UINTN Ecx
    );

/*++

Routine Description:

    This routine executes the mwait instruction, which is used to halt the
    processor until a specified memory location is written to. It is also used
    on Intel processors to enter C-states. A monitor instruction must have
    been executed prior to this to set up the monitoring region.

Arguments:

    Eax - Supplies the contents to load into EAX/RAX when executing the mwait
        instruction. This is a set of hints, including which C-state to enter
        on Intel processors.

    Ecx - Supplies the contents to load into the ECX (RCX in 64-bit) register
        when executing the mwait instruction. This is 1 when entering a C-state
        with interrupts disabled to indicate that an interrupt should still
        break out.

Return Value:

    None.

--*/

KERNEL_API
VOID
ArIoReadAndHalt (
    USHORT IoPort
    );

/*++

Routine Description:

    This routine performs a single 8-bit I/O port read and then halts the
    processor until the next interrupt comes in. This routine should be called
    with interrupts disabled, and will return with interrupts enabled.

Arguments:

    IoPort - Supplies the I/O port to read from.

Return Value:

    None.

--*/

VOID
ArGetKernelTssTrapFrame (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine converts the kernel TSS to a trap frame.

Arguments:

    TrapFrame - Supplies a pointer where the filled out trap frame information
        will be returned.

Return Value:

    None.

--*/

VOID
ArSetKernelTssTrapFrame (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine converts writes the given trap frame into the kernel TSS.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame data to write.

Return Value:

    None.

--*/

VOID
ArClearTssBusyBit (
    USHORT TssSegment
    );

/*++

Routine Description:

    This routine clears the busy bit in the GDT for the given segment. It is
    assumed this segment is used on the current processor.

Arguments:

    TssSegment - Supplies the TSS segment for the busy bit to clear.

Return Value:

    None.

--*/

VOID
ArpPageFaultHandlerAsm (
    ULONG ReturnEip,
    ULONG ReturnCodeSelector,
    ULONG ReturnEflags
    );

/*++

Routine Description:

    This routine is called directly when a page fault occurs.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the
        fault.

    ReturnCodeSelector - Supplies the code selector the code that faulted was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the fault.

Return Value:

    None.

--*/

VOID
ArpCreateSegmentDescriptor (
    PGDT_ENTRY GdtEntry,
    PVOID Base,
    ULONG Limit,
    UCHAR Granularity,
    UCHAR Access
    );

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

Return Value:

    None.

--*/

