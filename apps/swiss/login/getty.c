/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    getty.c

Abstract:

    This module implements the getty command, which connects to a terminal,
    gets the user name, and runs login to fire up a user session.

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
#include <sys/ioctl.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <utmpx.h>

#include "../swlib.h"
#include "lutil.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts the given character into its control code.
//

#define GETTY_CONTROL(_Character) ((_Character) ^ 0x40)

//
// ---------------------------------------------------------------- Definitions
//

#define GETTY_VERSION_MAJOR 1
#define GETTY_VERSION_MINOR 0

#define GETTY_USAGE                                                            \
    "usage: getty [options] port baud,... [term]\n"                            \
    "The getty utility opens a terminal, prompts for a login name, and \n"     \
    "executes login to create a new user session. Port is a device (off of \n" \
    "/dev if the path is relative). Options are:\n"                            \
    "  -8, --8bits -- Assume the terminal is 8-bit clean, disable parity \n"   \
    "      detection.\n"                                                       \
    "  -a, --autologin=user -- Log the given user in automatically without \n" \
    "      asking for a username or password.\n"                               \
    "  -f, --issue-file=file -- Set the given issue file instead of "          \
    "/etc/issue\n"                                                             \
    "  -H, --host=host -- Set the given host into utmp.\n"                     \
    "  -I, --init-string=string -- Send the given init string before \n"       \
    "      anything else. Non-printable characters can be escaped \n"          \
    "      (eg. \\012 is ASCII 10).\n"                                         \
    "  -l, --login-program=program -- Set the given login program instead \n"  \
    "      of /bin/login.\n"                                                   \
    "  -L, --local-line -- The line is a local line without the need for \n"   \
    "      carrier detect.\n"                                                  \
    "  -m, --extract-baud -- Try to detect the baud rate based on the \n"      \
    "      HAYES-compatible CONNECT string.\n"                                 \
    "  -n, --skip-login -- Don't prompt for a login name.\n"                   \
    "  -t, --timeout=timeout -- Terminate if no user name could be read in \n" \
    "      the given number of seconds.\n"                                     \
    "  -w, --wait-cr -- Wait for the terminal to send a carriage-return or \n" \
    "      line feed character before sending the issue file and login \n"     \
    "      prompt.\n"                                                          \
    "  --noclear -- Don't clear the screen.\n"                                 \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define GETTY_OPTIONS_STRING "8a:f:H:I:l:Lmnt:wHV"

#define GETTY_LOGIN_PATH "/bin/login"

//
// The clear screen sequence resets the scroll region, returns the cursor to
// the home position, and clears the screen below the cursor.
//

#define GETTY_CLEAR_SEQUENCE "\033[r\033[H\033[J"
#define GETTY_CLEAR_SEQUENCE_SIZE 9

//
// Define the maximum number of alternate baud rates.
//

#define GETTY_MAX_RATES 10

//
// Define application options.
//

//
// Set this option for a local terminal that doesn't need carrier detect.
//

#define GETTY_OPTION_LOCAL 0x00000001

//
// Set this option to autodetect the baud rate.
//

#define GETTY_OPTION_AUTO_BAUD 0x00000002

//
// Set this option to skip prompting for a login name.
//

#define GETTY_OPTION_NO_LOGIN_NAME 0x00000004

//
// Set this option to wait for a carriage return before spitting out the issue
// file and prompt.
//

#define GETTY_OPTION_WAIT_CR 0x00000008

//
// Set this option to automatically log in the given user.
//

#define GETTY_OPTION_AUTO_LOGIN 0x00000010

//
// Set this option to not clear the screen.
//

#define GETTY_OPTION_NO_CLEAR 0x00000020

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the tuple of a baud rate and its corresponding value.

Members:

    Rate - Stores the baud rate.

    Value - Stores the Bxxx value (like B9600).

--*/

typedef struct _GETTY_RATE {
    INT Rate;
    INT Value;
} GETTY_RATE, *PGETTY_RATE;

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTR
GettyParseInitString (
    PSTR String,
    size_t *ResultSize
    );

PSTR
GettyOpenTerminal (
    PSTR TtyName
    );

INT
GettySetTerminalAttributes (
    ULONG Options,
    struct termios *Settings,
    INT BaudValue
    );

INT
GettyFinalizeTerminal (
    struct termios *Settings
    );

PSTR
GettyGetUserName (
    ULONG Options,
    PSTR IssueFile,
    struct termios *Settings,
    UINTN BaudRateCount,
    PSTR TerminalName
    );

INT
GettyConvertBaudRateToValue (
    INT BaudRate
    );

INT
GettyWriteBuffer (
    int Descriptor,
    PVOID Buffer,
    size_t Size
    );

INT
GettyDetectBaudRate (
    struct termios *Settings
    );

void
GettyAlarmSignalHandler (
    int Signal
    );

//
// -------------------------------------------------------------------- Globals
//

struct option GettyLongOptions[] = {
    {"8bits", no_argument, 0, '8'},
    {"autologin", required_argument, 0, 'a'},
    {"issue-file", required_argument, 0, 'f'},
    {"host", required_argument, 0, 'H'},
    {"init-string", required_argument, 0, 'I'},
    {"login-program", required_argument, 0, 'l'},
    {"local-line", no_argument, 0, 'L'},
    {"extract-baud", no_argument, 0, 'm'},
    {"skip-login", no_argument, 0, 'n'},
    {"timeout", required_argument, 0, 't'},
    {"wait-cr", no_argument, 0, 'w'},
    {"noclear", no_argument, 0, 'N'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

GETTY_RATE GettyRates[] = {
    {50, B50},
    {75, B75},
    {110, B110},
    {134, B134},
    {150, B150},
    {200, B200},
    {300, B300},
    {600, B600},
    {1200, B1200},
    {1800, B1800},
    {2400, B2400},
    {4800, B4800},
    {9600, B9600},
    {19200, B19200},
    {38400, B38400},
    {57600, B57600},
    {115200, B115200},
    {230400, B230400},
    {0, 0}
};

BOOL GettyAlarmFired = FALSE;

//
// ------------------------------------------------------------------ Functions
//

INT
GettyMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the getty utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    BOOL AlarmSet;
    ULONG ArgumentIndex;
    UINTN BaudIndex;
    INT BaudRate;
    UINTN BaudRateCount;
    INT BaudRates[GETTY_MAX_RATES];
    PSTR BaudString;
    ssize_t BytesRead;
    CHAR Character;
    PSTR Copy;
    PSTR CurrentBaudString;
    INT FileDescriptor;
    int Flags;
    PSTR Host;
    PSTR InitString;
    size_t InitStringSize;
    PSTR IssuePath;
    UINTN LoginArgumentCount;
    PSTR LoginArguments[5];
    PSTR LoginProgram;
    PSTR NextComma;
    int Null;
    INT Option;
    ULONG Options;
    void *OriginalHandler;
    pid_t ProcessId;
    int Status;
    PSTR Swap;
    pid_t TerminalSession;
    struct termios TerminalSettings;
    PSTR TermVariable;
    INTN Timeout;
    PSTR TtyName;
    PSTR TtyPath;
    PSTR UserName;

    AlarmSet = FALSE;
    BaudRateCount = 0;
    BaudString = NULL;
    Copy = NULL;
    Host = NULL;
    InitString = NULL;
    InitStringSize = 0;
    IssuePath = ISSUE_PATH;
    LoginProgram = GETTY_LOGIN_PATH;
    Options = 0;
    OriginalHandler = NULL;
    TermVariable = NULL;
    Timeout = -1;
    TtyName = NULL;
    TtyPath = NULL;
    UserName = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             GETTY_OPTIONS_STRING,
                             GettyLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case '8':
            break;

        case 'a':
            Options |= GETTY_OPTION_AUTO_LOGIN | GETTY_OPTION_NO_LOGIN_NAME;
            UserName = optarg;
            break;

        case 'f':
            IssuePath = optarg;
            break;

        case 'H':
            Host = optarg;
            break;

        case 'I':
            InitString = GettyParseInitString(optarg, &InitStringSize);
            if (InitString == NULL) {
                SwPrintError(0, InitString, "Invalid init string");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'l':
            LoginProgram = optarg;
            break;

        case 'L':
            Options |= GETTY_OPTION_LOCAL;
            break;

        case 'm':
            Options |= GETTY_OPTION_AUTO_BAUD;
            break;

        case 'n':
            Options |= GETTY_OPTION_NO_LOGIN_NAME;
            break;

        case 'N':
            Options |= GETTY_OPTION_NO_CLEAR;
            break;

        case 't':
            Timeout = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid timeout");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'w':
            Options |= GETTY_OPTION_WAIT_CR;
            break;

        case 'V':
            SwPrintVersion(GETTY_VERSION_MAJOR, GETTY_VERSION_MINOR);
            return 1;

        case 'h':
            printf(GETTY_USAGE);
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

    if (ArgumentIndex + 1 >= ArgumentCount) {
        SwPrintError(0, NULL, "Argument expected");
        Status = 1;
        goto MainEnd;
    }

    TtyPath = Arguments[ArgumentIndex];
    BaudString = Arguments[ArgumentIndex + 1];
    ArgumentIndex += 2;

    //
    // Allow for one more argument, the TERM variable.
    //

    if (ArgumentIndex < ArgumentCount) {
        TermVariable = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        setenv("TERM", TermVariable, 1);
    }

    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, Arguments[ArgumentIndex], "Unexpected argument");
        Status = 1;
        goto MainEnd;
    }

    //
    // Allow both "tty baud" and "baud tty".
    //

    if (isdigit(*TtyPath) != 0) {
        Swap = TtyPath;
        TtyPath = BaudString;
        BaudString = Swap;
    }

    //
    // Parse the baud rates string.
    //

    Copy = strdup(BaudString);
    if (Copy == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    CurrentBaudString = Copy;
    while ((CurrentBaudString != NULL) && (BaudRateCount < GETTY_MAX_RATES)) {
        NextComma = strchr(CurrentBaudString, ',');
        if (NextComma != NULL) {
            *NextComma = '\0';
            NextComma += 1;
        }

        BaudRate = strtoul(CurrentBaudString, &AfterScan, 10);
        if (CurrentBaudString == AfterScan) {
            SwPrintError(0, CurrentBaudString, "Invalid baud rate");
            Status = 1;
            goto MainEnd;
        }

        BaudRates[BaudRateCount] = GettyConvertBaudRateToValue(BaudRate);
        if (BaudRates[BaudRateCount] == -1) {
            SwPrintError(0, CurrentBaudString, "Unsupported baud rate");

        } else {
            BaudRateCount += 1;
        }

        CurrentBaudString = NextComma;
    }

    free(Copy);
    Copy = NULL;
    if (BaudRateCount == 0) {
        SwPrintError(0, NULL, "No baud rates specified");
        Status = 1;
        goto MainEnd;
    }

    //
    // Create a new session and process group. Failure to create a new session
    // may occur if the process is already a session leader.
    //

    ProcessId = setsid();
    if (ProcessId < 0) {
        ProcessId = getpid();
        if (getsid(0) != ProcessId) {
            SwPrintError(0, NULL, "Failed to create new session");
            Status = 1;
            goto MainEnd;
        }

        TtyName = ttyname(STDIN_FILENO);
        if (TtyName != NULL) {
            FileDescriptor = SwOpen(TtyName, O_RDWR | O_NONBLOCK, 0);
            if (FileDescriptor >= 0) {
                OriginalHandler = signal(SIGHUP, SIG_IGN);
                ioctl(FileDescriptor, TIOCNOTTY);
                close(FileDescriptor);
                signal(SIGHUP, OriginalHandler);
            }
        }

        TtyName = NULL;
    }

    //
    // Close all other descriptors, open the log, and open the terminal.
    //

    SwCloseFrom(STDERR_FILENO + 1);
    openlog("getty", LOG_PID, LOG_AUTH);
    Null = SwOpen("/dev/null", O_RDWR, 0);
    if (Null < 0) {
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

    } else {
        dup2(Null, STDOUT_FILENO);
        dup2(Null, STDERR_FILENO);
        close(Null);
    }

    TtyName = GettyOpenTerminal(TtyPath);
    if (TtyName == NULL) {
        Status = 1;
        goto MainEnd;
    }

    //
    // Clear non-blocking mode.
    //

    Flags = fcntl(STDIN_FILENO, F_GETFL);
    Flags &= ~O_NONBLOCK;
    fcntl(STDIN_FILENO, F_SETFL, Flags);
    dup2(STDIN_FILENO, STDOUT_FILENO);
    dup2(STDIN_FILENO, STDERR_FILENO);

    //
    // Set the controlling terminal.
    //

    TerminalSession = tcgetsid(STDIN_FILENO);
    if ((TerminalSession < 0) || (TerminalSession != ProcessId)) {
        if (ioctl(STDIN_FILENO, TIOCSCTTY, 1) < 0) {
            SwPrintError(errno, TtyName, "Failed to set controlling terminal");
            Status = 1;
            goto MainEnd;
        }
    }

    if (tcsetpgrp(STDIN_FILENO, ProcessId) < 0) {
        SwPrintError(errno, TtyName, "Failed to set process group");
        Status = 1;
        goto MainEnd;
    }

    if (tcgetattr(STDIN_FILENO, &TerminalSettings) < 0) {
        SwPrintError(errno, TtyName, "Failed to get terminal settings");
        Status = 1;
        goto MainEnd;
    }

    SwUpdateUtmp(ProcessId, LOGIN_PROCESS, TtyName, "LOGIN", Host);

    //
    // Set up the terminal attributes.
    //

    Status = GettySetTerminalAttributes(Options,
                                        &TerminalSettings,
                                        BaudRates[0]);

    if (Status != 0) {
        SwPrintError(errno, TtyName, "Failed to set terminal attributes");
        goto MainEnd;
    }

    //
    // Write the init string if one was supplied.
    //

    if (InitString != NULL) {
        Status = GettyWriteBuffer(STDOUT_FILENO, InitString, InitStringSize);
        if (Status != 0) {
            SwPrintError(Status, NULL, "Warning: Failed to write init string");
        }
    }

    //
    // Auto-detect the baud rate if requested.
    //

    if ((Options & GETTY_OPTION_AUTO_BAUD) != 0) {
        Status = GettyDetectBaudRate(&TerminalSettings);
        if (Status != 0) {
            SwPrintError(Status, NULL, "Warning: Failed to detect baud rate");
        }
    }

    //
    // Clear the screen.
    //

    if ((Options & GETTY_OPTION_NO_CLEAR) == 0) {
        GettyWriteBuffer(STDOUT_FILENO,
                         GETTY_CLEAR_SEQUENCE,
                         GETTY_CLEAR_SEQUENCE_SIZE);
    }

    //
    // Set the timeout.
    //

    if (Timeout != -1) {
        OriginalHandler = signal(SIGALRM, GettyAlarmSignalHandler);
        alarm(Timeout);
        AlarmSet = TRUE;
    }

    //
    // Wait for a CR or LF if requested.
    //

    if ((Options & GETTY_OPTION_WAIT_CR) != 0) {
        while (TRUE) {
            BytesRead = read(STDIN_FILENO, &Character, 1);
            if ((BytesRead < 0) && (errno == EINTR)) {
                if (GettyAlarmFired != FALSE) {
                    SwPrintError(0,
                                 NULL,
                                 "Giving up due to %d second timeout",
                                 Timeout);

                    GettyFinalizeTerminal(&TerminalSettings);
                    Status = ETIMEDOUT;
                    goto MainEnd;
                }
            }

            if (BytesRead <= 0) {
                break;
            }

            if ((Character == '\r') || (Character == '\n')) {
                break;
            }
        }
    }

    //
    // Read the user name.
    //

    if ((Options & GETTY_OPTION_NO_LOGIN_NAME) == 0) {
        BaudIndex = 0;
        while (TRUE) {
            UserName = GettyGetUserName(Options,
                                        IssuePath,
                                        &TerminalSettings,
                                        BaudRateCount,
                                        TtyName);

            if (UserName != NULL) {
                break;
            }

            if (GettyAlarmFired != FALSE) {
                SwPrintError(0,
                             NULL,
                             "Giving up due to %d second timeout",
                             Timeout);

                Status = ETIMEDOUT;
                goto MainEnd;
            }

            BaudIndex = (BaudIndex + 1) % BaudRateCount;
            cfsetispeed(&TerminalSettings, BaudRates[BaudIndex]);
            cfsetospeed(&TerminalSettings, BaudRates[BaudIndex]);
            Status = tcsetattr(STDIN_FILENO, TCSANOW, &TerminalSettings);
            if (Status != 0) {
                Status = errno;
                SwPrintError(Status,
                             TtyName,
                             "Failed to set terminal settings");

                goto MainEnd;
            }
        }

    } else {

        //
        // Guess that the terminal hands back carriage returns.
        //

        TerminalSettings.c_iflag |= ICRNL;
        if ((Options & GETTY_OPTION_AUTO_LOGIN) != 0) {
            if (IssuePath != NULL) {
                SwPrintLoginIssue(IssuePath, TtyName);
            }

            SwPrintLoginPrompt();
            printf("%s (automatic login)", UserName);
            fflush(NULL);
        }
    }

    if (Timeout != -1) {
        alarm(0);
        signal(SIGALRM, OriginalHandler);
        AlarmSet = FALSE;
    }

    GettyFinalizeTerminal(&TerminalSettings);

    //
    // Fire off the login program.
    //

    LoginArguments[0] = LoginProgram;
    LoginArgumentCount = 1;
    if ((Options & GETTY_OPTION_AUTO_LOGIN) != 0) {
        LoginArguments[LoginArgumentCount] = "-f";
        LoginArgumentCount += 1;
    }

    LoginArguments[LoginArgumentCount] = "--";
    LoginArgumentCount += 1;
    if (UserName != NULL) {
        LoginArguments[LoginArgumentCount] = UserName;
        LoginArgumentCount += 1;
    }

    LoginArguments[LoginArgumentCount] = NULL;
    LoginArgumentCount += 1;
    execvp(LoginArguments[0], LoginArguments);
    SwPrintError(errno, LoginArguments[0], "Could not exec");
    Status = 1;

MainEnd:
    if (AlarmSet != FALSE) {
        alarm(0);
        signal(SIGALRM, OriginalHandler);
    }

    if ((UserName != NULL) && ((Options & GETTY_OPTION_AUTO_LOGIN) == 0)) {
        free(UserName);
    }

    if (TtyName != NULL) {
        free(TtyName);
    }

    if (Copy != NULL) {
        free(Copy);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PSTR
GettyParseInitString (
    PSTR String,
    size_t *ResultSize
    )

/*++

Routine Description:

    This routine parses the init string, handling escape characters.

Arguments:

    String - Supplies a pointer to the input string to unescape.

    ResultSize - Supplies a pointer where the size of the output string will be
        returned on success.

Return Value:

    Returns a pointer to a newly allocated string on success. The caller is
    responsible for freeing this buffer when finished.

    NULL on allocation failure.

--*/

{

    CHAR Character;
    UINTN Index;
    PSTR Out;
    PSTR Result;
    size_t Size;

    Result = malloc(strlen(String) + 1);
    if (Result == NULL) {
        return NULL;
    }

    Out = Result;
    Size = 0;
    while (*String != '\0') {
        if (*String == '\\') {
            String += 1;
            if (*String == '\\') {
                Character = '\\';
                String += 1;

            } else {
                Character = 0;
                for (Index = 0; Index < 3; Index += 1) {
                    if ((*String >= '0') && (*String <= '7')) {
                        Character *= 8;
                        Character += *String - '0';
                        String += 1;

                    } else {
                        break;
                    }
                }
            }

            *Out = Character;

        //
        // This is a non-escaped normal character.
        //

        } else {
            *Out = *String;
            String += 1;
        }

        Out += 1;
        Size += 1;
    }

    *ResultSize = Size;
    return Result;
}

PSTR
GettyOpenTerminal (
    PSTR TtyName
    )

/*++

Routine Description:

    This routine opens up the specified terminal.

Arguments:

    TtyName - Supplies the terminal name.

Return Value:

    Returns the final allocated terminal name on success. The caller is
    responsible for freeing this buffer.

    NULL on failure.

--*/

{

    int Descriptor;
    PSTR FinalName;
    int Result;
    size_t Size;

    if (strcmp(TtyName, "-") == 0) {
        Result = fcntl(STDIN_FILENO, F_GETFL);
        if ((Result & (O_RDWR | O_RDONLY | O_WRONLY)) != O_RDWR) {
            SwPrintError(0, NULL, "stdin not open for read and write");
            return NULL;
        }

        FinalName = ttyname(STDIN_FILENO);
        if (FinalName == NULL) {
            return NULL;
        }

        return strdup(FinalName);

    } else {
        if (*TtyName == '/') {
            FinalName = strdup(TtyName);
            if (FinalName == NULL) {
                return NULL;
            }

        } else {
            Size = 5 + strlen(TtyName);
            FinalName = malloc(Size);
            if (FinalName == NULL) {
                return NULL;
            }

            snprintf(FinalName, Size, "/dev/%s", TtyName);
        }

        close(STDIN_FILENO);
        Descriptor = SwOpen(FinalName, O_RDWR | O_NONBLOCK, 0);
        if (Descriptor < 0) {
            SwPrintError(errno, FinalName, "Failed to open");
            free(FinalName);
            return NULL;
        }

        dup2(Descriptor, STDIN_FILENO);
        Result = fchown(STDIN_FILENO, 0, 0);
        if (Result != 0) {
            free(FinalName);
            return NULL;
        }

        fchmod(STDIN_FILENO, S_IRUSR | S_IWUSR | S_IWGRP);
        if (Descriptor != STDIN_FILENO) {
            close(Descriptor);
        }
    }

    return FinalName;
}

INT
GettySetTerminalAttributes (
    ULONG Options,
    struct termios *Settings,
    INT BaudValue
    )

/*++

Routine Description:

    This routine sets the terminal settings according to the given attributes.

Arguments:

    Options - Supplies the application options. See GETTY_OPTION_* definitions.

    Settings - Supplies a pointer to the settings.

    BaudValue - Supplies the baud rate (definition) to set.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    tcdrain(STDIN_FILENO);
    tcflush(STDIN_FILENO, TCIOFLUSH);
    if (BaudValue != B0) {
        cfsetispeed(Settings, BaudValue);
        cfsetospeed(Settings, BaudValue);
    }

    //
    // Set up the terminal for 8-bit raw mode with blocking I/O.
    //

    Settings->c_cflag &= (CSTOPB | PARENB | PARODD);
    Settings->c_cflag |= CS8 | HUPCL | CREAD;
    if ((Options & GETTY_OPTION_LOCAL) != 0) {
        Settings->c_cflag |= CLOCAL;
    }

    Settings->c_iflag = 0;
    Settings->c_lflag = 0;
    Settings->c_oflag = OPOST | ONLCR;

    //
    // Reads should release as soon as one character is available, and wait
    // indefinitely for that character to arrive.
    //

    Settings->c_cc[VMIN] = 1;
    Settings->c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSANOW, Settings);
}

INT
GettyFinalizeTerminal (
    struct termios *Settings
    )

/*++

Routine Description:

    This routine sets the final terminal settings before launching login or
    exiting.

Arguments:

    Settings - Supplies the terminal settings to set.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Status;

    //
    // Enable software flow control.
    //

    Settings->c_iflag |= IXON | IXOFF | IMAXBEL;

    //
    // Set up canonical mode, echo, and signals.
    //

    Settings->c_lflag |= ICANON | ISIG | ECHO | ECHOE | ECHOKE | ECHOCTL;
    Settings->c_cc[VINTR] = GETTY_CONTROL('C');
    Settings->c_cc[VQUIT] = GETTY_CONTROL('\\');
    Settings->c_cc[VEOF] = GETTY_CONTROL('D');
    Settings->c_cc[VEOL] = '\n';
    Settings->c_cc[VKILL] = GETTY_CONTROL('U');
    Status = tcsetattr(STDIN_FILENO, TCSANOW, Settings);
    GettyWriteBuffer(STDOUT_FILENO, "\n", 1);
    if (Status == 0) {
        return 0;
    }

    return errno;
}

PSTR
GettyGetUserName (
    ULONG Options,
    PSTR IssueFile,
    struct termios *Settings,
    UINTN BaudRateCount,
    PSTR TerminalName
    )

/*++

Routine Description:

    This routine reads the user name from the terminal.

Arguments:

    Options - Supplies the application options. See GETTY_OPTION_* definitions.

    IssueFile - Supplies an optional pointer to the issue file to print.

    Settings - Supplies a pointer to the terminal settings.

    BaudRateCount - Supplies the number of possible baud rates.

    TerminalName - Supplies the name of this terminal.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    CHAR Character;
    PSTR Current;
    BOOL Done;
    CHAR Line[256];

    usleep(100000);
    tcflush(STDIN_FILENO, TCIFLUSH);
    Line[0] = '\0';
    do {
        if (IssueFile != NULL) {
            SwPrintLoginIssue(IssueFile, TerminalName);
        }

        SwPrintLoginPrompt();
        Current = Line;

        //
        // Loop reading characters.
        //

        Done = FALSE;
        while (Done == FALSE) {
            errno = EINTR;
            if (read(STDIN_FILENO, &Character, 1) < 1) {
                GettyFinalizeTerminal(Settings);
                if ((errno == EINTR) || (errno == EIO)) {
                    return NULL;
                }

                SwPrintError(errno, NULL, "Failed to read");
                return NULL;
            }

            switch (Character) {
            case '\r':
                Settings->c_iflag |= ICRNL;

                //
                // Fall through.
                //

            case '\n':
                *Current = '\0';
                Character = '\n';
                GettyWriteBuffer(STDOUT_FILENO, &Character, 1);
                Done = TRUE;
                break;

            case GETTY_CONTROL('U'):
                while (Current > Line) {
                    GettyWriteBuffer(STDOUT_FILENO, "\b \b", 3);
                    Current -= 1;
                }

                *Current = '\0';
                break;

            case '\b':
            case 0x7F:
                if (Current > Line) {
                    GettyWriteBuffer(STDOUT_FILENO, "\b \b", 3);
                    Current -= 1;
                }

                break;

            case GETTY_CONTROL('C'):
            case GETTY_CONTROL('D'):
                GettyFinalizeTerminal(Settings);
                return NULL;

            case '\0':
                if (BaudRateCount > 1) {
                    return NULL;
                }

                //
                // Fall through.
                //

            default:
                if ((Character >= ' ') &&
                    ((Current - Line) < sizeof(Line) - 1)) {

                    *Current = Character;
                    Current += 1;
                    GettyWriteBuffer(STDOUT_FILENO, &Character, 1);
                }

                break;
            }
        }

    } while (Line[0] == '\0');

    return strdup(Line);
}

INT
GettyConvertBaudRateToValue (
    INT BaudRate
    )

/*++

Routine Description:

    This routine converts a raw baud rate into its B definition.

Arguments:

    BaudRate - Supplies the baud rate to convert.

Return Value:

    Returns the baud value, or -1 on failure.

--*/

{

    UINTN Index;

    Index = 0;
    while (GettyRates[Index].Rate != 0) {
        if (GettyRates[Index].Rate == BaudRate) {
            return GettyRates[Index].Value;
        }

        Index += 1;
    }

    return -1;
}

INT
GettyWriteBuffer (
    int Descriptor,
    PVOID Buffer,
    size_t Size
    )

/*++

Routine Description:

    This routine writes the full buffer into the given descriptor.

Arguments:

    Descriptor - Supplies the file descriptor to write to.

    Buffer - Supplies a pointer to the buffer to write.

    Size - Supplies the number of bytes to write.

Return Value:

    0 on success.

    Returns errno on failure.

--*/

{

    ssize_t BytesWritten;

    while (Size > 0) {
        BytesWritten = write(Descriptor, Buffer, Size);
        if (BytesWritten <= 0) {
            if (errno == EINTR) {
                continue;
            }

            return errno;
        }

        Buffer += BytesWritten;
        Size -= BytesWritten;
    }

    return 0;
}

INT
GettyDetectBaudRate (
    struct termios *Settings
    )

/*++

Routine Description:

    This routine attempts to detect the baud rate from the modem status
    message.

Arguments:

    Settings - Supplies a pointer to the current terminal settings.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT BaudRate;
    INT BaudValue;
    ssize_t BytesRead;
    PSTR Current;
    CHAR Line[256];
    INT Status;
    cc_t Vmin;

    //
    // Don't block reads.
    //

    Vmin = Settings->c_cc[VMIN];
    Settings->c_cc[VMIN] = 0;
    Status = tcsetattr(STDIN_FILENO, TCSANOW, Settings);
    if (Status != 0) {
        Settings->c_cc[VMIN] = Vmin;
        return errno;
    }

    //
    // Wait a bit.
    //

    sleep(1);

    //
    // Try to read the status message. It's generally a set of digits amidst
    // some garbage before and after.
    //

    do {
        errno = 0;
        BytesRead = read(STDIN_FILENO, Line, sizeof(Line) - 1);

    } while ((BytesRead < 0) && (errno == EINTR));

    if (BytesRead > 0) {
        Current = Line;
        Line[BytesRead] = '\0';
        while (Current < Line + BytesRead) {
            if (isdigit(*Current)) {
                BaudRate = strtoul(Current, NULL, 10);
                BaudValue = GettyConvertBaudRateToValue(BaudRate);
                if (BaudValue > 0) {
                    cfsetispeed(Settings, BaudValue);
                    cfsetospeed(Settings, BaudValue);
                    break;
                }
            }

            Current += 1;
        }
    }

    Settings->c_cc[VMIN] = Vmin;
    return 0;
}

void
GettyAlarmSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine handles the alarm expiration.

Arguments:

    Signal - Supplies the signal that fired, in this case SIGALRM.

Return Value:

    None.

--*/

{

    GettyAlarmFired = TRUE;
    return;
}

