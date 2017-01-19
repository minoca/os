/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    syslog.c

Abstract:

    This module implements system logging support for the C library.

Author:

    Evan Green 22-Jan-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the path of the Unix socket the C library sends log messages to.
// A logging daemon is expected to be a socket server on the other end there.
//

#define SYSLOG_PATH "/dev/log"

//
// Define the console path syslog writes to if normal writes failed.
//

#define SYSLOG_CONSOLE_PATH "/dev/console"

//
// Define the maximum size of a syslog message.
//

#define SYSLOG_MESSAGE_MAX 2048
#define SYSLOG_MESSAGE_HEADER_MAX 130

#define SYSLOG_TIME_FORMAT "%h %e %T"
#define SYSLOG_TIME_BUFFER_SIZE 20

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ClpOpenLog (
    int Options,
    int Facility,
    BOOL MustConnect
    );

VOID
ClpCloseLog (
    VOID
    );

VOID
ClpSyslogWrite (
    int FileDescriptor,
    const void *Buffer,
    size_t ByteCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the identifier string sent with every message.
//

char *ClLogIdentifier;

//
// Store the current logging options.
//

int ClLogOptions;

//
// Store the default facility code.
//

int ClLogFacility = LOG_USER;

//
// Store the mask of priorities to log. The default is to log everything.
//

ULONG ClLogMask = LOG_UPTO(LOG_DEBUG);

//
// Store the file descriptor for the logging socket.
//

int ClLogSocket = -1;

//
// Store the socket type.
//

int ClLogSocketType;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void
openlog (
    const char *Identifier,
    int Options,
    int Facility
    )

/*++

Routine Description:

    This routine sets process attributes that affect subsequent calls to the
    syslog function.

Arguments:

    Identifier - Supplies an identifier that is prepended to every message.

    Options - Supplies a mask of logging options. See LOG_* definitions.

    Facility - Supplies the default facility to be assigned to all messages
        that don't already have a facility. The initial default facility is
        LOG_USER.

Return Value:

    None.

--*/

{

    char *NewIdentifier;

    //
    // TODO: Lock openlog when threading is supported.
    //

    if (Identifier != NULL) {
        NewIdentifier = strdup(Identifier);
        if (NewIdentifier != NULL) {
            if (ClLogIdentifier != NULL) {
                free(ClLogIdentifier);
            }

            ClLogIdentifier = NewIdentifier;
        }
    }

    ClpOpenLog(Options, Facility, FALSE);
    return;
}

LIBC_API
int
setlogmask (
    int PriorityMask
    )

/*++

Routine Description:

    This routine sets the log priority mask for the current process, and
    returns the previous mask. Calls to syslog with a priority not set in the
    given mask will be silently rejected. The default mask allows all
    priorities to be logged. A call to openlog is not requred prior to calling
    this function.

Arguments:

    PriorityMask - Supplies the mask of priority bits to log. Use LOG_MASK and
        LOG_UPTO macros to create this value. If this value is zero, the
        current mask is returned but is not changed.

Return Value:

    Returns the original mask before the potential change.

--*/

{

    int OriginalMask;

    if (PriorityMask != 0) {
        OriginalMask = RtlAtomicExchange32(&ClLogMask, PriorityMask);

    } else {
        OriginalMask = RtlAtomicOr32(&ClLogMask, 0);
    }

    return OriginalMask;
}

LIBC_API
void
vsyslog (
    int Priority,
    const char *Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine sends a message to an implementation-defined logging facility,
    which may log it to an implementation-defined system log, write it to the
    console, forward it over the network, or simply ignore it. The message
    header contains at least a timestamp and tag string.

Arguments:

    Priority - Supplies the priority and facility of the message.

    Format - Supplies the printf-style format string to print.

    ArgumentList - Supplies the remaining arguments, dictated by the format
        string.

Return Value:

    None.

--*/

{

    size_t BodyLength;
    int Console;
    time_t CurrentTime;
    char CurrentTimeBuffer[SYSLOG_TIME_BUFFER_SIZE];
    struct tm CurrentTimeFields;
    size_t HeaderLength;
    char Message[SYSLOG_MESSAGE_MAX];
    struct sigaction OldPipeAction;
    struct sigaction *OldPipeActionPointer;
    struct sigaction PipeAction;
    pid_t ProcessId;
    int SavedError;
    ssize_t SizeSent;

    //
    // Return quickly if the mask denies the log.
    //

    if (((LOG_MASK(LOG_PRI(Priority)) != 0) & ClLogMask) == 0) {
        return;
    }

    //
    // Handle invalid priority or facility bits.
    //

    if ((Priority & (~(LOG_PRIMASK | LOG_FACMASK))) != 0) {
        syslog(LOG_ERR | LOG_CONS | LOG_PERROR | LOG_PID,
               "syslog: Unknown facility/priority %x",
               Priority);

        Priority &= LOG_PRIMASK | LOG_FACMASK;
    }

    //
    // TODO: Lock vsyslog when threading is supported.
    //

    //
    // Set the facility if none was provided.
    //

    if ((Priority & LOG_FACMASK) == 0) {
        Priority |= ClLogFacility;
    }

    SavedError = errno;
    time(&CurrentTime);
    CurrentTimeBuffer[0] = '\0';
    strftime(CurrentTimeBuffer,
             sizeof(CurrentTimeBuffer),
             SYSLOG_TIME_FORMAT,
             localtime_r(&CurrentTime, &CurrentTimeFields));

    if (((ClLogOptions & LOG_PID) != 0) ||
        (ClLogIdentifier == NULL) ||
        (ClLogIdentifier[0] == '\0')) {

        ProcessId = getpid();
        HeaderLength = snprintf(Message,
                                SYSLOG_MESSAGE_HEADER_MAX,
                                "<%d>%s %s[%ld]: ",
                                Priority,
                                CurrentTimeBuffer,
                                ClLogIdentifier,
                                (long)ProcessId);

    } else {
        HeaderLength = snprintf(Message,
                                SYSLOG_MESSAGE_HEADER_MAX,
                                "<%d>%s %s: ",
                                Priority,
                                CurrentTimeBuffer,
                                ClLogIdentifier);
    }

    errno = SavedError;
    BodyLength = vsnprintf(Message + HeaderLength,
                           SYSLOG_MESSAGE_MAX - HeaderLength,
                           Format,
                           ArgumentList);

    //
    // Log to standard error if requested.
    //

    if ((ClLogOptions & LOG_PERROR) != 0) {
        ClpSyslogWrite(STDERR_FILENO, Message + HeaderLength, BodyLength);
        if (Message[HeaderLength + BodyLength] != '\n') {
            ClpSyslogWrite(STDERR_FILENO, "\n", 1);
        }
    }

    //
    // Prepare for a broken connection by ignoring SIGPIPE.
    //

    memset(&PipeAction, 0, sizeof(PipeAction));
    PipeAction.sa_handler = SIG_IGN;
    sigemptyset(&(PipeAction.sa_mask));
    OldPipeActionPointer = NULL;
    if (sigaction(SIGPIPE, &PipeAction, &OldPipeAction) == 0) {
        OldPipeActionPointer = &OldPipeAction;
    }

    if (ClLogSocket < 0) {
        ClpOpenLog(ClLogOptions, 0, TRUE);
    }

    //
    // For stream sockets, also send a null terminator to mark the end of the
    // record.
    //

    assert(HeaderLength + BodyLength < SYSLOG_MESSAGE_MAX);

    if (ClLogSocketType == SOCK_STREAM) {
        if ((HeaderLength + BodyLength + 1) < SYSLOG_MESSAGE_MAX) {
            BodyLength += 1;
        }

        Message[HeaderLength + BodyLength] = '\0';
    }

    SizeSent = 0;
    if (ClLogSocket >= 0) {
        SizeSent = send(ClLogSocket, Message, HeaderLength + BodyLength, 0);
    }

    if (SizeSent != HeaderLength + BodyLength) {
        if ((ClLogSocketType == SOCK_STREAM) && (BodyLength != 0)) {
            BodyLength -= 1;
        }

        ClpCloseLog();

        //
        // Log to the console if the send failed.
        //

        if ((ClLogOptions & LOG_CONS) != 0) {
            Console = open(SYSLOG_CONSOLE_PATH, O_WRONLY | O_NOCTTY);
            if (Console >= 0) {
                ClpSyslogWrite(Console, Message, HeaderLength + BodyLength);
                ClpSyslogWrite(Console, "\r\n", 2);
                close(Console);
            }
        }
    }

    //
    // Restore the pipe handler.
    //

    if (OldPipeActionPointer != NULL) {
        sigaction(SIGPIPE, &OldPipeAction, NULL);
    }

    return;
}

LIBC_API
void
syslog (
    int Priority,
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine sends a message to an implementation-defined logging facility,
    which may log it to an implementation-defined system log, write it to the
    console, forward it over the network, or simply ignore it. The message
    header contains at least a timestamp and tag string.

Arguments:

    Priority - Supplies the priority and facility of the message.

    Format - Supplies the printf-style format string to print.

    ... - Supplies the remaining arguments, dictated by the format string.

Return Value:

    None.

--*/

{

    va_list ArgumentList;

    va_start(ArgumentList, Format);
    vsyslog(Priority, Format, ArgumentList);
    va_end(ArgumentList);
    return;
}

LIBC_API
void
closelog (
    void
    )

/*++

Routine Description:

    This routine shuts down system logging facilities. They may be reopened by
    a subsequent call to openlog or syslog.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClLogIdentifier != NULL) {
        free(ClLogIdentifier);
        ClLogIdentifier = NULL;
    }

    //
    // TODO: Lock closelog once threading is implemented.
    //

    ClpCloseLog();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ClpOpenLog (
    int Options,
    int Facility,
    BOOL MustConnect
    )

/*++

Routine Description:

    This routine opens the log file, potentially.

Arguments:

    Options - Supplies a mask of logging options. See LOG_* definitions.

    Facility - Supplies the default facility to be assigned to all messages
        that don't already have a facility. The initial default facility is
        LOG_USER.

    MustConnect - Supplies a boolean indicating if this routine must attempt
        to connect (TRUE) or can delay according to the flags (FALSE).

Return Value:

    None.

--*/

{

    struct sockaddr_un Address;
    int OriginalError;
    int Result;
    int Socket;
    UINTN Try;
    int Type;

    ClLogOptions = Options;
    if ((Facility != 0) && ((Facility & ~LOG_FACMASK) == 0)) {
        ClLogFacility = Facility;
    }

    //
    // Don't bother connecting right now if not necessary.
    //

    if ((MustConnect == FALSE) && ((ClLogOptions & LOG_NDELAY) == 0)) {
        return;
    }

    if (ClLogSocket >= 0) {
        return;
    }

    //
    // Loop trying different socket types.
    //

    for (Try = 0; Try < 2; Try += 1) {
        if (Try == 0) {
            Type = SOCK_DGRAM;

        } else {
            Type = SOCK_STREAM;
        }

        Socket = socket(AF_UNIX, Type, 0);
        if (Socket <= 0) {
            return;
        }

        fcntl(Socket, F_SETFD, FD_CLOEXEC);
        OriginalError = errno;
        memset(&Address, 0, sizeof(Address));
        Address.sun_family = AF_UNIX;
        strncpy(Address.sun_path, SYSLOG_PATH, UNIX_PATH_MAX);
        Result = connect(Socket, (struct sockaddr *)&Address, sizeof(Address));

        //
        // On failure, restore the original error and either try another type
        // or fail.
        //

        if (Result != 0) {
            errno = OriginalError;
            close(Socket);

        //
        // Set the new connected descriptor.
        //

        } else {

            assert(ClLogSocket == -1);

            ClLogSocket = Socket;
            ClLogSocketType = Type;
        }
    }

    return;
}

VOID
ClpCloseLog (
    VOID
    )

/*++

Routine Description:

    This routine closes the syslog socket.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClLogSocket >= 0) {
        close(ClLogSocket);
        ClLogSocket = -1;
    }

    return;
}

VOID
ClpSyslogWrite (
    int FileDescriptor,
    const void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine attempts to write the specifed number of bytes to the given
    open file descriptor.

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

    ssize_t BytesComplete;
    size_t TotalComplete;

    TotalComplete = 0;
    while (TotalComplete != ByteCount) {
        BytesComplete = write(FileDescriptor,
                              Buffer + TotalComplete,
                              ByteCount - TotalComplete);

        if (BytesComplete < 0) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        TotalComplete += BytesComplete;
    }

    return;
}

