/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    remsrv.c

Abstract:

    This module implements remote debug server functionality.

Author:

    Evan Green 27-Aug-2014

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/debug/spproto.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "symbols.h"
#include "dbgapi.h"
#include "dbgrprof.h"
#include "console.h"
#include "userdbg.h"
#include "dbgrcomm.h"
#include "extsp.h"
#include "consio.h"
#include "remsrv.h"
#include "sock.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define DEBUGGER_SERVER_USAGE                                                  \
    "Usage: server [-r] <host> <port>\n"                                       \
    "       server <port>\n"                                                   \
    "       server help\n"                                                     \
    "       server status\n"                                                   \
    "       server stop\n"                                                     \
    "This command opens up a debug server that others can connect to. \n"      \
    "If -r is specified, then the server will connect in reverse mode, \n"     \
    "reaching out to a single client directly. This is useful in situations \n"\
    "where the server cannot accept incoming connections.\n"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
DbgrpServerThread (
    PVOID Parameter
    );

PVOID
DbgrpServerConnectionThread (
    PVOID Parameter
    );

PVOID
DbgrpServerConnectionReceiveThread (
    PVOID Parameter
    );

PVOID
DbgrpClientNetworkThread (
    PVOID Parameter
    );

INT
DbgrpClientSendInformation (
    PDEBUGGER_CONTEXT Context,
    INT Socket
    );

INT
DbgrpRemoteSendCommand (
    int Socket,
    PDEBUG_REMOTE_HEADER Header
    );

INT
DbgrpRemoteReceiveCommand (
    int Socket,
    PDEBUG_REMOTE_HEADER *Header
    );

INT
DbgrpRemoteSendData (
    int Socket,
    PVOID Data,
    ULONGLONG DataSize
    );

INT
DbgrpRemoteReceiveData (
    int Socket,
    PVOID Data,
    ULONGLONG DataSize
    );

INT
DbgrpServerCreateClient (
    PDEBUGGER_CONTEXT Context,
    INT ClientSocket,
    PSTR ClientHost,
    INT ClientPort
    );

VOID
DbgrpServerDestroyClient (
    PDEBUGGER_SERVER_CLIENT Client
    );

VOID
DbgrpServerAcquireLock (
    PDEBUGGER_CONTEXT Context
    );

VOID
DbgrpServerReleaseLock (
    PDEBUGGER_CONTEXT Context
    );

INT
DbgrpClientConvertRemoteAddressString (
    PSTR RemoteString,
    PSTR *HostString,
    PINT Port
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the set of commands that affect the local debugger, even when it's
// acting as a remote client.
//

PSTR DbgrLocalOnlyCommands[] = {
    "q",
    "srcpath",
    "srcpath+",
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

INT
DbgrServerCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine starts or stops a remote server interface.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    char *AfterScan;
    char *Host;
    char *HostCopy;
    BOOL LockHeld;
    int Port;
    BOOL Reverse;
    int Socket;
    int Status;

    Host = NULL;
    LockHeld = FALSE;
    Port = 0;
    Reverse = FALSE;
    Socket = -1;
    if (ArgumentCount == 2) {
        if (strstr(Arguments[1], "help") == 0) {
            DbgOut(DEBUGGER_SERVER_USAGE);
            Status = 0;
            goto ServerCommandEnd;

        } else if (strcasecmp(Arguments[1], "status") == 0) {
            if (Context->Server.Socket == -1) {
                DbgOut("Debug server not connected.\n");

            } else {
                if (Context->Server.Host != NULL) {
                    DbgOut("Debug server listening at address %s, port %d.\n",
                           Context->Server.Host,
                           Context->Server.Port);

                } else {
                    DbgOut("Debug server listening on port %d.\n",
                           Context->Server.Port);
                }
            }

            Status = 0;
            goto ServerCommandEnd;

        } else if (strcasecmp(Arguments[1], "stop") == 0) {
            if (Context->Server.Socket == -1) {
                DbgOut("Debug server not connected.\n");

            } else {
                DbgrpServerDestroy(Context);
            }

            Status = 0;
            goto ServerCommandEnd;
        }

        Port = strtoul(Arguments[1], &AfterScan, 0);
        if ((AfterScan == Arguments[1]) || (*AfterScan != '\0')) {
            DbgOut("Invalid port number '%s'.\n", Arguments[1]);
            Status = EINVAL;
            goto ServerCommandEnd;
        }

    } else if (ArgumentCount == 3) {
        Host = Arguments[1];
        Port = strtoul(Arguments[2], &AfterScan, 0);
        if ((AfterScan == Arguments[2]) || (*AfterScan != '\0')) {
            DbgOut("Invalid port number '%s'.\n", Arguments[1]);
            Status = EINVAL;
            goto ServerCommandEnd;
        }

    } else if (ArgumentCount == 4) {
        if (strcasecmp(Arguments[1], "-r") != 0) {
            DbgOut("Unknown argument %s.\n", Arguments[1]);
            Status = EINVAL;
            goto ServerCommandEnd;
        }

        Reverse = TRUE;
        Host = Arguments[2];
        Port = strtoul(Arguments[3], &AfterScan, 0);
        if ((AfterScan == Arguments[3]) || (*AfterScan != '\0')) {
            DbgOut("Invalid port number '%s'.\n", Arguments[1]);
            Status = EINVAL;
            goto ServerCommandEnd;
        }

    } else if (ArgumentCount > 4) {
        DbgOut("Too many arguments. Try --help for usage.\n");
        Status = EINVAL;
        goto ServerCommandEnd;
    }

    if (Context->Server.Socket != -1) {
        DbgOut("Debug server already listening. Run server stop to kill it.\n");
        Status = EINVAL;
        goto ServerCommandEnd;
    }

    DbgrSocketInitializeLibrary();
    Socket = DbgrSocketCreateStreamSocket();
    if (Socket == -1) {
        DbgOut("Failed to create socket.\n");
        Status = EINVAL;
        goto ServerCommandEnd;
    }

    //
    // Do an ugly conversion until someone can be bothered to use
    // getnameinfo.
    //

    if ((Host != NULL) && (strcmp(Host, "localhost") == 0)) {
        Host = NULL;
    }

    //
    // In reverse mode, reach out to the client directly.
    //

    if (Reverse != FALSE) {
        if (Host == NULL) {
            Host = "127.0.0.1";
        }

        Status = DbgrSocketConnect(Socket, Host, Port);
        if (Status != 0) {
            DbgOut("Failed to connect to %s on port %d: %s\n",
                   Host,
                   Port,
                   strerror(errno));

            Status = errno;
            goto ServerCommandEnd;
        }

        HostCopy = strdup(Host);
        Status = DbgrpServerCreateClient(Context, Socket, HostCopy, Port);
        if (Status != 0) {
            DbgOut("Failed to create client: %s\n", strerror(Status));
            DbgrSocketClose(Socket);
            Socket = -1;
            if (HostCopy != NULL) {
                free(HostCopy);
            }

            goto ServerCommandEnd;
        }

        Socket = -1;

    } else {
        Status = DbgrSocketBind(Socket, Host, Port);
        if (Status != 0) {
            DbgOut("Failed to bind to port %d.\n", Port);
            goto ServerCommandEnd;
        }

        Status = DbgrSocketListen(Socket);
        if (Status != 0) {
            DbgOut("Failed to listen: %s\n", strerror(errno));
            goto ServerCommandEnd;
        }

        DbgrpServerAcquireLock(Context);
        LockHeld = TRUE;
        Context->Server.ShutDown = 1;
        Status = DbgrOsCreateThread(DbgrpServerThread, Context);
        if (Status != 0) {
            goto ServerCommandEnd;
        }

        Context->Server.Socket = Socket;
        Context->Server.Host = NULL;
        Context->Server.Port = 0;
        Status = DbgrSocketGetName(Socket, &Host, &Port);
        if (Status == 0) {
            Context->Server.Host = Host;
            Context->Server.Port = Port;
        }

        Socket = -1;

        //
        // Wait for the server thread to come online before continuing.
        //

        while (Context->Server.ShutDown != 0) {
            CommStall(10);
        }

        DbgrpServerReleaseLock(Context);
        LockHeld = FALSE;
        DbgOut("Server listening on %s:%d\n",
               Context->Server.Host,
               Context->Server.Port);
    }

    Status = 0;

ServerCommandEnd:
    if (Socket != -1) {
        DbgrSocketClose(Socket);
    }

    if (LockHeld != FALSE) {
        DbgrpServerReleaseLock(Context);
    }

    return Status;
}

INT
DbgrClientMainLoop (
    PDEBUGGER_CONTEXT Context,
    PSTR RemoteString,
    BOOL ReverseRemote
    )

/*++

Routine Description:

    This routine implements the main loop of the debugger when connected to a
    remote server.

Arguments:

    Context - Supplies a pointer to the debugger context.

    RemoteString - Supplies a pointer to the remote host to connect to.

    ReverseRemote - Supplies a boolean indicating whether or not the client
        should act in "reverse": opening up a port and waiting for the server
        to connect. This is useful in situations where the server cannot
        accept incoming connections.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDEBUG_REMOTE_HEADER Command;
    ULONG CommandArgumentCount;
    PSTR CommandArguments[DEBUGGER_MAX_COMMAND_ARGUMENTS];
    PDEBUGGER_COMMAND_ENTRY CommandEntry;
    UINTN CommandIndex;
    INT Length;
    PSTR LocalHost;
    INT LocalPort;
    INT Match;
    INT Port;
    PSTR RemoteHost;
    PSTR RemoteServer;
    INT RemoteServerPort;
    INT RemoteServerSocket;
    INT Result;
    int Socket;

    RemoteHost = NULL;
    Socket = -1;
    Result = DbgrpClientConvertRemoteAddressString(RemoteString,
                                                   &RemoteHost,
                                                   &Port);

    if (Result != 0) {
        DbgOut("Invalid host string: '%s'.\n", RemoteString);
        goto ClientMainLoopEnd;
    }

    DbgrSocketInitializeLibrary();
    Socket = DbgrSocketCreateStreamSocket();
    if (Socket < 0) {
        DbgOut("Failed to create socket.\n");
        goto ClientMainLoopEnd;
    }

    Context->Client.Socket = Socket;

    //
    // If running in reverse, bind to the given host/port, and wait for an
    // incoming connection.
    //

    if (ReverseRemote != FALSE) {
        Result = DbgrSocketBind(Socket, RemoteHost, Port);
        if (Result != 0) {
            DbgOut("Failed to bind to %s:%d: %s.\n",
                   RemoteHost,
                   Port,
                   strerror(errno));

            goto ClientMainLoopEnd;
        }

        Result = DbgrSocketListen(Socket);
        if (Result != 0) {
            DbgOut("Failed to listen.\n");
            goto ClientMainLoopEnd;
        }

        Result = DbgrSocketGetName(Socket, &LocalHost, &LocalPort);
        if (Result == 0) {
            DbgOut("Waiting for connection on %s:%d...\n",
                   LocalHost,
                   LocalPort);

            if (LocalHost != NULL) {
                free(LocalHost);
            }

        } else {
            DbgOut("Waiting for connection...\n");
        }

        RemoteServerSocket = DbgrSocketAccept(Socket,
                                              &RemoteServer,
                                              &RemoteServerPort);

        if (RemoteServerSocket < 0) {
            DbgOut("Failed to accept incoming connection.\n");
            goto ClientMainLoopEnd;
        }

        DbgOut("Connected to %s:%d\n", RemoteServer, RemoteServerPort);
        if (RemoteServer != NULL) {
            free(RemoteServer);
        }

        //
        // Replace the main socket with the newly accepted connection.
        //

        DbgrSocketClose(Socket);
        Socket = RemoteServerSocket;
        Context->Client.Socket = Socket;

    } else {
        DbgOut("Connecting to %s:%d...\n", RemoteHost, Port);
        Result = DbgrSocketConnect(Socket, RemoteHost, Port);
        if (Result != 0) {
            DbgOut("Failed to connect to %s.\n", RemoteHost);
            goto ClientMainLoopEnd;
        }
    }

    Result = DbgrpClientSendInformation(Context, Socket);
    if (Result != 0) {
        DbgOut("Failed to send client information.\n");
        goto ClientMainLoopEnd;
    }

    UiSetCommandText("");
    Result = DbgrOsCreateThread(DbgrpClientNetworkThread, Context);
    if (Result != 0) {
        DbgOut("Failed to create client network thread.\n");
        Context->Client.Socket = -1;
        goto ClientMainLoopEnd;
    }

    //
    // Don't echo commands, as the server does that.
    //

    Context->Flags &= ~DEBUGGER_FLAG_ECHO_COMMANDS;

    //
    // Loop breaking in and waiting for the target.
    //

    while ((Context->Flags & DEBUGGER_FLAG_EXITING) == 0) {

        //
        // Process a command from the user.
        //

        Result = DbgrGetCommand(Context);
        if (Result == FALSE) {
            Result = EINVAL;
            goto ClientMainLoopEnd;
        }

        if (Context->CommandBuffer[0] == '\0') {
            continue;
        }

        //
        // Determine if this command should be acted on locally.
        //

        CommandIndex = 0;
        Match = 1;
        while (DbgrLocalOnlyCommands[CommandIndex] != NULL) {
            Length = strlen(DbgrLocalOnlyCommands[CommandIndex]);
            Match = strncasecmp(Context->CommandBuffer,
                                DbgrLocalOnlyCommands[CommandIndex],
                                Length);

            if ((Match == 0) &&
                ((isspace(Context->CommandBuffer[Length]) != 0) ||
                 (Context->CommandBuffer[Length] == '\0'))) {

                Result = DbgrpSplitCommandArguments(Context->CommandBuffer,
                                                    CommandArguments,
                                                    &CommandArgumentCount);

                if (Result == FALSE) {
                    Result = EINVAL;
                    goto ClientMainLoopEnd;
                }

                CommandEntry = DbgrLookupCommand(CommandArguments[0]);

                assert(CommandEntry != NULL);

                DbgOut("\n");
                CommandEntry->CommandRoutine(Context,
                                             CommandArguments,
                                             CommandArgumentCount);

                DbgOut("%s", Context->StandardOut.Prompt);
                break;
            }

            CommandIndex += 1;
        }

        if (Match == 0) {
            continue;
        }

        //
        // Send the command to the remote server.
        //

        Length = strlen(Context->CommandBuffer);
        Command = malloc(Length + sizeof(DEBUG_REMOTE_HEADER));
        if (Command == NULL) {
            DbgOut("Allocation failure.\n");
            continue;
        }

        Command->Command = DebugRemoteInput;
        Command->Length = Length;
        memcpy(Command + 1, Context->CommandBuffer, Length);
        Result = DbgrpRemoteSendCommand(Socket, Command);
        if (Result != 0) {
            DbgOut("Failed to send command.\n");
            break;
        }
    }

    //
    // Wait for the client thread to stop.
    //

    if (Context->Client.Socket != -1) {
        DbgrSocketShutdown(Socket);
        while (Context->Client.ShutDown == 0) {
            CommStall(10);
        }
    }

ClientMainLoopEnd:
    if (Socket != -1) {
        DbgrSocketClose(Socket);
        Context->Client.Socket = -1;
    }

    if (RemoteHost != NULL) {
        free(RemoteHost);
    }

    DbgrSocketDestroyLibrary();
    return Result;
}

INT
DbgrpClientRequestBreakIn (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine sends a break request across to the debug server.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    0 on success.

    Non-zero on error.

--*/

{

    DEBUG_REMOTE_HEADER Header;
    INT Result;

    Header.Command = DebugRemoteBreakRequest;
    Header.Length = 0;
    Result = DbgrpRemoteSendCommand(Context->Client.Socket, &Header);
    return Result;
}

VOID
DbgrpServerNotifyClients (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine notifies all debug clients connected to the given server that
    there is new activity to send off to the clients. This routine assumes the
    standard output lock is already held.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

{

    ssize_t BytesWritten;
    PDEBUGGER_SERVER_CLIENT Client;
    PLIST_ENTRY CurrentEntry;
    char OutputCharacter;

    //
    // Note that if the server is ever changed to synchronize on something
    // other than the standard output lock, then it would need to be acquired
    // here. All callers of this function are holding the standard output lock.
    //

    OutputCharacter = 'o';
    CurrentEntry = Context->Server.ClientList.Next;
    while (CurrentEntry != &(Context->Server.ClientList)) {
        Client = LIST_VALUE(CurrentEntry, DEBUGGER_SERVER_CLIENT, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        assert(Client->Context == Context);

        //
        // Wake up the client by writing to its pipe.
        //

        if ((Client->Pipe[1] != -1) && (Client->Update == 0)) {
            Client->Update = 1;
            do {
                BytesWritten = write(Client->Pipe[1], &OutputCharacter, 1);

            } while ((BytesWritten < 0) && (errno == EINTR));
        }
    }

    return;
}

VOID
DbgrpServerDestroy (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine tears down the debug server and all its connections.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

{

    PDEBUGGER_SERVER_CLIENT Client;
    PLIST_ENTRY CurrentEntry;

    DbgrpServerAcquireLock(Context);
    if (Context->Server.Socket != -1) {
        Context->Server.ShutDown = 1;
        DbgrSocketClose(Context->Server.Socket);
        Context->Server.Socket = -1;
    }

    if (Context->Client.Socket != -1) {
        DbgrSocketShutdown(Context->Client.Socket);
    }

    if (Context->Server.Host != NULL) {
        free(Context->Server.Host);
        Context->Server.Host = NULL;
    }

    Context->Server.Port = 0;
    CurrentEntry = Context->Server.ClientList.Next;
    while (CurrentEntry != &(Context->Server.ClientList)) {
        Client = LIST_VALUE(CurrentEntry, DEBUGGER_SERVER_CLIENT, ListEntry);
        if (Client->Socket != -1) {
            DbgrSocketShutdown(Client->Socket);
        }

        //
        // Close the write end of the pipe to unblock the connection thread
        // trying to read it.
        //

        if (Client->Pipe[1] != -1) {
            close(Client->Pipe[1]);
            Client->Pipe[1] = -1;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    DbgrpServerReleaseLock(Context);
    while ((LIST_EMPTY(&(Context->Server.ClientList)) == FALSE) ||
           (Context->Server.ShutDown != 0)) {

        CommStall(10);
        DbgrpServerAcquireLock(Context);
        DbgrpServerReleaseLock(Context);
    }

    //
    // Acquire and release the lock one more time as a barrier.
    //

    DbgrpServerAcquireLock(Context);
    DbgrpServerReleaseLock(Context);
    DbgrSocketDestroyLibrary();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
DbgrpServerThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the entry point for the debug server thread that simply
    accepts new connections and spawns worker threads to handle them.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. In
        this case, the debugger application context.

Return Value:

    NULL always.

--*/

{

    char *ClientHost;
    int ClientPort;
    int ClientSocket;
    PDEBUGGER_CONTEXT Context;
    int Result;
    int Socket;

    Context = Parameter;

    //
    // Mark that the server thread has fired up.
    //

    Context->Server.ShutDown = 0;

    //
    // Loop accepting connections.
    //

    while (TRUE) {
        Socket = Context->Server.Socket;
        if (Context->Server.ShutDown != 0) {
            break;
        }

        ClientHost = NULL;
        ClientSocket = DbgrSocketAccept(Socket, &ClientHost, &ClientPort);
        if (ClientSocket < 0) {
            continue;
        }

        Result = DbgrpServerCreateClient(Context,
                                         ClientSocket,
                                         ClientHost,
                                         ClientPort);

        if (Result != 0) {
            DbgrSocketClose(ClientSocket);
            if (ClientHost != NULL) {
                free(ClientHost);
            }
        }
    }

    //
    // Mark that the server thread is done.
    //

    Context->Server.ShutDown = 0;
    return NULL;
}

PVOID
DbgrpServerConnectionThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the entry point for the thread that manages an individual
    connection with a client for the debug server.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. In
        this case, a pointer to the debugger server client connection.

Return Value:

    NULL always.

--*/

{

    PDEBUGGER_SERVER_CLIENT Client;
    PDEBUG_REMOTE_CLIENT_INFORMATION ClientInformation;
    PDEBUGGER_CONTEXT Context;
    PDEBUG_REMOTE_HEADER Header;
    BOOL LockHeld;
    void *Output;
    ULONGLONG OutputIndex;
    char PipeCharacter;
    PSTR Prompt;
    int Result;
    DEBUG_REMOTE_SERVER_INFORMATION ServerInformation;
    int Size;
    BOOL SourceAvailable;
    PSTR SourceFile;
    ULONG SourceFileLength;
    PDEBUG_REMOTE_SOURCE_INFORMATION SourceInformation;
    ULONGLONG SourceLine;

    Client = Parameter;
    Context = Client->Context;
    LockHeld = FALSE;
    Output = NULL;
    OutputIndex = 0;
    Prompt = NULL;
    SourceFile = NULL;
    SourceInformation = NULL;
    memset(&ServerInformation, 0, sizeof(DEBUG_REMOTE_SERVER_INFORMATION));
    ServerInformation.Header.Command = DebugRemoteServerInformation;
    ServerInformation.Header.Length =
                sizeof(DEBUG_REMOTE_SERVER_INFORMATION) -
                FIELD_OFFSET(DEBUG_REMOTE_SERVER_INFORMATION, ProtocolVersion);

    ServerInformation.ProtocolVersion = DEBUG_REMOTE_PROTOCOL_VERSION;
    Result = DbgrpRemoteSendCommand(Client->Socket,
                                    &(ServerInformation.Header));

    if (Result != 0) {
        DbgOut("Failed to send server information to client.\n");
        goto ServerConnectionThreadEnd;
    }

    Result = DbgrpRemoteReceiveCommand(
                                   Client->Socket,
                                   (PDEBUG_REMOTE_HEADER *)&ClientInformation);

    if (Result != 0) {
        DbgOut("Failed to receive client information.\n");
        goto ServerConnectionThreadEnd;
    }

    if (ClientInformation->Header.Command != DebugRemoteClientInformation) {
        DbgOut("Received something other than remote client information.\n");
        free(ClientInformation);
        goto ServerConnectionThreadEnd;
    }

    ClientInformation->User[DEBUG_REMOTE_USER_SIZE - 1] = '\0';
    ClientInformation->Host[DEBUG_REMOTE_HOST_SIZE - 1] = '\0';
    Client->HostName = strdup(ClientInformation->Host);
    Client->UserName = strdup(ClientInformation->User);
    free(ClientInformation);
    DbgOut("\nUser %s on %s connected at %s:%d.\n",
           Client->UserName,
           Client->HostName,
           Client->Host,
           Client->Port);

    //
    // Make sure the user name is something readable. Replace the user name
    // with the host name or the host address if it started empty.
    //

    if ((Client->UserName == NULL) || (*(Client->UserName) == '\0')) {
        if (Client->UserName != NULL) {
            free(Client->UserName);
            Client->UserName = NULL;
        }

        if ((Client->HostName != NULL) && (*(Client->HostName) != '\0')) {
            Client->UserName = strdup(Client->HostName);

        } else {
            Client->UserName = strdup(Client->Host);
        }
    }

    //
    // Start the receive thread.
    //

    assert(Client->ReceiveState == DebuggerServerReceiveNotStarted);

    Client->ReceiveState = DebuggerServerReceiveRunning;
    Result = DbgrOsCreateThread(DbgrpServerConnectionReceiveThread, Client);
    if (Result != 0) {
        Client->ReceiveState = DebuggerServerReceiveNotStarted;
        goto ServerConnectionThreadEnd;
    }

    while (TRUE) {

        //
        // Clear the update flag before going through and doing the update.
        //

        Client->Update = 0;

        //
        // Loop writing output to the client.
        //

        Result = 0;
        while (OutputIndex != Context->StandardOut.ConsoleBufferSize) {
            DbgrpServerAcquireLock(Context);
            LockHeld = TRUE;

            //
            // The check in the while loop was not synchronized, so take a look
            // again now that the lock is held.
            //

            if (OutputIndex == Context->StandardOut.ConsoleBufferSize) {
                break;
            }

            //
            // Allocate and initialize buffer containing the output that has
            // not yet been sent.
            //

            Size = Context->StandardOut.ConsoleBufferSize - OutputIndex;

            assert(Size ==
                   Context->StandardOut.ConsoleBufferSize - OutputIndex);

            Output = malloc(Size + sizeof(DEBUG_REMOTE_HEADER));
            if (Output == NULL) {
                break;
            }

            memcpy(Output + sizeof(DEBUG_REMOTE_HEADER),
                   Context->StandardOut.ConsoleBuffer + OutputIndex,
                   Size);

            Header = Output;
            Header->Command = DebugRemoteOutput;
            Header->Length = Size;

            //
            // Drop the lock so the UI thread can continue, and then work on
            // sending the data.
            //

            DbgrpServerReleaseLock(Context);
            LockHeld = FALSE;
            Result = DbgrpRemoteSendCommand(Client->Socket, Header);
            if (Result != 0) {
                break;
            }

            OutputIndex += Size;
        }

        if (LockHeld != FALSE) {
            DbgrpServerReleaseLock(Context);
            LockHeld = FALSE;
        }

        if (Output != NULL) {
            free(Output);
            Output = NULL;
        }

        if (Result != 0) {
            break;
        }

        //
        // Check to see if the prompt has changed, and send the updated prompt
        // if so. Grab the source information while the lock is held.
        //

        assert((Prompt == NULL) && (SourceFile == NULL));

        SourceAvailable = FALSE;
        DbgrpServerAcquireLock(Context);
        if (Context->StandardOut.Prompt != NULL) {
            Prompt = strdup(Context->StandardOut.Prompt);
        }

        if (Context->SourceFile.Path != NULL) {
            SourceFile = strdup(Context->SourceFile.Path);
        }

        SourceLine = Context->SourceFile.LineNumber;
        if (Context->SourceFile.Contents != NULL) {
            SourceAvailable = TRUE;
        }

        DbgrpServerReleaseLock(Context);
        if (((Prompt == NULL) && (Client->Prompt != NULL)) ||
            ((Prompt != NULL) && (Client->Prompt == NULL)) ||
            ((Prompt != NULL) && (strcmp(Prompt, Client->Prompt) != 0))) {

            if (Client->Prompt != NULL) {
                free(Client->Prompt);
            }

            Client->Prompt = Prompt;
            Prompt = NULL;
            Size = 0;
            if (Client->Prompt != NULL) {
                Size = strlen(Client->Prompt);
            }

            Output = malloc(Size + sizeof(DEBUG_REMOTE_HEADER));
            if (Output == NULL) {
                break;
            }

            if (Size != 0) {
                memcpy(Output + sizeof(DEBUG_REMOTE_HEADER),
                       Client->Prompt,
                       Size);
            }

            Header = Output;
            Header->Command = DebugRemotePrompt;
            Header->Length = Size;
            Result = DbgrpRemoteSendCommand(Client->Socket, Header);
            if (Result != 0) {
                break;
            }
        }

        if (Prompt != NULL) {
            free(Prompt);
            Prompt = NULL;
        }

        if (Output != NULL) {
            free(Output);
            Output = NULL;
        }

        //
        // Send the updated source file and line if different.
        //

        if (((Client->SourceFile == NULL) && (SourceFile != NULL)) ||
            ((Client->SourceFile != NULL) && (SourceFile == NULL)) ||
            (Client->SourceLine != SourceLine) ||
            ((SourceFile != NULL) &&
             (strcmp(Client->SourceFile, SourceFile) != 0))) {

            if (Client->SourceFile != NULL) {
                free(Client->SourceFile);
                Client->SourceFile = NULL;
            }

            SourceFileLength = 0;
            if (SourceFile != NULL) {
                SourceFileLength = strlen(SourceFile);
            }

            SourceInformation = malloc(
                   sizeof(DEBUG_REMOTE_SOURCE_INFORMATION) + SourceFileLength);

            if (SourceInformation != NULL) {
                SourceInformation->Header.Command =
                                                  DebugRemoteSourceInformation;

                SourceInformation->Header.Length =
                    SourceFileLength +
                    (sizeof(DEBUG_REMOTE_SOURCE_INFORMATION) -
                     FIELD_OFFSET(DEBUG_REMOTE_SOURCE_INFORMATION, LineNumber));

                if (SourceFileLength != 0) {
                    memcpy(SourceInformation + 1,
                           SourceFile,
                           SourceFileLength);
                }

                SourceInformation->LineNumber = SourceLine;
                SourceInformation->SourceAvailable = SourceAvailable;
                Result = DbgrpRemoteSendCommand(Client->Socket,
                                                &(SourceInformation->Header));

                if (Result != 0) {
                    break;
                }
            }

            Client->SourceFile = SourceFile;
            SourceFile = NULL;
            Client->SourceLine = SourceLine;
        }

        if (SourceFile != NULL) {
            free(SourceFile);
            SourceFile = NULL;
        }

        if (SourceInformation != NULL) {
            free(SourceInformation);
            SourceInformation = NULL;
        }

        //
        // If there's still another update to do, go back and do it.
        //

        if (Client->Update != 0) {
            continue;
        }

        //
        // Block on the pipe, which will be written to when there's something
        // to do.
        //

        Result = read(Client->Pipe[0], &PipeCharacter, 1);
        if (Result <= 0) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }
    }

ServerConnectionThreadEnd:

    assert(LockHeld == FALSE);

    if (Output != NULL) {
        free(Output);
    }

    if (Prompt != NULL) {
        free(Prompt);
    }

    if (SourceFile != NULL) {
        free(SourceFile);
    }

    if (SourceInformation != NULL) {
        free(SourceInformation);
    }

    //
    // Stop the receive thread if needed.
    //

    if (Client->ReceiveState == DebuggerServerReceiveRunning) {
        Client->ReceiveState = DebuggerServerReceiveShutDownRequested;
        DbgrSocketShutdown(Client->Socket);
        while (Client->ReceiveState != DebuggerServerReceiveShutDown) {
            CommStall(10);
        }
    }

    DbgOut("\nDisconnected from %s:%d.\n", Client->Host, Client->Port);
    DbgrpServerAcquireLock(Context);
    DbgrpServerDestroyClient(Client);
    DbgrpServerReleaseLock(Context);
    return NULL;
}

PVOID
DbgrpServerConnectionReceiveThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the entry point for the thread that receives requests and
    input from a remote client.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. In
        this case, a pointer to the debugger context.

Return Value:

    NULL always.

--*/

{

    PDEBUGGER_SERVER_CLIENT Client;
    PDEBUGGER_CONTEXT Context;
    PDEBUG_REMOTE_HEADER Header;
    PDEBUGGER_REMOTE_COMMAND RemoteCommand;
    INT Result;
    INT Retries;
    PDEBUG_REMOTE_SOURCE_DATA SourceData;

    Client = Parameter;
    Context = Client->Context;
    Header = NULL;
    Retries = 10;
    while (Client->ReceiveState == DebuggerServerReceiveRunning) {
        Result = DbgrpRemoteReceiveCommand(Client->Socket, &Header);
        if (Result != 0) {
            Retries -= 1;
            if (Retries == 0) {
                break;
            }

            continue;
        }

        Retries = 10;
        switch (Header->Command) {

        //
        // Add a remote input command.
        //

        case DebugRemoteInput:
            RemoteCommand = malloc(sizeof(DEBUGGER_REMOTE_COMMAND));
            if (RemoteCommand == NULL) {
                break;
            }

            memset(RemoteCommand, 0, sizeof(DEBUGGER_REMOTE_COMMAND));
            RemoteCommand->Command = malloc(Header->Length + 1);
            if (RemoteCommand->Command == NULL) {
                free(RemoteCommand);
                break;
            }

            memcpy(RemoteCommand->Command, Header + 1, Header->Length);
            RemoteCommand->Command[Header->Length] = '\0';
            if (Client->UserName != NULL) {
                RemoteCommand->User = strdup(Client->UserName);
            }

            if (Client->HostName != NULL) {
                RemoteCommand->Host = strdup(Client->HostName);
            }

            AcquireDebuggerLock(Context->StandardIn.Lock);
            INSERT_BEFORE(&(RemoteCommand->ListEntry),
                          &(Context->StandardIn.RemoteCommandList));

            ReleaseDebuggerLock(Context->StandardIn.Lock);
            DbgrOsRemoteInputAdded();
            break;

        case DebugRemoteBreakRequest:
            DbgOut("Requesting break in...\t\t[%s@%s]\n",
                   Client->UserName,
                   Client->HostName);

            DbgRequestBreakIn(Context);
            break;

        case DebugRemoteSourceDataRequest:

            //
            // Send the current source data. The source file is protected by
            // the standard out lock, which is the same physically but not
            // conceptually as the server lock.
            //

            AcquireDebuggerLock(Context->StandardOut.Lock);
            SourceData = malloc(sizeof(DEBUG_REMOTE_SOURCE_DATA) +
                                Context->SourceFile.Size);

            if (SourceData != NULL) {
                SourceData->Header.Command = DebugRemoteSourceData;
                SourceData->Header.Length =
                    sizeof(DEBUG_REMOTE_SOURCE_DATA) -
                    (FIELD_OFFSET(DEBUG_REMOTE_SOURCE_DATA, FileNameCrc32)) +
                    Context->SourceFile.Size;

                SourceData->FileNameCrc32 = 0;
                if (Context->SourceFile.Path != NULL) {
                    SourceData->FileNameCrc32 = RtlComputeCrc32(
                                    0,
                                    Context->SourceFile.Path,
                                    RtlStringLength(Context->SourceFile.Path));
                }

                memcpy(SourceData + 1,
                       Context->SourceFile.Contents,
                       Context->SourceFile.Size);
            }

            ReleaseDebuggerLock(Context->StandardOut.Lock);
            if (SourceData != NULL) {
                DbgrpRemoteSendCommand(Client->Socket, &(SourceData->Header));
                free(SourceData);
            }

            break;

        default:
            DbgOut("Unknown remote command 0x%x received.\n", Header->Command);
            break;
        }
    }

    Client->ReceiveState = DebuggerServerReceiveShutDown;
    return NULL;
}

PVOID
DbgrpClientNetworkThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the entry point for the thread that manages network traffic
    from a remote client.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. In
        this case, a pointer to the debugger context.

Return Value:

    NULL always.

--*/

{

    PDEBUGGER_CONTEXT Context;
    ULONG CurrentNameLength;
    PDEBUG_REMOTE_HEADER Header;
    ULONG NameCrc32;
    PSTR Output;
    INT Result;
    PDEBUG_REMOTE_SOURCE_DATA SourceData;
    PSTR SourceFile;
    PSTR SourceFileBuffer;
    ULONGLONG SourceFileLength;
    PDEBUG_REMOTE_SOURCE_INFORMATION SourceInformation;
    ULONGLONG SourceLine;

    Context = Parameter;
    Context->Client.ShutDown = 0;
    while (TRUE) {
        Result = DbgrpRemoteReceiveCommand(Context->Client.Socket, &Header);
        if (Result != 0) {
            break;
        }

        assert(Header != NULL);

        switch (Header->Command) {
        case DebugRemoteOutput:
        case DebugRemotePrompt:
            if ((Header->Command == DebugRemotePrompt) &&
                (Header->Length == 0)) {

                DbgrpSetPromptText(Context, NULL);
                UiEnableCommands(FALSE);
                break;
            }

            Output = malloc(Header->Length + 1);
            if (Output != NULL) {
                memcpy(Output, Header + 1, Header->Length);
                Output[Header->Length] = '\0';
                if (Header->Command == DebugRemoteOutput) {
                    DbgOut("%s", Output);

                } else {

                    assert(Header->Command == DebugRemotePrompt);

                    DbgrpSetPromptText(Context, Output);
                    UiEnableCommands(TRUE);
                }

                free(Output);
            }

            break;

        case DebugRemoteSourceInformation:

            //
            // The source file name comes after the structure, so the source
            // file size is the whole payload minus the fields in the source
            // information (the header doesn't count in the length).
            //

            SourceInformation = (PDEBUG_REMOTE_SOURCE_INFORMATION)Header;

            //
            // Skip bogus lengths.
            //

            if (Header->Length <
                (sizeof(DEBUG_REMOTE_SOURCE_INFORMATION) -
                 FIELD_OFFSET(DEBUG_REMOTE_SOURCE_INFORMATION, LineNumber))) {

                break;
            }

            SourceFileLength =
                   Header->Length -
                   (sizeof(DEBUG_REMOTE_SOURCE_INFORMATION) -
                    FIELD_OFFSET(DEBUG_REMOTE_SOURCE_INFORMATION, LineNumber));

            SourceFileBuffer = (PSTR)(SourceInformation + 1);
            SourceFile = NULL;
            if (SourceFileLength != 0) {
                SourceFile = malloc(SourceFileLength + 1);
                if (SourceFile != NULL) {
                    memcpy(SourceFile, SourceFileBuffer, SourceFileLength);
                    SourceFile[SourceFileLength] = '\0';
                }
            }

            SourceLine = SourceInformation->LineNumber;

            //
            // The standard out lock protects the source file (which is the
            // same physically but not conceptually as the debug server lock).
            //

            AcquireDebuggerLock(Context->StandardOut.Lock);

            //
            // If the line number is zero, just unhighlight the line.
            //

            if (SourceInformation->LineNumber == 0) {
                DbgrpHighlightExecutingLine(Context, 0);

            } else {

                assert(SourceFile != NULL);

                //
                // If the file is the same, just move the line number.
                //

                if ((Context->SourceFile.Path != NULL) &&
                    (strcmp(Context->SourceFile.Path, SourceFile) == 0)) {

                    DbgrpHighlightExecutingLine(Context, SourceLine);

                //
                // The file needs to be loaded.
                //

                } else {
                    if (Context->SourceFile.Path != NULL) {
                        free(Context->SourceFile.Path);
                        Context->SourceFile.Path = NULL;
                    }

                    if (Context->SourceFile.ActualPath != NULL) {
                        free(Context->SourceFile.ActualPath);
                        Context->SourceFile.ActualPath = NULL;
                    }

                    if (Context->SourceFile.Contents != NULL) {
                        free(Context->SourceFile.Contents);
                        Context->SourceFile.Contents = NULL;
                    }

                    Context->SourceFile.LineNumber = 0;
                    Context->SourceFile.Path = SourceFile;

                    //
                    // First try to load the source locally.
                    //

                    Result = DbgrpLoadSourceFile(
                                             Context,
                                             SourceFile,
                                             &(Context->SourceFile.ActualPath),
                                             &(Context->SourceFile.Contents),
                                             &(Context->SourceFile.Size));

                    if (Result == 0) {
                        Result = UiLoadSourceFile(
                                                Context->SourceFile.ActualPath,
                                                Context->SourceFile.Contents,
                                                Context->SourceFile.Size);

                        if (Result != FALSE) {
                            DbgrpHighlightExecutingLine(Context, SourceLine);
                        }

                    //
                    // Source could not be loaded locally, try to request it
                    // from the server.
                    //

                    } else if (SourceInformation->SourceAvailable != 0) {

                        //
                        // Reuse the header to request the source data.
                        //

                        SourceInformation = NULL;
                        Header->Command = DebugRemoteSourceDataRequest;
                        Header->Length = 0;
                        Result = DbgrpRemoteSendCommand(Context->Client.Socket,
                                                        Header);

                        //
                        // If sending the request failed, blank out the screen.
                        //

                        if (Result != 0) {
                            UiLoadSourceFile(SourceFile, NULL, 0);
                        }

                        //
                        // Save the source line in the context for now.
                        //

                        Context->SourceFile.LineNumber = SourceLine;
                    }

                    SourceFile = NULL;
                }
            }

            ReleaseDebuggerLock(Context->StandardOut.Lock);
            if (SourceFile != NULL) {
                free(SourceFile);
            }

            break;

        case DebugRemoteSourceData:

            //
            // A response to a previous request to source data has come in.
            // Load the file finally.
            //

            SourceData = (PDEBUG_REMOTE_SOURCE_DATA)Header;

            //
            // Skip bogus lengths.
            //

            if (Header->Length <
                (sizeof(DEBUG_REMOTE_SOURCE_DATA) -
                 FIELD_OFFSET(DEBUG_REMOTE_SOURCE_DATA, FileNameCrc32))) {

                break;
            }

            SourceFileLength =
                       Header->Length -
                       (sizeof(DEBUG_REMOTE_SOURCE_DATA) -
                        FIELD_OFFSET(DEBUG_REMOTE_SOURCE_DATA, FileNameCrc32));

            SourceFileBuffer = (PSTR)(SourceData + 1);
            SourceFile = NULL;
            if (SourceFileLength != 0) {
                SourceFile = malloc(SourceFileLength + 1);
                if (SourceFile != NULL) {
                    memcpy(SourceFile, SourceFileBuffer, SourceFileLength);
                    SourceFile[SourceFileLength] = '\0';
                }
            }

            //
            // The standard out lock protects the source file (which is the
            // same physically but not conceptually as the debug server lock).
            //

            AcquireDebuggerLock(Context->StandardOut.Lock);
            if (Context->SourceFile.Path != NULL) {
                CurrentNameLength = RtlStringLength(Context->SourceFile.Path);
                NameCrc32 = RtlComputeCrc32(0,
                                            Context->SourceFile.Path,
                                            CurrentNameLength);

                //
                // If this data refers the same file as the client was
                // expecting, load it.
                //

                if (NameCrc32 == SourceData->FileNameCrc32) {

                    assert(Context->SourceFile.Contents == NULL);

                    Context->SourceFile.Contents = SourceFile;
                    Context->SourceFile.Size = SourceFileLength;
                    SourceFile = NULL;
                    Result = UiLoadSourceFile(Context->SourceFile.Path,
                                              Context->SourceFile.Contents,
                                              Context->SourceFile.Size);

                    if (Result != FALSE) {
                        SourceLine = Context->SourceFile.LineNumber;
                        Context->SourceFile.LineNumber = 0;
                        DbgrpHighlightExecutingLine(Context, SourceLine);
                    }
                }
            }

            ReleaseDebuggerLock(Context->StandardOut.Lock);
            if (SourceFile != NULL) {
                free(SourceFile);
            }

            break;

        default:
            DbgOut("Received unknown remote server command %d.\n",
                   Header->Command);

            break;
        }

        free(Header);
    }

    DbgrSocketClose(Context->Client.Socket);
    Context->Client.ShutDown = 1;
    return NULL;
}

INT
DbgrpClientSendInformation (
    PDEBUGGER_CONTEXT Context,
    INT Socket
    )

/*++

Routine Description:

    This routine sends sends client information to the remote server, and
    collects the server information.

Arguments:

    Context - Supplies the application context.

    Socket - Supplies the connected socket.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR Host;
    DEBUG_REMOTE_CLIENT_INFORMATION Information;
    INT Result;
    PDEBUG_REMOTE_SERVER_INFORMATION ServerInformation;
    PSTR User;

    memset(&Information, 0, sizeof(DEBUG_REMOTE_CLIENT_INFORMATION));
    Information.Header.Command = DebugRemoteClientInformation;
    Information.Header.Length = sizeof(DEBUG_REMOTE_CLIENT_INFORMATION) -
                                FIELD_OFFSET(DEBUG_REMOTE_CLIENT_INFORMATION,
                                             ProtocolVersion);

    Information.ProtocolVersion = DEBUG_REMOTE_PROTOCOL_VERSION;
    User = DbgrOsGetUserName();
    if (User != NULL) {
        strncpy(Information.User, User, sizeof(Information.User));
        Information.User[DEBUG_REMOTE_USER_SIZE - 1] = '\0';
    }

    Host = DbgrOsGetHostName();
    if (Host != NULL) {
        strncpy(Information.Host, Host, sizeof(Information.Host));
        Information.Host[DEBUG_REMOTE_HOST_SIZE - 1] = '\0';
        free(Host);
    }

    Result = DbgrpRemoteSendCommand(Socket, &(Information.Header));
    if (Result != 0) {
        DbgOut("Failed to send client information.\n");
        return Result;
    }

    Result = DbgrpRemoteReceiveCommand(
                                   Socket,
                                   (PDEBUG_REMOTE_HEADER *)&ServerInformation);

    if (Result != 0) {
        return Result;
    }

    if (ServerInformation->Header.Command != DebugRemoteServerInformation) {
        DbgOut("Got something other than server information.\n");
        free(ServerInformation);
        return EINVAL;
    }

    DbgOut("Connected to server version %d.%d\n",
           DEBUG_REMOTE_PROTOCOL_MAJOR(ServerInformation->ProtocolVersion),
           DEBUG_REMOTE_PROTOCOL_MINOR(ServerInformation->ProtocolVersion));

    if (DEBUG_REMOTE_PROTOCOL_MAJOR(ServerInformation->ProtocolVersion) >
        DEBUG_REMOTE_PROTOCOL_MAJOR(DEBUG_REMOTE_PROTOCOL_VERSION)) {

        DbgOut("This debug client must be upgraded from it's current version "
               "(%d.%d) to connect to the server, which runs remote protocol "
               "version %d.%d.\n",
               DEBUG_REMOTE_PROTOCOL_MAJOR(DEBUG_REMOTE_PROTOCOL_VERSION),
               DEBUG_REMOTE_PROTOCOL_MINOR(DEBUG_REMOTE_PROTOCOL_VERSION),
               DEBUG_REMOTE_PROTOCOL_MAJOR(ServerInformation->ProtocolVersion),
               DEBUG_REMOTE_PROTOCOL_MINOR(ServerInformation->ProtocolVersion));

        free(ServerInformation);
        return EINVAL;
    }

    free(ServerInformation);
    return 0;
}

INT
DbgrpRemoteSendCommand (
    int Socket,
    PDEBUG_REMOTE_HEADER Header
    )

/*++

Routine Description:

    This routine sends a command to the remote client or server.

Arguments:

    Socket - Supplies the socket to send the data on.

    Header - Supplies a pointer to the command to send. The data should be
        immediately after the command. The command type, length, and payload
        should already be filled in. The remainder of the header is filled in
        by this routine.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    Header->Magic = DEBUG_REMOTE_HEADER_MAGIC;
    Header->DataCrc32 = RtlComputeCrc32(0, Header + 1, Header->Length);
    Header->HeaderCrc32 = 0;
    Header->HeaderCrc32 = RtlComputeCrc32(0,
                                          Header,
                                          sizeof(DEBUG_REMOTE_HEADER));

    Result = DbgrpRemoteSendData(Socket,
                                 Header,
                                 Header->Length + sizeof(DEBUG_REMOTE_HEADER));

    return Result;
}

INT
DbgrpRemoteReceiveCommand (
    int Socket,
    PDEBUG_REMOTE_HEADER *Header
    )

/*++

Routine Description:

    This routine receives a command from the remote client or server.

Arguments:

    Socket - Supplies the socket to receive the data from.

    Header - Supplies a pointer where a pointer to the command will be returned
        on success. The caller is responsible for freeing this memory.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PDEBUG_REMOTE_HEADER Buffer;
    ULONG FoundCrc;
    DEBUG_REMOTE_HEADER LocalHeader;
    INT Result;

    *Header = NULL;
    Result = DbgrpRemoteReceiveData(Socket,
                                    &LocalHeader,
                                    sizeof(DEBUG_REMOTE_HEADER));

    if (Result != 0) {
        return Result;
    }

    if (LocalHeader.Magic != DEBUG_REMOTE_HEADER_MAGIC) {
        DbgOut("Received remote packet with bad magic.\n");
        return EINVAL;
    }

    FoundCrc = LocalHeader.HeaderCrc32;
    LocalHeader.HeaderCrc32 = 0;
    if (RtlComputeCrc32(0, &LocalHeader, sizeof(DEBUG_REMOTE_HEADER)) !=
        FoundCrc) {

        DbgOut("Received remote packet with bad CRC.\n");
        return EINVAL;
    }

    LocalHeader.HeaderCrc32 = 0;
    Buffer = malloc(sizeof(DEBUG_REMOTE_HEADER) + LocalHeader.Length);
    if (Buffer == NULL) {
        DbgOut("Failed to allocate 0x%I64x bytes for remote packet.\n",
               sizeof(DEBUG_REMOTE_HEADER) + LocalHeader.Length);

        return ENOMEM;
    }

    memcpy(Buffer, &LocalHeader, sizeof(DEBUG_REMOTE_HEADER));
    Result = DbgrpRemoteReceiveData(Socket, Buffer + 1, LocalHeader.Length);
    if (Result != 0) {
        DbgOut("Failed to receive 0x%I64x bytes: %s.\n",
               LocalHeader.Length,
               strerror(errno));

        free(Buffer);
        return errno;
    }

    if (RtlComputeCrc32(0, Buffer + 1, LocalHeader.Length) !=
        LocalHeader.DataCrc32) {

        free(Buffer);
        return EINVAL;
    }

    *Header = Buffer;
    return Result;
}

INT
DbgrpRemoteSendData (
    int Socket,
    PVOID Data,
    ULONGLONG DataSize
    )

/*++

Routine Description:

    This routine sends data across a socket.

Arguments:

    Socket - Supplies the socket to send to.

    Data - Supplies a pointer to the buffer containing the data to send.

    DataSize - Supplies the size of the data in bytes.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    ssize_t BytesSent;
    size_t BytesThisRound;

    while (DataSize != 0) {
        BytesThisRound = DataSize;
        if (BytesThisRound != DataSize) {
            BytesThisRound = 0x100000;
        }

        BytesSent = DbgrSocketSend(Socket, Data, BytesThisRound);
        if (BytesSent < 0) {
            return -1;
        }

        Data += BytesSent;
        DataSize -= BytesSent;
    }

    return 0;
}

INT
DbgrpRemoteReceiveData (
    int Socket,
    PVOID Data,
    ULONGLONG DataSize
    )

/*++

Routine Description:

    This routine sends data across a socket.

Arguments:

    Socket - Supplies the socket to send to.

    Data - Supplies a pointer to the buffer where the data will be returned.

    DataSize - Supplies the size of the data in bytes.

Return Value:

    0 on success.

    -1 on failure.

--*/

{

    ssize_t BytesSent;
    size_t BytesThisRound;

    while (DataSize != 0) {
        BytesThisRound = DataSize;
        if (BytesThisRound != DataSize) {
            BytesThisRound = 0x100000;
        }

        BytesSent = DbgrSocketReceive(Socket, Data, BytesThisRound);
        if (BytesSent < 0) {
            return -1;
        }

        Data += BytesSent;
        DataSize -= BytesSent;
    }

    return 0;
}

INT
DbgrpServerCreateClient (
    PDEBUGGER_CONTEXT Context,
    INT ClientSocket,
    PSTR ClientHost,
    INT ClientPort
    )

/*++

Routine Description:

    This routine creates, initializes, and inserts a client connection.

Arguments:

    Context - Supplies a pointer to the application context.

    ClientSocket - Supplies the socket containing the new connection with the
        client.

    ClientHost - Supplies a pointer to an allocated string containing the
        name of the remote client. This will be freed when the client
        connection is destroyed.

    ClientPort - Supplies the port of the client connection.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PDEBUGGER_SERVER_CLIENT ClientConnection;
    INT Result;

    ClientConnection = malloc(sizeof(DEBUGGER_SERVER_CLIENT));
    if (ClientConnection == NULL) {
        return ENOMEM;
    }

    memset(ClientConnection, 0, sizeof(DEBUGGER_SERVER_CLIENT));
    ClientConnection->Pipe[0] = -1;
    ClientConnection->Pipe[1] = -1;
    ClientConnection->Socket = ClientSocket;
    ClientConnection->Host = ClientHost;
    ClientConnection->Port = ClientPort;
    ClientConnection->Context = Context;
    Result = DbgrOsCreatePipe(ClientConnection->Pipe);
    if (Result != 0) {
        goto ServerCreateClientEnd;
    }

    //
    // Add this client connection officially to the list.
    //

    DbgrpServerAcquireLock(Context);
    if (Context->Server.ShutDown != 0) {
        Result = -1;
        DbgrpServerReleaseLock(Context);
        goto ServerCreateClientEnd;
    }

    INSERT_BEFORE(&(ClientConnection->ListEntry),
                  &(Context->Server.ClientList));

    Result = DbgrOsCreateThread(DbgrpServerConnectionThread,
                                ClientConnection);

    if (Result != 0) {

        //
        // The client can be destroyed officially, but don't free the host,
        // as the caller will do that on failure of this function.
        //

        ClientConnection->Host = NULL;
        ClientConnection->Socket = -1;
        DbgrpServerDestroyClient(ClientConnection);
        ClientConnection = NULL;
    }

    DbgrpServerReleaseLock(Context);

ServerCreateClientEnd:
    if (Result != 0) {
        if (ClientConnection != NULL) {
            if (ClientConnection->Pipe[0] != -1) {
                close(ClientConnection->Pipe[0]);
            }

            if (ClientConnection->Pipe[1] != -1) {
                close(ClientConnection->Pipe[1]);
            }

            free(ClientConnection);
        }
    }

    return Result;
}

VOID
DbgrpServerDestroyClient (
    PDEBUGGER_SERVER_CLIENT Client
    )

/*++

Routine Description:

    This routine destroys a debug server client connection.

Arguments:

    Client - Supplies a pointer to the client to destroy.

Return Value:

    None.

--*/

{

    if (Client->ListEntry.Next != NULL) {
        LIST_REMOVE(&(Client->ListEntry));
    }

    if (Client->Socket != -1) {
        DbgrSocketClose(Client->Socket);
        Client->Socket = -1;
    }

    if (Client->Pipe[0] != -1) {
        close(Client->Pipe[0]);
        Client->Pipe[0] = -1;
    }

    if (Client->Pipe[1] != -1) {
        close(Client->Pipe[1]);
        Client->Pipe[1] = -1;
    }

    if (Client->Host != NULL) {
        free(Client->Host);
    }

    if (Client->HostName != NULL) {
        free(Client->HostName);
    }

    if (Client->UserName != NULL) {
        free(Client->UserName);
    }

    if (Client->Prompt != NULL) {
        free(Client->Prompt);
    }

    free(Client);
    return;
}

VOID
DbgrpServerAcquireLock (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine acquires the global debug server lock.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

{

    AcquireDebuggerLock(Context->StandardOut.Lock);
    return;
}

VOID
DbgrpServerReleaseLock (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine releases the global debug server lock.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

{

    ReleaseDebuggerLock(Context->StandardOut.Lock);
    return;
}

INT
DbgrpClientConvertRemoteAddressString (
    PSTR RemoteString,
    PSTR *HostString,
    PINT Port
    )

/*++

Routine Description:

    This routine converts a remote string in the form address:port into an
    address string and a port number.

Arguments:

    RemoteString - Supplies a pointer to the remote string to convert.

    HostString - Supplies a pointer where a pointer to the host portion will be
        returned. This newly allocated string must be freed by the caller.

    Port - Supplies a pointer where the port number will be returned, or 0 if
        no port was specified.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    PSTR Host;
    PSTR Ip6Copy;
    PSTR LastColon;
    INT PortNumber;

    *HostString = NULL;
    *Port = 0;
    PortNumber = 0;
    Host = strdup(RemoteString);
    if (Host == NULL) {
        return ENOMEM;
    }

    LastColon = strrchr(Host, ':');
    if (LastColon == NULL) {
        *HostString = Host;
        return 0;
    }

    //
    // Look for other colons, and skip the port thing if there are some.
    //

    if (strchr(Host, ':') != LastColon) {

        //
        // If it's an IPv6 address, chop off the [] and get the port.
        //

        if ((Host[0] == '[') && (LastColon != Host) &&
            (*(LastColon - 1) == ']')) {

            PortNumber = strtoul(LastColon + 1, &AfterScan, 10);
            if (AfterScan == LastColon + 1) {
                return EINVAL;
            }

            *(LastColon - 1) = '\0';
            Ip6Copy = strdup(Host + 1);
            free(Host);
            if (Ip6Copy == NULL) {
                return ENOMEM;
            }

            Host = Ip6Copy;
        }

    //
    // There's only one colon, it's 255.255.255.255:1234.
    //

    } else {
        *LastColon = '\0';
        PortNumber = strtoul(LastColon + 1, &AfterScan, 10);
        if (AfterScan == LastColon + 1) {
            return EINVAL;
        }
    }

    *HostString = Host;
    *Port = PortNumber;
    return 0;
}

