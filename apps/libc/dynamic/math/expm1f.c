/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    expm1f.c

Abstract:

    This module implements support for the expentiation function, minus one.
    This is apparently useful in financial situations.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Chris Stevens 4-Jan-2017

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

#define EXPM1F_27LN2_WORD 0x4195b844
#define EXPM1F_UPPER_LIMIT_WORD 0x42b17218
#define EXPM1F_HALF_LN2_WORD 0x3eb17218
#define EXPM1F_3LN2_OVER_2_WORD 0x3F851592
#define EXPM1F_2_TO_NEGATIVE_25_WORD 0x33000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

static const float ClExpm1fThreshold = 8.8721679688e+01;
static const float ClExpm1f1 = -3.3333212137e-2;
static const float ClExpm1f2 = 1.5807170421e-3;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
expm1f (
    float Value
    )

/*++

Routine Description:

    This routine computes the base e exponential of the given value, minus one.

Arguments:

    Value - Supplies the value to raise e to.

Return Value:

    Returns e to the given value, minus one.

--*/

{

    float Correction;
    float Error;
    LONG Exponent;
    float HalfValue;
    float HalfValueSquared;
    float High;
    float Low;
    float Rational;
    float Result;
    LONG SignBit;
    float TwoRaisedExponent;
    FLOAT_PARTS ValueParts;
    volatile float VolatileValue;
    ULONG Word;
    float Working;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;

    //
    // Get and mask off the sign bit from the high word.
    //

    SignBit = Word & FLOAT_SIGN_BIT;
    Word = Word & ~FLOAT_SIGN_BIT;

    //
    // Handle gigantic and non-finite arguments.
    //

    if (Word >= EXPM1F_27LN2_WORD) {
        if (Word >= EXPM1F_UPPER_LIMIT_WORD) {
            if (Word > FLOAT_NAN) {
                return Value + Value;
            }

            if (Word == FLOAT_NAN) {

                //
                // expm1f(+-INF) = {INF, -1}.
                //

                if (SignBit == 0) {
                    return Value;
                }

                return -1.0;
            }

            if (Value > ClExpm1fThreshold) {

                //
                // Overflow.
                //

                return ClFloatHugeValue * ClFloatHugeValue;
            }
        }

        //
        // If the value is < -27ln2, return -1.0 with inexact.
        //

        if (SignBit != 0) {
            if (Value + ClFloatTinyValue < (float)0.0) {
                return ClFloatTinyValue - ClFloatOne;
            }
        }
    }

    //
    // Perform argument reduction.
    //

    if (Word > EXPM1F_HALF_LN2_WORD) {

        //
        // Handle |Value| > 0.5 ln2 and |Value| < 1.5ln2.
        //

        if (Word < EXPM1F_3LN2_OVER_2_WORD) {
            if (SignBit == 0) {
                High = Value - ClFloatLn2High[0];
                Low = ClFloatLn2Low[0];
                Exponent = 1;

            } else {
                High = Value + ClFloatLn2High[0];
                Low = ClFloatLn2Low[1];
                Exponent = -1;
            }

        } else {
            if (SignBit == 0) {
                Exponent = ClFloatInverseLn2 * Value + (float)0.5;

            } else {
                Exponent = ClFloatInverseLn2 * Value - (float)0.5;
            }

            Working = Exponent;
            High = Value - Working * ClFloatLn2High[0];
            Low = Working * ClFloatLn2Low[0];
        }

        VolatileValue = High - Low;
        Value = VolatileValue;
        Correction = (High - Value) - Low;

    //
    // When the |Value| is less than 2^-25, return the Value itself. Return
    // the value with inexact flags when the value is non-zero.
    //

    } else if (Word < EXPM1F_2_TO_NEGATIVE_25_WORD) {
        Working = ClFloatHugeValue + Value;
        return Value - (Working - (ClFloatHugeValue + Value));

    } else {
        Exponent = 0;
    }

    //
    // The value is now in range.
    //

    HalfValue = (float)0.5 * Value;
    HalfValueSquared = Value * HalfValue;
    Rational = ClFloatOne + HalfValueSquared *
               (ClExpm1f1 + HalfValueSquared * ClExpm1f2);

    Working = (float)3.0 - Rational * HalfValue;
    Error = HalfValueSquared *
            ((Rational - Working) / ((float)6.0 - Value * Working));

    if (Exponent == 0) {

        //
        // The correction is zero in this case.
        //

        return Value - (Value * Error - HalfValueSquared);

    } else {
        ValueParts.Ulong = FLOAT_ONE_WORD + (Exponent << FLOAT_EXPONENT_SHIFT);
        TwoRaisedExponent = ValueParts.Float;
        Error = (Value * (Error - Correction) - Correction);
        Error -= HalfValueSquared;
        if (Exponent == -1) {
            return (float)0.5 * (Value - Error) - (float)0.5;
        }

        if (Exponent == 1) {
            if (Value < (float)-0.25) {
                return -(float)2.0 * (Error - (Value + (float)0.5));

            } else {
                return ClFloatOne + (float)2.0 * (Value - Error);
            }
        }

        //
        // Return expf(Value) - 1.
        //

        if ((Exponent <= -2) || (Exponent > 56)) {
            Result = ClFloatOne - (Error - Value);
            if (Exponent == 128) {
                Result = Result * (float)2.0 * 0x1p127F;

            } else {
                Result = Result * TwoRaisedExponent;
            }

            return Result - ClFloatOne;
        }

        if (Exponent < 23) {

            //
            // Create 1 - 2^Exponent.
            //

            ValueParts.Ulong = FLOAT_ONE_WORD -
                               ((1 << (FLOAT_EXPONENT_SHIFT + 1)) >> Exponent);

            Result = ValueParts.Float - (Error - Value);
            Result = Result * TwoRaisedExponent;

       } else {

            //
            // Create 2^Exponent.
            //

            ValueParts.Ulong = (FLOAT_EXPONENT_BIAS - Exponent) <<
                               FLOAT_EXPONENT_SHIFT;

            Result = Value - (Error + ValueParts.Float);
            Result += ClFloatOne;
            Result = Result * TwoRaisedExponent;
        }
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

