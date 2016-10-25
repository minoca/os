/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    scalbn.c

Abstract:

    This module implements support for the scalbn (scale binary) family of
    functions.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 23-Jul-2013

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

#define DOUBLE_HUGE_VALUE_EXPONENT 0x7FE

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const double ClTwoNegative54 = 5.55111512312578270212e-17;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
ldexp (
    double Value,
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

    return scalbn(Value, Exponent);
}

LIBC_API
double
scalbn (
    double Value,
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

    ULONG ExponentHighWordMask;
    ULONG ExponentHighWordShift;
    LONG HighWord;
    LONG LowWord;
    LONG ValueExponent;
    DOUBLE_PARTS ValueParts;

    ExponentHighWordMask = DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT;
    ExponentHighWordShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;
    LowWord = ValueParts.Ulong.Low;

    //
    // Get the exponent of the value.
    //

    ValueExponent = (HighWord & ExponentHighWordMask) >> ExponentHighWordShift;

    //
    // Watch out for zero or a subnormal value.
    //

    if (ValueExponent == 0) {

        //
        // Handle +0 and -0.
        //

        if ((LowWord |
            (HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT))) == 0) {

            return Value;
        }

        Value *= ClTwo54;
        ValueParts.Double = Value;
        HighWord = ValueParts.Ulong.High;
        ValueExponent = ((HighWord & ExponentHighWordMask) >>
                         ExponentHighWordShift) - 54;

        //
        // Handle underflow.
        //

        if (Exponent< -50000) {
            return ClTinyValue * Value;
        }
    }

    //
    // Handle NaN or infinity.
    //

    if (ValueExponent == NAN_HIGH_WORD >> ExponentHighWordShift) {
        return Value + Value;
    }

    ValueExponent = ValueExponent + Exponent;
    if (ValueExponent > DOUBLE_HUGE_VALUE_EXPONENT) {
        return ClHugeValue * copysign(ClHugeValue, Value);
    }

    //
    // This is a normal looking value.
    //

    if (ValueExponent > 0) {
        ValueParts.Ulong.High = (HighWord & (~ExponentHighWordMask)) |
                                (ValueExponent << ExponentHighWordShift);

        return ValueParts.Double;
    }

    if (ValueExponent <= -54) {

            //
            // Watch out for integer overflow in the exponent plus the value
            // exponent.
            //

            if (Exponent > 50000) {
                return ClHugeValue * copysign(ClHugeValue, Value);

            } else {
                return ClTinyValue * copysign(ClTinyValue, Value);
            }
    }

    //
    // This is a subnormal result.
    //

    ValueExponent += 54;
    ValueParts.Ulong.High = (HighWord & (~ExponentHighWordMask)) |
                            (ValueExponent << ExponentHighWordShift);

    return Value * ClTwoNegative54;
}

//
// --------------------------------------------------------- Internal Functions
//

