/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    trighypf.c

Abstract:

    This module implements support for the hyperbolic trigonometric functions:
    sinh, cosh, and tanh.

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

#define FLOAT_NINE_WORD 0x41100000
#define FLOAT_SINH_TINY_WORD 0x39800000
#define FLOAT_SINH_MID_RANGE_WORD 0x42B17217
#define FLOAT_SINH_OVERFLOW_WORD 0x42B2d4FC

#define FLOAT_COSH_HALF_LN_2_WORD 0x3EB17218
#define FLOAT_COSH_TINY_WORD 0x39800000
#define FLOAT_COSH_HUGE_WORD 0x42B17217
#define FLOAT_COSH_HUGE_THRESHOLD_WORD 0x42B2D4FC

#define FLOAT_TANH_TINY_WORD 0x39800000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

float
ClpFloatLoadExponentExpBig (
    float Value,
    int Exponent
    );

float
ClpFloatExpBig (
    float Value,
    int *Exponent
    );

//
// -------------------------------------------------------------------- Globals
//

const float ClFloatSinhHuge = 1.0e37;

const ULONG ClFloatExpReductionConstant = 235;
const float ClFloatExpReductionConstantTimesLn2 = 162.88958740;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
sinhf (
    float Value
    )

/*++

Routine Description:

    This routine computes the hyperbolic sine of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic sine of.

Return Value:

    Returns the hyperbolic sine on success.

    +/- HUGE_VAL (with the same sign as the value) if the result cannot be
    represented.

    NaN if the input is NaN.

    Returns the value itself if the given value is +/- 0 or +/- Infinity.

--*/

{

    LONG AbsoluteWord;
    float ExpMinusOne;
    float Half;
    LONG Word;
    FLOAT_PARTS Parts;

    Parts.Float= Value;
    Word = Parts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;

    //
    // Handle Infinity or NaN.
    //

    if (AbsoluteWord >= FLOAT_NAN) {
        return Value + Value;
    }

    Half = 0.5;
    if (Word < 0) {
        Half = -Half;
    }

    //
    // If the |Value| is between [0,9], return
    // sign(Value) * 0.5 * (E + E / (E + 1)).
    //

    if (AbsoluteWord < FLOAT_NINE_WORD) {
        if (AbsoluteWord < FLOAT_SINH_TINY_WORD ) {
            if (ClFloatSinhHuge + Value > ClFloatOne) {

                //
                // With very tiny values, sinh(x) is x with an inexact
                // condition.
                //

                return Value;
            }
        }

        ExpMinusOne = expm1f(fabsf(Value));
        if (AbsoluteWord < FLOAT_ONE_WORD) {
            return Half *
                   ((float)2.0 * ExpMinusOne - ExpMinusOne * ExpMinusOne /
                    (ExpMinusOne + ClFloatOne));
        }

        return Half * (ExpMinusOne + ExpMinusOne / (ExpMinusOne + ClFloatOne));
    }

    //
    // For |Value| in [9, log(maxfloat)] return 0.5 * exp(|Value|).
    //

    if (AbsoluteWord < FLOAT_SINH_MID_RANGE_WORD) {
        return Half * expf(fabsf(Value));
    }

    //
    // For |Value| in [log(maxfloat), overflowthresold].
    //

    if (AbsoluteWord <= FLOAT_SINH_OVERFLOW_WORD) {
        return Half * (float)2.0 * ClpFloatLoadExponentExpBig(fabsf(Value), -1);
    }

    //
    // Finally, for absolute values greater than the overflow threshold, cause
    // an overflow.
    //

    return Value * ClFloatSinhHuge;
}

LIBC_API
float
coshf (
    float Value
    )

/*++

Routine Description:

    This routine computes the hyperbolic cosine of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic cosine of.

Return Value:

    Returns the hyperbolic cosine on success.

    +/- HUGE_VAL (with the same sign as the value) if the result cannot be
    represented.

    NaN if the input is NaN.

    1.0 if the value is +/- 0.

    +Infinity if the value is +/- Infinity.

--*/

{

    LONG AbsoluteWord;
    float Exponential;
    float ExponentialPlusOne;
    FLOAT_PARTS Parts;

    Parts.Float = Value;
    AbsoluteWord = Parts.Ulong & ~FLOAT_SIGN_BIT;

    //
    // Handle Infinity or NaN.
    //

    if (AbsoluteWord >= FLOAT_NAN) {
        return Value * Value;
    }

    //
    // If |Value| is in [0, 0.5 * ln2], return
    // 1 * expm1f(|Value|)^2 / (2 * expf(|Value|))
    //

    if (AbsoluteWord <= FLOAT_COSH_HALF_LN_2_WORD) {
        Exponential = expm1f(fabsf(Value));
        ExponentialPlusOne = ClFloatOne + Exponential;

        //
        // The cosh of a tiny value is 1.
        //

        if (AbsoluteWord < FLOAT_COSH_TINY_WORD) {
            return ClFloatOne;
        }

        return ClFloatOne + (Exponential * Exponential) /
                            (ExponentialPlusOne + ExponentialPlusOne);
    }

    //
    // If |Value| is in [0.5*ln2, 9], return
    // (expf(|Value|) + 1 / expf(|Value|) / 2.
    //

    if (AbsoluteWord < FLOAT_NINE_WORD) {
        Exponential = expf(fabsf(Value));
        return ClFloatOneHalf * Exponential + ClFloatOneHalf / Exponential;
    }

    //
    // If |Value| is in [9, log(maxdouble)] return 0.5 * exp(|Value|).
    //

    if (AbsoluteWord < FLOAT_COSH_HUGE_WORD)  {
        return ClFloatOneHalf * expf(fabsf(Value));
    }

    //
    // If |Value| is in [log(maxdouble), overflowthresold].
    //

    if (AbsoluteWord <= FLOAT_COSH_HUGE_THRESHOLD_WORD) {
        return ClpFloatLoadExponentExpBig(fabsf(Value), -1);
    }

    //
    // The value is really big, return an overflow.
    //

    return ClFloatHugeValue * ClFloatHugeValue;
}

LIBC_API
float
tanhf (
    float Value
    )

/*++

Routine Description:

    This routine computes the hyperbolic tangent of the given value.

Arguments:

    Value - Supplies the value to take the hyperbolic tangent of.

Return Value:

    Returns the hyperbolic tangent on success.

    Returns the value itself if the value is +/- 0.

    Returns +/- 1 if the value is +/- Infinity.

    Returns the value itself with a range error if the value is subnormal.

--*/

{

    LONG AbsoluteWord;
    float ExpMinusOne;
    FLOAT_PARTS Parts;
    float Result;
    LONG Word;

    Parts.Float = Value;
    Word = Parts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;

    //
    // Handle Inf or NaN.
    //

    if (AbsoluteWord >= FLOAT_NAN) {

        //
        // Tanh(+-Infinity) = +-1.
        //

        if (Word >= 0) {
            return ClFloatOne / Value + ClFloatOne;

        //
        // Tanh(NaN) = NaN.
        //

        } else {
            return ClFloatOne / Value - ClFloatOne;
        }
    }

    //
    // Handle |Values| < 9.
    //

    if (AbsoluteWord < FLOAT_NINE_WORD) {
        if (AbsoluteWord < FLOAT_TANH_TINY_WORD) {

            //
            // Tanh of a tiny value is a tiny value with inexact.
            //

            if (ClFloatHugeValue + Value > ClFloatOne) {
                return Value;
            }
        }

        if (AbsoluteWord >= FLOAT_ONE_WORD) {
            ExpMinusOne = expm1f((float)2.0 * fabsf(Value));
            Result = ClFloatOne - (float)2.0 / (ExpMinusOne + (float)2.0);

        } else {
            ExpMinusOne = expm1f((float)-2.0 * fabsf(Value));
            Result= -ExpMinusOne / (ExpMinusOne + (float)2.0);
        }

    //
    // If the |Value) is >= 9, return +-1.
    //

    } else {

        //
        // Raise the inexact flag.
        //

        Result = ClFloatOne - ClFloatTinyValue;
    }

    if (Word >= 0) {
        return Result;
    }

    return -Result;
}

//
// --------------------------------------------------------- Internal Functions
//

float
ClpFloatLoadExponentExpBig (
    float Value,
    int Exponent
    )

/*++

Routine Description:

    This routine computes exp(x) * 2^Exponent. They are intended for large
    arguments (real part >= ln(DBL_MAX)), where care is needed to avoid
    overflow. This implementation is narrowly tailored for the hyperbolic
    and exponential function; it assumes the exponent is small (0 or -1) and
    the caller has filtered out very large values for which the overflow would
    be inevitable.

Arguments:

    Value - Supplies the input value of the computation.

    Exponent - Supplies the exponent.

Return Value:

    Returns the exponential * 2^Exponent.

--*/

{

    int ExpExponent;
    float ExpResult;
    FLOAT_PARTS Parts;
    float Scale;

    ExpResult = ClpFloatExpBig(Value, &ExpExponent);
    Exponent += ExpExponent;
    Parts.Ulong = (FLOAT_EXPONENT_BIAS + Exponent) << FLOAT_EXPONENT_SHIFT;
    Scale = Parts.Float;
    return ExpResult * Scale;
}

float
ClpFloatExpBig (
    float Value,
    int *Exponent
    )

/*++

Routine Description:

    This routine computes exp(x), scaled to avoid spurious overflow. An
    exponent is returned separately. The input is assumed to be >= ln(DBL_MAX)
    and < ln(2 * DBL_MAX / DBL_MIN_DENORM) ~= 129.7.

Arguments:

    Value - Supplies the input value of the computation.

    Exponent - Supplies a pointer where the exponent will be returned.

Return Value:

    Returns expf(Value), somewhere between 2^127 and 2^128.

--*/

{

    float ExpResult;
    ULONG Word;
    FLOAT_PARTS Parts;

    //
    // Use expf(Value) = expf(Value - kln2) * 2^k, carefully chosen to
    // minimize |expf(kln2) - 2^k|.  Aalso scale the exponent to MAX_EXP so
    // that the result can be multiplied by a tiny number without losing
    // accuracy due to denormalization.
    //

    ExpResult = expf(Value - ClFloatExpReductionConstantTimesLn2);
    Parts.Float = ExpResult;
    Word = Parts.Ulong;
    *Exponent = (Word >> FLOAT_EXPONENT_SHIFT) -
                (FLOAT_EXPONENT_BIAS + 127) + ClFloatExpReductionConstant;

    Parts.Ulong = (Word & FLOAT_VALUE_MASK) |
                  ((FLOAT_EXPONENT_BIAS + 127) << FLOAT_EXPONENT_SHIFT);

    return Parts.Float;
}

