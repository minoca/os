/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    igmp.c

Abstract:

    This module implements the Internet Group Management Protocol (IGMP), which
    is used to support IPv4 multicast.

Author:

    Chris Stevens 23-Feb-2017

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
#include <minoca/net/ip4.h>
#include <minoca/net/igmp.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts IGMPv3 time codes to an actual time value. The time
// units depend on the supplied code being converted.
//

#define IGMP_CONVERT_TIME_CODE_TO_TIME(_ResponseCode) \
    (((_ResponseCode) < 128) ?                        \
     (_ResponseCode) :                                \
     ((((_ResponseCode) & 0x0F) | 0x10) <<            \
      ((((_ResponseCode) >> 4) & 0x07) + 3)))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used by the UDP socket protocol.
//

#define IGMP_PROTOCOL_ALLOCATION_TAG 0x706d6749 // 'pmgI'

//
// Define the size of an IGMP IPv4 header. Each packet should include the
// router alter option.
//

#define IGMP_IP4_HEADER_SIZE (sizeof(IP4_HEADER) + sizeof(ULONG))

//
// Store the 32-bit IPv4 router alert option sent with each IGMP packet.
//

#define IGMP_IP4_ROUTER_ALERT_OPTION CPU_TO_NETWORK32(0x94040000)

//
// Define the conversion between query response time units (1/10th of a second)
// and microseconds.
//

#define IGMP_MICROSECONDS_PER_QUERY_TIME_UNIT \
    (100 * MICROSECONDS_PER_MILLISECOND)

//
// Define the default max response code for version 1 query messages.
//

#define IGMP_QUERY_V1_MAX_RESPONSE_CODE 100

//
// Define the maximum number of group records that can be included in each
// report.
//

#define IGMP_MAX_GROUP_RECORD_COUNT MAX_USHORT

//
// Define the source IPv4 address for all IGMP general query messages -
// 224.0.0.1.
//

#define IGMP_ALL_SYSTEMS_ADDRESS CPU_TO_NETWORK32(0xE0000001)

//
// Define the IPv4 address to which all IGMPv2 leave messages are sent.
//

#define IGMP_ALL_ROUTERS_ADDRESS CPU_TO_NETWORK32(0xE0000002)

//
// Define the IPv4 addres to which all IGMPv3 report messages are sent.
//

#define IGMP_ALL_ROUTERS_ADDRESS_V3 CPU_TO_NETWORK32(0xE0000016)

//
// Define the IGMP message types.
//

#define IGMP_MESSAGE_TYPE_QUERY 0x11
#define IGMP_MESSAGE_TYPE_REPORT_V1 0x12
#define IGMP_MESSAGE_TYPE_REPORT_V2 0x16
#define IGMP_MESSAGE_TYPE_LEAVE_V2 0x17
#define IGMP_MESSAGE_TYPE_REPORT_V3 0x22

//
// Define the IGMP group record types.
//

#define IGMP_GROUP_RECORD_TYPE_MODE_IS_INCLUDE 1
#define IGMP_GROUP_RECORD_TYPE_MODE_IS_EXCLUDE 2
#define IGMP_GROUP_RECORD_TYPE_CHANGE_TO_INCLUDE_MODE 3
#define IGMP_GROUP_RECORD_TYPE_CHANGE_TO_EXCLUDE_MODE 4
#define IGMP_GROUP_RECORD_TYPE_ALLOW_NEW_SOURCES 5
#define IGMP_GROUP_RECORD_TYPE_BLOCK_OLD_SOURCES 6

//
// Define the IGMPv3 query message flag bits.
//

#define IGMP_QUERY_FLAG_SUPPRESS_ROUTER_PROCESSING 0x08
#define IGMP_QUERY_FLAG_ROBUSTNESS_MASK            0x07
#define IGMP_QUERY_FLAG_ROBUSTNESS_SHIFT           0

//
// Define the required number of compatibility modes.
//

#define IGMP_COMPATIBILITY_MODE_COUNT 2

//
// Define the default robustness variable.
//

#define IGMP_DEFAULT_ROBUSTNESS_VARIABLE 2

//
// Define the default query interval, in seconds.
//

#define IGMP_DEFAULT_QUERY_INTERVAL 125

//
// Define the default query response interval, in 1/10 of a second units.
//

#define IGMP_DEFAULT_MAX_RESPONSE_TIME 100

//
// Define the default timeout, in seconds, to wait in the presence of a querier
// with an older version.
//

#define IGMP_DEFAULT_COMPATIBILITY_TIMEOUT 400

//
// Define the default unsolicited report interval in 1/10 of a second units.
//

#define IGMP_DEFAULT_UNSOLICITED_REPORT_INTERVAL 10

//
// Define the set of multicast group flags.
//

#define IGMP_MULTICAST_GROUP_FLAG_LAST_REPORT  0x00000001
#define IGMP_MULTICAST_GROUP_FLAG_STATE_CHANGE 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _IGMP_VERSION {
    IgmpVersion1,
    IgmpVersion2,
    IgmpVersion3
} IGMP_VERSION, *PIGMP_VERSION;

/*++

Structure Description:

    This structure define the header common to all IGMP packets.

Members:

    Type - Stores the IGMP message type.

    MaxResponseCode - Stores the encoded maximum response time for query
        messages.

    Checksum - Stores the 16 bit one's complement of the one's complement
        sum of all 16 bit words in the IGMP message payload.

--*/

typedef struct _IGMP_HEADER {
    UCHAR Type;
    UCHAR MaxResponseCode;
    USHORT Checksum;
} PACKED IGMP_HEADER, *PIGMP_HEADER;

/*++

Structure Description:

    This structure defines a generic IGMP message. It is the same structure for
    IGMPv1 and IGMPv2 queries, reports, and leave messages.

Members:

    Header - Stores the common IGMP message header.

    GroupAddress - Stores the IPv4 address of the group being queried,
        reported or left.

--*/

typedef struct _IGMP_MESSAGE {
    IGMP_HEADER Header;
    ULONG GroupAddress;
} PACKED IGMP_MESSAGE, *PIGMP_MESSAGE;

/*++

Structure Description:

    This structure defines a IGMPv3 query message. At the end of the structure
    is an array of source IPv4 addresses.

Members:

    Message - Stores the common IGMP message that starts the IGMPv3 query.

    Flags - Stores a bitmask of IGMPv3 query flags. See IGMP_QUERY_FLAG_* for
        definitions.

    QueryIntervalCode - Stores the encoded query interval of the router.

    SourceAddressCount - Stores the number of source address entries that
        immediately follow this structure.

--*/

typedef struct _IGMP_QUERY_V3 {
    IGMP_MESSAGE Message;
    UCHAR Flags;
    UCHAR QueryIntervalCode;
    USHORT SourceAddressCount;
} PACKED IGMP_QUERY_V3, *PIGMP_QUERY_V3;

/*++

Structure Description:

    This structure defines an IGMPv3 group record.

Members:

    Type - Stores the group record type.

    DataLength - Stores the length of auxiliary data that starts at the end of
        the source address array.

    SourceAddressCount - Stores the number of source address entries in the
        array that starts at the end of this structure.

    MulticastAddress - Stores the multicast address of the group.

--*/

typedef struct _IGMP_GROUP_RECORD_V3 {
    UCHAR Type;
    UCHAR DataLength;
    USHORT SourceAddressCount;
    ULONG MulticastAddress;
} PACKED IGMP_GROUP_RECORD_V3, *PIGMP_GROUP_RECORD_V3;

/*++

Structure Description:

    This structure defines the IGMPv3 report message.

Members:

    Header - Stores the common IGMP header.

    Reserved - Stores two reserved bytes.

    GroupRecordCount - Stores the number of group records stored in the array
        that begins immediately after this structure.

--*/

typedef struct _IGMP_REPORT_V3 {
    IGMP_HEADER Header;
    USHORT Reserved;
    USHORT GroupRecordCount;
} PACKED IGMP_REPORT_V3, *PIGMP_REPORT_V3;

/*++

Structure Description:

    This structure defines an generic IGMP timer that kicks off a DPC, which
    then queues a work item.

Members:

    Timer - Stores a pointer to the internal timer.

    Dpc - Stores a pointer to the DPC that executes when the timer expires.

    WorkItem - Stores a pointer to the work item that is scheduled by the DPC.

--*/

typedef struct _IGMP_TIMER {
    PKTIMER Timer;
    PDPC Dpc;
    PWORK_ITEM WorkItem;
} IGMP_TIMER, *PIGMP_TIMER;

/*++

Structure Description:

    This structure defines an IGMP link.

Members:

    Node - Stores the link's entry into the global tree of IGMP links.

    ReferenceCount - Stores the reference count on the structure.

    Link - Stores a pointer to the network link to which this IGMP link is
        bound.

    LinkAddress - Stores a pointer to the network link address entry with which
        the IGMP link is associated.

    MaxPacketSize - Stores the maximum IGMP packet size that can be sent over
        the link.

    RobustnessVariable - Stores the multicast router's robustness variable.

    QueryInterval - Stores the multicat router's query interval, in seconds.

    MaxResponseTime - Stores the maximum response time for a IGMP report, in
        units of 1/10 seconds.

    Lock - Stores a queued lock that protects the IGMP link.

    CompatibilityMode - Stores the current compatibility mode of the IGMP link.
        This is based on the type of query messages received on the network.

    CompatibilityTimer - Stores an array of timers for each of the older
        versions of IGMP that must be supported.

    ReportTimer - Stores the report timer used for responding to generic
        queries.

    MulticastGroupCount - Stores the number of multicast groups that are
        associated with the link.

    MulticastGroupList - Stores the list of the multicast group structures
        associated with the link.

--*/

typedef struct _IGMP_LINK {
    RED_BLACK_TREE_NODE Node;
    volatile ULONG ReferenceCount;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    ULONG MaxPacketSize;
    ULONG RobustnessVariable;
    ULONG QueryInterval;
    ULONG MaxResponseTime;
    PQUEUED_LOCK Lock;
    volatile IGMP_VERSION CompatibilityMode;
    IGMP_TIMER CompatibilityTimer[IGMP_COMPATIBILITY_MODE_COUNT];
    IGMP_TIMER ReportTimer;
    ULONG MulticastGroupCount;
    LIST_ENTRY MulticastGroupList;
} IGMP_LINK, *PIGMP_LINK;

/*++

Structure Description:

    This structure defines an IGMP multicast group.

Members:

    ListEntry - Stores the group's entry into its parent's list of multicast
        groups.

    ReferenceCount - Stores the reference count on the structure.

    SendCount - Stores the number of pending report or leave messages to be
        sending. This number should always be less than or equal to the
        robustness value.

    Flags - Stores a bitmask of multicast group flags. See
        IGMP_MULTICAST_GROUP_FLAG_* for definitions.

    JoinCount - Stores the number of times a join request has been made for
        this multicast group. This is protected by the IGMP link's queued lock.

    Address - Stores the IPv4 multicast address of the group.

    IgmpLink - Stores a pointer to the IGMP link to which this group belongs.

    Timer - Stores the timer used to schedule delayed and repeated IGMP report
        and leave messages.

--*/

typedef struct _IGMP_MULTICAST_GROUP {
    LIST_ENTRY ListEntry;
    volatile ULONG ReferenceCount;
    volatile ULONG SendCount;
    volatile ULONG Flags;
    ULONG JoinCount;
    ULONG Address;
    PIGMP_LINK IgmpLink;
    IGMP_TIMER Timer;
} IGMP_MULTICAST_GROUP, *PIGMP_MULTICAST_GROUP;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpIgmpCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    );

VOID
NetpIgmpDestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIgmpBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpIgmpListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIgmpAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    );

KSTATUS
NetpIgmpConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpIgmpClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIgmpShutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    );

KSTATUS
NetpIgmpSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

VOID
NetpIgmpProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpIgmpProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpIgmpReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetpIgmpGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetpIgmpUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

KSTATUS
NetpIgmpJoinMulticastGroup (
    PSOCKET_IGMP_MULTICAST_REQUEST Request
    );

KSTATUS
NetpIgmpLeaveMulticastGroup (
    PSOCKET_IGMP_MULTICAST_REQUEST Request
    );

VOID
NetpIgmpProcessQuery (
    PIGMP_LINK Link,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

VOID
NetpIgmpProcessReport (
    PIGMP_LINK Link,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

VOID
NetpIgmpQueueReportTimer (
    PIGMP_TIMER ReportTimer,
    ULONGLONG StartTime,
    ULONG MaxResponseTime
    );

VOID
NetpIgmpTimerDpcRoutine (
    PDPC Dpc
    );

VOID
NetpIgmpGroupTimeoutWorker (
    PVOID Parameter
    );

VOID
NetpIgmpLinkReportTimeoutWorker (
    PVOID Parameter
    );

VOID
NetpIgmpLinkCompatibilityTimeoutWorker (
    PVOID Parameter
    );

VOID
NetpIgmpLinkCompatibilityTimeoutWorker (
    PVOID Parameter
    );

VOID
NetpIgmpQueueCompatibilityTimer (
    PIGMP_LINK IgmpLink,
    IGMP_VERSION CompatibilityMode
    );

VOID
NetpIgmpUpdateCompatibilityMode (
    PIGMP_LINK IgmpLink
    );

VOID
NetpIgmpSendGroupReport (
    PIGMP_MULTICAST_GROUP Group
    );

VOID
NetpIgmpSendLinkReport (
    PIGMP_LINK Link
    );

VOID
NetpIgmpSendGroupLeave (
    PIGMP_MULTICAST_GROUP Group
    );

VOID
NetpIgmpSendPackets (
    PIGMP_LINK IgmpLink,
    PNETWORK_ADDRESS Destination,
    PNET_PACKET_LIST PacketList
    );

KSTATUS
NetpIgmpUpdateAddressFilters (
    PIGMP_LINK IgmpLink
    );

PIGMP_LINK
NetpIgmpCreateOrLookupLink (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

VOID
NetpIgmpDestroyLink (
    PIGMP_LINK IgmpLink
    );

PIGMP_LINK
NetpIgmpLookupLink (
    PNET_LINK Link
    );

VOID
NetpIgmpLinkAddReference (
    PIGMP_LINK IgmpLink
    );

VOID
NetpIgmpLinkReleaseReference (
    PIGMP_LINK IgmpLink
    );

COMPARISON_RESULT
NetpIgmpCompareLinkEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

PIGMP_MULTICAST_GROUP
NetpIgmpCreateGroup (
    PIGMP_LINK IgmpLink,
    ULONG GroupAddress
    );

VOID
NetpIgmpDestroyGroup (
    PIGMP_MULTICAST_GROUP Group
    );

VOID
NetpIgmpGroupAddReference (
    PIGMP_MULTICAST_GROUP Group
    );

VOID
NetpIgmpGroupReleaseReference (
    PIGMP_MULTICAST_GROUP Group
    );

KSTATUS
NetpIgmpInitializeTimer (
    PIGMP_TIMER Timer,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID WorkParameter
    );

VOID
NetpIgmpDestroyTimer (
    PIGMP_TIMER Timer
    );

USHORT
NetpIgmpChecksumData (
    PVOID Data,
    ULONG Length
    );

//
// -------------------------------------------------------------------- Globals
//

NET_PROTOCOL_ENTRY NetIgmpProtocol = {
    {NULL, NULL},
    NetSocketDatagram,
    SOCKET_INTERNET_PROTOCOL_IGMP,
    0,
    NULL,
    NULL,
    {{0}, {0}, {0}},
    {
        NetpIgmpCreateSocket,
        NetpIgmpDestroySocket,
        NetpIgmpBindToAddress,
        NetpIgmpListen,
        NetpIgmpAccept,
        NetpIgmpConnect,
        NetpIgmpClose,
        NetpIgmpShutdown,
        NetpIgmpSend,
        NetpIgmpProcessReceivedData,
        NetpIgmpProcessReceivedSocketData,
        NetpIgmpReceive,
        NetpIgmpGetSetInformation,
        NetpIgmpUserControl
    }
};

//
// Stores a global tree of net links that are signed up for multicast groups
// via IGMP.
//

RED_BLACK_TREE NetIgmpLinkTree;
PSHARED_EXCLUSIVE_LOCK NetIgmpLinkLock;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpIgmpInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for the IGMP protocol.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    RtlRedBlackTreeInitialize(&NetIgmpLinkTree, 0, NetpIgmpCompareLinkEntries);
    NetIgmpLinkLock = KeCreateSharedExclusiveLock();
    if (NetIgmpLinkLock == NULL) {

        ASSERT(FALSE);

        return;
    }

    //
    // Register the IGMP socket handlers with the core networking library.
    //

    Status = NetRegisterProtocol(&NetIgmpProtocol, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
}

KSTATUS
NetpIgmpCreateSocket (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

VOID
NetpIgmpDestroySocket (
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

    return;
}

KSTATUS
NetpIgmpBindToAddress (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIgmpListen (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIgmpAccept (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIgmpConnect (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIgmpClose (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIgmpShutdown (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIgmpSend (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

VOID
NetpIgmpProcessReceivedData (
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

    USHORT ComputedChecksum;
    PIGMP_HEADER Header;
    PIGMP_LINK IgmpLink;
    ULONG Length;
    PNET_PACKET_BUFFER Packet;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Do nothing if this link is not registered with IGMP. The packet is
    // likely old.
    //

    IgmpLink = NetpIgmpLookupLink(ReceiveContext->Link);
    if (IgmpLink == NULL) {
        goto IgmpProcessReceivedDataEnd;
    }

    //
    // Make sure there is at least the common header to read.
    //

    Packet = ReceiveContext->Packet;
    Header = (PIGMP_HEADER)(Packet->Buffer + Packet->DataOffset);
    Length = Packet->FooterOffset - Packet->DataOffset;
    if (Length < sizeof(IGMP_HEADER)) {
        RtlDebugPrint("IGMP: Invalid length of %d. Expected at least %d "
                      "bytes.\n",
                      Length,
                      sizeof(IGMP_HEADER));

        goto IgmpProcessReceivedDataEnd;
    }

    //
    // Validate the IGMP checksum.
    //

    ComputedChecksum = NetpIgmpChecksumData((PVOID)Header, Length);
    if (ComputedChecksum != 0) {
        RtlDebugPrint("IGMP: Invalid checksum. Computed checksum: 0x%04x, "
                      "should have been zero.\n",
                      ComputedChecksum);

        goto IgmpProcessReceivedDataEnd;
    }

    //
    // Handle the IGMP packet based on the type field.
    //

    switch (Header->Type) {
    case IGMP_MESSAGE_TYPE_QUERY:
        NetpIgmpProcessQuery(IgmpLink,
                             Packet,
                             ReceiveContext->Source,
                             ReceiveContext->Destination);

        break;

    case IGMP_MESSAGE_TYPE_REPORT_V1:
    case IGMP_MESSAGE_TYPE_REPORT_V2:
        NetpIgmpProcessReport(IgmpLink,
                              Packet,
                              ReceiveContext->Source,
                              ReceiveContext->Destination);

        break;

    //
    // IGMPv3 reports are ignored.
    //

    case IGMP_MESSAGE_TYPE_REPORT_V3:
        break;

    //
    // A leave message should only be handled by a router.
    //

    case IGMP_MESSAGE_TYPE_LEAVE_V2:
        break;

    default:
        break;
    }

IgmpProcessReceivedDataEnd:
    if (IgmpLink != NULL) {
        NetpIgmpLinkReleaseReference(IgmpLink);
    }

    return;
}

KSTATUS
NetpIgmpProcessReceivedSocketData (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIgmpReceive (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIgmpGetSetInformation (
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

    SOCKET_IGMP_OPTION IgmpOption;
    PSOCKET_IGMP_MULTICAST_REQUEST MulticastRequest;
    UINTN RequiredSize;
    PVOID Source;
    KSTATUS Status;

    if (InformationType != SocketInformationIgmp) {
        Status = STATUS_INVALID_PARAMETER;
        goto IgmpGetSetInformationEnd;
    }

    RequiredSize = 0;
    Source = NULL;
    Status = STATUS_SUCCESS;
    IgmpOption = (SOCKET_IGMP_OPTION)Option;
    switch (IgmpOption) {
    case SocketIgmpOptionJoinMulticastGroup:
    case SocketIgmpOptionLeaveMulticastGroup:
        if (Set == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

        RequiredSize = sizeof(SOCKET_IGMP_MULTICAST_REQUEST);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        MulticastRequest = (PSOCKET_IGMP_MULTICAST_REQUEST)Data;
        if (!IP4_IS_MULTICAST_ADDRESS(MulticastRequest->MulticastAddress)) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (IgmpOption == SocketIgmpOptionJoinMulticastGroup) {
            Status = NetpIgmpJoinMulticastGroup(MulticastRequest);

        } else {
            Status = NetpIgmpLeaveMulticastGroup(MulticastRequest);
        }

        break;

    default:
        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto IgmpGetSetInformationEnd;
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
            goto IgmpGetSetInformationEnd;
        }
    }

IgmpGetSetInformationEnd:
    return Status;
}

KSTATUS
NetpIgmpUserControl (
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

KSTATUS
NetpIgmpJoinMulticastGroup (
    PSOCKET_IGMP_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine joins the multicast group on the network link provided in the
    request. If this is the first request to join the supplied multicast group
    on the specified link, then an IGMP report is sent out over the network
    and the hardware is reprogrammed to include messages to the multicast
    group's address.

Arguments:

    Request - Supplies a pointer to a multicast request, which includes the
        address of the group to join and the network link on which to join the
        group.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PIGMP_MULTICAST_GROUP Group;
    PIGMP_LINK IgmpLink;
    BOOL LinkLockHeld;
    PIGMP_MULTICAST_GROUP NewGroup;
    KSTATUS Status;

    LinkLockHeld = FALSE;
    NewGroup = NULL;

    //
    // Test to see if there is an IGMP link for the given network link,
    // creating one if the lookup fails.
    //

    IgmpLink = NetpIgmpLookupLink(Request->Link);
    if (IgmpLink == NULL) {
        IgmpLink = NetpIgmpCreateOrLookupLink(Request->Link,
                                              Request->LinkAddress);

        if (IgmpLink == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto JoinMulticastGroupEnd;
        }
    }

    //
    // Search the IGMP link for the multicast group. If a matching group is not
    // found then release the lock, allocate a group and search again. If the
    // group is still not found, add the newly allocated group.
    //

    NewGroup = NULL;
    Status = STATUS_NOT_FOUND;
    while (TRUE) {
        KeAcquireQueuedLock(IgmpLink->Lock);
        LinkLockHeld = TRUE;
        CurrentEntry = IgmpLink->MulticastGroupList.Next;
        while (CurrentEntry != &(IgmpLink->MulticastGroupList)) {
            Group = LIST_VALUE(CurrentEntry, IGMP_MULTICAST_GROUP, ListEntry);
            if (Group->Address == Request->MulticastAddress) {
                Status = STATUS_SUCCESS;
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (!KSUCCESS(Status)) {
            if (NewGroup == NULL) {
                KeReleaseQueuedLock(IgmpLink->Lock);
                LinkLockHeld = FALSE;
                NewGroup = NetpIgmpCreateGroup(IgmpLink,
                                               Request->MulticastAddress);

                if (NewGroup == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto JoinMulticastGroupEnd;
                }

                continue;
            }

            //
            // Add the newly allocate group to the link's list.
            //

            INSERT_BEFORE(&(NewGroup->ListEntry),
                          &(IgmpLink->MulticastGroupList));

            IgmpLink->MulticastGroupCount += 1;
            Group = NewGroup;
        }

        break;
    }

    Status = STATUS_SUCCESS;

    //
    // If the group was found and it had been previously joined, then the
    // multicast membership has already been reported and the hardware has
    // already been programmed.
    //

    Group->JoinCount += 1;
    if (Group->JoinCount > 1) {

        ASSERT(Group != NewGroup);

        goto JoinMulticastGroupEnd;
    }

    ASSERT(Group == NewGroup);

    //
    // Otherwise the hardware filters needs to be updated and a membership
    // report needs to be sent. The filters are updated with the lock held as
    // the each group's address needs to be sent to the hardware. This also
    // makes it necessary to have the new group already on the link. It would
    // also be bad to have a second join call run through before the hardware
    // is initialized.
    //

    Status = NetpIgmpUpdateAddressFilters(IgmpLink);
    if (!KSUCCESS(Status)) {
        Group->JoinCount = 0;
        LIST_REMOVE(&(Group->ListEntry));
        IgmpLink->MulticastGroupCount -= 1;
        goto JoinMulticastGroupEnd;
    }

    NewGroup = NULL;
    KeReleaseQueuedLock(IgmpLink->Lock);
    LinkLockHeld = FALSE;
    RtlAtomicOr32(&(Group->Flags), IGMP_MULTICAST_GROUP_FLAG_STATE_CHANGE);
    RtlAtomicExchange32(&(Group->SendCount), IgmpLink->RobustnessVariable);
    NetpIgmpSendGroupReport(Group);

JoinMulticastGroupEnd:
    if (LinkLockHeld != FALSE) {
        KeReleaseQueuedLock(IgmpLink->Lock);
    }

    if (IgmpLink != NULL) {
        NetpIgmpLinkReleaseReference(IgmpLink);
    }

    if (NewGroup != NULL) {
        NetpIgmpDestroyGroup(NewGroup);
    }

    return Status;
}

KSTATUS
NetpIgmpLeaveMulticastGroup (
    PSOCKET_IGMP_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine removes the local system from a multicast group. If this is
    the last request to leave a multicast group on the link, then a IGMP leave
    message is sent out over the network and the hardware is reprogrammed to
    filter out messages to the multicast group.

Arguments:

    Request - Supplies a pointer to a multicast request, which includes the
        address of the group to leave and the network link that has previously
        joined the group.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PIGMP_MULTICAST_GROUP Group;
    PIGMP_LINK IgmpLink;
    BOOL LinkLockHeld;
    BOOL LinkUp;
    KSTATUS Status;

    LinkLockHeld = FALSE;
    IgmpLink = NULL;

    //
    // Now see if there is an IGMP link for the given network link.
    //

    IgmpLink = NetpIgmpLookupLink(Request->Link);
    if (IgmpLink == NULL) {
        Status = STATUS_INVALID_ADDRESS;
        goto LeaveMulticastGroupEnd;
    }

    //
    // Search the IGMP link for the multicast group. If a matching group is not
    // found then the request fails.
    //

    Status = STATUS_INVALID_ADDRESS;
    KeAcquireQueuedLock(IgmpLink->Lock);
    LinkLockHeld = TRUE;
    CurrentEntry = IgmpLink->MulticastGroupList.Next;
    while (CurrentEntry != &(IgmpLink->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, IGMP_MULTICAST_GROUP, ListEntry);
        if (Group->Address == Request->MulticastAddress) {
            Status = STATUS_SUCCESS;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (!KSUCCESS(Status)) {
        goto LeaveMulticastGroupEnd;
    }

    //
    // If this is not the last reference on the group, the call is successful,
    // but takes no further action. The link as whole remains joined to the
    // multicast group.
    //

    if (Group->JoinCount > 1) {
        Group->JoinCount -= 1;
        goto LeaveMulticastGroupEnd;
    }

    //
    // Otherwise it's time for the group to go.
    //

    LIST_REMOVE(&(Group->ListEntry));
    IgmpLink->MulticastGroupCount -= 1;

    //
    // Now that the group is out of the list, update the filters.
    //

    Status = NetpIgmpUpdateAddressFilters(IgmpLink);
    if (!KSUCCESS(Status)) {
        INSERT_BEFORE(&(Group->ListEntry), &(IgmpLink->MulticastGroupList));
        IgmpLink->MulticastGroupCount += 1;
        goto LeaveMulticastGroupEnd;
    }

    //
    // Release the lock and flush out any reports that may be in the works.
    //

    KeReleaseQueuedLock(IgmpLink->Lock);
    LinkLockHeld = FALSE;
    KeCancelTimer(Group->Timer.Timer);
    KeFlushDpc(Group->Timer.Dpc);
    KeCancelWorkItem(Group->Timer.WorkItem);
    KeFlushWorkItem(Group->Timer.WorkItem);

    //
    // Now that the work item is flushed out. Officially mark that this group
    // is not joined. Otherwise the work item may prematurely send leave
    // messages.
    //

    ASSERT(Group->JoinCount == 1);

    Group->JoinCount = 0;

    //
    // If the link is up, start sending leave messages, up to the robustness
    // count. The group's initial reference will be released after the last
    // leave message is sent.
    //

    NetGetLinkState(IgmpLink->Link, &LinkUp, NULL);
    if (LinkUp != FALSE) {
        RtlAtomicOr32(&(Group->Flags), IGMP_MULTICAST_GROUP_FLAG_STATE_CHANGE);
        RtlAtomicExchange32(&(Group->SendCount), IgmpLink->RobustnessVariable);
        NetpIgmpSendGroupLeave(Group);

    //
    // Otherwise don't bother with the leave messages and just destroy the
    // group immediately.
    //

    } else {
        NetpIgmpGroupReleaseReference(Group);
    }

LeaveMulticastGroupEnd:
    if (LinkLockHeld != FALSE) {
        KeReleaseQueuedLock(IgmpLink->Lock);
    }

    if (IgmpLink != NULL) {
        NetpIgmpLinkReleaseReference(IgmpLink);
    }

    return Status;
}

VOID
NetpIgmpProcessQuery (
    PIGMP_LINK IgmpLink,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine processes an IGMP query message.

    In host mode, this generates a report for each multicast group to which the
    receiving link belongs.

    In router mode, a query message indicates that there is another multicast
    router on the local network. If this link has a higher IP address than the
    sender, this link will not send queries until the "other querier present
    interval" expires. Router mode is not currently supported.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link that received the packet.

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

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONGLONG CurrentTime;
    PIP4_ADDRESS Destination;
    PIGMP_MULTICAST_GROUP Group;
    ULONG Length;
    UCHAR MaxResponseCode;
    ULONG MaxResponseTime;
    PIGMP_MESSAGE Query;
    ULONG QueryInterval;
    PIGMP_QUERY_V3 QueryV3;
    ULONG RobustnessVariable;
    IGMP_VERSION Version;

    Destination = (PIP4_ADDRESS)DestinationAddress;

    //
    // Determine which version of query message was received. An 8 octet long
    // message with a max response code of 0 is an IGMPv1 query message. An 8
    // octet long message with a non-zero max response code is an IGMPv2 query
    // message. A message with a length greater than or equal to 12 octets is
    // an IGMPv3 query message. Any other message must be ignored.
    //

    Query = (PIGMP_MESSAGE)(Packet->Buffer + Packet->DataOffset);
    Length = Packet->FooterOffset - Packet->DataOffset;
    MaxResponseCode = Query->Header.MaxResponseCode;
    Version = IgmpVersion3;
    if (Length == sizeof(IGMP_MESSAGE)) {
        if (MaxResponseCode == 0) {
            Version = IgmpVersion1;
            MaxResponseCode = IGMP_QUERY_V1_MAX_RESPONSE_CODE;

        } else {
            Version = IgmpVersion2;
        }

        NetpIgmpQueueCompatibilityTimer(IgmpLink, Version);

    } else if (Length >= sizeof(IGMP_QUERY_V3)) {
        QueryV3 = (PIGMP_QUERY_V3)Query;
        QueryInterval = IGMP_CONVERT_TIME_CODE_TO_TIME(
                                                   QueryV3->QueryIntervalCode);

        RobustnessVariable =
                         (QueryV3->Flags >> IGMP_QUERY_FLAG_ROBUSTNESS_SHIFT) &
                         IGMP_QUERY_FLAG_ROBUSTNESS_MASK;

        //
        // Update the query interval and robustness variable if they are
        // non-zero.
        //

        if (QueryInterval != 0) {
            IgmpLink->QueryInterval = QueryInterval;
        }

        if (RobustnessVariable != 0) {
            IgmpLink->RobustnessVariable = RobustnessVariable;
        }

    } else {
        return;
    }

    //
    // Version 2 and 3 queries without the router-alert option should be
    // ignored for security reasons - theoretically helps to detect forged
    // queries from outside the local network.
    //

    if ((Version == IgmpVersion3) || (Version == IgmpVersion2)) {

        //
        // TODO: IGMP needs to get the IPv4 options.
        //

    }

    //
    // All general queries not sent to the all-systems address (224.0.0.1)
    // should be ignored for security reasons - the same forged query
    // detection discussed above.
    //

    if ((Query->GroupAddress == 0) &&
        (Destination->Address != IGMP_ALL_SYSTEMS_ADDRESS)) {

        return;
    }

    //
    // Ignore queries that target the all systems address. No reports are
    // supposed to be sent for the all systems address, making a query quite
    // mysterious.
    //

    if (Query->GroupAddress == IGMP_ALL_SYSTEMS_ADDRESS) {
        return;
    }

    //
    // Calculate the maximum response time. For query messages, the time unit
    // is 1/10th of a second.
    //

    MaxResponseTime = IGMP_CONVERT_TIME_CODE_TO_TIME(MaxResponseCode);

    //
    // The reports are not sent immediately, but delayed based on the max
    // response code.
    //

    KeAcquireQueuedLock(IgmpLink->Lock);

    //
    // Always save the max response time.
    //

    IgmpLink->MaxResponseTime = MaxResponseTime;

    //
    // If the host is operating in IGMPv3 mode and this is a general query, set
    // the global report timer. IGMPv3 can send one report that includes
    // information for all of the host's multicast memberships.
    //

    CurrentTime = KeGetRecentTimeCounter();
    if ((IgmpLink->CompatibilityMode == IgmpVersion3) &&
        (Query->GroupAddress == 0)) {

        NetpIgmpQueueReportTimer(&(IgmpLink->ReportTimer),
                                 CurrentTime,
                                 MaxResponseTime);

    //
    // Otherwise, iterate over the list of multicast groups to which this
    // link subscribes and update the timer for each group that matches the
    // query's group address - or all groups if it is a generic query.
    //

    } else {
        CurrentEntry = IgmpLink->MulticastGroupList.Next;
        while (CurrentEntry != &(IgmpLink->MulticastGroupList)) {
            Group = LIST_VALUE(CurrentEntry, IGMP_MULTICAST_GROUP, ListEntry);
            if ((Query->GroupAddress == 0) ||
                (Query->GroupAddress == Group->Address)) {

                RtlAtomicAnd32(&(Group->Flags),
                               ~IGMP_MULTICAST_GROUP_FLAG_STATE_CHANGE);

                RtlAtomicCompareExchange(&(Group->SendCount), 1, 0);
                NetpIgmpQueueReportTimer(&(Group->Timer),
                                         CurrentTime,
                                         MaxResponseTime);
            }

            CurrentEntry = CurrentEntry->Next;
        }
    }

    KeReleaseQueuedLock(IgmpLink->Lock);
    return;
}

VOID
NetpIgmpProcessReport (
    PIGMP_LINK IgmpLink,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine processes an IGMP report message.

    In host mode, this cancels any pending report messages for the reported
    multicast group. A router only needs to receive one report per multicast
    group on the local physical network. It does not need to know which
    specific hosts are subcribed to a group, just that at least one host is
    subscribed to a group.

    In router mode, a report should enabling forwarding packets detined for the
    reported multicast group. Router mode is not currently supported.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link that received the packet.

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

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PIP4_ADDRESS Destination;
    PIGMP_MULTICAST_GROUP Group;
    ULONG Length;
    PIP4_ADDRESS LocalAddress;
    PIGMP_MESSAGE Report;
    PIP4_ADDRESS Source;
    PIP4_ADDRESS SubnetAddress;

    //
    // IGMPv3 reports are always ignored. The size of the report must be 8
    // octets.
    //

    Report = (PIGMP_MESSAGE)(Packet->Buffer + Packet->DataOffset);
    Length = Packet->FooterOffset - Packet->DataOffset;
    if (Length != sizeof(IGMP_MESSAGE)) {
        return;
    }

    //
    // Reports from the any address must be accepted, otherwise the source must
    // be from the local subnet.
    //

    Source = (PIP4_ADDRESS)SourceAddress;
    if (Source->Address != 0) {
        SubnetAddress = (PIP4_ADDRESS)&(IgmpLink->LinkAddress->Subnet);
        LocalAddress = (PIP4_ADDRESS)&(IgmpLink->LinkAddress->Address);
        if ((LocalAddress->Address & SubnetAddress->Address) !=
            (Source->Address & SubnetAddress->Address)) {

            RtlDebugPrint("IGMP: Ignoring report from: \n");
            NetDebugPrintAddress((PNETWORK_ADDRESS)SourceAddress);
            RtlDebugPrint("IGMP: It is not in the local network of: \n");
            NetDebugPrintAddress((PNETWORK_ADDRESS)LocalAddress);
            RtlDebugPrint("IGMP: Subnet mask is: \n");
            NetDebugPrintAddress((PNETWORK_ADDRESS)SubnetAddress);
            return;
        }
    }

    //
    // Version 2 reports without the router-alert option should be ignored for
    // security reasons - theoretically helps to detect forged queries from
    // outside the local network.
    //

    if (Report->Header.Type == IGMP_MESSAGE_TYPE_REPORT_V2) {

        //
        // TODO: IGMP needs to get the IPv4 options.
        //

    }

    //
    // The report should have been sent to the multicast group it was reporting
    // on.
    //

    Destination = (PIP4_ADDRESS)DestinationAddress;
    if ((Destination->Address != Report->GroupAddress) ||
        (Destination->Address == 0)) {

        return;
    }

    //
    // If this IGMP link belongs to the multicast group, cancel any pending
    // reports and record that this link was not the last to send a report.
    //

    KeAcquireQueuedLock(IgmpLink->Lock);
    CurrentEntry = IgmpLink->MulticastGroupList.Next;
    while (CurrentEntry != &(IgmpLink->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, IGMP_MULTICAST_GROUP, ListEntry);
        if (Report->GroupAddress == Group->Address) {
            KeCancelTimer(Group->Timer.Timer);
            RtlAtomicAnd32(&(Group->Flags),
                           ~IGMP_MULTICAST_GROUP_FLAG_LAST_REPORT);

            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseQueuedLock(IgmpLink->Lock);
    return;
}

VOID
NetpIgmpQueueReportTimer (
    PIGMP_TIMER ReportTimer,
    ULONGLONG StartTime,
    ULONG MaxResponseTime
    )

/*++

Routine Description:

    This routine queues the given report timer to expire between 0 and the
    maximum delay time from the given start time.

Arguments:

    ReportTimer - Supplies a pointer to the report timer that needs to be
        queued.

    StartTime - Supplies the starting time to which the calculated delay will
        be added. This should be in timer ticks.

    MaxResponseTime - Supplies the maximum responce time supplied by the IGMP
        query that prompted the report. It is in 1/10th of a second units.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentDueTime;
    ULONG Delay;
    ULONGLONG DelayInMicroseconds;
    ULONGLONG DueTime;
    KSTATUS Status;

    //
    // The random delay is selected from the range (0, MaxResponseTime].
    //

    KeGetRandomBytes(&Delay, sizeof(Delay));
    Delay = (Delay % MaxResponseTime) + 1;
    DelayInMicroseconds = Delay * IGMP_MICROSECONDS_PER_QUERY_TIME_UNIT;
    DueTime = StartTime + KeConvertMicrosecondsToTimeTicks(DelayInMicroseconds);
    CurrentDueTime = KeGetTimerDueTime(ReportTimer->Timer);

    //
    // If the current due time is non-zero and less than the due time, do
    // nothing. The report is already scheduled to be sent.
    //

    if ((CurrentDueTime != 0) && (CurrentDueTime <= DueTime)) {
        return;
    }

    //
    // Otherwise, cancel the timer and reschedule it for the earlier time. If
    // the cancel is too late, then the timer just went off and the report
    // will be sent. Do not reschedule the timer.
    //

    if (CurrentDueTime != 0) {
        Status = KeCancelTimer(ReportTimer->Timer);
        if (Status == STATUS_TOO_LATE) {
            return;
        }
    }

    KeQueueTimer(ReportTimer->Timer,
                 TimerQueueSoft,
                 DueTime,
                 0,
                 0,
                 ReportTimer->Dpc);

    return;
}

VOID
NetpIgmpTimerDpcRoutine (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the IGMP timer DPC that gets called after a timer
    expires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PIGMP_TIMER ReportTimer;

    ReportTimer = (PIGMP_TIMER)Dpc->UserData;
    KeQueueWorkItem(ReportTimer->WorkItem);
    return;
}

VOID
NetpIgmpGroupTimeoutWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs the low level work when an IGMP group report timer
    expires. It sends a report or leave message for the group.

Arguments:

    Parameter - Supplies a pointer to the IGMP group whose timer expired.

Return Value:

    None.

--*/

{

    PIGMP_MULTICAST_GROUP Group;

    //
    // If there are no more sockets joined to the group, then send leave
    // messages. The group will be destroyed after the last leave message, so
    // don't touch the group structure after the call to send a leave message.
    //

    Group = (PIGMP_MULTICAST_GROUP)Parameter;
    if (Group->JoinCount == 0) {
        NetpIgmpSendGroupLeave(Group);

    //
    // Otherwise the timer has expired to send a simple group report.
    //

    } else {
        NetpIgmpSendGroupReport(Group);
    }

    return;
}

VOID
NetpIgmpLinkReportTimeoutWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs the low level work when an IGMP link report timer
    expires. It sends a IGMPv3 report message for all groups.

Arguments:

    Parameter - Supplies a pointer to the IGMP link whose link report timer
        expired.

Return Value:

    None.

--*/

{

    PIGMP_LINK IgmpLink;

    IgmpLink = (PIGMP_LINK)Parameter;
    NetpIgmpSendLinkReport(IgmpLink);
    return;
}

VOID
NetpIgmpLinkCompatibilityTimeoutWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs the low level work when a compatibility mode timer
    expires. It determines the new compatability mode.

Arguments:

    Parameter - Supplies a pointer to the IGMP link whose compatibility timer
        expired.

Return Value:

    None.

--*/

{

    PIGMP_LINK IgmpLink;

    IgmpLink = (PIGMP_LINK)Parameter;
    KeAcquireQueuedLock(IgmpLink->Lock);
    NetpIgmpUpdateCompatibilityMode(IgmpLink);
    KeReleaseQueuedLock(IgmpLink->Lock);
    return;
}

VOID
NetpIgmpQueueCompatibilityTimer (
    PIGMP_LINK IgmpLink,
    IGMP_VERSION CompatibilityMode
    )

/*++

Routine Description:

    This routine queues an IGMP compatibility timer for the given mode.

Arguments:

    IgmpLink - Supplies a pointer to IGMP link whose compatibility timer needs
        to be set.

    CompatibilityMode - Supplies a pointer to the compatibility mode whose
        timer needs to be set.

Return Value:

    None.

--*/

{

    ULONGLONG CurrentDueTime;
    ULONG DelayInMicroseconds;
    ULONGLONG DueTime;
    ULONGLONG StartTime;
    PIGMP_TIMER Timer;

    //
    // The compatibility mode interval is calculated as follows:
    //
    // (Robustness Variable * Query Interval) + (Query Response Interval)
    //
    // The Query Response Interval is the same as the maximum response time
    // provided by the last query.
    //

    DelayInMicroseconds = IgmpLink->RobustnessVariable *
                          IgmpLink->QueryInterval *
                          MICROSECONDS_PER_SECOND;

    DelayInMicroseconds += IgmpLink->MaxResponseTime *
                           IGMP_MICROSECONDS_PER_QUERY_TIME_UNIT;

    Timer = &(IgmpLink->CompatibilityTimer[CompatibilityMode]);
    StartTime = KeGetRecentTimeCounter();
    DueTime = StartTime + KeConvertMicrosecondsToTimeTicks(DelayInMicroseconds);

    //
    // If the timer is already scheduled, then it needs to be extended for
    // another compatibility timeout interval. Cancel it and requeue it. It's
    // OK if the DPC fires the work item in the meantime. The correct mode will
    // be set once the lock can be acquired by the work item.
    //

    KeAcquireQueuedLock(IgmpLink->Lock);
    CurrentDueTime = KeGetTimerDueTime(Timer->Timer);
    if (CurrentDueTime != 0) {
        KeCancelTimer(Timer->Timer);
    }

    KeQueueTimer(Timer->Timer,
                 TimerQueueSoft,
                 DueTime,
                 0,
                 0,
                 Timer->Dpc);

    NetpIgmpUpdateCompatibilityMode(IgmpLink);
    KeReleaseQueuedLock(IgmpLink->Lock);
    return;
}

VOID
NetpIgmpUpdateCompatibilityMode (
    PIGMP_LINK IgmpLink
    )

/*++

Routine Description:

    This routine updates the given IGMP link's compatibility mode based on the
    state of the compatibility timers. It assumes the IGMP link's lock is held.

Arguments:

    IgmpLink - Supplies a pointer to an IGMP link.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONGLONG DueTime;
    PIGMP_MULTICAST_GROUP Group;
    IGMP_VERSION ModeIndex;
    IGMP_VERSION NewMode;
    PIGMP_TIMER Timer;

    ASSERT(KeIsQueuedLockHeld(IgmpLink->Lock) != FALSE);

    NewMode = IgmpVersion3;
    for (ModeIndex = IgmpVersion1;
         ModeIndex < IGMP_COMPATIBILITY_MODE_COUNT;
         ModeIndex += 1) {

        Timer = &(IgmpLink->CompatibilityTimer[ModeIndex]);
        DueTime = KeGetTimerDueTime(Timer->Timer);
        if (DueTime != 0) {
            NewMode = ModeIndex;
            break;
        }
    }

    //
    // If compatibility mode is about to change, cancel all pending timers.
    //

    if (NewMode != IgmpLink->CompatibilityMode) {
        KeCancelTimer(IgmpLink->ReportTimer.Timer);
        CurrentEntry = IgmpLink->MulticastGroupList.Next;
        while (CurrentEntry != &(IgmpLink->MulticastGroupList)) {
            Group = LIST_VALUE(CurrentEntry, IGMP_MULTICAST_GROUP, ListEntry);
            KeCancelTimer(Group->Timer.Timer);
            CurrentEntry = CurrentEntry->Next;
        }
    }

    IgmpLink->CompatibilityMode = NewMode;
    return;
}

VOID
NetpIgmpSendGroupReport (
    PIGMP_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine sends a IGMP report message for a specific multicast group.

Arguments:

    Group - Supplies a pointer to the multicast group to report.

Return Value:

    None.

--*/

{

    ULONG BufferFlags;
    ULONG BufferSize;
    IGMP_VERSION CompatibilityMode;
    IP4_ADDRESS DestinationAddress;
    PIGMP_GROUP_RECORD_V3 GroupRecord;
    PIGMP_HEADER Header;
    NET_PACKET_LIST NetPacketList;
    PNET_PACKET_BUFFER Packet;
    PIGMP_MESSAGE Report;
    PIGMP_REPORT_V3 ReportV3;
    ULONG SendCount;
    KSTATUS Status;
    UCHAR Type;

    //
    // Never send a report for the all systems group.
    //

    if (Group->Address == IGMP_ALL_SYSTEMS_ADDRESS) {
        return;
    }

    //
    // Snap the compatibility mode.
    //

    CompatibilityMode = Group->IgmpLink->CompatibilityMode;
    if (CompatibilityMode == IgmpVersion3) {
        BufferSize = sizeof(IGMP_REPORT_V3) + sizeof(IGMP_GROUP_RECORD_V3);

        ASSERT(BufferSize <= Group->IgmpLink->MaxPacketSize);

    } else {
        BufferSize = sizeof(IGMP_MESSAGE);
    }

    BufferFlags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Status = NetAllocateBuffer(IGMP_IP4_HEADER_SIZE,
                               BufferSize,
                               0,
                               Group->IgmpLink->Link,
                               BufferFlags,
                               &Packet);

    if (!KSUCCESS(Status)) {
        return;
    }

    Type = IGMP_MESSAGE_TYPE_REPORT_V1;
    DestinationAddress.Domain = NetDomainIp4;
    Header = (PIGMP_HEADER)(Packet->Buffer + Packet->DataOffset);
    switch (CompatibilityMode) {
    case IgmpVersion3:
        Type = IGMP_MESSAGE_TYPE_REPORT_V3;
        DestinationAddress.Address = IGMP_ALL_ROUTERS_ADDRESS_V3;
        ReportV3 = (PIGMP_REPORT_V3)Header;
        ReportV3->Reserved = 0;
        ReportV3->GroupRecordCount = CPU_TO_NETWORK16(1);
        GroupRecord = (PIGMP_GROUP_RECORD_V3)(ReportV3 + 1);
        if ((Group->Flags & IGMP_MULTICAST_GROUP_FLAG_STATE_CHANGE) != 0) {
            GroupRecord->Type = IGMP_GROUP_RECORD_TYPE_CHANGE_TO_EXCLUDE_MODE;

        } else {
            GroupRecord->Type = IGMP_GROUP_RECORD_TYPE_MODE_IS_EXCLUDE;
        }

        GroupRecord->DataLength = 0;
        GroupRecord->SourceAddressCount = CPU_TO_NETWORK16(0);
        GroupRecord->MulticastAddress = Group->Address;
        break;

    case IgmpVersion2:
        Type = IGMP_MESSAGE_TYPE_REPORT_V2;

        //
        // Fall through to version 1 in order to fill out the group address.
        //

    case IgmpVersion1:
        Report = (PIGMP_MESSAGE)Header;
        Report->GroupAddress = Group->Address;
        DestinationAddress.Address = Group->Address;
        break;

    default:

        ASSERT(FALSE);

        return;
    }

    //
    // Fill out the IGMP header common to all versions and send it on to the
    // common send routine.
    //

    Header->Type = Type;
    Header->MaxResponseCode = 0;
    Header->Checksum = 0;
    Header->Checksum = NetpIgmpChecksumData((PVOID)Header, BufferSize);
    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    NET_ADD_PACKET_TO_LIST(Packet, &NetPacketList);
    NetpIgmpSendPackets(Group->IgmpLink,
                        (PNETWORK_ADDRESS)&DestinationAddress,
                        &NetPacketList);

    RtlAtomicOr32(&(Group->Flags), IGMP_MULTICAST_GROUP_FLAG_LAST_REPORT);

    //
    // Queue the report to be sent again if necessary.
    //

    SendCount = RtlAtomicAdd32(&(Group->SendCount), -1);

    ASSERT((SendCount != 0) && (SendCount < 0x10000000));

    if (SendCount > 1) {
        NetpIgmpQueueReportTimer(&(Group->Timer),
                                 KeGetRecentTimeCounter(),
                                 IGMP_DEFAULT_UNSOLICITED_REPORT_INTERVAL);
    }

    return;
}

VOID
NetpIgmpSendGroupLeave (
    PIGMP_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine sends an IGMP leave message to the all routers multicast group.

Arguments:

    Group - Supplies a pointer to the multicast group that the host is leaving.

Return Value:

    None.

--*/

{

    ULONG BufferFlags;
    ULONG BufferSize;
    IGMP_VERSION CompatibilityMode;
    IP4_ADDRESS DestinationAddress;
    BOOL DestroyGroup;
    PIGMP_GROUP_RECORD_V3 GroupRecord;
    PIGMP_HEADER Header;
    PIGMP_MESSAGE Leave;
    NET_PACKET_LIST NetPacketList;
    PNET_PACKET_BUFFER Packet;
    PIGMP_REPORT_V3 ReportV3;
    ULONG SendCount;
    KSTATUS Status;
    UCHAR Type;

    DestroyGroup = TRUE;

    //
    // Never send a leave report for the all systems group.
    //

    if (Group->Address == IGMP_ALL_SYSTEMS_ADDRESS) {
        goto SendGroupLeaveEnd;
    }

    //
    // If this link was not the last to report the group, then don't send
    // a leave message.
    //

    if ((Group->Flags & IGMP_MULTICAST_GROUP_FLAG_LAST_REPORT) == 0) {
        goto SendGroupLeaveEnd;
    }

    //
    // Snap the current compatibility mode. No leave message needs to be sent
    // if the host is operating in IGMPv1 mode.
    //

    CompatibilityMode = Group->IgmpLink->CompatibilityMode;
    if (CompatibilityMode == IgmpVersion1) {
        goto SendGroupLeaveEnd;
    }

    if (CompatibilityMode == IgmpVersion2) {
        BufferSize = sizeof(IGMP_MESSAGE);

    } else {
        BufferSize = sizeof(IGMP_REPORT_V3) + sizeof(IGMP_GROUP_RECORD_V3);

        ASSERT(CompatibilityMode == IgmpVersion3);
        ASSERT(BufferSize <= Group->IgmpLink->MaxPacketSize);
    }

    BufferFlags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Status = NetAllocateBuffer(IGMP_IP4_HEADER_SIZE,
                               BufferSize,
                               0,
                               Group->IgmpLink->Link,
                               BufferFlags,
                               &Packet);

    if  (!KSUCCESS(Status)) {
        goto SendGroupLeaveEnd;
    }

    DestinationAddress.Domain = NetDomainIp4;
    Header = (PIGMP_HEADER)(Packet->Buffer + Packet->DataOffset);
    switch (CompatibilityMode) {
    case IgmpVersion3:
        Type = IGMP_MESSAGE_TYPE_REPORT_V3;
        DestinationAddress.Address = IGMP_ALL_ROUTERS_ADDRESS_V3;
        ReportV3 = (PIGMP_REPORT_V3)Header;
        ReportV3->GroupRecordCount = CPU_TO_NETWORK16(1);
        ReportV3->Reserved = 0;
        GroupRecord = (PIGMP_GROUP_RECORD_V3)(ReportV3 + 1);
        GroupRecord->Type = IGMP_GROUP_RECORD_TYPE_CHANGE_TO_INCLUDE_MODE;
        GroupRecord->DataLength = 0;
        GroupRecord->SourceAddressCount = CPU_TO_NETWORK16(0);
        GroupRecord->MulticastAddress = Group->Address;
        break;

    case IgmpVersion2:
        Type = IGMP_MESSAGE_TYPE_LEAVE_V2;
        Leave = (PIGMP_MESSAGE)Header;
        Leave->GroupAddress = Group->Address;
        DestinationAddress.Address = IGMP_ALL_ROUTERS_ADDRESS;
        break;

    case IgmpVersion1:
    default:

        ASSERT(FALSE);

        goto SendGroupLeaveEnd;
    }

    Header->Type = Type;
    Header->MaxResponseCode = 0;
    Header->Checksum = 0;
    Header->Checksum = NetpIgmpChecksumData((PVOID)Header, BufferSize);
    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    NET_ADD_PACKET_TO_LIST(Packet, &NetPacketList);
    NetpIgmpSendPackets(Group->IgmpLink,
                        (PNETWORK_ADDRESS)&DestinationAddress,
                        &NetPacketList);

    //
    // Queue the leave message to be sent again if necessary.
    //

    SendCount = RtlAtomicAdd32(&(Group->SendCount), -1);

    ASSERT((SendCount != 0) && (SendCount < 0x10000000));

    if (SendCount > 1) {
        NetpIgmpQueueReportTimer(&(Group->Timer),
                                 KeGetRecentTimeCounter(),
                                 IGMP_DEFAULT_UNSOLICITED_REPORT_INTERVAL);

        DestroyGroup = FALSE;
    }

SendGroupLeaveEnd:
    if (DestroyGroup != FALSE) {
        NetpIgmpGroupReleaseReference(Group);
    }

    return;
}

VOID
NetpIgmpSendLinkReport (
    PIGMP_LINK IgmpLink
    )

/*++

Routine Description:

    This routine sends a IGMP report message for the whole link.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link to report.

Return Value:

    None.

--*/

{

    ULONG BufferFlags;
    ULONG BufferSize;
    PLIST_ENTRY CurrentEntry;
    ULONG CurrentGroupCount;
    IP4_ADDRESS DestinationAddress;
    PIGMP_MULTICAST_GROUP Group;
    PIGMP_GROUP_RECORD_V3 GroupRecord;
    ULONG GroupSize;
    PIGMP_HEADER Header;
    NET_PACKET_LIST NetPacketList;
    PNET_PACKET_BUFFER Packet;
    ULONG RemainingGroupCount;
    PIGMP_REPORT_V3 ReportV3;
    USHORT SourceAddressCount;
    KSTATUS Status;

    //
    // Send as many IGMPv3 "Current-State" records as required to notify the
    // all routers group of all the multicast groups to which the given link
    // belongs. This may take more than one packet if the link is subscribed to
    // more than MAX_USHORT groups or if the number of groups requires a packet
    // larger than the link's max transfer size.
    //

    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    KeAcquireQueuedLock(IgmpLink->Lock);

    //
    // Never report the all systems group. The count is one less than the
    // total.
    //

    RemainingGroupCount = IgmpLink->MulticastGroupCount - 1;
    CurrentEntry = IgmpLink->MulticastGroupList.Next;
    while (RemainingGroupCount != 0) {
        CurrentGroupCount = RemainingGroupCount;
        if (CurrentGroupCount > IGMP_MAX_GROUP_RECORD_COUNT) {
            CurrentGroupCount = IGMP_MAX_GROUP_RECORD_COUNT;
        }

        BufferSize = sizeof(IGMP_REPORT_V3) +
                     (sizeof(IGMP_GROUP_RECORD_V3) * CurrentGroupCount);

        if (BufferSize > IgmpLink->MaxPacketSize) {
            BufferSize = IgmpLink->MaxPacketSize;
            CurrentGroupCount = (BufferSize - sizeof(IGMP_REPORT_V3)) /
                                sizeof(IGMP_GROUP_RECORD_V3);
        }

        RemainingGroupCount -= CurrentGroupCount;
        BufferFlags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
                      NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
                      NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                      NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

        Status = NetAllocateBuffer(IGMP_IP4_HEADER_SIZE,
                                   BufferSize,
                                   0,
                                   IgmpLink->Link,
                                   BufferFlags,
                                   &Packet);

        if (!KSUCCESS(Status)) {
            break;
        }

        Header = (PIGMP_HEADER)(Packet->Buffer + Packet->DataOffset);
        Header->Type = IGMP_MESSAGE_TYPE_REPORT_V3;
        Header->MaxResponseCode = 0;
        Header->Checksum = 0;
        ReportV3 = (PIGMP_REPORT_V3)Header;
        ReportV3->Reserved = 0;
        ReportV3->GroupRecordCount = CPU_TO_NETWORK16(CurrentGroupCount);
        GroupRecord = (PIGMP_GROUP_RECORD_V3)(ReportV3 + 1);
        while (CurrentGroupCount != 0) {

            ASSERT(CurrentEntry != &(IgmpLink->MulticastGroupList));

            //
            // Skip the all systems group. It was not included in the total
            // count, so don't decrement the counter.
            //

            Group = LIST_VALUE(CurrentEntry, IGMP_MULTICAST_GROUP, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (Group->Address == IGMP_ALL_SYSTEMS_ADDRESS) {
                continue;
            }

            CurrentGroupCount -= 1;

            //
            // The count should be accurate and eliminate the need to check for
            // the head.
            //

            GroupRecord->Type = IGMP_GROUP_RECORD_TYPE_MODE_IS_EXCLUDE;
            GroupRecord->DataLength = 0;
            SourceAddressCount = 0;
            GroupRecord->SourceAddressCount =
                                          CPU_TO_NETWORK16(SourceAddressCount);

            GroupRecord->MulticastAddress = Group->Address;
            GroupSize = sizeof(IGMP_GROUP_RECORD_V3) +
                        (SourceAddressCount * sizeof(ULONG)) +
                        GroupRecord->DataLength;

            GroupRecord =
                      (PIGMP_GROUP_RECORD_V3)((PUCHAR)GroupRecord + GroupSize);
        }

        Header->Checksum = NetpIgmpChecksumData((PVOID)Header, BufferSize);
        NET_ADD_PACKET_TO_LIST(Packet, &NetPacketList);
    }

    KeReleaseQueuedLock(IgmpLink->Lock);
    if (NET_PACKET_LIST_EMPTY(&NetPacketList) != FALSE) {
        return;
    }

    DestinationAddress.Domain = NetDomainIp4;
    DestinationAddress.Address = IGMP_ALL_ROUTERS_ADDRESS_V3;
    NetpIgmpSendPackets(IgmpLink,
                        (PNETWORK_ADDRESS)&DestinationAddress,
                        &NetPacketList);

    return;
}

VOID
NetpIgmpSendPackets (
    PIGMP_LINK IgmpLink,
    PNETWORK_ADDRESS Destination,
    PNET_PACKET_LIST PacketList
    )

/*++

Routine Description:

    This routine sends a list of IGMP packets out over the provided link to the
    specified destination. It simply adds the IPv4 headers and sends the
    packets down the stack.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link over which to send the
        packet.

    Destination - Supplies a pointer to the destination address. This should be
        a multicast address.

    PacketList - Supplies a pointer to the list of packets to send.

Return Value:

    None.

--*/

{

    USHORT Checksum;
    PLIST_ENTRY CurrentEntry;
    PIP4_ADDRESS DestinationAddress;
    NETWORK_ADDRESS DestinationPhysical;
    PIP4_HEADER Header;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PNET_PACKET_BUFFER Packet;
    PULONG RouterAlert;
    PIP4_ADDRESS SourceAddress;
    KSTATUS Status;
    ULONG TotalLength;

    Link = IgmpLink->Link;
    LinkAddress = IgmpLink->LinkAddress;
    DestinationAddress = (PIP4_ADDRESS)Destination;
    SourceAddress = (PIP4_ADDRESS)&(LinkAddress->Address);

    //
    // Add the IPv4 header to each of the IGMP packets. Each packet includes
    // the router alert option.
    //

    CurrentEntry = PacketList->Head.Next;
    while (CurrentEntry != &(PacketList->Head)) {
        Packet = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(Packet->DataOffset >= IGMP_IP4_HEADER_SIZE);

        Packet->DataOffset -= IGMP_IP4_HEADER_SIZE;

        //
        // Fill out the IPv4 header. In order to avoid creating a socket and
        // because IGMP only works on top of IPv4, the IGMP module sends IPv4
        // packets directly to the physical layer.
        //

        Header = (PIP4_HEADER)(Packet->Buffer + Packet->DataOffset);
        Header->VersionAndHeaderLength =
                                 IP4_VERSION |
                                 (UCHAR)(IGMP_IP4_HEADER_SIZE / sizeof(ULONG));

        Header->Type = IP4_PRECEDENCE_NETWORK_CONTROL;
        TotalLength = Packet->FooterOffset - Packet->DataOffset;
        Header->TotalLength = CPU_TO_NETWORK16(TotalLength);
        Header->Identification = 0;
        Header->FragmentOffset = 0;
        Header->TimeToLive = 1;
        Header->Protocol = SOCKET_INTERNET_PROTOCOL_IGMP;
        Header->HeaderChecksum = 0;

        //
        // The source address is supposed to be the link's IP address. If the
        // link does not have an IP address assign, "0.0.0.0" is used. Either
        // way, the correct value is in the link address entry's address field.
        //

        Header->SourceAddress = SourceAddress->Address;
        Header->DestinationAddress = DestinationAddress->Address;
        RouterAlert = (PULONG)(Header + 1);
        *RouterAlert = IGMP_IP4_ROUTER_ALERT_OPTION;
        if ((Link->Properties.Capabilities &
             NET_LINK_CAPABILITY_TRANSMIT_IP_CHECKSUM_OFFLOAD) == 0) {

             Checksum = NetpIgmpChecksumData((PVOID)Header,
                                             IGMP_IP4_HEADER_SIZE);

             Header->HeaderChecksum = Checksum;

        } else {
            Packet->Flags |= NET_PACKET_FLAG_IP_CHECKSUM_OFFLOAD;
        }
    }

    //
    // Get the physical address for the IPv4 multicast destination address.
    //

    Status = Link->DataLinkEntry->Interface.ConvertToPhysicalAddress(
                                                          Destination,
                                                          &DestinationPhysical,
                                                          NetAddressMulticast);

    if (!KSUCCESS(Status)) {
        return;
    }

    Link->DataLinkEntry->Interface.Send(Link->DataLinkContext,
                                        PacketList,
                                        &(LinkAddress->PhysicalAddress),
                                        &DestinationPhysical,
                                        IP4_PROTOCOL_NUMBER);

    return;
}

KSTATUS
NetpIgmpUpdateAddressFilters (
    PIGMP_LINK IgmpLink
    )

/*++

Routine Description:

    This routine updates the given links address filtering based on the
    multicast groups to which the link is currently joined. It will gather
    a list of all the physical layer addresses that need to be enabled and pass
    them to the hardware for it to update its filters. It falls back to
    enabling promiscuous mode if the link does not support multicast address
    filtering.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link whose filters are to be
        updated.

Return Value:

    Status code.

--*/

{

    PNET_DEVICE_LINK_GET_SET_INFORMATION GetSetInformation;
    PNET_LINK Link;
    ULONG PromiscuousMode;
    UINTN PromiscuousModeSize;
    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(IgmpLink->Lock) != FALSE);

    Link = IgmpLink->Link;
    GetSetInformation = Link->Properties.Interface.GetSetInformation;

    //
    // Set the link into promiscuous mode if there are any groups. Otherwise
    // turn it off. Promiscuous must be supported for the link to have made it
    // this far in IGMP.
    //
    // TODO: Implement real multicast address filtering.
    //

    ASSERT((Link->Properties.Capabilities &
            NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0);

    PromiscuousMode = FALSE;
    if (IgmpLink->MulticastGroupCount != 0) {
        PromiscuousMode = TRUE;
    }

    PromiscuousModeSize = sizeof(ULONG);
    Status = GetSetInformation(Link->Properties.DeviceContext,
                               NetLinkInformationPromiscuousMode,
                               &PromiscuousMode,
                               &PromiscuousModeSize,
                               TRUE);

    return Status;
}

PIGMP_LINK
NetpIgmpCreateOrLookupLink (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine creates an IGMP link associated with the given local address
    and attempts to insert it into the tree. If an existing match is found,
    then the existing link is returned.

Arguments:

    Link - Supplies a pointer to the network link for which the IGMP link is
        to be created.

    LinkAddress - Supplies a pointer to the link address entry on the given
        network link with which the IGMP link shall be associated.

Return Value:

    Returns a pointer to the newly allocated IGMP link on success or NULL on
    failure.

--*/

{

    PNET_DATA_LINK_ENTRY DataLinkEntry;
    NET_PACKET_SIZE_INFORMATION DataSizeInformation;
    PRED_BLACK_TREE_NODE FoundNode;
    PIGMP_LINK IgmpLink;
    ULONG Index;
    PNET_PACKET_SIZE_INFORMATION LinkSizeInformation;
    ULONG MaxPacketSize;
    PIGMP_MULTICAST_GROUP NewGroup;
    PIGMP_LINK NewIgmpLink;
    IGMP_LINK SearchLink;
    KSTATUS Status;
    BOOL TreeLockHeld;

    IgmpLink = NULL;
    NewGroup = NULL;
    NewIgmpLink = NULL;
    TreeLockHeld = FALSE;

    //
    // If the link does not support promiscuous mode, then don't allow the
    // create to go any further.
    //

    if ((Link->Properties.Capabilities &
         NET_LINK_CAPABILITY_PROMISCUOUS_MODE) == 0) {

        Status = STATUS_NOT_SUPPORTED;
        goto CreateOrLookupLinkEnd;
    }

    NewIgmpLink = MmAllocatePagedPool(sizeof(IGMP_LINK),
                                      IGMP_PROTOCOL_ALLOCATION_TAG);

    if (NewIgmpLink == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateOrLookupLinkEnd;
    }

    RtlZeroMemory(NewIgmpLink, sizeof(IGMP_LINK));
    NewIgmpLink->ReferenceCount = 1;
    NetLinkAddReference(Link);
    NewIgmpLink->Link = Link;
    NewIgmpLink->LinkAddress = LinkAddress;
    NewIgmpLink->RobustnessVariable = IGMP_DEFAULT_ROBUSTNESS_VARIABLE;
    NewIgmpLink->QueryInterval = IGMP_DEFAULT_QUERY_INTERVAL;
    NewIgmpLink->MaxResponseTime = IGMP_DEFAULT_MAX_RESPONSE_TIME;
    NewIgmpLink->CompatibilityMode = IgmpVersion3;
    INITIALIZE_LIST_HEAD(&(NewIgmpLink->MulticastGroupList));
    NewIgmpLink->Lock = KeCreateQueuedLock();
    if (NewIgmpLink->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateOrLookupLinkEnd;
    }

    //
    // Determine the maximum allowed IGMP packet size based on the link.
    //

    LinkSizeInformation = &(Link->Properties.PacketSizeInformation);
    MaxPacketSize = LinkSizeInformation->MaxPacketSize;
    DataLinkEntry = Link->DataLinkEntry;
    DataLinkEntry->Interface.GetPacketSizeInformation(Link->DataLinkContext,
                                                      &DataSizeInformation,
                                                      0);

    if (MaxPacketSize > DataSizeInformation.MaxPacketSize) {
        MaxPacketSize = DataSizeInformation.MaxPacketSize;
    }

    MaxPacketSize -= (LinkSizeInformation->HeaderSize +
                      LinkSizeInformation->FooterSize +
                      DataSizeInformation.HeaderSize +
                      DataSizeInformation.FooterSize +
                      IGMP_IP4_HEADER_SIZE);

    NewIgmpLink->MaxPacketSize = MaxPacketSize;
    Status = NetpIgmpInitializeTimer(&(NewIgmpLink->ReportTimer),
                                     NetpIgmpLinkReportTimeoutWorker,
                                     NewIgmpLink);

    if (!KSUCCESS(Status)) {
        goto CreateOrLookupLinkEnd;
    }

    //
    // Initialize the compatibility mode counters.
    //

    for (Index = 0; Index < IGMP_COMPATIBILITY_MODE_COUNT; Index += 1) {
        Status = NetpIgmpInitializeTimer(
                                     &(NewIgmpLink->CompatibilityTimer[Index]),
                                     NetpIgmpLinkCompatibilityTimeoutWorker,
                                     NewIgmpLink);

        if (!KSUCCESS(Status)) {
            goto CreateOrLookupLinkEnd;
        }
    }

    //
    // All multicast hosts are supposed to join the all systems group (but
    // never report the membership). This is supposed to be done on
    // initialization, but opt to do it the first indication that multicast
    // is being used. This saves the system from processing multicast queries
    // where there is nothing to report.
    //

    NewGroup = NetpIgmpCreateGroup(NewIgmpLink, IGMP_ALL_SYSTEMS_ADDRESS);
    if (NewGroup == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateOrLookupLinkEnd;
    }

    //
    // The group now has a reference on the IGMP link. Destroying the group
    // will destroy the link. Prevent cleanup from releasing a link reference.
    //

    IgmpLink = NewIgmpLink;
    NewIgmpLink = NULL;

    //
    // The group must be inserted directly, before the link is added to the
    // tree. It cannot go through the normal join path for two reasons. 1) The
    // normal join path updates the link's address filters. At this point, two
    // threads may be racing to create the IGMP link; the address filters
    // should not be updated until one is a clear winner. 2) If the all systems
    // group creation/join were to happen after the new link wins the insert
    // race, it may still fail, which would break IGMP link dereference. The
    // dereference path is carefully implemented to synchronously remove the
    // all systems group and assumes that the last group to remain is the all
    // systems group. If the all systems group never got added but another
    // group did, then asserts would fire.
    //

    INSERT_BEFORE(&(NewGroup->ListEntry), &(IgmpLink->MulticastGroupList));
    IgmpLink->MulticastGroupCount = 1;
    NewGroup->JoinCount = 1;

    //
    // Attempt to insert the new IGMP link into the tree. If an existing link
    // is found, use that one and destroy the new one.
    //

    SearchLink.Link = Link;
    KeAcquireSharedExclusiveLockExclusive(NetIgmpLinkLock);
    TreeLockHeld = TRUE;
    FoundNode = RtlRedBlackTreeSearch(&NetIgmpLinkTree, &(SearchLink.Node));
    if (FoundNode == NULL) {

        //
        // Before this IGMP link hits the tree and another group can take a
        // reference on it, make sure the all systems group gets set in the
        // hardware filter. This is necessary in case the first group being
        // joined is the all systems group. That join request would be the
        // second request and would not update the filters.
        //

        KeAcquireQueuedLock(IgmpLink->Lock);
        Status = NetpIgmpUpdateAddressFilters(IgmpLink);
        KeReleaseQueuedLock(IgmpLink->Lock);
        if (!KSUCCESS(Status)) {
            LIST_REMOVE(&(NewGroup->ListEntry));
            NewGroup->JoinCount = 0;
            IgmpLink->MulticastGroupCount = 0;
            IgmpLink = NULL;
            goto CreateOrLookupLinkEnd;
        }

        RtlRedBlackTreeInsert(&NetIgmpLinkTree, &(IgmpLink->Node));
        NewGroup = NULL;

    } else {
        IgmpLink = RED_BLACK_TREE_VALUE(FoundNode, IGMP_LINK, Node);
    }

    NetpIgmpLinkAddReference(IgmpLink);
    KeReleaseSharedExclusiveLockExclusive(NetIgmpLinkLock);
    TreeLockHeld = FALSE;
    Status = STATUS_SUCCESS;

CreateOrLookupLinkEnd:
    if (TreeLockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetIgmpLinkLock);
    }

    if (NewGroup != NULL) {
        NetpIgmpGroupReleaseReference(NewGroup);
    }

    if (NewIgmpLink != NULL) {
        NetpIgmpLinkReleaseReference(NewIgmpLink);
    }

    return IgmpLink;
}

VOID
NetpIgmpDestroyLink (
    PIGMP_LINK IgmpLink
    )

/*++

Routine Description:

    This routine destroys an IGMP link and all of its resources.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link to destroy.

Return Value:

    None.

--*/

{

    ULONG Index;

    ASSERT(IgmpLink->ReferenceCount == 0);
    ASSERT(LIST_EMPTY(&(IgmpLink->MulticastGroupList)) != FALSE);

    NetpIgmpDestroyTimer(&(IgmpLink->ReportTimer));
    for (Index = 0; Index < IGMP_COMPATIBILITY_MODE_COUNT; Index += 1) {
        NetpIgmpDestroyTimer(&(IgmpLink->CompatibilityTimer[Index]));
    }

    if (IgmpLink->Lock != NULL) {
        KeDestroyQueuedLock(IgmpLink->Lock);
    }

    NetLinkReleaseReference(IgmpLink->Link);
    MmFreePagedPool(IgmpLink);
    return;
}

PIGMP_LINK
NetpIgmpLookupLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine finds an IGMP link associated with the given network link. The
    caller is expected to release a reference on the IGMP link.

Arguments:

    Link - Supplies a pointer to the network link, which is used to look up the
        IGMP link.

Return Value:

    Returns a pointer to the matching IGMP link on success or NULL on failure.

--*/

{

    PRED_BLACK_TREE_NODE FoundNode;
    PIGMP_LINK IgmpLink;
    IGMP_LINK SearchLink;

    IgmpLink = NULL;
    SearchLink.Link = Link;
    KeAcquireSharedExclusiveLockShared(NetIgmpLinkLock);
    FoundNode = RtlRedBlackTreeSearch(&NetIgmpLinkTree, &(SearchLink.Node));
    if (FoundNode == NULL) {
        goto FindLinkEnd;
    }

    IgmpLink = RED_BLACK_TREE_VALUE(FoundNode, IGMP_LINK, Node);
    NetpIgmpLinkAddReference(IgmpLink);

FindLinkEnd:
    KeReleaseSharedExclusiveLockShared(NetIgmpLinkLock);
    return IgmpLink;
}

VOID
NetpIgmpLinkAddReference (
    PIGMP_LINK IgmpLink
    )

/*++

Routine Description:

    This routine increments the reference count of an IGMP link.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(IgmpLink->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    return;
}

VOID
NetpIgmpLinkReleaseReference (
    PIGMP_LINK IgmpLink
    )

/*++

Routine Description:

    This routine releases a reference on an IGMP link.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link.

Return Value:

    None.

--*/

{

    PIGMP_MULTICAST_GROUP Group;
    ULONG OldReferenceCount;
    KSTATUS Status;

    //
    // Acquire the tree lock exclusively before decrementing the reference
    // count. This is necessary to make the decrement and removal from the tree
    // atomic. The link is removed from the tree when its reference count
    // reaches 2 and the all systems group has a join count of 1.
    //

    KeAcquireSharedExclusiveLockExclusive(NetIgmpLinkLock);
    OldReferenceCount = RtlAtomicAdd32(&(IgmpLink->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    //
    // If the third reference was just released, then the last two references
    // are from the all systems group and from creation. No other multicast
    // groups have a reference on the link and as the tree lock is held
    // exclusively, no other thread has a reference on the link. Therefore, if
    // the all systems group is only around due to the implicit join, then the
    // link can be removed from the tree and the all systems group can be
    // destroyed.
    //

    if (OldReferenceCount == 3) {
        Group = LIST_VALUE(IgmpLink->MulticastGroupList.Next,
                           IGMP_MULTICAST_GROUP,
                           ListEntry);

        //
        // This better be the only group and be the all systems group. And
        // since no other thread should have access to the IGMP link, the lock
        // should not be held - meaning the join count won't be changing.
        //

        ASSERT(IgmpLink->MulticastGroupCount == 1);
        ASSERT(Group->Address == IGMP_ALL_SYSTEMS_ADDRESS);
        ASSERT(KeIsQueuedLockHeld(IgmpLink->Lock) == FALSE);

        //
        // If only the implicit join is left, remove the group from the link
        // and update the address filters. On success, the link should have no
        // more multicast filters set. Remove it from the tree. On failure,
        // act like nothing happened and leave the group and link alone.
        //

        if (Group->JoinCount == 1) {
            KeAcquireQueuedLock(IgmpLink->Lock);
            LIST_REMOVE(&(Group->ListEntry));
            IgmpLink->MulticastGroupCount -= 1;
            Status = NetpIgmpUpdateAddressFilters(IgmpLink);
            if (!KSUCCESS(Status)) {
                INSERT_BEFORE(&(Group->ListEntry),
                              &(IgmpLink->MulticastGroupList));

                IgmpLink->MulticastGroupCount += 1;
                Group = NULL;

            } else {

                ASSERT(IgmpLink->MulticastGroupCount == 0);

                RtlRedBlackTreeRemove(&NetIgmpLinkTree, &(IgmpLink->Node));
                IgmpLink->Node.Parent = NULL;
                Group->JoinCount -= 1;
            }

            KeReleaseQueuedLock(IgmpLink->Lock);

        //
        // Otherwise the all systems group is still in use. When the group is
        // left, the link will be looked up, bumping the reference count to 3.
        // Then the group will be left and the link will be dereferenced,
        // invoking this code path again, but with the group's join count at 1.
        //

        } else {
            Group = NULL;
        }

        KeReleaseSharedExclusiveLockExclusive(NetIgmpLinkLock);

        //
        // If the group and link got removed, destroy the group. This should
        // release the 2nd to last reference on the link.
        //

        if (Group != NULL) {
            NetpIgmpGroupReleaseReference(Group);
        }

    //
    // If this is the second to last reference, then the only remaining
    // reference is the one added by creation. No multicast groups have a
    // reference on the link and it should have already been removed from the
    // link tree.
    //

    } else if (OldReferenceCount == 2) {

        ASSERT(IgmpLink->MulticastGroupCount == 0);
        ASSERT(IgmpLink->Node.Parent == NULL);

        KeReleaseSharedExclusiveLockExclusive(NetIgmpLinkLock);
        NetpIgmpLinkReleaseReference(IgmpLink);

    } else {
        KeReleaseSharedExclusiveLockExclusive(NetIgmpLinkLock);
        if (OldReferenceCount == 1) {
            NetpIgmpDestroyLink(IgmpLink);
        }
    }

    return;
}

COMPARISON_RESULT
NetpIgmpCompareLinkEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes.

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

    PIGMP_LINK FirstIgmpLink;
    PIGMP_LINK SecondIgmpLink;

    FirstIgmpLink = RED_BLACK_TREE_VALUE(FirstNode, IGMP_LINK, Node);
    SecondIgmpLink = RED_BLACK_TREE_VALUE(FirstNode, IGMP_LINK, Node);
    if (FirstIgmpLink->Link == SecondIgmpLink->Link) {
        return ComparisonResultSame;

    } else if (FirstIgmpLink->Link < SecondIgmpLink->Link) {
        return ComparisonResultAscending;
    }

    return ComparisonResultDescending;
}

PIGMP_MULTICAST_GROUP
NetpIgmpCreateGroup (
    PIGMP_LINK IgmpLink,
    ULONG GroupAddress
    )

/*++

Routine Description:

    This routine creats an IGMP multicast group structure.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link to which the multicast group
        will belong.

    GroupAddress - Supplies the IPv4 multicast address for the group.

Return Value:

    Returns a pointer to the newly allocated multicast group.

--*/

{

    PIGMP_MULTICAST_GROUP Group;
    KSTATUS Status;

    Group = MmAllocatePagedPool(sizeof(IGMP_MULTICAST_GROUP),
                                IGMP_PROTOCOL_ALLOCATION_TAG);

    if (Group == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateGroupEnd;
    }

    RtlZeroMemory(Group, sizeof(IGMP_MULTICAST_GROUP));
    Group->ReferenceCount = 1;
    NetpIgmpLinkAddReference(IgmpLink);
    Group->IgmpLink = IgmpLink;
    Group->Address = GroupAddress;
    Status = NetpIgmpInitializeTimer(&(Group->Timer),
                                     NetpIgmpGroupTimeoutWorker,
                                     Group);

    if (!KSUCCESS(Status)) {
        goto CreateGroupEnd;
    }

CreateGroupEnd:
    if (!KSUCCESS(Status)) {
        if (Group != NULL) {
            NetpIgmpDestroyGroup(Group);
            Group = NULL;
        }
    }

    return Group;
}

VOID
NetpIgmpDestroyGroup (
    PIGMP_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine destroys all the resources for the given multicast group.

Arguments:

    Group - Supplies a pointer to the group to destroy.

Return Value:

    None.

--*/

{

    ASSERT(Group->JoinCount == 0);

    NetpIgmpDestroyTimer(&(Group->Timer));
    NetpIgmpLinkReleaseReference(Group->IgmpLink);
    MmFreePagedPool(Group);
    return;
}

VOID
NetpIgmpGroupAddReference (
    PIGMP_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine increments the reference count of an IGMP multicast group.

Arguments:

    Group - Supplies a pointer to the IGMP multicast group.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Group->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    return;
}

VOID
NetpIgmpGroupReleaseReference (
    PIGMP_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine releases a reference on an IGMP multicast group.

Arguments:

    Group - Supplies a pointer to the IGMP multicast group.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Group->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        NetpIgmpDestroyGroup(Group);
    }

    return;
}

KSTATUS
NetpIgmpInitializeTimer (
    PIGMP_TIMER Timer,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID WorkParameter
    )

/*++

Routine Description:

    This routine initializes the given IGMP timer, setting up its timer, DPC,
    and work item.

Arguments:

    Timer - Supplies a pointer to the IGMP timer to initialize.

    WorkRoutine - Supplies a pointer to the routine that runs when the work
        item is scheduled.

    WorkParameter - Supplies a pointer that is passed to the work routine when
        it is invoked.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Timer->Timer = KeCreateTimer(IGMP_PROTOCOL_ALLOCATION_TAG);
    if (Timer->Timer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTimerEnd;
    }

    Timer->Dpc = KeCreateDpc(NetpIgmpTimerDpcRoutine, Timer);
    if (Timer->Dpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTimerEnd;
    }

    Timer->WorkItem = KeCreateWorkItem(NULL,
                                       WorkPriorityNormal,
                                       WorkRoutine,
                                       WorkParameter,
                                       IGMP_PROTOCOL_ALLOCATION_TAG);

    if (Timer->WorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTimerEnd;
    }

    Status = STATUS_SUCCESS;

InitializeTimerEnd:
    if (!KSUCCESS(Status)) {
        NetpIgmpDestroyTimer(Timer);
    }

    return Status;
}

VOID
NetpIgmpDestroyTimer (
    PIGMP_TIMER Timer
    )

/*++

Routine Description:

    This routine destroys all the resources of an IGMP timer. It does not
    release the structure itself, as it is ususally embedded within another
    structure.

Arguments:

    Timer - Supplies a pointer to an IGMP timer to destroy.

Return Value:

    None.

--*/

{

    if (Timer->Timer != NULL) {
        KeDestroyTimer(Timer->Timer);
    }

    if (Timer->Dpc != NULL) {
        KeDestroyDpc(Timer->Dpc);
    }

    if (Timer->WorkItem != NULL) {
        KeDestroyWorkItem(Timer->WorkItem);
    }

    return;
}

USHORT
NetpIgmpChecksumData (
    PVOID Data,
    ULONG Length
    )

/*++

Routine Description:

    This routine checksums a section of data for IGMP processing.

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

