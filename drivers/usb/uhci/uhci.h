/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uhci.h

Abstract:

    This header contains definitions for the UHCI Host controller driver.

Author:

    Evan Green 13-Jan-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "uhcihw.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the UHCI allocation tag.
//

#define UHCI_ALLOCATION_TAG 0x69636855 // 'ichU'
#define UHCI_BLOCK_ALLOCATION_TAG 0x6C426855 // 'lBhU'

//
// Define the number of blocks by which to expand the UHCI transfer and queue
// block allocator.
//

#define UHCI_BLOCK_ALLOCATOR_EXPANSION_COUNT 40

//
// Define the required alignment for the UHCI block allocator.
//

#define UHCI_BLOCK_ALLOCATOR_ALIGNMENT 16

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a UHCI endpoint.

Members:

    TransferType - Stores the transfer type of the endpoint.

    Speed - Stores the speed of the device exposing the endpoint.

    MaxPacketSize - Stores the maximum number of bytes that can be moved in a
        packet for this endpoint.

    EndpointNumber - Stores the endpoint number, as defined by the USB device.

    DataToggle - Stores whether or not to set the data toggle bit on the next
        packet to fly through this queue.

    QueueListHead - Stores the head of the list of queues representing
        transfers for this endpoint.

--*/

typedef struct _UHCI_ENDPOINT {
    USB_TRANSFER_TYPE TransferType;
    USB_DEVICE_SPEED Speed;
    ULONG MaxPacketSize;
    UCHAR EndpointNumber;
    UCHAR DataToggle;
    LIST_ENTRY QueueListHead;
} UHCI_ENDPOINT, *PUHCI_ENDPOINT;

/*++

Structure Description:

    This structure stores information about a UHCI transfer. Pointers to this
    structure must be 16-byte aligned as required by the UHCI specification.

Members:

    HardwareTransfer - Stores the hardware defined transfer descriptor.

    QueueListEntry - Stores pointers to the next and previous transfers in the
        endpoint queue.

    PhysicalAddress - Stores the physical address of this structure.

--*/

typedef struct _UHCI_TRANSFER {
    UHCI_TRANSFER_DESCRIPTOR HardwareTransfer;
    LIST_ENTRY QueueListEntry;
    PHYSICAL_ADDRESS PhysicalAddress;
} UHCI_TRANSFER, *PUHCI_TRANSFER;

/*++

Structure Description:

    This structure stores information about a UHCI transfer queue. Pointers to
    this structure must be 16-byte aligned as required by the UHCI
    specification.

Members:

    HardwareQueueHead - Stores the hardware defined transfer queue head. This
        is unused for isochronous transfers.

    GlobalListEntry - Stores pointers to the next and previous queues in the
        global schedule.

    EndpointListEntry - Stores pointers to the next and previous active queues
        in the endpoint.

    ListEntry - Stores pointers to the next and previous queues in the schedule.
        For queues that represent transfer sets these are list entries in the
        endpoint queue list. For controller dummy queues these list entries in
        the controller queue list head.

    TransferListHead - Stores the head of the list of transfers on this queue.

    PhysicalAddress - Stores the physical address of this structure.

    UsbTransfer - Stores a pointer to the transfer as defined by the USB core
        library.

    LinkToLastTransfer - Stores the link pointer value that points to the last
        transfer. This is used for IN control transfers that end early due to
        short packets, and need to be shortcutted to the last transfer to send
        the status phase.

    LastTransfer - Stores a pointer to the last request for this particular
        USB transfer submission. There may be more transfer descriptors on the
        list because the USB transfer was allocated with a larger maximum
        data amount than was submitted. This points to the last transfer
        descriptor for the amount actually submitted.

    Endpoint - Stores a pointer to the endpoint this transfer queue works for.

--*/

typedef struct _UHCI_TRANSFER_QUEUE {
    UHCI_QUEUE_HEAD HardwareQueueHead;
    LIST_ENTRY GlobalListEntry;
    LIST_ENTRY EndpointListEntry;
    LIST_ENTRY TransferListHead;
    PHYSICAL_ADDRESS PhysicalAddress;
    PUSB_TRANSFER_INTERNAL UsbTransfer;
    ULONG LinkToLastTransfer;
    PUHCI_TRANSFER LastTransfer;
    PUHCI_ENDPOINT Endpoint;
} UHCI_TRANSFER_QUEUE, *PUHCI_TRANSFER_QUEUE;

/*++

Structure Description:

    This structure stores USB state for a UHCI controller.

Members:

    IoPortBase - Stores the base I/O port where the controller registers are
        located.

    Schedule - Stores a pointer to the frame list schedule used by the
        controller.

    ScheduleIoBuffer - Stores a pointer to the I/O buffer containing the
        schedule.

    InterruptQueue - Stores a pointer to the Queue Head used to contain all
        of the interrupt transfers.

    ControlQueue - Stores a pointer to the Queue Head used to mark the beginning
        of all control transfers.

    QueueListHead - Stores the list head of queues in the schedule.

    IsochronousTransferListHead - Stores the list head of all active
        Isochronous transfers in the schedule.

    BlockAllocator - Stores a pointer to the block allocator used to allocate
        all transfers and queues.

    Lock - Stores the lock that protects access to all list entries under this
        controller. It must be a spin lock because it synchronizes with a DPC,
        which cannot block.

    UsbCoreHandle - Stores the handle returned by the USB core that identifies
        this controller.

    PendingStatusBits - Stores the bits in the USB status register that have
        not yet been addressed by the DPC.

    InterruptHandle - Stores the interrupt handle of the connected interrupt.

    PortStatusTimer - Stores a pointer to a timer that periodically fires to
        check the host controller's port status.

    PortStatusDpc - Stores a pointer to the DPC queued by the port status
        change timer.

--*/

typedef struct _UHCI_CONTROLLER {
    USHORT IoPortBase;
    PUHCI_SCHEDULE Schedule;
    PIO_BUFFER ScheduleIoBuffer;
    PUHCI_TRANSFER_QUEUE InterruptQueue;
    PUHCI_TRANSFER_QUEUE ControlQueue;
    LIST_ENTRY QueueListHead;
    LIST_ENTRY IsochronousTransferListHead;
    PBLOCK_ALLOCATOR BlockAllocator;
    KSPIN_LOCK Lock;
    HANDLE UsbCoreHandle;
    volatile ULONG PendingStatusBits;
    HANDLE InterruptHandle;
    PKTIMER PortStatusTimer;
    PDPC PortStatusDpc;
} UHCI_CONTROLLER, *PUHCI_CONTROLLER;

/*++

Structure Description:

    This structure stores information about a UHCI root hub device.

Members:

    UsbHandle - Stores the handle returned when the USB core enumerated the
        device.

    Controller - Stores a pointer to the controller enumerating this root hub.

--*/

typedef struct _UHCI_ROOT_HUB {
    HANDLE UsbHandle;
    PUHCI_CONTROLLER Controller;
} UHCI_ROOT_HUB, *PUHCI_ROOT_HUB;

//
// -------------------------------------------------------------------- Globals
//

extern PDRIVER UhciDriver;

//
// -------------------------------------------------------- Function Prototypes
//

PUHCI_CONTROLLER
UhcipInitializeControllerState (
    ULONG IoPortBase
    );

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

VOID
UhcipDestroyControllerState (
    PUHCI_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys the memory associated with a UHCI controller.

Arguments:

    Controller - Supplies a pointer to the UHCI controller state to release.

Return Value:

    None.

--*/

KSTATUS
UhcipRegisterController (
    PUHCI_CONTROLLER Controller,
    PDEVICE Device
    );

/*++

Routine Description:

    This routine registers the started UHCI controller with the core USB
    library.

Arguments:

    Controller - Supplies a pointer to the UHCI controller state of the
        controller to register.

Return Value:

    Status code.

--*/

VOID
UhcipSetInterruptHandle (
    PUHCI_CONTROLLER Controller,
    HANDLE InterruptHandle
    );

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

KSTATUS
UhcipResetController (
    PUHCI_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine resets and starts the UHCI controller.

Arguments:

    Controller - Supplies a pointer to the UHCI controller state of the
        controller to reset.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
UhcipInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the UHCI interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the UHCI device
        context.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
UhcipInterruptServiceDpc (
    PVOID Context
    );

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

KSTATUS
UhcipInitializePortChangeDetection (
    PUHCI_CONTROLLER Controller
    );

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

