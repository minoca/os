/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    env.c

Abstract:

    This module implements support for the env utility.

Author:

    Evan Green 26-Aug-2013

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

#define ENV_VERSION_MAJOR 1
#define ENV_VERSION_MINOR 0

#define ENV_USAGE \
    "usage: env [-i] [-] [-u name] [name=value]... [utility [argument...]]\n"  \
    "The env utility executes the given utility after setting the given \n"    \
    "environment variables. If no utility is supplied, the resulting \n"       \
    "environment shall be written to standard output.\n"                       \
    "Options are:\n"                                                           \
    "  -i, --ignore-environment -- Invoke the utility with exactly the \n"     \
    "        environment specified; the inherited environment shall be \n"     \
    "        ignored completely. A lone - is equivalent to the -i option.\n"   \
    "  -u, --unset <name> -- Unset the given environment variable.\n"          \
    "  --help -- Display this help text and exit.\n"                           \
    "  --version -- Display the application version and exit.\n"

#define ENV_OPTIONS_STRING "+iu:"

//
// Set this option to disable inheriting of the current environment.
//

#define ENV_OPTION_NO_INHERIT 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

extern char **environ;

struct option EnvLongOptions[] = {
    {"ignore-environment", no_argument, NULL, 'i'},
    {"unset", required_argument, NULL, 'u'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
EnvMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the env utility.

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
    ULONG EnvironmentIndex;
    INT Option;
    ULONG Options;
    int Status;

    Options = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             ENV_OPTIONS_STRING,
                             EnvLongOptions,
                             NULL);

        if (Option == -1) {

            //
            // Check for a lonely dash, which is the same as -i.
            //

            ArgumentIndex = optind;
            if (ArgumentIndex < ArgumentCount) {
                Argument = Arguments[ArgumentIndex];
                if (strcmp(Argument, "-") == 0) {
                    Options |= ENV_OPTION_NO_INHERIT;
                    optind += 1;
                }
            }

            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'i':
            Options |= ENV_OPTION_NO_INHERIT;
            break;

        case 'V':
            SwPrintVersion(ENV_VERSION_MAJOR, ENV_VERSION_MINOR);
            return 1;

        case 'h':
            printf(ENV_USAGE);
            return 1;

        case 'u':
            Argument = optarg;

            assert(Argument != NULL);

            //
            // Calling putenv without an equals unsets the variable.
            //

            putenv(Argument);
            break;

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

    //
    // Clear the environment if requested.
    //

    if ((Options & ENV_OPTION_NO_INHERIT) != 0) {
        environ = NULL;
    }

    //
    // Loop setting environment variables as long as there are arguments with
    // an equals sign.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        if (strchr(Argument, '=') == NULL) {
            break;
        }

        Status = putenv(Argument);
        if (Status != 0) {
            SwPrintError(errno, Argument, "Failed to set variable");
            goto MainEnd;
        }

        ArgumentIndex += 1;
    }

    //
    // If there are more arguments, then execute that utility with those
    // arguments.
    //

    if (ArgumentIndex < ArgumentCount) {
        Status = SwExec(Arguments[ArgumentIndex],
                        Arguments + ArgumentIndex,
                        ArgumentCount - ArgumentIndex);

        SwPrintError(errno, Arguments[ArgumentIndex], "Failed to exec");
        goto MainEnd;
    }

    //
    // There are no arguments, so print the current environment.
    //

    if (environ == NULL) {
        Status = 0;
        goto MainEnd;
    }

    EnvironmentIndex = 0;
    while (environ[EnvironmentIndex] != NULL) {
        printf("%s\n", environ[EnvironmentIndex]);
        EnvironmentIndex += 1;
    }

    Status = 0;

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

