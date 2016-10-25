/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dirname.c

Abstract:

    This module implements the dirname utility, which returns the directory
    portion of the given path name.

Author:

    Evan Green 30-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <errno.h>
#include <libgen.h>
#include <string.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define DIRNAME_VERSION_MAJOR 1
#define DIRNAME_VERSION_MINOR 0
#define DIRNAME_USAGE                                                         \
    "Usage: dirname <path>\n"                                                 \
    "The dirname utility returns the directory portion of the given path.\n\n"
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
DirnameMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the dirname utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Returns a positive value if an error occurred.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    PSTR Name;
    PSTR Result;
    BOOL SkipControlArguments;

    Name = NULL;
    if (ArgumentCount < 2) {
        fprintf(stderr, DIRNAME_USAGE);
        return 1;
    }

    //
    // Loop through processing arguments.
    //

    SkipControlArguments = TRUE;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        if ((SkipControlArguments != FALSE) && (Argument[0] == '-')) {
            Argument += 1;
            if (strcmp(Argument, "-help") == 0) {
                printf(DIRNAME_USAGE);
                return 1;

            } else if (strcmp(Argument, "-version") == 0) {
                SwPrintVersion(DIRNAME_VERSION_MAJOR, DIRNAME_VERSION_MINOR);
                return 1;

            } else if (strcmp(Argument, "-") == 0) {
                SkipControlArguments = FALSE;
            }

            continue;
        }

        if (Name == NULL) {
            Name = Argument;

        } else {
            fprintf(stderr, DIRNAME_USAGE);
            return 1;
        }
    }

    if (Name == NULL) {
        fprintf(stderr, DIRNAME_USAGE);
    }

    Result = dirname(Name);
    if (Result == NULL) {
        SwPrintError(errno, Name, "Unable to get dirname of");
        return errno;
    }

    printf("%s\n", Result);
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

