/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwhci.h

Abstract:

    This header contains internal definitions for the DesignWare High-Speed
    USB 2.0 On-The-Go (HS OTG) host controller.

Author:

    Chris Stevens 38-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dwhcihw.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the DWHCI allocation tags.
//

#define DWHCI_ALLOCATION_TAG 0x63687744 // 'chwD'
#define DWHCI_BLOCK_ALLOCATION_TAG 0x6C427744 // 'lBwD'

//
// Define the block expansion count for the DWHCI transfer and queue block
// allocator. This is defined in number of blocks.
//

#define DWHCI_BLOCK_ALLOCATOR_EXPANSION_COUNT 40

//
// Define the required alignment for DWHCI transfers and queues.
//

#define DWHCI_BLOCK_ALLOCATOR_ALIGNMENT 1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DWHCI_TRANSFER_SET DWHCI_TRANSFER_SET, *PDWHCI_TRANSFER_SET;
typedef struct _DWHCI_CHANNEL DWHCI_CHANNEL, *PDWHCI_CHANNEL;
typedef struct _DWHCI_ENDPOINT DWHCI_ENDPOINT, *PDWHCI_ENDPOINT;

/*++

Struction Description:

    This structure describes a DWHCI transfer.

Members:

    SetListEntry - Stores pointers to the next and previous transfers in the
        owning transfer set.

    Set - Stores a pointer to the owning transfer set.

    PhysicalAddress - Stores the physical address of data to transfer or the
        physical address of the buffer to receive the data.

    TransferLength - Stores the length of the transfer in bytes.

    Token - Stores the default data to be set in a channel's transfer setup
        register when submitting this transfer.

    InterruptMask - Stores the channel interrupts that should be enabled when
        this transfer is active.

    InTransfer - Stores a boolean indicating whether this is an IN (TRUE) or
        OUT (FALSE) transfer.

    LastTransfer - Stores a boolean indicating whether this is the last
        transfer submitted in the set.

    ErrorCount - Stores the number of errors encountered during the transfer.

    CompleteSplitCount - Stores the current complete split attempt number for
        the transfer. 0 indicates that the start split is in progress.

--*/

typedef struct _DWHCI_TRANSFER {
    LIST_ENTRY SetListEntry;
    PDWHCI_TRANSFER_SET Set;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG TransferLength;
    ULONG Token;
    ULONG InterruptMask;
    UCHAR InTransfer;
    UCHAR LastTransfer;
    UCHAR ErrorCount;
    UCHAR CompleteSplitCount;
} DWHCI_TRANSFER, *PDWHCI_TRANSFER;

/*++

Struction Description:

    This structure describes a DWHCI transfer set.

Members:

    EndpointListEntry - Stores pointers to the next and previous transfer set
        on the endpoint.

    TransferListHead - Stores the head of the list of active/pending transfers
        for this transfer set.

    Endpoint - Stores a pointer to the endpoint to which this transfer set
        belongs.

    UsbTransfer - Stores a pointer to the transfer as defined by the USB core
        library. Several DWHCI transfers may consitute and point to a single
        USB transfer.

    TransferCount - Stores the number of elements in the transfer array.

    Transfer - Stores an array of pointers to DWHCI transfers.

--*/

struct _DWHCI_TRANSFER_SET {
    LIST_ENTRY EndpointListEntry;
    LIST_ENTRY TransferListHead;
    PDWHCI_ENDPOINT Endpoint;
    PUSB_TRANSFER_INTERNAL UsbTransfer;
    ULONG TransferCount;
    PDWHCI_TRANSFER Transfer[ANYSIZE_ARRAY];
};

/*++

Struction Description:

    This structure describes a DWHCI endpoint.

Members:

    ListEntry - Stores pointers to the next and previous endpoints attached to
        the DWHCI controller. The endpoint can be part of one or none of the
        controller's five endpoint lists.

    TransferSetListHead - Stores the head of the list of transfer sets on this
        endpoint.

    Channel - Stores the DWHCI channel currently in use by the endpoint.

    TransferType - Stores the USB transfer type of the endpoint.

    Speed - Stores the speed of the device exposing the endpoint.

    ChannelControl - Stores the default information to program into a channel's
        control register when transmitting a transfer for this endpoint.

    SplitControl - Stores the default information to program into a channel's
        split control register when transmitting a transfer for this endpoint.
        Stores 0 if this is a high speed endpoint.

    MaxPacketSize - Stores the maximum number of bytes that can be moved in a
        packet for this endpoint.

    MaxPacketCount - Stores the maximum number of packets than can be sent in
        a transfer for this endpoint.

    MaxTransferSize - Stores the maximum number of bytes that can be sent in a
        single transfer for this endpoint.

    PollRate - Stores the interrupt poll rate, in (micro)frames. This field is
        only valid for interrupt transfers.

    NextFrame - Stores the next (micro)frame during which this endpoint's next
        transfer should be scheduled.

    StartFrame - Stores the microframe of the start split transaction.

    EndpointNumber - Stores the endpoint number, as defined by the USB device.

    PingRequired - Stores a boolean indicating whether or not the PING protocol
        should be executed on the next transfer.

    DataToggle - Stores whether or not to set the data toggle bit on the next
        packet to fly through this endpoint.

    Scheduled - Stores whether or not a transfer has been scheduled on this
        endpoint. An endpoint may have been assigned a channel, but may not
        have been able to queue a transfer.

--*/

struct _DWHCI_ENDPOINT {
    LIST_ENTRY ListEntry;
    LIST_ENTRY TransferSetListHead;
    PDWHCI_CHANNEL Channel;
    USB_TRANSFER_TYPE TransferType;
    USB_DEVICE_SPEED Speed;
    ULONG ChannelControl;
    ULONG SplitControl;
    ULONG MaxPacketSize;
    ULONG MaxPacketCount;
    ULONG MaxTransferSize;
    USHORT PollRate;
    USHORT NextFrame;
    USHORT StartFrame;
    UCHAR EndpointNumber;
    UCHAR PingRequired;
    UCHAR DataToggle;
    UCHAR Scheduled;
};

/*++

Struction Description:

    This structure describes a DWHCI host controller channel.

Members:

    FreeListEntry - Stores pointers to the next and previous free channels in
        the DWHCI controller.

    ChannelNumber - Stores the index number of the channel.

    PendingInterruptBits - Stores a bitmask of pending interrupts for this
        channel.

    Endpoint - Stores a pointer to the endpoint that is currently submitting
        transfers over this endpoint.

--*/

struct _DWHCI_CHANNEL {
    LIST_ENTRY FreeListEntry;
    ULONG ChannelNumber;
    ULONG PendingInterruptBits;
    PDWHCI_ENDPOINT Endpoint;
};

/*++

Struction Description:

    This structure describes a DWHCI host controller.

Members:

    RegisterBase - Stores the virtual address where the DWHCI control registers
        are mapped.

    UsbCoreHandle - Stores the handle returned by the USB core that identifies
        this controller.

    PeriodicActiveListHead - Stores the list head of all active periodic
        endpoints. This includes all isochronous and interrupt endpoints that
        have been assigned a channel.

    PeriodicInactiveListHead - Stores the list head of all inactive periodic
        endpionts. This includes all isochronous and interrupt endpoints that
        are waiting to be made ready at the correct (micro)frame.

    PeriodicReadyListHead - Stores the list head of all periodic endpoints that
        are ready to be made active.

    NonPeriodicActiveListHead - Stores the list head of all active non-periodic
        endpoints. This includes all bulk and control endpoints that have been
        assigned a channel.

    NonPeriodicReadyListHead - Stores the list head of all ready non-periodic
        endpoints. This includes all blulk and control endpoints that are ready
        to be made active.

    FreeChannelListHead - Store the list head of all host controller channels
        that are free to be allotted to an endpoint.

    BlockAllocator - Stores a pointer to the block allocator used to allocate
         all transfers.

    ControlStatusBuffer - Stores a pointer to an I/O buffer used for control
        transfer status phase DMA.

    Speed - Stores the speed of the DWHCI controller.

    Lock - Stores the lock that protects access to all list entries under this
        controller. It must be a spin lock because it synchronizes with a DPC,
        which cannot block.

    InterruptHandle - Stores the interrupt handle of the connected interrupt.

    InterruptDpc - Stores a pointer to the DPC queued by the ISR.

    PendingInterruptBits - Stores the bits in the DWHCI core interrupt register
        that have not yet been addressed by the DPC.

    InterruptLock - Stores the spin lock synchronizing access to the pending
        status bits.

    ChannelCount - Stores the number of channels on this DWHCI host controller.

    MaxTransferSize - Stores the maximum transfer size allowed on this DWHCI
        host controller.

    MaxPacketCount - Stores the maximum packet count allowed on this DWHCI host
        controller.

    PortCount - Stores the number of ports on the DWHCI host controller.

    Revision - Stores the DWHCI host controller revision.

    PortConnected - Stores a boolean indicating if the host port is connected.

    NextFrame - Stores the frame number for which the next periodic transfer is
        scheduled.

    Channel - Stores an array of DWHCI host controller channels.

--*/

typedef struct _DWHCI_CONTROLLER {
    PVOID RegisterBase;
    HANDLE UsbCoreHandle;
    LIST_ENTRY PeriodicActiveListHead;
    LIST_ENTRY PeriodicInactiveListHead;
    LIST_ENTRY PeriodicReadyListHead;
    LIST_ENTRY NonPeriodicActiveListHead;
    LIST_ENTRY NonPeriodicReadyListHead;
    LIST_ENTRY FreeChannelListHead;
    PBLOCK_ALLOCATOR BlockAllocator;
    PIO_BUFFER ControlStatusBuffer;
    USB_DEVICE_SPEED Speed;
    KSPIN_LOCK Lock;
    HANDLE InterruptHandle;
    PDPC InterruptDpc;
    ULONG PendingInterruptBits;
    KSPIN_LOCK InterruptLock;
    ULONG ChannelCount;
    ULONG MaxTransferSize;
    ULONG MaxPacketCount;
    ULONG PortCount;
    ULONG Revision;
    BOOL PortConnected;
    ULONG NextFrame;
    DWHCI_CHANNEL Channel[ANYSIZE_ARRAY];
} DWHCI_CONTROLLER, *PDWHCI_CONTROLLER;

//
// -------------------------------------------------------------------- Globals
//

extern PDRIVER DwhciDriver;

//
// -------------------------------------------------------- Function Prototypes
//

PDWHCI_CONTROLLER
DwhcipInitializeControllerState (
    PVOID RegisterBase,
    ULONG ChannelCount,
    USB_DEVICE_SPEED Speed,
    ULONG MaxTransferSize,
    ULONG MaxPacketCount,
    ULONG Revision
    );

/*++

Routine Description:

    This routine initializes the state and variables needed to start up a DWHCI
    host controller.

Arguments:

    RegisterBase - Supplies the virtual address of the base of the registers.

    ChannelCount - Supplies the number of host controller channels.

    Speed - Supplies the speed of the DWHCI host controller.

    MaxTransferSize - Supplies the maximum transfer size for the DWHCI host
        controller.

    MaxPacketCount - Supplies the maximum packet count for the DWHCI host
        controller.

    Revision - Supplies the revision of the DWHCI host controller.

Return Value:

    Returns a pointer to the DWHCI controller state object on success.

    NULL on failure.

--*/

VOID
DwhcipDestroyControllerState (
    PDWHCI_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys the memory associated with a DWHCI controller.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state to release.

Return Value:

    None.

--*/

KSTATUS
DwhcipRegisterController (
    PDWHCI_CONTROLLER Controller,
    PDEVICE Device
    );

/*++

Routine Description:

    This routine registers the started DWHCI controller with the core USB
    library.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller to register.

    Device - Supplies a pointer to the device object.

Return Value:

    Status code.

--*/

KSTATUS
DwhcipInitializeController (
    PDWHCI_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine initializes and starts the DWHCI controller.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state of the
        controller to reset.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
DwhcipInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the DWHCI interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the DWHCI
        controller.

Return Value:

    Interrupt status.

--*/

VOID
DwhcipSetInterruptHandle (
    PDWHCI_CONTROLLER Controller,
    HANDLE InterruptHandle
    );

/*++

Routine Description:

    This routine saves the handle of the connected interrupt in the DWHCI
    controller.

Arguments:

    Controller - Supplies a pointer to the DWHCI controller state.

    InterruptHandle - Supplies the connected interrupt handle.

Return Value:

    None.

--*/

