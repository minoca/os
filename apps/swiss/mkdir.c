/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mkdir.c

Abstract:

    This module implements the mkdir (make directory) utility.

Author:

    Evan Green 24-Jun-2013

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

#define MKDIR_VERSION_MAJOR 1
#define MKDIR_VERSION_MINOR 0

#define MKDIR_USAGE                                                            \
    "usage: mkdir [options] [dirs...]\n\n"                                     \
    "The mkdir utility creates one or more directories.\n\n"                   \
    "    -m, --mode=MODE -- Set the mode to the given file permissions "       \
    "(filtered through the umask).\n"                                          \
    "    -p, --parents -- Create any intermediate directories in the path "    \
    "that do not exist.\n"                                                     \
    "    -v, --verbose -- Print a message for every directory created.\n"      \
    "    --help -- Display this help text.\n"                                  \
    "    --version -- Display version information and exit.\n\n"

#define MKDIR_OPTIONS_STRING "m:pvhV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option MkdirLongOptions[] = {
    {"mode", required_argument, 0, 'm'},
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
MkdirMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the mkdir program.

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
    BOOL CreateIntermediatePaths;
    mode_t CreatePermissions;
    INT Option;
    mode_t OriginalUmask;
    BOOL Result;
    int Status;
    int TotalStatus;
    BOOL Verbose;

    CreateIntermediatePaths = FALSE;
    OriginalUmask = umask(0);
    CreatePermissions = MKDIR_DEFAULT_PERMISSIONS & ~OriginalUmask;
    Status = 1;
    TotalStatus = 0;
    Verbose = FALSE;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             MKDIR_OPTIONS_STRING,
                             MkdirLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            goto MainEnd;
        }

        switch (Option) {
        case 'm':
            Argument = optarg;

            assert(Argument != NULL);

            Result = SwParseFilePermissionsString(Argument,
                                                  TRUE,
                                                  &CreatePermissions);

            if (Result == FALSE) {
                SwPrintError(0, NULL, "Invalid mode %s", Argument);
                goto MainEnd;
            }

            break;

        case 'p':
            CreateIntermediatePaths = TRUE;
            break;

        case 'v':
            Verbose = TRUE;
            break;

        case 'V':
            SwPrintVersion(MKDIR_VERSION_MAJOR, MKDIR_VERSION_MINOR);
            goto MainEnd;

        case 'h':
            printf(MKDIR_USAGE);
            goto MainEnd;

        default:

            assert(FALSE);

            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // Loop through the arguments again and create the directories.
    //

    TotalStatus = 0;
    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        Status = SwCreateDirectoryCommand(Argument,
                                          CreateIntermediatePaths,
                                          Verbose,
                                          CreatePermissions);

        if (Status != 0) {
            TotalStatus = Status;
        }

        ArgumentIndex += 1;
    }

    Status = 0;

MainEnd:
    umask(OriginalUmask);
    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

