/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    kernxfr.S

Abstract:

    This module implements the code necessary to transfer execution from the
    loader to the kernel on ARM systems.

Author:

    Evan Green 15-Aug-2012

Environment:

    Boot

--*/

##
## ------------------------------------------------------------------ Includes
##

#include <minoca/arm.inc>

##
## --------------------------------------------------------------- Definitions
##

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## VOID
## BoEnablePaging (
##     VOID
##     )
##

/*++

Routine Description:

    This routine turns paging on in the processor.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION BoEnablePaging
    stmdb   %sp!, {%lr}
    bl      BoInvalidateEntireCache
    mov     %r0, #0

    ##
    ## Write to CP15, register 8 to clear the entire instruction and data TLB.
    ##

    mcr     p15, 0, %r0, %cr8, %cr7, 0

    ##
    ## Write to CONTEXTIDR to set the ASID to 0, which must be consistent
    ## across all processors.
    ##

    mcr     p15, 0, %r0, c13, c0, 1

    ##
    ## Write to CP15, register 3 to set up domain access control for domain 0
    ## as a client, which means use the translation table's access control.
    ##

    mov     %r0, #1
    mcr     p15, 0, %r0, %cr3, %cr0, 0

    ##
    ## Write to CP15, register 2 to set up the first level table base.
    ## An opcode2 of 2 sets the control register (TTBCR), which is being set to
    ## 0 to always use TTBR0. An opcode2 of 0 sets TTBR0, the Translation Table
    ## Base Register.
    ##

    mov     %r0, #0
    mcr     p15, 0, %r0, %cr2, %cr0, 2
    ldr     %r0, =BoFirstLevelTable
    ldr     %r0, [%r0]
    ldr     %r1, =TtbrAddressMask
    ldr     %r1, [%r1]
    bic     %r0, %r0, %r1
    ldr     %r1, =BoTtbrCacheAttributes
    ldr     %r1, [%r1]
    orr     %r0, %r0, %r1
    mcr     p15, 0, %r0, %cr2, %cr0, 0

    ##
    ## Enable the MMU.
    ##

    ldr     %r0, =MmuValue
    ldr     %r0, [%r0]
    mcr     p15, 0, %r0, %cr1, %cr0, 0
    isb

    ##
    ## Invalidate instruction caches and flush the branch target cache.
    ##

    mcr     p15, 0, %r0, %cr7, %cr5, 0  @ Write to ICIALLU, flush I-cache.

    ##
    ## Paging is now enabled. Return.
    ##

    ldmia   %sp!, {%lr}
    bx      %lr

##
## Constants for BoEnablePaging.
##

TtbrAddressMask: .word TTBR_ADDRESS_MASK
MmuValue: .word MMU_CONTROL_DEFAULT_VALUE

END_FUNCTION BoEnablePaging

##
## VOID
## BoTransferToKernelAsm (
##     PVOID Parameters,
##     PVOID EntryPoint,
##     PVOID StackAddress
##     )
##

/*++

Routine Description:

    This routine transfers control of execution to the kernel.

Arguments:

    Parameters - Supplies the parameter block to pass on to the kernel for its
        initialization.

    EntryPoint - Supplies the entry point of the kernel. The function will end
        with a jump to here.

    StackAddress - Supplies the top of the kernel stack. The stack pointer will
        get set to this value just before the kernel is launched.

Return Value:

    None. This function does not return.

--*/

FUNCTION BoTransferToKernelAsm
    mov     %sp, %r2                    @ Switch to the new stack.
    mov     %fp, #0                     @ Reset the ARM frame pointer.
    mov     %r7, #0                     @ Reset the Thumb frame pointer.
    mov     %lr, #0                     @ Reset the link register.
    bx      %r1                         @ Jump to the entry point.

END_FUNCTION BoTransferToKernelAsm

