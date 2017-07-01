/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fenv.S

Abstract:

    This module implements functionality for manipulating the floating point
    environment.

Author:

    Evan Green 18-Jan-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x64.inc>

//
// ---------------------------------------------------------------- Definitions
//

#define FE_ALL_EXCEPT 0x3F
#define FE_DFL_ENV -1

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// LIBC_API
// int
// fegetenv (
//     fenv_t *Environment
//     )
//

/*++

Routine Description:

    This routine stores the current floating point machine environment into the
    given environment pointer.

Arguments:

    Environment - Supplies the pointer to the environment to save the
        floating point context in.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

EXPORTED_FUNCTION(fegetenv)
    fnstenv (%rdi)              # Store the FP environment into the pointer.
    xorq    %rax, %rax          # Zero out rax.
    ret                         # Return success.

END_FUNCTION(fegetenv)

//
// LIBC_API
// int
// fesetenv (
//     const fenv_t *Environment
//     )
//

/*++

Routine Description:

    This routine sets the current machine floating point environment to that of
    the given saved environment.

Arguments:

    Environment - Supplies the pointer to the environment to load into the
        execution state.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

EXPORTED_FUNCTION(fesetenv)
    cmpq    $FE_DFL_ENV, %rdi   # Compare to the "default" environment value.
    jne     fesetenvLoad        # Jump to the load portion if it's custom.
    finit                       # Load the default state.

fesetenvLoad:
    fldenv  (%rdi)              # Load the FP environment from the pointer.
    xorq    %rax, %rax          # Zero out rax.
    ret                         # Return success.

END_FUNCTION(fesetenv)

//
// LIBC_API
// int
// fegetexceptflag (
//     fexcept_t *Destination,
//     int Mask
//     )
//

/*++

Routine Description:

    This routine stores an implementation defined representation of the
    exception flags indicated by the given mask into the given destination.

Arguments:

    Destination - Supplies a pointer where the implementation-defined
        representation of the current flags masked with the given value.

    Mask - Supplies a mask of the exceptions the caller is interested in. See
        FE_* definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

EXPORTED_FUNCTION(fegetexceptflag)
    xorq    %rax, %rax          # Zero out rax.
    fnstsw  %ax                 # Get the floating point flags.
    andq    %rax, %rsi          # AND them with the mask.
    andq    $FE_ALL_EXCEPT, %rsi # AND in the valid exceptions.
    movw    %dx, (%rdi)         # Save the flags into the destination.
    xorq    %rax, %rax          # Zero out rax return value.
    ret                         # return success.

END_FUNCTION(fegetexceptflag)

