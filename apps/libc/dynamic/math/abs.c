/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    abs.c

Abstract:

    This module implements absolute value functions for the math library.

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
int
abs (
    int Value
    )

/*++

Routine Description:

    This routine returns the absolute value of the given value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

{

    if (Value >= 0) {
        return Value;
    }

    return -Value;
}

LIBC_API
long
labs (
    long Value
    )

/*++

Routine Description:

    This routine returns the absolute value of the given long value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

{

    if (Value >= 0) {
        return Value;
    }

    return -Value;
}

LIBC_API
long long
llabs (
    long long Value
    )

/*++

Routine Description:

    This routine returns the absolute value of the given long long value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

{

    if (Value >= 0) {
        return Value;
    }

    return -Value;
}

LIBC_API
double
fabs (
    double Value
    )

/*++

Routine Description:

    This routine returns the absolute value of the given value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

{

    DOUBLE_PARTS ValueParts;

    ValueParts.Double = Value;
    ValueParts.Ulong.High &= (~DOUBLE_SIGN_BIT >> DOUBLE_HIGH_WORD_SHIFT);
    return ValueParts.Double;
}

LIBC_API
float
fabsf (
    float Value
    )

/*++

Routine Description:

    This routine returns the absolute value of the given value.

Arguments:

    Value - Supplies the value to get the absolute value of.

Return Value:

    Returns the absolute value.

--*/

{

    FLOAT_PARTS ValueParts;

    ValueParts.Float = Value;
    ValueParts.Ulong &= ~FLOAT_SIGN_BIT;
    return ValueParts.Float;
}

//
// --------------------------------------------------------- Internal Functions
//

