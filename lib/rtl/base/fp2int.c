/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fp2int.c

Abstract:

    This module implements conversions between floating point numbers and
    integers.

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

LONG
RtlpRoundAndPack32 (
    CHAR SignBit,
    ULONGLONG AbsoluteValue
    );

LONGLONG
RtlpRoundAndPack64 (
    CHAR SignBit,
    ULONGLONG AbsoluteValueHigh,
    ULONGLONG AbsoluteValueLow
    );

VOID
RtlpShift64ExtraRightJamming (
    ULONGLONG ValueInteger,
    ULONGLONG ValueFraction,
    SHORT Count,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define global exception flags.
//

ULONG RtlSoftFloatExceptionFlags = 0;

//
// Define the soft float rounding mode.
//

SOFT_FLOAT_ROUNDING_MODE RtlRoundingMode = SoftFloatRoundNearestEven;

//
// Define the method for detecting very small values.
//

SOFT_FLOAT_DETECT_TININESS RtlTininessDetection =
                                                SoftFloatTininessAfterRounding;

//
// ------------------------------------------------------------------ Functions
//

RTL_API
float
RtlFloatConvertFromInteger32 (
    LONG Integer
    )

/*++

Routine Description:

    This routine converts the given signed 32-bit integer into a float.

Arguments:

    Integer - Supplies the integer to convert to a float.

Return Value:

    Returns the float equivalent to the given integer.

--*/

{

    ULONG AbsoluteInteger;
    FLOAT_PARTS Parts;
    CHAR Sign;

    if (Integer == 0) {
        Parts.Ulong = 0;
        return Parts.Float;
    }

    if (Integer == MIN_LONG) {
        Parts.Ulong = FLOAT_PACK(1, 0x9E, 0);
        return Parts.Float;
    }

    AbsoluteInteger = Integer;
    Sign = 0;
    if (Integer < 0) {
        AbsoluteInteger = -Integer;
        Sign = 1;
    }

    return RtlpNormalizeRoundAndPackFloat(Sign, 0x9C, AbsoluteInteger);
}

RTL_API
float
RtlFloatConvertFromUnsignedInteger32 (
    ULONG Integer
    )

/*++

Routine Description:

    This routine converts the given unsigned 32-bit integer into a float.

Arguments:

    Integer - Supplies the integer to convert to a float.

Return Value:

    Returns the float equivalent to the given integer.

--*/

{

    FLOAT_PARTS Parts;

    if (Integer == 0) {
        Parts.Ulong = 0;
        return Parts.Float;
    }

    if ((Integer & FLOAT_SIGN_BIT) != 0) {
        return RtlpRoundAndPackFloat(0, 0x9D, Integer >> 1);
    }

    return RtlpNormalizeRoundAndPackFloat(0, 0x9C, Integer);
}

RTL_API
float
RtlFloatConvertFromInteger64 (
    LONGLONG Integer
    )

/*++

Routine Description:

    This routine converts the given signed 64-bit integer into a float.

Arguments:

    Integer - Supplies the integer to convert to a float.

Return Value:

    Returns the float equivalent to the given integer.

--*/

{

    ULONGLONG AbsoluteInteger;
    FLOAT_PARTS Parts;
    CHAR ShiftCount;
    CHAR Sign;

    if (Integer == 0) {
        Parts.Ulong = 0;
        return Parts.Float;
    }

    AbsoluteInteger = Integer;
    Sign = 0;
    if (Integer < 0) {
        Sign = 1;
        AbsoluteInteger = -Integer;
    }

    ShiftCount = RtlCountLeadingZeros64(AbsoluteInteger) - 40;
    if (ShiftCount >= 0) {
        Parts.Ulong = FLOAT_PACK(Sign,
                                 0x95 - ShiftCount,
                                 AbsoluteInteger << ShiftCount);

        return Parts.Float;
    }

    ShiftCount += 7;
    if (ShiftCount < 0) {
        RtlpShift64RightJamming(AbsoluteInteger, -ShiftCount, &AbsoluteInteger);

    } else {
        AbsoluteInteger <<= ShiftCount;
    }

    return RtlpRoundAndPackFloat(Sign, 0x9C - ShiftCount, AbsoluteInteger);
}

RTL_API
float
RtlFloatConvertFromUnsignedInteger64 (
    ULONGLONG Integer
    )

/*++

Routine Description:

    This routine converts the given unsigned 64-bit integer into a float.

Arguments:

    Integer - Supplies the unsigned integer to convert to a float.

Return Value:

    Returns the float equivalent to the given integer.

--*/

{

    FLOAT_PARTS Parts;
    CHAR ShiftCount;

    if (Integer == 0) {
        Parts.Ulong = 0;
        return Parts.Float;
    }

    ShiftCount = RtlCountLeadingZeros64(Integer) - 40;
    if (ShiftCount >= 0) {
        Parts.Ulong = FLOAT_PACK(0, 0x95 - ShiftCount, Integer << ShiftCount);
        return Parts.Float;
    }

    ShiftCount += 7;
    if (ShiftCount < 0) {
        RtlpShift64RightJamming(Integer, -ShiftCount, &Integer);

    } else {
        Integer <<= ShiftCount;
    }

    return RtlpRoundAndPackFloat(0, 0x9C - ShiftCount, Integer);
}

RTL_API
LONG
RtlFloatConvertToInteger32 (
    float Float
    )

/*++

Routine Description:

    This routine converts the given float into a signed 32 bit integer.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

{

    SHORT Exponent;
    FLOAT_PARTS Parts;
    SHORT ShiftCount;
    CHAR Sign;
    ULONG Significand;
    ULONGLONG Significand64;

    Parts.Float = Float;
    Significand = FLOAT_GET_SIGNIFICAND(Parts);
    Exponent = FLOAT_GET_EXPONENT(Parts);
    Sign = FLOAT_GET_SIGN(Parts);
    if ((Exponent == FLOAT_NAN_EXPONENT) && (Significand != 0)) {
        Sign = 0;
    }

    if (Exponent != 0) {
        Significand |= 1ULL << FLOAT_EXPONENT_SHIFT;;
    }

    ShiftCount = 0xAF - Exponent;
    Significand64 = Significand;
    Significand64 <<= 32;
    if (ShiftCount > 0) {
        RtlpShift64RightJamming(Significand64, ShiftCount, &Significand64);
    }

    return RtlpRoundAndPack32(Sign, Significand64);
}

RTL_API
LONG
RtlFloatConvertToInteger32RoundToZero (
    float Float
    )

/*++

Routine Description:

    This routine converts the given float into a signed 32 bit integer. It
    always rounds towards zero.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

{

    SHORT Exponent;
    FLOAT_PARTS Parts;
    LONG Result;
    SHORT ShiftCount;
    CHAR Sign;
    ULONG Significand;

    Parts.Float = Float;
    Significand = FLOAT_GET_SIGNIFICAND(Parts);
    Exponent = FLOAT_GET_EXPONENT(Parts);
    Sign = FLOAT_GET_SIGN(Parts);
    ShiftCount = Exponent - 0x9E;
    if (ShiftCount >= 0) {
        if (Parts.Ulong != 0xCF000000) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            if ((Sign == 0) ||
                ((Exponent == FLOAT_NAN_EXPONENT) && (Significand != 0))) {

                return MAX_LONG;
            }
        }

        return MIN_LONG;

    } else if (Exponent < FLOAT_EXPONENT_BIAS) {
        if ((Exponent != 0) || (Significand != 0)) {
            RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
        }

        return 0;
    }

    Significand = (Significand | (1UL << FLOAT_EXPONENT_SHIFT)) << 8;
    Result = Significand >> -ShiftCount;
    if (((ULONG)(Significand << (ShiftCount & 31))) != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    if (Sign != 0) {
        Result = -Result;
    }

    return Result;
}

RTL_API
LONGLONG
RtlFloatConvertToInteger64 (
    float Float
    )

/*++

Routine Description:

    This routine converts the given float into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

{

    SHORT Exponent;
    FLOAT_PARTS Parts;
    SHORT ShiftCount;
    CHAR Sign;
    ULONG Significand;
    ULONGLONG Significand64;
    ULONGLONG Significand64Extra;

    Parts.Float = Float;
    Significand = FLOAT_GET_SIGNIFICAND(Parts);
    Exponent = FLOAT_GET_EXPONENT(Parts);
    Sign = FLOAT_GET_SIGN(Parts);
    ShiftCount = 0xBE - Exponent;
    if (ShiftCount < 0) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if ((Sign == 0) ||
            ((Exponent == FLOAT_NAN_EXPONENT) && (Significand != 0))) {

            return MAX_LONGLONG;
        }

        return MIN_LONGLONG;
    }

    if (Exponent != 0) {
        Significand |= 1UL << FLOAT_EXPONENT_SHIFT;
    }

    Significand64 = Significand;
    Significand64 <<= 40;
    RtlpShift64ExtraRightJamming(Significand64,
                                 0,
                                 ShiftCount,
                                 &Significand64,
                                 &Significand64Extra);

    return RtlpRoundAndPack64(Sign, Significand64, Significand64Extra);
}

RTL_API
LONGLONG
RtlFloatConvertToInteger64RoundToZero (
    float Float
    )

/*++

Routine Description:

    This routine converts the given float into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned. This routine
    always rounds towards zero.

Arguments:

    Float - Supplies the float to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

{

    SHORT Exponent;
    FLOAT_PARTS Parts;
    LONGLONG Result;
    SHORT ShiftCount;
    CHAR Sign;
    ULONG Significand;
    ULONGLONG Significand64;

    Parts.Float = Float;
    Significand = FLOAT_GET_SIGNIFICAND(Parts);
    Exponent = FLOAT_GET_EXPONENT(Parts);
    Sign = FLOAT_GET_SIGN(Parts);
    ShiftCount = Exponent - 0xBE;
    if (ShiftCount >= 0) {
        if (Parts.Ulong != 0xDF000000) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            if ((Sign == 0) ||
                ((Exponent == FLOAT_NAN_EXPONENT) && (Significand != 0))) {

                return MAX_LONGLONG;
            }
        }

        return MIN_LONGLONG;
    }

    if (Exponent < FLOAT_EXPONENT_BIAS) {
        if ((Exponent != 0) || (Significand != 0)) {
            RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
        }

        return 0;
    }

    Significand64 = Significand | (1UL << FLOAT_EXPONENT_SHIFT);
    Significand64 <<= 40;
    Result = Significand64 >> -ShiftCount;
    if (((ULONGLONG)(Significand64 << (ShiftCount & 63))) != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    if (Sign != 0) {
        Result = -Result;
    }

    return Result;
}

RTL_API
double
RtlDoubleConvertFromInteger32 (
    LONG Integer
    )

/*++

Routine Description:

    This routine converts the given signed 32-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

{

    ULONG AbsoluteInteger;
    DOUBLE_PARTS Parts;
    INT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;

    if (Integer == 0) {
        Parts.Ulonglong = 0;
        return Parts.Double;
    }

    AbsoluteInteger = Integer;
    Sign = 0;
    if (Integer < 0) {
        AbsoluteInteger = -Integer;
        Sign = 1;
    }

    ShiftCount = RtlCountLeadingZeros32(AbsoluteInteger) + 21;
    Significand = AbsoluteInteger;
    Parts.Ulonglong = DOUBLE_PACK(Sign,
                                  0x432 - ShiftCount,
                                  Significand << ShiftCount);

    return Parts.Double;
}

RTL_API
double
RtlDoubleConvertFromUnsignedInteger32 (
    ULONG Integer
    )

/*++

Routine Description:

    This routine converts the given unsigned 32-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

{

    DOUBLE_PARTS Parts;
    INT ShiftCount;
    ULONGLONG Significand;

    if (Integer == 0) {
        Parts.Ulonglong = 0;
        return Parts.Double;
    }

    ShiftCount = RtlCountLeadingZeros32(Integer) + 21;
    Significand = Integer;
    Parts.Ulonglong = DOUBLE_PACK(0,
                                  0x432 - ShiftCount,
                                  Significand << ShiftCount);

    return Parts.Double;
}

RTL_API
double
RtlDoubleConvertFromInteger64 (
    LONGLONG Integer
    )

/*++

Routine Description:

    This routine converts the given signed 64-bit integer into a double.

Arguments:

    Integer - Supplies the integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

{

    DOUBLE_PARTS Parts;
    CHAR Sign;

    if (Integer == 0) {
        Parts.Ulonglong = 0;
        return Parts.Double;
    }

    if (Integer == (LONGLONG)MIN_LONGLONG) {
        Parts.Ulonglong = DOUBLE_PACK(1, 0x43E, 0);
        return Parts.Double;
    }

    Sign = 0;
    if (Integer < 0) {
        Sign = 1;
        Integer = -Integer;
    }

    return RtlpNormalizeRoundAndPackDouble(Sign, 0x43C, Integer);
}

RTL_API
double
RtlDoubleConvertFromUnsignedInteger64 (
    ULONGLONG Integer
    )

/*++

Routine Description:

    This routine converts the given unsigned 64-bit integer into a double.

Arguments:

    Integer - Supplies the unsigned integer to convert to a double.

Return Value:

    Returns the double equivalent to the given integer.

--*/

{

    DOUBLE_PARTS Parts;
    SHORT ShiftCount;

    if (Integer == 0) {
        Parts.Ulonglong = 0;
        return Parts.Double;
    }

    ShiftCount = RtlCountLeadingZeros64(Integer) - 1;
    if (ShiftCount < 0) {
        RtlpShift64RightJamming(Integer, -ShiftCount, &Integer);

    } else {
        Integer <<= ShiftCount;
    }

    return RtlpRoundAndPackDouble(0, 0x43C - ShiftCount, Integer);
}

RTL_API
LONG
RtlDoubleConvertToInteger32 (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a signed 32 bit integer.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    INT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if ((Exponent == DOUBLE_NAN_EXPONENT) && (Significand != 0)) {
        Sign = 0;
    }

    if (Exponent != 0) {
        Significand |= 1ULL << DOUBLE_EXPONENT_SHIFT;;
    }

    ShiftCount = 0x42C - Exponent;
    if (ShiftCount > 0) {
        RtlpShift64RightJamming(Significand, ShiftCount, &Significand);
    }

    return RtlpRoundAndPack32(Sign, Significand);
}

RTL_API
LONG
RtlDoubleConvertToInteger32RoundToZero (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a signed 32 bit integer. It
    always rounds towards zero.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    LONG Result;
    ULONGLONG SavedSignificand;
    SHORT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if (Exponent > 0x41E) {
        if ((Exponent == DOUBLE_NAN_EXPONENT) && (Significand != 0)) {
            Sign = 0;
        }

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if (Sign != 0) {
            return MIN_LONG;

        } else {
            return MAX_LONG;
        }

    } else if (Exponent < DOUBLE_EXPONENT_BIAS) {
        if ((Exponent != 0) || (Significand != 0)) {
            RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
        }

        return 0;
    }

    Significand |= 1ULL << DOUBLE_EXPONENT_SHIFT;
    ShiftCount = 0x433 - Exponent;
    SavedSignificand = Significand;
    Significand >>= ShiftCount;
    Result = Significand;
    if (Sign != 0) {
        Result = -Result;
    }

    //
    // Handle signed overflow, assuming it wraps (which actually it doesn't
    // have to do in C).
    //

    if ((Result < 0) ^ Sign) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if (Sign != 0) {
            return MIN_LONG;

        } else {
            return MAX_LONG;
        }
    }

    if ((Significand << ShiftCount) != SavedSignificand) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    return Result;
}

RTL_API
LONGLONG
RtlDoubleConvertToInteger64 (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded according to the current rounding mode.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    SHORT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;
    ULONGLONG SignificandExtra;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if (Exponent != 0) {
        Significand |= 1ULL << DOUBLE_EXPONENT_SHIFT;;
    }

    ShiftCount = 0x433 - Exponent;
    if (ShiftCount <= 0) {
        if (Exponent > 0x43E) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            if ((Sign == 0) ||
                ((Exponent == DOUBLE_NAN_EXPONENT) &&
                 (Significand != (1ULL << DOUBLE_EXPONENT_SHIFT)))) {

                return MAX_LONGLONG;
            }

            return MIN_LONGLONG;
        }

        SignificandExtra = 0;
        Significand <<= -ShiftCount;

    } else {
        RtlpShift64ExtraRightJamming(Significand,
                                     0,
                                     ShiftCount,
                                     &Significand,
                                     &SignificandExtra);
    }

    return RtlpRoundAndPack64(Sign, Significand, SignificandExtra);
}

RTL_API
LONGLONG
RtlDoubleConvertToInteger64RoundToZero (
    double Double
    )

/*++

Routine Description:

    This routine converts the given double into a signed 64 bit integer. If the
    value is NaN, then the largest positive integer is returned. This routine
    always rounds towards zero.

Arguments:

    Double - Supplies the double to convert.

Return Value:

    Returns the integer, rounded towards zero.

--*/

{

    SHORT Exponent;
    DOUBLE_PARTS Parts;
    LONGLONG Result;
    SHORT ShiftCount;
    CHAR Sign;
    ULONGLONG Significand;

    Parts.Double = Double;
    Significand = DOUBLE_GET_SIGNIFICAND(Parts);
    Exponent = DOUBLE_GET_EXPONENT(Parts);
    Sign = DOUBLE_GET_SIGN(Parts);
    if (Exponent != 0) {
        Significand |= 1ULL << DOUBLE_EXPONENT_SHIFT;
    }

    ShiftCount = Exponent - 0x433;
    if (ShiftCount >= 0) {
        if (Exponent >= 0x43E) {
            if (Parts.Ulonglong != 0xC3E0000000000000ULL) {
                RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
                if ((Sign == 0) ||
                    ((Exponent == DOUBLE_NAN_EXPONENT) &&
                     (Significand != (1ULL << DOUBLE_EXPONENT_SHIFT)))) {

                    return MAX_LONGLONG;
                }
            }

            return MIN_LONGLONG;
        }

        Result = Significand << ShiftCount;

    } else {
        if (Exponent < (DOUBLE_EXPONENT_BIAS - 1)) {
            if ((Exponent != 0) || (Significand != 0)) {
                RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
            }

            return 0;
        }

        Result = Significand >> (-ShiftCount);
        if ((LONGLONG)(Significand << (ShiftCount & 0x3F))) {
            RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
        }
    }

    if (Sign != 0) {
        Result = -Result;
    }

    return Result;
}

VOID
RtlpSoftFloatRaise (
    ULONG Flags
    )

/*++

Routine Description:

    This routine raises the given conditions in the soft float implementation.

Arguments:

    Flags - Supplies the flags to raise.

Return Value:

    None.

--*/

{

    RtlSoftFloatExceptionFlags |= Flags;
    return;
}

float
RtlpRoundAndPackFloat (
    CHAR SignBit,
    SHORT Exponent,
    ULONG Significand
    )

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded floating point value from that input. Overflow and
    underflow can be raised here.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        30 and 29, which is 7 bits to the left of its usual location. The
        shifted exponent must be normalized or smaller. If the significand is
        not normalized, the exponent must be 0. In that case, the result
        returned is a subnormal number, and it must not require rounding. In
        the normal case wehre the significand is normalized, the exponent must
        be one less than the true floating point exponent.

Return Value:

    Returns the float representation of the given components.

--*/

{

    BOOL IsTiny;
    FLOAT_PARTS Result;
    CHAR RoundBits;
    CHAR RoundIncrement;
    SOFT_FLOAT_ROUNDING_MODE RoundingMode;
    BOOL RoundNearestEven;

    RoundingMode = RtlRoundingMode;
    RoundNearestEven = FALSE;
    if (RoundingMode == SoftFloatRoundNearestEven) {
        RoundNearestEven = TRUE;
    }

    RoundIncrement = 0x40;
    if (RoundNearestEven == FALSE) {
        if (RoundingMode == SoftFloatRoundToZero) {
            RoundIncrement = 0;

        } else {
            RoundIncrement = 0x7F;
            if (SignBit != 0) {
                if (RoundingMode == SoftFloatRoundUp) {
                    RoundIncrement = 0;
                }

            } else {
                if (RoundingMode == SoftFloatRoundDown) {
                    RoundIncrement = 0;
                }
            }
        }
    }

    RoundBits = Significand & 0x7F;
    if ((USHORT)Exponent >= 0xFD) {
        if ((Exponent > 0xFD) ||
            ((Exponent == 0xFD) &&
             ((LONG)(Significand + RoundIncrement) < 0))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_OVERFLOW | SOFT_FLOAT_INEXACT);
            Result.Ulong = FLOAT_PACK(SignBit, 0xFF, 0);
            if (RoundIncrement == 0) {
                Result.Ulong -= 1;
            }

            return Result.Float;
        }

        if (Exponent < 0) {
            IsTiny = (RtlTininessDetection ==
                      SoftFloatTininessBeforeRounding) ||
                     (Exponent < -1) ||
                     (Significand + RoundIncrement < 0x80000000);

            RtlpShift32RightJamming(Significand, -Exponent, &Significand);
            Exponent = 0;
            RoundBits = Significand & 0x7F;
            if ((IsTiny != FALSE) && (RoundBits != 0)) {
                RtlpSoftFloatRaise(SOFT_FLOAT_UNDERFLOW);
            }
        }
    }

    if (RoundBits != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    Significand = (Significand + RoundIncrement ) >> 7;
    if (((RoundBits ^ 0x40) == 0) && (RoundNearestEven != FALSE)) {
        Significand &= ~0x1;
    }

    if (Significand == 0) {
        Exponent = 0;
    }

    Result.Ulong = FLOAT_PACK(SignBit, Exponent, Significand);
    return Result.Float;
}

float
RtlpNormalizeRoundAndPackFloat (
    CHAR SignBit,
    SHORT Exponent,
    ULONG Significand
    )

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded single floating point value from that input. Overflow and
    underflow can be raised here. This routine is very similar to the "round
    and pack float" routine except that the significand does not have to be
    normalized. Bit 31 of the significand must be zero, and the exponent must
    be one less than the true floating point exponent.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        30 and 29, which is 7 bits to the left of its usual location.

Return Value:

    Returns the float representation of the given components.

--*/

{

    double Result;
    INT ShiftCount;

    ShiftCount = RtlCountLeadingZeros32(Significand) - 1;
    Result = RtlpRoundAndPackFloat(SignBit,
                                   Exponent - ShiftCount,
                                   Significand << ShiftCount);

    return Result;
}

double
RtlpRoundAndPackDouble (
    CHAR SignBit,
    SHORT Exponent,
    ULONGLONG Significand
    )

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded double floating point value from that input. Overflow and
    underflow can be raised here.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        62 and 61, which is 10 bits to the left of its usual location. The
        shifted exponent must be normalized or smaller. If the significand is
        not normalized, the exponent must be 0. In that case, the result
        returned is a subnormal number, and it must not require rounding. In
        the normal case where the significand is normalized, the exponent must
        be one less than the true floating point exponent.

Return Value:

    Returns the double representation of the given components.

--*/

{

    BOOL IsTiny;
    DOUBLE_PARTS Result;
    SHORT RoundBits;
    SHORT RoundIncrement;
    SOFT_FLOAT_ROUNDING_MODE RoundingMode;
    BOOL RoundNearestEven;

    RoundingMode = RtlRoundingMode;
    RoundNearestEven = FALSE;
    if (RoundingMode == SoftFloatRoundNearestEven) {
        RoundNearestEven = TRUE;
    }

    RoundIncrement = 0x200;
    if (RoundNearestEven == FALSE) {
        if (RoundingMode == SoftFloatRoundToZero) {
            RoundIncrement = 0;

        } else {
            RoundIncrement = 0x3FF;
            if (SignBit != 0) {
                if (RoundingMode == SoftFloatRoundUp) {
                    RoundIncrement = 0;
                }

            } else {
                if (RoundingMode == SoftFloatRoundDown) {
                    RoundIncrement = 0;
                }
            }
        }
    }

    RoundBits = Significand & 0x3FF;
    if ((USHORT)Exponent >= 0x7FD) {
        if ((Exponent > 0x7FD) ||
            ((Exponent == 0x7FD) &&
             ((LONGLONG)(Significand + RoundIncrement) < 0))) {

            RtlpSoftFloatRaise(SOFT_FLOAT_OVERFLOW | SOFT_FLOAT_INEXACT);
            Result.Ulonglong = DOUBLE_PACK(SignBit, 0x7FF, 0);
            if (RoundIncrement == 0) {
                Result.Ulonglong -= 1;
            }

            return Result.Double;
        }

        if (Exponent < 0) {
            IsTiny = (RtlTininessDetection ==
                      SoftFloatTininessBeforeRounding) ||
                     (Exponent < -1) ||
                     (Significand + RoundIncrement < 0x8000000000000000ULL);

            RtlpShift64RightJamming(Significand, -Exponent, &Significand);
            Exponent = 0;
            RoundBits = Significand & 0x3FF;
            if ((IsTiny != FALSE) && (RoundBits != 0)) {
                RtlpSoftFloatRaise(SOFT_FLOAT_UNDERFLOW);
            }
        }
    }

    if (RoundBits != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    Significand = (Significand + RoundIncrement) >> 10;
    Significand &= ~(((RoundBits ^ 0x200) == 0) & (RoundNearestEven != FALSE));
    if (Significand == 0) {
        Exponent = 0;
    }

    Result.Ulonglong = DOUBLE_PACK(SignBit, Exponent, Significand);
    return Result.Double;
}

double
RtlpNormalizeRoundAndPackDouble (
    CHAR SignBit,
    SHORT Exponent,
    ULONGLONG Significand
    )

/*++

Routine Description:

    This routine takes a sign, exponent, and significand and creates the
    proper rounded double floating point value from that input. Overflow and
    underflow can be raised here. This routine is very similar to the "round
    and pack double" routine except that the significand does not have to be
    normalized. Bit 63 of the significand must be zero, and the exponent must
    be one less than the true floating point exponent.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    Exponent - Supplies the exponent for the value.

    Significand - Supplies the significand with its binary point between bits
        62 and 61, which is 10 bits to the left of its usual location.

Return Value:

    Returns the double representation of the given components.

--*/

{

    double Result;
    INT ShiftCount;

    ShiftCount = RtlCountLeadingZeros64(Significand) - 1;
    Result = RtlpRoundAndPackDouble(SignBit,
                                    Exponent - ShiftCount,
                                    Significand << ShiftCount);

    return Result;
}

VOID
RtlpShift32RightJamming (
    ULONG Value,
    SHORT Count,
    PULONG Result
    )

/*++

Routine Description:

    This routine shifts the given value right by the requested number of bits.
    If any bits are shifted off the right, the least significant bit is set.
    The imagery is that the bits get "jammed" on the end as they try to fall
    off.

Arguments:

    Value - Supplies the value to shift.

    Count - Supplies the number of bits to shift by.

    Result - Supplies a pointer where the result will be stored.

Return Value:

    None.

--*/

{

    ULONG ShiftedValue;

    if (Count == 0) {
        ShiftedValue = Value;

    } else if (Count < 32) {
        ShiftedValue = (Value >> Count) | ((Value << ((-Count) & 31)) != 0);

    } else {
        ShiftedValue = 0;
        if (Value != 0) {
            ShiftedValue = 1;
        }
    }

    *Result = ShiftedValue;
    return;
}

VOID
RtlpShift64RightJamming (
    ULONGLONG Value,
    SHORT Count,
    PULONGLONG Result
    )

/*++

Routine Description:

    This routine shifts the given value right by the requested number of bits.
    If any bits are shifted off the right, the least significant bit is set.
    The imagery is that the bits get "jammed" on the end as they try to fall
    off.

Arguments:

    Value - Supplies the value to shift.

    Count - Supplies the number of bits to shift by.

    Result - Supplies a pointer where the result will be stored.

Return Value:

    None.

--*/

{

    ULONGLONG ShiftedValue;

    if (Count == 0) {
        ShiftedValue = Value;

    } else if (Count < (sizeof(ULONGLONG) * BITS_PER_BYTE)) {
        ShiftedValue = (Value >> Count) | ((Value << ((-Count) & 63)) != 0);

    } else {
        ShiftedValue = 0;
        if (Value != 0) {
            ShiftedValue = 1;
        }
    }

    *Result = ShiftedValue;
}

//
// --------------------------------------------------------- Internal Functions
//

LONG
RtlpRoundAndPack32 (
    CHAR SignBit,
    ULONGLONG AbsoluteValue
    )

/*++

Routine Description:

    This routine takes a 64 bit fixed point value with binary point between
    bits 6 and 7 and returns the properly rounded 32 bit integer corresponding
    to the input. If the sign is one, the input is negated before being
    converted. If the fixed-point input is too large, the invalid exception is
    raised and the largest positive or negative integer is returned.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    AbsoluteValue - Supplies the value to convert.

Return Value:

    Returns the 32 bit integer representation of the fixed point number.

--*/

{

    ULONGLONG Mask;
    LONG Result;
    CHAR RoundBits;
    CHAR RoundIncrement;
    SOFT_FLOAT_ROUNDING_MODE RoundingMode;
    BOOL RoundNearestEven;

    RoundingMode = RtlRoundingMode;
    RoundNearestEven = FALSE;
    if (RoundingMode == SoftFloatRoundNearestEven) {
        RoundNearestEven = TRUE;
    }

    RoundIncrement = sizeof(ULONGLONG) * BITS_PER_BYTE;
    if (RoundNearestEven == FALSE) {
        if (RoundingMode == SoftFloatRoundToZero) {
            RoundIncrement = 0;

        } else {
            RoundIncrement = 0x7F;
            if (SignBit != 0) {
                if (RoundingMode == SoftFloatRoundUp) {
                    RoundIncrement = 0;
                }

            } else {
                if (RoundingMode == SoftFloatRoundDown) {
                    RoundIncrement = 0;
                }
            }
        }
    }

    //
    // Add the rounding amount and remove the fixed point.
    //

    RoundBits = AbsoluteValue & 0x7F;
    AbsoluteValue = (AbsoluteValue + RoundIncrement) >> 7;
    Mask = MAX_ULONGLONG;
    if (((RoundBits ^ (sizeof(ULONGLONG) * BITS_PER_BYTE)) == 0) &&
        (RoundNearestEven != FALSE)) {

        Mask ^= 0x1;
    }

    AbsoluteValue &= Mask;
    Result = AbsoluteValue;
    if (SignBit != 0) {
        Result = -Result;
    }

    if (((AbsoluteValue >> (sizeof(ULONG) * BITS_PER_BYTE)) != 0) ||
        ((Result != 0) && ((Result < 0) ^ SignBit))) {

        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if (SignBit != 0) {
            return MIN_LONG;
        }

        return MAX_LONG;
    }

    if (RoundBits != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    return Result;
}

LONGLONG
RtlpRoundAndPack64 (
    CHAR SignBit,
    ULONGLONG AbsoluteValueHigh,
    ULONGLONG AbsoluteValueLow
    )

/*++

Routine Description:

    This routine takes a 128-bit fixed point value with binary point between
    bits 63 and 64 and returns the properly rounded 64 bit integer corresponding
    to the input. If the sign is one, the input is negated before being
    converted. If the fixed-point input is too large, the invalid exception is
    raised and the largest positive or negative integer is returned.

Arguments:

    SignBit - Supplies a non-zero value if the value should be negated before
        being converted.

    AbsoluteValueHigh - Supplies the high part of the absolute value.

    AbsoluteValueLow - Supplies the low part of the absolute value.

Return Value:

    Returns the 32 bit integer representation of the fixed point number.

--*/

{

    CHAR Increment;
    LONGLONG Result;
    SOFT_FLOAT_ROUNDING_MODE RoundingMode;
    BOOL RoundNearestEven;

    RoundingMode = RtlRoundingMode;
    RoundNearestEven = FALSE;
    if (RoundingMode == SoftFloatRoundNearestEven) {
        RoundNearestEven = TRUE;
    }

    Increment = 0;
    if ((LONGLONG)AbsoluteValueLow < 0) {
        Increment = 1;
    }

    if (RoundNearestEven == FALSE) {
        if (RoundingMode == SoftFloatRoundToZero) {
            Increment = 0;

        } else {
            Increment = 0;
            if (SignBit != 0) {
                if ((RoundingMode == SoftFloatRoundDown) &&
                    (AbsoluteValueLow != 0)) {

                    Increment = 1;
                }

            } else {
                if ((RoundingMode == SoftFloatRoundUp) &&
                    (AbsoluteValueLow != 0)) {

                    Increment = 1;
                }
            }
        }
    }

    if (Increment != 0) {
        AbsoluteValueHigh += 1;
        if (AbsoluteValueHigh == 0) {
            RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
            if (SignBit != 0) {
                return MIN_LONGLONG;
            }

            return MAX_LONGLONG;
        }

        if (((ULONGLONG)(AbsoluteValueLow << 1) == 0) &&
            (RoundNearestEven != FALSE)) {

            AbsoluteValueHigh &= ~0x1ULL;
        }
    }

    Result = AbsoluteValueHigh;
    if (SignBit != 0) {
        Result = - Result;
    }

    if ((Result != 0) && (((Result < 0) ^ SignBit) != 0)) {
        RtlpSoftFloatRaise(SOFT_FLOAT_INVALID);
        if (SignBit != 0) {
            return MIN_LONGLONG;
        }

        return MAX_LONGLONG;
    }

    if (AbsoluteValueLow != 0) {
        RtlSoftFloatExceptionFlags |= SOFT_FLOAT_INEXACT;
    }

    return Result;
}

VOID
RtlpShift64ExtraRightJamming (
    ULONGLONG ValueInteger,
    ULONGLONG ValueFraction,
    SHORT Count,
    PULONGLONG ResultHigh,
    PULONGLONG ResultLow
    )

/*++

Routine Description:

    This routine shifts the given 128-bit value right by the requested number
    of bits plus 64. The shifted result is at most 64 non-zero bits. The bits
    shifted off form a second 64-bit result as follows: The last bit shifted
    off is the most significant bit of the extra result, and all other 63 bits
    of the extra result are all zero if and only if all but the last bits
    shifted off were all zero.

Arguments:

    ValueInteger - Supplies the integer portion of the fixed point 128 bit
        value. The binary point is between the integer and the fractional parts.

    ValueFraction - Supplies the lower portion of the fixed point 128 bit
        value.

    Count - Supplies the number of bits to shift by.

    ResultHigh - Supplies a pointer where the result will be returned.

    ResultLow - Supplies a pointer where the lower part of the result will be
        returned.

Return Value:

    None.

--*/

{

    INT NegativeCount;
    ULONGLONG ShiftedValueHigh;
    ULONGLONG ShiftedValueLow;

    NegativeCount = (-Count) & 63;
    if (Count == 0) {
        ShiftedValueLow = ValueFraction;
        ShiftedValueHigh = ValueInteger;

    } else if (Count < 64) {
        ShiftedValueLow = (ValueInteger << NegativeCount) |
                          (ValueFraction != 0);

        ShiftedValueHigh = ValueInteger >> Count;

    } else {
        if (Count == 64) {
            ShiftedValueLow = ValueInteger | (ValueFraction != 0);

        } else {
            ShiftedValueLow = ((ValueInteger | ValueFraction) != 0);
        }

        ShiftedValueHigh = 0;
    }

    *ResultLow = ShiftedValueLow;
    *ResultHigh = ShiftedValueHigh;
    return;
}
