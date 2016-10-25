/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    passwd.c

Abstract:

    This module implements functionality for getting information about a user.

Author:

    Evan Green 23-Jun-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the standard paths.
//

#define PASSWORD_FILE_PATH "/etc/passwd"
#define GROUP_FILE_PATH "/etc/group"

//
// Define the maximum number of groups a user will belong to in initgroups.
//

#define INITGROUPS_GROUP_MAX 1024

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the static login name buffer.
//

PCHAR ClLoginName;

//
// Store a pointer to the password file.
//

FILE *ClPasswordFile;

//
// Store a pointer to the shared user information structure. There is always a
// buffer after this structure that is the maximum password file line size.
//

struct passwd *ClPasswordInformation;

//
// Store a pointer to the groups file.
//

FILE *ClGroupFile;

//
// Store a pointer to the shared group information structure. There is always
// a buffer after this structure that is the maximum group file line size.
//

struct group *ClGroupInformation;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
struct passwd *
getpwnam (
    const char *UserName
    )

/*++

Routine Description:

    This routine searches the user database for a user matching the given
    name, and returns information about that user. This routine is neither
    reentrant nor thread safe.

Arguments:

    UserName - Supplies a pointer to the null terminated string containing the
        user name to search for.

Return Value:

    Returns a pointer to the user information on success. This buffer may be
    overwritten by subsequent calls to getpwent, getpwnam, or getpwuid.

    NULL on failure or if the given user was not found. On failure, errno will
    be set to provide more information.

--*/

{

    int Result;
    struct passwd *ResultPointer;

    if (ClPasswordInformation == NULL) {
        ClPasswordInformation = malloc(
                               sizeof(struct passwd) + USER_DATABASE_LINE_MAX);

        if (ClPasswordInformation == NULL) {
            return NULL;
        }
    }

    ResultPointer = NULL;
    Result = getpwnam_r(UserName,
                        ClPasswordInformation,
                        (char *)(ClPasswordInformation + 1),
                        USER_DATABASE_LINE_MAX,
                        &ResultPointer);

    if (Result != 0) {
        return NULL;
    }

    return ResultPointer;
}

LIBC_API
int
getpwnam_r (
    const char *UserName,
    struct passwd *UserInformation,
    char *Buffer,
    size_t BufferSize,
    struct passwd **Result
    )

/*++

Routine Description:

    This routine searches the user database for a user matching the given
    name, and returns information about that user. This routine is reentrant
    and thread safe.

Arguments:

    UserName - Supplies a pointer to the null terminated string containing the
        user name to search for.

    UserInformation - Supplies a pointer where the user information will be
        returned.

    Buffer - Supplies a buffer used to allocate strings that the user
        information points to out of. The maximum size needed for this buffer
        can be determined with the _SC_GETPW_R_SIZE_MAX sysconf parameter.

    BufferSize - Supplies the size of the supplied buffer in bytes.

    Result - Supplies a pointer where a pointer to the user information
        parameter will be returned on success, or NULL will be returned if the
        specified user could not be found.

Return Value:

    0 on success.

    Returns an error value on failure.

--*/

{

    FILE *File;
    struct passwd Information;
    int Status;

    *Result = NULL;
    File = fopen(PASSWORD_FILE_PATH, "r");
    if (File == NULL) {
        return errno;
    }

    //
    // Loop through looking for an entry that matches.
    //

    while (TRUE) {
        Status = fgetpwent_r(File, &Information, Buffer, BufferSize, Result);
        if (Status != 0) {
            *Result = NULL;
            break;
        }

        if (*Result == NULL) {
            break;
        }

        //
        // If the user name matches, return it.
        //

        if (strcmp(Information.pw_name, UserName) == 0) {
            memcpy(UserInformation, &Information, sizeof(Information));
            *Result = UserInformation;
            break;
        }
    }

    if (File != NULL) {
        fclose(File);
    }

    return Status;
}

LIBC_API
struct passwd *
getpwuid (
    uid_t UserId
    )

/*++

Routine Description:

    This routine searches the user database for a user matching the given
    ID, and returns information about that user. This routine is neither
    reentrant nor thread safe.

Arguments:

    UserId - Supplies the ID of the user to search for.

Return Value:

    Returns a pointer to the user information on success. This buffer may be
    overwritten by subsequent calls to getpwent, getpwnam, or getpwuid.

    NULL on failure or if the given user was not found. On failure, errno will
    be set to provide more information.

--*/

{

    int Result;
    struct passwd *ResultPointer;

    if (ClPasswordInformation == NULL) {
        ClPasswordInformation = malloc(
                               sizeof(struct passwd) + USER_DATABASE_LINE_MAX);

        if (ClPasswordInformation == NULL) {
            return NULL;
        }
    }

    ResultPointer = NULL;
    Result = getpwuid_r(UserId,
                        ClPasswordInformation,
                        (char *)(ClPasswordInformation + 1),
                        USER_DATABASE_LINE_MAX,
                        &ResultPointer);

    if (Result != 0) {
        return NULL;
    }

    return ResultPointer;
}

LIBC_API
int
getpwuid_r (
    uid_t UserId,
    struct passwd *UserInformation,
    char *Buffer,
    size_t BufferSize,
    struct passwd **Result
    )

/*++

Routine Description:

    This routine searches the user database for a user matching the given
    ID, and returns information about that user. This routine is reentrant
    and thread safe.

Arguments:

    UserId - Supplies the user ID to look up.

    UserInformation - Supplies a pointer where the user information will be
        returned.

    Buffer - Supplies a buffer used to allocate strings that the user
        information points to out of. The maximum size needed for this buffer
        can be determined with the _SC_GETPW_R_SIZE_MAX sysconf parameter.

    BufferSize - Supplies the size of the supplied buffer in bytes.

    Result - Supplies a pointer where a pointer to the user information
        parameter will be returned on success, or NULL will be returned if the
        specified user could not be found.

Return Value:

    0 on success.

    Returns an error value on failure.

--*/

{

    FILE *File;
    struct passwd Information;
    int Status;

    *Result = NULL;
    File = fopen(PASSWORD_FILE_PATH, "r");
    if (File == NULL) {
        return errno;
    }

    //
    // Loop through looking for an entry that matches.
    //

    while (TRUE) {
        Status = fgetpwent_r(File, &Information, Buffer, BufferSize, Result);
        if (Status != 0) {
            *Result = NULL;
            break;
        }

        if (*Result == NULL) {
            break;
        }

        //
        // If the user ID matches, return it.
        //

        if (Information.pw_uid == UserId) {
            memcpy(UserInformation, &Information, sizeof(Information));
            *Result = UserInformation;
            break;
        }
    }

    if (File != NULL) {
        fclose(File);
    }

    return Status;
}

LIBC_API
struct passwd *
getpwent (
    void
    )

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the user database. This function is neither thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to the next entry in the user database. The caller may
    not modify or free this memory, and it may be overwritten by subsequent
    calls to getpwent.

    NULL if the end of the user database is reached or on error.

--*/

{

    int Result;
    struct passwd *ReturnPointer;

    if (ClPasswordInformation == NULL) {
        ClPasswordInformation = malloc(
                               sizeof(struct passwd) + USER_DATABASE_LINE_MAX);

        if (ClPasswordInformation == NULL) {
            return NULL;
        }
    }

    ReturnPointer = NULL;
    Result = getpwent_r(ClPasswordInformation,
                        (char *)(ClPasswordInformation + 1),
                        USER_DATABASE_LINE_MAX,
                        &ReturnPointer);

    if (Result != 0) {
        errno = Result;
        return NULL;
    }

    return ReturnPointer;
}

LIBC_API
int
getpwent_r (
    struct passwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct passwd **ReturnPointer
    )

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the user database. This is the reentrant version of getpwent.

Arguments:

    Information - Supplies a pointer where the user information will be
        returned on success.

    Buffer - Supplies a pointer to the buffer where strings will be stored.

    BufferSize - Supplies the size of the given buffer in bytes.

    ReturnPointer - Supplies a pointer to a pointer to a user information
        structure. On success, the information pointer parameter will be
        returned here.

Return Value:

    0 on success.

    ENOENT if there are no more entries.

    Returns an error value on failure.

--*/

{

    int Result;

    if (ClPasswordFile == NULL) {
        setpwent();
    }

    if (ClPasswordFile == NULL) {
        return errno;
    }

    Result = fgetpwent_r(ClPasswordFile,
                         Information,
                         Buffer,
                         BufferSize,
                         ReturnPointer);

    return Result;
}

LIBC_API
int
fgetpwent_r (
    FILE *File,
    struct passwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct passwd **ReturnPointer
    )

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the user database.

Arguments:

    File - Supplies a pointer to a file to read the information from.

    Information - Supplies a pointer where the user information will be
        returned on success.

    Buffer - Supplies a pointer to the buffer where strings will be stored.

    BufferSize - Supplies the size of the given buffer in bytes.

    ReturnPointer - Supplies a pointer to a pointer to a user information
        structure. On success, the information pointer parameter will be
        returned here.

Return Value:

    0 on success.

    ENOENT if there are no more entries.

    Returns an error value on failure.

--*/

{

    PSTR AfterScan;
    PSTR Current;
    CHAR Line[USER_DATABASE_LINE_MAX];
    PSTR OriginalBuffer;
    size_t OriginalBufferSize;

    OriginalBuffer = Buffer;
    OriginalBufferSize = BufferSize;

    //
    // Loop trying to scan a good line.
    //

    *ReturnPointer = NULL;
    while (TRUE) {
        if (fgets(Line, sizeof(Line), File) == NULL) {
            if (ferror(File)) {
                return errno;
            }

            return 0;
        }

        Line[sizeof(Line) - 1] = '\0';
        Buffer = OriginalBuffer;
        BufferSize = OriginalBufferSize;

        //
        // Skip any spaces.
        //

        Current = Line;
        while (isspace(*Current) != 0) {
            Current += 1;
        }

        //
        // Skip any empty or commented lines.
        //

        if ((*Current == '\0') || (*Current == '#')) {
            continue;
        }

        //
        // Grab the username. Skip malformed lines.
        //

        Information->pw_name = Buffer;
        while ((BufferSize != 0) && (*Current != '\0') && (*Current != ':')) {
            *Buffer = *Current;
            Buffer += 1;
            Current += 1;
            BufferSize -= 1;
        }

        if (BufferSize != 0) {
            *Buffer = '\0';
            Buffer += 1;
        }

        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Grab the password.
        //

        Information->pw_passwd = Buffer;
        while ((BufferSize != 0) && (*Current != '\0') && (*Current != ':')) {
            *Buffer = *Current;
            Buffer += 1;
            Current += 1;
            BufferSize -= 1;
        }

        if (BufferSize != 0) {
            *Buffer = '\0';
            Buffer += 1;
        }

        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Grab the user ID.
        //

        Information->pw_uid = strtol(Current, &AfterScan, 10);
        if (AfterScan == Current) {
            continue;
        }

        Current = AfterScan;
        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Grab the group ID.
        //

        Information->pw_gid = strtoul(Current, &AfterScan, 10);
        if (AfterScan == Current) {
            continue;
        }

        Current = AfterScan;
        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Grab the full name of the user.
        //

        Information->pw_gecos = Buffer;
        while ((BufferSize != 0) && (*Current != '\0') && (*Current != ':')) {
            *Buffer = *Current;
            Buffer += 1;
            Current += 1;
            BufferSize -= 1;
        }

        if (BufferSize != 0) {
            *Buffer = '\0';
            Buffer += 1;
        }

        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Grab the home directory.
        //

        Information->pw_dir = Buffer;
        while ((BufferSize != 0) && (*Current != '\0') && (*Current != ':')) {
            *Buffer = *Current;
            Buffer += 1;
            Current += 1;
            BufferSize -= 1;
        }

        if (BufferSize != 0) {
            *Buffer = '\0';
            Buffer += 1;
        }

        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Grab the shell.
        //

        Information->pw_shell = Buffer;
        while ((BufferSize != 0) &&
               (*Current != '\0') && (*Current != ':') &&
               (!isspace(*Current))) {

            *Buffer = *Current;
            Buffer += 1;
            Current += 1;
            BufferSize -= 1;
        }

        if (BufferSize != 0) {
            *Buffer = '\0';
            Buffer += 1;
        }

        *ReturnPointer = Information;
        break;
    }

    return 0;
}

LIBC_API
void
setpwent (
    void
    )

/*++

Routine Description:

    This routine rewinds the user database handle back to the beginning of the
    user database. The next call to getpwent will return the first entry in the
    user database.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClPasswordFile == NULL) {
        ClPasswordFile = fopen(PASSWORD_FILE_PATH, "r");

    } else {
        fseek(ClPasswordFile, 0, SEEK_SET);
    }

    return;
}

LIBC_API
void
endpwent (
    void
    )

/*++

Routine Description:

    This routine closes an open handle to the user database established with
    setpwent or getpwent.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClPasswordFile != NULL) {
        fclose(ClPasswordFile);
        ClPasswordFile = NULL;
    }

    return;
}

LIBC_API
int
putpwent (
    const struct passwd *Record,
    FILE *Stream
    )

/*++

Routine Description:

    This routine records a new password record in the given stream.

Arguments:

    Record - Supplies the password record to add.

    Stream - Supplies the stream to write the record to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    PSTR Gecos;
    PSTR Home;
    PSTR Password;
    int Result;
    PSTR Shell;

    if ((Record == NULL) || (Stream == NULL)) {
        errno = EINVAL;
        return -1;
    }

    Password = "";
    Gecos = "";
    Home = "";
    Shell = "";
    if (Record->pw_passwd != NULL) {
        Password = Record->pw_passwd;
    }

    if (Record->pw_gecos != NULL) {
        Gecos = Record->pw_gecos;
    }

    if (Record->pw_dir != NULL) {
        Home = Record->pw_dir;
    }

    if (Record->pw_shell != NULL) {
        Shell = Record->pw_shell;
    }

    flockfile(Stream);
    if ((Record->pw_name[0] == '+') || (Record->pw_name[0] == '-')) {
        Result = fprintf_unlocked(Stream,
                                  "%s:%s:::%s:%s:%s\n",
                                  Record->pw_name,
                                  Password,
                                  Gecos,
                                  Home,
                                  Shell);

    } else {
        Result = fprintf_unlocked(Stream,
                                  "%s:%s:%lu:%lu:%s:%s:%s\n",
                                  Record->pw_name,
                                  Password,
                                  (unsigned long int)(Record->pw_uid),
                                  (unsigned long int)(Record->pw_gid),
                                  Gecos,
                                  Home,
                                  Shell);
    }

    funlockfile(Stream);
    if (Result < 0) {
        return -1;
    }

    return 0;
}

LIBC_API
char *
getlogin (
    void
    )

/*++

Routine Description:

    This routine returns a pointer to a string containing the user name
    associated by the login activity with the controlling terminal of the
    current process. This routine is neither reentrant nor thread safe.

Arguments:

    None.

Return Value:

    Returns a pointer to a buffer containing the name of the logged in user.
    This data may be overwritten by a subsequent call to this function.

    NULL on failure, and errno will be set to contain more information.

--*/

{

    return getenv("LOGNAME");
}

LIBC_API
int
getlogin_r (
    char *Buffer,
    size_t BufferSize
    )

/*++

Routine Description:

    This routine returns a pointer to a string containing the user name
    associated by the login activity with the controlling terminal of the
    current process. This routine is thread safe and reentrant.

Arguments:

    Buffer - Supplies a pointer to the buffer where the login name will be
        returned.

    BufferSize - Supplies the size of the supplied buffer in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    char *EnvironmentValue;

    if ((Buffer == NULL) || (BufferSize == 0)) {
        return EINVAL;
    }

    EnvironmentValue = getenv("LOGNAME");
    if (EnvironmentValue == NULL) {
        *Buffer = '\0';

    } else {
        strncpy(Buffer, EnvironmentValue, BufferSize);
    }

    return 0;
}

LIBC_API
struct group *
getgrnam (
    const char *GroupName
    )

/*++

Routine Description:

    This routine searches the group database for a group matching the given
    name, and returns information about that group. This routine is neither
    reentrant nor thread safe.

Arguments:

    GroupName - Supplies a pointer to the null terminated string containing the
        group name to search for.

Return Value:

    Returns a pointer to the group information on success. This buffer may be
    overwritten by subsequent calls to getgrent, getgrgid, or getgrnam.

    NULL on failure or if the given group was not found. On failure, errno will
    be set to provide more information.

--*/

{

    int Result;
    struct group *ResultPointer;

    if (ClGroupInformation == NULL) {
        ClGroupInformation = malloc(
                               sizeof(struct group) + USER_DATABASE_LINE_MAX);

        if (ClGroupInformation == NULL) {
            return NULL;
        }
    }

    ResultPointer = NULL;
    Result = getgrnam_r(GroupName,
                        ClGroupInformation,
                        (char *)(ClGroupInformation + 1),
                        USER_DATABASE_LINE_MAX,
                        &ResultPointer);

    if (Result != 0) {
        errno = Result;
        return NULL;
    }

    return ResultPointer;
}

LIBC_API
int
getgrnam_r (
    const char *GroupName,
    struct group *GroupInformation,
    char *Buffer,
    size_t BufferSize,
    struct group **Result
    )

/*++

Routine Description:

    This routine searches the group database for a group matching the given
    name, and returns information about that group. This routine is reentrant
    and thread safe.

Arguments:

    GroupName - Supplies a pointer to the null terminated string containing the
        group name to search for.

    GroupInformation - Supplies a pointer where the group information will be
        returned.

    Buffer - Supplies a buffer used to allocate strings that the group
        information points to out of. The maximum size needed for this buffer
        can be determined with the _SC_GETGR_R_SIZE_MAX sysconf parameter.

    BufferSize - Supplies the size of the supplied buffer in bytes.

    Result - Supplies a pointer where a pointer to the group information
        parameter will be returned on success, or NULL will be returned if the
        specified group could not be found.

Return Value:

    0 on success.

    ENOENT if there are no more entries.

    Returns an error value on failure.

--*/

{

    FILE *File;
    struct group Information;
    int Status;

    *Result = NULL;
    File = fopen(GROUP_FILE_PATH, "r");
    if (File == NULL) {
        return errno;
    }

    //
    // Loop through looking for an entry that matches.
    //

    while (TRUE) {
        Status = fgetgrent_r(File, &Information, Buffer, BufferSize, Result);
        if (Status != 0) {
            *Result = NULL;
            break;
        }

        if (*Result == NULL) {
            break;
        }

        //
        // If the user name matches, return it.
        //

        if (strcmp(Information.gr_name, GroupName) == 0) {
            memcpy(GroupInformation, &Information, sizeof(Information));
            *Result = GroupInformation;
            break;
        }
    }

    if (File != NULL) {
        fclose(File);
    }

    return Status;
}

LIBC_API
struct group *
getgrgid (
    gid_t GroupId
    )

/*++

Routine Description:

    This routine searches the group database for a group matching the given ID.
    This routine is neither reentrant nor thread safe.

Arguments:

    GroupId - Supplies the ID of the group to search for.

Return Value:

    Returns a pointer to the group information structure on success. This
    buffer may be overwritten by subsequent calls to getgrent, getgrgid, or
    getgrnam.

    NULL on failure or if the requested group was not found. On failure, errno
    will be set to provide more information.

--*/

{

    int Result;
    struct group *ResultPointer;

    if (ClGroupInformation == NULL) {
        ClGroupInformation = malloc(
                               sizeof(struct group) + USER_DATABASE_LINE_MAX);

        if (ClGroupInformation == NULL) {
            return NULL;
        }
    }

    ResultPointer = NULL;
    Result = getgrgid_r(GroupId,
                        ClGroupInformation,
                        (char *)(ClGroupInformation + 1),
                        USER_DATABASE_LINE_MAX,
                        &ResultPointer);

    if (Result != 0) {
        errno = Result;
        return NULL;
    }

    return ResultPointer;
}

LIBC_API
int
getgrgid_r (
    gid_t GroupId,
    struct group *Group,
    char *Buffer,
    size_t BufferSize,
    struct group **ResultPointer
    )

/*++

Routine Description:

    This routine searches the group database for a group matching the given ID.
    This routine is both reentrant and thread safe.

Arguments:

    GroupId - Supplies the ID of the group to search for.

    Group - Supplies a pointer where the group information will be returned.
        All pointers to buffers returned will point inside the passed in buffer.

    Buffer - Supplies a pointer to a buffer to use for additional group
        information. This buffer is used to return things like the group name
        and members array. The maximum size needed for this buffer can be
        determined with the _SC_GETGR_R_SIZE_MAX sysconf parameter.

    BufferSize - Supplies the size of the supplied buffer in bytes.

    ResultPointer - Supplies a pointer where the group parameter pointer will
        be returned on success. If no group matching the given ID was found,
        NULL will be returned here.

Return Value:

    0 on success.

    Returns an error value on failure.

--*/

{

    FILE *File;
    struct group Information;
    int Status;

    *ResultPointer = NULL;
    File = fopen(GROUP_FILE_PATH, "r");
    if (File == NULL) {
        return errno;
    }

    //
    // Loop through looking for an entry that matches.
    //

    while (TRUE) {
        Status = fgetgrent_r(File,
                             &Information,
                             Buffer,
                             BufferSize,
                             ResultPointer);

        if (Status != 0) {
            *ResultPointer = NULL;
            break;
        }

        if (*ResultPointer == NULL) {
            break;
        }

        //
        // If the group ID matches, return it.
        //

        if (Information.gr_gid == GroupId) {
            memcpy(Group, &Information, sizeof(Information));
            *ResultPointer = Group;
            break;
        }
    }

    if (File != NULL) {
        fclose(File);
    }

    return Status;
}

LIBC_API
struct group *
getgrent (
    void
    )

/*++

Routine Description:

    This routine returns a pointer to a structure containing the broken out
    fields of an entry in the group database. Subsequent calls to this function
    return the next successive entries in the group database, so this routine
    can be called repeatedly to iterate over the entire group database. This
    routine is neither thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to the first group entry upon the first call. The buffer
    returned may be overwritten by subsequent calls to this routine. The
    memory returned here must not be modified or freed by the caller.

    Returns pointers to successive group entries in the group database on
    additional calls.

    NULL if no more entries exist or an error occurred. The errno variable will
    be set to contain more information if an error occurred.

--*/

{

    int Result;
    struct group *ReturnPointer;

    if (ClGroupInformation == NULL) {
        ClGroupInformation = malloc(
                               sizeof(struct group) + USER_DATABASE_LINE_MAX);

        if (ClGroupInformation == NULL) {
            return NULL;
        }
    }

    ReturnPointer = NULL;
    Result = getgrent_r(ClGroupInformation,
                        (char *)(ClGroupInformation + 1),
                        USER_DATABASE_LINE_MAX,
                        &ReturnPointer);

    if (Result != 0) {
        return NULL;
    }

    return ReturnPointer;
}

LIBC_API
int
getgrent_r (
    struct group *Information,
    char *Buffer,
    size_t BufferSize,
    struct group **ReturnPointer
    )

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the group database. This is the reentrant version of getgrent.

Arguments:

    Information - Supplies a pointer where the group information will be
        returned on success.

    Buffer - Supplies a pointer to the buffer where strings will be stored.

    BufferSize - Supplies the size of the given buffer in bytes.

    ReturnPointer - Supplies a pointer to a pointer to a group information
        structure. On success, the information pointer parameter will be
        returned here.

Return Value:

    0 on success.

    ENOENT if there are no more entries.

    Returns an error number on failure.

--*/

{

    int Result;

    if (ClGroupFile == NULL) {
        setgrent();
    }

    if (ClGroupFile == NULL) {
        return errno;
    }

    Result = fgetgrent_r(ClGroupFile,
                         Information,
                         Buffer,
                         BufferSize,
                         ReturnPointer);

    return Result;
}

LIBC_API
int
fgetgrent_r (
    FILE *File,
    struct group *Information,
    char *Buffer,
    size_t BufferSize,
    struct group **ReturnPointer
    )

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the group database. This is the reentrant version of getgrent.

Arguments:

    File - Supplies a pointer to the group database file to read the
        information from.

    Information - Supplies a pointer where the group information will be
        returned on success.

    Buffer - Supplies a pointer to the buffer where strings will be stored.

    BufferSize - Supplies the size of the given buffer in bytes.

    ReturnPointer - Supplies a pointer to a pointer to a group information
        structure. On success, the information pointer parameter will be
        returned here.

Return Value:

    0 on success.

    ENOENT if there are no more entries.

    Returns an error number on failure.

--*/

{

    PSTR AfterScan;
    PSTR Current;
    CHAR Line[USER_DATABASE_LINE_MAX];
    INT MemberCount;
    INT MemberIndex;
    PSTR OriginalBuffer;
    size_t OriginalBufferSize;
    PSTR Search;

    OriginalBuffer = Buffer;
    OriginalBufferSize = BufferSize;

    //
    // Loop trying to scan a good line.
    //

    *ReturnPointer = NULL;
    while (TRUE) {
        if (fgets(Line, sizeof(Line), File) == NULL) {
            if (ferror(File)) {
                return errno;
            }

            return 0;
        }

        Line[sizeof(Line) - 1] = '\0';
        Buffer = OriginalBuffer;
        BufferSize = OriginalBufferSize;

        //
        // Skip any spaces.
        //

        Current = Line;
        while (isspace(*Current) != 0) {
            Current += 1;
        }

        //
        // Skip any empty or commented lines.
        //

        if ((*Current == '\0') || (*Current == '#')) {
            continue;
        }

        //
        // Grab the group name. Skip malformed lines.
        //

        Information->gr_name = Buffer;
        while ((BufferSize != 0) && (*Current != '\0') && (*Current != ':')) {
            *Buffer = *Current;
            Buffer += 1;
            Current += 1;
            BufferSize -= 1;
        }

        if (BufferSize != 0) {
            *Buffer = '\0';
            Buffer += 1;
        }

        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Grab the password.
        //

        Information->gr_passwd = Buffer;
        while ((BufferSize != 0) && (*Current != '\0') && (*Current != ':')) {
            *Buffer = *Current;
            Buffer += 1;
            Current += 1;
            BufferSize -= 1;
        }

        if (BufferSize != 0) {
            *Buffer = '\0';
            Buffer += 1;
        }

        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Grab the group ID.
        //

        Information->gr_gid = strtoul(Current, &AfterScan, 10);
        if (AfterScan == Current) {
            continue;
        }

        Current = AfterScan;
        if (*Current != ':') {
            continue;
        }

        Current += 1;

        //
        // Okay, time to deal with the array of members. First, count the
        // commas to determine how many members there are. Start with two to
        // account for the null member and the fact that there is one more
        // name than there is comma (ie a,b has two names and one comma).
        //

        Search = Current;
        MemberCount = 2;
        while ((*Search != '\0') && (*Search != ':') && (!isspace(*Search))) {
            if (*Search == ',') {
                MemberCount += 1;
            }

            Search += 1;
        }

        //
        // Allocate space from the buffer for the array.
        //

        Information->gr_mem = NULL;
        if (MemberCount * sizeof(PSTR) >= BufferSize) {
            break;
        }

        Information->gr_mem = (char **)Buffer;
        Buffer += MemberCount * sizeof(PSTR);
        BufferSize -= MemberCount * sizeof(PSTR);
        MemberIndex = 0;
        Information->gr_mem[MemberIndex] = NULL;

        //
        // Loop through and fill in the group members.
        //

        while ((*Current != '\0') && (*Current != ':') &&
               (!isspace(*Current)) && (BufferSize != 0)) {

            //
            // If it's a member separator, move to the next member, but only
            // if this member has something in it.
            //

            if (*Current == ',') {
                if (Information->gr_mem[MemberIndex] != NULL) {
                    *Buffer = '\0';
                    Buffer += 1;
                    BufferSize -= 1;
                    MemberIndex += 1;
                }

            } else {

                //
                // If this is the first character of the new member, set the
                // array pointer.
                //

                if (Information->gr_mem[MemberIndex] == NULL) {
                    Information->gr_mem[MemberIndex] = Buffer;
                    Information->gr_mem[MemberIndex + 1] = NULL;

                    assert(MemberIndex + 1 < MemberCount);
                }

                *Buffer = *Current;
                Buffer += 1;
                BufferSize -= 1;
            }

            Current += 1;
        }

        *ReturnPointer = Information;
        break;
    }

    return 0;
}

LIBC_API
void
setgrent (
    void
    )

/*++

Routine Description:

    This routine rewinds the group database to allow repeated searches via
    getgrent.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClGroupFile == NULL) {
        ClGroupFile = fopen(GROUP_FILE_PATH, "r");

    } else {
        fseek(ClGroupFile, 0, SEEK_SET);
    }

    return;
}

LIBC_API
void
endgrent (
    void
    )

/*++

Routine Description:

    This routine closes the group database when the process is done calling
    getgrent.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClGroupFile != NULL) {
        fclose(ClGroupFile);
        ClGroupFile = NULL;
    }

    return;
}

LIBC_API
int
putgrent (
    const struct group *Record,
    FILE *Stream
    )

/*++

Routine Description:

    This routine writes a group database record out to the given file.

Arguments:

    Record - Supplies a pointer to the group record.

    Stream - Supplies the file stream to write the record to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    UINTN Index;
    PSTR Password;
    int Result;

    if ((Record == NULL) || (Stream == NULL)) {
        errno = EINVAL;
        return -1;
    }

    Password = "";
    if (Record->gr_passwd != NULL) {
        Password = Record->gr_passwd;
    }

    flockfile(Stream);
    if ((Record->gr_name[0] == '+') || (Record->gr_name[0] == '-')) {
        Result = fprintf_unlocked(Stream,
                                  "%s:%s::",
                                  Record->gr_name,
                                  Password);

    } else {
        Result = fprintf_unlocked(Stream,
                                  "%s:%s:%lu:",
                                  Record->gr_name,
                                  Password,
                                  (unsigned long int)(Record->gr_gid));
    }

    if (Result < 0) {
        goto putgrentEnd;
    }

    Index = 0;
    while (Record->gr_mem[Index] != NULL) {
        if (Index == 0) {
            Result = fprintf_unlocked(Stream, "%s", Record->gr_mem[Index]);

        } else {
            Result = fprintf_unlocked(Stream, ",%s", Record->gr_mem[Index]);
        }

        if (Result <= 0) {
            goto putgrentEnd;
        }
    }

    if (fputc_unlocked('\n', Stream) == EOF) {
        Result = -1;
        goto putgrentEnd;
    }

    Result = 0;

putgrentEnd:
    funlockfile(Stream);
    if (Result != 0) {
        return -1;
    }

    return 0;
}

LIBC_API
int
getgrouplist (
    const char *UserName,
    gid_t GroupId,
    gid_t *Groups,
    int *GroupCount
    )

/*++

Routine Description:

    This routine gets the list of groups that the given user belongs to.

Arguments:

    UserName - Supplies a pointer to a string containing the user name of the
        user whose groups are desired.

    GroupId - Supplies a group ID that if not in the list of groups the given
        user belongs to will also be included in the return returns. Typically
        this argument is specified as the group ID from the password record
        for the given user.

    Groups - Supplies an array where the membership groups of the given user
        will be returned.

    GroupCount - Supplies a pointer that on input contains the maximum number
        of elements can can be stored in the supplied groups buffer. On
        output, contains the number of groups found for the user, even if this
        is greater than the number of groups supplied.

Return Value:

    Returns the number of groups the user belongs to on success.

    -1 if the number of groups the user belongs to is greater than the size of
    the buffer passed in. In this case the group count parameter will contain
    the correct number.

--*/

{

    UINTN Count;
    struct group *GroupInformation;
    UINTN Index;
    PSTR Member;
    UINTN MemberIndex;
    int Result;

    Count = *GroupCount;
    Result = 0;
    if (Count == 0) {
        Result = -1;

    } else {
        Groups[0] = GroupId;
    }

    Index = 1;
    setgrent();
    while (TRUE) {
        GroupInformation = getgrent();
        if (GroupInformation == NULL) {
            break;
        }

        if (GroupInformation->gr_gid == GroupId) {
            continue;
        }

        if (GroupInformation->gr_mem == NULL) {
            continue;
        }

        //
        // Loop through all the members looking for this user.
        //

        MemberIndex = 0;
        while (TRUE) {
            Member = GroupInformation->gr_mem[MemberIndex];
            if (Member == NULL) {
                break;
            }

            if (strcmp(Member, UserName) == 0) {
                if (Index >= Count) {
                    Result = -1;

                } else {
                    Groups[Index] = GroupInformation->gr_gid;
                }

                Index += 1;
                break;
            }

            MemberIndex += 1;
        }
    }

    endgrent();
    *GroupCount = Index;
    if (Result == 0) {
        return Index;
    }

    return Result;
}

LIBC_API
int
initgroups (
    const char *User,
    gid_t Group
    )

/*++

Routine Description:

    This routine initializes the group access list by reading the group
    database and setting the current supplementary group list to all the
    groups the user belongs to. The caller must have sufficient privileges to
    set the supplementary group list.

Arguments:

    User - Supplies a pointer to a null-terminated string containing the
        user name to initialize group access for.

    Group - Supplies an additional group that will also be added to the list.

Return Value:

    0 on success.

    -1 on failure, and errno is set to contain more information.

--*/

{

    gid_t *AllocatedGroups;
    int GroupCount;
    gid_t Groups[INITGROUPS_GROUP_MAX];
    int Result;

    //
    // Try first with a stack buffer that will be big enough for all but the
    // most extreme cases.
    //

    GroupCount = INITGROUPS_GROUP_MAX;
    Result = getgrouplist(User, Group, Groups, &GroupCount);
    if (Result > 0) {
        return setgroups(GroupCount, Groups);
    }

    //
    // Allocate a buffer to hold the massive list, and use that to get the
    // group list.
    //

    AllocatedGroups = malloc(GroupCount * sizeof(gid_t));
    if (AllocatedGroups != NULL) {
        Result = getgrouplist(User, Group, AllocatedGroups, &GroupCount);
        if (Result > 0) {
            Result = setgroups(GroupCount, AllocatedGroups);
        }

        free(AllocatedGroups);

    } else {
        Result = -1;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

