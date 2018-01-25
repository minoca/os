/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    remsrv.h

Abstract:

    This header contains definitions for remote debug server functionality.

Author:

    Evan Green 27-Aug-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros return the major and minor versions from the debug remote
// protocol version.
//

#define DEBUG_REMOTE_PROTOCOL_MAJOR(_ProtocolVersion) \
    (((_ProtocolVersion) >> 16) & 0x0000FFFF)

#define DEBUG_REMOTE_PROTOCOL_MINOR(_ProtocolVersion) \
    ((_ProtocolVersion) & 0x0000FFFF)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the magic value for the debug remote packet: 'Dbg:'.
//

#define DEBUG_REMOTE_HEADER_MAGIC 0x3A676244

//
// Define the current remote protocol version.
//

#define DEBUG_REMOTE_PROTOCOL_VERSION 0x00010000

//
// Define the size of the user and host strings.
//

#define DEBUG_REMOTE_USER_SIZE 48
#define DEBUG_REMOTE_HOST_SIZE 48

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the command types sent between remote clients and servers.
//

typedef enum _DEBUG_REMOTE_COMMAND_TYPE {
    DebugRemoteCommandInvalid,
    DebugRemoteClientInformation,
    DebugRemoteServerInformation,
    DebugRemoteOutput,
    DebugRemotePrompt,
    DebugRemoteInput,
    DebugRemoteBreakRequest,
    DebugRemoteSourceInformation,
    DebugRemoteSourceDataRequest,
    DebugRemoteSourceData,
} DEBUG_REMOTE_COMMAND_TYPE, *PDEBUG_REMOTE_COMMAND_TYPE;

//
// Define the various states the receive thread can be in.
//

typedef enum _DEBUGGER_SERVER_RECEIVE_STATE {
    DebuggerServerReceiveNotStarted,
    DebuggerServerReceiveRunning,
    DebuggerServerReceiveShutDownRequested,
    DebuggerServerReceiveShutDown
} DEBUGGER_SERVER_RECEIVE_STATE, *PDEBUGGER_SERVER_RECEIVE_STATE;

/*++

Structure Description:

    This structure stores the common header that goes on each remote packet.

Members:

    Magic - Stores the magic value. Set this to DEBUG_REMOTE_HEADER_MAGIC.

    Command - Stores the command. See the DEBUG_REMOTE_COMMAND_TYPE enum.

    HeaderCrc32 - Stores the CRC32 of the header. The data CRC32 is filled in,
        but the header CRC32 is set to zero when computing the checksum.

    Length - Stores the length of the data payload, in bytes.

    DataCrc32 - Stores the CRC32 of the data portion of the payload.

--*/

#pragma pack(push, 1)

typedef struct _DEBUG_REMOTE_HEADER {
    ULONG Magic;
    ULONG Command;
    ULONG HeaderCrc32;
    ULONGLONG Length;
    ULONG DataCrc32;
} PACKED DEBUG_REMOTE_HEADER, *PDEBUG_REMOTE_HEADER;

/*++

Structure Description:

    This structure stores debugger remote client information. The client sends
    this information immediately after connecting.

Members:

    Header - Stores the standard packet header.

    ProtocolVersion - Stores the protocol version number of the client.

    User - Stores the name of the remote client user.

    Host - Stores the name of the remote client host.

--*/

typedef struct _DEBUG_REMOTE_CLIENT_INFORMATION {
    DEBUG_REMOTE_HEADER Header;
    ULONG ProtocolVersion;
    CHAR User[DEBUG_REMOTE_USER_SIZE];
    CHAR Host[DEBUG_REMOTE_HOST_SIZE];
} PACKED DEBUG_REMOTE_CLIENT_INFORMATION, *PDEBUG_REMOTE_CLIENT_INFORMATION;

/*++

Structure Description:

    This structure stores debugger remote server information. It is sent in
    response to remote client information.

Members:

    Header - Stores the standard packet header.

    ProtocolVersion - Stores the protocol version number of the client.

--*/

typedef struct _DEBUG_REMOTE_SERVER_INFORMATION {
    DEBUG_REMOTE_HEADER Header;
    ULONG ProtocolVersion;
} PACKED DEBUG_REMOTE_SERVER_INFORMATION, *PDEBUG_REMOTE_SERVER_INFORMATION;

/*++

Structure Description:

    This structure stores debugger source file and line information. It is sent
    whenever the source file or line changes. The source file string follows
    immediately after this structure, and consumes the remainder of the payload.

Members:

    Header - Stores the standard packet header.

    LineNumber - Stores the line number.

    SourceAvailable - Stores a boolean indicating whether or not the server has
        the source for the given file.

--*/

typedef struct _DEBUG_REMOTE_SOURCE_INFORMATION {
    DEBUG_REMOTE_HEADER Header;
    ULONGLONG LineNumber;
    ULONG SourceAvailable;
} PACKED DEBUG_REMOTE_SOURCE_INFORMATION, *PDEBUG_REMOTE_SOURCE_INFORMATION;

/*++

Structure Description:

    This structure stores debugger source data. It is sent when requested by
    the client. The source data comes immediately after the structure and is
    the length of the rest of the payload.

Members:

    Header - Stores the standard packet header.

    FileNameCrc32 - Stores the CRC32 of the file path. This can be used by the
        client to ensure the correct data is being received.

--*/

typedef struct _DEBUG_REMOTE_SOURCE_DATA {
    DEBUG_REMOTE_HEADER Header;
    ULONG FileNameCrc32;
} PACKED DEBUG_REMOTE_SOURCE_DATA, *PDEBUG_REMOTE_SOURCE_DATA;

#pragma pack(pop)

/*++

Structure Description:

    This structure stores information pertaining to managing a remote debug
    server.

Members:

    ListEntry - Stores pointers to the next and previous client connections
        that the debug server is maintaining.

    Context - Stores a pointer to the applicaton context this connection
        belongs to.

    Socket - Stores the socket representing the connection between the server
        and its client.

    Pipe - Stores the pipe used to communicate to the thread managing the
        client connection.

    Update - Stores a boolean indicating whether or not there is a pending
        update the client thread should send along to the client.

    Host - Stores a pointer to the host string of the client.

    Port - Stores a pointer to the client remote port.

    HostName - Stores the name of the remote machine as reported by the client.

    UserName - Stores the name of the remote user as reporeted by the client.

    Prompt - Stores the last prompt sent to the client.

    SourceFile - Stores the last source file name send to the client.

    SourceLine - Stores the last source line sent to the client.

    ReceiveState - Stores the current state of the receive thread.

--*/

typedef struct _DEBUGGER_SERVER_CLIENT {
    LIST_ENTRY ListEntry;
    PDEBUGGER_CONTEXT Context;
    int Socket;
    int Pipe[2];
    volatile int Update;
    char *Host;
    int Port;
    char *HostName;
    char *UserName;
    char *Prompt;
    char *SourceFile;
    ULONGLONG SourceLine;
    volatile DEBUGGER_SERVER_RECEIVE_STATE ReceiveState;
} DEBUGGER_SERVER_CLIENT, *PDEBUGGER_SERVER_CLIENT;

/*++

Structure Description:

    This structure stores a complete remote command.

Members:

    ListEntry - Stores pointers to the next and previous remote commands in the
        queue.

    Command - Stores a pointer to the null terminated string containing the
        command to run. This must be freed.

    Host - Stores a pointer to a null terminated string containing the host
        that requests the command. This must be freed.

    User - Stores a pointer to a null terminated string containing the user
        that requests the command. This must be freed.

--*/

typedef struct _DEBUGGER_REMOTE_COMMAND {
    LIST_ENTRY ListEntry;
    PSTR Command;
    PSTR Host;
    PSTR User;
} DEBUGGER_REMOTE_COMMAND, *PDEBUGGER_REMOTE_COMMAND;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
DbgrClientMainLoop (
    PDEBUGGER_CONTEXT Context,
    PSTR RemoteString,
    BOOL ReverseRemote
    );

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

INT
DbgrpClientRequestBreakIn (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine sends a break request across to the debug server.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    0 on success.

    Non-zero on error.

--*/

VOID
DbgrpServerNotifyClients (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine notifies all debug clients connected to the given server that
    there is new activity to send off to the clients.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

VOID
DbgrpServerDestroy (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine tears down the debug server and all its connections.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

