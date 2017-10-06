/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    shadow.h

Abstract:

    This header contains definitions for shadow passwords.

Author:

    Evan Green 3-Mar-2015

--*/

#ifndef _SHADOW_H
#define _SHADOW_H

//
// ------------------------------------------------------------------- Includes
//

#include <paths.h>
#include <stdio.h>
#include <stddef.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the path to the shadow password file.
//

#define SHADOW _PATH_SHADOW

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the structure of the shadow password file.

Members:

    sp_namp - Stores a pointer to the login name.

    sp_pwdp - Stores a pointer to the encrypted password.

    sp_lstchg - Stores the date of the last password change, in number of days
        since 1970 UTC.

    sp_min - Stores the minimum number of days between password changes.

    sp_max - Stores the maximum number of days between password changes.

    sp_warn - Stores the number of days after which the user should be warned
        to change their password.

    sp_inact - Stores the number of days after the password expires that an
        account is disabled.

    sp_expire - Stores the date the account expires, in number of days since
        1970 UTC.

    sp_flag - Stores a reserved flags field.

--*/

struct spwd {
    char *sp_namp;
    char *sp_pwdp;
    long int sp_lstchg;
    long int sp_min;
    long int sp_max;
    long int sp_warn;
    long int sp_inact;
    long int sp_expire;
    unsigned long int sp_flag;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
struct spwd *
getspnam (
    const char *UserName
    );

/*++

Routine Description:

    This routine searches the shadow password database for a user matching the
    given name, and returns information about that user's password. This
    routine is neither reentrant nor thread safe.

Arguments:

    UserName - Supplies a pointer to the null terminated string containing the
        user name to search for.

Return Value:

    Returns a pointer to the user password information on success. This buffer
    may be overwritten by subsequent calls to getspent or getspnam.

    NULL on failure or if the given user was not found. On failure, errno will
    be set to provide more information.

--*/

LIBC_API
int
getspnam_r (
    const char *UserName,
    struct spwd *PasswordInformation,
    char *Buffer,
    size_t BufferSize,
    struct spwd **Result
    );

/*++

Routine Description:

    This routine searches the shadow password database for a user matching the
    given name, and returns information about that user's password.

Arguments:

    UserName - Supplies a pointer to the null terminated string containing the
        user name to search for.

    PasswordInformation - Supplies a pointer where the user's password
        information will be returned.

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
struct spwd *
getspent (
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
    calls to getspent.

    NULL if the end of the user database is reached or on error.

--*/

LIBC_API
int
getspent_r (
    struct spwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct spwd **ReturnPointer
    );

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the user password database. This is the reentrant version of getspent.

Arguments:

    Information - Supplies a pointer where the user password information will
        be returned on success.

    Buffer - Supplies a pointer to the buffer where strings will be stored.

    BufferSize - Supplies the size of the given buffer in bytes.

    ReturnPointer - Supplies a pointer to a pointer to a user password
        information structure. On success, the information pointer parameter
        will be returned here.

Return Value:

    0 on success.

    ENOENT if there are no more entries.

    Returns an error value on failure.

--*/

LIBC_API
struct spwd *
fgetspent (
    FILE *File
    );

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the user password database.

Arguments:

    File - Supplies a pointer to a file to read the information from.

Return Value:

    Returns a pointer to the next entry in the user database. The caller may
    not modify or free this memory, and it may be overwritten by subsequent
    calls to getspent.

    NULL if the end of the user database is reached or on error.

--*/

LIBC_API
int
fgetspent_r (
    FILE *File,
    struct spwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct spwd **ReturnPointer
    );

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the user password database.

Arguments:

    File - Supplies a pointer to a file to read the information from.

    Information - Supplies a pointer where the user password information will
        be returned on success.

    Buffer - Supplies a pointer to the buffer where strings will be stored.

    BufferSize - Supplies the size of the given buffer in bytes.

    ReturnPointer - Supplies a pointer to a pointer to a user password
        information structure. On success, the information pointer parameter
        will be returned here.

Return Value:

    0 on success.

    ENOENT if there are no more entries.

    Returns an error value on failure.

--*/

LIBC_API
struct spwd *
sgetspent (
    const char *String
    );

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the user database based on a shadow password file line.

Arguments:

    String - Supplies a pointer to the shadow password line.

Return Value:

    Returns a pointer to the next entry in the user database. The caller may
    not modify or free this memory, and it may be overwritten by subsequent
    calls to getspent.

    NULL if the end of the user database is reached or on error.

--*/

LIBC_API
int
sgetspent_r (
    const char *String,
    struct spwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct spwd **ReturnPointer
    );

/*++

Routine Description:

    This routine returns a pointer to the broken out fields of the next entry
    in the user database based on a shadow password file line.

Arguments:

    String - Supplies a pointer to the shadow password line.

    Information - Supplies a pointer where the user password information will
        be returned on success.

    Buffer - Supplies a pointer to the buffer where strings will be stored.

    BufferSize - Supplies the size of the given buffer in bytes.

    ReturnPointer - Supplies a pointer to a pointer to a user password
        information structure. On success, the information pointer parameter
        will be returned here.

Return Value:

    0 on success.

    ENOENT if there are no more entries.

    Returns an error value on failure.

--*/

LIBC_API
void
setspent (
    void
    );

/*++

Routine Description:

    This routine rewinds the user password shadow database handle back to the
    beginning of the database. The next call to getspent will return the first
    entry in the user database.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
void
endspent (
    void
    );

/*++

Routine Description:

    This routine closes an open handle to the user password database
    established with setspent or getspent.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
int
putspent (
    const struct spwd *Information,
    FILE *Stream
    );

/*++

Routine Description:

    This routine writes a shadow password entry into the given shadow password
    database file stream. This routine is neither thread safe nor reentrant.

Arguments:

    Information - Supplies a pointer to the password information to write.

    Stream - Supplies a pointer to the file stream to write it out to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
lckpwdf (
    void
    );

/*++

Routine Description:

    This routine locks the shadow password database file. This routine is not
    multi-thread safe. This mechanism does not prevent someone from writing
    directly to the shadow password file, as the locks it uses are
    discretionary.

Arguments:

    None.

Return Value:

    0 on success.

    -1 if the lock could not be acquired within 15 seconds, and errno may be
    set to contain more information.

--*/

LIBC_API
int
ulckpwdf (
    void
    );

/*++

Routine Description:

    This routine unlocks the shadow password file.

Arguments:

    None.

Return Value:

    0 on success.

    -1 on failure.

--*/

#ifdef __cplusplus

}

#endif
#endif

