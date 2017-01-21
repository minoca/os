/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    telnetd.c

Abstract:

    This module implements a simple telnet daemon.

Author:

    Evan Green 26-Sep-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#define TELOPTS 1

#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <netinet/in.h>
#include <poll.h>
#include <pty.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include "swlib.h"
#include "login/lutil.h"

//
// --------------------------------------------------------------------- Macros
//

//
// The buffer is empty when the producer and consumer are equal.
//

#define TELNETD_BUFFER_EMPTY(_Buffer) \
    ((_Buffer)->Producer == (_Buffer)->Consumer)

//
// The buffer is full when the producer is exactly one behind the consumer
// (modulo buffer size).
//

#define TELNETD_BUFFER_FULL(_Buffer) \
    (TELNETD_BUFFER_ADVANCE((_Buffer)->Producer) == (_Buffer)->Consumer)

//
// Determine the next buffer index, modulo the buffer size (which is assumed to
// be a power of two).
//

#define TELNETD_BUFFER_ADVANCE(_Value) \
    (((_Value) + 1) & (TELNETD_BUFFER_SIZE - 1))

//
// ---------------------------------------------------------------- Definitions
//

#define TELNETD_VERSION_MAJOR 1
#define TELNETD_VERSION_MINOR 0

#define TELNETD_USAGE                                                          \
    "usage: telnetd [options]\n"                                               \
    "The telnetd fires up a simple telnet daemon that accepts incoming \n"     \
    "connections. Note that everything (including passwords) is sent in \n"    \
    "plaintext, so telnet should not be used in production environments.\n"    \
    "Options are:\n"                                                           \
    "  -l login -- Execute the given login command upon connection \n"         \
    "      instead of /bin/login.\n"                                           \
    "  -f issue -- Send the given issue file instead of /etc/issue.\n"         \
    "  -K -- Close connection as soon as login exits.\n"                       \
    "  -p port -- Listen on the given port instead of 23.\n"                   \
    "  -b address[:port] -- Listen on the given address/port.\n"               \
    "  -F -- Run in the foreground.\n"                                         \
    "  -i -- Run in inetd mode.\n"                                             \
    "  -S -- Log to syslog. This is implied by -i without -F.\n"               \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define TELNETD_OPTIONS_STRING "l:f:Kp:b:FiShV"

//
// Define the telnetd buffer size.
//

#define TELNETD_BUFFER_SIZE 1024

//
// Define telnetd options
//

#define TELNETD_DEFAULT_LOGIN_PATH "/bin/login"
#define TELNETD_DEFAULT_ISSUE_PATH "/etc/issue"
#define TELNETD_DEFAULT_PORT 23

//
// Set this option to close the connection as soon as the login process
// terminates. Without this set, the connection is terminated when the slave
// pty is closed.
//

#define TELNETD_CLOSE_ON_LOGIN_EXIT 0x00000001

//
// Set this option to run in the foreground.
//

#define TELNETD_FOREGROUND 0x00000002

//
// Set this option to run in inetd mode.
//

#define TELNETD_INETD_MODE 0x00000004

//
// Set this option to log to syslog.
//

#define TELNETD_LOG_SYSLOG 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a telnetd buffer.

Members:

    Data - Stores the actual data buffer pointer.

    Producer - Stores the index of the next byte to go into the buffer. The
        buffer is empty when the producer equals the consumer.

    Consumer - Stores the index of the next byte to be read out of the buffer.

--*/

typedef struct _TELNETD_BUFFER {
    PUCHAR Data;
    ULONG Producer;
    ULONG Consumer;
} TELNETD_BUFFER, *PTELNETD_BUFFER;

/*++

Structure Description:

    This structure defines the context for a single telnet session.

Members:

    ListEntry - Stores pointers to the next and previous sessions.

    ToPty - Stores the data buffer going from the socket to the pseudo-terminal.

    FromPty - Stores the data buffer from from the pseudo-terminal to the
        socket.

    Pty - Stores the file descriptor of the master side of the pseudo-terminal
        created for the session.

    Input - Stores the file descriptor to use for gathering input to feed into
        the pseudo-terminal.

    Output - Stores the file descriptor to use for sending output gathered from
        the pseudo-terminal.

    Pid - Stores the process ID of the child process spawned for this session.

    PtyPoll - Stores the poll descriptor index for the pseudo-terminal.

    InputPoll - Stores the poll descriptor index for the input.

    OutputPoll - Stores the poll descriptor index for the output.

--*/

typedef struct _TELNETD_SESSION {
    LIST_ENTRY ListEntry;
    TELNETD_BUFFER ToPty;
    TELNETD_BUFFER FromPty;
    int Pty;
    int Input;
    int Output;
    pid_t Pid;
    int PtyPoll;
    int InputPoll;
    int OutputPoll;
} TELNETD_SESSION, *PTELNETD_SESSION;

/*++

Structure Description:

    This structure defines the context for an instantiation of the telnetd
    daemon.

Members:

    Options - Stores the bitfield of TELNETD_* application options.

    LoginPath - Stores the path to the login program to execute when a new
        connection is established.

    IssuePath - Stores the issue file path to spit out.

    SessionList - Stores the head of the list of active sessions.

    Port - Stores the port number to listen on.

    Poll - Stores a pointer to the poll descriptors the application waits on.

    PollCount - Stores the number of valid poll descriptors in the poll array.

    PollCapacity - Stores the maximum number of elements in the poll array
        before it needs to be resized.

--*/

typedef struct _TELNETD_CONTEXT {
    ULONG Options;
    PSTR LoginPath;
    PSTR IssuePath;
    LIST_ENTRY SessionList;
    USHORT Port;
    struct pollfd *Poll;
    ULONG PollCount;
    ULONG PollCapacity;
} TELNETD_CONTEXT, *PTELNETD_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
TelnetdCreateSession (
    PTELNETD_CONTEXT Context,
    int InputDescriptor
    );

VOID
TelnetdDestroySession (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session
    );

void
TelnetdChildSignalHandler (
    int Signal
    );

VOID
TelnetdWriteToTerminal (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session
    );

VOID
TelnetdWriteToSocket (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session
    );

VOID
TelnetdReadToBuffer (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session,
    int Descriptor,
    PTELNETD_BUFFER Buffer,
    struct pollfd *InputPoll,
    struct pollfd *OutputPoll
    );

VOID
TelnetdUpdatePollBits (
    PTELNETD_BUFFER Buffer,
    struct pollfd *InputPoll,
    struct pollfd *OutputPoll
    );

VOID
TelnetdKillSession (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session
    );

VOID
TelnetdSetNonBlock (
    int Descriptor
    );

ssize_t
TelnetdWrite (
    int FileDescriptor,
    const void *Buffer,
    size_t ByteCount
    );

ssize_t
TelnetdRead (
    int FileDescriptor,
    void *Buffer,
    size_t ByteCount
    );

int
TelnetdAddPollDescriptor (
    PTELNETD_CONTEXT Context,
    int FileDescriptor
    );

void
TelnetdReleasePollDescriptor (
    PTELNETD_CONTEXT Context,
    int Index
    );

//
// -------------------------------------------------------------------- Globals
//

struct option TelnetdLongOptions[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

UCHAR TelnetdInitialCommands[] = {
    IAC, DO, TELOPT_ECHO,
    IAC, DO, TELOPT_NAWS,
    IAC, WILL, TELOPT_ECHO,
    IAC, WILL, TELOPT_SGA
};

//
// Store the single global telnet session allowed per process. This is needed
// so the signal handler can get back to the context.
//

PTELNETD_CONTEXT TelnetdContext;

//
// ------------------------------------------------------------------ Functions
//

INT
TelnetdMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the telnetd daemon.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    pid_t Child;
    struct sigaction ChildAction;
    struct sigaction ChildActionBefore;
    BOOL ChildActionSet;
    TELNETD_CONTEXT Context;
    PLIST_ENTRY CurrentEntry;
    int DevNull;
    int EventCount;
    struct pollfd *InputPoll;
    PSTR ListenAddress;
    int ListenSocket;
    struct pollfd *ListenSocketPoll;
    INT ListenSocketPollIndex;
    int NewConnection;
    INT Option;
    struct pollfd *OutputPoll;
    struct pollfd *PtyPoll;
    PTELNETD_SESSION Session;
    struct sockaddr_in SocketAddress;
    int Status;
    int Value;

    ChildActionSet = FALSE;
    memset(&Context, 0, sizeof(TELNETD_CONTEXT));
    TelnetdContext = &Context;
    Context.LoginPath = TELNETD_DEFAULT_LOGIN_PATH;
    Context.IssuePath = TELNETD_DEFAULT_ISSUE_PATH;
    Context.Port = TELNETD_DEFAULT_PORT;
    INITIALIZE_LIST_HEAD(&(Context.SessionList));
    ListenAddress = NULL;
    ListenSocket = -1;
    ListenSocketPoll = NULL;
    ListenSocketPollIndex = -1;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TELNETD_OPTIONS_STRING,
                             TelnetdLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'l':
            Context.LoginPath = optarg;
            if (access(optarg, X_OK) != 0) {
                SwPrintError(errno, optarg, "Warning: not accessible");
            }

            break;

        case 'f':
            Context.IssuePath = optarg;
            break;

        case 'K':
            Context.Options |= TELNETD_CLOSE_ON_LOGIN_EXIT;
            break;

        case 'p':
            Context.Port = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid port");
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 'b':
            ListenAddress = optarg;
            break;

        case 'F':
            Context.Options |= TELNETD_FOREGROUND;
            break;

        case 'i':
            Context.Options |= TELNETD_INETD_MODE;
            break;

        case 'S':
            Context.Options |= TELNETD_LOG_SYSLOG;
            break;

        case 'V':
            SwPrintVersion(TELNETD_VERSION_MAJOR, TELNETD_VERSION_MINOR);
            return 1;

        case 'h':
            printf(TELNETD_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // Syslog mode is implied if inetd mode is on and not in the foreground.
    //

    if ((Context.Options & (TELNETD_INETD_MODE | TELNETD_FOREGROUND)) ==
        TELNETD_INETD_MODE) {

        Context.Options |= TELNETD_LOG_SYSLOG;
    }

    //
    // Get into the background if it's not inetd and it's not in foreground
    // mode.
    //

    if ((Context.Options & (TELNETD_INETD_MODE | TELNETD_FOREGROUND)) == 0) {

        //
        // Fork and exit in the parent, continue in the child.
        //

        Child = fork();
        if (Child < 0) {
            Status = errno;
            SwPrintError(Status, NULL, "Failed to fork");
            goto MainEnd;
        }

        //
        // Exit immediately in the parent, continue in the child.
        //

        if (Child > 0) {
            Status = 0;
            goto MainEnd;
        }

        //
        // Become a session leader, detaching from the controlling terminal.
        //

        if (setsid() < 0) {
            Status = errno;
            goto MainEnd;
        }

        //
        // Point standard in, out, and error at /dev/null.
        //

        DevNull = SwOpen("/dev/null", O_RDWR, 0);
        if (DevNull >= 0) {
            dup2(DevNull, STDIN_FILENO);
            dup2(DevNull, STDOUT_FILENO);
            dup2(DevNull, STDERR_FILENO);
            close(DevNull);
        }

        SwCloseFrom(STDERR_FILENO + 1);

        //
        // Double fork. Do this to prevent the grandchild process from ever
        // acquiring a controlling terminal. Since only the session leader
        // can acquire a controlling terminal, after the fork its new PID
        // will not be the session leader ID.
        //

        Child = fork();
        if (Child < 0) {
            exit(1);

        } else if (Child != 0) {
            exit(0);
        }

        //
        // The remainder now runs as the grandchild.
        //

    }

    //
    // Fire up syslog.
    //

    if ((Context.Options & TELNETD_LOG_SYSLOG) != 0) {
        openlog("telnetd", LOG_PID, LOG_DAEMON);
    }

    //
    // In inetd mode, just create a session based on stdin.
    //

    if ((Context.Options & TELNETD_INETD_MODE) != 0) {
        Status = TelnetdCreateSession(&Context, STDIN_FILENO);
        if (Status != 0) {
            goto MainEnd;
        }

    //
    // In regular mode, fire up a socket and listen for incoming connections.
    //

    } else {
        if (ListenAddress != NULL) {
            SwPrintError(0, ListenAddress, "Not currently implemented");
            Status = EINVAL;
            goto MainEnd;
        }

        ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (ListenSocket < 0) {
            Status = errno;
            SwPrintError(Status, NULL, "Cannot create socket");
            goto MainEnd;
        }

        memset(&SocketAddress, 0, sizeof(SocketAddress));
        SocketAddress.sin_family = AF_INET;
        SocketAddress.sin_port = htons(Context.Port);
        SocketAddress.sin_addr.s_addr = INADDR_ANY;
        Value = 1;
        setsockopt(ListenSocket,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &Value,
                   sizeof(Value));

        Status = bind(ListenSocket,
                      (struct sockaddr *)&SocketAddress,
                      sizeof(SocketAddress));

        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, NULL, "Cannot bind");
            goto MainEnd;
        }

        Status = listen(ListenSocket, 5);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, NULL, "Cannot listen");
            goto MainEnd;
        }

        fcntl(ListenSocket, F_SETFD, FD_CLOEXEC);
        ListenSocketPollIndex = TelnetdAddPollDescriptor(&Context,
                                                         ListenSocket);

        if (ListenSocketPollIndex == -1) {
            Status = errno;
            goto MainEnd;
        }

        ListenSocketPoll = &(Context.Poll[ListenSocketPollIndex]);
        ListenSocketPoll->events = POLLIN;
    }

    //
    // The whole daemon should not go down if a session goes down.
    //

    signal(SIGPIPE, SIG_IGN);

    //
    // Set the child signal handler if the caller wants to kill sessions when
    // children die. Otherwise, ignore child signals to prevent zombie
    // processes.
    //

    memset(&ChildAction, 0, sizeof(ChildAction));
    if ((Context.Options & TELNETD_CLOSE_ON_LOGIN_EXIT) != 0) {
        ChildAction.sa_handler = TelnetdChildSignalHandler;

    } else {
        ChildAction.sa_handler = SIG_IGN;
    }

    Status = sigaction(SIGCHLD, &ChildAction, &ChildActionBefore);
    if (Status == 0) {
        ChildActionSet = TRUE;
    }

    //
    // Loop pumping data back and forth.
    //

    while (TRUE) {
        EventCount = poll(Context.Poll, Context.PollCount, -1);
        if (EventCount == -1) {
            if (errno == EINTR) {
                continue;
            }

            Status = errno;
            SwPrintError(Status, NULL, "Failed to poll");
            goto MainEnd;
        }

        //
        // Check for a new connection.
        //

        if (ListenSocketPollIndex != -1) {
            ListenSocketPoll = &(Context.Poll[ListenSocketPollIndex]);
            if (ListenSocketPoll->revents != 0) {
                NewConnection = accept(ListenSocket, NULL, NULL);
                if (NewConnection >= 0) {
                    fcntl(NewConnection, F_SETFD, FD_CLOEXEC);
                }

                Status = TelnetdCreateSession(&Context, NewConnection);
                if (Status != 0) {
                    SwPrintError(Status, NULL, "Failed to create session");
                    close(NewConnection);
                }
            }
        }

        //
        // Loop through and process data for all sessions.
        //

        CurrentEntry = Context.SessionList.Next;
        while (CurrentEntry != &(Context.SessionList)) {
            Session = LIST_VALUE(CurrentEntry, TELNETD_SESSION, ListEntry);

            //
            // Move on to the next one early in case this session is destroyed.
            //

            CurrentEntry = CurrentEntry->Next;

            //
            // If a child signal occurred and the session died, clean it up.
            //

            if (Session->Pid < 0) {
                TelnetdDestroySession(&Context, Session);
            }

            PtyPoll = &(Context.Poll[Session->PtyPoll]);
            InputPoll = &(Context.Poll[Session->InputPoll]);
            OutputPoll = &(Context.Poll[Session->OutputPoll]);

            //
            // Write to the terminal from the buffer.
            //

            if ((PtyPoll->revents & POLLOUT) != 0) {
                TelnetdWriteToTerminal(&Context, Session);
            }

            //
            // Write to the socket output from the buffer.
            //

            if ((OutputPoll->revents & POLLOUT) != 0) {
                TelnetdWriteToSocket(&Context, Session);
            }

            //
            // Read from the socket input to the buffer.
            //

            if ((InputPoll->revents & POLLIN) != 0) {
                TelnetdReadToBuffer(&Context,
                                    Session,
                                    Session->Input,
                                    &(Session->ToPty),
                                    InputPoll,
                                    PtyPoll);
            }

            //
            // Read from the terminal to the buffer.
            //

            if ((PtyPoll->revents & POLLIN) != 0) {
                TelnetdReadToBuffer(&Context,
                                    Session,
                                    Session->Pty,
                                    &(Session->FromPty),
                                    PtyPoll,
                                    OutputPoll);
            }
        }
    }

    Status = 0;

MainEnd:
    while (!LIST_EMPTY(&(Context.SessionList))) {
        Session = LIST_VALUE(Context.SessionList.Next,
                             TELNETD_SESSION,
                             ListEntry);

        TelnetdKillSession(&Context, Session);
    }

    if (ListenSocket != -1) {
        close(ListenSocket);
    }

    if (Context.Poll != NULL) {
        free(Context.Poll);
    }

    signal(SIGPIPE, SIG_DFL);
    if (ChildActionSet != FALSE) {
        sigaction(SIGCHLD, &ChildActionBefore, NULL);
    }

    TelnetdContext = NULL;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
TelnetdCreateSession (
    PTELNETD_CONTEXT Context,
    int InputDescriptor
    )

/*++

Routine Description:

    This routine creates a new telnetd session and adds it to the application
    context.

Arguments:

    Context - Supplies a pointer to the application context.

    InputDescriptor - Supplies the file descriptor of the telnet input. This
        is usually either a socket or standard in.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    struct sockaddr_storage Address;
    socklen_t AddressLength;
    char AddressString[512];
    UINTN AllocationSize;
    pid_t Child;
    PSTR ExecArguments[2];
    int PtySlave;
    PTELNETD_SESSION Session;
    char SlavePath[PATH_MAX];
    INT Status;
    struct termios TerminalSettings;
    int Value;

    PtySlave = -1;
    AllocationSize = sizeof(TELNETD_SESSION) + (TELNETD_BUFFER_SIZE * 2);
    Session = malloc(AllocationSize);
    if (Session == NULL) {
        Status = errno;
        goto CreateSessionEnd;
    }

    memset(Session, 0, AllocationSize);
    Session->Pty = -1;
    Session->ToPty.Data = (PUCHAR)(Session + 1);
    Session->FromPty.Data = Session->ToPty.Data + TELNETD_BUFFER_SIZE;
    Session->Input = InputDescriptor;
    Session->Output = InputDescriptor;

    //
    // Create a new pseudo-terminal.
    //

    Status = openpty(&(Session->Pty), &PtySlave, SlavePath, NULL, NULL);
    if (Status != 0) {
        Status = errno;
        SwPrintError(Status, NULL, "Cannot create pty");
        goto CreateSessionEnd;
    }

    //
    // Make the terminal close on exec and non-blocking.
    //

    TelnetdSetNonBlock(Session->Pty);
    fcntl(Session->Pty, F_SETFD, FD_CLOEXEC);

    //
    // Make the socket (if it is a socket) keep-alive.
    //

    Value = 1;
    setsockopt(InputDescriptor,
               SOL_SOCKET,
               SO_KEEPALIVE,
               &Value,
               sizeof(Value));

    //
    // If the input is stdin, use stdout as the output.
    //

    TelnetdSetNonBlock(InputDescriptor);
    if (InputDescriptor == STDIN_FILENO) {
        InputDescriptor = STDOUT_FILENO;
        TelnetdSetNonBlock(InputDescriptor);
        Session->Output = InputDescriptor;
    }

    //
    // Create poll descriptors for each of the descriptors.
    //

    Session->PtyPoll = TelnetdAddPollDescriptor(Context, Session->Pty);
    Session->InputPoll = TelnetdAddPollDescriptor(Context, Session->Input);
    if ((Session->PtyPoll == -1) || (Session->InputPoll == -1)) {
        Status = errno;
        goto CreateSessionEnd;
    }

    Session->OutputPoll = Session->InputPoll;
    if (Session->Output != Session->Input) {
        Session->OutputPoll = TelnetdAddPollDescriptor(Context,
                                                       Session->Output);

        if (Session->OutputPoll == -1) {
            Status = errno;
            goto CreateSessionEnd;
        }
    }

    //
    // At first, only input data is requested. The buffers are empty, so
    // there's no desire to know about writes.
    //

    Context->Poll[Session->PtyPoll].events = POLLIN;
    Context->Poll[Session->InputPoll].events = POLLIN;

    //
    // Write the initial command sequence directly to the socket.
    //

    TelnetdWrite(Session->Output,
                 TelnetdInitialCommands,
                 sizeof(TelnetdInitialCommands));

    fflush(NULL);
    Child = fork();
    if (Child < 0) {
        Status = errno;
        SwPrintError(Status, NULL, "Failed to fork");
        goto CreateSessionEnd;
    }

    //
    // In the parent, the session is created, jump to the end of the function.
    //

    if (Child > 0) {
        Session->Pid = Child;
        INSERT_BEFORE(&(Session->ListEntry), &(Context->SessionList));
        Status = 0;
        goto CreateSessionEnd;
    }

    //
    // This is the child. Restore signal handling.
    //

    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_DFL);
    Child = getpid();

    //
    // Try to get the remote side string.
    //

    AddressString[0] = '\0';
    AddressLength = sizeof(Address);
    Status = getpeername(InputDescriptor,
                         (struct sockaddr *)&Address,
                         &AddressLength);

    if (Status == 0) {
        inet_ntop(Address.ss_family,
                  &Address,
                  AddressString,
                  sizeof(AddressString));
    }

    SwUpdateUtmp(Child, LOGIN_PROCESS, SlavePath, "LOGIN", AddressString);

    //
    // Fire up a new session, make this terminal the controlling terminal, set
    // it to standard in/out/error, and close the slave.
    //

    login_tty(PtySlave);
    PtySlave = -1;

    //
    // Set up the terminal.
    //

    tcgetattr(STDIN_FILENO, &TerminalSettings);
    TerminalSettings.c_lflag |= ECHO;
    TerminalSettings.c_oflag |= ONLCR;
    TerminalSettings.c_iflag |= ICRNL;
    TerminalSettings.c_iflag &= ~IXOFF;
    tcsetattr(STDIN_FILENO, TCSANOW, &TerminalSettings);
    SwPrintLoginIssue(Context->IssuePath, SlavePath);

    //
    // Hand things over to login.
    //

    ExecArguments[0] = Context->LoginPath;
    ExecArguments[1] = NULL;
    execvp(ExecArguments[0], ExecArguments);

    //
    // Blech, the command could not be executed. Exit directly since this is
    // the child.
    //

    Status = errno;
    SwPrintError(Status, ExecArguments[0], "Could not exec");
    exit(Status);

CreateSessionEnd:
    if (PtySlave != -1) {
        close(PtySlave);
    }

    if (Status != 0) {
        if (Session != NULL) {
            if (Session->Pty != -1) {
                close(Session->Pty);
            }

            free(Session);
        }
    }

    return Status;
}

VOID
TelnetdDestroySession (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session
    )

/*++

Routine Description:

    This routine destroys a telnetd session.

Arguments:

    Context - Supplies a pointer to the application context.

    Session - Supplies a pointer to the session to tear down and free.

Return Value:

    None.

--*/

{

    LIST_REMOVE(&(Session->ListEntry));
    close(Session->Pty);
    if ((Context->Options & TELNETD_INETD_MODE) == 0) {
        close(Session->Input);
        if (Session->Input != Session->Output) {
            close(Session->Output);
        }
    }

    if (Session->PtyPoll != -1) {
        TelnetdReleasePollDescriptor(Context, Session->PtyPoll);
    }

    if (Session->InputPoll != -1) {
        TelnetdReleasePollDescriptor(Context, Session->InputPoll);
    }

    if ((Session->OutputPoll != -1) &&
        (Session->OutputPoll != Session->InputPoll)) {

        TelnetdReleasePollDescriptor(Context, Session->OutputPoll);
    }

    free(Session);
    return;
}

void
TelnetdChildSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine responds to child signal handlers.

Arguments:

    Signal - Supplies the signal number that occurred, SIGCHLD here.

Return Value:

    None.

--*/

{

    pid_t Child;
    int ChildStatus;
    PLIST_ENTRY CurrentEntry;
    int SavedErrno;
    PTELNETD_SESSION Session;

    SavedErrno = errno;

    //
    // Loop reaping children: more than one child death can collapse into one
    // signal.
    //

    while (TRUE) {
        Child = waitpid(-1, &ChildStatus, WNOHANG);
        if (Child <= 0) {
            break;
        }

        CurrentEntry = TelnetdContext->SessionList.Next;
        while (CurrentEntry != &(TelnetdContext->SessionList)) {
            Session = LIST_VALUE(CurrentEntry, TELNETD_SESSION, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (Session->Pid == Child) {
                Session->Pid = -1;
                SwUpdateUtmp(Child, DEAD_PROCESS, NULL, NULL, NULL);
                break;
            }
        }
    }

    errno = SavedErrno;
    return;
}

VOID
TelnetdWriteToTerminal (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session
    )

/*++

Routine Description:

    This routine writes data from the buffer to the pseudo-terminal.

Arguments:

    Context - Supplies a pointer to the application context.

    Session - Supplies a pointer to the session.

Return Value:

    None. If the write fails the session will be terminated automatically.

--*/

{

    PTELNETD_BUFFER Buffer;
    ULONG Command;
    ULONG Current;
    PUCHAR Data;
    ULONG End;
    ULONG Index;
    ULONG Option;
    UCHAR Previous;
    int Status;
    size_t ToWrite;
    UCHAR WindowData[4];
    ULONG WindowDataIndex;
    struct winsize WindowSize;
    ssize_t Written;

    Buffer = &(Session->ToPty);
    Data = Buffer->Data;
    Previous = 0;

    //
    // Loop while there are still things to do.
    //

    while (!TELNETD_BUFFER_EMPTY(Buffer)) {
        Index = Buffer->Consumer;
        End = Buffer->Producer;
        if (End < Index) {
            End = TELNETD_BUFFER_SIZE;
        }

        //
        // Loop over characters that are not IACs and is not the sequence \r\n.
        //

        Current = Index;
        while ((Current < End) && (Data[Current] != IAC) &&
               (!((Previous == '\r') && (Data[Current] == '\n')))) {

            Previous = Data[Current];
            Current += 1;
        }

        //
        // Write all the stuff that's not an IAC or the \n part of \r\n.
        //

        ToWrite = Current - Index;
        if (ToWrite != 0) {
            Written = TelnetdWrite(Session->Pty, Data + Index, ToWrite);
            if (Written <= 0) {

                //
                // If it's EAGAIN, this operation would block, so stop trying.
                // Otherwise, kill the session.
                //

                Status = errno;
                if (Status != EAGAIN) {
                    SwPrintError(Status, NULL, "Failed to write to terminal");
                    TelnetdKillSession(Context, Session);
                    Session = NULL;
                }

                break;
            }

            //
            // Try again if not everything was written, fixing up the previous
            // character.
            //

            Current = Index + Written;
            Buffer->Consumer = Current;
            if (Written != ToWrite) {
                Previous = Data[Index + Written - 1];
                Buffer->Consumer = Current;
                continue;
            }
        }

        //
        // If it stopped because it's at the end, update the pointers and start
        // over.
        //

        if (Current == End) {
            if (End == TELNETD_BUFFER_SIZE) {
                Buffer->Consumer = 0;
            }

            continue;
        }

        assert(Current < TELNETD_BUFFER_SIZE);

        //
        // If this was a \r\n, skip the \n part (the \r was already written).
        //

        if ((Data[Current] == '\n') && (Previous == '\r')) {
            Buffer->Consumer = TELNETD_BUFFER_ADVANCE(Current);
            Previous = '\n';
            continue;
        }

        //
        // There's definitely an IAC here. Skip it and handle the next byte. If
        // there is no next byte yet then leave it unhandled.
        //

        assert(Data[Current] == IAC);

        Current = TELNETD_BUFFER_ADVANCE(Current);
        if (Current == Buffer->Producer) {
            break;
        }

        Command = Data[Current];
        Current = TELNETD_BUFFER_ADVANCE(Current);

        //
        // Ignore a no-op and a subnegotiation end.
        //

        if ((Command == NOP) || (Command == SE)) {
            Buffer->Consumer = Current;
            continue;

        //
        // Handle a literal IAC.
        //

        } else if (Command == IAC) {
            Written = TelnetdWrite(Session->Pty, &Command, 1);
            if (Written < 0) {
                Status = errno;
                if (Status != EAGAIN) {
                    SwPrintError(Status, NULL, "Failed to write to terminal");
                    TelnetdKillSession(Context, Session);
                    Session = NULL;
                }

                break;

            } else {
                Buffer->Consumer = Current;
            }

        //
        // Handle a subnegotiation begin.
        //

        } else if (Command == SB) {
            if (Current == Buffer->Producer) {
                break;
            }

            Option = Data[Current];
            Current = TELNETD_BUFFER_ADVANCE(Current);

            //
            // Handle a set window size command.
            //

            if (Option == TELOPT_NAWS) {
                WindowDataIndex = 0;
                while ((WindowDataIndex < sizeof(WindowData)) &&
                       (Current != Buffer->Producer)) {

                    WindowData[WindowDataIndex] = Data[Current];
                    WindowDataIndex += 1;
                    Current = TELNETD_BUFFER_ADVANCE(Current);
                }

                if (WindowDataIndex == sizeof(WindowData)) {
                    WindowSize.ws_col = (WindowData[0] << 8) | WindowData[1];
                    WindowSize.ws_row = (WindowData[2] << 8) | WindowData[3];
                    ioctl(Session->Pty, TIOCSWINSZ, &WindowSize);
                    Buffer->Consumer = Current;

                } else {
                    break;
                }

            } else {
                SwPrintError(0, NULL, "Ignoring SB+%d", Option);
                Buffer->Consumer = Current;
            }

        //
        // Unknown IAC. Skip another byte and print it for the developers.
        //

        } else {
            if (Current == Buffer->Producer) {
                break;
            }

            Option = Data[Current];
            Current = TELNETD_BUFFER_ADVANCE(Current);
            if ((TELCMD_OK(Command)) && TELOPT_OK(Option)) {
                SwPrintError(0,
                             NULL,
                             "Ignoring IAC %s,%s",
                             TELCMD(Command),
                             TELOPT(Option));

            } else {
                SwPrintError(0,
                             NULL,
                             "Ignoring unknown IAC %d, %d",
                             Command,
                             Option);
            }

            Buffer->Consumer = Current;
        }
    }

    if (Session != NULL) {

        //
        // Input -> Buffer -> pty.
        //

        TelnetdUpdatePollBits(Buffer,
                              &(Context->Poll[Session->InputPoll]),
                              &(Context->Poll[Session->PtyPoll]));
    }

    return;
}

VOID
TelnetdWriteToSocket (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session
    )

/*++

Routine Description:

    This routine writes data from the buffer to the socket.

Arguments:

    Context - Supplies a pointer to the application context.

    Session - Supplies a pointer to the session.

Return Value:

    None. If the write fails the session will be terminated automatically.

--*/

{

    PTELNETD_BUFFER Buffer;
    ULONG Current;
    PUCHAR Data;
    UCHAR DoubleIac[2];
    ULONG End;
    ULONG Index;
    int Status;
    ssize_t Written;

    Buffer = &(Session->FromPty);
    Data = Buffer->Data;

    //
    // Loop while there are still things to do.
    //

    while (!TELNETD_BUFFER_EMPTY(Buffer)) {
        Index = Buffer->Consumer;
        End = Buffer->Producer;
        if (End < Index) {
            End = TELNETD_BUFFER_SIZE;
        }

        //
        // Loop over characters that are not IACs, which are most of them.
        //

        Current = Index;
        while ((Current < End) && (Data[Current] != IAC)) {
            Current += 1;
        }

        //
        // Write all the stuff that's not an IAC.
        //

        if (Current - Index != 0) {
            Written = TelnetdWrite(Session->Output,
                                   Data + Index,
                                   Current - Index);

            if (Written < 0) {

                //
                // If it's EAGAIN, this operation would block, so stop trying.
                // Otherwise, kill the session.
                //

                Status = errno;
                if (Status != EAGAIN) {
                    SwPrintError(Status, NULL, "Failed to write to socket");
                    TelnetdKillSession(Context, Session);
                    Session = NULL;
                }

                break;
            }

            Current = Index + Written;
            Buffer->Consumer = Current;
        }

        //
        // If it stopped because it's at the end, update the pointers and start
        // over.
        //

        if (Current == End) {
            if (End == TELNETD_BUFFER_SIZE) {
                Buffer->Consumer = 0;
            }

            continue;
        }

        assert(Current < TELNETD_BUFFER_SIZE);

        //
        // If the current byte is not an IAC (because the write performed less
        // than it was supposed to), then go loop around again.
        //

        if (Data[Current] != IAC) {
            continue;
        }

        //
        // There's an IAC here. Write out two so it's interpreted as a literal
        // IAC.
        //

        DoubleIac[0] = IAC;
        DoubleIac[1] = IAC;
        Written = TelnetdWrite(Session->Output, DoubleIac, 2);
        if (Written < 0) {
            Status = errno;
            if (Status != EAGAIN) {
                SwPrintError(Status, NULL, "Failed to write to socket");
                TelnetdKillSession(Context, Session);
                Session = NULL;
            }

            break;
        }

        Buffer->Consumer = TELNETD_BUFFER_ADVANCE(Current);
    }

    if (Session != NULL) {

        //
        // Pty -> Buffer -> output.
        //

        TelnetdUpdatePollBits(Buffer,
                              &(Context->Poll[Session->PtyPoll]),
                              &(Context->Poll[Session->OutputPoll]));
    }

    return;
}

VOID
TelnetdReadToBuffer (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session,
    int Descriptor,
    PTELNETD_BUFFER Buffer,
    struct pollfd *InputPoll,
    struct pollfd *OutputPoll
    )

/*++

Routine Description:

    This routine reads data from the socket or the pseudo-terminal into a
    buffer.

Arguments:

    Context - Supplies a pointer to the application context.

    Session - Supplies a pointer to the session.

    Descriptor - Supplies the descriptor to read from.

    Buffer - Supplies the buffer to read into.

    InputPoll - Supplies a pointer to the input poll descriptor to this buffer.

    OutputPoll - Supplies a pointer to the output poll descriptor to this
        buffer.

Return Value:

    None. If the read fails the session will be terminated automatically.

--*/

{

    PUCHAR Data;
    ssize_t Read;
    ULONG Size;
    INT Status;

    Data = Buffer->Data;

    //
    // Loop while the buffer is not completely full (a bit optimistic really).
    //

    while (!TELNETD_BUFFER_FULL(Buffer)) {
        if (Buffer->Producer >= Buffer->Consumer) {
            Size = TELNETD_BUFFER_SIZE - Buffer->Producer;

        } else {
            Size = (Buffer->Consumer - Buffer->Producer) - 1;
        }

        assert(Size != 0);

        Read = TelnetdRead(Descriptor, Data + Buffer->Producer, Size);
        if (Read <= 0) {
            Status = errno;
            if (Status != EAGAIN) {
                SwPrintError(Status, NULL, "Failed to read");
                TelnetdKillSession(Context, Session);
                Session = NULL;
            }

            break;
        }

        Buffer->Producer += Read;
        if (Buffer->Producer == TELNETD_BUFFER_SIZE) {
            Buffer->Producer = 0;
        }
    }

    //
    // Update the poll flags.
    //

    if (Session != NULL) {
        TelnetdUpdatePollBits(Buffer, InputPoll, OutputPoll);
    }

    return;
}

VOID
TelnetdUpdatePollBits (
    PTELNETD_BUFFER Buffer,
    struct pollfd *InputPoll,
    struct pollfd *OutputPoll
    )

/*++

Routine Description:

    This routine updates the buffer poll bits given the current buffer
    condition.

Arguments:

    Buffer - Supplies a pointer to the buffer to update based on.

    InputPoll - Supplies a pointer to the poll descriptor that produces data
        into the buffer.

    OutputPoll - Supplies a pointer to the poll descriptor that data from the
        buffer is written out to.

Return Value:

    None.

--*/

{

    //
    // If the buffer is not empty, then data can be written to the output
    // or terminal.
    //

    OutputPoll->events &= ~POLLOUT;
    if (!TELNETD_BUFFER_EMPTY(Buffer)) {
        OutputPoll->events |= POLLOUT;
    }

    //
    // If the buffer is not full, then more data can be read into it.
    //

    InputPoll->events &= ~POLLIN;
    if (!TELNETD_BUFFER_FULL(Buffer)) {
        InputPoll->events |= POLLIN;
    }

    return;
}

VOID
TelnetdKillSession (
    PTELNETD_CONTEXT Context,
    PTELNETD_SESSION Session
    )

/*++

Routine Description:

    This routine tears down a telnet session, updating utmp and destroying
    the session.

Arguments:

    Context - Supplies a pointer to the application context.

    Session - Supplies a pointer to the session.

Return Value:

    None.

--*/

{

    if (Session->Pid > 0) {
        SwUpdateUtmp(Session->Pid, DEAD_PROCESS, NULL, NULL, NULL);
    }

    TelnetdDestroySession(Context, Session);
    return;
}

VOID
TelnetdSetNonBlock (
    int Descriptor
    )

/*++

Routine Description:

    This routine sets the non-blocking flag on the given descriptor.

Arguments:

    Descriptor - Supplies the descriptor to set the O_NONBLOCK on.

Return Value:

    None.

--*/

{

    int Flags;

    Flags = fcntl(Descriptor, F_GETFL);
    if ((Flags & O_NONBLOCK) == 0) {
        fcntl(Descriptor, F_SETFL, Flags | O_NONBLOCK);
    }

    return;
}

ssize_t
TelnetdWrite (
    int FileDescriptor,
    const void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine attempts to write the specifed number of bytes to the given
    open file descriptor, retrying upon interrupt.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer containing the bytes to be
        written.

    ByteCount - Supplies the number of bytes to write.

Return Value:

    Returns the number of bytes successfully written to the file.

    -1 on failure, and errno will contain more information.

--*/

{

    ssize_t Result;

    do {
        Result = write(FileDescriptor, Buffer, ByteCount);

    } while ((Result == -1) && (errno == EINTR));

    return Result;
}

ssize_t
TelnetdRead (
    int FileDescriptor,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine attempts to read the specifed number of bytes from the given
    open file descriptor, retrying upon interrupt.

Arguments:

    FileDescriptor - Supplies the file descriptor returned by the open function.

    Buffer - Supplies a pointer to the buffer where the read bytes will be
        returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes successfully read from the file.

    -1 on failure, and errno will contain more information.

--*/

{

    ssize_t Result;

    do {
        Result = read(FileDescriptor, Buffer, ByteCount);

    } while ((Result == -1) && (errno == EINTR));

    return Result;
}

int
TelnetdAddPollDescriptor (
    PTELNETD_CONTEXT Context,
    int FileDescriptor
    )

/*++

Routine Description:

    This routine adds a poll descriptor to the array of descriptors to wait on.

Arguments:

    Context - Supplies a pointer to the application context.

    FileDescriptor - Supplies the descriptor to poll on.

Return Value:

    Returns the index of the poll descriptor in the poll array to use.

    -1 on allocation failure.

--*/

{

    ULONG Index;
    PVOID NewBuffer;
    ULONG NewCapacity;
    struct pollfd *Poll;

    //
    // First look for a free one.
    //

    for (Index = 0; Index < Context->PollCount; Index += 1) {
        Poll = &(Context->Poll[Index]);
        if (Poll->fd < 0) {
            break;
        }
    }

    //
    // If a new one is needed, the whole array may need to be reallocated.
    //

    if (Index == Context->PollCount) {
        if (Context->PollCount <= Context->PollCapacity) {
            if (Context->PollCapacity == 0) {
                NewCapacity = 8;

            } else {
                NewCapacity = Context->PollCapacity * 2;
            }

            NewBuffer = realloc(Context->Poll,
                                NewCapacity * sizeof(struct pollfd));

            if (NewBuffer == NULL) {
                return -1;
            }

            Context->Poll = NewBuffer;
            Context->PollCapacity = NewCapacity;
        }

        Poll = &(Context->Poll[Context->PollCount]);
        Context->PollCount += 1;
    }

    Poll->fd = FileDescriptor;
    Poll->events = 0;
    Poll->revents = 0;
    return Index;
}

void
TelnetdReleasePollDescriptor (
    PTELNETD_CONTEXT Context,
    int Index
    )

/*++

Routine Description:

    This routine releases a poll descriptor.

Arguments:

    Context - Supplies a pointer to the application context.

    Index - Supplies the index in the array of poll descriptors to release.

Return Value:

    None.

--*/

{

    struct pollfd *Poll;

    assert(Index < Context->PollCount);

    Poll = &(Context->Poll[Index]);
    Poll->fd = -1;

    //
    // Optimistically shrink the array if possible.
    //

    while ((Context->PollCount != 0) &&
           (Context->Poll[Context->PollCount - 1].fd < 0)) {

        Context->PollCount -= 1;
    }

    return;
}

