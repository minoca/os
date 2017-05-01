/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#define NET_API __DLLEXPORT

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
NetlinkpInitializeLink (
    PNET_LINK Link
    );

VOID
NetlinkpDestroyLink (
    PNET_LINK Link
    );

KSTATUS
NetlinkpInitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    );

KSTATUS
NetlinkpBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address,
    ULONG Flags
    );

KSTATUS
NetlinkpListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetlinkpConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetlinkpDisconnect (
    PNET_SOCKET Socket
    );

KSTATUS
NetlinkpClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetlinkpSend (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Destination,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride,
    PNET_PACKET_LIST PacketList
    );

VOID
NetlinkpProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

ULONG
NetlinkpPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

KSTATUS
NetlinkpGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

VOID
NetlinkpProcessReceivedPackets (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PNET_PACKET_LIST PacketList
    );

VOID
NetlinkpProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

VOID
NetlinkpProcessReceivedKernelData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

VOID
NetlinkpBroadcastPackets (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PNET_PACKET_LIST PacketList
    );

VOID
NetlinkpSendAck (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress,
    KSTATUS PacketStatus
    );

VOID
NetlinkpLeaveMulticastGroup (
    PNET_SOCKET Socket,
    ULONG GroupId,
    BOOL LockHeld
    );

//
// -------------------------------------------------------------------- Globals
//

LIST_ENTRY NetlinkMulticastSocketList;
PSHARED_EXCLUSIVE_LOCK NetlinkMulticastLock;

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

    INITIALIZE_LIST_HEAD(&NetlinkMulticastSocketList);
    NetlinkMulticastLock = KeCreateSharedExclusiveLock();
    if (NetlinkMulticastLock == NULL) {

        ASSERT(FALSE);

    }

    //
    // Register the netlink handlers with the core networking library.
    //

    RtlZeroMemory(&NetworkEntry, sizeof(NET_NETWORK_ENTRY));
    NetworkEntry.Domain = NetDomainNetlink;
    NetworkEntry.ParentProtocolNumber = INVALID_PROTOCOL_NUMBER;
    NetworkEntry.Interface.InitializeLink = NetlinkpInitializeLink;
    NetworkEntry.Interface.DestroyLink = NetlinkpDestroyLink;
    NetworkEntry.Interface.InitializeSocket = NetlinkpInitializeSocket;
    NetworkEntry.Interface.BindToAddress = NetlinkpBindToAddress;
    NetworkEntry.Interface.Listen = NetlinkpListen;
    NetworkEntry.Interface.Connect = NetlinkpConnect;
    NetworkEntry.Interface.Disconnect = NetlinkpDisconnect;
    NetworkEntry.Interface.Close = NetlinkpClose;
    NetworkEntry.Interface.Send = NetlinkpSend;
    NetworkEntry.Interface.ProcessReceivedData = NetlinkpProcessReceivedData;
    NetworkEntry.Interface.PrintAddress = NetlinkpPrintAddress;
    NetworkEntry.Interface.GetSetInformation = NetlinkpGetSetInformation;
    Status = NetRegisterNetworkLayer(&NetworkEntry, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
}

KSTATUS
NetlinkpInitializeLink (
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
NetlinkpDestroyLink (
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
NetlinkpInitializeSocket (
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
    PNETLINK_SOCKET NetlinkSocket;

    //
    // The protocol interface must supply a few routines.
    //

    NetlinkSocket = (PNETLINK_SOCKET)NewSocket;
    if (NetlinkSocket->ProtocolInterface.JoinMulticastGroup == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Assume the header is always included on a netlink socket. It is
    // essentially a raw network.
    //

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
NetlinkpBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address,
    ULONG Flags
    )

/*++

Routine Description:

    This routine binds the given socket to the specified network address.

Arguments:

    Socket - Supplies a pointer to the socket to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

    Flags - Supplies a bitmask of binding flags. See NET_SOCKET_BINDING_FLAG_*
        for definitions.

Return Value:

    Status code.

--*/

{

    ULONG BindingFlags;
    PNETWORK_ADDRESS LocalAddress;
    NET_LINK_LOCAL_ADDRESS LocalInformation;
    PNETLINK_ADDRESS LocalNetlinkAddress;
    PNETLINK_ADDRESS NetlinkAddress;
    PNETLINK_SOCKET NetlinkSocket;
    KSTATUS Status;

    ASSERT(Link == NULL);

    LocalInformation.Link = NULL;
    LocalInformation.LinkAddress = NULL;
    if (Address->Domain != NetDomainNetlink) {
        Status = STATUS_NOT_SUPPORTED;
        goto BindToAddressEnd;
    }

    //
    // If the socket is not locally or fully bound, then make an attempt at
    // locally binding it.
    //

    NetlinkAddress = (PNETLINK_ADDRESS)Address;
    if ((Socket->BindingType != SocketLocallyBound) &&
        (Socket->BindingType != SocketFullyBound)) {

        //
        // If this is a kernel socket, then the only port to which it can be
        // bound is port zero. Fail if this is not the case.
        //

        BindingFlags = Flags;
        if ((Socket->Flags & NET_SOCKET_FLAG_KERNEL) != 0) {
            if (NetlinkAddress->Port != 0) {
                Status = STATUS_INVALID_PARAMETER;
                goto BindToAddressEnd;
            }

            //
            // Make sure the binding code does not assign an ephemeral port.
            //

            BindingFlags |= NET_SOCKET_BINDING_FLAG_NO_PORT_ASSIGNMENT;
        }

        LocalAddress = &(LocalInformation.ReceiveAddress);
        RtlCopyMemory(LocalAddress, Address, sizeof(NETWORK_ADDRESS));

        //
        // Do not allow netcore to bind to a group. This would prevent
        // non-multicast packets from ever reaching this socket. Group bindings
        // are handled separately below.
        //

        LocalNetlinkAddress = (PNETLINK_ADDRESS)LocalAddress;
        LocalNetlinkAddress->Group = 0;

        //
        // For netlink sockets, the receive and send addresses are always the
        // same.
        //

        RtlCopyMemory(&(LocalInformation.SendAddress),
                      LocalAddress,
                      sizeof(NETWORK_ADDRESS));

        //
        // There are no "unbound" netlink sockets. The Port ID is either filled
        // in or it is zero and an ephemeral port will be assigned. Note that
        // kernel netlink sockets always have a port of zero and the binding
        // flags dictate that a port should not be assigned.
        //

        Status = NetBindSocket(Socket,
                               SocketLocallyBound,
                               &LocalInformation,
                               NULL,
                               BindingFlags);

        if (!KSUCCESS(Status)) {
            goto BindToAddressEnd;
        }

    //
    // Otherwise this must just be an attempt to join a multicast group. The
    // address port must match the socket's local address's port.
    //

    } else {
        if (Socket->LocalReceiveAddress.Port != Address->Port) {
            Status = STATUS_INVALID_PARAMETER;
            goto BindToAddressEnd;
        }

        ASSERT(Socket->LocalSendAddress.Port == Address->Port);
    }

    //
    // If the request includes being bound to a group, then add this socket to
    // the multicast group.
    //

    if (NetlinkAddress->Group != 0) {
        NetlinkSocket = (PNETLINK_SOCKET)Socket;
        Status = NetlinkSocket->ProtocolInterface.JoinMulticastGroup(
                                                        Socket,
                                                        NetlinkAddress->Group);

        if (!KSUCCESS(Status)) {
            goto BindToAddressEnd;
        }
    }

BindToAddressEnd:
    return Status;
}

KSTATUS
NetlinkpListen (
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
        Status = NetlinkpBindToAddress(Socket, NULL, &LocalAddress, 0);
        if (!KSUCCESS(Status)) {
            goto ListenEnd;
        }
    }

    Status = NetActivateSocket(Socket);
    if (!KSUCCESS(Status)) {
        goto ListenEnd;
    }

ListenEnd:
    return Status;
}

KSTATUS
NetlinkpConnect (
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
        goto ConnectEnd;
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
        goto ConnectEnd;
    }

    Status = STATUS_SUCCESS;

ConnectEnd:
    return Status;
}

KSTATUS
NetlinkpDisconnect (
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
NetlinkpClose (
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

    PNETLINK_SOCKET NetlinkSocket;

    //
    // Deactivate the socket. This will most likely release a reference. There
    // should be at least one more sitting around.
    //

    ASSERT(Socket->KernelSocket.ReferenceCount > 1);

    //
    // If the socket is a member of any multicast groups, just remove it from
    // the list of sockets responding to multicast messages.
    //

    NetlinkSocket = (PNETLINK_SOCKET)Socket;
    if (NetlinkSocket->MulticastListEntry.Next != NULL) {
        KeAcquireSharedExclusiveLockExclusive(NetlinkMulticastLock);
        LIST_REMOVE(&(NetlinkSocket->MulticastListEntry));
        NetlinkSocket->MulticastListEntry.Next = NULL;
        KeReleaseSharedExclusiveLockExclusive(NetlinkMulticastLock);
    }

    NetDeactivateSocket(Socket);
    return STATUS_SUCCESS;
}

KSTATUS
NetlinkpSend (
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

    NET_RECEIVE_CONTEXT ReceiveContext;

    ReceiveContext.Link = Socket->Link;
    ReceiveContext.Protocol = Socket->Protocol;
    ReceiveContext.Network = Socket->Network;
    ReceiveContext.Source = &(Socket->LocalSendAddress);
    ReceiveContext.Destination = Destination;
    ReceiveContext.ParentProtocolNumber =
                                        Socket->Protocol->ParentProtocolNumber;

    NetlinkpProcessReceivedPackets(&ReceiveContext, PacketList);
    return STATUS_SUCCESS;
}

VOID
NetlinkpProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link and packet information.

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
NetlinkpPrintAddress (
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
NetlinkpGetSetInformation (
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

    DataSize - Supplies a pointer that on input contains the size of the data
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

    ULONG GroupId;
    NETLINK_SOCKET_OPTION NetlinkOption;
    PNETLINK_SOCKET NetlinkSocket;
    UINTN RequiredSize;
    PVOID Source;
    KSTATUS Status;

    NetlinkSocket = (PNETLINK_SOCKET)Socket;
    if (InformationType != SocketInformationNetlink) {
        Status = STATUS_INVALID_PARAMETER;
        goto GetSetInformationEnd;
    }

    RequiredSize = 0;
    Source = NULL;
    Status = STATUS_SUCCESS;
    NetlinkOption = (NETLINK_SOCKET_OPTION)Option;
    switch (NetlinkOption) {
    case NetlinkSocketOptionJoinMulticastGroup:
    case NetlinkSocketOptionLeaveMulticastGroup:
        RequiredSize = sizeof(ULONG);
        if (Set == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        GroupId = *((PULONG)Data);
        if ((GroupId > SOCKET_OPTION_MAX_ULONG) || (GroupId == 0)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Adding a socket to a multicast group is a bit tricky, as it needs to
        // validate the group ID with the protocol layer. So perform a re-bind.
        //

        if (NetlinkOption == NetlinkSocketOptionJoinMulticastGroup) {
            Status = NetlinkSocket->ProtocolInterface.JoinMulticastGroup(
                                                                      Socket,
                                                                      GroupId);

            if (!KSUCCESS(Status)) {
                break;
            }

        //
        // Dropping membership is easy, as the socket's propertie can validate
        // if the group ID is valid and silently fail if it is not.
        //

        } else {
            NetlinkpLeaveMulticastGroup(Socket, GroupId, FALSE);
        }

        break;

    default:
        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto GetSetInformationEnd;
    }

    //
    // Truncate all copies for get requests down to the required size and
    // always return the required size on set requests.
    //

    if (*DataSize > RequiredSize) {
        *DataSize = RequiredSize;
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

        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto GetSetInformationEnd;
        }
    }

GetSetInformationEnd:
    return Status;
}

NETLINK_API
KSTATUS
NetlinkSendMessage (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine sends a netlink message to the given destination address. The
    caller should have already filled the buffer with the netlink header.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to send the
        message.

    Packet - Supplies a pointer to the network packet to be sent.

    DestinationAddress - Supplies a pointer to the destination address to which
        the message will be sent.

Return Value:

    Status code.

--*/

{

    SOCKET_IO_PARAMETERS IoParameters;
    KSTATUS Status;

    //
    // The packet should be filled up. Set the data offset back to zero to send
    // all the data in the packet.
    //

    ASSERT(Packet->DataOffset == Packet->FooterOffset);

    Packet->DataOffset = 0;

    //
    // Send the packet using the I/O API.
    //

    RtlZeroMemory(&IoParameters, sizeof(SOCKET_IO_PARAMETERS));
    IoParameters.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    IoParameters.NetworkAddress = DestinationAddress;
    IoParameters.Size = Packet->FooterOffset - Packet->DataOffset;
    MmSetIoBufferCurrentOffset(Packet->IoBuffer, Packet->DataOffset);
    Status = IoSocketSendData(TRUE,
                              Socket->KernelSocket.IoHandle,
                              &IoParameters,
                              Packet->IoBuffer);

    return Status;
}

NETLINK_API
KSTATUS
NetlinkSendMultipartMessage (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress,
    ULONG SequenceNumber
    )

/*++

Routine Description:

    This routine sends a multipart message packet. It will append the final
    DONE message, which the packet must have space for, reset the packet's data
    offset to the beginning and then send the entire packet off to the
    destination address.

Arguments:

    Socket - Supplies a pointer to the network socket from which the packet
        will be sent.

    Packet - Supplies a pointer to the network packet to send.

    DestinationAddress - Supplies a pointer to the network address to which the
        packet will be sent.

    SequenceNumber - Supplies the sequence number to set in the header of the
        DONE message that is appended to the packet.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Add the done message. It is simply just a netlink header with the type
    // set to DONE and the multi-part message flag set.
    //

    Status = NetlinkAppendHeader(Socket,
                                 Packet,
                                 0,
                                 SequenceNumber,
                                 NETLINK_MESSAGE_TYPE_DONE,
                                 NETLINK_HEADER_FLAG_MULTIPART);

    if (!KSUCCESS(Status)) {
        goto SendMultipartMessageEnd;
    }

    //
    // Send the entire multipart packet to the destination as one "message".
    //

    Status = NetlinkSendMessage(Socket, Packet, DestinationAddress);
    if (!KSUCCESS(Status)) {
        goto SendMultipartMessageEnd;
    }

SendMultipartMessageEnd:
    return Status;
}

NETLINK_API
KSTATUS
NetlinkAppendHeader (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    ULONG Length,
    ULONG SequenceNumber,
    USHORT Type,
    USHORT Flags
    )

/*++

Routine Description:

    This routine appends a base netlink header to the given network packet. It
    validates if there is enough space remaining in the packet and moves the
    data offset forwards to the first byte after the header on success.

Arguments:

    Socket - Supplies a pointer to the socket that will send the packet. The
        header's port ID is taken from the socket's local address.

    Packet - Supplies a pointer to the network packet to which a base netlink
        header will be added.

    Length - Supplies the length of the netlink message, not including the
        header.

    SequenceNumber - Supplies the desired sequence number for the netlink
        message.

    Type - Supplies the message type to be set in the header.

    Flags - Supplies a bitmask of netlink message flags to be set. See
        NETLINK_HEADER_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    PNETLINK_HEADER Header;
    ULONG PacketLength;
    PNETLINK_ADDRESS SourceAddress;
    KSTATUS Status;

    Length += NETLINK_HEADER_LENGTH;
    PacketLength = Packet->FooterOffset - Packet->DataOffset;
    if (PacketLength < Length) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto AppendHeaderEnd;
    }

    SourceAddress = (PNETLINK_ADDRESS)&(Socket->LocalSendAddress);
    Header = Packet->Buffer + Packet->DataOffset;
    Header->Length = Length;
    Header->Type = Type;
    Header->Flags = Flags;
    Header->SequenceNumber = SequenceNumber;
    Header->PortId = SourceAddress->Port;
    Packet->DataOffset += NETLINK_HEADER_LENGTH;
    Status = STATUS_SUCCESS;

AppendHeaderEnd:
    return Status;
}

NETLINK_API
KSTATUS
NetlinkAppendAttribute (
    PNET_PACKET_BUFFER Packet,
    USHORT Type,
    PVOID Data,
    USHORT DataLength
    )

/*++

Routine Description:

    This routine appends a netlink attribute to the given network packet. It
    validates that there is enough space for the attribute and moves the
    packet's data offset to the first byte after the attribute. The exception
    to this rule is if a NULL data buffer is supplied; the packet's data offset
    is only moved to the first byte after the attribute header.

Arguments:

    Packet - Supplies a pointer to the network packet to which the attribute
        will be added.

    Type - Supplies the netlink attribute type.

    Data - Supplies an optional pointer to the attribute data to be stored in
        the network packet. Even if no data buffer is supplied, a data length
        may be supplied for the case of child attributes that are yet to be
        appended.

    DataLength - Supplies the length of the data, in bytes.

Return Value:

    Status code.

--*/

{

    PNETLINK_ATTRIBUTE Attribute;
    ULONG AttributeLength;
    ULONG PacketLength;

    AttributeLength = NETLINK_ATTRIBUTE_SIZE(DataLength);
    PacketLength = Packet->FooterOffset - Packet->DataOffset;
    if (PacketLength < AttributeLength) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    Attribute = Packet->Buffer + Packet->DataOffset;
    Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(DataLength);
    Attribute->Type = Type;
    if (Data != NULL) {
        RtlCopyMemory(NETLINK_ATTRIBUTE_DATA(Attribute), Data, DataLength);
        Packet->DataOffset += AttributeLength;

    } else {
        Packet->DataOffset += NETLINK_ATTRIBUTE_HEADER_LENGTH;
    }

    return STATUS_SUCCESS;
}

NETLINK_API
KSTATUS
NetlinkGetAttribute (
    PVOID Attributes,
    ULONG AttributesLength,
    USHORT Type,
    PVOID *Data,
    PUSHORT DataLength
    )

/*++

Routine Description:

    This routine parses the given attributes buffer and returns a pointer to
    the desired attribute.

Arguments:

    Attributes - Supplies a pointer to the start of the generic command
        attributes.

    AttributesLength - Supplies the length of the attributes buffer, in bytes.

    Type - Supplies the netlink generic attribute type.

    Data - Supplies a pointer that receives a pointer to the data for the
        requested attribute type.

    DataLength - Supplies a pointer that receives the length of the requested
        attribute data.

Return Value:

    Status code.

--*/

{

    PNETLINK_ATTRIBUTE Attribute;
    ULONG AttributeSize;

    Attribute = (PNETLINK_ATTRIBUTE)Attributes;
    while (AttributesLength != 0) {
        if ((AttributesLength < NETLINK_ATTRIBUTE_HEADER_LENGTH) ||
            (AttributesLength < Attribute->Length)) {

            break;
        }

        if (Attribute->Type == Type) {
            *DataLength = Attribute->Length - NETLINK_ATTRIBUTE_HEADER_LENGTH;
            *Data = NETLINK_ATTRIBUTE_DATA(Attribute);
            return STATUS_SUCCESS;
        }

        AttributeSize = NETLINK_ALIGN(Attribute->Length);
        Attribute = (PVOID)Attribute + AttributeSize;
        AttributesLength -= AttributeSize;
    }

    *Data = NULL;
    *DataLength = 0;
    return STATUS_NOT_FOUND;
}

NETLINK_API
KSTATUS
NetlinkJoinMulticastGroup (
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
    KeAcquireSharedExclusiveLockExclusive(NetlinkMulticastLock);
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
                         &NetlinkMulticastSocketList);
        }
    }

    ASSERT(NetlinkSocket->MulticastListEntry.Next != NULL);

    KeReleaseSharedExclusiveLockExclusive(NetlinkMulticastLock);
    if (ReleaseBitmap != NULL) {
        MmFreePagedPool(ReleaseBitmap);
    }

    return STATUS_SUCCESS;
}

NETLINK_API
VOID
NetlinkRemoveSocketsFromMulticastGroups (
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

    if (LIST_EMPTY(&NetlinkMulticastSocketList) != FALSE) {
        return;
    }

    KeAcquireSharedExclusiveLockExclusive(NetlinkMulticastLock);
    CurrentEntry = NetlinkMulticastSocketList.Next;
    while (CurrentEntry != &NetlinkMulticastSocketList) {
        Socket = LIST_VALUE(CurrentEntry, NETLINK_SOCKET, MulticastListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Socket->NetSocket.Protocol->ParentProtocolNumber !=
            ParentProtocolNumber) {

            continue;
        }

        for (Index = 0; Index < GroupCount; Index += 1) {
            NetlinkpLeaveMulticastGroup(&(Socket->NetSocket),
                                        GroupOffset + Index,
                                        TRUE);
        }
    }

    KeReleaseSharedExclusiveLockExclusive(NetlinkMulticastLock);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetlinkpProcessReceivedPackets (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PNET_PACKET_LIST PacketList
    )

/*++

Routine Description:

    This routine processes a list of packets, handling netlink message parsing
    and error handling that is common to all protocols.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

    PacketList - Supplies a list of packets to process.

Return Value:

    None.

--*/

{

    PNETLINK_ADDRESS Destination;
    PNET_PACKET_BUFFER Packet;
    PNET_SOCKET Socket;
    KSTATUS Status;

    Socket = NULL;

    //
    // Attempt broadcast processing on the packet list.
    //

    NetlinkpBroadcastPackets(ReceiveContext, PacketList);

    //
    // The kernel should never get any multicast packets, so just drop it
    // now before getting to the kernel.
    //

    Destination = (PNETLINK_ADDRESS)ReceiveContext->Destination;
    if ((Destination->Group != 0) &&
        (Destination->Port == NETLINK_KERNEL_PORT_ID)) {

        NetDestroyBufferList(PacketList);
        goto ProcessReceivedPacketsEnd;
    }

    //
    // Zero out the group so that sockets can match.
    //

    Destination->Group = 0;

    //
    // Find the socket targeted by the destination address.
    //

    Status = NetFindSocket(ReceiveContext, &Socket);
    if (!KSUCCESS(Status)) {
        goto ProcessReceivedPacketsEnd;
    }

    ASSERT(Socket->Protocol == ReceiveContext->Protocol);

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

        ReceiveContext->Packet = Packet;
        NetlinkpProcessReceivedSocketData(Socket, ReceiveContext);
    }

ProcessReceivedPacketsEnd:
    if (Socket != NULL) {
        IoSocketReleaseReference(&(Socket->KernelSocket));
    }

    return;
}

VOID
NetlinkpProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine handles received packet processing for a netlink socket.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

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
        NetlinkpProcessReceivedKernelData(Socket, ReceiveContext);

    } else {
        Protocol = Socket->Protocol;
        Protocol->Interface.ProcessReceivedSocketData(Socket, ReceiveContext);
    }

    return;
}

VOID
NetlinkpProcessReceivedKernelData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is handles received packet processing for a kernel socket.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    None.

--*/

{

    PNETLINK_HEADER Header;
    ULONG MessageSize;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketLength;
    PNET_PROTOCOL_PROCESS_RECEIVED_SOCKET_DATA ProcessReceivedSocketData;
    PNET_PROTOCOL_ENTRY Protocol;
    KSTATUS Status;

    Packet = ReceiveContext->Packet;
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
        Status = ProcessReceivedSocketData(Socket, ReceiveContext);
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
            NetlinkpSendAck(Socket, Packet, ReceiveContext->Source, Status);
        }

        Packet->DataOffset += MessageSize;
        PacketLength -= MessageSize;
    }

    NetFreeBuffer(Packet);
    return;
}

VOID
NetlinkpBroadcastPackets (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PNET_PACKET_LIST PacketList
    )

/*++

Routine Description:

    This routine broadcasts a list of packets to all netlink sockets listening
    on the destination address's multicast group.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

    PacketList - Supplies a list of packets to process.

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
    KSTATUS Status;

    //
    // If a group ID is not supplied, then this is a unicast list of packets.
    //

    Destination = (PNETLINK_ADDRESS)ReceiveContext->Destination;
    if (Destination->Group == 0) {
        return;
    }

    //
    // Threads sending to multiple sockets must have the broadcast permission
    // set.
    //

    Status = PsCheckPermission(PERMISSION_NET_BROADCAST);
    if (!KSUCCESS(Status)) {
        return;
    }

    //
    // Send the packet to all sockets listening to that multicast group. A
    // socket must match on the protocol and have its bitmap set for the group.
    // If a port is also specified in the address, do not send it to the socket
    // with the port during multicast processing.
    //

    PacketEntry = PacketList->Head.Next;
    while (PacketEntry != &(PacketList->Head)) {
        Packet = LIST_VALUE(PacketEntry, NET_PACKET_BUFFER, ListEntry);
        Packet->Flags |= NET_PACKET_FLAG_MULTICAST;
        GroupIndex = NETLINK_SOCKET_BITMAP_INDEX(Destination->Group);
        GroupMask = NETLINK_SOCKET_BITMAP_MASK(Destination->Group);
        KeAcquireSharedExclusiveLockShared(NetlinkMulticastLock);
        SocketEntry = NetlinkMulticastSocketList.Next;
        while (SocketEntry != &NetlinkMulticastSocketList) {
            NetlinkSocket = LIST_VALUE(SocketEntry,
                                       NETLINK_SOCKET,
                                       MulticastListEntry);

            Socket = &(NetlinkSocket->NetSocket);
            SocketEntry = SocketEntry->Next;
            if (Socket->Protocol != ReceiveContext->Protocol) {
                continue;
            }

            if (Socket->LocalReceiveAddress.Port == Destination->Port) {
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

            ReceiveContext->Packet = Packet;
            NetlinkpProcessReceivedSocketData(Socket, ReceiveContext);
        }

        KeReleaseSharedExclusiveLockShared(NetlinkMulticastLock);

        //
        // Clear out the multicast group and send it on to the socket
        // specified by the port.
        //

        Packet->Flags &= ~NET_PACKET_FLAG_MULTICAST;
        PacketEntry = PacketEntry->Next;
    }

    return;
}

VOID
NetlinkpSendAck (
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
    ULONG HeaderLength;
    PNETLINK_HEADER OriginalHeader;
    ULONG PayloadLength;
    KSTATUS Status;

    //
    // Create the ACK packet with the appropriate error message based on the
    // given status.
    //

    AckPacket = NULL;
    CopyLength = NETLINK_HEADER_LENGTH;
    PayloadLength = sizeof(NETLINK_ERROR_MESSAGE);
    HeaderLength = NETLINK_HEADER_LENGTH;
    if (!KSUCCESS(PacketStatus)) {
        CopyLength = Packet->FooterOffset - Packet->DataOffset;
        PayloadLength += CopyLength - NETLINK_HEADER_LENGTH;
    }

    AckLength = HeaderLength + PayloadLength;
    Status = NetAllocateBuffer(0,
                               AckLength,
                               0,
                               NULL,
                               0,
                               &AckPacket);

    if (!KSUCCESS(Status)) {
        return;
    }

    OriginalHeader = Packet->Buffer + Packet->DataOffset;
    Status = NetlinkAppendHeader(Socket,
                                 AckPacket,
                                 PayloadLength,
                                 OriginalHeader->SequenceNumber,
                                 NETLINK_MESSAGE_TYPE_ERROR,
                                 0);

    if (!KSUCCESS(Status)) {
        goto SendAckEnd;
    }

    ErrorMessage = AckPacket->Buffer + AckPacket->DataOffset;
    ErrorMessage->Error = (INT)PacketStatus;
    RtlCopyMemory(&(ErrorMessage->Header), OriginalHeader, CopyLength);
    AckPacket->DataOffset += PayloadLength;

    //
    // Send the ACK packet back to where the original packet came from.
    //

    Status = NetlinkSendMessage(Socket, AckPacket, DestinationAddress);
    if (!KSUCCESS(Status)) {
        goto SendAckEnd;
    }

SendAckEnd:
    if (AckPacket != NULL) {
        NetFreeBuffer(AckPacket);
    }

    return;
}

VOID
NetlinkpLeaveMulticastGroup (
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
        KeAcquireSharedExclusiveLockExclusive(NetlinkMulticastLock);
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
        KeReleaseSharedExclusiveLockExclusive(NetlinkMulticastLock);
    }

    return;
}

