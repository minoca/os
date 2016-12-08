/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    useradd.c

Abstract:

    This module implements support for the useradd utility, which adds a new
    user to the system.

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
#include <dirent.h>
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

#define USERADD_VERSION_MAJOR 1
#define USERADD_VERSION_MINOR 0

#define USERADD_USAGE                                                          \
    "usage: useradd [options] username\n"                                      \
    "The useradd utility adds a new user to the system. Options are:\n"        \
    "  -b, --base-dir=dir -- Sets the base directory for the home directory \n"\
    "      of the new account (eg. /home).\n"                                  \
    "  -c, --comment=gecos -- Sets the GECOS field.\n"                         \
    "  -d, --home=dir -- Sets the home directory.\n"                           \
    "  -g, --gid=group -- Sets the name or ID of the primary group for the\n"  \
    "      new account.\n"                                                     \
    "  -G, --groups=group,group -- Sets the supplementary groups.\n"           \
    "  -k, --skel=dir -- Sets the alternate skeleton directory location.\n"    \
    "  -m, --create-home -- Creates the home directory if it does not exist.\n"\
    "  -M, --no-create-home -- Do not create the home directory.\n"            \
    "  -N, --no-user-group -- Do not create a group with the same name as \n"  \
    "      the user.\n"                                                        \
    "  -o, --non-unique -- Allow users with duplicate IDs.\n"                  \
    "  -p, --password=pw -- Sets the user's password hash value directly.\n"   \
    "  -R, --root=dir -- Chroot into the given directory before operating.\n"  \
    "  -r, --system -- Sets this as a system account.\n"                       \
    "  -s, --shell=shell -- Sets the user's shell.\n"                          \
    "  -u, --uid=id -- Sets the user ID of the new user.\n"                    \
    "  -U, --user-group -- Create a group with the same name as the user.\n"   \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define USERADD_OPTIONS_STRING "b:c:d:g:G:k:mMNop:R:rs:u:UHV"

#define USERADD_DEFAULT_GECOS ""
#define USERADD_DEFAULT_SKELETON "/etc/skel"
#define USERADD_DEFAULT_SHELL "/bin/sh"
#define USERADD_DEFAULT_PASSWORD "x"
#define USERADD_DEFAULT_GROUP "nogroup"
#define USERADD_DEFAULT_BASE_DIRECTORY "/home"
#define USERADD_HOME_PERMISSIONS \
    (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH | S_ISGID)

#define USERADD_MAX_ID 0x7FFFFFFE

//
// Define application options.
//

//
// Set this option to create the home directory if it does not exist.
//

#define USERADD_OPTION_CREATE_HOME 0x00000001

//
// Set this option to create a group with the same name as the user.
//

#define USERADD_OPTION_CREATE_GROUP 0x00000002

//
// Set this option to create a system account.
//

#define USERADD_OPTION_SYSTEM 0x00000004

//
// Set this option to allow non-unique user IDs.
//

#define USERADD_OPTION_NON_UNIQUE 0x00000008

//
// Define the default algorithm to use on new accounts: SHA-512.
//

#define USERADD_PASSWORD_ALGORITHM "$6$"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
UseraddIsValidUserName (
    PSTR Name
    );

INT
UseraddCreateSelfieGroup (
    PSTR Name,
    gid_t Id
    );

INT
UseraddAddUserToSupplementaryGroups (
    PSTR User,
    PSTR SupplementaryGroups
    );

//
// -------------------------------------------------------------------- Globals
//

struct option UseraddLongOptions[] = {
    {"base-dir", required_argument, 0, 'b'},
    {"comment", required_argument, 0, 'c'},
    {"home", required_argument, 0, 'd'},
    {"gid", required_argument, 0, 'g'},
    {"groups", required_argument, 0, 'G'},
    {"skel", required_argument, 0, 'k'},
    {"create-home", no_argument, 0, 'm'},
    {"no-create-home", no_argument, 0, 'M'},
    {"no-user-group", no_argument, 0, 'N'},
    {"non-unique", no_argument, 0, 'o'},
    {"password", required_argument, 0, 'p'},
    {"root", required_argument, 0, 'R'},
    {"system", no_argument, 0, 'r'},
    {"shell", required_argument, 0, 's'},
    {"uid", required_argument, 0, 'u'},
    {"user-group", no_argument, 0, 'U'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
UseraddMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the useradd utility.

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
    PSTR BaseDirectory;
    CHOWN_CONTEXT ChownContext;
    BOOL CreateGroupSpecified;
    uid_t ExistingId;
    PSTR GroupsString;
    PSTR GroupString;
    PSTR Home;
    ULONG HomeSize;
    INT Option;
    ULONG Options;
    mode_t OriginalUmask;
    PSTR Password;
    BOOL Result;
    PSTR RootDirectory;
    struct spwd Shadow;
    PSTR Skeleton;
    struct stat Stat;
    int Status;
    struct passwd User;
    PSTR UserName;

    memset(&Shadow, 0, sizeof(Shadow));
    memset(&User, 0, sizeof(User));
    User.pw_uid = -1;
    User.pw_gid = -1;
    BaseDirectory = USERADD_DEFAULT_BASE_DIRECTORY;
    CreateGroupSpecified = FALSE;
    GroupString = NULL;
    GroupsString = NULL;
    Home = NULL;
    Options = USERADD_OPTION_CREATE_GROUP | USERADD_OPTION_CREATE_HOME;
    Password = NULL;
    RootDirectory = NULL;
    Skeleton = USERADD_DEFAULT_SKELETON;
    UserName = NULL;
    User.pw_gecos = USERADD_DEFAULT_GECOS;
    User.pw_passwd = USERADD_DEFAULT_PASSWORD;
    User.pw_shell = USERADD_DEFAULT_SHELL;
    memcpy(&Shadow, &SwShadowTemplate, sizeof(struct spwd));
    Shadow.sp_lstchg = time(NULL) / (3600 * 24);
    OriginalUmask = umask(0);

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             USERADD_OPTIONS_STRING,
                             UseraddLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'b':
            BaseDirectory = optarg;
            break;

        case 'c':
            User.pw_gecos = optarg;
            break;

        case 'd':
            User.pw_dir = optarg;
            break;

        case 'g':
            GroupString = optarg;
            Options &= ~USERADD_OPTION_CREATE_GROUP;
            break;

        case 'G':
            GroupsString = optarg;
            break;

        case 'k':
            Skeleton = optarg;
            break;

        case 'm':
            Options |= USERADD_OPTION_CREATE_HOME;
            break;

        case 'M':
            Options &= ~USERADD_OPTION_CREATE_HOME;
            break;

        case 'N':
            Options &= ~USERADD_OPTION_CREATE_GROUP;
            break;

        case 'o':
            Options |= USERADD_OPTION_NON_UNIQUE;
            break;

        case 'p':
            Password = optarg;
            break;

        case 'R':
            RootDirectory = optarg;
            break;

        case 'r':
            Options |= USERADD_OPTION_SYSTEM;
            break;

        case 's':
            User.pw_shell = optarg;
            break;

        case 'u':
            User.pw_uid = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid user ID");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'U':
            Options |= USERADD_OPTION_CREATE_GROUP;
            CreateGroupSpecified = TRUE;
            break;

        case 'V':
            SwPrintVersion(USERADD_VERSION_MAJOR, USERADD_VERSION_MINOR);
            return 1;

        case 'H':
            printf(USERADD_USAGE);
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

    UserName = Arguments[ArgumentIndex];
    ArgumentIndex += 1;
    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, NULL, "Unexpected additional arguments");
        return 1;
    }

    //
    // Fail on conflicting options.
    //

    if ((CreateGroupSpecified != FALSE) && (GroupString != NULL)) {
        SwPrintError(0, NULL, "-g and -U conflict");
        Status = EINVAL;
        goto MainEnd;
    }

    //
    // Enforce a valid user name.
    //

    if (SwIsValidUserName(UserName) == FALSE) {
        SwPrintError(0, UserName, "Invalid username");
        Status = 1;
        goto MainEnd;
    }

    if (Password != NULL) {
        Shadow.sp_pwdp = Password;
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

    User.pw_name = UserName;
    Shadow.sp_namp = UserName;

    //
    // Create the directory from the base if needed.
    //

    if (User.pw_dir == NULL) {
        Result = SwAppendPath(BaseDirectory,
                              strlen(BaseDirectory) + 1,
                              UserName,
                              strlen(UserName) + 1,
                              &Home,
                              &HomeSize);

        if (Result == FALSE) {
            Status = ENOMEM;
            goto MainEnd;
        }

        User.pw_dir = Home;
    }

    //
    // Parse the primary and supplementary groups.
    //

    if ((Options & USERADD_OPTION_CREATE_GROUP) == 0) {
        if (GroupString == NULL) {
            GroupString = USERADD_DEFAULT_GROUP;
        }
    }

    if (GroupString != NULL) {
        Status = SwGetGroupIdFromName(GroupString, &(User.pw_gid));
        if (Status != 0) {
            SwPrintError(0, GroupString, "Invalid group");
            goto MainEnd;
        }
    }

    //
    // Ensure there are no duplicates in the user name.
    //

    if (SwGetUserIdFromName(UserName, &ExistingId) == 0) {
        SwPrintError(0,
                     NULL,
                     "User %s already exists (ID %d)",
                     UserName,
                     ExistingId);

        Status = 1;
        goto MainEnd;
    }

    //
    // Find a user ID.
    //

    if (User.pw_uid == (uid_t)-1) {
        if ((Options & USERADD_OPTION_SYSTEM) != 0) {
            User.pw_uid = BASE_SYSTEM_UID;

        } else {
            User.pw_uid = BASE_NON_SYSTEM_UID;
        }

        while (User.pw_uid < USERADD_MAX_ID) {
            if ((getpwuid(User.pw_uid) == NULL) &&
                (((Options & USERADD_OPTION_CREATE_GROUP) == 0) ||
                 (getgrgid(User.pw_uid) == NULL))) {

                 break;
            }

            User.pw_uid += 1;
        }

        if (User.pw_uid >= USERADD_MAX_ID) {
            SwPrintError(0, NULL, "User IDs exhausted");
            Status = 1;
            goto MainEnd;
        }

        if (User.pw_gid == (gid_t)-1) {
            User.pw_gid = User.pw_uid;
            if (getgrnam(UserName) != NULL) {
                SwPrintError(0, UserName, "Group already exists");
                Status = 1;
                goto MainEnd;
            }
        }

    //
    // The user wanted a specific ID.
    //

    } else {
        if ((Options & USERADD_OPTION_NON_UNIQUE) == 0) {
            if (getpwuid(User.pw_uid) != NULL) {
                SwPrintError(0, NULL, "User ID %d in use", User.pw_uid);
                Status = 1;
                goto MainEnd;
            }
        }
    }

    //
    // Create a group specifically for the user.
    //

    if ((Options & USERADD_OPTION_CREATE_GROUP) != 0) {
        User.pw_gid = User.pw_uid;
        Status = UseraddCreateSelfieGroup(UserName, User.pw_uid);
        if (Status != 0) {
            SwPrintError(Status, UserName, "Unable to create group");
            goto MainEnd;
        }
    }

    //
    // Add the passwd and shadow entries.
    //

    Status = SwUpdatePasswordLine(&User, &Shadow, UpdatePasswordAddLine);
    if (Status != 0) {
        SwPrintError(Status, UserName, "Failed to add user");
        goto MainEnd;
    }

    //
    // Add the user to all the supplementary groups.
    //

    if (GroupsString != NULL) {
        Result = UseraddAddUserToSupplementaryGroups(UserName, GroupsString);
        if (Result != 0) {
            goto MainEnd;
        }
    }

    //
    // Create the home directory.
    //

    if ((Options & USERADD_OPTION_CREATE_HOME) != 0) {
        Status = mkdir(User.pw_dir,
                       USERADD_HOME_PERMISSIONS & (~S_ISGID));

        if ((Status != 0) && (errno != EEXIST)) {
            Status = errno;
            SwPrintError(Status,
                         User.pw_dir,
                         "Failed to create home directory");

            goto MainEnd;
        }

        //
        // If the directory was created, copy the skeleton contents over.
        //

        if ((Status == 0) && (stat(Skeleton, &Stat) == 0)) {
            SwCopy(COPY_OPTION_RECURSIVE, Skeleton, User.pw_dir);
        }

        //
        // Change the new user to be the owner of everything in there.
        //

        memset(&ChownContext, 0, sizeof(ChownContext));
        ChownContext.Options = CHOWN_OPTION_RECURSIVE;
        ChownContext.User = User.pw_uid;
        ChownContext.Group = User.pw_gid;
        ChownContext.FromUser = -1;
        ChownContext.FromGroup = -1;
        Status = ChownChangeOwnership(&ChownContext, User.pw_dir, 0);
        if (Status != 0) {
            SwPrintError(Status, User.pw_dir, "Failed to change ownership");
            Status = 0;
        }

        Status = chmod(User.pw_dir, USERADD_HOME_PERMISSIONS);
        if (Status != 0) {
            SwPrintError(errno, User.pw_dir, "Failed to change mode");
            Status = 0;
        }
    }

    Status = 0;

MainEnd:
    umask(OriginalUmask);
    if (Home != NULL) {
        free(Home);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
UseraddCreateSelfieGroup (
    PSTR Name,
    gid_t Id
    )

/*++

Routine Description:

    This routine creates a group with the same name as the given user and with
    one member: the user.

Arguments:

    Name - Supplies a pointer to the user/group name.

    Id - Supplies the ID of the user/group.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    struct group Group;

    memset(&Group, 0, sizeof(Group));
    Group.gr_name = Name;
    Group.gr_passwd = USERADD_DEFAULT_PASSWORD;
    Group.gr_gid = Id;
    return SwUpdateGroupLine(&Group, UpdatePasswordAddLine);
}

INT
UseraddAddUserToSupplementaryGroups (
    PSTR User,
    PSTR SupplementaryGroups
    )

/*++

Routine Description:

    This routine adds the user to all the supplementary groups.

Arguments:

    User - Supplies the user name to add.

    SupplementaryGroups - Supplies the supplementary groups to add the user to.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Copy;
    PSTR Current;
    PSTR NextComma;
    INT Result;
    INT TotalResult;

    Copy = strdup(SupplementaryGroups);
    if (Copy == NULL) {
        return ENOMEM;
    }

    TotalResult = 0;
    Current = Copy;
    while (Current != NULL) {
        NextComma = strchr(Current, ',');
        if (NextComma != NULL) {
            *NextComma = '\0';
            NextComma += 1;
        }

        if (*Current != '\0') {
            Result = SwUpdatePasswordFile(GROUP_FILE_PATH,
                                          Current,
                                          NULL,
                                          User,
                                          UpdatePasswordAddGroupMember);

            if (Result != 0) {
                SwPrintError(Result, Current, "Failed to add user to group");
                TotalResult = Result;
            }
        }

        Current = NextComma;
    }

    free(Copy);
    return TotalResult;
}

