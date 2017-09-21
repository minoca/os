/*++

Copyright (c) 2017 Minoca Corp. All Rights Reserved

Module Name:

    ip6.c

Abstract:

    This module implements support for the Internet Protocol version 6 (IPv6).

Author:

    Chris Stevens 22-Aug-2017

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
#include <minoca/net/ip6.h>
#include <minoca/net/icmp6.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum sizes of IPv6 address and IPv4 strings, including the
// null terminator.
//

#define IP6_MAX_ADDRESS_STRING_SIZE \
    sizeof("[ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255]:65535")

//
// --------------------------------------------------------------------- Macros
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpIp6InitializeLink (
    PNET_LINK Link
    );

VOID
NetpIp6DestroyLink (
    PNET_LINK Link
    );

KSTATUS
NetpIp6InitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    );

VOID
NetpIp6DestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp6BindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address,
    ULONG Flags
    );

KSTATUS
NetpIp6Listen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp6Connect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpIp6Disconnect (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp6Close (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp6Send (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Destination,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride,
    PNET_PACKET_LIST PacketList
    );

VOID
NetpIp6ProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

ULONG
NetpIp6PrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

KSTATUS
NetpIp6GetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

NET_ADDRESS_TYPE
NetpIp6GetAddressType (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpIp6SendTranslationRequest (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS QueryAddress
    );

ULONG
NetpIp6ChecksumPseudoHeader (
    PNETWORK_ADDRESS Source,
    PNETWORK_ADDRESS Destination,
    ULONG PacketLength,
    UCHAR Protocol
    );

KSTATUS
NetpIp6ConfigureLinkAddress (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    BOOL Configure
    );

KSTATUS
NetpIp6JoinLeaveMulticastGroup (
    PNET_NETWORK_MULTICAST_REQUEST Request,
    BOOL Join
    );

KSTATUS
NetpIp6TranslateNetworkAddress (
    PNET_NETWORK_ENTRY Network,
    PNETWORK_ADDRESS NetworkAddress,
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    );

KSTATUS
NetpIp6ProcessExtensionHeaders (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

VOID
NetpIp6SendParameterProblemMessage (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    UCHAR Code,
    ULONG Pointer
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL NetIp6DebugPrintPackets = FALSE;

NET_NETWORK_ENTRY NetIp6Network = {
    {NULL, NULL},
    NetDomainIp6,
    IP6_PROTOCOL_NUMBER,
    {
        NetpIp6InitializeLink,
        NetpIp6DestroyLink,
        NetpIp6InitializeSocket,
        NetpIp6DestroySocket,
        NetpIp6BindToAddress,
        NetpIp6Listen,
        NetpIp6Connect,
        NetpIp6Disconnect,
        NetpIp6Close,
        NetpIp6Send,
        NetpIp6ProcessReceivedData,
        NetpIp6PrintAddress,
        NetpIp6GetSetInformation,
        NetpIp6GetAddressType,
        NetpIp6ChecksumPseudoHeader,
        NetpIp6ConfigureLinkAddress,
        NetpIp6JoinLeaveMulticastGroup
    }
};

//
// Store well-known IPv6 addresses.
//

const UCHAR NetIp6AllNodesMulticastAddress[IP6_ADDRESS_SIZE] = {
    0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

const UCHAR NetIp6AllRoutersMulticastAddress[IP6_ADDRESS_SIZE] = {
    0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
};

const UCHAR NetIp6AllMld2RoutersMulticastAddress[IP6_ADDRESS_SIZE] = {
    0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16
};

const UCHAR NetIp6SolicitedNodeMulticastPrefix[IP6_ADDRESS_SIZE] = {
    0xFF, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00, 0x00
};

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpIp6Initialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for IPv6 packets.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Register the IPv6 handlers with the core networking library.
    //

    Status = NetRegisterNetworkLayer(&NetIp6Network, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
}

KSTATUS
NetpIp6InitializeLink (
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

    PNET_LINK_ADDRESS_ENTRY AddressEntry;
    PUCHAR BytePointer;
    IP6_ADDRESS InitialAddress;
    PUCHAR MacAddress;
    IP6_ADDRESS MulticastAddress;
    PNETWORK_ADDRESS PhysicalAddress;
    KSTATUS Status;

    //
    // Initizlie a link address entry with an EUI-64 formatted link-local
    // address.
    //

    PhysicalAddress = &(Link->Properties.PhysicalAddress);

    //
    // This currently only supports creating an EUI-64 based interface
    // identifier from 48-bit MAC addresses. If a different data link layer is
    // added, this work probably needs to be farmed out to each data link layer.
    //

    ASSERT((PhysicalAddress->Domain == NetDomainEthernet) ||
           (PhysicalAddress->Domain == NetDomain80211));

    MacAddress = (PUCHAR)(PhysicalAddress->Address);
    RtlZeroMemory((PNETWORK_ADDRESS)&InitialAddress, sizeof(NETWORK_ADDRESS));
    InitialAddress.Domain = NetDomainIp6;
    BytePointer = (PUCHAR)(InitialAddress.Address);
    BytePointer[15] = MacAddress[5];
    BytePointer[14] = MacAddress[4];
    BytePointer[13] = MacAddress[3];
    BytePointer[12] = 0xFE;
    BytePointer[11] = 0xFF;
    BytePointer[10] = MacAddress[2];
    BytePointer[9] = MacAddress[1];
    BytePointer[8] = (MacAddress[0] & 0xFD) | (~MacAddress[0] & 0x02);
    InitialAddress.Address[0] = CPU_TO_NETWORK32(IP6_LINK_LOCAL_PREFIX);
    Status = NetCreateLinkAddressEntry(Link,
                                       (PNETWORK_ADDRESS)&InitialAddress,
                                       NULL,
                                       NULL,
                                       TRUE,
                                       &AddressEntry);

    if (!KSUCCESS(Status)) {
        goto Ip6InitializeLinkEnd;
    }

    //
    // Every IPv6 node should join the all-nodes multicast group.
    //

    RtlZeroMemory(&MulticastAddress, sizeof(IP6_ADDRESS));
    MulticastAddress.Domain = NetDomainIp6;
    RtlCopyMemory(MulticastAddress.Address,
                  NetIp6AllNodesMulticastAddress,
                  IP6_ADDRESS_SIZE);

    Status = NetJoinLinkMulticastGroup(Link,
                                       AddressEntry,
                                       (PNETWORK_ADDRESS)&MulticastAddress);

    if (!KSUCCESS(Status)) {
        goto Ip6InitializeLinkEnd;
    }

Ip6InitializeLinkEnd:
    if (!KSUCCESS(Status)) {
        if (AddressEntry != NULL) {
            NetDestroyLinkAddressEntry(Link, AddressEntry);
        }
    }

    return Status;
}

VOID
NetpIp6DestroyLink (
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

    //
    // Destroy any multicast groups that the link still belongs to. These
    // should only be the groups that it joined during initialization, as this
    // routine is called after the last reference on the link is released.
    //

    NetDestroyLinkMulticastGroups(Link);
    return;
}

KSTATUS
NetpIp6InitializeSocket (
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

    //
    // If this is coming from the raw protocol and the network protocol is the
    // raw, wildcard protocol, then this socket automatically gets the headers
    // included flag.
    //

    if ((ProtocolEntry->Type == NetSocketRaw) &&
        (NetworkProtocol == SOCKET_INTERNET_PROTOCOL_RAW)) {

        RtlAtomicOr32(&(NewSocket->Flags),
                      NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED);
    }

    //
    // Determine if the maximum IPv6 packet size plus all existing headers and
    // footers is less than the current maximum packet size. If so, truncate
    // the maximum packet size. The IPv6 packet size does not include the
    // header so add the header length to the maximum size as well.
    //

    MaxPacketSize = NewSocket->PacketSizeInformation.HeaderSize +
                    sizeof(IP6_HEADER) +
                    IP6_MAX_PAYLOAD_LENGTH +
                    NewSocket->PacketSizeInformation.FooterSize;

    if (NewSocket->PacketSizeInformation.MaxPacketSize > MaxPacketSize) {
        NewSocket->PacketSizeInformation.MaxPacketSize = MaxPacketSize;
    }

    //
    // Add the IPv6 header size for higher layers to perform the same
    // truncation procedure. Skip this for raw sockets using the raw protocol;
    // they must always supply an IPv6 header, so it doesn't make sense to add
    // it to the header size. It comes in the data packet.
    //

    if ((ProtocolEntry->Type != NetSocketRaw) ||
        (NetworkProtocol != SOCKET_INTERNET_PROTOCOL_RAW)) {

        NewSocket->PacketSizeInformation.HeaderSize += sizeof(IP6_HEADER);
    }

    //
    // Set IPv6 specific socket setting defaults.
    //

    NewSocket->HopLimit = IP6_DEFAULT_HOP_LIMIT;
    NewSocket->DifferentiatedServicesCodePoint = 0;
    NewSocket->MulticastHopLimit = IP6_DEFAULT_MULTICAST_HOP_LIMIT;
    return NetInitializeMulticastSocket(NewSocket);
}

VOID
NetpIp6DestroySocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys any pieces allocated by the network layer for the
    socket.

Arguments:

    Socket - Supplies a pointer to the socket to destroy.

Return Value:

    None.

--*/

{

    NetDestroyMulticastSocket(Socket);
    return;
}

KSTATUS
NetpIp6BindToAddress (
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

    NET_SOCKET_BINDING_TYPE BindingType;
    PIP6_ADDRESS Ip6Address;
    NET_LINK_LOCAL_ADDRESS LocalInformation;
    ULONG Port;
    KSTATUS Status;

    Ip6Address = (PIP6_ADDRESS)Address;
    LocalInformation.Link = NULL;

    //
    // Classify the address and binding type. Leaving it as unknown is OK.
    //

    BindingType = SocketLocallyBound;
    if (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Address->Address) != FALSE) {
        BindingType = SocketUnbound;
    }

    //
    // If a specific link is given, try to find the given address in that link.
    //

    if (Link != NULL) {
        Port = Address->Port;
        Address->Port = 0;
        Status = NetFindLinkForLocalAddress(Address,
                                            Link,
                                            &LocalInformation);

        Address->Port = Port;
        if (!KSUCCESS(Status)) {
            goto Ip6BindToAddressEnd;
        }

        LocalInformation.ReceiveAddress.Port = Port;
        LocalInformation.SendAddress.Port = Port;

    //
    // No specific link was passed.
    //

    } else {

        //
        // If the address is not the unspecified address, then look for the
        // link that owns this address.
        //

        if (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Address->Address) == FALSE) {
            Port = Address->Port;
            Address->Port = 0;
            Status = NetFindLinkForLocalAddress(Address,
                                                NULL,
                                                &LocalInformation);

            Address->Port = Port;
            if (!KSUCCESS(Status)) {
                goto Ip6BindToAddressEnd;
            }

            LocalInformation.ReceiveAddress.Port = Port;
            LocalInformation.SendAddress.Port = Port;

        //
        // No link was passed, this is a generic bind to a port on any address.
        //

        } else {
            LocalInformation.Link = NULL;
            LocalInformation.LinkAddress = NULL;
            RtlCopyMemory(&(LocalInformation.ReceiveAddress),
                          Address,
                          sizeof(NETWORK_ADDRESS));

            RtlCopyMemory(&(LocalInformation.SendAddress),
                          Address,
                          sizeof(NETWORK_ADDRESS));
        }
    }

    //
    // Bind the socket to the local address. The socket remains inactive,
    // unable to receive packets.
    //

    Status = NetBindSocket(Socket, BindingType, &LocalInformation, NULL, Flags);
    if (!KSUCCESS(Status)) {
        goto Ip6BindToAddressEnd;
    }

    Status = STATUS_SUCCESS;

Ip6BindToAddressEnd:
    if (LocalInformation.Link != NULL) {
        NetLinkReleaseReference(LocalInformation.Link);
    }

    return Status;
}

KSTATUS
NetpIp6Listen (
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
        LocalAddress.Domain = NetDomainIp6;
        Status = NetpIp6BindToAddress(Socket, NULL, &LocalAddress, 0);
        if (!KSUCCESS(Status)) {
            goto Ip6ListenEnd;
        }
    }

    Status = NetActivateSocket(Socket);
    if (!KSUCCESS(Status)) {
        goto Ip6ListenEnd;
    }

Ip6ListenEnd:
    return Status;
}

KSTATUS
NetpIp6Connect (
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
    KSTATUS Status;

    //
    // Fully bind the socket and activate it. It's ready to receive.
    //

    Flags = NET_SOCKET_BINDING_FLAG_ACTIVATE;
    Status = NetBindSocket(Socket, SocketFullyBound, NULL, Address, Flags);
    if (!KSUCCESS(Status)) {
        goto Ip6ConnectEnd;
    }

    Status = STATUS_SUCCESS;

Ip6ConnectEnd:
    return Status;
}

KSTATUS
NetpIp6Disconnect (
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

    KSTATUS Status;

    //
    // Roll the fully bound socket back to the locally bound state.
    //

    Status = NetDisconnectSocket(Socket);
    if (!KSUCCESS(Status)) {
        goto Ip6DisconnectEnd;
    }

    Status = STATUS_SUCCESS;

Ip6DisconnectEnd:
    return Status;
}

KSTATUS
NetpIp6Close (
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
NetpIp6Send (
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

    PLIST_ENTRY CurrentEntry;
    ULONG DataOffset;
    ULONG FooterOffset;
    PIP6_HEADER Header;
    UCHAR HopLimit;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PIP6_ADDRESS LocalAddress;
    ULONG MaxPacketSize;
    PNET_SOCKET_LINK_OVERRIDE MulticastInterface;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketFlags;
    PNETWORK_ADDRESS PhysicalNetworkAddress;
    NETWORK_ADDRESS PhysicalNetworkAddressBuffer;
    NET_RECEIVE_CONTEXT ReceiveContext;
    PIP6_ADDRESS RemoteAddress;
    PNET_DATA_LINK_SEND Send;
    PNETWORK_ADDRESS Source;
    KSTATUS Status;
    ULONG TotalLength;
    ULONG VersionClassFlow;

    ASSERT(Destination->Domain == Socket->KernelSocket.Domain);
    ASSERT((Socket->KernelSocket.Type == NetSocketRaw) ||
           (Socket->KernelSocket.Protocol ==
            Socket->Protocol->ParentProtocolNumber));

    //
    // Multicast packets must use the multicast hop limit, which default to 1
    // rather than 64, as multicast packets aren't typically meant to go beyond
    // the local network.
    //

    HopLimit = Socket->HopLimit;
    RemoteAddress = (PIP6_ADDRESS)Destination;
    if (IP6_IS_MULTICAST_ADDRESS(RemoteAddress->Address) != FALSE) {
        HopLimit = Socket->MulticastHopLimit;

        //
        // Also use the multicast interface information if it is present.
        //

        MulticastInterface = &(Socket->MulticastInterface);
        if (MulticastInterface->LinkInformation.Link != NULL) {
            LinkOverride = MulticastInterface;
        }
    }

    //
    // If an override was supplied, prefer that link and link address.
    //

    if (LinkOverride != NULL) {
        Link = LinkOverride->LinkInformation.Link;
        LinkAddress = LinkOverride->LinkInformation.LinkAddress;
        MaxPacketSize = LinkOverride->PacketSizeInformation.MaxPacketSize;
        Source = &(LinkOverride->LinkInformation.SendAddress);

    //
    // Otherwise use the socket's information.
    //

    } else {
        Link = Socket->Link;
        LinkAddress = Socket->LinkAddress;
        MaxPacketSize = Socket->PacketSizeInformation.MaxPacketSize;
        Source = &(Socket->LocalSendAddress);
    }

    LocalAddress = (PIP6_ADDRESS)Source;

    //
    // There better be a link and link address.
    //

    ASSERT((Link != NULL) && (LinkAddress != NULL));

    //
    // Figure out the physical network address for the given IP destination
    // address. This answer is the same for every packet. Use the cached
    // version in the network socket if it's there and the destination matches
    // the remote address in the net socket.
    //

    PhysicalNetworkAddress = &(Socket->RemotePhysicalAddress);
    if (Destination != &(Socket->RemoteAddress)) {
        PhysicalNetworkAddressBuffer.Domain = NetDomainInvalid;
        PhysicalNetworkAddress = &PhysicalNetworkAddressBuffer;
    }

    if (PhysicalNetworkAddress->Domain == NetDomainInvalid) {
        Status = NetpIp6TranslateNetworkAddress(Socket->Network,
                                                Destination,
                                                Link,
                                                LinkAddress,
                                                PhysicalNetworkAddress);

        if (!KSUCCESS(Status)) {
            goto Ip6SendEnd;
        }

        ASSERT(PhysicalNetworkAddress->Domain != NetDomainInvalid);
    }

    //
    // Add the IP6 and Ethernet headers to each packet.
    //

    CurrentEntry = PacketList->Head.Next;
    while (CurrentEntry != &(PacketList->Head)) {
        Packet = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // If the socket is supposed to include the IP header in its
        // packets, but this packet is too large, then fail without sending any
        // packets.
        //

        if ((Packet->DataSize > MaxPacketSize) &&
            ((Socket->Flags & NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED) != 0)) {

            Status = STATUS_MESSAGE_TOO_LONG;
            goto Ip6SendEnd;

        //
        // If the current packet's total data size (including all headers and
        // footers) is larger than the socket's/link's maximum size, then the
        // IP layer needs to break it into multiple fragments.
        //

        } else if (Packet->DataSize > MaxPacketSize) {

            //
            // TODO: Implement IPv6 fragmentation.
            //

            ASSERT(FALSE);
        }

        //
        // Add the IPv6 network header unless it is already included.
        //

        if ((Socket->Flags & NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED) == 0) {

            //
            // The IPv6 header length field does not include the IPv6 header.
            //

            TotalLength = Packet->FooterOffset - Packet->DataOffset;

            //
            // Get a pointer to the header, which is right before the data.
            //

            ASSERT(Packet->DataOffset > sizeof(IP6_HEADER));

            Packet->DataOffset -= sizeof(IP6_HEADER);
            Header = (PIP6_HEADER)(Packet->Buffer + Packet->DataOffset);

            //
            // Fill out that IPv6 header.
            //

            VersionClassFlow = (IP6_VERSION << IP6_VERSION_SHIFT) &
                               IP6_VERSION_MASK;

            Header->VersionClassFlow = CPU_TO_NETWORK32(VersionClassFlow);
            Header->PayloadLength = CPU_TO_NETWORK16(TotalLength);

            ASSERT(Socket->KernelSocket.Protocol !=
                   SOCKET_INTERNET_PROTOCOL_RAW);

            Header->NextHeader = Socket->KernelSocket.Protocol;
            Header->HopLimit = HopLimit;
            RtlCopyMemory(&(Header->SourceAddress),
                          &(LocalAddress->Address),
                          IP6_ADDRESS_SIZE);

            RtlCopyMemory(&(Header->DestinationAddress),
                          &(RemoteAddress->Address),
                          IP6_ADDRESS_SIZE);

            Socket->SendPacketCount += 1;

        //
        // Otherwise the packet may need to be shifted. Unless this is a raw
        // socket using the "raw" protocol, the packet was created thinking
        // that the IPv6 header needed to be included by the network layer. The
        // flags now indicate that the IPv6 header is included by the caller.
        // The packet needs to be properly aligned for the hardware, so it
        // needs to be shifted by the IPv6 header size.
        //

        } else {

            ASSERT(Socket->KernelSocket.Protocol == NetSocketRaw);

            //
            // This can be skipped if the socket is signed up to use the "raw"
            // protocol. The IPv6 header size isn't added to such sockets upon
            // initialization.
            //

            if (Socket->KernelSocket.Protocol != SOCKET_INTERNET_PROTOCOL_RAW) {

                ASSERT(Packet->DataOffset > sizeof(IP6_HEADER));

                Header = (PIP6_HEADER)(Packet->Buffer +
                                       Packet->DataOffset -
                                       sizeof(IP6_HEADER));

                TotalLength = Packet->DataSize - Packet->DataOffset;
                RtlCopyMemory(Header,
                              Packet->Buffer + Packet->DataOffset,
                              TotalLength);

                Packet->DataOffset -= sizeof(IP6_HEADER);
                Packet->FooterOffset -= sizeof(IP6_HEADER);
                Packet->DataSize -= sizeof(IP6_HEADER);
            }
        }
    }

    //
    // If this is a multicast address and the loopback bit is set, send the
    // packets back up the stack before sending them down. This needs to be
    // done first because the physical layer releases the packet structures
    // when it's finished with them.
    //

    if ((IP6_IS_MULTICAST_ADDRESS(RemoteAddress->Address) != FALSE) &&
        ((Socket->Flags & NET_SOCKET_FLAG_MULTICAST_LOOPBACK) != 0)) {

        RtlZeroMemory(&ReceiveContext, sizeof(NET_RECEIVE_CONTEXT));
        ReceiveContext.Link = Link;
        ReceiveContext.Network = Socket->Network;
        CurrentEntry = PacketList->Head.Next;
        while (CurrentEntry != &(PacketList->Head)) {
            Packet = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
            CurrentEntry = CurrentEntry->Next;

            //
            // Save and restore the data and footer offsets as the higher
            // level protocols modify them as the packet moves up the stack.
            // Also save and restore the flags and set the checksum offload
            // flags, as if the hardware already checked the packet. It would
            // be silly for the receive path to validate checksums that were
            // just calculated or to validate checksums that were not yet
            // calculated due to offloading.
            //

            DataOffset = Packet->DataOffset;
            FooterOffset = Packet->FooterOffset;
            ReceiveContext.Packet = Packet;
            PacketFlags = Packet->Flags;
            Packet->Flags |= NET_PACKET_FLAG_CHECKSUM_OFFLOAD_MASK;
            NetpIp6ProcessReceivedData(&ReceiveContext);
            Packet->DataOffset = DataOffset;
            Packet->FooterOffset = FooterOffset;
            Packet->Flags = PacketFlags;
        }
    }

    //
    // The packets are all ready to go, send them down the link.
    //

    Send = Link->DataLinkEntry->Interface.Send;
    Status = Send(Link->DataLinkContext,
                  PacketList,
                  &(LinkAddress->PhysicalAddress),
                  PhysicalNetworkAddress,
                  Socket->Network->ParentProtocolNumber);

    if (!KSUCCESS(Status)) {
        goto Ip6SendEnd;
    }

    Status = STATUS_SUCCESS;

Ip6SendEnd:
    if (NetIp6DebugPrintPackets != FALSE) {
        RtlDebugPrint("Net: IP6 Packet sent from ");
        NetDebugPrintAddress(Source);
        RtlDebugPrint(" to ");
        NetDebugPrintAddress(Destination);
        RtlDebugPrint(" : %x.\n", Status);
    }

    return Status;
}

VOID
NetpIp6ProcessReceivedData (
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

    IP6_ADDRESS DestinationAddress;
    PIP6_HEADER Header;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketLength;
    PNET_PROTOCOL_ENTRY ProtocolEntry;
    IP6_ADDRESS SourceAddress;
    KSTATUS Status;
    USHORT TotalLength;
    ULONG Version;
    ULONG VersionClassFlow;

    Packet = ReceiveContext->Packet;
    PacketLength = Packet->FooterOffset - Packet->DataOffset;

    //
    // Make sure a header is even present.
    //

    if (PacketLength < sizeof(IP6_HEADER)) {
        RtlDebugPrint("Invalid IPv6 packet length: 0x%08x.\n", PacketLength);
        goto Ip6ProcessReceivedDataEnd;
    }

    //
    // Check the protocol version.
    //

    Header = (PIP6_HEADER)(Packet->Buffer + Packet->DataOffset);
    VersionClassFlow = NETWORK_TO_CPU32(Header->VersionClassFlow);
    Version = (VersionClassFlow & IP6_VERSION_MASK) >> IP6_VERSION_SHIFT;
    if (Version != IP6_VERSION) {
        RtlDebugPrint("Invalid IPv6 version. Byte: 0x%02x.\n", Version);
        goto Ip6ProcessReceivedDataEnd;
    }

    //
    // Validate the total length field.
    //

    TotalLength = NETWORK_TO_CPU16(Header->PayloadLength);
    if (TotalLength > (PacketLength - sizeof(IP6_HEADER))) {
        RtlDebugPrint("Invalid IPv6 total length %d is bigger than packet "
                      "data, which is only %d bytes large.\n",
                      TotalLength,
                      (PacketLength - sizeof(IP6_HEADER)));

        goto Ip6ProcessReceivedDataEnd;
    }

    //
    // Initialize the network address.
    //

    RtlZeroMemory(&SourceAddress, sizeof(NETWORK_ADDRESS));
    RtlZeroMemory(&DestinationAddress, sizeof(NETWORK_ADDRESS));
    SourceAddress.Domain = NetDomainIp6;
    RtlCopyMemory(&(SourceAddress.Address),
                  &(Header->SourceAddress),
                  IP6_ADDRESS_SIZE);

    DestinationAddress.Domain = NetDomainIp6;
    RtlCopyMemory(&(DestinationAddress.Address),
                  &(Header->DestinationAddress),
                  IP6_ADDRESS_SIZE);

    //
    // Update the packet's size. Raw sockets should get everything at the IPv6
    // layer. So, lop any footers beyond the IPv6 packet. IPv6 has no footer
    // itself.
    //

    Packet->FooterOffset = Packet->DataOffset +
                           sizeof(IP6_HEADER) +
                           TotalLength;

    //
    // Notify the debugger of a complete packet's arrival.
    //

    if (NetIp6DebugPrintPackets != FALSE) {
        RtlDebugPrint("Net: IP6 Packet received from ");
        NetDebugPrintAddress((PNETWORK_ADDRESS)&SourceAddress);
        RtlDebugPrint(" to ");
        NetDebugPrintAddress((PNETWORK_ADDRESS)&DestinationAddress);
        RtlDebugPrint("\n");
    }

    //
    // Record if the packet has a link-local or maximum hop limit.
    //

    if (Header->HopLimit == IP6_LINK_LOCAL_HOP_LIMIT) {
        Packet->Flags |= NET_PACKET_FLAG_LINK_LOCAL_HOP_LIMIT;

    } else if (Header->HopLimit == IP6_MAX_HOP_LIMIT) {
        Packet->Flags |= NET_PACKET_FLAG_MAX_HOP_LIMIT;
    }

    //
    // Add the source and destination addresses to the receive context.
    //

    ReceiveContext->Source = (PNETWORK_ADDRESS)&SourceAddress;
    ReceiveContext->Destination = (PNETWORK_ADDRESS)&DestinationAddress;
    ReceiveContext->ParentProtocolNumber = Header->NextHeader;

    //
    // Give raw sockets a chance to look at the packet.
    //

    ProtocolEntry = NetGetProtocolEntry(SOCKET_INTERNET_PROTOCOL_RAW);
    if (ProtocolEntry != NULL) {
        ReceiveContext->Protocol = ProtocolEntry;
        ProtocolEntry->Interface.ProcessReceivedData(ReceiveContext);
        ReceiveContext->Protocol = NULL;
    }

    //
    // Parse the IPv6 extension headers. This will find the targeted
    // upper-layer protocol, if any, and fill out the receive context.
    //

    Status = NetpIp6ProcessExtensionHeaders(ReceiveContext);
    if (!KSUCCESS(Status)) {
        goto Ip6ProcessReceivedDataEnd;
    }

    //
    // Pass the packet up the stack if an upper-layer protocol was found.
    //

    if (ReceiveContext->Protocol != NULL) {
        ProtocolEntry = ReceiveContext->Protocol;
        ProtocolEntry->Interface.ProcessReceivedData(ReceiveContext);
    }

Ip6ProcessReceivedDataEnd:
    return;
}

ULONG
NetpIp6PrintAddress (
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

    PUCHAR BytePointer;
    LONG CurrentRun;
    LONG CurrentRunSize;
    PIP6_ADDRESS Ip6Address;
    ULONG Length;
    UINTN RemainingSize;
    PSTR String;
    UINTN StringSize;
    LONG WinnerRun;
    LONG WinnerRunSize;
    ULONG WordCount;
    ULONG WordIndex;
    USHORT Words[IP6_ADDRESS_SIZE / sizeof(USHORT)];
    CHAR WorkingString[IP6_MAX_ADDRESS_STRING_SIZE];

    if (Address == NULL) {
        return IP6_MAX_ADDRESS_STRING_SIZE;
    }

    ASSERT(Address->Domain== NetDomainIp6);

    Ip6Address = (PIP6_ADDRESS)Address;

    //
    // Copy the address into its word array.
    //

    BytePointer = (PUCHAR)(Ip6Address->Address);
    WordCount = IP6_ADDRESS_SIZE / sizeof(USHORT);
    for (WordIndex = 0; WordIndex < WordCount; WordIndex += 1) {
        Words[WordIndex] = ((BytePointer[WordIndex * 2]) << 8) |
                           BytePointer[(WordIndex * 2) + 1];
    }

    //
    // Find the longest run of zeroes in the array. This makes for a nice
    // interview question.
    //

    WinnerRun = -1;
    WinnerRunSize = 0;
    CurrentRun = -1;
    CurrentRunSize = 0;
    for (WordIndex = 0; WordIndex < WordCount; WordIndex += 1) {

        //
        // If a zero is found, start or update the current run.
        //

        if (Words[WordIndex] == 0) {
            if (CurrentRun == -1) {
                CurrentRun = WordIndex;
                CurrentRunSize = 1;

            } else {
                CurrentRunSize += 1;
            }

            //
            // Keep the max up to date as well.
            //

            if (CurrentRunSize > WinnerRunSize) {
                WinnerRun = CurrentRun;
                WinnerRunSize = CurrentRunSize;
            }

        //
        // The run is broken.
        //

        } else {
            CurrentRun = -1;
            CurrentRunSize = 0;
        }
    }

    //
    // Print the formatted string.
    //

    String = WorkingString;
    if (Ip6Address->Port != 0) {
        *String = '[';
        String += 1;
    }

    for (WordIndex = 0; WordIndex < WordCount; WordIndex += 1) {

        //
        // Represent the run of zeros with a single extra colon (so it looks
        // like "::").
        //

        if ((WinnerRun != -1) && (WordIndex >= WinnerRun) &&
            (WordIndex < (WinnerRun + WinnerRunSize))) {

            if (WordIndex == WinnerRun) {
                *String = ':';
                String += 1;
            }

            continue;
        }

        //
        // Every number is preceded by a colon except the first.
        //

        if (WordIndex != 0) {
            *String = ':';
            String += 1;
        }

        //
        // Potentially print an encapsulated IPv4 address.
        //

        if ((WordIndex == 6) && (WinnerRun == 0) &&
            ((WinnerRunSize == 6) ||
             ((WinnerRunSize == 5) && (Words[5] == 0xFFFF)))) {

            StringSize = (UINTN)String - (UINTN)WorkingString;
            RemainingSize = IP6_MAX_ADDRESS_STRING_SIZE - StringSize;
            Length = RtlPrintToString(String,
                                      RemainingSize,
                                      CharacterEncodingDefault,
                                      "%d.%d.%d.%d",
                                      BytePointer[12],
                                      BytePointer[13],
                                      BytePointer[14],
                                      BytePointer[15]);

            String += (Length - 1);
            break;
        }

        Length = RtlPrintToString(String,
                                  5,
                                  CharacterEncodingDefault,
                                  "%x",
                                  Words[WordIndex]);

        String += (Length - 1);
    }

    //
    // If the winning run of zeros goes to the end, then a final extra colon
    // is needed since the lower half of the preceding loop never got a chance
    // to run.
    //

    if ((WinnerRun != -1) && ((WinnerRun + WinnerRunSize) == WordCount)) {
        *String = ':';
        String += 1;
    }

    if (Ip6Address->Port != 0) {
        *String = ']';
        String += 1;
        StringSize = (UINTN)String - (UINTN)WorkingString;
        RemainingSize = IP6_MAX_ADDRESS_STRING_SIZE - StringSize;
        Length = RtlPrintToString(String,
                                  RemainingSize,
                                  CharacterEncodingDefault,
                                  "%d",
                                  Ip6Address->Port);

        String += (Length - 1);
    }

    //
    // Null terminate the string.
    //

    *String = '\0';
    String += 1;
    StringSize = (UINTN)String - (UINTN)WorkingString;

    ASSERT(StringSize <= IP6_MAX_ADDRESS_STRING_SIZE);

    if ((Buffer != NULL) && (BufferLength >= StringSize)) {
        RtlCopyMemory(Buffer, WorkingString, StringSize);
    }

    return (ULONG)StringSize;
}

KSTATUS
NetpIp6GetSetInformation (
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

    UCHAR ByteOption;
    ULONG IntegerOption;
    PIP6_ADDRESS InterfaceAddress;
    PSOCKET_IP6_MULTICAST_REQUEST Ip6MulticastRequest;
    SOCKET_IP6_OPTION Ip6Option;
    PIP6_ADDRESS MulticastAddress;
    NET_SOCKET_MULTICAST_REQUEST MulticastRequest;
    PNET_PROTOCOL_ENTRY Protocol;
    UINTN RequiredSize;
    PVOID Source;
    KSTATUS Status;

    if (InformationType != SocketInformationIp6) {
        Status = STATUS_INVALID_PARAMETER;
        goto Ip6GetSetInformationEnd;
    }

    RequiredSize = 0;
    Source = NULL;
    Status = STATUS_SUCCESS;
    Protocol = Socket->Protocol;
    Ip6Option = (SOCKET_IP4_OPTION)Option;
    switch (Ip6Option) {
    case SocketIp6OptionUnicastHops:
        RequiredSize = sizeof(ULONG);
        if (Set != FALSE) {
            if (*DataSize < RequiredSize) {
                *DataSize = RequiredSize;
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            IntegerOption = *((PULONG)Data);
            if (IntegerOption > MAX_UCHAR) {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            Socket->HopLimit = (UCHAR)IntegerOption;

        } else {
            Source = &IntegerOption;
            IntegerOption = Socket->HopLimit;
        }

        break;

    case SocketIp6OptionJoinMulticastGroup:
    case SocketIp6OptionLeaveMulticastGroup:
        if (Set == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

        //
        // This is not allowed on connection based protocols.
        //

        if ((Protocol->Flags & NET_PROTOCOL_FLAG_CONNECTION_BASED) != 0) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

        RequiredSize = sizeof(SOCKET_IP6_MULTICAST_REQUEST);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        Ip6MulticastRequest = (PSOCKET_IP6_MULTICAST_REQUEST)Data;
        if (!IP6_IS_MULTICAST_ADDRESS(Ip6MulticastRequest->Address)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        RtlZeroMemory(&MulticastRequest, sizeof(NET_SOCKET_MULTICAST_REQUEST));
        MulticastAddress = (PIP6_ADDRESS)&(MulticastRequest.MulticastAddress);
        MulticastAddress->Domain = NetDomainIp6;
        RtlCopyMemory(MulticastAddress->Address,
                      Ip6MulticastRequest->Address,
                      IP6_ADDRESS_SIZE);

        InterfaceAddress = (PIP6_ADDRESS)&(MulticastRequest.InterfaceAddress);
        InterfaceAddress->Domain = NetDomainIp6;
        MulticastRequest.InterfaceId = Ip6MulticastRequest->Interface;
        if (Ip6Option == SocketIp6OptionJoinMulticastGroup) {
            Status = NetJoinSocketMulticastGroup(Socket, &MulticastRequest);

        } else {
            Status = NetLeaveSocketMulticastGroup(Socket, &MulticastRequest);
        }

        goto Ip6GetSetInformationEnd;

    case SocketIp6OptionMulticastHops:
        RequiredSize = sizeof(UCHAR);
        if (Set != FALSE) {
            if (*DataSize < RequiredSize) {
                *DataSize = RequiredSize;
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            ByteOption = *((PUCHAR)Data);
            Socket->MulticastHopLimit = ByteOption;

        } else {
            Source = &ByteOption;
            ByteOption = Socket->MulticastHopLimit;
        }

        break;

    case SocketIp6OptionMulticastInterface:
        RequiredSize = sizeof(ULONG);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        //
        // Multiple structure types are allowed for the set. The size is used
        // to determine which one was supplied.
        //

        MulticastAddress = (PIP6_ADDRESS)&(MulticastRequest.MulticastAddress);
        InterfaceAddress = (PIP6_ADDRESS)&(MulticastRequest.InterfaceAddress);
        if (Set != FALSE) {
            RtlZeroMemory(&MulticastRequest,
                          sizeof(NET_SOCKET_MULTICAST_REQUEST));

            MulticastAddress->Domain = NetDomainIp6;
            InterfaceAddress->Domain = NetDomainIp6;
            MulticastRequest.InterfaceId = *((PULONG)Data);
            Status = NetSetSocketMulticastInterface(Socket, &MulticastRequest);
            if (!KSUCCESS(Status)) {
                break;
            }

        //
        // A get request only ever returns the IPv6 address of the interface.
        //

        } else {
            Status = NetGetSocketMulticastInterface(Socket, &MulticastRequest);
            if (!KSUCCESS(Status)) {
                break;
            }

            IntegerOption = MulticastRequest.InterfaceId;
            Source = &IntegerOption;
        }

        break;

    case SocketIp6OptionMulticastLoopback:
        RequiredSize = sizeof(UCHAR);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (Set != FALSE) {
            ByteOption = *((PUCHAR)Data);
            if (ByteOption != FALSE) {
                RtlAtomicOr32(&(Socket->Flags),
                              NET_SOCKET_FLAG_MULTICAST_LOOPBACK);

            } else {
                RtlAtomicAnd32(&(Socket->Flags),
                               ~NET_SOCKET_FLAG_MULTICAST_LOOPBACK);
            }

        } else {
            Source = &ByteOption;
            ByteOption = FALSE;
            if ((Socket->Flags & NET_SOCKET_FLAG_MULTICAST_LOOPBACK) != 0) {
                ByteOption = TRUE;
            }
        }

        break;

    default:
        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto Ip6GetSetInformationEnd;
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
            goto Ip6GetSetInformationEnd;
        }
    }

Ip6GetSetInformationEnd:
    return Status;
}

NET_ADDRESS_TYPE
NetpIp6GetAddressType (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine gets the type of the given address, categorizing it as unicast,
    broadcast, or multicast.

Arguments:

    Link - Supplies an optional pointer to the network link to which the
        address is bound.

    LinkAddressEntry - Supplies an optional pointer to a network link address
        entry to use while classifying the address.

    Address - Supplies a pointer to the network address to categorize.

Return Value:

    Returns the type of the specified address.

--*/

{

    PIP6_ADDRESS Ip6Address;

    if (Address->Domain != NetDomainIp6) {
        return NetAddressUnknown;
    }

    Ip6Address = (PIP6_ADDRESS)Address;
    if (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Address->Address) != FALSE) {
        return NetAddressAny;
    }

    if (IP6_IS_MULTICAST_ADDRESS(Ip6Address->Address) != FALSE) {
        return NetAddressMulticast;
    }

    return NetAddressUnicast;
}

ULONG
NetpIp6ChecksumPseudoHeader (
    PNETWORK_ADDRESS Source,
    PNETWORK_ADDRESS Destination,
    ULONG PacketLength,
    UCHAR Protocol
    )

/*++

Routine Description:

    This routine computes the network's pseudo-header checksum as the one's
    complement sum of all 32-bit words in the header. The pseudo-header is
    folded into a 16-bit checksum by the caller.

Arguments:

    Source - Supplies a pointer to the source address.

    Destination - Supplies a pointer to the destination address.

    PacketLength - Supplies the packet length to include in the pseudo-header.

    Protocol - Supplies the protocol value used in the pseudo header.

Return Value:

    Returns the checksum of the pseudo-header.

--*/

{

    ULONG Checksum;
    ULONG Index;
    PULONG LongPointer;
    ULONG NextValue;

    ASSERT(Source->Domain == NetDomainIp6);
    ASSERT(Destination->Domain == NetDomainIp6);

    Checksum = 0;
    LongPointer = (PULONG)&(Source->Address);
    for (Index = 0; Index < (IP6_ADDRESS_SIZE / sizeof(ULONG)); Index += 1) {
        Checksum += LongPointer[Index];
        if (Checksum < LongPointer[Index]) {
            Checksum += 1;
        }
    }

    LongPointer = (PULONG)&(Destination->Address);
    for (Index = 0; Index < (IP6_ADDRESS_SIZE / sizeof(ULONG)); Index += 1) {
        Checksum += LongPointer[Index];
        if (Checksum < LongPointer[Index]) {
            Checksum += 1;
        }
    }

    NextValue = CPU_TO_NETWORK32(PacketLength);
    Checksum += NextValue;
    if (Checksum < NextValue) {
        Checksum += 1;
    }

    NextValue = CPU_TO_NETWORK32(Protocol);
    Checksum += NextValue;
    if (Checksum < NextValue) {
        Checksum += 1;
    }

    return Checksum;
}

KSTATUS
NetpIp6ConfigureLinkAddress (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    BOOL Configure
    )

/*++

Routine Description:

    This routine configures or dismantles the given link address for use over
    the network on the given link.

Arguments:

    Link - Supplies a pointer to the link to which the address entry belongs.

    LinkAddress - Supplies a pointer to the link address entry to configure.

    Configure - Supplies a boolean indicating whether or not the link address
        should be configured for use (TRUE) or taken out of service (FALSE).

Return Value:

    Status code.

--*/

{

    UINTN Option;
    PNET_PROTOCOL_ENTRY Protocol;
    ICMP6_ADDRESS_CONFIGURATION_REQUEST Request;
    UINTN RequestSize;
    KSTATUS Status;

    //
    // ICMPv6 handles address configuration, hand off to the protocol.
    //

    Protocol = NetGetProtocolEntry(SOCKET_INTERNET_PROTOCOL_ICMP6);
    if (Protocol == NULL) {
        return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
    }

    RequestSize = sizeof(ICMP6_ADDRESS_CONFIGURATION_REQUEST);
    RtlZeroMemory(&Request, RequestSize);
    Request.Link = Link;
    Request.LinkAddress = LinkAddress;
    Request.Configure = Configure;
    Option = SocketIcmp6OptionConfigureAddress;
    Status = Protocol->Interface.GetSetInformation(NULL,
                                                   SocketInformationIcmp6,
                                                   Option,
                                                   &Request,
                                                   &RequestSize,
                                                   TRUE);

    return Status;
}

KSTATUS
NetpIp6JoinLeaveMulticastGroup (
    PNET_NETWORK_MULTICAST_REQUEST Request,
    BOOL Join
    )

/*++

Routine Description:

    This routine joins or leaves a multicast group using a network-specific
    protocol.

Arguments:

    Request - Supplies a pointer to the multicast group join/leave request.

    Join - Supplies a boolean indicating whether to join (TRUE) or leave
        (FALSE) the multicast group.

Return Value:

    Status code.

--*/

{

    UINTN Option;
    PNET_PROTOCOL_ENTRY Protocol;
    UINTN RequestSize;
    KSTATUS Status;

    //
    // This isn't going to get very far without ICMPv6 support.
    //

    Protocol = NetGetProtocolEntry(SOCKET_INTERNET_PROTOCOL_ICMP6);
    if (Protocol == NULL) {
        return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
    }

    //
    // ICMPv6 actually doesn't depend on a socket to join/leave a multicast
    // group. Don't bother passing one around.
    //

    Option = SocketIcmp6OptionLeaveMulticastGroup;
    if (Join != FALSE) {
        Option = SocketIcmp6OptionJoinMulticastGroup;
    }

    RequestSize = sizeof(NET_NETWORK_MULTICAST_REQUEST);
    Status = Protocol->Interface.GetSetInformation(NULL,
                                                   SocketInformationIcmp6,
                                                   Option,
                                                   Request,
                                                   &RequestSize,
                                                   TRUE);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
NetpIp6TranslateNetworkAddress (
    PNET_NETWORK_ENTRY Network,
    PNETWORK_ADDRESS NetworkAddress,
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine translates a network level address to a physical address.

Arguments:

    Network - Supplies a pointer to the network requesting translation.

    NetworkAddress - Supplies a pointer to the network address to translate.

    Link - Supplies a pointer to the link to use.

    LinkAddress - Supplies a pointer to the link address entry to use for this
        request.

    PhysicalAddress - Supplies a pointer where the corresponding physical
        address for this network address will be returned.

Return Value:

    Status code.

--*/

{

    NET_ADDRESS_TYPE AddressType;
    PIP6_ADDRESS Ip6Address;
    KSTATUS Status;

    AddressType = NetAddressUnknown;
    Ip6Address = (PIP6_ADDRESS)NetworkAddress;

    //
    // This function is very simple: it perform some filtering on known
    // addresses, and if none of those match passes it on to the link layer.
    // Start by checking against the unspecified address.
    //

    if (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Address->Address) != FALSE) {
        return STATUS_INVALID_ADDRESS;
    }

    //
    // Check against the broadcast address, which automatically translates to
    // the broadcast link address.
    //

    Status = STATUS_SUCCESS;
    if (IP6_IS_MULTICAST_ADDRESS(Ip6Address->Address) != FALSE) {
        AddressType = NetAddressMulticast;
        goto Ip6TranslateNetworkAddressEnd;
    }

    //
    // Well, it looks like a run-of-the-mill IP address, translate it.
    //
    // TODO: IPv6 neighbor discovery.
    //

    ASSERT(FALSE);

    AddressType = NetAddressUnicast;
    Status = STATUS_NOT_IMPLEMENTED;

Ip6TranslateNetworkAddressEnd:

    //
    // Multicast addresses need to be translated by the data link layer.
    //

    if (AddressType == NetAddressMulticast) {
        Status = Link->DataLinkEntry->Interface.ConvertToPhysicalAddress(
                                                               NetworkAddress,
                                                               PhysicalAddress,
                                                               AddressType);
    }

    return Status;
}

KSTATUS
NetpIp6ProcessExtensionHeaders (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine processes an IPv6 packet's extension headers. It fills out the
    upper-layer protocol entry in the receive context if one is found.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link and packet information.

Return Value:

    Status code.

--*/

{

    UCHAR Code;
    PIP6_EXTENSION_HEADER Extension;
    PIP6_EXTENSION_HEADER FirstExtension;
    PIP6_HEADER Header;
    ULONG HeaderOffset;
    UCHAR NextHeader;
    ULONG NextHeaderOffset;
    PIP6_OPTION Option;
    ULONG OptionBytesRemaining;
    ULONG OptionLength;
    PNET_PACKET_BUFFER Packet;
    KSTATUS Status;
    BOOL UnrecognizedHeader;

    ReceiveContext->Protocol = NULL;
    Packet = ReceiveContext->Packet;
    HeaderOffset = Packet->DataOffset;
    Header = (PIP6_HEADER)(Packet->Buffer + Packet->DataOffset);
    NextHeader = Header->NextHeader;
    NextHeaderOffset = HeaderOffset + FIELD_OFFSET(IP6_HEADER, NextHeader);
    Packet->DataOffset += sizeof(IP6_HEADER);
    Extension = (PIP6_EXTENSION_HEADER)(Packet->Buffer + Packet->DataOffset);
    FirstExtension = Extension;

    //
    // Although the IPv6 specification recommands an order to the extension
    // headers, be flexible. But do process them in the order in which they
    // were sent, without skipping to search for a specific header.
    //

    Status = STATUS_SUCCESS;
    UnrecognizedHeader = FALSE;
    while (TRUE) {
        switch (NextHeader) {

        //
        // The "no next header" value is the end of the line. Treat this as
        // a success, because any additional processing (e.g. forwarding)
        // should happen. There just isn't an upper-layer protocol.
        //

        case SOCKET_INTERNET_PROTOCOL_IPV6_NO_NEXT:
            goto Ip6ProcessExtensionHeadersEnd;

        //
        // The Hop-by-hop options header must be the first extension. If it is
        // not, then send an ICMP "unrecognized next header" message.
        //

        case SOCKET_INTERNET_PROTOCOL_HOPOPT:
            if (Extension != FirstExtension) {
                UnrecognizedHeader = TRUE;
                break;
            }

            Option = (PIP6_OPTION)(Extension + 1);
            OptionBytesRemaining = IP6_EXTENSION_HEADER_LENGTH_BASE;
            OptionBytesRemaining += (Extension->Length *
                                     IP6_EXTENSION_HEADER_LENGTH_MULTIPLE);

            OptionBytesRemaining -= sizeof(IP6_EXTENSION_HEADER);
            while (OptionBytesRemaining != 0) {
                OptionLength = Option->Length + sizeof(IP6_OPTION);
                switch (Option->Type) {
                case IP6_OPTION_TYPE_PAD1:
                    OptionLength = 1;
                    break;

                case IP6_OPTION_TYPE_ROUTER_ALERT:
                    Packet->Flags |= NET_PACKET_FLAG_ROUTER_ALERT;
                    break;

                case IP6_OPTION_TYPE_PADN:
                default:
                    break;
                }

                Option = (PIP6_OPTION)((PVOID)Option + OptionLength);
                OptionBytesRemaining -= OptionLength;
            }

            break;

        //
        // TODO: Parse IPv6 extension headers.
        //

        case SOCKET_INTERNET_PROTOCOL_IPV6_ROUTING:
        case SOCKET_INTERNET_PROTOCOL_IPV6_FRAGMENT:
        case SOCKET_INTERNET_PROTOCOL_ESP:
        case SOCKET_INTERNET_PROTOCOL_AH:
        case SOCKET_INTERNET_PROTOCOL_IPV6_DESTINATION:
        case SOCKET_INTERNET_PROTOCOL_IPV6_MOBILITY:
        case SOCKET_INTERNET_PROTOCOL_HIP:
        case SOCKET_INTERNET_PROTOCOL_SHIM6:
        case SOCKET_INTERNET_PROTOCOL_TEST1:
        case SOCKET_INTERNET_PROTOCOL_TEST2:
            RtlDebugPrint("IPv6: Unhandled extension header 0x%02x\n",
                          NextHeader);

            break;

        //
        // The first next header value that is not in the known list should be
        // from an upper-layer protocol.
        //

        default:
            ReceiveContext->Protocol = NetGetProtocolEntry(NextHeader);
            if (ReceiveContext->Protocol != NULL) {
                goto Ip6ProcessExtensionHeadersEnd;
            }

            UnrecognizedHeader = TRUE;
            break;
        }

        //
        // If an unknown next header arrived, send the unrecognized extension
        // header error message.
        //

        if (UnrecognizedHeader != FALSE) {
            Packet->DataOffset = HeaderOffset;
            Code = ICMP6_PARAMETER_PROBLEM_CODE_UNRECOGNIZED_NEXT_HEADER;
            NetpIp6SendParameterProblemMessage(ReceiveContext,
                                               Code,
                                               NextHeaderOffset);

            Status = STATUS_UNSUCCESSFUL;
            goto Ip6ProcessExtensionHeadersEnd;
        }

        //
        // Get the type of and a pointer to the next extension header.
        //

        NextHeader = Extension->NextHeader;
        NextHeaderOffset = Packet->DataOffset;
        Packet->DataOffset += IP6_EXTENSION_HEADER_LENGTH_BASE;
        Packet->DataOffset += (Extension->Length *
                               IP6_EXTENSION_HEADER_LENGTH_MULTIPLE);

        Extension = (PIP6_EXTENSION_HEADER)(Packet->Buffer +
                                            Packet->DataOffset);
    }

Ip6ProcessExtensionHeadersEnd:
    return Status;
}

VOID
NetpIp6SendParameterProblemMessage (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    UCHAR Code,
    ULONG Pointer
    )

/*++

Routine Description:

    This routine sends an ICMPv6 parameter problem message in response to a bad
    IPv6 packet.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context for the
        problematic packet.

    Code - Supplies the parameter problem code to send.

    Pointer - Supplies the pointer to set in the paramter problem message.

Return Value:

    None.

--*/

{

    USHORT Checksum;
    PNETWORK_ADDRESS Destination;
    NETWORK_ADDRESS DestinationPhysical;
    ULONG Flags;
    PICMP6_HEADER Icmp6Header;
    ULONG Icmp6Length;
    PIP6_HEADER Ip6Header;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PNET_PACKET_BUFFER Message;
    PVOID MessageData;
    PULONG MessagePointer;
    ULONG MessageSize;
    PNET_PACKET_BUFFER Packet;
    PVOID PacketData;
    NET_PACKET_LIST PacketList;
    ULONG PayloadLength;
    PNET_DATA_LINK_SEND Send;
    PNETWORK_ADDRESS Source;
    KSTATUS Status;
    ULONG VersionClassFlow;

    NET_INITIALIZE_PACKET_LIST(&PacketList);
    Link = ReceiveContext->Link;
    Packet = ReceiveContext->Packet;

    //
    // Switch the destination and source.
    //

    Source = ReceiveContext->Destination;
    Destination = ReceiveContext->Source;

    //
    // Allocate a packet to hold the ICMPv6 parameter problem message.
    //

    MessageSize = sizeof(ULONG) + (Packet->FooterOffset - Packet->DataOffset);
    if (MessageSize > IP6_MINIMUM_LINK_MTU) {
        MessageSize = IP6_MINIMUM_LINK_MTU;
    }

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Status = NetAllocateBuffer(sizeof(ICMP6_HEADER) + sizeof(IP6_HEADER),
                               MessageSize,
                               0,
                               Link,
                               Flags,
                               &Message);

    if (!KSUCCESS(Status)) {
        goto Ip6SendParameterProblemMessage;
    }

    NET_ADD_PACKET_TO_LIST(Message, &PacketList);

    //
    // Copy the pointer and as much of the problem packet into the body of the
    // ICMPv6 message.
    //

    MessagePointer = (PULONG)(Message->Buffer + Message->DataOffset);
    *MessagePointer = Pointer;
    MessageData = (PVOID)(MessagePointer + 1);
    PacketData = Packet->Buffer + Packet->DataOffset;
    RtlCopyMemory(MessageData, PacketData, MessageSize - sizeof(ULONG));

    //
    // Set the ICMPv6 header.
    //

    Message->DataOffset -= sizeof(ICMP6_HEADER);
    Icmp6Header = (PICMP6_HEADER)(Message->Buffer + Message->DataOffset);
    Icmp6Header->Type = ICMP6_MESSAGE_TYPE_PARAMETER_PROBLEM;
    Icmp6Header->Code = Code;
    Icmp6Header->Checksum = 0;
    Icmp6Length = Packet->FooterOffset - Packet->DataOffset;
    Checksum = NetChecksumPseudoHeaderAndData(ReceiveContext->Network,
                                              Icmp6Header,
                                              Icmp6Length,
                                              Source,
                                              Destination,
                                              SOCKET_INTERNET_PROTOCOL_ICMP6);

    Icmp6Header->Checksum = Checksum;

    //
    // Set the IPv6 header.
    //

    PayloadLength = Message->FooterOffset - Message->DataOffset;

    ASSERT(PayloadLength <= IP6_MAX_PAYLOAD_LENGTH);

    Message->DataOffset -= sizeof(IP6_HEADER);
    Ip6Header = (PIP6_HEADER)(Message->Buffer + Message->DataOffset);
    VersionClassFlow = (IP6_VERSION << IP6_VERSION_SHIFT) & IP6_VERSION_MASK;
    Ip6Header->VersionClassFlow = CPU_TO_NETWORK32(VersionClassFlow);
    Ip6Header->PayloadLength = CPU_TO_NETWORK16((USHORT)PayloadLength);
    Ip6Header->NextHeader = SOCKET_INTERNET_PROTOCOL_ICMP6;
    Ip6Header->HopLimit = IP6_DEFAULT_HOP_LIMIT;
    RtlCopyMemory(Ip6Header->SourceAddress, Source->Address, IP6_ADDRESS_SIZE);
    RtlCopyMemory(Ip6Header->DestinationAddress,
                  Destination->Address,
                  IP6_ADDRESS_SIZE);

    //
    // Get the source and destination physical addresses. Getting the
    // destination physical address should not need to use NDP, as the result
    // should already be in the cache.
    //

    Status = NetFindEntryForAddress(Link, Source, &LinkAddress);
    if (!KSUCCESS(Status)) {
        goto Ip6SendParameterProblemMessage;
    }

    Status = NetpIp6TranslateNetworkAddress(ReceiveContext->Network,
                                            Destination,
                                            Link,
                                            LinkAddress,
                                            &DestinationPhysical);

    if (!KSUCCESS(Status)) {
        goto Ip6SendParameterProblemMessage;
    }

    //
    // Send the message down to the data link layer.
    //

    Send = Link->DataLinkEntry->Interface.Send;
    Status = Send(Link->DataLinkContext,
                  &PacketList,
                  &(LinkAddress->PhysicalAddress),
                  &DestinationPhysical,
                  IP6_PROTOCOL_NUMBER);

    if (!KSUCCESS(Status)) {
        goto Ip6SendParameterProblemMessage;
    }

Ip6SendParameterProblemMessage:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return;
}

