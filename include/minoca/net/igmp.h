/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    igmp.h

Abstract:

    This header contains public definitions for the IGMP protocol layer.

Author:

    Chris Stevens 2-Mar-2017

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
// Define the IPv4 address to which all IGMP general query messages are sent.
//

#define IGMP_ALL_SYSTEMS_ADDRESS CPU_TO_NETWORK32(0xE0000001)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Enumeration Description:

    This enumeration describes the various IGMP options for the IGMP socket
    information class.

Values:

    SocketIgmpOptionInvalid - Indicates an invalid IGMP socket option.

    SocketIgmpOptionJoinMulticastGroup - Indicates a request to join a
        multicast group. This option takes a NET_NETWORK_MULTICAST_REQUEST
        structure.

    SocketIgmpOptionLeaveMulticastGroup - Indicates a request to leave a
        multicast group. This option takes a NET_NETWORK_MULTICAST_REQUEST
        structure.

--*/

typedef enum _SOCKET_IGMP_OPTION {
    SocketIgmpOptionInvalid,
    SocketIgmpOptionJoinMulticastGroup,
    SocketIgmpOptionLeaveMulticastGroup
} SOCKET_IGMP_OPTION, *PSOCKET_IGMP_OPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

