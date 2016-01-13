/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    smpa.S

Abstract:

    This module implements assembly routines necessary for booting other
    processors on the RK32xx.

Author:

    Evan Green 10-Jul-2015

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

.equ RK32_GIC_CPU_INTERFACE_BASE, 0xFFC02000

##
## ---------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER
.arch armv7-a
.arch_extension sec

##
## .globl allows these labels to be visible to the linker.
##

.globl EfipRk32ProcessorStartup
.globl EfipRk32ParkingLoop
.globl EfipRk32ParkingLoopEnd

##
## VOID
## EfipRk32SendEvent (
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

FUNCTION EfipRk32SendEvent
    dsb                             @ Data Synchronization Barrier.
    sev                             @ Send Event.
    bx      %lr                     @ Return.

END_FUNCTION EfipRk32SendEvent

##
## VOID
## EfipRk32ProcessorStartup (
##     VOID
##     )
##

/*++

Routine Description:

    This routine implements the startup routine for the alternate CPUs on the
    RK32xx. Since this is the very first set of instructions executed on this
    core there is nothing set up, including a stack.

Arguments:

    None.

Return Value:

    None. This function does not return, as there is nothing to return to.

--*/

.arm
EfipRk32ProcessorStartup:

    ##
    ## Set the SMP bit in the auxiliary control register on this core.
    ##

    mrc     p15, 0, %r0, %cr1, %cr0, 1
    ldr     %r1, =CORTEX_A17_AUX_SMP_ENABLE
    ldr     %r1, [%r1]
    orr     %r0, %r0, %r1
    mcr     p15, 0, %r0, %cr1, %cr0, 1

    ##
    ## Initialize the local GIC CPU interface by setting the lowest priority
    ## and enabling interrupts.
    ##

    ldr     %r0, =RK32_GIC_CPU_INTERFACE_BASE
    mov     %r1, #0x3
    str     %r1, [%r0, #0x8]
    mov     %r1, #0xF0
    str     %r1, [%r0, #0x4]
    mov     %r1, #0x1
    str     %r1, [%r0]

    ##
    ## Move to the parking location for this processor.
    ##

    mov     %r3, #0                           @ Clear out R3.
    ldr     %r2, =EfiRk32ProcessorId          @ Get the processor ID address.
    ldr     %r0, [%r2]                        @ Get the value.
    ldr     %r2, =EfiRk32ProcessorJumpAddress @ Get the jump destination.
    ldr     %r4, [%r2]                        @ Get the value.
    str     %r3, [%r2]                        @ Clear the value.
    ldr     %r3, =0xFFFFF000                  @ Get the page mask.
    and     %r1, %r4, %r3                     @ Set the parking location.
    bx      %r4                               @ Jump to the destination.

.ltorg

##
## VOID
## EfipRk32ParkingLoop (
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

EfipRk32ParkingLoop:
    dsb                                     @ Data synchronization barrier.
    ldr     %r2, [%r1]                      @ Read the processor ID.
    cmp     %r0, %r2                        @ Compare to this processor ID.
    beq     EfipRk32ParkingLoopJump         @ Move to the jump if it's real.
    wfi                                     @ Wait for an interrupt.
    b       EfipRk32ParkingLoop             @ Try again.

EfipRk32ParkingLoopJump:
    ldr     %r2, [%r1, #8]                  @ Get the jump address.
    mov     %r3, #0                         @ Clear R3.
    str     %r3, [%r1, #8]                  @ Store zero into jump address.
    dsb                                     @ One final breath, then...
    bx      %r2                             @ Jump head first into the abyss.

##
## Dump any literals being saved up.
##

.ltorg

EfipRk32ParkingLoopEnd:

##
## --------------------------------------------------------- Internal Functions
##

