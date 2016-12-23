/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    unsocket.h

Abstract:

    This header contains definitions for the internal local socket interface.

Author:

    Evan Green 19-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes parameters that are passed through to the socket
    creation function via path walk.

Members:

    Domain - Stores the socket domain.

    Type - Stores the socket connection type.

    Protocol - Stores the socket protocol.

    ExistingSocket - Stores a pointer to an existing socket. If this is
        non-NULL, then this socket will be used instead of creating a new one.

--*/

typedef struct _SOCKET_CREATION_PARAMETERS {
    NET_DOMAIN_TYPE Domain;
    NET_SOCKET_TYPE Type;
    ULONG Protocol;
    PSOCKET ExistingSocket;
} SOCKET_CREATION_PARAMETERS, *PSOCKET_CREATION_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
IopCreateUnixSocketPair (
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    PIO_HANDLE NewSockets[2]
    );

/*++

Routine Description:

    This routine creates a pair of connected local domain sockets.

Arguments:

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value for this socket used on the
        network. This value is network specific.

    OpenFlags - Supplies a bitfield of open flags governing the new handles.
        See OPEN_FLAG_* definitions.

    NewSockets - Supplies an array where the two pointers to connected sockets
        will be returned.

Return Value:

    Status code.

--*/

KSTATUS
IopCreateUnixSocket (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    PSOCKET *NewSocket
    );

/*++

Routine Description:

    This routine creates a new Unix socket object.

Arguments:

    Domain - Supplies the network domain to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value for this socket used on the
        network. This value is network specific.

    NewSocket - Supplies a pointer where a pointer to a newly allocated
        socket structure will be returned. The caller is responsible for
        allocating the socket (and potentially a larger structure for its own
        context). The kernel will fill in the standard socket structure after
        this routine returns.

Return Value:

    Status code.

--*/

VOID
IopDestroyUnixSocket (
    PSOCKET Socket
    );

/*++

Routine Description:

    This routine destroys the Unix socket object.

Arguments:

    Socket - Supplies a pointer to the socket.

Return Value:

    None.

--*/

KSTATUS
IopUnixSocketBindToAddress (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PNETWORK_ADDRESS Address,
    PCSTR Path,
    UINTN PathSize
    );

/*++

Routine Description:

    This routine binds the Unix socket to the given path and starts listening
    for client requests.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Handle - Supplies a pointer to the handle to bind.

    Address - Supplies a pointer to the address to bind the socket to.

    Path - Supplies an optional pointer to a path, required if the network
        address is a local socket.

    PathSize - Supplies the size of the path in bytes including the null
        terminator.

Return Value:

    Status code.

--*/

KSTATUS
IopUnixSocketListen (
    PSOCKET Socket,
    ULONG BacklogCount
    );

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing sockets to attempt to connect to it.

Arguments:

    Socket - Supplies a pointer to the socket to mark as listening.

    BacklogCount - Supplies the number of attempted connections that can be
        queued before additional connections are refused.

Return Value:

    Status code.

--*/

KSTATUS
IopUnixSocketAccept (
    PSOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress,
    PCSTR *RemotePath,
    PUINTN RemotePathSize
    );

/*++

Routine Description:

    This routine accepts an incoming connection on a listening connection-based
    socket.

Arguments:

    Socket - Supplies a pointer to the socket to accept a connection from.

    NewConnectionSocket - Supplies a pointer where a new socket will be
        returned that represents the accepted connection with the remote
        host.

    RemoteAddress - Supplies a pointer where the address of the connected
        remote host will be returned.

    RemotePath - Supplies a pointer where a string containing the remote path
        will be returned on success. The caller does not own this string, it is
        connected with the new socket coming out.

    RemotePathSize - Supplies a pointer where the size of the remote path in
        bytes will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
IopUnixSocketConnect (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PNETWORK_ADDRESS Address,
    PCSTR RemotePath,
    UINTN RemotePathSize
    );

/*++

Routine Description:

    This routine attempts to make an outgoing connection to a server.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode (TRUE) or user mode (FALSE).

    Socket - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the address to connect to.

    RemotePath - Supplies a pointer to the string containing the path to
        connect to.

    RemotePathSize - Supplies the size fo the remote path string in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

KSTATUS
IopUnixSocketSendData (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine sends the given data buffer through the local socket.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Socket - Supplies a pointer to the socket to send the data to.

    Parameters - Supplies a pointer to the I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        send.

Return Value:

    Status code.

--*/

KSTATUS
IopUnixSocketReceiveData (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine is called by the user to receive data from the socket.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Socket - Supplies a pointer to the socket to receive data from.

    Parameters - Supplies a pointer to the socket I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer where the received data
        will be returned.

Return Value:

    Status code.

--*/

KSTATUS
IopUnixSocketGetSetSocketInformation (
    PSOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN Option,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets properties of the given socket.

Arguments:

    Socket - Supplies a pointer to the socket to get or set information for.

    InformationType - Supplies the socket information type category to which
        specified option belongs.

    Option - Supplies the option to get or set, which is specific to the
        information type. The type of this value is generally
        SOCKET_<information_type>_OPTION.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input constains the size of the data
        buffer. On output, this contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the information type is incorrect.

    STATUS_BUFFER_TOO_SMALL if the data buffer is too small to receive the
        requested option.

    STATUS_NOT_SUPPORTED_BY_PROTOCOL if the socket option is not supported by
        the socket.

--*/

KSTATUS
IopUnixSocketShutdown (
    PSOCKET Socket,
    ULONG ShutdownType
    );

/*++

Routine Description:

    This routine shuts down communication with a given socket.

Arguments:

    Socket - Supplies a pointer to the socket.

    ShutdownType - Supplies the shutdown type to perform. See the
        SOCKET_SHUTDOWN_* definitions.

Return Value:

    Status code.

--*/

KSTATUS
IopUnixSocketClose (
    PSOCKET Socket
    );

/*++

Routine Description:

    This routine closes down a local socket.

Arguments:

    Socket - Supplies a pointer to the socket.

Return Value:

    Status code.

--*/

