/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ceilf.c

Abstract:

    This module implements support for the ceiling math functions.

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
ceilf (
    float Value
    )

/*++

Routine Description:

    This routine computes the smallest integral value not less then the given
    value.

Arguments:

    Value - Supplies the value to compute the ceiling of.

Return Value:

    Returns the ceiling on success.

    NaN if the given value is NaN.

    Returns the value itself for +/- 0 and +/- Infinity.

--*/

{

    LONG Exponent;
    ULONG NonExponent;
    FLOAT_PARTS Parts;
    LONG Word;

    Parts.Float= Value;
    Word = Parts.Ulong;
    Exponent = ((Word & FLOAT_EXPONENT_MASK) >> FLOAT_EXPONENT_SHIFT) -
               FLOAT_EXPONENT_BIAS;

    if (Exponent < 23) {

        //
        // Raise an inexact if the value isn't zero.
        //

        if (Exponent < 0) {

            //
            // Return 0 * sign(Value) if |Value| < 1.
            //

            if (ClFloatHugeValue + Value > (float)0.0) {
                if (Word < 0) {
                    Word = FLOAT_SIGN_BIT;

                } else if (Word != 0) {
                    Word = FLOAT_ONE_WORD;
                }
            }

        } else {
            NonExponent = FLOAT_VALUE_MASK >> Exponent;

            //
            // Return the value itself if it's integral.
            //

            if ((Word & NonExponent) == 0) {
                return Value;
            }

            //
            // Raise the inexact flag.
            //

            if (ClFloatHugeValue + Value > (float)0.0) {
                if (Word > 0) {
                    Word += (1 << FLOAT_EXPONENT_SHIFT) >> Exponent;
                }

                Word &= ~NonExponent;
            }
        }

    } else {

        //
        // Handle infinity or NaN.
        //

        if (Exponent == (FLOAT_NAN_EXPONENT - FLOAT_EXPONENT_BIAS)) {
            return Value + Value;

        //
        // Return the value itself if it is integral.
        //

        } else {
            return Value;
        }
    }

    Parts.Ulong = Word;
    return Parts.Float;
}

//
// --------------------------------------------------------- Internal Functions
//

