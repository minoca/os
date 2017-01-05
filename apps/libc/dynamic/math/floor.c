/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    floor.c

Abstract:

    This module implements the floor functions for the math library.

    Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.

    Developed at SunPro, a Sun Microsystems, Inc. business.
    Permission to use, copy, modify, and distribute this
    software is freely granted, provided that this notice
    is preserved.

Author:

    Evan Green 23-Jul-2013

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

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
floor (
    double Value
    )

/*++

Routine Description:

    This routine computes the largest integral value not greater than the
    given value.

Arguments:

    Value - Supplies the value to use.

Return Value:

    Returns the largest integral value not greater than the input value.

--*/

{

    LONG Exponent;
    ULONG ExponentHighWordMask;
    ULONG ExponentHighWordShift;
    LONG HighWord;
    ULONG Low;
    LONG LowWord;
    ULONG Max;
    ULONG ValueHighWordMask;
    DOUBLE_PARTS ValueParts;

    ExponentHighWordMask = DOUBLE_EXPONENT_MASK >> DOUBLE_HIGH_WORD_SHIFT;
    ExponentHighWordShift = DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT;
    ValueHighWordMask = (~(DOUBLE_SIGN_BIT | DOUBLE_EXPONENT_MASK)) >>
                         DOUBLE_HIGH_WORD_SHIFT;

    ValueParts.Double = Value;
    HighWord = ValueParts.Ulong.High;
    LowWord = ValueParts.Ulong.Low;

    //
    // Get the (positive) exponent.
    //

    Exponent = ((HighWord & ExponentHighWordMask) >>
               ExponentHighWordShift) - DOUBLE_EXPONENT_BIAS;

    if (Exponent < 20) {

        //
        // Raise inexact if the value is not zero.
        //

        if (Exponent < 0) {

            //
            // Return 0 (in the correct sign) if the absolute value is less
            // than 1.
            //

            if (ClDoubleHugeValue + Value > 0.0) {
                if (HighWord >= 0) {
                    HighWord = 0;
                    LowWord = 0;

                } else if ((ValueParts.Ulonglong & (~DOUBLE_SIGN_BIT)) != 0) {
                    HighWord = DOUBLE_NEGATIVE_ZERO_HIGH_WORD;
                    LowWord = 0;
                }
            }

        } else {
            Max = ValueHighWordMask >> Exponent;

            //
            // Return if the value is integral.
            //

            if (((HighWord & Max) | LowWord) == 0) {
                return Value;
            }

            //
            // Raise the inexact flag.
            //

            if (ClDoubleHugeValue + Value > 0.0) {
                if (HighWord < 0) {
                    HighWord += (1 << ExponentHighWordShift) >> Exponent;
                }

                HighWord &= ~Max;
                LowWord=0;
            }
        }

    } else if (Exponent > 51) {

        //
        // Watch out for infinity or NaN.
        //

        if (Exponent ==
            (NAN_HIGH_WORD >> ExponentHighWordShift) - DOUBLE_EXPONENT_BIAS) {

            return Value + Value;
        }

        //
        // The value is integral.
        //

        return Value;

    } else {
        Max = ((ULONG)0xFFFFFFFF) >> (Exponent - 20);

        //
        // The value is integral.
        //

        if ((LowWord & Max) == 0) {
            return Value;
        }

        //
        // Raise the inexact flag.
        //

        if (ClDoubleHugeValue + Value > 0.0) {
            if (HighWord < 0) {
                if (Exponent == 20) {
                    HighWord+=1;

                } else {
                    Low = LowWord + (1 << (52 - Exponent));

                    //
                    // Do the carry if a rollover occurred.
                    //

                    if (Low < LowWord) {
                        HighWord += 1;
                    }

                    LowWord = Low;
                }
            }

            LowWord &= ~Max;
        }
    }

    ValueParts.Ulong.High = HighWord;
    ValueParts.Ulong.Low = LowWord;
    return ValueParts.Double;
}

//
// --------------------------------------------------------- Internal Functions
//

