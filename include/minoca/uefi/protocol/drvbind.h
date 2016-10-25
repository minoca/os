/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    drvbind.h

Abstract:

    This header contains definitions for the UEFI Driver Binding Protocol.

Author:

    Evan Green 8-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// The global ID for the Driver Binding protocol.
//

#define EFI_DRIVER_BINDING_PROTOCOL_GUID                    \
    {                                                       \
        0x18A031AB, 0xB443, 0x4D1A,                         \
        {0xA5, 0xC0, 0x0C, 0x09, 0x26, 0x1E, 0x9F, 0x71 }   \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_DRIVER_BINDING_PROTOCOL  EFI_DRIVER_BINDING_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_DRIVER_BINDING_SUPPORTED) (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

/*++

Routine Description:

    This routine tests to see if this driver supports a given controller. If a
    child device is provided, it further tests to see if this driver supports
    creating a handle for the specified child device.

    This function checks to see if the driver specified by protocol instance
    ("this") supports the device specified by the controller handle.
    Drivers will typically use the device path attached to the controller
    handle and/or the services from the bus I/O abstraction attached to the
    controller handle to determine if the driver supports the handle. This
    function may be called many times during platform initialization. In order
    to reduce boot times, the tests performed by this function must be very
    small, and take as little time as possible to execute. This function must
    not change the state of any hardware devices, and this function must be
    aware that the device specified by controller handle may already be managed
    by the same driver or a different driver. This function must match its
    calls to AllocatePages with FreePages, AllocatePool with FreePool, and
    OpenProtocol with CloseProtocol. Because ControllerHandle may have been
    previously started by the same driver, if a protocol is already in the
    opened state, then it must not be closed with CloseProtocol. This is
    required to guarantee the state of the controller handle is not modified by
    this function.

Arguments:

    This - Supplies a pointer to the protocol instance, which is the
        instance of the driver binding protocol connected to the driver.

    ControllerHandle - Supplies the handle of the controller to test. The
        handle must support a protocol interface that supplies an I/O
        abstraction to the driver.

    RemainingDevicePath - Supplies an optional pointer to the remaining portion
        of a device path.  This parameter is ignored by device drivers, and is
        optional for bus drivers. For bus drivers, if this parameter is not
        NULL, then the bus driver must determine if the bus controller
        specified by the given handle and the child controller specified
        by the remaining device path are both supported by this bus driver.

Return Value:

    EFI_SUCCESS if the device specified by this controller handle and
    remaining device path are supported by the driver specified by the "This"
    pointer.

    EFI_ALREADY_STARTED if the device specified by the controller handle and
    remaining device path is already been managed by the driver specified by
    "This".

    EFI_ACCESS_DENIED if the device specified by the controller handle and
    remaining device path is already being managed by a different driver or an
    application that requires exclusive access. Currently not implemented.

    EFI_UNSUPPORTED if the device specified by the controller handle and
    remaining device path is not supported by the driver specified by the
    "This" pointer.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_DRIVER_BINDING_START) (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

/*++

Routine Description:

    This routine starts a device controller or a bus controller. This function
    is designed to be invoked from the EFI boot service ConnectController.
    As a result, much of the error checking on the parameters to this routine
    has been moved into this common boot service. It is legal to call this
    routine from other locations, but the following calling restrictions must
    be followed, or the system behavior will not be deterministic.
    1. The controller handle must be a valid EFI_HANDLE.
    2. If the remaining device path is not NULL, then it must be a pointer to a
       naturally aligned EFI_DEVICE_PATH_PROTOCOL.
    3. Prior to calling Start, the Supported function for the driver specified
       by This must have been called with the same calling parameters, and it
       must have returned EFI_SUCCESS.

Arguments:

    This - Supplies a pointer to the driver binding protocol instance.

    ControllerHandle - Supplies the handle of the controller to start. This
        handle must support a protocol interface that supplies an I/O
        abstraction to the driver.

    RemainingDevicePath - Supplies an optional pointer to the remaining
        portion of a device path. This parameter is ignored by device drivers,
        and is optional for bus drivers. For a bus driver, if this parameter is
        NULL, then handles for all the children of the controller are created by
        this driver. If this parameter is not NULL and the first Device Path
        Node is not the End of Device Path Node, then only the handle for the
        child device specified by the first Device Path Node of the remaining
        device path is created by this driver. If the first Device Path Node of
        the remaining device path is the End of Device Path Node, no child
        handle is created by this driver.

Return Value:

    EFI_SUCCESS if the device was started.

    EFI_DEVICE_ERROR if the device could not be started due to a device error.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    Other error codes if the driver failed to start the device.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_DRIVER_BINDING_STOP) (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    UINTN NumberOfChildren,
    EFI_HANDLE *ChildHandleBuffer
    );

/*++

Routine Description:

    This routine stops a device controller or bus controller. This function is
    designed to be invoked from the EFI boot service DisconnectController.
    As a result, much of the error checking on the parameters to Stop has been
    moved into this common boot service. It is legal to call Stop from other
    locations, but the following calling restrictions must be followed, or the
    system behavior will not be deterministic.
    1. The controller handle must be a valid EFI_HANDLE that was used on a
       previous call to this same driver's Start function.
    2. The first "number of children" handles of the child handle buffer must
       all be a valid EFI_HANDLEs. In addition, all of these handles must have
       been created in this driver's Start function, and the Start function
       must have called OpenProtocol on the controller handle with an attribute
       of EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER.

Arguments:

    This - Supplies a pointer to the driver binding protocol instance.

    ControllerHandle - Supplies the handle of the device being stopped. The
        handle must support a bus specific I/O protocol for the driver to use
        to stop the device.

    NumberOfChildren - Supplies the number of child devices in the child handle
        buffer.

    ChildHandleBuffer - Supplies an optional array of child device handles to
        be freed. This can be NULL if the number of children specified is zero.

Return Value:

    EFI_SUCCESS if the device was stopped.

    EFI_DEVICE_ERROR if the device could not be stopped due to a device error.

--*/

/*++

Structure Description:

    This structure defines the Driver Binding Protocol. This protocol provides
    the services required to determine if a driver supports a given controller.
    If a controller is supported, then it also provides routines to start and
    stop the controller.

Members:

    Supported - Stores a pointer to a function used to query a driver to
        determine if it can support a given device handle.

    Start - Stores a pointer to a function used to start the device.

    Stop - Stores a pointer to a function used to stop the device.

    Version - Stores the version number of the UEFI driver that produced the
        driver binding protocol. This is used to determine which driver should
        be called first (newest wins). The values of 0x0-0x0f and
        0xfffffff0-0xffffffff are reserved for platform/OEM specific drivers.
        The Version values of 0x10-0xffffffef are reserved for IHV-developed
        drivers.

    ImageHandle - Stores the image handle of the UEFI driver that produced
        this instance of the driver binding protocol.

    DriverBindingHandle - Stores the handle on which this instance of the
        driver binding protocol is installed. In most cases, this is the same
        handle as the image handle. However, for UEFI drivers that produce more
        than one instance of the driver binding protocol, this value may not be
        the same as the image handle.

--*/

struct _EFI_DRIVER_BINDING_PROTOCOL {
    EFI_DRIVER_BINDING_SUPPORTED Supported;
    EFI_DRIVER_BINDING_START Start;
    EFI_DRIVER_BINDING_STOP Stop;
    UINT32 Version;
    EFI_HANDLE ImageHandle;
    EFI_HANDLE DriverBindingHandle;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

