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

.equ IA32_EFLAG_TF,        0x00000100
.equ IA32_EFLAG_IF,        0x00000200
.equ IA32_EFLAG_IOPL_MASK, 0x00003000
.equ IA32_EFLAG_VM,        0x00020000
.equ IA32_EFLAG_VIF,       0x00080000

.equ SEGMENT_PRIVILEGE_MASK, 0x0003

##
## CR0 flags
##

.equ CR0_PAGING_ENABLE,        0x80000000
.equ CR0_CACHE_DISABLE,        0x40000000
.equ CR0_NOT_WRITE_THROUGH,    0x20000000
.equ CR0_WRITE_PROTECT_ENABLE, 0x00010000
.equ CR0_X87_EXCEPTIONS,       0x00000020
.equ CR0_TASK_SWITCHED,        0x00000008
.equ CR0_EMULATE_FPU,          0x00000004
.equ CR0_MONITOR_COPROCESSOR,  0x00000002
.equ CR0_PROTECTED_MODE,       0x00000001

.equ CR0_OR_MASK, 0x8001002B
.equ CR0_AND_MASK, ~(0x60000004)

##
## Basic constants.
##

.equ FALSE, 0
.equ TRUE, 1
.equ NULL, 0

##
## Kernel constants.
##

.equ KERNEL_CS,        0x08
.equ KERNEL_DS,        0x10
.equ USER_CS,          (0x18 | 3)
.equ USER_DS,          (0x20 | 3)
.equ GDT_PROCESSOR,    0x28
.equ GDT_THREAD,       (0x30 | 3)
.equ KERNEL_TSS,       0x38
.equ DOUBLE_FAULT_TSS, 0x40
.equ NMI_TSS,          0x48
.equ GDT_ENTRIES,      10

.equ EXCEPTION_NMI,               0x02
.equ EXCEPTION_BREAK,             0x03
.equ EXCEPTION_SINGLE_STEP,       0x04
.equ EXCEPTION_ACCESS_VIOLATION,  0x05
.equ EXCEPTION_ASSERTION_FAILURE, 0x07
.equ EXCEPTION_DOUBLE_FAULT,      0x0C

.equ CONTEXT_SWAP_MAGIC, 0x5A4A3A2A

##
## Relevant TSS structure definitions.
##

.equ TSS_ESP0,  0x04

##
## Processor block offsets.
##

.equ PROCESSOR_BLOCK_TSS, 0x0C
.equ PROCESSOR_BLOCK_GDT, 0x10

##
## Definition for the TRAP_FRAME structure and the exception stack directly
## above it.
##

.equ TRAP_DS,             0
.equ TRAP_ES,             4
.equ TRAP_FS,             8
.equ TRAP_GS,             12
.equ TRAP_SS,             16
.equ TRAP_EAX,            20
.equ TRAP_EBX,            24
.equ TRAP_ECX,            28
.equ TRAP_EDX,            32
.equ TRAP_ESI,            36
.equ TRAP_EDI,            40
.equ TRAP_EBP,            44
.equ TRAP_ERRORCODE,      48
.equ TRAP_EIP,            52
.equ TRAP_CS,             56
.equ TRAP_EFLAGS,         60
.equ TRAP_ESP,            64
.equ TRAP_RET_ERRORCODE,  68
.equ TRAP_RET_EIP,        72
.equ TRAP_RET_CS,         76
.equ TRAP_RET_EFLAGS,     80
.equ TRAP_RET_ESP,        84
.equ TRAP_RET_SS,         88
.equ TRAP_RET_ES,         92
.equ TRAP_RET_DS,         96
.equ TRAP_RET_FS,         100
.equ TRAP_RET_GS,         104

.equ TRAP_FRAME_SIZE,     68

##
## Define the minimum and maximum external interrupt vectors.
##

.equ MINIMUM_VECTOR, 0x30
.equ MAXIMUM_VECTOR, 0xFF

##
## APIC End Of Interrupt Offset.
##

.equ APIC_EOI_OFFSET,    0xB

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

##
## This macro defines a function declaration, callable from C code.
##

#if defined(__ELF__)

#define FUNCTION(_Name) \
    .func _Name ; \
    .type _Name, %function ; \
    .global _Name ; \
    _Name:

#define END_FUNCTION(_Name) \
    .size _Name, .-_Name ; \
    .endfunc

#else

#define FUNCTION(_Name) \
    _Name: \
    .def _##_Name; .scl 2; .type 32; .endef ; \
    .global _##_Name ; \
    _##_Name:

#define END_FUNCTION(_Name)

#endif

