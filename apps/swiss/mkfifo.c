/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mkfifo.c

Abstract:

    This module implements the mkfifo utility, which creates a named pipe.

Author:

    Evan Green 28-May-2015

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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MKFIFO_VERSION_MAJOR 1
#define MKFIFO_VERSION_MINOR 0

#define MKFIFO_USAGE                                                           \
    "usage: mkfifo [options] files...\n"                                       \
    "The mkfifo utility creates one or more named pipe. Options are:\n"        \
    "  -m, --mode=mode -- Set the file permission bits. Default is \n"         \
    "      read/write on all, minus the umask.\n"                              \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define MKFIFO_OPTIONS_STRING "m:"

//
// Define the default creation permissions for the pipe.
//

#define MKFIFO_DEFAULT_PERMISSIONS \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

//
// Define mkfifo options.
//

//
// This option is set if a mode was explicitly supplied.
//

#define MKFIFO_OPTION_MODE_SPECIFIED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option MkfifoLongOptions[] = {
    {"mode", required_argument, 0, 'm'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
MkfifoMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the mkfifo utility.

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
    mode_t Mode;
    INT Option;
    ULONG Options;
    mode_t OriginalUmask;
    BOOL Result;
    int Status;
    int TotalStatus;

    Mode = MKFIFO_DEFAULT_PERMISSIONS;
    Options = 0;
    OriginalUmask = 0;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             MKFIFO_OPTIONS_STRING,
                             MkfifoLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'm':
            Result = SwParseFilePermissionsString(optarg, FALSE, &Mode);
            if (Result == FALSE) {
                SwPrintError(0, optarg, "Invalid permissions");
                Status = 1;
                goto MainEnd;
            }

            Options |= MKFIFO_OPTION_MODE_SPECIFIED;
            OriginalUmask = umask(0);
            break;

        case 'V':
            SwPrintVersion(MKFIFO_VERSION_MAJOR, MKFIFO_VERSION_MINOR);
            return 1;

        case 'h':
            printf(MKFIFO_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex == ArgumentCount) {
        SwPrintError(0, NULL, "Argument expected. Try --help for usage");
        Status = 1;
        goto MainEnd;
    }

    //
    // Loop through the arguments again and perform the moves.
    //

    Status = 0;
    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        Status = mkfifo(Argument, Mode);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, Argument, "Cannot create fifo");
            TotalStatus = Status;
        }
    }

MainEnd:

    //
    // Restore the umask if it was changed.
    //

    if ((Options & MKFIFO_OPTION_MODE_SPECIFIED) != 0) {
        umask(OriginalUmask);
    }

    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

