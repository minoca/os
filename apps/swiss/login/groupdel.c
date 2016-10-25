/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    groupdel.c

Abstract:

    This module implements the groupdel command, which deletes a group account
    from the system.

Author:

    Evan Green 11-Mar-2015

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

#include "../swlib.h"
#include "lutil.h"

//
// ---------------------------------------------------------------- Definitions
//

#define GROUPDEL_VERSION_MAJOR 1
#define GROUPDEL_VERSION_MINOR 0

#define GROUPDEL_USAGE                                                         \
    "usage: groupdel [options] groupname\n"                                    \
    "The groupdel utility deletes a group from the system. Options are:\n"     \
    "  -R, --root=dir -- Chroot into the given directory before operation.\n"  \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define GROUPDEL_OPTIONS_STRING "R:HV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option GroupdelLongOptions[] = {
    {"root", required_argument, 0, 'R'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
GroupdelMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the groupdel utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    struct group *Group;
    PSTR GroupName;
    INT Option;
    PSTR RootDirectory;
    struct spwd Shadow;
    int Status;
    int TotalStatus;

    memset(&Shadow, 0, sizeof(Shadow));
    RootDirectory = NULL;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             GROUPDEL_OPTIONS_STRING,
                             GroupdelLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'R':
            RootDirectory = optarg;
            break;

        case 'V':
            SwPrintVersion(GROUPDEL_VERSION_MAJOR, GROUPDEL_VERSION_MINOR);
            return 1;

        case 'H':
            printf(GROUPDEL_USAGE);
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

    //
    // Chroot if requested.
    //

    if (RootDirectory != NULL) {
        Status = chroot(RootDirectory);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, RootDirectory, "Failed to chroot");
            goto MainEnd;
        }

        Status = chdir("/");
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, RootDirectory, "Failed to chdir");
            goto MainEnd;
        }
    }

    GroupName = Arguments[ArgumentIndex];
    ArgumentIndex += 1;
    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, NULL, "Unexpected additional arguments");
        return 1;
    }

    Group = getgrnam(GroupName);
    if (Group == NULL) {
        SwPrintError(0, GroupName, "No such group");
        TotalStatus = ENOENT;
        goto MainEnd;
    }

    //
    // Delete the line out of the group database.
    //

    Status = SwUpdatePasswordFile(GROUP_FILE_PATH,
                                  GroupName,
                                  NULL,
                                  NULL,
                                  UpdatePasswordDeleteLine);

    if (Status != 0) {
        SwPrintError(Status, GroupName, "Failed to delete group");
        TotalStatus = Status;
    }

    Status = 0;

MainEnd:
    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

