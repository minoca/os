/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sm91c1.h

Abstract:

    This header contains definitions for the SMSC91C111 LAN Ethernet Controller.

Author:

    Chris Stevens 16-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define SM91C1_ALLOCATION_TAG 0x31396D53 // '19mS'

//
// Define the flags for the transmit control register.
//

#define SM91C1_TRANSMIT_CONTROL_ENABLE                0x0001
#define SM91C1_TRANSMIT_CONTROL_LOOP                  0x0002
#define SM91C1_TRANSMIT_CONTROL_FORCE_COLLISION       0x0004
#define SM91C1_TRANSMIT_CONTROL_PAD_ENABLE            0x0080
#define SM91C1_TRANSMIT_CONTROL_NO_CRC                0x0100
#define SM91C1_TRANSMIT_CONTROL_MONITOR_CARRIER_SENSE 0x0400
#define SM91C1_TRANSMIT_CONTROL_FULL_DUPLEX           0x0800
#define SM91C1_TRANSMIT_CONTROL_STOP_ON_SQET          0x1000
#define SM91C1_TRANSMIT_CONTROL_EPH_LOOP              0x2000
#define SM91C1_TRANSMIT_CONTROL_SWITCHED_FULL_DUPLEX  0x8000

//
// Define the flags for the EPH status register.
//

#define SM91C1_EPH_STATUS_TRANSMIT_SUCCESSFUL     0x0001
#define SM91C1_EPH_STATUS_SINGLE_COLLISION        0x0002
#define SM91C1_EPH_STATUS_MULTIPLE_COLLISIONS     0x0004
#define SM91C1_EPH_STATUS_LAST_TRANSMIT_MULTICAST 0x0008
#define SM91C1_EPH_STATUS_16_COLLISIONS           0x0010
#define SM91C1_EPH_STATUS_SQET                    0x0020
#define SM91C1_EPH_STATUS_LAST_TRANSMIT_BROADCAST 0x0040
#define SM91C1_EPH_STATUS_TRANSMIT_DEFERRED       0x0080
#define SM91C1_EPH_STATUS_LATE_COLLISION          0x0200
#define SM91C1_EPH_STATUS_LOST_CARRIER_SENSE      0x0400
#define SM91C1_EPH_STATUS_EXCESSIVE_DEFERRAL      0x0800
#define SM91C1_EPH_STATUS_COUNTER_ROLLOVER        0x1000
#define SM91C1_EPH_STATUS_LINK_OK                 0x4000

//
// Define the flags for the receive control register.
//

#define SM91C1_RECEIVE_CONTROL_ABORTED        0x0001
#define SM91C1_RECEIVE_CONTROL_PROMISCUOUS    0x0002
#define SM91C1_RECEIVE_CONTROL_ALL_MULTICAST  0x0004
#define SM91C1_RECEIVE_CONTROL_ENABLE         0x0100
#define SM91C1_RECEIVE_CONTROL_STRIP_CRC      0x0200
#define SM91C1_RECEIVE_CONTROL_ABORT_ENABLE   0x2000
#define SM91C1_RECEIVE_CONTROL_FILTER_CARRIER 0x4000
#define SM91C1_RECEIVE_CONTROL_SOFT_RESET     0x8000

//
// Define the bits for the counter register.
//

#define SM91C1_COUNTER_SINGLE_COLLISION_MASK              0x000F
#define SM91C1_COUNTER_SINGLE_COLLISION_SHIFT             0
#define SM91C1_COUNTER_MULTIPLE_COLLISION_MASK            0x00F0
#define SM91C1_COUNTER_MULTIPLE_COLLISION_SHIFT           4
#define SM91C1_COUNTER_DEFERRED_TRANSMITS_MASK            0x0F00
#define SM91C1_COUNTER_DEFERRED_TRANSMITS_SHIFT           8
#define SM91C1_COUNTER_EXCESSIVE_DEFERRED_TRANSMITS_MASK  0xF000
#define SM91C1_COUNTER_EXCESSIVE_DEFERRED_TRANSMITS_SHIFT 12

//
// Define the bits for the memory register.
//

#define SM91C1_MEMORY_INFORMATION_SIZE_MASK  0x00FF
#define SM91C1_MEMORY_INFORMATION_SIZE_SHIFT 0
#define SM91C1_MEMORY_INFORMATION_FREE_MASK  0xFF00
#define SM91C1_MEMORY_INFORMATION_FREE_SHIFT 8

#define SM91C1_MEMORY_UNIT_SIZE _2KB

//
// Define the bits for the PHY control register.
//

#define SM91C1_PHY_CONTROL_LED_SELECT_0B   0x0004
#define SM91C1_PHY_CONTROL_LED_SELECT_1B   0x0008
#define SM91C1_PHY_CONTROL_LED_SELECT_2B   0x0010
#define SM91C1_PHY_CONTROL_LED_SELECT_0A   0x0020
#define SM91C1_PHY_CONTROL_LED_SELECT_1A   0x0040
#define SM91C1_PHY_CONTROL_LED_SELECT_2A   0x0080
#define SM91C1_PHY_CONTROL_AUTONEGOTIATION 0x0800
#define SM91C1_PHY_CONTROL_FULL_DUPLEX     0x1000
#define SM91C1_PHY_CONTROL_SPEED_100       0x2000

//
// Define the flags for the bank select register.
//

#define SM91C1_BANK_SELECT_BANK_MASK  0x0007
#define SM91C1_BANK_SELECT_BANK_SHIFT 0

//
// Define the flags for the configuration register.
//

#define SM91C1_CONFIGURATION_REGISTER_EXTERNAL_PHY            0x0200
#define SM91C1_CONFIGURATION_REGISTER_GENERAL_PURPOSE_CONTROL 0x0400
#define SM91C1_CONFIGURATION_REGISTER_NO_WAIT                 0x1000
#define SM91C1_CONFIGURATION_REGISTER_EPH_POWER_ENABLE        0x8000

//
// Define the bits for the base address register.
//

#define SM91C1_BASE_ADDRESS_BITS_5_TO_9_MASK    0x1F00
#define SM91C1_BASE_ADDRESS_BITS_5_TO_9_SHIFT   8
#define SM91C1_BASE_ADDRESS_BITS_13_TO_15_MASK  0xE000
#define SM91C1_BASE_ADDRESS_BITS_13_TO_15_SHIFT 13

//
// Define the flags for the control register.
//

#define SM91C1_CONTROL_EEPROM_STORE            0x0001
#define SM91C1_CONTROL_EEPROM_RELOAD           0x0002
#define SM91C1_CONTROL_EEPROM_SELECT           0x0004
#define SM91C1_CONTROL_TRANSMIT_ERROR_ENABLE   0x0020
#define SM91C1_CONTROL_COUTER_ROLLOVER_ENABLE  0x0040
#define SM91C1_CONTROL_LINK_ERROR_ENABLE       0x0080
#define SM91C1_CONTROL_AUTO_RELEASE            0x0800
#define SM91C1_CONTROL_RECEIVE_BAD_CRC_PACKETS 0x4000

//
// Define the flags for the MMU command register.
//

#define SM91C1_MMU_COMMAND_BUSY            0x0001
#define SM91C1_MMU_COMMAND_OPERATION_MASK  0x00E0
#define SM91C1_MMU_COMMAND_OPERATION_SHIFT 5

//
// Define the MMU operations.
//

#define SM91C1_MMU_OPERATION_NO_OP                           0x0
#define SM91C1_MMU_OPERATION_ALLOCATE_FOR_TRANSMIT           0x1
#define SM91C1_MMU_OPERATION_RESET                           0x2
#define SM91C1_MMU_OPERATION_RECEIVE_FIFO_REMOVE             0x3
#define SM91C1_MMU_OPERATION_RECEiVE_FIFO_REMOVE_AND_RELEASE 0x4
#define SM91C1_MMU_OPERATION_RELEASE_PACKET                  0x5
#define SM91C1_MMU_OPERATION_QUEUE_PACKET_FOR_TRANSMIT       0x6
#define SM91C1_MMU_OPERATION_TRANSMIT_FIFO_RESET             0x7

//
// Define the flags for the packet number register.
//

#define SM91C1_PACKET_NUMBER_MASK  0x3F
#define SM91C1_PACKET_NUMBER_SHIFT 0

//
// Define the flags for the allocation result register.
//

#define SM91C1_ALLOCATION_RESULT_PACKET_NUMBER_MASK  0x3F
#define SM91C1_ALLOCATION_RESULT_PACKET_NUMBER_SHIFT 0
#define SM91C1_ALLOCATION_RESULT_FAILED              0x80

//
// Define the flags for the transmit FIFO register.
//

#define SM91C1_FIFO_PORTS_TRANSMIT_PACKET_NUMBER_MASK  0x3F
#define SM91C1_FIFO_PORTS_TRANSMIT_PACKET_NUMBER_SHIFT 0
#define SM91C1_FIFO_PORTS_TRANSMIT_EMPTY               0x80

//
// Define the flags for the receive FIFO register.
//

#define SM91C1_FIFO_PORTS_RECEIVE_PACKET_NUMBER_MASK   0x3F
#define SM91C1_FIFO_PORTS_RECEIVE_PACKET_NUMBER_SHIFT  0
#define SM91C1_FIFO_PORTS_RECEIVE_EMPTY                0x80

//
// Define the flags for the pointer register.
//

#define SM91C1_POINTER_MASK           0x07FF
#define SM91C1_POINTER_SHIFT          0
#define SM91C1_POINTER_NOT_EMPTY      0x0800
#define SM91C1_POINTER_READ           0x2000
#define SM91C1_POINTER_WRITE          0x0000
#define SM91C1_POINTER_AUTO_INCREMENT 0x4000
#define SM91C1_POINTER_TRANSMIT       0x0000
#define SM91C1_POINTER_RECEIVE        0x8000

//
// Define the flags for the interrupt register.
//

#define SM91C1_INTERRUPT_RECEIVE         0x01
#define SM91C1_INTERRUPT_TRANSMIT        0x02
#define SM91C1_INTERRUPT_TRANSMIT_EMPTY  0x04
#define SM91C1_INTERRUPT_ALLOCATE        0x08
#define SM91C1_INTERRUPT_RECEIVE_OVERRUN 0x10
#define SM91C1_INTERRUPT_EPH             0x20
#define SM91C1_INTERRUPT_MD              0x80

#define SM91C1_DEFAULT_INTERRUPTS       \
    (SM91C1_INTERRUPT_MD |              \
     SM91C1_INTERRUPT_EPH |             \
     SM91C1_INTERRUPT_TRANSMIT |        \
     SM91C1_INTERRUPT_RECEIVE_OVERRUN | \
     SM91C1_INTERRUPT_RECEIVE)

#define SM91C1_ACKNOWLEDGE_INTERRUPT_MASK \
    (SM91C1_INTERRUPT_MD |                \
     SM91C1_INTERRUPT_RECEIVE_OVERRUN |   \
     SM91C1_INTERRUPT_TRANSMIT_EMPTY |    \
     SM91C1_INTERRUPT_TRANSMIT)

//
// Define the flags for the management interface register.
//

#define SM91C1_MANAGEMENT_INTERFACE_MII_MDO           0x0001
#define SM91C1_MANAGEMENT_INTERFACE_MII_MDI           0x0002
#define SM91C1_MANAGEMENT_INTERFACE_MII_CLOCK         0x0004
#define SM91C1_MANAGEMENT_INTERFACE_MII_OUTPUT_ENABLE 0x0008
#define SM91C1_MANAGEMENT_INTERFACE_DISABLE_CRS100    0x4000

//
// Define the flags for the revision register.
//

#define SM91C1_REVISION_ID_MASK       0x000F
#define SM91C1_REVISION_ID_SHIFT      0
#define SM91C1_REVISION_CHIP_ID_MASK  0x00F0
#define SM91C1_REVISION_CHIP_ID_SHIFT 4

//
// Define the flags for the receive register.
//

#define SM91C1_RECEIVE_DISCARD 0x0080

//
// Define the flags used to parse a SM91C1_REGISTER value.
//

#define SM91C1_REGISTER_OFFSET_MASK      0x00F
#define SM91C1_REGISTER_OFFSET_SHIFT     0
#define SM91C1_REGISTER_BYTE_COUNT_MASK  0x0F0
#define SM91C1_REGISTER_BYTE_COUNT_SHIFT 4
#define SM91C1_REGISTER_BANK_MASK        0xF00
#define SM91C1_REGISTER_BANK_SHIFT       8

//
// Define SMSC91c111 Phy MII Basic Control register bits.
//

#define SM91C1_MII_BASIC_CONTROL_COLLISION_TEST          0x0080
#define SM91C1_MII_BASIC_CONTROL_FULL_DUPLEX             0x0100
#define SM91C1_MII_BASIC_CONTROL_RESTART_AUTONEGOTIATION 0x0200
#define SM91C1_MII_BASIC_CONTROL_ISOLATE                 0x0400
#define SM91C1_MII_BASIC_CONTROL_POWER_DOWN              0x0800
#define SM91C1_MII_BASIC_CONTROL_ENABLE_AUTONEGOTIATION  0x1000
#define SM91C1_MII_BASIC_CONTROL_SPEED_100               0x2000
#define SM91C1_MII_BASIC_CONTROL_LOOPBACK                0x4000
#define SM91C1_MII_BASIC_CONTROL_RESET                   0x8000

//
// Define SMSC91c111 Phy MII Basic Status register bits.
//

#define SM91C1_MII_BASIC_STATUS_EXTENDED_CAPABILITY         0x0001
#define SM91C1_MII_BASIC_STATUS_JABBER_DETECTED             0x0002
#define SM91C1_MII_BASIC_STATUS_LINK_STATUS                 0x0004
#define SM91C1_MII_BASIC_STATUS_AUTONEGOTIATE_CAPABLE       0x0008
#define SM91C1_MII_BASIC_STATUS_REMOTE_FAULT                0x0010
#define SM91C1_MII_BASIC_STATUS_AUTONEGOTIATE_COMPLETE      0x0020
#define SM91C1_MII_BASIC_STATUS_PREAMBLE_SUPRESSION_CAPABLE 0x0040

//
// This bit is set if there is extended status in register 0x0F.
//

#define SM91C1_MII_BASIC_STATUS_EXTENDED_STATUS             0x0100

//
// This bit is set if the PHY can do 100BASE-T2 half-duplex.
//

#define SM91C1_MII_BASIC_STATUS_100_HALF2                   0x0200

//
// This bit is set if the PHY can do 100BASE-T2 full-duplex.
//

#define SM91C1_MII_BASIC_STATUS_100_FULL2                   0x0400

//
// This bit is set if the PHY can do 10 Mbps half-duplex.
//

#define SM91C1_MII_BASIC_STATUS_10_HALF                     0x0800

//
// This bit is set if the PHY can do 10 Mbps full-duplex.
//

#define SM91C1_MII_BASIC_STATUS_10_FULL                     0x1000

//
// This bit is set if the PHY can do 100 Mbps, half-duplex.
//

#define SM91C1_MII_BASIC_STATUS_100_HALF                    0x2000

//
// This bit is set if the PHY can do 100 Mbps, full-duplex.
//

#define SM91C1_MII_BASIC_STATUS_100_FULL                    0x4000

//
// This bit is set if the PHY can do 100 Mbps with 4k packets.
//

#define SM91C1_MII_BASIC_STATUS_100_BASE4                   0x8000

//
// Define SMSC91c111 Phy MII Advertise register bits.
//

#define SM91C1_MII_ADVERTISE_CSMA             0x0001
#define SM91C1_MII_ADVERTISE_10_HALF          0x0020
#define SM91C1_MII_ADVERTISE_10_FULL          0x0040
#define SM91C1_MII_ADVERTISE_100_HALF         0x0080
#define SM91C1_MII_ADVERTISE_100_FULL         0x0100
#define SM91C1_MII_ADVERTISE_100_BASE4        0x0200
#define SM91C1_MII_ADVERTISE_REMOTE_FAULT     0x2000
#define SM91C1_MII_ADVERTISE_LINK_PARTNER_ACK 0x4000
#define SM91C1_MII_ADVERTISE_NEXT_PAGE        0x8000

#define SM91C1_MII_ADVERTISE_FULL    \
    (SM91C1_MII_ADVERTISE_100_FULL | \
     SM91C1_MII_ADVERTISE_10_FULL |  \
     SM91C1_MII_ADVERTISE_CSMA)

#define SM91C1_MII_ADVERTISE_ALL     \
    (SM91C1_MII_ADVERTISE_10_HALF |  \
     SM91C1_MII_ADVERTISE_10_FULL |  \
     SM91C1_MII_ADVERTISE_100_HALF | \
     SM91C1_MII_ADVERTISE_100_FULL)

//
// Define SMSC91c111 Phy MII Configuraiton 1 register bits.
//

#define SM91C1_MII_CONFIGURATION_1_UTP_CABLE           0x0000
#define SM91C1_MII_CONFIGURATION_1_STP_CABLE           0x0080
#define SM91C1_MII_CONFIGURATION_1_EQUALIZER_DISABLE   0x0100
#define SM91C1_MII_CONFIGURATION_1_UNSCRAMBLED_DISABLE 0x0200
#define SM91C1_MII_CONFIGURATION_1_BYPASS_SCRAMBLER    0x0400
#define SM91C1_MII_CONFIGURATION_1_TRANSMIT_POWER_DOWN 0x2000
#define SM91C1_MII_CONFIGURATION_1_TRANSMIT_DISABLE    0x4000
#define SM91C1_MII_CONFIGURATION_1_LINK_DISABLE        0x8000

//
// Define the SMSC91c111 Phy MII Interrupt Status register bits.
//

#define SM91C1_MII_INTERRUPT_STATUS_DUPLEX                0x0040
#define SM91C1_MII_INTERRUPT_STATUS_SPEED_100             0x0080
#define SM91C1_MII_INTERRUPT_STATUS_JABBER                0x0100
#define SM91C1_MII_INTERRUPT_STATUS_POLARITY              0x0200
#define SM91C1_MII_INTERRUPT_STATUS_END_OF_STREAM_ERROR   0x0400
#define SM91C1_MII_INTERRUPT_STATUS_START_OF_STREAM_ERROR 0x0800
#define SM91C1_MII_INTERRUPT_STATUS_CODEWORD              0x1000
#define SM91C1_MII_INTERRUPT_STATUS_LOST_SYNCHRONIZATION  0x2000
#define SM91C1_MII_INTERRUPT_STATUS_LINK_FAIL             0x4000
#define SM91C1_MII_INTERRUPT_STATUS_INTERRUPT             0x8000

//
// Define the number of 1's that need to be written to the MII interface to
// synchronize it into the IDLE state.
//

#define SM91C1_MII_SYNCHRONIZE_COUNT 32

//
// Define the size of an SM91c111 packet header and footer, in bytes.
//

#define SM91C1_PACKET_HEADER_SIZE 4
#define SM91C1_PACKET_FOOTER_SIZE 2
#define SM91C1_PACKET_CRC_SIZE 4

//
// Define the maximum packet size, including the headers, footers, and CRC.
//

#define SM91C1_MAX_PACKET_SIZE SM91C1_MEMORY_UNIT_SIZE

//
// Define the bits from the Smsc91c111 control byte.
//

#define SM91C1_CONTROL_BYTE_CRC 0x10
#define SM91C1_CONTROL_BYTE_ODD 0x20

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the SMS91C1 Phy MII register values.
// TODO: Refactor the generic MII registers and bit definitions to use mii.h.
//

typedef enum _SM91C1_MII_REGISTER {
    Sm91c1MiiRegisterBasicControl        = 0x00,
    Sm91c1MiiRegisterBasicStatus         = 0x01,
    Sm91c1MiiRegisterPhysicalId1         = 0x02,
    Sm91c1MiiRegisterPhysicalId2         = 0x03,
    Sm91c1MiiRegisterAdvertise           = 0x04,
    Sm91c1MiiRegisterRemoteEndCapability = 0x05,
    Sm91c1MiiRegisterConfiguration1      = 0x10,
    Sm91c1MiiRegisterConfiguration2      = 0x11,
    Sm91c1MiiRegisterInterrupt           = 0x12,
    Sm91c1MiiRegisterInterruptMask       = 0x13
} SM91C1_MII_REGISTER, *PSM91C1_MII_REGISTER;

//
// Define the SMSC91C1 register values. See the SM91C1_REGISTER_* mask to
// decode the register.
//

typedef enum _SM91C1_REGISTER {
    Sm91c1RegisterTransmitControl     = 0x020,
    Sm91c1RegisterEphStatus           = 0x022,
    Sm91c1RegisterReceiveControl      = 0x024,
    Sm91c1RegisterCounter             = 0x026,
    Sm91c1RegisterMemoryInformation   = 0x028,
    Sm91c1RegisterPhyControl          = 0x02A,
    Sm91c1RegisterBankSelect          = 0x02E,
    Sm91c1RegisterConfiguration       = 0x120,
    Sm91c1RegisterBaseAddress         = 0x122,
    Sm91c1RegisterIndividualAddress0  = 0x114,
    Sm91c1RegisterIndividualAddress1  = 0x115,
    Sm91c1RegisterIndividualAddress2  = 0x116,
    Sm91c1RegisterIndividualAddress3  = 0x117,
    Sm91c1RegisterIndividualAddress4  = 0x118,
    Sm91c1RegisterIndividualAddress5  = 0x119,
    Sm91c1RegisterGeneralPurpose      = 0x12A,
    Sm91c1RegisterControl             = 0x12C,
    Sm91c1RegisterMmuCommand          = 0x220,
    Sm91c1RegisterPacketNumber        = 0x212,
    Sm91c1RegisterAllocationResult    = 0x213,
    Sm91c1RegisterTransmitFifo        = 0x214,
    Sm91c1RegisterReceiveFifo         = 0x215,
    Sm91c1RegisterPointer             = 0x226,
    Sm91c1RegisterData                = 0x228,
    Sm91c1RegisterInterrupt           = 0x21C,
    Sm91c1RegisterInterruptMask       = 0x21D,
    Sm91c1RegisterMulticastTable0     = 0x310,
    Sm91c1RegisterMulticastTable1     = 0x311,
    Sm91c1RegisterMulticastTable2     = 0x312,
    Sm91c1RegisterMulticastTable3     = 0x313,
    Sm91c1RegisterMulticastTable4     = 0x314,
    Sm91c1RegisterMulticastTable5     = 0x315,
    Sm91c1RegisterMulticastTable6     = 0x316,
    Sm91c1RegisterMulticastTable7     = 0x317,
    Sm91c1RegisterManagementInterface = 0x328,
    Sm91c1RegisterRevision            = 0x32A,
    Sm91c1RegisterReceive             = 0x32C
} SM91C1_REGISTER, *PSM91C1_REGISTER;

/*++

Structure Description:

    This structure defines an SMSC91C1 LAN device.

Members:

    OsDevice - Stores a pointer to the OS device object.

    ControllerBase - Stores the virtual address of the memory mapping to the
        Smsc91c111's registers.

    NetworkLink - Stores a pointer to the core networking link.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    InterruptLock - Stores the spin lock, synchronized at the interrupt
        run level, that synchronizes access to the pending status bits, DPC,
        and work item.

    TransmitPacketList - Stores a list of network packets waiting to be sent.

    Lock - Stores a queued lock that protects access to the transmit packet
        list and various other values.

    ReceiveIoBuffer - Stores a pointer to the I/O buffer used to process a
        received packet.

    PendingTransmitPacket - Stores the packet number of the transmit packet for
        which status in pending.

    PendingInterrupts - Stores the bitmask of pending interrupts. See
        SM91C1_INTERRUPT_* for definitions.

    PendingPhyInterrupts - Stores the bitmask of pending MII interrupts. See
        SM91C1_MII_INTERRUPT_STATUS_* for definitions.

    SupportedCapabilities - Stores the set of capabilities that this device
        supports. See NET_LINK_CAPABILITY_* for definitions.

    EnabledCapabilities - Stores the currently enabled capabilities on the
        devices. See NET_LINK_CAPABILITY_* for definitions.

    AllocateInProgress - Stores a boolean indicating whether or not packet
        allocation is in progress. This is protected by the Lock member.

    BankLock - Stores a lock that synchronizes access to the Smsc91c111
        registers. It must be acquired ad the interrupt run level as the ISR
        reads the Smsc91c111 registers as well.

    SelectedBank - Stores the currently selected register bank.

    MacAddress - Stores the default MAC address of the device.

--*/

typedef struct _SM91C1_DEVICE {
    PDEVICE OsDevice;
    PVOID ControllerBase;
    PNET_LINK NetworkLink;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    KSPIN_LOCK InterruptLock;
    NET_PACKET_LIST TransmitPacketList;
    PQUEUED_LOCK Lock;
    PIO_BUFFER ReceiveIoBuffer;
    USHORT PendingTransmitPacket;
    volatile ULONG PendingInterrupts;
    volatile ULONG PendingPhyInterrupts;
    ULONG SupportedCapabilities;
    ULONG EnabledCapabilities;
    BOOL AllocateInProgress;
    KSPIN_LOCK BankLock;
    BYTE SelectedBank;
    BYTE MacAddress[ETHERNET_ADDRESS_SIZE];
} SM91C1_DEVICE, *PSM91C1_DEVICE;

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
Sm91c1Send (
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
Sm91c1GetSetInformation (
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
Sm91c1pInitializeDeviceStructures (
    PSM91C1_DEVICE Device
    );

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an SM91C1 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

VOID
Sm91c1pDestroyDeviceStructures (
    PSM91C1_DEVICE Device
    );

/*++

Routine Description:

    This routine performs destroy any device structures allocated for the
    SM91C1 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

KSTATUS
Sm91c1pInitialize (
    PSM91C1_DEVICE Device
    );

/*++

Routine Description:

    This routine initializes and enables the SM91C1 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
Sm91c1pInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the SM91C1 interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
Sm91c1pInterruptServiceWorker (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the SM91C1 low level interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the SM91c1 device
        structure.

Return Value:

    Interrupt status.

--*/

//
// Administrative functions called by the hardware side.
//

KSTATUS
Sm91c1pAddNetworkDevice (
    PSM91C1_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

