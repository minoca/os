/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    genctrl.c

Abstract:

    This module implements the generic netlink control family message handling.

Author:

    Chris Stevens 10-Feb-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Typically, generic netlink families are supposed to be able to stand on
// their own (i.e. be able to be implemented outside the core net library). For
// the built-in ones, avoid including netcore.h, but still redefine those
// functions that would otherwise generate imports. This, however, needs to
// include generic.h to get access to the tree of families.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/netlink.h>
#include "generic.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the multicast group indices. These must match the order of the
// multicast group array below.
//

#define NETLINK_GENERIC_CONTROL_MULTICAST_NOTIFY 0

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetlinkpGenericControlGetFamily (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    );

KSTATUS
NetlinkpGenericControlSendCommand (
    PNETLINK_GENERIC_FAMILY Family,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command,
    PNETLINK_GENERIC_MULTICAST_GROUP Group
    );

//
// -------------------------------------------------------------------- Globals
//

NETLINK_GENERIC_COMMAND NetlinkGenericControlCommands[] = {
    {
        NETLINK_CONTROL_COMMAND_GET_FAMILY,
        0,
        NetlinkpGenericControlGetFamily
    },
};

NETLINK_GENERIC_MULTICAST_GROUP NetlinkGenericControlMulticastGroups[] = {
    {
        NETLINK_GENERIC_CONTROL_MULTICAST_NOTIFY,
        sizeof(NETLINK_CONTROL_MULTICAST_NOTIFY_NAME),
        NETLINK_CONTROL_MULTICAST_NOTIFY_NAME
    },
};

NETLINK_GENERIC_FAMILY_PROPERTIES NetlinkGenericControlFamilyProperties = {
    NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION,
    NETLINK_GENERIC_ID_CONTROL,
    sizeof(NETLINK_GENERIC_CONTROL_NAME),
    NETLINK_GENERIC_CONTROL_NAME,
    NetlinkGenericControlCommands,
    sizeof(NetlinkGenericControlCommands) /
        sizeof(NetlinkGenericControlCommands[0]),

    NetlinkGenericControlMulticastGroups,
    sizeof(NetlinkGenericControlMulticastGroups) /
        sizeof(NetlinkGenericControlMulticastGroups[0]),
};

//
// Store a pointer to the netlink generic control family for easy access to
// its multicast groups.
//

PNETLINK_GENERIC_FAMILY NetlinkGenericControlFamily = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetlinkpGenericControlInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the built in generic netlink control family.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    Status = NetlinkGenericRegisterFamily(
                                        &NetlinkGenericControlFamilyProperties,
                                        &NetlinkGenericControlFamily);

    if (!KSUCCESS(Status)) {

        ASSERT(KSUCCESS(Status));

    }

    return;
}

KSTATUS
NetlinkpGenericControlSendNotification (
    PNETLINK_GENERIC_FAMILY Family,
    UCHAR Command,
    PNETLINK_GENERIC_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine sends a generic netlink control command based on the family
    and or group information.

Arguments:

    Family - Supplies a pointer to the generic netlink family for which the
        command is being sent.

    Command - Supplies the generic netlink control command to be sent.

    Group - Supplies an optional pointers to the multicast group that has
        just arrived or is being deleted.

Return Value:

    Status code.

--*/

{

    NETLINK_ADDRESS Destination;
    NETLINK_GENERIC_COMMAND_INFORMATION SendCommand;
    NETLINK_ADDRESS Source;
    KSTATUS Status;

    if (NetlinkGenericControlFamily == NULL) {
        return STATUS_TOO_EARLY;
    }

    //
    // The notifications always come from the kernel.
    //

    Source.Domain = NetDomainNetlink;
    Source.Port = 0;
    Source.Group = 0;

    //
    // Notifications are always sent to the generic netlink control
    // notification multicast group. This is the first multicast group for the
    // control family. As the generic control family has access to the family
    // structure, just do the multicast group offset conversion here so that
    // the same helper routine can send notifications and reply to family
    // information requests.
    //

    Destination.Domain = NetDomainNetlink;
    Destination.Port = 0;
    Destination.Group = NetlinkGenericControlFamily->MulticastGroupOffset +
                        NETLINK_GENERIC_CONTROL_MULTICAST_NOTIFY;

    //
    // Fill out the command information and send out the notification.
    //

    SendCommand.Message.SourceAddress = (PNETWORK_ADDRESS)&Source;
    SendCommand.Message.DestinationAddress = (PNETWORK_ADDRESS)&Destination;
    SendCommand.Message.SequenceNumber = 0;
    SendCommand.Command = Command;
    SendCommand.Version = 0;
    Status = NetlinkpGenericControlSendCommand(Family, &SendCommand, Group);
    if (!KSUCCESS(Status)) {
        goto SendNotificationEnd;
    }

SendNotificationEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
NetlinkpGenericControlGetFamily (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
    )

/*++

Routine Description:

    This routine is called to process a received generic netlink control packet.

Arguments:

    Socket - Supplies a pointer to the network socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    Command - Supplies a pointer to the command information.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    PVOID Attributes;
    ULONG AttributesLength;
    PVOID Data;
    USHORT DataLength;
    PNETLINK_GENERIC_FAMILY Family;
    NETLINK_GENERIC_COMMAND_INFORMATION SendCommand;
    KSTATUS Status;

    Family = NULL;

    //
    // Search the packet for an attribute that identifies the family.
    //

    AttributesLength = Packet->FooterOffset - Packet->DataOffset;
    Attributes = Packet->Buffer + Packet->DataOffset;
    Status = NetlinkGetAttribute(Attributes,
                                 AttributesLength,
                                 NETLINK_CONTROL_ATTRIBUTE_FAMILY_NAME,
                                 &Data,
                                 &DataLength);

    if (KSUCCESS(Status)) {
        Family = NetlinkpGenericLookupFamilyByName((PSTR)Data);
    }

    if (Family == NULL) {
        Status = NetlinkGetAttribute(Attributes,
                                     AttributesLength,
                                     NETLINK_CONTROL_ATTRIBUTE_FAMILY_ID,
                                     &Data,
                                     &DataLength);

        if (KSUCCESS(Status)) {
            if (DataLength != sizeof(USHORT)) {
                Status = STATUS_DATA_LENGTH_MISMATCH;
                goto GetFamilyEnd;
            }

            Family = NetlinkpGenericLookupFamilyById(*((PUSHORT)Data));
        }
    }

    if (Family == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto GetFamilyEnd;
    }

    //
    // A family was found. Send it to the general processing command that
    // handles all family oriented control commands.
    //

    SendCommand.Message.SourceAddress = &(Socket->LocalSendAddress);
    SendCommand.Message.DestinationAddress = Command->Message.SourceAddress;
    SendCommand.Message.SequenceNumber = Command->Message.SequenceNumber;
    SendCommand.Command = NETLINK_CONTROL_COMMAND_NEW_FAMILY;
    SendCommand.Version = 0;
    Status = NetlinkpGenericControlSendCommand(Family, &SendCommand, NULL);
    if (!KSUCCESS(Status)) {
        goto GetFamilyEnd;
    }

GetFamilyEnd:
    if (Family != NULL) {
        NetlinkpGenericFamilyReleaseReference(Family);
    }

    return Status;
}

KSTATUS
NetlinkpGenericControlSendCommand (
    PNETLINK_GENERIC_FAMILY Family,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command,
    PNETLINK_GENERIC_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine sends a generic netlink control command based on the family
    and or group information.

Arguments:

    Family - Supplies a pointer to the generic netlink family for which the
        command is being sent.

    Command - Supplies a pointer to the command information.

    Group - Supplies an optional pointers to the multicast group that has
        just arrived or is being deleted.

Return Value:

    Status code.

--*/

{

    USHORT Attribute;
    ULONG GroupCount;
    PNETLINK_GENERIC_MULTICAST_GROUP Groups;
    ULONG GroupsLength;
    ULONG HeaderLength;
    ULONG IdSize;
    ULONG Index;
    ULONG NameSize;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketLength;
    UINTN PayloadLength;
    KSTATUS Status;

    //
    // Determine the size of the command payload.
    //

    PayloadLength = 0;
    switch (Command->Command) {
    case NETLINK_CONTROL_COMMAND_NEW_FAMILY:
    case NETLINK_CONTROL_COMMAND_DELETE_FAMILY:
        GroupCount = Family->Properties.MulticastGroupCount;
        Groups = Family->Properties.MulticastGroups;
        break;

    case NETLINK_CONTROL_COMMAND_NEW_MULTICAST_GROUP:
    case NETLINK_CONTROL_COMMAND_DELETE_MULTICAST_GROUP:
        if ((Group == NULL) || (Group->Id == 0)) {
            Status = STATUS_INVALID_PARAMETER;
            goto SendCommandEnd;
        }

        GroupCount = 1;
        Groups = Group;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        goto SendCommandEnd;
    }

    //
    // All commands get the family ID and name attributes.
    //

    PayloadLength += NETLINK_ATTRIBUTE_SIZE(sizeof(USHORT)) +
                     NETLINK_ATTRIBUTE_SIZE(Family->Properties.NameLength);

    //
    // Add the size for the multicast groups based on the information
    // determined above. These take advantage of nested attributes.
    //

    if (GroupCount != 0) {
        PayloadLength += NETLINK_ATTRIBUTE_SIZE(0);
        GroupsLength = NETLINK_ATTRIBUTE_SIZE(0) * GroupCount;
        GroupsLength += NETLINK_ATTRIBUTE_SIZE(sizeof(ULONG)) * GroupCount;
        for (Index = 0; Index < GroupCount; Index += 1) {
            Group = &(Groups[Index]);
            GroupsLength += NETLINK_ATTRIBUTE_SIZE(Group->NameLength);
        }

        PayloadLength += GroupsLength;
    }

    HeaderLength = NETLINK_HEADER_LENGTH + NETLINK_GENERIC_HEADER_LENGTH;
    PacketLength = HeaderLength + PayloadLength;
    Status = NetAllocateBuffer(0,
                               PacketLength,
                               0,
                               NULL,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    Status = NetlinkGenericAppendHeaders(NetlinkGenericControlFamily,
                                         Packet,
                                         PayloadLength,
                                         Command->Message.SequenceNumber,
                                         0,
                                         Command->Command,
                                         Command->Version);

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    //
    // Append the family ID and family name attributes to the packet.
    //

    Status = NetlinkAppendAttribute(Packet,
                                    NETLINK_CONTROL_ATTRIBUTE_FAMILY_ID,
                                    &(Family->Properties.Id),
                                    sizeof(USHORT));

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    Status = NetlinkAppendAttribute(Packet,
                                    NETLINK_CONTROL_ATTRIBUTE_FAMILY_NAME,
                                    Family->Properties.Name,
                                    Family->Properties.NameLength);

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    //
    // Append the multicast groups if present.
    //

    if (GroupCount != 0) {
        Attribute = NETLINK_CONTROL_ATTRIBUTE_MULTICAST_GROUPS;
        Status = NetlinkAppendAttribute(Packet, Attribute, NULL, GroupsLength);
        if (!KSUCCESS(Status)) {
            goto SendCommandEnd;
        }

        IdSize = NETLINK_ATTRIBUTE_SIZE(sizeof(ULONG));
        for (Index = 0; Index < GroupCount; Index += 1) {
            Group = &(Groups[Index]);

            ASSERT((Group->Id != 0) && (Group->NameLength != 0));

            NameSize = NETLINK_ATTRIBUTE_SIZE(Group->NameLength);
            Status = NetlinkAppendAttribute(Packet,
                                            Index + 1,
                                            NULL,
                                            IdSize + NameSize);

            if (!KSUCCESS(Status)) {
                goto SendCommandEnd;
            }

            Attribute = NETLINK_CONTROL_MULTICAST_GROUP_ATTRIBUTE_ID;
            Status = NetlinkAppendAttribute(Packet,
                                            Attribute,
                                            &(Group->Id),
                                            sizeof(ULONG));

            if (!KSUCCESS(Status)) {
                goto SendCommandEnd;
            }

            Attribute = NETLINK_CONTROL_MULTICAST_GROUP_ATTRIBUTE_NAME;
            Status = NetlinkAppendAttribute(Packet,
                                            Attribute,
                                            Group->Name,
                                            Group->NameLength);

            if (!KSUCCESS(Status)) {
                goto SendCommandEnd;
            }
        }
    }

    //
    // Actually send the packet out to the address specified in the given
    // parameters.
    //

    Status = NetlinkGenericSendCommand(NetlinkGenericControlFamily,
                                       Packet,
                                       Command->Message.DestinationAddress);

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

SendCommandEnd:
    if (Packet != NULL) {
        NetFreeBuffer(Packet);
    }

    return Status;
}

