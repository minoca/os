/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    arp.h

Abstract:

    This header contains definitions for the Address Resolution Protocol,
    which facilitates translation of network addresses to physical ones.

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
NetpArpSendRequest (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS QueryAddress
    );

/*++

Routine Description:

    This routine allocates, assembles, and sends an ARP request to translate
    the given network address into a physical address. This routine returns
    as soon as the ARP request is successfully queued for transmission.

Arguments:

    Link - Supplies a pointer to the link to send the request down.

    LinkAddress - Supplies the source address of the request.

    QueryAddress - Supplies the network address to ask about.

Return Value:

    STATUS_SUCCESS if the request was successfully sent off.

    STATUS_INSUFFICIENT_RESOURCES if the transmission buffer couldn't be
    allocated.

    Other errors on other failures.

--*/

