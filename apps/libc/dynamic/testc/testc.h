/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testc.h

Abstract:

    This header contains definitions for the C library test

Author:

    Evan Green 9-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../libcp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

ULONG
TestRegularExpressions (
    VOID
    );

/*++

Routine Description:

    This routine implements the entry point for the regular expression tests.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

ULONG
TestQuickSort (
    VOID
    );

/*++

Routine Description:

    This routine implements the entry point for the quicksort test.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

ULONG
TestBinarySearch (
    VOID
    );

/*++

Routine Description:

    This routine implements the entry point for the binary search test.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

ULONG
TestMath (
    VOID
    );

/*++

Routine Description:

    This routine implements the entry point for the C library math test.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

ULONG
TestMathFloat (
    VOID
    );

/*++

Routine Description:

    This routine implements the entry point for the C library single-precision
    floating point math test.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

ULONG
TestGetopt (
    VOID
    );

/*++

Routine Description:

    This routine tests the getopt functions.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

