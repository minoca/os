/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    nproc.c

Abstract:

    This module implements the nproc utility, which simply reports the number
    of processors in the system.

Author:

    Evan Green 4-Mar-2016

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

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define NPROC_VERSION_MAJOR 1
#define NPROC_VERSION_MINOR 0

#define NPROC_USAGE                                                            \
    "usage: nproc [options...]\n\n"                                            \
    "The nproc utility reports the number of processors in the system. \n"     \
    "Options are:\n"                                                           \
    "  -a, --all -- Report the number of installed processors, rather than \n" \
    "      the number of active processors.\n"                                 \
    "  -i, --ignore=N -- Exclude N processors, if possible.\n"                 \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define NPROC_OPTIONS_STRING "ai:hV"

#define NPROC_OPTION_ALL 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option NprocLongOptions[] = {
    {"all", no_argument, 0, 'a'},
    {"ignore", required_argument, 0, 'i'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
NprocMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the nproc utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    ULONG ArgumentIndex;
    INT Count;
    BOOL GetOnline;
    INT Ignore;
    INT Option;
    ULONG Options;
    int Status;

    Ignore = 0;
    Options = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             NPROC_OPTIONS_STRING,
                             NprocLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'a':
            Options |= NPROC_OPTION_ALL;
            break;

        case 'i':
            Ignore = strtoul(optarg, &AfterScan, 10);
            if ((AfterScan == optarg) || (*AfterScan != '\0') || (Ignore < 0)) {
                SwPrintError(0, optarg, "Invalid number");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'V':
            SwPrintVersion(NPROC_VERSION_MAJOR, NPROC_VERSION_MINOR);
            return 1;

        case 'h':
            printf(NPROC_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, Arguments[ArgumentIndex], "Unexpected operand");
        Status = 1;
        goto MainEnd;
    }

    GetOnline = FALSE;
    if ((Options & NPROC_OPTION_ALL) == 0) {
        GetOnline = TRUE;
    }

    Count = SwGetProcessorCount(GetOnline);
    if (Count < 1) {
        Count = 1;
    }

    if (Ignore >= Count) {
        Count = 1;

    } else {
        Count -= Ignore;
    }

    printf("%d\n", Count);
    Status = 0;

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

