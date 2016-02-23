/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    x86.inc

Abstract:

    This module contains common definitions for the x86 architecture.

Author:

    Evan Green 18-Jul-2012

Environment:

    Kernel mode

--*/

##
## --------------------------------------------------------------- Definitions
##

##
## Processor flags
##

#define IA32_EFLAG_TF        0x00000100
#define IA32_EFLAG_IF        0x00000200
#define IA32_EFLAG_IOPL_MASK 0x00003000
#define IA32_EFLAG_VM        0x00020000
#define IA32_EFLAG_VIF       0x00080000

#define SEGMENT_PRIVILEGE_MASK 0x0003

##
## CR0 flags
##

#define CR0_PAGING_ENABLE        0x80000000
#define CR0_CACHE_DISABLE        0x40000000
#define CR0_NOT_WRITE_THROUGH    0x20000000
#define CR0_WRITE_PROTECT_ENABLE 0x00010000
#define CR0_X87_EXCEPTIONS       0x00000020
#define CR0_TASK_SWITCHED        0x00000008
#define CR0_EMULATE_FPU          0x00000004
#define CR0_MONITOR_COPROCESSOR  0x00000002
#define CR0_PROTECTED_MODE       0x00000001

#define CR0_OR_MASK 0x8001002B
#define CR0_AND_MASK ~(0x60000004)

##
## Basic constants.
##

#define FALSE 0
#define TRUE 1
#define NULL 0

##
## Kernel constants.
##

#define KERNEL_CS        0x08
#define KERNEL_DS        0x10
#define USER_CS          (0x18 | 3)
#define USER_DS          (0x20 | 3)
#define GDT_PROCESSOR    0x28
#define GDT_THREAD       (0x30 | 3)
#define KERNEL_TSS       0x38
#define DOUBLE_FAULT_TSS 0x40
#define NMI_TSS          0x48
#define GDT_ENTRIES      10

#define EXCEPTION_NMI               0x02
#define EXCEPTION_BREAK             0x03
#define EXCEPTION_SINGLE_STEP       0x04
#define EXCEPTION_ACCESS_VIOLATION  0x05
#define EXCEPTION_ASSERTION_FAILURE 0x07
#define EXCEPTION_DOUBLE_FAULT      0x0C

#define CONTEXT_SWAP_MAGIC 0x5A4A3A2A

##
## Relevant TSS structure definitions.
##

#define TSS_ESP0  0x04

##
## Processor block offsets.
##

#define PROCESSOR_BLOCK_TSS 0x0C
#define PROCESSOR_BLOCK_GDT 0x10

##
## Definition for the TRAP_FRAME structure and the exception stack directly
## above it.
##

#define TRAP_DS             0
#define TRAP_ES             4
#define TRAP_FS             8
#define TRAP_GS             12
#define TRAP_SS             16
#define TRAP_EAX            20
#define TRAP_EBX            24
#define TRAP_ECX            28
#define TRAP_EDX            32
#define TRAP_ESI            36
#define TRAP_EDI            40
#define TRAP_EBP            44
#define TRAP_ERRORCODE      48
#define TRAP_EIP            52
#define TRAP_CS             56
#define TRAP_EFLAGS         60
#define TRAP_ESP            64
#define TRAP_RET_ERRORCODE  68
#define TRAP_RET_EIP        72
#define TRAP_RET_CS         76
#define TRAP_RET_EFLAGS     80
#define TRAP_RET_ESP        84
#define TRAP_RET_SS         88
#define TRAP_RET_ES         92
#define TRAP_RET_DS         96
#define TRAP_RET_FS         100
#define TRAP_RET_GS         104

#define TRAP_FRAME_SIZE     68

#define PROCESSOR_CONTEXT_SIZE 0x60

##
## Define the minimum and maximum external interrupt vectors.
##

#define MINIMUM_VECTOR 0x30
#define MAXIMUM_VECTOR 0xFF

##
## APIC End Of Interrupt Offset.
##

#define APIC_EOI_OFFSET    0xB

##
## -------------------------------------------------------------------- Macros
##

##
## This macro switches the DS and ES data segments to the kernel's data
## segments.
##

.macro LoadKernelDataSegments
    movw    $KERNEL_DS, %ax         # Get the data segment selector,
    mov     %ax, %ds                # and save it into the data segments.
    mov     %ax, %es                #
    mov     $GDT_PROCESSOR, %ax     # Get the processor segment.
    mov     %ax, %fs                # Load it.
.endm

#if defined(__ELF__)

##
## This macro defines a function, callable from C code in any module and
## capable of being overridden by other functions.
##

#define EXPORTED_FUNCTION(_Name) \
    .func _Name ; \
    .type _Name, %function ; \
    .cfi_startproc ; \
    .global _Name ; \
    _Name:

##
## This macro defines a function, callable from C code in the current module
## only.
##

#define FUNCTION(_Name) \
    .hidden _Name ; \
    EXPORTED_FUNCTION(_Name)

##
## This macro defines a function, callable from C code in any module but always
## called locally in the current module.
##

#define PROTECTED_FUNCTION(_Name) \
    .protected _Name ; \
    EXPORTED_FUNCTION(_Name)

#define END_FUNCTION(_Name) \
    .size _Name, .-_Name ; \
    .endfunc ; \
    .cfi_endproc

#else

#define FUNCTION(_Name) \
    _Name: \
    .def _##_Name; .scl 2; .type 32; .endef ; \
    .global _##_Name ; \
    _##_Name:

#define PROTECTED_FUNCTION(_Name) FUNCTION(_Name)
#define EXPORTED_FUNCTION(_Name) FUNCTION(_Name)

#define END_FUNCTION(_Name)

#endif

