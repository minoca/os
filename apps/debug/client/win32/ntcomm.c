/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntcomm.c

Abstract:

    This module implements the common win32 functionality for the debugger
    client between the GUI version and the command line version.

Author:

    Evan Green 2-Jul-2012

Environment:

    Debug Client (Win32)

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _WIN32_WINNT 0x0501

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <windows.h>
#include <winsock.h>

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif

#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "dbgrprof.h"
#include "console.h"
#include "sock.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time in milliseconds to wait before declaring failure
// when opening a communications device. Qemu for instance needs a couple
// seconds to open up its pipe servers, etc.
//

#define DEBUGGER_OPEN_TIMEOUT 10000

//
// Define the amount of time to wait in milliseconds between open attempts.
//

#define DEBUGGER_OPEN_RETRY_RATE 100

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CHANNEL_TYPE {
    CommChannelInvalid,
    CommChannelPipe,
    CommChannelSerial,
    CommChannelTcp,
    CommChannelExec
} CHANNEL_TYPE, *PCHANNEL_TYPE;

typedef struct _NT_THREAD_CREATION_CONTEXT {
    PDBGR_THREAD_ROUTINE ThreadRoutine;
    PVOID Parameter;
} NT_THREAD_CREATION_CONTEXT, *PNT_THREAD_CREATION_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

HANDLE CommChannel = INVALID_HANDLE_VALUE;
HANDLE CommChannelOut = INVALID_HANDLE_VALUE;
CHANNEL_TYPE CommChannelType = CommChannelInvalid;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
DbgrpCreateExecPipe (
    PSTR Command
    );

DWORD
WINAPI
DbgrpOsThreadStart (
    LPVOID Parameter
    );

VOID
DbgrpPrintLastError (
    VOID
    );

//
// ------------------------------------------------------------------ Functions
//

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

    PNT_THREAD_CREATION_CONTEXT Context;
    INT Result;
    HANDLE Thread;

    Context = malloc(sizeof(NT_THREAD_CREATION_CONTEXT));
    if (Context == NULL) {
        Result = ENOMEM;
        goto OsCreateThreadEnd;
    }

    Context->ThreadRoutine = ThreadRoutine;
    Context->Parameter = Parameter;
    Thread = CreateThread(NULL, 0, DbgrpOsThreadStart, Context, 0, NULL);
    if (Thread == NULL) {
        Result = EINVAL;
        goto OsCreateThreadEnd;
    }

    Context = NULL;
    Result = 0;

OsCreateThreadEnd:
    if (Context != NULL) {
        free(Context);
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

    return _pipe(FileDescriptors, 0, O_BINARY);
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

    return getenv("USERNAME");
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
    UCHAR EmptyBuffer[8];
    PSTR HostCopy;
    unsigned long Port;
    BOOL Result;
    DCB SerialParameters;
    int Socket;
    ULONG Timeout;
    COMMTIMEOUTS Timeouts;

    HostCopy = NULL;
    Result = FALSE;
    Socket = -1;

    //
    // Connect via TCP.
    //

    if (strncasecmp(Channel, "tcp:", 4) == 0) {
        if (DbgrSocketInitializeLibrary() != 0) {
            DbgOut("Failed to initialize socket library.\n");
            return FALSE;
        }

        HostCopy = strdup(Channel + 4);
        if (HostCopy == NULL) {
            goto InitializeCommunicationsEnd;
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

        Socket = DbgrSocketCreateStreamSocket();
        if (Socket < 0) {
            DbgOut("Failed to create socket.\n");
            goto InitializeCommunicationsEnd;
        }

        DbgOut("Connecting via TCP to %s on port %u...", HostCopy, Port);
        if (DbgrSocketConnect(Socket, HostCopy, Port) != 0) {
            DbgOut("Failed to connect: ");
            DbgrpPrintLastError();
            goto InitializeCommunicationsEnd;
        }

        DbgOut("Connected.\n");
        CommChannel = (HANDLE)Socket;
        CommChannelType = CommChannelTcp;
        Socket = -1;
        Result = TRUE;
        goto InitializeCommunicationsEnd;

    //
    // Execute another process and use its stdin/stdout as the kernel debug
    // channel.
    //

    } else if (strncasecmp(Channel, "exec:", 5) == 0) {
        Result = DbgrpCreateExecPipe(Channel + 5);
        goto InitializeCommunicationsEnd;
    }

    //
    // CreateFile can open both named pipes and COM ports. Named pipes usually
    // take the form "\\.\pipe\mypipe", and COM ports take the form "\\.\com1".
    // Open the resource for read/write access, no sharing, and with no other
    // remarkable properties.
    //

    Timeout = 0;
    while (Timeout <= DEBUGGER_OPEN_TIMEOUT) {
        CommChannel = CreateFile(Channel,
                                 GENERIC_READ | GENERIC_WRITE,
                                 0,
                                 NULL,
                                 OPEN_EXISTING,
                                 0,
                                 NULL);

        if (CommChannel != INVALID_HANDLE_VALUE) {
            break;
        }

        CommStall(DEBUGGER_OPEN_RETRY_RATE);
        Timeout += DEBUGGER_OPEN_RETRY_RATE;
    }

    if (CommChannel == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    //
    // If the channel was a serial port, set up the serial parameters.
    //

    if ((strncmp("COM", Channel, 3) == 0) ||
        (strncmp("com", Channel, 3) == 0) ||
        (strncmp("\\\\.\\COM", Channel, 7) == 0) ||
        (strncmp("\\\\.\\com", Channel, 7) == 0)) {

        memset(&SerialParameters, 0, sizeof(DCB));
        SerialParameters.DCBlength = sizeof(DCB);
        if (GetCommState(CommChannel, &SerialParameters) == FALSE) {
            goto InitializeCommunicationsEnd;
        }

        SerialParameters.BaudRate = Baudrate;
        SerialParameters.ByteSize = 8;
        SerialParameters.StopBits = ONESTOPBIT;
        SerialParameters.Parity = NOPARITY;
        SerialParameters.fOutX = FALSE;
        SerialParameters.fInX = FALSE;
        if (SetCommState(CommChannel, &SerialParameters) == FALSE) {
            goto InitializeCommunicationsEnd;
        }

        //
        // Set up a timeout to prevent blocking if there's no data available.
        //

        memset(&Timeouts, 0, sizeof(COMMTIMEOUTS));
        Timeouts.ReadIntervalTimeout = 50;
        Timeouts.ReadTotalTimeoutConstant = 1000;
        Timeouts.ReadTotalTimeoutMultiplier = 2;
        Timeouts.WriteTotalTimeoutConstant = 1000;
        Timeouts.WriteTotalTimeoutMultiplier = 10;
        if (SetCommTimeouts(CommChannel, &Timeouts) == FALSE) {
            DbgOut("Unable to set timeouts.\n");
            goto InitializeCommunicationsEnd;
        }

        CommChannelType = CommChannelSerial;

    } else {
        CommChannelType = CommChannelPipe;

        //
        // Send some data down the wire to "clear the pipes". Qemu on x86 is
        // the only known platform that needs this.
        //

        memset(EmptyBuffer, 0, sizeof(EmptyBuffer));
        CommSend(EmptyBuffer, sizeof(EmptyBuffer));
    }

    Result = TRUE;

InitializeCommunicationsEnd:
    if (HostCopy != NULL) {
        free(HostCopy);
    }

    if (Socket >= 0) {
        DbgrSocketClose(Socket);
        Socket = -1;
    }

    if (Result == FALSE) {
        if (CommChannel != INVALID_HANDLE_VALUE) {
            CloseHandle(CommChannel);
            CommChannel = INVALID_HANDLE_VALUE;
            CommChannelType = CommChannelInvalid;
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

    if (CommChannelType == CommChannelTcp) {
        DbgrSocketClose((int)CommChannel);

    } else if (CommChannel != INVALID_HANDLE_VALUE) {
        CloseHandle(CommChannel);
        CommChannel = INVALID_HANDLE_VALUE;
        if (CommChannelOut != INVALID_HANDLE_VALUE) {
            CloseHandle(CommChannelOut);
            CommChannelOut = INVALID_HANDLE_VALUE;
        }

        CommChannelType = CommChannelInvalid;
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

    ULONG BytesRead;
    int BytesReceived;
    PVOID CurrentPosition;
    ULONG Result;
    ULONG TotalBytesReceived;

    CurrentPosition = Buffer;
    TotalBytesReceived = 0;
    while (TotalBytesReceived < BytesToRead) {
        if (CommChannelType == CommChannelTcp) {
            BytesReceived = DbgrSocketReceive((int)CommChannel,
                                              CurrentPosition,
                                              BytesToRead - TotalBytesReceived);

            if (BytesReceived == 0) {
                DbgOut("Socket closed\n");
                return FALSE;

            } else if (BytesReceived < 0) {
                DbgOut("Receive failure.\n");
                return FALSE;
            }

            BytesRead = BytesReceived;

        } else {
            Result = ReadFile(CommChannel,
                              CurrentPosition,
                              BytesToRead - TotalBytesReceived,
                              &BytesRead,
                              NULL);

            if (Result == FALSE) {
                return FALSE;
            }
        }

        TotalBytesReceived += BytesRead;
        CurrentPosition += BytesRead;
    }

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

    int BytesSent;
    ULONG BytesWritten;
    PVOID CurrentPosition;
    HANDLE Handle;
    ULONG Result;
    ULONG TotalBytesWritten;

    CurrentPosition = Buffer;
    TotalBytesWritten = 0;
    while (TotalBytesWritten < BytesToSend) {
        if (CommChannelType == CommChannelTcp) {
            BytesSent = DbgrSocketSend((int)CommChannel,
                                       CurrentPosition,
                                       BytesToSend - TotalBytesWritten);

            if (BytesSent <= 0) {
                DbgOut("Send failure.\n");
                return FALSE;
            }

        } else {
            Handle = CommChannel;
            if (CommChannelType == CommChannelExec) {
                Handle = CommChannelOut;
            }

            Result = WriteFile(Handle,
                               CurrentPosition,
                               BytesToSend - TotalBytesWritten,
                               &BytesWritten,
                               NULL);

            if (Result == FALSE) {
                return FALSE;
            }
        }

        TotalBytesWritten += BytesWritten;
        CurrentPosition += BytesWritten;
    }

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

    DWORD BytesAvailable;
    char Peek[1024];
    int PeekSize;
    BOOL Result;
    COMSTAT SerialStatus;

    //
    // For both a named pipe and a serial port, determine how many bytes are
    // available to be received.
    //

    BytesAvailable = 0;
    switch (CommChannelType) {
    case CommChannelPipe:
    case CommChannelExec:
        Result = PeekNamedPipe(CommChannel,
                               NULL,
                               0,
                               NULL,
                               &BytesAvailable,
                               NULL);

        if (Result == FALSE) {
            BytesAvailable = 0;
        }

        break;

    case CommChannelSerial:
        Result = ClearCommError(CommChannel, NULL, &SerialStatus);
        if (Result != FALSE) {
            BytesAvailable = SerialStatus.cbInQue;
        }

        break;

    case CommChannelTcp:
        PeekSize = DbgrSocketPeek((int)CommChannel, Peek, sizeof(Peek));
        if (PeekSize > 0) {
            BytesAvailable = PeekSize;
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    if (BytesAvailable != 0) {
        return TRUE;
    }

    return FALSE;
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

    Sleep(Milliseconds);
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

    HANDLE Mutex;

    Mutex = CreateMutex(NULL, FALSE, NULL);
    return (HANDLE)Mutex;
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

    HANDLE Mutex;

    Mutex = (HANDLE)Lock;
    WaitForSingleObject(Mutex, INFINITE);
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

    HANDLE Mutex;

    Mutex = (HANDLE)Lock;
    ReleaseMutex(Mutex);
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

    HANDLE Mutex;

    Mutex = (HANDLE)Lock;
    CloseHandle(Mutex);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
DbgrpCreateExecPipe (
    PSTR Command
    )

/*++

Routine Description:

    This routine execs the given command and uses its stdin and stdout as the
    kernel debug channel.

Arguments:

    Command - Supplies a pointer to a string of the command line.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    DWORD Inherit;
    PROCESS_INFORMATION ProcessInformation;
    BOOL Result;
    HANDLE StandardIn[2];
    HANDLE StandardOut[2];
    STARTUPINFO StartupInfo;

    memset(&StartupInfo, 0, sizeof(STARTUPINFO));
    StartupInfo.cb = sizeof(STARTUPINFO);
    StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    StandardIn[0] = INVALID_HANDLE_VALUE;
    StandardIn[1] = INVALID_HANDLE_VALUE;
    StandardOut[0] = INVALID_HANDLE_VALUE;
    StandardOut[1] = INVALID_HANDLE_VALUE;
    Result = FALSE;
    if ((!CreatePipe(&(StandardIn[0]), &(StandardIn[1]), NULL, 0)) ||
        (!CreatePipe(&(StandardOut[0]), &(StandardOut[1]), NULL, 0))) {

        goto CreateExecPipeEnd;
    }

    Inherit = HANDLE_FLAG_INHERIT;
    if ((!SetHandleInformation(StandardIn[0], Inherit, Inherit)) ||
        (!SetHandleInformation(StandardOut[1], Inherit, Inherit))) {

        goto CreateExecPipeEnd;
    }

    StartupInfo.hStdInput = StandardIn[0];
    StartupInfo.hStdOutput = StandardOut[1];
    StartupInfo.hStdError = StandardOut[1]; //GetStdHandle(STD_ERROR_HANDLE);
    DbgOut("Spawning '%s'\n", Command);
    Result = CreateProcess(NULL,
                           Command,
                           NULL,
                           NULL,
                           TRUE,
                           CREATE_NEW_CONSOLE,
                           NULL,
                           NULL,
                           &StartupInfo,
                           &ProcessInformation);

    if (Result == FALSE) {
        DbgOut("Failed to exec process: %s: ", Command);
        DbgrpPrintLastError();
        goto CreateExecPipeEnd;
    }

    DbgOut("Created process %x.\n", ProcessInformation.dwProcessId);
    CloseHandle(ProcessInformation.hProcess);
    CloseHandle(ProcessInformation.hThread);
    CommChannel = StandardOut[0];
    StandardOut[0] = INVALID_HANDLE_VALUE;
    CommChannelOut = StandardIn[1];
    StandardIn[1] = INVALID_HANDLE_VALUE;
    CommChannelType = CommChannelExec;

CreateExecPipeEnd:
    if (StandardIn[0] != INVALID_HANDLE_VALUE) {
        CloseHandle(StandardIn[0]);
    }

    if (StandardIn[1] != INVALID_HANDLE_VALUE) {
        CloseHandle(StandardIn[1]);
    }

    if (StandardOut[0] != INVALID_HANDLE_VALUE) {
        CloseHandle(StandardOut[0]);
    }

    if (StandardOut[1] != INVALID_HANDLE_VALUE) {
        CloseHandle(StandardOut[1]);
    }

    return Result;
}

DWORD
WINAPI
DbgrpOsThreadStart (
    LPVOID Parameter
    )

/*++

Routine Description:

    This routine implements a short wrapper for threads in Windows.

Arguments:

    Parameter - Supplies a pointer to the thread creation context. This routine
        will free this parameter.

Return Value:

    0 always.

--*/

{

    PNT_THREAD_CREATION_CONTEXT Context;
    PDBGR_THREAD_ROUTINE Routine;
    PVOID RoutineParameter;

    Context = (PNT_THREAD_CREATION_CONTEXT)Parameter;
    Routine = Context->ThreadRoutine;
    RoutineParameter = Context->Parameter;
    free(Context);
    Routine(RoutineParameter);
    return 0;
}

VOID
DbgrpPrintLastError (
    VOID
    )

/*++

Routine Description:

    This routine prints a description of GetLastError to standard error and
    also prints a newline.

Arguments:

    None.

Return Value:

    None.

--*/

{

    char *Message;

    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                   NULL,
                   GetLastError(),
                   0,
                   (LPSTR)&Message,
                   0,
                   NULL);

    DbgOut("%s", Message);
    LocalFree(Message);
    return;
}

