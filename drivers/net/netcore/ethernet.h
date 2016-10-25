/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ethernet.h

Abstract:

    This header contains definitions for Ethernet links.

Author:

    Evan Green 5-Apr-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of an ethernet header and footer.
//

#define ETHERNET_HEADER_SIZE ((2 * ETHERNET_ADDRESS_SIZE) + sizeof(USHORT))
#define ETHERNET_FOOTER_SIZE sizeof(ULONG)

//
// Define the minimum and maximum valid ethernet payload size. This does not
// include the header or footer.
//

#define ETHERNET_MINIMUM_PAYLOAD_SIZE 46
#define ETHERNET_MAXIMUM_PAYLOAD_SIZE 1500

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

