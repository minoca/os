/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    net8022.h

Abstract:

    This header contains definitions for the IEEE 802.2 Logical Link Layer.

Author:

    Chris Stevens 22-Oct-2015

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
// Define the set of SAP addresses.
//

#define NET8022_SAP_ADDRESS_SNAP_EXTENSION 0xAA

//
// Define the bits for the LLC header's control field.
//

#define NET8022_CONTROL_TYPE_MASK       0x03
#define NET8022_CONTROL_TYPE_SHIFT      0
#define NET8022_CONTROL_TYPE_UNNUMBERED 0x3

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the IEEE 802.2 logical link layer header.

Members:

    DestinationSapAddress - Stores the destination SAP address for the frame.

    SourceSapAddress - Stores the source SAP address for the frame.

    Control - Stores a bitmask of bits that describe the frame. See
        NET8022_CONTROL_* for definitions.

--*/

#pragma pack(push, 1)

typedef struct _NET8022_LLC_HEADER {
    UCHAR DestinationSapAddress;
    UCHAR SourceSapAddress;
    UCHAR Control;
} PACKED NET8022_LLC_HEADER, *PNET8022_LLC_HEADER;

/*++

Structure Description:

    This structure defines the 802.2 SNAP extension.

Members:

    OrganizationCode - Stores the 24-bit organization code for the frame.

    EthernetType - Stores the frame type. Values are taken from the IEEE 802.3
        Ethernet standard.

--*/

typedef struct _NET8022_SNAP_EXTENSION {
    UCHAR OrganizationCode[3];
    USHORT EthernetType;
} PACKED NET8022_SNAP_EXTENSION, *PNET8022_SNAP_EXTENSION;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

