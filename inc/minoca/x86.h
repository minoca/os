/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    x86.h

Abstract:

    This header contains definitions for aspects of the system that are specific
    to the x86 architecture.

Author:

    Evan Green 3-Jul-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define TASK_GATE_TYPE         0x05
#define CALL_GATE_TYPE         0x0C
#define INTERRUPT_GATE_TYPE    0x0E
#define TRAP_GATE_TYPE         0x0F

#define SEGMENT_PRIVILEGE_MASK      0x0003
#define SEGMENT_PRIVILEGE_KERNEL    0x0000
#define SEGMENT_PRIVILEGE_USER      0x0003

#define KERNEL_CS           0x08
#define KERNEL_DS           0x10
#define USER_CS             (0x18 | SEGMENT_PRIVILEGE_USER)
#define USER_DS             (0x20 | SEGMENT_PRIVILEGE_USER)
#define GDT_PROCESSOR       0x28
#define GDT_THREAD          (0x30 | SEGMENT_PRIVILEGE_USER)
#define KERNEL_TSS          0x38
#define DOUBLE_FAULT_TSS    0x40
#define NMI_TSS             0x48
#define GDT_ENTRIES         10

#define DEFAULT_GDT_ACCESS      0x80
#define DEFAULT_GDT_GRANULARITY 0x40
#define MAX_GDT_LIMIT           0xFFFFF
#define GDT_SYSTEM_SEGMENT      0x00
#define GDT_CODE_DATA_SEGMENT   0x10

#define IDT_SIZE 0x100
#define VECTOR_DIVIDE_ERROR         0x00
#define VECTOR_DEBUG                0x01
#define VECTOR_NMI                  0x02
#define VECTOR_BREAKPOINT           0x03
#define VECTOR_OVERFLOW             0x04
#define VECTOR_BOUND                0x05
#define VECTOR_INVALID_OPCODE       0x06
#define VECTOR_DEVICE_NOT_AVAILABLE 0x07
#define VECTOR_DOUBLE_FAULT         0x08
#define VECTOR_SEGMENT_OVERRUN      0x09
#define VECTOR_INVALID_TSS          0x0A
#define VECTOR_INVALID_SEGMENT      0x0B
#define VECTOR_STACK_EXCEPTION      0x0C
#define VECTOR_PROTECTION_FAULT     0x0D
#define VECTOR_PAGE_FAULT           0x0E
#define VECTOR_MATH_FAULT           0x10
#define VECTOR_ALIGNMENT_CHECK      0x11
#define VECTOR_MACHINE_CHECK        0x12
#define VECTOR_SIMD_EXCEPTION       0x13
#define VECTOR_DEBUG_SERVICE        0x21
#define VECTOR_SYSTEM_CALL          0x2F
#define VECTOR_CLOCK_INTERRUPT      0xD0
#define VECTOR_IPI_INTERRUPT        0xE0
#define VECTOR_TLB_IPI              0xE1
#define VECTOR_PROFILER_INTERRUPT   0xF0

#define PROCESSOR_VECTOR_COUNT 0x20
#define MINIMUM_VECTOR 0x30
#define MIDPOINT_VECTOR 0x80
#define MAXIMUM_VECTOR 0xFF
#define MAXIMUM_DEVICE_VECTOR 0xBF
#define INTERRUPT_VECTOR_COUNT IDT_SIZE
#define IO_PORT_COUNT 0x10000

#define IA32_EFLAG_CF 0x00000001
#define IA32_EFLAG_PF 0x00000004
#define IA32_EFLAG_AF 0x00000010
#define IA32_EFLAG_ZF 0x00000040
#define IA32_EFLAG_SF 0x00000080
#define IA32_EFLAG_TF 0x00000100
#define IA32_EFLAG_IF 0x00000200
#define IA32_EFLAG_DF 0x00000400
#define IA32_EFLAG_OF 0x00000800
#define IA32_EFLAG_IOPL_MASK 0x00003000
#define IA32_EFLAG_IOPL_USER 0x00003000
#define IA32_EFLAG_IOPL_SHIFT 12
#define IA32_EFLAG_NT 0x00004000
#define IA32_EFLAG_RF 0x00010000
#define IA32_EFLAG_VM 0x00020000
#define IA32_EFLAG_AC 0x00040000
#define IA32_EFLAG_VIF 0x00080000
#define IA32_EFLAG_VIP 0x00100000
#define IA32_EFLAG_ID 0x00200000
#define IA32_EFLAG_ALWAYS_0 0xFFC08028
#define IA32_EFLAG_ALWAYS_1 0x00000002
#define CR0_PAGING_ENABLE 0x80000000
#define CR0_WRITE_PROTECT_ENABLE 0x00010000
#define CR0_TASK_SWITCHED 0x00000008

#define CR4_OS_XMM_EXCEPTIONS 0x00000400
#define CR4_OS_FX_SAVE_RESTORE 0x00000200
#define CR4_PAGE_GLOBAL_ENABLE 0x00000080

#define PAGE_SIZE 4096
#define PAGE_MASK 0x00000FFF
#define PAGE_SHIFT 12
#define PAGE_DIRECTORY_SHIFT 22
#define PDE_INDEX_MASK 0xFFC00000
#define PTE_INDEX_MASK 0x003FF000

#define X86_FAULT_FLAG_PROTECTION_VIOLATION 0x00000001
#define X86_FAULT_ERROR_CODE_WRITE          0x00000002

//
// Define the location of the legacy keyboard controller. While not strictly
// architectural, it's pretty close.
//

#define PC_8042_CONTROL_PORT        0x64
#define PC_8042_RESET_VALUE         0xFE
#define PC_8042_INPUT_BUFFER_FULL   0x02

//
// Define CPUID EAX values.
//

#define X86_CPUID_IDENTIFICATION 0x00000000
#define X86_CPUID_BASIC_INFORMATION 0x00000001
#define X86_CPUID_EXTENDED_IDENTIFICATION 0x80000000
#define X86_CPUID_EXTENDED_INFORMATION 0x80000001
#define X86_CPUID_ADVANCED_POWER_MANAGEMENT 0x80000007

//
// Define basic information CPUID bits (eax is 1).
//

#define X86_CPUID_BASIC_EDX_SYSENTER (1 << 11)
#define X86_CPUID_BASIC_EDX_CMOV (1 << 15)
#define X86_CPUID_BASIC_EDX_FX_SAVE_RESTORE (1 << 24)

//
// Define extended information CPUID bits (eax is 0x80000001).
//

#define X86_CPUID_EXTENDED_INFORMATION_EDX_SYSCALL (1 << 11)

//
// Define advanced power management CPUID bits (eax 0x80000007).
//

//
// This bit is set to indicate that the TSC is invariant across all P-states
// and C-states
//

#define X86_CPUID_ADVANCED_POWER_EDX_TSC_INVARIANT (1 << 8)

//
// Define the required alignment for FPU context.
//

#define FPU_CONTEXT_ALIGNMENT 16

//
// Define MSR values.
//

#define X86_MSR_SYSENTER_CS     0x00000174
#define X86_MSR_SYSENTER_ESP    0x00000175
#define X86_MSR_SYSENTER_EIP    0x00000176
#define X86_MSR_STAR            0xC0000081
#define X86_MSR_LSTAR           0xC0000082
#define X86_MSR_FMASK           0xC0000084

//
// Define the PTE bits.

#define PTE_FLAG_PRESENT        0x00000001
#define PTE_FLAG_WRITABLE       0x00000002
#define PTE_FLAG_USER_MODE      0x00000004
#define PTE_FLAG_WRITE_THROUGH  0x00000008
#define PTE_FLAG_CACHE_DISABLED 0x00000010
#define PTE_FLAG_ACCESSED       0x00000020
#define PTE_FLAG_DIRTY          0x00000040
#define PTE_FLAG_LARGE_PAGE     0x00000080
#define PTE_FLAG_GLOBAL         0x00000100
#define PTE_FLAG_ENTRY_MASK     0xFFFFF000
#define PTE_FLAG_ENTRY_SHIFT    12

//
// --------------------------------------------------------------------- Macros
//

//
// This macro gets a value at the given offset from the current processor block.
// _Result should be a ULONG.
//

#define GET_PROCESSOR_BLOCK_OFFSET(_Result, _Offset) \
    asm volatile ("mov %%fs:(%1), %0" : "=r" (_Result) : "r" (_Offset))

//
// This macro determines whether or not the given trap frame is from privileged
// mode.
//

#define IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(_TrapFrame) \
    (((_TrapFrame)->Cs & SEGMENT_PRIVILEGE_MASK) == 0)

//
// ------------------------------------------------------ Data Type Definitions
//

#pragma pack(push, 1)

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
} PROCESSOR_GATE, *PPROCESSOR_GATE;

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
} TABLE_REGISTER, *PTABLE_REGISTER;

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
} TSS, *PTSS;

typedef enum _GDT_GRANULARITY {
    GdtByteGranularity = 0x00,
    GdtKilobyteGranularity = 0x80
} GDT_GRANULARITY, *PGDT_GRANULARITY;

typedef enum _GDT_SEGMENT_TYPE {
    GdtDataReadOnly = 0x0,
    GdtDataReadWrite = 0x2,
    GdtCodeExecuteOnly = 0x8,
    Gdt32BitTss = 0x9
} GDT_SEGMENT_TYPE, *PGDT_SEGMENT_TYPE;

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
             |  G  |  D  |  0  |  A  | Segment length 19:16  |

             G - Granularity. 0 = 1 byte, 1 = 1 KByte.

             D - Operand Size. 0 = 16 bit, 1 = 32 bit.

             0 - Always zero.

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
} GDT_ENTRY, *PGDT_ENTRY;

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
} PTE, *PPTE;

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
    UCHAR Padding[224];
};

#pragma pack(pop)

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
};

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

PPTE
ArGetCurrentPageDirectory (
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
    PVOID Value
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

    None.

--*/

VOID
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

    This routine sets the TS bit of CR0, allowing access to the FPU.

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
    GDT_GRANULARITY Granularity,
    GDT_SEGMENT_TYPE Access,
    UCHAR PrivilegeLevel,
    BOOL System
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

    PrivilegeLevel - Supplies the privilege level that this segment requires.
        Valid values are 0 (most privileged, kernel) to 3 (user mode, least
        privileged).

    System - Supplies a flag indicating whether this is a system segment (TRUE)
        or a code/data segment.

Return Value:

    None.

--*/

