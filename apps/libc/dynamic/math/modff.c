/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    modff.c

Abstract:

    This module implements support for the mod function, which splits a
    floating point value into an integer portion and a fraction portion.

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

//
// ---------------------------------------------------------------- Definitions
//

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
float
modff (
    float Value,
    float *IntegerPortion
    )

/*++

Routine Description:

    This routine breaks the given value up into integral and fractional parts,
    each of which has the same sign as the argument. It stores the integral
    part as a floating point value.

Arguments:

    Value - Supplies the value to decompose into an integer and a fraction.

    IntegerPortion - Supplies a pointer where the integer portion of the
        value will be returned. If the given value is NaN or +/- Infinity, then
        NaN or +/- Infinity will be returned.

Return Value:

    Returns the fractional portion of the given value on success.

    NaN if the input is NaN.

    0 if +/- Infinity is given.

--*/

{

    LONG Exponent;
    ULONG FractionMask;
    FLOAT_PARTS Parts;
    LONG Word;

    Parts.Float = Value;
    Word = Parts.Ulong;
    Exponent = ((Word & FLOAT_EXPONENT_MASK) >> FLOAT_EXPONENT_SHIFT) -
               FLOAT_EXPONENT_BIAS;

    //
    // If the exponent is small, then there's some fractional part in the word.
    //

    if (Exponent < 23) {

        //
        // If the exponent is negative the absolute value is less than 1,
        // so there is no integer portion. Copy the sign and return the value
        // as the fractional portion.
        //

        if (Exponent < 0) {
            Parts.Ulong = Word & FLOAT_SIGN_BIT;
            *IntegerPortion = Parts.Float;
            return Value;

        } else {
            FractionMask = FLOAT_VALUE_MASK >> Exponent;

            //
            // If the fraction mask portion is zero, the value is integral.
            // Return +/- 0 to match the sign.
            //

            if ((Word & FractionMask) == 0) {
                *IntegerPortion = Value;
                Parts.Ulong = Word & FLOAT_SIGN_BIT;
                return Parts.Float;
            }

            Parts.Ulong = Word & ~FractionMask;
            *IntegerPortion = Parts.Float;
            return Value - *IntegerPortion;
        }
    }

    //
    // Otherwise all the bits are integral bits. Handle NaN.
    //

    *IntegerPortion = Value * ClFloatOne;
    if (Value != Value) {
        return Value;
    }

    //
    // Return +/- 0.
    //

    Parts.Ulong = Word & FLOAT_SIGN_BIT;
    return Parts.Float;
}

//
// --------------------------------------------------------- Internal Functions
//

