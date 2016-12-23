/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    id.c

Abstract:

    This module implements the id utility, which prints out the user and group
    identifiers for the calling process.

Author:

    Evan Green 6-Oct-2014

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

#define ID_VERSION_MAJOR 1
#define ID_VERSION_MINOR 0

#define ID_USAGE                                                               \
    "usage: id [user]\n"                                                       \
    "       id -G[-n] [user]\n"                                                \
    "       id -g[-nr] [user]\n"                                               \
    "       id -u[-nr] [user]\n"                                               \
    "The id utility prints the user and group IDs and names of the invoking \n"\
    "process. If the effective and real IDs do not match, both will be \n"     \
    "printed. If a user parameter is specified, then that user's data \n"      \
    "will be printed, assuming the effective and real IDs match."              \
    "Options are:\n"                                                           \
    "  -G, --groups -- Output all different group IDs (effective, real, and \n"\
    "      supplementary) only.\n"                                             \
    "  -g, --group -- Output only the effective group ID.\n"                   \
    "  -n, --name -- Output the name instead of a number.\n"                   \
    "  -r, --real -- Output the real ID instead of the effective ID.\n"        \
    "  -u, --user -- Output only the effective user ID.\n"                     \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define ID_OPTIONS_STRING "Ggnruh"

//
// Define ID options.
//

//
// Set this option to print only all different group IDs.
//

#define ID_OPTION_ONLY_GROUPS 0x00000001

//
// Set this option to print only the effective group ID.
//

#define ID_OPTION_ONLY_GROUP 0x00000002

//
// Set this option to print only the effective user ID.
//

#define ID_OPTION_ONLY_USER 0x00000004

//
// Set this option to print names instead of numbers.
//

#define ID_OPTION_PRINT_NAMES 0x00000008

//
// Set this option to use the real instead of the effective ID.
//

#define ID_OPTION_REAL_ID 0x00000010

//
// Define the options that are mutually exclusive.
//

#define ID_OPTION_EXCLUSIVE_MASK \
    (ID_OPTION_ONLY_GROUPS | ID_OPTION_ONLY_GROUP | ID_OPTION_ONLY_USER)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
IdPrintGroups (
    uid_t UserId,
    gid_t GroupId,
    INT Options
    );

VOID
IdPrintUserId (
    uid_t UserId,
    INT Options
    );

VOID
IdPrintGroupId (
    gid_t GroupId,
    INT Options
    );

//
// -------------------------------------------------------------------- Globals
//

struct option IdLongOptions[] = {
    {"groups", no_argument, 0, 'G'},
    {"group", no_argument, 0, 'g'},
    {"name", no_argument, 0, 'n'},
    {"real", no_argument, 0, 'r'},
    {"user", no_argument, 0, 'u'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
IdMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the id utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    gid_t EffectiveGroupId;
    uid_t EffectiveUserId;
    INT Option;
    ULONG Options;
    gid_t RealGroupId;
    uid_t RealUserId;
    int Status;
    PSWISS_USER_INFORMATION UserInformation;
    PSTR UserNameArgument;

    Options = 0;
    UserInformation = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             ID_OPTIONS_STRING,
                             IdLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'G':
            if ((Options & ID_OPTION_EXCLUSIVE_MASK) != 0) {
                SwPrintError(0,
                             NULL,
                             "Multiple mutually exclusive options supplied");

                Status = EINVAL;
                goto MainEnd;
            }

            Options |= ID_OPTION_ONLY_GROUPS;
            break;

        case 'g':
            if ((Options & ID_OPTION_EXCLUSIVE_MASK) != 0) {
                SwPrintError(0,
                             NULL,
                             "Multiple mutually exclusive options supplied");

                Status = EINVAL;
                goto MainEnd;
            }

            Options |= ID_OPTION_ONLY_GROUP;
            break;

        case 'n':
            Options |= ID_OPTION_PRINT_NAMES;
            break;

        case 'r':
            Options |= ID_OPTION_REAL_ID;
            break;

        case 'u':
            if ((Options & ID_OPTION_EXCLUSIVE_MASK) != 0) {
                SwPrintError(0,
                             NULL,
                             "Multiple mutually exclusive options supplied");

                Status = EINVAL;
                goto MainEnd;
            }

            Options |= ID_OPTION_ONLY_USER;
            break;

        case 'V':
            SwPrintVersion(ID_VERSION_MAJOR, ID_VERSION_MINOR);
            return 1;

        case 'h':
            printf(ID_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // The modifiers are only valid if one of the "only" options was specified.
    //

    if (((Options & (ID_OPTION_PRINT_NAMES | ID_OPTION_REAL_ID)) != 0) &&
        ((Options & ID_OPTION_EXCLUSIVE_MASK) == 0)) {

        SwPrintError(0,
                     NULL,
                     "Cannot print names or real IDs in the default format");

        Status = 1;
        goto MainEnd;
    }

    Status = 0;
    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    UserNameArgument = NULL;
    if (ArgumentIndex < ArgumentCount) {
        UserNameArgument = Arguments[ArgumentIndex];
        if (ArgumentIndex + 1 != ArgumentCount) {
            SwPrintError(0, NULL, "Only one argument expected");
            Status = EINVAL;
            goto MainEnd;
        }
    }

    if (UserNameArgument == NULL) {
        RealUserId = SwGetRealUserId();
        RealGroupId = SwGetRealGroupId();
        EffectiveUserId = SwGetEffectiveUserId();
        EffectiveGroupId = SwGetEffectiveGroupId();

    } else {
        Status = SwGetUserInformationByName(UserNameArgument, &UserInformation);
        if (Status != 0) {
            SwPrintError(Status,
                         UserNameArgument,
                         "Failed to get information for user");

            goto MainEnd;
        }

        RealUserId = UserInformation->UserId;
        RealGroupId = UserInformation->GroupId;
        EffectiveUserId = RealUserId;
        EffectiveGroupId = RealGroupId;
        free(UserInformation);
        UserInformation = NULL;
    }

    if (((Options & ID_OPTION_EXCLUSIVE_MASK) != 0) &&
        ((Options & ID_OPTION_REAL_ID) != 0)) {

        EffectiveUserId = RealUserId;
        EffectiveGroupId = RealGroupId;
    }

    if ((Options & ID_OPTION_ONLY_USER) != 0) {
        IdPrintUserId(EffectiveUserId, Options);

    } else if ((Options & ID_OPTION_ONLY_GROUP) != 0) {
        IdPrintGroupId(EffectiveGroupId, Options);

    } else if ((Options & ID_OPTION_ONLY_GROUPS) != 0) {
        Status = IdPrintGroups(EffectiveUserId, EffectiveGroupId, Options);

    //
    // Print the fancy default format.
    //

    } else {
        printf("uid=");
        IdPrintUserId(RealUserId, Options);
        printf(" gid=");
        IdPrintGroupId(RealGroupId, Options);
        if (RealUserId != EffectiveUserId) {
            printf(" euid=");
            IdPrintUserId(EffectiveUserId, Options);
        }

        if (RealGroupId != EffectiveGroupId) {
            printf(" egid=");
            IdPrintGroupId(EffectiveGroupId, Options);
        }

        printf(" groups=");
        Status = IdPrintGroups(EffectiveUserId, EffectiveGroupId, Options);
    }

    printf("\n");

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
IdPrintGroups (
    uid_t UserId,
    gid_t GroupId,
    INT Options
    )

/*++

Routine Description:

    This routine prints all the groups a user is a member of.

Arguments:

    UserId - Supplies the ID of the user to print.

    GroupId - Supplies the primary group the user belongs to.

    Options - Supplies the options the utility was invoked with.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    size_t GroupCount;
    size_t GroupIndex;
    gid_t *Groups;
    INT Result;

    Groups = NULL;
    GroupCount = 0;
    Result = SwGetGroupList(UserId, GroupId, &Groups, &GroupCount);
    if (Result != 0) {
        SwPrintError(Result,
                     NULL,
                     "Failed to get groups for user %u",
                     (unsigned int)UserId);

        return Result;
    }

    for (GroupIndex = 0; GroupIndex < GroupCount; GroupIndex += 1) {
        IdPrintGroupId(Groups[GroupIndex], Options);
        if (GroupIndex != GroupCount - 1) {
            if ((Options & ID_OPTION_ONLY_GROUPS) != 0) {
                printf(" ");

            } else {
                printf(",");
            }
        }
    }

    if (Groups != NULL) {
        free(Groups);
    }

    return 0;
}

VOID
IdPrintUserId (
    uid_t UserId,
    INT Options
    )

/*++

Routine Description:

    This routine prints a user ID (real or effective).

Arguments:

    UserId - Supplies the ID of the user to print.

    Options - Supplies the options the utility was invoked with, which governs
        whether the id number, name or both are printed.

Return Value:

    None.

--*/

{

    PSTR UserName;

    //
    // If there was no exclusive option or names were requested, get the name.
    //

    UserName = NULL;
    if (((Options & ID_OPTION_EXCLUSIVE_MASK) == 0) ||
        ((Options & ID_OPTION_PRINT_NAMES) != 0)) {

        if (SwGetUserNameFromId(UserId, &UserName) != 0) {

            assert(UserName == NULL);

            printf("%u", (unsigned int)UserId);
            return;
        }
    }

    //
    // If one of the "only" options was specified, then either the name or the
    // number is printed.
    //

    if ((Options & ID_OPTION_EXCLUSIVE_MASK) != 0) {
        if ((Options & ID_OPTION_PRINT_NAMES) != 0) {
            printf("%s", UserName);

        } else {
            printf("%u", (unsigned int)UserId);
        }

    //
    // Print both if present.
    //

    } else {
        printf("%u(%s)", (unsigned int)UserId, UserName);
    }

    if (UserName != NULL) {
        free(UserName);
    }

    return;
}

VOID
IdPrintGroupId (
    gid_t GroupId,
    INT Options
    )

/*++

Routine Description:

    This routine prints a group ID.

Arguments:

    GroupId - Supplies the ID of the group to print.

    Options - Supplies the options the utility was invoked with, which governs
        whether the id number, name or both are printed.

Return Value:

    None.

--*/

{

    PSTR GroupName;

    //
    // If there was no exclusive option or names were requested, get the name.
    //

    GroupName = NULL;
    if (((Options & ID_OPTION_EXCLUSIVE_MASK) == 0) ||
        ((Options & ID_OPTION_PRINT_NAMES) != 0)) {

        if (SwGetGroupNameFromId(GroupId, &GroupName) != 0) {

            assert(GroupName == NULL);

            printf("%u", (unsigned int)GroupId);
            return;
        }
    }

    //
    // If one of the "only" options was specified, then either the name or the
    // number is printed.
    //

    if ((Options & ID_OPTION_EXCLUSIVE_MASK) != 0) {
        if ((Options & ID_OPTION_PRINT_NAMES) != 0) {
            printf("%s", GroupName);

        } else {
            printf("%u", (unsigned int)GroupId);
        }

    //
    // Print both if present.
    //

    } else {
        printf("%u(%s)", (unsigned int)GroupId, GroupName);
    }

    if (GroupName != NULL) {
        free(GroupName);
    }

    return;
}

