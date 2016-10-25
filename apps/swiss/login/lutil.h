/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lutil.h

Abstract:

    This header contains utility definitions for the login utilities.

Author:

    Evan Green 10-Mar-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <grp.h>
#include <pwd.h>
#include <shadow.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro zeros memory and ensures that the compiler doesn't optimize away
// the memset.
//

#define SECURITY_ZERO(_Buffer, _Size)                                       \
    {                                                                       \
        memset((_Buffer), 0, (_Size));                                      \
        *(volatile char *)(_Buffer) = *((volatile char *)(_Buffer) + 1);    \
    }

//
// ---------------------------------------------------------------- Definitions
//

#define PASSWD_FILE_PATH "/etc/passwd"
#define GROUP_FILE_PATH "/etc/group"
#define ISSUE_PATH "/etc/issue"
#define URANDOM_PATH "/dev/urandom"

#define BASE_SYSTEM_UID 1
#define BASE_NON_SYSTEM_UID 1000
#define BASE_SYSTEM_GID BASE_SYSTEM_UID
#define BASE_NON_SYSTEM_GID BASE_NON_SYSTEM_UID

#define LOGIN_FAIL_DELAY 4

//
// Define the default password algorithm: SHA512.
//

#define PASSWD_DEFAULT_ALGORITHM "$6$"

#define PASSWORD_SHADOWED "x"

//
// Set this flag to avoid changing to the user's home directory.
//

#define SETUP_USER_ENVIRONMENT_NO_DIRECTORY 0x00000001

//
// Set this flag to wipe out the entire environment except TERM, and set
// PATH, USER, LOGNAME, HOME, and SHELL.
//

#define SETUP_USER_ENVIRONMENT_CLEAR_ENVIRONMENT 0x00000002

//
// Set this flag to set USER and LOGNAME (unless becoming root), HOME, and
// SHELL.
//

#define SETUP_USER_ENVIRONMENT_CHANGE_ENVIRONMENT 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _UPDATE_PASSWORD_OPERATION {
    UpdatePasswordAddLine,
    UpdatePasswordUpdateLine,
    UpdatePasswordDeleteLine,
    UpdatePasswordAddGroupMember,
    UpdatePasswordDeleteGroupMember
} UPDATE_PASSWORD_OPERATION, *PUPDATE_PASSWORD_OPERATION;

/*++

Structure Description:

    This structure stores the tuple of a crypt hashing algorithm's ID and
    function pointer.

Members:

    Name - Stores the name of the algorithm.

    Id - Stores the ID of the algorithm.

--*/

typedef struct _PASSWD_ALGORITHM {
    PSTR Name;
    PSTR Id;
} PASSWD_ALGORITHM, *PPASSWD_ALGORITHM;

//
// -------------------------------------------------------------------- Globals
//

extern PASSWD_ALGORITHM SwPasswordAlgorithms[];
extern const struct spwd SwShadowTemplate;

//
// -------------------------------------------------------- Function Prototypes
//

INT
SwUpdatePasswordLine (
    struct passwd *User,
    struct spwd *Shadow,
    UPDATE_PASSWORD_OPERATION Operation
    );

/*++

Routine Description:

    This routine adds or updates an entry in the password database.

Arguments:

    User - Supplies a pointer to the user information structure.

    Shadow - Supplies a pointer to the shadow information.

    Operation - Supplies the operation to perform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SwUpdateGroupLine (
    struct group *Group,
    UPDATE_PASSWORD_OPERATION Operation
    );

/*++

Routine Description:

    This routine adds or updates an entry in the group database.

Arguments:

    Group - Supplies the group to add or update.

    Operation - Supplies the operation to perform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SwUpdatePasswordFile (
    PSTR FilePath,
    PSTR Name,
    PSTR NewLine,
    PSTR GroupMember,
    UPDATE_PASSWORD_OPERATION Operation
    );

/*++

Routine Description:

    This routine updates a password file, usually either passwd, groups,
    shadow, or gshadow.

Arguments:

    FilePath - Supplies a pointer to the path of the file to update.

    Name - Supplies the name of the member to update.

    NewLine - Supplies the new line to set.

    GroupMember - Supplies the group member to add or remove for group
        operations.

    Operation - Supplies the operation to perform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

PSTR
SwCreateHashedPassword (
    PSTR Algorithm,
    int RandomSource,
    UINTN Rounds,
    PSTR Password
    );

/*++

Routine Description:

    This routine creates a hashed password, choosing an algorithm and a salt.

Arguments:

    Algorithm - Supplies the algorithm to use. Valid values are something like
        "$1$", "$5$", or "$6$".

    RandomSource - Supplies an optional file descriptor to a source of random
        data.

    Rounds - Supplies the rounds parameter for SHA256 and SHA512 algorithms.
        Supply 0 to use a default value.

    Password - Supplies the plaintext password to encode.

Return Value:

    Returns a pointer to the hashed password on success. This comes from a
    static buffer and may be overwritten by subsequent calls to this function.

    NULL on failure.

--*/

INT
SwCheckAccount (
    struct passwd *User
    );

/*++

Routine Description:

    This routine validates that the account is enabled and can be logged in via
    password.

Arguments:

    User - Supplies a pointer to the user to get information for.

Return Value:

    0 if account is enabled.

    Non-zero if the account cannot be logged in to via password.

--*/

INT
SwGetAndCheckPassword (
    struct passwd *User,
    PSTR Prompt
    );

/*++

Routine Description:

    This routine asks for and validates the password for the given user.

Arguments:

    User - Supplies a pointer to the user to get information for.

    Prompt - Supplies an optional pointer to the prompt to use. Supply NULL
        to use the prompt "Enter password:".

Return Value:

    0 if the password matched.

    EPERM if the password did not match.

    EACCES if the account is locked or expired.

    Other error codes on other failures.

--*/

BOOL
SwCheckPassword (
    PSTR Password,
    PSTR EncryptedPassword
    );

/*++

Routine Description:

    This routine checks a password against its hash.

Arguments:

    Password - Supplies a pointer to the plaintext password to check.

    EncryptedPassword - Supplies the password hash.

Return Value:

    TRUE if the passwords match.

    FALSE if the passwords do not match.

--*/

PSTR
SwCrypt (
    PSTR Password,
    PSTR Salt
    );

/*++

Routine Description:

    This routine calls the crypt function off in libcrypt.

Arguments:

    Password - Supplies the plaintext password to encode.

    Salt - Supplies the algorithm and salt information.

Return Value:

    Returns a pointer to the hashed password on success. This comes from a
    static buffer and may be overwritten by subsequent calls to this function.

    NULL on failure.

--*/

BOOL
SwIsValidUserName (
    PSTR Name
    );

/*++

Routine Description:

    This routine validates that the given username doesn't have any invalid
    characters.

Arguments:

    Name - Supplies a pointer to the user name to check.

Return Value:

    TRUE if it is valid.

    FALSE if it is not valid.

--*/

INT
SwBecomeUser (
    struct passwd *User
    );

/*++

Routine Description:

    This routine changes the current identity to that of the given user,
    including the real user ID, group ID, and supplementary groups.

Arguments:

    User - Supplies a pointer to the user to become.

Return Value:

    None.

--*/

VOID
SwSetupUserEnvironment (
    struct passwd *User,
    PSTR Shell,
    ULONG Flags
    );

/*++

Routine Description:

    This routine changes the current identity to that of the given user,
    including the real user ID, group ID, and supplementary groups.

Arguments:

    User - Supplies a pointer to the user to set up for.

    Shell - Supplies a pointer to the shell to use. If not specified
        the ever-classic /bin/sh will be used.

    Flags - Supplies a bitfield of flags. See SETUP_USER_ENVIRONMENT_*
        definitions.

Return Value:

    None.

--*/

VOID
SwExecuteShell (
    PSTR Shell,
    BOOL LoginShell,
    PSTR Command,
    PSTR *AdditionalArguments
    );

/*++

Routine Description:

    This routine execs this program into a shell program with the given
    arguments.

Arguments:

    Shell - Supplies a pointer to the shell to use. If not specified
        the ever-classic /bin/sh will be used.

    LoginShell - Supplies a boolean indicating if this is a login shell or
        not.

    Command - Supplies the command to run.

    AdditionalArguments - Supplies an array of the additional arguments.

Return Value:

    None. On success, this routine does not return.

--*/

VOID
SwSanitizeEnvironment (
    VOID
    );

/*++

Routine Description:

    This routine removes several dangerous environment variables from the
    environment, and resets the PATH.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
SwPrintLoginPrompt (
    VOID
    );

/*++

Routine Description:

    This routine prints the standard login prompt.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
SwUpdateUtmp (
    pid_t ProcessId,
    int NewType,
    PSTR TerminalName,
    PSTR UserName,
    PSTR HostName
    );

/*++

Routine Description:

    This routine updates a utmp entry, and potentially wtmp as well.

Arguments:

    ProcessId - Supplies the current process ID.

    NewType - Supplies the type of entry to add.

    TerminalName - Supplies a pointer to the terminal name.

    UserName - Supplies a pointer to the user name.

    HostName - Supplies a pointer to the host name.

Return Value:

    None.

--*/

VOID
SwPrintLoginIssue (
    PSTR IssuePath,
    PSTR TerminalName
    );

/*++

Routine Description:

    This routine prints the login issue file to standard out.

Arguments:

    IssuePath - Supplies an optional path to the issue file. If not specified,
        the default of /etc/issue will be used.

    TerminalName - Supplies the name of this terminal, used for expanding %l
        escapes in the issue file.

Return Value:

    None.

--*/

