/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#define NET_API DLLEXPORT

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

#define NET_MAX_INCOMING_CONNECTIONS 32

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
extern PQUEUED_LOCK NetPluginListLock;

//
// Define the trees that hold all the sockets.
//

extern RED_BLACK_TREE NetSocketTree[SocketBindingTypeCount];
extern PQUEUED_LOCK NetSocketsLock;

//
// Define the list of raw sockets. These do not get put in the socket trees.
//

extern LIST_ENTRY NetRawSocketsList;
extern PQUEUED_LOCK NetRawSocketsLock;

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

//
// Prototype to special built-in raw socket receive routine.
//

VOID
NetpRawSocketProcessReceivedData (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress
    );

/*++

Routine Description:

    This routine is called to process a received packet for a particular raw
    socket.

Arguments:

    Socket - Supplies a pointer to the socket that is meant to receive this
        data.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        The packet is not modified by the routine.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

Return Value:

    None.

--*/

