/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    su.c

Abstract:

    This module implements the su command, which is used to execute commands
    as another user.

Author:

    Evan Green 12-Mar-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
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

#define SU_VERSION_MAJOR 1
#define SU_VERSION_MINOR 0

#define SU_USAGE                                                               \
    "usage: su [options] [username]\n"                                         \
    "The su utility is used to become another user during a login session. \n" \
    "With no username specified, su defaults to becoming the superuser. \n"    \
    "Options are:\n"                                                           \
    "  -c, --command=cmd -- Specify a command that will be invoked by the \n"  \
    "      shell using its -c argument format. The executed program will \n"   \
    "      have no controlling terminal, and so it cannot be used to \n"       \
    "      execute interactive programs.\n"                                    \
    "  -, -l, --login -- Provide an environment similar to what the user \n"   \
    "      would expect if he or she had logged in directly.\n"                \
    "  -s, --shell=shell -- Specifies the shell to be invoked. If this is \n"  \
    "      not specified, $SHELL will be invoked, or the shell from the \n"    \
    "      user's account, or /bin/sh.\n"                                      \
    "  -m, --preserve-environment -- Preserve the current environment, \n"     \
    "      except for PATH and IFS.\n"                                         \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define SU_OPTIONS_STRING "c:ls:mHV"

//
// Define application options.
//

//
// Set this option to act as a login environment.
//

#define SU_OPTION_LOGIN 0x00000001

//
// Set this option to preserve the environment variables.
//

#define SU_OPTION_PRESERVE_ENVIRONMENT 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
SuIsRestrictedShell (
    PSTR Shell
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SuLongOptions[] = {
    {"command", required_argument, 0, 'c'},
    {"login", no_argument, 0, 'l'},
    {"shell", required_argument, 0, 's'},
    {"preserve-environment", no_argument, 0, 'm'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
SuMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the su utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    PSTR Command;
    uid_t CurrentUserId;
    PSTR CurrentUserName;
    BOOL Login;
    BOOL LogOpen;
    INT Option;
    ULONG Options;
    ULONG SetupFlags;
    PSTR Shell;
    int Status;
    struct passwd *User;
    PSTR UserName;

    Command = NULL;
    CurrentUserName = NULL;
    Options = 0;
    Shell = NULL;
    User = NULL;
    UserName = NULL;
    openlog("su", 0, LOG_AUTH);
    LogOpen = TRUE;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SU_OPTIONS_STRING,
                             SuLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'c':
            Command = optarg;
            break;

        case 'l':
            Options |= SU_OPTION_LOGIN;
            break;

        case 'm':
            Options |= SU_OPTION_PRESERVE_ENVIRONMENT;
            break;

        case 's':
            Shell = optarg;
            break;

        case 'V':
            SwPrintVersion(SU_VERSION_MAJOR, SU_VERSION_MINOR);
            return 1;

        case 'H':
            printf(SU_USAGE);
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

    if ((ArgumentIndex < ArgumentCount) &&
        (strcmp(Arguments[ArgumentIndex], "-") == 0)) {

        Options |= SU_OPTION_LOGIN;
        ArgumentIndex += 1;
    }

    //
    // Get the current user name.
    //

    CurrentUserId = getuid();
    User = getpwuid(CurrentUserId);
    if (User == NULL) {
        SwPrintError(0, NULL, "Failed to get current user name");
        Status = 1;
        goto MainEnd;
    }

    CurrentUserName = strdup(User->pw_name);
    if (CurrentUserName == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    //
    // Get the user entry to act as, either specified on the command line or
    // assumed to be 0 (root).
    //

    if (ArgumentIndex < ArgumentCount) {
        UserName = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        User = getpwnam(UserName);

    } else {
        User = getpwuid(0);
    }

    if (User == NULL) {
        SwPrintError(0, UserName, "Failed to find user");
        Status = 1;
        goto MainEnd;
    }

    if (CurrentUserId == 0) {
        Status = 0;

    } else {
        Status = SwGetAndCheckPassword(User, NULL);
    }

    if (Status == 0) {
        syslog(LOG_NOTICE,
               "%c %s %s:%s",
               '+',
               ttyname(STDIN_FILENO),
               CurrentUserName,
               User->pw_name);

    } else {
        syslog(LOG_NOTICE,
               "%c %s %s:%s",
               '-',
               ttyname(STDIN_FILENO),
               CurrentUserName,
               User->pw_name);

        sleep(LOGIN_FAIL_DELAY);
        if (Status == EPERM) {
            SwPrintError(0, NULL, "Incorrect password");
        }

        Status = 1;
        goto MainEnd;
    }

    closelog();
    LogOpen = FALSE;

    //
    // If a shell wasn't provided but the caller wants to preserve the
    // environment, get the shell from the environment.
    //

    if ((Shell == NULL) && ((Options & SU_OPTION_PRESERVE_ENVIRONMENT) != 0)) {
        Shell = getenv("SHELL");
    }

    //
    // If the accounts main shell is not a user shell, this is probably a
    // special account. Don't override its shell.
    //

    if ((Shell != NULL) && (CurrentUserId != 0) &&
        (User->pw_shell != NULL) &&
        (SuIsRestrictedShell(User->pw_shell) != FALSE)) {

        SwPrintError(0, NULL, "Using restricted shell");
        Shell = NULL;
    }

    if ((Shell == NULL) || (*Shell == '\0')) {
        Shell = User->pw_shell;
    }

    Status = SwBecomeUser(User);
    if (Status != 0) {
        goto MainEnd;
    }

    SetupFlags = 0;
    Login = FALSE;
    if ((Options & SU_OPTION_LOGIN) != 0) {
        SetupFlags |= SETUP_USER_ENVIRONMENT_CLEAR_ENVIRONMENT;
        Login = TRUE;

    } else {
        SetupFlags |= SETUP_USER_ENVIRONMENT_NO_DIRECTORY;
    }

    if ((Options & SU_OPTION_PRESERVE_ENVIRONMENT) == 0) {
        SetupFlags |= SETUP_USER_ENVIRONMENT_CHANGE_ENVIRONMENT;
    }

    SwSetupUserEnvironment(User, Shell, SetupFlags);
    SwExecuteShell(Shell, Login, Command, Arguments + ArgumentIndex);
    Status = 1;

MainEnd:
    if (LogOpen != FALSE) {
        closelog();
    }

    if (CurrentUserName != NULL) {
        free(CurrentUserName);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
SuIsRestrictedShell (
    PSTR Shell
    )

/*++

Routine Description:

    This routine determines if the given shell is a restricted shell, meaning
    a shell not in the user shell list (as determined by getusershell).

Arguments:

    Shell - Supplies a pointer to the path to the shell in question.

Return Value:

    TRUE if the shell is restricted (not on the user shell list).

    FALSE if the shell is a user shell.

--*/

{

    PSTR Line;
    BOOL Restricted;

    Restricted = TRUE;
    setusershell();
    while (TRUE) {
        Line = getusershell();
        if (Line == NULL) {
            break;
        }

        if ((*Line != '#') && (strcmp(Line, Shell) == 0)) {
            Restricted = FALSE;
            break;
        }
    }

    endusershell();
    return Restricted;
}

