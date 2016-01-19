/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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
#include <sys/param.h>
#include <sys/stat.h>
#include <utmp.h>
#include <utmpx.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the permissions used when creating a user accounting database.
//

#define UTMPX_CREATE_PERMISSIONS (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ClpInitializeUserAccountingDatabase (
    PSTR DatabaseFile,
    const struct utmpx *Value
    );

INT
ClpAddUserAccountingEntry (
    const struct utmpx *Value
    );

INT
ClpRemoveUserAccountingEntry (
    const struct utmpx *Id
    );

INT
ClpOpenUserAccountingDatabase (
    PSTR DatabaseFile
    );

FILE *
ClpOpenUserAccountingDatabaseForWrite (
    PSTR DatabaseFile
    );

INT
ClpGetUserAccountingEntry (
    struct utmpx *Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the file pointer to the user accounting database.
//

char *ClUserAccountingFilePath = NULL;
FILE *ClUserAccountingFile = NULL;
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
int
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

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return updwtmpx(FileName, (struct utmpx *)Record);
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

    if (ClUserAccountingFile != NULL) {
        fclose(ClUserAccountingFile);
        ClUserAccountingFile = NULL;
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

    Result = ClpGetUserAccountingEntry(NULL);
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

    struct utmpx Value;

    while (TRUE) {
        if (ClpGetUserAccountingEntry(&Value) < 0) {
            return NULL;
        }

        switch (Value.ut_type) {
        case USER_PROCESS:
        case INIT_PROCESS:
        case LOGIN_PROCESS:
        case DEAD_PROCESS:
            switch (Id->ut_type) {
            case USER_PROCESS:
            case INIT_PROCESS:
            case LOGIN_PROCESS:
            case DEAD_PROCESS:
                if (memcmp(Id->ut_id, Value.ut_id, sizeof(Value.ut_id)) == 0) {
                    goto getutxidEnd;
                }

                break;

            default:
                break;
            }

            break;

        default:
            if (Value.ut_type== Id->ut_type) {
                goto getutxidEnd;
            }

            break;
        }
    }

getutxidEnd:
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
        if (ClpGetUserAccountingEntry(&Value) < 0) {
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
        if (ClpGetUserAccountingEntry(&Value) < 0) {
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

    INT Result;

    switch (Value->ut_type) {
    case BOOT_TIME:
        Result = ClpInitializeUserAccountingDatabase(NULL, Value);
        break;

    case OLD_TIME:
    case NEW_TIME:
    case USER_PROCESS:
    case INIT_PROCESS:
    case LOGIN_PROCESS:
        Result = ClpAddUserAccountingEntry(Value);
        break;

    case DEAD_PROCESS:
        Result = ClpRemoveUserAccountingEntry(Value);
        break;

    default:
        errno = EINVAL;
        Result = -1;
        break;
    }

    if (Result != 0) {
        return NULL;
    }

    //
    // Allocate the static storage if necesary.
    //

    if (ClUserAccountingEntry == NULL) {
        ClUserAccountingEntry = malloc(sizeof(struct utmpx));
        if (ClUserAccountingEntry == NULL) {
            errno = ENOMEM;
            return NULL;
        }
    }

    memcpy(ClUserAccountingEntry, Value, sizeof(struct utmpx));
    return ClUserAccountingEntry;
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
int
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

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    int Descriptor;
    ssize_t Written;

    Descriptor = open(FileName, O_WRONLY | O_APPEND, 0);
    if (Descriptor < 0) {
        return -1;
    }

    do {
        Written = write(Descriptor, Record, sizeof(struct utmpx));

    } while ((Written <= 0) && (errno == EINTR));

    if (Written != sizeof(struct utmpx)) {
        close(Descriptor);
        return -1;
    }

    return close(Descriptor);
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
ClpInitializeUserAccountingDatabase (
    PSTR DatabaseFile,
    const struct utmpx *Value
    )

/*++

Routine Description:

    This routine truncates the user accounting database and adds the given
    entry.

Arguments:

    DatabaseFile - Supplies an optional pointer to the database file.

    Value - Supplies a pointer to the entry to add.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    int Descriptor;

    if (DatabaseFile == NULL) {
        DatabaseFile = UTMPX_FILE;
    }

    //
    // Open the file and truncate it.
    //

    Descriptor = open(DatabaseFile,
                      O_CREAT | O_RDWR | O_TRUNC,
                      UTMPX_CREATE_PERMISSIONS);

    if (Descriptor < 0) {
        return -1;
    }

    write(Descriptor, Value, sizeof(*Value));
    close(Descriptor);
    return 0;
}

INT
ClpAddUserAccountingEntry (
    const struct utmpx *Value
    )

/*++

Routine Description:

    This routine adds a new utmpx entry to the user accounting database.

Arguments:

    Value - Supplies a pointer to the entry to add.

Return Value:

    0 on success.

    -1 on failure, and errno is set to contain more information.

--*/

{

    BOOL BreakOut;
    struct utmpx Entry;
    FILE *File;
    int Result;
    off_t SecondBest;

    File = ClpOpenUserAccountingDatabaseForWrite(NULL);
    if (File == NULL) {
        return -1;
    }

    BreakOut = FALSE;
    Result = 0;
    SecondBest = -1;
    while ((BreakOut == FALSE) &&
           (fread(&Entry, sizeof(Entry), 1, File) == 1)) {

        switch (Entry.ut_type) {

        //
        // Leave some records alone.
        //

        case BOOT_TIME:
        case NEW_TIME:
        case OLD_TIME:
        case RUN_LVL:
            break;

        case USER_PROCESS:
        case INIT_PROCESS:
        case LOGIN_PROCESS:
        case DEAD_PROCESS:

            //
            // Overwrite an entry if the ID matches.
            //

            if (memcmp(Value->ut_id, Entry.ut_id, sizeof(Entry.ut_id)) == 0) {
                Result = fseeko(File, -(off_t)sizeof(struct utmpx), SEEK_CUR);
                if (Result < 0) {
                    goto AddUtxEntryEnd;
                }

                //
                // Clear out the second best so that the entry is definitely
                // written here and be sure the exit the loop.
                //

                SecondBest = -1;
                BreakOut = TRUE;
                break;
            }

            //
            // Fall through.
            //

        case EMPTY:
        default:
            if (SecondBest == -1) {
                SecondBest = ftello(File);
                if (SecondBest >= sizeof(struct utmpx)) {
                    SecondBest -= sizeof(struct utmpx);

                } else {
                    SecondBest = -1;
                }
            }

            break;
        }
    }

    //
    // The file is either at the end, or is at the right spot. If there's a
    // second best option, use that. (If there was a best option, it cleared
    // out the second best option).
    //

    if (SecondBest >= 0) {
        Result = fseeko(File, SecondBest, SEEK_SET);
        if (Result < 0) {
            goto AddUtxEntryEnd;
        }
    }

    if (fwrite(Value, sizeof(*Value), 1, File) != 1) {
        Result = -1;
        goto AddUtxEntryEnd;
    }

    Result = 0;

AddUtxEntryEnd:
    if (File != NULL) {
        fclose(File);
    }

    return Result;
}

INT
ClpRemoveUserAccountingEntry (
    const struct utmpx *Id
    )

/*++

Routine Description:

    This routine removes a user login session that matches ut_id, and writes
    the given entry over it.

Arguments:

    Id - Supplies a pointer to the utmpx structure, which contains the ut_id
        to match and the value to overwrite.

Return Value:

    0 on success.

    -1 on failure, and errno is set to contain more information.

--*/

{

    INT Error;
    FILE *File;
    INT Result;
    struct utmpx Value;

    File = ClpOpenUserAccountingDatabaseForWrite(NULL);
    if (File == NULL) {
        return -1;
    }

    Error = ESRCH;
    Result = -1;
    while ((fread(&Value, sizeof(Value), 1, File) == 1) && (Result != 0)) {
        switch (Value.ut_type) {
        case USER_PROCESS:
        case INIT_PROCESS:
        case LOGIN_PROCESS:
            if (memcmp(Value.ut_id, Id->ut_id, sizeof(Value.ut_id)) != 0) {
                break;
            }

            if (fseeko(File, -(off_t)sizeof(Value), SEEK_CUR) < 0) {
                Error = errno;
                break;
            }

            if (fwrite(Id, sizeof(*Id), 1, File) != 1) {
                Error = errno;
                break;
            }

            Result = 0;
            break;

        default:
            break;
        }
    }

    if (File != NULL) {
        fclose(File);
    }

    if (Result != 0) {
        errno = Error;
    }

    return Result;
}

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

    if (ClUserAccountingFile != NULL) {
        endutxent();
    }

    assert(ClUserAccountingFile == NULL);

    ClUserAccountingFile = fopen(DatabaseFile, "re");
    if (ClUserAccountingFile == NULL) {
        return -1;
    }

    //
    // Avoid reading partial records.
    //

    setvbuf(ClUserAccountingFile,
            NULL,
            _IOFBF,
            rounddown(BUFSIZ, sizeof(struct utmpx)));

    return 0;
}

FILE *
ClpOpenUserAccountingDatabaseForWrite (
    PSTR DatabaseFile
    )

/*++

Routine Description:

    This routine opens the user accounting database file for write access.

Arguments:

    DatabaseFile - Supplies an optional pointer to a string containing the
        path to open.

Return Value:

    Returns an open file stream on success.

    NULL on failure.

--*/

{

    int Descriptor;
    FILE *File;

    if (DatabaseFile == NULL) {
        DatabaseFile = UTMPX_FILE;
    }

    Descriptor = open(DatabaseFile,
                      O_CREAT | O_RDWR,
                      UTMPX_CREATE_PERMISSIONS);

    if (Descriptor < 0) {
        return NULL;
    }

    File = fdopen(Descriptor, "r+");
    if (File == NULL) {
        close(Descriptor);
        return NULL;
    }

    return File;
}

INT
ClpGetUserAccountingEntry (
    struct utmpx *Value
    )

/*++

Routine Description:

    This routine reads the next entry out of the user accounting database.

Arguments:

    Value - Supplies an optional pointer where the entry will be returned on
        success. If this is NULL, the static area will be used.

Return Value:

    0 on success.

    -1 on failure, and errno is set to contain more information.

--*/

{

    INT Result;

    //
    // Allocate the static space if necessary.
    //

    if (ClUserAccountingEntry == NULL) {
        ClUserAccountingEntry = malloc(sizeof(struct utmpx));
        if (ClUserAccountingEntry == NULL) {
            errno = ENOMEM;
            return -1;
        }
    }

    if (Value == NULL) {
        Value = ClUserAccountingEntry;
    }

    //
    // Open the database if necessary.
    //

    if (ClUserAccountingFile == NULL) {
        Result = ClpOpenUserAccountingDatabase(ClUserAccountingFilePath);
        if (Result < 0) {
            return Result;
        }
    }

    if (fread(Value, sizeof(*Value), 1, ClUserAccountingFile) != 1) {
        return -1;
    }

    return 0;
}

