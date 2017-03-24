/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    minmaxf.c

Abstract:

    This module implements fminf and fmaxf, which coupte the minimum and
    maximum of two values, respectively.

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

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
float
fminf (
    float FirstValue,
    float SecondValue
    )

/*++

Routine Description:

    This routine returns the minimum numeric value between the two given
    arguments. NaN arguments are treated as missing data. If one argument is
    NaN and the other is not, the numeric argument is returned.

Arguments:

    FirstValue - Supplies the first value to consider.

    SecondValue - Supplies the second value to consider.

Return Value:

    Returns the minimum of the two.

--*/

{

    FLOAT_PARTS FirstParts;
    FLOAT_PARTS SecondParts;

    FirstParts.Float = FirstValue;
    SecondParts.Float = SecondValue;

    //
    // Handle NaNs first.
    //

    if ((FirstParts.Ulong >> FLOAT_EXPONENT_SHIFT) == FLOAT_NAN_EXPONENT) {
        if ((FirstParts.Ulong & FLOAT_VALUE_MASK) != 0) {
            return SecondValue;
        }
    }

    if ((SecondParts.Ulong >> FLOAT_EXPONENT_SHIFT) == FLOAT_NAN_EXPONENT) {
        if ((SecondParts.Ulong & FLOAT_VALUE_MASK) != 0) {
            return FirstValue;
        }
    }

    //
    // Handle sign difference.
    //

    if (((FirstParts.Ulong ^ SecondParts.Ulong) & FLOAT_SIGN_BIT) != 0) {
        if ((FirstParts.Ulong & FLOAT_SIGN_BIT) != 0) {
            return SecondValue;
        }

        return FirstValue;
    }

    //
    // Okay, simply compare them.
    //

    if (FirstValue < SecondValue) {
        return FirstValue;
    }

    return SecondValue;
}

LIBC_API
float
fmaxf (
    float FirstValue,
    float SecondValue
    )

/*++

Routine Description:

    This routine returns the maximum numeric value between the two given
    arguments. NaN arguments are treated as missing data. If one argument is
    NaN and the other is not, the numeric argument is returned.

Arguments:

    FirstValue - Supplies the first value to consider.

    SecondValue - Supplies the second value to consider.

Return Value:

    Returns the maximum of the two.

--*/

{

    FLOAT_PARTS FirstParts;
    FLOAT_PARTS SecondParts;

    FirstParts.Float = FirstValue;
    SecondParts.Float = SecondValue;

    //
    // Handle NaNs first.
    //

    if ((FirstParts.Ulong >> FLOAT_EXPONENT_SHIFT) == FLOAT_NAN_EXPONENT) {
        if ((FirstParts.Ulong & FLOAT_VALUE_MASK) != 0) {
            return SecondValue;
        }
    }

    if ((SecondParts.Ulong >> FLOAT_EXPONENT_SHIFT) == FLOAT_NAN_EXPONENT) {
        if ((SecondParts.Ulong & FLOAT_VALUE_MASK) != 0) {
            return FirstValue;
        }
    }

    //
    // Handle sign difference.
    //

    if (((FirstParts.Ulong ^ SecondParts.Ulong) & FLOAT_SIGN_BIT) != 0) {
        if ((FirstParts.Ulong & FLOAT_SIGN_BIT) != 0) {
            return FirstValue;
        }

        return SecondValue;
    }

    //
    // Okay, simply compare them.
    //

    if (FirstValue > SecondValue) {
        return FirstValue;
    }

    return SecondValue;
}

//
// --------------------------------------------------------- Internal Functions
//

