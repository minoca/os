/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    utmp.h

Abstract:

    This header contains older definitions for the user accounting database.

Author:

    Evan Green 4-Mar-2015

--*/

#ifndef _UTMP_H
#define _UTMP_H

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
// Define file paths.
//

#define UTMP_FILE _PATH_UTMP
#define UTMP_FILENAME _PATH_UTMP
#define WTMP_FILE _PATH_WTMP
#define WTMP_FILENAME _PATH_WTMP

//
// Define the sizes of various arrays.
//

#define UT_LINESIZE 32
#define UT_NAMESIZE 32
#define UT_HOSTSIZE 256

//
// Define values for the type field of a utmp structure. Note that these are
// the same values as are in utmpx.h.
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
// Define some compatibility member names of the utmp structure.
//

#define ut_name ut_user
#define ut_time ut_tv.tv_sec
#define ut_xtime ut_tv.tv_sec
#define ut_addr ut_addr_v6[0]

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the format of the database that stores entries of
    previous logins.

Members:

    ll_time - Stores the time of last login.

    ll_line - Stores the terminal the login occurred under.

    ll_host - Stores the host name that last logged in.

--*/

struct lastlog {
    time_t ll_time;
    char ll_line[UT_LINESIZE];
    char ll_host[UT_HOSTSIZE];
};

/*++

Structure Description:

    This structure defines the exit status code in a utmp structure.

Members:

    e_termination - Stores the process termination status.

    e_exit - Stores the process exit status.

--*/

struct exit_status {
    short int e_termination;
    short int e_exit;
};

/*++

Structure Description:

    This structure defines the format of the user accounting database entries.
    Note that this is exactly the same as the utmpx structure.

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

struct utmp {
    short int ut_type;
    pid_t ut_pid;
    char ut_line[UT_LINESIZE];
    char ut_id[4];
    char ut_user[UT_NAMESIZE];
    char ut_host[UT_HOSTSIZE];
    struct exit_status ut_exit;
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
setutent (
    void
    );

/*++

Routine Description:

    This routine resets the current pointer into the user database back to the
    beginning. This function is neither thread-safe nor reentrant. This
    function is equivalent to setutxent, and new applications should use that
    function.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
void
endutent (
    void
    );

/*++

Routine Description:

    This routine closes the user accounting database. This function is neither
    thread-safe nor reentrant. This function is equivalent to endutxent, and
    new applications should use that function.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
struct utmp *
getutent (
    void
    );

/*++

Routine Description:

    This routine returns the next entry in the user accounting database. If
    the database is not already open, it will open it. If it reaches the end
    of the database, it fails. This function is neither thread-safe nor
    reentrant. Since utmp and utmpx structures are the same, this function is
    equivalent to getutxent, and new applications should use that function.

Arguments:

    None.

Return Value:

    Returns a pointer to a copy of the user accounting information on success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
struct utmp *
getutid (
    const struct utmp *Id
    );

/*++

Routine Description:

    This routine searches forward from the current point in the user accounting
    database. If the ut_type value of the supplied utmp structure is
    BOOT_TIME, OLD_TIME, or NEW_TIME, then it stops when it finds an entry with
    a matching ut_type value. If the ut_type is INIT_PROCESS, USER_PROCESS, or
    DEAD_PROCESS, it stops when it finds an entry whose type is one of these
    four and whose ut_id matches the one in the given structure. If the end of
    the database is reached without a match, the routine shall fail. This
    function is neither thread-safe nor reentrant. Since utmp and utmpx
    structures are the same, this function is equivalent to getutxent, and new
    applications should use that function.

Arguments:

    Id - Supplies a pointer to a structure containing the type and possibly
        user ID to match on.

Return Value:

    Returns a pointer to a copy of the user accounting information on success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
struct utmp *
getutline (
    const struct utmp *Line
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

    Since utmp and utmpx structures are the same, this function is equivalent
    to getutxline, and new applications should use that function.

Arguments:

    Line - Supplies a pointer to a structure containing the line to match.

Return Value:

    Returns a pointer to a copy of the user accounting information on success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
struct utmp *
pututline (
    const struct utmp *Value
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
    neither thread-safe nor reentrant. Since utmp and utmpx structures are the
    same, this function is equivalent to getutxline, and new applications
    should use that function.

Arguments:

    Value - Supplies a pointer to a structure containing the new data.

Return Value:

    Returns a pointer to a copy of the written user accounting information on
    success.

    NULL on failure, and errno may be set on error.

--*/

LIBC_API
int
utmpname (
    const char *FilePath
    );

/*++

Routine Description:

    This routine updates the file path that utmp* functions open and access.
    This must be called before those routines open the file. This routine does
    not check to ensure the file exists. This routine is neither thread-safe
    nor reentrant. This routine is equivalent to utmpxname, and new
    applications should call that function.

Arguments:

    FilePath - Supplies a pointer to the new file path. A copy of this string
        will be made.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
void
logwtmp (
    const char *Terminal,
    const char *User,
    const char *Host
    );

/*++

Routine Description:

    This routine creates a new utmp entry with the given terminal line, user
    name, host name, the current process ID, and current time. It appends the
    new record using updwtmp to the wtmp file.

Arguments:

    Terminal - Supplies an optional pointer to the terminal.

    User - Supplies an optional pointer to the user.

    Host - Supplies a pointer to the host.

Return Value:

    None.

--*/

LIBC_API
void
updwtmp (
    const char *FileName,
    const struct utmp *Record
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
int
login_tty (
    int TerminalDescriptor
    );

/*++

Routine Description:

    This routine prepares for a login on the given terminal. It creates a new
    session, makes the given terminal descriptor the controlling terminal for
    the session, sets the terminal as standard input, output, and error, and
    closes the given descriptor.

Arguments:

    TerminalDescriptor - Supplies the file descriptor of the terminal to start
        a login on.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

