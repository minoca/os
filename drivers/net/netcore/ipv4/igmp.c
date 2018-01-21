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
// Define the allocation tag used by IGMP.
//

#define IGMP_ALLOCATION_TAG 0x706d6749 // 'pmgI'

//
// Define the size of an IGMP IPv4 header. Each packet should include the
// router alert option.
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
#define IGMP_MULTICAST_GROUP_FLAG_LEAVE_SENT   0x00000004

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

#pragma pack(push, 1)

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

    DataLength - Stores the length of auxiliary data, in 32-bit words, that
        starts at the end of the source address array.

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

#pragma pack(pop)

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

    GroupCount - Stores the number of multicast groups that are associated
        with the link and should be reported in a total link report.

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
    ULONG GroupCount;
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
        robustness value. Updates are protected by the IGMP link's queued lock.

    Flags - Stores a bitmask of multicast group flags. See
        IGMP_MULTICAST_GROUP_FLAG_* for definitions. Updates are protected by
        the IGMP link's queued lock.

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
    ULONG SendCount;
    ULONG Flags;
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
    PNET_NETWORK_MULTICAST_REQUEST Request
    );

KSTATUS
NetpIgmpLeaveMulticastGroup (
    PNET_NETWORK_MULTICAST_REQUEST Request
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
    PIP4_ADDRESS GroupAddress
    );

VOID
NetpIgmpDestroyGroup (
    PIGMP_MULTICAST_GROUP Group
    );

PIGMP_MULTICAST_GROUP
NetpIgmpLookupGroup (
    PIGMP_LINK IgmpLink,
    PIP4_ADDRESS GroupAddress
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

BOOL
NetpIgmpIsReportableAddress (
    ULONG Address
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

    ComputedChecksum = NetChecksumData((PVOID)Header, Length);
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
    PIP4_ADDRESS MulticastAddress;
    PNET_NETWORK_MULTICAST_REQUEST MulticastRequest;
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

        RequiredSize = sizeof(NET_NETWORK_MULTICAST_REQUEST);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        MulticastRequest = (PNET_NETWORK_MULTICAST_REQUEST)Data;
        MulticastAddress = (PIP4_ADDRESS)MulticastRequest->MulticastAddress;
        if ((MulticastAddress->Domain != NetDomainIp4) ||
            (!IP4_IS_MULTICAST_ADDRESS(MulticastAddress->Address))) {

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
    PNET_NETWORK_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine joins the multicast group on the network link provided in the
    request. If this is the first request to join the supplied multicast group
    on the specified link, then an IGMP report is sent out over the network.

Arguments:

    Request - Supplies a pointer to a multicast request, which includes the
        address of the group to join and the network link on which to join the
        group.

Return Value:

    Status code.

--*/

{

    PIGMP_MULTICAST_GROUP Group;
    PIP4_ADDRESS GroupAddress;
    PIGMP_LINK IgmpLink;
    BOOL LinkLockHeld;
    PIGMP_MULTICAST_GROUP NewGroup;
    KSTATUS Status;

    Group = NULL;
    LinkLockHeld = FALSE;
    GroupAddress = (PIP4_ADDRESS)Request->MulticastAddress;
    NewGroup = NULL;

    //
    // If the group never needs to be reported, don't bother to record it at
    // this layer. Netcore already has a record of it.
    //

    if (NetpIgmpIsReportableAddress(GroupAddress->Address) == FALSE) {
        return STATUS_SUCCESS;
    }

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

    Status = STATUS_SUCCESS;
    while (TRUE) {
        KeAcquireQueuedLock(IgmpLink->Lock);
        LinkLockHeld = TRUE;
        Group = NetpIgmpLookupGroup(IgmpLink, GroupAddress);
        if (Group != NULL) {
            Group->JoinCount += 1;
            goto JoinMulticastGroupEnd;
        }

        if (NewGroup == NULL) {
            KeReleaseQueuedLock(IgmpLink->Lock);
            LinkLockHeld = FALSE;
            NewGroup = NetpIgmpCreateGroup(IgmpLink, GroupAddress);
            if (NewGroup == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto JoinMulticastGroupEnd;
            }

            continue;
        }

        //
        // Add the newly allocated group to the link's list.
        //

        INSERT_BEFORE(&(NewGroup->ListEntry), &(IgmpLink->MulticastGroupList));
        IgmpLink->GroupCount += 1;
        break;
    }

    //
    // Initialize the send count to the robustness variable. This will cause
    // multiple join messages to be sent, up to the robustness count.
    //

    NewGroup->SendCount = IgmpLink->RobustnessVariable;

    //
    // An initial join sends state change messages and at least one message
    // will be sent, so start the group as the last reporter.
    //

    NewGroup->Flags |= IGMP_MULTICAST_GROUP_FLAG_STATE_CHANGE |
                       IGMP_MULTICAST_GROUP_FLAG_LAST_REPORT;

    //
    // Take an extra reference on the new group so that it is not destroyed
    // while sending the report. Once the lock is released, a leave request
    // could run through and attempt to take it down.
    //

    NetpIgmpGroupAddReference(NewGroup);
    KeReleaseQueuedLock(IgmpLink->Lock);
    LinkLockHeld = FALSE;

    //
    // Actually send out the group's join IGMP state change messages.
    //

    NetpIgmpSendGroupReport(NewGroup);

JoinMulticastGroupEnd:
    if (LinkLockHeld != FALSE) {
        KeReleaseQueuedLock(IgmpLink->Lock);
    }

    if (IgmpLink != NULL) {
        NetpIgmpLinkReleaseReference(IgmpLink);
    }

    if (NewGroup != NULL) {
        NetpIgmpGroupReleaseReference(NewGroup);
    }

    if (Group != NULL) {
        NetpIgmpGroupReleaseReference(Group);
    }

    return Status;
}

KSTATUS
NetpIgmpLeaveMulticastGroup (
    PNET_NETWORK_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine removes the local system from a multicast group. If this is
    the last request to leave a multicast group on the link, then a IGMP leave
    message is sent out over the network.

Arguments:

    Request - Supplies a pointer to a multicast request, which includes the
        address of the group to leave and the network link that has previously
        joined the group.

Return Value:

    Status code.

--*/

{

    PIGMP_MULTICAST_GROUP Group;
    PIGMP_LINK IgmpLink;
    BOOL LinkLockHeld;
    BOOL LinkUp;
    PIP4_ADDRESS MulticastAddress;
    KSTATUS Status;

    Group = NULL;
    IgmpLink = NULL;
    LinkLockHeld = FALSE;
    MulticastAddress = (PIP4_ADDRESS)Request->MulticastAddress;

    //
    // If the address is not reportable, an IGMP group was never made for it.
    //

    if (NetpIgmpIsReportableAddress(MulticastAddress->Address) == FALSE) {
        return STATUS_SUCCESS;
    }

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

    KeAcquireQueuedLock(IgmpLink->Lock);
    LinkLockHeld = TRUE;
    Group = NetpIgmpLookupGroup(IgmpLink, MulticastAddress);
    if (Group == NULL) {
        Status = STATUS_INVALID_ADDRESS;
        goto LeaveMulticastGroupEnd;
    }

    //
    // If this is not the last leave request for the group, the call is
    // successful, but takes no further action. The link remains joined to the
    // multicast group.
    //

    Group->JoinCount -= 1;
    if (Group->JoinCount != 0) {
        goto LeaveMulticastGroupEnd;
    }

    //
    // Otherwise it's time for the group to go.
    //

    LIST_REMOVE(&(Group->ListEntry));
    Group->ListEntry.Next = NULL;
    IgmpLink->GroupCount -= 1;

    //
    // The number of leave messages sent is dictated by the robustness variable.
    //

    Group->SendCount = IgmpLink->RobustnessVariable;

    //
    // Leave messages are state change messages.
    //

    Group->Flags |= IGMP_MULTICAST_GROUP_FLAG_STATE_CHANGE;

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
    // The send count should not have been modified.
    //

    ASSERT(Group->SendCount == IgmpLink->RobustnessVariable);

    //
    // If the link is up, start sending leave messages, up to the robustness
    // count. The group's initial reference will be released after the last
    // leave message is sent.
    //

    NetGetLinkState(IgmpLink->Link, &LinkUp, NULL);
    if (LinkUp != FALSE) {
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

    if (Group != NULL) {
        NetpIgmpGroupReleaseReference(Group);
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

        RobustnessVariable = (QueryV3->Flags &
                              IGMP_QUERY_FLAG_ROBUSTNESS_MASK) >>
                             IGMP_QUERY_FLAG_ROBUSTNESS_SHIFT;

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
        if ((Packet->Flags & NET_PACKET_FLAG_ROUTER_ALERT) == 0) {
            return;
        }
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

                Group->Flags &= ~IGMP_MULTICAST_GROUP_FLAG_STATE_CHANGE;
                if (Group->SendCount == 0) {
                    Group->SendCount = 1;
                }

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

    In router mode, a report should enabling forwarding packets destined for
    the reported multicast group. Router mode is not currently supported.

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
        if ((Packet->Flags & NET_PACKET_FLAG_ROUTER_ALERT) == 0) {
            return;
        }
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
            Group->Flags &= ~IGMP_MULTICAST_GROUP_FLAG_LAST_REPORT;
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
    // The worker thread should only send leave messages after the first leave
    // message is sent by the initial leave request. The group will be
    // destroyed after the last leave message, so don't touch the group
    // structure after the call to send a leave message.
    //

    Group = (PIGMP_MULTICAST_GROUP)Parameter;
    if ((Group->Flags & IGMP_MULTICAST_GROUP_FLAG_LEAVE_SENT) != 0) {
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
    KSTATUS Status;
    UCHAR Type;

    //
    // Reports should be heading to reportable groups only.
    //

    ASSERT(NetpIgmpIsReportableAddress(Group->Address) != FALSE);

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

        NetFreeBuffer(Packet);
        return;
    }

    //
    // Fill out the IGMP header common to all versions and send it on to the
    // common send routine.
    //

    Header->Type = Type;
    Header->MaxResponseCode = 0;
    Header->Checksum = 0;
    Header->Checksum = NetChecksumData((PVOID)Header, BufferSize);
    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    NET_ADD_PACKET_TO_LIST(Packet, &NetPacketList);
    NetpIgmpSendPackets(Group->IgmpLink,
                        (PNETWORK_ADDRESS)&DestinationAddress,
                        &NetPacketList);

    //
    // Note that this link sent the last report for this group, making it on
    // the hook for sending the leave messages. Also test to see whether more
    // join messages need to be sent.
    //

    KeAcquireQueuedLock(Group->IgmpLink->Lock);
    Group->Flags |= IGMP_MULTICAST_GROUP_FLAG_LAST_REPORT;
    if (Group->ListEntry.Next != NULL) {
        Group->SendCount -= 1;
        if (Group->SendCount > 0) {
            NetpIgmpQueueReportTimer(&(Group->Timer),
                                     KeGetRecentTimeCounter(),
                                     IGMP_DEFAULT_UNSOLICITED_REPORT_INTERVAL);
        }
    }

    KeReleaseQueuedLock(Group->IgmpLink->Lock);
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
    KSTATUS Status;
    UCHAR Type;

    DestroyGroup = TRUE;

    //
    // Leave reports should be heading to reportable groups only.
    //

    ASSERT(NetpIgmpIsReportableAddress(Group->Address) != FALSE);

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

        NetFreeBuffer(Packet);
        goto SendGroupLeaveEnd;
    }

    Header->Type = Type;
    Header->MaxResponseCode = 0;
    Header->Checksum = 0;
    Header->Checksum = NetChecksumData((PVOID)Header, BufferSize);
    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    NET_ADD_PACKET_TO_LIST(Packet, &NetPacketList);
    NetpIgmpSendPackets(Group->IgmpLink,
                        (PNETWORK_ADDRESS)&DestinationAddress,
                        &NetPacketList);

    //
    // Note that a leave message has now been sent, allowing the worker to send
    // more leave messages. If the worker were to send leave messages before
    // an initial leave message is sent by the leave request, it may be doing
    // so on behalf of a previous join message. This messes up the send count
    // and reference counting.
    //

    KeAcquireQueuedLock(Group->IgmpLink->Lock);
    Group->Flags |= IGMP_MULTICAST_GROUP_FLAG_LEAVE_SENT;

    ASSERT(Group->SendCount > 0);

    Group->SendCount -= 1;
    if (Group->SendCount > 0) {
        NetpIgmpQueueReportTimer(&(Group->Timer),
                                 KeGetRecentTimeCounter(),
                                 IGMP_DEFAULT_UNSOLICITED_REPORT_INTERVAL);

        DestroyGroup = FALSE;
    }

    KeReleaseQueuedLock(Group->IgmpLink->Lock);

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
    RemainingGroupCount = IgmpLink->GroupCount;
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

            Group = LIST_VALUE(CurrentEntry, IGMP_MULTICAST_GROUP, ListEntry);
            CurrentEntry = CurrentEntry->Next;

            ASSERT(NetpIgmpIsReportableAddress(Group->Address) != FALSE);

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

        Header->Checksum = NetChecksumData((PVOID)Header, BufferSize);
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
    PNET_DATA_LINK_SEND Send;
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

             Checksum = NetChecksumData((PVOID)Header, IGMP_IP4_HEADER_SIZE);
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
        goto IgmpSendPacketsEnd;
    }

    Send = Link->DataLinkEntry->Interface.Send;
    Status = Send(Link->DataLinkContext,
                  PacketList,
                  &(LinkAddress->PhysicalAddress),
                  &DestinationPhysical,
                  IP4_PROTOCOL_NUMBER);

    if (!KSUCCESS(Status)) {
        goto IgmpSendPacketsEnd;
    }

IgmpSendPacketsEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(PacketList);
    }

    return;
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
    PIGMP_LINK NewIgmpLink;
    IGMP_LINK SearchLink;
    KSTATUS Status;

    IgmpLink = NULL;
    NewIgmpLink = MmAllocatePagedPool(sizeof(IGMP_LINK), IGMP_ALLOCATION_TAG);
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
    // Attempt to insert the new IGMP link into the tree. If an existing link
    // is found, use that one and destroy the new one.
    //

    SearchLink.Link = Link;
    KeAcquireSharedExclusiveLockExclusive(NetIgmpLinkLock);
    FoundNode = RtlRedBlackTreeSearch(&NetIgmpLinkTree, &(SearchLink.Node));
    if (FoundNode == NULL) {
        RtlRedBlackTreeInsert(&NetIgmpLinkTree, &(NewIgmpLink->Node));
        IgmpLink = NewIgmpLink;
        NewIgmpLink = NULL;

    } else {
        IgmpLink = RED_BLACK_TREE_VALUE(FoundNode, IGMP_LINK, Node);
    }

    NetpIgmpLinkAddReference(IgmpLink);
    KeReleaseSharedExclusiveLockExclusive(NetIgmpLinkLock);

CreateOrLookupLinkEnd:
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

    ULONG OldReferenceCount;

    //
    // Acquire the tree lock exclusively before decrementing the reference
    // count. This is necessary to make the decrement and removal from the tree
    // atomic.
    //

    KeAcquireSharedExclusiveLockExclusive(NetIgmpLinkLock);
    OldReferenceCount = RtlAtomicAdd32(&(IgmpLink->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    //
    // If the second reference was just released, then the last references is
    // from creation. No multicast groups have a reference on the link and as
    // the tree lock is held exclusively, no other threads have references on
    // the link. Therefore, the link can be removed from the tree.
    //

    if (OldReferenceCount == 2) {

        ASSERT(LIST_EMPTY(&(IgmpLink->MulticastGroupList)) != FALSE);
        ASSERT(IgmpLink->GroupCount == 0);

        RtlRedBlackTreeRemove(&NetIgmpLinkTree, &(IgmpLink->Node));
        IgmpLink->Node.Parent = NULL;
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
    SecondIgmpLink = RED_BLACK_TREE_VALUE(SecondNode, IGMP_LINK, Node);
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
    PIP4_ADDRESS GroupAddress
    )

/*++

Routine Description:

    This routine creats an IGMP multicast group structure.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link to which the multicast group
        will belong.

    GroupAddress - Supplies a pointer to the IPv4 multicast address for the
        group.

Return Value:

    Returns a pointer to the newly allocated multicast group.

--*/

{

    PIGMP_MULTICAST_GROUP Group;
    KSTATUS Status;

    Group = MmAllocatePagedPool(sizeof(IGMP_MULTICAST_GROUP),
                                IGMP_ALLOCATION_TAG);

    if (Group == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateGroupEnd;
    }

    RtlZeroMemory(Group, sizeof(IGMP_MULTICAST_GROUP));
    Group->ReferenceCount = 1;
    Group->JoinCount = 1;
    NetpIgmpLinkAddReference(IgmpLink);
    Group->IgmpLink = IgmpLink;
    Group->Address = GroupAddress->Address;
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

PIGMP_MULTICAST_GROUP
NetpIgmpLookupGroup (
    PIGMP_LINK IgmpLink,
    PIP4_ADDRESS GroupAddress
    )

/*++

Routine Description:

    This routine finds a multicast group with the given address that the given
    link has joined. It takes a reference on the found group.

Arguments:

    IgmpLink - Supplies a pointer to the IGMP link that owns the group to find.

    GroupAddress - Supplies a pointer to the IPv4 multicast address of the
        group.

Return Value:

    Returns a pointer to a multicast group on success or NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PIGMP_MULTICAST_GROUP Group;

    ASSERT(KeIsQueuedLockHeld(IgmpLink->Lock) != FALSE);

    CurrentEntry = IgmpLink->MulticastGroupList.Next;
    while (CurrentEntry != &(IgmpLink->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, IGMP_MULTICAST_GROUP, ListEntry);
        if (Group->Address == GroupAddress->Address) {
            NetpIgmpGroupAddReference(Group);
            return Group;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
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

    Timer->Timer = KeCreateTimer(IGMP_ALLOCATION_TAG);
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
                                       IGMP_ALLOCATION_TAG);

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

BOOL
NetpIgmpIsReportableAddress (
    ULONG GroupAddress
    )

/*++

Routine Description:

    This routine determines whether or not the given group address should be
    reported.

Arguments:

    GroupAddress - Supplies a pointer to the group address to check.

Return Value:

    Returns TRUE if the address should be reported or FALSE otherwise.

--*/

{

    if (GroupAddress == IGMP_ALL_SYSTEMS_ADDRESS) {
        return FALSE;
    }

    return TRUE;
}

