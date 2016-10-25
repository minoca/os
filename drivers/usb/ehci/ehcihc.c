/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ehcihc.c

Abstract:

    This module implements support for the EHCI USB 2.0 Host Controller.

Author:

    Evan Green 18-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/usb/usbhost.h>
#include "ehci.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to a EHCI host controller register.
//

#define EHCI_READ_REGISTER(_Controller, _Register) \
    HlReadRegister32((_Controller)->RegisterBase + _Register)

#define EHCI_WRITE_REGISTER(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->RegisterBase + _Register, _Value);

#define EHCI_READ_PORT_REGISTER(_Controller, _PortIndex) \
    EHCI_READ_REGISTER(                                  \
                   _Controller,                          \
                   EhciRegisterPortStatusBase + ((_PortIndex) * sizeof(ULONG)))

#define EHCI_WRITE_PORT_REGISTER(_Controller, _PortIndex, _Value) \
    EHCI_WRITE_REGISTER(                                                       \
                  _Controller,                                                 \
                  EhciRegisterPortStatusBase + ((_PortIndex) * sizeof(ULONG)), \
                  _Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define values to convert between frames and microframes.
//

#define EHCI_MICROFRAMES_PER_FRAME 8
#define EHCI_MICROFRAMES_PER_FRAME_SHIFT 3

//
// Define EHCI debug flags.
//

#define EHCI_DEBUG_PORTS 0x00000001
#define EHCI_DEBUG_TRANSFERS 0x00000002
#define EHCI_DEBUG_ERRORS 0x00000004

//
// Define the timeout value for the endpoint flush operation.
//

#define EHCI_ENDPOINT_FLUSH_TIMEOUT 10

//
// Define the timeout value for the polled I/O operations.
//

#define EHCI_POLLED_TRANSFER_TIMEOUT 10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
EhcipCreateEndpoint (
    PVOID HostControllerContext,
    PUSB_HOST_ENDPOINT_CREATION_REQUEST Endpoint,
    PVOID *EndpointContext
    );

VOID
EhcipResetEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxPacketSize
    );

KSTATUS
EhcipFlushEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PULONG TransferCount
    );

VOID
EhcipDestroyEndpoint (
    PVOID HostControllerContext,
    PVOID EndpointContext
    );

KSTATUS
EhcipCreateTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    ULONG MaxBufferSize,
    ULONG Flags,
    PVOID *TransferContext
    );

VOID
EhcipDestroyTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PVOID TransferContext
    );

KSTATUS
EhcipSubmitTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
EhcipSubmitPolledTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
EhcipSubmitTransferSet (
    PEHCI_CONTROLLER Controller,
    PEHCI_ENDPOINT Endpoint,
    PEHCI_TRANSFER_SET TransferSet,
    PULONG SubmittedTransferCount,
    BOOL LockRequired
    );

KSTATUS
EhcipCancelTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    );

KSTATUS
EhcipGetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS HubStatus
    );

KSTATUS
EhcipSetRootHubStatus (
    PVOID HostControllerContext,
    PUSB_HUB_STATUS NewStatus
    );

RUNLEVEL
EhcipAcquireControllerLock (
    PEHCI_CONTROLLER Controller
    );

VOID
EhcipReleaseControllerLock (
    PEHCI_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    );

VOID
EhcipProcessInterrupt (
    PEHCI_CONTROLLER Controller,
    ULONG PendingStatusBits
    );

VOID
EhcipFillOutTransferDescriptor (
    PEHCI_CONTROLLER Controller,
    PEHCI_TRANSFER EhciTransfer,
    ULONG Offset,
    ULONG Length,
    BOOL LastTransfer,
    PBOOL DataToggle,
    PEHCI_TRANSFER AlternateNextTransfer
    );

VOID
EhcipLinkTransferSetInHardware (
    PEHCI_TRANSFER_SET TransferSet
    );

BOOL
EhcipProcessPotentiallyCompletedTransfer (
    PEHCI_TRANSFER Transfer
    );

VOID
EhcipRemoveCompletedTransferSet (
    PEHCI_CONTROLLER Controller,
    PEHCI_TRANSFER_SET TransferSet
    );

VOID
EhcipProcessAsyncOnAdvanceInterrupt (
    PEHCI_CONTROLLER Controller
    );

VOID
EhcipRemoveCancelledTransferSet (
    PEHCI_CONTROLLER Controller,
    PEHCI_TRANSFER_SET TransferSet
    );

VOID
EhcipDestroyQueuesWorkRoutine (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define a bitfield of debug flags that enable various print messages for
// EHCI. See EHCI_DEBUG_* definitions.
//

ULONG EhciDebugFlags = 0x0;

//
// ------------------------------------------------------------------ Functions
//

PEHCI_CONTROLLER
EhcipInitializeControllerState (
    PVOID OperationalRegisterBase,
    PHYSICAL_ADDRESS RegisterBasePhysical,
    ULONG PortCount,
    PDEBUG_USB_HANDOFF_DATA HandoffData
    )

/*++

Routine Description:

    This routine initializes the state and variables needed to start up an EHCI
    host controller.

Arguments:

    OperationalRegisterBase - Supplies the virtual address of the base of the
        operational registers.

    RegisterBasePhysical - Supplies the physical address of the base of the
        EHCI registers (not the operational registers).

    PortCount - Supplies the number of ports on the EHCI controller.

    HandoffData - Supplies an optional pointer to the debug handoff data if the
        kernel debugger is using this controller.

Return Value:

    Returns a pointer to the EHCI controller state object on success.

    NULL on failure.

--*/

{

    ULONG BlockSize;
    PEHCI_CONTROLLER Controller;
    PEHCI_DEBUG_HANDOFF_DATA EhciHandoff;
    ULONG Flags;
    ULONG Frame;
    ULONG FrameBits;
    ULONG IoBufferFlags;
    PEHCI_TRANSFER_QUEUE PreviousTransferQueue;
    PEHCI_QUEUE_HEAD QueueHead;
    PHYSICAL_ADDRESS QueueHeadPhysicalAddress;
    ULONG RemainingSize;
    KSTATUS Status;
    PEHCI_TRANSFER_QUEUE TransferQueue;
    ULONG TreeLevel;

    ASSERT(PortCount != 0);

    EhciHandoff = NULL;
    if (HandoffData != NULL) {

        ASSERT(HandoffData->HostDataSize == sizeof(EHCI_DEBUG_HANDOFF_DATA));

        EhciHandoff = HandoffData->HostData;
    }

    //
    // Allocate the controller structure itself.
    //

    Controller = MmAllocateNonPagedPool(sizeof(EHCI_CONTROLLER),
                                        EHCI_ALLOCATION_TAG);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    RtlZeroMemory(Controller, sizeof(EHCI_CONTROLLER));
    INITIALIZE_LIST_HEAD(&(Controller->IsochronousTransferListHead));
    INITIALIZE_LIST_HEAD(&(Controller->TransferListHead));
    INITIALIZE_LIST_HEAD(&(Controller->AsyncOnAdvanceReadyListHead));
    INITIALIZE_LIST_HEAD(&(Controller->AsyncOnAdvancePendingListHead));
    INITIALIZE_LIST_HEAD(&(Controller->QueuesToDestroyListHead));
    INITIALIZE_LIST_HEAD(&(Controller->EndpointListHead));
    Controller->RegisterBase = OperationalRegisterBase;
    Controller->PhysicalBase = RegisterBasePhysical;
    Controller->UsbCoreHandle = INVALID_HANDLE;
    Controller->InterruptHandle = INVALID_HANDLE;
    Controller->PortCount = PortCount;
    Controller->HandoffData = EhciHandoff;
    KeInitializeSpinLock(&(Controller->Lock));
    Controller->DestroyQueuesWorkItem = KeCreateWorkItem(
                                                 NULL,
                                                 WorkPriorityNormal,
                                                 EhcipDestroyQueuesWorkRoutine,
                                                 Controller,
                                                 EHCI_ALLOCATION_TAG);

    if (Controller->DestroyQueuesWorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    //
    // Allocate and initialize the buffer used to hold the EHCI schedule. Since
    // the controller never writes to the periodic schedule memory, just map
    // it cached and manage it carefully (rather than mapping the whole schedule
    // uncached).
    //

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Controller->PeriodicScheduleIoBuffer = MmAllocateNonPagedIoBuffer(
                                                0,
                                                MAX_ULONG,
                                                EHCI_FRAME_LIST_ALIGNMENT,
                                                sizeof(EHCI_PERIODIC_SCHEDULE),
                                                IoBufferFlags);

    if (Controller->PeriodicScheduleIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    ASSERT(Controller->PeriodicScheduleIoBuffer->FragmentCount == 1);
    ASSERT(Controller->PeriodicScheduleIoBuffer->Fragment[0].Size >=
                                               sizeof(EHCI_PERIODIC_SCHEDULE));

    Controller->PeriodicSchedule =
              Controller->PeriodicScheduleIoBuffer->Fragment[0].VirtualAddress;

    //
    // Create the block allocator used to allocate transfers and queues. The
    // block size is that of the larger structure.
    //

    ASSERT((EHCI_BLOCK_ALLOCATOR_ALIGNMENT & (~EHCI_LINK_ADDRESS_MASK)) == 0);

    if (sizeof(EHCI_TRANSFER_DESCRIPTOR) >= sizeof(EHCI_QUEUE_HEAD)) {
        BlockSize = sizeof(EHCI_TRANSFER_DESCRIPTOR);

    } else {
        BlockSize = sizeof(EHCI_QUEUE_HEAD);
    }

    Flags = BLOCK_ALLOCATOR_FLAG_NON_CACHED |
            BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS;

    Controller->BlockAllocator = MmCreateBlockAllocator(
                                          BlockSize,
                                          EHCI_BLOCK_ALLOCATOR_ALIGNMENT,
                                          EHCI_BLOCK_ALLOCATOR_EXPANSION_COUNT,
                                          Flags,
                                          EHCI_BLOCK_ALLOCATION_TAG);

    if (Controller->BlockAllocator == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    //
    // Create the periodic schedule, which is a tree of empty queues. Interrupt
    // transfers can get different polling rates by inserting themselves at
    // different levels of the tree.
    //

    for (TreeLevel = 0;
         TreeLevel < EHCI_PERIODIC_SCHEDULE_TREE_DEPTH;
         TreeLevel += 1) {

        QueueHead = MmAllocateBlock(Controller->BlockAllocator,
                                    &QueueHeadPhysicalAddress);

        if (QueueHead == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeControllerStateEnd;
        }

        //
        // Initialize the transfer queue.
        //

        TransferQueue = &(Controller->InterruptTree[TreeLevel]);
        INITIALIZE_LIST_HEAD(&(TransferQueue->ListEntry));
        TransferQueue->HardwareQueueHead = QueueHead;
        TransferQueue->PhysicalAddress = QueueHeadPhysicalAddress;

        //
        // Initialize the queue head. This is non-cached memory, so don't
        // needlessly zero the structure. Be smart about it.
        //

        QueueHead->HorizontalLink = EHCI_LINK_TERMINATE;
        QueueHead->Destination = 0;
        QueueHead->SplitInformation = EHCI_QUEUE_1_TRANSACTION_PER_MICRO_FRAME;
        QueueHead->CurrentTransferDescriptorLink = 0;
        QueueHead->TransferOverlay.NextTransfer = EHCI_LINK_TERMINATE;
        QueueHead->TransferOverlay.AlternateNextTransfer = EHCI_LINK_TERMINATE;
        QueueHead->TransferOverlay.Token = EHCI_TRANSFER_STATUS_HALTED;
        RemainingSize = sizeof(EHCI_TRANSFER_DESCRIPTOR) -
                        FIELD_OFFSET(EHCI_TRANSFER_DESCRIPTOR, BufferPointer);

        RtlZeroMemory(&(QueueHead->TransferOverlay.BufferPointer),
                      RemainingSize);

        //
        // Unless this is the first (least often polled) queue, set the
        // previous queue to point at this more often polled queue.
        //

        if (TreeLevel != 0) {
            PreviousTransferQueue = &(Controller->InterruptTree[TreeLevel - 1]);

            ASSERT(((ULONG)QueueHeadPhysicalAddress &
                                              (~EHCI_LINK_ADDRESS_MASK)) == 0);

            PreviousTransferQueue->HardwareQueueHead->HorizontalLink =
                         ((ULONG)QueueHeadPhysicalAddress &
                           EHCI_LINK_ADDRESS_MASK) | EHCI_LINK_TYPE_QUEUE_HEAD;

            INSERT_AFTER(&(TransferQueue->ListEntry),
                         &(PreviousTransferQueue->ListEntry));
        }
    }

    //
    // Initialize the array of frame list pointers for the periodic schedule
    // to point to the various levels of the tree with their respective
    // frequencies.
    //

    for (Frame = 0; Frame < EHCI_DEFAULT_FRAME_LIST_ENTRY_COUNT; Frame += 1) {
        FrameBits = Frame;
        TreeLevel = 0;

        //
        // Figure out how many zero bits are clear. For example, one in 8 times,
        // the first three bits are 0. Use that to determine which frequency
        // level to put for this frame.
        //

        while (((FrameBits & 0x1) == 0) &&
               (TreeLevel < EHCI_PERIODIC_SCHEDULE_TREE_DEPTH - 1)) {

            TreeLevel += 1;
            FrameBits = FrameBits >> 1;
        }

        //
        // The most common one is at the end, remember.
        //

        TreeLevel = EHCI_PERIODIC_SCHEDULE_TREE_DEPTH - 1 - TreeLevel;

        ASSERT(TreeLevel < EHCI_PERIODIC_SCHEDULE_TREE_DEPTH);

        TransferQueue = &(Controller->InterruptTree[TreeLevel]);
        Controller->PeriodicSchedule->FrameLink[Frame] =
                                      ((ULONG)TransferQueue->PhysicalAddress &
                                       EHCI_LINK_ADDRESS_MASK) |
                                      EHCI_LINK_TYPE_QUEUE_HEAD;
    }

    //
    // Clean the cache of the periodic schedule.
    //

    MmFlushBufferForDataOut(Controller->PeriodicSchedule,
                            sizeof(EHCI_PERIODIC_SCHEDULE));

    //
    // Create an empty queue head for the asynchronous list.
    //

    QueueHead = MmAllocateBlock(Controller->BlockAllocator,
                                &QueueHeadPhysicalAddress);

    if (QueueHead == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    //
    // Link the asynchronous schedule with this new queue head.
    //

    TransferQueue = &(Controller->AsynchronousSchedule);
    INITIALIZE_LIST_HEAD(&(TransferQueue->ListEntry));
    TransferQueue->HardwareQueueHead = QueueHead;
    TransferQueue->PhysicalAddress = QueueHeadPhysicalAddress;

    //
    // Initialize the queue head. Do not zero the whole thing, as every field
    // will be filled in below.
    //

    QueueHead->SplitInformation = EHCI_QUEUE_1_TRANSACTION_PER_MICRO_FRAME;
    QueueHead->CurrentTransferDescriptorLink = 0;
    QueueHead->TransferOverlay.NextTransfer = EHCI_LINK_TERMINATE;
    QueueHead->TransferOverlay.AlternateNextTransfer = EHCI_LINK_TERMINATE;
    QueueHead->TransferOverlay.Token = EHCI_TRANSFER_STATUS_HALTED;
    RemainingSize = sizeof(EHCI_TRANSFER_DESCRIPTOR) -
                    FIELD_OFFSET(EHCI_TRANSFER_DESCRIPTOR, BufferPointer);

    RtlZeroMemory(&(QueueHead->TransferOverlay.BufferPointer),
                  RemainingSize);

    //
    // Here's where things get interesting. If there's handoff data, then the
    // kernel debugger has set up two queue heads already, one is an empty
    // reclamation queue, and the other is an empty queue that's never used.
    // It inserts its own queue heads in between these two queues. The
    // handoff data contains the actual pointers to the queue heads the kernel
    // debugger uses, so they can be moved.
    //

    if (EhciHandoff != NULL) {

        //
        // Take down the kernel debug connection, as the controller's going to
        // be reset. The USB core driver will reconnect when it re-discovers
        // the debug device.
        //

        RtlDebugPrint("EHCI: Temporarily disconnecting kernel debugger "
                      "while the controller is reinitialized.\n");

        KdDisconnect();

        //
        // Use the newly allocated queue to replace the end queue. This EHCI
        // driver will insert all its queue heads after this new end queue head,
        // as it appears to be the start of the asynchronous schedule. The
        // actual start of the schedule is the reclamation queue head in the
        // kernel debugger.
        //

        QueueHead->HorizontalLink = EhciHandoff->EndQueue->HorizontalLink;
        QueueHead->Destination = EhciHandoff->EndQueue->Destination;
        EhciHandoff->ReclamationQueue->HorizontalLink =
                          QueueHeadPhysicalAddress | EHCI_LINK_TYPE_QUEUE_HEAD;

        //
        // Replace the end queue for the kernel debugger.
        //

        EhciHandoff->EndQueue = QueueHead;
        EhciHandoff->EndQueuePhysical = QueueHeadPhysicalAddress;

    //
    // There is no handoff data, so initialize the queue head to be the
    // reclamation queue head and the beginning of the asynchronous schedule.
    // Loop that queue to point back to itself.
    //

    } else {
        QueueHead->Destination = EHCI_QUEUE_RECLAMATION_HEAD;
        QueueHead->HorizontalLink = ((ULONG)QueueHeadPhysicalAddress &
                                     EHCI_LINK_ADDRESS_MASK) |
                                    EHCI_LINK_TYPE_QUEUE_HEAD;
    }

    Status = STATUS_SUCCESS;

InitializeControllerStateEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            EhcipDestroyControllerState(Controller);
            Controller = NULL;
        }
    }

    return Controller;
}

VOID
EhcipDestroyControllerState (
    PEHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys the memory associated with an EHCI controller.

Arguments:

    Controller - Supplies a pointer to the EHCI controller state to release.

Return Value:

    None.

--*/

{

    ULONG TreeLevel;

    ASSERT(LIST_EMPTY(&(Controller->EndpointListHead)) != FALSE);

    if (Controller->DestroyQueuesWorkItem != NULL) {
        KeDestroyWorkItem(Controller->DestroyQueuesWorkItem);
    }

    if (Controller->PeriodicScheduleIoBuffer != NULL) {
        MmFreeIoBuffer(Controller->PeriodicScheduleIoBuffer);
    }

    for (TreeLevel = 0;
         TreeLevel < EHCI_PERIODIC_SCHEDULE_TREE_DEPTH;
         TreeLevel += 1) {

        if (Controller->InterruptTree[TreeLevel].HardwareQueueHead == NULL) {
            continue;
        }

        MmFreeBlock(Controller->BlockAllocator,
                    Controller->InterruptTree[TreeLevel].HardwareQueueHead);

        Controller->InterruptTree[TreeLevel].HardwareQueueHead = NULL;
    }

    //
    // If there's handoff data, it's not great that the controller is going
    // down. Disconnect the debugger for safety.
    //

    if (Controller->HandoffData != NULL) {
        RtlDebugPrint("EHCI: Disconnecting kernel debugger as EHCI "
                      "controller is being removed.\n");

        KdDisconnect();
    }

    if (Controller->AsynchronousSchedule.HardwareQueueHead != NULL) {
        MmFreeBlock(Controller->BlockAllocator,
                    Controller->AsynchronousSchedule.HardwareQueueHead);

        Controller->AsynchronousSchedule.HardwareQueueHead = NULL;
    }

    if (Controller->BlockAllocator != NULL) {
        MmDestroyBlockAllocator(Controller->BlockAllocator);
        Controller->BlockAllocator = NULL;
    }

    ASSERT(LIST_EMPTY(&(Controller->IsochronousTransferListHead)) != FALSE);

    if (Controller->UsbCoreHandle != INVALID_HANDLE) {
        UsbHostDestroyControllerState(Controller->UsbCoreHandle);
    }

    MmFreeNonPagedPool(Controller);
    return;
}

KSTATUS
EhcipRegisterController (
    PEHCI_CONTROLLER Controller,
    PDEVICE Device
    )

/*++

Routine Description:

    This routine registers the started EHCI controller with the core USB
    library.

Arguments:

    Controller - Supplies a pointer to the EHCI controller state of the
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
    // the EHCI controller.
    //

    RtlZeroMemory(&Interface, sizeof(USB_HOST_CONTROLLER_INTERFACE));
    Interface.Version = USB_HOST_CONTROLLER_INTERFACE_VERSION;
    Interface.DriverObject = EhciDriver;
    Interface.DeviceObject = Device;
    Interface.HostControllerContext = Controller;
    Interface.Speed = UsbDeviceSpeedHigh;
    Interface.Identifier = Controller->PhysicalBase;
    Interface.DebugPortSubType = DEBUG_PORT_USB_EHCI;
    Interface.RootHubPortCount = Controller->PortCount;
    Interface.CreateEndpoint = EhcipCreateEndpoint;
    Interface.ResetEndpoint = EhcipResetEndpoint;
    Interface.FlushEndpoint = EhcipFlushEndpoint;
    Interface.DestroyEndpoint = EhcipDestroyEndpoint;
    Interface.CreateTransfer = EhcipCreateTransfer;
    Interface.DestroyTransfer = EhcipDestroyTransfer;
    Interface.SubmitTransfer = EhcipSubmitTransfer;
    Interface.SubmitPolledTransfer = EhcipSubmitPolledTransfer;
    Interface.CancelTransfer = EhcipCancelTransfer;
    Interface.GetRootHubStatus = EhcipGetRootHubStatus;
    Interface.SetRootHubStatus = EhcipSetRootHubStatus;
    Status = UsbHostRegisterController(&Interface,
                                       &(Controller->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto RegisterControllerEnd;
    }

RegisterControllerEnd:
    return Status;
}

VOID
EhcipSetInterruptHandle (
    PEHCI_CONTROLLER Controller,
    HANDLE InterruptHandle
    )

/*++

Routine Description:

    This routine saves the handle of the connected interrupt in the EHCI
    controller.

Arguments:

    Controller - Supplies a pointer to the EHCI controller state.

    InterruptHandle - Supplies the connected interrupt handle.

Return Value:

    None.

--*/

{

    Controller->InterruptHandle = InterruptHandle;
    return;
}

KSTATUS
EhcipResetController (
    PEHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine resets and starts the EHCI controller.

Arguments:

    Controller - Supplies a pointer to the EHCI controller state of the
        controller to reset.

Return Value:

    Status code.

--*/

{

    ULONG CommandRegister;
    ULONG InterruptRegister;
    PIO_BUFFER PeriodicIoBuffer;
    ULONG PhysicalAddress;
    ULONG PortIndex;
    ULONG PortStatusRegister;

    //
    // Reset the host controller and wait for the hardware to clear the bit,
    // which indicates that the reset is complete.
    //

    CommandRegister = EHCI_COMMAND_CONTROLLER_RESET;
    EHCI_WRITE_REGISTER(Controller, EhciRegisterUsbCommand, CommandRegister);
    do {

        //
        // AND in the hardware register to see if the bit has cleared.
        //

        CommandRegister &= EHCI_READ_REGISTER(Controller,
                                              EhciRegisterUsbCommand);

    } while (CommandRegister != 0);

    //
    // Clear the status register.
    //

    EHCI_WRITE_REGISTER(Controller, EhciRegisterUsbStatus, 0);

    //
    // Write the the segment selector to use the first 4GB of physical memory.
    //

    EHCI_WRITE_REGISTER(Controller, EhciRegisterSegmentSelector, 0);

    //
    // Enable all interrupts except the frame list rollover.
    //

    InterruptRegister = EHCI_INTERRUPT_ASYNC_ADVANCE |
                        EHCI_INTERRUPT_HOST_SYSTEM_ERROR |
                        EHCI_INTERRUPT_PORT_CHANGE |
                        EHCI_INTERRUPT_USB_ERROR |
                        EHCI_INTERRUPT_ENABLE;

    EHCI_WRITE_REGISTER(Controller,
                        EhciRegisterUsbInterruptEnable,
                        InterruptRegister);

    //
    // Set the periodic list base register to the physical address of the EHCI
    // periodic schedule.
    //

    PeriodicIoBuffer = Controller->PeriodicScheduleIoBuffer;
    PhysicalAddress = (ULONG)PeriodicIoBuffer->Fragment[0].PhysicalAddress;

    ASSERT(PhysicalAddress == PeriodicIoBuffer->Fragment[0].PhysicalAddress);

    EHCI_WRITE_REGISTER(Controller,
                        EhciRegisterPeriodicListBase,
                        PhysicalAddress);

    //
    // Write the asynchronous list base to the reclamation list head.
    //

    PhysicalAddress = (ULONG)Controller->AsynchronousSchedule.PhysicalAddress;

    ASSERT(PhysicalAddress == Controller->AsynchronousSchedule.PhysicalAddress);

    EHCI_WRITE_REGISTER(Controller,
                        EhciRegisterAsynchronousListAddress,
                        PhysicalAddress);

    //
    // Write to the command register to start the controller.
    //

    CommandRegister = EHCI_COMMAND_INTERRUPT_EVERY_8_UFRAMES |
                      ECHI_COMMAND_ASYNC_PARK_ENABLE |
                      (3 << EHCI_COMMAND_PARK_COUNT_SHIFT) |
                      EHCI_COMMAND_ENABLE_ASYNC_SCHEDULE |
                      EHCI_COMMAND_ENABLE_PERIODIC_SCHEDULE |
                      EHCI_COMMAND_1024_FRAME_LIST_ENTRIES |
                      EHCI_COMMAND_RUN;

    EHCI_WRITE_REGISTER(Controller, EhciRegisterUsbCommand, CommandRegister);
    Controller->CommandRegister = CommandRegister;

    //
    // Set the config flag, which switches all the ports to EHCI away from the
    // companion controllers.
    //

    EHCI_WRITE_REGISTER(Controller, EhciRegisterConfigured, 1);

    //
    // Fire up the ports.
    //

    for (PortIndex = 0; PortIndex < Controller->PortCount; PortIndex += 1) {
        PortStatusRegister = EHCI_READ_PORT_REGISTER(Controller, PortIndex);
        if ((PortStatusRegister & EHCI_PORT_POWER) == 0) {
            PortStatusRegister |= EHCI_PORT_POWER;
            EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, PortStatusRegister);
        }
    }

    return STATUS_SUCCESS;
}

INTERRUPT_STATUS
EhcipInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the EHCI interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the EHCI
        controller.

Return Value:

    Interrupt status.

--*/

{

    PEHCI_CONTROLLER Controller;
    INTERRUPT_STATUS InterruptStatus;
    USHORT UsbStatus;

    Controller = (PEHCI_CONTROLLER)Context;
    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Read the status register. If it's non-zero, this is USB's interrupt.
    //

    UsbStatus = EHCI_READ_REGISTER(Controller, EhciRegisterUsbStatus) &
                EHCI_STATUS_INTERRUPT_MASK;

    if (UsbStatus != 0) {
        InterruptStatus = InterruptStatusClaimed;
        RtlAtomicOr32(&(Controller->PendingStatusBits), UsbStatus);

        //
        // Clear the bits in the status register to acknowledge the interrupt.
        //

        EHCI_WRITE_REGISTER(Controller, EhciRegisterUsbStatus, UsbStatus);
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
EhcipInterruptServiceDpc (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the EHCI dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the EHCI controller
        structure.

Return Value:

    None.

--*/

{

    PEHCI_CONTROLLER Controller;
    ULONG StatusBits;

    Controller = Parameter;
    StatusBits = RtlAtomicExchange32(&(Controller->PendingStatusBits), 0);
    if (StatusBits == 0) {
        return InterruptStatusNotClaimed;
    }

    EhcipProcessInterrupt(Controller, StatusBits);
    return InterruptStatusClaimed;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
EhcipCreateEndpoint (
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

    ULONG ClosestRate;
    PEHCI_CONTROLLER Controller;
    ULONG Destination;
    PEHCI_TRANSFER DummyTransfer;
    ULONG EndMask;
    PEHCI_TRANSFER_DESCRIPTOR HardwareTransfer;
    PHYSICAL_ADDRESS HardwareTransferPhysicalAddress;
    ULONG InterruptTreeLevel;
    ULONG NakReloadCount;
    PEHCI_ENDPOINT NewEndpoint;
    PEHCI_TRANSFER_QUEUE NewQueue;
    RUNLEVEL OldRunLevel;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG PollRate;
    PEHCI_TRANSFER_QUEUE QueueBefore;
    PEHCI_QUEUE_HEAD QueueHead;
    PHYSICAL_ADDRESS QueueHeadPhysicalAddress;
    ULONG RemainingSize;
    ULONG SplitInformation;
    ULONG StartMicroFrame;
    KSTATUS Status;

    Controller = (PEHCI_CONTROLLER)HostControllerContext;
    NewEndpoint = MmAllocateNonPagedPool(sizeof(EHCI_ENDPOINT),
                                         EHCI_ALLOCATION_TAG);

    if (NewEndpoint == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEndpointEnd;
    }

    RtlZeroMemory(NewEndpoint, sizeof(EHCI_ENDPOINT));
    INITIALIZE_LIST_HEAD(&(NewEndpoint->TransferListHead));
    NewEndpoint->TransferType = Endpoint->Type;

    ASSERT((Endpoint->Speed == UsbDeviceSpeedLow) ||
           (Endpoint->Speed == UsbDeviceSpeedFull) ||
           (Endpoint->Speed == UsbDeviceSpeedHigh));

    NewEndpoint->Speed = Endpoint->Speed;

    ASSERT(Endpoint->MaxPacketSize != 0);

    NewEndpoint->MaxPacketSize = Endpoint->MaxPacketSize;
    NewEndpoint->EndpointNumber = Endpoint->EndpointNumber;
    NewEndpoint->PollRate = Endpoint->PollRate;

    //
    // If the endpoint is high speed, the units are in microframes. But EHCI
    // periodic schedules run in frames, so convert down (rounding up).
    //

    if (NewEndpoint->Speed == UsbDeviceSpeedHigh) {
        NewEndpoint->PollRate =
            ALIGN_RANGE_UP(NewEndpoint->PollRate, EHCI_MICROFRAMES_PER_FRAME) >>
            EHCI_MICROFRAMES_PER_FRAME_SHIFT;
    }

    //
    // For isochronous endpoints, that's all that is needed.
    //

    if (NewEndpoint->TransferType == UsbTransferTypeIsochronous) {
        Status = STATUS_SUCCESS;
        goto CreateEndpointEnd;
    }

    //
    // Create the hardware queue head.
    //

    QueueHead = MmAllocateBlock(Controller->BlockAllocator,
                                &QueueHeadPhysicalAddress);

    if (QueueHead == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEndpointEnd;
    }

    RtlZeroMemory(QueueHead, sizeof(EHCI_QUEUE_HEAD));
    NewQueue = &(NewEndpoint->Queue);
    NewQueue->HardwareQueueHead = QueueHead;
    NewQueue->PhysicalAddress = QueueHeadPhysicalAddress;

    //
    // Set the NAK reload count to the maximum for control and bulk transfers.
    // Interrupt and isochronous transfers must have the NAK reload count set
    // to zero.
    //

    if ((NewEndpoint->TransferType == UsbTransferTypeControl) ||
        (NewEndpoint->TransferType == UsbTransferTypeBulk)) {

        NakReloadCount = EHCI_QUEUE_DEFAULT_NAK_RELOAD_COUNT;

    } else {
        NakReloadCount = 0;
    }

    //
    // Initialize the hardware queue entry. Notice one thing conspicuously
    // missing is the device address. This gets initialized to zero, and fixed
    // up during transfer submissions (when the device is potentially moved off
    // address zero).
    //

    Destination =
          (NakReloadCount << EHCI_QUEUE_NAK_RELOAD_COUNT_SHIFT) |
          ((NewEndpoint->MaxPacketSize << EHCI_QUEUE_MAX_PACKET_LENGTH_SHIFT) &
           EHCI_QUEUE_MAX_PACKET_LENGTH_MASK) |
          ((NewEndpoint->EndpointNumber & USB_ENDPOINT_ADDRESS_MASK) <<
           EHCI_QUEUE_ENDPOINT_SHIFT);

    switch (NewEndpoint->Speed) {
    case UsbDeviceSpeedLow:
        Destination |= EHCI_QUEUE_LOW_SPEED;
        break;

    case UsbDeviceSpeedFull:
        Destination |= EHCI_QUEUE_FULL_SPEED;
        break;

    case UsbDeviceSpeedHigh:
        Destination |= EHCI_QUEUE_HIGH_SPEED;
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto CreateEndpointEnd;
    }

    //
    // All control transfers handle the data toggle without hardware
    // assistance. Non-high speed control transfers must have the control
    // endpoint flag set. High speed control transfers should not have said
    // flag set.
    //

    if (NewEndpoint->TransferType == UsbTransferTypeControl) {
        Destination |= EHCI_QUEUE_USE_TRANSFER_DESCRIPTOR_DATA_TOGGLE;
        if (NewEndpoint->Speed != UsbDeviceSpeedHigh) {
            Destination |= EHCI_QUEUE_CONTROL_ENDPOINT;
        }
    }

    QueueHead->Destination = Destination;

    //
    // Set the split information in the hardware queue entry.
    //

    SplitInformation = EHCI_QUEUE_1_TRANSACTION_PER_MICRO_FRAME;
    if ((NewEndpoint->Speed == UsbDeviceSpeedLow) ||
        (NewEndpoint->Speed == UsbDeviceSpeedFull)) {

        ASSERT(Endpoint->HubAddress != 0);
        ASSERT(Endpoint->HubPortNumber != 0);

        SplitInformation |=
                   ((Endpoint->HubPortNumber << EHCI_QUEUE_PORT_NUMBER_SHIFT) &
                    EHCI_QUEUE_PORT_NUMBER_MASK) |
                   ((Endpoint->HubAddress << EHCI_QUEUE_HUB_ADDRESS_SHIFT) &
                    EHCI_QUEUE_HUB_ADDRESS_MASK);

        if (NewEndpoint->TransferType == UsbTransferTypeInterrupt) {

            //
            // Make a weak attempt at spreading out these transfers throughout
            // micro frames. Only start in 0-3, inclusive, to avoid dealing
            // with Frame Split Transaction Nodes.
            //
            // N.B. Interrupt transfer cancellation will need to change if the
            //      above behavior is changed.
            //

            StartMicroFrame = Controller->EndpointCount & 0x3;

            //
            // Isochronous OUT endpoints don't use complete splits, but
            // interrupt and other endpoints usually skip a microframe and then
            // issue complete splits for the next three.
            //

            if ((Endpoint->Type == UsbTransferTypeIsochronous) &&
                (Endpoint->Direction == UsbTransferDirectionOut)) {

                EndMask = 0;

            } else {
                EndMask = (1 << (StartMicroFrame + 2)) |
                          (1 << (StartMicroFrame + 3)) |
                          (1 << (StartMicroFrame + 4));
            }

            SplitInformation |=
                              ((EndMask << EHCI_QUEUE_SPLIT_COMPLETION_SHIFT) &
                               EHCI_QUEUE_SPLIT_COMPLETION_MASK) |
                              ((1 << StartMicroFrame) &
                               EHCI_QUEUE_SPLIT_START_MASK);
        }

    } else {

        //
        // Make a weak attempt at spreading the transfers throughout micro-
        // frames.
        //

        if (NewEndpoint->TransferType == UsbTransferTypeInterrupt) {
            SplitInformation |= ((1 << (Controller->EndpointCount & 0x7)) &
                                EHCI_QUEUE_SPLIT_START_MASK);
        }
    }

    QueueHead->SplitInformation = SplitInformation;

    //
    // Allocate an initial dummy transfer to point this queue at.
    //

    DummyTransfer = MmAllocateNonPagedPool(sizeof(EHCI_TRANSFER),
                                           EHCI_ALLOCATION_TAG);

    if (DummyTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEndpointEnd;
    }

    HardwareTransfer = MmAllocateBlock(Controller->BlockAllocator,
                                       &HardwareTransferPhysicalAddress);

    if (HardwareTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateEndpointEnd;
    }

    RtlZeroMemory(DummyTransfer, sizeof(EHCI_TRANSFER));
    NewQueue->DummyTransfer = DummyTransfer;
    DummyTransfer->HardwareTransfer = HardwareTransfer;
    DummyTransfer->PhysicalAddress = HardwareTransferPhysicalAddress;
    HardwareTransfer->NextTransfer = EHCI_LINK_TERMINATE;
    HardwareTransfer->AlternateNextTransfer = EHCI_LINK_TERMINATE;
    HardwareTransfer->Token = EHCI_TRANSFER_STATUS_HALTED;
    RemainingSize = sizeof(EHCI_TRANSFER_DESCRIPTOR) -
                    FIELD_OFFSET(EHCI_TRANSFER_DESCRIPTOR, BufferPointer);

    RtlZeroMemory(&(HardwareTransfer->BufferPointer),
                  RemainingSize);

    //
    // Point the queue at the dummy transfer.
    //

    QueueHead->TransferOverlay.NextTransfer =
                                        (ULONG)HardwareTransferPhysicalAddress;

    QueueHead->TransferOverlay.AlternateNextTransfer =
                                        (ULONG)HardwareTransferPhysicalAddress;

    //
    // Figure out where to insert this queue. If it's an interrupt transfer,
    // determine what level of the tree it belongs in based on the polling rate.
    //

    if (NewEndpoint->TransferType == UsbTransferTypeInterrupt) {
        PollRate = NewEndpoint->PollRate;
        if (PollRate > MAX_USHORT / 2) {
            PollRate = MAX_USHORT / 2;
        }

        ClosestRate = 1;
        while (ClosestRate < PollRate) {
            ClosestRate <<= 1;
        }

        if (ClosestRate != PollRate) {
            ClosestRate >>= 1;
        }

        ASSERT(ClosestRate != 0);

        InterruptTreeLevel = EHCI_PERIODIC_SCHEDULE_TREE_DEPTH - 1;
        while (((ClosestRate & 0x1) == 0) && (InterruptTreeLevel != 0)) {
            ClosestRate >>= 1;
            InterruptTreeLevel -= 1;
        }

        ASSERT(InterruptTreeLevel < EHCI_PERIODIC_SCHEDULE_TREE_DEPTH);

        QueueBefore = &(Controller->InterruptTree[InterruptTreeLevel]);
        NewEndpoint->PollRate = ClosestRate;

    } else {
        QueueBefore = &(Controller->AsynchronousSchedule);
    }

    //
    // Insert the endpoint onto the global queue, both the software list and
    // the hardware's singly linked list. Use register writes for memory that
    // is potentially being actively observed by hardware.
    //

    OldRunLevel = EhcipAcquireControllerLock(Controller);
    Controller->EndpointCount += 1;
    INSERT_BEFORE(&(NewEndpoint->ListEntry), &(Controller->EndpointListHead));
    INSERT_AFTER(&(NewQueue->ListEntry), &(QueueBefore->ListEntry));
    QueueHead->HorizontalLink = QueueBefore->HardwareQueueHead->HorizontalLink;

    ASSERT(((ULONG)NewQueue->PhysicalAddress & (~EHCI_LINK_ADDRESS_MASK)) == 0);

    PhysicalAddress = NewQueue->PhysicalAddress | EHCI_LINK_TYPE_QUEUE_HEAD;
    HlWriteRegister32(&(QueueBefore->HardwareQueueHead->HorizontalLink),
                      (ULONG)PhysicalAddress);

    EhcipReleaseControllerLock(Controller, OldRunLevel);
    Status = STATUS_SUCCESS;

CreateEndpointEnd:
    if (!KSUCCESS(Status)) {
        if (NewEndpoint != NULL) {
            DummyTransfer = NewEndpoint->Queue.DummyTransfer;
            if (DummyTransfer != NULL) {
                if (DummyTransfer->HardwareTransfer != NULL) {
                    MmFreeBlock(Controller->BlockAllocator,
                                DummyTransfer->HardwareTransfer);
                }

                MmFreeNonPagedPool(DummyTransfer);
            }

            MmFreeNonPagedPool(NewEndpoint);
            NewEndpoint = NULL;
        }
    }

    *EndpointContext = NewEndpoint;
    return Status;
}

VOID
EhcipResetEndpoint (
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

    PEHCI_ENDPOINT Endpoint;
    PEHCI_QUEUE_HEAD HardwareQueueHead;
    ULONG Token;

    Endpoint = (PEHCI_ENDPOINT)EndpointContext;

    //
    // There better not be any active transfers running around during an
    // endpoint reset.
    //

    ASSERT(LIST_EMPTY(&(Endpoint->TransferListHead)) != FALSE);

    //
    // If the max packet size changed, update the queue head.
    //

    HardwareQueueHead = Endpoint->Queue.HardwareQueueHead;
    if (MaxPacketSize != Endpoint->MaxPacketSize) {
        Endpoint->MaxPacketSize = MaxPacketSize;
        HardwareQueueHead->Destination =
                       (HardwareQueueHead->Destination &
                        (~EHCI_QUEUE_MAX_PACKET_LENGTH_MASK)) |
                       ((MaxPacketSize << EHCI_QUEUE_MAX_PACKET_LENGTH_SHIFT) &
                        EHCI_QUEUE_MAX_PACKET_LENGTH_MASK);
    }

    //
    // Reset the data toggle in the transfer overlay.
    //

    Token = HlReadRegister32(&(HardwareQueueHead->TransferOverlay.Token));
    Token &= ~EHCI_TRANSFER_DATA_TOGGLE;
    HlWriteRegister32(&(HardwareQueueHead->TransferOverlay.Token), Token);
    return;
}

KSTATUS
EhcipFlushEndpoint (
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

    PEHCI_CONTROLLER Controller;
    ULONG Count;
    PLIST_ENTRY CurrentEntry;
    PEHCI_ENDPOINT Endpoint;
    PEHCI_TRANSFER NextTransfer;
    BOOL RemoveSet;
    KSTATUS Status;
    ULONGLONG Timeout;
    PEHCI_TRANSFER Transfer;
    PEHCI_TRANSFER_SET TransferSet;

    //
    // This routine removes transfers without acquiring the controller lock. It
    // is expected that the caller is using under special circumstances at high
    // run level (e.g. to prepare for crash dump writes during system failure).
    //

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Controller = (PEHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PEHCI_ENDPOINT)EndpointContext;
    if (Endpoint->TransferType == UsbTransferTypeIsochronous) {

        //
        // TODO: Implement support for isochronous transfers.
        //

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    //
    // Let every transfer set in the endpoint complete. If the caller is about
    // to use this endpoint for an operation during a system failure, then the
    // endpoint better be alive enough to finish the rest of its current
    // transfers.
    //

    Timeout = HlQueryTimeCounter() +
              (HlQueryTimeCounterFrequency() * EHCI_ENDPOINT_FLUSH_TIMEOUT);

    Count = 0;
    while (LIST_EMPTY(&(Endpoint->TransferListHead)) == FALSE) {
        if (HlQueryTimeCounter() > Timeout) {
            Status = STATUS_TIMEOUT;
            goto FlushEndpointEnd;
        }

        CurrentEntry = Endpoint->TransferListHead.Next;
        while (CurrentEntry != &(Endpoint->TransferListHead)) {

            ASSERT((CurrentEntry != NULL) && (CurrentEntry->Next != NULL));

            Transfer = LIST_VALUE(CurrentEntry,
                                  EHCI_TRANSFER,
                                  EndpointListEntry);

            CurrentEntry = CurrentEntry->Next;
            RemoveSet = EhcipProcessPotentiallyCompletedTransfer(Transfer);
            if (RemoveSet != FALSE) {

                //
                // Get the current entry off of this set, as several transfers
                // may be removed here.
                //

                TransferSet = Transfer->Set;
                if (CurrentEntry != &(Endpoint->TransferListHead)) {
                    NextTransfer = LIST_VALUE(CurrentEntry,
                                              EHCI_TRANSFER,
                                              EndpointListEntry);

                    while (NextTransfer->Set == TransferSet) {
                        CurrentEntry = CurrentEntry->Next;
                        if (CurrentEntry == &(Endpoint->TransferListHead)) {
                            break;
                        }

                        NextTransfer = LIST_VALUE(CurrentEntry,
                                                  EHCI_TRANSFER,
                                                  EndpointListEntry);
                    }
                }

                //
                // Remove the transfer set from the owning endpoint's queue,
                // but don't bother to call the completion routine. It's really
                // just lights out for this transfer.
                //

                EhcipRemoveCompletedTransferSet(Controller, TransferSet);
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
EhcipDestroyEndpoint (
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

    ULONG CommandRegister;
    PEHCI_CONTROLLER Controller;
    PEHCI_ENDPOINT Endpoint;
    BOOL LockHeld;
    RUNLEVEL OldRunLevel;
    PEHCI_TRANSFER_QUEUE Queue;
    PEHCI_TRANSFER_QUEUE QueueBefore;
    BOOL ReleaseEndpoint;

    Controller = (PEHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PEHCI_ENDPOINT)EndpointContext;
    LockHeld = FALSE;
    ReleaseEndpoint = TRUE;

    ASSERT(LIST_EMPTY(&(Endpoint->TransferListHead)) != FALSE);

    //
    // Remove the endpoint's queue from the hardware schedule.
    //

    if (Endpoint->Queue.HardwareQueueHead != NULL) {
        OldRunLevel = EhcipAcquireControllerLock(Controller);
        LockHeld = TRUE;
        if (Endpoint->Queue.HardwareQueueHead == NULL) {
            goto DestroyEndpointEnd;
        }

        Queue = &(Endpoint->Queue);
        Controller->EndpointCount -= 1;
        LIST_REMOVE(&(Endpoint->ListEntry));

        //
        // Isochronous transfers are handled differently.
        //

        if (Endpoint->TransferType == UsbTransferTypeIsochronous) {

            ASSERT(FALSE);

            goto DestroyEndpointEnd;

        //
        // Remove the interrupt endpoint's queue from the synchronous schedule.
        //

        } else if (Endpoint->TransferType == UsbTransferTypeInterrupt) {

            ASSERT(Queue->ListEntry.Next != NULL);

            QueueBefore = LIST_VALUE(Queue->ListEntry.Previous,
                                     EHCI_TRANSFER_QUEUE,
                                     ListEntry);

            HlWriteRegister32(&(QueueBefore->HardwareQueueHead->HorizontalLink),
                              Queue->HardwareQueueHead->HorizontalLink);

            LIST_REMOVE(&(Queue->ListEntry));
            Queue->ListEntry.Next = NULL;

            //
            // Now release the lock and wait a full frame to make sure that the
            // periodic schedule has moved beyond this queue head. This simple
            // wait accounts for split transactions, but will need to be
            // updated if Frame Split Transaction Nodes are supported.
            //

            EhcipReleaseControllerLock(Controller, OldRunLevel);
            LockHeld = FALSE;
            KeDelayExecution(FALSE, FALSE, 1 * MICROSECONDS_PER_MILLISECOND);

            //
            // The queue can be safely destroyed.
            //

            if (Queue->DummyTransfer != NULL) {
                if (Queue->DummyTransfer->HardwareTransfer != NULL) {
                    MmFreeBlock(Controller->BlockAllocator,
                                Queue->DummyTransfer->HardwareTransfer);
                }

                MmFreeNonPagedPool(Queue->DummyTransfer);
            }

            MmFreeBlock(Controller->BlockAllocator, Queue->HardwareQueueHead);
            Queue->HardwareQueueHead = NULL;

        //
        // Remove bulk and control endpoint's queue head from the asynchronous
        // schedule. The transfer set will be fully removed from the queue head
        // once the interrupt for async-on-advance has fired.
        //

        } else {

            ASSERT(Queue->AsyncOnAdvanceCancel == FALSE);
            ASSERT((Endpoint->TransferType == UsbTransferTypeControl) ||
                   (Endpoint->TransferType == UsbTransferTypeBulk));

            QueueBefore = LIST_VALUE(Queue->ListEntry.Previous,
                                     EHCI_TRANSFER_QUEUE,
                                     ListEntry);

            HlWriteRegister32(&(QueueBefore->HardwareQueueHead->HorizontalLink),
                              Queue->HardwareQueueHead->HorizontalLink);

            LIST_REMOVE(&(Queue->ListEntry));

            //
            // If the asynchronous on advance ready list is empty, then add
            // this queue head to the ready list and ring the doorbell.
            //

            if (LIST_EMPTY(&(Controller->AsyncOnAdvanceReadyListHead))) {
                INSERT_BEFORE(&(Queue->ListEntry),
                              &(Controller->AsyncOnAdvanceReadyListHead));

                CommandRegister = Controller->CommandRegister;
                CommandRegister |= EHCI_COMMAND_INTERRUPT_ON_ASYNC_ADVANCE;
                EHCI_WRITE_REGISTER(Controller,
                                    EhciRegisterUsbCommand,
                                    CommandRegister);

            //
            // Otherwise the doorbell has already been rung. This queue head
            // will have to wait for the next chance to ring it. Put it on the
            // pending list.
            //

            } else {
                INSERT_BEFORE(&(Queue->ListEntry),
                              &(Controller->AsyncOnAdvancePendingListHead));
            }

            //
            // Do not release the endpoint. It will get released along with the
            // queue when the async-on-advance interrupt is handled.
            //

            ReleaseEndpoint = FALSE;
        }
    }

DestroyEndpointEnd:
    if (LockHeld != FALSE) {
        EhcipReleaseControllerLock(Controller, OldRunLevel);
    }

    if (ReleaseEndpoint != FALSE) {
        MmFreeNonPagedPool(Endpoint);
    }

    return;
}

KSTATUS
EhcipCreateTransfer (
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

    ULONG AllocationSize;
    PEHCI_CONTROLLER Controller;
    PEHCI_ENDPOINT Endpoint;
    BOOL ForceShortTransfer;
    PEHCI_TRANSFER_DESCRIPTOR HardwareTransfer;
    PHYSICAL_ADDRESS HardwareTransferPhysicalAddress;
    KSTATUS Status;
    PEHCI_TRANSFER Transfer;
    PEHCI_TRANSFER *TransferArray;
    ULONG TransferCount;
    ULONG TransferIndex;
    PEHCI_TRANSFER_SET TransferSet;

    ASSERT(TransferContext != NULL);

    Controller = (PEHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PEHCI_ENDPOINT)EndpointContext;
    ForceShortTransfer = FALSE;
    if ((Flags & USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) {
        ForceShortTransfer = TRUE;
    }

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
    // Create enough data transfers knowing that all submitted transfers will
    // have virtually contiguous data. An extra page must be added to the max
    // transfer size for the transfer calculation because a non page-aligned
    // buffer could cause an EHCI max packet size aligned buffer to be split
    // across two hardware transfers.
    //

    if (MaxBufferSize != 0) {
        MaxBufferSize += (EHCI_PAGE_SIZE - 1);
        TransferCount += MaxBufferSize / EHCI_TRANSFER_MAX_PACKET_SIZE;
        if ((MaxBufferSize % EHCI_TRANSFER_MAX_PACKET_SIZE) != 0) {
            TransferCount += 1;
        }

        //
        // If a short transfer needs to be forced and the last packet might not
        // be a short packet, then add another transfer to account for the
        // forced zero length packet.
        //

        if ((ForceShortTransfer != FALSE) &&
            (MaxBufferSize >= Endpoint->MaxPacketSize)) {

            TransferCount += 1;
        }

    //
    // Account for a USB transfer that will only send zero length packets and
    // for control transfers that need to force a zero length packet in the
    // data phase.
    //

    } else if ((ForceShortTransfer != FALSE) ||
               (Endpoint->TransferType != UsbTransferTypeControl)) {

        TransferCount += 1;
    }

    //
    // Allocate the transfer set structure. Include space for all but the first
    // EHCI_TRANSFER. The first transfer is swapped with the queue's dummy
    // transfer and must be done with its own allocation.
    //

    AllocationSize = sizeof(EHCI_TRANSFER_SET);
    if (TransferCount > 1) {
        AllocationSize += sizeof(PEHCI_TRANSFER) * (TransferCount - 1);
        AllocationSize += sizeof(EHCI_TRANSFER) * (TransferCount - 1);
    }

    TransferSet = MmAllocateNonPagedPool(AllocationSize, EHCI_ALLOCATION_TAG);
    if (TransferSet == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTransferEnd;
    }

    RtlZeroMemory(TransferSet, AllocationSize);
    TransferSet->TransferCount = TransferCount;
    TransferSet->Endpoint = Endpoint;
    TransferArray = (PEHCI_TRANSFER *)&(TransferSet->Transfer);

    //
    // Allocate the first transfer.
    //

    ASSERT(TransferCount >= 1);

    Transfer = MmAllocateNonPagedPool(sizeof(EHCI_TRANSFER),
                                      EHCI_ALLOCATION_TAG);

    if (Transfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateTransferEnd;
    }

    RtlZeroMemory(Transfer, sizeof(EHCI_TRANSFER));
    Transfer->Set = TransferSet;
    TransferArray[0] = Transfer;

    //
    // Create the new transfer's hardware descriptors while initializing the
    // transfers that are included within the transfer set allocation.
    //

    Transfer = (PEHCI_TRANSFER)((PVOID)(TransferSet + 1) +
                                (sizeof(PEHCI_TRANSFER) * (TransferCount - 1)));

    for (TransferIndex = 0; TransferIndex < TransferCount; TransferIndex += 1) {
        HardwareTransfer = MmAllocateBlock(Controller->BlockAllocator,
                                           &HardwareTransferPhysicalAddress);

        if (HardwareTransfer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateTransferEnd;
        }

        if (TransferIndex != 0) {
            TransferArray[TransferIndex] = Transfer;
            Transfer->Set = TransferSet;
            Transfer += 1;
        }

        TransferArray[TransferIndex]->HardwareTransfer = HardwareTransfer;
        TransferArray[TransferIndex]->PhysicalAddress =
                                               HardwareTransferPhysicalAddress;

        ASSERT((HardwareTransferPhysicalAddress & EHCI_LINK_ADDRESS_MASK) ==
               HardwareTransferPhysicalAddress);
    }

    Status = STATUS_SUCCESS;

CreateTransferEnd:
    if (!KSUCCESS(Status)) {
        if (TransferSet != NULL) {
            TransferArray = (PEHCI_TRANSFER *)&(TransferSet->Transfer);
            for (TransferIndex = 0;
                 TransferIndex < TransferSet->TransferCount;
                 TransferIndex += 1) {

                Transfer = TransferArray[TransferIndex];
                if (Transfer != NULL) {
                    if (Transfer->HardwareTransfer != NULL) {
                        MmFreeBlock(Controller->BlockAllocator,
                                    Transfer->HardwareTransfer);
                    }

                    if (TransferIndex == 0) {
                        MmFreeNonPagedPool(Transfer);
                    }
                }
            }

            MmFreeNonPagedPool(TransferSet);
            TransferSet = NULL;
        }
    }

    *TransferContext = TransferSet;
    return Status;
}

VOID
EhcipDestroyTransfer (
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

    PEHCI_CONTROLLER Controller;
    PEHCI_TRANSFER Transfer;
    PEHCI_TRANSFER *TransferArray;
    ULONG TransferIndex;
    PEHCI_TRANSFER_SET TransferSet;

    Controller = (PEHCI_CONTROLLER)HostControllerContext;
    TransferSet = (PEHCI_TRANSFER_SET)TransferContext;

    //
    // Free all transfers that were allocated.
    //

    TransferArray = (PEHCI_TRANSFER *)&(TransferSet->Transfer);
    for (TransferIndex = 0;
         TransferIndex < TransferSet->TransferCount;
         TransferIndex += 1) {

        Transfer = TransferArray[TransferIndex];

        ASSERT(Transfer != NULL);
        ASSERT(Transfer->HardwareTransfer != NULL);
        ASSERT(Transfer->EndpointListEntry.Next == NULL);

        MmFreeBlock(Controller->BlockAllocator, Transfer->HardwareTransfer);
        if (TransferIndex == 0) {
            MmFreeNonPagedPool(Transfer);
        }

        TransferArray[TransferIndex] = NULL;
    }

    MmFreeNonPagedPool(TransferSet);
    return;
}

KSTATUS
EhcipSubmitTransfer (
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

    PEHCI_CONTROLLER Controller;
    PEHCI_ENDPOINT Endpoint;
    UCHAR QueueDeviceAddress;
    KSTATUS Status;
    PEHCI_TRANSFER_SET TransferSet;

    Controller = (PEHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PEHCI_ENDPOINT)EndpointContext;
    TransferSet = (PEHCI_TRANSFER_SET)TransferContext;
    TransferSet->UsbTransfer = Transfer;

    //
    // Before filling out and inserting transfers, take a look to see if the
    // device address has changed. If it has, then it should still be in the
    // enumeration phase, meaning there are no pending transfers floating
    // around.
    //

    QueueDeviceAddress = Endpoint->Queue.HardwareQueueHead->Destination &
                         EHCI_QUEUE_DEVICE_ADDRESS_MASK;

    if (Transfer->DeviceAddress != QueueDeviceAddress) {

        ASSERT((QueueDeviceAddress == 0) && (Transfer->DeviceAddress != 0));
        ASSERT(LIST_EMPTY(&(Endpoint->TransferListHead)) != FALSE);

        Endpoint->Queue.HardwareQueueHead->Destination |=
                      Transfer->DeviceAddress & EHCI_QUEUE_DEVICE_ADDRESS_MASK;
    }

    //
    // Initialize and submit the EHCI transfer set.
    //

    Status = EhcipSubmitTransferSet(Controller,
                                    Endpoint,
                                    TransferSet,
                                    NULL,
                                    FALSE);

    return Status;
}

KSTATUS
EhcipSubmitPolledTransfer (
    PVOID HostControllerContext,
    PVOID EndpointContext,
    PUSB_TRANSFER_INTERNAL Transfer,
    PVOID TransferContext
    )

/*++

Routine Description:

    This routine submits a transfer to the USB host controller for execution
    and busy waits for it to complete. This routine is meant for crash dump
    support to allow USB transfers when the system is fragile. As a result, it
    forgoes acquiring the normal sequence of locks.

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

    PEHCI_CONTROLLER Controller;
    PEHCI_TRANSFER EhciTransfer;
    PEHCI_ENDPOINT Endpoint;
    volatile PULONG HardwareStatus;
    PEHCI_TRANSFER_QUEUE Queue;
    UCHAR QueueDeviceAddress;
    BOOL RemoveSet;
    KSTATUS Status;
    ULONGLONG Timeout;
    PEHCI_TRANSFER *TransferArray;
    ULONG TransferCount;
    ULONG TransferIndex;
    PEHCI_TRANSFER_SET TransferSet;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Controller = (PEHCI_CONTROLLER)HostControllerContext;
    Endpoint = (PEHCI_ENDPOINT)EndpointContext;
    TransferSet = (PEHCI_TRANSFER_SET)TransferContext;
    TransferSet->UsbTransfer = Transfer;
    TransferArray = (PEHCI_TRANSFER *)&(TransferSet->Transfer);

    //
    // Then endpoint better not be in the middle of a transfer.
    //

    ASSERT(LIST_EMPTY(&(Endpoint->TransferListHead)) != FALSE);

    //
    // The queue head should be pointing at the dummy transfer and that dummy
    // transfer should be the end of the line.
    //

    Queue = &(Endpoint->Queue);

    ASSERT(Queue->HardwareQueueHead->TransferOverlay.NextTransfer ==
           Queue->DummyTransfer->PhysicalAddress);

    ASSERT(Queue->DummyTransfer->HardwareTransfer->NextTransfer ==
           EHCI_LINK_TERMINATE);

    ASSERT(Queue->DummyTransfer->HardwareTransfer->AlternateNextTransfer ==
           EHCI_LINK_TERMINATE);

    ASSERT(Queue->DummyTransfer->HardwareTransfer->Token ==
           EHCI_TRANSFER_STATUS_HALTED);

    //
    // Before filling out and inserting transfers, assert that the device's
    // address has not changed. Polled I/O should not be used during a device's
    // enumeration phase.
    //

    QueueDeviceAddress = Queue->HardwareQueueHead->Destination &
                         EHCI_QUEUE_DEVICE_ADDRESS_MASK;

    ASSERT(Transfer->DeviceAddress == QueueDeviceAddress);

    //
    // Initialize and submit the EHCI transfer set.
    //

    Status = EhcipSubmitTransferSet(Controller,
                                    Endpoint,
                                    TransferSet,
                                    &TransferCount,
                                    TRUE);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // The transfer is under way. Time to wait for it to complete. This
    // requires a busy spin as threads cannot yield in the limited environment
    // this routine is meant for.
    //

    Timeout = HlQueryTimeCounter() +
              (HlQueryTimeCounterFrequency() * EHCI_POLLED_TRANSFER_TIMEOUT);

    for (TransferIndex = 0; TransferIndex < TransferCount; TransferIndex += 1) {
        EhciTransfer = TransferArray[TransferIndex];
        HardwareStatus = &(EhciTransfer->HardwareTransfer->Token);
        while ((*HardwareStatus & EHCI_TRANSFER_STATUS_ACTIVE) != 0) {
            if (HlQueryTimeCounter() > Timeout) {
                Transfer->Public.Status = STATUS_TIMEOUT;
                goto SubmitPolledTransferEnd;
            }
        }

        RemoveSet = EhcipProcessPotentiallyCompletedTransfer(EhciTransfer);
        if (RemoveSet != FALSE) {
            break;
        }
    }

    EhcipRemoveCompletedTransferSet(Controller, TransferSet);

SubmitPolledTransferEnd:
    return Transfer->Public.Status;
}

KSTATUS
EhcipSubmitTransferSet (
    PEHCI_CONTROLLER Controller,
    PEHCI_ENDPOINT Endpoint,
    PEHCI_TRANSFER_SET TransferSet,
    PULONG SubmittedTransferCount,
    BOOL LockNotRequired
    )

/*++

Routine Description:

    This routine submits the given transfer set on the provided endpoint.

Arguments:

    Controller - Supplies a pointer to the EHCI controller context.

    Endpoint - Supplies a pointer to the endpoint that owns the transfer set.

    TransferSet - Supplies a pointer to the transfer set to submit.

    SubmittedTransferCount - Supplies an optional pointer to a boolean that
        receives the total number of transfers submitted for the set.

    LockNotRequired - Supplies a pointer indicating whether or not the
        controllers lock is required when submitting. The default is FALSE.

Return Value:

    Status code.

--*/

{

    LIST_ENTRY ControllerList;
    BOOL ControlTransfer;
    BOOL DataToggle;
    PEHCI_TRANSFER EhciTransfer;
    LIST_ENTRY EndpointList;
    PEHCI_TRANSFER FinalTransfer;
    BOOL ForceShortTransfer;
    BOOL LastTransfer;
    ULONG Length;
    ULONG Offset;
    RUNLEVEL OldRunLevel;
    ULONG PageOffset;
    PEHCI_TRANSFER PreviousTransfer;
    ULONG TotalLength;
    PUSB_TRANSFER_INTERNAL Transfer;
    PEHCI_TRANSFER *TransferArray;
    ULONG TransferCount;
    ULONG TransferIndex;

    ControlTransfer = FALSE;
    EhciTransfer = NULL;
    FinalTransfer = NULL;
    Transfer = TransferSet->UsbTransfer;
    TransferArray = (PEHCI_TRANSFER *)&(TransferSet->Transfer);

    //
    // This queue had better be inserted.
    //

    ASSERT(Endpoint->Queue.ListEntry.Next != NULL);

    //
    // The transfer set had better not already be queued.
    //

    ASSERT((TransferSet->Flags & EHCI_TRANSFER_SET_FLAG_QUEUED) == 0);

    //
    // Initialize the state to queued. Old state from the last go-around should
    // be wiped.
    //

    TransferSet->Flags = EHCI_TRANSFER_SET_FLAG_QUEUED;

    //
    // Assume that this is going to be a rousing success.
    //

    Transfer->Public.Status = STATUS_SUCCESS;
    Transfer->Public.Error = UsbErrorNone;

    //
    // Determine the number of EHCI transfers needed for this USB transfer,
    // and loop filling them out. This is necessary because the number of
    // EHCI transfers per USB transfer is not constant; the system may re-use a
    // transfer and change the length.
    //

    PageOffset = REMAINDER(Transfer->Public.BufferPhysicalAddress,
                           EHCI_PAGE_SIZE);

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
        PageOffset += sizeof(USB_SETUP_PACKET);
        PageOffset = REMAINDER(PageOffset, EHCI_PAGE_SIZE);
    }

    ForceShortTransfer = FALSE;
    if ((Transfer->Public.Flags &
         USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER) != 0) {

        ForceShortTransfer = TRUE;
    }

    //
    // If the USB transfer has data, the number of data transfers depends on
    // the length of the data and the page offset for the start of the data.
    //

    if (TotalLength != 0) {
        TotalLength += PageOffset;
        TransferCount += TotalLength / EHCI_TRANSFER_MAX_PACKET_SIZE;
        if ((TotalLength % EHCI_TRANSFER_MAX_PACKET_SIZE) != 0) {
            TransferCount += 1;
        }

        //
        // If a short transfer must be sent and the total length is a multiple,
        // of the max packet size, then add an extra transfer to make sure a
        // short transfer is sent.
        //

        if ((ForceShortTransfer != FALSE) &&
            ((TotalLength % Endpoint->MaxPacketSize) == 0)) {

            TransferCount += 1;
        }

    //
    // Make sure at least one packet is set for zero-length packets. Unless a
    // short transfer is being forced, exclude control transfers as there is
    // just no data phase if this is the case.
    //

    } else if ((ForceShortTransfer != FALSE) ||
               (Endpoint->TransferType != UsbTransferTypeControl)) {

        TransferCount = 1;
    }

    ASSERT(TransferSet->TransferCount >= TransferCount);

    //
    // Now that the transfer count has been computed, save the ultimate
    // transfer if it's a control request.
    //

    if (ControlTransfer != FALSE) {
        FinalTransfer = TransferArray[TransferCount - 1];
    }

    PageOffset = REMAINDER(Transfer->Public.BufferPhysicalAddress,
                           EHCI_PAGE_SIZE);

    DataToggle = FALSE;
    Offset = 0;
    LastTransfer = FALSE;
    INITIALIZE_LIST_HEAD(&ControllerList);
    INITIALIZE_LIST_HEAD(&EndpointList);
    for (TransferIndex = 0; TransferIndex < TransferCount; TransferIndex += 1) {

        //
        // Calculate the length for this transfer descriptor.
        //

        Length = EHCI_TRANSFER_MAX_PACKET_SIZE - PageOffset;
        if ((Offset + Length) > Transfer->Public.Length) {
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
            // The last part of a control transfer is the status phase and the
            // length better be zero.
            //

            ASSERT((LastTransfer == FALSE) || (Length == 0));
        }

        ASSERT((Length != 0) ||
               (LastTransfer != FALSE) ||
               ((ForceShortTransfer != FALSE) && (ControlTransfer != FALSE)));

        //
        // Fill out this transfer descriptor.
        //

        EhciTransfer = TransferArray[TransferIndex];
        EhcipFillOutTransferDescriptor(Controller,
                                       EhciTransfer,
                                       Offset,
                                       Length,
                                       LastTransfer,
                                       &DataToggle,
                                       FinalTransfer);

        //
        // Point the previous transfer to this transfer.
        //

        if (TransferIndex != 0) {
            PreviousTransfer = TransferArray[TransferIndex - 1];
            PreviousTransfer->HardwareTransfer->NextTransfer =
                                          (ULONG)EhciTransfer->PhysicalAddress;
        }

        ASSERT(EhciTransfer->GlobalListEntry.Next == NULL);

        INSERT_BEFORE(&(EhciTransfer->EndpointListEntry), &EndpointList);
        INSERT_BEFORE(&(EhciTransfer->GlobalListEntry), &ControllerList);

        //
        // Advance the buffer position.
        //

        Offset += Length;
        PageOffset += Length;
        PageOffset = REMAINDER(PageOffset, EHCI_PAGE_SIZE);
    }

    //
    // Acquire the lock, if requested. It did not need to be acquired for
    // filling out the descriptors because no modifiable global or endpoint
    // state was read or modified.
    //

    if (LockNotRequired == FALSE) {
        OldRunLevel = EhcipAcquireControllerLock(Controller);
    }

    //
    // Add the transfer to the endpoint and controller global lists by
    // appending the locally created lists.
    //

    APPEND_LIST(&EndpointList, &(Endpoint->TransferListHead));
    APPEND_LIST(&ControllerList, &(Controller->TransferListHead));

    //
    // The transfer is ready to go. Do the actual insertion.
    //

    if (Transfer->Type == UsbTransferTypeIsochronous) {

        //
        // TODO: Implement isochronous support.
        //

        ASSERT(FALSE);

        return STATUS_NOT_IMPLEMENTED;

    } else {

        //
        // Mark the last transfer, then submit the transfer array to the
        // hardware.
        //

        ASSERT(TransferCount != 0);

        TransferArray[TransferCount - 1]->LastTransfer = TRUE;
        EhcipLinkTransferSetInHardware(TransferSet);
    }

    //
    // All done. Release the lock, if necessary, and return.
    //

    if (LockNotRequired == FALSE) {
        EhcipReleaseControllerLock(Controller, OldRunLevel);
    }

    if (SubmittedTransferCount != NULL) {
        *SubmittedTransferCount = TransferCount;
    }

    return STATUS_SUCCESS;
}

KSTATUS
EhcipCancelTransfer (
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

    ULONG CommandRegister;
    PEHCI_CONTROLLER Controller;
    ULONG HorizontalLink;
    ULONG InterruptTreeLevel;
    RUNLEVEL OldRunLevel;
    ULONG PollRate;
    PEHCI_TRANSFER_QUEUE Queue;
    PEHCI_TRANSFER_QUEUE QueueBefore;
    KSTATUS Status;
    PEHCI_TRANSFER_SET TransferSet;

    Controller = (PEHCI_CONTROLLER)HostControllerContext;
    Status = STATUS_SUCCESS;
    TransferSet = (PEHCI_TRANSFER_SET)TransferContext;

    ASSERT(TransferSet->UsbTransfer == Transfer);

    //
    // Lock the controller to manipulate lists.
    //

    OldRunLevel = EhcipAcquireControllerLock(Controller);

    //
    // If the transfer set is not currently queued, then there is nothing to be
    // done.
    //

    if ((TransferSet->Flags & EHCI_TRANSFER_SET_FLAG_QUEUED) == 0) {
        Status = STATUS_TOO_LATE;
        goto CancelTransferEnd;
    }

    //
    // Isochronous transfers are handled differently.
    //

    if (Transfer->Type == UsbTransferTypeIsochronous) {

        ASSERT(FALSE);

        Status = STATUS_NOT_IMPLEMENTED;
        goto CancelTransferEnd;

    //
    // Remove the interrupt endpoint's queue head from the synchronous schedule.
    //

    } else if (Transfer->Type == UsbTransferTypeInterrupt) {
        Queue = &(TransferSet->Endpoint->Queue);

        //
        // This code assumes that there is only one transfer on an interrupt
        // endpoint.
        //

        ASSERT(Queue->ListEntry.Next != NULL);

        QueueBefore = LIST_VALUE(Queue->ListEntry.Previous,
                                 EHCI_TRANSFER_QUEUE,
                                 ListEntry);

        HlWriteRegister32(&(QueueBefore->HardwareQueueHead->HorizontalLink),
                          Queue->HardwareQueueHead->HorizontalLink);

        LIST_REMOVE(&(Queue->ListEntry));
        Queue->ListEntry.Next = NULL;

        //
        // Now release the lock and wait a full frame to make sure that the
        // periodic schedule has moved beyond this queue head. This simple wait
        // accounts for split transactions, but will need to be updated if
        // Frame Split Transaction Nodes are supported.
        //

        EhcipReleaseControllerLock(Controller, OldRunLevel);
        KeDelayExecution(FALSE, FALSE, 1 * MICROSECONDS_PER_MILLISECOND);

        //
        // Reacquire the lock to complete the cancellation.
        //

        OldRunLevel = EhcipAcquireControllerLock(Controller);

        ASSERT(Queue->ListEntry.Next == NULL);

        //
        // If the interrupt was completed while the lock was released, then
        // return that it was too late to cancel.
        //

        if ((TransferSet->Flags & EHCI_TRANSFER_SET_FLAG_QUEUED) == 0) {
            Status = STATUS_TOO_LATE;

        //
        // Otherwise mark the transfer as cancelled, remove the transfer set
        // and complete the callback.
        //

        } else {
            Transfer->Public.Status = STATUS_OPERATION_CANCELLED;
            Transfer->Public.Error = UsbErrorTransferCancelled;
            EhcipRemoveCancelledTransferSet(Controller, TransferSet);
            UsbHostProcessCompletedTransfer(TransferSet->UsbTransfer);
        }

        //
        // Add the queue back into the periodic schedule.
        //

        PollRate = TransferSet->Endpoint->PollRate;

        ASSERT(PollRate != 0);

        InterruptTreeLevel = EHCI_PERIODIC_SCHEDULE_TREE_DEPTH - 1;
        while (((PollRate & 0x1) == 0) && (InterruptTreeLevel != 0)) {
            PollRate = PollRate >> 1;
            InterruptTreeLevel -= 1;
        }

        ASSERT(InterruptTreeLevel < EHCI_PERIODIC_SCHEDULE_TREE_DEPTH);

        QueueBefore = &(Controller->InterruptTree[InterruptTreeLevel]);
        INSERT_AFTER(&(Queue->ListEntry), &(QueueBefore->ListEntry));
        Queue->HardwareQueueHead->HorizontalLink =
                                QueueBefore->HardwareQueueHead->HorizontalLink;

        HorizontalLink = (ULONG)Queue->PhysicalAddress;

        ASSERT((HorizontalLink & (~EHCI_LINK_ADDRESS_MASK)) == 0);

        HorizontalLink |= EHCI_LINK_TYPE_QUEUE_HEAD;
        HlWriteRegister32(&(QueueBefore->HardwareQueueHead->HorizontalLink),
                          HorizontalLink);

    //
    // Remove bulk and control endpoint's queue head from the asynchronous
    // schedule. The transfer set will be fully removed from the queue head
    // once the interrupt for async-on-advance has fired.
    //

    } else {

        ASSERT((Transfer->Type == UsbTransferTypeControl) ||
               (Transfer->Type == UsbTransferTypeBulk));

        //
        // Mark that the transfer set is in the process of being cancelled.
        //

        TransferSet->Flags |= EHCI_TRANSFER_SET_FLAG_CANCELLING;

        //
        // If the queue's async on advance state is already set, that means it
        // is already out of the hardware's queue head and on a list. This
        // transfer will be handled by interrupt processing.
        //

        Queue = &(TransferSet->Endpoint->Queue);
        if (Queue->AsyncOnAdvanceCancel != FALSE) {
            goto CancelTransferEnd;
        }

        Queue->AsyncOnAdvanceCancel = TRUE;

        //
        // Otherwise the queue must be removed from the hardware list.
        //

        QueueBefore = LIST_VALUE(Queue->ListEntry.Previous,
                                 EHCI_TRANSFER_QUEUE,
                                 ListEntry);

        HlWriteRegister32(&(QueueBefore->HardwareQueueHead->HorizontalLink),
                          Queue->HardwareQueueHead->HorizontalLink);

        LIST_REMOVE(&(Queue->ListEntry));

        //
        // If the asynchronous on advance ready list is empty, then add this
        // queue head to the ready list and ring the doorbell.
        //

        if (LIST_EMPTY(&(Controller->AsyncOnAdvanceReadyListHead)) != FALSE) {
            INSERT_BEFORE(&(Queue->ListEntry),
                          &(Controller->AsyncOnAdvanceReadyListHead));

            CommandRegister = Controller->CommandRegister;
            CommandRegister |= EHCI_COMMAND_INTERRUPT_ON_ASYNC_ADVANCE;
            EHCI_WRITE_REGISTER(Controller,
                                EhciRegisterUsbCommand,
                                CommandRegister);

        //
        // Otherwise the doorbell has already been rung. This queue head will
        // have to wait for the next chance to ring it. Put it on the pending
        // list.
        //

        } else {
            INSERT_BEFORE(&(Queue->ListEntry),
                          &(Controller->AsyncOnAdvancePendingListHead));
        }
    }

CancelTransferEnd:

    //
    // Release the lock and return.
    //

    EhcipReleaseControllerLock(Controller, OldRunLevel);
    return Status;
}

KSTATUS
EhcipGetRootHubStatus (
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
    PEHCI_CONTROLLER Controller;
    USHORT HardwareStatus;
    ULONG PortIndex;
    PUSB_PORT_STATUS PortStatus;
    USHORT SoftwareStatus;

    Controller = (PEHCI_CONTROLLER)HostControllerContext;

    ASSERT(Controller->PortCount != 0);
    ASSERT(HubStatus->PortStatus != NULL);

    for (PortIndex = 0; PortIndex < Controller->PortCount; PortIndex += 1) {
        HardwareStatus = EHCI_READ_PORT_REGISTER(Controller, PortIndex);

        //
        // Set the corresponding software bits. If the owner bit is set,
        // pretend like there's nothing here.
        //

        SoftwareStatus = 0;
        if (((HardwareStatus & EHCI_PORT_CONNECT_STATUS) != 0) &&
            ((HardwareStatus & EHCI_PORT_OWNER) == 0)) {

            SoftwareStatus |= USB_PORT_STATUS_CONNECTED;

            //
            // If the port is presenting a K state, then it's a low speed.
            // Otherwise, assume that if it hasn't yet been passed off to the
            // companion controller that it's a high speed device. If it turns
            // out to be a full speed device, it will eventually get
            // disconnected from here and passed on to the companion controller.
            //

            if ((HardwareStatus & EHCI_PORT_LINE_STATE_MASK) ==
                EHCI_PORT_LINE_STATE_K) {

                HubStatus->PortDeviceSpeed[PortIndex] = UsbDeviceSpeedLow;

                //
                // Release ownership of this device.
                //

                HardwareStatus |= EHCI_PORT_OWNER;
                EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
                HardwareStatus = 0;
                SoftwareStatus = 0;

            } else {
                HubStatus->PortDeviceSpeed[PortIndex] = UsbDeviceSpeedHigh;
            }
        }

        if ((HardwareStatus & EHCI_PORT_ENABLE) != 0) {
            SoftwareStatus |= USB_PORT_STATUS_ENABLED;
        }

        if ((HardwareStatus & EHCI_PORT_RESET) != 0) {
            SoftwareStatus |= USB_PORT_STATUS_RESET;
        }

        if ((HardwareStatus & EHCI_PORT_OVER_CURRENT_ACTIVE) != 0) {
            SoftwareStatus |= USB_PORT_STATUS_OVER_CURRENT;
        }

        //
        // If the new software status is different from the current software
        // status, record the change bits and set the new software status.
        //

        PortStatus = &(HubStatus->PortStatus[PortIndex]);
        if (SoftwareStatus != PortStatus->Status) {
            ChangeBits = SoftwareStatus ^ PortStatus->Status;

            //
            // Because the change bits correspond with the status bits 1-to-1,
            // just OR in the change bits.
            //

            PortStatus->Change |= ChangeBits;
            PortStatus->Status = SoftwareStatus;
        }

        //
        // Acknowledge the over current change bit if it is set.
        //

        if ((HardwareStatus & EHCI_PORT_OVER_CURRENT_CHANGE) != 0) {
            PortStatus->Change |= USB_PORT_STATUS_CHANGE_OVER_CURRENT;
            EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
        }

        //
        // Acknowledge the port connection status change in the hardware and
        // set the bit in the software's port status change bits. It may be
        // that the port transitioned from connected to connected and the above
        // checks did not pick up the change.
        //

        if ((HardwareStatus & EHCI_PORT_CONNECT_STATUS_CHANGE) != 0) {
            PortStatus->Change |= USB_PORT_STATUS_CHANGE_CONNECTED;

            //
            // If the port is not in the middle of a reset, clear the connect
            // status change bit in the hardware by setting it to 1. Resets
            // clear the connect status changed bit.
            //

            if ((HardwareStatus & EHCI_PORT_RESET) == 0) {
                EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
            }
        }

        if ((EhciDebugFlags & EHCI_DEBUG_PORTS) != 0) {
            RtlDebugPrint(
                     "EHCI: Controller 0x%x Port %d Status 0x%x. "
                     "Connected %d, Owner %d, Enabled %d, Reset %d, "
                     "Changed %d.\n",
                     Controller,
                     PortIndex,
                     HardwareStatus,
                     (HardwareStatus & EHCI_PORT_CONNECT_STATUS) != 0,
                     (HardwareStatus & EHCI_PORT_OWNER) != 0,
                     (HardwareStatus & EHCI_PORT_ENABLE) != 0,
                     (HardwareStatus & EHCI_PORT_RESET) != 0,
                     (HardwareStatus & EHCI_PORT_CONNECT_STATUS_CHANGE) != 0);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
EhcipSetRootHubStatus (
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

    PEHCI_CONTROLLER Controller;
    USHORT HardwareStatus;
    USHORT OriginalHardwareStatus;
    ULONG PortIndex;
    PUSB_PORT_STATUS PortStatus;

    Controller = (PEHCI_CONTROLLER)HostControllerContext;

    ASSERT(Controller->PortCount != 0);

    //
    // The supplied hub status has change bits indicate what is to be newly set
    // in each port's software status. This routine will clear any change bits
    // it handles.
    //

    for (PortIndex = 0; PortIndex < Controller->PortCount; PortIndex += 1) {

        //
        // The caller is required to notify the routine about what needs to be
        // set by updating the change bits. If there are not changed bits, then
        // skip the port.
        //

        PortStatus = &(HubStatus->PortStatus[PortIndex]);
        if (PortStatus->Change == 0) {
            continue;
        }

        OriginalHardwareStatus = EHCI_READ_PORT_REGISTER(Controller, PortIndex);
        HardwareStatus = OriginalHardwareStatus;

        //
        // Leave the port alone if it's not owned by EHCI and there isn't an
        // active reset.
        //

        if (((HardwareStatus & EHCI_PORT_OWNER) != 0) &&
            ((PortStatus->Status & USB_PORT_STATUS_RESET) == 0)) {

            //
            // Clear any change bits that this routine would otherwise handle.
            // This acknowledges that they were dealt with (i.e. this port is
            // dead and there is nothing anyone else should do with the change
            // bits later).
            //

            PortStatus->Change &= ~(USB_PORT_STATUS_CHANGE_RESET |
                                    USB_PORT_STATUS_CHANGE_ENABLED |
                                    USB_PORT_STATUS_CHANGE_SUSPENDED);

            continue;
        }

        //
        // Clear out the bits that may potentially be adjusted.
        //

        HardwareStatus &= ~(EHCI_PORT_ENABLE | EHCI_PORT_RESET |
                            EHCI_PORT_SUSPEND | EHCI_PORT_INDICATOR_MASK |
                            EHCI_PORT_OWNER);

        //
        // Set the hardware bits according to what's passed in.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_ENABLED) != 0) {

            //
            // If the port is being enabled, then set the enabled bits, power
            // it on and turn on the green indicator.
            //

            if ((PortStatus->Status & USB_PORT_STATUS_ENABLED) != 0) {
                HardwareStatus |= EHCI_PORT_ENABLE |
                                  EHCI_PORT_INDICATOR_GREEN |
                                  EHCI_PORT_POWER;
            }

            //
            // Acknowledge that the enable bit was handled.
            //

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_ENABLED;
        }

        //
        // The EHCI spec says that whenever the reset bit is set, the enable
        // bit must be cleared. If the port is high speed, the enable bit will
        // be set automatically once the reset completes.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_RESET) != 0) {
            if ((PortStatus->Status & USB_PORT_STATUS_RESET) != 0) {
                HardwareStatus |= EHCI_PORT_RESET;
                HardwareStatus &= ~EHCI_PORT_ENABLE;
            }

            //
            // Acknowledge that the reset bit was handled.
            //

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_RESET;
        }

        //
        // Suspend the port if requested.
        //

        if ((PortStatus->Change & USB_PORT_STATUS_CHANGE_SUSPENDED) != 0) {
            if ((PortStatus->Status & USB_PORT_STATUS_SUSPENDED) != 0) {
                HardwareStatus |= EHCI_PORT_SUSPEND;
            }

            PortStatus->Change &= ~USB_PORT_STATUS_CHANGE_SUSPENDED;
        }

        //
        // Write out the new value if it is different than the old one.
        //

        if (HardwareStatus != OriginalHardwareStatus) {
            EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
        }

        //
        // If reset was set, wait the required amount of time and then clear
        // the reset bit, as if this were a hub and it was cleared
        // automatically.
        //

        if ((HardwareStatus & EHCI_PORT_RESET) != 0) {
            HlBusySpin(20 * 1000);
            HardwareStatus = EHCI_READ_PORT_REGISTER(Controller, PortIndex);
            HardwareStatus &= ~EHCI_PORT_RESET;
            EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);

            //
            // Wait a further 5ms (the EHCI spec says the host controller has
            // to have it done in 2ms), and if the port is not enabled, then
            // it's a full speed device, and should be handed off to the
            // companion controller.
            //

            HlBusySpin(5 * 1000);
            HardwareStatus = EHCI_READ_PORT_REGISTER(Controller, PortIndex);
            if ((HardwareStatus & EHCI_PORT_ENABLE) == 0) {
                HardwareStatus |= EHCI_PORT_OWNER;
                EHCI_WRITE_PORT_REGISTER(Controller, PortIndex, HardwareStatus);
            }
        }
    }

    return STATUS_SUCCESS;
}

RUNLEVEL
EhcipAcquireControllerLock (
    PEHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine acquires the given EHCI controller's lock at dispatch level.

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
EhcipReleaseControllerLock (
    PEHCI_CONTROLLER Controller,
    RUNLEVEL OldRunLevel
    )

/*++

Routine Description:

    This routine releases the given EHCI controller's lock, and returns the
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
EhcipProcessInterrupt (
    PEHCI_CONTROLLER Controller,
    ULONG PendingStatusBits
    )

/*++

Routine Description:

    This routine performs the work associated with receiving an EHCI interrupt.
    This routine runs at dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller.

    PendingStatusBits - Supplies the pending status bits to service.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEHCI_TRANSFER NextTransfer;
    RUNLEVEL OldRunLevel;
    BOOL RemoveSet;
    PEHCI_TRANSFER Transfer;
    PEHCI_TRANSFER_SET TransferSet;

    //
    // Lock the controller and loop until this routine has caught up with the
    // interrupts.
    //

    OldRunLevel = EhcipAcquireControllerLock(Controller);

    //
    // If the interrupt was a device change interrupt, then notify the USB core
    // that the root hub noticed a device change.
    //

    if ((PendingStatusBits & EHCI_STATUS_PORT_CHANGE_DETECT) != 0) {
        UsbHostNotifyPortChange(Controller->UsbCoreHandle);
    }

    //
    // TODO: Go through the isochronous transfers.
    //

    ASSERT(LIST_EMPTY(&(Controller->IsochronousTransferListHead)) != FALSE);

    //
    // Loop through every transfer in the schedule.
    //

    CurrentEntry = Controller->TransferListHead.Next;
    while (CurrentEntry != &(Controller->TransferListHead)) {

        ASSERT((CurrentEntry != NULL) && (CurrentEntry->Next != NULL));

        Transfer = LIST_VALUE(CurrentEntry, EHCI_TRANSFER, GlobalListEntry);
        CurrentEntry = CurrentEntry->Next;
        RemoveSet = EhcipProcessPotentiallyCompletedTransfer(Transfer);
        if (RemoveSet != FALSE) {

            //
            // Get the current entry off of this set, as several transfers
            // may be removed here.
            //

            TransferSet = Transfer->Set;
            if (CurrentEntry != &(Controller->TransferListHead)) {
                NextTransfer = LIST_VALUE(CurrentEntry,
                                          EHCI_TRANSFER,
                                          GlobalListEntry);

                while (NextTransfer->Set == TransferSet) {
                    CurrentEntry = CurrentEntry->Next;
                    if (CurrentEntry == &(Controller->TransferListHead)) {
                        break;
                    }

                    NextTransfer = LIST_VALUE(CurrentEntry,
                                              EHCI_TRANSFER,
                                              GlobalListEntry);
                }
            }

            //
            // Remove the transfer set from the owning endpoint's queue and
            // call the completion routine.
            //

            EhcipRemoveCompletedTransferSet(Controller, TransferSet);
            UsbHostProcessCompletedTransfer(TransferSet->UsbTransfer);
        }
    }

    //
    // If the interrupt was the "interrupt on asynchronous schedule advance"
    // doorbell, then process the ready list, knowing that hardware is no
    // longer using it. Run this after processing all the transfers in case a
    // transfer finished before any of the queues were removed.
    //

    if ((PendingStatusBits & EHCI_STATUS_INTERRUPT_ON_ASYNC_ADVANCE) != 0) {
        EhcipProcessAsyncOnAdvanceInterrupt(Controller);
    }

    //
    // Release the lock.
    //

    EhcipReleaseControllerLock(Controller, OldRunLevel);
    return;
}

VOID
EhcipFillOutTransferDescriptor (
    PEHCI_CONTROLLER Controller,
    PEHCI_TRANSFER EhciTransfer,
    ULONG Offset,
    ULONG Length,
    BOOL LastTransfer,
    PBOOL DataToggle,
    PEHCI_TRANSFER AlternateNextTransfer
    )

/*++

Routine Description:

    This routine fills out an EHCI transfer descriptor.

Arguments:

    Controller - Supplies a pointer to the EHCI controller.

    EhciTransfer - Supplies a pointer to EHCI's transfer descriptor information.

    Offset - Supplies the offset from the public transfer physical address that
        this transfer descriptor should be initialize to.

    Length - Supplies the length of the transfer, in bytes.

    LastTransfer - Supplies a boolean indicating if this transfer descriptor
        represents the last transfer in a set. For control transfers, this is
        the status phase where the in/out is reversed and the length had better
        be zero.

    DataToggle - Supplies a pointer to a boolean that indicates the current
        data toggle status for the overall transfer. This routine will update
        the data toggle upon return to indicate what the data toggle should be
        for the next transfer to be initialized.

    AlternateNextTransfer - Supplies an optional pointer to a transfer to move
        to if this transfer is an IN and comes up short of its max transfer
        length.

Return Value:

    None.

--*/

{

    ULONG BufferIndex;
    ULONG BufferPhysical;
    ULONG EndAddress;
    PEHCI_ENDPOINT Endpoint;
    PEHCI_TRANSFER_DESCRIPTOR HardwareTransfer;
    ULONG Token;
    PUSB_TRANSFER_INTERNAL Transfer;
    PEHCI_TRANSFER_SET TransferSet;

    TransferSet = EhciTransfer->Set;
    Endpoint = TransferSet->Endpoint;
    Transfer = TransferSet->UsbTransfer;
    EhciTransfer->LastTransfer = FALSE;
    HardwareTransfer = EhciTransfer->HardwareTransfer;

    //
    // Set up the buffer pointers.
    //

    BufferPhysical = Transfer->Public.BufferPhysicalAddress + Offset;
    EndAddress = BufferPhysical + Length;

    ASSERT((REMAINDER(BufferPhysical, EHCI_PAGE_SIZE) + Length) <=
           EHCI_TRANSFER_MAX_PACKET_SIZE);

    for (BufferIndex = 0;
         BufferIndex < EHCI_TRANSFER_POINTER_COUNT;
         BufferIndex += 1) {

        if (BufferPhysical < EndAddress) {
            HardwareTransfer->BufferPointer[BufferIndex] = BufferPhysical;
            BufferPhysical += EHCI_PAGE_SIZE;
            BufferPhysical = ALIGN_RANGE_DOWN(BufferPhysical, EHCI_PAGE_SIZE);

        } else {
            HardwareTransfer->BufferPointer[BufferIndex] = 0;
        }

        HardwareTransfer->BufferAddressHigh[BufferIndex] = 0;
    }

    //
    // Figure out the token value for this transfer descriptor.
    //

    EhciTransfer->TransferLength = Length;
    Token = (Length << EHCI_TRANSFER_TOTAL_BYTES_SHIFT);
    Token |= EHCI_TRANSFER_3_ERRORS_ALLOWED;
    Token |= EHCI_TRANSFER_STATUS_ACTIVE;

    //
    // The first packet in a control transfer is always a setup packet. It does
    // not have the data toggle set, but prepares for the next transfer to have
    // the bit set by setting the data toggle to true.
    //

    if ((Endpoint->TransferType == UsbTransferTypeControl) && (Offset == 0)) {
        Token |= EHCI_TRANSFER_PID_CODE_SETUP;
        *DataToggle = TRUE;

    //
    // Do it backwards if this is the status phase. Status phases always have
    // a data toggle of 1. The data toggle boolean does not need to be updated
    // as this is always the last transfer.
    //

    } else if ((Endpoint->TransferType == UsbTransferTypeControl) &&
               (LastTransfer != FALSE)) {

        Token |= EHCI_TRANSFER_DATA_TOGGLE;

        ASSERT((Length == 0) &&
               (Endpoint->TransferType == UsbTransferTypeControl));

        if (Transfer->Public.Direction == UsbTransferDirectionIn) {
            Token |= EHCI_TRANSFER_PID_CODE_OUT;

        } else {

            ASSERT(Transfer->Public.Direction == UsbTransferDirectionOut);

            Token |= EHCI_TRANSFER_PID_CODE_IN;
        }

    //
    // Not setup and not status, fill this out like a normal descriptor.
    //

    } else {
        if (Transfer->Public.Direction == UsbTransferDirectionIn) {
            Token |= EHCI_TRANSFER_PID_CODE_IN;

        } else {

            ASSERT(Transfer->Public.Direction == UsbTransferDirectionOut);

            Token |= EHCI_TRANSFER_PID_CODE_OUT;
        }

        //
        // The host controller keeps track of the data toggle bits for control
        // transfers (rather than the hardware), so set the data toggle bit
        // accordingly and update the data toggle boolean for the next
        // transfer.
        //

        if (Endpoint->TransferType == UsbTransferTypeControl) {
            if (*DataToggle != FALSE) {
                Token |= EHCI_TRANSFER_DATA_TOGGLE;
                *DataToggle = FALSE;

            } else {
                *DataToggle = TRUE;
            }
        }
    }

    ASSERT((Endpoint->Speed == UsbDeviceSpeedLow) ||
           (Endpoint->Speed == UsbDeviceSpeedFull) ||
           (Endpoint->Speed == UsbDeviceSpeedHigh));

    //
    // Don't set the interrupt flag if 1) This is not the last descriptor or
    // 2) The caller requested not to.
    //

    if ((LastTransfer != FALSE) &&
        ((Transfer->Public.Flags &
          USB_TRANSFER_FLAG_NO_INTERRUPT_ON_COMPLETION) == 0)) {

        Token |= EHCI_TRANSFER_INTERRUPT_ON_COMPLETE;
    }

    HardwareTransfer->Token = Token;
    if ((EhciDebugFlags & EHCI_DEBUG_TRANSFERS) != 0) {
        RtlDebugPrint("EHCI: Adding transfer (0x%08x) PA 0x%I64x to endpoint "
                      "(0x%08x): Token 0x%08x.\n",
                      EhciTransfer,
                      EhciTransfer->PhysicalAddress,
                      Endpoint,
                      EhciTransfer->HardwareTransfer->Token);
    }

    //
    // Set up the link pointers of the transfer descriptor. With the exception
    // of isochronous transfers (which will get patched up later) transfer
    // descriptors are always put at the end of the queue. They confusingly
    // point back to the first transfer because the first transfer will
    // eventually get swapped out to be a dummy last transfer. That fact is
    // anticipated here so that now all transfers lead to the dummy at the end.
    //

    HardwareTransfer->NextTransfer =
                            (ULONG)(TransferSet->Transfer[0]->PhysicalAddress);

    if ((AlternateNextTransfer != NULL) &&
        (AlternateNextTransfer != EhciTransfer)) {

        ASSERT((ULONG)AlternateNextTransfer->PhysicalAddress ==
               AlternateNextTransfer->PhysicalAddress);

        HardwareTransfer->AlternateNextTransfer =
                                 (ULONG)AlternateNextTransfer->PhysicalAddress;

    } else {

        //
        // Point the next transfer to what will become the end of this set, so
        // that if a short packet comes in this transfer set will be done and
        // the queue moves to the next set of transfers.
        //

        HardwareTransfer->AlternateNextTransfer =
                            (ULONG)(TransferSet->Transfer[0]->PhysicalAddress);

    }

    if (Transfer->Type == UsbTransferTypeIsochronous) {

        //
        // TODO: Implement isochronous transfers.
        //

        ASSERT(FALSE);
    }

    return;
}

VOID
EhcipLinkTransferSetInHardware (
    PEHCI_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine links a set of transfer descriptors up to their proper queue
    head, making them visible to the hardware. This routine assumes the
    controller lock is already held.

Arguments:

    TransferSet - Supplies a pointer to the transfer set.

Return Value:

    None.

--*/

{

    PEHCI_ENDPOINT Endpoint;
    PEHCI_TRANSFER OriginalDummyTransfer;
    PEHCI_TRANSFER OriginalFirstTransfer;
    PEHCI_QUEUE_HEAD QueueHead;
    ULONG RemainingSize;
    ULONG Token;

    Endpoint = TransferSet->Endpoint;

    //
    // TODO: Implement support for isochronous.
    //

    ASSERT(Endpoint->TransferType != UsbTransferTypeIsochronous);

    OriginalDummyTransfer = Endpoint->Queue.DummyTransfer;
    OriginalFirstTransfer = TransferSet->Transfer[0];

    ASSERT((OriginalDummyTransfer->HardwareTransfer->Token &
            EHCI_TRANSFER_STATUS_HALTED) != 0);

    //
    // The way this is going to work is to not actually use the first transfer
    // of the set, but to copy it into the dummy transfer that's already on the
    // hardware list. That dummy transfer becomes the first transfer of the set,
    // and the original first transfer becomes the new dummy. Begin by saving
    // the original first transfer's token.
    //

    Token = OriginalFirstTransfer->HardwareTransfer->Token;
    OriginalFirstTransfer->HardwareTransfer->Token =
                                OriginalDummyTransfer->HardwareTransfer->Token;

    //
    // Copy the remainder of the original first transfer over the dummy, but
    // make sure it stays inactive so the hardware doesn't look at it.
    //

    OriginalDummyTransfer->HardwareTransfer->NextTransfer =
                          OriginalFirstTransfer->HardwareTransfer->NextTransfer;

    OriginalDummyTransfer->HardwareTransfer->AlternateNextTransfer =
                OriginalFirstTransfer->HardwareTransfer->AlternateNextTransfer;

    RemainingSize = sizeof(EHCI_TRANSFER_DESCRIPTOR) -
                    FIELD_OFFSET(EHCI_TRANSFER_DESCRIPTOR, BufferPointer);

    RtlCopyMemory(&(OriginalDummyTransfer->HardwareTransfer->BufferPointer),
                  &(OriginalFirstTransfer->HardwareTransfer->BufferPointer),
                  RemainingSize);

    ASSERT((OriginalDummyTransfer->EndpointListEntry.Next == NULL) &&
           (OriginalDummyTransfer->GlobalListEntry.Next == NULL) &&
           (OriginalFirstTransfer->EndpointListEntry.Next != NULL) &&
           (OriginalFirstTransfer->GlobalListEntry.Next != NULL));

    //
    // Add the dummy transfer to the software lists, and remove the original
    // first transfer.
    //

    INSERT_BEFORE(&(OriginalDummyTransfer->EndpointListEntry),
                  &(OriginalFirstTransfer->EndpointListEntry));

    INSERT_BEFORE(&(OriginalDummyTransfer->GlobalListEntry),
                  &(OriginalFirstTransfer->GlobalListEntry));

    LIST_REMOVE(&(OriginalFirstTransfer->GlobalListEntry));
    LIST_REMOVE(&(OriginalFirstTransfer->EndpointListEntry));
    OriginalFirstTransfer->EndpointListEntry.Next = NULL;
    OriginalFirstTransfer->GlobalListEntry.Next = NULL;

    //
    // Copy over any other aspects.
    //

    OriginalFirstTransfer->HardwareTransfer->NextTransfer = EHCI_LINK_TERMINATE;
    OriginalFirstTransfer->HardwareTransfer->AlternateNextTransfer =
                                                           EHCI_LINK_TERMINATE;

    OriginalDummyTransfer->TransferLength =
                                         OriginalFirstTransfer->TransferLength;

    OriginalFirstTransfer->TransferLength = 0;
    OriginalDummyTransfer->LastTransfer = OriginalFirstTransfer->LastTransfer;
    OriginalFirstTransfer->LastTransfer = FALSE;

    //
    // Switch their roles.
    //

    TransferSet->Transfer[0] = OriginalDummyTransfer;
    OriginalFirstTransfer->Set = NULL;
    OriginalDummyTransfer->Set = TransferSet;
    Endpoint->Queue.DummyTransfer = OriginalFirstTransfer;

    //
    // Make everything live by setting the token in the new first transfer.
    // Use the register write function to ensure the compiler does this in a
    // single write (and not something goofy like byte by byte). This routine
    // also serves as a full memory barrier.
    //

    QueueHead = Endpoint->Queue.HardwareQueueHead;
    HlWriteRegister32(&(OriginalDummyTransfer->HardwareTransfer->Token), Token);

    //
    // If the queue head was halted, it needs to be restarted. Zero out the
    // current descriptor so nothing gets written back, set the next link to
    // the start of the list, and zero out the token. Avoid the very rare
    // situation where the hardware got all the way through the transfers
    // linked in the previous line (and has an errata where the halted
    // descriptor is copied into the overlay).
    //

    if (((QueueHead->TransferOverlay.Token &
          EHCI_TRANSFER_STATUS_HALTED) != 0) &&
        (QueueHead->CurrentTransferDescriptorLink !=
         (ULONG)(OriginalFirstTransfer->PhysicalAddress))) {

        QueueHead->CurrentTransferDescriptorLink = 0;
        QueueHead->TransferOverlay.NextTransfer =
                                        OriginalDummyTransfer->PhysicalAddress;

        HlWriteRegister32(
                 &(QueueHead->TransferOverlay.Token),
                 QueueHead->TransferOverlay.Token & EHCI_TRANSFER_DATA_TOGGLE);
    }

    return;
}

BOOL
EhcipProcessPotentiallyCompletedTransfer (
    PEHCI_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine processes a transfer descriptor, adjusting the USB transfer if
    the transfer descriptor errored out.

Arguments:

    Transfer - Supplies a pointer to the transfer to evaluate.

Return Value:

    TRUE if the transfer set should be removed from the list because the
        transfer has failed.

    FALSE if the transfer set should not be removed from the list.

--*/

{

    ULONG HardwareStatus;
    ULONG LengthTransferred;
    PEHCI_QUEUE_HEAD QueueHead;
    BOOL RemoveSet;
    PUSB_TRANSFER UsbTransfer;

    RemoveSet = FALSE;

    //
    // Skip the transfer if it's already been dealt with.
    //

    if (Transfer->GlobalListEntry.Next == NULL) {
        goto ProcessPotentiallyCompletedTransferEnd;
    }

    HardwareStatus = Transfer->HardwareTransfer->Token;
    if ((HardwareStatus & EHCI_TRANSFER_STATUS_ACTIVE) == 0) {
        if ((EhciDebugFlags & EHCI_DEBUG_TRANSFERS) != 0) {
            RtlDebugPrint("EHCI: Transfer (0x%08x) PA 0x%I64x completed with "
                          "token 0x%08x\n",
                          Transfer,
                          Transfer->PhysicalAddress,
                          HardwareStatus);
        }

        LIST_REMOVE(&(Transfer->EndpointListEntry));
        Transfer->EndpointListEntry.Next = NULL;
        LIST_REMOVE(&(Transfer->GlobalListEntry));
        Transfer->GlobalListEntry.Next = NULL;
        LengthTransferred = Transfer->TransferLength -
                            ((HardwareStatus &
                              EHCI_TRANSFER_TOTAL_BYTES_MASK) >>
                             EHCI_TRANSFER_TOTAL_BYTES_SHIFT);

        UsbTransfer = &(Transfer->Set->UsbTransfer->Public);
        UsbTransfer->LengthTransferred += LengthTransferred;

        //
        // If error bits were set, it's curtains for this transfer. Figure out
        // exactly what went wrong. A halted error is first in line even if
        // another bit (e.g. Babble) is set, because the driver may want to
        // clear the halted state.
        //

        if ((HardwareStatus & EHCI_TRANSFER_ERROR_MASK) != 0) {
            if (((EhciDebugFlags & EHCI_DEBUG_ERRORS) != 0) &&
                ((EhciDebugFlags & EHCI_DEBUG_TRANSFERS) == 0)) {

                RtlDebugPrint("EHCI: Transfer (0x%08x) PA 0x%I64x completed "
                              "with token 0x%08x\n",
                              Transfer,
                              Transfer->PhysicalAddress,
                              HardwareStatus);
            }

            RemoveSet = TRUE;
            UsbTransfer->Status = STATUS_DEVICE_IO_ERROR;
            if ((HardwareStatus & EHCI_TRANSFER_STATUS_HALTED) != 0) {
                UsbTransfer->Error = UsbErrorTransferStalled;

                //
                // Clear out the current link so that when the next transfer
                // set is linked in it won't get confused if this transfer
                // is reused.
                //

                QueueHead = Transfer->Set->Endpoint->Queue.HardwareQueueHead;
                QueueHead->CurrentTransferDescriptorLink = 0;

            } else if ((HardwareStatus &
                        EHCI_TRANSFER_MISSED_MICRO_FRAME_ERROR) != 0) {

                UsbTransfer->Error = UsbErrorTransferMissedMicroFrame;

            } else if ((HardwareStatus &
                        EHCI_TRANSFER_TRANSACTION_ERROR) != 0) {

                UsbTransfer->Error = UsbErrorTransferCrcOrTimeoutError;

            } else if ((HardwareStatus & EHCI_TRANSFER_BABBLE_ERROR) != 0) {
                UsbTransfer->Error = UsbErrorTransferBabbleDetected;

            } else if ((HardwareStatus &
                        EHCI_TRANSFER_STATUS_DATA_BUFFER_ERROR) != 0) {

                UsbTransfer->Error = UsbErrorTransferDataBuffer;
            }

        //
        // Also check for short packets.
        //

        } else if ((LengthTransferred != Transfer->TransferLength) &&
                   ((UsbTransfer->Flags &
                     USB_TRANSFER_FLAG_NO_SHORT_TRANSFERS) != 0)) {

            UsbTransfer->Status = STATUS_DATA_LENGTH_MISMATCH;
            UsbTransfer->Error = UsbErrorShortPacket;
        }

        //
        // If this is the last transfer, then signal that processing on this
        // set is complete.
        //

        if ((Transfer->LastTransfer != FALSE) ||
            (LengthTransferred != Transfer->TransferLength)) {

            RemoveSet = TRUE;
        }
    }

ProcessPotentiallyCompletedTransferEnd:
    return RemoveSet;
}

VOID
EhcipRemoveCompletedTransferSet (
    PEHCI_CONTROLLER Controller,
    PEHCI_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine removes a completed transfer set from the schedule. This
    routine assumes that the controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller being operated on.

    TransferSet - Supplies a pointer to the set of transfers to remove.

Return Value:

    None.

--*/

{

    ULONG BackwardsTransferIndex;
    PEHCI_TRANSFER EhciTransfer;
    PEHCI_ENDPOINT Endpoint;
    PEHCI_TRANSFER *TransferArray;
    ULONG TransferIndex;

    Endpoint = TransferSet->Endpoint;

    //
    // Isochronous transfers are handled differently.
    //

    if (Endpoint->TransferType == UsbTransferTypeIsochronous) {

        ASSERT(FALSE);

        goto RemoveCompletedTransferSetEnd;
    }

    TransferArray = (PEHCI_TRANSFER *)&(TransferSet->Transfer);
    for (TransferIndex = 0;
         TransferIndex < TransferSet->TransferCount;
         TransferIndex += 1) {

        BackwardsTransferIndex = TransferSet->TransferCount - 1 - TransferIndex;
        EhciTransfer = TransferArray[BackwardsTransferIndex];

        //
        // Skip this transfer if it's done or otherwise not currently queued.
        //

        if (EhciTransfer->EndpointListEntry.Next == NULL) {
            continue;
        }

        //
        // Since the transfer set completed, all of the transfers are already
        // out of the hardware's queue. Just remove them from the software
        // list.
        //

        LIST_REMOVE(&(EhciTransfer->EndpointListEntry));
        EhciTransfer->EndpointListEntry.Next = NULL;

        ASSERT(EhciTransfer->GlobalListEntry.Next != NULL);

        LIST_REMOVE(&(EhciTransfer->GlobalListEntry));
        EhciTransfer->GlobalListEntry.Next = NULL;
    }

RemoveCompletedTransferSetEnd:

    //
    // Transfer set has been removed. Mark that it is no longer queued.
    //

    TransferSet->Flags &= ~EHCI_TRANSFER_SET_FLAG_QUEUED;
    return;
}

VOID
EhcipProcessAsyncOnAdvanceInterrupt (
    PEHCI_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine processes the queue heads that were waiting for an advance of
    the asynchronous schedule. This routine assumes that the controller lock is
    held.

Arguments:

    Controller - Supplies a pointer to the controller being operated on.

Return Value:

    None.

--*/

{

    ULONG CommandRegister;
    PLIST_ENTRY CurrentEntry;
    PEHCI_TRANSFER EhciTransfer;
    PEHCI_ENDPOINT Endpoint;
    ULONG Flags;
    ULONG HorizontalLink;
    PEHCI_TRANSFER LastTransfer;
    PEHCI_TRANSFER_QUEUE Queue;
    PEHCI_TRANSFER_QUEUE QueueBefore;
    LIST_ENTRY QueueListHead;
    BOOL QueueWorkItem;
    PEHCI_TRANSFER *TransferArray;
    ULONG TransferIndex;
    PEHCI_TRANSFER_SET TransferSet;
    PUSB_TRANSFER UsbTransfer;

    ASSERT(KeIsSpinLockHeld(&(Controller->Lock)) != FALSE);

    //
    // First transfer the list of queue heads that are ready to be processed to
    // a local list. Be a bit defensive against spurious async advance
    // interrupts (potentially caused by KD USB).
    //

    if (LIST_EMPTY(&(Controller->AsyncOnAdvanceReadyListHead)) != FALSE) {
        INITIALIZE_LIST_HEAD(&QueueListHead);

    } else {
        MOVE_LIST(&(Controller->AsyncOnAdvanceReadyListHead), &QueueListHead);
    }

    INITIALIZE_LIST_HEAD(&(Controller->AsyncOnAdvanceReadyListHead));

    //
    // If the pending list is not empty, transfer it to the ready list and ring
    // the doorbell.
    //

    if (LIST_EMPTY(&(Controller->AsyncOnAdvancePendingListHead)) == FALSE) {
        MOVE_LIST(&(Controller->AsyncOnAdvancePendingListHead),
                  &(Controller->AsyncOnAdvanceReadyListHead));

        INITIALIZE_LIST_HEAD(&(Controller->AsyncOnAdvancePendingListHead));
        CommandRegister = Controller->CommandRegister;
        CommandRegister |= EHCI_COMMAND_INTERRUPT_ON_ASYNC_ADVANCE;
        EHCI_WRITE_REGISTER(Controller,
                            EhciRegisterUsbCommand,
                            CommandRegister);
    }

    //
    // Now that the next doorbell is all set up, process the list of queue
    // heads that have been fully removed from the hardware's grasp. There are
    // two reasons for which a queue head can be removed. The first is if the
    // endpoint is being removed. The second is if a transfer set in the queue
    // was cancelled.
    //

    QueueWorkItem = FALSE;
    while (LIST_EMPTY(&QueueListHead) == FALSE) {
        Queue = LIST_VALUE(QueueListHead.Next, EHCI_TRANSFER_QUEUE, ListEntry);
        LIST_REMOVE(&(Queue->ListEntry));

        //
        // If the queue has no async on advance context, then it's on the list
        // in order to be destroyed. Add it to the list of queue heads to
        // destroy. Note that a work item needs to be scheduled if this is the
        // first entry on the list.
        //

        if (Queue->AsyncOnAdvanceCancel == FALSE) {
            if (LIST_EMPTY(&(Controller->QueuesToDestroyListHead)) != FALSE) {
                QueueWorkItem = TRUE;
            }

            INSERT_BEFORE(&(Queue->ListEntry),
                          &(Controller->QueuesToDestroyListHead));

        //
        // Otherwise the queue is here to remove one or more transfer sets that
        // were cancelled.
        //

        } else {
            Endpoint = PARENT_STRUCTURE(Queue, EHCI_ENDPOINT, Queue);
            Queue->AsyncOnAdvanceCancel = FALSE;

            ASSERT((Endpoint->TransferType == UsbTransferTypeControl) ||
                   (Endpoint->TransferType == UsbTransferTypeBulk));

            CurrentEntry = Endpoint->TransferListHead.Next;
            while (CurrentEntry != &(Endpoint->TransferListHead)) {
                EhciTransfer = LIST_VALUE(CurrentEntry,
                                          EHCI_TRANSFER,
                                          EndpointListEntry);

                //
                // If the transfer set was not marked for cancelling, skip it.
                //

                TransferSet = EhciTransfer->Set;
                Flags = TransferSet->Flags;

                ASSERT((Flags & EHCI_TRANSFER_SET_FLAG_QUEUED) != 0);

                if ((Flags & EHCI_TRANSFER_SET_FLAG_CANCELLING) == 0) {
                    CurrentEntry = CurrentEntry->Next;
                    continue;
                }

                //
                // The next transfer to process is the next transfer after this
                // set.
                //

                CurrentEntry = NULL;
                TransferArray = (PEHCI_TRANSFER *)&(TransferSet->Transfer);
                for (TransferIndex = 0;
                     TransferIndex < TransferSet->TransferCount;
                     TransferIndex += 1) {

                    if (TransferArray[TransferIndex]->LastTransfer != FALSE) {
                        LastTransfer = TransferArray[TransferIndex];
                        CurrentEntry = LastTransfer->EndpointListEntry.Next;
                        break;
                    }
                }

                ASSERT(CurrentEntry != NULL);

                //
                // Officially mark the transfer as cancelled, remove the
                // transfer set and call the completion routine.
                //

                UsbTransfer = &(TransferSet->UsbTransfer->Public);
                UsbTransfer->Status = STATUS_OPERATION_CANCELLED;
                UsbTransfer->Error = UsbErrorTransferCancelled;
                EhcipRemoveCancelledTransferSet(Controller, TransferSet);
                UsbHostProcessCompletedTransfer(TransferSet->UsbTransfer);
            }

            //
            // Now that all of the queue's cancelled transfer sets have been
            // processed add it back to the asynchronous schedule.
            //

            QueueBefore = &(Controller->AsynchronousSchedule);
            INSERT_AFTER(&(Queue->ListEntry), &(QueueBefore->ListEntry));
            Queue->HardwareQueueHead->HorizontalLink =
                                QueueBefore->HardwareQueueHead->HorizontalLink;

            HorizontalLink = (ULONG)Queue->PhysicalAddress;

            ASSERT((HorizontalLink & (~EHCI_LINK_ADDRESS_MASK)) == 0);

            HorizontalLink |= EHCI_LINK_TYPE_QUEUE_HEAD;
            HlWriteRegister32(&(QueueBefore->HardwareQueueHead->HorizontalLink),
                              HorizontalLink);
        }
    }

    //
    // Queue the work item now if there is work to do.
    //

    if (QueueWorkItem != FALSE) {
        KeQueueWorkItem(Controller->DestroyQueuesWorkItem);
    }

    return;
}

VOID
EhcipRemoveCancelledTransferSet (
    PEHCI_CONTROLLER Controller,
    PEHCI_TRANSFER_SET TransferSet
    )

/*++

Routine Description:

    This routine removes a cancelled transfer set from the schedule. This
    routine assumes that the controller lock is already held.

Arguments:

    Controller - Supplies a pointer to the controller being operated on.

    TransferSet - Supplies a pointer to the set of transfers to remove.

Return Value:

    None.

--*/

{

    ULONG BackwardsTransferIndex;
    PEHCI_TRANSFER EhciTransfer;
    PEHCI_ENDPOINT Endpoint;
    PEHCI_QUEUE_HEAD HardwareQueueHead;
    PEHCI_TRANSFER_DESCRIPTOR HardwareTransfer;
    PLIST_ENTRY NextEntry;
    PEHCI_TRANSFER NextTransfer;
    PLIST_ENTRY PreviousEntry;
    PEHCI_TRANSFER PreviousTransfer;
    PEHCI_TRANSFER_QUEUE Queue;
    PEHCI_TRANSFER *TransferArray;
    ULONG TransferIndex;

    ASSERT(KeIsSpinLockHeld(&(Controller->Lock)) != FALSE);

    Endpoint = TransferSet->Endpoint;
    Queue = &(Endpoint->Queue);
    TransferArray = (PEHCI_TRANSFER *)&(TransferSet->Transfer);

    //
    // Isochronous transfers are handled differently.
    //

    if (Endpoint->TransferType == UsbTransferTypeIsochronous) {

        ASSERT(FALSE);

        goto RemoveCancelledTransferSetEnd;
    }

    //
    // Loop over all transfers in the set, removing any incomplete transfers.
    //

    NextEntry = NULL;
    PreviousEntry = NULL;
    for (TransferIndex = 0;
         TransferIndex < TransferSet->TransferCount;
         TransferIndex += 1) {

        BackwardsTransferIndex = TransferSet->TransferCount - 1 - TransferIndex;
        EhciTransfer = TransferArray[BackwardsTransferIndex];

        //
        // Skip this transfer if it's done or otherwise not currently queued.
        //

        if (EhciTransfer->EndpointListEntry.Next == NULL) {
            continue;
        }

        //
        // Record the transfer directly following the set. This is the next
        // entry of the last transfer in the set.
        //

        if (EhciTransfer->LastTransfer != FALSE) {
            NextEntry = EhciTransfer->EndpointListEntry.Next;
        }

        //
        // Record the previous entry of the first transfer in the set that is
        // still queued. This loop iterates backwards, so just record it every
        // time.
        //

        PreviousEntry = EhciTransfer->EndpointListEntry.Previous;

        //
        // Either the previous entry is valid or this transfer was previously
        // the first transfer in the queue.
        //

        ASSERT((PreviousEntry != &(Endpoint->TransferListHead)) ||
               (Queue->HardwareQueueHead->CurrentTransferDescriptorLink == 0) ||
               (Queue->HardwareQueueHead->CurrentTransferDescriptorLink ==
                EhciTransfer->PhysicalAddress));

        //
        // Remove the transfer from the software lists. The endpoint's queue
        // head is not in the schedule so the hardware transfer does not need
        // to be modified - that is handled below.
        //

        LIST_REMOVE(&(EhciTransfer->EndpointListEntry));
        EhciTransfer->EndpointListEntry.Next = NULL;

        ASSERT(EhciTransfer->GlobalListEntry.Next != NULL);

        LIST_REMOVE(&(EhciTransfer->GlobalListEntry));
        EhciTransfer->GlobalListEntry.Next = NULL;
    }

    //
    // Determine the next transfer in the queue after the set being removed.
    // It could be the dummy transfer.
    //

    if ((NextEntry == NULL) || (NextEntry == &(Endpoint->TransferListHead))) {
        NextTransfer = Queue->DummyTransfer;

    } else {
        NextTransfer = LIST_VALUE(NextEntry, EHCI_TRANSFER, EndpointListEntry);
    }

    //
    // If there was a previous transfer in the queue, then point that at the
    // next transfer.
    //

    if ((PreviousEntry != NULL) &&
        (PreviousEntry != &(Endpoint->TransferListHead))) {

        PreviousTransfer = LIST_VALUE(PreviousEntry,
                                      EHCI_TRANSFER,
                                      EndpointListEntry);

        HardwareTransfer = PreviousTransfer->HardwareTransfer;
        HlWriteRegister32(&(HardwareTransfer->NextTransfer),
                          (ULONG)(NextTransfer->PhysicalAddress));

        HlWriteRegister32(&(HardwareTransfer->AlternateNextTransfer),
                          (ULONG)(NextTransfer->PhysicalAddress));

    //
    // Otherwise the queue head needs to be updated to grab the next transfer
    // the next time is runs in the schedule.
    //

    } else {
        HardwareQueueHead = Queue->HardwareQueueHead;
        HardwareQueueHead->CurrentTransferDescriptorLink = 0;
        HardwareQueueHead->TransferOverlay.NextTransfer =
                                        (ULONG)(NextTransfer->PhysicalAddress);

        HardwareQueueHead->TransferOverlay.AlternateNextTransfer =
                                        (ULONG)(NextTransfer->PhysicalAddress);
    }

RemoveCancelledTransferSetEnd:

    //
    // Transfer set has been removed. Mark that it is no longer queued.
    //

    TransferSet->Flags &= ~EHCI_TRANSFER_SET_FLAG_QUEUED;
    return;
}

VOID
EhcipDestroyQueuesWorkRoutine (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the queue head destruction work routine.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item. The EHCI controller context is supplied in this case.

Return Value:

    None.

--*/

{

    PEHCI_CONTROLLER Controller;
    PEHCI_ENDPOINT Endpoint;
    RUNLEVEL OldRunLevel;
    PEHCI_TRANSFER_QUEUE Queue;
    LIST_ENTRY QueueListHead;

    Controller = (PEHCI_CONTROLLER)Parameter;

    ASSERT(LIST_EMPTY(&(Controller->QueuesToDestroyListHead)) == FALSE);

    //
    // Acquire the controller lock and move all the queue heads that are
    // awaiting removal to a local list.
    //

    OldRunLevel = EhcipAcquireControllerLock(Controller);
    MOVE_LIST(&(Controller->QueuesToDestroyListHead), &QueueListHead);
    INITIALIZE_LIST_HEAD(&(Controller->QueuesToDestroyListHead));
    EhcipReleaseControllerLock(Controller, OldRunLevel);

    //
    // Iterate over the local list, destroying each queue head.
    //

    while (LIST_EMPTY(&QueueListHead) == FALSE) {
        Queue = LIST_VALUE(QueueListHead.Next, EHCI_TRANSFER_QUEUE, ListEntry);
        Endpoint = PARENT_STRUCTURE(Queue, EHCI_ENDPOINT, Queue);
        LIST_REMOVE(&(Queue->ListEntry));
        if (Queue->DummyTransfer != NULL) {
            if (Queue->DummyTransfer->HardwareTransfer != NULL) {
                MmFreeBlock(Controller->BlockAllocator,
                            Queue->DummyTransfer->HardwareTransfer);
            }

            MmFreeNonPagedPool(Queue->DummyTransfer);
        }

        if (Queue->HardwareQueueHead != NULL) {
            MmFreeBlock(Controller->BlockAllocator, Queue->HardwareQueueHead);
        }

        MmFreeNonPagedPool(Endpoint);
    }

    return;
}

