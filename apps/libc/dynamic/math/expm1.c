/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    expm1.c

Abstract:

    This module implements support for the expentiation function, minus one.
    This is apparently useful in financial situations.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 25-Aug-2016

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

#define EXPM1_56LN2_HIGH_WORD 0x4043687A
#define EXPM1_UPPER_LIMIT_HIGH_WORD 0x40862E42
#define EXPM1_HALF_LN2_HIGH_WORD 0x3FD62E42
#define EXPM1_3LN2_OVER_2_HIGH_WORD 0x3FF0A2B2
#define EXPM1_2_TO_NEGATIVE_54_HIGH_WORD 0x3C900000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

static const double ClExpm1Threshold = 7.09782712893383973096e+02;
static const double ClExpm11 = -3.33333333333331316428e-02;
static const double ClExpm12 = 1.58730158725481460165e-03;
static const double ClExpm13 = -7.93650757867487942473e-05;
static const double ClExpm14 = 4.00821782732936239552e-06;
static const double ClExpm15 = -2.01099218183624371326e-07;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
expm1 (
    double Value
    )

/*++

Routine Description:

    This routine computes the base e exponential of the given value, minus one.

Arguments:

    Value - Supplies the value to raise e to.

Return Value:

    Returns e to the given value, minus one.

--*/

{

    double Correction;
    double Error;
    LONG Exponent;
    double HalfValue;
    double HalfValueSquared;
    double High;
    ULONG HighWord;
    double Low;
    ULONG LowWord;
    double Rational;
    double Result;
    LONG SignBit;
    double TwoRaisedExponent;
    DOUBLE_PARTS ValueParts;
    volatile double VolatileValue;
    double Working;

    //
    // There are three steps to the method.
    // 1. Argument reduction
    //    Given x, find r and integer k such that
    //
    //    x = k * ln2 + r,  |r| <= 0.5 * ln2 (~0.34658).
    //    A correction term c will be computed to compensate for the error in
    //    r introduced by rounding.
    //
    // 2. Approximation of expm1(r) by a special rational function on
    //    the interval [0, 0.34658]:
    //    Since
    //        r * (exp(r) + 1) / (exp(r) - 1) = 2 + r^2 / 6 + r^4 / 360 + ...
    //    Define R1(r * r) =
    //        r * (exp(r) + 1) / (exp(r) - 1) = 2 + r^2 / 6 + R1(r * r)
    //    That is
    //        R1(r^2) = 6 / r * ((exp(r) + 1) / (exp(r) - 1) - 2 / r)
    //                = 1 - r^2 / 60 + r^4 / 2520 - r^6 / 100800 + ...
    //    Use a special Remes algorithm on [0,0.347] to generate a polynomial
    //    of degree 5 in r * r to approximate R1. The maximum error of this
    //    polynomial approximation is bounded by 2**-61. In other words,
    //        R1(z) ~ 1.0 + P1*z + P2*z^2 + P3*z^3 + P4*z^4 + P5*z^5
    //    (where z = r * r, and the values of P1 to P5 are listed below)
    //    and the error is bounded by
    //
    //    |                        5          |     -61
    //    | 1.0 + P1*z + ... + P5*z  - R1(z)  | <= 2
    //    |                                   |
    //
    //    The computation of exp(r) thus becomes
    //                     2     3
    //                    r     r      3 - (R1 + R1 * r / 2)
    //    expm1(r) = r + --- + --- +  -----------------------
    //                    2     2     6 - r * (3 - R1 * r / 2)
    //
    //    To compensate for the error, use
    //        expm1(r + c) = expm1(r) + c + expm1(r) * c
    //                     ~ expm1(r) + c + r * c
    //    Thus c + r * c will be added in as the correction terms for
    //    expm1(r + c). Now rearrange:
    //                            2                                          2
    //                   ({    ( r    R1 - (3 - R1 * r / 2        )    }    r  )
    //    expm1(r) ~ r - ({r * (--- * ------------------------ - c) - c} - --- )
    //                   ({    ( 2    6 - r * (3 - R1 * r / 2)    )    }       )
    //
    //             = r - E
    // 3. Scale back to obtain expm1(x):
    //    From step 1:
    //    expm1(x) = 2^k * [expm1(r) + 1] - 1 OR
    //             = 2^k * [expm1(r) + (1 - 2^-k)]
    //
    // Special cases:
    //    expm1(INF) is INF, expm1(NaN) is NaN;
    //    expm1(-INF) is -1, and
    //    for finite argument, only exp(0)=0 is exact.
    //
    // Accuracy:
    //    according to an error analysis, the error is always less than
    //    1 ulp (unit in the last place).
    //
    // Miscellaneous:
    //    For an IEEE double:
    //    If the value is greater than 7.09782712893383973096e+02 then exp(x)
    //    will overflow.
    //

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;

    //
    // Get and mask off the sign bit from the high word.
    //

    SignBit = HighWord >> (DOUBLE_SIGN_BIT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);
    HighWord = HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // Handle gigantic and non-finite arguments.
    //

    if (HighWord >= EXPM1_56LN2_HIGH_WORD) {
        if (HighWord >= EXPM1_UPPER_LIMIT_HIGH_WORD) {
            if (HighWord >= NAN_HIGH_WORD) {
                LowWord = ValueParts.Ulong.Low;

                //
                // Return NaN.
                //

                if (((HighWord & DOUBLE_HIGH_VALUE_MASK) | LowWord) != 0) {
                     return Value + Value;

                } else {

                    //
                    // expm1(+-INF) = {INF, -1}.
                    //

                    if (SignBit == 0) {
                        return Value;
                    }

                    return -1.0;
                }
            }

            if (Value > ClExpm1Threshold) {

                //
                // Overflow.
                //

                return ClDoubleHugeValue * ClDoubleHugeValue;
            }
        }

        //
        // If the value is < -56ln2, return -1.0 with inexact.
        //

        if (SignBit != 0) {
            if (Value + ClDoubleTinyValue < 0.0) {
                return ClDoubleTinyValue - ClDoubleOne;
            }
        }
    }

    //
    // Perform argument reduction.
    //

    if (HighWord > EXPM1_HALF_LN2_HIGH_WORD) {

        //
        // Handle |Value| > 0.5 ln2 and |Value| < 1.5ln2.
        //

        if (HighWord < EXPM1_3LN2_OVER_2_HIGH_WORD) {
            if (SignBit == 0) {
                High = Value - ClDoubleLn2High[0];
                Low = ClDoubleLn2Low[0];
                Exponent = 1;

            } else {
                High = Value + ClDoubleLn2High[0];
                Low = ClDoubleLn2Low[1];
                Exponent = -1;
            }

        } else {
            if (SignBit == 0) {
                Exponent = ClInverseLn2 * Value + 0.5;

            } else {
                Exponent = ClInverseLn2 * Value - 0.5;
            }

            Working = Exponent;
            High = Value - Working * ClDoubleLn2High[0];
            Low = Working * ClDoubleLn2Low[0];
        }

        VolatileValue = High - Low;
        Value = VolatileValue;
        Correction = (High - Value) - Low;

    //
    // When the |Value| is less than 2^-54, return the Value itself. Return
    // the value with inexact flags when the value is non-zero.
    //

    } else if (HighWord < EXPM1_2_TO_NEGATIVE_54_HIGH_WORD) {
        Working = ClDoubleHugeValue + Value;
        return Value - (Working - (ClDoubleHugeValue + Value));

    } else {
        Exponent = 0;
    }

    //
    // The value is now in range.
    //

    HalfValue = 0.5 * Value;
    HalfValueSquared = Value * HalfValue;
    Rational = ClDoubleOne + HalfValueSquared *
               (ClExpm11 + HalfValueSquared *
                (ClExpm12 + HalfValueSquared *
                 (ClExpm13 + HalfValueSquared *
                  (ClExpm14 + HalfValueSquared * ClExpm15))));

    Working = 3.0 - Rational * HalfValue;
    Error = HalfValueSquared * ((Rational - Working) / (6.0 - Value * Working));
    if (Exponent == 0) {

        //
        // The correction is zero in this case.
        //

        return Value - (Value * Error - HalfValueSquared);

    } else {
        ValueParts.Ulong.High =
                             (DOUBLE_EXPONENT_BIAS + Exponent) <<
                             (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

        ValueParts.Ulong.Low = 0;
        TwoRaisedExponent = ValueParts.Double;
        Error = (Value * (Error - Correction) - Correction);
        Error -= HalfValueSquared;
        if (Exponent == -1) {
            return 0.5 * (Value - Error) - 0.5;
        }

        if (Exponent == 1) {
            if (Value < -0.25) {
                return -2.0 * (Error - (Value + 0.5));

            } else {
                return ClDoubleOne + 2.0 * (Value - Error);
            }
        }

        //
        // Return exp(Value) - 1.
        //

        if ((Exponent <= -2) || (Exponent > 56)) {
            Result = ClDoubleOne - (Error - Value);
            if (Exponent == 1024) {
                Result = Result * 2.0 * 0x1p1023;

            } else {
                Result = Result * TwoRaisedExponent;
            }

            return Result - ClDoubleOne;
        }

        ValueParts.Double = ClDoubleOne;
        if (Exponent < 20) {

            //
            // Create 1 - 2^Exponent.
            //

            ValueParts.Ulong.High -=
                     (2 << (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT)) >>
                     Exponent;

            Result = ValueParts.Double - (Error - Value);
            Result = Result * TwoRaisedExponent;

       } else {

            //
            // Create 2^-Exponent.
            //

            ValueParts.Ulong.High =
                              (DOUBLE_EXPONENT_BIAS - Exponent) <<
                              (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

            Result = Value - (Error + ValueParts.Double);
            Result += ClDoubleOne;
            Result = Result * TwoRaisedExponent;
        }
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

