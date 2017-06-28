/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    x86defs.h

Abstract:

    This header contains architecture preprocessor definitions that are common
    to x86 and x64 (AMD64). This file is included by assembly.

Author:

    Evan Green 2-Jun-2017

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define SEGMENT_PRIVILEGE_MASK      0x0003
#define SEGMENT_PRIVILEGE_KERNEL    0x0000
#define SEGMENT_PRIVILEGE_USER      0x0003

//
// Define the GDT entries. The first batch is used by both x86 and x64. The
// second set is used only by x86. There's a blank entry after the kernel TSS
// entry because in 64-bit mode it's doubled in size.
//

#define KERNEL_CS               0x08
#define KERNEL_DS               0x10
#define KERNEL64_TRANSITION_CS  0x18
#define USER_CS                 (0x18 | SEGMENT_PRIVILEGE_USER)
#define USER_DS                 (0x20 | SEGMENT_PRIVILEGE_USER)
#define KERNEL_TSS              0x28
#define X64_GDT_ENTRIES         7

#define GDT_PROCESSOR           0x38
#define GDT_THREAD              (0x40 | SEGMENT_PRIVILEGE_USER)
#define DOUBLE_FAULT_TSS        0x48
#define NMI_TSS                 0x50
#define X86_GDT_ENTRIES         11

//
// Define the IST indices.
//

#define X64_IST_NMI             1
#define X64_IST_DOUBLE_FAULT    2

#define GATE_ACCESS_PRESENT     0x80
#define GATE_ACCESS_USER        (SEGMENT_PRIVILEGE_USER << 5)
#define MAX_GDT_LIMIT           0xFFFFF
#define GDT_SYSTEM_SEGMENT      0x00
#define GDT_CODE_DATA_SEGMENT   0x10
#define GDT_TSS_BUSY            0x02

#define GDT_TYPE_DATA_READ 0x10
#define GDT_TYPE_DATA_WRITE 0x12
#define GDT_TYPE_SYSTEM_LDT 0x02
#define GDT_TYPE_CODE 0x18
#define GATE_TYPE_TASK 0x05
#define GDT_TYPE_TSS 0x09
#define GATE_TYPE_CALL 0x0C
#define GATE_TYPE_INTERRUPT 0x0E
#define GATE_TYPE_TRAP 0x0F

#define GDT_GRANULARITY_KILOBYTE 0x80
#define GDT_GRANULARITY_64BIT    0x20
#define GDT_GRANULARITY_32BIT    0x40

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
#define VECTOR_CLOCK_IPI            0xD1
#define VECTOR_IPI_INTERRUPT        0xE0
#define VECTOR_TLB_IPI              0xE1
#define VECTOR_PROFILER_INTERRUPT   0xF0

#define PROCESSOR_VECTOR_COUNT 0x20
#define MINIMUM_VECTOR 0x30
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

#define IA32_EFLAG_STATUS \
    (IA32_EFLAG_CF | IA32_EFLAG_PF | IA32_EFLAG_AF | IA32_EFLAG_ZF | \
     IA32_EFLAG_SF | IA32_EFLAG_OF)

#define IA32_EFLAG_USER \
    (IA32_EFLAG_STATUS | IA32_EFLAG_DF | IA32_EFLAG_TF | IA32_EFLAG_RF)

#define CR0_PAGING_ENABLE 0x80000000
#define CR0_CACHE_DISABLE 0x40000000
#define CR0_NOT_WRITE_THROUGH 0x20000000
#define CR0_ALIGNMENT_MASK 0x00040000
#define CR0_WRITE_PROTECT_ENABLE 0x00010000
#define CR0_NUMERIC_ERROR 0x00000020
#define CR0_EXTENSION_TYPE 0x00000010
#define CR0_TASK_SWITCHED 0x00000008
#define CR0_X87_EMULATION 0x00000004
#define CR0_MONITOR_COPROCESSOR 0x00000002
#define CR0_PROTECTED_MODE 0x00000001

//
// Default masks.
//

#define CR0_OR_MASK \
    (CR0_PROTECTED_MODE | CR0_MONITOR_COPROCESSOR | CR0_TASK_SWITCHED | \
     CR0_NUMERIC_ERROR | CR0_WRITE_PROTECT_ENABLE | CR0_PAGING_ENABLE)

#define CR0_AND_MASK \
    ~(CR0_CACHE_DISABLE | CR0_NOT_WRITE_THROUGH | CR0_X87_EMULATION)

#define CR4_PKE_ENABLE 0x00400000
#define CR4_SMAP_ENABLE 0x00200000
#define CR4_SMEP_ENABLE 0x00100000
#define CR4_OS_XSAVE 0x00040000
#define CR4_PCID_ENABLE 0x00020000
#define CR4_FSGS_BASE 0x00010000
#define CR4_TXT_EXTENSIONS 0x00004000
#define CR4_VT_X_EXTENSIONS 0x00002000
#define CR4_OS_XMM_EXCEPTIONS 0x00000400
#define CR4_OS_FX_SAVE_RESTORE 0x00000200
#define CR4_PERF_COUNTER_ENABLE 0x00000100
#define CR4_PAGE_GLOBAL_ENABLE 0x00000080
#define CR4_MACHINE_CHECK_EXTENSION 0x00000040
#define CR4_PHYSICAL_ADDRESS_EXTENSION 0x00000020
#define CR4_PAGE_SIZE_EXTENSION 0x00000010
#define CR4_DEBUGGING_EXTENSIONS 0x00000008
#define CR4_TIMESTAMP_DISABLE 0x00000004
#define CR4_PROTECTED_MODE_VIRTUAL_INTERRUPTS 0x00000002
#define CR4_V8086_EXTENSIONS 0x00000001

#define CR4_OR_MASK \
    (CR4_OS_FX_SAVE_RESTORE | CR4_PAGE_GLOBAL_ENABLE | \
     CR4_PHYSICAL_ADDRESS_EXTENSION)

#define EFER_TRANSLATION_CACHE_EXTENSION 0x00008000
#define EFER_FAST_FXSAVE 0x00004000
#define EFER_LONG_MODE_SEGMENT_LIMIT 0x00002000
#define EFER_SECURE_VIRTUAL_MACHINE 0x00001000
#define EFER_NO_EXECUTE_ENABLE 0x00000800
#define EFER_LONG_MODE_ACTIVE 0x00000400
#define EFER_LONG_MODE_ENABLE 0x00000100
#define EFER_SYSTEM_CALL_EXTENSIONS 0x00000001

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
#define X86_CPUID_MWAIT 0x00000005
#define X86_CPUID_EXTENDED_IDENTIFICATION 0x80000000
#define X86_CPUID_EXTENDED_INFORMATION 0x80000001
#define X86_CPUID_ADVANCED_POWER_MANAGEMENT 0x80000007

//
// Define basic information CPUID bits (eax is 1).
//

#define X86_CPUID_BASIC_EAX_STEPPING_MASK 0x00000003
#define X86_CPUID_BASIC_EAX_BASE_MODEL_MASK (0xF << 4)
#define X86_CPUID_BASIC_EAX_BASE_MODEL_SHIFT 4
#define X86_CPUID_BASIC_EAX_BASE_FAMILY_MASK (0xF << 8)
#define X86_CPUID_BASIC_EAX_BASE_FAMILY_SHIFT 8
#define X86_CPUID_BASIC_EAX_EXTENDED_MODEL_MASK (0xF << 16)
#define X86_CPUID_BASIC_EAX_EXTENDED_MODEL_SHIFT 16
#define X86_CPUID_BASIC_EAX_EXTENDED_FAMILY_MASK (0xFF << 20)
#define X86_CPUID_BASIC_EAX_EXTENDED_FAMILY_SHIFT 20

#define X86_CPUID_BASIC_ECX_MONITOR (1 << 3)
#define X86_CPUID_BASIC_EDX_SYSENTER (1 << 11)
#define X86_CPUID_BASIC_EDX_CMOV (1 << 15)
#define X86_CPUID_BASIC_EDX_FX_SAVE_RESTORE (1 << 24)

//
// Define known CPU vendors.
//

#define X86_VENDOR_INTEL 0x756E6547
#define X86_VENDOR_AMD 0x68747541

//
// Define monitor/mwait leaf bits.
//

#define X86_CPUID_MWAIT_ECX_EXTENSIONS_SUPPORTED 0x00000001
#define X86_CPUID_MWAIT_ECX_INTERRUPT_BREAK 0x00000002

//
// Define extended information CPUID bits (eax is 0x80000001).
//

#define X86_CPUID_EXTENDED_INFORMATION_EDX_SYSCALL (1 << 11)
#define X86_CPUID_EXTENDED_INFORMATION_EDX_1GB_PAGES (1 << 26)
#define X86_CPUID_EXTENDED_INFORMATION_EDX_LONG_MODE (1 << 29)

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

#define FPU_CONTEXT_ALIGNMENT 64

//
// Define MSR values.
//

#define X86_MSR_POWER_CONTROL_C1E_PROMOTION 0x00000002

#define X86_MSR_SYSENTER_CS     0x00000174
#define X86_MSR_SYSENTER_ESP    0x00000175
#define X86_MSR_SYSENTER_EIP    0x00000176
#define X86_MSR_POWER_CONTROL   0x000001FC
#define X86_MSR_EFER            0xC0000080
#define X86_MSR_STAR            0xC0000081
#define X86_MSR_LSTAR           0xC0000082
#define X86_MSR_FMASK           0xC0000084
#define X86_MSR_FSBASE          0xC0000100
#define X86_MSR_GSBASE          0xC0000101
#define X86_MSR_KERNEL_GSBASE   0xC0000102

//
// Define the PTE bits.
//

#define X86_PTE_PRESENT        0x00000001
#define X86_PTE_WRITABLE       0x00000002
#define X86_PTE_USER_MODE      0x00000004
#define X86_PTE_WRITE_THROUGH  0x00000008
#define X86_PTE_CACHE_DISABLED 0x00000010
#define X86_PTE_ACCESSED       0x00000020
#define X86_PTE_DIRTY          0x00000040
#define X86_PTE_LARGE          0x00000080
#define X86_PTE_GLOBAL         0x00000100
#define X86_PTE_NX             0x8000000000000000
#define X86_PTE_ENTRY_SHIFT    12

//
// Define the size of the x64 red zone. This is not used in the kernel, since
// the exception handlers in the hardware don't honor it. But it is used by
// user mode, so signal dispatching has to be careful.
//

#define X64_RED_ZONE 128

//
// Define the location of the identity mapped stub. Since x86 doesn't have
// relative addressing the AP code really is hardwired for this address. This
// needs to be in the first megabyte since it starts running in real mode, and
// needs to avoid known BIOS regions.
//

#define IDENTITY_STUB_ADDRESS 0x00001000

//
// --------------------------------------------------------------------- Macros
//

//
// This macro gets a value at the given offset from the current processor block.
// _Result should be the appropriate size.
//

#define FS_READ32(_Result, _Offset) \
    asm volatile ("movl %%fs:(%1), %k0" : "=r" (_Result) : "r" (_Offset))

#define FS_READ64(_Result, _Offset) \
    asm volatile ("movq %%fs:(%1), %q0" : "=r" (_Result) : "r" (_Offset))

#define GS_READ32(_Result, _Offset) \
    asm volatile ("movl %%gs:(%1), %k0" : "=r" (_Result) : "r" (_Offset))

#define GS_READ64(_Result, _Offset) \
    asm volatile ("movq %%gs:(%1), %q0" : "=r" (_Result) : "r" (_Offset))

#if __SIZEOF_LONG__ == 8

#define FS_READN(_Result, _Offset) FS_READ64(_Result, _Offset)
#define GS_READN(_Result, _Offset) GS_READ64(_Result, _Offset)

#else

#define FS_READN(_Result, _Offset) FS_READ32(_Result, _Offset)
#define GS_READN(_Result, _Offset) GS_READ32(_Result, _Offset)

#endif

//
// This macro determines whether or not the given trap frame is from privileged
// mode.
//

#define IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(_TrapFrame) \
    (((_TrapFrame)->Cs & SEGMENT_PRIVILEGE_MASK) == 0)

//
// This macro determines whether or not the given trap frame is complete or
// left mostly uninitialized by the system call handler. The system call
// handler sets CS to user DS as a hint that the trap frame is incomplete.
//

#define IS_TRAP_FRAME_COMPLETE(_TrapFrame) \
    (IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(_TrapFrame) || \
     ((_TrapFrame)->Cs == USER_CS))

//
// This macro extracts the address ut of a PTE (or PDE, etc).
//

#define X86_PTE_ENTRY(_Pte) ((_Pte) & ~(PAGE_MASK | X86_PTE_NX))

