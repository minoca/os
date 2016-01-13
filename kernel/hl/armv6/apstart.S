/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    apstart.S

Abstract:

    This module implements routines necessary to bootstrap application
    processors on ARMv6.

Author:

    Chris Stevens 2-Feb-2014

Environment:

    Kernel mode

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/arm.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## Define offsets into the PROCESSOR_START_BLOCK structure.
##

.equ PROCESSOR_START_BLOCK_STACK_BASE, 0x0
.equ PROCESSOR_START_BLOCK_STACK_SIZE, 0x4

##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER
.cpu arm1176jz-s
.arm

##
## .globl allows these labels to be visible to the linker.
##

.globl HlpTrampolineCode
.globl HlTrampolineTtbr0
.globl HlTrampolineSystemControlRegister
.globl HlpTrampolineCodeEnd

##
## This represents the entrypoint for an embryonic processor trying to boot.
## R0 should contain the physical address of this code.
##

HlpTrampolineCode:

    ##
    ## Disable interrupts and switch to SVC mode.
    ##

    mov     %r2, #(PSR_FLAG_IRQ | ARM_MODE_SVC)
    msr     CPSR_c, %r2

    ##
    ## Invalidate the entire instruction cache and branch predictor array.
    ##

    mcr     p15, 0, %r0, c7, c5, 0      @ ICIALLU, Invalidate I-Cache.
    mcr     p15, 0, %r0, c7, c5, 6      @ BPIALL, Invalidate Branch Predictor.

    ##
    ## Clear the TLB.
    ##

    mcr     p15, 0, %r0, c8, c7, 0      @ Clear the TLB.

    ##
    ## Write to CONTEXTIDR to set the ASID to 0, which must be consistent
    ## across all processors.
    ##

    mov     %r4, #0
    mcr     p15, 0, %r4, c13, c0, 1

    ##
    ## Stash R0 into R12 temporarily while this function is called. Normally
    ## R12 is volatile, but this upcoming "function" uses no stack and only
    ## trashes specific registers.
    ##

    mov     %r12, %r0

    ##
    ## Invalidate the entire data cache.
    ##

    bl      HlpInvalidateDataCache
    mov     %r0, %r12                   @ Restore R0.

    ##
    ## Load the variables. These must be relative loads to work, as the actual
    ## address is moved around from its original location. Start by loading the
    ## original address of this code. Then for every variable retrieved,
    ## subtract the original address of this code to get the offset from the
    ## new address. Then add the new address to get the actual address of the
    ## given variable.
    ##

    ldr     %r3, =HlpTrampolineCode     @ Load the original code address.
    ldr     %r1, =HlTrampolineTtbr0     @ Load the original TTBR0 address.
    sub     %r1, %r1, %r3               @ Subtract original offset.
    add     %r1, %r1, %r0               @ Add new address.
    ldr     %r1, [%r1]                  @ Load the variable.
    ldr     %r2, =HlTrampolineSystemControlRegister
    sub     %r2, %r2, %r3               @ Subtract original offset.
    add     %r2, %r2, %r0               @ Add new address.
    ldr     %r2, [%r2]                  @ Load the variable.

    ##
    ## Write to CP15, register 3 to set up domain access control for domain 0
    ## as a client, which means use the translation table's access control.
    ##

    mov     %r3, #1                     @ Set Domain 0 access to Client.
    mcr     p15, 0, %r3, c3, c0, 0      @ Write DACR.

    ##
    ## Load the TTBR0 and then SCTLR registers up. This enables translation.
    ##

    mcr     p15, 0, %r1, c2, c0, 0      @ Load up TTBR0.
    mov     %r1, #0                     @ Load 0.
    mcr     p15, 0, %r1, c2, c0, 2      @ Set TTBCR to 0.
    mcr     p15, 0, %r2, c1, c0, 0      @ Load up SCTLR.
    mcr     p15, 0, %r1, %cr7, %cr10, 4 @ Data Synchronization barrier.
    mcr     p15, 0, %r1, %cr7, %cr5, 4  @ Instruction Synchronization barrier.

    ##
    ## Invalidate the instruction cache, branch predictory array, and TLB
    ## again now that translation is enabled.
    ##

    mcr     p15, 0, %r1, c7, c5, 0      @ ICIALLU, Invalidate I-Cache.
    mcr     p15, 0, %r1, c7, c5, 6      @ BPIALL, Invalidate Branch Predictor.
    mcr     p15, 0, %r1, c8, c7, 0      @ Clear the TLB.
    mcr     p15, 0, %r1, %cr7, %cr5, 4  @ Instruction Synchronization barrier.

    ##
    ## Get the processor start block structure.
    ##

    ldr     %r0, =HlProcessorStartBlock @ Get address of start block variable.
    ldr     %r0, [%r0]                  @ Get the value of the variable.

    ##
    ## Get the stack base and stack size, and set up a stack.
    ##

    ldr     %r1, [%r0, #PROCESSOR_START_BLOCK_STACK_BASE]   @ Load stack base.
    ldr     %r2, [%r0, #PROCESSOR_START_BLOCK_STACK_SIZE]   @ Load stack size.
    add     %sp, %r1, %r2               @ Load the top of the stack.
    sub     %sp, %sp, #4                @ Adjust by a word.

    ##
    ## Get off of the temporarily mapped startup stub and move to the original
    ## (and permanent) mapping of this code.
    ##

    ldr     %r7, =HlpTrampolineCodeAtOriginalLocation @ Get original location
    bx      %r7                         @ Jump to non identity mapped version.

HlpTrampolineCodeAtOriginalLocation:

    ##
    ## Grab the address of the initialization routine before declaring
    ## victory (and allowing the processor starting this one to clear that
    ## variable).
    ##

    ldr     %lr, =HlProcessorInitializationRoutine
    ldr     %lr, [%lr]                  @ Get the value in the pointer.

    ##
    ## Increment the processors launched variable to let the hardware library
    ## know this processor is alive.
    ##

    ldr     %r3, =HlProcessorsLaunched
    ldr     %r4, [%r3]
    add     %r4, %r4, #1
    str     %r4, [%r3]
    mov     %r1, #0
    mcr     p15, 0, %r1, %cr7, %cr10, 4 @ Data Synchronization barrier.

    ##
    ## Call the main initialization routine. The routine takes one parameter,
    ## a pointer to the processor start block, which is already in R0.
    ##

    bx      %lr                         @ Jump to that routine.

##
## --------------------------------------------------------- Internal Functions
##

##
## VOID
## HlpInvalidateDataCache (
##     VOID
##     )
##

/*++

Routine Description:

    This routine invalidates the entire data cache by Set/Way. This "function"
    requires no stack, but does corrupt non-volatile registers. This function
    is taken from the ARM Architecture Reference Manual.

Arguments:

    None.

Return Value:

    None.

--*/

HlpInvalidateDataCache:
    mov     %r0, #0
    mcr     p15, 0, %r0, %cr7, %cr6, 0  @ Invalidate entire data cache.
    mcr     p15, 0, %r0, %cr7, %cr10, 4 @ Data Synchronization barrier.
    bx      %lr

##
## Dump any literals out now (before the trampoline end).
##

.ltorg

##
## -------------------------------------------------------------------- Globals
##

##
## Define a global that contains the Translation Table Base register.
##

HlTrampolineTtbr0:
    .long   0

##
## Define a global that contains the system control register value.
##

HlTrampolineSystemControlRegister:
    .long   0

##
## This label represents the end of the trampoline code and data.
##

HlpTrampolineCodeEnd:

