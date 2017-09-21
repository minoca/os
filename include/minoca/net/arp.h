/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    arp.h

Abstract:

    This header contains public definitions for the ARP network layer.

Author:

    Chris Stevens 20-Sept-2017

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

    This enumeration describes the various ARP options for the ARP socket
    information class.

Values:

    SocketArpOptionInvalid - Indicates an invalid ARP socket option.

    SocketArpOptionTranslateAddress - Indicates a request to translate a
        network address into a physical layer address. This option takes and
        returns a NET_TRANSLATION_REQUEST structure.

--*/

typedef enum _SOCKET_ARP_OPTION {
    SocketArpOptionInvalid,
    SocketArpOptionTranslateAddress,
} SOCKET_ARP_OPTION, *PSOCKET_ARP_OPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

