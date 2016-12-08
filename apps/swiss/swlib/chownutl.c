/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    chownutl.c

Abstract:

    This module implements the core functionality of the chown utility, which
    is used by several different commands (including chown).

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
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ChownPrintUserGroupName (
    uid_t UserId,
    gid_t GroupId
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ChownChangeOwnership (
    PCHOWN_CONTEXT Context,
    PSTR Path,
    ULONG RecursionDepth
    )

/*++

Routine Description:

    This routine executes the body of the chown utility action on a single
    argument.

Arguments:

    Context - Supplies a pointer to the chown context.

    Path - Supplies the path to change.

    RecursionDepth - Supplies the recursion depth. Supply 0 initially.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    BOOL Changed;
    DIR *Directory;
    struct dirent *Entry;
    gid_t OriginalGroup;
    uid_t OriginalUser;
    int Result;
    struct stat Stat;

    if ((Context->Options & CHOWN_OPTION_AFFECT_SYMBOLIC_LINKS) != 0) {
        Result = lstat(Path, &Stat);

    } else {
        Result = stat(Path, &Stat);
    }

    if (Result != 0) {
        Result = errno;
        if ((Context->Options & CHOWN_OPTION_QUIET) == 0) {
            SwPrintError(Result, Path, "Unable to stat");
            return Result;
        }
    }

    //
    // Modify the user/group.
    //

    OriginalGroup = Stat.st_gid;
    OriginalUser = Stat.st_uid;
    if (((Context->FromUser == (uid_t)-1) ||
         (Context->FromUser == Stat.st_uid)) &&
        ((Context->FromGroup == (gid_t)-1) ||
         (Context->FromGroup == Stat.st_gid))) {

        if (Context->User != (uid_t)-1) {
            Stat.st_uid = Context->User;
        }

        if (Context->Group != (gid_t)-1) {
            Stat.st_gid = Context->Group;
        }
    }

    Changed = FALSE;
    if ((Stat.st_uid != OriginalUser) || (Stat.st_gid != OriginalGroup)) {
        Changed = TRUE;
    }

    //
    // Print if needed.
    //

    if (((Context->Options & CHOWN_OPTION_VERBOSE) != 0) ||
        (((Context->Options & CHOWN_OPTION_PRINT_CHANGES) != 0) &&
         (Changed != FALSE))) {

        if (Changed != FALSE) {
            printf("Changed ownership of '%s' from ", Path);
            ChownPrintUserGroupName(OriginalUser, OriginalGroup);
            printf(" to ");
            ChownPrintUserGroupName(Stat.st_uid, Stat.st_gid);

        } else {
            printf("Ownership of '%s' retained as ", Path);
            ChownPrintUserGroupName(OriginalUser, OriginalGroup);
        }

        printf("\n");
    }

    //
    // Actually execute the change.
    //

    Result = 0;
    if (Changed != FALSE) {
        if ((Context->Options & CHOWN_OPTION_AFFECT_SYMBOLIC_LINKS) != 0) {
            Result = lchown(Path, Stat.st_uid, Stat.st_gid);

        } else {
            Result = chown(Path, Stat.st_uid, Stat.st_gid);
        }

        if (Result != 0) {
            Result = errno;
            if ((Context->Options & CHOWN_OPTION_QUIET) == 0) {
                SwPrintError(Result, Path, "Unable to change ownership");
            }
        }
    }

    //
    // Return now if not recursing.
    //

    if ((Context->Options & CHOWN_OPTION_RECURSIVE) == 0) {
        return Result;
    }

    //
    // Recurse down through this directory. Don't go through symbolic links
    // unless requested.
    //

    if (((Context->Options & CHOWN_OPTION_SYMBOLIC_DIRECTORIES) != 0) ||
        (((Context->Options &
           CHOWN_OPTION_SYMBOLIC_DIRECTORY_ARGUMENTS) != 0) &&
          (RecursionDepth == 0))) {

        Result = stat(Path, &Stat);

    } else {
        Result = lstat(Path, &Stat);
    }

    if (Result != 0) {
        Result = errno;
        if ((Context->Options & CHOWN_OPTION_QUIET) == 0) {
            SwPrintError(Result, Path, "Unable to stat");
            return Result;
        }
    }

    if (!S_ISDIR(Stat.st_mode)) {
        return 0;
    }

    Directory = opendir(Path);
    if (Directory == NULL) {
        Result = errno;
        if ((Context->Options & CHOWN_OPTION_QUIET) == 0) {
            SwPrintError(Result, Path, "Cannot open directory");
        }

        return Result;
    }

    Result = 0;
    while (TRUE) {
        errno = 0;
        Entry = readdir(Directory);
        if (Entry == NULL) {
            Result = errno;
            if (Result != 0) {
                if ((Context->Options & CHOWN_OPTION_QUIET) == 0) {
                    SwPrintError(Result, Path, "Unable to read directory");
                }
            }

            break;
        }

        if ((strcmp(Entry->d_name, ".") == 0) ||
            (strcmp(Entry->d_name, "..") == 0)) {

            continue;
        }

        Result = SwAppendPath(Path,
                              strlen(Path) + 1,
                              Entry->d_name,
                              strlen(Entry->d_name) + 1,
                              &AppendedPath,
                              &AppendedPathSize);

        if (Result == FALSE) {
            Result = ENOMEM;
            break;
        }

        Result = ChownChangeOwnership(Context,
                                      AppendedPath,
                                      RecursionDepth + 1);

        free(AppendedPath);
        if (Result != 0) {
            break;
        }
    }

    closedir(Directory);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ChownPrintUserGroupName (
    uid_t UserId,
    gid_t GroupId
    )

/*++

Routine Description:

    This routine prints the user/group ID string to standard out.

Arguments:

    UserId - Supplies the user ID to print.

    GroupId - Supplies the group ID to print.

Return Value:

    None.

--*/

{

    struct group *GroupInformation;
    struct passwd *UserInformation;

    UserInformation = getpwuid(UserId);
    if (UserInformation == NULL) {
        printf("%lu:", (long unsigned int)UserId);

    } else {
        printf("%s:", UserInformation->pw_name);
    }

    GroupInformation = getgrgid(GroupId);
    if (GroupInformation == NULL) {
        printf("%lu", (long unsigned int)GroupId);

    } else {
        printf("%s", GroupInformation->gr_name);
    }

    return;
}

