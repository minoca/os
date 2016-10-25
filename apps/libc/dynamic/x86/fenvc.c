/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fenvc.c

Abstract:

    This module implements architecture-specific floating point support for the
    C library.

Author:

    Evan Green 18-Jul-2014

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include <fenv.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define FE_ROUNDINGMASK 0x0C00

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
fesetexceptflag (
    const fexcept_t *Source,
    int Mask
    )

/*++

Routine Description:

    This routine attempts to store an implementation-defined representation of
    the given floating point status flags into the current machine state. This
    function does not raise exceptions, it only sets the flags.

Arguments:

    Source - Supplies a pointer to the implementation-defined representation of
        the floating point status to set.

    Mask - Supplies a mask of the exceptions to operate on. See FE_*
        definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    fenv_t Environment;

    Mask &= FE_ALL_EXCEPT;
    fegetenv(&Environment);
    Environment.Status &= ~Mask;
    Environment.Status |= *Source & Mask;
    fesetenv(&Environment);
    return 0;
}

LIBC_API
int
feclearexcept (
    int Exceptions
    )

/*++

Routine Description:

    This routine attempts to clear the given floating point exceptions from the
    current machine state.

Arguments:

    Exceptions - Supplies a mask of the exceptions to clear. See FE_*
        definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    fenv_t Environment;

    Exceptions &= FE_ALL_EXCEPT;
    fegetenv(&Environment);
    Environment.Status &= ~Exceptions;
    fesetenv(&Environment);
    return 0;
}

LIBC_API
int
feraiseexcept (
    int Exceptions
    )

/*++

Routine Description:

    This routine attempts to raise the given supported floating point
    exceptions. The order in which these exceptions are raised is unspecified.

Arguments:

    Exceptions - Supplies a mask of the exceptions to raise. See FE_*
        definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    fenv_t Environment;

    fegetenv(&Environment);
    Environment.Status |= Exceptions & FE_ALL_EXCEPT;
    fesetenv(&Environment);
    return 0;
}

LIBC_API
int
fetestexcept (
    int Exceptions
    )

/*++

Routine Description:

    This routine determines which of a specified subset of the floating-point
    exceptions are currently set.

Arguments:

    Exceptions - Supplies a mask of the exceptions to query. See FE_*
        definitions.

Return Value:

    Returns the bitmask of which of the specified exceptions are currently
    raised.

--*/

{

    fexcept_t Status;

    fegetexceptflag(&Status, Exceptions);
    return (int)Status;
}

LIBC_API
int
fegetround (
    void
    )

/*++

Routine Description:

    This routine returns the current rounding direction of the floating point
    unit.

Arguments:

    None.

Return Value:

    Returns the current rounding mode on success. See FE_* definitions.

    Returns a negative number on failure.

--*/

{

    fenv_t Environment;

    fegetenv(&Environment);
    return (Environment.Control & FE_ROUNDINGMASK);
}

LIBC_API
int
fesetround (
    int Mode
    )

/*++

Routine Description:

    This routine attempts to set the rounding mode of the floating point unit.

Arguments:

    Mode - Supplies the new mode to set. See FE_* definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    fenv_t Environment;

    fegetenv(&Environment);
    Environment.Control &= ~FE_ROUNDINGMASK;
    Environment.Control |= Mode & FE_ROUNDINGMASK;
    fesetenv(&Environment);
    return 0;
}

LIBC_API
int
feholdexcept (
    fenv_t *Environment
    )

/*++

Routine Description:

    This routine saves the current floating point environment, clears the
    status flags, and installs a non-stop (continue on floating-point
    exceptions) mode, if available, for all floating point exceptions.

Arguments:

    Environment - Supplies a pointer where the environment will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    fenv_t EnvironmentCopy;

    fegetenv(Environment);
    memcpy(&EnvironmentCopy, Environment, sizeof(fenv_t));

    //
    // Clear the exception flags, and mask all exceptions.
    //

    EnvironmentCopy.Status &= ~FE_ALL_EXCEPT;
    EnvironmentCopy.Control |= FE_ALL_EXCEPT;
    fesetenv(&EnvironmentCopy);
    return 0;
}

LIBC_API
int
feupdateenv (
    fenv_t *Environment
    )

/*++

Routine Description:

    This routine saves the currently raised floating-point exceptions, loads
    the given floating-point environment, and then raises the saved floating
    point exceptions.

Arguments:

    Environment - Supplies a pointer to the environment to load.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    int Exceptions;

    Exceptions = fetestexcept(FE_ALL_EXCEPT);
    fesetenv(Environment);
    feraiseexcept(Exceptions);
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

