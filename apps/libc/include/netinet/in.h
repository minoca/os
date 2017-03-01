/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    in.h

Abstract:

    This header contains definitions for the internet address family.

Author:

    Evan Green 3-May-2013

--*/

#ifndef _NETINET_IN_H
#define _NETINET_IN_H

//
// ------------------------------------------------------------------- Includes
//

#include <endian.h>
#include <libcbase.h>
#include <inttypes.h>
#include <sys/socket.h>

//
// --------------------------------------------------------------------- Macros
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define macros that determine if the given IPv4 address is in the given
// class.
//

#define IN_CLASSA(_Address) (((uint32_t)(_Address) & 0x80000000) == 0)
#define IN_CLASSB(_Address) (((uint32_t)(_Address) & 0xC0000000) == 0x80000000)
#define IN_CLASSC(_Address) (((uint32_t)(_Address) & 0xE0000000) == 0xC0000000)
#define IN_CLASSD(_Address) (((uint32_t)(_Address) & 0xF0000000) == 0xE0000000)
#define IN_MULTICAST(_Address) IN_CLASSD(_Address)
#define IN_EXPERIMENTAL(_Address) \
    (((uint32_t)(_Address) & 0xF0000000) == 0xF0000000)

#define IN_LOOPBACK(_Address) \
    (((uint32_t)(_Address) & 0xFF000000) == 0x7F000000)

//
// This macros determines whether or not the given address is the IPv6
// unspecified address.
//

#define IN6_IS_ADDR_UNSPECIFIED(_Address)                               \
    (((_Address)->s6_u.s6u_addr32[0] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[1] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[2] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[3] == 0))

//
// This macros determines whether or not the given address is the IPv6 loopback
// address.
//

#define IN6_IS_ADDR_LOOPBACK(_Address)                                  \
    (((_Address)->s6_u.s6u_addr32[0] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[1] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[2] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[3] == htonl(1)))

//
// This macros determines whether or not the given IPv6 address is a multicast
// address.
//

#define IN6_IS_ADDR_MULTICAST(_Address) ((_Address)->s6_addr[0] == 0xFF)

//
// This macros determines whether or not the given IPv6 address is a unicast
// link-local address.
//

#define IN6_IS_ADDR_LINKLOCAL(_Address)                                \
    ((*((uint32_t *)&((_Address)->s6_addr[0])) == htonl(0xFE800000) && \
     (*((uint32_t *)&((_Address)->s6_addr[4])) == 0)))

//
// This macros determines whether or not the given IPv6 address is a unicast
// site-local address.
//

#define IN6_IS_ADDR_SITELOCAL(_Address)         \
    (((_Address)->s6_addr[0] == 0xFE) &&        \
     (((_Address)->s6_addr[1] & 0xC0) == 0xC0))

//
// This macros determines whether or not the given IPv6 address is an IPv4
// mapped address.
//

#define IN6_IS_ADDR_V4MAPPED(_Address)                                  \
    (((_Address)->s6_u.s6u_addr32[0] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[1] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[2] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[3] == htonl(0x0000FFFF)))

//
// This macros determines whether or not the given IPv6 address is an IPv4
// compatible address.
//

#define IN6_IS_ADDR_V4COMPAT(_Address)                                  \
    (((_Address)->s6_u.s6u_addr32[0] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[1] == 0) &&                           \
     ((_Address)->s6_u.s6u_addr32[2] == 0) &&                           \
     (((_Address)->s6_u.s6u_addr32[3] & ~htonl(1)) != 0))

//
// This macros determines whether or not the given IPv6 address is a multicast
// node-local address.
//

#define IN6_IS_ADDR_MC_NODELOCAL(_Address)     \
    (IN6_IS_ADDR_MULTICAST(_Address) &&        \
     (((_Address)->s6_addr[1] & 0x0F) == 0x1))

//
// This macros determines whether or not the given IPv6 address is a multicast
// link-local address.
//

#define IN6_IS_ADDR_MC_LINKLOCAL(_Address)     \
    (IN6_IS_ADDR_MULTICAST(_Address) &&        \
     (((_Address)->s6_addr[1] & 0x0F) == 0x2))

//
// This macros determines whether or not the given IPv6 address is a multicast
// site-local address.
//

#define IN6_IS_ADDR_MC_SITELOCAL(_Address)     \
    (IN6_IS_ADDR_MULTICAST(_Address) &&        \
     (((_Address)->s6_addr[1] & 0x0F) == 0x5))

//
// This macros determines whether or not the given IPv6 address is a multicast
// organization-local address.
//

#define IN6_IS_ADDR_MC_ORGLOCAL(_Address)       \
    (IN6_IS_ADDR_MULTICAST(_Address) &&         \
     (((_Address)->s6_addr[1] & 0x0F) == 0x8))

//
// This macros determines whether or not the given IPv6 address is a multicast
// global address.
//

#define IN6_IS_ADDR_MC_GLOBAL(_Address)         \
    (IN6_IS_ADDR_MULTICAST(_Address) &&         \
     (((_Address)->s6_addr[1] & 0x0F) == 0xE))

//
// This macro returns non-zero if the two given IPv6 addresses are the same.
//

#define IN6_ARE_ADDR_EQUAL(_FirstAddress, _SecondAddress)       \
    (((_FirstAddress)->s6_u.s6u_addr32[0] ==                    \
      (_SecondAddress)->s6_u.s6u_addr32[0]) &&                  \
     ((_FirstAddress)->s6_u.s6u_addr32[1] ==                    \
      (_SecondAddress)->s6_u.s6u_addr32[1]) &&                  \
     ((_FirstAddress)->s6_u.s6u_addr32[2] ==                    \
      (_SecondAddress)->s6_u.s6u_addr32[2]) &&                  \
     ((_FirstAddress)->s6_u.s6u_addr32[3] ==                    \
      (_SecondAddress)->s6_u.s6u_addr32[3]))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the IPv4 broadcast and any addresses.
//

#define INADDR_ANY ((uint32_t)0x00000000)
#define INADDR_LOOPBACK ((uint32_t)0x7F000001)
#define INADDR_BROADCAST ((uint32_t)0xFFFFFFFF)
#define INADDR_NONE ((uint32_t)0xFFFFFFFF)

//
// Define the network number for the local host loopback.
//

#define IN_LOOPBACKNET 127

//
// Define the IPv6 loopback and any addresses.
//

#define IN6_ANY_INIT {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}}
#define IN6_LOOPBACK_INIT {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}}

//
// Define IP protocols. These match the IANA protocol values so that undefined
// protocols can be used on raw sockets.
//

#define IPPROTO_ICMP 1
#define IPPROTO_IP 4
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41
#define IPPROTO_ICMPV6 58
#define IPPROTO_RAW 255

//
// Define some well known port numbers.
//

#define IPPORT_ECHO 7
#define IPPORT_DISCARD 9
#define IPPORT_SYSTAT 11
#define IPPORT_DAYTIME 13
#define IPPORT_NETSTAT 15
#define IPPORT_FTP 21
#define IPPORT_TELNET 23
#define IPPORT_SMTP 25
#define IPPORT_TIMESERVER 37
#define IPPORT_NAMESERVER 42
#define IPPORT_WHOIS 43
#define IPPORT_MTP 57
#define IPPORT_TFTP 69
#define IPPORT_RJE 77
#define IPPORT_FINGER 79
#define IPPORT_TTYLINK 87
#define IPPORT_SUPDUP 95
#define IPPORT_EXECSERVER 512
#define IPPORT_LOGINSERVER 513
#define IPPORT_CMDSERVER 514
#define IPPORT_EFSSERVER 520

#define IPPORT_BIFFUDP 512
#define IPPORT_WHOSERVER 513
#define IPPORT_ROUTESERVER 520

//
// Define the value below which are reserved for privileged processes.
//

#define IPPORT_RESERVED 1024

//
// Ports greater than this value are reserved for non-privileged servers.
//

#define IPPORT_USERRESERVED 5000

//
// Define the constant size of an IP address string.
//

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

//
// Define socket options for IPv4.
//

//
// This option indicates that data packets contain the IPv4 header.
//

#define IP_HDRINCL 1

//
// This option joins a multicast group.
//

#define IP_ADD_MEMBERSHIP 2

//
// This option leaves a multicast group.
//

#define IP_DROP_MEMBERSHIP 3

//
// This option defines the interface to use for outgoing multicast packets.
//

#define IP_MULTICAST_IF 4

//
// This options defines the time-to-live value for outgoing multicast packets.
//

#define IP_MULTICAST_TTL 5

//
// This option specifies if packets are delivered back to the local application.
//

#define IP_MULTICAST_LOOP 6

//
// This options defines the time-to-live value for outgoing packets.
//

#define IP_TTL 7

//
// This options defines the type-of-service value for outgoing packets. This
// field is now known as the differentiated services code point (DSCP).
//

#define IP_TOS 8

//
// Define socket options for IPv6.
//

//
// This option joins a multicast group.
//

#define IPV6_JOIN_GROUP 1

//
// This option leaves a multicast group.
//

#define IPV6_LEAVE_GROUP 2

//
// This option defines the multicast hop limit.
//

#define IPV6_MULTICAST_HOPS 3

//
// This option defines the interface to use for outgoing multicast packets.
//

#define IPV6_MULTICAST_IF 4

//
// This option specifies if packets are delivered back to the local application.
//

#define IPV6_MULTICAST_LOOP 5

//
// This option defines the unicast hop limit.
//

#define IPV6_UNICAST_HOPS 6

//
// This option restricts a socket to IPv6 communications only.
//

#define IPV6_V6ONLY 7

//
// Define IPv4 class A, B, C, and D definitions.
//

#define IN_CLASSA_NET 0xFF000000
#define IN_CLASSA_NSHIFT 24
#define IN_CLASSA_HOST (0xFFFFFFFF & ~IN_CLASSA_NET)
#define IN_CLASSA_MAX 128

#define IN_CLASSB_NET 0xFFFF0000
#define IN_CLASSB_NSHIFT 16
#define IN_CLASSB_HOST (0xFFFFFFFF & ~IN_CLASSB_NET)
#define IN_CLASSB_MAX 65536

#define IN_CLASSC_NET 0xFFFFFF00
#define IN_CLASSC_NSHIFT 8
#define IN_CLASSC_HOST (0xFFFFFFFF & ~IN_CLASSC_NET)

#define IN_CLASSD_NET 0xF0000000
#define IN_CLASSD_NSHIFT 28
#define IN_CLASSD_HOST (0xFFFFFFFF & ~IN_CLASSD_NET)

#define IN_LOOPBACKNET 127

//
// This macro is defined to handle the conflict between the standards
// definition of in6_addr (which is that it has a member named s6_addr), and
// the macros which need to access the data by larger quantities and cannot
// break strict-aliasing rules. Nameless unions are also not allowed in strict
// C.
//

#define s6_addr s6_u.s6u_addr8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define types for the port and address fields.
//

typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

/*++

Structure Description:

    This structure defines an internet family address.

Members:

    s_addr - Stores the internet family (v4) address.

--*/

struct in_addr {
    in_addr_t s_addr;
};

/*++

Structure Description:

    This structure defines an internet family socket address.

Members:

    sin_family - Stores the family name, which is always AF_INET for internet
        family addresses.

    sin_port - Stores the port number, in network byte order.

    sin_addr - Stores the IPv4 address, in network byte order.

    sin_zero - Stores padding bytes to make the size of the structure line up
        with the sockaddr structure.

--*/

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    char sin_zero[20];
};

/*++

Structure Description:

    This structure defines an internet family version 4 multicast request.

Members:

    imr_multiaddr - Stores the multicast address of the group to join or leave.

    imr_interface - Stores the address of the interface that is to join or
        leave the multicast group.

--*/

struct ip_mreq {
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
};

/*++

Structure Description:

    This structure defines an internet family version 6 address. This structure
    is defined to only have one member, s6_addr, which is an array of 8-bit
    integers. Many macros need to access the words of it however, and to avoid
    breaking strict-aliasing rules in C a union is provided. To continue being
    standards-compliant, a macro is defined for s6_addr to reach through the
    union.

Members:

    s6_addr - Stores the IPv6 network address, in network byte order.

--*/

struct in6_addr {
    union {
        uint8_t s6u_addr8[16];
        uint16_t s6u_addr16[8];
        uint32_t s6u_addr32[4];
    } s6_u;
};

/*++

Structure Description:

    This structure defines an internet family version 6 socket address.

Members:

    sin6_family - Stores the family name, which is always AF_INET6 for internet
        family version 6 addresses.

    sin6_port - Stores the port number, in network byte order.

    sin6_flowinfo - Stores IPv6 traffic class and flow information.

    sin6_addr - Stores the IPv6 address, in network byte order.

    sin6_scope_id - Stores the set of interfaces for a scope.

--*/

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

/*++

Structure Description:

    This structure defines an internet family version 6 multicast request.

Members:

    ipv6mr_multiaddr - Stores the multicast address of the group to join or
        leave.

    ipv6mr_interface - Stores the index of the interface that is to join or
        leave the multicast group.

--*/

struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr;
    unsigned ipv6mr_interface;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Define the "any" address for IPv6.
//

extern LIBC_API const struct in6_addr in6addr_any;

//
// Define the IPv6 loopback address.
//

extern LIBC_API const struct in6_addr in6addr_loopback;

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
uint32_t
htonl (
    uint32_t HostValue
    );

/*++

Routine Description:

    This routine converts a 32-bit value from host order to network order.

Arguments:

    HostValue - Supplies the integer to convert into network order.

Return Value:

    Returns the given integer in network order.

--*/

LIBC_API
uint32_t
ntohl (
    uint32_t NetworkValue
    );

/*++

Routine Description:

    This routine converts a 32-bit value from network order to host order.

Arguments:

    NetworkValue - Supplies the integer to convert into host order.

Return Value:

    Returns the given integer in host order.

--*/

LIBC_API
uint16_t
htons (
    uint16_t HostValue
    );

/*++

Routine Description:

    This routine converts a 16-bit value from host order to network order.

Arguments:

    HostValue - Supplies the integer to convert into network order.

Return Value:

    Returns the given integer in network order.

--*/

LIBC_API
uint16_t
ntohs (
    uint16_t NetworkValue
    );

/*++

Routine Description:

    This routine converts a 16-bit value from network order to host order.

Arguments:

    NetworkValue - Supplies the integer to convert into host order.

Return Value:

    Returns the given integer in host order.

--*/

#ifdef __cplusplus

}

#endif
#endif

