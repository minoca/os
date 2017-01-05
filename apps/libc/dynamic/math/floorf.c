/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    floorf.c

Abstract:

    This module implements the floor functions for the math library.

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
floorf (
    float Value
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
    ULONG Max;
    FLOAT_PARTS ValueParts;
    LONG Word;

    ValueParts.Float = Value;
    Word = ValueParts.Ulong;

    //
    // Get the (positive) exponent.
    //

    Exponent = ((Word & FLOAT_EXPONENT_MASK) >> FLOAT_EXPONENT_SHIFT) -
               FLOAT_EXPONENT_BIAS;

    if (Exponent < 23) {

        //
        // Raise inexact if the value is not zero.
        //

        if (Exponent < 0) {

            //
            // Return 0 (in the correct sign) if the absolute value is less
            // than 1.
            //

            if (ClFloatHugeValue + Value > (float)0.0) {
                if (Word >= 0) {
                    Word = 0;

                } else if ((ValueParts.Ulong & ~FLOAT_SIGN_BIT) != 0) {
                    Word = FLOAT_NEGATIVE_ZERO_WORD;
                }
            }

        } else {
            Max = FLOAT_VALUE_MASK >> Exponent;

            //
            // Return if the value is integral.
            //

            if ((Word & Max) == 0) {
                return Value;
            }

            //
            // Raise the inexact flag.
            //

            if (ClFloatHugeValue + Value > (float)0.0) {
                if (Word < 0) {
                    Word += (1 << FLOAT_EXPONENT_SHIFT) >> Exponent;
                }

                Word &= ~Max;
            }
        }

    } else {

        //
        // Watch out for infinity or NaN.
        //

        if (Exponent ==
            (FLOAT_NAN >> FLOAT_EXPONENT_SHIFT) - FLOAT_EXPONENT_BIAS) {

            return Value + Value;
        }

        //
        // The value is integral.
        //

        return Value;
    }

    ValueParts.Ulong = Word;
    return ValueParts.Float;
}

//
// --------------------------------------------------------- Internal Functions
//

