/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smsc95.h

Abstract:

    This header contains definitions for the SMSC95xx family of USB Ethernet
    Controllers.

Author:

    Evan Green 7-Nov-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/net/mii.h>

//
// ---------------------------------------------------------------- Definitions
//

#define SM95_ALLOCATION_TAG 0x35396D53 // '59mS'

//
// Define the maximum size of the control transfer data.
//

#define SM95_MAX_CONTROL_TRANSFER_SIZE (sizeof(USB_SETUP_PACKET) + 8)

//
// Define the maximum size of interrupt transfer data. Align up to cache size.
//

#define SM95_MAX_INTERRUPT_TRANSFER_SIZE 32

//
// Define the number of bytes needed at the front of every transmit packet.
//

#define SM95_TRANSMIT_HEADER_SIZE 8

//
// Define how long to wait for the PHY to finish before timing out, in seconds.
//

#define SM95_DEVICE_TIMEOUT 1

//
// Define how long to wait for the EEPROM to finish before timing out, in
// seconds.
//

#define SM95_EEPROM_TIMEOUT 1

//
// Define the fixed device ID of the PHY on the internal bus of the SMSC95xx.
//

#define SM95_PHY_ID 1

//
// Define the offset in the EEPROM where the MAC address is stored.
//

#define SM95_EEPROM_MAC_ADDRESS 0x01

//
// Define the status bits coming from the interrupt endpoint.
//

#define SM95_INTERRUPT_STATUS_PHY 0x00008000
#define SM95_INTERRUPT_MASK       0xFFFFFFFF

//
// Define the maximum size of single packet, including any headers and footers.
//

#define SM95_MAX_PACKET_SIZE 2048

//
// Define the maximum burst size for high speed and full speed devices.
//

#define SM95_HIGH_SPEED_TRANSFER_SIZE 512
#define SM95_FULL_SPEED_TRANSFER_SIZE 64
#define SM95_HIGH_SPEED_BURST_SIZE \
    ((16 * 1024) + (5 * SM95_HIGH_SPEED_TRANSFER_SIZE))

#define SM95_FULL_SPEED_BURST_SIZE \
    ((6 * 1024) + (33 * SM95_FULL_SPEED_TRANSFER_SIZE))

#define SM95_DEFAULT_BULK_IN_DELAY 0x00002000

//
// Define vendor-specific requests on the control endpoint.
//

#define SM95_VENDOR_REQUEST_WRITE_REGISTER 0xA0
#define SM95_VENDOR_REQUEST_READ_REGISTER  0xA1

//
// Define transmit control register bits.
//

#define SM95_TRANSMIT_CONTROL_ENABLE 0x00000004

//
// Define Hardware Configuration register bits.
//

#define SM95_HARDWARE_CONFIG_LITE_RESET               0x00000008
#define SM95_HARDWARE_CONFIG_BURST_CAP_ENABLED        0x00000002
#define SM95_HARDWARE_CONFIG_MULTIPLE_ETHERNET_FRAMES 0x00000020
#define SM95_HARDWARE_CONFIG_RX_DATA_OFFSET_MASK      0x00000600
#define SM95_HARDWARE_CONFIG_RX_DATA_OFFSET_SHIFT     9
#define SM95_HARDWARE_CONFIG_BULK_IN_EMPTY_RESPONSE   0x00001000

//
// Define the offset after the SM95 receive header at which the Ethernet frame
// should begin.
//

#define SM95_RECEIVE_DATA_OFFSET 2

//
// Define Power Control register bits.
//

#define SM95_POWER_CONTROL_PHY_RESET 0x00000010

//
// Define LED GPIO Configuration register bits.
//

#define SM95_LED_GPIO_CONFIG_SPEED_LED       0x01000000
#define SM95_LED_GPIO_CONFIG_LINK_LED        0x00100000
#define SM95_LED_GPIO_CONFIG_FULL_DUPLEX_LED 0x00010000

//
// Define Interrupt Endpoint Control register bits.
//

#define SM95_INTERRUPT_ENDPOINT_CONTROL_PHY_INTERRUPTS 0x00008000

//
// Define an auto-flow control default with a high water mark of 15.5KB, a
// low water mark of 3KB, and a backpressure duration of about 350us.
//

#define SM95_AUTO_FLOW_CONTROL_DEFAULT 0x00F830A1

//
// Define MAC control register bits.
//

#define SM95_MAC_CONTROL_RECEIVE_ALL     0x80000000
#define SM95_MAC_CONTROL_RECEIVE_OWN     0x00800000
#define SM95_MAC_CONTROL_LOOPBACK        0x00200000
#define SM95_MAC_CONTROL_FULL_DUPLEX     0x00100000
#define SM95_MAC_CONTROL_MULTICAST_PAS   0x00080000
#define SM95_MAC_CONTROL_PROMISCUOUS     0x00040000
#define SM95_MAC_CONTROL_PASS_BAD        0x00010000
#define SM95_MAC_CONTROL_HP_FILTER       0x00002000
#define SM95_MAC_CONTROL_ENABLE_TRANSMIT 0x00000008
#define SM95_MAC_CONTROL_ENABLE_RECEIVE  0x00000004

//
// Define MII address register bits.
//

#define SM95_MII_ADDRESS_BUSY         0x00000001
#define SM95_MII_ADDRESS_WRITE        0x00000002
#define SM95_MII_ADDRESS_PHY_ID_SHIFT 11
#define SM95_MII_ADDRESS_INDEX_SHIFT  6

//
// Define EEPROM command register bits.
//

#define SM95_EEPROM_COMMAND_BUSY         0x80000000
#define SM95_EEPROM_COMMAND_TIMEOUT      0x00000400
#define SM95_EEPROM_COMMAND_LOADED       0x00000200
#define SM95_EEPROM_COMMAND_ADDRESS_MASK 0x000001FF

//
// Define checksum offload control register bits.
//

#define SM95_CHECKSUM_CONTROL_TRANSMIT_ENABLE 0x00010000
#define SM95_CHECKSUM_CONTROL_RECEIVE_ENABLE  0x00000001

//
// Define the VLAN1 register value for 802.1Q extended headers.
//

#define SM95_VLAN_8021Q 0x8100

//
// Define PHY Interrupt mask bits.
//

#define SM95_PHY_INTERRUPT_AUTONEGOTIATION_COMPLETE 0x0040
#define SM95_PHY_INTERRUPT_LINK_DOWN                0x0010

//
// Define transmit packet flags.
//

#define SM95_TRANSMIT_FLAG_FIRST_SEGMENT 0x00002000
#define SM95_TRANSMIT_FLAG_LAST_SEGMENT  0x00001000

//
// Define receive packet flags.
//

#define SM95_RECEIVE_FLAG_CRC_ERROR        0x00000002
#define SM95_RECEIVE_FLAG_DRIBBLING_BIT    0x00000004
#define SM95_RECEIVE_FLAG_MII_ERROR        0x00000008
#define SM95_RECEIVE_FLAG_WATCHDOG_TIMEOUT 0x00000010
#define SM95_RECEIVE_FLAG_ETHERNET_FRAME   0x00000020
#define SM95_RECEIVE_FLAG_COLLISION        0x00000040
#define SM95_RECEIVE_FLAG_FRAME_TOO_LONG   0x00000080
#define SM95_RECEIVE_FLAG_MULTICAST_FRAME  0x00000400
#define SM95_RECEIVE_FLAG_RUNT_FRAME       0x00000800
#define SM95_RECEIVE_FLAG_LENGTH_ERROR     0x00001000
#define SM95_RECEIVE_FLAG_BROADCAST_FRAME  0x00002000
#define SM95_RECEIVE_FLAG_ERROR_SUMMARY    0x00008000
#define SM95_RECEIVE_FRAME_LENGTH_MASK     0x3FFF0000
#define SM95_RECEIVE_FRAME_LENGTH_SHIFT    16

//
// Define the number of bulk IN transfer to allocate.
//

#define SM95_BULK_IN_TRANSFER_COUNT 5

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define PHY registers specific to the SMSC95xx device.
//

typedef enum _SM95_PHY_REGISTER {
    Sm95PhyRegisterInterruptSource = 0x1D,
    Sm95PhyRegisterInterruptMask   = 0x1E,
} SM95_PHY_REGISTER, *PSM95_PHY_REGISTER;

typedef enum _SM95_REGISTER {
    Sm95RegisterIdRevision      = 0x00,
    Sm95RegisterInterruptStatus = 0x08,
    Sm95RegisterTransmitControl = 0x10,
    Sm95RegisterHardwareConfig  = 0x14,
    Sm95RegisterPowerControl    = 0x20,
    Sm95RegisterLedGpioConfig   = 0x24,
    Sm95RegisterAutoFlowControl = 0x2C,
    Sm95RegisterEepromCommand   = 0x30,
    Sm95RegisterEepromData      = 0x34,
    Sm95RegisterBurstCapability = 0x38,
    Sm95RegisterInterruptEndpointControl = 0x68,
    Sm95RegisterBulkInDelay     = 0x6C,
    Sm95RegisterMacControl      = 0x100,
    Sm95RegisterMacAddressHigh  = 0x104,
    Sm95RegisterMacAddressLow   = 0x108,
    Sm95RegisterMiiAddress      = 0x114,
    Sm95RegisterMiiData         = 0x118,
    Sm95RegisterFlowControl     = 0x11C,
    Sm95RegisterVlan1           = 0x120,
    Sm95RegisterChecksumOffloadControl = 0x130,
} SM95_REGISTER, *PSM95_REGISTER;

/*++

Structure Description:

    This structure defines an SMSC95xx LAN device.

Members:

    OsDevice - Stores a pointer to the system device object.

    NetworkLink - Stores a pointer to the core networking link.

    UsbCoreHandle - Stores the handle returned by the USB core.

    ReferenceCount - Stores the reference count for the device.

    IoBuffer - Stores a pointer to the I/O buffer used for both the bulk
        receive and the control transfers.

    ControlTransfer - Stores a pointer to the control transfer used for
        register reads and writes.

    InterruptTransfer - Stores a pointer to the interrupt transfer used to get
        notifications from the device.

    BulkInTransfer - Stores an array of pointers to transfers used to receive
        packets.

    BulkOutFreeTransferList - Stores the head of the list of free transfers to
        use to send data.

    BulkOutTransferCount - Stores the number of currently submitted bulk out
        transfers.

    BulkOutListLock - Stores a pointer to a lock that protects the list of free
        bulk OUT transfers.

    ConfigurationLock - Stores a queued lock that synchronizes changes to the
        enabled capabilities field and their supporting hardware registers.

    PhyId - Stores the device ID of the PHY on the controller's internal
        interconnect bus.

    MacControl - Stores a shadow copy of the MAC control register so that it
        does not have to be read constantly.

    SupportedCapabilities - Stores the set of capabilities that this device
        supports. See NET_LINK_CAPABILITY_* for definitions.

    EnabledCapabilities - Stores the currently enabled capabilities on the
        devices. See NET_LINK_CAPABILITY_* for definitions.

    InterfaceClaimed - Stores a boolean indicating if the interface has
        already been claimed.

    InterfaceNumber - Stores the number of the interface this device interacts
        on.

    BulkInEndpoint - Stores the endpoint number for the bulk in endpoint.

    BulkOutEndpoint - Stores the endpoint number for the bulk out endpoint.

    InterruptEndpoint - Stores the endpoint number for the interrupt (in)
        endpoint.

    MacAddress - Stores the default MAC address of the device.

--*/

typedef struct _SM95_DEVICE {
    PDEVICE OsDevice;
    PNET_LINK NetworkLink;
    HANDLE UsbCoreHandle;
    volatile ULONG ReferenceCount;
    PIO_BUFFER IoBuffer;
    PUSB_TRANSFER ControlTransfer;
    PUSB_TRANSFER InterruptTransfer;
    PUSB_TRANSFER BulkInTransfer[SM95_BULK_IN_TRANSFER_COUNT];
    LIST_ENTRY BulkOutFreeTransferList;
    volatile ULONG BulkOutTransferCount;
    PQUEUED_LOCK BulkOutListLock;
    PQUEUED_LOCK ConfigurationLock;
    ULONG PhyId;
    ULONG MacControl;
    ULONG SupportedCapabilities;
    ULONG EnabledCapabilities;
    BOOL InterfaceClaimed;
    UCHAR InterfaceNumber;
    UCHAR BulkInEndpoint;
    UCHAR BulkOutEndpoint;
    UCHAR InterruptEndpoint;
    BYTE MacAddress[ETHERNET_ADDRESS_SIZE];
} SM95_DEVICE, *PSM95_DEVICE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
Sm95Send (
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
Sm95GetSetInformation (
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

VOID
Sm95InterruptTransferCompletion (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine is called when the interrupt transfer returns. It processes
    the notification from the device.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

VOID
Sm95BulkInTransferCompletion (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine is called when the bulk in transfer returns. It processes
    the notification from the device.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

KSTATUS
Sm95pInitialize (
    PSM95_DEVICE Device
    );

/*++

Routine Description:

    This routine initializes and enables the SMSC95xx device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

VOID
Sm95pDestroyBulkOutTransfers (
    PSM95_DEVICE Device
    );

/*++

Routine Description:

    This routine destroys the SMSC95xx device's bulk out tranfers.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

KSTATUS
Sm95pAddNetworkDevice (
    PSM95_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

