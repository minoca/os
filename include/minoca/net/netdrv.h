/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    netdrv.h

Abstract:

    This header contains definitions necessary for implementing network
    drivers.

Author:

    Evan Green 4-Apr-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/devinfo/net.h>

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros to convert from CPU to network format and back.
//

#define CPU_TO_NETWORK64(_Input) RtlByteSwapUlonglong(_Input)
#define NETWORK_TO_CPU64(_Input) RtlByteSwapUlonglong(_Input)
#define CPU_TO_NETWORK32(_Input) RtlByteSwapUlong(_Input)
#define NETWORK_TO_CPU32(_Input) RtlByteSwapUlong(_Input)
#define CPU_TO_NETWORK16(_Input) RtlByteSwapUshort(_Input)
#define NETWORK_TO_CPU16(_Input) RtlByteSwapUshort(_Input)

//
// This macro gets a network sockets last error.
//

#define NET_SOCKET_GET_LAST_ERROR(_Socket) (_Socket)->LastError

//
// This macro gets and clears a network sockets last error.
//

#define NET_SOCKET_GET_AND_CLEAR_LAST_ERROR(_Socket)               \
    RtlAtomicExchange32((volatile ULONG *)&((_Socket)->LastError), \
                        STATUS_SUCCESS);

//
// This macro sets the network sockets last error state.
//

#define NET_SOCKET_SET_LAST_ERROR(_Socket, _Error) \
    RtlAtomicExchange32((volatile ULONG *)&((_Socket)->LastError), (_Error));

//
// This macro clears the network sockets last error state.
//

#define NET_SOCKET_CLEAR_LAST_ERROR(_Socket) \
    NET_SOCKET_GET_AND_CLEAR_LAST_ERROR(_Socket)

//
// This macro initializes a network packet list.
//

#define NET_INITIALIZE_PACKET_LIST(_PacketList)   \
    INITIALIZE_LIST_HEAD(&((_PacketList)->Head)); \
    (_PacketList)->Count = 0;

//
// This macro adds a network packet to a network packet list.
//

#define NET_ADD_PACKET_TO_LIST(_Packet, _PacketList)                \
    INSERT_BEFORE(&((_Packet)->ListEntry), &((_PacketList)->Head)); \
    (_PacketList)->Count += 1;

//
// This macro adds a network packet to beginning of a network packet list.
//

#define NET_ADD_PACKET_TO_LIST_HEAD(_Packet, _PacketList)          \
    INSERT_AFTER(&((_Packet)->ListEntry), &((_PacketList)->Head)); \
    (_PacketList)->Count += 1;

//
// This macro removes a network packet from a network packet list.
//

#define NET_REMOVE_PACKET_FROM_LIST(_Packet, _PacketList) \
    LIST_REMOVE(&(_Packet)->ListEntry);                   \
    (_PacketList)->Count -= 1;

//
// This macro inserts a new packet before an existing packet.
//

#define NET_INSERT_PACKET_BEFORE(_New, _Existing, _PacketList)      \
    INSERT_BEFORE(&((_New)->ListEntry), &((_Existing)->ListEntry)); \
    (_PacketList)->Count += 1;

//
// This macro inserts a new packet after an existing packet.
//

#define NET_INSERT_PACKET_AFTER(_New, _Existing, _PacketList)      \
    INSERT_AFTER(&((_New)->ListEntry), &((_Existing)->ListEntry)); \
    (_PacketList)->Count += 1;

//
// This macro determines if the packet list is empty.
//

#define NET_PACKET_LIST_EMPTY(_PacketList) ((_PacketList)->Count == 0)

//
// This macro appends a list of network packets to another list of network
// packets, leaving the original appended list empty.
//

#define NET_APPEND_PACKET_LIST(_AppendList, _ExistingList)         \
    APPEND_LIST(&((_AppendList)->Head), &((_ExistingList)->Head)); \
    (_ExistingList)->Count += (_AppendList)->Count;                \
    NET_INITIALIZE_PACKET_LIST(_AppendList);

//
// ---------------------------------------------------------------- Definitions
//

#ifndef NET_API

#define NET_API __DLLIMPORT

#endif

//
// Define the current version number of the net link properties structure.
//

#define NET_LINK_PROPERTIES_VERSION 1

//
// Define some common network link speeds.
//

#define NET_SPEED_NONE 0
#define NET_SPEED_10_MBPS 10000000ULL
#define NET_SPEED_100_MBPS 100000000ULL
#define NET_SPEED_1000_MBPS 1000000000ULL
#define NET_SPEED_2500_MBPS 2500000000ULL

//
// Define well-known protocol numbers.
//

#define IP4_PROTOCOL_NUMBER      0x0800
#define IP6_PROTOCOL_NUMBER      0x86DD
#define ARP_PROTOCOL_NUMBER      0x0806
#define EAPOL_PROTOCOL_NUMBER    0x888E

//
// Define an "invalid" protocol number for networks that don't actually expect
// to receive packets from the physical layer (e.g. Netlink).
//

#define INVALID_PROTOCOL_NUMBER (ULONG)-1

//
// Define the network socket flags.
//

#define NET_SOCKET_FLAG_REUSE_ANY_ADDRESS       0x00000001
#define NET_SOCKET_FLAG_REUSE_TIME_WAIT         0x00000002
#define NET_SOCKET_FLAG_REUSE_EXACT_ADDRESS     0x00000004
#define NET_SOCKET_FLAG_BROADCAST_ENABLED       0x00000008
#define NET_SOCKET_FLAG_ACTIVE                  0x00000010
#define NET_SOCKET_FLAG_PREVIOUSLY_ACTIVE       0x00000020
#define NET_SOCKET_FLAG_TIME_WAIT               0x00000040
#define NET_SOCKET_FLAG_FORKED_LISTENER         0x00000080
#define NET_SOCKET_FLAG_NETWORK_HEADER_INCLUDED 0x00000100
#define NET_SOCKET_FLAG_KERNEL                  0x00000200

//
// Define the set of network socket flags that should be carried over to a
// copied socket after a spawned connection.
//

#define NET_SOCKET_FLAGS_INHERIT_MASK       0x0000000F

//
// Define the network buffer allocation flags.
//

#define NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS 0x00000001
#define NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS 0x00000002
#define NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS   0x00000004
#define NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS   0x00000008
#define NET_ALLOCATE_BUFFER_FLAG_UNENCRYPTED             0x00000010

//
// Define the network packet flags.
//

#define NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD  0x00000001
#define NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD 0x00000002
#define NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD 0x00000004
#define NET_PACKET_FLAG_IP_CHECKSUM_FAILED   0x00000008
#define NET_PACKET_FLAG_UDP_CHECKSUM_FAILED  0x00000010
#define NET_PACKET_FLAG_TCP_CHECKSUM_FAILED  0x00000020
#define NET_PACKET_FLAG_FORCE_TRANSMIT       0x00000040
#define NET_PACKET_FLAG_UNENCRYPTED          0x00000080
#define NET_PACKET_FLAG_MULTICAST            0x00000100

#define NET_PACKET_FLAG_CHECKSUM_OFFLOAD_MASK \
    (NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD |    \
     NET_PACKET_FLAG_UDP_CHECKSUM_OFFLOAD |   \
     NET_PACKET_FLAG_TCP_CHECKSUM_OFFLOAD)

//
// Define the network link capabilities.
//

#define NET_LINK_CAPABILITY_TRANSMIT_IP_CHECKSUM_OFFLOAD  0x00000001
#define NET_LINK_CAPABILITY_TRANSMIT_UDP_CHECKSUM_OFFLOAD 0x00000002
#define NET_LINK_CAPABILITY_TRANSMIT_TCP_CHECKSUM_OFFLOAD 0x00000004
#define NET_LINK_CAPABILITY_RECEIVE_IP_CHECKSUM_OFFLOAD   0x00000008
#define NET_LINK_CAPABILITY_RECEIVE_UDP_CHECKSUM_OFFLOAD  0x00000010
#define NET_LINK_CAPABILITY_RECEIVE_TCP_CHECKSUM_OFFLOAD  0x00000020
#define NET_LINK_CAPABILITY_PROMISCUOUS_MODE              0x00000040

#define NET_LINK_CAPABILITY_CHECKSUM_TRANSMIT_MASK       \
    (NET_LINK_CAPABILITY_TRANSMIT_IP_CHECKSUM_OFFLOAD |  \
     NET_LINK_CAPABILITY_TRANSMIT_UDP_CHECKSUM_OFFLOAD | \
     NET_LINK_CAPABILITY_TRANSMIT_TCP_CHECKSUM_OFFLOAD)

#define NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK       \
    (NET_LINK_CAPABILITY_RECEIVE_IP_CHECKSUM_OFFLOAD |  \
     NET_LINK_CAPABILITY_RECEIVE_UDP_CHECKSUM_OFFLOAD | \
     NET_LINK_CAPABILITY_RECEIVE_TCP_CHECKSUM_OFFLOAD)

#define NET_LINK_CAPABILITY_CHECKSUM_MASK         \
    (NET_LINK_CAPABILITY_CHECKSUM_TRANSMIT_MASK | \
     NET_LINK_CAPABILITY_CHECKSUM_RECEIVE_MASK)

//
// Define the network packet size information flags.
//

#define NET_PACKET_SIZE_FLAG_UNENCRYPTED 0x00000001

//
// Define the socket binding flags.
//

#define NET_SOCKET_BINDING_FLAG_ACTIVATE                0x00000001
#define NET_SOCKET_BINDING_FLAG_NO_PORT_ASSIGNMENT      0x00000002
#define NET_SOCKET_BINDING_FLAG_ALLOW_REBIND            0x00000004
#define NET_SOCKET_BINDING_FLAG_ALLOW_UNBIND            0x00000008
#define NET_SOCKET_BINDING_FLAG_OVERWRITE_LOCAL         0x00000010
#define NET_SOCKET_BINDING_FLAG_SKIP_ADDRESS_VALIDATION 0x00000020

//
// Define the protocol entry flags.
//

#define NET_PROTOCOL_FLAG_UNICAST_ONLY        0x00000001
#define NET_PROTOCOL_FLAG_MATCH_ANY_PROTOCOL  0x00000002
#define NET_PROTOCOL_FLAG_FIND_ALL_SOCKETS    0x00000004
#define NET_PROTOCOL_FLAG_NO_DEFAULT_PROTOCOL 0x00000008
#define NET_PROTOCOL_FLAG_PORTLESS            0x00000010
#define NET_PROTOCOL_FLAG_NO_BIND_PERMISSIONS 0x00000020
#define NET_PROTOCOL_FLAG_CONNECTION_BASED    0x00000040

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _NET_SOCKET_BINDING_TYPE {
    SocketUnbound,
    SocketLocallyBound,
    SocketFullyBound,
    SocketBindingTypeCount,
    SocketBindingInvalid
} NET_SOCKET_BINDING_TYPE, *PNET_SOCKET_BINDING_TYPE;

typedef enum _NET_LINK_INFORMATION_TYPE {
    NetLinkInformationInvalid,
    NetLinkInformationChecksumOffload,
    NetLinkInformationPromiscuousMode
} NET_LINK_INFORMATION_TYPE, *PNET_LINK_INFORMATION_TYPE;

typedef enum _NET_ADDRESS_TYPE {
    NetAddressUnknown,
    NetAddressAny,
    NetAddressUnicast,
    NetAddressBroadcast,
    NetAddressMulticast
} NET_ADDRESS_TYPE, *PNET_ADDRESS_TYPE;

/*++

Structure Description:

    This structure defines packet size information.

Members:

    HeaderSize - Stores the total size of the headers needed to send a packet.

    FooterSize - Stores the total size of the footers needed to send a packet.

    MaxPacketSize - Stores the maximum size of a packet that can be sent to the
        physical layer. This includes all headers and footers. This is limited
        by the protocol, network, and link for bound sockets, but is only
        limited by the protocol and network for unbound sockets.

    MinPacketSize - Stores the minimum size of a packet that can be sent to the
        physical layer. This includes all headers and footers. This is only
        ever limited by the device link layer.

--*/

typedef struct _NET_PACKET_SIZE_INFORMATION {
    ULONG HeaderSize;
    ULONG FooterSize;
    ULONG MaxPacketSize;
    ULONG MinPacketSize;
} NET_PACKET_SIZE_INFORMATION, *PNET_PACKET_SIZE_INFORMATION;

/*++

Structure Description:

    This structure defines an entry in the list of link layer network addresses
    owned by the link.

Members:

    ListEntry - Stores pointers to the next and previous addresses owned by the
        link.

    Configured - Stores a boolean indicating whether or not the network link
        address is configured.

    StaticAddress - Stores a boolean indicating whether or not the network
        address is static (TRUE) or dynamic (FALSE).

    Address - Stores the network address of the link.

    Subnet - Stores the network subnet mask of the link.

    DefaultGateway - Stores the default gateway network address for the link.

    DnsServer - Stores an array of network addresses of Domain Name Servers
        to try, in order.

    DnsServerCount - Stores the number of valid DNS servers in the array.

    PhysicalAddress - Stores the physical address of the link.

    LeaseServerAddress - Stores the network address of the server who provided
        the network address if it is a dynamic address.

    LeaseStartTime - Stores the time the lease on the network address began.

    LeaseEndTime - Stores the time the lease on the network address ends.

--*/

typedef struct _NET_LINK_ADDRESS_ENTRY {
    LIST_ENTRY ListEntry;
    BOOL Configured;
    BOOL StaticAddress;
    NETWORK_ADDRESS Address;
    NETWORK_ADDRESS Subnet;
    NETWORK_ADDRESS DefaultGateway;
    NETWORK_ADDRESS DnsServer[NETWORK_DEVICE_MAX_DNS_SERVERS];
    ULONG DnsServerCount;
    NETWORK_ADDRESS PhysicalAddress;
    NETWORK_ADDRESS LeaseServerAddress;
    SYSTEM_TIME LeaseStartTime;
    SYSTEM_TIME LeaseEndTime;
} NET_LINK_ADDRESS_ENTRY, *PNET_LINK_ADDRESS_ENTRY;

/*++

Structure Description:

    This structure defines information about a network packet.

Members:

    ListEntry - Stores pointers to the next and previous network packets.

    Buffer - Stores the virtual address of the buffer.

    IoBuffer - Stores a pointer to the I/O buffer backing this buffer.

    BufferPhysicalAddress - Stores the physical address of the buffer.

    Flags - Stores a bitmask of network packet buffer flags. See
        NET_PACKET_FLAG_* for definitions.

    BufferSize - Stores the size of the buffer, in bytes.

    DataSize - Stores the size of the data, including the headers payload, and
        footers.

    DataOffset - Stores the offset from the beginning of the buffer to the
        beginning of the valid data. The next lower layer should put its own
        headers right before this offset.

    FooterOffset - Stores the offset from the beginning of the buffer to the
        beginning of the footer data (ie the location to store the first byte
        of new footer).

--*/

typedef struct _NET_PACKET_BUFFER {
    LIST_ENTRY ListEntry;
    PVOID Buffer;
    PIO_BUFFER IoBuffer;
    PHYSICAL_ADDRESS BufferPhysicalAddress;
    ULONG Flags;
    ULONG BufferSize;
    ULONG DataSize;
    ULONG DataOffset;
    ULONG FooterOffset;
} NET_PACKET_BUFFER, *PNET_PACKET_BUFFER;

/*++

Struction Description:

    This structure defines a list of network packet buffers.

Members:

    Head - Stores pointers to the first and last network packet buffers in the
        list.

    Count - Stores the total number of packets in the list.

--*/

typedef struct _NET_PACKET_LIST {
    LIST_ENTRY Head;
    UINTN Count;
} NET_PACKET_LIST, *PNET_PACKET_LIST;

typedef
KSTATUS
(*PNET_DEVICE_LINK_SEND) (
    PVOID DeviceContext,
    PNET_PACKET_LIST PacketList
    );

/*++

Routine Description:

    This routine sends data through the network.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link down which this data is to be sent.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

Return Value:

    STATUS_SUCCESS if all packets were sent.

    STATUS_RESOURCE_IN_USE if some or all of the packets were dropped due to
    the hardware being backed up with too many packets to send.

    Other failure codes indicate that none of the packets were sent.

--*/

typedef
KSTATUS
(*PNET_DEVICE_LINK_GET_SET_INFORMATION) (
    PVOID DeviceContext,
    NET_LINK_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the network device layer's link information.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link for which information is being set or queried.

    InformationType - Supplies the type of information being queried or set.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or a
        set operation (TRUE).

Return Value:

    Status code.

--*/

typedef
VOID
(*PNET_DEVICE_LINK_DESTROY_LINK) (
    PVOID DeviceContext
    );

/*++

Routine Description:

    This routine notifies the device layer that the networking core is in the
    process of destroying the link and will no longer call into the device for
    this link. This allows the device layer to release any context that was
    supporting the device link interface.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link being destroyed.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the interface to a device link from the core
    networking library.

Members:

    Send - Stores a pointer to a function used to transmit data to the network.

    GetSetInformation - Supplies a pointer to a function used to get or set
        network link information.

    DestroyLink - Supplies a pointer to a function used to notify the device
        that the network link is no longer in use by the networking core and
        any link interface context can be destroyed.

--*/

typedef struct _NET_DEVICE_LINK_INTERFACE {
    PNET_DEVICE_LINK_SEND Send;
    PNET_DEVICE_LINK_GET_SET_INFORMATION GetSetInformation;
    PNET_DEVICE_LINK_DESTROY_LINK DestroyLink;
} NET_DEVICE_LINK_INTERFACE, *PNET_DEVICE_LINK_INTERFACE;

/*++

Structure Description:

    This structure defines characteristics about a network link.

Members:

    Version - Stores the version number of the structure. Set this to
        NET_LINK_PROPERTIES_VERSION.

    TransmitAlignment - Stores the alignment requirement for transmit buffers.

    Device - Stores a pointer to the physical layer device backing the link.

    DeviceContext - Stores a pointer to device-specific context on this link.

    PacketSizeInformation - Stores the packet size information that includes
        the maximum number of bytes that can be sent over the physical link and
        the header and footer sizes.

    Capabilities - Stores a bitmask of capabilities indicating whether or not
        certain features are supported by the link. See NET_LINK_CAPABILITY_*
        for definitions. This is a static field and does not describe which
        features are currently enabled.

    DataLinkType - Stores the type of the data link layer used by the network
        link.

    MaxPhysicalAddress - Stores the maximum physical address that the network
        controller can access.

    PhysicalAddress - Stores the original primary physical address of the link.

    Interface - Stores the list of functions used by the core networking
        library to call into the link.

--*/

typedef struct _NET_LINK_PROPERTIES {
    ULONG Version;
    ULONG TransmitAlignment;
    PDEVICE Device;
    PVOID DeviceContext;
    NET_PACKET_SIZE_INFORMATION PacketSizeInformation;
    ULONG Capabilities;
    NET_DOMAIN_TYPE DataLinkType;
    PHYSICAL_ADDRESS MaxPhysicalAddress;
    NETWORK_ADDRESS PhysicalAddress;
    NET_DEVICE_LINK_INTERFACE Interface;
} NET_LINK_PROPERTIES, *PNET_LINK_PROPERTIES;

typedef struct _NET_DATA_LINK_ENTRY NET_DATA_LINK_ENTRY, *PNET_DATA_LINK_ENTRY;

/*++

Structure Description:

    This structure defines a network link, something that can actually send
    packets out onto the network.

Members:

    ListEntry - Stores pointers to the next and previous network links
        available in the system.

    ReferenceCount - Stores the reference count of the link.

    QueuedLock - Stores a queued lock protecting access to various data
        structures in this structure. This lock must only be called at low
        level.

    LinkAddressList - Stores the head of the list of link layer addresses owned
        by this link. For example, in IPv4 this would be the list of IP
        addresses this link responds to. These entries are of type
        NET_LINK_ADDRESS_ENTRY.

    LinkUp - Stores a boolean indicating whether the link is active (TRUE) or
        disconnected (FALSE).

    LinkSpeed - Stores the maximum speed of the link, in bits per second.

    DataLinkEntry - Stores a pointer to the data link entry to use for this
        link.

    DataLinkContext - Stores a pointer to a private context for the data link
        layer. This can be set directly during data link initialization.

    Properties - Stores the link properties.

    AddressTranslationEvent - Stores the event waited on when a new address
        translation is required.

    AddressTranslationTree - Stores the tree containing translations between
        network addresses and physical addresses, keyed by network address.

--*/

typedef struct _NET_LINK {
    LIST_ENTRY ListEntry;
    volatile ULONG ReferenceCount;
    PQUEUED_LOCK QueuedLock;
    LIST_ENTRY LinkAddressList;
    BOOL LinkUp;
    ULONGLONG LinkSpeed;
    PNET_DATA_LINK_ENTRY DataLinkEntry;
    PVOID DataLinkContext;
    NET_LINK_PROPERTIES Properties;
    PKEVENT AddressTranslationEvent;
    RED_BLACK_TREE AddressTranslationTree;
} NET_LINK, *PNET_LINK;

typedef
KSTATUS
(*PNET_DATA_LINK_INITIALIZE_LINK) (
    PNET_LINK Link
    );

/*++

Routine Description:

    This routine initializes any pieces of information needed by the data link
    layer for a new link.

Arguments:

    Link - Supplies a pointer to the new link.

Return Value:

    Status code.

--*/

typedef
VOID
(*PNET_DATA_LINK_DESTROY_LINK) (
    PNET_LINK Link
    );

/*++

Routine Description:

    This routine allows the data link layer to tear down any state before a
    link is destroyed.

Arguments:

    Link - Supplies a pointer to the dying link.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PNET_DATA_LINK_SEND) (
    PVOID DataLinkContext,
    PNET_PACKET_LIST PacketList,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    );

/*++

Routine Description:

    This routine sends data through the data link layer and out the link.

Arguments:

    DataLinkContext - Supplies a pointer to the data link context for the
        link on which to send the data.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

    SourcePhysicalAddress - Supplies a pointer to the source (local) physical
        network address.

    DestinationPhysicalAddress - Supplies the optional physical address of the
        destination, or at least the next hop. If NULL is provided, then the
        packets will be sent to the data link layer's broadcast address.

    ProtocolNumber - Supplies the protocol number of the data inside the data
        link header.

Return Value:

    Status code.

--*/

typedef
VOID
(*PNET_DATA_LINK_PROCESS_RECEIVED_PACKET) (
    PVOID DataLinkContext,
    PNET_PACKET_BUFFER Packet
    );

/*++

Routine Description:

    This routine is called to process a received data link layer packet.

Arguments:

    DataLinkContext - Supplies a pointer to the data link context for the link
        that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

typedef
KSTATUS
(*PNET_DATA_LINK_CONVERT_TO_PHYSICAL_ADDRESS) (
    PNETWORK_ADDRESS NetworkAddress,
    PNETWORK_ADDRESS PhysicalAddress,
    NET_ADDRESS_TYPE NetworkAddressType
    );

/*++

Routine Description:

    This routine converts the given network address to a physical layer address
    based on the provided network address type.

Arguments:

    NetworkAddress - Supplies a pointer to the network layer address to convert.

    PhysicalAddress - Supplies a pointer to an address that receives the
        converted physical layer address.

    NetworkAddressType - Supplies the classified type of the given network
        address, which aids in conversion.

Return Value:

    Status code.

--*/

typedef
ULONG
(*PNET_DATA_LINK_PRINT_ADDRESS) (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

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

typedef
VOID
(*PNET_DATA_LINK_GET_PACKET_SIZE_INFORMATION) (
    PVOID DataLinkContext,
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation,
    ULONG Flags
    );

/*++

Routine Description:

    This routine gets the current packet size information for the given link.
    As the number of required headers can be different for each link, the
    packet size information is not a constant for an entire data link layer.

Arguments:

    DataLinkContext - Supplies a pointer to the data link context of the link
        whose packet size information is being queried.

    PacketSizeInformation - Supplies a pointer to a structure that receives the
        link's data link layer packet size information.

    Flags - Supplies a bitmask of flags indicating which packet size
        information is desired. See NET_PACKET_SIZE_FLAG_* for definitions.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the interface to the data link from the core
    networking library.

Members:

    InitializeLink - Stores a pointer to a function called when a new link
        is created.

    DestroyLink - Stores a pointer to a function called before a link is
        destroyed.

    Send - Stores a pointer to a function used to transmit data to the network.

    ProcessReceivedPacket - Stores a pointer to a function used to process
        received data link layer packets.

    GetBroadcastAddress - Stores a pointer to a function used to retrieve the
        data link layer's physical broadcast address.

    PrintAddress - Stores a pointer to a function used to convert a data link
        address into a string representation.

--*/

typedef struct _NET_DATA_LINK_INTERFACE {
    PNET_DATA_LINK_INITIALIZE_LINK InitializeLink;
    PNET_DATA_LINK_DESTROY_LINK DestroyLink;
    PNET_DATA_LINK_SEND Send;
    PNET_DATA_LINK_PROCESS_RECEIVED_PACKET ProcessReceivedPacket;
    PNET_DATA_LINK_CONVERT_TO_PHYSICAL_ADDRESS ConvertToPhysicalAddress;
    PNET_DATA_LINK_PRINT_ADDRESS PrintAddress;
    PNET_DATA_LINK_GET_PACKET_SIZE_INFORMATION GetPacketSizeInformation;
} NET_DATA_LINK_INTERFACE, *PNET_DATA_LINK_INTERFACE;

/*++

Structure Description:

    This structure defines a data link entry.

Members:

    ListEntry - Stores pointers to the next and previous data link entries,
        used internally by the core network library.

    Domain - Stores the network domain type this data link implements.

    Interface - Stores the interface presented to the core networking library
        for this data link.

--*/

struct _NET_DATA_LINK_ENTRY {
    LIST_ENTRY ListEntry;
    NET_DOMAIN_TYPE Domain;
    NET_DATA_LINK_INTERFACE Interface;
};

/*++

Structure Description:

    This structure defines the link information associated with a local address.

Members:

    Link - Stores a pointer to the link that owns the local address.

    LinkAdress - Stores a pointer to the link address entry that owns the local
        address.

    ReceiveAddress - Stores the local address on which packets can be received.

    SendAddress - Stores the local address from which packets will be sent.

--*/

typedef struct _NET_LINK_LOCAL_ADDRESS {
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    NETWORK_ADDRESS ReceiveAddress;
    NETWORK_ADDRESS SendAddress;
} NET_LINK_LOCAL_ADDRESS, *PNET_LINK_LOCAL_ADDRESS;

typedef struct _NET_PROTOCOL_ENTRY NET_PROTOCOL_ENTRY, *PNET_PROTOCOL_ENTRY;
typedef struct _NET_NETWORK_ENTRY NET_NETWORK_ENTRY, *PNET_NETWORK_ENTRY;
typedef struct _NET_RECEIVE_CONTEXT NET_RECEIVE_CONTEXT, *PNET_RECEIVE_CONTEXT;

/*++

Structure Description:

    This structure defines a core networking library socket.

Members:

    KernelSocket - Stores the common parameters recognized by the kernel.

    Protocol - Stores a pointer to the protocol entry responsible for this
        socket.

    Network - Stores a pointer to the network layer entry responsible for this
        socket.

    LocalReceiveAddress - Stores the local address to which the socket is bound
        to for receiving packets. This may be the any address or broadcast
        address.

    LocalSendAddress - Stores the local address to which the socket is bound to
        for sending packets. This must be a unicast address.

    RemoteAddress - Stores the remote address of this connection.

    RemotePhysicalAddress - Stores the remote physical address of this
        connection.

    TreeEntry - Stores the information about this socket in the tree of
        sockets (which is either on the link itself or global).

    ListEntry - Stores the information about this socket in the list of sockets.
        This is only used for raw sockets; they do not get inserted in a tree.

    BindingType - Stores the type of binding for this socket (unbound, locally
        bound, or fully bound).

    Flags - Stores a bitmask of network socket flags. See NET_SOCKET_FLAG_*
        for definitions.

    PacketSizeInformation - Stores the packet size information bound by the
        protocol, network and link layers if the socket is locally bound. For
        unbound sockets, this stores the size information limited by only the
        protocol and network layers.

    UnboundPacketSizeInformation - Stores the packet size information bound by
        only the protocol and network layers.

    LastError - Stores the last error encountered by this socket.

    Link - Stores a pointer to the link this socket is associated with.

    LinkAddress - Stores the link address information for the given socket.

    SendPacketCount - Stores the number of packets sent on this socket.

    MaxIncomingConnections - Stores the maximum number of pending but not yet
        accepted connections that are allowed to accumulate before connections
        are refused. In the sockets API this is known as the backlog count.

    NetworkSocketInformation - Stores an optional pointer to the network
        layer's socket information.

--*/

typedef struct _NET_SOCKET {
    SOCKET KernelSocket;
    PNET_PROTOCOL_ENTRY Protocol;
    PNET_NETWORK_ENTRY Network;
    NETWORK_ADDRESS LocalReceiveAddress;
    NETWORK_ADDRESS LocalSendAddress;
    NETWORK_ADDRESS RemoteAddress;
    NETWORK_ADDRESS RemotePhysicalAddress;
    union {
        RED_BLACK_TREE_NODE TreeEntry;
        LIST_ENTRY ListEntry;
    } U;

    NET_SOCKET_BINDING_TYPE BindingType;
    volatile ULONG Flags;
    NET_PACKET_SIZE_INFORMATION PacketSizeInformation;
    NET_PACKET_SIZE_INFORMATION UnboundPacketSizeInformation;
    volatile KSTATUS LastError;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    ULONG SendPacketCount;
    ULONG MaxIncomingConnections;
    PVOID NetworkSocketInformation;
} NET_SOCKET, *PNET_SOCKET;

/*++

Structure Description:

    This structure defines a core networking socket link override. This stores
    all the socket and link specific information needed to send a packet. This
    can be used to send data from a link on behalf of a socket if the socket
    is not yet bound to a link.

Members:

    LinkInformation - Stores the local address and its associated link and link
        address entry.

    PacketSizeInformation - Stores the packet size information bound by the
        protocol, network and link layers.

--*/

typedef struct _NET_SOCKET_LINK_OVERRIDE {
    NET_LINK_LOCAL_ADDRESS LinkInformation;
    NET_PACKET_SIZE_INFORMATION PacketSizeInformation;
} NET_SOCKET_LINK_OVERRIDE, *PNET_SOCKET_LINK_OVERRIDE;

typedef
KSTATUS
(*PNET_PROTOCOL_CREATE_SOCKET) (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    );

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

typedef
VOID
(*PNET_PROTOCOL_DESTROY_SOCKET) (
    PNET_SOCKET Socket
    );

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

typedef
KSTATUS
(*PNET_PROTOCOL_BIND_TO_ADDRESS) (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

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

typedef
KSTATUS
(*PNET_PROTOCOL_LISTEN) (
    PNET_SOCKET Socket
    );

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing clients to attempt to connect to it.

Arguments:

    Socket - Supplies a pointer to the socket to mark as listning.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET_PROTOCOL_ACCEPT) (
    PNET_SOCKET Socket,
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
(*PNET_PROTOCOL_CONNECT) (
    PNET_SOCKET Socket,
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
(*PNET_PROTOCOL_CLOSE) (
    PNET_SOCKET Socket
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
(*PNET_PROTOCOL_SHUTDOWN) (
    PNET_SOCKET Socket,
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
(*PNET_PROTOCOL_SEND) (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

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

typedef
VOID
(*PNET_PROTOCOL_PROCESS_RECEIVED_DATA) (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

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

typedef
KSTATUS
(*PNET_PROTOCOL_PROCESS_RECEIVED_SOCKET_DATA) (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

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

typedef
KSTATUS
(*PNET_PROTOCOL_RECEIVE) (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

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

typedef
KSTATUS
(*PNET_PROTOCOL_GET_SET_INFORMATION) (
    PNET_SOCKET Socket,
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

    STATUS_NOT_HANDLED if the protocol does not override the default behavior
        for a basic socket option.

--*/

typedef
KSTATUS
(*PNET_PROTOCOL_USER_CONTROL) (
    PNET_SOCKET Socket,
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

    This structure defines interface between the core networking library and
    a network protocol.

Members:

    CreateSocket - Stores a pointer to a function that creates a new socket.

    DestroySocket - Stores a pointer to a function that destroys all resources
        associated with a socket, called when the socket's reference count
        drops to zero.

    BindToAddress - Stores a pointer to a function that binds an open but
        unbound socket to a particular network address.

    Listen - Stores a pointer to a function that starts the socket listening
        for incoming connections.

    Accept - Stores a pointer to a function that accepts an incoming connection
        from a remote host on a listening socket.

    Connect - Stores a pointer to a function used to establish a connection with
        a remote server.

    Close - Stores a pointer to a function used to shut down and close a
        connection.

    Shutdown - Stores a pointer to a function used to shut down a connection.

    Send - Stores a pointer to a function used to send data to a connection.

    ProcessReceivedData - Stores a pointer to a function called when a packet
        is received from the network.

    ProcessReceivedSocketData - Stores a pointer to a function that processes a
        packet targeting a specific socket.

    Receive - Stores a pointer to a function called by the user to receive
        data from the socket.

    GetSetInformation - Stores a pointer to a function used to get or set
        socket information.

    UserControl - Stores a pointer to a function used to respond to user
        control (ioctl) requests.

--*/

typedef struct _NET_PROTOCOL_INTERFACE {
    PNET_PROTOCOL_CREATE_SOCKET CreateSocket;
    PNET_PROTOCOL_DESTROY_SOCKET DestroySocket;
    PNET_PROTOCOL_BIND_TO_ADDRESS BindToAddress;
    PNET_PROTOCOL_LISTEN Listen;
    PNET_PROTOCOL_ACCEPT Accept;
    PNET_PROTOCOL_CONNECT Connect;
    PNET_PROTOCOL_CLOSE Close;
    PNET_PROTOCOL_SHUTDOWN Shutdown;
    PNET_PROTOCOL_SEND Send;
    PNET_PROTOCOL_PROCESS_RECEIVED_DATA ProcessReceivedData;
    PNET_PROTOCOL_PROCESS_RECEIVED_SOCKET_DATA ProcessReceivedSocketData;
    PNET_PROTOCOL_RECEIVE Receive;
    PNET_PROTOCOL_GET_SET_INFORMATION GetSetInformation;
    PNET_PROTOCOL_USER_CONTROL UserControl;
} NET_PROTOCOL_INTERFACE, *PNET_PROTOCOL_INTERFACE;

/*++

Structure Description:

    This structure defines a network protocol entry.

Members:

    ListEntry - Stores pointers to the next and previous protocol entries, used
        internally by the core networking library.

    Type - Stores the connection type this protocol implements.

    ParentProtocolNumber - Stores the protocol number in the parent layer's
        protocol.

    Flags - Stores a bitmask of protocol flags. See NET_PROTOCOL_FLAG_* for
        definitions.

    LastSocket - Stores a pointer to the last socket that received a packet.

    SocketLock - Stores a pointer to a shared exclusive lock that protects the
        socket trees.

    SocketTree - Stores an array of Red Black Trees, one each for fully bound,
        locally bound, and unbound sockets.

    Interface - Stores the interface presented to the kernel for this type of
        socket.

--*/

struct _NET_PROTOCOL_ENTRY {
    LIST_ENTRY ListEntry;
    NET_SOCKET_TYPE Type;
    ULONG ParentProtocolNumber;
    ULONG Flags;
    volatile PNET_SOCKET LastSocket;
    PSHARED_EXCLUSIVE_LOCK SocketLock;
    RED_BLACK_TREE SocketTree[SocketBindingTypeCount];
    NET_PROTOCOL_INTERFACE Interface;
};

typedef
KSTATUS
(*PNET_NETWORK_INITIALIZE_LINK) (
    PNET_LINK Link
    );

/*++

Routine Description:

    This routine initializes any pieces of information needed by the network
    layer for a new link.

Arguments:

    Link - Supplies a pointer to the new link.

Return Value:

    Status code.

--*/

typedef
VOID
(*PNET_NETWORK_DESTROY_LINK) (
    PNET_LINK Link
    );

/*++

Routine Description:

    This routine allows the network layer to tear down any state before a link
    is destroyed.

Arguments:

    Link - Supplies a pointer to the dying link.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PNET_NETWORK_INITIALIZE_SOCKET) (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    );

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

typedef
VOID
(*PNET_NETWORK_DESTROY_SOCKET) (
    PNET_SOCKET Socket
    );

/*++

Routine Description:

    This routine destroys any pieces allocated by the network layer for the
    socket.

Arguments:

    Socket - Supplies a pointer to the socket to destroy.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PNET_NETWORK_BIND_TO_ADDRESS) (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address,
    ULONG Flags
    );

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

typedef
KSTATUS
(*PNET_NETWORK_LISTEN) (
    PNET_SOCKET Socket
    );

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing clients to attempt to connect to it.

Arguments:

    Socket - Supplies a pointer to the socket to mark as listning.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET_NETWORK_CONNECT) (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

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

typedef
KSTATUS
(*PNET_NETWORK_DISCONNECT) (
    PNET_SOCKET Socket
    );

/*++

Routine Description:

    This routine will disconnect the given socket from its remote address.

Arguments:

    Socket - Supplies a pointer to the socket to disconnect.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET_NETWORK_CLOSE) (
    PNET_SOCKET Socket
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
(*PNET_NETWORK_SEND) (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Destination,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride,
    PNET_PACKET_LIST PacketList
    );

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

typedef
VOID
(*PNET_NETWORK_PROCESS_RECEIVED_DATA) (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

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

typedef
ULONG
(*PNET_NETWORK_PRINT_ADDRESS) (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

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

typedef
KSTATUS
(*PNET_NETWORK_GET_SET_INFORMATION) (
    PNET_SOCKET Socket,
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
(*PNET_NETWORK_COPY_INFORMATION) (
    PNET_SOCKET DestinationSocket,
    PNET_SOCKET SourceSocket
    );

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

typedef
NET_ADDRESS_TYPE
(*PNET_NETWORK_GET_ADDRESS_TYPE) (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry,
    PNETWORK_ADDRESS Address
    );

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

/*++

Structure Description:

    This structure defines interface between the core networking library and
    a network.

Members:

    InitializeLink - Stores a pointer to a function called when a new link
        is created.

    DestroyLink - Stores a pointer to a function called before a link is
        destroyed.

    InitializeSocket - Stores a pointer to a function that initializes a newly
        created socket.

    DestroySocket - Stores a pointer to a function that destroys network
        specific socket structures allocated during initialize. This function
        is optional if the initialize routine made no allocations.

    BindToAddress - Stores a pointer to a function that binds an open but
        unbound socket to a particular network address.

    Listen - Stores a pointer to a function that marks the packet as listening
        and allows clients to attempt to connect to it.

    Connect - Stores a pointer to a function that binds a socket to a remote
        host.

    Close - Stores a pointer to a function used to shut down a connection.

    Send - Stores a pointer to a function used to send data.

    ProcessReceivedData - Stores a pointer to a function called when a
        received packet comes in.

    PrintAddress - Stores a pointer to a function used to convert a network
        address into a string representation.

    GetSetInformation - Stores a pointer to a function used to get or set
        socket information.

    CopyInformation - Stores a pointer to a function used to copy socket option
        information from one socket to another. This function is optional if
        there are no network specific socket options.

    GetAddressType - Stores a pointer to a function used to categorize a given
        network address into one of many types (e.g. unicast, broadcast,
        mulitcast). This function is optional if unicast is the only supported
        address type.

--*/

typedef struct _NET_NETWORK_INTERFACE {
    PNET_NETWORK_INITIALIZE_LINK InitializeLink;
    PNET_NETWORK_DESTROY_LINK DestroyLink;
    PNET_NETWORK_INITIALIZE_SOCKET InitializeSocket;
    PNET_NETWORK_DESTROY_SOCKET DestroySocket;
    PNET_NETWORK_BIND_TO_ADDRESS BindToAddress;
    PNET_NETWORK_LISTEN Listen;
    PNET_NETWORK_CONNECT Connect;
    PNET_NETWORK_DISCONNECT Disconnect;
    PNET_NETWORK_CLOSE Close;
    PNET_NETWORK_SEND Send;
    PNET_NETWORK_PROCESS_RECEIVED_DATA ProcessReceivedData;
    PNET_NETWORK_PRINT_ADDRESS PrintAddress;
    PNET_NETWORK_GET_SET_INFORMATION GetSetInformation;
    PNET_NETWORK_COPY_INFORMATION CopyInformation;
    PNET_NETWORK_GET_ADDRESS_TYPE GetAddressType;
} NET_NETWORK_INTERFACE, *PNET_NETWORK_INTERFACE;

/*++

Structure Description:

    This structure defines a network entry.

Members:

    ListEntry - Stores pointers to the next and previous network entries, used
        internally by the core networking library.

    Domain - Stores the domain this network implements.

    ParentProtocolNumber - Stores the protocol number in the parent layer's
        protocol.

    Interface - Stores the interface presented to the core networking library
        for this network.

--*/

struct _NET_NETWORK_ENTRY {
    LIST_ENTRY ListEntry;
    NET_DOMAIN_TYPE Domain;
    ULONG ParentProtocolNumber;
    NET_NETWORK_INTERFACE Interface;
};

/*++

Structure Description:

    This structure defines the context for receiving a network packet. Each
    layer will fill in the portions of the context it owns and pass it up the
    stack. This structure and even the address pointers can be stack allocated
    as it will not be referenced after the network layers have completed the
    receive.

Members:

    Packet - Supplies a pointer to the packet that came in over the network.
        This structure may not be used as a scratch space while the packet
        travels up the stack as it may be sent out to multiple sockets (e.g.
        multicast or broadcast packets).

    Link - Supplies a pointer to the network link that received the packet.

    Network - Supplies a pointer to the network to which the packet belongs.

    Protocol - Supplies a pointer to the protocol to which the packet belongs.

    Source - Supplies a pointer to the source (remote) address of the packet.

    Destination - Supplies a pointer to the destination (local) address of the
        packet.

    ParentProtocolNumber - Stores the protocol number in the parent layer's
        protocol. This will always be set after the network layer executes.

--*/

struct _NET_RECEIVE_CONTEXT {
    PNET_PACKET_BUFFER Packet;
    PNET_LINK Link;
    PNET_NETWORK_ENTRY Network;
    PNET_PROTOCOL_ENTRY Protocol;
    PNETWORK_ADDRESS Source;
    PNETWORK_ADDRESS Destination;
    ULONG ParentProtocolNumber;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

NET_API
KSTATUS
NetRegisterProtocol (
    PNET_PROTOCOL_ENTRY NewProtocol,
    PHANDLE ProtocolHandle
    );

/*++

Routine Description:

    This routine registers a new protocol type with the core networking
    library.

Arguments:

    NewProtocol - Supplies a pointer to the protocol information. The core
        networking library *will* continue to use the memory after the function
        returns, so this pointer had better not point to stack allocated
        memory.

    ProtocolHandle - Supplies an optional pointer that receives a handle to the
        registered protocol on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if part of the structure isn't filled out
    correctly.

    STATUS_DUPLICATE_ENTRY if the socket type is already registered.

--*/

NET_API
VOID
NetUnregisterProtocol (
    HANDLE ProtocolHandle
    );

/*++

Routine Description:

    This routine unregisters the given protocol from the core networking
    library.

Arguments:

    ProtocolHandle - Supplies the handle to the protocol to unregister.

Return Value:

    None.

--*/

NET_API
KSTATUS
NetRegisterNetworkLayer (
    PNET_NETWORK_ENTRY NewNetworkEntry,
    PHANDLE NetworkHandle
    );

/*++

Routine Description:

    This routine registers a new network type with the core networking library.

Arguments:

    NewNetworkEntry - Supplies a pointer to the network information. The core
        library will not reference this memory after the function returns, a
        copy will be made.

    NetworkHandle - Supplies an optional pointer that receives a handle to the
        registered network layer on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if part of the structure isn't filled out
    correctly.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

    STATUS_DUPLICATE_ENTRY if the network type is already registered.

--*/

NET_API
VOID
NetUnregisterNetworkLayer (
    HANDLE NetworkHandle
    );

/*++

Routine Description:

    This routine unregisters the given network layer from the core networking
    library.

Arguments:

    NetworkHandle - Supplies the handle to the network layer to unregister.

Return Value:

    None.

--*/

NET_API
KSTATUS
NetRegisterDataLinkLayer (
    PNET_DATA_LINK_ENTRY NewDataLinkEntry,
    PHANDLE DataLinkHandle
    );

/*++

Routine Description:

    This routine registers a new data link type with the core networking
    library.

Arguments:

    NewDataLinkEntry - Supplies a pointer to the link information. The core
        library will not reference this memory after the function returns, a
        copy will be made.

    DataLinkHandle - Supplies an optional pointer that receives a handle to the
        registered data link layer on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if part of the structure isn't filled out
    correctly.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

    STATUS_DUPLICATE_ENTRY if the link type is already registered.

--*/

NET_API
VOID
NetUnregisterDataLinkLayer (
    HANDLE DataLinkHandle
    );

/*++

Routine Description:

    This routine unregisters the given data link layer from the core networking
    library.

Arguments:

    DataLinkHandle - Supplies the handle to the data link layer to unregister.

Return Value:

    None.

--*/

NET_API
PNET_NETWORK_ENTRY
NetGetNetworkEntry (
    ULONG ParentProtocolNumber
    );

/*++

Routine Description:

    This routine looks up a registered network layer given the parent protocol
    number.

Arguments:

    ParentProtocolNumber - Supplies the parent protocol number of the desired
        network layer.

Return Value:

    Returns a pointer to the network layer entry on success.

--*/

NET_API
PNET_PROTOCOL_ENTRY
NetGetProtocolEntry (
    ULONG ParentProtocolNumber
    );

/*++

Routine Description:

    This routine looks up a registered protocol layer given the parent protocol
    number.

Arguments:

    ParentProtocolNumber - Supplies the parent protocol number of the desired
        protocol layer.

Return Value:

    Returns a pointer to the protocol layer entry on success.

--*/

NET_API
VOID
NetProcessReceivedPacket (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

/*++

Routine Description:

    This routine is called by the low level NIC driver to pass received packets
    onto the core networking library for dispatching.

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

NET_API
BOOL
NetGetGlobalDebugFlag (
    VOID
    );

/*++

Routine Description:

    This routine returns the current value of the global networking debug flag.

Arguments:

    None.

Return Value:

    TRUE if debug information should be collected throughout the networking
    subsystem.

    FALSE if verbose debug information should be suppressed globally.

--*/

NET_API
VOID
NetDebugPrintAddress (
    PNETWORK_ADDRESS Address
    );

/*++

Routine Description:

    This routine prints the given address to the debug console.

Arguments:

    Address - Supplies a pointer to the address to print.

Return Value:

    None.

--*/

NET_API
KSTATUS
NetAddLink (
    PNET_LINK_PROPERTIES Properties,
    PNET_LINK *NewLink
    );

/*++

Routine Description:

    This routine adds a new network link based on the given properties. The
    link must be ready to send and receive traffic and have a valid physical
    layer address supplied in the properties.

Arguments:

    Properties - Supplies a pointer describing the properties and interface of
        the link. This memory will not be referenced after the function returns,
        so this may be a stack allocated structure.

    NewLink - Supplies a pointer where a pointer to the new link will be
        returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
        structure.

--*/

NET_API
VOID
NetLinkAddReference (
    PNET_LINK Link
    );

/*++

Routine Description:

    This routine increases the reference count on a network link.

Arguments:

    Link - Supplies a pointer to the network link whose reference count
        should be incremented.

Return Value:

    None.

--*/

NET_API
VOID
NetLinkReleaseReference (
    PNET_LINK Link
    );

/*++

Routine Description:

    This routine decreases the reference count of a network link, and destroys
    the link if the reference count drops to zero.

Arguments:

    Link - Supplies a pointer to the network link whose reference count
        should be decremented.

Return Value:

    None.

--*/

NET_API
VOID
NetSetLinkState (
    PNET_LINK Link,
    BOOL LinkUp,
    ULONGLONG LinkSpeed
    );

/*++

Routine Description:

    This routine sets the link state of the given link. The physical device
    layer is responsible for synchronizing link state changes.

Arguments:

    Link - Supplies a pointer to the link whose state is changing.

    LinkUp - Supplies a boolean indicating whether the link is active (TRUE) or
        disconnected (FALSE).

    LinkSpeed - Supplies the speed of the link, in bits per second.

Return Value:

    None.

--*/

NET_API
VOID
NetGetLinkState (
    PNET_LINK Link,
    PBOOL LinkUp,
    PULONGLONG LinkSpeed
    );

/*++

Routine Description:

    This routine gets the link state of the given link.

Arguments:

    Link - Supplies a pointer to the link whose state is being retrieved.

    LinkUp - Supplies a pointer that receives a boolean indicating whether the
        link is active (TRUE) or disconnected (FALSE). This parameter is
        optional.

    LinkSpeed - Supplies a pointer that receives the speed of the link, in bits
        per second. This parameter is optional.

Return Value:

    None.

--*/

NET_API
KSTATUS
NetGetSetLinkDeviceInformation (
    PNET_LINK Link,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets device information for a link.

Arguments:

    Link - Supplies a pointer to the link whose device information is being
        retrieved or set.

    Uuid - Supplies a pointer to the information identifier.

    Data - Supplies a pointer to the data buffer.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer in bytes. On output, returns the needed size of the data buffer,
        even if the supplied buffer was nonexistant or too small.

    Set - Supplies a boolean indicating whether to get the information (FALSE)
        or set the information (TRUE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the supplied buffer was too small.

    STATUS_NOT_HANDLED if the given UUID was not recognized.

--*/

NET_API
VOID
NetRemoveLink (
    PNET_LINK Link
    );

/*++

Routine Description:

    This routine removes a link from the networking core after its device has
    been removed. This should not be used if the media has simply been removed.
    In that case, setting the link state to 'down' is suffiient. There may
    still be outstanding references on the link, so the networking core will
    call the device back to notify it when the link is destroyed.

Arguments:

    Link - Supplies a pointer to the link to remove.

Return Value:

    None.

--*/

NET_API
KSTATUS
NetFindLinkForLocalAddress (
    PNET_NETWORK_ENTRY Network,
    PNETWORK_ADDRESS LocalAddress,
    PNET_LINK Link,
    PNET_LINK_LOCAL_ADDRESS LinkResult
    );

/*++

Routine Description:

    This routine searches for a link and the associated address entry that
    matches the given local address. If a link is supplied as a hint, then the
    given link must be able to service the given address for this routine to
    succeed.

Arguments:

    Network - Supplies a pointer to the network entry to which the address
        belongs.

    LocalAddress - Supplies a pointer to the local address to test against.

    Link - Supplies an optional pointer to a link that the local address must
        be from.

    LinkResult - Supplies a pointer that receives the found link, link address
        entry, and local address.

Return Value:

    STATUS_SUCCESS if a link was found and bound with the socket.

    STATUS_INVALID_ADDRESS if no link was found to own that address.

    STATUS_NO_NETWORK_CONNECTION if no networks are available.

--*/

NET_API
KSTATUS
NetFindLinkForRemoteAddress (
    PNETWORK_ADDRESS RemoteAddress,
    PNET_LINK_LOCAL_ADDRESS LinkResult
    );

/*++

Routine Description:

    This routine searches for a link and associated address entry that can
    reach the given remote address.

Arguments:

    RemoteAddress - Supplies a pointer to the address to test against.

    LinkResult - Supplies a pointer that receives the link information,
        including the link, link address entry, and associated local address.

Return Value:

    STATUS_SUCCESS if a link was found and bound with the socket.

    STATUS_NO_NETWORK_CONNECTION if no networks are available.

--*/

NET_API
KSTATUS
NetLookupLinkByDevice (
    PDEVICE Device,
    PNET_LINK *Link
    );

/*++

Routine Description:

    This routine looks for a link that belongs to the given device. If a link
    is found, a reference will be added. It is the callers responsibility to
    release this reference.

Arguments:

    Device - Supplies a pointer to the device for which the link is being
        searched.

    Link - Supplies a pointer that receives a pointer to the link, if found.

Return Value:

    Status code.

--*/

NET_API
KSTATUS
NetCreateLinkAddressEntry (
    PNET_LINK Link,
    PNETWORK_ADDRESS Address,
    PNETWORK_ADDRESS Subnet,
    PNETWORK_ADDRESS DefaultGateway,
    BOOL StaticAddress,
    PNET_LINK_ADDRESS_ENTRY *NewLinkAddress
    );

/*++

Routine Description:

    This routine initializes a new network link address entry.

Arguments:

    Link - Supplies a pointer to the physical link that has the new network
        address.

    Address - Supplies an optional pointer to the address to assign to the link
        address entry.

    Subnet - Supplies an optional pointer to the subnet mask to assign to the
        link address entry.

    DefaultGateway - Supplies an optional pointer to the default gateway
        address to assign to the link address entry.

    StaticAddress - Supplies a boolean indicating if the provided information
        is a static configuration of the link address entry. This parameter is
        only used when the Address, Subnet, and DefaultGateway parameters are
        supplied.

    NewLinkAddress - Supplies a pointer where a pointer to the new link address
        will be returned. The new link address will also be inserted onto the
        link.

Return Value:

    Status code.

--*/

NET_API
VOID
NetDestroyLinkAddressEntry (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

/*++

Routine Description:

    This routine removes and destroys a link address.

Arguments:

    Link - Supplies a pointer to the physical link that has the network address.

    LinkAddress - Supplies a pointer to the link address to remove and destroy.

Return Value:

    None.

--*/

NET_API
KSTATUS
NetTranslateNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    );

/*++

Routine Description:

    This routine translates a network level address to a physical address.

Arguments:

    NetworkAddress - Supplies a pointer to the network address to translate.

    Link - Supplies a pointer to the link to use.

    LinkAddress - Supplies a pointer to the link address entry to use for this
        request.

    PhysicalAddress - Supplies a pointer where the corresponding physical
        address for this network address will be returned.

Return Value:

    Status code.

--*/

NET_API
KSTATUS
NetAddNetworkAddressTranslation (
    PNET_LINK Link,
    PNETWORK_ADDRESS NetworkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    );

/*++

Routine Description:

    This routine adds a mapping between a network address and its associated
    physical address.

Arguments:

    Link - Supplies a pointer to the link receiving the mapping.

    NetworkAddress - Supplies a pointer to the network address whose physical
        mapping is known.

    PhysicalAddress - Supplies a pointer to the physical address corresponding
        to the network address.

Return Value:

    Status code.

--*/

NET_API
KSTATUS
NetFindEntryForAddress (
    PNET_LINK Link,
    PNET_NETWORK_ENTRY Network,
    PNETWORK_ADDRESS Address,
    PNET_LINK_ADDRESS_ENTRY *AddressEntry
    );

/*++

Routine Description:

    This routine searches for a link address entry within the given link
    matching the desired address.

Arguments:

    Link - Supplies the link whose address entries should be searched.

    Network - Supplies an optional pointer to the network entry to which the
        address belongs.

    Address - Supplies the address to search for.

    AddressEntry - Supplies a pointer where the address entry will be returned
        on success.

Return Value:

    STATUS_SUCCESS if a link was found and bound with the socket.

    STATUS_INVALID_ADDRESS if no link was found to own that address.

--*/

NET_API
KSTATUS
NetActivateSocket (
    PNET_SOCKET Socket
    );

/*++

Routine Description:

    This routine activates or re-activates a socket, making it eligible to
    receive data or updating it from an unbound or locally bound socket to a
    fully bound socket.

Arguments:

    Socket - Supplies a pointer to the initialized socket to add to the
        listening sockets tree.

Return Value:

    Status code.

--*/

NET_API
VOID
NetDeactivateSocket (
    PNET_SOCKET Socket
    );

/*++

Routine Description:

    This routine removes a socket from the socket tree it's on, removing it
    from eligibility to receive packets. If the socket is removed from the
    tree then a reference will be released.

Arguments:

    Socket - Supplies a pointer to the initialized socket to remove from the
        socket tree.

Return Value:

    None.

--*/

NET_API
KSTATUS
NetBindSocket (
    PNET_SOCKET Socket,
    NET_SOCKET_BINDING_TYPE TreeType,
    PNET_LINK_LOCAL_ADDRESS LocalInformation,
    PNETWORK_ADDRESS RemoteAddress,
    ULONG Flags
    );

/*++

Routine Description:

    This routine officially binds a socket to a local address, local port,
    remote address and remote port tuple by adding it to the appropriate socket
    tree. It can also re-bind a socket in the case where it has already been
    bound to a different tree.

Arguments:

    Socket - Supplies a pointer to the initialized socket to bind.

    TreeType - Supplies the type of tree to add the socket to.

    LocalInformation - Supplies an optional pointer to the information for the
        local link or address to which the socket shall be bound. Use this for
        unbound sockets, leaving the link and link address NULL.

    RemoteAddress - Supplies an optional pointer to a remote address to use
        when fully binding the socket.

    Flags - Supplies a bitmask of binding flags. See NET_SOCKET_BINDING_FLAG_*
        for definitions.

Return Value:

    Status code.

--*/

NET_API
KSTATUS
NetDisconnectSocket (
    PNET_SOCKET Socket
    );

/*++

Routine Description:

    This routine disconnects a socket from the fully bound state, rolling it
    back to the locally bound state.

Arguments:

    Socket - Supplies a pointer to the socket to disconnect.

Return Value:

    Status code.

--*/

NET_API
VOID
NetInitializeSocketLinkOverride (
    PNET_SOCKET Socket,
    PNET_LINK_LOCAL_ADDRESS LinkInformation,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride
    );

/*++

Routine Description:

    This routine initializes the given socket link override structure with the
    appropriate mix of socket and link information.

Arguments:

    Socket - Supplies a pointer to a network socket.

    LinkInformation - Supplies a pointer to link local address information.

    LinkOverride - Supplies a pointer to a socket link override structure that
        will be filled in by this routine.

Return Value:

    None.

--*/

NET_API
KSTATUS
NetFindSocket (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PNET_SOCKET *Socket
    );

/*++

Routine Description:

    This routine attempts to find a socket on the receiving end of the given
    context based on matching the addresses and protocol. If the socket is
    found and returned, the reference count will be increased on it. It is the
    caller's responsiblity to release that reference. If this routine returns
    that more processing is required, then subsequent calls should pass the
    previously found socket back to the routine and the search will pick up
    where it left off.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context used to find
        the socket. This contains the remote address, local address, protocol,
        and network to match on.

    Socket - Supplies a pointer that receives a pointer to the found socket on
        output. On input, it can optionally contain a pointer to the socket
        from which the search for a new socket should start.

Return Value:

    STATUS_SUCCESS if a socket was found.

    STATUS_MORE_PROCESSING_REQUIRED if a socket was found, but more sockets
    may match the given address tuple.

    Error status code otherwise.

--*/

NET_API
KSTATUS
NetGetSetNetworkDeviceInformation (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry,
    PNETWORK_DEVICE_INFORMATION Information,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the network device information for a particular
    link.

Arguments:

    Link - Supplies a pointer to the link to work with.

    LinkAddressEntry - Supplies an optional pointer to the specific address
        entry to set. If NULL, a link address entry matching the network type
        contained in the information will be found.

    Information - Supplies a pointer that either receives the device
        information, or contains the new information to set. For set operations,
        the information buffer will contain the current settings on return.

    Set - Supplies a boolean indicating if the information should be set or
        returned.

Return Value:

    Status code.

--*/

NET_API
COMPARISON_RESULT
NetCompareNetworkAddresses (
    PNETWORK_ADDRESS FirstAddress,
    PNETWORK_ADDRESS SecondAddress
    );

/*++

Routine Description:

    This routine compares two network addresses.

Arguments:

    FirstAddress - Supplies a pointer to the left side of the comparison.

    SecondAddress - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

NET_API
KSTATUS
NetAllocateBuffer (
    ULONG HeaderSize,
    ULONG Size,
    ULONG FooterSize,
    PNET_LINK Link,
    ULONG Flags,
    PNET_PACKET_BUFFER *NewBuffer
    );

/*++

Routine Description:

    This routine allocates a network buffer.

Arguments:

    HeaderSize - Supplies the number of header bytes needed.

    Size - Supplies the number of data bytes needed.

    FooterSize - Supplies the number of footer bytes needed.

    Link - Supplies a pointer to the link the buffer will be sent through. If
        a link is provided, then the buffer will be backed by physically
        contiguous pages for the link's hardware. If no link is provided, then
        the buffer will not be backed by physically contiguous pages.

    Flags - Supplies a bitmask of allocation flags. See
        NET_ALLOCATE_BUFFER_FLAG_* for definitions.

    NewBuffer - Supplies a pointer where a pointer to the new allocation will be
        returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if a zero length buffer was requested.

    STATUS_INSUFFICIENT_RESOURCES if the buffer or any auxiliary structures
        could not be allocated.

--*/

NET_API
VOID
NetFreeBuffer (
    PNET_PACKET_BUFFER Buffer
    );

/*++

Routine Description:

    This routine frees a previously allocated network buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer returned by the allocation
        routine.

Return Value:

    None.

--*/

NET_API
VOID
NetDestroyBufferList (
    PNET_PACKET_LIST BufferList
    );

/*++

Routine Description:

    This routine destroys a list of network packet buffers, releasing all of
    its associated resources, not including the buffer list structure.

Arguments:

    BufferList - Supplies a pointer to the buffer list to be destroyed.

Return Value:

    None.

--*/

//
// Link-specific definitions.
//

NET_API
BOOL
NetIsEthernetAddressValid (
    BYTE Address[ETHERNET_ADDRESS_SIZE]
    );

/*++

Routine Description:

    This routine determines if the given ethernet address is a valid individual
    address or not. This routine returns FALSE for 00:00:00:00:00:00 and
    FF:FF:FF:FF:FF:FF, and TRUE for everything else.

Arguments:

    Address - Supplies the address to check.

Return Value:

    TRUE if the ethernet address is a valid individual address.

    FALSE if the address is not valid.

--*/

NET_API
VOID
NetCreateEthernetAddress (
    BYTE Address[ETHERNET_ADDRESS_SIZE]
    );

/*++

Routine Description:

    This routine generates a random ethernet address.

Arguments:

    Address - Supplies the array where the new address will be stored.

Return Value:

    None.

--*/

