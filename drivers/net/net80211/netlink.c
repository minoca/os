/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    netlink.c

Abstract:

    This module implements the generic netlink 802.11 family message handling.

Author:

    Chris Stevens 29-Mar-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "net80211.h"
#include <minoca/net/netlink.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the multicast group indices. These must match the order of the
// multicast group array below.
//

#define NETLINK_GENERIC_80211_MULTICAST_SCAN 0

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Net80211pNetlinkJoin (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    );

KSTATUS
Net80211pNetlinkLeave (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    );

KSTATUS
Net80211pNetlinkScanStart (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    );

VOID
Net80211pNetlinkScanCompletionRoutine (
    PNET80211_LINK Link,
    KSTATUS ScanStatus
    );

KSTATUS
Net80211pNetlinkScanGetResults (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    );

KSTATUS
Net80211pNetlinkGetLink (
    PNET_PACKET_BUFFER Packet,
    PNET80211_LINK *Link
    );

VOID
Net80211pNetlinkSendScanNotification (
    PNET80211_LINK Link,
    UCHAR Command
    );

//
// -------------------------------------------------------------------- Globals
//

NETLINK_GENERIC_COMMAND Net80211NetlinkCommands[] = {
    {
        NETLINK_80211_COMMAND_JOIN,
        0,
        Net80211pNetlinkJoin
    },

    {
        NETLINK_80211_COMMAND_LEAVE,
        0,
        Net80211pNetlinkLeave
    },

    {
        NETLINK_80211_COMMAND_SCAN_START,
        0,
        Net80211pNetlinkScanStart
    },

    {
        NETLINK_80211_COMMAND_SCAN_GET_RESULTS,
        NETLINK_HEADER_FLAG_DUMP,
        Net80211pNetlinkScanGetResults
    },
};

NETLINK_GENERIC_MULTICAST_GROUP Net80211NetlinkMulticastGroups[] = {
    {
        NETLINK_GENERIC_80211_MULTICAST_SCAN,
        sizeof(NETLINK_80211_MULTICAST_SCAN_NAME),
        NETLINK_80211_MULTICAST_SCAN_NAME
    },
};

NETLINK_GENERIC_FAMILY_PROPERTIES Net80211NetlinkFamilyProperties = {
    NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION,
    0,
    sizeof(NETLINK_GENERIC_80211_NAME),
    NETLINK_GENERIC_80211_NAME,
    Net80211NetlinkCommands,
    sizeof(Net80211NetlinkCommands) / sizeof(Net80211NetlinkCommands[0]),
    Net80211NetlinkMulticastGroups,
    sizeof(Net80211NetlinkMulticastGroups) /
        sizeof(Net80211NetlinkMulticastGroups[0]),
};

PNETLINK_GENERIC_FAMILY Net80211NetlinkFamily = NULL;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Net80211pNetlinkInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the generic netlink 802.11 family.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = NetlinkGenericRegisterFamily(&Net80211NetlinkFamilyProperties,
                                          &Net80211NetlinkFamily);

    return Status;
}

VOID
Net80211pNetlinkDestroy (
    VOID
    )

/*++

Routine Description:

    This routine tears down support for the generic netlink 802.11 family.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (Net80211NetlinkFamily != NULL) {
        NetlinkGenericUnregisterFamily(Net80211NetlinkFamily);
        Net80211NetlinkFamily = NULL;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Net80211pNetlinkJoin (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    )

/*++

Routine Description:

    This routine is called to process an 802.11 netlink network join request.
    It attempts to join a device to a network as specified by the netlink
    message.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    Command - Supplies a pointer to the command information.

Return Value:

    Status code.

--*/

{

    PVOID Attributes;
    ULONG AttributesLength;
    PVOID Bssid;
    USHORT BssidLength;
    PNET80211_LINK Link;
    PUCHAR Passphrase;
    USHORT PassphraseLength;
    NET80211_SCAN_STATE ScanParameters;
    PUCHAR Ssid;
    USHORT SsidLength;
    KSTATUS Status;

    //
    // Parse the packet to find the 802.11 link that is to join a network.
    //

    Status = Net80211pNetlinkGetLink(Packet, &Link);
    if (!KSUCCESS(Status)) {
        goto NetlinkJoinEnd;
    }

    //
    // An SSID is necessary even if a BSSID is supplied. The user shouldn't
    // join an BSSID that switched SSID's on it.
    //

    Attributes = Packet->Buffer + Packet->DataOffset;
    AttributesLength = Packet->FooterOffset - Packet->DataOffset;
    Status = NetlinkGetAttribute(Attributes,
                                 AttributesLength,
                                 NETLINK_80211_ATTRIBUTE_SSID,
                                 (PVOID *)&Ssid,
                                 &SsidLength);

    if (!KSUCCESS(Status)) {
        goto NetlinkJoinEnd;
    }

    //
    // The attribute should store a null-terminated SSID string. Fail if it is
    // not present and strip it if it is present. The scan parameters do not
    // take a null-terminator SSID.
    //

    if (Ssid[SsidLength - 1] != STRING_TERMINATOR) {
        Status = STATUS_INVALID_PARAMETER;
        goto NetlinkJoinEnd;
    }

    SsidLength -= 1;
    if (SsidLength > NET80211_MAX_SSID_LENGTH) {
        Status = STATUS_NAME_TOO_LONG;
        goto NetlinkJoinEnd;
    }

    //
    // The passphrase is optional as some network do not require them. Make
    // sure it is null-terminated and strip the null character if it is.
    //

    Status = NetlinkGetAttribute(Attributes,
                                 AttributesLength,
                                 NETLINK_80211_ATTRIBUTE_PASSPHRASE,
                                 (PVOID *)&Passphrase,
                                 &PassphraseLength);

    if (KSUCCESS(Status)) {
        if (Passphrase[PassphraseLength - 1] != STRING_TERMINATOR) {
            Status = STATUS_INVALID_PARAMETER;
            goto NetlinkJoinEnd;
        }

        PassphraseLength -= 1;
        if (PassphraseLength > NET80211_MAX_PASSPHRASE_LENGTH) {
            Status = STATUS_NAME_TOO_LONG;
            goto NetlinkJoinEnd;
        }
    }

    //
    // The BSSID is optional as the MAC address of the access point is not
    // always known.
    //

    Status = NetlinkGetAttribute(Attributes,
                                 AttributesLength,
                                 NETLINK_80211_ATTRIBUTE_BSSID,
                                 &Bssid,
                                 &BssidLength);

    if (KSUCCESS(Status)) {
        if (BssidLength != NET80211_ADDRESS_SIZE) {
            Status = STATUS_INVALID_PARAMETER;
            goto NetlinkJoinEnd;
        }
    }

    RtlZeroMemory(&ScanParameters, sizeof(NET80211_SCAN_STATE));
    ScanParameters.Link = Link;
    ScanParameters.Flags = NET80211_SCAN_FLAG_JOIN;
    if (BssidLength == 0) {
        ScanParameters.Flags |= NET80211_SCAN_FLAG_BROADCAST;

    } else {
        RtlCopyMemory(ScanParameters.Bssid, Bssid, BssidLength);
    }

    if (PassphraseLength != 0) {
        ScanParameters.PassphraseLength = PassphraseLength;
        RtlCopyMemory(ScanParameters.Passphrase, Passphrase, PassphraseLength);
    }

    RtlCopyMemory(ScanParameters.Ssid, Ssid, SsidLength);
    ScanParameters.SsidLength = SsidLength;
    Status = Net80211pStartScan(Link, &ScanParameters);
    if (!KSUCCESS(Status)) {
        goto NetlinkJoinEnd;
    }

NetlinkJoinEnd:
    if (Link != NULL) {
        Net80211LinkReleaseReference(Link);
    }

    return Status;
}

KSTATUS
Net80211pNetlinkLeave (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    )

/*++

Routine Description:

    This routine is called to process an 802.11 netlink request for a device to
    leave its current network.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    Command - Supplies a pointer to the command information.

Return Value:

    Status code.

--*/

{

    PNET80211_LINK Link;
    KSTATUS Status;

    //
    // Parse the packet to find the 802.11 link that is to leave its network.
    //

    Status = Net80211pNetlinkGetLink(Packet, &Link);
    if (!KSUCCESS(Status)) {
        goto NetlinkLeaveEnd;
    }

    //
    // Setting the link state to initialized will deactivate the current
    // connection and send the appropriate deactivation messages to the access
    // point.
    //

    Net80211pSetState(Link, Net80211StateInitialized);

NetlinkLeaveEnd:
    if (Link != NULL) {
        Net80211LinkReleaseReference(Link);
    }

    return Status;
}

KSTATUS
Net80211pNetlinkScanStart (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    )

/*++

Routine Description:

    This routine is called to process an 802.11 netlink request to start
    scanning for available wireless networks.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    Command - Supplies a pointer to the command information.

Return Value:

    Status code.

--*/

{

    PNET80211_LINK Link;
    NET80211_SCAN_STATE ScanParameters;
    KSTATUS Status;

    //
    // Parse the packet to find the 802.11 link that is to scan for networks.
    //

    Status = Net80211pNetlinkGetLink(Packet, &Link);
    if (!KSUCCESS(Status)) {
        goto NetlinkScanStartEnd;
    }

    //
    // Kick off a background scan to update the BSS cache for this link.
    //

    RtlZeroMemory(&ScanParameters, sizeof(NET80211_SCAN_STATE));
    ScanParameters.Link = Link;
    ScanParameters.Flags = NET80211_SCAN_FLAG_BROADCAST;
    ScanParameters.CompletionRoutine = Net80211pNetlinkScanCompletionRoutine;
    Status = Net80211pStartScan(Link, &ScanParameters);
    if (!KSUCCESS(Status)) {
        goto NetlinkScanStartEnd;
    }

    //
    // Notify the scan multicast group that this scan is starting.
    //

    Net80211pNetlinkSendScanNotification(Link,
                                         NETLINK_80211_COMMAND_SCAN_START);

NetlinkScanStartEnd:
    if (Link != NULL) {
        Net80211LinkReleaseReference(Link);
    }

    return Status;
}

VOID
Net80211pNetlinkScanCompletionRoutine (
    PNET80211_LINK Link,
    KSTATUS ScanStatus
    )

/*++

Routine Description:

    This routine is called when a scan for nearby BSS access points has
    completed.

Arguments:

    Link - Supplies a pointer to the 802.11 link that performed the scan.

    ScanStatus - Supplies the status result of the scan.

Return Value:

    None.

--*/

{

    UCHAR Command;

    //
    // Report success or failure without any further details on an error.
    //

    Command = NETLINK_80211_COMMAND_SCAN_RESULT;
    if (!KSUCCESS(ScanStatus)) {
        Command = NETLINK_80211_COMMAND_SCAN_ABORTED;
    }

    Net80211pNetlinkSendScanNotification(Link, Command);
    return;
}

KSTATUS
Net80211pNetlinkScanGetResults (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    )

/*++

Routine Description:

    This routine gets the results from the latest scan, packages them up as
    a netlink message and sends them back to the caller.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    Command - Supplies a pointer to the command information.

Return Value:

    Status code.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    ULONG BssCount;
    ULONG BssLength;
    ULONG BssStatus;
    PLIST_ENTRY CurrentEntry;
    DEVICE_ID DeviceId;
    PNET80211_LINK Link;
    ULONG ResultLength;
    PNET_PACKET_BUFFER Results;
    ULONG ResultsLength;
    LONG SignalMbm;
    KSTATUS Status;

    //
    // Parse the packet to find the 802.11 link whose scan results are to be
    // queried.
    //

    Status = Net80211pNetlinkGetLink(Packet, &Link);
    if (!KSUCCESS(Status)) {
        goto NetlinkScanGetResultsEnd;
    }

    //
    // Determine the required size of the BSS replies.
    //

    BssCount = 0;
    ResultsLength = 0;
    KeAcquireQueuedLock(Link->Lock);
    CurrentEntry = Link->BssList.Next;
    while (CurrentEntry != &(Link->BssList)) {
        Bss = LIST_VALUE(CurrentEntry, NET80211_BSS_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        ResultsLength += NETLINK_HEADER_LENGTH + NETLINK_GENERIC_HEADER_LENGTH;
        ResultsLength += NETLINK_ATTRIBUTE_SIZE(sizeof(DEVICE_ID));
        BssLength = NETLINK_ATTRIBUTE_SIZE(NET80211_ADDRESS_SIZE) +
                    NETLINK_ATTRIBUTE_SIZE(sizeof(Bss->State.Capabilities)) +
                    NETLINK_ATTRIBUTE_SIZE(sizeof(Bss->State.BeaconInterval)) +
                    NETLINK_ATTRIBUTE_SIZE(sizeof(BssStatus)) +
                    NETLINK_ATTRIBUTE_SIZE(sizeof(SignalMbm)) +
                    NETLINK_ATTRIBUTE_SIZE(Bss->ElementsSize);

        ResultsLength += NETLINK_ATTRIBUTE_SIZE(BssLength);
        BssCount += 1;
    }

    KeReleaseQueuedLock(Link->Lock);

    //
    // At least always send a done message.
    //

    ResultsLength += NETLINK_HEADER_LENGTH;

    //
    // Allocate the network packet buffer to hold all of the results.
    //

    Status = NetAllocateBuffer(0, ResultsLength, 0, NULL, 0, &Results);
    if (!KSUCCESS(Status)) {
        goto NetlinkScanGetResultsEnd;
    }

    //
    // Package up the BSS entry list into multiple netlink scan result messages.
    // If new entries arrived since the lock was held before, they came in due
    // to an additional scan. A future scan results request will package them
    // up.
    //

    DeviceId = IoGetDeviceNumericId(Link->Properties.Device);
    KeAcquireQueuedLock(Link->Lock);
    CurrentEntry = Link->BssList.Next;
    while ((CurrentEntry != &(Link->BssList)) && (BssCount != 0)) {
        Bss = LIST_VALUE(CurrentEntry, NET80211_BSS_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Determine the length of the entire message.
        //

        ResultLength = NETLINK_ATTRIBUTE_SIZE(sizeof(DEVICE_ID));
        BssLength = NETLINK_ATTRIBUTE_SIZE(NET80211_ADDRESS_SIZE) +
                    NETLINK_ATTRIBUTE_SIZE(sizeof(Bss->State.Capabilities)) +
                    NETLINK_ATTRIBUTE_SIZE(sizeof(Bss->State.BeaconInterval)) +
                    NETLINK_ATTRIBUTE_SIZE(sizeof(BssStatus)) +
                    NETLINK_ATTRIBUTE_SIZE(sizeof(SignalMbm)) +
                    NETLINK_ATTRIBUTE_SIZE(Bss->ElementsSize);

        ResultLength += NETLINK_ATTRIBUTE_SIZE(BssLength);

        //
        // Add a generic and base header for this entry.
        //

        Status = NetlinkGenericAppendHeaders(Net80211NetlinkFamily,
                                             Results,
                                             ResultLength,
                                             Command->Message.SequenceNumber,
                                             NETLINK_HEADER_FLAG_MULTIPART,
                                             NETLINK_80211_COMMAND_SCAN_RESULT,
                                             0);

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        //
        // Add the attributes.
        //

        Status = NetlinkAppendAttribute(Results,
                                        NETLINK_80211_ATTRIBUTE_DEVICE_ID,
                                        &DeviceId,
                                        sizeof(DEVICE_ID));

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        Status = NetlinkAppendAttribute(Results,
                                        NETLINK_80211_ATTRIBUTE_BSS,
                                        NULL,
                                        BssLength);

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        Status = NetlinkAppendAttribute(Results,
                                        NETLINK_80211_BSS_ATTRIBUTE_BSSID,
                                        Bss->State.Bssid,
                                        NET80211_ADDRESS_SIZE);

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        Status = NetlinkAppendAttribute(Results,
                                        NETLINK_80211_BSS_ATTRIBUTE_CAPABILITY,
                                        &(Bss->State.Capabilities),
                                        sizeof(Bss->State.Capabilities));

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        Status = NetlinkAppendAttribute(
                                   Results,
                                   NETLINK_80211_BSS_ATTRIBUTE_BEACON_INTERVAL,
                                   &(Bss->State.BeaconInterval),
                                   sizeof(Bss->State.BeaconInterval));

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        SignalMbm = Bss->State.Rssi * 100;
        Status = NetlinkAppendAttribute(Results,
                                        NETLINK_80211_BSS_ATTRIBUTE_SIGNAL_MBM,
                                        &SignalMbm,
                                        sizeof(SignalMbm));

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        BssStatus = NETLINK_80211_BSS_STATUS_NOT_CONNECTED;
        if (Bss == Link->ActiveBss) {
            switch (Link->State) {
            case Net80211StateAssociated:
            case Net80211StateEncrypted:
                Status = NETLINK_80211_BSS_STATUS_ASSOCIATED;
                break;

            case Net80211StateAssociating:
                Status = NETLINK_80211_BSS_STATUS_AUTHENTICATED;
                break;

            default:
                break;
            }
        }

        Status = NetlinkAppendAttribute(Results,
                                        NETLINK_80211_BSS_ATTRIBUTE_STATUS,
                                        &BssStatus,
                                        sizeof(BssStatus));

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        Status = NetlinkAppendAttribute(
                              Results,
                              NETLINK_80211_BSS_ATTRIBUTE_INFORMATION_ELEMENTS,
                              Bss->Elements,
                              Bss->ElementsSize);

        if (!KSUCCESS(Status)) {
            goto NetlinkScanGetResultsEnd;
        }

        BssCount -= 1;
    }

    KeReleaseQueuedLock(Link->Lock);

    //
    // Send this multipart message back to the source of the request. This
    // routine will add the terminating DONE message and then send the entire
    // set of messages in the results packet.
    //

    Status = NetlinkSendMultipartMessage(Socket,
                                         Results,
                                         Command->Message.SourceAddress,
                                         Command->Message.SequenceNumber);

    if (!KSUCCESS(Status)) {
        goto NetlinkScanGetResultsEnd;
    }

NetlinkScanGetResultsEnd:
    if (Link != NULL) {
        Net80211LinkReleaseReference(Link);
    }

    return Status;
}

KSTATUS
Net80211pNetlinkGetLink (
    PNET_PACKET_BUFFER Packet,
    PNET80211_LINK *Link
    )

/*++

Routine Description:

    This routine parses the given 802.11 netlink message packet for the device
    ID attribute and then looks for the corresponding 802.11 link. If found, a
    reference is taken on the the 802.11 link and it is the caller's
    responsibility to release the reference.

Arguments:

    Packet - Supplies a pointer to the netlink message to parse for the device
        ID or order to determine the target 802.11 link.

    Link - Supplies a pointer that receives a pointer an 802.11 network link.

Return Value:

    Status code.

--*/

{

    PVOID Attributes;
    ULONG AttributesLength;
    PDEVICE Device;
    PVOID DeviceId;
    USHORT DeviceIdLength;
    PNET80211_LINK Net80211Link;
    PNET_LINK NetLink;
    KSTATUS Status;

    Device = NULL;
    NetLink = NULL;
    Net80211Link = NULL;

    //
    // Get the device ID. It is necessary to find the appropriate link.
    //

    Attributes = Packet->Buffer + Packet->DataOffset;
    AttributesLength = Packet->FooterOffset - Packet->DataOffset;
    Status = NetlinkGetAttribute(Attributes,
                                 AttributesLength,
                                 NETLINK_80211_ATTRIBUTE_DEVICE_ID,
                                 &DeviceId,
                                 &DeviceIdLength);

    if (!KSUCCESS(Status)) {
        goto NetlinkGetLinkEnd;
    }

    if (DeviceIdLength != sizeof(DEVICE_ID)) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto NetlinkGetLinkEnd;
    }

    Device = IoGetDeviceByNumericId(*((PDEVICE_ID)DeviceId));
    if (Device == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto NetlinkGetLinkEnd;
    }

    Status = NetLookupLinkByDevice(Device, &NetLink);
    if (!KSUCCESS(Status)) {
        goto NetlinkGetLinkEnd;
    }

    //
    // If the link is not an 802.11 type then nothing can be done.
    //

    if (NetLink->Properties.DataLinkType != NetDomain80211) {
        Status = STATUS_NOT_SUPPORTED;
        goto NetlinkGetLinkEnd;
    }

    //
    // Setting the link state to initialized will deactivate the current
    // connection and send the appropriate deactivation messages to the access
    // point.
    //

    Net80211Link = NetLink->DataLinkContext;
    Net80211LinkAddReference(Net80211Link);

NetlinkGetLinkEnd:
    if (Device != NULL) {
        IoDeviceReleaseReference(Device);
    }

    if (NetLink != NULL) {
        NetLinkReleaseReference(NetLink);
    }

    *Link = Net80211Link;
    return Status;
}

VOID
Net80211pNetlinkSendScanNotification (
    PNET80211_LINK Link,
    UCHAR Command
    )

/*++

Routine Description:

    This routine notifies the scan multicast group about a scan's progress.

Arguments:

    Link - Supplies a pointer to the 802.11 link that performed the scan.

    Command - Supplies the 802.11 netlink command to send to the scan multicast
        group.

Return Value:

    None.

--*/

{

    DEVICE_ID DeviceId;
    ULONG HeaderSize;
    PNET_PACKET_BUFFER Packet;
    ULONG PayloadSize;
    ULONG Size;
    KSTATUS Status;

    //
    // Allocate and build a network buffer to hold the scan properties.
    //

    Packet = NULL;
    HeaderSize = NETLINK_HEADER_LENGTH + NETLINK_GENERIC_HEADER_LENGTH;
    PayloadSize = NETLINK_ATTRIBUTE_SIZE(sizeof(DEVICE_ID));
    Size = HeaderSize + PayloadSize;
    Status = NetAllocateBuffer(0,
                               Size,
                               0,
                               NULL,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto NetlinkSendScanNotification;
    }

    Status = NetlinkGenericAppendHeaders(Net80211NetlinkFamily,
                                         Packet,
                                         PayloadSize,
                                         0,
                                         0,
                                         Command,
                                         0);

    if (!KSUCCESS(Status)) {
        goto NetlinkSendScanNotification;
    }

    DeviceId = IoGetDeviceNumericId(Link->Properties.Device);
    Status = NetlinkAppendAttribute(Packet,
                                    NETLINK_80211_ATTRIBUTE_DEVICE_ID,
                                    &DeviceId,
                                    sizeof(DEVICE_ID));

    if (!KSUCCESS(Status)) {
        goto NetlinkSendScanNotification;
    }

    //
    // Send the packet out to the 802.11 scan multicast group.
    //

    Status = NetlinkGenericSendMulticastCommand(
                                         Net80211NetlinkFamily,
                                         Packet,
                                         NETLINK_GENERIC_80211_MULTICAST_SCAN);

    if (!KSUCCESS(Status)) {
        goto NetlinkSendScanNotification;
    }

NetlinkSendScanNotification:
    if (Packet != NULL) {
        NetFreeBuffer(Packet);
    }

    return;
}

