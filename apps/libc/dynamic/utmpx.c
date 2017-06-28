/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    utmpx.c

Abstract:

    This module implements support for the user accounting database, which
    tracks user logins and other activity.

Author:

    Evan Green 30-Jan-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <utmp.h>
#include <utmpx.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ClpOpenUserAccountingDatabase (
    PSTR DatabaseFile
    );

INT
ClpReadWriteUserAccountingEntry (
    struct utmpx *Entry,
    INT Type
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the file pointer to the user accounting database.
//

char *ClUserAccountingFilePath = NULL;
int ClUserAccountingFile = -1;
struct utmpx *ClUserAccountingEntry = NULL;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void
setutent (
    void
    )

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

{

    setutxent();
    return;
}

LIBC_API
void
endutent (
    void
    )

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

{

    endutxent();
    return;
}

LIBC_API
struct utmp *
getutent (
    void
    )

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

{

    return (struct utmp *)getutxent();
}

LIBC_API
struct utmp *
getutid (
    const struct utmp *Id
    )

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

{

    return (struct utmp *)getutxid((struct utmpx *)Id);
}

LIBC_API
struct utmp *
getutline (
    const struct utmp *Line
    )

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

{

    return (struct utmp *)getutxline((struct utmpx *)Line);
}

LIBC_API
struct utmp *
pututline (
    const struct utmp *Value
    )

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

{

    return (struct utmp *)pututxline((struct utmpx *)Value);
}

LIBC_API
int
utmpname (
    const char *FilePath
    )

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

{

    return utmpxname(FilePath);
}

LIBC_API
void
logwtmp (
    const char *Terminal,
    const char *User,
    const char *Host
    )

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

{

    struct utmp Record;

    memset(&Record, 0, sizeof(Record));
    Record.ut_pid = getpid();
    if ((User != NULL) && (User[0] != '\0')) {
        Record.ut_type = USER_PROCESS;
        strncpy(Record.ut_user, User, sizeof(Record.ut_user));

    } else {
        Record.ut_type = DEAD_PROCESS;
    }

    if (Terminal != NULL) {
        strncpy(Record.ut_line, Terminal, sizeof(Record.ut_line));
    }

    strncpy(Record.ut_host, Host, sizeof(Record.ut_host));
    gettimeofday(&(Record.ut_tv), NULL);
    updwtmp(_PATH_WTMP, (struct utmp *)&Record);
    return;
}

LIBC_API
void
updwtmp (
    const char *FileName,
    const struct utmp *Record
    )

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

{

    updwtmpx(FileName, (struct utmpx *)Record);
    return;
}

LIBC_API
void
setutxent (
    void
    )

/*++

Routine Description:

    This routine resets the current pointer into the user database back to the
    beginning. This function is neither thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Allocate the static space if necessary.
    //

    if (ClUserAccountingEntry == NULL) {
        ClUserAccountingEntry = malloc(sizeof(struct utmpx));
        if (ClUserAccountingEntry == NULL) {
            errno = ENOMEM;
            return;
        }
    }

    ClpOpenUserAccountingDatabase(ClUserAccountingFilePath);
    return;
}

LIBC_API
void
endutxent (
    void
    )

/*++

Routine Description:

    This routine closes the user accounting database. This function is neither
    thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClUserAccountingFile >= 0) {
        close(ClUserAccountingFile);
        ClUserAccountingFile = -1;
    }

    return;
}

LIBC_API
struct utmpx *
getutxent (
    void
    )

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

{

    INT Result;

    Result = ClpReadWriteUserAccountingEntry(NULL, F_RDLCK);
    if (Result < 0) {
        return NULL;
    }

    return ClUserAccountingEntry;
}

LIBC_API
struct utmpx *
getutxid (
    const struct utmpx *Id
    )

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

{

    BOOL Match;
    struct utmpx Value;

    Match = FALSE;
    while (TRUE) {
        if (ClpReadWriteUserAccountingEntry(&Value, F_RDLCK) < 0) {
            return NULL;
        }

        //
        // If it's any of the one-time entries (RUN_LVL, BOOT_TIME, NEW_TIME,
        // or OLD_TIME) just match on the type.
        //

        if ((Id->ut_type != EMPTY) && (Id->ut_type <= OLD_TIME)) {
            if (Id->ut_type == Value.ut_type) {
                Match = TRUE;
                break;
            }

        //
        // If it's a process entry (INIT_PROCESS, LOGIN_PROCESS, USER_PROCESS,
        // or DEAD_PROCESS) then find one that matches the ID.
        //

        } else if (Id->ut_type <= DEAD_PROCESS) {
            if (strncmp(Id->ut_id, Value.ut_id, sizeof(Value.ut_id)) == 0) {
                Match = TRUE;
                break;
            }
        }
    }

    if (Match == FALSE) {
        return NULL;
    }

    memcpy(ClUserAccountingEntry, &Value, sizeof(struct utmpx));
    return ClUserAccountingEntry;
}

LIBC_API
struct utmpx *
getutxline (
    const struct utmpx *Line
    )

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

{

    struct utmpx Value;

    while (TRUE) {
        if (ClpReadWriteUserAccountingEntry(&Value, F_RDLCK) < 0) {
            return NULL;
        }

        if ((Value.ut_type == USER_PROCESS) ||
            (Value.ut_type == LOGIN_PROCESS)) {

            if (strncmp(Value.ut_line, Line->ut_line, sizeof(Value.ut_line)) ==
                0) {

                goto getutxlineEnd;
            }
        }
    }

getutxlineEnd:
    memcpy(ClUserAccountingEntry, &Value, sizeof(struct utmpx));
    return ClUserAccountingEntry;
}

LIBC_API
struct utmpx *
getutxuser (
    const struct utmpx *User
    )

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

{

    struct utmpx Value;

    while (TRUE) {
        if (ClpReadWriteUserAccountingEntry(&Value, F_RDLCK) < 0) {
            return NULL;
        }

        if (Value.ut_type == LOGIN_PROCESS) {
            if (strncmp(Value.ut_user, User->ut_user, sizeof(Value.ut_user)) ==
                0) {

                goto getutxuserEnd;
            }
        }
    }

getutxuserEnd:
    memcpy(ClUserAccountingEntry, &Value, sizeof(struct utmpx));
    return ClUserAccountingEntry;
}

LIBC_API
struct utmpx *
pututxline (
    const struct utmpx *Value
    )

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

{

    struct utmpx Copy;
    struct utmpx *Found;
    INT Result;

    //
    // Copy the passed in value in case it is the static storage.
    //

    memcpy(&Copy, Value, sizeof(struct utmpx));

    //
    // Find the entry.
    //

    Found = getutxid(&Copy);
    if (Found != NULL) {
        lseek(ClUserAccountingFile, -(off_t)sizeof(struct utmpx), SEEK_CUR);

    } else {
        lseek(ClUserAccountingFile, 0, SEEK_END);
    }

    Result = ClpReadWriteUserAccountingEntry(&Copy, F_WRLCK);
    if (Result < 0) {
        return NULL;
    }

    memcpy(ClUserAccountingEntry, &Copy, sizeof(struct utmpx));
    return (struct utmpx *)Value;
}

LIBC_API
int
utmpxname (
    const char *FilePath
    )

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

{

    if (ClUserAccountingFilePath != NULL) {
        free(ClUserAccountingFilePath);
        ClUserAccountingFilePath = NULL;
    }

    if (FilePath == NULL) {
        return 0;
    }

    ClUserAccountingFilePath = strdup(FilePath);
    if (ClUserAccountingFilePath != NULL) {
        return 0;
    }

    return -1;
}

LIBC_API
void
updwtmpx (
    const char *FileName,
    const struct utmpx *Record
    )

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

{

    int Descriptor;
    ssize_t Written;

    Descriptor = open(FileName, O_WRONLY | O_APPEND, 0);
    if (Descriptor < 0) {
        return;
    }

    do {
        Written = write(Descriptor, Record, sizeof(struct utmpx));

    } while ((Written <= 0) && (errno == EINTR));

    close(Descriptor);
    return;
}

LIBC_API
void
getutmpx (
    const struct utmp *ValueToConvert,
    struct utmpx *ConvertedValue
    )

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

{

    assert(sizeof(struct utmp) == sizeof(struct utmpx));

    memcpy(ConvertedValue, ValueToConvert, sizeof(struct utmpx));
    return;
}

LIBC_API
void
getutmp (
    const struct utmpx *ValueToConvert,
    struct utmp *ConvertedValue
    )

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

{

    assert(sizeof(struct utmp) == sizeof(struct utmpx));

    memcpy(ConvertedValue, ValueToConvert, sizeof(struct utmp));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ClpOpenUserAccountingDatabase (
    PSTR DatabaseFile
    )

/*++

Routine Description:

    This routine opens the user accounting database file.

Arguments:

    DatabaseFile - Supplies an optional pointer to a string containing the
        path to open.

Return Value:

    0 on success.

    -1 on failure, and errno is set to contain more information.

--*/

{

    if (DatabaseFile == NULL) {
        DatabaseFile = UTMPX_FILE;
    }

    if (ClUserAccountingFile >= 0) {
        endutxent();
    }

    assert(ClUserAccountingFile == -1);

    ClUserAccountingFile = open(DatabaseFile, O_RDWR);
    if (ClUserAccountingFile < 0) {
        ClUserAccountingFile = open(DatabaseFile, O_RDONLY);
    }

    if (ClUserAccountingFile < 0) {
        return -1;
    }

    return 0;
}

INT
ClpReadWriteUserAccountingEntry (
    struct utmpx *Entry,
    INT Type
    )

/*++

Routine Description:

    This routine reads from or writes to a user accounting database. It uses
    voluntary file locking to achieve synchronization.

Arguments:

    Entry - Supplies a pointer to the entry to read or write.

    Type - Supplies the file locking operation to perform. Valid values are
        F_WRLCK or F_RDLCK.

Return Value:

    0 on success.

    -1 on failure, and errno is set to contain more information.

--*/

{

    ssize_t BytesDone;
    struct flock Lock;
    off_t Offset;

    if (Entry == NULL) {
        Entry = ClUserAccountingEntry;
    }

    //
    // Save the previous offset in case it has to be restored due to a partial
    // read or write.
    //

    Offset = lseek(ClUserAccountingFile, 0, SEEK_CUR);

    //
    // Lock the region of interest in the file.
    //

    Lock.l_start = Offset;
    Lock.l_len = sizeof(struct utmpx);
    Lock.l_pid = 0;
    Lock.l_type = Type;
    Lock.l_whence = SEEK_SET;
    if (fcntl(ClUserAccountingFile, F_SETLKW, &Lock) != 0) {
        return -1;
    }

    do {
        if (Type == F_WRLCK) {
            BytesDone = write(ClUserAccountingFile,
                              Entry,
                              sizeof(struct utmpx));

        } else {

            assert(Type == F_RDLCK);

            BytesDone = read(ClUserAccountingFile, Entry, sizeof(struct utmpx));
        }

    } while ((BytesDone < 0) && (errno == EINTR));

    //
    // Unlock the file.
    //

    Lock.l_type = F_UNLCK;
    fcntl(ClUserAccountingFile, F_SETLK, &Lock);
    if (BytesDone != sizeof(struct utmpx)) {
        lseek(ClUserAccountingFile, Offset, SEEK_SET);
        return -1;
    }

    return 0;
}

