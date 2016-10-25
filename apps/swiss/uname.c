/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uname.c

Abstract:

    This module implements support for the uname utility.

Author:

    Evan Green 17-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <getopt.h>
#include <string.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define UNAME_VERSION_MAJOR 1
#define UNAME_VERSION_MINOR 0

#define UNAME_USAGE                                                            \
    "usage: uname [-asnrvm]\n"                                                 \
    "The uname utility prints out the system name and version number."         \
    "Options are:\n"                                                           \
    "  -a, --all -- Turns on all options and prints them out separated by "    \
    "spaces.\n"                                                                \
    "  -s, --kernel-name -- Print the system name.\n"                          \
    "  -n, --nodename -- Print out the name of this system on the network.\n"  \
    "  -r, --kernel-release -- Print out the system release number string.\n"  \
    "  -v, --kernel-version -- Print out the version string within this "      \
    "release.\n"                                                               \
    "  -m, --machine -- Print out the machine type.\n"                         \
    "  --help -- Display this help text and exit.\n"                           \
    "  --version -- Display the application version and exit.\n"               \

#define UNAME_OPTIONS_STRING "asnrvm"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option UnameLongOptions[] = {
    {"all", no_argument, 0, 'a'},
    {"kernel-name", no_argument, 0, 's'},
    {"nodename", no_argument, 0, 'n'},
    {"kernel-release", no_argument, 0, 'r'},
    {"kernel-version", no_argument, 0, 'v'},
    {"machine", no_argument, 0, 'm'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
UnameMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the uname utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    SYSTEM_NAME Name;
    INT Option;
    BOOL PrintedSomething;
    BOOL PrintMachine;
    BOOL PrintNodeName;
    BOOL PrintRelease;
    BOOL PrintSystemName;
    BOOL PrintVersion;
    int Status;

    PrintSystemName = FALSE;
    PrintNodeName = FALSE;
    PrintRelease = FALSE;
    PrintVersion = FALSE;
    PrintMachine = FALSE;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             UNAME_OPTIONS_STRING,
                             UnameLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            return Status;
        }

        switch (Option) {
        case 'a':
            PrintSystemName = TRUE;
            PrintNodeName = TRUE;
            PrintRelease = TRUE;
            PrintVersion = TRUE;
            PrintMachine = TRUE;
            break;

        case 's':
            PrintSystemName = TRUE;
            break;

        case 'n':
            PrintNodeName = TRUE;
            break;

        case 'r':
            PrintRelease = TRUE;
            break;

        case 'v':
            PrintVersion = TRUE;
            break;

        case 'm':
            PrintMachine = TRUE;
            break;

        case 'V':
            SwPrintVersion(UNAME_VERSION_MAJOR, UNAME_VERSION_MINOR);
            return 1;

        case 'h':
            printf(UNAME_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            return Status;
        }
    }

    //
    // If everything is off, just print the system name.
    //

    if ((PrintSystemName == FALSE) && (PrintNodeName == FALSE) &&
        (PrintRelease == FALSE) && (PrintVersion == FALSE) &&
        (PrintMachine == FALSE)) {

        PrintSystemName = TRUE;
    }

    Status = SwGetSystemName(&Name);
    if (Status != 0) {
        goto MainEnd;
    }

    PrintedSomething = FALSE;
    if (PrintSystemName != FALSE) {
        printf("%s", Name.SystemName);
        PrintedSomething = TRUE;
    }

    if (PrintNodeName != FALSE) {
        if (PrintedSomething != FALSE) {
            fputc(' ', stdout);
        }

        printf("%s", Name.NodeName);
        PrintedSomething = TRUE;
    }

    if (PrintRelease != FALSE) {
        if (PrintedSomething != FALSE) {
            fputc(' ', stdout);
        }

        printf("%s", Name.Release);
        PrintedSomething = TRUE;
    }

    if (PrintVersion != FALSE) {
        if (PrintedSomething != FALSE) {
            fputc(' ', stdout);
        }

        printf("%s", Name.Version);
        PrintedSomething = TRUE;
    }

    if (PrintMachine != FALSE) {
        if (PrintedSomething != FALSE) {
            fputc(' ', stdout);
        }

        printf("%s", Name.Machine);
        PrintedSomething = TRUE;
    }

    if (PrintedSomething != FALSE) {
        fputc('\n', stdout);
    }

    Status = 0;

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

