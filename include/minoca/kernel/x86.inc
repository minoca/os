/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
## ------------------------------------------------------------------ Includes
##

#include <minoca/kernel/x86defs.h>

##
## --------------------------------------------------------------- Definitions
##

##
## Basic constants.
##

#define FALSE 0
#define TRUE 1

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
#define PROCESSOR_BLOCK_RUNNING_THREAD 0x14

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
#define SIGNAL_CONTEXT_SIZE 32

##
## Thread structure offsets.
##

#define THREAD_SIGNAL_PENDING 84

##
## Define the thread signal pending state options.
##

#define ThreadSignalPending 2

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
## Define the system call number for resuming after a signal.
##

#define SystemCallRestoreContext 1
#define SIGNAL_PARAMETERS_SIZE 24
#define SIGNAL_CONTEXT_SIZE 32

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

#if defined(__WINNT__) || defined(__CYGWIN__)

#define FUNCTION(_Name) \
    _Name: \
    .def _##_Name; .scl 2; .type 32; .endef ; \
    .global _##_Name ; \
    _##_Name:

#define PROTECTED_FUNCTION(_Name) FUNCTION(_Name)
#define EXPORTED_FUNCTION(_Name) FUNCTION(_Name)

#define END_FUNCTION(_Name)

#elif defined(__ELF__)

##
## This macro defines a function, callable from C code in any module and
## capable of being overridden by other functions.
##

#define EXPORTED_FUNCTION(_Name) \
    .func _Name ; \
    .type _Name, %function ; \
    .cfi_startproc ; \
    .cfi_def_cfa %esp, 4 ; \
    .cfi_offset %eip, -4 ; \
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
    .global _Name ; \
    _Name:

#define PROTECTED_FUNCTION(_Name) FUNCTION(_Name)
#define EXPORTED_FUNCTION(_Name) FUNCTION(_Name)

#define END_FUNCTION(_Name)

#endif

##
## This macro sets the call frame information so that the debugger can unwind
## a trap frame. It assumes the CFA register is esp, and sets the CFA to the
## base of the trap frame just to make things easier.
##

#define CFI_TRAP_FRAME_PUSHED           \
    .cfi_def_cfa_offset 0 ;             \
    .cfi_offset %eax, TRAP_EAX ;        \
    .cfi_offset %ebx, TRAP_EBX ;        \
    .cfi_offset %ecx, TRAP_ECX ;        \
    .cfi_offset %edx, TRAP_EDX ;        \
    .cfi_offset %esi, TRAP_ESI ;        \
    .cfi_offset %edi, TRAP_EDI ;        \
    .cfi_offset %ebp, TRAP_EBP ;        \
    .cfi_offset %eip, TRAP_EIP ;        \
    .cfi_offset %esp, TRAP_ESP ;        \
    .cfi_offset %eflags, TRAP_EFLAGS

##
## This macro sets the call frame information just after a trap frame was
## restored. It indicates to the debugger that most registers are now in their
## proper place. It assumes the CFA register is esp+0.
##

#define CFI_TRAP_FRAME_POPPED \
    .cfi_same_value %eax ; \
    .cfi_same_value %ebx ; \
    .cfi_same_value %ecx ; \
    .cfi_same_value %edx ; \
    .cfi_same_value %esi ; \
    .cfi_same_value %edi ; \
    .cfi_same_value %ebp ; \
    .cfi_offset %eip, 4

##
## Define .cfi directives, macroed so they can be excised if unneeded.
##

#define CFI_DEF_CFA(_Register, _Offset) .cfi_def_cfa _Register, _Offset
#define CFI_DEF_CFA_OFFSET(_Offset) .cfi_def_cfa_offset _Offset
#define CFI_ADJUST_CFA_OFFSET(_Amount) .cfi_adjust_cfa_offset _Amount
#define CFI_OFFSET(_Register, _Offset) .cfi_offset _Register, _Offset
#define CFI_UNDEFINED(_Register) .cfi_undefined _Register
#define CFI_SAME_VALUE(_Register) .cfi_same_value _Register
