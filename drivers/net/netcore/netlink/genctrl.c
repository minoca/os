/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

#define NET_API DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/netlink.h>
#include "generic.h"

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
NetlinkpGenericControlGetFamily (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
    );

KSTATUS
NetlinkpGenericControlSendCommand (
    PNET_SOCKET Socket,
    PNETLINK_GENERIC_FAMILY Family,
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters,
    PNETLINK_GENERIC_MULTICAST_GROUP Group,
    ULONG GroupId
    );

//
// -------------------------------------------------------------------- Globals
//

NETLINK_GENERIC_COMMAND NetlinkGenericControlCommands[] = {
    {
        NETLINK_GENERIC_CONTROL_GET_FAMILY,
        NetlinkpGenericControlGetFamily
    },
};

NETLINK_GENERIC_MULTICAST_GROUP NetlinkGenericControlMulticastGroups[] = {
    {
        sizeof(NETLINK_GENERIC_CONTROL_MULTICAST_NOTIFY_NAME),
        NETLINK_GENERIC_CONTROL_MULTICAST_NOTIFY_NAME
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
    PNET_SOCKET Socket,
    UCHAR Command,
    PNETLINK_GENERIC_FAMILY Family,
    PNETLINK_GENERIC_MULTICAST_GROUP Group,
    ULONG GroupId
    )

/*++

Routine Description:

    This routine sends a generic netlink control command based on the family
    and or group information.

Arguments:

    Socket - Supplies a pointer to the network socket on which to send the
        command.

    Command - Supplies the generic netlink control command to be sent.

    Family - Supplies a pointer to the generic netlink family for which the
        command is being sent.

    Group - Supplies an optional pointers to the multicast group that has
        just arrived or is being deleted.

    GroupId - Supplies an optional ID of the multicast group that has just
        arrived or is being deleted.

Return Value:

    Status code.

--*/

{

    NETLINK_ADDRESS DestinationAddress;
    ULONG Offset;
    NETLINK_GENERIC_COMMAND_PARAMETERS Parameters;
    NETLINK_ADDRESS SourceAddress;
    KSTATUS Status;

    if (NetlinkGenericControlFamily == NULL) {
        return STATUS_TOO_EARLY;
    }

    //
    // The notifications always come from the kernel.
    //

    SourceAddress.Domain = NetDomainNetlink;
    SourceAddress.Port = 0;
    SourceAddress.Group = 0;

    //
    // They are always sent to the generic netlink control notification
    // multicast group. Which is the first multicast group for the control
    // family.
    //

    DestinationAddress.Domain = NetDomainNetlink;
    DestinationAddress.Port = 0;
    Offset = NetlinkGenericControlFamily->MulticastGroupOffset;
    DestinationAddress.Group = Offset;

    //
    // Fill out the command parameters and send out the notification.
    //

    Parameters.Message.SourceAddress = (PNETWORK_ADDRESS)&SourceAddress;
    Parameters.Message.DestinationAddress =
                                         (PNETWORK_ADDRESS)&DestinationAddress;

    Parameters.Message.SequenceNumber = 0;
    Parameters.Message.Type = NETLINK_GENERIC_ID_CONTROL;
    Parameters.Command = Command;
    Parameters.Version = 0;
    Status = NetlinkpGenericControlSendCommand(Socket,
                                               Family,
                                               &Parameters,
                                               Group,
                                               GroupId);

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
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
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

    Parameters - Supplies a pointer to the command parameters.

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
    NETLINK_GENERIC_COMMAND_PARAMETERS SendParameters;
    KSTATUS Status;

    Family = NULL;

    //
    // Search the packet for an attribute that identifies the family.
    //

    AttributesLength = Packet->FooterOffset - Packet->DataOffset;
    Attributes = Packet->Buffer + Packet->DataOffset;
    Status = NetlinkGenericGetAttribute(
                                 Attributes,
                                 AttributesLength,
                                 NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_NAME,
                                 &Data,
                                 &DataLength);

    if (KSUCCESS(Status)) {
        Family = NetlinkpGenericLookupFamilyByName((PSTR)Data);
    }

    if (Family == NULL) {
        Status = NetlinkGenericGetAttribute(
                                   Attributes,
                                   AttributesLength,
                                   NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_ID,
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

    SendParameters.Message.SourceAddress = &(Socket->LocalAddress);
    SendParameters.Message.DestinationAddress =
                                             Parameters->Message.SourceAddress;

    SendParameters.Message.SequenceNumber = Parameters->Message.SequenceNumber;
    SendParameters.Message.Type = NETLINK_GENERIC_ID_CONTROL;
    SendParameters.Command = NETLINK_GENERIC_CONTROL_NEW_FAMILY;
    SendParameters.Version = 0;
    Status = NetlinkpGenericControlSendCommand(Socket,
                                                  Family,
                                                  &SendParameters,
                                                  NULL,
                                                  0);

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
    PNET_SOCKET Socket,
    PNETLINK_GENERIC_FAMILY Family,
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters,
    PNETLINK_GENERIC_MULTICAST_GROUP Group,
    ULONG GroupId
    )

/*++

Routine Description:

    This routine sends a generic netlink control command based on the family
    and or group information.

Arguments:

    Socket - Supplies a pointer to the network socket on which to send the
        command.

    Family - Supplies a pointer to the generic netlink family for which the
        command is being sent.

    Parameters - Supplies a pointer to the command parameters.

    Group - Supplies an optional pointers to the multicast group that has
        just arrived or is being deleted.

    GroupId - Supplies an optional ID of the multicast group that has just
        arrived or is being deleted.

Return Value:

    Status code.

--*/

{

    PNETLINK_ATTRIBUTE Attribute;
    PVOID Data;
    ULONG GroupCount;
    ULONG GroupOffset;
    PNETLINK_GENERIC_MULTICAST_GROUP Groups;
    ULONG GroupsLength;
    ULONG HeaderLength;
    ULONG IdSize;
    ULONG Index;
    ULONG NameSize;
    PNET_PACKET_BUFFER Packet;
    UINTN PacketLength;
    KSTATUS Status;

    //
    // Determine the size of the command payload.
    //

    PacketLength = 0;
    switch (Parameters->Command) {
    case NETLINK_GENERIC_CONTROL_NEW_FAMILY:
    case NETLINK_GENERIC_CONTROL_DELETE_FAMILY:
        GroupCount = Family->Properties.MulticastGroupCount;
        Groups = Family->Properties.MulticastGroups;
        GroupOffset = Family->MulticastGroupOffset;
        break;

    case NETLINK_GENERIC_CONTROL_NEW_MULTICAST_GROUP:
    case NETLINK_GENERIC_CONTROL_DELETE_MULTICAST_GROUP:
        if ((Group == NULL) || (GroupId == 0)) {
            Status = STATUS_INVALID_PARAMETER;
            goto SendCommandEnd;
        }

        GroupCount = 1;
        Groups = Group;
        GroupOffset = GroupId;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        goto SendCommandEnd;
    }

    //
    // All commands get the family ID and name attributes.
    //

    PacketLength += NETLINK_ATTRIBUTE_SIZE(sizeof(USHORT)) +
                    NETLINK_ATTRIBUTE_SIZE(Family->Properties.NameLength);

    //
    // Add the size for the multicast groups based on the information
    // determined above. These take advantage of nested attributes.
    //

    if (GroupCount != 0) {
        PacketLength += NETLINK_ATTRIBUTE_SIZE(0);
        GroupsLength = NETLINK_ATTRIBUTE_SIZE(0) * GroupCount;
        GroupsLength += NETLINK_ATTRIBUTE_SIZE(sizeof(ULONG)) * GroupCount;
        for (Index = 0; Index < GroupCount; Index += 1) {
            Group = &(Groups[Index]);
            GroupsLength += NETLINK_ATTRIBUTE_SIZE(Group->NameLength);
        }

        PacketLength += GroupsLength;
    }

    HeaderLength = NETLINK_HEADER_LENGTH + NETLINK_GENERIC_HEADER_LENGTH;
    Status = NetAllocateBuffer(HeaderLength,
                               PacketLength,
                               0,
                               NULL,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    //
    // Add the family ID and family name attributes to the data portion of the
    // packet.
    //

    Attribute = Packet->Buffer + Packet->DataOffset;
    Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(sizeof(USHORT));
    Attribute->Type = NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_ID;
    Data = NETLINK_ATTRIBUTE_DATA(Attribute);
    *((PUSHORT)Data) = Family->Properties.Id;
    Attribute = (PVOID)Attribute + NETLINK_ATTRIBUTE_SIZE(sizeof(USHORT));
    Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(Family->Properties.NameLength);
    Attribute->Type = NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_NAME;
    RtlStringCopy(NETLINK_ATTRIBUTE_DATA(Attribute),
                  Family->Properties.Name,
                  Family->Properties.NameLength);

    Attribute = (PVOID)Attribute +
                NETLINK_ATTRIBUTE_SIZE(Family->Properties.NameLength);

    //
    // Add the multicast groups.
    //

    if (GroupCount != 0) {
        Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(GroupsLength);
        Attribute->Type = NETLINK_GENERIC_CONTROL_ATTRIBUTE_MULTICAST_GROUPS;
        Attribute = (PVOID)Attribute + NETLINK_ATTRIBUTE_SIZE(0);
        IdSize = NETLINK_ATTRIBUTE_SIZE(sizeof(ULONG));
        for (Index = 0; Index < GroupCount; Index += 1) {
            Group = &(Groups[Index]);
            NameSize = NETLINK_ATTRIBUTE_SIZE(Group->NameLength);
            Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(IdSize + NameSize);
            Attribute->Type = Index + 1;
            Attribute = (PVOID)Attribute + NETLINK_ATTRIBUTE_SIZE(0);
            Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(sizeof(ULONG));
            Attribute->Type = NETLINK_GENERIC_MULTICAST_GROUP_ATTRIBUTE_ID;
            Data = NETLINK_ATTRIBUTE_DATA(Attribute);
            *((PUSHORT)Data) = GroupOffset + Index;
            Attribute = (PVOID)Attribute + IdSize;
            Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(Group->NameLength);
            Attribute->Type = NETLINK_GENERIC_MULTICAST_GROUP_ATTRIBUTE_NAME;
            RtlStringCopy(NETLINK_ATTRIBUTE_DATA(Attribute),
                          Group->Name,
                          Group->NameLength);

            Attribute = (PVOID)Attribute + NameSize;
        }
    }

    //
    // Actually send the packet out to the address specified in the given
    // parameters.
    //

    Parameters->Message.Type = NETLINK_GENERIC_ID_CONTROL;
    Status = NetlinkGenericSendCommand(Socket, Packet, Parameters);
    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

SendCommandEnd:
    if (Packet != NULL) {
        NetFreeBuffer(Packet);
    }

    return Status;
}

