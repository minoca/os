/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
.arch_extension virt

//
// .globl allows these labels to be visible to the linker.
//

.globl EfipBcm2836ProcessorStartup
.globl EfipBcm2836ParkingLoop
.globl EfipBcm2836ParkingLoopEnd

//
// VOID
// EfipBcm2836SendEvent (
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

FUNCTION EfipBcm2836SendEvent
    DSB                             @ Data Synchronization Barrier.
    sev                             @ Send Event.
    bx      %lr                     @ Return.

END_FUNCTION EfipBcm2836SendEvent

//
// UINT32
// EfipBcm2836GetMultiprocessorIdRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Multiprocessor ID register (MPIDR).

Arguments:

    None.

Return Value:

    Returns the value of the MPIDR.

--*/

FUNCTION EfipBcm2836GetMultiprocessorIdRegister
    mrc     p15, 0, %r0, %c0, %c0, 5            @ Get the MPIDR
    bx      %lr                                 @

END_FUNCTION EfipBcm2836GetMultiprocessorIdRegister

//
// VOID
// EfipBcm2836SwitchToSvcMode (
//     VOID
//     )
//

/*++

Routine Description:

    This routine disabled interrupts and switches to SVC mode. It handles the
    case where the core is in HYP mode, which requires an execution state
    transition in order to enter SVC mode.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION EfipBcm2836SwitchToSvcMode

    //
    // Disable interrupts.
    //

    cpsid   i

    //
    // Test to see if the core is in HYP mode. Transitioning from HYP mode to
    // SVC mode requires an ERET to switch execution states.
    //

    mrs     %r0, CPSR
    and     %r1, %r0, #ARM_MODE_MASK
    cmp     %r1, #ARM_MODE_HYP
    bne     EfipBcm2836SwitchToSvcModeEnd

    //
    // While in HYP mode, zero out the Generic Timer's virtual offset. Assume
    // that if the core is not in HYP mode that some other source appropriately
    // set the virtual offset.
    //

    mov     %r2, #0
    mov     %r3, #0
    mcrr    p15, 4, %r2, %r3, %c14

    //
    // Trasition to SVC mode with the ERET.
    //

    msr     ELR_hyp, %lr
    and     %r0, %r0, #~(ARM_MODE_MASK)
    orr     %r0, %r0, #ARM_MODE_SVC
    msr     SPSR_hyp, %r0
    eret

EfipBcm2836SwitchToSvcModeEnd:

    //
    // The core was not in HYP mode. Switch to SVC mode the easy way.
    //

    mov     %r0, #(PSR_FLAG_IRQ | ARM_MODE_SVC)
    msr     CPSR_c, %r0
    bx      %lr

END_FUNCTION EfipBcm2836SwitchToSvcMode

//
// VOID
// EfipBcm2836ProcessorStartup (
//     VOID
//     )
//

/*++

Routine Description:

    This routine implements the startup routine for the alternate CPUs on the
    Raspberry Pi 2. Since this is the very first set of instructions executed
    on this core there is nothing set up, including a stack.

Arguments:

    None.

Return Value:

    None. This function does not return, as there is nothing to return to.

--*/

.arm
.align 4
EfipBcm2836ProcessorStartup:

    //
    // Disable interrupts and switch to SVC mode.
    //

    bl      EfipBcm2836SwitchToSvcMode

    //
    // Park the core again, waiting until the firmware can allocate a page for
    // the final parking location.
    //

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
    bic     %r1, %r2, #0xF00                @ Set the parking location.
    bic     %r1, %r1, #0x0FF
    DSB                                     @ One final breath, then...
    bx      %r2                             @ Jump head first into the abyss.

.ltorg

//
// VOID
// EfipBcm2836ParkingLoop (
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

//
// Dump any literals being saved up.
//

.ltorg

EfipBcm2836ParkingLoopEnd:

//
// --------------------------------------------------------- Internal Functions
//

