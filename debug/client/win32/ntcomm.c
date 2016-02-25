/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "dbgrprof.h"
#include "console.h"

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
    CommChannelSerial
} CHANNEL_TYPE, *PCHANNEL_TYPE;

typedef struct _NT_THREAD_CREATION_CONTEXT {
    PDBGR_THREAD_ROUTINE ThreadRoutine;
    PVOID Parameter;
} NT_THREAD_CREATION_CONTEXT, *PNT_THREAD_CREATION_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

HANDLE CommChannel = INVALID_HANDLE_VALUE;
CHANNEL_TYPE CommChannelType = CommChannelInvalid;

//
// ----------------------------------------------- Internal Function Prototypes
//

DWORD
WINAPI
DbgrpOsThreadStart (
    LPVOID Parameter
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

    return _pipe(FileDescriptors, 0, _O_BINARY);
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

    UCHAR EmptyBuffer[8];
    BOOL Result;
    DCB SerialParameters;
    ULONG Timeout;
    COMMTIMEOUTS Timeouts;

    Result = FALSE;

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

    if (CommChannel != INVALID_HANDLE_VALUE) {
        CloseHandle(CommChannel);
        CommChannel = INVALID_HANDLE_VALUE;
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
    PVOID CurrentPosition;
    ULONG Result;
    ULONG TotalBytesReceived;

    CurrentPosition = Buffer;
    TotalBytesReceived = 0;
    while (TotalBytesReceived < BytesToRead) {
        Result = ReadFile(CommChannel,
                          CurrentPosition,
                          BytesToRead - TotalBytesReceived,
                          &BytesRead,
                          NULL);

        if (Result == FALSE) {
            return FALSE;
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

    ULONG BytesSent;
    PVOID CurrentPosition;
    ULONG Result;
    ULONG TotalBytesSent;

    CurrentPosition = Buffer;
    TotalBytesSent = 0;
    while (TotalBytesSent < BytesToSend) {
        Result = WriteFile(CommChannel,
                           CurrentPosition,
                           BytesToSend - TotalBytesSent,
                           &BytesSent,
                           NULL);

        if (Result == FALSE) {
            return FALSE;
        }

        TotalBytesSent += BytesSent;
        CurrentPosition += BytesSent;
    }

    return TRUE;
}

ULONG
CommReceiveBytesReady (
    VOID
    )

/*++

Routine Description:

    This routine determines how many bytes of data are ready to be read from
    the communication channel.

Arguments:

    None.

Return Value:

    Returns the number of bytes ready to be read.

--*/

{

    DWORD BytesAvailable;
    BOOL Result;
    COMSTAT SerialStatus;

    //
    // For both a named pipe and a serial port, determine how many bytes are
    // available to be received.
    //

    switch (CommChannelType) {
    case CommChannelPipe:
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
        if (Result == FALSE) {
            BytesAvailable = 0;

        } else {
            BytesAvailable = SerialStatus.cbInQue;
        }

        break;

    default:

        assert(FALSE);

        BytesAvailable = 0;
        break;
    }

    return BytesAvailable;
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

