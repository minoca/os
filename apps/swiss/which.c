/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    which.c

Abstract:

    This module implements the which utility.

Author:

    Evan Green 15-Aug-2016

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

#define WHICH_VERSION_MAJOR 1
#define WHICH_VERSION_MINOR 0

#define WHICH_USAGE                                                            \
    "usage: cp [-a|-s] executable...\n"                                        \
    "The which utility prints the full path to an executable that would \n"    \
    "have been run had the argument been typed into a shell. Options are:\n"   \
    "  -a, --all -- Print all valid paths, not just the first one.\n"          \
    "  -s, --silent -- Do not print anything.\n"                               \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"     \
    "Returns 0 if all command line paths evaluated to executable paths.\n"     \
    "Returns 1 if one or more of the paths are not executables.\n"             \
    "Returns 2 on other failures.\n"                                           \

#define WHICH_OPTIONS_STRING "ashV"

//
// This flag is set if which should print out all valid matches.
//

#define WHICH_OPTION_ALL 0x00000001

//
// This flag is set if which should not print anything out.
//

#define WHICH_OPTION_SILENT 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
WhichPrintMatches (
    PSTR Argument,
    PSTR Path,
    ULONG Options
    );

BOOL
WhichIsExecutable (
    PSTR Argument,
    ULONG Options
    );

//
// -------------------------------------------------------------------- Globals
//

struct option WhichLongOptions[] = {
    {"all", no_argument, 0, 'a'},
    {"silent", no_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
WhichMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the which utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    INT Option;
    ULONG Options;
    PSTR Path;
    PSTR RawPath;
    INT Status;
    INT TotalStatus;

    Options = 0;
    Path = NULL;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             WHICH_OPTIONS_STRING,
                             WhichLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 2;
            goto MainEnd;
        }

        switch (Option) {
        case 'a':
            Options |= WHICH_OPTION_ALL;
            break;

        case 's':
            Options |= WHICH_OPTION_SILENT;
            break;

        case 'V':
            SwPrintVersion(WHICH_VERSION_MAJOR, WHICH_VERSION_MINOR);
            return 2;

        case 'h':
            printf(WHICH_USAGE);
            return 2;

        default:

            assert(FALSE);

            Status = 2;
            goto MainEnd;
        }
    }

    //
    // Create a copy of the PATH variable that can be modified.
    //

    RawPath = getenv("PATH");
    if (RawPath == NULL) {
        RawPath = "";
    }

    Path = strdup(RawPath);
    if (Path == NULL) {
        Status = 2;
        goto MainEnd;
    }

    ArgumentIndex = optind;
    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0, NULL, "Argument expected");
        Status = 2;
        goto MainEnd;
    }

    while (ArgumentIndex < ArgumentCount) {
        Status = WhichPrintMatches(Arguments[ArgumentIndex], Path, Options);
        if (Status == 2) {
            TotalStatus = Status;

        } else if (Status == 1) {
            if (TotalStatus == 0) {
                TotalStatus = 1;
            }
        }

        ArgumentIndex += 1;
    }

MainEnd:
    if ((Status != 0) && (TotalStatus == 0)) {
        TotalStatus = Status;
    }

    if (Path != NULL) {
        free(Path);
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
WhichPrintMatches (
    PSTR Argument,
    PSTR Path,
    ULONG Options
    )

/*++

Routine Description:

    This routine prints the matching paths for the given argument.

Arguments:

    Argument - Supplies a pointer to the executable argument.

    Path - Supplies a pointer to the PATH variable whose buffer can be modified.

    Options - Supplies the WHICH_OPTION_* bitfield.

Return Value:

    0 on success.

    1 on no match.

    2 on other errors.

--*/

{

    PSTR CompletePath;
    UINTN CompletePathSize;
    BOOL FoundSomething;
    BOOL IsExecutable;
    PSTR NextSeparator;

    if (strchr(Argument, '/') != NULL) {
        if (WhichIsExecutable(Argument, Options) != FALSE) {
            return 0;
        }

        return 1;
    }

    CompletePathSize = strlen(Argument) + strlen(Path) + 2;
    CompletePath = malloc(CompletePathSize);
    if (CompletePath == NULL) {
        return 2;
    }

    FoundSomething = FALSE;
    while (TRUE) {
        NextSeparator = strchr(Path, PATH_LIST_SEPARATOR);
        if (NextSeparator != NULL) {
            *NextSeparator = '\0';
        }

        snprintf(CompletePath, CompletePathSize, "%s/%s", Path, Argument);
        if (NextSeparator != NULL) {
            *NextSeparator = PATH_LIST_SEPARATOR;
        }

        IsExecutable = WhichIsExecutable(CompletePath, Options);
        if (IsExecutable != FALSE) {
            FoundSomething = TRUE;
            if ((Options & WHICH_OPTION_ALL) == 0) {
                goto PrintMatchesEnd;
            }
        }

        if (NextSeparator != NULL) {
            Path = NextSeparator + 1;

        } else {
            break;
        }
    }

PrintMatchesEnd:
    free(CompletePath);
    if (FoundSomething != FALSE) {
        return 0;
    }

    return 1;
}

BOOL
WhichIsExecutable (
    PSTR Argument,
    ULONG Options
    )

/*++

Routine Description:

    This routine determines if the given path is an executable file or not.
    If it is, the path is printed (unless specified otherwise in the options).

Arguments:

    Argument - Supplies a pointer to the executable argument.

    Options - Supplies a pointer to the options. See WHICH_OPTION_* definitions.

Return Value:

    TRUE if the given path is to an executable file.

    FALSE if not.

--*/

{

    struct stat Stat;

    if (stat(Argument, &Stat) != 0) {
        return FALSE;
    }

    if ((S_ISREG(Stat.st_mode)) &&
        ((Stat.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)) {

        if ((Options & WHICH_OPTION_SILENT) == 0) {
            puts(Argument);
        }

        return TRUE;
    }

    return FALSE;
}

