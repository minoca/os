/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    setjmpa.S

Abstract:

    This module implements functionality for non-local goto statements.

Author:

    Evan Green 28-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/arm.inc>

//
// ---------------------------------------------------------------- Definitions
//

#define JUMP_BUFFER_SAVE_MASK 0x00

//
// This offset represents the start offset of the registers in the jump buffer.
//

#define JUMP_BUFFER_REGISTER_OFFSET 0x0C

//
// This offset represents the end of the registers in the jump buffer.
//

#define JUMP_BUFFER_REGISTER_END_OFFSET 0x34

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// int
// setjmp (
//     jmp_buf Environment
//     )
//

/*++

Routine Description:

    This routine saves the calling environment into the given buffer for
    later use by longjmp.

Arguments:

    Environment - Supplies the pointer to the environment to save the
        application context in.

Return Value:

    0 if this was the direct call to set jump.

    Non-zero if this was a call from long jump.

--*/

EXPORTED_FUNCTION _setjmp
END_FUNCTION _setjmp
EXPORTED_FUNCTION setjmp

    //
    // Clear out the boolean indicating whether or not the mask was saved.
    //

    mov     %r2, #0
    str     %r2, [%r0, #JUMP_BUFFER_SAVE_MASK]

    //
    // Save the registers.
    //

    ldr     %r2, =JUMP_BUFFER_REGISTER_END_OFFSET
    add     %r2, %r0, %r2
    mov     %r12, %sp
    stmdb   %r2, {%r4-%r11, %r12, %lr}

    //
    // Return 0.
    //

    mov     %r0, #0
    bx      %lr

END_FUNCTION setjmp

//
// int
// sigsetjmp (
//     sigjmp_buf Environment,
//     int SaveMask
//     )
//

/*++

Routine Description:

    This routine saves the calling environment into the given buffer for
    later use by longjmp.

Arguments:

    Environment - Supplies the pointer to the environment to save the
        application context in.

    SaveMask - Supplies a value indicating if the caller would like the
        current signal mask to be saved in the environment as well.

Return Value:

    0 if this was the direct call to setjmp.

    Non-zero if this was a call from longjmp.

--*/

EXPORTED_FUNCTION sigsetjmp

    //
    // Save the registers.
    //

    ldr     %r2, =JUMP_BUFFER_REGISTER_END_OFFSET
    add     %r2, %r0, %r2
    mov     %r12, %sp
    stmdb   %r2, {%r4-%r11, %r12, %lr}

    //
    // Call the helper routine to potentially save the mask.
    //

    stmdb   %sp!, {%lr}          @ Save the original lr.
    bl      ClpSetJump           @ Call the helper with the same parameters.
    ldmia   %sp!, {%lr}          @ Restore lr.

    //
    // Return 0.
    //

    mov     %r0, #0
    bx      %lr

END_FUNCTION sigsetjmp

//
// void
// ClpLongJump (
//     jmp_buf Environment,
//     int Value
//     )
//

/*++

Routine Description:

    This routine restores given environment.

Arguments:

    Environment - Supplies a pointer to the environment to restore.

    Value - Supplies the value to make appear as the return value from the
        set jump function.

Return Value:

    None, the function does not return.

--*/

FUNCTION ClpLongJump
    ldr     %r2, =JUMP_BUFFER_REGISTER_OFFSET
    add     %r2, %r0, %r2
    mov     %r0, %r1
    ldmia   %r2, {%r4-%r11, %r12, %lr}
    mov     %sp, %r12
    bx      %lr

END_FUNCTION ClpLongJump

