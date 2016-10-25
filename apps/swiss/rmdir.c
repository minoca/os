/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rmdir.c

Abstract:

    This module implements support for the rmdir utility.

Author:

    Evan Green 30-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define RMDIR_VERSION_MAJOR 1
#define RMDIR_VERSION_MINOR 0

#define RMDIR_USAGE                                                            \
    "usage: rmdir [-p] dirs...\n\n"                                            \
    "The rmdir utility removes the named empty directories.\n\n"               \
    "  -p, --parents -- Remove all directories in a pathname. For each \n"     \
    "        operand, rmdir will be called on each component of the path.\n"   \
    "  -v, --verbose -- Verbose. Print each directory removed.\n"              \
    "  --help -- Display this help text.\n"                                    \
    "  --version -- Display version information and exit.\n\n"

#define RMDIR_OPTIONS_STRING "pv"

//
// Define rmdir options.
//

//
// Set this option to print each directory that's deleted.
//

#define RMDIR_OPTION_VERBOSE 0x00000001

//
// Set this option to split arguments into components and remove each one.
//

#define RMDIR_OPTION_REMOVE_PARENTS 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
RmdirRemoveDirectory (
    ULONG Options,
    PSTR Argument
    );

//
// -------------------------------------------------------------------- Globals
//

struct option RmdirLongOptions[] = {
    {"parents", no_argument, 0, 'p'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
RmdirMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the rmdir program.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    BOOL DidSomething;
    PSTR LastSlash;
    ULONG Length;
    INT Option;
    ULONG Options;
    int Status;
    int TotalStatus;

    Options = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             RMDIR_OPTIONS_STRING,
                             RmdirLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            return Status;
        }

        switch (Option) {
        case 'p':
            Options |= RMDIR_OPTION_REMOVE_PARENTS;
            break;

        case 'v':
            Options |= RMDIR_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(RMDIR_VERSION_MAJOR, RMDIR_VERSION_MINOR);
            return 1;

        case 'h':
            printf(RMDIR_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            return Status;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // Loop through the remaining arguments and remove the files.
    //

    DidSomething = FALSE;
    TotalStatus = 0;
    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        DidSomething = TRUE;
        Status = RmdirRemoveDirectory(Options, Argument);
        if (Status != 0) {
            TotalStatus = Status;

        } else if ((Options & RMDIR_OPTION_REMOVE_PARENTS) != 0) {
            Length = strlen(Argument);
            while ((Length != 0) && (Argument[Length - 1] == '/')) {
                Argument[Length - 1] = '\0';
                Length -= 1;
            }

            //
            // Remove each of the parent components as well.
            //

            LastSlash = strrchr(Argument, '/');
            while ((LastSlash != NULL) && (LastSlash != Argument)) {
                *LastSlash = '\0';
                Status = RmdirRemoveDirectory(Options, Argument);
                if (Status != 0) {
                    TotalStatus = Status;
                    break;
                }

                LastSlash = strrchr(Argument, '/');
            }
        }

        ArgumentIndex += 1;
    }

    if (DidSomething == FALSE) {
        SwPrintError(0, NULL, "Missing operand");
        TotalStatus = 1;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
RmdirRemoveDirectory (
    ULONG Options,
    PSTR Argument
    )

/*++

Routine Description:

    This routine removes the given directory.

Arguments:

    Options - Supplies the bitfield of options. See RMDIR_OPTION_*
        definitions.

    Argument - Supplies a pointer to a null terminated string containing the
        directory path to delete.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR QuotedArgument;
    INT Result;

    Result = SwRemoveDirectory(Argument);
    if (Result == 0) {
        if ((Options & RMDIR_OPTION_VERBOSE) != 0) {
            QuotedArgument = SwQuoteArgument(Argument);
            printf("rmdir: Removed directory '%s'.\n", QuotedArgument);
            if (QuotedArgument != Argument) {
                free(QuotedArgument);
            }
        }

        return 0;
    }

    Result = errno;
    SwPrintError(Result, Argument, "Could not remove directory");
    return Result;
}

