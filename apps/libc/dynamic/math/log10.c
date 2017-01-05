/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    log10.c

Abstract:

    This module implements support for the base 10 logarithm function.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 13-Aug-2013

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

const double ClInverseLn10High = 4.34294481878168880939e-01;
const double ClInverseLn10Low = 2.50829467116452752298e-11;
const double ClLog10Of2High = 3.01029995663611771306e-01;
const double ClLog10Of2Low = 3.69423907715893078616e-13;

const double ClLgValue1 = 6.666666666666735130e-01;
const double ClLgValue2 = 3.999999999940941908e-01;
const double ClLgValue3 = 2.857142874366239149e-01;
const double ClLgValue4 = 2.222219843214978396e-01;
const double ClLgValue5 = 1.818357216161805012e-01;
const double ClLgValue6 = 1.531383769920937332e-01;
const double ClLgValue7 = 1.479819860511658591e-01;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
log10 (
    double Value
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
    double ExponentDouble;
    double Extra;
    LONG ExtraExponent;
    double HalfSquare;
    double High;
    LONG HighWord;
    double Log10Exponent;
    double LogResult;
    double Low;
    ULONG LowWord;
    LONG OneExponent;
    DOUBLE_PARTS Parts;
    double ResultHigh;
    double ResultLow;
    double ValueMinusOne;

    OneExponent = 1 << (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    //
    // The method is generally the same as the log() function.
    // log10(x) = (ValueMinusOne - 0.5 * ValueMinusOne^2 +
    //             log1plus(ValueMinusOne)) / ln10 + Exponent * log10(2)
    //

    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    LowWord = Parts.Ulong.Low;
    Exponent = 0;

    //
    // Handle values less than 2^-1022.
    //

    if (HighWord < OneExponent) {

        //
        // Log(+-0) is -Infinity.
        //

        if (((HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) |
             LowWord) == 0) {

            return -ClTwo54 / ClDoubleZero;
        }

        //
        // Log of a negative number is NaN.
        //

        if (HighWord < 0) {
            return (Value - Value) / ClDoubleZero;
        }

        //
        // This is a subnormal number, scale up the value.
        //

        Exponent -= 54;
        Value *= ClTwo54;
        Parts.Double = Value;
        HighWord = Parts.Ulong.High;
    }

    if (HighWord >= NAN_HIGH_WORD) {
        return Value + Value;
    }

    //
    // Log(1) is zero.
    //

    if ((HighWord == DOUBLE_ONE_HIGH_WORD) && (LowWord == 0)) {
        return ClDoubleZero;
    }

    Exponent += (HighWord >> (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT)) -
                DOUBLE_EXPONENT_BIAS;

    HighWord &= DOUBLE_HIGH_VALUE_MASK;
    ExtraExponent = (HighWord + 0x95F64) & OneExponent;

    //
    // Normalize value or value / 2.
    //

    Parts.Ulong.High = HighWord | (ExtraExponent ^ DOUBLE_ONE_HIGH_WORD);
    Value = Parts.Double;
    Exponent += (ExtraExponent >>
                 (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT));

    ExponentDouble = (double)Exponent;
    ValueMinusOne = Value - 1.0;
    HalfSquare = 0.5 * ValueMinusOne * ValueMinusOne;
    LogResult = ClpLogOnePlus(ValueMinusOne);

    //
    // See the log2 function for more details about this.
    //

    High = ValueMinusOne - HalfSquare;
    Parts.Double = High;
    Parts.Ulong.Low = 0;
    High = Parts.Double;
    Low = (ValueMinusOne - High) - HalfSquare + LogResult;
    ResultHigh = High * ClInverseLn10High;
    Log10Exponent = ExponentDouble * ClLog10Of2High;
    ResultLow = ExponentDouble * ClLog10Of2Low +
                (Low + High) * ClInverseLn10Low +
                Low * ClInverseLn10High;

    //
    // Extra precision in for adding ExponentDouble * Log10Of2High is not
    // strictly needed since there is no very large cancellation near
    // Value = sqrt(2) or Value = 1 / sqrt(2), but we do it anyway since it
    // costs little on CPUs with some parallelism and it reduces the error for
    // many args.
    //

    Extra = Log10Exponent + ResultHigh;
    ResultLow += (Log10Exponent - Extra) + ResultHigh;
    ResultHigh = Extra;
    return ResultLow + ResultHigh;
}

double
ClpLogOnePlus (
    double Value
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

    double Approximation;
    double Evens;
    double HalfSquare;
    double Odds;
    double ScaledValue;
    double ScaledValue2;
    double ScaledValue4;

    ScaledValue = Value / (2.0 + Value);
    ScaledValue2 = ScaledValue * ScaledValue;
    ScaledValue4 = ScaledValue2 * ScaledValue2;
    Evens = ScaledValue4 * (ClLgValue2 + ScaledValue4 *
                            (ClLgValue4 + ScaledValue4 * ClLgValue6));

    Odds = ScaledValue2 * (ClLgValue1 + ScaledValue4 *
                           (ClLgValue3 + ScaledValue4 *
                            (ClLgValue5 + ScaledValue4 * ClLgValue7)));

    Approximation = Odds + Evens;
    HalfSquare = 0.5 * Value * Value;
    return ScaledValue * (HalfSquare + Approximation);
}

//
// --------------------------------------------------------- Internal Functions
//

