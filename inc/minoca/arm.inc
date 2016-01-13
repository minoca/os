/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    arm.inc

Abstract:

    This module contains common definitions for the ARM architecture.

Author:

    Evan Green 11-Aug-2012

Environment:

    Kernel mode

--*/

##
## --------------------------------------------------------------- Definitions
##

##
## Set this to 1 to enable Thumb-2 instructions. Better yet, this should
## really be a preprocessor macro.
##

.equ THUMB, 0

.equ NULL, 0x0

##
## Processor flags
##

##
## Program Status Register flags.
##

.equ PSR_FLAG_NEGATIVE,   0x80000000
.equ PSR_FLAG_ZERO,       0x40000000
.equ PSR_FLAG_CARRY,      0x20000000
.equ PSR_FLAG_OVERFLOW,   0x10000000
.equ PSR_FLAG_SATURATION, 0x08000000
.equ PSR_FLAG_JAZELLE,    0x01000000
.equ PSR_FLAG_THUMB,      0x00000020
.equ PSR_FLAG_IRQ,        0x00000080
.equ PSR_FLAG_FIQ,        0x00000040

##
## Processor modes
##

.equ ARM_MODE_USER,   0x00000010
.equ ARM_MODE_FIQ,    0x00000011
.equ ARM_MODE_IRQ,    0x00000012
.equ ARM_MODE_SVC,    0x00000013
.equ ARM_MODE_ABORT,  0x00000017
.equ ARM_MODE_UNDEF,  0x0000001B
.equ ARM_MODE_SYSTEM, 0x0000001F
.equ ARM_MODE_MASK,   0x0000001F

##
## Basic constants.
##

.equ FALSE, 0
.equ TRUE, 1

##
## Kernel constants.
##

.equ EXCEPTION_BREAK,             0x03
.equ EXCEPTION_SINGLE_STEP,       0x04
.equ EXCEPTION_ACCESS_VIOLATION,  0x05
.equ EXCEPTION_ASSERTION_FAILURE, 0x07

##
## Translation table base register address mask.
##
## Bit definitions are tricky for this register because they change based on
## whether or not the Multiprocessing Extensions are supported on the CPU.
##

.equ TTBR_ADDRESS_MASK,                          0x00003FFF

##
## MMU Control bits (SCTLR, CP15, register 1).
##

.equ MMU_ENABLED,                     0x00000001
.equ MMU_ALIGNMENT_FAULT_ENABLED,     0x00000002
.equ MMU_DCACHE_UNIFIED_ENABLED,      0x00000004
.equ MMU_WRITE_BUFFER_ENABLED,        0x00000008
.equ MMU_ENDIANNESS,                  0x00000080
.equ MMU_SYSTEM_PROTECTION,           0x00000100
.equ MMU_ROM_PROTECTION,              0x00000200
.equ MMU_BRANCH_PREDICTION_ENABLED,   0x00000800
.equ MMU_ICACHE_ENABLED,              0x00001000
.equ MMU_HIGH_EXCEPTION_VECTORS,      0x00002000
.equ MMU_PREDICTABLE_REPLACEMENT,     0x00004000
.equ MMU_DISABLE_THUMB_DEPRECATED,    0x00008000
.equ MMU_FAST_INTERRUPTS,             0x00200000
.equ MMU_UNALIGNED_ACCESS_ENABLED,    0x00400000
.equ MMU_VMSA6_ENABLED,               0x00800000
.equ MMU_VECTORED_INTERRUPTS_ENABLED, 0x01000000
.equ MMU_EXCEPTION_ENDIAN,            0x02000000
.equ MMU_THUMB_EXCEPTIONS,            0x40000000

.equ MMU_CONTROL_DEFAULT_VALUE,      (0x00C0180D)

##
## ARMv6 auxiliary control register bits (ACTLR).
##

.equ ARMV6_AUX_16K_CACHE_SIZE, 0x00000040

##
## Cortex A17 auxiliary control register bits (ACTLR).
##

.equ CORTEX_A17_AUX_SMP_ENABLE, 0x00000040

##
## Nonsecure Access Control Register bits (CP15, register 1, opcode2 2).
##

.equ MMU_TLB_LOCKING_ENABLE,          0x00020000

##
## Definition for the structure on the exception stacks.
##

.equ EXCEPTION_STACK_R0,   0
.equ EXCEPTION_STACK_CPSR, 4
.equ EXCEPTION_STACK_SPSR, 8
.equ EXCEPTION_STACK_LR,   12

.equ EXCEPTION_STACK_SIZE, 20

.equ EXCEPTION_STACK_R0,   0
.equ EXCEPTION_STACK_CPSR, 4

.equ EXCEPTION_STACK_SIZE, 8

##
## -------------------------------------------------------------------- Macros
##

##
## This macro goes at the top of an assembly file.
## .text specifies that this code belongs in the executable section.
## .thumb specifies this is Thumb code (not ARM code).
## .syntax unified turns on Unified Assembly Language which is required to
## enable Thumb-2 features.
## .align 4 ensures functions are emitted on proper boundaries.
##

.macro ASSEMBLY_FILE_HEADER
    .text
    .if THUMB
        .thumb
        .syntax unified
    .else
        .arm
    .endif
.endm

##
## This macro defines a function, callable from C code.
##

.macro FUNCTION FunctionName
    .func \FunctionName
    .type \FunctionName, %function
    .if THUMB
        .thumb_func
    .endif
    .globl \FunctionName
    \FunctionName:
.endm

##
## This macro defines the end of a function.
##

.macro END_FUNCTION FunctionName
    .ltorg
    .size \FunctionName, .-\FunctionName
    .endfunc
.endm

##
## Define a macro used to generate the debugger break instruction.
##

.macro DEBUGGER_BREAK
    .if THUMB
        .hword 0xDE20
    .else
        .word 0xE7F000F3
    .endif
.endm

##
## Define a macro used to generate the debug service instruction.
##

.macro DEBUG_SERVICE
    .if THUMB
        .hword 0xDE24
    .else
        .word 0xE7F000F4
    .endif
.endm

##
## This macro can be run immediately after an exception or interrupt. It
## switches back to SVC mode and creates a trap frame. This uses several
## instructions from ARMv6T2, including srs, cpsid #mode, and clrex.
##

.macro ARM_ENTER_INTERRUPT
    srsdb   %sp!, #ARM_MODE_SVC                 @ Push lr and spsr to SVC.
    mrs     %lr, cpsr                           @ Get exception mode CPSR.
    stmdb   %sp, {%r0, %lr}                     @ Push R0 and exception CPSR.
    sub     %r0, %sp, #8                        @ Remember exception stack.
    cpsid   i, #ARM_MODE_SVC                    @ Switch to SVC mode.
    stmdb   %sp!, {%r1-%r12, %lr}               @ Push general registers.
    ldmia   %r0, {%r1, %r2}                     @ Get R0 and exception CPSR.
    stmdb   %sp!, {%r1, %r2}                    @ Save onto trap frame.
    mov     %r0, %sp                            @ Get SVC stack.
    sub     %sp, %sp, #12                       @ Allocate space.
    cpsid   i, #ARM_MODE_SYSTEM                 @ Move to system mode.
    str     %lr, [%r0, #-4]                     @ Save usermode SP.
    str     %sp, [%r0, #-8]                     @ Save usermode LR.
    cpsid   i, #ARM_MODE_SVC                    @ Switch back to SVC mode.
    str     %sp, [%sp]                          @ Save SVC stack pointer.
    clrex                                       @ Clear exclusive monitors.

.endm

##
## This macro can be used for an interrupt or exception that was entered with
## ARM_INTERRUPT_ENTER. It restores the state in the trap frame and returns
## from the exception.
##

.macro ARM_EXIT_INTERRUPT

    ##
    ## Restore the stack pointer. The rest of the context better be at the new
    ## stack location. Then restore the user mode sp and lr.
    ##

    ldr     %sp, [%sp]                          @ Restore stack pointer.
    add     %sp, %sp, #4                        @ Pop stack pointer.

    ##
    ## Restore the user mode stack and link registers.
    ##

    mov     %r0, %sp                            @ Get SVC stack pointer.
    cpsid   i, #ARM_MODE_SYSTEM                 @ Switch to system mode.
    ldr     %sp, [%r0]                          @ Restore usermode SP.
    ldr     %lr, [%r0, #4]                      @ Resotre usermode LR.
    cpsid   i, #ARM_MODE_SVC                    @ Switch back to svc mode.
    ldr     %r0, [%sp, #8]                      @ Restore R0.
    add     %sp, %sp, #16                       @ Pop up to R1.
    ldmia   %sp!, {%r1-%r12, %lr}               @ Restore general registers.
    rfeia   %sp!                                @ Restore PC and CPSR.

.endm

