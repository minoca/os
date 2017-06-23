/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    e100.h

Abstract:

    This header contains internal definitions for the Intel e100 Integrated
    LAN driver (i8255x compatible).

Author:

    Evan Green 4-Apr-2013

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

#define E100_READ_REGISTER32(_Controller, _Register) \
    HlReadRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define E100_READ_REGISTER16(_Controller, _Register) \
    HlReadRegister16((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define E100_READ_REGISTER8(_Controller, _Register)  \
    HlReadRegister8((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define E100_WRITE_REGISTER32(_Controller, _Register, _Value)              \
    HlWriteRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

#define E100_WRITE_REGISTER16(_Controller, _Register, _Value)              \
    HlWriteRegister16((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

#define E100_WRITE_REGISTER8(_Controller, _Register, _Value)               \
    HlWriteRegister8((PUCHAR)(_Controller)->ControllerBase + (_Register),  \
                     (_Value))

//
// Define macros for writing specifically to the SCB command and status
// registers.
//

#define E100_READ_COMMAND_REGISTER(_Controller) \
    E100_READ_REGISTER16(_Controller, E100RegisterCommand)

#define E100_WRITE_COMMAND_REGISTER(_Controller, _Value) \
    E100_WRITE_REGISTER16(_Controller, E100RegisterCommand, _Value)

#define E100_READ_STATUS_REGISTER(_Controller) \
    E100_READ_REGISTER16(_Controller, E100RegisterStatus)

#define E100_WRITE_STATUS_REGISTER(_Controller, _Value) \
    E100_WRITE_REGISTER16(_Controller, E100RegisterStatus, _Value)

//
// Define macros for incrementing the ring variables.
//

#define E100_INCREMENT_RING_INDEX(_Index, _PowerOf2Size) \
    (((_Index) + 1) & ((_PowerOf2Size) - 1))

#define E100_DECREMENT_RING_INDEX(_Index, _PowerOf2Size) \
    (((_Index) - 1) & ((_PowerOf2Size) - 1))

//
// ---------------------------------------------------------------- Definitions
//

#define E100_ALLOCATION_TAG 0x30303145 // '001E'

//
// Define how often to check the link for connect/disconnect, in seconds.
//

#define E100_LINK_CHECK_INTERVAL 5

//
// Define the size of receive frame data.
//

#define RECEIVE_FRAME_DATA_SIZE 1520

//
// Define the number of commands that can exist in the command ring.
//

#define E100_COMMAND_RING_COUNT 32

//
// Define the number of receive buffers that will be allocated for the
// controller.
//

#define E100_RECEIVE_FRAME_COUNT 32

//
// Define the amount of time to wait in microseconds for the status to move to
// ready.
//

#define E100_READY_TIMEOUT MICROSECONDS_PER_SECOND

//
// Define how often, in microseconds, the link is checked.
//

#define E100_LINK_CHECK_PERIOD_MICROSECONDS (5 * MICROSECONDS_PER_SECOND)

//
// Define how long to wait for a free command descriptor before just giving
// up and trying anyway.
//

#define E100_COMMAND_BLOCK_WAIT_INTERVAL WAIT_TIME_INDEFINITE

//
// Define SCB status register bits.
//

#define E100_STATUS_COMMAND_COMPLETE                        (1 << 15)
#define E100_STATUS_FRAME_RECEIVED                          (1 << 14)
#define E100_STATUS_COMMAND_NOT_ACTIVE                      (1 << 13)
#define E100_STATUS_RECEIVE_NOT_READY                       (1 << 12)
#define E100_STATUS_MDI_CYCLE_COMPLETE                      (1 << 11)
#define E100_STATUS_SOFTWARE_INTERRUPT                      (1 << 10)
#define E100_STATUS_FLOW_CONTROL_PAUSE                      (1 << 8)
#define E100_STATUS_INTERRUPT_MASK    \
    (E100_STATUS_COMMAND_COMPLETE |   \
     E100_STATUS_FRAME_RECEIVED |     \
     E100_STATUS_COMMAND_NOT_ACTIVE | \
     E100_STATUS_RECEIVE_NOT_READY |  \
     E100_STATUS_MDI_CYCLE_COMPLETE | \
     E100_STATUS_SOFTWARE_INTERRUPT | \
     E100_STATUS_FLOW_CONTROL_PAUSE)

#define E100_STATUS_COMMAND_UNIT_STATUS_MASK                0x000000C0
#define E100_STATUS_COMMAND_UNIT_IDLE                       0x00000000
#define E100_STATUS_COMMAND_UNIT_SUSPENDED                  0x00000040
#define E100_STATUS_COMMAND_UNIT_LOW_PRIORITY_QUEUE_ACTIVE  0x00000080
#define E100_STATUS_COMMAND_UNIT_HIGH_PRIORITY_QUEUE_ACTIVE 0x000000C0
#define E100_STATUS_RECEIVE_UNIT_STATUS_MASK                0x0000003C
#define E100_STATUS_RECEIVE_UNIT_IDLE                       0x00000000
#define E100_STATUS_RECEIVE_UNIT_SUSPENDED                  0x00000004
#define E100_STATUS_RECEIVE_UNIT_NO_RESOURCES               0x00000008
#define E100_STATUS_RECEIVE_UNIT_READY                      0x00000010

//
// Define SCB command register bits, assuming the command register is accessed
// aligned to 2 bytes (just the command register).
//

#define E100_COMMAND_MASK_COMMAND_COMPLETE        (1 << 15)
#define E100_COMMAND_MASK_FRAME_RECEIVED          (1 << 14)
#define E100_COMMAND_MASK_COMMAND_NOT_ACTIVE      (1 << 13)
#define E100_COMMAND_MASK_RECEIVE_NOT_READY       (1 << 12)
#define E100_COMMAND_MASK_EARLY_RECEIVE           (1 << 11)
#define E100_COMMAND_MASK_FLOW_CONTROL_PAUSE      (1 << 10)
#define E100_COMMAND_GENERATE_SOFTWARE_INTERRUPT  (1 << 9)
#define E100_COMMAND_GLOBAL_MASK                  (1 << 8)
#define E100_COMMAND_NOP                          (0x0 << 4)
#define E100_COMMAND_UNIT_START                   (0x1 << 4)
#define E100_COMMAND_UNIT_RESUME                  (0x2 << 4)
#define E100_COMMAND_UNIT_LOAD_DUMP_BASE          (0x4 << 4)
#define E100_COMMAND_UNIT_DUMP_COUNTERS           (0x5 << 4)
#define E100_COMMAND_UNIT_LOAD_BASE               (0x6 << 4)
#define E100_COMMAND_UNIT_DUMP_AND_RESET_COUNTERS (0x7 << 4)
#define E100_COMMAND_UNIT_STATIC_RESUME           (0xA << 4)
#define E100_COMMAND_UNIT_COMMAND_MASK            (0xF << 4)
#define E100_COMMAND_REGISTER_COMMAND_SHIFT       4
#define E100_COMMAND_RECEIVE_NOP                  0x0000
#define E100_COMMAND_RECEIVE_START                0x0001
#define E100_COMMAND_RECEIVE_RESUME               0x0002
#define E100_COMMAND_RECEIVE_DMA_REDIRECT         0x0003
#define E100_COMMAND_RECEIVE_ABORT                0x0004
#define E100_COMMAND_RECEIVE_LOAD_HEADER_SIZE     0x0005
#define E100_COMMAND_RECEIVE_LOAD_BASE            0x0006
#define E100_COMMAND_RECEIVE_COMMAND_MASK         0x0007

//
// Define E100 command bits.
//

#define E100_COMMAND_END_OF_LIST     0x80000000
#define E100_COMMAND_SUSPEND         0x40000000
#define E100_COMMAND_INTERRUPT       0x20000000
#define E100_COMMAND_COMPLETE        0x00008000
#define E100_COMMAND_OK              0x00002000
#define E100_COMMAND_SELF_TEST_PASS  0x00000800
#define E100_COMMAND_BLOCK_COMMAND_SHIFT 16
#define E100_COMMAND_BLOCK_COMMAND_MASK 0x00070000

//
// Define command bits specific to the transmit command.
//

#define E100_COMMAND_TRANSMIT_INTERRUPT_DELAY_SHIFT    24
#define E100_COMMAND_TRANSMIT_NO_CRC_OR_SOURCE_ADDRESS 0x00100000
#define E100_COMMAND_TRANSMIT_FLEXIBLE_MODE            0x00080000
#define E100_COMMAND_TRANSMIT_UNDERRUN                 0x00001000

//
// Define transmit buffer descriptor property bits.
//

#define E100_TRANSMIT_BUFFER_DESCRIPTOR_COUNT_SHIFT    24
#define E100_TRANSMIT_THRESHOLD                        (2 << 16)
#define E100_TRANSMIT_LENGTH_MASK                      0x00003FFF

#define E100_TRANSMIT_BUFFER_END_OF_LIST               0x00010000

//
// Define receive command bits.
//

#define E100_RECEIVE_COMMAND_END_OF_LIST   (1 << 31)
#define E100_RECEIVE_COMMAND_SUSPEND       (1 << 30)
#define E100_RECEIVE_COMMAND_HEADER_ONLY   (1 << 20)
#define E100_RECEIVE_COMMAND_FLEXIBLE_MODE (1 << 19)

//
// Define receive frame status bits.
//

#define E100_RECEIVE_COMPLETE         0x00008000
#define E100_RECEIVE_OK               0x00002000
#define E100_RECEIVE_CRC_ERROR        0x00000800
#define E100_RECEIVE_ALIGNMENT_ERROR  0x00000400
#define E100_RECEIVE_BUFFER_TOO_SMALL 0x00000200
#define E100_RECEIVE_DMA_OVERRUN      0x00000100
#define E100_RECEIVE_FRAME_TOO_SHORT  0x00000080
#define E100_RECEIVE_TYPE_FRAME       0x00000020
#define E100_RECEIVE_ERROR            0x00000010
#define E100_RECEIVE_NO_ADDRESS_MATCH 0x00000004
#define E100_RECEIVE_INDIVIDUAL_MATCH 0x00000002
#define E100_RECEIVE_COLLISION        0x00000001

//
// Define receive sizes bitfields.
//

#define E100_RECEIVE_SIZE_FRAME_COMPLETE    0x00008000
#define E100_RECEIVE_SIZE_UPDATED           0x00004000
#define E100_RECEIVE_SIZE_ACTUAL_COUNT_MASK 0x00003FFF
#define E100_RECEIVE_SIZE_BUFFER_SIZE_SHIFT 16

//
// Define EEPROM control register definitions.
//

#define E100_EEPROM_DATA_OUT    0x0008
#define E100_EEPROM_DATA_IN     0x0004
#define E100_EEPROM_CHIP_SELECT 0x0002
#define E100_EEPROM_CLOCK       0x0001

//
// Define the number of bits in the EEPROM opcode. It's actually only a 2 bit
// opcode, but there's also a start bit that just gets glued in there as if it
// were an opcode.
//

#define E100_EEPROM_OPCODE_LENGTH 3
#define E100_EEPROM_OPCODE_READ   6
#define E100_EEPROM_OPCODE_WRITE  5
#define E100_EEPROM_DELAY_MICROSECONDS 10
#define E100_EEPROM_INDIVIDUAL_ADDRESS_OFFSET 0

//
// Define the EEPROM PHY device record offset and bit mask information.
//

#define E100_EEPROM_PHY_DEVICE_RECORD_OFFSET 6
#define E100_EEPROM_PHY_DEVICE_RECORD_10MBPS_ONLY   0x8000
#define E100_EEPROM_PHY_DEVICE_RECORD_VENDOR_CODE   0x4000
#define E100_EEPROM_PHY_DEVICE_RECORD_CODE_MASK     0x3F00
#define E100_EEPROM_PHY_DEVICE_RECORD_CODE_SHIFT    8
#define E100_EEPROM_PHY_DEVICE_RECORD_ADDRESS_MASK  0x00FF
#define E100_EEPROM_PHY_DEVICE_RECORD_ADDRESS_SHIFT 0

#define E100_EEPROM_PHY_DEVICE_CODE_NO_PHY      0x0
#define E100_EEPROM_PHY_DEVICE_CODE_I82553AB    0x1
#define E100_EEPROM_PHY_DEVICE_CODE_I82553C     0x2
#define E100_EEPROM_PHY_DEVICE_CODE_I82503      0x3
#define E100_EEPROM_PHY_DEVICE_CODE_DP83840     0x4
#define E100_EEPROM_PHY_DEVICE_CODE_S80C240     0x5
#define E100_EEPROM_PHY_DEVICE_CODE_S80C24      0x6
#define E100_EEPROM_PHY_DEVICE_CODE_I82555      0x7
#define E100_EEPROM_PHY_DEVICE_CODE_MICROLINEAR 0x8
#define E100_EEPROM_PHY_DEVICE_CODE_LEVEL_ONE   0x9
#define E100_EEPROM_PHY_DEVICE_CODE_DP83840A    0xA
#define E100_EEPROM_PHY_DEVICE_CODE_ICS1890     0xB

//
// Define PORT opcodes.
//

#define E100_PORT_RESET           0x00000000
#define E100_PORT_SELF_TEST       0x00000001
#define E100_PORT_SELECTIVE_RESET 0x00000002
#define E100_PORT_DUMP            0x00000003
#define E100_PORT_DUMP_WAKE_UP    0x00000007

//
// Define the number of microseconds after issuing a PORT reset to wait before
// accessing the controller again.
//

#define E100_PORT_RESET_DELAY_MICROSECONDS 10

//
// Define general status register bits.
//

#define E100_CONTROL_STATUS_LINK_UP  0x01
#define E100_CONTROL_STATUS_100_MBPS 0x02

//
// Define the E100 revision IDs.
//

#define E100_REVISION_82557_A   0x01
#define E100_REVISION_82557_B   0x02
#define E100_REVISION_82557_C   0x03
#define E100_REVISION_82558_A   0x04
#define E100_REVISION_82558_B   0x05
#define E100_REVISION_82559_A   0x06
#define E100_REVISION_82559_B   0x07
#define E100_REVISION_82559_C   0x08
#define E100_REVISION_82559ER_A 0x09
#define E100_REVISION_82550_A   0x0C
#define E100_REVISION_82550_B   0x0D
#define E100_REVISION_82550_C   0x0E
#define E100_REVISION_82551_A   0x0F
#define E100_REVISION_82551_B   0x10

//
// Define the E100 configuration values.
//

#define E100_CONFIG_BYTE3_MWI_ENABLE 0x01

#define E100_CONFIG_BYTE6_SAVE_BAD_FRAMES 0x80

#define E100_CONFIG_BYTE7_DISCARD_SHORT_RECEIVE 0x01

#define E100_CONFIG_BYTE8_MII_MODE 0x01

#define E100_CONFIG_BYTE12_LINEAR_PRIORITY_MODE 0x01

#define E100_CONFIG_BYTE15_CRS_OR_CDT  0x80
#define E100_CONFIG_BYTE15_PROMISCUOUS 0x01

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the SCB register offsets, in bytes.
//

typedef enum _E100_REGISTER {
    E100RegisterStatus              = 0x0,
    E100RegisterAcknowledge         = 0x1,
    E100RegisterCommand             = 0x2,
    E100RegisterPointer             = 0x4,
    E100RegisterPort                = 0x8,
    E100RegisterEepromControl       = 0xE,
    E100RegisterMdiControl          = 0x10,
    E100RegisterReceiveDmaByteCount = 0x14,
    E100RegisterFlowControl         = 0x18,
    E100RegisterControl             = 0x1C,
    E100RegisterGeneralStatus       = 0x1D,
    E100RegisterFunctionEvent       = 0x30,
    E100RegisterFunctionEventMask   = 0x34,
    E100RegisterFunctionStatus      = 0x38,
    E100RegisterForceEvent          = 0x3C,
} E100_REGISTER, *PE100_REGISTER;

typedef enum _E100_COMMAND_TYPE {
    E100CommandNop                  = 0x0,
    E100CommandSetIndividualAddress = 0x1,
    E100CommandConfigure            = 0x2,
    E100CommandMulticastSetup       = 0x3,
    E100CommandTransmit             = 0x4,
    E100CommandLoadMicrocode        = 0x5,
    E100CommandDump                 = 0x6,
    E100CommandDiagnose             = 0x7
} E100_COMMAND_TYPE, *PE100_COMMAND_TYPE;

/*++

Structure Description:

    This structure defines the hardware mandated command format.

Members:

    Command - Stores overall command information. The controller also reports
        status information in this field when a command is executed.

    NextCommand - Stores the physical address of the next command.

    Address - Stores the MAC address to program into the NIC if the command is
        a "Set Address" command.

    Configuration - Stores the configuration bytes of the NIC if the command
        is a "Configure" command.

    AddressList - Stores the list of MAC addresses that the NIC should
        respond to if the command is a "Multicast Setup" command.

    DescriptorAddress - Stores the location of the transmit Buffer Descriptor
        array, if the command is a "Transmit" command. In this case, the
        buffer descriptor is always immediately after these fields (the
        buffer address field is the first and only buffer descriptor).

    DescriptorProperties - Stores properties about the transmit buffer
        descriptor if the command is a "Transmit" command. The most important
        property is the buffer descriptor count, which for this implementation
        is always 1.

    BufferAddress - Stores the physical address of the data to transmit if the
        command is a "Transmit" command. This is the first element of the first
        and only buffer descriptor.

    BufferProperties - Stores properties (such as the buffer length) of the
        buffer above, if the command is a "Transmit" command.

    BufferVirtual - Stores the virtual address of the transmit buffer. This is
        not used by software, not hardware.

    BufferAddress - Stores the address where the NICs internal state should be
        dumped to, if the command is a "Dump" command. This is generally only
        useful for debugging and diagnostics.

--*/

typedef struct _E100_COMMAND {
    volatile ULONG Command;
    ULONG NextCommand;
    union {
        struct {
            UCHAR Address[6];
        } PACKED SetAddress;

        struct {
            UCHAR Configuration[24];
        } PACKED Configure;

        struct {
            UCHAR AddressList[24];
        } PACKED MulticastSetup;

        struct {
            ULONG DescriptorAddress;
            ULONG DescriptorProperties;
            ULONG BufferAddress;
            ULONG BufferProperties;
            PVOID BufferVirtual;
        } PACKED Transmit;

        struct {
            ULONG BufferAddress;
        } PACKED Dump;
    } U;

} PACKED E100_COMMAND, *PE100_COMMAND;

/*++

Structure Description:

    This structure defines the hardware mandated format for a receive frame.

Members:

    Status - Stores status information written by the device about the frame.

    NextFrame - Stores the physical address of the next receive frame.

    Reserved - Stores a reserved area.

    Sizes - Stores the size of the buffer, as well as the size of the data
        actually received.

    ReceiveFrame - Stores the receive data.

--*/

typedef struct _E100_RECEIVE_FRAME {
    ULONG Status;
    ULONG NextFrame;
    ULONG Reserved;
    ULONG Sizes;
    ULONG ReceiveFrame[RECEIVE_FRAME_DATA_SIZE / sizeof(ULONG)];
} PACKED E100_RECEIVE_FRAME, *PE100_RECEIVE_FRAME;

/*++

Structure Description:

    This structure defines an Intel e100 LAN device.

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

    ControllerBase - Stores the virtual address of the memory mapping to the
        E100's registers.

    NetworkLink - Stores a pointer to the core networking link.

    ReceiveFrameIoBuffer - Stores a pointer to the I/O buffer associated with
        the receive frames.

    ReceiveFrame - Stores the pointer to the array of receive frames.

    ReceiveListBegin - Stores the index of the beginning of the list, which is
        the oldest received frame and the first one to dispatch.

    ReceiveListLock - Stores a pointer to a queued lock that protects the
        received list.

    CommandPhysicalAddress - Stores the physical address of the base of the
        command list (called a list but is really an array).

    CommandIoBuffer - Stores a pointer to the I/O buffer associated with
        the command block list.

    Command - Stores a pointer to the command list (array).

    CommandPacket - Stores a pointer to the array of net packet buffers that
        go with each command.

    CommandLastReaped - Stores the index of the last command that was reaped.

    CommandNextToUse - Stores the index where the next command should be placed.
        If this equals the next index to be reaped, then the list is full.

    CommandFreeCount - Stores the number of command ring entries that are
        currently free to use.

    CommandListLock - Stores the lock protecting simultaneous software access
        to the command list.

    TransmitPacketList - Stores a list of network packets waiting to be sent.

    LinkActive - Stores a boolean indicating if there is an active network link.

    LinkSpeed - Stores a the current link speed of the device.

    LinkCheckTimer - Stores a pointer to the timer that fires periodically to
        see if the link is active.

    LinkCheckDpc - Stores a pointer to the DPC associated with the link check
        timer.

    WorkItem - Stores a pointer to the work item queued from the DPC.

    PendingStatusBits - Stores the bitfield of status bits that have yet to be
        dealt with by software.

    EepromAddressBits - Stores the number of addressing bits the EEPROM
        supports.

    EepromMacAddress - Stores the default MAC address of the device.

    SupportedCapabilities - Stores the set of capabilities that this device
        supports. See NET_LINK_CAPABILITY_* for definitions.

    EnabledCapabilities - Stores the currently enabled capabilities on the
        devices. See NET_LINK_CAPABILITY_* for definitions.

    ConfigurationLock - Stores a queued lock that synchronizes changes to the
        enabled capabilities field and their supporting hardware registers.

    PciConfigInterface - Stores the interface to access PCI configuration space.

    PciConfigInterfaceAvailable - Stores a boolean indicating if the PCI
        config interface is actively available.

    RegisteredForPciConfigInterfaces - Stores a boolean indicating whether or
        not the driver has regsistered for PCI Configuration Space interface
        access.

    Revision - Stores the E100 device revision gathered from PCI configuration
        space.

    MiiPresent - Stores a boolean indicating whether or not a MII is present.

--*/

typedef struct _E100_DEVICE {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PNET_LINK NetworkLink;
    PIO_BUFFER ReceiveFrameIoBuffer;
    PE100_RECEIVE_FRAME ReceiveFrame;
    ULONG ReceiveListBegin;
    PQUEUED_LOCK ReceiveListLock;
    PIO_BUFFER CommandIoBuffer;
    PE100_COMMAND Command;
    PNET_PACKET_BUFFER *CommandPacket;
    ULONG CommandLastReaped;
    ULONG CommandNextToUse;
    ULONG CommandFreeCount;
    PQUEUED_LOCK CommandListLock;
    NET_PACKET_LIST TransmitPacketList;
    BOOL LinkActive;
    ULONGLONG LinkSpeed;
    PKTIMER LinkCheckTimer;
    PDPC LinkCheckDpc;
    PWORK_ITEM WorkItem;
    ULONG PendingStatusBits;
    ULONG EepromAddressBits;
    BYTE EepromMacAddress[ETHERNET_ADDRESS_SIZE];
    ULONG SupportedCapabilities;
    ULONG EnabledCapabilities;
    PQUEUED_LOCK ConfigurationLock;
    INTERFACE_PCI_CONFIG_ACCESS PciConfigInterface;
    BOOL PciConfigInterfaceAvailable;
    BOOL RegisteredForPciConfigInterfaces;
    ULONG Revision;
    BOOL MiiPresent;
} E100_DEVICE, *PE100_DEVICE;

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
E100Send (
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
E100GetSetInformation (
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
E100pInitializeDeviceStructures (
    PE100_DEVICE Device
    );

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an E100 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

KSTATUS
E100pResetDevice (
    PE100_DEVICE Device
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
E100pInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the e100 interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
E100pInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine processes interrupts for the e100 controller at low level.

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
E100pAddNetworkDevice (
    PE100_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

