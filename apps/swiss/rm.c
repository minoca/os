/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rm.c

Abstract:

    This module implements the "rm" (remove) utility that is used to delete
    files and directories.

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

#define RM_VERSION_MAJOR 1
#define RM_VERSION_MINOR 0

#define RM_USAGE                                                            \
    "usage: rm [-fiRrv] files...\n\n"                                       \
    "The rm utility removes the named files or directories.\n\n"            \
    "  -f, --force -- Skip all prompts.\n"                                  \
    "  -i, --intractive -- Interactive mode. Prompt for each file.\n"       \
    "  -R, --recursive -- Recursive. Delete the contents inside all \n"     \
    "        directories specified.\n"                                      \
    "  -r -- Same as -R.\n"                                                 \
    "  -v, --verbose -- Verbose, print each file being removed.\n"          \
    "  --help -- Display this help text.\n"                                 \
    "  --version -- Display version information and exit.\n\n"

#define RM_OPTIONS_STRING "fiRrv"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option RmLongOptions[] = {
    {"force", no_argument, 0, 'f'},
    {"interactive", no_argument, 0, 'i'},
    {"recursive", no_argument, 0, 'r'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
RmMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the rm program.

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
    INT Option;
    ULONG Options;
    int Status;
    int TotalStatus;

    Options = 0;
    if (isatty(STDIN_FILENO)) {
        Options |= DELETE_OPTION_STDIN_IS_TERMINAL;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             RM_OPTIONS_STRING,
                             RmLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            return Status;
        }

        switch (Option) {
        case 'f':
            Options |= DELETE_OPTION_FORCE;
            Options &= ~DELETE_OPTION_INTERACTIVE;
            break;

        case 'i':
            Options |= DELETE_OPTION_INTERACTIVE;
            Options &= ~DELETE_OPTION_FORCE;
            break;

        case 'r':
        case 'R':
            Options |= DELETE_OPTION_RECURSIVE;
            break;

        case 'v':
            Options |= DELETE_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(RM_VERSION_MAJOR, RM_VERSION_MINOR);
            return 1;

        case 'h':
            printf(RM_USAGE);
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
    // Loop through the arguments again and remove the files.
    //

    DidSomething = FALSE;
    TotalStatus = 0;
    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        DidSomething = TRUE;
        Status = SwDelete(Options, Argument);
        if (Status != 0) {
            TotalStatus = Status;
        }

        ArgumentIndex += 1;
    }

    if ((DidSomething == FALSE) && ((Options & DELETE_OPTION_FORCE) == 0)) {
        SwPrintError(0, NULL, "Missing operand. Try --help for usage");
        TotalStatus = 1;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

