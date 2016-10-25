/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    div.c

Abstract:

    This module implements support for the divide routines in the standard C
    library.

Author:

    Evan Green 29-Aug-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"
#include <stdlib.h>

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
div_t
div (
    int Numerator,
    int Denominator
    )

/*++

Routine Description:

    This routine divides two integers. If the division is inexact, the
    resulting quotient is the integer of lesser magnitude that is nearest to
    the algebraic quotient. If the result cannot be represented, the behavior
    is undefined.

Arguments:

    Numerator - Supplies the dividend.

    Denominator - Supplies the divisor.

Return Value:

    Returns the quotient of the division.

--*/

{

    div_t Result;

    Result.quot = Numerator / Denominator;
    Result.rem = Numerator % Denominator;
    return Result;
}

LIBC_API
ldiv_t
ldiv (
    long Numerator,
    long Denominator
    )

/*++

Routine Description:

    This routine divides two long integers. If the division is inexact, the
    resulting quotient is the integer of lesser magnitude that is nearest to
    the algebraic quotient. If the result cannot be represented, the behavior
    is undefined.

Arguments:

    Numerator - Supplies the dividend.

    Denominator - Supplies the divisor.

Return Value:

    Returns the quotient of the division.

--*/

{

    ldiv_t Result;

    Result.quot = Numerator / Denominator;
    Result.rem = Numerator % Denominator;
    return Result;
}

LIBC_API
lldiv_t
lldiv (
    long long Numerator,
    long long Denominator
    )

/*++

Routine Description:

    This routine divides two long long integers. If the division is inexact,
    the resulting quotient is the integer of lesser magnitude that is nearest
    to the algebraic quotient. If the result cannot be represented, the
    behavior is undefined.

Arguments:

    Numerator - Supplies the dividend.

    Denominator - Supplies the divisor.

Return Value:

    Returns the quotient of the division.

--*/

{

    lldiv_t Result;

    Result.quot = Numerator / Denominator;
    Result.rem = Numerator % Denominator;
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

