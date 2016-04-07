/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
    );

KSTATUS
Net80211pNetlinkLeave (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
    );

KSTATUS
Net80211pNetlinkScanStart (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
    );

VOID
Net80211pNetlinkScanCompletionRoutine (
    PNET80211_LINK Link,
    KSTATUS ScanStatus
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
        NETLINK_GENERIC_80211_JOIN,
        Net80211pNetlinkJoin
    },

    {
        NETLINK_GENERIC_80211_LEAVE,
        Net80211pNetlinkLeave
    },

    {
        NETLINK_GENERIC_80211_SCAN_START,
        Net80211pNetlinkScanStart
    },

};

NETLINK_GENERIC_MULTICAST_GROUP Net80211NetlinkMulticastGroups[] = {
    {
        NETLINK_GENERIC_80211_MULTICAST_SCAN,
        sizeof(NETLINK_GENERIC_80211_MULTICAST_SCAN_NAME),
        NETLINK_GENERIC_80211_MULTICAST_SCAN_NAME
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
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
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

    Parameters - Supplies a pointer to the command parameters.

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
    Status = NetlinkGenericGetAttribute(Attributes,
                                        AttributesLength,
                                        NETLINK_GENERIC_80211_ATTRIBUTE_SSID,
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

    Status = NetlinkGenericGetAttribute(
                                    Attributes,
                                    AttributesLength,
                                    NETLINK_GENERIC_80211_ATTRIBUTE_PASSPHRASE,
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

    Status = NetlinkGenericGetAttribute(Attributes,
                                        AttributesLength,
                                        NETLINK_GENERIC_80211_ATTRIBUTE_BSSID,
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
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
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

    Parameters - Supplies a pointer to the command parameters.

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
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
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

    Parameters - Supplies a pointer to the command parameters.

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
    ScanParameters.Flags = NET80211_SCAN_FLAG_BACKGROUND |
                           NET80211_SCAN_FLAG_BROADCAST;

    ScanParameters.CompletionRoutine = Net80211pNetlinkScanCompletionRoutine;
    Status = Net80211pStartScan(Link, &ScanParameters);
    if (!KSUCCESS(Status)) {
        goto NetlinkScanStartEnd;
    }

    //
    // Notify the scan multicast group that this scan is starting.
    //

    Net80211pNetlinkSendScanNotification(Link,
                                         NETLINK_GENERIC_80211_SCAN_START);

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

    Command = NETLINK_GENERIC_80211_SCAN_RESULT;
    if (!KSUCCESS(ScanStatus)) {
        Command = NETLINK_GENERIC_80211_SCAN_ABORTED;
    }

    Net80211pNetlinkSendScanNotification(Link, Command);
    return;
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
    Status = NetlinkGenericGetAttribute(
                                     Attributes,
                                     AttributesLength,
                                     NETLINK_GENERIC_80211_ATTRIBUTE_DEVICE_ID,
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

    PNETLINK_ATTRIBUTE Attribute;
    PVOID Data;
    ULONG HeaderSize;
    PNET_PACKET_BUFFER Packet;
    ULONG Size;
    KSTATUS Status;

    //
    // Allocate and build a network buffer to hold the scan properties.
    //

    Packet = NULL;
    Size = NETLINK_ATTRIBUTE_SIZE(sizeof(DEVICE_ID));
    HeaderSize = NETLINK_HEADER_LENGTH + NETLINK_GENERIC_HEADER_LENGTH;
    Status = NetAllocateBuffer(HeaderSize,
                               Size,
                               0,
                               NULL,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto NetlinkSendScanNotification;
    }

    Attribute = Packet->Buffer + Packet->DataOffset;
    Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(sizeof(DEVICE_ID));
    Attribute->Type = NETLINK_GENERIC_80211_ATTRIBUTE_DEVICE_ID;
    Data = NETLINK_ATTRIBUTE_DATA(Attribute);
    *((PDEVICE_ID)Data) = IoGetDeviceNumericId(Link->Properties.Device);

    //
    // Send the packet out to the 802.11 scan multicast group.
    //

    Status = NetlinkGenericSendMulticastCommand(
                                          Net80211NetlinkFamily,
                                          Packet,
                                          NETLINK_GENERIC_80211_MULTICAST_SCAN,
                                          Command);

    if (!KSUCCESS(Status)) {
        goto NetlinkSendScanNotification;
    }

NetlinkSendScanNotification:
    if (Packet != NULL) {
        NetFreeBuffer(Packet);
    }

    return;
}

