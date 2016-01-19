/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    stubs.c

Abstract:

    This module implements basic runtime library stubs for system-level
    functions in the build environment.

Author:

    Evan Green 23-Oct-2013

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"

#include <stdio.h>
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

RTL_API
VOID
RtlRaiseAssertion (
    PSTR Expression,
    PSTR SourceFile,
    ULONG SourceLine
    )

/*++

Routine Description:

    This routine is a stub for the callouts to the rtl library from the mm
    library. Actually it effectively implements RtlRaiseAssertion.

Arguments:

    Expression - Supplies the string containing the expression that failed.

    SourceFile - Supplies the string describing the source file of the failure.

    SourceLine - Supplies the source line number of the failure.

Return Value:

    None.

--*/

{

    fprintf(stderr,
            "\n *** Assertion Failure: %s\n *** File: %s, Line %d\n",
            Expression,
            SourceFile,
            SourceLine);

    abort();
    return;
}

RTL_API
VOID
RtlDebugPrint (
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine is a stub for the debugger print routines.

Arguments:

    Format - Supplies the printf-style format string to print. The contents of
        this string determine the rest of the arguments passed.

    ... - Supplies any arguments needed to convert the Format string.

Return Value:

    None.

--*/

{

    va_list Arguments;

    va_start(Arguments, Format);
    vprintf(Format, Arguments);
    va_end(Arguments);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

