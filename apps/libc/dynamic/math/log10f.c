/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    log10f.c

Abstract:

    This module implements support for the base 10 logarithm function.

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

const float ClFloatInverseLn10High = 4.3432617188e-01;
const float ClFloatInverseLn10Low = -3.1689971365e-05;
const float ClFloatLog10Of2High = 3.0102920532e-01;
const float ClFloatLog10Of2Low = 7.9034151668e-07;

const float ClFloatLgValue1 = 6.6666662693e-01;
const float ClFloatLgValue2 = 4.0000972152e-01;
const float ClFloatLgValue3 = 2.8498786688e-01;
const float ClFloatLgValue4 = 2.4279078841e-01;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
log10f (
    float Value
    )

/*++

Routine Description:

    This routine returns the base 10 logarithm of the given value.

Arguments:

    Value - Supplies the value to get the logarithm of.

Return Value:

    Returns the logarithm of the given value.

--*/

{

    LONG Exponent;
    float ExponentFloat;
    LONG ExtraExponent;
    float HalfSquare;
    float High;
    float LogResult;
    float Low;
    FLOAT_PARTS Parts;
    float Result;
    float ValueMinusOne;
    LONG Word;

    //
    // The method is generally the same as the log() function.
    // log10(x) = (ValueMinusOne - 0.5 * ValueMinusOne^2 +
    //             log1plus(ValueMinusOne)) / ln10 + Exponent * log10(2)
    //

    Parts.Float = Value;
    Word = Parts.Ulong;
    Exponent = 0;

    //
    // Handle values less than 2^-126.
    //

    if (Word < (1 << FLOAT_EXPONENT_SHIFT)) {

        //
        // Log(+-0) is -Infinity.
        //

        if ((Word & ~FLOAT_SIGN_BIT) == 0) {
            return -ClFloatTwo25 / ClFloatZero;
        }

        //
        // Log of a negative number is NaN.
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

    //
    // Log(1) is zero.
    //

    if (Word == FLOAT_ONE_WORD) {
        return ClFloatZero;
    }

    Exponent += (Word >> FLOAT_EXPONENT_SHIFT) - FLOAT_EXPONENT_BIAS;
    Word &= FLOAT_VALUE_MASK;
    ExtraExponent = (Word + 0x4afb0d) & (1 << FLOAT_EXPONENT_SHIFT);

    //
    // Normalize value or value / 2.
    //

    Parts.Ulong = Word | (ExtraExponent ^ FLOAT_ONE_WORD);
    Value = Parts.Float;
    Exponent += (ExtraExponent >> FLOAT_EXPONENT_SHIFT);
    ExponentFloat = (float)Exponent;
    ValueMinusOne = Value - ClFloatOne;
    HalfSquare = (float)0.5 * ValueMinusOne * ValueMinusOne;
    LogResult = ClpLogOnePlusFloat(ValueMinusOne);

    //
    // See the log2 and log2f function for more details about this.
    //

    High = ValueMinusOne - HalfSquare;
    Parts.Float = High;
    Word = Parts.Ulong;
    Word &= FLOAT_TRUNCATE_VALUE_MASK;
    Parts.Ulong = Word;
    High = Parts.Float;
    Low = (ValueMinusOne - High) - HalfSquare + LogResult;
    Result = (ExponentFloat * ClFloatLog10Of2Low) +
             ((Low + High) * ClFloatInverseLn10Low) +
             (Low * ClFloatInverseLn10High) +
             (High * ClFloatInverseLn10High) +
             (ExponentFloat * ClFloatLog10Of2High);

    return Result;
}

float
ClpLogOnePlusFloat (
    float Value
    )

/*++

Routine Description:

    This routine returns log(1 + value) - value for 1 + value in
    ~[sqrt(2)/2, sqrt(2)].

Arguments:

    Value - Supplies the input value to compute log(1 + value) for.

Return Value:

    Returns log(1 + value).

--*/

{

    float Approximation;
    float Evens;
    float HalfSquare;
    float Odds;
    float ScaledValue;
    float ScaledValue2;
    float ScaledValue4;

    ScaledValue = Value / ((float)2.0 + Value);
    ScaledValue2 = ScaledValue * ScaledValue;
    ScaledValue4 = ScaledValue2 * ScaledValue2;
    Evens = ScaledValue4 * (ClFloatLgValue2 + ScaledValue4 * ClFloatLgValue4);
    Odds = ScaledValue2 * (ClFloatLgValue1 + ScaledValue4 * ClFloatLgValue3);
    Approximation = Odds + Evens;
    HalfSquare = (float)0.5 * Value * Value;
    return ScaledValue * (HalfSquare + Approximation);
}

//
// --------------------------------------------------------- Internal Functions
//

