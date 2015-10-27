/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    control.c

Abstract:

    This module implements control frame handling functionality for the 802.11
    core wireless networking library.

Author:

    Chris Stevens 20-Oct-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "net80211.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _NET80211_ACK_FRAME {
    USHORT FrameControl;
    USHORT Duration;
    UCHAR ReceiverAddress[NET80211_ADDRESS_SIZE];
} NET80211_ACK_FRAME, *PNET80211_ACK_FRAME;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
Net80211pProcessControlFrame (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an 802.11 control frame.

Arguments:

    Link - Supplies a pointer to the network link on which the frame arrived.

    Packet - Supplies a pointer to the network packet.

Return Value:

    None.

--*/

{

    ULONG FrameSubtype;
    PNET80211_FRAME_HEADER Header;

    Header = Packet->Buffer + Packet->DataOffset;
    FrameSubtype = NET80211_GET_FRAME_SUBTYPE(Header);
    switch (FrameSubtype) {
    case NET80211_CONTROL_FRAME_SUBTYPE_ACK:
    case NET80211_CONTROL_FRAME_SUBTYPE_CONTROL_WRAPPER:
    case NET80211_CONTROL_FRAME_SUBTYPE_BLOCK_ACK_REQUEST:
    case NET80211_CONTROL_FRAME_SUBTYPE_BLOCK_ACK:
    case NET80211_CONTROL_FRAME_SUBTYPE_PS_POLL:
    case NET80211_CONTROL_FRAME_SUBTYPE_RTS:
    case NET80211_CONTROL_FRAME_SUBTYPE_CTS:
    case NET80211_CONTROL_FRAME_SUBTYPE_CF_END:
    case NET80211_CONTROL_FRAME_SUBTYPE_CF_END_ACK:
    default:
        break;
    }

    return;
}

VOID
Net80211pSendAcknowledgeFrame (
    PNET_LINK Link,
    PNET80211_FRAME_HEADER ReceivedFrameHeader
    )

/*++

Routine Description:

    This routine acknowledges the received packet by sending an ACK control
    frame.

Arguments:

    Link - Supplies the link on which the frame was received.

    ReceivedFrameHeader - Supplies the header of the received frame that needs
        to be acknowledged.

Return Value:

    None.

--*/

{

    PNET80211_ACK_FRAME AckFrame;
    PVOID DriverContext;
    ULONG Flags;
    PNET_PACKET_BUFFER Packet;
    LIST_ENTRY PacketListHead;
    ULONG ReceivedFrameSubtype;
    ULONG ReceivedFrameType;
    KSTATUS Status;

    //
    // Unicast frames do not get acknowledged.
    //

    if (NET80211_IS_MULTICAST_BROADCAST(ReceivedFrameHeader) == FALSE) {
        return;
    }

    //
    // All data and management frames get acknowledged, but only a select set
    // of control frames get acknowledged.
    //

    ReceivedFrameType = NET80211_GET_FRAME_TYPE(ReceivedFrameHeader);
    ReceivedFrameSubtype = NET80211_GET_FRAME_SUBTYPE(ReceivedFrameHeader);
    if ((ReceivedFrameType == NET80211_FRAME_TYPE_CONTROL) &&
        ((ReceivedFrameSubtype != NET80211_CONTROL_FRAME_SUBTYPE_BLOCK_ACK) &&
         (ReceivedFrameSubtype != NET80211_CONTROL_FRAME_SUBTYPE_PS_POLL) &&
         (ReceivedFrameSubtype !=
          NET80211_CONTROL_FRAME_SUBTYPE_BLOCK_ACK_REQUEST))) {

        return;
    }

    //
    // Allocate a network packet for the ACK.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS;

    Packet = NULL;
    Status = NetAllocateBuffer(0,
                               sizeof(NET80211_ACK_FRAME),
                               0,
                               Link,
                               Flags,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto SendAcknowledgeFrameEnd;
    }

    //
    // Initialize the ACK frame, which is the entry body of the 802.11 packet.
    //

    AckFrame = Packet->Buffer + Packet->DataOffset;
    AckFrame->FrameControl = (NET80211_FRAME_CONTROL_PROTOCOL_VERSION <<
                              NET80211_FRAME_CONTROL_PROTOCOL_VERSION_SHIFT) |
                             (NET80211_FRAME_TYPE_CONTROL <<
                              NET80211_FRAME_CONTROL_TYPE_SHIFT) |
                             (NET80211_CONTROL_FRAME_SUBTYPE_ACK <<
                              NET80211_FRAME_CONTROL_SUBTYPE_SHIFT);

    //
    // TODO: Determine how much time to subtract from the received duration ID.
    //

    AckFrame->Duration = ReceivedFrameHeader->DurationId - 0;
    RtlCopyMemory(AckFrame->ReceiverAddress,
                  ReceivedFrameHeader->Address2,
                  NET80211_ADDRESS_SIZE);

    INITIALIZE_LIST_HEAD(&PacketListHead);
    INSERT_BEFORE(&(Packet->ListEntry), &PacketListHead);
    DriverContext = Link->Properties.DriverContext;
    Status = Link->Properties.Interface.Send(DriverContext, &PacketListHead);
    if (!KSUCCESS(Status)) {
        goto SendAcknowledgeFrameEnd;
    }

SendAcknowledgeFrameEnd:
    if (!KSUCCESS(Status)) {
        if (Packet != NULL) {
            NetFreeBuffer(Packet);
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

