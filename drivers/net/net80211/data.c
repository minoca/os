/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

KSTATUS
Net80211pSendNullDataFrame (
    PNET80211_LINK Link,
    USHORT FrameControl
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Net80211pSendDataFrames (
    PNET80211_LINK Link,
    PNET_PACKET_LIST PacketList,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    )

/*++

Routine Description:

    This routine adds 802.2 SAP headers and 802.11 data frame headers to the
    given packets and sends them down the the device link layer.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which to send the data.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

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

    PNET80211_BSS_ENTRY Bss;
    PLIST_ENTRY CurrentEntry;
    BOOL DataPaused;
    PNET80211_DATA_FRAME_HEADER Header;
    ULONG Index;
    PNET8022_LLC_HEADER LlcHeader;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PausedPacketList;
    PNET_DEVICE_LINK_SEND Send;
    PNET8022_SNAP_EXTENSION SnapExtension;
    KSTATUS Status;

    //
    // Get the active BSS in order to determine the correct receiver address
    // and whether or not the data needs to be encrypted.
    //

    Bss = Net80211pGetBss(Link);
    if (Bss == NULL) {
        return STATUS_NOT_CONNECTED;
    }

    //
    // Determine if transmission is paused. If it is, then the BSS may no
    // longer be active. Fill out as much of the headers as possible and queue
    // the packets for later.
    //

    DataPaused = FALSE;
    if ((Link->Flags & NET80211_LINK_FLAG_DATA_PAUSED) != 0) {
        DataPaused = TRUE;
        NET_INITIALIZE_PACKET_LIST(&PausedPacketList);
    }

    //
    // Fill out the 802.11 headers for these data frames.
    //

    Status = STATUS_SUCCESS;
    CurrentEntry = PacketList->Head.Next;
    while (CurrentEntry != &(PacketList->Head)) {
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

        RtlCopyMemory(Header->TransmitterAddress,
                      SourcePhysicalAddress->Address,
                      NET80211_ADDRESS_SIZE);

        //
        // If data transmission is paused and this packet should not be forced
        // down to the driver, add it to the local list of packets to send
        // later. Do not fill out any BSS specific information or the sequence
        // number. The BSS may change by the time data transmission is resumed.
        //

        if ((DataPaused != FALSE) &&
            ((Packet->Flags & NET_PACKET_FLAG_FORCE_TRANSMIT) == 0)) {

            NET_REMOVE_PACKET_FROM_LIST(Packet, PacketList);
            NET_ADD_PACKET_TO_LIST(Packet, &PausedPacketList);
            continue;
        }

        Header->SequenceControl = Net80211pGetSequenceNumber(Link);
        Header->SequenceControl <<=
                               NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_SHIFT;

        RtlCopyMemory(Header->ReceiverAddress,
                      Bss->State.Bssid,
                      NET80211_ADDRESS_SIZE);

        //
        // Only encrypt the packet if transmission is not paused. If it is
        // paused then this station may be in the middle of acquiring new keys
        // for the BSS.
        //

        if (((Bss->Flags & NET80211_BSS_FLAG_ENCRYPT_DATA) != 0) &&
            ((Packet->Flags & NET_PACKET_FLAG_UNENCRYPTED) == 0)) {

            Header->FrameControl |= NET80211_FRAME_CONTROL_PROTECTED_FRAME;
            Net80211pEncryptPacket(Link, Bss, Packet);
        }
    }

    //
    // If any packets were added to the local paused list, then add them to the
    // link's list.
    //

    if ((DataPaused != FALSE) &&
        (NET_PACKET_LIST_EMPTY(&PausedPacketList) == FALSE)) {

        KeAcquireQueuedLock(Link->Lock);
        NET_APPEND_PACKET_LIST(&PausedPacketList,
                               &(Link->PausedPacketList));

        KeReleaseQueuedLock(Link->Lock);
    }

    //
    // Send any remaining packets down to the physical device layer.
    //

    if (NET_PACKET_LIST_EMPTY(PacketList) == FALSE) {
        Send = Link->Properties.Interface.Send;
        Status = Send(Link->Properties.DeviceContext, PacketList);

        //
        // If the link layer returns that the resource is in use it means it
        // was too busy to send all of the packets. Release the packets for it
        // and convert this into a success status.
        //

        if (Status == STATUS_RESOURCE_IN_USE) {
            NetDestroyBufferList(PacketList);
            Status = STATUS_SUCCESS;
        }
    }

    Net80211pBssEntryReleaseReference(Bss);
    return Status;
}

VOID
Net80211pProcessDataFrame (
    PNET80211_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine processes an 802.11 data frame.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which the frame arrived.

    Packet - Supplies a pointer to the network packet.

Return Value:

    None.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    ULONG BytesRemaining;
    UCHAR DestinationSap;
    PNET80211_DATA_FRAME_HEADER Header;
    PNET8022_LLC_HEADER LlcHeader;
    PNET_NETWORK_ENTRY NetworkEntry;
    ULONG NetworkProtocol;
    NET_RECEIVE_CONTEXT ReceiveContext;
    PNET8022_SNAP_EXTENSION SnapExtension;
    UCHAR SourceSap;
    KSTATUS Status;

    //
    // Make sure there are at least enough bytes for a data frame header.
    //

    BytesRemaining = Packet->FooterOffset - Packet->DataOffset;
    if (BytesRemaining < sizeof(NET80211_DATA_FRAME_HEADER)) {
        RtlDebugPrint("802.2: malformed data packet missing bytes for data "
                      "frame header. Expected %d bytes, has %d.\n",
                      sizeof(NET80211_DATA_FRAME_HEADER),
                      BytesRemaining);

        return;
    }

    //
    // If the packet is protected, then decrypt it. The decryption leaves the
    // packet's data offset at the start of the decrypted ciphertext.
    //

    Header = Packet->Buffer + Packet->DataOffset;
    if ((Header->FrameControl & NET80211_FRAME_CONTROL_PROTECTED_FRAME) != 0) {
        Bss = Net80211pGetBss(Link);
        if (Bss == NULL) {
            return;
        }

        Status = Net80211pDecryptPacket(Link, Bss, Packet);
        Net80211pBssEntryReleaseReference(Bss);
        if (!KSUCCESS(Status)) {
            return;
        }

    //
    // Otherwise remove the 802.11 header. It should always be the same size as
    // this node does not handle QoS at the moment and is only expecting '
    // traffic from the DS.
    //

    } else {
        Packet->DataOffset += sizeof(NET80211_DATA_FRAME_HEADER);
    }

    //
    // Reject packets that do not have enough room for the LLC header.
    //

    BytesRemaining = Packet->FooterOffset - Packet->DataOffset;
    if (BytesRemaining < sizeof(NET8022_LLC_HEADER)) {
        RtlDebugPrint("802.2: malformed data packet missing bytes for LLC "
                      "header. Expected %d bytes, has %d.\n",
                      sizeof(NET8022_LLC_HEADER),
                      BytesRemaining);

        return;
    }

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
    // Reject packets that do not have enough room for the SNAP extension.
    //

    BytesRemaining = Packet->FooterOffset - Packet->DataOffset;
    if (BytesRemaining < sizeof(NET8022_SNAP_EXTENSION)) {
        RtlDebugPrint("802.2: malformed data packet missing bytes for SNAP "
                      "extension. Expected %d bytes, has %d.\n",
                      sizeof(NET8022_SNAP_EXTENSION),
                      BytesRemaining);

        return;
    }

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
    RtlZeroMemory(&ReceiveContext, sizeof(NET_RECEIVE_CONTEXT));
    ReceiveContext.Packet = Packet;
    ReceiveContext.Link = Link->NetworkLink;
    ReceiveContext.Network = NetworkEntry;
    NetworkEntry->Interface.ProcessReceivedData(&ReceiveContext);
    return;
}

VOID
Net80211pPauseDataFrames (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine pauses the outgoing data frame traffic on the given network
    link. It assumes that the 802.11 link's queued lock is held.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which to pause the outgoing
        data frames.

Return Value:

    None.

--*/

{

    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

    //
    // If associated, send a power save null data frame to the AP in order to
    // pause all incoming data traffic.
    //

    if (Link->ActiveBss != NULL) {
        Net80211pSendNullDataFrame(Link,
                                   NET80211_FRAME_CONTROL_POWER_MANAGEMENT);
    }

    Link->Flags |= NET80211_LINK_FLAG_DATA_PAUSED;
    return;
}

VOID
Net80211pResumeDataFrames (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine resumes the outgoing data frame traffic on the given network
    link, flushing any packets that were held while the link was paused. It
    assumes that the 802.11 link's queued lock is held.

Arguments:

    Link - Supplies a pointer to the 802.11 link on which to resume the
        outgoing data frames.

Return Value:

    None.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    PLIST_ENTRY CurrentEntry;
    PNET80211_DATA_FRAME_HEADER Header;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    PNET_DEVICE_LINK_SEND Send;
    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(Link->Lock) != FALSE);

    //
    // There is nothing to be done if the data frames were not paused.
    //

    if ((Link->Flags & NET80211_LINK_FLAG_DATA_PAUSED) == 0) {
        return;
    }

    //
    // If the link is associated, then send the AP a null data frame indicating
    // that the station is coming out of power save mode.
    //

    if (Link->ActiveBss != NULL) {
        Net80211pSendNullDataFrame(Link, 0);
    }

    //
    // Attempt to flush the packets that were queued up.
    //

    Link->Flags &= ~NET80211_LINK_FLAG_DATA_PAUSED;
    if (NET_PACKET_LIST_EMPTY(&(Link->PausedPacketList)) != FALSE) {
        return;
    }

    NET_INITIALIZE_PACKET_LIST(&PacketList);
    NET_APPEND_PACKET_LIST(&(Link->PausedPacketList), &PacketList);

    //
    // With the link lock held, just use the active BSS to fill out and encrypt
    // the queued packets.
    //

    Bss = Link->ActiveBss;

    ASSERT(Bss != NULL);

    CurrentEntry = PacketList.Head.Next;
    while (CurrentEntry != &(PacketList.Head)) {
        Packet = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Header = Packet->Buffer + Packet->DataOffset;
        RtlCopyMemory(Header->ReceiverAddress,
                      Bss->State.Bssid,
                      NET80211_ADDRESS_SIZE);

        Header->SequenceControl = Net80211pGetSequenceNumber(Link);
        Header->SequenceControl <<=
                               NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_SHIFT;

        if (((Bss->Flags & NET80211_BSS_FLAG_ENCRYPT_DATA) != 0) &&
            ((Packet->Flags & NET_PACKET_FLAG_UNENCRYPTED) == 0)) {

            Header->FrameControl |= NET80211_FRAME_CONTROL_PROTECTED_FRAME;
            Net80211pEncryptPacket(Link, Bss, Packet);
        }
    }

    Send = Link->Properties.Interface.Send;
    Status = Send(Link->Properties.DeviceContext, &PacketList);
    if (Status == STATUS_RESOURCE_IN_USE) {
        NetDestroyBufferList(&PacketList);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Net80211pSendNullDataFrame (
    PNET80211_LINK Link,
    USHORT FrameControl
    )

/*++

Routine Description:

    This routine sends an 802.11 null data frame with the given frame control
    bits set. This bypasses the normal data frame submission paths because it
    never requires encryption and does not require the 802.2 headers.

Arguments:

    Link - Supplies a pointer to the link on which to send the data frame.

    FrameControl - Supplies a bitmask of extra frame control values to stick
        in the header.

Return Value:

    Status code.

--*/

{

    PVOID DeviceContext;
    ULONG Flags;
    PNET80211_DATA_FRAME_HEADER Header;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    KSTATUS Status;

    NET_INITIALIZE_PACKET_LIST(&PacketList);

    //
    // Allocate a network packet to send down to the lower layers.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS;

    Packet = NULL;
    Status = NetAllocateBuffer(sizeof(NET80211_DATA_FRAME_HEADER),
                               0,
                               0,
                               Link->NetworkLink,
                               Flags,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto SendNullDataFrame;
    }

    //
    // Move the offset backwards and fill in the 802.11 management frame header.
    //

    Packet->DataOffset -= sizeof(NET80211_DATA_FRAME_HEADER);
    Header = Packet->Buffer + Packet->DataOffset;
    FrameControl &= ~NET80211_FRAME_CONTROL_FROM_DS;
    FrameControl |= NET80211_FRAME_CONTROL_TO_DS |
                    (NET80211_FRAME_CONTROL_PROTOCOL_VERSION <<
                     NET80211_FRAME_CONTROL_PROTOCOL_VERSION_SHIFT) |
                    (NET80211_FRAME_TYPE_DATA <<
                     NET80211_FRAME_CONTROL_TYPE_SHIFT) |
                    (NET80211_DATA_FRAME_SUBTYPE_NO_DATA <<
                     NET80211_FRAME_CONTROL_SUBTYPE_SHIFT);

    Header->FrameControl = FrameControl;

    //
    // The hardware handles the duration.
    //

    Header->DurationId = 0;

    //
    // Initialize the header's addresses. The receiver and destination address
    // are always the BSSID as this is being sent to the AP.
    //

    ASSERT(Link->ActiveBss != NULL);

    RtlCopyMemory(Header->ReceiverAddress,
                  Link->ActiveBss->State.Bssid,
                  NET80211_ADDRESS_SIZE);

    //
    // The source address is always the local link's physical address (i.e. the
    // MAC address).
    //

    RtlCopyMemory(Header->TransmitterAddress,
                  Link->Properties.PhysicalAddress.Address,
                  NET80211_ADDRESS_SIZE);

    RtlCopyMemory(Header->SourceDestinationAddress,
                  Link->ActiveBss->State.Bssid,
                  NET80211_ADDRESS_SIZE);

    //
    // The header gets the next sequence number for the link. This is only 1
    // fragment, so that remains 0.
    //

    Header->SequenceControl = Net80211pGetSequenceNumber(Link);
    Header->SequenceControl <<= NET80211_SEQUENCE_CONTROL_SEQUENCE_NUMBER_SHIFT;

    //
    // Send the packet off.
    //

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);
    DeviceContext = Link->Properties.DeviceContext;
    Status = Link->Properties.Interface.Send(DeviceContext, &PacketList);
    if (!KSUCCESS(Status)) {
        goto SendNullDataFrame;
    }

SendNullDataFrame:
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

