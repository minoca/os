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

#define ICMP6_MESSAGE_TYPE_MLD_QUERY 130
#define ICMP6_MESSAGE_TYPE_MLD_REPORT 131
#define ICMP6_MESSAGE_TYPE_MLD_DONE 132
#define ICMP6_MESSAGE_TYPE_MLD2_REPORT 143

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
    BYTE Type;
    BYTE Code;
    USHORT Checksum;
} ICMP6_HEADER, *PICMP6_HEADER;

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

--*/

typedef enum _SOCKET_ICMP6_OPTION {
    SocketIcmp6OptionInvalid,
    SocketIcmp6OptionJoinMulticastGroup,
    SocketIcmp6OptionLeaveMulticastGroup
} SOCKET_ICMP6_OPTION, *PSOCKET_ICMP6_OPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

