/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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
// The raw protocol is an exception to the rule and is built into the
// networking core library. As such, directly include netcore.h.
//

#include <minoca/driver.h>
#include "netcore.h"

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
// Define the maximum packet size allowed on raw socket.
//

#define RAW_MAX_PACKET_SIZE MAX_ULONG

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

    ReceiveLock - Stores the spin lock that protects the received packets list,
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
        before the socket is made readable.

    DroppedPacketCount - Stores the number of packets that have been dropped
        because the receive queue was full.

    ShutdownTypes - Stores the mask of shutdowns that have occurred on this
        socket.

    MaxPacketSize - Stores the maximum size of RAW datagrams.

--*/

typedef struct _RAW_SOCKET {
    NET_SOCKET NetSocket;
    LIST_ENTRY ReceivedPacketList;
    KSPIN_LOCK ReceiveLock;
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

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpRawCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket
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
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress,
    PNET_PROTOCOL_ENTRY ProtocolEntry
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
    SocketTypeRaw,
    SOCKET_INTERNET_PROTOCOL_RAW,
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
        NetpRawReceive,
        NetpRawGetSetInformation,
        NetpRawUserControl
    }
};

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

    Status = NetRegisterProtocol(&NetRawProtocol);
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
    PNET_SOCKET *NewSocket
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
        structure after this routine returns.

Return Value:

    Status code.

--*/

{

    PRAW_SOCKET RawSocket;
    KSTATUS Status;

    ASSERT(ProtocolEntry->Type == SocketTypeRaw);
    ASSERT(ProtocolEntry->ParentProtocolNumber == SOCKET_INTERNET_PROTOCOL_RAW);

    RawSocket = MmAllocatePagedPool(sizeof(RAW_SOCKET),
                                    RAW_PROTOCOL_ALLOCATION_TAG);

    if (RawSocket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RawCreateSocketEnd;
    }

    RtlZeroMemory(RawSocket, sizeof(RAW_SOCKET));
    RawSocket->NetSocket.KernelSocket.Protocol = NetworkProtocol;
    RawSocket->NetSocket.KernelSocket.ReferenceCount = 1;
    INITIALIZE_LIST_HEAD(&(RawSocket->ReceivedPacketList));
    KeInitializeSpinLock(&(RawSocket->ReceiveLock));
    RawSocket->ReceiveTimeout = WAIT_TIME_INDEFINITE;
    RawSocket->ReceiveBufferTotalSize = RAW_DEFAULT_RECEIVE_BUFFER_SIZE;
    RawSocket->ReceiveBufferFreeSize = RawSocket->ReceiveBufferTotalSize;
    RawSocket->ReceiveMinimum = RAW_DEFAULT_RECEIVE_MINIMUM;
    RawSocket->MaxPacketSize = RAW_MAX_PACKET_SIZE;

    //
    // Give the lower layers a chance to initialize. Start the maximum packet
    // size at the largest possible value.
    //

    ASSERT(RAW_MAX_PACKET_SIZE == MAX_ULONG);

    RawSocket->NetSocket.MaxPacketSize = RAW_MAX_PACKET_SIZE;
    Status = NetworkEntry->Interface.InitializeSocket(ProtocolEntry,
                                                      NetworkEntry,
                                                      NetworkProtocol,
                                                      &(RawSocket->NetSocket));

    if (!KSUCCESS(Status)) {
        goto RawCreateSocketEnd;
    }

    //
    // Broadcast is disabled by default on RAW sockets.
    //

    RawSocket->NetSocket.Flags |= NET_SOCKET_FLAG_BROADCAST_DISABLED;
    Status = STATUS_SUCCESS;

RawCreateSocketEnd:
    if (!KSUCCESS(Status)) {
        if (RawSocket != NULL) {
            MmFreePagedPool(RawSocket);
            RawSocket = NULL;
        }
    }

    *NewSocket = &(RawSocket->NetSocket);
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

    KeAcquireSpinLock(&(RawSocket->ReceiveLock));
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

    KeReleaseSpinLock(&(RawSocket->ReceiveLock));
    MmFreePagedPool(RawSocket);
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

    ULONG OriginalPort;
    PRAW_SOCKET RawSocket;
    KSTATUS Status;

    RawSocket = (PRAW_SOCKET)Socket;

    //
    // Allow raw sockets to get bound multiple times, unless they are already
    // connected to a peer address. They get bound to the any address upon
    // creation.
    //

    if (Socket->RemoteAddress.Network != SocketNetworkInvalid) {
        Status = STATUS_INVALID_PARAMETER;
        goto RawBindToAddressEnd;
    }

    //
    // Currently only IPv4 addresses are supported.
    //

    if (Address->Network != SocketNetworkIp4) {
        Status = STATUS_NOT_SUPPORTED;
        goto RawBindToAddressEnd;
    }

    //
    // The port doesn't make a different on raw sockets, so zero it out.
    //

    OriginalPort = Address->Port;
    Address->Port = 0;

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.BindToAddress(Socket, Link, Address);
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

    IoSetIoObjectState(RawSocket->NetSocket.KernelSocket.IoState,
                       POLL_EVENT_OUT,
                       TRUE);

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
    PRAW_SOCKET RawSocket;
    KSTATUS Status;

    RawSocket = (PRAW_SOCKET)Socket;

    //
    // The port doesn't make a different on raw sockets, so zero it out.
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

    IoSetIoObjectState(RawSocket->NetSocket.KernelSocket.IoState,
                       POLL_EVENT_OUT,
                       TRUE);

    Status = STATUS_SUCCESS;

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

    RawSocket = (PRAW_SOCKET)Socket;
    RtlAtomicOr32(&(RawSocket->ShutdownTypes), ShutdownType);

    //
    // Signal the read event if the read end was shut down.
    //

    if ((ShutdownType & SOCKET_SHUTDOWN_READ) != 0) {
        KeAcquireSpinLock(&(RawSocket->ReceiveLock));
        IoSetIoObjectState(RawSocket->NetSocket.KernelSocket.IoState,
                           POLL_EVENT_IN,
                           TRUE);

        KeReleaseSpinLock(&(RawSocket->ReceiveLock));
    }

    if ((ShutdownType & SOCKET_SHUTDOWN_WRITE) != 0) {
        IoSetIoObjectState(RawSocket->NetSocket.KernelSocket.IoState,
                           POLL_EVENT_OUT,
                           TRUE);
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
    LIST_ENTRY PacketListHead;
    PRAW_SOCKET RawSocket;
    UINTN Size;
    KSTATUS Status;

    BytesComplete = 0;
    Flags = Parameters->SocketIoFlags;
    Parameters->SocketIoFlags = 0;
    Link = NULL;
    LinkInformation.Link = NULL;
    LinkOverride = NULL;
    INITIALIZE_LIST_HEAD(&PacketListHead);
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
        (Destination->Network == SocketNetworkInvalid)) {

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
        if (KSUCCESS(Status)) {

            //
            // Synchronously get the correct header, footer, and max packet
            // sizes.
            //

            Status = NetInitializeSocketLinkOverride(Socket,
                                                     &LinkInformation,
                                                     &LinkOverrideBuffer);

            if (KSUCCESS(Status)) {
                LinkOverride = &LinkOverrideBuffer;
            }
        }

        if (!KSUCCESS(Status) && (Status != STATUS_CONNECTION_EXISTS)) {
            goto RawSendEnd;
        }
    }

    //
    // Set the necessary local variables based on whether the socket's link or
    // an override link will be used to send the data.
    //

    if (LinkOverride != NULL) {

        ASSERT(LinkOverride == &LinkOverrideBuffer);

        Link = LinkOverrideBuffer.LinkInformation.Link;
        HeaderSize = LinkOverrideBuffer.HeaderSize;
        FooterSize = LinkOverrideBuffer.FooterSize;

    } else {

        ASSERT(Socket->Link != NULL);

        Link = Socket->Link;
        HeaderSize = Socket->HeaderSize;
        FooterSize = Socket->FooterSize;
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

    INSERT_BEFORE(&(Packet->ListEntry), &PacketListHead);

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
                                             &PacketListHead);

    if (!KSUCCESS(Status)) {
        goto RawSendEnd;
    }

    Packet = NULL;
    BytesComplete = Size;

RawSendEnd:
    Parameters->Size = BytesComplete;
    if (!KSUCCESS(Status)) {
        while (LIST_EMPTY(&PacketListHead) == FALSE) {
            Packet = LIST_VALUE(PacketListHead.Next,
                                NET_PACKET_BUFFER,
                                ListEntry);

            LIST_REMOVE(&(Packet->ListEntry));
            NetFreeBuffer(Packet);
        }
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
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress,
    PNET_PROTOCOL_ENTRY ProtocolEntry
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    Link - Supplies a pointer to the link that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

    ProtocolEntry - Supplies a pointer to this protocol's protocol entry.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    //
    // No incoming packets should match the raw socket protocol. Raw sockets
    // receive data directly from net core and not their networking layer.
    //

    ASSERT(FALSE);

    return;
}

VOID
NetpRawSocketProcessReceivedData (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress
    )

/*++

Routine Description:

    This routine is called to process a received packet for a particular raw
    socket.

Arguments:

    Socket - Supplies a pointer to the socket that is meant to receive this
        data.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        The packet is not modified by the routine.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

Return Value:

    None.

--*/

{

    ULONG AllocationSize;
    ULONG AvailableBytes;
    ULONG Length;
    PRAW_RECEIVED_PACKET RawPacket;
    PRAW_SOCKET RawSocket;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Socket != NULL);

    RawSocket = (PRAW_SOCKET)Socket;
    Length = Packet->FooterOffset - Packet->DataOffset;

    //
    // Create a received packet entry for this data.
    //

    AllocationSize = sizeof(RAW_RECEIVED_PACKET) + Length;
    RawPacket = MmAllocatePagedPool(AllocationSize,
                                    RAW_PROTOCOL_ALLOCATION_TAG);

    if (RawPacket == NULL) {
        return;
    }

    RtlCopyMemory(&(RawPacket->Address),
                  SourceAddress,
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

    KeAcquireSpinLock(&(RawSocket->ReceiveLock));
    if (RawPacket->Size <= RawSocket->ReceiveBufferFreeSize) {
        INSERT_BEFORE(&(RawPacket->ListEntry),
                      &(RawSocket->ReceivedPacketList));

        RawSocket->ReceiveBufferFreeSize -= RawPacket->Size;

        ASSERT(RawSocket->ReceiveBufferFreeSize <
               RawSocket->ReceiveBufferTotalSize);

        //
        // Signal the event if enough bytes have been received.
        //

        AvailableBytes = RawSocket->ReceiveBufferTotalSize -
                         RawSocket->ReceiveBufferFreeSize;

        if (AvailableBytes >= RawSocket->ReceiveMinimum) {
            IoSetIoObjectState(RawSocket->NetSocket.KernelSocket.IoState,
                               POLL_EVENT_IN,
                               TRUE);
        }

        RawPacket = NULL;

    } else {
        RawSocket->DroppedPacketCount += 1;
    }

    KeReleaseSpinLock(&(RawSocket->ReceiveLock));

    //
    // If the packet wasn't nulled out, that's an indication it wasn't added to
    // the list, so free it up.
    //

    if (RawPacket != NULL) {
        MmFreePagedPool(RawPacket);
    }

    return;
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
    PIO_OBJECT_STATE IoState;
    PRAW_RECEIVED_PACKET Packet;
    PLIST_ENTRY PacketEntry;
    PRAW_SOCKET RawSocket;
    KSTATUS RemoteCopyStatus;
    ULONG ReturnedEvents;
    UINTN Size;
    KSTATUS Status;
    ULONGLONG TimeCounterFrequency;
    ULONG Timeout;
    ULONG WaitTime;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    BytesComplete = 0;
    EndTime = 0;
    Parameters->SocketIoFlags = 0;
    Packet = NULL;
    Size = Parameters->Size;
    TimeCounterFrequency = 0;
    Timeout = Parameters->TimeoutInMilliseconds;
    RawSocket = (PRAW_SOCKET)Socket;

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
    // Loop trying to get some data.
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
        // Otherwise when the read event is signalled, there are at least the
        // receive minimum bytes available.
        //

        IoState = RawSocket->NetSocket.KernelSocket.IoState;
        Status = IoWaitForIoObjectState(IoState,
                                        POLL_EVENT_IN,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            break;
        }

        if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
            if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                Status = STATUS_NO_NETWORK_CONNECTION;

            } else {
                Status = NET_SOCKET_GET_LAST_ERROR(&(RawSocket->NetSocket));
                if (KSUCCESS(Status)) {
                    Status = STATUS_DEVICE_IO_ERROR;
                }
            }

            break;
        }

        KeAcquireSpinLock(&(RawSocket->ReceiveLock));

        //
        // Fail with EOF if the socket has already been closed for reading.
        //

        if ((RawSocket->ShutdownTypes & SOCKET_SHUTDOWN_READ) != 0) {
            KeReleaseSpinLock(&(RawSocket->ReceiveLock));
            Status = STATUS_END_OF_FILE;
            goto RawReceiveEnd;
        }

        //
        // If there is data to read, then read it. This routine can only get
        // here if the receive minimum bytes have become available, which means
        // it is open season to read until all the packets are gone.
        //

        if (LIST_EMPTY(&(RawSocket->ReceivedPacketList)) == FALSE) {
            PacketEntry = RawSocket->ReceivedPacketList.Next;
            LIST_REMOVE(PacketEntry);
            Packet = LIST_VALUE(PacketEntry, RAW_RECEIVED_PACKET, ListEntry);
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

            //
            // If the list is now empty, unsignal the event.
            //

            if (LIST_EMPTY(&(RawSocket->ReceivedPacketList)) != FALSE) {
                IoSetIoObjectState(RawSocket->NetSocket.KernelSocket.IoState,
                                   POLL_EVENT_IN,
                                   FALSE);
            }
        }

        KeReleaseSpinLock(&(RawSocket->ReceiveLock));

        //
        // If a packet was grabbed, return it.
        //

        if (Packet != NULL) {
            Status = STATUS_SUCCESS;
            Packet = LIST_VALUE(PacketEntry, RAW_RECEIVED_PACKET, ListEntry);
            CopySize = Packet->Size;
            if (Size < CopySize) {
                CopySize = Size;
                Status = STATUS_BUFFER_TOO_SMALL;
                Parameters->SocketIoFlags |= SOCKET_IO_DATA_TRUNCATED;
            }

            Status = MmCopyIoBufferData(IoBuffer,
                                        Packet->DataBuffer,
                                        BytesComplete,
                                        CopySize,
                                        TRUE);

            if (KSUCCESS(Status)) {
                BytesComplete += CopySize;
            }

            //
            // If the caller wants the remote address, copy it over, regardless
            // of whether the main copy succeeded or not.
            //

            if (Parameters->NetworkAddress != NULL) {
                if (FromKernelMode != FALSE) {
                    RtlCopyMemory(Parameters->NetworkAddress,
                                  &(Packet->Address),
                                  sizeof(NETWORK_ADDRESS));

                } else {
                    RemoteCopyStatus = MmCopyToUserMode(
                                                    Parameters->NetworkAddress,
                                                    &(Packet->Address),
                                                    sizeof(NETWORK_ADDRESS));

                    if (!KSUCCESS(RemoteCopyStatus)) {
                        Status = RemoteCopyStatus;
                    }
                }
            }

            //
            // Release the packet and break out of this loop. If this loop
            // were made not to break, the failing status would need to be
            // dealt with here.
            //

            MmFreePagedPool(Packet);
            break;
        }
    }

RawReceiveEnd:
    Parameters->Size = BytesComplete;
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

--*/

{

    SOCKET_BASIC_OPTION BasicOption;
    PBOOL BooleanOption;
    PRAW_SOCKET RawSocket;
    PULONG SizeOption;
    KSTATUS Status;
    PULONG TimeoutOption;

    RawSocket = (PRAW_SOCKET)Socket;
    if ((InformationType != SocketInformationTypeBasic) &&
        (InformationType != SocketInformationTypeUdp)) {

        Status = STATUS_INVALID_PARAMETER;
        goto RawGetSetInformationEnd;
    }

    Status = STATUS_SUCCESS;
    if (InformationType == SocketInformationTypeBasic) {
        BasicOption = (SOCKET_BASIC_OPTION)Option;
        switch (BasicOption) {
        case SocketBasicOptionSendBufferSize:
            if (Set != FALSE) {
                if (*DataSize != sizeof(ULONG)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                SizeOption = (PULONG)Data;
                if (*SizeOption > RAW_MAX_PACKET_SIZE) {
                    RawSocket->MaxPacketSize = RAW_MAX_PACKET_SIZE;

                } else if (*SizeOption < Socket->MaxPacketSize) {
                    RawSocket->MaxPacketSize = Socket->MaxPacketSize;

                } else {
                    RawSocket->MaxPacketSize = *SizeOption;
                }

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                SizeOption = (PULONG)Data;
                *SizeOption = RawSocket->MaxPacketSize;
            }

            break;

        case SocketBasicOptionSendMinimum:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                SizeOption = (PULONG)Data;
                *SizeOption = RAW_SEND_MINIMUM;
            }

            break;

        case SocketBasicOptionSendTimeout:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                TimeoutOption = (PULONG)Data;
                *TimeoutOption = WAIT_TIME_INDEFINITE;
            }

            break;

        case SocketBasicOptionReceiveBufferSize:
            if (Set != FALSE) {
                if (*DataSize != sizeof(ULONG)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                SizeOption = (PULONG)Data;
                KeAcquireSpinLock(&(RawSocket->ReceiveLock));
                if (*SizeOption < RAW_MIN_RECEIVE_BUFFER_SIZE) {
                    RawSocket->ReceiveBufferTotalSize =
                                                   RAW_MIN_RECEIVE_BUFFER_SIZE;

                } else {
                    RawSocket->ReceiveBufferTotalSize = *SizeOption;
                }

                //
                // Truncate the available free space if necessary. Do not
                // remove any packets that have already been received. This is
                // not meant to be a truncate call.
                //

                if (RawSocket->ReceiveBufferFreeSize >
                    RawSocket->ReceiveBufferTotalSize) {

                    RawSocket->ReceiveBufferFreeSize =
                                             RawSocket->ReceiveBufferTotalSize;
                }

                KeReleaseSpinLock(&(RawSocket->ReceiveLock));

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                SizeOption = (PULONG)Data;
                *SizeOption = RawSocket->ReceiveBufferTotalSize;
            }

            break;

        case SocketBasicOptionReceiveMinimum:
            if (Set != FALSE) {
                if (*DataSize != sizeof(ULONG)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                SizeOption = (PULONG)Data;
                RawSocket->ReceiveMinimum = *SizeOption;

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                SizeOption = (PULONG)Data;
                *SizeOption = RawSocket->ReceiveMinimum;
            }

            break;

        case SocketBasicOptionReceiveTimeout:
            if (Set != FALSE) {
                if (*DataSize != sizeof(ULONG)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                TimeoutOption = (PULONG)Data;
                RawSocket->ReceiveTimeout = *TimeoutOption;

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                TimeoutOption = (PULONG)Data;
                *TimeoutOption = RawSocket->ReceiveTimeout;
            }

            break;

        case SocketBasicOptionAcceptConnections:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;

            } else {
                if (*DataSize < sizeof(BOOL)) {
                    *DataSize = sizeof(BOOL);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                *BooleanOption = FALSE;
            }

            break;

        case SocketBasicOptionBroadcastEnabled:
            if (Set != FALSE) {
                if (*DataSize != sizeof(BOOL)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                if (*BooleanOption != FALSE) {
                    RtlAtomicAnd32(&(Socket->Flags),
                                   ~NET_SOCKET_FLAG_BROADCAST_DISABLED);

                } else {
                    RtlAtomicOr32(&(Socket->Flags),
                                  NET_SOCKET_FLAG_BROADCAST_DISABLED);
                }

            } else {
                if (*DataSize < sizeof(BOOL)) {
                    *DataSize = sizeof(BOOL);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                if ((Socket->Flags & NET_SOCKET_FLAG_BROADCAST_DISABLED) == 0) {
                    *BooleanOption = TRUE;

                } else {
                    *BooleanOption = FALSE;
                }
            }

            break;

        default:
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

    } else {

        ASSERT(InformationType == SocketInformationTypeRaw);

        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
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

