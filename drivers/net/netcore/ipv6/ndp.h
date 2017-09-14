/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ndp.h

Abstract:

    This header contains definitions for Neighbor Discovery Protocol for IPv6.
    It is a sub-protocol of ICMPv6.

Author:

    Chris Stevens 7-Sept-2017

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

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
NetpNdpInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for NDP.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
NetpNdpProcessReceivedData (
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
NetpNdpConfigureAddress (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    BOOL Configure
    );

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

