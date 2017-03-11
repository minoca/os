/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    udp.c

Abstract:

    This module implements the User Datagram Protocol (UDP).

Author:

    Evan Green 5-Apr-2013

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
// Define the allocation tag used by the UDP socket protocol.
//

#define UDP_PROTOCOL_ALLOCATION_TAG 0x21706455 // '!pdU'

//
// Define the maximum supported packet size of the UDP protocol, including the
// UDP headers.
//

#define UDP_MAX_PACKET_SIZE 0xFFFF

//
// Define the default size of UDP's receive data buffer, in bytes.
//

#define UDP_DEFAULT_RECEIVE_BUFFER_SIZE (256 * _1KB)

//
// Define the minimum receive buffer size.
//

#define UDP_MIN_RECEIVE_BUFFER_SIZE _2KB

//
// Define the default minimum number of bytes necessary for the UDP socket to
// become readable.
//

#define UDP_DEFAULT_RECEIVE_MINIMUM 1

//
// Define the minmum number of bytes necessary for UDP sockets to become
// writable. There is no minimum and bytes are immediately sent on the wire.
//

#define UDP_SEND_MINIMUM 1

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a UDP data socket.

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

    MaxPacketSize - Stores the maximum size of UDP datagrams.

--*/

typedef struct _UDP_SOCKET {
    NET_SOCKET NetSocket;
    LIST_ENTRY ReceivedPacketList;
    PQUEUED_LOCK ReceiveLock;
    ULONG ReceiveBufferTotalSize;
    ULONG ReceiveBufferFreeSize;
    ULONG ReceiveTimeout;
    ULONG ReceiveMinimum;
    ULONG DroppedPacketCount;
    ULONG ShutdownTypes;
    USHORT MaxPacketSize;
} UDP_SOCKET, *PUDP_SOCKET;

/*++

Structure Description:

    This structure defines a UDP protocol header, which is pretty darn simple.

Members:

    SourcePort - Stores the optional source port number (use 0 if not supplied).

    DestinationPort - Stores the destination port number.

    Length - Stores the length of the header and data.

    Checksum - Stores the optional checksum. Set to 0 if not supplied. The
        checksum is the one's complement of the one's complement sum of the
        entire header plus data, padded with zeros if needed to be a multiple of
        two octets. A pseudo IP header is used for the calculation.

--*/

typedef struct _UDP_HEADER {
    USHORT SourcePort;
    USHORT DestinationPort;
    USHORT Length;
    USHORT Checksum;
} PACKED UDP_HEADER, *PUDP_HEADER;

/*++

Structure Description:

    This structure defines a UDP received message.

Members:

    ListEntry - Stores pointers to the next and previous packets.

    Address - Stores the network address where this data came from.

    DataBuffer - Stores a pointer to the buffer containing the actual data.

    Size - Stores the number of bytes in the data buffer.

--*/

typedef struct _UDP_RECEIVED_PACKET {
    LIST_ENTRY ListEntry;
    NETWORK_ADDRESS Address;
    PVOID DataBuffer;
    ULONG Size;
} UDP_RECEIVED_PACKET, *PUDP_RECEIVED_PACKET;

/*++

Structure Description:

    This structure defines a UDP socket option.

Members:

    InformationType - Stores the information type for the socket option.

    Option - Stores the type-specific option identifier.

    Size - Stores the size of the option value, in bytes.

    SetAllowed - Stores a boolean indicating whether or not the option is
        allowed to be set.

--*/

typedef struct _UDP_SOCKET_OPTION {
    SOCKET_INFORMATION_TYPE InformationType;
    UINTN Option;
    UINTN Size;
    BOOL SetAllowed;
} UDP_SOCKET_OPTION, *PUDP_SOCKET_OPTION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpUdpCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    );

VOID
NetpUdpDestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpUdpBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpUdpListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpUdpAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    );

KSTATUS
NetpUdpConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpUdpClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetpUdpShutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    );

KSTATUS
NetpUdpSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

VOID
NetpUdpProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpUdpProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpUdpReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetpUdpGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetpUdpUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

//
// -------------------------------------------------------------------- Globals
//

NET_PROTOCOL_ENTRY NetUdpProtocol = {
    {NULL, NULL},
    NetSocketDatagram,
    SOCKET_INTERNET_PROTOCOL_UDP,
    0,
    NULL,
    NULL,
    {{0}, {0}, {0}},
    {
        NetpUdpCreateSocket,
        NetpUdpDestroySocket,
        NetpUdpBindToAddress,
        NetpUdpListen,
        NetpUdpAccept,
        NetpUdpConnect,
        NetpUdpClose,
        NetpUdpShutdown,
        NetpUdpSend,
        NetpUdpProcessReceivedData,
        NetpUdpProcessReceivedSocketData,
        NetpUdpReceive,
        NetpUdpGetSetInformation,
        NetpUdpUserControl
    }
};

UDP_SOCKET_OPTION NetUdpSocketOptions[] = {
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
// ------------------------------------------------------------------ Functions
//

VOID
NetpUdpInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for UDP sockets.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Register the UDP socket handlers with the core networking library.
    //

    Status = NetRegisterProtocol(&NetUdpProtocol, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
}

KSTATUS
NetpUdpCreateSocket (
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

    ULONG MaxPacketSize;
    PNET_SOCKET NetSocket;
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation;
    KSTATUS Status;
    PUDP_SOCKET UdpSocket;

    ASSERT(ProtocolEntry->Type == NetSocketDatagram);
    ASSERT((ProtocolEntry->ParentProtocolNumber ==
            SOCKET_INTERNET_PROTOCOL_UDP) &&
           (NetworkProtocol == ProtocolEntry->ParentProtocolNumber));

    //
    // TCP only operates in phase 0.
    //

    if (Phase != 0) {
        return STATUS_SUCCESS;
    }

    NetSocket = NULL;
    UdpSocket = MmAllocatePagedPool(sizeof(UDP_SOCKET),
                                    UDP_PROTOCOL_ALLOCATION_TAG);

    if (UdpSocket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto UdpCreateSocketEnd;
    }

    RtlZeroMemory(UdpSocket, sizeof(UDP_SOCKET));
    NetSocket = &(UdpSocket->NetSocket);
    NetSocket->KernelSocket.Protocol = NetworkProtocol;
    NetSocket->KernelSocket.ReferenceCount = 1;
    INITIALIZE_LIST_HEAD(&(UdpSocket->ReceivedPacketList));
    UdpSocket->ReceiveTimeout = WAIT_TIME_INDEFINITE;
    UdpSocket->ReceiveBufferTotalSize = UDP_DEFAULT_RECEIVE_BUFFER_SIZE;
    UdpSocket->ReceiveBufferFreeSize = UdpSocket->ReceiveBufferTotalSize;
    UdpSocket->ReceiveMinimum = UDP_DEFAULT_RECEIVE_MINIMUM;
    UdpSocket->MaxPacketSize = UDP_MAX_PACKET_SIZE;
    UdpSocket->ReceiveLock = KeCreateQueuedLock();
    if (UdpSocket->ReceiveLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto UdpCreateSocketEnd;
    }

    //
    // Give the lower layers a chance to initialize. Start the maximum packet
    // size at the largest possible value.
    //

    PacketSizeInformation = &(NetSocket->PacketSizeInformation);
    PacketSizeInformation->MaxPacketSize = MAX_ULONG;
    Status = NetworkEntry->Interface.InitializeSocket(ProtocolEntry,
                                                      NetworkEntry,
                                                      NetworkProtocol,
                                                      NetSocket);

    if (!KSUCCESS(Status)) {
        goto UdpCreateSocketEnd;
    }

    //
    // If the max packet size is greater than what is allowed for a UDP packet
    // plus all the previous headers and footers, then truncate the max packet
    // size. Note that the UDP max packet size includes the UDP header.
    //

    MaxPacketSize = PacketSizeInformation->HeaderSize +
                    UDP_MAX_PACKET_SIZE +
                    PacketSizeInformation->FooterSize;

    if (PacketSizeInformation->MaxPacketSize > MaxPacketSize) {
        PacketSizeInformation->MaxPacketSize = MaxPacketSize;
    }

    //
    // Add the UDP header size to the protocol header size.
    //

    PacketSizeInformation->HeaderSize += sizeof(UDP_HEADER);
    Status = STATUS_SUCCESS;

UdpCreateSocketEnd:
    if (!KSUCCESS(Status)) {
        if (UdpSocket != NULL) {
            if (UdpSocket->ReceiveLock != NULL) {
                KeDestroyQueuedLock(UdpSocket->ReceiveLock);
            }

            MmFreePagedPool(UdpSocket);
            UdpSocket = NULL;
            NetSocket = NULL;
        }
    }

    *NewSocket = NetSocket;
    return Status;
}

VOID
NetpUdpDestroySocket (
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

    PUDP_RECEIVED_PACKET Packet;
    PUDP_SOCKET UdpSocket;

    UdpSocket = (PUDP_SOCKET)Socket;

    //
    // Loop through and free any leftover packets.
    //

    KeAcquireQueuedLock(UdpSocket->ReceiveLock);
    while (!LIST_EMPTY(&(UdpSocket->ReceivedPacketList))) {
        Packet = LIST_VALUE(UdpSocket->ReceivedPacketList.Next,
                            UDP_RECEIVED_PACKET,
                            ListEntry);

        LIST_REMOVE(&(Packet->ListEntry));
        UdpSocket->ReceiveBufferFreeSize += Packet->Size;
        MmFreePagedPool(Packet);
    }

    ASSERT(UdpSocket->ReceiveBufferFreeSize ==
           UdpSocket->ReceiveBufferTotalSize);

    KeReleaseQueuedLock(UdpSocket->ReceiveLock);
    if (Socket->Network->Interface.DestroySocket != NULL) {
        Socket->Network->Interface.DestroySocket(Socket);
    }

    KeDestroyQueuedLock(UdpSocket->ReceiveLock);
    MmFreePagedPool(UdpSocket);
    return;
}

KSTATUS
NetpUdpBindToAddress (
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

    KSTATUS Status;

    if (Socket->LocalReceiveAddress.Domain != NetDomainInvalid) {
        Status = STATUS_INVALID_PARAMETER;
        goto UdpBindToAddressEnd;
    }

    //
    // Currently only IPv4 addresses are supported.
    //

    if (Address->Domain != NetDomainIp4) {
        Status = STATUS_NOT_SUPPORTED;
        goto UdpBindToAddressEnd;
    }

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.BindToAddress(Socket, Link, Address, 0);
    if (!KSUCCESS(Status)) {
        goto UdpBindToAddressEnd;
    }

    //
    // Begin listening immediately, as there is no explicit listen step for UDP.
    //

    Status = Socket->Network->Interface.Listen(Socket);
    if (!KSUCCESS(Status)) {
        goto UdpBindToAddressEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);

UdpBindToAddressEnd:
    return Status;
}

KSTATUS
NetpUdpListen (
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
NetpUdpAccept (
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
NetpUdpConnect (
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

    KSTATUS Status;

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.Connect(Socket, Address);
    if (!KSUCCESS(Status)) {
        goto UdpConnectEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);

UdpConnectEnd:
    return Status;
}

KSTATUS
NetpUdpClose (
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
        goto UdpCloseEnd;
    }

    IoSocketReleaseReference(&(Socket->KernelSocket));

UdpCloseEnd:
    return Status;
}

KSTATUS
NetpUdpShutdown (
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

    PUDP_SOCKET UdpSocket;

    //
    // Shutdown is not supported unless the socket is connected.
    //

    if (Socket->RemoteAddress.Domain == NetDomainInvalid) {
        return STATUS_NOT_CONNECTED;
    }

    UdpSocket = (PUDP_SOCKET)Socket;
    RtlAtomicOr32(&(UdpSocket->ShutdownTypes), ShutdownType);

    //
    // Signal the read event if the read end was shut down.
    //

    if ((ShutdownType & SOCKET_SHUTDOWN_READ) != 0) {
        KeAcquireQueuedLock(UdpSocket->ReceiveLock);
        IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_IN, TRUE);
        KeReleaseQueuedLock(UdpSocket->ReceiveLock);
    }

    if ((ShutdownType & SOCKET_SHUTDOWN_WRITE) != 0) {
        IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);
    }

    return STATUS_SUCCESS;
}

KSTATUS
NetpUdpSend (
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
    NETWORK_ADDRESS LocalAddress;
    USHORT NetworkLocalPort;
    USHORT NetworkRemotePort;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    UINTN Size;
    USHORT SourcePort;
    KSTATUS Status;
    PUDP_HEADER UdpHeader;
    PUDP_SOCKET UdpSocket;

    ASSERT(Socket->PacketSizeInformation.MaxPacketSize > sizeof(UDP_HEADER));

    BytesComplete = 0;
    Link = NULL;
    LinkInformation.Link = NULL;
    LinkOverride = NULL;
    NET_INITIALIZE_PACKET_LIST(&PacketList);
    Size = Parameters->Size;
    UdpSocket = (PUDP_SOCKET)Socket;
    Flags = Parameters->SocketIoFlags;
    Parameters->SocketIoFlags = 0;
    Destination = Parameters->NetworkAddress;
    if ((Destination != NULL) && (FromKernelMode == FALSE)) {
        Status = MmCopyFromUserMode(&DestinationLocal,
                                    Destination,
                                    sizeof(NETWORK_ADDRESS));

        Destination = &DestinationLocal;
        if (!KSUCCESS(Status)) {
            goto UdpSendEnd;
        }
    }

    if ((Destination == NULL) ||
        (Destination->Domain == NetDomainInvalid)) {

        if (Socket->RemoteAddress.Port == 0) {
            Status = STATUS_NOT_CONFIGURED;
            goto UdpSendEnd;
        }

        Destination = &(Socket->RemoteAddress);
    }

    //
    // Fail if the socket has already been closed for writing.
    //

    if ((UdpSocket->ShutdownTypes & SOCKET_SHUTDOWN_WRITE) != 0) {
        if ((Flags & SOCKET_IO_NO_SIGNAL) != 0) {
            Status = STATUS_BROKEN_PIPE_SILENT;

        } else {
            Status = STATUS_BROKEN_PIPE;
        }

        goto UdpSendEnd;
    }

    //
    // Fail if the socket's link went down.
    //

    if ((Socket->KernelSocket.IoState->Events & POLL_EVENT_DISCONNECTED) != 0) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto UdpSendEnd;
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto UdpSendEnd;
    }

    //
    // If the size, including the header, is greater than the UDP socket's
    // maximum packet size, fail.
    //

    if ((Size + sizeof(UDP_HEADER)) > UdpSocket->MaxPacketSize) {
        Status = STATUS_MESSAGE_TOO_LONG;
        goto UdpSendEnd;
    }

    //
    // If the socket is not yet bound, then at least try to bind it to a local
    // port. This bind attempt may race with another bind attempt, but leave it
    // to the socket owner to synchronize bind and send.
    //

    if (Socket->BindingType == SocketBindingInvalid) {
        RtlZeroMemory(&LocalAddress, sizeof(NETWORK_ADDRESS));
        LocalAddress.Domain = Socket->Network->Domain;
        Status = NetpUdpBindToAddress(Socket, NULL, &LocalAddress);
        if (!KSUCCESS(Status)) {
            goto UdpSendEnd;
        }
    }

    //
    // The socket needs to at least be bound to a local port.
    //

    ASSERT(Socket->LocalSendAddress.Port != 0);

    //
    // If the socket has no link, then try to find a link that can service the
    // destination address.
    //

    if (Socket->Link == NULL) {
        Status = NetFindLinkForRemoteAddress(Destination, &LinkInformation);
        if (!KSUCCESS(Status)) {
            goto UdpSendEnd;
        }

        //
        // The link override should use the socket's port.
        //

        LinkInformation.SendAddress.Port = Socket->LocalSendAddress.Port;

        //
        // Synchronously get the correct header, footer, and max packet ssizes.
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
        SourcePort = LinkOverrideBuffer.LinkInformation.SendAddress.Port;

    } else {

        ASSERT(Socket->Link != NULL);

        Link = Socket->Link;
        HeaderSize = Socket->PacketSizeInformation.HeaderSize;
        FooterSize = Socket->PacketSizeInformation.FooterSize;
        SourcePort = Socket->LocalSendAddress.Port;
    }

    NetworkLocalPort = CPU_TO_NETWORK16(SourcePort);
    NetworkRemotePort = CPU_TO_NETWORK16(Destination->Port);

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
        goto UdpSendEnd;
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
        goto UdpSendEnd;
    }

    //
    // Add the UDP header.
    //

    ASSERT(Packet->DataOffset >= sizeof(UDP_HEADER));

    Packet->DataOffset -= sizeof(UDP_HEADER);
    UdpHeader = (PUDP_HEADER)(Packet->Buffer + Packet->DataOffset);
    UdpHeader->SourcePort = NetworkLocalPort;
    UdpHeader->DestinationPort = NetworkRemotePort;
    UdpHeader->Length = CPU_TO_NETWORK16(Size + sizeof(UDP_HEADER));
    UdpHeader->Checksum = 0;
    if ((Link->Properties.Capabilities &
        NET_LINK_CAPABILITY_TRANSMIT_UDP_CHECKSUM_OFFLOAD) != 0) {

        Packet->Flags |= NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD;
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
        goto UdpSendEnd;
    }

    Packet = NULL;
    BytesComplete = Size;

UdpSendEnd:
    Parameters->BytesCompleted = BytesComplete;
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    if (LinkInformation.Link != NULL) {
        NetLinkReleaseReference(LinkInformation.Link);
    }

    if (LinkOverride == &LinkOverrideBuffer) {

        ASSERT(LinkOverrideBuffer.LinkInformation.Link != NULL);

        NetLinkReleaseReference(LinkOverrideBuffer.LinkInformation.Link);
    }

    return Status;
}

VOID
NetpUdpProcessReceivedData (
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

    PUDP_HEADER Header;
    USHORT Length;
    PNET_PACKET_BUFFER Packet;
    PNET_SOCKET PreviousSocket;
    PNET_SOCKET Socket;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Packet = ReceiveContext->Packet;
    Header = (PUDP_HEADER)(Packet->Buffer + Packet->DataOffset);
    Length = NETWORK_TO_CPU16(Header->Length);
    if (Length > (Packet->FooterOffset - Packet->DataOffset)) {
        RtlDebugPrint("Invalid UDP length %d is bigger than packet data, "
                      "which is only %d bytes large.\n",
                      Length,
                      (Packet->FooterOffset - Packet->DataOffset));

        return;
    }

    ReceiveContext->Source->Port = NETWORK_TO_CPU16(Header->SourcePort);
    ReceiveContext->Destination->Port =
                                     NETWORK_TO_CPU16(Header->DestinationPort);

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

        NetpUdpProcessReceivedSocketData(Socket, ReceiveContext);

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

    return;
}

KSTATUS
NetpUdpProcessReceivedSocketData (
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
    PUDP_HEADER Header;
    USHORT Length;
    PNET_PACKET_BUFFER Packet;
    USHORT PayloadLength;
    KSTATUS Status;
    PUDP_RECEIVED_PACKET UdpPacket;
    PUDP_SOCKET UdpSocket;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    UdpSocket = (PUDP_SOCKET)Socket;
    Packet = ReceiveContext->Packet;
    Header = (PUDP_HEADER)(Packet->Buffer + Packet->DataOffset);
    Length = NETWORK_TO_CPU16(Header->Length);
    if (Length > (Packet->FooterOffset - Packet->DataOffset)) {
        RtlDebugPrint("Invalid UDP length %d is bigger than packet data, "
                      "which is only %d bytes large.\n",
                      Length,
                      (Packet->FooterOffset - Packet->DataOffset));

        Status = STATUS_BUFFER_TOO_SMALL;
        goto ProcessReceivedSocketDataEnd;
    }

    //
    // Since the socket has already been matched, the source and destination
    // addresses better be completely filled in.
    //

    ASSERT(ReceiveContext->Source->Port ==
           NETWORK_TO_CPU16(Header->SourcePort));

    ASSERT(ReceiveContext->Destination->Port ==
           NETWORK_TO_CPU16(Header->DestinationPort));

    //
    // Create a received packet entry for this data.
    //

    PayloadLength = Length - sizeof(UDP_HEADER);
    AllocationSize = sizeof(UDP_RECEIVED_PACKET) + PayloadLength;
    UdpPacket = MmAllocatePagedPool(AllocationSize,
                                    UDP_PROTOCOL_ALLOCATION_TAG);

    if (UdpPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ProcessReceivedSocketDataEnd;
    }

    RtlCopyMemory(&(UdpPacket->Address),
                  ReceiveContext->Source,
                  sizeof(NETWORK_ADDRESS));

    UdpPacket->DataBuffer = (PVOID)(UdpPacket + 1);
    UdpPacket->Size = PayloadLength;

    //
    // Copy the packet contents into the receive packet buffer.
    //

    RtlCopyMemory(UdpPacket->DataBuffer, Header + 1, PayloadLength);

    //
    // Work to insert the packet on the list of received packets.
    //

    KeAcquireQueuedLock(UdpSocket->ReceiveLock);
    if (UdpPacket->Size <= UdpSocket->ReceiveBufferFreeSize) {
        INSERT_BEFORE(&(UdpPacket->ListEntry),
                      &(UdpSocket->ReceivedPacketList));

        UdpSocket->ReceiveBufferFreeSize -= UdpPacket->Size;

        ASSERT(UdpSocket->ReceiveBufferFreeSize <
               UdpSocket->ReceiveBufferTotalSize);

        //
        // One packet is always enough to notify a waiting receiver.
        //

        IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_IN, TRUE);
        UdpPacket = NULL;

    } else {
        UdpSocket->DroppedPacketCount += 1;
    }

    KeReleaseQueuedLock(UdpSocket->ReceiveLock);

    //
    // If the packet wasn't nulled out, that's an indication it wasn't added to
    // the list, so free it up.
    //

    if (UdpPacket != NULL) {
        MmFreePagedPool(UdpPacket);
    }

    Status = STATUS_SUCCESS;

ProcessReceivedSocketDataEnd:
    return Status;
}

KSTATUS
NetpUdpReceive (
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
    PUDP_RECEIVED_PACKET Packet;
    PLIST_ENTRY PacketEntry;
    ULONG ReturnedEvents;
    ULONG ReturnSize;
    UINTN Size;
    KSTATUS Status;
    ULONGLONG TimeCounterFrequency;
    ULONG Timeout;
    PUDP_SOCKET UdpSocket;
    ULONG WaitTime;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    BytesComplete = 0;
    EndTime = 0;
    LockHeld = FALSE;
    Flags = Parameters->SocketIoFlags;
    Parameters->SocketIoFlags = 0;
    if ((Flags & SOCKET_IO_OUT_OF_BAND) != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto UdpReceiveEnd;
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto UdpReceiveEnd;
    }

    Packet = NULL;
    Size = Parameters->Size;
    TimeCounterFrequency = 0;
    Timeout = Parameters->TimeoutInMilliseconds;
    UdpSocket = (PUDP_SOCKET)Socket;

    //
    // Set a timeout timer to give up on. The socket stores the maximum timeout.
    //

    if (Timeout > UdpSocket->ReceiveTimeout) {
        Timeout = UdpSocket->ReceiveTimeout;
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
        // Otherwise when the read event is signalled, there is at least one
        // packet to receive.
        //

        Status = IoWaitForIoObjectState(Socket->KernelSocket.IoState,
                                        POLL_EVENT_IN,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            goto UdpReceiveEnd;
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

            goto UdpReceiveEnd;
        }

        KeAcquireQueuedLock(UdpSocket->ReceiveLock);
        LockHeld = TRUE;

        //
        // Fail with EOF if the socket has already been closed for reading.
        //

        if ((UdpSocket->ShutdownTypes & SOCKET_SHUTDOWN_READ) != 0) {
            Status = STATUS_END_OF_FILE;
            goto UdpReceiveEnd;
        }

        //
        // If another thread beat this one to the punch, try again.
        //

        if (LIST_EMPTY(&(UdpSocket->ReceivedPacketList)) != FALSE) {
            KeReleaseQueuedLock(UdpSocket->ReceiveLock);
            LockHeld = FALSE;
            continue;
        }

        //
        // This should be the first packet being read.
        //

        ASSERT(BytesComplete == 0);

        PacketEntry = UdpSocket->ReceivedPacketList.Next;
        Packet = LIST_VALUE(PacketEntry, UDP_RECEIVED_PACKET, ListEntry);
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
            goto UdpReceiveEnd;
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
                    goto UdpReceiveEnd;
                }
            }
        }

        BytesComplete = ReturnSize;

        //
        // Remove the packet if not peeking.
        //

        if ((Flags & SOCKET_IO_PEEK) == 0) {
            LIST_REMOVE(&(Packet->ListEntry));
            UdpSocket->ReceiveBufferFreeSize += Packet->Size;

            //
            // The total receive buffer size may have been decreased. Don't
            // increment the free size above the total.
            //

            if (UdpSocket->ReceiveBufferFreeSize >
                UdpSocket->ReceiveBufferTotalSize) {

                UdpSocket->ReceiveBufferFreeSize =
                                             UdpSocket->ReceiveBufferTotalSize;
            }

            MmFreePagedPool(Packet);

            //
            // Unsignal the IN event if there are no more packets.
            //

            if (LIST_EMPTY(&(UdpSocket->ReceivedPacketList)) != FALSE) {
                IoSetIoObjectState(Socket->KernelSocket.IoState,
                                   POLL_EVENT_IN,
                                   FALSE);
            }
        }

        //
        // Wait-all does not apply to UDP sockets. Break out.
        //

        Status = STATUS_SUCCESS;
        break;
    }

UdpReceiveEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(UdpSocket->ReceiveLock);
    }

    Parameters->BytesCompleted = BytesComplete;
    return Status;
}

KSTATUS
NetpUdpGetSetInformation (
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
    PNET_PACKET_SIZE_INFORMATION SizeInformation;
    ULONG SizeOption;
    PSOCKET_TIME SocketTime;
    SOCKET_TIME SocketTimeBuffer;
    PVOID Source;
    KSTATUS Status;
    PUDP_SOCKET_OPTION UdpOption;
    PUDP_SOCKET UdpSocket;

    UdpSocket = (PUDP_SOCKET)Socket;
    if ((InformationType != SocketInformationBasic) &&
        (InformationType != SocketInformationUdp)) {

        Status = STATUS_NOT_SUPPORTED;
        goto UdpGetSetInformationEnd;
    }

    //
    // Search to see if the socket option is supported by UDP.
    //

    Count = sizeof(NetUdpSocketOptions) / sizeof(NetUdpSocketOptions[0]);
    for (Index = 0; Index < Count; Index += 1) {
        UdpOption = &(NetUdpSocketOptions[Index]);
        if ((UdpOption->InformationType == InformationType) &&
            (UdpOption->Option == Option)) {

            break;
        }
    }

    if (Index == Count) {
        if (InformationType == SocketInformationBasic) {
            Status = STATUS_NOT_HANDLED;

        } else {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        }

        goto UdpGetSetInformationEnd;
    }

    //
    // Handle failure cases common to all options.
    //

    if (Set != FALSE) {
        if (UdpOption->SetAllowed == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            goto UdpGetSetInformationEnd;
        }

        if (*DataSize < UdpOption->Size) {
            *DataSize = UdpOption->Size;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto UdpGetSetInformationEnd;
        }
    }

    //
    // There are currently no UDP options.
    //

    ASSERT(InformationType != SocketInformationUdp);

    //
    // Parse the basic socket option, getting the information from the UDP
    // socket or setting the new state in the UDP socket.
    //

    Source = NULL;
    Status = STATUS_SUCCESS;
    switch ((SOCKET_BASIC_OPTION)Option) {
    case SocketBasicOptionSendBufferSize:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);

            ASSERT(UDP_MAX_PACKET_SIZE <= SOCKET_OPTION_MAX_ULONG);

            SizeInformation = &(Socket->PacketSizeInformation);
            if (SizeOption > UDP_MAX_PACKET_SIZE) {
                SizeOption = UDP_MAX_PACKET_SIZE;

            } else if (SizeOption < SizeInformation->MaxPacketSize) {
                SizeOption = SizeInformation->MaxPacketSize;
            }

            UdpSocket->MaxPacketSize = SizeOption;

        } else {
            SizeOption = UdpSocket->MaxPacketSize;
            Source = &SizeOption;
        }

        break;

    case SocketBasicOptionSendMinimum:

        ASSERT(Set == FALSE);

        SizeOption = UDP_SEND_MINIMUM;
        Source = &SizeOption;
        break;

    case SocketBasicOptionReceiveBufferSize:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);
            if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                SizeOption = SOCKET_OPTION_MAX_ULONG;
            }

            if (SizeOption < UDP_MIN_RECEIVE_BUFFER_SIZE) {
                SizeOption = UDP_MIN_RECEIVE_BUFFER_SIZE;
            }

            //
            // Set the receive buffer size and truncate the available free
            // space if necessary. Do not remove any packets that have already
            // been received. This is not meant to be a truncate call.
            //

            KeAcquireQueuedLock(UdpSocket->ReceiveLock);
            UdpSocket->ReceiveBufferTotalSize = SizeOption;
            if (UdpSocket->ReceiveBufferFreeSize > SizeOption) {
                UdpSocket->ReceiveBufferFreeSize = SizeOption;
            }

            KeReleaseQueuedLock(UdpSocket->ReceiveLock);

        } else {
            SizeOption = UdpSocket->ReceiveBufferTotalSize;
            Source = &SizeOption;
        }

        break;

    case SocketBasicOptionReceiveMinimum:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);
            if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                SizeOption = SOCKET_OPTION_MAX_ULONG;
            }

            UdpSocket->ReceiveMinimum = SizeOption;

        } else {
            Source = &SizeOption;
            SizeOption = UdpSocket->ReceiveMinimum;
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

            UdpSocket->ReceiveTimeout = (ULONG)(LONG)Milliseconds;

        } else {
            Source = &SocketTimeBuffer;
            if (UdpSocket->ReceiveTimeout == WAIT_TIME_INDEFINITE) {
                SocketTimeBuffer.Seconds = 0;
                SocketTimeBuffer.Microseconds = 0;

            } else {
                SocketTimeBuffer.Seconds = UdpSocket->ReceiveTimeout /
                                           MILLISECONDS_PER_SECOND;

                SocketTimeBuffer.Microseconds = (UdpSocket->ReceiveTimeout %
                                                 MILLISECONDS_PER_SECOND) *
                                                MICROSECONDS_PER_MILLISECOND;
            }
        }

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_HANDLED;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto UdpGetSetInformationEnd;
    }

    //
    // Truncate all copies for get requests down to the required size and only
    // return the required size on set requests.
    //

    if (*DataSize > UdpOption->Size) {
        *DataSize = UdpOption->Size;
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

        if (*DataSize < UdpOption->Size) {
            *DataSize = UdpOption->Size;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto UdpGetSetInformationEnd;
        }
    }

UdpGetSetInformationEnd:
    return Status;
}

KSTATUS
NetpUdpUserControl (
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

