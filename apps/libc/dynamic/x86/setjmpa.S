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

#include <minoca/kernel/x86.inc>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the register offsets within the jump buffer. The first offset is a
// boolean indicating whether or not the mask was saved. The next few are the
// signal mask. If the size of the signal mask changes, these offsets will need
// to be adjusted to make additional room.
//

.equ JUMP_BUFFER_SAVE_MASK, 0x00
.equ JUMP_BUFFER_EBX, 0x0C
.equ JUMP_BUFFER_ESI, 0x10
.equ JUMP_BUFFER_EDI, 0x14
.equ JUMP_BUFFER_EBP, 0x18
.equ JUMP_BUFFER_ESP, 0x1C
.equ JUMP_BUFFER_EIP, 0x20

//
// ----------------------------------------------------------------------- Code
//

//
// .text specifies that this code belongs in the executable section.
//
// .code32 specifies that this is 32-bit protected mode code.
//

.text
.code32

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

EXPORTED_FUNCTION(_setjmp)
END_FUNCTION(_setjmp)
EXPORTED_FUNCTION(setjmp)
    movl    (%esp), %ecx        # Load the return address.
    leal    4(%esp), %edx       # Get the caller's stack pointer.
    movl    (%edx), %eax        # Get the jump buffer.

    //
    // Mark the mask as having not been saved.
    //

    movl    $0, JUMP_BUFFER_SAVE_MASK(%eax)

    //
    // Save the non-volatile registers.
    //

    movl    %ebx, JUMP_BUFFER_EBX(%eax)
    movl    %esi, JUMP_BUFFER_ESI(%eax)
    movl    %edi, JUMP_BUFFER_EDI(%eax)
    movl    %ebp, JUMP_BUFFER_EBP(%eax)
    movl    %edx, JUMP_BUFFER_ESP(%eax)
    movl    %ecx, JUMP_BUFFER_EIP(%eax)


    //
    // Return 0 as a direct call from set jump.
    //

    movl    $0, %eax
    ret

END_FUNCTION(setjmp)

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

EXPORTED_FUNCTION(sigsetjmp)
    movl    (%esp), %ecx        # Load the return address.
    leal    4(%esp), %edx       # Get the caller's stack pointer.
    movl    (%edx), %eax        # Get the jump buffer.

    //
    // Mark the mask as having not been saved.
    //

    movl    $0, JUMP_BUFFER_SAVE_MASK(%eax)

    //
    // Save the non-volatile registers.
    //

    movl    %ebx, JUMP_BUFFER_EBX(%eax)
    movl    %esi, JUMP_BUFFER_ESI(%eax)
    movl    %edi, JUMP_BUFFER_EDI(%eax)
    movl    %ebp, JUMP_BUFFER_EBP(%eax)
    movl    %edx, JUMP_BUFFER_ESP(%eax)
    movl    %ecx, JUMP_BUFFER_EIP(%eax)

    //
    // Call the internal set jump C routine to help save the signal mask if
    // needed.
    //

    movl    8(%esp), %ecx       # Get the SaveMask parameter.
    pushl   %ecx                # Push the SaveMask parameter.
    pushl   %eax                # Push the jump buffer parameter.
    call    ClpSetJump          # Call the helper.
    addl    $8, %esp            # Pop the parameters.

    //
    // Return 0 as a direct call from set jump.
    //

    movl    $0, %eax
    ret

END_FUNCTION(sigsetjmp)

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

.equ Environment, 4
.equ Value, 8

FUNCTION(ClpLongJump)
    movl    Environment(%esp), %ecx # Get the environment buffer.
    movl    Value(%esp), %eax       # Get the return value in position.

    //
    // Restore the non-volatile registers.
    //

    movl    JUMP_BUFFER_EBX(%ecx), %ebx
    movl    JUMP_BUFFER_ESI(%ecx), %esi
    movl    JUMP_BUFFER_EDI(%ecx), %edi
    movl    JUMP_BUFFER_EBP(%ecx), %ebp
    movl    JUMP_BUFFER_ESP(%ecx), %esp

    //
    // Get and jump to the original EIP.
    //

    movl    JUMP_BUFFER_EIP(%ecx), %edx
    jmp     *%edx

END_FUNCTION(ClpLongJump)

