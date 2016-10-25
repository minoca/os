/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    groupadd.c

Abstract:

    This module implements support for the groupadd utility, which adds a new
    group to the system.

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

#define GROUPADD_VERSION_MAJOR 1
#define GROUPADD_VERSION_MINOR 0

#define GROUPADD_USAGE                                                         \
    "usage: groupadd [options] groupname\n"                                    \
    "The groupadd utility adds a new group to the system. Options are:\n"      \
    "  -f, --force -- Exit successfully if the group already exists, and \n"   \
    "      cancel -g if the given GID is already in use.\n"                    \
    "  -g, --gid=gid -- Use the given group ID number for the new group.\n"    \
    "  -o, --non-unique -- Succeed even if a group with the same ID exists.\n" \
    "  -p, --password - Sets the password for the group.\n"                    \
    "  -r, --system -- Sets this as a system group.\n"                         \
    "  -R, --root=dir -- Chroot into the given directory before operating.\n"  \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define GROUPADD_OPTIONS_STRING "fg:op:rR:HV"

#define GROUPADD_DEFAULT_PASSWORD "x"
#define GROUPADD_MAX_ID 0x7FFFFFFE

//
// Define application options.
//

//
// Set this option to succeed if the group already exists, and cancel -g if
// the GID is in use.
//

#define GROUPADD_OPTION_FORCE 0x00000001

//
// Set this option to create a system account.
//

#define GROUPADD_OPTION_SYSTEM 0x00000002

//
// Set this option to allow non-unique user IDs.
//

#define GROUPADD_OPTION_NON_UNIQUE 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option GroupaddLongOptions[] = {
    {"force", no_argument, 0, 'f'},
    {"gid", required_argument, 0, 'g'},
    {"non-unique", no_argument, 0, 'o'},
    {"password", required_argument, 0, 'p'},
    {"root", required_argument, 0, 'R'},
    {"system", no_argument, 0, 'r'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
GroupaddMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the groupadd utility.

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
    struct group Group;
    PSTR GroupName;
    INT Option;
    ULONG Options;
    PSTR Password;
    PSTR RootDirectory;
    int Status;

    memset(&Group, 0, sizeof(Group));
    Group.gr_gid = -1;
    Group.gr_passwd = GROUPADD_DEFAULT_PASSWORD;
    GroupName = NULL;
    Options = 0;
    Password = NULL;
    RootDirectory = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             GROUPADD_OPTIONS_STRING,
                             GroupaddLongOptions,
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
            Options |= GROUPADD_OPTION_FORCE;
            break;

        case 'g':
            Group.gr_gid = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid group ID");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'o':
            Options |= GROUPADD_OPTION_NON_UNIQUE;
            break;

        case 'p':
            Password = optarg;
            break;

        case 'R':
            RootDirectory = optarg;
            break;

        case 'r':
            Options |= GROUPADD_OPTION_SYSTEM;
            break;

        case 'V':
            SwPrintVersion(GROUPADD_VERSION_MAJOR, GROUPADD_VERSION_MINOR);
            return 1;

        case 'H':
            printf(GROUPADD_USAGE);
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

    GroupName = Arguments[ArgumentIndex];
    ArgumentIndex += 1;
    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, NULL, "Unexpected additional arguments");
        return 1;
    }

    //
    // Enforce a valid user name.
    //

    if (SwIsValidUserName(GroupName) == FALSE) {
        SwPrintError(0, GroupName, "Invalid group name");
        Status = 1;
        goto MainEnd;
    }

    //
    // If the hoards are clamoring for it, implement passwords on groups.
    //

    if (Password != NULL) {
        SwPrintError(0,
                     NULL,
                     "Group passwords currently not implemented. Let us know "
                     "that you want it.");

        Status = 1;
        goto MainEnd;
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

    Group.gr_name = GroupName;

    //
    // Ensure there are no duplicates in the group name.
    //

    if (getgrnam(GroupName) != NULL) {
        if ((Options & GROUPADD_OPTION_FORCE) == 0) {
            SwPrintError(0, GroupName, "Group already exists");
            Status = 1;
            goto MainEnd;

        } else {
            Status = 0;
            goto MainEnd;
        }
    }

    if (Group.gr_gid != (gid_t)-1) {
        if ((Options & GROUPADD_OPTION_NON_UNIQUE) == 0) {
            if (getgrgid(Group.gr_gid) != NULL) {

                //
                // If force is enabled and the GID already exists, just cancel
                // it.
                //

                if ((Options & GROUPADD_OPTION_FORCE) != 0) {
                    Group.gr_gid = -1;

                } else {
                    SwPrintError(0, NULL, "Group ID %d in use", Group.gr_gid);
                    Status = 1;
                    goto MainEnd;
                }
            }
        }
    }

    //
    // Find a group ID.
    //

    if (Group.gr_gid == -1) {
        if ((Options & GROUPADD_OPTION_SYSTEM) != 0) {
            Group.gr_gid = BASE_SYSTEM_GID;

        } else {
            Group.gr_gid = BASE_NON_SYSTEM_GID;
        }

        while (Group.gr_gid < GROUPADD_MAX_ID) {
            if (getgrgid(Group.gr_gid) == NULL) {
                break;
            }

            Group.gr_gid += 1;
        }

        if (Group.gr_gid >= GROUPADD_MAX_ID) {
            SwPrintError(0, NULL, "Group IDs exhausted");
            Status = 1;
            goto MainEnd;
        }
    }

    //
    // Create the group.
    //

    Status = SwUpdateGroupLine(&Group, UpdatePasswordAddLine);
    if (Status != 0) {
        SwPrintError(Status, GroupName, "Failed to add group");
        goto MainEnd;
    }

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

