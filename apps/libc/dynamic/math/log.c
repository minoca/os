/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    log.c

Abstract:

    This module implements support for the mathematical logarithm function.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 24-Jul-2013

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

const double ClLog1 = 6.666666666666735130e-01;
const double ClLog2 = 3.999999999940941908e-01;
const double ClLog3 = 2.857142874366239149e-01;
const double ClLog4 = 2.222219843214978396e-01;
const double ClLog5 = 1.818357216161805012e-01;
const double ClLog6 = 1.531383769920937332e-01;
const double ClLog7 = 1.479819860511658591e-01;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
log (
    double Value
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

    double Approximation;
    double Evens;
    LONG Exponent;
    double ExponentDouble;
    ULONG ExponentShift;
    LONG ExtraExponent;
    LONG ExtraThreshold;
    double HalfSquare;
    LONG HighWord;
    double Input;
    double Input2;
    double Input4;
    double Logarithm;
    ULONG LowWord;
    double Odds;
    DOUBLE_PARTS Parts;
    LONG Threshold;
    double ValueMinusOne;

    //
    // Method :
    // 1. Argument Reduction: find k and f such that
    //        x = 2^k * (1+f),
    //    where  sqrt(2)/2 < 1+f < sqrt(2).
    //
    // 2. Approximation of log(1 + f).
    //    Let s = f / (2 + f); based on log(1 + f) = log(1 + s) - log(1 - s)
    //          = 2s + 2/3 s^3 + 2/5 s^5 + .....,
    //          = 2s + s*R
    //
    //    Use a special Reme algorithm on [0, 0.1716] to generate
    //    a polynomial of degree 14 to approximate R. The maximum error
    //    of this polynomial approximation is bounded by 2^-58.45. In
    //    other words,
    //                2       4       6       8       10       12       14
    //    R(z) ~ Lg1*s + Lg2*s + Lg3*s + Lg4*s + Lg5*s  + Lg6*s  + Lg7*s
    //
    //    (the values of Lg1 to Lg7 are listed in the program) and
    //
    //    |      2          14          |     -58.45
    //    | Lg1*s +...+Lg7*s    -  R(z) | <= 2
    //    |                             |
    //
    //    Note that 2s = f - s*f = f - hfsq + s*hfsq, where hfsq = f*f/2.
    //    In order to guarantee error in log below 1ulp, we compute log by
    //        log(1 + f) = f - s * (f - R)              (if f is not too large)
    //        log(1 + f) = f - (hfsq - s * (hfsq + R)). (better accuracy)
    //
    // 3. Finally,
    //        log(x) = k * ln2 + log(1 + f).
    //               = k * ln2_hi + (f - (hfsq - (s * (hfsq + R) + k * ln2_lo)))
    //
    //    Here ln2 is split into two floating point number:
    //        ln2_hi + ln2_lo,
    //    where n * ln2_hi is always exact for |n| < 2000.
    //
    // Special cases:
    //    log(x) is NaN with signal if x < 0 (including -INF);
    //    log(+INF) is +INF; log(0) is -INF with signal;
    //    log(NaN) is that NaN with no signal.
    //
    // Accuracy:
    //    According to an error analysis, the error is always less than
    //    1 ulp (unit in the last place).
    //

    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    LowWord = Parts.Ulong.Low;
    Exponent = 0;

    //
    // Handle a very small value.
    //

    if (HighWord < (1 << ExponentShift)) {
        if (((HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) |
             LowWord) == 0) {

            //
            // The log(+-0) = -Infinity.
            //

            return -ClTwo54 / ClDoubleZero;
        }

        //
        // The log of a negative number is NaN.
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

    Exponent += (HighWord >> ExponentShift) - DOUBLE_EXPONENT_BIAS;
    HighWord &= DOUBLE_HIGH_VALUE_MASK;
    ExtraExponent = (HighWord + 0x95F64) & (1 << ExponentShift);

    //
    // Normalize value or half the value.
    //

    Parts.Double = Value;
    Parts.Ulong.High = HighWord | (ExtraExponent ^ DOUBLE_ONE_HIGH_WORD);
    Value = Parts.Double;
    Exponent += ExtraExponent >> ExponentShift;
    ValueMinusOne = Value - 1.0;

    //
    // Handle the value minus one being between -2^-20 and 2^-20.
    //

    if ((DOUBLE_HIGH_VALUE_MASK & (2 + HighWord)) < 3) {
        if (ValueMinusOne == ClDoubleZero) {
            if (Exponent == 0) {
                return ClDoubleZero;

            } else {
                ExponentDouble = (double)Exponent;
                Logarithm = ExponentDouble * ClDoubleLn2High[0] +
                            ExponentDouble * ClDoubleLn2Low[0];

                return Logarithm;
            }
        }

        Approximation = ValueMinusOne * ValueMinusOne *
                        (0.5 - 0.33333333333333333 * ValueMinusOne);

        if (Exponent == 0) {
            return ValueMinusOne - Approximation;

        } else {
            ExponentDouble = (double)Exponent;
            Logarithm = ExponentDouble * ClDoubleLn2High[0] -
                        ((Approximation - ExponentDouble * ClDoubleLn2Low[0]) -
                         ValueMinusOne);

            return Logarithm;
        }
    }

    Input = ValueMinusOne / (2.0 + ValueMinusOne);
    ExponentDouble = (double)Exponent;
    Input2 = Input * Input;
    Threshold = HighWord - 0x6147a;
    Input4 = Input2 * Input2;
    ExtraThreshold = 0x6b851 - HighWord;
    Evens = Input4 * (ClLog2 + Input4 * (ClLog4 + Input4 * ClLog6));
    Odds = Input2 * (ClLog1 + Input4 *
                     (ClLog3 + Input4 * (ClLog5 + Input4 * ClLog7)));

    Threshold |= ExtraThreshold;
    Approximation = Odds + Evens;
    if (Threshold > 0) {
        HalfSquare = 0.5 * ValueMinusOne * ValueMinusOne;
        if (Exponent == 0) {
            Logarithm = ValueMinusOne -
                        (HalfSquare - Input * (HalfSquare + Approximation));

            return Logarithm;
        }

        Logarithm = ExponentDouble * ClDoubleLn2High[0] -
                    ((HalfSquare - (Input * (HalfSquare + Approximation) +
                      ExponentDouble * ClDoubleLn2Low[0])) - ValueMinusOne);

        return Logarithm;
    }

    if (Exponent == 0) {
        return ValueMinusOne - Input * (ValueMinusOne - Approximation);
    }

    Logarithm = ExponentDouble * ClDoubleLn2High[0] -
                ((Input * (ValueMinusOne - Approximation) -
                  ExponentDouble * ClDoubleLn2Low[0]) - ValueMinusOne);

    return Logarithm;
}

//
// --------------------------------------------------------- Internal Functions
//

