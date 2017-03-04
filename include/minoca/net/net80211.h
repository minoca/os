/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    net80211.h

Abstract:

    This header contains definitions for the IEEE 802.11 Network Layer.

Author:

    Chris Stevens 8-Oct-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macros determines if the 802.11 packet is a multicast packet based on
// header.
//

#define NET80211_IS_MULTICAST_BROADCAST(_Header) \
    (((_Header)->Address1[0] & 0x01) != 0)

//
// This macros returns the sequences number for the given 802.11 header.
//

#define NET80211_GET_SEQUENCE_NUMBER(_Header)            \
    (((_Header)->SequenceControl &                       \
      NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_MASK) >> \
     NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_SHIFT)

//
// This macro returns the 802.11 packet's type.
//

#define NET80211_GET_FRAME_TYPE(_Header)                             \
    (((_Header)->FrameControl & NET80211_FRAME_CONTROL_TYPE_MASK) >> \
     NET80211_FRAME_CONTROL_TYPE_SHIFT)

//
// This macro returns the 802.11 packet's subtype.
//

#define NET80211_GET_FRAME_SUBTYPE(_Header)                             \
    (((_Header)->FrameControl & NET80211_FRAME_CONTROL_SUBTYPE_MASK) >> \
     NET80211_FRAME_CONTROL_SUBTYPE_SHIFT)

//
// This macro gets the packet number from a CCMP header.
//

#define NET80211_GET_CCMP_HEADER_PACKET_NUMBER(_Header, _PacketNumber) \
    (_PacketNumber) = (_Header)->PacketNumberLow;                      \
    (_PacketNumber) |= (_Header)->PacketNumberHigh << 16;              \

//
// This macro sets the packet number for a CCMP header.
//

#define NET80211_SET_CCMP_HEADER_PACKET_NUMBER(_Header, _PacketNumber)      \
    (_Header)->PacketNumberLow = (_PacketNumber) & 0xFFFF;                  \
    (_Header)->PacketNumberHigh = ((_PacketNumber) & 0xFFFFFFFF0000) >> 16; \

//
// This macro gets the ID from the given information element.
//

#define NET80211_GET_ELEMENT_ID(_Element) \
    ((PUCHAR)(_Element))[NET80211_ELEMENT_ID_OFFSET]

//
// This macro gets the length from the given information element.
//

#define NET80211_GET_ELEMENT_LENGTH(_Element) \
    ((PUCHAR)(_Element))[NET80211_ELEMENT_LENGTH_OFFSET]

//
// This macro returns a pointer to the first byte of the element data array.
//

#define NET80211_GET_ELEMENT_DATA(_Element) \
    ((PUCHAR)(_Element) + NET80211_ELEMENT_DATA_OFFSET)

//
// ---------------------------------------------------------------- Definitions
//

#ifndef NET80211_API

#define NET80211_API __DLLIMPORT

#endif

//
// Define the current version number of the 802.11 net link properties
// structure.
//

#define NET80211_LINK_PROPERTIES_VERSION 1

//
// Define the current version number of the 802.11 BSS information structure.
//

#define NET80211_BSS_VERSION 1

//
// Define the current version number of the 802.11 received packet structure.
//

#define NET80211_RECEIVE_PACKET_VERSION 1

//
// Define the size of an 802.11 MAC address.
//

#define NET80211_ADDRESS_SIZE 6

//
// Define the frame control bits for an 802.11 frame header.
//

#define NET80211_FRAME_CONTROL_ORDER                  0x8000
#define NET80211_FRAME_CONTROL_PROTECTED_FRAME        0x4000
#define NET80211_FRAME_CONTROL_MORE_DATA              0x2000
#define NET80211_FRAME_CONTROL_POWER_MANAGEMENT       0x1000
#define NET80211_FRAME_CONTROL_RETRY                  0x0800
#define NET80211_FRAME_CONTROL_MORE_FRAGMENTS         0x0400
#define NET80211_FRAME_CONTROL_FROM_DS                0x0200
#define NET80211_FRAME_CONTROL_TO_DS                  0x0100
#define NET80211_FRAME_CONTROL_SUBTYPE_MASK           0x00F0
#define NET80211_FRAME_CONTROL_SUBTYPE_SHIFT          4
#define NET80211_FRAME_CONTROL_TYPE_MASK              0x000C
#define NET80211_FRAME_CONTROL_TYPE_SHIFT             2
#define NET80211_FRAME_CONTROL_PROTOCOL_VERSION_MASK  0x0003
#define NET80211_FRAME_CONTROL_PROTOCOL_VERSION_SHIFT 0
#define NET80211_FRAME_CONTROL_PROTOCOL_VERSION       0

//
// Define the 802.11 frame types.
//

#define NET80211_FRAME_TYPE_MANAGEMENT 0
#define NET80211_FRAME_TYPE_CONTROL    1
#define NET80211_FRAME_TYPE_DATA       2

//
// Define the management frame subtypes for the 802.11 header.
//

#define NET80211_MANAGEMENT_FRAME_SUBTYPE_ASSOCIATION_REQUEST    0x0
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_ASSOCIATION_RESPONSE   0x1
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_REASSOCIATION_REQUEST  0x2
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_REASSOCIATION_RESPONSE 0x3
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_REQUEST          0x4
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_PROBE_RESPONSE         0x5
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_TIMING_ADVERTISEMENT   0x6
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_BEACON                 0x8
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_ATIM                   0x9
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_DISASSOCIATION         0xA
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_AUTHENTICATION         0xB
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_DEAUTHENTICATION       0xC
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_ACTION                 0xD
#define NET80211_MANAGEMENT_FRAME_SUBTYPE_ACTION_NO_ACK          0xE

//
// Define the control frame subtypes for the 802.11 header.
//

#define NET80211_CONTROL_FRAME_SUBTYPE_CONTROL_WRAPPER   0x7
#define NET80211_CONTROL_FRAME_SUBTYPE_BLOCK_ACK_REQUEST 0x8
#define NET80211_CONTROL_FRAME_SUBTYPE_BLOCK_ACK         0x9
#define NET80211_CONTROL_FRAME_SUBTYPE_PS_POLL           0xA
#define NET80211_CONTROL_FRAME_SUBTYPE_RTS               0xB
#define NET80211_CONTROL_FRAME_SUBTYPE_CTS               0xC
#define NET80211_CONTROL_FRAME_SUBTYPE_ACK               0xD
#define NET80211_CONTROL_FRAME_SUBTYPE_CF_END            0xE
#define NET80211_CONTROL_FRAME_SUBTYPE_CF_END_ACK        0xF

//
// Define the data frame subtypes for the 802.11 header.
//

#define NET80211_DATA_FRAME_SUBTYPE_DATA                    0x0
#define NET80211_DATA_FRAME_SUBTYPE_DATA_CF_ACK             0x1
#define NET80211_DATA_FRAME_SUBTYPE_DATA_CF_POLL            0x2
#define NET80211_DATA_FRAME_SUBTYPE_DATA_CF_ACK_POLL        0x3
#define NET80211_DATA_FRAME_SUBTYPE_NO_DATA                 0x4
#define NET80211_DATA_FRAME_SUBTYPE_NO_DATA_CF_ACK          0x5
#define NET80211_DATA_FRAME_SUBTYPE_NO_DATA_CF_POLL         0x6
#define NET80211_DATA_FRAME_SUBTYPE_NO_DATA_CF_ACK_POLL     0x7
#define NET80211_DATA_FRAME_SUBTYPE_QOS_DATA                0x8
#define NET80211_DATA_FRAME_SUBTYPE_QOS_DATA_CF_ACK         0x9
#define NET80211_DATA_FRAME_SUBTYPE_QOS_DATA_CF_POLL        0xA
#define NET80211_DATA_FRAME_SUBTYPE_QOS_DATA_CF_ACK_POLL    0xB
#define NET80211_DATA_FRAME_SUBTYPE_QOS_NO_DATA             0xC
#define NET80211_DATA_FRAME_SUBTYPE_QOS_NO_DATA_CF_POLL     0xE
#define NET80211_DATA_FRAME_SUBTYPE_QOS_NO_DATA_CF_ACK_POLL 0xF

//
// Define the sequence control bits for an 802.11 frame header.
//

#define NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_MASK  0xFFF0
#define NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_SHIFT 4
#define NET80211_SEQUENCE_CONTROL_FRAGMENT_NUMBER_MASK  0x000F
#define NET80211_SEQUENCE_CONTROL_FRAGMENT_NUMBER_SHIFT 0

//
// Define the quality of service control bits for an 802.11 frame header.
//

#define NET80211_QOS_CONTROL_QUEUE_SIZE_MASK               0xFF00
#define NET80211_QOS_CONTROL_QUEUE_SIZE_SHIFT              8
#define NET80211_QOS_CONTROL_TXOP_DURATION_REQUESTED_MASK  0xFF00
#define NET80211_QOS_CONTROL_TXOP_DURATION_REQUESTED_SHIFT 8
#define NET80211_QOS_CONTROL_AP_PS_BUFFER_STATE_MASK       0xFF00
#define NET80211_QOS_CONTROL_AP_PS_BUFFER_STATE_SHIFT      8
#define NET80211_QOS_CONTROL_TXOP_LIMIT_MASK               0xFF00
#define NET80211_QOS_CONTROL_TXOP_LIMIT_SHIFT              8
#define NET80211_QOS_CONTROL_RSPI                          0x0400
#define NET80211_QOS_CONTROL_MESH_POWER_SAVE_LEVEL         0x0200
#define NET80211_QOS_CONTROL_MESH_CONTROL_PRESENT          0x0100
#define NET80211_QOS_CONTROL_AMSDU_PRESENT                 0x0080
#define NET80211_QOS_CONTROL_ACK_POLICY_MASK               0x0060
#define NET80211_QOS_CONTROL_ACK_POLICY_SHIFT              5
#define NET80211_QOS_CONTROL_EOSP                          0x0010
#define NET80211_QOS_CONTROL_TID_MASK                      0x000F
#define NET80211_QOS_CONTROL_TID_SHIFT                     0

//
// Define the HT control bits for an 802.11 frame header.
//

#define NET80211_HT_CONTROL_RDG_MORE_PPDU                 0x80000000
#define NET80211_HT_CONTROL_AC_CONSTRAINT                 0x40000000
#define NET80211_HT_CONTROL_NDP_ANNOUCEMENT               0x01000000
#define NET80211_HT_CONTROL_CSI_STEERING_MASK             0x00C00000
#define NET80211_HT_CONTROL_CSI_STEERING_SHIFT            22
#define NET80211_HT_CONTROL_CALIBRATION_SEQUENCE_MASK     0x000C0000
#define NET80211_HT_CONTROL_CALIBRATION_SEQUENCE_SHIFT    18
#define NET80211_HT_CONTROL_CALIBRATION_POSITION_MASK     0x00030000
#define NET80211_HT_CONTROL_CALIBRATION_POSITION_SHIFT    16
#define NET80211_HT_CONTROL_LINK_ADAPTATION_CONTROL_MASK  0x0000FFFF
#define NET80211_HT_CONTROL_LINK_ADAPTATION_CONTROL_SHIFT 0

//
// Define the size, in bytes, for each of the fixed size non-information
// element 802.11 fields.
//

#define NET80211_AUTHENTICATION_ALGORITHM_SIZE 2
#define NET80211_AUTHENTICATION_TRANSACTION_SEQUENCE_SIZE 2
#define NET80211_BEACON_INTERVAL_SIZE 2
#define NET80211_CAPABILITY_SIZE 2
#define NET80211_CURRENT_AP_ADDRESS_SIZE 6
#define NET80211_LISTEN_INTERVAL_SIZE 2
#define NET80211_REASON_CODE_SIZE 2
#define NET80211_ASSOCIATION_ID_SIZE 2
#define NET80211_STATUS_CODE_SIZE 2
#define NET80211_TIMESTAMP_SIZE 8
#define NET80211_DIALOG_TOKEN_SIZE 1
#define NET80211_DLS_TIMEOUT_VALUE_SIZE 2
#define NET80211_BLOCK_ACK_PARAMETER_SET_SIZE 2
#define NET80211_BLOCK_ACK_TIMEOUT_SIZE 2
#define NET80211_DELBA_PARAMETER_SET_SIZE 2
#define NET80211_QOS_INFORMATION_SIZE 1
#define NET80211_MEASUREMENT_PILOT_INTERVAL_SIZE 1
#define NET80211_MAX_TRANSMIT_POWER_SIZE 1
#define NET80211_TRANSMIT_POWER_USED_SIZE 1
#define NET80211_CHANNEL_WIDTH_SIZE 1
#define NET80211_SM_POWER_CONTROL_SIZE 1
#define NET80211_PCO_PHASE_CONTROL_SIZE 1
#define NET80211_PSMP_PARAMETER_SET_SIZE 2
#define NET80211_PSMP_STATION_INFORMATION_SIZE 8
#define NET80211_MIMO_CONTROL_SIZE 6
#define NET80211_ANTENNA_SELECTION_INDICES_SIZE 1
#define NET80211_RATE_IDENTIFICATION_SIZE 4
#define NET80211_GAS_QUERY_RESPONSE_FRAGMENT_ID_SIZE 1
#define NET80211_VENUE_INFORMATION_SIZE 2
#define NET80211_TARGET_CHANNEL_SIZE 1
#define NET80211_OPERATING_CLASS_SIZE 1
#define NET80211_SEND_CONFIRM_SIZE 2
#define NET80211_FINITE_CYCLIC_GROUP_SIZE 2

//
// Define the bits for the 802.11 capability information field.
//

#define NET80211_CAPABILITY_IMMEDIATE_BLOCK_ACK 0x8000
#define NET80211_CAPABILITY_DELAYED_BLOCK_ACK   0x4000
#define NET80211_CAPABILITY_DSSS_OFDM           0x2000
#define NET80211_CAPABILITY_RADIO_MEASUREMENT   0x1000
#define NET80211_CAPABILITY_APSD                0x0800
#define NET80211_CAPABILITY_SHORT_SLOT_TIME     0x0400
#define NET80211_CAPABILITY_QOS                 0x0200
#define NET80211_CAPABILITY_SPECTRUM_MGMT       0x0100
#define NET80211_CAPABILITY_CHANNEL_AGILITY     0x0080
#define NET80211_CAPABILITY_PBCC                0x0040
#define NET80211_CAPABILITY_SHORT_PREAMBLE      0x0020
#define NET80211_CAPABILITY_PRIVACY             0x0010
#define NET80211_CAPABILITY_CF_POLL_REQUEST     0x0008
#define NET80211_CAPABILITY_CF_POLLABLE         0x0004
#define NET80211_CAPABILITY_IBSS                0x0002
#define NET80211_CAPABILITY_ESS                 0x0001

//
// Define the mask for the association ID. The upper 2 bits are always set.
//

#define NET80211_ASSOCIATION_ID_MASK 0x3FFF

//
// Define the 802.11 reason codes.
//

#define NET80211_REASON_CODE_RESERVED 0
#define NET80211_REASON_CODE_UNSPECIFIED 1
#define NET80211_REASON_CODE_AUTHENTICATION_EXPIRED 2
#define NET80211_REASON_CODE_DEAUTHENTICATION_LEAVING 3
#define NET80211_REASON_CODE_DISASSOCIATION_INACTIVE 4
#define NET80211_REASON_CODE_DISASSOCIATION_FULL 5
#define NET80211_REASON_CODE_CLASS_2_NOT_AUTHORIZED 6
#define NET80211_REASON_CODE_CLASS_3_NOT_ASSOCIATED 7
#define NET80211_REASON_CODE_DISASSOCIATION_LEAVING 8
#define NET80211_REASON_CODE_ASSOCIATION_NOT_AUTHORIZED 9
#define NET80211_REASON_CODE_DISASSOCIATION_INVALID_POWER_CAPABILITY 10

//
// Define the set of 802.11 information element IDs.
//

#define NET80211_ELEMENT_SSID                     0x00
#define NET80211_ELEMENT_SUPPORTED_RATES          0x01
#define NET80211_ELEMENT_FH                       0x02
#define NET80211_ELEMENT_DSSS                     0x03
#define NET80211_ELEMENT_EDCA                     0x0C
#define NET80211_ELEMENT_RSN                      0x30
#define NET80211_ELEMENT_EXTENDED_SUPPORTED_RATES 0x32

//
// Define the base size that is common to all elements.
//

#define NET80211_ELEMENT_HEADER_SIZE 2
#define NET80211_ELEMENT_ID_OFFSET 0
#define NET80211_ELEMENT_LENGTH_OFFSET 1
#define NET80211_ELEMENT_DATA_OFFSET 2

//
// Define the sizes for the fixed-size 802.11 information element fields.
//

#define NET80211_DSSS_SIZE 3

//
// Define the bits for 802.11 rates.
//

#define NET80211_RATE_BASIC       0x80
#define NET80211_RATE_VALUE_MASK  0x7F
#define NET80211_RATE_VALUE_SHIFT 0

//
// Define the BSS membership selector values encoded into the suppored rates
// element.
//

#define NET80211_MEMBERSHIP_SELECTOR_HT_PHY 127

//
// Define the 802.11 authentication management frame algorithm numbers.
//

#define NET80211_AUTHENTICATION_ALGORITHM_OPEN 0
#define NET80211_AUTHENTICATION_ALGORITHM_SHARED_KEY 1
#define NET80211_AUTHENTICATION_ALGORITHM_FAST_BSS_TRANSITION 2
#define NET80211_AUTHENTICATION_ALGORITHM_SAE 3

//
// Define the 802.11 authentication transaction sequence numbers.
//

#define NET80211_AUTHENTICATION_REQUEST_SEQUENCE_NUMBER 0x0001
#define NET80211_AUTHENTICATION_RESPONSE_SEQUENCE_NUMBER 0x0002

//
// Define the 802.11 management frame status codes.
//

#define NET80211_STATUS_CODE_SUCCESS 0
#define NET80211_STATUS_CODE_REFUSED 1

//
// Define the maximum SSID supported in the 802.11 SSID element.
//

#define NET80211_MAX_SSID_LENGTH 32

//
// Define the maximum number of rates allowed in the 802.11 supported rates
// element.
//

#define NET80211_MAX_SUPPORTED_RATES 8

//
// Define the maximum number of rates allowed in the 802.11 extended supported
// rates element.
//

#define NET80211_MAX_EXTENDED_SUPPORTED_RATES 255

//
// Define the current version for the RSN element.
//

#define NET80211_RSN_VERSION 1

//
// Define the RSN cipher suite types.
//

#define NET80211_CIPHER_SUITE_USE_GROUP_CIPHER  0x000FAC00
#define NET80211_CIPHER_SUITE_WEP_40            0x000FAC01
#define NET80211_CIPHER_SUITE_TKIP              0x000FAC02
#define NET80211_CIPHER_SUITE_CCMP              0x000FAC04
#define NET80211_CIPHER_SUITE_WEP_104           0x000FAC05
#define NET80211_CIPHER_SUITE_BIP               0x000FAC06
#define NET80211_CIPHER_SUITE_GROUP_NOT_ALLOWED 0x000FAC07

//
// Define the RSN AKM suite types.
//

#define NET80211_AKM_SUITE_8021X         0x000FAC01
#define NET80211_AKM_SUITE_PSK           0x000FAC02
#define NET80211_AKM_SUITE_FT_8021X      0x000FAC03
#define NET80211_AKM_SUITE_FT_PSK        0x000FAC04
#define NET80211_AKM_SUITE_8021X_SHA256  0x000FAC05
#define NET80211_AKM_SUITE_PSK_SHA256    0x000FAC06
#define NET80211_AKM_SUITE_TDLS_TPK      0x000FAC07
#define NET80211_AKM_SUITE_SAE_SHA256    0x000FAC08
#define NET80211_AKM_SUITE_FT_SAE_SHA256 0x000FAC09

//
// Define the bits for the RSN capabilities.
//

#define NET80211_RSN_CAPABILITY_EXTENDED_KEY_ID            0x2000
#define NET80211_RSN_CAPABILITY_PBAC                       0x1000
#define NET80211_RSN_CAPABILITY_SPP_AMSDU_REQUIRED         0x0800
#define NET80211_RSN_CAPABILITY_SPP_AMSDU_CAPABLE          0x0400
#define NET80211_RSN_CAPABILITY_PEERKEY_ENABLED            0x0200
#define NET80211_RSN_CAPABILITY_MFPC                       0x0080
#define NET80211_RSN_CAPABILITY_MFPR                       0x0040
#define NET80211_RSN_CAPABILITY_GTKSA_REPLAY_COUNTER_MASK  0x0030
#define NET80211_RSN_CAPABILITY_GTKSA_REPLAY_COUNTER_SHIFT 4
#define NET80211_RSN_CAPABILITY_PTKSA_REPLAY_COUNTER_MASK  0x000C
#define NET80211_RNS_CAPABILITY_PTKSA_REPLAY_COUNTER_SHIFT 2
#define NET80211_RSN_CAPABILITY_NO_PAIRWISE                0x0002
#define NET80211_RSN_CAPABILITY_PREAUTHENTICATION          0x0001

//
// Define the length, in bytes, of an pairwise master key identifier (PMKID).
//

#define NET80211_RSN_PMKID_LENGTH 16

//
// Define the values for the RSN capability replay counter fields.
//

#define NET80211_RSN_REPLAY_COUNTER_1  0
#define NET80211_RSN_REPLAY_COUNTER_2  1
#define NET80211_RSN_REPLAY_COUNTER_4  2
#define NET80211_RSN_REPLAY_COUNTER_16 3

//
// Define the maximum data frame body size, in bytes.
//

#define NET80211_MAX_DATA_FRAME_BODY_SIZE 2304

//
// The 802.11 rates are defined in 500Kb/s units.
//

#define NET80211_RATE_UNIT 500000ULL

//
// 802.11 times are defined in units of 1024 microseconds.
//

#define NET80211_TIME_UNIT 1024

//
// Define the flags for the CCMP header.
//

#define NET80211_CCMP_FLAG_KEY_ID_MASK  0xC0
#define NET80211_CCMP_FLAG_KEY_ID_SHIFT 6
#define NET80211_CCMP_FLAG_EXT_IV       0x20

//
// Define the maximum number of keys that can be in use by CCMP.
//

#define NET80211_CCMP_MAX_KEY_COUNT 4

//
// Define the size, in bytes, of the MIC appended to the end of the PDU for
// CCMP encryption.
//

#define NET80211_CCMP_MIC_SIZE 8

//
// Define the size, in bytes, of the CCM length field used by CCMP encryption.
//

#define NET80211_CCMP_LENGTH_FIELD_SIZE 2

//
// Define the size of the packet number used in CCMP.
//

#define NET80211_CCMP_PACKET_NUMBER_SIZE 6

//
// Define the set of frame control bits that are carried over from the MPDU
// header to the AAD. The mask is different for QoS frames.
//

#define NET80211_AAD_FRAME_CONTROL_DEFAULT_MASK \
    ~(NET80211_FRAME_CONTROL_SUBTYPE_MASK |     \
      NET80211_FRAME_CONTROL_RETRY |            \
      NET80211_FRAME_CONTROL_POWER_MANAGEMENT | \
      NET80211_FRAME_CONTROL_MORE_DATA)

#define NET80211_AAD_FRAME_CONTROL_QOS_MASK     \
    ~(NET80211_FRAME_CONTROL_SUBTYPE_MASK |     \
      NET80211_FRAME_CONTROL_RETRY |            \
      NET80211_FRAME_CONTROL_POWER_MANAGEMENT | \
      NET80211_FRAME_CONTROL_MORE_DATA |        \
      NET80211_FRAME_CONTROL_ORDER)

//
// Define the portion of the sequence control field that is carried over from
// the MPDU header to the AAD.
//

#define NET80211_AAD_SEQUENCE_CONTROL_MASK \
    ~(NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_MASK)

//
// Define the flags for the CCM nonce.
//

#define NET80211_CCM_NONCE_FLAG_MANAGEMENT     0x10
#define NET80211_CCM_NONCE_FLAG_PRIORITY_MASK  0x0F
#define NET80211_CCM_NONCE_FLAG_PRIORITY_SHIFT 0

//
// Define the maximum number of bytes supported for the CCM authentication
// field.
//

#define NET80211_CCM_MAX_AUTHENTICATION_FIELD_SIZE 16

//
// Define the minimum and maximum allowed CCM length field sizes.
//

#define NET80211_CCM_MAX_LENGTH_FIELD_SIZE 8
#define NET80211_CCM_MIN_LENGTH_FIELD_SIZE 2

//
// Define the bitmask of CCM flags used in the first byte of the first
// encryption block.
//

#define NET80211_CCM_FLAG_AAD                        0x40
#define NET80211_CCM_FLAG_AUTHENTICATION_FIELD_MASK  0x38
#define NET80211_CCM_FLAG_AUTHENTICATION_FIELD_SHIFT 3
#define NET80211_CCM_FLAG_LENGTH_MASK                0x07
#define NET80211_CCM_FLAG_LENGTH_SHIFT               0

//
// Define necessary encoding values for the CCM AAD length.
//

#define NET80211_CCM_AAD_MAX_SHORT_LENGTH 0xFEFF
#define NET80211_CCM_AAD_LONG_ENCODING 0xFEFF

//
// Define the bits that describe the 802.11 networking core key flags.
//

#define NET80211_KEY_FLAG_CCMP     0x00000001
#define NET80211_KEY_FLAG_GLOBAL   0x00000002
#define NET80211_KEY_FLAG_TRANSMIT 0x00000004

//
// Define the data rates that define the different 802.11 modes in bits per
// second.
//

#define NET80211_MODE_B_MAX_RATE 11000000ULL
#define NET80211_MODE_G_MAX_RATE 54000000ULL

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _NET80211_LINK NET80211_LINK, *PNET80211_LINK;

/*++

Structure Description:

    This structure defines the maximum 802.11 header that may come with a
    packet. Depending on the packet type, not all of this data may be present
    in the header.

Members:

    FrameControl - Stores frame control bits for the packet. See
        NET80211_FRAME_CONTROL_* for definitions.

    DurationId - Stores packet duration and ID information.

    Address1 - Stores the destination address of the packet.

    Address2 - Stores the source address of the packet.

    Address3 - Stores a third address whose meaning depends on the packet
        type.

    SequenceControl - Stores the sequence and fragment numbers.

    Address4 - Stores a fourth address whose meaning depends on the packet
        type.

    QosControl - Stores quality of service information for the packet.

    HtControl - Stores high throughput information for the packet.

--*/

typedef struct _NET80211_FRAME_HEADER {
    USHORT FrameControl;
    USHORT DurationId;
    UCHAR Address1[NET80211_ADDRESS_SIZE];
    UCHAR Address2[NET80211_ADDRESS_SIZE];
    UCHAR Address3[NET80211_ADDRESS_SIZE];
    USHORT SequenceControl;
    UCHAR Address4[NET80211_ADDRESS_SIZE];
    USHORT QosControl;
    ULONG HtControl;
} PACKED NET80211_FRAME_HEADER, *PNET80211_FRAME_HEADER;

/*++

Structure Description:

    This structure defines the header for the 802.11 data frames that a station
    needs to handle. In these cases, only one of the "To DS" and "From DS" bits
    are set. That is, it does not account for AP to AP communication or station
    to station communication in an ad hoc network.

Members:

    FrameControl - Stores frame control bits for the packet. See
        NET80211_FRAME_CONTROL_* for definitions.

    DurationId - Stores packet duration and ID information.

    ReceiverAddress - Stores the physical address of the receiving node, which
        is either the station or the access point.

    TransmitterAddress - Stores the physical address of the transmitting node,
        which is either the station or the access point.

    SourceDestinationAddress - Stores the physical address of either the source
        node (if coming from the DS) or the destination node (if being sent to
        the DS).

    SequenceControl - Stores the sequence and fragment numbers.

--*/

typedef struct _NET80211_DATA_FRAME_HEADER {
    USHORT FrameControl;
    USHORT DurationId;
    UCHAR ReceiverAddress[NET80211_ADDRESS_SIZE];
    UCHAR TransmitterAddress[NET80211_ADDRESS_SIZE];
    UCHAR SourceDestinationAddress[NET80211_ADDRESS_SIZE];
    USHORT SequenceControl;
} PACKED NET80211_DATA_FRAME_HEADER, *PNET80211_DATA_FRAME_HEADER;

/*++

Structure Description:

    This structure defines the header for 802.11 management frames.

Members:

    FrameControl - Stores frame control bits for the packet. See
        NET80211_FRAME_CONTROL_* for definitions.

    Duration - Stores packet duration information.

    DestinationAddress - Stores the destination address of the packet.

    SourceAddressAddress - Stores the source address of the packet.

    Bssid - Stores a third address whose meaning depends on the packet type.

    SequenceControl - Stores the sequence and fragment numbers.

--*/

typedef struct _NET80211_MANAGEMENT_FRAME_HEADER {
    USHORT FrameControl;
    USHORT Duration;
    UCHAR DestinationAddress[NET80211_ADDRESS_SIZE];
    UCHAR SourceAddress[NET80211_ADDRESS_SIZE];
    UCHAR Bssid[NET80211_ADDRESS_SIZE];
    USHORT SequenceControl;
} PACKED NET80211_MANAGEMENT_FRAME_HEADER, *PNET80211_MANAGEMENT_FRAME_HEADER;

/*++

Structure Description:

    This structure defines the 802.11 CTR with CBC-MAC Protocol (CCMP) header
    used on encrypted data packets.

Members:

    PacketNumberLow - Stores the lowest two bytes of the 48-bit packet number.

    Reserved - Stores a reserved field.

    Flags - Stores a bitmask of CCMP flags. See NET80211_CCMP_FLAG_* for
        definitions;

    PacketNumberHigh - Stores the highest four bytes of the 48-bit packet
        number.

--*/

typedef struct _NET80211_CCMP_HEADER {
    USHORT PacketNumberLow;
    UCHAR Reserved;
    UCHAR Flags;
    ULONG PacketNumberHigh;
} PACKED NET80211_CCMP_HEADER, *PNET80211_CCMP_HEADER;

/*++

Structure Description:

    This structure defines the base additional authentication data (AAD) used
    for CCMP encryption.

Members:

    FrameControl - Stores a masked version of the frame control field from the
        MPDU frame control field. See NET80211_AAD_FRAME_CONTROL_MASK.

    Address1 - Stores the first address from the MPDU header.

    Address2 - Stores the second address from the MPDU header.

    Address3 - Stores the third address from the MPDU header.

    SequenceControl - Stores a masked version of the sequence control field
        from the MPDU frame. See NET80211_AAD_SEQUENCE_CONTROL_MASK.

--*/

typedef struct _NET80211_AAD {
    USHORT FrameControl;
    UCHAR Address1[NET80211_ADDRESS_SIZE];
    UCHAR Address2[NET80211_ADDRESS_SIZE];
    UCHAR Address3[NET80211_ADDRESS_SIZE];
    USHORT SequenceControl;
} PACKED NET80211_AAD, *PNET80211_AAD;

/*++

Structure Description:

    This structure defines the CCM nonce value used during CCMP encryption.

Members:

    Flags - Stores a bitmask of flags. See NET80211_CCM_NONCE_FLAG_* for
        definitions.

    Address2 - Stores the second address from the MPDU header.

    PacketNumber - Stores the packet number from the CCMP header.

--*/

typedef struct _NET80211_CCM_NONCE {
    UCHAR Flags;
    UCHAR Address2[NET80211_ADDRESS_SIZE];
    UCHAR PacketNumber[NET80211_CCMP_PACKET_NUMBER_SIZE];
} PACKED NET80211_CCM_NONCE, *PNET80211_CCM_NONCE;

typedef enum _NET80211_STATE {
    Net80211StateInvalid,
    Net80211StateUninitialized,
    Net80211StateInitialized,
    Net80211StateProbing,
    Net80211StateAuthenticating,
    Net80211StateAssociating,
    Net80211StateReassociating,
    Net80211StateAssociated,
    Net80211StateEncrypted
} NET80211_STATE, *PNET80211_STATE;

typedef enum _NET80211_MODE {
    Net80211ModeB,
    Net80211ModeG,
    Net80211ModeN
} NET80211_MODE, *PNET80211_MODE;

/*++

Structure Description:

    This structure defines the set of supported 802.11 rates.

Members:

    Count - Stores the number of valid rates in the array.

    Rate - Stores an array of rates supported by the device.

--*/

typedef struct _NET80211_RATE_INFORMATION {
    UCHAR Count;
    PUCHAR Rate;
} NET80211_RATE_INFORMATION, *PNET80211_RATE_INFORMATION;

/*++

Structure Description:

    This structure defines the information required for an 802.11 device to
    transition to a new state.

Members:

    Version - Stores the version number of the structure. Set this to
        NET80211_BSS_VERSION.

    Bssid - Stores the MAC address of the BSS's access point (a.k.a. the BSSID).

    BeaconInterval - Stores the beacon interval for the BSS to which the
        station is associated.

    Capabilities - Stores the bitmask of 802.11 capabilities for the BSS. See
        NET80211_CAPABILTY_FLAG_* for definitions.

    AssociationId - Stores the ID of the local station's association with the
        BSS.

    Timestamp - Stores the timestamp taken from the BSS access point when
        probing.

    Channel - Stores the current channel to which the device is set.

    Rssi - Stores the received signal strength indication value for the BSS.

    Mode - Stores the maximum available mode for the BSS, based on the
        AP and local station's rates.

    MaxRate - Stores the maximum supported rate shared between the BSS's AP and
        the local station.

    Rates - Stores the rates supported by the BSS.

--*/

typedef struct _NET80211_BSS {
    ULONG Version;
    UCHAR Bssid[NET80211_ADDRESS_SIZE];
    USHORT BeaconInterval;
    USHORT Capabilities;
    USHORT AssociationId;
    ULONGLONG Timestamp;
    ULONG Channel;
    LONG Rssi;
    NET80211_MODE Mode;
    UCHAR MaxRate;
    NET80211_RATE_INFORMATION Rates;
} NET80211_BSS, *PNET80211_BSS;

/*++

Structure Description:

    This structure defines information about a received 802.11 packet.

Members:

    Version - Stores the version number of the structure. SEt this to
        NET80211_RECEIVE_PACKET_VERSION.

    NetPacket - Stores a pointer to the network packet that holds the actual
        data.

    Rssi - Stores the received signal strength indication for the packet.

--*/

typedef struct _NET80211_RECEIVE_PACKET {
    ULONG Version;
    PNET_PACKET_BUFFER NetPacket;
    LONG Rssi;
} NET80211_RECEIVE_PACKET, *PNET80211_RECEIVE_PACKET;

typedef
KSTATUS
(*PNET80211_DEVICE_LINK_SEND) (
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

typedef
KSTATUS
(*PNET80211_DEVICE_LINK_GET_SET_INFORMATION) (
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

typedef
VOID
(*PNET80211_DEVICE_LINK_DESTROY_LINK) (
    PVOID DeviceContext
    );

/*++

Routine Description:

    This routine notifies the device layer that the 802.11 core is in the
    process of destroying the link and will no longer call into the device for
    this link. This allows the device layer to release any context that was
    supporting the device link interface.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the link being destroyed.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PNET80211_DEVICE_LINK_SET_CHANNEL) (
    PVOID DeviceContext,
    ULONG Channel
    );

/*++

Routine Description:

    This routine sets the 802.11 link's channel to the given value.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the 802.11 link whose channel is to be set.

    Channel - Supplies the channel to which the device should be set.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PNET80211_DEVICE_LINK_SET_STATE) (
    PVOID DeviceContext,
    NET80211_STATE State,
    PNET80211_BSS Bss
    );

/*++

Routine Description:

    This routine sets the 802.11 link to the given state. State information is
    provided to communicate the details of the 802.11 core's current state.

Arguments:

    DeviceContext - Supplies a pointer to the device context associated with
        the 802.11 link whose state is to be set.

    State - Supplies the state to which the link is being set.

    Bss - Supplies an optional pointer to information on the BSS with which the
        link is authenticating or associating.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines the interface to a device link from the 802.11
    networking library.

Members:

    Send - Stores a pointer to a function used to transmit data to the network.

    GetSetInformation - Supplies a pointer to a function used to get or set
        network link information.

    DestroyLink - Supplies a pointer to a function used to notify the device
        that the 802.11 link is no longer in use by the 802.11 core and any
        link interface context can be destroyed.

    SetChannel - Stores a pointer to a function used to set the channel.

    SetState - Stores a pointer to a function used to set the state.

--*/

typedef struct _NET80211_DEVICE_LINK_INTERFACE {
    PNET80211_DEVICE_LINK_SEND Send;
    PNET80211_DEVICE_LINK_GET_SET_INFORMATION GetSetInformation;
    PNET80211_DEVICE_LINK_DESTROY_LINK DestroyLink;
    PNET80211_DEVICE_LINK_SET_CHANNEL SetChannel;
    PNET80211_DEVICE_LINK_SET_STATE SetState;
} NET80211_DEVICE_LINK_INTERFACE, *PNET80211_DEVICE_LINK_INTERFACE;

/*++

Structure Description:

    This structure defines characteristics about an 802.11 network link.

Members:

    Version - Stores the version number of the structure. Set this to
        NET80211_LINK_PROPERTIES_VERSION.

    TransmitAlignment - Stores the alignment requirement for transmit buffers.

    Device - Stores a pointer to the physical layer device backing the link.

    DeviceContext - Stores a pointer to device-specific context on this link.

    LinkCapabilities - Stores a bitmask of capabilities indicating whether or
        not certain features are supported by the link. See
        NET_LINK_CAPABILITY_* for definitions. This is a static field and does
        not describe which features are currently enabled.

    MaxChannel - Stores the maximum supported channel the 802.11 device
        supports.

    Net80211Capabilities - Stores a bitmask of 802.11 capabilities for the
        link. See NET80211_CAPABILITY_* for definitions. These get sent with
        all association request packets.

    PacketSizeInformation - Stores the packet size information that includes
        the maximum number of bytes that can be sent over the physical link and
        the header and footer sizes.

    MaxPhysicalAddress - Stores the maximum physical address that the network
        controller can access.

    PhysicalAddress - Stores the original primary physical address of the link.

    SupportedRates - Stores a pointer to the set of rates supported by the
        802.11 device.

    Interface - Stores the list of functions used by the 802.11 networking
        library to call into the link.

--*/

typedef struct _NET80211_LINK_PROPERTIES {
    ULONG Version;
    ULONG TransmitAlignment;
    PDEVICE Device;
    PVOID DeviceContext;
    ULONG LinkCapabilities;
    ULONG MaxChannel;
    USHORT Net80211Capabilities;
    NET_PACKET_SIZE_INFORMATION PacketSizeInformation;
    PHYSICAL_ADDRESS MaxPhysicalAddress;
    NETWORK_ADDRESS PhysicalAddress;
    PNET80211_RATE_INFORMATION SupportedRates;
    NET80211_DEVICE_LINK_INTERFACE Interface;
} NET80211_LINK_PROPERTIES, *PNET80211_LINK_PROPERTIES;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

NET80211_API
KSTATUS
Net80211AddLink (
    PNET80211_LINK_PROPERTIES Properties,
    PNET80211_LINK *NewLink
    );

/*++

Routine Description:

    This routine adds the device link to the 802.11 networking core. The device
    must be ready to start sending and receiving 802.11 management frames in
    order to establish a BSS connection.

Arguments:

    Link - Supplies a pointer to the network link that was created for the
        802.11 device by the networking core.

    Properties - Supplies a pointer describing the properties and interface of
        the 802.11 link. This memory will not be referenced after the function
        returns, so this may be a stack allocated structure.

Return Value:

    Status code.

--*/

NET80211_API
VOID
Net80211RemoveLink (
    PNET80211_LINK Link
    );

/*++

Routine Description:

    This routine removes a link from the 802.11 core after its device has been
    removed. There may be outstanding references on the link, so the 802.11
    core will invoke the link destruction callback when all the references are
    released.

Arguments:

    Link - Supplies a pointer to the link to destroy. The link must be all
        cleaned up before this routine can be called.

Return Value:

    None.

--*/

NET80211_API
VOID
Net80211LinkAddReference (
    PNET80211_LINK Link
    );

/*++

Routine Description:

    This routine increases the reference count on a 802.11 link.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose reference count
        should be incremented.

Return Value:

    None.

--*/

NET80211_API
VOID
Net80211LinkReleaseReference (
    PNET80211_LINK Link
    );

/*++

Routine Description:

    This routine decreases the reference count of a 802.11 link, and destroys
    the link if the reference count drops to zero.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose reference count
        should be decremented.

Return Value:

    None.

--*/

NET80211_API
VOID
Net80211ProcessReceivedPacket (
    PNET80211_LINK Link,
    PNET80211_RECEIVE_PACKET Packet
    );

/*++

Routine Description:

    This routine is called by the low level WiFi driver to pass received
    packets onto the 802.11 core networking library for dispatching.

Arguments:

    Link - Supplies a pointer to the 802.11 link that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure and the network packet it contains may be used as a
        scratch space while this routine executes and the packet travels up the
        stack, but will not be accessed after this routine returns.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

NET80211_API
KSTATUS
Net80211GetSetLinkDeviceInformation (
    PNET80211_LINK Link,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets device information for an 802.11 link.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose device information is
        being retrieved or set.

    Uuid - Supplies a pointer to the information identifier.

    Data - Supplies a pointer to the data buffer.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer in bytes. On output, returns the needed size of the data buffer,
        even if the supplied buffer was nonexistant or too small.

    Set - Supplies a boolean indicating whether to get the information (FALSE)
        or set the information (TRUE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the supplied buffer was too small.

    STATUS_NOT_HANDLED if the given UUID was not recognized.

--*/

NET80211_API
KSTATUS
Net80211SetKey (
    PNET80211_LINK Link,
    PUCHAR KeyValue,
    ULONG KeyLength,
    ULONG KeyFlags,
    ULONG KeyId
    );

/*++

Routine Description:

    This routine sets the given key into the given network link. The 802.11
    networking library makes a local copy of all parameters.

Arguments:

    Link - Supplies a pointer to the networking link to which the keys should
        be added.

    KeyValue - Supplies a pointer to the key value.

    KeyLength - Supplies the length of the key value, in bytes.

    KeyFlags - Supplies a bitmask of flags to describe the key. See
        NET80211_KEY_FLAG_* for definitions.

    KeyId - Supplies the ID of the key negotiated between this station and its
        peers and/or access point.

Return Value:

    Status code.

--*/

