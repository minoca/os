/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    arm.h

Abstract:

    This header contains definitions for aspects of the system that are specific
    to the ARM architecture.

Author:

    Evan Green 11-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro gets the index into the first level page table for the given
// virtual address.
//

#define FLT_INDEX(_Address) \
    (((ULONG)(_Address) & FLT_INDEX_MASK) >> FLT_INDEX_SHIFT)

//
// This macro gets the index into the second level page table for the given
// virtual address.
//

#define SLT_INDEX(_Address) \
    (((ULONG)(_Address) & SLT_INDEX_MASK) >> SLT_INDEX_SHIFT)

//
// This macro gets the fault status type from the fault status register.
//

#define ARM_FAULT_STATUS_TYPE(_FaultStatus) \
    ((_FaultStatus) & ARM_FAULT_STATUS_TYPE_MASK)

//
// This macro determines if the given fault status is a page fault.
//

#define IS_ARM_PAGE_FAULT(_FaultStatus)             \
    ((ARM_FAULT_STATUS_TYPE(_FaultStatus) ==        \
      ARM_FAULT_STATUS_TYPE_SECTION_TRANLSATION) || \
     (ARM_FAULT_STATUS_TYPE(_FaultStatus) ==        \
      ARM_FAULT_STATUS_TYPE_PAGE_TRANSLATION))

#define IS_ARM_PERMISSION_FAULT(_FaultStatus)      \
    ((ARM_FAULT_STATUS_TYPE(_FaultStatus) ==       \
      ARM_FAULT_STATUS_TYPE_SECTION_PERMISSION) || \
     (ARM_FAULT_STATUS_TYPE(_FaultStatus) ==       \
      ARM_FAULT_STATUS_TYPE_PAGE_PERMISSION))

#define IS_ARM_DEBUG_BREAK(_FaultStatus) \
    (ARM_FAULT_STATUS_TYPE(_FaultStatus) == ARM_FAULT_STATUS_TYPE_DEBUG)

//
// This macro removes the thumb bit from the PC.
//

#define REMOVE_THUMB_BIT(_Pc) ((_Pc) & ~ARM_THUMB_BIT)

//
// This macro extracts the if-then state from a current program status register
// value.
//

#define PSR_GET_IT_STATE(_Cpsr) \
    ((((_Cpsr) >> 8) & 0xFC) | (((_Cpsr) >> 25) & 0x3))

//
// This macro returns the given current program status register value with the
// if-then state bits set to the given if-then state.
//

#define PSR_SET_IT_STATE(_Cpsr, _ItState)   \
    (((_Cpsr) & 0xF9FF03FF) |               \
     (((_ItState) << 25) & 0x06000000) |    \
     (((_ItState) << 8) & 0x0000FC00))

//
// This macro determines if, given a current Program Status Register value,
// the if-then state is active in any form..
//

#define PSR_IS_IT_ACTIVE(_Cpsr) (((_Cpsr) & 0x06000C00) != 0)

//
// This macro determines if the given if-then state is active.
//

#define IS_THUMB_IT_STATE_ACTIVE(_ItState) (((_ItState) & 0x0F) != 0)

//
// This macro extracts the active condition code from the given if-then state.
//

#define THUMB_CONDITION_FROM_IT_STATE(_ItState) (((_ItState) >> 4) & 0xF)

//
// This macro returns the given if-then state, advanced by one instruction.
//

#define THUMB_ADVANCE_IT_STATE(_ItState)                \
    ((((_ItState) & 0x07) == 0) ? 0 :                   \
    ((((_ItState) << 1) & 0xF) | ((_ItState) & 0xF0)))

//
// This macro reverses the if-then state by one instruction, placing the given
// next bit in the next conditional position. This macro assumes the if-then
// state is already active, it does not add the trailing one.
//

#define THUMB_RETREAT_IT_STATE(_ItState, _NextBit) \
    ((((_ItState) >> 1) & 0xF) | ((_NextBit) << 4) | ((_ItState) & 0xE0))

//
// This macro returns whether or not the given trap from is from privileged
// mode.
//

#define IS_TRAP_FRAME_FROM_PRIVILEGED_MODE(_TrapFrame) \
    (((_TrapFrame)->Cpsr & ARM_MODE_MASK) != ARM_MODE_USER)

//
// ---------------------------------------------------------------- Definitions
//

#define ARM_INSTRUCTION_LENGTH 4
#define THUMB16_INSTRUCTION_LENGTH 2
#define THUMB32_INSTRUCTION_LENGTH 4

#define ARM_THUMB_BIT 0x00000001

//
// Processor modes.
//

#define ARM_MODE_USER   0x00000010
#define ARM_MODE_FIQ    0x00000011
#define ARM_MODE_IRQ    0x00000012
#define ARM_MODE_SVC    0x00000013
#define ARM_MODE_ABORT  0x00000017
#define ARM_MODE_UNDEF  0x0000001B
#define ARM_MODE_SYSTEM 0x0000001F
#define ARM_MODE_MASK   0x0000001F

//
// Program Status Register flags.
//

#define PSR_FLAG_NEGATIVE   0x80000000
#define PSR_FLAG_ZERO       0x40000000
#define PSR_FLAG_CARRY      0x20000000
#define PSR_FLAG_OVERFLOW   0x10000000
#define PSR_FLAG_SATURATION 0x08000000
#define PSR_FLAG_JAZELLE    0x01000000
#define PSR_FLAG_THUMB      0x00000020
#define PSR_FLAG_IRQ        0x00000080
#define PSR_FLAG_FIQ        0x00000040

//
// Interrupt vector ranges.
//

#define MINIMUM_VECTOR 0x30
#define MAXIMUM_VECTOR 0xFF
#define MAXIMUM_DEVICE_VECTOR 0xBF
#define INTERRUPT_VECTOR_COUNT (MAXIMUM_VECTOR + 1)
#define IO_PORT_COUNT 0

//
// Interrupt vectors.
//

#define VECTOR_CLOCK_INTERRUPT    0xD0
#define VECTOR_IPI_INTERRUPT      0xE0
#define VECTOR_TLB_IPI            0xE1
#define VECTOR_PROFILER_INTERRUPT 0xF0
#define VECTOR_NMI                0xF1

//
// Undefined instructions used for debug breakpoints.
//

#define THUMB_BREAK_INSTRUCTION 0xDE20
#define THUMB_DEBUG_SERVICE_INSTRUCTION 0xDE24
#define THUMB_SINGLE_STEP_INSTRUCTION 0xDE21

#define ARM_BREAK_INSTRUCTION 0xE7F000F3
#define ARM_SINGLE_STEP_INSTRUCTION 0xE7F000F1
#define ARM_DEBUG_SERVICE_INSTRUCTION 0xE7F000F4

//
// Memory related definitions.
//

#define PAGE_SIZE 4096
#define PAGE_MASK 0x00000FFF
#define PAGE_SHIFT 12
#define EXCEPTION_VECTOR_ADDRESS 0xFFFF0000
#define EXCEPTION_VECTOR_LOW_ADDRESS 0x00000000

//
// Translation table base register address mask.
//
// Bit definitions are tricky for this register because they change based on
// whether or not the Multiprocessing Extensions are supported on the CPU.
//

#define TTBR_ADDRESS_MASK                          0x00003FFF
#define TTBR_NO_MP_INNER_CACHEABLE                 0x00000001
#define TTBR_SHAREABLE                             0x00000002
#define TTBR_NOT_OUTER_SHAREABLE                   0x00000020
#define TTBR_MP_INNER_NON_CACHEABLE                0x00000000
#define TTBR_MP_INNER_WRITE_BACK_WRITE_ALLOCATE    0x00000040
#define TTBR_MP_INNER_WRITE_THROUGH                0x00000001
#define TTBR_MP_INNER_WRITE_BACK_NO_WRITE_ALLOCATE 0x00000041
#define TTBR_OUTER_NON_CACHEABLE                   0x00000000
#define TTBR_OUTER_WRITE_BACK_WRITE_ALLOCATE       0x00000008
#define TTBR_OUTER_WRITE_THROUGH                   0x00000010
#define TTBR_OUTER_WRITE_BACK_NO_WRITE_ALLOCATE    0x00000018

#define TTBR_NO_MP_KERNEL_MASK             \
    (TTBR_NO_MP_INNER_CACHEABLE |          \
     TTBR_OUTER_WRITE_BACK_WRITE_ALLOCATE)

#define TTBR_MP_KERNEL_MASK                    \
    (TTBR_SHAREABLE |                          \
     TTBR_MP_INNER_WRITE_BACK_WRITE_ALLOCATE | \
     TTBR_OUTER_WRITE_BACK_WRITE_ALLOCATE |    \
     TTBR_NOT_OUTER_SHAREABLE)

//
// Page table sizes and alignments.
//

#define FLT_SIZE 0x4000
#define FLT_ALIGNMENT 0x4000
#define FLT_INDEX_MASK 0xFFF00000
#define FLT_INDEX_SHIFT 20
#define SLT_SIZE 1024
#define SLT_INDEX_MASK 0x000FF000
#define SLT_INDEX_SHIFT 12
#define SLT_ALIGNMENT 10

//
// First level page table formats.
//

#define FLT_UNMAPPED          0
#define FLT_COARSE_PAGE_TABLE 1
#define FLT_SECTION           2
#define FLT_SUPERSECTION      2

//
// Second level page table formats.
//

#define SLT_UNMAPPED          0
#define SLT_LARGE_PAGE        1
#define SLT_SMALL_PAGE        2
#define SLT_SMALL_PAGE_NO_EXECUTE 3

//
// Second level page table access permission bits.
//

#define SLT_ACCESS_NONE                 0
#define SLT_ACCESS_SUPERVISOR           1
#define SLT_ACCESS_USER_READ_ONLY       2
#define SLT_ACCESS_USER_FULL            3

//
// Second level page table access permission bits when the Extended Access Bit
// is set. Note that the "read only all modes" value only works for ARMv7, on
// ARMv6 and below this value was reserved and 2 is the correct value.
//

#define SLT_XACCESS_SUPERVISOR_READ_ONLY 1
#define SLT_XACCESS_READ_ONLY_ALL_MODES  3

//
// Second level page table cache attributes
//

#define SLT_TEX_NORMAL 0
#define SLT_UNCACHED 0
#define SLT_SHARED_DEVICE 1
#define SLT_WRITE_THROUGH 2
#define SLT_WRITE_BACK 3

//
// MMU Control bits (SCTLR, CP15, register 1).
//

#define MMU_ENABLED                     0x00000001
#define MMU_ALIGNMENT_FAULT_ENABLED     0x00000002
#define MMU_DCACHE_UNIFIED_ENABLED      0x00000004
#define MMU_WRITE_BUFFER_ENABLED        0x00000008
#define MMU_ENDIANNESS                  0x00000080
#define MMU_SYSTEM_PROTECTION           0x00000100
#define MMU_ROM_PROTECTION              0x00000200
#define MMU_BRANCH_PREDICTION_ENABLED   0x00000800
#define MMU_ICACHE_ENABLED              0x00001000
#define MMU_HIGH_EXCEPTION_VECTORS      0x00002000
#define MMU_PREDICTABLE_REPLACEMENT     0x00004000
#define MMU_DISABLE_THUMB_DEPRECATED    0x00008000
#define MMU_FAST_INTERRUPTS             0x00200000
#define MMU_UNALIGNED_ACCESS_ENABLED    0x00400000
#define MMU_VMSA6_ENABLED               0x00800000
#define MMU_VECTORED_INTERRUPTS_ENABLED 0x01000000
#define MMU_EXCEPTION_ENDIAN            0x02000000
#define MMU_THUMB_EXCEPTIONS            0x40000000

//
// ARMv6 auxiliary control register bits (ACTLR).
//

#define ARMV6_AUX_16K_CACHE_SIZE 0x00000040

//
// Cortex A17 auxiliary control register bits (ACTLR).
//

#define CORTEX_A17_AUX_SMP_ENABLE 0x00000040

//
// Define multiprocessor ID register bits.
//

#define MPIDR_MP_EXTENSIONS_ENABLED          0x80000000
#define MPIDR_UNIPROCESSOR_SYSTEM            0x40000000
#define MPIDR_LOWEST_AFFINITY_INTERDEPENDENT 0x01000000

//
// Define processor features bits.
//

#define CPUID_PROCESSOR1_SECURITY_EXTENSION_MASK 0x000000F0
#define CPUID_PROCESSOR1_SECURITY_EXTENSION_UNSUPPORTED 0

//
// Define bits in the ARMv7 Cache Type Register (CTR).
//

#define ARMV7_CACHE_TYPE_DATA_CACHE_SIZE_MASK 0x000F0000
#define ARMV7_CACHE_TYPE_DATA_CACHE_SIZE_SHIFT 16
#define ARMV7_CACHE_TYPE_INSTRUCTION_CACHE_SIZE_MASK 0x0000000F
#define ARMV7_CACHE_TYPE_INSTRUCTION_CACHE_TYPE_MASK 0x0000C000

//
// Physically indexed, physically tagged caches are the easiest to deal with.
//

#define ARMV7_CACHE_TYPE_INSTRUCTION_CACHE_TYPE_PIPT 0x0000C000

//
// Define bits in the ARMv6 Cache Type Register (CTR).
//

#define ARMV6_CACHE_TYPE_SEPARATE_MASK                 0x01000000
#define ARMV6_CACHE_TYPE_DATA_CACHE_SIZE_MASK          0x003C0000
#define ARMV6_CACHE_TYPE_DATA_CACHE_SIZE_SHIFT         18
#define ARMV6_CACHE_TYPE_DATA_CACHE_LENGTH_MASK        0x00003000
#define ARMV6_CACHE_TYPE_DATA_CACHE_LENGTH_SHIFT       12
#define ARMV6_CACHE_TYPE_INSTRUCTION_CACHE_LENGTH_MASK 0x00000003

//
// Define ARM fault status bits.
//

#define ARM_FAULT_STATUS_EXTERNAL  0x00001000
#define ARM_FAULT_STATUS_WRITE     0x00000800
#define ARM_FAULT_STATUS_TYPE_MASK 0x0000040F

#define ARM_FAULT_STATUS_TYPE_ALIGNMENT                         0x00000001
#define ARM_FAULT_STATUS_TYPE_ICACHE_MAINTENANCE                0x00000004
#define ARM_FAULT_STATUS_TYPE_SYNCHRONOUS_EXTERNAL_FIRST_LEVEL  0x0000000C
#define ARM_FAULT_STATUS_TYPE_SYNCHRONOUS_EXTERNAL_SECOND_LEVEL 0x0000000E
#define ARM_FAULT_STATUS_TYPE_PARITY_FIRST_LEVEL                0x0000040C
#define ARM_FAULT_STATUS_TYPE_PARITY_SECOND_LEVEL               0x0000040E
#define ARM_FAULT_STATUS_TYPE_SECTION_TRANLSATION               0x00000005
#define ARM_FAULT_STATUS_TYPE_PAGE_TRANSLATION                  0x00000007
#define ARM_FAULT_STATUS_TYPE_SECTION_ACCESS                    0x00000003
#define ARM_FAULT_STATUS_TYPE_PAGE_ACCESS                       0x00000006
#define ARM_FAULT_STATUS_TYPE_SECTION_DOMAIN                    0x00000009
#define ARM_FAULT_STATUS_TYPE_PAGE_DOMAIN                       0x0000000B
#define ARM_FAULT_STATUS_TYPE_SECTION_PERMISSION                0x0000000D
#define ARM_FAULT_STATUS_TYPE_PAGE_PERMISSION                   0x0000000F
#define ARM_FAULT_STATUS_TYPE_DEBUG                             0x00000002
#define ARM_FAULT_STATUS_TYPE_SYNCHRONOUS_EXTERNAL              0x00000008
#define ARM_FAULT_STATUS_TYPE_PARITY_MEMORY                     0x00000409
#define ARM_FAULT_STATUS_TYPE_ASYNCHRONOUS_EXTERNAL             0x00000406
#define ARM_FAULT_STATUS_TYPE_ASYNCHRONOUS_PARITY               0x00000408

//
// Define performance monitor control register bits.
//

#define PERF_CONTROL_CYCLE_COUNT_DIVIDE_64 0x00000008
#define PERF_CONTROL_ENABLE                0x00000001

//
// Define the cycle counter performance monitor bit.
//

#define PERF_MONITOR_CYCLE_COUNTER 0x80000000

//
// Define the mask of all performance counter bits.
//

#define PERF_MONITOR_COUNTER_MASK 0xFFFFFFFF

//
// Define performance monitor user mode access enable bit.
//

#define PERF_USER_ACCESS_ENABLE 0x00000001

//
// Define the interrupt mask for the ARM1176 (ARMv6) PMCR.
//

#define ARMV6_PERF_MONITOR_INTERRUPT_MASK 0x00000070

//
// Define the size of an exception stack, in bytes.
//

#define EXCEPTION_STACK_SIZE 8

//
// Define the number of exception stacks that are needed (IRQ, FIQ, Abort,
// and Undefined instruction).
//

#define EXCEPTION_STACK_COUNT 4

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
KSTATUS
(*PGET_NEXT_PC_READ_MEMORY_FUNCTION) (
    PVOID Address,
    ULONG Size,
    PVOID Data
    );

/*++

Routine Description:

    This routine attempts to read memory on behalf of the function trying to
    figure out what the next instruction will be.

Arguments:

    Address - Supplies the virtual address that needs to be read.

    Size - Supplies the number of bytes to be read.

    Data - Supplies a pointer to the buffer where the read data will be
        returned on success.

Return Value:

    Status code. STATUS_SUCCESS will only be returned if all the requested
    bytes could be read.

--*/

/*++

Structure Description:

    This structure outlines a trap frame that will be generated during most
    interrupts and exceptions.

Members:

    Registers - Stores the current state of the machine's registers. These
        values will be restored upon completion of the interrupt or exception.

--*/

struct _TRAP_FRAME {
    ULONG SvcSp;
    ULONG UserSp;
    ULONG UserLink;
    ULONG R0;
    ULONG ExceptionCpsr;
    ULONG R1;
    ULONG R2;
    ULONG R3;
    ULONG R4;
    ULONG R5;
    ULONG R6;
    ULONG R7;
    ULONG R8;
    ULONG R9;
    ULONG R10;
    ULONG R11;
    ULONG R12;
    ULONG SvcLink;
    ULONG Pc;
    ULONG Cpsr;
};

/*++

Structure Description:

    This structure contains the state of the processor, including both the
    non-volatile general registers and the system registers configured by the
    kernel. This structure is used in a manner similar to the C library
    setjmp/longjmp routines, the save context function appears to return
    twice. It returns once after the saving is complete, and then again with
    a different return value after restoring.

Members:

    Pc - Stores the PC to branch to upon restore. By default this is
        initialized to the return address of the save/restore function, though
        it can be manipulated after the function returns.

    Cpsr - Stores the program status word (processor flags and mode).

    R0 - Stores the R0 register, also the return value from the restore
        operation. By default this is initialized to 1.

    R1 - Stores the R1 register, which can be used for a second argument in
        case this context is being manipulated.

    R2 - Stores the R2 register, which can be used for a third argument in
        case the PC is manipulated after save context returns.

    R3 - Stores the R3 register, which can be used for a third argument in
        case the PC is manipulated after save context returns.

    R4 - Stores a non-volatile register.

    R5 - Stores a non-volatile register.

    R6 - Stores a non-volatile register.

    R7 - Stores a non-volatile register.

    R8 - Stores a non-volatile register.

    R9 - Stores a non-volatile register.

    R10 - Stores a non-volatile register.

    R11 - Stores a non-volatile register. R12 is volatile, and is not available
        since the restore code needs a register for its operation.

    Sp - Stores the stack pointer.

    VirtualAddress - Stores the virtual address of this structure member. The
        restore process might enable paging when the SCTLR is restored, so this
        contains the address to continue the restore from in virtual land.

    Sctlr - Stores the system control register.

    Ttbr0 - Stores the translation table base register 0.

    Ttbr1 - Stores the translation table base register 1.

    Actlr - Stores the auxiliary system control register.

    Cpacr - Stores the coprocessor access control register.

    Prrr - Stores the primary region remap register.

    Nmrr - Stores the normal memory remap register.

    ContextIdr - Stores the ASID register.

    Dfsr - Stores the data fault status register.

    Dfar - Store the data fault address register.

    Ifsr - Stores the instruction fault status register.

    Ifar - Stores the instruction fault address register.

    Dacr - Stores the domain access control register.

    Vbar - Stores the virtual base address register.

    Tpidrprw - Stores the privileged thread pointer register.

    Tpidruro - Stores the user read-only thread pointer register.

    Tpidrurw - Stores the user read-write thread pointer register.

    Pmcr - Stores the performance control register.

    Pminten - Stores the performance enabled interrupts.

    Pmuserenr - Stores the performance user enable register.

    Pmcnten - Stores the performance counter enable value.

    Pmccntr - Stores the cycle counter value.

--*/

struct _PROCESSOR_CONTEXT {
    ULONG Pc;
    ULONG Cpsr;
    ULONG R0;
    ULONG R1;
    ULONG R2;
    ULONG R3;
    ULONG R4;
    ULONG R5;
    ULONG R6;
    ULONG R7;
    ULONG R8;
    ULONG R9;
    ULONG R10;
    ULONG R11;
    ULONG Sp;
    ULONG VirtualAddress;
    ULONG Sctlr;
    ULONG Ttbr0;
    ULONG Ttbr1;
    ULONG Actlr;
    ULONG Cpacr;
    ULONG Prrr;
    ULONG Nmrr;
    ULONG ContextIdr;
    ULONG Dfsr;
    ULONG Dfar;
    ULONG Ifsr;
    ULONG Ifar;
    ULONG Dacr;
    ULONG Vbar;
    ULONG Tpidrprw;
    ULONG Tpidruro;
    ULONG Tpidrurw;
    ULONG Pmcr;
    ULONG Pminten;
    ULONG Pmuserenr;
    ULONG Pmcntenset;
    ULONG Pmccntr;
};

/*++

Structure Description:

    This structure outlines an ARM interrupt dispatch table. The first half of
    this table is defined by the hardware, and contains instructions at known
    locations where the PC is snapped to when various types of exceptions occur.
    The second half of the table contains pointers to handler routines. The
    instructions in the table by default contain load PC instructions for the
    corresponding pointers. The locations of these pointers (but not their
    values) need to be kept near to the jump table because a ldr instruction
    can only reach so far.

Members:

    ResetInstruction - Stores the instruction to execute on a Reset.

    UndefinedInstructionInstruction - Stores the instruction to execute
        upon encountering an undefined instruction.

    SoftwareInterruptInstruction - Stores the instruction to execute on a SWI
        instruction.

    PrefetchAbortInstruction - Stores the instruction to execute on an
        instruction fetch page fault.

    DataAbortInstruction - Stores the instruction to execute on a data access
        fault.

    Reserved - This space is reserved by the ARM ISA.

    IrqInstruction - Stores the instruction to execute on an IRQ interrupt.

    FiqInstruction - Stores the instruction to execute on an FIQ interrupt.

    UndefinedInstructionVector - Stores the address to jump to on encountering
        an undefined instruction. This is used for setting software
        breakpoints.

    SoftwareInterruptVector - Stores the address to jump to on encountering
        an SWI instruction. This is used for user to kernel transitions.

    PrefetchAbortVector - Stores the address to jump to on encountering an
        instruction fetch fault.

    DataAbortVector - Stores the address to jump to on encountering a data
        access fault.

    IrqVector - Stores the address to jump to on an IRQ interrupt.

    FiqVector - Stores the address to jump to on an FIQ interrupt.

    ResetVector - Stores the address to jump to on a reset.

--*/

typedef struct _ARM_INTERRUPT_TABLE {
    ULONG ResetInstruction;
    ULONG UndefinedInstructionInstruction;
    ULONG SoftwareInterruptInstruction;
    ULONG PrefetchAbortInstruction;
    ULONG DataAbortInstruction;
    ULONG Reserved;
    ULONG IrqInstruction;
    ULONG FiqInstruction;
    PVOID UndefinedInstructionVector;
    PVOID SoftwareInterruptVector;
    PVOID PrefetchAbortVector;
    PVOID DataAbortVector;
    PVOID IrqVector;
    PVOID FiqVector;
    PVOID ResetVector;
} ARM_INTERRUPT_TABLE, *PARM_INTERRUPT_TABLE;

/*++

Structure Description:

    This structure describes the first level page table entry for a "Coarse
    Page Table". It is equivalent to a PDE for x86.

Members:

    Format - Stores the format of this table entry, which should be set to
        1 to describe this structure, a Coarse Page Table. Other formats
        include Section (2), and Fault (0). Not present entries should set this
        to 0 (Fault).

    Reserved - Set to 0.

    Domain - Stores the broad level domain this entry falls under.

    ImplementationDefined - Stores an implementation-defined bit.

    Entry - Stores the high 22 bits of the physical address for the second
        level page table. The low 12 bits are 0 because the second level page
        table must be page-aligned.

--*/

typedef struct _FIRST_LEVEL_TABLE {
    ULONG Format:2;
    ULONG Reserved:3;
    ULONG Domain:4;
    ULONG ImplementationDefined:1;
    ULONG Entry:22;
} FIRST_LEVEL_TABLE, *PFIRST_LEVEL_TABLE;

/*++

Structure Description:

    This structure describes the second level page table entry format for "Small
    Pages", which are 4KB in size.

Members:

    Format - Stores the format of the second level page table entry. For this
        structure, this should be set to 2 or 3 (Extended Small Page). Unmapped
        pages would be marked 0 (Fault). Large pages would be marked 1.

    CacheAttributes - Stores the caching attributes for the page. Options are
        uncached, shared device, write back, and write through.

    Access - Stores the access permissions for user mode and supervisor mode to
        the page.

    CacheTypeExtension - Stores extension bits to the caching attributes. Set
        to 0 for most cache types.

    AccessExtension - Stores the extension bit to the access attributes. Set to
        0 for read-only modes and 1 for full access modes.

    Shared - Stores whether or not this page is shared among multiple processors
        or restricted to one. This only applies for normal memory, device
        memory uses the TEX + CB (CacheAttributes) bits.

    NotGlobal - Stores whether this page is global (0) or local to the current
        process.

    Entry - Stores the high 20 bits of the physical address of the "Small page".

--*/

typedef struct _SECOND_LEVEL_TABLE {
    ULONG Format:2;
    ULONG CacheAttributes:2;
    ULONG Access:2;
    ULONG CacheTypeExtension:3;
    ULONG AccessExtension:1;
    ULONG Shared:1;
    ULONG NotGlobal:1;
    ULONG Entry:20;
} SECOND_LEVEL_TABLE, *PSECOND_LEVEL_TABLE;

/*++

Structure Description:

    This structure passes information about the ARM CPU Identification
    registers.

Members:

    ProcessorFeatures - Stores a bitfield of processor features (ID_PFR0 and
        ID_PFR1).

    DebugFeatures - Stores a bitfield of debug hardware features (ID_DFR0).

    AuxiliaryFeatures - Stores an implementation-defined feature bitfield
        (ID_AFR0).

    MemoryModelFeatures - Stores bitfields of memory model features (ID_MMFR0,
        ID_MMFR1, ID_MMFR2, and ID_MMFR3).

    IsaFeatures - Stores bitfields about the supported instruction sets on
        this processor (ID_ISAR0, ID_ISAR1, ID_ISAR2, ID_ISAR3, ID_ISAR4, and
        ID_ISAR5).

--*/

typedef struct _ARM_CPUID {
    ULONG ProcessorFeatures[2];
    ULONG DebugFeatures;
    ULONG AuxiliaryFeatures;
    ULONG MemoryModelFeatures[4];
    ULONG IsaFeatures[6];
} PACKED ARM_CPUID, *PARM_CPUID;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

ULONG
ArGetCacheTypeRegister (
    VOID
    );

/*++

Routine Description:

    This routine retrieves the Cache Type Register (CTR) from the system
    coprocessor.

Arguments:

    None.

Return Value:

    Returns the value of the CTR.

--*/

VOID
ArCleanEntireCache (
    VOID
    );

/*++

Routine Description:

    This routine cleans the entire data cache.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArCleanInvalidateEntireCache (
    VOID
    );

/*++

Routine Description:

    This routine cleans and invalidates the entire data cache.

Arguments:

    None.

Return Value:

    None.

--*/

ULONG
ArLockTlbEntry (
    ULONG TlbEntry,
    PVOID VirtualAddress,
    ULONG NextTlbEntry
    );

/*++

Routine Description:

    This routine locks a translation in the TLB. This translation will stick
    even across total TLB invalidates.

Arguments:

    TlbEntry - Supplies the base and victim number of the TLB entry to lock.

    VirtualAddress - Supplies the virtual address that should be locked in the
        TLB. The association to physical address will be created by touching
        that address, so the address had better be mapped.

    NextTlbEntry - Supplies the base and victim number to set after locking the
        entry.

Return Value:

    Returns the value of the lockdown register after the TLB miss was forced.
    The lowest bit of this value should be set. If it is not, this indicates
    that TLB lockdown is not supported.

--*/

VOID
ArpInitializeExceptionStacks (
    PVOID ExceptionStacksBase
    );

/*++

Routine Description:

    This routine initializes the stack pointer for all privileged ARM modes. It
    switches into each mode and initializes the banked r13. This function
    should be called with interrupts disabled and returns with interrupts
    disabled.

Arguments:

    ExceptionStacksBase - Supplies a pointer to the lowest address that should
        be used for exception stacks. Each stack takes up 16 bytes and there are
        4 modes, so at least 64 bytes are needed.

Return Value:

    None.

--*/

VOID
ArpInitializePerformanceMonitor (
    VOID
    );

/*++

Routine Description:

    This routine initializes the system's performance monitor.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArpUndefinedInstructionEntry (
    VOID
    );

/*++

Routine Description:

    This routine directly handles an exception generated by an undefined
    instruction.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArpSoftwareInterruptEntry (
    VOID
    );

/*++

Routine Description:

    This routine directly handles an exception generated by a software
    interrupt (a system call).

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArpPrefetchAbortEntry (
    VOID
    );

/*++

Routine Description:

    This routine directly handles an exception generated by a prefetch abort
    (page fault).

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArpDataAbortEntry (
    VOID
    );

/*++

Routine Description:

    This routine directly handles an exception generated by a data abort (page
    fault).

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArpIrqEntry (
    VOID
    );

/*++

Routine Description:

    This routine directly handles an exception generated by an external
    interrupt on the IRQ pin.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
ArpFiqEntry (
    VOID
    );

/*++

Routine Description:

    This routine directly handles an exception generated by an external
    interrupt on the FIQ pin.

Arguments:

    None.

Return Value:

    None.

--*/

PVOID
ArGetDataFaultingAddress (
    VOID
    );

/*++

Routine Description:

    This routine determines which address caused a data abort.

Arguments:

    None.

Return Value:

    Returns the faulting address.

--*/

VOID
ArSetDataFaultingAddress (
    PVOID Value
    );

/*++

Routine Description:

    This routine sets the data faulting address register (DFAR).

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

PVOID
ArGetInstructionFaultingAddress (
    VOID
    );

/*++

Routine Description:

    This routine determines which address caused a prefetch abort.

Arguments:

    None.

Return Value:

    Returns the faulting address.

--*/

VOID
ArSetInstructionFaultingAddress (
    PVOID Value
    );

/*++

Routine Description:

    This routine sets the instruction faulting address register (IFAR).

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetDataFaultStatus (
    VOID
    );

/*++

Routine Description:

    This routine determines the reason for the fault by reading the DFSR
    register.

Arguments:

    None.

Return Value:

    Returns the contents of the Data Fault Status Register.

--*/

VOID
ArSetDataFaultStatus (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the data fault status register (DFSR).

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

ULONG
ArGetInstructionFaultStatus (
    VOID
    );

/*++

Routine Description:

    This routine determines the reason for the prefetch abort by reading the
    IFAR register.

Arguments:

    None.

Return Value:

    Returns the contents of the Instruction Fault Status Register.

--*/

VOID
ArSetInstructionFaultStatus (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the instruction fault status register (IFSR).

Arguments:

    Value - Supplies the value to set.

Return Value:

    None.

--*/

VOID
ArCpuid (
    PARM_CPUID Features
    );

/*++

Routine Description:

    This routine returns the set of processor features present on the current
    processor.

Arguments:

    Features - Supplies a pointer where the processor feature register values
        will be returned.

Return Value:

    None.

--*/

ULONG
ArGetSystemControlRegister (
    VOID
    );

/*++

Routine Description:

    This routine returns the MMU system control register (SCTLR).

Arguments:

    None.

Return Value:

    Returns the current SCTLR value.

--*/

VOID
ArSetSystemControlRegister (
    ULONG NewValue
    );

/*++

Routine Description:

    This routine sets the MMU system control register (SCTLR).

Arguments:

    NewValue - Supplies the value to set as the new MMU SCTLR.

Return Value:

    None.

--*/

ULONG
ArGetAuxiliaryControlRegister (
    VOID
    );

/*++

Routine Description:

    This routine returns the auxiliary system control register (ACTLR).

Arguments:

    None.

Return Value:

    Returns the current value.

--*/

VOID
ArSetAuxiliaryControlRegister (
    ULONG NewValue
    );

/*++

Routine Description:

    This routine sets the auxiliary system control register (ACTLR).

Arguments:

    NewValue - Supplies the value to set.

Return Value:

    None.

--*/

PVOID
ArGetVectorBaseAddress (
    VOID
    );

/*++

Routine Description:

    This routine gets the vector base address register (VBAR) which determines
    where the ARM exception vector table starts.

Arguments:

    None.

Return Value:

    Returns the current VBAR.

--*/

VOID
ArSetVectorBaseAddress (
    PVOID VectorBaseAddress
    );

/*++

Routine Description:

    This routine sets the vector base address register (VBAR) which determines
    where the ARM exception vector table starts.

Arguments:

    VectorBaseAddress - Supplies a pointer to the ARM exception vector base
        address. This value must be 32-byte aligned.

Return Value:

    None.

--*/

PVOID
ArGetProcessorBlockRegister (
    VOID
    );

/*++

Routine Description:

    This routine gets the register used to store a pointer to the processor
    block (TPIDRPRW in the ARMARM; Thread and Process ID Registers in the
    ARM1176 TRM).

Arguments:

    None.

Return Value:

    Returns a pointer to the processor block.

--*/

PVOID
ArGetProcessorBlockRegisterForDebugger (
    VOID
    );

/*++

Routine Description:

    This routine gets the register used to store a pointer to the processor
    block (TPIDRPRW in the ARMARM; Thread and Process ID Registers in the
    ARM1176 TRM). This routine is called inside the debugger.

Arguments:

    None.

Return Value:

    Returns a pointer to the processor block.

--*/

VOID
ArSetProcessorBlockRegister (
    PVOID ProcessorBlockRegisterValue
    );

/*++

Routine Description:

    This routine sets the register used to store a pointer to the processor
    block (TPIDRPRW in the ARMARM; Thread and Process ID Registers in the
    ARM1176 TRM).

Arguments:

    ProcessorBlockRegisterValue - Supplies the value to assign to the register
        used to store the processor block.

Return Value:

    None.

--*/

UINTN
ArDereferenceProcessorBlock (
    UINTN Offset
    );

/*++

Routine Description:

    This routine performs a native integer read of the processor block plus
    a given offset. The C equivalent of this would be
    *((PUINTN)(ProcessorBlock + Offset)).

Arguments:

    Offset - Supplies the offset into the processor block to read.

Return Value:

    Returns the native integer read at the given address.

--*/

ULONG
ArGetTranslationTableBaseRegister0 (
    VOID
    );

/*++

Routine Description:

    This routine gets the translation table base register 0 (TTBR0), used as
    the base for all virtual to physical memory lookups.

Arguments:

    None.

Return Value:

    Returns the contents of TTBR0.

--*/

VOID
ArSetTranslationTableBaseRegister0 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the translation table base register 0 (TTBR0).

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

ULONG
ArGetTranslationTableBaseRegister1 (
    VOID
    );

/*++

Routine Description:

    This routine gets the translation table base register 1 (TTBR1).

Arguments:

    None.

Return Value:

    Returns the contents of TTBR1.

--*/

VOID
ArSetTranslationTableBaseRegister1 (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the translation table base register 1 (TTBR1).

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

ULONG
ArGetPrimaryRegionRemapRegister (
    VOID
    );

/*++

Routine Description:

    This routine gets the Primary Region Remap Register (PRRR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

VOID
ArSetPrimaryRegionRemapRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the PRRR.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

ULONG
ArGetNormalMemoryRemapRegister (
    VOID
    );

/*++

Routine Description:

    This routine gets the Normal Memory Remap Register (NMRR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

VOID
ArSetNormalMemoryRemapRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the NMRR.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

ULONG
ArGetPhysicalAddressRegister (
    VOID
    );

/*++

Routine Description:

    This routine gets the Physical Address Register (PAR).

Arguments:

    None.

Return Value:

    Returns the contents of the register.

--*/

VOID
ArSetPhysicalAddressRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the Physical Address Register (PAR).

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

VOID
ArSetPrivilegedReadTranslateRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the Privileged Read address translation command register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

VOID
ArSetPrivilegedWriteTranslateRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the Privileged Write address translation command register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

VOID
ArSetUnprivilegedReadTranslateRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the Unrivileged Read address translation command register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

VOID
ArSetUnprivilegedWriteTranslateRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the Unprivileged Write address translation command
    register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    None.

--*/

ULONG
ArGetMultiprocessorIdRegister (
     VOID
     );

/*++

Routine Description:

    This routine gets the Multiprocessor ID register (MPIDR).

Arguments:

    None.

Return Value:

    Returns the value of the MPIDR.

--*/

ULONG
ArTranslateVirtualToPhysical (
    PVOID VirtualAddress
    );

/*++

Routine Description:

    This routine translates a virtual address to its corresponding physical
    address by using the current translation tables.

Arguments:

    VirtualAddress - Supplies the virtual address to translate.

Return Value:

    Returns the physical address that the virtual address corresponds to
    (with some bits at the bottom relating to the cache type).

--*/

VOID
ArSetThreadPointerUserReadOnly (
    PVOID NewPointer
    );

/*++

Routine Description:

    This routine sets the TPIDRURO user-mode-read-only thread pointer register.

Arguments:

    NewPointer - Supplies the value to write.

Return Value:

    None.

--*/

ULONG
ArGetThreadPointerUser (
    VOID
    );

/*++

Routine Description:

    This routine sets the TPIDRURW user-mode thread pointer register.

Arguments:

    None.

Return Value:

    Returns the current value of the TPIDRURW.

--*/

ULONG
ArGetPerformanceControlRegister (
    VOID
    );

/*++

Routine Description:

    This routine retrieves the PMCR (Performance Monitor Control Register).

Arguments:

    None.

Return Value:

    Returns the value of the PMCR.

--*/

VOID
ArSetPerformanceControlRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the PMCR (Performance Monitor Control Register).

Arguments:

    Value - Supplies the value to set in the PMCR.

Return Value:

    None.

--*/

VOID
ArClearPerformanceInterruptRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the PMINTENCLR (Performance Monitor Interrupt Clear)
    register.

Arguments:

    Value - Supplies the value to set in the PMINTENCLR.

Return Value:

    None.

--*/

VOID
ArSetPerformanceUserEnableRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the PMUSERENR (Performance Monitor User Enable Register).

Arguments:

    Value - Supplies the value to set in the PMUSERENR.

Return Value:

    None.

--*/

ULONG
ArGetPerformanceCounterEnableRegister (
    VOID
    );

/*++

Routine Description:

    This routine retrieves the PMCNTENSET (Performance Monitor Counter Enable
    Set) register.

Arguments:

    None.

Return Value:

    Returns the value of the PMCNTENSET.

--*/

VOID
ArSetPerformanceCounterEnableRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the PMCNTENSET (Performance Monitor Counter Enable
    Set) register.

Arguments:

    Value - Supplies the value to set in the PMCNTENSET register.

Return Value:

    None.

--*/

ULONG
ArGetCycleCountRegister (
    VOID
    );

/*++

Routine Description:

    This routine retrieves the PMCCNTR (Performance Monitor Cycle Counter)
    register.

Arguments:

    None.

Return Value:

    Returns the value of the PMCCNTR.

--*/

VOID
ArSetCycleCountRegister (
    ULONG Value
    );

/*++

Routine Description:

    This routine sets the PMCCNTR (Performance Monitor Cycle Counter) register.

Arguments:

    Value - Supplies the value to set in the PMCCNTR register.

Return Value:

    None.

--*/

KSTATUS
ArGetNextPc (
    PTRAP_FRAME TrapFrame,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PBOOL IsFunctionReturning,
    PVOID *NextPcValue
    );

/*++

Routine Description:

    This routine attempts to predict the next instruction to be executed. It
    will decode the current instruction, check if the condition matches, and
    attempt to follow any branches.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

    ReadMemoryFunction - Supplies a pointer to a function this routine can
        call when it needs to read target memory.

    IsFunctionReturning - Supplies an optional pointer where a boolean will be
        stored indicating if the current instruction is a return of some kind.

    NextPcValue - Supplies a pointer of the next executing address.

Return Value:

    Status code. This routine will attempt to make a guess at the next PC even
    if the status code is failing, but chances it's right go way down if a
    failing status is returned.

--*/

VOID
ArBackUpIfThenState (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine backs up the Thumb if-then state in the CPSR by one
    instruction, assuming that the previous instruction tested positively for
    being executed.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

Return Value:

    Status code. This routine will attempt to make a guess at the next PC even
    if the status code is failing, but chances it's right go way down if a
    failing status is returned.

--*/

