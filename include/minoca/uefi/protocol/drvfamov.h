/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    drvfamov.h

Abstract:

    This header contains definitions for the UEFI Driver Family Override
    Protocol.

Author:

    Evan Green 5-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL_GUID            \
    {                                                       \
        0xB1DD129E, 0xDA36, 0x4181,                         \
        {0x91, 0xF8, 0x04, 0xA4, 0x92, 0x37, 0x66, 0xA7}    \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL
    EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL;

typedef
UINT32
(EFIAPI *EFI_DRIVER_FAMILY_OVERRIDE_GET_VERSION) (
    EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL *This
    );

/*++

Routine Description:

    This routine returns the version value associated with a driver.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    Returns the version value associated with the driver.

--*/

/*++

Structure Description:

    This structure defines the Driver Family Override Protocol. When installed,
    it produces a GUID that represents a family of driver. Drivers with the
    same GUID are members of the same family. When drivers are connected to
    controllers, drivers with a higher revision value in the same driver
    family are connected with a higher priority than drivers with a lower
    revision value in the same driver family. The EFI Boot Service Connect
    Controller uses five rules to build a prioritized list of drivers
    when a request is made to connect a driver to a controller. The Driver
    Family Protocol rule is below the Platform Specific Driver Override
    Protocol and the above the Bus Specific Driver Override Protocol.

Members:

    GetVersion - Supplies a pointer to a function used to get the driver
        version.

--*/

struct _EFI_DRIVER_FAMILY_OVERRIDE_PROTOCOL {
    EFI_DRIVER_FAMILY_OVERRIDE_GET_VERSION GetVersion;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
