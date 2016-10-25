/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    drvplato.h

Abstract:

    This header contains definitions for the UEFI Platform Driver Override
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

#define EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL_GUID          \
    {                                                       \
        0x6B30C738, 0xA391, 0x11D4,                         \
        {0x9A, 0x3B, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}    \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL
    EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_PLATFORM_DRIVER_OVERRIDE_GET_DRIVER) (
    EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE *DriverImageHandle
    );

/*++

Routine Description:

    This routine retrieves the image handle of the platform override driver
    for a controller in the system.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ControllerHandle - Supplies the device handle of the controller to check
        for a driver override.

    DriverImageHandle - Supplies a pointer that on input contains a pointer to
        the previous driver image handle returned by GetDriver. On output,
        returns a pointer to the next driver image handle.

Return Value:

    EFI_SUCCESS if the driver override for the given controller handle was
    returned.

    EFI_NOT_FOUND if a driver override for the given controller was not found.

    EFI_INVALID_PARAMETER if the controller handle was NULL or the driver image
    handle is not a handle that was returned by a previous call to GetDriver.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_PLATFORM_DRIVER_OVERRIDE_GET_DRIVER_PATH) (
    EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL **DriverImagePath
    );

/*++

Routine Description:

    This routine retrieves the device path of the platform override driver
    for a controller in the system.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ControllerHandle - Supplies the device handle of the controller to check
        for a driver override.

    DriverImagePath - Supplies a pointer that on input contains a pointer to
        the previous device path returned by GetDriverPath. On output,
        returns a pointer to the next driver device path. Passing a pointer to
        NULL will return the first driver device path for the controller handle.

Return Value:

    EFI_SUCCESS if the driver override for the given controller handle was
    returned.

    EFI_UNSUPPORTED if the operation is not supported.

    EFI_NOT_FOUND if a driver override for the given controller was not found.

    EFI_INVALID_PARAMETER if the controller handle was NULL or the driver image
    path is not a handle that was returned by a previous call to GetDriverPath.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_PLATFORM_DRIVER_OVERRIDE_DRIVER_LOADED) (
    EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *DriverImagePath,
    EFI_HANDLE DriverImageHandle
    );

/*++

Routine Description:

    This routine is used to associate a driver image handle with a device path
    that was returned on a prior call to the GetDriverPath function. This
    driver image handle will then be available through the GetDriver function.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ControllerHandle - Supplies the device handle of the controller.

    DriverImagePath - Supplies a pointer to the driver device path that was
        returned in a previous call to GetDriverPath.

    DriverImageHandle - Supplies the driver image handle that was returned by
        LoadImage when the driver specified in the driver image path was
        loaded into memory.

Return Value:

    EFI_SUCCESS if the association between the driver image path and driver
    image handle was successfully established for the specified controller.

    EFI_UNSUPPORTED if the operation is not supported.

    EFI_NOT_FOUND if the driver image path is not a device path that was
    returned on a prior call to GetDriverPath for the controller.

    EFI_INVALID_PARAMETER if the controller handle was NULL, the driver image
    path is not valid, or the driver image handle is not valid.

--*/

/*++

Structure Description:

    This structure defines the Platform Driver Override Protocol. This protocol
    matches one or more drivers to a controller. A platform driver produces
    this protocol, and it is installed on a separate handle. This protocol
    is used by the ConnectController() boot service to select the best driver
    for a controller. All of the drivers returned by this protocol have a
    higher precedence than drivers found from an EFI Bus Specific Driver
    Override Protocol or drivers found from the general UEFI driver Binding
    search algorithm. If more than one driver is returned by this protocol,
    then the drivers are returned in order from highest precedence to lowest
    precedence.

Members:

    GetDriver - Stores a pointer to a function used to get an override driver
        for a controller.

    GetDriverPath - Stores a pointer to a function used to get a device path
        for an override driver.

    DriverLoaded - Stores a pointer to a function used to associate a loaded
        driver with a driver path returned by GetDriverPath.

--*/

struct _EFI_PLATFORM_DRIVER_OVERRIDE_PROTOCOL {
    EFI_PLATFORM_DRIVER_OVERRIDE_GET_DRIVER GetDriver;
    EFI_PLATFORM_DRIVER_OVERRIDE_GET_DRIVER_PATH GetDriverPath;
    EFI_PLATFORM_DRIVER_OVERRIDE_DRIVER_LOADED DriverLoaded;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
