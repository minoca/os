/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smpa.S

Abstract:

    This module implements assembly routines necessary for booting the
    second core on the OMAP4.

Author:

    Evan Green 31-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/arm.inc>

//
// --------------------------------------------------------------- Definitions
//

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// .globl allows these labels to be visible to the linker.
//

.globl EfipOmap4ProcessorStartup
.globl EfipOmap4ParkingLoop
.globl EfipOmap4ParkingLoopEnd

//
// UINT32
// EfipOmap4SmcCommand (
//     UINT32 Argument1,
//     UINT32 Argument2,
//     UINT32 Command
//     )
//

/*++

Routine Description:

    This routine executes an SMC command up to secure mode. The OMAP4 defines
    a software interface for non-secure software to request certain services
    using the SMC command.

Arguments:

    Argument1 - Supplies the 32-bit value to put into R0. This is dependent on
        which command is issued.

    Argument2 - Supplies the 32-bit value to put into R1. This is also
        dependent on the command issued.

    Command - Supplies the SMC command number to issue.

Return Value:

    Returns the result as passed back from the SMC command.

--*/

FUNCTION EfipOmap4SmcCommand
    stmdb   sp!, {%r4-%r12, %lr}    @ Save registers.
    mov     %r12, %r2               @ Move command into place.
    DSB                             @ Data synchronization barrier.
    smc     #0                      @ Execute the SMC instruction.
    ldmia   sp!, {%r4-%r12, %lr}    @ Restore registers.
    bx      %lr

END_FUNCTION EfipOmap4SmcCommand

//
// VOID
// EfipOmap4SendEvent (
//     VOID
//     )
//

/*++

Routine Description:

    This routine executes a SEV instruction, which is a hint instruction that
    causes an event to be signalled to all processors.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION EfipOmap4SendEvent
    DSB                             @ Data Synchronization Barrier.
    sev                             @ Send Event.
    bx      %lr                     @ Return.

END_FUNCTION EfipOmap4SendEvent

//
// VOID
// EfipOmap4ProcessorStartup (
//     VOID
//     )
//

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
EfipOmap4ProcessorStartup:
    ldr     %r2, =EfiOmap4ProcessorId       @ Get the processor ID address.
    ldr     %r0, [%r2]                      @ Get the value.
    ldr     %r2, =EfiOmap4ProcessorJumpAddress  @ Get the jump destination.
    ldr     %r4, [%r2]                      @ Get the value.
    mov     %r3, #0                         @ Clear out R3.
    str     %r3, [%r2]                      @ Clear the value.
    bic     %r1, %r4, #0xF00                @ Set the parking location.
    bic     %r1, %r1, #0x0FF
    bx      %r4                             @ Jump to the destination.

.ltorg

//
// VOID
// EfipOmap4ParkingLoop (
//     UINT32 ProcessorId,
//     VOID *ParkingLocation
//     )
//

/*++

Routine Description:

    This routine implements the MP parking protocol loop.

Arguments:

    ProcessorId - Supplies the ID of this processor.

    ParkingLocation - Supplies the parking protocol mailbox base.

Return Value:

    None. This function does not return, it launches the core.

--*/

EfipOmap4ParkingLoop:
    DSB                                     @ Data synchronization barrier.
    ldr     %r2, [%r1]                      @ Read the processor ID.
    cmp     %r0, %r2                        @ Compare to this processor ID.
    beq     EfipOmap4ParkingLoopJump        @ Move to the jump if it's real.
    wfi                                     @ Wait for an interrupt.
    b       EfipOmap4ParkingLoop            @ Try again.

EfipOmap4ParkingLoopJump:
    ldr     %r2, [%r1, #8]                  @ Get the jump address.
    mov     %r3, #0                         @ Clear R3.
    str     %r3, [%r1, #8]                  @ Store zero into jump address.
    DSB                                     @ One final breath, then...
    bx      %r2                             @ Jump head first into the abyss.

//
// Dump any literals being saved up.
//

.ltorg

EfipOmap4ParkingLoopEnd:

//
// --------------------------------------------------------- Internal Functions
//

