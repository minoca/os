/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pcnet.h

Abstract:

    This header contains internal definitions for the AMD 79C9xx PCnet32 LANCE
    driver.

Author:

    Chris Stevens 9-Nov-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros for accessing a generic register in the E100.
//

#define PCNET_READ_REGISTER32(_Controller, _Register) \
    HlIoPortInLong((_Controller)->IoPortAddress + (_Register))

#define PCNET_READ_REGISTER16(_Controller, _Register) \
    HlIoPortInShort((_Controller)->IoPortAddress + (_Register))

#define PCNET_READ_REGISTER8(_Controller, _Register) \
    HlIoPortInByte((_Controller)->IoPortAddress + (_Register))

#define PCNET_WRITE_REGISTER32(_Controller, _Register, _Value) \
    HlIoPortOutLong((_Controller)->IoPortAddress + (_Register), (_Value))

#define PCNET_WRITE_REGISTER16(_Controller, _Register, _Value) \
    HlIoPortOutShort((_Controller)->IoPortAddress + (_Register), (_Value))

#define PCNET_WRITE_REGISTER8(_Controller, _Register, _Value) \
    HlIoPortOutByte((_Controller)->IoPortAddress + (_Register), (_Value))

//
// Define macros for incrementing the ring variables.
//

#define PCNET_INCREMENT_RING_INDEX(_Index, _PowerOf2Size) \
    (((_Index) + 1) & ((_PowerOf2Size) - 1))

#define PCNET_DECREMENT_RING_INDEX(_Index, _PowerOf2Size) \
    (((_Index) - 1) & ((_PowerOf2Size) - 1))

//
// ---------------------------------------------------------------- Definitions
//

#define PCNET_ALLOCATION_TAG 0x746E4350 // 'tnCP'

//
// Define the amount of time to wait in microseconds for initialization to
// complete.
//

#define PCNET_INITIALIZATION_TIMEOUT MICROSECONDS_PER_SECOND

//
// Define the length of the receive descriptor ring.
//

#define PCNET_RECEIVE_RING_LENGTH 32

//
// Define the length of the transmit descriptor ring.
//

#define PCNET_TRANSMIT_RING_LENGTH 32

//
// Define the bits for the controller status register - CSR0.
//

#define PCNET_CSR0_ERROR              (1 << 15)
#define PCNET_CSR0_BABBLE             (1 << 14)
#define PCNET_CSR0_COLLISION          (1 << 13)
#define PCNET_CSR0_MISSED_FRAME       (1 << 12)
#define PCNET_CSR0_MEMORY_ERROR       (1 << 11)
#define PCNET_CSR0_RECEIVE_INTERRUPT  (1 << 10)
#define PCNET_CSR0_TRANSMIT_INTERRUPT (1 << 9)
#define PCNET_CSR0_INIT_DONE          (1 << 8)
#define PCNET_CSR0_INTERRUPT          (1 << 7)
#define PCNET_CSR0_INTERRUPT_ENABLED  (1 << 6)
#define PCNET_CSR0_RECEIVE_ON         (1 << 5)
#define PCNET_CSR0_TRANSMIT_ON        (1 << 4)
#define PCNET_CSR0_TRANSMIT_DEMAND    (1 << 3)
#define PCNET_CSR0_STOP               (1 << 2)
#define PCNET_CSR0_START              (1 << 1)
#define PCNET_CSR0_INIT               (1 << 0)

#define PCNET_CSR0_INTERRUPT_MASK   \
    (PCNET_CSR0_ERROR |             \
     PCNET_CSR0_INTERRUPT |         \
     PCNET_CSR0_RECEIVE_INTERRUPT | \
     PCNET_CSR0_TRANSMIT_INTERRUPT)

//
// Define the bits for the lower initialization block address register - CSR1.
//

#define PCNET_CSR1_BLOCK_ADDRESS_LOWER_MASK (0xFFFF << 0)

//
// Define the bits for the upper initialization block address register - CSR2.
//

#define PCNET_CSR2_BLOCK_ADDRESS_UPPER_MASK (0xFFFF << 0)

//
// Define the bits for the interrupt masks and deferral register - CSR3.
//

#define PCNET_CSR3_BABBLE_MASK                        (1 << 14)
#define PCNET_CSR3_MISSED_FRAME_MASK                  (1 << 12)
#define PCNET_CSR3_MEMORY_ERROR_MASK                  (1 << 11)
#define PCNET_CSR3_RECEIVE_INTERRUPT_MASK             (1 << 10)
#define PCNET_CSR3_TRANSMIT_INTERRUPT_MASK            (1 << 9)
#define PCNET_CSR3_INIT_DONE_MASK                     (1 << 8)
#define PCNET_CSR3_LOOK_AHEAD_ENABLE                  (1 << 5)
#define PCNET_CSR3_TRANSMIT_TWO_PART_DEFFERAL_DISABLE (1 << 4)
#define PCNET_CSR3_MODIFIED_BACKOFF_ENABLE            (1 << 3)
#define PCNET_CSR3_BIG_ENDIAN                         (1 << 2)

//
// Define the bits for the test and feature control register - CSR4.
//

#define PCNET_CSR4_TEST_ENABLE                        (1 << 15)
#define PCNET_CSR4_DMA_PLUS_DISABLE                   (1 << 14)
#define PCNET_CSR4_TIMER_ENABLE                       (1 << 13)
#define PCNET_CSR4_TRANSMIT_POLL_DISABLE              (1 << 12)
#define PCNET_CSR4_AUTO_PAD_TRANSMIT                  (1 << 11)
#define PCNET_CSR4_AUTO_STRIP_RECEIVE                 (1 << 10)
#define PCNET_CSR4_MISSED_FRAME_COUNTER_OVERFLOW      (1 << 9)
#define PCNET_CSR4_MISSED_FRAME_COUNTER_OVERFLOW_MASK (1 << 8)
#define PCNET_CSR4_COLLISION_COUNTER_OVERFLOW         (1 << 5)
#define PCNET_CSR4_COLLISION_COUNTER_OVERFLOW_MASK    (1 << 4)
#define PCNET_CSR4_TRANSMIT_START                     (1 << 3)
#define PCNET_CSR4_TRANSMIT_START_MASK                (1 << 2)
#define PCNET_CSR4_JABBER                             (1 << 1)
#define PCNET_CSR4_JABBER_MASK                        (1 << 0)

//
// Define the bits for the transmit and receive table length register - CSR6.
//

#define PCNET_CSR6_TRANSMIT_RING_LENGTH_MASK  (0xF << 12)
#define PCNET_CSR6_TRANSMIT_RING_LENGTH_SHIFT 12
#define PCNET_CSR6_RECEIVE_RING_LENGTH_MASK   (0xF << 8)
#define PCNET_CSR6_RECEIVE_RING_LENGTH_SHIFT  8

//
// Define the bits for the burst and bus control register - BCR18.
//

#define PCNET_BCR18_ROM_TIMING_MASK    (0xF << 12)
#define PCNET_BCR18_ROM_TIMING_SHIFT   12
#define PCNET_BCR18_MEMORY_COMMAND     (1 << 9)
#define PCNET_BCR18_EXTENDED_REQUEST   (1 << 8)
#define PCNET_BCR18_DOUBLE_WORD_IO     (1 << 7)
#define PCNET_BCR18_BURST_READ_ENABLE  (1 << 6)
#define PCNET_BCR18_BURST_WRITE_ENABLE (1 << 5)

//
// Define the bits for the software style register - BCR20.
//

#define PCNET_BCR20_ADVANCED_PARITY_ERROR_HANDLING_ENABLE (1 << 10)
#define PCNET_BCR20_CSR_PCNET_ISA_CONFIGURATION           (1 << 9)
#define PCNET_BCR20_SOFTWARE_SIZE_32                      (1 << 8)
#define PCNET_BCR20_SOFTWARE_STYLE_PCNET_ISA_LANCE        0x00
#define PCNET_BCR20_SOFTWARE_STYLE_ILACC                  0x01
#define PCNET_BCR20_SOFTWARE_STYLE_PCNET_PCI              0x02
#define PCNET_BCR20_SOFTWARE_STYLE_PCNET_PCI_II           0x03
#define PCNET_BCR20_SOFTWARE_STYLE_MASK                   (0xFF << 0)
#define PCNET_BCR20_SOFTWARE_STYLE_SHIFT                  0

//
// Define the two descriptor ring alignment options.
//

#define PCNET_DESCRIPTOR_RING_ALIGNMENT_16 8
#define PCNET_DESCRIPTOR_RING_ALIGNMENT_32 16

//
// Define the maximum support physical addresses for data frame buffers.
//

#define PCNET_MAX_DATA_FRAME_ADDRESS_16 0x00FFFFFF
#define PCNET_MAX_DATA_FRAME_ADDRESS_32 0xFFFFFFFF

//
// Define the flag bits for receive descriptor.
//

#define PCNET_RECEIVE_DESCRIPTOR_OWN              (1 << 31)
#define PCNET_RECEIVE_DESCRIPTOR_ERROR            (1 << 30)
#define PCNET_RECEIVE_DESCRIPTOR_FRAME_ERROR      (1 << 29)
#define PCNET_RECEIVE_DESCRIPTOR_OVERFLOW         (1 << 28)
#define PCNET_RECEIVE_DESCRIPTOR_CRC              (1 << 27)
#define PCNET_RECEIVE_DESCRIPTOR_BUFFER           (1 << 26)
#define PCNET_RECEIVE_DESCRIPTOR_START            (1 << 25)
#define PCNET_RECEIVE_DESCRIPTOR_END              (1 << 24)
#define PCNET_RECEIVE_DESCRIPTOR_BUS_PARITY_ERROR (1 << 23)
#define PCNET_RECEIVE_DESCRIPTOR_PHYSICAL_MATCH   (1 << 22)
#define PCNET_RECEIVE_DESCRIPTOR_LOGICAL_MATCH    (1 << 21)
#define PCNET_RECEIVE_DESCRIPTOR_BROADCAST_MATCH  (1 << 20)

#define PCNET_RECEIVE_DESCRIPTOR_FLAGS_MASK_16 \
    (PCNET_RECEIVE_DESCRIPTOR_OWN |            \
     PCNET_RECEIVE_DESCRIPTOR_ERROR |          \
     PCNET_RECEIVE_DESCRIPTOR_FRAME_ERROR |    \
     PCNET_RECEIVE_DESCRIPTOR_OVERFLOW |       \
     PCNET_RECEIVE_DESCRIPTOR_CRC |            \
     PCNET_RECEIVE_DESCRIPTOR_BUFFER |         \
     PCNET_RECEIVE_DESCRIPTOR_START |          \
     PCNET_RECEIVE_DESCRIPTOR_END)

#define PCNET_RECEIVE_DESCRIPTOR_FLAGS_MASK_32   \
    (PCNET_RECEIVE_DESCRIPTOR_FLAGS_MASK_16 |    \
     PCNET_RECEIVE_DESCRIPTOR_BUS_PARITY_ERROR | \
     PCNET_RECEIVE_DESCRIPTOR_PHYSICAL_MATCH |   \
     PCNET_RECEIVE_DESCRIPTOR_LOGICAL_MATCH |    \
     PCNET_RECEIVE_DESCRIPTOR_BROADCAST_MATCH)

//
// Define the mask for the message and buffer lengths.
//

#define PCNET_RECEIVE_DESCRIPTOR_LENGTH_MASK (0xFFF << 0)

//
// Define the flag bits for the transmit descriptor.
//

#define PCNET_TRANSMIT_DESCRIPTOR_OWN        (1 << 31)
#define PCNET_TRANSMIT_DESCRIPTOR_ERROR      (1 << 30)
#define PCNET_TRANSMIT_DESCRIPTOR_FCS        (1 << 29)
#define PCNET_TRANSMIT_DESCRIPTOR_MORE_RETRY (1 << 28)
#define PCNET_TRANSMIT_DESCRIPTOR_ONE_RETRY  (1 << 27)
#define PCNET_TRANSMIT_DESCRIPTOR_DEFERRED   (1 << 26)
#define PCNET_TRANSMIT_DESCRIPTOR_START      (1 << 25)
#define PCNET_TRANSMIT_DESCRIPTOR_END        (1 << 24)

#define PCNET_TRANSMIT_DESCRIPTOR_FLAGS_MASK \
    (PCNET_TRANSMIT_DESCRIPTOR_OWN |         \
     PCNET_TRANSMIT_DESCRIPTOR_ERROR |       \
     PCNET_TRANSMIT_DESCRIPTOR_FCS |         \
     PCNET_TRANSMIT_DESCRIPTOR_MORE_RETRY |  \
     PCNET_TRANSMIT_DESCRIPTOR_ONE_RETRY |   \
     PCNET_TRANSMIT_DESCRIPTOR_DEFERRED |    \
     PCNET_TRANSMIT_DESCRIPTOR_START |       \
     PCNET_TRANSMIT_DESCRIPTOR_END)

//
// Define the flag bits for the transmit descriptor.
//

#define PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_BUFFER    (1 << 31)
#define PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_UNDERFLOW (1 << 30)
#define PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_DEFERRAL  (1 << 29)
#define PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_COLLISION (1 << 28)
#define PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_CARRIER   (1 << 27)
#define PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_RETRY     (1 << 26)

#define PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAGS_MASK    \
    (PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_BUFFER |    \
     PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_UNDERFLOW | \
     PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_DEFERRAL |  \
     PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_COLLISION | \
     PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_CARRIER |   \
     PCNET_TRANSMIT_DESCRIPTOR_ERROR_FLAG_RETRY)

//
// Define the required alignment for transmit descriptor buffers.
//

#define PCNET_TRANSMIT_BUFFER_ALIGNMENT 1

//
// Define the size of the receive frame data and the artificial alignment to
// 2K, which reduces the pressue of allocating a large chunk of memory.
//

#define PCNET_RECEIVE_FRAME_SIZE 1518
#define PCNET_RECEIVE_FRAME_ALIGNMENT 2048

//
// Define the bits for the initialization block mode.
//

#define PCNET_MODE_PROMISCUOUS                      (1 << 15)
#define PCNET_MODE_DISABLE_RECEIVE_BROADCAST        (1 << 14)
#define PCNET_MODE_DISABLE_RECEIVE_PHYSICAL_ADDRESS (1 << 13)
#define PCNET_MODE_DISABLE_LINK_STATUS              (1 << 12)
#define PCNET_MODE_DISABLE_POLARITY_CORRECTION      (1 << 11)
#define PCNET_MODE_MENDEC_LOOPBACK                  (1 << 10)
#define PCNET_MODE_LOW_RECEIVE_THRESHOLD            (1 << 9)
#define PCNET_MODE_TRANSMIT_MODE_SELECT             (1 << 9)
#define PCNET_MODE_PORT_SELECT_MASK                 (0x3 << 7)
#define PCNET_MODE_PORT_SELECT_SHIFT                7
#define PCNET_MODE_INTERNAL_LOOPBACK                (1 << 6)
#define PCNET_MODE_DISABLE_RETRY                    (1 << 5)
#define PCNET_MODE_FORCE_COLLISION                  (1 << 4)
#define PCNET_MODE_DISABLE_TRANSMIT_CRC             (1 << 3)
#define PCNET_MODE_LOOPBACK                         (1 << 2)
#define PCNET_MODE_DISABLE_TRANSMIT                 (1 << 1)
#define PCNET_MODE_DISABLE_RECEIVE                  (1 << 0)

//
// Define the bits for the initialization block ring lengths.
//

#define PCNET_INIT16_RECEIVE_RING_LENGTH_MASK  (0x7 << 29)
#define PCNET_INIT16_RECEIVE_RING_LENGTH_SHIFT 29
#define PCNET_INIT16_TRANSMIT_RING_LENGTH_MASK  (0x7 << 29)
#define PCNET_INIT16_TRANSMIT_RING_LENGTH_SHIFT 29

#define PCNET_INIT32_RECEIVE_RING_LENGTH_MASK   (0xF << 20)
#define PCNET_INIT32_RECEIVE_RING_LENGTH_SHIFT  20
#define PCNET_INIT32_TRANSMIT_RING_LENGTH_MASK  (0xF << 28)
#define PCNET_INIT32_TRANSMIT_RING_LENGTH_SHIFT 28

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _PCNET_CSR {
    PcnetCsr0Status = 0,
    PcnetCsr1InitBlockAddress0 = 1,
    PcnetCsr2InitBlockAddress1 = 2,
    PcnetCsr4FeatureControl = 4,
} PCNET_CSR, *PPCNET_CSR;

typedef enum _PCNET_BCR {
    PcnetBcr18BusControl = 18,
    PcnetBcr20SoftwareStyle = 20,
} PCNET_BCR, *PPCNET_PCR;

typedef enum _PCNET_WIO_REGISTER {
    PcnetWioAprom = 0x0,
    PcnetWioRegisterDataPort = 0x10,
    PcnetWioRegisterAddressPort = 0x12,
    PcnetWioReset = 0x14,
    PcnetWioBusDataPort = 0x16,
} PCNET_WIO_REGISTER, *PPCNET_WIO_REGISTER;

typedef enum _PCNET_DWIO_REGISTER {
    PcnetDwioRegisterDataPort = 0x10,
    PcnetDwioRegisterAddressPort = 0x14,
    PcnetDwioReset = 0x18,
    PcnetDwioBusDataPort = 0x1C,
} PCNET_DWIO_REGISTER, *PPCNET_DWIO_REGISTER;

/*++

Structure Description:

    This structure defines the 16-bit initialization block.

Members:

    Mode - Stores the mode bits equal to those defined by CSR15.

    PhysicalAddress - Stores the 6-byte MAC address to set.

    LogicalAddress - Stores 64-bits of logical address filtering.

    ReceiveRingAddress - Stores the 24-bit receive ring physical address along
        with the encoded receive ring length.

    TransmitRingAddress - Stores the 23-bit transmit ring physical address
        along with the encoded transmit ring length.

--*/

typedef struct _PCNET_INITIALIZATION_BLOCK_16 {
    USHORT Mode;
    BYTE PhysicalAddress[ETHERNET_ADDRESS_SIZE];
    ULONGLONG LogicalAddress;
    ULONG ReceiveRingAddress;
    ULONG TransmitRingAddress;
} PACKED PCNET_INITIALIZATION_BLOCK_16, *PPCNET_INITIALIZATION_BLOCK_16;

/*++

Structure Description:

    This structure defines the 32-bit initialization block.

Members:

    Mode - Stores the mode bits equal to those defined by CSR15. The upper
        16-bits store the encoded transmit and receive ring lengths.

    PhysicalAddress - Stores the 6-byte MAC address to set.

    Reserved - Stores 2 received bytes.

    LogicalAddress - Stores 64-bits of logical address filtering.

    ReceiveRingAddress - Stores the 32-bit receive ring physical address.

    TransmitRingAddress - Stores the 32-bit transmit ring physical address
        along.

--*/

typedef struct _PCNET_INITIALIZATION_BLOCK_32 {
    ULONG Mode;
    BYTE PhysicalAddress[ETHERNET_ADDRESS_SIZE];
    USHORT Reserved;
    ULONGLONG LogicalAddress;
    ULONG ReceiveRingAddress;
    ULONG TransmitRingAddress;
} PACKED PCNET_INITIALIZATION_BLOCK_32, *PPCNET_INITIALIZATION_BLOCK_32;

/*++

Structure Description:

    This structure defines a 16-bit receive descriptor.

Members:

    BufferAddress - Stores the 24-bit receive buffer physical address along
        with the flags.

    BufferLength - Stores the 16-bit two's complement of the buffer length.

    MessageLength - Stores the 16-bit unsigned integer length of the received
        packet.

--*/

typedef struct _PCNET_RECEIVE_DESCRIPTOR_16 {
    ULONG BufferAddress;
    USHORT BufferLength;
    USHORT MessageLength;
} PACKED PCNET_RECEIVE_DESCRIPTOR_16, *PPCNET_RECEIVE_DESCRIPTOR_16;

/*++

Structure Description:

    This structure defines a 32-bit receive descriptor.

Members:

    BufferAddress - Stores the 32-bit receive buffer physical address.

    BufferLength - Stores the 16-bit two's complement of the buffer length
        along with the flags.

    MessageLength - Stores the 16-bit unsigned integer length of the received
        packet.

    Reserved - Stores 4 reserved bytes.

--*/

typedef struct _PCNET_RECEIVE_DESCRIPTOR_32 {
    ULONG BufferAddress;
    ULONG BufferLength;
    ULONG MessageLength;
    ULONG Reserved;
} PACKED PCNET_RECEIVE_DESCRIPTOR_32, *PPCNET_RECEIVE_DESCRIPTOR_32;

/*++

Structure Description:

    This structure defines a 16-bit transmit descriptor.

Members:

    BufferAddress - Stores the 24-bit transmit buffer physical address along
        with the flags.

    BufferLength - Stores the 16-bit two's complement of the buffer length and
        the error flags.

--*/

typedef struct _PCNET_TRANSMIT_DESCRIPTOR_16 {
    ULONG BufferAddress;
    ULONG BufferLength;
} PACKED PCNET_TRANSMIT_DESCRIPTOR_16, *PPCNET_TRANSMIT_DESCRIPTOR_16;

/*++

Structure Description:

    This structure defines a 32-bit transmit descriptor.

Members:

    BufferAddress - Stores the 32-bit transmit buffer physical address.

    BufferLength - Stores the 16-bit two's complement of the buffer length and
        the flags.

    ErrorFlags - Stores the error flags.

    Reserved - Stores 4 reserved bytes.

--*/

typedef struct _PCNET_TRANSMIT_DESCRIPTOR_32 {
    ULONG BufferAddress;
    ULONG BufferLength;
    ULONG ErrorFlags;
    ULONG Reserved;
} PACKED PCNET_TRANSMIT_DESCRIPTOR_32, *PPCNET_TRANSMIT_DESCRIPTOR_32;

/*++

Structure Description:

    This structure defines an PCnet32 LANCE device.

Members:

    OsDevice - Stores a pointer to the OS device object.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    IoPortAddress - Stores the I/O port address for the PCnet's registers.

    NetworkLink - Stores a pointer to the core networking link.

    IoBuffer - Stores an I/O buffer that contains the initialization block and
        both descriptor rings.

    ReceiveIoBuffer - Stores an I/O buffer that contains the receive data frame
        buffers.

    InitializationBlock - Stores the pointer to the initialization block.

    ReceiveDescriptor - Stores the pointer to the array of receive descriptors.

    ReceiveListBegin - Stores the index of the beginning of the list, which is
        the oldest received descriptor and the first one to dispatch.

    ReceiveListLock - Stores a pointer to a queued lock that protects the
        received list.

    TransmitDescriptor - Stores a pointer to array of transmit descriptors.

    TransmitPacket - Stores a pointer to the array of net packet buffers that
        go with each transmit descriptor.

    TransmitLastReaped - Stores the index of the last transmit descriptor that
        was reaped.

    TransmitNextToUse - Stores the index of the next transmit descriptor to
        use. If this equals the next index to be reaped, then the list is full.

    TransmitListLock - Stores the lock protecting simultaneous software access
        to the transmit descriptor data structures.

    TransmitPacketList - Stores a list of network packets waiting to be sent.

    LinkActive - Stores a boolean indicating if there is an active network link.

    PendingStatusBits - Stores the bitfield of status bits that have yet to be
        dealt with by software.

    EepromMacAddress - Stores the default MAC address of the device.

    Registers32 - Stores a boolean indicating whether or not the I/O port
        address should be accessed with 16-bit or 32-bit reads.

    Software32 - Stores a boolean indicating whether or not this device is
        operating with 32-bit structures (TRUE) or 16-bit structures (FALSE).

--*/

typedef struct _PCNET_DEVICE {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    USHORT IoPortAddress;
    PNET_LINK NetworkLink;
    PIO_BUFFER IoBuffer;
    PIO_BUFFER ReceiveIoBuffer;
    PVOID InitializationBlock;
    PVOID ReceiveDescriptor;
    ULONG ReceiveListBegin;
    PQUEUED_LOCK ReceiveListLock;
    PVOID TransmitDescriptor;
    PNET_PACKET_BUFFER *TransmitPacket;
    ULONG TransmitLastReaped;
    ULONG TransmitNextToUse;
    PQUEUED_LOCK TransmitListLock;
    NET_PACKET_LIST TransmitPacketList;
    BOOL LinkActive;
    ULONG PendingStatusBits;
    BYTE EepromMacAddress[ETHERNET_ADDRESS_SIZE];
    BOOL Registers32;
    BOOL Software32;
} PCNET_DEVICE, *PPCNET_DEVICE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Hardware functions called by the administrative side.
//

KSTATUS
PcnetSend (
    PVOID DeviceContext,
    PNET_PACKET_LIST PacketList
    );

/*++

Routine Description:

    This routine sends data through the network.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link down which this data is to be sent.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

Return Value:

    STATUS_SUCCESS if all packets were sent.

    STATUS_RESOURCE_IN_USE if some or all of the packets were dropped due to
    the hardware being backed up with too many packets to send.

    Other failure codes indicate that none of the packets were sent.

--*/

KSTATUS
PcnetGetSetInformation (
    PVOID DeviceContext,
    NET_LINK_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets the network device layer's link information.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link for which information is being set or queried.

    InformationType - Supplies the type of information being queried or set.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or a
        set operation (TRUE).

Return Value:

    Status code.

--*/

KSTATUS
PcnetpInitializeDevice (
    PPCNET_DEVICE Device
    );

KSTATUS
PcnetpInitializeDeviceStructures (
    PPCNET_DEVICE Device
    );

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an PCnet LANCE device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

KSTATUS
PcnetpResetDevice (
    PPCNET_DEVICE Device
    );

/*++

Routine Description:

    This routine resets the E100 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
PcnetpInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the PCnet interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
PcnetpInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine processes interrupts for the PCnet controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt status.

--*/

//
// Administrative functions called by the hardware side.
//

KSTATUS
PcnetpAddNetworkDevice (
    PPCNET_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

