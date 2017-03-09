/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    e1000.h

Abstract:

    This header contains internal definitions for the Intel e1000 Integrated
    LAN driver (i8255x compatible).

Author:

    Evan Green 8-Nov-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// Define macros for accessing a generic register in the E1000.
//

#define E1000_READ(_Controller, _Register) \
    HlReadRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define E1000_WRITE(_Controller, _Register, _Value)                        \
    HlWriteRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

#define E1000_READ_ARRAY(_Controller, _Register, _Offset) \
    E1000_READ((_Controller), (_Register) + ((_Offset) << 2))

#define E1000_WRITE_ARRAY(_Controller, _Register, _Offset, _Value) \
    E1000_WRITE((_Controller), (_Register) + ((_Offset) << 2), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag: IE1k
//

#define E1000_ALLOCATION_TAG 0x6B314549

//
// Define the size of receive frame data.
//

#define E1000_RX_DATA_SIZE 2048

//
// Define the number of commands that can exist in the command ring.
//

#define E1000_TX_RING_SIZE 256

//
// Define the number of receive buffers that will be allocated for the
// controller.
//

#define E1000_RX_RING_SIZE 128

//
// Define the number of receive address registers in the device.
//

#define E1000_RECEIVE_ADDRESSES 15

//
// Define the number of multicast table entries.
//

#define E1000_MULTICAST_TABLE_SIZE 128

//
// Define the maximum amount of packets that E1000 will keep queued before it
// starts to drop packets.
//

#define E1000_MAX_TRANSMIT_PACKET_LIST_COUNT (E1000_TX_RING_SIZE * 2)

//
// Flow control values.
//

#define E1000_FLOW_CONTROL_TYPE 0x00008808
#define E1000_FLOW_CONTROL_ADDRESS_HIGH 0x00000100
#define E1000_FLOW_CONTROL_ADDRESS_LOW 0x00C28001
#define E1000_FLOW_CONTROL_PAUSE_TIME 0x0700

#define E1000_VLAN_ETHERTYPE 0x8100

//
// Transmit descriptor command bits.
//

//
// Set this bit if this descriptor contains the end of the packet.
//

#define E1000_TX_COMMAND_END 0x01

//
// Set this bit to enable calculation of the CRC field. This should only be set
// if the END bit is set as well.
//

#define E1000_TX_COMMAND_CRC 0x02

//
// Set this bit to insert a TCP checksum as defined by the ChecksumOffset and
// ChecksumStart fields. This bit is only valid if the END bit is set.
//

#define E1000_TX_COMMAND_CHECKSUM 0x04

//
// Set this bit to have the hardware report status.
//

#define E1000_TX_COMMAND_REPORT_STATUS 0x08

//
// Set this bit to enable VLAN tagging. The tag comes from the special field,
// and the EtherType field comes from the VET register.
//

#define E1000_TX_COMMAND_VLAN 0x40

//
// Set this bit to enable delaying interrupts for a bit, to allow transmitted
// packets to batch.
//

#define E1000_TX_COMMAND_INTERRUPT_DELAY 0x80

//
// Transmit descriptor status bits.
//

//
// This bit is set when the descriptor is completely processed by the hardware.
//

#define E1000_TX_STATUS_DONE 0x01

//
// This bit is set if there were too many collisions while trying to transmit.
//

#define E1000_TX_STATUS_COLLISIONS 0x02

//
// This bit is set if there was a late collision.
//

#define E1000_TX_STATUS_LATE_COLLISION 0x04

//
// Receive descriptor status bits.
//

//
// This bit is set when the hardware is done with the descriptor.
//

#define E1000_RX_STATUS_DONE 0x01

//
// This bit is set if this is the last descriptor in an incoming packet.
//

#define E1000_RX_STATUS_END_OF_PACKET 0x02

//
// This bit is set if checksum indication bits should be ignored.
//

#define E1000_RX_STATUS_IGNORE_CHECKSUM 0x04

//
// This bit is set if the packet is 802.1Q.
//

#define E1000_RX_STATUS_8021Q 0x08

//
// This bit is set if the UDP checksum is calculated on the packet.
//

#define E1000_RX_STATUS_UDP_CHECKSUM 0x10

//
// This bit is set if the TCP checksum is calculated on the packet.
//

#define E1000_RX_STATUS_TCP_CHECKSUM 0x20

//
// This bit is set if the IPv4 checksum is calculated on the packet.
//

#define E1000_RX_STATUS_IP4_CHECKSUM 0x40

//
// This bit is set if the packet passed an inexact filter.
//

#define E1000_RX_STATUS_INEXACT_FILTER 0x80

//
// Receive descriptor error bits.
//

#define E1000_RX_ERROR_CRC_ALIGNMENT 0x01
#define E1000_RX_ERROR_SYMBOL 0x02
#define E1000_RX_ERROR_SEQUENCE 0x04
#define E1000_RX_ERROR_TCP_UDP_CHECKSUM 0x20
#define E1000_RX_ERROR_IP_CHECKSUM 0x40
#define E1000_RX_ERROR_DATA 0x80

#define E1000_RX_INTERRUPT_DELAY 0
#define E1000_RX_ABSOLUTE_INTERRUPT_DELAY 8

//
// Device control register bits.
//

#define E1000_DEVICE_CONTROL_DUPLEX (1 << 0)
#define E1000_DEVICE_CONTROL_LINK_RESET (1 << 3)
#define E1000_DEVICE_CONTROL_AUTO_SPEED (1 << 5)
#define E1000_DEVICE_CONTROL_SET_LINK_UP (1 << 6)
#define E1000_DEVICE_CONTROL_INVERT_LOSS_OF_SIGNAL (1 << 7)
#define E1000_DEVICE_CONTROL_SPEED_MASK (0x3 << 8)
#define E1000_DEVICE_CONTROL_SPEED_10 (0x0 << 8)
#define E1000_DEVICE_CONTROL_SPEED_100 (0x1 << 8)
#define E1000_DEVICE_CONTROL_SPEED_1000 (0x2 << 8)
#define E1000_DEVICE_CONTROL_FORCE_SPEED (1 << 11)
#define E1000_DEVICE_CONTROL_FORCE_DUPLEX (1 << 12)
#define E1000_DEVICE_CONTROL_SDP0_DATA (1 << 18)
#define E1000_DEVICE_CONTROL_SDP1_DATA (1 << 19)
#define E1000_DEVICE_CONTROL_SDP2_DATA (1 << 20)
#define E1000_DEVICE_CONTROL_SDP3_DATA (1 << 21)
#define E1000_DEVICE_CONTROL_D3COLD_WAKEUP (1 << 20)
#define E1000_DEVICE_CONTROL_PHY_POWER_MANAGEMENT (1 << 21)
#define E1000_DEVICE_CONTROL_SDP0_DIRECTION (1 << 22)
#define E1000_DEVICE_CONTROL_SDP1_DIRECTION (1 << 23)
#define E1000_DEVICE_CONTROL_SDP2_DIRECTION (1 << 24)
#define E1000_DEVICE_CONTROL_SDP3_DIRECTION (1 << 25)
#define E1000_DEVICE_CONTROL_RESET (1 << 26)
#define E1000_DEVICE_CONTROL_RX_FLOW (1 << 27)
#define E1000_DEVICE_CONTROL_TX_FLOW (1 << 28)
#define E1000_DEVICE_CONTROL_VLAN_ENABLE (1 << 30)
#define E1000_DEVICE_CONTROL_PHY_RESET (1 << 31)

//
// Device status register bits.
//

#define E1000_DEVICE_STATUS_FULL_DUPLEX (1 << 0)
#define E1000_DEVICE_STATUS_LINK_UP (1 << 1)
#define E1000_DEVICE_STATUS_TX_OFF (1 << 4)
#define E1000_DEVICE_STATUS_TBI_MODE (1 << 5)
#define E1000_DEVICE_STATUS_SPEED_MASK (0x3 << 6)
#define E1000_DEVICE_STATUS_SPEED_10 (0x0 << 6)
#define E1000_DEVICE_STATUS_SPEED_100 (0x1 << 6)
#define E1000_DEVICE_STATUS_SPEED_1000 (0x2 << 6)
#define E1000_DEVICE_STATUS_AUTO_SPEED_DETECTION_SHIFT 8
#define E1000_DEVICE_STATUS_PCI66 (1 << 11)
#define E1000_DEVICE_STATUS_BUS64 (1 << 12)
#define E1000_DEVICE_STATUS_2500_CAPABLE (1 << 12)
#define E1000_DEVICE_STATUS_PCIX (1 << 13)
#define E1000_DEVICE_STATUS_SPEED_2500 (1 << 13)
#define E1000_DEVICE_STATUS_PCIX_SPEED_MASK (0x3 << 14)
#define E1000_DEVICE_STATUS_PCIX_SPEED_50_66_MHZ (0x0 << 14)
#define E1000_DEVICE_STATUS_PCIX_SPEED_66_100_MHZ (0x1 << 14)
#define E1000_DEVICE_STATUS_PCIX_SPEED_100_133_MHZ (0x2 << 14)

//
// Define the software pins that are hooked up to MDIO for 82543.
//

#define E1000_DEVICE_CONTROL_MDIO_DIRECTION E1000_DEVICE_CONTROL_SDP2_DIRECTION
#define E1000_DEVICE_CONTROL_MDC_DIRECTION E1000_DEVICE_CONTROL_SDP3_DIRECTION
#define E1000_DEVICE_CONTROL_MDIO E1000_DEVICE_CONTROL_SDP2_DATA
#define E1000_DEVICE_CONTROL_MDC E1000_DEVICE_CONTROL_SDP3_DATA

//
// EEPROM/Flash control register bits.
//

#define E1000_EEPROM_CONTROL_CLOCK_INPUT (1 << 0)
#define E1000_EEPROM_CONTROL_CHIP_SELECT (1 << 1)
#define E1000_EEPROM_CONTROL_DATA_INPUT (1 << 2)
#define E1000_EEPROM_CONTROL_DATA_OUTPUT (1 << 3)
#define E1000_EEPROM_CONTROL_FLASH_WRITE_DISABLED (0x1 << 4)
#define E1000_EEPROM_CONTROL_FLASH_WRITE_ENABLED (0x2 << 4)
#define E1000_EEPROM_CONTROL_FLASH_WRITE_MASK (0x3 << 4)
#define E1000_EEPROM_CONTROL_REQUEST_ACCESS (1 << 6)
#define E1000_EEPROM_CONTROL_GRANT_ACCESS (1 << 7)
#define E1000_EEPROM_CONTROL_PRESENT (1 << 8)
#define E1000_EEPROM_CONTROL_NM_SIZE (1 << 9)
#define E1000_EEPROM_CONTROL_MW_SPI_SIZE (1 << 10)
#define E1000_EEPROM_CONTROL_SPI (1 << 13)

//
// Extended device control register bits.
//

#define E1000_EXTENDED_CONTROL_GPI0_ENABLE (1 << 0)
#define E1000_EXTENDED_CONTROL_GPI1_ENABLE (1 << 1)
#define E1000_EXTENDED_CONTROL_PHY_INTERRUPT_ENABLE (1 << 1)
#define E1000_EXTENDED_CONTROL_GPI2_ENABLE (1 << 2)
#define E1000_EXTENDED_CONTROL_GPI3_ENABLE (1 << 3)
#define E1000_EXTENDED_CONTROL_SDP4_DATA (1 << 4)
#define E1000_EXTENDED_CONTROL_SDP5_DATA (1 << 5)
#define E1000_EXTENDED_CONTROL_PHY_INTERRUPT (1 << 5)
#define E1000_EXTENDED_CONTROL_SDP6_DATA (1 << 6)
#define E1000_EXTENDED_CONTROL_SDP7_DATA (1 << 7)
#define E1000_EXTENDED_CONTROL_SDP4_DIRECTION (1 << 8)
#define E1000_EXTENDED_CONTROL_SDP5_DIRECTION (1 << 9)
#define E1000_EXTENDED_CONTROL_SDP6_DIRECTION (1 << 10)
#define E1000_EXTENDED_CONTROL_SDP7_DIRECTION (1 << 11)
#define E1000_EXTENDED_CONTROL_ASD_CHECK (1 << 12)
#define E1000_EXTENDED_CONTROL_EEPROM_RESET (1 << 13)
#define E1000_EXTENDED_CONTROL_SPEED_BYPASS (1 << 15)
#define E1000_EXTENDED_CONTROL_RELAXED_ORDERING_DISABLED (1 << 17)
#define E1000_EXTENDED_CONTROL_POWER_DOWN (1 << 21)
#define E1000_EXTENDED_CONTROL_LINK_MASK (0x3 << 22)
#define E1000_EXTENDED_CONTROL_LINK_1000BASE_KX (0x1 << 22)
#define E1000_EXTENDED_CONTROL_LINK_SERDES (0x2 << 22)
#define E1000_EXTENDED_CONTROL_LINK_TBI (0x3 << 22)
#define E1000_EXTENDED_CONTROL_DRIVER_LOADED (1 << 28)

//
// MDI control register bits.
//

#define E1000_MDI_CONTROL_REGISTER_SHIFT 16
#define E1000_MDI_CONTROL_PHY_ADDRESS_SHIFT 21
#define E1000_MDI_CONTROL_PHY_OP_SHIFT 26
#define E1000_MDI_CONTROL_READY (1 << 28)
#define E1000_MDI_CONTROL_INTERRUPT_ENABLE (1 << 29)
#define E1000_MDI_CONTROL_ERROR (1 << 30)

//
// PCS configuration word 0 register bits.
//

#define E1000_PCS_CONFIGURATION_PCS_ENABLE (1 << 3)
#define E1000_PCS_CONFIGURATION_PCS_ISOLATE (1 << 30)
#define E1000_PCS_CONFIGURATION_PCS_SOFT_RESET (1 << 31)

//
// PCS link control register bits.
//

#define E1000_PCS_CONTROL_FORCED_LINK_VALUE (1 << 0)
#define E1000_PCS_CONTROL_FORCED_SPEED_10 (0x0 << 1)
#define E1000_PCS_CONTROL_FORCED_SPEED_100 (0x1 << 1)
#define E1000_PCS_CONTROL_FORCED_SPEED_1000 (0x2 << 1)
#define E1000_PCS_CONTROL_FORCED_DUPLEX_FULL (1 << 3)
#define E1000_PCS_CONTROL_FORCE_SPEED_DUPLEX (1 << 4)
#define E1000_PCS_CONTROL_FORCE_LINK (1 << 5)
#define E1000_PCS_CONTROL_LINK_LATCH_LOW (1 << 6)
#define E1000_PCS_CONTROL_FORCE_FLOW_CONTROL (1 << 7)
#define E1000_PCS_CONTROL_AUTONEGOTIATE_ENABLE (1 << 16)
#define E1000_PCS_CONTROL_AUTONEGOTIATE_RESTART (1 << 17)
#define E1000_PCS_CONTROL_AUTONEGOTIATE_TIMEOUT_ENABLE (1 << 18)
#define E1000_PCS_CONTROL_AUTONEGOTIATE_SGMII_BYPASS (1 << 19)
#define E1000_PCS_CONTROL_AUTONEGOTIATE_SGMII_TRIGGER (1 << 20)
#define E1000_PCS_CONTROL_FAST_LINK_TIMER (1 << 24)
#define E1000_PCS_CONTROL_LINK_OK_FIX (1 << 25)

//
// Receive control register bits
//

#define E1000_RX_CONTROL_RESET (1 << 0)
#define E1000_RX_CONTROL_ENABLE (1 << 1)
#define E1000_RX_CONTROL_STORE_BAD_PACKETS (1 << 2)
#define E1000_RX_CONTROL_UNICAST_PROMISCUOUS (1 << 3)
#define E1000_RX_CONTROL_MULTICAST_PROMISCUOUS (1 << 4)
#define E1000_RX_CONTROL_LONG_PACKET_ENABLE (1 << 5)
#define E1000_RX_CONTROL_LOOPBACK (1 << 6)
#define E1000_RX_CONTROL_DESCRIPTOR_MINIMUM_THRESHOLD_SHIFT 8
#define E1000_RX_CONTROL_DESCRIPTOR_MINIMUM_THRESHOLD_MASK (0x3 << 8)
#define E1000_RX_CONTROL_MULTICAST_OFFSET_SHIFT 12
#define E1000_RX_CONTROL_MULTICAST_OFFSET_MASK (0x3 << 12)
#define E1000_RX_CONTROL_BROADCAST_ACCEPT (1 << 15)
#define E1000_RX_CONTROL_BUFFER_SIZE_SHIFT 16
#define E1000_RX_CONTROL_BUFFER_SIZE_2K (0x0 << 16)
#define E1000_RX_CONTROL_BUFFER_SIZE_1K (0x1 << 16)
#define E1000_RX_CONTROL_BUFFER_SIZE_512 (0x2 << 16)
#define E1000_RX_CONTROL_BUFFER_SIZE_256 (0x3 << 16)
#define E1000_RX_CONTROL_BUFFER_SIZE_X_16K (0x1 << 16)
#define E1000_RX_CONTROL_BUFFER_SIZE_X_8K (0x2 << 16)
#define E1000_RX_CONTROL_BUFFER_SIZE_X_4K (0x3 << 16)
#define E1000_RX_CONTROL_BUFFER_SIZE_MASK (0x3 << 16)
#define E1000_RX_CONTROL_VLAN_FILTER (1 << 18)
#define E1000_RX_CONTROL_CANONICAL_FORM_INDICATOR_ENABLE (1 << 19)
#define E1000_RX_CONTROL_CANONICAL_FORM_INDICATOR_VALUE (1 << 20)
#define E1000_RX_CONTROL_DISCARD_PAUSE_FRAMES (1 << 22)
#define E1000_RX_CONTROL_PASS_MAC_CONTROL (1 << 23)
#define E1000_RX_CONTROL_BUFFER_SIZE_EXTENSION (1 << 25)
#define E1000_RX_CONTROL_STRIP_CRC (1 << 26)

//
// Receive checksum control register bits.
//

#define E1000_RX_CHECKSUM_START 14
#define E1000_RX_CHECKSUM_IP_OFFLOAD (1 << 8)
#define E1000_RX_CHECKSUM_TCP_UDP_OFFLOAD (1 << 9)
#define E1000_RX_CHECKSUM_IPV6_OFFLOAD (1 << 10)

//
// Receive descriptor control register bits.
//

#define E1000_RXD_CONTROL_HOST_THRESHOLD_SHIFT 8
#define E1000_RXD_CONTROL_WRITE_THRESHOLD_SHIFT 16
#define E1000_RXD_CONTROL_ENABLE (1 << 25)
#define E1000_RXD_CONTROL_FLUSH (1 << 26)

#define E1000_RXD_CONTROL_DEFAULT_VALUE_I354 \
    (12 | \
     (8 << E1000_RXD_CONTROL_HOST_THRESHOLD_SHIFT) | \
     (1 << E1000_RXD_CONTROL_WRITE_THRESHOLD_SHIFT) | \
     E1000_RXD_CONTROL_ENABLE)

#define E1000_RXD_CONTROL_DEFAULT_VALUE \
    (8 | \
     (8 << E1000_RXD_CONTROL_HOST_THRESHOLD_SHIFT) | \
     (1 << E1000_RXD_CONTROL_WRITE_THRESHOLD_SHIFT) | \
     E1000_RXD_CONTROL_ENABLE)

//
// Transmit control register bits.
//

#define E1000_TX_CONTROL_ENABLE (1 << 1)
#define E1000_TX_CONTROL_PAD_SHORT_PACKETS (1 << 3)
#define E1000_TX_CONTROL_COLLISION_THRESHOLD_SHIFT 4
#define E1000_TX_CONTROL_COLLISION_DISTANCE_MASK (0x3FF << 12)
#define E1000_TX_CONTROL_COLLISION_DISTANCE_SHIFT 12
#define E1000_TX_CONTROL_XOFF_TRANSMISSION (1 << 22)
#define E1000_TX_CONTROL_RETRANSMIT_LATE_COLLISION (1 << 24)
#define E1000_TX_CONTROL_NO_RETRANSMIT_UNDERRUN (1 << 25)

#define E1000_TX_CONTROL_DEFAULT_COLLISION_DISTANCE 63

//
// Transmit configuration word register bits.
//

#define E1000_TX_CONFIGURATION_FULL_DUPLEX (1 << 5)
#define E1000_TX_CONFIGURATION_HALF_DUPLEX (1 << 6)
#define E1000_TX_CONFIGURATION_PAUSE (1 << 7)
#define E1000_TX_CONFIGURATION_PAUSE_DIRECTION (1 << 8)
#define E1000_TX_CONFIGURATION_PAUSE_MASK (0x3 << 7)
#define E1000_TX_CONFIGURATION_REMOTE_FAULT (0x2 << 12)
#define E1000_TX_CONFIGURATION_NEXT_PAGE (1 << 15)
#define E1000_TX_CONFIGURATION_C_ORDERED_SETS (1 << 30)
#define E1000_TX_CONFIGURATION_AUTONEGOTIATE_ENABLE (1 << 31)

//
// Transmit inter-packet gap and interrupt delay default values.
//

#define E1000_TX_IPG_VALUE ((8 << 10) | (6 << 20) | 8)
#define E1000_TX_INTERRUPT_DELAY 8
#define E1000_TX_INTERRUPT_ABSOLUTE_DELAY 32

//
// Transmit descriptor control register bits.
//

#define E1000_TXD_CONTROL_PREFETCH_THRESHOLD_SHIFT 8
#define E1000_TXD_CONTROL_WRITEBACK_THRESHOLD_SHIFT 16
#define E1000_TXD_CONTROL_WRITEBACK_THRESHOLD_MASK (0x3F << 16)
#define E1000_TXD_CONTROL_DESCRIPTOR_GRANULARITY (1 << 24)
#define E1000_TXD_CONTROL_LOW_THRESHOLD_SHIFT 25
#define E1000_TXD_CONTROL_LOW_THRESHOLD_MASK (0x7F << 25)
#define E1000_TXD_CONTROL_ENABLE (1 << 25)
#define E1000_TXD_CONTROL_FLUSH (1 << 26)

#define E1000_TXD_CONTROL_DEFAULT_VALUE_I354 \
    (20 | \
     (8 << E1000_TXD_CONTROL_PREFETCH_THRESHOLD_SHIFT) | \
     (16 << E1000_TXD_CONTROL_WRITEBACK_THRESHOLD_SHIFT) | \
     E1000_TXD_CONTROL_ENABLE)

#define E1000_TXD_CONTROL_DEFAULT_VALUE \
    (8 | \
     (8 << E1000_TXD_CONTROL_PREFETCH_THRESHOLD_SHIFT) | \
     (16 << E1000_TXD_CONTROL_WRITEBACK_THRESHOLD_SHIFT) | \
     E1000_TXD_CONTROL_ENABLE)

//
// Receive address register bits.
//

#define E1000_RECEIVE_ADDRESS_HIGH_VALID (1 << 31)

//
// Interrupt mask bits.
//

#define E1000_INTERRUPT_TX_DESCRIPTOR_WRITTEN_BACK (1 << 0)
#define E1000_INTERRUPT_TX_QUEUE_EMPTY (1 << 1)
#define E1000_INTERRUPT_LINK_STATUS_CHANGE (1 << 2)
#define E1000_INTERRUPT_RX_SEQUENCE_ERROR (1 << 3)
#define E1000_INTERRUPT_RX_MIN_THRESHOLD (1 << 4)
#define E1000_INTERRUPT_RX_OVERRUN (1 << 6)
#define E1000_INTERRUPT_RX_TIMER (1 << 7)
#define E1000_INTERRUPT_MDIO_ACCESS_COMPLETE (1 << 9)
#define E1000_INTERRUPT_RX_ORDERED (1 << 10)
#define E1000_INTERRUPT_PHY_INTERRUPT (1 << 12)
#define E1000_INTERRUPT_TX_LOW_THRESHOLD (1 << 15)
#define E1000_INTERRUPT_SMALL_RX_PACKET (1 << 16)

//
// Define the mask of interrupts to enable here.
//

#define E1000_INTERRUPT_ENABLE_MASK \
    (E1000_INTERRUPT_RX_TIMER | \
     E1000_INTERRUPT_TX_DESCRIPTOR_WRITTEN_BACK | \
     E1000_INTERRUPT_RX_MIN_THRESHOLD | \
     E1000_INTERRUPT_RX_SEQUENCE_ERROR | \
     E1000_INTERRUPT_LINK_STATUS_CHANGE)

//
// Management control register bits
//

#define E1000_MANAGEMENT_SMBUS_ENABLE (1 << 0)
#define E1000_MANAGEMENT_ASF_MODE (1 << 1)
#define E1000_MANAGEMENT_RESET_ON_FORCE_TCO (1 << 2)
#define E1000_MANAGEMENT_FLEX_FILTER_ENABLE (1 << 5)
#define E1000_MANAGEMENT_IP4_ADDRESS_VALID (1 << 6)
#define E1000_MANAGEMENT_IP6_ADDRESS_VALID (1 << 7)
#define E1000_MANAGEMENT_RCMP_026F_FILTERING (1 << 8)
#define E1000_MANAGEMENT_RCMP_0298_FILTERING (1 << 9)
#define E1000_MANAGEMENT_ARP_REQUEST_FILTERING (1 << 13)
#define E1000_MANAGEMENT_ARP_RESPONSE_FILTERING (1 << 15)

//
// Microwire EEPROM commands
//

#define E1000_EEPROM_MICROWIRE_WRITE 0x05
#define E1000_EEPROM_MICROWIRE_READ 0x06
#define E1000_EEPROM_MICROWIRE_ERASE 0x07
#define E1000_EEPROM_MICROWIRE_WRITE_DISABLE 0x10
#define E1000_EEPROM_MICROWIRE_WRITE_ENABLE 0x13

//
// SPI EEPROM commands
//

#define E1000_EEPROM_SPI_WRITE_STATUS 0x01
#define E1000_EEPROM_SPI_WRITE 0x02
#define E1000_EEPROM_SPI_READ 0x03
#define E1000_EEPROM_SPI_WRITE_DISABLE 0x04
#define E1000_EEPROM_SPI_READ_STATUS 0x05
#define E1000_EEPROM_SPI_WRITE_ENABLE 0x06
#define E1000_EEPROM_SPI_ADDRESS8 0x08
#define E1000_EEPROM_SPI_ERASE_4K 0x20
#define E1000_EEPROM_SPI_ERASE_64K 0xD8
#define E1000_EEPROM_SPI_ERASE_256 0xDB

//
// SPI EEPROM status register bits.
//

#define E1000_EEPROM_SPI_STATUS_BUSY 0x01
#define E1000_EEPROM_SPI_STATUS_WRITE_ENABLE 0x02
#define E1000_EEPROM_SPI_STATUS_BP0 0x04
#define E1000_EEPROM_SPI_STATUS_BP1 0x08
#define E1000_EEPROM_SPI_STATUS_WRITE_PROTECT 0x80

//
// Random PHY definitions.
//

#define E1000_PHY_REVISION_MASK 0xFFFFFFF0

#define E1000_PHY_MAX_MULTI_PAGE_REGISTER 0x0F
#define E1000_PHY_REGISTER_ADDRESS 0x1F
#define E1000_PHY_PREAMBLE 0xFFFFFFFF
#define E1000_PHY_PREAMBLE_SIZE 32
#define E1000_PHY_SOF 0x1
#define E1000_PHY_OP_WRITE 0x1
#define E1000_PHY_OP_READ 0x2
#define E1000_PHY_TURNAROUND 0x2

//
// PHY registers.
//

#define E1000_PHY_CONTROL 0x00
#define E1000_PHY_STATUS 0x01
#define E1000_PHY_ID1 0x02
#define E1000_PHY_ID2 0x03
#define E1000_PHY_AUTONEGOTIATE_ADVERTISEMENT 0x04
#define E1000_PHY_LINK_PARTNER_ABILITY 0x05
#define E1000_PHY_AUTONEGOTIATE_EXPANSION 0x06
#define E1000_PHY_NEXT_PAGE_TX 0x07
#define E1000_PHY_LINK_PARTNER_NEXT_PAGE 0x08
#define E1000_PHY_1000T_CONTROL 0x09
#define E1000_PHY_EXTENDED_STATUS 0x0F

//
// PHY control register bits.
//

#define E1000_PHY_CONTROL_COLLISION_TEST_ENABLE 0x0080
#define E1000_PHY_CONTROL_FULL_DUPLEX 0x0100
#define E1000_PHY_CONTROL_RESTART_AUTO_NEGOTIATION 0x0200
#define E1000_PHY_CONTROL_ISOLATE 0x0400
#define E1000_PHY_CONTROL_POWER_DOWN 0x0800
#define E1000_PHY_CONTROL_AUTO_NEGOTIATE_ENABLE 0x1000
#define E1000_PHY_CONTROL_LOOPBACK 0x4000
#define E1000_PHY_CONTROL_RESET 0x8000

//
// PHY status register bits.
//

#define E1000_PHY_STATUS_EXTENDED_CAPABILITIES 0x0001
#define E1000_PHY_STATUS_JABBER_DETECTED 0x0002
#define E1000_PHY_STATUS_LINK 0x0004
#define E1000_PHY_STATUS_AUTONEGOTIATION_CAPABLE 0x0008
#define E1000_PHY_STATUS_REMOTE_FAULT 0x0010
#define E1000_PHY_STATUS_AUTONEGOTIATION_COMPLETE 0x0020
#define E1000_PHY_STATUS_SUPPRESS_PREAMBLE 0x0040
#define E1000_PHY_STATUS_EXTENDED_STATUS 0x0100
#define E1000_PHY_STATUS_100T2_HALF_CAPABLE 0x0200
#define E1000_PHY_STATUS_100T2_FULL_CAPABLE 0x0400
#define E1000_PHY_STATUS_10T_HALF_CAPABLE 0x0800
#define E1000_PHY_STATUS_10T_FULL_CAPABLE 0x1000
#define E1000_PHY_STATUS_100X_HALF_CAPABLE 0x2000
#define E1000_PHY_STATUS_100X_FULL_CAPABLE 0x4000
#define E1000_PHY_STATUS_100T4_CAPABLE 0x8000

//
// PHY autonegotiate advertise register bits.
//

#define E1000_AUTONEGOTIATE_ADVERTISE_10_HALF 0x0020
#define E1000_AUTONEGOTIATE_ADVERTISE_10_FULL 0x0040
#define E1000_AUTONEGOTIATE_ADVERTISE_100_HALF 0x0080
#define E1000_AUTONEGOTIATE_ADVERTISE_100_FULL 0x0100

//
// PHY 1000T control register bits.
//

#define E1000_1000T_CONTROL_ADVERTISE_1000_FULL 0x0200

//
// IGP01E1000 specific PHY registers.
//

#define E1000_IGP1_PHY_PORT_CONFIGURATION 0x10
#define E1000_IGP1_PHY_PORT_STATUS 0x11
#define E1000_IGP1_PHY_PORT_CONTROL 0x12
#define E1000_IGP1_PHY_LINK_HEALTH 0x13
#define E1000_IGP1_GMII_FIFO 0x14
#define E1000_IGP1_PHY_CHANNEL_QUALITY 0x19
#define E1000_IGP1_PHY_PAGE_SELECT 0x1F

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the SCB register offsets, in bytes.
//

typedef enum _E1000_REGISTER {
    E1000DeviceControl = 0x0000,
    E1000DeviceStatus = 0x0008,
    E1000EepromControl = 0x0010,
    E1000EepromRead = 0x0014,
    E1000ExtendedDeviceControl = 0x0018,
    E1000FlashAccess = 0x001C,
    E1000MdiControl = 0x0020,
    E1000SerdesControl = 0x0024,
    E1000FlowControlAddressLow = 0x0028,
    E1000FlowControlAddressHigh = 0x002C,
    E1000FlowControlType = 0x0030,
    E1000KumControl = 0x0034,
    E1000VlanEthertype = 0x0038,
    E1000FlowControlTransmitTimerValue = 0x0170,
    E1000TxConfigurationWord = 0x0178,
    E1000RxConfigurationWord = 0x0180,
    E1000LedControl = 0x0E00,
    E1000PacketBufferAllocation = 0x1000,
    E1000MngEepromControl = 0x1010,
    E1000FirmwareSync = 0x5B5C,
    E1000InterruptCauseRead = 0x00C0,
    E1000InterruptThrottlingRate = 0x00C4,
    E1000InterruptCauseSet = 0x00C8,
    E1000InterruptMaskSet = 0x00D0,
    E1000InterruptMaskClear = 0x00D8,
    E1000InterruptAckAutoMask = 0x00E0,
    E1000RxControl = 0x0100,
    E1000EarlyRxThreshold = 0x2008,
    E1000FlowRxThresholdLow = 0x2160,
    E1000FlowRxThresholdHigh = 0x2168,
    E1000SplitRxControl = 0x2170,
    E1000RxDescriptorBaseLow0 = 0x2800,
    E1000RxDescriptorBaseHigh0 = 0x2804,
    E1000RxDescriptorLength0 = 0x2808,
    E1000RxDescriptorHead0 = 0x2810,
    E1000RxDescriptorTail0 = 0x2818,
    E1000RxInterruptDelayTimer = 0x2820,
    E1000RxDescriptorControl0 = 0x2828,
    E1000RxInterruptAbsoluteDelayTimer = 0x282C,
    E1000RxSmallPacketDetect = 0x2C00,
    E1000RxAckInterruptDelay = 0x2C08,
    E1000CpuVector = 0x2C10,
    E1000PcsConfiguration = 0x4200,
    E1000PcsControl = 0x4208,
    E1000PcsLinkStatus = 0x420C,
    E1000PcsDebug0 = 0x4210,
    E1000PcsDebug1 = 0x4214,
    E1000PcsAutonegotiateAdvertisement = 0x4218,
    E1000PcsLinkPartnerAbility = 0x421C,
    E1000PcsAutonegotiateNextPage = 0x4220,
    E1000PcsLinkPartnerNextPage = 0x4224,
    E1000RxChecksumControl = 0x5000,
    E1000RxFilterControl = 0x5008,
    E1000MulticastTable = 0x5200,
    E1000RxAddressLow = 0x5400,
    E1000RxAddressHigh = 0x5404,
    E1000VlanFilterTable = 0x5600,
    E1000MultipleRxQueuesCommand = 0x5818,
    E1000RssInterruptMask = 0x5864,
    E1000RssInterruptRequest = 0x5868,
    E1000RedirectionTable = 0x5C00,
    E1000RssRandomKey = 0x5C80,
    E1000TxControl = 0x0400,
    E1000TxIpg = 0x0410,
    E1000AdaptiveIpsThrottle = 0x0458,
    E1000TxDescriptorBaseLow0 = 0x3800,
    E1000TxDescriptorBaseHigh0 = 0x3804,
    E1000TxDescriptorLength0 = 0x3808,
    E1000TxDescriptorHead0 = 0x3810,
    E1000TxDescriptorTail0 = 0x3818,
    E1000TxInterruptDelayValue = 0x3820,
    E1000TxDescriptorControl0 = 0x3828,
    E1000TxAbsoluteInterruptDelayValue = 0x382C,
    E1000TxArbitrationCounter0 = 0x3840,
    E1000WakeupControl = 0x5800,
    E1000WakeupFilterControl = 0x5808,
    E1000WakeupStatus = 0x5810,
    E1000IpAddressValid = 0x5838,
    E1000Ip4AddressTable = 0x5840,
    E1000Ip6AddressTable = 0x5880,
    E1000WakeupPacketLength = 0x5900,
    E1000WakeupPacketMemory = 0x5A00,
    E1000FlexibleFilterLengthTable = 0x5F00,
    E1000FlexibleFilterMaskTable = 0x9000,
    E1000FlexibleFilterValueTable = 0x9800,
    E1000ManagementControl = 0x5820,
    E1000PacketBufferEcc = 0x1100,
    E1000PcieControl = 0x5B00,
    E1000PcieStatisticsControl1 = 0x5B10,
    E1000PcieStatisticsControl2 = 0x5B14,
    E1000PcieStatisticsControl3 = 0x5B18,
    E1000PcieStatisticsControl4 = 0x5B1C,
    E1000PcieCounter0 = 0x5B20,
    E1000PcieCounter1 = 0x5B24,
    E1000PcieCounter2 = 0x5B28,
    E1000PcieCounter3 = 0x5B2C,
    E1000FunctionActivePowerState = 0x5B30,
    E1000SoftwareSemaphore = 0x5B50,
    E1000FirmwareSemaphore = 0x5B54,
} E1000_REGISTER, *PE1000_REGISTER;

typedef enum _E1000_MAC_TYPE {
    E1000MacInvalid,
    E1000Mac82543,
    E1000Mac82540,
    E1000Mac82545,
    E1000Mac82574,
    E1000MacI350,
    E1000MacI354,
} E1000_MAC_TYPE, *PE1000_MAC_TYPE;

typedef enum _E1000_PHY_TYPE {
    E1000PhyInvalid,
    E1000PhyUnknown,
    E1000PhyM88,
    E1000PhyIgp,
    E1000PhyIgp2,
    E1000PhyIgp3,
    E1000Phy8211,
    E1000Phy8201,
    E1000PhyGg82563,
    E1000PhyIfe,
    E1000PhyBm,
    E1000Phy82577,
    E1000Phy82578,
    E1000Phy82579,
    E1000PhyI217,
} E1000_PHY_TYPE, *PE1000_PHY_TYPE;

typedef enum _E1000_MEDIA_TYPE {
    E1000MediaUnknown,
    E1000MediaCopper,
    E1000MediaInternalSerdes
} E1000_MEDIA_TYPE, *PE1000_MEDIA_TYPE;

typedef enum _E1000_EEPROM_TYPE {
    E1000EepromMicrowire,
    E1000EepromSpi,
} E1000_EEPROM_TYPE, *PE1000_EEPROM_TYPE;

/*++

Structure Description:

    This structure defines the hardware mandated transmit descriptor format.

Members:

    Address - Stores the byte aligned physical address of the data to
        transmit.

    Length - Stores the length of the data.

    ChecksumOffset - Stores the offset from the beginning of the packet where
        a TCP checksum should be inserted.

    Command - Stores the command, usually transmit.

    Status - Stores the status bits.

    VlanTag - Stores the VLAN tag for the packet.

--*/

typedef struct _E1000_TX_DESCRIPTOR {
    ULONGLONG Address;
    USHORT Length;
    UCHAR ChecksumOffset;
    UCHAR Command;
    UCHAR Status;
    UCHAR ChecksumStart;
    USHORT VlanTag;
} PACKED E1000_TX_DESCRIPTOR, *PE1000_TX_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the hardware mandated format for a receive
    descriptor.

Members:

    Address - Stores the byte aligned buffer address where the received data is
        put.

    Length - Stores the length of the received data.

    Checksum - Stores the checksum of the packet.

    Status - Stores the status bits. See E1000_RX_STATUS_* definitions.

    Errors - Stores the receive error bits.

    VlanTag - Stores the VLAN information.

--*/

typedef struct _E1000_RX_DESCRIPTOR {
    ULONGLONG Address;
    USHORT Length;
    USHORT Checksum;
    UCHAR Status;
    UCHAR Errors;
    USHORT VlanTag;
} PACKED E1000_RX_DESCRIPTOR, *PE1000_RX_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the EEPROM configuration details for the given
    device.

Members:

    Type - Stores the EEPROM type.

    WordSize - Stores the size of a word in the EEPROM.

    OpcodeBits - Stores the size of an opcode in the EEPROM.

    AddressBits - Stores the number of bits in the address in the EEPROM.

    Delay - Stores the number of microseconds to wait for the EEPROM to
        complete the command.

    PageSize - Stores the size of a page in the EEPROM.

--*/

typedef struct _E1000_EEPROM_INFO {
    E1000_EEPROM_TYPE Type;
    USHORT WordSize;
    USHORT OpcodeBits;
    USHORT AddressBits;
    USHORT Delay;
    USHORT PageSize;
} E1000_EEPROM_INFO, *PE1000_EEPROM_INFO;

/*++

Structure Description:

    This structure defines an Intel e1000 LAN device.

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
        E1000's registers.

    FlashBase - Stores a pointer to the alternate memory BAR, used for mapping
        flash sometimes.

    NetworkLink - Stores a pointer to the core networking link.

    RxIoBuffer - Stores a pointer to the I/O buffer associated with the receive
        descriptors.

    RxDescriptors - Stores the pointer to the array of receive descriptors.

    RxPackets - Stores an array of pointers to packet buffers: one for each
        receive descriptor.

    RxListBegin - Stores the index of the beginning of the list, which is
        the oldest received frame and the first one to dispatch.

    RxListLock - Stores a pointer to a queued lock that protects the receive
        list.

    TxIoBuffer - Stores a pointer to the I/O buffer associated with
        the transmit descriptor list.

    TxDescriptors - Stores a pointer to the transmit descriptor array.

    TxPacket - Stores a pointer to the array of net packet buffers that
        go with each transmit descriptor.

    TxNextReap - Stores the index of the next packet to attempt to reap. If
        this equals the next to use, then the list is empty.

    TxNextToUse - Stores the index where the next command should be placed.

    TxListLock - Stores the lock protecting simultaneous software access
        to the transmit descriptors list.

    TxPacketList - Stores a list of network packets waiting to be sent.

    LinkSpeed - Stores the current link speed. If 0, the link is not active.

    LinkCheckTimer - Stores a pointer to the timer that fires periodically to
        see if the link is active.

    PendingStatusBits - Stores the bitfield of status bits that have yet to be
        dealt with by software.

    MacType - Stores the MAC type for this device.

    EepromInfo - Stores the EEPROM information.

    EepromMacAddress - Stores the default MAC address of the device.

    MediaType - Stores the type of the physical medium.

    PhyType - Stores the type of PHY connected to this device.

    PhyId - Stores the address of the PHY.

    PhyRevision - Stores the revision ID of the PHY.

    SupportedCapabilities - Stores the set of capabilities that this device
        supports. See NET_LINK_CAPABILITY_* for definitions.

    EnabledCapabilities - Stores the currently enabled capabilities on the
        devices. See NET_LINK_CAPABILITY_* for definitions.

    ConfigurationLock - Stores a queued lock that synchronizes changes to the
        enabled capabilities field and their supporting hardware registers.

--*/

typedef struct _E1000_DEVICE {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PVOID FlashBase;
    PNET_LINK NetworkLink;
    PIO_BUFFER RxIoBuffer;
    PE1000_RX_DESCRIPTOR RxDescriptors;
    PNET_PACKET_BUFFER *RxPackets;
    ULONG RxListBegin;
    PQUEUED_LOCK RxListLock;
    PIO_BUFFER TxIoBuffer;
    PE1000_TX_DESCRIPTOR TxDescriptors;
    PNET_PACKET_BUFFER *TxPacket;
    ULONG TxNextReap;
    ULONG TxNextToUse;
    PQUEUED_LOCK TxListLock;
    NET_PACKET_LIST TxPacketList;
    ULONGLONG LinkSpeed;
    PKTIMER LinkCheckTimer;
    ULONG PendingStatusBits;
    E1000_MAC_TYPE MacType;
    E1000_EEPROM_INFO EepromInfo;
    BYTE EepromMacAddress[ETHERNET_ADDRESS_SIZE];
    E1000_MEDIA_TYPE MediaType;
    E1000_PHY_TYPE PhyType;
    ULONG PhyId;
    ULONG PhyRevision;
    ULONG SupportedCapabilities;
    ULONG EnabledCapabilities;
    PQUEUED_LOCK ConfigurationLock;
} E1000_DEVICE, *PE1000_DEVICE;

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
E1000Send (
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
E1000GetSetInformation (
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
E1000pInitializeDeviceStructures (
    PE1000_DEVICE Device
    );

/*++

Routine Description:

    This routine performs housekeeping preparation for resetting and enabling
    an E1000 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

KSTATUS
E1000pResetDevice (
    PE1000_DEVICE Device
    );

/*++

Routine Description:

    This routine resets the E1000 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

VOID
E1000pEnableInterrupts (
    PE1000_DEVICE Device
    );

/*++

Routine Description:

    This routine enables interrupts on the E1000 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

INTERRUPT_STATUS
E1000pInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the e1000 interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e1000 device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
E1000pInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine processes interrupts for the e1000 controller at low level.

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
E1000pAddNetworkDevice (
    PE1000_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

