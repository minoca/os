/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rint.c

Abstract:

    This module implements support for the round to nearest integral math
    functions.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Chris Stevens 13-Jul-2016

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
lrint (
    double Value
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
    Result = (long)rint(Value);
    if (fetestexcept(FE_INVALID) != 0) {
        feclearexcept(FE_INEXACT);
    }

    feupdateenv(&Environment);
    return Result;
}

LIBC_API
long long
llrint (
    double Value
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
    Result = (long long)rint(Value);
    if (fetestexcept(FE_INVALID) != 0) {
        feclearexcept(FE_INEXACT);
    }

    feupdateenv(&Environment);
    return Result;
}

LIBC_API
double
nearbyint (
    double Value
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
    double Result;

    fegetenv(&Environment);
    Result = rint(Value);
    fesetenv(&Environment);
    return Result;
}

LIBC_API
double
rint (
    double Value
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
    ULONG ExponentMask;
    ULONG ExponentShift;
    LONG HighWord;
    ULONG HighWordAbsolute;
    ULONG LowWord;
    ULONG NonExponent;
    DOUBLE_PARTS Parts;
    LONG SignBit;
    ULONG SignBitShift;
    volatile double VolatileValue;

    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    LowWord = Parts.Ulong.Low;
    SignBitShift = DOUBLE_SIGN_BIT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    SignBit = (ULONG)HighWord >> SignBitShift;
    ExponentMask = DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT;
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    Exponent = ((HighWord & ExponentMask) >> ExponentShift) -
               DOUBLE_EXPONENT_BIAS;

    if (Exponent < 20) {
        if (Exponent < 0) {
            HighWordAbsolute = HighWord &
                               (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

            if ((HighWordAbsolute | LowWord) == 0) {
                return Value;
            }

            //
            // Set bit 51 if the fraction is non-zero. This likely has
            // something to do with avoiding double rounding.
            //

            LowWord |= HighWord & DOUBLE_HIGH_VALUE_MASK;
            HighWord &= RINT_NEGATIVE_EXP_HIGH_MASK;
            HighWord |= ((LowWord | -LowWord) >>
                         (SignBitShift - (ExponentShift - 1))) &
                        (1 << (ExponentShift - 1));

            Parts.Ulong.High = HighWord;
            VolatileValue = ClTwo52[SignBit] + Parts.Double;
            Parts.Double = VolatileValue - ClTwo52[SignBit];
            HighWord = Parts.Ulong.High;
            HighWordAbsolute = HighWord &
                               (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

            Parts.Ulong.High = HighWordAbsolute | (SignBit << SignBitShift);
            return Parts.Double;

        } else {
            NonExponent = DOUBLE_HIGH_VALUE_MASK >> Exponent;

            //
            // Return the value itself if it's integral.
            //

            if (((HighWord & NonExponent) | LowWord) == 0) {
                return Value;
            }

            //
            // If a bit is set after the 0.5 bit, adjust the 0.25 guard bit
            // (i.e. the 0.125 bit) to avoid double rounding below when using
            // ClTwo52. Exponents of 18 and 19 are special-cased as they span
            // the word boundary.
            //

            NonExponent >>= 1;
            if (((HighWord & NonExponent) | LowWord) != 0) {
                if (Exponent == 19) {
                    LowWord = RINT_ADJUSTMENT_EXP_19;

                } else if (Exponent == 18) {
                    LowWord = RINT_ADJUSTMENT_EXP_18;

                } else {
                    HighWord = (HighWord & ~NonExponent) |
                               (RINT_ADJUSTMENT_HIGH >> Exponent);
                }
            }
        }

    } else if (Exponent > 51) {

        //
        // Handle infinity or NaN.
        //

        if (Exponent == (DOUBLE_NAN_EXPONENT - DOUBLE_EXPONENT_BIAS)) {
            return Value + Value;

        //
        // Return the value itself if it is integral.
        //

        } else {
            return Value;
        }

    } else {
        NonExponent = ((ULONG)(MAX_ULONG)) >> (Exponent - ExponentShift);

        //
        // Check to see if the value is integral.
        //

        if ((LowWord & NonExponent) == 0) {
            return Value;
        }

        //
        // If a bit is set after the 0.5 bit, adjust the 0.25 guard bit (i.e
        // the 0.125 bit) to avoid double rounding below when using ClTwo52.
        //

        NonExponent >>= 1;
        if ((LowWord & NonExponent) != 0) {
            LowWord = (LowWord & ~NonExponent) |
                      (RINT_ADJUSTMENT_LOW >> (Exponent - ExponentShift));
        }
    }

    Parts.Ulong.High = HighWord;
    Parts.Ulong.Low = LowWord;
    VolatileValue = ClTwo52[SignBit] + Parts.Double;
    Parts.Double = VolatileValue - ClTwo52[SignBit];
    return Parts.Double;
}

//
// --------------------------------------------------------- Internal Functions
//

