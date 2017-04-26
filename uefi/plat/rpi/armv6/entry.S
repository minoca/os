/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    entry.S

Abstract:

    This module implements the initial entry point into the ARMv6 firmware. Its
    job is to do essential initialization and call the C main function.

Author:

    Chris Stevens 31-Dec-2014

Environment:

    Firmware

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/arm.inc>

##
## ---------------------------------------------------------------- Definitions
##

.equ STACK_SIZE,    0x4000

##
## .text specifies that this code belongs in the executable section. This is
## the only section in the MBR code, data also lives in the text section.
## .arm specifies that this code should be compiled in ARM mode.
##

.text
.arm

##
## Stick this in the .init section so it ends up at the front of the binary.
##

.section .init

##
## .global allows this label to be visible to the linker. _start is the entry
## point to the MBR code, so it needs to be globally visible.
##

.global _start

##
## This is the entry point for the ARM firmware. It changes to SVC mode,
## sets up the initial stack, and jumps to the firmware core.
##

_start:

    ##
    ## Disable interrupts and switch to SVC mode.
    ##

    mov     %r2, #(PSR_FLAG_IRQ | ARM_MODE_SVC)
    msr     CPSR_c, %r2

    ##
    ## Limit the cache to 16K until page coloring is supported.
    ##

    mrc     p15, 0, %r0, %cr1, %cr0, 1
    orr     %r0, %r0, #ARMV6_AUX_16K_CACHE_SIZE
    mcr     p15, 0, %r0, %cr1, %cr0, 1

    ##
    ## Flip some essential MMU bits allowing unaligned accesses.
    ##

    mrc     p15, 0, %r0, %cr1, %cr0, 0
    bic     %r0, %r0, #MMU_ALIGNMENT_FAULT_ENABLED
    orr     %r0, %r0, #MMU_UNALIGNED_ACCESS_ENABLED
    mcr     p15, 0, %r0, %cr1, %cr0, 0

    ##
    ## Zero out the BSS section.
    ##

    ldr     %r1, =__bss_start__
    ldr     %r2, =__bss_end__
    mov     %r0, #0

BssZeroLoop:
    str     %r0, [%r1], #4
    cmp     %r1, %r2
    blt     BssZeroLoop

    ##
    ## The stack starts at the image base and works downwards.
    ##

    adr     %r0, _start             @ Get the current address for the stack top.
    ldr     %r1, =STACK_SIZE        @ Get the stack size.
    mov     %sp, %r0                @ Set the stack.
    mov     %r11, #0                @ Zero the ARM frame pointer.
    mov     %r7, #0                 @ Zero out the Thumb frame pointer.
    mov     %r2, %r7                @ Zero out another temporary register.
    push    {%r2, %r7}              @ Create an empty stack frame for debugging.
    blx     EfiRpiMain

LoopForever:
    b       LoopForever

