/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

#define NET80211_GET_PACKET_TYPE(_Header)                            \
    (((_Header)->FrameControl & NET80211_FRAME_CONTROL_TYPE_MASK) >> \
     NET80211_FRAME_CONTROL_TYPE_SHIFT)

//
// This macro returns the 802.11 packet's subtype.
//

#define NET80211_GET_PACKET_SUBTYPE(_Header)                            \
    (((_Header)->FrameControl & NET80211_FRAME_CONTROL_SUBTYPE_MASK) >> \
     NET80211_FRAME_CONTROL_SUBTYPE_SHIFT)

//
// ---------------------------------------------------------------- Definitions
//

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
#define NET80211_FRAME_CONTROL_TYPE_MANAGEMENT        0
#define NET80211_FRAME_CONTROL_TYPE_CONTROL           1
#define NET80211_FRAME_CONTROL_TYPE_DATA              2
#define NET80211_FRAME_CONTROL_PROTOCOL_VERSION_MASK  0x0003
#define NET80211_FRAME_CONTROL_PROTOCOL_VERSION_SHIFT 0
#define NET80211_FRAME_CONTROL_PROTOCOL_VERSION       0

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
// ------------------------------------------------------ Data Type Definitions
//

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

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
