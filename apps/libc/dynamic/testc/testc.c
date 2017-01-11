/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testc.c

Abstract:

    This module implements the C library tests.

Author:

    Evan Green 9-Jul-2013

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "testc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the entry point for the C librarytest program. It executes
    the tests.

Arguments:

    ArgumentCount - Supplies the number of arguments specified on the command
        line.

    Arguments - Supplies an array of strings representing the command line
        arguments.

Return Value:

    Returns the number of failures that occurred during the test.

--*/

{

    ULONG Failures;
    ULONG TotalFailures;

    TotalFailures = 0;
    Failures = TestRegularExpressions();
    if (Failures != 0) {
        printf("%d regular expression test failures.\n", Failures);
        TotalFailures += Failures;
    }

    Failures = TestQuickSort();
    if (Failures != 0) {
        printf("%d qsort failures.\n", Failures);
        TotalFailures += Failures;
    }

    Failures += TestBinarySearch();
    if (Failures != 0) {
        printf("%d binary search failures.\n", Failures);
        TotalFailures += Failures;
    }

    Failures = TestMath();
    if (Failures != 0) {
        printf("%d math failures.\n", Failures);
        TotalFailures += Failures;
    }

    Failures = TestMathFloat();
    if (Failures != 0) {
        printf("%d math float failures.\n", Failures);
        TotalFailures += Failures;
    }

    Failures = TestGetopt();
    if (Failures != 0) {
        printf("%d getopt failures.\n", Failures);
        TotalFailures += Failures;
    }

    if (TotalFailures != 0) {
        printf("*** %d C library test failures ***\n", TotalFailures);

    } else {
        printf("All C library tests passed.\n");
    }

    return TotalFailures;
}

//
// --------------------------------------------------------- Internal Functions
//

void
_assert (
    const char *Expression,
    const char *File,
    int Line
    )

/*++

Routine Description:

    This routine implements the underlying assert function that backs the
    assert macro.

Arguments:

    File - Supplies a pointer to the null terminated string describing the file
        the assertion failure occurred in.

    Line - Supplies the line number the assertion failure occurred on.

    Expression - Supplies a pointer to the string representation of the
        source expression that failed.

Return Value:

    This routine does not return.

--*/

{

    fprintf(stderr, "Assertion failure: %s: %d: %s\n", File, Line, Expression);
    abort();
}
