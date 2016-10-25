/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbhost.h

Abstract:

    This header contains USB Core library support for host controllers.

Author:

    Evan Green 15-Jan-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/usb/usb.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the current version of the USB host controller interface.
//

#define USB_HOST_CONTROLLER_INTERFACE_VERSION 1

//
// Define the current version of the USB endpoint creation request structure.
//

#define USB_HOST_ENDPOINT_CREATION_REQUEST_VERSION 1

//
// Define the standard USB PID values.
//

#define USB_PID_OUT   0xE1
#define USB_PID_IN    0x69
#define USB_PID_SOF   0xA5
#define USB_PID_SETUP 0x2D
#define USB_PID_DATA0 0xC3
#define USB_PID_DATA1 0x4B
#define USB_PID_DATA2 0x87
#define USB_PID_MDATA 0x0F
#define USB_PID_ACK   0xD2
#define USB_PID_NAK   0x5A
#define USB_PID_STALL 0x1E
#define USB_PID_NYET  0x96
#define USB_PID_PRE   0x3C
#define USB_PID_ERR   0x3C
#define USB_PID_SPLIT 0x78
#define USB_PID_PING  0xB4

//
// Define USB port status bits. These do not correspond directly to any defined
// bits in the USB hub specification.
//

#define USB_PORT_STATUS_CONNECTED    0x0001
#define USB_PORT_STATUS_ENABLED      0x0002
#define USB_PORT_STATUS_SUSPENDED    0x0004
#define USB_PORT_STATUS_OVER_CURRENT 0x0008
#define USB_PORT_STATUS_RESET        0x0010

//
// Define USB port status change bits. These do not correspond directly to any
// defined bits in the USB hub specification. They correspond 1-to-1 with their
// respective status bits.
//

#define USB_PORT_STATUS_CHANGE_CONNECTED USB_PORT_STATUS_CONNECTED
#define USB_PORT_STATUS_CHANGE_ENABLED USB_PORT_STATUS_ENABLED
#define USB_PORT_STATUS_CHANGE_SUSPENDED USB_PORT_STATUS_SUSPENDED
#define USB_PORT_STATUS_CHANGE_OVER_CURRENT USB_PORT_STATUS_OVER_CURRENT
#define USB_PORT_STATUS_CHANGE_RESET USB_PORT_STATUS_RESET

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _USB_HUB USB_HUB, *PUSB_HUB;

/*++

Structure Description:

    This structure stores port status information.

Members:

    Status - Stores a bitmaks of the current port status. See USB_PORT_STATUS_*
        definitions.

    Change - Stores a bitmaks of the port status bits that have changed and are
        yet to be handled. See USB_PORT_STATUS_CHANGE_* definitions.

--*/

typedef struct _USB_PORT_STATUS {
    USHORT Status;
    USHORT Change;
} USB_PORT_STATUS, *PUSB_PORT_STATUS;

/*++

Structure Description:

    This structure stores status information for each of the ports in a USB
    hub.

Members:

    PortStatus - Stores an array of port status structures. One for each port.

    PortDeviceSpeed - Stores an array containing the speed of the device
        connected at each port. This value is ignored if no device is connected
        to the port.

--*/

typedef struct _USB_HUB_STATUS {
    PUSB_PORT_STATUS PortStatus;
    PUSB_DEVICE_SPEED PortDeviceSpeed;
} USB_HUB_STATUS, *PUSB_HUB_STATUS;

/*++

Structure Description:

    This structure stores information passed to a USB host controller when an
    endpoint is being created.

Members:

    Version - Stores the version of this structure.

    Type - Stores the type of endpoint being created.

    Direction - Stores the direction of the endpoint being created.

    Speed - Stores the speed of the device the endpoint is being created for.

    MaxPacketSize - Stores the maximum number of payload bytes that can be
        moved per transfer.

    PollRate - Stores the poll rate, in (micro)frames.

    EndpointNumber - Stores the endpoint number of the endpoint, as defined by
        the USB device.

    HubAddress - Stores the address of the device's parent hub, required for
        full or low speed devices on a high speed bus. This field will contain
        0 for root hub enumerated devices.

    HubPortNumber - Stores the port number this devices is connected to on the
        parent hub. This field will be zero for root hub enumerated devices.

--*/

typedef struct _USB_HOST_ENDPOINT_CREATION_REQUEST {
    ULONG Version;
    USB_TRANSFER_TYPE Type;
    USB_TRANSFER_DIRECTION Direction;
    USB_DEVICE_SPEED Speed;
    ULONG MaxPacketSize;
    USHORT PollRate;
    UCHAR EndpointNumber;
    UCHAR HubAddress;
    UCHAR HubPortNumber;
} USB_HOST_ENDPOINT_CREATION_REQUEST, *PUSB_HOST_ENDPOINT_CREATION_REQUEST;

/*++

Structure Description:

    This structure stores information about a USB transfer.

Members:

    Public - Stores the public portion of the transfer, which is available to
        all users of the USB core.

    DeviceAddress - Stores the device address where the transfer is pointed.

    EndpointNumber - Stores the endpoint number of the endpoint this transfer
        is aimed at.

    Type - Stores the type of USB request that this transfer is.

--*/

typedef struct _USB_TRANSFER_INTERNAL {
    USB_TRANSFER Public;
    UCHAR DeviceAddress;
    UCHAR EndpointNumber;
    USB_TRANSFER_TYPE Type;
} USB_TRANSFER_INTERNAL, *PUSB_TRANSFER_INTERNAL;

//
// Host controller functions.
//

typedef
KSTATUS
(*PUSB_HOST_CREATE_ENDPOINT) (
    PVOID HostControllerContext,
    PUSB_HOST_ENDPOINT_CREATION_REQUEST Endpoint,
    PVOID *EndpointContext
    );

/*++

Routine Description:

    This routine is called by the USB core when a new endpoint is being opened.
    It allows the host controller to create and store any context needed to
    support a new endpoint (such as a queue head).

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    Endpoint - Supplies a pointer containing information about the endpoint
        being created. The host controller cannot count on this buffer sticking
        around after the function returns. If it needs this information it
        should make a copy of it.

    EndpointContext - Supplies a pointer where the host controller can store a
        context pointer identifying the endpoint created.

Return Value:

    STATUS_SUCCESS if the endpoint can be successfully accommodated.

    Failing status code if the endpoint cannot be opened.

--*/

typedef
VOID
(*PUSB_HOST_RESET_ENDPOINT) (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxPacketSize
    );

/*++

Routine Description:

    This routine is called by the USB core when an endpoint needs to be reset.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

    MaxPacketSize - Supplies the maximum packet size of the endpoint, which
        may have changed in the case of endpoint zero.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PUSB_HOST_FLUSH_ENDPOINT) (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PULONG TransferCount
    );

/*++

Routine Description:

    This routine flushes all the active transfers from an endpoint. It does so
    by polling for completion status and does not return until all transfers
    are completed. This must be called at high run level.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

    TransferCount - Supplies a pointer to a boolean that receives the number
        of transfers that were flushed.

Return Value:

    Status code.

--*/

typedef
VOID
(*PUSB_HOST_DESTROY_ENDPOINT) (
    PVOID HostControllerContext,
    PVOID EndpointContext
    );

/*++

Routine Description:

    This routine tears down and destroys an endpoint created with the endpoint
    creation routine.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PUSB_HOST_CREATE_TRANSFER) (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxBufferSize,
    ULONG Flags,
    PVOID *TransferContext
    );

/*++

Routine Description:

    This routine allocates structures needed for the USB host controller to
    support a transfer.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the host controller's context of
        the endpoint that this transfer will eventually be submitted to.

    MaxBufferSize - Supplies the maximum buffer length, in bytes, of the
        transfer when it is submitted. It is assumed that the host controller
        will set up as many transfer descriptors as are needed to support a
        transfer of this size.

    Flags - Supplies a bitfield of flags regarding the transaction. See
        USB_TRANSFER_FLAG_* definitions.

    TransferContext - Supplies a pointer where the host controller can store a
        context pointer containing any needed structures for the transfer.

Return Value:

    None.

--*/

typedef
VOID
(*PUSB_HOST_DESTROY_TRANSFER) (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PVOID TransferContext
    );

/*++

Routine Description:

    This routine destroys host controller structures associated with a USB
    transfer.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the host controller context for the
        endpoint this transfer belonged to.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PUSB_HOST_SUBMIT_TRANSFER) (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

/*++

Routine Description:

    This routine submits a transfer to the USB host controller for execution.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies the context pointer provided to the USB core by
        the host controller when the endpoint was created.

    Transfer - Supplies a pointer to the USB transfer to execute.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    STATUS_SUCCESS if the transfer was successfully added to the hardware queue.

    Failure codes if the transfer could not be added.

--*/

typedef
KSTATUS
(*PUSB_HOST_CANCEL_TRANSFER) (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

/*++

Routine Description:

    This routine submits attempts to cancel a transfer that was previously
    submitted for execution.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies the context pointer provided to the USB core by
        the host controller when the endpoint was created.

    Transfer - Supplies a pointer to the USB transfer to execute.

    TransferContext - Supplies the pointer provided to the USB core by the host
        controller when the transfer was created.

Return Value:

    STATUS_SUCCESS if the transfer was successfully removed from the hardware
    queue.

    STATUS_TOO_LATE if the transfer had already completed.

    Other failure codes if the transfer could not be cancelled but has not yet
    completed.

--*/

typedef
KSTATUS
(*PUSB_HOST_GET_ROOT_HUB_STATUS) (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    );

/*++

Routine Description:

    This routine queries the host controller for the status of the root hub.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    HubStatus - Supplies a pointer where the host controller should fill out
        the root hub status.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    Failure codes if the status could not be queried.

--*/

typedef
KSTATUS
(*PUSB_HOST_SET_ROOT_HUB_STATUS) (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    );

/*++

Routine Description:

    This routine sets the state of the root hub in the USB host controller. It
    looks at the status change bits for each port in order to determine what
    needs to be set.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    HubStatus - Supplies a pointer to the status that should be set in the root
        hub.

Return Value:

    STATUS_SUCCESS if the hub state was successfully programmed into the device.

    Failure codes if the status could not be set.

--*/

/*++

Structure Description:

    This structure stores an interface of functions that the USB core will use
    to call into a specific host controller driver instance.

Members:

    Version - Stores the USB controller interface version number.

    DriverObject - Stores a pointer to the host controller's driver object,
        which is used to create child devices on its behalf.

    DeviceObject - Stores a pointer to the host controller's device object,
        which is used to create child devices on its behalf.

    HostControllerContext - Stores a pointer's worth of context for the USB
        host controller. The USB core library will pass this context pointer
        to the host controller when calling its interface functions.

    Identifier - Stores a unique identifier used to match against the KD debug
        handoff data. Often this is the base physical address of the controller.

    DebugPortSubType - Stores the host controller type as defined by the Debug
        Port Table 2. Set to -1 if the controller isn't defined in the Debug
        Port Table 2.

    Speed - Stores the maximum supporte speed of the controller.

    RootHubPortCount - Stores the number of ports on the root hub of this
        controller.

    CreateEndpoint - Stores a pointer to a function that the USB core library
        calls when an endpoint is being prepared for use.

    ResetEndpoint - Stores a pointer to a function that the USB core library
        calls to reset an endpoint.

    FlushEndpoint - Stores a pointer to a function that the USB core library
        calls to flush transfers from an endpoint. This routine is required if
        polled I/O is supported.

    DestroyEndpoint - Stores a pointer to a function that the USB core library
        calls to destroy an endpoint.

    CreateTransfer - Stores a pointer to a function that the USB core library
        calls to create a new transfer.

    DestroyTransfer - Stores a pointer to a function that the USB core library
        calls to destroy a USB transfer.

    SubmitTransfer - Stores a pointer to a function that the USB core library
        calls to submit a USB transfer for execution.

    SubmitPolledTransfer - Stores a pointer to a function that the USB core
        library calls to submit a USB transfer for polled I/O execution.

    CancelTransfer - Stores a pointer to a function that the USB core library
        calls to cancel a submitted transfer.

    GetRootHubStatus - Stores a pointer to a function that the USB core library
        calls to get the current state of the root hub.

    SetRootHubStatus - Stores a pointer to a function that the USB core library
        calls to set the current state of the root hub.

--*/

typedef struct _USB_HOST_CONTROLLER_INTERFACE {
    ULONG Version;
    PDRIVER DriverObject;
    PDEVICE DeviceObject;
    PVOID HostControllerContext;
    ULONGLONG Identifier;
    USHORT DebugPortSubType;
    USB_DEVICE_SPEED Speed;
    ULONG RootHubPortCount;
    PUSB_HOST_CREATE_ENDPOINT CreateEndpoint;
    PUSB_HOST_RESET_ENDPOINT ResetEndpoint;
    PUSB_HOST_FLUSH_ENDPOINT FlushEndpoint;
    PUSB_HOST_DESTROY_ENDPOINT DestroyEndpoint;
    PUSB_HOST_CREATE_TRANSFER CreateTransfer;
    PUSB_HOST_DESTROY_TRANSFER DestroyTransfer;
    PUSB_HOST_SUBMIT_TRANSFER SubmitTransfer;
    PUSB_HOST_SUBMIT_TRANSFER SubmitPolledTransfer;
    PUSB_HOST_CANCEL_TRANSFER CancelTransfer;
    PUSB_HOST_GET_ROOT_HUB_STATUS GetRootHubStatus;
    PUSB_HOST_SET_ROOT_HUB_STATUS SetRootHubStatus;
} USB_HOST_CONTROLLER_INTERFACE, *PUSB_HOST_CONTROLLER_INTERFACE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

USB_API
KSTATUS
UsbHostRegisterController (
    PUSB_HOST_CONTROLLER_INTERFACE ControllerInterface,
    PHANDLE ControllerHandle
    );

/*++

Routine Description:

    This routine registers a new host controller instance with the USB core.
    This routine must be called at low level.

Arguments:

    ControllerInterface - Supplies a pointer to the interface that the USB
        core will use to call back into the host controller. This contents of
        this memory will be copied during this call, so the caller can pass
        a pointer to a stack-allocated buffer here.

    ControllerHandle - Supplies a pointer where a handle will be returned
        representing this controller instance. When calls are made to the USB
        core regarding a specific controller, pass this handle.

Return Value:

    STATUS_SUCCESS on success. A handle will also be returned on success.

    STATUS_NOT_SUPPORTED if an unsupported version was supplied with the
    controller interface.

    Other error codes on other failures.

--*/

USB_API
VOID
UsbHostDestroyControllerState (
    HANDLE ControllerHandle
    );

/*++

Routine Description:

    This routine destroys the state of a USB host controller that was created
    during registration.

Arguments:

    ControllerHandle - Supplies a handle to a controller instance.

Return Value:

    None.

--*/

USB_API
VOID
UsbHostProcessCompletedTransfer (
    PUSB_TRANSFER_INTERNAL Transfer
    );

/*++

Routine Description:

    This routine is called by the USB host controller when the host controller
    is done with a transfer. This routine must be called if the transfer is
    completed successfully, failed, or was cancelled.

    This routine must be called at dispatch level or less.

Arguments:

    Transfer - Supplies a pointer to the transfer that has completed.

Return Value:

    None.

--*/

USB_API
VOID
UsbHostNotifyPortChange (
    HANDLE ControllerHandle
    );

/*++

Routine Description:

    This routine notifies the USB core that the USB host controller detected a
    port change.

Arguments:

    ControllerHandle - Supplies a handle to the USB core instance that needs to
        be notified that a host port changed status.

Return Value:

    None.

--*/

USB_API
KSTATUS
UsbHostQueryChildren (
    PIRP Irp,
    HANDLE UsbDeviceHandle
    );

/*++

Routine Description:

    This routine responds to the Query Children IRP for a USB Host controller.

Arguments:

    Irp - Supplies a pointer to the Query Children IRP.

    UsbDeviceHandle - Supplies a pointer to the USB Host controller handle.

Return Value:

    Status code.

--*/

USB_API
KSTATUS
UsbCreateHub (
    HANDLE DeviceHandle,
    PUSB_HUB *Hub
    );

/*++

Routine Description:

    This routine creates a new USB hub device. This routine must be called at
    low level.

Arguments:

    DeviceHandle - Supplies the open device handle to the hub.

    Hub - Supplies a pointer where a pointer to the hub context will be
        returned.

Return Value:

    Status code.

--*/

USB_API
VOID
UsbDestroyHub (
    PUSB_HUB Hub
    );

/*++

Routine Description:

    This routine destroys a USB hub context. This should only be called once
    all of the hub's transfers have completed.

Arguments:

    Hub - Supplies a pointer to the hub to tear down.

Return Value:

    None.

--*/

USB_API
KSTATUS
UsbStartHub (
    PUSB_HUB Hub
    );

/*++

Routine Description:

    This routine starts a USB hub.

Arguments:

    Hub - Supplies a pointer to the hub to start.

Return Value:

    None.

--*/

USB_API
KSTATUS
UsbHubQueryChildren (
    PIRP Irp,
    PUSB_HUB Hub
    );

/*++

Routine Description:

    This routine responds to the Query Children IRP for a USB Hub. This routine
    must be called at low level.

Arguments:

    Irp - Supplies a pointer to the Query Children IRP.

    Hub - Supplies a pointer to the hub to query.

Return Value:

    Status code.

--*/

