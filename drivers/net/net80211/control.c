/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an 802.11 control frame.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which the frame arrived.

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

//
// --------------------------------------------------------- Internal Functions
//

