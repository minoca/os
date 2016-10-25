/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    readlink.c

Abstract:

    This module implements the readlink command, which returns the destination
    of a symbolic link.

Author:

    Evan Green 24-Mar-2015

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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define READLINK_VERSION_MAJOR 1
#define READLINK_VERSION_MINOR 0

#define READLINK_USAGE                                                         \
    "usage: readlink [options] path\n"                                         \
    "The readlink utility prints the destination of a symbolic link. \n"       \
    "Options are:\n"                                                           \
    "  -f, --canonicalize -- Canonicalize the path by following every \n"      \
    "      symbolic link in every component of the path.\n"                    \
    "      is still logged in, or another user uses the same home directory.\n"\
    "  -n, --no-newline -- Do not output a trailing newline.\n"                \
    "  -v, --verbose -- Print error messages.\n"                               \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define READLINK_OPTIONS_STRING "fnvhV"

//
// Define application options.
//

//
// Set this option to canonicalize the path.
//

#define READLINK_OPTION_CANONICALIZE 0x00000001

//
// Set this option to omit the newline.
//

#define READLINK_OPTION_NO_NEWLINE 0x00000002

//
// Set this option to be verbose.
//

#define READLINK_OPTION_VERBOSE 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option ReadlinkLongOptions[] = {
    {"canonicalize", no_argument, 0, 'f'},
    {"no-newline", no_argument, 0, 'n'},
    {"verbose", required_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
ReadlinkMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the readlink utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    PSTR LinkPath;
    INT Option;
    ULONG Options;
    CHAR ResolvedPath[PATH_MAX + 1];
    PSTR Result;
    int Status;

    Options = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             READLINK_OPTIONS_STRING,
                             ReadlinkLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'f':
            Options |= READLINK_OPTION_CANONICALIZE;
            break;

        case 'n':
            Options |= READLINK_OPTION_NO_NEWLINE;
            break;

        case 'v':
            Options |= READLINK_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(READLINK_VERSION_MAJOR, READLINK_VERSION_MINOR);
            return 1;

        case 'h':
            printf(READLINK_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0, NULL, "Argument expected. Try --help for usage");
        return 1;
    }

    LinkPath = Arguments[ArgumentIndex];
    ArgumentIndex += 1;
    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, Arguments[ArgumentIndex], "Unexpected argument");
        Status = 1;
        goto MainEnd;
    }

    if ((Options & READLINK_OPTION_CANONICALIZE) != 0) {
        Result = realpath(LinkPath, ResolvedPath);
        if (Result == NULL) {
            Status = 1;
            if ((Options & READLINK_OPTION_VERBOSE) != 0) {
                SwPrintError(errno, LinkPath, "Failed to get real path");
            }

            goto MainEnd;
        }

    } else {
        Status = readlink(LinkPath, ResolvedPath, sizeof(ResolvedPath) - 1);
        if (Status < 0) {
            Status = 1;
            if ((Options & READLINK_OPTION_VERBOSE) != 0) {
                SwPrintError(errno, LinkPath, "Failed to get link target");
            }

            goto MainEnd;
        }

        ResolvedPath[Status] = '\0';
    }

    ResolvedPath[sizeof(ResolvedPath) - 1] = '\0';
    printf("%s", ResolvedPath);
    if ((Options & READLINK_OPTION_NO_NEWLINE) == 0) {
        printf("\n");
    }

    fflush(NULL);
    Status = 0;

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

