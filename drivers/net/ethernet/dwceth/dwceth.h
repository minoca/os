/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwceth.h

Abstract:

    This header contains definitions for the DesignWare Ethernet controller.

Author:

    Evan Green 5-Dec-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write registers in the controller.
//

#define DWE_READ(_Controller, _Register) \
    HlReadRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register))

#define DWE_WRITE(_Controller, _Register, _Value)                          \
    HlWriteRegister32((PUCHAR)(_Controller)->ControllerBase + (_Register), \
                      (_Value))

//
// This macro creates a descriptor value given the two buffer sizes.
//

#define DWE_BUFFER_SIZE(_Size1, _Size2) \
    (((_Size1) & DWE_BUFFER_SIZE_MASK) | \
     (((_Size2) & DWE_BUFFER_SIZE_MASK) << DWE_BUFFER2_SHIFT))

//
// These macros return the register numbers for one of the programmable MAC
// addresses.
//

#define DWE_MAC_ADDRESS_HIGH(_Index) \
    (DweRegisterMacAddress0High + ((_Index) * 8))

#define DWE_MAC_ADDRESS_LOW(_Index) \
    (DweRegisterMacAddress0Low + ((_Index) * 8))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the DesignWare Ethernet controller allocation tag: DwEt
//

#define DWE_ALLOCATION_TAG 0x74457744

//
// Define the size of receive frame data.
//

#define DWE_RECEIVE_FRAME_DATA_SIZE 1520

//
// Define the number of receive buffers that will be allocated for the
// controller.
//

#define DWE_RECEIVE_FRAME_COUNT 32

//
// Define the number of transmit descriptors to allocate for the controller.
//

#define DWE_TRANSMIT_DESCRIPTOR_COUNT 32

//
// Define how often to poll the link state, in seconds.
//

#define DWE_LINK_CHECK_INTERVAL 5

//
// Define the number of seconds to wait for the MII to respond.
//

#define DWE_MII_TIMEOUT 5

#define DWE_MII_CLOCK_VALUE 2

//
// Define receive descriptor status bits. Some bits have double (or triple)
// meanings depending on what features are enabled.
//

//
// If timestamping or checksum offloading is enabled, bit 0 describes whether
// or not the extended status word is valid. If neither of these features are
// available, the bit describes whether MAC Address 0 matched the packet
// destination (1) or MAC Address 1-15 matched (0).
//

#define DWE_RX_STATUS_EXTENDED_STATUS           (1 << 0)
#define DWE_RX_STATUS_MAC0_MATCH                (1 << 0)

#define DWE_RX_STATUS_CRC_ERROR                 (1 << 1)
#define DWE_RX_STATUS_DRIBBLE_BIT_ERROR         (1 << 2)
#define DWE_RX_STATUS_RECEIVE_ERROR             (1 << 3)
#define DWE_RX_STATUS_WATCHDOG_TIMEOUT          (1 << 4)
#define DWE_RX_STATUS_ETHERTYPE                 (1 << 5)
#define DWE_RX_STATUS_LATE_COLLISION            (1 << 6)

//
// If timestamping is enabled, this bit indicates the timestamp fields are
// valid. If IP checksumming is enabled, this bit indicates that the IPv4
// header checksum failed. Otherwise, this bit indicates the giant frame status.
//

#define DWE_RX_STATUS_TIMESTAMP_AVAILABLE       (1 << 7)
#define DWE_RX_STATUS_IP_CHECKSUM_ERROR         (1 << 7)
#define DWE_RX_STATUS_GIANT_FRAME               (1 << 7)

#define DWE_RX_STATUS_LAST_DESCRIPTOR           (1 << 8)
#define DWE_RX_STATUS_FIRST_DESCRIPTOR          (1 << 9)
#define DWE_RX_STATUS_VLAN                      (1 << 10)
#define DWE_RX_STATUS_LENGTH_ERROR              (1 << 11)
#define DWE_RX_STATUS_SOURCE_FILTER_FAIL        (1 << 13)
#define DWE_RX_STATUS_DESCRIPTOR_ERROR          (1 << 14)
#define DWE_RX_STATUS_ERROR_SUMMARY             (1 << 15)
#define DWE_RX_STATUS_FRAME_LENGTH_SHIFT        16
#define DWE_RX_STATUS_FRAME_LENGTH_MASK         0x3FFF
#define DWE_RX_STATUS_DESTINATION_FILTER_FAIL   (1 << 30)
#define DWE_RX_STATUS_DMA_OWNED                 (1 << 31)

#define DWE_RX_STATUS_ERROR_MASK                                        \
    (DWE_RX_STATUS_LENGTH_ERROR | DWE_RX_STATUS_SOURCE_FILTER_FAIL |    \
     DWE_RX_STATUS_DESCRIPTOR_ERROR | DWE_RX_STATUS_ERROR_SUMMARY |     \
     DWE_RX_STATUS_DESTINATION_FILTER_FAIL)

//
// Define the generic descriptor buffer size bits.
//

#define DWE_BUFFER_SIZE_MASK    0x00000FFF
#define DWE_BUFFER2_SHIFT       16

//
// Define the receive descriptor buffer size bits.
//

#define DWE_RX_SIZE_CHAINED             (1 << 14)
#define DWE_RX_SIZE_END_OF_RING         (1 << 15)
#define DWE_RX_SIZE_DISABLE_INTERRUPT   (1 << 31)

//
// Define receive descriptor extended status bits.
//

#define DWE_RX_STATUS2_IP_PAYLOAD_TYPE_MASK         0x00000007
#define DWE_RX_STATUS2_IP_PAYLOAD_NONE              0
#define DWE_RX_STATUS2_IP_PAYLOAD_UDP               1
#define DWE_RX_STATUS2_IP_PAYLOAD_TCP               2
#define DWE_RX_STATUS2_IP_PAYLOAD_ICMP              3
#define DWE_RX_STATUS2_IP_HEADER_ERROR              (1 << 3)
#define DWE_RX_STATUS2_IP_PAYLOAD_ERROR             (1 << 4)
#define DWE_RX_STATUS2_IP_CHECKSUM_BYPASSED         (1 << 5)
#define DWE_RX_STATUS2_IP4_PACKET_RECEIVED          (1 << 6)
#define DWE_RX_STATUS2_IP6_PACKET_RECEIVED          (1 << 7)
#define DWE_RX_STATUS2_MESSAGE_TYPE_MASK            0x00000F00
#define DWE_RX_STATUS2_MESAGE_TYPE_SHIFT            8
#define DWE_RX_STATUS2_MESSAGE_NONE                 0
#define DWE_RX_STATUS2_MESSAGE_SYNC                 1
#define DWE_RX_STATUS2_MESSAGE_FOLLOW_UP            2
#define DWE_RX_STATUS2_MESSAGE_DELAY_REQUEST        3
#define DWE_RX_STATUS2_MESSAGE_DELAY_RESPONSE       4
#define DWE_RX_STATUS2_MESSAGE_PEER_DELAY_REQUEST   5
#define DWE_RX_STATUS2_MESSAGE_PEER_DELAY_RESPONSE  6
#define DWE_RX_STATUS2_MESSAGE_PEER_DELAY_FOLLOW_UP 7
#define DWE_RX_STATUS2_MESSAGE_ANNOUNCE             8
#define DWE_RX_STATUS2_MESSAGE_MANAGEMENT           9
#define DWE_RX_STATUS2_MESSAGE_SIGNALING            10
#define DWE_RX_STATUS2_MESSAGE_RESERVED             15
#define DWE_RX_STATUS2_PTP_FRAME_TYPE               (1 << 12)
#define DWE_RX_STATUS2_PTP_VERSION                  (1 << 13)
#define DWE_RX_STATUS2_TIMESTAMP_DROPPED            (1 << 14)
#define DWE_RX_STATUS2_LAYER_3_FILTER_MATCH         (1 << 24)
#define DWE_RX_STATUS2_LAYER_4_FILTER_MATCH         (1 << 25)
#define DWE_RX_STATUS2_LAYER_FILTER_MASK            0x00000003
#define DWE_RX_STATUS2_LAYER_FILTER_SHIFT           26

//
// Define transmit descriptor control/status bits.
//

#define DWE_TX_CONTROL_DEFERRED                     (1 << 0)
#define DWE_TX_CONTROL_UNDERFLOW_ERROR              (1 << 1)
#define DWE_TX_CONTROL_EXCESSIVE_DEFERRAL           (1 << 2)
#define DWE_TX_CONTROL_COLLISION_COUNT_MASK         0x0000000F
#define DWE_TX_CONTROL_COLLISION_COUNT_SHIFT        3
#define DWE_TX_CONTROL_VLAN                         (1 << 7)
#define DWE_TX_CONTROL_EXCESSIVE_COLLISION          (1 << 8)
#define DWE_TX_CONTROL_NO_CARRIER                   (1 << 10)
#define DWE_TX_CONTROL_LOST_CARRIER                 (1 << 11)
#define DWE_TX_CONTROL_IP_PAYLOAD_ERROR             (1 << 12)
#define DWE_TX_CONTROL_FRAME_FLUSHED                (1 << 13)
#define DWE_TX_CONTROL_JABBER_TIMEOUT               (1 << 14)
#define DWE_TX_CONTROL_ERROR_SUMMARY                (1 << 15)
#define DWE_TX_CONTROL_IP_HEADER_ERROR              (1 << 16)
#define DWE_TX_CONTROL_TRANSMIT_TIMESTAMP_STATUS    (1 << 17)
#define DWE_TX_CONTROL_CHAINED                      (1 << 20)
#define DWE_TX_CONTROL_END_OF_RING                  (1 << 21)
#define DWE_TX_CONTROL_CHECKSUM_NONE                (0x0 << 22)
#define DWE_TX_CONTROL_CHECKSUM_IP_HEADER           (0x1 << 22)
#define DWE_TX_CONTROL_CHECKSUM_IP                  (0x2 << 22)
#define DWE_TX_CONTROL_CHECKSUM_PSEUDOHEADER        (0x3 << 22)
#define DWE_TX_CONTROL_TRANSMIT_TIMESTAMP           (1 << 25)
#define DWE_TX_CONTROL_DISABLE_PAD                  (1 << 26)
#define DWE_TX_CONTROL_DISABLE_CRC                  (1 << 27)
#define DWE_TX_CONTROL_FIRST_SEGMENT                (1 << 28)
#define DWE_TX_CONTROL_LAST_SEGMENT                 (1 << 29)
#define DWE_TX_CONTROL_INTERRUPT_ON_COMPLETE        (1 << 30)
#define DWE_TX_CONTROL_DMA_OWNED                    (1 << 31)

#define DWE_TX_CONTROL_ERROR_MASK \
    (DWE_TX_CONTROL_UNDERFLOW_ERROR | DWE_TX_CONTROL_EXCESSIVE_DEFERRAL | \
     DWE_TX_CONTROL_EXCESSIVE_COLLISION | DWE_TX_CONTROL_NO_CARRIER | \
     DWE_TX_CONTROL_LOST_CARRIER | DWE_TX_CONTROL_IP_PAYLOAD_ERROR | \
     DWE_TX_CONTROL_JABBER_TIMEOUT | DWE_TX_CONTROL_ERROR_SUMMARY | \
     DWE_TX_CONTROL_IP_HEADER_ERROR)

//
// Define MAC configuration register bit definitions.
//

#define DWE_MAC_CONFIGURATION_7_BYTE_PREAMBLE                   (0x0 << 0)
#define DWE_MAC_CONFIGURATION_5_BYTE_PREAMBLE                   (0x1 << 0)
#define DWE_MAC_CONFIGURATION_3_BYTE_PREAMBLE                   (0x2 << 0)
#define DWE_MAC_CONFIGURATION_RECEIVER_ENABLE                   (1 << 2)
#define DWE_MAC_CONFIGURATION_TRANSMITTER_ENABLE                (1 << 3)
#define DWE_MAC_CONFIGURATION_DEFERRAL_CHECK                    (1 << 4)
#define DWE_MAC_CONFIGURATION_BACKOFF_LIMIT_10                  (0x0 << 5)
#define DWE_MAC_CONFIGURATION_BACKOFF_LIMIT_8                   (0x1 << 5)
#define DWE_MAC_CONFIGURATION_BACKOFF_LIMIT_4                   (0x2 << 5)
#define DWE_MAC_CONFIGURATION_BACKOFF_LIMIT_1                   (0x3 << 5)
#define DWE_MAC_CONFIGURATION_AUTO_PAD_CRC_STRIPPING            (1 << 7)
#define DWE_MAC_CONFIGURATION_DISABLE_RETRY                     (1 << 9)
#define DWE_MAC_CONFIGURATION_CHECKSUM_OFFLOAD                  (1 << 10)
#define DWE_MAC_CONFIGURATION_DUPLEX_MODE                       (1 << 11)
#define DWE_MAC_CONFIGURATION_LOOPBACK_MODE                     (1 << 12)
#define DWE_MAC_CONFIGURATION_DISABLE_RECEIVE_OWN               (1 << 13)
#define DWE_MAC_CONFIGURATION_RMII_SPEED_100                    (1 << 14)
#define DWE_MAC_CONFIGURATION_RMII_NOT_GIGABIT                  (1 << 15)
#define DWE_MAC_CONFIGURATION_DISABLE_CARRIER_SENSE_DURING_TX   (1 << 16)
#define DWE_MAC_CONFIGURATION_FRAME_GAP_96                      (0x0 << 17)
#define DWE_MAC_CONFIGURATION_FRAME_GAP_88                      (0x1 << 17)
#define DWE_MAC_CONFIGURATION_FRAME_GAP_80                      (0x2 << 17)
#define DWE_MAC_CONFIGURATION_FRAME_GAP_40                      (0x7 << 17)
#define DWE_MAC_CONFIGURATION_JUMBO_FRAME_ENABLE                (1 << 20)
#define DWE_MAC_CONFIGURATION_BURST_ENABLE                      (1 << 21)
#define DWE_MAC_CONFIGURATION_JABBER_DISABLE                    (1 << 22)
#define DWE_MAC_CONFIGURATION_WATCHDOG_DISABLE                  (1 << 23)
#define DWE_MAC_CONFIGURATION_2K_FRAMES                         (1 << 27)
#define DWE_MAC_CONFIGURATION_SOURCE_ADDRESS_REPLACE            (0x3 << 28)

//
// Define MAC frame filter register bit definitions.
//

#define DWE_MAC_FRAME_FILTER_PROMISCUOUS                    (1 << 0)
#define DWE_MAC_FRAME_FILTER_HASH_UNICAST                   (1 << 1)
#define DWE_MAC_FRAME_FILTER_HASH_MULTICAST                 (1 << 2)
#define DWE_MAC_FRAME_FILTER_DESTINATION_INVERSE_FILTERING  (1 << 3)
#define DWE_MAC_FRAME_FILTER_PASS_ALL_MULTICAST             (1 << 4)
#define DWE_MAC_FRAME_FILTER_DISABLE_BROADCAST_FRAMES       (1 << 5)
#define DWE_MAC_FRAME_FILTER_NO_CONTROL                     (0x0 << 6)
#define DWE_MAC_FRAME_FILTER_ALL_CONTROL_NOT_PAUSE          (0x1 << 6)
#define DWE_MAC_FRAME_FILTER_ALL_CONTROL                    (0x2 << 6)
#define DWE_MAC_FRAME_FILTER_PASS_CONTROL                   (0x3 << 6)
#define DWE_MAC_FRAME_FILTER_SOURCE_INVERSE                 (1 << 8)
#define DWE_MAC_FRAME_FILTER_SOURCE_ENABLE                  (1 << 9)
#define DWE_MAC_FRAME_FILTER_HASH_OR_PERFECT                (1 << 10)
#define DWE_MAC_FRAME_FILTER_VLAN                           (1 << 16)
#define DWE_MAC_FRAME_FILTER_PASS_ALL                       (1 << 31)

//
// Define GMII address register bit definitions.
//

#define DWE_GMII_ADDRESS_DEVICE_MASK                0x1F
#define DWE_GMII_ADDRESS_DEVICE_SHIFT               11
#define DWE_GMII_ADDRESS_REGISTER_MASK              0x1F
#define DWE_GMII_ADDRESS_REGISTER_SHIFT             6
#define DWE_GMII_ADDRESS_CLOCK_RANGE_MASK           0xF
#define DWE_GMII_ADDRESS_CLOCK_RANGE_SHIFT          2
#define DWE_GMII_ADDRESS_WRITE                      (1 << 1)
#define DWE_GMII_ADDRESS_BUSY                       (1 << 0)

//
// Define bus mode register bit definitions.
//

#define DWE_BUS_MODE_SOFTWARE_RESET                 (1 << 0)
#define DWE_BUS_MODE_DMA_ARBITRATION_FIXED          (1 << 1)
#define DWE_BUS_MODE_DESCRIPTOR_SKIP_LENGTH_MASK    0x0000001F
#define DWE_BUS_MODE_DESCRIPTOR_SKIP_LENGTH_SHIFT   2
#define DWE_BUS_MODE_LARGE_DESCRIPTORS              (1 << 7)
#define DWE_BUS_MODE_TX_BURST_LENGTH_MASK           0x0000001F
#define DWE_BUS_MODE_TX_BURST_LENGTH_SHIFT          8
#define DWE_BUS_MODE_PRIORITY_RATIO_MASK            0x00000003
#define DWE_BUS_MODE_PRIORITY_RATIO_SHIFT           14
#define DWE_BUS_MODE_FIXED_BURST                    (1 << 16)
#define DWE_BUS_MODE_RX_BURST_LENGTH_MASK           0x0000001F
#define DWE_BUS_MODE_RX_BURST_LENGTH_SHIFT          17
#define DWE_BUS_MODE_USE_SEPARATE_BURST_LENGTHS     (1 << 23)
#define DWE_BUS_MODE_8X_BURST_LENGTHS               (1 << 24)
#define DWE_BUS_MODE_ADDRESS_ALIGNED_BEATS          (1 << 25)
#define DWE_BUS_MODE_MIXED_BURST                    (1 << 26)
#define DWE_BUS_MODE_TRANSMIT_PRIORITY              (1 << 27)
#define DWE_BUS_MODE_CHANNEL_PRIORITY_WEIGHT_MASK   0x00000003
#define DWE_BUS_MODE_CHANNEL_PRIORITY_WEIGHT_SHIFT  28
#define DWE_BUS_MODE_REBUILD_REBUILD_INCR_BURST     (1 << 31)

//
// Define default values used for the bus mode register.
//

#define DWE_BUS_MODE_TX_BURST_LENGTH                8

//
// Define operation mode register bit definitions.
//

#define DWE_OPERATION_MODE_START_RECEIVE                        (1 << 1)
#define DWE_OPERATION_MODE_OPERATE_ON_SECOND_FRAME              (1 << 2)
#define DWE_OPERATION_MODE_RX_THRESHOLD_64                      (0x0 << 3)
#define DWE_OPERATION_MODE_RX_THRESHOLD_32                      (0x1 << 3)
#define DWE_OPERATION_MODE_RX_THRESHOLD_96                      (0x2 << 3)
#define DWE_OPERATION_MODE_RX_THRESHOLD_128                     (0x3 << 3)
#define DWE_OPERATION_MODE_FORWARD_UNDERSIZED_GOOD_FRAMES       (1 << 6)
#define DWE_OPERATION_MODE_FORWARD_ERROR_FRAMES                 (1 << 7)
#define DWE_OPERATION_MODE_ENABLE_HW_FLOW_CONTROL               (1 << 8)
#define DWE_OPERATION_MODE_ACTIVATE_FLOW_CONTROL_SHIFT          9
#define DWE_OPERATION_MODE_DEACTIVATE_FLOW_CONTROL_SHIFT        11
#define DWE_OPERATION_MODE_FLOW_FULL_MINUS_1KB                  0
#define DWE_OPERATION_MODE_FLOW_FULL_MINUS_2KB                  1
#define DWE_OPERATION_MODE_FLOW_FULL_MINUS_3KB                  2
#define DWE_OPERATION_MODE_FLOW_FULL_MINUS_4KB                  3
#define DWE_OPERATION_MODE_START_TRANSMIT                       (1 << 13)
#define DWE_OPERATION_MODE_TX_THRESHOLD_64                      (0x0 << 14)
#define DWE_OPERATION_MODE_TX_THRESHOLD_128                     (0x1 << 14)
#define DWE_OPERATION_MODE_TX_THRESHOLD_192                     (0x2 << 14)
#define DWE_OPERATION_MODE_TX_THRESHOLD_256                     (0x3 << 14)
#define DWE_OPERATION_MODE_TX_THRESHOLD_40                      (0x4 << 14)
#define DWE_OPERATION_MODE_TX_THRESHOLD_32                      (0x5 << 14)
#define DWE_OPERATION_MODE_TX_THRESHOLD_24                      (0x6 << 14)
#define DWE_OPERATION_MODE_TX_THRESHOLD_16                      (0x7 << 14)
#define DWE_OPERATION_MODE_FLUSH_TX_FIFO                        (1 << 20)
#define DWE_OPERATION_MODE_TX_STORE_AND_FORWARD                 (1 << 21)
#define DWE_OPERATION_MODE_DEACTIVATE_FLOW_CONTROL_HIGH         (1 << 22)
#define DWE_OPERATION_MODE_ACTIVATE_FLOW_CONTROL_HIGH           (1 << 23)
#define DWE_OPERATION_MODE_DISABLE_FLUSHING_RECEIVED_FRAMES     (1 << 24)
#define DWE_OPERATION_MODE_RX_STORE_AND_FORWARD                 (1 << 25)
#define DWE_OPERATION_MODE_DISABLE_DROPPING_CHECKSUM_FAILURES   (1 << 26)

//
// Define interrupt enable register bit definitions.
//

#define DWE_INTERRUPT_ENABLE_TX                     (1 << 0)
#define DWE_INTERRUPT_ENABLE_TX_STOPPED             (1 << 1)
#define DWE_INTERRUPT_ENABLE_TX_BUFFER_UNAVAILABLE  (1 << 2)
#define DWE_INTERRUPT_ENABLE_TX_JABBER_TIMEOUT      (1 << 3)
#define DWE_INTERRUPT_ENABLE_OVERFLOW               (1 << 4)
#define DWE_INTERRUPT_ENABLE_UNDERFLOW              (1 << 5)
#define DWE_INTERRUPT_ENABLE_RX                     (1 << 6)
#define DWE_INTERRUPT_ENABLE_RX_BUFFER_UNAVAILABLE  (1 << 7)
#define DWE_INTERRUPT_ENABLE_RX_STOPPED             (1 << 8)
#define DWE_INTERRUPT_ENABLE_RX_WATCHDOG_TIMEOUT    (1 << 9)
#define DWE_INTERRUPT_ENABLE_EARLY_TX               (1 << 10)
#define DWE_INTERRUPT_ENABLE_FATAL_BUS_ERROR        (1 << 13)
#define DWE_INTERRUPT_ENABLE_EARLY_RX               (1 << 14)
#define DWE_INTERRUPT_ENABLE_ABNORMAL_SUMMARY       (1 << 15)
#define DWE_INTERRUPT_ENABLE_NORMAL_SUMMARY         (1 << 16)

#define DWE_INTERRUPT_ENABLE_DEFAULT                        \
    (DWE_INTERRUPT_ENABLE_TX | DWE_INTERRUPT_ENABLE_RX |    \
     DWE_INTERRUPT_ENABLE_ABNORMAL_SUMMARY |                \
     DWE_INTERRUPT_ENABLE_NORMAL_SUMMARY |                  \
     DWE_INTERRUPT_ENABLE_FATAL_BUS_ERROR |                 \
     DWE_INTERRUPT_ENABLE_UNDERFLOW)

//
// Define DMA status register bit definitions.
//

#define DWE_STATUS_TRANSMIT_INTERRUPT           (1 << 0)
#define DWE_STATUS_TRANSMIT_STOPPED             (1 << 1)
#define DWE_STATUS_TRANSMIT_BUFFER_UNAVAILABLE  (1 << 2)
#define DWE_STATUS_TRANSMIT_JABBER_TIMEOUT      (1 << 3)
#define DWE_STATUS_RECEIVE_OVERFLOW             (1 << 4)
#define DWE_STATUS_TRANSMIT_UNDERFLOW           (1 << 5)
#define DWE_STATUS_RECEIVE_INTERRUPT            (1 << 6)
#define DWE_STATUS_RECEIVE_BUFFER_UNAVAILABLE   (1 << 7)
#define DWE_STATUS_RECEIVE_STOPPED              (1 << 8)
#define DWE_STATUS_RECEIVE_WATCHDOG_TIMEOUT     (1 << 9)
#define DWE_STATUS_EARLY_TRANSMIT_INTERRUPT     (1 << 10)
#define DWE_STATUS_FATAL_BUS_ERROR_INTERRUPT    (1 << 13)
#define DWE_STATUS_EARLY_RECEIVE_INTERRUPT      (1 << 14)
#define DWE_STATUS_ABNORMAL_INTERRUPT_SUMMARY   (1 << 15)
#define DWE_STATUS_NORMAL_INTERRUPT_SUMMARY     (1 << 16)
#define DWE_STATUS_RECEIVE_STATE_MASK           0x0000007
#define DWE_STATUS_RECEIVE_STATE_SHIFT          17
#define DWE_STATUS_TRANSMIT_STATE_MASK          0x0000007
#define DWE_STATUS_TRANSMIT_STATE_SHIFT         20
#define DWE_STATUS_ERROR_BITS_MASK              0x0000007
#define DWE_STATUS_ERROR_BITS_SHIFT             23
#define DWE_STATUS_MAC_MMC_INTERRUPT            (1 << 27)
#define DWE_STATUS_TIMESTAMP_TRIGGER_INTERRUPT  (1 << 28)

#define DWE_STATUS_ERROR_MASK \
    (DWE_STATUS_TRANSMIT_JABBER_TIMEOUT | DWE_STATUS_RECEIVE_OVERFLOW | \
     DWE_STATUS_TRANSMIT_UNDERFLOW | DWE_STATUS_RECEIVE_WATCHDOG_TIMEOUT | \
     DWE_STATUS_FATAL_BUS_ERROR_INTERRUPT | \
     DWE_STATUS_ABNORMAL_INTERRUPT_SUMMARY)

//
// Define receive interrupt mask register bit definitions.
//

#define DWE_RECEIVE_INTERRUPT_MASK 0x03FFFFFF

//
// Define transmit interrupt mask register bit definitions.
//

#define DWE_TRANSMIT_INTERRUPT_MASK 0x03FFFFFF

//
// Define receive checksum offload interrupt register bit definitions.
//

#define DWE_RECEIVE_CHECKSUM_INTERRUPT_MASK 0x3FFF3FFF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DWE_REGISTER {
    DweRegisterMacConfiguration = 0x0000,
    DweRegisterMacFrameFilter = 0x0004,
    DweRegisterHashTableHigh = 0x0008,
    DweRegisterHashTableLow = 0x000C,
    DweRegisterGmiiAddress = 0x0010,
    DweRegisterGmiiData = 0x0014,
    DweRegisterFlowControl = 0x0018,
    DweRegisterVlanTag = 0x001C,
    DweRegisterVersion = 0x0020,
    DweRegisterDebug = 0x0024,
    DweRegisterInterrupt = 0x0038,
    DweRegisterInterruptMask = 0x003C,
    DweRegisterMacAddress0High = 0x0040,
    DweRegisterMacAddress0Low = 0x0044,
    DweRegisterMmcControl = 0x0100,
    DweRegisterMmcReceiveInterrupt = 0x0104,
    DweRegisterMmcTransmitInterrupt = 0x0108,
    DweRegisterMmcReceiveInterruptMask = 0x010C,
    DweRegisterMmcTransmitInterruptMask = 0x0110,
    DweRegisterReceiveChecksumOffloadInterruptMask = 0x0200,
    DweRegisterReceiveChecksumOffloadInterrupt = 0x0208,
    DweRegisterVlanTagInclusionReplacement = 0x584,
    DweRegisterVlanHashTable = 0x588,
    DweRegisterTimestampControl = 0x0700,
    DweRegisterSubSecondIncrement = 0x0704,
    DweRegisterSystemTimeSeconds = 0x0708,
    DweRegisterSystemTimeNanoseconds = 0x070C,
    DweRegisterSystemTimeSecondsUpdate = 0x0710,
    DweRegisterSystemTimeNanosecondsUpdate = 0x714,
    DweRegisterTimestampAddend = 0x0718,
    DweRegisterTargetTimeSeconds = 0x071C,
    DweRegisterTargetTimeNanoseconds = 0x0720,
    DweRegisterSystemTimeHigherWordSeconds = 0x0724,
    DweRegisterTimestampStatus = 0x0728,
    DweRegisterBusMode = 0x1000,
    DweRegisterTransmitPollDemand = 0x1004,
    DweRegisterReceivePollDemand = 0x1008,
    DweRegisterReceiveDescriptorListAddress = 0x100C,
    DweRegisterTransmitDescriptorListAddress = 0x1010,
    DweRegisterStatus = 0x1014,
    DweRegisterOperationMode = 0x1018,
    DweRegisterInterruptEnable = 0x101C,
    DweRegisterMissedFrameAndBufferOverflowCount = 0x1020,
    DweRegisterReceiveInterruptWatchdogTimer = 0x1024,
    DweRegisterAhbStatus = 0x102C,
    DweRegisterCurrentHostTransmitDescriptor = 0x1048,
    DweRegisterCurrentHostReceiveDescriptor = 0x104C,
    DweRegisterCurrentHostTransmitBufferAddress = 0x1050,
    DweRegisterCurrentHostReceiveBufferAddress = 0x1054,
    DweRegisterHardwareFeature = 0x1058
} DWE_REGISTER, *PDWE_REGISTER;

/*++

Structure Description:

    This structure defines the DesignWare Ethernet controller transmit and
    receive descriptor format, as defined by the hardware.

Members:

    Control - Stores control and/or status bits.

    BufferSize - Stores the sizes of one (or both) buffers the descriptor is
        describing.

    Address1 - Stores the physical address of the first buffer.

    Address2OrNextDescriptor - Stores either the physical address of the second
        buffer in "ring mode" or the physical address of the next transmit
        descriptor in "chain mode".

    ExtendedStatus - Stores extended status bits for receive descriptors. For
        transmit descriptors, this field is reserved.

    Reserved - Stores reserved regions.

    Timestamp - Stores the hardware timestamp when the packet was sent or
        received if timestamping is enabled.

--*/

typedef struct _DWE_DESCRIPTOR {
    ULONG Control;
    ULONG BufferSize;
    ULONG Address1;
    ULONG Address2OrNextDescriptor;
    ULONG ExtendedStatus;
    ULONG Reserved;
    ULONGLONG Timestamp;
} PACKED DWE_DESCRIPTOR, *PDWE_DESCRIPTOR;

/*++

Structure Description:

    This structure defines an DesignWare Ethernet controller device.

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
        controller's registers.

    NetworkLink - Stores a pointer to the core networking link.

    ReceiveDataIoBuffer - Stores a pointer to the I/O buffer associated with
        the receive frames.

    ReceiveData - Stores the pointer to the array of receive frames.

    ReceiveBegin - Stores the index of the beginning of the list, which is
        the oldest received frame and the first one to dispatch.

    ReceiveLock - Stores a pointer to a queued lock that protects the
        received list.

    ConfigurationLock - Stores a pointer to a queued lock that protects the
        enabled capabilities field and synchronizes configuration register
        access between capability updates and checking the link state.

    CommandPhysicalAddress - Stores the physical address of the base of the
        command list (called a list but is really an array).

    DescriptorIoBuffer - Stores a pointer to the I/O buffer associated with
        the command block list.

    TransmitDescriptors - Stores the virtual address of the array of transmit
        descriptors.

    ReceiveDescriptors - Stores the virtual address of the array of receive
        descriptors.

    TransmitPacket - Stores a pointer to the array of net packet buffers that
        go with each command.

    TransmitPacketList - Stores a list of network packets waiting to be sent.

    TransmitBegin - Stores the index of the least recent command, the first
        one to reap.

    TransmitEnd - Stores the index where the next command should be placed.

    TransmitLock - Stores the lock protecting software access to the transmit
        descriptors.

    CommandEntryFreeEvent - Stores the event to wait on in the event that no
        command entries are free.

    LinkActive - Stores a boolean indicating if there is an active network link.

    LinkSpeed - Stores the current link speed, if active.

    FullDuplex - Stores the duplex status of the link, TRUE for full duplex and
        FALSE for half duplex.

    LinkCheckTimer - Stores a pointer to the timer that fires periodically to
        see if the link is active.

    LinkCheckDpc - Stores a pointer to the DPC associated with the link check
        timer.

    NextLinkCheck - Stores the time counter value when the next link check
        should be performed.

    LinkCheckInterval - Stores the interval in time counter ticks that the
        link state should be polled.

    WorkItem - Stores a pointer to the work item queued from the DPC.

    PendingStatusBits - Stores the bitfield of status bits that have yet to be
        dealt with by software.

    MacAddressAssigned - Stores a boolean indicating if the MAC address matter
        has been settled.

    MacAddress - Stores the default MAC address of the device.

    PhyId - Stores the address of the PHY.

    DroppedTxPackets - Stores the number of packets that were dropped from
        being transmitted because there were no descriptors available. This is
        a sign that more descriptors should be allocated.

    SupportedCapabilities - Stores the set of capabilities that this device
        supports. See NET_LINK_CAPABILITY_* for definitions.

    EnabledCapabilities - Stores the currently enabled capabilities on the
        devices. See NET_LINK_CAPABILITY_* for definitions.

--*/

typedef struct _DWE_DEVICE {
    PDEVICE OsDevice;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    PVOID ControllerBase;
    PNET_LINK NetworkLink;
    PIO_BUFFER ReceiveDataIoBuffer;
    PVOID ReceiveData;
    ULONG ReceiveBegin;
    PQUEUED_LOCK ReceiveLock;
    PQUEUED_LOCK ConfigurationLock;
    PIO_BUFFER DescriptorIoBuffer;
    PDWE_DESCRIPTOR TransmitDescriptors;
    PDWE_DESCRIPTOR ReceiveDescriptors;
    PNET_PACKET_BUFFER *TransmitPacket;
    NET_PACKET_LIST TransmitPacketList;
    ULONG TransmitBegin;
    ULONG TransmitEnd;
    PQUEUED_LOCK TransmitLock;
    BOOL LinkActive;
    ULONGLONG LinkSpeed;
    BOOL FullDuplex;
    PKTIMER LinkCheckTimer;
    PDPC LinkCheckDpc;
    ULONGLONG NextLinkCheck;
    ULONGLONG LinkCheckInterval;
    PWORK_ITEM WorkItem;
    volatile ULONG PendingStatusBits;
    BOOL MacAddressAssigned;
    BYTE MacAddress[ETHERNET_ADDRESS_SIZE];
    ULONG PhyId;
    UINTN DroppedTxPackets;
    ULONG SupportedCapabilities;
    ULONG EnabledCapabilities;
} DWE_DEVICE, *PDWE_DEVICE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
DweSend (
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
DweGetSetInformation (
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
DwepInitializeDeviceStructures (
    PDWE_DEVICE Device
    );

/*++

Routine Description:

    This routine creates the data structures needed for a DesignWare Ethernet
    controller.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

KSTATUS
DwepResetDevice (
    PDWE_DEVICE Device
    );

/*++

Routine Description:

    This routine resets the DesignWare Ethernet device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Status code.

--*/

INTERRUPT_STATUS
DwepInterruptService (
    PVOID Context
    );

/*++

Routine Description:

    This routine implements the DesignWare Ethernet interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the e100 device
        structure.

Return Value:

    Interrupt status.

--*/

INTERRUPT_STATUS
DwepInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine processes interrupts for the DesignWare Ethernet controller at
    low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt status.

--*/

KSTATUS
DwepAddNetworkDevice (
    PDWE_DEVICE Device
    );

/*++

Routine Description:

    This routine adds the device to core networking's available links.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/
