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
#include <float.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define FE_ROUNDINGMASK FE_TOWARDZERO

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

unsigned int
ClpGetFpscr (
    void
    );

void
ClpSetFpscr (
    unsigned int Value
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL ClVfpSupported;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
fegetexceptflag (
    fexcept_t *Destination,
    int Mask
    )

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

{

    fenv_t Environment;
    int Result;

    Result = fegetenv(&Environment);
    *Destination = Environment.Fpscr & FE_ALL_EXCEPT & Mask;
    return Result;
}

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
    Environment.Fpscr |= Mask;
    return fesetenv(&Environment);
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
    Environment.Fpscr &= ~Exceptions;
    return fesetenv(&Environment);
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

    float Max;
    float Min;
    float One;
    float OneE32;
    volatile float Result;
    float Three;
    float Two;
    float Zero;

    One = 1.0;
    Two = 2.0;
    Three = 3.0;
    Zero = 0.0;
    Max = FLT_MAX;
    Min = FLT_MIN;
    OneE32 = 1.0e32F;
    if ((Exceptions & FE_INVALID) != 0) {
        Result = Zero / Zero;
    }

    if ((Exceptions & FE_DIVBYZERO) != 0) {
        Result = One / Zero;
    }

    if ((Exceptions & FE_OVERFLOW) != 0) {
        Result = Max + OneE32;
    }

    if ((Exceptions & FE_UNDERFLOW) != 0) {
        Result = Min / Two;
    }

    if ((Exceptions & FE_INEXACT) != 0) {
        Result = Two / Three;
    }

    if ((Exceptions & FE_DENORM) != 0) {
        Result = Min / Max;
    }

    //
    // Keep the compiler from complaining about an unused result.
    //

    (void)Result;
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
    int Result;

    Result = fegetenv(&Environment);
    if (Result != 0) {
        return Result;
    }

    return (Environment.Fpscr & FE_ROUNDINGMASK);
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
    Environment.Fpscr &= ~FE_ROUNDINGMASK;
    Environment.Fpscr |= Mode & FE_ROUNDINGMASK;
    return fesetenv(&Environment);
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

    EnvironmentCopy.Fpscr &= ~FE_ALL_EXCEPT;
    EnvironmentCopy.Fpscr &= ~(FE_ALL_EXCEPT << FE_EXCEPT_SHIFT);
    return fesetenv(&EnvironmentCopy);
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
    return feraiseexcept(Exceptions);
}

LIBC_API
int
fegetenv (
    fenv_t *Environment
    )

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

{

    if (ClVfpSupported == FALSE) {
        ClVfpSupported = OsTestProcessorFeature(OsArmVfp);
        if (ClVfpSupported == FALSE) {
            return -1;
        }
    }

    Environment->Fpscr = ClpGetFpscr();
    return 0;
}

LIBC_API
int
fesetenv (
    const fenv_t *Environment
    )

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

{

    if (ClVfpSupported == FALSE) {
        ClVfpSupported = OsTestProcessorFeature(OsArmVfp);
        if (ClVfpSupported == FALSE) {
            return -1;
        }
    }

    ClpSetFpscr(Environment->Fpscr);
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

