/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mld.h

Abstract:

    This header contains definitions for Multicast Listener Discovery for IPv6.
    It is a sub-protocol of ICMPv6.

Author:

    Chris Stevens 25-Aug-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an MLD request to join or leave a multicast group.

Members:

    Link - Supplies a pointer to the network link associated with the multicast
        group. Requests will send MLD notifications over this link and update
        the address filters in this link's physical layer.

    LinkAddress - Supplies a pointer to the link address entry with which the
        multicast group is associated.

    MulticastAddress - Supplies the IPv6 multicast group address.

--*/

typedef struct _SOCKET_MLD_MULTICAST_REQUEST {
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    UCHAR MulticastAddress[IP6_ADDRESS_SIZE];
} SOCKET_MLD_MULTICAST_REQUEST, *PSOCKET_MLD_MULTICAST_REQUEST;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
NetpMldInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for the MLD protocol.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
NetpMldProcessReceivedData (
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

KSTATUS
NetpMldJoinMulticastGroup (
    PNET_NETWORK_MULTICAST_REQUEST Request
    );

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

KSTATUS
NetpMldLeaveMulticastGroup (
    PNET_NETWORK_MULTICAST_REQUEST Request
    );

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

