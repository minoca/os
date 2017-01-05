/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pow.c

Abstract:

    This module implements the power function (pow).

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

#define SQUARE_ROOT_3_OVER_2_HIGH_VALUE 0x3988E
#define SQUARE_ROOT_3_HIGH_VALUE 0xBB67A
#define DOUBLE_1024_HIGH_WORD 0x40900000
#define DOUBLE_1075_HIGH_WORD 0x4090cc00
#define DOUBLE_NEGATIVE_1075_HIGH_WORD 0xc090cc00
#define DOUBLE_2_TO_31_HIGH_WORD 0x41e00000
#define DOUBLE_2_TO_64_HIGH_WORD 0x43f00000
#define POWER_BIG_HIGH_WORD 0x43400000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const double ClInverseLn2High = 1.44269502162933349609e+00;
const double ClInverseLn2Low = 1.92596299112661746887e-08;
const double ClTwo53 = 9007199254740992.0;

//
// Define the polynomial coefficients for (3/2) * (log(x) - 2s - 2/3 * s^3)
//

const double ClPowerLog1 = 5.99999999999994648725e-01;
const double ClPowerLog2 = 4.28571428578550184252e-01;
const double ClPowerLog3 = 3.33333329818377432918e-01;
const double ClPowerLog4 = 2.72728123808534006489e-01;
const double ClPowerLog5 = 2.30660745775561754067e-01;
const double ClPowerLog6 = 2.06975017800338417784e-01;

const double ClPower1 = 1.66666666666666019037e-01;
const double ClPower2 = -2.77777777770155933842e-03;
const double ClPower3 = 6.61375632143793436117e-05;
const double ClPower4 = -1.65339022054652515390e-06;
const double ClPower5 = 4.13813679705723846039e-08;

const double Cl2Over3Ln2 = 9.61796693925975554329e-01;
const double Cl2Over3Ln2High = 9.61796700954437255859e-01;
const double Cl2Over3Ln2Low = -7.02846165095275826516e-09;

const double ClLg2 = 6.93147180559945286227e-01;
const double ClLg2High = 6.93147182464599609375e-01;
const double ClLg2Low = -1.90465429995776804525e-09;

//
// The power overflow value is -(1024 - log2(Overflow + 0.5ULP)).
//

const double ClPowerOverflow = 8.0085662595372944372e-0017;
const double ClPowerDpHigh[] = {
    0.0,
    5.84962487220764160156e-01
};

const double ClPowerDpLow[] = {
    0.0,
    1.35003920212974897128e-08
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
pow (
    double Value,
    double Power
    )

/*++

Routine Description:

    This routine raises the given value to the given power.

Arguments:

    Value - Supplies the value to raise.

    Power - Supplies the power to raise the value to.

Return Value:

    Returns the given value raised to the given power.

--*/

{

    LONG AbsolutePowerHigh;
    LONG AbsoluteResultHigh;
    double AbsoluteValue;
    LONG AbsoluteValueHigh;
    double AppliedPolynomial;
    double Component1;
    double Component2;
    LONG Exponent;
    ULONG ExponentShift;
    double Input;
    LONG Interval;
    double Log;
    double LogAbsoluteValue;
    double LogAbsoluteValueTail;
    LONG NotSign;
    DOUBLE_PARTS Parts;
    double PowerHigh;
    LONG PowerHighWord;
    LONG PowerIntegerStatus;
    double PowerLog;
    double PowerLogTail;
    ULONG PowerLow;
    double PowerPolynomial;
    double Remainder;
    double Result;
    LONG ResultExponent;
    LONG ResultHigh;
    LONG ResultLow;
    double ScaledPowerLog;
    double ScaledPowerLogTail;
    double Sign;
    ULONG SignHighMask;
    double Sum;
    double SumTail;
    double Value2;
    LONG ValueHigh;
    ULONG ValueLow;
    double ValueQuotient;
    double ValueQuotient2;
    double ValueQuotientHigh;
    double ValueQuotientLow;

    //
    //                    n
    // Method:  Let x =  2  * (1 + f)
    // 1. Compute and return log2(x) in two pieces:
    //        log2(x) = w1 + w2,
    //    where w1 has 53-24 = 29 bit trailing zeros.
    //
    // 2. Perform y * log2(x) = n + y' by simulating muti-precision
    //    arithmetic, where |y'|<=0.5.
    // 3. Return x^y = 2^n * exp(y'*log2)
    //
    // Special cases:
    //    1.  (anything) ^ 0  is 1
    //    2.  (anything) ^ 1  is itself
    //    3.  (anything) ^ NAN is NAN
    //    4.  NAN ^ (anything except 0) is NAN
    //    5.  +-(|x| > 1) ^  +INF is +INF
    //    6.  +-(|x| > 1) ^  -INF is +0
    //    7.  +-(|x| < 1) ^  +INF is +0
    //    8.  +-(|x| < 1) ^  -INF is +INF
    //    9.  +-1         ^ +-INF is NAN
    //    10. +0 ^ (+anything except 0, NAN)               is +0
    //    11. -0 ^ (+anything except 0, NAN, odd integer)  is +0
    //    12. +0 ^ (-anything except 0, NAN)               is +INF
    //    13. -0 ^ (-anything except 0, NAN, odd integer)  is +INF
    //    14. -0 ^ (odd integer) = -( +0 ** (odd integer) )
    //    15. +INF ^ (+anything except 0,NAN) is +INF
    //    16. +INF ^ (-anything except 0,NAN) is +0
    //    17. -INF ^ (anything)  = -0 ** (-anything)
    //    18. (-anything) ^ (integer) is (-1)^(integer) * (+anything^integer)
    //    19. (-anything except 0 and inf) ^ (non-integer) is NAN
    //
    // Accuracy:
    //    pow(x,y) returns x**y nearly rounded. In particular
    //    pow(integer,integer)
    //    always returns the correct integer provided it is
    //    representable.
    //

    SignHighMask = ~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT;
    ExponentShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    Parts.Double = Value;
    ValueHigh = Parts.Ulong.High;
    AbsoluteValueHigh = ValueHigh &
                        (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    ValueLow = Parts.Ulong.Low;
    Parts.Double = Power;
    PowerHighWord = Parts.Ulong.High;
    AbsolutePowerHigh = PowerHighWord &
                        (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    PowerLow = Parts.Ulong.Low;

    //
    // Anything raised to zero is one.
    //

    if ((AbsolutePowerHigh | PowerLow) == 0) {
        return ClDoubleOne;
    }

    //
    // One raised to anything (even NaN) is one.
    //

    if ((ValueHigh == DOUBLE_ONE_HIGH_WORD) && (ValueLow == 0)) {
        return ClDoubleOne;
    }

    //
    // The power is not zero, so the result is NaN if either argument is NaN.
    //

    if ((AbsoluteValueHigh > NAN_HIGH_WORD) ||
        ((AbsoluteValueHigh == NAN_HIGH_WORD) && (ValueLow != 0)) ||
        (AbsolutePowerHigh > NAN_HIGH_WORD) ||
        ((AbsolutePowerHigh == NAN_HIGH_WORD) && (PowerLow != 0))) {

        return (Value + 0.0) + (Power + 0.0);
    }

    //
    // Determine if the power is an odd integer while the value is negative.
    // The power integer status is zero if it is not an integer, 1 if it is an
    // odd integer, and 2 if it is an even integer.
    //

    PowerIntegerStatus = 0;
    if (ValueHigh < 0) {
        if (AbsolutePowerHigh >= POWER_BIG_HIGH_WORD) {

            //
            // This is an even integer power.
            //

            PowerIntegerStatus = 2;

        } else if (AbsolutePowerHigh >= DOUBLE_ONE_HIGH_WORD) {
            Exponent = (AbsolutePowerHigh >> ExponentShift) -
                       DOUBLE_EXPONENT_BIAS;

            if (Exponent > ExponentShift) {
                ResultHigh = PowerLow >> (DOUBLE_EXPONENT_SHIFT - Exponent);
                if ((ResultHigh << (DOUBLE_EXPONENT_SHIFT - Exponent)) ==
                    PowerLow) {

                    PowerIntegerStatus = 2 - (ResultHigh & 0x1);
                }

            } else if (PowerLow == 0) {
                ResultHigh = AbsolutePowerHigh >> (ExponentShift - Exponent);
                if ((ResultHigh << (ExponentShift - Exponent)) ==
                    AbsolutePowerHigh) {

                    PowerIntegerStatus = 2 - (ResultHigh & 0x1);
                }
            }
        }
    }

    //
    // Check for special powers.
    //

    if (PowerLow == 0) {

        //
        // Handle an infinite power.
        //

        if (AbsolutePowerHigh == NAN_HIGH_WORD) {
            if (((AbsoluteValueHigh - DOUBLE_ONE_HIGH_WORD) | ValueLow) == 0) {

                //
                // (-1)^(+-Infinity) is 1.
                //

                return ClDoubleOne;

            //
            // (|Value| > 1)^(+-Infinity) is Infinity, 0.
            //

            } else if (AbsoluteValueHigh >= DOUBLE_ONE_HIGH_WORD) {
                if (PowerHighWord >= 0) {
                    return Power;
                }

                return ClDoubleZero;

            //
            // (|Value| < 1)^(+-Infinity) is Infinity, 0.
            //

            } else {
                if (PowerHighWord < 0) {
                    return -Power;
                }

                return ClDoubleZero;
            }
        }

        //
        // Handle a power of +/- 1.
        //

        if (AbsolutePowerHigh == DOUBLE_ONE_HIGH_WORD) {
            if (PowerHighWord < 0) {
                return ClDoubleOne / Value;
            }

            return Value;
        }

        //
        // Handle powers of 2, 3, 4, and 0.5.
        //

        if (PowerHighWord == DOUBLE_TWO_HIGH_WORD) {
            return Value * Value;
        }

        if (PowerHighWord == DOUBLE_THREE_HIGH_WORD) {
            return Value * Value * Value;
        }

        if (PowerHighWord == DOUBLE_FOUR_HIGH_WORD) {
            Value2 = Value * Value;
            return Value2 * Value2;
        }

        if (PowerHighWord == DOUBLE_ONE_HALF_HIGH_WORD) {
            if (ValueHigh >= 0) {
                return sqrt(Value);
            }
        }
    }

    AbsoluteValue = fabs(Value);

    //
    // Work through some special base values.
    //

    if (ValueLow == 0) {

        //
        // Deal with the value being +/- 0, +/- Infinity, and +/- 1.
        //

        if ((AbsoluteValueHigh == NAN_HIGH_WORD) ||
            (AbsoluteValueHigh == 0) ||
            (AbsoluteValueHigh == DOUBLE_ONE_HIGH_WORD)) {

            Result = AbsoluteValue;
            if (PowerHighWord < 0) {
                Result = ClDoubleOne / Result;
            }

            if (ValueHigh < 0) {
                if (((AbsoluteValueHigh - DOUBLE_ONE_HIGH_WORD) |
                     PowerIntegerStatus) == 0) {

                    //
                    // (-1)^Non-Integer is NaN.
                    //

                    Result = (Result - Result) / (Result - Result);

                } else if (PowerIntegerStatus == 1) {

                    //
                    // (Value < 0)^Odd = -(|Value|^odd).
                    //

                    Result = -Result;
                }
            }

            return Result;
        }
    }

    NotSign = ((ULONG)ValueHigh >>
               (DOUBLE_SIGN_BIT_SHIFT - DOUBLE_HIGH_WORD_SHIFT)) - 1;

    //
    // (Value < 0)^Non-Integer is NaN.
    //

    if ((NotSign | PowerIntegerStatus) == 0) {
        return (Value - Value) / (Value - Value);
    }

    Sign = ClDoubleOne;
    if ((NotSign | (PowerIntegerStatus - 1)) == 0) {
        Sign = -ClDoubleOne;
    }

    //
    // Check for a huge absolute value of power.
    // Handle a power greater than 2^31.
    //

    if (AbsolutePowerHigh > DOUBLE_2_TO_31_HIGH_WORD) {

        //
        // A power greater than 2^64 must over/underflow.
        //

        if (AbsolutePowerHigh > DOUBLE_2_TO_64_HIGH_WORD) {
            if (AbsoluteValueHigh < DOUBLE_ONE_HIGH_WORD) {
                if (PowerHighWord < 0) {
                    return ClDoubleHugeValue * ClDoubleHugeValue;

                } else {
                    return ClDoubleTinyValue * ClDoubleTinyValue;
                }
            }

            if (AbsoluteValueHigh >= DOUBLE_ONE_HIGH_WORD) {
                if (PowerHighWord > 0) {
                    return ClDoubleHugeValue * ClDoubleHugeValue;

                } else {
                    return ClDoubleTinyValue * ClDoubleTinyValue;
                }
            }
        }

        //
        // Over/underflow if the value is not close to one.
        //

        if (AbsoluteValueHigh < DOUBLE_ONE_HIGH_WORD - 1) {
            if (PowerHighWord < 0) {
                return Sign * ClDoubleHugeValue * ClDoubleHugeValue;

            } else {
                return Sign * ClDoubleTinyValue * ClDoubleTinyValue;
            }
        }

        if (AbsoluteValueHigh > DOUBLE_ONE_HIGH_WORD) {
            if (PowerHighWord > 0) {
                return Sign * ClDoubleHugeValue * ClDoubleHugeValue;

            } else {
                return Sign * ClDoubleTinyValue * ClDoubleTinyValue;
            }
        }

        //
        // |1 - Value| is less than or equal to 2^-20, approximate log(Value)
        // with Value - Value^2 / 2 + Value^3 / 3 - Value^4 / 4.
        // Input still has 20 trailing zeros.
        //

        Input = AbsoluteValue - ClDoubleOne;
        Log = (Input * Input) *
              (0.5 - Input * (0.3333333333333333333333 - Input * 0.25));

        Component1 = ClInverseLn2High * Input;
        Component2 = Input * ClInverseLn2Low - Log * ClInverseLn2;
        LogAbsoluteValue = Component1 + Component2;
        Parts.Double = LogAbsoluteValue;
        Parts.Ulong.Low = 0;
        LogAbsoluteValue = Parts.Double;
        LogAbsoluteValueTail = Component2 - (LogAbsoluteValue - Component1);

    } else {
        ResultExponent = 0;

        //
        // Handle subnormal values.
        //

        if (AbsoluteValueHigh < (1 << ExponentShift)) {
            AbsoluteValue *= ClTwo53;
            ResultExponent -= 53;

            //
            // Refresh the absolute value high variable.
            //

            Parts.Double = AbsoluteValue;
            AbsoluteValueHigh = Parts.Ulong.High;
        }

        ResultExponent += ((AbsoluteValueHigh) >> 20) - DOUBLE_EXPONENT_BIAS;
        ResultHigh = AbsoluteValueHigh & DOUBLE_HIGH_VALUE_MASK;

        //
        // Determine the interval. Normalize the high word of the absolute
        // value.
        //

        AbsoluteValueHigh = ResultHigh | DOUBLE_ONE_HIGH_WORD;
        if (ResultHigh <= SQUARE_ROOT_3_OVER_2_HIGH_VALUE) {
            Interval = 0;

        } else if (ResultHigh < SQUARE_ROOT_3_HIGH_VALUE) {
            Interval = 1;

        } else {
            Interval = 0;
            ResultExponent += 1;
            AbsoluteValueHigh -=
                         1 << (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);
        }

        //
        // Set the high word.
        //

        Parts.Double = AbsoluteValue;
        Parts.Ulong.High = AbsoluteValueHigh;
        AbsoluteValue = Parts.Double;

        //
        // Compute ValueQuotient = ValueQuotientHigh + ValueQuotientLow
        //                       = (Value - 1) / (Value + 1) or
        //                         (Value - 1.5) / (Value + 1.5)
        //

        if (Interval == 0) {
            Component1 = AbsoluteValue - 1.0;
            Component2 = ClDoubleOne / (AbsoluteValue + 1.0);

        } else {
            Component1 = AbsoluteValue - 1.5;
            Component2 = ClDoubleOne / (AbsoluteValue + 1.5);
        }

        ValueQuotient = Component1 * Component2;
        ValueQuotientHigh = ValueQuotient;

        //
        // Set the low word to zero.
        //

        Parts.Double = ValueQuotientHigh;
        Parts.Ulong.Low = 0;
        ValueQuotientHigh = Parts.Double;

        //
        // Sum = AbsoluteValue + (1 or 1.5) High.
        //

        Parts.Double = ClDoubleZero;
        Parts.Ulong.High = ((AbsoluteValueHigh >> 1) | 0x20000000) +
                            0x00080000 + (Interval << 18);

        Sum = Parts.Double;
        if (Interval == 0) {
            SumTail = AbsoluteValue - (Sum - 1.0);

        } else {
            SumTail = AbsoluteValue - (Sum - 1.5);
        }

        ValueQuotientLow = Component2 *
                           ((Component1 - ValueQuotientHigh * Sum) -
                            ValueQuotientHigh * SumTail);

        //
        // Compute log(AbsoluteValue).
        //

        ValueQuotient2 = ValueQuotient * ValueQuotient;
        Log = ValueQuotient2 * ValueQuotient2 *
              (ClPowerLog1 + ValueQuotient2 *
               (ClPowerLog2 + ValueQuotient2 *
                (ClPowerLog3 + ValueQuotient2 *
                 (ClPowerLog4 + ValueQuotient2 *
                  (ClPowerLog5 + ValueQuotient2 * ClPowerLog6)))));

        Log += ValueQuotientLow * (ValueQuotientHigh + ValueQuotient);
        ValueQuotient2 = ValueQuotientHigh * ValueQuotientHigh;
        Sum = 3.0 + ValueQuotient2 + Log;
        Parts.Double = Sum;
        Parts.Ulong.Low = 0;
        Sum = Parts.Double;
        SumTail = Log - ((Sum - 3.0) - ValueQuotient2);

        //
        // Component1 + Component2 = ValueQuotient * (1 + ...).
        //

        Component1 = ValueQuotientHigh * Sum;
        Component2 = ValueQuotientLow * Sum + SumTail * ValueQuotient;

        //
        // 2 / (3log2) * (ValueQuotient + ...).
        //

        PowerLog = Component1 + Component2;
        Parts.Double = PowerLog;
        Parts.Ulong.Low = 0;
        PowerLog = Parts.Double;
        PowerLogTail = Component2 - (PowerLog - Component1);

        //
        // cp_h + cp_l = 2 / (3log2).
        //

        ScaledPowerLog = Cl2Over3Ln2High * PowerLog;
        ScaledPowerLogTail = Cl2Over3Ln2Low * PowerLog +
              PowerLogTail * Cl2Over3Ln2 + ClPowerDpLow[Interval];

        //
        // log2(AbsoluteValue) = (ValueQuotient + ...) * 2 / (3log2)
        //                     = ResultExponent + dp_h + ScaledPowerLog + Tail.
        //

        Input = (double)ResultExponent;
        LogAbsoluteValue = (((ScaledPowerLog + ScaledPowerLogTail) +
                             ClPowerDpHigh[Interval]) + Input);

        Parts.Double = LogAbsoluteValue;
        Parts.Ulong.Low = 0;
        LogAbsoluteValue = Parts.Double;
        LogAbsoluteValueTail = ScaledPowerLogTail -
                               (((LogAbsoluteValue - Input) -
                                 ClPowerDpHigh[Interval]) - ScaledPowerLog);
    }

    //
    // Split up the power into y1 + y2 and compute
    // (y1 + y2) * (LogAbsoluteValue + LogAbsoluteValueTail).
    //

    Parts.Double = Power;
    Parts.Ulong.Low = 0;
    PowerHigh = Parts.Double;
    PowerLogTail = (Power - PowerHigh) * LogAbsoluteValue +
                   Power * LogAbsoluteValueTail;

    PowerLog = PowerHigh * LogAbsoluteValue;
    Result = PowerLogTail + PowerLog;
    Parts.Double = Result;
    ResultHigh = Parts.Ulong.High;
    ResultLow = Parts.Ulong.Low;

    //
    // Return an overflow if the exponent became too big.
    //

    if (ResultHigh >= DOUBLE_1024_HIGH_WORD) {
        if (((ResultHigh - DOUBLE_1024_HIGH_WORD) | ResultLow) != 0) {
            return Sign * ClDoubleHugeValue * ClDoubleHugeValue;

        } else {
            if (PowerLogTail + ClPowerOverflow > Result - PowerLog) {
                return Sign * ClDoubleHugeValue * ClDoubleHugeValue;
            }
        }

    //
    // Return an underflow if the exponent became too small.
    //

    } else if ((ResultHigh & SignHighMask) >= DOUBLE_1075_HIGH_WORD ) {
        if (((ResultHigh - DOUBLE_NEGATIVE_1075_HIGH_WORD) | ResultLow) != 0) {
            return Sign * ClDoubleTinyValue * ClDoubleTinyValue;

        } else {
            if (PowerLogTail <= Result - PowerLog) {
                return Sign * ClDoubleTinyValue * ClDoubleTinyValue;
            }
        }
    }

    //
    // Compute 2^(PowerLog + PowerLogTail)
    //

    AbsoluteResultHigh = ResultHigh & SignHighMask;
    Exponent = (AbsoluteResultHigh >> ExponentShift) - DOUBLE_EXPONENT_BIAS;
    ResultExponent = 0;

    //
    // If |Result| > 0.5, set ResultExponent = [Result + 0.5].
    //

    if (AbsoluteResultHigh > DOUBLE_ONE_HALF_HIGH_WORD) {
        ResultExponent = ResultHigh + ((1 << ExponentShift) >> (Exponent + 1));

        //
        // Set a new exponent for ResultExponent.
        //

        Exponent = ((ResultExponent & SignHighMask) >> ExponentShift) -
                   DOUBLE_EXPONENT_BIAS;

        Parts.Double = ClDoubleZero;
        Parts.Ulong.High = ResultExponent & ~(DOUBLE_HIGH_VALUE_MASK >>
                                              Exponent);

        Input = Parts.Double;
        ResultExponent = ((ResultExponent & DOUBLE_HIGH_VALUE_MASK) |
                          (1 << ExponentShift)) >> (ExponentShift - Exponent);

        if (ResultHigh < 0) {
            ResultExponent = -ResultExponent;
        }

        PowerLog -= Input;
    }

    Input = PowerLogTail + PowerLog;
    Parts.Double = Input;
    Parts.Ulong.Low = 0;
    Input = Parts.Double;
    Component1 = Input * ClLg2High;
    Component2 = (PowerLogTail - (Input - PowerLog)) * ClLg2 + Input * ClLg2Low;
    Result = Component1 + Component2;
    Remainder = Component2 - (Result - Component1);
    Input = Result * Result;
    PowerPolynomial = Result - Input * (ClPower1 + Input *
                                        (ClPower2 + Input *
                                         (ClPower3 + Input *
                                          (ClPower4 + Input * ClPower5))));

    AppliedPolynomial = (Result * PowerPolynomial) / (PowerPolynomial - 2.0) -
                        (Remainder + Result * Remainder);

    Result = ClDoubleOne - (AppliedPolynomial - Result);
    Parts.Double = Result;
    ResultHigh = Parts.Ulong.High;
    ResultHigh += (ResultExponent << ExponentShift);
    if ((ResultHigh >> ExponentShift) <= 0) {

        //
        // This is a subnormal output.
        //

        Result = scalbn(Result, ResultExponent);

    } else {
        Parts.Ulong.High = ResultHigh;
        Result = Parts.Double;
    }

    return Sign * Result;
}

//
// --------------------------------------------------------- Internal Functions
//

