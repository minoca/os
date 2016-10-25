/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    log2.c

Abstract:

    This module implements the mathematical base 2 logarithm functions.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 13-Aug-2013

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

const double ClInverseLn2HighForLog2 = 1.44269504072144627571e+00;
const double ClInverseLn2LowForLog2 = 1.67517131648865118353e-10;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
log2 (
    double Value
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
    double ExponentDouble;
    double Extra;
    LONG ExtraExponent;
    double HalfSquare;
    double High;
    LONG HighWord;
    double LogResult;
    double Low;
    ULONG LowWord;
    LONG OneExponent;
    DOUBLE_PARTS Parts;
    double ResultHigh;
    double ResultLow;
    double ValueMinusOne;

    OneExponent = 1 << (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT);

    //
    // The method is generally the same as the log() function.
    // This reduces the value to {Exponent, 1 + ValueMinusOne}, then calls the
    // limited range function. Finally, it does the combining and scaling steps
    // log2(Value) = (ValueMinusOne - 0.5 * ValueMinusOne^2 +
    //                LogOnePlus(ValueMinusOne)) / ln2 + Exponent.
    //

    Parts.Double = Value;
    HighWord = Parts.Ulong.High;
    LowWord = Parts.Ulong.Low;
    Exponent = 0;

    //
    // Handle values less than 2^-1022.
    //

    if (HighWord < OneExponent) {

        //
        // Log(+-0) is -Infinity.
        //

        if (((HighWord & (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT)) |
             LowWord) == 0) {

            return -ClTwo54 / ClDoubleZero;
        }

        //
        // Log of a negative number is NaN.
        //

        if (HighWord < 0) {
            return (Value - Value) / ClDoubleZero;
        }

        //
        // Scale up a subnormal value.
        //

        Exponent -= 54;
        Value *= ClTwo54;
        Parts.Double = Value;
        HighWord = Parts.Ulong.High;
    }

    if (HighWord >= NAN_HIGH_WORD) {
        return Value + Value;
    }

    //
    // Log(1) is +0.
    //

    if ((HighWord == DOUBLE_ONE_HIGH_WORD) && (LowWord == 0)) {
        return ClDoubleZero;
    }

    Exponent += (HighWord >> (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT)) -
                DOUBLE_EXPONENT_BIAS;

    HighWord &= DOUBLE_HIGH_VALUE_MASK;

    //
    // Normalize Value or Value / 2.
    //

    ExtraExponent = (HighWord + 0x95F64) & OneExponent;
    Parts.Ulong.High = HighWord | (ExtraExponent ^ DOUBLE_ONE_HIGH_WORD);
    Value = Parts.Double;
    Exponent += (ExtraExponent >>
                 (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT));

    ExponentDouble = (double)Exponent;
    ValueMinusOne = Value - 1.0;
    HalfSquare = 0.5 * ValueMinusOne * ValueMinusOne;
    LogResult = ClpLogOnePlus(ValueMinusOne);

    //
    // ValueMinusOne - HalfSquare must (for arguments near 1) be evaluated in
    // extra precision to avoid a large cancellation when Value is near sqrt(2)
    // or 1 / sqrt(2). This is fairly efficient since ValueMinusOne - HalfSquare
    // only depends on ValueMinusOne, so it can be evaluated in parallel with
    // R. Not combining HalfSquare with R also keeps R small (though not as
    // small as a true 'Low' term would be), so that extra precision is not
    // needed for terms involving R.
    //
    // Compiler bugs involving extra precision used to break Dekker's
    // theorem for splitting ValueMinusOne - HalfSquare as High + Low, unless
    // double_t was used or the multi-precision calculations were avoided when
    // double_t has extra precision. These problems are now automatically
    // avoided as a side effect of the optimization of combining the Dekker
    // splitting step with the clear-low-bits step.
    //
    // ExponentDouble must (for argumentss near sqrt(2) and 1 / sqrt(2)) be
    // added in extra precision to avoid a very large cancellation when Value
    // is very near these values. Unlike the above cancellations, this problem
    // is specific to base 2. It is strange that adding +-1 is so much harder
    // than adding +-ln2 or +-log10_2.
    //
    // This uses Dekker's theorem to normalize ExponentDouble + ResultHigh, so
    // the compiler bugs are back in some configurations, sigh.
    //
    // The multi-precision calculations for the multiplications are
    // routine.
    //

    High = ValueMinusOne - HalfSquare;
    Parts.Double = High;
    Parts.Ulong.Low = 0;
    High = Parts.Double;
    Low = (ValueMinusOne - High) - HalfSquare + LogResult;
    ResultHigh = High * ClInverseLn2HighForLog2;
    ResultLow = (Low + High) * ClInverseLn2LowForLog2 +
                Low * ClInverseLn2HighForLog2;

    Extra = ExponentDouble + ResultHigh;
    ResultLow += (ExponentDouble - Extra) + ResultHigh;
    ResultHigh = Extra;
    return ResultLow + ResultHigh;
}

//
// --------------------------------------------------------- Internal Functions
//

