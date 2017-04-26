/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    start.S

Abstract:

    This module implements the entry point for the first stage loader on
    OMAP4 SoCs.

Author:

    Evan Green 1-Apr-2014

Environment:

    Firmware

--*/

##
## ------------------------------------------------------------------- Includes
##

##
## ---------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

##
## .text specifies that this code belongs in the executable section.
##
## .arm specifies that this is ARM code, not Thumb code.
##
## .align 4 specifies that ARM instructions must be word aligned.
##

.text
.arm
.align 4

##
## Stick this in the .init section so it ends up at the front of the binary.
##

.section .init

##
## .globl allows this label to be visible to the linker.
##

.globl _start
.globl _end
.globl EfiDisableInterrupts
.globl EfiEnableInterrupts

.func _start

##
## VOID
## _start (
##     )
##

/*++

Routine Description:

    This routine is the default entry point for a user mode program in the ELF
    binary format. It is responsible for initializing the C library and
    calling main.

Arguments:

    None.

Return Value:

    None.

--*/

_start:

    ##
    ## Copy the image whereever it was loaded to where it was supposed to be
    ## loaded.
    ##

    adr     %r7, _start             @ Get the current address.
    adr     %r8, ImageSize          @ Get the image size address (relatively).
    ldr     %r8, [%r8]              @ Get the image size.
    add     %r7, %r7, %r8           @ Add size to start to get image end.
    ldr     %r8, =_start            @ Get correct address of image start.
    ldr     %r9, =_end              @ Get correct address of image end.

CopyImage:
    ldr     %r6, [%r7], #-4         @ Load a word from the source.
    str     %r6, [%r9], #-4         @ Copy that word to the destination.
    cmp     %r9, %r8                @ Finished?
    bne     CopyImage               @ If not, keep copying.

    ##
    ## Jump to the C routine.
    ##

    ldr     %r8, =EfiFirstStageLoader   @ Get C routine address, absolute.
    bx      %r8                     @ Jump to destination image.

DeadLoop:
    b       DeadLoop                @ Spin forever.

ImageSize:
    .word   _end - _start

.ltorg

##
## BOOLEAN
## EfiDisableInterrupts (
##     VOID
##     )
##

/*++

Routine Description:

    This routine disables all interrupts on the current processor.

Arguments:

    None.

Return Value:

    TRUE if interrupts were previously enabled on the processor.

    FALSE if interrupts were not previously enabled on the processor.

--*/

EfiDisableInterrupts:
    mrs     %r1, CPSR               @ Get the status register.
    orr     %r0, %r1, #0x80         @ Turn on the interrupt mask bits.
    msr     CPSR_cxsf, %r0          @ Write the status register.
    cmp     %r0, %r1                @ Were interrupts previously enabled?
    moveq   %r0, #0                 @ If not, return FALSE.
    movne   %r0, #1                 @ If enabled, return TRUE.
    bx      %lr                     @

##
## VOID
## EfiEnableInterrupts (
##     VOID
##     )
##

/*++

Routine Description:

    This routine enables interrupts on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/

EfiEnableInterrupts:
    mrs     %r0, CPSR               @ Get the status register.
    bic     %r0, %r0, #0x80         @ Clear the interrupt bits.
    msr     CPSR_cxsf, %r0          @ Write the status register.
    bx      %lr                     @

