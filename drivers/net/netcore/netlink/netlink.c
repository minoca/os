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

//
// -------------------------------------------------------------------- Globals
//

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
    if ((Socket->Flags & NET_SOCKET_FLAG_KERNEL) != 0) {
        NetlinkAddress = (PNETLINK_ADDRESS)Address;
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

    Status = STATUS_SUCCESS;

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

    PNET_PACKET_BUFFER Packet;

    //
    // Use the socket's protocol and turn it around to process data.
    //

    while (NET_PACKET_LIST_EMPTY(PacketList) == FALSE) {
        Packet = LIST_VALUE(PacketList->Head.Next,
                            NET_PACKET_BUFFER,
                            ListEntry);

        NET_REMOVE_PACKET_FROM_LIST(Packet, PacketList);
        Socket->Protocol->Interface.ProcessReceivedData(Socket->Link,
                                                        Packet,
                                                        &Socket->LocalAddress,
                                                        Destination,
                                                        Socket->Protocol);
    }

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
    // If the buffer is present, print that bad boy out.
    //

    if (NetlinkAddress->GroupMask != 0) {
        Length = RtlPrintToString(Buffer,
                                  BufferLength,
                                  CharacterEncodingDefault,
                                  "%08x:%08x",
                                  NetlinkAddress->Port,
                                  NetlinkAddress->GroupMask);

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
        command.

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

//
// --------------------------------------------------------- Internal Functions
//

