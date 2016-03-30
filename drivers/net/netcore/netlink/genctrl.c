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
NetpNetlinkGenericControlGetFamily (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
    );

//
// -------------------------------------------------------------------- Globals
//

NETLINK_GENERIC_COMMAND NetNetlinkGenericControlCommands[] = {
    {
        NETLINK_GENERIC_CONTROL_GET_FAMILY,
        NetpNetlinkGenericControlGetFamily
    },
};

NETLINK_GENERIC_FAMILY_PROPERTIES NetNetlinkGenericControlFamily = {
    NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION,
    NETLINK_GENERIC_ID_CONTROL,
    NETLINK_GENERIC_CONTROL_NAME,
    NetNetlinkGenericControlCommands,
    sizeof(NetNetlinkGenericControlCommands) /
    sizeof(NetNetlinkGenericControlCommands[0]),
};

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpNetlinkGenericControlInitialize (
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

    Status = NetNetlinkGenericRegisterFamily(&NetNetlinkGenericControlFamily,
                                             NULL);

    if (!KSUCCESS(Status)) {

        ASSERT(KSUCCESS(Status));

    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
NetpNetlinkGenericControlGetFamily (
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

    PNETLINK_ATTRIBUTE Attribute;
    ULONG AttributeLength;
    PVOID Data;
    ULONG DataLength;
    PNETLINK_GENERIC_FAMILY Family;
    USHORT FamilyId;
    PSTR FamilyName;
    ULONG FamilyNameLength;
    ULONG HeaderLength;
    ULONG PacketLength;
    UINTN ReplyLength;
    PNET_PACKET_BUFFER ReplyPacket;
    NETLINK_GENERIC_COMMAND_PARAMETERS SendParameters;
    KSTATUS Status;

    ReplyPacket = NULL;

    //
    // Search the packet for an attribute that identifies the family.
    //

    PacketLength = Packet->FooterOffset - Packet->DataOffset;
    Attribute = (PNETLINK_ATTRIBUTE)(Packet->Buffer + Packet->DataOffset);

    ASSERT(IS_POINTER_ALIGNED(Attribute, NETLINK_ATTRIBUTE_ALIGNMENT) != FALSE);

    Family = NULL;
    while ((PacketLength != 0) && (Family == NULL)) {
        if ((PacketLength < NETLINK_ATTRIBUTE_HEADER_LENGTH) ||
            (PacketLength < Attribute->Length)) {

            break;
        }

        Data = NETLINK_ATTRIBUTE_DATA(Attribute);
        DataLength = Attribute->Length - NETLINK_ATTRIBUTE_HEADER_LENGTH;
        switch (Attribute->Type) {
        case NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_ID:
            if (DataLength != sizeof(USHORT)) {
                break;
            }

            FamilyId = *(PUSHORT)Data;
            Family = NetpNetlinkGenericLookupFamilyById(FamilyId);
            break;

        case NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_NAME:
            FamilyName = (PSTR)Data;
            Family = NetpNetlinkGenericLookupFamilyByName(FamilyName);
            break;

        default:
            break;
        }

        AttributeLength = NETLINK_ALIGN(Attribute->Length);
        if (AttributeLength > PacketLength) {
            break;
        }

        Attribute = (PVOID)Attribute + AttributeLength;
        PacketLength -= AttributeLength;
    }

    if (Family == NULL) {
        Status = STATUS_NOT_FOUND;
        goto GetFamilyEnd;
    }

    //
    // A family was found. Fill out the new family message and send it back to
    // the socket that requested the family information.
    //

    FamilyNameLength = RtlStringLength(Family->Properties.Name) + 1;
    HeaderLength = NETLINK_HEADER_LENGTH + NETLINK_GENERIC_HEADER_LENGTH;
    ReplyLength = NETLINK_ATTRIBUTE_SIZE(sizeof(USHORT)) +
                  NETLINK_ATTRIBUTE_SIZE(FamilyNameLength);

    Status = NetAllocateBuffer(HeaderLength,
                               ReplyLength,
                               0,
                               NULL,
                               0,
                               &ReplyPacket);

    if (!KSUCCESS(Status)) {
        goto GetFamilyEnd;
    }

    //
    // Add the family ID and family name attributes to the data portion of the
    // packet.
    //

    Attribute = ReplyPacket->Buffer + ReplyPacket->DataOffset;
    Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(sizeof(USHORT));
    Attribute->Type = NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_ID;
    Data = NETLINK_ATTRIBUTE_DATA(Attribute);
    *((PUSHORT)Data) = Family->Properties.Id;
    Attribute = (PVOID)Attribute + NETLINK_ATTRIBUTE_SIZE(sizeof(USHORT));
    Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(FamilyNameLength);
    Attribute->Type = NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_NAME;
    RtlStringCopy(NETLINK_ATTRIBUTE_DATA(Attribute),
                  Family->Properties.Name,
                  FamilyNameLength);

    //
    // Send the packet off to the generic netlink protocol to be sent.
    //

    SendParameters.Message.SourceAddress = &(Socket->LocalAddress);
    SendParameters.Message.DestinationAddress =
                                             Parameters->Message.SourceAddress;

    SendParameters.Message.SequenceNumber = Parameters->Message.SequenceNumber;
    SendParameters.Message.Type = NETLINK_GENERIC_ID_CONTROL;
    SendParameters.Command = NETLINK_GENERIC_CONTROL_NEW_FAMILY;
    SendParameters.Version = 0;
    Status = NetNetlinkGenericSendCommand(Socket, ReplyPacket, &SendParameters);
    if (!KSUCCESS(Status)) {
        goto GetFamilyEnd;
    }

GetFamilyEnd:
    if (Family != NULL) {
        NetpNetlinkGenericFamilyReleaseReference(Family);
    }

    if (ReplyPacket != NULL) {
        NetFreeBuffer(ReplyPacket);
    }

    return Status;
}

