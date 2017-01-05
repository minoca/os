/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hypotf.c

Abstract:

    This module implements the hypotenuse of a right-angled triangle with the
    two given sides.

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

#define HYPOTENUSE_FLOAT_RATIO_THRESHOLD 0xf000000
#define HYPOTENUSE_FLOAT_UPPER_THRESHOLD 0x58800000
#define HYPOTENUSE_FLOAT_SCALE_DOWN 0x22000000
#define HYPOTENUSE_FLOAT_SCALE_DOWN_EXPONENT 68
#define HYPOTENUSE_FLOAT_LOWER_THRESHOLD 0x26800000
#define HYPOTENUSE_FLOAT_LENGTH2_BIG_HIGH 0x7e800000
#define HYPOTENUSE_FLOAT_LENGTH2_BIG_EXPONENT 126
#define HYPOTENUSE_FLOAT_SCALE_UP 0x22000000
#define HYPOTENUSE_FLOAT_SCALE_UP_EXPONENT 68

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
float
hypotf (
    float Length,
    float Width
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

    LONG AbsoluteLength;
    LONG AbsoluteWidth;
    FLOAT_PARTS FloatParts;
    LONG Exponent;
    float HigherValue;
    float Length2;
    float Length2Remainder;
    float LowerValue;
    float Result;
    LONG Swap;
    float WidthChopped;
    float WidthRemainder;

    FloatParts.Float = Length;
    AbsoluteLength = FloatParts.Ulong & ~FLOAT_SIGN_BIT;
    FloatParts.Float = Width;
    AbsoluteWidth = FloatParts.Ulong & ~FLOAT_SIGN_BIT;
    if (AbsoluteWidth > AbsoluteLength) {
        HigherValue = Width;
        LowerValue = Length;
        Swap = AbsoluteLength;
        AbsoluteLength = AbsoluteWidth;
        AbsoluteWidth = Swap;

    } else {
        HigherValue = Length;
        LowerValue = Width;
    }

    HigherValue = fabsf(HigherValue);
    LowerValue = fabsf(LowerValue);

    //
    // Return the sum if the ratio of length to width is greater than 2^60.
    //

    if ((AbsoluteLength - AbsoluteWidth) > HYPOTENUSE_FLOAT_RATIO_THRESHOLD) {
        return HigherValue + LowerValue;
    }

    //
    // Handle a really big value, > 2^50.
    //

    Exponent = 0;
    if (AbsoluteLength > HYPOTENUSE_FLOAT_UPPER_THRESHOLD) {

        //
        // Handle Infinity or NaN.
        //

        if (AbsoluteLength >= FLOAT_NAN) {

            //
            // Use the original argument order if the result is NaN.
            //

            Result = fabsf(Length + (float)0.0) - fabsf(Width + (float)0.0);
            if (AbsoluteLength == FLOAT_NAN) {
                Result = HigherValue;
            }

            if (AbsoluteWidth == FLOAT_NAN) {
                Result = LowerValue;
            }

            return Result;
        }

        //
        // Scale the values by 2^-68.
        //

        AbsoluteLength -= HYPOTENUSE_FLOAT_SCALE_DOWN;
        AbsoluteWidth -= HYPOTENUSE_FLOAT_SCALE_DOWN;
        Exponent += HYPOTENUSE_FLOAT_SCALE_DOWN_EXPONENT;
        FloatParts.Ulong = AbsoluteLength;
        HigherValue = FloatParts.Float;
        FloatParts.Ulong = AbsoluteWidth;
        LowerValue = FloatParts.Float;
    }

    //
    // Handle a really small value, < 2^-50.
    //

    if (AbsoluteWidth < HYPOTENUSE_FLOAT_LOWER_THRESHOLD) {

        //
        // Handle a subnormal lower value, or zero.
        //

        if (AbsoluteWidth <= FLOAT_VALUE_MASK) {
            if (AbsoluteWidth == 0) {
                return HigherValue;
            }

            //
            // Set Length2 to 2^126.
            //

            FloatParts.Ulong = HYPOTENUSE_FLOAT_LENGTH2_BIG_HIGH ;
            Length2 = FloatParts.Float;
            LowerValue *= Length2;
            HigherValue *= Length2;
            Exponent -= HYPOTENUSE_FLOAT_LENGTH2_BIG_EXPONENT;

        //
        // Scale the values by 2^68.
        //

        } else {
            AbsoluteLength += HYPOTENUSE_FLOAT_SCALE_UP;
            AbsoluteWidth += HYPOTENUSE_FLOAT_SCALE_UP;
            Exponent -= HYPOTENUSE_FLOAT_SCALE_UP_EXPONENT;
            FloatParts.Ulong = AbsoluteLength;
            HigherValue = FloatParts.Float;
            FloatParts.Ulong = AbsoluteWidth;
            LowerValue = FloatParts.Float;
        }
    }

    //
    // Handle medium sized values.
    //

    Result = HigherValue - LowerValue;
    if (Result > LowerValue) {
        FloatParts.Ulong = AbsoluteLength & FLOAT_TRUNCATE_VALUE_MASK;
        Length2 = FloatParts.Float;
        Length2Remainder = HigherValue - Length2;
        Result = sqrtf(Length2 * Length2 -
                       (LowerValue * (-LowerValue) - Length2Remainder *
                        (HigherValue + Length2)));

    } else {
        HigherValue = HigherValue + HigherValue;
        FloatParts.Ulong = AbsoluteWidth & FLOAT_TRUNCATE_VALUE_MASK;
        WidthChopped = FloatParts.Float;
        WidthRemainder = LowerValue - WidthChopped;
        FloatParts.Ulong = AbsoluteLength + FLOAT_VALUE_MASK + 1;
        FloatParts.Ulong &= FLOAT_TRUNCATE_VALUE_MASK;
        Length2 = FloatParts.Float;
        Length2Remainder = HigherValue - Length2;
        Result = sqrtf(Length2 * WidthChopped -
                       (Result * (-Result) - (Length2 * WidthRemainder +
                                              Length2Remainder * LowerValue)));
    }

    if (Exponent != 0) {
        FloatParts.Ulong = FLOAT_ONE_WORD + (Exponent << FLOAT_EXPONENT_SHIFT);
        Length2 = FloatParts.Float;
        return Length2 * Result;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

