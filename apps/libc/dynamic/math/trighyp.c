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

#define COSH_TINY_HIGH_WORD 0x3C800000
#define COSH_HUGE_THRESHOLD_HIGH_WORD 0x408633CE

#define TANH_TINY_HIGH_WORD 0x3E300000

#define EXP_MINUS_ONE_BIG_HIGH_WORD 0x4043687A
#define EXP_MINUS_ONE_HUGE_HIGH_WORD 0x40862E42
#define EXP_MINUS_ONE_HALF_LN_2_HIGH_WORD 0x3FD62E42
#define EXP_MINUS_ONE_THREE_HALVES_LN_2_HIGH_WORD 0x3FF0A2B2
#define EXP_MINUS_ONE_TINY_HIGH_WORD 0x3c900000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

double
ClpExpMinusOne (
    double Value
    );

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

const double ClExpMinusOneOverflowThreshold = 7.09782712893383973096e+02;

const double ClExpMinusOne1 = -3.33333333333331316428e-02;
const double ClExpMinusOne2 = 1.58730158725481460165e-03;
const double ClExpMinusOne3 = -7.93650757867487942473e-05;
const double ClExpMinusOne4 = 4.00821782732936239552e-06;
const double ClExpMinusOne5 = -2.01099218183624371326e-07;

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

        ExpMinusOne = ClpExpMinusOne(fabs(Value));
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
    // 1 * ExpMinusOne(|Value|)^2 / (2 * exp(|Value|))
    //

    if (AbsoluteHighWord <= EXP_MINUS_ONE_HALF_LN_2_HIGH_WORD) {
        Exponential = ClpExpMinusOne(fabs(Value));
        ExponentialPlusOne = ClDoubleOne + Exponential;

        //
        // The cosh of a tiny value is 1.
        //

        if (AbsoluteHighWord < COSH_TINY_HIGH_WORD) {
            return ExponentialPlusOne;
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

    if (AbsoluteHighWord < EXP_MINUS_ONE_HUGE_HIGH_WORD)  {
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

    return ClHugeValue * ClHugeValue;
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

            if (ClHugeValue + Value > ClDoubleOne) {
                return Value;
            }
        }

        if (AbsoluteHighWord >= DOUBLE_ONE_HIGH_WORD) {
            ExpMinusOne = ClpExpMinusOne(2.0 * fabs(Value));
            Result = ClDoubleOne - 2.0 / (ExpMinusOne + 2.0);

        } else {
            ExpMinusOne = ClpExpMinusOne(-2.0 * fabs(Value));
            Result= -ExpMinusOne / (ExpMinusOne + 2.0);
        }

    //
    // If the |Value) is >= 22, return +-1.
    //

    } else {

        //
        // Raise the inexact flag.
        //

        Result = ClDoubleOne - ClTinyValue;
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
ClpExpMinusOne (
    double Value
    )

/*++

Routine Description:

    This routine computes the exponential of a given number, minus one:
    exp(Value) - 1.

Arguments:

    Value - Supplies the input value of the computation.

Return Value:

    Returns one less than the exponential.

--*/

{

    double Approximation;
    double Correction;
    double Denominator;
    LONG Exponent;
    ULONG ExponentShift;
    double FinalResult;
    double HalfSquared;
    double HalfValue;
    double High;
    ULONG HighWord;
    double Low;
    DOUBLE_PARTS Parts;
    double Polynomial;
    LONG SignBit;
    double TwoRaisedToExponent;
    volatile double VolatileValue;

    //
    // Method
    //  1. Argument reduction:
    //     Given x, find r and integer k such that
    //
    //               x = k*ln2 + r,  |r| <= 0.5*ln2 ~ 0.34658
    //
    //     Here a correction term c will be computed to compensate
    //     the error in r when rounded to a floating-point number.
    //
    //  2. Approximating expm1(r) by a special rational function on
    //     the interval [0,0.34658]:
    //     Since
    //        r *(exp(r) + 1) / (exp(r) - 1) = 2 + r^2 / 6 - r^4 / 360 + ...
    //     we define R1(r*r) by
    //        r * (exp(r) + 1) / (exp(r) - 1) = 2 + r^2 / 6 * R1(r * r)
    //     That is,
    //        R1(r^2) = 6 / r *((exp(r) + 1) / (exp(r) - 1) - 2 / r)
    //                = 6 / r * (1 + 2.0 * (1 / (exp(r) - 1) - 1 / r))
    //                = 1 - r^2 / 60 + r^4 / 2520 - r^6 / 100800 + ...
    //     Use a special Reme algorithm on [0,0.347] to generate
    //     a polynomial of degree 5 in r*r to approximate R1. The
    //     maximum error of this polynomial approximation is bounded
    //     by 2^-61. In other words,
    //        R1(z) ~ 1.0 + Q1*z + Q2*z^2 + Q3*z^3 + Q4*z^4 + Q5*z^5
    //     where z = r * r,
    //     with error bounded by
    //        |                        5           |     -61
    //        | 1.0 + Q1*z + ... + Q5*z   -  R1(z) | <= 2
    //        |                                    |
    //
    //     expm1(r) = exp(r) - 1 is then computed by the following
    //     specific way which minimizes the accumulation rounding error:
    //                          2     3
    //                         r     r    [ 3 - (R1 + R1 * r/2)  ]
    //         expm1(r) = r + --- + --- * ----------------------
    //                         2     2    [ 6 - r * (3 - R1 * r/2) ]
    //
    //     To compensate the error in the argument reduction, use
    //     expm1(r + c) = expm1(r) + c + expm1(r) * c
    //                  ~ expm1(r) + c + r*c
    //     Thus c + r * c will be added in as the correction terms for
    //     expm1(r + c). Now rearrange the term to avoid optimization
    //     screw up:
    //     expm1(r + c) ~
    //             (        2                                             2 )
    //             ({    ( r    [ R1 -  (3 - R1 * r / 2)  ]    )    }    r  )
    //         r - ({r * (--- * --------------------------- - c) - c} - --- )
    //             ({    ( 2    [ 6 - r * (3 - R1 * r / 2)]    )    }    2  )
    //
    //         = r - E
    //  3. Scale back to obtain expm1(x):
    //     From step 1:
    //        expm1(x) = either 2^k * [expm1(r)+1] - 1
    //                 = or     2^k * [expm1(r) + (1 - 2^-k)]
    //
    //  4. Implementation notes:
    //       (A). To save one multiplication, scale the coefficient Qi
    //            to Qi * 2^i, and replace z by (x^2) / 2.
    //       (B). To achieve maximum accuracy, compute expm1(x) by
    //            (i)   if x < -56*ln2, return -1.0, (raise inexact if x != inf)
    //            (ii)  if k = 0, return r - E
    //            (iii) if k = -1, return 0.5 * (r - E) - 0.5
    //            (iv)  if k = 1 if r < -0.25, return 2 * ((r + 0.5) - E)
    //                  else      return  1.0 + 2.0 * (r - E);
    //            (v)   if (k < -2 || k > 56) return 2^k(1 - (E - r)) - 1
    //                  (or exp(x) - 1)
    //            (vi)  if k <= 20, return 2^k((1 - 2^-k) - (E - r)), else
    //            (vii) return 2^k(1 - ((E + 2^-k) - r))
    //
    //    Special cases:
    //       expm1(INF) is INF, expm1(NaN) is NaN;
    //       expm1(-INF) is -1, and
    //       for finite argument, only expm1(0) = 0 is exact.
    //
    //  Accuracy:
    //    according to an error analysis, the error is always less than
    //    1 ulp (unit in the last place).
    //
    //  Misc. info.
    //   For IEEE double
    //       if x >  7.09782712893383973096e+02 then expm1(x) overflow
    //

    Correction = 0;
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    SignBit = HighWord & (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // Get the absolute value by taking out the sign.
    //

    HighWord &= (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    //
    // Filter out huge and non-finite arguments.
    //

    if (HighWord >= EXP_MINUS_ONE_BIG_HIGH_WORD) {

        //
        // The value is greater than 56 * ln2.
        //

        if (HighWord >= EXP_MINUS_ONE_HUGE_HIGH_WORD) {

            //
            // The value is greater than 709.78.
            //

            if (HighWord >= NAN_HIGH_WORD) {
                if (((HighWord & DOUBLE_HIGH_VALUE_MASK) |
                      Parts.Ulong.Low) != 0) {

                    //
                    // Return NaN.
                    //

                    return Value + Value;

                } else {

                    //
                    // Exp(+-Infinity) = {Infinity, -1}.
                    //

                    if (SignBit == 0) {
                        return Value;
                    }

                    return -1.0;
                }
            }

            if (Value > ClExpMinusOneOverflowThreshold) {

                //
                // Return an overflow.
                //

                return ClHugeValue * ClHugeValue;
            }
        }

        //
        // If the value is < -56*ln2, return -1.0 with inexact.
        //

        if (SignBit != 0) {
            if (Value + ClTinyValue < 0.0) {
                return ClTinyValue - ClDoubleOne;
            }
        }
    }

    //
    // Perform argument reduction.
    //

    if (HighWord > EXP_MINUS_ONE_HALF_LN_2_HIGH_WORD) {
        if (HighWord < EXP_MINUS_ONE_THREE_HALVES_LN_2_HIGH_WORD) {
            if (SignBit == 0) {
                High = Value - ClLn2High[0];
                Low = ClLn2Low[0];
                Exponent = 1;

            } else {
                High = Value + ClLn2High[0];
                Low = -ClLn2Low[0];
                Exponent = -1;
            }

        } else {
            if (SignBit == 0) {
                Exponent  = ClInverseLn2 * Value + 0.5;

            } else {
                Exponent  = ClInverseLn2 * Value - 0.5;
            }

            Approximation = Exponent;

            //
            // Approximation * ln2High is exact here.
            //

            High = Value - Approximation * ClLn2High[0];
            Low = Approximation * ClLn2Low[0];
        }

        VolatileValue = High - Low;
        Value = VolatileValue;
        Correction = (High - Value) - Low;

    //
    // Return the value itself when |Value| < 2^-54.
    //

    } else if (HighWord < EXP_MINUS_ONE_TINY_HIGH_WORD) {

        //
        // Return the value with inexact flags when the value is non-zero.
        //

        Approximation = ClHugeValue + Value;
        return Value - (Approximation - (ClHugeValue + Value));

    } else {
        Exponent = 0;
    }

    //
    // The value is now in the primary range.
    //

    HalfValue = 0.5 * Value;
    HalfSquared = Value * HalfValue;
    Polynomial = ClDoubleOne + HalfSquared *
                 (ClExpMinusOne1 + HalfSquared *
                  (ClExpMinusOne2 + HalfSquared *
                   (ClExpMinusOne3 + HalfSquared *
                    (ClExpMinusOne4 + HalfSquared * ClExpMinusOne5))));

    Approximation = 3.0 - Polynomial * HalfValue;
    Denominator = HalfSquared *
                  ((Polynomial - Approximation) /
                   (6.0 - Value * Approximation));

    if (Exponent == 0) {

        //
        // Correction is zero in this case.
        //

        return Value - (Value * Denominator - HalfSquared);

    } else {
        Parts.Ulong.High = DOUBLE_ONE_HIGH_WORD + (Exponent << ExponentShift);
        Parts.Ulong.Low = 0;
        TwoRaisedToExponent = Parts.Double;
        Denominator = (Value * (Denominator - Correction) - Correction);
        Denominator -= HalfSquared;
        if (Exponent == -1) {
            return 0.5 * (Value - Denominator) - 0.5;
        }

        if (Exponent == 1) {
            if (Value < -0.25) {
                return -2.0 * (Denominator - (Value + 0.5));

            } else {
                return ClDoubleOne + 2.0 * (Value - Denominator);
            }
        }

        if ((Exponent <= -2) || (Exponent > 56)) {
            FinalResult = ClDoubleOne - (Denominator - Value);
            if (Exponent == 1024) {
                FinalResult = FinalResult * 2.0 * 0x1p1023;

            } else {
                FinalResult = FinalResult * TwoRaisedToExponent;
            }

            return FinalResult - ClDoubleOne;
        }

        Approximation = ClDoubleOne;
        Parts.Double = Approximation;
        if (Exponent < 20) {

            //
            // Set the approximation to 1 - 2^-Exponent.
            //

            Parts.Ulong.High = DOUBLE_ONE_HIGH_WORD -
                               ((2 << ExponentShift) >> Exponent);

            Approximation = Parts.Double;
            FinalResult = Approximation - (Denominator - Value);
            FinalResult = FinalResult * TwoRaisedToExponent;

        } else {

            //
            // Set the approximation to 2^-Exponent.
            //

            Parts.Ulong.High = (DOUBLE_EXPONENT_BIAS - Exponent) <<
                               ExponentShift;

            Approximation = Parts.Double;
            FinalResult = Value - (Denominator + Approximation);
            FinalResult += ClDoubleOne;
            FinalResult = FinalResult * TwoRaisedToExponent;
        }
    }

    return FinalResult;
}

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

