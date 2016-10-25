/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    reboot.c

Abstract:

    This module implements a simple utility that resets the system.

Author:

    Evan Green 6-Oct-2014

Environment:

    User Mode

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

#define REBOOT_VERSION_MAJOR 1
#define REBOOT_VERSION_MINOR 0

#define REBOOT_USAGE                                                           \
    "usage: reboot [-cwsq]\n"                                                  \
    "The reboot utility resets the system immediately. Options are: \n"        \
    "  -c, --cold -- Perform a cold reboot.\n"                                 \
    "  -w, --warm -- Perform a warm reboot. This is the default.\n"            \
    "  -s, --shutdown -- Perform a shutdown and power off.\n"                  \
    "  -q, --quiet -- Do not print a message.\n"                               \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define REBOOT_OPTIONS_STRING "cwsqh"

//
// This option disables printing a message.
//

#define REBOOT_OPTION_QUIET 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option RebootLongOptions[] = {
    {"cold", no_argument, 0, 'c'},
    {"warm", no_argument, 0, 'w'},
    {"halt", no_argument, 0, 's'},
    {"quiet", no_argument, 0, 'q'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
RebootMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the reboot utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    struct tm *CurrentTimeFields;
    PSTR CurrentTimeString;
    time_t CurrentTimeValue;
    INT Option;
    ULONG Options;
    SWISS_REBOOT_TYPE RebootType;
    PSTR RebootTypeString;
    int Status;

    Options = 0;
    RebootType = RebootTypeWarm;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             REBOOT_OPTIONS_STRING,
                             RebootLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'c':
            RebootType = RebootTypeCold;
            break;

        case 'w':
            RebootType = RebootTypeWarm;
            break;

        case 's':
            RebootType = RebootTypeHalt;
            break;

        case 'q':
            Options |= REBOOT_OPTION_QUIET;
            break;

        case 'V':
            SwPrintVersion(REBOOT_VERSION_MAJOR, REBOOT_VERSION_MINOR);
            return 1;

        case 'h':
            printf(REBOOT_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = EINVAL;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, NULL, "Unexpected argument");
        Status = 1;
        goto MainEnd;
    }

    if ((Options & REBOOT_OPTION_QUIET) == 0) {
        CurrentTimeValue = time(NULL);
        CurrentTimeFields = localtime(&CurrentTimeValue);
        CurrentTimeString = asctime(CurrentTimeFields);
        switch (RebootType) {
        case RebootTypeCold:
            RebootTypeString = "cold reboot";
            break;

        case RebootTypeWarm:
            RebootTypeString = "warm reboot";
            break;

        case RebootTypeHalt:
            RebootTypeString = "shutdown";
            break;

        default:

            assert(FALSE);

            Status = EINVAL;
            goto MainEnd;
        }

        printf("Requesting %s at %s", RebootTypeString, CurrentTimeString);
    }

    fflush(NULL);
    Status = SwRequestReset(RebootType);

MainEnd:
    if (Status != 0) {
        SwPrintError(Status, NULL, "reboot failed");
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

