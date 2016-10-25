/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    chroot.c

Abstract:

    This module implements the chroot utility, which runs a command line
    jailed to a specific region of the file system.

Author:

    Evan Green 22-Oct-2014

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

#define CHROOT_VERSION_MAJOR 1
#define CHROOT_VERSION_MINOR 0

#define CHROOT_USAGE                                                           \
    "usage: chroot [options] new_root [command [arguments]]\n"                 \
    "       chroot options\n"                                                  \
    "The chroot utility runs the given command or interactive shell jailed \n" \
    "to a specific region of the file system. If no command is given, \n"      \
    "${SHELL} -i is run (with a default of /bin/sh). Options are\n"            \
    "  -e, --escape -- Attempt to escape the current root.\n"                  \
    "  -u, --userspec=user:group -- Specifies the user and group (ID or \n"    \
    "       name) to change to before executing the command.\n"                \
    "  -G, --groups=groups -- Specifies the supplementary groups the \n"       \
    "      process will become a member of before executing the command.\n"    \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define CHROOT_OPTIONS_STRING "eu:G:h"

#define CHROOT_SHELL_VARIABLE "SHELL"
#define CHROOT_DEFAULT_SHELL "/bin/sh"

//
// Set this option to attempt to escape the current chroot.
//

#define CHROOT_OPTION_ESCAPE 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option ChrootLongOptions[] = {
    {"escape", no_argument, 0, 'e'},
    {"userspec", required_argument, 0, 'u'},
    {"groups", required_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
ChrootMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the chroot utility, which changes
    the root directory and runs a command.

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
    id_t Group;
    size_t GroupCount;
    gid_t *Groups;
    gid_t NewGroup;
    uid_t NewUser;
    INT Option;
    ULONG Options;
    PSTR ShellArguments[3];
    int Status;
    uid_t User;

    Groups = NULL;
    GroupCount = 0;
    Options = 0;
    User = SwGetRealUserId();
    Group = SwGetRealGroupId();
    NewUser = User;
    NewGroup = Group;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CHROOT_OPTIONS_STRING,
                             ChrootLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'e':
            Options |= CHROOT_OPTION_ESCAPE;
            break;

        case 'u':
            Status = SwParseUserAndGroupString(optarg, &NewUser, &NewGroup);
            if (Status != 0) {
                if (Status == EINVAL) {
                    SwPrintError(0, optarg, "Invalid user/group string");

                } else {
                    SwPrintError(Status, optarg, "Invalid user/group string");
                }

                goto MainEnd;
            }

            if (NewUser == (uid_t)-1) {
                NewUser = User;
            }

            if (NewGroup == (gid_t)-1) {
                NewGroup = Group;
            }

            break;

        case 'g':
            Status = SwParseGroupList(optarg, &Groups, &GroupCount);
            if (Status != 0) {
                SwPrintError(Status, optarg, "Invalid group list");
                goto MainEnd;
            }

            break;

        case 'V':
            SwPrintVersion(CHROOT_VERSION_MAJOR, CHROOT_VERSION_MINOR);
            return 1;

        case 'h':
            printf(CHROOT_USAGE);
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

    //
    // If the user wants to escape, try to leave the root by passing in NULL.
    //

    if ((Options & CHROOT_OPTION_ESCAPE) != 0) {
        Status = SwChroot(NULL);
        if (Status != 0) {
            SwPrintError(Status, NULL, "Failed to change root");
            goto MainEnd;
        }

    //
    // Change the root. Change the current directory too.
    //

    } else {
        if (ArgumentIndex == ArgumentCount) {
            SwPrintError(0, NULL, "New root directory expected");
            Status = EINVAL;
            goto MainEnd;
        }

        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        Status = chdir(Argument);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, Argument, "Failed to change directory");
            goto MainEnd;
        }

        Status = SwChroot(".");
        if (Status != 0) {
            SwPrintError(Status, Argument, "Failed to change root");
            goto MainEnd;
        }
    }

    //
    // Change the user, group, and groups if needed.
    //

    if (Groups != NULL) {
        Status = SwSetGroups(GroupCount, Groups);
        if (Status != 0) {
            SwPrintError(Status, NULL, "Failed to set supplementary groups");
            goto MainEnd;
        }
    }

    if (NewGroup != Group) {
        Status = SwSetRealGroupId(NewGroup);
        if (Status != 0) {
            SwPrintError(Status, NULL, "Failed to set group ID");
            goto MainEnd;
        }
    }

    if (NewUser != User) {
        Status = SwSetRealUserId(NewUser);
        if (Status != 0) {
            SwPrintError(Status, NULL, "Failed to set user ID");
            goto MainEnd;
        }
    }

    if (ArgumentIndex == ArgumentCount) {
        ShellArguments[0] = getenv("SHELL");
        if (ShellArguments[0] == NULL) {
            ShellArguments[0] = CHROOT_DEFAULT_SHELL;
        }

        ShellArguments[1] = "-i";
        ShellArguments[2] = NULL;
        Argument = ShellArguments[0];
        Status = SwExec(ShellArguments[0], ShellArguments, 2);

    } else {
        Argument = Arguments[ArgumentIndex];
        Status = SwExec(Arguments[ArgumentIndex],
                        &(Arguments[ArgumentIndex]),
                        ArgumentCount - ArgumentIndex);
    }

    SwPrintError(Status, Argument, "Failed to execute");

MainEnd:
    if (Groups != NULL) {
        free(Groups);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

