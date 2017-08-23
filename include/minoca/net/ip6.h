/*++

Copyright (c) 2017 Minoca Corp. All Rights Reserved

Module Name:

    ip6.h

Abstract:

    This header contains public definitions for the IPv6 network layer.

Author:

    Chris Stevens 22-Aug-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macros determines whether or not the given IPv6 address is the any
// address.
//

#define IP6_IS_ANY_ADDRESS(_Ip6Address)                \
    ((*((PULONG)&((_Ip6Address)->Address[0])) == 0) && \
     (*((PULONG)&((_Ip6Address)->Address[4])) == 0) && \
     (*((PULONG)&((_Ip6Address)->Address[8])) == 0) && \
     (*((PULONG)&((_Ip6Address)->Address[12])) == 0))

//
// This macros determines whether or not the given IPv6 address is a multicast
// address.
//

#define IP6_IS_MULTICAST_ADDRESS(_Ip6Address) \
    ((_Ip6Address)->Address[0] == 0xFF)

//
// This macros determines whether or not the given IPv6 address is a multicast
// link-local address.
//

#define IP6_IS_MULTICAST_LINK_LOCAL_ADDRESS(_Ip6Address) \
    (IN6_IS_ADDR_MULTICAST(_Ip6Address) &&               \
     (((_Ip6Address)->Address[1] & 0x0F) == 0x2))

//
// This macros determines whether or not the given IPv6 address is a unicast
// link-local address.
//

#define IP6_IS_UNICAST_LINK_LOCAL_ADDRESS(_Ip6Address) \
    ((*((PULONG)&((_Ip6Address)->Address[0])) ==       \
      CPU_TO_NETWORK32(0xFE800000)) &&                 \
     (*((PULONG)&((_Ip6Address)->Address[4])) == 0))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the current IPv6 version number.
//

#define IP6_VERSION 6

//
// Define the maximum payload length that can be stored in an IPv6 header.
//

#define IP6_MAX_PAYLOAD_LENGTH 0xFFFF

//
// Define default and maximum values for the IPv6 header hop limit.
//

#define IP6_DEFAULT_HOP_LIMIT 64
#define IP6_MAX_HOP_LIMIT 0xFFFF

//
// Define the size of an IPv6 address, in bytes.
//

#define IP6_ADDRESS_SIZE 16

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an IPv6 address.

Members:

    Domain - Stores the network domain of this address.

    Port - Stores the 16 bit port number.

    Address - Stores the 128 bit IP address.

    NetworkAddress - Stores the unioned opaque version, used to ensure the
        structure is the proper size.

--*/

typedef struct _IP6_ADDRESS {
    union {
        struct {
            NET_DOMAIN_TYPE Network;
            ULONG Port;
            UCHAR Address[IP6_ADDRESS_SIZE];
        };

        NETWORK_ADDRESS NetworkAddress;
    };

} IP6_ADDRESS, *PIP6_ADDRESS;

/*++

Structure Description:

    This structure defines an IPv6 header.

Members:

    Version - Stores the version number.

    TrafficClass - Stores the packet's traffic classification information.

    FlowLabel - Stores a label used by routers to help identify groups of
        packets to be sent on the same path.

    PayloadLength - Store the length of the rest of the packet, in bytes. This
        does not include the IPv6 header.

    NextHeader - Stores the type of the next header in the packet. This takes
        the same values as the IPv4 header's protocol field.

    HopLimit - Stores the limit on the number of intermediate nodes the packet
        can encounter before being discarded.

    SourceAddress - Stores the source IP address of the packet.

    DestinationAddress - Stores the destination IP address of the packet.

--*/

typedef struct _IP6_HEADER {
    struct {
        ULONG Version:4;
        ULONG TrafficClass:8;
        ULONG FlowLabel:20;
    };

    USHORT PayloadLength;
    UCHAR NextHeader;
    UCHAR HopLimit;
    BYTE SourceAddress[IP6_ADDRESS_SIZE];
    BYTE DestinationAddress[IP6_ADDRESS_SIZE];
} PACKED IP6_HEADER, *PIP6_HEADER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
