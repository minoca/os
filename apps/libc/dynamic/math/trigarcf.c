/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    trigarcf.c

Abstract:

    This module implements support for the inverse trigonometric functions:
    arc sine arc cosine, and arc tangent.

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

#define ARC_SINE_FLOAT_LOW_THRESHOLD_WORD 0x32000000
#define ARC_SINE_FLOAT_UPPER_APPROXIMATION_WORD 0x3F79999A
#define ARC_COSINE_FLOAT_LOW_THRESHOLD_WORD 0x32800000
#define ARC_TANGENT_FLOAT_HIGH_THRESHOLD_WORD 0x4C800000
#define ARC_TANGENT_FLOAT_LOW_THRESHOLD_WORD 0x3EE00000
#define ARC_TANGENT_FLOAT_ZERO_THRESHOLD_WORD 0x39800000
#define ARC_TANGENT_FLOAT_MIDDLE_THRESHOLD_WORD 0x3F980000
#define ARC_TANGENT_FLOAT_MIDDLE_LOW_THRESHOLD_WORD 0x3F300000
#define ARC_TANGENT_FLOAT_MIDDLE_HIGH_THRESHOLD_WORD 0x401C0000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const float ClFloatPiLow = -8.7422776573e-08;
const float ClFloatPiOver2High = 1.5707962513e+00;
const float ClFloatPiOver2Low = 7.5497894159e-08;
const float ClFloatPiOver4High = 7.8539816339e-01;

//
// Define the coefficients used the in the R(x^2) calculation used for arc sine.
//

const float ClArcSineFloatNumerator0 = 1.666675248e-01;
const float ClArcSineFloatNumerator1 = 7.495297643e-02;
const float ClArcSineFloatNumerator2 = 4.547037598e-02;
const float ClArcSineFloatNumerator3 = 2.417951451e-02;
const float ClArcSineFloatNumerator4 = 4.216630880e-02;

//
// Define the coefficients used the in the R(x^2) calculation used for arc
// cosine.
//

const float ClArcCosineFloatNumerator0 = 1.6666586697e-01;
const float ClArcCosineFloatNumerator1 = -4.2743422091e-02;
const float ClArcCosineFloatNumerator2 = -8.6563630030e-03;
const float ClArcCosineFloatDenominator1 = -7.0662963390e-01;

//
// Define constants used in the arc tangent computation. The high and low
// arrays store precomputed values for the arc tangent of 0.5, 1.0, 1.5, and
// infinity.
//

const float ClArcTangentFloatHigh[] = {
    4.6364760399e-01,
    7.8539812565e-01,
    9.8279368877e-01,
    1.5707962513e+00,
};

const float ClArcTangentFloatLow[] = {
    5.0121582440e-09,
    3.7748947079e-08,
    3.4473217170e-08,
    7.5497894159e-08,
};

const float ClArcTangentFloat[] = {
    3.3333328366e-01,
    -1.9999158382e-01,
    1.4253635705e-01,
    -1.0648017377e-01,
    6.1687607318e-02,
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
asinf (
    float Value
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

    LONG AbsoluteWord;
    float ArcSine;
    float Correction;
    float Denominator;
    float HalfFlipped;
    float Numerator;
    float Root;
    float RootHigh;
    float Value2;
    FLOAT_PARTS ValueParts;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;

    //
    // Handle the absolute value being greater than or equal to one.
    //

    if (AbsoluteWord >= FLOAT_ONE_WORD) {

        //
        // asin(1) = +-pi/2 with inexact.
        //

        if (AbsoluteWord == FLOAT_ONE_WORD) {
            return Value * ClFloatPiOver2High + Value * ClFloatPiOver2Low;
        }

        //
        // asin of a value greater than 1 is NaN.
        //

        return (Value - Value) / (Value - Value);

    //
    // Work it out if the absolute value is less than 0.5.
    //

    } else if (AbsoluteWord < FLOAT_ONE_HALF_WORD) {
        if (AbsoluteWord < ARC_SINE_FLOAT_LOW_THRESHOLD_WORD) {
            if (ClFloatHugeValue + Value > ClFloatOne) {

                //
                // Return the value with inexact if the value is not zero.
                //

                return Value;
            }
        }

        Value2 = Value * Value;
        Numerator = Value2 *
                    (ClArcSineFloatNumerator0 + Value2 *
                     (ClArcSineFloatNumerator1 + Value2 *
                      (ClArcSineFloatNumerator2 + Value2 *
                       (ClArcSineFloatNumerator3 + Value2 *
                        ClArcSineFloatNumerator4))));

        return Value + Value * Numerator;
    }

    //
    // The absolute value must be less than one and greater than or equal to
    // 0.5.
    //

    HalfFlipped = ClFloatOne - fabsf(Value);
    HalfFlipped = HalfFlipped * ClFloatOneHalf;
    Numerator = HalfFlipped *
                (ClArcSineFloatNumerator0 + HalfFlipped *
                 (ClArcSineFloatNumerator1 + HalfFlipped *
                  (ClArcSineFloatNumerator2 + HalfFlipped *
                   (ClArcSineFloatNumerator3 + HalfFlipped *
                    ClArcSineFloatNumerator4))));

    Root = sqrtf(HalfFlipped);

    //
    // Use the easier evaluation if the absolute value is greater than about
    // 0.975.
    //

    if (AbsoluteWord >= ARC_SINE_FLOAT_UPPER_APPROXIMATION_WORD) {
        ArcSine = ClFloatPiOver2High -
                  ((float)2.0 * (Root + Root * Numerator) - ClFloatPiOver2Low);

    } else {
        ValueParts.Float = Root;
        ValueParts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
        RootHigh = ValueParts.Float;
        Correction = (HalfFlipped - RootHigh * RootHigh) / (Root + RootHigh);
        Numerator = (float)2.0 * Root * Numerator -
                    (ClFloatPiOver2Low - (float)2.0 * Correction);

        Denominator = ClFloatPiOver4High - (float)2.0 * RootHigh;
        ArcSine = ClFloatPiOver4High - (Numerator - Denominator);
    }

    if (Word > 0) {
        return ArcSine;
    }

    return -ArcSine;
}

LIBC_API
float
acosf (
    float Value
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

    LONG AbsoluteWord;
    float Approximation;
    float ArcCosine;
    float Correction;
    float Denominator;
    float Input;
    float Numerator;
    float Quotient;
    float Root;
    float RootHigh;
    FLOAT_PARTS ValueParts;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;

    //
    // Check if the value is greater than or equal to one.
    //

    if (AbsoluteWord >= FLOAT_ONE_WORD) {

        //
        // Check if the absolute value equals one exactly.
        //

        if (AbsoluteWord == FLOAT_ONE_WORD) {

            //
            // The arc cosine of 1 is zero.
            //

            if (Word > 0) {
                return 0.0;
            }

            //
            // The arc cosine of -1 is pi.
            //

            return ClFloatPi + (float)2.0 * ClFloatPiOver2Low;
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

    if (AbsoluteWord < FLOAT_ONE_HALF_WORD) {

        //
        // Really small values are just pi/2.
        //

        if (AbsoluteWord <= ARC_COSINE_FLOAT_LOW_THRESHOLD_WORD) {
            return ClFloatPiOver2High + ClFloatPiOver2Low;
        }

        Input = Value * Value;
        Numerator = Input *
                    (ClArcCosineFloatNumerator0 + Input *
                     (ClArcCosineFloatNumerator1 + Input *
                      ClArcCosineFloatNumerator2));

        Denominator = ClFloatOne + Input * ClArcCosineFloatDenominator1;
        Quotient = Numerator / Denominator;
        ArcCosine = ClFloatPiOver2High -
                    (Value - (ClFloatPiOver2Low - Value * Quotient));

        return ArcCosine;

    //
    // Handle the value being less than -0.5.
    //

    } else if (Word < 0) {
        Input = (ClFloatOne + Value) * (float)0.5;
        Numerator = Input *
                    (ClArcCosineFloatNumerator0 + Input *
                     (ClArcCosineFloatNumerator1 + Input *
                      ClArcCosineFloatNumerator2));

        Denominator = ClFloatOne + Input * ClArcCosineFloatDenominator1;
        Root = sqrtf(Input);
        Quotient = Numerator / Denominator;
        Approximation = Quotient * Root - ClFloatPiOver2Low;
        return ClFloatPi - (float)2.0 * (Root + Approximation);
    }

    //
    // The value is greater than 0.5.
    //

    Input = (ClFloatOne - Value) * (float)0.5;
    Root = sqrtf(Input);
    ValueParts.Float = Root;
    ValueParts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
    RootHigh = ValueParts.Float;
    Correction = (Input - RootHigh * RootHigh) / (Root + RootHigh);
    Numerator = Input *
                (ClArcCosineFloatNumerator0 + Input *
                 (ClArcCosineFloatNumerator1 + Input *
                  ClArcCosineFloatNumerator2));

    Denominator = ClFloatOne + Input * ClArcCosineFloatDenominator1;
    Quotient = Numerator / Denominator;
    Approximation = Quotient * Root + Correction;
    return (float)2.0 * (RootHigh + Approximation);
}

LIBC_API
float
atanf (
    float Value
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

    LONG AbsoluteWord;
    float ArcTangent;
    float Evens;
    float Odds;
    float Value2;
    float Value4;
    FLOAT_PARTS ValueParts;
    LONG Word;
    LONG Zone;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;
    if (AbsoluteWord >= ARC_TANGENT_FLOAT_HIGH_THRESHOLD_WORD) {

        //
        // Handle infinity and NaN.
        //

        if (AbsoluteWord > FLOAT_NAN) {
            return Value + Value;
        }

        if (Word > 0) {
            ArcTangent = ClArcTangentFloatHigh[3] +
                         *(volatile float *)&ClArcTangentFloatLow[3];

            return ArcTangent;
        }

        ArcTangent = -ClArcTangentFloatHigh[3] -
                     *(volatile float *)&ClArcTangentFloatLow[3];

        return ArcTangent;
    }

    //
    // Handle the value in its smallest range.
    //

    if (AbsoluteWord < ARC_TANGENT_FLOAT_LOW_THRESHOLD_WORD) {

        //
        // Handle the value being basically zero.
        //

        if (AbsoluteWord < ARC_TANGENT_FLOAT_ZERO_THRESHOLD_WORD) {
            if (ClFloatHugeValue + Value > ClFloatOne) {

                //
                // Raise an inexact condition.
                //

                return Value;
            }
        }

        Zone = -1;

    } else {
        Value = fabsf(Value);
        if (AbsoluteWord < ARC_TANGENT_FLOAT_MIDDLE_THRESHOLD_WORD) {
            if (AbsoluteWord < ARC_TANGENT_FLOAT_MIDDLE_LOW_THRESHOLD_WORD) {
                Zone = 0;
                Value = ((float)2.0 * Value - ClFloatOne) /
                        ((float)2.0 + Value);

            } else {
                Zone = 1;
                Value = (Value - ClFloatOne) / (Value + ClFloatOne);
            }

        } else {
            if (AbsoluteWord < ARC_TANGENT_FLOAT_MIDDLE_HIGH_THRESHOLD_WORD) {
                Zone = 2;
                Value = (Value - (float)1.5) /
                        (ClFloatOne + (float)1.5 * Value);

            } else {
                Zone = 3;
                Value  = -(float)1.0 / Value;
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
            (ClArcTangentFloat[0] + Value4 *
             (ClArcTangentFloat[2] + Value4 * ClArcTangentFloat[4]));

    Odds = Value4 * (ClArcTangentFloat[1] + Value4 * ClArcTangentFloat[3]);
    if (Zone < 0) {
        return Value - Value * (Evens + Odds);
    }

    ArcTangent = ClArcTangentFloatHigh[Zone] -
                 ((Value * (Evens + Odds) - ClArcTangentFloatLow[Zone]) -
                  Value);

    if (Word < 0) {
        return -ArcTangent;
    }

    return ArcTangent;
}

LIBC_API
float
atan2f (
    float Numerator,
    float Denominator
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

    LONG AbsoluteDenominator;
    LONG AbsoluteNumerator;
    float ArcTangent;
    LONG DenominatorWord;
    FLOAT_PARTS DenominatorParts;
    LONG NumeratorWord;
    FLOAT_PARTS NumeratorParts;
    LONG QuotientHigh;
    LONG Signs;

    DenominatorParts.Float = Denominator;
    DenominatorWord = DenominatorParts.Ulong;
    AbsoluteDenominator = DenominatorWord & ~FLOAT_SIGN_BIT;
    NumeratorParts.Float = Numerator;
    NumeratorWord = NumeratorParts.Ulong;
    AbsoluteNumerator = NumeratorWord & ~FLOAT_SIGN_BIT;

    //
    // Check to see if the denominator or numerator is NaN.
    //

    if ((AbsoluteDenominator > FLOAT_NAN) || (AbsoluteNumerator > FLOAT_NAN)) {
        return Denominator + Numerator;
    }

    //
    // If the denominator is one, just return the arc tangent of the numerator.
    //

    if (DenominatorWord == FLOAT_ONE_WORD) {
        return atanf(Numerator);
    }

    //
    // Switch based on the signs.
    //

    Signs = ((NumeratorWord >> FLOAT_SIGN_BIT_SHIFT) & 0x1) |
            ((DenominatorWord >> (FLOAT_SIGN_BIT_SHIFT - 1)) & 0x2);

    //
    // Handle the case when the numerator is zero.
    //

    if (AbsoluteNumerator == 0) {
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
            return ClFloatPi + ClFloatTinyValue;

        //
        // The arc tangent of -0 / -anything is -pi.
        //

        case 3:
            return -ClFloatPi - ClFloatTinyValue;
        }
    }

    //
    // Handle the denominator being zero.
    //

    if (AbsoluteDenominator == 0) {
        if (NumeratorWord < 0) {
            return -ClFloatPiOver2High - ClFloatTinyValue;

        } else {
            return ClFloatPiOver2High + ClFloatTinyValue;
        }
    }

    //
    // Handle the denominator being Infinity.
    //

    if (AbsoluteDenominator == FLOAT_NAN) {
        if (AbsoluteNumerator == FLOAT_NAN) {
            switch (Signs) {

            //
            // The arc tangent of +Infinity / +Infinity is pi/4.
            //

            case 0:
                return ClFloatPiOver4High + ClFloatTinyValue;

            //
            // The arc tangent of -Infinity / +Infinity is -pi/4.
            //

            case 1:
                return -ClFloatPiOver4High - ClFloatTinyValue;

            //
            // The arc tangent of +Infinity / -Infinity is 3pi/4.
            //

            case 2:
                return (float)3.0 * ClFloatPiOver4High + ClFloatTinyValue;

            //
            // The arc tangent of -Infinity / -Infinity is -3pi/4.
            //

            case 3:
                return (float)-3.0 * ClFloatPiOver4High - ClFloatTinyValue;
            }

        } else {
            switch (Signs) {

            //
            // The arc tangent of +anything / +Infinity is 0.
            //

            case 0:
                return ClFloatZero;

            //
            // The arc tangent of -anything / +Infinity is -0.
            //

            case 1:
                return -ClFloatZero;

            //
            // The arc tangent of +anything / -Infinity is pi.
            //

            case 2:
                return ClFloatPi + ClFloatTinyValue;

            //
            // The arc tangent of -anything / -Infinity is -pi.
            //

            case 3:
                return -ClFloatPi - ClFloatTinyValue;
            }
        }
    }

    //
    // Handle the numerator being Infinity.
    //

    if (AbsoluteNumerator == FLOAT_NAN) {
        if (NumeratorWord < 0) {
            return -ClFloatPiOver2High - ClFloatTinyValue;

        } else {
            return ClFloatPiOver2High + ClFloatTinyValue;
        }
    }

    //
    // Compute the quotient.
    //

    QuotientHigh = (AbsoluteNumerator - AbsoluteDenominator) >>
                   FLOAT_EXPONENT_SHIFT;

    //
    // Handle |Numerator / Denominator| > 2^26 (unsafe division).
    //

    if (QuotientHigh > 26) {
        ArcTangent = ClFloatPiOver2High + (float)0.5 * ClFloatPiLow;
        Signs &= 0x1;

    } else if ((DenominatorWord < 0) && (QuotientHigh < -26)) {
        ArcTangent = 0.0;

    } else {
        ArcTangent = atanf(fabsf(Numerator / Denominator));
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
        return ClFloatPi - (ArcTangent - ClFloatPiLow);

    //
    // Return the result minus pi for a negative numerator and denominator.
    //

    default:
        break;
    }

    return (ArcTangent - ClFloatPiLow) - ClFloatPi;
}

//
// --------------------------------------------------------- Internal Functions
//

