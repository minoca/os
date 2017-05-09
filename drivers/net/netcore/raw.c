/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    raw.c

Abstract:

    This module implements the raw socket protocol.

Author:

    Chris Stevens 20-May-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Protocol drivers are supposed to be able to stand on their own (ie be able to
// be implemented outside the core net library). For the builtin ones, avoid
// including netcore.h, but still redefine those functions that would otherwise
// generate imports.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used by the raw socket protocol.
//

#define RAW_PROTOCOL_ALLOCATION_TAG 0x21707352 // '!psR'

//
// Define the default size of the RAW socket's receive data buffer, in bytes.
//

#define RAW_DEFAULT_RECEIVE_BUFFER_SIZE (256 * _1KB)

//
// Define the minimum receive buffer size.
//

#define RAW_MIN_RECEIVE_BUFFER_SIZE _2KB

//
// Define the default minimum number of bytes necessary for the RAW socket to
// become readable.
//

#define RAW_DEFAULT_RECEIVE_MINIMUM 1

//
// Define the minimum number of bytes necessary for RAW sockets to become
// writable. There is no minimum and bytes are immediately sent on the wire.
//

#define RAW_SEND_MINIMUM 1

//
// Define the maximum packet size allowed on a raw socket.
//

#define RAW_MAX_PACKET_SIZE MAX_ULONG

//
// Define the default protocol entry flags.
//

#define RAW_DEFAULT_PROTOCOL_FLAGS          \
    NET_PROTOCOL_FLAG_MATCH_ANY_PROTOCOL |  \
    NET_PROTOCOL_FLAG_FIND_ALL_SOCKETS |    \
    NET_PROTOCOL_FLAG_NO_DEFAULT_PROTOCOL | \
    NET_PROTOCOL_FLAG_PORTLESS |            \
    NET_PROTOCOL_FLAG_NO_BIND_PERMISSIONS

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a raw socket protocol's data socket.

Members:

    NetSocket - Stores the common core networking parameters.

    ReceivedPacketList - Stores the list of packets ready to be read by the
        user.

    ReceiveLock - Stores the lock that protects the received packets list,
        dropped packet count, and various receive buffer parameters. This lock
        must always be acquired at low level.

    ReceiveBufferTotalSize - Stores the total size of the receive buffer, in
        bytes. Packets that are received but will not fit in the buffer are
        discarded.

    ReceiveBufferFreeSize - Stores the receive buffer's free space, in bytes.
        Packets that are received but do not fit in the free space are
        discarded.

    ReceiveTimeout - Stores the maximum amount of time, in milliseconds, that
        the  socket will wait when receiving data.

    ReceiveMinimum - Stores the minimum amount of bytes that must be available
        before the socket is made readable. This is ignored.

    DroppedPacketCount - Stores the number of packets that have been dropped
        because the receive queue was full.

    ShutdownTypes - Stores the mask of shutdowns that have occurred on this
        socket.

    MaxPacketSize - Stores the maximum size of RAW datagrams.

--*/

typedef struct _RAW_SOCKET {
    NET_SOCKET NetSocket;
    LIST_ENTRY ReceivedPacketList;
    PQUEUED_LOCK ReceiveLock;
    ULONG ReceiveBufferTotalSize;
    ULONG ReceiveBufferFreeSize;
    ULONG ReceiveTimeout;
    ULONG ReceiveMinimum;
    ULONG DroppedPacketCount;
    ULONG MaxPacketSize;
    ULONG ShutdownTypes;
} RAW_SOCKET, *PRAW_SOCKET;

/*++

Structure Description:

    This structure defines a raw socket protocol received message.

Members:

    ListEntry - Stores pointers to the next and previous packets.

    Address - Stores the network address where this data came from.

    DataBuffer - Stores a pointer to the buffer containing the actual data.

    Size - Stores the number of bytes in the data buffer.

--*/

typedef struct _RAW_RECEIVED_PACKET {
    LIST_ENTRY ListEntry;
    NETWORK_ADDRESS Address;
    PVOID DataBuffer;
    ULONG Size;
} RAW_RECEIVED_PACKET, *PRAW_RECEIVED_PACKET;

/*++

Structure Description:

    This structure defines a raw socket option.

Members:

    InformationType - Stores the information type for the socket option.

    Option - Stores the type-specific option identifier.

    Size - Stores the size of the option value, in bytes.

    SetAllowed - Stores a boolean indicating whether or not the option is
        allowed to be set.

--*/

typedef struct _RAW_SOCKET_OPTION {
    SOCKET_INFORMATION_TYPE InformationType;
    UINTN Option;
    UINTN Size;
    BOOL SetAllowed;
} RAW_SOCKET_OPTION, *PRAW_SOCKET_OPTION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpRawCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    );

VOID
NetpRawDestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpRawBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpRawListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpRawAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    );

KSTATUS
NetpRawConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpRawClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetpRawShutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    );

KSTATUS
NetpRawSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

VOID
NetpRawProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpRawProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpRawReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetpRawGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetpRawUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

//
// -------------------------------------------------------------------- Globals
//

NET_PROTOCOL_ENTRY NetRawProtocol = {
    {NULL, NULL},
    NetSocketRaw,
    SOCKET_INTERNET_PROTOCOL_RAW,
    RAW_DEFAULT_PROTOCOL_FLAGS,
    NULL,
    NULL,
    {{0}, {0}, {0}},
    {
        NetpRawCreateSocket,
        NetpRawDestroySocket,
        NetpRawBindToAddress,
        NetpRawListen,
        NetpRawAccept,
        NetpRawConnect,
        NetpRawClose,
        NetpRawShutdown,
        NetpRawSend,
        NetpRawProcessReceivedData,
        NetpRawProcessReceivedSocketData,
        NetpRawReceive,
        NetpRawGetSetInformation,
        NetpRawUserControl
    }
};

RAW_SOCKET_OPTION NetRawSocketOptions[] = {
    {
        SocketInformationBasic,
        SocketBasicOptionSendBufferSize,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionSendMinimum,
        sizeof(ULONG),
        FALSE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveBufferSize,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveMinimum,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveTimeout,
        sizeof(SOCKET_TIME),
        TRUE
    },
};

//
// Store the number of raw sockets that could potentially receive a packet.
//

volatile UINTN NetRawSocketCount = 0;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpRawInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for raw sockets.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Register the raw socket handler with the core networking library. There
    // is no real "raw protocol", so this is a special protocol that gets to
    // filter packets from every protocol.
    //

    Status = NetRegisterProtocol(&NetRawProtocol, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
}

KSTATUS
NetpRawCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    )

/*++

Routine Description:

    This routine allocates resources associated with a new socket. The protocol
    driver is responsible for allocating the structure (with additional length
    for any of its context). The core networking library will fill in the
    common header when this routine returns.

Arguments:

    ProtocolEntry - Supplies a pointer to the protocol information.

    NetworkEntry - Supplies a pointer to the network information.

    NetworkProtocol - Supplies the raw protocol value for this socket used on
        the network. This value is network specific.

    NewSocket - Supplies a pointer where a pointer to a newly allocated
        socket structure will be returned. The caller is responsible for
        allocating the socket (and potentially a larger structure for its own
        context). The core network library will fill in the standard socket
        structure after this routine returns. In phase 1, this will contain
        a pointer to the socket allocated during phase 0.

    Phase - Supplies the socket creation phase. Phase 0 is the allocation phase
        and phase 1 is the advanced initialization phase, which is invoked
        after net core is done filling out common portions of the socket
        structure.

Return Value:

    Status code.

--*/

{

    NETWORK_ADDRESS LocalAddress;
    PNET_SOCKET NetSocket;
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation;
    PRAW_SOCKET RawSocket;
    KSTATUS Status;

    ASSERT(ProtocolEntry->Type == NetSocketRaw);
    ASSERT(ProtocolEntry->ParentProtocolNumber == SOCKET_INTERNET_PROTOCOL_RAW);

    NetSocket = NULL;
    RawSocket = NULL;

    //
    // The thread must have permission to create raw sockets.
    //

    Status = PsCheckPermission(PERMISSION_NET_RAW);
    if (!KSUCCESS(Status)) {
        goto RawCreateSocketEnd;
    }

    //
    // Phase 0 allocates the socket and begins initialization.
    //

    if (Phase == 0) {
        RawSocket = MmAllocatePagedPool(sizeof(RAW_SOCKET),
                                        RAW_PROTOCOL_ALLOCATION_TAG);

        if (RawSocket == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RawCreateSocketEnd;
        }

        RtlZeroMemory(RawSocket, sizeof(RAW_SOCKET));
        NetSocket = &(RawSocket->NetSocket);
        NetSocket->KernelSocket.Protocol = NetworkProtocol;
        NetSocket->KernelSocket.ReferenceCount = 1;
        INITIALIZE_LIST_HEAD(&(RawSocket->ReceivedPacketList));
        RawSocket->ReceiveTimeout = WAIT_TIME_INDEFINITE;
        RawSocket->ReceiveBufferTotalSize = RAW_DEFAULT_RECEIVE_BUFFER_SIZE;
        RawSocket->ReceiveBufferFreeSize = RawSocket->ReceiveBufferTotalSize;
        RawSocket->ReceiveMinimum = RAW_DEFAULT_RECEIVE_MINIMUM;
        RawSocket->MaxPacketSize = RAW_MAX_PACKET_SIZE;
        RawSocket->ReceiveLock = KeCreateQueuedLock();
        if (RawSocket->ReceiveLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RawCreateSocketEnd;
        }

        //
        // Set some kernel socket fields. A raw socket needs to be bound to the
        // any address and made ready to receive as soon as create returns. To
        // avoid requiring common code to handle this, initialize the kernel
        // socket so that the bind routines can be invoked.
        //

        NetSocket->KernelSocket.IoState = IoCreateIoObjectState(FALSE, FALSE);
        if (NetSocket->KernelSocket.IoState == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RawCreateSocketEnd;
        }

        NetSocket->KernelSocket.Domain = NetworkEntry->Domain;
        NetSocket->KernelSocket.Type = ProtocolEntry->Type;

        //
        // Give the lower layers a chance to initialize. Start the maximum
        // packet size at the largest possible value.
        //

        ASSERT(RAW_MAX_PACKET_SIZE == MAX_ULONG);

        PacketSizeInformation = &(NetSocket->PacketSizeInformation);
        PacketSizeInformation->MaxPacketSize = RAW_MAX_PACKET_SIZE;
        Status = NetworkEntry->Interface.InitializeSocket(ProtocolEntry,
                                                          NetworkEntry,
                                                          NetworkProtocol,
                                                          NetSocket);

        if (!KSUCCESS(Status)) {
            goto RawCreateSocketEnd;
        }

        RtlAtomicAdd(&NetRawSocketCount, 1);
        Status = STATUS_SUCCESS;

    //
    // Phase 1 finishes raw specific initialization after netcore is done with
    // its initialization steps.
    //

    } else {

        ASSERT(Phase == 1);
        ASSERT(*NewSocket != NULL);
        ASSERT(RawSocket == NULL);

        NetSocket = *NewSocket;

        //
        // Perform the implicit bind to the any address.
        //

        RtlZeroMemory(&LocalAddress, sizeof(NETWORK_ADDRESS));
        LocalAddress.Domain = NetSocket->KernelSocket.Domain;
        Status = NetpRawBindToAddress(NetSocket, NULL, &LocalAddress);
        if (!KSUCCESS(Status)) {
            goto RawCreateSocketEnd;
        }
    }

RawCreateSocketEnd:
    if (!KSUCCESS(Status)) {
        if (RawSocket != NULL) {
            if (RawSocket->ReceiveLock != NULL) {
                KeDestroyQueuedLock(RawSocket->ReceiveLock);
            }

            MmFreePagedPool(RawSocket);
            RawSocket = NULL;
            NetSocket = NULL;
        }
    }

    *NewSocket = NetSocket;
    return Status;
}

VOID
NetpRawDestroySocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys resources associated with an open socket, officially
    marking the end of the kernel and core networking library's knowledge of
    this structure.

Arguments:

    Socket - Supplies a pointer to the socket to destroy. The core networking
        library will have already destroyed any resources inside the common
        header, the protocol should not reach through any pointers inside the
        socket header except the protocol and network entries.

Return Value:

    None. This routine is responsible for freeing the memory associated with
    the socket structure itself.

--*/

{

    PRAW_RECEIVED_PACKET Packet;
    PRAW_SOCKET RawSocket;

    RawSocket = (PRAW_SOCKET)Socket;

    //
    // Loop through and free any leftover packets.
    //

    KeAcquireQueuedLock(RawSocket->ReceiveLock);
    while (!LIST_EMPTY(&(RawSocket->ReceivedPacketList))) {
        Packet = LIST_VALUE(RawSocket->ReceivedPacketList.Next,
                            RAW_RECEIVED_PACKET,
                            ListEntry);

        LIST_REMOVE(&(Packet->ListEntry));
        RawSocket->ReceiveBufferFreeSize += Packet->Size;
        MmFreePagedPool(Packet);
    }

    ASSERT(RawSocket->ReceiveBufferFreeSize ==
           RawSocket->ReceiveBufferTotalSize);

    KeReleaseQueuedLock(RawSocket->ReceiveLock);
    if (Socket->Network->Interface.DestroySocket != NULL) {
        Socket->Network->Interface.DestroySocket(Socket);
    }

    KeDestroyQueuedLock(RawSocket->ReceiveLock);
    MmFreePagedPool(RawSocket);
    RtlAtomicAdd(&NetRawSocketCount, (UINTN)-1);
    return;
}

KSTATUS
NetpRawBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine binds the given socket to the specified network address.
    Usually this is a no-op for the protocol, it's simply responsible for
    passing the request down to the network layer.

Arguments:

    Socket - Supplies a pointer to the socket to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

Return Value:

    Status code.

--*/

{

    ULONG Flags;
    ULONG OriginalPort;
    KSTATUS Status;

    //
    // Allow raw sockets to get bound multiple times, unless they are already
    // connected to a peer address. They get bound to the any address upon
    // creation.
    //

    if (Socket->RemoteAddress.Domain != NetDomainInvalid) {
        Status = STATUS_INVALID_PARAMETER;
        goto RawBindToAddressEnd;
    }

    //
    // Currently only IPv4 addresses are supported.
    //

    if (Address->Domain != NetDomainIp4) {
        Status = STATUS_NOT_SUPPORTED;
        goto RawBindToAddressEnd;
    }

    //
    // The port doesn't make a difference on raw sockets. Set it to the
    // protocol value, which is storked in the kernel socket.
    //

    OriginalPort = Address->Port;
    Address->Port = Socket->KernelSocket.Protocol;

    //
    // Pass the request down to the network layer. Raw sockets have slightly
    // different bind behavior than other socket types. Indicate this with the
    // flags.
    //

    Flags = NET_SOCKET_BINDING_FLAG_ALLOW_REBIND |
            NET_SOCKET_BINDING_FLAG_ALLOW_UNBIND |
            NET_SOCKET_BINDING_FLAG_NO_PORT_ASSIGNMENT |
            NET_SOCKET_BINDING_FLAG_OVERWRITE_LOCAL |
            NET_SOCKET_BINDING_FLAG_SKIP_ADDRESS_VALIDATION;

    Status = Socket->Network->Interface.BindToAddress(Socket,
                                                      Link,
                                                      Address,
                                                      Flags);

    Address->Port = OriginalPort;
    if (!KSUCCESS(Status)) {
        goto RawBindToAddressEnd;
    }

    //
    // Begin listening immediately, as there is no explicit listen step for raw
    // sockets.
    //

    Status = Socket->Network->Interface.Listen(Socket);
    if (!KSUCCESS(Status)) {
        goto RawBindToAddressEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);

RawBindToAddressEnd:
    return Status;
}

KSTATUS
NetpRawListen (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing clients to attempt to connect to it.

Arguments:

    Socket - Supplies a pointer to the socket to mark as listning.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpRawAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    )

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

{

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpRawConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine attempts to make an outgoing connection to a server.

Arguments:

    Socket - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the address to connect to.

Return Value:

    Status code.

--*/

{

    ULONG OriginalPort;
    KSTATUS Status;

    //
    // Ports don't mean anything to raw sockets. Zero it out. Other
    // implementations seem to keep the port and return it for APIs like
    // getpeername(). This is confusing as a packet is never matched to a
    // socket based on the port. Setting it to zero also makes life easier when
    // searching for sockets during packet reception. The received packet has
    // no raw protocol port. If the socket were connected to some user defined
    // port, then the search compare routines would have to know to skip port
    // validation. Setting the port to zero allows the default compare routines
    // to be used.
    //

    OriginalPort = Address->Port;
    Address->Port = 0;

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.Connect(Socket, Address);
    Address->Port = OriginalPort;
    if (!KSUCCESS(Status)) {
        goto RawConnectEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);

RawConnectEnd:
    return Status;
}

KSTATUS
NetpRawClose (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine closes a socket connection.

Arguments:

    Socket - Supplies a pointer to the socket to shut down.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Close it at the lower level and then release the reference taken on
    // create if the close was successful.
    //

    Status = Socket->Network->Interface.Close(Socket);
    if (!KSUCCESS(Status)) {
        goto RawCloseEnd;
    }

    IoSocketReleaseReference(&(Socket->KernelSocket));

RawCloseEnd:
    return Status;
}

KSTATUS
NetpRawShutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    )

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

{

    PRAW_SOCKET RawSocket;

    //
    // Shutdown is not supported unless the socket is connected.
    //

    if (Socket->RemoteAddress.Domain == NetDomainInvalid) {
        return STATUS_NOT_CONNECTED;
    }

    RawSocket = (PRAW_SOCKET)Socket;
    RtlAtomicOr32(&(RawSocket->ShutdownTypes), ShutdownType);

    //
    // Signal the read event if the read end was shut down.
    //

    if ((ShutdownType & SOCKET_SHUTDOWN_READ) != 0) {
        KeAcquireQueuedLock(RawSocket->ReceiveLock);
        IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_IN, TRUE);
        KeReleaseQueuedLock(RawSocket->ReceiveLock);
    }

    if ((ShutdownType & SOCKET_SHUTDOWN_WRITE) != 0) {
        IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);
    }

    return STATUS_SUCCESS;
}

KSTATUS
NetpRawSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine sends the given data buffer through the network using a
    specific protocol.

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

{

    UINTN BytesComplete;
    PNETWORK_ADDRESS Destination;
    NETWORK_ADDRESS DestinationLocal;
    ULONG Flags;
    ULONG FooterSize;
    ULONG HeaderSize;
    PNET_LINK Link;
    NET_LINK_LOCAL_ADDRESS LinkInformation;
    PNET_SOCKET_LINK_OVERRIDE LinkOverride;
    NET_SOCKET_LINK_OVERRIDE LinkOverrideBuffer;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    PRAW_SOCKET RawSocket;
    UINTN Size;
    KSTATUS Status;

    BytesComplete = 0;
    Flags = Parameters->SocketIoFlags;
    Parameters->SocketIoFlags = 0;
    Link = NULL;
    LinkInformation.Link = NULL;
    LinkOverride = NULL;
    NET_INITIALIZE_PACKET_LIST(&PacketList);
    Size = Parameters->Size;
    RawSocket = (PRAW_SOCKET)Socket;
    Destination = Parameters->NetworkAddress;
    if ((Destination != NULL) && (FromKernelMode == FALSE)) {
        Status = MmCopyFromUserMode(&DestinationLocal,
                                    Destination,
                                    sizeof(NETWORK_ADDRESS));

        Destination = &DestinationLocal;
        if (!KSUCCESS(Status)) {
            goto RawSendEnd;
        }
    }

    if ((Destination == NULL) ||
        (Destination->Domain == NetDomainInvalid)) {

        if (Socket->RemoteAddress.Port == 0) {
            Status = STATUS_NOT_CONFIGURED;
            goto RawSendEnd;
        }

        Destination = &(Socket->RemoteAddress);
    }

    //
    // Fail if the socket has already been closed for writing.
    //

    if ((RawSocket->ShutdownTypes & SOCKET_SHUTDOWN_WRITE) != 0) {
        if ((Flags & SOCKET_IO_NO_SIGNAL) != 0) {
            Status = STATUS_BROKEN_PIPE_SILENT;

        } else {
            Status = STATUS_BROKEN_PIPE;
        }

        goto RawSendEnd;
    }

    //
    // Fail if the socket's link went down.
    //

    if ((Socket->KernelSocket.IoState->Events & POLL_EVENT_DISCONNECTED) != 0) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto RawSendEnd;
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto RawSendEnd;
    }

    //
    // If the socket has no link, then try to find a link that can service the
    // destination address.
    //

    if (Socket->Link == NULL) {
        Status = NetFindLinkForRemoteAddress(Destination, &LinkInformation);
        if (!KSUCCESS(Status)) {
            goto RawSendEnd;
        }

        //
        // Synchronously get the correct header, footer, and max packet sizes.
        //

        LinkOverride = &LinkOverrideBuffer;
        NetInitializeSocketLinkOverride(Socket, &LinkInformation, LinkOverride);
    }

    //
    // Set the necessary local variables based on whether the socket's link or
    // an override link will be used to send the data.
    //

    if (LinkOverride != NULL) {

        ASSERT(LinkOverride == &LinkOverrideBuffer);

        Link = LinkOverrideBuffer.LinkInformation.Link;
        HeaderSize = LinkOverrideBuffer.PacketSizeInformation.HeaderSize;
        FooterSize = LinkOverrideBuffer.PacketSizeInformation.FooterSize;

    } else {

        ASSERT(Socket->Link != NULL);

        Link = Socket->Link;
        HeaderSize = Socket->PacketSizeInformation.HeaderSize;
        FooterSize = Socket->PacketSizeInformation.FooterSize;
    }

    //
    // Allocate a buffer for the packet.
    //

    Status = NetAllocateBuffer(HeaderSize,
                               Size,
                               FooterSize,
                               Link,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto RawSendEnd;
    }

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);

    //
    // Copy the packet data.
    //

    Status = MmCopyIoBufferData(IoBuffer,
                                Packet->Buffer + Packet->DataOffset,
                                BytesComplete,
                                Size - BytesComplete,
                                FALSE);

    if (!KSUCCESS(Status)) {
        goto RawSendEnd;
    }

    //
    // Send the datagram down to the network layer, which may have to send it
    // in fragments.
    //

    Status = Socket->Network->Interface.Send(Socket,
                                             Destination,
                                             LinkOverride,
                                             &PacketList);

    if (!KSUCCESS(Status)) {
        goto RawSendEnd;
    }

    Packet = NULL;
    BytesComplete = Size;

RawSendEnd:
    Parameters->BytesCompleted = BytesComplete;
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    if (LinkInformation.Link != NULL) {
        NetLinkReleaseReference(LinkInformation.Link);
    }

    if (LinkOverride == &(LinkOverrideBuffer)) {

        ASSERT(LinkOverrideBuffer.LinkInformation.Link != NULL);

        NetLinkReleaseReference(LinkOverrideBuffer.LinkInformation.Link);
    }

    return Status;
}

VOID
NetpRawProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    PNET_SOCKET PreviousSocket;
    PNET_SOCKET Socket;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // If no raw sockets are present, then immediately exit.
    //

    if (NetRawSocketCount == 0) {
        return;
    }

    //
    // Each raw sockets' local receive address was initialized with the port
    // set to the protocol number. Each raw socket's remote address was set to
    // 0 when it was fully bound. Initialize the receive context in this way
    // as well so that the ports will match any activated sockets.
    //

    ReceiveContext->Source->Port = 0;
    ReceiveContext->Destination->Port = ReceiveContext->ParentProtocolNumber;;

    //
    // Find all the sockets willing to take this packet.
    //

    Socket = NULL;
    PreviousSocket = NULL;
    do {
        Status = NetFindSocket(ReceiveContext, &Socket);
        if (!KSUCCESS(Status) && (Status != STATUS_MORE_PROCESSING_REQUIRED)) {
            break;
        }

        //
        // Pass the packet onto the socket for copying and safe keeping until
        // the data is read.
        //

        NetpRawProcessReceivedSocketData(Socket, ReceiveContext);

        //
        // Release the reference on the previous socket added by the find
        // socket call.
        //

        if (PreviousSocket != NULL) {
            IoSocketReleaseReference(&(PreviousSocket->KernelSocket));
        }

        PreviousSocket = Socket;

    } while (Status == STATUS_MORE_PROCESSING_REQUIRED);

    if (PreviousSocket != NULL) {
        IoSocketReleaseReference(&(PreviousSocket->KernelSocket));
    }

    ReceiveContext->Destination->Port = 0;
    return;
}

KSTATUS
NetpRawProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called for a particular socket to process a received packet
    that was sent to it.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG Length;
    PNET_PACKET_BUFFER Packet;
    PRAW_RECEIVED_PACKET RawPacket;
    PRAW_SOCKET RawSocket;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Socket != NULL);

    RawSocket = (PRAW_SOCKET)Socket;
    Packet = ReceiveContext->Packet;
    Length = Packet->FooterOffset - Packet->DataOffset;

    //
    // Create a received packet entry for this data.
    //

    AllocationSize = sizeof(RAW_RECEIVED_PACKET) + Length;
    RawPacket = MmAllocatePagedPool(AllocationSize,
                                    RAW_PROTOCOL_ALLOCATION_TAG);

    if (RawPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ProcessReceivedKernelDataEnd;
    }

    RtlCopyMemory(&(RawPacket->Address),
                  ReceiveContext->Source,
                  sizeof(NETWORK_ADDRESS));

    RawPacket->DataBuffer = (PVOID)(RawPacket + 1);
    RawPacket->Size = Length;

    //
    // Copy the packet contents into the receive packet buffer.
    //

    RtlCopyMemory(RawPacket->DataBuffer,
                  Packet->Buffer + Packet->DataOffset,
                  Length);

    //
    // Work to insert the packet on the list of received packets.
    //

    KeAcquireQueuedLock(RawSocket->ReceiveLock);
    if (RawPacket->Size <= RawSocket->ReceiveBufferFreeSize) {
        INSERT_BEFORE(&(RawPacket->ListEntry),
                      &(RawSocket->ReceivedPacketList));

        RawSocket->ReceiveBufferFreeSize -= RawPacket->Size;

        ASSERT(RawSocket->ReceiveBufferFreeSize <
               RawSocket->ReceiveBufferTotalSize);

        //
        // One packet is always enough to notify a waiting receiver.
        //

        IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_IN, TRUE);
        RawPacket = NULL;

    } else {
        RawSocket->DroppedPacketCount += 1;
    }

    KeReleaseQueuedLock(RawSocket->ReceiveLock);

    //
    // If the packet wasn't nulled out, that's an indication it wasn't added to
    // the list, so free it up.
    //

    if (RawPacket != NULL) {
        MmFreePagedPool(RawPacket);
    }

    Status = STATUS_SUCCESS;

ProcessReceivedKernelDataEnd:
    return Status;
}

KSTATUS
NetpRawReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine is called by the user to receive data from the socket on a
    particular protocol.

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

{

    UINTN BytesComplete;
    ULONG CopySize;
    ULONGLONG CurrentTime;
    ULONGLONG EndTime;
    ULONG Flags;
    BOOL LockHeld;
    PRAW_RECEIVED_PACKET Packet;
    PLIST_ENTRY PacketEntry;
    PRAW_SOCKET RawSocket;
    ULONG ReturnedEvents;
    ULONG ReturnSize;
    UINTN Size;
    KSTATUS Status;
    ULONGLONG TimeCounterFrequency;
    ULONG Timeout;
    ULONG WaitTime;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    BytesComplete = 0;
    EndTime = 0;
    LockHeld = FALSE;
    Flags = Parameters->SocketIoFlags;
    Parameters->SocketIoFlags = 0;
    Size = Parameters->Size;
    TimeCounterFrequency = 0;
    Timeout = Parameters->TimeoutInMilliseconds;
    RawSocket = (PRAW_SOCKET)Socket;
    if ((Flags & SOCKET_IO_OUT_OF_BAND) != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto RawReceiveEnd;
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto RawReceiveEnd;
    }

    //
    // Set a timeout timer to give up on. The socket stores the maximum timeout.
    //

    if (Timeout > RawSocket->ReceiveTimeout) {
        Timeout = RawSocket->ReceiveTimeout;
    }

    if ((Timeout != 0) && (Timeout != WAIT_TIME_INDEFINITE)) {
        EndTime = KeGetRecentTimeCounter();
        EndTime += KeConvertMicrosecondsToTimeTicks(
                                       Timeout * MICROSECONDS_PER_MILLISECOND);

        TimeCounterFrequency = HlQueryTimeCounterFrequency();
    }

    //
    // Loop trying to get some data. This loop exits once one packet is read.
    //

    while (TRUE) {

        //
        // Wait for a packet to become available. Start by computing the wait
        // time.
        //

        if (Timeout == 0) {
            WaitTime = 0;

        } else if (Timeout != WAIT_TIME_INDEFINITE) {
            CurrentTime = KeGetRecentTimeCounter();
            WaitTime = (EndTime - CurrentTime) * MILLISECONDS_PER_SECOND /
                       TimeCounterFrequency;

        } else {
            WaitTime = WAIT_TIME_INDEFINITE;
        }

        //
        // Wait for something to maybe become available. If the wait fails due
        // to a timeout, interruption, or something else, then fail out.
        // Otherwise when the read event is signalled, there are at least one
        // packet available.
        //

        Status = IoWaitForIoObjectState(Socket->KernelSocket.IoState,
                                        POLL_EVENT_IN,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            goto RawReceiveEnd;
        }

        if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
            if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                Status = STATUS_NO_NETWORK_CONNECTION;

            } else {
                Status = NET_SOCKET_GET_LAST_ERROR(Socket);
                if (KSUCCESS(Status)) {
                    Status = STATUS_DEVICE_IO_ERROR;
                }
            }

            goto RawReceiveEnd;
        }

        KeAcquireQueuedLock(RawSocket->ReceiveLock);
        LockHeld = TRUE;

        //
        // Fail with EOF if the socket has already been closed for reading.
        //

        if ((RawSocket->ShutdownTypes & SOCKET_SHUTDOWN_READ) != 0) {
            Status = STATUS_END_OF_FILE;
            goto RawReceiveEnd;
        }

        //
        // If another thread beat this one to the punch, try again.
        //

        if (LIST_EMPTY(&(RawSocket->ReceivedPacketList)) != FALSE) {
            KeReleaseQueuedLock(RawSocket->ReceiveLock);
            LockHeld = FALSE;
            continue;
        }

        //
        // This should be the first packet being read.
        //

        ASSERT(BytesComplete == 0);

        PacketEntry = RawSocket->ReceivedPacketList.Next;
        Packet = LIST_VALUE(PacketEntry, RAW_RECEIVED_PACKET, ListEntry);
        ReturnSize = Packet->Size;
        CopySize = ReturnSize;
        if (CopySize > Size) {
            Parameters->SocketIoFlags |= SOCKET_IO_DATA_TRUNCATED;
            CopySize = Size;

            //
            // The real packet size is only returned to the user on truncation
            // if the truncated flag was supplied to this routine. Default to
            // returning the truncated size.
            //

            if ((Flags & SOCKET_IO_DATA_TRUNCATED) == 0) {
                ReturnSize = CopySize;
            }
        }

        Status = MmCopyIoBufferData(IoBuffer,
                                    Packet->DataBuffer,
                                    0,
                                    CopySize,
                                    TRUE);

        if (!KSUCCESS(Status)) {
            goto RawReceiveEnd;
        }

        //
        // Copy the packet address out to the caller if requested.
        //

        if (Parameters->NetworkAddress != NULL) {
            if (FromKernelMode != FALSE) {
                RtlCopyMemory(Parameters->NetworkAddress,
                              &(Packet->Address),
                              sizeof(NETWORK_ADDRESS));

            } else {
                Status = MmCopyToUserMode(Parameters->NetworkAddress,
                                          &(Packet->Address),
                                          sizeof(NETWORK_ADDRESS));

                if (!KSUCCESS(Status)) {
                    goto RawReceiveEnd;
                }
            }
        }

        BytesComplete = ReturnSize;

        //
        // Remove the packet if not peeking.
        //

        if ((Flags & SOCKET_IO_PEEK) == 0) {
            LIST_REMOVE(&(Packet->ListEntry));
            RawSocket->ReceiveBufferFreeSize += Packet->Size;

            //
            // The total receive buffer size may have been decreased. Don't
            // increment the free size above the total.
            //

            if (RawSocket->ReceiveBufferFreeSize >
                RawSocket->ReceiveBufferTotalSize) {

                RawSocket->ReceiveBufferFreeSize =
                                             RawSocket->ReceiveBufferTotalSize;
            }

            MmFreePagedPool(Packet);

            //
            // Unsignal the IN event if there are no more packets.
            //

            if (LIST_EMPTY(&(RawSocket->ReceivedPacketList)) != FALSE) {
                IoSetIoObjectState(Socket->KernelSocket.IoState,
                                   POLL_EVENT_IN,
                                   FALSE);
            }
        }

        //
        // Wait-all does not apply to raw sockets. Break out.
        //

        Status = STATUS_SUCCESS;
        break;
    }

RawReceiveEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(RawSocket->ReceiveLock);
    }

    Parameters->BytesCompleted = BytesComplete;
    return Status;
}

KSTATUS
NetpRawGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN Option,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

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

    STATUS_NOT_HANDLED if the protocol does not override the default behavior
        for a basic socket option.

--*/

{

    ULONG Count;
    ULONG Index;
    LONGLONG Milliseconds;
    PRAW_SOCKET_OPTION RawOption;
    PRAW_SOCKET RawSocket;
    PNET_PACKET_SIZE_INFORMATION SizeInformation;
    ULONG SizeOption;
    PSOCKET_TIME SocketTime;
    SOCKET_TIME SocketTimeBuffer;
    PVOID Source;
    KSTATUS Status;

    RawSocket = (PRAW_SOCKET)Socket;
    if ((InformationType != SocketInformationBasic) &&
        (InformationType != SocketInformationUdp)) {

        Status = STATUS_NOT_SUPPORTED;
        goto RawGetSetInformationEnd;
    }

    //
    // Search to see if the socket option is supported by the raw protocol.
    //

    Count = sizeof(NetRawSocketOptions) / sizeof(NetRawSocketOptions[0]);
    for (Index = 0; Index < Count; Index += 1) {
        RawOption = &(NetRawSocketOptions[Index]);
        if ((RawOption->InformationType == InformationType) &&
            (RawOption->Option == Option)) {

            break;
        }
    }

    if (Index == Count) {
        if (InformationType == SocketInformationBasic) {
            Status = STATUS_NOT_HANDLED;

        } else {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        }

        goto RawGetSetInformationEnd;
    }

    //
    // Handle failure cases common to all options.
    //

    if (Set != FALSE) {
        if (RawOption->SetAllowed == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            goto RawGetSetInformationEnd;
        }

        if (*DataSize < RawOption->Size) {
            *DataSize = RawOption->Size;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto RawGetSetInformationEnd;
        }
    }

    //
    // There are currently no raw protocol options.
    //

    ASSERT(InformationType != SocketInformationRaw);

    //
    // Parse the basic socket option, getting the information from the raw
    // socket or setting the new state in the raw socket.
    //

    Source = NULL;
    Status = STATUS_SUCCESS;
    switch ((SOCKET_BASIC_OPTION)Option) {
    case SocketBasicOptionSendBufferSize:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);
            if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                SizeOption = SOCKET_OPTION_MAX_ULONG;
            }

            SizeInformation = &(Socket->PacketSizeInformation);
            if (SizeOption > RAW_MAX_PACKET_SIZE) {
                SizeOption = RAW_MAX_PACKET_SIZE;

            } else if (SizeOption < SizeInformation->MaxPacketSize) {
                SizeOption = SizeInformation->MaxPacketSize;
            }

            RawSocket->MaxPacketSize = SizeOption;

        } else {
            Source = &SizeOption;
            SizeOption = RawSocket->MaxPacketSize;
        }

        break;

    case SocketBasicOptionSendMinimum:

        ASSERT(Set == FALSE);

        Source = &SizeOption;
        SizeOption = RAW_SEND_MINIMUM;
        break;

    case SocketBasicOptionReceiveBufferSize:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);
            if (SizeOption < RAW_MIN_RECEIVE_BUFFER_SIZE) {
                SizeOption = RAW_MIN_RECEIVE_BUFFER_SIZE;

            } else if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                SizeOption = SOCKET_OPTION_MAX_ULONG;
            }

            //
            // Set the receive buffer size and truncate the available free
            // space if necessary. Do not remove any packets that have already
            // been received. This is not meant to be a truncate call.
            //

            KeAcquireQueuedLock(RawSocket->ReceiveLock);
            RawSocket->ReceiveBufferTotalSize = SizeOption;
            if (RawSocket->ReceiveBufferFreeSize > SizeOption) {
                RawSocket->ReceiveBufferFreeSize = SizeOption;
            }

            KeReleaseQueuedLock(RawSocket->ReceiveLock);

        } else {
            Source = &SizeOption;
            SizeOption = RawSocket->ReceiveBufferTotalSize;
        }

        break;

    case SocketBasicOptionReceiveMinimum:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);
            if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                SizeOption = SOCKET_OPTION_MAX_ULONG;
            }

            RawSocket->ReceiveMinimum = SizeOption;

        } else {
            Source = &SizeOption;
            SizeOption = RawSocket->ReceiveMinimum;
        }

        break;

    case SocketBasicOptionReceiveTimeout:
        if (Set != FALSE) {
            SocketTime = (PSOCKET_TIME)Data;
            if (SocketTime->Seconds < 0) {
                Status = STATUS_DOMAIN_ERROR;
                break;
            }

            Milliseconds = SocketTime->Seconds * MILLISECONDS_PER_SECOND;
            if (Milliseconds < SocketTime->Seconds) {
                Status = STATUS_DOMAIN_ERROR;
                break;
            }

            Milliseconds += SocketTime->Microseconds /
                            MICROSECONDS_PER_MILLISECOND;

            if ((Milliseconds < 0) || (Milliseconds > MAX_LONG)) {
                Status = STATUS_DOMAIN_ERROR;
                break;
            }

            RawSocket->ReceiveTimeout = (ULONG)(LONG)Milliseconds;

        } else {
            Source = &SocketTimeBuffer;
            if (RawSocket->ReceiveTimeout == WAIT_TIME_INDEFINITE) {
                SocketTimeBuffer.Seconds = 0;
                SocketTimeBuffer.Microseconds = 0;

            } else {
                SocketTimeBuffer.Seconds = RawSocket->ReceiveTimeout /
                                           MILLISECONDS_PER_SECOND;

                SocketTimeBuffer.Microseconds = (RawSocket->ReceiveTimeout %
                                                 MILLISECONDS_PER_SECOND) *
                                                MICROSECONDS_PER_MILLISECOND;
            }
        }

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto RawGetSetInformationEnd;
    }

    //
    // Truncate all copies for get requests down to the required size and only
    // return the required size on set requests.
    //

    if (*DataSize > RawOption->Size) {
        *DataSize = RawOption->Size;
    }

    //
    // For get requests, copy the gathered information to the supplied data
    // buffer.
    //

    if (Set == FALSE) {

        ASSERT(Source != NULL);

        RtlCopyMemory(Data, Source, *DataSize);

        //
        // If the copy truncated the data, report that the given buffer was too
        // small. The caller can choose to ignore this if the truncated data is
        // enough.
        //

        if (*DataSize < RawOption->Size) {
            *DataSize = RawOption->Size;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto RawGetSetInformationEnd;
        }
    }

RawGetSetInformationEnd:
    return Status;
}

KSTATUS
NetpRawUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    )

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

{

    return STATUS_NOT_SUPPORTED;
}

//
// --------------------------------------------------------- Internal Functions
//

