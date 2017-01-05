/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    expf.c

Abstract:

    This module implements the exponential function.

    Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.

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

#define EXPF_UPPER_THRESHOLD_WORD 0x42B17218
#define EXPF_HALF_LN_2_WORD 0x3EB17218
#define EXPF_3_HALVES_LN_2_WORD 0x3F851592
#define EXPF_LOWER_THRESHOLD_WORD 0x39000000
#define EXPF_2_TO_127 0x1p127F

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const float ClExpfOverflowThreshold = 8.8721679688e+01;
const float ClExpfUnderflowThreshold = -1.0397208405e+02;

const float ClExpf1 = 1.6666625440e-01;
const float ClExpf2 = -2.7667332906e-3;

//
// Define 2^-100 for underflow.
//

const double ClTwoNegative100 = 7.8886090522e-31;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
frexpf (
    float Value,
    int *Exponent
    )

/*++

Routine Description:

    This routine breaks a floating point number down into a normalized fraction
    and an integer power of 2.

Arguments:

    Value - Supplies the value to normalize.

    Exponent - Supplies a pointer where the exponent will be returned.

Return Value:

    Returns the normalized fraction (the significand).

--*/

{

    LONG AbsoluteWord;
    FLOAT_PARTS Parts;
    LONG Word;

    Parts.Float = Value;
    Word = Parts.Ulong;
    AbsoluteWord = Word & ~FLOAT_SIGN_BIT;
    *Exponent = 0;

    //
    // Skip 0, infininy, or NaN.
    //

    if ((AbsoluteWord >= FLOAT_NAN) || (AbsoluteWord == 0)) {
        return Value;
    }

    //
    // Handle subnormal values.
    //

    if (AbsoluteWord < (1 << FLOAT_EXPONENT_SHIFT)) {
        Value *= ClFloatTwo25;
        Parts.Float = Value;
        Word = Parts.Ulong;
        AbsoluteWord = Word & ~FLOAT_SIGN_BIT;
        *Exponent = -25;
    }

    *Exponent += (AbsoluteWord >> FLOAT_EXPONENT_SHIFT) -
                 (FLOAT_EXPONENT_BIAS - 1);

    Word = (Word & (FLOAT_VALUE_MASK | FLOAT_SIGN_BIT)) |
           ((FLOAT_EXPONENT_BIAS - 1) << FLOAT_EXPONENT_SHIFT);

    Parts.Ulong = Word;
    Value = Parts.Float;
    return Value;
}

LIBC_API
float
expf (
    float Value
    )

/*++

Routine Description:

    This routine computes the base e exponential of the given value.

Arguments:

    Value - Supplies the value to raise e to.

Return Value:

    Returns e to the given value.

--*/

{

    float Approximation;
    float Exponentiation;
    float High;
    float Input;
    LONG Ln2Multiple;
    float Low;
    LONG SignBit;
    float TwoPower;
    FLOAT_PARTS ValueParts;
    volatile float VolatileValue;
    ULONG Word;

    High = 0.0;
    Low = 0.0;
    Ln2Multiple = 0;
    ValueParts.Float= Value;
    Word = ValueParts.Ulong;
    SignBit = Word >> FLOAT_SIGN_BIT_SHIFT;

    //
    // Get the absolute value.
    //

    Word = Word & ~FLOAT_SIGN_BIT;

    //
    // Filter out a non-finite argument.
    //

    if (Word >= EXPF_UPPER_THRESHOLD_WORD) {
        if (Word > FLOAT_NAN) {
            return Value + Value;
        }

        if (Word == FLOAT_NAN) {

            //
            // Exponentiation of +/- Infinity is Infinity, and 0.
            //

            if (SignBit == 0) {
                return Value;

            } else {
                return 0.0;
            }
        }

        //
        // Handle overflow and underflow cases.
        //

        if (Value > ClExpfOverflowThreshold) {
            return ClFloatHugeValue * ClFloatHugeValue;
        }

        if (Value < ClExpfUnderflowThreshold) {
            return ClTwoNegative100 * ClTwoNegative100;
        }
    }

    //
    // Perform argument reduction.
    //

    if (Word > EXPF_HALF_LN_2_WORD) {
        if (Word < EXPF_3_HALVES_LN_2_WORD) {
            High = Value - ClFloatLn2High[SignBit];
            Low = ClFloatLn2Low[SignBit];
            Ln2Multiple = 1 - SignBit - SignBit;

        } else {
            if (SignBit != 0) {
                Ln2Multiple = (INT)(ClFloatInverseLn2 * Value - ClFloatOneHalf);

            } else {
                Ln2Multiple = (INT)(ClFloatInverseLn2 * Value + ClFloatOneHalf);
            }

            Input = Ln2Multiple;

            //
            // Input * Ln2High is exact here.
            //

            High = Value - Input * ClFloatLn2High[0];
            Low = Input * ClFloatLn2Low[0];
        }

        VolatileValue = High - Low;
        Value = VolatileValue;

    //
    // Handle very small values.
    //

    } else if (Word < EXPF_LOWER_THRESHOLD_WORD)  {
        if (ClFloatHugeValue + Value > ClFloatOne) {

            //
            // Trigger an inexact condition.
            //

            return ClFloatOne + Value;
        }

    } else {
        Ln2Multiple = 0;
    }

    //
    // The value is now in the primary range.
    //

    Input  = Value * Value;
    if (Ln2Multiple >= -125) {
        ValueParts.Ulong = FLOAT_ONE_WORD +
                           (Ln2Multiple << FLOAT_EXPONENT_SHIFT);

    } else {
        ValueParts.Ulong = FLOAT_ONE_WORD +
                           ((Ln2Multiple + 100) << FLOAT_EXPONENT_SHIFT);
    }

    TwoPower = ValueParts.Float;
    Approximation = Value - Input * (ClExpf1 + Input * ClExpf2);
    if (Ln2Multiple == 0) {
        Exponentiation = ClFloatOne -
                         ((Value * Approximation) /
                          (Approximation - (float)2.0) -
                         Value);

        return Exponentiation;
    }

    Exponentiation = ClFloatOne -
        ((Low - (Value * Approximation) / ((float)2.0 - Approximation)) - High);

    if (Ln2Multiple >= -125) {
        if (Ln2Multiple == 128) {
            return Exponentiation * (float)2.0 * EXPF_2_TO_127;
        }

        return Exponentiation * TwoPower;
    }

    return Exponentiation * TwoPower * ClTwoNegative100;
}

//
// --------------------------------------------------------- Internal Functions
//

