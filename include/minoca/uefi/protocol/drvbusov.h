/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    drvbusov.h

Abstract:

    This header contains definitions for the UEFI Bus Specific Driver Override
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

#define EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL_GUID      \
    {                                                       \
        0x3BC1B285, 0x8A15, 0x4A82,                         \
        {0xAA, 0xBF, 0x4D, 0x7D, 0x13, 0xFB, 0x32, 0x65}    \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL
    EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_GET_DRIVER) (
    EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL *This,
    EFI_HANDLE *DriverImageHandle
    );

/*++

Routine Description:

    This routine uses a bus specific algorithm to retrieve a driver image
    handle for a controller.

Arguments:

    This - Supplies a pointer to the protocol instance.

    DriverImageHandle - Supplies a pointer that on input contains the previous
        driver handle returned. On output, returns a pointer to the next
        driver image handle. Passing in NULL will return the first driver image
        handle.

Return Value:

    EFI_SUCCESS if a bus specific override driver is returned in the driver
    image handle.

    EFI_NOT_FOUND if the end of the list of override drivers was reached.

    EFI_INVALID_PARAMETER if the driver image handle is not a handle that was
    returned on a previous call to GetDriver.

--*/

/*++

Structure Description:

    This structure defines the Bus Specific Driver Override Protocol. This
    protocol matches one or more drivers to a controller. This protocol is
    produced by a bus driver and it is instlaled on the child handles of
    buses tha require a bus specific algorithm for matching drivers to
    controllers.

Members:

    GetDriver - Supplies a pointer to a function used to get the driver image
        handle for a given controller handle.

--*/

struct _EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_PROTOCOL {
    EFI_BUS_SPECIFIC_DRIVER_OVERRIDE_GET_DRIVER GetDriver;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
