/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    exp.c

Abstract:

    This module implements the exponential function.

    Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.

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

#define EXP_UPPER_THRESHOLD_HIGH_WORD 0x40862E42
#define EXP_HALF_LN_2_HIGH_WORD 0x3FD62E42
#define EXP_3_HALVES_LN_2_HIGH_WORD 0x3FF0A2B2
#define EXP_LOWER_THRESHOLD_HIGH_WORD 0x3E300000
#define EXP_2_TO_1023 0x1p1023

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const double ClExpOverflowThreshold = 7.09782712893383973096e+02;
const double ClExpUnderflowThreshold = -7.45133219101941108420e+02;

const double ClExp1 = 1.66666666666666019037e-01;
const double ClExp2 = -2.77777777770155933842e-03;
const double ClExp3 = 6.61375632143793436117e-05;
const double ClExp4 = -1.65339022054652515390e-06;
const double ClExp5 = 4.13813679705723846039e-08;

//
// Define 2^-1000 for underflow.
//

const double ClTwoNegative1000 = 9.33263618503218878990e-302;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
frexp (
    double Value,
    int *Exponent
    )

/*++

Routine Description:

    This routine breaks a floating point number down into a normalized fraction
    and an integer power of 2.

Arguments:

    Value - Supplies the value to normalize.

    Exponent - Supplies a pointer where the exponent will be returned.

Return Value:

    Returns the normalized fraction (the significand).

--*/

{

    LONG AbsoluteHighWord;
    ULONG ExponentShift;
    LONG HighWord;
    LONG LowWord;
    DOUBLE_PARTS Parts;
    LONG SignBit;

    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    LowWord = Parts.Ulong.Low;
    SignBit = (LONG)(DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);
    AbsoluteHighWord = HighWord & (~SignBit);
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    *Exponent = 0;

    //
    // Skip 0, infininy, or NaN.
    //

    if ((AbsoluteHighWord >= NAN_HIGH_WORD) ||
        ((AbsoluteHighWord | LowWord) == 0)) {

        return Value;
    }

    //
    // Handle subnormal values.
    //

    if (AbsoluteHighWord < (1 << ExponentShift)) {
        Value *= ClTwo54;
        Parts.Double = Value;
        HighWord = Parts.Ulong.High;
        AbsoluteHighWord = HighWord & (~SignBit);
        *Exponent = -54;
    }

    *Exponent += (AbsoluteHighWord >> ExponentShift) -
                 (DOUBLE_EXPONENT_BIAS - 1);

    HighWord = (HighWord & (DOUBLE_HIGH_VALUE_MASK | SignBit)) |
               ((DOUBLE_EXPONENT_BIAS - 1) << ExponentShift);

    Parts.Ulong.High = HighWord;
    Value = Parts.Double;
    return Value;
}

LIBC_API
double
exp (
    double Value
    )

/*++

Routine Description:

    This routine computes the base e exponential of the given value.

Arguments:

    Value - Supplies the value to raise e to.

Return Value:

    Returns e to the given value.

--*/

{

    double Approximation;
    double Exponentiation;
    double High;
    ULONG HighWord;
    double Input;
    LONG Ln2Multiple;
    double Low;
    ULONG LowWord;
    LONG SignBit;
    double TwoPower;
    DOUBLE_PARTS ValueParts;
    volatile double VolatileValue;

    //
    // There are three steps to the method.
    // 1. Argument reduction
    //    Reduce x to an r so that |r| <= 0.5*ln2 ~ 0.34658.
    //    Given x, find r and integer k such that
    //
    //    x = k * ln2 + r,  |r| <= 0.5 * ln2.
    //    Here r will be represented as r = hi-lo for better accuracy.
    //
    // 2. Approximation of exp(r) by a special rational function on
    //    the interval [0, 0.34658]:
    //    Write
    //        R(r^22) = r * (exp(r) + 1) / (exp(r) - 1)
    //                = 2 + r*r / 6 - r^4 / 360 + ...
    //    Use a special Remes algorithm on [0,0.34658] to generate
    //    a polynomial of degree 5 to approximate R. The maximum error
    //    of this polynomial approximation is bounded by 2**-59. In
    //    other words,
    //        R(z) ~ 2.0 + P1*z + P2*z^2 + P3*z^3 + P4*z^4 + P5*z^5
    //    (where z = r * r, and the values of P1 to P5 are listed below)
    //    and
    //
    //    |                        5          |     -59
    //    | 2.0 + P1*z + ... + P5*z  -  R(z)  | <= 2
    //    |                                   |
    //
    //    The computation of exp(r) thus becomes
    //                   2*r
    //    exp(r) = 1 + -------
    //                  R - r
    //
    //                      r * R1(r)
    //           = 1 + r + ----------- (for better accuracy)
    //                      2 - R1(r)
    //    where
    //                     2       4             10
    //    R1(r) = r - (P1*r  + P2*r  + ... + P5*r  ).
    //
    // 3. Scale back to obtain exp(x):
    //    From step 1:
    //    exp(x) = 2^k * exp(r)
    //
    // Special cases:
    //    exp(INF) is INF, exp(NaN) is NaN;
    //    exp(-INF) is 0, and
    //    for finite argument, only exp(0)=1 is exact.
    //
    // Accuracy:
    //    according to an error analysis, the error is always less than
    //    1 ulp (unit in the last place).
    //
    // Miscellaneous:
    //    For an IEEE double:
    //    If the value is greater than 7.09782712893383973096e+02 then exp(x)
    //    will overflow.
    //    If the value is less than  -7.45133219101941108420e+02 then exp(x)
    //    will underflow.
    //

    High = 0.0;
    Low = 0.0;
    Ln2Multiple = 0;
    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;
    SignBit = HighWord >> (DOUBLE_SIGN_BIT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    //
    // Get the absolute value.
    //

    HighWord = HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // Filter out a non-finite argument.
    //

    if (HighWord >= EXP_UPPER_THRESHOLD_HIGH_WORD) {
        if (HighWord >= NAN_HIGH_WORD) {
            LowWord = ValueParts.Ulong.Low;
            if (((HighWord & DOUBLE_HIGH_VALUE_MASK) | LowWord) != 0) {

                //
                // Return NaN.
                //

                return Value + Value;
            }

            //
            // Exponentiation of +/- Infinity is Infinity, and 0.
            //

            if (SignBit == 0) {
                return Value;

            } else {
                return 0.0;
            }
        }

        //
        // Handle overflow and underflow cases.
        //

        if (Value > ClExpOverflowThreshold) {
            return ClDoubleHugeValue * ClDoubleHugeValue;
        }

        if (Value < ClExpUnderflowThreshold) {
            return ClTwoNegative1000 * ClTwoNegative1000;
        }
    }

    //
    // Perform argument reduction.
    //

    if (HighWord > EXP_HALF_LN_2_HIGH_WORD) {
        if (HighWord < EXP_3_HALVES_LN_2_HIGH_WORD) {
            High = Value - ClDoubleLn2High[SignBit];
            Low = ClDoubleLn2Low[SignBit];
            Ln2Multiple = 1 - SignBit - SignBit;

        } else {
            if (SignBit != 0) {
                Ln2Multiple = (INT)(ClInverseLn2 * Value - ClDoubleOneHalf);

            } else {
                Ln2Multiple = (INT)(ClInverseLn2 * Value + ClDoubleOneHalf);
            }

            Input = Ln2Multiple;

            //
            // Input * Ln2High is exact here.
            //

            High = Value - Input * ClDoubleLn2High[0];
            Low = Input * ClDoubleLn2Low[0];
        }

        VolatileValue = High - Low;
        Value = VolatileValue;

    //
    // Handle very small values.
    //

    } else if (HighWord < EXP_LOWER_THRESHOLD_HIGH_WORD)  {
        if (ClDoubleHugeValue + Value > ClDoubleOne) {

            //
            // Trigger an inexact condition.
            //

            return ClDoubleOne + Value;
        }

    } else {
        Ln2Multiple = 0;
    }

    //
    // The value is now in the primary range.
    //

    Input  = Value * Value;
    if (Ln2Multiple >= -1021) {
        ValueParts.Ulong.High =
                            DOUBLE_ONE_HIGH_WORD +
                            (Ln2Multiple <<
                             (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT));

    } else {
        ValueParts.Ulong.High =
                            DOUBLE_ONE_HIGH_WORD +
                            ((Ln2Multiple + 1000) <<
                             (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT));
    }

    ValueParts.Ulong.Low = 0;
    TwoPower = ValueParts.Double;
    Approximation = Value - Input *
                    (ClExp1 + Input *
                     (ClExp2 + Input *
                      (ClExp3 + Input *
                       (ClExp4 + Input * ClExp5))));

    if (Ln2Multiple == 0) {
        Exponentiation = ClDoubleOne -
                         ((Value * Approximation) / (Approximation - 2.0) -
                          Value);

        return Exponentiation;
    }

    Exponentiation = ClDoubleOne -
        ((Low - (Value * Approximation) / (2.0 - Approximation)) - High);

    if (Ln2Multiple >= -1021) {
        if (Ln2Multiple == 1024) {
            return Exponentiation * 2.0 * EXP_2_TO_1023;
        }

        return Exponentiation * TwoPower;
    }

    return Exponentiation * TwoPower * ClTwoNegative1000;
}

//
// --------------------------------------------------------- Internal Functions
//

