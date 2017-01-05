/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    logf.c

Abstract:

    This module implements support for the mathematical logarithm function.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Chris Stevens 3-Jan-2017

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

const float ClFloatLog1 = 6.6666668653e-01;
const float ClFloatLog2 = 4.0000000596e-01;
const float ClFloatLog3 = 2.8571429849e-01;
const float ClFloatLog4 = 2.2222198546e-01;
const float ClFloatLog5 = 1.8183572590e-01;
const float ClFloatLog6 = 1.5313838422e-01;
const float ClFloatLog7 = 1.4798198640e-01;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
logf (
    float Value
    )

/*++

Routine Description:

    This routine returns the natural logarithm (base e) of the given value.

Arguments:

    Value - Supplies the value to get the logarithm of.

Return Value:

    Returns the logarithm of the given value.

--*/

{

    float Approximation;
    float Evens;
    LONG Exponent;
    float ExponentFloat;
    LONG ExtraExponent;
    LONG ExtraThreshold;
    float HalfSquare;
    float Input;
    float Input2;
    float Input4;
    float Logarithm;
    float Odds;
    FLOAT_PARTS Parts;
    LONG Threshold;
    float ValueMinusOne;
    LONG Word;

    Parts.Float = Value;
    Word = Parts.Ulong;
    Exponent = 0;

    //
    // Handle a very small value.
    //

    if (Word < (1 << FLOAT_EXPONENT_SHIFT)) {
        if ((Word & ~FLOAT_SIGN_BIT) == 0) {

            //
            // The log(+-0) = -Infinity.
            //

            return -ClFloatTwo25 / ClFloatZero;
        }

        //
        // The log of a negative number is NaN.
        //

        if (Word < 0) {
            return (Value - Value) / ClFloatZero;
        }

        //
        // This is a subnormal number, scale up the value.
        //

        Exponent -= 25;
        Value *= ClFloatTwo25;
        Parts.Float = Value;
        Word = Parts.Ulong;
    }

    if (Word >= FLOAT_NAN) {
        return Value + Value;
    }

    Exponent += (Word >> FLOAT_EXPONENT_SHIFT) - FLOAT_EXPONENT_BIAS;
    Word &= FLOAT_VALUE_MASK;
    ExtraExponent = (Word + (0x95F64 << 3)) & (1 << FLOAT_EXPONENT_SHIFT);

    //
    // Normalize value or half the value.
    //

    Parts.Ulong = Word | (ExtraExponent ^ FLOAT_ONE_WORD);
    Value = Parts.Float;
    Exponent += ExtraExponent >> FLOAT_EXPONENT_SHIFT;
    ValueMinusOne = Value - ClFloatOne;

    //
    // Handle the value minus one being between -2^-9 and 2^-9.
    //

    if ((FLOAT_VALUE_MASK & (0x8000 + Word)) < 0xC000) {
        if (ValueMinusOne == ClFloatZero) {
            if (Exponent == 0) {
                return ClFloatZero;

            } else {
                ExponentFloat = (float)Exponent;
                Logarithm = ExponentFloat * ClFloatLn2High[0] +
                            ExponentFloat * ClFloatLn2Low[0];

                return Logarithm;
            }
        }

        Approximation = ValueMinusOne * ValueMinusOne *
                        ((float)0.5 - (float)0.33333333333333333 *
                         ValueMinusOne);

        if (Exponent == 0) {
            return ValueMinusOne - Approximation;

        } else {
            ExponentFloat = (float)Exponent;
            Logarithm = ExponentFloat * ClFloatLn2High[0] -
                        ((Approximation - ExponentFloat * ClFloatLn2Low[0]) -
                         ValueMinusOne);

            return Logarithm;
        }
    }

    Input = ValueMinusOne / ((float)2.0 + ValueMinusOne);
    ExponentFloat = (float)Exponent;
    Input2 = Input * Input;
    Threshold = Word - (0x6147A << 3);
    Input4 = Input2 * Input2;
    ExtraThreshold = (0x6B851 << 3) - Word;
    Evens = Input4 * (ClFloatLog2 + Input4 *
                      (ClFloatLog4 + Input4 * ClFloatLog6));

    Odds = Input2 * (ClFloatLog1 + Input4 *
                     (ClFloatLog3 + Input4 *
                      (ClFloatLog5 + Input4 * ClFloatLog7)));

    Threshold |= ExtraThreshold;
    Approximation = Odds + Evens;
    if (Threshold > 0) {
        HalfSquare = (float)0.5 * ValueMinusOne * ValueMinusOne;
        if (Exponent == 0) {
            Logarithm = ValueMinusOne -
                        (HalfSquare - Input * (HalfSquare + Approximation));

            return Logarithm;
        }

        Logarithm = ExponentFloat * ClFloatLn2High[0] -
                    ((HalfSquare - (Input * (HalfSquare + Approximation) +
                      ExponentFloat * ClFloatLn2Low[0])) - ValueMinusOne);

        return Logarithm;
    }

    if (Exponent == 0) {
        return ValueMinusOne - Input * (ValueMinusOne - Approximation);
    }

    Logarithm = ExponentFloat * ClFloatLn2High[0] -
                ((Input * (ValueMinusOne - Approximation) -
                  ExponentFloat * ClFloatLn2Low[0]) - ValueMinusOne);

    return Logarithm;
}

//
// --------------------------------------------------------- Internal Functions
//

