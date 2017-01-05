/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rintf.c

Abstract:

    This module implements support for the round to nearest integral math
    functions.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Chris Stevens 5-Jan-2017

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include "mathp.h"
#include <fenv.h>

//
// ---------------------------------------------------------------- Definitions
//

#define RINT_ADJUSTMENT_EXP_19      0x40000000
#define RINT_ADJUSTMENT_EXP_18      0x80000000
#define RINT_ADJUSTMENT_HIGH        0x00020000
#define RINT_ADJUSTMENT_LOW         0x40000000
#define RINT_NEGATIVE_EXP_HIGH_MASK 0xfffe0000

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
long
lrintf (
    float Value
    )

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integer value.

    Returns an undefined value if the integer is NaN or out of range.

--*/

{

    fenv_t Environment;
    long Result;

    feholdexcept(&Environment);
    Result = (long)rintf(Value);
    if (fetestexcept(FE_INVALID) != 0) {
        feclearexcept(FE_INEXACT);
    }

    feupdateenv(&Environment);
    return Result;
}

LIBC_API
long long
llrintf (
    float Value
    )

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integer value.

    Returns an undefined value if the integer is NaN or out of range.

--*/

{

    fenv_t Environment;
    long long Result;

    feholdexcept(&Environment);
    Result = (long long)rintf(Value);
    if (fetestexcept(FE_INVALID) != 0) {
        feclearexcept(FE_INEXACT);
    }

    feupdateenv(&Environment);
    return Result;
}

LIBC_API
float
nearbyintf (
    float Value
    )

/*++

Routine Description:

    This routine round the given value to the nearest integer, using the
    current rounding direction. This routine does not raise an inexact
    exception.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integral value in the direction of the current rounding
    mode.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

{

    fenv_t Environment;
    float Result;

    fegetenv(&Environment);
    Result = rintf(Value);
    fesetenv(&Environment);
    return Result;
}

LIBC_API
float
rintf (
    float Value
    )

/*++

Routine Description:

    This routine converts the given value into the nearest integral in the
    direction of the current rounding mode.

Arguments:

    Value - Supplies the value to round into an integral.

Return Value:

    Returns the nearest integral value in the direction of the current rounding
    mode.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

{

    LONG Exponent;
    LONG Word;
    FLOAT_PARTS Parts;
    LONG SignBit;
    volatile float VolatileValue;

    Parts.Float = Value;
    Word = Parts.Ulong;
    SignBit = (ULONG)Word >> FLOAT_SIGN_BIT_SHIFT;
    Exponent = ((Word & FLOAT_EXPONENT_MASK) >> FLOAT_EXPONENT_SHIFT) -
               FLOAT_EXPONENT_BIAS;

    if (Exponent < 23) {
        if (Exponent < 0) {
            if ((Word & ~FLOAT_SIGN_BIT) == 0) {
                return Value;
            }

            Parts.Ulong = Word;
            VolatileValue = ClFloatTwo23[SignBit] + Parts.Float;
            Parts.Float = VolatileValue - ClFloatTwo23[SignBit];
            Word = Parts.Ulong;
            Parts.Ulong = (Word & ~FLOAT_SIGN_BIT) |
                          (SignBit << FLOAT_SIGN_BIT_SHIFT);

            return Parts.Float;
        }

        Parts.Ulong = Word;
        VolatileValue = ClFloatTwo23[SignBit] + Parts.Float;
        Parts.Float = VolatileValue - ClFloatTwo23[SignBit];
        return Parts.Float;
    }

    //
    // Check for infinite or NaN.
    //

    if (Exponent ==
        ((FLOAT_EXPONENT_MASK >> FLOAT_EXPONENT_SHIFT) - FLOAT_EXPONENT_BIAS)) {

        return Value + Value;
    }

    //
    // The value is integral.
    //

    return Value;
}

//
// --------------------------------------------------------- Internal Functions
//

