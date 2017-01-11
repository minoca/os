/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    scalbnf.c

Abstract:

    This module implements support for the scalbn (scale binary) family of
    functions.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Chris Stevens 4-Jan-2017

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

#define FLOAT_HUGE_VALUE_EXPONENT 0xFE

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const float ClTwoNegative25 = 2.9802322388e-08;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
ldexpf (
    float Value,
    int Exponent
    )

/*++

Routine Description:

    This routine computes the given value times two raised to the given
    exponent efficiently. That is, Value * (2 ^ Exponent). On systems where
    FLT_RADIX is 2, this is equivalent to the scalbn function.

Arguments:

    Value - Supplies the value to multiply.

    Exponent - Supplies the exponent to raise two to.

Return Value:

    Returns the scaled value.

--*/

{

    return scalbnf(Value, Exponent);
}

LIBC_API
float
scalbnf (
    float Value,
    int Exponent
    )

/*++

Routine Description:

    This routine computes the given value times FLT_RADIX raised to the given
    exponent efficiently. That is, Value * 2 ^ Exponent.

Arguments:

    Value - Supplies the value to multiply.

    Exponent - Supplies the exponent to raise the radix to.

Return Value:

    Returns the scaled value.

--*/

{

    LONG ValueExponent;
    FLOAT_PARTS ValueParts;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;

    //
    // Get the exponent of the value.
    //

    ValueExponent = (Word & FLOAT_EXPONENT_MASK) >> FLOAT_EXPONENT_SHIFT;

    //
    // Watch out for zero or a subnormal value.
    //

    if (ValueExponent == 0) {

        //
        // Handle +0 and -0.
        //

        if ((Word & ~FLOAT_SIGN_BIT) == 0) {
            return Value;
        }

        Value *= ClFloatTwo25;
        ValueParts.Float = Value;
        Word = ValueParts.Ulong;
        ValueExponent = ((Word & FLOAT_EXPONENT_MASK) >>
                         FLOAT_EXPONENT_SHIFT) - 25;

        //
        // Handle underflow.
        //

        if (Exponent < -50000) {
            return ClFloatTinyValue * Value;
        }
    }

    //
    // Handle NaN or infinity.
    //

    if (ValueExponent == (FLOAT_NAN >> FLOAT_EXPONENT_SHIFT)) {
        return Value + Value;
    }

    ValueExponent = ValueExponent + Exponent;
    if (ValueExponent > FLOAT_HUGE_VALUE_EXPONENT) {
        return ClFloatHugeValue * copysignf(ClFloatHugeValue, Value);
    }

    //
    // This is a normal looking value.
    //

    if (ValueExponent > 0) {
        ValueParts.Ulong = (Word & ~FLOAT_EXPONENT_MASK) |
                           (ValueExponent << FLOAT_EXPONENT_SHIFT);

        return ValueParts.Float;
    }

    if (ValueExponent <= -25) {

            //
            // Watch out for integer overflow in the exponent plus the value
            // exponent.
            //

            if (Exponent > 50000) {
                return ClFloatHugeValue * copysignf(ClFloatHugeValue, Value);

            } else {
                return ClFloatTinyValue * copysignf(ClFloatTinyValue, Value);
            }
    }

    //
    // This is a subnormal result.
    //

    ValueExponent += 25;
    ValueParts.Ulong = (Word & ~FLOAT_EXPONENT_MASK) |
                       (ValueExponent << FLOAT_EXPONENT_SHIFT);

    Value = ValueParts.Float;
    return Value * ClTwoNegative25;
}

//
// --------------------------------------------------------- Internal Functions
//

