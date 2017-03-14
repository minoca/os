/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ip4.c

Abstract:

    This module implements support for the Internet Protocol version 4 (IPv4).

Author:

    Evan Green 4-Apr-2013

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
#include <minoca/net/ip4.h>
#include <minoca/net/igmp.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum size of an IPv4 address string, including the null
// terminator. The longest string would look something like
// "255.255.255.255:65535"
//

#define IP4_MAX_ADDRESS_STRING 22

//
// Define the timeout for a set if IPv4 fragments, in microseconds.
//

#define IP4_FRAGMENT_TIMEOUT (15 * MICROSECONDS_PER_SECOND)

//
// Define the byte alignment for IPv4 fragment lengths.
//

#define IP4_FRAGMENT_ALIGNMENT 8

//
// Define the maximum number of fragments that can be stored at any one time.
//

#define IP4_MAX_FRAGMENT_COUNT 1000

//
// Define IPv4 the socket information flags.
//

#define IP4_SOCKET_FLAG_MULTICAST_LOOPBACK 0x00000001

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros to convert between fragment bytes and fragment offset units.
//

#define IP4_CONVERT_OFFSET_TO_BYTES(_Offset) (_Offset << 3)
#define IP4_CONVERT_BYTES_TO_OFFSET(_Bytes) (_Bytes >> 3)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an IPv4 fragmented packet node which represents one
    IPv4 packet that is in the process of being reassembled from its various
    fragments.

Members:

    Node - Stores the Red-Black tree node information for this IPv4 packet.

    FargmentListHead - Stores the head of the list of fragments being
        reassembled into a packet.

    Timeout - Stores the time, in time ticks, at which the the attempt to
        reassemble this packet will be abandoned.

    LocalAddress - Stores the local IPv4 address for the packet.

    RemoteAddress - Stores the remote IPv4 address for the packet.

    Protocol - Stores the IPv4 protocol for which this packet is destined.

    Identification - Stores the IPv4 unique identification for the packet.

--*/

typedef struct _IP4_FRAGMENTED_PACKET_NODE {
    RED_BLACK_TREE_NODE Node;
    LIST_ENTRY FragmentListHead;
    ULONGLONG Timeout;
    ULONG LocalAddress;
    ULONG RemoteAddress;
    USHORT Protocol;
    USHORT Identification;
} IP4_FRAGMENTED_PACKET_NODE, *PIP4_FRAGMENTED_PACKET_NODE;

/*++

Structure Description:

    This structure defines a fragment entry for an IPv4 packet. This entry
    may contain data from one or more fragments that have already been
    processed.

Members:

    ListEntry - Stores pointer to the next and previous fragments in the list.

    Length - Stores the length, in bytes, of this fragment.

    Offset - Stores the offset, in 8 byte blocks, from the beginning of the
        packet.

    LastFragment - Stores a boolean indicating whether or not this fragment
        contains the last chunk of data for the packet.

--*/

typedef struct _IP4_FRAGMENT_ENTRY {
    LIST_ENTRY ListEntry;
    ULONG Length;
    USHORT Offset;
    BOOL LastFragment;
} IP4_FRAGMENT_ENTRY, *PIP4_FRAGMENT_ENTRY;

/*++

Structure Description:

    This structure defines a multicast group for an IPv4 socket.

Members:

    ListEntry - Stores pointers to the previous and next multicast groups in
        the socket's list.

    Link - Supplies a pointer to the network link to which the multicast group
        is attached.

    LinkAddress - Supplies a pointer to the link address entry with which the
        multicast group is associated.

    Address - Stores the IPv4 multicast address of the group.

--*/

typedef struct _IP4_MULTICAST_GROUP {
    LIST_ENTRY ListEntry;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    ULONG MulticastAddress;
} IP4_MULTICAST_GROUP, *PIP4_MULTICAST_GROUP;

/*++

Structure Description:

    This structure defines the IP4 socket option information.

Members:

    Flags - Stores a bitmask of IPv4 socket information flags. See
        IP4_SOCKET_FLAG_* for definitions.

    TimeToLive - Stores the time-to-live that is to be set in the IPv4 header
        for every packet sent by this socket.

    DifferentiatedServicesCodePoint - Stores the differentiated services code
        point that is to be set in the IPv4 header for every packet sent by
        this socket.

    MulticastTimeToLive - Stores the time-to-live that is to be set in the IPv4
        header for every multicast packet sent by this socket.

    MulticastInterface - Stores the interface over which to send all multicast
        packets. If this is not initialized, then a default interface is chosen
        just as it would be for unicast packets.

    MulticastLock - Supplies a pointer to the queued lock that protects access
        to the multicast information.

    MulticastGroupList - Stores the head of the list of multicast groups to
        which the socket belongs.

--*/

typedef struct _IP4_SOCKET_INFORMATION {
    volatile ULONG Flags;
    UCHAR TimeToLive;
    UCHAR DifferentiatedServicesCodePoint;
    UCHAR MulticastTimeToLive;
    NET_SOCKET_LINK_OVERRIDE MulticastInterface;
    volatile PQUEUED_LOCK MulticastLock;
    LIST_ENTRY MulticastGroupList;
} IP4_SOCKET_INFORMATION, *PIP4_SOCKET_INFORMATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpIp4InitializeLink (
    PNET_LINK Link
    );

VOID
NetpIp4DestroyLink (
    PNET_LINK Link
    );

KSTATUS
NetpIp4InitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    );

VOID
NetpIp4DestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp4BindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address,
    ULONG Flags
    );

KSTATUS
NetpIp4Listen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp4Connect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpIp4Disconnect (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp4Close (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp4Send (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Destination,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride,
    PNET_PACKET_LIST PacketList
    );

VOID
NetpIp4ProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

ULONG
NetpIp4PrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

KSTATUS
NetpIp4GetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetpIp4CopyInformation (
    PNET_SOCKET DestinationSocket,
    PNET_SOCKET SourceSocket
    );

NET_ADDRESS_TYPE
NetpIp4GetAddressType (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry,
    PNETWORK_ADDRESS Address
    );

USHORT
NetpIp4ChecksumData (
    PVOID Data,
    ULONG Length
    );

KSTATUS
NetpIp4TranslateNetworkAddress (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS NetworkAddress,
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    );

PNET_PACKET_BUFFER
NetpIp4ProcessPacketFragment (
    PNET_LINK Link,
    PNET_PACKET_BUFFER PacketFragment
    );

COMPARISON_RESULT
NetpIp4CompareFragmentedPacketEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

VOID
NetpIp4RemoveFragmentedPackets (
    PNET_SOCKET Socket
    );

PIP4_FRAGMENTED_PACKET_NODE
NetpIp4CreateFragmentedPacketNode (
    PIP4_HEADER Header
    );

VOID
NetpIp4DestroyFragmentedPacketNode (
    PIP4_FRAGMENTED_PACKET_NODE PacketNode
    );

KSTATUS
NetpIp4JoinMulticastGroup (
    PNET_SOCKET Socket,
    PSOCKET_IP4_MULTICAST_REQUEST Request
    );

KSTATUS
NetpIp4LeaveMulticastGroup (
    PNET_SOCKET Socket,
    PSOCKET_IP4_MULTICAST_REQUEST Request
    );

VOID
NetpIp4DestroyMulticastGroups (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIp4FindLinkForMulticastRequest (
    PNET_NETWORK_ENTRY Network,
    PSOCKET_IP4_MULTICAST_REQUEST Request,
    PNET_LINK_LOCAL_ADDRESS LinkResult
    );

KSTATUS
NetpIp4AcquireMulticastLock (
    PIP4_SOCKET_INFORMATION SocketInformation
    );

VOID
NetpIp4ReleaseMulticastLock (
    PIP4_SOCKET_INFORMATION SocketInformation
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL NetIp4DebugPrintPackets = FALSE;

//
// Store values used to manage fragmented IPv4 packets.
//

ULONG NetIp4FragmentCount;
PQUEUED_LOCK NetIp4FragmentedPacketLock;
RED_BLACK_TREE NetIp4FragmentedPacketTree;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpIp4Initialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for IPv4 packets.

Arguments:

    None.

Return Value:

    None.

--*/

{

    NET_NETWORK_ENTRY NetworkEntry;
    KSTATUS Status;

    //
    // Initialize the IPv4 fragmented packet tree.
    //

    RtlRedBlackTreeInitialize(&NetIp4FragmentedPacketTree,
                              0,
                              NetpIp4CompareFragmentedPacketEntries);

    NetIp4FragmentCount = 0;
    NetIp4FragmentedPacketLock = KeCreateQueuedLock();
    if (NetIp4FragmentedPacketLock == NULL) {

        ASSERT(FALSE);

        goto Ip4InitializeEnd;
    }

    //
    // Register the IPv4 handlers with the core networking library.
    //

    RtlZeroMemory(&NetworkEntry, sizeof(NET_NETWORK_ENTRY));
    NetworkEntry.Domain = NetDomainIp4;
    NetworkEntry.ParentProtocolNumber = IP4_PROTOCOL_NUMBER;
    NetworkEntry.Interface.InitializeLink = NetpIp4InitializeLink;
    NetworkEntry.Interface.DestroyLink = NetpIp4DestroyLink;
    NetworkEntry.Interface.InitializeSocket = NetpIp4InitializeSocket;
    NetworkEntry.Interface.DestroySocket = NetpIp4DestroySocket;
    NetworkEntry.Interface.BindToAddress = NetpIp4BindToAddress;
    NetworkEntry.Interface.Listen = NetpIp4Listen;
    NetworkEntry.Interface.Connect = NetpIp4Connect;
    NetworkEntry.Interface.Disconnect = NetpIp4Disconnect;
    NetworkEntry.Interface.Close = NetpIp4Close;
    NetworkEntry.Interface.Send = NetpIp4Send;
    NetworkEntry.Interface.ProcessReceivedData = NetpIp4ProcessReceivedData;
    NetworkEntry.Interface.PrintAddress = NetpIp4PrintAddress;
    NetworkEntry.Interface.GetSetInformation = NetpIp4GetSetInformation;
    NetworkEntry.Interface.CopyInformation = NetpIp4CopyInformation;
    NetworkEntry.Interface.GetAddressType = NetpIp4GetAddressType;
    Status = NetRegisterNetworkLayer(&NetworkEntry, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

Ip4InitializeEnd:
    return;
}

KSTATUS
NetpIp4InitializeLink (
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
    IP4_ADDRESS InitialAddress;
    KSTATUS Status;

    //
    // A dummy address with only the network filled in is required, otherwise
    // this link entry cannot be bound to in order to establish the real
    // address.
    //

    RtlZeroMemory((PNETWORK_ADDRESS)&InitialAddress, sizeof(NETWORK_ADDRESS));
    InitialAddress.Domain = NetDomainIp4;
    InitialAddress.Address = 0;
    Status = NetCreateLinkAddressEntry(Link,
                                       (PNETWORK_ADDRESS)&InitialAddress,
                                       NULL,
                                       NULL,
                                       FALSE,
                                       &AddressEntry);

    if (!KSUCCESS(Status)) {
        goto Ip4InitializeLinkEnd;
    }

Ip4InitializeLinkEnd:
    if (!KSUCCESS(Status)) {
        if (AddressEntry != NULL) {
            NetDestroyLinkAddressEntry(Link, AddressEntry);
        }
    }

    return Status;
}

VOID
NetpIp4DestroyLink (
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
NetpIp4InitializeSocket (
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
    PIP4_SOCKET_INFORMATION SocketInformation;

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
    // Determine if the maximum IPv4 packet size plus all existing headers and
    // footers is less than the current maximum packet size. If so, truncate
    // the maximum packet size. Note that the IPv4 maximum packet size includes
    // the size of the header.
    //

    MaxPacketSize = NewSocket->PacketSizeInformation.HeaderSize +
                    IP4_MAX_PACKET_SIZE +
                    NewSocket->PacketSizeInformation.FooterSize;

    if (NewSocket->PacketSizeInformation.MaxPacketSize > MaxPacketSize) {
        NewSocket->PacketSizeInformation.MaxPacketSize = MaxPacketSize;
    }

    //
    // Add the IPv4 header size for higher layers to perform the same
    // truncation procedure. Skip this for raw sockets using the raw protocol;
    // the must always supply an IPv4 header, so it doesn't make sense to add
    // it to the header size. It comes in the data packet.
    //

    if ((ProtocolEntry->Type != NetSocketRaw) ||
        (NetworkProtocol != SOCKET_INTERNET_PROTOCOL_RAW)) {

        NewSocket->PacketSizeInformation.HeaderSize += sizeof(IP4_HEADER);
    }

    //
    // Allocate and initialize a socket information structure for this socket.
    //

    SocketInformation = MmAllocatePagedPool(sizeof(IP4_SOCKET_INFORMATION),
                                            IP4_ALLOCATION_TAG);

    if (SocketInformation == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(SocketInformation, sizeof(IP4_SOCKET_INFORMATION));
    SocketInformation->Flags = IP4_SOCKET_FLAG_MULTICAST_LOOPBACK;
    SocketInformation->TimeToLive = IP4_INITIAL_TIME_TO_LIVE;
    SocketInformation->MulticastTimeToLive = IP4_INITIAL_MULTICAST_TIME_TO_LIVE;
    SocketInformation->DifferentiatedServicesCodePoint = 0;
    SocketInformation->MulticastLock = NULL;
    INITIALIZE_LIST_HEAD(&(SocketInformation->MulticastGroupList));
    NewSocket->NetworkSocketInformation = SocketInformation;
    return STATUS_SUCCESS;
}

VOID
NetpIp4DestroySocket (
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

    if (Socket->NetworkSocketInformation != NULL) {
        NetpIp4DestroyMulticastGroups(Socket);
        MmFreePagedPool(Socket->NetworkSocketInformation);
        Socket->NetworkSocketInformation = NULL;
    }

    return;
}

KSTATUS
NetpIp4BindToAddress (
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
    PIP4_ADDRESS Ip4Address;
    NET_LINK_LOCAL_ADDRESS LocalInformation;
    ULONG Port;
    KSTATUS Status;

    Ip4Address = (PIP4_ADDRESS)Address;
    LocalInformation.Link = NULL;

    //
    // Classify the address and binding type. Leaving it as unknown is OK.
    // Differentiating between a unicast address and a subnet broadcast address
    // is not possible without the link address entry's information.
    //

    BindingType = SocketLocallyBound;
    if (Ip4Address->Address == 0) {
        BindingType = SocketUnbound;
    }

    //
    // If a specific link is given, try to find the given address in that link.
    //

    if (Link != NULL) {
        Port = Address->Port;
        Address->Port = 0;
        Status = NetFindLinkForLocalAddress(Socket->Network,
                                            Address,
                                            Link,
                                            &LocalInformation);

        Address->Port = Port;
        if (!KSUCCESS(Status)) {
            goto Ip4BindToAddressEnd;
        }

        LocalInformation.ReceiveAddress.Port = Port;
        LocalInformation.SendAddress.Port = Port;

    //
    // No specific link was passed.
    //

    } else {

        //
        // If the address is not the "any" or broadcast address, then look for
        // the link that owns this address.
        //

        if ((Ip4Address->Address != 0) &&
            (Ip4Address->Address != IP4_BROADCAST_ADDRESS)) {

            Port = Address->Port;
            Address->Port = 0;
            Status = NetFindLinkForLocalAddress(Socket->Network,
                                                Address,
                                                NULL,
                                                &LocalInformation);

            Address->Port = Port;
            if (!KSUCCESS(Status)) {
                goto Ip4BindToAddressEnd;
            }

            LocalInformation.ReceiveAddress.Port = Port;
            LocalInformation.SendAddress.Port = Port;

        //
        // No link was passed, this is a generic bind to a port on any or the
        // broadcast address.
        //

        } else {
            LocalInformation.Link = NULL;
            LocalInformation.LinkAddress = NULL;
            RtlCopyMemory(&(LocalInformation.ReceiveAddress),
                          Address,
                          sizeof(NETWORK_ADDRESS));

            //
            // Even in the broadcast case, the send address should be the any
            // address. It should only get the port from the supplied address.
            //

            RtlZeroMemory(&(LocalInformation.SendAddress),
                          sizeof(NETWORK_ADDRESS));

            LocalInformation.SendAddress.Port = Address->Port;
        }
    }

    //
    // Bind the socket to the local address. The socket remains inactive,
    // unable to receive packets.
    //

    Status = NetBindSocket(Socket, BindingType, &LocalInformation, NULL, Flags);
    if (!KSUCCESS(Status)) {
        goto Ip4BindToAddressEnd;
    }

    Status = STATUS_SUCCESS;

Ip4BindToAddressEnd:
    if (LocalInformation.Link != NULL) {
        NetLinkReleaseReference(LocalInformation.Link);
    }

    return Status;
}

KSTATUS
NetpIp4Listen (
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
        LocalAddress.Domain = NetDomainIp4;
        Status = NetpIp4BindToAddress(Socket, NULL, &LocalAddress, 0);
        if (!KSUCCESS(Status)) {
            goto Ip4ListenEnd;
        }
    }

    Status = NetActivateSocket(Socket);
    if (!KSUCCESS(Status)) {
        goto Ip4ListenEnd;
    }

Ip4ListenEnd:
    return Status;
}

KSTATUS
NetpIp4Connect (
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
        goto Ip4ConnectEnd;
    }

    Status = STATUS_SUCCESS;

Ip4ConnectEnd:
    return Status;
}

KSTATUS
NetpIp4Disconnect (
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
        goto Ip4DisconnectEnd;
    }

    Status = STATUS_SUCCESS;

Ip4DisconnectEnd:
    return Status;
}

KSTATUS
NetpIp4Close (
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

    //
    // Now that the socket is deactiviated, destroy any pending fragments.
    //

    if (Socket->LocalReceiveAddress.Domain == NetDomainIp4) {
        KeAcquireQueuedLock(NetIp4FragmentedPacketLock);
        NetpIp4RemoveFragmentedPackets(Socket);
        KeReleaseQueuedLock(NetIp4FragmentedPacketLock);
    }

    return STATUS_SUCCESS;
}

KSTATUS
NetpIp4Send (
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

    ULONG BytesCompleted;
    ULONG BytesRemaining;
    USHORT Checksum;
    PLIST_ENTRY CurrentEntry;
    ULONG DataOffset;
    ULONG FooterOffset;
    ULONG FooterSize;
    PNET_PACKET_BUFFER Fragment;
    ULONG FragmentLength;
    USHORT FragmentOffset;
    PIP4_HEADER Header;
    ULONG HeaderSize;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PIP4_ADDRESS LocalAddress;
    ULONG MaxFragmentLength;
    ULONG MaxPacketSize;
    PNET_SOCKET_LINK_OVERRIDE MulticastInterface;
    PNET_PACKET_BUFFER Packet;
    PVOID PacketBuffer;
    ULONG PacketFlags;
    PNETWORK_ADDRESS PhysicalNetworkAddress;
    NETWORK_ADDRESS PhysicalNetworkAddressBuffer;
    NET_RECEIVE_CONTEXT ReceiveContext;
    PIP4_ADDRESS RemoteAddress;
    PNET_DATA_LINK_SEND Send;
    PIP4_SOCKET_INFORMATION SocketInformation;
    PNETWORK_ADDRESS Source;
    KSTATUS Status;
    ULONG TimeToLive;
    ULONG TotalLength;

    ASSERT((Socket->KernelSocket.Type == NetSocketRaw) ||
           (Socket->KernelSocket.Protocol ==
            Socket->Protocol->ParentProtocolNumber));

    SocketInformation = Socket->NetworkSocketInformation;

    ASSERT(SocketInformation != NULL);

    //
    // Multicast packets must use the multicast time-to-live, which defaults to
    // 1 (rather than 63) as multicast packets aren't typically meant to go
    // beyond the local network.
    //

    TimeToLive = SocketInformation->TimeToLive;
    RemoteAddress = (PIP4_ADDRESS)Destination;
    if (IP4_IS_MULTICAST_ADDRESS(RemoteAddress->Address) != FALSE) {
        TimeToLive = SocketInformation->MulticastTimeToLive;

        //
        // Also use the multicast interface information if it is present.
        //

        MulticastInterface = &(SocketInformation->MulticastInterface);
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

    LocalAddress = (PIP4_ADDRESS)Source;

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
    if ((Destination != &(Socket->RemoteAddress)) ||
        (PhysicalNetworkAddress->Domain == NetDomainInvalid)) {

        if (Destination != &(Socket->RemoteAddress)) {
            PhysicalNetworkAddress = &PhysicalNetworkAddressBuffer;
        }

        Status = NetpIp4TranslateNetworkAddress(Socket,
                                                Destination,
                                                Link,
                                                LinkAddress,
                                                PhysicalNetworkAddress);

        if (!KSUCCESS(Status)) {
            goto Ip4SendEnd;
        }

        ASSERT(PhysicalNetworkAddress->Domain != NetDomainInvalid);
    }

    //
    // Add the IP4 and Ethernet headers to each packet.
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
            goto Ip4SendEnd;

        //
        // If the current packet's total data size (including all headers and
        // footers) is larger than the socket's/link's maximum size, then the
        // IP layer needs to break it into multiple fragments.
        //

        } else if (Packet->DataSize > MaxPacketSize) {

            //
            // Determine the size of the remaining headers and footers that
            // will be added to each fragment. These can be determined from the
            // current packet's data offset, total size and footer offset.
            //

            HeaderSize = Packet->DataOffset;
            FooterSize = Packet->DataSize - Packet->FooterOffset;

            //
            // Determine the maximum size of each fragment based on the headers
            // and footers required.
            //

            MaxFragmentLength = MaxPacketSize - HeaderSize - FooterSize;
            MaxFragmentLength = ALIGN_RANGE_DOWN(MaxFragmentLength,
                                                 IP4_FRAGMENT_ALIGNMENT);

            //
            // Iterate over the current packet, breaking it up into multiple
            // fragments.
            //

            PacketBuffer = Packet->Buffer + Packet->DataOffset;
            BytesCompleted = 0;
            BytesRemaining = Packet->FooterOffset - Packet->DataOffset;
            while (BytesRemaining != 0) {
                FragmentLength = MaxFragmentLength;
                if (FragmentLength > BytesRemaining) {
                    FragmentLength = BytesRemaining;
                }

                Status = NetAllocateBuffer(HeaderSize,
                                           FragmentLength,
                                           FooterSize,
                                           Link,
                                           0,
                                           &Fragment);

                if (!KSUCCESS(Status)) {
                    goto Ip4SendEnd;
                }

                //
                // Copy the data from the packet to the fragment.
                //

                RtlCopyMemory(Fragment->Buffer + Fragment->DataOffset,
                              PacketBuffer,
                              FragmentLength);

                //
                // Get a pointer to the header, which is right before the data.
                //

                ASSERT(Fragment->DataOffset > sizeof(IP4_HEADER));

                Fragment->DataOffset -= sizeof(IP4_HEADER);
                Header = (PIP4_HEADER)(Fragment->Buffer + Fragment->DataOffset);

                //
                // Fill out that IPv4 header.
                //

                Header->VersionAndHeaderLength =
                     IP4_VERSION | (UCHAR)(sizeof(IP4_HEADER) / sizeof(ULONG));

                Header->Type = 0;
                Header->Type |=
                            SocketInformation->DifferentiatedServicesCodePoint;

                TotalLength = Fragment->FooterOffset - Fragment->DataOffset;
                Header->TotalLength = CPU_TO_NETWORK16(TotalLength);
                Header->Identification =
                                     CPU_TO_NETWORK16(Socket->SendPacketCount);

                ASSERT(IS_ALIGNED(BytesCompleted, IP4_FRAGMENT_ALIGNMENT) != 0);

                FragmentOffset = IP4_CONVERT_BYTES_TO_OFFSET(BytesCompleted);
                FragmentOffset &= IP4_FRAGMENT_OFFSET_MASK;
                FragmentOffset <<= IP4_FRAGMENT_OFFSET_SHIFT;
                if (FragmentLength != BytesRemaining) {
                    FragmentOffset |= (IP4_FLAG_MORE_FRAGMENTS <<
                                       IP4_FRAGMENT_FLAGS_SHIFT);
                }

                Header->FragmentOffset = CPU_TO_NETWORK16(FragmentOffset);
                Header->TimeToLive = TimeToLive;

                ASSERT(Socket->KernelSocket.Protocol !=
                       SOCKET_INTERNET_PROTOCOL_RAW);

                Header->Protocol = Socket->KernelSocket.Protocol;
                Header->SourceAddress = LocalAddress->Address;
                Header->DestinationAddress = RemoteAddress->Address;
                Header->HeaderChecksum = 0;
                if ((Link->Properties.Capabilities &
                     NET_LINK_CAPABILITY_TRANSMIT_IP_CHECKSUM_OFFLOAD) == 0) {

                    Checksum = NetpIp4ChecksumData((PSHORT)Header,
                                                   sizeof(IP4_HEADER));

                    Header->HeaderChecksum = Checksum;

                } else {
                    Fragment->Flags |= NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD;
                }

                //
                // Add the fragment to the list of packets.
                //

                NET_INSERT_PACKET_BEFORE(Fragment, Packet, PacketList);
                PacketBuffer += FragmentLength;
                BytesCompleted += FragmentLength;
                BytesRemaining -= FragmentLength;
            }

            //
            // Remove the original packet. It just got fragmented. And move on
            // to the next packet ID.
            //

            Socket->SendPacketCount += 1;
            NET_REMOVE_PACKET_FROM_LIST(Packet, PacketList);
            NetFreeBuffer(Packet);
            continue;
        }

        //
        // Add the IP4 network header unless it is already included.
        //

        if ((Socket->Flags & NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED) == 0) {

            //
            // Get a pointer to the header, which is right before the data.
            //

            ASSERT(Packet->DataOffset > sizeof(IP4_HEADER));

            Packet->DataOffset -= sizeof(IP4_HEADER);
            Header = (PIP4_HEADER)(Packet->Buffer + Packet->DataOffset);

            //
            // Fill out that IPv4 header.
            //

            Header->VersionAndHeaderLength = IP4_VERSION |
                                             (UCHAR)(sizeof(IP4_HEADER) /
                                                     sizeof(ULONG));

            Header->Type = 0;
            Header->Type |= SocketInformation->DifferentiatedServicesCodePoint;
            TotalLength = Packet->FooterOffset - Packet->DataOffset;
            Header->TotalLength = CPU_TO_NETWORK16(TotalLength);
            Header->Identification = CPU_TO_NETWORK16(Socket->SendPacketCount);
            Socket->SendPacketCount += 1;
            Header->FragmentOffset = 0;
            Header->TimeToLive = TimeToLive;

            ASSERT(Socket->KernelSocket.Protocol !=
                   SOCKET_INTERNET_PROTOCOL_RAW);

            Header->Protocol = Socket->KernelSocket.Protocol;
            Header->SourceAddress = LocalAddress->Address;
            Header->DestinationAddress = RemoteAddress->Address;
            Header->HeaderChecksum = 0;
            if ((Link->Properties.Capabilities &
                 NET_LINK_CAPABILITY_TRANSMIT_IP_CHECKSUM_OFFLOAD) == 0) {

                Checksum = NetpIp4ChecksumData((PVOID)Header,
                                               sizeof(IP4_HEADER));

                Header->HeaderChecksum = Checksum;

            } else {
                Packet->Flags |= NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD;
            }

        //
        // Otherwise the packet may need to be shifted. Unless this is a raw
        // socket using the "raw" protocol, the packet was created thinking
        // that the IPv4 header needed to be included by the network layer. The
        // flags now indicate that the IPv4 header is included by the caller.
        // The packet needs to be properly aligned for the hardware, so it
        // needs to be shifted by the IPv4 header size.
        //

        } else {

            ASSERT(Socket->KernelSocket.Type == NetSocketRaw);

            //
            // This can be skipped if the socket is signed up to use the "raw"
            // protocol. The IPv4 header size isn't added to such sockets upon
            // initialization.
            //

            if (Socket->KernelSocket.Protocol != SOCKET_INTERNET_PROTOCOL_RAW) {

                ASSERT(Packet->DataOffset > sizeof(IP4_HEADER));

                Header = (PIP4_HEADER)(Packet->Buffer +
                                       Packet->DataOffset -
                                       sizeof(IP4_HEADER));

                TotalLength = Packet->DataSize - Packet->DataOffset;
                RtlCopyMemory(Header,
                              Packet->Buffer + Packet->DataOffset,
                              TotalLength);

                Packet->DataOffset -= sizeof(IP4_HEADER);
                Packet->FooterOffset -= sizeof(IP4_HEADER);
                Packet->DataSize -= sizeof(IP4_HEADER);
            }
        }
    }

    //
    // If this is a multicast address and the loopback bit is set, send the
    // packets back up the stack before sending them down. This needs to be
    // done first because the physical layer releases the packet structures
    // when it's finished with them.
    //

    if ((IP4_IS_MULTICAST_ADDRESS(RemoteAddress->Address) != FALSE) &&
        ((SocketInformation->Flags &
          IP4_SOCKET_FLAG_MULTICAST_LOOPBACK) != 0)) {

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
            NetpIp4ProcessReceivedData(&ReceiveContext);
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
        goto Ip4SendEnd;
    }

    Status = STATUS_SUCCESS;

Ip4SendEnd:
    if (NetIp4DebugPrintPackets != FALSE) {
        RtlDebugPrint("Net: IP4 Packet send from ");
        NetDebugPrintAddress(Source);
        RtlDebugPrint(" to ");
        NetDebugPrintAddress(Destination);
        RtlDebugPrint(" : %d.\n", Status);
    }

    return Status;
}

VOID
NetpIp4ProcessReceivedData (
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

    USHORT ComputedChecksum;
    IP4_ADDRESS DestinationAddress;
    USHORT FragmentFlags;
    USHORT FragmentOffset;
    PIP4_HEADER Header;
    ULONG HeaderSize;
    PNET_PACKET_BUFFER Packet;
    PNET_PROTOCOL_ENTRY ProtocolEntry;
    PNET_PACKET_BUFFER ReassembledPacket;
    IP4_ADDRESS SourceAddress;
    USHORT TotalLength;

    ReassembledPacket = NULL;
    Packet = ReceiveContext->Packet;
    Header = (PIP4_HEADER)(Packet->Buffer + Packet->DataOffset);

    //
    // Check the protocol version and header length.
    //

    if ((Header->VersionAndHeaderLength & IP4_VERSION_MASK) != IP4_VERSION) {
        RtlDebugPrint("Invalid IPv4 version. Byte: 0x%02x.\n",
                      Header->VersionAndHeaderLength);

        goto Ip4ProcessReceivedDataEnd;
    }

    HeaderSize = (Header->VersionAndHeaderLength & IP4_HEADER_LENGTH_MASK) *
                 sizeof(ULONG);

    if (HeaderSize < sizeof(IP4_HEADER)) {
        RtlDebugPrint("Invalid IPv4 header length. Byte: 0x%02x.\n",
                      Header->VersionAndHeaderLength);

        goto Ip4ProcessReceivedDataEnd;
    }

    //
    // Validate the total length field.
    //

    TotalLength = NETWORK_TO_CPU16(Header->TotalLength);
    if (TotalLength > (Packet->FooterOffset - Packet->DataOffset)) {
        RtlDebugPrint("Invalid IPv4 total length %d is bigger than packet "
                      "data, which is only %d bytes large.\n",
                      TotalLength,
                      (Packet->FooterOffset - Packet->DataOffset));

        goto Ip4ProcessReceivedDataEnd;
    }

    //
    // Validate the header checksum, which with the checksum field should work
    // out to zero. Skip this if the checksum was offloaded and valid.
    //

    if (((Packet->Flags & NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD) == 0) ||
        ((Packet->Flags & NET_PACKET_FLAG_IP_CHECKSUM_FAILED) != 0)) {

        ComputedChecksum = NetpIp4ChecksumData((PVOID)Header, HeaderSize);
        if (ComputedChecksum != 0) {
            RtlDebugPrint("Invalid IPv4 header checksum. Computed checksum: "
                          "0x%04x, should have been zero.\n",
                          ComputedChecksum);

            goto Ip4ProcessReceivedDataEnd;
        }
    }

    //
    // Initialize the network address.
    //

    RtlZeroMemory(&SourceAddress, sizeof(NETWORK_ADDRESS));
    RtlZeroMemory(&DestinationAddress, sizeof(NETWORK_ADDRESS));
    SourceAddress.Domain = NetDomainIp4;
    SourceAddress.Address = Header->SourceAddress;
    DestinationAddress.Domain = NetDomainIp4;
    DestinationAddress.Address = Header->DestinationAddress;

    //
    // Update the packet's size. Raw sockets should get everything at the IPv4
    // layer. So, lop any footers beyond the IPv4 packet. IPv4 has no footer
    // itself.
    //

    Packet->FooterOffset = Packet->DataOffset + TotalLength;

    //
    // If this is part of a fragmented datagram, add it to the mix with hopes
    // of completing the reassembly of the protocol layer packet.
    //

    FragmentOffset = NETWORK_TO_CPU16(Header->FragmentOffset);
    FragmentFlags = (FragmentOffset >> IP4_FRAGMENT_FLAGS_SHIFT) &
                    IP4_FRAGMENT_FLAGS_MASK;

    FragmentOffset = (FragmentOffset >> IP4_FRAGMENT_OFFSET_SHIFT) &
                     IP4_FRAGMENT_OFFSET_MASK;

    if (((FragmentFlags & IP4_FLAG_MORE_FRAGMENTS) != 0) ||
        (FragmentOffset != 0)) {

        if (NetIp4DebugPrintPackets != FALSE) {
            RtlDebugPrint("IP4: Fragment for protocol %d:\n%20s: ",
                          Header->Protocol,
                          "LocalAddress");

            NetDebugPrintAddress((PNETWORK_ADDRESS)&DestinationAddress);
            RtlDebugPrint("\n%20s: ", "RemoteAddress");
            NetDebugPrintAddress((PNETWORK_ADDRESS)&SourceAddress);
            RtlDebugPrint("\n%20s: 0x%x\n"
                          "%20s: 0x%x\n"
                          "%20s: 0x%x\n",
                          "ID",
                          NETWORK_TO_CPU16(Header->Identification),
                          "Offset",
                          FragmentOffset,
                          "Flags",
                          FragmentFlags);
        }

        //
        // If the "do not fragment" flag is also set, skip this fragment.
        //

        if ((FragmentFlags & IP4_FLAG_DO_NOT_FRAGMENT) != 0) {
            goto Ip4ProcessReceivedDataEnd;
        }

        ReassembledPacket = NetpIp4ProcessPacketFragment(ReceiveContext->Link,
                                                         Packet);

        if (ReassembledPacket == NULL) {
            goto Ip4ProcessReceivedDataEnd;
        }

        Packet = ReassembledPacket;

        //
        // Update the header information. There is no reason to validate it. It
        // just got created from a trusted source.
        //

        Header = (PIP4_HEADER)(Packet->Buffer + Packet->DataOffset);
        HeaderSize = (Header->VersionAndHeaderLength & IP4_HEADER_LENGTH_MASK) *
                     sizeof(ULONG);

        TotalLength = NETWORK_TO_CPU16(Header->TotalLength);

    //
    // Otherwise notify the debugger of a complete packet's arrival.
    //

    } else if (NetIp4DebugPrintPackets != FALSE) {
        RtlDebugPrint("Net: IP4 Packet received from ");
        NetDebugPrintAddress((PNETWORK_ADDRESS)&SourceAddress);
        RtlDebugPrint(" to ");
        NetDebugPrintAddress((PNETWORK_ADDRESS)&DestinationAddress);
        RtlDebugPrint("\n");
    }

    //
    // Add the source and destination addresses to the receive context.
    //

    ReceiveContext->Source = (PNETWORK_ADDRESS)&SourceAddress;
    ReceiveContext->Destination = (PNETWORK_ADDRESS)&DestinationAddress;
    ReceiveContext->ParentProtocolNumber = Header->Protocol;

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
    // Find the local protocol entry for the protocol specified in the header
    // and process the packet.
    //

    ProtocolEntry = NetGetProtocolEntry(Header->Protocol);
    if (ProtocolEntry == NULL) {
        RtlDebugPrint("No protocol found for IPv4 packet protocol number "
                      "0x%02x.\n",
                      Header->Protocol);

        goto Ip4ProcessReceivedDataEnd;
    }

    //
    // Update the packet's data offset so that it starts at the protocol layer.
    //

    Packet->DataOffset += HeaderSize;
    ReceiveContext->Protocol = ProtocolEntry;
    ProtocolEntry->Interface.ProcessReceivedData(ReceiveContext);

Ip4ProcessReceivedDataEnd:
    if (ReassembledPacket != NULL) {
        NetFreeBuffer(ReassembledPacket);
    }

    return;
}

ULONG
NetpIp4PrintAddress (
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

    UCHAR Components[4];
    PIP4_ADDRESS Ip4Address;
    ULONG Length;

    if (Address == NULL) {
        return IP4_MAX_ADDRESS_STRING;
    }

    ASSERT(Address->Domain == NetDomainIp4);

    Ip4Address = (PIP4_ADDRESS)Address;
    Components[0] = (UCHAR)(Ip4Address->Address);
    Components[1] = (UCHAR)(Ip4Address->Address >> 8);
    Components[2] = (UCHAR)(Ip4Address->Address >> 16);
    Components[3] = (UCHAR)(Ip4Address->Address >> 24);

    //
    // If the buffer is present, print that bad boy out.
    //

    if (Ip4Address->Port != 0) {
        Length = RtlPrintToString(Buffer,
                                  BufferLength,
                                  CharacterEncodingDefault,
                                  "%d.%d.%d.%d:%d",
                                  Components[0],
                                  Components[1],
                                  Components[2],
                                  Components[3],
                                  Ip4Address->Port);

    } else {
        Length = RtlPrintToString(Buffer,
                                  BufferLength,
                                  CharacterEncodingDefault,
                                  "%d.%d.%d.%d",
                                  Components[0],
                                  Components[1],
                                  Components[2],
                                  Components[3]);
    }

    return Length;
}

KSTATUS
NetpIp4GetSetInformation (
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

    ULONG BooleanOption;
    UCHAR ByteOption;
    ULONG Flags;
    ULONG IntegerOption;
    SOCKET_IP4_MULTICAST_REQUEST InterfaceRequest;
    PIP4_ADDRESS Ip4Address;
    SOCKET_IP4_OPTION Ip4Option;
    PNET_LINK_LOCAL_ADDRESS LinkInformation;
    NET_LINK_LOCAL_ADDRESS LinkResult;
    PNET_SOCKET_LINK_OVERRIDE MulticastInterface;
    PSOCKET_IP4_MULTICAST_REQUEST MulticastRequest;
    NET_SOCKET_LINK_OVERRIDE NewInterface;
    PNET_LINK OldInterfaceLink;
    PNET_PROTOCOL_ENTRY Protocol;
    UINTN RequiredSize;
    PIP4_SOCKET_INFORMATION SocketInformation;
    PVOID Source;
    KSTATUS Status;

    if (InformationType != SocketInformationIp4) {
        Status = STATUS_INVALID_PARAMETER;
        goto Ip4GetSetInformationEnd;
    }

    SocketInformation = Socket->NetworkSocketInformation;
    if (SocketInformation == NULL) {
        Status = STATUS_NOT_INITIALIZED;
        goto Ip4GetSetInformationEnd;
    }

    RequiredSize = 0;
    Source = NULL;
    Status = STATUS_SUCCESS;
    Protocol = Socket->Protocol;
    Ip4Option = (SOCKET_IP4_OPTION)Option;
    switch (Ip4Option) {
    case SocketIp4OptionHeaderIncluded:
        RequiredSize = sizeof(ULONG);
        if (Set != FALSE) {

            //
            // Setting the header included option is only allowed on raw
            // sockets that are not operating on the "raw" network protocol.
            //

            if ((Socket->KernelSocket.Type != NetSocketRaw) ||
                (Socket->KernelSocket.Protocol ==
                 SOCKET_INTERNET_PROTOCOL_RAW)) {

                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
                break;
            }

            if (*DataSize < RequiredSize) {
                *DataSize = RequiredSize;
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            BooleanOption = *((PULONG)Data);
            if (BooleanOption != FALSE) {
                RtlAtomicOr32(&(Socket->Flags),
                              NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED);

            } else {
                RtlAtomicAnd32(&(Socket->Flags),
                               ~NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED);
            }

        } else {
            Source = &BooleanOption;
            BooleanOption = FALSE;
            Flags = Socket->Flags;
            if ((Flags & NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED) != 0) {
                BooleanOption = TRUE;
            }
        }

        break;

    case SocketIp4OptionTimeToLive:
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

            SocketInformation->TimeToLive = (UCHAR)IntegerOption;

        } else {
            Source = &IntegerOption;
            IntegerOption = SocketInformation->TimeToLive;
        }

        break;

    case SocketIp4DifferentiatedServicesCodePoint:
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

            IntegerOption &= IP4_TYPE_DSCP_MASK;
            SocketInformation->DifferentiatedServicesCodePoint =
                                                          (UCHAR)IntegerOption;

        } else {
            Source = &IntegerOption;
            IntegerOption = SocketInformation->DifferentiatedServicesCodePoint;
        }

        break;

    case SocketIp4OptionJoinMulticastGroup:
    case SocketIp4OptionLeaveMulticastGroup:
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

        RequiredSize = sizeof(SOCKET_IP4_MULTICAST_REQUEST);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        MulticastRequest = (PSOCKET_IP4_MULTICAST_REQUEST)Data;
        if (!IP4_IS_MULTICAST_ADDRESS(MulticastRequest->Address)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (Ip4Option == SocketIp4OptionJoinMulticastGroup) {
            Status = NetpIp4JoinMulticastGroup(Socket, MulticastRequest);

        } else {
            Status = NetpIp4LeaveMulticastGroup(Socket, MulticastRequest);
        }

        goto Ip4GetSetInformationEnd;

    case SocketIp4OptionMulticastTimeToLive:
        RequiredSize = sizeof(UCHAR);
        if (Set != FALSE) {
            if (*DataSize < RequiredSize) {
                *DataSize = RequiredSize;
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            ByteOption = *((PUCHAR)Data);
            SocketInformation->MulticastTimeToLive = ByteOption;

        } else {
            Source = &ByteOption;
            ByteOption = SocketInformation->MulticastTimeToLive;
        }

        break;

    case SocketIp4OptionMulticastInterface:
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

        MulticastInterface = &(SocketInformation->MulticastInterface);
        if (Set != FALSE) {

            //
            // The size accounts for one IPv4 address, but not two. Interpret
            // it as the IPv4 address of the desired interface.
            //

            if (*DataSize < sizeof(SOCKET_IP4_MULTICAST_REQUEST)) {
                RtlZeroMemory(&InterfaceRequest, sizeof(InterfaceRequest));
                InterfaceRequest.Interface = *((PULONG)Data);

            //
            // Otherwise interpret it as a multicast request structure.
            //

            } else {

                ASSERT(*DataSize >= sizeof(SOCKET_IP4_MULTICAST_REQUEST));

                RequiredSize = sizeof(SOCKET_IP4_MULTICAST_REQUEST);
                RtlCopyMemory(&InterfaceRequest, Data, RequiredSize);
            }

            //
            // The any address resets the multicast interface to the default.
            //

            if (InterfaceRequest.Interface == 0) {
                RtlZeroMemory(&NewInterface, sizeof(NET_SOCKET_LINK_OVERRIDE));

            //
            // Otherwise the net link for the supplied interface is found.
            //

            } else {
                Status = NetpIp4FindLinkForMulticastRequest(Socket->Network,
                                                            &InterfaceRequest,
                                                            &LinkResult);

                if (!KSUCCESS(Status)) {
                    break;
                }

                NetInitializeSocketLinkOverride(Socket,
                                                &LinkResult,
                                                &NewInterface);
            }

            //
            // Acquire the multicast lock and smash in the new interface.
            //

            Status = NetpIp4AcquireMulticastLock(SocketInformation);
            if (!KSUCCESS(Status)) {
                if (NewInterface.LinkInformation.Link != NULL) {
                    NetLinkReleaseReference(NewInterface.LinkInformation.Link);
                }

                break;
            }

            OldInterfaceLink = MulticastInterface->LinkInformation.Link;
            RtlCopyMemory(MulticastInterface,
                          &NewInterface,
                          sizeof(NET_SOCKET_LINK_OVERRIDE));

            NetpIp4ReleaseMulticastLock(SocketInformation);
            if (OldInterfaceLink != NULL) {
                NetLinkReleaseReference(OldInterfaceLink);
            }

        //
        // A get request only ever returns the IPv4 address of the interface.
        // This must acquire the lock as the set call copies the address into
        // place byte by byte. Avoid a torn read.
        //

        } else {
            LinkInformation = &(MulticastInterface->LinkInformation);
            Ip4Address = (PIP4_ADDRESS)&(LinkInformation->SendAddress);
            Source = &IntegerOption;
            Status = NetpIp4AcquireMulticastLock(SocketInformation);
            if (!KSUCCESS(Status)) {
                break;
            }

            IntegerOption = Ip4Address->Address;
            NetpIp4ReleaseMulticastLock(SocketInformation);
        }

        break;

    case SocketIp4OptionMulticastLoopback:
        RequiredSize = sizeof(UCHAR);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        if (Set != FALSE) {
            ByteOption = *((PUCHAR)Data);
            if (ByteOption != FALSE) {
                RtlAtomicOr32(&(SocketInformation->Flags),
                              IP4_SOCKET_FLAG_MULTICAST_LOOPBACK);

            } else {
                RtlAtomicAnd32(&(SocketInformation->Flags),
                               ~IP4_SOCKET_FLAG_MULTICAST_LOOPBACK);
            }

        } else {
            Source = &ByteOption;
            ByteOption = FALSE;
            Flags = SocketInformation->Flags;
            if ((Flags & IP4_SOCKET_FLAG_MULTICAST_LOOPBACK) != 0) {
                ByteOption = TRUE;
            }
        }

        break;

    default:
        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto Ip4GetSetInformationEnd;
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
            goto Ip4GetSetInformationEnd;
        }
    }

Ip4GetSetInformationEnd:
    return Status;
}

KSTATUS
NetpIp4CopyInformation (
    PNET_SOCKET DestinationSocket,
    PNET_SOCKET SourceSocket
    )

/*++

Routine Description:

    This routine copies socket information properties from the source socket to
    the destination socket.

Arguments:

    DestinationSocket - Supplies a pointer to the socket whose information will
        be overwritten with the source socket's information.

    SourceSocket - Supplies a pointer to the socket whose information will
        be copied to the destination socket.

Return Value:

    Status code.

--*/

{

    PIP4_SOCKET_INFORMATION DestinationInformation;

    if ((DestinationSocket->NetworkSocketInformation == NULL) ||
        (SourceSocket->NetworkSocketInformation == NULL)) {

        return STATUS_NOT_INITIALIZED;
    }

    //
    // Copy all of the socket information. This routine is invoked when a
    // connection is accepted and the listening socket is forked.
    //

    RtlCopyMemory(DestinationSocket->NetworkSocketInformation,
                  SourceSocket->NetworkSocketInformation,
                  sizeof(IP4_SOCKET_INFORMATION));

    //
    // Reset the multicast information. The new socket should not inherit that
    // information.
    //

    DestinationInformation = DestinationSocket->NetworkSocketInformation;
    DestinationInformation->MulticastLock = NULL;
    RtlZeroMemory(&(DestinationInformation->MulticastInterface),
                  sizeof(NET_SOCKET_LINK_OVERRIDE));

    INITIALIZE_LIST_HEAD(&(DestinationInformation->MulticastGroupList));
    return STATUS_SUCCESS;
}

NET_ADDRESS_TYPE
NetpIp4GetAddressType (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine gets the type of the given address, categorizing it as unicast,
    broadcast, or multicast.

Arguments:

    Link - Supplies a pointer to the network link to which the address is bound.

    LinkAddressEntry - Supplies an optional pointer to a network link address
        entry to use while classifying the address.

    Address - Supplies a pointer to the network address to categorize.

Return Value:

    Returns the type of the specified address.

--*/

{

    PIP4_ADDRESS Ip4Address;
    PIP4_ADDRESS LocalAddress;
    volatile ULONG LocalIpAddress;
    PIP4_ADDRESS SubnetAddress;
    ULONG SubnetBroadcast;
    volatile ULONG SubnetMask;

    if (Address->Domain != NetDomainIp4) {
        return NetAddressUnknown;
    }

    Ip4Address = (PIP4_ADDRESS)Address;
    if (Ip4Address->Address == 0) {
        return NetAddressAny;
    }

    if (Ip4Address->Address == IP4_BROADCAST_ADDRESS) {
        return NetAddressBroadcast;
    }

    if (IP4_IS_MULTICAST_ADDRESS(Ip4Address->Address) != FALSE) {
        return NetAddressMulticast;
    }

    //
    // Check to see if this is the local IP address. This requires getting the
    // link address entry for the current domain (if not supplied). Normally
    // this requires acquiring a lock and searching over the link's list of
    // network address entries. That is costly on every DGRAM packet receive.
    // The network address entry list needs to be reconsidered anyway, so just
    // grab the first one off the list (as only IPv4 is present anyway).
    //
    // TODO: Replace link address list with an array for constant lookup.
    //

    if (LinkAddressEntry == NULL) {
        LinkAddressEntry = LIST_VALUE(Link->LinkAddressList.Next,
                                      NET_LINK_ADDRESS_ENTRY,
                                      ListEntry);

        ASSERT(LinkAddressEntry->Address.Domain == NetDomainIp4);
    }

    LocalAddress = (PIP4_ADDRESS)&(LinkAddressEntry->Address);
    LocalIpAddress = LocalAddress->Address;
    if (Ip4Address->Address == LocalIpAddress) {
        return NetAddressUnicast;
    }

    //
    // Check to see if the address is the local subnet's broadcast address.
    // Without the lock held, this address entry may change if the network goes
    // down. This would trigger all existing sockets to receive the
    // disconnected event. A broadcast address may not properly be determined,
    // so return it as unknown.
    //

    SubnetAddress = (PIP4_ADDRESS)&(LinkAddressEntry->Subnet);
    SubnetMask = SubnetAddress->Address;
    SubnetBroadcast = (LocalIpAddress & SubnetMask) | ~SubnetMask;
    if (Ip4Address->Address == SubnetBroadcast) {
        return NetAddressBroadcast;
    }

    return NetAddressUnknown;
}

//
// --------------------------------------------------------- Internal Functions
//

USHORT
NetpIp4ChecksumData (
    PVOID Data,
    ULONG Length
    )

/*++

Routine Description:

    This routine checksums a section of data for use in an IP datagram
    checksum and returns it in network byte order.

Arguments:

    Data - Supplies a pointer to the data to checksum.

    Length - Supplies the number of bytes to checksum. This must be an even
        number.

Return Value:

    Returns the checksum of the data.

--*/

{

    PUCHAR BytePointer;
    PULONG LongPointer;
    ULONG NextValue;
    USHORT ShortOne;
    PUSHORT ShortPointer;
    USHORT ShortTwo;
    ULONG Sum;

    ASSERT((Length & 0x1) == 0);

    Sum = 0;
    LongPointer = (PULONG)Data;
    while (Length >= sizeof(ULONG)) {
        NextValue = *LongPointer;
        LongPointer += 1;
        Sum += NextValue;
        if (Sum < NextValue) {
            Sum += 1;
        }

        Length -= sizeof(ULONG);
    }

    BytePointer = (PUCHAR)LongPointer;
    if (Length == sizeof(USHORT)) {
        ShortPointer = (PUSHORT)BytePointer;
        NextValue = (USHORT)*ShortPointer;
        Sum += NextValue;
        if (Sum < NextValue) {
            Sum += 1;
        }
    }

    //
    // Fold the 32-bit value down to 16-bits.
    //

    ShortOne = (USHORT)Sum;
    ShortTwo = (USHORT)(Sum >> 16);
    ShortTwo += ShortOne;
    if (ShortTwo < ShortOne) {
        ShortTwo += 1;
    }

    return (USHORT)~ShortTwo;
}

KSTATUS
NetpIp4TranslateNetworkAddress (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS NetworkAddress,
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine translates a network level address to a physical address.

Arguments:

    Socket - Supplies a pointer to the socket requesting the translation.

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
    ULONG BitsDifferentInSubnet;
    NETWORK_ADDRESS DefaultGateway;
    PIP4_ADDRESS Ip4Address;
    PIP4_ADDRESS LocalIpAddress;
    BOOL LockHeld;
    KSTATUS Status;
    ULONG SubnetBroadcast;
    PIP4_ADDRESS SubnetMask;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    AddressType = NetAddressUnknown;
    Ip4Address = (PIP4_ADDRESS)NetworkAddress;
    LockHeld = FALSE;

    //
    // This function is very simple: it perform some filtering on known
    // addresses, and if none of those match passes it on to the link layer.
    // Start by checking against 0.0.0.0, an invalid address.
    //

    if (Ip4Address->Address == 0) {
        return STATUS_INVALID_ADDRESS;
    }

    //
    // Check against the broadcast address, which automatically translates to
    // the broadcast link address.
    //

    Status = STATUS_SUCCESS;
    if (Ip4Address->Address == IP4_BROADCAST_ADDRESS) {
        AddressType = NetAddressBroadcast;
        goto Ip4TranslateNetworkAddressEnd;
    }

    if (IP4_IS_MULTICAST_ADDRESS(Ip4Address->Address) != FALSE) {
        AddressType = NetAddressMulticast;
        goto Ip4TranslateNetworkAddressEnd;
    }

    //
    // Make sure the link address is still configured when using it.
    //

    KeAcquireQueuedLock(Link->QueuedLock);
    LockHeld = TRUE;
    if (LinkAddress->Configured == FALSE) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto Ip4TranslateNetworkAddressEnd;
    }

    //
    // Check to see if the destination address is in the subnet. If it is,
    // then pass it down directly to get translated. Otherwise, pass down the
    // gateway address.
    //

    LocalIpAddress = (PIP4_ADDRESS)&(LinkAddress->Address);
    SubnetMask = (PIP4_ADDRESS)&(LinkAddress->Subnet);

    //
    // This calculates if any bits are different within the subnet mask. If
    // they are, then the destination is outside of the subnet and should go to
    // the default gateway.
    //

    BitsDifferentInSubnet = ((Ip4Address->Address ^ LocalIpAddress->Address) &
                            SubnetMask->Address);

    if (BitsDifferentInSubnet != 0) {
        RtlCopyMemory(&DefaultGateway,
                      &(LinkAddress->DefaultGateway),
                      sizeof(NETWORK_ADDRESS));

        NetworkAddress = &DefaultGateway;

    //
    // Check to see if the address is a subnet broadcast address.
    //

    } else {
        SubnetBroadcast = (LocalIpAddress->Address & SubnetMask->Address) |
                          ~SubnetMask->Address;

        if (Ip4Address->Address == SubnetBroadcast) {
            AddressType = NetAddressBroadcast;
            goto Ip4TranslateNetworkAddressEnd;
        }
    }

    KeReleaseQueuedLock(Link->QueuedLock);
    LockHeld = FALSE;

    //
    // Well, it looks like a run-of-the-mill IP address, so pass it on to get
    // translated.
    //

    Status = NetTranslateNetworkAddress(NetworkAddress,
                                        Link,
                                        LinkAddress,
                                        PhysicalAddress);

    AddressType = NetAddressUnicast;

Ip4TranslateNetworkAddressEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Link->QueuedLock);
    }

    //
    // Sending to a broadcast address must be specifically requested through
    // socket options.
    //

    if ((AddressType == NetAddressBroadcast) &&
        ((Socket->Flags & NET_SOCKET_FLAG_BROADCAST_ENABLED) == 0)) {

        return STATUS_ACCESS_DENIED;
    }

    //
    // Broadcast and multicast addresses need to be translated by the data link
    // layer.
    //

    if ((AddressType == NetAddressBroadcast) ||
        (AddressType == NetAddressMulticast)) {

        Status = Link->DataLinkEntry->Interface.ConvertToPhysicalAddress(
                                                               NetworkAddress,
                                                               PhysicalAddress,
                                                               AddressType);
    }

    return Status;
}

PNET_PACKET_BUFFER
NetpIp4ProcessPacketFragment (
    PNET_LINK Link,
    PNET_PACKET_BUFFER PacketFragment
    )

/*++

Routine Description:

    This routine processes a fragment of an IPv4 packet. The fragment will get
    added to the list of received fragments. If it is the missing piece and
    completes the original packet, then the reassembled packet will be returned.

Arguments:

    Link - Supplies a pointer to the owning network link.

    PacketFragment - Supplies a pointer to the IPv4 fragment. This packet
        includes the IPv4 header.

Return Value:

    Returns a pointer to a reassembled packet if the given fragment completed
    the packet. Returns NULL if the fragment was not enough to form a completed
    packet.

--*/

{

    ULONG AllocationSize;
    USHORT Checksum;
    PNET_PACKET_BUFFER CompletedPacket;
    PLIST_ENTRY CurrentEntry;
    PVOID DestinationBuffer;
    PRED_BLACK_TREE_NODE FoundNode;
    USHORT FragmentEnd;
    PIP4_FRAGMENT_ENTRY FragmentEntry;
    USHORT FragmentFlags;
    ULONG FragmentLength;
    USHORT FragmentOffset;
    PIP4_HEADER Header;
    ULONG HeaderSize;
    BOOL JoinNext;
    BOOL JoinPrevious;
    BOOL LastFragment;
    PIP4_FRAGMENT_ENTRY NewFragment;
    PIP4_HEADER NewHeader;
    PIP4_FRAGMENT_ENTRY NextEntry;
    PIP4_FRAGMENTED_PACKET_NODE PacketNode;
    PIP4_FRAGMENT_ENTRY PreviousEntry;
    IP4_FRAGMENTED_PACKET_NODE SearchNode;
    PVOID SourceBuffer;
    USHORT StartingOffset;
    KSTATUS Status;
    ULONG TotalLength;

    CompletedPacket = NULL;
    Header = (PIP4_HEADER)(PacketFragment->Buffer + PacketFragment->DataOffset);
    KeAcquireQueuedLock(NetIp4FragmentedPacketLock);

    //
    // Run through the tree and remove any entries that have expired.
    //

    NetpIp4RemoveFragmentedPackets(NULL);

    //
    // If there are too many packets, then exit.
    //

    if (NetIp4FragmentCount > IP4_MAX_FRAGMENT_COUNT) {
        goto Ip4ProcessPacketFragmentEnd;
    }

    //
    // Now that the tree has been purged of old entries, attempt to find an
    // existing entry for this fragment.
    //

    SearchNode.LocalAddress = Header->DestinationAddress;
    SearchNode.RemoteAddress = Header->SourceAddress;
    SearchNode.Protocol = Header->Protocol;
    SearchNode.Identification = NETWORK_TO_CPU16(Header->Identification);
    FoundNode = RtlRedBlackTreeSearch(&NetIp4FragmentedPacketTree,
                                      &(SearchNode.Node));

    //
    // If an entry is found the new fragment will be inserted in the existing
    // packt's list of fragments.
    //

    if (FoundNode != NULL) {
        PacketNode = RED_BLACK_TREE_VALUE(FoundNode,
                                          IP4_FRAGMENTED_PACKET_NODE,
                                          Node);

    //
    // Otherwise a new fragmented packet needs to be created before the
    // fragment can be inserted.
    //

    } else {
        PacketNode = NetpIp4CreateFragmentedPacketNode(Header);
        if (PacketNode == NULL) {
            goto Ip4ProcessPacketFragmentEnd;
        }
    }

    //
    // Determine the fragment's flags and offset.
    //

    FragmentOffset = NETWORK_TO_CPU16(Header->FragmentOffset);
    FragmentFlags = (FragmentOffset >> IP4_FRAGMENT_FLAGS_SHIFT) &
                    IP4_FRAGMENT_FLAGS_MASK;

    FragmentOffset = (FragmentOffset >> IP4_FRAGMENT_OFFSET_SHIFT) &
                     IP4_FRAGMENT_OFFSET_MASK;

    ASSERT(((FragmentFlags & IP4_FLAG_MORE_FRAGMENTS) != 0) ||
           ((FragmentOffset & IP4_FRAGMENT_OFFSET_MASK) != 0));

    //
    // Find this fragment's place in the list. It goes before the first entry
    // with a larger offset.
    //

    NextEntry = NULL;
    PreviousEntry = NULL;
    CurrentEntry = PacketNode->FragmentListHead.Next;
    while (CurrentEntry != &(PacketNode->FragmentListHead)) {
        FragmentEntry = LIST_VALUE(CurrentEntry, IP4_FRAGMENT_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (FragmentEntry->Offset <= FragmentOffset) {
            PreviousEntry = FragmentEntry;
            continue;
        }

        NextEntry = FragmentEntry;
        break;
    }

    JoinNext = FALSE;
    JoinPrevious = FALSE;
    LastFragment = FALSE;
    NewFragment = NULL;
    StartingOffset = FragmentOffset;
    HeaderSize = (Header->VersionAndHeaderLength & IP4_HEADER_LENGTH_MASK) *
                 sizeof(ULONG);

    //
    // Calculate the length of the fragment, not including the IPv4 header.
    // That doesn't need to be saved for a packet fragment. A valid header will
    // be rebuilt once all the fragments are received.
    //

    FragmentLength = PacketFragment->FooterOffset -
                     PacketFragment->DataOffset;

    ASSERT(FragmentLength == NETWORK_TO_CPU16(Header->TotalLength));

    FragmentLength -= HeaderSize;
    TotalLength = FragmentLength;

    //
    // If a previous fragment exists, combine it with the new fragment if they
    // are adjacent. Ignore overlapping fragments.
    //

    if (PreviousEntry != NULL) {

        ASSERT(IS_ALIGNED(PreviousEntry->Length, IP4_FRAGMENT_ALIGNMENT));

        FragmentEnd = PreviousEntry->Offset +
                      IP4_CONVERT_BYTES_TO_OFFSET(PreviousEntry->Length);

        if (FragmentEnd == FragmentOffset) {
            TotalLength += PreviousEntry->Length;

            ASSERT(PreviousEntry->LastFragment == FALSE);

            StartingOffset = PreviousEntry->Offset;
            JoinPrevious = TRUE;

        } else if (FragmentEnd > FragmentOffset) {
            if (NetIp4DebugPrintPackets != FALSE) {
                RtlDebugPrint("IP4: Ignoring overlapping fragment at offset "
                              "0x%x.\n",
                              FragmentOffset);
            }

            goto Ip4ProcessPacketFragmentEnd;
        }
    }

    //
    // If a next fragment exists, combine it with the new fragment if they are
    // adjacent. Ignore overlapping fragments.
    //

    if (NextEntry != NULL) {

        ASSERT(IS_ALIGNED(TotalLength, IP4_FRAGMENT_ALIGNMENT));

        FragmentEnd = StartingOffset + IP4_CONVERT_BYTES_TO_OFFSET(TotalLength);
        if (FragmentEnd == NextEntry->Offset) {
            TotalLength += NextEntry->Length;
            LastFragment = NextEntry->LastFragment;
            JoinNext = TRUE;

        } else if (FragmentEnd > NextEntry->Offset) {
            if (NetIp4DebugPrintPackets != FALSE) {
                RtlDebugPrint("IP4: Ignoring overlapping fragment at offset "
                              "0x%x.\n",
                              FragmentOffset);
            }

            goto Ip4ProcessPacketFragmentEnd;
        }
    }

    //
    // Record if the new fragment is the last fragment.
    //

    if ((FragmentFlags & IP4_FLAG_MORE_FRAGMENTS) == 0) {
        LastFragment = TRUE;
    }

    //
    // If the total length is now greater than the maximum packet size, exit.
    // Something suspicious is up with these fragments.
    //

    if (TotalLength > IP4_MAX_PACKET_SIZE) {
        NetpIp4DestroyFragmentedPacketNode(PacketNode);
        goto Ip4ProcessPacketFragmentEnd;
    }

    //
    // After the coalescing, if the last fragment is included and the starting
    // offset is zero, then the packet is complete.
    //

    FragmentEntry = NULL;
    if ((LastFragment != FALSE) && (StartingOffset == 0)) {
        Status = NetAllocateBuffer(sizeof(IP4_HEADER),
                                   TotalLength,
                                   0,
                                   Link,
                                   0,
                                   &CompletedPacket);

        if (!KSUCCESS(Status)) {

            ASSERT(CompletedPacket == NULL);

            goto Ip4ProcessPacketFragmentEnd;
        }

        DestinationBuffer = CompletedPacket->Buffer +
                            CompletedPacket->DataOffset;

    //
    // Otherwise allocate a new fragment to contain the new fragment and any
    // adjacent fragments.
    //

    } else {
        AllocationSize = sizeof(IP4_FRAGMENT_ENTRY) + TotalLength;
        NewFragment = MmAllocatePagedPool(AllocationSize, IP4_ALLOCATION_TAG);
        if (NewFragment == NULL) {
            goto Ip4ProcessPacketFragmentEnd;
        }

        NewFragment->LastFragment = LastFragment;
        NewFragment->Length = TotalLength;
        NewFragment->Offset = StartingOffset;
        DestinationBuffer = (PVOID)(NewFragment + 1);
        NetIp4FragmentCount += 1;
    }

    //
    // Copy the data into the destination buffer.
    //

    if (JoinPrevious != FALSE) {
        SourceBuffer = (PVOID)(PreviousEntry + 1);
        RtlCopyMemory(DestinationBuffer, SourceBuffer, PreviousEntry->Length);
        DestinationBuffer += PreviousEntry->Length;
    }

    SourceBuffer = PacketFragment->Buffer +
                   PacketFragment->DataOffset +
                   HeaderSize;

    RtlCopyMemory(DestinationBuffer, SourceBuffer, FragmentLength);
    DestinationBuffer += FragmentLength;
    if (JoinNext != FALSE) {
        SourceBuffer = (PVOID)(NextEntry + 1);
        RtlCopyMemory(DestinationBuffer, SourceBuffer, NextEntry->Length);
    }

    //
    // If the packet was completed, finish up by destroying the packet node and
    // adding an IP4 header.
    //

    if (CompletedPacket != NULL) {
        NetpIp4DestroyFragmentedPacketNode(PacketNode);

        //
        // Fill out the IPv4 header. Raw sockets get an accurate IPv4 header
        // once all the fragments arrive.
        //

        CompletedPacket->DataOffset -= sizeof(IP4_HEADER);
        NewHeader = CompletedPacket->Buffer + CompletedPacket->DataOffset;
        NewHeader->VersionAndHeaderLength = IP4_VERSION |
                                           (UCHAR)(sizeof(IP4_HEADER) /
                                                   sizeof(ULONG));

        NewHeader->Type = 0;
        TotalLength = CompletedPacket->FooterOffset -
                      CompletedPacket->DataOffset;

        NewHeader->TotalLength = CPU_TO_NETWORK16(TotalLength);
        NewHeader->Identification = Header->Identification;
        NewHeader->FragmentOffset = 0;
        NewHeader->TimeToLive = Header->TimeToLive;
        NewHeader->Protocol = Header->Protocol;
        NewHeader->SourceAddress = Header->SourceAddress;
        NewHeader->DestinationAddress = Header->DestinationAddress;
        NewHeader->HeaderChecksum = 0;
        Checksum = NetpIp4ChecksumData((PSHORT)Header, sizeof(IP4_HEADER));
        NewHeader->HeaderChecksum = Checksum;

    //
    // Otherwise insert the new fragment into the appropriate position.
    //

    } else {

        ASSERT(NewFragment != NULL);

        //
        // If a previous entry exists then the entry gets inserted after that
        // entry.
        //

        if (PreviousEntry != NULL) {
            INSERT_AFTER(&(NewFragment->ListEntry),
                         &(PreviousEntry->ListEntry));

        //
        // Otherwise the new fragment is first in the list.
        //

        } else {

            ASSERT(((NextEntry != NULL) &&
                    (NextEntry->ListEntry.Previous ==
                     &(PacketNode->FragmentListHead))) ||
                   (LIST_EMPTY(&(PacketNode->FragmentListHead)) != FALSE));

            INSERT_AFTER(&(NewFragment->ListEntry),
                         &(PacketNode->FragmentListHead));
        }

        //
        // Remove any entries that were coalesced.
        //

        if (JoinPrevious != FALSE) {
            LIST_REMOVE(&(PreviousEntry->ListEntry));
            MmFreePagedPool(PreviousEntry);
            NetIp4FragmentCount -= 1;
        }

        if (JoinNext != FALSE) {
            LIST_REMOVE(&(NextEntry->ListEntry));
            MmFreePagedPool(NextEntry);
            NetIp4FragmentCount -= 1;
        }
    }

Ip4ProcessPacketFragmentEnd:
    KeReleaseQueuedLock(NetIp4FragmentedPacketLock);
    return CompletedPacket;
}

COMPARISON_RESULT
NetpIp4CompareFragmentedPacketEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes, in this case two IPv4
    fragmented packet nodes.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PIP4_FRAGMENTED_PACKET_NODE FirstPacket;
    PIP4_FRAGMENTED_PACKET_NODE SecondPacket;

    FirstPacket = RED_BLACK_TREE_VALUE(FirstNode,
                                       IP4_FRAGMENTED_PACKET_NODE,
                                       Node);

    SecondPacket = RED_BLACK_TREE_VALUE(SecondNode,
                                        IP4_FRAGMENTED_PACKET_NODE,
                                        Node);

    if (FirstPacket->Protocol != SecondPacket->Protocol) {
        if (FirstPacket->Protocol < SecondPacket->Protocol) {
            return ComparisonResultAscending;

        } else {
            return ComparisonResultDescending;
        }
    }

    if (FirstPacket->RemoteAddress != SecondPacket->RemoteAddress) {
        if (FirstPacket->RemoteAddress < SecondPacket->RemoteAddress) {
            return ComparisonResultAscending;

        } else {
            return ComparisonResultDescending;
        }
    }

    if (FirstPacket->LocalAddress != SecondPacket->LocalAddress) {
        if (FirstPacket->LocalAddress < SecondPacket->LocalAddress) {
            return ComparisonResultAscending;

        } else {
            return ComparisonResultDescending;
        }
    }

    if (FirstPacket->Identification != SecondPacket->Identification) {
        if (FirstPacket->Identification < SecondPacket->Identification) {
            return ComparisonResultAscending;

        } else {
            return ComparisonResultDescending;
        }
    }

    return ComparisonResultSame;
}

VOID
NetpIp4RemoveFragmentedPackets (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine removes fragmented packets from the tree of fragmented
    packets. If a socket is supplied, then it removes all of the packets for
    that socket. Otherwise it removes all of the expired packets, for all
    sockets.

Arguments:

    Socket - Supplies an optional pointer to a network socket.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentTime;
    PIP4_ADDRESS LocalAddress;
    PIP4_FRAGMENTED_PACKET_NODE PacketNode;
    PIP4_ADDRESS RemoteAddress;
    PRED_BLACK_TREE_NODE TreeNode;

    ASSERT(KeIsQueuedLockHeld(NetIp4FragmentedPacketLock) != FALSE);

    //
    // If a socket was supplied, remove all pending fragments for the local
    // address, remote address, and protocol tuple.
    //

    if (Socket != NULL) {

        ASSERT(Socket->LocalReceiveAddress.Domain == NetDomainIp4);
        ASSERT((Socket->RemoteAddress.Domain == NetDomainIp4) ||
               (Socket->RemoteAddress.Domain == NetDomainInvalid));

        LocalAddress = (PIP4_ADDRESS)&(Socket->LocalReceiveAddress);
        RemoteAddress = (PIP4_ADDRESS)&(Socket->RemoteAddress);

    //
    // Otherwise collect the current time as only the expired packets will be
    // removed.
    //

    } else {
        CurrentTime = HlQueryTimeCounter();
    }

    //
    // Iterate over the tree and remove the appropriate packets.
    //

    TreeNode = RtlRedBlackTreeGetNextNode(&NetIp4FragmentedPacketTree,
                                          FALSE,
                                          NULL);

    while (TreeNode != NULL) {
        PacketNode = RED_BLACK_TREE_VALUE(TreeNode,
                                          IP4_FRAGMENTED_PACKET_NODE,
                                          Node);

        TreeNode = RtlRedBlackTreeGetNextNode(&NetIp4FragmentedPacketTree,
                                              FALSE,
                                              TreeNode);

        if (Socket != NULL) {
            if ((PacketNode->LocalAddress != LocalAddress->Address) ||
                (PacketNode->RemoteAddress != RemoteAddress->Address) ||
                (PacketNode->Protocol !=
                 Socket->Protocol->ParentProtocolNumber)) {

                continue;
            }

        } else {
            if (PacketNode->Timeout > CurrentTime) {
                continue;
            }
        }

        //
        // This packet needs to be destroyed.
        //

        NetpIp4DestroyFragmentedPacketNode(PacketNode);
    }

    return;
}

PIP4_FRAGMENTED_PACKET_NODE
NetpIp4CreateFragmentedPacketNode (
    PIP4_HEADER Header
    )

/*++

Routine Description:

    This routine allocates a new fragmented packet node and inserts it into the
    tree.

Arguments:

    Header - Supplies a pointer to the header of the fragmented IPv4 packet.

Return Value:

    Returns a pointer to the newly created fragmented packet node on success or
    NULL on failure.

--*/

{

    PIP4_FRAGMENTED_PACKET_NODE NewNode;
    KSTATUS Status;
    ULONGLONG Timeout;

    //
    // Allocate the new fragmented packet node and insert it into the tree.
    //

    NewNode = MmAllocatePagedPool(sizeof(IP4_FRAGMENTED_PACKET_NODE),
                                  IP4_ALLOCATION_TAG);

    if (NewNode == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Ip4CreateFragmentedPacketNodeEnd;
    }

    RtlZeroMemory(NewNode, sizeof(IP4_FRAGMENTED_PACKET_NODE));
    INITIALIZE_LIST_HEAD(&(NewNode->FragmentListHead));
    NewNode->LocalAddress = Header->DestinationAddress;
    NewNode->RemoteAddress = Header->SourceAddress;
    NewNode->Protocol = Header->Protocol;
    NewNode->Identification = NETWORK_TO_CPU16(Header->Identification);
    Timeout = HlQueryTimeCounter() +
              KeConvertMicrosecondsToTimeTicks(IP4_FRAGMENT_TIMEOUT);

    NewNode->Timeout = Timeout;
    RtlRedBlackTreeInsert(&NetIp4FragmentedPacketTree, &(NewNode->Node));
    Status = STATUS_SUCCESS;

Ip4CreateFragmentedPacketNodeEnd:
    if (!KSUCCESS(Status)) {
        if (NewNode != NULL) {
            NetpIp4DestroyFragmentedPacketNode(NewNode);
            NewNode = NULL;
        }
    }

    return NewNode;
}

VOID
NetpIp4DestroyFragmentedPacketNode (
    PIP4_FRAGMENTED_PACKET_NODE PacketNode
    )

/*++

Routine Description:

    This routine destroys the given fragmented packet node, removing it from
    the tree and destroying any fragments associated with it.

Arguments:

    PacketNode - Supplies a pointer to the fragmented packet node to destroy.

Return Value:

    None.

--*/

{

    PIP4_FRAGMENT_ENTRY FragmentEntry;

    if (PacketNode->Node.Parent != NULL) {
        RtlRedBlackTreeRemove(&NetIp4FragmentedPacketTree, &(PacketNode->Node));
    }

    while (LIST_EMPTY(&(PacketNode->FragmentListHead)) == FALSE) {
        FragmentEntry = LIST_VALUE(PacketNode->FragmentListHead.Next,
                                   IP4_FRAGMENT_ENTRY,
                                   ListEntry);

        LIST_REMOVE(&(FragmentEntry->ListEntry));
        MmFreePagedPool(FragmentEntry);
        NetIp4FragmentCount -= 1;
    }

    MmFreePagedPool(PacketNode);
    return;
}

KSTATUS
NetpIp4JoinMulticastGroup (
    PNET_SOCKET Socket,
    PSOCKET_IP4_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine adds the given socket to a multicast group.

Arguments:

    Socket - Supplies a pointer to a socket.

    Request - Supplies a pointer to the multicast join request. This stores
        the multicast group address to join and the address of the interface on
        which to join it.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PIP4_MULTICAST_GROUP Group;
    SOCKET_IGMP_MULTICAST_REQUEST IgmpRequest;
    NET_LINK_LOCAL_ADDRESS LinkResult;
    BOOL LockHeld;
    PIP4_MULTICAST_GROUP NewGroup;
    PNET_PROTOCOL_ENTRY Protocol;
    UINTN RequestSize;
    PIP4_SOCKET_INFORMATION SocketInformation;
    KSTATUS Status;

    LinkResult.Link = NULL;
    LockHeld = FALSE;
    NewGroup = NULL;
    SocketInformation = Socket->NetworkSocketInformation;

    //
    // This isn't going to get very far without IGMP. Fail immediately if it's
    // not present.
    //

    Protocol = NetGetProtocolEntry(SOCKET_INTERNET_PROTOCOL_IGMP);
    if (Protocol == NULL) {
        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        goto JoinMulticastGroupEnd;
    }

    //
    // Attempt to find a network link that can reach the multicast address, or
    // find the one specified by the request.
    //

    Status = NetpIp4FindLinkForMulticastRequest(Socket->Network,
                                                Request,
                                                &LinkResult);

    if (!KSUCCESS(Status)) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto JoinMulticastGroupEnd;
    }

    Status = NetpIp4AcquireMulticastLock(SocketInformation);
    if (!KSUCCESS(Status)) {
        goto JoinMulticastGroupEnd;
    }

    LockHeld = TRUE;

    //
    // Check to see if this socket already joined the group.
    //

    CurrentEntry = SocketInformation->MulticastGroupList.Next;
    while (CurrentEntry != &(SocketInformation->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, IP4_MULTICAST_GROUP, ListEntry);
        if ((Group->MulticastAddress == Request->Address) &&
            (Group->Link == LinkResult.Link) &&
            (Group->LinkAddress == LinkResult.LinkAddress)) {

            Status = STATUS_ADDRESS_IN_USE;
            goto JoinMulticastGroupEnd;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Prepare for success and allocate a new IPv4 multicast group.
    //

    NewGroup = MmAllocatePagedPool(sizeof(IP4_MULTICAST_GROUP),
                                   IP4_ALLOCATION_TAG);

    if (NewGroup == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto JoinMulticastGroupEnd;
    }

    //
    // Ask IGMP to join the multicast group. A multithreaded request for this
    // socket to join or leave a muliticast group is highly unlikely, so don't
    // fret too much about holding the lock.
    //

    IgmpRequest.Link = LinkResult.Link;
    IgmpRequest.LinkAddress = LinkResult.LinkAddress;
    IgmpRequest.MulticastAddress = Request->Address;
    RequestSize = sizeof(SOCKET_IGMP_MULTICAST_REQUEST);
    Status = Protocol->Interface.GetSetInformation(
                                            Socket,
                                            SocketInformationIgmp,
                                            SocketIgmpOptionJoinMulticastGroup,
                                            &IgmpRequest,
                                            &RequestSize,
                                            TRUE);

    if (!KSUCCESS(Status)) {
        goto JoinMulticastGroupEnd;
    }

    NewGroup->MulticastAddress = Request->Address;
    NewGroup->Link = LinkResult.Link;
    NewGroup->LinkAddress = LinkResult.LinkAddress;
    INSERT_BEFORE(&(NewGroup->ListEntry),
                  &(SocketInformation->MulticastGroupList));

    //
    // Make sure to take the link's reference from the link result.
    //

    LinkResult.Link = NULL;

JoinMulticastGroupEnd:
    if (LockHeld != FALSE) {
        NetpIp4ReleaseMulticastLock(SocketInformation);
    }

    if (LinkResult.Link != NULL) {
        NetLinkReleaseReference(LinkResult.Link);
    }

    if (!KSUCCESS(Status)) {
        if (NewGroup != NULL) {
            MmFreePagedPool(NewGroup);
        }
    }

    return Status;
}

KSTATUS
NetpIp4LeaveMulticastGroup (
    PNET_SOCKET Socket,
    PSOCKET_IP4_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine removes the given socket from a multicast group.

Arguments:

    Socket - Supplies a pointer to a socket.

    Request - Supplies a pointer to the multicast leave request. This stores
        the multicast group address to leave and the address of the interface
        on which the socket joined the group.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PIP4_MULTICAST_GROUP DestroyGroup;
    PIP4_MULTICAST_GROUP Group;
    SOCKET_IGMP_MULTICAST_REQUEST IgmpRequest;
    NET_LINK_LOCAL_ADDRESS LinkResult;
    BOOL LockHeld;
    PNET_PROTOCOL_ENTRY Protocol;
    UINTN RequestSize;
    PIP4_SOCKET_INFORMATION SocketInformation;
    KSTATUS Status;

    DestroyGroup = NULL;
    LinkResult.Link = NULL;
    LockHeld = FALSE;

    //
    // If the multicast lock is not allocated or the list is empty, then this
    // socket never joined any multicast groups.
    //

    SocketInformation = Socket->NetworkSocketInformation;
    if (LIST_EMPTY(&(SocketInformation->MulticastGroupList)) != FALSE) {
        Status = STATUS_INVALID_ADDRESS;
        goto LeaveMulticastGroupEnd;
    }

    ASSERT(SocketInformation->MulticastLock != NULL);

    //
    // Attempt to find a network link that can reach the multicast address, or
    // find the one specified by the request.
    //

    Status = NetpIp4FindLinkForMulticastRequest(Socket->Network,
                                                Request,
                                                &LinkResult);

    if (!KSUCCESS(Status)) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto LeaveMulticastGroupEnd;
    }

    //
    // Search through the multicast groups for a matching entry.
    //

    Status = NetpIp4AcquireMulticastLock(SocketInformation);
    if (!KSUCCESS(Status)) {
        goto LeaveMulticastGroupEnd;
    }

    LockHeld = TRUE;
    Status = STATUS_INVALID_ADDRESS;
    CurrentEntry = SocketInformation->MulticastGroupList.Next;
    while (CurrentEntry != &(SocketInformation->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, IP4_MULTICAST_GROUP, ListEntry);
        if ((Group->MulticastAddress == Request->Address) &&
            (Group->Link == LinkResult.Link) &&
            (Group->LinkAddress == LinkResult.LinkAddress)) {

            Status = STATUS_SUCCESS;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (!KSUCCESS(Status)) {
        goto LeaveMulticastGroupEnd;
    }

    //
    // Remove the group from the list and mark the group for destruction.
    //

    LIST_REMOVE(&(Group->ListEntry));
    NetpIp4ReleaseMulticastLock(SocketInformation);
    LockHeld = FALSE;
    DestroyGroup = Group;

    //
    // Now notify IGMP that this socket has left the group. IGMP should not be
    // failing at this point.
    //

    Protocol = NetGetProtocolEntry(SOCKET_INTERNET_PROTOCOL_IGMP);
    if (Protocol == NULL) {
        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        goto LeaveMulticastGroupEnd;
    }

    IgmpRequest.Link = Group->Link;
    IgmpRequest.LinkAddress = Group->LinkAddress;
    IgmpRequest.MulticastAddress = Group->MulticastAddress;
    RequestSize = sizeof(SOCKET_IGMP_MULTICAST_REQUEST);
    Status = Protocol->Interface.GetSetInformation(
                                           Socket,
                                           SocketInformationIgmp,
                                           SocketIgmpOptionLeaveMulticastGroup,
                                           &IgmpRequest,
                                           &RequestSize,
                                           TRUE);

    if (!KSUCCESS(Status)) {
        goto LeaveMulticastGroupEnd;
    }

LeaveMulticastGroupEnd:
    if (LockHeld != FALSE) {
        NetpIp4ReleaseMulticastLock(SocketInformation);
    }

    if (LinkResult.Link != NULL) {
        NetLinkReleaseReference(LinkResult.Link);
    }

    if (DestroyGroup != NULL) {
        NetLinkReleaseReference(Group->Link);
        MmFreePagedPool(Group);
    }

    return Status;
}

VOID
NetpIp4DestroyMulticastGroups (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys all multicast groups that the given socket joined. It
    notifies IGMP that the socket is leaving the group and then destroys the
    group structure.

Arguments:

    Socket - Supplies a pointer to the socket meant to destroy its multicast
        groups.

Return Value:

    None.

--*/

{

    PIP4_MULTICAST_GROUP Group;
    PLIST_ENTRY MulticastGroupList;
    PNET_PROTOCOL_ENTRY Protocol;
    SOCKET_IGMP_MULTICAST_REQUEST Request;
    UINTN RequestSize;
    PIP4_SOCKET_INFORMATION SocketInformation;

    ASSERT(Socket->NetworkSocketInformation != NULL);

    SocketInformation = Socket->NetworkSocketInformation;
    MulticastGroupList = &(SocketInformation->MulticastGroupList);
    if (LIST_EMPTY(MulticastGroupList) != FALSE) {
        goto DestroyMulticastGroupsEnd;
    }

    ASSERT(SocketInformation->MulticastLock != NULL);

    Protocol = NetGetProtocolEntry(SOCKET_INTERNET_PROTOCOL_IGMP);
    if (Protocol == NULL) {
        goto DestroyMulticastGroupsEnd;
    }

    //
    // Run through the local list, leave each multicast group and destroy the
    // group structures.
    //

    while (LIST_EMPTY(MulticastGroupList) == FALSE) {
        Group = LIST_VALUE(MulticastGroupList->Next,
                           IP4_MULTICAST_GROUP,
                           ListEntry);

        LIST_REMOVE(&(Group->ListEntry));
        Request.Link = Group->Link;
        Request.LinkAddress = Group->LinkAddress;
        Request.MulticastAddress = Group->MulticastAddress;
        RequestSize = sizeof(SOCKET_IGMP_MULTICAST_REQUEST);
        Protocol->Interface.GetSetInformation(
                                           Socket,
                                           SocketInformationIgmp,
                                           SocketIgmpOptionLeaveMulticastGroup,
                                           &Request,
                                           &RequestSize,
                                           TRUE);

        NetLinkReleaseReference(Group->Link);
        MmFreePagedPool(Group);
    }

DestroyMulticastGroupsEnd:
    if (SocketInformation->MulticastLock != NULL) {
        KeDestroyQueuedLock(SocketInformation->MulticastLock);
    }

    return;
}

KSTATUS
NetpIp4FindLinkForMulticastRequest (
    PNET_NETWORK_ENTRY Network,
    PSOCKET_IP4_MULTICAST_REQUEST Request,
    PNET_LINK_LOCAL_ADDRESS LinkResult
    )

/*++

Routine Description:

    This routine searches for a network link that matches the given multicast
    request. If the any address is supplied, then the multicast address will be
    used to find a link that can reach it. A reference is taken on the returned
    network link. The caller is responsible for releasing the reference.

Arguments:

    Network - Supplies a pointer to the network to which the address belongs.
        This should be a pointer to the IPv4 network entry.

    Request - Supplies the multicast request for which a link needs to be
        found.

    LinkResult - Supplies a pointer that receives the link information,
        including the link, link address entry, and associated local address.

Return Value:

    Status code.

--*/

{

    IP4_ADDRESS LocalAddress;
    IP4_ADDRESS RemoteAddress;
    KSTATUS Status;

    //
    // If the any address is supplied, find a link that can reach the multicast
    // address.
    //

    if (Request->Interface == 0) {
        RtlZeroMemory(&RemoteAddress, sizeof(IP4_ADDRESS));
        RemoteAddress.Domain = NetDomainIp4;
        RemoteAddress.Address = Request->Address;
        Status = NetFindLinkForRemoteAddress((PNETWORK_ADDRESS)&RemoteAddress,
                                             LinkResult);

        if (KSUCCESS(Status)) {
            goto FindLinkForMulticastRequest;
        }
    }

    //
    // Otherwise a link that matches the given IPv4 address must be found.
    //

    RtlZeroMemory(&LocalAddress, sizeof(IP4_ADDRESS));
    LocalAddress.Domain = NetDomainIp4;
    LocalAddress.Address = Request->Interface;
    Status = NetFindLinkForLocalAddress(Network,
                                        (PNETWORK_ADDRESS)&LocalAddress,
                                        NULL,
                                        LinkResult);

    if (!KSUCCESS(Status)) {
        goto FindLinkForMulticastRequest;
    }

FindLinkForMulticastRequest:
    return Status;
}

KSTATUS
NetpIp4AcquireMulticastLock (
    PIP4_SOCKET_INFORMATION SocketInformation
    )

/*++

Routine Description:

    This routine acquires the given socket information's multicast lock,
    allocating it on the fly if it does not already exist. This is done so most
    sockets don't have to allocate the lock, as most sockets don't perform
    multicast actions.

Arguments:

    SocketInformation - Supplies a pointer to the IPv4 socket information whose
        multicast lock is to be acquired.

Return Value:

    Status code.

--*/

{

    PQUEUED_LOCK NewLock;
    PQUEUED_LOCK OldLock;
    KSTATUS Status;

    //
    // If there is no multicast lock. Create one before going any further. This
    // is done on the fly so that most sockets don't need to allocate the lock
    // resource.
    //

    if (SocketInformation->MulticastLock == NULL) {
        NewLock = KeCreateQueuedLock();
        if (NewLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AcquireMulticastLockEnd;
        }

        //
        // Try to exchange the lock into place.
        //

        OldLock = (PQUEUED_LOCK)RtlAtomicCompareExchange(
                                   (PUINTN)&(SocketInformation->MulticastLock),
                                   (UINTN)NewLock,
                                   (UINTN)NULL);

        if (OldLock != NULL) {
            KeDestroyQueuedLock(NewLock);
        }
    }

    ASSERT(SocketInformation->MulticastLock != NULL);

    KeAcquireQueuedLock(SocketInformation->MulticastLock);
    Status = STATUS_SUCCESS;

AcquireMulticastLockEnd:
    return Status;
}

VOID
NetpIp4ReleaseMulticastLock (
    PIP4_SOCKET_INFORMATION SocketInformation
    )

/*++

Routine Description:

    This routine releases the multicast lock for the given socket information.

Arguments:

    SocketInformation - Supplies a pointer to an IPv4 sockets information.

Return Value:

    None.

--*/

{

    ASSERT(SocketInformation->MulticastLock != NULL);

    KeReleaseQueuedLock(SocketInformation->MulticastLock);
    return;
}

