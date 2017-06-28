/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    x64.inc

Abstract:

    This module contains common definitions for the 64-bit x86 architecture.

Author:

    Evan Green 17-Jan-2015

Environment:

    Any

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/x86defs.h>

//
// --------------------------------------------------------------- Definitions
//

//
// Basic constants.
//

#define FALSE 0
#define TRUE 1

#define EXCEPTION_NMI               0x02
#define EXCEPTION_BREAK             0x03
#define EXCEPTION_SINGLE_STEP       0x04
#define EXCEPTION_ACCESS_VIOLATION  0x05
#define EXCEPTION_ASSERTION_FAILURE 0x07
#define EXCEPTION_DOUBLE_FAULT      0x0C

#define CONTEXT_SWAP_MAGIC 0x5A4A3A2A

//
// Definition for the TRAP_FRAME structure and the exception stack directly
// above it.
//

#define TRAP_DS             0
#define TRAP_ES             4
#define TRAP_FS             8
#define TRAP_GS             12
#define TRAP_PADDING        16
#define TRAP_RAX            24
#define TRAP_RBX            32
#define TRAP_RCX            40
#define TRAP_RDX            48
#define TRAP_RSI            56
#define TRAP_RDI            64
#define TRAP_RBP            72
#define TRAP_R8             80
#define TRAP_R9             88
#define TRAP_R10            96
#define TRAP_R11            104
#define TRAP_R12            112
#define TRAP_R13            120
#define TRAP_R14            128
#define TRAP_R15            136
#define TRAP_ERRORCODE      144
#define TRAP_RIP            152
#define TRAP_CS             160
#define TRAP_RFLAGS         168
#define TRAP_RSP            176
#define TRAP_SS             184

#define TRAP_FRAME_SIZE     192

//
// TODO: Fix these sizes.
//

#define PROCESSOR_CONTEXT_SIZE 0x60
#define SIGNAL_CONTEXT_SIZE 28

//
// Define the minimum and maximum external interrupt vectors.
//

#define MINIMUM_VECTOR 0x30
#define MAXIMUM_VECTOR 0xFF

//
// APIC End Of Interrupt Offset.
//

#define APIC_EOI_OFFSET 0x0B

//
// -------------------------------------------------------------------- Macros
//

//
// This macro goes at the start of every assembly file.
//

#define ASSEMBLY_FILE_HEADER \
    .text ; \
    .code64

//
// This macro loads DS, ES, and GS with the correct values for the kernel.
//

#define LOAD_KERNEL_DATA_SEGMENTS  \
    movw    $KERNEL_DS, %ax ;      \
    mov     %ax, %ds ;             \
    mov     %ax, %ss ;             \
    mov     %ax, %es ;             \
    xorl    %eax, %eax ;           \
    mov     %ax, %fs ;             \
    mov     %ax, %gs ;

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
    .cfi_def_cfa %rsp, 8 ; \
    .cfi_offset %rip, -8 ; \
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

#elif defined(__APPLE__)

#define FUNCTION(_Name) \
    .global _##_Name ; \
    _##_Name:

#define PROTECTED_FUNCTION(_Name) FUNCTION(_Name)
#define EXPORTED_FUNCTION(_Name) FUNCTION(_Name)

#define END_FUNCTION(_Name)

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
    .cfi_offset %rax, TRAP_RAX ;        \
    .cfi_offset %rbx, TRAP_RBX ;        \
    .cfi_offset %rcx, TRAP_RCX ;        \
    .cfi_offset %rdx, TRAP_RDX ;        \
    .cfi_offset %rsi, TRAP_RSI ;        \
    .cfi_offset %rdi, TRAP_RDI ;        \
    .cfi_offset %rbp, TRAP_RBP ;        \
    .cfi_offset %r8, TRAP_R8   ;        \
    .cfi_offset %r9, TRAP_R9   ;        \
    .cfi_offset %r10, TRAP_R10 ;        \
    .cfi_offset %r11, TRAP_R11 ;        \
    .cfi_offset %r12, TRAP_R12 ;        \
    .cfi_offset %r13, TRAP_R13 ;        \
    .cfi_offset %r14, TRAP_R14 ;        \
    .cfi_offset %r15, TRAP_R15 ;        \
    .cfi_offset %rip, TRAP_RIP ;        \
    .cfi_offset %rsp, TRAP_RSP ;        \
    .cfi_offset %rflags, TRAP_RFLAGS

##
## This macro sets the call frame information just after a trap frame was
## restored. It indicates to the debugger that most registers are now in their
## proper place. It assumes the CFA register is esp+0.
##

#define CFI_TRAP_FRAME_POPPED \
    .cfi_same_value %rax ; \
    .cfi_same_value %rbx ; \
    .cfi_same_value %rcx ; \
    .cfi_same_value %rdx ; \
    .cfi_same_value %rsi ; \
    .cfi_same_value %rdi ; \
    .cfi_same_value %rbp ; \
    .cfi_same_value %r8  ; \
    .cfi_same_value %r9  ; \
    .cfi_same_value %r10 ; \
    .cfi_same_value %r11 ; \
    .cfi_same_value %r12 ; \
    .cfi_same_value %r13 ; \
    .cfi_same_value %r14 ; \
    .cfi_same_value %r15 ; \
    .cfi_offset %rip, 0

##
## Define .cfi directives, macroed so they can be excised if unneeded.
##

#define CFI_DEF_CFA(_Register, _Offset) .cfi_def_cfa _Register, _Offset
#define CFI_DEF_CFA_OFFSET(_Offset) .cfi_def_cfa_offset _Offset
#define CFI_ADJUST_CFA_OFFSET(_Amount) .cfi_adjust_cfa_offset _Amount
#define CFI_OFFSET(_Register, _Offset) .cfi_offset _Register, _Offset
#define CFI_UNDEFINED(_Register) .cfi_undefined _Register
#define CFI_SAME_VALUE(_Register) .cfi_same_value _Register

