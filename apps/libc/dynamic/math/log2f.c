/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    log2f.c

Abstract:

    This module implements the mathematical base 2 logarithm functions.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
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

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

const float ClInverseLn2HighForLog2f = 1.4428710938e+00;
const float ClInverseLn2LowForLog2f = -1.7605285393e-04;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
log2f (
    float Value
    )

/*++

Routine Description:

    This routine implements the base two logarithm function.

Arguments:

    Value - Supplies the value to take the base 2 logarithm of.

Return Value:

    Returns the base 2 logarithm of the given value.

--*/

{

    LONG Exponent;
    float ExponentFloat;
    LONG ExtraExponent;
    float HalfSquare;
    float High;
    float LogResult;
    float Low;
    FLOAT_PARTS Parts;
    float Result;
    float ValueMinusOne;
    LONG Word;

    //
    // The method is generally the same as the log() function.
    // This reduces the value to {Exponent, 1 + ValueMinusOne}, then calls the
    // limited range function. Finally, it does the combining and scaling steps
    // log2(Value) = (ValueMinusOne - 0.5 * ValueMinusOne^2 +
    //                LogOnePlus(ValueMinusOne)) / ln2 + Exponent.
    //

    Parts.Float = Value;
    Word = Parts.Ulong;
    Exponent = 0;

    //
    // Handle values less than 2^-126.
    //

    if (Word < (1 << FLOAT_EXPONENT_SHIFT)) {

        //
        // Log(+-0) is -Infinity.
        //

        if ((Word & ~FLOAT_SIGN_BIT) == 0) {
            return -ClFloatTwo25 / ClFloatZero;
        }

        //
        // Log of a negative number is NaN.
        //

        if (Word < 0) {
            return (Value - Value) / ClFloatZero;
        }

        //
        // Scale up a subnormal value.
        //

        Exponent -= 25;
        Value *= ClFloatTwo25;
        Parts.Float = Value;
        Word = Parts.Ulong;
    }

    if (Word >= FLOAT_NAN) {
        return Value + Value;
    }

    if (Word == FLOAT_ONE_WORD) {
        return ClFloatZero;
    }

    Exponent += (Word >> FLOAT_EXPONENT_SHIFT) - FLOAT_EXPONENT_BIAS;
    Word &= FLOAT_VALUE_MASK;

    //
    // Normalize Value or Value / 2.
    //

    ExtraExponent = (Word + (0x4afb0d)) & (1 << FLOAT_EXPONENT_SHIFT);
    Parts.Ulong = Word | (ExtraExponent ^ FLOAT_ONE_WORD);
    Value = Parts.Float;
    Exponent += (ExtraExponent >> FLOAT_EXPONENT_SHIFT);
    ExponentFloat = (float)Exponent;
    ValueMinusOne = Value - ClFloatOne;
    HalfSquare = (float)0.5 * ValueMinusOne * ValueMinusOne;
    LogResult = ClpLogOnePlusFloat(ValueMinusOne);

    //
    // There is no longer a need to avoid falling into the multi-precision
    // calculations due to compiler bugs breaking Dekker's theorem.
    // Keep avoiding this as an optimization.  See log2.c for more
    // details (some details are here only because the optimization
    // is not yet available in double precision).
    //

    High = ValueMinusOne - HalfSquare;
    Parts.Float = High;
    Word = Parts.Ulong;
    Word &= FLOAT_TRUNCATE_VALUE_MASK;
    Parts.Ulong = Word;
    High = Parts.Float;
    Low = (ValueMinusOne - High) - HalfSquare + LogResult;
    Result = ((Low + High) * ClInverseLn2LowForLog2f) +
             (Low * ClInverseLn2HighForLog2f) +
             (High * ClInverseLn2HighForLog2f) +
             ExponentFloat;

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

