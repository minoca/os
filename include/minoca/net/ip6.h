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

#define IP6_IS_ANY_ADDRESS(_Ip6Address)     \
    ((*((PULONG)&(_Ip6Address[0])) == 0) && \
     (*((PULONG)&(_Ip6Address[4])) == 0) && \
     (*((PULONG)&(_Ip6Address[8])) == 0) && \
     (*((PULONG)&(_Ip6Address[12])) == 0))

//
// This macros determines whether or not the given IPv6 address is a multicast
// address.
//

#define IP6_IS_MULTICAST_ADDRESS(_Ip6Address) \
    ((_Ip6Address[0]) == 0xFF)

//
// This macros determines whether or not the given IPv6 address is a multicast
// link-local address.
//

#define IP6_IS_MULTICAST_LINK_LOCAL_ADDRESS(_Ip6Address) \
    (IN6_IS_ADDR_MULTICAST(_Ip6Address) &&               \
     (((_Ip6Address[1]) & 0x0F) == 0x2))

//
// This macros determines whether or not the given IPv6 address is a unicast
// link-local address.
//

#define IP6_IS_UNICAST_LINK_LOCAL_ADDRESS(_Ip6Address)                 \
    ((*((PULONG)&(_Ip6Address[0])) == CPU_TO_NETWORK32(0xFE800000)) && \
     (*((PULONG)&(_Ip6Address[4])) == 0))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the current IPv6 version number.
//

#define IP6_VERSION 6

//
// Define the bits for the header member that stores the version, traffic
// class, and flow label.
//

#define IP6_VERSION_MASK        0xF0000000
#define IP6_VERSION_SHIFT       28
#define IP6_TRAFFIC_CLASS_MASK  0x0FF00000
#define IP6_TRAFFIC_CLASS_SHIFT 20
#define IP6_FLOW_LABEL_MASK     0x000FFFFF
#define IP6_FLOW_LABEL_SHIFT    0

//
// Define the maximum payload length that can be stored in an IPv6 header.
//

#define IP6_MAX_PAYLOAD_LENGTH 0xFFFF

//
// Define the minimum link MTU required by IPv6.
//

#define IP6_MINIMUM_LINK_MTU 1280

//
// Define the base length of all IPv6 extension headers and the multiple of the
// header's stored length value.
//

#define IP6_EXTENSION_HEADER_LENGTH_BASE 8
#define IP6_EXTENSION_HEADER_LENGTH_MULTIPLE 8

//
// Define default and maximum values for the IPv6 header hop limit.
//

#define IP6_DEFAULT_HOP_LIMIT 64
#define IP6_DEFAULT_MULTICAST_HOP_LIMIT 1
#define IP6_MAX_HOP_LIMIT 0xFF

//
// Define the hop limit that indicates a link local packet.
//

#define IP6_LINK_LOCAL_HOP_LIMIT 1

//
// Define the size of an IPv6 address, in bytes.
//

#define IP6_ADDRESS_SIZE 16

//
// Define the IPv6 extension header options types.
//

#define IP6_OPTION_TYPE_PAD1 0
#define IP6_OPTION_TYPE_PADN 1
#define IP6_OPTION_TYPE_ROUTER_ALERT 5

//
// Define the IPv6 router alert codes.
//

#define IP6_ROUTER_ALERT_CODE_MLD 0
#define IP6_ROUTER_ALERT_CODE_RSVP 1
#define IP6_ROUTER_ALERT_CODE_ACTIVE_NETWORK 2

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
            NET_DOMAIN_TYPE Domain;
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

    VersionClassFlow - Stores the version number, traffic class, and flow label.

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
    ULONG VersionClassFlow;
    USHORT PayloadLength;
    UCHAR NextHeader;
    UCHAR HopLimit;
    BYTE SourceAddress[IP6_ADDRESS_SIZE];
    BYTE DestinationAddress[IP6_ADDRESS_SIZE];
} PACKED IP6_HEADER, *PIP6_HEADER;

/*++

Structure Description:

    This structure defines the header common to all IPv6 extension headers.

Members:

    NextHeader - Stores the type of the header following this extension header.

    Length - Stores the length of the extension header, not including the first
        8 bytes; all extension headers must be an integer multiple of 8 bytes.

--*/

typedef struct _IP6_EXTENSION_HEADER {
    UCHAR NextHeader;
    UCHAR Length;
} PACKED IP6_EXTENSION_HEADER, *PIP6_EXTENSION_HEADER;

/*++

Structure Description:

    This structure defines an IPv6 extension header option. The variable-length
    option data immediately follows this structure.

Members:

    Type - Stores the type of IPv6 option. See IP6_OPTION_TYPE_* for
        definitions.

    Length - Stores the length of the IPv6 option, in bytes.

--*/

typedef struct _IP6_OPTION {
    UCHAR Type;
    UCHAR Length;
} PACKED IP6_OPTION, *PIP6_OPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

