/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    login.c

Abstract:

    This module implements the login utility, which reads a user's password and
    sets up the login.

Author:

    Evan Green 16-Mar-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <utmpx.h>

#include "../swlib.h"
#include "lutil.h"

//
// ---------------------------------------------------------------- Definitions
//

#define LOGIN_VERSION_MAJOR 1
#define LOGIN_VERSION_MINOR 0

#define LOGIN_USAGE                                                            \
    "usage: login [options] [username] [ENV=var]\n"                            \
    "The login utility authenticates a user and establishes a new session. \n" \
    "Options are:\n"                                                           \
    "  -f -- Do not perform authentication, user is preauthenticated."         \
    "  -h host -- Name of the remote host for this login.\n"                   \
    "  -p -- Preserve the environment.\n"                                      \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define LOGIN_OPTIONS_STRING "fh:pHV"

//
// Define the number of seconds before login times out.
//

#define LOGIN_TIMEOUT 60

#define LOGIN_ATTEMPT_COUNT 3
#define LOGIN_MAX_EMPTY_USER_NAME_TRIES 5

#define LOGIN_SECURE_TERMINALS_PATH "/etc/securetty"
#define LOGIN_NOLOGIN_PATH "/etc/nologin"
#define LOGIN_MOTD_PATH "/etc/motd"

//
// Define application options.
//

//
// Set this option to skip authentication.
//

#define LOGIN_OPTION_NO_AUTHENTICATION 0x00000001

//
// Set this option to preserve the environment.
//

#define LOGIN_OPTION_PRESERVE_ENVIRONMENT 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
LoginCheckSecureTerminal (
    PSTR Terminal
    );

INT
LoginCheckNologin (
    VOID
    );

VOID
LoginPrintMessageOfTheDay (
    VOID
    );

void
LoginAlarmSignalHandler (
    int Signal
    );

//
// -------------------------------------------------------------------- Globals
//

struct option LoginLongOptions[] = {
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

struct termios *SwLoginTerminalSettings = NULL;
volatile BOOL SwLoginTimeout;

//
// ------------------------------------------------------------------ Functions
//

INT
LoginMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the login utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    ULONG Attempt;
    uid_t EffectiveUserId;
    ULONG EmptyUserNameCount;
    BOOL Failed;
    PSTR Host;
    PSTR Line;
    size_t LineBufferSize;
    ssize_t LineSize;
    INT Option;
    ULONG Options;
    ULONG SetupFlags;
    int Status;
    struct termios TerminalSettings;
    PSTR TtyName;
    struct passwd *User;
    uid_t UserId;
    PSTR UserName;

    Attempt = 0;
    EmptyUserNameCount = 0;
    Host = NULL;
    Line = NULL;
    LineBufferSize = 0;
    Options = 0;
    TtyName = NULL;
    UserName = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             LOGIN_OPTIONS_STRING,
                             LoginLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'f':
            Options |= LOGIN_OPTION_NO_AUTHENTICATION;
            break;

        case 'p':
            Options |= LOGIN_OPTION_PRESERVE_ENVIRONMENT;
            break;

        case 'h':
            Host = optarg;
            break;

        case 'V':
            SwPrintVersion(LOGIN_VERSION_MAJOR, LOGIN_VERSION_MINOR);
            return 1;

        case 'H':
            printf(LOGIN_USAGE);
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

    EffectiveUserId = geteuid();
    UserId = getuid();
    if (EffectiveUserId != UserId) {
        SwSanitizeEnvironment();
    }

    //
    // Only root can skip authentication and set a host.
    //

    if (EffectiveUserId != 0) {
        Options &= ~LOGIN_OPTION_NO_AUTHENTICATION;
        Host = NULL;
    }

    //
    // Grab the username argument if it's there.
    //

    if (ArgumentIndex < ArgumentCount) {
        UserName = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
    }

    if ((UserName == NULL) &&
        ((Options & LOGIN_OPTION_NO_AUTHENTICATION) != 0)) {

        SwPrintError(0, NULL, "Username required with -f");
        Status = 1;
        goto MainEnd;
    }

    //
    // Set environment values for the remainder of the arguments.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        if (strchr(Argument, '=') == NULL) {
            SwPrintError(0, Argument, "Unexpected argument");
            Status = 1;
            goto MainEnd;
        }

        putenv(Argument);
    }

    if ((tcgetattr(STDIN_FILENO, &TerminalSettings) < 0) ||
        (isatty(STDOUT_FILENO) == 0)) {

        SwPrintError(0, NULL, "Not a terminal");
        Status = 1;
        goto MainEnd;
    }

    SwLoginTerminalSettings = &TerminalSettings;
    SwLoginTimeout = FALSE;
    signal(SIGINT, SIG_DFL);
    signal(SIGALRM, LoginAlarmSignalHandler);
    alarm(LOGIN_TIMEOUT);

    //
    // Get the current terminal name.
    //

    TtyName = ttyname(STDIN_FILENO);
    if (TtyName == NULL) {
        TtyName = "(unknown terminal)";
    }

    TtyName = strdup(TtyName);
    openlog("login", LOG_PID, LOG_AUTH);
    while (TRUE) {
        if (SwLoginTimeout != FALSE) {
            Status = 1;
            goto MainEnd;
        }

        //
        // Get rid of anything built up.
        //

        tcflush(0, TCIFLUSH);

        //
        // Read in the username if not already supplied.
        //

        if (UserName == NULL) {
            SwPrintLoginPrompt();
            LineSize = getline(&Line, &LineBufferSize, stdin);
            if (LineSize <= 0) {
                Status = 1;
                goto MainEnd;
            }

            while ((LineSize != 0) && (isspace(Line[LineSize - 1]))) {
                LineSize -= 1;
            }

            UserName = Line;
            UserName[LineSize] = '\0';

            //
            // Get past whitespace.
            //

            while (isspace(*UserName)) {
                UserName += 1;
            }

            if (*UserName == '\0') {
                EmptyUserNameCount += 1;
                if (EmptyUserNameCount >= LOGIN_MAX_EMPTY_USER_NAME_TRIES) {
                    Status = 1;
                    goto MainEnd;
                }

                UserName = NULL;
                continue;
            }
        }

        Failed = FALSE;
        User = getpwnam(UserName);
        if (User != NULL) {
            if ((Options & LOGIN_OPTION_NO_AUTHENTICATION) != 0) {
                break;
            }

            //
            // Root login may be restricted to a few terminals.
            //

            if ((User->pw_uid == 0) &&
                (LoginCheckSecureTerminal(TtyName) != 0)) {

                Failed = TRUE;
            }
        }

        if (Failed != FALSE) {
            Status = EPERM;

        } else {
            Status = SwGetAndCheckPassword(User, NULL);
        }

        if (Status == 0) {
            break;
        }

        //
        // Handle an authentication failure.
        //

        sleep(LOGIN_FAIL_DELAY);
        if (Status == EPERM) {
            printf("Login incorrect\n");
            Attempt += 1;

        } else {
            syslog(LOG_WARNING,
                   "Authentication failure: uid=%ld, euid=%ld, tty=%s "
                   "user=%s rhost=%s",
                   (long int)UserId,
                   (long int)EffectiveUserId,
                   TtyName,
                   UserName,
                   Host);

            Status = 1;
            goto MainEnd;
        }

        if (Attempt >= LOGIN_ATTEMPT_COUNT) {
            if (Host != NULL) {
                syslog(LOG_WARNING,
                       "invalid password for %s on %s from %s",
                       UserName,
                       TtyName,
                       Host);

            } else {
                syslog(LOG_WARNING,
                       "invalid password for %s on %s",
                       UserName,
                       TtyName);
            }

            syslog(LOG_WARNING,
                   "Authentication failure: uid=%ld, euid=%ld, tty=%s "
                   "user=%s rhost=%s",
                   (long int)UserId,
                   (long int)EffectiveUserId,
                   TtyName,
                   UserName,
                   Host);

            SwPrintError(0,
                         NULL,
                         "Maximum number of tries exceeded (%d)",
                         LOGIN_ATTEMPT_COUNT);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // Authentication was successful.
    //

    alarm(0);
    if (User->pw_uid != 0) {
        if (LoginCheckNologin() != 0) {
            Status = 1;
            goto MainEnd;
        }
    }

    Status = fchown(STDIN_FILENO, User->pw_uid, User->pw_gid);
    if (Status != 0) {
        Status = errno;
        goto MainEnd;
    }

    fchmod(STDIN_FILENO, S_IRUSR | S_IWUSR);
    SwUpdateUtmp(getpid(), USER_PROCESS, TtyName, UserName, Host);
    SwBecomeUser(User);
    SetupFlags = SETUP_USER_ENVIRONMENT_CHANGE_ENVIRONMENT;
    if ((Options & LOGIN_OPTION_PRESERVE_ENVIRONMENT) == 0) {
        SetupFlags |= SETUP_USER_ENVIRONMENT_CLEAR_ENVIRONMENT;
    }

    SwSetupUserEnvironment(User, User->pw_shell, SetupFlags);
    LoginPrintMessageOfTheDay();
    if (Host != NULL) {
        syslog(LOG_INFO, "login as %s on %s from %s", UserName, TtyName, Host);
        if (User->pw_uid == 0) {
            syslog(LOG_INFO, "root login %s from %s", TtyName, Host);
        }

    } else {
        syslog(LOG_INFO, "login as %s on %s", UserName, TtyName);
        if (User->pw_uid == 0) {
            syslog(LOG_INFO, "root login on %s", TtyName);
        }
    }

    signal(SIGINT, SIG_DFL);
    closelog();
    SwCloseFrom(STDERR_FILENO + 1);
    SwExecuteShell(User->pw_shell, TRUE, NULL, NULL);
    Status = 1;

MainEnd:
    closelog();
    if (Line != NULL) {
        free(Line);
    }

    SwLoginTerminalSettings = NULL;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
LoginCheckSecureTerminal (
    PSTR Terminal
    )

/*++

Routine Description:

    This routine checks the secure terminals list. If the list exists
    (/etc/securetty) and this terminal is not on it, then root cannot log in.

Arguments:

    Terminal - Supplies a pointer to the name of this terminal.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Line;
    size_t LineBufferSize;
    ssize_t LineSize;
    PSTR SecureTerminal;
    FILE *SecureTerminals;
    INT Status;

    Line = NULL;
    LineBufferSize = 0;

    //
    // Open the file. If it does not exist, root can log in anywhere.
    //

    SecureTerminals = fopen(LOGIN_SECURE_TERMINALS_PATH, "r");
    if (SecureTerminals == NULL) {
        if (errno != ENOENT) {
            return EPERM;
        }

        return 0;
    }

    Status = EPERM;
    while (TRUE) {
        LineSize = getline(&Line, &LineBufferSize, stdin);
        if (LineSize < 0) {
            break;
        }

        while ((LineSize != 0) && (isspace(Line[LineSize - 1]))) {
            LineSize -= 1;
        }

        SecureTerminal = Line;
        SecureTerminal[LineSize] = '\0';

        //
        // Get past whitespace.
        //

        while (isspace(*SecureTerminal)) {
            SecureTerminal += 1;
        }

        //
        // Skip any commented lines.
        //

        if ((*SecureTerminal == '\0') || (*SecureTerminal == '#')) {
            continue;
        }

        if (strcmp(SecureTerminal, Terminal) == 0) {
            Status = 0;
            break;
        }
    }

    if (Line != NULL) {
        free(Line);
    }

    fclose(SecureTerminals);
    return Status;
}

INT
LoginCheckNologin (
    VOID
    )

/*++

Routine Description:

    This routine checks /etc/nologin, and prevents logging anyone but root in
    if the file exists.

Arguments:

    None.

Return Value:

    0 if login should proceed.

    Non-zero if login is prevented.

--*/

{

    INT Character;
    BOOL Empty;
    FILE *Nologin;

    Nologin = fopen(LOGIN_NOLOGIN_PATH, "r");
    if (Nologin == NULL) {
        return 0;
    }

    //
    // Spit out the contents of the nologin file.
    //

    Empty = TRUE;
    while (TRUE) {
        Character = fgetc(Nologin);
        if (Character == EOF) {
            break;
        }

        if (Character == '\n') {
            fputc('\r', stdout);
        }

        fputc(Character, stdout);
        Empty = FALSE;
    }

    if (Empty != FALSE) {
        printf("\r\nSystem temporarily closed.\r\n");
    }

    fclose(Nologin);
    fflush(NULL);
    tcdrain(STDOUT_FILENO);
    return 1;
}

VOID
LoginPrintMessageOfTheDay (
    VOID
    )

/*++

Routine Description:

    This routine is the timeout alarm signal handler.

Arguments:

    Signal - Supplies the signal number that fired.

Return Value:

    None.

--*/

{

    int Character;
    FILE *File;

    File = fopen(LOGIN_MOTD_PATH, "r");
    if (File == NULL) {
        return;
    }

    while (TRUE) {
        Character = fgetc(File);
        if (Character == EOF) {
            break;
        }

        fputc(Character, stdout);
    }

    fclose(File);
    return;
}

void
LoginAlarmSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine is the timeout alarm signal handler.

Arguments:

    Signal - Supplies the signal number that fired.

Return Value:

    None.

--*/

{

    int Flags;

    Flags = fcntl(STDOUT_FILENO, F_GETFL);
    fcntl(STDOUT_FILENO, F_SETFL, Flags | O_NONBLOCK);
    tcsetattr(STDOUT_FILENO, TCSANOW, SwLoginTerminalSettings);
    printf("\r\nLogin timed out after %u seconds.\r\n", LOGIN_TIMEOUT);
    fflush(NULL);
    fcntl(STDOUT_FILENO, F_SETFL, Flags);
    _Exit(1);
    return;
}

