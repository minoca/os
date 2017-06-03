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

#define X64_PTE_COUNT 512

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

#define X64_CANONICAL_HIGH 0xFFF8000000000000
#define X64_CANONICAL_LOW  0x0007FFFFFFFFFFFF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef ULONGLONG PTE, *PPTE;

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
    ULONG ErrorCode;
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
    ULONGLONG Rip;
    ULONG Cs;
    ULONGLONG Rflags;
    ULONGLONG Rsp;
} PACKED;

//
// -------------------------------------------------------------------- Globals
//

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
    PULONGLONG TssSegment
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

ULONGLONG
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
    ULONGLONG Value
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

ULONGLONG
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
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the CR0 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONGLONG
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
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the CR4 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONGLONG
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
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the DR0 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONGLONG
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
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the DR1 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONGLONG
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
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the DR2 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONGLONG
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
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the DR3 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONGLONG
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
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the DR6 register.

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONGLONG
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
    ULONGLONG Value
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

