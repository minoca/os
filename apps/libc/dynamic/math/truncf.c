/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    truncf.c

Abstract:

    This module implements truncf, which rounds the given value to an integer,
    nearest to but not larger in magnitude than the argument.

Author:

    Evan Green 23-Mar-2017

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include "mathp.h"

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

//
// -------------------------------------------------------------------- Globals
//

const float ClTruncfHugeValue = 1.0e300;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
truncf (
    float Value
    )

/*++

Routine Description:

    This routine truncates the value to an integer, nearest to but not greater
    in magnitude than the argument.

Arguments:

    Value - Supplies the value to truncated.

Return Value:

    Returns the nearest integer.

--*/

{

    LONG Exponent;
    ULONG Integer;
    LONG Low;
    FLOAT_PARTS Parts;

    Parts.Float = Value;
    Low = Parts.Ulong;
    Exponent = (Low >> FLOAT_EXPONENT_SHIFT) - FLOAT_EXPONENT_BIAS;
    if (Exponent < 23) {

        //
        // Raise an inexact error if the value is not zero.
        //

        if (Exponent < 0) {

            //
            // If |Value| < 1, return 0 * sign(Value).
            //

            if (ClTruncfHugeValue + Value > 0.0F) {
                Low &= FLOAT_SIGN_BIT;
            }

        } else {
            Integer = FLOAT_VALUE_MASK >> Exponent;

            //
            // See if the value is already integral.
            //

            if ((Low & Integer) == 0) {
                return Value;
            }

            //
            // Raise an inexact error.
            //

            if (ClTruncfHugeValue + Value > 0.0F) {
                Low &= ~Integer;
            }
        }

    } else {

        //
        // Handle infinity or NaN.
        //

        if (Exponent == 0x80) {
            return Value + Value;
        }

        //
        // The value is already integral.
        //

        return Value;
    }

    Parts.Ulong = Low;
    return Parts.Float;
}

//
// --------------------------------------------------------- Internal Functions
//

