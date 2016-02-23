/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    netlink.h

Abstract:

    This header contains definitions for netlink socket communication endpoints.

Author:

    Chris Stevens 11-Feb-2016

--*/

#ifndef _NETLINK_H
#define _NETLINK_H

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns the required alignment for a given length. This is a
// constant expression.
//

#define NLMSG_ALIGN(_Length) \
    (((_Length) + NLMSG_ALIGNTO - 1) & ~(NLMSG_ALIGNTO - 1))

//
// This macro returns the aligned size of a netlink message header.
//

#define NLMSG_HDRLEN (int)NLMSG_ALIGN(sizeof(struct nlmsghdr))

//
// This macro returns the value to store in the nlmsgdr length member, taking
// into account any necessary alignment. It takes the data length as an
// argument. This is a constant expression.
//

#define NLMSG_LENGTH(_Length) (NLMSG_HDRLEN + (_Length))

//
// This macro returns the number of bytes a netlink message with the given
// payload size takes up. This is a constant expression.
//

#define NLMSG_SPACE(_Length) NLMSG_ALIGN(NLMSG_LENGTH(_Length))

//
// This macro evaluates to a pointer to the ancillary data following a nlmsghdr
// structure.
//

#define NLMSG_DATA(_Header) ((void *)((char *)(_Header) + NLMSG_HDRLEN))

//
// This macro returns the next message header, but first decrements the length
// of the current header's message from the given length.
//

#define NLMSG_NEXT(_Header, _Length)                                           \
    (_Length) -= NLMSG_ALIGN((_Header)->nlmsg_len),                            \
    (struct nlmsghdr *)((char *)(_Header) + NLMSG_ALIGN((_Header)->nlmsg_len))

//
// This macro determines if the given netlink message header and length are
// valid.
//

#define NLMSG_OK(_Header, _Length) \
    (((_Length) >= (int)sizeof(struct nlmsghdr)) && \
     ((_Header)->nlmsg_len >= sizeof(struct nlmsghdr)) && \
     ((_Header)->nlmsg_len <= (_Length)))

//
// This macro returns the bytes remaining in a message beyond a given length.
//

#define NLMSG_PAYLOAD(_Header, _Length) \
    ((_Header)->nlmsg_len - NLMSG_SPACE(_Length))

//
// This macro returns the required alignment for a given netlink attribute
// length. This is a constant expression.
//

#define NLA_ALIGN(_Length) (((_Length) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))

//
// This macro returns the aligned aize of a netlink message header.
//

#define NLA_HDRLEN (int)NLA_ALIGN(sizeof(struct nlattr))

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the protocol values for the AF_NETLINK socket domain.
//

#define NETLINK_GENERIC 0

//
// Define the flags for the netlink message header.
//

#define NLM_F_REQUEST   0x0001
#define NLM_F_MULTI     0x0002
#define NLM_F_ACK       0x0004
#define NLM_F_ECHO      0x0008
#define NLM_F_DUMP_INTR 0x0010
#define NLM_F_ROOT      0x0020
#define NLM_F_MATCH     0x0040
#define NLM_F_REPLACE   0x0080
#define NLM_F_EXCL      0x0100
#define NLM_F_CREATE    0x0200
#define NLM_F_APPEND    0x0400

#define NLM_F_DUMP (NLM_F_ROOT | NLM_F_MATCH)

//
// Define the required alignment for netlink messages.
//

#define NLMSG_ALIGNTO 4U

//
// Define the global netlink message types.
//

#define NLMSG_NOOP 1
#define NLMSG_ERROR 2
#define NLMSG_DONE 3
#define NLMSG_OVERRUN 4

//
// Define the mininimum allowed message type for the netlink protocol's private
// message types.
//

#define NLMSG_MIN_TYPE 16

//
// Define the netlink attribute flags.
//

#define NLA_F_NESTED        0x8000
#define NLA_F_NET_BYTEORDER 0x4000

//
// Define the netlink attribute type mask used to strip away the flags.
//

#define NLA_TYPE_MASK ~(NLA_F_NESTED | NLA_F_NET_BYTEORDER)

//
// Define the netlink attribute alignment.
//

#define NLA_ALIGNTO 4

//
// Define the netlink socket options.
//

#define NETLINK_ADD_MEMBERSHIP 1
#define NETLINK_DROP_MEMBERSHIP 2
#define NETLINK_PKTINFO 3
#define NETLINK_BROADCAST_ERROR 4
#define NETLINK_NO_ENOBUFS 5
#define NETLINK_RX_RING 6
#define NETLINK_TX_RING 7
#define NETLINK_LISTEN_ALL_NSID 8
#define NETLINK_LIST_MEMBERSHIPS 9

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the unsigned short type used for the netlink sockaddr family type.
//

typedef unsigned short __kernel_sa_family_t;

/*++

Structure Description:

    This structure defines a netlink socket address.

Members:

    nl_family - Stores the netlink socket address family. Should be AF_NETLINK.

    nl_pad - Stores 2 bytes of padding.

    nl_pid - Stores the port ID of the netlink socket address.

    nl_groups - Stores a bitmask of multicast groups.

--*/

struct sockaddr_nl {
    __kernel_sa_family_t nl_family;
    unsigned short nl_pad;
    uint32_t nl_pid;
    uint32_t nl_groups;
};

/*++

Structure Description:

    This structure defines a netlink message header.

Members:

    nlmsg_len - Stores the length of the message in bytes, including the
        message header.

    nlmsg_type - Stores the protocol specific netlink message type.

    nlmsg_flags - Stores a bitmask of netlink message flags. See NLM_F_* for
        definitions.

    nlmsg_seq - Stores the sequence number of the netlink message.

    nlmsg_pid - Stores the port ID of the sending netlink socket.

--*/

struct nlmsghdr {
    uint32_t nlmsg_len;
    uint16_t nlmsg_type;
    uint16_t nlmsg_flags;
    uint32_t nlmsg_seq;
    uint32_t nlmsg_pid;
};

/*++

Structure Description:

    This structure defines a netlink error message.

Members:

    error - Stores the error value generated by the message that caused the
        error.

    msg - Stores the header of the message that caused the error.

--*/

struct nlmsgerr {
    int error;
    struct nlmsghdr msg;
};

/*++

Structure Description:

    This structure defines the netlink socket packet information socket option
    data.

Members:

    group - Stores the group packet information.

--*/

struct nl_pktinfo {
    int group;
};

/*++

Structure Description:

    This structure defines a netlink message attribute.

Members:

    nla_len - Stores the length of the attribute in bytes, including the the
        header.

    nla_type - Stores the type of the attribute and flags. The type depends on
        the protocol and the flags are defined as NLA_F_*.

--*/

struct nlattr {
    uint16_t nla_len;
    uint16_t nla_type;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#ifdef __cplusplus

}

#endif
#endif

