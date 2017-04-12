/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    socket.h

Abstract:

    This header contains definitions for socket-based communication endpoints.

Author:

    Evan Green 3-May-2013

--*/

#ifndef _SOCKET_H
#define _SOCKET_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/uio.h>
#include <sys/ioctl.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro evaluates to a pointer to the ancillary data following a cmsghdr
// structure.
//

#define CMSG_DATA(_Control) \
    ((unsigned char *)((struct cmsghdr *)(_Control) + 1))

//
// This macro advances a cmsghdr pointer to the next cmsghdr, or assigns it to
// NULL if it is the last one. The first parameter is a pointer to the original
// msghdr.
//

#define CMSG_NXTHDR(_Message, _Control) __cmsg_nxthdr((_Message), (_Control))

//
// This macro evaluates to the first cmsghdr given a msghdr structure, or
// NULL if there is no data.
//

#define CMSG_FIRSTHDR(_Message)                                 \
    (((_Message)->msg_controllen >= sizeof(struct cmsghdr)) ?   \
     (struct cmsghdr *)(_Message)->msg_control :                \
     NULL)

//
// This macro returns the required alignment for a given length. This is a
// constant expression.
//

#define CMSG_ALIGN(_Length) \
    (((_Length) + sizeof(size_t) - 1) & (size_t)~(sizeof(size_t) - 1))

//
// This macro returns the number of bytes an ancillary element with the given
// payload size takes up. This is a constant expression.
//

#define CMSG_SPACE(_Length) \
    (CMSG_ALIGN(_Length) + CMSG_ALIGN(sizeof(struct cmsghdr)))

//
// This macro returns the value to store in the cmsghdr length member, taking
// into account any necessary alignment. It takes the data length as an
// argument. This is a constant expression.
//

#define CMSG_LEN(_Length) \
    (CMSG_ALIGN(sizeof(struct cmsghdr)) + (_Length))

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the valid address families.
//

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_LOCAL  AF_UNIX
#define AF_INET   2
#define AF_INET6  3
#define AF_LINK   5

//
// Define valid protocol families as the same as the address families.
//

#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_LOCAL  AF_LOCAL
#define PF_INET   AF_INET
#define PF_INET6  AF_INET6
#define PF_LINK   AF_LINK

//
// Define the socket types.
//

//
// Datagram sockets provide connectionless unreliable packets of a fixed
// maximum length.
//

#define SOCK_DGRAM     1

//
// Raw sockets snoop all traffic.
//

#define SOCK_RAW       2

//
// Sequenced packet sockets provide reliable bidirectional connection-based
// tranmission paths for records.
//

#define SOCK_SEQPACKET 3

//
// Streams provide reliable bidirectional connection-mode byte streams, and may
// provide a transmission mechanism for out of band data.
//

#define SOCK_STREAM    4

//
// Define flags to the accept4 function.
//

//
// Set this flag to request that the new descriptor be created with the
// non-blocking flag set.
//

#define SOCK_NONBLOCK 0x00001000

//
// Set this flag to request that the new descriptor be created with the
// close-on-execute flag set.
//

#define SOCK_CLOEXEC 0x00004000

//
// Define flags that can be passed to the send and recv functions.
//

//
// Peeks at an incoming message without officially receiving it. The data is
// treated as unread and the next recv or similar function call still returns
// the same data.
//

#define MSG_PEEK 0x00000001

//
// Requests out-of-band data. The significance and semantics of out-of-band
// data are protocol-specific. This flag is also returned by the kernel when
// out-of-band data is received.
//

#define MSG_OOB 0x00000002

//
// On SOCK_STREAM sockets this requests that the function block until the full
// amount of data can be returned. The function may return the smaller amount
// of data if the socket is a message-based socket, if a signal is caught, if
// the connection is terminated, is MSG_PEEK was specified, or if an error is
// pending for the socket.
//

#define MSG_WAITALL 0x00000004

//
// This flag indicates a complete message, used by sequential packet sockets.
// This flag can be set by user-mode on transmit and kernel-mode on receive.
//

#define MSG_EOR 0x00000008

//
// This flag is returned by the kernel when the trailing portion of the
// datagram was discarded because the datagram was larger than the buffer
// supplied.
//

#define MSG_TRUNC 0x00000010

//
// This flag is returned by the kernel when some control/ancillary data is
// discarded due to lack of space in the provided ancillary buffer.
//

#define MSG_CTRUNC 0x00000020

//
// This flag requests not to send a broken pipe signal on stream oriented
// sockets when the other end breaks the connection. The broken pipe status
// is still returned.
//

#define MSG_NOSIGNAL 0x00000040

//
// This flag requests that the operation not block.
//

#define MSG_DONTWAIT 0x00000080

//
// This flag requests that routing tables not be used when sending a packet.
// This limits the system to sending the packet across networks that are
// directly connected.
//

#define MSG_DONTROUTE 0x00000100

//
// Define the shutdown types. Read closes the socket for further reading, write
// closes the socket for further writing, and rdwr closes the socket for both
// reading and writing.
//

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

//
// Define socket level control message types, currently only used by local
// sockets.
//

//
// This control message type allows the passing of file descriptors.
//

#define SCM_RIGHTS 1

//
// This control message type allows the passing of credentials.
//

#define SCM_CREDENTIALS 2

//
// Define socket options.
//

//
// This options reports whether or not socket listening is enabled. The option
// value is an int boolean and is read only.
//

#define SO_ACCEPTCONN 1

//
// This option permits the sending of broadcast messages, if supported by the
// protocol. The option value is an int boolean.
//

#define SO_BROADCAST 2

//
// This option turns on recording of debugging information in the protocol. The
// option value is an int boolean.
//

#define SO_DEBUG 3

//
// This option requests that outgoing messages bypass the standard routing
// facilities. The destination shall assumed to be directly connected, and
// messages are directed to the appropriate network interface based on the
// destination address. The affect, if any, depends on what protocl is in use.
// This option takes an int boolean value.
//

#define SO_DONTROUTE 4

//
// This option reports information about the error status and clears it. The
// option value is an int.
//

#define SO_ERROR 5

//
// This option keeps connections active by enabling the periodic transmission
// of messages, if supported by the protocol. This option takes an int boolean.
// If the connected socket fails to respond to these messages, the connection
// is broken and threads writing to the socket are notified with a SIGPIPE
// signal.
//

#define SO_KEEPALIVE 6

//
// This option lingers on a close function if data is present. This option
// controls the action taken when unsent messages queue on a socket and close
// is performed. If this option is set, the system blocks the calling thread
// during close until it can transmit the data or until the time expires. If
// this option is not set and close is called, the system handles the call in
// a way that allows the calling thread to continue as quickly as possible. The
// option takes a linger structure to specify the state of the option and
// linger interval.
//

#define SO_LINGER 7

//
// This option leaves out-of-band data (data marked urgent) inline. The option
// value is an integer boolean.
//

#define SO_OOBINLINE 8

//
// This option sets the receive buffer size. It taks an int value.
//

#define SO_RCVBUF 9

//
// This option sets the minimum number of bytes to process for socket input
// operations. The default value is 1 byte. If this is set to a larger value,
// blocking receive calls normally wait until they have received the smaller of
// the low water mark or the requested amount. They may return less than the
// low water mark if an error or signal occurs. This option takes an int value.
//

#define SO_RCVLOWAT 10

//
// This option sets the maximum amount of time an input function waits until it
// completes. The value is a timeval structure specifying how long to wait
// before returning with whatever data was collected, if any. The default value
// is zero, meaning the receive operation does not time out.
//

#define SO_RCVTIMEO 11

//
// This option sets the send buffer size. It takes an int value.
//

#define SO_SNDBUF 12

//
// This option sets the minimum number of bytes to process for socket output
// operations. Non-blocking output operations shall process no data if flow
// control does not allow the smaller of the send low water mark value or the
// entire request to be processed. This option takes an int value.
//

#define SO_SNDLOWAT 13

//
// This option sets maximum amount of time an output function would block
// because flow control is preventing data from being sent. If a send operation
// has blocked for this time, it shall return with a partial count or 0 if no
// data was sent. The default value is 0, indicating that send operations do
// not time out. This option takes a timeval structure.
//

#define SO_SNDTIMEO 14

//
// This option reports the socket type. The value is an int.
//

#define SO_TYPE 15

//
// Despite its name, when enabled, this option allows the socket to bind to
// the same local port as an existing socket as long as one of them is bound to
// the any address and the other is bound to a different local address (i.e.
// they cannot both be bound to the any address). Additionally, this option
// allows the socket to bind to the exact same local address and port as an
// existing socket if the existing socket is in the time-wait state. Both
// sockets must have this option set for it to take effect. This option takes
// an int boolean.
//

#define SO_REUSEADDR 16

//
// This option allows a socket to bind to the exact same local address and
// port as an existing socket. Both sockets must have the option set for it to
// take effect. This option takes an int boolean.
//

#define SO_REUSEPORT 17

//
// This option determines whether or not to send and receive credentials
// automatically in the control data. This only applies to local sockets.
//

#define SO_PASSCRED 18

//
// This option returns the credentials of the foreign socket at the time of
// connect. This only applies to local sockets. The argument is a pointer to
// a ucred structure.
//

#define SO_PEERCRED 19

//
// Define the level number for the get/setsockopts function to apply to the
// socket itself.
//

#define SOL_SOCKET 0xFFFF

//
// Define socket ioctl numbers.
//

//
// This ioctl returns a non-zero integer if the inbound data stream is at the
// urgent mark. If the SO_OOBINLINE option is not and SIOCATMARK returns true,
// then the next read from the socket will return the bytes following the
// urgent data. Note that a read never reads across the urgent mark.
//

#define SIOCATMARK 0x7300

//
// This ioctl returns the amount of unread data in the receive buffer for
// stream sockets.
//

#define SIOCINQ FIONREAD

//
// Define the maximum length of the connection backlog queue for listen calls
// before the system stats refusing connection requests.
//

#define SOMAXCONN 512

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the unsigned integer type used for the sockaddr family type.
//

typedef unsigned int sa_family_t;

//
// Define the type used for passing the length of a socket.
//

typedef unsigned long socklen_t;

/*++

Structure Description:

    This structure defines a socket address.

Members:

    sa_family - Stores the socket address family. See AF_* definitions.

    sa_data - Stores the address data information, which may or may not use
        all of the bytes available in this member.

--*/

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[28];
};

/*++

Structure Description:

    This structure defines a socket address storage structure, which is
    guaranteed to be as big as the biggest type of sockaddr.

Members:

    sa_family - Stores the socket address family. See AF_* definitions.

    sa_data - Stores the address data information, which may or may not use
        all of the bytes available in this member.

--*/

struct sockaddr_storage {
    sa_family_t ss_family;
    char ss_data[28];
};

/*++

Structure Description:

    This structure defines the linger state for a socket.

Members:

    l_onoff - Stores a boolean value indicating whether or not lingering is
        enabled on socket close.

    l_linger - Stores the time, in seconds, that the socket is set to linger
        on close.

--*/

struct linger {
    int l_onoff;
    int l_linger;
};

/*++

Structure Description:

    This structure defines a socket message.

Members:

    msg_name - Stores a pointer to the socket address to send to or receive
        from.

    msg_namelen - Stores the size of the name buffer in bytes.

    msg_iov - Stores a pointer to an array of I/O vectors to do the I/O to or
        from.

    msg_iovlen - Stores the number of elements in the I/O vector.

    msg_control - Stores an optional pointer to the ancillary data.

    msg_controllen - Stores the length of the ancillary data in bytes on input.
        On output, this value is adjusted to indicate the actual amount of
        data.

    msg_flags - Stores a bitmask of message flags. See MSG_* for definitions.

--*/

struct msghdr {
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    size_t msg_iovlen;
    void *msg_control;
    socklen_t msg_controllen;
    int msg_flags;
};

/*++

Structure Description:

    This structure defines a socket control message, the header for the socket
    ancillary data.

Members:

    cmsg_len - Stores the length of the data for this message, including this
        structure.

    cmsg_level - Stores the originating protocol of the control message.

    cmsg_type - Stores the control message type.

--*/

struct cmsghdr {
    socklen_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
};

/*++

Structure Description:

    This structure defines the user credential structure used when passing a
    SCM_CREDENTIALS ancillary message. These credentials are checked and
    validated by the kernel on the sending side unless the sender has the
    system administrator permission.

Members:

    pid - Stores the ID of the process that sent the message.

    uid - Stores the user ID of the process that sent the message.

    gid - Stores the group ID of the process that sent the message.

--*/

struct ucred {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
socketpair (
    int Domain,
    int Type,
    int Protocol,
    int Sockets[2]
    );

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

LIBC_API
int
socket (
    int Domain,
    int Type,
    int Protocol
    );

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

LIBC_API
int
bind (
    int Socket,
    const struct sockaddr *Address,
    socklen_t AddressLength
    );

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

LIBC_API
int
listen (
    int Socket,
    int Backlog
    );

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

LIBC_API
int
accept (
    int Socket,
    struct sockaddr *Address,
    socklen_t *AddressLength
    );

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

LIBC_API
int
accept4 (
    int Socket,
    struct sockaddr *Address,
    socklen_t *AddressLength,
    int Flags
    );

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

LIBC_API
int
connect (
    int Socket,
    const struct sockaddr *Address,
    socklen_t AddressLength
    );

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

LIBC_API
ssize_t
send (
    int Socket,
    const void *Data,
    size_t Length,
    int Flags
    );

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

LIBC_API
ssize_t
sendto (
    int Socket,
    const void *Data,
    size_t Length,
    int Flags,
    const struct sockaddr *DestinationAddress,
    socklen_t DestinationAddressLength
    );

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

LIBC_API
ssize_t
sendmsg (
    int Socket,
    const struct msghdr *Message,
    int Flags
    );

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

LIBC_API
ssize_t
recv (
    int Socket,
    void *Buffer,
    size_t Length,
    int Flags
    );

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

LIBC_API
ssize_t
recvfrom (
    int Socket,
    void *Buffer,
    size_t Length,
    int Flags,
    struct sockaddr *SourceAddress,
    socklen_t *SourceAddressLength
    );

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

LIBC_API
ssize_t
recvmsg (
    int Socket,
    struct msghdr *Message,
    int Flags
    );

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

LIBC_API
int
shutdown (
    int Socket,
    int How
    );

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

LIBC_API
int
setsockopt (
    int Socket,
    int Level,
    int OptionName,
    const void *OptionValue,
    socklen_t OptionLength
    );

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

LIBC_API
int
getsockopt (
    int Socket,
    int Level,
    int OptionName,
    void *OptionValue,
    socklen_t *OptionLength
    );

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

LIBC_API
int
getsockname (
    int Socket,
    struct sockaddr *SocketAddress,
    socklen_t *AddressLength
    );

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

LIBC_API
int
getpeername (
    int Socket,
    struct sockaddr *SocketAddress,
    socklen_t *AddressLength
    );

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

LIBC_API
struct cmsghdr *
__cmsg_nxthdr (
    struct msghdr *Message,
    struct cmsghdr *ControlMessage
    );

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

#ifdef __cplusplus

}

#endif
#endif

