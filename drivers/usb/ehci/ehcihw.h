/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ehcihw.h

Abstract:

    This header contains EHCI hardware definitions.

Author:

    Evan Green 18-Mar-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define PCI configuration register offsets.
//

#define EHCI_USB_REGISTER_BASE_REGISTER 0x10
#define EHCI_USB_REGISTER_BASE_ADDRESS_MASK 0xFFFFFF00

#define EHCI_EECP_LEGACY_SUPPORT_REGISTER 0x00
#define EHCI_LEGACY_SUPPORT_OS_OWNED      (1 << 24)
#define EHCI_LEGACY_SUPPORT_BIOS_OWNED    (1 << 16)
#define EHCI_EECP_LEGACY_SUPPORT_REGISTER 0x00
#define EHCI_EECP_LEGACY_CONTROL_REGISTER 0x04

//
// Define EHCI capability register offsets.
//

#define EHCI_CAPABILITY_LENGTH_REGISTER                0x00
#define EHCI_CAPABILITY_VERSION_REGISTER               0x02
#define EHCI_CAPABILITY_PARAMETERS_REGISTER            0x04
#define EHCI_CAPABILITY_PARAMETERS_PORT_COUNT_MASK     0x0000000F
#define EHCI_CAPABILITY_CAPABILITIES_REGISTER          0x08
#define EHCI_CAPABILITY_CAPABILITIES_EXTENDED_CAPABILITIES_MASK 0x0000FF00
#define EHCI_CAPABILITY_CAPABILITIES_EXTENDED_CAPABILITIES_SHIFT 8
#define EHCI_CAPABILITY_PORT_ROUTING_REGISTER          0x0C

//
// Define the default number of frame list pointers in a schedule.
//

#define EHCI_DEFAULT_FRAME_LIST_ENTRY_COUNT 1024
#define EHCI_FRAME_LIST_ALIGNMENT 4096
#define EHCI_TRANSFER_POINTER_COUNT 5
#define EHCI_PAGE_SIZE 4096
#define EHCI_TRANSFER_MAX_PACKET_SIZE \
    (EHCI_TRANSFER_POINTER_COUNT * EHCI_PAGE_SIZE)

//
// EHCI USB Command bit definitions
//

#define EHCI_COMMAND_INTERRUPT_EVERY_1_UFRAME   (0x1 << 16)
#define EHCI_COMMAND_INTERRUPT_EVERY_2_UFRAMES  (0x2 << 16)
#define EHCI_COMMAND_INTERRUPT_EVERY_4_UFRAMES  (0x4 << 16)
#define EHCI_COMMAND_INTERRUPT_EVERY_8_UFRAMES  (0x8 << 16)
#define EHCI_COMMAND_INTERRUPT_EVERY_16_UFRAMES (0x10 << 16)
#define EHCI_COMMAND_INTERRUPT_EVERY_32_UFRAMES (0x20 << 16)
#define EHCI_COMMAND_INTERRUPT_EVERY_64_UFRAMES (0x40 << 16)
#define ECHI_COMMAND_ASYNC_PARK_ENABLE          (1 << 11)
#define EHCI_COMMAND_PARK_COUNT_SHIFT           8
#define EHCI_COMMAND_LIGHT_CONTROLLER_RESET     (1 << 7)
#define EHCI_COMMAND_INTERRUPT_ON_ASYNC_ADVANCE (1 << 6)
#define EHCI_COMMAND_ENABLE_ASYNC_SCHEDULE      (1 << 5)
#define EHCI_COMMAND_ENABLE_PERIODIC_SCHEDULE   (1 << 4)
#define EHCI_COMMAND_1024_FRAME_LIST_ENTRIES    (0 << 2)
#define EHCI_COMMAND_512_FRAME_LIST_ENTRIES     (1 << 2)
#define EHCI_COMMAND_256_FRAME_LIST_ENTRIES     (2 << 2)
#define EHCI_COMMAND_CONTROLLER_RESET           (1 << 1)
#define EHCI_COMMAND_RUN                        (1 << 0)

//
// EHCI USB Status bit definitions
//

#define EHCI_STATUS_ASYNC_SCHEDULE_STATUS       (1 << 15)
#define EHCI_STATUS_PERIODIC_SCHEDULE_STATUS    (1 << 14)
#define EHCI_STATUS_RECLAMATION                 (1 << 13)
#define EHCI_STATUS_HALTED                      (1 << 12)
#define EHCI_STATUS_INTERRUPT_ON_ASYNC_ADVANCE  (1 << 5)
#define EHCI_STATUS_HOST_SYSTEM_ERROR           (1 << 4)
#define EHCI_STATUS_FRAME_LIST_ROLLOVER         (1 << 3)
#define EHCI_STATUS_PORT_CHANGE_DETECT          (1 << 2)
#define EHCI_STATUS_USB_ERROR_INTERRUPT         (1 << 1)
#define EHCI_STATUS_USB_INTERRUPT               (1 << 0)
#define EHCI_STATUS_INTERRUPT_MASK            \
    (EHCI_STATUS_INTERRUPT_ON_ASYNC_ADVANCE | \
     EHCI_STATUS_HOST_SYSTEM_ERROR |          \
     EHCI_STATUS_FRAME_LIST_ROLLOVER |        \
     EHCI_STATUS_PORT_CHANGE_DETECT |         \
     EHCI_STATUS_USB_ERROR_INTERRUPT |        \
     EHCI_STATUS_USB_INTERRUPT)

//
// EHCI Interrupt Enable bit definitions
//

#define EHCI_INTERRUPT_ASYNC_ADVANCE            (1 << 5)
#define EHCI_INTERRUPT_HOST_SYSTEM_ERROR        (1 << 4)
#define EHCI_INTERRUPT_FRAME_LIST_ROLLOVER      (1 << 3)
#define EHCI_INTERRUPT_PORT_CHANGE              (1 << 2)
#define EHCI_INTERRUPT_USB_ERROR                (1 << 1)
#define EHCI_INTERRUPT_ENABLE                   (1 << 0)

//
// EHCI Port Status bit definitions
// For the line state registers, K state is a low speed device, J state is a
// full or high speed device.
//

#define EHCI_PORT_WAKE_ON_OVER_CURRENT          (1 << 22)
#define EHCI_PORT_WAKE_ON_DISCONNECT            (1 << 21)
#define EHCI_PORT_WAKE_ON_CONNECT               (1 << 20)
#define EHCI_PORT_TEST_MODE_DISABLED            (0x0 << 16)
#define EHCI_PORT_TEST_J_STATE                  (0x1 << 16)
#define EHCI_PORT_TEST_K_STATE                  (0x2 << 16)
#define EHCI_PORT_TEST_SE0_NAK                  (0x3 << 16)
#define EHCI_PORT_TEST_PACKET                   (0x4 << 16)
#define EHCI_PORT_TEST_FORCE_ENABLE             (0x5 << 16)
#define EHCI_PORT_INDICATOR_OFF                 (0x0 << 14)
#define EHCI_PORT_INDICATOR_AMBER               (0x1 << 14)
#define EHCI_PORT_INDICATOR_GREEN               (0x2 << 14)
#define EHCI_PORT_INDICATOR_MASK                (0x3 << 14)
#define EHCI_PORT_OWNER                         (1 << 13)
#define EHCI_PORT_POWER                         (1 << 12)
#define EHCI_PORT_LINE_STATE_SE0                (0x0 << 10)
#define EHCI_PORT_LINE_STATE_K                  (0x1 << 10)
#define EHCI_PORT_LINE_STATE_J                  (0x2 << 10)
#define EHCI_PORT_LINE_STATE_MASK               (0x3 << 10)
#define EHCI_PORT_RESET                         (1 << 8)
#define EHCI_PORT_SUSPEND                       (1 << 7)
#define EHCI_PORT_RESUME                        (1 << 6)
#define EHCI_PORT_OVER_CURRENT_CHANGE           (1 << 5)
#define EHCI_PORT_OVER_CURRENT_ACTIVE           (1 << 4)
#define EHCI_PORT_ENABLE_CHANGE                 (1 << 3)
#define EHCI_PORT_ENABLE                        (1 << 2)
#define EHCI_PORT_CONNECT_STATUS_CHANGE         (1 << 1)
#define EHCI_PORT_CONNECT_STATUS                (1 << 0)

//
// Define common bits across all link pointers.
//

#define EHCI_LINK_TYPE_ISOCHRONOUS_TRANSFER       (0x0 << 1)
#define EHCI_LINK_TYPE_QUEUE_HEAD                 (0x1 << 1)
#define EHCI_LINK_TYPE_SPLIT_ISOCHRONOUS_TRANSFER (0x2 << 1)
#define EHCI_LINK_TYPE_FRAME_SPAN_TRAVERSAL_NODE  (0x3 << 1)
#define EHCI_LINK_TERMINATE                       (1 << 0)
#define EHCI_LINK_ADDRESS_MASK                    0xFFFFFFE0
#define EHCI_LINK_ALIGNMENT                       32

//
// Define Transfer Descriptor Token and Page 0 bit definitions.
//

#define EHCI_TRANSFER_DATA_TOGGLE               (1 << 31)
#define EHCI_TRANSFER_TOTAL_BYTES_MASK          0x7FFF0000
#define EHCI_TRANSFER_TOTAL_BYTES_SHIFT         16
#define EHCI_TRANSFER_INTERRUPT_ON_COMPLETE     (1 << 15)
#define EHCI_TRANSFER_CURRENT_PAGE_SHIFT        12
#define EHCI_TRANSFER_3_ERRORS_ALLOWED          (0x3 << 10)
#define EHCI_TRANSFER_2_ERRORS_ALLOWED          (0x2 << 10)
#define EHCI_TRANSFER_1_ERROR_ALLOWED           (0x1 << 10)
#define EHCI_TRANSFER_UNLIMITED_ERRORS          (0x0 << 10)
#define EHCI_TRANSFER_ERROR_COUNT_SHIFT         10
#define EHCI_TRANSFER_PID_CODE_OUT              (0x0 << 8)
#define EHCI_TRANSFER_PID_CODE_IN               (0x1 << 8)
#define EHCI_TRANSFER_PID_CODE_SETUP            (0x2 << 8)
#define EHCI_TRANSFER_STATUS_ACTIVE             (1 << 7)
#define EHCI_TRANSFER_STATUS_HALTED             (1 << 6)
#define EHCI_TRANSFER_STATUS_DATA_BUFFER_ERROR  (1 << 5)
#define EHCI_TRANSFER_BABBLE_ERROR              (1 << 4)
#define EHCI_TRANSFER_TRANSACTION_ERROR         (1 << 3)
#define EHCI_TRANSFER_MISSED_MICRO_FRAME_ERROR  (1 << 2)
#define EHCI_TRANSFER_SPLIT_DO_COMPLETE         (1 << 1)
#define EHCI_TRANSFER_SPLIT_DO_PING             (1 << 0)
#define EHCI_TRANSFER_ERROR_MASK              \
    (EHCI_TRANSFER_STATUS_HALTED |            \
     EHCI_TRANSFER_STATUS_DATA_BUFFER_ERROR | \
     EHCI_TRANSFER_BABBLE_ERROR |             \
     EHCI_TRANSFER_TRANSACTION_ERROR |        \
     EHCI_TRANSFER_MISSED_MICRO_FRAME_ERROR)

#define EHCI_TRANSFER_CURRENT_OFFSET_MASK       0x00000FFF
#define EHCI_TRANSFER_BUFFER_POINTER_MASK       0xFFFFF000

//
// Define Queue Head endpoint capability bit definitions.
//

#define EHCI_QUEUE_DEFAULT_NAK_RELOAD_COUNT            1
#define EHCI_QUEUE_MAX_NAK_RELOAD_COUNT                0xF
#define EHCI_QUEUE_NAK_RELOAD_COUNT_SHIFT              28
#define EHCI_QUEUE_CONTROL_ENDPOINT                    (1 << 27)
#define EHCI_QUEUE_MAX_PACKET_LENGTH_SHIFT             16
#define EHCI_QUEUE_MAX_PACKET_LENGTH_MASK              0x07FF0000
#define EHCI_QUEUE_RECLAMATION_HEAD                    (1 << 15)
#define EHCI_QUEUE_USE_TRANSFER_DESCRIPTOR_DATA_TOGGLE (1 << 14)
#define EHCI_QUEUE_FULL_SPEED                          (0x0 << 12)
#define EHCI_QUEUE_LOW_SPEED                           (0x1 << 12)
#define EHCI_QUEUE_HIGH_SPEED                          (0x2 << 12)
#define EHCI_QUEUE_ENDPOINT_SHIFT                      8
#define EHCI_QUEUE_INACTIVATE_ON_NEXT_TRANSACTION      (1 << 7)
#define EHCI_QUEUE_DEVICE_ADDRESS_MASK                 0x7F

#define EHCI_QUEUE_1_TRANSACTION_PER_MICRO_FRAME       (0x1 << 30)
#define EHCI_QUEUE_2_TRANSACTIONS_PER_MICRO_FRAME      (0x2 << 30)
#define EHCI_QUEUE_3_TRANSACTIONS_PER_MICRO_FRAME      (0x3 << 30)
#define EHCI_QUEUE_PORT_NUMBER_SHIFT                   23
#define EHCI_QUEUE_PORT_NUMBER_MASK                    0x3F800000
#define EHCI_QUEUE_HUB_ADDRESS_SHIFT                   16
#define EHCI_QUEUE_HUB_ADDRESS_MASK                    0x007F0000
#define EHCI_QUEUE_SPLIT_COMPLETION_MASK               0x0000FF00
#define EHCI_QUEUE_SPLIT_COMPLETION_SHIFT              8
#define EHCI_QUEUE_INTERRUPT_SCHEDULE_MASK             0x000000FF
#define EHCI_QUEUE_SPLIT_START_MASK EHCI_QUEUE_INTERRUPT_SCHEDULE_MASK

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _EHCI_REGISTER {
    EhciRegisterUsbCommand              = 0x00, // USBCMD
    EhciRegisterUsbStatus               = 0x04, // USBSTS
    EhciRegisterUsbInterruptEnable      = 0x08, // USBINTR
    EhciRegisterFrameNumber             = 0x0C, // FRINDEX
    EhciRegisterSegmentSelector         = 0x10, // CTRLDSSEGMENT
    EhciRegisterPeriodicListBase        = 0x14, // PERIODICLISTBASE
    EhciRegisterAsynchronousListAddress = 0x18, // ASYNCLISTADDR
    EhciRegisterConfigured              = 0x40, // CONFIGFLAG
    EhciRegisterPortStatusBase          = 0x44, // PORTSC[1-N_PORTS]
} EHCI_REGISTER, *PEHCI_REGISTER;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for EHCI transfer
    descriptors, which are the heart of moving data through USB on EHCI host
    controllers. Pointers to this structure must be 32-byte aligned as required
    by the EHCI specification.

Members:

    NextTransfer - Stores a link pointer containing the physical address of the
        next transfer in the queue.

    AlternateNextTransfer - Stores a link pointer containing the physical
        address of the next transfer should this transfer be a short IN
        packet.

    Token - Stores the working control/status information of the transfer.

    BufferPointer - Stores an array of physical pointers to the data to
        transfer. This data must be virtually contiguous, essentially meaning
        that buffers that are not the first or last must be a full 4096 bytes
        large.

    BufferAddressHigh - Stores the high 32 bits of each of the addresses in the
        buffer pointers. This is only referenced by the hardware if 64 bit
        structures are supported.

--*/

#pragma pack(push, 1)

typedef struct _EHCI_TRANSFER_DESCRIPTOR {
    ULONG NextTransfer;
    ULONG AlternateNextTransfer;
    ULONG Token;
    ULONG BufferPointer[EHCI_TRANSFER_POINTER_COUNT];
    ULONG BufferAddressHigh[EHCI_TRANSFER_POINTER_COUNT];
} PACKED EHCI_TRANSFER_DESCRIPTOR, *PEHCI_TRANSFER_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for EHCI transfer
    queues, which manage sets of transfer descriptors for all transfer types
    except isochronous. Pointers to this structure must be 32-byte aligned as
    required by the EHCI specification.

Members:

    HorizontalLink - Stores a pointer to the next element after this queue,
        which if on the periodic list may be an Isochronous Transfer Descriptor,
        Queue Head, or Frame Span Traversal Node. On the asynchronous list, this
        would only be another Queue Head.

    Destination - Stores queue adressing information and transfer length.

    SplitInformation - Stores information relating largely to performing full/
        low speed split transactions.

    CurrentTransferDescriptorLink - Stores the link pointer to the transfer
        descriptor currently being processed.

    TransferOverlay - Stores the working space for the EHCI controller when
        processing transfer descriptors on this queue.

--*/

typedef struct _EHCI_QUEUE_HEAD {
    ULONG HorizontalLink;
    ULONG Destination;
    ULONG SplitInformation;
    ULONG CurrentTransferDescriptorLink;
    EHCI_TRANSFER_DESCRIPTOR TransferOverlay;
} PACKED EHCI_QUEUE_HEAD, *PEHCI_QUEUE_HEAD;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for EHCI Frame
    Span Traversal Nodes, which are used to ensure split transactions that
    occur across a frame boundary get the appropriate attention from the
    periodic schedule.

Members:

    NormalPathLink - Stores the normal path to the next element in the periodic
        schedule, which is automatically traversed during micro-frames 2-7 and
        when not in restore path mode.

    BackPathLink - Store the link to the valid in-progress split transaction,
        in which case this is called a Save-Place node. If the T bit is set,
        then the back path link is invalid, and this node is considered a
        Restore node.

--*/

typedef struct _EHCI_FRAME_SPAN_TRAVERSAL_NODE {
    ULONG NormalPathLink;
    ULONG BackPathLink;
} PACKED EHCI_FRAME_SPAN_TRAVERSAL_NODE, *PEHCI_FRAME_SPAN_TRAVERSAL_NODE;

/*++

Structure Description:

    This structure defines the hardware-mandated structure for an EHCI periodic
    schedule. It contains 1024 frame list pointers by default, which get
    executed 8 times for each frame.

Members:

    Frame - Stores the array of frame list pointers, one for each frame in
        the schedule.

--*/

typedef struct _EHCI_PERIODIC_SCHEDULE {
    ULONG FrameLink[EHCI_DEFAULT_FRAME_LIST_ENTRY_COUNT];
} PACKED EHCI_PERIODIC_SCHEDULE, *PEHCI_PERIODIC_SCHEDULE;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
