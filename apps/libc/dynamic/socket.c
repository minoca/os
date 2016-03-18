/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#include "libcp.h"
#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netlink/netlink.h>
#include "net.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro asserts that the MSG_* flags are equal to the SOCKET_IO_* flags
// and require no translation. It is basically a static assert that gets
// compiled away.
//

#define ASSERT_SOCKET_IO_FLAGS_ARE_EQUIVALENT()  \
    ASSERT((MSG_PEEK == SOCKET_IO_PEEK) &&       \
           (MSG_OOB == SOCKET_IO_OUT_OF_BAND) && \
           (MSG_WAITALL == SOCKET_IO_WAIT_ALL))

#define ASSERT_SOCKET_TYPES_EQUIVALENT()                   \
    ASSERT((SOCK_DGRAM == NetSocketDatagram) &&            \
           (SOCK_RAW == NetSocketRaw) &&                   \
           (SOCK_SEQPACKET == NetSocketSequencedPacket) && \
           (SOCK_STREAM == NetSocketStream));

#define ASSERT_DOMAIN_TYPES_EQUIVALENT()   \
    ASSERT((AF_UNIX == NetDomainLocal) &&  \
           (AF_LOCAL == NetDomainLocal) && \
           (AF_INET == NetDomainIp4) &&    \
           (AF_INET6 == NetDomainIp6));

//
// ---------------------------------------------------------------- Definitions
//

#define INVALID_SOCKET_OPTION 0

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ClpGetSocketAddress (
    int Socket,
    SOCKET_BASIC_OPTION Option,
    struct sockaddr *SocketAddress,
    socklen_t *AddressLength
    );

KSTATUS
ClpConvertSocketLevelAndOptionName (
    INT Level,
    INT OptionName,
    PSOCKET_INFORMATION_TYPE InformationType,
    PUINTN SocketOption,
    PUINTN SecondOption
    );

VOID
ClpGetPathFromSocketAddress (
    struct sockaddr *Address,
    socklen_t *AddressLength,
    PSTR *Path,
    PUINTN PathSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define an array to help convert POSIX basic option names.
//

SOCKET_BASIC_OPTION ClpSocketOptionNameToBasicOption[] = {
    SocketBasicOptionInvalid,
    SocketBasicOptionDebug,
    SocketBasicOptionAcceptConnections,
    SocketBasicOptionBroadcastEnabled,
    SocketBasicOptionInvalid,
    SocketBasicOptionKeepAlive,
    SocketBasicOptionLinger,
    SocketBasicOptionInlineOutOfBand,
    SocketBasicOptionSendBufferSize,
    SocketBasicOptionReceiveBufferSize,
    SocketBasicOptionRoutingDisabled,
    SocketBasicOptionReceiveMinimum,
    SocketBasicOptionReceiveTimeout,
    SocketBasicOptionSendMinimum,
    SocketBasicOptionSendTimeout,
    SocketBasicOptionErrorStatus,
    SocketBasicOptionType,
    SocketBasicOptionReuseExactAddress,
    SocketBasicOptionPassCredentials,
    SocketBasicOptionPeerCredentials,
};

//
// Define an array to help convert POSIX TCP option names.
//

SOCKET_BASIC_OPTION ClpSocketOptionNameToTcpOption[] = {
    SocketTcpOptionInvalid,
    SocketTcpOptionNoDelay,
    SocketTcpOptionKeepAliveTimeout,
    SocketTcpOptionKeepAlivePeriod,
    SocketTcpOptionKeepAliveProbeLimit
};

//
// Define an array to help convert POSIX IPv4 option names.
//

SOCKET_BASIC_OPTION ClpSocketOptionNameToIp4Option[] = {
    SocketIp4OptionInvalid,
    SocketIp4OptionHeaderIncluded
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
socketpair (
    int Domain,
    int Type,
    int Protocol,
    int Sockets[2]
    )

/*++

Routine Description:

    This routine creates an unbound pair of connected sockets. The two sockets
    are identical.

Arguments:

    Domain - Supplies the communicaion domain in which a sockets are to be
        created. Currently only AF_UNIX is supported for socket pairs.

    Type - Supplies the type of socket to be created. See the SOCK_*
        definitions. Common values include SOCK_STREAM and SOCK_DGRAM.

    Protocol - Supplies the particular protocol to use for the given domain
        and type. Supply 0 to use a default protocol appropriate for the
        specified type.

    Sockets - Supplies an array where the two connected sockets will be
        returned on success.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    HANDLE Handles[2];
    ULONG OpenFlags;
    KSTATUS Status;

    ASSERT_SOCKET_TYPES_EQUIVALENT();
    ASSERT_DOMAIN_TYPES_EQUIVALENT();

    OpenFlags = 0;
    if ((Type & SOCK_CLOEXEC) != 0) {
        OpenFlags |= SYS_OPEN_FLAG_CLOSE_ON_EXECUTE;
    }

    if ((Type & SOCK_NONBLOCK) != 0) {
        OpenFlags |= SYS_OPEN_FLAG_NON_BLOCKING;
    }

    Type &= ~(SOCK_CLOEXEC | SOCK_NONBLOCK);
    Status = OsSocketCreatePair(Domain, Type, Protocol, OpenFlags, Handles);
    Sockets[0] = (int)(UINTN)(Handles[0]);
    Sockets[1] = (int)(UINTN)(Handles[1]);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
socket (
    int Domain,
    int Type,
    int Protocol
    )

/*++

Routine Description:

    This routine creates a new socket for communication.

Arguments:

    Domain - Supplies the communicaion domain in which a socket is to be
        created. See the AF_* or PF_* definitions. The most common values are
        AF_INET, AF_INET6, and AF_UNIX.

    Type - Supplies the type of socket to be created. See the SOCK_*
        definitions. Common values include SOCK_STREAM and SOCK_DGRAM.

    Protocol - Supplies the particular protocol to use for the given domain
        and type. Supply 0 to use a default protocol appropriate for the
        specified type.

Return Value:

    Returns a non-negative integer representing the descriptor for the new
    socket.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    ULONG OpenFlags;
    HANDLE Socket;
    KSTATUS Status;

    //
    // The network domains and socket types line up.
    //

    ASSERT_DOMAIN_TYPES_EQUIVALENT();
    ASSERT_SOCKET_TYPES_EQUIVALENT();

    OpenFlags = 0;
    if ((Type & SOCK_CLOEXEC) != 0) {
        OpenFlags |= SYS_OPEN_FLAG_CLOSE_ON_EXECUTE;
    }

    if ((Type & SOCK_NONBLOCK) != 0) {
        OpenFlags |= SYS_OPEN_FLAG_NON_BLOCKING;
    }

    Type &= ~(SOCK_CLOEXEC | SOCK_NONBLOCK);

    //
    // Create the socket.
    //

    Status = OsSocketCreate(Domain, Type, Protocol, OpenFlags, &Socket);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return (int)(UINTN)Socket;
}

LIBC_API
int
bind (
    int Socket,
    const struct sockaddr *Address,
    socklen_t AddressLength
    )

/*++

Routine Description:

    This routine assigns a local socket address to a socket that currently has
    no local address assigned.

Arguments:

    Socket - Supplies the file descriptor of the socket to be bound.

    Address - Supplies a pointer to the address to bind the socket to. The
        length and format depend on the address family of the socket.

    AddressLength - Supplies the length of the address structure in bytes.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NETWORK_ADDRESS NetworkAddress;
    PSTR Path;
    UINTN PathSize;
    KSTATUS Status;

    //
    // Convert the address structure into a network address that the kernel
    // understands.
    //

    Path = NULL;
    PathSize = 0;
    Status = ClpConvertToNetworkAddress(Address,
                                        AddressLength,
                                        &NetworkAddress,
                                        &Path,
                                        &PathSize);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    Status = OsSocketBind((HANDLE)(UINTN)Socket,
                          &NetworkAddress,
                          Path,
                          PathSize);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = EOPNOTSUPP;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    return 0;
}

LIBC_API
int
listen (
    int Socket,
    int Backlog
    )

/*++

Routine Description:

    This routine marks a connection-mode socket as ready to accept new
    incoming connections.

Arguments:

    Socket - Supplies the file descriptor of the socket to be marked as
        listening.

    Backlog - Supplies a suggestion to the system as to the number of
        un-accepted connections to queue up before refusing additional
        incoming connection requests.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    KSTATUS Status;

    Status = OsSocketListen((HANDLE)(UINTN)Socket, Backlog);
    if (!KSUCCESS(Status)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = EOPNOTSUPP;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    return 0;
}

LIBC_API
int
accept (
    int Socket,
    struct sockaddr *Address,
    socklen_t *AddressLength
    )

/*++

Routine Description:

    This routine extracts the first pending incoming connection from the
    given listening socket, creates a new socket representing that connection,
    and allocates a file descriptor for that new socket. These newly created
    file descriptors are then ready for reading and writing.

Arguments:

    Socket - Supplies the file descriptor of the listening socket to accept
        a connection on.

    Address - Supplies an optional pointer where the address of the connecting
        socket will be returned.

    AddressLength - Supplies a pointer that on input contains the length of the
        specified Address structure, and on output returns the length of the
        returned address.

Return Value:

    Returns a non-negative file descriptor representing the new connection on
    success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    return accept4(Socket, Address, AddressLength, 0);
}

LIBC_API
int
accept4 (
    int Socket,
    struct sockaddr *Address,
    socklen_t *AddressLength,
    int Flags
    )

/*++

Routine Description:

    This routine extracts the first pending incoming connection from the
    given listening socket, creates a new socket representing that connection,
    and allocates a file descriptor for that new socket. These newly created
    file descriptors are then ready for reading and writing.

Arguments:

    Socket - Supplies the file descriptor of the listening socket to accept
        a connection on.

    Address - Supplies an optional pointer where the address of the connecting
        socket will be returned.

    AddressLength - Supplies a pointer that on input contains the length of the
        specified Address structure, and on output returns the length of the
        returned address.

    Flags - Supplies an optional bitfield of flags governing the newly created
        file descriptor. Set SOCK_CLOEXEC to set the O_CLOEXEC flag on the new
        descriptor, and SOCK_NONBLOCK to set the O_NONBLOCK flag on the new
        descriptor.

Return Value:

    Returns a non-negative file descriptor representing the new connection on
    success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NETWORK_ADDRESS NetworkAddress;
    HANDLE NewSocket;
    ULONG OpenFlags;
    PSTR RemotePath;
    UINTN RemotePathSize;
    KSTATUS Status;

    assert((SOCK_CLOEXEC == O_CLOEXEC) && (SOCK_NONBLOCK == O_NONBLOCK));

    OpenFlags = 0;
    if (Flags != 0) {
        if ((Flags & SOCK_CLOEXEC) != 0) {
            OpenFlags |= SYS_OPEN_FLAG_CLOSE_ON_EXECUTE;
        }

        if ((Flags & SOCK_NONBLOCK) != 0) {
            OpenFlags |= SYS_OPEN_FLAG_NON_BLOCKING;
        }
    }

    ClpGetPathFromSocketAddress(Address,
                                AddressLength,
                                &RemotePath,
                                &RemotePathSize);

    Status = OsSocketAccept((HANDLE)(UINTN)Socket,
                            &NewSocket,
                            &NetworkAddress,
                            RemotePath,
                            &RemotePathSize,
                            OpenFlags);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = EOPNOTSUPP;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    //
    // Convert the network address returned by the kernel into the C library
    // sockaddr structure.
    //

    if ((Address != NULL) && (AddressLength != NULL)) {
        Status = ClpConvertFromNetworkAddress(&NetworkAddress,
                                              Address,
                                              AddressLength,
                                              RemotePathSize);

        if (!KSUCCESS(Status)) {
            errno = EINVAL;
            return -1;
        }
    }

    return (int)(UINTN)NewSocket;
}

LIBC_API
int
connect (
    int Socket,
    const struct sockaddr *Address,
    socklen_t AddressLength
    )

/*++

Routine Description:

    This routine attempts to reach out and establish a connection with another
    socket.

Arguments:

    Socket - Supplies the file descriptor of the socket to use for the
        connection.

    Address - Supplies a pointer to the address to connect to. The length and
        format depend on the address family of the socket.

    AddressLength - Supplies the length of the address structure in bytes.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NETWORK_ADDRESS NetworkAddress;
    PSTR RemotePath;
    UINTN RemotePathSize;
    KSTATUS Status;

    //
    // Convert the address structure into a network address that the kernel
    // understands.
    //

    RemotePath = NULL;
    RemotePathSize = 0;
    Status = ClpConvertToNetworkAddress(Address,
                                        AddressLength,
                                        &NetworkAddress,
                                        &RemotePath,
                                        &RemotePathSize);

    if (!KSUCCESS(Status)) {
        errno = EINVAL;
        return -1;
    }

    Status = OsSocketConnect((HANDLE)(UINTN)Socket,
                             &NetworkAddress,
                             RemotePath,
                             RemotePathSize);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = EOPNOTSUPP;

        } else if (Status == STATUS_RESOURCE_IN_USE) {
            errno = EADDRNOTAVAIL;

        } else if (Status == STATUS_INVALID_ADDRESS) {
            errno = EPROTOTYPE;

        } else if (Status == STATUS_TIMEOUT) {
            errno = ETIMEDOUT;

        } else if (Status == STATUS_UNEXPECTED_TYPE) {
            errno = EAFNOSUPPORT;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    return 0;
}

LIBC_API
ssize_t
send (
    int Socket,
    const void *Data,
    size_t Length,
    int Flags
    )

/*++

Routine Description:

    This routine sends data out of a connected socket.

Arguments:

    Socket - Supplies the file descriptor of the socket to send data out of.

    Data - Supplies the buffer of data to send.

    Length - Supplies the length of the data buffer, in bytes.

    Flags - Supplies a bitfield of flags governing the transmission of the data.
        See MSG_* definitions.

Return Value:

    Returns the number of bytes sent on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    return sendto(Socket, Data, Length, Flags, NULL, 0);
}

LIBC_API
ssize_t
sendto (
    int Socket,
    const void *Data,
    size_t Length,
    int Flags,
    const struct sockaddr *DestinationAddress,
    socklen_t DestinationAddressLength
    )

/*++

Routine Description:

    This routine sends data out of a socket, potentially to a specific
    destination address for connection-less sockets.

Arguments:

    Socket - Supplies the file descriptor of the socket to send data out of.

    Data - Supplies the buffer of data to send.

    Length - Supplies the length of the data buffer, in bytes.

    Flags - Supplies a bitfield of flags governing the transmission of the data.
        See MSG_* definitions.

    DestinationAddress - Supplies an optional pointer to the destination
        address to send the data to.

    DestinationAddressLength - Supplies the length of the destination address
        structure.

Return Value:

    Returns the number of bytes sent on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NETWORK_ADDRESS NetworkAddress;
    SOCKET_IO_PARAMETERS Parameters;
    KSTATUS Status;

    Parameters.Size = Length;
    Parameters.IoFlags = SYS_IO_FLAG_WRITE;
    Parameters.SocketIoFlags = Flags;
    Parameters.TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = NULL;
    Parameters.RemotePath = NULL;
    Parameters.RemotePathSize = 0;
    Parameters.ControlData = NULL;
    Parameters.ControlDataSize = 0;

    ASSERT_SOCKET_IO_FLAGS_ARE_EQUIVALENT();

    //
    // A specific desination address was supplied, so it needs to be converted.
    //

    if (DestinationAddress != NULL) {
        Status = ClpConvertToNetworkAddress(DestinationAddress,
                                            DestinationAddressLength,
                                            &NetworkAddress,
                                            &(Parameters.RemotePath),
                                            &(Parameters.RemotePathSize));

        if (!KSUCCESS(Status)) {
            errno = EINVAL;
            return -1;
        }

        Parameters.NetworkAddress = &NetworkAddress;
    }

    Status = OsSocketPerformIo((HANDLE)(UINTN)Socket, &Parameters, (PVOID)Data);
    if (!KSUCCESS(Status)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = EOPNOTSUPP;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    return (ssize_t)(Parameters.Size);
}

LIBC_API
ssize_t
sendmsg (
    int Socket,
    const struct msghdr *Message,
    int Flags
    )

/*++

Routine Description:

    This routine sends a message out of a socket, potentially to a specific
    destination address for connection-less sockets. This version of the send
    function allows for vectored I/O and sending of ancillary data.

Arguments:

    Socket - Supplies the file descriptor of the socket to send data out of.

    Message - Supplies a pointer to the message details to send.

    Flags - Supplies a bitfield of flags governing the transmission of the data.
        See MSG_* definitions.

Return Value:

    Returns the number of bytes sent on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NETWORK_ADDRESS Address;
    SOCKET_IO_PARAMETERS Parameters;
    KSTATUS Status;
    UINTN VectorIndex;

    if (Message == NULL) {
        errno = EINVAL;
        return -1;
    }

    Parameters.Size = 0;
    for (VectorIndex = 0; VectorIndex < Message->msg_iovlen; VectorIndex += 1) {
        Parameters.Size += Message->msg_iov[VectorIndex].iov_len;
    }

    ASSERT_SOCKET_IO_FLAGS_ARE_EQUIVALENT();

    Parameters.IoFlags = SYS_IO_FLAG_WRITE;
    Parameters.SocketIoFlags = Flags;
    Parameters.TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = NULL;
    Parameters.RemotePath = NULL;
    Parameters.RemotePathSize = 0;
    if ((Message->msg_name != NULL) && (Message->msg_namelen != 0)) {
        Status = ClpConvertToNetworkAddress(Message->msg_name,
                                            Message->msg_namelen,
                                            &Address,
                                            &(Parameters.RemotePath),
                                            &(Parameters.RemotePathSize));

        if (!KSUCCESS(Status)) {
            errno = EINVAL;
            return -1;
        }

        Parameters.NetworkAddress = &Address;
    }

    Parameters.ControlData = Message->msg_control;
    Parameters.ControlDataSize = Message->msg_controllen;
    Status = OsSocketPerformVectoredIo((HANDLE)(UINTN)Socket,
                                       &Parameters,
                                       (PIO_VECTOR)(Message->msg_iov),
                                       Message->msg_iovlen);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = EOPNOTSUPP;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    return (ssize_t)(Parameters.Size);
}

LIBC_API
ssize_t
recv (
    int Socket,
    void *Buffer,
    size_t Length,
    int Flags
    )

/*++

Routine Description:

    This routine receives data from a connected socket.

Arguments:

    Socket - Supplies the file descriptor of the socket to receive data from.

    Buffer - Supplies a pointer to a buffer where the received data will be
        returned.

    Length - Supplies the length of the data buffer, in bytes.

    Flags - Supplies a bitfield of flags governing the reception of the data.
        See MSG_* definitions.

Return Value:

    Returns the number of bytes received on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    return recvfrom(Socket, Buffer, Length, Flags, NULL, NULL);
}

LIBC_API
ssize_t
recvfrom (
    int Socket,
    void *Buffer,
    size_t Length,
    int Flags,
    struct sockaddr *SourceAddress,
    socklen_t *SourceAddressLength
    )

/*++

Routine Description:

    This routine receives data from a socket, potentially receiving the
    source address for connection-less sockets.

Arguments:

    Socket - Supplies the file descriptor of the socket to receive data from.

    Buffer - Supplies a pointer to a buffer where the received data will be
        returned.

    Length - Supplies the length of the data buffer, in bytes.

    Flags - Supplies a bitfield of flags governing the reception of the data.
        See MSG_* definitions.

    SourceAddress - Supplies an optional pointer where the source of the packet
        will be returned for connection-less sockets.

    SourceAddressLength - Supplies the length of the source address structure.

Return Value:

    Returns the number of bytes received on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NETWORK_ADDRESS NetworkAddress;
    SOCKET_IO_PARAMETERS Parameters;
    KSTATUS Status;

    Parameters.Size = Length;
    Parameters.IoFlags = 0;
    Parameters.SocketIoFlags = Flags;
    Parameters.TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = NULL;
    Parameters.RemotePath = NULL;
    Parameters.RemotePathSize = 0;
    Parameters.ControlData = NULL;
    Parameters.ControlDataSize = 0;

    ASSERT_SOCKET_IO_FLAGS_ARE_EQUIVALENT();

    if (SourceAddress != NULL) {
        NetworkAddress.Domain = NetDomainInvalid;
        ClpGetPathFromSocketAddress(SourceAddress,
                                    SourceAddressLength,
                                    &(Parameters.RemotePath),
                                    &(Parameters.RemotePathSize));

        Parameters.NetworkAddress = &NetworkAddress;
    }

    Status = OsSocketPerformIo((HANDLE)(UINTN)Socket, &Parameters, Buffer);
    if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = EOPNOTSUPP;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    //
    // If requested, attempt to translate the network address provided by the
    // kernel to a C library socket address.
    //

    if (SourceAddress != NULL) {
        Status = ClpConvertFromNetworkAddress(&NetworkAddress,
                                              SourceAddress,
                                              SourceAddressLength,
                                              Parameters.RemotePathSize);

        if (!KSUCCESS(Status)) {
            errno = EINVAL;
            return -1;
        }
    }

    return (ssize_t)(Parameters.Size);
}

LIBC_API
ssize_t
recvmsg (
    int Socket,
    struct msghdr *Message,
    int Flags
    )

/*++

Routine Description:

    This routine receives data from a socket, potentially receiving the
    source address for connection-less sockets. This variation of the recv
    function has the ability to receive vectored I/O, as well as ancillary
    data.

Arguments:

    Socket - Supplies the file descriptor of the socket to receive data from.

    Message - Supplies a pointer to an initialized structure where the message
        information will be returned. The caller must initialize the
        appropriate members to valid buffers if the remote network address or
        ancillary data is desired.

    Flags - Supplies a bitfield of flags governing the reception of the data.
        See MSG_* definitions.

Return Value:

    Returns the number of bytes received on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NETWORK_ADDRESS Address;
    SOCKET_IO_PARAMETERS Parameters;
    KSTATUS Status;
    UINTN VectorIndex;

    if (Message == NULL) {
        errno = EINVAL;
        return -1;
    }

    Parameters.Size = 0;
    for (VectorIndex = 0; VectorIndex < Message->msg_iovlen; VectorIndex += 1) {
        Parameters.Size += Message->msg_iov[VectorIndex].iov_len;
    }

    ASSERT_SOCKET_IO_FLAGS_ARE_EQUIVALENT();

    Parameters.IoFlags = 0;
    Parameters.SocketIoFlags = Flags;
    Parameters.TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = NULL;
    Parameters.RemotePath = NULL;
    Parameters.RemotePathSize = 0;
    if ((Message->msg_name != NULL) && (Message->msg_namelen != 0)) {
        Address.Domain = NetDomainInvalid;
        ClpGetPathFromSocketAddress(Message->msg_name,
                                    &(Message->msg_namelen),
                                    &(Parameters.RemotePath),
                                    &(Parameters.RemotePathSize));

        Parameters.NetworkAddress = &Address;
    }

    Parameters.ControlData = Message->msg_control;
    Parameters.ControlDataSize = Message->msg_controllen;
    Status = OsSocketPerformVectoredIo((HANDLE)(UINTN)Socket,
                                       &Parameters,
                                       (PIO_VECTOR)(Message->msg_iov),
                                       Message->msg_iovlen);

    Message->msg_flags = Parameters.SocketIoFlags;
    if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        if (Status == STATUS_NOT_SUPPORTED) {
            errno = EOPNOTSUPP;

        } else {
            errno = ClConvertKstatusToErrorNumber(Status);
        }

        return -1;
    }

    //
    // If requested, attempt to translate the network address provided by the
    // kernel to a C library socket address.
    //

    if ((Message->msg_name != NULL) && (Message->msg_namelen != 0)) {
        Status = ClpConvertFromNetworkAddress(&Address,
                                              Message->msg_name,
                                              &(Message->msg_namelen),
                                              Parameters.RemotePathSize);

        if (!KSUCCESS(Status)) {
            errno = EINVAL;
            return -1;
        }
    }

    return (ssize_t)(Parameters.Size);
}

LIBC_API
int
shutdown (
    int Socket,
    int How
    )

/*++

Routine Description:

    This routine shuts down all or part of a full-duplex socket connection.

Arguments:

    Socket - Supplies the socket to shut down.

    How - Supplies the type of shutdown. Valid values are SHUT_RD to disable
        further receive operations, SHUT_WR to disable further send operations,
        or SHUT_RDWR to disable further send and receive operations.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    ULONG ShutdownType;
    KSTATUS Status;

    switch (How) {
    case SHUT_RD:
        ShutdownType = SOCKET_SHUTDOWN_READ;
        break;

    case SHUT_WR:
        ShutdownType = SOCKET_SHUTDOWN_WRITE;
        break;

    case SHUT_RDWR:
        ShutdownType = SOCKET_SHUTDOWN_READ | SOCKET_SHUTDOWN_WRITE;
        break;

    default:
        errno = EINVAL;
        return -1;
    }

    Status = OsSocketShutdown((HANDLE)(UINTN)Socket, ShutdownType);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
setsockopt (
    int Socket,
    int Level,
    int OptionName,
    const void *OptionValue,
    socklen_t OptionLength
    )

/*++

Routine Description:

    This routine sets a socket option for the given socket.

Arguments:

    Socket - Supplies the file descriptor of the socket to set options for.

    Level - Supplies the protocol level at which the option resides. To set
        options at the socket level, supply SOL_SOCKET. To set options at other
        levels, specify the identifier for the protocol controlling the option.
        For example, to indicate that an option is interpreted by the TCP
        protocol, set this parameter to IPPROTO_TCP.

    OptionName - Supplies the option name that is passed to the protocol module
        for interpretation. See SO_* definitions.

    OptionValue - Supplies a pointer to a buffer that is passed uninterpreted
        to the protocol module. The contents of the buffer are option-specific.

    OptionLength - Supplies the length of the option buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    BOOL BooleanOption;
    PVOID Data;
    UINTN DataSize;
    SOCKET_INFORMATION_TYPE InformationType;
    UINTN IntegerOption;
    PINT IntegerOptionValue;
    struct linger *LingerOptionValue;
    INT Result;
    UINTN SecondOption;
    SOCKET_LINGER SocketLingerOption;
    UINTN SocketOption;
    KSTATUS Status;
    ULONG TimeoutOption;
    struct timeval *TimevalOptionValue;

    //
    // Convert from the POSIX style option value to the OS value.
    //

    Result = 0;
    switch (Level) {
    case SOL_SOCKET:
        switch (OptionName) {
        case SO_DEBUG:
        case SO_ACCEPTCONN:
        case SO_BROADCAST:
        case SO_REUSEADDR:
        case SO_KEEPALIVE:
        case SO_OOBINLINE:
        case SO_DONTROUTE:
        case SO_REUSEPORT:
        case SO_PASSCRED:
            if (OptionLength != sizeof(INT)) {
                errno = EINVAL;
                Result = -1;
                break;
            }

            IntegerOptionValue = (PINT)OptionValue;
            if (*IntegerOptionValue != 0) {
                BooleanOption = TRUE;

            } else {
                BooleanOption = FALSE;
            }

            Data = &BooleanOption;
            DataSize = sizeof(BOOL);
            break;

        case SO_LINGER:
            if (OptionLength != sizeof(struct linger)) {
                errno = EINVAL;
                Result = -1;
                break;
            }

            LingerOptionValue = (struct linger *)OptionValue;
            if (LingerOptionValue->l_onoff != 0) {
                SocketLingerOption.LingerEnabled = TRUE;

            } else {
                SocketLingerOption.LingerEnabled = FALSE;
            }

            SocketLingerOption.LingerTimeout = LingerOptionValue->l_linger *
                                               MILLISECONDS_PER_SECOND;

            Data = &SocketLingerOption;
            DataSize = sizeof(SOCKET_LINGER);
            break;

        case SO_SNDBUF:
        case SO_RCVBUF:
        case SO_RCVLOWAT:
        case SO_SNDLOWAT:
            if (OptionLength != sizeof(INT)) {
                errno = EINVAL;
                Result = -1;
                break;
            }

            IntegerOptionValue = (PINT)OptionValue;
            IntegerOption = *IntegerOptionValue;
            Data = &IntegerOption;
            DataSize = sizeof(UINTN);
            break;

        case SO_RCVTIMEO:
        case SO_SNDTIMEO:
            if (OptionLength != sizeof(struct timeval)) {
                errno = EINVAL;
                Result = -1;
                break;
            }

            TimevalOptionValue = (struct timeval *)OptionValue;

            //
            // Infinite wait time is represented by the 0 value in POSIX.
            //

            if ((TimevalOptionValue->tv_sec == 0) &&
                (TimevalOptionValue->tv_usec == 0)) {

                TimeoutOption = SYS_WAIT_TIME_INDEFINITE;

            } else {
                TimeoutOption = (TimevalOptionValue->tv_sec *
                                 MILLISECONDS_PER_SECOND) +
                                (TimevalOptionValue->tv_usec /
                                 MICROSECONDS_PER_MILLISECOND);
            }

            Data = &TimeoutOption;
            DataSize = sizeof(ULONG);
            break;

        //
        // Pass the buffer directly in, since the ucred structure is the same
        // as the unix socket credentials structure.
        //

        case SO_PEERCRED:
            Data = (PVOID)OptionValue;
            DataSize = OptionLength;
            break;

        default:
            errno = EINVAL;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_TCP:
        switch (OptionName) {
        case TCP_NODELAY:
        case TCP_KEEPIDLE:
        case TCP_KEEPINTVL:
        case TCP_KEEPCNT:
            if (OptionLength != sizeof(INT)) {
                errno = EINVAL;
                Result = -1;
                break;
            }

            IntegerOptionValue = (PINT)OptionValue;
            IntegerOption = *IntegerOptionValue;
            Data = &IntegerOption;
            DataSize = sizeof(ULONG);
            break;

        default:
            errno = ENOTSUP;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_IP:
        switch (OptionName) {
        case IP_HDRINCL:
            if (OptionLength != sizeof(INT)) {
                errno = EINVAL;
                Result = -1;
                break;
            }

            IntegerOptionValue = (PINT)OptionValue;
            if (*IntegerOptionValue != 0) {
                BooleanOption = TRUE;

            } else {
                BooleanOption = FALSE;
            }

            Data = &BooleanOption;
            DataSize = sizeof(BOOL);
            break;

        default:
            errno = ENOTSUP;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_UDP:
    case IPPROTO_RAW:
    case IPPROTO_ICMP:
    case IPPROTO_IPV6:
    default:
        errno = ENOTSUP;
        Result = -1;
        break;
    }

    if (Result != 0) {
        goto setsockoptEnd;
    }

    //
    // Convert the POSIX style level and option name.
    //

    Status = ClpConvertSocketLevelAndOptionName(Level,
                                                OptionName,
                                                &InformationType,
                                                &SocketOption,
                                                &SecondOption);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        Result = -1;
        goto setsockoptEnd;
    }

    //
    // Set the converted socket option.
    //

    Status = OsSocketGetSetInformation((HANDLE)(UINTN)Socket,
                                       InformationType,
                                       SocketOption,
                                       Data,
                                       &DataSize,
                                       TRUE);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        Result = -1;
        goto setsockoptEnd;
    }

    //
    // If the POSIX option maps to a second socket option, then set that one
    // as well. It will always be of the same type and take the same data.
    //

    if (SecondOption != INVALID_SOCKET_OPTION) {

        //
        // This is only enabled for boolean values.
        //

        assert(Data == &BooleanOption);

        Status = OsSocketGetSetInformation((HANDLE)(UINTN)Socket,
                                           InformationType,
                                           SecondOption,
                                           Data,
                                           &DataSize,
                                           TRUE);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            Result = -1;
            goto setsockoptEnd;
        }
    }

setsockoptEnd:
    return Result;
}

LIBC_API
int
getsockopt (
    int Socket,
    int Level,
    int OptionName,
    void *OptionValue,
    socklen_t *OptionLength
    )

/*++

Routine Description:

    This routine retrieves the current value of a given socket option.

Arguments:

    Socket - Supplies the file descriptor of the socket.

    Level - Supplies the protocol level at which the option resides. To get
        options at the socket level, supply SOL_SOCKET. To get options at other
        levels, specify the identifier for the protocol controlling the option.
        For example, to indicate that an option is interpreted by the TCP
        protocol, set this parameter to IPPROTO_TCP.

    OptionName - Supplies the option name that is passed to the protocol module
        for interpretation. See SO_* definitions.

    OptionValue - Supplies a pointer to a buffer where the option value is
        returned on success.

    OptionLength - Supplies a pointer that on input contains the size of the
        option value buffer in bytes. On output, this will contain the actual
        size of the value. If the value returned was greater than the value
        passed in, then the option value was silently truncated.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    BOOL BooleanOption;
    PVOID Data;
    UINTN DataSize;
    SOCKET_INFORMATION_TYPE InformationType;
    UINTN IntegerOption;
    INT IntegerOptionValue;
    struct linger LingerOptionValue;
    INT Result;
    UINTN SecondOption;
    SOCKET_LINGER SocketLingerOption;
    UINTN SocketOption;
    KSTATUS Status;
    ULONG TimeoutOption;
    struct timeval TimevalOptionValue;
    NET_SOCKET_TYPE TypeOption;

    //
    // Convert from the POSIX style option value to the OS value.
    //

    Result = 0;
    switch (Level) {
    case SOL_SOCKET:
        switch (OptionName) {
        case SO_DEBUG:
        case SO_ACCEPTCONN:
        case SO_BROADCAST:
        case SO_REUSEADDR:
        case SO_KEEPALIVE:
        case SO_OOBINLINE:
        case SO_DONTROUTE:
        case SO_REUSEPORT:
        case SO_PASSCRED:
            Data = &BooleanOption;
            DataSize = sizeof(BOOL);
            break;

        case SO_LINGER:
            Data = &SocketLingerOption;
            DataSize = sizeof(SOCKET_LINGER);
            break;

        case SO_SNDBUF:
        case SO_RCVBUF:
        case SO_RCVLOWAT:
        case SO_SNDLOWAT:
        case SO_ERROR:
            Data = &IntegerOption;
            DataSize = sizeof(UINTN);
            break;

        case SO_RCVTIMEO:
        case SO_SNDTIMEO:
            Data = &TimeoutOption;
            DataSize = sizeof(ULONG);
            break;

        case SO_TYPE:
            Data = &TypeOption;
            DataSize = sizeof(NET_SOCKET_TYPE);
            break;

        case SO_PEERCRED:
            Data = OptionValue;
            DataSize = *OptionLength;
            break;

        default:
            errno = EINVAL;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_TCP:
        switch (OptionName) {
        case TCP_NODELAY:
        case TCP_KEEPIDLE:
        case TCP_KEEPINTVL:
        case TCP_KEEPCNT:
            Data = &IntegerOption;
            DataSize = sizeof(ULONG);
            break;

        default:
            errno = ENOTSUP;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_IP:
        switch (OptionName) {
        case IP_HDRINCL:
            Data = &BooleanOption;
            DataSize = sizeof(BOOL);
            break;

        default:
            errno = ENOTSUP;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_UDP:
    case IPPROTO_RAW:
    case IPPROTO_ICMP:
    case IPPROTO_IPV6:
    default:
        errno = ENOTSUP;
        Result = -1;
        break;
    }

    if (Result != 0) {
        goto getsockoptEnd;
    }

    //
    // Convert the POSIX style level and option name.
    //

    Status = ClpConvertSocketLevelAndOptionName(Level,
                                                OptionName,
                                                &InformationType,
                                                &SocketOption,
                                                &SecondOption);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        Result = -1;
        goto getsockoptEnd;
    }

    //
    // Get the converted socket option from the system.
    //

    Status = OsSocketGetSetInformation((HANDLE)(UINTN)Socket,
                                       InformationType,
                                       SocketOption,
                                       Data,
                                       &DataSize,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        Result = -1;
        goto getsockoptEnd;
    }

    //
    // Return the requested property back to the caller, truncating the data if
    // the supplied option length is not big enough.
    //

    Result = 0;
    switch (Level) {
    case SOL_SOCKET:
        switch (OptionName) {
        case SO_DEBUG:
        case SO_ACCEPTCONN:
        case SO_BROADCAST:
        case SO_REUSEADDR:
        case SO_KEEPALIVE:
        case SO_OOBINLINE:
        case SO_DONTROUTE:
        case SO_REUSEPORT:
        case SO_PASSCRED:

            //
            // If there is a second option and the first option was enabled,
            // then get the second option. If the second is also enabled then
            // the routine will report the combo as enabled, otherwise the
            // combo should be considered disabled.
            //

            if ((SecondOption != INVALID_SOCKET_OPTION) &&
                (BooleanOption != FALSE)) {

                Data = &BooleanOption;
                DataSize = sizeof(BOOL);
                Status = OsSocketGetSetInformation((HANDLE)(UINTN)Socket,
                                                   InformationType,
                                                   SecondOption,
                                                   Data,
                                                   &DataSize,
                                                   FALSE);

                if (!KSUCCESS(Status)) {
                    errno = ClConvertKstatusToErrorNumber(Status);
                    Result = -1;
                    goto getsockoptEnd;
                }
            }

            IntegerOptionValue = BooleanOption;
            Data = &IntegerOptionValue;
            DataSize = sizeof(INT);
            break;

        case SO_LINGER:
            LingerOptionValue.l_onoff = SocketLingerOption.LingerEnabled;
            LingerOptionValue.l_linger = SocketLingerOption.LingerTimeout /
                                         MILLISECONDS_PER_SECOND;

            Data = &LingerOptionValue;
            DataSize = sizeof(struct linger);
            break;

        case SO_SNDBUF:
        case SO_RCVBUF:
        case SO_RCVLOWAT:
        case SO_SNDLOWAT:
        case SO_ERROR:
            IntegerOptionValue = IntegerOption;
            Data = &IntegerOptionValue;
            DataSize = sizeof(INT);
            break;

        case SO_RCVTIMEO:
        case SO_SNDTIMEO:

            //
            // Infinite wait time is represented by the 0 value in POSIX.
            //

            if (TimeoutOption == SYS_WAIT_TIME_INDEFINITE) {
                TimeoutOption = 0;
            }

            TimevalOptionValue.tv_sec = TimeoutOption / MILLISECONDS_PER_SECOND;
            TimevalOptionValue.tv_usec = (TimeoutOption %
                                          MILLISECONDS_PER_SECOND) *
                                         MICROSECONDS_PER_MILLISECOND;

            Data = &TimevalOptionValue;
            DataSize = sizeof(struct timeval);
            break;

        case SO_TYPE:

            ASSERT_SOCKET_TYPES_EQUIVALENT();

            IntegerOptionValue = TypeOption;
            Data = &IntegerOptionValue;
            DataSize = sizeof(INT);
            break;

        //
        // For this option, the data was written directly in the caller's
        // buffer.
        //

        case SO_PEERCRED:
            break;

        default:
            errno = EINVAL;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_TCP:
        switch (OptionName) {
        case TCP_NODELAY:
        case TCP_KEEPIDLE:
        case TCP_KEEPINTVL:
        case TCP_KEEPCNT:
            IntegerOptionValue = IntegerOption;
            Data = &IntegerOptionValue;
            DataSize = sizeof(INT);
            break;

        default:
            errno = ENOTSUP;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_IP:
        switch (OptionName) {
        case IP_HDRINCL:
            IntegerOptionValue = BooleanOption;
            Data = &IntegerOptionValue;
            DataSize = sizeof(INT);
            break;

        default:
            errno = ENOTSUP;
            Result = -1;
            break;
        }

        break;

    case IPPROTO_UDP:
    case IPPROTO_RAW:
    case IPPROTO_ICMP:
    case IPPROTO_IPV6:
    default:
        errno = ENOTSUP;
        Result = -1;
        break;
    }

    if (Result != 0) {
        goto getsockoptEnd;
    }

    //
    // The data has been converted back to the expected type. Copy it from the
    // data pointer up to the data option length.
    //

    if ((UINTN)*OptionLength < DataSize) {
        DataSize = *OptionLength;

    } else {
        *OptionLength = DataSize;
    }

    RtlCopyMemory(OptionValue, Data, DataSize);

getsockoptEnd:
    return Result;
}

LIBC_API
int
getsockname (
    int Socket,
    struct sockaddr *SocketAddress,
    socklen_t *AddressLength
    )

/*++

Routine Description:

    This routine returns the current address to which the given socket is bound.

Arguments:

    Socket - Supplies the file descriptor of the socket.

    SocketAddress - Supplies a pointer where the socket address will be
        returned.

    AddressLength - Supplies a pointer that on input supplies the size of the
        socket address buffer. On output, this will contain the actual size of
        the buffer. The buffer will have been truncated if the number returned
        here is greater than the number supplied.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    int Result;

    Result = ClpGetSocketAddress(Socket,
                                 SocketBasicOptionLocalAddress,
                                 SocketAddress,
                                 AddressLength);

    return Result;
}

LIBC_API
int
getpeername (
    int Socket,
    struct sockaddr *SocketAddress,
    socklen_t *AddressLength
    )

/*++

Routine Description:

    This routine returns the peer address of the specified socket.

Arguments:

    Socket - Supplies the file descriptor of the socket.

    SocketAddress - Supplies a pointer where the socket's peer address will be
        returned.

    AddressLength - Supplies a pointer that on input supplies the size of the
        socket address buffer. On output, this will contain the actual size of
        the buffer. The buffer will have been truncated if the number returned
        here is greater than the number supplied.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    int Result;

    Result = ClpGetSocketAddress(Socket,
                                 SocketBasicOptionRemoteAddress,
                                 SocketAddress,
                                 AddressLength);

    return Result;
}

KSTATUS
ClpConvertToNetworkAddress (
    const struct sockaddr *Address,
    UINTN AddressLength,
    PNETWORK_ADDRESS NetworkAddress,
    PSTR *Path,
    PUINTN PathSize
    )

/*++

Routine Description:

    This routine converts a sockaddr address structure into a network address
    structure.

Arguments:

    Address - Supplies a pointer to the address structure.

    AddressLength - Supplies the length of the address structure in bytes.

    NetworkAddress - Supplies a pointer where the corresponding network address
        will be returned.

    Path - Supplies an optional pointer where a pointer to the path will be
        returned if this is a Unix address.

    PathSize - Supplies an optional pointer where the path size will be
        returned if this is a Unix address.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_ADDRESS on failure.

--*/

{

    struct sockaddr_in *Ip4SocketAddress;
    struct sockaddr_in6 *Ip6SocketAddress;
    struct sockaddr_nl *NetlinkAddress;
    UINTN StringSize;
    struct sockaddr_un *UnixAddress;

    RtlZeroMemory(NetworkAddress, sizeof(NETWORK_ADDRESS));
    if (AddressLength < sizeof(sa_family_t)) {
        return STATUS_INVALID_ADDRESS;
    }

    if (Address->sa_family == AF_INET) {
        if (AddressLength < sizeof(struct sockaddr_in)) {
            return STATUS_INVALID_ADDRESS;
        }

        Ip4SocketAddress = (struct sockaddr_in *)Address;
        NetworkAddress->Domain = NetDomainIp4;

        //
        // The network address port is in host order, but the address is in
        // network order.
        //

        NetworkAddress->Port = ntohs(Ip4SocketAddress->sin_port);
        RtlCopyMemory(NetworkAddress->Address,
                      &(Ip4SocketAddress->sin_addr.s_addr),
                      sizeof(in_addr_t));

    } else if (Address->sa_family == AF_INET6) {
        if (AddressLength < sizeof(struct sockaddr_in6)) {
            return STATUS_INVALID_ADDRESS;
        }

        Ip6SocketAddress = (struct sockaddr_in6 *)Address;
        NetworkAddress->Domain = NetDomainIp6;

        //
        // The network address port is in host order, but the address is in
        // network order.
        //

        NetworkAddress->Port = ntohs(Ip6SocketAddress->sin6_port);
        RtlCopyMemory(NetworkAddress->Address,
                      &(Ip6SocketAddress->sin6_addr),
                      sizeof(struct in6_addr));

        //
        // TODO: Update NETWORK_ADDRESS for IPv6.
        //

    } else if (Address->sa_family == AF_UNIX) {
        UnixAddress = (struct sockaddr_un *)Address;
        NetworkAddress->Domain = NetDomainLocal;
        if (Path != NULL) {
            *Path = UnixAddress->sun_path;
        }

        //
        // The address length is supposed to include a null terminator. If the
        // last character isn't a null terminator, then append a null
        // terminator and increase the size.
        //

        StringSize = AddressLength - FIELD_OFFSET(struct sockaddr_un, sun_path);
        if ((StringSize + 1 < UNIX_PATH_MAX) &&
            (StringSize != 0) &&
            (UnixAddress->sun_path[0] != '\0') &&
            (UnixAddress->sun_path[StringSize - 1] != '\0')) {

            UnixAddress->sun_path[StringSize] = '\0';
            StringSize += 1;
        }

        if (PathSize != NULL) {
            *PathSize = StringSize;
        }

    } else if (Address->sa_family == AF_NETLINK) {
        if (AddressLength < sizeof(struct sockaddr_nl)) {
            return STATUS_INVALID_ADDRESS;
        }

        NetlinkAddress = (struct sockaddr_nl *)Address;
        NetworkAddress->Domain = NetDomainNetlink;
        NetworkAddress->Port = NetlinkAddress->nl_pid;
        *((PULONG)NetworkAddress->Address) = NetlinkAddress->nl_groups;

    } else {
        return STATUS_INVALID_ADDRESS;
    }

    return STATUS_SUCCESS;
}

KSTATUS
ClpConvertFromNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    struct sockaddr *Address,
    socklen_t *AddressLength,
    UINTN PathSize
    )

/*++

Routine Description:

    This routine converts a network address structure into a sockaddr structure.

Arguments:

    NetworkAddress - Supplies a pointer to the network address to convert.

    Address - Supplies a pointer where the address structure will be returned.

    AddressLength - Supplies a pointer that on input contains the length of the
        specified Address structure, and on output returns the length of the
        returned address. If the supplied buffer is not big enough to hold the
        address, the address is truncated, and the larger needed buffer size
        will be returned here.

    PathSize - Supplies the size of the path, if this is a local Unix address.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the address buffer is not big enough.

    STATUS_INVALID_ADDRESS on failure.

--*/

{

    INTN CopySize;
    struct sockaddr_in Ip4Address;
    struct sockaddr_in6 Ip6Address;
    struct sockaddr_nl NetlinkAddress;
    PVOID Source;
    INTN TotalSize;
    struct sockaddr_un UnixAddress;

    if (NetworkAddress->Domain == NetDomainIp4) {
        Ip4Address.sin_family = AF_INET;
        Ip4Address.sin_port = htons((USHORT)(NetworkAddress->Port));
        Ip4Address.sin_addr.s_addr = *((PULONG)(NetworkAddress->Address));
        TotalSize = sizeof(Ip4Address);
        Source = &Ip4Address;

    } else if (NetworkAddress->Domain == NetDomainIp6) {
        Ip6Address.sin6_family = AF_INET6;
        Ip6Address.sin6_port = htons((USHORT)(NetworkAddress->Port));

        //
        // TODO: Update NETWORK_ADDRESS for IPv6.
        //

        Ip6Address.sin6_flowinfo = 0;
        Ip6Address.sin6_scope_id = 0;
        RtlCopyMemory(&(Ip6Address.sin6_addr),
                      NetworkAddress->Address,
                      sizeof(struct in6_addr));

        TotalSize = sizeof(Ip6Address);
        Source = &Ip6Address;

    } else if (NetworkAddress->Domain == NetDomainLocal) {
        UnixAddress.sun_family = AF_UNIX;
        TotalSize = FIELD_OFFSET(struct sockaddr_un, sun_path) + PathSize;
        Source = &UnixAddress;

    } else if (NetworkAddress->Domain == NetDomainNetlink) {
        NetlinkAddress.nl_family = AF_NETLINK;
        NetlinkAddress.nl_pad = 0;
        NetlinkAddress.nl_pid = NetworkAddress->Port;
        NetlinkAddress.nl_groups = *((PULONG)NetworkAddress->Address);
        TotalSize = sizeof(NetlinkAddress);
        Source = &NetlinkAddress;

    } else {
        return STATUS_INVALID_ADDRESS;
    }

    CopySize = TotalSize;
    if (CopySize < *AddressLength) {
        CopySize = *AddressLength;
    }

    RtlCopyMemory(Address, Source, CopySize);
    *AddressLength = TotalSize;
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ClpGetSocketAddress (
    int Socket,
    SOCKET_BASIC_OPTION Option,
    struct sockaddr *SocketAddress,
    socklen_t *AddressLength
    )

/*++

Routine Description:

    This routine returns either the address of the socket itself or the socket
    this socket is connected to.

Arguments:

    Socket - Supplies the file descriptor of the socket.

    Option - Supplies the socket option. Valid arguments are the local address
        and remote address.

    SocketAddress - Supplies a pointer where the socket address will be
        returned.

    AddressLength - Supplies a pointer that on input supplies the size of the
        socket address buffer. On output, this will contain the actual size of
        the buffer. The buffer will have been truncated if the number returned
        here is greater than the number supplied.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    UCHAR Buffer[sizeof(NETWORK_ADDRESS) + UNIX_PATH_MAX];
    UINTN BufferSize;
    PNETWORK_ADDRESS LocalAddress;
    UINTN SizeMinusHeader;
    KSTATUS Status;
    struct sockaddr_un *UnixAddress;

    ASSERT((Option == SocketBasicOptionLocalAddress) ||
           (Option == SocketBasicOptionRemoteAddress));

    LocalAddress = (PNETWORK_ADDRESS)Buffer;
    LocalAddress->Domain = NetDomainInvalid;
    BufferSize = sizeof(Buffer);
    Status = OsSocketGetSetInformation((HANDLE)(UINTN)Socket,
                                       SocketInformationTypeBasic,
                                       Option,
                                       Buffer,
                                       &BufferSize,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    //
    // Even if the socket is not bound, it should at least return the any
    // address and any port.
    //

    assert(LocalAddress->Domain != NetDomainInvalid);

    Status = ClpConvertFromNetworkAddress(LocalAddress,
                                          SocketAddress,
                                          AddressLength,
                                          BufferSize - sizeof(NETWORK_ADDRESS));

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    if (LocalAddress->Domain == NetDomainLocal) {

        ASSERT(BufferSize >= sizeof(NETWORK_ADDRESS));

        BufferSize -= sizeof(NETWORK_ADDRESS);
        SizeMinusHeader = *AddressLength -
                          FIELD_OFFSET(struct sockaddr_un, sun_path);

        if (BufferSize > SizeMinusHeader) {
            BufferSize = SizeMinusHeader;
        }

        if (BufferSize != 0) {
            UnixAddress = (struct sockaddr_un *)SocketAddress;
            memcpy(UnixAddress->sun_path, LocalAddress + 1, BufferSize);
        }
    }

    return 0;
}

KSTATUS
ClpConvertSocketLevelAndOptionName (
    INT Level,
    INT OptionName,
    PSOCKET_INFORMATION_TYPE InformationType,
    PUINTN SocketOption,
    PUINTN SecondOption
    )

/*++

Routine Description:

    This routine converts a POSIX socket level and option name into the
    system's socket information type and socket option.

Arguments:

    Level - Supplies the protocol level at which a socket option resides. This
        is the same type as the Level argument to getsocketopt and setsockopt.

    OptionName - Supplies the level-specific option name. This is the same type
        as the OptionName argument to getsockopt and setsockopt.

    InformationType - Supplies a pointer that receives the converted socket
        information type based on the given Level.

    SocketOption - Supplies a pointer that receives the converted socket option
        based on the given OptionName.

    SecondOption - Supplies a pointer that receives a possible second socket
        option that maps to the supplied POSIX option name.

Return Value:

    Status code.

--*/

{

    UINTN ElementCount;
    KSTATUS Status;

    *SocketOption = INVALID_SOCKET_OPTION;
    *SecondOption = INVALID_SOCKET_OPTION;

    //
    // Convert from the POSIX style level and option name to the OS types.
    //

    Status = STATUS_SUCCESS;
    switch (Level) {
    case SOL_SOCKET:
        *InformationType = SocketInformationTypeBasic;
        switch (OptionName) {
        case SO_REUSEADDR:
            *SocketOption = SocketBasicOptionReuseAnyAddress;
            *SecondOption = SocketBasicOptionReuseTimeWait;
            break;

        default:
            ElementCount = sizeof(ClpSocketOptionNameToBasicOption) /
                           sizeof(ClpSocketOptionNameToBasicOption[0]);

            if ((UINTN)OptionName >= ElementCount) {
                Status = STATUS_INVALID_PARAMETER;

            } else {
                *SocketOption = ClpSocketOptionNameToBasicOption[OptionName];
                if (*SocketOption == SocketBasicOptionInvalid) {
                    Status = STATUS_NOT_SUPPORTED;
                }
            }

            break;
        }

        break;

    case IPPROTO_TCP:
        *InformationType = SocketInformationTypeTcp;
        ElementCount = sizeof(ClpSocketOptionNameToTcpOption) /
                       sizeof(ClpSocketOptionNameToTcpOption[0]);

        if ((UINTN)OptionName >= ElementCount) {
            Status = STATUS_INVALID_PARAMETER;

        } else {
            *SocketOption = ClpSocketOptionNameToTcpOption[OptionName];
            if (*SocketOption == SocketTcpOptionInvalid) {
                Status = STATUS_NOT_SUPPORTED;
            }
        }

        break;

    case IPPROTO_UDP:
        *InformationType = SocketInformationTypeUdp;
        Status = STATUS_NOT_SUPPORTED;
        break;

    case IPPROTO_RAW:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case IPPROTO_ICMP:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case IPPROTO_IP:
        *InformationType = SocketInformationTypeIp4;
        ElementCount = sizeof(ClpSocketOptionNameToIp4Option) /
                       sizeof(ClpSocketOptionNameToIp4Option[0]);

        if ((UINTN)OptionName >= ElementCount) {
            Status = STATUS_INVALID_PARAMETER;

        } else {
            *SocketOption = ClpSocketOptionNameToIp4Option[OptionName];
            if (*SocketOption == SocketIp4OptionInvalid) {
                Status = STATUS_NOT_SUPPORTED;
            }
        }

        break;

    case IPPROTO_IPV6:
        *InformationType = SocketInformationTypeIp6;
        Status = STATUS_NOT_SUPPORTED;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

VOID
ClpGetPathFromSocketAddress (
    struct sockaddr *Address,
    socklen_t *AddressLength,
    PSTR *Path,
    PUINTN PathSize
    )

/*++

Routine Description:

    This routine returns the path and path size from an optional address if it
    is a Unix socket address.

Arguments:

    Address - Supplies an optional pointer to a socket address.

    AddressLength - Supplies an optional pointer to the address length.

    Path - Supplies a pointer where a pointer within the address to the path
        will be returned on success.

    PathSize - Supplies a pointer where the size of the path part will be
        returned on success.

Return Value:

    None.

--*/

{

    struct sockaddr_un *UnixAddress;

    *Path = NULL;
    *PathSize = 0;
    UnixAddress = (struct sockaddr_un *)Address;
    if ((Address != NULL) &&
        (AddressLength != NULL) &&
        ((UINTN)*AddressLength >= FIELD_OFFSET(struct sockaddr_un, sun_path))) {

        *Path = UnixAddress->sun_path;
        *PathSize = *AddressLength -
                    FIELD_OFFSET(struct sockaddr_un, sun_path);
    }

    return;
}

