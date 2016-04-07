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
};

NETLINK_GENERIC_MULTICAST_GROUP Net80211NetlinkMulticastGroups[] = {
    {
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

    Status = NetNetlinkGenericRegisterFamily(&Net80211NetlinkFamilyProperties,
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
        NetNetlinkGenericUnregisterFamily(Net80211NetlinkFamily);
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

    This routine is called to process a received generic netlink packet for
    a given command type.

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
    PDEVICE Device;
    PVOID DeviceId;
    USHORT DeviceIdLength;
    PNET80211_LINK Link;
    PNET_LINK NetLink;
    PUCHAR Passphrase;
    USHORT PassphraseLength;
    NET80211_SCAN_STATE ScanParameters;
    PUCHAR Ssid;
    USHORT SsidLength;
    KSTATUS Status;

    Device = NULL;
    NetLink = NULL;

    //
    // Get the device ID. It is necessary to find the appropriate link.
    //

    Attributes = Packet->Buffer + Packet->DataOffset;
    AttributesLength = Packet->FooterOffset - Packet->DataOffset;
    Status = NetNetlinkGenericGetAttribute(
                                     Attributes,
                                     AttributesLength,
                                     NETLINK_GENERIC_80211_ATTRIBUTE_DEVICE_ID,
                                     &DeviceId,
                                     &DeviceIdLength);

    if (!KSUCCESS(Status)) {
        goto NetlinkJoinEnd;
    }

    if (DeviceIdLength != sizeof(DEVICE_ID)) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto NetlinkJoinEnd;
    }

    //
    // An SSID is necessary even if a BSSID is supplied. The user shouldn't
    // join an BSSID that switched SSID's on it.
    //

    Status = NetNetlinkGenericGetAttribute(Attributes,
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

    Status = NetNetlinkGenericGetAttribute(
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

    Status = NetNetlinkGenericGetAttribute(
                                         Attributes,
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

    Device = IoGetDeviceByNumericId(*((PDEVICE_ID)DeviceId));
    if (Device == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto NetlinkJoinEnd;
    }

    Status = NetLookupLinkByDevice(Device, &NetLink);
    if (!KSUCCESS(Status)) {
        goto NetlinkJoinEnd;
    }

    Link = NetLink->DataLinkContext;
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
    if (Device != NULL) {
        IoDeviceReleaseReference(Device);
    }

    if (NetLink != NULL) {
        NetLinkReleaseReference(NetLink);
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

    This routine is called to process a received generic netlink packet for
    a given command type.

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
    PDEVICE Device;
    PVOID DeviceId;
    USHORT DeviceIdLength;
    PNET80211_LINK Link;
    PNET_LINK NetLink;
    KSTATUS Status;

    Device = NULL;
    NetLink = NULL;

    //
    // Get the device ID. It is necessary to find the appropriate link.
    //

    Attributes = Packet->Buffer + Packet->DataOffset;
    AttributesLength = Packet->FooterOffset - Packet->DataOffset;
    Status = NetNetlinkGenericGetAttribute(
                                     Attributes,
                                     AttributesLength,
                                     NETLINK_GENERIC_80211_ATTRIBUTE_DEVICE_ID,
                                     &DeviceId,
                                     &DeviceIdLength);

    if (!KSUCCESS(Status)) {
        goto NetlinkLeaveEnd;
    }

    if (DeviceIdLength != sizeof(DEVICE_ID)) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto NetlinkLeaveEnd;
    }

    Device = IoGetDeviceByNumericId(*((PDEVICE_ID)DeviceId));
    if (Device == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto NetlinkLeaveEnd;
    }

    Status = NetLookupLinkByDevice(Device, &NetLink);
    if (!KSUCCESS(Status)) {
        goto NetlinkLeaveEnd;
    }

    //
    // Setting the link state to initialized will deactivate the current
    // connection and send the appropriate deactivation messages to the access
    // point.
    //

    Link = NetLink->DataLinkContext;
    Net80211pSetState(Link, Net80211StateInitialized);

NetlinkLeaveEnd:
    if (Device != NULL) {
        IoDeviceReleaseReference(Device);
    }

    if (NetLink != NULL) {
        NetLinkReleaseReference(NetLink);
    }

    return Status;
}

