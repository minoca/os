/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdusbp.h

Abstract:

    This header contains internal definitions for the kernel debugger USB
    support module.

Author:

    Evan Green 2-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/kdusb.h>
#include <minoca/usb/usb.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _KD_USB_DEVICE KD_USB_DEVICE, *PKD_USB_DEVICE;

/*++

Structure Description:

    This structure stores the information about a USB debug device.

Members:

    Controller - Stores a pointer to the host controller device.

    DeviceAddress - Stores the address of the device.

    Configuration - Stores the configuration value the device is set in.

    VendorId - Stores the vendor ID of the device.

    ProductId - Stores the product ID of the device.

    PortCount - Stores the number of ports this hub has if this is a hub
        device. For non-hub devices, this is zero.

    InterfaceNumber - Store the hub interface number if this is a hub.

    Hub - Stores a pointer to the hub this device sits off of.

    HubPortNumber - Stores the one-based port number on the parent hub of this
        device.

    Speed - Stores the speed of the device.

    EndpointZero - Stores information about the default endpoint.

--*/

struct _KD_USB_DEVICE {
    PHARDWARE_USB_DEBUG_DEVICE Controller;
    UCHAR DeviceAddress;
    UCHAR Configuration;
    USHORT VendorId;
    USHORT ProductId;
    ULONG PortCount;
    UCHAR InterfaceNumber;
    PKD_USB_DEVICE Hub;
    ULONG HubPortNumber;
    DEBUG_USB_DEVICE_SPEED Speed;
    DEBUG_USB_ENDPOINT EndpointZero;
};

typedef
KSTATUS
(*PKD_USB_DRIVER_ENTRY) (
    PKD_USB_DEVICE Device,
    PDEBUG_DEVICE_DESCRIPTION Interface
    );

/*++

Routine Description:

    This routine implements the entry point into a specific KD USB device
    driver.

Arguments:

    Device - Supplies a pointer to the device the driver lives on.

    Interface - Supplies a pointer where the driver fills in the I/O
        interface on success.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines the mapping between a USB Vendor/Product ID and
    a KD USB driver.

Members:

    VendorId - Stores the vendor ID the driver supports.

    ProductId - Stores the product ID the driver supports.

    DriverEntry - Stores a pointer to the function called when that device is
        found.

--*/

typedef struct _KD_USB_DRIVER_MAPPING {
    USHORT VendorId;
    USHORT ProductId;
    PKD_USB_DRIVER_ENTRY DriverEntry;
} KD_USB_DRIVER_MAPPING, *PKD_USB_DRIVER_MAPPING;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
KdpUsbDefaultControlTransfer (
    PKD_USB_DEVICE Device,
    PUSB_SETUP_PACKET Setup,
    DEBUG_USB_TRANSFER_DIRECTION Direction,
    PVOID Buffer,
    PULONG BufferSize
    );

/*++

Routine Description:

    This routine performs a control transfer to endpoint zero of the given
    device.

Arguments:

    Device - Supplies a pointer to the target of the transfer.

    Setup - Supplies a pointer to the initialized setup packet.

    Direction - Supplies the transfer direction.

    Buffer - Supplies the transfer buffer.

    BufferSize - Supplies an optional pointer that upon input contains the size
        of the buffer in bytes. On output returns the number of bytes actually
        transferred.

Return Value:

    Status code.

--*/

KSTATUS
KdpUsbControlTransfer (
    PKD_USB_DEVICE Device,
    PDEBUG_USB_ENDPOINT Endpoint,
    PUSB_SETUP_PACKET Setup,
    DEBUG_USB_TRANSFER_DIRECTION Direction,
    PVOID Buffer,
    PULONG BufferSize
    );

/*++

Routine Description:

    This routine performs a control transfer to the given endpoint of the
    given device.

Arguments:

    Device - Supplies a pointer to the target of the transfer.

    Endpoint - Supplies a pointer to the endpoint.

    Setup - Supplies a pointer to the initialized setup packet.

    Direction - Supplies the transfer direction.

    Buffer - Supplies the transfer buffer.

    BufferSize - Supplies an optional pointer that upon input contains the size
        of the buffer in bytes. On output returns the number of bytes actually
        transferred.

Return Value:

    Status code.

--*/

KSTATUS
KdpUsbSetupTransfer (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    PDEBUG_USB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine allocates a buffer and initializes the given USB transfer.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Transfer - Supplies a pointer to the transfer to initialize to. The caller
        must have filled out the endpoint, direction, and length members. This
        routine will allocate buffer space for the transfer data.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not filled out correctly.

    STATUS_INSUFFICIENT_RESOURCES if a buffer for the transfer could not be
    allocated.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

KSTATUS
KdpUsbSubmitTransfer (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    PDEBUG_USB_TRANSFER Transfer,
    BOOL WaitForCompletion
    );

/*++

Routine Description:

    This routine submits a previously set up USB transfer.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Transfer - Supplies a pointer to the transfer to submit.

    WaitForCompletion - Supplies a boolean indicating if the routine should
        block until the transfer completes.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

    STATUS_TIMEOUT if the caller wanted to wait for the transfer but it
    never completed.

--*/

KSTATUS
KdpUsbCheckTransfer (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    PDEBUG_USB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine checks on the completion status of a transfer.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Transfer - Supplies a pointer to the transfer to check.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

    STATUS_MORE_PROCESSING_REQUIRED if the transfer is still in progress.

--*/

KSTATUS
KdpUsbRetireTransfer (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    PDEBUG_USB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine retires a USB transfer. This frees the buffer allocated
    during setup.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Transfer - Supplies a pointer to the transfer to retire.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

KSTATUS
KdpUsbStall (
    PHARDWARE_USB_DEBUG_DEVICE Device,
    ULONG Milliseconds
    );

/*++

Routine Description:

    This routine stalls execution for the given duration.

Arguments:

    Device - Supplies a pointer to the hardware debug device.

    Milliseconds - Supplies the number of milliseconds to stall for.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

KSTATUS
KdpUsbInitializeEndpoint (
    PKD_USB_DEVICE Device,
    PUSB_ENDPOINT_DESCRIPTOR Descriptor,
    PDEBUG_USB_ENDPOINT Endpoint
    );

/*++

Routine Description:

    This routine initializes an endpoint based on a given descriptor.

Arguments:

    Device - Supplies a pointer to the KD USB device.

    Descriptor - Supplies a pointer to the endpoint descriptor.

    Endpoint - Supplies a pointer to the endpoint to initialize.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the device or descriptor was invalid.

--*/

//
// Hub functions
//

KSTATUS
KdpUsbHubReset (
    PKD_USB_DEVICE Device
    );

/*++

Routine Description:

    This routine resets a USB hub.

Arguments:

    Device - Supplies a pointer to the USB device.

Return Value:

    Status code.

--*/

KSTATUS
KdpUsbHubGetStatus (
    PKD_USB_DEVICE Device,
    ULONG PortNumber,
    PULONG PortStatus
    );

/*++

Routine Description:

    This routine queries a USB hub for a port status.

Arguments:

    Device - Supplies a pointer to the USB hub device.

    PortNumber - Supplies the one-based port number to query.

    PortStatus - Supplies a pointer where the port status is returned.

Return Value:

    Status code.

--*/

KSTATUS
KdpUsbHubSetStatus (
    PKD_USB_DEVICE Device,
    ULONG PortNumber,
    ULONG PortStatus
    );

/*++

Routine Description:

    This routine sets the port status on a USB hub.

Arguments:

    Device - Supplies a pointer to the USB hub device.

    PortNumber - Supplies the one-based port number to query.

    PortStatus - Supplies the port status to set.

Return Value:

    Status code.

--*/

//
// KD USB Driver Entry routines.
//

KSTATUS
KdpFtdiDriverEntry (
    PKD_USB_DEVICE Device,
    PDEBUG_DEVICE_DESCRIPTION Interface
    );

/*++

Routine Description:

    This routine initializes an FTDI USB to Serial KD USB device.

Arguments:

    Device - Supplies a pointer to the device the driver lives on.

    Interface - Supplies a pointer where the driver fills in the I/O
        interface on success.

Return Value:

    Status code.

--*/

