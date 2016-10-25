/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ehcidbg.h

Abstract:

    This header contains definitions for supporting EHCI as a debug host
    controller.

Author:

    Evan Green 17-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EHCI_DEBUG_ALLOCATION_TAG 0x44636845 // 'DchE'

//
// Define the amount of uncached memory the EHCI debug device needs for
// queue heads, transfer descriptors, and transfer data.
//

#define EHCI_MEMORY_ALLOCATION_SIZE 0x1000

//
// Define the amount of time to wait for a synchronous transfer to complete,
// in milliseconds.
//

#define EHCI_SYNCHRONOUS_TIMEOUT (1024 * 5)

//
// Define the maximum number of simultaneous EHCI transfers.
//

#define EHCI_DEBUG_TRANSFER_COUNT 2

//
// Define the alignment for EHCI descriptors in debug mode that ensures
// data structures will never cross 4K boundaries.
//

#define EHCI_DEBUG_LINK_ALIGNMENT 64

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the information required to hand off primary
    control of the debug device to the real EHCI driver.

Members:

    ReclamationQueue - Stores a pointer to the reclamation queue head. The
        debugger places its transfer queue heads after this queue.

    EndQueue - Stores a pointer to an empty unused queue head. The debugger
        places all its queue heads before this queue head, so if EHCI gets
        interrupted anywhere in the queue head removal process the debugger
        won't point new queue heads at the queue head EHCI is trying to remove.

    ReclamationQueuePhysical - Stores the physical address of the reclamation
        queue head.

    EndQueuePhysical - Stores the physical address of the end queue head.

--*/

typedef struct _EHCI_DEBUG_HANDOFF_DATA {
    PEHCI_QUEUE_HEAD ReclamationQueue;
    PEHCI_QUEUE_HEAD EndQueue;
    PHYSICAL_ADDRESS ReclamationQueuePhysical;
    PHYSICAL_ADDRESS EndQueuePhysical;
} EHCI_DEBUG_HANDOFF_DATA, *PEHCI_DEBUG_HANDOFF_DATA;

/*++

Structure Description:

    This structure stores the context for an EHCI debug transport.

Members:

    QueuePhysical - Stores the physical address of the transfer queue
        head.

    BufferPhysical - Stores the physical address of the transfer buffer.

    Queue - Stores the transfer queue head.

    Buffer - Stores the buffer that gets chopped up into transfer descriptors
        and data.

    BufferSize - Stores the size of the transfer buffer in bytes.

    Allocated - Stores a boolean indicating if the transfer buffer is in use.

    CheckIndex - Stores the index of the next transfer to check.

--*/

typedef struct _EHCI_DEBUG_TRANSFER {
    PHYSICAL_ADDRESS QueuePhysical;
    PHYSICAL_ADDRESS BufferPhysical;
    PEHCI_QUEUE_HEAD Queue;
    PVOID Buffer;
    ULONG BufferSize;
    BOOL Allocated;
    ULONG CheckIndex;
} EHCI_DEBUG_TRANSFER, *PEHCI_DEBUG_TRANSFER;

/*++

Structure Description:

    This structure stores the context for an EHCI debug transport.

Members:

    RegisterBase - Stores the virtual address of the EHCI registers.

    OperationalBase - Stores the base of the operational registers.

    PortCount - Stores the number of ports in the controller.

    HandoffComplete - Stores a boolean indicating whether or not the handoff
        to the real driver has occurred.

    Data - Stores the handoff data.

    Transfers - Stores the array of transfers that can be allocated.

--*/

typedef struct _EHCI_DEBUG_DEVICE {
    PVOID RegisterBase;
    PVOID OperationalBase;
    ULONG PortCount;
    BOOL HandoffComplete;
    EHCI_DEBUG_HANDOFF_DATA Data;
    EHCI_DEBUG_TRANSFER Transfers[EHCI_DEBUG_TRANSFER_COUNT];
} EHCI_DEBUG_DEVICE, *PEHCI_DEBUG_DEVICE;

/*++

Structure Description:

    This structure stores the context for an EHCI debug transport.

Members:

    Descriptor - Stores the hardware defined transfer descriptor.

    TransferLength - Stores the length of this transfer descriptor.

--*/

typedef struct _EHCI_DEBUG_TRANSFER_DESCRIPTOR {
    EHCI_TRANSFER_DESCRIPTOR Descriptor;
    ULONG TransferLength;
} EHCI_DEBUG_TRANSFER_DESCRIPTOR, *PEHCI_DEBUG_TRANSFER_DESCRIPTOR;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
