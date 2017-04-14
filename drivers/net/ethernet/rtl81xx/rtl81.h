/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtl81.h

Abstract:

    This header contains definitions for for the Realtek RTL81xx family of
    Ethernet controllers.

Author:

    Chris Stevens 20-Jun-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/intrface/pci.h>

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros for accessing a generic register in the controller.
//

#define RTL81_READ_REGISTER32(_Controller, _Register) \
    HlReadRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define RTL81_READ_REGISTER16(_Controller, _Register) \
    HlReadRegister16((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define RTL81_READ_REGISTER8(_Controller, _Register) \
    HlReadRegister8((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define RTL81_WRITE_REGISTER32(_Controller, _Register, _Value)             \
    HlWriteRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

#define RTL81_WRITE_REGISTER16(_Controller, _Register, _Value)             \
    HlWriteRegister16((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

#define RTL81_WRITE_REGISTER8(_Controller, _Register, _Value)             \
    HlWriteRegister8((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                     (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define RTL81_ALLOCATION_TAG 0x31387452 // '18tR'

//
// Define the required alignment for transmit descriptor physical addresses.
//

#define RTL81_TRANSMIT_ALIGNMENT sizeof(ULONG)

//
// Define the number of available transmit descriptors for the legacy chips.
//

#define RTL81_TRANSMIT_DESCRIPTOR_COUNT_LEGACY 4

//
// Define the size of the receive packet ring from the hardware's perspective.
//

#define RTL81_RECEIVE_RING_BUFFER_SIZE _64KB

//
// Define the size of the receive packet ring buffer for the legacy chips,
// including the padding to handle wrapping.
//

#define RTL81_RECEIVE_RING_BUFFER_PADDED_SIZE \
    (RTL81_RECEIVE_RING_BUFFER_SIZE + 16 + 1536)

//
// Define the alignment of the receive packet ring buffer for the legacy chips.
//

#define RTL81_RECEIVE_RING_BUFFER_ALIGNMENT sizeof(ULONG)

//
// Define the the maximum receive ring buffer offset for the legacy chips.
//

#define RTL81_MAXIMUM_RECEIVE_RING_BUFFER_OFFSET RTL81_RECEIVE_RING_BUFFER_SIZE

//
// Define the adjustment necessary for receive offsets to prevent overflows.
//

#define RTL81_RECEIVE_OFFSET_ADJUSTMENT 16

//
// Define the size of the CRC that comes at the end of a received buffer.
//

#define RTL81_RECEIVE_CRC_LENGTH sizeof(ULONG)

//
// Define the maximum and minimum sizes allowed for a received packet.
//

#define RTL81_MINIMUM_PACKET_LENGTH 64
#define RTL81_MAXIMUM_PACKET_LENGTH _4KB

//
// Define the maximum transmit packet size.
//

#define RTL81_MAX_TRANSMIT_PACKET_SIZE 0xFFF

//
// Define the maximum receive packet size.
//

#define RTL81_MAX_RECEIVE_PACKET_SIZE 0x1FFF

//
// Define the descriptor alignment for the newer RTL81xx chips (i.e. RTL8139C+
// and later).
//

#define RTL81_DESCRIPTOR_ALIGNMENT 256

//
// Define the transmit descriptor count for the older chips that still support
// dynamic descriptor.
//

#define RTL81_TRANSMIT_DESCRIPTOR_COUNT_LIMITED 64

//
// Define the receive descriptor count for the older chips that still support
// dynamic descriptors.
//

#define RTL81_RECEIVE_DESCRIPTOR_COUNT_LIMITED 64

//
// Define the transmit descriptor count for the default chips that support
// dynamic descriptors.
//

#define RTL81_TRANSMIT_DESCRIPTOR_COUNT_DEFAULT 256

//
// Define the receive descriptor count for the default chips that support
// dynamic descriptors.
//

#define RTL81_RECEIVE_DESCRIPTOR_COUNT_DEFAULT 256

//
// Define the maximum size of each receive descriptor's data buffer.
//
// N.B. RTL8168 and RTL8169 can support larger receive buffers, greater than
//      the 4KB maximum for RTL8139C+.
//

#define RTL81_RECEIVE_BUFFER_DATA_SIZE 1536

//
// Define how long to wait for the device to perform an initialization
// operation before timing out, in seconds.
//

#define RTL81_DEVICE_TIMEOUT 1

//
// Define a set of flags used to determine if MSI/MSI-X interrupt should be
// used.
//

#define RTL81_PCI_MSI_FLAG_INTERFACE_REGISTERED 0x00000001
#define RTL81_PCI_MSI_FLAG_INTERFACE_AVAILABLE  0x00000002
#define RTL81_PCI_MSI_FLAG_RESOURCES_REQUESTED  0x00000004
#define RTL81_PCI_MSI_FLAG_RESOURCES_ALLOCATED  0x00000008

//
// Define the transmit status register bits.
//

#define RTL81_TRANSMIT_STATUS_CARRIER_SENSE_LOST             (1 << 31)
#define RTL81_TRANSMIT_STATUS_ABORT                          (1 << 30)
#define RTL81_TRANSMIT_STATUS_OUT_OF_WINDOW_COLLISION        (1 << 29)
#define RTL81_TRANSMIT_STATUS_CD_HEART_BEAT                  (1 << 28)
#define RTL81_TRANSMIT_STATUS_COLLISION_COUNT_MASK           (0xF << 24)
#define RTL81_TRANSMIT_STATUS_COLLISION_COUNT_SHIFT          24
#define RTL81_TRANSMIT_STATUS_EARLY_TRANSMIT_THRESHOLD_MASK  (0x3F << 16)
#define RTL81_TRANSMIT_STATUS_EARLY_TRANSMIT_THRESHOLD_SHIFT 16
#define RTL81_TRANSMIT_STATUS_OK                             (1 << 15)
#define RTL81_TRANSMIT_STATUS_FIFO_UNDERRUN                  (1 << 14)
#define RTL81_TRANSMIT_STATUS_OWN                            (1 << 13)
#define RTL81_TRANSMIT_STATUS_SIZE_MASK                      (0xFFF << 0)
#define RTL81_TRANSMIT_STATUS_SIZE_SHIFT                     0

//
// Define the early receive status register bits.
//

#define RTL81_EARLY_RECEIVE_STATUS_GOOD_PACKET 0x08
#define RTL81_EARLY_RECEIVE_STATUS_BAD_PACKET  0x04
#define RTL81_EARLY_RECEIVE_STATUS_OVERWRITE   0x02
#define RTL81_EARLY_RECEIVE_STATUS_OK          0x01

//
// Define the command register bits.
//

#define RTL81_COMMAND_REGISTER_RESET           0x10
#define RTL81_COMMAND_REGISTER_RECEIVE_ENABLE  0x08
#define RTL81_COMMAND_REGISTER_TRANSMIT_ENABLE 0x04
#define RTL81_COMMAND_REGISTER_BUFFER_EMPTY    0x01

//
// Define the interrupt mask and status register bits.
//

#define RTL81_INTERRUPT_SYSTEM_ERROR                0x8000
#define RTL81_INTERRUPT_TIMEOUT                     0x4000
#define RTL81_INTERRUPT_CABLE_LENGTH_CHANGE         0x2000
#define RTL81_INTERRUPT_SOFTWARE                    0x0100
#define RTL81_INTERRUPT_TRANSMIT_UNAVAILABLE        0x0080
#define RTL81_INTERRUPT_RECEIVE_FIFO_OVERFLOW       0x0040
#define RTL81_INTERRUPT_PACKET_UNDERRUN             0x0020
#define RTL81_INTERRUPT_LINK_CHANGE                 0x0020
#define RTL81_INTERRUPT_RECEIVE_OVERFLOW            0x0010
#define RTL81_INTERRUPT_TRANSMIT_ERROR              0x0008
#define RTL81_INTERRUPT_TRANSMIT_OK                 0x0004
#define RTL81_INTERRUPT_RECEIVE_ERROR               0x0002
#define RTL81_INTERRUPT_RECEIVE_OK                  0x0001

//
// Define the default set of interrupts to enable.
//

#define RTL81_DEFAULT_INTERRUPT_MASK         \
    (RTL81_INTERRUPT_TRANSMIT_OK |           \
     RTL81_INTERRUPT_RECEIVE_OK |            \
     RTL81_INTERRUPT_RECEIVE_OVERFLOW |      \
     RTL81_INTERRUPT_TRANSMIT_ERROR |        \
     RTL81_INTERRUPT_RECEIVE_ERROR |         \
     RTL81_INTERRUPT_RECEIVE_FIFO_OVERFLOW | \
     RTL81_INTERRUPT_TRANSMIT_UNAVAILABLE |  \
     RTL81_INTERRUPT_TIMEOUT |               \
     RTL81_INTERRUPT_SYSTEM_ERROR |          \
     RTL81_INTERRUPT_LINK_CHANGE)

//
// Define the transmit configuration register bits.
//

#define RTL81_TRANSMIT_CONFIGURATION_HARDWARE_VERSION_MASK    0x7CC00000
#define RTL81_TRANSMIT_CONFIGURATION_INTERFRAME_GAP_MASK      (0x3 << 24)
#define RTL81_TRANSMIT_CONFIGURATION_INTERFRAME_GAP_SHIFT     24
#define RTL81_TRANSMIT_CONFIGURATION_INTERFRAME_GAP_DEFAULT   0x3
#define RTL81_TRANSMIT_CONFIGURATION_INTERFRAME_GAP_2         (1 << 19)
#define RTL81_TRANSMIT_CONFIGURATION_LOOPBACK_TEST_MASK       (0x3 << 17)
#define RTL81_TRANSMIT_CONFIGURATION_LOOPBACK_TEST_SHIFT      17
#define RTL81_TRANSMIT_CONFIGURATION_CRC_NO_APPEND            (1 << 16)
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_MASK       (0x7 << 8)
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_SHIFT      8
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_16_BYTES   0x0
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_32_BYTES   0x1
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_64_BYTES   0x2
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_128_BYTES  0x3
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_256_BYTES  0x4
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_512_BYTES  0x5
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_1024_BYTES 0x6
#define RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_2048_BYTES 0x7
#define RTL81_TRANSMIT_CONFIGURATION_RETRY_COUNT_MASK         (0xF << 4)
#define RTL81_TRANSMIT_CONFIGURATION_RETRY_COUNT_SHIFT        4
#define RTL81_TRANSMIT_CONFIGURATION_CLEAR_ABORT              (1 << 0)

#define RTL81_TRANSMIT_CONFIGURATION_DEFAULT_OPTIONS          \
    (RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_2048_BYTES << \
     RTL81_TRANSMIT_CONFIGURATION_MAX_DMA_BURST_SHIFT) | \
    (RTL81_TRANSMIT_CONFIGURATION_INTERFRAME_GAP_DEFAULT <<   \
     RTL81_TRANSMIT_CONFIGURATION_INTERFRAME_GAP_SHIFT)

//
// Define various hardware versions for the RTL81xx chips.
//

#define RTL81_HARDWARE_VERSION_8101      0x74C00000
#define RTL81_HARDWARE_VERSION_8102EL    0x24800000
#define RTL81_HARDWARE_VERSION_8130      0x7C000000
#define RTL81_HARDWARE_VERSION_8139      0x60000000
#define RTL81_HARDWARE_VERSION_8139A     0x70000000
#define RTL81_HARDWARE_VERSION_8139AG    0x70800000
#define RTL81_HARDWARE_VERSION_8139B     0x78000000
#define RTL81_HARDWARE_VERSION_8139C     0x74000000
#define RTL81_HARDWARE_VERSION_8139CPLUS 0x74800000
#define RTL81_HARDWARE_VERSION_8168E_VL  0x2C800000

//
// Define the receive configuration register bits.
//

#define RTL81_RECEIVE_CONFIGURATION_EARLY_TRESHOLD_MASK             (0xF << 24)
#define RTL81_RECEIVE_CONFIGURATION_EARLY_TRESHOLD_SHIFT            24
#define RTL81_RECEIVE_CONFIGURATION_DEFAULT_EARLY_THRESHOLD         0xF
#define RTL81_RECEIVE_CONFIGURATION_MULTIPLE_EARLY_INTERRUPT        (1 << 17)
#define RTL81_RECEIVE_CONFIGURATION_8_BYTE_ERROR_PACKETS            (1 << 16)
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_MASK             (0x7 << 13)
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_SHIFT            13
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_16_BYTES         0x0
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_32_BYTES         0x1
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_64_BYTES         0x2
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_128_BYTES        0x3
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_256_BYTES        0x4
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_512_BYTES        0x5
#define RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_1024_BYTES       0x6
#define RTL81_RECEIVE_CONFIGURATION_FIFO_NO_THRESHOLD               0x7
#define RTL81_RECEIVE_CONFIGURATION_BUFFER_LENGTH_MASK              (0x3 << 11)
#define RTL81_RECEIVE_CONFIGURATION_BUFFER_LENGTH_SHIFT             11
#define RTL81_RECEIVE_CONFIGURATION_BUFFER_LENGTH_8K                0x0
#define RTL81_RECEIVE_CONFIGURATION_BUFFER_LENGTH_16K               0x1
#define RTL81_RECEIVE_CONFIGURATION_BUFFER_LENGTH_32K               0x2
#define RTL81_RECEIVE_CONFIGURATION_BUFFER_LENGTH_64K               0x3
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_MASK              (0x7 << 8)
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_SHIFT             8
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_32_BYTES          0x1
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_64_BYTES          0x2
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_128_BYTES         0x3
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_256_BYTES         0x4
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_512_BYTES         0x5
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_1024_BYTES        0x6
#define RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_UNLIMITED         0x7
#define RTL81_RECEIVE_CONFIGURATION_NO_WRAP                         (1 << 7)
#define RTL81_RECEIVE_CONFIGURATION_EEPROM_9356                     (1 << 6)
#define RTL81_RECEIVE_CONFIGURATION_ACCEPT_ERROR_PACKETS            (1 << 5)
#define RTL81_RECEIVE_CONFIGURATION_ACCEPT_RUNT_PACKETS             (1 << 4)
#define RTL81_RECEIVE_CONFIGURATION_ACCEPT_BROADCAST_PACKETS        (1 << 3)
#define RTL81_RECEIVE_CONFIGURATION_ACCEPT_MULTICAST_PACKETS        (1 << 2)
#define RTL81_RECEIVE_CONFIGURATION_ACCEPT_PHYSICAL_MATCH_PACKETS   (1 << 1)
#define RTL81_RECEIVE_CONFIGURATION_ACCEPT_ALL_PHYSICAL_PACKETS     (1 << 0)

#define RTL81_RECEIVE_CONFIGURATION_DEFAULT_OPTIONS          \
    ((RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_UNLIMITED << \
      RTL81_RECEIVE_CONFIGURATION_MAX_DMA_BURST_SHIFT) |     \
     (RTL81_RECEIVE_CONFIGURATION_BUFFER_LENGTH_64K <<       \
      RTL81_RECEIVE_CONFIGURATION_BUFFER_LENGTH_SHIFT) |     \
     (RTL81_RECEIVE_CONFIGURATION_FIFO_NO_THRESHOLD <<       \
      RTL81_RECEIVE_CONFIGURATION_FIFO_THRESHOLD_SHIFT))

//
// Define the EEPROM command register bits.
//

#define RTL81_EEPROM_COMMAND_MODE_MASK                        (0x3 << 6)
#define RTL81_EEPROM_COMMAND_MODE_SHIFT                       6
#define RTL81_EEPROM_COMMAND_MODE_NORMAL                      0x0
#define RTL81_EEPROM_COMMAND_MODE_AUTO_LOAD                   0x1
#define RTL81_EEPROM_COMMAND_MODE_93C46                       0x2
#define RTL81_EEPROM_COMMAND_MODE_CONFIGURATION_WRITE_ENABLED 0x3
#define RTL81_EEPROM_COMMAND_EECS_PIN                         (1 << 3)
#define RTL81_EEPROM_COMMAND_EESK_PIN                         (1 << 2)
#define RTL81_EEPROM_COMMAND_EEDI_PIN                         (1 << 1)
#define RTL81_EEPROM_COMMAND_EEDO_PIN                         (1 << 0)

//
// Define the media status register bits.
//

#define RTL81_MEDIA_STATUS_TRANSMIT_FLOW_CONTROL_ENABLED 0x80
#define RTL81_MEDIA_STATUS_RECEIVE_FLOW_CONTROL_ENABLED  0x40
#define RTL81_MEDIA_STATUS_AUX_POWER_PRESENT             0x10
#define RTL81_MEDIA_STATUS_SPEED_10                      0x08
#define RTL81_MEDIA_STATUS_LINK_DOWN                     0x04
#define RTL81_MEDIA_STATUS_TRANSMIT_PAUSE                0x02
#define RTL81_MEDIA_STATUS_RECEIVE_PAUSE                 0x01

//
// Define the MII access register bits.
//

#define RTL81_MII_ACCESS_COMPLETE_MASK  0x80000000
#define RTL81_MII_ACCESS_WRITE_COMPLETE 0x00000000
#define RTL81_MII_ACCESS_READ_COMPLETE  0x80000000
#define RTL81_MII_ACCESS_WRITE          0x80000000
#define RTL81_MII_ACCESS_READ           0x00000000
#define RTL81_MII_ACCESS_REGISTER_MASK  0x001F0000
#define RTL81_MII_ACCESS_REGISTER_SHIFT 16
#define RTL81_MII_ACCESS_DATA_MASK      0x0000FFFF
#define RTL81_MII_ACCESS_DATA_SHIFT     0

//
// Define the basic mode control register bits.
//

#define RTL81_BASIC_MODE_CONTROL_RESET                    0x8000
#define RTL81_BASIC_MODE_CONTROL_SPEED_SET_100            0x2000
#define RTL81_BASIC_MODE_CONTROL_AUTO_NEGOTIATION_ENABLE  0x1000
#define RTL81_BASIC_MODE_CONTROL_RESTART_AUTO_NEGOTIATION 0x0200
#define RTL81_BASIC_MODE_CONTROL_DUPLEX_MODE              0x0100

#define RTL81_BASIC_MODE_CONTROL_INITIAL_VALUE          \
    (RTL81_BASIC_MODE_CONTROL_RESET |                   \
     RTL81_BASIC_MODE_CONTROL_AUTO_NEGOTIATION_ENABLE | \
     RTL81_BASIC_MODE_CONTROL_RESTART_AUTO_NEGOTIATION)

//
// Define the basic mode status register bits.
//

#define RTL81_BASIC_MODE_STATUS_100_BASE_T4               0x8000
#define RTL81_BASIC_MODE_STATUS_100_BASE_TX_FULL_DUPLEX   0x4000
#define RTL81_BASIC_MODE_STATUS_100_BASE_TX_HALF_DUPLEX   0x2000
#define RTL81_BASIC_MODE_STATUS_10_BASE_T_FULL_DUPLEX     0x1000
#define RTL81_BASIC_MODE_STATUS_10_BASE_T_HALF_DUPLEX     0x0800
#define RTL81_BASIC_MODE_STATUS_MEDIUM_MODE_1             0x0080
#define RTL81_BASIC_MODE_STATUS_MEDIUM_MODE_0             0x0040
#define RTL81_BASIC_MODE_STATUS_AUTO_NEGOTIATION_COMPLETE 0x0020
#define RTL81_BASIC_MODE_STATUS_REMOTE_FAULT              0x0010
#define RTL81_BASIC_MODE_STATUS_AUTO_NEGOTIATION          0x0008
#define RTL81_BASIC_MODE_STATUS_LINK                      0x0004
#define RTL81_BASIC_MODE_STATUS_JABBER_DETECT             0x0002
#define RTL81_BASIC_MODE_STATUS_EXTENDED_CAPABILITY       0x0001

//
// Define the PHY status register bits.
//

#define RTL81_PHY_STATUS_TBI_ENABLED           0x80
#define RTL81_PHY_STATUS_TRANSMIT_FLOW_CONTROL 0x40
#define RTL81_PHY_STATUS_RECEIVE_FLOW_CONTROL  0x20
#define RTL81_PHY_STATUS_SPEED_1000            0x10
#define RTL81_PHY_STATUS_SPEED_100             0x08
#define RTL81_PHY_STATUS_SPEED_10              0x04
#define RTL81_PHY_STATUS_LINK_UP               0x02
#define RTL81_PHY_STATUS_FULL_DUPLEX           0x01

//
// Define the transmit priority polling register bits.
//

#define RTL81_TRANSMIT_PRIORITY_POLLING_HIGH                     0x80
#define RTL81_TRANSMIT_PRIORITY_POLLING_NORMAL                   0x40
#define RTL81_TRANSMIT_PRIORITY_POLLING_FORCE_SOFTWARE_INTERRUPT 0x01

//
// Define the 2nd command register's bits.
//

#define RTL81_COMMAND_2_REGISTER_RECEIVE_VLAN_DETAGGING   0x0040
#define RTL81_COMMAND_2_REGISTER_RECEIVE_CHECKSUM_OFFLOAD 0x0020
#define RTL81_COMMAND_2_REGISTER_DUAL_ADDRESS_CYCLE       0x0010
#define RTL81_COMMAND_2_REGISTER_MULTIPLE_READ_WRITE      0x0008
#define RTL81_COMMAND_2_REGISTER_RECEIVE_ENABLE           0x0002
#define RTL81_COMMAND_2_REGISTER_TRANSMIT_ENABLE          0x0001

#define RTL81_COMMAND_2_REGISTER_DEFAULT                 \
    (RTL81_COMMAND_2_REGISTER_TRANSMIT_ENABLE |          \
     RTL81_COMMAND_2_REGISTER_RECEIVE_ENABLE |           \
     RTL81_COMMAND_2_REGISTER_MULTIPLE_READ_WRITE)

//
// Define the default value to write to the early transmit threshold register.
//

#define RTL81_EARLY_TRANSMIT_THRESHOLD_DEFAULT 0x3F

//
// Define the receive packet header flags.
//

#define RTL81_RECEIVE_PACKET_STATUS_MULTICAST_ADDRESS        0x8000
#define RTL81_RECEIVE_PACKET_STATUS_PHYSICAL_ADDRESS_MATCHED 0x4000
#define RTL81_RECEIVE_PACKET_STATUS_BROADCAST_ADDRESS        0x2000
#define RTL81_RECEIVE_PACKET_STATUS_INVALID_SYMBOL_ERROR     0x0020
#define RTL81_RECEIVE_PACKET_STATUS_RUNT_PACKET              0x0010
#define RTL81_RECEIVE_PACKET_STATUS_LONG_PACKET              0x0008
#define RTL81_RECEIVE_PACKET_STATUS_CRC_ERROR                0x0004
#define RTL81_RECEIVE_PACKET_STATUS_FRAME_ALIGNMENT_ERROR    0x0002
#define RTL81_RECEIVE_PACKET_STATUS_OK                       0x0001

//
// Define the mask of receive packet errors.
//

#define RTL81_RECEIVE_PACKET_ERROR_MASK                  \
    (RTL81_RECEIVE_PACKET_STATUS_FRAME_ALIGNMENT_ERROR | \
     RTL81_RECEIVE_PACKET_STATUS_CRC_ERROR |             \
     RTL81_RECEIVE_PACKET_STATUS_LONG_PACKET |           \
     RTL81_RECEIVE_PACKET_STATUS_RUNT_PACKET |           \
     RTL81_RECEIVE_PACKET_STATUS_INVALID_SYMBOL_ERROR)

//
// Define the transmit descriptor command bits.
//

#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_OWN                     (1 << 31)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_END_OF_RING             (1 << 30)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_FIRST_SEGMENT           (1 << 29)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_LAST_SEGMENT            (1 << 28)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_LARGE_SEND              (1 << 27)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_LARGE_SEND_SIZE_MASK    (0x7FF << 16)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_LARGE_SEND_SIZE_SHIFT   16
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_FIFO_UNDERRUN           (1 << 25)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_ERROR_SUMMARY           (1 << 23)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_OUT_OF_WINDOW_COLLISION (1 << 22)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_LINK_FAILURE            (1 << 21)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_EXCESSIVE_COLLISIONS    (1 << 20)
#define RLT81_TRANSMIT_DESCRIPTOR_COMMAND_COLLISION_COUNT_MASK    (0xF << 16)
#define RLT81_TRANSMIT_DESCRIPTOR_COMMAND_COLLISION_COUNT_SHIFT   16
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_IP_CHECKSUM_OFFLOAD     (1 << 18)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_UDP_CHECKSUM_OFFLOAD    (1 << 17)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_TCP_CHECKSUM_OFFLOAD    (1 << 16)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_SIZE_MASK               (0xFFFF << 0)
#define RTL81_TRANSMIT_DESCRIPTOR_COMMAND_SIZE_SHIFT              0

//
// Define the transmit descriptor VLAN bits.
//

#define RTL81_TRANSMIT_DESCRIPTOR_VLAN_UDP_CHECKSUM_OFFLOAD (1 << 31)
#define RTL81_TRANSMIT_DESCRIPTOR_VLAN_TCP_CHECKSUM_OFFLOAD (1 << 30)
#define RTL81_TRANSMIT_DESCRIPTOR_VLAN_IP_CHECKSUM_OFFLOAD  (1 << 29)

//
// Define the receive descriptor command bits.
//

#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_OWN                    (1 << 31)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_END_OF_RING            (1 << 30)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_FIRST_SEGMENT          (1 << 29)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_LAST_SEGMENT           (1 << 28)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_FRAME_ALIGNMENT_ERROR  (1 << 27)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_MULTICAST              (1 << 26)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_PHYSICAL_MATCH         (1 << 25)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_BROADCAST              (1 << 24)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_BUFFER_OVERFLOW        (1 << 23)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_FIFO_OVERFLOW          (1 << 22)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_WATCHDOG_TIMER_EXPIRED (1 << 21)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_ERROR_SUMMARY          (1 << 20)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_RUNT                   (1 << 19)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_CRC_ERROR              (1 << 18)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_MASK          (0x3 << 16)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_SHIFT         16
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_NON_IP        0x0
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_TCP_IP        0x1
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_UDP_IP        0x2
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_PROTOCOL_IP            0x3
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_IP_CHECKSUM_FAILURE    (1 << 15)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_UDP_CHECKSUM_FAILURE   (1 << 14)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_TCP_CHECKSUM_FAILURE   (1 << 13)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_LARGE_SIZE_MASK        (0x1FFF << 0)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_LARGE_SIZE_SHIFT       0
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_SIZE_MASK              (0xFFF << 0)
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_SIZE_SHIFT             0

//
// Define the default state of a receive descriptor.
//

#define RTL81_RECEIVE_DESCRIPTOR_DEFAULT_COMMAND      \
    (RTL81_RECEIVE_DESCRIPTOR_COMMAND_OWN |           \
     ((RTL81_RECEIVE_BUFFER_DATA_SIZE <<              \
       RTL81_RECEIVE_DESCRIPTOR_COMMAND_SIZE_SHIFT) & \
      RTL81_RECEIVE_DESCRIPTOR_COMMAND_SIZE_MASK))

//
// Define the receive descriptor VLAN bits.
//

#define RTL81_RECEIVE_DESCRIPTOR_VLAN_IP4 (1 << 30)

//
// Define the mask and shift of the RTL8168 and above values that needs to be
// shifted by 1 to match those of the RTL8139C+.
//

#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_MASK 0x0FFFE000
#define RTL81_RECEIVE_DESCRIPTOR_COMMAND_SHIFT 1

//
// Define MII Basic Control register bits.
//

#define RTL81_MII_BASIC_CONTROL_SPEED_1000              0x0040
#define RTL81_MII_BASIC_CONTROL_COLLISION_TEST          0x0080
#define RTL81_MII_BASIC_CONTROL_FULL_DUPLEX             0x0100
#define RTL81_MII_BASIC_CONTROL_RESTART_AUTONEGOTIATION 0x0200
#define RTL81_MII_BASIC_CONTROL_ISOLATE                 0x0400
#define RTL81_MII_BASIC_CONTROL_POWER_DOWN              0x0800
#define RTL81_MII_BASIC_CONTROL_ENABLE_AUTONEGOTIATION  0x1000
#define RTL81_MII_BASIC_CONTROL_SPEED_100               0x2000
#define RTL81_MII_BASIC_CONTROL_LOOPBACK                0x4000
#define RTL81_MII_BASIC_CONTROL_RESET                   0x8000

//
// Define MII Basic Status register bits.
//

#define RTL81_MII_BASIC_STATUS_EXTENDED_CAPABILITY      0x0001
#define RTL81_MII_BASIC_STATUS_JABBER_DETECTED          0x0002
#define RTL81_MII_BASIC_STATUS_LINK_STATUS              0x0004
#define RTL81_MII_BASIC_STATUS_AUTONEGOTIATE_CAPABLE    0x0008
#define RTL81_MII_BASIC_STATUS_REMOTE_FAULT             0x0010
#define RTL81_MII_BASIC_STATUS_AUTONEGOTIATE_COMPLETE   0x0020
#define RTL81_MII_BASIC_STATUS_EXTENDED_STATUS          0x0100
#define RTL81_MII_BASIC_STATUS_100_HALF2                0x0200
#define RTL81_MII_BASIC_STATUS_100_FULL2                0x0400
#define RTL81_MII_BASIC_STATUS_10_HALF                  0x0800
#define RTL81_MII_BASIC_STATUS_10_FULL                  0x1000
#define RTL81_MII_BASIC_STATUS_100_HALF                 0x2000
#define RTL81_MII_BASIC_STATUS_100_FULL                 0x4000
#define RTL81_MII_BASIC_STATUS_100_BASE4                0x8000

//
// Define MII Advertise register bits.
//

#define RTL81_MII_ADVERTISE_SELECT_MASK            0x001F
#define RTL81_MII_ADVERTISE_CSMA                   0x0001
#define RTL81_MII_ADVERTISE_10_HALF                0x0020
#define RTL81_MII_ADVERTISE_1000X_FULL             0x0020
#define RTL81_MII_ADVERTISE_10_FULL                0x0040
#define RTL81_MII_ADVERTISE_1000X_HALF             0x0040
#define RTL81_MII_ADVERTISE_100_HALF               0x0080
#define RTL81_MII_ADVERTISE_1000X_PAUSE            0x0080
#define RTL81_MII_ADVERTISE_100_FULL               0x0100
#define RTL81_MII_ADVERTISE_1000X_PAUSE_ASYMMETRIC 0x0100
#define RTL81_MII_ADVERTISE_100_BASE4              0x0200
#define RTL81_MII_ADVERTISE_PAUSE                  0x0400
#define RTL81_MII_ADVERTISE_PAUSE_ASYMMETRIC       0x0800
#define RTL81_MII_ADVERTISE_REMOTE_FAULT           0x2000
#define RTL81_MII_ADVERTISE_LINK_PARTNER           0x4000
#define RTL81_MII_ADVERTISE_NEXT_PAGE              0x8000

#define RTL81_MII_ADVERTISE_FULL    \
    (RTL81_MII_ADVERTISE_100_FULL | \
     RTL81_MII_ADVERTISE_10_FULL |  \
     RTL81_MII_ADVERTISE_CSMA)

#define RTL81_MII_ADVERTISE_ALL     \
    (RTL81_MII_ADVERTISE_10_HALF |  \
     RTL81_MII_ADVERTISE_10_FULL |  \
     RTL81_MII_ADVERTISE_100_HALF | \
     RTL81_MII_ADVERTISE_100_FULL | \
     RTL81_MII_ADVERTISE_CSMA)

//
// Define MII Gigabit control register bits.
//

#define RTL81_MII_GIGABIT_CONTROL_MANUAL_MASTER       0x1000
#define RTL81_MII_GIGABIT_CONTROL_ADVANCED_MASTER     0x0800
#define RTL81_MII_GIGABIT_CONTROL_ADVERTISE_1000_FULL 0x0200
#define RTL81_MII_GIGABIT_CONTROL_ADVERTISE_1000_HALF 0x0100

//
// Define the flags used to describe an RTL81xx device.
//

#define RTL81_FLAG_TRANSMIT_MODE_LEGACY     0x00000001
#define RTL81_FLAG_REGISTER_SET_LEGACY      0x00000002
#define RTL81_FLAG_DESCRIPTOR_LIMIT_64      0x00000004
#define RTL81_FLAG_MULTI_SEGMENT_SUPPORT    0x00000008
#define RTL81_FLAG_RECEIVE_COMMAND_LEGACY   0x00000010
#define RTL81_FLAG_CHECKSUM_OFFLOAD_DEFAULT 0x00000020
#define RTL81_FLAG_CHECKSUM_OFFLOAD_VLAN    0x00000040

//
// Define the mask of different checksum offload types supported.
//

#define RTL81_FLAG_CHECKSUM_OFFLOAD_MASK   \
    (RTL81_FLAG_CHECKSUM_OFFLOAD_DEFAULT | \
     RTL81_FLAG_CHECKSUM_OFFLOAD_VLAN)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RTL81_REGISTER {
    Rtl81RegisterId0 = 0x0,
    Rtl81RegisterId1 = 0x1,
    Rtl81RegisterId2 = 0x2,
    Rtl81RegisterId3 = 0x3,
    Rtl81RegisterId4 = 0x4,
    Rtl81RegisterId5 = 0x5,
    Rtl81RegisterMulticast0 = 0x8,
    Rtl81RegisterMulticast1 = 0x9,
    Rtl81RegisterMulticast2 = 0xA,
    Rtl81RegisterMulticast3 = 0xB,
    Rtl81RegisterMulticast4 = 0xC,
    Rtl81RegisterMulticast5 = 0xD,
    Rtl81RegisterMulticast6 = 0xE,
    Rtl81RegisterMulticast7 = 0xF,
    RTL81RegisterDumpTallyCommand = 0x10,
    Rtl81RegisterTransmitStatus0 = 0x10,
    Rtl81RegisterTransmitStatus1 = 0x14,
    Rtl81RegisterTransmitStatus2 = 0x18,
    Rtl81RegisterTransmitStatus3 = 0x1C,
    Rtl81RegisterTransmitDescriptorBaseLow = 0x20,
    Rtl81RegisterTransmitAddress0 = 0x20,
    Rtl81RegisterTransmitDescriptorBaseHigh = 0x24,
    Rtl81RegisterTransmitAddress1 = 0x24,
    Rtl81RegisterUrgentTransmitDescriptorBaseLow = 0x28,
    Rtl81RegisterTransmitAddress2 = 0x28,
    Rtl81RegisterUrgentTransmitDescriptorBaseHigh = 0x2C,
    Rtl81RegisterTransmitAddress3 = 0x2C,
    Rtl81RegisterReceiveBufferStart = 0x30,
    Rtl81RegisterEarlyReceiveStatus = 0x36,
    Rtl81RegisterCommand = 0x37,
    Rtl81RegisterReadPacketAddress = 0x38,
    Rtl81RegisterTransmitPriorityPolling1 = 0x38,
    Rtl81RegisterReceiveBufferCurrent = 0x3A,
    Rtl81RegisterInterruptMask = 0x3C,
    Rtl81RegisterInterruptStatus = 0x3E,
    Rtl81RegisterTransmitConfiguration = 0x40,
    Rtl81RegisterReceiveConfiguration = 0x44,
    Rtl81RegisterTimeCount = 0x48,
    Rtl81RegisterMissedPacketCounter = 0x4C,
    Rtl81RegisterEepromCommand = 0x50,
    Rtl81RegisterLegacyConfiguration0 = 0x51,
    Rtl81RegisterConfiguration0 = 0x51,
    Rtl81RegisterLegacyConfiguration1 = 0x52,
    Rtl81RegisterConfiguration1 = 0x52,
    Rtl81RegisterConfiguration2 = 0x53,
    Rtl81RegisterConfiguration3 = 0x54,
    Rtl81RegisterTimerInterrupt = 0x54,
    Rtl81RegisterConfiguration4 = 0x55,
    Rtl81RegisterConfiguration5 = 0x56,
    Rtl81RegisterMediaStatus = 0x58,
    Rtl81RegisterLegacyConfiguration3 = 0x59,
    Rtl81RegisterLegacyConfiguration4 = 0x5A,
    Rtl81RegisterMultipleInterruptSelect = 0x5C,
    Rtl81RegisterPciRevision = 0x5E,
    Rtl81RegisterTransmitStatusAll = 0x60,
    Rtl81RegisterMiiAccess = 0x60,
    Rtl81RegisterBasicModeControl = 0x62,
    Rtl81RegisterBasicModeStatus = 0x64,
    Rtl81RegisterAutoNegotiationAdvertisement = 0x66,
    Rtl81RegisterAutoNegotiationLinkPartner = 0x68,
    Rtl81RegisterAutoNegotiationExpansion = 0x6A,
    Rtl81RegisterDisconnectCounter = 0x6C,
    Rtl81RegisterPhyStatus = 0x6C,
    Rtl81RegisterFalseCarrierSenseCounter = 0x6E,
    Rtl81RegisterNwayTest = 0x70,
    Rtl81RegisterReceiveErrorCounter = 0x72,
    Rtl81RegisterCsConfiguration = 0x74,
    Rtl81RegisterPhyParameter1 = 0x78,
    Rtl81RegisterTwisterParameter = 0x7C,
    Rtl81RegisterPhyParameter2 = 0x80,
    Rtl81RegisterLegacyConfiguration5 = 0xD8,
    Rtl81RegisterTransmitPriorityPolling2 = 0xD9,
    Rtl81RegisterReceiveMaxPacketSize = 0xDA,
    Rtl81RegisterCommand2 = 0xE0,
    Rtl81RegisterReceiveDescriptorBaseLow = 0xE4,
    Rtl81RegisterReceiveDescriptorBaseHigh = 0xE8,
    Rtl81RegisterEarlyTransmitThreshold = 0xEC
} RTL81_REGISTER, *PRTL81_REGISTER;

//
// TODO: Refactor the generic MII registers and bit definitions to use mii.h.
//

typedef enum _RTL81_MII_REGISTER {
    Rtl81MiiRegisterBasicControl               = 0x00, // BMCR
    Rtl81MiiRegisterBasicStatus                = 0x01, // BMSR
    Rtl81MiiRegisterPhysicalId1                = 0x02, // PHYSID1
    Rtl81MiiRegisterPhysicalId2                = 0x03, // PHYSID2
    Rtl81MiiRegisterAdvertise                  = 0x04, // ADVERTISE
    Rtl81MiiRegisterLinkPartnerAbility         = 0x05, // LPA
    Rtl81MiiRegisterExpansion                  = 0x06, // EXPANSION
    Rtl81MiiRegisterGigabitControl             = 0x09, // CTRL1000
    Rtl81MiiRegisterGigabitStatus              = 0x0A, // STAT1000
    Rtl81MiiRegisterExtendedStatus             = 0x0F, // ESTATUS
    Rtl81MiiRegisterDisconnectCounter          = 0x12, // DCOUNTER
    Rtl81MiiRegisterFalseCarrierCounter        = 0x13, // FCSCOUNTER
    Rtl81MiiRegisterNWayTest                   = 0x14, // NWAYTEST
    Rtl81MiiRegisterReceiveErrorCounter        = 0x15, // RERRCOUNTER
    Rtl81MiiRegisterSiliconRevision            = 0x16, // SREVISION
    Rtl81MiiRegisterLoopbackReceiveBypassError = 0x18, // LBRERROR
    Rtl81MiiRegisterPhyAddress                 = 0x19, // PHYADDR
    Rtl81MiiRegisterTpiStatus                  = 0x1B, // TPISTATUS
    Rtl81MiiRegisterNetworkConfiguration       = 0x1C, // NCONFIG
    Rtl81MiiRegisterMax                        = 0x1F
} RTL81_MII_REGISTER, *PRTL81_MII_REGISTER;

/*++

Structure Description:

    This structure defines an RTL81xx received packet header.

Members:

    Status - Stores the received packet status.

    Length - Stores the length of the received packet.

--*/

typedef struct _RTL81_PACKET_HEADER {
    USHORT Status;
    USHORT Length;
} RTL81_PACKET_HEADER, *PRTL81_PACKET_HEADER;

/*++

Structure Description:

    This structure defines a transmit descriptor for newer RTL81xx chips,
    including RTL8139C+, RTL8168, and RTL8169.

Members:

    Command - Stores the command flags that indicate the descriptor's status.

    VlanTag - Stores the VLAN tag associated with the packet.

    PhysicalAddress - Stores the physical address of the buffer to send out the
        wire.

--*/

typedef struct _RTL81_TRANSMIT_DESCRIPTOR {
    ULONG Command;
    ULONG VlanTag;
    ULONGLONG PhysicalAddress;
} PACKED RTL81_TRANSMIT_DESCRIPTOR, *PRTL81_TRANSMIT_DESCRIPTOR;

/*++

Structure Description:

    This structure defines a receive descriptor for newer RTL81xx chips,
    including RTL8139C+, RTL8168, and RTL8169.

Members:

    Command - Stores the command flags that indicate the descriptor's status.

    VlanTag - Stores the VLAN tag associated with the packet.

    PhysicalAddress - Stores the physical address of the buffer to send out the
        wire.

--*/

typedef struct _RTL81_RECEIVE_DESCRIPTOR {
    ULONG Command;
    ULONG VlanTag;
    ULONGLONG PhysicalAddress;
} PACKED RTL81_RECEIVE_DESCRIPTOR, *PRTL81_RECEIVE_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the extra data required to transmit and receive on
    an RTL8139 device.

Members:

    ReceiveIoBuffer - Stores a pointer to the I/O buffer used to store received
        data.

    ActiveTransmitPackets - Stores the array of transmit packets that are
        active in the RTL81xx controller.

    TransmitNextToUse - Stores the index of the next transmit descriptor to use
        when sending a new packet.

    TransmitNextToClean - Stores the index of the oldest in-flight packet, the
        first one to check to see if transmission is done.

--*/

typedef struct _RTL81_LEGACY_DATA {
    PIO_BUFFER ReceiveIoBuffer;
    PNET_PACKET_BUFFER
        ActiveTransmitPackets[RTL81_TRANSMIT_DESCRIPTOR_COUNT_LEGACY];

    BYTE TransmitNextToUse;
    BYTE TransmitNextToClean;
} RTL81_LEGACY_DATA, *PRTL81_LEGACY_DATA;

/*++

Structure Description:

    This structure defines the extra data required to transmit and receive on
    an RTL8139C+, RTL8168, and RTL8169 device.

Members:

    DescriptorIoBuffer - Supplies a pointer to the I/O buffer taht holds the
        transmit descriptor array, received descriptor array, array of transmit
        buffer virtual addresses, and the array of received packet memory.

    TransmitDescriptor - Stores the array of transmit descriptor heads. Must be
        256-byte aligned.

    TransmitBuffer - Stores an array of points to the virtual addresses of the
        transmitted network packets. This is used when freeing packets that
        have successfully been sent.

    ReceiveDescriptor - Stores the array of receive descriptor heads. Must be
        256-byte aligned.

    ReceivePacketData - Stores the virtual address of the first receive
        descriptor's packet data.

    TransmitNextToUse - Stores the index of the next transmit descriptor to use
        when sending a new packet.

    TransmitNextToClean - Stores the index of the oldest in-flight packet, the
        first one to check to see if transmission is done.

    TransmitDescriptorCount - Stores the number of transmit descriptors.

    ReceiveNextToReap - Stores the index of the next receive descriptor to
        check for data.

    ReceiveDescriptorCount - Stores the number of receive descriptors.

--*/

typedef struct _RTL81_DEFAULT_DATA {
    PIO_BUFFER DescriptorIoBuffer;
    PRTL81_TRANSMIT_DESCRIPTOR TransmitDescriptor;
    PNET_PACKET_BUFFER *TransmitBuffer;
    PRTL81_RECEIVE_DESCRIPTOR ReceiveDescriptor;
    PVOID ReceivePacketData;
    USHORT TransmitNextToUse;
    USHORT TransmitNextToClean;
    USHORT TransmitDescriptorCount;
    USHORT ReceiveNextToReap;
    USHORT ReceiveDescriptorCount;
} RTL81_DEFAULT_DATA, *PRTL81_DEFAULT_DATA;

/*++

Structure Description:

    This structure defines an RTL81xx LAN device.

Members:

    Flags - Stores a bitmask of flags indicating the type of device described
        by this structure. See RTL81_FLAG_* for definitions.

    OsDevice - Stores a pointer to the OS device object.

    ControllerBase - Stores the virtual address of the memory mapping to the
        RTL81xx's registers.

    NetworkLink - Stores a pointer to the core networking link.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    TransmitLock - Stores a queued lock that protects access to the transmit
        packet list and various other values.

    ReceiveLock - Stores a queued lock that protects access to the receive
        descriptors.

    ConfigurationLock - Stores a queued lock that synchronizes changes to the
        enabled capabilities field and their supporting hardware registers.

    TransmitInterruptMask - Stores a mask of interrupt status bits that trigger
        the processing of the transmit descriptors.

    ReceiveInterruptMask - Stores a mask of interrupt status bits that trigger
        the processing of received frames.

    PciMsiFlags - Stores a bitmask of flags indicating whether or not MSI/MSI-X
        interrupts should be used. See RTL81_PCI_MSI_FLAG_* for definitions.

    PciMsiInterface - Stores the interface to enable PCI message signaled
        interrupts.

    PendingInterrupts - Stores the bitmask of pending interrupts. See
        RTL81_INTERRUPT_* for definitions.

    MacAddress - Stores the default MAC address of the device.

    TransmitPacketList - Stores the list of network packets waiting to be sent.

    MaxTransmitPacketListCount - Stores the maximum number of packets to remain
        on the list of packets waiting to be sent.

    SupportedCapabilities - Stores the set of capabilities that this device
        supports. See NET_LINK_CAPABILITY_* for definitions.

    EnabledCapabilities - Stores the currently enabled capabilities on the
        devices. See NET_LINK_CAPABILITY_* for definitions.

    ReceiveConfiguration - Stores the receive configuration register state to
        use during a reset. See RTL81_RECEIVE_CONFIGURATION_* for definitions.

    LegacyData - Stores the extra data required to transmit and receive packets
        on a legacy device.

    DefaultData - Stores the extra data required to transmit and receive
        packets on the newer RTL8139C+, RTL8168, and RTL8169 devices.

--*/

typedef struct _RTL81_DEVICE {
    ULONG Flags;
    PDEVICE OsDevice;
    PVOID ControllerBase;
    PNET_LINK NetworkLink;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PQUEUED_LOCK TransmitLock;
    PQUEUED_LOCK ReceiveLock;
    PQUEUED_LOCK ConfigurationLock;
    USHORT TransmitInterruptMask;
    USHORT ReceiveInterruptMask;
    ULONG PciMsiFlags;
    INTERFACE_PCI_MSI PciMsiInterface;
    volatile ULONG PendingInterrupts;
    BYTE MacAddress[ETHERNET_ADDRESS_SIZE];
    NET_PACKET_LIST TransmitPacketList;
    ULONG MaxTransmitPacketListCount;
    ULONG SupportedCapabilities;
    ULONG EnabledCapabilities;
    ULONG ReceiveConfiguration;
    union {
        RTL81_LEGACY_DATA LegacyData;
        RTL81_DEFAULT_DATA DefaultData;
    } U;

} RTL81_DEVICE, *PRTL81_DEVICE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
Rtl81Send (
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
Rtl81GetSetInformation (
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
Rtl81pInitializeDeviceStructures (
    PRTL81_DEVICE Device
    );

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an RTL81xx device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

VOID
Rtl81pDestroyDeviceStructures (
    PRTL81_DEVICE Device
    );

/*++

Routine Description:

    This routine performs destroy any device structures allocated for the
    RTL81xx device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

KSTATUS
Rtl81pInitialize (
    PRTL81_DEVICE Device
    );

/*++

Routine Description:

    This routine initializes and enables the RTL81xx device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
Rtl81pInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the RTL81xx interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
Rtl81pInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine processes interrupts for the RTL81xx controller at low level.

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
Rtl81pAddNetworkDevice (
    PRTL81_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

