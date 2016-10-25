/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    userdel.c

Abstract:

    This module implements the userdel command, which deletes a user account
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

#define USERDEL_VERSION_MAJOR 1
#define USERDEL_VERSION_MINOR 0

#define USERDEL_USAGE                                                          \
    "usage: userdel [options] username\n"                                      \
    "The userdel utility deletes a user from the system. Options are:\n"       \
    "  -f, --force -- Force the removal of the account, even if the user \n"   \
    "      is still logged in, or another user uses the same home directory.\n"\
    "  -r, --remove -- Delete the home directory and its files.\n"             \
    "  -R, --root=dir -- Chroot into the given directory before operation.\n"  \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define USERDEL_OPTIONS_STRING "frR:HV"

//
// Define application options.
//

//
// Set this option to force the removal.
//

#define USERDEL_OPTION_FORCE 0x00000001

//
// Set this option to remove the home directory and mail spool.
//

#define USERDEL_OPTION_REMOVE 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option UserdelLongOptions[] = {
    {"force", no_argument, 0, 'f'},
    {"remove", no_argument, 0, 'r'},
    {"root", required_argument, 0, 'R'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
UserdelMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the userdel utility.

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
    int GroupCount;
    UINTN GroupIndex;
    gid_t *Groups;
    PSTR Home;
    gid_t *NewGroups;
    INT Option;
    ULONG Options;
    PSTR RootDirectory;
    struct spwd Shadow;
    int Status;
    int TotalStatus;
    struct passwd *User;
    struct passwd UserCopy;
    PSTR UserName;

    memset(&Shadow, 0, sizeof(Shadow));
    Groups = NULL;
    GroupCount = 0;
    Home = NULL;
    Options = 0;
    RootDirectory = NULL;
    TotalStatus = 0;
    UserName = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             USERDEL_OPTIONS_STRING,
                             UserdelLongOptions,
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
            Options |= USERDEL_OPTION_FORCE;
            break;

        case 'r':
            Options |= USERDEL_OPTION_REMOVE;
            break;

        case 'R':
            RootDirectory = optarg;
            break;

        case 'V':
            SwPrintVersion(USERDEL_VERSION_MAJOR, USERDEL_VERSION_MINOR);
            return 1;

        case 'H':
            printf(USERDEL_USAGE);
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

    while (ArgumentIndex < ArgumentCount) {
        UserName = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        User = getpwnam(UserName);
        if (User == NULL) {
            SwPrintError(0, UserName, "No such user");
            TotalStatus = ENOENT;
            continue;
        }

        memcpy(&UserCopy, User, sizeof(struct passwd));
        UserCopy.pw_name = UserName;
        Home = strdup(User->pw_dir);

        //
        // Get all the groups this user belongs to.
        //

        Status = getgrouplist(UserName, UserCopy.pw_gid, Groups, &GroupCount);
        if (Status < 0) {

            //
            // Allocate or reallocate the buffer. Add some extras in case
            // things shift a bit in the meantime.
            //

            NewGroups = realloc(Groups, (GroupCount + 5) * sizeof(gid_t));
            if (NewGroups == NULL) {
                Status = ENOMEM;
                goto MainEnd;
            }

            Groups = NewGroups;
            Status = getgrouplist(UserName,
                                  UserCopy.pw_gid,
                                  Groups,
                                  &GroupCount);

            if (Status < 0) {
                SwPrintError(0, UserName, "Failed to get groups");
                Status = errno;
                goto MainEnd;
            }
        }

        if (Status > 0) {

            //
            // Remove the user from all supplementary groups.
            //

            for (GroupIndex = 0; GroupIndex < GroupCount; GroupIndex += 1) {
                Group = getgrgid(Groups[GroupIndex]);
                if ((Group != NULL) && (Group->gr_gid != UserCopy.pw_gid)) {
                    Status = SwUpdatePasswordFile(
                                              GROUP_FILE_PATH,
                                              Group->gr_name,
                                              NULL,
                                              UserName,
                                              UpdatePasswordDeleteGroupMember);

                    if (Status != 0) {
                        SwPrintError(Status,
                                     NULL,
                                     "Failed to remove user %s from group %s",
                                     UserName,
                                     Group->gr_name);

                        TotalStatus = Status;
                    }
                }
            }
        }

        //
        // If there is a group with the same name as the user, delete that
        // group.
        //

        Group = getgrnam(UserName);
        if (Group != NULL) {
            Status = SwUpdatePasswordFile(GROUP_FILE_PATH,
                                          UserName,
                                          NULL,
                                          NULL,
                                          UpdatePasswordDeleteLine);

            if (Status != 0) {
                TotalStatus = Status;
            }
        }

        //
        // Remove the user itself.
        //

        UserCopy.pw_name = UserName;
        Shadow.sp_namp = UserName;
        Status = SwUpdatePasswordLine(&UserCopy,
                                      &Shadow,
                                      UpdatePasswordDeleteLine);

        if (Status != 0) {
            SwPrintError(Status, UserName, "Failed to remove user");
            goto MainEnd;
        }

        //
        // Remove the home directory.
        //

        if (((Options & USERDEL_OPTION_REMOVE) != 0) && (Home != NULL)) {
            Status = SwDelete(DELETE_OPTION_RECURSIVE | DELETE_OPTION_FORCE,
                              Home);

            if (Status != 0) {
                SwPrintError(Status, Home, "Failed to delete home directory");
                TotalStatus = Status;
            }
        }

        if (Home != NULL) {
            free(Home);
            Home = NULL;
        }
    }

    Status = 0;

MainEnd:
    if (Home != NULL) {
        free(Home);
    }

    if (Groups != NULL) {
        free(Groups);
    }

    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

