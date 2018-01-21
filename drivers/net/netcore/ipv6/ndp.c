/*++

Copyright (c) 2017 Minoca Corp. All Rights Reserved

Module Name:

    ndp.c

Abstract:

    This module implements support for the Neighbor Discovery Protocol, which
    translates network layer addresses (such as IP addresses) to physical
    addresses (such as MAC addresses) and allows a node to find routers and its
    neighbors.

Author:

    Chris Stevens 7-Sept-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Network layer drivers are supposed to be able to stand on their own (ie be
// able to be implemented outside the core net library). For the builtin once,
// avoid including netcore.h, but still redefine those functions that would
// otherwise generate imports.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/ip6.h>
#include <minoca/net/icmp6.h>

//
// While ICMPv6 and NDP are built into netcore and the same binary as IPv6,
// share the well-known addresses. This could easily be changed if the binaries
// need to be separated out.
//

#include "ip6addr.h"

//
// ---------------------------------------------------------------- Definitions
//

#define NDP_ALLOCATION_TAG 0x2170644E // '!pdN'

//
// Define the set of router advertisement flags.
//

#define NDP_ROUTER_FLAG_MANAGED_ADDRESS_CONFIGURATION 0x01
#define NDP_ROUTER_FLAG_OTHER_CONFIGURATION           0x02

//
// Define the set of neighbor advertisement flags.
//

#define NDP_NEIGHBOR_FLAG_ROUTER    0x01
#define NDP_NEIGHBOR_FLAG_SOLICITED 0x02
#define NDP_NEIGHBOR_FLAG_OVERRIDE  0x04

//
// Define the neighbor discovery message option types.
//

#define NDP_OPTION_TYPE_SOURCE_LINK_ADDRESS   0x01
#define NDP_OPTION_TYPE_TARGET_LINK_ADDRESS   0x02
#define NDP_OPTION_TYPE_PREFIX_INFORMATION    0x03
#define NDP_OPTION_TYPE_REDIRECTED_HEADER     0x04
#define NDP_OPTION_TYPE_MAX_TRANSMISSION_UNIT 0x05

//
// Define the bytes per unit of length for the NDP options.
//

#define NDP_OPTION_LENGTH_MULTIPLE 8

//
// Define the set of prefix information option flags.
//

#define NDP_PREFIX_FLAG_ON_LINK                          0x01
#define NDP_PREFIX_FLAG_AUTONOMOUS_ADDRESS_CONFIGURATION 0x02

//
// All NDP packets should go out with an IPv6 hop limit of 255.
//

#define NDP_IP6_HOP_LIMIT 255

//
// Define the maximum amount of time to delay a solicitation, in milliseconds.
//

#define NDP_SOLICITATION_DELAY_MAX 1000

//
// Define the default retransmit timer, in milliseconds.
//

#define NDP_DEFAULT_RETRANMIT_TIMEOUT 1000

//
// Define the default number of duplicate address detection transmits.
// RFC 4862 specifies the default as 1.
//

#define NDP_DEFAULT_DUPLICATE_ADDRESS_DETECTION_TRANSMIT_COUNT 1

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a router solicitation message. The message options
    immediately follow this structure.

Members:

    Header - Stores the ICMPv6 message header.

    Reserved - This field is reserved. Must be set to zero.

--*/

#pragma pack(push, 1)

typedef struct _NDP_ROUTER_SOLICITATION {
    ICMP6_HEADER Header;
    ULONG Reserved;
} PACKED NDP_ROUTER_SOLICITATION, *PNDP_ROUTER_SOLICITATION;

/*++

Structure Description:

    This structure defines a router advertisement message. The message options
    immediately follows this structure.

Members:

    Header - Stores the ICMPv6 message header.

    CurrentHopLimit - Stores the default value for the IPv6 hop limit that
        should be used for outgoing packets.

    Flags - Stores a bitmask of flags. See NDP_ROUTER_FLAG_* for definitions.

    RouterLifetime - Stores the lifetime of the router, in seconds.

    ReachableTime - Stores the time, in milliseconds, for which a node should
        assume a neighbor is reachable after receiving a reachability
        confirmation.

    RetransmitTimer - Stores the time, in milliseconds, to be waited between
        retransmitting neighbor solicitation messages.

--*/

typedef struct _NDP_ROUTER_ADVERTISEMENT {
    ICMP6_HEADER Header;
    UCHAR CurrentHopLimit;
    UCHAR Flags;
    USHORT RouterLifetime;
    ULONG ReachableTime;
    ULONG RetransmitTimer;
} PACKED NDP_ROUTER_ADVERTISEMENT, *PNDP_ROUTER_ADVERTISEMENT;

/*++

Structure Description:

    This structure defines a neighbor solicitation message. The message options
    immediately follow this structure.

Members:

    Header - Stores the ICMPv6 message header.

    Reserved - This field is reserved.

    TargetAddress - Stores the IPv6 address of the target that is being
        solicited. This cannot be a multicast address.

--*/

typedef struct _NDP_NEIGHBOR_SOLICITATION {
    ICMP6_HEADER Header;
    ULONG Reserved;
    ULONG TargetAddress[IP6_ADDRESS_SIZE / sizeof(ULONG)];
} PACKED NDP_NEIGHBOR_SOLICITATION, *PNDP_NEIGHBOR_SOLICITATION;

/*++

Structure Description:

    This structure defines a neighbor advertisement message. The message
    options immediately follow this structure.

Members:

    Header - Stores the ICMPv6 message header.

    Flags - Store a bitmask of neighbor advertisement flags. See
        NDP_NEIGHBOR_FLAG_* for definitions.

    TargetAddress - Stores the IPv6 address of the node whose link-layer
        address follows in the options. For solicited advertisements, this is
        the IPv6 address sent in the neighbor solicitation. For unsolicited
        advertisements, this is the IPv6 address of the node whose link-layer
        address has changed.

--*/

typedef struct _NDP_NEIGHBOR_ADVERTISEMENT {
    ICMP6_HEADER Header;
    ULONG Flags;
    ULONG TargetAddress[IP6_ADDRESS_SIZE / sizeof(ULONG)];
} PACKED NDP_NEIGHBOR_ADVERTISEMENT, *PNDP_NEIGHBOR_ADVERTISEMENT;

/*++

Structure Description:

    This structure defines an NDP redirect message. The message options
    immediately follow this structure.

Members:

    Header - Stores the ICMPv6 message header.

    Reserved - This field is reserved.

    TargetAddress - Stores the IPv6 address that a better first hop to use when
        comminicating with the destination address.

    DestinationAddress - Stores the IPv6 address for which communication should
        be redirected to the target address.

--*/

typedef struct _NDP_REDIRECT {
    ICMP6_HEADER Header;
    ULONG Reserved;
    ULONG TargetAddress[IP6_ADDRESS_SIZE / sizeof(ULONG)];
    ULONG DestinationAddress[IP6_ADDRESS_SIZE / sizeof(ULONG)];
} PACKED NDP_REDIRECT, *PNDP_REDIRECT;

/*++

Structure Description:

    This structure defines the header for an NDP option. The option data
    immediately follows this structure.

Members:

    Type - Stores the NDP option type.

    Length - Stores the length of the option, including the option header, in
        8-byte units.

--*/

typedef struct _NDP_OPTION {
    UCHAR Type;
    UCHAR Length;
} PACKED NDP_OPTION, *PNDP_OPTION;

/*++

Structure Description:

    This structure defines the NDP prefix information option. This should
    appear in router advertisement messages and be ignored for other messages.

Members:

    Header - Stores the NDP option header.

    PrefixLength - Stores the number of leading bits in the prefix that are
        valid. Values can range from 0 to 128.

    Flags - Stores a bitmask of prefix information flags. See NDP_PREFIX_FLAG_*
        for definitions.

    ValidLifetime - Stores the length of time, in seconds, for which the prefix
        is valid for on-link determination. If all bits are set, then there
        is no timeout on the validity.

    PreferredLifetime - Stores the length fo time, in seconds, for which
        addresses generated using the prefix via stateless address
        autoconfiguration (SLAAC) remain preferred.

    Reserved - This field is reserved.

    Prefix - Stores an IPv6 address or the prefix of an IPv6 address, the
        length of which is specified by the prefix length field.

--*/

typedef struct _NDP_OPTION_PREFIX_INFORMATION {
    NDP_OPTION Header;
    UCHAR PrefixLength;
    UCHAR Flags;
    ULONG ValidLifetime;
    ULONG PreferredLifetime;
    ULONG Reserved;
    ULONG Prefix[IP6_ADDRESS_SIZE / sizeof(ULONG)];
} PACKED NDP_OPTION_PREFIX_INFORMATION, *PNDP_OPTION_PREFIX_INFORMATION;

/*++

Structure Description:

    This structure defines the redirect header option. The IP header and data
    immediately follow this structure.

Members:

    Header - Stores the NDP option header.

    Reserved1 - This field is reserved.

    Reserved2 - This field is reserved.

--*/

typedef struct _NDP_OPTION_REDIRECT_HEADER {
    NDP_OPTION Header;
    USHORT Reserved1;
    ULONG Reserved2;
} PACKED NDP_OPTION_REDIRECT_HEADER, *PNDP_OPTION_REDIRECT_HEADER;

/*++

Structure Description:

    This structure defines the NDP maximum transmission unit option.

Members:

    Header - Stores the NDP option header.

    Reserved - This field is reserved.

    MaxTransmissionUnit - Stores the maximum transmission unit for the network,
        in bytes.

--*/

typedef struct _NDP_OPTION_MTU {
    NDP_OPTION Header;
    USHORT Reserved;
    ULONG MaxTransmissionUnit;
} PACKED NDP_OPTION_MTU, *PNDP_OPTION_MTU;

#pragma pack(pop)

/*++

Structure Description:

    This structure defines the NDP thread context used for address
    configuration.

Members:

    NdpLink - Stores a pointer to the NDP link to work on.

    LinkAddress - Stores a pointer to the link address entry to configure an
        address for.

--*/

typedef struct _NDP_CONTEXT {
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
} NDP_CONTEXT, *PNDP_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
NetpNdpAutoconfigurationThread (
    PVOID Parameter
    );

KSTATUS
NetpNdpDuplicateAddressDetection (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS Target
    );

VOID
NetpNdpProcessRouterAdvertisement (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

VOID
NetpNdpProcessNeighborSolicitation (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

VOID
NetpNdpProcessNeighborAdvertisement (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpNdpSendNeighborAdvertisement (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS Destination,
    PNETWORK_ADDRESS DestinationPhysical,
    BOOL Solicited
    );

KSTATUS
NetpNdpSendNeighborSolicitation (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS Source,
    PNETWORK_ADDRESS Destination,
    PNETWORK_ADDRESS DestinationPhysical,
    PNETWORK_ADDRESS Target
    );

VOID
NetpNdpSendPackets (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS Source,
    PNETWORK_ADDRESS Destination,
    PNETWORK_ADDRESS DestinationPhysical,
    PNET_PACKET_LIST PacketList,
    UCHAR Type
    );

PNDP_CONTEXT
NetpNdpCreateContext (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

VOID
NetpNdpDestroyContext (
    PNDP_CONTEXT Context
    );

VOID
NetpNdpGetSolicitedNodeMulticastAddress (
    PNETWORK_ADDRESS Address,
    PNETWORK_ADDRESS MulticastAddress
    );

VOID
NetpNdpRandomDelay (
    ULONG DelayMax
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL NetNdpDebug = FALSE;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpNdpInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for NDP.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (NetNdpDebug == FALSE) {
        NetNdpDebug = NetGetGlobalDebugFlag();
    }

    return;
}

VOID
NetpNdpProcessReceivedData (
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
    PNET_PACKET_BUFFER Packet;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // All NDP messages must have the max hop limit set (255), indicating that
    // they came from a link-local node and were not forwarded by a router.
    //

    Packet = ReceiveContext->Packet;
    if ((Packet->Flags & NET_PACKET_FLAG_MAX_HOP_LIMIT) == 0) {
        return;
    }

    //
    // Act based on the ICMPv6 message type. The ICMPv6 module already
    // validated the ICMPv6 header and its checksum.
    //

    Header = (PICMP6_HEADER)(Packet->Buffer + Packet->DataOffset);
    switch (Header->Type) {

    //
    // Minoca does not currently run in router mode.
    //

    case ICMP6_MESSAGE_TYPE_NDP_ROUTER_SOLICITATION:
        break;

    case ICMP6_MESSAGE_TYPE_NDP_ROUTER_ADVERTISEMENT:
        NetpNdpProcessRouterAdvertisement(ReceiveContext);
        break;

    case ICMP6_MESSAGE_TYPE_NDP_NEIGHBOR_SOLICITATION:
        NetpNdpProcessNeighborSolicitation(ReceiveContext);
        break;

    case ICMP6_MESSAGE_TYPE_NDP_NEIGHBOR_ADVERTISEMENT:
        NetpNdpProcessNeighborAdvertisement(ReceiveContext);
        break;

    case ICMP6_MESSAGE_TYPE_NDP_REDIRECT:
        break;

    default:
        break;
    }

    return;
}

KSTATUS
NetpNdpConfigureAddress (
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

    PIP6_ADDRESS Ip6Address;
    NETWORK_ADDRESS MulticastAddress;
    PNDP_CONTEXT NdpContext;
    KSTATUS Status;

    NdpContext = NULL;
    Ip6Address = (PIP6_ADDRESS)&(LinkAddress->Address);

    //
    // The system should not be trying to configure a multicast address.
    //

    if (IP6_IS_MULTICAST_ADDRESS(Ip6Address->Address) != FALSE) {
        Status = STATUS_INVALID_PARAMETER;
        goto NdpConfigureAddressEnd;
    }

    //
    // Address configuration requires sending a receiving a few messages, do it
    // asynchronously by kicking off a thread.
    //

    if (Configure != FALSE) {
        NdpContext = NetpNdpCreateContext(Link, LinkAddress);
        if (NdpContext == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto NdpConfigureAddressEnd;
        }

        Status = PsCreateKernelThread(NetpNdpAutoconfigurationThread,
                                      NdpContext,
                                      "NdpAutoConfigThread");

        if (!KSUCCESS(Status)) {
            NetpNdpDestroyContext(NdpContext);
            goto NdpConfigureAddressEnd;
        }

    //
    // Tear down does not require another thread. It is not as complex.
    //

    } else {
        if (LinkAddress->State < NetLinkAddressConfigured) {
            Status = STATUS_INVALID_PARAMETER;
            goto NdpConfigureAddressEnd;
        }

        //
        // Leave the solicted-node multicast group that was joined when the
        // address was configured.
        //

        NetpNdpGetSolicitedNodeMulticastAddress(&(LinkAddress->Address),
                                                &MulticastAddress);

        Status = NetLeaveLinkMulticastGroup(Link,
                                            LinkAddress,
                                            &MulticastAddress);

        if (!KSUCCESS(Status)) {
            goto NdpConfigureAddressEnd;
        }
    }

NdpConfigureAddressEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpNdpAutoconfigurationThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine attempts to autoconfigure an address for a link. It will use a
    mix of duplicate address detection, router solicitation, and may possibly
    kick off DHCPv6 in order to determine an address.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread, in
        this case a pointer to the NDP context structure.

Return Value:

    None.

--*/

{

    NETWORK_DEVICE_INFORMATION Information;
    PIP6_ADDRESS Ip6Target;
    PNDP_CONTEXT NdpContext;
    KSTATUS Status;
    NETWORK_ADDRESS Target;

    NdpContext = (PNDP_CONTEXT)Parameter;

    //
    // The link address entry stores the target address to configure. Make a
    // copy while holding the links lock in order to get a consistent read.
    // It should not be configured at the moment.
    //

    KeAcquireQueuedLock(NdpContext->Link->QueuedLock);
    RtlCopyMemory(&Target,
                  &(NdpContext->LinkAddress->Address),
                  sizeof(NETWORK_ADDRESS));

    KeReleaseQueuedLock(NdpContext->Link->QueuedLock);

    //
    // If the given address is a link-local address, duplicate address
    // detection needs to be performed.
    //

    Ip6Target = (PIP6_ADDRESS)&Target;
    if (IP6_IS_UNICAST_LINK_LOCAL_ADDRESS(Ip6Target->Address) != FALSE) {
        Status = NetpNdpDuplicateAddressDetection(NdpContext->Link,
                                                  NdpContext->LinkAddress,
                                                  &Target);

        if (!KSUCCESS(Status)) {
            goto NdpAutoconfigurationThreadEnd;
        }

    //
    // Otherwise, a global scope address should be determined through router
    // discovery and possibly DHCPv6.
    //
    // TODO: Handle global scope address assignment.
    //

    } else {
        Status = STATUS_NOT_IMPLEMENTED;
        goto NdpAutoconfigurationThreadEnd;
    }

    //
    // The address was configured! Tell net core that it's ready to go.
    //

    RtlZeroMemory(&Information, sizeof(NETWORK_DEVICE_INFORMATION));
    Information.Version = NETWORK_DEVICE_INFORMATION_VERSION;
    Information.Flags = NETWORK_DEVICE_FLAG_CONFIGURED;
    Information.Domain = NetDomainIp6;
    Information.ConfigurationMethod = NetworkAddressConfigurationStateless;
    RtlCopyMemory(&(Information.Address), &Target, sizeof(NETWORK_ADDRESS));
    Status = NetGetSetNetworkDeviceInformation(NdpContext->Link,
                                               NdpContext->LinkAddress,
                                               &Information,
                                               TRUE);

    if (!KSUCCESS(Status)) {
        goto NdpAutoconfigurationThreadEnd;
    }

    RtlDebugPrint("NDP Autoconfiguration:\n\t");
    if (IP6_IS_UNICAST_LINK_LOCAL_ADDRESS(Ip6Target->Address) != FALSE) {
        NetDebugPrintAddress(&Target);
        RtlDebugPrint("\n");
    }

NdpAutoconfigurationThreadEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Net: NDP autoconfiguration failed: %d\n", Status);
    }

    NetpNdpDestroyContext(NdpContext);
    return;
}

KSTATUS
NetpNdpDuplicateAddressDetection (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS Target
    )

/*++

Routine Description:

    This routine performs duplicate address detection. As it must wait for
    messages to be received, do not call it in a critical code path.

Arguments:

    Link - Supplies a pointer to the link over which the address detection will
        be conducted.

    LinkAddress - Supplies a pointer to the link address that owns the address
        that needs to be checked.

    Target - Supplies a pointer to the network address that needs to be checked
        for duplication over the local network.

Return Value:

    Status code.

--*/

{

    BOOL MulticastJoined;
    NET_LINK_ADDRESS_STATE OldState;
    NETWORK_ADDRESS SolicitedNodeAddress;
    NETWORK_ADDRESS SolicitedNodePhysical;
    KSTATUS Status;
    ULONG TransmitCount;
    BOOL TransmitSolicitation;
    NETWORK_ADDRESS UnspecifiedAddress;

    MulticastJoined = FALSE;

    //
    // Duplicate address detection requires the link to join the all-nodes
    // multicast group and the solicited-node multicast group for the address
    // in question. The all-nodes group is joined during link initialization
    // and ensures that the node receives advertisements from a different node
    // already using the address. The solicited-node group ensures that this
    // node detects another node running duplicate address detection for the
    // address.
    //
    // Join the solicited-node multicast group for the target address after a
    // random delay.
    //

    NetpNdpGetSolicitedNodeMulticastAddress(Target, &SolicitedNodeAddress);
    NetpNdpRandomDelay(NDP_SOLICITATION_DELAY_MAX);
    Status = NetJoinLinkMulticastGroup(Link,
                                       LinkAddress,
                                       &SolicitedNodeAddress);

    if (!KSUCCESS(Status)) {
        goto NdpDuplicateAddressDetectionEnd;
    }

    MulticastJoined = TRUE;

    //
    // The source for all duplicate address detection messages is the
    // unspecified address and the destination is the solicited-node multicast
    // address.
    //

    RtlZeroMemory(&UnspecifiedAddress, sizeof(NETWORK_ADDRESS));
    UnspecifiedAddress.Domain = NetDomainIp6;
    Status = Link->DataLinkEntry->Interface.ConvertToPhysicalAddress(
                                                        &SolicitedNodeAddress,
                                                        &SolicitedNodePhysical,
                                                        NetAddressMulticast);

    if (!KSUCCESS(Status)) {
        goto NdpDuplicateAddressDetectionEnd;
    }

    //
    // Set the link address entry to tentative.
    //

    OldState = RtlAtomicCompareExchange32(&(LinkAddress->State),
                                          NetLinkAddressTentative,
                                          NetLinkAddressNotConfigured);

    //
    // If the state did not get set to tentative, abort the duplicate address
    // detection as another thread is working with this cache entry.
    //

    if (OldState != NetLinkAddressNotConfigured) {
        Status = STATUS_TOO_LATE;
        goto NdpDuplicateAddressDetectionEnd;
    }

    //
    // Send neighbor solicitations until a response is received, as indicated
    // by a neighbor cache entry, or until the retransmit count and timer run
    // out, at which point the address is considered unique.
    //

    TransmitSolicitation = TRUE;
    TransmitCount = NDP_DEFAULT_DUPLICATE_ADDRESS_DETECTION_TRANSMIT_COUNT;
    while (TRUE) {

        //
        // If the target address's entry got marked as duplicate, than this
        // interface cannot be used. Another system on the link has the same
        // link-layer address.
        //

        if (LinkAddress->State == NetLinkAddressDuplicate) {
            Status = STATUS_DUPLICATE_ENTRY;
            RtlDebugPrint("NDP: Duplicate Address Detected: ");
            NetDebugPrintAddress(Target);
            RtlDebugPrint("\n");
            goto NdpDuplicateAddressDetectionEnd;
        }

        if (TransmitSolicitation != FALSE) {

            //
            // If the transmit count is zero, it means that the system has
            // waited a full retransmit timeout, after the required number of
            // solicitations have been sent, without a response. The address
            // is unique.
            //

            if (TransmitCount == 0) {
                Status = STATUS_SUCCESS;
                break;
            }

            Status = NetpNdpSendNeighborSolicitation(Link,
                                                     LinkAddress,
                                                     &UnspecifiedAddress,
                                                     &SolicitedNodeAddress,
                                                     &SolicitedNodePhysical,
                                                     Target);

            if (!KSUCCESS(Status)) {
                goto NdpDuplicateAddressDetectionEnd;
            }

            TransmitCount -= 1;
            TransmitSolicitation = FALSE;
        }

        //
        // Wait for a neighbor advertisement to arrive.
        //

        Status = KeWaitForEvent(Link->AddressTranslationEvent,
                                FALSE,
                                NDP_DEFAULT_RETRANMIT_TIMEOUT);

        if (Status == STATUS_TIMEOUT) {
            TransmitSolicitation = TRUE;

        } else if (!KSUCCESS(Status)) {
            goto NdpDuplicateAddressDetectionEnd;
        }
    }

NdpDuplicateAddressDetectionEnd:
    if (!KSUCCESS(Status)) {
        if (MulticastJoined != FALSE) {
            NetLeaveLinkMulticastGroup(Link,
                                       LinkAddress,
                                       &SolicitedNodeAddress);
        }
    }

    return Status;
}

VOID
NetpNdpProcessRouterAdvertisement (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine handles router advertisement NDP messages.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    None.

--*/

{

    //
    // TODO: Handle router advertisement messages.
    //

    return;
}

VOID
NetpNdpProcessNeighborSolicitation (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine handles neighbor solicitation NDP messages.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    None.

--*/

{

    PIP6_ADDRESS Ip6Destination;
    PIP6_ADDRESS Ip6Source;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    NET_LINK_ADDRESS_STATE OldState;
    PNDP_OPTION OptionHeader;
    ULONG OptionSize;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketSize;
    PNDP_NEIGHBOR_SOLICITATION Solicitation;
    PNETWORK_ADDRESS SourcePhysical;
    NETWORK_ADDRESS SourcePhysicalBuffer;
    KSTATUS Status;
    NETWORK_ADDRESS Target;

    Ip6Source = (PIP6_ADDRESS)ReceiveContext->Source;
    Ip6Destination = (PIP6_ADDRESS)ReceiveContext->Destination;
    Link = ReceiveContext->Link;
    Packet = ReceiveContext->Packet;
    PacketSize = Packet->FooterOffset - Packet->DataOffset;
    if (PacketSize < sizeof(NDP_NEIGHBOR_SOLICITATION)) {
        goto NdpProcessNeighborSolicitationEnd;
    }

    //
    // Get the target IP address out of the message. Drop the packet if it is
    // a multicast address.
    //

    Solicitation = (PNDP_NEIGHBOR_SOLICITATION)(Packet->Buffer +
                                                Packet->DataOffset);

    if (IP6_IS_MULTICAST_ADDRESS(Solicitation->TargetAddress) != FALSE) {
        goto NdpProcessNeighborSolicitationEnd;
    }

    Target.Domain = NetDomainIp6;
    Target.Port = 0;
    RtlCopyMemory(Target.Address,
                  Solicitation->TargetAddress,
                  IP6_ADDRESS_SIZE);

    //
    // If supplied, get the source's link-layer address out of the message
    // options.
    //

    SourcePhysical = NULL;
    PacketSize -= sizeof(NDP_NEIGHBOR_SOLICITATION);
    OptionHeader = (PNDP_OPTION)(Solicitation + 1);
    while (PacketSize != 0) {
        OptionSize = OptionHeader->Length * NDP_OPTION_LENGTH_MULTIPLE;
        if ((OptionSize == 0) || (OptionSize > PacketSize)) {
            goto NdpProcessNeighborSolicitationEnd;
        }

        if (OptionHeader->Type == NDP_OPTION_TYPE_SOURCE_LINK_ADDRESS) {
            if ((OptionSize - sizeof(NDP_OPTION)) != ETHERNET_ADDRESS_SIZE) {
                goto NdpProcessNeighborSolicitationEnd;
            }

            SourcePhysical = &SourcePhysicalBuffer;
            SourcePhysical->Domain = Link->Properties.DataLinkType;
            SourcePhysical->Port = 0;
            RtlCopyMemory(SourcePhysical->Address,
                          (PVOID)(OptionHeader + 1),
                          ETHERNET_ADDRESS_SIZE);
        }

        PacketSize -= OptionSize;
    }

    //
    // If the source is unspecified, then there must not be a source physical
    // address specified and the destination should have been a solicited-node
    // multicast address.
    //

    if (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Source->Address) != FALSE) {
        if (IP6_IS_SOLICITED_NODE_MULTICAST_ADDRESS(Ip6Destination->Address) ==
            FALSE) {

            goto NdpProcessNeighborSolicitationEnd;
        }

        if (SourcePhysical != NULL) {
            goto NdpProcessNeighborSolicitationEnd;
        }
    }

    if (NetNdpDebug != FALSE) {
        RtlDebugPrint("NDP RX: Who has ");
        NetDebugPrintAddress(&Target);
        RtlDebugPrint("? Tell ");
        NetDebugPrintAddress(ReceiveContext->Source);
        if (SourcePhysical != NULL) {
            RtlDebugPrint(" (");
            NetDebugPrintAddress(SourcePhysical);
            RtlDebugPrint(")\n");

        } else {
            RtlDebugPrint("\n");
        }
    }

    Status = NetFindEntryForAddress(Link,
                                    &Target,
                                    &LinkAddress);

    if (!KSUCCESS(Status)) {
        goto NdpProcessNeighborSolicitationEnd;
    }

    //
    // If the link address entry is not configured, then it is likely a
    // "tentative" target. Special processing applies.
    //

    if (LinkAddress->State < NetLinkAddressConfigured) {

        //
        // If the source is unspecified and the address is really tentative,
        // then another node is also performing address duplication detection.
        // Do not use the tentative address. As the NDP multicast packets do
        // not get looped back, this does not need to check if this node sent
        // the solicitation.
        //

        if (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Source->Address) != FALSE) {
            OldState = RtlAtomicCompareExchange32(&(LinkAddress->State),
                                                  NetLinkAddressDuplicate,
                                                  NetLinkAddressTentative);

            if (OldState == NetLinkAddressTentative) {
                KeSignalEvent(Link->AddressTranslationEvent, SignalOptionPulse);
            }
        }

        return;
    }

    //
    // If the solicitation supplied a link-layer address and the network
    // address is valid, then save it.
    //

    if (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Source->Address) == FALSE) {

        //
        // NDP does not require unicast neighbor solicitations to include the
        // source link-layer address. The work around is to get the source
        // physical address from the data link layer. Drop the packets for now.
        //
        // TODO: Get source physical address from data link layer.
        //

        if (SourcePhysical == NULL) {
            goto NdpProcessNeighborSolicitationEnd;
        }

        //
        // TODO: Add solicitation to neighbor cache.
        //

    }

    //
    // Respond with a solicited advertisement.
    //

    NetpNdpSendNeighborAdvertisement(Link,
                                     LinkAddress,
                                     (PNETWORK_ADDRESS)Ip6Source,
                                     SourcePhysical,
                                     TRUE);

NdpProcessNeighborSolicitationEnd:
    return;
}

VOID
NetpNdpProcessNeighborAdvertisement (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine handles neighbor advertisement NDP messages.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    None.

--*/

{

    PNDP_NEIGHBOR_ADVERTISEMENT Advertisement;
    PIP6_ADDRESS Ip6Destination;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    NET_LINK_ADDRESS_STATE OldState;
    PNDP_OPTION OptionHeader;
    ULONG OptionSize;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketSize;
    KSTATUS Status;
    NETWORK_ADDRESS Target;
    PNETWORK_ADDRESS TargetPhysical;
    NETWORK_ADDRESS TargetPhysicalBuffer;

    Ip6Destination = (PIP6_ADDRESS)ReceiveContext->Destination;
    Link = ReceiveContext->Link;
    Packet = ReceiveContext->Packet;
    PacketSize = Packet->FooterOffset - Packet->DataOffset;
    if (PacketSize < sizeof(NDP_NEIGHBOR_ADVERTISEMENT)) {
        goto NdpProcessNeighborAdvertisementEnd;
    }

    //
    // Get the target IP address out of the message. Drop the packet if it is a
    // multicast address.
    //

    Advertisement = (PNDP_NEIGHBOR_ADVERTISEMENT)(Packet->Buffer +
                                                  Packet->DataOffset);

    if (IP6_IS_MULTICAST_ADDRESS(Advertisement->TargetAddress) != FALSE) {
        goto NdpProcessNeighborAdvertisementEnd;
    }

    Target.Domain = NetDomainIp6;
    Target.Port = 0;
    RtlCopyMemory(Target.Address,
                  Advertisement->TargetAddress,
                  IP6_ADDRESS_SIZE);

    //
    // If the destination is a multicast address, the solicited flag better be
    // zero.
    //

    if ((IP6_IS_MULTICAST_ADDRESS(Ip6Destination->Address) != FALSE) &&
        ((Advertisement->Flags & NDP_NEIGHBOR_FLAG_SOLICITED) != 0)) {

        goto NdpProcessNeighborAdvertisementEnd;
    }

    //
    // If supplied, get the target's link-layer address out of the message
    // options.
    //

    TargetPhysical = NULL;
    PacketSize -= sizeof(NDP_NEIGHBOR_SOLICITATION);
    OptionHeader = (PNDP_OPTION)(Advertisement + 1);
    while (PacketSize != 0) {
        OptionSize = OptionHeader->Length * NDP_OPTION_LENGTH_MULTIPLE;
        if ((OptionSize == 0) || (OptionSize > PacketSize)) {
            goto NdpProcessNeighborAdvertisementEnd;
        }

        if (OptionHeader->Type == NDP_OPTION_TYPE_TARGET_LINK_ADDRESS) {
            if ((OptionSize - sizeof(NDP_OPTION)) != ETHERNET_ADDRESS_SIZE) {
                goto NdpProcessNeighborAdvertisementEnd;
            }

            TargetPhysical = &TargetPhysicalBuffer;
            TargetPhysical->Domain = Link->Properties.DataLinkType;
            TargetPhysical->Port = 0;
            RtlCopyMemory(TargetPhysical->Address,
                          (PVOID)(OptionHeader + 1),
                          ETHERNET_ADDRESS_SIZE);
        }

        PacketSize -= OptionSize;
    }

    //
    // Test to see if the target address matches any of this nodes addresses.
    // If it does, then the address is not unique. Mark it as duplicate. No
    // new connections will use it.
    //

    Status = NetFindEntryForAddress(Link, &Target, &LinkAddress);
    if (KSUCCESS(Status)) {
        OldState = RtlAtomicExchange(&(LinkAddress->State),
                                     NetLinkAddressDuplicate);

        if (OldState == NetLinkAddressTentative) {
            KeSignalEvent(Link->AddressTranslationEvent, SignalOptionPulse);
        }

        goto NdpProcessNeighborAdvertisementEnd;
    }

    //
    // TODO: Process neighbor advertisements beyond duplicate address detection.
    //

NdpProcessNeighborAdvertisementEnd:
    return;
}

KSTATUS
NetpNdpSendNeighborAdvertisement (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS Destination,
    PNETWORK_ADDRESS DestinationPhysical,
    BOOL Solicited
    )

/*++

Routine Description:

    This routine allocates, assembles, and sends an NDP advertisement to
    communicate the physical address of one of the network addresses owned by
    this machine. This routine returns as soon as the NDP request is
    successfully queued for transmission.

Arguments:

    Link - Supplies a pointer to the link to send the advertisement down.

    LinkAddress - Supplies a pointer to the source address of the advertisement.

    Destination - Supplies a pointer to the network address to
        send the response to.

    DestinationPhysical - Supplies a pointer to the physical address to
        send the response to.

    Solicited - Supplies a boolean indicating whether ot not the advertisement
        was prompted by a solicitation.

Return Value:

    STATUS_SUCCESS if the request was successfully sent off.

    STATUS_INSUFFICIENT_RESOURCES if the transmission buffer couldn't be
    allocated.

    Other errors on other failures.

--*/

{

    PNDP_NEIGHBOR_ADVERTISEMENT Advertisement;
    NETWORK_ADDRESS AllNodesAddress;
    NETWORK_ADDRESS AllNodesPhysicalAddress;
    ULONG BufferFlags;
    PIP6_ADDRESS Ip6Destination;
    BOOL LockHeld;
    PNDP_OPTION OptionHeader;
    ULONG OptionSize;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    ULONG PacketSize;
    NETWORK_ADDRESS Source;
    KSTATUS Status;

    NET_INITIALIZE_PACKET_LIST(&PacketList);
    LockHeld = FALSE;

    //
    // Determine the size of the packet. If this is not a duplicate address
    // detection, then a source link-layer address option is added.
    //

    PacketSize = sizeof(NDP_NEIGHBOR_SOLICITATION);
    OptionSize = sizeof(NDP_OPTION) + ETHERNET_ADDRESS_SIZE;
    PacketSize += OptionSize;
    BufferFlags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Status = NetAllocateBuffer(sizeof(IP6_HEADER),
                               PacketSize,
                               0,
                               Link,
                               BufferFlags,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto NdpSendNeighborAdvertisementEnd;
    }

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);

    //
    // Initialize the ICMPv6 NDP neighbor advertisement message. For solicited
    // advertisements, the target address is that received during solicitation.
    // For unsolicited advertisements, the target address is that for the link
    // whose link-layer address has changed.
    //

    Advertisement = Packet->Buffer + Packet->DataOffset;
    Advertisement->Flags = 0;
    if ((Solicited != FALSE) &&
        (IP6_IS_MULTICAST_ADDRESS(Destination->Address) == FALSE)) {

        Advertisement->Flags |= NDP_NEIGHBOR_FLAG_SOLICITED;
    }

    //
    // The override flag should be set unless solicited by an anycast address.
    //

    Advertisement->Flags |= NDP_NEIGHBOR_FLAG_OVERRIDE;

    //
    // Acquire the link lock to get a consistent read of the link address entry.
    //

    KeAcquireQueuedLock(Link->QueuedLock);
    LockHeld = TRUE;
    if (LinkAddress->State < NetLinkAddressConfigured) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto NdpSendNeighborAdvertisementEnd;
    }

    ASSERT(LinkAddress->Address.Domain == NetDomainIp6);

    RtlCopyMemory(&Source, &(LinkAddress->Address), sizeof(NETWORK_ADDRESS));
    KeReleaseQueuedLock(Link->QueuedLock);
    LockHeld = FALSE;
    RtlCopyMemory(Advertisement->TargetAddress,
                  Source.Address,
                  IP6_ADDRESS_SIZE);

    //
    // Add the NDP target link-layer address option.
    //

    OptionHeader = (PNDP_OPTION)(Advertisement + 1);
    OptionHeader->Type = NDP_OPTION_TYPE_TARGET_LINK_ADDRESS;
    OptionHeader->Length = OptionSize / NDP_OPTION_LENGTH_MULTIPLE;
    RtlCopyMemory((PVOID)(OptionHeader + 1),
                  &(LinkAddress->PhysicalAddress.Address),
                  ETHERNET_ADDRESS_SIZE);

    //
    // Craft up a solicited-node multicast address based on the given query
    // address.
    //

    Ip6Destination = (PIP6_ADDRESS)Destination;
    if ((Solicited == FALSE) ||
        (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Destination->Address) != FALSE)) {

        RtlZeroMemory(&AllNodesAddress, sizeof(NETWORK_ADDRESS));
        AllNodesAddress.Domain = NetDomainIp6;
        RtlCopyMemory(AllNodesAddress.Address,
                      NetIp6AllNodesMulticastAddress,
                      IP6_ADDRESS_SIZE);

        Status = Link->DataLinkEntry->Interface.ConvertToPhysicalAddress(
                                                      &AllNodesAddress,
                                                      &AllNodesPhysicalAddress,
                                                      NetAddressMulticast);

        if (!KSUCCESS(Status)) {
            goto NdpSendNeighborAdvertisementEnd;
        }

        Destination = &AllNodesAddress;
        DestinationPhysical = &AllNodesPhysicalAddress;
    }

    //
    // Send the neighbor advertisement message down to ICMPv6.
    //

    NetpNdpSendPackets(Link,
                       LinkAddress,
                       &Source,
                       Destination,
                       DestinationPhysical,
                       &PacketList,
                       ICMP6_MESSAGE_TYPE_NDP_NEIGHBOR_ADVERTISEMENT);

NdpSendNeighborAdvertisementEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Link->QueuedLock);
    }

    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

KSTATUS
NetpNdpSendNeighborSolicitation (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS Source,
    PNETWORK_ADDRESS Destination,
    PNETWORK_ADDRESS DestinationPhysical,
    PNETWORK_ADDRESS Target
    )

/*++

Routine Description:

    This routine allocates, assembles, and sends an NDP request to translate
    the given network address into a physical address. This routine returns
    as soon as the NDP request is successfully queued for transmission.

Arguments:

    Link - Supplies a pointer to the link to send the request down.

    LinkAddress - Supplies a pointer to the link address associated with the
        solicitation. The link address may be in the middle of configuration,
        so its address is not used.

    Source - Supplies a pointer to the source address of the request.

    SourcePhysical - Supplies a pointer the source's physical address.

    Destination - Supplies a pointer to the destination address of the message.

    DestinationPhysical - Supplies a pointer the destinations's physical
        address.

    Target - Supplies a pointer to the network address to ask about.

Return Value:

    Status code.

--*/

{

    ULONG BufferFlags;
    BOOL DuplicateDetection;
    PIP6_ADDRESS Ip6Source;
    PIP6_ADDRESS Ip6Target;
    PNDP_OPTION OptionHeader;
    ULONG OptionSize;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    ULONG PacketSize;
    PNDP_NEIGHBOR_SOLICITATION Solicitation;
    KSTATUS Status;

    ASSERT(Target->Domain == NetDomainIp6);

    NET_INITIALIZE_PACKET_LIST(&PacketList);
    Ip6Target = (PIP6_ADDRESS)Target;
    if (IP6_IS_MULTICAST_ADDRESS(Ip6Target->Address) != FALSE) {
        Status = STATUS_INVALID_PARAMETER;
        goto NdpSendNeighborSolicitationEnd;
    }

    Ip6Source = (PIP6_ADDRESS)Source;
    DuplicateDetection = FALSE;
    if (IP6_IS_UNSPECIFIED_ADDRESS(Ip6Source->Address) != FALSE) {
        DuplicateDetection = TRUE;
    }

    //
    // Determine the size of the packet. If this is not a duplicate address
    // detection, then a source link-layer address option is added.
    //

    PacketSize = sizeof(NDP_NEIGHBOR_SOLICITATION);
    if (DuplicateDetection == FALSE) {
        OptionSize = sizeof(NDP_OPTION) + ETHERNET_ADDRESS_SIZE;
        PacketSize += OptionSize;
    }

    BufferFlags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
                  NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Status = NetAllocateBuffer(sizeof(IP6_HEADER),
                               PacketSize,
                               0,
                               Link,
                               BufferFlags,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto NdpSendNeighborSolicitationEnd;
    }

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);

    //
    // Initialize the ICMPv6 NDP neighbor solicitation message.
    //

    Solicitation = (PNDP_NEIGHBOR_SOLICITATION)(Packet->Buffer +
                                                Packet->DataOffset);

    Solicitation->Reserved = 0;
    RtlCopyMemory(&(Solicitation->TargetAddress),
                  &(Ip6Target->Address),
                  IP6_ADDRESS_SIZE);

    //
    // Add the NDP source link-layer address option if this is not for
    // duplicate address detection.
    //

    if (DuplicateDetection == FALSE) {
        OptionHeader = (PNDP_OPTION)(Solicitation + 1);
        OptionHeader->Type = NDP_OPTION_TYPE_SOURCE_LINK_ADDRESS;
        OptionHeader->Length = OptionSize / NDP_OPTION_LENGTH_MULTIPLE;
        RtlCopyMemory((PVOID)(OptionHeader + 1),
                      LinkAddress->PhysicalAddress.Address,
                      ETHERNET_ADDRESS_SIZE);
    }

    NetpNdpSendPackets(Link,
                       LinkAddress,
                       Source,
                       Destination,
                       DestinationPhysical,
                       &PacketList,
                       ICMP6_MESSAGE_TYPE_NDP_NEIGHBOR_SOLICITATION);

NdpSendNeighborSolicitationEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

VOID
NetpNdpSendPackets (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS Source,
    PNETWORK_ADDRESS Destination,
    PNETWORK_ADDRESS DestinationPhysical,
    PNET_PACKET_LIST PacketList,
    UCHAR Type
    )

/*++

Routine Description:

    This routine sends a list of NDP packets out over the provided link to the
    specified destination. It adds the ICMPv6 and IPv6 headers and sends the
    packets down the stack.

Arguments:

    Link - Supplies a pointer to the link over which to send the packet.

    LinkAddress - Supplies a pointer to the link address for which the packet
        is being sent.

    Source - Supplies a pointer to the IPv6 source address.

    Destination - Supplies a pointer to the IPv6 destination address.

    DestinationPhysical - Supplies a pointer to the physical address of the
        destination.

    PacketList - Supplies a pointer to the list of packets to send.

    Type - Supplies the ICMPv6 message type for the packets.

Return Value:

    None.

--*/

{

    USHORT Checksum;
    PLIST_ENTRY CurrentEntry;
    PICMP6_HEADER Icmp6Header;
    ULONG Icmp6Length;
    PIP6_HEADER Ip6Header;
    PNET_PACKET_BUFFER Packet;
    ULONG PayloadLength;
    PNET_DATA_LINK_SEND Send;
    KSTATUS Status;
    ULONG VersionClassFlow;

    //
    // For each packet in the list, add an ICMPv6 and IPv6 header.
    //

    CurrentEntry = PacketList->Head.Next;
    while (CurrentEntry != &(PacketList->Head)) {
        Packet = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Initialize the ICMPv6 header. The data offset should already be
        // set to the ICMPv6 header as all NDP messages include an ICMPv6
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
        // Now add the IPv6 header.
        //

        PayloadLength = Packet->FooterOffset - Packet->DataOffset;
        if (PayloadLength > IP6_MAX_PAYLOAD_LENGTH) {
            Status = STATUS_MESSAGE_TOO_LONG;
            goto NdpSendPacketsEnd;
        }

        ASSERT(Packet->DataOffset >= sizeof(IP6_HEADER));

        Packet->DataOffset -= sizeof(IP6_HEADER);
        Ip6Header = (PIP6_HEADER)(Packet->Buffer + Packet->DataOffset);
        VersionClassFlow = (IP6_VERSION << IP6_VERSION_SHIFT) &
                           IP6_VERSION_MASK;

        Ip6Header->VersionClassFlow = CPU_TO_NETWORK32(VersionClassFlow);
        Ip6Header->PayloadLength = CPU_TO_NETWORK16((USHORT)PayloadLength);
        Ip6Header->NextHeader = SOCKET_INTERNET_PROTOCOL_ICMP6;
        Ip6Header->HopLimit = NDP_IP6_HOP_LIMIT;
        RtlCopyMemory(Ip6Header->SourceAddress,
                      Source->Address,
                      IP6_ADDRESS_SIZE);

        RtlCopyMemory(Ip6Header->DestinationAddress,
                      Destination->Address,
                      IP6_ADDRESS_SIZE);
    }

    Send = Link->DataLinkEntry->Interface.Send;
    Status = Send(Link->DataLinkContext,
                  PacketList,
                  &(LinkAddress->PhysicalAddress),
                  DestinationPhysical,
                  IP6_PROTOCOL_NUMBER);

    if (!KSUCCESS(Status)) {
        goto NdpSendPacketsEnd;
    }

NdpSendPacketsEnd:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(PacketList);
    }

    return;
}

PNDP_CONTEXT
NetpNdpCreateContext (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine creates an NDP context.

Arguments:

    Link - Supplies a pointer to the link for the context.

    LinkAddress - Supplies a pointer to the network link address for the NDP
        context.

Return Value:

    Returns a pointer to the newly allocated NDP context on success, or NULL
    on failure.

--*/

{

    PNDP_CONTEXT Context;

    Context = MmAllocatePagedPool(sizeof(NDP_CONTEXT), NDP_ALLOCATION_TAG);
    if (Context == NULL) {
        return NULL;
    }

    RtlZeroMemory(Context, sizeof(NDP_CONTEXT));
    NetLinkAddReference(Link);
    Context->Link = Link;
    Context->LinkAddress = LinkAddress;
    return Context;
}

VOID
NetpNdpDestroyContext (
    PNDP_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the given NDP context.

Arguments:

    Context - Supplies a pointer to the NDP context to destroy.

Return Value:

    None.

--*/

{

    ASSERT(Context->Link != NULL);

    NetLinkReleaseReference(Context->Link);
    MmFreePagedPool(Context);
    return;
}

VOID
NetpNdpGetSolicitedNodeMulticastAddress (
    PNETWORK_ADDRESS Address,
    PNETWORK_ADDRESS MulticastAddress
    )

/*++

Routine Description:

    This routine creates a solicited-node multicast address for the given IPv6
    address.

Arguments:

    Address - Supplies a pointer to the IPv6 address on which to base the
        solicited-node multicast address.

    MulticastAddress - Supplies a pointer to a network address that is
        initialized with the given address's solicited-node multicast address.

Return Value:

    None.

--*/

{

    PIP6_ADDRESS Ip6Address;
    PIP6_ADDRESS Ip6Multicast;

    Ip6Address = (PIP6_ADDRESS)Address;
    Ip6Multicast = (PIP6_ADDRESS)MulticastAddress;
    RtlZeroMemory(Ip6Multicast, sizeof(IP6_ADDRESS));
    Ip6Multicast->Domain = NetDomainIp6;
    RtlCopyMemory(Ip6Multicast->Address,
                  NetIp6SolicitedNodeMulticastPrefix,
                  IP6_ADDRESS_SIZE);

    Ip6Multicast->Address[3] |= CPU_TO_NETWORK32(0x00FFFFFF) &
                                Ip6Address->Address[3];

    return;
}

VOID
NetpNdpRandomDelay (
    ULONG DelayMax
    )

/*++

Routine Description:

    This routine delays for a random amount of time between 0 and the given
    delay maximum.

Arguments:

    DelayMax - Supplies the maximum delay, in milliseconds.

Return Value:

    None.

--*/

{

    ULONG Delay;
    ULONGLONG DelayInMicroseconds;

    //
    // The random delay is selected from the range (0, MaxResponseTime].
    //

    KeGetRandomBytes(&Delay, sizeof(Delay));
    Delay = (Delay % DelayMax) + 1;
    DelayInMicroseconds = Delay * MICROSECONDS_PER_MILLISECOND;
    KeDelayExecution(FALSE, FALSE, DelayInMicroseconds);
    return;
}

