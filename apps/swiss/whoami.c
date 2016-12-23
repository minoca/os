/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    whoami.c

Abstract:

    This module implements the whoami utility, which prints out the user name
    associated with the current effective user ID.

Author:

    Vasco Costa 7-Dec-2016

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define WHOAMI_VERSION_MAJOR 1
#define WHOAMI_VERSION_MINOR 0

#define WHOAMI_USAGE                                                        \
    "usage: whoami [options]\n"                                              \
    "Print the user name associated with the current effective user ID. \n" \
    "Same as id -un.\n"                                                     \
    "Options are:\n"                                                        \
    "  --help -- Show this help text and exit.\n"                           \
    "  --version -- Print the application version information and exit.\n"

#define WHOAMI_OPTIONS_STRING "h"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
WhoamiPrintUserName (
    uid_t UserId
    );

//
// -------------------------------------------------------------------- Globals
//

struct option WhoamiLongOptions[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
WhoamiMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the whoami utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    uid_t EffectiveUserId;
    INT Option;
    int Status;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             WHOAMI_OPTIONS_STRING,
                             WhoamiLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'V':
            SwPrintVersion(WHOAMI_VERSION_MAJOR, WHOAMI_VERSION_MINOR);
            return 1;

        case 'h':
            printf(WHOAMI_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    Status = 0;
    EffectiveUserId = SwGetEffectiveUserId();
    WhoamiPrintUserName(EffectiveUserId);
    printf("\n");

MainEnd:
    return Status;
}

VOID
WhoamiPrintUserName (
    uid_t UserId
    )

/*++

Routine Description:

    This routine prints a user name.

Arguments:

    UserId - Supplies the ID of the user to print.

Return Value:

    None.

--*/

{

    PSTR UserName;

    UserName = NULL;
    if (SwGetUserNameFromId(UserId, &UserName) != 0) {

        assert(UserName == NULL);

        printf("%u", (unsigned int)UserId);
        return;
    }

    printf("%s", UserName);
    if (UserName != NULL) {
        free(UserName);
    }

    return;
}
