/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    trigarc.c

Abstract:

    This module implements support for the inverse trigonometric functions:
    arc sine arc cosine, and arc tangent.

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

#define ARC_SINE_LOW_THRESHOLD_HIGH_WORD 0x3E500000
#define ARC_SINE_UPPER_APPROXIMATION_HIGH_WORD 0x3FEF3333
#define ARC_COSINE_LOW_THRESHOLD_HIGH_WORD 0x3C600000
#define ARC_TANGENT_HIGH_THRESHOLD_HIGH_WORD 0x44100000
#define ARC_TANGENT_LOW_THRESHOLD_HIGH_WORD 0x3FDC0000
#define ARC_TANGENT_ZERO_THRESHOLD_HIGH_WORD 0x3E400000
#define ARC_TANGENT_MIDDLE_THRESHOLD_HIGH_WORD 0x3FF30000
#define ARC_TANGENT_MIDDLE_LOW_THRESHOLD_HIGH_WORD 0x3FE60000
#define ARC_TANGENT_MIDDLE_HIGH_THRESHOLD_HIGH_WORD 0x40038000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const double ClPiLow = 1.2246467991473531772E-16;
const double ClPiOver2High = 1.57079632679489655800e+00;
const double ClPiOver2Low = 6.12323399573676603587e-17;
const double ClPiOver4High = 7.8539816339744827900E-01;

//
// Define the coefficients used the in the R(x^2) calculation used for arc sine.
//

double ClArcSineNumerator0 =  1.66666666666666657415e-01;
double ClArcSineNumerator1 = -3.25565818622400915405e-01;
double ClArcSineNumerator2 =  2.01212532134862925881e-01;
double ClArcSineNumerator3 = -4.00555345006794114027e-02;
double ClArcSineNumerator4 =  7.91534994289814532176e-04;
double ClArcSineNumerator5 =  3.47933107596021167570e-05;
double ClArcSineDenominator1 = -2.40339491173441421878e+00;
double ClArcSineDenominator2 =  2.02094576023350569471e+00;
double ClArcSineDenominator3 = -6.88283971605453293030e-01;
double ClArcSineDenominator4 =  7.70381505559019352791e-02;

//
// Define constants used in the arc tangent computation. The high and low
// arrays store precomputed values for the arc tangent of 0.5, 1.0, 1.5, and
// infinity.
//

const double ClArcTangentHigh[] = {
    4.63647609000806093515e-01,
    7.85398163397448278999e-01,
    9.82793723247329054082e-01,
    1.57079632679489655800e+00,
};

const double ClArcTangentLow[] = {
    2.26987774529616870924e-17,
    3.06161699786838301793e-17,
    1.39033110312309984516e-17,
    6.12323399573676603587e-17,
};

const double ClArcTangent[] = {
    3.33333333333329318027e-01,
    -1.99999999998764832476e-01,
    1.42857142725034663711e-01,
    -1.11111104054623557880e-01,
    9.09088713343650656196e-02,
    -7.69187620504482999495e-02,
    6.66107313738753120669e-02,
    -5.83357013379057348645e-02,
    4.97687799461593236017e-02,
    -3.65315727442169155270e-02,
    1.62858201153657823623e-02,
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
asin (
    double Value
    )

/*++

Routine Description:

    This routine computes the arc sine of the given value.

Arguments:

    Value - Supplies the sine value to convert back to an angle.

Return Value:

    Returns the arc sine of the value, in radians.

--*/

{

    LONG AbsoluteHighWord;
    double Approximation;
    double ArcSine;
    double Correction;
    double Denominator;
    double HalfFlipped;
    LONG HighWord;
    ULONG LowWord;
    double Numerator;
    double Root;
    double RootHigh;
    double Value2;
    DOUBLE_PARTS ValueParts;

    //
    // Here's the method:
    // Since asin(x) = x + x^3/6 + x^5*3/40 + x^7*15/336 + ...
    // approximate asin(x) on [0, 0.5] by
    //     asin(x) = x + x * x^2 * R(x^2)
    // where
    //     R(x^2) is a rational approximation of (asin(x) - x) / x^3 and its
    // remez error is bounded by
    //     |(asin(x) - x) / x^3 - R(x^2)| < 2^(-58.75)
    //
    // For x in [0.5, 1]
    //     asin(x) = pi/2 - 2 * asin(sqrt((1 - x) / 2))
    // Let y = 1 - x, z = y/2, and s = sqrt(z). Then for x > 0.98:
    //     asin(x) = pi/2 - 2 * (s + s * z * R(z))
    //
    // For x <= 0.98, let f = high part of s, and
    //     c = sqrt(z) - f = (z - f*f) / (s+f) ... f + c = sqrt(z)
    // and so
    //     asin(x) = pi/2 - 2 * (s + s * z * R(z))
    //             = pi/4high + (pi/4 - 2s) - (2s * z * R(z) - pi/2low)
    //             = pi/4high + (pi/4 - 2f) - (2s * z * R(z) - (pi/2low + 2c))
    //

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;
    AbsoluteHighWord = HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // Handle the absolute value being greater than or equal to one.
    //

    if (AbsoluteHighWord >= DOUBLE_ONE_HIGH_WORD) {
        LowWord = ValueParts.Ulong.Low;

        //
        // asin(1) = +-pi/2 with inexact.
        //

        if (((AbsoluteHighWord - DOUBLE_ONE_HIGH_WORD) | LowWord) == 0) {
            return Value * ClPiOver2High + Value * ClPiOver2Low;
        }

        //
        // asin of a value greater than 1 is NaN.
        //

        return (Value - Value) / (Value - Value);

    //
    // Work it out if the absolute value is less than 0.5.
    //

    } else if (AbsoluteHighWord < DOUBLE_ONE_HALF_HIGH_WORD) {
        if (AbsoluteHighWord < ARC_SINE_LOW_THRESHOLD_HIGH_WORD) {
            if (ClDoubleHugeValue + Value > ClDoubleOne) {

                //
                // Return the value with inexact if the value is not zero.
                //

                return Value;
            }
        }

        Value2 = Value * Value;
        Numerator = Value2 *
                    (ClArcSineNumerator0 + Value2 *
                     (ClArcSineNumerator1 + Value2 *
                      (ClArcSineNumerator2 + Value2 *
                       (ClArcSineNumerator3 + Value2 *
                        (ClArcSineNumerator4 + Value2 *
                         ClArcSineNumerator5)))));

        Denominator = ClDoubleOne + Value2 *
                      (ClArcSineDenominator1 + Value2 *
                       (ClArcSineDenominator2 + Value2 *
                        (ClArcSineDenominator3 + Value2 *
                         ClArcSineDenominator4)));

        Approximation = Numerator / Denominator;
        return Value + Value * Approximation;
    }

    //
    // The absolute value must be less than one and greater than or equal to
    // 0.5.
    //

    HalfFlipped = ClDoubleOne - fabs(Value);
    HalfFlipped = HalfFlipped * ClDoubleOneHalf;
    Numerator = HalfFlipped *
                (ClArcSineNumerator0 + HalfFlipped *
                 (ClArcSineNumerator1 + HalfFlipped *
                  (ClArcSineNumerator2 + HalfFlipped *
                   (ClArcSineNumerator3 + HalfFlipped *
                    (ClArcSineNumerator4 + HalfFlipped *
                     ClArcSineNumerator5)))));

    Denominator = ClDoubleOne + HalfFlipped *
                  (ClArcSineDenominator1 + HalfFlipped *
                   (ClArcSineDenominator2 + HalfFlipped *
                    (ClArcSineDenominator3 + HalfFlipped *
                     ClArcSineDenominator4)));

    Root = sqrt(HalfFlipped);

    //
    // Use the easier evaluation if the absolute value is greater than about
    // 0.975.
    //

    if (AbsoluteHighWord >= ARC_SINE_UPPER_APPROXIMATION_HIGH_WORD) {
        Approximation = Numerator / Denominator;
        ArcSine = ClPiOver2High - (2.0 * (Root + Root * Approximation) -
                                   ClPiOver2Low);

    } else {
        ValueParts.Double = Root;
        ValueParts.Ulong.Low = 0;
        RootHigh = ValueParts.Double;
        Correction = (HalfFlipped - RootHigh * RootHigh) / (Root + RootHigh);
        Approximation = Numerator / Denominator;
        Numerator = 2.0 * Root * Approximation -
                    (ClPiOver2Low - 2.0 * Correction);

        Denominator = ClPiOver4 - 2.0 * RootHigh;
        ArcSine = ClPiOver4 - (Numerator - Denominator);
    }

    if (HighWord > 0) {
        return ArcSine;
    }

    return -ArcSine;
}

LIBC_API
double
acos (
    double Value
    )

/*++

Routine Description:

    This routine computes the arc cosine of the given value.

Arguments:

    Value - Supplies the cosine value to convert back to an angle.

Return Value:

    Returns the arc cosine of the value, in radians.

--*/

{

    LONG AbsoluteHighWord;
    double Approximation;
    double Correction;
    double Denominator;
    LONG HighWord;
    double Input;
    ULONG LowWord;
    double Numerator;
    double Quotient;
    double Root;
    double RootHigh;
    DOUBLE_PARTS ValueParts;

    //
    // Here's the method:
    // acos(x) = pi/2 - asin(x)
    // acos(-x) = pi/2 + asin(x)
    // For |x| <= 0.5
    //     acos(x) = pi/2 - (x + x * x^2 * R(x^2)) (see the comment for asin)
    //
    // For |x| > 0.5
    //     acos(x) = pi/2 - (pi/2 - 2asin(sqrt((1 - x)/ 2)))
    //             = 2asin(sqrt((1 - x) / 2))
    //             = 2s + 2s * z * R(z) ... z = (1 - x) / 2, s = sqrt(z)
    //             = 2f + (2c + 2s * z * R(z))
    //
    // where f is the high part of s, and c = (z - f * f) / (s + f) is the
    // correction term for f so that f + c ~ sqrt(z).
    //
    // For x < -0.5:
    //     acos(x) = pi - 2asin(sqrt((1 - |x|) / 2))
    //             = pi - 0.5 * (s + s * z * R(z)),
    // where z = (1 - |x|) / 2,s = sqrt(z)
    // If the value is NaN, return x itself. If the absolute value is greater
    // than 1, return NaN with an invalid signal.
    //

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;
    AbsoluteHighWord = HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // Check if the value is greater than or equal to one.
    //

    if (AbsoluteHighWord >= DOUBLE_ONE_HIGH_WORD) {
        LowWord = ValueParts.Ulong.Low;

        //
        // Check if the absolute value equals one exactly.
        //

        if (((AbsoluteHighWord - DOUBLE_ONE_HIGH_WORD) | LowWord) == 0) {

            //
            // The arc cosine of 1 is zero.
            //

            if (HighWord > 0) {
                return 0.0;
            }

            //
            // The arc cosine of -1 is pi.
            //

            return ClPi + 2.0 * ClPiOver2Low;
        }

        //
        // The arc cosine of something with an absolute value greater than one
        // is NaN.
        //

        return (Value - Value) / (Value - Value);
    }

    //
    // Handle an absolute value less than 0.5.
    //

    if (AbsoluteHighWord < DOUBLE_ONE_HALF_HIGH_WORD) {

        //
        // Really small values are just pi/2.
        //

        if (AbsoluteHighWord <= ARC_COSINE_LOW_THRESHOLD_HIGH_WORD) {
            return ClPiOver2High + ClPiOver2Low;
        }

        Input = Value * Value;
        Numerator = Input *
                    (ClArcSineNumerator0 + Input *
                     (ClArcSineNumerator1 + Input *
                      (ClArcSineNumerator2 + Input *
                       (ClArcSineNumerator3 + Input *
                        (ClArcSineNumerator4 + Input *
                         ClArcSineNumerator5)))));

        Denominator = ClDoubleOne + Input *
                      (ClArcSineDenominator1 + Input *
                       (ClArcSineDenominator2 + Input *
                        (ClArcSineDenominator3 + Input *
                         ClArcSineDenominator4)));

        Quotient = Numerator / Denominator;
        return ClPiOver2High - (Value - (ClPiOver2Low - Value * Quotient));

    //
    // Handle the value being less than -0.5.
    //

    } else if (HighWord < 0) {
        Input = (ClDoubleOne + Value) * 0.5;
        Numerator = Input *
                    (ClArcSineNumerator0 + Input *
                     (ClArcSineNumerator1 + Input *
                      (ClArcSineNumerator2 + Input *
                       (ClArcSineNumerator3 + Input *
                        (ClArcSineNumerator4 + Input *
                         ClArcSineNumerator5)))));

        Denominator = ClDoubleOne + Input *
                      (ClArcSineDenominator1 + Input *
                       (ClArcSineDenominator2 + Input *
                        (ClArcSineDenominator3 + Input *
                         ClArcSineDenominator4)));

        Root = sqrt(Input);
        Quotient = Numerator / Denominator;
        Approximation = Quotient * Root - ClPiOver2Low;
        return ClPi - 2.0 * (Root + Approximation);
    }

    //
    // The value is greater than 0.5.
    //

    Input = (ClDoubleOne - Value) * 0.5;
    Root = sqrt(Input);
    ValueParts.Double = Root;
    ValueParts.Ulong.Low = 0;
    RootHigh = ValueParts.Double;
    Correction = (Input - RootHigh * RootHigh) / (Root + RootHigh);
    Numerator = Input *
                (ClArcSineNumerator0 + Input *
                 (ClArcSineNumerator1 + Input *
                  (ClArcSineNumerator2 + Input *
                   (ClArcSineNumerator3 + Input *
                    (ClArcSineNumerator4 + Input *
                     ClArcSineNumerator5)))));

    Denominator = ClDoubleOne + Input *
                  (ClArcSineDenominator1 + Input *
                   (ClArcSineDenominator2 + Input *
                    (ClArcSineDenominator3 + Input * ClArcSineDenominator4)));

    Quotient = Numerator / Denominator;
    Approximation = Quotient * Root + Correction;
    return 2.0 * (RootHigh + Approximation);
}

LIBC_API
double
atan (
    double Value
    )

/*++

Routine Description:

    This routine computes the arc tangent of the given value.

Arguments:

    Value - Supplies the tangent value to convert back to an angle.

Return Value:

    Returns the arc tangent of the value, in radians.

--*/

{

    LONG AbsoluteHighWord;
    double ArcTangent;
    double Evens;
    LONG HighWord;
    ULONG LowWord;
    double Odds;
    double Value2;
    double Value4;
    DOUBLE_PARTS ValueParts;
    LONG Zone;

    //
    // The method has two steps:
    // 1. Reduce the value to positive by atan(x) = -atan(-x).
    // 2. According to the integer k = 4t + 0.25 chopped, t = x, the argument
    //    is further reduced to one of the following intervals and the arc
    //    tangent of t is evaluated by the corresponding formula:
    //
    //    [0,7/16]      atan(x) = t - t^3 *
    //                            (a1+ t^2 * (a2 + ... (a10 + t^2 * a11) ...)
    //
    //    [7/16,11/16]  atan(x) = atan(1/2) + atan((t - 0.5) / (1 + t / 2))
    //    [11/16.19/16] atan(x) = atan(1) + atan((t - 1) / (1 + t))
    //    [19/16,39/16] atan(x) = atan(3/2) + atan((t - 1.5) / (1 + 1.5t))
    //    [39/16,INF]   atan(x) = atan(INF) + atan(-1 / t)
    //

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;
    AbsoluteHighWord = HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);
    if (AbsoluteHighWord >= ARC_TANGENT_HIGH_THRESHOLD_HIGH_WORD) {
        LowWord = ValueParts.Ulong.Low;

        //
        // Handle infinity and NaN.
        //

        if ((AbsoluteHighWord > NAN_HIGH_WORD) ||
            ((AbsoluteHighWord == NAN_HIGH_WORD) && (LowWord != 0))) {

            return Value + Value;
        }

        if (HighWord > 0) {
            ArcTangent = ClArcTangentHigh[3] +
                         *(volatile double *)&ClArcTangentLow[3];

            return ArcTangent;
        }

        return -ClArcTangentHigh[3] - *(volatile double *)&ClArcTangentLow[3];
    }

    //
    // Handle the value in its smallest range.
    //

    if (AbsoluteHighWord < ARC_TANGENT_LOW_THRESHOLD_HIGH_WORD) {

        //
        // Handle the value being basically zero.
        //

        if (AbsoluteHighWord < ARC_TANGENT_ZERO_THRESHOLD_HIGH_WORD) {
            if (ClDoubleHugeValue + Value > ClDoubleOne) {

                //
                // Raise an inexact condition.
                //

                return Value;
            }
        }

        Zone = -1;

    } else {
        Value = fabs(Value);
        if (AbsoluteHighWord < ARC_TANGENT_MIDDLE_THRESHOLD_HIGH_WORD) {
            if (AbsoluteHighWord < ARC_TANGENT_MIDDLE_LOW_THRESHOLD_HIGH_WORD) {
                Zone = 0;
                Value = (2.0 * Value - ClDoubleOne) / (2.0 + Value);

            } else {
                Zone = 1;
                Value = (Value - ClDoubleOne) / (Value + ClDoubleOne);
            }

        } else {
            if (AbsoluteHighWord <
                ARC_TANGENT_MIDDLE_HIGH_THRESHOLD_HIGH_WORD) {

                Zone = 2;
                Value = (Value - 1.5) / (ClDoubleOne + 1.5 * Value);

            } else {
                Zone = 3;
                Value  = -1.0 / Value;
            }
        }
    }

    Value2 = Value * Value;
    Value4 = Value2 * Value2;

    //
    // Calculate the big polynomial in two chunks, the even coefficients and
    // the odd ones.
    //

    Evens = Value2 *
            (ClArcTangent[0] + Value4 *
             (ClArcTangent[2] + Value4 *
              (ClArcTangent[4] + Value4 *
               (ClArcTangent[6] + Value4 *
                (ClArcTangent[8] + Value4 *
                 ClArcTangent[10])))));

    Odds = Value4 *
           (ClArcTangent[1] + Value4 *
            (ClArcTangent[3] + Value4 *
             (ClArcTangent[5] + Value4 *
              (ClArcTangent[7] + Value4 * ClArcTangent[9]))));

    if (Zone < 0) {
        return Value - Value * (Evens + Odds);
    }

    ArcTangent = ClArcTangentHigh[Zone] -
                 ((Value * (Evens + Odds) - ClArcTangentLow[Zone]) - Value);

    if (HighWord < 0) {
        return -ArcTangent;
    }

    return ArcTangent;
}

LIBC_API
double
atan2 (
    double Numerator,
    double Denominator
    )

/*++

Routine Description:

    This routine computes the arc tangent of the given values, using the signs
    of both the numerator and the denominator to determine the correct
    quadrant for the output angle.

Arguments:

    Numerator - Supplies the numerator to the tangent value.

    Denominator - Supplies the denominator to the tangent value.

Return Value:

    Returns the arc tangent of the value, in radians.

    Pi if the numerator is +/- 0 and the denominator is negative.

    +/- 0 if the numerator is +/- 0 and the denominator is positive.

    Negative pi over 2 if the numerator is negative and the denominator is
    +/- 0.

    Pi over 2 if the numerator is positive and the denominator is +/- 0.

    NaN if either input is NaN.

    Returns the numerator over the denominator if the result underflows.

    +/- Pi if the numerator is +/- 0 and the denominator is -0.

    +/- 0 if the numerator is +/- 0 and the denominator is +0.

    +/- Pi for positive finite values of the numerator and -Infinity in the
    denominator.

    +/- 0 for positive finite values of the numerator and +Infinity in the
    denominator.

    +/- Pi/2 for finite values of the denominator if the numerator is
    +/- Infinity.

    +/- 3Pi/4 if the numerator is +/- Infinity and the denominator is -Infinity.

    +/- Pi/4 if the numerator is +/- Infinity and the denominator is +Infinity.

--*/

{

    LONG AbsoluteDenominatorHigh;
    LONG AbsoluteNumeratorHigh;
    double ArcTangent;
    LONG DenominatorHigh;
    ULONG DenominatorLow;
    DOUBLE_PARTS DenominatorParts;
    LONG NumeratorHigh;
    ULONG NumeratorLow;
    DOUBLE_PARTS NumeratorParts;
    LONG QuotientHigh;
    LONG Signs;
    ULONG SignShift;

    SignShift = DOUBLE_SIGN_BIT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    DenominatorParts.Double = Denominator;
    DenominatorHigh = DenominatorParts.Ulong.High;
    AbsoluteDenominatorHigh = DenominatorHigh &
                              (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    DenominatorLow = DenominatorParts.Ulong.Low;
    NumeratorParts.Double = Numerator;
    NumeratorHigh = NumeratorParts.Ulong.High;
    AbsoluteNumeratorHigh = NumeratorHigh &
                            (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    NumeratorLow = NumeratorParts.Ulong.Low;

    //
    // Check to see if the denominator or numerator is NaN.
    //

    if (((AbsoluteDenominatorHigh |
          ((DenominatorLow | -DenominatorLow) >> SignShift)) > NAN_HIGH_WORD) ||
        ((AbsoluteNumeratorHigh |
          ((NumeratorLow | -NumeratorLow) >> SignShift)) > NAN_HIGH_WORD)) {

        return Denominator + Numerator;
    }

    //
    // If the denominator is one, just return the arc tangent of the numerator.
    //

    if (((DenominatorHigh - DOUBLE_ONE_HIGH_WORD) | DenominatorLow) == 0) {
        return atan(Numerator);
    }

    //
    // Switch based on the signs.
    //

    Signs = ((NumeratorHigh >> SignShift) & 0x1) |
            ((DenominatorHigh >> (SignShift - 1)) & 0x2);

    //
    // Handle the case when the numerator is zero.
    //

    if ((AbsoluteNumeratorHigh | NumeratorLow) == 0) {
        switch (Signs) {

        //
        // The arc tangent of +-0 / +anything is +-0.
        //

        case 0:
        case 1:
            return Numerator;

        //
        // The arc tangent of +0 / -anything is pi.
        //

        case 2:
            return ClPi + ClDoubleTinyValue;

        //
        // The arc tangent of -0 / -anything is -pi.
        //

        case 3:
            return -ClPi - ClDoubleTinyValue;
        }
    }

    //
    // Handle the denominator being zero.
    //

    if ((AbsoluteDenominatorHigh | DenominatorLow) == 0) {
        if (NumeratorHigh < 0) {
            return -ClPiOver2High - ClDoubleTinyValue;

        } else {
            return ClPiOver2High + ClDoubleTinyValue;
        }
    }

    //
    // Handle the denominator being Infinity.
    //

    if (AbsoluteDenominatorHigh == NAN_HIGH_WORD) {
        if (AbsoluteNumeratorHigh == NAN_HIGH_WORD) {
            switch (Signs) {

            //
            // The arc tangent of +Infinity / +Infinity is pi/4.
            //

            case 0:
                return ClPiOver4High + ClDoubleTinyValue;

            //
            // The arc tangent of -Infinity / +Infinity is -pi/4.
            //

            case 1:
                return -ClPiOver4High - ClDoubleTinyValue;

            //
            // The arc tangent of +Infinity / -Infinity is 3pi/4.
            //

            case 2:
                return 3.0 * ClPiOver4High + ClDoubleTinyValue;

            //
            // The arc tangent of -Infinity / -Infinity is -3pi/4.
            //

            case 3:
                return -3.0 * ClPiOver4High - ClDoubleTinyValue;
            }

        } else {
            switch (Signs) {

            //
            // The arc tangent of +anything / +Infinity is 0.
            //

            case 0:
                return ClDoubleZero;

            //
            // The arc tangent of -anything / +Infinity is -0.
            //

            case 1:
                return -ClDoubleZero;

            //
            // The arc tangent of +anything / -Infinity is pi.
            //

            case 2:
                return ClPi + ClDoubleTinyValue;

            //
            // The arc tangent of -anything / -Infinity is -pi.
            //

            case 3:
                return -ClPi - ClDoubleTinyValue;
            }
        }
    }

    //
    // Handle the numerator being Infinity.
    //

    if (AbsoluteNumeratorHigh == NAN_HIGH_WORD) {
        if (NumeratorHigh < 0) {
            return -ClPiOver2High - ClDoubleTinyValue;

        } else {
            return ClPiOver2High + ClDoubleTinyValue;
        }
    }

    //
    // Compute the quotient.
    //

    QuotientHigh = (AbsoluteNumeratorHigh - AbsoluteDenominatorHigh) >>
                   (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    //
    // Handle |Numerator / Denominator| > 2^60 (unsafe division).
    //

    if (QuotientHigh > 60) {
        ArcTangent = ClPiOver2High + 0.5 * ClPiLow;
        Signs &= 0x1;

    } else if ((DenominatorHigh < 0) && (QuotientHigh < -60)) {
        ArcTangent = 0.0;

    } else {
        ArcTangent = atan(fabs(Numerator / Denominator));
    }

    switch (Signs) {

    //
    // Return the arc tangent of two positive numbers.
    //

    case 0:
        return ArcTangent;

    //
    // Negate the value for a negative numerator.
    //

    case 1:
        return -ArcTangent;

    //
    // Return pi minus the result for a negative denominator.
    //

    case 2:
        return ClPi - (ArcTangent - ClPiLow);

    //
    // Return the result minus pi for a negative numerator and denominator.
    //

    default:
        break;
    }

    return (ArcTangent - ClPiLow) - ClPi;
}

//
// --------------------------------------------------------- Internal Functions
//

