/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdusb.h

Abstract:

    This header contains definitions for USB debug devices.

Author:

    Evan Green 26-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define DEBUG_USB_HOST_DESCRIPTION_VERSION 1

//
// Define USB port status bits.
//

#define DEBUG_USB_PORT_STATUS_CONNECTED    0x00000001
#define DEBUG_USB_PORT_STATUS_ENABLED      0x00000002
#define DEBUG_USB_PORT_STATUS_SUSPENDED    0x00000004
#define DEBUG_USB_PORT_STATUS_OVER_CURRENT 0x00000008
#define DEBUG_USB_PORT_STATUS_RESET        0x00000010
#define DEBUG_USB_PORT_STATUS_LOW_SPEED    0x00000100
#define DEBUG_USB_PORT_STATUS_FULL_SPEED   0x00000200
#define DEBUG_USB_PORT_STATUS_HIGH_SPEED   0x00000400
#define DEBUG_USB_PORT_STATUS_SUPER_SPEED  0x00000800

#define DEBUG_USB_SETUP_PACKET_SIZE 8
#define DEBUG_USB_ENDPOINT_ADDRESS_MASK 0x0F

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DEBUG_USB_DEVICE_SPEED {
    DebugUsbDeviceSpeedInvalid,
    DebugUsbDeviceSpeedLow,
    DebugUsbDeviceSpeedFull,
    DebugUsbDeviceSpeedHigh,
    DebugUsbDeviceSpeedSuper,
} DEBUG_USB_DEVICE_SPEED, *PDEBUG_USB_DEVICE_SPEED;

typedef enum _DEBUG_USB_TRANSFER_TYPE {
    DebugUsbTransferTypeInvalid,
    DebugUsbTransferTypeControl,
    DebugUsbTransferTypeInterrupt,
    DebugUsbTransferTypeBulk,
    DebugUsbTransferTypeIsochronous,
    DebugUsbTransferTypeCount
} DEBUG_USB_TRANSFER_TYPE, *PDEBUG_USB_TRANSFER_TYPE;

typedef enum _DEBUG_USB_TRANSFER_DIRECTION {
    DebugUsbTransferDirectionInvalid,
    DebugUsbTransferDirectionIn,
    DebugUsbTransferDirectionOut,
    DebugUsbTransferBidirectional,
    DebugUsbTransferDirectionCount
} DEBUG_USB_TRANSFER_DIRECTION, *PDEBUG_USB_TRANSFER_DIRECTION;

/*++

Structure Description:

    This structure stores information about a debug USB transfer request.

Members:

    Type - Stores the type of USB request that this transfer is.

    Direction - Stores the direction of the USB endpoint.

    Speed - Stores the speed of the destination device.

    HubAddress - Stores the address of the hub this device is connected to.
        This is only required for full or low speed devices on a high speed
        bus.

    HubPort - Stores the one-based port number of the hub this device is
        connected to. This is only required for full or low speed devices on a
        high speed bus.

    EndpointNumber - Stores the endpoint number, including the high 0x80 bit.

    DeviceAddress - Stores the device address of the device that owns this
        endpoint.

    DataToggle - Stores the data toggle value of the next transfer descriptor
        to be filled out.

    Halted - Stores a boolean indicating if the endpoint is halted.

    MaxPacketSize - Stores the maximum packet size of this endpoint.

--*/

typedef struct _DEBUG_USB_ENDPOINT {
    DEBUG_USB_TRANSFER_TYPE Type;
    DEBUG_USB_TRANSFER_DIRECTION Direction;
    DEBUG_USB_DEVICE_SPEED Speed;
    UCHAR HubAddress;
    UCHAR HubPort;
    UCHAR EndpointNumber;
    UCHAR DeviceAddress;
    BOOL DataToggle;
    BOOL Halted;
    ULONG MaxPacketSize;
} DEBUG_USB_ENDPOINT, *PDEBUG_USB_ENDPOINT;

/*++

Structure Description:

    This structure stores information about a debug USB transfer request.

Members:

    Endpoint - Stores a pointer to the endpoint this transfer is associated
        with.

    Direction - Stores the direction of the USB transfer. This must be
        consistent with the endpoint being sent to.

    Length - Stores the length of the request, in bytes.

    LengthTransferred - Stores the number of bytes that have actually been
        transferred.

    Buffer - Stores a pointer to the data buffer.

    BufferPhysicalAddress - Stores the physical address of the data buffer.

    HostContext - Stores a pointer of context for the host controller.

    HostDescriptorCount - Stores a value used optionally and internally by the
        host controller. Consumers should not use this value.

    Status - Stores the completion status of the request.

--*/

typedef struct _DEBUG_USB_TRANSFER {
    PDEBUG_USB_ENDPOINT Endpoint;
    DEBUG_USB_TRANSFER_DIRECTION Direction;
    ULONG Length;
    ULONG LengthTransferred;
    PVOID Buffer;
    PHYSICAL_ADDRESS BufferPhysicalAddress;
    PVOID HostContext;
    ULONG HostDescriptorCount;
    KSTATUS Status;
} DEBUG_USB_TRANSFER, *PDEBUG_USB_TRANSFER;

typedef
KSTATUS
(*PDEBUG_USB_HOST_INITIALIZE) (
    PVOID Context
    );

/*++

Routine Description:

    This routine initializes a USB debug device, preparing it to return the
    root hub status and ultimately send and receive transfers.

Arguments:

    Context - Supplies a pointer to the device context.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PDEBUG_USB_HOST_GET_ROOT_HUB_STATUS) (
    PVOID Context,
    ULONG PortIndex,
    PULONG PortStatus
    );

/*++

Routine Description:

    This routine queries the host controller for the status of a root hub port.

Arguments:

    Context - Supplies a pointer to the device context.

    PortIndex - Supplies the zero-based port number to query.

    PortStatus - Supplies a pointer where the port status is returned.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_OUT_OF_BOUNDS if the controller port index is out of range.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

typedef
KSTATUS
(*PDEBUG_USB_HOST_SET_ROOT_HUB_STATUS) (
    PVOID Context,
    ULONG PortIndex,
    ULONG PortStatus
    );

/*++

Routine Description:

    This routine sets the host controller for the status of a root hub port.

Arguments:

    Context - Supplies a pointer to the device context.

    PortIndex - Supplies the zero-based port number to set.

    PortStatus - Supplies the port status to set.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_OUT_OF_BOUNDS if the controller port index is out of range.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

typedef
KSTATUS
(*PDEBUG_USB_HOST_SETUP_TRANSFER) (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine allocates a buffer and initializes the given USB transfer.

Arguments:

    Context - Supplies a pointer to the device context.

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

typedef
KSTATUS
(*PDEBUG_USB_HOST_SUBMIT_TRANSFER) (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer,
    BOOL WaitForCompletion
    );

/*++

Routine Description:

    This routine submits a previously set up USB transfer.

Arguments:

    Context - Supplies a pointer to the device context.

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

typedef
KSTATUS
(*PDEBUG_USB_HOST_CHECK_TRANSFER) (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine checks on the completion status of a transfer.

Arguments:

    Context - Supplies a pointer to the device context.

    Transfer - Supplies a pointer to the transfer to check.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

    STATUS_MORE_PROCESSING_REQUIRED if the transfer is still in progress.

--*/

typedef
KSTATUS
(*PDEBUG_USB_HOST_RETIRE_TRANSFER) (
    PVOID Context,
    PDEBUG_USB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine retires an EHCI. This frees the buffer allocated during setup.

Arguments:

    Context - Supplies a pointer to the device context.

    Transfer - Supplies a pointer to the transfer to retire.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_INVALID_PARAMETER if the given transfer was not initialized
    correctly.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

typedef
KSTATUS
(*PDEBUG_USB_HOST_STALL) (
    PVOID Context,
    ULONG Milliseconds
    );

/*++

Routine Description:

    This routine burns time using the frame index register to mark time.

Arguments:

    Context - Supplies a pointer to the device context.

    Milliseconds - Supplies the number of milliseconds to stall for.

Return Value:

    STATUS_SUCCESS if the hub status was successfully queried.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

typedef
KSTATUS
(*PDEBUG_USB_HOST_GET_HANDOFF_DATA) (
    PVOID Context,
    PDEBUG_USB_HANDOFF_DATA HandoffData
    );

/*++

Routine Description:

    This routine returns the controller specific handoff data in preparation
    for the real USB driver taking over primary functionality.

Arguments:

    Context - Supplies a pointer to the device context.

    HandoffData - Supplies a pointer to the handoff data. The controller should
        fill in the host controller specific data and size fields.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure contains the function table for a debug USB host device.

Members:

    Initialize - Stores a pointer to a function used to initialize the USB
        host controller.

    GetRootHubStatus - Stores a pointer to a function used to get the status
        of a port on the root hub of the USB host controller.

    SetRootHubStatus - Stores a pointer to a function used to set the status
        of a port on the root hub of the USB host controller.

    SetupTransfer - Stores a pointer to a function used to allocate and
        initialize a transfer that will be submitted to the USB host controller.

    SubmitTransfer - Stores a pointer to a function use to submit a transfer
        to the USB host controller.

    CheckTransfer - Stores a pointer to a function used to check the completion
        status of a submitted transfer.

    RetireTransfer - Stores a pointer to a function used to deschedule and
        deallocate a previously set up usb transfer.

    Stall - Stores a pointer to a function that provides time-based busy
        spinning using the USB host controller's frame counter.

    GetHandoffData - Stores a pointer to a function used to get the handoff
        data in preparation for the official USB host controller driver taking
        over.

--*/

typedef struct _DEBUG_USB_HOST_FUNCTION_TABLE {
    PDEBUG_USB_HOST_INITIALIZE Initialize;
    PDEBUG_USB_HOST_GET_ROOT_HUB_STATUS GetRootHubStatus;
    PDEBUG_USB_HOST_SET_ROOT_HUB_STATUS SetRootHubStatus;
    PDEBUG_USB_HOST_SETUP_TRANSFER SetupTransfer;
    PDEBUG_USB_HOST_SUBMIT_TRANSFER SubmitTransfer;
    PDEBUG_USB_HOST_CHECK_TRANSFER CheckTransfer;
    PDEBUG_USB_HOST_RETIRE_TRANSFER RetireTransfer;
    PDEBUG_USB_HOST_STALL Stall;
    PDEBUG_USB_HOST_GET_HANDOFF_DATA GetHandoffData;
} DEBUG_USB_HOST_FUNCTION_TABLE, *PDEBUG_USB_HOST_FUNCTION_TABLE;

/*++

Structure Description:

    This structure is used to describe a USB host controller implementation
    that can be used for kernel debugging.

Members:

    TableVersion - Stores the version of the USB host controller description
        table as understood by the hardware module. Set this to
        DEBUG_USB_HOST_DESCRIPTION_VERSION.

    FunctionTable - Stores the table of pointers to the hardware module's
        functions.

    Context - Stores a pointer's worth of data specific to this instance. This
        pointer will be passed back to the hardware module on each call.

    Identifier - Stores the unique identifier of the controller.

    PortSubType - Stores the host controller sub-type, as defined by the Debug
        Port Table 2 specification.

--*/

typedef struct _DEBUG_USB_HOST_DESCRIPTION {
    ULONG TableVersion;
    DEBUG_USB_HOST_FUNCTION_TABLE FunctionTable;
    PVOID Context;
    ULONGLONG Identifier;
    USHORT PortSubType;
} DEBUG_USB_HOST_DESCRIPTION, *PDEBUG_USB_HOST_DESCRIPTION;

/*++

Structure Description:

    This structure stores the information about a USB debug device.

Members:

    Host - Stores a pointer to the host controller.

--*/

typedef struct _HARDWARE_USB_DEBUG_DEVICE {
    PDEBUG_USB_HOST_DESCRIPTION Host;
} HARDWARE_USB_DEBUG_DEVICE, *PHARDWARE_USB_DEBUG_DEVICE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
KdUsbInitialize (
    PDEBUG_USB_HOST_DESCRIPTION Host,
    BOOL TestInterface
    );

/*++

Routine Description:

    This routine initializes a USB debug based transport.

Arguments:

    Host - Supplies a pointer to the host controller.

    TestInterface - Supplies a boolean indicating if the interface test should
        be run. This is only true under debugging scenarios where the USB
        debug transport itself is being debugged.

Return Value:

    Status code.

--*/

//
// Host controller module initialization functions.
//

VOID
KdEhciModuleEntry (
    VOID
    );

/*++

Routine Description:

    This routine is the entry point for a hardware module. Its role is to
    detect the prescense of any of the hardware modules it contains
    implementations for and instantiate them with the kernel.

Arguments:

    None.

Return Value:

    None.

--*/

