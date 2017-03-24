/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    trunc.c

Abstract:

    This module implements trunc, which rounds the given value to an integer,
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

const double ClTruncHugeValue = 1.0e300;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
double
trunc (
    double Value
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
    LONG High;
    ULONG Integer;
    LONG Low;
    DOUBLE_PARTS Parts;

    Parts.Double = Value;
    High = Parts.Ulong.High;
    Low = Parts.Ulong.Low;
    Exponent = (High >> (DOUBLE_EXPONENT_SHIFT - DOUBLE_HIGH_WORD_SHIFT)) -
               DOUBLE_EXPONENT_BIAS;

    if (Exponent < 20) {

        //
        // Raise an inexact error if the value is not zero.
        //

        if (Exponent < 0) {

            //
            // If |Value| < 1, return 0 * sign(Value).
            //

            if (ClTruncHugeValue + Value > 0.0) {
                High &= DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT;
                Low = 0;
            }

        } else {
            Integer = DOUBLE_HIGH_VALUE_MASK >> Exponent;

            //
            // See if the value is already integral.
            //

            if (((High & Integer) | Low) == 0) {
                return Value;
            }

            //
            // Raise an inexact error.
            //

            if (ClTruncHugeValue + Value > 0.0) {
                High &= ~Integer;
                Low = 0;
            }
        }

    } else if (Exponent > 51) {

        //
        // Handle infinity or NaN.
        //

        if (Exponent == 0x400) {
            return Value + Value;
        }

        //
        // Otherwise the value is already integral.
        //

        return Value;

    } else {
        Integer = MAX_ULONG >> (Exponent - 20);

        //
        // See if the value is already integral.
        //

        if ((Low & Integer) == 0) {
            return Value;
        }

        //
        // Raise an inexact error.
        //

        if (ClTruncHugeValue + Value > 0.0) {
            Low &= ~Integer;
        }
    }

    Parts.Ulong.High = High;
    Parts.Ulong.Low = Low;
    return Parts.Double;
}

//
// --------------------------------------------------------- Internal Functions
//

