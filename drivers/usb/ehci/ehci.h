/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ehci.h

Abstract:

    This header contains internal definitions for the EHCI USB Host Controller
    driver.

Author:

    Evan Green 18-Mar-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ehcihw.h"
#include "ehcidbg.h"
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/kdusb.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the EHCI allocation tag.
//

#define EHCI_ALLOCATION_TAG 0x69636845 // 'ichE'
#define EHCI_BLOCK_ALLOCATION_TAG 0x6C426845 // 'lBhE'

//
// Define the block expansion count for the EHCI transfer and queue block
// allocator. This is defined in number of blocks.
//

#define EHCI_BLOCK_ALLOCATOR_EXPANSION_COUNT 40

//
// Define the required alignment for EHCI transfers and queues.
//

#define EHCI_BLOCK_ALLOCATOR_ALIGNMENT EHCI_LINK_ALIGNMENT

//
// Define the number of levels in the periodic schedule tree.
//

#define EHCI_PERIODIC_SCHEDULE_TREE_DEPTH 8

//
// Define the set of flags for the EHCI transfer set.
//

#define EHCI_TRANSFER_SET_FLAG_QUEUED     0x00000001
#define EHCI_TRANSFER_SET_FLAG_CANCELLING 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EHCI_TRANSFER_SET EHCI_TRANSFER_SET, *PEHCI_TRANSFER_SET;

/*++

Structure Description:

    This structure stores information about an EHCI transfer.

Members:

    GlobalListEntry - Stores pointers to the next and previous transfers in the
        global list of in-flight transfers.

    EndpointListEntry - Stores pointers to the next and previous transfers in
        the endpoint queue.

    Set - Stores a pointer to the transfer set that this transfer is a part of.

    HardwareTransfer - Stores a pointer to the hardware defined transfer
        descriptor.

    PhysicalAddress - Stores the physical address of the hardware transfer
        descriptor.

    TransferLength - Stores the length of the transfer (as the hardware
        decrements the field towards 0).

    LastTransfer - Stores a boolean indicating whether this is the last
        transfer submitted in the set.

--*/

typedef struct _EHCI_TRANSFER {
    LIST_ENTRY GlobalListEntry;
    LIST_ENTRY EndpointListEntry;
    PEHCI_TRANSFER_SET Set;
    PEHCI_TRANSFER_DESCRIPTOR HardwareTransfer;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG TransferLength;
    BOOL LastTransfer;
} EHCI_TRANSFER, *PEHCI_TRANSFER;

/*++

Structure Description:

    This structure stores information about an EHCI transfer queue.

Members:

    ListEntry - Stores pointers to the next and previous transfer queue
        attached to the EHCI controller.

    DummyTransfer - Stores a pointer to the current inactive dummy transfer
        that gets left on every queue. This dummy transfer is needed so that
        additional sets of transfers can be added to a queue without race
        conditions. The dummy transfer rotates around as sets of transfers are
        added, as the first transfer in that set becomes the new dummy.

    HardwareQueueHead - Stores a pointer to the hardware defined transfer queue
        head. This is unused for isochronous transfers.

    PhysicalAddress - Stores the physical address of the hardware queue head.

    AsyncOnAdvanceCancel - Stores a boolean indicating whether or not the queue
        is being processed by the "async on advance" interrupt due to
        cancellation (TRUE) or destruction (FALSE).

--*/

typedef struct _EHCI_TRANSFER_QUEUE {
    LIST_ENTRY ListEntry;
    PEHCI_TRANSFER DummyTransfer;
    PEHCI_QUEUE_HEAD HardwareQueueHead;
    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL AsyncOnAdvanceCancel;
} EHCI_TRANSFER_QUEUE, *PEHCI_TRANSFER_QUEUE;

/*++

Structure Description:

    This structure stores information about an EHCI endpoint.

Members:

    ListEntry - Stores pointers to the next and previous endpoints attached to
        the EHCI controller.

    TransferListHead - Stores the head of the list of transfers on this queue.

    Queue - Stores the EHCI transfer queue for this endpoint.

    TransferType - Stores the transfer type of the endpoint.

    Speed - Stores the speed of the device exposing the endpoint.

    MaxPacketSize - Stores the maximum number of bytes that can be moved in a
        packet for this endpoint.

    PollRate - Stores the interrupt poll rate, in (micro)frames.

    EndpointNumber - Stores the endpoint number, as defined by the USB device.

--*/

typedef struct _EHCI_ENDPOINT {
    LIST_ENTRY ListEntry;
    LIST_ENTRY TransferListHead;
    EHCI_TRANSFER_QUEUE Queue;
    USB_TRANSFER_TYPE TransferType;
    USB_DEVICE_SPEED Speed;
    ULONG MaxPacketSize;
    ULONG PollRate;
    UCHAR EndpointNumber;
} EHCI_ENDPOINT, *PEHCI_ENDPOINT;

/*++

Structure Description:

    This structure stores a collection of EHCI transfers that together
    comprise a USB transfer.

Members:

    ListEntry - Stores pointers to the next and previous transfer sets in
        whatever list of transfer sets this set is in (usually the set of
        lists waiting to be processed once the async advance doorbel is rung).

    TransferCount - Stores the number of elements in the transfer array.

    Flags - Stores a bitmask of flags for the transfer set. See
        EHCI_TRANSFER_SET_FLAG_* for definitions.

    UsbTransfer - Stores a pointer to the transfer as defined by the USB core
        library. Several EHCI transfers may constitute and point to a single USB
        transfer.

    Endpoint - Stores a pointer to the endpoint that owns this set of transfers.

    Transfer - Stores an array of pointers to EHCI transfers.

--*/

struct _EHCI_TRANSFER_SET {
    LIST_ENTRY ListEntry;
    ULONG TransferCount;
    ULONG Flags;
    PUSB_TRANSFER_INTERNAL UsbTransfer;
    PEHCI_ENDPOINT Endpoint;
    PEHCI_TRANSFER Transfer[ANYSIZE_ARRAY];
};

/*++

Structure Description:

    This structure stores USB state for an EHCI controller.

Members:

    RegisterBase - Stores the virtual address where the Operational Registers
        are mapped.

    PhysicalBase - Stores the physical address of the base of the registers
        (the base of EHCI itself, not the operational registers).

    PeriodicSchedule - Stores a pointer to the frame list schedule used by the
        controller.

    PeriodicScheduleIoBuffer - Stores a pointer to the I/O buffer containing the
        periodic schedule.

    AsynchronousSchedule - Stores the empty transfer queue that represents the
        head of the asynchronous schedule.

    IsochronousTransferListHead - Stores the list head of all active
        Isochronous transfers in the schedule.

    BlockAllocator - Stores a pointer to the block allocator used to
        allocate all queues and transfers.

    TransferListHead - Stores the global list of active transfers on the
        schedule.

    AsyncOnAdvanceReadyListHead - Stores the list of transfer queues that have
        been removed from the asynchronous schedule and that the host
        controller can no longer reach.

    AsyncOnAdvancePendingListHead - Stores the list of transfer queues that
        have been removed from the asynchronous schedule, but that the host
        controller may still be able to reach.

    QueuesToDestroyListHead - Stores the list of transfer queues that need to
        be destroyed by the destroy queues work item.

    Lock - Stores the lock that protects access to all list entries under this
        controller. It must be a spin lock because it synchronizes with a DPC,
        which cannot block.

    UsbCoreHandle - Stores the handle returned by the USB core that identifies
        this controller.

    CommandRegister - Stores the current state of the USB command register
        (to avoid unnecessary reads).

    PendingStatusBits - Stores the bits in the USB status register that have
        not yet been addressed by the DPC.

    InterruptHandle - Stores the interrupt handle of the connected interrupt.

    InterruptTree - Stores an array of empty transfer queues at each level of
        the periodic schedule tree. Think of each frame in the schedule as a
        leaf node, and each leaf node points at a more common entry, which
        points at an even more common entry, etc. The last entry in this array
        is polled every millisecond, the next last one every 2ms, then every
        4ms, etc.

    DestroyQueuesWorkItem - Stores a pointer to a work item that destroys queue
        heads that have been completely removed from the schedule.

    PortCount - Stores the number of ports on the EHCI host controller.

    EndpointCount - Stores the number of active endpoints in the controller.

    EndpointListHead - Stores the head of the list of endpoints.

    HandoffData - Stores an optional pointer to the kernel debugger handoff
        data.

--*/

typedef struct _EHCI_CONTROLLER {
    PVOID RegisterBase;
    PHYSICAL_ADDRESS PhysicalBase;
    PEHCI_PERIODIC_SCHEDULE PeriodicSchedule;
    PIO_BUFFER PeriodicScheduleIoBuffer;
    EHCI_TRANSFER_QUEUE AsynchronousSchedule;
    LIST_ENTRY IsochronousTransferListHead;
    PBLOCK_ALLOCATOR BlockAllocator;
    LIST_ENTRY TransferListHead;
    LIST_ENTRY AsyncOnAdvanceReadyListHead;
    LIST_ENTRY AsyncOnAdvancePendingListHead;
    LIST_ENTRY QueuesToDestroyListHead;
    KSPIN_LOCK Lock;
    HANDLE UsbCoreHandle;
    ULONG CommandRegister;
    volatile ULONG PendingStatusBits;
    HANDLE InterruptHandle;
    EHCI_TRANSFER_QUEUE InterruptTree[EHCI_PERIODIC_SCHEDULE_TREE_DEPTH];
    PWORK_ITEM DestroyQueuesWorkItem;
    ULONG PortCount;
    ULONG EndpointCount;
    LIST_ENTRY EndpointListHead;
    PEHCI_DEBUG_HANDOFF_DATA HandoffData;
} EHCI_CONTROLLER, *PEHCI_CONTROLLER;

//
// -------------------------------------------------------------------- Globals
//

extern PDRIVER EhciDriver;

//
// -------------------------------------------------------- Function Prototypes
//

PEHCI_CONTROLLER
EhcipInitializeControllerState (
    PVOID OperationalRegisterBase,
    PHYSICAL_ADDRESS RegisterBasePhysical,
    ULONG PortCount,
    PDEBUG_USB_HANDOFF_DATA HandoffData
    );

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

VOID
EhcipDestroyControllerState (
    PEHCI_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys the memory associated with an EHCI controller.

Arguments:

    Controller - Supplies a pointer to the EHCI controller state to release.

Return Value:

    None.

--*/

KSTATUS
EhcipRegisterController (
    PEHCI_CONTROLLER Controller,
    PDEVICE Device
    );

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

VOID
EhcipSetInterruptHandle (
    PEHCI_CONTROLLER Controller,
    HANDLE InterruptHandle
    );

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

KSTATUS
EhcipResetController (
    PEHCI_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine resets and starts the EHCI controller.

Arguments:

    Controller - Supplies a pointer to the EHCI controller state of the
        controller to reset.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
EhcipInterruptService (
    PVOID Context
    );

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

INTERRUPT_STATUS
EhcipInterruptServiceDpc (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine implements the EHCI dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the EHCI controller
        structure.

Return Value:

    None.

--*/

