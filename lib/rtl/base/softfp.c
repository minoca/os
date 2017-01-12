/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    softfp.c

Abstract:

    This module implements support for software based floating point operations.

    This implementation is base on John R. Hauser's SoftFloat package, version
    2b.

Author:

    Evan Green 25-Jul-2013

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"
#include "softfp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

float
RtlpFloatAdd (
    FLOAT_PARTS Value1,
    FLOAT_PARTS Value2,
    CHAR Sign
    );

float
RtlpFloatSubtract (
    FLOAT_PARTS Value1,
    FLOAT_PARTS Value2,
    CHAR Sign
    );

double
RtlpDoubleAdd (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2,
    CHAR Sign
    );

double
RtlpDoubleSubtract (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2,
    CHAR Sign
    );

VOID
RtlpMultiply64To128 (
    ULONGLONG Value1,
    ULONGLONG Value2,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    );

ULONGLONG
RtlpEstimateDivide128To64 (
    ULONGLONG DividendHigh,
    ULONGLONG DividendLow,
    ULONGLONG Divisor
    );

ULONGLONG
RtlpEstimateSquareRoot32 (
    SHORT ValueExponent,
    ULONG Value
    );

VOID
RtlpAdd128 (
    ULONGLONG Value1High,
    ULONGLONG Value1Low,
    ULONGLONG Value2High,
    ULONGLONG Value2Low,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    );

VOID
RtlpSubtract128 (
    ULONGLONG Value1High,
    ULONGLONG Value1Low,
    ULONGLONG Value2High,
    ULONGLONG Value2Low,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    );

float
RtlpFloatPropagateNan (
    FLOAT_PARTS Value1,
    FLOAT_PARTS Value2
    );

double
RtlpDoublePropagateNan (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2
    );

VOID
RtlpNormalizeDoubleSubnormal (
    ULONGLONG Significand,
    PSHORT Exponent,
    PULONGLONG Result
    );

VOID
RtlpNormalizeFloatSubnormal (
    ULONG Significand,
    PSHORT Exponent,
    PULONG Result
    );

COMMON_NAN
RtlpDoubleToCommonNan (
    DOUBLE_PARTS Value
    );

COMMON_NAN
RtlpFloatToCommonNan (
    FLOAT_PARTS Value
    );

float
RtlpCommonNanToFloat (
    COMMON_NAN Nan
    );

double
RtlpCommonNanToDouble (
    COMMON_NAN Nan
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define constants used for estimating the square root.
//

const USHORT RtlSquareRootOddAdjustments[] = {
    0x0004, 0x0022, 0x005D, 0x00B1, 0x011D, 0x019F, 0x0236, 0x02E0,
    0x039C, 0x0468, 0x0545, 0x0631, 0x072B, 0x0832, 0x0946, 0x0A67
};

const USHORT RtlSquareRootEvenAdjustments[] = {
    0x0A2D, 0x08AF, 0x075A, 0x0629, 0x051A, 0x0429, 0x0356, 0x029E,
    0x0200, 0x0179, 0x0109, 0x00AF, 0x0068, 0x0034, 0x0012, 0x0002
};

//
// ------------------------------------------------------------------ Functions
//

RTL_API
BOOL
RtlFloatIsNan (
    float Value
    )

/*++

Routine Description:

    This routine determines if the given value is Not a Number.

Arguments:

    Value - Supplies the floating point value to query.

Return Value:

    TRUE if the given value is Not a Number.

    FALSE otherwise.

--*/

{

    FLOAT_PARTS Parts;

    Parts.Float = Value;
    if (FLOAT_GET_EXPONENT(Parts) == FLOAT_NAN_EXPONENT) {
        return TRUE;
    }

    return FALSE;
}

RTL_API
double
RtlFloatConvertToDouble (
    float Float
    )

/*++

Routine Description:

    This routine converts the given float into a double.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the double equivalent.

--*/

{

    DOUBLE_PARTS DoubleParts;
    SHORT Exponent;
    FLOAT_PARTS FloatParts;
    CHAR Sign;
    ULONG Significand;

    FloatParts.Float = Float;
    Significand = FLOAT_GET_SIGNIFICAND(FloatParts);
    Exponent = FLOAT_GET_EXPONENT(FloatParts);
    Sign = FLOAT_GET_SIGN(FloatParts);
    if (Exponent == FLOAT_NAN_EXPONENT) {
        if (Significand != 0) {
            return RtlpCommonNanToDouble(RtlpFloatToCommonNan(FloatParts));
        }

        DoubleParts.Ulonglong = DOUBLE_PACK(Sign, DOUBLE_NAN_EXPONENT, 0);
        return DoubleParts.Double;
    }

    if (Exponent == 0) {
        if (Significand == 0) {
            DoubleParts.Ulonglong = DOUBLE_PACK(Sign, 0, 0);
            return DoubleParts.Double;
        }

        RtlpNormalizeFloatSubnormal(Significand,
                                    &Exponent,
                                    &Significand);

        Exponent -= 1;
    }

    DoubleParts.Ulonglong = DOUBLE_PACK(Sign,
                                        Exponent + 0x380,
                                        (ULONGLONG)Significand << 29);

    return DoubleParts.Double;
}

RTL_API
float
RtlFloatAdd (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine adds two floats together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the sum of the two values.

--*/

{

    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    Sign1 = FLOAT_GET_SIGN(Parts1);
    Sign2 = FLOAT_GET_SIGN(Parts2);
    if (Sign1 == Sign2) {
        return RtlpFloatAdd(Parts1, Parts2, Sign1);
    }

    return RtlpFloatSubtract(Parts1, Parts2, Sign1);
}

RTL_API
float
RtlFloatSubtract (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine subtracts two floats from each other.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value, the value to subtract.

Return Value:

    Returns the difference of the two values.

--*/

{

    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    Sign1 = FLOAT_GET_SIGN(Parts1);
    Sign2 = FLOAT_GET_SIGN(Parts2);
    if (Sign1 == Sign2) {
        return RtlpFloatSubtract(Parts1, Parts2, Sign1);
    }

    return RtlpFloatAdd(Parts1, Parts2, Sign1);
}

RTL_API
float
RtlFloatMultiply (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine multiplies two floats together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the product of the two values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;
    FLOAT_PARTS Result;
    SHORT ResultExponent;
    BOOL ResultSign;
    ULONG ResultSignificand;
    ULONGLONG ResultSignificand64;
    BOOL Sign1;
    BOOL Sign2;
    ULONG Significand1;
    ULONG Significand2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    Significand1 = FLOAT_GET_SIGNIFICAND(Parts1);
    Exponent1 = FLOAT_GET_EXPONENT(Parts1);
    Sign1 = FLOAT_GET_SIGN(Parts1);
    Significand2 = FLOAT_GET_SIGNIFICAND(Parts2);
    Exponent2 = FLOAT_GET_EXPONENT(Parts2);
    Sign2 = FLOAT_GET_SIGN(Parts2);
    ResultSign = Sign1 ^ Sign2;
    if (Exponent1 == FLOAT_NAN_EXPONENT) {
        if ((Significand1 != 0) ||
            ((Exponent2 == FLOAT_NAN_EXPONENT) && (Significand2 != 0))) {

            return RtlpFloatPropagateNan(Parts1, Parts2);
        }

        if ((Exponent2 | Significand2) == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulong = FLOAT_DEFAULT_NAN;
            return Result.Float;
        }

        Result.Ulong = FLOAT_PACK(ResultSign, FLOAT_NAN_EXPONENT, 0);
        return Result.Float;
    }

    if (Exponent2 == FLOAT_NAN_EXPONENT) {
        if (Significand2 != 0) {
            return RtlpFloatPropagateNan(Parts1, Parts2);
        }

        if ((Exponent1 | Significand1) == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulong = FLOAT_DEFAULT_NAN;
            return Result.Float;
        }

        Result.Ulong = FLOAT_PACK(ResultSign, FLOAT_NAN_EXPONENT, 0);
        return Result.Float;
    }

    if (Exponent1 == 0) {
        if (Significand1 == 0) {
            Result.Ulong = FLOAT_PACK(ResultSign, 0, 0);
            return Result.Float;
        }

        RtlpNormalizeFloatSubnormal(Significand1, &Exponent1, &Significand1);
    }

    if (Exponent2 == 0) {
        if (Significand2 == 0) {
            Result.Ulong = FLOAT_PACK(ResultSign, 0, 0);
            return Result.Float;
        }

        RtlpNormalizeFloatSubnormal(Significand2, &Exponent2, &Significand2);
    }

    ResultExponent = Exponent1 + Exponent2 - FLOAT_EXPONENT_BIAS;
    Significand1 = (Significand1 | 0x00800000UL) << 7;
    Significand2 = (Significand2 | 0x00800000UL) << 8;
    RtlpShift64RightJamming((ULONGLONG)Significand1 * Significand2,
                            32,
                            &ResultSignificand64);

    ResultSignificand = (ULONG)ResultSignificand64;
    if ((LONG)(ResultSignificand << 1) >= 0) {
        ResultSignificand <<= 1;
        ResultExponent -= 1;
    }

    Result.Float = RtlpRoundAndPackFloat(ResultSign,
                                         ResultExponent,
                                         ResultSignificand);

    return Result.Float;
}

RTL_API
float
RtlFloatDivide (
    float Dividend,
    float Divisor
    )

/*++

Routine Description:

    This routine divides one float into another.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the quotient of the two values.

--*/

{

    SHORT DividendExponent;
    FLOAT_PARTS DividendParts;
    BOOL DividendSign;
    ULONG DividendSignificand;
    SHORT DivisorExponent;
    FLOAT_PARTS DivisorParts;
    BOOL DivisorSign;
    ULONG DivisorSignificand;
    FLOAT_PARTS Result;
    SHORT ResultExponent;
    BOOL ResultSign;
    ULONG ResultSignificand;

    DividendParts.Float = Dividend;
    DivisorParts.Float = Divisor;
    DividendSignificand = FLOAT_GET_SIGNIFICAND(DividendParts);
    DividendExponent = FLOAT_GET_EXPONENT(DividendParts);
    DividendSign = FLOAT_GET_SIGN(DividendParts);
    DivisorSignificand = FLOAT_GET_SIGNIFICAND(DivisorParts);
    DivisorExponent = FLOAT_GET_EXPONENT(DivisorParts);
    DivisorSign = FLOAT_GET_SIGN(DivisorParts);
    ResultSign = DividendSign ^ DivisorSign;
    if (DividendExponent == FLOAT_NAN_EXPONENT) {
        if (DividendSignificand != 0) {
            return RtlpFloatPropagateNan(DividendParts, DivisorParts);
        }

        if (DivisorExponent == FLOAT_NAN_EXPONENT) {
            if (DivisorSignificand != 0) {
                return RtlpFloatPropagateNan(DividendParts, DivisorParts);
            }

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulong = FLOAT_DEFAULT_NAN;
            return Result.Float;
        }

        Result.Ulong = FLOAT_PACK(ResultSign, FLOAT_NAN_EXPONENT, 0);
        return Result.Float;
    }

    if (DivisorExponent == FLOAT_NAN_EXPONENT) {
        if (DivisorSignificand != 0) {
            return RtlpFloatPropagateNan(DividendParts, DivisorParts);
        }

        Result.Ulong = FLOAT_PACK(ResultSign, 0, 0);
        return Result.Float;
    }

    if (DivisorExponent == 0) {
        if (DivisorSignificand == 0) {
            if ((DividendExponent | DividendSignificand) == 0) {
                RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
                Result.Ulong = FLOAT_DEFAULT_NAN;
                return Result.Float;
            }

            RtlpSoftFloatRaise(SOFT_FLOAT_DIVIDE_BY_ZERO);
            Result.Ulong = FLOAT_PACK(ResultSign, FLOAT_NAN_EXPONENT, 0);
            return Result.Float;
        }

        RtlpNormalizeFloatSubnormal(DivisorSignificand,
                                    &DivisorExponent,
                                    &DivisorSignificand);
    }

    if (DividendExponent == 0) {
        if (DividendSignificand == 0) {
            Result.Ulong = FLOAT_PACK(ResultSign, 0, 0);
            return Result.Float;
        }

        RtlpNormalizeFloatSubnormal(DividendSignificand,
                                    &DividendExponent,
                                    &DividendSignificand);
    }

    ResultExponent = DividendExponent - DivisorExponent + 0x7D;
    DividendSignificand = (DividendSignificand | 0x00800000UL) << 7;
    DivisorSignificand = (DivisorSignificand | 0x00800000UL) << 8;
    if (DivisorSignificand <= (DividendSignificand + DividendSignificand)) {
        DividendSignificand >>= 1;
        ResultExponent += 1;
    }

    ResultSignificand = (((ULONGLONG)DividendSignificand) << 32) /
                        DivisorSignificand;

    if ((ResultSignificand & 0x3F) == 2) {
        if (((ULONGLONG)DivisorSignificand * ResultSignificand) !=
            (((ULONGLONG)DividendSignificand) << 32)) {

            ResultSignificand |= 0x1;
        }
    }

    Result.Float = RtlpRoundAndPackFloat(ResultSign,
                                         ResultExponent,
                                         ResultSignificand);

    return Result.Float;
}

RTL_API
float
RtlFloatModulo (
    float Dividend,
    float Divisor
    )

/*++

Routine Description:

    This routine divides one float into another, and returns the remainder.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the modulo of the two values.

--*/

{

    ULONG AlternateDividendSignificand;
    SHORT DividendExponent;
    FLOAT_PARTS DividendParts;
    CHAR DividendSign;
    ULONG DividendSignificand;
    ULONGLONG DividendSignificand64;
    SHORT DivisorExponent;
    FLOAT_PARTS DivisorParts;
    ULONG DivisorSignificand;
    ULONGLONG DivisorSignificand64;
    SHORT ExponentDifference;
    ULONG Quotient;
    ULONGLONG Quotient64;
    FLOAT_PARTS Result;
    CHAR ResultSign;
    LONG SignificandMean;

    DividendParts.Float = Dividend;
    DivisorParts.Float = Divisor;
    DividendSignificand = FLOAT_GET_SIGNIFICAND(DividendParts);
    DividendExponent = FLOAT_GET_EXPONENT(DividendParts);
    DividendSign = FLOAT_GET_SIGN(DividendParts);
    DivisorSignificand = FLOAT_GET_SIGNIFICAND(DivisorParts);
    DivisorExponent = FLOAT_GET_EXPONENT(DivisorParts);
    if (DividendExponent == FLOAT_NAN_EXPONENT) {
        if ((DividendSignificand != 0) ||
            ((DivisorExponent == FLOAT_NAN_EXPONENT) &&
             (DivisorSignificand != 0))) {

            return RtlpFloatPropagateNan(DividendParts, DivisorParts);
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulong = FLOAT_DEFAULT_NAN;
        return Result.Float;
    }

    if (DivisorExponent == FLOAT_NAN_EXPONENT) {
        if (DivisorSignificand != 0) {
            return RtlpFloatPropagateNan(DividendParts, DivisorParts);
        }

        return DividendParts.Float;
    }

    if (DivisorExponent == 0) {
        if (DivisorSignificand == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulong = FLOAT_DEFAULT_NAN;
            return Result.Float;
        }

        RtlpNormalizeFloatSubnormal(DivisorSignificand,
                                    &DivisorExponent,
                                    &DivisorSignificand);
    }

    if (DividendExponent == 0) {
        if (DividendSignificand == 0) {
            return DividendParts.Float;
        }

        RtlpNormalizeFloatSubnormal(DividendSignificand,
                                    &DividendExponent,
                                    &DividendSignificand);
    }

    ExponentDifference = DividendExponent - DivisorExponent;
    DividendSignificand |= 1UL << FLOAT_EXPONENT_SHIFT;
    DivisorSignificand |= 1UL << FLOAT_EXPONENT_SHIFT;
    if (ExponentDifference < 32) {
        DividendSignificand <<= 8;
        DivisorSignificand <<= 8;
        if (ExponentDifference < 0) {
            if (ExponentDifference < -1) {
                return DividendParts.Float;
            }

            DividendSignificand >>= 1;
        }

        Quotient = 0;
        if (DivisorSignificand <= DividendSignificand) {
            Quotient = 1;
        }

        if (Quotient != 0) {
            DividendSignificand -= DivisorSignificand;
        }

        if (ExponentDifference > 0) {
            Quotient = (((ULONGLONG)DividendSignificand) << 32) /
                       DivisorSignificand;

            Quotient >>= 32 - ExponentDifference;
            DivisorSignificand >>= 2;
            DividendSignificand = ((DividendSignificand >> 1) <<
                                   (ExponentDifference - 1)) -
                                  DivisorSignificand * Quotient;

        } else {
            DividendSignificand >>= 2;
            DivisorSignificand >>= 2;
        }

    } else {
        if (DivisorSignificand <= DividendSignificand) {
            DividendSignificand -= DivisorSignificand;
        }

        DividendSignificand64 = ((ULONGLONG)DividendSignificand) << 40;
        DivisorSignificand64 = ((ULONGLONG)DivisorSignificand) << 40;
        ExponentDifference -= 64;
        while (ExponentDifference > 0) {
            Quotient64 = RtlpEstimateDivide128To64(DividendSignificand64,
                                                   0,
                                                   DivisorSignificand64);

            if (Quotient64 > 2) {
                Quotient64 -= 2;

            } else {
                Quotient64 = 0;
            }

            DividendSignificand64 = -((DivisorSignificand * Quotient64) << 38);
            ExponentDifference -= 62;
        }

        ExponentDifference += 64;
        Quotient64 = RtlpEstimateDivide128To64(DividendSignificand64,
                                               0,
                                               DivisorSignificand64);

        if (Quotient64 > 2) {
            Quotient64 -= 2;

        } else {
            Quotient64 = 0;
        }

        Quotient = Quotient64 >> (64 - ExponentDifference);
        DivisorSignificand <<= 6;
        DividendSignificand = ((DividendSignificand64 >> 33) <<
                               (ExponentDifference - 1)) -
                              DivisorSignificand * Quotient;
    }

    do {
        AlternateDividendSignificand = DividendSignificand;
        Quotient += 1;
        DividendSignificand -= DivisorSignificand;

    } while ((LONG)DividendSignificand >= 0);

    SignificandMean = DividendSignificand + AlternateDividendSignificand;
    if ((SignificandMean < 0 ) ||
        ((SignificandMean == 0) && ((Quotient & 0x1) != 0))) {

        DividendSignificand = AlternateDividendSignificand;
    }

    ResultSign = 0;
    if ((LONG)DividendSignificand < 0) {
        ResultSign = 1;
    }

    if (ResultSign != 0) {
        DividendSignificand = -DividendSignificand;
    }

    Result.Float = RtlpNormalizeRoundAndPackFloat(DividendSign ^ ResultSign,
                                                  DivisorExponent,
                                                  DividendSignificand);

    return Result.Float;
}

RTL_API
float
RtlFloatSquareRoot (
    float Value
    )

/*++

Routine Description:

    This routine returns the square root of the given float.

Arguments:

    Value - Supplies the value to take the square root of.

Return Value:

    Returns the square root of the given value.

--*/

{

    ULONGLONG Remainder;
    FLOAT_PARTS Result;
    SHORT ResultExponent;
    ULONG ResultSignificand;
    ULONGLONG Term;
    SHORT ValueExponent;
    FLOAT_PARTS ValueParts;
    BOOL ValueSign;
    ULONG ValueSignificand;

    ValueParts.Float = Value;
    ValueSignificand = FLOAT_GET_SIGNIFICAND(ValueParts);
    ValueExponent = FLOAT_GET_EXPONENT(ValueParts);
    ValueSign = FLOAT_GET_SIGN(ValueParts);
    if (ValueExponent == FLOAT_NAN_EXPONENT) {
        if (ValueSignificand != 0) {
            return RtlpFloatPropagateNan(ValueParts, ValueParts);
        }

        if (ValueSign == 0) {
            return ValueParts.Float;
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulong = FLOAT_DEFAULT_NAN;
        return Result.Float;
    }

    if (ValueSign != 0) {
        if ((ValueExponent | ValueSignificand) == 0) {
            return ValueParts.Float;
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulong = FLOAT_DEFAULT_NAN;
        return Result.Float;
    }

    if (ValueExponent == 0) {
        if (ValueSignificand == 0) {
            return 0;
        }

        RtlpNormalizeFloatSubnormal(ValueSignificand,
                                    &ValueExponent,
                                    &ValueSignificand);
    }

    ResultExponent = ((ValueExponent - FLOAT_EXPONENT_BIAS) >> 1) +
                     (FLOAT_EXPONENT_BIAS - 1);

    ValueSignificand = (ValueSignificand | (1UL << FLOAT_EXPONENT_SHIFT)) << 8;
    ResultSignificand = RtlpEstimateSquareRoot32(ValueExponent,
                                                 ValueSignificand);

    ResultSignificand += 2;
    if ((ResultSignificand & 0x7F) <= 5) {
        if (ResultSignificand < 2) {
            ResultSignificand = (FLOAT_EXPONENT_MASK | FLOAT_VALUE_MASK);
            goto FloatSquareRootEnd;
        }

        ValueSignificand >>= ValueExponent & 0x1;
        Term = ((ULONGLONG)ResultSignificand) * ResultSignificand;
        Remainder = ((ULONGLONG)ValueSignificand << 32) - Term;
        while ((LONGLONG)Remainder < 0) {
            ResultSignificand -= 1;
            Remainder += (((ULONGLONG)ResultSignificand) << 1) | 0x1;
        }

        if (Remainder != 0) {
            ResultSignificand |= 0x1;
        }
    }

    RtlpShift32RightJamming(ResultSignificand, 1, &ResultSignificand);

FloatSquareRootEnd:
    return RtlpRoundAndPackFloat(0, ResultExponent, ResultSignificand);
}

RTL_API
BOOL
RtlFloatIsEqual (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine determines if the given floats are equal.

Arguments:

    Value1 - Supplies the first value to compare.

    Value2 - Supplies the second value to compare.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal. Note that NaN is not equal to anything,
    including itself.

--*/

{

    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    if (((FLOAT_GET_EXPONENT(Parts1) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((FLOAT_GET_EXPONENT(Parts2) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts2) != 0))) {

        if (FLOAT_IS_SIGNALING_NAN(Parts1) ||
            FLOAT_IS_SIGNALING_NAN(Parts2)) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    if ((Parts1.Ulong == Parts2.Ulong) ||
        ((ULONG)((Parts1.Ulong | Parts2.Ulong) << 1) == 0)) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlFloatIsLessThanOrEqual (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

{

    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    if (((FLOAT_GET_EXPONENT(Parts1) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((FLOAT_GET_EXPONENT(Parts2) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    Sign1 = FLOAT_GET_SIGN(Parts1);
    Sign2 = FLOAT_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) ||
            ((ULONG)((Parts1.Ulong | Parts2.Ulong) << 1) == 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulong == Parts2.Ulong) ||
        (Sign1 ^ (Parts1.Ulong < Parts2.Ulong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlFloatIsLessThan (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    if (((FLOAT_GET_EXPONENT(Parts1) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((FLOAT_GET_EXPONENT(Parts2) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    Sign1 = FLOAT_GET_SIGN(Parts1);
    Sign2 = FLOAT_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) &&
            ((ULONG)((Parts1.Ulong | Parts2.Ulong) << 1) != 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulong != Parts2.Ulong) &&
        (Sign1 ^ (Parts1.Ulong < Parts2.Ulong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlFloatSignalingIsEqual (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine determines if the given values are equal, generating an
    invalid floating point exception if either is NaN.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    if (((FLOAT_GET_EXPONENT(Parts1) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((FLOAT_GET_EXPONENT(Parts2) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    if ((Parts1.Ulong == Parts2.Ulong) ||
        ((ULONG)((Parts1.Ulong | Parts2.Ulong) << 1) == 0)) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlFloatIsLessThanOrEqualQuiet (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value. Quiet NaNs do not generate floating point exceptions.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

{

    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    if (((FLOAT_GET_EXPONENT(Parts1) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((FLOAT_GET_EXPONENT(Parts2) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts2) != 0))) {

        if ((FLOAT_IS_SIGNALING_NAN(Parts1)) ||
            (FLOAT_IS_SIGNALING_NAN(Parts2))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    Sign1 = FLOAT_GET_SIGN(Parts1);
    Sign2 = FLOAT_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) ||
            ((ULONG)((Parts1.Ulong | Parts2.Ulong) << 1) == 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulong == Parts2.Ulong) ||
        (Sign1 ^ (Parts1.Ulong < Parts2.Ulong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlFloatIsLessThanQuiet (
    float Value1,
    float Value2
    )

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value. Quiet NaNs do not cause float point exceptions to be raised.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    FLOAT_PARTS Parts1;
    FLOAT_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Float = Value1;
    Parts2.Float = Value2;
    if (((FLOAT_GET_EXPONENT(Parts1) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((FLOAT_GET_EXPONENT(Parts2) == FLOAT_NAN_EXPONENT) &&
         (FLOAT_GET_SIGNIFICAND(Parts2) != 0))) {

        if ((FLOAT_IS_SIGNALING_NAN(Parts1)) ||
            (FLOAT_IS_SIGNALING_NAN(Parts2))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    Sign1 = FLOAT_GET_SIGN(Parts1);
    Sign2 = FLOAT_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) &&
            ((ULONG)((Parts1.Ulong | Parts2.Ulong) << 1) != 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulong != Parts2.Ulong) &&
        (Sign1 ^ (Parts1.Ulong < Parts2.Ulong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsNan (
    double Value
    )

/*++

Routine Description:

    This routine determines if the given value is Not a Number.

Arguments:

    Value - Supplies the floating point value to query.

Return Value:

    TRUE if the given value is Not a Number.

    FALSE otherwise.

--*/

{

    DOUBLE_PARTS Parts;

    Parts.Double = Value;
    if (DOUBLE_GET_EXPONENT(Parts) == DOUBLE_NAN_EXPONENT) {
        return TRUE;
    }

    return FALSE;
}

RTL_API
float
RtlDoubleConvertToFloat (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a float (32 bit floating
    point number).

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the float equivalent.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    FLOAT_PARTS Result;
    ULONG ResultSignificand;
    CHAR Sign;
    ULONGLONG Significand;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if (Exponent == DOUBLE_NAN_EXPONENT) {
        if (Significand != 0) {
            return RtlpCommonNanToFloat(RtlpDoubleToCommonNan(Parts));
        }

        Result.Ulong = FLOAT_PACK(Sign, 0xFF, 0);
        return Result.Float;
    }

    RtlpShift64RightJamming(Significand, 22, &Significand);
    ResultSignificand = Significand;
    if ((Exponent != 0) || (ResultSignificand != 0)) {
        ResultSignificand |= 0x40000000;
        Exponent -= 0x381;
    }

    return RtlpRoundAndPackFloat(Sign, Exponent, ResultSignificand);
}

RTL_API
double
RtlDoubleAdd (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine adds two doubles together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the sum of the two values.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 == Sign2) {
        return RtlpDoubleAdd(Parts1, Parts2, Sign1);
    }

    return RtlpDoubleSubtract(Parts1, Parts2, Sign1);
}

RTL_API
double
RtlDoubleSubtract (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine subtracts two doubles from each other.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value, the value to subtract.

Return Value:

    Returns the difference of the two values.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 == Sign2) {
        return RtlpDoubleSubtract(Parts1, Parts2, Sign1);
    }

    return RtlpDoubleAdd(Parts1, Parts2, Sign1);
}

RTL_API
double
RtlDoubleMultiply (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine multiplies two doubles together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

Return Value:

    Returns the product of the two values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    BOOL ResultSign;
    ULONGLONG ResultSignificand0;
    ULONGLONG ResultSignificand1;
    BOOL Sign1;
    BOOL Sign2;
    ULONGLONG Significand1;
    ULONGLONG Significand2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    Significand1 = DOUBLE_GET_SIGNIFICAND(Parts1);
    Exponent1 = DOUBLE_GET_EXPONENT(Parts1);
    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Significand2 = DOUBLE_GET_SIGNIFICAND(Parts2);
    Exponent2 = DOUBLE_GET_EXPONENT(Parts2);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    ResultSign = Sign1 ^ Sign2;
    if (Exponent1 == DOUBLE_NAN_EXPONENT) {
        if ((Significand1 != 0) ||
            ((Exponent2 == DOUBLE_NAN_EXPONENT) && (Significand2 != 0))) {

            return RtlpDoublePropagateNan(Parts1, Parts2);
        }

        if ((Exponent2 | Significand2) == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulonglong = DOUBLE_DEFAULT_NAN;
            return Result.Double;
        }

        Result.Ulonglong = DOUBLE_PACK(ResultSign, DOUBLE_NAN_EXPONENT, 0);
        return Result.Double;
    }

    if (Exponent2 == DOUBLE_NAN_EXPONENT) {
        if (Significand2 != 0) {
            return RtlpDoublePropagateNan(Parts1, Parts2);
        }

        if ((Exponent1 | Significand1) == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulonglong = DOUBLE_DEFAULT_NAN;
            return Result.Double;
        }

        Result.Ulonglong = DOUBLE_PACK(ResultSign, DOUBLE_NAN_EXPONENT, 0);
        return Result.Double;
    }

    if (Exponent1 == 0) {
        if (Significand1 == 0) {
            Result.Ulonglong = DOUBLE_PACK(ResultSign, 0, 0);
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(Significand1, &Exponent1, &Significand1);
    }

    if (Exponent2 == 0) {
        if (Significand2 == 0) {
            Result.Ulonglong = DOUBLE_PACK(ResultSign, 0, 0);
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(Significand2, &Exponent2, &Significand2);
    }

    ResultExponent = Exponent1 + Exponent2 - DOUBLE_EXPONENT_BIAS;
    Significand1 = (Significand1 | 0x0010000000000000ULL) << 10;
    Significand2 = (Significand2 | 0x0010000000000000ULL) << 11;
    RtlpMultiply64To128(Significand1,
                        Significand2,
                        &ResultSignificand0,
                        &ResultSignificand1);

    if (ResultSignificand1 != 0) {
        ResultSignificand0 |= 0x1;
    }

    if ((LONGLONG)(ResultSignificand0 << 1) >= 0) {
        ResultSignificand0 <<= 1;
        ResultExponent -= 1;
    }

    Result.Double = RtlpRoundAndPackDouble(ResultSign,
                                           ResultExponent,
                                           ResultSignificand0);

    return Result.Double;
}

RTL_API
double
RtlDoubleDivide (
    double Dividend,
    double Divisor
    )

/*++

Routine Description:

    This routine divides one double into another.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the quotient of the two values.

--*/

{

    SHORT DividendExponent;
    DOUBLE_PARTS DividendParts;
    BOOL DividendSign;
    ULONGLONG DividendSignificand;
    SHORT DivisorExponent;
    DOUBLE_PARTS DivisorParts;
    BOOL DivisorSign;
    ULONGLONG DivisorSignificand;
    ULONGLONG Remainder0;
    ULONGLONG Remainder1;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    BOOL ResultSign;
    ULONGLONG ResultSignificand;
    ULONGLONG Term0;
    ULONGLONG Term1;

    DividendParts.Double = Dividend;
    DivisorParts.Double = Divisor;
    DividendSignificand = DOUBLE_GET_SIGNIFICAND(DividendParts);
    DividendExponent = DOUBLE_GET_EXPONENT(DividendParts);
    DividendSign = DOUBLE_GET_SIGN(DividendParts);
    DivisorSignificand = DOUBLE_GET_SIGNIFICAND(DivisorParts);
    DivisorExponent = DOUBLE_GET_EXPONENT(DivisorParts);
    DivisorSign = DOUBLE_GET_SIGN(DivisorParts);
    ResultSign = DividendSign ^ DivisorSign;
    if (DividendExponent == DOUBLE_NAN_EXPONENT) {
        if (DividendSignificand ) {
            return RtlpDoublePropagateNan(DividendParts, DivisorParts);
        }

        if (DivisorExponent == DOUBLE_NAN_EXPONENT) {
            if (DivisorSignificand != 0) {
                return RtlpDoublePropagateNan(DividendParts, DivisorParts);
            }

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulonglong = DOUBLE_DEFAULT_NAN;
            return Result.Double;
        }

        Result.Ulonglong = DOUBLE_PACK(ResultSign, DOUBLE_NAN_EXPONENT, 0);
        return Result.Double;
    }

    if (DivisorExponent == DOUBLE_NAN_EXPONENT) {
        if (DivisorSignificand != 0) {
            return RtlpDoublePropagateNan(DividendParts, DivisorParts);
        }

        Result.Ulonglong = DOUBLE_PACK(ResultSign, 0, 0);
        return Result.Double;
    }

    if (DivisorExponent == 0) {
        if (DivisorSignificand == 0) {
            if ((DividendExponent | DividendSignificand) == 0) {
                RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
                Result.Ulonglong = DOUBLE_DEFAULT_NAN;
                return Result.Double;
            }

            RtlpSoftFloatRaise(SOFT_FLOAT_DIVIDE_BY_ZERO);
            Result.Ulonglong = DOUBLE_PACK(ResultSign, DOUBLE_NAN_EXPONENT, 0);
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(DivisorSignificand,
                                     &DivisorExponent,
                                     &DivisorSignificand);
    }

    if (DividendExponent == 0) {
        if (DividendSignificand == 0) {
            Result.Ulonglong = DOUBLE_PACK(ResultSign, 0, 0);
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(DividendSignificand,
                                     &DividendExponent,
                                     &DividendSignificand);
    }

    ResultExponent = DividendExponent - DivisorExponent + 0x3FD;
    DividendSignificand = (DividendSignificand | 0x0010000000000000ULL) << 10;
    DivisorSignificand = (DivisorSignificand | 0x0010000000000000ULL) << 11;
    if (DivisorSignificand <= (DividendSignificand + DividendSignificand)) {
        DividendSignificand >>= 1;
        ResultExponent += 1;
    }

    ResultSignificand = RtlpEstimateDivide128To64(DividendSignificand,
                                                  0,
                                                  DivisorSignificand);

    if ((ResultSignificand & 0x1FF) <= 2) {
        RtlpMultiply64To128(DivisorSignificand,
                            ResultSignificand,
                            &Term0,
                            &Term1);

        RtlpSubtract128(DividendSignificand,
                        0,
                        Term0,
                        Term1,
                        &Remainder0,
                        &Remainder1);

        while ((LONGLONG)Remainder0 < 0) {
            ResultSignificand -= 1;
            RtlpAdd128(Remainder0,
                       Remainder1,
                       0,
                       DivisorSignificand,
                       &Remainder0,
                       &Remainder1);
        }

        ResultSignificand |= (Remainder1 != 0);
    }

    Result.Double = RtlpRoundAndPackDouble(ResultSign,
                                           ResultExponent,
                                           ResultSignificand);

    return Result.Double;
}

RTL_API
double
RtlDoubleModulo (
    double Dividend,
    double Divisor
    )

/*++

Routine Description:

    This routine divides one double into another, and returns the remainder.

Arguments:

    Dividend - Supplies the numerator.

    Divisor - Supplies the denominator.

Return Value:

    Returns the modulo of the two values.

--*/

{

    ULONGLONG AlternateDividendSignificand;
    SHORT DividendExponent;
    DOUBLE_PARTS DividendParts;
    CHAR DividendSign;
    ULONGLONG DividendSignificand;
    SHORT DivisorExponent;
    DOUBLE_PARTS DivisorParts;
    ULONGLONG DivisorSignificand;
    SHORT ExponentDifference;
    ULONGLONG Quotient;
    DOUBLE_PARTS Result;
    CHAR ResultSign;
    LONGLONG SignificandMean;

    DividendParts.Double = Dividend;
    DivisorParts.Double = Divisor;
    DividendSignificand = DOUBLE_GET_SIGNIFICAND(DividendParts);
    DividendExponent = DOUBLE_GET_EXPONENT(DividendParts);
    DividendSign = DOUBLE_GET_SIGN(DividendParts);
    DivisorSignificand = DOUBLE_GET_SIGNIFICAND(DivisorParts);
    DivisorExponent = DOUBLE_GET_EXPONENT(DivisorParts);
    if (DividendExponent == DOUBLE_NAN_EXPONENT) {
        if ((DividendSignificand != 0) ||
            ((DivisorExponent == DOUBLE_NAN_EXPONENT) &&
             (DivisorSignificand != 0))) {

            return RtlpDoublePropagateNan(DividendParts, DivisorParts);
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulonglong = DOUBLE_DEFAULT_NAN;
        return Result.Double;
    }

    if (DivisorExponent == DOUBLE_NAN_EXPONENT) {
        if (DivisorSignificand != 0) {
            return RtlpDoublePropagateNan(DividendParts, DivisorParts);
        }

        return DividendParts.Double;
    }

    if (DivisorExponent == 0) {
        if (DivisorSignificand == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            Result.Ulonglong = DOUBLE_DEFAULT_NAN;
            return Result.Double;
        }

        RtlpNormalizeDoubleSubnormal(DivisorSignificand,
                                     &DivisorExponent,
                                     &DivisorSignificand);
    }

    if (DividendExponent == 0) {
        if (DividendSignificand == 0) {
            return DividendParts.Double;
        }

        RtlpNormalizeDoubleSubnormal(DividendSignificand,
                                     &DividendExponent,
                                     &DividendSignificand);
    }

    ExponentDifference = DividendExponent - DivisorExponent;
    DividendSignificand = (DividendSignificand | 0x0010000000000000ULL) << 11;
    DivisorSignificand = (DivisorSignificand | 0x0010000000000000ULL) << 11;
    if (ExponentDifference < 0) {
        if (ExponentDifference < -1) {
            return DividendParts.Double;
        }

        DividendSignificand >>= 1;
    }

    Quotient = 0;
    if (DivisorSignificand <= DividendSignificand) {
        Quotient = 1;
    }

    if (Quotient != 0) {
        DividendSignificand -= DivisorSignificand;
    }

    ExponentDifference -= 64;
    while (ExponentDifference > 0) {
        Quotient = RtlpEstimateDivide128To64(DividendSignificand,
                                             0,
                                             DivisorSignificand);

        if (Quotient > 2) {
            Quotient -= 2;

        } else {
            Quotient = 0;
        }

        DividendSignificand = -((DivisorSignificand >> 2) * Quotient);
        ExponentDifference -= 62;
    }

    ExponentDifference += 64;
    if (ExponentDifference > 0) {
        Quotient = RtlpEstimateDivide128To64(DividendSignificand,
                                             0,
                                             DivisorSignificand);

        if (Quotient > 2) {
            Quotient -= 2;

        } else {
            Quotient = 0;
        }

        Quotient >>= 64 - ExponentDifference;
        DivisorSignificand >>= 2;
        DividendSignificand = ((DividendSignificand >> 1) <<
                               (ExponentDifference - 1)) -
                              DivisorSignificand * Quotient;

    } else {
        DividendSignificand >>= 2;
        DivisorSignificand >>= 2;
    }

    do {
        AlternateDividendSignificand = DividendSignificand;
        Quotient += 1;
        DividendSignificand -= DivisorSignificand;

    } while ((LONGLONG)DividendSignificand >= 0);

    SignificandMean = DividendSignificand + AlternateDividendSignificand;
    if ((SignificandMean < 0 ) ||
        ((SignificandMean == 0) && ((Quotient & 0x1) != 0))) {

        DividendSignificand = AlternateDividendSignificand;
    }

    ResultSign = 0;
    if ((LONGLONG)DividendSignificand < 0) {
        ResultSign = 1;
    }

    if (ResultSign != 0) {
        DividendSignificand = -DividendSignificand;
    }

    Result.Double = RtlpNormalizeRoundAndPackDouble(DividendSign ^ ResultSign,
                                                    DivisorExponent,
                                                    DividendSignificand);

    return Result.Double;
}

RTL_API
double
RtlDoubleSquareRoot (
    double Value
    )

/*++

Routine Description:

    This routine returns the square root of the given double.

Arguments:

    Value - Supplies the value to take the square root of.

Return Value:

    Returns the square root of the given value.

--*/

{

    ULONGLONG DoubleResultSignificand;
    ULONGLONG PreDivisionSignificand;
    ULONGLONG RemainderHigh;
    ULONGLONG RemainderLow;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    ULONGLONG ResultSignificand;
    ULONGLONG TermHigh;
    ULONGLONG TermLow;
    SHORT ValueExponent;
    DOUBLE_PARTS ValueParts;
    BOOL ValueSign;
    ULONGLONG ValueSignificand;

    ValueParts.Double = Value;
    ValueSignificand = DOUBLE_GET_SIGNIFICAND(ValueParts);
    ValueExponent = DOUBLE_GET_EXPONENT(ValueParts);
    ValueSign = DOUBLE_GET_SIGN(ValueParts);
    if (ValueExponent == DOUBLE_NAN_EXPONENT) {
        if (ValueSignificand != 0) {
            return RtlpDoublePropagateNan(ValueParts, ValueParts);
        }

        if (ValueSign == 0) {
            return ValueParts.Double;
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulonglong = DOUBLE_DEFAULT_NAN;
        return Result.Double;
    }

    if (ValueSign != 0) {
        if ((ValueExponent | ValueSignificand) == 0) {
            return ValueParts.Double;
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulonglong = DOUBLE_DEFAULT_NAN;
        return Result.Double;
    }

    if (ValueExponent == 0) {
        if (ValueSignificand == 0) {
            return 0;
        }

        RtlpNormalizeDoubleSubnormal(ValueSignificand,
                                     &ValueExponent,
                                     &ValueSignificand);
    }

    ResultExponent = ((ValueExponent - DOUBLE_EXPONENT_BIAS) >> 1) +
                     (DOUBLE_EXPONENT_BIAS - 1);

    ValueSignificand |= (1ULL << DOUBLE_EXPONENT_SHIFT);
    ResultSignificand = RtlpEstimateSquareRoot32(ValueExponent,
                                                 ValueSignificand >> 21);

    ValueSignificand <<= 9 - (ValueExponent & 0x1);
    PreDivisionSignificand = ResultSignificand;
    ResultSignificand = RtlpEstimateDivide128To64(ValueSignificand,
                                                  0,
                                                  ResultSignificand << 32);

    ResultSignificand += (PreDivisionSignificand << 30);
    if ((ResultSignificand & 0x1FF) <= 5) {
        DoubleResultSignificand = ResultSignificand << 1;
        RtlpMultiply64To128(ResultSignificand,
                            ResultSignificand,
                            &TermHigh,
                            &TermLow);

        RtlpSubtract128(ValueSignificand,
                        0,
                        TermHigh,
                        TermLow,
                        &RemainderHigh,
                        &RemainderLow);

        while ((LONGLONG)RemainderHigh < 0) {
            ResultSignificand -= 1;
            DoubleResultSignificand -= 2;
            RtlpAdd128(RemainderHigh,
                       RemainderLow,
                       ResultSignificand >> 63,
                       DoubleResultSignificand | 1,
                       &RemainderHigh,
                       &RemainderLow );
        }

        if (((RemainderHigh | RemainderLow ) != 0)) {
            ResultSignificand |= 0x1;
        }
    }

    return RtlpRoundAndPackDouble(0, ResultExponent, ResultSignificand);
}

RTL_API
BOOL
RtlDoubleIsEqual (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given doubles are equal.

Arguments:

    Value1 - Supplies the first value to compare.

    Value2 - Supplies the second value to compare.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal. Note that NaN is not equal to anything,
    including itself.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        if (DOUBLE_IS_SIGNALING_NAN(Parts1) ||
            DOUBLE_IS_SIGNALING_NAN(Parts2)) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong == Parts2.Ulonglong) ||
        (((Parts1.Ulonglong | Parts2.Ulonglong) << 1) == 0)) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsLessThanOrEqual (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) ||
            ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) == 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong == Parts2.Ulonglong) ||
        (Sign1 ^ (Parts1.Ulonglong < Parts2.Ulonglong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsLessThan (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) &&
            ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) != 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong != Parts2.Ulonglong) &&
        (Sign1 ^ (Parts1.Ulonglong < Parts2.Ulonglong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleSignalingIsEqual (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given values are equal, generating an
    invalid floating point exception if either is NaN.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        return FALSE;
    }

    if ((Parts1.Ulonglong == Parts2.Ulonglong) ||
        ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) == 0)) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsLessThanOrEqualQuiet (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given value is less than or equal to the
    second value. Quiet NaNs do not generate floating point exceptions.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is less than or equal to the first.

    FALSE if the first value is greater than the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        if ((DOUBLE_IS_SIGNALING_NAN(Parts1)) ||
            (DOUBLE_IS_SIGNALING_NAN(Parts2))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) ||
            ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) == 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong == Parts2.Ulonglong) ||
        (Sign1 ^ (Parts1.Ulonglong < Parts2.Ulonglong))) {

        return TRUE;
    }

    return FALSE;
}

RTL_API
BOOL
RtlDoubleIsLessThanQuiet (
    double Value1,
    double Value2
    )

/*++

Routine Description:

    This routine determines if the given value is strictly less than the
    second value. Quiet NaNs do not cause float point exceptions to be raised.

Arguments:

    Value1 - Supplies the first value to compare, the left hand side of the
        comparison.

    Value2 - Supplies the second value to compare, the right hand side of the
        comparison.

Return Value:

    TRUE if the first value is strictly less than to the first.

    FALSE if the first value is greater than or equal to the second.

--*/

{

    DOUBLE_PARTS Parts1;
    DOUBLE_PARTS Parts2;
    CHAR Sign1;
    CHAR Sign2;

    Parts1.Double = Value1;
    Parts2.Double = Value2;
    if (((DOUBLE_GET_EXPONENT(Parts1) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts1) != 0)) ||
        ((DOUBLE_GET_EXPONENT(Parts2) == DOUBLE_NAN_EXPONENT) &&
         (DOUBLE_GET_SIGNIFICAND(Parts2) != 0))) {

        if ((DOUBLE_IS_SIGNALING_NAN(Parts1)) ||
            (DOUBLE_IS_SIGNALING_NAN(Parts2))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        }

        return FALSE;
    }

    Sign1 = DOUBLE_GET_SIGN(Parts1);
    Sign2 = DOUBLE_GET_SIGN(Parts2);
    if (Sign1 != Sign2) {
        if ((Sign1 != 0) &&
            ((ULONGLONG)((Parts1.Ulonglong | Parts2.Ulonglong) << 1) != 0)) {

            return TRUE;
        }

        return FALSE;
    }

    if ((Parts1.Ulonglong != Parts2.Ulonglong) &&
        (Sign1 ^ (Parts1.Ulonglong < Parts2.Ulonglong))) {

        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

float
RtlpFloatAdd (
    FLOAT_PARTS Value1,
    FLOAT_PARTS Value2,
    CHAR Sign
    )

/*++

Routine Description:

    This routine adds the absolute values of two float together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value to add.

    Sign - Supplies 1 if the sum should be negated before being returned, or
        0 if it should be left alone.

Return Value:

    Returns the sum of the two absolute values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    SHORT ExponentDifference;
    FLOAT_PARTS Result;
    SHORT ResultExponent;
    ULONG ResultSignificand;
    ULONG Significand1;
    ULONG Significand2;

    Significand1 = FLOAT_GET_SIGNIFICAND(Value1);
    Exponent1 = FLOAT_GET_EXPONENT(Value1);
    Significand2 = FLOAT_GET_SIGNIFICAND(Value2);
    Exponent2 = FLOAT_GET_EXPONENT(Value2);
    ExponentDifference = Exponent1 - Exponent2;
    Significand1 <<= 6;
    Significand2 <<= 6;
    if (ExponentDifference > 0) {
        if (Exponent1 == FLOAT_NAN_EXPONENT) {
            if (Significand1 != 0) {
                return RtlpFloatPropagateNan(Value1, Value2);
            }

            return Value1.Float;
        }

        if (Exponent2 == 0) {
            ExponentDifference -= 1;

        } else {
            Significand2 |= 0x20000000UL;
        }

        RtlpShift32RightJamming(Significand2,
                                ExponentDifference,
                                &Significand2);

        ResultExponent = Exponent1;

    } else if (ExponentDifference < 0) {
        if (Exponent2 == FLOAT_NAN_EXPONENT) {
            if (Significand2 != 0) {
                return RtlpFloatPropagateNan(Value1, Value2);
            }

            Result.Ulong = FLOAT_PACK(Sign, FLOAT_NAN_EXPONENT, 0);
            return Result.Float;
        }

        if (Exponent1 == 0) {
            ExponentDifference += 1;

        } else {
            Significand1 |= 0x20000000UL;
        }

        RtlpShift32RightJamming(Significand1,
                                -ExponentDifference,
                                &Significand1);

        ResultExponent = Exponent2;

    } else {
        if (Exponent1 == FLOAT_NAN_EXPONENT) {
            if ((Significand1 | Significand2) != 0) {
                return RtlpFloatPropagateNan( Value1, Value2 );
            }

            return Value1.Float;
        }

        if (Exponent1 == 0) {
            Result.Ulong = FLOAT_PACK(Sign,
                                      0,
                                      (Significand1 + Significand2) >> 6);

            return Result.Float;
        }

        ResultSignificand = 0x40000000UL + Significand1 + Significand2;
        ResultExponent = Exponent1;
        goto FloatAddEnd;
    }

    Significand1 |= 0x20000000UL;
    ResultSignificand = (Significand1 + Significand2) << 1;
    ResultExponent -= 1;
    if ((LONG)ResultSignificand < 0 ) {
        ResultSignificand = Significand1 + Significand2;
        ResultExponent += 1;
    }

FloatAddEnd:
    return RtlpRoundAndPackFloat(Sign, ResultExponent, ResultSignificand);
}

float
RtlpFloatSubtract (
    FLOAT_PARTS Value1,
    FLOAT_PARTS Value2,
    CHAR Sign
    )

/*++

Routine Description:

    This routine subtracts the absolute values of two floats from each other.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the value to subtract.

    Sign - Supplies 1 if the difference should be negated before being
        returned, or 0 if it should be left alone.

Return Value:

    Returns the difference of the two absolute values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    SHORT ExponentDifference;
    FLOAT_PARTS Result;
    SHORT ResultExponent;
    ULONG ResultSignificand;
    ULONG Significand1;
    ULONG Significand2;

    Significand1 = FLOAT_GET_SIGNIFICAND(Value1);
    Exponent1 = FLOAT_GET_EXPONENT(Value1);
    Significand2 = FLOAT_GET_SIGNIFICAND(Value2);
    Exponent2 = FLOAT_GET_EXPONENT(Value2);
    ExponentDifference = Exponent1 - Exponent2;
    Significand1 <<= 7;
    Significand2 <<= 7;
    if (ExponentDifference > 0) {
        if (Exponent1 == FLOAT_NAN_EXPONENT) {
            if (Significand1 != 0) {
                return RtlpFloatPropagateNan(Value1, Value2);
            }

            return Value1.Float;
        }

        if (Exponent2 == 0) {
            ExponentDifference -= 1;

        } else {
            Significand2 |= 0x40000000UL;
        }

        RtlpShift32RightJamming(Significand2,
                                ExponentDifference,
                                &Significand2);

        Significand1 |= 0x40000000UL;
        ResultSignificand = Significand1 - Significand2;
        ResultExponent = Exponent1;
        goto FloatSubtractEnd;
    }

    if (ExponentDifference < 0) {
        if (Exponent2 == FLOAT_NAN_EXPONENT) {
            if (Significand2 != 0) {
                return RtlpFloatPropagateNan(Value1, Value2);
            }

            if (Sign != 0) {
                Result.Ulong = FLOAT_PACK(0, FLOAT_NAN_EXPONENT, 0);

            } else {
                Result.Ulong = FLOAT_PACK(1, FLOAT_NAN_EXPONENT, 0);
            }

            return Result.Float;
        }

        if (Exponent1 == 0) {
            ExponentDifference += 1;

        } else {
            Significand1 |= 0x40000000UL;
        }

        RtlpShift32RightJamming(Significand1,
                                -ExponentDifference,
                                &Significand1);

        Significand2 |= 0x40000000UL;
        ResultSignificand = Significand2 - Significand1;
        ResultExponent = Exponent2;
        Sign ^= 1;
        goto FloatSubtractEnd;
    }

    if (Exponent1 == FLOAT_NAN_EXPONENT) {
        if ((Significand1 | Significand2) != 0) {
            return RtlpFloatPropagateNan(Value1, Value2);
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulong = FLOAT_DEFAULT_NAN;
        return Result.Float;
    }

    if (Exponent1 == 0) {
        Exponent1 = 1;
        Exponent2 = 1;
    }

    if (Significand2 < Significand1) {
        ResultSignificand = Significand1 - Significand2;
        ResultExponent = Exponent1;
        goto FloatSubtractEnd;
    }

    if (Significand1 < Significand2) {
        ResultSignificand = Significand2 - Significand1;
        ResultExponent = Exponent2;
        Sign ^= 1;
        goto FloatSubtractEnd;
    }

    if (RtlRoundingMode == SoftFloatRoundDown) {
        Result.Ulong = FLOAT_PACK(1, 0, 0);

    } else {
        Result.Ulong = FLOAT_PACK(0, 0, 0);
    }

    return Result.Float;

FloatSubtractEnd:
    ResultExponent -= 1;
    Result.Float = RtlpNormalizeRoundAndPackFloat(Sign,
                                                  ResultExponent,
                                                  ResultSignificand);

    return Result.Float;
}

double
RtlpDoubleAdd (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2,
    CHAR Sign
    )

/*++

Routine Description:

    This routine adds the absolute values of two doubles together.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value to add.

    Sign - Supplies 1 if the sum should be negated before being returned, or
        0 if it should be left alone.

Return Value:

    Returns the sum of the two absolute values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    SHORT ExponentDifference;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    ULONGLONG ResultSignificand;
    ULONGLONG Significand1;
    ULONGLONG Significand2;

    Significand1 = DOUBLE_GET_SIGNIFICAND(Value1);
    Exponent1 = DOUBLE_GET_EXPONENT(Value1);
    Significand2 = DOUBLE_GET_SIGNIFICAND(Value2);
    Exponent2 = DOUBLE_GET_EXPONENT(Value2);
    ExponentDifference = Exponent1 - Exponent2;
    Significand1 <<= 9;
    Significand2 <<= 9;
    if (ExponentDifference > 0) {
        if (Exponent1 == DOUBLE_NAN_EXPONENT) {
            if (Significand1 != 0) {
                return RtlpDoublePropagateNan(Value1, Value2);
            }

            return Value1.Double;
        }

        if (Exponent2 == 0) {
            ExponentDifference -= 1;

        } else {
            Significand2 |= 0x2000000000000000ULL;
        }

        RtlpShift64RightJamming(Significand2,
                                ExponentDifference,
                                &Significand2);

        ResultExponent = Exponent1;

    } else if (ExponentDifference < 0) {
        if (Exponent2 == DOUBLE_NAN_EXPONENT) {
            if (Significand2 != 0) {
                return RtlpDoublePropagateNan(Value1, Value2);
            }

            Result.Ulonglong = DOUBLE_PACK(Sign, DOUBLE_NAN_EXPONENT, 0);
            return Result.Double;
        }

        if (Exponent1 == 0) {
            ExponentDifference += 1;

        } else {
            Significand1 |= 0x2000000000000000ULL;
        }

        RtlpShift64RightJamming(Significand1,
                                -ExponentDifference,
                                &Significand1);

        ResultExponent = Exponent2;

    } else {
        if (Exponent1 == DOUBLE_NAN_EXPONENT) {
            if ((Significand1 | Significand2) != 0) {
                return RtlpDoublePropagateNan( Value1, Value2 );
            }

            return Value1.Double;
        }

        if (Exponent1 == 0) {
            Result.Ulonglong = DOUBLE_PACK(Sign,
                                           0,
                                           (Significand1 + Significand2) >> 9);

            return Result.Double;
        }

        ResultSignificand = 0x4000000000000000ULL + Significand1 + Significand2;
        ResultExponent = Exponent1;
        goto DoubleAddEnd;
    }

    Significand1 |= 0x2000000000000000ULL;
    ResultSignificand = (Significand1 + Significand2) << 1;
    ResultExponent -= 1;
    if ((LONGLONG)ResultSignificand < 0 ) {
        ResultSignificand = Significand1 + Significand2;
        ResultExponent += 1;
    }

DoubleAddEnd:
    return RtlpRoundAndPackDouble(Sign, ResultExponent, ResultSignificand);
}

double
RtlpDoubleSubtract (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2,
    CHAR Sign
    )

/*++

Routine Description:

    This routine subtracts the absolute values of two doubles from each other.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the value to subtract.

    Sign - Supplies 1 if the difference should be negated before being
        returned, or 0 if it should be left alone.

Return Value:

    Returns the difference of the two absolute values.

--*/

{

    SHORT Exponent1;
    SHORT Exponent2;
    SHORT ExponentDifference;
    DOUBLE_PARTS Result;
    SHORT ResultExponent;
    ULONGLONG ResultSignificand;
    ULONGLONG Significand1;
    ULONGLONG Significand2;

    Significand1 = DOUBLE_GET_SIGNIFICAND(Value1);
    Exponent1 = DOUBLE_GET_EXPONENT(Value1);
    Significand2 = DOUBLE_GET_SIGNIFICAND(Value2);
    Exponent2 = DOUBLE_GET_EXPONENT(Value2);
    ExponentDifference = Exponent1 - Exponent2;
    Significand1 <<= 10;
    Significand2 <<= 10;
    if (ExponentDifference > 0) {
        if (Exponent1 == DOUBLE_NAN_EXPONENT) {
            if (Significand1 != 0) {
                return RtlpDoublePropagateNan(Value1, Value2);
            }

            return Value1.Double;
        }

        if (Exponent2 == 0) {
            ExponentDifference -= 1;

        } else {
            Significand2 |= 0x4000000000000000ULL;
        }

        RtlpShift64RightJamming(Significand2,
                                ExponentDifference,
                                &Significand2);

        Significand1 |= 0x4000000000000000ULL;
        ResultSignificand = Significand1 - Significand2;
        ResultExponent = Exponent1;
        goto DoubleSubtractEnd;
    }

    if (ExponentDifference < 0) {
        if (Exponent2 == DOUBLE_NAN_EXPONENT) {
            if (Significand2 != 0) {
                return RtlpDoublePropagateNan(Value1, Value2);
            }

            if (Sign != 0) {
                Result.Ulonglong = DOUBLE_PACK(0, DOUBLE_NAN_EXPONENT, 0);

            } else {
                Result.Ulonglong = DOUBLE_PACK(1, DOUBLE_NAN_EXPONENT, 0);
            }

            return Result.Double;
        }

        if (Exponent1 == 0) {
            ExponentDifference += 1;

        } else {
            Significand1 |= 0x4000000000000000ULL;
        }

        RtlpShift64RightJamming(Significand1,
                                -ExponentDifference,
                                &Significand1);

        Significand2 |= 0x4000000000000000ULL;
        ResultSignificand = Significand2 - Significand1;
        ResultExponent = Exponent2;
        Sign ^= 1;
        goto DoubleSubtractEnd;
    }

    if (Exponent1 == DOUBLE_NAN_EXPONENT) {
        if ((Significand1 | Significand2) != 0) {
            return RtlpDoublePropagateNan(Value1, Value2);
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        Result.Ulonglong = DOUBLE_DEFAULT_NAN;
        return Result.Double;
    }

    if (Exponent1 == 0) {
        Exponent1 = 1;
        Exponent2 = 1;
    }

    if (Significand2 < Significand1) {
        ResultSignificand = Significand1 - Significand2;
        ResultExponent = Exponent1;
        goto DoubleSubtractEnd;
    }

    if (Significand1 < Significand2) {
        ResultSignificand = Significand2 - Significand1;
        ResultExponent = Exponent2;
        Sign ^= 1;
        goto DoubleSubtractEnd;
    }

    if (RtlRoundingMode == SoftFloatRoundDown) {
        Result.Ulonglong = DOUBLE_PACK(1, 0, 0);

    } else {
        Result.Ulonglong = DOUBLE_PACK(0, 0, 0);
    }

    return Result.Double;

DoubleSubtractEnd:
    ResultExponent -= 1;
    Result.Double = RtlpNormalizeRoundAndPackDouble(Sign,
                                                    ResultExponent,
                                                    ResultSignificand);

    return Result.Double;
}

VOID
RtlpMultiply64To128 (
    ULONGLONG Value1,
    ULONGLONG Value2,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    )

/*++

Routine Description:

    This routine multiplies two values to obtain a 128-bit product.

Arguments:

    Value1 - Supplies the first value.

    Value2 - Supplies the second value.

    ResultHigh - Supplies a pointer where the high part of the product will be
        returned.

    ResultLow - Supplies a pointer where the low part of the product will be
        returned.

Return Value:

    None.

--*/

{

    ULONGLONG ProductHigh;
    ULONGLONG ProductLow;
    ULONGLONG ProductMiddleA;
    ULONGLONG ProductMiddleB;
    ULONG UlongBitSize;
    ULONG Value1High;
    ULONG Value1Low;
    ULONG Value2High;
    ULONG Value2Low;

    UlongBitSize = sizeof(ULONG) * BITS_PER_BYTE;
    Value1Low = Value1;
    Value1High = Value1 >> UlongBitSize;
    Value2Low = Value2;
    Value2High = Value2 >> UlongBitSize;
    ProductLow = ((ULONGLONG)Value1Low) * Value2Low;
    ProductMiddleA = ((ULONGLONG)Value1Low) * Value2High;
    ProductMiddleB = ((ULONGLONG)Value1High) * Value2Low;
    ProductHigh = ((ULONGLONG) Value1High ) * Value2High;
    ProductMiddleA += ProductMiddleB;
    ProductHigh += (((ULONGLONG)(ProductMiddleA < ProductMiddleB)) <<
                   UlongBitSize) +
                  (ProductMiddleA >> UlongBitSize);

    ProductMiddleA <<= UlongBitSize;
    ProductLow += ProductMiddleA;
    ProductHigh += (ProductLow < ProductMiddleA);
    *ResultLow = ProductLow;
    *ResultHigh = ProductHigh;
    return;
}

ULONGLONG
RtlpEstimateDivide128To64 (
    ULONGLONG DividendHigh,
    ULONGLONG DividendLow,
    ULONGLONG Divisor
    )

/*++

Routine Description:

    This routine returns an approximation of the 64-bit integer quotient
    obtained by dividing the given divisor into the given 128-bit divident.
    The divisor must be at least 2^63. If q is the exact quotient truncated
    toward zero, the approximation returned lies between q and q + 2,
    inclusive. If the exact quotient q is larger than 64 bits, the maximum
    positive 64-bit unsigned integer is returned.

Arguments:

    DividendHigh - Supplies the high part of the dividend.

    DividendLow - Supplies the low part of the dividend.

    Divisor - Supplies the divisor.

Return Value:

    Returns the approximation of the quotient.

--*/

{

    ULONGLONG DivisorHigh;
    ULONGLONG DivisorLow;
    ULONGLONG RemainderHigh;
    ULONGLONG RemainderLow;
    ULONGLONG Result;
    ULONGLONG Term0;
    ULONGLONG Term1;
    ULONG UlongBits;

    if (Divisor <= DividendHigh) {
        return MAX_ULONGLONG;
    }

    UlongBits = sizeof(ULONG) * BITS_PER_BYTE;
    DivisorHigh = Divisor >> UlongBits;
    if ((DivisorHigh << UlongBits) <= DividendHigh) {
        Result = 0xFFFFFFFF00000000ULL;

    } else {
        Result = (DividendHigh / DivisorHigh) << UlongBits;
    }

    RtlpMultiply64To128(Divisor, Result, &Term0, &Term1);
    RtlpSubtract128(DividendHigh,
                    DividendLow,
                    Term0,
                    Term1,
                    &RemainderHigh,
                    &RemainderLow);

    while (((LONGLONG)RemainderHigh) < 0) {
        Result -= (ULONGLONG)MAX_ULONG + 1ULL;
        DivisorLow = Divisor << UlongBits;
        RtlpAdd128(RemainderHigh,
                   RemainderLow,
                   DivisorHigh,
                   DivisorLow,
                   &RemainderHigh,
                   &RemainderLow);
    }

    RemainderHigh = (RemainderHigh << UlongBits) | (RemainderLow >> UlongBits);
    if ((DivisorHigh << UlongBits) <= RemainderHigh) {
        Result |= MAX_ULONG;

    } else {
        Result |= RemainderHigh / DivisorHigh;
    }

    return Result;
}

ULONGLONG
RtlpEstimateSquareRoot32 (
    SHORT ValueExponent,
    ULONG Value
    )

/*++

Routine Description:

    This routine returns an approximation of the square root of the given
    32-bit significand. Considered as an integer, the value must be at least
    2^31. If bit 0 of the exponent is 1, the integer returned approximates
    2^31 * sqrt(Value / 2^31), where the value is considered an integer. If bit
    0 of the exponent is 0, the integer returned approximates
    2^31*sqrt(Value/2^30). In either case, the approximation returned lies
    strictly within +/-2 of the exact value.

Arguments:

    ValueExponent - Supplies the exponent of the value.

    Value - Supplies the significand to estimate the square root of.

Return Value:

    Returns an approximation of the square root.

--*/

{

    ULONG Index;
    ULONG Result;

    Index = (Value >> 27) & 0xF;
    if ((ValueExponent & 0x1) != 0) {
        Result = 0x4000 + (Value >> 17) - RtlSquareRootOddAdjustments[Index];
        Result = ((Value / Result) << 14) + (Result << 15);
        Value >>= 1;

    } else {
        Result = 0x8000 + (Value >> 17) - RtlSquareRootEvenAdjustments[Index];
        Result = Value / Result + Result;
        if (Result >= 0x20000) {
            Result = 0xFFFF8000;

        } else {
            Result = Result << 15;
        }

        if (Result <= Value) {
            return (ULONG)(((LONG)Value) >> 1);
        }
    }

    return ((ULONG)((((LONGLONG)Value) << 31) / Result)) + (Result >> 1);
}

VOID
RtlpAdd128 (
    ULONGLONG Value1High,
    ULONGLONG Value1Low,
    ULONGLONG Value2High,
    ULONGLONG Value2Low,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    )

/*++

Routine Description:

    This routine adds two 128-bit values together. Addition is modulo 2^128,
    so any carry is lost.

Arguments:

    Value1High - Supplies the high 64 bits of the first value.

    Value1Low - Supplies the low 64 bits of the first value.

    Value2High - Supplies the high 64 bits of the second value.

    Value2Low - Supplies the low 64 bits of the second value.

    ResultHigh - Supplies a pointer where the high 64 bits of the result will
        be returned.

    ResultLow - Supplies a pointer where the low 64 bits of the result will be
        returned.

Return Value:

    None.

--*/

{

    ULONGLONG SumLow;

    SumLow = Value1Low + Value2Low;
    *ResultLow = SumLow;
    *ResultHigh = Value1High + Value2High;
    if (SumLow < Value1Low) {
        *ResultHigh += 1;
    }

    return;
}

VOID
RtlpSubtract128 (
    ULONGLONG Value1High,
    ULONGLONG Value1Low,
    ULONGLONG Value2High,
    ULONGLONG Value2Low,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    )

/*++

Routine Description:

    This routine subtracts two 128-bit values from each other.
    Addition is modulo 2^128, so any borrow is lost.

Arguments:

    Value1High - Supplies the high 64 bits of the first value.

    Value1Low - Supplies the low 64 bits of the first value.

    Value2High - Supplies the high 64 bits of the second value.

    Value2Low - Supplies the low 64 bits of the second value.

    ResultHigh - Supplies a pointer where the high 64 bits of the result will
        be returned.

    ResultLow - Supplies a pointer where the low 64 bits of the result will be
        returned.

Return Value:

    None.

--*/

{

    *ResultLow = Value1Low - Value2Low;
    *ResultHigh = Value1High - Value2High;
    if (Value1Low < Value2Low) {
        *ResultHigh -= 1;
    }

    return;
}

float
RtlpFloatPropagateNan (
    FLOAT_PARTS Value1,
    FLOAT_PARTS Value2
    )

/*++

Routine Description:

    This routine takes two float values, one of which is NaN, and returns the
    appropriate NaN result. If with one is a signaling NaN, then the invalid
    exception is raised.

Arguments:

    Value1 - Supplies the first possibly NaN value.

    Value2 - Supplies the second possibly NaN value.

Return Value:

    Returns the appropriate NaN value.

--*/

{

    BOOL Value1IsNan;
    BOOL Value1IsSignalingNan;
    BOOL Value2IsNan;
    BOOL Value2IsSignalingNan;

    Value1IsNan = FLOAT_IS_NAN(Value1);
    Value1IsSignalingNan = FLOAT_IS_SIGNALING_NAN(Value1);
    Value2IsNan = FLOAT_IS_NAN(Value2);
    Value2IsSignalingNan = FLOAT_IS_SIGNALING_NAN(Value2);
    Value1.Ulong |= 1ULL << (FLOAT_EXPONENT_SHIFT - 1);
    Value2.Ulong |= 1ULL << (FLOAT_EXPONENT_SHIFT - 1);
    if ((Value1IsSignalingNan != FALSE) || (Value2IsSignalingNan != FALSE)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
    }

    if (Value1IsSignalingNan != FALSE) {
        if (Value2IsSignalingNan == FALSE) {
            if (Value2IsNan != FALSE) {
                return Value2.Float;
            }

            return Value1.Float;
        }

    } else if (Value1IsNan != FALSE) {
        if ((Value2IsSignalingNan != FALSE) || (Value2IsNan == FALSE)) {
            return Value1.Float;
        }

    } else {
        return Value2.Float;
    }

    if ((ULONG)(Value1.Ulong << 1) < (ULONG)(Value2.Ulong << 1)) {
        return Value2.Float;
    }

    if ((ULONG)(Value2.Ulong << 1) < (ULONG)(Value1.Ulong << 1)) {
        return Value1.Float;
    }

    if (Value1.Ulong < Value2.Ulong) {
        return Value1.Float;
    }

    return Value2.Float;
}

double
RtlpDoublePropagateNan (
    DOUBLE_PARTS Value1,
    DOUBLE_PARTS Value2
    )

/*++

Routine Description:

    This routine takes two double values, one of which is NaN, and returns the
    appropriate NaN result. If with one is a signaling NaN, then the invalid
    exception is raised.

Arguments:

    Value1 - Supplies the first possibly NaN value.

    Value2 - Supplies the second possibly NaN value.

Return Value:

    Returns the appropriate NaN value.

--*/

{

    BOOL Value1IsNan;
    BOOL Value1IsSignalingNan;
    BOOL Value2IsNan;
    BOOL Value2IsSignalingNan;

    Value1IsNan = DOUBLE_IS_NAN(Value1);
    Value1IsSignalingNan = DOUBLE_IS_SIGNALING_NAN(Value1);
    Value2IsNan = DOUBLE_IS_NAN(Value2);
    Value2IsSignalingNan = DOUBLE_IS_SIGNALING_NAN(Value2);
    Value1.Ulonglong |= 1ULL << (DOUBLE_EXPONENT_SHIFT - 1);
    Value2.Ulonglong |= 1ULL << (DOUBLE_EXPONENT_SHIFT - 1);
    if ((Value1IsSignalingNan != FALSE) || (Value2IsSignalingNan != FALSE)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
    }

    if (Value1IsSignalingNan != FALSE) {
        if (Value2IsSignalingNan == FALSE) {
            if (Value2IsNan != FALSE) {
                return Value2.Double;
            }

            return Value1.Double;
        }

    } else if (Value1IsNan != FALSE) {
        if ((Value2IsSignalingNan != FALSE) || (Value2IsNan == FALSE)) {
            return Value1.Double;
        }

    } else {
        return Value2.Double;
    }

    if ((ULONGLONG)(Value1.Ulonglong << 1) <
        (ULONGLONG)(Value2.Ulonglong << 1)) {

        return Value2.Double;
    }

    if ((ULONGLONG)(Value2.Ulonglong << 1) <
        (ULONGLONG)(Value1.Ulonglong << 1)) {

        return Value1.Double;
    }

    if (Value1.Ulonglong < Value2.Ulonglong) {
        return Value1.Double;
    }

    return Value2.Double;
}

VOID
RtlpNormalizeDoubleSubnormal (
    ULONGLONG Significand,
    PSHORT Exponent,
    PULONGLONG Result
    )

/*++

Routine Description:

    This routine normalizes a subnormal double floating point value represented
    by the given denormalized significand. The normalized exponent and
    significand are returned.

Arguments:

    Significand - Supplies the significand to normalize.

    Exponent - Supplies a pointer where the exponent from the normalization
        will be returned.

    Result - Supplies a pointer where the normalized significand will be
        returned.

Return Value:

    None.

--*/

{

    CHAR ShiftCount;

    ShiftCount = RtlCountLeadingZeros64(Significand) - 11;
    *Result = Significand << ShiftCount;
    *Exponent = 1 - ShiftCount;
    return;
}

VOID
RtlpNormalizeFloatSubnormal (
    ULONG Significand,
    PSHORT Exponent,
    PULONG Result
    )

/*++

Routine Description:

    This routine normalizes a subnormal single-precision floating point value
    represented by the given denormalized significand. The normalized exponent
    and significand are returned.

Arguments:

    Significand - Supplies the significand to normalize.

    Exponent - Supplies a pointer where the exponent from the normalization
        will be returned.

    Result - Supplies a pointer where the normalized significand will be
        returned.

Return Value:

    None.

--*/

{

    CHAR ShiftCount;

    ShiftCount = RtlCountLeadingZeros32(Significand) - 8;
    *Result = Significand << ShiftCount;
    *Exponent = 1 - ShiftCount;
    return;
}

COMMON_NAN
RtlpDoubleToCommonNan (
    DOUBLE_PARTS Value
    )

/*++

Routine Description:

    This routine converts a double value to a canonical NaN type. If the value
    is signaling, the invalid exception is raised.

Arguments:

    Value - Supplies the double parts.

Return Value:

    Returns the canonical NaN.

--*/

{

    COMMON_NAN Result;

    if (DOUBLE_IS_SIGNALING_NAN(Value)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
    }

    Result.Sign = (Value.Ulong.High &
                   (DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) >>
                  (DOUBLE_SIGN_BIT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    Result.Low = 0;
    Result.High = Value.Ulonglong << 12;
    return Result;
}

COMMON_NAN
RtlpFloatToCommonNan (
    FLOAT_PARTS Value
    )

/*++

Routine Description:

    This routine converts a float value to a canonical NaN type. If the value
    is signaling, the invalid exception is raised.

Arguments:

    Value - Supplies the double parts.

Return Value:

    Returns the canonical NaN.

--*/

{

    COMMON_NAN Result;

    if (FLOAT_IS_SIGNALING_NAN(Value)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
    }

    Result.Sign = FLOAT_GET_SIGN(Value);
    Result.Low = 0;
    Result.High = (ULONGLONG)Value.Ulong << 41;
    return Result;
}

float
RtlpCommonNanToFloat (
    COMMON_NAN Nan
    )

/*++

Routine Description:

    This routine converts a canonical NaN into a 32 bit floating point number.

Arguments:

    Nan - Supplies the canonical NaN to convert.

Return Value:

    Returns the float representation.

--*/

{

    FLOAT_PARTS Parts;

    Parts.Ulong = (((ULONG)Nan.Sign) << FLOAT_SIGN_BIT_SHIFT) |
                  FLOAT_NAN |
                  (1 << (FLOAT_EXPONENT_SHIFT - 1)) |
                  (Nan.High >> 41);

    return Parts.Float;
}

double
RtlpCommonNanToDouble (
    COMMON_NAN Nan
    )

/*++

Routine Description:

    This routine converts a canonical NaN into a 64 bit floating point number.

Arguments:

    Nan - Supplies the canonical NaN to convert.

Return Value:

    Returns the double representation.

--*/

{

    DOUBLE_PARTS Parts;

    Parts.Ulonglong = ((ULONGLONG)Nan.Sign << DOUBLE_SIGN_BIT_SHIFT) |
                      ((ULONGLONG)NAN_HIGH_WORD << DOUBLE_HIGH_WORD_SHIFT) |
                      (1ULL << (DOUBLE_EXPONENT_SHIFT - 1)) |
                      Nan.High >> 12;

    return Parts.Double;
}
