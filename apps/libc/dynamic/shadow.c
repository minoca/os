/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    shadow.c

Abstract:

    This module implements support for manipulating the shadow password file.
    Generally applications need to have special privileges for these functions
    to succeed.

Author:

    Evan Green 3-Mar-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <fcntl.h>
#include <shadow.h>
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the path to the shadow lock file.
//

#define SHADOW_LOCK_PATH "/etc/.pwd.lock"

//
// Define how long to wait to acquire the lock in seconds.
//

#define SHADOW_LOCK_TIMEOUT 15

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void
ClpEmptySignalHandler (
    int Signal
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the shadow password file.
//

FILE *ClShadowFile;

//
// Store a pointer to the shared shadow information structure. There is always
// a buffer after this structure that is the maximum password file line size.
//

struct spwd *ClShadowInformation;

//
// Store the file descriptor for the locked shadow password file.
//

int ClShadowLockDescriptor = -1;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
struct spwd *
getspnam (
    const char *UserName
    )

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

{

    int Result;
    struct spwd *ResultPointer;

    if (ClShadowInformation == NULL) {
        ClShadowInformation = malloc(
                                 sizeof(struct spwd) + USER_DATABASE_LINE_MAX);

        if (ClShadowInformation == NULL) {
            return NULL;
        }
    }

    ResultPointer = NULL;
    Result = getspnam_r(UserName,
                        ClShadowInformation,
                        (char *)(ClShadowInformation + 1),
                        USER_DATABASE_LINE_MAX,
                        &ResultPointer);

    if (Result != 0) {
        return NULL;
    }

    return ResultPointer;
}

LIBC_API
int
getspnam_r (
    const char *UserName,
    struct spwd *PasswordInformation,
    char *Buffer,
    size_t BufferSize,
    struct spwd **Result
    )

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

{

    FILE *File;
    struct spwd Information;
    int Status;

    *Result = NULL;
    File = fopen(_PATH_SHADOW, "r");
    if (File == NULL) {
        return errno;
    }

    //
    // Loop through looking for an entry that matches.
    //

    while (TRUE) {
        Status = fgetspent_r(File, &Information, Buffer, BufferSize, Result);
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

        if (strcmp(Information.sp_namp, UserName) == 0) {
            memcpy(PasswordInformation, &Information, sizeof(Information));
            *Result = PasswordInformation;
            break;
        }
    }

    if (File != NULL) {
        fclose(File);
    }

    return Status;
}

LIBC_API
struct spwd *
getspent (
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
    calls to getspent.

    NULL if the end of the user database is reached or on error.

--*/

{

    int Result;
    struct spwd *ReturnPointer;

    if (ClShadowInformation == NULL) {
        ClShadowInformation = malloc(
                                 sizeof(struct spwd) + USER_DATABASE_LINE_MAX);

        if (ClShadowInformation == NULL) {
            return NULL;
        }
    }

    ReturnPointer = NULL;
    Result = getspent_r(ClShadowInformation,
                        (char *)(ClShadowInformation + 1),
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
getspent_r (
    struct spwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct spwd **ReturnPointer
    )

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

{

    int Result;

    if (ClShadowFile == NULL) {
        setspent();
    }

    if (ClShadowFile == NULL) {
        return errno;
    }

    Result = fgetspent_r(ClShadowFile,
                         Information,
                         Buffer,
                         BufferSize,
                         ReturnPointer);

    return Result;
}

LIBC_API
struct spwd *
fgetspent (
    FILE *File
    )

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

{

    int Result;
    struct spwd *ReturnPointer;

    if (ClShadowInformation == NULL) {
        ClShadowInformation = malloc(
                                 sizeof(struct spwd) + USER_DATABASE_LINE_MAX);

        if (ClShadowInformation == NULL) {
            return NULL;
        }
    }

    ReturnPointer = NULL;
    Result = fgetspent_r(File,
                         ClShadowInformation,
                         (char *)(ClShadowInformation + 1),
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
fgetspent_r (
    FILE *File,
    struct spwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct spwd **ReturnPointer
    )

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

{

    PSTR Current;
    CHAR Line[USER_DATABASE_LINE_MAX];
    PSTR OriginalBuffer;
    size_t OriginalBufferSize;
    int Result;

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

        Result = sgetspent_r(Line,
                             Information,
                             Buffer,
                             BufferSize,
                             ReturnPointer);

        if (Result == 0) {
            break;
        }
    }

    return 0;
}

LIBC_API
struct spwd *
sgetspent (
    const char *String
    )

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

{

    int Result;
    struct spwd *ReturnPointer;

    if (ClShadowInformation == NULL) {
        ClShadowInformation = malloc(
                                 sizeof(struct spwd) + USER_DATABASE_LINE_MAX);

        if (ClShadowInformation == NULL) {
            return NULL;
        }
    }

    ReturnPointer = NULL;
    Result = sgetspent_r(String,
                         ClShadowInformation,
                         (char *)(ClShadowInformation + 1),
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
sgetspent_r (
    const char *String,
    struct spwd *Information,
    char *Buffer,
    size_t BufferSize,
    struct spwd **ReturnPointer
    )

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

{

    char *AfterScan;
    const char *Current;

    //
    // Loop trying to scan a good line.
    //

    *ReturnPointer = NULL;

    //
    // Skip any spaces.
    //

    Current = String;
    while (isspace(*Current) != 0) {
        Current += 1;
    }

    //
    // Skip any empty or commented lines.
    //

    if ((*Current == '\0') || (*Current == '#')) {
        return EINVAL;
    }

    //
    // Grab the username. Skip malformed lines.
    //

    Information->sp_namp = Buffer;
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
        return EINVAL;
    }

    Current += 1;

    //
    // Grab the password.
    //

    Information->sp_pwdp = Buffer;
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
        return EINVAL;
    }

    Current += 1;

    //
    // Grab the last changed days.
    //

    Information->sp_lstchg = strtol(Current, &AfterScan, 10);
    if (AfterScan == Current) {
        Information->sp_lstchg = -1;
    }

    Current = AfterScan;
    if (*Current != ':') {
        return EINVAL;
    }

    Current += 1;

    //
    // Grab the number of days before the password may be changed.
    //

    Information->sp_min = strtoul(Current, &AfterScan, 10);
    if (AfterScan == Current) {
        Information->sp_min = -1;
    }

    Current = AfterScan;
    if (*Current != ':') {
        return EINVAL;
    }

    Current += 1;

    //
    // Grab the number of days before the password must be changed.
    //

    Information->sp_max = strtoul(Current, &AfterScan, 10);
    if (AfterScan == Current) {
        Information->sp_max = -1;
    }

    Current = AfterScan;
    if (*Current != ':') {
        return EINVAL;
    }

    Current += 1;

    //
    // Grab the number of days before warning that the password should be
    // changed.
    //

    Information->sp_warn = strtoul(Current, &AfterScan, 10);
    if (AfterScan == Current) {
        Information->sp_warn = -1;
    }

    Current = AfterScan;
    if (*Current != ':') {
        return EINVAL;
    }

    Current += 1;

    //
    // Grab the number of days after the password expires that the account is
    // disabled.
    //

    Information->sp_inact = strtoul(Current, &AfterScan, 10);
    if (AfterScan == Current) {
        Information->sp_inact = -1;
    }

    Current = AfterScan;
    if (*Current != ':') {
        return EINVAL;
    }

    Current += 1;

    //
    // Grab the number of days since 1970 that an account has been disabled.
    //

    Information->sp_expire = strtoul(Current, &AfterScan, 10);
    if (AfterScan == Current) {
        Information->sp_expire = -1;
    }

    Current = AfterScan;
    if (*Current != ':') {
        return EINVAL;
    }

    Current += 1;

    //
    // Grab the reserved flags.
    //

    Information->sp_flag = strtoul(Current, &AfterScan, 0);
    if (AfterScan == Current) {
        Information->sp_flag = -1;
    }

    *ReturnPointer = Information;
    return 0;
}

LIBC_API
void
setspent (
    void
    )

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

{

    if (ClShadowFile == NULL) {
        ClShadowFile = fopen(_PATH_SHADOW, "r");

    } else {
        fseek(ClShadowFile, 0, SEEK_SET);
    }

    return;
}

LIBC_API
void
endspent (
    void
    )

/*++

Routine Description:

    This routine closes an open handle to the user password database
    established with setspent or getspent.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClShadowFile != NULL) {
        fclose(ClShadowFile);
        ClShadowFile = NULL;
    }

    return;
}

LIBC_API
int
putspent (
    const struct spwd *Information,
    FILE *Stream
    )

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

{

    int FinalResult;

    flockfile(Stream);
    FinalResult = 0;
    if (fprintf(Stream, "%s:", Information->sp_namp) < 0) {
        FinalResult = -1;
    }

    if (Information->sp_pwdp != NULL) {
        if (fprintf(Stream, "%s:", Information->sp_pwdp) < 0) {
            FinalResult = -1;
        }

    } else {
        if (fputc(':', Stream) == EOF) {
            FinalResult = -1;
        }
    }

    if (Information->sp_min != (long int)-1) {
        if (fprintf(Stream, "%ld:", Information->sp_min) < 0) {
            FinalResult = -1;
        }

    } else {
        if (fputc(':', Stream) == EOF) {
            FinalResult = -1;
        }
    }

    if (Information->sp_max != (long int)-1) {
        if (fprintf(Stream, "%ld:", Information->sp_max) < 0) {
            FinalResult = -1;
        }

    } else {
        if (fputc(':', Stream) == EOF) {
            FinalResult = -1;
        }
    }

    if (Information->sp_warn != (long int)-1) {
        if (fprintf(Stream, "%ld:", Information->sp_warn) < 0) {
            FinalResult = -1;
        }

    } else {
        if (fputc(':', Stream) == EOF) {
            FinalResult = -1;
        }
    }

    if (Information->sp_inact != (long int)-1) {
        if (fprintf(Stream, "%ld:", Information->sp_inact) < 0) {
            FinalResult = -1;
        }

    } else {
        if (fputc(':', Stream) == EOF) {
            FinalResult = -1;
        }
    }

    if (Information->sp_expire != (long int)-1) {
        if (fprintf(Stream, "%ld:", Information->sp_expire) < 0) {
            FinalResult = -1;
        }

    } else {
        if (fputc(':', Stream) == EOF) {
            FinalResult = -1;
        }
    }

    if (Information->sp_flag != (unsigned long int)-1) {
        if (fprintf(Stream, "%lu", Information->sp_flag) < 0) {
            FinalResult = -1;
        }
    }

    if (fputc('\n', Stream) == EOF) {
        FinalResult = -1;
    }

    funlockfile(Stream);
    return FinalResult;
}

LIBC_API
int
lckpwdf (
    void
    )

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

{

    struct sigaction AlarmAction;
    BOOL AlarmActionSet;
    int Flags;
    struct flock Lock;
    sigset_t Mask;
    int OpenFlags;
    struct sigaction OriginalAlarmAction;
    sigset_t OriginalMask;
    int Result;

    AlarmActionSet = FALSE;

    //
    // If the lock is already held by this process, fail.
    //

    if (ClShadowLockDescriptor != -1) {
        return -1;
    }

    //
    // TODO: Add O_CLOEXEC to this.
    //

    OpenFlags = O_WRONLY | O_CREAT;
    ClShadowLockDescriptor = open(SHADOW_LOCK_PATH,
                                  OpenFlags,
                                  S_IRUSR | S_IWUSR);

    if (ClShadowLockDescriptor < 0) {
        return -1;
    }

    Flags = fcntl(ClShadowLockDescriptor, F_GETFD, 0);
    if (Flags == -1) {
        Result = -1;
        goto lckpwdfEnd;
    }

    Flags |= FD_CLOEXEC;
    Result = fcntl(ClShadowLockDescriptor, F_SETFD, Flags);
    if (Result < 0) {
        goto lckpwdfEnd;
    }

    //
    // Set an alarm handler so the signal comes in.
    //

    memset(&AlarmAction, 0, sizeof(AlarmAction));
    AlarmAction.sa_handler = ClpEmptySignalHandler;
    sigfillset(&(AlarmAction.sa_mask));
    AlarmAction.sa_flags = 0;
    Result = sigaction(SIGALRM, &AlarmAction, &OriginalAlarmAction);
    if (Result < 0) {
        goto lckpwdfEnd;
    }

    AlarmActionSet = TRUE;

    //
    // Make sure alarm is not blocked.
    //

    sigemptyset(&Mask);
    sigaddset(&Mask, SIGALRM);
    Result = sigprocmask(SIG_UNBLOCK, &Mask, &OriginalMask);
    if (Result < 0) {
        goto lckpwdfEnd;
    }

    //
    // Start a timer.
    //

    alarm(SHADOW_LOCK_TIMEOUT);

    //
    // Acquire the lock.
    //

    memset(&Lock, 0, sizeof(Lock));
    Lock.l_type = F_WRLCK;
    Lock.l_whence = SEEK_SET;
    Result = fcntl(ClShadowLockDescriptor, F_SETLKW, &Lock);

    //
    // Restore the mask.
    //

    sigprocmask(SIG_SETMASK, &OriginalMask, NULL);

lckpwdfEnd:
    if (AlarmActionSet != FALSE) {
        sigaction(SIGALRM, &OriginalAlarmAction, NULL);
    }

    if (Result != 0) {
        if (ClShadowLockDescriptor >= 0) {
            close(ClShadowLockDescriptor);
            ClShadowLockDescriptor = -1;
        }
    }

    return Result;
}

LIBC_API
int
ulckpwdf (
    void
    )

/*++

Routine Description:

    This routine unlocks the shadow password file.

Arguments:

    None.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    int Result;

    //
    // If there is no descriptor, the caller's trying to unlock something
    // they never locked.
    //

    if (ClShadowLockDescriptor < 0) {
        return -1;
    }

    Result = close(ClShadowLockDescriptor);
    ClShadowLockDescriptor = -1;
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

void
ClpEmptySignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine implements an empty signal handler. This is needed when it is
    desired that a certain signal interrupt an operation, but that signal's
    default action would be to terminate.

Arguments:

    Signal - Supplies the signal number that occurred. This is ignored.

Return Value:

    None.

--*/

{

    return;
}

