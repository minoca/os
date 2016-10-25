/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbcore.h

Abstract:

    This header contains internal definitions for the core USB library.

Author:

    Evan Green 15-Jan-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Redefine the API define into an export.
//

#define USB_API __DLLEXPORT

#include <minoca/usb/usbhost.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/kdusb.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used across the USB core library.
//

#define USB_CORE_ALLOCATION_TAG 0x43627355 // 'CbsU'

//
// Define the magic number used to catch people who attempt to allocate USB
// transfers themselves.
//

#define USB_TRANSFER_INTERNAL_MAGIC 0xBEEF57A8

//
// Define the number of entries in the first level table of USB children by
// address.
//

#define USB_HOST_ADDRESS_SEGMENT_COUNT 8

//
// Define the number of addresses per segment.
//

#define USB_HOST_ADDRESSES_PER_SEGMENT 16

//
// Define private transfer flags.
//

//
// This flag is set if the tranfser was submitted synchronously.
//

#define USB_TRANSFER_PRIVATE_SYNCHRONOUS 0x00000001

//
// Define USB debug flags.
//

#define USB_DEBUG_TRANSFERS             0x00000001
#define USB_DEBUG_TRANSFER_COMPLETION   0x00000002
#define USB_DEBUG_HUB                   0x00000004
#define USB_DEBUG_ENUMERATION           0x00000008
#define USB_DEBUG_DEBUGGER_HANDOFF      0x00000010
#define USB_DEBUG_ERRORS                0x00000020

//
// Define USB core specific errors that are reported to the system.
//

#define USB_CORE_ERROR_ENDPOINT_HALTED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a transfer completion queue.

Members:

    WorkQueue - Stores a pointer to the work queue on which the work item runs.

    WorkItem - Stores a pointer to the work item that does the work of
        completing the transfers.

    CompletedTransfersList - Stores the head of the list of completed transfers
        whose callback routines need to be invoked.

    CompletedTransfersListLock - Stores a spin lock that protects the completed
        transfers list.

--*/

typedef struct _USB_TRANSFER_COMPLETION_QUEUE {
    PWORK_QUEUE WorkQueue;
    PWORK_ITEM WorkItem;
    LIST_ENTRY CompletedTransfersList;
    KSPIN_LOCK CompletedTransfersListLock;
} USB_TRANSFER_COMPLETION_QUEUE, *PUSB_TRANSFER_COMPLETION_QUEUE;

/*++

Structure Description:

    This structure stores information about a host controller instance, internal
    to the USB core library.

Members:

    ListEntry - Stores pointers to the next and previous host controllers in the
        master list.

    Device - Stores the interface back to the host controller.

    RootDevice - Stores a pointer to the root hub's USB device.

    RootHub - Stores a pointer to the root hub.

    ChildrenByAddress - Stores an array of arrays that index the allocated
        devices by device address. They're segmented so as to avoid allocating
        128 * sizeof(PVOID) bytes for every host controller.

    Lock - Stores a pointer to the lock that synchronizes some accesses to this
        controller, including control transfers sent to address zero.

    AddressLock - Stores a pointer to the lock that synchronizes address
        assignment for this controller.

    ControllerFull - Stores a boolean indicating that every address in the
        controller is currently allocated.

    TransferWorkItem - Stores a pointer to the work item used to process
        finished transfers.

    CompletedTransfersList - Stores a pointer to the list of transfers that are
        complete and awaiting processing.

    TransferWorkItemQueued - Stores a boolean indicating if the transfer work
        item has already been queued.

    CompletedTransfersListLock - Stores a lock protecting the completed
        transfers list and the work item queued flag.

    PortStatusWorkItem - Stores a pointer to the work item used to process port
        status changes.

    PortStatusWorkItemQueued - Stores a boolean indicating if the port status
        work item is queued.

    HandoffData - Stores a pointer to the KD debug handoff data for this
        controller.

--*/

typedef struct _USB_HOST_CONTROLLER {
    LIST_ENTRY ListEntry;
    USB_HOST_CONTROLLER_INTERFACE Device;
    PUSB_DEVICE RootDevice;
    PUSB_HUB RootHub;
    PUSB_DEVICE *ChildrenByAddress[USB_HOST_ADDRESS_SEGMENT_COUNT];
    PQUEUED_LOCK Lock;
    PQUEUED_LOCK AddressLock;
    BOOL ControllerFull;
    USB_TRANSFER_COMPLETION_QUEUE TransferCompletionQueue;
    PWORK_ITEM PortStatusWorkItem;
    volatile ULONG PortStatusWorkItemQueued;
    PDEBUG_HANDOFF_DATA HandoffData;
 } USB_HOST_CONTROLLER, *PUSB_HOST_CONTROLLER;

/*++

Structure Description:

    This structure stores information about a USB device configuration.

Members:

    Description - Stores the public description.

    ListEntry - Stores pointers to the next and previous cached configurations
        for the owning device.

--*/

typedef struct _USB_CONFIGURATION {
    USB_CONFIGURATION_DESCRIPTION Description;
    LIST_ENTRY ListEntry;
} USB_CONFIGURATION, *PUSB_CONFIGURATION;

/*++

Structure Description:

    This structure stores information about an active USB endpoint.

Members:

    ListEntry - Stores pointers to the next and previous endpoints in the
        interface.

    ReferenceCount - Stores the reference count on the endpoint.

    HostControllerContext - Stores a pointer to the opaque host controller
        data associated with this endpoint.

    Type - Stores the USB endpoint flavor.

    Direction - Stores the direction of the endpoint. Not all combinations of
        endpoint type and direction are valid.

    Number - Stores the endpoint number.

    MaxPacketSize - Stores the maximum packet size of the endpoint.

    PollRate - Stores the polling rate for interrupt and isochronous endpoints,
        in (micro)frames. It stores the NAK rate for high-speed control and
        bulk out endpoints.

--*/

typedef struct _USB_ENDPOINT {
    LIST_ENTRY ListEntry;
    volatile ULONG ReferenceCount;
    PVOID HostControllerContext;
    USB_TRANSFER_TYPE Type;
    USB_TRANSFER_DIRECTION Direction;
    UCHAR Number;
    ULONG MaxPacketSize;
    USHORT PollRate;
} USB_ENDPOINT, *PUSB_ENDPOINT;

/*++

Enumeration Description:

    This enumeration describes the different types of USB devices.

Values:

    UsbDeviceTypeNonHub - Indicates a USB device that is not a hub.

    UsbDeviceTypeHub - Indicates a USB device that is a hub, but not the root.

    UsbDeviceTypeRootHub - Indicates a USB device that is the root hub.

--*/

typedef enum _USB_DEVICE_TYPE {
    UsbDeviceTypeNonHub,
    UsbDeviceTypeHub,
    UsbDeviceTypeRootHub,
} USB_DEVICE_TYPE, *PUSB_DEVICE_TYPE;

/*++

Structure Description:

    This structure stores information about an active USB device.

Members:

    ListEntry - Stores pointers to the next and previous devices enumerated by
        the parent hub.

    GlobalListEntry - Stores pointers to the next and previous USB devices in
        the entire system.

    ReferenceCount - Stores the number of references currently held against
        the device.

    Type - Stores the device type.

    Controller - Stores a pointer to the host controller that owns this device.

    Parent - Stores a pointer to the parent device. Root hubs have no parent.

    Speed - Stores the device speed.

    Device - Stores a pointer to the OS device associated with this USB device.

    Driver - Stores a pointer to the OS driver associated with this USB device.

    BusAddress - Stores the device address on the USB.

    EndpointZero - Stores a pointer to the default control pipe endpoint.

    ConfigurationLock - Stores a pointer to a queued lock that guards access to
        the configuration settings.

    ConfigurationCount - Stores the number of configurations in the device.

    ConfigurationList - Stores the head of the list of cached configurations.

    ActiveConfiguration - Stores a pointer to the currently active configuration
        of the device.

    ChildLock - Stores a pointer to a queued lock that guards access to the
        child list and the port status of the children.

    ChildList - Stores the head of the list of children for a hub device.

    ChildPortCount - Stores the number of downstream ports the hub has. For
        non-hub devices, this value will be zero.

    PortNumber - Stores which port of the parent hub this device lives in.

    Depth - Stores the hub depth of the device. Zero is a root hub, one is a
        device attached to the root hub, etc.

    Manufacturer - Stores a pointer to the manufacturer string.

    ProductName - Stores a pointer to the product name string.

    SerialNumber - Stores a pointer to the serial number string.

    VendorId - Stores the Vendor ID (VID) of the device.

    ProductId - Stores the Product ID (PID) of the device.

    ClassCode - Stores the device class code.

    SubclassCode - Stores the device subclass.

    ProtocolCode - Stores the device protocol code (remember the class,
        subclass, and protocol form a tuple).

    Lock - Stores a pointer to a queued lock that guards access to the device's
        status, including its connected state and transfer list.

    Connected - Stores a boolean indicating if the device is connected to the
        system (TRUE), or is removed and waiting for remaining handles to be
        closed (FALSE).

    DebugDevice - Stores a boolean indicating that this is the debug device.

    TransferList - Stores the head of the list of transfers for the device.

--*/

struct _USB_DEVICE {
    LIST_ENTRY ListEntry;
    LIST_ENTRY GlobalListEntry;
    volatile ULONG ReferenceCount;
    USB_DEVICE_TYPE Type;
    PUSB_HOST_CONTROLLER Controller;
    PUSB_DEVICE Parent;
    USB_DEVICE_SPEED Speed;
    PDEVICE Device;
    PDRIVER Driver;
    UCHAR BusAddress;
    PUSB_ENDPOINT EndpointZero;
    PQUEUED_LOCK ConfigurationLock;
    UCHAR ConfigurationCount;
    LIST_ENTRY ConfigurationList;
    PUSB_CONFIGURATION ActiveConfiguration;
    PQUEUED_LOCK ChildLock;
    LIST_ENTRY ChildList;
    ULONG ChildPortCount;
    UCHAR PortNumber;
    UCHAR Depth;
    PSTR Manufacturer;
    PSTR ProductName;
    PSTR SerialNumber;
    USHORT VendorId;
    USHORT ProductId;
    UCHAR ClassCode;
    UCHAR SubclassCode;
    UCHAR ProtocolCode;
    PQUEUED_LOCK Lock;
    BOOL Connected;
    BOOL DebugDevice;
    LIST_ENTRY TransferList;
};

/*++

Structure Description:

    This structure stores information about an active USB interface.

Members:

    Description - Stores the public description of the interface.

    EndpointList - Stores the head of the list of USB endpoints (the internal
        structures).

    Device - Stores a pointer to the OS device associated with this interface.

    Driver - Stores a pointer to the OS driver associated with this interface.

--*/

typedef struct _USB_INTERFACE {
    USB_INTERFACE_DESCRIPTION Description;
    LIST_ENTRY EndpointList;
    PDEVICE Device;
    PDRIVER Driver;
} USB_INTERFACE, *PUSB_INTERFACE;

/*++

Enumeration Description:

    This enumeration describes the various states of a USB transfer.

Values:

    TransferInvalid - Indicates that the transfer is not yet fully initialized.

    TransferInactive - Indicates that the transfer is not actively being
        processed by USB core.

    TransferActive - Indicates that the transfer is actively being processed by
        USB core.

    TransferInCallback - Indicates that the transfer is in the middle of the
        driver's callback routine.

--*/

typedef enum _USB_TRANSFER_STATE {
    TransferInvalid,
    TransferInactive,
    TransferActive,
    TransferInCallback
} USB_TRANSFER_STATE, *PUSB_TRANSFER_STATE;

/*++

Structure Description:

    This structure stores information about an active USB transfer.

Members:

    Protected - Stores the public and semi-public portions of the transfer.

    Magic - Stores a magic number, used to ensure that some cowboy didn't try
        to allocate the public version of the structure on his own. Set to
        USB_TRANSFER_INTERNAL_MAGIC.

    ReferenceCount - Stores a reference count for the transfer.

    CompletionListEntry - Stores pointers to the next and previous transfers in
        the list of unprocessed but completed transfers.

    DeviceListEntry -  Stores pointers to the next and previous transfers in
        the list of transfers that belong to the transfer's device.

    Device - Stores a copy of the pointer to the device the transfer was
        allocated for, used to ensure that people trying to be clever don't
        change this pointer to a different device.

    Endpoint - Stores a pointer to the endpoint this transfer is aimed at. This
        value is calculated when the transfer is submitted.

    LastEndpointNumber - Stores the endpoint number of the transfer last time
        it was submitted. This is used to avoid traversing the linked lists
        again if the endpoint number hasn't changed.

    MaxTransferSize - Stores the maximum length that can be supported with
        this transfer.

    HostControllerContext - Stores an pointer to the host controller context for
        the transfer.

    PrivateFlags - Stores a bitfield of internal flags. See
        USB_TRANSFER_PRIVATE_* definitions.

    Event - Supplies a pointer to the event used for synchronous transfers.

    State - Stores the current state of the transfer. This is of type
        USB_TRANSFER_STATE.

--*/

typedef struct _USB_TRANSFER_PRIVATE {
    USB_TRANSFER_INTERNAL Protected;
    ULONG Magic;
    volatile ULONG ReferenceCount;
    LIST_ENTRY CompletionListEntry;
    LIST_ENTRY DeviceListEntry;
    PUSB_DEVICE Device;
    PUSB_ENDPOINT Endpoint;
    UCHAR LastEndpointNumber;
    ULONG MaxTransferSize;
    PVOID HostControllerContext;
    ULONG PrivateFlags;
    PKEVENT Event;
    volatile ULONG State;
} USB_TRANSFER_PRIVATE, *PUSB_TRANSFER_PRIVATE;

//
// -------------------------------------------------------------------- Globals
//

extern PDRIVER UsbCoreDriver;

//
// Store a list of all active host controllers and a lock that protects this
// list.
//

extern LIST_ENTRY UsbHostControllerList;
extern PQUEUED_LOCK UsbHostControllerListLock;

//
// Store a pointer to the USB core work queue.
//

extern PWORK_QUEUE UsbCoreWorkQueue;

//
// Store a list of all active USB devices in the system.
//

extern LIST_ENTRY UsbDeviceList;
extern PQUEUED_LOCK UsbDeviceListLock;

//
// Store a bitfield of enabled USB debug flags. See USB_DEBUG_* definitions.
//

extern ULONG UsbDebugFlags;

//
// Set this to enable debugging only a single device address. If this is zero,
// it's enabled on all addresses.
//

extern UCHAR UsbDebugDeviceAddress;

//
// Store a pointer to the USB debugger handoff data.
//

extern PDEBUG_HANDOFF_DATA UsbDebugHandoffData;

//
// Define transfer direction and endpoint type strings.
//

extern PSTR UsbTransferDirectionStrings[UsbTransferDirectionCount];
extern PSTR UsbTransferTypeStrings[UsbTransferTypeCount];
extern PSTR UsbErrorStrings[UsbErrorCount];

//
// -------------------------------------------------------- Function Prototypes
//

//
// Core routines
//

PUSB_TRANSFER
UsbpAllocateTransfer (
    PUSB_DEVICE Device,
    UCHAR EndpointNumber,
    ULONG MaxTransferSize,
    ULONG Flags
    );

/*++

Routine Description:

    This routine allocates a new USB transfer structure. This routine must be
    used to allocate transfers.

Arguments:

    Device - Supplies a pointer to the device the transfer will eventually be
        submitted to. This must not be changed by the caller in the transfer
        structure once set.

    EndpointNumber - Supplies the endpoint number that the transfer will go to.

    MaxTransferSize - Supplies the maximum length, in bytes, of the transfer.
        Attempts to submit a transfer with lengths longer than this initialized
        length will fail. Longer transfer sizes do require more resources as
        they are split into subpackets, so try to be reasonable.

    Flags - Supplies a bitfield of flags regarding the transaction. See
        USB_TRANSFER_FLAG_* definitions.

Return Value:

    Returns a pointer to the new USB transfer on success.

    NULL when there are insufficient resources to complete the request.

--*/

VOID
UsbpCancelAllTransfers (
    PUSB_DEVICE Device
    );

/*++

Routine Description:

    This routine cancels all transfers for the given USB core device. The
    device must be disconnected before calling into this routine.

Arguments:

    Device - Supplies the core handle to the device whose transfers are
        to be cancelled.

Return Value:

    None.

--*/

KSTATUS
UsbpReadConfigurationDescriptors (
    PUSB_DEVICE Device,
    PUSB_DEVICE_DESCRIPTOR DeviceDescriptor
    );

/*++

Routine Description:

    This routine attempts to read all configuration descriptors from the device.

Arguments:

    Device - Supplies a pointer to the device to query.

    DeviceDescriptor - Supplies a pointer to the device descriptor.

Return Value:

    Status code.

--*/

KSTATUS
UsbpInitializeTransferCompletionQueue (
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue,
    BOOL PrivateWorkQueue
    );

/*++

Routine Description:

    This routine initializes the given transfer completion queue.

Arguments:

    CompletionQueue - Supplies a pointer to a USB transfer completion queue
        that is to be initialized.

    PrivateWorkQueue - Supplies a boolean indicating whether or not the
        completion queue requires a private work queue for queuing its work
        item.

Return Value:

    Status code.

--*/

VOID
UsbpDestroyTransferCompletionQueue (
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue
    );

/*++

Routine Description:

    This routine destroys the given transfer completion queue. It does not
    release the completion queue's memory.

Arguments:

    CompletionQueue - Supplies a pointer to a USB transfer completion queue
        that is to be destroyed.

Return Value:

    Status code.

--*/

VOID
UsbpProcessCompletedTransfer (
    PUSB_TRANSFER_INTERNAL Transfer
    );

/*++

Routine Description:

    This routine processes the completed transfer. It will either signal
    synchronous transfers or queue asynchronous transfers on the correct
    transfer completion queue so that its callback routine can be completed at
    low level. This routine is called at dispatch.

Arguments:

    Transfer - Supplies a pointer to a completed transfer.

Return Value:

    None.

--*/

//
// Host routines
//

KSTATUS
UsbpCreateEndpoint (
    PUSB_DEVICE Device,
    UCHAR Number,
    USB_TRANSFER_DIRECTION Direction,
    USB_TRANSFER_TYPE Type,
    ULONG MaxPacketSize,
    ULONG PollRate,
    PUSB_ENDPOINT *CreatedEndpoint
    );

/*++

Routine Description:

    This routine creates the accounting structures associated with a new USB
    endpoint.

Arguments:

    Device - Supplies a pointer to the device that will own the endpoint.

    Number - Supplies the endpoint number of the new endpoint.

    Direction - Supplies the direction of the endpoint.

    Type - Supplies the type of the endpoint.

    MaxPacketSize - Supplies the maximum packet size for the endpoint, in bytes.

    PollRate - Supplies the polling rate of the endpoint.

    CreatedEndpoint - Supplies a pointer where the newly minted endpoint will
        be returned.

Return Value:

    Status code.

--*/

VOID
UsbpResetEndpoint (
    PUSB_DEVICE Device,
    PUSB_ENDPOINT Endpoint
    );

/*++

Routine Description:

    This routine resets a USB endpoint.

Arguments:

    Device - Supplies a pointer to the device to which the endpoint belongs.

    Endpoint - Supplies a pointer to the USB endpoint.

Return Value:

    None.

--*/

KSTATUS
UsbpFlushEndpoint (
    PUSB_DEVICE Device,
    PUSB_ENDPOINT Endpoint,
    PULONG TransferCount
    );

/*++

Routine Description:

    This routine flushes the given endpoint for the given USB device. This
    includes busily waiting for all active transfers to complete. This is only
    meant to be used at high run level when preparing to write a crash dump
    file using USB Mass Storage.

Arguments:

    Device - Supplies a pointer to the device to which the endpoint belongs.

    Endpoint - Supplies a pointer to the USB endpoint.

    TransferCount - Supplies a pointer that receives the total number of
        transfers that were flushed.

Return Value:

    Status code.

--*/

VOID
UsbpEndpointAddReference (
    PUSB_ENDPOINT Endpoint
    );

/*++

Routine Description:

    This routine increments the reference count on the given endpoint.

Arguments:

    Endpoint - Supplies a pointer to the endpoint whose reference count should
        be incremented.

Return Value:

    None.

--*/

VOID
UsbpEndpointReleaseReference (
    PUSB_DEVICE Device,
    PUSB_ENDPOINT Endpoint
    );

/*++

Routine Description:

    This routine decrements the reference count on the given endpoint, and
    destroys it if it hits zero.

Arguments:

    Device - Supplies a pointer to the device that owns the endpoint.

    Endpoint - Supplies a pointer to the endpoint whose reference count should
        be decremented.

Return Value:

    None.

--*/

//
// Enumeration routines
//

VOID
UsbpDeviceAddReference (
    PUSB_DEVICE Device
    );

/*++

Routine Description:

    This routine increments the reference count on the given device.

Arguments:

    Device - Supplies a pointer to the device whose reference count should be
        incremented.

Return Value:

    None.

--*/

VOID
UsbpDeviceReleaseReference (
    PUSB_DEVICE Device
    );

/*++

Routine Description:

    This routine decrements the reference count on the given device, and
    destroys it if it hits zero.

Arguments:

    Device - Supplies a pointer to the device whose reference count should be
        decremented.

Return Value:

    None.

--*/

KSTATUS
UsbpEnumerateDevice (
    PUSB_HUB ParentHub,
    PUSB_DEVICE ParentHubDevice,
    UCHAR PortNumber,
    USB_DEVICE_SPEED DeviceSpeed,
    PHANDLE DeviceHandle
    );

/*++

Routine Description:

    This routine creates a new USB device in the system. This routine must be
    called at low level, and must be called with the parent hub's child lock
    held.

Arguments:

    ParentHub - Supplies a pointer to the parent hub object.

    ParentHubDevice - Supplies the handle of the parent USB hub device
        enumerating the new device.

    PortNumber - Supplies the parent hub's one-based port number where this
        device exists.

    DeviceSpeed - Supplies the speed of the device being enumerated.

    DeviceHandle - Supplies a pointer where a handle representing the device
        will be returned upon success.

Return Value:

    Status code.

--*/

VOID
UsbpRemoveDevice (
    PUSB_DEVICE Device
    );

/*++

Routine Description:

    This routine removes a device from its parent hub. The parent USB device's
    child lock should be held.

Arguments:

    Device - Supplies a pointer to the device that is to be removed.

Return Value:

    None.

--*/

KSTATUS
UsbpReserveDeviceAddress (
    PUSB_HOST_CONTROLLER Controller,
    PUSB_DEVICE Device,
    UCHAR Address
    );

/*++

Routine Description:

    This routine assigns the given device to a specific address.

Arguments:

    Controller - Supplies a pointer to the controller the device lives on.

    Device - Supplies a pointer to the USB device reserving the address. This
        can be NULL.

    Address - Supplies the address to reserve.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if an allocation failed.

    STATUS_RESOURCE_IN_USE if the address is already assigned.

--*/

//
// Hub routines
//

VOID
UsbpNotifyRootHubStatusChange (
    PUSB_HUB RootHub
    );

/*++

Routine Description:

    This routine handles notifications from the host controller indicating that
    a port on the root hub has changed. It queries the port status for the hub
    and notifies the system if something is different.

Arguments:

    RootHub - Supplies a pointer to the USB root hub.

Return Value:

    None.

--*/

KSTATUS
UsbpResetHubPort (
    PUSB_HUB Hub,
    UCHAR PortIndex
    );

/*++

Routine Description:

    This routine resets the device behind the given port. The controller lock
    must be held.

Arguments:

    Hub - Supplies a pointer to the context of the hub whose port is to be
        reset.

    PortIndex - Supplies the zero-based port index on the hub to reset.

Return Value:

    Status code.

--*/

