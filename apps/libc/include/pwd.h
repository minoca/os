/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    pwd.h

Abstract:

    This header contains definitions for working with the current user.

Author:

    Evan Green 23-Jun-2013

--*/

#ifndef _PWD_H
#define _PWD_H

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes information about a user.

Members:

    pw_name - Stores a pointer to a string containing the user's login name.

    pw_passwd - Stores a pointer to a string containing the user's encrypted
        password.

    pw_uid - Stores the user's unique identifier.

    pw_gid - Stores the user's group identifier.

    pw_gecos - Stores a pointer to a string containing the user's real name,
        and possibly other information such as a phone number.

    pw_dir - Stores a pointer to a string containing the user's home directory
        path.

    pw_shell - Stores a pointer to a string containing the path to a program
        the user should use as a shell.

--*/

struct passwd {
    char *pw_name;
    char *pw_passwd;
    uid_t pw_uid;
    gid_t pw_gid;
    char *pw_gecos;
    char *pw_dir;
    char *pw_shell;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
struct passwd *
getpwnam (
    const char *UserName
    );

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

LIBC_API
int
getpwnam_r (
    const char *UserName,
    struct passwd *UserInformation,
    char *Buffer,
    size_t BufferSize,
    struct passwd **Result
    );

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

LIBC_API
struct passwd *
getpwuid (
    uid_t UserId
    );

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

LIBC_API
int
getpwuid_r (
    uid_t UserId,
    struct passwd *UserInformation,
    char *Buffer,
    size_t BufferSize,
    struct passwd **Result
    );

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

LIBC_API
struct passwd *
getpwent (
    void
    );

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

LIBC_API
int
getpwent_r (
    struct passwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct passwd **ReturnPointer
    );

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

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
fgetpwent_r (
    FILE *File,
    struct passwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct passwd **ReturnPointer
    );

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

LIBC_API
void
setpwent (
    void
    );

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

LIBC_API
void
endpwent (
    void
    );

/*++

Routine Description:

    This routine closes an open handle to the user database established with
    setpwent or getpwent.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
int
putpwent (
    const struct passwd *Record,
    FILE *Stream
    );

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

#ifdef __cplusplus

}

#endif
#endif

