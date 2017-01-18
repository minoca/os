/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sulogin.c

Abstract:

    This module implements the sulogin command, which completes a single-user
    login, usually used during boot for emergencies.

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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../swlib.h"
#include "lutil.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SULOGIN_VERSION_MAJOR 1
#define SULOGIN_VERSION_MINOR 0

#define SULOGIN_USAGE                                                          \
    "usage: sulogin [options] [TTY]\n"                                         \
    "The sulogin utility performs a single-user login, usually used during\n"  \
    "boot for emergencies. Options are:\n"                                     \
    "  -e -- If the root account information cannot be brought up, log \n"     \
    "     in anyway. This should only be used on the console to fix \n"        \
    "     damaged systems.\n"                                                  \
    "  -p -- Invoke the shell as a login shell (prefixing argv[0] with a "     \
    "dash).\n"                                                                 \
    "  -t secs -- Only wait the given number of seconds for user input.\n"     \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define SULOGIN_OPTIONS_STRING "ept:HV"

#define SULOGIN_PROMPT \
    "Give root password for system maintenance\n" \
    "(or type Control-D for normal startup):"

#define SULOGIN_INITIAL_PASSWORD_BUFFER_SIZE 64

//
// Define application options.
//

//
// Set this option to simply log in if the password files appear to be
// damaged or missing.
//

#define SULOGIN_OPTION_EMERGENCY 0x00000001

//
// Set this option to invoke a login shell.
//

#define SULOGIN_OPTION_LOGIN 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SuloginGetAndCheckPassword (
    struct passwd *User,
    PSTR Prompt,
    INTN Timeout
    );

INT
SuloginGetPassword (
    const char *Prompt,
    char **String,
    size_t *Size,
    INTN Timeout
    );

void
SuloginSignalHandler (
    int Signal
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SuloginLongOptions[] = {
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

int *SwSuloginGetPasswordSignals = NULL;

//
// ------------------------------------------------------------------ Functions
//

INT
SuloginMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the sulogin (single-user login)
    utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    ULONG ArgumentIndex;
    uid_t EffectiveId;
    int File;
    PSTR HashedPassword;
    BOOL LoginShell;
    INT Option;
    ULONG Options;
    uid_t RealId;
    struct spwd *Shadow;
    PSTR Shell;
    int Status;
    PSTR Terminal;
    INTN Timeout;
    struct passwd *User;

    LoginShell = FALSE;
    Options = 0;
    Shadow = NULL;
    Terminal = NULL;
    Timeout = -1;
    User = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SULOGIN_OPTIONS_STRING,
                             SuloginLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'e':
            Options |= SULOGIN_OPTION_EMERGENCY;
            break;

        case 'p':
            Options |= SULOGIN_OPTION_LOGIN;
            LoginShell = TRUE;
            break;

        case 't':
            Timeout = strtoul(optarg, &AfterScan, 10);
            if (optarg == AfterScan) {
                SwPrintError(0, optarg, "Invalid timeout specified");
                Timeout = -1;
            }

            break;

        case 'V':
            SwPrintVersion(SULOGIN_VERSION_MAJOR, SULOGIN_VERSION_MINOR);
            return 1;

        case 'H':
            printf(SULOGIN_USAGE);
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

    if (ArgumentIndex != ArgumentCount) {
        Terminal = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
    }

    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0,
                     Arguments[ArgumentIndex],
                     "Unexpected argument ignored");
    }

    //
    // If a terminal was specified, close the standard descriptors and open
    // the terminal.
    //

    if (Terminal != NULL) {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        File = SwOpen(Terminal, O_RDWR, 0);
        if (File >= 0) {
            dup2(File, STDOUT_FILENO);
            close(STDERR_FILENO);
            dup2(File, STDERR_FILENO);

        //
        // On failure, try to copy stderr back to stdin and stdout.
        //

        } else {
            dup2(STDERR_FILENO, STDIN_FILENO);
            dup2(STDERR_FILENO, STDOUT_FILENO);
        }
    }

    if ((isatty(STDIN_FILENO) == 0) || (isatty(STDOUT_FILENO) == 0) ||
        (isatty(STDERR_FILENO) == 0)) {

        SwPrintError(0, NULL, "Not a terminal");
        Status = 1;
        goto MainEnd;
    }

    RealId = getuid();
    EffectiveId = geteuid();
    if (RealId != EffectiveId) {
        SwSanitizeEnvironment();
    }

    User = getpwuid(0);
    if (User == NULL) {
        if ((Options & SULOGIN_OPTION_EMERGENCY) == 0) {
            SwPrintError(0, NULL, "Failed to get root account information");
            Status = 1;
            goto MainEnd;
        }

    } else {
        Shadow = getspnam(User->pw_name);
        if ((Shadow == NULL) && ((errno == EACCES) || (errno == EPERM))) {
            SwPrintError(errno, NULL, "Cannot access the password file");
            Status = 1;
            goto MainEnd;
        }
    }

    HashedPassword = NULL;
    if (Shadow != NULL) {
        HashedPassword = Shadow->sp_pwdp;

    } else if (User != NULL) {
        HashedPassword = User->pw_passwd;
    }

    if ((HashedPassword == NULL) ||
        ((!isalnum(*HashedPassword)) &&
         (*HashedPassword != '\0') &&
         (*HashedPassword != '.') && (*HashedPassword != '/') &&
         (*HashedPassword != '$'))) {

        if ((Options & SULOGIN_OPTION_EMERGENCY) == 0) {
            SwPrintError(0, NULL, "Root account unavailable");
            Status = 1;
            goto MainEnd;
        }

    } else {
        while (TRUE) {
            Status = SuloginGetAndCheckPassword(User, SULOGIN_PROMPT, Timeout);
            if (Status == 0) {
                break;

            } else if (Status != EPERM) {
                SwPrintError(0, NULL, "Normal startup");
                Status = 0;
                goto MainEnd;
            }

            sleep(LOGIN_FAIL_DELAY);
            SwPrintError(0, NULL, "Incorrect password");
        }
    }

    printf("System maintenance mode\n");
    Shell = getenv("SUSHELL");
    if (Shell == NULL) {
        Shell = getenv("sushell");
    }

    if ((Shell == NULL) && (User != NULL) && (User->pw_shell != NULL) &&
        (User->pw_shell[0] != '\0')) {

        Shell = User->pw_shell;
    }

    SwExecuteShell(Shell, LoginShell, NULL, NULL);
    Status = 0;

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SuloginGetAndCheckPassword (
    struct passwd *User,
    PSTR Prompt,
    INTN Timeout
    )

/*++

Routine Description:

    This routine asks for and validates the password for the given user.

Arguments:

    User - Supplies a pointer to the user to get information for.

    Prompt - Supplies a pointer to the prompt to use.

    Timeout - Supplies the timeout before just continuing. Supply -1 to
        indicate no timeout.

Return Value:

    0 if the password matched.

    EPERM if the password did not match.

    EACCES if the account is locked or expired.

    ETIMEDOUT on timeout.

    Other error codes on other failures.

--*/

{

    BOOL Correct;
    PSTR HashedPassword;
    PSTR Password;
    size_t PasswordSize;
    struct spwd *Shadow;
    INT Status;

    Password = NULL;
    PasswordSize = 0;
    HashedPassword = User->pw_passwd;
    Shadow = getspnam(User->pw_name);
    if ((Shadow == NULL) && (errno != ENOENT)) {
        if ((errno == EPERM) || (errno == EACCES)) {
            SwPrintError(errno, NULL, "Cannot access the password file");
            return errno;
        }

        SwPrintError(errno,
                     User->pw_name,
                     "Error: Could not read password information for user");

        return EACCES;
    }

    if (Shadow != NULL) {
        HashedPassword = Shadow->sp_pwdp;
    }

    //
    // If the password is empty just return success, no need to ask really.
    //

    if (*HashedPassword == '\0') {
        return 0;
    }

    if (Prompt == NULL) {
        Prompt = "Enter password:";
    }

    Status = SuloginGetPassword(Prompt, &Password, &PasswordSize, Timeout);
    if (Status != 0) {
        goto GetAndCheckPasswordEnd;
    }

    if (*HashedPassword == '!') {
        SwPrintError(0, NULL, "Account locked");
        Status = EACCES;
        goto GetAndCheckPasswordEnd;
    }

    Status = EPERM;
    Correct = SwCheckPassword(Password, HashedPassword);
    if (Correct != FALSE) {
        Status = 0;
    }

GetAndCheckPasswordEnd:
    if (Password != NULL) {
        memset(Password, 0, PasswordSize);
        free(Password);
    }

    return Status;
}

INT
SuloginGetPassword (
    const char *Prompt,
    char **String,
    size_t *Size,
    INTN Timeout
    )

/*++

Routine Description:

    This routine reads outputs the given prompt, and reads in a line of input
    without echoing it. This routine attempts to use the process' controlling
    terminal, or stdin/stderr otherwise. This routine is neither thread-safe
    nor reentrant.

Arguments:

    Prompt - Supplies a pointer to the prompt to print.

    String - Supplies a pointer where the allocated password will be returned.

    Size - Supplies a pointer where the size of the allocated password buffer
        will be returned on success.

    Timeout - Supplies the timeout, or -1 for no timeout.

Return Value:

    Returns a pointer to the entered input on success. If this is a password,
    the caller should be sure to clear this buffer out as soon as possible.

    NULL on failure.

--*/

{

    ssize_t BytesRead;
    char Character;
    int FileIn;
    size_t LineSize;
    struct sigaction NewAction;
    void *NewBuffer;
    size_t NewBufferSize;
    struct termios NewSettings;
    struct termios OriginalSettings;
    struct sigaction SaveAlarm;
    struct sigaction SaveHup;
    struct sigaction SaveInt;
    struct sigaction SavePipe;
    struct sigaction SaveQuit;
    struct sigaction SaveTerm;
    struct sigaction SaveTstop;
    struct sigaction SaveTtin;
    struct sigaction SaveTtou;
    int Signal;
    int Signals[NSIG];

    memset(Signals, 0, sizeof(Signals));
    SwSuloginGetPasswordSignals = Signals;
    FileIn = STDIN_FILENO;

    //
    // Turn off echoing.
    //

    if (tcgetattr(FileIn, &OriginalSettings) != 0) {
        return errno;
    }

    memcpy(&NewSettings, &OriginalSettings, sizeof(struct termios));
    NewSettings.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    if (tcsetattr(FileIn, TCSAFLUSH, &NewSettings) != 0) {
        return errno;
    }

    //
    // Handle all signals so that the terminal settings can be put back.
    //

    sigemptyset(&(NewAction.sa_mask));
    NewAction.sa_flags = 0;
    NewAction.sa_handler = SuloginSignalHandler;
    sigaction(SIGALRM, &NewAction, &SaveAlarm);
    sigaction(SIGHUP, &NewAction, &SaveHup);
    sigaction(SIGINT, &NewAction, &SaveInt);
    sigaction(SIGPIPE, &NewAction, &SavePipe);
    sigaction(SIGQUIT, &NewAction, &SaveQuit);
    sigaction(SIGTERM, &NewAction, &SaveTerm);
    sigaction(SIGTSTP, &NewAction, &SaveTstop);
    sigaction(SIGTTIN, &NewAction, &SaveTtin);
    sigaction(SIGTTOU, &NewAction, &SaveTtou);

    //
    // Print the prompt.
    //

    fprintf(stderr, "%s", Prompt);
    fflush(stderr);

    //
    // Set the alarm if requested.
    //

    if (Timeout > 0) {
        alarm(Timeout);
    }

    //
    // Loop reading characters from the input.
    //

    LineSize = 0;
    while (TRUE) {
        do {
            BytesRead = read(FileIn, &Character, 1);
            if (SwSuloginGetPasswordSignals[SIGALRM] > 0) {
                BytesRead = 0;
                break;
            }

        } while ((BytesRead < 0) && (errno == EINTR));

        if (BytesRead <= 0) {
            break;
        }

        //
        // Reset the alarm since something was read.
        //

        if (Timeout > 0) {
            alarm(Timeout);
        }

        //
        // Reallocate the buffer if needed.
        //

        if (LineSize + 1 >= *Size) {
            if (*Size == 0) {
                NewBufferSize = SULOGIN_INITIAL_PASSWORD_BUFFER_SIZE;

            } else {
                NewBufferSize = *Size * 2;
            }

            NewBuffer = malloc(NewBufferSize);

            //
            // Whether or not the allocation succeeded, zero out the previous
            // buffer to avoid leaking potential passwords.
            //

            if (*Size != 0) {
                if (NewBuffer != NULL) {
                    memcpy(NewBuffer, *String, *Size);
                }

                memset(*String, 0, *Size);
                free(*String);
                *String = NULL;
                *Size = 0;
            }

            if (NewBuffer == NULL) {
                LineSize = 0;
                BytesRead = -1;
                break;

            } else {
                *String = NewBuffer;
                *Size = NewBufferSize;
            }
        }

        if ((Character == '\r') || (Character == '\n')) {
            break;
        }

        //
        // Add the character to the buffer.
        //

        (*String)[LineSize] = Character;
        LineSize += 1;
    }

    if (BytesRead >= 0) {
        if ((BytesRead > 0) || (LineSize > 0)) {

            assert(LineSize + 1 < *Size);

            (*String)[LineSize] = '\0';
            LineSize += 1;

        } else {
            fputc('\n', stderr);
            BytesRead = -1;
        }
    }

    //
    // Restore the original terminal settings.
    //

    tcsetattr(FileIn, TCSAFLUSH, &OriginalSettings);

    //
    // Restore the original signal handlers.
    //

    sigaction(SIGALRM, &SaveAlarm, NULL);
    sigaction(SIGHUP, &SaveHup, NULL);
    sigaction(SIGINT, &SaveInt, NULL);
    sigaction(SIGPIPE, &SavePipe, NULL);
    sigaction(SIGQUIT, &SaveQuit, NULL);
    sigaction(SIGTERM, &SaveTerm, NULL);
    sigaction(SIGTSTP, &SaveTstop, NULL);
    sigaction(SIGTTIN, &SaveTtin, NULL);
    sigaction(SIGTTOU, &SaveTtou, NULL);

    //
    // Replay any signals that were sent during the read.
    //

    for (Signal = 0; Signal < NSIG; Signal += 1) {
        if (Signal == SIGALRM) {
            continue;
        }

        while (SwSuloginGetPasswordSignals[Signal] != 0) {
            kill(getpid(), Signal);
            SwSuloginGetPasswordSignals[Signal] -= 1;
        }
    }

    SwSuloginGetPasswordSignals = NULL;
    if (BytesRead < 0) {
        return -1;
    }

    return 0;
}

void
SuloginSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine is the signal handler installed during the get password
    function. It simply tracks signals for replay later.

Arguments:

    Signal - Supplies the signal number that fired.

Return Value:

    None.

--*/

{

    assert((SwSuloginGetPasswordSignals != NULL) && (Signal < NSIG));

    SwSuloginGetPasswordSignals[Signal] += 1;
    return;
}

