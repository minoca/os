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
#include <limits.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro asserts that the MSG_* flags are equal to the SOCKET_IO_* flags
// and require no translation. It is basically a static assert that gets
// compiled away.
//

#define ASSERT_SOCKET_IO_FLAGS_ARE_EQUIVALENT()           \
    ASSERT((MSG_PEEK == SOCKET_IO_PEEK) &&                \
           (MSG_OOB == SOCKET_IO_OUT_OF_BAND) &&          \
           (MSG_WAITALL == SOCKET_IO_WAIT_ALL) &&         \
           (MSG_TRUNC == SOCKET_IO_DATA_TRUNCATED) &&     \
           (MSG_CTRUNC == SOCKET_IO_CONTROL_TRUNCATED) && \
           (MSG_NOSIGNAL == SOCKET_IO_NO_SIGNAL) &&       \
           (MSG_DONTWAIT == SOCKET_IO_NON_BLOCKING) &&    \
           (MSG_DONTROUTE == SOCKET_IO_DONT_ROUTE))

#define ASSERT_SOCKET_TYPES_EQUIVALENT()                   \
    ASSERT((SOCK_DGRAM == NetSocketDatagram) &&            \
           (SOCK_RAW == NetSocketRaw) &&                   \
           (SOCK_SEQPACKET == NetSocketSequencedPacket) && \
           (SOCK_STREAM == NetSocketStream))

#define ASSERT_DOMAIN_TYPES_EQUIVALENT()   \
    ASSERT((AF_UNIX == NetDomainLocal) &&  \
           (AF_LOCAL == NetDomainLocal) && \
           (AF_INET == NetDomainIp4) &&    \
           (AF_INET6 == NetDomainIp6))

#define ASSERT_SOCKET_LEVELS_EQUIVALENT()            \
    ASSERT((SOL_SOCKET == SocketInformationBasic) && \
           (IPPROTO_IP == SocketInformationIp4) &&   \
           (IPPROTO_IPV6 == SocketInformationIp6) && \
           (IPPROTO_TCP == SocketInformationTcp) &&  \
           (IPPROTO_UDP == SocketInformationUdp) &&  \
           (IPPROTO_RAW == SocketInformationRaw))

#define ASSERT_SOCKET_BASIC_OPTIONS_EQUIVALENT()                    \
    ASSERT((SO_ACCEPTCONN == SocketBasicOptionAcceptConnections) && \
           (SO_BROADCAST == SocketBasicOptionBroadcastEnabled) &&   \
           (SO_DEBUG == SocketBasicOptionDebug) &&                  \
           (SO_DONTROUTE == SocketBasicOptionRoutingDisabled) &&    \
           (SO_ERROR == SocketBasicOptionErrorStatus) &&            \
           (SO_KEEPALIVE == SocketBasicOptionKeepAlive) &&          \
           (SO_LINGER == SocketBasicOptionLinger) &&                \
           (SO_OOBINLINE == SocketBasicOptionInlineOutOfBand) &&    \
           (SO_RCVBUF == SocketBasicOptionReceiveBufferSize) &&     \
           (SO_RCVLOWAT == SocketBasicOptionReceiveMinimum) &&      \
           (SO_RCVTIMEO == SocketBasicOptionReceiveTimeout) &&      \
           (SO_SNDBUF == SocketBasicOptionSendBufferSize) &&        \
           (SO_SNDLOWAT == SocketBasicOptionSendMinimum) &&         \
           (SO_SNDTIMEO == SocketBasicOptionSendTimeout) &&         \
           (SO_TYPE == SocketBasicOptionType) &&                    \
           (SO_PASSCRED == SocketBasicOptionPassCredentials) &&     \
           (SO_PEERCRED == SocketBasicOptionPeerCredentials))

#define ASSERT_SOCKET_IPV4_OPTIONS_EQUIVALENT()                          \
    ASSERT((IP_HDRINCL == SocketIp4OptionHeaderIncluded) &&              \
           (IP_ADD_MEMBERSHIP == SocketIp4OptionJoinMulticastGroup) &&   \
           (IP_DROP_MEMBERSHIP == SocketIp4OptionLeaveMulticastGroup) && \
           (IP_MULTICAST_IF == SocketIp4OptionMulticastInterface) &&     \
           (IP_MULTICAST_TTL == SocketIp4OptionMulticastTimeToLive) &&   \
           (IP_MULTICAST_LOOP == SocketIp4OptionMulticastLoopback) &&    \
           (IP_TTL == SocketIp4OptionTimeToLive) &&                      \
           (IP_TOS == SocketIp4DifferentiatedServicesCodePoint))

#define ASSERT_SOCKET_IPV6_OPTIONS_EQUIVALENT()                         \
    ASSERT((IPV6_JOIN_GROUP == SocketIp6OptionJoinMulticastGroup) &&    \
           (IPV6_LEAVE_GROUP == SocketIp6OptionLeaveMulticastGroup) &&  \
           (IPV6_MULTICAST_HOPS == SocketIp6OptionMulticastHops) &&     \
           (IPV6_MULTICAST_IF == SocketIp6OptionMulticastInterface) &&  \
           (IPV6_MULTICAST_LOOP == SocketIp6OptionMulticastLoopback) && \
           (IPV6_UNICAST_HOPS == SocketIp6OptionUnicastHops) &&       \
           (IPV6_V6ONLY == SocketIp6OptionIpv6Only))

#define ASSERT_SOCKET_TCP_OPTIONS_EQUIVALENT()                  \
    ASSERT((TCP_NODELAY == SocketTcpOptionNoDelay) &&           \
           (TCP_KEEPIDLE == SocketTcpOptionKeepAliveTimeout) && \
           (TCP_KEEPINTVL == SocketTcpOptionKeepAlivePeriod) && \
           (TCP_KEEPCNT == SocketTcpOptionKeepAliveProbeLimit))

//
// ---------------------------------------------------------------- Definitions
//

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
    Status = ClConvertToNetworkAddress(Address,
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

    if (Backlog < 0) {
        Backlog = 0;
    }

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
        Status = ClConvertFromNetworkAddress(&NetworkAddress,
                                             Address,
                                             AddressLength,
                                             RemotePath,
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
    Status = ClConvertToNetworkAddress(Address,
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

    //
    // Truncate the byte count, so that it does not exceed the maximum number
    // of bytes that can be returned.
    //

    if (Length > (size_t)SSIZE_MAX) {
        Length = (size_t)SSIZE_MAX;
    }

    Parameters.Size = Length;
    Parameters.BytesCompleted = 0;
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
        Status = ClConvertToNetworkAddress(DestinationAddress,
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

    return (ssize_t)(Parameters.BytesCompleted);
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

    //
    // Truncate the byte count, so that it does not exceed the maximum number
    // of bytes that can be returned.
    //

    if (Parameters.Size > (UINTN)SSIZE_MAX) {
        Parameters.Size = (UINTN)SSIZE_MAX;
    }

    ASSERT_SOCKET_IO_FLAGS_ARE_EQUIVALENT();

    Parameters.BytesCompleted = 0;
    Parameters.IoFlags = SYS_IO_FLAG_WRITE;
    Parameters.SocketIoFlags = Flags;
    Parameters.TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = NULL;
    Parameters.RemotePath = NULL;
    Parameters.RemotePathSize = 0;
    if ((Message->msg_name != NULL) && (Message->msg_namelen != 0)) {
        Status = ClConvertToNetworkAddress(Message->msg_name,
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

    return (ssize_t)(Parameters.BytesCompleted);
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

    //
    // Truncate the byte count, so that it does not exceed the maximum number
    // of bytes that can be returned.
    //

    if (Length > (size_t)SSIZE_MAX) {
        Length = (size_t)SSIZE_MAX;
    }

    Parameters.Size = Length;
    Parameters.BytesCompleted = 0;
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
        Status = ClConvertFromNetworkAddress(&NetworkAddress,
                                             SourceAddress,
                                             SourceAddressLength,
                                             Parameters.RemotePath,
                                             Parameters.RemotePathSize);

        if (!KSUCCESS(Status)) {
            errno = EINVAL;
            return -1;
        }
    }

    return (ssize_t)(Parameters.BytesCompleted);
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

    //
    // Truncate the byte count, so that it does not exceed the maximum number
    // of bytes that can be returned.
    //

    if (Parameters.Size > (UINTN)SSIZE_MAX) {
        Parameters.Size = (UINTN)SSIZE_MAX;
    }

    ASSERT_SOCKET_IO_FLAGS_ARE_EQUIVALENT();

    Parameters.BytesCompleted = 0;
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
    Message->msg_controllen = Parameters.ControlDataSize;
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
        Status = ClConvertFromNetworkAddress(&Address,
                                             Message->msg_name,
                                             &(Message->msg_namelen),
                                             Parameters.RemotePath,
                                             Parameters.RemotePathSize);

        if (!KSUCCESS(Status)) {
            errno = EINVAL;
            return -1;
        }
    }

    return (ssize_t)(Parameters.BytesCompleted);
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

    socklen_t LocalOptionLength;
    KSTATUS Status;

    ASSERT_SOCKET_TYPES_EQUIVALENT();
    ASSERT_SOCKET_LEVELS_EQUIVALENT();
    ASSERT_SOCKET_BASIC_OPTIONS_EQUIVALENT();
    ASSERT_SOCKET_IPV4_OPTIONS_EQUIVALENT();
    ASSERT_SOCKET_IPV6_OPTIONS_EQUIVALENT();
    ASSERT_SOCKET_TCP_OPTIONS_EQUIVALENT();

    LocalOptionLength = OptionLength;
    Status = OsSocketGetSetInformation((HANDLE)(UINTN)Socket,
                                       Level,
                                       OptionName,
                                       (PVOID)OptionValue,
                                       (PUINTN)&LocalOptionLength,
                                       TRUE);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_BUFFER_TOO_SMALL) {
            Status = STATUS_INVALID_PARAMETER;
        }

        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
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
        option value buffer in bytes. If the supplied length is less than the
        actual size of the option value, then the option value will be silently
        truncated. On output, if the supplied length is greater than the actual
        size of the value, this will contain the actual size of the value.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    UINTN CopySize;
    KSTATUS ErrorStatus;
    INT ErrorValue;
    INT LeadingZeros;
    INT Mask;
    socklen_t OriginalOptionLength;
    INT Shift;
    KSTATUS Status;

    ASSERT_SOCKET_TYPES_EQUIVALENT();
    ASSERT_SOCKET_LEVELS_EQUIVALENT();
    ASSERT_SOCKET_BASIC_OPTIONS_EQUIVALENT();
    ASSERT_SOCKET_IPV4_OPTIONS_EQUIVALENT();
    ASSERT_SOCKET_IPV6_OPTIONS_EQUIVALENT();
    ASSERT_SOCKET_TCP_OPTIONS_EQUIVALENT();

    //
    // Get the converted socket option from the system.
    //

    OriginalOptionLength = *OptionLength;
    Status = OsSocketGetSetInformation((HANDLE)(UINTN)Socket,
                                       Level,
                                       OptionName,
                                       OptionValue,
                                       (PUINTN)OptionLength,
                                       FALSE);

    if (Status == STATUS_BUFFER_TOO_SMALL) {
        Status = STATUS_SUCCESS;
        *OptionLength = OriginalOptionLength;
    }

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;

    //
    // If this is the SO_ERROR option, then the status code must be converted
    // from a KSTATUS to an errno value.
    //

    } else if ((Level == SOL_SOCKET) && (OptionName == SO_ERROR)) {
        CopySize = *OptionLength;
        if (CopySize > sizeof(KSTATUS)) {
            CopySize = sizeof(KSTATUS);
        }

        ErrorStatus = 0;
        RtlCopyMemory(&ErrorStatus, OptionValue, CopySize);

        //
        // If the error status is positive, then the option length is probably
        // less than sizeof(KSTATUS). All of the KSTATUS values are negative,
        // so the error status needs to be sign extended.
        //

        if (ErrorStatus > 0) {

            assert(CopySize < sizeof(KSTATUS));

            LeadingZeros = RtlCountLeadingZeros32(ErrorStatus);
            Shift = (sizeof(KSTATUS) * BITS_PER_BYTE) - LeadingZeros;
            Mask = ~((1 << Shift) - 1);
            ErrorStatus |= Mask;

            assert(ErrorStatus < 0);
        }

        ErrorValue = ClConvertKstatusToErrorNumber(ErrorStatus);
        RtlCopyMemory(OptionValue, &ErrorValue, *OptionLength);
    }

    return 0;
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

LIBC_API
KSTATUS
ClConvertToNetworkAddress (
    const struct sockaddr *Address,
    socklen_t AddressLength,
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

    PLIST_ENTRY CurrentEntry;
    PCL_TYPE_CONVERSION_INTERFACE Entry;
    struct sockaddr_in *Ip4SocketAddress;
    struct sockaddr_in6 *Ip6SocketAddress;
    KSTATUS Status;
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

    } else {
        Status = STATUS_INVALID_ADDRESS;
        pthread_mutex_lock(&ClTypeConversionInterfaceLock);
        CurrentEntry = ClTypeConversionInterfaceList.Next;
        while (CurrentEntry != &ClTypeConversionInterfaceList) {
            Entry = LIST_VALUE(CurrentEntry,
                                   CL_TYPE_CONVERSION_INTERFACE,
                                   ListEntry);

            CurrentEntry = CurrentEntry->Next;
            if ((Entry->Type != ClConversionNetworkAddress) ||
                (Entry->Interface.Network->Version !=
                 CL_NETWORK_CONVERSION_INTERFACE_VERSION) ||
                (Entry->Interface.Network->AddressFamily !=
                 Address->sa_family)) {

                continue;
            }

            Status = Entry->Interface.Network->ToNetworkAddress(Address,
                                                                AddressLength,
                                                                NetworkAddress);

            break;
        }

        pthread_mutex_unlock(&ClTypeConversionInterfaceLock);
        return Status;
    }

    return STATUS_SUCCESS;
}

LIBC_API
KSTATUS
ClConvertFromNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    struct sockaddr *Address,
    socklen_t *AddressLength,
    PSTR Path,
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

    Path - Supplies the path, if this is a local Unix address.

    PathSize - Supplies the size of the path, if this is a local Unix address.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the address buffer is not big enough.

    STATUS_INVALID_ADDRESS on failure.

--*/

{

    UINTN CopySize;
    PLIST_ENTRY CurrentEntry;
    PCL_TYPE_CONVERSION_INTERFACE Entry;
    struct sockaddr_in Ip4Address;
    struct sockaddr_in6 Ip6Address;
    PVOID Source;
    KSTATUS Status;
    UINTN TotalSize;
    struct sockaddr_un UnixAddress;

    if (NetworkAddress->Domain == NetDomainIp4) {
        Ip4Address.sin_family = AF_INET;
        Ip4Address.sin_port = htons((USHORT)(NetworkAddress->Port));
        Ip4Address.sin_addr.s_addr = (ULONG)(NetworkAddress->Address[0]);
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
        if (PathSize > UNIX_PATH_MAX) {
            PathSize = UNIX_PATH_MAX;
        }

        if ((Path == NULL) || (PathSize == 0)) {
            PathSize = 1;

        } else if (Path != NULL) {
            memcpy(UnixAddress.sun_path, Path, PathSize);
        }

        UnixAddress.sun_path[PathSize - 1] = '\0';
        TotalSize = FIELD_OFFSET(struct sockaddr_un, sun_path) + PathSize;
        Source = &UnixAddress;

    } else {
        Status = STATUS_INVALID_ADDRESS;
        pthread_mutex_lock(&ClTypeConversionInterfaceLock);
        CurrentEntry = ClTypeConversionInterfaceList.Next;
        while (CurrentEntry != &ClTypeConversionInterfaceList) {
            Entry = LIST_VALUE(CurrentEntry,
                                   CL_TYPE_CONVERSION_INTERFACE,
                                   ListEntry);

            CurrentEntry = CurrentEntry->Next;
            if ((Entry->Type != ClConversionNetworkAddress) ||
                (Entry->Interface.Network->Version !=
                 CL_NETWORK_CONVERSION_INTERFACE_VERSION) ||
                (Entry->Interface.Network->AddressDomain !=
                 NetworkAddress->Domain)) {

                continue;
            }

            Status = Entry->Interface.Network->FromNetworkAddress(
                                                                NetworkAddress,
                                                                Address,
                                                                AddressLength);

            break;
        }

        pthread_mutex_unlock(&ClTypeConversionInterfaceLock);
        return Status;
    }

    Status = STATUS_SUCCESS;
    CopySize = TotalSize;
    if (CopySize > *AddressLength) {
        CopySize = *AddressLength;
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    RtlCopyMemory(Address, Source, CopySize);
    *AddressLength = TotalSize;
    return Status;
}

LIBC_API
struct cmsghdr *
__cmsg_nxthdr (
    struct msghdr *Message,
    struct cmsghdr *ControlMessage
    )

/*++

Routine Description:

    This routine gets the next control message in the buffer of ancillary data.

Arguments:

    Message - Supplies a pointer to the beginning of the ancillary data.

    ControlMessage - Supplies the previous control message. This routine
        returns the next control message after this one.

Return Value:

    Returns a pointer to the control message after the given control message.

    NULL if there are no more messages or the buffer does not contain enough
    space.

--*/

{

    PUCHAR End;
    struct cmsghdr *Result;

    if (ControlMessage->cmsg_len < sizeof(struct cmsghdr)) {
        return NULL;
    }

    Result = (struct cmsghdr *)((PUCHAR)ControlMessage +
                                CMSG_ALIGN(ControlMessage->cmsg_len));

    End = (PUCHAR)(Message->msg_control) + Message->msg_controllen;
    if (((PUCHAR)(Result + 1) > End) ||
        ((PUCHAR)Result + CMSG_ALIGN(Result->cmsg_len) > End)) {

        return NULL;
    }

    return Result;
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
    KSTATUS Status;

    ASSERT((Option == SocketBasicOptionLocalAddress) ||
           (Option == SocketBasicOptionRemoteAddress));

    LocalAddress = (PNETWORK_ADDRESS)Buffer;
    LocalAddress->Domain = NetDomainInvalid;
    BufferSize = sizeof(Buffer);
    Status = OsSocketGetSetInformation((HANDLE)(UINTN)Socket,
                                       SocketInformationBasic,
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

    ASSERT(LocalAddress->Domain != NetDomainInvalid);
    ASSERT(BufferSize >= sizeof(NETWORK_ADDRESS));

    BufferSize -= sizeof(NETWORK_ADDRESS);
    Status = ClConvertFromNetworkAddress(LocalAddress,
                                         SocketAddress,
                                         AddressLength,
                                         (PSTR)(LocalAddress + 1),
                                         BufferSize);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
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

