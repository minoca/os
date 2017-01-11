/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    powf.c

Abstract:

    This module implements the power function (pow).

    Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.

    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Chris Stevens 3-Jan-2017

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

#define SQUARE_ROOT_3_OVER_2_VALUE 0x1cc471
#define SQUARE_ROOT_3_VALUE 0x5db3d7
#define FLOAT_128_WORD 0x43000000
#define FLOAT_150_WORD 0x43160000
#define FLOAT_NEGATIVE_150_WORD 0xc3160000
#define FLOAT_2_TO_27_WORD 0x4d000000
#define POWER_BIG_WORD 0x4b800000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const float ClFloatInverseLn2High = 1.4426879883e+00;
const float ClFloatInverseLn2Low = 7.0526075433e-06;
const float ClFloatTwo24 = 16777216.0;

//
// Define the polynomial coefficients for (3/2) * (log(x) - 2s - 2/3 * s^3)
//

const float ClFloatPowerLog1 = 6.0000002384e-01;
const float ClFloatPowerLog2 = 4.2857143283e-01;
const float ClFloatPowerLog3 = 3.3333334327e-01;
const float ClFloatPowerLog4 = 2.7272811532e-01;
const float ClFloatPowerLog5 = 2.3066075146e-01;
const float ClFloatPowerLog6 = 2.0697501302e-01;

const float ClFloatPower1 = 1.6666667163e-01;
const float ClFloatPower2 = -2.7777778450e-03;
const float ClFloatPower3 = 6.6137559770e-05;
const float ClFloatPower4 = -1.6533901999e-06;
const float ClFloatPower5 = 4.1381369442e-08;

const float ClFloat2Over3Ln2 = 9.6179670095e-01;
const float ClFloat2Over3Ln2High = 9.6191406250e-01;
const float ClFloat2Over3Ln2Low = -1.1736857402e-04;

const float ClFloatLg2 = 6.9314718246e-01;
const float ClFloatLg2High = 6.93145752e-01;
const float ClFloatLg2Low = 1.42860654e-06;

//
// The power overflow value is -(128 - log2(Overflow + 0.5ULP)).
//

const float ClFloatPowerOverflow = 4.2995665694e-08;
const float ClFloatPowerDpHigh[] = {
    0.0,
    5.84960938e-01
};

const float ClFloatPowerDpLow[] = {
    0.0,
    1.56322085e-06
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
powf (
    float Value,
    float Power
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

    LONG AbsolutePowerWord;
    LONG AbsoluteResultWord;
    float AbsoluteValue;
    LONG AbsoluteValueWord;
    float AppliedPolynomial;
    float Component1;
    float Component2;
    LONG Exponent;
    float Input;
    LONG Interval;
    float Log;
    float LogAbsoluteValue;
    float LogAbsoluteValueTail;
    LONG NotSign;
    FLOAT_PARTS Parts;
    float PowerHigh;
    LONG PowerIntegerStatus;
    float PowerLog;
    float PowerLogTail;
    float PowerPolynomial;
    LONG PowerWord;
    float Remainder;
    float Result;
    LONG ResultExponent;
    LONG ResultWord;
    float ScaledPowerLog;
    float ScaledPowerLogTail;
    float Sign;
    float Sum;
    float SumTail;
    float ValueQuotient;
    float ValueQuotient2;
    float ValueQuotientHigh;
    float ValueQuotientLow;
    LONG ValueWord;

    Parts.Float = Value;
    ValueWord = Parts.Ulong;
    AbsoluteValueWord = ValueWord & ~FLOAT_SIGN_BIT;
    Parts.Float = Power;
    PowerWord = Parts.Ulong;
    AbsolutePowerWord = PowerWord & ~FLOAT_SIGN_BIT;

    //
    // Anything raised to zero is one.
    //

    if (AbsolutePowerWord == 0) {
        return ClFloatOne;
    }

    //
    // One raised to anything (even NaN) is one.
    //

    if (ValueWord == FLOAT_ONE_WORD) {
        return ClFloatOne;
    }

    //
    // The power is not zero, so the result is NaN if either argument is NaN.
    //

    if ((AbsoluteValueWord > FLOAT_NAN) || (AbsolutePowerWord > FLOAT_NAN)) {
        return (Value + (float)0.0) + (Power + (float)0.0F);
    }

    //
    // Determine if the power is an odd integer while the value is negative.
    // The power integer status is zero if it is not an integer, 1 if it is an
    // odd integer, and 2 if it is an even integer.
    //

    PowerIntegerStatus = 0;
    if (ValueWord < 0) {
        if (AbsolutePowerWord >= POWER_BIG_WORD) {

            //
            // This is an even integer power.
            //

            PowerIntegerStatus = 2;

        } else if (AbsolutePowerWord >= FLOAT_ONE_WORD) {
            Exponent = (AbsolutePowerWord >> FLOAT_EXPONENT_SHIFT) -
                       FLOAT_EXPONENT_BIAS;

            ResultWord = AbsolutePowerWord >> (FLOAT_EXPONENT_SHIFT - Exponent);
            if ((ResultWord << (FLOAT_EXPONENT_SHIFT - Exponent)) ==
                AbsolutePowerWord) {

                PowerIntegerStatus = 2 - (ResultWord & 0x1);
            }
        }
    }

    //
    // Handle an infinite power.
    //

    if (AbsolutePowerWord == FLOAT_NAN) {
        if (AbsoluteValueWord == FLOAT_ONE_WORD) {

            //
            // (-1)^(+-Infinity) is 1.
            //

            return ClFloatOne;

        //
        // (|Value| > 1)^(+-Infinity) is Infinity, 0.
        //

        } else if (AbsoluteValueWord > FLOAT_ONE_WORD) {
            if (PowerWord >= 0) {
                return Power;
            }

            return ClFloatZero;

        //
        // (|Value| < 1)^(+-Infinity) is Infinity, 0.
        //

        } else {
            if (PowerWord < 0) {
                return -Power;
            }

            return ClFloatZero;
        }
    }

    //
    // Handle a power of +/- 1.
    //

    if (AbsolutePowerWord == FLOAT_ONE_WORD) {
        if (PowerWord < 0) {
            return ClFloatOne / Value;
        }

        return Value;
    }

    //
    // Handle powers of 2 and 0.5.
    //

    if (PowerWord == FLOAT_TWO_WORD) {
        return Value * Value;
    }

    if (PowerWord == FLOAT_ONE_HALF_WORD) {
        if (ValueWord >= 0) {
            return sqrtf(Value);
        }
    }

    AbsoluteValue = fabsf(Value);

    //
    // Deal with the value being +/- 0, +/- Infinity, and +/- 1.
    //

    if ((AbsoluteValueWord == FLOAT_NAN) ||
        (AbsoluteValueWord == 0) ||
        (AbsoluteValueWord == FLOAT_ONE_WORD)) {

        Result = AbsoluteValue;
        if (PowerWord < 0) {
            Result = ClFloatOne / Result;
        }

        if (ValueWord < 0) {
            if (((AbsoluteValueWord - FLOAT_ONE_WORD) |
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

    NotSign = ((ULONG)ValueWord >> FLOAT_SIGN_BIT_SHIFT) - 1;

    //
    // (Value < 0)^Non-Integer is NaN.
    //

    if ((NotSign | PowerIntegerStatus) == 0) {
        return (Value - Value) / (Value - Value);
    }

    Sign = ClFloatOne;
    if ((NotSign | (PowerIntegerStatus - 1)) == 0) {
        Sign = -ClFloatOne;
    }

    //
    // Check for a huge absolute value of power. Handle a power greater than
    // 2^27.
    //

    if (AbsolutePowerWord > FLOAT_2_TO_27_WORD) {

        //
        // Over/underflow if the value is not close to one.
        //

        if (AbsoluteValueWord < (FLOAT_ONE_WORD - 8)) {
            if (PowerWord < 0) {
                return Sign * ClFloatHugeValue * ClFloatHugeValue;

            } else {
                return Sign * ClFloatTinyValue * ClFloatTinyValue;
            }
        }

        if (AbsoluteValueWord > (FLOAT_ONE_WORD + 7)) {
            if (PowerWord > 0) {
                return Sign * ClFloatHugeValue * ClFloatHugeValue;

            } else {
                return Sign * ClFloatTinyValue * ClFloatTinyValue;
            }
        }

        //
        // |1 - Value| is less than or equal to 2^-20, approximate log(Value)
        // with Value - Value^2 / 2 + Value^3 / 3 - Value^4 / 4.
        // Input still has 20 trailing zeros.
        //

        Input = AbsoluteValue - 1;
        Log = (Input * Input) *
              ((float)0.5 - Input *
               ((float)0.333333333333 - Input * (float)0.25));

        Component1 = ClFloatInverseLn2High * Input;
        Component2 = Input * ClFloatInverseLn2Low - Log * ClFloatInverseLn2;
        LogAbsoluteValue = Component1 + Component2;
        Parts.Float = LogAbsoluteValue;
        Parts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
        LogAbsoluteValue = Parts.Float;
        LogAbsoluteValueTail = Component2 - (LogAbsoluteValue - Component1);

    } else {
        ResultExponent = 0;

        //
        // Handle subnormal values.
        //

        if (AbsoluteValueWord < (1 << FLOAT_EXPONENT_SHIFT)) {
            AbsoluteValue *= ClFloatTwo24;
            ResultExponent -= 24;

            //
            // Refresh the absolute value variable.
            //

            Parts.Float = AbsoluteValue;
            AbsoluteValueWord = Parts.Ulong;
        }

        ResultExponent += ((AbsoluteValueWord) >> FLOAT_EXPONENT_SHIFT) -
                          FLOAT_EXPONENT_BIAS;

        ResultWord = AbsoluteValueWord & FLOAT_VALUE_MASK;

        //
        // Determine the interval. Normalize the high word of the absolute
        // value.
        //

        AbsoluteValueWord = ResultWord | FLOAT_ONE_WORD;
        if (ResultWord <= SQUARE_ROOT_3_OVER_2_VALUE) {
            Interval = 0;

        } else if (ResultWord < SQUARE_ROOT_3_VALUE) {
            Interval = 1;

        } else {
            Interval = 0;
            ResultExponent += 1;
            AbsoluteValueWord -= (1 << FLOAT_EXPONENT_SHIFT);
        }

        //
        // Set the word.
        //

        Parts.Ulong = AbsoluteValueWord;
        AbsoluteValue = Parts.Float;

        //
        // Compute ValueQuotient = ValueQuotientHigh + ValueQuotientLow
        //                       = (Value - 1) / (Value + 1) or
        //                         (Value - 1.5) / (Value + 1.5)
        //

        if (Interval == 0) {
            Component1 = AbsoluteValue - (float)1.0;
            Component2 = ClFloatOne / (AbsoluteValue + (float)1.0);

        } else {
            Component1 = AbsoluteValue - (float)1.5;
            Component2 = ClFloatOne / (AbsoluteValue + (float)1.5);
        }

        ValueQuotient = Component1 * Component2;
        ValueQuotientHigh = ValueQuotient;

        //
        // Set mask off the low bits.
        //

        Parts.Float = ValueQuotientHigh;
        Parts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
        ValueQuotientHigh = Parts.Float;

        //
        // Sum = AbsoluteValue + (1 or 1.5) High.
        //

        Parts.Ulong = (((AbsoluteValueWord >> 1) & 0xFFFFF000) | 0x20000000) +
                      0x00400000 + (Interval << 21);

        Sum = Parts.Float;
        if (Interval == 0) {
            SumTail = AbsoluteValue - (Sum - (float)1.0);

        } else {
            SumTail = AbsoluteValue - (Sum - (float)1.5);
        }

        ValueQuotientLow = Component2 *
                           ((Component1 - ValueQuotientHigh * Sum) -
                            ValueQuotientHigh * SumTail);

        //
        // Compute log(AbsoluteValue).
        //

        ValueQuotient2 = ValueQuotient * ValueQuotient;
        Log = ValueQuotient2 * ValueQuotient2 *
              (ClFloatPowerLog1 + ValueQuotient2 *
               (ClFloatPowerLog2 + ValueQuotient2 *
                (ClFloatPowerLog3 + ValueQuotient2 *
                 (ClFloatPowerLog4 + ValueQuotient2 *
                  (ClFloatPowerLog5 + ValueQuotient2 * ClFloatPowerLog6)))));

        Log += ValueQuotientLow * (ValueQuotientHigh + ValueQuotient);
        ValueQuotient2 = ValueQuotientHigh * ValueQuotientHigh;
        Sum = (float)3.0 + ValueQuotient2 + Log;
        Parts.Float = Sum;
        Parts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
        Sum = Parts.Float;
        SumTail = Log - ((Sum - (float)3.0) - ValueQuotient2);

        //
        // Component1 + Component2 = ValueQuotient * (1 + ...).
        //

        Component1 = ValueQuotientHigh * Sum;
        Component2 = ValueQuotientLow * Sum + SumTail * ValueQuotient;

        //
        // 2 / (3log2) * (ValueQuotient + ...).
        //

        PowerLog = Component1 + Component2;
        Parts.Float = PowerLog;
        Parts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
        PowerLog = Parts.Float;
        PowerLogTail = Component2 - (PowerLog - Component1);

        //
        // cp_h + cp_l = 2 / (3log2).
        //

        ScaledPowerLog = ClFloat2Over3Ln2High * PowerLog;
        ScaledPowerLogTail = ClFloat2Over3Ln2Low * PowerLog +
                             PowerLogTail * ClFloat2Over3Ln2 +
                             ClFloatPowerDpLow[Interval];

        //
        // log2(AbsoluteValue) = (ValueQuotient + ...) * 2 / (3log2)
        //                     = ResultExponent + dp_h + ScaledPowerLog + Tail.
        //

        Input = (float)ResultExponent;
        LogAbsoluteValue = (((ScaledPowerLog + ScaledPowerLogTail) +
                             ClFloatPowerDpHigh[Interval]) + Input);

        Parts.Float = LogAbsoluteValue;
        Parts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
        LogAbsoluteValue = Parts.Float;
        LogAbsoluteValueTail = ScaledPowerLogTail -
                               (((LogAbsoluteValue - Input) -
                                 ClFloatPowerDpHigh[Interval]) -
                                ScaledPowerLog);
    }

    //
    // Split up the power into y1 + y2 and compute
    // (y1 + y2) * (LogAbsoluteValue + LogAbsoluteValueTail).
    //

    Parts.Float = Power;
    Parts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
    PowerHigh = Parts.Float;
    PowerLogTail = (Power - PowerHigh) * LogAbsoluteValue +
                   Power * LogAbsoluteValueTail;

    PowerLog = PowerHigh * LogAbsoluteValue;
    Result = PowerLogTail + PowerLog;
    Parts.Float = Result;
    ResultWord = Parts.Ulong;

    //
    // Return an overflow if the exponent became too big.
    //

    if (ResultWord > FLOAT_128_WORD) {
        return Sign * ClFloatHugeValue * ClFloatHugeValue;

    } else if (ResultWord == FLOAT_128_WORD) {
        if ((PowerLogTail + ClFloatPowerOverflow) > (Result - PowerLog)) {
            return Sign * ClFloatHugeValue * ClFloatHugeValue;
        }

    //
    // Return an underflow if the exponent became too small.
    //

    } else if ((ResultWord & ~FLOAT_SIGN_BIT) > FLOAT_150_WORD) {
        return Sign * ClFloatTinyValue * ClFloatTinyValue;

    } else if (ResultWord == FLOAT_NEGATIVE_150_WORD) {
        if (PowerLogTail <= Result - PowerLog) {
            return Sign * ClFloatTinyValue * ClFloatTinyValue;
        }
    }

    //
    // Compute 2^(PowerLog + PowerLogTail)
    //

    AbsoluteResultWord = ResultWord & ~FLOAT_SIGN_BIT;
    Exponent = (AbsoluteResultWord >> FLOAT_EXPONENT_SHIFT) -
               FLOAT_EXPONENT_BIAS;

    ResultExponent = 0;

    //
    // If |Result| > 0.5, set ResultExponent = [Result + 0.5].
    //

    if (AbsoluteResultWord > FLOAT_ONE_HALF_WORD) {
        ResultExponent = ResultWord +
                         ((1 << FLOAT_EXPONENT_SHIFT) >> (Exponent + 1));

        //
        // Set a new exponent for ResultExponent.
        //

        Exponent = ((ResultExponent & ~FLOAT_SIGN_BIT) >>
                    FLOAT_EXPONENT_SHIFT) -
                   FLOAT_EXPONENT_BIAS;

        Parts.Ulong = ResultExponent & ~(FLOAT_VALUE_MASK >> Exponent);
        Input = Parts.Float;
        ResultExponent = ((ResultExponent & FLOAT_VALUE_MASK) |
                          (1 << FLOAT_EXPONENT_SHIFT)) >>
                         (FLOAT_EXPONENT_SHIFT - Exponent);

        if (ResultWord < 0) {
            ResultExponent = -ResultExponent;
        }

        PowerLog -= Input;
    }

    Input = PowerLogTail + PowerLog;
    Parts.Float = Input;
    Parts.Ulong &= 0xFFFF8000;
    Input = Parts.Float;
    Component1 = Input * ClFloatLg2High;
    Component2 = (PowerLogTail - (Input - PowerLog)) * ClFloatLg2 +
                 Input * ClFloatLg2Low;

    Result = Component1 + Component2;
    Remainder = Component2 - (Result - Component1);
    Input = Result * Result;
    PowerPolynomial = Result -
                      Input * (ClFloatPower1 + Input *
                               (ClFloatPower2 + Input *
                                (ClFloatPower3 + Input *
                                 (ClFloatPower4 + Input * ClFloatPower5))));

    AppliedPolynomial = (Result * PowerPolynomial) /
                        (PowerPolynomial - (float)2.0) -
                        (Remainder + Result * Remainder);

    Result = ClFloatOne - (AppliedPolynomial - Result);
    Parts.Float = Result;
    ResultWord = Parts.Ulong;
    ResultWord += (ResultExponent << FLOAT_EXPONENT_SHIFT);
    if ((ResultWord >> FLOAT_EXPONENT_SHIFT) <= 0) {

        //
        // This is a subnormal output.
        //

        Result = scalbnf(Result, ResultExponent);

    } else {
        Parts.Ulong = ResultWord;
        Result = Parts.Float;
    }

    return Sign * Result;
}

//
// --------------------------------------------------------- Internal Functions
//

