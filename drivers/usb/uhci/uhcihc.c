/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uhcihc.c

Abstract:

    This module implements the meaty support for the UHCI Host Controller.

Author:

    Evan Green 14-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usbhost.h>
#include "uhci.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// These macros read from and write to a UHCI host controller register.
//

#define UHCI_READ_REGISTER(_Controller, _Register) \
    HlIoPortInShort((_Controller)->IoPortBase + _Register)

#define UHCI_WRITE_REGISTER(_Controller, _Register, _Value) \
    HlIoPortOutShort((_Controller)->IoPortBase + _Register, _Value);

#define UHCI_READ_REGISTER_LONG(_Controller, _Register) \
    HlIoPortInLong((_Controller)->IoPortBase + _Register)

#define UHCI_WRITE_REGISTER_LONG(_Controller, _Register, _Value) \
    HlIoPortOutLong((_Controller)->IoPortBase + _Register, _Value);

//
// Define the polling period for the UHCI port status.
//

#define UHCI_PORT_STATUS_CHANGE_PERIOD (3000 * MICROSECONDS_PER_MILLISECOND)

//
// Define UHCI debug flags.
//

#define UHCI_DEBUG_PORTS 0x00000001
#define UHCI_DEBUG_TRANSFERS 0x00000002

//
// Define the timeout value for the endpoint flush operation.
//

#define UHCI_ENDPOINT_FLUSH_TIMEOUT 10

//
// Define the timeout value for the polled I/O operations.
//

#define UHCI_POLLED_TRANSFER_TIMEOUT 10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UhcipCreateEndpoint (
    PVOID HostControllerContext,
    PUSB_HOST_ENDPOINT_CREATION_REQUEST Endpoint,
    PVOID *EndpointContext
    );

VOID
UhcipResetEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxPacketSize
    );

KSTATUS
UhcipFlushEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PULONG TransferCount
    );

VOID
UhcipDestroyEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext
    );

KSTATUS
UhcipCreateTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxBufferSize,
    ULONG Flags,
    PVOID *TransferContext
    );

VOID
UhcipDestroyTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PVOID TransferContext
    );

KSTATUS
UhcipSubmitTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
UhcipSubmitPolledTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
UhcipSubmitTransferQueue (
    PUHCI_CONTROLLER Controller,
    PUHCI_ENDPOINT Endpoint,
    PUHCI_TRANSFER_QUEUE Queue,
    PULONG SubmittedTransferCount,
    BOOL LockNotRequired
    );

KSTATUS
UhcipCancelTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
UhcipGetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    );

KSTATUS
UhcipSetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS NewStatus
    );

RUNLEVEL
UhcipAcquireControllerLock (
    PUHCI_CONTROLLER Controller
    );

VOID
UhcipReleaseControllerLock (
    PUHCI_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    );

VOID
UhcipWaitForNextFrame (
    PUHCI_CONTROLLER Controller
    );

VOID
UhcipProcessInterrupt (
    PUHCI_CONTROLLER Controller,
    ULONG PendingStatus
    );

VOID
UhcipFillOutTransferDescriptor (
    PUHCI_CONTROLLER Controller,
    PUHCI_ENDPOINT Endpoint,
    PUHCI_TRANSFER_QUEUE Queue,
    PUHCI_TRANSFER UhciTransfer,
    PUSB_TRANSFER_INTERNAL Transfer,
    ULONG Offset,
    ULONG Length,
    BOOL LastTransfer
    );

BOOL
UhcipProcessPotentiallyCompletedTransfer (
    PUHCI_TRANSFER_QUEUE Queue,
    PUHCI_TRANSFER Transfer
    );

VOID
UhcipRemoveTransferQueue (
    PUHCI_CONTROLLER Controller,
    PUHCI_TRANSFER_QUEUE Queue,
    BOOL Cancel
    );

VOID
UhcipPortStatusDpc (
    PDPC Dpc
    );

BOOL
UhcipHasPortStatusChanged (
    PUHCI_CONTROLLER Controller
    );

VOID
UhcipFlushCacheRegion (
    PVOID VirtualAddress,
    ULONG Size
    );

VOID
UhcipFixDataToggles (
    PUHCI_TRANSFER_QUEUE RemovingQueue,
    BOOL Toggle
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define a bitfield of debug flags that enable various print messages for
// UHCI. See UHCI_DEBUG_* definitions.
//

ULONG UhciDebugFlags = 0x0;

//
// ------------------------------------------------------------------ Functions
//

PUHCI_CONTROLLER
UhcipInitializeControllerState (
    ULONG IoPortBase
    )

/*++

Routine Description:

    This routine initializes the state and variables needed to start up a UHCI
    host controller.

Arguments:

    IoPortBase - Supplies the base I/O port of the UHCI registers.

Return Value:

    Returns a pointer to the UHCI controller state object on success.

    NULL on failure.

--*/

{

    ULONG BlockSize;
    PUHCI_CONTROLLER Controller;
    PHYSICAL_ADDRESS ControlQueuePhysicalAddress;
    ULONG Flags;
    ULONG Frame;
    PHYSICAL_ADDRESS InterruptQueuePhysicalAddress;
    ULONG IoBufferFlags;
    KSTATUS Status;

    //
    // Allocate the controller structure itself.
    //

    Controller = MmAllocateNonPagedPool(sizeof(UHCI_CONTROLLER),
                                        UHCI_ALLOCATION_TAG);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    RtlZeroMemory(Controller, sizeof(UHCI_CONTROLLER));
    INITIALIZE_LIST_HEAD(&(Controller->QueueListHead));
    INITIALIZE_LIST_HEAD(&(Controller->IsochronousTransferListHead));
    Controller->IoPortBase = (USHORT)IoPortBase;
    Controller->UsbCoreHandle = INVALID_HANDLE;
    Controller->InterruptHandle = INVALID_HANDLE;
    KeInitializeSpinLock(&(Controller->Lock));

    //
    // Allocate and initialize the buffer used to hold the UHCI schedule.
    //

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Controller->ScheduleIoBuffer = MmAllocateNonPagedIoBuffer(
                                                     0,
                                                     MAX_ULONG,
                                                     UHCI_FRAME_LIST_ALIGNMENT,
                                                     sizeof(UHCI_SCHEDULE),
                                                     IoBufferFlags);

    if (Controller->ScheduleIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    ASSERT(Controller->ScheduleIoBuffer->FragmentCount == 1);
    ASSERT(Controller->ScheduleIoBuffer->Fragment[0].Size >=
                                                        sizeof(UHCI_SCHEDULE));

    Controller->Schedule =
                      Controller->ScheduleIoBuffer->Fragment[0].VirtualAddress;

    //
    // Create the block allocator used to allocate transfers and queues. The
    // block size is that of the larger structure.
    //

    if (sizeof(UHCI_TRANSFER) >= sizeof(UHCI_TRANSFER_QUEUE)) {
        BlockSize = sizeof(UHCI_TRANSFER);

    } else {
        BlockSize = sizeof(UHCI_TRANSFER_QUEUE);
    }

    Flags = BLOCK_ALLOCATOR_FLAG_NON_PAGED |
            BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;

    Controller->BlockAllocator = MmCreateBlockAllocator(
                                          BlockSize,
                                          UHCI_BLOCK_ALLOCATOR_ALIGNMENT,
                                          UHCI_BLOCK_ALLOCATOR_EXPANSION_COUNT,
                                          Flags,
                                          UHCI_BLOCK_ALLOCATION_TAG);

    if (Controller->BlockAllocator == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    //
    // Allocate and initialize the head of the interrupt queue.
    //

    Controller->InterruptQueue = MmAllocateBlock(
                                               Controller->BlockAllocator,
                                               &InterruptQueuePhysicalAddress);

    if (Controller->InterruptQueue == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    RtlZeroMemory(Controller->InterruptQueue, sizeof(UHCI_TRANSFER_QUEUE));
    Controller->InterruptQueue->PhysicalAddress = InterruptQueuePhysicalAddress;
    INITIALIZE_LIST_HEAD(&(Controller->InterruptQueue->TransferListHead));
    Controller->InterruptQueue->HardwareQueueHead.ElementLink =
                                                UHCI_QUEUE_HEAD_LINK_TERMINATE;

    //
    // Allocate and initialize the control queue.
    //

    Controller->ControlQueue = MmAllocateBlock(Controller->BlockAllocator,
                                               &ControlQueuePhysicalAddress);

    if (Controller->ControlQueue == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    RtlZeroMemory(Controller->ControlQueue, sizeof(UHCI_TRANSFER_QUEUE));
    Controller->ControlQueue->PhysicalAddress = ControlQueuePhysicalAddress;
    INITIALIZE_LIST_HEAD(&(Controller->ControlQueue->TransferListHead));
    Controller->ControlQueue->HardwareQueueHead.ElementLink =
                                                UHCI_QUEUE_HEAD_LINK_TERMINATE;

    //
    // Point the interrupt queue at the control queue, and the control queue
    // back at the control queue. Bulk transfers will insert themselves
    // after the control queue and Isochronous transfers will insert thesmelves
    // at specific frames before the interrupt queue. So the total order will
    // go Isochronous, Interrupt, Control, Bulk, and then loop back to
    // Control and Bulk if there is time remaining.
    //

    ASSERT((ControlQueuePhysicalAddress &
            (~UHCI_QUEUE_HEAD_LINK_ADDRESS_MASK)) == 0);

    ASSERT((InterruptQueuePhysicalAddress &
            (~UHCI_QUEUE_HEAD_LINK_ADDRESS_MASK)) == 0);

    Controller->InterruptQueue->HardwareQueueHead.LinkPointer =
          (ULONG)ControlQueuePhysicalAddress | UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD;

    Controller->ControlQueue->HardwareQueueHead.LinkPointer =
          (ULONG)ControlQueuePhysicalAddress | UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD;

    //
    // Wire up the software list as well.
    //

    INSERT_AFTER(&(Controller->InterruptQueue->GlobalListEntry),
                 &(Controller->QueueListHead));

    INSERT_AFTER(&(Controller->ControlQueue->GlobalListEntry),
                 &(Controller->InterruptQueue->GlobalListEntry));

    //
    // Initialize all frames to point at the interrupt queue.
    //

    for (Frame = 0; Frame < UHCI_FRAME_LIST_ENTRY_COUNT; Frame += 1) {
        Controller->Schedule->Frame[Frame] =
                                        (ULONG)InterruptQueuePhysicalAddress |
                                        UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD;
    }

    UhcipFlushCacheRegion(Controller->Schedule, sizeof(UHCI_SCHEDULE));
    UhcipFlushCacheRegion(&(Controller->ControlQueue->HardwareQueueHead),
                          sizeof(UHCI_QUEUE_HEAD));

    UhcipFlushCacheRegion(&(Controller->InterruptQueue->HardwareQueueHead),
                          sizeof(UHCI_QUEUE_HEAD));

    //
    // Create the port status timer and DPC.
    //

    Controller->PortStatusTimer = KeCreateTimer(UHCI_ALLOCATION_TAG);
    if (Controller->PortStatusTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    Controller->PortStatusDpc = KeCreateDpc(UhcipPortStatusDpc, Controller);
    if (Controller->PortStatusDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    Status = STATUS_SUCCESS;

InitializeControllerStateEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            UhcipDestroyControllerState(Controller);
            Controller = NULL;
        }
    }

    return Controller;
}

VOID
UhcipDestroyControllerState (
    PUHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys the memory associated with a UHCI controller.

Arguments:

    Controller - Supplies a pointer to the UHCI controller state to release.

Return Value:

    None.

--*/

{

    if (Controller->ScheduleIoBuffer != NULL) {
        MmFreeIoBuffer(Controller->ScheduleIoBuffer);
    }

    if (Controller->InterruptQueue != NULL) {
        MmFreeBlock(Controller->BlockAllocator,
                    Controller->InterruptQueue);
    }

    if (Controller->ControlQueue != NULL) {
        MmFreeBlock(Controller->BlockAllocator,
                    Controller->ControlQueue);
    }

    if (Controller->BlockAllocator != NULL) {
        MmDestroyBlockAllocator(Controller->BlockAllocator);
    }

    ASSERT(LIST_EMPTY(&(Controller->QueueListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Controller->IsochronousTransferListHead)) != FALSE);

    if (Controller->PortStatusTimer != NULL) {
        KeDestroyTimer(Controller->PortStatusTimer);
    }

    if (Controller->PortStatusDpc != NULL) {
        KeDestroyDpc(Controller->PortStatusDpc);
    }

    if (Controller->UsbCoreHandle != INVALID_HANDLE) {
        UsbHostDestroyControllerState(Controller->UsbCoreHandle);
    }

    MmFreeNonPagedPool(Controller);
    return;
}

KSTATUS
UhcipRegisterController (
    PUHCI_CONTROLLER Controller,
    PDEVICE Device
    )

/*++

Routine Description:

    This routine registers the started UHCI controller with the core USB
    library.

Arguments:

    Controller - Supplies a pointer to the UHCI controller state of the
        controller to register.

    Device - Supplies a pointer to the device object.

Return Value:

    Status code.

--*/

{

    USB_HOST_CONTROLLER_INTERFACE Interface;
    KSTATUS Status;

    //
    // Fill out the functions that the USB core library will use to control
    // the UHCI controller.
    //

    RtlZeroMemory(&Interface, sizeof(USB_HOST_CONTROLLER_INTERFACE));
    Interface.Version = USB_HOST_CONTROLLER_INTERFACE_VERSION;
    Interface.DriverObject = UhciDriver;
    Interface.DeviceObject = Device;
    Interface.HostControllerContext = Controller;
    Interface.Speed = UsbDeviceSpeedFull;
    Interface.DebugPortSubType = -1;
    Interface.RootHubPortCount = 2;
    Interface.CreateEndpoint = UhcipCreateEndpoint;
    Interface.ResetEndpoint = UhcipResetEndpoint;
    Interface.FlushEndpoint = UhcipFlushEndpoint;
    Interface.DestroyEndpoint = UhcipDestroyEndpoint;
    Interface.CreateTransfer = UhcipCreateTransfer;
    Interface.DestroyTransfer = UhcipDestroyTransfer;
    Interface.SubmitTransfer = UhcipSubmitTransfer;
    Interface.SubmitPolledTransfer = UhcipSubmitPolledTransfer;
    Interface.CancelTransfer = UhcipCancelTransfer;
    Interface.GetRootHubStatus = UhcipGetRootHubStatus;
    Interface.SetRootHubStatus = UhcipSetRootHubStatus;
    Status = UsbHostRegisterController(&Interface,
                                       &(Controller->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto RegisterControllerEnd;
    }

RegisterControllerEnd:
    return Status;
}

VOID
UhcipSetInterruptHandle (
    PUHCI_CONTROLLER Controller,
    HANDLE InterruptHandle
    )

/*++

Routine Description:

    This routine saves the handle of the connected interrupt in the UHCI
    controller.

Arguments:

    Controller - Supplies a pointer to the UHCI controller state.

    InterruptHandle - Supplies the connected interrupt handle.

Return Value:

    None.

--*/

{

    Controller->InterruptHandle = InterruptHandle;
    return;
}

KSTATUS
UhcipResetController (
    PUHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine resets and starts the UHCI controller.

Arguments:

    Controller - Supplies a pointer to the UHCI controller state of the
        controller to reset.

Return Value:

    Status code.

--*/

{

    ULONG CommandRegister;
    ULONG FrameBaseRegister;
    ULONG InterruptRegister;
    ULONG PortStatusRegister;

    //
    // Reset the host controller and wait for the hardware to clear the bit,
    // which indicates that the reset is complete.
    //

    CommandRegister = UHCI_COMMAND_HOST_CONTROLLER_RESET;
    UHCI_WRITE_REGISTER(Controller, UhciRegisterUsbCommand, CommandRegister);
    do {

        //
        // AND in the hardware register to see if the bit has cleared.
        //

        CommandRegister &= UHCI_READ_REGISTER(Controller,
                                              UhciRegisterUsbCommand);

    } while (CommandRegister != 0);

    //
    // Disable the ports.
    //

    UHCI_WRITE_REGISTER(Controller, UhciRegisterPort1StatusControl, 0);
    UHCI_WRITE_REGISTER(Controller, UhciRegisterPort2StatusControl, 0);

    //
    // Clear the status register.
    //

    UHCI_WRITE_REGISTER(Controller, UhciRegisterUsbStatus, 0);

    //
    // Enable all interrupts.
    //

    InterruptRegister = UHCI_INTERRUPT_SHORT_PACKET |
                        UHCI_INTERRUPT_COMPLETION |
                        UHCI_INTERRUPT_RESUME |
                        UHCI_INTERRUPT_TIMEOUT_CRC_ERROR;

    UHCI_WRITE_REGISTER(Controller,
                        UhciRegisterUsbInterruptEnable,
                        InterruptRegister);

    //
    // Set the frame list base register to the physical address of the UHCI
    // schedule.
    //

    FrameBaseRegister =
              (ULONG)Controller->ScheduleIoBuffer->Fragment[0].PhysicalAddress;

    UHCI_WRITE_REGISTER_LONG(Controller,
                             UhciRegisterFrameBaseAddress,
                             FrameBaseRegister);

    //
    // Write to the command register to start the controller.
    //

    CommandRegister = UHCI_COMMAND_MAX_RECLAMATION_PACKET_64 |
                      UHCI_COMMAND_CONFIGURED |
                      UHCI_COMMAND_RUN;

    UHCI_WRITE_REGISTER(Controller, UhciRegisterUsbCommand, CommandRegister);

    //
    // Fire up both ports.
    //

    PortStatusRegister = UHCI_PORT_ENABLED;
    UHCI_WRITE_REGISTER(Controller,
                        UhciRegisterPort1StatusControl,
                        PortStatusRegister);

    UHCI_WRITE_REGISTER(Controller,
                        UhciRegisterPort2StatusControl,
                        PortStatusRegister);

    return STATUS_SUCCESS;
}

INTERRUPT_STATUS
UhcipInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the UHCI interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the UHCI
        controller.

Return Value:

    Interrupt status.

--*/

{

    PUHCI_CONTROLLER Controller;
    INTERRUPT_STATUS InterruptStatus;
    USHORT UsbStatus;

    Controller = (PUHCI_CONTROLLER)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register. If it's non-zero, this is USB's interrupt.
    //

    UsbStatus = UHCI_READ_REGISTER(Controller, UhciRegisterUsbStatus);
    if (UsbStatus != 0) {
        InterruptStatus = InterruptStatusClaimed;
        UHCI_WRITE_REGISTER(Controller, UhciRegisterUsbStatus, UsbStatus);
        RtlAtomicOr32(&(Controller->PendingStatusBits), UsbStatus);
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
UhcipInterruptServiceDpc (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the dispatch level UHCI interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the UHCI
        controller.

Return Value:

    Interrupt status.

--*/

{

    PUHCI_CONTROLLER Controller;
    ULONG PendingStatus;

    Controller = Context;
    PendingStatus = RtlAtomicExchange32(&(Controller->PendingStatusBits), 0);
    if (PendingStatus == 0) {
        return InterruptStatusNotClaimed;
    }

    UhcipProcessInterrupt(Controller, PendingStatus);
    return InterruptStatusClaimed;
}

KSTATUS
UhcipInitializePortChangeDetection (
    PUHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes the UHCI port status change timer in order to
    periodically check to see if devices have been added or removed from
    the USB root hub.

Arguments:

    Controller - Supplies a pointer to the UHCI controller state of the
        controller whose ports need status change detection.

Return Value:

    Status code.

--*/

{

    ULONGLONG Period;
    KSTATUS Status;

    Period = KeConvertMicrosecondsToTimeTicks(UHCI_PORT_STATUS_CHANGE_PERIOD);
    Status = KeQueueTimer(Controller->PortStatusTimer,
                          TimerQueueSoft,
                          0,
                          Period,
                          0,
                          Controller->PortStatusDpc);

    ASSERT(KSUCCESS(Status));

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UhcipCreateEndpoint (
    PVOID HostControllerContext,
    PUSB_HOST_ENDPOINT_CREATION_REQUEST Endpoint,
    PVOID *EndpointContext
    )

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

{

    PUHCI_ENDPOINT NewEndpoint;
    KSTATUS Status;

    NewEndpoint = MmAllocateNonPagedPool(sizeof(UHCI_ENDPOINT),
                                         UHCI_ALLOCATION_TAG);

    if (NewEndpoint == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEndpointEnd;
    }

    RtlZeroMemory(NewEndpoint, sizeof(UHCI_ENDPOINT));
    INITIALIZE_LIST_HEAD(&(NewEndpoint->QueueListHead));
    NewEndpoint->TransferType = Endpoint->Type;

    ASSERT((Endpoint->Speed == UsbDeviceSpeedLow) ||
           (Endpoint->Speed == UsbDeviceSpeedFull));

    NewEndpoint->Speed = Endpoint->Speed;

    ASSERT(Endpoint->MaxPacketSize != 0);

    NewEndpoint->MaxPacketSize = Endpoint->MaxPacketSize;
    NewEndpoint->EndpointNumber = Endpoint->EndpointNumber;
    Status = STATUS_SUCCESS;

CreateEndpointEnd:
    if (!KSUCCESS(Status)) {
        if (NewEndpoint != NULL) {
            MmFreeNonPagedPool(NewEndpoint);
            NewEndpoint = NULL;
        }
    }

    *EndpointContext = NewEndpoint;
    return Status;
}

VOID
UhcipResetEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxPacketSize
    )

/*++

Routine Description:

    This routine is called by the USB core when an endpoint needs to be reset.

Arguments:

    HostControllerContext - Supplies the context pointer passed to the USB core
        when the controller was created. This is used to identify the USB host
        controller to the host controller driver.

    EndpointContext - Supplies a pointer to the context returned by the host
        controller when the endpoint was created.

    MaxPacketSize - Supplies the maximum transfer size of the endpoint.

Return Value:

    None.

--*/

{

    PUHCI_ENDPOINT Endpoint;

    Endpoint = (PUHCI_ENDPOINT)EndpointContext;

    //
    // There better not be any active queues running around during an endpoint
    // reset.
    //

    ASSERT(LIST_EMPTY(&(Endpoint->QueueListHead)) != FALSE);

    Endpoint->DataToggle = FALSE;
    Endpoint->MaxPacketSize = MaxPacketSize;
    return;
}

KSTATUS
UhcipFlushEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PULONG TransferCount
    )

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

{

    PUHCI_CONTROLLER Controller;
    ULONG Count;
    PLIST_ENTRY CurrentQueueEntry;
    PLIST_ENTRY CurrentTransferEntry;
    PUHCI_ENDPOINT Endpoint;
    PUHCI_TRANSFER_QUEUE Queue;
    BOOL RemoveQueue;
    KSTATUS Status;
    ULONGLONG Timeout;
    PUHCI_TRANSFER Transfer;

    //
    // This routine removes transfers without acquiring the controller lock. It
    // is expected that the caller is using under special circumstances at high
    // run level (e.g. to prepare for crash dump writes during system failure).
    //

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Controller = (PUHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PUHCI_ENDPOINT)EndpointContext;
    if (Endpoint->TransferType == UsbTransferTypeIsochronous) {

        //
        // TODO: Implement support for isochronous transfers.
        //

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    //
    // Let every transfer queue in the endpoint complete. If the caller is
    // about to use this endpoint for an operation during a system failure,
    // then the endpoint better be alive enough to finish the rest of its
    // current transfers.
    //

    Timeout = HlQueryTimeCounter() +
              (HlQueryTimeCounterFrequency() * UHCI_ENDPOINT_FLUSH_TIMEOUT);

    Count = 0;
    while (LIST_EMPTY(&(Endpoint->QueueListHead)) == FALSE) {
        if (HlQueryTimeCounter() > Timeout) {
            Status = STATUS_TIMEOUT;
            goto FlushEndpointEnd;
        }

        CurrentQueueEntry = Endpoint->QueueListHead.Next;
        while (CurrentQueueEntry != &(Endpoint->QueueListHead)) {
            Queue = LIST_VALUE(CurrentQueueEntry,
                               UHCI_TRANSFER_QUEUE,
                               EndpointListEntry);

            CurrentQueueEntry = CurrentQueueEntry->Next;

            ASSERT(Queue != Controller->ControlQueue);
            ASSERT(Queue != Controller->InterruptQueue);

            //
            // Loop through every transfer in the queue.
            //

            RemoveQueue = FALSE;
            CurrentTransferEntry = Queue->TransferListHead.Next;
            while (CurrentTransferEntry != &(Queue->TransferListHead)) {
                Transfer = LIST_VALUE(CurrentTransferEntry,
                                      UHCI_TRANSFER,
                                      QueueListEntry);

                CurrentTransferEntry = CurrentTransferEntry->Next;

                //
                // Examine the tranfser, and determine whether or not it's
                // complete.
                //

                RemoveQueue = UhcipProcessPotentiallyCompletedTransfer(
                                                                     Queue,
                                                                     Transfer);

                if ((RemoveQueue != FALSE) ||
                    (Transfer == Queue->LastTransfer)) {

                    break;
                }
            }

            //
            // If the queue isn't already slated to be removed, look to see
            // if it is empty.
            //

            if ((RemoveQueue == FALSE) &&
                ((Queue->HardwareQueueHead.ElementLink &
                 UHCI_QUEUE_HEAD_LINK_TERMINATE) != 0)) {

                RemoveQueue = TRUE;
            }

            //
            // If necessary, remove the queue from the schedule. Do not notify
            // the USB core that the transfer is done. This routine is meant
            // to be used at high run level during system failure. There isn't
            // anyone listening for the transfer completion.
            //

            if (RemoveQueue != FALSE) {
                UhcipRemoveTransferQueue(Controller, Queue, FALSE);
                Count += 1;
            }
        }
    }

    Status = STATUS_SUCCESS;

FlushEndpointEnd:
    *TransferCount = Count;
    return Status;
}

VOID
UhcipDestroyEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext
    )

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

{

    PUHCI_ENDPOINT Endpoint;

    Endpoint = (PUHCI_ENDPOINT)EndpointContext;

    ASSERT(LIST_EMPTY(&(Endpoint->QueueListHead)) != FALSE);

    MmFreeNonPagedPool(Endpoint);
    return;
}

KSTATUS
UhcipCreateTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxBufferSize,
    ULONG Flags,
    PVOID *TransferContext
    )

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

{

    PUHCI_CONTROLLER Controller;
    PUHCI_ENDPOINT Endpoint;
    BOOL ForceShortTransfer;
    PUHCI_TRANSFER_QUEUE Queue;
    PHYSICAL_ADDRESS QueuePhysicalAddress;
    KSTATUS Status;
    PUHCI_TRANSFER Transfer;
    ULONG TransferCount;
    ULONG TransferIndex;
    PHYSICAL_ADDRESS TransferPhysicalAddress;

    ASSERT(TransferContext != NULL);

    Controller = (PUHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PUHCI_ENDPOINT)EndpointContext;
    ForceShortTransfer = FALSE;
    if ((Flags & USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) {
        ForceShortTransfer = TRUE;
    }

    //
    // Create a new transfer queue.
    //

    Queue = MmAllocateBlock(Controller->BlockAllocator, &QueuePhysicalAddress);
    if (Queue == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTransferEnd;
    }

    RtlZeroMemory(Queue, sizeof(UHCI_TRANSFER_QUEUE));
    Queue->PhysicalAddress = QueuePhysicalAddress;
    INITIALIZE_LIST_HEAD(&(Queue->TransferListHead));
    Queue->HardwareQueueHead.ElementLink = UHCI_QUEUE_HEAD_LINK_TERMINATE;
    Queue->HardwareQueueHead.LinkPointer = UHCI_QUEUE_HEAD_LINK_TERMINATE;
    Queue->Endpoint = Endpoint;

    //
    // Figure out the number of transfers needed. The first 8 bytes of a
    // control transfer (the setup packet) are always on their own. Control
    // transfers also have a status stage at the end.
    //

    TransferCount = 0;
    if (Endpoint->TransferType == UsbTransferTypeControl) {

        ASSERT(MaxBufferSize >= sizeof(USB_SETUP_PACKET));

        MaxBufferSize -= sizeof(USB_SETUP_PACKET);

        //
        // Account for both the setup and status stage here.
        //

        TransferCount += 2;
    }

    //
    // Create enough data transfers, where one transfer can hold up to the max
    // packet size.
    //

    if (MaxBufferSize != 0) {
        TransferCount += MaxBufferSize / Endpoint->MaxPacketSize;
        if ((MaxBufferSize % Endpoint->MaxPacketSize) != 0) {
            TransferCount += 1;
        }

        //
        // If this transfer needs to indicate completion with a short packet,
        // make sure another transfer is available. This is only necessary if
        // the max size for this transfer won't guarantee a short transfer.
        //

        if (((Flags & USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) &&
            (MaxBufferSize >= Endpoint->MaxPacketSize)) {

            TransferCount += 1;
        }

    //
    // Account for a USB transfer that will only send zero length packets and
    // for control transfers what need to force a zero length packet in the
    // data phase.
    //

    } else if ((ForceShortTransfer != FALSE) ||
               (Endpoint->TransferType != UsbTransferTypeControl)) {

        TransferCount += 1;
    }

    //
    // Create the new transfers.
    //

    for (TransferIndex = 0; TransferIndex < TransferCount; TransferIndex += 1) {

        //
        // Allocate a new transfer.
        //

        Transfer = MmAllocateBlock(Controller->BlockAllocator,
                                   &TransferPhysicalAddress);

        if (Transfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateTransferEnd;
        }

        RtlZeroMemory(Transfer, sizeof(UHCI_TRANSFER));
        Transfer->PhysicalAddress = TransferPhysicalAddress;

        ASSERT((TransferPhysicalAddress &
                UHCI_TRANSFER_DESCRIPTOR_LINK_ADDRESS_MASK) ==
               TransferPhysicalAddress);

        //
        // Add the transfer to the end of the queue.
        //

        INSERT_BEFORE(&(Transfer->QueueListEntry), &(Queue->TransferListHead));
    }

    Status = STATUS_SUCCESS;

CreateTransferEnd:
    if (!KSUCCESS(Status)) {
        if (Queue != NULL) {

            //
            // Free all transfers that were allocated.
            //

            while (LIST_EMPTY(&(Queue->TransferListHead)) == FALSE) {
                Transfer = LIST_VALUE(Queue->TransferListHead.Next,
                                      UHCI_TRANSFER,
                                      QueueListEntry);

                LIST_REMOVE(&(Transfer->QueueListEntry));
                MmFreeBlock(Controller->BlockAllocator, Transfer);
            }

            MmFreeBlock(Controller->BlockAllocator, Queue);
            Queue = NULL;
        }
    }

    *TransferContext = Queue;
    return Status;
}

VOID
UhcipDestroyTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PVOID TransferContext
    )

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

{

    PUHCI_CONTROLLER Controller;
    PUHCI_TRANSFER_QUEUE Queue;
    PUHCI_TRANSFER Transfer;

    Controller = (PUHCI_CONTROLLER)HostControllerContext;
    Queue = (PUHCI_TRANSFER_QUEUE)TransferContext;

    //
    // Free all transfers that were allocated.
    //

    while (LIST_EMPTY(&(Queue->TransferListHead)) == FALSE) {
        Transfer = LIST_VALUE(Queue->TransferListHead.Next,
                              UHCI_TRANSFER,
                              QueueListEntry);

        LIST_REMOVE(&(Transfer->QueueListEntry));
        MmFreeBlock(Controller->BlockAllocator, Transfer);
    }

    MmFreeBlock(Controller->BlockAllocator, Queue);
    return;
}

KSTATUS
UhcipSubmitTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

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

{

    PUHCI_CONTROLLER Controller;
    PUHCI_ENDPOINT Endpoint;
    PUHCI_TRANSFER_QUEUE Queue;
    KSTATUS Status;

    Controller = (PUHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PUHCI_ENDPOINT)EndpointContext;
    Queue = (PUHCI_TRANSFER_QUEUE)TransferContext;

    //
    // Prepare and submit the transfer queue.
    //

    Queue->UsbTransfer = Transfer;
    Status = UhcipSubmitTransferQueue(Controller, Endpoint, Queue, NULL, FALSE);
    return Status;
}

KSTATUS
UhcipSubmitPolledTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine submits a transfer to the USB host controller for execution
    and busily waits until the transfer has completed.

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

{

    PUHCI_CONTROLLER Controller;
    PLIST_ENTRY CurrentEntry;
    PUHCI_ENDPOINT Endpoint;
    volatile PULONG HardwareStatus;
    PUHCI_TRANSFER_QUEUE Queue;
    BOOL RemoveQueue;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG TransferCount;
    ULONG TransferIndex;
    PUHCI_TRANSFER UhciTransfer;

    Controller = (PUHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PUHCI_ENDPOINT)EndpointContext;
    Queue = (PUHCI_TRANSFER_QUEUE)TransferContext;

    //
    // Polled I/O should only be requested at high run level.
    //

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    //
    // There should be no other active queues on the endpoint.
    //

    ASSERT(LIST_EMPTY(&(Endpoint->QueueListHead)) != FALSE);

    //
    // Prepare and submit the transfer queue.
    //

    Queue->UsbTransfer = Transfer;
    Status = UhcipSubmitTransferQueue(Controller,
                                      Endpoint,
                                      Queue,
                                      &TransferCount,
                                      TRUE);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Now poll the transfers in the queue in until they are complete.
    //

    Timeout = HlQueryTimeCounter() +
              (HlQueryTimeCounterFrequency() * UHCI_POLLED_TRANSFER_TIMEOUT);

    CurrentEntry = Queue->TransferListHead.Next;
    for (TransferIndex = 0; TransferIndex < TransferCount; TransferIndex += 1) {
        UhciTransfer = LIST_VALUE(CurrentEntry, UHCI_TRANSFER, QueueListEntry);
        CurrentEntry = CurrentEntry->Next;
        HardwareStatus = &(UhciTransfer->HardwareTransfer.Status);
        while ((*HardwareStatus &
                UHCI_TRANSFER_DESCRIPTOR_STATUS_ACTIVE) != 0) {

            if (HlQueryTimeCounter() > Timeout) {
                Transfer->Public.Status = STATUS_TIMEOUT;
                goto SubmitPolledTransferEnd;
            }
        }

        RemoveQueue = UhcipProcessPotentiallyCompletedTransfer(Queue,
                                                               UhciTransfer);

        if (RemoveQueue != FALSE) {
            break;
        }
    }

    UhcipRemoveTransferQueue(Controller, Queue, FALSE);

SubmitPolledTransferEnd:
    return Transfer->Public.Status;
}

KSTATUS
UhcipSubmitTransferQueue (
    PUHCI_CONTROLLER Controller,
    PUHCI_ENDPOINT Endpoint,
    PUHCI_TRANSFER_QUEUE Queue,
    PULONG SubmittedTransferCount,
    BOOL LockNotRequired
    )

/*++

Routine Description:

    This routine submits a UHCI transfer queue, initializing the transfers and
    placing them in the schedule.

Arguments:

    Controller - Supplies a pointer to the UHCI controller context.

    Endpoint - Supplies a pointer to the context for the UHCI endpoint on which
        the queue will be submitted.

    Queue - Supplies a pointer to the UHCI transfer queue to submit.

    SubmittedTransferCount - Supplies an optional pointer that receives the
        number of individual transfers submitted for the queue.

    LockNotRequired - Supplies a boolean indicating if the global controller
        lock does not need to be acquired when making the queue submission. The
        default value is FALSE. The lock is only not required in certain
        critical code paths.

Return Value:

    Status code.

--*/

{

    BOOL ControlTransfer;
    PLIST_ENTRY CurrentEntry;
    BOOL ForceShortTransfer;
    BOOL InGlobalList;
    BOOL LastTransfer;
    ULONG Length;
    ULONG Offset;
    RUNLEVEL OldRunLevel;
    PUHCI_TRANSFER PreviousLastTransfer;
    PUHCI_TRANSFER_QUEUE QueueBefore;
    ULONG TotalLength;
    PUSB_TRANSFER_INTERNAL Transfer;
    ULONG TransferCount;
    ULONG TransferIndex;
    PUHCI_TRANSFER UhciTransfer;

    ControlTransfer = FALSE;
    Queue->LinkToLastTransfer = 0;
    Transfer = Queue->UsbTransfer;
    UhciTransfer = NULL;

    //
    // This queue had better not be on a list already.
    //

    ASSERT((Queue->GlobalListEntry.Next == NULL) &&
           (Queue->EndpointListEntry.Next == NULL));

    //
    // Assume that this is going to be a rousing success.
    //

    Transfer->Public.Status = STATUS_SUCCESS;
    Transfer->Public.Error = UsbErrorNone;

    //
    // Determine the number of transfers needed for this transfer, and loop
    // filling them out. This is necessary because the number of transfers
    // per transfer is not constant; the system may re-use a transfer and
    // and change the length.
    //

    TransferCount = 0;
    TotalLength = Transfer->Public.Length;
    if (Endpoint->TransferType == UsbTransferTypeControl) {
        ControlTransfer = TRUE;

        ASSERT(TotalLength >= sizeof(USB_SETUP_PACKET));

        TotalLength -= sizeof(USB_SETUP_PACKET);

        //
        // Account for both the setup and status transfers.
        //

        TransferCount += 2;
    }

    ForceShortTransfer = FALSE;
    if ((Transfer->Public.Flags &
         USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) {

        ForceShortTransfer = TRUE;
    }

    //
    // The required number of transfers for the data can be obtained by
    // dividing the total length by the maximum packet size. An additional
    // transfer is necessary for a remaining short transfer or if a short
    // transfer must be forced in order to complete the whole transaction.
    // Non-control zero length transfers also need to have at least one
    // transfer.
    //

    TransferCount += TotalLength / Endpoint->MaxPacketSize;
    if (((TotalLength % Endpoint->MaxPacketSize) != 0) ||
        ((TotalLength == 0) &&
         (Endpoint->TransferType != UsbTransferTypeControl)) ||
        (ForceShortTransfer != FALSE)) {

        TransferCount += 1;
    }

    Offset = 0;
    CurrentEntry = Queue->TransferListHead.Next;

    //
    // Acquire the lock, if required. It is acquired here as opposed to after
    // the transfer descriptors are filled out to protect the endpoint's data
    // toggle bit, which needs to be sequential even if multiple transfers are
    // being submitted simultaneously.
    //

    if (LockNotRequired == FALSE) {
        OldRunLevel = UhcipAcquireControllerLock(Controller);
    }

    LastTransfer = FALSE;
    for (TransferIndex = 0; TransferIndex < TransferCount; TransferIndex += 1) {

        //
        // Calculate the length for this transfer descriptor.
        //

        Length = Endpoint->MaxPacketSize;
        if (Offset + Length > Transfer->Public.Length) {
            Length = Transfer->Public.Length - Offset;
        }

        if (TransferIndex == (TransferCount - 1)) {
            LastTransfer = TRUE;
        }

        if (ControlTransfer != FALSE) {

            //
            // The first part of a control transfer is the setup packet, which
            // is always 8 bytes long.
            //

            if (Offset == 0) {
                Length = sizeof(USB_SETUP_PACKET);
            }

            //
            // The last part of a control transfer is the status phase and it
            // must be zero in length.
            //

            ASSERT((LastTransfer == FALSE) || (Length == 0));

        }

        ASSERT((Length != 0) ||
               (LastTransfer != FALSE) ||
               ((ForceShortTransfer != FALSE) && (ControlTransfer != FALSE)));

        //
        // Fill out this transfer descriptor.
        //

        ASSERT(CurrentEntry != &(Queue->TransferListHead));

        UhciTransfer = LIST_VALUE(CurrentEntry, UHCI_TRANSFER, QueueListEntry);
        UhcipFillOutTransferDescriptor(Controller,
                                       Endpoint,
                                       Queue,
                                       UhciTransfer,
                                       Transfer,
                                       Offset,
                                       Length,
                                       LastTransfer);

        //
        // Move on to the next descriptor.
        //

        CurrentEntry = CurrentEntry->Next;
        Offset += Length;
    }

    //
    // Terminate the last transaction filled out.
    //

    UhciTransfer->HardwareTransfer.LinkPointer =
                                       UHCI_TRANSFER_DESCRIPTOR_LINK_TERMINATE;

    Queue->LastTransfer = UhciTransfer;

    //
    // For control transfers, remember the link value that points to the last
    // transfer.
    //

    if (Transfer->Type == UsbTransferTypeControl) {
        UhciTransfer = LIST_VALUE(UhciTransfer->QueueListEntry.Previous,
                                  UHCI_TRANSFER,
                                  QueueListEntry);

        Queue->LinkToLastTransfer = UhciTransfer->HardwareTransfer.LinkPointer;
    }

    //
    // The transfer is ready to go. Do the actual insertion.
    //

    if (Transfer->Type == UsbTransferTypeIsochronous) {

        //
        // TODO: Implement support for isochronous transfers.
        //

        ASSERT(FALSE);

    //
    // If this is not an isochronous transfer, put the transfer in the hardware
    // queue head corresponding to its endpoint.
    //

    } else {
        INSERT_BEFORE(&(Queue->EndpointListEntry), &(Endpoint->QueueListHead));

        //
        // The async schedule looks something like this. Forgive the ASCII art.
        // ControlQueue -> EP0,Q0 -> EPX,Q0 -> Interrupt Queue -> ...
        //                   |     //   |     /
        //                 EP0,Q1_// EPX, Q1_/
        //                   TD   |     TD
        //                   TD   |
        //                   ...  |
        //                 EP0,Q2/
        //                   TD
        //                   ...
        //
        // Queues encaspulate the many transfer descriptors that make up a
        // single USB Transfer. All the transfers for an single endpoint run
        // vertically, and all link pointers for that endpoint point at the
        // next endpoint's column of stuff (so that if something stalls in an
        // endpoint, the controller moves on to other work).
        //
        // If this is the only queue/transfer in the endpoint, then link onto
        // the global queues.
        //

        if (Queue->EndpointListEntry.Previous == &(Endpoint->QueueListHead)) {
            InGlobalList = TRUE;
            if (Transfer->Type == UsbTransferTypeControl) {
                QueueBefore = Controller->ControlQueue;

            } else if (Transfer->Type == UsbTransferTypeInterrupt) {
                QueueBefore = Controller->InterruptQueue;

            } else {

                ASSERT(Transfer->Type == UsbTransferTypeBulk);
                ASSERT(LIST_EMPTY(&(Controller->QueueListHead)) == FALSE);

                QueueBefore = LIST_VALUE(Controller->QueueListHead.Previous,
                                         UHCI_TRANSFER_QUEUE,
                                         GlobalListEntry);
            }

        //
        // There are other transfer queues in for this endpoint, so link onto
        // the last transfer descriptor of the last queue.
        //

        } else {
            InGlobalList = FALSE;
            QueueBefore = LIST_VALUE(Queue->EndpointListEntry.Previous,
                                     UHCI_TRANSFER_QUEUE,
                                     EndpointListEntry);
        }

        INSERT_AFTER(&(Queue->GlobalListEntry),
                     &(QueueBefore->GlobalListEntry));

        //
        // Set the link of this queue to point wherever the previous queue
        // pointed.
        //

        Queue->HardwareQueueHead.LinkPointer =
                                    QueueBefore->HardwareQueueHead.LinkPointer;

        UhcipFlushCacheRegion(&(Queue->HardwareQueueHead),
                              sizeof(UHCI_QUEUE_HEAD));

        //
        // If being inserted into the global list, then insert this queue into
        // the chain.
        //

        if (InGlobalList != FALSE) {
            QueueBefore->HardwareQueueHead.LinkPointer =
               (ULONG)Queue->PhysicalAddress | UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD;

            UhcipFlushCacheRegion(&(QueueBefore->HardwareQueueHead),
                                  sizeof(UHCI_QUEUE_HEAD));

        //
        // If this queue goes on the tail of another queue, find the last
        // transfer descriptor of the previous queue and stick it there.
        //

        } else {

            ASSERT(LIST_EMPTY(&(QueueBefore->TransferListHead)) == FALSE);

            PreviousLastTransfer = QueueBefore->LastTransfer;

            ASSERT(PreviousLastTransfer->HardwareTransfer.LinkPointer ==
                   UHCI_TRANSFER_DESCRIPTOR_LINK_TERMINATE);

            PreviousLastTransfer->HardwareTransfer.LinkPointer =
                (ULONG)Queue->PhysicalAddress |
                UHCI_TRANSFER_DESCRIPTOR_LINK_QUEUE_HEAD;

            UhcipFlushCacheRegion(&(QueueBefore->HardwareQueueHead),
                                  sizeof(UHCI_TRANSFER_DESCRIPTOR));

            //
            // There was just a race between this routine setting the new link
            // and the controller reading and recording the old terminate. If
            // the queue before has already got a terminate in it, then set
            // the next element to this queue head so the controller finds this
            // queue.
            //

            if (QueueBefore->HardwareQueueHead.ElementLink ==
                UHCI_QUEUE_HEAD_LINK_TERMINATE) {

                QueueBefore->HardwareQueueHead.ElementLink =
                                               (ULONG)Queue->PhysicalAddress |
                                               UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD;

                UhcipFlushCacheRegion(&(QueueBefore->HardwareQueueHead),
                                      sizeof(UHCI_QUEUE_HEAD));
            }
        }
    }

    //
    // All done. Release the lock, if required, and return.
    //

    if (LockNotRequired == FALSE) {
        UhcipReleaseControllerLock(Controller, OldRunLevel);
    }

    if (SubmittedTransferCount != NULL) {
        *SubmittedTransferCount = TransferCount;
    }

    return STATUS_SUCCESS;
}

KSTATUS
UhcipCancelTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

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

{

    PUHCI_CONTROLLER Controller;
    RUNLEVEL OldRunLevel;
    PUHCI_TRANSFER_QUEUE Queue;
    KSTATUS Status;

    Controller = (PUHCI_CONTROLLER)HostControllerContext;
    Queue = (PUHCI_TRANSFER_QUEUE)TransferContext;

    ASSERT(Queue->UsbTransfer == Transfer);

    //
    // Lock the controller to manipulate lists.
    //

    OldRunLevel = UhcipAcquireControllerLock(Controller);

    //
    // If the queue was already taken off the global list, then the
    // transfer has already completed.
    //

    if (Queue->GlobalListEntry.Next == NULL) {

        ASSERT(Queue->EndpointListEntry.Next == NULL);

        Status = STATUS_TOO_LATE;
        goto CancelTransferEnd;
    }

    //
    // For successfully cancelled, non-isochronous transfers, send the transfer
    // back to USB core. It will be queued there for full completion, so this
    // call is safe while holding the lock.
    //

    if (Transfer->Type != UsbTransferTypeIsochronous) {
        UhcipRemoveTransferQueue(Controller, Queue, TRUE);
        Transfer->Public.Status = STATUS_OPERATION_CANCELLED;
        Transfer->Public.Error = UsbErrorTransferCancelled;
        UsbHostProcessCompletedTransfer(Transfer);

    } else {

        //
        // TODO: Implement support for isochronous transfers.
        //

        ASSERT(FALSE);
    }

    Status = STATUS_SUCCESS;

CancelTransferEnd:

    //
    // Release the lock and return.
    //

    UhcipReleaseControllerLock(Controller, OldRunLevel);
    return Status;
}

KSTATUS
UhcipGetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    )

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

{

    USHORT ChangeBits;
    PUHCI_CONTROLLER Controller;
    USHORT HardwareStatus;
    ULONG PortIndex;
    PUSB_PORT_STATUS PortStatus;
    UHCI_REGISTER Register;
    USHORT SoftwareStatus;

    ASSERT(HubStatus->PortStatus != NULL);

    Controller = (PUHCI_CONTROLLER)HostControllerContext;
    for (PortIndex = 0; PortIndex < UHCI_PORT_COUNT; PortIndex += 1) {

        //
        // Read the hardware register.
        //

        if (PortIndex == 0) {
            Register = UhciRegisterPort1StatusControl;

        } else {

            ASSERT(PortIndex == 1);

            Register = UhciRegisterPort2StatusControl;
        }

        HardwareStatus = UHCI_READ_REGISTER(Controller, Register);

        //
        // Set the corresponding software bits.
        //

        SoftwareStatus = 0;
        if ((HardwareStatus & UHCI_PORT_DEVICE_CONNECTED) != 0) {
            SoftwareStatus |= USB_PORT_STATUS_CONNECTED;
            if ((HardwareStatus & UHCI_PORT_LOW_SPEED) != 0) {
                HubStatus->PortDeviceSpeed[PortIndex] = UsbDeviceSpeedLow;

            } else {
                HubStatus->PortDeviceSpeed[PortIndex] = UsbDeviceSpeedFull;
            }
        }

        if ((HardwareStatus & UHCI_PORT_ENABLED) != 0) {
            SoftwareStatus |= USB_PORT_STATUS_ENABLED;
        }

        if ((HardwareStatus & UHCI_PORT_RESET) != 0) {
            SoftwareStatus |= USB_PORT_STATUS_RESET;
        }

        //
        // If the new software status is different from the current status,
        // then set the appropriate change bits and update the status.
        //

        PortStatus = &(HubStatus->PortStatus[PortIndex]);
        if (SoftwareStatus != PortStatus->Status) {
            ChangeBits = SoftwareStatus ^ PortStatus->Status;

            //
            // Since the status bits are 1-to-1 with the change bits, just OR
            // in the new bits.
            //

            PortStatus->Change |= ChangeBits;
            PortStatus->Status = SoftwareStatus;
        }

        //
        // Acknowledge port connection changes in the hardware and set the
        // change bit in the software. This may have been missed above if the
        // port transitions from connected to connected.
        //

        if ((HardwareStatus & UHCI_PORT_CONNECT_STATUS_CHANGED) != 0) {
            PortStatus->Change |= USB_PORT_STATUS_CHANGE_CONNECTED;
            UHCI_WRITE_REGISTER(Controller, Register, HardwareStatus);
        }

        if ((UhciDebugFlags & UHCI_DEBUG_PORTS) != 0) {
            RtlDebugPrint(
                     "UHCI: Controller 0x%x Port %d Status 0x%x. "
                     "Connected %d, LowSpeed %d, Enabled %d, Reset %d, "
                     "Changed %d.\n",
                     Controller,
                     PortIndex,
                     HardwareStatus,
                     (HardwareStatus & UHCI_PORT_DEVICE_CONNECTED) != 0,
                     (HardwareStatus & UHCI_PORT_LOW_SPEED) != 0,
                     (HardwareStatus & UHCI_PORT_ENABLED) != 0,
                     (HardwareStatus & UHCI_PORT_RESET) != 0,
                     (HardwareStatus & UHCI_PORT_CONNECT_STATUS_CHANGED) != 0);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
UhcipSetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    )

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

{

    PUHCI_CONTROLLER Controller;
    USHORT HardwareStatus;
    USHORT OriginalHardwareStatus;
    ULONG PortIndex;
    PUSB_PORT_STATUS PortStatus;
    UHCI_REGISTER Register;
    USHORT RegisterValue;

    Controller = (PUHCI_CONTROLLER)HostControllerContext;
    for (PortIndex = 0; PortIndex < UHCI_PORT_COUNT; PortIndex += 1) {

        //
        // The caller is required to notify the routine about what needs to be
        // set by updating the change bits. If there are not changed bits, then
        // skip the port.
        //

        PortStatus = &(HubStatus->PortStatus[PortIndex]);
        if (PortStatus->Change == 0) {
            continue;
        }

        //
        // Read the hardware register.
        //

        if (PortIndex == 0) {
            Register = UhciRegisterPort1StatusControl;

        } else {

            ASSERT(PortIndex == 1);

            Register = UhciRegisterPort2StatusControl;
        }

        OriginalHardwareStatus = UHCI_READ_REGISTER(Controller, Register);
        HardwareStatus = OriginalHardwareStatus;

        //
        // Clear out the bits that may potentially be adjusted.
        //

        HardwareStatus &= ~(UHCI_PORT_RESET |
                            UHCI_PORT_ENABLED |
                            UHCI_PORT_SUSPEND);

        //
        // Set the hardware bits according to what's changed.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_ENABLED) != 0) {
            if ((PortStatus->Status & USB_PORT_STATUS_ENABLED) != 0) {
                HardwareStatus |= UHCI_PORT_ENABLED;
            }

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_ENABLED;
        }

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_RESET) != 0) {
            if ((PortStatus->Status & USB_PORT_STATUS_RESET) != 0) {
                HardwareStatus |= UHCI_PORT_RESET;
            }

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_RESET;
        }

        //
        // Section 2.1.7 of the UHCI Specification says that the PORTSC suspend
        // bit should not be written to 1 if EGSM is set in USBCMD.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_SUSPENDED) != 0) {
            if ((PortStatus->Status & USB_PORT_STATUS_SUSPENDED) != 0) {
                RegisterValue = UHCI_READ_REGISTER(Controller,
                                                   UhciRegisterUsbCommand);

                if ((RegisterValue & UHCI_COMMAND_ENTER_GLOBAL_SUSPEND) == 0) {
                    HardwareStatus |= UHCI_PORT_SUSPEND;
                }
            }

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_SUSPENDED;
        }

        //
        // Write out the new value if it is different than the old one.
        //

        if (HardwareStatus != OriginalHardwareStatus) {
            UHCI_WRITE_REGISTER(Controller, Register, HardwareStatus);
        }

        //
        // If reset was set, wait the required amount of time and then clear
        // the reset bit, as if this were a hub and it was cleared
        // automatically.
        //

        if ((HardwareStatus & UHCI_PORT_RESET) != 0) {
            HlBusySpin(20 * 1000);
            HardwareStatus &= ~UHCI_PORT_RESET;
            UHCI_WRITE_REGISTER(Controller, Register, HardwareStatus);
        }
    }

    return STATUS_SUCCESS;
}

RUNLEVEL
UhcipAcquireControllerLock (
    PUHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine acquires the given UHCI controller's lock at dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller to lock.

Return Value:

    Returns the previous run-level, which must be passed in when the controller
    is unlocked.

--*/

{

    RUNLEVEL OldRunLevel;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Controller->Lock));
    return OldRunLevel;
}

VOID
UhcipReleaseControllerLock (
    PUHCI_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the given UHCI controller's lock, and returns the
    run-level to its previous value.

Arguments:

    Controller - Supplies a pointer to the controller to unlock.

    OldRunLevel - Supplies the original run level returned when the lock was
        acquired.

Return Value:

    None.

--*/

{

    KeReleaseSpinLock(&(Controller->Lock));
    KeLowerRunLevel(OldRunLevel);
    return;
}

VOID
UhcipWaitForNextFrame (
    PUHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine does not return until the UHCI hardware controller has
    advanced at least one frame.

Arguments:

    Controller - Supplies a pointer to the controller to wait for.

Return Value:

    None.

--*/

{

    ULONG CurrentFrame;
    RUNLEVEL RunLevel;

    RunLevel = KeGetRunLevel();
    CurrentFrame = UHCI_READ_REGISTER(Controller, UhciRegisterFrameNumber);
    while (UHCI_READ_REGISTER(Controller, UhciRegisterFrameNumber) ==
           CurrentFrame) {

        if (RunLevel < RunLevelDispatch) {
            KeYield();
        }
    }

    return;
}

VOID
UhcipProcessInterrupt (
    PUHCI_CONTROLLER Controller,
    ULONG PendingStatus
    )

/*++

Routine Description:

    This routine performs the work associated with receiving a UHCI interrupt.
    This routine runs at dispatch level.

Arguments:

    Controller - Supplies a pointer to the UHCI controller.

    PendingStatus - Supplies the pending status bits to deal with.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentQueueEntry;
    PLIST_ENTRY CurrentTransferEntry;
    RUNLEVEL OldRunLevel;
    PUHCI_TRANSFER_QUEUE Queue;
    BOOL RemoveQueue;
    PUHCI_TRANSFER Transfer;

    //
    // Lock the controller and loop until this routine has caught up with the
    // interrupts.
    //

    OldRunLevel = UhcipAcquireControllerLock(Controller);

    //
    // TODO: Go through the isochronous transfers.
    //

    //
    // Loop through every queue in the schedule.
    //

    CurrentQueueEntry = Controller->QueueListHead.Next;
    while (CurrentQueueEntry != &(Controller->QueueListHead)) {
        Queue = LIST_VALUE(CurrentQueueEntry,
                           UHCI_TRANSFER_QUEUE,
                           GlobalListEntry);

        CurrentQueueEntry = CurrentQueueEntry->Next;

        //
        // Loop through every transfer in the queue.
        //

        RemoveQueue = FALSE;
        CurrentTransferEntry = Queue->TransferListHead.Next;
        while (CurrentTransferEntry != &(Queue->TransferListHead)) {
            Transfer = LIST_VALUE(CurrentTransferEntry,
                                  UHCI_TRANSFER,
                                  QueueListEntry);

            CurrentTransferEntry = CurrentTransferEntry->Next;

            //
            // Examine the tranfser, and determine whether or not it's
            // failed.
            //

            RemoveQueue = UhcipProcessPotentiallyCompletedTransfer(Queue,
                                                                   Transfer);

            if ((RemoveQueue != FALSE) || (Transfer == Queue->LastTransfer)) {
                break;
            }
        }

        //
        // If the queue isn't already slated to be removed, look to see
        // if it is empty. Unless it is one of the sentinal queues, empty
        // queues should be removed.
        //

        if ((RemoveQueue == FALSE) &&
            ((Queue->HardwareQueueHead.ElementLink &
             UHCI_QUEUE_HEAD_LINK_TERMINATE) != 0) &&
            (Queue != Controller->ControlQueue) &&
            (Queue != Controller->InterruptQueue)) {

            RemoveQueue = TRUE;
        }

        //
        // If necessary, remove the queue from the schedule and call the USB
        // host to notify USB core that the transfer is done. This is safe to
        // do at dispatch level because the USB core queues any real work.
        //

        if (RemoveQueue != FALSE) {
            UhcipRemoveTransferQueue(Controller, Queue, FALSE);
            UsbHostProcessCompletedTransfer(Queue->UsbTransfer);
        }
    }

    //
    // Release the controller lock.
    //

    UhcipReleaseControllerLock(Controller, OldRunLevel);
    return;
}

VOID
UhcipFillOutTransferDescriptor (
    PUHCI_CONTROLLER Controller,
    PUHCI_ENDPOINT Endpoint,
    PUHCI_TRANSFER_QUEUE Queue,
    PUHCI_TRANSFER UhciTransfer,
    PUSB_TRANSFER_INTERNAL Transfer,
    ULONG Offset,
    ULONG Length,
    BOOL LastTransfer
    )

/*++

Routine Description:

    This routine fills out a UHCI transfer descriptor.

Arguments:

    Controller - Supplies a pointer to the UHCI controller.

    Endpoint - Supplies a pointer to the endpoint the transfer will go on.

    Queue - Supplies an optional pointer to the transfer queue the transfer
        is going on.

    UhciTransfer - Supplies a pointer to UHCI's transfer descriptor information.

    Transfer - Supplies a pointer to the core USB library transfer.

    Offset - Supplies the offset from the public transfer physical address that
        this transfer descriptor should be initialize to.

    Length - Supplies the length of the transfer, in bytes.

    LastTransfer - Supplies a boolean indicating if this transfer descriptor
        represents the last transfer in a set. For control transfers, this is
        the status phase where the in/out is reversed and the length had better
        be zero.

Return Value:

    None.

--*/

{

    ULONG Control;
    PUHCI_TRANSFER PreviousTransfer;
    BOOL Setup;
    ULONG Token;

    Setup = FALSE;

    //
    // Set up the token field of the hardware transfer descriptor.
    //

    UhciTransfer->HardwareTransfer.BufferPointer =
                               Transfer->Public.BufferPhysicalAddress + Offset;

    Token = (Length - 1) <<
            UHCI_TRANSFER_DESCRIPTOR_TOKEN_MAX_LENGTH_SHIFT;

    Token |= (Endpoint->EndpointNumber & USB_ENDPOINT_ADDRESS_MASK) <<
             UHCI_TRANSFER_DESCRIPTOR_TOKEN_ENDPOINT_SHIFT;

    Token |= Transfer->DeviceAddress <<
             UHCI_TRANSFER_DESCRIPTOR_TOKEN_ADDRESS_SHIFT;

    //
    // The first packet in a control transfer is always a setup packet.
    //

    if ((Endpoint->TransferType == UsbTransferTypeControl) && (Offset == 0)) {
        Token |= USB_PID_SETUP;
        Endpoint->DataToggle = FALSE;
        Setup = TRUE;

    //
    // Do it backwards if this is the status phase. Status phases always have
    // a data toggle of 1.
    //

    } else if ((Endpoint->TransferType == UsbTransferTypeControl) &&
               (LastTransfer != FALSE)) {

        Endpoint->DataToggle = TRUE;

        ASSERT((Length == 0) &&
               (Endpoint->TransferType == UsbTransferTypeControl));

        if (Transfer->Public.Direction == UsbTransferDirectionIn) {
            Token |= USB_PID_OUT;

        } else {

            ASSERT(Transfer->Public.Direction == UsbTransferDirectionOut);

            Token |= USB_PID_IN;
        }

    //
    // Not setup and not status, fill this out like a normal descriptor.
    //

    } else {
        if (Transfer->Public.Direction == UsbTransferDirectionIn) {
            Token |= USB_PID_IN;

        } else {

            ASSERT(Transfer->Public.Direction == UsbTransferDirectionOut);

            Token |= USB_PID_OUT;
        }
    }

    ASSERT(UhciTransfer->HardwareTransfer.Token == 0);

    UhciTransfer->HardwareTransfer.Token = Token;

    //
    // Set up the control/status field of the hardware transfer descriptor.
    // Avoid setting the short packet detect bit if the caller specified not
    // to allow short transfers.
    //

    Control = UHCI_TRANSFER_DESCRIPTOR_STATUS_ACTIVE;
    if ((Setup == FALSE) &&
        ((Transfer->Public.Flags &
          USB_TRANSFER_FLAG_NO_SHORT_TRANSFERS) == 0)) {

        Control |= UHCI_TRANSFER_DESCRIPTOR_STATUS_SHORT_PACKET;
    }

    ASSERT((Endpoint->Speed == UsbDeviceSpeedLow) ||
           (Endpoint->Speed == UsbDeviceSpeedFull));

    if ((Queue != NULL) && (Endpoint->Speed == UsbDeviceSpeedLow)) {
        Control |= UHCI_TRANSFER_DESCRIPTOR_STATUS_LOW_SPEED;
    }

    //
    // Mark isochronous transfers. For all other transfer types, set the error
    // count to 3. Isochronous transfers do not get an error count because the
    // active bit is always set to 0 by the hardware after execution,
    // regardless of the result.
    //

    if (Transfer->Type == UsbTransferTypeIsochronous) {
        Control |= UHCI_TRANSFER_DESCRIPTOR_STATUS_ISOCHRONOUS;

    } else {
        Control |= UHCI_TRANSFER_DESCRIPTOR_STATUS_3_ERRORS;
    }

    //
    // Don't set the interrupt flag if 1) This is not the last descriptor or
    // 2) The caller requested not to.
    //

    if ((LastTransfer != FALSE) &&
        ((Transfer->Public.Flags &
          USB_TRANSFER_FLAG_NO_INTERRUPT_ON_COMPLETION) == 0)) {

        Control |= UHCI_TRANSFER_DESCRIPTOR_STATUS_INTERRUPT;
    }

    UhciTransfer->HardwareTransfer.Status = Control;

    //
    // Set up the link pointer of the transfer descriptor. With the exception
    // of isochronous transfers (which will get patched up later) transfer
    // descriptors are always put at the end of the queue.
    //

    UhciTransfer->HardwareTransfer.LinkPointer =
                                       UHCI_TRANSFER_DESCRIPTOR_LINK_TERMINATE;

    if (Transfer->Type == UsbTransferTypeIsochronous) {

        //
        // TODO: Implement support for isochronous transfers.
        //

        ASSERT(FALSE);

    //
    // If the transfer is not isochronous, set the data toggle bit.
    //

    } else {
        if (Endpoint->DataToggle != FALSE) {
            UhciTransfer->HardwareTransfer.Token |=
                                    UHCI_TRANSFER_DESCRIPTOR_TOKEN_DATA_TOGGLE;

            Endpoint->DataToggle = FALSE;

        } else {
            Endpoint->DataToggle = TRUE;
        }
    }

    if ((UhciDebugFlags & UHCI_DEBUG_TRANSFERS) != 0) {
        RtlDebugPrint("UHCI: Adding transfer (0x%08x) to endpoint (0x%08x): "
                      "Status: 0x%08x, Token 0x%08x.\n",
                      UhciTransfer,
                      Endpoint,
                      UhciTransfer->HardwareTransfer.Status,
                      UhciTransfer->HardwareTransfer.Token);
    }

    //
    // If this is not an isochronous transfer, fix up the hardware links so that
    // this transfer goes at the back of the list.
    //

    if (Transfer->Type != UsbTransferTypeIsochronous) {

        //
        // If this is the first element in the list, set the queue's vertical
        // link pointer directly.
        //

        if (UhciTransfer->QueueListEntry.Previous ==
            &(Queue->TransferListHead)) {

            Queue->HardwareQueueHead.ElementLink =
                                          (ULONG)UhciTransfer->PhysicalAddress;

        //
        // If the queue is not empty, use the previous transfer descriptor in
        // the software list to insert it into the hardware list.
        //

        } else {
            PreviousTransfer = LIST_VALUE(UhciTransfer->QueueListEntry.Previous,
                                          UHCI_TRANSFER,
                                          QueueListEntry);

            ASSERT((PreviousTransfer->HardwareTransfer.LinkPointer &
                    UHCI_QUEUE_HEAD_LINK_TERMINATE) != 0);

            PreviousTransfer->HardwareTransfer.LinkPointer =
                                          (ULONG)UhciTransfer->PhysicalAddress;
        }
    }

    UhcipFlushCacheRegion(&(UhciTransfer->HardwareTransfer),
                          sizeof(UHCI_TRANSFER_DESCRIPTOR));

    UhcipFlushCacheRegion(&(Queue->HardwareQueueHead), sizeof(UHCI_QUEUE_HEAD));
    return;
}

BOOL
UhcipProcessPotentiallyCompletedTransfer (
    PUHCI_TRANSFER_QUEUE Queue,
    PUHCI_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine processes a transfer descriptor, adjusting the USB transfer if
    the transfer descriptor errored out.

Arguments:

    Queue - Supplies a pointer to the transfer queue this transfer is operating
        under.

    Transfer - Supplies a pointer to the transfer to evaluate.

Return Value:

    TRUE if the queue should be removed from the list because the transfer has
        failed.

    FALSE if the queue should not be removed from the list.

--*/

{

    ULONG CrcOrTimeoutError;
    ULONG DataBufferError;
    ULONG ElementLinkPhysicalAddress;
    ULONG HardwareStatus;
    ULONG LengthTransferred;
    ULONG MaxLength;
    BOOL NewToggle;
    PUHCI_TRANSFER NextTransfer;
    BOOL RemoveQueue;
    PUSB_TRANSFER UsbTransfer;

    RemoveQueue = FALSE;

    //
    // If the transfer has a zero token, then it's already been dealt with, so
    // stop looking.
    //

    if (Transfer->HardwareTransfer.Token == 0) {
        goto ProcessPotentiallyCompletedTransferEnd;
    }

    HardwareStatus = Transfer->HardwareTransfer.Status;
    if ((HardwareStatus &
        UHCI_TRANSFER_DESCRIPTOR_STATUS_ACTIVE) == 0) {

        if ((UhciDebugFlags & UHCI_DEBUG_TRANSFERS) != 0) {
            RtlDebugPrint("UHCI: Transfer (0x%08x) completed with status "
                          "0x%08x, token 0x%08x\n",
                          Transfer,
                          HardwareStatus,
                          Transfer->HardwareTransfer.Token);
        }

        UsbTransfer = &(Queue->UsbTransfer->Public);
        LengthTransferred =
            ((HardwareStatus + 1) &
             UHCI_TRANSFER_DESCRIPTOR_STATUS_ACTUAL_LENGTH_MASK);

        UsbTransfer->LengthTransferred += LengthTransferred;

        //
        // If error bits were set, it's curtains for this transfer. Figure out
        // exactly what went wrong. A halted error is first in line even if
        // another bit (e.g. Babble) is set, because the driver may want to
        // clear the halted state.
        //

        if ((HardwareStatus &
             UHCI_TRANSFER_DESCRIPTOR_STATUS_ERROR_MASK) != 0) {

            RemoveQueue = TRUE;
            UsbTransfer->Status = STATUS_DEVICE_IO_ERROR;
            DataBufferError = UHCI_TRANSFER_DESCRIPTOR_STATUS_DATA_BUFFER_ERROR;
            CrcOrTimeoutError = UHCI_TRANSFER_DESCRIPTOR_STATUS_CRC_OR_TIMEOUT;
            if ((HardwareStatus & DataBufferError) != 0) {
                UsbTransfer->Error = UsbErrorTransferDataBuffer;

            } else if ((HardwareStatus &
                       UHCI_TRANSFER_DESCRIPTOR_STATUS_BABBLE) != 0) {

                UsbTransfer->Error = UsbErrorTransferBabbleDetected;

            } else if ((HardwareStatus &
                       UHCI_TRANSFER_DESCRIPTOR_STATUS_NAK) != 0) {

                UsbTransfer->Error = UsbErrorTransferNakReceived;

            } else if ((HardwareStatus & CrcOrTimeoutError) != 0) {
                UsbTransfer->Error = UsbErrorTransferCrcOrTimeoutError;

            } else if ((HardwareStatus &
                        UHCI_TRANSFER_DESCRIPTOR_STATUS_STALLED) != 0) {

                UsbTransfer->Error = UsbErrorTransferStalled;
            }

            //
            // If the transfer was not the last one, fix up the data toggles.
            // A failed transfer does not cause a toggle, so the next queue
            // should have the same toggle as this failed one.
            //

            if (Transfer->QueueListEntry.Next != &(Queue->TransferListHead)) {
                NewToggle = FALSE;
                if ((Transfer->HardwareTransfer.Token &
                     UHCI_TRANSFER_DESCRIPTOR_TOKEN_DATA_TOGGLE) != 0) {

                    NewToggle = TRUE;
                }

                UhcipFixDataToggles(Queue, NewToggle);
            }

        } else {

            //
            // Check to see if it was a short IN transfer.
            //

            MaxLength = (Transfer->HardwareTransfer.Token >>
                         UHCI_TRANSFER_DESCRIPTOR_TOKEN_MAX_LENGTH_SHIFT) + 1;

            if ((LengthTransferred != MaxLength) &&
                (UsbTransfer->Direction == UsbTransferDirectionIn) &&
                (Transfer->QueueListEntry.Next !=
                 &(Queue->TransferListHead))) {

                //
                // For a control transfer, move the queue pointer to the last
                // transfer. Then the queue will complete normally.
                //

                ElementLinkPhysicalAddress =
                                         Queue->HardwareQueueHead.ElementLink &
                                         UHCI_QUEUE_HEAD_LINK_ADDRESS_MASK;

                if ((Queue->UsbTransfer->Type == UsbTransferTypeControl) &&
                    (ElementLinkPhysicalAddress == Transfer->PhysicalAddress)) {

                    ASSERT(Queue->LinkToLastTransfer != 0);

                    Queue->HardwareQueueHead.ElementLink =
                                                     Queue->LinkToLastTransfer;

                    UhcipFlushCacheRegion(&(Queue->HardwareQueueHead),
                                          sizeof(UHCI_QUEUE_HEAD));

                } else {
                    RemoveQueue = TRUE;
                    if ((UsbTransfer->Flags &
                         USB_TRANSFER_FLAG_NO_SHORT_TRANSFERS) != 0) {

                        UsbTransfer->Status = STATUS_DATA_LENGTH_MISMATCH;
                        UsbTransfer->Error = UsbErrorShortPacket;
                    }

                    //
                    // If the short packet was not the last transfer descriptor
                    // then the upcoming data toggles need to be fixed up. The
                    // packet was short but successful, so the next queue
                    // should have the opposite toggle of this one.
                    //

                    if (Transfer->QueueListEntry.Next !=
                        &(Queue->TransferListHead)) {

                        NewToggle = FALSE;
                        if ((Transfer->HardwareTransfer.Token &
                             UHCI_TRANSFER_DESCRIPTOR_TOKEN_DATA_TOGGLE) == 0) {

                            NewToggle = TRUE;
                        }

                        UhcipFixDataToggles(Queue, NewToggle);
                    }
                }
            }
        }

        //
        // Clear out the token to indicate this packet has been dealt with.
        //

        Transfer->HardwareTransfer.Token = 0;

        //
        // If this is the last transfer, then signal that processing on this
        // queue is complete.
        //

        if (Transfer->QueueListEntry.Next == &(Queue->TransferListHead)) {
            RemoveQueue = TRUE;

        } else {
            NextTransfer = LIST_VALUE(Transfer->QueueListEntry.Next,
                                      UHCI_TRANSFER,
                                      QueueListEntry);

            if (NextTransfer->HardwareTransfer.Token == 0) {
                RemoveQueue = TRUE;
            }
        }
    }

ProcessPotentiallyCompletedTransferEnd:
    return RemoveQueue;
}

VOID
UhcipRemoveTransferQueue (
    PUHCI_CONTROLLER Controller,
    PUHCI_TRANSFER_QUEUE Queue,
    BOOL Cancel
    )

/*++

Routine Description:

    This routine removes a transfer queue from the schedule.  This routine
    assumes that the controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller being operated on.

    Queue - Supplies a pointer to the inserted queue to remove from the
        schedule.

    Cancel - Supplies a boolean indicating if this transfer is being canceled.
        If it is, data toggles for subsequent queues may need to be fixed up.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PUHCI_ENDPOINT Endpoint;
    BOOL NewToggle;
    ULONG NextLink;
    PUHCI_TRANSFER_QUEUE NextQueue;
    PUHCI_ENDPOINT PreviousEndpoint;
    PUHCI_TRANSFER_QUEUE PreviousQueue;
    PUHCI_TRANSFER_QUEUE QueueToFix;
    PUHCI_TRANSFER Transfer;

    Endpoint = Queue->Endpoint;

    ASSERT(Endpoint != NULL);

    PreviousQueue = LIST_VALUE(Queue->GlobalListEntry.Previous,
                               UHCI_TRANSFER_QUEUE,
                               GlobalListEntry);

    PreviousEndpoint = PreviousQueue->Endpoint;

    //
    // Figure out what previous queues pointing at this one should point at
    // instead. They should point at the next queue in this endpoint if there
    // is one, or whatever this queue's link pointer is pointing at if not.
    //

    if (Queue->EndpointListEntry.Next != &(Endpoint->QueueListHead)) {
        NextQueue = LIST_VALUE(Queue->EndpointListEntry.Next,
                               UHCI_TRANSFER_QUEUE,
                               EndpointListEntry);

        NextLink = (ULONG)(NextQueue->PhysicalAddress) |
                   UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD;

    } else {
        NextLink = Queue->HardwareQueueHead.LinkPointer;
    }

    //
    // If this is the first entry in the endpoint, then potentially many queue
    // heads point at it and need to be fixed up.
    //

    if (Endpoint->QueueListHead.Next == &(Queue->EndpointListEntry)) {

        //
        // If there's a previous endpoint, then for each queue in that endpoint
        // point to the next queue in this endpoint.
        //

        if (PreviousEndpoint != NULL) {
            CurrentEntry = PreviousEndpoint->QueueListHead.Next;
            while (CurrentEntry != &(PreviousEndpoint->QueueListHead)) {
                QueueToFix = LIST_VALUE(CurrentEntry,
                                        UHCI_TRANSFER_QUEUE,
                                        EndpointListEntry);

                //
                // The queue should already point at this queue. Fix it up to
                // point beyond.
                //

                ASSERT(QueueToFix->HardwareQueueHead.LinkPointer ==
                       ((ULONG)(Queue->PhysicalAddress) |
                        UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD));

                QueueToFix->HardwareQueueHead.LinkPointer = NextLink;
                UhcipFlushCacheRegion(&(QueueToFix->HardwareQueueHead),
                                      sizeof(UHCI_QUEUE_HEAD));

                CurrentEntry = CurrentEntry->Next;
            }

        //
        // There is no previous endpoint, so the previous queue is a sentinal
        // queue. Just move its link.
        //

        } else {

            ASSERT(LIST_EMPTY(&(PreviousQueue->TransferListHead)) != FALSE);
            ASSERT(PreviousQueue->HardwareQueueHead.LinkPointer ==
                   ((ULONG)(Queue->PhysicalAddress) |
                    UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD));

            PreviousQueue->HardwareQueueHead.LinkPointer = NextLink;
        }

    //
    // This is not the first queue in the endpoint, so only the previous queue
    // points to it.
    //

    } else {
        QueueToFix = LIST_VALUE(Queue->EndpointListEntry.Previous,
                                UHCI_TRANSFER_QUEUE,
                                EndpointListEntry);

        ASSERT(QueueToFix->LastTransfer->HardwareTransfer.LinkPointer ==
               ((ULONG)(Queue->PhysicalAddress) |
                UHCI_TRANSFER_DESCRIPTOR_LINK_QUEUE_HEAD));

        QueueToFix->LastTransfer->HardwareTransfer.LinkPointer = NextLink;
    }

    //
    // Wait for the next frame to ensure that the controller isn't sitting on
    // this just-removed queue head.
    //

    UhcipWaitForNextFrame(Controller);

    //
    // The queue and all transfers are now no longer visible to the hardware.
    // Clear the token fields of all transfers.
    //

    CurrentEntry = Queue->TransferListHead.Next;
    while (CurrentEntry != &(Queue->TransferListHead)) {
        Transfer = LIST_VALUE(CurrentEntry, UHCI_TRANSFER, QueueListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // If the queue was cancelled (meaning it was ripped out from under the
        // controller) and this transfer is still active, then fix up the
        // data toggles for subsequent queues. Because the transfer was never
        // completed, the next queue should have the same toggle bit as this
        // one.
        //

        if ((Cancel != FALSE) &&
            ((Transfer->HardwareTransfer.Status &
              UHCI_TRANSFER_DESCRIPTOR_STATUS_ACTIVE) != 0)) {

            NewToggle = FALSE;
            if ((Transfer->HardwareTransfer.Token &
                 UHCI_TRANSFER_DESCRIPTOR_TOKEN_DATA_TOGGLE) != 0) {

                NewToggle = TRUE;
            }

            UhcipFixDataToggles(Queue, NewToggle);

            //
            // Prevent this fixup from happening multiple times.
            //

            Cancel = FALSE;
        }

        Transfer->HardwareTransfer.Token = 0;
    }

    //
    // Finally, pull the queue out of the software lists.
    //

    LIST_REMOVE(&(Queue->GlobalListEntry));
    Queue->GlobalListEntry.Next = NULL;
    LIST_REMOVE(&(Queue->EndpointListEntry));
    Queue->EndpointListEntry.Next = NULL;
    return;
}

VOID
UhcipPortStatusDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the UHCI DPC that is fired when the port status
    timer expires.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    BOOL Changed;
    PUHCI_CONTROLLER Controller;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);

    //
    // Test to see if the UHCI ports have changed. If they have, then call USB
    // core to notify it of the change.
    //

    Controller = (PUHCI_CONTROLLER)Dpc->UserData;
    Changed = UhcipHasPortStatusChanged(Controller);
    if (Changed != FALSE) {
        UsbHostNotifyPortChange(Controller->UsbCoreHandle);
    }

    return;
}

BOOL
UhcipHasPortStatusChanged (
    PUHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine determines if the port status and control registers have
    changed for the root hub of the USB host controller.

Arguments:

    Controller - Supplies a pointer to the UHCI host controller whose port
        status needs to be queried for changes.

Return Value:

    Returns TRUE if the port status and control registers have changed, or
    FALSE otherwise.

--*/

{

    BOOL Changed;
    USHORT HardwareStatus;
    ULONG PortIndex;
    UHCI_REGISTER Register;

    ASSERT(Controller != NULL);

    //
    // Loop through each UHCI host controller port to see if it's connection
    // status has changed.
    //

    Changed = FALSE;
    for (PortIndex = 0; PortIndex < UHCI_PORT_COUNT; PortIndex += 1) {

        //
        // Read the hardware register.
        //

        if (PortIndex == 0) {
            Register = UhciRegisterPort1StatusControl;

        } else {

            ASSERT(PortIndex == 1);

            Register = UhciRegisterPort2StatusControl;
        }

        //
        // If any port's connection status has changed, exit reporting a
        // change.
        //

        HardwareStatus = UHCI_READ_REGISTER(Controller, Register);
        if ((HardwareStatus & UHCI_PORT_CONNECT_STATUS_CHANGED) != 0) {
            Changed = TRUE;
            if ((UhciDebugFlags & UHCI_DEBUG_PORTS) != 0) {
                RtlDebugPrint("UHCI: Controller 0x%x, Port %d changed. "
                              "Status 0x%x\n.",
                              Controller,
                              PortIndex,
                              HardwareStatus);
            }

            break;
        }
    }

    return Changed;
}

VOID
UhcipFlushCacheRegion (
    PVOID VirtualAddress,
    ULONG Size
    )

/*++

Routine Description:

    This routine flushes the given region of memory for visibility to the
    hardware.

Arguments:

    VirtualAddress - Supplies the virtual address of the region to flush.

    Size - Supplies the number of bytes to flush.

Return Value:

    None.

--*/

{

    //
    // UHCI currently only runs on x86 architectures, and x86 architectures
    // are cache coherent, so no action is needed here. Fill this in if UHCI
    // is ever implemented on an architecture with a weaker memory model.
    //

    return;
}

VOID
UhcipFixDataToggles (
    PUHCI_TRANSFER_QUEUE RemovingQueue,
    BOOL Toggle
    )

/*++

Routine Description:

    This routine fixes up the data toggle bits for every queue after the given
    one. It is called when a packet comes in short, errors out, or is cancelled.

Arguments:

    RemovingQueue - Supplies a pointer to the queue that is disappearing.
        Every queue after this one in the endpoint will be fixed up.

    Toggle - Supplies the toggle value that the first transfer in the next
        queue should have.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PUHCI_ENDPOINT Endpoint;
    ULONG NewToken;
    PUHCI_TRANSFER_QUEUE Queue;
    PUHCI_TRANSFER Transfer;
    PLIST_ENTRY TransferEntry;

    Endpoint = RemovingQueue->Endpoint;
    if ((Endpoint->TransferType == UsbTransferTypeControl) ||
        (Endpoint->TransferType == UsbTransferTypeIsochronous)) {

        return;
    }

    if ((UhciDebugFlags & UHCI_DEBUG_TRANSFERS) != 0) {
        RtlDebugPrint("UHCI: Fixing data toggles for Endpoint 0x%x 0x%x, "
                      "RemovingQueue 0x%x, Toggle 0x%x\n",
                      Endpoint->EndpointNumber,
                      Endpoint,
                      RemovingQueue,
                      Toggle);
    }

    //
    // Loop through every remaining queue in the endpoint. The USB spec says
    // devices should simply ignore packets that come in with the wrong data
    // toggle, so it's okay to fix these up live as long as they're fixed up
    // in order.
    //

    CurrentEntry = RemovingQueue->EndpointListEntry.Next;
    while (CurrentEntry != &(Endpoint->QueueListHead)) {
        Queue = LIST_VALUE(CurrentEntry,
                           UHCI_TRANSFER_QUEUE,
                           EndpointListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Loop through every transfer in the queue.
        //

        TransferEntry = Queue->TransferListHead.Next;
        while (TransferEntry != &(Queue->TransferListHead)) {
            Transfer = LIST_VALUE(TransferEntry, UHCI_TRANSFER, QueueListEntry);
            TransferEntry = TransferEntry->Next;
            NewToken = Transfer->HardwareTransfer.Token &
                       (~UHCI_TRANSFER_DESCRIPTOR_TOKEN_DATA_TOGGLE);

            ASSERT(NewToken != 0);

            if (Toggle != FALSE) {
                NewToken |= UHCI_TRANSFER_DESCRIPTOR_TOKEN_DATA_TOGGLE;
                Toggle = FALSE;

            } else {
                Toggle = TRUE;
            }

            if (NewToken != Transfer->HardwareTransfer.Token) {
                Transfer->HardwareTransfer.Token = NewToken;
                RtlMemoryBarrier();
                UhcipFlushCacheRegion(&(Transfer->HardwareTransfer),
                                      sizeof(UHCI_TRANSFER));
            }
        }
    }

    Endpoint->DataToggle = Toggle;
    return;
}

