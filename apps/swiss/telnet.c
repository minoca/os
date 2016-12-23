/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    telnet.c

Abstract:

    This module implements a very simple telnet client.

Author:

    Evan Green 4-Oct-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Add some definitions to get the strings out of telnet.h. The telnet daemon
// also relies on the client having the string declarations here.
//

#define TELCMDS 1

#include <minoca/lib/types.h>

#include <arpa/telnet.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "swlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define TELNET_VERSION_MAJOR 1
#define TELNET_VERSION_MINOR 0

#define TELNET_USAGE                                                           \
    "usage: telnet [-l user] host [port]\n"                                    \
    "The telnet utility implements a simple telnet client. Options are:\n"     \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define TELNET_OPTIONS_STRING "l:hv"

#define TELNET_ENTERING_MODE_FORMAT \
    "\nEntering %s mode. Escape character is %s.\n"

#define TELNET_BUFFER_SIZE 256
#define TELNET_IAC_BUFFER_SIZE 64

//
// Define the escape character, ^].
//

#define TELNET_ESCAPE 0x1D

#define TELNET_FLAG_ECHO 0x00000001
#define TELNET_FLAG_SUPPRESS_GO_AHEAD 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TELNET_STATE {
    TelnetStateInvalid,
    TelnetStateNormal,
    TelnetStateCopy,
    TelnetStateIac,
    TelnetStateOption,
    TelnetStateSubnegotiation1,
    TelnetStateSubnegotiation2,
    TelnetStateCr
} TELNET_STATE, *PTELNET_STATE;

typedef enum _TELNET_CHARACTER_MODE {
    TelnetCharacterModeTry,
    TelnetCharacterModeOn,
    TelnetCharacterModeOff
} TELNET_CHARACTER_MODE, *PTELNET_CHARACTER_MODE;

/*++

Structure Description:

    This structure defines the context for an instantiation of the telnet
    client.

Members:

    WindowHeight - Stores the window height in characters.

    WindowWidth - Stores the window width in characters.

    TerminalType - Stores the terminal type.

    Socket - Stores a pointer to the socket.

    Poll - Stores the poll descriptors.

    Sigint - Stores a boolean indicating if a pending sigint occurred.

    Exit - Stores a boolean indicating an exit is requested.

    Buffer - Stores the data buffer.

    State - Stores the current telnet command state.

    Wish - Stores the option that the remote side is asking/telling this client
        about.

    Flags - Stores telnet flags. See TELNET_FLAG_* definitions.

    CharacterMode - Stores the character mode negotiation state.

    IacBuffer - Stores the buffer used to send telnet options and
        subnegotiations.

    IacSize - Stores the size of the IAC buffer.

--*/

typedef struct _TELNET_CONTEXT {
    int WindowHeight;
    int WindowWidth;
    PSTR TerminalType;
    int Socket;
    struct pollfd Poll[2];
    BOOL Sigint;
    BOOL Exit;
    UCHAR Buffer[TELNET_BUFFER_SIZE];
    TELNET_STATE State;
    UCHAR Wish;
    ULONG Flags;
    TELNET_CHARACTER_MODE CharacterMode;
    UCHAR IacBuffer[TELNET_IAC_BUFFER_SIZE];
    ULONG IacSize;
} TELNET_CONTEXT, *PTELNET_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

void
TelnetSigintHandler (
    int Signal
    );

INT
TelnetWriteToSocket (
    PTELNET_CONTEXT Context,
    size_t Size
    );

INT
TelnetWriteToOutput (
    PTELNET_CONTEXT Context,
    size_t Size
    );

INT
TelnetWrite (
    int Descriptor,
    void *Buffer,
    size_t Size
    );

INT
TelnetEscape (
    PTELNET_CONTEXT Context
    );

VOID
TelnetHandleOption (
    PTELNET_CONTEXT Context,
    CHAR Option
    );

VOID
TelnetHandleSubnegotiation (
    PTELNET_CONTEXT Context,
    CHAR Option
    );

VOID
TelnetHandleEchoOption (
    PTELNET_CONTEXT Context
    );

VOID
TelnetHandleSgaOption (
    PTELNET_CONTEXT Context
    );

VOID
TelnetHandleTTypeOption (
    PTELNET_CONTEXT Context
    );

VOID
TelnetHandleNawsOption (
    PTELNET_CONTEXT Context
    );

VOID
TelnetHandleUnsupportedOption (
    PTELNET_CONTEXT Context,
    CHAR Option
    );

VOID
TelnetSendWindowSize (
    PTELNET_CONTEXT Context,
    UCHAR Option,
    INT Width,
    INT Height
    );

VOID
TelnetSendSuboptIac (
    PTELNET_CONTEXT Context,
    UCHAR Option,
    PSTR String
    );

VOID
TelnetSendDoLineMode (
    PTELNET_CONTEXT Context
    );

VOID
TelnetSendWillCharMode (
    PTELNET_CONTEXT Context
    );

VOID
TelnetSetConsoleMode (
    PTELNET_CONTEXT Context
    );

VOID
TelnetAddIacWish (
    PTELNET_CONTEXT Context,
    UCHAR Wish,
    UCHAR Option
    );

VOID
TelnetAddIac (
    PTELNET_CONTEXT Context,
    UCHAR Character
    );

VOID
TelnetFlushIacs (
    PTELNET_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

struct option TelnetLongOptions[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

PTELNET_CONTEXT TelnetGlobalContext;

//
// ------------------------------------------------------------------ Functions
//

INT
TelnetMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the telnet utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    struct sigaction Action;
    struct addrinfo *Address;
    struct addrinfo *AddressInfo;
    ULONG ArgumentIndex;
    ssize_t BytesDone;
    TELNET_CONTEXT Context;
    PSTR Host;
    INT Option;
    struct sigaction OriginalAction;
    PSTR PortString;
    int Status;
    int Value;

    AddressInfo = NULL;
    memset(&Context, 0, sizeof(TELNET_CONTEXT));
    Context.State = TelnetStateNormal;
    Context.Socket = -1;
    Context.TerminalType = getenv("TERM");
    SwGetTerminalDimensions(&(Context.WindowWidth), &(Context.WindowHeight));
    SwSetRawInputMode(NULL, NULL);
    TelnetGlobalContext = &Context;
    memset(&Action, 0, sizeof(struct sigaction));
    Action.sa_handler = TelnetSigintHandler;
    sigaction(SIGINT, &Action, &OriginalAction);

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TELNET_OPTIONS_STRING,
                             TelnetLongOptions,
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
            SwPrintVersion(TELNET_VERSION_MAJOR, TELNET_VERSION_MINOR);
            return 1;

        case 'h':
            printf(TELNET_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0, NULL, "Argument required. Try --help for usage");
        Status = 1;
        goto MainEnd;
    }

    PortString = "23";
    Host = Arguments[ArgumentIndex];
    ArgumentIndex += 1;
    if (ArgumentIndex < ArgumentCount) {
        PortString = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        if (ArgumentIndex < ArgumentCount) {
            SwPrintError(0, NULL, "Too many arguments");
            Status = 1;
            goto MainEnd;
        }
    }

    Status = getaddrinfo(Host, PortString, NULL, &AddressInfo);
    if (Status != 0) {
        SwPrintError(0,
                     NULL,
                     "Cannot resolve %s:%s: %s.\n",
                     Host,
                     PortString,
                     gai_strerror(Status));

        Status = errno;
        if (Status == 0) {
            Status = 1;
        }

        goto MainEnd;
    }

    Address = AddressInfo;
    while (Address != NULL) {
        if (Address->ai_socktype == SOCK_STREAM) {
            Context.Socket = socket(Address->ai_family,
                                    Address->ai_socktype,
                                    Address->ai_protocol);

            if (Context.Socket < 0) {
                Status = errno;
                SwPrintError(Status, NULL, "Unable to create socket");
                goto MainEnd;
            }

            Status = connect(Context.Socket,
                             Address->ai_addr,
                             Address->ai_addrlen);

            if (Status != 0) {
                Status = errno;
                SwPrintError(Status, Host, "Unable to connect");
                close(Context.Socket);
                Context.Socket = -1;
            }

            break;
        }

        Address = Address->ai_next;
    }

    if (Context.Socket < 0) {
        Status = 1;
        SwPrintError(0, Host, "Connection failed");
        goto MainEnd;
    }

    Value = 1;
    setsockopt(Context.Socket, SOL_SOCKET, SO_KEEPALIVE, &Value, sizeof(Value));
    Context.Poll[0].fd = STDIN_FILENO;
    Context.Poll[0].events = POLLIN;
    Context.Poll[1].fd = Context.Socket;
    Context.Poll[1].events = POLLIN;

    //
    // This is the main loop, which shuttles data.
    //

    while (Context.Exit == FALSE) {
        if (poll(Context.Poll, 2, -1) < 0) {
            if (Context.Sigint != 0) {
                TelnetEscape(&Context);

            } else {
                if (errno != 0) {
                    Status = errno;
                    if (errno == EINTR) {
                        continue;
                    }

                    SwPrintError(Status, NULL, "Error");
                    break;
                }
            }
        }

        //
        // Check standard in heading out to the socket.
        //

        if ((Context.Poll[0].revents & POLLIN) != 0) {
            do {
                BytesDone = read(STDIN_FILENO,
                                 Context.Buffer,
                                 TELNET_BUFFER_SIZE);

            } while ((BytesDone <= 0) && (errno == EINTR));

            if (BytesDone == 0) {
                break;

            } else if (BytesDone < 0) {
                Status = errno;
                SwPrintError(Status, NULL, "Failed to read");
                goto MainEnd;
            }

            TelnetWriteToSocket(&Context, BytesDone);
        }

        //
        // Check the socket headed off to standard out.
        //

        if ((Context.Poll[1].revents & POLLIN) != 0) {
            do {
                BytesDone = read(Context.Socket,
                                 Context.Buffer,
                                 TELNET_BUFFER_SIZE);

            } while ((BytesDone <= 0) && (errno == EINTR));

            if (BytesDone == 0) {
                break;

            } else if (BytesDone < 0) {
                Status = errno;
                SwPrintError(Status, NULL, "Failed to read");
                goto MainEnd;
            }

            TelnetWriteToOutput(&Context, BytesDone);
        }
    }

    shutdown(Context.Socket, SHUT_RDWR);

MainEnd:
    sigaction(SIGINT, &OriginalAction, NULL);
    TelnetGlobalContext = NULL;
    if (Context.Socket >= 0) {
        close(Context.Socket);
    }

    if (AddressInfo != NULL) {
        freeaddrinfo(AddressInfo);
    }

    SwRestoreInputMode();
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

void
TelnetSigintHandler (
    int Signal
    )

/*++

Routine Description:

    This routine handles SIGINT signals, simply marking that they occurred.

Arguments:

    Signal - Supplies the signal number that fired, which in this case is
        SIGINT.

Return Value:

    None.

--*/

{

    TelnetGlobalContext->Sigint = TRUE;
    return;
}

INT
TelnetWriteToSocket (
    PTELNET_CONTEXT Context,
    size_t Size
    )

/*++

Routine Description:

    This routine writes data from standard input out to the socket,
    transforming to work with telnet as needed.

Arguments:

    Context - Supplies a pointer to the application context.

    Size - Supplies the number of valid bytes in the buffer.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    UCHAR Character;
    UINTN Index;
    PUCHAR Input;
    UCHAR Output[TELNET_BUFFER_SIZE * 2];
    size_t OutSize;
    INT Status;

    Input = Context->Buffer;
    OutSize = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Character = *Input;
        if (Character == TELNET_ESCAPE) {
            Status = TelnetEscape(Context);
            if ((Status != 0) || (Context->Exit != FALSE)) {
                return Status;
            }

            continue;
        }

        Output[OutSize] = Character;
        OutSize += 1;

        //
        // IACs should be doubled going out so they're literal.
        //

        if (Character == IAC) {
            Output[OutSize] = Character;
            OutSize += 1;

        //
        // Convert \r to \r\0.
        //

        } else if (Character == '\r') {
            Output[OutSize] = '\0';
            OutSize += 1;
        }
    }

    if (OutSize > 0) {
        return TelnetWrite(Context->Socket, Output, Size);
    }

    return 0;
}

INT
TelnetWriteToOutput (
    PTELNET_CONTEXT Context,
    size_t Size
    )

/*++

Routine Description:

    This routine writes data from the socket to standard out, transforming to
    work with telnet as needed.

Arguments:

    Context - Supplies a pointer to the application context.

    Size - Supplies the number of valid bytes in the buffer.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    UCHAR Character;
    size_t CopyStart;
    size_t Index;

    CopyStart = 0;
    for (Index = 0; Index < Size; Index += 1) {
        Character = Context->Buffer[Index];
        if (Context->State == TelnetStateNormal) {
            if (Character == IAC) {
                CopyStart = Index;
                Context->State = TelnetStateIac;

            } else if (Character == '\r') {
                CopyStart = Index + 1;
                Context->State = TelnetStateCr;
            }

            continue;
        }

        switch (Context->State) {

        //
        // If the previous character was a carriage return and this one is
        // null, ignore it.
        //

        case TelnetStateCr:
            Context->State = TelnetStateCopy;
            if (Character == '\0') {
                break;
            }

            //
            // Fall through.
            //

        case TelnetStateCopy:

            //
            // In copy mode, move bytes backwards towards earlier in the buffer
            // over bytes that were skipped.
            //

            if (Character == IAC) {
                Context->State = TelnetStateIac;

            } else {
                Context->Buffer[CopyStart] = Character;
                CopyStart += 1;
            }

            if (Character == '\r') {
                Context->State = TelnetStateCr;
            }

            break;

        case TelnetStateIac:

            //
            // Convert a double IAC down to just one IAC.
            //

            if (Character == IAC) {
                Context->Buffer[CopyStart] = Character;
                CopyStart += 1;
                Context->State = TelnetStateCopy;

            } else {
                switch (Character) {
                case SB:
                    Context->State = TelnetStateSubnegotiation1;
                    break;

                case DO:
                case DONT:
                case WILL:
                case WONT:
                    Context->Wish = Character;
                    Context->State = TelnetStateOption;
                    break;

                default:
                    Context->State = TelnetStateCopy;
                    break;
                }
            }

            break;

        case TelnetStateOption:
            TelnetHandleOption(Context, Character);
            Context->State = TelnetStateCopy;
            break;

        case TelnetStateSubnegotiation1:
        case TelnetStateSubnegotiation2:
            TelnetHandleSubnegotiation(Context, Character);
            break;

        default:

            assert(FALSE);

            return EINVAL;
        }
    }

    if (Context->State != TelnetStateNormal) {
        if (Context->IacSize != 0) {
            TelnetFlushIacs(Context);
        }

        if (Context->State == TelnetStateCopy) {
            Context->State = TelnetStateNormal;
        }

        Size = CopyStart;
    }

    if (Size != 0) {
        return TelnetWrite(STDOUT_FILENO, Context->Buffer, Size);
    }

    return 0;
}

INT
TelnetWrite (
    int Descriptor,
    void *Buffer,
    size_t Size
    )

/*++

Routine Description:

    This routine writes the given data out to the given file descriptor in full.

Arguments:

    Descriptor - Supplies the descriptor to write to.

    Buffer - Supplies a pointer to the buffer to write.

    Size - Supplies the number of bytes to write.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT Status;
    ssize_t Written;

    while (Size != 0) {
        Written = write(Descriptor, Buffer, Size);
        if (Written <= 0) {
            if (errno == EINTR) {
                continue;
            }

            Status = errno;
            SwPrintError(Status, NULL, "Failed to write");
            return Status;
        }

        Buffer += Written;
        Size -= Written;
    }

    return 0;
}

INT
TelnetEscape (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles local telnet interaction.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ssize_t BytesRead;
    CHAR Character;
    INT Status;

    if (Context->Sigint != FALSE) {
        SwSetRawInputMode(NULL, NULL);
    }

    printf("\nConsole escape:\n"
           " l - Set line mode.\n"
           " c - Set character mode.\n"
           " z - Suspend telnet.\n"
           " e - Exit telnet.\n\n"
           "telnet> ");

    do {
        BytesRead = read(STDIN_FILENO, &Character, 1);

    } while ((BytesRead < 0) && (errno == EINTR));

    if (BytesRead <= 0) {
        Status = errno;
        SwPrintError(Status, NULL, "Failed to read");
        return Status;
    }

    switch (Character) {
    case 'l':
        TelnetSendDoLineMode(Context);
        goto TelnetEscapeEnd;

    case 'c':
        TelnetSendWillCharMode(Context);
        goto TelnetEscapeEnd;

    case 'z':
        SwRestoreInputMode();
        kill(0, SIGTSTP);
        SwSetRawInputMode(NULL, NULL);
        break;

    case 'e':
        Context->Exit = TRUE;
        goto TelnetEscapeEnd;
    }

    printf("Continuing...\n");
    if (Context->Sigint != FALSE) {
        SwRestoreInputMode();
    }

TelnetEscapeEnd:
    Context->Sigint = FALSE;
    return 0;
}

VOID
TelnetHandleOption (
    PTELNET_CONTEXT Context,
    CHAR Option
    )

/*++

Routine Description:

    This routine handles an incoming telnet option.

Arguments:

    Context - Supplies a pointer to the application context.

    Option - Supplies the option to handle.

Return Value:

    None.

--*/

{

    switch (Option) {
    case TELOPT_ECHO:
        TelnetHandleEchoOption(Context);
        break;

    case TELOPT_SGA:
        TelnetHandleSgaOption(Context);
        break;

    case TELOPT_TTYPE:
        TelnetHandleTTypeOption(Context);
        break;

    case TELOPT_NAWS:
        TelnetHandleNawsOption(Context);
        TelnetSendWindowSize(Context,
                             Option,
                             Context->WindowWidth,
                             Context->WindowHeight);

        break;

    default:
        TelnetHandleUnsupportedOption(Context, Option);
        break;
    }

    return;
}

VOID
TelnetHandleSubnegotiation (
    PTELNET_CONTEXT Context,
    CHAR Option
    )

/*++

Routine Description:

    This routine handles a telnet subnegotiation.

Arguments:

    Context - Supplies a pointer to the application context.

    Option - Supplies the subnegotiation option.

Return Value:

    None.

--*/

{

    switch (Context->State) {
    case TelnetStateSubnegotiation1:
        if ((UCHAR)Option == IAC) {
            Context->State = TelnetStateSubnegotiation2;
            break;
        }

        if ((Option == TELOPT_TTYPE) && (Context->TerminalType != NULL)) {
            TelnetSendSuboptIac(Context,
                                TELOPT_TTYPE,
                                Context->TerminalType);
        }

        break;

    case TelnetStateSubnegotiation2:
        if ((UCHAR)Option == SE) {
            Context->State = TelnetStateCopy;
            break;
        }

        Context->State = TelnetStateSubnegotiation1;
        break;

    default:

        assert(FALSE);

        break;
    }

    return;
}

VOID
TelnetHandleEchoOption (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles an incoming telnet echo option.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    //
    // If the server wants the client to echo, refuse to do it.
    //

    if (Context->Wish == DO) {
        TelnetAddIacWish(Context, WONT, TELOPT_ECHO);
        return;
    }

    if (Context->Wish == DONT) {
        return;
    }

    //
    // Do nothing if they already agree with what the client is doing.
    //

    if ((Context->Flags & TELNET_FLAG_ECHO) != 0) {
        if (Context->Wish == WILL) {
            return;
        }

    } else if (Context->Wish == WONT) {
        return;
    }

    if (Context->CharacterMode != TelnetCharacterModeOff) {
        Context->Flags ^= TELNET_FLAG_ECHO;
    }

    if ((Context->Flags & TELNET_FLAG_ECHO) != 0) {
        TelnetAddIacWish(Context, DO, TELOPT_ECHO);

    } else {
        TelnetAddIacWish(Context, DONT, TELOPT_ECHO);
    }

    TelnetSetConsoleMode(Context);
    printf("\n");
    return;
}

VOID
TelnetHandleSgaOption (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles an incoming telnet Suppress Go Ahead option.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    //
    // Do nothing if it already agrees.
    //

    if ((Context->Flags & TELNET_FLAG_SUPPRESS_GO_AHEAD) != 0) {
        if (Context->Wish == WILL) {
            return;
        }

    } else if (Context->Wish == WONT) {
        return;
    }

    Context->Flags ^= TELNET_FLAG_SUPPRESS_GO_AHEAD;
    if ((Context->Flags & TELNET_FLAG_SUPPRESS_GO_AHEAD) != 0) {
        TelnetAddIacWish(Context, DO, TELOPT_SGA);

    } else {
        TelnetAddIacWish(Context, DONT, TELOPT_SGA);
    }

    return;
}

VOID
TelnetHandleTTypeOption (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles an incoming telnet terminal type option.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    if (Context->TerminalType != NULL) {
        TelnetAddIacWish(Context, WILL, TELOPT_TTYPE);

    } else {
        TelnetAddIacWish(Context, WONT, TELOPT_TTYPE);
    }

    return;
}

VOID
TelnetHandleNawsOption (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles an incoming telnet window size option.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    TelnetAddIacWish(Context, WILL, TELOPT_NAWS);
    return;
}

VOID
TelnetHandleUnsupportedOption (
    PTELNET_CONTEXT Context,
    CHAR Option
    )

/*++

Routine Description:

    This routine handles an incoming telnet option that this client does not
    recognize.

Arguments:

    Context - Supplies a pointer to the application context.

    Option - Supplies the unsupported option.

Return Value:

    None.

--*/

{

    if (Context->Wish == WILL) {
        TelnetAddIacWish(Context, WONT, Option);

    } else {
        TelnetAddIacWish(Context, DONT, Option);
    }

    return;
}

VOID
TelnetSendWindowSize (
    PTELNET_CONTEXT Context,
    UCHAR Option,
    INT Width,
    INT Height
    )

/*++

Routine Description:

    This routine sends a window size IAC to the server.

Arguments:

    Context - Supplies a pointer to the application context.

    Option - Supplies the option byte received.

    Width - Supplies the window width in characters.

    Height - Supplies the window height in characters.

Return Value:

    None.

--*/

{

    if (Context->IacSize + 9 > TELNET_IAC_BUFFER_SIZE) {
        TelnetFlushIacs(Context);
    }

    TelnetAddIac(Context, IAC);
    TelnetAddIac(Context, SB);
    TelnetAddIac(Context, Option);
    TelnetAddIac(Context, (Width >> BITS_PER_BYTE) & 0xFF);
    TelnetAddIac(Context, Width & 0xFF);
    TelnetAddIac(Context, (Height >> BITS_PER_BYTE) & 0xFF);
    TelnetAddIac(Context, Height & 0xFF);
    TelnetAddIac(Context, IAC);
    TelnetAddIac(Context, SE);
    return;
}

VOID
TelnetSendSuboptIac (
    PTELNET_CONTEXT Context,
    UCHAR Option,
    PSTR String
    )

/*++

Routine Description:

    This routine sends a suboption IAC control sequence.

Arguments:

    Context - Supplies a pointer to the application context.

    Option - Supplies the option byte to send.

    String - Supplies a pointer to a null-terminated string to send as the
        argument.

Return Value:

    None.

--*/

{

    ULONG Length;

    Length = strlen(String) + 6;
    if (Context->IacSize + Length > TELNET_IAC_BUFFER_SIZE) {
        TelnetFlushIacs(Context);
    }

    TelnetAddIac(Context, IAC);
    TelnetAddIac(Context, SB);
    TelnetAddIac(Context, Option);
    TelnetAddIac(Context, 0);
    while (*String != '\0') {
        TelnetAddIac(Context, *String);
        String += 1;
    }

    TelnetAddIac(Context, IAC);
    TelnetAddIac(Context, SE);
    return;
}

VOID
TelnetSendDoLineMode (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles transitioning the client to line mode.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    Context->CharacterMode = TelnetCharacterModeTry;
    Context->Flags &= ~(TELNET_FLAG_ECHO | TELNET_FLAG_SUPPRESS_GO_AHEAD);
    TelnetSetConsoleMode(Context);
    TelnetAddIacWish(Context, DONT, TELOPT_ECHO);
    TelnetAddIacWish(Context, DONT, TELOPT_SGA);
    TelnetFlushIacs(Context);
    return;
}

VOID
TelnetSendWillCharMode (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine handles transitioning the client to character mode.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    Context->CharacterMode = TelnetCharacterModeTry;
    Context->Flags |= TELNET_FLAG_ECHO | TELNET_FLAG_SUPPRESS_GO_AHEAD;
    TelnetSetConsoleMode(Context);
    TelnetAddIacWish(Context, DO, TELOPT_ECHO);
    TelnetAddIacWish(Context, DO, TELOPT_SGA);
    TelnetFlushIacs(Context);
    return;
}

VOID
TelnetSetConsoleMode (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine sets the current console mode to either raw mode or cooked
    mode depending on the flags in the context.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    if ((Context->Flags & TELNET_FLAG_ECHO) != 0) {
        if (Context->CharacterMode == TelnetCharacterModeTry) {
            Context->CharacterMode = TelnetCharacterModeOn;
            printf(TELNET_ENTERING_MODE_FORMAT, "character", "^]");
            SwSetRawInputMode(NULL, NULL);
        }

    } else {
        if (Context->CharacterMode != TelnetCharacterModeOff) {
            Context->CharacterMode = TelnetCharacterModeOff;
            printf(TELNET_ENTERING_MODE_FORMAT, "line", "^C");
            SwRestoreInputMode();
        }
    }

    return;
}

VOID
TelnetAddIacWish (
    PTELNET_CONTEXT Context,
    UCHAR Wish,
    UCHAR Option
    )

/*++

Routine Description:

    This routine sends an IAC DO/DONT/WILL/WONT sequence to the outgoing IAC
    buffer.

Arguments:

    Context - Supplies a pointer to the application context.

    Wish - Supplies the wish, which should be DO, DONT, WILL, or WONT (defined
        in telnet.h).

    Option - Supplies the option to wish for or against.

Return Value:

    None.

--*/

{

    if (Context->IacSize + 3 > TELNET_IAC_BUFFER_SIZE) {
        TelnetFlushIacs(Context);
    }

    TelnetAddIac(Context, IAC);
    TelnetAddIac(Context, Wish);
    TelnetAddIac(Context, Option);
    return;
}

VOID
TelnetAddIac (
    PTELNET_CONTEXT Context,
    UCHAR Character
    )

/*++

Routine Description:

    This routine adds a character to the IAC buffer.

Arguments:

    Context - Supplies a pointer to the application context.

    Character - Supplies the character to add.

Return Value:

    None.

--*/

{

    if (Context->IacSize >= TELNET_IAC_BUFFER_SIZE) {
        TelnetFlushIacs(Context);
    }

    assert(Context->IacSize < TELNET_IAC_BUFFER_SIZE);

    Context->IacBuffer[Context->IacSize] = Character;
    Context->IacSize += 1;
    return;
}

VOID
TelnetFlushIacs (
    PTELNET_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes the IAC buffer out to the socket.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    TelnetWrite(Context->Socket, Context->IacBuffer, Context->IacSize);
    Context->IacSize = 0;
    return;
}

