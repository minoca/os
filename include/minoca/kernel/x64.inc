/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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
// --------------------------------------------------------------- Definitions
//

//
// Basic constants.
//

#define FALSE 0
#define TRUE 1
#define NULL 0

//
// Kernel constants.
//

#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS (0x18 | 3)
#define USER_DS (0x20 | 3)
#define GDT_PROCESSOR 0x28
#define GDT_THREAD (0x30 | 3)
#define KERNEL_TSS 0x38
#define NMI_TSS 0x48
#define GDT_ENTRIES 10

#define EXCEPTION_NMI 0x02
#define EXCEPTION_BREAK 0x03
#define EXCEPTION_SINGLE_STEP 0x04
#define EXCEPTION_ACCESS_VIOLATION 0x05
#define EXCEPTION_ASSERTION_FAILURE 0x07
#define EXCEPTION_DOUBLE_FAULT 0x0C

#define CONTEXT_SWAP_MAGIC 0x9A8A7A6A5A4A3A2A

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
    .text

//
// This macro loads DS, ES, and GS with the correct values for the kernel.
//

#define LOAD_KERNEL_DATA_SEGMENTS  \
    movw    $KERNEL_DS, %ax ;      \
    mov     %ax, %ds ;             \
    mov     %ax, %es ;             \
    mov     $GDT_PROCESSOR, %ax ;  \
    mov     %ax, %fs ;             \

##
## This macro defines a function, callable from C code in any module and
## capable of being overridden by other functions.
##

#define EXPORTED_FUNCTION(_Name) \
    .func _Name ; \
    .type _Name, %function ; \
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
    .endfunc

