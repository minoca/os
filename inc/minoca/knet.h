/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    knet.h

Abstract:

    This header contains the interface between the kernel and the networking
    core library.

Author:

    Evan Green 4-Apr-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros for manipulating a sequence of socket control messages.
//

//
// This macro evaluates to a pointer to the ancillary data following a cmsghdr
// structure.
//

#define SOCKET_CONTROL_DATA(_Control) \
    ((PVOID)((PSOCKET_CONTROL_MESSAGE)(_Control) + 1))

//
// This macro advances a cmsghdr pointer to the next cmsghdr, or assigns it to
// NULL if it is the last one. The first parameter is a pointer to the original
// msghdr.
//

#define SOCKET_CONTROL_NEXT(_ControlBuffer, _ControlBufferSize, _Control)      \
    if ((_Control)->Length < sizeof(SOCKET_CONTROL_MESSAGE)) {                 \
        (_Control) = NULL;                                                     \
                                                                               \
    } else {                                                                   \
        (_Control) = (PSOCKET_CONTROL_MESSAGE)((PVOID)(_Control) +             \
                                    SOCKET_CONTROL_ALIGN((_Control)->Length)); \
                                                                               \
        if (((PVOID)((_Control) + 1) >                                         \
             (PVOID)(_ControlBuffer) + (_ControlBufferSize)) ||                \
            ((PVOID)(_Control) + SOCKET_CONTROL_ALIGN((_Control)->Length) >    \
             ((PVOID)(_ControlBuffer) + (_ControlBufferSize)))) {              \
                                                                               \
            (_Control) = NULL;                                                 \
        }                                                                      \
    }

//
// This macro evaluates to the first cmsghdr given a msghdr structure, or
// NULL if there is no data.
//

#define SOCKET_CONTROL_FIRST(_ControlBuffer, _ControlBufferSize)    \
    (((_ControlBufferSize) >= sizeof(SOCKET_CONTROL_MESSAGE)) ?     \
     (PSOCKET_CONTROL_MESSAGE)(_ControlBuffer) :                    \
     NULL)

//
// This macro returns the required alignment for a given length. This is a
// constant expression.
//

#define SOCKET_CONTROL_ALIGN(_Length) ALIGN_RANGE_UP(_Length, sizeof(UINTN))

//
// This macro returns the number of bytes an ancillary element with the given
// payload size takes up. This is a constant expression.
//

#define SOCKET_CONTROL_SPACE(_Length) \
    (SOCKET_CONTROL_ALIGN(_Length) +  \
     SOCKET_CONTROL_ALIGN(sizeof(SOCKET_CONTROL_MESSAGE)))

//
// This macro returns the value to store in the cmsghdr length member, taking
// into account any necessary alignment. It takes the data length as an
// argument. This is a constant expression.
//

#define SOCKET_CONTROL_LENGTH(_Length) \
    (SOCKET_CONTROL_ALIGN(sizeof(SOCKET_CONTROL_MESSAGE)) + (_Length))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of bytes in a network address.
//

#define MAX_NETWORK_ADDRESS_SIZE 16

//
// Define socket shutdown types. These can be ORed together.
//

#define SOCKET_SHUTDOWN_READ  0x00000001
#define SOCKET_SHUTDOWN_WRITE 0x00000002

//
// Define socket I/O flags. These should match up to the C library MSG_* flags.
//

//
// Peeks at an incoming message without officially receiving it. The data is
// treated as unread and the next recv or similar function call still returns
// the same data.
//

#define SOCKET_IO_PEEK 0x00000001

//
// Requests out-of-band data. The significants and semantics of out-of-band
// data are protocol-specific.
//

#define SOCKET_IO_OUT_OF_BAND 0x00000002

//
// On SOCK_STREAM sockets this requests that the function block until the full
// amount of data can be returned. The function may return the smaller amount
// of data if the socket is a message-based socket, if a signal is caught, if
// the connection is terminated, is MSG_PEEK was specified, or if an error is
// pending for the socket.
//

#define SOCKET_IO_WAIT_ALL 0x00000004

//
// This flag indicates a complete message, used by sequential packet sockets.
// This flag can be set by user-mode on transmit and kernel-mode on receive.
//

#define SOCKET_IO_END_OF_RECORD 0x00000008

//
// This flag is returned by the kernel when the trailing portion of the
// datagram was discarded because the datagram was larger than the buffer
// supplied.
//

#define SOCKET_IO_DATA_TRUNCATED 0x00000010

//
// This flag is returned by the kernel when some control/ancillary data is
// discarded due to lack of space in the provided ancillary buffer.
//

#define SOCKET_IO_CONTROL_TRUNCATED 0x00000020

//
// This flag requests not to send a broken pipe signal on stream oriented
// sockets when the other end breaks the connection. The broken pipe status
// is still returned.
//

#define SOCKET_IO_NO_SIGNAL 0x00000040

//
// This flag is requests that the operation not block.
//

#define SOCKET_IO_NON_BLOCKING 0x00000080

//
// Define common internet protocol numbers, as defined by the IANA.
//

#define SOCKET_INTERNET_PROTOCOL_ICMP 1
#define SOCKET_INTERNET_PROTOCOL_IPV4 4
#define SOCKET_INTERNET_PROTOCOL_TCP 6
#define SOCKET_INTERNET_PROTOCOL_UDP 17
#define SOCKET_INTERNET_PROTOCOL_IPV6 41

//
// Define the socket level of control messages.
//

#define SOCKET_LEVEL_SOCKET 0xFFFF

//
// There is no raw protocol value, so give it a wildcard value of negative one.
//

#define SOCKET_INTERNET_PROTOCOL_RAW -1

//
// Define socket level control message types, currently only used by local
// sockets. These must match up with the C library SCM_* definitions.
//

//
// This control message type allows the passing of file descriptors.
//

#define SOCKET_CONTROL_RIGHTS 1

//
// This control message type allows the passing of credentials.
//

#define SOCKET_CONTROL_CREDENTIALS 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SOCKET_TYPE {
    SocketTypeInvalid,
    SocketTypeDatagram,
    SocketTypeRaw,
    SocketTypeSequencedPacket,
    SocketTypeStream
} SOCKET_TYPE, *PSOCKET_TYPE;

typedef enum _SOCKET_PROTOCOL {
    SocketProtocolInvalid,
    SocketProtocolTcp,
    SocketProtocolUdp,
    SocketProtocolRaw
} SOCKET_PROTOCOL, *PSOCKET_PROTOCOL;

typedef enum _SOCKET_NETWORK {
    SocketNetworkInvalid,
    SocketNetworkLocal,
    SocketNetworkPhysical,
    SocketNetworkArp,
    SocketNetworkIp4,
    SocketNetworkIp6
} SOCKET_NETWORK, *PSOCKET_NETWORK;

/*++

Structure Description:

    This structure defines a generic network address.

Members:

    Network - Stores the network type of this address.

    Port - Stores the port number, which may or may not be relevant depending
        on the protocol and network layers. This number is in host order.

    Address - Stores the network-specific addressing information. The address
        is in network order.

--*/

typedef struct _NETWORK_ADDRESS {
    SOCKET_NETWORK Network;
    ULONG Port;
    UINTN Address[MAX_NETWORK_ADDRESS_SIZE / sizeof(UINTN)];
} NETWORK_ADDRESS, *PNETWORK_ADDRESS;

/*++

Enumeration Description:

    This enumeration describes the various types of socket information that
    can be requested or modified. It dictates the meaning of the socket option
    parameter supplied when getting or setting socket information.

Values:

    SocketInformationTypeInvalid - Indicates an invalid socket information type.

    SocketInformationTypeBasic - Indicates that the socket option should be
        interpreted as a basic socket option.

    SocketInformationTypeIp4 - Indicates that the socket option should be
        interpreted as an IPv4 socket option.

    SocketInformationTypeIp6 - Indicates that the socket option should be
        interpreted as an IPv6 socket option.

    SocketInformationTypeTcp - Indicates that the socket option should be
        interpreted as a TCP socket option.

    SocketInformationTypeUdp - Indicates that the socket option should be
        interpreted as an UDP socket option.

    SocketInformationTypeCount - Indicates the number of socket information
        types.

--*/

typedef enum _SOCKET_INFORMATION_TYPE {
    SocketInformationTypeInvalid,
    SocketInformationTypeBasic,
    SocketInformationTypeIp4,
    SocketInformationTypeIp6,
    SocketInformationTypeTcp,
    SocketInformationTypeUdp,
    SocketInformationTypeRaw,
    SocketInformationTypeCount
} SOCKET_INFORMATION_TYPE, *PSOCKET_INFORMATION_TYPE;

/*++

Enumeration Description:

    This enumeration describes the various socket options for the basic socket
    information class.

Values:

    SocketBasicOptionInvalid - Indicates an invalid basic socket option.

    SocketBasicOptionDebug - Indicates that debugging should be enabled or
        disabled for the socket, or that the current debug state should be
        retrieved. This option takes a boolean.

    SocketBasicOptionAcceptConnections - Indicates that the listening state of
        the socket should be retrieved. This option is read only and takes a
        boolean.

    SocketBasicOptionBroadcastEnabled - Indicates that the sending of broadcast
        packets should be enabled or disabled, or that the current state of the
        ability to send broadcast packets should be retrieved. This option
        takes a boolean.

    SocketBasicOptionReuseAnyAddress - Indicates that the reuse of the any
        address should be allowed on the same port as previously bound
        address (or vice versa) should be enabled or disabled, or that the
        state of reusing the any address should be retrieved. Both sockets
        must have this option set for any address reuse to be allowed on a bind
        operation. This option takes a boolean.

    SocketBasicOptionReuseTimeWait - Indicates that the reuse of an exact local
        address is allowed on a bind operation as long as the existing socket
        is in the time wait state. This option takes a boolean.

    SocketBasicOptionReuseExactAddress - Indicates that the reuse of an exact
        local address is allowed on a bind operation. Both sockets must have
        this option enabled. This option takes a boolean.

    SocketBasicOptionKeepAlive - Indicates that the performance of periodic
        connection checks should be enabled or disabled, or that the state of
        the use of such checks should be retrieved. This option takes a boolean.

    SocketBasicOptionLinger - Indicates that the socket's linger state should
        be modified or retrieved. This option takes a SOCKET_LINGER structure.
        If disabled, a connected socket will return immediately from a close
        operation and attempt to gracefully shut down the connection. If
        enabled without a timeout, a connected socket will abort the connection
        on a close option. If enabled with a timeout, the close operation will
        not return until all data has been sent and a graceful shutdown is
        complete or until the timer has expired, at which point the connection
        will be aborted.

    SocketBasicOptionInlineOutOfBand - Indicates that the inclusion of urgent
        data in the mainline packet processing should be enabled or disabled,
        or retrieves the current state of urgent packet processing. This option
        takes a boolean.

    SocketBasicOptionRoutingDisabled - Indicates that the default routing
        process for packets should be enabled or disabled, or retrieves whether
        or not default routing is disabled. This option takes a boolean.

    SocketBasicOptionSendBufferSize - Indicates the size of the socket's send
        buffer to set, in bytes, or retrieves the current size of the socket's
        send buffer, in bytes. This option takes a UINTN.

    SocketBasicOptionSendMinimum - Indicates the minimum amount of data, in
        bytes, that needs to be sent before the socket will actually transmit
        packets. This option takes a UINTN.

    SocketBasicOptionSendTimeout - Indicates the maximum amount of time, in
        milliseconds, that a send operation should wait to send data if it is
        blocked by flow control. This option takes a ULONG.

    SocketBasicOptionReceiveBufferSize - Indicates the size of the socket's
        receive bufffer to set, in bytes, or retrieves the current size of the
        socket's receive buffer. This option takes a UINTN.

    SocketBasicOptionReceiveMinimum - Indicates the minimum amount of data, in
        bytes, that needs to be received before the system will alert any
        readers that may be waiting on poll or receive operations. This option
        takes a UINTN.

    SocketBasicOptionReceiveTimeout - Indicates the maximum amount of time, in
        milliseconds, that a receive operation should wait for more data before
        completing. This option takes a ULONG.

    SocketBasicOptionErrorStatus - Indicates that the socket's error status
        should be retrieved and cleared. This option is read only and takes a
        UINTN.

    SocketBasicOptionProtocol - Indicates that the socket's protocol should be
        retrieved. This option is read only and takes a SOCKET_PROTOCOL
        structure.

    SocketBasicOptionNetwork - Indicates that the socket's network should be
        retrieved. This option is read only and takes a SOCKET_NETWORK
        structure.

    SocketBasicOptionLocalAddress - Indicates that the socket's local address
        should be retrieved. This option is read only and takes a
        NETWORK_ADDRESS structure.

    SocketBasicOptionRemoteAddress - Indicates that the socket's remote address
        should be retrieved. This option is read only and takes a
        NETWORK_ADDRESS structure.

    SocketBasicOptionPassCredentials - Indicates that credentials should be
        sent and received automatically with messages on the socket. This is
        only applicable for local sockets.

    SocketBasicOptionPeerCredentials - Indicates the credentials of the
        foreign socket at the time of connect. This is only applicable for
        local sockets.

    SocketBasicOptionCount - Indicates the number of basic socket options.

--*/

typedef enum _SOCKET_BASIC_OPTION {
    SocketBasicOptionInvalid,
    SocketBasicOptionDebug,
    SocketBasicOptionAcceptConnections,
    SocketBasicOptionBroadcastEnabled,
    SocketBasicOptionReuseAnyAddress,
    SocketBasicOptionReuseTimeWait,
    SocketBasicOptionReuseExactAddress,
    SocketBasicOptionKeepAlive,
    SocketBasicOptionLinger,
    SocketBasicOptionInlineOutOfBand,
    SocketBasicOptionRoutingDisabled,
    SocketBasicOptionSendBufferSize,
    SocketBasicOptionSendMinimum,
    SocketBasicOptionSendTimeout,
    SocketBasicOptionReceiveBufferSize,
    SocketBasicOptionReceiveMinimum,
    SocketBasicOptionReceiveTimeout,
    SocketBasicOptionErrorStatus,
    SocketBasicOptionType,
    SocketBasicOptionNetwork,
    SocketBasicOptionLocalAddress,
    SocketBasicOptionRemoteAddress,
    SocketBasicOptionPassCredentials,
    SocketBasicOptionPeerCredentials,
    SocketBasicOptionCount
} SOCKET_BASIC_OPTION, *PSOCKET_BASIC_OPTION;

/*++

Structure Description:

    This structure defines the set of socket linger information.

Members:

    LingerEnabled - Stores a boolean indicating whether or not lingering is
        enabled on the socket.

    LingerTimeout - Stores the amount of time, in milliseconds, the socket will
        wait for data to be sent before forcefully closing.

--*/

typedef struct _SOCKET_LINGER {
    BOOL LingerEnabled;
    ULONG LingerTimeout;
} SOCKET_LINGER, *PSOCKET_LINGER;

/*++

Enumeration Description:

    This enumeration describes the various IPv4 options for the IPv4 socket
    information class.

Values:

    SocketIp4OptionInvalid - Indicates an invalid IPv4 socket option.

    SocketIp4OptionHeaderIncluded - Indicates that packets supplied to the send
        call for this socket include an IPv4 header. This options takes a
        boolean.

    SocketIp4OptionCount - Indicates the number of IPv4 socket options.

--*/

typedef enum _SOCKET_IP4_OPTION {
    SocketIp4OptionInvalid,
    SocketIp4OptionHeaderIncluded,
    SocketIp4OptionCount
} SOCKET_IP4_OPTION, *PSOCKET_IP4_OPTION;

/*++

Enumeration Description:

    This enumeration describes the various IPv6 options for the IPv6 socket
    information class.

Values:

    SocketIp6OptionInvalid - Indicates an invalid IPv6 socket option.

    SocketIp6OptionCount - Indicates the number of IPv6 socket options.

--*/

typedef enum _SOCKET_IP6_OPTION {
    SocketIp6OptionInvalid,
    SocketIp6OptionCount
} SOCKET_IP6_OPTION, *PSOCKET_IP6_OPTION;

/*++

Enumeration Description:

    This enumeration describes the various TCP options for the TCP socket
    information class.

Values:

    SocketTcpOptionInvalid - Indicates an invalid TCP socket option.

    SocketTcpOptionNoDelay - Indicates whether outgoing data is sent
        immediately or batched together (the default).

    SocketTcpOptionKeepAliveTimeout - Indicates the time, in seconds, until the
        first keep alive probe is sent after the TCP connection goes idle. This
        option takes an ULONG.

    SocketTcpOptionKeepAlivePeriod - Indicates the the time, in seconds,
        between keep alive probes. This option takes a ULONG.

    SocketTcpOptionKeepAliveProbeLimit - Indicates the number of TCP keep alive
        probes to be sent, without response, before the connection is aborted.
        This option takes a ULONG.

    SocketTcpOptionCount - Indicates the number of TCP socket options.

--*/

typedef enum _SOCKET_TCP_OPTION {
    SocketTcpOptionInvalid,
    SocketTcpOptionNoDelay,
    SocketTcpOptionKeepAliveTimeout,
    SocketTcpOptionKeepAlivePeriod,
    SocketTcpOptionKeepAliveProbeLimit,
    SocketTcpOptionCount
} SOCKET_TCP_OPTION, *PSOCKET_TCP_OPTION;

/*++

Enumeration Description:

    This enumeration describes the various UDP options for the UDP socket
    information class.

Values:

    SocketUdpOptionInvalid - Indicates an invalid UDP socket option.

    SocketUdpOptionCount - Indicates the number of UDP socket options.

--*/

typedef enum _SOCKET_UDP_OPTION {
    SocketUdpOptionInvalid,
    SocketUdpOptionCount
} SOCKET_UDP_OPTION, *PSOCKET_UDP_OPTION;

/*++

Enumeration Description:

    This enumeration describes the various socket options for the raw socket
    information class.

Values:

    SocketRawOptionInvalid - Indicates an invalid raw socket option.

    SocketRawOptionCount - Indicates the number of raw socket options.

--*/

typedef enum _SOCKET_RAW_OPTION {
    SocketRawOptionInvalid,
    SocketRawOptionCount
} SOCKET_RAW_OPTION, *PSOCKET_RAW_OPTION;

/*++

Structure Description:

    This structure defines the common portion of a socket that must be at the
    beginning of every socket structure. depending on the type of socket, there
    may be more fields in this structure (ie this structure is only the first
    member in a larger socket structure).

Members:

    Network - Stores the network type of this socket.

    Type - Stores the socket type.

    Protocol - Stores the raw protocol value of this socket that is used
        on the network.

    ReferenceCount - Stores the reference count on the socket.

    IoState - Stores a pointer to the I/O object state for this socket. If the
        networking driver allocates this on socket creation, the kernel will
        take ownership of the structure upon return from create. The driver
        should never destroy it.

    IoHandle - Stores a pointer to the I/O handle that goes along with this
        socket.

--*/

typedef struct _SOCKET {
    SOCKET_NETWORK Network;
    SOCKET_TYPE Type;
    ULONG Protocol;
    UINTN ReferenceCount;
    PIO_OBJECT_STATE IoState;
    PIO_HANDLE IoHandle;
} SOCKET, *PSOCKET;

/*++

Structure Description:

    This structure defines parameters associated with a socket I/O request.

Members:

    Size - Stores the size in bytes of the I/O request. Upon return, the
        number of bytes successfully sent or received will be returned here.

    IoFlags - Stores the standard I/O flags. See IO_FLAG_* definitions for
        kernel mode or SYS_IO_FLAG_* definitions for user mode.

    SocketIoFlags - Stores a set of socket-specific I/O flags. See SOCKET_IO_*
        definitions.

    TimeoutInMilliseconds - Stores the timeout in milliseconds before the
        operation returns with what it has.

    NetworkAddress - Stores an optional pointer to a remote network address.

    RemotePath - Stores an optional pointer to a socket file path for local
        sockets.

    RemotePathSize - Stores the size of the remote path buffer in bytes. On
        return, will contain the actual size of the remote path, including
        the null terminator.

    ControlData - Stores an optional pointer to the ancillary data associated
        with this request.

    ControlDataSize - Stores the size of the control data buffer in bytes. On
        return, returns the actual size of the control data.

--*/

typedef struct _SOCKET_IO_PARAMETERS {
    UINTN Size;
    ULONG IoFlags;
    ULONG SocketIoFlags;
    ULONG TimeoutInMilliseconds;
    PNETWORK_ADDRESS NetworkAddress;
    PSTR RemotePath;
    UINTN RemotePathSize;
    PVOID ControlData;
    UINTN ControlDataSize;
} SOCKET_IO_PARAMETERS, *PSOCKET_IO_PARAMETERS;

/*++

Structure Description:

    This structure defines a socket control message, the header for the socket
    ancillary data. This structure lines up exactly with the C library cmsghdr
    structure.

Members:

    Length - Stores the length of the data for this message, including this
        structure.

    Protocol- Stores the originating protocol of the control message.

    Type - Stores the control message type.

--*/

typedef struct SOCKET_CONTROL_MESSAGE {
    UINTN Length;
    ULONG Protocol;
    ULONG Type;
} SOCKET_CONTROL_MESSAGE, *PSOCKET_CONTROL_MESSAGE;

typedef
KSTATUS
(*PNET_CREATE_SOCKET) (
    SOCKET_NETWORK Network,
    SOCKET_TYPE Type,
    ULONG Protocol,
    PSOCKET *NewSocket
    );

/*++

Routine Description:

    This routine allocates resources associated with a new socket. The core
    networking driver is responsible for allocating the structure (with
    additional length for any of its context). The kernel will fill in the
    common header when this routine returns.

Arguments:

    Network - Supplies the network to use on the socket.

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

typedef
VOID
(*PNET_DESTROY_SOCKET) (
    PSOCKET Socket
    );

/*++

Routine Description:

    This routine destroys resources associated with an open socket, officially
    marking the end of the kernel's knowledge of this structure.

Arguments:

    Socket - Supplies a pointer to the socket to destroy. The kernel will have
        already destroyed any resources inside the common header, the core
        networking library should not reach through any pointers inside the
        socket header.

Return Value:

    None. This routine is responsible for freeing the memory associated with
    the socket structure itself.

--*/

typedef
KSTATUS
(*PNET_BIND_TO_ADDRESS) (
    PSOCKET Socket,
    PVOID Link,
    PNETWORK_ADDRESS Address
    );

/*++

Routine Description:

    This routine binds the given socket to the specified network address.

Arguments:

    Socket - Supplies a pointer to the socket to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET_LISTEN) (
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

typedef
KSTATUS
(*PNET_ACCEPT) (
    PSOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
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

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET_CONNECT) (
    PSOCKET Socket,
    PNETWORK_ADDRESS Address
    );

/*++

Routine Description:

    This routine attempts to make an outgoing connection to a server.

Arguments:

    Socket - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the address to connect to.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET_CLOSE_SOCKET) (
    PSOCKET Socket
    );

/*++

Routine Description:

    This routine closes a socket connection.

Arguments:

    Socket - Supplies a pointer to the socket to shut down.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET_SEND_DATA) (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine sends the given data buffer through the network.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request is
        coming from kernel mode (TRUE) or user mode (FALSE).

    Socket - Supplies a pointer to the socket to send the data to.

    Parameters - Supplies a pointer to the socket I/O parameters. This will
        always be a kernel mode pointer.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        send.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET_RECEIVE_DATA) (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine is called by the user to receive data from the socket.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request is
        coming from kernel mode (TRUE) or user mode (FALSE).

    Socket - Supplies a pointer to the socket to receive data from.

    Parameters - Supplies a pointer to the socket I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer where the received data
        will be returned.

Return Value:

    STATUS_SUCCESS if any bytes were read.

    STATUS_TIMEOUT if the request timed out.

    STATUS_BUFFER_TOO_SMALL if the incoming datagram was too large for the
        buffer. The remainder of the datagram is discarded in this case.

    Other error codes on other failures.

--*/

typedef
KSTATUS
(*PNET_GET_SET_SOCKET_INFORMATION) (
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

typedef
KSTATUS
(*PNET_SHUTDOWN) (
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

typedef
KSTATUS
(*PNET_USER_CONTROL) (
    PSOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

/*++

Routine Description:

    This routine handles user control requests destined for a socket.

Arguments:

    Socket - Supplies a pointer to the socket.

    CodeNumber - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines interface between the kernel and the core networking
    library. More specifically, it defines the set of functions that the
    kernel will call when it needs networking support.

Members:

    CreateSocket - Stores a pointer to a function that creates a new socket.

    DestroySocket - Stores a pointer to a function that destroys all resources
        associated with a socket.

    BindToAddress - Stores a pointer to a function that binds a network
        address to the socket.

    Listen - Stores a pointer to a function that starts a bound socket listening
        for incoming connections.

    Accept - Stores a pointer to a function that accepts an incoming connection
        request from a remote host.

    Connect - Stores a pointer to a function that attempts to create an
        outgoing connection.

    CloseSocket - Stores a pointer to a function that closes a socket and
        destroys all resources associated with it.

    Send - Stores a pointer to a function used to send data into a socket.

    Receive - Stores a pointer to a function used to receive data from a socket.

    GetSetInformation - Stores a pointer to a function used to get or set
        socket information.

    Shutdown - Stores a pointer to a function used to shut down communication
        with a socket.

    UserControl - Stores a pointer to a function used to support ioctls to
        sockets.

--*/

typedef struct _NET_INTERFACE {
    PNET_CREATE_SOCKET CreateSocket;
    PNET_DESTROY_SOCKET DestroySocket;
    PNET_BIND_TO_ADDRESS BindToAddress;
    PNET_LISTEN Listen;
    PNET_ACCEPT Accept;
    PNET_CONNECT Connect;
    PNET_CLOSE_SOCKET CloseSocket;
    PNET_SEND_DATA Send;
    PNET_RECEIVE_DATA Receive;
    PNET_GET_SET_SOCKET_INFORMATION GetSetSocketInformation;
    PNET_SHUTDOWN Shutdown;
    PNET_USER_CONTROL UserControl;
} NET_INTERFACE, *PNET_INTERFACE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
VOID
IoInitializeCoreNetworking (
    PNET_INTERFACE Interface
    );

/*++

Routine Description:

    This routine initializes the interface between the kernel and the core
    networking library. This routine should not be called by random drivers.

Arguments:

    Interface - Supplies a pointer to the core networking library interface.

Return Value:

    None.

--*/

KERNEL_API
ULONG
IoSocketAddReference (
    PSOCKET Socket
    );

/*++

Routine Description:

    This routine increases the reference count on a socket.

Arguments:

    Socket - Supplies a pointer to the socket whose reference count should be
        incremented.

Return Value:

    Returns the old reference count.

--*/

KERNEL_API
ULONG
IoSocketReleaseReference (
    PSOCKET Socket
    );

/*++

Routine Description:

    This routine decreases the reference count of a socket, and destroys the
    socket if in this call the reference count drops to zero.

Arguments:

    Socket - Supplies a pointer to the socket whose reference count should be
        decremented.

Return Value:

    Returns the old reference count.

--*/

KERNEL_API
KSTATUS
IoSocketCreatePair (
    SOCKET_NETWORK Network,
    SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    PIO_HANDLE IoHandles[2]
    );

/*++

Routine Description:

    This routine creates a pair of sockets that are connected to each other.

Arguments:

    Network - Supplies the network to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value used on the network.

    OpenFlags - Supplies a bitfield of open flags governing the new handles.
        See OPEN_FLAG_* definitions.

    IoHandles - Supplies an array where the two I/O handles to the connected
        sockets will be returned on success.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSocketCreate (
    SOCKET_NETWORK Network,
    SOCKET_TYPE Type,
    ULONG Protocol,
    ULONG OpenFlags,
    PIO_HANDLE *IoHandle
    );

/*++

Routine Description:

    This routine allocates resources associated with a new socket.

Arguments:

    Network - Supplies the network to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value used on the network.

    OpenFlags - Supplies the open flags for the socket. See OPEN_FLAG_*
        definitions.

    IoHandle - Supplies a pointer where a pointer to the new socket's I/O
        handle will be returned.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSocketBindToAddress (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PVOID Link,
    PNETWORK_ADDRESS Address,
    PSTR Path,
    UINTN PathSize
    );

/*++

Routine Description:

    This routine binds the socket to the given address and starts listening for
    client requests.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Handle - Supplies a pointer to the socket handle to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

    Path - Supplies an optional pointer to a path, required if the network
        address is a local socket.

    PathSize - Supplies the size of the path in bytes including the null
        terminator.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSocketListen (
    PIO_HANDLE Handle,
    ULONG BacklogCount
    );

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing sockets to attempt to connect to it.

Arguments:

    Handle - Supplies a pointer to the socket to mark as listening.

    BacklogCount - Supplies the number of attempted connections that can be
        queued before additional connections are refused.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSocketAccept (
    PIO_HANDLE Handle,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress,
    PSTR *RemotePath,
    PUINTN RemotePathSize
    );

/*++

Routine Description:

    This routine accepts an incoming connection on a listening connection-based
    socket.

Arguments:

    Handle - Supplies a pointer to the socket to accept a connection from.

    NewConnectionSocket - Supplies a pointer where a new socket will be
        returned that represents the accepted connection with the remote
        host.

    RemoteAddress - Supplies a pointer where the address of the connected
        remote host will be returned.

    RemotePath - Supplies a pointer where a string containing the remote path
        will be returned on success. The caller does not own this string, it is
        connected with the new socket coming out. This only applies to local
        sockets.

    RemotePathSize - Supplies a pointer where the size of the remote path in
        bytes will be returned on success.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSocketConnect (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PNETWORK_ADDRESS Address,
    PSTR RemotePath,
    UINTN RemotePathSize
    );

/*++

Routine Description:

    This routine attempts to make an outgoing connection to a server.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode.

    Handle - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the address to connect to.

    RemotePath - Supplies a pointer to the path to connect to, if this is a
        local socket.

    RemotePathSize - Supplies the size of the remote path buffer in bytes,
        including the null terminator.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSocketSendData (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

/*++

Routine Description:

    This routine sends the given data buffer through the network.

Arguments:

    FromKernelMode - Supplies a boolean indicating if the request is coming
        from kernel mode or user mode. This value affects the root path node
        to traverse for local domain sockets.

    Handle - Supplies a pointer to the socket to send the data to.

    Parameters - Supplies a pointer to the socket I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        send.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoSocketReceiveData (
    BOOL FromKernelMode,
    PIO_HANDLE Handle,
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

    Handle - Supplies a pointer to the socket to receive data from.

    Parameters - Supplies a pointer to the socket I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer where the received data
        will be returned.

Return Value:

    STATUS_SUCCESS if any bytes were read.

    STATUS_TIMEOUT if the request timed out.

    STATUS_BUFFER_TOO_SMALL if the incoming datagram was too large for the
        buffer. The remainder of the datagram is discarded in this case.

    Other error codes on other failures.

--*/

KERNEL_API
KSTATUS
IoSocketGetSetInformation (
    PIO_HANDLE IoHandle,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets information about the given socket.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle of the socket.

    InformationType - Supplies the socket information type category to which
        specified option belongs.

    SocketOption - Supplies the option to get or set, which is specific to the
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

    STATUS_INVALID_PARAMETER if the data is not appropriate for the socket
        option.

    STATUS_BUFFER_TOO_SMALL if the socket option information does not fit in
        the supplied buffer.

    STATUS_NOT_SUPPORTED_BY_PROTOCOL if the socket option or information type
        is not supported by the socket.

    STATUS_NOT_A_SOCKET if the given handle wasn't a socket.

--*/

KERNEL_API
KSTATUS
IoSocketShutdown (
    PIO_HANDLE IoHandle,
    ULONG ShutdownType
    );

/*++

Routine Description:

    This routine shuts down communication with a given socket.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle of the socket.

    ShutdownType - Supplies the shutdown type to perform. See the
        SOCKET_SHUTDOWN_* definitions.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_A_SOCKET if the given handle wasn't a socket.

    Other error codes on failure.

--*/

KERNEL_API
KSTATUS
IoSocketUserControl (
    PIO_HANDLE Handle,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

/*++

Routine Description:

    This routine handles user control requests destined for a socket.

Arguments:

    Handle - Supplies the open file handle.

    CodeNumber - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

KERNEL_API
KSTATUS
IoGetSocketFromHandle (
    PIO_HANDLE IoHandle,
    PSOCKET *Socket
    );

/*++

Routine Description:

    This routine returns the socket structure from inside an I/O handle. This
    routine is usually only used by networking protocol to get their own
    structures for the socket they create in the "accept" function.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle whose corresponding socket
        is desired.

    Socket - Supplies a pointer where a pointer to the socket corresponding to
        the given handle will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_HANDLE if the given handle wasn't a socket.

--*/

