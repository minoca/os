/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    trigf.c

Abstract:

    This module implements the base trignometric mathematical functions (sine,
    cosine, and tangent).

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

#define FLOAT_SINE_SMALL_VALUE_WORD 0x32000000
#define FLOAT_COSINE_SMALL_VALUE_WORD 0x32000000
#define FLOAT_COSINE_MEDIUM_VALUE_WORD 0x3E99999A
#define FLOAT_COSINE_HIGH_VALUE_WORD 0x3F480000

#define FLOAT_TANGENT_SMALL_VALUE_WORD 0x39000000
#define FLOAT_TANGENT_THRESHOLD_WORD 0x3F2CA140
#define FLOAT_TANGENT_ONE_TO_NEGATIVE_THIRTEEN 0x1p-13

#define FLOAT_PI_OVER_TWO_WORD 0x3FC90FD0
#define FLOAT_PI_OVER_TWO_MASK 0xFFFFFFF0
#define FLOAT_PI_OVER_2_MEDIUM_WORD_LIMIT 0x43490F80

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

float
ClpSineFloat (
    float Value,
    float Tail,
    BOOL TailValid
    );

float
ClpCosineFloat (
    float Value,
    float Tail
    );

float
ClpTangentFloat (
    float Value,
    float Tail,
    INT TailAndSign
    );

INT
ClpRemovePiOver2Float (
    float Value,
    float Remainder[2]
    );

INT
ClpSubtractPiOver2MultipleFloat (
    float Value,
    BOOL Positive,
    int Multiplier,
    float Remainder[2]
    );

INT
ClpRemovePiOver2BigFloat (
    float *Input,
    float Output[3],
    LONG InputExponent,
    LONG InputCount,
    FLOATING_PRECISION Precision
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the coefficients of the polynomial used to compute sine.
//

const float ClFloatSine1 = -1.6666667163e-01;
const float ClFloatSine2 = 8.3333337680e-03;
const float ClFloatSine3 = -1.9841270114e-04;
const float ClFloatSine4 = 2.7557314297e-06;
const float ClFloatSine5 = -2.5050759689e-08;
const float ClFloatSine6 = 1.5896910177e-10;

//
// Define the coefficients of the polynomial used to compute cosine.
//

const float ClFloatCosine0 = 1.0000000000e+00;
const float ClFloatCosine1 = 4.1666667908e-02;
const float ClFloatCosine2 = -1.3888889225e-03;
const float ClFloatCosine3 = 2.4801587642e-05;
const float ClFloatCosine4 = -2.7557314297e-07;
const float ClFloatCosine5 = 2.0875723372e-09;
const float ClFloatCosine6 = -1.1359647598e-11;

//
// Define the coefficients of the polynomial used to compute the tangent
// function.
//

const float ClFloatTangent[13] = {
    3.3333334327e-01,
    1.3333334029e-01,
    5.3968254477e-02,
    2.1869488060e-02,
    8.8632395491e-03,
    3.5920790397e-03,
    1.4562094584e-03,
    5.8804126456e-04,
    2.4646313977e-04,
    7.8179444245e-05,
    7.1407252108e-05,
    -1.8558637748e-05,
    2.5907305826e-05,
};

//
// Define some basic constants. The tail is the result of subtracting the
// pi over 2 variable from the real pi/2. The 2 and 3 versions define the next
// least significant bits for pi/2.
//

const float ClFloatPiOverTwo1 = 1.5707855225e+00;
const float ClFloatPiOverTwo1Tail = 1.0804334124e-05;
const float ClFloatPiOverTwo2 = 1.0804273188e-05;
const float ClFloatPiOverTwo2Tail = 6.0770999344e-11;
const float ClFloatPiOverTwo3 = 6.0770943833e-11;
const float ClFloatPiOverTwo3Tail = 8.47842766036889956997e-32;
const float ClFloatInversePiOverTwo = 6.3661980629e-01;
const float ClFloatTwo8 = 2.5600000000e+02;
const float ClFloatTwoNegative8 = 3.9062500000e-03;

//
// Define the continuing bits of pi/2.
//

const float ClFloatPiOver2[11] = {
    1.5703125000e+00,
    4.5776367188e-04,
    2.5987625122e-05,
    7.5437128544e-08,
    6.0026650317e-11,
    7.3896444519e-13,
    5.3845816694e-15,
    5.6378512969e-18,
    8.3009228831e-20,
    3.2756352257e-22,
    6.3331015649e-25,
};

//
// Define the initial number of pi/2 terms to use for a given precision level.
//

INT ClFloatPiOverTwoInitialTermCount[FloatingPrecisionCount] = {4, 7, 9};

//
// This array stores constants for -pi/2.
//

const ULONG ClFloatNegativePiOverTwoIntegers[] = {
    0x3fc90f00, 0x40490f00, 0x4096cb00, 0x40c90f00, 0x40fb5300,
    0x4116cb00, 0x412fed00, 0x41490f00, 0x41623100, 0x417b5300,
    0x418a3a00, 0x4196cb00, 0x41a35c00, 0x41afed00, 0x41bc7e00,
    0x41c90f00, 0x41d5a000, 0x41e23100, 0x41eec200, 0x41fb5300,
    0x4203f200, 0x420a3a00, 0x42108300, 0x4216cb00, 0x421d1400,
    0x42235c00, 0x4229a500, 0x422fed00, 0x42363600, 0x423c7e00,
    0x4242c700, 0x42490f00
};

//
// Define a table of constants for 2/pi. This is an integer array, each
// element containing 24 bits of 2/pi after the binary point. The corresponding
// floating value is array[i] * 2^(-24 * (i + 1)).
// Note that this table must have at least (Exponent - 3) / 24 + InitialCount
// terms.
//

const ULONG ClFloatTwoOverPiIntegers[] = {
  0xA2, 0xF9, 0x83, 0x6E, 0x4E, 0x44, 0x15, 0x29, 0xFC,
  0x27, 0x57, 0xD1, 0xF5, 0x34, 0xDD, 0xC0, 0xDB, 0x62,
  0x95, 0x99, 0x3C, 0x43, 0x90, 0x41, 0xFE, 0x51, 0x63,
  0xAB, 0xDE, 0xBB, 0xC5, 0x61, 0xB7, 0x24, 0x6E, 0x3A,
  0x42, 0x4D, 0xD2, 0xE0, 0x06, 0x49, 0x2E, 0xEA, 0x09,
  0xD1, 0x92, 0x1C, 0xFE, 0x1D, 0xEB, 0x1C, 0xB1, 0x29,
  0xA7, 0x3E, 0xE8, 0x82, 0x35, 0xF5, 0x2E, 0xBB, 0x44,
  0x84, 0xE9, 0x9C, 0x70, 0x26, 0xB4, 0x5F, 0x7E, 0x41,
  0x39, 0x91, 0xD6, 0x39, 0x83, 0x53, 0x39, 0xF4, 0x9C,
  0x84, 0x5F, 0x8B, 0xBD, 0xF9, 0x28, 0x3B, 0x1F, 0xF8,
  0x97, 0xFF, 0xDE, 0x05, 0x98, 0x0F, 0xEF, 0x2F, 0x11,
  0x8B, 0x5A, 0x0A, 0x6D, 0x1F, 0x6D, 0x36, 0x7E, 0xCF,
  0x27, 0xCB, 0x09, 0xB7, 0x4F, 0x46, 0x3F, 0x66, 0x9E,
  0x5F, 0xEA, 0x2D, 0x75, 0x27, 0xBA, 0xC7, 0xEB, 0xE5,
  0xF1, 0x7B, 0x3D, 0x07, 0x39, 0xF7, 0x8A, 0x52, 0x92,
  0xEA, 0x6B, 0xFB, 0x5F, 0xB1, 0x1F, 0x8D, 0x5D, 0x08,
  0x56, 0x03, 0x30, 0x46, 0xFC, 0x7B, 0x6B, 0xAB, 0xF0,
  0xCF, 0xBC, 0x20, 0x9A, 0xF4, 0x36, 0x1D, 0xA9, 0xE3,
  0x91, 0x61, 0x5E, 0xE6, 0x1B, 0x08, 0x65, 0x99, 0x85,
  0x5F, 0x14, 0xA0, 0x68, 0x40, 0x8D, 0xFF, 0xD8, 0x80,
  0x4D, 0x73, 0x27, 0x31, 0x06, 0x06, 0x15, 0x56, 0xCA,
  0x73, 0xA8, 0xC9, 0x60, 0xE2, 0x7B, 0xC0, 0x8C, 0x6B,
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
sinf (
    float Value
    )

/*++

Routine Description:

    This routine returns the sine of the given value.

Arguments:

    Value - Supplies the value to compute the sine of, in radians.

Return Value:

    Returns the sine of the value.

--*/

{

    LONG AbsoluteWord;
    float Output[2];
    INT PiOver2Count;
    FLOAT_PARTS ValueParts;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;

    //
    // See if the absolute value of x is less than pi/4. If it is, the
    // calculation can be done directly. If it's too small, sin approximates to
    // the value itself.
    //

    if (AbsoluteWord <= FLOAT_PI_OVER_4_WORD) {
        return ClpSineFloat(Value, 0.0, FALSE);
    }

    //
    // Sine of infinity or NaN is NaN.
    //

    if (AbsoluteWord >= FLOAT_NAN) {
        return Value - Value;
    }

    //
    // Range reduction is needed to get the value near pi/4.
    //

    PiOver2Count = ClpRemovePiOver2Float(Value, Output);
    switch (PiOver2Count & 3) {
    case 0:
        return ClpSineFloat(Output[0], Output[1], TRUE);

    case 1:
        return ClpCosineFloat(Output[0], Output[1]);

    case 2:
        return -ClpSineFloat(Output[0], Output[1], TRUE);

    case 3:
        break;
    }

    return -ClpCosineFloat(Output[0], Output[1]);
}

LIBC_API
float
cosf (
    float Value
    )

/*++

Routine Description:

    This routine returns the cosine of the given value.

Arguments:

    Value - Supplies the value to compute the cosine of, in radians.

Return Value:

    Returns the cosine of the value.

--*/

{

    LONG AbsoluteWord;
    float Output[2];
    LONG PiOver2Count;
    FLOAT_PARTS ValueParts;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;

    //
    // The computation can be done directly with values less than or equal to
    // pi/4.
    //

    if (AbsoluteWord <= FLOAT_PI_OVER_4_WORD) {
        return ClpCosineFloat(Value, 0.0);
    }

    //
    // Cosine of infinity or NaN is NaN.
    //

    if (AbsoluteWord >= FLOAT_NAN) {
        return Value - Value;
    }

    //
    // Range reduction is needed to get the value near pi/4.
    //

    PiOver2Count = ClpRemovePiOver2Float(Value, Output);
    switch (PiOver2Count & 3) {
    case 0:
        return ClpCosineFloat(Output[0], Output[1]);

    case 1:
        return -ClpSineFloat(Output[0], Output[1], TRUE);

    case 2:
        return -ClpCosineFloat(Output[0], Output[1]);

    default:
        break;
    }

    return ClpSineFloat(Output[0], Output[1], TRUE);
}

LIBC_API
float
tanf (
    float Value
    )

/*++

Routine Description:

    This routine returns the tangent of the given value.

Arguments:

    Value - Supplies the value to compute the tangent of, in radians.

Return Value:

    Returns the cosine of the value.

--*/

{

    LONG AbsoluteWord;
    float Output[2];
    LONG PiOver2Count;
    INT TailAndSign;
    FLOAT_PARTS ValueParts;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;

    //
    // If the absolute value is near pi/4, no fuss is needed.
    //

    if (AbsoluteWord <= FLOAT_PI_OVER_4_WORD) {
        return ClpTangentFloat(Value, 0.0, 1);
    }

    //
    // Tangent of infinity or NaN is NaN.
    //

    if (AbsoluteWord >= FLOAT_NAN) {
        return Value - Value;
    }

    //
    // Range reduction is needed here.
    //

    PiOver2Count = ClpRemovePiOver2Float(Value, Output);

    //
    // Compute regular tangent on even quadrants, and -1/tangent on odd
    // quadrants.
    //

    TailAndSign = 1;
    if ((PiOver2Count & 0x1) != 0) {
        TailAndSign = -1;
    }

    return ClpTangentFloat(Output[0], Output[1], TailAndSign);
}

//
// --------------------------------------------------------- Internal Functions
//

float
ClpSineFloat (
    float Value,
    float Tail,
    BOOL TailValid
    )

/*++

Routine Description:

    This routine implements the sine function along the range of
    [-pi/4 to pi/4].

Arguments:

    Value - Supplies the value between -pi/4 and pi/4.

    Tail - Supplies the tail of the value.

    TailValid - Supplies a boolean indicating if the tail is valid (TRUE) or
        assumed to be zero (FALSE).

Return Value:

    Returns the sine of the given value.

--*/

{

    LONG AbsoluteWord;
    FLOAT_PARTS Parts;
    float Sine;
    float UpperDegrees;
    float Value2;
    float Value3;

    //
    // If the absolute value is less than 2^-27, generate inexact.
    //

    Parts.Float = Value;
    AbsoluteWord = Parts.Ulong & ~FLOAT_SIGN_BIT;
    if (AbsoluteWord < FLOAT_SINE_SMALL_VALUE_WORD) {
        if ((INT)Value == 0) {
            return Value;
        }
    }

    Value2 = Value * Value;
    Value3 = Value2 * Value;
    UpperDegrees = ClFloatSine2 + Value2 *
                   (ClFloatSine3 + Value2 *
                    (ClFloatSine4 + Value2 *
                     (ClFloatSine5 + Value2 * ClFloatSine6)));

    if (TailValid == FALSE) {
        Sine = Value + Value3 * (ClFloatSine1 + Value2 * UpperDegrees);

    } else {
        Sine = Value -
               ((Value2 * (ClFloatOneHalf * Tail - Value3 * UpperDegrees) -
                 Tail) -
                Value3 * ClFloatSine1);
    }

    return Sine;
}

float
ClpCosineFloat (
    float Value,
    float Tail
    )

/*++

Routine Description:

    This routine implements the cosine function along the range of
    [-pi/4 to pi/4].

Arguments:

    Value - Supplies the value between -pi/4 and pi/4.

    Tail - Supplies the tail of the value.

Return Value:

    Returns the sine of the given value.

--*/

{

    LONG AbsoluteWord;
    float Flipped;
    FLOAT_PARTS Parts;
    float Result;
    float UpperDegrees;
    float Value2;
    float Value2Over2;
    float ValueOver4;

    //
    // If the absolute value is less than 2^-27, generate inexact.
    //

    Parts.Float = Value;
    AbsoluteWord = Parts.Ulong & ~FLOAT_SIGN_BIT;
    if (AbsoluteWord < FLOAT_COSINE_SMALL_VALUE_WORD) {
        if ((INT)Value == 0) {
            return ClFloatOne;
        }
    }

    Value2 = Value * Value;
    UpperDegrees = Value2 *
                   (ClFloatCosine1 + Value2 *
                    (ClFloatCosine2 + Value2 *
                     (ClFloatCosine3 + Value2 *
                      (ClFloatCosine4 + Value2 *
                       (ClFloatCosine5 + Value2 * ClFloatCosine6)))));

    Value2Over2 = ClFloatOneHalf * Value2;
    if (AbsoluteWord < FLOAT_COSINE_MEDIUM_VALUE_WORD) {
        Result = Value2Over2 - (Value2 * UpperDegrees - Value * Tail);
        return ClFloatOne - Result;
    }

    if (AbsoluteWord > FLOAT_COSINE_HIGH_VALUE_WORD) {
        ValueOver4 = (float)0.28125;

    } else {
        Parts.Ulong = AbsoluteWord - (1 << (FLOAT_EXPONENT_SHIFT + 1));
        ValueOver4 = Parts.Float;
    }

    Value2Over2 -= ValueOver4;
    Flipped = ClDoubleOne - ValueOver4;
    return Flipped - (Value2Over2 - (Value2 * UpperDegrees - Value * Tail));
}

float
ClpTangentFloat (
    float Value,
    float Tail,
    INT TailAndSign
    )

/*++

Routine Description:

    This routine implements the tangent function along the range of
    [-pi/4 to pi/4].

Arguments:

    Value - Supplies the value between -pi/4 and pi/4.

    Tail - Supplies the tail of the value.

    TailAndSign - Supplies zero if the tail is assumed to be zero, 1 if the
        positive tangent is to be computed, and -1 if -1/tangent is to be
        computed.

Return Value:

    Returns the tangent of the given value.

--*/

{

    LONG AbsoluteWord;
    float Evens;
    float InverseTangent;
    float InverseTangentHigh;
    float Odds;
    float Sign;
    float Tangent;
    float TangentHigh;
    float TangentTerms;
    float Value2;
    float Value3;
    float Value4;
    FLOAT_PARTS ValueParts;
    float ValueSign;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;

    //
    // If the absolute value is less than 2^-13, generate inexact.
    //

    if (AbsoluteWord < FLOAT_TANGENT_SMALL_VALUE_WORD) {
        if ((INT)Value == 0) {
            if ((AbsoluteWord | (TailAndSign + 1)) == 0) {
                return ClFloatOne / fabsf(Value);
            }

            if (TailAndSign == 1) {
                return Value;
            }

            return -ClFloatOne / Value;
        }
    }

    //
    // For values above a certain threshold near pi/4, let y = pi/4 - Value.
    // Then:
    // tan(x) = tan(pi/4-y) = (1-tan(y)) / (1+tan(y))) =
    // 1 + 2*tan(y) - (tan(y)^2) / (1+tan(y)))
    //

    if (AbsoluteWord >= FLOAT_TANGENT_THRESHOLD_WORD) {
        if (Word < 0) {
            Value = -Value;
            Tail = -Tail;
        }

        Value2 = ClFloatPiOver4 - Value;
        Value4 = ClFloatPiOver4Tail - Tail;
        Value = Value2 + Value4;
        Tail = 0.0;
        if (fabsf(Value) < FLOAT_TANGENT_ONE_TO_NEGATIVE_THIRTEEN) {
            ValueSign = 1.0;
            if ((Word & FLOAT_SIGN_BIT) != 0) {
                ValueSign = -1.0;
            }

            return ValueSign * TailAndSign *
                   ((float)1.0 - (float)2.0 * TailAndSign * Value);
        }
    }

    Value2 = Value * Value;
    Value4 = Value2 * Value2;

    //
    // Break Value5 * (T[1] + Value2 * T[2] + ...) into
    // Value5 * (T[1] + Value4 * T[3] + ... + Value20 * T[11]) +
    // Value5 * (Value2 * (T[2] + Value4 * T[4] + ... + Value22 * T[12])).
    // Otherwise the parentheses just get maddening.
    //

    Odds = ClFloatTangent[1] + Value4 *
           (ClFloatTangent[3] + Value4 *
            (ClFloatTangent[5] + Value4 *
             (ClFloatTangent[7] + Value4 *
              (ClFloatTangent[9] + Value4 * ClFloatTangent[11]))));

    Evens = Value2 *
            (ClFloatTangent[2] + Value4 *
             (ClFloatTangent[4] + Value4 *
              (ClFloatTangent[6] + Value4 *
               (ClFloatTangent[8] + Value4 *
                (ClFloatTangent[10] + Value4 * ClFloatTangent[12])))));

    Value3 = Value2 * Value;
    TangentTerms = Tail + Value2 * (Value3 * (Odds + Evens) + Tail);
    TangentTerms += ClFloatTangent[0] * Value3;
    Tangent = Value + TangentTerms;
    if (AbsoluteWord >= FLOAT_TANGENT_THRESHOLD_WORD) {
        Sign = (float)TailAndSign;
        ValueSign = 1.0;
        if ((Word & FLOAT_SIGN_BIT) != 0) {
            ValueSign = -1.0;
        }

        Tangent = ValueSign *
                  (Sign - (float)2.0 *
                   (Value - (Tangent * Tangent / (Tangent + Sign) -
                             TangentTerms)));

        return Tangent;
    }

    if (TailAndSign == 1) {
        return Tangent;
    }

    //
    // Compute -1.0 / (Value + Odds) accurately.
    //

    ValueParts.Float = Tangent;
    ValueParts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
    TangentHigh = ValueParts.Float;

    //
    // TangentHigh + Events = Odds + Value.
    //

    Evens = TangentTerms - (TangentHigh - Value);
    InverseTangent = -(float)1.0 / Tangent;
    ValueParts.Float = InverseTangent;
    ValueParts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
    InverseTangentHigh = ValueParts.Float;
    Value3 = (float)1.0 + InverseTangentHigh * TangentHigh;
    Tangent = InverseTangentHigh + InverseTangent *
              (Value3 + InverseTangentHigh * Evens);

    return Tangent;
}

INT
ClpRemovePiOver2Float (
    float Value,
    float Remainder[2]
    )

/*++

Routine Description:

    This routine removes multiples of pi/2 from the given value.

Arguments:

    Value - Supplies the value to reduce.

    Remainder - Supplies a pointer where the remainder of the number after
        reducing pi/2 will be returned.

Return Value:

    Returns the value divided by pi/2.

--*/

{

    float AbsoluteValue;
    LONG AbsoluteWord;
    LONG Exponent;
    LONG ExponentDifference;
    float Extra;
    float Input[3];
    ULONG InputCount;
    ULONG InputIndex;
    float Output[3];
    LONG PiOver2Count;
    BOOL Positive;
    float PreviousExtra;
    FLOAT_PARTS RaisedValue;
    FLOAT_PARTS RemainderParts;
    ULONG RemainderWord;
    INT Result;
    float RoundedValue;
    float Tail;
    FLOAT_PARTS ValueParts;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;
    Positive = TRUE;
    if ((ValueParts.Ulong & FLOAT_SIGN_BIT) != 0) {
        Positive = FALSE;
    }

    //
    // If the absolute value is less than or equal pi/4, there is no need for
    // reduction.
    //

    if (AbsoluteWord <= FLOAT_PI_OVER_4_WORD) {
        Remainder[0] = Value;
        Remainder[1] = 0.0;
        return 0;
    }

    //
    // Use the small version if the value is less than 3pi/4 and is not close
    // to pi or pi/2.
    //

    if (AbsoluteWord < FLOAT_3_PI_OVER_4_WORD) {
        Result = ClpSubtractPiOver2MultipleFloat(Value,
                                                 Positive,
                                                 1,
                                                 Remainder);

        return Result;
    }

    //
    // Use the medium size for value less than or equal 2^7 * (pi/2), or the
    // bad cancellation cases before.
    //

    if (AbsoluteWord <= FLOAT_PI_OVER_2_MEDIUM_WORD_LIMIT) {
        AbsoluteValue = fabsf(Value);
        RoundedValue = AbsoluteValue * ClFloatInversePiOverTwo + ClFloatOneHalf;
        PiOver2Count = (LONG)RoundedValue;
        RoundedValue = (float)PiOver2Count;
        Extra = AbsoluteValue - RoundedValue * ClFloatPiOverTwo1;
        Tail = RoundedValue * ClFloatPiOverTwo1Tail;
        if ((PiOver2Count < 32) &
            ((AbsoluteWord & 0xFFFFFF00) !=
             ClFloatNegativePiOverTwoIntegers[PiOver2Count - 1])) {

            Remainder[0] = Extra - Tail;

        } else {
            Exponent = AbsoluteWord >> FLOAT_EXPONENT_SHIFT;
            Remainder[0] = Extra - Tail;
            RemainderParts.Float = Remainder[0];
            RemainderWord = RemainderParts.Ulong;
            ExponentDifference = Exponent -
                                 ((RemainderWord & FLOAT_EXPONENT_MASK) >>
                                  FLOAT_EXPONENT_SHIFT);

            //
            // For a really high exponent, another iteration is needed. This is
            // good to 57 bits.
            //

            if (ExponentDifference > 8) {
                PreviousExtra = Extra;
                Tail = RoundedValue * ClFloatPiOverTwo2;
                Extra = PreviousExtra - Tail;
                Tail = RoundedValue * ClFloatPiOverTwo2Tail -
                       ((PreviousExtra - Extra) - Tail);

                Remainder[0] = Extra - Tail;
                RemainderParts.Float = Remainder[0];
                RemainderWord = RemainderParts.Ulong;
                ExponentDifference = Exponent -
                                     ((RemainderWord & FLOAT_EXPONENT_MASK) >>
                                      FLOAT_EXPONENT_SHIFT);

                //
                // Do one final iteration, good for 74 bits of accuracy, which
                // should covert all possible cases.
                //

                if (ExponentDifference > 25) {
                    PreviousExtra = Extra;
                    Tail = RoundedValue * ClFloatPiOverTwo3;
                    Extra = PreviousExtra - Tail;
                    Tail = RoundedValue * ClFloatPiOverTwo3Tail -
                           ((PreviousExtra - Extra) - Tail);

                    Remainder[0] = Extra - Tail;
                }
            }
        }

        Remainder[1] = (Extra - Remainder[0]) - Tail;
        if (Word < 0) {
            Remainder[0] = -Remainder[0];
            Remainder[1] = -Remainder[1];
            return -PiOver2Count;
        }

        return PiOver2Count;
    }

    //
    // Making it this far means this is a very large argument. Deal with the
    // value being infinity or NaN real quick.
    //

    if (AbsoluteWord >= FLOAT_NAN) {
        Remainder[0] = Value - Value;
        Remainder[1] = Value - Value;
        return 0;
    }

    //
    // Break the value into three 24-bit integers in single precision format.
    // It looks like this:
    // Exponent = ilogbf(Value) - 7;
    // RaisedValue = scalbnf(Value, -Exponent)
    // for i = 0, 1, 2
    //     Input[i] = floorf(RaisedValue)
    //     RaisedValue = (RaisedValue - Input[i]) * 2^8
    //

    Exponent = (AbsoluteWord >> FLOAT_EXPONENT_SHIFT) - 134;
    RaisedValue.Ulong = AbsoluteWord - (Exponent << FLOAT_EXPONENT_SHIFT);
    for (InputIndex = 0; InputIndex < 2; InputIndex += 1) {
        Input[InputIndex] = (float)(LONG)RaisedValue.Float;
        RaisedValue.Float = (RaisedValue.Float - Input[InputIndex]) *
                            ClFloatTwo8;
    }

    Input[2] = RaisedValue.Float;

    //
    // Skip zero terms.
    //

    InputCount = 3;
    while ((InputCount >= 1) && (Input[InputCount - 1] == ClFloatZero)) {
        InputCount -= 1;
    }

    PiOver2Count = ClpRemovePiOver2BigFloat(Input,
                                            Output,
                                            Exponent,
                                            InputCount,
                                            2);

    Remainder[0] = Output[0];
    Remainder[1] = Output[1];
    if (Word < 0) {
        Remainder[0] = -Remainder[0];
        Remainder[1] = -Remainder[1];
        return -PiOver2Count;
    }

    return PiOver2Count;
}

INT
ClpSubtractPiOver2MultipleFloat (
    float Value,
    BOOL Positive,
    int Multiplier,
    float Remainder[2]
    )

/*++

Routine Description:

    This routine removes multiples of pi/2 from the given value.

Arguments:

    Value - Supplies the value to reduce.

    Positive - Supplies a boolean indicating if the multiplier should be
        negated.

    Multiplier - Supplies the multiplier of pi/2 to use.

    Remainder - Supplies a pointer where the remainder of the number after
        reducing pi/2 will be returned (sum the two values).

Return Value:

    Returns the multiplier (or the negative multiplier if positive is false).

--*/

{

    LONG AbsoluteWord;
    FLOAT_PARTS Parts;
    BOOL PiOverTwo2;
    float PiOverTwoTail;
    float Subtraction;

    Parts.Float = Value;
    AbsoluteWord = Parts.Ulong;
    if ((AbsoluteWord & FLOAT_PI_OVER_TWO_MASK) != FLOAT_PI_OVER_TWO_WORD) {
        PiOverTwo2 = FALSE;
        PiOverTwoTail = ClFloatPiOverTwo1Tail;

    } else {
        PiOverTwo2 = TRUE;
        PiOverTwoTail = ClFloatPiOverTwo2Tail;
    }

    if (Positive != FALSE) {
        Subtraction = Value - Multiplier * ClFloatPiOverTwo1;
        if (PiOverTwo2 != FALSE) {
            Subtraction -= ClFloatPiOverTwo2;
        }

        Remainder[0] = Subtraction - Multiplier * PiOverTwoTail;
        Remainder[1] = (Subtraction - Remainder[0]) -
                       Multiplier * PiOverTwoTail;

        return Multiplier;
    }

    Subtraction = Value + Multiplier * ClFloatPiOverTwo1;
    if (PiOverTwo2 != FALSE) {
        Subtraction += ClFloatPiOverTwo2;
    }

    Remainder[0] = Subtraction + Multiplier * PiOverTwoTail;
    Remainder[1] = (Subtraction - Remainder[0]) + Multiplier * PiOverTwoTail;
    return -Multiplier;
}

INT
ClpRemovePiOver2BigFloat (
    float *Input,
    float Output[3],
    LONG InputExponent,
    LONG InputCount,
    FLOATING_PRECISION Precision
    )

/*++

Routine Description:

    This routine removes multiples of pi/2 from the given large value.

Arguments:

    Input - Supplies a pointer to an array of input values, broken up into
        24-bit chunks of floating point values.

    Output - Supplies a pointer where the output remainder will be returned.
        The result comes from adding these values together.

    InputExponent - Supplies the exponent of the first input double.

    InputCount - Supplies the number of elements in the input array.

    Precision - Supplies the precision of the floating point type.

Return Value:

    Returns the number of pi/2s (mod 8) that went into this input.

--*/

{

    LONG Carry;
    LONG EndIndex;
    float Final;
    float FinalProduct[20];
    LONG HighWord;
    LONG Index;
    LONG Index2;
    LONG InitialTermCount;
    float Integral[20];
    LONG IntegralExponent;
    LONG IntegralInteger[20];
    LONG LastInput;
    LONG NeededTerms;
    float PiOver2[20];
    LONG PiOver2Count;
    LONG TableIndex;
    LONG TermCount;
    float Value;
    volatile float VolatileValue;

    //
    // Get the number of terms to start with based on the supplied precision.
    //

    InitialTermCount = ClFloatPiOverTwoInitialTermCount[Precision];
    LastInput = InputCount - 1;
    TableIndex = (InputExponent - 3) / 8;
    if (TableIndex < 0) {
        TableIndex = 0;
    }

    IntegralExponent =  InputExponent - (8 * (TableIndex + 1));

    //
    // Initialize the pi/2 variable up to LastInput + InitialTermCount, where
    // the value at LastInput + InitialTermCount is the integer array at
    // TableIndex + InitialTermCount.
    //

    Index2 = TableIndex - LastInput;
    EndIndex = LastInput + InitialTermCount;
    for (Index = 0; Index <= EndIndex; Index += 1) {
        if (Index2 < 0) {
            PiOver2[Index] = ClFloatZero;

        } else {
            PiOver2[Index] = (float)ClFloatTwoOverPiIntegers[Index2];
        }

        Index2 += 1;
    }

    //
    // Compute the initial integral values up to the initial term count.
    //

    for (Index = 0; Index <= InitialTermCount; Index += 1) {
        Final = 0.0;
        for (Index2 = 0; Index2 <= LastInput; Index2 += 1) {
            Final += Input[Index2] * PiOver2[LastInput + Index - Index2];
        }

        Integral[Index] = Final;
    }

    TermCount = InitialTermCount;

    //
    // Loop recomputing while the number of terms needed is changing.
    //

    while (TRUE) {

        //
        // Distill the integral array into the integer array in reverse.
        //

        Index = 0;
        Value = Integral[TermCount];
        for (Index2 = TermCount; Index2 > 0; Index2 -= 1) {
            Final = (float)(LONG)(ClFloatTwoNegative8 * Value);
            IntegralInteger[Index] = (LONG)(Value - (ClFloatTwo8 * Final));
            Value = Integral[Index2 - 1] + Final;
            Index += 1;
        }

        //
        // Compute the integer count of dividing by pi/2.
        //

        Value  = scalbnf(Value, IntegralExponent);

        //
        // Trim off anything above 8.
        //

        Value -= (float)8.0 * floorf(Value * (float)0.125);
        PiOver2Count = (LONG)Value;
        Value -= (float)PiOver2Count;
        HighWord = 0;

        //
        // The last integral integer is needed to determine the count of pi/2s.
        //

        if (IntegralExponent > 0) {
            Index = (IntegralInteger[TermCount - 1] >> (8 - IntegralExponent));
            PiOver2Count += Index;
            IntegralInteger[TermCount - 1] -= Index << (8 - IntegralExponent);
            HighWord = IntegralInteger[TermCount - 1] >>
                       (7 - IntegralExponent);

        } else if (IntegralExponent == 0) {
            HighWord = IntegralInteger[TermCount - 1] >> 7;

        } else if (Value >= (float)0.5) {
            HighWord = 2;
        }

        //
        // Check to see if the integral is greater than 0.5.
        //

        if (HighWord > 0) {
            PiOver2Count += 1;
            Carry = 0;
            for (Index = 0; Index < TermCount; Index += 1) {
                Index2 = IntegralInteger[Index];
                if (Carry == 0) {
                    if (Index2 != 0) {
                        Carry = 1;
                        IntegralInteger[Index] = 0x100 - Index2;
                    }

                } else {
                    IntegralInteger[Index] = 0xFF - Index2;
                }
            }

            //
            // 1 in 12 times this will occur.
            //

            if (IntegralExponent > 0) {
                switch (IntegralExponent) {
                case 1:
                    IntegralInteger[TermCount - 1] &= 0x7F;
                    break;

                case 2:
                    IntegralInteger[TermCount - 1] &= 0x3F;
                    break;

                default:
                    break;
                }
            }

            if (HighWord == 2) {
                Value = ClFloatOne - Value;
                if (Carry != 0) {
                    Value -= scalbnf(ClFloatOne, IntegralExponent);
                }
            }
        }

        //
        // Decide whether or not recomputation is necesary.
        //

        if (Value == (float)0.0) {
            Index2 = 0;
            for (Index = TermCount - 1; Index >= InitialTermCount; Index -= 1) {
                Index2 |= IntegralInteger[Index];
            }

            if (Index2 == 0) {
                NeededTerms = 1;
                while (IntegralInteger[InitialTermCount - NeededTerms] == 0) {
                    NeededTerms += 1;
                }

                //
                // Add Integral[TermCount + 1] to
                // Integral[TermCount + NeededTerms].
                //

                for (Index = TermCount + 1;
                     Index <= TermCount + NeededTerms;
                     Index += 1) {

                    PiOver2[LastInput + Index] =
                           (float)ClFloatTwoOverPiIntegers[TableIndex + Index];

                    Final = 0.0;
                    for (Index2 = 0; Index2 <= LastInput; Index2 += 1) {
                        Final += Input[Index2] *
                                 PiOver2[LastInput + Index - Index2];
                    }

                    Integral[Index] = Final;
                }

                TermCount += NeededTerms;

                //
                // Go back up and recompute.
                //

                continue;
            }
        }

        break;
    }

    //
    // Chop off any zero terms.
    //

    if (Value == (float)0.0) {
        TermCount -= 1;
        IntegralExponent -= 8;
        while (IntegralInteger[TermCount] == 0) {
            TermCount -= 1;
            IntegralExponent -= 8;
        }

    //
    // Break the value into 8 bit chunks if necessary.
    //

    } else {
        Value = scalbnf(Value, -IntegralExponent);
        if (Value >= ClFloatTwo8) {
            Final = (float)((LONG)(ClFloatTwoNegative8 * Value));
            IntegralInteger[TermCount] = (LONG)(Value - ClFloatTwo8 * Final);
            TermCount += 1;
            IntegralExponent += 8;
            IntegralInteger[TermCount] = (LONG)Final;

        } else {
            IntegralInteger[TermCount] = (LONG)Value;
        }
    }

    //
    // Convert the integer bit chunk into a floating point value.
    //

    Final = scalbnf(ClFloatOne, IntegralExponent);
    for (Index = TermCount; Index >= 0; Index -= 1) {
        Integral[Index] = Final * (float)IntegralInteger[Index];
        Final *= ClFloatTwoNegative8;
    }

    //
    // Compute PiOver2[0 ... InitialTermCount] * Integral[TermCount ... 0]
    //

    for (Index = TermCount; Index >= 0; Index -= 1) {
        Final = 0.0;
        for (Index2 = 0;
             (Index2 <= InitialTermCount) && (Index2 <= TermCount - Index);
             Index2 += 1) {

            Final += ClFloatPiOver2[Index2] * Integral[Index + Index2];
        }

        FinalProduct[TermCount - Index] = Final;
    }

    //
    // Compress the final product into the output.
    //

    switch (Precision) {
    case FloatingPrecisionSingle:
        Final = 0.0;
        for (Index = TermCount; Index >= 0; Index -= 1) {
            Final += FinalProduct[Index];
        }

        if (HighWord == 0) {
            Output[0] = Final;

        } else {
            Output[0] = -Final;
        }

        break;

    case FloatingPrecisionDouble:
    case FloatingPrecisionExtended:
        Final = 0.0;
        for (Index = TermCount; Index >= 0; Index -= 1) {
            Final += FinalProduct[Index];
        }

        VolatileValue = Final;
        Final = VolatileValue;
        if (HighWord == 0) {
            Output[0] = Final;

        } else {
            Output[0] = -Final;
        }

        Final = FinalProduct[0] - Final;
        for (Index = 1; Index <= TermCount; Index += 1) {
            Final += FinalProduct[Index];
        }

        if (HighWord == 0) {
            Output[1] = Final;

        } else {
            Output[1] = -Final;
        }

        break;

    case FloatingPrecisionQuad:
        for (Index = TermCount; Index > 0; Index -= 1) {
            Final = FinalProduct[Index - 1] + FinalProduct[Index];
            FinalProduct[Index] += FinalProduct[Index - 1] - Final;
            FinalProduct[Index - 1] = Final;
        }

        for (Index = TermCount; Index > 1; Index -= 1) {
            Final = FinalProduct[Index - 1] + FinalProduct[Index];
            FinalProduct[Index] += FinalProduct[Index - 1] - Final;
            FinalProduct[Index - 1] = Final;
        }

        Final = 0.0;
        for (Index = TermCount; Index >= 2; Index -= 1) {
            Final += FinalProduct[Index];
        }

        if (HighWord == 0) {
            Output[0] = FinalProduct[0];
            Output[1] = FinalProduct[1];
            Output[2] = Final;

        } else {
            Output[0] = -FinalProduct[0];
            Output[1] = -FinalProduct[1];
            Output[2] = -Final;
        }

        break;

    default:
        break;
    }

    return PiOver2Count & 7;
}

