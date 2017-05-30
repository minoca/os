/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    setjmpa.S

Abstract:

    This module implements functionality for non-local goto statements.

Author:

    Evan Green 17-Jan-2015

Environment:

    User Mode C Library

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/x64.inc>

##
## ---------------------------------------------------------------- Definitions
##

##
## Define the register offsets within the jump buffer. The first offset is a
## boolean indicating whether or not the mask was saved. The next few are the
## signal mask. If the size of the signal mask changes, these offsets will need
## to be adjusted to make additional room.
##

#define JUMP_BUFFER_SAVE_MASK 0x00
#define JUMP_BUFFER_RBX 0x10
#define JUMP_BUFFER_RBP 0x18
#define JUMP_BUFFER_RSP 0x20
#define JUMP_BUFFER_R10 0x28
#define JUMP_BUFFER_R11 0x30
#define JUMP_BUFFER_R12 0x38
#define JUMP_BUFFER_R13 0x40
#define JUMP_BUFFER_R14 0x48
#define JUMP_BUFFER_R15 0x50
#define JUMP_BUFFER_RIP 0x58

##
## ----------------------------------------------------------------------- Code
##

ASSEMBLY_FILE_HEADER

##
## int
## setjmp (
##     jmp_buf Environment
##     )
##

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
    int     $3                  # TODO: Check this, it's never been run!
    movq    (%rsp), %rcx        # Load the return address.

    ##
    ## Mark the mask as having not been saved.
    ##

    movl    $0, JUMP_BUFFER_SAVE_MASK(%rdi)

    ##
    ## Save non-volatile registers.
    ##

    movq    %rbx, JUMP_BUFFER_RBX(%rdi)
    movq    %rbp, JUMP_BUFFER_RBP(%rdi)
    movq    %r10, JUMP_BUFFER_R10(%rdi)
    movq    %r11, JUMP_BUFFER_R11(%rdi)
    movq    %r12, JUMP_BUFFER_R12(%rdi)
    movq    %r13, JUMP_BUFFER_R13(%rdi)
    movq    %r14, JUMP_BUFFER_R14(%rdi)
    movq    %r15, JUMP_BUFFER_R15(%rdi)
    movq    %rcx, JUMP_BUFFER_RIP(%rdi)

    ##
    ## Save the stack.
    ##

    leaq    8(%rsp), %rdx       # Load the return address.
    movq    %rdx, JUMP_BUFFER_RSP(%rdi) # Save it to the RSP slot.
    xorq    %rax, %rax          # Return 0 to indicate return from setjmp.
    ret

END_FUNCTION(setjmp)

##
## int
## sigsetjmp (
##     sigjmp_buf Environment,
##     int SaveMask
##     )
##

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
    int     $3                  # TODO: Check this, it's never been run!
    movq    (%rsp), %rcx        # Load the return address.

    ##
    ## Mark the mask as having not been saved.
    ##

    movl    $0, JUMP_BUFFER_SAVE_MASK(%rdi)

    ##
    ## Save non-volatile registers.
    ##

    movq    %rbx, JUMP_BUFFER_RBX(%rdi)
    movq    %rbp, JUMP_BUFFER_RBP(%rdi)
    movq    %r10, JUMP_BUFFER_R10(%rdi)
    movq    %r11, JUMP_BUFFER_R11(%rdi)
    movq    %r12, JUMP_BUFFER_R12(%rdi)
    movq    %r13, JUMP_BUFFER_R13(%rdi)
    movq    %r14, JUMP_BUFFER_R14(%rdi)
    movq    %r15, JUMP_BUFFER_R15(%rdi)
    movq    %rcx, JUMP_BUFFER_RIP(%rdi)

    ##
    ## Save the stack.
    ##

    leaq    8(%rsp), %rdx       # Load the return address.
    movq    %rdx, JUMP_BUFFER_RSP(%rdi) # Save it to the RSP slot.

    ##
    ## Call the helper routine (with the same parameters) to save the signal
    ## mask.
    ##

    call    ClpSetJump          # Call helper with same arguments.
    xorq    %rax, %rax          # Return 0 to indicate return from setjmp.
    ret

END_FUNCTION(sigsetjmp)

##
## void
## ClpLongJump (
##     jmp_buf Environment,
##     int Value
##     )
##

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

FUNCTION(ClpLongJump)
    int     $3                  # TODO: Check this, it's never been run!
    movq    JUMP_BUFFER_R15(%rdi), %r15
    movq    JUMP_BUFFER_R14(%rdi), %r14
    movq    JUMP_BUFFER_R13(%rdi), %r13
    movq    JUMP_BUFFER_R12(%rdi), %r12
    movq    JUMP_BUFFER_R11(%rdi), %r11
    movq    JUMP_BUFFER_R10(%rdi), %r10
    movq    JUMP_BUFFER_RBP(%rdi), %rbp
    movq    JUMP_BUFFER_RBX(%rdi), %rbx

    ##
    ## Restore the stack.
    ##

    movq    JUMP_BUFFER_RSP(%rdi), %rsp

    ##
    ## Move the return value to rax and jump to the original rip.
    ##

    movq    %rsi, %rax
    movq    JUMP_BUFFER_RIP(%rdi), %rdx
    jmp     *%rdx

END_FUNCTION(ClpLongJump)

