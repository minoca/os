/*++

Copyright (c) 2017 Minoca Corp. All Rights Reserved

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ip6addr.h

Abstract:

    This header contains private definitions for well-known IPv6 addresses.

Author:

    Chris Stevens 31-Aug-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store well-known IPv6 addresses.
//

extern const UCHAR NetIp6AllNodesMulticastAddress[IP6_ADDRESS_SIZE];
extern const UCHAR NetIp6AllRoutersMulticastAddress[IP6_ADDRESS_SIZE];
extern const UCHAR NetIp6AllMld2RoutersMulticastAddress[IP6_ADDRESS_SIZE];
extern const UCHAR NetIp6SolicitedNodeMulticastPrefix[IP6_ADDRESS_SIZE];

//
// -------------------------------------------------------- Function Prototypes
//

