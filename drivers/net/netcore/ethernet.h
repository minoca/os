/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    ethernet.h

Abstract:

    This header contains definitions for Ethernet links.

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
// Define the size of an ethernet header and footer.
//

#define ETHERNET_HEADER_SIZE ((2 * ETHERNET_ADDRESS_SIZE) + sizeof(USHORT))
#define ETHERNET_FOOTER_SIZE sizeof(ULONG)

//
// Define the minimum and maximum valid ethernet payload size. This does not
// include the header or footer.
//

#define ETHERNET_MINIMUM_PAYLOAD_SIZE 46
#define ETHERNET_MAXIMUM_PAYLOAD_SIZE 1500

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
NetpEthernetInitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    );

/*++

Routine Description:

    This routine initializes any pieces of information needed by the link
    layer for the socket. The core networking library will fill in the common
    header when this routine returns.

Arguments:

    ProtocolEntry - Supplies a pointer to the protocol information.

    NetworkEntry - Supplies a pointer to the network information.

    NetworkProtocol - Supplies the raw protocol value for this socket used on
        the network. This value is network specific.

    NewSocket - Supplies a pointer to the new socket. The network layer should
        at the very least add any needed header size.

Return Value:

    Status code.

--*/

VOID
NetpEthernetGetBroadcastAddress (
    PNETWORK_ADDRESS PhysicalNetworkAddress
    );

/*++

Routine Description:

    This routine gets the ethernet broadcast address.

Arguments:

    PhysicalNetworkAddress - Supplies a pointer where the physical network
        broadcast address will be returned.

Return Value:

    None.

--*/

VOID
NetpEthernetAddHeader (
    PNET_PACKET_BUFFER SendBuffer,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    USHORT ProtocolNumber
    );

/*++

Routine Description:

    This routine adds headers to an ethernet packet.

Arguments:

    SendBuffer - Supplies a pointer to the sending buffer to add the headers
        to.

    SourcePhysicalAddress - Supplies a pointer to the source (local) physical
        network address.

    DestinationPhysicalAddress - Supplies the physical address of the
        destination, or at least the next hop.

    ProtocolNumber - Supplies the protocol number of the data inside the
        ethernet header.

Return Value:

    None.

--*/

VOID
NetpEthernetProcessReceivedPacket (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

/*++

Routine Description:

    This routine is called to process a received ethernet packet.

Arguments:

    Link - Supplies a pointer to the link that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

ULONG
NetpEthernetPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

/*++

Routine Description:

    This routine is called to convert a network address into a string, or
    determine the length of the buffer needed to convert an address into a
    string.

Arguments:

    Address - Supplies an optional pointer to a network address to convert to
        a string.

    Buffer - Supplies an optional pointer where the string representation of
        the address will be returned.

    BufferLength - Supplies the length of the supplied buffer, in bytes.

Return Value:

    Returns the maximum length of any address if no network address is
    supplied.

    Returns the actual length of the network address string if a network address
    was supplied, including the null terminator.

--*/

