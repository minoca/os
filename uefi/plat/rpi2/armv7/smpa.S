/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    smpa.S

Abstract:

    This module implements assembly routines necessary for booting the
    application processors on the BCM2836.

Author:

    Chris Stevens 19-Apr-2015

Environment:

    Firmware

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

##
## .globl allows these labels to be visible to the linker.
##

.globl EfipBcm2836ProcessorStartup
.globl EfipBcm2836ParkingLoop
.globl EfipBcm2836ParkingLoopEnd

##
## VOID
## EfipBcm2836SendEvent (
##     VOID
##     )
##

/*++

Routine Description:

    This routine executes a SEV instruction, which is a hint instruction that
    causes an event to be signalled to all processors.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION EfipBcm2836SendEvent
    DSB                             @ Data Synchronization Barrier.
    sev                             @ Send Event.
    bx      %lr                     @ Return.

END_FUNCTION EfipBcm2836SendEvent

##
## VOID
## EfipBcm2836ProcessorStartup (
##     VOID
##     )
##

/*++

Routine Description:

    This routine implements the startup routine for the second CPU on the TI
    OMAP4. Since this is the very first set of instructions executed on this
    core there is nothing set up, including a stack.

Arguments:

    None.

Return Value:

    None. This function does not return, as there is nothing to return to.

--*/

.arm
.align 4
EfipBcm2836ProcessorStartup:
    mov     %r3, #0
    ldr     %r1, =EfiBcm2836ProcessorId     @ Get the processor ID address.
    ldr     %r0, [%r1]                      @ Get the value.
    str     %r3, [%r1]                      @ Zero the value.

EfipBcm2836ProcessorStartupLoop:
    DSB                                     @ Data synchronization barrier.
    ldr     %r2, [%r1]                      @ Load the processor ID.
    cmp     %r0, %r2
    beq     EfipBcm2836ProcessorStartupEnd  @ Move to the jump if it's real.
    wfe                                     @ Wait for an event.
    b       EfipBcm2836ProcessorStartupLoop @ Try again.

EfipBcm2836ProcessorStartupEnd:
    ldr     %r1, =EfiBcm2836JumpAddress     @ Get the jump address.
    ldr     %r2, [%r1]                      @ Get the value.
    str     %r3, [%r1]                      @ Store zero into jump address.
    ldr     %r3, =0xFFFFF000                @ Get the page mask.
    and     %r1, %r2, %r3                   @ Set the parking location.
    DSB                                     @ One final breath, then...
    bx      %r2                             @ Jump head first into the abyss.

.ltorg

##
## VOID
## EfipBcm2836ParkingLoop (
##     UINT32 ProcessorId,
##     VOID *ParkingLocation
##     )
##

/*++

Routine Description:

    This routine implements the MP parking protocol loop.

Arguments:

    ProcessorId - Supplies the ID of this processor.

    ParkingLocation - Supplies the parking protocol mailbox base.

Return Value:

    None. This function does not return, it launches the core.

--*/

EfipBcm2836ParkingLoop:
    DSB                                     @ Data synchronization barrier.
    ldr     %r2, [%r1]                      @ Read the processor ID.
    cmp     %r0, %r2                        @ Compare to this processor ID.
    beq     EfipBcm2836ParkingLoopJump      @ Move to the jump if it's real.
    wfi                                     @ Wait for an interrupt.
    b       EfipBcm2836ParkingLoop          @ Try again.

EfipBcm2836ParkingLoopJump:
    ldr     %r2, [%r1, #8]                  @ Get the jump address.
    mov     %r3, #0                         @ Clear R3.
    str     %r3, [%r1, #8]                  @ Store zero into jump address.
    DSB                                     @ One final breath, then...
    bx      %r2                             @ Jump head first into the abyss.

##
## Dump any literals being saved up.
##

.ltorg

EfipBcm2836ParkingLoopEnd:

##
## --------------------------------------------------------- Internal Functions
##

