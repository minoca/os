/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    exp2f.c

Abstract:

    This module implements exp2f, which computes the base two exponential of
    the given value.

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
exp2f (
    float Value
    )

/*++

Routine Description:

    This routine computes the base 2 exponential of the given value.

Arguments:

    Value - Supplies the value to raise 2 to.

Return Value:

    Returns 2 to the given value.

--*/

{

    return powf(2.0, Value);
}

//
// --------------------------------------------------------- Internal Functions
//

