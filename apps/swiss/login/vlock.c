/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    vlock.c

Abstract:

    This module implements the vlock command, which locks a terminal until a
    password unlocks it.

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
#include <grp.h>
#include <pwd.h>
#include <shadow.h>
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

#define VLOCK_VERSION_MAJOR 1
#define VLOCK_VERSION_MINOR 0

#define VLOCK_USAGE                                                            \
    "usage: vlock\n"                                                           \
    "The vlock utility locks a terminal, requiring the user's password to \n"  \
    "unlock it. Options are:\n"                                                \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define VLOCK_OPTIONS_STRING "HV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option VlockLongOptions[] = {
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
VlockMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the vlock utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    struct sigaction NewAction;
    struct termios NewSettings;
    INT Option;
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
    int Status;
    struct passwd *User;
    uid_t UserId;

    UserId = getuid();
    User = getpwuid(UserId);
    if (User == NULL) {
        SwPrintError(0,
                     NULL,
                     "Cannot get user information for user ID %d.\n",
                     UserId);

        Status = 1;
        goto MainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             VLOCK_OPTIONS_STRING,
                             VlockLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'V':
            SwPrintVersion(VLOCK_VERSION_MAJOR, VLOCK_VERSION_MINOR);
            return 1;

        case 'H':
            printf(VLOCK_USAGE);
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
        SwPrintError(0, NULL, "Unexpected argument");
        Status = 1;
        goto MainEnd;
    }

    if (SwCheckAccount(User) != 0) {
        SwPrintError(0, User->pw_name, "Not locking terminal for account");
        Status = 1;
        goto MainEnd;
    }

    //
    // Turn off echoing, signals and breaks.
    //

    if (tcgetattr(STDIN_FILENO, &OriginalSettings) != 0) {
        Status = 1;
        goto MainEnd;
    }

    memcpy(&NewSettings, &OriginalSettings, sizeof(struct termios));
    NewSettings.c_iflag &= ~(BRKINT | ISIG | ECHO | ECHOE | ECHOK | ECHONL);
    NewSettings.c_iflag |= IGNBRK;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &NewSettings) != 0) {
        Status = 1;
        goto MainEnd;
    }

    //
    // Handle all signals so that the terminal settings can be put back.
    //

    sigemptyset(&(NewAction.sa_mask));
    NewAction.sa_flags = 0;
    NewAction.sa_handler = SIG_IGN;
    sigaction(SIGALRM, &NewAction, &SaveAlarm);
    sigaction(SIGHUP, &NewAction, &SaveHup);
    sigaction(SIGINT, &NewAction, &SaveInt);
    sigaction(SIGPIPE, &NewAction, &SavePipe);
    sigaction(SIGQUIT, &NewAction, &SaveQuit);
    sigaction(SIGTERM, &NewAction, &SaveTerm);
    sigaction(SIGTSTP, &NewAction, &SaveTstop);
    sigaction(SIGTTIN, &NewAction, &SaveTtin);
    sigaction(SIGTTOU, &NewAction, &SaveTtou);
    while (TRUE) {
        printf("Console locked by %s.\n", User->pw_name);
        Status = SwGetAndCheckPassword(User, NULL);
        if (Status == 0) {
            break;
        }

        sleep(LOGIN_FAIL_DELAY);
        if (Status == EPERM) {
            printf("vlock: Incorrect password.\n");
        }
    }

    //
    // Restore the original terminal settings.
    //

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &OriginalSettings);

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
    Status = 0;

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

