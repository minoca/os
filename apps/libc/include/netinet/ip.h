/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    ip.h

Abstract:

    This header contains definitions for the Internet Protocol.

Author:

    Evan Green 14-Jan-2015

--*/

#ifndef _NETINET_IP_H
#define _NETINET_IP_H

//
// ------------------------------------------------------------------- Includes
//

#include <netinet/in.h>
#include <netinet/in_systm.h>

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros for options.
//

#define IPOPT_COPIED(_Options) ((_Options & 0x80)
#define IPOPT_CLASS(_Options) ((_Options) & 0x60)
#define IPOPT_NUMBER(_Options) ((_Options) & 0x1F)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the IP protocol version number.
//

#define IPVERSION 4

//
// Define the maximum time to live, in seconds.
//

#define MAXTTL 255

//
// Define the default time to live.
//

#define IPDEFTTL 64

//
// Define the time to live for fragments.
//

#define IPFRAGTTL 60

//
// Define how much to subtract from the TTL when forwarding.
//

#define IPTTLDEC 1

//
// Define the default maximum segment size.
//

#define IP_MSS 576

//
// Define fragment offset bit definitions.
//

//
// This flag is a reserved fragment flag.
//

#define IP_RF 0x8000

//
// This flag is set to indicate "don't fragment" this packet.
//

#define IP_DF 0x4000

//
// This flag is set to indicate more fragments are coming.
//

#define IP_MF 0x2000

//
// This is the mask for fragmenting bits.
//

#define IP_OFFMASK 0x1FFF

//
// Define the maximum IP packet size.
//

#define IP_MAXPACKET 65535

//
// Define types of IP service. CE and ECT are deprecated.
//

#define IPTOS_CE 0x01
#define IPTOS_ECT 0x02

#define IPTOS_MINCOST 0x02
#define IPTOS_RELIABILITY 0x04
#define IPTOS_THROUGHPUT 0x08
#define IPTOS_LOWDELAY 0x10

//
// Define ECN (Explicit Congestion Notification) codepoints from RFC3168,
// mapped to the bottom two bits of the type of service field.
//

//
// This value indicates no ECT.
//

#define IPTOS_ECN_NOTECT 0x00

//
// This value indicates an ECT capable transport (1).
//

#define IPTOS_ECN_ECT1 0x01

//
// This value indicates an ECT capable transport (0).
//

#define IPTOS_ECN_ECT0 0x02

//
// This value indicates that congestion was experienced.
//

#define IPTOS_ECN_CE 0x03

//
// This value is the ECN field mask.
//

#define IPTOS_ECN_MASK 0x03

//
// Define IP precedence values, also in the TOS field.
//

#define IPTOS_PREC_ROUTINE 0x00
#define IPTOS_PREC_IMMEDIATE 0x40
#define IPTOS_PREC_PRIORITY 0x20
#define IPTOS_PREC_FLASH 0x60
#define IPTOS_PREC_FLASHOVERRIDE 0x80
#define IPTOS_PREC_CRITIC_ECP 0xA0
#define IPTOS_PREC_INTERNETCONTROL 0xC0
#define IPTOS_PREC_NETCONTROL 0xE0

//
// Define traffic class definitions, used by wireless LANs.
//

//
// This value indicates standard, best effort service.
//

#define IP_TCLASS_BE 0x00

//
// This value indicates background, low priority data.
//

#define IP_TCLASS_BK 0x20

//
// This value indicates interactive data.
//

#define IP_TCLASS_VI 0x80

//
// This value indicates signaling data.
//

#define IP_TCLASS_VO 0xC0

//
// Define IP options.
//

#define IPOPT_CONTROL 0x00
#define IPOPT_RESERVED1 0x20
#define IPOPT_DEBMEAS 0x40
#define IPOPT_RESERVED2 0x80

//
// Define the end of option list option.
//

#define IPOPT_EOL 0

//
// Define the no-operation option.
//

#define IPOPT_NOP 1

//
// Define the record packet route option.
//

#define IPOPT_RR 7

//
// Define the timestamp option.
//

#define IPOPT_TS 68

//
// Define the security option.
//

#define IPOPT_SECURITY 130

//
// Define the loose source route option.
//

#define IPOPT_LSRR 131

//
// Define the satnet ID option.
//

#define IPOPT_SATID 136

//
// Define the strict source route option.
//

#define IPOPT_SSRR 137

//
// Define the router alert option.
//

#define IPOPT_RA 148

//
// Define offsets to fields in options.
//

//
// Define the option ID offset.
//

#define IPOPT_OPTVAL 0

//
// Define the option length offset.
//

#define IPOPT_OLEN 1

//
// Define the offset within the option.
//

#define IPOPT_OFFSET 2

//
// Define the minimum offset value.
//

#define IPOPT_MINOFF 4

//
// Define values for IP timestamp flags.
//

//
// This value indicates the data contains timestamps only.
//

#define IPOPT_TS_TSONLY 0

//
// This value indicates timestamps and address are present.
//

#define IPOPT_TS_TSANDADDR 1

//
// This value indicates specified modules only.
//

#define IPOPT_TS_PRESPEC 3

//
// Define bits for the security option. These are not byte swapped.
//

#define IPOPT_SECUR_UNCLASS 0x0000
#define IPOPT_SECUR_CONFID 0xF135
#define IPOPT_SECUR_EFTO 0x789A
#define IPOPT_SECUR_MMMM 0xBC4D
#define IPOPT_SECUR_RESTR 0xAF13
#define IPOPT_SECUR_SECRET 0xD788
#define IPOPT_SECUR_TOPSECRET 0x6BC5

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an Internet Protocol header.

Members:

    ip_vhl - Stores the IP version (shifted left by 4) and the header length
        (divided by 4).

    ip_tos - Stores the type of service.

    ip_len - Stores the total length of the packet.

    ip_id - Stores the identification.

    ip_off - Stores the fragment offset field. See IP_* definitions.

    ip_ttl - Stores the time to live.

    ip_p - Stores the inner protocol.

    ip_sum - Stores the IP checksum.

    ip_src - Stores the source address.

    ip_dst - Stores the destination address.

--*/

struct ip {
    u_char ip_vhl;
    u_char ip_tos;
    u_short ip_len;
    u_short ip_id;
    u_short ip_off;
    u_char ip_ttl;
    u_char ip_p;
    u_short ip_sum;
    struct in_addr ip_src;
    struct in_addr ip_dst;
};

/*++

Structure Description:

    This structure defines an Internet Protocol timestamp.

Members:

    ipt_code - Stores the option type, set to IPOPT_TS.

    ipt_len - Stores the size of the structure.

    ipt_ptr - Stores the index of the current entry.

    ipt_flag - Stores the flags value. See IPOPT_TS_* definitions.

    ipt_oflow - Stores the overflow counter.

--*/

struct ip_timestamp {
    u_char ipt_code;
    u_char ipt_len;
    u_char ipt_ptr;
    u_int ipt_flg:4;
    u_int ipt_oflow:4;
    union ipt_timestamp {
        n_long ipt_time[1];
        struct ipt_ta {
            struct in_addr ipt_addr;
            n_long ipt_time;
        } ipt_ta[1];

    } ipt_timestamp;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

