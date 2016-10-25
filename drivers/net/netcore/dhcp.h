/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dhcp.h

Abstract:

    This header contains public definitions for the Dynamic Host Configuration
    Protocol (DHCP).

Author:

    Evan Green 5-Apr-2013

--*/

//
// ------------------------------------------------------------------- Includes
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

KSTATUS
NetpDhcpBeginAssignment (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

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

KSTATUS
NetpDhcpCancelLease (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

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

