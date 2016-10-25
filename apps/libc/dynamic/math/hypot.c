/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hypot.c

Abstract:

    This module implements the hypotenuse of a right-angled triangle with the
    two given sides.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 18-Oct-2013

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

#define HYPOTENUSE_RATIO_THRESHOLD 0x03C00000
#define HYPOTENUSE_UPPER_THRESHOLD_HIGH 0x5F300000
#define HYPOTENUSE_SCALE_DOWN_HIGH 0x25800000
#define HYPOTENUSE_SCALE_DOWN_EXPONENT 600
#define HYPOTENUSE_LOWER_THRESHOLD_HIGH 0x20B00000
#define HYPOTENUSE_LENGTH2_BIG_HIGH 0x7FD00000
#define HYPOTENUSE_LENGTH2_BIG_EXPONENT 1022
#define HYPOTENUSE_SCALE_UP_HIGH 0x25800000
#define HYPOTENUSE_SCALE_UP_EXPONENT 600

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
hypot (
    double Length,
    double Width
    )

/*++

Routine Description:

    This routine computes the square root of a2 + b2 without undue overflow
    or underflow.

Arguments:

    Length - Supplies the length of the triangle.

    Width - Supplies the width of the triangle.

Return Value:

    Returns the hypotenuse of the triangle.

--*/

{

    LONG AbsoluteLengthHigh;
    LONG AbsoluteWidthHigh;
    DOUBLE_PARTS DoubleParts;
    LONG Exponent;
    double HigherValue;
    double Length2;
    double Length2Remainder;
    ULONG Low;
    double LowerValue;
    double Result;
    LONG Swap;
    double WidthChopped;
    double WidthRemainder;

    //
    // Method:
    // If z = x * x + y * y has less error than sqrt(2) / 2 ULP, then sqrt(z)
    // has less than 1 ULP error.
    //
    // So, compute sqrt(x * x + y * y) with some care as follows to keep the
    // error below 1 ULP.
    // Assume x > y > 0:
    // (If possible, set round-to-nearest)
    // 1. If x > 2y, use
    //    x1 * x1 + (y * y + (x2 * (x + x1))) for x * x + y * y
    //    where x1 = x with the lower 32 bits cleared, x2 = x - x1.
    //    Otherwise:
    // 2. If x <= 2y, use
    //    Length2 * WidthChopped = ((x - y) * (x - y) +
    //    (Length2 * WidthRemainder + Length2Remainder * y))
    //    where Length2 = 2x with the lower 32 bits cleared, and
    //    Length2Remainder = 2x - Length2.
    //    WidthChopped = y with the lower 32 bits chopped off, and
    //    WidthRemainder = y - WidthChopped.
    //
    // Note that scaling may be necessary if some argument is too large or too
    // tiny.
    //
    // Special cases:
    // hypot(x, y) is INF if x or y is +INF or -INF.
    // hypot(x, y) is NAN if x or y is NAN.
    //

    DoubleParts.Double = Length;
    AbsoluteLengthHigh = DoubleParts.Ulong.High &
                         (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    DoubleParts.Double = Width;
    AbsoluteWidthHigh = DoubleParts.Ulong.High &
                        (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);

    if (AbsoluteWidthHigh > AbsoluteLengthHigh) {
        HigherValue = Width;
        LowerValue = Length;
        Swap = AbsoluteLengthHigh;
        AbsoluteLengthHigh = AbsoluteWidthHigh;
        AbsoluteWidthHigh = Swap;

    } else {
        HigherValue = Length;
        LowerValue = Width;
    }

    HigherValue = fabs(HigherValue);
    LowerValue = fabs(LowerValue);

    //
    // Return the sum if the ratio of length to width is greater than 2^60.
    //

    if ((AbsoluteLengthHigh - AbsoluteWidthHigh) > HYPOTENUSE_RATIO_THRESHOLD) {
        return HigherValue + LowerValue;
    }

    //
    // Handle a really big value, > 2^500.
    //

    Exponent = 0;
    if (AbsoluteLengthHigh > HYPOTENUSE_UPPER_THRESHOLD_HIGH) {

        //
        // Handle Infinity or NaN.
        //

        if (AbsoluteLengthHigh >= NAN_HIGH_WORD) {

            //
            // Use the original argument order if the result is NaN.
            //

            Result = fabs(Length + 0.0) - fabs(Width + 0.0);
            DoubleParts.Double = HigherValue;
            Low = DoubleParts.Ulong.Low;
            if (((AbsoluteLengthHigh & DOUBLE_HIGH_VALUE_MASK) | Low) == 0) {
                Result = HigherValue;
            }

            DoubleParts.Double = LowerValue;
            Low = DoubleParts.Ulong.Low;
            if (((AbsoluteWidthHigh ^ NAN_HIGH_WORD) | Low) == 0) {
                Result = LowerValue;
            }

            return Result;
        }

        //
        // Scale the values by 2^-600.
        //

        AbsoluteLengthHigh -= HYPOTENUSE_SCALE_DOWN_HIGH;
        AbsoluteWidthHigh -= HYPOTENUSE_SCALE_DOWN_HIGH;
        Exponent += HYPOTENUSE_SCALE_DOWN_EXPONENT;
        DoubleParts.Double = HigherValue;
        DoubleParts.Ulong.High = AbsoluteLengthHigh;
        HigherValue = DoubleParts.Double;
        DoubleParts.Double = LowerValue;
        DoubleParts.Ulong.High = AbsoluteWidthHigh;
        LowerValue = DoubleParts.Double;
    }

    //
    // Handle a really small value, < 2^-500.
    //

    if (AbsoluteWidthHigh < HYPOTENUSE_LOWER_THRESHOLD_HIGH) {

        //
        // Handle a subnormal lower value, or zero.
        //

        if (AbsoluteWidthHigh <= DOUBLE_HIGH_VALUE_MASK) {
            DoubleParts.Double = LowerValue;
            Low = DoubleParts.Ulong.Low;
            if ((AbsoluteWidthHigh | Low) == 0) {
                return HigherValue;
            }

            //
            // Set Length2 to 2^1022.
            //

            DoubleParts.Double = 0;
            DoubleParts.Ulong.High = HYPOTENUSE_LENGTH2_BIG_HIGH;
            Length2 = DoubleParts.Double;
            LowerValue *= Length2;
            HigherValue *= Length2;
            Exponent -= HYPOTENUSE_LENGTH2_BIG_EXPONENT;

        //
        // Scale the values by 2^600.
        //

        } else {
            AbsoluteLengthHigh += HYPOTENUSE_SCALE_UP_HIGH;
            AbsoluteWidthHigh += HYPOTENUSE_SCALE_UP_HIGH;
            Exponent -= HYPOTENUSE_SCALE_UP_EXPONENT;
            DoubleParts.Double = HigherValue;
            DoubleParts.Ulong.High = AbsoluteLengthHigh;
            HigherValue = DoubleParts.Double;
            DoubleParts.Double = LowerValue;
            DoubleParts.Ulong.High = AbsoluteWidthHigh;
            LowerValue = DoubleParts.Double;
        }
    }

    //
    // Handle medium sized values.
    //

    Result = HigherValue - LowerValue;
    if (Result > LowerValue) {
        DoubleParts.Double = 0;
        DoubleParts.Ulong.High = AbsoluteLengthHigh;
        Length2 = DoubleParts.Double;
        Length2Remainder = HigherValue - Length2;
        Result = sqrt(Length2 * Length2 -
                      (LowerValue * (-LowerValue) - Length2Remainder *
                       (HigherValue + Length2)));

    } else {
        HigherValue = HigherValue + HigherValue;
        DoubleParts.Double = 0;
        DoubleParts.Ulong.High = AbsoluteWidthHigh;
        WidthChopped = DoubleParts.Double;
        WidthRemainder = LowerValue - WidthChopped;
        DoubleParts.Double = 0;
        DoubleParts.Ulong.High = AbsoluteLengthHigh + DOUBLE_VALUE_MASK + 1;
        Length2 = DoubleParts.Double;
        Length2Remainder = HigherValue - Length2;
        Result = sqrt(Length2 * WidthChopped -
                      (Result * (-Result) - (Length2 * WidthRemainder +
                                             Length2Remainder * LowerValue)));
    }

    if (Exponent != 0) {
        DoubleParts.Double = 1.0;
        DoubleParts.Ulong.High +=
                  Exponent << (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

        Length2 = DoubleParts.Double;
        return Length2 * Result;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

