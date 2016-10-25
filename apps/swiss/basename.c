/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    basename.c

Abstract:

    This module implements the basename utility, which returns the file
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

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BASENAME_VERSION_MAJOR 1
#define BASENAME_VERSION_MINOR 0
#define BASENAME_USAGE                                                         \
    "Usage: basename <path> [suffix]\n"                                        \
    "The basename utility returns the file name portion of the given path. \n" \
    "If the suffix string is provided and the basename string ends in \n"      \
    "the given suffix (but is not only the suffix), then the suffix will \n"   \
    "be removed from the string before being printed.\n\n"

#define BASENAME_OPTIONS_STRING "h"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option BasenameLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
BasenameMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the basename utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Returns a positive value if an error occurred.

--*/

{

    INT ArgumentIndex;
    LONG Index;
    PSTR Name;
    INT Option;
    PSTR Result;
    size_t ResultLength;
    PSTR Suffix;
    size_t SuffixLength;

    Name = NULL;
    Suffix = NULL;

    //
    // Loop through processing arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             BASENAME_OPTIONS_STRING,
                             BasenameLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            return 1;
        }

        switch (Option) {
        case 'V':
            SwPrintVersion(BASENAME_VERSION_MAJOR, BASENAME_VERSION_MINOR);
            return 1;

        case 'h':
            printf(BASENAME_USAGE);
            return 1;

        default:

            assert(FALSE);

            return 1;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        Name = Arguments[ArgumentIndex];
        if (ArgumentIndex + 1 < ArgumentCount) {
            Suffix = Arguments[ArgumentIndex + 1];
        }
    }

    if (Name == NULL) {
        fprintf(stderr, BASENAME_USAGE);
        return 1;
    }

    Result = basename(Name);
    if (Result == NULL) {
        SwPrintError(errno, Name, "Unable to get basename of");
        return errno;
    }

    //
    // If there's a suffix, determine if the result ends in the given suffix.
    // Don't do this if the result length is less than or equal to the suffix
    // length.
    //

    if (Suffix != NULL) {
        ResultLength = strlen(Result);
        SuffixLength = strlen(Suffix);
        if (ResultLength > SuffixLength) {
            for (Index = 0; Index < SuffixLength; Index += 1) {
                if (Result[ResultLength - Index - 1] !=
                    Suffix[SuffixLength - Index - 1]) {

                    break;
                }
            }

            if (Index == SuffixLength) {
                Result[ResultLength - SuffixLength] = '\0';
            }
        }
    }

    printf("%s\n", Result);
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

