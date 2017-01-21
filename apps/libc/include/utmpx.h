/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    utmpx.h

Abstract:

    This header contains definitions for the user accounting database.

Author:

    Evan Green 23-Jan-2015

--*/

#ifndef _UTMPX_H
#define _UTMPX_H

//
// ------------------------------------------------------------------- Includes
//

#include <paths.h>
#include <sys/time.h>
#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the path to the utmpx file.
//

#define UTMPX_FILE _PATH_UTMPX

//
// Define values for the type field of a utmpx structure. Note that these are
// the same values as are in utmp.h.
//

//
// Empty: No valid user accounting information.
//

#define EMPTY 0

//
// Identifies a change in system run-level
//

#define RUN_LVL 1

//
// Identifies the time of system boot
//

#define BOOT_TIME 2

//
// Identifies time after the system clock changed
//

#define NEW_TIME 3

//
// Identifies time when the system clock changed
//

#define OLD_TIME 4

//
// Identifies a process spawned by the init process
//

#define INIT_PROCESS 5

//
// Identifies the session leader of a logged in user
//

#define LOGIN_PROCESS 6

//
// Identifies a normal user process
//

#define USER_PROCESS 7

//
// Identifies a session leader who has exited.
//

#define DEAD_PROCESS 8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// This definition is needed by the conversion functions.
//

struct utmp;

/*++

Structure Description:

    This structure defines the exit status code in a utmpx structure.

Members:

    e_termination - Stores the process termination status.

    e_exit - Stores the process exit status.

--*/

struct __exit_status {
    short int e_termination;
    short int e_exit;
};

/*++

Structure Description:

    This structure defines the format of the user accounting database entries.
    Note that this is exactly the same as the utmp structure.

Members:

    ut_type - Stores the type of entry.

    ut_pid - Stores the process ID of the entry.

    ut_line - Stores the device name.

    ut_id - Stores the inittab ID.

    ut_user - Stores the user name.

    ut_host - Stores the host name.

    ut_exit - Stores the process exit status.

    ut_session - Stores the session ID.

    ut_tv - Stores the timestamp of the entry.

    ut_addr_v6 - Stores the Internet address of the remote host.

--*/

struct utmpx {
    short int ut_type;
    pid_t ut_pid;
    char ut_line[32];
    char ut_id[4];
    char ut_user[32];
    char ut_host[256];
    struct __exit_status ut_exit;
    long int ut_session;
    struct timeval ut_tv;
    int32_t ut_addr_v6[4];
    char __ut_reserved[32];
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
void
setutxent (
    void
    );

/*++

Routine Description:

    This routine resets the current pointer into the user database back to the
    beginning. This function is neither thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
void
endutxent (
    void
    );

/*++

Routine Description:

    This routine closes the user accounting database. This function is neither
    thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
struct utmpx *
getutxent (
    void
    );

/*++

Routine Description:

    This routine returns the next entry in the user accounting database. If
    the database is not already open, it will open it. If it reaches the end
    of the database, it fails. This function is neither thread-safe nor
    reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to a copy of the user accounting information on success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
struct utmpx *
getutxid (
    const struct utmpx *Id
    );

/*++

Routine Description:

    This routine searches forward from the current point in the user accounting
    database. If the ut_type value of the supplied utmpx structure is
    BOOT_TIME, OLD_TIME, or NEW_TIME, then it stops when it finds an entry with
    a matching ut_type value. If the ut_type is INIT_PROCESS, USER_PROCESS, or
    DEAD_PROCESS, it stops when it finds an entry whose type is one of these
    four and whose ut_id matches the one in the given structure. If the end of
    the database is reached without a match, the routine shall fail. This
    function is neither thread-safe nor reentrant.

Arguments:

    Id - Supplies a pointer to a structure containing the type and possibly
        user ID to match on.

Return Value:

    Returns a pointer to a copy of the user accounting information on success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
struct utmpx *
getutxline (
    const struct utmpx *Line
    );

/*++

Routine Description:

    This routine searches forward from the current point in the user accounting
    database, looking for an entry of type LOGIN_PROCESS or USER_PROCESS which
    also matches the ut_line value in the given structure. If the end of the
    database is reached without a match, the routine shall fail. This function
    is neither thread-safe nor reentrant.

    This function may cache data, so to search for multiple occurrences it is
    important to zero out the static data (the return value from the previous
    result). Otherwise, the same result may be returned infinitely.

Arguments:

    Line - Supplies a pointer to a structure containing the line to match.

Return Value:

    Returns a pointer to a copy of the user accounting information on success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
struct utmpx *
getutxuser (
    const struct utmpx *User
    );

/*++

Routine Description:

    This routine searches forward from the current point in the user accounting
    database, looking for an entry of type USER_PROCESS which also matches the
    ut_user value in the given structure. If the end of the database is reached
    without a match, the routine shall fail. This function is neither
    thread-safe nor reentrant.

    This function may cache data, so to search for multiple occurrences it is
    important to zero out the static data (the return value from the previous
    result). Otherwise, the same result may be returned infinitely.

Arguments:

    User - Supplies a pointer to a structure containing the userto match.

Return Value:

    Returns a pointer to a copy of the user accounting information on success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
struct utmpx *
pututxline (
    const struct utmpx *Value
    );

/*++

Routine Description:

    This routine writes out the structure to the user accounting database. It
    shall use getutxid to search for a record that satisfies the request. If
    the search succeeds, then the entry will be replaced. Otherwise, a new
    entry is made at the end of the user accounting database. The caller must
    have sufficient privileges. The implicit read done by this function if it
    finds it is not already at the correct place shall not modify the static
    structure passed as a return of the other utx functions (so the application
    may use that space to write back a modified value). This function is
    neither thread-safe nor reentrant.

Arguments:

    Value - Supplies a pointer to a structure containing the new data.

Return Value:

    Returns a pointer to a copy of the written user accounting information on
    success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
int
utmpxname (
    const char *FilePath
    );

/*++

Routine Description:

    This routine updates the file path that utmpx* functions open and access.
    This must be called before those routines open the file. This routine does
    not check to ensure the file exists. This routine is neither thread-safe
    nor reentrant.

Arguments:

    FilePath - Supplies a pointer to the new file path. A copy of this string
        will be made.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
void
updwtmpx (
    const char *FileName,
    const struct utmpx *Record
    );

/*++

Routine Description:

    This routine adds an entry into the wtmp user database.

Arguments:

    FileName - Supplies a pointer to path of the wtmp file to open. Set this to
        WTMP_FILE by default.

    Record - Supplies a pointer to the record to append.

Return Value:

    None.

--*/

LIBC_API
void
getutmpx (
    const struct utmp *ValueToConvert,
    struct utmpx *ConvertedValue
    );

/*++

Routine Description:

    This routine converts a utmp structure into a utmpx structure. Since the
    structures are exactly the same, this is just a straight copy.

Arguments:

    ValueToConvert - Supplies a pointer to the utmp structure to convert.

    ConvertedValue - Supplies a pointer where the converted utmpx strucutre
        will be returned.

Return Value:

    None.

--*/

LIBC_API
void
getutmp (
    const struct utmpx *ValueToConvert,
    struct utmp *ConvertedValue
    );

/*++

Routine Description:

    This routine converts a utmpx structure into a utmp structure. Since the
    structures are exactly the same, this is just a straight copy.

Arguments:

    ValueToConvert - Supplies a pointer to the utmpx structure to convert.

    ConvertedValue - Supplies a pointer where the converted utmp strucutre
        will be returned.

Return Value:

    None.

--*/

#ifdef __cplusplus

}

#endif
#endif

