/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    netcore.h

Abstract:

    This header contains internal definitions for the core networking library.

Author:

    Evan Green 4-Apr-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Redefine the API define into an export.
//

#define NET_API __DLLEXPORT

#include <minoca/net/netdrv.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used across the networking core library.
//

#define NET_CORE_ALLOCATION_TAG 0x4374654E // 'CteN'

//
// Define the maximum number of incoming but not accepted connections that are
// allowed to accumulate in a socket.
//

#define NET_MAX_INCOMING_CONNECTIONS 512

#define NET_PRINT_ADDRESS_STRING_LENGTH 200

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the global debug flag, which propagates throughout the networking
// subsystem.
//

extern BOOL NetGlobalDebug;

//
// Define the list of supported socket types.
//

extern LIST_ENTRY NetProtocolList;
extern LIST_ENTRY NetNetworkList;
extern LIST_ENTRY NetDataLinkList;
extern PSHARED_EXCLUSIVE_LOCK NetPluginListLock;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
NetpInitializeNetworkLayer (
    VOID
    );

/*++

Routine Description:

    This routine initialize support for generic Network layer functionality.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
NetpInitializeBuffers (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for network buffers.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
NetpDestroyBuffers (
    VOID
    );

/*++

Routine Description:

    This routine destroys any allocations made during network buffer
    initialization.

Arguments:

    None.

Return Value:

    None.

--*/

COMPARISON_RESULT
NetpCompareNetworkAddresses (
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

COMPARISON_RESULT
NetpCompareFullyBoundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

/*++

Routine Description:

    This routine compares two fully bound sockets, where both the local and
    remote addresses are fixed.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

COMPARISON_RESULT
NetpCompareLocallyBoundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

/*++

Routine Description:

    This routine compares two locally bound sockets, where the local address
    and port are fixed.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

COMPARISON_RESULT
NetpCompareUnboundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

/*++

Routine Description:

    This routine compares two unbound sockets, meaning only the local port
    number is known.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

//
// Prototypes to the entry points for built in protocols.
//

VOID
NetpUdpInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for UDP sockets.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
NetpTcpInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for TCP sockets.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
NetpRawInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for raw sockets.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
NetpIgmpInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for the IGMP protocol.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
NetpNetlinkGenericInitialize (
    ULONG Phase
    );

/*++

Routine Description:

    This routine initializes support for UDP sockets.

Arguments:

    Phase - Supplies the phase of the initialization. Phase 0 happens before
        the networking core registers with the kernel, meaning sockets cannot
        be created. Phase 1 happens after the networking core has registered
        with the kernel allowing socket creation.

Return Value:

    None.

--*/

//
// Prototypes to entry points for built in networks.
//

VOID
NetpIp4Initialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for IPv4 packets.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
NetpArpInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for ARP packets.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
NetpNetlinkInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for netlink packets.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Prototypes to entry points for built in data link layers.
//

VOID
NetpEthernetInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for Ethernet frames.

Arguments:

    None.

Return Value:

    None.

--*/

//
// Prototypes to entry points for built in components.
//

VOID
NetpDhcpInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for DHCP.

Arguments:

    None.

Return Value:

    None.

--*/

