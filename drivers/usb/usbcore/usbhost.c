/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbhost.c

Abstract:

    This module implements USB host controller support routines.

Author:

    Evan Green 16-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "usbcore.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
UsbpDestroyEndpoint (
    PUSB_DEVICE Device,
    PUSB_ENDPOINT Endpoint
    );

VOID
UsbpPortStatusChangeWorker (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

USB_API
KSTATUS
UsbHostRegisterController (
    PUSB_HOST_CONTROLLER_INTERFACE ControllerInterface,
    PHANDLE ControllerHandle
    )

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

{

    UCHAR Address;
    PUSB_TRANSFER_COMPLETION_QUEUE CompletionQueue;
    PUSB_HOST_CONTROLLER Controller;
    KSTATUS Status;

    //
    // Surely DriverEntry must have been called to initialize this list head.
    //

    ASSERT(UsbHostControllerList.Next != NULL);

    Controller = NULL;
    if (UsbHostControllerListLock == NULL) {
        Status = STATUS_NOT_READY;
        goto HostRegisterControllerEnd;
    }

    //
    // Validate parameters.
    //

    if ((ControllerInterface == NULL) || (ControllerHandle == NULL)) {
        Status = STATUS_INVALID_PARAMETER;
        goto HostRegisterControllerEnd;
    }

    if (ControllerInterface->Version < USB_HOST_CONTROLLER_INTERFACE_VERSION) {
        Status = STATUS_NOT_SUPPORTED;
        goto HostRegisterControllerEnd;
    }

    if ((ControllerInterface->DriverObject == NULL) ||
        (ControllerInterface->DeviceObject == NULL) ||
        (ControllerInterface->CreateEndpoint == NULL) ||
        (ControllerInterface->DestroyEndpoint == NULL) ||
        (ControllerInterface->CreateTransfer == NULL) ||
        (ControllerInterface->DestroyTransfer == NULL) ||
        (ControllerInterface->SubmitTransfer == NULL) ||
        (ControllerInterface->CancelTransfer == NULL) ||
        (ControllerInterface->GetRootHubStatus == NULL) ||
        (ControllerInterface->SetRootHubStatus == NULL) ||
        (ControllerInterface->Speed == UsbDeviceSpeedInvalid) ||
        (ControllerInterface->RootHubPortCount == 0)) {

        Status = STATUS_INVALID_PARAMETER;
        goto HostRegisterControllerEnd;
    }

    //
    // The endpoint flush routine is required if polled I/O is supported.
    //

    if ((ControllerInterface->SubmitPolledTransfer != NULL) &&
        (ControllerInterface->FlushEndpoint == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto HostRegisterControllerEnd;
    }

    //
    // Create a controller structure.
    //

    Controller = MmAllocateNonPagedPool(sizeof(USB_HOST_CONTROLLER),
                                        USB_CORE_ALLOCATION_TAG);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto HostRegisterControllerEnd;
    }

    RtlZeroMemory(Controller, sizeof(USB_HOST_CONTROLLER));
    RtlCopyMemory(&(Controller->Device),
                  ControllerInterface,
                  sizeof(USB_HOST_CONTROLLER_INTERFACE));

    Controller->Lock = KeCreateQueuedLock();
    if (Controller->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto HostRegisterControllerEnd;
    }

    Controller->AddressLock = KeCreateQueuedLock();
    if (Controller->AddressLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto HostRegisterControllerEnd;
    }

    //
    // Initialize the completed transfers queue. It uses the USB core's work
    // queue.
    //

    CompletionQueue = &(Controller->TransferCompletionQueue);
    Status = UsbpInitializeTransferCompletionQueue(CompletionQueue, FALSE);
    if (!KSUCCESS(Status)) {
        goto HostRegisterControllerEnd;
    }

    //
    // Allocate a work item for handling root hub port change notifications.
    //

    Controller->PortStatusWorkItem = KeCreateWorkItem(
                                                    UsbCoreWorkQueue,
                                                    WorkPriorityNormal,
                                                    UsbpPortStatusChangeWorker,
                                                    Controller,
                                                    USB_CORE_ALLOCATION_TAG);

    if (Controller->PortStatusWorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto HostRegisterControllerEnd;
    }

    ASSERT(Controller->PortStatusWorkItemQueued == FALSE);

    //
    // If the debugger handoff data refers to this controller, set it in the
    // host controller.
    //

    if ((UsbDebugHandoffData != NULL) &&
        (ControllerInterface->DebugPortSubType ==
         UsbDebugHandoffData->PortSubType) &&
        (ControllerInterface->Identifier == UsbDebugHandoffData->Identifier)) {

        if ((UsbDebugFlags & USB_DEBUG_DEBUGGER_HANDOFF) != 0) {
            RtlDebugPrint("USB: Handoff data matches host 0x%x\n", Controller);
        }

        Controller->HandoffData = UsbDebugHandoffData;

        //
        // Reserve the debugger device and hub address if they're valid.
        //

        Address = Controller->HandoffData->U.Usb.DeviceAddress;
        if (Address != 0) {
            Status = UsbpReserveDeviceAddress(Controller, NULL, Address);
            if (!KSUCCESS(Status)) {
                goto HostRegisterControllerEnd;
            }
        }

        Address = Controller->HandoffData->U.Usb.HubAddress;
        if (Address != 0) {
            Status = UsbpReserveDeviceAddress(Controller, NULL, Address);
            if (!KSUCCESS(Status)) {
                goto HostRegisterControllerEnd;
            }
        }
    }

    //
    // Add the controller to the master list and return successfully.
    //

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(UsbHostControllerListLock);
    INSERT_BEFORE(&(Controller->ListEntry), &UsbHostControllerList);
    KeReleaseQueuedLock(UsbHostControllerListLock);
    Status = STATUS_SUCCESS;

HostRegisterControllerEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            UsbHostDestroyControllerState((HANDLE)Controller);
            Controller = NULL;
        }

        *ControllerHandle = INVALID_HANDLE;

    } else {
        *ControllerHandle = (HANDLE)Controller;
    }

    return Status;
}

USB_API
VOID
UsbHostDestroyControllerState (
    HANDLE ControllerHandle
    )

/*++

Routine Description:

    This routine destroys the state of a USB host controller that was created
    during registration.

Arguments:

    ControllerHandle - Supplies a handle to a controller instance.

Return Value:

    None.

--*/

{

    PUSB_HOST_CONTROLLER Controller;

    ASSERT(ControllerHandle != INVALID_HANDLE);

    Controller = (PUSB_HOST_CONTROLLER)ControllerHandle;
    if (Controller->PortStatusWorkItem != NULL) {
        KeDestroyWorkItem(Controller->PortStatusWorkItem);
    }

    UsbpDestroyTransferCompletionQueue(&(Controller->TransferCompletionQueue));
    if (Controller->AddressLock != NULL) {
        KeDestroyQueuedLock(Controller->AddressLock);
    }

    if (Controller->Lock != NULL) {
        KeDestroyQueuedLock(Controller->Lock);
    }

    MmFreeNonPagedPool(Controller);
    return;
}

USB_API
VOID
UsbHostProcessCompletedTransfer (
    PUSB_TRANSFER_INTERNAL Transfer
    )

/*++

Routine Description:

    This routine is called by the USB host controller when the host controller
    is done with a transfer. This routine must be called if the transfer is
    completed successfully, failed, or was cancelled.

    This routine must be called while the host controller holds its controller
    lock. This is expected to be done at dispatch level.

Arguments:

    Transfer - Supplies a pointer to the transfer that has completed.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelDispatch);
    ASSERT(Transfer->Public.LengthTransferred <= Transfer->Public.Length);

    if (((UsbDebugFlags & USB_DEBUG_TRANSFER_COMPLETION) != 0) ||
        ((!KSUCCESS(Transfer->Public.Status)) &&
         ((UsbDebugFlags & USB_DEBUG_ERRORS) != 0))) {

        if ((UsbDebugDeviceAddress == 0) ||
            (UsbDebugDeviceAddress == Transfer->DeviceAddress)) {

            ASSERT(Transfer->Public.Error < UsbErrorCount);

            RtlDebugPrint(
                       "USB: Transfer (0x%08x) %s dev %d EP%x status %d (%s), "
                       "len 0x%x of 0x%x\n",
                       Transfer,
                       UsbTransferDirectionStrings[Transfer->Public.Direction],
                       Transfer->DeviceAddress,
                       Transfer->EndpointNumber,
                       Transfer->Public.Status,
                       UsbErrorStrings[Transfer->Public.Error],
                       Transfer->Public.LengthTransferred,
                       Transfer->Public.Length);
        }
    }

    //
    // Forward this on for the transfer code to handle.
    //

    UsbpProcessCompletedTransfer(Transfer);
    return;
}

USB_API
VOID
UsbHostNotifyPortChange (
    HANDLE ControllerHandle
    )

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

{

    PUSB_HOST_CONTROLLER Controller;
    BOOL OldValue;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);
    ASSERT(ControllerHandle != INVALID_HANDLE);

    //
    // Do nothing if the root hub is not yet initialized.
    //

    Controller = (PUSB_HOST_CONTROLLER)ControllerHandle;
    if (Controller->RootHub == NULL) {
        goto HostNotifyPortChangeEnd;
    }

    //
    // Queue a work item to handle the actual processing since this is running
    // at dispatch. But be sure not to queue the work item if it is already
    // on the queue.
    //

    OldValue = RtlAtomicCompareExchange32(
                                       &(Controller->PortStatusWorkItemQueued),
                                       TRUE,
                                       FALSE);

    if (OldValue != FALSE) {
        goto HostNotifyPortChangeEnd;
    }

    Status = KeQueueWorkItem(Controller->PortStatusWorkItem);

    ASSERT(KSUCCESS(Status));

HostNotifyPortChangeEnd:
    return;
}

KSTATUS
UsbpCreateEndpoint (
    PUSB_DEVICE Device,
    UCHAR Number,
    USB_TRANSFER_DIRECTION Direction,
    USB_TRANSFER_TYPE Type,
    ULONG MaxPacketSize,
    ULONG PollRate,
    PUSB_ENDPOINT *CreatedEndpoint
    )

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

{

    PVOID Context;
    PUSB_HOST_CREATE_ENDPOINT CreateEndpoint;
    PUSB_ENDPOINT Endpoint;
    USB_HOST_ENDPOINT_CREATION_REQUEST Request;
    KSTATUS Status;

    //
    // Convert the supplied poll rate into (micro)frames. For isochronous high
    // and full speed endpoints and high speed interrupt endpoints, the
    // supplied poll rate value is (x), between 1 and 16, where the
    // (micro)frame period is calculated by the forumla 2^(x-1).
    //
    // For all other combinations, the poll rate is a value between 1 and 255.
    // For full and low-speed interrupts this indicates the frame rate. For
    // high-speed control and bulk transfers, this indicates the maximum NAK
    // rate.
    //

    if ((PollRate != 0) &&
        (((Type == UsbTransferTypeInterrupt) &&
          (Device->Speed == UsbDeviceSpeedHigh)) ||
         ((Type == UsbTransferTypeIsochronous) &&
          ((Device->Speed == UsbDeviceSpeedFull) ||
           (Device->Speed == UsbDeviceSpeedHigh))))) {

        PollRate = 1 << (PollRate - 1);
    }

    //
    // Allocate and initialize the endpoint structures.
    //

    Endpoint = MmAllocateNonPagedPool(sizeof(USB_ENDPOINT),
                                      USB_CORE_ALLOCATION_TAG);

    if (Endpoint == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEndpointEnd;
    }

    RtlZeroMemory(Endpoint, sizeof(USB_ENDPOINT));
    Endpoint->Type = Type;
    Endpoint->Direction = Direction;
    Endpoint->MaxPacketSize = MaxPacketSize;
    Endpoint->PollRate = PollRate;
    Endpoint->Number = Number;
    Endpoint->ReferenceCount = 1;

    //
    // Fill out the endpoint creation request.
    //

    RtlZeroMemory(&Request, sizeof(USB_HOST_ENDPOINT_CREATION_REQUEST));
    Request.Version = USB_HOST_ENDPOINT_CREATION_REQUEST_VERSION;
    Request.Type = Type;
    Request.Direction = Direction;
    Request.Speed = Device->Speed;
    Request.MaxPacketSize = Endpoint->MaxPacketSize;
    Request.PollRate = Endpoint->PollRate;
    Request.EndpointNumber = Endpoint->Number;
    Request.HubPortNumber = Device->PortNumber;
    if (Device->Parent != NULL) {
        Request.HubAddress = Device->Parent->BusAddress;
    }

    //
    // Call the host controller to create any needed endpoint structures on its
    // end, and save the context pointer it returns.
    //

    CreateEndpoint = Device->Controller->Device.CreateEndpoint;
    Context = Device->Controller->Device.HostControllerContext;
    Status = CreateEndpoint(Context,
                            &Request,
                            &(Endpoint->HostControllerContext));

    if (!KSUCCESS(Status)) {
        goto CreateEndpointEnd;
    }

CreateEndpointEnd:
    if (!KSUCCESS(Status)) {
        if (Endpoint != NULL) {
            MmFreeNonPagedPool(Endpoint);
            Endpoint = NULL;
        }
    }

    *CreatedEndpoint = Endpoint;
    return Status;
}

VOID
UsbpResetEndpoint (
    PUSB_DEVICE Device,
    PUSB_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine resets a USB endpoint.

Arguments:

    Device - Supplies a pointer to the device to which the endpoint belongs.

    Endpoint - Supplies a pointer to the USB endpoint.

Return Value:

    None.

--*/

{

    PVOID Context;
    PUSB_HOST_RESET_ENDPOINT ResetEndpoint;

    ResetEndpoint = Device->Controller->Device.ResetEndpoint;
    Context = Device->Controller->Device.HostControllerContext;
    ResetEndpoint(Context,
                  Endpoint->HostControllerContext,
                  Endpoint->MaxPacketSize);

    return;
}

KSTATUS
UsbpFlushEndpoint (
    PUSB_DEVICE Device,
    PUSB_ENDPOINT Endpoint,
    PULONG TransferCount
    )

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

{

    PVOID Context;
    PUSB_HOST_FLUSH_ENDPOINT FlushEndpoint;
    KSTATUS Status;

    FlushEndpoint = Device->Controller->Device.FlushEndpoint;
    if (FlushEndpoint == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    Context = Device->Controller->Device.HostControllerContext;
    Status = FlushEndpoint(Context,
                           Endpoint->HostControllerContext,
                           TransferCount);

    return Status;
}

VOID
UsbpEndpointAddReference (
    PUSB_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine increments the reference count on the given endpoint.

Arguments:

    Endpoint - Supplies a pointer to the endpoint whose reference count should
        be incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Endpoint->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x1000));

    return;
}

VOID
UsbpEndpointReleaseReference (
    PUSB_DEVICE Device,
    PUSB_ENDPOINT Endpoint
    )

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

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Endpoint->ReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x1000));

    if (OldReferenceCount == 1) {
        UsbpDestroyEndpoint(Device, Endpoint);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
UsbpDestroyEndpoint (
    PUSB_DEVICE Device,
    PUSB_ENDPOINT Endpoint
    )

/*++

Routine Description:

    This routine destroys a created USB endpoint.

Arguments:

    Device - Supplies a pointer to the device that owns the endpoint.

    Endpoint - Supplies a pointer to the endpoint.

Return Value:

    None.

--*/

{

    PVOID ControllerContext;
    PUSB_HOST_DESTROY_ENDPOINT DestroyEndpoint;

    if (Endpoint->ListEntry.Next != NULL) {
        LIST_REMOVE(&(Endpoint->ListEntry));
    }

    DestroyEndpoint = Device->Controller->Device.DestroyEndpoint;
    ControllerContext = Device->Controller->Device.HostControllerContext;
    DestroyEndpoint(ControllerContext, Endpoint->HostControllerContext);
    MmFreeNonPagedPool(Endpoint);
    return;
}

VOID
UsbpPortStatusChangeWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes a port status change notification for the host
    controller.

Arguments:

    Parameter - Supplies a pointer to the USB host controller.

Return Value:

    None.

--*/

{

    PUSB_HOST_CONTROLLER Controller;
    BOOL OldValue;

    Controller = (PUSB_HOST_CONTROLLER)Parameter;

    //
    // Only the hub module can accurately handle this.
    //

    ASSERT(Controller->RootHub != NULL);

    UsbpNotifyRootHubStatusChange(Controller->RootHub);

    //
    // The above call collected the port status and cleared the hardware change
    // bits. Now allow another item to queue. This is done after the hub
    // notification to prevent the host controller from queuing a second work
    // item based on the same change information.
    //

    ASSERT(Controller->PortStatusWorkItemQueued != FALSE);

    OldValue = RtlAtomicCompareExchange32(
                                       &(Controller->PortStatusWorkItemQueued),
                                       FALSE,
                                       TRUE);

    ASSERT(OldValue != FALSE);

    return;
}

