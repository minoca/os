/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lroundf.c

Abstract:

    This module implements support for the lround family of math functions.

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
#include <fenv.h>
#include <limits.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This routine determines if the given value is in range. The first line
// checks to see if the max is outside the integral capability of the floating
// point type. If so, assume it's in range. The second part makes sure that
// after being adjusted closer to 0 by 0.5 the value is within range.
//

#define LROUND_FLOAT_IN_RANGE(_Value, _Max, _Min) \
    (((float)(((_Max) + 0.5) - (_Max)) != 0.5) || \
     (((_Value) > ((_Min) - 0.5)) && \
      ((_Value) < ((_Max) + 0.5))))

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
long
lroundf (
    float Value
    )

/*++

Routine Description:

    This routine rounds the given value to the nearest integer value, rounding
    halfway cases away from zero, regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded integer on success.

    Returns an unspecified value if the given value is out of range, or NaN.

--*/

{

    if (!LROUND_FLOAT_IN_RANGE(Value, LONG_MAX, LONG_MIN)) {
        feraiseexcept(FE_INVALID);
        return MAX_LONG;
    }

    Value = roundf(Value);
    return (long)Value;
}

LIBC_API
long long
llroundf (
    float Value
    )

/*++

Routine Description:

    This routine rounds the given value to the nearest integer value, rounding
    halfway cases away from zero, regardless of the current rounding direction.

Arguments:

    Value - Supplies the value to round.

Return Value:

    Returns the rounded integer on success.

    Returns an unspecified value if the given value is out of range, or NaN.

--*/

{

    if (!LROUND_FLOAT_IN_RANGE(Value, LONG_LONG_MAX, LONG_LONG_MIN)) {
        feraiseexcept(FE_INVALID);
        return MAX_LONG;
    }

    Value = roundf(Value);
    return (long long)Value;
}

//
// --------------------------------------------------------- Internal Functions
//

