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
// ------------------------------------------------------ Data Type Definitions
//

/*++

Enumeration Description:

    This enumeration describes the various IGMP options for the IGMP socket
    information class.

Values:

    SocketIgmpOptionInvalid - Indicates an invalid IGMP socket option.

    SocketIgmpOptionJoinMulticastGroup - Indicates a request to join a
        multicast group. This option takes a SOCKET_IGMP_MULTICAST_REQUEST
        structure.

    SocketIgmpOptionLeaveMulticastGroup - Indicates a request to leave a
        multicast group. This option takes a SOCKET_IGMP_MULTICAST_REQUEST
        structure.

--*/

typedef enum _SOCKET_IGMP_OPTION {
    SocketIgmpOptionInvalid,
    SocketIgmpOptionJoinMulticastGroup,
    SocketIgmpOptionLeaveMulticastGroup
} SOCKET_IGMP_OPTION, *PSOCKET_IGMP_OPTION;

/*++

Structure Description:

    This structure defines an IGMP request to join or leave a multicast group.

Members:

    Link - Supplies a pointer to the network link associated with the multicast
        group. Requests will send IGMP notifications over this link and update
        the address filters in this link's physical layer.

    LinkAddress - Supplies a pointer to the link address entry with which the
        multicast group is associated.

    MulticastAddress - Supplies the IPv4 multicast group address.

--*/

typedef struct _SOCKET_IGMP_MULTICAST_REQUEST {
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    ULONG MulticastAddress;
} SOCKET_IGMP_MULTICAST_REQUEST, *PSOCKET_IGMP_MULTICAST_REQUEST;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

