/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    passwd.c

Abstract:

    This module implements the passwd utility, which allows a user to change
    his or her password (or the superuser to change any password).

Author:

    Evan Green 11-Mar-2015

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
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../swlib.h"
#include "lutil.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PASSWD_VERSION_MAJOR 1
#define PASSWD_VERSION_MINOR 0

#define PASSWD_USAGE                                                           \
    "usage: passwd [options] username\n"                                       \
    "The passwd utility allows a user to change his or her password, or \n"    \
    "allows the superuser to change any password. Options are:\n"              \
    "  -A, --algorithm -- Specifies the password algorighm to use.\n"          \
    "      The default is SHA512.\n" \
    "  -d, --delete -- Delete a user's password (make it empty). This means\n" \
    "      no password is necessary to log in to the account.\n"               \
    "  -l, --lock -- Lock the password, disabling password-based\n "           \
    "      authentication to this account.\n"                                  \
    "  -R, --root=dir -- Chroot into the given directory before operation.\n"  \
    "  -u, --unlock -- Unlock the password.\n"                                 \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define PASSWD_OPTIONS_STRING "A:dlR:uHV"

//
// Define application options.
//

//
// Set this option to delete the password.
//

#define PASSWD_OPTION_DELETE 0x00000001

//
// Set this option to lock the password.
//

#define PASSWD_OPTION_LOCK 0x00000002

//
// Set this option to unlock the password.
//

#define PASSWD_OPTION_UNLOCK 0x00000004

//
// Define the set of password options that need root.
//

#define PASSWD_OPTIONS_ROOT \
    (PASSWD_OPTION_DELETE | PASSWD_OPTION_LOCK | PASSWD_OPTION_UNLOCK)

//
// Define the number of times to attempt to get a matching and different new
// password.
//

#define PASSWD_NEW_ATTEMPTS 3

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTR
PasswdGetNewPassword (
    struct passwd *User,
    struct spwd *Shadow,
    uid_t CurrentUid,
    PSTR Algorithm
    );

VOID
PasswdLogMessage (
    int Priority,
    PSTR Format,
    ...
    );

//
// -------------------------------------------------------------------- Globals
//

struct option PasswdLongOptions[] = {
    {"algorithm", required_argument, 0, 'A'},
    {"delete", no_argument, 0, 'd'},
    {"lock", no_argument, 0, 'l'},
    {"root", required_argument, 0, 'R'},
    {"unlock", no_argument, 0, 'u'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
PasswdMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the passwd utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Algorithm;
    PPASSWD_ALGORITHM AlgorithmEntry;
    ULONG ArgumentIndex;
    size_t Length;
    struct spwd LocalShadow;
    PSTR NewPassword;
    PSTR OldPassword;
    INT Option;
    ULONG Options;
    PSTR RootDirectory;
    struct spwd *Shadow;
    UPDATE_PASSWORD_OPERATION ShadowOperation;
    int Status;
    PSTR ThisUserName;
    struct passwd *User;
    uid_t UserId;
    PSTR UserName;

    Algorithm = PASSWD_DEFAULT_ALGORITHM;
    NewPassword = NULL;
    Options = 0;
    RootDirectory = NULL;
    ShadowOperation = UpdatePasswordUpdateLine;
    ThisUserName = NULL;
    UserName = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             PASSWD_OPTIONS_STRING,
                             PasswdLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {

        //
        // Select a new password hashing algorithm.
        //

        case 'A':
            if (strcasecmp(optarg, "des") == 0) {
                SwPrintError(0, NULL, "The DES algorithm has been deprecated");
                Status = 1;
                goto MainEnd;
            }

            AlgorithmEntry = &(SwPasswordAlgorithms[0]);
            while (AlgorithmEntry->Name != NULL) {
                if (strcasecmp(optarg, AlgorithmEntry->Name) == 0) {
                    Algorithm = AlgorithmEntry->Id;
                    break;
                }

                AlgorithmEntry += 1;
            }

            if (AlgorithmEntry->Name == NULL) {
                SwPrintError(0, optarg, "Unknown algorithm");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'd':
            Options |= PASSWD_OPTION_DELETE;
            break;

        case 'l':
            Options |= PASSWD_OPTION_LOCK;
            Options &= ~PASSWD_OPTION_UNLOCK;
            break;

        case 'R':
            RootDirectory = optarg;
            break;

        case 'u':
            Options |= PASSWD_OPTION_UNLOCK;
            Options &= ~PASSWD_OPTION_LOCK;
            break;

        case 'V':
            SwPrintVersion(PASSWD_VERSION_MAJOR, PASSWD_VERSION_MINOR);
            return 1;

        case 'H':
            printf(PASSWD_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // Chroot if requested. Warm up libcrypt in case it's not in the chrooted
    // environment.
    //

    if (RootDirectory != NULL) {
        SwCrypt(NULL, NULL);
        Status = chroot(RootDirectory);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, RootDirectory, "Failed to chroot");
            goto MainEnd;
        }

        Status = chdir("/");
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, RootDirectory, "Failed to chdir");
            goto MainEnd;
        }
    }

    if (ArgumentIndex < ArgumentCount) {
        UserName = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
    }

    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, NULL, "Unexpected additional arguments");
        return 1;
    }

    openlog("passwd", 0, LOG_AUTH);
    UserId = getuid();

    //
    // Only root can delete, lock, or unlock a password.
    //

    if (UserId != 0) {
        if ((Options & PASSWD_OPTIONS_ROOT) != 0) {
            SwPrintError(0, NULL, "-l, -u, and -d require root privileges");
            Status = 1;
            goto MainEnd;
        }
    }

    //
    // Get the current user's name.
    //

    User = getpwuid(UserId);
    if (User == NULL) {
        SwPrintError(0, NULL, "User %ld not found", (long int)UserId);
        Status = ENOENT;
        goto MainEnd;
    }

    ThisUserName = strdup(User->pw_name);
    if (ThisUserName == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    //
    // If the user specified a login name on the command line, get that one.
    //

    if (UserName != NULL) {
        User = getpwnam(UserName);
    }

    //
    // The user can only change their own password, except for the superuser.
    //

    if ((UserId != 0) && (User->pw_uid != UserId)) {
        PasswdLogMessage(LOG_WARNING,
                         "passwd: User %s cannot change password for "
                         "account %s",
                         ThisUserName,
                         User->pw_name);

        Status = EPERM;
        goto MainEnd;
    }

    //
    // Get the shadow data.
    //

    errno = 0;
    Shadow = getspnam(User->pw_name);
    if ((Shadow == NULL) && (errno != ENOENT)) {
        Status = errno;
        if ((Status == EPERM) || (Status == EACCES)) {
            SwPrintError(errno, NULL, "Cannot access the password file");
            goto MainEnd;
        }

        PasswdLogMessage(LOG_WARNING,
                         "passwd: warning: No shadow record of user %s, "
                         "creating one: %s",
                         User->pw_name,
                         strerror(Status));

        memcpy(&LocalShadow, &SwShadowTemplate, sizeof(struct spwd));
        LocalShadow.sp_namp = User->pw_name;
        LocalShadow.sp_lstchg = time(NULL) / (3600 * 24);
        Shadow = &LocalShadow;
        ShadowOperation = UpdatePasswordAddLine;
    }

    if (Shadow != NULL) {
        OldPassword = Shadow->sp_pwdp;

    } else {
        OldPassword = User->pw_passwd;
    }

    //
    // Potentially lock the password.
    //

    if ((Options & PASSWD_OPTION_LOCK) != 0) {
        if (OldPassword[0] != '!') {
            Length = strlen(OldPassword) + 2;
            NewPassword = malloc(Length);
            if (NewPassword != NULL) {
                NewPassword[0] = '!';
                strncpy(NewPassword + 1, OldPassword, Length - 1);
            }
        }

    //
    // Potentially unlock the password.
    //

    } else if ((Options & PASSWD_OPTION_UNLOCK) != 0) {
        if (OldPassword[0] == '!') {
            NewPassword = strdup(OldPassword + 1);
        }

    } else if ((Options & PASSWD_OPTION_DELETE) != 0) {
        NewPassword = strdup("");

    //
    // This is not a lock, unlock, or delete, just a regular password change.
    //

    } else {
        if ((UserId != 0) && (OldPassword[0] == '!')) {
            PasswdLogMessage(LOG_WARNING,
                             "passwd: Cannot change password for %s: "
                             "Account locked",
                             User->pw_name);

            Status = EPERM;
            goto MainEnd;
        }

        NewPassword = PasswdGetNewPassword(User, Shadow, UserId, Algorithm);
    }

    if (NewPassword == NULL) {
        SwPrintError(0,
                     NULL,
                     "passwd: Password for %s is unchanged",
                     User->pw_name);

        Status = 1;
        goto MainEnd;
    }

    if (Shadow != NULL) {
        Shadow->sp_pwdp = NewPassword;
        User->pw_passwd = PASSWORD_SHADOWED;
        Shadow->sp_lstchg = time(NULL) / (3600 * 24);

    } else {
        User->pw_passwd = NewPassword;
    }

    Status = SwUpdatePasswordLine(User, Shadow, ShadowOperation);
    if (Status < 0) {
        PasswdLogMessage(LOG_ERR,
                         "passwd: Unable to change password for %s: %s",
                         User->pw_name,
                         strerror(Status));

    } else {
        PasswdLogMessage(LOG_NOTICE,
                         "passwd: Password changed for user %s",
                         User->pw_name);
    }

MainEnd:
    closelog();
    if (NewPassword != NULL) {
        SECURITY_ZERO(NewPassword, strlen(NewPassword));
        free(NewPassword);
    }

    if (ThisUserName != NULL) {
        free(ThisUserName);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PSTR
PasswdGetNewPassword (
    struct passwd *User,
    struct spwd *Shadow,
    uid_t CurrentUid,
    PSTR Algorithm
    )

/*++

Routine Description:

    This routine reads a new password in for the user.

Arguments:

    User - Supplies a pointer to the user structure.

    Shadow - Supplies an optional pointer to the shadow structure.

    CurrentUid - Supplies the current running user ID.

    Algorithm - Supplies a pointer to the algorithm to use.

Return Value:

    Returns a pointer to an allocated hashed password on success.

    NULL on failure.

--*/

{

    UINTN Attempt;
    BOOL Correct;
    PSTR CurrentPassword;
    int Match;
    PSTR NewHashedPassword;
    PSTR NewHashedPasswordCopy;
    PSTR NewPassword;
    PSTR NewPasswordCopy;
    PSTR OldPasswordHash;

    if (Shadow != NULL) {
        OldPasswordHash = Shadow->sp_pwdp;

    } else {
        OldPasswordHash = User->pw_passwd;
    }

    //
    // Validate the old password first.
    //

    if ((CurrentUid != 0) && (OldPasswordHash != NULL) &&
        (OldPasswordHash[0] != '\0')) {

        CurrentPassword = getpass("Old password: ");
        if (CurrentPassword == NULL) {
            return NULL;
        }

        Correct = SwCheckPassword(CurrentPassword, OldPasswordHash);

        //
        // Zero out the password buffer.
        //

        SECURITY_ZERO(CurrentPassword, strlen(CurrentPassword));
        if (Correct == FALSE) {
            sleep(LOGIN_FAIL_DELAY);
            PasswdLogMessage(LOG_WARNING,
                             "Incorrect password for %s",
                             User->pw_name);

            return NULL;
        }
    }

    for (Attempt = 0; Attempt < PASSWD_NEW_ATTEMPTS; Attempt += 1) {

        //
        // Ask for the new password.
        //

        NewPassword = getpass("New password: ");
        if (NewPassword == NULL) {
            return NULL;
        }

        NewPasswordCopy = strdup(NewPassword);
        SECURITY_ZERO(NewPassword, strlen(NewPassword));
        if (NewPasswordCopy == NULL) {
            return NULL;
        }

        //
        // Ask for it again.
        //

        NewPassword = getpass("Retype new password: ");
        if (NewPassword == NULL) {
            SECURITY_ZERO(NewPasswordCopy, strlen(NewPasswordCopy));
            free(NewPasswordCopy);
            NewPasswordCopy = NULL;
            return NULL;
        }

        //
        // Complain if they don't match, it's the same as the old password,
        // or it's empty.
        //

        Match = strcmp(NewPassword, NewPasswordCopy);
        SECURITY_ZERO(NewPassword, strlen(NewPassword));
        Correct = SwCheckPassword(NewPasswordCopy, OldPasswordHash);
        if ((Match != 0) ||
            (Correct != FALSE) ||
            (NewPasswordCopy[0] == '\0')) {

            if (Match != 0) {
                SwPrintError(0, NULL, "Passwords don't match");

            } else if (NewPasswordCopy[0] == '\0') {
                SwPrintError(0,
                             NULL,
                             "Error: Password is empty, use -d to delete a "
                             "password");

            } else {
                SwPrintError(0,
                             NULL,
                             "New password is the same as the old one");
            }

            SECURITY_ZERO(NewPasswordCopy, strlen(NewPasswordCopy));
            free(NewPasswordCopy);
            NewPasswordCopy = NULL;
            continue;
        }

        break;
    }

    //
    // Hash up the password.
    //

    NewHashedPassword = NULL;
    NewHashedPasswordCopy = NULL;
    if (NewPasswordCopy != NULL) {
        NewHashedPassword = SwCreateHashedPassword(Algorithm,
                                                   -1,
                                                   0,
                                                   NewPasswordCopy);

        SECURITY_ZERO(NewPasswordCopy, strlen(NewPasswordCopy));
        free(NewPasswordCopy);

        //
        // Return a copy of that hash to get it out of the static buffer.
        //

        NewHashedPasswordCopy = strdup(NewHashedPassword);
        SECURITY_ZERO(NewHashedPassword, strlen(NewHashedPassword));
    }

    return NewHashedPasswordCopy;
}

VOID
PasswdLogMessage (
    int Priority,
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine logs a message to both stderr and the syslog.

Arguments:

    Priority - Supplies the priority of the log message. See LOG_* definitions.

    Format - Supplies the printf-style format string.

    ... - Supplies the additional arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;

    va_start(ArgumentList, Format);
    vsyslog(Priority, Format, ArgumentList);
    va_end(ArgumentList);
    va_start(ArgumentList, Format);
    vfprintf(stderr, Format, ArgumentList);
    va_end(ArgumentList);
    fputc('\n', stderr);
    return;
}

