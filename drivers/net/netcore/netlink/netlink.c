/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    netlink.c

Abstract:

    This module implements support for netlink sockets.

Author:

    Chris Stevens 9-Feb-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Network layer drivers are supposed to be able to stand on their own (i.e. be
// able to be implemented outside the core net library). For the builtin ones,
// avoid including netcore.h, but still redefine those functions that would
// otherwise generate imports.
//

#define NET_API DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/netlink.h>

//
// ---------------------------------------------------------------- Definitions
//

#define NETLINK_ALLOCATION_TAG 0x694C654E // 'iLeN'

//
// Define the maximum size of an netlink address string, including the null
// terminator. The longest string would look something like
// "FFFFFFFF:FFFFFFFF"
//

#define NETLINK_MAX_ADDRESS_STRING 18

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpNetlinkInitializeLink (
    PNET_LINK Link
    );

VOID
NetpNetlinkDestroyLink (
    PNET_LINK Link
    );

KSTATUS
NetpNetlinkInitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    );

KSTATUS
NetpNetlinkBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpNetlinkListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpNetlinkConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpNetlinkDisconnect (
    PNET_SOCKET Socket
    );

KSTATUS
NetpNetlinkClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetpNetlinkSend (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Destination,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride,
    PNET_PACKET_LIST PacketList
    );

VOID
NetpNetlinkProcessReceivedData (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

ULONG
NetpNetlinkPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

KSTATUS
NetpNetlinkGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

VOID
NetpNetlinkProcessReceivedPackets (
    PNET_LINK Link,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress,
    PNET_PACKET_LIST PacketList,
    PNET_PROTOCOL_ENTRY Protocol
    );

VOID
NetpNetlinkProcessReceivedSocketData (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

VOID
NetpNetlinkProcessReceivedKernelData (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

VOID
NetpNetlinkSendAck (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress,
    KSTATUS PacketStatus
    );

KSTATUS
NetpNetlinkJoinMulticastGroup (
    PNET_SOCKET Socket,
    ULONG GroupId
    );

VOID
NetpNetlinkLeaveMulticastGroup (
    PNET_SOCKET Socket,
    ULONG GroupId,
    BOOL LockHeld
    );

//
// -------------------------------------------------------------------- Globals
//

LIST_ENTRY NetNetlinkMulticastSocketList;
PSHARED_EXCLUSIVE_LOCK NetNetlinkMulticastLock;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpNetlinkInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for netlink packets.

Arguments:

    None.

Return Value:

    None.

--*/

{

    NET_NETWORK_ENTRY NetworkEntry;
    KSTATUS Status;

    INITIALIZE_LIST_HEAD(&NetNetlinkMulticastSocketList);
    NetNetlinkMulticastLock = KeCreateSharedExclusiveLock();
    if (NetNetlinkMulticastLock == NULL) {

        ASSERT(FALSE);

    }

    //
    // Register the netlink handlers with the core networking library.
    //

    NetworkEntry.Domain = NetDomainNetlink;
    NetworkEntry.ParentProtocolNumber = 0;
    NetworkEntry.Interface.InitializeLink = NetpNetlinkInitializeLink;
    NetworkEntry.Interface.DestroyLink = NetpNetlinkDestroyLink;
    NetworkEntry.Interface.InitializeSocket = NetpNetlinkInitializeSocket;
    NetworkEntry.Interface.BindToAddress = NetpNetlinkBindToAddress;
    NetworkEntry.Interface.Listen = NetpNetlinkListen;
    NetworkEntry.Interface.Connect = NetpNetlinkConnect;
    NetworkEntry.Interface.Disconnect = NetpNetlinkDisconnect;
    NetworkEntry.Interface.Close = NetpNetlinkClose;
    NetworkEntry.Interface.Send = NetpNetlinkSend;
    NetworkEntry.Interface.ProcessReceivedData = NetpNetlinkProcessReceivedData;
    NetworkEntry.Interface.PrintAddress = NetpNetlinkPrintAddress;
    NetworkEntry.Interface.GetSetInformation = NetpNetlinkGetSetInformation;
    Status = NetRegisterNetworkLayer(&NetworkEntry, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
}

KSTATUS
NetpNetlinkInitializeLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine initializes any pieces of information needed by the network
    layer for a new link.

Arguments:

    Link - Supplies a pointer to the new link.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

VOID
NetpNetlinkDestroyLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine allows the network layer to tear down any state before a link
    is destroyed.

Arguments:

    Link - Supplies a pointer to the dying link.

Return Value:

    None.

--*/

{

    return;
}

KSTATUS
NetpNetlinkInitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    )

/*++

Routine Description:

    This routine initializes any pieces of information needed by the network
    layer for the socket. The core networking library will fill in the common
    header when this routine returns.

Arguments:

    ProtocolEntry - Supplies a pointer to the protocol information.

    NetworkEntry - Supplies a pointer to the network information.

    NetworkProtocol - Supplies the raw protocol value for this socket used on
        the network. This value is network specific.

    NewSocket - Supplies a pointer to the new socket. The network layer should
        at the very least add any needed header size.

Return Value:

    Status code.

--*/

{

    ULONG MaxPacketSize;

    RtlAtomicOr32(&(NewSocket->Flags), NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED);

    //
    // Determine if the maximum IPv4 packet size plus all existing headers and
    // footers is less than the current maximum packet size. If so, truncate
    // the maximum packet size. Note that the IPv4 maximum packet size includes
    // the size of the header.
    //

    MaxPacketSize = NewSocket->PacketSizeInformation.HeaderSize +
                    NETLINK_MAX_PACKET_SIZE +
                    NewSocket->PacketSizeInformation.FooterSize;

    if (NewSocket->PacketSizeInformation.MaxPacketSize > MaxPacketSize) {
        NewSocket->PacketSizeInformation.MaxPacketSize = MaxPacketSize;
    }

    NewSocket->PacketSizeInformation.HeaderSize += NETLINK_HEADER_LENGTH;
    return STATUS_SUCCESS;
}

KSTATUS
NetpNetlinkBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    )

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

{

    ULONG BindingFlags;
    NET_LINK_LOCAL_ADDRESS LocalInformation;
    PNETLINK_ADDRESS LocalNetlinkAddress;
    PNETLINK_ADDRESS NetlinkAddress;
    KSTATUS Status;

    ASSERT(Link == NULL);

    LocalInformation.Link = NULL;
    LocalInformation.LinkAddress = NULL;
    if (Address->Domain != NetDomainNetlink) {
        Status = STATUS_NOT_SUPPORTED;
        goto NetlinkBindToAddressEnd;
    }

    //
    // If this is a kernel socket, then the only port to which it can be bound
    // is port zero. Fail if this is not the case.
    //

    BindingFlags = 0;
    NetlinkAddress = (PNETLINK_ADDRESS)Address;
    if ((Socket->Flags & NET_SOCKET_FLAG_KERNEL) != 0) {
        if (NetlinkAddress->Port != 0) {
            Status = STATUS_INVALID_PARAMETER;
            goto NetlinkBindToAddressEnd;
        }

        //
        // Make sure the binding code does not assign an ephemeral port.
        //

        BindingFlags |= NET_SOCKET_BINDING_FLAG_NO_PORT_ASSIGNMENT;
    }

    RtlCopyMemory(&(LocalInformation.LocalAddress),
                  Address,
                  sizeof(NETWORK_ADDRESS));

    //
    // Do not allow netcore to bind to a group. This would prevent
    // non-multicast packets from ever reaching this socket. Group bindings
    // are handled separately below.
    //

    LocalNetlinkAddress = (PNETLINK_ADDRESS)&(LocalInformation.LocalAddress);
    LocalNetlinkAddress->Group = 0;

    //
    // There are no "unbound" netlink sockets. The Port ID is either filled in
    // or it is zero and an ephemeral port will be assigned. Note that kernel
    // netlink sockets always have a port of zero and the binding flags dictate
    // that a port should not be assigned.
    //

    Status = NetBindSocket(Socket,
                           SocketLocallyBound,
                           &LocalInformation,
                           NULL,
                           BindingFlags);

    if (!KSUCCESS(Status)) {
        goto NetlinkBindToAddressEnd;
    }

    //
    // If the request includes being bound to a group, then add this socket to
    // the multicast group.
    //

    if (NetlinkAddress->Group != 0) {
        Status = NetpNetlinkJoinMulticastGroup(Socket, NetlinkAddress->Group);
        if (!KSUCCESS(Status)) {
            goto NetlinkBindToAddressEnd;
        }
    }

NetlinkBindToAddressEnd:
    return Status;
}

KSTATUS
NetpNetlinkListen (
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

    NETWORK_ADDRESS LocalAddress;
    KSTATUS Status;

    RtlZeroMemory(&(Socket->RemoteAddress), sizeof(NETWORK_ADDRESS));
    if (Socket->BindingType == SocketBindingInvalid) {
        RtlZeroMemory(&LocalAddress, sizeof(NETWORK_ADDRESS));
        LocalAddress.Domain = NetDomainNetlink;
        Status = NetpNetlinkBindToAddress(Socket, NULL, &LocalAddress);
        if (!KSUCCESS(Status)) {
            goto NetlinkListenEnd;
        }
    }

    Status = NetActivateSocket(Socket);
    if (!KSUCCESS(Status)) {
        goto NetlinkListenEnd;
    }

NetlinkListenEnd:
    return Status;
}

KSTATUS
NetpNetlinkConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine connects the given socket to a specific remote address. It
    will implicitly bind the socket if it is not yet locally bound.

Arguments:

    Socket - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the remote address to bind this socket to.

Return Value:

    Status code.

--*/

{

    ULONG Flags;
    NET_LINK_LOCAL_ADDRESS LocalInformation;
    KSTATUS Status;

    if (Address->Domain != NetDomainNetlink) {
        Status = STATUS_NOT_SUPPORTED;
        goto NetlinkConnectEnd;
    }

    //
    // Zero the local information. If the socket is already locally bound, it
    // will not be used. If it is not locally bound, then this will trigger a
    // ephemeral port assignment.
    //

    RtlZeroMemory(&LocalInformation, sizeof(NET_LINK_LOCAL_ADDRESS));

    //
    // If this is a kernel socket, then the only port to which it can be
    // locally bound is port zero. Make sure a local ephemeral port is not
    // assigned.
    //

    Flags = NET_SOCKET_BINDING_FLAG_ACTIVATE;
    if ((Socket->Flags & NET_SOCKET_FLAG_KERNEL) != 0) {
        Flags |= NET_SOCKET_BINDING_FLAG_NO_PORT_ASSIGNMENT;
    }

    //
    // Fully bind the socket and activate it. It's ready to receive.
    //

    Status = NetBindSocket(Socket,
                           SocketFullyBound,
                           &LocalInformation,
                           Address,
                           Flags);

    if (!KSUCCESS(Status)) {
        goto NetlinkConnectEnd;
    }

    Status = STATUS_SUCCESS;

NetlinkConnectEnd:
    return Status;
}

KSTATUS
NetpNetlinkDisconnect (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine will disconnect the given socket from its remote address.

Arguments:

    Socket - Supplies a pointer to the socket to disconnect.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpNetlinkClose (
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

    //
    // Deactivate the socket. This will most likely release a reference. There
    // should be at least one more sitting around.
    //

    ASSERT(Socket->KernelSocket.ReferenceCount > 1);

    NetDeactivateSocket(Socket);
    return STATUS_SUCCESS;
}

KSTATUS
NetpNetlinkSend (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Destination,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride,
    PNET_PACKET_LIST PacketList
    )

/*++

Routine Description:

    This routine sends data through the network.

Arguments:

    Socket - Supplies a pointer to the socket to send the data to.

    Destination - Supplies a pointer to the network address to send to.

    LinkOverride - Supplies an optional pointer to a structure that contains
        all the necessary information to send data out a link on behalf
        of the given socket.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

Return Value:

    Status code. It is assumed that either all packets are submitted (if
    success is returned) or none of the packets were submitted (if a failing
    status is returned).

--*/

{

    NetpNetlinkProcessReceivedPackets(Socket->Link,
                                      &Socket->LocalAddress,
                                      Destination,
                                      PacketList,
                                      Socket->Protocol);

    return STATUS_SUCCESS;
}

VOID
NetpNetlinkProcessReceivedData (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
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

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    //
    // A netlink packet header does not contain the protocol number. As a
    // result this routine cannot be used to process netlink packets.
    //

    ASSERT(FALSE);

    return;
}

ULONG
NetpNetlinkPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    )

/*++

Routine Description:

    This routine is called to convert a network address into a string, or
    determine the length of the buffer needed to convert an address into a
    string.

Arguments:

    Address - Supplies an optional pointer to a network address to convert to
        a string.

    Buffer - Supplies an optional pointer where the string representation of
        the address will be returned.

    BufferLength - Supplies the length of the supplied buffer, in bytes.

Return Value:

    Returns the maximum length of any address if no network address is
    supplied.

    Returns the actual length of the network address string if a network address
    was supplied, including the null terminator.

--*/

{

    ULONG Length;
    PNETLINK_ADDRESS NetlinkAddress;

    if (Address == NULL) {
        return NETLINK_MAX_ADDRESS_STRING;
    }

    ASSERT(Address->Domain == NetDomainNetlink);

    NetlinkAddress = (PNETLINK_ADDRESS)Address;

    //
    // If the group is present, print that bad boy out.
    //

    if (NetlinkAddress->Group != 0) {
        Length = RtlPrintToString(Buffer,
                                  BufferLength,
                                  CharacterEncodingDefault,
                                  "%08x:%08x",
                                  NetlinkAddress->Port,
                                  NetlinkAddress->Group);

    } else {
        Length = RtlPrintToString(Buffer,
                                  BufferLength,
                                  CharacterEncodingDefault,
                                  "%08x",
                                  NetlinkAddress->Port);
    }

    return Length;
}

KSTATUS
NetpNetlinkGetSetInformation (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

NET_API
KSTATUS
NetNetlinkSendMessage (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_MESSAGE_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine sends a netlink message, filling out the header based on the
    parameters.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to send the
        message.

    Packet - Supplies a pointer to the network packet to be sent.

    Parameters - Supplies a pointer to the message parameters.

Return Value:

    Status code.

--*/

{

    PNETLINK_HEADER Header;
    SOCKET_IO_PARAMETERS IoParameters;
    PNETLINK_ADDRESS SourceAddress;
    KSTATUS Status;

    if (Packet->DataOffset < NETLINK_HEADER_LENGTH) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto SendMessageEnd;
    }

    //
    // Fill out the message header.
    //

    Packet->DataOffset -= NETLINK_HEADER_LENGTH;
    Header = Packet->Buffer + Packet->DataOffset;
    Header->Length = Packet->FooterOffset - Packet->DataOffset;
    Header->Type = Parameters->Type;
    Header->Flags = 0;
    Header->SequenceNumber = Parameters->SequenceNumber;
    SourceAddress = (PNETLINK_ADDRESS)Parameters->SourceAddress;
    Header->PortId = SourceAddress->Port;

    //
    // Send the message to the destination address.
    //

    RtlZeroMemory(&IoParameters, sizeof(SOCKET_IO_PARAMETERS));
    IoParameters.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    IoParameters.NetworkAddress = Parameters->DestinationAddress;
    IoParameters.Size = Header->Length;
    MmSetIoBufferCurrentOffset(Packet->IoBuffer, Packet->DataOffset);
    Status = IoSocketSendData(TRUE,
                              Socket->KernelSocket.IoHandle,
                              &IoParameters,
                              Packet->IoBuffer);

    if (!KSUCCESS(Status)) {
        goto SendMessageEnd;
    }

SendMessageEnd:
    return Status;
}

NET_API
VOID
NetNetlinkRemoveSocketsFromMulticastGroups (
    ULONG ParentProtocolNumber,
    ULONG GroupOffset,
    ULONG GroupCount
    )

/*++

Routine Description:

    This routine removes any socket listening for multicast message from the
    groups specified by the offset and count. It will only match sockets for
    the given protocol.

Arguments:

    ParentProtocolNumber - Supplies the protocol number of the protocol that
        owns the given range of multicast groups.

    GroupOffset - Supplies the offset into the multicast namespace for the
        range of multicast groups from which the sockets should be removed.

    GroupCount - Supplies the number of multicast groups from which the sockets
        should be removed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONG Index;
    PNETLINK_SOCKET Socket;

    if (LIST_EMPTY(&NetNetlinkMulticastSocketList) != FALSE) {
        return;
    }

    KeAcquireSharedExclusiveLockExclusive(NetNetlinkMulticastLock);
    CurrentEntry = NetNetlinkMulticastSocketList.Next;
    while (CurrentEntry != &NetNetlinkMulticastSocketList) {
        Socket = LIST_VALUE(CurrentEntry, NETLINK_SOCKET, MulticastListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Socket->NetSocket.Protocol->ParentProtocolNumber !=
            ParentProtocolNumber) {

            continue;
        }

        for (Index = 0; Index < GroupCount; Index += 1) {
            NetpNetlinkLeaveMulticastGroup(&(Socket->NetSocket),
                                           GroupOffset + Index,
                                           TRUE);
        }
    }

    KeReleaseSharedExclusiveLockExclusive(NetNetlinkMulticastLock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpNetlinkProcessReceivedPackets (
    PNET_LINK Link,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress,
    PNET_PACKET_LIST PacketList,
    PNET_PROTOCOL_ENTRY Protocol
    )

/*++

Routine Description:

    This routine processes a list of packets, handling netlink message parsing
    and error handling that is common to all protocols.

Arguments:

    Link - Supplies a pointer to the link that received the packets.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

    PacketList - Supplies a list of packets to process.

    Protocol - Supplies a pointer to this protocol's protocol entry.

Return Value:

    None.

--*/

{

    ULONG Count;
    PNETLINK_ADDRESS Destination;
    ULONG GroupIndex;
    ULONG GroupMask;
    PULONG MulticastBitmap;
    PNETLINK_SOCKET NetlinkSocket;
    PNET_PACKET_BUFFER Packet;
    PLIST_ENTRY PacketEntry;
    PNET_SOCKET Socket;
    PLIST_ENTRY SocketEntry;

    //
    // If a group ID is supplied in the address, then send the packet to all
    // sockets listening to that multicast group. A socket must match on the
    // protocol and have its bitmap set for the group. If a port is also
    // specified in the address, do not send it to the socket with the port
    // during multicast processing; fall through and do that at the end.
    //

    Destination = (PNETLINK_ADDRESS)DestinationAddress;
    if (Destination->Group != 0) {
        PacketEntry = PacketList->Head.Next;
        while (PacketEntry != &(PacketList->Head)) {
            Packet = LIST_VALUE(PacketEntry, NET_PACKET_BUFFER, ListEntry);
            Packet->Flags |= NET_PACKET_FLAG_MULTICAST;
            GroupIndex = NETLINK_SOCKET_BITMAP_INDEX(Destination->Group);
            GroupMask = NETLINK_SOCKET_BITMAP_MASK(Destination->Group);
            KeAcquireSharedExclusiveLockShared(NetNetlinkMulticastLock);
            SocketEntry = NetNetlinkMulticastSocketList.Next;
            while (SocketEntry != &NetNetlinkMulticastSocketList) {
                NetlinkSocket = LIST_VALUE(SocketEntry,
                                             NETLINK_SOCKET,
                                             MulticastListEntry);

                Socket = &(NetlinkSocket->NetSocket);
                SocketEntry = SocketEntry->Next;
                if (Socket->Protocol != Protocol) {
                    continue;
                }

                if (Socket->LocalAddress.Port == Destination->Port) {
                    continue;
                }

                Count = NETLINK_SOCKET_BITMAP_GROUP_ID_COUNT(NetlinkSocket);
                if (Destination->Group >= Count) {
                    continue;
                }

                MulticastBitmap = NetlinkSocket->MulticastBitmap;
                if ((MulticastBitmap[GroupIndex] & GroupMask) == 0) {
                    continue;
                }

                //
                // This needs to be reconsidered if kernel sockets are signed
                // up for multicast groups. Kernel sockets are known to respond
                // to requests with multicast messages as an event notification
                // mechanism. This could potentially deadlock as the lock is
                // held during packet processing.
                //

                ASSERT((Socket->Flags & NET_SOCKET_FLAG_KERNEL) == 0);

                NetpNetlinkProcessReceivedSocketData(Link,
                                                     Socket,
                                                     Packet,
                                                     SourceAddress,
                                                     DestinationAddress);
            }

            KeReleaseSharedExclusiveLockShared(NetNetlinkMulticastLock);

            //
            // Clear out the multicast group and send it on to the socket
            // specified by the port.
            //

            Packet->Flags &= ~NET_PACKET_FLAG_MULTICAST;
            PacketEntry = PacketEntry->Next;
        }

        Destination->Group = 0;
        Socket = NULL;

        //
        // The kernel should never get any multicast packets, so just drop it
        // now before getting to the kernel.
        //

        if (Destination->Port == NETLINK_KERNEL_PORT_ID) {
            NetDestroyBufferList(PacketList);
            goto ProcessReceivedPacketsEnd;
        }
    }

    //
    // Find the socket targeted by the destination address.
    //

    Socket = NetFindSocket(Protocol, DestinationAddress, SourceAddress);
    if (Socket == NULL) {
        goto ProcessReceivedPacketsEnd;
    }

    ASSERT(Socket->Protocol == Protocol);

    //
    // Send each packet on to the protocol layer for processing. The packet
    // handling routines take ownership of a non-multicast packets and free
    // them.
    //

    while (NET_PACKET_LIST_EMPTY(PacketList) == FALSE) {
        Packet = LIST_VALUE(PacketList->Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        NET_REMOVE_PACKET_FROM_LIST(Packet, PacketList);

        ASSERT((Packet->Flags & NET_PACKET_FLAG_MULTICAST) == 0);

        NetpNetlinkProcessReceivedSocketData(Link,
                                             Socket,
                                             Packet,
                                             SourceAddress,
                                             DestinationAddress);
    }

ProcessReceivedPacketsEnd:
    if (Socket != NULL) {
        IoSocketReleaseReference(&(Socket->KernelSocket));
    }

    return;
}

VOID
NetpNetlinkProcessReceivedSocketData (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine handles received packet processing for a netlink socket.

Arguments:

    Link - Supplies a pointer to the network link that received the packet.

    Socket - Supplies a pointer to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        Use of this structure depends on its flags. If it is a multicast
        packet, then it cannot be modified by this routine. Otherwise it can
        be used as scratch space and modified.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

Return Value:

    None.

--*/

{

    PNET_PROTOCOL_ENTRY Protocol;

    //
    // Netlink handles kernel sockets differently in order to reduce code
    // duplication for error handling and message acknowledgement.
    //

    if ((Socket->Flags & NET_SOCKET_FLAG_KERNEL) != 0) {
        NetpNetlinkProcessReceivedKernelData(Link,
                                             Socket,
                                             Packet,
                                             SourceAddress,
                                             DestinationAddress);

    } else {
        Protocol = Socket->Protocol;
        Protocol->Interface.ProcessReceivedSocketData(Link,
                                                      Socket,
                                                      Packet,
                                                      SourceAddress,
                                                      DestinationAddress);
    }

    return;
}

VOID
NetpNetlinkProcessReceivedKernelData (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine is handles received packet processing for a kernel socket.

Arguments:

    Link - Supplies a pointer to the network link that received the packet.

    Socket - Supplies a pointer to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space and this routine will
        release it when it is done.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

Return Value:

    None.

--*/

{

    PNETLINK_HEADER Header;
    ULONG MessageSize;
    ULONG PacketLength;
    PNET_PROTOCOL_PROCESS_RECEIVED_SOCKET_DATA ProcessReceivedSocketData;
    PNET_PROTOCOL_ENTRY Protocol;
    KSTATUS Status;

    Protocol = Socket->Protocol;
    ProcessReceivedSocketData = Protocol->Interface.ProcessReceivedSocketData;

    //
    // Parse the packet for as many netlink messages as can be found, sending
    // each one up to the protocol.
    //

    PacketLength = Packet->FooterOffset - Packet->DataOffset;
    while (PacketLength >= NETLINK_HEADER_LENGTH) {
        Header = Packet->Buffer + Packet->DataOffset;
        Status = STATUS_SUCCESS;

        //
        // Toss any malformed messages that claim to go beyond the end of the
        // packet.
        //

        MessageSize = NETLINK_ALIGN(Header->Length);
        if (MessageSize > PacketLength) {
            MessageSize = PacketLength;
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto ProcessReceivedKernelDataNextMessage;
        }

        //
        // The kernel only handles requests.
        //

        if ((Header->Flags & NETLINK_HEADER_FLAG_REQUEST) == 0) {
            goto ProcessReceivedKernelDataNextMessage;
        }

        //
        // There is no work to do for standard messages other than replying
        // with an ACK.
        //

        if (Header->Type < NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM) {
            goto ProcessReceivedKernelDataNextMessage;
        }

        Packet->FooterOffset = Packet->DataOffset + MessageSize;
        Status = ProcessReceivedSocketData(Link,
                                           Socket,
                                           Packet,
                                           SourceAddress,
                                           DestinationAddress);

        if (!KSUCCESS(Status)) {
            goto ProcessReceivedKernelDataNextMessage;
        }

ProcessReceivedKernelDataNextMessage:

        //
        // If this message was not successfully parsed or an ACK was requested,
        // then send back an ACK or a NACK.
        //

        if (!KSUCCESS(Status) ||
            ((Header->Flags & NETLINK_HEADER_FLAG_ACK) != 0)) {

            Packet->FooterOffset = Packet->DataOffset + MessageSize;
            NetpNetlinkSendAck(Socket, Packet, SourceAddress, Status);
        }

        Packet->DataOffset += MessageSize;
        PacketLength -= MessageSize;
    }

    NetFreeBuffer(Packet);
    return;
}

VOID
NetpNetlinkSendAck (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress,
    KSTATUS PacketStatus
    )

/*++

Routine Description:

    This routine allocates, packages and sends an acknowledgement message.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to send the
        ACK.

    Packet - Supplies a pointer to the network packet that is being
        acknowledged.

    DestinationAddress - Supplies a pointer to the address of the socket that
        is to receive the ACK message.

    PacketStatus - Supplies the error status to send with the ACK.

Return Value:

    None.

--*/

{

    ULONG AckLength;
    PNET_PACKET_BUFFER AckPacket;
    ULONG CopyLength;
    PNETLINK_ERROR_MESSAGE ErrorMessage;
    NETLINK_MESSAGE_PARAMETERS Parameters;
    KSTATUS Status;

    //
    // Create the ACK packet with the appropriate error message based on the
    // given status.
    //

    AckPacket = NULL;
    CopyLength = NETLINK_HEADER_LENGTH;
    AckLength = sizeof(NETLINK_ERROR_MESSAGE);
    if (!KSUCCESS(PacketStatus)) {
        CopyLength = Packet->FooterOffset - Packet->DataOffset;
        AckLength += CopyLength - NETLINK_HEADER_LENGTH;
    }

    Status = NetAllocateBuffer(NETLINK_HEADER_LENGTH,
                               AckLength,
                               0,
                               NULL,
                               0,
                               &AckPacket);

    if (!KSUCCESS(Status)) {
        return;
    }

    ErrorMessage = AckPacket->Buffer + AckPacket->DataOffset;
    ErrorMessage->Error = (INT)PacketStatus;
    RtlCopyMemory(&(ErrorMessage->Header),
                  Packet->Buffer + Packet->DataOffset,
                  CopyLength);

    //
    // Send the ACK packet back to where the original packet came from.
    //

    Parameters.SourceAddress = &(Socket->LocalAddress);
    Parameters.DestinationAddress = DestinationAddress;
    Parameters.SequenceNumber = ErrorMessage->Header.SequenceNumber;
    Parameters.Type = NETLINK_MESSAGE_TYPE_ERROR;
    NetNetlinkSendMessage(Socket, AckPacket, &Parameters);
    NetFreeBuffer(AckPacket);
    return;
}

KSTATUS
NetpNetlinkJoinMulticastGroup (
    PNET_SOCKET Socket,
    ULONG GroupId
    )

/*++

Routine Description:

    This routine joins a socket to a multicast group by updating the socket's
    multicast group bitmap and adding the socket to the global list of socket's
    joined to multicast groups.

Arguments:

    Socket - Supplies a pointer to the socket that is requesting to join a
        multicast group.

    GroupId - Supplies the ID of the multicast group to join.

Return Value:

    Status code.

--*/

{

    ULONG AlignedGroupId;
    ULONG GroupCount;
    ULONG GroupIndex;
    ULONG GroupMask;
    PNETLINK_SOCKET NetlinkSocket;
    PULONG NewBitmap;
    ULONG NewBitmapSize;
    ULONG NewGroups;
    PULONG ReleaseBitmap;

    NetlinkSocket = (PNETLINK_SOCKET)Socket;
    NewBitmap = NULL;
    NewBitmapSize = 0;
    NewGroups = 0;

    //
    // Expand the bitmap if necessary. The group ID should have been validated
    // by the protocol layer before reaching this point in the stack.
    //

    GroupCount = NETLINK_SOCKET_BITMAP_GROUP_ID_COUNT(NetlinkSocket);
    if (GroupId >= GroupCount) {
        AlignedGroupId = ALIGN_RANGE_UP(GroupId + 1,
                                        (sizeof(ULONG) * BITS_PER_BYTE));

        NewGroups = AlignedGroupId - GroupCount;
        NewBitmapSize = NetlinkSocket->MulticastBitmapSize +
                        (NewGroups / BITS_PER_BYTE);

        NewBitmap = MmAllocatePagedPool(NewBitmapSize, NETLINK_ALLOCATION_TAG);
        if (NewBitmap == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    GroupIndex = NETLINK_SOCKET_BITMAP_INDEX(GroupId);
    GroupMask = NETLINK_SOCKET_BITMAP_MASK(GroupId);
    KeAcquireSharedExclusiveLockExclusive(NetNetlinkMulticastLock);
    if (GroupId >= NETLINK_SOCKET_BITMAP_GROUP_ID_COUNT(NetlinkSocket)) {

        ASSERT(NewBitmapSize > NetlinkSocket->MulticastBitmapSize);

        RtlCopyMemory(NewBitmap,
                      NetlinkSocket->MulticastBitmap,
                      NetlinkSocket->MulticastBitmapSize);

        RtlZeroMemory((PVOID)NewBitmap + NetlinkSocket->MulticastBitmapSize,
                      (NewGroups / BITS_PER_BYTE));

        ReleaseBitmap = NetlinkSocket->MulticastBitmap;
        NetlinkSocket->MulticastBitmap = NewBitmap;
        NetlinkSocket->MulticastBitmapSize = NewBitmapSize;

    } else {
        ReleaseBitmap = NewBitmap;
    }

    if ((NetlinkSocket->MulticastBitmap[GroupIndex] & GroupMask) == 0) {
        NetlinkSocket->MulticastBitmap[GroupIndex] |= GroupMask;
        NetlinkSocket->MulticastGroupCount += 1;
        if (NetlinkSocket->MulticastListEntry.Next == NULL) {
            INSERT_AFTER(&(NetlinkSocket->MulticastListEntry),
                         &NetNetlinkMulticastSocketList);
        }
    }

    ASSERT(NetlinkSocket->MulticastListEntry.Next != NULL);

    KeReleaseSharedExclusiveLockExclusive(NetNetlinkMulticastLock);
    if (ReleaseBitmap != NULL) {
        MmFreePagedPool(ReleaseBitmap);
    }

    return STATUS_SUCCESS;
}

VOID
NetpNetlinkLeaveMulticastGroup (
    PNET_SOCKET Socket,
    ULONG GroupId,
    BOOL LockHeld
    )

/*++

Routine Description:

    This routine removes a socket from a multicast group.

Arguments:

    Socket - Supplies a pointer to the socket to remove from the group.

    GroupId - Supplies the ID of the multicast group to leave.

    LockHeld - Supplies a boolean indicating whether or not the global
        multicast lock is already held.

Return Value:

    None.

--*/

{

    ULONG GroupIndex;
    ULONG GroupMask;
    PNETLINK_SOCKET NetlinkSocket;

    NetlinkSocket = (PNETLINK_SOCKET)Socket;
    if (GroupId >= NETLINK_SOCKET_BITMAP_GROUP_ID_COUNT(NetlinkSocket)) {
        return;
    }

    GroupIndex = NETLINK_SOCKET_BITMAP_INDEX(GroupId);
    GroupMask = NETLINK_SOCKET_BITMAP_MASK(GroupId);
    if (LockHeld == FALSE) {
        KeAcquireSharedExclusiveLockExclusive(NetNetlinkMulticastLock);
    }

    if ((NetlinkSocket->MulticastBitmap[GroupIndex] & GroupMask) != 0) {
        NetlinkSocket->MulticastBitmap[GroupIndex] &= ~GroupMask;
        NetlinkSocket->MulticastGroupCount -= 1;
        if (NetlinkSocket->MulticastGroupCount == 0) {
            LIST_REMOVE(&(NetlinkSocket->MulticastListEntry));
            NetlinkSocket->MulticastListEntry.Next = NULL;
        }
    }

    if (LockHeld == FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetNetlinkMulticastLock);
    }

    return;
}

