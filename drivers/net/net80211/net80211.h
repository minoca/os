/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    net80211.h

Abstract:

    This header contains definitions for the 802.11 core wireless networking
    library.

Author:

    Chris Stevens 19-Oct-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/driver.h>
#include <minoca/net/netdrv.h>

//
// Redefine the API define into an export.
//

#define NET80211_API DLLEXPORT

#include <minoca/net/net80211.h>
#include <minoca/net/net8022.h>

//
// ---------------------------------------------------------------- Definitions
//

#define NET80211_ALLOCATION_TAG 0x69666957 // 'ifiW'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _NET80211_STATE {
    Net80211StateInvalid,
    Net80211StateInitialized,
    Net80211StateStarted,
    Net80211StateStopped,
    Net80211StateProbeSent,
    Net80211StateProbeReceived,
    Net80211StateAuthenticationSent,
    Net80211StateDeauthenticationSent,
    Net80211StateAuthenticated,
    Net80211StateAssociationSent,
    Net80211StateReassociationSent,
    Net80211StateDisassociationSent,
    Net80211StateAssociated,
} NET80211_STATE, *PNET80211_STATE;

/*++

Structure Description:

    This structure defines link information that is private to the 802.11 core.

Members:

    State - Stores the current state of the 802.11 link.

    SequenceNumber - Stores the current sequence for the 802.11 link.

    Lock - Stores a pointer to a queued lock that synchronizes access to the
        802.11 link structure.

    ManagementFrameEvent - Stores an event that is signaled when an expected
        management frame arrives.

    ManagementFrameList - Stores the list of saved received management frames.

    Bssid - Stores the physical address of the access point to which this
        802.11 link is associated. In other words, the BSSID.

    Properites - Stores the 802.11 link properties.

--*/

typedef struct _NET80211_LINK {
    NET80211_STATE State;
    volatile ULONG SequenceNumber;
    PQUEUED_LOCK Lock;
    PKEVENT ManagementFrameEvent;
    LIST_ENTRY ManagementFrameList;
    NETWORK_ADDRESS Bssid;
    NET80211_LINK_PROPERTIES Properties;
} NET80211_LINK, *PNET80211_LINK;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
Net80211pJoinBss (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PSTR Ssid,
    ULONG SsidLength
    );

/*++

Routine Description:

    This routine attempts to join the link to the service set identified by the
    given SSID.

Arguments:

    Link - Supplies a pointer to the link that is requesting to join a network.

    LinkAddress - Supplies the link address for the link that wants to join the
         network.

    Ssid - Supplies the SSID of the network to join.

    SsidLength - Supplies the length of the SSID string, including the NULL
        terminator.

Return Value:

    Status code.

--*/

VOID
Net80211pProcessManagementFrame (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

/*++

Routine Description:

    This routine processes 802.11 management frames.

Arguments:

    Link - Supplies a pointer to the network link on which the frame arrived.

    Packet - Supplies a pointer to the network packet.

Return Value:

    None.

--*/

VOID
Net80211pProcessControlFrame (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

/*++

Routine Description:

    This routine processes an 802.11 control frame.

Arguments:

    Link - Supplies a pointer to the network link on which the frame arrived.

    Packet - Supplies a pointer to the network packet.

Return Value:

    None.

--*/

VOID
Net80211pSendAcknowledgeFrame (
    PNET_LINK Link,
    PNET80211_FRAME_HEADER ReceivedFrameHeader
    );

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

KSTATUS
Net80211pSendDataFrames (
    PNET_LINK Link,
    PLIST_ENTRY PacketListHead,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    );

/*++

Routine Description:

    This routine adds 802.2 SAP headers and 802.11 data frame headers to the
    given packets and sends them down the the device link layer.

Arguments:

    Link - Supplies a pointer to the link on which to send the data.

    PacketListHead - Supplies a pointer to the head of the list of network
        packets to send. Data in these packets may be modified by this routine,
        but must not be used once this routine returns.

    SourcePhysicalAddress - Supplies a pointer to the source (local) physical
        network address.

    DestinationPhysicalAddress - Supplies the optional physical address of the
        destination, or at least the next hop. If NULL is provided, then the
        packets will be sent to the data link layer's broadcast address.

    ProtocolNumber - Supplies the protocol number of the data inside the data
        link header.

Return Value:

    Status code.

--*/

VOID
Net80211pProcessDataFrame (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

/*++

Routine Description:

    This routine processes an 802.11 data frame.

Arguments:

    Link - Supplies a pointer to the network link on which the frame arrived.

    Packet - Supplies a pointer to the network packet.

Return Value:

    None.

--*/

ULONG
Net80211pGetSequenceNumber (
    PNET_LINK Link
    );

/*++

Routine Description:

    This routine returns the sequence number to use for the given link.

Arguments:

    Link - Supplies a pointer to the link whose sequence number is requested.

Return Value:

    Returns the sequence number to use for the given link.

--*/

