/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uhcihw.h

Abstract:

    This header contains UHCI hardware definitions.

Author:

    Evan Green 13-Jan-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of frame list pointers in a schedule.
//

#define UHCI_FRAME_LIST_ENTRY_COUNT 1024
#define UHCI_FRAME_LIST_ALIGNMENT 4096

#define UHCI_PORT_COUNT 2

//
// USB Command register bit definitions.
//

#define UHCI_COMMAND_MAX_RECLAMATION_PACKET_64 (1 << 7)
#define UHCI_COMMAND_MAX_RECLAMATION_PACKET_32 (0 << 7)
#define UHCI_COMMAND_CONFIGURED                (1 << 6)
#define UHCI_COMMAND_SINGLE_STEP               (1 << 5)
#define UHCI_COMMAND_FORCE_GLOBAL_RESUME       (1 << 4)
#define UHCI_COMMAND_ENTER_GLOBAL_SUSPEND      (1 << 3)
#define UHCI_COMMAND_GLOBAL_RESET              (1 << 2)
#define UHCI_COMMAND_HOST_CONTROLLER_RESET     (1 << 1)
#define UHCI_COMMAND_RUN                       (1 << 0)

//
// USB Status register bit definitions
//

#define UHCI_STATUS_HALTED                     (1 << 5)
#define UHCI_STATUS_PROCESS_ERROR              (1 << 4)
#define UHCI_STATUS_HOST_SYSTEM_ERROR          (1 << 3)
#define UHCI_STATUS_RESUME_DETECT              (1 << 2)
#define UHCI_STATUS_ERROR_INTERRUPT            (1 << 1)
#define UHCI_STATUS_INTERRUPT                  (1 << 0)

//
// USB Interrupt enable register bit definitions.
//

#define UHCI_INTERRUPT_SHORT_PACKET            (1 << 3)
#define UHCI_INTERRUPT_COMPLETION              (1 << 2)
#define UHCI_INTERRUPT_RESUME                  (1 << 1)
#define UHCI_INTERRUPT_TIMEOUT_CRC_ERROR       (1 << 0)

//
// USB Port Status/Control register definitions.
//

#define UHCI_PORT_SUSPEND                      (1 << 12)
#define UHCI_PORT_RESET                        (1 << 9)
#define UHCI_PORT_LOW_SPEED                    (1 << 8)
#define UHCI_PORT_RESUME_DETECT                (1 << 6)
#define UHCI_PORT_DPLUS                        (1 << 5)
#define UHCI_PORT_DMINUS                       (1 << 4)
#define UHCI_PORT_ENABLE_STATUS_CHANGED        (1 << 3)
#define UHCI_PORT_ENABLED                      (1 << 2)
#define UHCI_PORT_CONNECT_STATUS_CHANGED       (1 << 1)
#define UHCI_PORT_DEVICE_CONNECTED             (1 << 0)

//
// Define the bit definitions for each of the 1024 frame list entries.
//

#define UHCI_FRAME_LIST_ENTRY_ADDRESS_MASK     0xFFFFFFF0
#define UHCI_FRAME_LIST_ENTRY_QUEUE_HEAD       0x00000002
#define UHCI_FRAME_LIST_ENTRY_TERMINATE        0x00000001

//
// Define register definitions for the UHCI link pointer used in transfer
// descriptors.
//

#define UHCI_TRANSFER_DESCRIPTOR_LINK_ADDRESS_MASK 0xFFFFFFF0
#define UHCI_TRANSFER_DESCRIPTOR_LINK_DEPTH_FIRST  0x00000004
#define UHCI_TRANSFER_DESCRIPTOR_LINK_QUEUE_HEAD   0x00000002
#define UHCI_TRANSFER_DESCRIPTOR_LINK_TERMINATE    0x00000001

//
// Define register bit definitions for the status field of the transfer
// descriptor.
//

#define UHCI_TRANSFER_DESCRIPTOR_STATUS_SHORT_PACKET      (1 << 29)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_3_ERRORS          (0x3 << 27)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_2_ERRORS          (0x2 << 27)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_1_ERROR           (0x1 << 27)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_NO_ERRORS         (0x0 << 27)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_LOW_SPEED         (1 << 26)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_ISOCHRONOUS       (1 << 25)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_INTERRUPT         (1 << 24)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_ACTIVE            (1 << 23)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_STALLED           (1 << 22)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_DATA_BUFFER_ERROR (1 << 21)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_BABBLE            (1 << 20)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_NAK               (1 << 19)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_CRC_OR_TIMEOUT    (1 << 18)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_BITSTUFF_ERROR    (1 << 17)
#define UHCI_TRANSFER_DESCRIPTOR_STATUS_ERROR_MASK        \
    (UHCI_TRANSFER_DESCRIPTOR_STATUS_STALLED |            \
     UHCI_TRANSFER_DESCRIPTOR_STATUS_DATA_BUFFER_ERROR |  \
     UHCI_TRANSFER_DESCRIPTOR_STATUS_BABBLE |             \
     UHCI_TRANSFER_DESCRIPTOR_STATUS_CRC_OR_TIMEOUT |     \
     UHCI_TRANSFER_DESCRIPTOR_STATUS_BITSTUFF_ERROR)

#define UHCI_TRANSFER_DESCRIPTOR_STATUS_ACTUAL_LENGTH_MASK (0x000003FF)

//
// Define register bit definitions for the token field of the transfer
// descriptor.
//

#define UHCI_TRANSFER_DESCRIPTOR_TOKEN_MAX_LENGTH_SHIFT 21
#define UHCI_TRANSFER_DESCRIPTOR_TOKEN_DATA_TOGGLE (1 << 19)
#define UHCI_TRANSFER_DESCRIPTOR_TOKEN_ENDPOINT_SHIFT 15
#define UHCI_TRANSFER_DESCRIPTOR_TOKEN_ENDPOINT_MASK 0x00078000
#define UHCI_TRANSFER_DESCRIPTOR_TOKEN_ADDRESS_SHIFT 8
#define UHCI_TRANSFER_DESCRIPTOR_TOKEN_ADDRESS_MASK 0x00007F00
#define UHCI_TRANSFER_DESCRIPTOR_TOKEN_PID_MASK 0x000000FF

//
// Define register definitions for the UHCI link and element link pointers used
// in queue heads.
//

#define UHCI_QUEUE_HEAD_LINK_ADDRESS_MASK 0xFFFFFFF0
#define UHCI_QUEUE_HEAD_LINK_QUEUE_HEAD   0x00000002
#define UHCI_QUEUE_HEAD_LINK_TERMINATE    0x00000001

//
// Define the offset within the device's PCI Configuration Space where the
// legacy support register lives.
//

#define UHCI_LEGACY_SUPPORT_REGISTER_OFFSET 0xC0

//
// Define the value written into the legacy support register (off in PCI config
// space) to enable UHCI interrupts and stop trapping into SMIs for legacy
// keyboard support.
//

#define UHCI_LEGACY_SUPPORT_ENABLE_USB_INTERRUPTS 0x2000

//
//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _UHCI_REGISTER {
    UhciRegisterUsbCommand         = 0x00, // USBCMD
    UhciRegisterUsbStatus          = 0x02, // USBSTS
    UhciRegisterUsbInterruptEnable = 0x04, // USBINTR
    UhciRegisterFrameNumber        = 0x06, // FRNUM
    UhciRegisterFrameBaseAddress   = 0x08, // FRBASEADD
    UhciRegisterStartOfFrameModify = 0x0C, // SOFMOD
    UhciRegisterPort1StatusControl = 0x10, // PORTSC1
    UhciRegisterPort2StatusControl = 0x12, // PORTSC2
} UHCI_REGISTER, *PUHCI_REGISTER;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for UHCI transfer
    descriptors, which are the heart of moving data through USB on UHCI.

Members:

    LinkPointer - Stores the link pointer and flags to the next transfer
        descriptor or queue head.

    Status - Stores status information about how this transfer is going. The
        host controller hardware writes to this field.

    Token - Stores addressing and length information about the transfer.

    BufferPointer - Stores the physical address of the buffer to transfer, down
        to the byte granularity.

--*/

#pragma pack(push, 1)

typedef struct _UHCI_TRANSFER_DESCRIPTOR {
    ULONG LinkPointer;
    ULONG Status;
    ULONG Token;
    ULONG BufferPointer;
} PACKED UHCI_TRANSFER_DESCRIPTOR, *PUHCI_TRANSFER_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for UHCI queue head,
    which point to a chain of transfer descriptors (or queue heads in rare
    cases).

Members:

    LinkPointer - Stores a pointer to the next queue head or transfer
        descriptor. This is also known as the "horizontal" link.

    ElementLink - Stores a pointer to the next element in the queue. This is
        also known as the "vertical" link.

--*/

typedef struct _UHCI_QUEUE_HEAD {
    ULONG LinkPointer;
    ULONG ElementLink;
} PACKED UHCI_QUEUE_HEAD, *PUHCI_QUEUE_HEAD;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for a UHCI schedule.
    It contains 1024 frame list pointers, which get executed sequentially
    with each frame.

Members:

    Frame - Stores the array of frame list pointers, one for each frame in
        the schedule.

--*/

typedef struct _UHCI_SCHEDULE {
    ULONG Frame[UHCI_FRAME_LIST_ENTRY_COUNT];
} PACKED UHCI_SCHEDULE, *PUHCI_SCHEDULE;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
