/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pdouble.c

Abstract:

    This module handles printing a floating point value.

Author:

    Evan Green 2-Jun-2017

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

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

VOID
RtlpGetDoubleArgument (
    BOOL LongDouble,
    va_list *ArgumentList,
    PDOUBLE_PARTS DoubleParts
    )

/*++

Routine Description:

    This routine gets a double from the argument list. It is used by printf,
    and is a separate function so that floating point support can be shaved out
    of the library.

Arguments:

    LongDouble - Supplies a boolean indicating if the argument is a long double
        or just a regular double.

    ArgumentList - Supplies a pointer to the VA argument list. It's a pointer
        so that the effect of the va_arg can be felt by the calling function.

    DoubleParts - Supplies a pointer where the double is returned, disguised in
        a structure so as not to force floating point arguments.

Return Value:

    None.

--*/

{

    DoubleParts->Ulonglong = (ULONGLONG)DOUBLE_NAN_EXPONENT <<
                             DOUBLE_EXPONENT_SHIFT;

    return;
}

BOOL
RtlpPrintDouble (
    PPRINT_FORMAT_CONTEXT Context,
    double Value,
    PPRINT_FORMAT_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine prints a double to the destination given the style
    properties.

Arguments:

    Context - Supplies a pointer to the initialized context structure.

    DestinationSize - Supplies the size of the destination buffer. If NULL was
        passed as the Destination argument, then this argument is ignored.

    Value - Supplies a pointer to the value to convert to a string.

    Properties - Supplies the style characteristics to use when printing this
        integer.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ASSERT(FALSE);

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

