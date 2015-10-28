/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    data.c

Abstract:

    This module implements data frame handling functionality for the 802.11
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

KSTATUS
Net80211pSendDataFrames (
    PNET_LINK Link,
    PLIST_ENTRY PacketListHead,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    )

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

{

    PLIST_ENTRY CurrentEntry;
    PVOID DriverContext;
    PNET80211_DATA_FRAME_HEADER Header;
    ULONG Index;
    PNET8022_LLC_HEADER LlcHeader;
    PNET80211_LINK Net80211Link;
    PNET_PACKET_BUFFER Packet;
    PNET8022_SNAP_EXTENSION SnapExtension;

    Net80211Link = Link->DataLinkContext;

    //
    // Fill out the 802.11 headers for these data frames.
    //

    CurrentEntry = PacketListHead->Next;
    while (CurrentEntry != PacketListHead) {
        Packet = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Add the 802.2 headers.
        //

        Packet->DataOffset -= sizeof(NET8022_SNAP_EXTENSION);
        SnapExtension = Packet->Buffer + Packet->DataOffset;
        RtlZeroMemory(SnapExtension, sizeof(NET8022_SNAP_EXTENSION));
        SnapExtension->EthernetType = CPU_TO_NETWORK16((USHORT)ProtocolNumber);
        Packet->DataOffset -= sizeof(NET8022_LLC_HEADER);
        LlcHeader = Packet->Buffer + Packet->DataOffset;
        LlcHeader->DestinationSapAddress = NET8022_SAP_ADDRESS_SNAP_EXTENSION;
        LlcHeader->SourceSapAddress = NET8022_SAP_ADDRESS_SNAP_EXTENSION;
        LlcHeader->Control = (NET8022_CONTROL_TYPE_UNNUMBERED <<
                              NET8022_CONTROL_TYPE_SHIFT);

        //
        // Add 802.11 headers.
        //

        Packet->DataOffset -= sizeof(NET80211_DATA_FRAME_HEADER);
        Header = Packet->Buffer + Packet->DataOffset;
        Header->FrameControl = (NET80211_FRAME_CONTROL_PROTOCOL_VERSION <<
                                NET80211_FRAME_CONTROL_PROTOCOL_VERSION_SHIFT) |
                               (NET80211_FRAME_TYPE_DATA <<
                                NET80211_FRAME_CONTROL_TYPE_SHIFT) |
                               (NET80211_DATA_FRAME_SUBTYPE_DATA <<
                                NET80211_FRAME_CONTROL_SUBTYPE_SHIFT);

        //
        // The hardware handles the duration.
        //

        Header->DurationId = 0;

        //
        // As the 802.11 core only supports operating in station mode at the
        // moment, assume all packets are going out to the DS. As a result, the
        // receive address is set to the AP's MAC address (i.e. the BSSID) and
        // the real destination is set in the header's third address.
        //

        Header->FrameControl |= NET80211_FRAME_CONTROL_TO_DS;
        if (DestinationPhysicalAddress != NULL) {
            RtlCopyMemory(Header->SourceDestinationAddress,
                          DestinationPhysicalAddress->Address,
                          NET80211_ADDRESS_SIZE);

        } else {
            for (Index = 0; Index < NET80211_ADDRESS_SIZE; Index += 1) {
                Header->SourceDestinationAddress[Index] = 0xFF;
            }
        }

        RtlCopyMemory(Header->ReceiverAddress,
                      Net80211Link->BssState.Bssid,
                      NET80211_ADDRESS_SIZE);

        RtlCopyMemory(Header->TransmitterAddress,
                      SourcePhysicalAddress->Address,
                      NET80211_ADDRESS_SIZE);

        Header->SequenceControl = Net80211pGetSequenceNumber(Link);
        Header->SequenceControl <<=
                               NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_SHIFT;
    }

    DriverContext = Link->Properties.DriverContext;
    return Link->Properties.Interface.Send(DriverContext, PacketListHead);
}

VOID
Net80211pProcessDataFrame (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an 802.11 data frame.

Arguments:

    Link - Supplies a pointer to the network link on which the frame arrived.

    Packet - Supplies a pointer to the network packet.

Return Value:

    None.

--*/

{

    UCHAR DestinationSap;
    PNET8022_LLC_HEADER LlcHeader;
    PNET_NETWORK_ENTRY NetworkEntry;
    ULONG NetworkProtocol;
    PNET8022_SNAP_EXTENSION SnapExtension;
    UCHAR SourceSap;

    //
    // Remove the 802.11 header. It should be the same size as this station
    // does not handle QoS at the moment and is only expecting traffic from the
    // DS.
    //

    Packet->DataOffset += sizeof(NET80211_DATA_FRAME_HEADER);

    //
    // Check the LLC header to look for the SNAP extension and unnumbered
    // control type. The 802.11 core does not handle any other packet types.
    //

    LlcHeader = Packet->Buffer + Packet->DataOffset;
    DestinationSap = LlcHeader->DestinationSapAddress;
    SourceSap = LlcHeader->SourceSapAddress;
    if ((DestinationSap != NET8022_SAP_ADDRESS_SNAP_EXTENSION) ||
        (SourceSap != NET8022_SAP_ADDRESS_SNAP_EXTENSION)) {

        return;
    }

    if ((LlcHeader->Control & NET8022_CONTROL_TYPE_MASK) !=
        NET8022_CONTROL_TYPE_UNNUMBERED) {

        return;
    }

    Packet->DataOffset += sizeof(NET8022_LLC_HEADER);

    //
    // Get the network protocol out of the SNAP extension.
    //

    SnapExtension = Packet->Buffer + Packet->DataOffset;
    NetworkProtocol = NETWORK_TO_CPU16(SnapExtension->EthernetType);

    //
    // Get the network layer to deal with this.
    //

    NetworkEntry = NetGetNetworkEntry(NetworkProtocol);
    if (NetworkEntry == NULL) {
        RtlDebugPrint("Unknown protocol number 0x%x found in 802.2 header.\n",
                      NetworkProtocol);

        return;
    }

    Packet->DataOffset += sizeof(NET8022_SNAP_EXTENSION);
    NetworkEntry->Interface.ProcessReceivedData(Link, Packet);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

