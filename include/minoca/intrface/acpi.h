/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpi.h

Abstract:

    This header contains ACPI device interfaces.

Author:

    Evan Green 19-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Interface UUID for getting the ACPI bus address.
//

#define UUID_ACPI_BUS_ADDRESS \
    {{0x73696D6F, 0x74207365, 0x656B206F, 0x61207065}}

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the interface for returning a device's ACPI bus
    address.

Members:

    BusAddress - Stores the bus address of the device in the ACPI _ADR format.
        This format is bus-specific to the types of buses that ACPI knows about.

--*/

typedef struct _INTERFACE_ACPI_BUS_ADDRESS {
    ULONGLONG BusAddress;
} INTERFACE_ACPI_BUS_ADDRESS, *PINTERFACE_ACPI_BUS_ADDRESS;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

