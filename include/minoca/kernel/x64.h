/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    x64.h

Abstract:

    This header contains definitions for aspects of the system that are specific
    to the AMD64 architecture.

Author:

    Evan Green 26-May-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x86defs.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros get the various page table components of an address. The
// levels, from top (highest address bits) to bottom are Page Map Level 4
// (PML4), Page Directory Pointer (PDP), Page Directory (PD), and Page Table
// (PT).
//

#define X64_PML4_INDEX(_VirtualAddress) \
    (((UINTN)(_VirtualAddress) >> X64_PML4E_SHIFT) & X64_PT_MASK)

#define X64_PDP_INDEX(_VirtualAddress) \
    (((UINTN)(_VirtualAddress) >> X64_PDPE_SHIFT) & X64_PT_MASK)

#define X64_PD_INDEX(_VirtualAddress) \
    (((UINTN)(_VirtualAddress) >> X64_PDE_SHIFT) & X64_PT_MASK)

#define X64_PT_INDEX(_VirtualAddress) \
    (((UINTN)(_VirtualAddress) >> X64_PTE_SHIFT) & X64_PT_MASK)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the nesting level of page tables.
//

#define X64_PAGE_LEVEL 4

//
// Define the number of entries in a page table, directory, directory pointer,
// and level 4 table.
//

#define X64_PTE_COUNT 512ULL

//
// Define page address masks.
//

#define X64_PTE_BITS 9
#define X64_PT_MASK 0x1FFULL
#define X64_PTE_SHIFT 12
#define X64_PTE_MASK (X64_PT_MASK << X64_PTE_SHIFT)
#define X64_PDE_SHIFT 21
#define X64_PDE_MASK (X64_PT_MASK << X64_PDE_SHIFT)
#define X64_PDPE_SHIFT 30
#define X64_PDPE_MASK (X64_PT_MASK << X64_PDPE_SHIFT)
#define X64_PML4E_SHIFT 39
#define X64_PML4E_MASK (X64_PT_MASK << X64_PML4E_SHIFT)

//
// Define the fixed self map address. This is set up by the boot loader and
// used directly by the kernel. The advantage is it's a compile-time constant
// to get to page tables. The disadvantage is that VA can't be used by anything
// else. But given that there's no physical memory up there and there's oodles
// of VA space, that seems fine.
//
// Don't use the last index as that would put the PML4T at 0xFFFFFFFFFFFFF000.
// Any kernel underflows from null would hit that page and be awful to debug.
// Use the second to last index.
//

#define X64_SELF_MAP_INDEX (X64_PTE_COUNT - 2)

#define X64_CANONICAL_HIGH 0xFFFF800000000000
#define X64_CANONICAL_LOW  0x00007FFFFFFFFFFF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef ULONGLONG PTE, *PPTE;

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

    This structure defines the format of a task, interrupt, or call gate
    descriptor. This structure must not be padded, since the hardware relies on
    this exact format.

Members:

    LowOffset - Stores the lower 16 bits of the gate's destination address.

    Selector - Stores the code segment selector the gate code should run in.

    Ist - Stores the interrupt stack table index specifying the stack to use
        when taking this interrupt or trap. Set to 0 to not switch stacks.

    Access - Stores various properties of the gate.
        Bit 7: Present. 1 if the gate is present, 0 if not present.
        Bits 6-5: DPL. Sets the ring number this handler executes in. Zero is
            the most privileged ring, 3 is least privileged.
        Bit 4: Reserved (set to 0).
        Bits 3-0: The gate type. Set to CALL_GATE_TYPE, INTERRUPT_GATE_TYPE,
            TASK_GATE_TYPE, or TRAP_GATE_TYPE.

    MidOffset - Stores bits 16-31 of the handler's address.

    HighWord - Stores bits 32-63 of the handler's address.

    Reserved - Stores a reserved word that should be zero.

--*/

typedef struct _PROCESSOR_GATE {
    USHORT LowOffset;
    USHORT Selector;
    BYTE Ist;
    BYTE Access;
    USHORT MidOffset;
    ULONG HighWord;
    ULONG Reserved;
} PACKED PROCESSOR_GATE, *PPROCESSOR_GATE;

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

    This structure define a 64-bit global descriptor table entry. These are
    used by the TSS and LDT types.

Members:

    LimitLow - Stores the lower 16 bits of the descriptor limit.

    BaseLow - Stores the lower 16 bits of the descriptor base.

    BaseMiddle - Stores the next 8 bits of the base.

    Access - Stores the access flags. See GATE_ACCESS_* definitions.

    Granularity - Stores the granularity bits for the descriptor. See
        GDT_GRANULARITY_* definitions.

    BaseHigh - Stores bits 24-31 of the base address.

    BaseHigh32 - Stores bits 32-63 of the base address.

    Reserved - Stores a reserved value that must be zero.

--*/

typedef struct _GDT64_ENTRY {
    USHORT LimitLow;
    USHORT BaseLow;
    UCHAR BaseMiddle;
    UCHAR Access;
    UCHAR Granularity;
    UCHAR BaseHigh;
    ULONG BaseHigh32;
    ULONG Reserved;
} PACKED GDT64_ENTRY, *PGDT64_ENTRY;

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
    ULONGLONG Base;
} PACKED TABLE_REGISTER, *PTABLE_REGISTER;

/*++

Structure Description:

    This structure defines the x86 Task State Segment. It represents a complete
    task state as understood by the hardware.

Members:

    Rsp0-2 - Stores the stack pointer to load for each of the privilege levels.

    Ist - Stores the interrupt stack values for RSP, indices 0-7. Technically
        in the spec Ist[0] is marked as reserved, since IST index zero means
        "no stack switching".

    IoMapBase - Stores the 16 bit offset from the TSS base to the 8192 byte I/O
        Bitmap.

--*/

typedef struct _TSS64 {
    ULONG Reserved0;
    ULONGLONG Rsp[3];
    ULONGLONG Ist[8];
    ULONGLONG Reserved2;
    USHORT Reserved3;
    USHORT IoMapBase;
} PACKED TSS64, *PTSS64;

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
    ULONGLONG Padding;
    ULONGLONG Rax;
    ULONGLONG Rbx;
    ULONGLONG Rcx;
    ULONGLONG Rdx;
    ULONGLONG Rsi;
    ULONGLONG Rdi;
    ULONGLONG Rbp;
    ULONGLONG R8;
    ULONGLONG R9;
    ULONGLONG R10;
    ULONGLONG R11;
    ULONGLONG R12;
    ULONGLONG R13;
    ULONGLONG R14;
    ULONGLONG R15;
    ULONGLONG ErrorCode;
    ULONGLONG Rip;
    ULONGLONG Cs;
    ULONGLONG Rflags;
    ULONGLONG Rsp;
    ULONGLONG Ss;
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

typedef struct _SIGNAL_CONTEXT_X64 {
    SIGNAL_CONTEXT Common;
    TRAP_FRAME TrapFrame;
    FPU_CONTEXT FpuContext;
} PACKED SIGNAL_CONTEXT_X64, *PSIGNAL_CONTEXT_X64;

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

    Rax - Stores the value to return when restoring.

    Rip - Stores the instruction pointer to jump back to on restore. By default
        this is initialized to the return from whoever called save.

    Cs - Stores the code segment.

    Rflags - Stores the eflags register.

    Rbx - Stores a non-volatile general register.

    Rbp - Stores a non-volatile general register.

    Rsp - Stores the stack pointer. This should be restored after the final
        page tables are in place to avoid NMIs having an invalid stack.

    R12 - Stores a non-volatile general register.

    R13 - Stores a non-volatile general register.

    R14 - Stores a non-volatile general register.

    R15 - Stores a non-volatile general register.

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

    Fsbase - Stores the FS: base address.

    Gsbase - Stores the GS: base address.

    Tr - Stores the task register (must be restored after the GDT).

    Idt - Stores the interrupt descriptor table. The task register and GDT
        should be restored before this so that the ISTs are set up if an NMI
        comes in.

    Gdt - Stores the global descriptor table.

--*/

struct _PROCESSOR_CONTEXT {
    UINTN Rax;
    UINTN Rip;
    UINTN Cs;
    UINTN Rflags;
    UINTN Rbx;
    UINTN Rbp;
    UINTN Rsp;
    UINTN R12;
    UINTN R13;
    UINTN R14;
    UINTN R15;
    UINTN Dr7;
    UINTN Dr6;
    UINTN Dr0;
    UINTN Dr1;
    UINTN Dr2;
    UINTN Dr3;
    UINTN VirtualAddress;
    UINTN Cr0;
    UINTN Cr2;
    UINTN Cr3;
    UINTN Cr4;
    UINTN Fsbase;
    UINTN Gsbase;
    UINTN Tr;
    TABLE_REGISTER Idt;
    TABLE_REGISTER Gdt;
} PACKED;

/*++

Structure Description:

    This structure defines the architecture specific form of an address space
    structure.

Members:

    Common - Stores the common address space information.

    Pml4Physical - Stores the physical address of the top level page
        directory.

    AllocatedPageTables - Stores the total number of active and inactive page
        table pages allocated on behalf of user mode for this process.

    ActivePageTables - Stores the number of page table pages that are in
        service for user mode of this process.

--*/

typedef struct _ADDRESS_SPACE_X64 {
    ADDRESS_SPACE Common;
    PHYSICAL_ADDRESS Pml4Physical;
    UINTN AllocatedPageTables;
    UINTN ActivePageTables;
} ADDRESS_SPACE_X64, *PADDRESS_SPACE_X64;

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
    PTABLE_REGISTER Gdt
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
    VOID
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
    UINTN Value
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

UINTN
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
    UINTN Value
    );

/*++

Routine Description:

    This routine sets the CR0 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

UINTN
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
    UINTN Value
    );

/*++

Routine Description:

    This routine sets the CR4 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

UINTN
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
    UINTN Value
    );

/*++

Routine Description:

    This routine sets the DR0 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

UINTN
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
    UINTN Value
    );

/*++

Routine Description:

    This routine sets the DR1 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

UINTN
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
    UINTN Value
    );

/*++

Routine Description:

    This routine sets the DR2 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

UINTN
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
    UINTN Value
    );

/*++

Routine Description:

    This routine sets the DR3 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

UINTN
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
    UINTN Value
    );

/*++

Routine Description:

    This routine sets the DR6 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

UINTN
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
    UINTN Value
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
    ULONGLONG Msr,
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

PVOID
ArReadFsbase (
    VOID
    );

/*++

Routine Description:

    This routine reads the fs: base register.

Arguments:

    None.

Return Value:

    Returns the fsbase pointer.

--*/

VOID
ArWriteFsbase (
    PVOID Fsbase
    );

/*++

Routine Description:

    This routine writes the fs: base register.

Arguments:

    Fsbase - Supplies the new fsbase value to write.

Return Value:

    None.

--*/

PVOID
ArReadGsbase (
    VOID
    );

/*++

Routine Description:

    This routine reads the gs: base register.

Arguments:

    None.

Return Value:

    Returns the gsbase pointer.

--*/

VOID
ArWriteGsbase (
    PVOID Gsbase
    );

/*++

Routine Description:

    This routine writes the gs: base register.

Arguments:

    Gsbase - Supplies the new gsbase value to write.

Return Value:

    None.

--*/

VOID
ArSwapGs (
    VOID
    );

/*++

Routine Description:

    This routine exchanges the GS base hidden register with the kernel GS base
    MSR.

Arguments:

    None.

Return Value:

    None.

--*/

KERNEL_API
VOID
ArMonitor (
    PVOID Address,
    UINTN Rcx,
    UINTN Rdx
    );

/*++

Routine Description:

    This routine arms the monitoring hardware in preparation for an mwait
    instruction.

Arguments:

    Address - Supplies the address pointer to monitor.

    Rcx - Supplies the contents to load into the RCX register when executing
        the monitor instruction. These are defined as hints.

    Rdx - Supplies the contents to load into the RDX register. These are also
        hints.

Return Value:

    None.

--*/

KERNEL_API
VOID
ArMwait (
    UINTN Rax,
    UINTN Rcx
    );

/*++

Routine Description:

    This routine executes the mwait instruction, which is used to halt the
    processor until a specified memory location is written to. It is also used
    on Intel processors to enter C-states. A monitor instruction must have
    been executed prior to this to set up the monitoring region.

Arguments:

    Rax - Supplies the contents to load into RAX when executing the mwait
        instruction. This is a set of hints, including which C-state to enter
        on Intel processors.

    Rcx - Supplies the contents to load into the RCX register when executing
        the mwait instruction. This is 1 when entering a C-state with
        interrupts disabled to indicate that an interrupt should still break
        out.

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

