/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    grp.h

Abstract:

    This header contains definitions for dealing with security groups.

Author:

    Evan Green 26-Jun-2013

--*/

#ifndef _GRP_H
#define _GRP_H

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

    This structure defines the group structure which contains information about
    a security group.

Members:

    gr_name - Stores a pointer to a null terminated string containing the name
        of the group.

    gr_passwd - Stores a pointer to the null terminated encrypted group
        password string (an obscure feature).

    gr_gid - Stores the numerical group ID.

    gr_mem - Stores a pointer to an array of character pointers to group member
        names. The array is null terminated.

--*/

struct group {
    char *gr_name;
    char *gr_passwd;
    gid_t gr_gid;
    char **gr_mem;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
struct group *
getgrnam (
    const char *GroupName
    );

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

LIBC_API
int
getgrnam_r (
    const char *GroupName,
    struct group *GroupInformation,
    char *Buffer,
    size_t BufferSize,
    struct group **Result
    );

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

    Returns an error value on failure.

--*/

LIBC_API
struct group *
getgrgid (
    gid_t GroupId
    );

/*++

Routine Description:

    This routine searches the group database for a group matching the given ID.
    This routine is neither reentrant nor thread safe.

Arguments:

    GroupId - Supplies the ID of the group to search for.

Return Value:

    Returns a pointer to the group information structure on success. This
    buffer may be overwritten by subsequent calls to this routine.

    NULL on failure or if the requested group was not found. On failure, errno
    will be set to provide more information.

--*/

LIBC_API
int
getgrgid_r (
    gid_t GroupId,
    struct group *Group,
    char *Buffer,
    size_t BufferSize,
    struct group **ResultPointer
    );

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

LIBC_API
struct group *
getgrent (
    void
    );

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

LIBC_API
int
getgrent_r (
    struct group *Information,
    char *Buffer,
    size_t BufferSize,
    struct group **ReturnPointer
    );

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

LIBC_API
int
fgetgrent_r (
    FILE *File,
    struct group *Information,
    char *Buffer,
    size_t BufferSize,
    struct group **ReturnPointer
    );

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

LIBC_API
void
setgrent (
    void
    );

/*++

Routine Description:

    This routine rewinds the group database to allow repeated searches via
    getgrent.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
void
endgrent (
    void
    );

/*++

Routine Description:

    This routine closes the group database when the process is done calling
    getgrent.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
int
putgrent (
    const struct group *Record,
    FILE *Stream
    );

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

LIBC_API
int
getgrouplist (
    const char *UserName,
    gid_t GroupId,
    gid_t *Groups,
    int *GroupCount
    );

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

LIBC_API
int
initgroups (
    const char *User,
    gid_t Group
    );

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

LIBC_API
int
setgroups (
    size_t ElementCount,
    const gid_t *GroupList
    );

/*++

Routine Description:

    This routine sets the supplementary group membership of the calling
    process. The caller must have sufficient privileges to set supplementary
    group IDs.

Arguments:

    ElementCount - Supplies the size (in elements) of the supplied group list
        buffer.

    GroupList - Supplies an array of supplementary group IDs.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

