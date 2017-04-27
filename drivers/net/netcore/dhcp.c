/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dhcp.c

Abstract:

    This module implements support for the Dynamic Host Configuration Protocol,
    or DHCP.

Author:

    Evan Green 5-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Network layer drivers are supposed to be able to stand on their own (ie be
// able to be implemented outside the core net library). For the builtin ones,
// avoid including netcore.h, but still redefine those functions that would
// otherwise generate imports.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/ip4.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros get the renewal and binding times as a percentage of the total
// lease time. The renewal time is at 50% of the lease and the rebinding time
// is at 87.5% of the lease time.
//

#define DHCP_GET_DEFAULT_RENEWAL_TIME(_LeaseTime) (_LeaseTime) >> 1
#define DHCP_GET_DEFAULT_REBINDING_TIME(_LeaseTime) \
    ((_LeaseTime) - ((_LeaseTime) >> 3))

//
// ---------------------------------------------------------------- Definitions
//

#define DHCP_ALLOCATION_TAG 0x70636844 // 'pchD'

//
// Define the maximum number of DNS server addresses that will be saved in this
// implementation.
//

#define DHCP_MAX_DNS_SERVERS 4

//
// Define some well-known port numbers.
//

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

//
// Define DHCP packet field values.
//

#define DHCP_OPERATION_REQUEST                0x01
#define DHCP_OPERATION_REPLY                  0x02

#define DHCP_HARDWARE_TYPE_ETHERNET           0x01

#define DHCP_ETHERNET_HARDWARE_ADDRESS_LENGTH 6
#define DHCP_FLAG_BROADCAST                   0x01

#define DHCP_MAGIC_COOKIE                     0x63825363

#define DHCP_OPTION_HEADER_SIZE               2
#define DHCP_SCRATCH_PACKET_SIZE              4096

//
// Define how many times discovery should be retried.
//

#define DHCP_DISCOVER_RETRY_COUNT 5

//
// Define the number of times to retry binding, and how long to wait in
// microseconds.
//

#define DHCP_BIND_RETRY_COUNT 20
#define DHCP_BIND_DELAY (5 * MICROSECONDS_PER_SECOND)
#define DHCP_BIND_VARIANCE (15 * MICROSECONDS_PER_SECOND)

//
// Define how long to wait for an offer and acknowledge, in milliseconds.
//

#define DHCP_OFFER_TIMEOUT                 5000
#define DHCP_ACKNOWLEDGE_TIMEOUT           DHCP_OFFER_TIMEOUT

//
// Define DHCP option codes.
//

#define DHCP_OPTION_PAD                    0
#define DHCP_OPTION_SUBNET_MASK            1
#define DHCP_OPTION_TIME_OFFSET            2
#define DHCP_OPTION_ROUTER                 3
#define DHCP_OPTION_DOMAIN_NAME_SERVER     6
#define DHCP_OPTION_HOST_NAME              12
#define DHCP_OPTION_DOMAIN_NAME            15
#define DHCP_OPTION_REQUESTED_IP_ADDRESS   50
#define DHCP_OPTION_IP_ADDRESS_LEASE_TIME  51
#define DHCP_OPTION_OPTION_OVERLOAD        52
#define DHCP_OPTION_DHCP_MESSAGE_TYPE      53
#define DHCP_OPTION_DHCP_SERVER            54
#define DHCP_OPTION_PARAMETER_REQUEST_LIST 55
#define DHCP_OPTION_MESSAGE                56
#define DHCP_OPTION_RENEWAL_TIME           58
#define DHCP_OPTION_REBINDING_TIME         59
#define DHCP_OPTION_TFTP_SERVER_NAME       66
#define DHCP_OPTION_BOOT_FILE_NAME         67
#define DHCP_OPTION_END                    255

//
// Define DHCP message types.
//

#define DHCP_MESSAGE_DISCOVER    1
#define DHCP_MESSAGE_OFFER       2
#define DHCP_MESSAGE_REQUEST     3
#define DHCP_MESSAGE_DECLINE     4
#define DHCP_MESSAGE_ACKNOWLEDGE 5
#define DHCP_MESSAGE_NAK         6
#define DHCP_MESSAGE_RELEASE     7
#define DHCP_MESSAGE_INFORM      8

//
// Define what goes in the discovery request.
//

#define DHCP_OPTION_MESSAGE_TYPE_SIZE 3

#define DHCP_DISCOVER_OPTION_COUNT            2
#define DHCP_DISCOVER_PARAMETER_REQUEST_COUNT 4
#define DHCP_DISCOVER_OPTIONS_SIZE    \
    (DHCP_OPTION_MESSAGE_TYPE_SIZE +  \
     (DHCP_OPTION_HEADER_SIZE +       \
      DHCP_DISCOVER_PARAMETER_REQUEST_COUNT) + 2)

//
// Define the minimum due time delta for the DHCP lease timer, in seconds.
//

#define DHCP_TIMER_DURATION_MINIMUM 60

//
// Define the debug flags for DHCP.
//

#define DHCP_DEBUG_FLAG_EXTEND 0x1
#define DHCP_DEBUG_FLAG_OFFER 0X2

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the structure of a DHCP packet request or response.
    After this structure, zero or more options may follow.

Members:

    OperationCode - Stores the operation code, either request or reply.

    HardwareType - Stores the media type, pretty much always the code for
        "ethernet".

    HardwareAddressLength - Stores the length of a hardware address, pretty
        much always 6 for ethernet MAC addresses.

    Hops - Stores 0 usually, can be optionally set by relay agents when booting
        via a relay agent.

    TransactionIdentifier - Stores a unique value used to match requests to
        replies.

    Seconds - Stores the number of seconds since the client began the
        renewal or request process.

    Flags - Stores the broadcast flag.

    ClientIpAddress - Stores the client's current IP address.

    YourIpAddress - Stores the IP address being offered by the DHCP server.

    ServerIpAddress - Stores the IP address of the DHCP server.

    GatewayIpAddress - Stores the IP address to contact for requests outside
        the subnet.

    ClientHardwareAddress - Stores the hardware address associated with this
        offer.

    ServerName - Stores an optional NULL terminated string containing the
        server name, used by the BOOTP protocol

    BootFileName - Stores an optional fully qualified path to the BOOTP boot
        file name.

    MagicCookie - Stores a constant value. Options potentially follow after this
        magic.

--*/

typedef struct _DHCP_PACKET {
    UCHAR OperationCode;
    UCHAR HardwareType;
    UCHAR HardwareAddressLength;
    UCHAR Hops;
    ULONG TransactionIdentifier;
    USHORT Seconds;
    USHORT Flags;
    ULONG ClientIpAddress;
    ULONG YourIpAddress;
    ULONG ServerIpAddress;
    ULONG GatewayIpAddress;
    UCHAR ClientHardwareAddress[16];
    UCHAR ServerName[64];
    UCHAR BootFileName[128];
    ULONG MagicCookie;
} PACKED DHCP_PACKET, *PDHCP_PACKET;

/*++

Structure Description:

    This structure defines the required data parsed from a DHCP response.

Members:

    MessageType - Stores the DHCP option message type of the response. See
        DHCP_MESSAGE_* for values.

    ServerIpAddress - Stores the IP address of the DHCP server.

    OfferedIpAddress - Stores the IP address being offered by the DHCP server.

    RouterIpAddress - Stores the default gateway IP address associated with the
        offered address.Stores the IP address of the

    SubnetMask - Stores the subnet mask of the local network, as given by the
        DHCP server.

    DomainNameServer - Stores an array of DNS addresses associated with the
        offer.

    DomainNameServerCount - Stores the number of valid addresses in the DNS
        address array.

    LeaseTime - Stores the lease time returned in the offer or acknowledge, in
        seconds.

    RenewalTime - Stores the time until the renewal state begins, in seconds.

    RebindingTime - Stores the time until the rebinding state begins, in
        seconds.

--*/

typedef struct _DHCP_REPLY {
    UCHAR MessageType;
    ULONG ServerIpAddress;
    ULONG OfferedIpAddress;
    ULONG RouterIpAddress;
    ULONG SubnetMask;
    ULONG DomainNameServer[DHCP_MAX_DNS_SERVERS];
    ULONG DomainNameServerCount;
    ULONG LeaseTime;
    ULONG RenewalTime;
    ULONG RebindingTime;
} DHCP_REPLY, *PDHCP_REPLY;

typedef enum _DHCP_LEASE_STATE {
    DhcpLeaseStateInvalid,
    DhcpLeaseStateInitialize,
    DhcpLeaseStateBound,
    DhcpLeaseStateRenewing,
    DhcpLeaseStateRebinding
} DHCP_LEASE_STATE, *PDHCP_LEASE_STATE;

/*++

Structure Description:

    This structure defines the DHCP state for a leased network address.

Members:

    ListEntry - Stores pointers to the next and previous leased address handed
        out by DHCP.

    Link - Stores a pointer to the link that owns the link address entry.

    LinkAddress - Stores a pointer to the link address entry to which the lease
        was given.

    Timer - Stores a pointer to the timer that will trigger work to either
        renew or rebind the lease.

    Dpc - Stores a pointer to the DPC that is to run once the lease timer
        expires.

    WorkItem - Stores a pointer to the work item that is to be queued by the
        DPC.

    LeaseTime - Stores the total time of the lease.

    RenewalTime - Stores the time at which the lease enters the renewal phase.

    RebindingTime - Stores the time at which the lease enters the rebinding
        phase.

    State - Stores the state of the lease.

    ReferenceCount - Stores the number of references taken on the lease.

--*/

typedef struct _DHCP_LEASE {
    LIST_ENTRY ListEntry;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PKTIMER Timer;
    PDPC Dpc;
    PWORK_ITEM WorkItem;
    ULONG LeaseTime;
    ULONG RenewalTime;
    ULONG RebindingTime;
    DHCP_LEASE_STATE State;
    volatile ULONG ReferenceCount;
} DHCP_LEASE, *PDHCP_LEASE;

/*++

Structure Description:

    This structure defines DHCP context used throughout the assignment
    sequence.

Members:

    Link - Stores a pointer to the link to work on.

    LinkAddress - Stores a pointer to the link address entry to assign an
        address for.

    Lease - Stores a pointer to any existing lease for the given link and link
        address combination.

    ScratchPacket - Stores a pointer to a scratch packet, used for building
        requests.

    ScratchPacketSize - Stores the total size of the scratch packet, including
        extra length for options.

    ScratchPacketIoBuffer - Stores a pointer to an I/O buffer containing the
        scratch packet buffer.

    Socket - Stores a pointer to the UDP socket connection.

    ExpectedTransactionId - Stores the transaction ID that is expected to come
        back in the response. All others get ignored.

    OfferClientAddress - Stores the network address that the DHCP server is
        offering to this machine.

    OfferSubnetMask - Stores the subnet mask of the local network, as given by
        the DHCP server.

    OfferServerAddress - Stores the network address of the server making the
        offer.

    OfferRouter - Stores the default gateway address associated with the offered
        address.

    OfferDnsAddress - Stores an array of DNS addresses associated with the
        offer.

    OfferDnsAddressCount - Stores the number of valid addresses in the DNS
        address array.

    LeaseTime - Stores the lease time returned in the offer or acknowledge, in
        seconds.

    RenewalTime - Stores the time until the renewal state begins, in seconds.

    RebindingTime - Stores the time until the rebinding state begins, in
        seconds.

    LeaseRequestTime - Stores the time at which the lease was requested.

--*/

typedef struct _DHCP_CONTEXT {
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PDHCP_LEASE Lease;
    PDHCP_PACKET ScratchPacket;
    ULONG ScratchPacketSize;
    PIO_BUFFER ScratchPacketIoBuffer;
    PIO_HANDLE Socket;
    ULONG ExpectedTransactionId;
    NETWORK_ADDRESS OfferClientAddress;
    NETWORK_ADDRESS OfferSubnetMask;
    NETWORK_ADDRESS OfferServerAddress;
    NETWORK_ADDRESS OfferRouter;
    NETWORK_ADDRESS OfferDnsAddress[DHCP_MAX_DNS_SERVERS];
    ULONG OfferDnsAddressCount;
    ULONG LeaseTime;
    ULONG RenewalTime;
    ULONG RebindingTime;
    SYSTEM_TIME LeaseRequestTime;
} DHCP_CONTEXT, *PDHCP_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
NetpDhcpAssignmentThread (
    PVOID Parameter
    );

KSTATUS
NetpDhcpBeginLeaseExtension (
    PDHCP_LEASE Lease
    );

VOID
NetpDhcpLeaseExtensionThread (
    PVOID Parameter
    );

KSTATUS
NetpDhcpBeginRelease (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

VOID
NetpDhcpReleaseThread (
    PVOID Parameter
    );

KSTATUS
NetpDhcpSendDiscover (
    PDHCP_CONTEXT Context
    );

KSTATUS
NetpDhcpReceiveOffer (
    PDHCP_CONTEXT Context
    );

KSTATUS
NetpDhcpSendRequest (
    PDHCP_CONTEXT Context
    );

KSTATUS
NetpDhcpReceiveAcknowledge (
    PDHCP_CONTEXT Context
    );

KSTATUS
NetpDhcpSendRelease (
    PDHCP_CONTEXT Context
    );

KSTATUS
NetpDhcpReceiveReply (
    PDHCP_CONTEXT Context,
    PDHCP_REPLY Reply
    );

PDHCP_CONTEXT
NetpDhcpCreateContext (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PDHCP_LEASE Lease
    );

VOID
NetpDhcpDestroyContext (
    PDHCP_CONTEXT Context
    );

PDHCP_LEASE
NetpDhcpCreateLease (
    VOID
    );

VOID
NetpDhcpLeaseAddReference (
    PDHCP_LEASE Lease
    );

VOID
NetpDhcpLeaseReleaseReference (
    PDHCP_LEASE Lease
    );

PDHCP_LEASE
NetpDhcpFindLease (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

VOID
NetpDhcpDestroyLease (
    PDHCP_LEASE AssignmentContext
    );

VOID
NetpDhcpQueueLeaseExtension (
    PDHCP_LEASE Lease
    );

VOID
NetpDhcpLeaseDpcRoutine (
    PDPC Dpc
    );

VOID
NetpDhcpLeaseWorkRoutine (
    PVOID Parameter
    );

KSTATUS
NetpDhcpCopyReplyToContext (
    PDHCP_CONTEXT Context,
    PDHCP_REPLY Reply
    );

KSTATUS
NetpDhcpBind (
    PDHCP_CONTEXT Context,
    PNETWORK_ADDRESS Address
    );

VOID
NetpDhcpPrintContext (
    PDHCP_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

LIST_ENTRY NetDhcpLeaseListHead;
KSPIN_LOCK NetDhcpLeaseListLock;

//
// Store a bitfield of enabled DHCP debug flags. See DHCP_DEBUG_* definitions.
//

ULONG NetDhcpDebugFlags = 0x0;

//
// Set this debug value to overried the lease renewal and rebinding times.
//

BOOL NetDhcpDebugOverrideRenewal = FALSE;

//
// Set these values to the desired renewal and rebinding times if force renewal
// is set.
//

ULONG NetDhcpDebugRenewalTime = 0;
ULONG NetDhcpDebugRebindingTime = 0;

//
// Set these debug values to force failures in the renewal and/or rebinding
// phase.
//

BOOL NetDhcpDebugFailRenewal = FALSE;
BOOL NetDhcpDebugFailRebinding = FALSE;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpDhcpInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for DHCP.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INITIALIZE_LIST_HEAD(&NetDhcpLeaseListHead);
    KeInitializeSpinLock(&NetDhcpLeaseListLock);
    return;
}

KSTATUS
NetpDhcpBeginAssignment (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine kicks off the process of assigning a network address to this
    link address entry by using DHCP.

Arguments:

    Link - Supplies a pointer to the link to send the discovery request out on.

    LinkAddress - Supplies a pointer to the address structure to bind to.

Return Value:

    Status code indicating whether or not the process was successfully
    initiated.

--*/

{

    PDHCP_CONTEXT DhcpContext;
    KSTATUS Status;

    DhcpContext = NetpDhcpCreateContext(Link, LinkAddress, NULL);
    if (DhcpContext == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = PsCreateKernelThread(NetpDhcpAssignmentThread,
                                  DhcpContext,
                                  "DhcpAssignThread");

    if (!KSUCCESS(Status)) {
        NetpDhcpDestroyContext(DhcpContext);
    }

    return Status;
}

KSTATUS
NetpDhcpCancelLease (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine attempts to cancel a DHCP lease.

Arguments:

    Link - Supplies a pointer to the network link to which the lease was
        provided.

    LinkAddress - Supplies a pointer to the network link address that was
        leased.

Return Value:

    Status code.

--*/

{

    PDHCP_LEASE Lease;
    DHCP_LEASE_STATE LeaseState;
    BOOL LinkUp;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    Lease = NetpDhcpFindLease(Link, LinkAddress);
    if (Lease == NULL) {
        goto DhcpCancelLeaseEnd;
    }

    //
    // Remove the lease from the global list.
    //

    KeAcquireSpinLock(&NetDhcpLeaseListLock);
    LIST_REMOVE(&(Lease->ListEntry));
    Lease->ListEntry.Next = NULL;
    KeReleaseSpinLock(&NetDhcpLeaseListLock);

    //
    // Save the lease state. If the lease is in the initialized state then the
    // lease has expired (or never started).
    //

    LeaseState = Lease->State;

    ASSERT(LeaseState != DhcpLeaseStateInvalid);

    //
    // Release the original reference on the lease and the reference taken by
    // the find routine.
    //

    NetpDhcpLeaseReleaseReference(Lease);
    NetpDhcpLeaseReleaseReference(Lease);

    //
    // Be kind. If the link is still up, attempt to release the leased IP
    // address if it is in the bound, renewing, or rebinding state.
    //

    NetGetLinkState(Link, &LinkUp, NULL);
    if ((LinkUp != FALSE) && (LeaseState != DhcpLeaseStateInitialize)) {
        Status = NetpDhcpBeginRelease(Link, LinkAddress);
        if (!KSUCCESS(Status)) {
            goto DhcpCancelLeaseEnd;
        }
    }

DhcpCancelLeaseEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpDhcpAssignmentThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine attempts to assign an address to a link using DHCP.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread, in
        this case a pointer to the DHCP context structure.

Return Value:

    None.

--*/

{

    BOOL BroadcastEnabled;
    UINTN DataSize;
    PDHCP_CONTEXT DhcpContext;
    NETWORK_DEVICE_INFORMATION Information;
    PDHCP_LEASE Lease;
    BOOL LeaseAcquired;
    SYSTEM_TIME LeaseEndTime;
    NETWORK_ADDRESS LocalAddress;
    ULONG RetryCount;
    KSTATUS Status;
    PSTR Step;

    DhcpContext = (PDHCP_CONTEXT)Parameter;
    Lease = NULL;
    LeaseAcquired = FALSE;
    Step = "Init";

    ASSERT(DhcpContext->Lease == NULL);

    //
    // Make sure there are no left over leases for this link and link address
    // combination. If the DHCP assignment results in the same IP address then
    // an old lease will not be canceled by the networking core. If it results
    // in a different IP address then it will attempt to destroy the old lease,
    // preventing reuse of any current lease here.
    //

    Status = NetpDhcpCancelLease(DhcpContext->Link, DhcpContext->LinkAddress);
    if (!KSUCCESS(Status)) {
        goto DhcpAssignmentThreadEnd;
    }

    Lease = NetpDhcpCreateLease();
    if (Lease == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DhcpAssignmentThreadEnd;
    }

    //
    // Create the scratch packet space and the socket.
    //

    DhcpContext->ScratchPacket = MmAllocatePagedPool(DHCP_SCRATCH_PACKET_SIZE,
                                                     DHCP_ALLOCATION_TAG);

    if (DhcpContext->ScratchPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DhcpAssignmentThreadEnd;
    }

    DhcpContext->ScratchPacketSize = DHCP_SCRATCH_PACKET_SIZE;
    Status = MmCreateIoBuffer(DhcpContext->ScratchPacket,
                              DhcpContext->ScratchPacketSize,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &(DhcpContext->ScratchPacketIoBuffer));

    if (!KSUCCESS(Status)) {
        goto DhcpAssignmentThreadEnd;
    }

    Status = IoSocketCreate(NetDomainIp4,
                            NetSocketDatagram,
                            SOCKET_INTERNET_PROTOCOL_UDP,
                            0,
                            &(DhcpContext->Socket));

    if (!KSUCCESS(Status)) {
        goto DhcpAssignmentThreadEnd;
    }

    //
    // Bind that socket to the known DHCP client port. The any address must be
    // used as the DHCP server will reply with broadcast packets and only an
    // unbound socket will pick those up.
    //

    RtlZeroMemory(&LocalAddress, sizeof(NETWORK_ADDRESS));
    LocalAddress.Domain = NetDomainIp4;
    LocalAddress.Port = DHCP_CLIENT_PORT;
    Status = NetpDhcpBind(DhcpContext, &LocalAddress);
    if (!KSUCCESS(Status)) {
        goto DhcpAssignmentThreadEnd;
    }

    //
    // Enable broadcast messages on this socket.
    //

    BroadcastEnabled = TRUE;
    DataSize = sizeof(BOOL);
    Status = IoSocketGetSetInformation(DhcpContext->Socket,
                                       SocketInformationBasic,
                                       SocketBasicOptionBroadcastEnabled,
                                       &BroadcastEnabled,
                                       &DataSize,
                                       TRUE);

    if (!KSUCCESS(Status)) {
        goto DhcpAssignmentThreadEnd;
    }

    Lease->State = DhcpLeaseStateInitialize;
    NetpDhcpLeaseAddReference(Lease);
    DhcpContext->Lease = Lease;
    RetryCount = 0;
    while (RetryCount < DHCP_DISCOVER_RETRY_COUNT) {
        RetryCount += 1;

        //
        // Kick off the sequence by sending a discovery packet.
        //

        Step = "SendDiscover";
        Status = NetpDhcpSendDiscover(DhcpContext);
        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Get the offer back from the server.
        //

        Step = "ReceiveOffer";
        Status = NetpDhcpReceiveOffer(DhcpContext);
        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Request the address that came back.
        //

        Step = "SendRequest";
        Status = NetpDhcpSendRequest(DhcpContext);
        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Get the acknowledge back to make sure the DHCP heard the request.
        //

        Step = "ReceiveAcknowledge";
        Status = NetpDhcpReceiveAcknowledge(DhcpContext);
        if (!KSUCCESS(Status)) {
            continue;
        }

        break;
    }

    if (!KSUCCESS(Status)) {
        goto DhcpAssignmentThreadEnd;
    }

    LeaseAcquired = TRUE;

    //
    // Calculate the lease's end based on the DHCP offer.
    //

    RtlCopyMemory(&LeaseEndTime,
                  &(DhcpContext->LeaseRequestTime),
                  sizeof(SYSTEM_TIME));

    LeaseEndTime.Seconds += DhcpContext->LeaseTime;

    //
    // The address reservation is complete. Set the parameters in the link
    // address entry.
    //

    Step = "SetNetworkAddress";
    RtlZeroMemory(&Information, sizeof(NETWORK_DEVICE_INFORMATION));
    Information.Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Information.Flags = NETWORK_DEVICE_FLAG_CONFIGURED;
    Information.Domain = NetDomainIp4;
    Information.ConfigurationMethod = NetworkAddressConfigurationDhcp;
    RtlCopyMemory(&(Information.Address),
                  &(DhcpContext->OfferClientAddress),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information.Subnet),
                  &(DhcpContext->OfferSubnetMask),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information.Gateway),
                  &(DhcpContext->OfferRouter),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information.DnsServers),
                  &(DhcpContext->OfferDnsAddress),
                  sizeof(NETWORK_ADDRESS) * DhcpContext->OfferDnsAddressCount);

    Information.DnsServerCount = DhcpContext->OfferDnsAddressCount;
    RtlCopyMemory(&(Information.LeaseServerAddress),
                  &(DhcpContext->OfferServerAddress),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information.LeaseStartTime),
                  &(DhcpContext->LeaseRequestTime),
                  sizeof(SYSTEM_TIME));

    RtlCopyMemory(&(Information.LeaseEndTime),
                  &LeaseEndTime,
                  sizeof(SYSTEM_TIME));

    Status = NetGetSetNetworkDeviceInformation(DhcpContext->Link,
                                               DhcpContext->LinkAddress,
                                               &Information,
                                               TRUE);

    if (!KSUCCESS(Status)) {
        goto DhcpAssignmentThreadEnd;
    }

    //
    // Celebrate the assignment with some debugger prints.
    //

    RtlDebugPrint("DHCP Assignment:\n");
    NetpDhcpPrintContext(DhcpContext);

    //
    // Finish initializing the lease, including adding it to the global list.
    //

    NetLinkAddReference(DhcpContext->Link);
    Lease->Link = DhcpContext->Link;
    Lease->LinkAddress = DhcpContext->LinkAddress;
    Lease->State = DhcpLeaseStateBound;
    Lease->LeaseTime = DhcpContext->LeaseTime;
    Lease->RenewalTime = DhcpContext->RenewalTime;
    Lease->RebindingTime = DhcpContext->RebindingTime;
    KeAcquireSpinLock(&NetDhcpLeaseListLock);
    INSERT_BEFORE(&(Lease->ListEntry), &NetDhcpLeaseListHead);
    KeReleaseSpinLock(&NetDhcpLeaseListLock);

    //
    // The lease is established. Set the lease timer so that a lease renewal is
    // attemtped at the time specified by the server.
    //

    NetpDhcpQueueLeaseExtension(Lease);

DhcpAssignmentThreadEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Net: DHCP assignment failed at step '%s': %d.\n",
                      Step,
                      Status);

        //
        // If the routine failed after the lease was acquired, kindly release
        // the IP address back to the server.
        //

        if (LeaseAcquired != FALSE) {
            RtlZeroMemory(&Information, sizeof(NETWORK_DEVICE_INFORMATION));
            Information.Version = NETWORK_DEVICE_INFORMATION_VERSION;
            Information.Domain = NetDomainIp4;
            Information.ConfigurationMethod = NetworkAddressConfigurationNone;
            NetGetSetNetworkDeviceInformation(DhcpContext->Link,
                                              DhcpContext->LinkAddress,
                                              &Information,
                                              TRUE);

            NetpDhcpSendRelease(DhcpContext);
        }

        if (Lease != NULL) {
            NetpDhcpLeaseReleaseReference(Lease);
        }
    }

    NetpDhcpDestroyContext(DhcpContext);
    return;
}

KSTATUS
NetpDhcpBeginLeaseExtension (
    PDHCP_LEASE Lease
    )

/*++

Routine Description:

    This routine kicks off the process of extending the given DHCP lease.

Arguments:

    Lease - Supplies a pointer to a DHCP lease that is to be renewed.

Return Value:

    Status code indicating whether or not the process was successfully
    initiated.

--*/

{

    PDHCP_CONTEXT DhcpContext;
    KSTATUS Status;

    ASSERT(Lease->Link != NULL);
    ASSERT(Lease->LinkAddress != NULL);
    ASSERT((Lease->State == DhcpLeaseStateRenewing) ||
           (Lease->State == DhcpLeaseStateRebinding));

    DhcpContext = NetpDhcpCreateContext(Lease->Link, Lease->LinkAddress, Lease);
    if (DhcpContext == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = PsCreateKernelThread(NetpDhcpLeaseExtensionThread,
                                  DhcpContext,
                                  "DhcpExtendThread");

    if (!KSUCCESS(Status)) {
        NetpDhcpDestroyContext(DhcpContext);
        return Status;
    }

    return STATUS_SUCCESS;
}

VOID
NetpDhcpLeaseExtensionThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine attempts to extend the lease on an address for a link using
    DHCP.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread, in
        this case a pointer to the DHCP context structure.

Return Value:

    None.

--*/

{

    PDHCP_CONTEXT DhcpContext;
    NETWORK_DEVICE_INFORMATION Information;
    PDHCP_LEASE Lease;
    SYSTEM_TIME LeaseEndTime;
    NETWORK_ADDRESS LocalAddress;
    BOOL LockHeld;
    KSTATUS Status;
    PSTR Step;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    DhcpContext = (PDHCP_CONTEXT)Parameter;
    Lease = DhcpContext->Lease;
    LockHeld = FALSE;
    Step = "Init";

    ASSERT(Lease != NULL);
    ASSERT((Lease->State == DhcpLeaseStateRenewing) ||
           (Lease->State == DhcpLeaseStateRebinding));

    //
    // If the debug state is set to fail this phase, then skip to the end.
    //

    if (((NetDhcpDebugFailRenewal != FALSE) &&
         (Lease->State == DhcpLeaseStateRenewing)) ||
        ((NetDhcpDebugFailRebinding != FALSE) &&
         (Lease->State == DhcpLeaseStateRebinding))) {

        Step = "ForceFailure";
        Status = STATUS_TRY_AGAIN;
        goto DhcpLeaseExtensionThreadEnd;
    }

    //
    // Create the scratch packet space and the socket.
    //

    DhcpContext->ScratchPacket = MmAllocatePagedPool(DHCP_SCRATCH_PACKET_SIZE,
                                                     DHCP_ALLOCATION_TAG);

    if (DhcpContext->ScratchPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DhcpLeaseExtensionThreadEnd;
    }

    DhcpContext->ScratchPacketSize = DHCP_SCRATCH_PACKET_SIZE;
    Status = MmCreateIoBuffer(DhcpContext->ScratchPacket,
                              DhcpContext->ScratchPacketSize,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &(DhcpContext->ScratchPacketIoBuffer));

    if (!KSUCCESS(Status)) {
        goto DhcpLeaseExtensionThreadEnd;
    }

    Status = IoSocketCreate(NetDomainIp4,
                            NetSocketDatagram,
                            SOCKET_INTERNET_PROTOCOL_UDP,
                            0,
                            &(DhcpContext->Socket));

    if (!KSUCCESS(Status)) {
        goto DhcpLeaseExtensionThreadEnd;
    }

    //
    // Bind that socket to the known DHCP client port. Make sure the link
    // address is still valid before copying it.
    //

    KeAcquireQueuedLock(DhcpContext->Link->QueuedLock);
    LockHeld = TRUE;
    if (DhcpContext->LinkAddress->Configured == FALSE) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto DhcpLeaseExtensionThreadEnd;
    }

    RtlCopyMemory(&LocalAddress,
                  &(DhcpContext->LinkAddress->Address),
                  sizeof(NETWORK_ADDRESS));

    KeReleaseQueuedLock(DhcpContext->Link->QueuedLock);
    LockHeld = FALSE;
    LocalAddress.Port = DHCP_CLIENT_PORT;
    Status = NetpDhcpBind(DhcpContext, &LocalAddress);
    if (!KSUCCESS(Status)) {
        goto DhcpLeaseExtensionThreadEnd;
    }

    //
    // Request the address that came back.
    //

    Step = "SendRequest";
    Status = NetpDhcpSendRequest(DhcpContext);
    if (!KSUCCESS(Status)) {
        goto DhcpLeaseExtensionThreadEnd;
    }

    //
    // Get the acknowledge back to make sure the DHCP heard the request.
    //

    Step = "ReceiveAcknowledge";
    Status = NetpDhcpReceiveAcknowledge(DhcpContext);
    if (!KSUCCESS(Status)) {
        goto DhcpLeaseExtensionThreadEnd;
    }

    //
    // Calculate the lease's end based on the DHCP offer.
    //

    RtlCopyMemory(&LeaseEndTime,
                  &(DhcpContext->LeaseRequestTime),
                  sizeof(SYSTEM_TIME));

    LeaseEndTime.Seconds += DhcpContext->LeaseTime;

    //
    // The lease extension is complete. Set the parameters in the link address
    // entry.
    //

    Step = "SetNetworkAddress";
    RtlZeroMemory(&Information, sizeof(NETWORK_DEVICE_INFORMATION));
    Information.Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Information.Flags = NETWORK_DEVICE_FLAG_CONFIGURED;
    Information.Domain = NetDomainIp4;
    Information.ConfigurationMethod = NetworkAddressConfigurationDhcp;
    RtlCopyMemory(&(Information.Address),
                  &(DhcpContext->OfferClientAddress),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information.Subnet),
                  &(DhcpContext->OfferSubnetMask),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information.Gateway),
                  &(DhcpContext->OfferRouter),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information.DnsServers),
                  &(DhcpContext->OfferDnsAddress),
                  sizeof(NETWORK_ADDRESS) * DhcpContext->OfferDnsAddressCount);

    Information.DnsServerCount = DhcpContext->OfferDnsAddressCount;
    RtlCopyMemory(&(Information.LeaseServerAddress),
                  &(DhcpContext->OfferServerAddress),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information.LeaseStartTime),
                  &(DhcpContext->LeaseRequestTime),
                  sizeof(SYSTEM_TIME));

    RtlCopyMemory(&(Information.LeaseEndTime),
                  &LeaseEndTime,
                  sizeof(SYSTEM_TIME));

    Status = NetGetSetNetworkDeviceInformation(DhcpContext->Link,
                                               DhcpContext->LinkAddress,
                                               &Information,
                                               TRUE);

    if (!KSUCCESS(Status)) {
        goto DhcpLeaseExtensionThreadEnd;
    }

    //
    // Celebrate the extnesion with some debugger prints.
    //

    RtlDebugPrint("DHCP Extension:\n");
    NetpDhcpPrintContext(DhcpContext);
    if ((NetDhcpDebugFlags & DHCP_DEBUG_FLAG_EXTEND) != 0) {
        RtlDebugPrint("Net: DHCP extended lease (0x%08x) for link (0x%08x) "
                      "from state %d.\n",
                      Lease,
                      Lease->Link,
                      Lease->State);
    }

    //
    // Mark that the lease is now in the bound state.
    //

    Lease->State = DhcpLeaseStateBound;

    //
    // The lease has been extended. Set the lease timer so that a lease
    // renewal is attemtped at the time specified by the server.
    //

    NetpDhcpQueueLeaseExtension(Lease);

DhcpLeaseExtensionThreadEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(DhcpContext->Link->QueuedLock);
    }

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Net: DHCP lease extension failed at step '%s': %d.\n",
                      Step,
                      Status);

        //
        // No matter when the extension failed, try to queue lease extension
        // again. This is OK even if this routine fails after extending the
        // lease. There is no harm in prematurely asking the DHCP server for
        // an extension.
        //

        NetpDhcpQueueLeaseExtension(Lease);
    }

    NetpDhcpDestroyContext(DhcpContext);
    return;
}

KSTATUS
NetpDhcpBeginRelease (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine kicks off the process of releasing the IP address previously
    assigned to the given link and address via DHCP.

Arguments:

    Link - Supplies a pointer to the link to send the discovery request out on.

    LinkAddress - Supplies a pointer to the address structure to bind to.

Return Value:

    Status code indicating whether or not the process was successfully
    initiated.

--*/

{

    PDHCP_CONTEXT DhcpContext;
    KSTATUS Status;

    DhcpContext = NetpDhcpCreateContext(Link, LinkAddress, NULL);
    if (DhcpContext == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = PsCreateKernelThread(NetpDhcpReleaseThread,
                                  DhcpContext,
                                  "DhcpReleaseThread");

    if (!KSUCCESS(Status)) {
        NetpDhcpDestroyContext(DhcpContext);
        return Status;
    }

    return STATUS_SUCCESS;
}

VOID
NetpDhcpReleaseThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine attempts to release the IP address previously assigned via
    DHCP.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread, in
        this case a pointer to the DHCP context structure.

Return Value:

    None.

--*/

{

    PDHCP_CONTEXT DhcpContext;
    NETWORK_ADDRESS LocalAddress;
    BOOL LockHeld;
    KSTATUS Status;
    PSTR Step;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    DhcpContext = (PDHCP_CONTEXT)Parameter;
    LockHeld = FALSE;
    Step = "Init";

    //
    // Create the scratch packet space and the socket.
    //

    DhcpContext->ScratchPacket = MmAllocatePagedPool(DHCP_SCRATCH_PACKET_SIZE,
                                                     DHCP_ALLOCATION_TAG);

    if (DhcpContext->ScratchPacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DhcpLeaseReleaseThreadEnd;
    }

    DhcpContext->ScratchPacketSize = DHCP_SCRATCH_PACKET_SIZE;
    Status = MmCreateIoBuffer(DhcpContext->ScratchPacket,
                              DhcpContext->ScratchPacketSize,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &(DhcpContext->ScratchPacketIoBuffer));

    if (!KSUCCESS(Status)) {
        goto DhcpLeaseReleaseThreadEnd;
    }

    Status = IoSocketCreate(NetDomainIp4,
                            NetSocketDatagram,
                            SOCKET_INTERNET_PROTOCOL_UDP,
                            0,
                            &(DhcpContext->Socket));

    if (!KSUCCESS(Status)) {
        goto DhcpLeaseReleaseThreadEnd;
    }

    //
    // Bind that socket to the known DHCP client port. Make sure the link
    // address is still valid before copying it.
    //

    KeAcquireQueuedLock(DhcpContext->Link->QueuedLock);
    LockHeld = TRUE;
    if (DhcpContext->LinkAddress->Configured == FALSE) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto DhcpLeaseReleaseThreadEnd;
    }

    RtlCopyMemory(&LocalAddress,
                  &(DhcpContext->LinkAddress->Address),
                  sizeof(NETWORK_ADDRESS));

    KeReleaseQueuedLock(DhcpContext->Link->QueuedLock);
    LockHeld = FALSE;
    LocalAddress.Port = DHCP_CLIENT_PORT;
    Status = NetpDhcpBind(DhcpContext, &LocalAddress);
    if (!KSUCCESS(Status)) {
        goto DhcpLeaseReleaseThreadEnd;
    }

    //
    // Request the address that came back.
    //

    Step = "SendRelease";
    Status = NetpDhcpSendRelease(DhcpContext);
    if (!KSUCCESS(Status)) {
        goto DhcpLeaseReleaseThreadEnd;
    }

DhcpLeaseReleaseThreadEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(DhcpContext->Link->QueuedLock);
    }

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Net: DHCP lease release failed at step '%s': %d.\n",
                      Step,
                      Status);
    }

    NetpDhcpDestroyContext(DhcpContext);
    return;
}

KSTATUS
NetpDhcpSendDiscover (
    PDHCP_CONTEXT Context
    )

/*++

Routine Description:

    This routine sends the DHCP discovery request out onto the subnet.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

Return Value:

    Status code indicating whether or not the discovery request was
    successfully sent.

--*/

{

    PBYTE OptionByte;
    SOCKET_IO_PARAMETERS Parameters;
    IP4_ADDRESS RemoteAddress;
    PDHCP_PACKET Request;
    KSTATUS Status;
    ULONG TotalPacketSize;

    Request = NULL;

    //
    // Initialize the DHCP discover request.
    //

    Request = Context->ScratchPacket;
    RtlZeroMemory(Request, sizeof(DHCP_PACKET));
    Request->OperationCode = DHCP_OPERATION_REQUEST;
    Request->HardwareType = DHCP_HARDWARE_TYPE_ETHERNET;
    Request->HardwareAddressLength = DHCP_ETHERNET_HARDWARE_ADDRESS_LENGTH;
    Request->Hops = 0;
    Request->TransactionIdentifier = HlQueryTimeCounter() & MAX_ULONG;
    Context->ExpectedTransactionId = Request->TransactionIdentifier;
    Request->Seconds = 0;
    Request->Flags = 0;
    Request->ClientIpAddress = 0;
    RtlCopyMemory(&(Request->ClientHardwareAddress),
                  &(Context->LinkAddress->PhysicalAddress.Address),
                  DHCP_ETHERNET_HARDWARE_ADDRESS_LENGTH);

    Request->MagicCookie = CPU_TO_NETWORK32(DHCP_MAGIC_COOKIE);

    //
    // Initialize the options, which come right after the request. The first
    // option tells the server this is a discovery request. Options are always
    // a tuple of (OptionType, LengthOfValue, Value).
    //

    OptionByte = (PBYTE)(Request + 1);
    *OptionByte = DHCP_OPTION_DHCP_MESSAGE_TYPE;
    OptionByte += 1;
    *OptionByte = 1;
    OptionByte += 1;
    *OptionByte = DHCP_MESSAGE_DISCOVER;
    OptionByte += 1;

    //
    // Add the parameter request list option. Five parameters are being
    // requested with the offer.
    //

    *OptionByte = DHCP_OPTION_PARAMETER_REQUEST_LIST;
    OptionByte += 1;
    *OptionByte = 5;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_SUBNET_MASK;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_ROUTER;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_DOMAIN_NAME;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_DOMAIN_NAME_SERVER;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_IP_ADDRESS_LEASE_TIME;
    OptionByte += 1;

    //
    // Add the end tag.
    //

    *OptionByte = DHCP_OPTION_END;
    OptionByte += 1;
    *OptionByte = 0;
    OptionByte += 1;
    TotalPacketSize = (UINTN)OptionByte - (UINTN)Request;

    ASSERT(TotalPacketSize <= Context->ScratchPacketSize);

    //
    // Send off this request!
    //

    RtlZeroMemory(&RemoteAddress, sizeof(NETWORK_ADDRESS));
    RemoteAddress.Domain = NetDomainIp4;
    RemoteAddress.Address = IP4_BROADCAST_ADDRESS;
    RemoteAddress.Port = DHCP_SERVER_PORT;
    RtlZeroMemory(&Parameters, sizeof(SOCKET_IO_PARAMETERS));
    Parameters.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = &(RemoteAddress.NetworkAddress);
    Parameters.Size = TotalPacketSize;
    Status = IoSocketSendData(TRUE,
                              Context->Socket,
                              &Parameters,
                              Context->ScratchPacketIoBuffer);

    if (!KSUCCESS(Status)) {
        goto DhcpSendDiscoverEnd;
    }

    if (Parameters.BytesCompleted != TotalPacketSize) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto DhcpSendDiscoverEnd;
    }

    Status = STATUS_SUCCESS;

DhcpSendDiscoverEnd:
    return Status;
}

KSTATUS
NetpDhcpReceiveOffer (
    PDHCP_CONTEXT Context
    )

/*++

Routine Description:

    This routine receives the DHCP offer response, hopefully.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

Return Value:

    Status code.

--*/

{

    ULONG Attempts;
    DHCP_REPLY Reply;
    KSTATUS Status;

    Attempts = 5;
    while (Attempts != 0) {
        Attempts -= 1;

        //
        // Atempt to receive a reply from the DHCP server. Quit if the request
        // times out but try again for any other failure.
        //

        Status = NetpDhcpReceiveReply(Context, &Reply);
        if (Status == STATUS_TIMEOUT) {
            break;
        }

        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Try again if an offer message was not received.
        //

        if (Reply.MessageType != DHCP_MESSAGE_OFFER) {
            RtlDebugPrint("Skipping DHCP message as it wasn't an offer (%d), "
                          "instead it had a message type of %d.\n",
                          DHCP_MESSAGE_OFFER,
                          Reply.MessageType);

            continue;
        }

        //
        // Copy the reply to the context. This will make sure that all the
        // required information is present in the reply.
        //

        Status = NetpDhcpCopyReplyToContext(Context, &Reply);
        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Print the offer to the debugger now that the context is filled in,
        // if requested.
        //

        if ((NetDhcpDebugFlags & DHCP_DEBUG_FLAG_OFFER) != 0) {
            RtlDebugPrint("Net: DHCP Offer\n");
            NetpDhcpPrintContext(Context);
        }

        Status = STATUS_SUCCESS;
        break;
    }

    return Status;
}

KSTATUS
NetpDhcpSendRequest (
    PDHCP_CONTEXT Context
    )

/*++

Routine Description:

    This routine sends the DHCP address request out onto the subnet.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

    Renew - Supplies a boolean indicating whether or not this is a request to
        renew a lease.

Return Value:

    Status code indicating whether or not the discovery request was
    successfully sent.

--*/

{

    PIP4_ADDRESS Ip4Address;
    PDHCP_LEASE Lease;
    BOOL LockHeld;
    PBYTE OptionByte;
    SOCKET_IO_PARAMETERS Parameters;
    IP4_ADDRESS RemoteAddress;
    PDHCP_PACKET Request;
    PIP4_ADDRESS RequestedIp4Address;
    PIP4_ADDRESS ServerIp4Address;
    KSTATUS Status;
    ULONG TotalPacketSize;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Context->Lease != NULL);

    Lease = Context->Lease;
    LockHeld = FALSE;
    Request = NULL;

    //
    // Initialize the DHCP discover request.
    //

    Request = Context->ScratchPacket;
    RtlZeroMemory(Request, sizeof(DHCP_PACKET));
    Request->OperationCode = DHCP_OPERATION_REQUEST;
    Request->HardwareType = DHCP_HARDWARE_TYPE_ETHERNET;
    Request->HardwareAddressLength = DHCP_ETHERNET_HARDWARE_ADDRESS_LENGTH;
    Request->Hops = 0;
    Request->TransactionIdentifier = Context->ExpectedTransactionId;
    Request->Seconds = 0;
    Request->Flags = 0;
    Request->ClientIpAddress = 0;

    //
    // To renew or rebind a lease, the client IP address is set in the reqest
    // header.
    //

    if ((Lease->State == DhcpLeaseStateRenewing) ||
        (Lease->State == DhcpLeaseStateRebinding)) {

        //
        // Renew and rebind are trying to extend an already configured link
        // address's IP address. Make sure that the link address is still
        // configured before copying its data.
        //

        KeAcquireQueuedLock(Context->Link->QueuedLock);
        LockHeld = TRUE;
        if (Context->LinkAddress->Configured == FALSE) {
            Status = STATUS_NO_NETWORK_CONNECTION;
            goto DhcpSendRequestEnd;
        }

        Ip4Address = (PIP4_ADDRESS)&(Context->LinkAddress->Address);

        ASSERT(Ip4Address->Domain == NetDomainIp4);
        ASSERT(Ip4Address->Address != 0);

        Request->ClientIpAddress = Ip4Address->Address;
        KeReleaseQueuedLock(Context->Link->QueuedLock);
        LockHeld = FALSE;
    }

    //
    // The physical address of a link address entry does not change. There is
    // no need to acquire the lock here.
    //

    RtlCopyMemory(&(Request->ClientHardwareAddress),
                  &(Context->LinkAddress->PhysicalAddress.Address),
                  DHCP_ETHERNET_HARDWARE_ADDRESS_LENGTH);

    Request->MagicCookie = CPU_TO_NETWORK32(DHCP_MAGIC_COOKIE);

    //
    // Initialize the options, which come right after the request. The first
    // option tells the server this is a request (for an address). Options are
    // always a tuple of (OptionType, LengthOfValue, Value).
    //

    OptionByte = (PBYTE)(Request + 1);
    *OptionByte = DHCP_OPTION_DHCP_MESSAGE_TYPE;
    OptionByte += 1;
    *OptionByte = 1;
    OptionByte += 1;
    *OptionByte = DHCP_MESSAGE_REQUEST;
    OptionByte += 1;

    //
    // Add the parameter request list option. Five parameters are being
    // requested. This must match the parameters requested during the
    // discover message (if a discover message was sent before the request).
    //

    *OptionByte = DHCP_OPTION_PARAMETER_REQUEST_LIST;
    OptionByte += 1;
    *OptionByte = 5;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_SUBNET_MASK;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_ROUTER;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_DOMAIN_NAME;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_DOMAIN_NAME_SERVER;
    OptionByte += 1;
    *OptionByte = DHCP_OPTION_IP_ADDRESS_LEASE_TIME;
    OptionByte += 1;

    //
    // The requested IP address and server identifier options must not be sent
    // on a renew or rebind request.
    //

    if ((Lease->State != DhcpLeaseStateRenewing) &&
        (Lease->State != DhcpLeaseStateRebinding)) {

        //
        // Add the requested address.
        //

        ASSERT(sizeof(ULONG) == 4);

        RequestedIp4Address = (PIP4_ADDRESS)(&(Context->OfferClientAddress));
        *OptionByte = DHCP_OPTION_REQUESTED_IP_ADDRESS;
        OptionByte += 1;
        *OptionByte = 4;
        OptionByte += 1;
        *((PULONG)OptionByte) = RequestedIp4Address->Address;
        OptionByte += sizeof(ULONG);

        //
        // Add the server address.
        //

        ServerIp4Address = (PIP4_ADDRESS)(&(Context->OfferServerAddress));
        *OptionByte = DHCP_OPTION_DHCP_SERVER;
        OptionByte += 1;
        *OptionByte = 4;
        OptionByte += 1;
        *((PULONG)OptionByte) = ServerIp4Address->Address;
        OptionByte += sizeof(ULONG);
    }

    //
    // Add the end tag.
    //

    *OptionByte = DHCP_OPTION_END;
    OptionByte += 1;
    *OptionByte = 0;
    OptionByte += 1;
    TotalPacketSize = (UINTN)OptionByte - (UINTN)Request;

    ASSERT(TotalPacketSize <= Context->ScratchPacketSize);

    //
    // Record the time at which the request was sent. This will be considered
    // the lease start time upon success.
    //

    KeGetSystemTime(&(Context->LeaseRequestTime));

    //
    // Send off this request! On renew, it is unicast to the server that
    // initially offered the lease. Otherwise it's broadcast to let all the
    // other DHCP servers that made offers know which one was selected.
    //

    RtlZeroMemory(&RemoteAddress, sizeof(NETWORK_ADDRESS));
    RemoteAddress.Domain = NetDomainIp4;
    if (Lease->State == DhcpLeaseStateRenewing) {

        //
        // Make sure that the link address is still configured before copying
        // its data.
        //

        KeAcquireQueuedLock(Context->Link->QueuedLock);
        LockHeld = TRUE;
        if (Context->LinkAddress->Configured == FALSE) {
            Status = STATUS_NO_NETWORK_CONNECTION;
            goto DhcpSendRequestEnd;
        }

        Ip4Address = (PIP4_ADDRESS)&(Context->LinkAddress->LeaseServerAddress);

        ASSERT(Ip4Address->Domain == NetDomainIp4);
        ASSERT(Ip4Address->Address != 0);

        RemoteAddress.Address = Ip4Address->Address;
        KeReleaseQueuedLock(Context->Link->QueuedLock);
        LockHeld = FALSE;

    } else {
        RemoteAddress.Address = IP4_BROADCAST_ADDRESS;
    }

    RemoteAddress.Port = DHCP_SERVER_PORT;
    RtlZeroMemory(&Parameters, sizeof(SOCKET_IO_PARAMETERS));
    Parameters.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = &(RemoteAddress.NetworkAddress);
    Parameters.Size = TotalPacketSize;
    Status = IoSocketSendData(TRUE,
                              Context->Socket,
                              &Parameters,
                              Context->ScratchPacketIoBuffer);

    if (!KSUCCESS(Status)) {
        goto DhcpSendRequestEnd;
    }

    if (Parameters.BytesCompleted != TotalPacketSize) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto DhcpSendRequestEnd;
    }

    Status = STATUS_SUCCESS;

DhcpSendRequestEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Context->Link->QueuedLock);
    }

    return Status;
}

KSTATUS
NetpDhcpReceiveAcknowledge (
    PDHCP_CONTEXT Context
    )

/*++

Routine Description:

    This routine receives the acknowledgement from the DHCP server to the
    request just made.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

Return Value:

    Status code.

--*/

{

    ULONG Attempts;
    PIP4_ADDRESS ClientIp4Address;
    DHCP_REPLY Reply;
    PIP4_ADDRESS ServerIp4Address;
    KSTATUS Status;

    Attempts = 5;
    while (Attempts != 0) {
        Attempts -= 1;

        //
        // Atempt to receive a reply from the DHCP server. Quit if the request
        // times out but try again for any other failure.
        //

        Status = NetpDhcpReceiveReply(Context, &Reply);
        if (Status == STATUS_TIMEOUT) {
            break;
        }

        if (!KSUCCESS(Status)) {
            continue;
        }

        //
        // Try again if an acknowledge message was not received.
        //

        if (Reply.MessageType != DHCP_MESSAGE_ACKNOWLEDGE) {
            RtlDebugPrint("Skipping DHCP message as it wasn't an ACK (%d), "
                          "instead it had a message type of %d.\n",
                          DHCP_MESSAGE_ACKNOWLEDGE,
                          Reply.MessageType);

            continue;
        }

        //
        // If the DHCP lease is initializing, then an offer should have already
        // been received and stored in the context. If the client or server
        // addresses were provided in the acknowledgement, ensure they are the
        // same as the original offer.
        //

        if (Context->Lease->State == DhcpLeaseStateInitialize) {
            ServerIp4Address = (PIP4_ADDRESS)(&(Context->OfferServerAddress));
            ClientIp4Address = (PIP4_ADDRESS)(&(Context->OfferClientAddress));
            if (((Reply.ServerIpAddress != 0) &&
                 (Reply.ServerIpAddress != ServerIp4Address->Address)) ||
                ((Reply.OfferedIpAddress != 0) &&
                 (Reply.OfferedIpAddress != ClientIp4Address->Address))) {

                continue;
            }

            //
            // If the lease time does not equal the offer's lease time, then
            // recalculate the renewal and rebinding times if they were not
            // supplied.
            //

            if ((Reply.LeaseTime != 0) &&
                (Reply.LeaseTime != Context->LeaseTime)) {

                if (Reply.RenewalTime == 0) {
                    Reply.RenewalTime = DHCP_GET_DEFAULT_RENEWAL_TIME(
                                                              Reply.LeaseTime);
                }

                if (Reply.RebindingTime == 0) {
                    Reply.RebindingTime = DHCP_GET_DEFAULT_REBINDING_TIME(
                                                              Reply.LeaseTime);
                }

                Context->LeaseTime = Reply.LeaseTime;
                Context->RenewalTime = Reply.RenewalTime;
                Context->RebindingTime = Reply.RebindingTime;
            }

        //
        // For the renewal and rebinding states, the acknowledgement should
        // contain the complete information for the renewal offer. Copy the
        // whole reply to the context. If something is missing, the copy
        // routine will fail.
        //

        } else {

            ASSERT((Context->Lease->State == DhcpLeaseStateRenewing) ||
                   (Context->Lease->State == DhcpLeaseStateRebinding));

            Status = NetpDhcpCopyReplyToContext(Context, &Reply);
            if (!KSUCCESS(Status)) {
                continue;
            }
        }

        Status = STATUS_SUCCESS;
        break;
    }

    return Status;
}

KSTATUS
NetpDhcpSendRelease (
    PDHCP_CONTEXT Context
    )

/*++

Routine Description:

    This routine sends a release message to the DHCP server in order to release
    the IP address that the server leased to it.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

Return Value:

    Status code.

--*/

{

    PIP4_ADDRESS Ip4Address;
    PBYTE OptionByte;
    SOCKET_IO_PARAMETERS Parameters;
    IP4_ADDRESS RemoteAddress;
    PDHCP_PACKET Request;
    PIP4_ADDRESS ServerIp4Address;
    KSTATUS Status;
    ULONG TotalPacketSize;

    Request = NULL;

    //
    // Initialize the DHCP release request.
    //

    Request = Context->ScratchPacket;
    RtlZeroMemory(Request, sizeof(DHCP_PACKET));
    Request->OperationCode = DHCP_OPERATION_REQUEST;
    Request->HardwareType = DHCP_HARDWARE_TYPE_ETHERNET;
    Request->HardwareAddressLength = DHCP_ETHERNET_HARDWARE_ADDRESS_LENGTH;
    Request->Hops = 0;
    Request->TransactionIdentifier = HlQueryTimeCounter() & MAX_ULONG;
    Context->ExpectedTransactionId = Request->TransactionIdentifier;
    Request->Seconds = 0;
    Request->Flags = 0;
    Ip4Address = (PIP4_ADDRESS)&(Context->OfferClientAddress);

    ASSERT(Ip4Address->Domain == NetDomainIp4);
    ASSERT(Ip4Address->Address != 0);

    Request->ClientIpAddress = Ip4Address->Address;
    RtlCopyMemory(&(Request->ClientHardwareAddress),
                  &(Context->LinkAddress->PhysicalAddress.Address),
                  DHCP_ETHERNET_HARDWARE_ADDRESS_LENGTH);

    Request->MagicCookie = CPU_TO_NETWORK32(DHCP_MAGIC_COOKIE);

    //
    // Initialize the options, which come right after the request. The first
    // option tells the server this is a release request. Options are always
    // a tuple of (OptionType, LengthOfValue, Value).
    //

    OptionByte = (PBYTE)(Request + 1);
    *OptionByte = DHCP_OPTION_DHCP_MESSAGE_TYPE;
    OptionByte += 1;
    *OptionByte = 1;
    OptionByte += 1;
    *OptionByte = DHCP_MESSAGE_RELEASE;
    OptionByte += 1;

    //
    // Add the server address.
    //

    ServerIp4Address = (PIP4_ADDRESS)(&(Context->OfferServerAddress));

    ASSERT(ServerIp4Address->Domain == NetDomainIp4);
    ASSERT(ServerIp4Address->Address != 0);

    *OptionByte = DHCP_OPTION_DHCP_SERVER;
    OptionByte += 1;
    *OptionByte = 4;
    OptionByte += 1;
    *((PULONG)OptionByte) = ServerIp4Address->Address;
    OptionByte += sizeof(ULONG);

    //
    // Add the end tag.
    //

    *OptionByte = DHCP_OPTION_END;
    OptionByte += 1;
    *OptionByte = 0;
    OptionByte += 1;
    TotalPacketSize = (UINTN)OptionByte - (UINTN)Request;

    ASSERT(TotalPacketSize <= Context->ScratchPacketSize);

    //
    // Send off this request!
    //

    RtlZeroMemory(&RemoteAddress, sizeof(NETWORK_ADDRESS));
    RemoteAddress.Domain = NetDomainIp4;
    Ip4Address = (PIP4_ADDRESS)&(Context->OfferServerAddress);

    ASSERT(Ip4Address->Domain == NetDomainIp4);
    ASSERT(Ip4Address->Address != 0);

    RemoteAddress.Address = Ip4Address->Address;
    RemoteAddress.Port = DHCP_SERVER_PORT;
    RtlZeroMemory(&Parameters, sizeof(SOCKET_IO_PARAMETERS));
    Parameters.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = &(RemoteAddress.NetworkAddress);
    Parameters.Size = TotalPacketSize;
    Status = IoSocketSendData(TRUE,
                              Context->Socket,
                              &Parameters,
                              Context->ScratchPacketIoBuffer);

    if (!KSUCCESS(Status)) {
        goto DhcpSendDiscoverEnd;
    }

    if (Parameters.BytesCompleted != TotalPacketSize) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto DhcpSendDiscoverEnd;
    }

    Status = STATUS_SUCCESS;

DhcpSendDiscoverEnd:
    return Status;
}

KSTATUS
NetpDhcpReceiveReply (
    PDHCP_CONTEXT Context,
    PDHCP_REPLY Reply
    )

/*++

Routine Description:

    This routine attempts to receive a replay from the DHCP server for either
    an offer or an acknowledge packet.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

    Reply - Supplies a pointer to the DHCP reply context that is to be filled
        in by this routine.

Return Value:

    Status code.

--*/

{

    ULONG AddressOffset;
    ULONG Offset;
    BYTE OptionByte;
    PBYTE OptionBytePointer;
    BYTE OptionLength;
    ULONGLONG PacketSize;
    SOCKET_IO_PARAMETERS Parameters;
    ULONG RebindingTime;
    PDHCP_PACKET Response;
    ULONG RouterIp;
    NETWORK_ADDRESS ServerAddress;
    ULONG ServerIp;
    KSTATUS Status;

    Response = Context->ScratchPacket;
    OptionBytePointer = (PUCHAR)Response;
    RtlZeroMemory(&Parameters, sizeof(SOCKET_IO_PARAMETERS));
    Parameters.TimeoutInMilliseconds = DHCP_ACKNOWLEDGE_TIMEOUT;
    Parameters.NetworkAddress = &ServerAddress;
    Parameters.Size = Context->ScratchPacketSize;
    Status = IoSocketReceiveData(TRUE,
                                 Context->Socket,
                                 &Parameters,
                                 Context->ScratchPacketIoBuffer);

    if (Status == STATUS_TIMEOUT) {
        goto DhcpReceiveReplyEnd;
    }

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("NetpDhcpReceiveReply skipping packet because receive "
                      "status was %d.\n",
                      Status);

        goto DhcpReceiveReplyEnd;
    }

    PacketSize = Parameters.BytesCompleted;

    //
    // Validate some basic attributes about the packet.
    //

    if (PacketSize < sizeof(DHCP_PACKET)) {
        RtlDebugPrint("DHCP ack packet too small. Was %d bytes, should "
                      "have been at least %d bytes.\n",
                      PacketSize,
                      sizeof(DHCP_PACKET));

        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto DhcpReceiveReplyEnd;
    }

    if (Response->OperationCode != DHCP_OPERATION_REPLY) {
        Status = STATUS_UNSUCCESSFUL;
        goto DhcpReceiveReplyEnd;
    }

    if ((Response->HardwareType != DHCP_HARDWARE_TYPE_ETHERNET) ||
        (Response->HardwareAddressLength !=
         DHCP_ETHERNET_HARDWARE_ADDRESS_LENGTH)) {

        RtlDebugPrint("DHCP packet skipped because hardware type or "
                      "length didn't match standard ethernet.\n");

        Status = STATUS_UNSUCCESSFUL;
        goto DhcpReceiveReplyEnd;
    }

    if (NETWORK_TO_CPU32(Response->MagicCookie) != DHCP_MAGIC_COOKIE) {
        RtlDebugPrint("DHCP packet skipped because the magic cookie was "
                      "wrong.\n");

        Status = STATUS_UNSUCCESSFUL;
        goto DhcpReceiveReplyEnd;
    }

    //
    // Quietly skip packets not directed at this request.
    //

    if (Response->TransactionIdentifier != Context->ExpectedTransactionId) {
        Status = STATUS_UNSUCCESSFUL;
        goto DhcpReceiveReplyEnd;
    }

    RtlZeroMemory(Reply, sizeof(DHCP_REPLY));
    Reply->ServerIpAddress = Response->ServerIpAddress;
    Reply->OfferedIpAddress = Response->YourIpAddress;

    //
    // Parse the options.
    //

    Offset = sizeof(DHCP_PACKET);
    while (Offset < PacketSize) {
        OptionByte = OptionBytePointer[Offset];
        Offset += 1;

        //
        // Skip padding.
        //

        if (OptionByte == DHCP_OPTION_PAD) {
            continue;
        }

        //
        // Stop if the end is reached.
        //

        if (OptionByte == DHCP_OPTION_END) {
            break;
        }

        //
        // Get the length of the option.
        //

        if (Offset >= PacketSize) {
            break;
        }

        OptionLength = OptionBytePointer[Offset];
        Offset += 1;

        //
        // Stop if the entire option value cannot be retrieved.
        //

        if (Offset + OptionLength > PacketSize) {
            break;
        }

        //
        // Parse the known options, starting with the message type.
        //

        if (OptionByte == DHCP_OPTION_DHCP_MESSAGE_TYPE) {
            Reply->MessageType = OptionBytePointer[Offset];

        } else if (OptionByte == DHCP_OPTION_DHCP_SERVER) {
            if (OptionLength == 4) {
                ServerIp = *((PULONG)(&(OptionBytePointer[Offset])));
                Reply->ServerIpAddress = ServerIp;
            }

        } else if (OptionByte == DHCP_OPTION_SUBNET_MASK) {
            if (OptionLength == 4) {
                Reply->SubnetMask = *((PULONG)(&(OptionBytePointer[Offset])));
            }

        } else if (OptionByte == DHCP_OPTION_ROUTER) {
            if (OptionLength == 4) {
                RouterIp = *((PULONG)(&(OptionBytePointer[Offset])));
                Reply->RouterIpAddress = RouterIp;
            }

        } else if (OptionByte == DHCP_OPTION_DOMAIN_NAME_SERVER) {
            AddressOffset = Offset;
            while (AddressOffset + 4 <= Offset + OptionLength) {
                Reply->DomainNameServer[Reply->DomainNameServerCount] =
                              *((PULONG)(&(OptionBytePointer[AddressOffset])));

                AddressOffset += 4;
                Reply->DomainNameServerCount += 1;
                if (Reply->DomainNameServerCount == DHCP_MAX_DNS_SERVERS) {
                    break;
                }
            }

        } else if (OptionByte == DHCP_OPTION_IP_ADDRESS_LEASE_TIME) {
            if (OptionLength == 4) {
                Reply->LeaseTime = *((PULONG)(&(OptionBytePointer[Offset])));
                Reply->LeaseTime = NETWORK_TO_CPU32(Reply->LeaseTime);
            }

        } else if (OptionByte == DHCP_OPTION_RENEWAL_TIME) {
            if (OptionLength == 4) {
                Reply->RenewalTime = *((PULONG)(&(OptionBytePointer[Offset])));
                Reply->RenewalTime = NETWORK_TO_CPU32(Reply->RenewalTime);
            }

        } else if (OptionByte == DHCP_OPTION_REBINDING_TIME) {
            if (OptionLength == 4) {
                RebindingTime = *((PULONG)(&(OptionBytePointer[Offset])));
                Reply->RebindingTime = NETWORK_TO_CPU32(RebindingTime);
            }
        }

        //
        // Skip over the option length.
        //

        Offset += OptionLength;
    }

    ASSERT(Status == STATUS_SUCCESS);

    //
    // Set the override renewal and rebinding times if enabled.
    //

    if (NetDhcpDebugOverrideRenewal != FALSE) {
        Reply->RenewalTime = NetDhcpDebugRenewalTime;
        Reply->RebindingTime = NetDhcpDebugRebindingTime;
    }

DhcpReceiveReplyEnd:
    return Status;
}

PDHCP_CONTEXT
NetpDhcpCreateContext (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PDHCP_LEASE Lease
    )

/*++

Routine Description:

    This routine creates a DHCP context.

Arguments:

    Link - Supplies a pointer to the network link for the DHCP context.

    LinkAddress - Supplies a pointer to the network link address for the DHCP
        context.

    Lease - Supplies an optional pointer to a DHCP lease to be associated with
        the context.

Return Value:

    Returns a pointer to the newly allocated DHCP context on success, or NULL
    on failure.

--*/

{

    PDHCP_CONTEXT Context;

    Context = MmAllocatePagedPool(sizeof(DHCP_CONTEXT), DHCP_ALLOCATION_TAG);
    if (Context == NULL) {
        return NULL;
    }

    RtlZeroMemory(Context, sizeof(DHCP_CONTEXT));
    NetLinkAddReference(Link);
    Context->Link = Link;
    Context->LinkAddress = LinkAddress;
    if (Lease != NULL) {
        NetpDhcpLeaseAddReference(Lease);
        Context->Lease = Lease;
    }

    return Context;
}

VOID
NetpDhcpDestroyContext (
    PDHCP_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the given DHCP context.

Arguments:

    Context - Supplies a pointer to the DHCP context to destroy.

Return Value:

    None.

--*/

{

    if (Context->Socket != NULL) {
        IoClose(Context->Socket);
    }

    if (Context->ScratchPacketIoBuffer != NULL) {
        MmFreeIoBuffer(Context->ScratchPacketIoBuffer);
    }

    if (Context->ScratchPacket != NULL) {
        MmFreePagedPool(Context->ScratchPacket);
    }

    ASSERT(Context->Link != NULL);

    NetLinkReleaseReference(Context->Link);
    if (Context->Lease != NULL) {
        NetpDhcpLeaseReleaseReference(Context->Lease);
    }

    MmFreePagedPool(Context);
    return;
}

PDHCP_LEASE
NetpDhcpCreateLease (
    VOID
    )

/*++

Routine Description:

    This routine creates an the context for a DHCP lease.

Arguments:

    None.

Return Value:

    Returns a pointer to a DHCP lease context on success, or NULL on failure.

--*/

{

    PDHCP_LEASE NewLease;
    KSTATUS Status;

    NewLease = MmAllocateNonPagedPool(sizeof(DHCP_LEASE), DHCP_ALLOCATION_TAG);
    if (NewLease == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DhcpCreateAssignmentContextEnd;
    }

    RtlZeroMemory(NewLease, sizeof(DHCP_LEASE));
    NewLease->ReferenceCount = 1;
    NewLease->Timer = KeCreateTimer(DHCP_ALLOCATION_TAG);
    if (NewLease->Timer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DhcpCreateAssignmentContextEnd;
    }

    NewLease->Dpc = KeCreateDpc(NetpDhcpLeaseDpcRoutine, NewLease);
    if (NewLease->Dpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DhcpCreateAssignmentContextEnd;
    }

    NewLease->WorkItem = KeCreateWorkItem(NULL,
                                          WorkPriorityNormal,
                                          NetpDhcpLeaseWorkRoutine,
                                          NewLease,
                                          DHCP_ALLOCATION_TAG);

    if (NewLease->WorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DhcpCreateAssignmentContextEnd;
    }

    Status = STATUS_SUCCESS;

DhcpCreateAssignmentContextEnd:
    if (!KSUCCESS(Status)) {
        if (NewLease != NULL) {
            NetpDhcpDestroyLease(NewLease);
            NewLease = NULL;
        }
    }

    return NewLease;
}

VOID
NetpDhcpLeaseAddReference (
    PDHCP_LEASE Lease
    )

/*++

Routine Description:

    This routine increases the reference count on a DHCP lease.

Arguments:

    Lease - Supplies a pointer to the DHCP lease whose reference count should
        be incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Lease->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) & (OldReferenceCount < 0x20000000));

    return;
}

VOID
NetpDhcpLeaseReleaseReference (
    PDHCP_LEASE Lease
    )

/*++

Routine Description:

    This routine decreases the reference count of a DHCP lease, and destroys
    the lease if the reference count drops to zero.

Arguments:

    Lease - Supplies a pointer to the DHCP lease whose reference count should
        be decremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Lease->ReferenceCount), -1);

    ASSERT(OldReferenceCount != 0);

    if (OldReferenceCount == 1) {
        NetpDhcpDestroyLease(Lease);
    }

    return;
}

PDHCP_LEASE
NetpDhcpFindLease (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine attemps to find an existing lease for the given link and link
    address. If a lease is found, a reference is added to the lease.

Arguments:

    Link - Supplies a pointer to a network link.

    LinkAddress - Supplies a pointer to a network link address.

Return Value:

    Returns a pointer to the DHCP lease that exists for the given link and link
    address, or NULL if no such lease exists.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDHCP_LEASE CurrentLease;
    PDHCP_LEASE FoundLease;

    FoundLease = NULL;
    KeAcquireSpinLock(&NetDhcpLeaseListLock);
    CurrentEntry = NetDhcpLeaseListHead.Next;
    while (CurrentEntry != &(NetDhcpLeaseListHead)) {
        CurrentLease = LIST_VALUE(CurrentEntry, DHCP_LEASE, ListEntry);
        if ((CurrentLease->Link == Link) &&
            (CurrentLease->LinkAddress == LinkAddress)) {

            NetpDhcpLeaseAddReference(CurrentLease);
            FoundLease = CurrentLease;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSpinLock(&NetDhcpLeaseListLock);
    return FoundLease;
}

VOID
NetpDhcpDestroyLease (
    PDHCP_LEASE Lease
    )

/*++

Routine Description:

    This routine destroys a DHCP lease context.

Arguments:

    Lease - Supplies a pointer to a DHCP lease.

Return Value:

    None.

--*/

{

    ASSERT(Lease->ListEntry.Next == NULL);

    if (Lease->Link != NULL) {
        NetLinkReleaseReference(Lease->Link);
    }

    if (Lease->Timer != NULL) {
        KeDestroyTimer(Lease->Timer);
    }

    if (Lease->Dpc != NULL) {
        KeDestroyDpc(Lease->Dpc);
    }

    if (Lease->WorkItem != NULL) {
        KeDestroyWorkItem(Lease->WorkItem);
    }

    MmFreeNonPagedPool(Lease);
    return;
}

VOID
NetpDhcpQueueLeaseExtension (
    PDHCP_LEASE Lease
    )

/*++

Routine Description:

    This routine queues the lease timer to attempt a lease extension. It
    determines the correct duration for the timer.

Arguments:

    Lease - Supplies a pointer to the lease that needs its timer queued.

Return Value:

    None.

--*/

{

    SYSTEM_TIME CurrentSystemTime;
    ULONGLONG DueTime;
    LONGLONG ElapsedLeaseTime;
    ULONGLONG StateChangeTime;
    KSTATUS Status;
    ULONGLONG TimerDuration;
    ULONGLONG TimeToStateChange;

    if (Lease->State == DhcpLeaseStateBound) {

        ASSERT(Lease->RenewalTime != 0);

        TimerDuration = Lease->RenewalTime;

    } else {
        KeGetSystemTime(&CurrentSystemTime);
        ElapsedLeaseTime = CurrentSystemTime.Seconds -
                           Lease->LinkAddress->LeaseStartTime.Seconds;

        ASSERT(ElapsedLeaseTime >= 0);

        //
        // Determine the time of the next state change.
        //

        if (Lease->State == DhcpLeaseStateRenewing) {
            StateChangeTime = Lease->RebindingTime;

        } else {

            ASSERT(Lease->State == DhcpLeaseStateRebinding);

            StateChangeTime = Lease->LeaseTime;
        }

        //
        // Set the time for half the time until the next state change. If
        // that results in a timer duration that is less than the minimum,
        // just schedule the timer for the next state change time.
        //

        TimerDuration = 0;
        if (StateChangeTime > ElapsedLeaseTime) {
            TimeToStateChange = StateChangeTime - ElapsedLeaseTime;
            TimerDuration = TimeToStateChange >> 1;
            if (TimerDuration < DHCP_TIMER_DURATION_MINIMUM) {
                TimerDuration = TimeToStateChange;
            }
        }
    }

    TimerDuration *= MICROSECONDS_PER_SECOND;
    TimerDuration = KeConvertMicrosecondsToTimeTicks(TimerDuration);
    DueTime = HlQueryTimeCounter() + TimerDuration;
    Status = KeQueueTimer(Lease->Timer,
                          TimerQueueSoft,
                          DueTime,
                          0,
                          0,
                          Lease->Dpc);

    ASSERT(KSUCCESS(Status));

    return;
}

VOID
NetpDhcpLeaseDpcRoutine (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the DPC routine that fires when a lease timer
    expires. It queues the work item.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PDHCP_LEASE Lease;

    Lease = (PDHCP_LEASE)Dpc->UserData;
    KeQueueWorkItem(Lease->WorkItem);
    return;
}

VOID
NetpDhcpLeaseWorkRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the lease timer expiration work routine.

Arguments:

    Parameter - Supplies a pointer to a lease context.

Return Value:

    None.

--*/

{

    SYSTEM_TIME CurrentSystemTime;
    LONGLONG ElapsedLeaseTime;
    PDHCP_LEASE Lease;
    BOOL LinkUp;
    KSTATUS Status;

    Lease = (PDHCP_LEASE)Parameter;

    ASSERT(Lease->ReferenceCount >= 1);

    //
    // If the link is down then do not continue.
    //

    NetGetLinkState(Lease->Link, &LinkUp, NULL);
    if (LinkUp == FALSE) {
        return;
    }

    //
    // Determine the current state based on the time.
    //

    KeGetSystemTime(&CurrentSystemTime);
    ElapsedLeaseTime = CurrentSystemTime.Seconds -
                       Lease->LinkAddress->LeaseStartTime.Seconds;

    //
    // If the elapsed time is greater than the lease time, then move back to
    // the initialize phase.
    //

    if (ElapsedLeaseTime >= Lease->LeaseTime) {
        Lease->State = DhcpLeaseStateInitialize;

    //
    // If the elapsed time is greater than or equal to the rebinding time,
    // then the lease is in the rebinding state.
    //

    } else if (ElapsedLeaseTime >= Lease->RebindingTime) {
        Lease->State = DhcpLeaseStateRebinding;

    //
    // Otherwise, even if the elapsed time is less than the renewal time, move
    // the lease to the renewal state.
    //

    } else {
        Lease->State = DhcpLeaseStateRenewing;
    }

    //
    // If the lease is back in the initialization state, then the lease
    // expired. Try to re-initialize the address. Note that the lease cannot be
    // destroyed in its own work-item. The re-assignment process will handle
    // destroying the lease.
    //

    if (Lease->State == DhcpLeaseStateInitialize) {
        Status = NetpDhcpBeginAssignment(Lease->Link, Lease->LinkAddress);
        if (!KSUCCESS(Status)) {

            //
            // TODO: Handle failed DHCP.
            //

            ASSERT(FALSE);
        }

    //
    // Otherwise schedule work to try to renew (or rebind) the lease. If this
    // fails, try again later.
    //

    } else {
        Status = NetpDhcpBeginLeaseExtension(Lease);
        if (!KSUCCESS(Status)) {
            NetpDhcpQueueLeaseExtension(Lease);
        }
    }

    return;
}

KSTATUS
NetpDhcpCopyReplyToContext (
    PDHCP_CONTEXT Context,
    PDHCP_REPLY Reply
    )

/*++

Routine Description:

    This routine copies the state from the given reply into the context. It
    makes sure that all the necessary state is present in the reply.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

    Reply - Supplies a pointer to the DHCP reply context that will be copied to
        the DHCP context.

Return Value:

    Status code.

--*/

{

    ULONG AddressIndex;
    PIP4_ADDRESS Ip4Address;
    KSTATUS Status;

    if ((Reply->ServerIpAddress == 0) ||
        (Reply->OfferedIpAddress == 0) ||
        (Reply->RouterIpAddress == 0) ||
        (Reply->SubnetMask == 0) ||
        (Reply->DomainNameServerCount == 0) ||
        (Reply->LeaseTime == 0)) {

        RtlDebugPrint("DHCP: A required parameter was missing from the "
                      "reply:\n ServerIp: 0x%x\n OfferedIpAddress: 0x%x,\n "
                      "Router: 0x%x\n SubnetMask: 0x%x\n "
                      "DomainNameServerCount: 0x%x\n LeaseTime: 0x%x\n",
                      Reply->ServerIpAddress,
                      Reply->OfferedIpAddress,
                      Reply->RouterIpAddress,
                      Reply->SubnetMask,
                      Reply->DomainNameServerCount,
                      Reply->LeaseTime);

        Status = STATUS_INVALID_PARAMETER;
        goto DhcpCopyReplyToContextEnd;
    }

    //
    // Fill out the network address structures.
    //

    Ip4Address = (PIP4_ADDRESS)(&(Context->OfferClientAddress));
    Ip4Address->Domain = NetDomainIp4;
    Ip4Address->Address = Reply->OfferedIpAddress;
    Ip4Address = (PIP4_ADDRESS)(&(Context->OfferSubnetMask));
    Ip4Address->Domain = NetDomainIp4;
    Ip4Address->Address = Reply->SubnetMask;
    Ip4Address = (PIP4_ADDRESS)(&(Context->OfferServerAddress));
    Ip4Address->Domain = NetDomainIp4;
    Ip4Address->Address = Reply->ServerIpAddress;
    Ip4Address = (PIP4_ADDRESS)(&(Context->OfferRouter));
    Ip4Address->Domain = NetDomainIp4;
    Ip4Address->Address = Reply->RouterIpAddress;
    Context->OfferDnsAddressCount = Reply->DomainNameServerCount;
    for (AddressIndex = 0;
         AddressIndex < Reply->DomainNameServerCount;
         AddressIndex += 1) {

        Ip4Address =
                 (PIP4_ADDRESS)(&(Context->OfferDnsAddress[AddressIndex]));

        Ip4Address->Domain = NetDomainIp4;
        Ip4Address->Address = Reply->DomainNameServer[AddressIndex];
    }

    //
    // Copy the lease time information.
    //

    Context->LeaseTime = Reply->LeaseTime;
    if (Reply->RenewalTime == 0) {
        Reply->RenewalTime = DHCP_GET_DEFAULT_RENEWAL_TIME(Reply->LeaseTime);
    }

    Context->RenewalTime = Reply->RenewalTime;
    if (Reply->RebindingTime == 0) {
        Reply->RebindingTime = DHCP_GET_DEFAULT_REBINDING_TIME(
                                                             Reply->LeaseTime);
    }

    Context->RebindingTime = Reply->RebindingTime;
    Status = STATUS_SUCCESS;

DhcpCopyReplyToContextEnd:
    return Status;
}

KSTATUS
NetpDhcpBind (
    PDHCP_CONTEXT Context,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine attempts to bind the given context to the any address on the
    DHCP port. It is patient and will retry as multiple NICs may be coming up
    at the same time.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

    Address - Supplies the address to bind to.

Return Value:

    Status code.

--*/

{

    ULONGLONG Delay;
    KSTATUS Status;
    ULONG Try;

    Status = STATUS_SUCCESS;
    for (Try = 0; Try < DHCP_BIND_RETRY_COUNT; Try += 1) {
        Status = IoSocketBindToAddress(TRUE,
                                       Context->Socket,
                                       Context->Link,
                                       Address,
                                       NULL,
                                       0);

        if ((KSUCCESS(Status)) || (Status != STATUS_ADDRESS_IN_USE)) {
            break;
        }

        if (Try + 1 < DHCP_BIND_RETRY_COUNT) {
            KeGetRandomBytes(&Delay, sizeof(Delay));
            Delay = DHCP_BIND_DELAY + (Delay % DHCP_BIND_VARIANCE);
            KeDelayExecution(FALSE, FALSE, Delay);
        }
    }

    return Status;
}

VOID
NetpDhcpPrintContext (
    PDHCP_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints out the IP address and lease time information for a
    DHCP context.

Arguments:

    Context - Supplies a pointer to the DHCP context information.

Return Value:

    None.

--*/

{

    PCSTR DurationUnit;
    ULONG LeaseTime;
    PCSTR Plural;

    Plural = "s";
    if (Context->LeaseTime >= SECONDS_PER_DAY) {
        LeaseTime = Context->LeaseTime / SECONDS_PER_DAY;
        DurationUnit = "day";

    } else if (Context->LeaseTime >= SECONDS_PER_HOUR) {
        LeaseTime = Context->LeaseTime / SECONDS_PER_HOUR;
        DurationUnit = "hour";

    } else {
        LeaseTime = Context->LeaseTime / SECONDS_PER_MINUTE;
        DurationUnit = "minute";
    }

    if (LeaseTime == 1) {
        Plural = "";
    }

    RtlDebugPrint("%20s: %d.%d.%d.%d\n"
                  "%20s: %d.%d.%d.%d\n"
                  "%20s: %d.%d.%d.%d\n"
                  "%20s: %d.%d.%d.%d\n"
                  "%20s: %d %s%s.\n",
                  "Server IP",
                  (UCHAR)Context->OfferServerAddress.Address[0],
                  (UCHAR)(Context->OfferServerAddress.Address[0] >> 8),
                  (UCHAR)(Context->OfferServerAddress.Address[0] >> 16),
                  (UCHAR)(Context->OfferServerAddress.Address[0] >> 24),
                  "Offered IP",
                  (UCHAR)Context->OfferClientAddress.Address[0],
                  (UCHAR)(Context->OfferClientAddress.Address[0] >> 8),
                  (UCHAR)(Context->OfferClientAddress.Address[0] >> 16),
                  (UCHAR)(Context->OfferClientAddress.Address[0] >> 24),
                  "Router IP",
                  (UCHAR)Context->OfferRouter.Address[0],
                  (UCHAR)(Context->OfferRouter.Address[0] >> 8),
                  (UCHAR)(Context->OfferRouter.Address[0] >> 16),
                  (UCHAR)(Context->OfferRouter.Address[0] >> 24),
                  "DNS Server IP",
                  (UCHAR)Context->OfferDnsAddress[0].Address[0],
                  (UCHAR)(Context->OfferDnsAddress[0].Address[0] >> 8),
                  (UCHAR)(Context->OfferDnsAddress[0].Address[0] >> 16),
                  (UCHAR)(Context->OfferDnsAddress[0].Address[0] >> 24),
                  "Lease Time",
                  LeaseTime,
                  DurationUnit,
                  Plural);

    return;
}

