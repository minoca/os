/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ip4.h

Abstract:

    This header contains public definitions for the IPv4 network layer.

Author:

    Evan Green 5-Apr-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines whether or not the given IPv4 address is a multicast
// address. The address is treated as being in network byte order.
//

#define IP4_IS_MULTICAST_ADDRESS(_Ip4Address) \
    (((_Ip4Address) & 0x000000F0) == 0x000000E0)

//
// ---------------------------------------------------------------- Definitions
//

#define IP4_ALLOCATION_TAG 0x21347049 // '!4pI'

#define IP4_VERSION              0x40
#define IP4_VERSION_MASK         0xF0
#define IP4_HEADER_LENGTH_MASK   0x0F
#define IP4_MAX_PACKET_SIZE      0xFFFF

#define IP4_TYPE_ECN_MASK  0x03
#define IP4_TYPE_DSCP_MASK 0xFC

#define IP4_PRECEDENCE_NETWORK_CONTROL 0xC0

#define IP4_FLAG_MORE_FRAGMENTS  0x1
#define IP4_FLAG_DO_NOT_FRAGMENT 0x2
#define IP4_FLAG_RESERVED        0x4
#define IP4_FLAGS               \
    (IP4_FLAG_RESERVED |        \
     IP4_FLAG_DO_NOT_FRAGMENT | \
     IP4_FLAG_MORE_FRAGMENTS)

#define IP4_FRAGMENT_FLAGS_MASK   0x7
#define IP4_FRAGMENT_FLAGS_SHIFT  13
#define IP4_FRAGMENT_OFFSET_MASK  0x1FFF
#define IP4_FRAGMENT_OFFSET_SHIFT 0

#define IP4_INITIAL_TIME_TO_LIVE 63
#define IP4_INITIAL_MULTICAST_TIME_TO_LIVE 1

#define IP4_BROADCAST_ADDRESS    0xFFFFFFFF

#define IP4_ADDRESS_SIZE         4

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an IPv4 address.

Members:

    Domain - Stores the network domain of this address.

    Port - Stores the 16 bit port number.

    Address - Stores the 32 bit IP address.

    NetworkAddress - Stores the unioned opaque version, used to ensure the
        structure is the proper size.

--*/

typedef struct _IP4_ADDRESS {
    union {
        struct {
            NET_DOMAIN_TYPE Domain;
            ULONG Port;
            ULONG Address;
        };

        NETWORK_ADDRESS NetworkAddress;
    };

} IP4_ADDRESS, *PIP4_ADDRESS;

/*++

Structure Description:

    This structure defines an IPv4 header.

Members:

    Version - Stores the version number and header length.

    Type - Stores the Differentiated Services Code Point (originally called
        Type of Service), and the Explicit Congestion Notification.

    TotalLength - Stores the total length of the packet, including the header
        and data, in bytes.

    Identification - Stores an identification field, usually used for uniquely
        identifying fragments of an original IP datagram.

    FragmentOffset - Stores some flags, as well as the fragment offset
        relative to the beginning of the original unfragmented IP datagram.

    TimeToLive - Stores the number of remaining hops this packet can make
        before being discarded.

    Protocol - Stores the protocol number for the next protocol.

    HeaderChecksum - Stores the 16 bit one's complement of the one's complement
        sum of all 16 bit words in the header.

    SourceAddress - Stores the source IP address of the packet.

    DestinationAddress - Stores the destination IP address of the packet.

--*/

typedef struct _IP4_HEADER {
    UCHAR VersionAndHeaderLength;
    UCHAR Type;
    USHORT TotalLength;
    USHORT Identification;
    USHORT FragmentOffset;
    UCHAR TimeToLive;
    UCHAR Protocol;
    USHORT HeaderChecksum;
    ULONG SourceAddress;
    ULONG DestinationAddress;
} PACKED IP4_HEADER, *PIP4_HEADER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

