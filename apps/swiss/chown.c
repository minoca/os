/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    chown.c

Abstract:

    This module implements the chown utility for changing file user and group
    ownership.

Author:

    Evan Green 10-Mar-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CHOWN_VERSION_MAJOR 1
#define CHOWN_VERSION_MINOR 0

#define CHOWN_USAGE                                                            \
    "usage: chown [options] [owner][:[group]] files\n"                         \
    "       chown [options] --reference=file files\n"                          \
    "The chown utility changes file user and group ownership. Options are:\n"  \
    "  -c, --changes -- Print only when a change is made.\n"                   \
    "  -f, --silent, --quiet -- Suppress most error messages.\n"               \
    "  -v, --verbose -- Print something for every file processed.\n"           \
    "      --dereference -- Affect the destination of a symbolic link,\n"      \
    "      (default), rather than the link itself.\n"                          \
    "  -h, --no-dereference -- Affect a symbolic link rather than its "        \
    "target.\n"                                                                \
    "      --from=owner:group -- Change the owner and/or group only if \n"     \
    "      it matches the current given owner or group.\n"                     \
    "      --reference=file -- Use the given file's owner/group.\n"            \
    "  -R, --recursive -- Operate on directories recursively.\n"               \
    "  -H -- Traverse symbolic links to a directory on the command line.\n"    \
    "  -L -- Traverse all symbolic links to directories.\n"                    \
    "  -P -- Do not traverse any symbolic links (default).\n"                  \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define CHOWN_OPTIONS_STRING "cfvhRHLPV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ChownConvertUserGroupName (
    PSTR Argument,
    uid_t *UserId,
    gid_t *GroupId
    );

//
// -------------------------------------------------------------------- Globals
//

struct option ChownLongOptions[] = {
    {"changes", no_argument, 0, 'c'},
    {"silent", no_argument, 0, 'f'},
    {"quiet", no_argument, 0, 'f'},
    {"from", required_argument, 0, 'F'},
    {"dereference", no_argument, 0, 'D'},
    {"no-dereference", no_argument, 0, 'h'},
    {"reference", required_argument, 0, 'r'},
    {"recursive", no_argument, 0, 'R'},
    {"help", no_argument, 0, 'e'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
ChownMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the chown utility.

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
    CHOWN_CONTEXT Context;
    INT Option;
    struct stat Stat;
    int Status;
    int TotalStatus;

    memset(&Context, 0, sizeof(CHOWN_CONTEXT));
    Context.User = -1;
    Context.Group = -1;
    Context.FromUser = -1;
    Context.FromGroup = -1;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CHOWN_OPTIONS_STRING,
                             ChownLongOptions,
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
            Context.Options |= CHOWN_OPTION_PRINT_CHANGES;
            Context.Options &= ~(CHOWN_OPTION_QUIET | CHOWN_OPTION_VERBOSE);
            break;

        case 'f':
            Context.Options |= CHOWN_OPTION_QUIET;
            Context.Options &= ~(CHOWN_OPTION_PRINT_CHANGES |
                                 CHOWN_OPTION_VERBOSE);

            break;

        case 'v':
            Context.Options |= CHOWN_OPTION_VERBOSE;
            Context.Options &= ~(CHOWN_OPTION_QUIET |
                                 CHOWN_OPTION_PRINT_CHANGES);

            break;

        case 'F':
            Status = ChownConvertUserGroupName(optarg,
                                               &(Context.User),
                                               &(Context.Group));

            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'D':
            Context.Options &= ~CHOWN_OPTION_AFFECT_SYMBOLIC_LINKS;
            break;

        case 'h':
            Context.Options |= CHOWN_OPTION_AFFECT_SYMBOLIC_LINKS;
            break;

        case 'r':
            Status = stat(optarg, &Stat);
            if (Status != 0) {
                Status = errno;
                SwPrintError(Status, optarg, "Cannot stat reference file");
                goto MainEnd;
            }

            Context.User = Stat.st_uid;
            Context.Group = Stat.st_gid;
            break;

        case 'H':
            Context.Options |= CHOWN_OPTION_SYMBOLIC_DIRECTORY_ARGUMENTS;
            break;

        case 'L':
            Context.Options |= CHOWN_OPTION_SYMBOLIC_DIRECTORIES;
            break;

        case 'P':
            Context.Options &= ~(CHOWN_OPTION_SYMBOLIC_DIRECTORY_ARGUMENTS |
                                 CHOWN_OPTION_SYMBOLIC_DIRECTORIES);

            break;

        case 'R':
            Context.Options |= CHOWN_OPTION_RECURSIVE;
            break;

        case 'V':
            SwPrintVersion(CHOWN_VERSION_MAJOR, CHOWN_VERSION_MINOR);
            return 1;

        case 'e':
            printf(CHOWN_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0, NULL, "Argument expected");
        Status = EINVAL;
        goto MainEnd;
    }

    if ((Context.User == (uid_t)-1) && (Context.Group == (gid_t)-1)) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        Status = ChownConvertUserGroupName(Argument,
                                           &(Context.User),
                                           &(Context.Group));

        if (Status != 0) {
            goto MainEnd;
        }
    }

    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0, NULL, "Argument expected");
        Status = EINVAL;
        goto MainEnd;
    }

    //
    // Now that the options have been figured out, loop through again to
    // actually change ownership.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        Status = ChownChangeOwnership(&Context, Argument, 0);
        if (Status != 0) {
            TotalStatus = Status;
        }

        ArgumentIndex += 1;
    }

MainEnd:
    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ChownConvertUserGroupName (
    PSTR Argument,
    uid_t *UserId,
    gid_t *GroupId
    )

/*++

Routine Description:

    This routine converts a user:group or user.group string into a user ID and
        group ID.

Arguments:

    Argument - Supplies the user:group string, where either user and group are
        optional.

    UserId - Supplies a pointer where the user ID will be returned. This will
        be left alone if the user is not specified.

    GroupId - Supplies a pointer where the group ID will be returned. This
        will be left alone if the group is not specified.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    PSTR Copy;
    struct group *GroupInformation;
    PSTR GroupName;
    PSTR Separator;
    INT Status;
    struct passwd *UserInformation;
    PSTR UserName;

    if (*Argument == '\0') {
        return EINVAL;
    }

    GroupName = NULL;

    //
    // If the whole thing is a valid username, then that must be it. Check
    // this first to avoid the string duplication if not needed, and to
    // prioritize usernames with dots over the user.group separator.
    //

    UserInformation = getpwnam(Argument);
    if (UserInformation != NULL) {
        *UserId = UserInformation->pw_uid;
        return 0;
    }

    Copy = strdup(Argument);
    if (Copy == NULL) {
        return ENOMEM;
    }

    UserName = Copy;
    Separator = strchr(UserName, ':');
    if (Separator == NULL) {
        Separator = strrchr(UserName, '.');
    }

    if (Separator != NULL) {
        *Separator = '\0';
        GroupName = Separator + 1;
    }

    //
    // Convert the user name.
    //

    if ((UserName != NULL) && (*UserName != '\0')) {
        if (isdigit(*UserName) != 0) {
            *UserId = strtoul(UserName, &AfterScan, 10);
            if (AfterScan == UserName) {
                SwPrintError(0, UserName, "Invalid user ID");
                Status = EINVAL;
                goto ConvertUserGroupNameEnd;
            }

        } else {
            UserInformation = getpwnam(UserName);
            if (UserInformation == NULL) {
                SwPrintError(0, UserName, "User not found");
                Status = ENOENT;
                goto ConvertUserGroupNameEnd;
            }

            *UserId = UserInformation->pw_uid;
        }
    }

    if ((GroupName != NULL) && (*GroupName != '\0')) {
        if (isdigit(*GroupName) != 0) {
            *GroupId = strtoul(GroupName, &AfterScan, 10);
            if (AfterScan == GroupName) {
                SwPrintError(0, GroupName, "Invalid group ID");
                Status = EINVAL;
                goto ConvertUserGroupNameEnd;
            }

        } else {
            GroupInformation = getgrnam(GroupName);
            if (GroupInformation == NULL) {
                SwPrintError(0, GroupName, "Group not found");
                Status = ENOENT;
                goto ConvertUserGroupNameEnd;
            }

            *GroupId = GroupInformation->gr_gid;
        }
    }

    Status = 0;

ConvertUserGroupNameEnd:
    free(Copy);
    return Status;
}
