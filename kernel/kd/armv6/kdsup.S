/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    kdsup.s

Abstract:

    This module implements ARMv6 specific support routines for the Kernel
    Debugging subsystem.

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
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER
.cpu arm1176jz-s

##
## VOID
## KdpBreak (
##     VOID
##     )
##

/*++

Routine Description:

    This routine causes a break into the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION KdpBreak
    DEBUGGER_BREAK                              @ Debugger break.
    bx  %lr                                     @ Return.

END_FUNCTION KdpBreak

##
## VOID
## KdpCleanMemory (
##     PVOID Address
##     )
##

/*++

Routine Description:

    This routine cleans memory modified by the kernel debugger, flushing it
    out of the instruciton and data caches.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned.

Return Value:

    None.

--*/

FUNCTION KdpCleanMemory
    mcr     p15, 0, %r0, %cr7, %cr10, 1         @ Clean the cache line.
    mov     %r0, #0                             @
    mcr     p15, 0, %r0, %cr7, %cr5, 0          @ Invalidate instruction cache.
    mcr     p15, 0, %r0, %cr7, %cr10, 4         @ Make instructions finish.
    mcr     p15, 0, %r0, %cr7, %cr5, 4          @ Prevent speculative fetching.
    bx      %lr                                 @

END_FUNCTION KdpCleanMemory

##
## VOID
## KdpInvalidateInstructionCache (
##     VOID
##     )
##

/*++

Routine Description:

    This routine invalidates the instruction cache to PoU inner shareable.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION KdpInvalidateInstructionCache
    mov     %r0, #0                             @
    mcr     p15, 0, %r0, %cr7, %cr5, 0          @ Invalidate instruction cache.
    mcr     p15, 0, %r0, %cr7, %cr10, 4         @ Make instructions finish.
    mcr     p15, 0, %r0, %cr7, %cr5, 4          @ Prevent speculative fetching.
    bx      %lr                                 @

END_FUNCTION KdpInvalidateInstructionCache

##
## VOID
## KdpDisableInterrupts (
##     VOID
##     )
##

/*++

Routine Description:

    This routine disables all interrupts on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION KdpDisableInterrupts
    mrs     %r1, CPSR                        @ Get the status register.
    orr     %r0, %r1, #PSR_FLAG_IRQ          @ Turn on the interrupt mask bits.
    msr     CPSR_cxsf, %r0                   @ Write the status register.
    mov     %r0, #0
    mcr     p15, 0, %r0, %cr7, %cr10, 4      @ Make instructions finish.
    bx      %lr                              @

END_FUNCTION KdpDisableInterrupts

##
## --------------------------------------------------------- Internal Functions
##

