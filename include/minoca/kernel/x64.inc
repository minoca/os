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

#define CONTEXT_SWAP_MAGIC 0x9A8A7A6A5A4A3A2A

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
    mov     %ax, %es ;             \
    mov     $GDT_PROCESSOR, %ax ;  \
    mov     %ax, %fs ;             \

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

#else

#define FUNCTION(_Name) \
    .global _Name ; \
    _Name:

#define PROTECTED_FUNCTION(_Name) FUNCTION(_Name)
#define EXPORTED_FUNCTION(_Name) FUNCTION(_Name)

#define END_FUNCTION(_Name)

#endif

##
## Define .cfi directives, macroed so they can be excised if unneeded.
##

#define CFI_DEF_CFA(_Register, _Offset) .cfi_def_cfa _Register, _Offset
#define CFI_DEF_CFA_OFFSET(_Offset) .cfi_def_cfa_offset _Offset
#define CFI_ADJUST_CFA_OFFSET(_Amount) .cfi_adjust_cfa_offset _Amount
#define CFI_OFFSET(_Register, _Offset) .cfi_offset _Register, _Offset
#define CFI_UNDEFINED(_Register) .cfi_undefined _Register
#define CFI_SAME_VALUE(_Register) .cfi_same_value _Register

