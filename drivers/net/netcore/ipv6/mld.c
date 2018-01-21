/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mld.c

Abstract:

    This module implements support for the Multicast Listener Discovery
    protocol for IPv6.

Author:

    Chris Stevens 29-Aug-2017

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
// While ICMPv6 and MLD are built into netcore and the same binary as IPv6,
// share the well-known addresses. This could easily be changed if the binaries
// need to be separated out.
//

#include "ip6addr.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts MLDv2 response codes to actual time values.
//

#define MLD_CONVERT_RESPONSE_CODE_TO_TIME(_ResponseCode) \
    (((_ResponseCode) < 32768) ?                         \
     (_ResponseCode) :                                   \
     ((((_ResponseCode) & 0x0FFF) | 0x1000) <<           \
      ((((_ResponseCode) >> 12) & 0x7) + 3)))

//
// This macro converts MLDv2 query interval codes to actual time values.
//

#define MLD_CONVERT_INTERVAL_CODE_TO_TIME(_IntervalCode) \
    (((_IntervalCode) < 128) ?                           \
     (_IntervalCode) :                                   \
     ((((_IntervalCode) & 0x0F) | 0x10) <<               \
      ((((_IntervalCode) >> 4) & 0x7) + 3)))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used by MLD.
//

#define MLD_ALLOCATION_TAG 0x21646C4D // '!dlM'

//
// Define the size of the MLD IPv6 headers. Each packet should include a
// hop-by-hop extension header with a router alert option and a Pad-N option
// of size 0.
//

#define MLD_IP6_HEADER_SIZE                   \
    (sizeof(IP6_HEADER) +                     \
     (sizeof(IP6_EXTENSION_HEADER) +          \
      (sizeof(IP6_OPTION) + sizeof(USHORT)) + \
      (sizeof(IP6_OPTION))))

//
// All MLD packets should go out with an IPv6 hop limit of 1.
//

#define MLD_IP6_HOP_LIMIT 1

//
// Define the conversion between query response time units (milliseconds)
// and microseconds.
//

#define MLD_MICROSECONDS_PER_QUERY_TIME_UNIT MICROSECONDS_PER_MILLISECOND

//
// Define the maximum number of address records that can be included in each
// report.
//

#define MLD_MAX_ADDRESS_RECORD_COUNT MAX_USHORT

//
// Define the MLD address record types.
//

#define MLD_ADDRESS_RECORD_TYPE_MODE_IS_INCLUDE 1
#define MLD_ADDRESS_RECORD_TYPE_MODE_IS_EXCLUDE 2
#define MLD_ADDRESS_RECORD_TYPE_CHANGE_TO_INCLUDE_MODE 3
#define MLD_ADDRESS_RECORD_TYPE_CHANGE_TO_EXCLUDE_MODE 4
#define MLD_ADDRESS_RECORD_TYPE_ALLOW_NEW_SOURCES 5
#define MLD_ADDRESS_RECORD_TYPE_BLOCK_OLD_SOURCES 6

//
// Define the MLDv2 query message flag bits.
//

#define MLD_QUERY_FLAG_SUPPRESS_ROUTER_PROCESSING 0x08
#define MLD_QUERY_FLAG_ROBUSTNESS_MASK            0x07
#define MLD_QUERY_FLAG_ROBUSTNESS_SHIFT           0

//
// Define the required number of compatibility modes.
//

#define MLD_COMPATIBILITY_MODE_COUNT 1

//
// Define the default robustness variable.
//

#define MLD_DEFAULT_ROBUSTNESS_VARIABLE 2

//
// Define the default query interval, in seconds.
//

#define MLD_DEFAULT_QUERY_INTERVAL 125

//
// Define the default query response interval, in milliseconds.
//

#define MLD_DEFAULT_MAX_RESPONSE_TIME 10000

//
// Define the default unsolicited report interval in milliseconds
//

#define MLD_DEFAULT_UNSOLICITED_REPORT_INTERVAL 1000

//
// Define the set of multicast group flags.
//

#define MLD_MULTICAST_GROUP_FLAG_LAST_REPORT  0x00000001
#define MLD_MULTICAST_GROUP_FLAG_STATE_CHANGE 0x00000002
#define MLD_MULTICAST_GROUP_FLAG_LEAVE_SENT   0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MLD_VERSION {
    MldVersion1,
    MldVersion2
} MLD_VERSION, *PMLD_VERSION;

/*++

Structure Description:

    This structures defines the MLD message format.

Members:

    MaxResponseCode - Stores the encoded maximum allowed delay,
        in milliseconds, before a node must send a report message in response
        to a query message. This should be set to zero and ignored for
        non-query messages.

    Reserved - Stores 16-bites of reserved space.

    MulticastAddress - Stores the IPv6 multicast address being queried by
        address-specific queries, the address being listened to by report
        senders, and the address no longer being listened to by done messages
        senders. This should be 0 for general query messages.

--*/

#pragma pack(push, 1)

typedef struct _MLD_MESSAGE {
    ICMP6_HEADER Header;
    USHORT MaxResponseCode;
    USHORT Reserved;
    ULONG MulticastAddress[IP6_ADDRESS_SIZE / sizeof(ULONG)];
} PACKED MLD_MESSAGE, *PMLD_MESSAGE;

/*++

Structure Description:

    This structure defines an MLDv2 query message.

Members:

    Message - Stores the base MLD message information, compatible with MLDv1.

    Flags - Stores a bitmask of MLDv2 query flags. See MLD_QUERY_FLAG_* for
        definitions.

    QueryIntervalCode - Stores the encoded query interval of the router.

    SourceAddressCount - Stores the number of source address entries that
        immediately follow this structure.

--*/

typedef struct _MLD2_QUERY {
    MLD_MESSAGE Message;
    UCHAR Flags;
    UCHAR QueryIntervalCode;
    USHORT SourceAddressCount;
} PACKED MLD2_QUERY, *PMLD2_QUERY;

/*++

Structure Description:

    This structure defines an MLDv2 report message.

Members:

    Header - Stores the ICMPv6 message header.

    Reserved - Stores 16-bits of reserved data.

    AddressRecordCount - Stores the number of multicast address records stored
        in the array that begins immediately after this structure.

--*/

typedef struct _MLD2_REPORT {
    ICMP6_HEADER Header;
    USHORT Reserved;
    USHORT AddressRecordCount;
} PACKED MLD2_REPORT, *PMLD2_REPORT;

/*++

Structure Description:

    This structure defines an MLDv2 multicast address record.

Members:

    Type - Stores the multicast address record type.

    DataLength - Stores the length of auxiliary data, in 32-bit words, that
        starts at the end of the source address array.

    SourceAddressCount - Stores the number of source address entries in the
        array that starts at the end of this structure.

    MulticastAddress - Stores the multicast address of the record.

--*/

typedef struct _MLD2_ADDRESS_RECORD {
    UCHAR Type;
    UCHAR DataLength;
    USHORT SourceAddressCount;
    ULONG MulticastAddress[IP6_ADDRESS_SIZE / sizeof(ULONG)];
} PACKED MLD2_ADDRESS_RECORD, *PMLD2_ADDRESS_RECORD;

#pragma pack(pop)

/*++

Structure Description:

    This structure defines an generic MLD timer that kicks off a DPC, which
    then queues a work item.

Members:

    Timer - Stores a pointer to the internal timer.

    Dpc - Stores a pointer to the DPC that executes when the timer expires.

    WorkItem - Stores a pointer to the work item that is scheduled by the DPC.

--*/

typedef struct _MLD_TIMER {
    PKTIMER Timer;
    PDPC Dpc;
    PWORK_ITEM WorkItem;
} MLD_TIMER, *PMLD_TIMER;

/*++

Structure Description:

    This structure defines an MLD link.

Members:

    Node - Stores the link's entry into the global tree of MLD links.

    ReferenceCount - Stores the reference count on the structure.

    Link - Stores a pointer to the network link to which this MLD link is
        bound.

    LinkAddress - Stores a pointer to the network link address entry with which
        the MLD link is associated.

    MaxPacketSize - Stores the maximum MLD packet size that can be sent over
        the link.

    RobustnessVariable - Stores the multicast router's robustness variable.

    QueryInterval - Stores the multicat router's query interval, in seconds.

    MaxResponseTime - Stores the maximum response time for a MLD report, in
        milliseconds.

    Lock - Stores a queued lock that protects the MLD link.

    CompatibilityMode - Stores the current compatibility mode of the MLD link.
        This is based on the type of query messages received on the network.

    CompatibilityTimer - Stores an array of timers for each of the older
        versions of MLD that must be supported.

    ReportTimer - Stores the report timer used for responding to generic
        queries.

    GroupCount - Stores the number of multicast groups that are associated with
        the link and should be reported in a total link report.

    MulticastGroupList - Stores the list of the multicast group structures
        associated with the link.

--*/

typedef struct _MLD_LINK {
    RED_BLACK_TREE_NODE Node;
    volatile ULONG ReferenceCount;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    ULONG MaxPacketSize;
    ULONG RobustnessVariable;
    ULONG QueryInterval;
    ULONG MaxResponseTime;
    PQUEUED_LOCK Lock;
    volatile MLD_VERSION CompatibilityMode;
    MLD_TIMER CompatibilityTimer[MLD_COMPATIBILITY_MODE_COUNT];
    MLD_TIMER ReportTimer;
    ULONG GroupCount;
    LIST_ENTRY MulticastGroupList;
} MLD_LINK, *PMLD_LINK;

/*++

Structure Description:

    This structure defines an MLD multicast group.

Members:

    ListEntry - Stores the group's entry into its parent's list of multicast
        groups.

    ReferenceCount - Stores the reference count on the structure.

    SendCount - Stores the number of pending report or leave messages to be
        sending. This number should always be less than or equal to the
        robustness value. Updates are protected by the IGMP link's queued lock.

    Flags - Stores a bitmask of multicast group flags. See
        MLD_MULTICAST_GROUP_FLAG_* for definitions. Updates are protected by
        the IGMP link's queued lock.

    JoinCount - Stores the number of times a join request has been made for
        this multicast group. This is protected by the MLD link's queued lock.

    Address - Stores the IPv6 multicast address of the group.

    MldLink - Stores a pointer to the MLD link to which this group belongs.

    Timer - Stores the timer used to schedule delayed and repeated MLD report
        and leave messages.

--*/

typedef struct _MLD_MULTICAST_GROUP {
    LIST_ENTRY ListEntry;
    volatile ULONG ReferenceCount;
    ULONG SendCount;
    ULONG Flags;
    ULONG JoinCount;
    ULONG Address[IP6_ADDRESS_SIZE / sizeof(ULONG)];
    PMLD_LINK MldLink;
    MLD_TIMER Timer;
} MLD_MULTICAST_GROUP, *PMLD_MULTICAST_GROUP;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
NetpMldProcessQuery (
    PMLD_LINK Link,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

VOID
NetpMldProcessReport (
    PMLD_LINK Link,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

VOID
NetpMldQueueReportTimer (
    PMLD_TIMER ReportTimer,
    ULONGLONG StartTime,
    ULONG MaxResponseTime
    );

VOID
NetpMldTimerDpcRoutine (
    PDPC Dpc
    );

VOID
NetpMldGroupTimeoutWorker (
    PVOID Parameter
    );

VOID
NetpMldLinkReportTimeoutWorker (
    PVOID Parameter
    );

VOID
NetpMldLinkCompatibilityTimeoutWorker (
    PVOID Parameter
    );

VOID
NetpMldQueueCompatibilityTimer (
    PMLD_LINK MldLink,
    MLD_VERSION CompatibilityMode
    );

VOID
NetpMldUpdateCompatibilityMode (
    PMLD_LINK MldLink
    );

VOID
NetpMldSendGroupReport (
    PMLD_MULTICAST_GROUP Group
    );

VOID
NetpMldSendLinkReport (
    PMLD_LINK Link
    );

VOID
NetpMldSendPackets (
    PMLD_LINK MldLink,
    PNETWORK_ADDRESS Destination,
    PNET_PACKET_LIST PacketList,
    UCHAR Type
    );

VOID
NetpMldSendGroupLeave (
    PMLD_MULTICAST_GROUP Group
    );

PMLD_LINK
NetpMldCreateOrLookupLink (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

VOID
NetpMldDestroyLink (
    PMLD_LINK MldLink
    );

PMLD_LINK
NetpMldLookupLink (
    PNET_LINK Link
    );

VOID
NetpMldLinkAddReference (
    PMLD_LINK MldLink
    );

VOID
NetpMldLinkReleaseReference (
    PMLD_LINK MldLink
    );

COMPARISON_RESULT
NetpMldCompareLinkEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

PMLD_MULTICAST_GROUP
NetpMldCreateGroup (
    PMLD_LINK MldLink,
    PIP6_ADDRESS GroupAddress
    );

VOID
NetpMldDestroyGroup (
    PMLD_MULTICAST_GROUP Group
    );

PMLD_MULTICAST_GROUP
NetpMldLookupGroup (
    PMLD_LINK IgmpLink,
    PIP6_ADDRESS GroupAddress
    );

VOID
NetpMldGroupAddReference (
    PMLD_MULTICAST_GROUP Group
    );

VOID
NetpMldGroupReleaseReference (
    PMLD_MULTICAST_GROUP Group
    );

KSTATUS
NetpMldInitializeTimer (
    PMLD_TIMER Timer,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID WorkParameter
    );

VOID
NetpMldDestroyTimer (
    PMLD_TIMER Timer
    );

BOOL
NetpMldIsReportableAddress (
    PULONG GroupAddress
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Stores a global tree of net links that are signed up for multicast groups
// via MLD.
//

RED_BLACK_TREE NetMldLinkTree;
PSHARED_EXCLUSIVE_LOCK NetMldLinkLock;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpMldInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for the MLD protocol.

Arguments:

    None.

Return Value:

    None.

--*/

{

    RtlRedBlackTreeInitialize(&NetMldLinkTree, 0, NetpMldCompareLinkEntries);
    NetMldLinkLock = KeCreateSharedExclusiveLock();
    if (NetMldLinkLock == NULL) {

        ASSERT(FALSE);

        return;
    }

    return;
}

VOID
NetpMldProcessReceivedData (
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

    PICMP6_HEADER Header;
    PMLD_LINK MldLink;
    PNET_PACKET_BUFFER Packet;
    PIP6_ADDRESS Source;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // All messages should come from link-local source addresses.
    //

    Source = (PIP6_ADDRESS)ReceiveContext->Source;
    if (IP6_IS_UNICAST_LINK_LOCAL_ADDRESS(Source->Address) == FALSE) {
        return;
    }

    //
    // Do nothing if this link is not registered with MLD. The packet is likely
    // old.
    //

    MldLink = NetpMldLookupLink(ReceiveContext->Link);
    if (MldLink == NULL) {
        goto MldProcessReceivedDataEnd;
    }

    //
    // Handle the MLD packet based on the ICMPv6 type field. ICMPv6 already
    // validated the header and its checksum.
    //

    Packet = ReceiveContext->Packet;
    Header = (PICMP6_HEADER)(Packet->Buffer + Packet->DataOffset);
    switch (Header->Type) {
    case ICMP6_MESSAGE_TYPE_MLD_QUERY:
        NetpMldProcessQuery(MldLink,
                            Packet,
                            ReceiveContext->Source,
                            ReceiveContext->Destination);

        break;

    case ICMP6_MESSAGE_TYPE_MLD_REPORT:
    case ICMP6_MESSAGE_TYPE_MLD2_REPORT:
        NetpMldProcessReport(MldLink,
                             Packet,
                             ReceiveContext->Source,
                             ReceiveContext->Destination);

        break;

    //
    // A done message should only be handled by a router.
    //

    case ICMP6_MESSAGE_TYPE_MLD_DONE:
        break;

    default:
        break;
    }

MldProcessReceivedDataEnd:
    if (MldLink != NULL) {
        NetpMldLinkReleaseReference(MldLink);
    }

    return;
}

KSTATUS
NetpMldJoinMulticastGroup (
    PNET_NETWORK_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine joins the multicast group on the network link provided in the
    request. If this is the first request to join the supplied multicast group
    on the specified link, then an MLD report is sent out over the network.

Arguments:

    Request - Supplies a pointer to a multicast request, which includes the
        address of the group to join and the network link on which to join the
        group.

Return Value:

    Status code.

--*/

{

    PMLD_MULTICAST_GROUP Group;
    PIP6_ADDRESS GroupAddress;
    BOOL LinkLockHeld;
    PMLD_LINK MldLink;
    PMLD_MULTICAST_GROUP NewGroup;
    KSTATUS Status;

    Group = NULL;
    LinkLockHeld = FALSE;
    GroupAddress = (PIP6_ADDRESS)Request->MulticastAddress;
    NewGroup = NULL;

    //
    // If the group never needs to be reported, don't bother to record it at
    // this layer. Netcore already has a record of it.
    //

    if (NetpMldIsReportableAddress(GroupAddress->Address) == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Test to see if there is an MLD link for the given network link, creating
    // one if the lookup fails.
    //

    MldLink = NetpMldLookupLink(Request->Link);
    if (MldLink == NULL) {
        MldLink = NetpMldCreateOrLookupLink(Request->Link,
                                            Request->LinkAddress);

        if (MldLink == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto JoinMulticastGroupEnd;
        }
    }

    //
    // Search the MLD link for the multicast group. If a matching group is not
    // found then release the lock, allocate a group and search again. If the
    // group is still not found, add the newly allocated group.
    //

    Status = STATUS_SUCCESS;
    while (TRUE) {
        KeAcquireQueuedLock(MldLink->Lock);
        LinkLockHeld = TRUE;
        Group = NetpMldLookupGroup(MldLink, GroupAddress);
        if (Group != NULL) {
            Group->JoinCount += 1;
            goto JoinMulticastGroupEnd;
        }

        if (NewGroup == NULL) {
            KeReleaseQueuedLock(MldLink->Lock);
            LinkLockHeld = FALSE;
            NewGroup = NetpMldCreateGroup(MldLink, GroupAddress);
            if (NewGroup == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto JoinMulticastGroupEnd;
            }

            continue;
        }

        //
        // Add the newly allocated group to the link's list.
        //

        INSERT_BEFORE(&(NewGroup->ListEntry), &(MldLink->MulticastGroupList));
        MldLink->GroupCount += 1;
        break;
    }

    //
    // Initialize the send count to the robustness variable. This will cause
    // multiple join messages to be sent, up to the robustness count.
    //

    NewGroup->SendCount = MldLink->RobustnessVariable;

    //
    // An initial join sends state change messages and at least one message
    // will be sent, so start the group as the last reporter.
    //

    NewGroup->Flags |= MLD_MULTICAST_GROUP_FLAG_STATE_CHANGE |
                       MLD_MULTICAST_GROUP_FLAG_LAST_REPORT;

    //
    // Take an extra reference on the new group so that it is not destroyed
    // while sending the report. Once the lock is released, a leave request
    // could run through and attempt to take it down.
    //

    NetpMldGroupAddReference(NewGroup);
    KeReleaseQueuedLock(MldLink->Lock);
    LinkLockHeld = FALSE;

    //
    // Actually send out the group's join MLD state change messages.
    //

    NetpMldSendGroupReport(NewGroup);

JoinMulticastGroupEnd:
    if (LinkLockHeld != FALSE) {
        KeReleaseQueuedLock(MldLink->Lock);
    }

    if (MldLink != NULL) {
        NetpMldLinkReleaseReference(MldLink);
    }

    if (NewGroup != NULL) {
        NetpMldGroupReleaseReference(NewGroup);
    }

    if (Group != NULL) {
        NetpMldGroupReleaseReference(Group);
    }

    return Status;
}

KSTATUS
NetpMldLeaveMulticastGroup (
    PNET_NETWORK_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine removes the local system from a multicast group. If this is
    the last request to leave a multicast group on the link, then a MLD leave
    message is sent out over the network.

Arguments:

    Request - Supplies a pointer to a multicast request, which includes the
        address of the group to leave and the network link that has previously
        joined the group.

Return Value:

    Status code.

--*/

{

    PMLD_MULTICAST_GROUP Group;
    BOOL LinkLockHeld;
    BOOL LinkUp;
    PMLD_LINK MldLink;
    PIP6_ADDRESS MulticastAddress;
    KSTATUS Status;

    Group = NULL;
    MldLink = NULL;
    LinkLockHeld = FALSE;
    MulticastAddress = (PIP6_ADDRESS)Request->MulticastAddress;

    //
    // If the address is not reportable, an MLD group was never made for it.
    //

    if (NetpMldIsReportableAddress(MulticastAddress->Address) == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Now see if there is an MLD link for the given network link.
    //

    MldLink = NetpMldLookupLink(Request->Link);
    if (MldLink == NULL) {
        Status = STATUS_INVALID_ADDRESS;
        goto LeaveMulticastGroupEnd;
    }

    //
    // Search the MLD link for the multicast group. If a matching group is not
    // found then the request fails.
    //

    KeAcquireQueuedLock(MldLink->Lock);
    LinkLockHeld = TRUE;
    Group = NetpMldLookupGroup(MldLink, MulticastAddress);
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
    MldLink->GroupCount -= 1;

    //
    // The number of leave messages sent is dictated by the robustness variable.
    //

    Group->SendCount = MldLink->RobustnessVariable;

    //
    // Leave messages are state change messages.
    //

    Group->Flags |= MLD_MULTICAST_GROUP_FLAG_STATE_CHANGE;

    //
    // Release the lock and flush out any reports that may be in the works.
    //

    KeReleaseQueuedLock(MldLink->Lock);
    LinkLockHeld = FALSE;
    KeCancelTimer(Group->Timer.Timer);
    KeFlushDpc(Group->Timer.Dpc);
    KeCancelWorkItem(Group->Timer.WorkItem);
    KeFlushWorkItem(Group->Timer.WorkItem);

    //
    // The send count should not have been modified.
    //

    ASSERT(Group->SendCount == MldLink->RobustnessVariable);

    //
    // If the link is up, start sending leave messages, up to the robustness
    // count. The group's initial reference will be released after the last
    // leave message is sent.
    //

    NetGetLinkState(MldLink->Link, &LinkUp, NULL);
    if (LinkUp != FALSE) {
        NetpMldSendGroupLeave(Group);

    //
    // Otherwise don't bother with the leave messages and just destroy the
    // group immediately.
    //

    } else {
        NetpMldGroupReleaseReference(Group);
    }

LeaveMulticastGroupEnd:
    if (LinkLockHeld != FALSE) {
        KeReleaseQueuedLock(MldLink->Lock);
    }

    if (MldLink != NULL) {
        NetpMldLinkReleaseReference(MldLink);
    }

    if (Group != NULL) {
        NetpMldGroupReleaseReference(Group);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpMldProcessQuery (
    PMLD_LINK MldLink,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine processes an MLD query message.

    In host mode, this generates a report for each multicast group to which the
    receiving link belongs.

    In router mode, a query message indicates that there is another multicast
    router on the local network. If this link has a higher IP address than the
    sender, this link will not send queries until the "other querier present
    interval" expires. Router mode is not currently supported.

Arguments:

    MldLink - Supplies a pointer to the MLD link that received the packet.

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
    PIP6_ADDRESS Destination;
    BOOL Equal;
    BOOL GeneralQuery;
    PMLD_MULTICAST_GROUP Group;
    ULONG Length;
    UCHAR MaxResponseCode;
    ULONG MaxResponseTime;
    PMLD_MESSAGE Query;
    ULONG QueryInterval;
    PMLD2_QUERY QueryV2;
    ULONG RobustnessVariable;
    MLD_VERSION Version;

    Destination = (PIP6_ADDRESS)DestinationAddress;

    //
    // Determine which version of query message was received. An 8 octet long
    // message with a max response code of 0 is an MLDv1 query message. An 8
    // octet long message with a non-zero max response code is an MLDv2 query
    // message. A message with a length greater than or equal to 12 octets is
    // an MLDv3 query message. Any other message must be ignored.
    //

    Query = (PMLD_MESSAGE)(Packet->Buffer + Packet->DataOffset);
    Length = Packet->FooterOffset - Packet->DataOffset;
    MaxResponseCode = Query->Header.Code;
    Version = MldVersion2;
    if (Length == sizeof(MLD_MESSAGE)) {
        Version = MldVersion1;
        NetpMldQueueCompatibilityTimer(MldLink, Version);

    } else if (Length >= sizeof(MLD2_QUERY)) {
        QueryV2 = (PMLD2_QUERY)Query;
        QueryInterval = MLD_CONVERT_INTERVAL_CODE_TO_TIME(
                                                   QueryV2->QueryIntervalCode);

        RobustnessVariable = (QueryV2->Flags &
                              MLD_QUERY_FLAG_ROBUSTNESS_MASK) >>
                             MLD_QUERY_FLAG_ROBUSTNESS_SHIFT;

        //
        // Update the query interval and robustness variable if they are
        // non-zero.
        //

        if (QueryInterval != 0) {
            MldLink->QueryInterval = QueryInterval;
        }

        if (RobustnessVariable != 0) {
            MldLink->RobustnessVariable = RobustnessVariable;
        }

    } else {
        return;
    }

    //
    // Version 2 queries with a hop limit greater than 1 or without the
    // router-alert option should be ignored for security reasons.
    //

    if (Version == MldVersion2) {
        if (((Packet->Flags & NET_PACKET_FLAG_LINK_LOCAL_HOP_LIMIT) == 0) ||
            ((Packet->Flags & NET_PACKET_FLAG_ROUTER_ALERT) == 0)) {

            return;
        }
    }

    //
    // All general queries not sent to the all-nodes multicast address
    // (FF02::1) should be ignored for security reasons.
    //

    GeneralQuery = IP6_IS_UNSPECIFIED_ADDRESS(Query->MulticastAddress);
    if (GeneralQuery != FALSE) {
        Equal = RtlCompareMemory(Destination->Address,
                                 NetIp6AllNodesMulticastAddress,
                                 IP6_ADDRESS_SIZE);

        if (Equal == FALSE) {
            return;
        }
    }

    //
    // Ignore queries that target the all-nodes multicast address. No reports
    // are supposed to be sent for the all systems address, making a query
    // quite mysterious.
    //

    Equal = RtlCompareMemory(Query->MulticastAddress,
                             NetIp6AllNodesMulticastAddress,
                             IP6_ADDRESS_SIZE);

    if (Equal != FALSE) {
        return;
    }

    //
    // Calculate the maximum response time. For query messages, the time unit
    // is in milliseconds.
    //

    MaxResponseTime = MLD_CONVERT_RESPONSE_CODE_TO_TIME(MaxResponseCode);

    //
    // The reports are not sent immediately, but delayed based on the max
    // response code.
    //

    KeAcquireQueuedLock(MldLink->Lock);

    //
    // Always save the max response time.
    //

    MldLink->MaxResponseTime = MaxResponseTime;

    //
    // If the host is operating in MLDv2 mode and this is a general query, set
    // the global report timer. MLDv2 can send one report that includes
    // information for all of the host's multicast memberships.
    //

    CurrentTime = KeGetRecentTimeCounter();
    if ((MldLink->CompatibilityMode == MldVersion2) &&
        (GeneralQuery != FALSE)) {

        NetpMldQueueReportTimer(&(MldLink->ReportTimer),
                                 CurrentTime,
                                 MaxResponseTime);

    //
    // Otherwise, iterate over the list of multicast groups to which this
    // link subscribes and update the timer for each group that matches the
    // query's group address - or all groups if it is a general query.
    //

    } else {
        CurrentEntry = MldLink->MulticastGroupList.Next;
        while (CurrentEntry != &(MldLink->MulticastGroupList)) {
            Group = LIST_VALUE(CurrentEntry, MLD_MULTICAST_GROUP, ListEntry);
            Equal = FALSE;
            if (GeneralQuery == FALSE) {
                Equal = RtlCompareMemory(Query->MulticastAddress,
                                         Group->Address,
                                         IP6_ADDRESS_SIZE);

            }

            if ((GeneralQuery != FALSE) || (Equal != FALSE)) {
                Group->Flags &= ~MLD_MULTICAST_GROUP_FLAG_STATE_CHANGE;
                if (Group->SendCount == 0) {
                    Group->SendCount = 1;
                }

                NetpMldQueueReportTimer(&(Group->Timer),
                                         CurrentTime,
                                         MaxResponseTime);
            }

            CurrentEntry = CurrentEntry->Next;
        }
    }

    KeReleaseQueuedLock(MldLink->Lock);
    return;
}

VOID
NetpMldProcessReport (
    PMLD_LINK MldLink,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine processes an MLD report message.

    In host mode, this cancels any pending report messages for the reported
    multicast group. A router only needs to receive one report per multicast
    group on the local physical network. It does not need to know which
    specific hosts are subcribed to a group, just that at least one host is
    subscribed to a group.

    In router mode, a report should enabling forwarding packets destined for
    the reported multicast group. Router mode is not currently supported.

Arguments:

    MldLink - Supplies a pointer to the MLD link that received the packet.

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
    PIP6_ADDRESS Destination;
    BOOL Equal;
    PMLD_MULTICAST_GROUP Group;
    ULONG Length;
    PMLD_MESSAGE Report;

    //
    // MLDv2 reports are always ignored by hosts.
    //

    Report = (PMLD_MESSAGE)(Packet->Buffer + Packet->DataOffset);
    Length = Packet->FooterOffset - Packet->DataOffset;
    if (Length != sizeof(MLD_MESSAGE)) {
        return;
    }

    //
    // Version 2 reports without the router-alert option and a hop limit of 1
    // should be ignored for security reasons.
    //

    if (Report->Header.Type == ICMP6_MESSAGE_TYPE_MLD2_REPORT) {
        if (((Packet->Flags & NET_PACKET_FLAG_LINK_LOCAL_HOP_LIMIT) == 0) ||
            ((Packet->Flags & NET_PACKET_FLAG_ROUTER_ALERT) == 0)) {

            return;
        }
    }

    //
    // The report should have been sent to the multicast group it was reporting
    // on.
    //

    Destination = (PIP6_ADDRESS)DestinationAddress;
    Equal = RtlCompareMemory(Destination->Address,
                             Report->MulticastAddress,
                             IP6_ADDRESS_SIZE);

    if ((Equal == FALSE) ||
        (IP6_IS_UNSPECIFIED_ADDRESS(Destination->Address) != FALSE)) {

        return;
    }

    //
    // If this MLD link belongs to the multicast group, cancel any pending
    // reports and record that this link was not the last to send a report.
    //

    KeAcquireQueuedLock(MldLink->Lock);
    CurrentEntry = MldLink->MulticastGroupList.Next;
    while (CurrentEntry != &(MldLink->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, MLD_MULTICAST_GROUP, ListEntry);
        Equal = RtlCompareMemory(Report->MulticastAddress,
                                 Group->Address,
                                 IP6_ADDRESS_SIZE);

        if (Equal != FALSE) {
            KeCancelTimer(Group->Timer.Timer);
            Group->Flags &= ~MLD_MULTICAST_GROUP_FLAG_LAST_REPORT;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseQueuedLock(MldLink->Lock);
    return;
}

VOID
NetpMldQueueReportTimer (
    PMLD_TIMER ReportTimer,
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

    MaxResponseTime - Supplies the maximum responce time supplied by the MLD
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
    DelayInMicroseconds = Delay * MLD_MICROSECONDS_PER_QUERY_TIME_UNIT;
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
NetpMldTimerDpcRoutine (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the MLD timer DPC that gets called after a timer
    expires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PMLD_TIMER ReportTimer;

    ReportTimer = (PMLD_TIMER)Dpc->UserData;
    KeQueueWorkItem(ReportTimer->WorkItem);
    return;
}

VOID
NetpMldGroupTimeoutWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs the low level work when an MLD group report timer
    expires. It sends a report or leave message for the group.

Arguments:

    Parameter - Supplies a pointer to the MLD group whose timer expired.

Return Value:

    None.

--*/

{

    PMLD_MULTICAST_GROUP Group;

    //
    // The worker thread should only send leave messages after the first leave
    // message is sent by the initial leave request. The group will be
    // destroyed after the last leave message, so don't touch the group
    // structure after the call to send a leave message.
    //

    Group = (PMLD_MULTICAST_GROUP)Parameter;
    if ((Group->Flags & MLD_MULTICAST_GROUP_FLAG_LEAVE_SENT) != 0) {
        NetpMldSendGroupLeave(Group);

    //
    // Otherwise the timer has expired to send a simple group report.
    //

    } else {
        NetpMldSendGroupReport(Group);
    }

    return;
}

VOID
NetpMldLinkReportTimeoutWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs the low level work when an MLD link report timer
    expires. It sends a MLDv3 report message for all groups.

Arguments:

    Parameter - Supplies a pointer to the MLD link whose link report timer
        expired.

Return Value:

    None.

--*/

{

    PMLD_LINK MldLink;

    MldLink = (PMLD_LINK)Parameter;
    NetpMldSendLinkReport(MldLink);
    return;
}

VOID
NetpMldLinkCompatibilityTimeoutWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine performs the low level work when a compatibility mode timer
    expires. It determines the new compatability mode.

Arguments:

    Parameter - Supplies a pointer to the MLD link whose compatibility timer
        expired.

Return Value:

    None.

--*/

{

    PMLD_LINK MldLink;

    MldLink = (PMLD_LINK)Parameter;
    KeAcquireQueuedLock(MldLink->Lock);
    NetpMldUpdateCompatibilityMode(MldLink);
    KeReleaseQueuedLock(MldLink->Lock);
    return;
}

VOID
NetpMldQueueCompatibilityTimer (
    PMLD_LINK MldLink,
    MLD_VERSION CompatibilityMode
    )

/*++

Routine Description:

    This routine queues an MLD compatibility timer for the given mode.

Arguments:

    MldLink - Supplies a pointer to MLD link whose compatibility timer needs
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
    PMLD_TIMER Timer;

    //
    // The compatibility mode interval is calculated as follows:
    //
    // (Robustness Variable * Query Interval) + (Query Response Interval)
    //
    // The Query Response Interval is the same as the maximum response time
    // provided by the last query.
    //

    DelayInMicroseconds = MldLink->RobustnessVariable *
                          MldLink->QueryInterval *
                          MICROSECONDS_PER_SECOND;

    DelayInMicroseconds += MldLink->MaxResponseTime *
                           MLD_MICROSECONDS_PER_QUERY_TIME_UNIT;

    Timer = &(MldLink->CompatibilityTimer[CompatibilityMode]);
    StartTime = KeGetRecentTimeCounter();
    DueTime = StartTime + KeConvertMicrosecondsToTimeTicks(DelayInMicroseconds);

    //
    // If the timer is already scheduled, then it needs to be extended for
    // another compatibility timeout interval. Cancel it and requeue it. It's
    // OK if the DPC fires the work item in the meantime. The correct mode will
    // be set once the lock can be acquired by the work item.
    //

    KeAcquireQueuedLock(MldLink->Lock);
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

    NetpMldUpdateCompatibilityMode(MldLink);
    KeReleaseQueuedLock(MldLink->Lock);
    return;
}

VOID
NetpMldUpdateCompatibilityMode (
    PMLD_LINK MldLink
    )

/*++

Routine Description:

    This routine updates the given MLD link's compatibility mode based on the
    state of the compatibility timers. It assumes the MLD link's lock is held.

Arguments:

    MldLink - Supplies a pointer to an MLD link.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONGLONG DueTime;
    PMLD_MULTICAST_GROUP Group;
    MLD_VERSION ModeIndex;
    MLD_VERSION NewMode;
    PMLD_TIMER Timer;

    ASSERT(KeIsQueuedLockHeld(MldLink->Lock) != FALSE);

    NewMode = MldVersion2;
    for (ModeIndex = MldVersion1;
         ModeIndex < MLD_COMPATIBILITY_MODE_COUNT;
         ModeIndex += 1) {

        Timer = &(MldLink->CompatibilityTimer[ModeIndex]);
        DueTime = KeGetTimerDueTime(Timer->Timer);
        if (DueTime != 0) {
            NewMode = ModeIndex;
            break;
        }
    }

    //
    // If compatibility mode is about to change, cancel all pending timers.
    //

    if (NewMode != MldLink->CompatibilityMode) {
        KeCancelTimer(MldLink->ReportTimer.Timer);
        CurrentEntry = MldLink->MulticastGroupList.Next;
        while (CurrentEntry != &(MldLink->MulticastGroupList)) {
            Group = LIST_VALUE(CurrentEntry, MLD_MULTICAST_GROUP, ListEntry);
            KeCancelTimer(Group->Timer.Timer);
            CurrentEntry = CurrentEntry->Next;
        }
    }

    MldLink->CompatibilityMode = NewMode;
    return;
}

VOID
NetpMldSendGroupReport (
    PMLD_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine sends a MLD report message for a specific multicast group.

Arguments:

    Group - Supplies a pointer to the multicast group to report.

Return Value:

    None.

--*/

{

    PMLD2_ADDRESS_RECORD AddressRecord;
    ULONG BufferFlags;
    ULONG BufferSize;
    MLD_VERSION CompatibilityMode;
    IP6_ADDRESS Destination;
    PMLD_MESSAGE Message;
    NET_PACKET_LIST NetPacketList;
    PNET_PACKET_BUFFER Packet;
    UCHAR RecordType;
    PMLD2_REPORT Report;
    KSTATUS Status;
    UCHAR Type;

    //
    // Reports should be heading to reportable groups only.
    //

    ASSERT(NetpMldIsReportableAddress(Group->Address) != FALSE);

    //
    // Snap the compatibility mode.
    //

    CompatibilityMode = Group->MldLink->CompatibilityMode;
    if (CompatibilityMode == MldVersion2) {
        BufferSize = sizeof(MLD2_REPORT) + sizeof(MLD2_ADDRESS_RECORD);

        ASSERT(BufferSize <= Group->MldLink->MaxPacketSize);

    } else {
        BufferSize = sizeof(MLD_MESSAGE);
    }

    BufferFlags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Status = NetAllocateBuffer(MLD_IP6_HEADER_SIZE,
                               BufferSize,
                               0,
                               Group->MldLink->Link,
                               BufferFlags,
                               &Packet);

    if (!KSUCCESS(Status)) {
        return;
    }

    Destination.Domain = NetDomainIp6;
    Message = (PMLD_MESSAGE)(Packet->Buffer + Packet->DataOffset);
    switch (CompatibilityMode) {
    case MldVersion2:
        Type = ICMP6_MESSAGE_TYPE_MLD2_REPORT;
        RtlCopyMemory(Destination.Address,
                      NetIp6AllMld2RoutersMulticastAddress,
                      IP6_ADDRESS_SIZE);

        Report = (PMLD2_REPORT)Message;
        Report->Reserved = 0;
        Report->AddressRecordCount = CPU_TO_NETWORK16(1);
        AddressRecord = (PMLD2_ADDRESS_RECORD)(Report + 1);
        if ((Group->Flags & MLD_MULTICAST_GROUP_FLAG_STATE_CHANGE) != 0) {
            RecordType = MLD_ADDRESS_RECORD_TYPE_CHANGE_TO_EXCLUDE_MODE;

        } else {
            RecordType = MLD_ADDRESS_RECORD_TYPE_MODE_IS_EXCLUDE;
        }

        AddressRecord->Type = RecordType;
        AddressRecord->DataLength = 0;
        AddressRecord->SourceAddressCount = CPU_TO_NETWORK16(0);
        RtlCopyMemory(AddressRecord->MulticastAddress,
                      Group->Address,
                      IP6_ADDRESS_SIZE);

        break;

    case MldVersion1:
        Type = ICMP6_MESSAGE_TYPE_MLD_REPORT;
        RtlCopyMemory(Message->MulticastAddress,
                      Group->Address,
                      IP6_ADDRESS_SIZE);

        RtlCopyMemory(Destination.Address,
                      Group->Address,
                      IP6_ADDRESS_SIZE);

        break;

    default:

        ASSERT(FALSE);

        NetFreeBuffer(Packet);
        return;
    }

    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    NET_ADD_PACKET_TO_LIST(Packet, &NetPacketList);
    NetpMldSendPackets(Group->MldLink,
                       (PNETWORK_ADDRESS)&Destination,
                       &NetPacketList,
                       Type);

    //
    // Note that this link sent the last report for this group, making it on
    // the hook for sending the leave messages. Also test to see whether more
    // join messages need to be sent.
    //

    KeAcquireQueuedLock(Group->MldLink->Lock);
    Group->Flags |= MLD_MULTICAST_GROUP_FLAG_LAST_REPORT;
    if (Group->ListEntry.Next != NULL) {
        Group->SendCount -= 1;
        if (Group->SendCount > 0) {
            NetpMldQueueReportTimer(&(Group->Timer),
                                    KeGetRecentTimeCounter(),
                                    MLD_DEFAULT_UNSOLICITED_REPORT_INTERVAL);
        }
    }

    KeReleaseQueuedLock(Group->MldLink->Lock);
    return;
}

VOID
NetpMldSendGroupLeave (
    PMLD_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine sends an MLD leave message to the all routers multicast group.

Arguments:

    Group - Supplies a pointer to the multicast group that the host is leaving.

Return Value:

    None.

--*/

{

    PMLD2_ADDRESS_RECORD AddressRecord;
    ULONG BufferFlags;
    ULONG BufferSize;
    MLD_VERSION CompatibilityMode;
    IP6_ADDRESS Destination;
    BOOL DestroyGroup;
    PMLD_MESSAGE Message;
    NET_PACKET_LIST NetPacketList;
    PNET_PACKET_BUFFER Packet;
    PMLD2_REPORT Report;
    KSTATUS Status;
    UCHAR Type;

    DestroyGroup = TRUE;

    //
    // Leave reports should be heading to reportable groups only.
    //

    ASSERT(NetpMldIsReportableAddress(Group->Address) != FALSE);

    //
    // If this link was not the last to report the group, then don't send
    // a done message.
    //

    if ((Group->Flags & MLD_MULTICAST_GROUP_FLAG_LAST_REPORT) == 0) {
        goto SendGroupLeaveEnd;
    }

    //
    // Snap the current compatibility mode.
    //

    CompatibilityMode = Group->MldLink->CompatibilityMode;
    if (CompatibilityMode == MldVersion1) {
        BufferSize = sizeof(MLD_MESSAGE);

    } else {
        BufferSize = sizeof(MLD2_REPORT) + sizeof(MLD2_ADDRESS_RECORD);

        ASSERT(CompatibilityMode == MldVersion2);
        ASSERT(BufferSize <= Group->MldLink->MaxPacketSize);
    }

    BufferFlags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Status = NetAllocateBuffer(MLD_IP6_HEADER_SIZE,
                               BufferSize,
                               0,
                               Group->MldLink->Link,
                               BufferFlags,
                               &Packet);

    if  (!KSUCCESS(Status)) {
        goto SendGroupLeaveEnd;
    }

    Destination.Domain = NetDomainIp6;
    Message = (PMLD_MESSAGE)(Packet->Buffer + Packet->DataOffset);
    switch (CompatibilityMode) {
    case MldVersion2:
        Type = ICMP6_MESSAGE_TYPE_MLD2_REPORT;
        RtlCopyMemory(Destination.Address,
                      NetIp6AllMld2RoutersMulticastAddress,
                      IP6_ADDRESS_SIZE);

        Report = (PMLD2_REPORT)Message;
        Report->Reserved = 0;
        Report->AddressRecordCount = CPU_TO_NETWORK16(1);
        AddressRecord = (PMLD2_ADDRESS_RECORD)(Report + 1);
        AddressRecord->Type = MLD_ADDRESS_RECORD_TYPE_CHANGE_TO_INCLUDE_MODE;
        AddressRecord->DataLength = 0;
        AddressRecord->SourceAddressCount = CPU_TO_NETWORK16(0);
        RtlCopyMemory(AddressRecord->MulticastAddress,
                      Group->Address,
                      IP6_ADDRESS_SIZE);

        break;

    case MldVersion1:
        Type = ICMP6_MESSAGE_TYPE_MLD_DONE;
        Message = (PMLD_MESSAGE)Message;
        RtlCopyMemory(Message->MulticastAddress,
                      Group->Address,
                      IP6_ADDRESS_SIZE);

        RtlCopyMemory(Destination.Address,
                      NetIp6AllRoutersMulticastAddress,
                      IP6_ADDRESS_SIZE);

        break;

    default:

        ASSERT(FALSE);

        NetFreeBuffer(Packet);
        goto SendGroupLeaveEnd;
    }

    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    NET_ADD_PACKET_TO_LIST(Packet, &NetPacketList);
    NetpMldSendPackets(Group->MldLink,
                       (PNETWORK_ADDRESS)&Destination,
                       &NetPacketList,
                       Type);

    //
    // Note that a leave message has now been sent, allowing the worker to send
    // more leave messages. If the worker were to send leave messages before
    // an initial leave message is sent by the leave request, it may be doing
    // so on behalf of a previous join message. This messes up the send count
    // and reference counting.
    //

    KeAcquireQueuedLock(Group->MldLink->Lock);
    Group->Flags |= MLD_MULTICAST_GROUP_FLAG_LEAVE_SENT;

    ASSERT(Group->SendCount > 0);

    Group->SendCount -= 1;
    if (Group->SendCount > 0) {
        NetpMldQueueReportTimer(&(Group->Timer),
                                KeGetRecentTimeCounter(),
                                MLD_DEFAULT_UNSOLICITED_REPORT_INTERVAL);

        DestroyGroup = FALSE;
    }

    KeReleaseQueuedLock(Group->MldLink->Lock);

SendGroupLeaveEnd:
    if (DestroyGroup != FALSE) {
        NetpMldGroupReleaseReference(Group);
    }

    return;
}

VOID
NetpMldSendLinkReport (
    PMLD_LINK MldLink
    )

/*++

Routine Description:

    This routine sends a MLD report message for the whole link.

Arguments:

    MldLink - Supplies a pointer to the MLD link to report.

Return Value:

    None.

--*/

{

    PMLD2_ADDRESS_RECORD AddressRecord;
    ULONG BufferFlags;
    ULONG BufferSize;
    PLIST_ENTRY CurrentEntry;
    ULONG CurrentRecordCount;
    IP6_ADDRESS Destination;
    PMLD_MULTICAST_GROUP Group;
    NET_PACKET_LIST NetPacketList;
    PNET_PACKET_BUFFER Packet;
    ULONG RecordSize;
    ULONG RemainingRecordCount;
    PMLD2_REPORT Report;
    USHORT SourceAddressCount;
    KSTATUS Status;

    //
    // Send as many MLDv2 "Current-State" records as required to notify the
    // all MLDv2-capable routers group of all the multicast groups to which the
    // given link belongs. This may take more than one packet if the link is
    // subscribed to more than MAX_USHORT groups or if the number of groups
    // requires a packet larger than the link's max transfer size.
    //

    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    KeAcquireQueuedLock(MldLink->Lock);
    RemainingRecordCount = MldLink->GroupCount;
    CurrentEntry = MldLink->MulticastGroupList.Next;
    while (RemainingRecordCount != 0) {
        CurrentRecordCount = RemainingRecordCount;
        if (CurrentRecordCount > MLD_MAX_ADDRESS_RECORD_COUNT) {
            CurrentRecordCount = MLD_MAX_ADDRESS_RECORD_COUNT;
        }

        BufferSize = sizeof(MLD2_REPORT) +
                     (sizeof(MLD2_ADDRESS_RECORD) * CurrentRecordCount);

        if (BufferSize > MldLink->MaxPacketSize) {
            BufferSize = MldLink->MaxPacketSize;
            CurrentRecordCount = (BufferSize - sizeof(MLD2_REPORT)) /
                                sizeof(MLD2_ADDRESS_RECORD);
        }

        RemainingRecordCount -= CurrentRecordCount;
        BufferFlags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
                      NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
                      NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                      NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

        Status = NetAllocateBuffer(MLD_IP6_HEADER_SIZE,
                                   BufferSize,
                                   0,
                                   MldLink->Link,
                                   BufferFlags,
                                   &Packet);

        if (!KSUCCESS(Status)) {
            break;
        }

        Report = (PMLD2_REPORT)Packet->Buffer + Packet->DataOffset;
        Report->Reserved = 0;
        Report->AddressRecordCount = CPU_TO_NETWORK16(CurrentRecordCount);
        AddressRecord = (PMLD2_ADDRESS_RECORD)(Report + 1);
        while (CurrentRecordCount != 0) {

            ASSERT(CurrentEntry != &(MldLink->MulticastGroupList));

            Group = LIST_VALUE(CurrentEntry, MLD_MULTICAST_GROUP, ListEntry);
            CurrentEntry = CurrentEntry->Next;

            ASSERT(NetpMldIsReportableAddress(Group->Address) != FALSE);

            CurrentRecordCount -= 1;

            //
            // The count should be accurate and eliminate the need to check for
            // the head.
            //

            AddressRecord->Type = MLD_ADDRESS_RECORD_TYPE_MODE_IS_EXCLUDE;
            AddressRecord->DataLength = 0;
            SourceAddressCount = 0;
            AddressRecord->SourceAddressCount =
                                          CPU_TO_NETWORK16(SourceAddressCount);

            RtlCopyMemory(AddressRecord->MulticastAddress,
                          Group->Address,
                          IP6_ADDRESS_SIZE);

            RecordSize = sizeof(MLD2_ADDRESS_RECORD) +
                        (SourceAddressCount * sizeof(ULONG)) +
                        AddressRecord->DataLength;

            AddressRecord =
                    (PMLD2_ADDRESS_RECORD)((PUCHAR)AddressRecord + RecordSize);
        }

        NET_ADD_PACKET_TO_LIST(Packet, &NetPacketList);
    }

    KeReleaseQueuedLock(MldLink->Lock);
    if (NET_PACKET_LIST_EMPTY(&NetPacketList) != FALSE) {
        return;
    }

    Destination.Domain = NetDomainIp6;
    RtlCopyMemory(Destination.Address,
                  NetIp6AllMld2RoutersMulticastAddress,
                  IP6_ADDRESS_SIZE);

    NetpMldSendPackets(MldLink,
                       (PNETWORK_ADDRESS)&Destination,
                       &NetPacketList,
                       ICMP6_MESSAGE_TYPE_MLD2_REPORT);

    return;
}

VOID
NetpMldSendPackets (
    PMLD_LINK MldLink,
    PNETWORK_ADDRESS Destination,
    PNET_PACKET_LIST PacketList,
    UCHAR Type
    )

/*++

Routine Description:

    This routine sends a list of MLD packets out over the provided link to the
    specified destination. It adds the ICMPv6 and IPv6 headers and sends the
    packets down the stack.

Arguments:

    MldLink - Supplies a pointer to the MLD link over which to send the
        packet.

    Destination - Supplies a pointer to the destination address. This should be
        a multicast address.

    PacketList - Supplies a pointer to the list of packets to send.

    Type - Supplies the ICMPv6 message type for the packets.

Return Value:

    None.

--*/

{

    USHORT Checksum;
    PLIST_ENTRY CurrentEntry;
    NETWORK_ADDRESS DestinationPhysical;
    PICMP6_HEADER Icmp6Header;
    ULONG Icmp6Length;
    PIP6_EXTENSION_HEADER Ip6ExtensionHeader;
    PIP6_HEADER Ip6Header;
    PIP6_OPTION Ip6Option;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PNET_PACKET_BUFFER Packet;
    ULONG PayloadLength;
    PUSHORT RouterAlertCode;
    PNET_DATA_LINK_SEND Send;
    PIP6_ADDRESS Source;
    KSTATUS Status;
    IP6_ADDRESS UnspecifiedAddress;
    ULONG VersionClassFlow;

    //
    // For each packet in the list, add an ICMPv6 and IPv6 header.
    //

    Link = MldLink->Link;
    LinkAddress = MldLink->LinkAddress;

    //
    // The source address must be link local or the unspecified address.
    //

    if (LinkAddress->State >= NetLinkAddressConfigured) {
        Source = (PIP6_ADDRESS)&(LinkAddress->Address);

        ASSERT(IP6_IS_UNICAST_LINK_LOCAL_ADDRESS(Source->Address) != FALSE);

    } else {
        RtlZeroMemory(&UnspecifiedAddress, sizeof(IP6_ADDRESS));
        UnspecifiedAddress.Domain = NetDomainIp6;
        Source = &UnspecifiedAddress;
    }

    CurrentEntry = PacketList->Head.Next;
    while (CurrentEntry != &(PacketList->Head)) {
        Packet = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Initialize the ICMPv6 header. The data offset should already be
        // set to the ICMPv6 header as all MLD messages include an ICMPv6
        // header.
        //

        Icmp6Header = (PICMP6_HEADER)(Packet->Buffer + Packet->DataOffset);
        Icmp6Header->Type = Type;
        Icmp6Header->Code = 0;
        Icmp6Header->Checksum = 0;

        //
        // Calculate the ICMPv6 checksum.
        //

        Icmp6Length = Packet->FooterOffset - Packet->DataOffset;
        Checksum = NetChecksumPseudoHeaderAndData(
                                               LinkAddress->Network,
                                               Icmp6Header,
                                               Icmp6Length,
                                               (PNETWORK_ADDRESS)Source,
                                               Destination,
                                               SOCKET_INTERNET_PROTOCOL_ICMP6);

        Icmp6Header->Checksum = Checksum;

        //
        // Add the IPv6 extended header. Work backwards from the Pad-N option.
        //

        Packet->DataOffset -= sizeof(IP6_OPTION);
        Ip6Option = (PIP6_OPTION)(Packet->Buffer + Packet->DataOffset);
        Ip6Option->Type = IP6_OPTION_TYPE_PADN;
        Ip6Option->Length = 0;
        Packet->DataOffset -= sizeof(USHORT);
        RouterAlertCode = (PUSHORT)(Packet->Buffer + Packet->DataOffset);
        *RouterAlertCode = CPU_TO_NETWORK16(IP6_ROUTER_ALERT_CODE_MLD);
        Packet->DataOffset -= sizeof(IP6_OPTION);
        Ip6Option = (PIP6_OPTION)(Packet->Buffer + Packet->DataOffset);
        Ip6Option->Type = IP6_OPTION_TYPE_ROUTER_ALERT;
        Ip6Option->Length = sizeof(USHORT);
        Packet->DataOffset -= sizeof(IP6_EXTENSION_HEADER);

        //
        // The extension header length is measured in 8 byte units and does not
        // include the first 8 bytes. Thus, it is zero in this instance.
        //

        Ip6ExtensionHeader = (PIP6_EXTENSION_HEADER)(Packet->Buffer +
                                                     Packet->DataOffset);

        Ip6ExtensionHeader->NextHeader = SOCKET_INTERNET_PROTOCOL_ICMP6;
        Ip6ExtensionHeader->Length = 0;

        //
        // Now add the IPv6 header.
        //

        PayloadLength = Packet->FooterOffset - Packet->DataOffset;
        if (PayloadLength > IP6_MAX_PAYLOAD_LENGTH) {
            Status = STATUS_MESSAGE_TOO_LONG;
            goto MldSendPacketsEnd;
        }

        ASSERT(Packet->DataOffset >= sizeof(IP6_HEADER));

        Packet->DataOffset -= sizeof(IP6_HEADER);
        Ip6Header = (PIP6_HEADER)(Packet->Buffer + Packet->DataOffset);
        VersionClassFlow = (IP6_VERSION << IP6_VERSION_SHIFT) &
                           IP6_VERSION_MASK;

        Ip6Header->VersionClassFlow = CPU_TO_NETWORK32(VersionClassFlow);
        Ip6Header->PayloadLength = CPU_TO_NETWORK16((USHORT)PayloadLength);
        Ip6Header->NextHeader = SOCKET_INTERNET_PROTOCOL_HOPOPT;
        Ip6Header->HopLimit = MLD_IP6_HOP_LIMIT;
        RtlCopyMemory(Ip6Header->SourceAddress,
                      Source->Address,
                      IP6_ADDRESS_SIZE);

        RtlCopyMemory(Ip6Header->DestinationAddress,
                      Destination->Address,
                      IP6_ADDRESS_SIZE);
    }

    //
    // Get the physical address for the IPv6 address.
    //

    ASSERT(IP6_IS_MULTICAST_ADDRESS(((PIP6_ADDRESS)Destination)->Address));

    Status = Link->DataLinkEntry->Interface.ConvertToPhysicalAddress(
                                                          Destination,
                                                          &DestinationPhysical,
                                                          NetAddressMulticast);

    if (!KSUCCESS(Status)) {
        goto MldSendPacketsEnd;
    }

    Send = Link->DataLinkEntry->Interface.Send;
    Status = Send(Link->DataLinkContext,
                  PacketList,
                  &(LinkAddress->PhysicalAddress),
                  &DestinationPhysical,
                  IP6_PROTOCOL_NUMBER);

    if (!KSUCCESS(Status)) {
        goto MldSendPacketsEnd;
    }

MldSendPacketsEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(PacketList);
    }

    return;
}

PMLD_LINK
NetpMldCreateOrLookupLink (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine creates an MLD link associated with the given local address
    and attempts to insert it into the tree. If an existing match is found,
    then the existing link is returned.

Arguments:

    Link - Supplies a pointer to the network link for which the MLD link is
        to be created.

    LinkAddress - Supplies a pointer to the link address entry on the given
        network link with which the MLD link shall be associated.

Return Value:

    Returns a pointer to the newly allocated MLD link on success or NULL on
    failure.

--*/

{

    PNET_DATA_LINK_ENTRY DataLinkEntry;
    NET_PACKET_SIZE_INFORMATION DataSizeInformation;
    PRED_BLACK_TREE_NODE FoundNode;
    ULONG Index;
    PNET_PACKET_SIZE_INFORMATION LinkSizeInformation;
    ULONG MaxPacketSize;
    PMLD_LINK MldLink;
    PMLD_LINK NewMldLink;
    MLD_LINK SearchLink;
    KSTATUS Status;

    MldLink = NULL;
    NewMldLink = MmAllocatePagedPool(sizeof(MLD_LINK), MLD_ALLOCATION_TAG);
    if (NewMldLink == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateOrLookupLinkEnd;
    }

    RtlZeroMemory(NewMldLink, sizeof(MLD_LINK));
    NewMldLink->ReferenceCount = 1;
    NetLinkAddReference(Link);
    NewMldLink->Link = Link;
    NewMldLink->LinkAddress = LinkAddress;
    NewMldLink->RobustnessVariable = MLD_DEFAULT_ROBUSTNESS_VARIABLE;
    NewMldLink->QueryInterval = MLD_DEFAULT_QUERY_INTERVAL;
    NewMldLink->MaxResponseTime = MLD_DEFAULT_MAX_RESPONSE_TIME;
    NewMldLink->CompatibilityMode = MldVersion2;
    INITIALIZE_LIST_HEAD(&(NewMldLink->MulticastGroupList));
    NewMldLink->Lock = KeCreateQueuedLock();
    if (NewMldLink->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateOrLookupLinkEnd;
    }

    //
    // Determine the maximum allowed MLD packet size based on the link.
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
                      MLD_IP6_HEADER_SIZE);

    NewMldLink->MaxPacketSize = MaxPacketSize;
    Status = NetpMldInitializeTimer(&(NewMldLink->ReportTimer),
                                     NetpMldLinkReportTimeoutWorker,
                                     NewMldLink);

    if (!KSUCCESS(Status)) {
        goto CreateOrLookupLinkEnd;
    }

    //
    // Initialize the compatibility mode counters.
    //

    for (Index = 0; Index < MLD_COMPATIBILITY_MODE_COUNT; Index += 1) {
        Status = NetpMldInitializeTimer(
                                     &(NewMldLink->CompatibilityTimer[Index]),
                                     NetpMldLinkCompatibilityTimeoutWorker,
                                     NewMldLink);

        if (!KSUCCESS(Status)) {
            goto CreateOrLookupLinkEnd;
        }
    }

    //
    // Attempt to insert the new MLD link into the tree. If an existing link
    // is found, use that one and destroy the new one.
    //

    SearchLink.Link = Link;
    KeAcquireSharedExclusiveLockExclusive(NetMldLinkLock);
    FoundNode = RtlRedBlackTreeSearch(&NetMldLinkTree, &(SearchLink.Node));
    if (FoundNode == NULL) {
        RtlRedBlackTreeInsert(&NetMldLinkTree, &(NewMldLink->Node));
        MldLink = NewMldLink;
        NewMldLink = NULL;

    } else {
        MldLink = RED_BLACK_TREE_VALUE(FoundNode, MLD_LINK, Node);
    }

    NetpMldLinkAddReference(MldLink);
    KeReleaseSharedExclusiveLockExclusive(NetMldLinkLock);

CreateOrLookupLinkEnd:
    if (NewMldLink != NULL) {
        NetpMldLinkReleaseReference(NewMldLink);
    }

    return MldLink;
}

VOID
NetpMldDestroyLink (
    PMLD_LINK MldLink
    )

/*++

Routine Description:

    This routine destroys an MLD link and all of its resources.

Arguments:

    MldLink - Supplies a pointer to the MLD link to destroy.

Return Value:

    None.

--*/

{

    ULONG Index;

    ASSERT(MldLink->ReferenceCount == 0);
    ASSERT(LIST_EMPTY(&(MldLink->MulticastGroupList)) != FALSE);

    NetpMldDestroyTimer(&(MldLink->ReportTimer));
    for (Index = 0; Index < MLD_COMPATIBILITY_MODE_COUNT; Index += 1) {
        NetpMldDestroyTimer(&(MldLink->CompatibilityTimer[Index]));
    }

    if (MldLink->Lock != NULL) {
        KeDestroyQueuedLock(MldLink->Lock);
    }

    NetLinkReleaseReference(MldLink->Link);
    MmFreePagedPool(MldLink);
    return;
}

PMLD_LINK
NetpMldLookupLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine finds an MLD link associated with the given network link. The
    caller is expected to release a reference on the MLD link.

Arguments:

    Link - Supplies a pointer to the network link, which is used to look up the
        MLD link.

Return Value:

    Returns a pointer to the matching MLD link on success or NULL on failure.

--*/

{

    PRED_BLACK_TREE_NODE FoundNode;
    PMLD_LINK MldLink;
    MLD_LINK SearchLink;

    MldLink = NULL;
    SearchLink.Link = Link;
    KeAcquireSharedExclusiveLockShared(NetMldLinkLock);
    FoundNode = RtlRedBlackTreeSearch(&NetMldLinkTree, &(SearchLink.Node));
    if (FoundNode == NULL) {
        goto FindLinkEnd;
    }

    MldLink = RED_BLACK_TREE_VALUE(FoundNode, MLD_LINK, Node);
    NetpMldLinkAddReference(MldLink);

FindLinkEnd:
    KeReleaseSharedExclusiveLockShared(NetMldLinkLock);
    return MldLink;
}

VOID
NetpMldLinkAddReference (
    PMLD_LINK MldLink
    )

/*++

Routine Description:

    This routine increments the reference count of an MLD link.

Arguments:

    MldLink - Supplies a pointer to the MLD link.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(MldLink->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    return;
}

VOID
NetpMldLinkReleaseReference (
    PMLD_LINK MldLink
    )

/*++

Routine Description:

    This routine releases a reference on an MLD link.

Arguments:

    MldLink - Supplies a pointer to the MLD link.

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

    KeAcquireSharedExclusiveLockExclusive(NetMldLinkLock);
    OldReferenceCount = RtlAtomicAdd32(&(MldLink->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    //
    // If the second reference was just released, then the last references is
    // from creation. No multicast groups have a reference on the link and as
    // the tree lock is held exclusively, no other threads have references on
    // the link. Therefore, the link can be removed from the tree.
    //

    if (OldReferenceCount == 2) {

        ASSERT(LIST_EMPTY(&(MldLink->MulticastGroupList)) != FALSE);
        ASSERT(MldLink->GroupCount == 0);

        RtlRedBlackTreeRemove(&NetMldLinkTree, &(MldLink->Node));
        MldLink->Node.Parent = NULL;
        KeReleaseSharedExclusiveLockExclusive(NetMldLinkLock);
        NetpMldLinkReleaseReference(MldLink);

    } else {
        KeReleaseSharedExclusiveLockExclusive(NetMldLinkLock);
        if (OldReferenceCount == 1) {
            NetpMldDestroyLink(MldLink);
        }
    }

    return;
}

COMPARISON_RESULT
NetpMldCompareLinkEntries (
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

    PMLD_LINK FirstMldLink;
    PMLD_LINK SecondMldLink;

    FirstMldLink = RED_BLACK_TREE_VALUE(FirstNode, MLD_LINK, Node);
    SecondMldLink = RED_BLACK_TREE_VALUE(SecondNode, MLD_LINK, Node);
    if (FirstMldLink->Link == SecondMldLink->Link) {
        return ComparisonResultSame;

    } else if (FirstMldLink->Link < SecondMldLink->Link) {
        return ComparisonResultAscending;
    }

    return ComparisonResultDescending;
}

PMLD_MULTICAST_GROUP
NetpMldCreateGroup (
    PMLD_LINK MldLink,
    PIP6_ADDRESS GroupAddress
    )

/*++

Routine Description:

    This routine creats an MLD multicast group structure.

Arguments:

    MldLink - Supplies a pointer to the MLD link to which the multicast group
        will belong.

    GroupAddress - Supplies a pointer to the IPv6 multicast address for the
        group.

Return Value:

    Returns a pointer to the newly allocated multicast group.

--*/

{

    PMLD_MULTICAST_GROUP Group;
    KSTATUS Status;

    Group = MmAllocatePagedPool(sizeof(MLD_MULTICAST_GROUP),
                                MLD_ALLOCATION_TAG);

    if (Group == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateGroupEnd;
    }

    RtlZeroMemory(Group, sizeof(MLD_MULTICAST_GROUP));
    Group->ReferenceCount = 1;
    Group->JoinCount = 1;
    NetpMldLinkAddReference(MldLink);
    Group->MldLink = MldLink;
    RtlCopyMemory(Group->Address, GroupAddress->Address, IP6_ADDRESS_SIZE);
    Status = NetpMldInitializeTimer(&(Group->Timer),
                                     NetpMldGroupTimeoutWorker,
                                     Group);

    if (!KSUCCESS(Status)) {
        goto CreateGroupEnd;
    }

CreateGroupEnd:
    if (!KSUCCESS(Status)) {
        if (Group != NULL) {
            NetpMldDestroyGroup(Group);
            Group = NULL;
        }
    }

    return Group;
}

VOID
NetpMldDestroyGroup (
    PMLD_MULTICAST_GROUP Group
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

    NetpMldDestroyTimer(&(Group->Timer));
    NetpMldLinkReleaseReference(Group->MldLink);
    MmFreePagedPool(Group);
    return;
}

PMLD_MULTICAST_GROUP
NetpMldLookupGroup (
    PMLD_LINK MldLink,
    PIP6_ADDRESS GroupAddress
    )

/*++

Routine Description:

    This routine finds a multicast group with the given address that the given
    link has joined. It takes a reference on the found group.

Arguments:

    MldLink - Supplies a pointer to the MLD link that owns the group to find.

    GroupAddress - Supplies a pointer to the IPv6 multicast address of the
        group.

Return Value:

    Returns a pointer to a multicast group on success or NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    BOOL Equal;
    PMLD_MULTICAST_GROUP Group;

    ASSERT(KeIsQueuedLockHeld(MldLink->Lock) != FALSE);

    CurrentEntry = MldLink->MulticastGroupList.Next;
    while (CurrentEntry != &(MldLink->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, MLD_MULTICAST_GROUP, ListEntry);
        Equal = RtlCompareMemory(Group->Address,
                                 GroupAddress->Address,
                                 IP6_ADDRESS_SIZE);

        if (Equal != FALSE) {
            NetpMldGroupAddReference(Group);
            return Group;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

VOID
NetpMldGroupAddReference (
    PMLD_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine increments the reference count of an MLD multicast group.

Arguments:

    Group - Supplies a pointer to the MLD multicast group.

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
NetpMldGroupReleaseReference (
    PMLD_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine releases a reference on an MLD multicast group.

Arguments:

    Group - Supplies a pointer to the MLD multicast group.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Group->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        NetpMldDestroyGroup(Group);
    }

    return;
}

KSTATUS
NetpMldInitializeTimer (
    PMLD_TIMER Timer,
    PWORK_ITEM_ROUTINE WorkRoutine,
    PVOID WorkParameter
    )

/*++

Routine Description:

    This routine initializes the given MLD timer, setting up its timer, DPC,
    and work item.

Arguments:

    Timer - Supplies a pointer to the MLD timer to initialize.

    WorkRoutine - Supplies a pointer to the routine that runs when the work
        item is scheduled.

    WorkParameter - Supplies a pointer that is passed to the work routine when
        it is invoked.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Timer->Timer = KeCreateTimer(MLD_ALLOCATION_TAG);
    if (Timer->Timer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTimerEnd;
    }

    Timer->Dpc = KeCreateDpc(NetpMldTimerDpcRoutine, Timer);
    if (Timer->Dpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTimerEnd;
    }

    Timer->WorkItem = KeCreateWorkItem(NULL,
                                       WorkPriorityNormal,
                                       WorkRoutine,
                                       WorkParameter,
                                       MLD_ALLOCATION_TAG);

    if (Timer->WorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTimerEnd;
    }

    Status = STATUS_SUCCESS;

InitializeTimerEnd:
    if (!KSUCCESS(Status)) {
        NetpMldDestroyTimer(Timer);
    }

    return Status;
}

VOID
NetpMldDestroyTimer (
    PMLD_TIMER Timer
    )

/*++

Routine Description:

    This routine destroys all the resources of an MLD timer. It does not
    release the structure itself, as it is ususally embedded within another
    structure.

Arguments:

    Timer - Supplies a pointer to an MLD timer to destroy.

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
NetpMldIsReportableAddress (
    PULONG GroupAddress
    )

/*++

Routine Description:

    This routine determines whether or not the given group address should be
    reported in MLD link-wide reports.

Arguments:

    GroupAddress - Supplies a pointer to the multicast group address to check.

Return Value:

    Returns TRUE if the group address should be reported or FALSE otherwise.

--*/

{

    BOOL Equal;

    Equal = RtlCompareMemory(GroupAddress,
                             NetIp6AllNodesMulticastAddress,
                             IP6_ADDRESS_SIZE);

    if (Equal != FALSE) {
        return FALSE;
    }

    return TRUE;
}

