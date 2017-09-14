/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    icmp6.h

Abstract:

    This header contains public definitions for the IGMP protocol layer.

Author:

    Chris Stevens 25-Aug-2017

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
// Define ICMPv6 message types.
//

#define ICMP6_MESSAGE_TYPE_PARAMETER_PROBLEM 4
#define ICMP6_MESSAGE_TYPE_MLD_QUERY 130
#define ICMP6_MESSAGE_TYPE_MLD_REPORT 131
#define ICMP6_MESSAGE_TYPE_MLD_DONE 132
#define ICMP6_MESSAGE_TYPE_NDP_ROUTER_SOLICITATION 133
#define ICMP6_MESSAGE_TYPE_NDP_ROUTER_ADVERTISEMENT 134
#define ICMP6_MESSAGE_TYPE_NDP_NEIGHBOR_SOLICITATION 135
#define ICMP6_MESSAGE_TYPE_NDP_NEIGHBOR_ADVERTISEMENT 136
#define ICMP6_MESSAGE_TYPE_NDP_REDIRECT 137
#define ICMP6_MESSAGE_TYPE_MLD2_REPORT 143

//
// Define the ICMPv6 Parameter Problem message codes.
//

#define ICMP6_PARAMETER_PROBLEM_CODE_ERRONEOUS_HEADER 0
#define ICMP6_PARAMETER_PROBLEM_CODE_UNRECOGNIZED_NEXT_HEADER 1
#define ICMP6_PARAMETER_PROBLEM_CODE_UNRECOGNIZED_IPV6_OPTION 2

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the header for ICMPv6 messages.

Members:

    Type - Stores the ICMPv6 message type.

    Code - Stores the ICMPv6 message code.

    Checksum - Stores the ICMP checksum of the ICMPv6 message.

--*/

typedef struct _ICMP6_HEADER {
    UCHAR Type;
    UCHAR Code;
    USHORT Checksum;
} ICMP6_HEADER, *PICMP6_HEADER;

/*++

Structure Description:

    This structure defines a request for ICMPv6 to configure or dismantle a
    network address.

Members:

    Link - Stores a pointer to the link to which the network address belongs.

    LinkAddress - Stores a pointer to the link address entry to configure or
        dismantle.

    Configure - Stores a boolean indicating if the address shall be configured
        (TRUE) or dismantled (FALSE).

--*/

typedef struct _ICMP6_ADDRESS_CONFIGURATION_REQUEST {
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    BOOL Configure;
} ICMP6_ADDRESS_CONFIGURATION_REQUEST, *PICMP6_ADDRESS_CONFIGURATION_REQUEST;

/*++

Enumeration Description:

    This enumeration describes the various ICMPv6 options for the ICMPv6 socket
    information class.

Values:

    SocketIcmp6OptionInvalid - Indicates an invalid ICMPv6 socket option.

    SocketIcmp6OptionJoinMulticastGroup - Indicates a request to join a
        multicast group. This option takes a NET_NETWORK_MULTICAST_REQUEST
        structure.

    SocketIcmp6OptionLeaveMulticastGroup - Indicates a request to leave a
        multicast group. This option takes a NET_NETWORK_MULTICAST_REQUEST
        structure.

    SocketIcmp6OptionConfigureAddress - Indicates a request to configure or
        dismantle a network address. This options takes an
        ICMP6_ADDRESS_CONFIGURATION_REQUEST structure.

--*/

typedef enum _SOCKET_ICMP6_OPTION {
    SocketIcmp6OptionInvalid,
    SocketIcmp6OptionJoinMulticastGroup,
    SocketIcmp6OptionLeaveMulticastGroup,
    SocketIcmp6OptionConfigureAddress
} SOCKET_ICMP6_OPTION, *PSOCKET_ICMP6_OPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

