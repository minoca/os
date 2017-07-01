/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efia.S

Abstract:

    This module implements assembly support routines for UEFI support.

Author:

    Evan Green 27-Mar-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/arm.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// UINTN
// BopEfiGetStackPointer (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the value of the stack register. Note that this can only
    be used as an approximate value, since as soon as this function returns
    the stack pointer changes.

Arguments:

    None.

Return Value:

    Returns the current stack pointer.

--*/

FUNCTION BopEfiGetStackPointer
    mov     %r0, %sp                    @ Move the stack to the return value.
    bx      %lr                         @ Return.

END_FUNCTION BopEfiGetStackPointer

//
// VOID
// BopEfiSaveFirmwareExceptionStacks (
//     VOID
//     )
//

/*++

Routine Description:

    This routine saves the current banked registers into globals.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION BopEfiSaveFirmwareExceptionStacks

    //
    // Save IRQ registers.
    //

    cps     #ARM_MODE_IRQ               @ Switch to IRQ mode.
    mov     %r0, %sp                    @ Get the stack pointer.
    ldr     %r1, =BoFirmwareIrqStack    @ Get the address of the global.
    str     %r0, [%r1]                  @ Save the stack pointer.
    mov     %r0, %lr                    @ Get the link pointer.
    ldr     %r1, =BoFirmwareIrqLink     @ Get the address of the global.
    str     %r0, [%r1]                  @ Save to the global.

    //
    // Save FIQ registers.
    //

    cps     #ARM_MODE_FIQ               @ Switch to FIQ mode.
    mov     %r0, %sp                    @ Get the stack pointer.
    ldr     %r1, =BoFirmwareFiqStack    @ Get the address of the global.
    str     %r0, [%r1]                  @ Save the stack pointer.
    mov     %r0, %lr                    @ Get the link pointer.
    ldr     %r1, =BoFirmwareFiqLink     @ Get the address of the global.
    str     %r0, [%r1]                  @ Save to the global.

    //
    // Save undefined instruction registers.
    //

    cps     #ARM_MODE_UNDEF             @ Switch to undefined instruction mode.
    mov     %r0, %sp                    @ Get the stack pointer.
    ldr     %r1, =BoFirmwareUndefStack  @ Get the address of the global.
    str     %r0, [%r1]                  @ Save the stack pointer.
    mov     %r0, %lr                    @ Get the link pointer.
    ldr     %r1, =BoFirmwareUndefLink   @ Get the address of the global.
    str     %r0, [%r1]                  @ Save to the global.

    //
    // Save abort registers.
    //

    cps     #ARM_MODE_ABORT             @ Switch to abort mode.
    mov     %r0, %sp                    @ Get the stack pointer.
    ldr     %r1, =BoFirmwareAbortStack  @ Get the address of the global.
    str     %r0, [%r1]                  @ Save the stack pointer.
    mov     %r0, %lr                    @ Get the link pointer.
    ldr     %r1, =BoFirmwareAbortLink   @ Get the address of the global.
    str     %r0, [%r1]                  @ Save to the global.

    //
    // Switch back to SVC mode and return.
    //

    cps     #ARM_MODE_SVC               @ Switch back to SVC mode.
    bx      %lr                         @ Return.

END_FUNCTION BopEfiSaveFirmwareExceptionStacks

//
// VOID
// BopEfiRestoreFirmwareExceptionStacks (
//     VOID
//     )
//

/*++

Routine Description:

    This routine restores the firmware's banked registers from the globals.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION BopEfiRestoreFirmwareExceptionStacks

    //
    // Restore IRQ registers.
    //

    cps     #ARM_MODE_IRQ               @ Switch to IRQ mode.
    ldr     %r1, =BoFirmwareIrqStack    @ Get the address of the global.
    ldr     %r0, [%r1]                  @ Get the value of the global.
    mov     %r0, %sp                    @ Move it into the stack register.
    ldr     %r1, =BoFirmwareIrqLink     @ Get the address of the global.
    ldr     %r0, [%r1]                  @ Get the value of the global.
    mov     %r0, %lr                    @ Move it into the link register.

    //
    // Restore FIQ registers.
    //

    cps     #ARM_MODE_FIQ               @ Switch to FIQ mode.
    ldr     %r1, =BoFirmwareFiqStack    @ Get the address of the global.
    ldr     %r0, [%r1]                  @ Get the value of the global.
    mov     %r0, %sp                    @ Move it into the stack register.
    ldr     %r1, =BoFirmwareFiqLink     @ Get the address of the global.
    ldr     %r0, [%r1]                  @ Get the value of the global.
    mov     %r0, %lr                    @ Move it into the link register.

    //
    // Restore undefined instruction registers.
    //

    cps     #ARM_MODE_UNDEF             @ Switch to undefined instruction mode.
    ldr     %r1, =BoFirmwareUndefStack  @ Get the address of the global.
    ldr     %r0, [%r1]                  @ Get the value of the global.
    mov     %r0, %sp                    @ Move it into the stack register.
    ldr     %r1, =BoFirmwareUndefLink   @ Get the address of the global.
    ldr     %r0, [%r1]                  @ Get the value of the global.
    mov     %r0, %lr                    @ Move it into the link register.

    //
    // Restore abort registers.
    //

    cps     #ARM_MODE_ABORT             @ Switch to abort mode.
    ldr     %r1, =BoFirmwareAbortStack  @ Get the address of the global.
    ldr     %r0, [%r1]                  @ Get the value of the global.
    mov     %r0, %sp                    @ Move it into the stack register.
    ldr     %r1, =BoFirmwareAbortLink   @ Get the address of the global.
    ldr     %r0, [%r1]                  @ Get the value of the global.
    mov     %r0, %lr                    @ Move it into the link register.

    //
    // Switch back to SVC mode and return.
    //

    cps     #ARM_MODE_SVC               @ Switch back to SVC mode.
    bx      %lr                         @ Return.

END_FUNCTION BopEfiRestoreFirmwareExceptionStacks

//
// --------------------------------------------------------- Internal Functions
//

