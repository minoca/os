/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    commos.c

Abstract:

    This module implements the common POSIX-like debugger functionality.

Author:

    Evan Green 20-Mar-2017

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <minoca/lib/types.h>
#include <minoca/lib/tty.h>
#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "dbgrprof.h"
#include "console.h"
#include "sock.h"

//
// ---------------------------------------------------------------- Definitions
//

#ifndef O_BINARY
#define O_BINARY 0x0000
#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void
DbgrConsoleInterruptHandler (
    int Signal
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the file descriptor of the open kernel serial connection.
//

int DbgKdDescriptor = -1;
struct termios DbgOriginalKdSettings;

//
// Store the ID of the terminals' initial process group.
//

pid_t DbgInitialTerminalInputForegroundProcessGroup;
pid_t DbgInitialTerminalOutputForegroundProcessGroup;
pid_t DbgInitialTerminalErrorForegroundProcessGroup;

//
// Store the desired terminal parameters and the previous terminal parameters.
//

struct termios DbgTerminalSettings;
struct termios DbgOriginalTerminalSettings;
struct sigaction DbgOriginalSigint;

//
// Store the ID of the terminal's original foreground process group.
//

pid_t DbgOriginalTerminalForegroundProcessGroupId;

//
// Store the remote pipe.
//

int DbgRemoteInputPipe[2] = {-1, -1};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the program. It collects the
    options passed to it, and creates the output image.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    return DbgrMain(ArgumentCount, Arguments);
}

BOOL
DbgrOsInitializeConsole (
    PBOOL EchoCommands
    )

/*++

Routine Description:

    This routine performs any initialization steps necessary before the console
    can be used.

Arguments:

    EchoCommands - Supplies a pointer where a boolean will be returned
        indicating if the debugger should echo commands received (TRUE) or if
        the console has already echoed the command (FALSE).

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    struct sigaction Action;
    int Result;

    if (tcgetattr(STDIN_FILENO, &DbgTerminalSettings) != 0) {
        perror("Cannot get terminal settings");
        return FALSE;
    }

    //
    // Set 8 bit characters.
    //

    DbgTerminalSettings.c_cflag |= CS8;

    //
    // Change the local mode to enable canonical mode, echo, erase, extended
    // functions, and signal characters.
    //

    DbgTerminalSettings.c_lflag |= ECHO | ICANON | ISIG | ECHONL;
    DbgInitialTerminalInputForegroundProcessGroup = tcgetpgrp(STDIN_FILENO);
    DbgInitialTerminalOutputForegroundProcessGroup = tcgetpgrp(STDOUT_FILENO);
    DbgInitialTerminalErrorForegroundProcessGroup = tcgetpgrp(STDERR_FILENO);
    Result = pipe(DbgRemoteInputPipe);
    if (Result != 0) {
        return FALSE;
    }

    //
    // Set the Control+C handler.
    //

    memset(&Action, 0, sizeof(Action));
    Action.sa_handler = DbgrConsoleInterruptHandler;
    sigaction(SIGINT, &Action, &DbgOriginalSigint);
    return TRUE;
}

VOID
DbgrOsDestroyConsole (
    VOID
    )

/*++

Routine Description:

    This routine cleans up anything related to console functionality as a
    debugger is exiting.

Arguments:

    None.

Return Value:

    None.

--*/

{

    struct sigaction OriginalAction;
    int Result;
    struct sigaction SignalAction;

    sigaction(SIGINT, &DbgOriginalSigint, NULL);

    //
    // Temporarily ignore SIGTTOU as the current process may not be in the
    // foreground process group, causing the SIGTTOU signal to fire.
    //

    memset(&SignalAction, 0, sizeof(struct sigaction));
    SignalAction.sa_handler = SIG_IGN;
    Result = sigaction(SIGTTOU, &SignalAction, &OriginalAction);
    if (Result == 0) {
        tcsetpgrp(STDIN_FILENO, DbgInitialTerminalInputForegroundProcessGroup);
        tcsetpgrp(STDOUT_FILENO,
                  DbgInitialTerminalOutputForegroundProcessGroup);

        tcsetpgrp(STDERR_FILENO, DbgInitialTerminalErrorForegroundProcessGroup);
        sigaction(SIGTTOU, &OriginalAction, NULL);
    }

    if (DbgRemoteInputPipe[0] != -1) {
        close(DbgRemoteInputPipe[0]);
    }

    if (DbgRemoteInputPipe[1] != -1) {
        close(DbgRemoteInputPipe[1]);
    }

    return;
}

INT
DbgrOsCreateThread (
    PDBGR_THREAD_ROUTINE ThreadRoutine,
    PVOID Parameter
    )

/*++

Routine Description:

    This routine creates a new thread.

Arguments:

    ThreadRoutine - Supplies a pointer to the routine to run in the new thread.
        The thread is destroyed when the supplied routine returns.

    Parameter - Supplies a pointer to a parameter to pass to the thread.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    int Result;
    pthread_t Thread;

    Result = pthread_create(&Thread, NULL, ThreadRoutine, Parameter);
    if (Result == 0) {
        pthread_detach(Thread);
    }

    return Result;
}

int
DbgrOsCreatePipe (
    int FileDescriptors[2]
    )

/*++

Routine Description:

    This routine creates an anonymous pipe.

Arguments:

    FileDescriptors - Supplies a pointer where handles will be returned
        representing the read and write ends of the pipe.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    return pipe(FileDescriptors);
}

char *
DbgrOsGetUserName (
    void
    )

/*++

Routine Description:

    This routine returns the user name of the current process.

Arguments:

    None.

Return Value:

    Returns a pointer to a string containing the user name if it can be found.
    The caller should not free or modify this memory, and it may be reused on
    subsequent calls.

--*/

{

    struct passwd *Information;

    Information = getpwuid(geteuid());
    if ((Information == NULL) || (Information->pw_name == NULL) ||
        (*(Information->pw_name) == '\0')) {

        return getenv("USER");
    }

    return Information->pw_name;
}

char *
DbgrOsGetHostName (
    void
    )

/*++

Routine Description:

    This routine returns the host name of the current machine.

Arguments:

    None.

Return Value:

    Returns a pointer to a string containing the user name if it can be found.
    The caller is responsible for freeing this memory.

--*/

{

    char LocalHost[100];
    int Result;

    Result = gethostname(LocalHost, sizeof(LocalHost));
    if (Result != 0) {
        return NULL;
    }

    return strdup(LocalHost);
}

VOID
DbgrOsPrepareToReadInput (
    VOID
    )

/*++

Routine Description:

    This routine is called before the debugger begins to read a line of input
    from the user.

Arguments:

    None.

Return Value:

    None.

--*/

{

    struct sigaction OriginalAction;
    int Result;
    struct sigaction SignalAction;

    if (tcgetattr(STDIN_FILENO, &DbgOriginalTerminalSettings) != 0) {
        return;
    }

    DbgOriginalTerminalForegroundProcessGroupId = tcgetpgrp(STDIN_FILENO);
    tcsetattr(STDIN_FILENO, TCSANOW, &DbgTerminalSettings);

    //
    // Make the debugger's process group the foreground process ground. This
    // was saved when the debugger launched. Ignore SIGTTOU for this operation,
    // otherwise the debugger will be sent a stop signal as its in the
    // background process group.
    //

    memset(&SignalAction, 0, sizeof(struct sigaction));
    SignalAction.sa_handler = SIG_IGN;
    Result = sigaction(SIGTTOU, &SignalAction, &OriginalAction);
    if (Result == 0) {
        tcsetpgrp(STDIN_FILENO, DbgInitialTerminalInputForegroundProcessGroup);
        sigaction(SIGTTOU, &OriginalAction, NULL);
    }

    return;
}

BOOL
DbgrOsGetCharacter (
    PUCHAR Key,
    PUCHAR ControlKey
    )

/*++

Routine Description:

    This routine gets one character from the standard input console.

Arguments:

    Key - Supplies a pointer that receives the printable character. If this
        parameter is NULL, printing characters will be discarded from the input
        buffer.

    ControlKey - Supplies a pointer that receives the non-printable character.
        If this parameter is NULL, non-printing characters will be discarded
        from the input buffer.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ssize_t BytesRead;
    UCHAR Character;
    UCHAR ControlKeyValue;
    struct pollfd Events[2];
    int Result;

    ControlKeyValue = 0;
    while (TRUE) {
        fflush(NULL);

        //
        // Wait for either standard in or a remote command.
        //

        Events[0].fd = STDIN_FILENO;
        Events[0].events = POLLIN;
        Events[0].revents = 0;
        Events[1].fd = DbgRemoteInputPipe[1];
        Events[1].events = POLLIN;
        Events[1].revents = 0;
        Result = poll(Events, 2, -1);
        if (Result == -1) {
            if (errno == EINTR) {
                continue;

            } else {
                DbgOut("Failed to poll: %s\n", strerror(errno));
                return FALSE;
            }
        }

        //
        // Grab a character from standard in.
        //

        if ((Events[0].revents & POLLIN) != 0) {
            do {
                BytesRead = read(STDIN_FILENO, &Character, 1);

            } while ((BytesRead < 0) && (errno == EINTR));

            if (BytesRead <= 0) {
                return FALSE;
            }

            break;

        //
        // Perform the read from the pipe to get the data out. The data
        // itself doesn't matter, the pipe is just a signaling mechanism.
        //

        } else if ((Events[1].revents & POLLIN) != 0) {
            do {
                BytesRead = read(DbgRemoteInputPipe[1], &Character, 1);

            } while ((BytesRead < 0) && (errno == EINTR));

            Character = 0;
            ControlKeyValue = KEY_REMOTE;
            break;

        } else {
            DbgOut("Poll succeeded, but nothing available.\n");
        }
    }

    //
    // Handle non-printing characters.
    //

    if (Character == '\n') {
        Character = 0;
        ControlKeyValue = KEY_RETURN;
    }

    if (Key != NULL) {
        *Key = Character;
    }

    if (ControlKey != NULL) {
        *ControlKey = ControlKeyValue;
    }

    return TRUE;
}

VOID
DbgrOsRemoteInputAdded (
    VOID
    )

/*++

Routine Description:

    This routine is called after a remote command is received and placed on the
    standard input remote command list. It wakes up a thread blocked on local
    user input in an OS-dependent fashion.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ssize_t BytesWritten;
    char Character;

    //
    // It doesn't matter what the character is, just write something into the
    // pipe being used for inter-thread communication.
    //

    Character = 'r';
    do {
        BytesWritten = write(DbgRemoteInputPipe[1], &Character, 1);

    } while ((BytesWritten < 0) && (errno == EINTR));

    return;
}

VOID
DbgrOsPostInputCallback (
    VOID
    )

/*++

Routine Description:

    This routine is called after a line of input is read from the user, giving
    the OS specific code a chance to restore anything it did in the prepare
    to read input function.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // This change of the foreground process group does not need to ignore
    // SIGTTOU because the debugger should be in the current foreground process
    // group.
    //

    tcsetpgrp(STDIN_FILENO, DbgOriginalTerminalForegroundProcessGroupId);
    tcsetattr(STDIN_FILENO, TCSANOW, &DbgOriginalTerminalSettings);
    return;
}

BOOL
UiLoadSourceFile (
    PSTR Path,
    PVOID Contents,
    ULONGLONG Size
    )

/*++

Routine Description:

    This routine loads the contents of a file into the source window.

Arguments:

    Path - Supplies the path of the file being loaded. If this is NULL, then
        the source window should be cleared.

    Contents - Supplies the source file data. This can be NULL.

    Size - Supplies the size of the source file data in bytes.

Return Value:

    Returns TRUE if there was no error, or FALSE if there was an error.

--*/

{

    return TRUE;
}

BOOL
UiHighlightExecutingLine (
    LONG LineNumber,
    BOOL Enable
    )

/*++

Routine Description:

    This routine highlights the currently executing source line and scrolls to
    it.

Arguments:

    LineNumber - Supplies the 1-based line number to highlight (ie the first
        line in the source file is line 1).

    Enable - Supplies a flag indicating whether to highlight this line (TRUE)
        or restore the line to its original color (FALSE).

Return Value:

    Returns TRUE if there was no error, or FALSE if there was an error.

--*/

{

    return TRUE;
}

VOID
UiEnableCommands (
    BOOL Enable
    )

/*++

Routine Description:

    This routine enables or disables the command edit control from being
    enabled. If disabled, the edit control will be made read only.

Arguments:

    Enable - Supplies a flag indicating whether or not to enable (TRUE) or
        disable (FALSE) the command box.

Return Value:

    None.

--*/

{

    return;
}

VOID
UiSetCommandText (
    PSTR Text
    )

/*++

Routine Description:

    This routine sets the text inside the command edit box.

Arguments:

    Text - Supplies a pointer to a null terminated string to set the command
        text to.

Return Value:

    None.

--*/

{

    return;
}

VOID
UiSetPromptText (
    PSTR Text
    )

/*++

Routine Description:

    This routine sets the text inside the prompt edit box.

Arguments:

    Text - Supplies a pointer to a null terminated string to set the prompt
        text to.

Return Value:

    None.

--*/

{

    return;
}

VOID
UiDisplayProfilerData (
    PROFILER_DATA_TYPE DataType,
    PROFILER_DISPLAY_REQUEST DisplayRequest,
    ULONG Threshold
    )

/*++

Routine Description:

    This routine displays the profiler data collected by the core debugging
    infrastructure.

Arguments:

    DataType - Supplies the type of profiler data that is to be displayed.

    DisplayRequest - Supplies a value requesting a display action, which can
        either be to display data once, continually, or to stop continually
        displaying data.

    Threshold - Supplies the minimum percentage a stack entry hit must be in
        order to be displayed.

Return Value:

    None.

--*/

{

    DbgrDisplayCommandLineProfilerData(DataType, DisplayRequest, Threshold);
    return;
}

BOOL
InitializeCommunications (
    PSTR Channel,
    ULONG Baudrate
    )

/*++

Routine Description:

    This routine initializes the communication medium the debugger uses to
    communicate with the target.

Arguments:

    Channel - Supplies a description of the communication medium.

    Baudrate - Supplies the baudrate to use for serial based communications.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    PSTR AfterScan;
    PSTR Colon;
    PSTR HostCopy;
    unsigned long Port;
    BOOL Result;
    struct termios Termios;
    PTTY_BAUD_RATE Rate;

    HostCopy = NULL;
    Result = FALSE;
    if (strncasecmp(Channel, "tcp:", 4) == 0) {
        if (DbgrSocketInitializeLibrary() != 0) {
            DbgOut("Failed to initialize socket library.\n");
            return FALSE;
        }

        HostCopy = strdup(Channel + 4);
        if (HostCopy == NULL) {
            return FALSE;
        }

        Colon = strrchr(HostCopy, ':');
        if (Colon == NULL) {
            DbgOut("Error: Port number expected in the form host:port.\n");
            goto InitializeCommunicationsEnd;
        }

        *Colon = '\0';
        Port = strtoul(Colon + 1, &AfterScan, 10);
        if ((*AfterScan != '\0') || (AfterScan == Colon + 1)) {
            DbgOut("Error: Invalid port '%s'.\n", Colon + 1);
        }

        DbgKdDescriptor = DbgrSocketCreateStreamSocket();
        if (DbgKdDescriptor < 0) {
            DbgOut("Failed to create socket.\n");
            goto InitializeCommunicationsEnd;
        }

        DbgOut("Connecting via TCP to %s on port %u...", HostCopy, Port);
        if (DbgrSocketConnect(DbgKdDescriptor, HostCopy, Port) != 0) {
            DbgOut("Failed to connect: %s", strerror(errno));
            goto InitializeCommunicationsEnd;
        }

    } else {
        DbgKdDescriptor = open(Channel, O_RDWR | O_BINARY);
        if (DbgKdDescriptor < 0) {
            DbgOut("Cannot open %s: %s\n", Channel, strerror(errno));
            return FALSE;
        }

        if (tcgetattr(DbgKdDescriptor, &Termios) == 0) {
            memcpy(&DbgOriginalKdSettings, &Termios, sizeof(Termios));
            Termios.c_cflag = CS8 | CREAD | HUPCL;
            Termios.c_lflag = 0;
            Termios.c_iflag = 0;
            Termios.c_oflag = 0;

            //
            // Convert the baud rate into a speed_t value.
            //

            for (Rate = TtyBaudRates; Rate->Name != NULL; Rate += 1) {
                if (Rate->Rate == Baudrate) {
                    break;
                }
            }

            if (Rate->Name == NULL) {
                DbgOut("Invalid baud rate: %lu\n", Baudrate);
                return FALSE;
            }

            cfsetispeed(&Termios, Rate->Value);
            cfsetospeed(&Termios, Rate->Value);
            if (tcsetattr(DbgKdDescriptor, TCSANOW, &Termios) != 0) {
                DbgOut("Warning: Failed to set serial settings on %s: %s\n",
                       Channel,
                       strerror(errno));
            }
        }
    }

    Result = TRUE;

InitializeCommunicationsEnd:
    if (HostCopy != NULL) {
        free(HostCopy);
    }

    if (Result == FALSE) {
        if (DbgKdDescriptor >= 0) {
            DbgrSocketClose(DbgKdDescriptor);
            DbgKdDescriptor = -1;
        }
    }

    return Result;
}

VOID
DestroyCommunications (
    VOID
    )

/*++

Routine Description:

    This routine tears down the debug communication channel.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (DbgKdDescriptor >= 0) {
        tcsetattr(DbgKdDescriptor, TCSANOW, &DbgOriginalKdSettings);
        close(DbgKdDescriptor);
        DbgKdDescriptor = -1;
    }

    return;
}

BOOL
CommReceive (
    PVOID Buffer,
    ULONG BytesToRead
    )

/*++

Routine Description:

    This routine receives a number of bytes from the debugger/debuggee
    connection.

Arguments:

    Buffer - Supplies a pointer to the buffer where the data should be returned.

    BytesToRead - Supplies the number of bytes that should be received into the
        buffer.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ssize_t BytesRead;

    do {
        do {
            BytesRead = read(DbgKdDescriptor, Buffer, BytesToRead);

        } while ((BytesRead < 0) && (errno == EINTR));

        if (BytesRead == 0) {
            return FALSE;
        }

        BytesToRead -= BytesRead;
        Buffer += BytesRead;

    } while (BytesToRead != 0);

    return TRUE;
}

BOOL
CommSend (
    PVOID Buffer,
    ULONG BytesToSend
    )

/*++

Routine Description:

    This routine sends a number of bytes through the debugger/debuggee
    connection.

Arguments:

    Buffer - Supplies a pointer to the buffer where the data to be sent resides.

    BytesToSend - Supplies the number of bytes that should be sent.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    ssize_t BytesWritten;

    do {
        do {
            BytesWritten = write(DbgKdDescriptor, Buffer, BytesToSend);

        } while ((BytesWritten < 0) && (errno == EINTR));

        if (BytesWritten <= 0) {
            return FALSE;
        }

        Buffer += BytesWritten;
        BytesToSend -= BytesWritten;

    } while (BytesToSend != 0);

    return TRUE;
}

BOOL
CommReceiveBytesReady (
    VOID
    )

/*++

Routine Description:

    This routine determines whether or not bytes can be read from the
    debugger connection.

Arguments:

    None.

Return Value:

    TRUE if there are bytes ready to be read.

    FALSE if no bytes are ready at this time.

--*/

{

    struct pollfd Poll;

    Poll.fd = DbgKdDescriptor;
    Poll.events = POLLIN;
    if (poll(&Poll, 1, 0) <= 0) {
        return FALSE;
    }

    return TRUE;
}

VOID
CommStall (
    ULONG Milliseconds
    )

/*++

Routine Description:

    This routine pauses for the given amount of time.

Arguments:

    Milliseconds - Supplies the amount of time, in milliseconds, to stall the
        current thread for.

Return Value:

    None.

--*/

{

    poll(NULL, 0, Milliseconds);
    return;
}

HANDLE
CreateDebuggerLock (
    VOID
    )

/*++

Routine Description:

    This routine creates a debugger lock.

Arguments:

    None.

Return Value:

    Returns a handle to a debugger lock on success, or NULL on failure.

--*/

{

    pthread_mutex_t *Lock;

    Lock = malloc(sizeof(pthread_mutex_t));
    if (Lock == NULL) {
        return NULL;
    }

    pthread_mutex_init(Lock, NULL);
    return (HANDLE)Lock;
}

VOID
AcquireDebuggerLock (
    HANDLE Lock
    )

/*++

Routine Description:

    This routine acquires a debugger lock. This routine does not return until
    the lock is required.

Arguments:

    Lock - Supplies a handle to the lock that is to be acquired.

Return Value:

    None.

--*/

{

    pthread_mutex_t *Mutex;

    Mutex = (pthread_mutex_t *)Lock;
    pthread_mutex_lock(Mutex);
    return;
}

VOID
ReleaseDebuggerLock (
    HANDLE Lock
    )

/*++

Routine Description:

    This routine releases a debugger lock.

Arguments:

    Lock - Supplies a handle to the lock that is to be released.

Return Value:

    None.

--*/

{

    pthread_mutex_t *Mutex;

    Mutex = (pthread_mutex_t *)Lock;
    pthread_mutex_unlock(Mutex);
    return;
}

VOID
DestroyDebuggerLock (
    HANDLE Lock
    )

/*++

Routine Description:

    This routine destroys a debugger lock.

Arguments:

    Lock - Supplies a handle to the lock that is to be destroyed.

Return Value:

    None.

--*/

{

    pthread_mutex_t *Mutex;

    Mutex = (pthread_mutex_t *)Lock;
    pthread_mutex_destroy(Mutex);
    free(Mutex);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

void
DbgrConsoleInterruptHandler (
    int Signal
    )

/*++

Routine Description:

    This routine is called when a debug process receives SIGINT, for example
    using Control+C. It requests a break in.

Arguments:

    Signal - Supplies the number of triggered signal. Should be SIGINT.

Return Value:

    None.

--*/

{

    DbgrRequestBreakIn();
    return;
}

