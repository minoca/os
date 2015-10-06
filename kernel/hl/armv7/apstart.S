/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    apstart.S

Abstract:

    This module implements routines necessary to bootstrap application
    processors on ARM.

Author:

    Evan Green 10-Nov-2012

Environment:

    Kernel mode

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/arm.inc>

##
## ---------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER
.arm

##
## .globl allows these labels to be visible to the linker.
##

.globl HlpProcessorStartupEnd
.globl HlpTrampolineCode
.globl HlTrampolineTtbr0
.globl HlTrampolineSystemControlRegister
.globl HlpTrampolineCodeEnd

##
## This small amount of code lives in each processor's page, and jumps to the
## common and identity mapped code. It's expected that at the start of the
## page the PC is the ARM_PARKED_PAGE structure.
##

FUNCTION HlpProcessorStartup
    mov     %r0, %pc                    @ Get the current code address.
    lsr     %r0, %r0, #12               @ Shift down to mask off page offset.
    lsl     %r0, %r0, #12               @ Shift back up to get page base.
    add     %r0, %r0, #0x10             @ Get the base of the identity jump.
    ldr     %r1, [%r0]                  @ Load the identity jump address.
    ldr     %r0, [%r0, #4]              @ Load the processor context VA.
    blx     %r1                         @ Jump to identity mapped code.

END_FUNCTION HlpProcessorStartup
HlpProcessorStartupEnd:

##
## This represents the entry point for an embryonic processor trying to boot or
## resume. It takes one argument, which is a pointer to a PROCESSOR_CONTEXT
## structure to pass into the restore context routine.
##

HlpTrampolineCode:
    mov     %r1, %pc                    @ Get the current code address.
    sub     %r1, %r1, #8                @ Subtract to get the start address.
    mov     %r6, %r0                    @ Save processor context in R6.

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

    mcr     p15, 0, %r0, c8, c7, 0      @ TLBIALL, clear entire TLB.

    ##
    ## Write to CONTEXTIDR to set the ASID to 0, which must be consistent
    ## across all processors.
    ##

    mov     %r4, #0
    mcr     p15, 0, %r4, c13, c0, 1     @ Clear CONTEXTIDR.
    mcr     p15, 0, %r4, c2, c0, 1      @ Clear TTBR1

    ##
    ## Stash some variables into registers not trashed by the invalidate data
    ## function.
    ## R1 -> R12 - Trampoline code base address.
    ## R6 - Processor context pointer.
    ##

    mov     %r12, %r1                   @ Save R1 into R12.

    ##
    ## Invalidate the entire data cache.
    ##

    bl      HlpInvalidateDataCache
    mov     %r0, %r12                   @ Restore trampoline base address.

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
    ## It's necessary to get into some valid kernel address space so that
    ## restore processor context can actually be called.
    ##

    mcr     p15, 0, %r1, c2, c0, 0      @ Load up TTBR0.
    mov     %r1, #0                     @ Load 0.
    mcr     p15, 0, %r1, c2, c0, 2      @ Set TTBCR to 0.
    dsb                                 @ Ensure all writes have completed.
    isb                                 @ Don't prefetch past here.
    mcr     p15, 0, %r2, c1, c0, 0      @ Load up SCTLR.
    dsb                                 @ Data Synchronization barrier.
    isb                                 @ Instruction Synchronization barrier.

    ##
    ## Invalidate the instruction cache, branch predictory array, and TLB
    ## again now that translation is enabled.
    ##

    mcr     p15, 0, %r0, c7, c5, 0      @ ICIALLU, Invalidate I-Cache.
    mcr     p15, 0, %r0, c7, c5, 6      @ BPIALL, Invalidate Branch Predictor.
    mcr     p15, 0, %r0, c8, c7, 0      @ Clear the TLB, TLBIALL.
    isb                                 @ Instruction syncrhonization barrier.

    ##
    ## Restore the real processor context. Do an absolute load instead of a
    ## relative branch with link because this code has been relocated.
    ##

    mov     %r0, %r6                    @ Set up PROCESSOR_CONTEXT parameter.
    ldr     %r1, =ArRestoreProcessorContext @ Get the absolute function address.
    bx      %r1                         @ Restore context. Does not return.

    ##
    ## The restore context function does not return, so this code below will
    ## never run.
    ##

HlpTrampolineCodeImpossibleReturn:
    DEBUGGER_BREAK
    wfi
    b       HlpTrampolineCodeImpossibleReturn

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

    This routine invalidates the entire data cache by Set/Way up to the point
    of unification inner-shareable. This "function" requires no stack, but does
    use registers R0-R5, R7, R9, R10, and R11. This function is taken from the
    ARM Architecture Reference Manual.

Arguments:

    None.

Return Value:

    None.

--*/

HlpInvalidateDataCache:
    mrc     p15, 1, %r0, c0, c0, 1      @ Read CLIDR into R0.
    ands    %r3, %r0, #0xE00000         @
    mov     %r3, %r3, LSR #20           @ Cache level value (naturally aligned).
    beq     HlpInvalidateDataCacheEnd   @
    mov     %r10, #0                    @

HlpInvalidateDataCacheLoop1:
    add     %r2, %r10, %r10, LSR #1     @ Work out 3 x cache level.
    mov     %r1, %r0, LSR %r2           @ Bottom 3 bits are the Cache Type for
    and     %r1, %r1, #7                @ this level. Get those 3 bits.
    cmp     %r1, #2                     @ Check to see if there's no cache or
    blt     HlpInvalidateDataCacheSkip  @ only instruction cache at this level.
    mcr     p15, 2, %r10, c0, c0, 0     @ Write CSSELR from R10.
    isb                                 @ ISB to sync the change to CCSIDR.
    mrc     p15, 1, %r1, c0, c0, 0      @ Read current CCSIDR
    and     %r2, %r1, #7                @ Extract the line length field.
    add     %r2, %r2, #4                @ Add 4 for the line length offset
    ldr     %r4, =0x3FF                 @ (log2 16 bytes).
    ands    %r4, %r4, %r1, LSR #3       @ R4 is the max number on the way size
                                        @ (right aligned).
    clz     %r5, %r4                    @ R5 is the bit position of way size
                                        @ increment.
    mov     %r9, %r4                    @ R9 is the working copy of the max way
                                        @ size (right aligned).
HlpInvalidateDataCacheLoop2:
    ldr     %r7, =0x00007FFF            @
    ands    %r7, %r7, %r1, LSR #13      @ R7 is the max number of the index size
                                        @ (right aligned).
HlpInvalidateDataCacheLoop3:
    lsl     %r11, %r9, %r5              @ Factor in the way number and cache
    orr     %r11, %r10, %r11            @ number into R11.
    lsl     %r4, %r7, %r2               @ Factor in the
    orr     %r11, %r11, %r4             @ index number.
    mcr     p15, 0, %r11, c7, c6, 2     @ DCISW, invalidate by set/way.
    subs    %r7, %r7, #1                @ Decrement the index.
    bge     HlpInvalidateDataCacheLoop3 @
    subs    %r9, %r9, #1                @ Decrement the way number.
    bge     HlpInvalidateDataCacheLoop2 @

HlpInvalidateDataCacheSkip:
    add     %r10, %r10, #2              @ Increment the cache number.
    cmp     %r3, %r10
    bgt     HlpInvalidateDataCacheLoop1
    dsb

HlpInvalidateDataCacheEnd:
    dsb                                 @ Data Synchronization barrier.
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

