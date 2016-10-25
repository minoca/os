/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testmm.c

Abstract:

    This module implements the test cases for the kernel memory manager.

Author:

    Evan Green 27-Jul-2012

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "../mmp.h"
#include "testmm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
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

    This routine is the entry point for the MM test program. It executes the
    tests.

Arguments:

    ArgumentCount - Supplies the number of arguments specified on the command
        line.

    Arguments - Supplies an array of strings representing the command line
        arguments.

Return Value:

    returns 0 on success, or nonzero on failure.

--*/

{

    ULONG Failures;
    time_t Seed;
    ULONG TotalTestsFailed;

    Seed = time(NULL);
    srand(Seed);
    TotalTestsFailed = 0;
    Failures = TestMdls();
    if (Failures != 0) {
        printf("\nMDL test had %d failures.\n", Failures);
    }

    TotalTestsFailed += Failures;
    Failures = TestUserVa();
    if (Failures != 0) {
        printf("\nUser VA test had %d failures.\n", Failures);
    }

    TotalTestsFailed += Failures;

    //
    // Tests are over, print results.
    //

    if (TotalTestsFailed != 0) {
        printf("Seed was %lu\n", (long unsigned int)Seed);
        printf("*** %d Failure(s) in MM Test. ***\n", TotalTestsFailed);
        return 1;
    }

    printf("All MM tests passed.\n");
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

