/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    easy.c

Abstract:

    This module implements support for really simple utilities like true and
    false.

Author:

    Evan Green 19-Aug-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SLEEP_VERSION_MAJOR 1
#define SLEEP_VERSION_MINOR 0

#define SLEEP_USAGE                                                            \
    "usage: sleep time\n"                                                      \
    "The sleep utility simply pauses for the specified number of seconds.\n"   \

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
TrueMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the true utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 always.

--*/

{

    return 0;
}

INT
FalseMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the false utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    1 always.

--*/

{

    return 1;
}

INT
SleepMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the sleep utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    1 always.

--*/

{

    PSTR AfterScan;
    long Interval;

    if (ArgumentCount != 2) {
        SwPrintError(0, NULL, "Expected exactly one operand");
        return 1;
    }

    if (strcmp(Arguments[1], "--help") == 0) {
        printf(SLEEP_USAGE);
        return 1;
    }

    if (strcmp(Arguments[1], "--version") == 0) {
        SwPrintVersion(SLEEP_VERSION_MAJOR, SLEEP_VERSION_MINOR);
        return 1;
    }

    Interval = strtol(Arguments[1], &AfterScan, 10);
    if ((Interval < 0) || (AfterScan == Arguments[1])) {
        SwPrintError(0, Arguments[1], "Invalid argument");
        return 2;
    }

    SwSleep((ULONGLONG)Interval * 1000000);
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

