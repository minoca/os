/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbcore.c

Abstract:

    This module implements the USB core library.

Author:

    Evan Green 15-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/fw/acpitabs.h>
#include "usbcore.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial allocation size for a configuration descriptor.
//

#define USB_INITIAL_CONFIGURATION_LENGTH 0xFF

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UsbpCancelTransfer (
    PUSB_TRANSFER_PRIVATE Transfer
    );

VOID
UsbpDestroyTransfer (
    PUSB_TRANSFER Transfer
    );

KSTATUS
UsbpGetConfiguration (
    PUSB_DEVICE Device,
    UCHAR ConfigurationNumber,
    BOOL NumberIsIndex,
    PUSB_CONFIGURATION *Configuration
    );

KSTATUS
UsbpSubmitTransfer (
    PUSB_TRANSFER Transfer,
    ULONG PrivateFlags,
    BOOL PolledMode
    );

KSTATUS
UsbpCreateEndpointsForInterface (
    PUSB_DEVICE Device,
    PUSB_INTERFACE Interface
    );

PUSB_ENDPOINT
UsbpGetDeviceEndpoint (
    PUSB_DEVICE Device,
    UCHAR EndpointNumber
    );

VOID
UsbpCompletedTransferWorker (
    PVOID Parameter
    );

RUNLEVEL
UsbpAcquireCompletedTransfersLock (
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue
    );

VOID
UsbpReleaseCompletedTransfersLock (
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue,
    RUNLEVEL OldRunLevel
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER UsbCoreDriver;

//
// Store a pointer to the USB core work queue.
//

PWORK_QUEUE UsbCoreWorkQueue = NULL;

//
// Store a pointer to the special USB paging transfer completion queue.
//

PUSB_TRANSFER_COMPLETION_QUEUE UsbCorePagingCompletionQueue = NULL;

//
// Store a list of all active host controllers and a lock that protects this
// list.
//

LIST_ENTRY UsbHostControllerList;
PQUEUED_LOCK UsbHostControllerListLock = NULL;

//
// Store a list of all active USB devices in the system.
//

LIST_ENTRY UsbDeviceList;
PQUEUED_LOCK UsbDeviceListLock = NULL;

//
// Store a bitfield of enabled USB debug flags. See USB_DEBUG_* definitions.
//

ULONG UsbDebugFlags = 0x0;

//
// Set this to enable debugging only a single device address. If this is zero,
// it's enabled on all addresses.
//

UCHAR UsbDebugDeviceAddress = 0x0;

//
// Store a pointer to the USB debugger handoff data.
//

PDEBUG_HANDOFF_DATA UsbDebugHandoffData = NULL;

//
// Define transfer direction and endpoint type strings.
//

PSTR UsbTransferDirectionStrings[UsbTransferDirectionCount] = {
    "INVALID",
    "from",
    "to",
    "from/to"
};

PSTR UsbTransferTypeStrings[UsbTransferTypeCount] = {
    "INVALID",
    "control",
    "interrupt",
    "bulk",
    "isochronous",
};

PSTR UsbErrorStrings[UsbErrorCount] = {
    "No error",
    "Not started",
    "Cancelled",
    "Allocated incorrectly",
    "Double submitted",
    "Incorrectly filled out",
    "Failed to submit",
    "Stalled",
    "Data buffer",
    "Babble",
    "Nak",
    "CrcOrTimeout",
    "Bitstuff",
    "Missed microframe",
    "Misaligned buffer",
    "Device not connected",
    "Short packet",
};

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine implements the initial entry point of the USB core library,
    called when the library is first loaded.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    Status code.

--*/

{

    ULONG PathIndex;
    KSTATUS Status;

    Status = STATUS_INSUFFICIENT_RESOURCES;
    UsbCoreDriver = Driver;

    //
    // Initialize USB structures.
    //

    INITIALIZE_LIST_HEAD(&UsbHostControllerList);
    INITIALIZE_LIST_HEAD(&UsbDeviceList);

    ASSERT((UsbHostControllerListLock == NULL) &&
           (UsbDeviceListLock == NULL) &&
           (UsbCorePagingCompletionQueue == NULL) &&
           (UsbCoreWorkQueue == NULL));

    UsbHostControllerListLock = KeCreateQueuedLock();
    if (UsbHostControllerListLock == NULL) {
        goto DriverEntryEnd;
    }

    UsbDeviceListLock = KeCreateQueuedLock();
    if (UsbDeviceListLock == NULL) {
        goto DriverEntryEnd;
    }

    UsbCoreWorkQueue = KeCreateWorkQueue(WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL,
                                         "UsbCoreWorker");

    if (UsbCoreWorkQueue== NULL) {
        goto DriverEntryEnd;
    }

    Status = KdGetDeviceInformation(&UsbDebugHandoffData);
    if ((!KSUCCESS(Status)) || (UsbDebugHandoffData == NULL) ||
        (UsbDebugHandoffData->PortType != DEBUG_PORT_TYPE_USB)) {

        UsbDebugHandoffData = NULL;
    }

    if ((UsbDebugFlags & USB_DEBUG_DEBUGGER_HANDOFF) != 0) {
        RtlDebugPrint("USB: Debug handoff data: 0x%x\n", UsbDebugHandoffData);
        if (UsbDebugHandoffData != NULL) {
            RtlDebugPrint("USB: Debug device %04X:%04X is at path ",
                          UsbDebugHandoffData->U.Usb.VendorId,
                          UsbDebugHandoffData->U.Usb.ProductId);

            for (PathIndex = 0;
                 PathIndex < UsbDebugHandoffData->U.Usb.DevicePathSize;
                 PathIndex += 1) {

                if (PathIndex != 0) {
                    RtlDebugPrint(", ");
                }

                RtlDebugPrint(
                             "%d",
                             UsbDebugHandoffData->U.Usb.DevicePath[PathIndex]);
            }

            RtlDebugPrint("\n");
        }
    }

    Status = STATUS_SUCCESS;

DriverEntryEnd:
    return Status;
}

USB_API
HANDLE
UsbDeviceOpen (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine attempts to open a USB device for I/O.

Arguments:

    Device - Supplies a pointer to the device to open.

Return Value:

    Returns a handle to the device upon success.

    INVALID_HANDLE if the device could not be opened.

--*/

{

    if (Device->Connected != FALSE) {
        UsbpDeviceAddReference(Device);
        return (HANDLE)Device;
    }

    return INVALID_HANDLE;
}

USB_API
VOID
UsbDeviceClose (
    HANDLE UsbDeviceHandle
    )

/*++

Routine Description:

    This routine closes an open USB handle.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

Return Value:

    None.

--*/

{

    PUSB_DEVICE Device;

    if (UsbDeviceHandle == INVALID_HANDLE) {
        return;
    }

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    UsbpDeviceReleaseReference(Device);
    return;
}

USB_API
PUSB_TRANSFER
UsbAllocateTransfer (
    HANDLE UsbDeviceHandle,
    UCHAR EndpointNumber,
    ULONG MaxTransferSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine allocates a new USB transfer structure. This routine must be
    used to allocate transfers.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

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

{

    PUSB_TRANSFER Transfer;

    Transfer = UsbpAllocateTransfer((PUSB_DEVICE)UsbDeviceHandle,
                                    EndpointNumber,
                                    MaxTransferSize,
                                    Flags);

    return Transfer;
}

USB_API
VOID
UsbDestroyTransfer (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine destroys an allocated transfer. This transfer must not be
    actively transferring.

Arguments:

    Transfer - Supplies a pointer to the transfer to destroy.

Return Value:

    None.

--*/

{

    UsbTransferReleaseReference(Transfer);
    return;
}

USB_API
KSTATUS
UsbSubmitTransfer (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine submits a USB transfer. The routine returns immediately,
    indicating only whether the transfer was submitted successfully. When the
    transfer actually completes, the callback routine will be called.

Arguments:

    Transfer - Supplies a pointer to the transfer to submit.

Return Value:

    STATUS_SUCCESS if the transfer was submitted to the USB host controller's
    queue.

    STATUS_INVALID_PARAMETER if one or more of the transfer fields is not
        properly filled out.

    Failing status codes if the request could not be submitted.

--*/

{

    return UsbpSubmitTransfer(Transfer, 0, FALSE);
}

USB_API
KSTATUS
UsbSubmitSynchronousTransfer (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine submits a USB transfer, and does not return until the transfer
    is completed successfully or with an error. This routine must be called at
    low level.

Arguments:

    Transfer - Supplies a pointer to the transfer to submit.

Return Value:

    STATUS_SUCCESS if the transfer was submitted to the USB host controller's
    queue.

    STATUS_INVALID_PARAMETER if one or more of the transfer fields is not
        properly filled out.

    Failing status codes if the request could not be submitted.

--*/

{

    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    CompleteTransfer = (PUSB_TRANSFER_PRIVATE)Transfer;
    KeSignalEvent(CompleteTransfer->Event, SignalOptionUnsignal);
    Status = UsbpSubmitTransfer(Transfer,
                                USB_TRANSFER_PRIVATE_SYNCHRONOUS,
                                FALSE);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Wait for the transfer to complete.
    //

    KeWaitForEvent(CompleteTransfer->Event, FALSE, WAIT_TIME_INDEFINITE);

    //
    // Assert that the transfer is now inactive. The caller should coordinate
    // not re-submitting this transfer before this call returns the status.
    //

    ASSERT(CompleteTransfer->State == TransferInactive);

    return Transfer->Status;
}

USB_API
KSTATUS
UsbSubmitPolledTransfer (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine submits a USB transfer, and does not return until the transfer
    is completed successfully or with an error. This routine is meant to be
    called in critical code paths at high level.

Arguments:

    Transfer - Supplies a pointer to the transfer to submit.

Return Value:

    Status code.

--*/

{

    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    ULONG OriginalState;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);
    ASSERT(Transfer->CallbackRoutine == NULL);

    Transfer->Flags |= USB_TRANSFER_FLAG_NO_INTERRUPT_ON_COMPLETION;
    Status = UsbpSubmitTransfer(Transfer,
                                USB_TRANSFER_PRIVATE_SYNCHRONOUS,
                                TRUE);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // If the transfer was successful, then it should be in the active state.
    // Flip it back to inactive.
    //

    CompleteTransfer = (PUSB_TRANSFER_PRIVATE)Transfer;
    OriginalState = RtlAtomicCompareExchange32(&(CompleteTransfer->State),
                                               TransferInactive,
                                               TransferActive);

    ASSERT(OriginalState == TransferActive);

    return Status;
}

USB_API
KSTATUS
UsbCancelTransfer (
    PUSB_TRANSFER Transfer,
    BOOL Wait
    )

/*++

Routine Description:

    This routine cancels a USB transfer, waiting for the transfer to enter the
    inactive state before returning. Must be called at low level.

Arguments:

    Transfer - Supplies a pointer to the transfer to cancel.

    Wait - Supplies a boolean indicating that the caller wants to wait for the
        transfer the reach the inactive state. Specify TRUE if unsure.

Return Value:

    Returns STATUS_SUCCESS if the transfer was successfully cancelled.

    Returns STATUS_TOO_LATE if the transfer was not cancelled, but moved to the
    inactive state.

--*/

{

    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Attempt to cancel the transfer.
    //

    CompleteTransfer = (PUSB_TRANSFER_PRIVATE)Transfer;
    Status = UsbpCancelTransfer(CompleteTransfer);

    //
    // If desired, wait until the transfer has entered the inactive state.
    //

    if (Wait != FALSE) {
        while (CompleteTransfer->State != TransferInactive) {
            KeYield();
        }

        //
        // If the transfer was successfully pulled off the hardware queue, then
        // it really shouldn't be active. If it was too late to cancel, then it
        // may be active again. Tough luck.
        //

        ASSERT(!KSUCCESS(Status) ||
               (CompleteTransfer->State == TransferInactive));
    }

    return Status;
}

USB_API
KSTATUS
UsbInitializePagingDeviceTransfers (
    VOID
    )

/*++

Routine Description:

    This routine initializes the USB core to handle special paging device
    transfers that are serviced on their own work queue.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue;
    PVOID OriginalQueue;
    KSTATUS Status;

    //
    // If the paging device transfer completion queue is already initialized,
    // then all is ready to go.
    //

    if (UsbCorePagingCompletionQueue != NULL) {
        return STATUS_SUCCESS;
    }

    //
    // Otherwise initialize a transfer completion queue.
    //

    AllocationSize = sizeof(USB_TRANSFER_COMPLETION_QUEUE);
    CompletionQueue = MmAllocateNonPagedPool(AllocationSize,
                                             USB_CORE_ALLOCATION_TAG);

    if (CompletionQueue == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePagingDeviceTransfersEnd;
    }

    Status = UsbpInitializeTransferCompletionQueue(CompletionQueue, TRUE);
    if (!KSUCCESS(Status)) {
        goto InitializePagingDeviceTransfersEnd;
    }

    //
    // Now try make this new transfer completion queue the global queue.
    //

    OriginalQueue = (PVOID)RtlAtomicCompareExchange(
                               (volatile UINTN *)&UsbCorePagingCompletionQueue,
                               (UINTN)CompletionQueue,
                               (UINTN)NULL);

    //
    // If the original queue value was still NULL, then this completion queue
    // won the race, do not destroy it below.
    //

    if (OriginalQueue == NULL) {
        CompletionQueue = NULL;
    }

    Status = STATUS_SUCCESS;

InitializePagingDeviceTransfersEnd:
    if (CompletionQueue != NULL) {
        UsbpDestroyTransferCompletionQueue(CompletionQueue);
        MmFreeNonPagedPool(CompletionQueue);
    }

    return Status;
}

USB_API
ULONG
UsbTransferAddReference (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine adds a reference to a USB transfer.

Arguments:

    Transfer - Supplies a pointer to the transfer that is to be referenced.

Return Value:

    Returns the old reference count.

--*/

{

    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    ULONG OldReferenceCount;

    CompleteTransfer = (PUSB_TRANSFER_PRIVATE)Transfer;
    OldReferenceCount = RtlAtomicAdd32(&(CompleteTransfer->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return OldReferenceCount;
}

USB_API
ULONG
UsbTransferReleaseReference (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine releases a reference on a USB transfer.

Arguments:

    Transfer - Supplies a pointer to the transfer that is to be reference.

Return Value:

    Returns the old reference count.

--*/

{

    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    ULONG OldReferenceCount;

    CompleteTransfer = (PUSB_TRANSFER_PRIVATE)Transfer;
    OldReferenceCount = RtlAtomicAdd32(&(CompleteTransfer->ReferenceCount),
                                       (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        UsbpDestroyTransfer(Transfer);
    }

    return OldReferenceCount;
}

USB_API
KSTATUS
UsbGetStatus (
    HANDLE UsbDeviceHandle,
    UCHAR RequestRecipient,
    USHORT Index,
    PUSHORT Data
    )

/*++

Routine Description:

    This routine gets the status from the given device, interface, or endpoint,
    as determined based on the request type and index. This routine must be
    called at low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    RequestRecipient - Supplies the recipient of this get status request.

    Index - Supplies the index of this get status request. This can be
        zero for devices, an interface number, or an endpoint number.

    Data - Supplies a pointer that receives the status from the request.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    ULONG LengthTransferred;
    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Validate the arguments.
    //

    if ((UsbDeviceHandle == INVALID_HANDLE) ||
        ((RequestRecipient != USB_SETUP_REQUEST_DEVICE_RECIPIENT) &&
         (RequestRecipient != USB_SETUP_REQUEST_INTERFACE_RECIPIENT) &&
         (RequestRecipient != USB_SETUP_REQUEST_ENDPOINT_RECIPIENT))) {

        Status = STATUS_INVALID_PARAMETER;
        goto GetStatusEnd;
    }

    //
    // Initialize the setup packet to send the device.
    //

    RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
    SetupPacket.RequestType = RequestRecipient | USB_SETUP_REQUEST_TO_HOST;
    SetupPacket.Request = USB_REQUEST_GET_STATUS;
    SetupPacket.Value = 0;
    SetupPacket.Index = Index;
    SetupPacket.Length = sizeof(USHORT);

    //
    // Send the transfer.
    //

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Status = UsbSendControlTransfer(Device,
                                    UsbTransferDirectionIn,
                                    &SetupPacket,
                                    Data,
                                    sizeof(USHORT),
                                    &LengthTransferred);

    //
    // Return failure if the transfer succeeded, but not enough bytes were
    // returned.
    //

    if (KSUCCESS(Status) && (LengthTransferred < sizeof(USHORT))) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto GetStatusEnd;
    }

GetStatusEnd:
    return Status;
}

USB_API
KSTATUS
UsbSetFeature (
    HANDLE UsbDeviceHandle,
    UCHAR RequestRecipient,
    USHORT Feature,
    USHORT Index
    )

/*++

Routine Description:

    This routine sets the given feature for a device, interface or endpoint,
    as specified by the request type and index. This routine must be called at
    low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    RequestRecipient - Supplies the recipient of this clear feature request.

    Feature - Supplies the value of this clear feature request.

    Index - Supplies the index of this clear feature request. This can be
        zero for devices, an interface number, or an endpoint number.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Validate the arguments.
    //

    if ((UsbDeviceHandle == INVALID_HANDLE) ||
        ((RequestRecipient != USB_SETUP_REQUEST_DEVICE_RECIPIENT) &&
         (RequestRecipient != USB_SETUP_REQUEST_INTERFACE_RECIPIENT) &&
         (RequestRecipient != USB_SETUP_REQUEST_ENDPOINT_RECIPIENT)) ||
        ((RequestRecipient == USB_SETUP_REQUEST_ENDPOINT_RECIPIENT) &&
         (Feature != USB_FEATURE_ENDPOINT_HALT)) ||
        ((RequestRecipient == USB_SETUP_REQUEST_DEVICE_RECIPIENT) &&
         (Feature != USB_FEATURE_DEVICE_REMOTE_WAKEUP))) {

        Status = STATUS_INVALID_PARAMETER;
        goto SetFeatureEnd;
    }

    //
    // There are no interface features defined in the USB specification.
    //

    ASSERT(RequestRecipient != USB_SETUP_REQUEST_INTERFACE_RECIPIENT);

    //
    // The test mode feature is not allowed to be cleared.
    //

    ASSERT(Feature != USB_FEATURE_DEVICE_TEST_MODE);

    //
    // Initialize the setup packet to send the device.
    //

    RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
    SetupPacket.RequestType = RequestRecipient | USB_SETUP_REQUEST_TO_DEVICE;
    SetupPacket.Request = USB_REQUEST_SET_FEATURE;
    SetupPacket.Value = Feature;
    SetupPacket.Index = Index;
    SetupPacket.Length = 0;

    //
    // Send the transfer.
    //

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Status = UsbSendControlTransfer(Device,
                                    UsbTransferDirectionOut,
                                    &SetupPacket,
                                    NULL,
                                    0,
                                    NULL);

    if (!KSUCCESS(Status)) {
        goto SetFeatureEnd;
    }

SetFeatureEnd:
    return Status;
}

USB_API
KSTATUS
UsbClearFeature (
    HANDLE UsbDeviceHandle,
    UCHAR RequestRecipient,
    USHORT Feature,
    USHORT Index
    )

/*++

Routine Description:

    This routine clears the given feature from a device, interface or endpoint,
    as specified by the request type and index. This routine must be called at
    low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    RequestRecipient - Supplies the recipient of this clear feature request.

    Feature - Supplies the value of this clear feature request.

    Index - Supplies the index of this clear feature request. This can be
        zero for devices, an interface number, or an endpoint number.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    PUSB_ENDPOINT Endpoint;
    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Validate the arguments.
    //

    if ((UsbDeviceHandle == INVALID_HANDLE) ||
        ((RequestRecipient != USB_SETUP_REQUEST_DEVICE_RECIPIENT) &&
         (RequestRecipient != USB_SETUP_REQUEST_INTERFACE_RECIPIENT) &&
         (RequestRecipient != USB_SETUP_REQUEST_ENDPOINT_RECIPIENT)) ||
        ((RequestRecipient == USB_SETUP_REQUEST_ENDPOINT_RECIPIENT) &&
         (Feature != USB_FEATURE_ENDPOINT_HALT)) ||
        ((RequestRecipient == USB_SETUP_REQUEST_DEVICE_RECIPIENT) &&
         (Feature != USB_FEATURE_DEVICE_REMOTE_WAKEUP))) {

        Status = STATUS_INVALID_PARAMETER;
        goto ClearFeatureEnd;
    }

    //
    // There are no interface features defined in the USB specification.
    //

    ASSERT(RequestRecipient != USB_SETUP_REQUEST_INTERFACE_RECIPIENT);

    //
    // The test mode feature is not allowed to be cleared.
    //

    ASSERT(Feature != USB_FEATURE_DEVICE_TEST_MODE);

    //
    // Initialize the setup packet to send the device.
    //

    RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
    SetupPacket.RequestType = RequestRecipient | USB_SETUP_REQUEST_TO_DEVICE;
    SetupPacket.Request = USB_REQUEST_CLEAR_FEATURE;
    SetupPacket.Value = Feature;
    SetupPacket.Index = Index;
    SetupPacket.Length = 0;

    //
    // Send the transfer.
    //

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Status = UsbSendControlTransfer(Device,
                                    UsbTransferDirectionOut,
                                    &SetupPacket,
                                    NULL,
                                    0,
                                    NULL);

    if (!KSUCCESS(Status)) {
        goto ClearFeatureEnd;
    }

    //
    // If this was a successful attempt to clear an endpoint's HALT feature,
    // then the endpoint's data toggle needs to be unset, ensuring that the
    // next transfer on the endpoint will use DATA0.
    //

    if ((RequestRecipient == USB_SETUP_REQUEST_ENDPOINT_RECIPIENT) &&
        (Feature == USB_FEATURE_ENDPOINT_HALT)) {

        Endpoint = UsbpGetDeviceEndpoint(Device, Index);
        if (Endpoint == NULL) {

            ASSERT(Endpoint != NULL);

            Status = STATUS_NOT_FOUND;
            goto ClearFeatureEnd;
        }

        UsbpResetEndpoint(Device, Endpoint);
    }

ClearFeatureEnd:
    return Status;
}

USB_API
ULONG
UsbGetConfigurationCount (
    HANDLE UsbDeviceHandle
    )

/*++

Routine Description:

    This routine gets the number of possible configurations in a given device.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

Return Value:

    Returns the number of configurations in the device.

--*/

{

    PUSB_DEVICE Device;

    if (UsbDeviceHandle == INVALID_HANDLE) {
        return 0;
    }

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    return Device->ConfigurationCount;
}

USB_API
KSTATUS
UsbGetConfiguration (
    HANDLE UsbDeviceHandle,
    UCHAR ConfigurationNumber,
    BOOL NumberIsIndex,
    PUSB_CONFIGURATION_DESCRIPTION *Configuration
    )

/*++

Routine Description:

    This routine gets a configuration out of the given device. This routine will
    send a blocking request to the device. This routine must be called at low
    level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    ConfigurationNumber - Supplies the index or configuration value of the
        configuration to get.

    NumberIsIndex - Supplies a boolean indicating whether the configuration
        number is an index (TRUE) or a specific configuration value (FALSE).

    Configuration - Supplies a pointer where a pointer to the desired
        configuration will be returned.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    PUSB_CONFIGURATION InternalConfiguration;
    KSTATUS Status;

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Status = UsbpGetConfiguration(Device,
                                  ConfigurationNumber,
                                  NumberIsIndex,
                                  &InternalConfiguration);

    *Configuration = &(InternalConfiguration->Description);
    return Status;
}

USB_API
PUSB_CONFIGURATION_DESCRIPTION
UsbGetActiveConfiguration (
    HANDLE UsbDeviceHandle
    )

/*++

Routine Description:

    This routine gets the currently active configuration set in the device.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

Return Value:

    Returns a pointer to the current configuration.

    NULL if the device is not currently configured.

--*/

{

    PUSB_DEVICE Device;

    if (UsbDeviceHandle == INVALID_HANDLE) {
        return NULL;
    }

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    if (Device->ActiveConfiguration == NULL) {
        return NULL;
    }

    return &(Device->ActiveConfiguration->Description);
}

USB_API
KSTATUS
UsbSetConfiguration (
    HANDLE UsbDeviceHandle,
    UCHAR ConfigurationNumber,
    BOOL NumberIsIndex
    )

/*++

Routine Description:

    This routine sets the configuration to the given configuration value. This
    routine must be called at low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    ConfigurationNumber - Supplies the configuration index or value to set.

    NumberIsIndex - Supplies a boolean indicating whether the configuration
        number is an index (TRUE) or a specific configuration value (FALSE).

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION Configuration;
    PLIST_ENTRY CurrentEndpointEntry;
    PLIST_ENTRY CurrentInterfaceEntry;
    PUSB_DEVICE Device;
    PUSB_ENDPOINT Endpoint;
    PUSB_INTERFACE Interface;
    PLIST_ENTRY InterfaceListHead;
    ULONG LengthTransferred;
    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;

    Device = (PUSB_DEVICE)UsbDeviceHandle;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // First, get the configuration being described.
    //

    Status = UsbpGetConfiguration(Device,
                                  ConfigurationNumber,
                                  NumberIsIndex,
                                  &Configuration);

    if (!KSUCCESS(Status)) {
        goto SetConfigurationEnd;
    }

    //
    // Initialize the setup packet to send the device.
    //

    RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
    SetupPacket.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                              USB_SETUP_REQUEST_STANDARD |
                              USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    SetupPacket.Request = USB_DEVICE_REQUEST_SET_CONFIGURATION;
    SetupPacket.Value =
                      Configuration->Description.Descriptor.ConfigurationValue;

    SetupPacket.Index = 0;
    SetupPacket.Length = 0;

    //
    // Lock the device and send the set request. The device is locked to avoid
    // getting the active configuration variable out of sync with what the
    // device actually has set.
    //

    KeAcquireQueuedLock(Device->ConfigurationLock);
    Status = UsbSendControlTransfer(Device,
                                    UsbTransferDirectionOut,
                                    &SetupPacket,
                                    NULL,
                                    0,
                                    &LengthTransferred);

    if (KSUCCESS(Status)) {
        Device->ActiveConfiguration = Configuration;
    }

    KeReleaseQueuedLock(Device->ConfigurationLock);

    //
    // Setting the configuration resets the DATA toggle for every endpoint on
    // the device. See Section 9.1.1.5 of the USB 2.0 Specification.
    //

    if (KSUCCESS(Status)) {
        UsbpResetEndpoint(Device, Device->EndpointZero);
        InterfaceListHead = &(Configuration->Description.InterfaceListHead);
        CurrentInterfaceEntry = InterfaceListHead->Next;
        while (CurrentInterfaceEntry != InterfaceListHead) {
            Interface = LIST_VALUE(CurrentInterfaceEntry,
                                   USB_INTERFACE,
                                   Description.ListEntry);

            CurrentEndpointEntry = Interface->EndpointList.Next;
            CurrentInterfaceEntry = CurrentInterfaceEntry->Next;
            while (CurrentEndpointEntry != &(Interface->EndpointList)) {
                Endpoint = LIST_VALUE(CurrentEndpointEntry,
                                      USB_ENDPOINT,
                                      ListEntry);

                UsbpResetEndpoint(Device, Endpoint);
                CurrentEndpointEntry = CurrentEndpointEntry->Next;
            }
        }
    }

SetConfigurationEnd:
    return Status;
}

USB_API
KSTATUS
UsbClaimInterface (
    HANDLE UsbDeviceHandle,
    UCHAR InterfaceNumber
    )

/*++

Routine Description:

    This routine claims an interface, preparing it for I/O use. An interface
    can be claimed more than once. This routine must be called at low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    InterfaceNumber - Supplies the number of the interface to claim.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION Configuration;
    PLIST_ENTRY CurrentEntry;
    PUSB_DEVICE Device;
    PUSB_ENDPOINT Endpoint;
    PUSB_INTERFACE Interface;
    KSTATUS Status;

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Interface = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Lock the device.
    //

    KeAcquireQueuedLock(Device->ConfigurationLock);

    //
    // If no interface has been set on the device yet, then an interface
    // cannot be claimed.
    //

    Configuration = Device->ActiveConfiguration;
    if (Configuration == NULL) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto ClaimInterfaceEnd;
    }

    //
    // Loop through looking for the requested interface.
    //

    CurrentEntry = Configuration->Description.InterfaceListHead.Next;
    while (CurrentEntry != &(Configuration->Description.InterfaceListHead)) {
        Interface = LIST_VALUE(CurrentEntry,
                               USB_INTERFACE,
                               Description.ListEntry);

        if (Interface->Description.Descriptor.InterfaceNumber ==
                                                             InterfaceNumber) {

            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &(Configuration->Description.InterfaceListHead)) {
        Status = STATUS_NOT_FOUND;
        goto ClaimInterfaceEnd;
    }

    //
    // If the interface isn't supposed to have any endpoints, then finish.
    //

    if (LIST_EMPTY(&(Interface->Description.EndpointListHead)) != FALSE) {
        Status = STATUS_SUCCESS;
        goto ClaimInterfaceEnd;
    }

    //
    // If there are no endpoints yet, they'll have to be created now.
    //

    if (LIST_EMPTY(&(Interface->EndpointList)) != FALSE) {
        Status = UsbpCreateEndpointsForInterface(Device, Interface);
        if (!KSUCCESS(Status)) {
            goto ClaimInterfaceEnd;
        }

    //
    // The endpoints are there, up the reference counts on them.
    //

    } else {
        CurrentEntry = Interface->EndpointList.Next;
        while (CurrentEntry != &(Interface->EndpointList)) {
            Endpoint = LIST_VALUE(CurrentEntry, USB_ENDPOINT, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            UsbpEndpointAddReference(Endpoint);
        }
    }

    Status = STATUS_SUCCESS;

ClaimInterfaceEnd:
    KeReleaseQueuedLock(Device->ConfigurationLock);
    return Status;
}

USB_API
VOID
UsbReleaseInterface (
    HANDLE UsbDeviceHandle,
    UCHAR InterfaceNumber
    )

/*++

Routine Description:

    This routine releases an interface that was previously claimed for I/O.
    After this call, the caller that had claimed the interface should not use
    it again without reclaiming it.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    InterfaceNumber - Supplies the number of the interface to release.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION Configuration;
    PLIST_ENTRY CurrentEntry;
    PUSB_DEVICE Device;
    PUSB_ENDPOINT Endpoint;
    PUSB_INTERFACE Interface;

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Interface = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Lock the device.
    //

    KeAcquireQueuedLock(Device->ConfigurationLock);

    //
    // If no interface has been set on the device yet, then an interface
    // cannot be claimed.
    //

    Configuration = Device->ActiveConfiguration;
    if (Configuration == NULL) {
        goto ClaimInterfaceEnd;
    }

    //
    // Loop through looking for the requested interface.
    //

    CurrentEntry = Configuration->Description.InterfaceListHead.Next;
    while (CurrentEntry != &(Configuration->Description.InterfaceListHead)) {
        Interface = LIST_VALUE(CurrentEntry,
                               USB_INTERFACE,
                               Description.ListEntry);

        if (Interface->Description.Descriptor.InterfaceNumber ==
                                                             InterfaceNumber) {

            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &(Configuration->Description.InterfaceListHead)) {
        goto ClaimInterfaceEnd;
    }

    //
    // If the interface isn't supposed to have any endpoints, then finish.
    //

    if (LIST_EMPTY(&(Interface->Description.EndpointListHead)) != FALSE) {
        goto ClaimInterfaceEnd;
    }

    //
    // Decrement the reference count on each endpoint. It's important to move
    // to the next list entry before releasing the reference, as doing so may
    // cause the endpoint to get unlinked and released.
    //

    CurrentEntry = Interface->EndpointList.Next;
    while (CurrentEntry != &(Interface->EndpointList)) {
        Endpoint = LIST_VALUE(CurrentEntry, USB_ENDPOINT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        UsbpEndpointReleaseReference(Device, Endpoint);
    }

ClaimInterfaceEnd:
    KeReleaseQueuedLock(Device->ConfigurationLock);
    return;
}

USB_API
KSTATUS
UsbSendControlTransfer (
    HANDLE UsbDeviceHandle,
    USB_TRANSFER_DIRECTION TransferDirection,
    PUSB_SETUP_PACKET SetupPacket,
    PVOID Buffer,
    ULONG BufferLength,
    PULONG LengthTransferred
    )

/*++

Routine Description:

    This routine sends a syncrhonous control transfer to or from the given USB
    device.

Arguments:

    UsbDeviceHandle - Supplies a pointer to the device to talk to.

    TransferDirection - Supplies whether or not the transfer is to the device
        or to the host.

    SetupPacket - Supplies a pointer to the setup packet.

    Buffer - Supplies a pointer to the buffer to be sent or received. This does
        not include the setup packet, this is the optional data portion only.

    BufferLength - Supplies the length of the buffer, not including the setup
        packet.

    LengthTransferred - Supplies a pointer where the number of bytes that were
        actually transfered (not including the setup packet) will be returned.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    UINTN BufferAlignment;
    PUSB_DEVICE Device;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    KSTATUS Status;
    PUSB_TRANSFER Transfer;
    PVOID TransferBuffer;
    ULONG TransferLength;

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Transfer = NULL;
    if (LengthTransferred != NULL) {
        *LengthTransferred = 0;
    }

    ASSERT(TransferDirection != UsbTransferDirectionInvalid);

    //
    // Create the I/O buffer that will be used for the transfer.
    //

    TransferLength = BufferLength + sizeof(USB_SETUP_PACKET);
    BufferAlignment = MmGetIoBufferAlignment();
    AllocationSize = ALIGN_RANGE_UP(TransferLength, BufferAlignment);
    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                          MAX_ULONG,
                                          BufferAlignment,
                                          AllocationSize,
                                          IoBufferFlags);

    if (IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendControlTransferEnd;
    }

    ASSERT(IoBuffer->FragmentCount == 1);

    TransferBuffer = IoBuffer->Fragment[0].VirtualAddress;
    RtlCopyMemory(TransferBuffer, SetupPacket, sizeof(USB_SETUP_PACKET));
    if ((TransferDirection == UsbTransferDirectionOut) &&
        (BufferLength != 0)) {

        RtlCopyMemory(TransferBuffer + sizeof(USB_SETUP_PACKET),
                      Buffer,
                      BufferLength);
    }

    //
    // Create a USB transfer.
    //

    Transfer = UsbpAllocateTransfer(Device, 0, AllocationSize, 0);
    if (Transfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendControlTransferEnd;
    }

    Transfer->Direction = TransferDirection;
    Transfer->Length = TransferLength;
    Transfer->Buffer = IoBuffer->Fragment[0].VirtualAddress;
    Transfer->BufferPhysicalAddress = IoBuffer->Fragment[0].PhysicalAddress;
    Transfer->BufferActualLength = IoBuffer->Fragment[0].Size;

    //
    // Submit the transfer and wait for it to complete.
    //

    Status = UsbSubmitSynchronousTransfer(Transfer);
    if (!KSUCCESS(Status)) {
        goto SendControlTransferEnd;
    }

    ASSERT(KSUCCESS(Transfer->Status));

    //
    // Copy the results into the caller's buffer.
    //

    ASSERT(Transfer->LengthTransferred >= sizeof(USB_SETUP_PACKET));
    ASSERT(Transfer->LengthTransferred - sizeof(USB_SETUP_PACKET) <=
           BufferLength);

    if ((TransferDirection == UsbTransferDirectionIn) &&
        (Transfer->LengthTransferred > sizeof(USB_SETUP_PACKET))) {

        if (LengthTransferred != NULL) {
            *LengthTransferred = Transfer->LengthTransferred -
                                 sizeof(USB_SETUP_PACKET);
        }

        RtlCopyMemory(Buffer,
                      Transfer->Buffer + sizeof(USB_SETUP_PACKET),
                      Transfer->LengthTransferred - sizeof(USB_SETUP_PACKET));
    }

    Status = STATUS_SUCCESS;

SendControlTransferEnd:
    if (Transfer != NULL) {
        UsbDestroyTransfer(Transfer);
    }

    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return Status;
}

PUSB_TRANSFER
UsbpAllocateTransfer (
    PUSB_DEVICE Device,
    UCHAR EndpointNumber,
    ULONG MaxTransferSize,
    ULONG Flags
    )

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

{

    ULONG AllocationSize;
    PUSB_HOST_CREATE_TRANSFER CreateTransfer;
    PUSB_HOST_DESTROY_TRANSFER DestroyTransfer;
    PUSB_ENDPOINT Endpoint;
    PVOID HostControllerContext;
    BOOL ReleaseLock;
    KSTATUS Status;
    PUSB_TRANSFER_PRIVATE Transfer;
    BOOL TransferCreated;

    CreateTransfer = Device->Controller->Device.CreateTransfer;
    DestroyTransfer = Device->Controller->Device.DestroyTransfer;
    Endpoint = NULL;
    HostControllerContext = Device->Controller->Device.HostControllerContext;
    ReleaseLock = FALSE;
    Transfer = NULL;
    TransferCreated = FALSE;

    //
    // Add a reference to the device to account for the transfer. This is to
    // potentially allow a driver to roll through the removal IRP destroying
    // everything except for some pending transfer which depends on the USB
    // core. The USB core device will get cleaned up when said transfer get
    // destroyed, releasing this reference.
    //

    UsbpDeviceAddReference(Device);

    //
    // Find the endpoint associated with this transfer.
    //

    Endpoint = UsbpGetDeviceEndpoint(Device, EndpointNumber);
    if (Endpoint == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto AllocateTransferEnd;
    }

    //
    // Allocate the transfer.
    //

    AllocationSize = sizeof(USB_TRANSFER_PRIVATE);
    Transfer = MmAllocateNonPagedPool(AllocationSize, USB_CORE_ALLOCATION_TAG);
    if (Transfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateTransferEnd;
    }

    RtlZeroMemory(Transfer, AllocationSize);
    Transfer->Magic = USB_TRANSFER_INTERNAL_MAGIC;
    Transfer->ReferenceCount = 1;
    Transfer->Device = Device;
    Transfer->Protected.DeviceAddress = Device->BusAddress;
    Transfer->Protected.EndpointNumber = EndpointNumber;
    Transfer->Protected.Type = Endpoint->Type;
    Transfer->MaxTransferSize = MaxTransferSize;
    Transfer->Endpoint = Endpoint;
    Transfer->Protected.Public.Flags = Flags;
    Transfer->Event = KeCreateEvent(NULL);
    if (Transfer->Event == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AllocateTransferEnd;
    }

    ASSERT(Transfer->State == TransferInvalid);
    ASSERT(Transfer->CompletionListEntry.Next == NULL);

    //
    // Don't let a new transfer be created for a disconnected device.
    //

    KeAcquireQueuedLock(Device->Lock);
    ReleaseLock = TRUE;
    if (Device->Connected == FALSE) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto AllocateTransferEnd;
    }

    //
    // Call into the host controller to allocate any of its needed structures.
    //

    Status = CreateTransfer(HostControllerContext,
                            Endpoint->HostControllerContext,
                            MaxTransferSize,
                            Flags,
                            &(Transfer->HostControllerContext));

    if (!KSUCCESS(Status)) {
        goto AllocateTransferEnd;
    }

    //
    // Now that the transfer is successfully created, mark it as inactive and
    // add it to the USB device's list of transfers.
    //

    Transfer->State = TransferInactive;
    INSERT_BEFORE(&(Transfer->DeviceListEntry), &(Device->TransferList));
    KeReleaseQueuedLock(Device->Lock);
    ReleaseLock = FALSE;
    TransferCreated = TRUE;
    Status = STATUS_SUCCESS;

AllocateTransferEnd:
    if (!KSUCCESS(Status)) {
        if (Transfer != NULL) {
            if (TransferCreated != FALSE) {
                DestroyTransfer(HostControllerContext,
                                Endpoint->HostControllerContext,
                                Transfer->HostControllerContext);
            }

            if (Transfer->Event != NULL) {
                KeDestroyEvent(Transfer->Event);
            }

            MmFreeNonPagedPool(Transfer);
            Transfer = NULL;
        }

        if (ReleaseLock != FALSE) {
            KeReleaseQueuedLock(Device->Lock);
        }

        UsbpDeviceReleaseReference(Device);
    }

    return (PUSB_TRANSFER)Transfer;
}

VOID
UsbpCancelAllTransfers (
    PUSB_DEVICE Device
    )

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

{

    PLIST_ENTRY CurrentEntry;
    PUSB_TRANSFER_PRIVATE Transfer;

    ASSERT(Device != NULL);
    ASSERT(Device->Connected == FALSE);
    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Loop through the transfers and add a reference to each. This way the
    // device lock does not need to be held while going through the cancel
    // process, potentially impeding a transfer's ability to fail resubmission.
    //

    KeAcquireQueuedLock(Device->Lock);
    CurrentEntry = Device->TransferList.Next;
    while (CurrentEntry != &(Device->TransferList)) {
        Transfer = (PUSB_TRANSFER_PRIVATE)LIST_VALUE(CurrentEntry,
                                                     USB_TRANSFER_PRIVATE,
                                                     DeviceListEntry);

        UsbTransferAddReference((PUSB_TRANSFER)Transfer);
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Release the lock. It is safe to proceed outside the lock because a
    // reference has been added to each transfer to prevent deletion and
    // because the device has been disconnected, preventing insertion.
    //

    KeReleaseQueuedLock(Device->Lock);

    //
    // Loop through the transfers again and cancel them all.
    //

    CurrentEntry = Device->TransferList.Next;
    while (CurrentEntry != &(Device->TransferList)) {
        Transfer = (PUSB_TRANSFER_PRIVATE)LIST_VALUE(CurrentEntry,
                                                     USB_TRANSFER_PRIVATE,
                                                     DeviceListEntry);

        UsbpCancelTransfer(Transfer);
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Now wait on all transfers to enter the inactive state.
    //

    CurrentEntry = Device->TransferList.Next;
    while (CurrentEntry != &(Device->TransferList)) {
        Transfer = (PUSB_TRANSFER_PRIVATE)LIST_VALUE(CurrentEntry,
                                                     USB_TRANSFER_PRIVATE,
                                                     DeviceListEntry);

        while (Transfer->State != TransferInactive) {
            KeYield();
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Loop one last time, releasing the references. Be aware that this could
    // be the last reference on some transfers, meaning the lock cannot be
    // held because the release could trigger deletion.
    //

    CurrentEntry = Device->TransferList.Next;
    while (CurrentEntry != &(Device->TransferList)) {
        Transfer = (PUSB_TRANSFER_PRIVATE)LIST_VALUE(CurrentEntry,
                                                     USB_TRANSFER_PRIVATE,
                                                     DeviceListEntry);

        CurrentEntry = CurrentEntry->Next;
        UsbTransferReleaseReference((PUSB_TRANSFER)Transfer);
    }

    return;
}

KSTATUS
UsbpReadConfigurationDescriptors (
    PUSB_DEVICE Device,
    PUSB_DEVICE_DESCRIPTOR DeviceDescriptor
    )

/*++

Routine Description:

    This routine attempts to read all configuration descriptors from the device.

Arguments:

    Device - Supplies a pointer to the device to query.

    DeviceDescriptor - Supplies a pointer to the device descriptor.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION Configuration;
    UCHAR ConfigurationCount;
    UCHAR ConfigurationIndex;
    KSTATUS OverallStatus;
    KSTATUS Status;

    OverallStatus = STATUS_SUCCESS;
    ConfigurationCount = DeviceDescriptor->ConfigurationCount;
    for (ConfigurationIndex = 0;
         ConfigurationIndex < ConfigurationCount;
         ConfigurationIndex += 1) {

        Status = UsbpGetConfiguration(Device,
                                      ConfigurationIndex,
                                      TRUE,
                                      &Configuration);

        if (!KSUCCESS(Status)) {
            OverallStatus = Status;
        }
    }

    return OverallStatus;
}

USB_API
PVOID
UsbGetDeviceToken (
    PUSB_DEVICE Device
    )

/*++

Routine Description:

    This routine returns the system device token associated with the given USB
    device.

Arguments:

    Device - Supplies a pointer to a USB device.

Return Value:

    Returns a system device token.

--*/

{

    return Device->Device;
}

KSTATUS
UsbpInitializeTransferCompletionQueue (
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue,
    BOOL PrivateWorkQueue
    )

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

{

    KSTATUS Status;
    ULONG WorkQueueFlags;

    RtlZeroMemory(CompletionQueue, sizeof(USB_TRANSFER_COMPLETION_QUEUE));
    INITIALIZE_LIST_HEAD(&(CompletionQueue->CompletedTransfersList));
    KeInitializeSpinLock(&(CompletionQueue->CompletedTransfersListLock));
    if (PrivateWorkQueue != FALSE) {
        WorkQueueFlags = WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL;
        CompletionQueue->WorkQueue = KeCreateWorkQueue(WorkQueueFlags,
                                                       "UsbCorePrivateWorker");

        if (CompletionQueue->WorkQueue == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeTransferCompletionQueueEnd;
        }

    } else {
        CompletionQueue->WorkQueue = UsbCoreWorkQueue;
    }

    ASSERT(CompletionQueue->WorkQueue != NULL);

    CompletionQueue->WorkItem = KeCreateWorkItem(CompletionQueue->WorkQueue,
                                                 WorkPriorityNormal,
                                                 UsbpCompletedTransferWorker,
                                                 CompletionQueue,
                                                 USB_CORE_ALLOCATION_TAG);

    if (CompletionQueue->WorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeTransferCompletionQueueEnd;
    }

    Status = STATUS_SUCCESS;

InitializeTransferCompletionQueueEnd:
    if (!KSUCCESS(Status)) {
        UsbpDestroyTransferCompletionQueue(CompletionQueue);
    }

    return Status;
}

VOID
UsbpDestroyTransferCompletionQueue (
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue
    )

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

{

    if (CompletionQueue->WorkItem != NULL) {
        KeDestroyWorkItem(CompletionQueue->WorkItem);
    }

    if ((CompletionQueue->WorkQueue != NULL) &&
        (CompletionQueue->WorkQueue != UsbCoreWorkQueue)) {

        KeDestroyWorkQueue(CompletionQueue->WorkQueue);
    }

    return;
}

VOID
UsbpProcessCompletedTransfer (
    PUSB_TRANSFER_INTERNAL Transfer
    )

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

{

    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue;
    PUSB_HOST_CONTROLLER Controller;
    ULONG FlushAlignment;
    ULONG FlushLength;
    RUNLEVEL OldRunLevel;
    USB_TRANSFER_STATE OldState;
    ULONG PrivateFlags;
    BOOL QueueWorkItem;
    KSTATUS Status;

    CompleteTransfer = (PUSB_TRANSFER_PRIVATE)Transfer;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);
    ASSERT(CompleteTransfer->CompletionListEntry.Next == NULL);

    //
    // For any transfer that read data (i.e. all but the out transfers),
    // invalidate the data cache again so that the consumer reads the correct
    // data.
    //

    if (Transfer->Public.Direction != UsbTransferDirectionOut) {

        ASSERT((Transfer->Public.Direction == UsbTransferDirectionIn) ||
               (Transfer->Public.Direction == UsbTransferBidirectional));

        FlushAlignment = MmGetIoBufferAlignment();

        ASSERT(POWER_OF_2(FlushAlignment) != FALSE);

        FlushLength = ALIGN_RANGE_UP(Transfer->Public.LengthTransferred,
                                     FlushAlignment);

        MmFlushBufferForDataIn(Transfer->Public.Buffer, FlushLength);
    }

    //
    // For synchronous transfers, fire the event.
    //

    PrivateFlags = CompleteTransfer->PrivateFlags;
    if ((PrivateFlags & USB_TRANSFER_PRIVATE_SYNCHRONOUS) != 0) {

        //
        // Mark that the transfer is no longer in flight.
        //

        OldState = RtlAtomicCompareExchange32(&(CompleteTransfer->State),
                                              TransferInactive,
                                              TransferActive);

        ASSERT(OldState == TransferActive);

        KeSignalEvent(CompleteTransfer->Event, SignalOptionSignalAll);

        //
        // USB core is done with this transfer, so release the reference taken
        // on submit.
        //

        UsbTransferReleaseReference((PUSB_TRANSFER)Transfer);

    //
    // Queue all non-synchronous transfers to handle the callback at low-level.
    //

    } else {

        //
        // If this is a paging device transfer, then use the paging device
        // completion queue.
        //

        if ((Transfer->Public.Flags & USB_TRANSFER_FLAG_PAGING_DEVICE) != 0) {

            ASSERT(UsbCorePagingCompletionQueue != NULL);

            CompletionQueue = UsbCorePagingCompletionQueue;

        //
        // Otherwise use the controller's completion queue.
        //

        } else {
            Controller = CompleteTransfer->Device->Controller;
            CompletionQueue = &(Controller->TransferCompletionQueue);
        }

        //
        // Add the transfer to the completion list and potentially queue the
        // work item to empty the list.
        //

        OldRunLevel = UsbpAcquireCompletedTransfersLock(CompletionQueue);

        //
        // If the list is currently empty, then the work item needs to be
        // queued to process this new insertion.
        //

        if (LIST_EMPTY(&(CompletionQueue->CompletedTransfersList)) != FALSE) {
            QueueWorkItem = TRUE;

        //
        // If it is not empty, then the work item is already queued and the
        // insertion below will be picked up.
        //

        } else {
            QueueWorkItem = FALSE;
        }

        INSERT_BEFORE(&(CompleteTransfer->CompletionListEntry),
                      &(CompletionQueue->CompletedTransfersList));

        if (QueueWorkItem != FALSE) {
            Status = KeQueueWorkItem(CompletionQueue->WorkItem);

            ASSERT(KSUCCESS(Status));
        }

        UsbpReleaseCompletedTransfersLock(CompletionQueue, OldRunLevel);
    }

    return;
}

USB_API
BOOL
UsbIsPolledIoSupported (
    HANDLE UsbDeviceHandle
    )

/*++

Routine Description:

    This routine returns a boolean indicating whether or not the given USB
    device's controller supports polled I/O mode. Polled I/O should only be
    used in dire circumstances. That is, during system failure when a crash
    dump file needs to be written over USB Mass Storage at high run level with
    interrupts disabled.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

Return Value:

    Returns a boolean indicating if polled I/O is supported (TRUE) or not
    (FALSE).

--*/

{

    PUSB_DEVICE Device;

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    if (Device->Controller->Device.SubmitPolledTransfer != NULL) {
        return TRUE;
    }

    return FALSE;
}

USB_API
KSTATUS
UsbResetEndpoint (
    HANDLE UsbDeviceHandle,
    UCHAR EndpointNumber
    )

/*++

Routine Description:

    This routine resets the given endpoint for the given USB device. This
    includes resetting the data toggle to DATA 0.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    EndpointNumber - Supplies the number of the endpoint to be reset.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    PUSB_ENDPOINT Endpoint;
    KSTATUS Status;

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Endpoint = UsbpGetDeviceEndpoint(Device, EndpointNumber);
    if (Endpoint == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto ResetEndpointEnd;
    }

    UsbpResetEndpoint(Device, Endpoint);
    Status = STATUS_SUCCESS;

ResetEndpointEnd:
    return Status;
}

USB_API
KSTATUS
UsbFlushEndpoint (
    HANDLE UsbDeviceHandle,
    UCHAR EndpointNumber,
    PULONG TransferCount
    )

/*++

Routine Description:

    This routine flushes the given endpoint for the given USB device. This
    includes busily waiting for all active transfers to complete. This is only
    meant to be used at high run level when preparing to write a crash dump
    file using USB Mass Storage.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    EndpointNumber - Supplies the number of the endpoint to be reset.

    TransferCount - Supplies a pointer that receives the total number of
        transfers that were flushed.

Return Value:

    Status code.

--*/

{

    PUSB_DEVICE Device;
    PUSB_ENDPOINT Endpoint;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Device = (PUSB_DEVICE)UsbDeviceHandle;
    Endpoint = UsbpGetDeviceEndpoint(Device, EndpointNumber);
    if (Endpoint == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto FlushEndpointEnd;
    }

    Status = UsbpFlushEndpoint(Device, Endpoint, TransferCount);
    if (!KSUCCESS(Status)) {
        goto FlushEndpointEnd;
    }

FlushEndpointEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UsbpCancelTransfer (
    PUSB_TRANSFER_PRIVATE Transfer
    )

/*++

Routine Description:

    This routine cancels a USB transfer.

Arguments:

    Transfer - Supplies a pointer to the transfer to cancel.

Return Value:

    Status code.

--*/

{

    PUSB_HOST_CANCEL_TRANSFER CancelTransfer;
    PUSB_HOST_CONTROLLER Controller;
    PUSB_ENDPOINT Endpoint;
    KSTATUS Status;

    Endpoint = Transfer->Endpoint;
    Controller = Transfer->Device->Controller;
    CancelTransfer = Controller->Device.CancelTransfer;

    //
    // Try to cancel the transfer. This only makes an attempt at cancelling
    // the transfer and does not guarantee success or that the transfer is
    // out of USB core's domain. The caller needs to handle the various failure
    // cases. If the transfer is currently inactive, just return that the
    // cancel is too early.
    //

    if (Transfer->State == TransferInactive) {
        Status = STATUS_TOO_EARLY;

    } else {
        Status = CancelTransfer(Controller->Device.HostControllerContext,
                                Endpoint->HostControllerContext,
                                (PUSB_TRANSFER_INTERNAL)Transfer,
                                Transfer->HostControllerContext);

        if (!KSUCCESS(Status)) {

            ASSERT(Status == STATUS_TOO_LATE);

        }
    }

    return Status;
}

VOID
UsbpDestroyTransfer (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine destroys an allocated transfer. This transfer must not be
    actively transferring.

Arguments:

    Transfer - Supplies a pointer to the transfer to destroy.

Return Value:

    None.

--*/

{

    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    PUSB_HOST_DESTROY_TRANSFER DestroyTransfer;
    PUSB_HOST_CONTROLLER HostController;
    PVOID HostControllerContext;

    CompleteTransfer = (PUSB_TRANSFER_PRIVATE)Transfer;

    ASSERT(CompleteTransfer->CompletionListEntry.Next == NULL);
    ASSERT(CompleteTransfer->Magic == USB_TRANSFER_INTERNAL_MAGIC);
    ASSERT(CompleteTransfer->State == TransferInactive);

    //
    // Remove the transfer from its USB device's list of transfers.
    //

    KeAcquireQueuedLock(CompleteTransfer->Device->Lock);
    LIST_REMOVE(&(CompleteTransfer->DeviceListEntry));
    KeReleaseQueuedLock(CompleteTransfer->Device->Lock);

    //
    // Call the host controller to destroy the transfer.
    //

    HostController = CompleteTransfer->Device->Controller;
    DestroyTransfer = HostController->Device.DestroyTransfer;
    HostControllerContext = HostController->Device.HostControllerContext;
    DestroyTransfer(HostControllerContext,
                    CompleteTransfer->Endpoint->HostControllerContext,
                    CompleteTransfer->HostControllerContext);

    KeDestroyEvent(CompleteTransfer->Event);

    //
    // Releae the reference the transfer took on the device.
    //

    UsbpDeviceReleaseReference(CompleteTransfer->Device);

    //
    // Destroy the transfer itself.
    //

    MmFreeNonPagedPool(CompleteTransfer);
    return;
}

KSTATUS
UsbpGetConfiguration (
    PUSB_DEVICE Device,
    UCHAR ConfigurationNumber,
    BOOL NumberIsIndex,
    PUSB_CONFIGURATION *Configuration
    )

/*++

Routine Description:

    This routine gets a configuration out of the given device. This routine
    will send a blocking request to the device. This routine must be called at
    low level.

Arguments:

    Device - Supplies a pointer to the device.

    ConfigurationNumber - Supplies the index or configuration value of the
        configuration to get.

    NumberIsIndex - Supplies a boolean indicating whether the configuration
        number is an index (TRUE) or a specific configuration value (FALSE).

    Configuration - Supplies a pointer where a pointer to the created
        configuration will be returned.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PUCHAR BufferPointer;
    PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor;
    UCHAR ConfigurationValue;
    PUSB_CONFIGURATION CurrentConfiguration;
    PLIST_ENTRY CurrentEntry;
    PUSB_INTERFACE CurrentInterface;
    PUSB_CONFIGURATION_DESCRIPTION Description;
    UCHAR DescriptorLength;
    UCHAR DescriptorType;
    PUSB_ENDPOINT_DESCRIPTION Endpoint;
    ULONG EndpointCount;
    ULONG InterfaceCount;
    ULONG Length;
    ULONG LengthTransferred;
    PUCHAR NewBufferPointer;
    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;
    USHORT TotalLength;
    PUSB_UNKNOWN_DESCRIPTION Unknown;
    ULONG UnknownCount;
    ULONG UnknownSize;

    *Configuration = NULL;
    ConfigurationDescriptor = NULL;
    CurrentConfiguration = NULL;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(Device->ConfigurationLock);

    //
    // First look to see if the configuration already exists.
    //

    CurrentEntry = Device->ConfigurationList.Next;
    while (CurrentEntry != &(Device->ConfigurationList)) {
        CurrentConfiguration = LIST_VALUE(CurrentEntry,
                                          USB_CONFIGURATION,
                                          ListEntry);

        //
        // Match on either the index or the value.
        //

        Description = &(CurrentConfiguration->Description);
        if (NumberIsIndex != FALSE) {
            if (ConfigurationNumber == Description->Index) {
                break;
            }

        } else {
            ConfigurationValue = Description->Descriptor.ConfigurationValue;
            if (ConfigurationNumber == ConfigurationValue) {
                break;
            }
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry != &(Device->ConfigurationList)) {
        Status = STATUS_SUCCESS;
        goto GetConfigurationEnd;
    }

    CurrentConfiguration = NULL;

    //
    // The USB spec does not support requesting descriptors by value, so this
    // had better be a "by-index" request.
    //

    ASSERT(NumberIsIndex != FALSE);

    //
    // Allocate space for the entire descriptor, which includes all of the
    // interface and endpoint descriptors (hopefully).
    //

    ConfigurationDescriptor = MmAllocatePagedPool(
                                              USB_INITIAL_CONFIGURATION_LENGTH,
                                              USB_CORE_ALLOCATION_TAG);

    if (ConfigurationDescriptor == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetConfigurationEnd;
    }

    //
    // Read in the configuration descriptor.
    //

    RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
    SetupPacket.RequestType = USB_SETUP_REQUEST_TO_HOST |
                              USB_SETUP_REQUEST_STANDARD |
                              USB_SETUP_REQUEST_DEVICE_RECIPIENT;

    SetupPacket.Request = USB_DEVICE_REQUEST_GET_DESCRIPTOR;
    SetupPacket.Value = (UsbDescriptorTypeConfiguration << 8) |
                        ConfigurationNumber;

    SetupPacket.Index = 0;
    SetupPacket.Length = USB_INITIAL_CONFIGURATION_LENGTH;
    Status = UsbSendControlTransfer(Device,
                                    UsbTransferDirectionIn,
                                    &SetupPacket,
                                    ConfigurationDescriptor,
                                    USB_INITIAL_CONFIGURATION_LENGTH,
                                    &LengthTransferred);

    if (!KSUCCESS(Status)) {
        goto GetConfigurationEnd;
    }

    if (LengthTransferred < sizeof(USB_CONFIGURATION_DESCRIPTOR)) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto GetConfigurationEnd;
    }

    //
    // If the buffer was too small, allocate a bigger one and read it in again.
    //

    TotalLength = ConfigurationDescriptor->TotalLength;
    if (TotalLength > USB_INITIAL_CONFIGURATION_LENGTH) {
        MmFreePagedPool(ConfigurationDescriptor);
        ConfigurationDescriptor = MmAllocatePagedPool(TotalLength,
                                                      USB_CORE_ALLOCATION_TAG);

        if (ConfigurationDescriptor == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetConfigurationEnd;
        }

        SetupPacket.Length = TotalLength;
        Status = UsbSendControlTransfer(Device,
                                        UsbTransferDirectionIn,
                                        &SetupPacket,
                                        ConfigurationDescriptor,
                                        TotalLength,
                                        &LengthTransferred);

        if (!KSUCCESS(Status)) {
            goto GetConfigurationEnd;
        }

        if (LengthTransferred != TotalLength) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto GetConfigurationEnd;
        }
    }

    //
    // Count the number of interfaces and endpoints to determine the allocation
    // size for the description.
    //

    InterfaceCount = 0;
    EndpointCount = 0;
    UnknownCount = 0;
    UnknownSize = 0;
    Length = ConfigurationDescriptor->Length;
    BufferPointer = (PUCHAR)ConfigurationDescriptor +
                    ConfigurationDescriptor->Length;

    while (Length + 1 < LengthTransferred) {

        //
        // Get this descriptor and count it.
        //

        DescriptorLength = *BufferPointer;
        DescriptorType = *(BufferPointer + 1);
        if (DescriptorType == UsbDescriptorTypeInterface) {
            InterfaceCount += 1;

        } else if (DescriptorType == UsbDescriptorTypeEndpoint) {
            EndpointCount += 1;

        } else {
            UnknownCount += 1;
            UnknownSize += DescriptorLength + sizeof(ULONGLONG) - 1;
        }

        //
        // Move on to the next descriptor.
        //

        BufferPointer += DescriptorLength;
        Length += DescriptorLength;
    }

    //
    // Now allocate space for the configuration description.
    //

    AllocationSize = sizeof(USB_CONFIGURATION) +
                     (InterfaceCount * sizeof(USB_INTERFACE)) +
                     (EndpointCount * sizeof(USB_ENDPOINT_DESCRIPTION)) +
                     (UnknownCount * sizeof(USB_UNKNOWN_DESCRIPTION)) +
                     UnknownSize;

    CurrentConfiguration = MmAllocatePagedPool(AllocationSize,
                                               USB_CORE_ALLOCATION_TAG);

    if (CurrentConfiguration == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetConfigurationEnd;
    }

    RtlZeroMemory(CurrentConfiguration, AllocationSize);
    CurrentConfiguration->Description.Index = ConfigurationNumber;
    RtlCopyMemory(&(CurrentConfiguration->Description.Descriptor),
                  ConfigurationDescriptor,
                  sizeof(USB_CONFIGURATION_DESCRIPTOR));

    INITIALIZE_LIST_HEAD(
                       &(CurrentConfiguration->Description.InterfaceListHead));

    //
    // Go through the descriptor again and create analogous structures for them.
    //

    CurrentInterface = NULL;
    Length = ConfigurationDescriptor->Length;
    BufferPointer = (PUCHAR)ConfigurationDescriptor +
                    ConfigurationDescriptor->Length;

    NewBufferPointer = (PUCHAR)(CurrentConfiguration + 1);
    while (Length + 1 < LengthTransferred) {

        //
        // Get this descriptor and create the analogous structure.
        //

        DescriptorLength = *BufferPointer;
        DescriptorType = *(BufferPointer + 1);
        if (Length + DescriptorLength > LengthTransferred) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto GetConfigurationEnd;
        }

        if (DescriptorType == UsbDescriptorTypeInterface) {
            CurrentInterface = (PUSB_INTERFACE)NewBufferPointer;
            if (DescriptorLength < sizeof(USB_INTERFACE_DESCRIPTOR)) {
                Status = STATUS_INVALID_CONFIGURATION;
                goto GetConfigurationEnd;
            }

            RtlCopyMemory(&(CurrentInterface->Description.Descriptor),
                          BufferPointer,
                          sizeof(USB_INTERFACE_DESCRIPTOR));

            INITIALIZE_LIST_HEAD(
                            &(CurrentInterface->Description.EndpointListHead));

            INITIALIZE_LIST_HEAD(
                            &(CurrentInterface->Description.UnknownListHead));

            INITIALIZE_LIST_HEAD(&(CurrentInterface->EndpointList));
            INSERT_BEFORE(
                       &(CurrentInterface->Description.ListEntry),
                       &(CurrentConfiguration->Description.InterfaceListHead));

            NewBufferPointer = (PUCHAR)(CurrentInterface + 1);

        } else if (DescriptorType == UsbDescriptorTypeEndpoint) {

            //
            // If an endpoint came with no interface, that's illegal.
            //

            if (CurrentInterface == NULL) {
                Status = STATUS_INVALID_CONFIGURATION;
                goto GetConfigurationEnd;
            }

            Endpoint = (PUSB_ENDPOINT_DESCRIPTION)NewBufferPointer;
            if (DescriptorLength < sizeof(USB_ENDPOINT_DESCRIPTOR)) {
                Status = STATUS_INVALID_CONFIGURATION;
                goto GetConfigurationEnd;
            }

            RtlCopyMemory(&(Endpoint->Descriptor),
                          BufferPointer,
                          sizeof(USB_ENDPOINT_DESCRIPTOR));

            INSERT_BEFORE(&(Endpoint->ListEntry),
                          &(CurrentInterface->Description.EndpointListHead));

            NewBufferPointer = (PUCHAR)(Endpoint + 1);

        //
        // Add an unknown descriptor to the interface if there is one. HID
        // descriptors are nestled in this way.
        //

        } else {
            if (CurrentInterface != NULL) {
                Unknown = (PUSB_UNKNOWN_DESCRIPTION)NewBufferPointer;
                Unknown->Descriptor = (PUCHAR)(Unknown + 1);
                RtlCopyMemory(Unknown->Descriptor,
                              BufferPointer,
                              DescriptorLength);

                INSERT_BEFORE(&(Unknown->ListEntry),
                              &(CurrentInterface->Description.UnknownListHead));

                NewBufferPointer =
                     ALIGN_POINTER_UP(Unknown->Descriptor + DescriptorLength,
                                      sizeof(ULONGLONG));
            }
        }

        //
        // Move on to the next descriptor.
        //

        BufferPointer += DescriptorLength;
        Length += DescriptorLength;
    }

    ASSERT((UINTN)NewBufferPointer - (UINTN)CurrentConfiguration <=
                                                               AllocationSize);

    //
    // Insert the new configuration onto the global list to cache it for
    // future calls.
    //

    INSERT_BEFORE(&(CurrentConfiguration->ListEntry),
                  &(Device->ConfigurationList));

GetConfigurationEnd:
    KeReleaseQueuedLock(Device->ConfigurationLock);
    if (!KSUCCESS(Status)) {
        if (CurrentConfiguration != NULL) {
            MmFreePagedPool(CurrentConfiguration);
            CurrentConfiguration = NULL;
        }
    }

    if (ConfigurationDescriptor != NULL) {
        MmFreePagedPool(ConfigurationDescriptor);
    }

    *Configuration = CurrentConfiguration;
    return Status;
}

KSTATUS
UsbpSubmitTransfer (
    PUSB_TRANSFER Transfer,
    ULONG PrivateFlags,
    BOOL PolledMode
    )

/*++

Routine Description:

    This routine submits a USB transfer. The routine returns immediately,
    indicating only whether the transfer was submitted successfully. When the
    transfer actually completes, the callback routine will be called.

Arguments:

    Transfer - Supplies a pointer to the transfer to destroy.

    PrivateFlags - Supplies an optional bitfield of private flags regarding the
        transfer. See USB_TRANSFER_PRIVATE_* definitions.

    PolledMode - Supplies a boolean indicating whether the I/O should be done
        in polled mode or not. This is reserved for I/O paths after a critical
        system error.

Return Value:

    STATUS_SUCCESS if the transfer was submitted to the USB host controller's
    queue.

    STATUS_INVALID_PARAMETER if one or more of the transfer fields is not
        properly filled out.

    Failing status codes if the request could not be submitted.

--*/

{

    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    PUSB_HOST_CONTROLLER Controller;
    PUSB_DEVICE Device;
    PUSB_ENDPOINT Endpoint;
    ULONG FlushAlignment;
    ULONG FlushLength;
    USB_TRANSFER_STATE OriginalState;
    BOOL PacketQueued;
    BOOL ReleaseDeviceLock;
    PUSB_SETUP_PACKET Setup;
    KSTATUS Status;
    PUSB_HOST_SUBMIT_TRANSFER SubmitTransfer;

    ASSERT(Transfer != NULL);

    CompleteTransfer = (PUSB_TRANSFER_PRIVATE)Transfer;
    Endpoint = CompleteTransfer->Endpoint;
    Controller = CompleteTransfer->Device->Controller;
    Device = (PUSB_DEVICE)CompleteTransfer->Device;
    PacketQueued = FALSE;
    ReleaseDeviceLock = FALSE;

    //
    // Reference the transfer.
    //

    UsbTransferAddReference(Transfer);

    //
    // Callers are not allowed to allocate their own transfer structures, nor
    // are they allowed to resubmit packets that have not completed.
    //

    if (CompleteTransfer->Magic != USB_TRANSFER_INTERNAL_MAGIC) {

        ASSERT(FALSE);

        Transfer->Error = UsbErrorTransferAllocatedIncorrectly;
        Status = STATUS_INVALID_PARAMETER;
        goto SubmitTransferEnd;
    }

    //
    // Also fail if a transfer is submitted while it is still in-flight. It
    // should either be inactive or in the middle of the callback.
    //

    if (CompleteTransfer->State == TransferActive) {

        ASSERT(FALSE);

        Transfer->Error = UsbErrorTransferSubmittedWhileStillActive;
        Status = STATUS_RESOURCE_IN_USE;
        goto SubmitTransferEnd;
    }

    ASSERT(CompleteTransfer->CompletionListEntry.Next == NULL);

    //
    // Validate the transfer.
    //

    if ((Transfer->Length == 0) ||
        (Transfer->Length > CompleteTransfer->MaxTransferSize) ||
        (Transfer->Buffer == NULL) ||
        (Transfer->BufferPhysicalAddress == INVALID_PHYSICAL_ADDRESS) ||
        (Transfer->BufferActualLength < Transfer->Length) ||
        ((Transfer->Direction != UsbTransferDirectionIn) &&
         (Transfer->Direction != UsbTransferDirectionOut))) {

        ASSERT(FALSE);

        Transfer->Error = UsbErrorTransferIncorrectlyFilledOut;
        Status = STATUS_INVALID_PARAMETER;
        goto SubmitTransferEnd;
    }

    if ((PrivateFlags & USB_TRANSFER_PRIVATE_SYNCHRONOUS) != 0) {
        CompleteTransfer->PrivateFlags |= USB_TRANSFER_PRIVATE_SYNCHRONOUS;

    } else {
        CompleteTransfer->PrivateFlags &= ~USB_TRANSFER_PRIVATE_SYNCHRONOUS;
        if (Transfer->CallbackRoutine == NULL) {
            Transfer->Error = UsbErrorTransferIncorrectlyFilledOut;
            Status = STATUS_INVALID_PARAMETER;
            goto SubmitTransferEnd;
        }
    }

    Transfer->Status = STATUS_NOT_STARTED;
    Transfer->Error = UsbErrorTransferNotStarted;
    Transfer->LengthTransferred = 0;
    if (PolledMode == FALSE) {
        SubmitTransfer = Controller->Device.SubmitTransfer;

        ASSERT(SubmitTransfer != NULL);

    } else {
        SubmitTransfer = Controller->Device.SubmitPolledTransfer;
        if (SubmitTransfer == NULL) {
            Status = STATUS_NOT_SUPPORTED;
            goto SubmitTransferEnd;
        }
    }

    //
    // Clean the data buffer in preparation for the USB controller doing DMA
    // to/from it. Control transfers always have an outgoing portion.
    //

    FlushAlignment = MmGetIoBufferAlignment();

    ASSERT(POWER_OF_2(FlushAlignment) != FALSE);

    FlushLength = ALIGN_RANGE_UP(Transfer->Length, FlushAlignment);
    if ((ALIGN_RANGE_DOWN((UINTN)Transfer->Buffer, FlushAlignment) !=
         (UINTN)Transfer->Buffer) ||
        (FlushLength > Transfer->BufferActualLength)) {

        ASSERT(FALSE);

        Transfer->Error = UsbErrorTransferBufferNotAligned;
        Status = STATUS_INVALID_PARAMETER;
        Transfer->Status = Status;
        goto SubmitTransferEnd;
    }

    //
    // Print out any debug information. The transfer isn't guaranteed to be
    // submitted after this point, but this touches the transfer buffer, which
    // needs to be flushed and then not touched.
    //

    if ((UsbDebugFlags & USB_DEBUG_TRANSFERS) != 0) {
        if ((UsbDebugDeviceAddress == 0) ||
            (UsbDebugDeviceAddress ==
             CompleteTransfer->Protected.DeviceAddress)) {

            ASSERT(Transfer->Direction < UsbTransferDirectionCount);
            ASSERT(CompleteTransfer->Protected.Type < UsbTransferTypeCount);

            RtlDebugPrint(
                     "USB: Transfer (0x%08x) %s dev %d, EP%x, %s, "
                     "Buffer 0x%x, Length 0x%x\n",
                     Transfer,
                     UsbTransferDirectionStrings[Transfer->Direction],
                     CompleteTransfer->Protected.DeviceAddress,
                     CompleteTransfer->Protected.EndpointNumber,
                     UsbTransferTypeStrings[CompleteTransfer->Protected.Type],
                     Transfer->Buffer,
                     Transfer->Length);

            if (CompleteTransfer->Protected.Type == UsbTransferTypeControl) {

                ASSERT(Transfer->Length >= sizeof(USB_SETUP_PACKET));

                Setup = Transfer->Buffer;
                RtlDebugPrint("USB: RequestType 0x%x, Request 0x%x, "
                              "Value 0x%x, Index 0x%x, Length 0x%x\n",
                              Setup->RequestType,
                              Setup->Request,
                              Setup->Value,
                              Setup->Index,
                              Setup->Length);
            }
        }
    }

    //
    // Flush the transfer buffer. Do not access the buffer beyond this point.
    //

    if (CompleteTransfer->Endpoint->Type == UsbTransferTypeControl) {
        if (Transfer->Direction == UsbTransferDirectionOut) {
            MmFlushBufferForDataOut(Transfer->Buffer, FlushLength);

        } else {
            MmFlushBufferForDataIo(Transfer->Buffer, FlushLength);
        }

    //
    // Bulk, interrupt, and isochronous transfers really only go the direction
    // they claim.
    //

    } else {
        if (Transfer->Direction == UsbTransferDirectionOut) {
            MmFlushBufferForDataOut(Transfer->Buffer, FlushLength);

        } else {

            ASSERT(Transfer->Direction == UsbTransferDirectionIn);

            MmFlushBufferForDataIn(Transfer->Buffer, FlushLength);
        }
    }

    //
    // Acquire the USB device's lock to check the status. Transfers should not
    // be submitted to disconnected devices.
    //

    if (PolledMode == FALSE) {
        KeAcquireQueuedLock(Device->Lock);
        ReleaseDeviceLock = TRUE;
    }

    if (Device->Connected == FALSE) {
        Transfer->Error = UsbErrorTransferDeviceNotConnected;
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto SubmitTransferEnd;
    }

    //
    // Update the transfer state to 'active' before submission to the host
    // controller. This could be a transition from either the callback state
    // or the inactive state.
    //

    OriginalState = RtlAtomicCompareExchange32(&(CompleteTransfer->State),
                                               TransferActive,
                                               TransferInCallback);

    if (OriginalState != TransferInCallback) {
        OriginalState = RtlAtomicCompareExchange32(&(CompleteTransfer->State),
                                                   TransferActive,
                                                   TransferInactive);

        if (OriginalState != TransferInactive) {
            KeCrashSystem(CRASH_USB_ERROR,
                          UsbErrorTransferSubmittedWhileStillActive,
                          (UINTN)Transfer,
                          CompleteTransfer->State,
                          0);
        }
    }

    //
    // Submit the transfer to the host controller.
    //

    Status = SubmitTransfer(Controller->Device.HostControllerContext,
                            Endpoint->HostControllerContext,
                            &(CompleteTransfer->Protected),
                            CompleteTransfer->HostControllerContext);

    if (!KSUCCESS(Status)) {
        Transfer->Error = UsbErrorTransferFailedToSubmit;

        //
        // Flip the transfer state to inactive, always.
        //

        OriginalState = RtlAtomicCompareExchange32(&(CompleteTransfer->State),
                                                   TransferInactive,
                                                   TransferActive);

        ASSERT(OriginalState == TransferActive);

        goto SubmitTransferEnd;
    }

    if (PolledMode == FALSE) {
        KeReleaseQueuedLock(Device->Lock);
        ReleaseDeviceLock = FALSE;
    }

    PacketQueued = TRUE;
    Status = STATUS_SUCCESS;

SubmitTransferEnd:
    if (!KSUCCESS(Status)) {

        //
        // Release the device lock, if necessary.
        //

        if (ReleaseDeviceLock != FALSE) {

            ASSERT(PolledMode == FALSE);

            KeReleaseQueuedLock(Device->Lock);
        }

        //
        // Report transfer failures.
        //

        if ((UsbDebugFlags & (USB_DEBUG_TRANSFERS | USB_DEBUG_ERRORS)) != 0) {
            if ((UsbDebugDeviceAddress == 0) ||
                (UsbDebugDeviceAddress ==
                 CompleteTransfer->Protected.DeviceAddress)) {

            RtlDebugPrint(
                     "USB: Submit failed, transfer (0x%08x) %s "
                     "dev %d, EP%x, %s, Buffer 0x%x, Len 0x%x. Status %d\n",
                     Transfer,
                     UsbTransferDirectionStrings[Transfer->Direction],
                     CompleteTransfer->Protected.DeviceAddress,
                     CompleteTransfer->Protected.EndpointNumber,
                     UsbTransferTypeStrings[CompleteTransfer->Protected.Type],
                     Transfer->Buffer,
                     Transfer->Length,
                     Status);
            }
        }

        //
        // Upon failure, cancel the transfer if it was submitted. This will
        // modify the transfer state. Also, it could fail if the transfer went
        // through very quickly. This, however, is not currently a valid error
        // path - just future proofing.
        //

        if (PacketQueued != FALSE) {
            UsbCancelTransfer(Transfer, TRUE);

        //
        // Relese the reference on failure. If the cancel path was taken, then
        // the reference will be released after the callback. Also set the
        // transfer status here; the cancel path does that as well.
        //

        } else {
            Transfer->Status = Status;
            UsbTransferReleaseReference(Transfer);
        }
    }

    return Status;
}

KSTATUS
UsbpCreateEndpointsForInterface (
    PUSB_DEVICE Device,
    PUSB_INTERFACE Interface
    )

/*++

Routine Description:

    This routine creates endpoints for the given interface.

Arguments:

    Device - Supplies a pointer to the device owning the interface and
        endpoints.

    Interface - Supplies a pointer to the interface to create endpoints for.

Return Value:

    Status code.

--*/

{

    UCHAR Attributes;
    PLIST_ENTRY CurrentEntry;
    USB_TRANSFER_DIRECTION Direction;
    PUSB_ENDPOINT Endpoint;
    PUSB_ENDPOINT_DESCRIPTION EndpointDescription;
    UCHAR EndpointNumber;
    ULONG MaxPacketSize;
    ULONG PollRate;
    KSTATUS Status;
    USB_TRANSFER_TYPE Type;

    //
    // Loop through all the endpoint descriptions.
    //

    CurrentEntry = Interface->Description.EndpointListHead.Next;
    while (CurrentEntry != &(Interface->Description.EndpointListHead)) {
        EndpointDescription = LIST_VALUE(CurrentEntry,
                                         USB_ENDPOINT_DESCRIPTION,
                                         ListEntry);

        CurrentEntry = CurrentEntry->Next;
        PollRate = 0;

        //
        // Get the endpoint number.
        //

        EndpointNumber = EndpointDescription->Descriptor.EndpointAddress;

        //
        // Get the endpoint type.
        //

        Attributes = EndpointDescription->Descriptor.Attributes;
        switch (Attributes & USB_ENDPOINT_ATTRIBUTES_TYPE_MASK) {
        case USB_ENDPOINT_ATTRIBUTES_TYPE_CONTROL:
            Type = UsbTransferTypeControl;
            break;

        case USB_ENDPOINT_ATTRIBUTES_TYPE_ISOCHRONOUS:
            Type = UsbTransferTypeIsochronous;
            PollRate = EndpointDescription->Descriptor.Interval;
            break;

        case USB_ENDPOINT_ATTRIBUTES_TYPE_BULK:
            Type = UsbTransferTypeBulk;
            if ((EndpointNumber & USB_ENDPOINT_ADDRESS_DIRECTION_IN) == 0) {
                PollRate = EndpointDescription->Descriptor.Interval;
            }

            break;

        case USB_ENDPOINT_ATTRIBUTES_TYPE_INTERRUPT:
        default:
            Type = UsbTransferTypeInterrupt;
            PollRate = EndpointDescription->Descriptor.Interval;
            break;
        }

        //
        // Get the direction.
        //

        if (Type == UsbTransferTypeControl) {
            Direction = UsbTransferBidirectional;

        } else {
            Direction = UsbTransferDirectionOut;
            if ((EndpointNumber & USB_ENDPOINT_ADDRESS_DIRECTION_IN) != 0) {
                Direction = UsbTransferDirectionIn;
            }
        }

        MaxPacketSize = EndpointDescription->Descriptor.MaxPacketSize;
        Status = UsbpCreateEndpoint(Device,
                                    EndpointNumber,
                                    Direction,
                                    Type,
                                    MaxPacketSize,
                                    PollRate,
                                    &Endpoint);

        if (!KSUCCESS(Status)) {
            goto CreateEndpointsForInterfaceEnd;
        }

        INSERT_BEFORE(&(Endpoint->ListEntry), &(Interface->EndpointList));
    }

    Status = STATUS_SUCCESS;

CreateEndpointsForInterfaceEnd:
    if (!KSUCCESS(Status)) {

        //
        // Loop through and free any endpoints that were created.
        //

        CurrentEntry = Interface->EndpointList.Next;
        while (CurrentEntry != &(Interface->EndpointList)) {
            Endpoint = LIST_VALUE(CurrentEntry, USB_ENDPOINT, ListEntry);
            CurrentEntry = CurrentEntry->Next;

            ASSERT(Endpoint->ReferenceCount == 1);

            UsbpEndpointReleaseReference(Device, Endpoint);
        }
    }

    return Status;
}

PUSB_ENDPOINT
UsbpGetDeviceEndpoint (
    PUSB_DEVICE Device,
    UCHAR EndpointNumber
    )

/*++

Routine Description:

    This routine looks up a USB endpoint for the given device.

Arguments:

    Device - Supplies a pointer to a USB device.

    EndpointNumber - Supplies the number of the desired endpoint.

Return Value:

    Returns a pointer to a USB endpoint on success, or NULL if the given
    endpoint does not exist for the given device.

--*/

{

    PUSB_CONFIGURATION ActiveConfiguration;
    PLIST_ENTRY CurrentEndpointEntry;
    PLIST_ENTRY CurrentInterfaceEntry;
    PUSB_ENDPOINT Endpoint;
    PUSB_INTERFACE Interface;
    PLIST_ENTRY InterfaceListHead;

    //
    // Endpoint zero is easy to retrieve.
    //

    if (EndpointNumber == 0) {
        return Device->EndpointZero;
    }

    //
    // Run through the list of interfaces and associated endpoints to find
    // non-zero endpoints.
    //

    ASSERT(Device->ActiveConfiguration != NULL);

    ActiveConfiguration = Device->ActiveConfiguration;
    InterfaceListHead = &(ActiveConfiguration->Description.InterfaceListHead);
    CurrentInterfaceEntry = InterfaceListHead->Next;
    Endpoint = NULL;
    while (CurrentInterfaceEntry != InterfaceListHead) {
        Interface = LIST_VALUE(CurrentInterfaceEntry,
                               USB_INTERFACE,
                               Description.ListEntry);

        CurrentEndpointEntry = Interface->EndpointList.Next;
        CurrentInterfaceEntry = CurrentInterfaceEntry->Next;
        while (CurrentEndpointEntry != &(Interface->EndpointList)) {
            Endpoint = LIST_VALUE(CurrentEndpointEntry,
                                  USB_ENDPOINT,
                                  ListEntry);

            CurrentEndpointEntry = CurrentEndpointEntry->Next;
            if (Endpoint->Number == EndpointNumber) {
                break;
            }
        }

        if ((Endpoint != NULL) && (Endpoint->Number == EndpointNumber)) {
            break;
        }
    }

    return Endpoint;
}

VOID
UsbpCompletedTransferWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes completed USB transfers.

Arguments:

    Parameter - Supplies a pointer to the USB host controller.

Return Value:

    None.

--*/

{

    PUSB_TRANSFER_CALLBACK CallbackRoutine;
    PUSB_TRANSFER_PRIVATE CompleteTransfer;
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue;
    PLIST_ENTRY CurrentEntry;
    RUNLEVEL OldRunLevel;
    USB_TRANSFER_STATE OldState;
    LIST_ENTRY TransferList;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    CompletionQueue = (PUSB_TRANSFER_COMPLETION_QUEUE)Parameter;

    //
    // Acquire the lock and pull all transfers off of the list. Once the list
    // is empty and the lock is released, other completed transfers will know
    // that the work item needs to be queued.
    //

    ASSERT(LIST_EMPTY(&(CompletionQueue->CompletedTransfersList)) == FALSE);

    OldRunLevel = UsbpAcquireCompletedTransfersLock(CompletionQueue);
    MOVE_LIST(&(CompletionQueue->CompletedTransfersList), &TransferList);
    INITIALIZE_LIST_HEAD(&(CompletionQueue->CompletedTransfersList));
    UsbpReleaseCompletedTransfersLock(CompletionQueue, OldRunLevel);

    //
    // Now that the lock is released and execution is at low level, process the
    // work items.
    //

    while (LIST_EMPTY(&TransferList) == FALSE) {
        CurrentEntry = TransferList.Next;
        LIST_REMOVE(CurrentEntry);
        CompleteTransfer = LIST_VALUE(CurrentEntry,
                                      USB_TRANSFER_PRIVATE,
                                      CompletionListEntry);

        ASSERT(CompleteTransfer->Magic == USB_TRANSFER_INTERNAL_MAGIC);

        //
        // Mark that the transfer is no longer in flight, but in the callback.
        //

        OldState = RtlAtomicCompareExchange32(&(CompleteTransfer->State),
                                              TransferInCallback,
                                              TransferActive);

        ASSERT(OldState == TransferActive);

        //
        // Call the callback routine.
        //

        CompleteTransfer->CompletionListEntry.Next = NULL;
        CallbackRoutine = CompleteTransfer->Protected.Public.CallbackRoutine;
        CallbackRoutine(&(CompleteTransfer->Protected.Public));

        //
        // If the callback did not resubmit the transfer, then move it to the
        // inactive state. See the submit routine for how this change
        // synchronizes with re-submits that are outside the callback (e.g. in
        // a work item).
        //

        RtlAtomicCompareExchange32(&(CompleteTransfer->State),
                                   TransferInactive,
                                   TransferInCallback);

        //
        // Once the callback is called, USB core is done with this transfer;
        // release the reference taken during submit.
        //

        UsbTransferReleaseReference((PUSB_TRANSFER)CompleteTransfer);
    }

    return;
}

RUNLEVEL
UsbpAcquireCompletedTransfersLock (
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue
    )

/*++

Routine Description:

    This routine acquires the given completion queue's completed transfers lock
    at dispatch level.

Arguments:

    CompletionQueue - Supplies a pointer to the completion queue to lock.

Return Value:

    Returns the previous run-level, which must be passed in when the completion
    queue is unlocked.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(CompletionQueue->CompletedTransfersListLock));
    return OldRunLevel;
}

VOID
UsbpReleaseCompletedTransfersLock (
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue,
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the given completion queue's completed transfers
    lock, and returns the run-level to its previous value.

Arguments:

    CompletionQueue - Supplies a pointer to the completion queue to unlock.

    OldRunLevel - Supplies the original run level returned when the lock was
        acquired.

Return Value:

    None.

--*/

{

    KeReleaseSpinLock(&(CompletionQueue->CompletedTransfersListLock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

