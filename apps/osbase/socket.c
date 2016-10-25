/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    socket.c

Abstract:

    This module implements support for socket-based communication in user mode.

Author:

    Evan Green 3-May-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

OS_API
KSTATUS
OsSocketCreatePair (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    HANDLE Sockets[2]
    )

/*++

Routine Description:

    This routine creates a pair of sockets that are connected to each other.

Arguments:

    Domain - Supplies the network domain to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value used on the network.

    OpenFlags - Supplies an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

    Sockets - Supplies an array where the two handles to the connected
        sockets will be returned on success.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_CREATE_PAIR Parameters;
    KSTATUS Status;

    Parameters.Domain = Domain;
    Parameters.Type = Type;
    Parameters.Protocol = Protocol;
    Parameters.OpenFlags = OpenFlags;
    Status = OsSystemCall(SystemCallSocketCreatePair, &Parameters);
    Sockets[0] = Parameters.Socket1;
    Sockets[1] = Parameters.Socket2;
    return Status;
}

OS_API
KSTATUS
OsSocketCreate (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    PHANDLE Socket
    )

/*++

Routine Description:

    This routine creates a new socket for communication.

Arguments:

    Domain - Supplies the network domain to use for the socket.

    Type - Supplies the socket type.

    Protocol - Supplies the raw protocol value to use for the socket. This is
        network specific.

    OpenFlags - Supplies an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

    Socket - Supplies a pointer where the new socket handle will be returned on
        success.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_CREATE Request;
    KSTATUS Status;

    Request.Domain = Domain;
    Request.Type = Type;
    Request.Protocol = Protocol;
    Request.OpenFlags = OpenFlags;
    Status = OsSystemCall(SystemCallSocketCreate, &Request);
    *Socket = Request.Socket;
    return Status;
}

OS_API
KSTATUS
OsSocketBind (
    HANDLE Socket,
    PNETWORK_ADDRESS Address,
    PSTR Path,
    UINTN PathSize
    )

/*++

Routine Description:

    This routine binds a newly created socket to a local address.

Arguments:

    Socket - Supplies a handle to the fresh socket.

    Address - Supplies a pointer to the local network address to bind to.

    Path - Supplies a pointer to the path to bind to in the case that this is
        a local (Unix) socket.

    PathSize - Supplies the size of the path in bytes including the null
        terminator.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_BIND Request;

    Request.Socket = Socket;
    RtlCopyMemory(&(Request.Address), Address, sizeof(NETWORK_ADDRESS));
    Request.Path = Path;
    Request.PathSize = PathSize;
    return OsSystemCall(SystemCallSocketBind, &Request);
}

OS_API
KSTATUS
OsSocketListen (
    HANDLE Socket,
    ULONG SuggestedBacklog
    )

/*++

Routine Description:

    This routine activates a socket, making it eligible to accept new
    incoming connections.

Arguments:

    Socket - Supplies the socket to activate.

    SuggestedBacklog - Supplies a suggestion to the kernel as to the number of
        un-accepted incoming connections to queue up before incoming
        connections are refused.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_LISTEN Request;

    Request.Socket = Socket;
    Request.BacklogCount = SuggestedBacklog;
    return OsSystemCall(SystemCallSocketListen, &Request);
}

OS_API
KSTATUS
OsSocketAccept (
    HANDLE Socket,
    PHANDLE NewSocket,
    PNETWORK_ADDRESS Address,
    PSTR RemotePath,
    PUINTN RemotePathSize,
    ULONG OpenFlags
    )

/*++

Routine Description:

    This routine accepts an incoming connection on a listening socket and spins
    it off into a new socket. This routine will block until an incoming
    connection request is received.

Arguments:

    Socket - Supplies the listening socket to accept a new connection from.

    NewSocket - Supplies a pointer where a new socket representing the
        incoming connection will be returned on success.

    Address - Supplies an optional pointer where the address of the remote host
        will be returned.

    RemotePath - Supplies a pointer where the remote path of the client socket
        will be copied on success. This only applies to local sockets.

    RemotePathSize - Supplies a pointer that on input contains the size of the
        remote path buffer. On output, contains the true size of the remote
        path, even if it was bigger than the input.

    OpenFlags - Supplies an optional bitfield of open flags for the new socket.
        Only SYS_OPEN_FLAG_NON_BLOCKING and SYS_OPEN_FLAG_CLOSE_ON_EXECUTE
        are accepted.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_ACCEPT Request;
    KSTATUS Status;

    Request.Socket = Socket;
    Request.NewSocket = INVALID_HANDLE;
    Request.RemotePath = RemotePath;
    Request.RemotePathSize = 0;
    if (RemotePathSize != NULL) {
        Request.RemotePathSize = *RemotePathSize;
    }

    Request.OpenFlags = OpenFlags;
    Status = OsSystemCall(SystemCallSocketAccept, &Request);
    *NewSocket = Request.NewSocket;
    if (Address != NULL) {
        RtlCopyMemory(Address, &(Request.Address), sizeof(NETWORK_ADDRESS));
    }

    if (RemotePathSize != NULL) {
        *RemotePathSize = Request.RemotePathSize;
    }

    return Status;
}

OS_API
KSTATUS
OsSocketConnect (
    HANDLE Socket,
    PNETWORK_ADDRESS Address,
    PSTR RemotePath,
    UINTN RemotePathSize
    )

/*++

Routine Description:

    This routine attempts to establish a new outgoing connection on a socket.

Arguments:

    Socket - Supplies a handle to the fresh socket to use to establish the
        connection.

    Address - Supplies a pointer to the destination socket address.

    RemotePath - Supplies a pointer to the path to connect to, if this is a
        local socket.

    RemotePathSize - Supplies the size of the remote path buffer in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_CONNECT Request;

    Request.Socket = Socket;
    RtlCopyMemory(&(Request.Address), Address, sizeof(NETWORK_ADDRESS));
    Request.RemotePath = RemotePath;
    Request.RemotePathSize = RemotePathSize;
    return OsSystemCall(SystemCallSocketConnect, &Request);
}

OS_API
KSTATUS
OsSocketPerformIo (
    HANDLE Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine performs I/O on an open handle.

Arguments:

    Socket - Supplies a pointer to the socket.

    Parameters - Supplies a pointer to the socket I/O request details.

    Buffer - Supplies a pointer to the buffer containing the data to write or
        where the read data should be returned, depending on the operation.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_PERFORM_IO Request;

    Request.Socket = Socket;
    Request.Parameters = Parameters;
    Request.Buffer = Buffer;
    return OsSystemCall(SystemCallSocketPerformIo, &Request);
}

OS_API
KSTATUS
OsSocketPerformVectoredIo (
    HANDLE Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_VECTOR VectorArray,
    UINTN VectorCount
    )

/*++

Routine Description:

    This routine performs vectored I/O on an open handle.

Arguments:

    Socket - Supplies a pointer to the socket.

    Parameters - Supplies a pointer to the socket I/O request details.

    VectorArray - Supplies an array of I/O vector structures to do I/O to/from.

    VectorCount - Supplies the number of elements in the vector array.

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_PERFORM_VECTORED_IO Request;

    Request.Socket = Socket;
    Request.Parameters = Parameters;
    Request.VectorArray = VectorArray;
    Request.VectorCount = VectorCount;
    return OsSystemCall(SystemCallSocketPerformVectoredIo, &Request);
}

OS_API
KSTATUS
OsSocketGetSetInformation (
    HANDLE Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN Option,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets socket information.

Arguments:

    Socket - Supplies the socket handle.

    InformationType - Supplies the socket information type category to which
        specified option belongs.

    Option - Supplies the option to get or set, which is specific to the
        information type. The type of this value is generally
        SOCKET_<information_type>_OPTION.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation. If the
        buffer is too small for a get request, the truncated data will be
        returned and the routine will fail with STATUS_BUFFER_TOO_SMALL.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, this contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    SYSTEM_CALL_SOCKET_GET_SET_INFORMATION Request;
    KSTATUS Status;

    Request.Socket = Socket;
    Request.InformationType = InformationType;
    Request.Option = Option;
    Request.Data = Data;
    Request.DataSize = *DataSize;
    Request.Set = Set;
    Status = OsSystemCall(SystemCallSocketGetSetInformation, &Request);
    *DataSize = Request.DataSize;
    return Status;
}

OS_API
KSTATUS
OsSocketShutdown (
    HANDLE Socket,
    ULONG ShutdownType
    )

/*++

Routine Description:

    This routine shuts down communication with a given socket.

Arguments:

    Socket - Supplies the socket handle.

    ShutdownType - Supplies the shutdown type to perform. See the
        SOCKET_SHUTDOWN_* definitions. These are NOT the same as the C library
        definitions.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_A_SOCKET if the given handle wasn't a socket.

    Other error codes on failure.

--*/

{

    SYSTEM_CALL_SOCKET_SHUTDOWN Parameters;

    Parameters.Socket = Socket;
    Parameters.ShutdownType = ShutdownType;
    return OsSystemCall(SystemCallSocketShutdown, &Parameters);
}

//
// --------------------------------------------------------- Internal Functions
//

