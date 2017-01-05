/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    trighyp.c

Abstract:

    This module implements support for the hyperbolic trigonometric functions:
    sinh, cosh, and tanh.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 29-Aug-2013

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

#define TWENTY_TWO_HIGH_WORD 0x40360000
#define SINH_TINY_HIGH_WORD 0x3E300000
#define SINH_MID_RANGE_HIGH_WORD 0x40862E42
#define SINH_OVERFLOW_HIGH_WORD 0x408633CE

#define COSH_HALF_LN2_HIGH_WORD 0x3FD62E42
#define COSH_TINY_HIGH_WORD 0x3C800000
#define COSH_HUGE_HIGH_WORD 0x40862E42
#define COSH_HUGE_THRESHOLD_HIGH_WORD 0x408633CE

#define TANH_TINY_HIGH_WORD 0x3E300000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

double
ClpLoadExponentExpBig (
    double Value,
    int Exponent
    );

double
ClpExpBig (
    double Value,
    int *Exponent
    );

//
// -------------------------------------------------------------------- Globals
//

const double ClSinhHuge = 1.0E307;

const ULONG ClExpReductionConstant = 1799;
const double ClExpReductionConstantTimesLn2 = 1246.97177782734161156;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
sinh (
    double Value
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

    LONG AbsoluteHighWord;
    double ExpMinusOne;
    double Half;
    LONG HighWord;
    DOUBLE_PARTS Parts;

    //
    // Mathematically sinh(x) is defined to be (exp(x) - exp(-x)) / 2.
    // 1. Replace x by |x|, as (sinh(-x) = -sinh(x)).
    // 2.
    //
    //                                             E + E / (E + 1)
    //      0        <= x <= 22     :  sinh(x) := ----------------, E = expm1(x)
    //                                                  2
    //
    //      22       <= x <= lnovft :  sinh(x) := exp(x) / 2
    //      lnovft   <= x <= ln2ovft:  sinh(x) := exp(x / 2) / 2 * exp(x / 2)
    //      ln2ovft  <  x           :  sinh(x) := x * sinhhuge (overflow)
    //

    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    AbsoluteHighWord = HighWord &
                       (~(DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT));

    //
    // Handle Infinity or NaN.
    //

    if (AbsoluteHighWord >= NAN_HIGH_WORD) {
        return Value + Value;
    }

    Half = 0.5;
    if (HighWord < 0) {
        Half = -Half;
    }

    //
    // If the |Value| is between [0,22], return
    // sign(Value) * 0.5 * (E + E / (E + 1)).
    //

    if (AbsoluteHighWord < TWENTY_TWO_HIGH_WORD) {
        if (AbsoluteHighWord < SINH_TINY_HIGH_WORD) {
            if (ClSinhHuge + Value > ClDoubleOne) {

                //
                // With very tiny values, sinh(x) is x with an inexact
                // condition.
                //

                return Value;
            }
        }

        ExpMinusOne = expm1(fabs(Value));
        if (AbsoluteHighWord < DOUBLE_ONE_HIGH_WORD) {
            return Half * (2.0 * ExpMinusOne - ExpMinusOne * ExpMinusOne /
                           (ExpMinusOne + ClDoubleOne));
        }

        return Half * (ExpMinusOne + ExpMinusOne / (ExpMinusOne + ClDoubleOne));
    }

    //
    // For |Value| in [22, log(maxdouble)] return 0.5 * exp(|Value|).
    //

    if (AbsoluteHighWord < SINH_MID_RANGE_HIGH_WORD) {
        return Half * exp(fabs(Value));
    }

    //
    // For |Value| in [log(maxdouble), overflowthresold].
    //

    if (AbsoluteHighWord <= SINH_OVERFLOW_HIGH_WORD) {
        return Half * 2.0 * ClpLoadExponentExpBig(fabs(Value), -1);
    }

    //
    // Finally, for absolute values greater than the overflow threshold, cause
    // an overflow.
    //

    return Value * ClSinhHuge;
}

LIBC_API
double
cosh (
    double Value
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

    //
    // Method :
    // mathematically cosh(x) if defined to be (exp(x) + exp(-x)) / 2
    //  1. Replace x by |x| (cosh(x) = cosh(-x)).
    //  2.
    //                                                 [ exp(x) - 1 ]^2
    //      0        <= x <= ln2/2  :  cosh(x) := 1 + -------------------
    //                                                    2 * exp(x)
    //
    //                                             exp(x) +  1 / exp(x)
    //      ln2/2    <= x <= 22     :  cosh(x) := ---------------------
    //                                                       2
    //
    //      22       <= x <= lnovft :  cosh(x) := exp(x) / 2
    //      lnovft   <= x <= ln2ovft:  cosh(x) := exp(x / 2) / 2 * exp(x / 2)
    //      ln2ovft  <  x           :  cosh(x) := (overflow)
    //
    // Special cases:
    //  cosh(x) is |x| if x is +INF, -INF, or NaN.
    //  only cosh(0)=1 is exact for finite x.
    //

    LONG AbsoluteHighWord;
    double Exponential;
    double ExponentialPlusOne;
    DOUBLE_PARTS Parts;

    Parts.Double = Value;
    AbsoluteHighWord = Parts.Ulong.High;
    AbsoluteHighWord &= (~DOUBLE_SIGN_BIT) >> DOUBLE_HIGH_WORD_SHIFT;

    //
    // Handle Infinity or NaN.
    //

    if (AbsoluteHighWord >= NAN_HIGH_WORD) {
        return Value * Value;
    }

    //
    // If |Value| is in [0, 0.5 * ln2], return
    // 1 * expm1(|Value|)^2 / (2 * exp(|Value|))
    //

    if (AbsoluteHighWord <= COSH_HALF_LN2_HIGH_WORD) {
        Exponential = expm1(fabs(Value));
        ExponentialPlusOne = ClDoubleOne + Exponential;

        //
        // The cosh of a tiny value is 1.
        //

        if (AbsoluteHighWord < COSH_TINY_HIGH_WORD) {
            return ClDoubleOne;
        }

        return ClDoubleOne + (Exponential * Exponential) /
                             (ExponentialPlusOne + ExponentialPlusOne);
    }

    //
    // If |Value| is in [0.5*ln2, 22], return
    // (exp(|Value|) + 1 / exp(|Value|) / 2.
    //

    if (AbsoluteHighWord < TWENTY_TWO_HIGH_WORD) {
        Exponential = exp(fabs(Value));
        return ClDoubleOneHalf * Exponential + ClDoubleOneHalf / Exponential;
    }

    //
    // If |Value| is in [22, log(maxdouble)] return 0.5 * exp(|Value|).
    //

    if (AbsoluteHighWord < COSH_HUGE_HIGH_WORD)  {
        return ClDoubleOneHalf * exp(fabs(Value));
    }

    //
    // If |Value| is in [log(maxdouble), overflowthresold].
    //

    if (AbsoluteHighWord <= COSH_HUGE_THRESHOLD_HIGH_WORD) {
        return ClpLoadExponentExpBig(fabs(Value), -1);
    }

    //
    // The value is really big, return an overflow.
    //

    return ClDoubleHugeValue * ClDoubleHugeValue;
}

LIBC_API
double
tanh (
    double Value
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

    LONG AbsoluteHighWord;
    double ExpMinusOne;
    LONG HighWord;
    DOUBLE_PARTS Parts;
    double Result;

    //
    // Method :
    //                                x    -x
    //                               e  - e
    //  0. tanh(x) is defined to be -----------
    //                                x    -x
    //                               e  + e
    //
    //  1. Reduce x to non-negative by tanh(-x) = -tanh(x).
    //  2.  0      <= x <  2^-28  : tanh(x) := x with inexact if x != 0
    //                                          -t
    //      2**-28 <= x <  1      : tanh(x) := -----; t = expm1(-2x)
    //                                         t + 2
    //                                               2
    //      1      <= x <  22     : tanh(x) := 1 - -----; t = expm1(2x)
    //                                             t + 2
    //      22     <= x <= INF    : tanh(x) := 1.
    //
    // Special cases:
    //  tanh(NaN) is NaN;
    //  only tanh(0)=0 is exact for finite argument.
    //

    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    AbsoluteHighWord = HighWord &
                       ((~DOUBLE_SIGN_BIT) >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // Handle Inf or NaN.
    //

    if (AbsoluteHighWord >= NAN_HIGH_WORD) {

        //
        // Tanh(+-Infinity) = +-1.
        //

        if (HighWord >= 0) {
            return ClDoubleOne / Value + ClDoubleOne;

        //
        // Tanh(NaN) = NaN.
        //

        } else {
            return ClDoubleOne / Value - ClDoubleOne;
        }
    }

    //
    // Handle |Values| < 22.
    //

    if (AbsoluteHighWord < TWENTY_TWO_HIGH_WORD) {
        if (AbsoluteHighWord < TANH_TINY_HIGH_WORD) {

            //
            // Tanh of a tiny value is a tiny value with inexact.
            //

            if (ClDoubleHugeValue + Value > ClDoubleOne) {
                return Value;
            }
        }

        if (AbsoluteHighWord >= DOUBLE_ONE_HIGH_WORD) {
            ExpMinusOne = expm1(2.0 * fabs(Value));
            Result = ClDoubleOne - 2.0 / (ExpMinusOne + 2.0);

        } else {
            ExpMinusOne = expm1(-2.0 * fabs(Value));
            Result= -ExpMinusOne / (ExpMinusOne + 2.0);
        }

    //
    // If the |Value) is >= 22, return +-1.
    //

    } else {

        //
        // Raise the inexact flag.
        //

        Result = ClDoubleOne - ClDoubleTinyValue;
    }

    if (HighWord >= 0) {
        return Result;
    }

    return -Result;
}

//
// --------------------------------------------------------- Internal Functions
//

double
ClpLoadExponentExpBig (
    double Value,
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
    double ExpResult;
    DOUBLE_PARTS Parts;
    double Scale;

    ExpResult = ClpExpBig(Value, &ExpExponent);
    Exponent += ExpExponent;
    Parts.Ulong.High = (DOUBLE_EXPONENT_BIAS + Exponent) <<
                       (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    Parts.Ulong.Low = 0;
    Scale = Parts.Double;
    return ExpResult * Scale;
}

double
ClpExpBig (
    double Value,
    int *Exponent
    )

/*++

Routine Description:

    This routine computes exp(x), scaled to avoid spurious overflow. An
    exponent is returned separately. The input is assumed to be >= ln(DBL_MAX)
    and < ln(2 * DBL_MAX / DBL_MIN_DENORM) ~= 1454.91.

Arguments:

    Value - Supplies the input value of the computation.

    Exponent - Supplies a pointer where the exponent will be returned.

Return Value:

    Returns exp(Value), somewhere between 2^1023 and 2^1024.

--*/

{

    ULONG ExponentShift;
    double ExpResult;
    ULONG HighWord;
    DOUBLE_PARTS Parts;

    //
    // Use exp(Value) = exp(Value - kln2) * 2^k, carefully chosen to
    // minimize |exp(kln2) - 2^k|.  Aalso scale the exponent to MAX_EXP so
    // that the result can be multiplied by a tiny number without losing
    // accuracy due to denormalization.
    //

    ExpResult = exp(Value - ClExpReductionConstantTimesLn2);
    Parts.Double = ExpResult;
    HighWord = Parts.Ulong.High;
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    *Exponent = (HighWord >> ExponentShift) -
                (DOUBLE_EXPONENT_BIAS + 1023) + ClExpReductionConstant;

    Parts.Ulong.High = (HighWord & DOUBLE_HIGH_VALUE_MASK) |
                       ((DOUBLE_EXPONENT_BIAS + 1023) << ExponentShift);

    return Parts.Double;
}

