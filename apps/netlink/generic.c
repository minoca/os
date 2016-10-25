/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    generic.c

Abstract:

    This module implements the generic netlink library subsystem.

Author:

    Chris Stevens 25-Mar-2016

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "netlinkp.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context used for parsing a new family netlink
    message for a multicast group ID based on the given group name.

Members:

    GroupName - Stores the name of the group whose ID is being queried.

    GroupId - Stores the group ID that corresponds to the group name.

--*/

typedef struct _NL_GENERIC_GROUP_ID_CONTEXT {
    PSTR GroupName;
    INT GroupId;
} NL_GENERIC_GROUP_ID_CONTEXT, *PNL_GENERIC_GROUP_ID_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
NlpGenericSendGetFamilyCommand (
    PNL_SOCKET Socket,
    PUSHORT FamilyId,
    PSTR FamilyName
    );

VOID
NlpGenericParseFamilyId (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    );

VOID
NlpGenericParseGroupId (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBNETLINK_API
INT
NlGenericAppendHeaders (
    PNL_SOCKET Socket,
    PNL_MESSAGE_BUFFER Message,
    ULONG PayloadLength,
    ULONG SequenceNumber,
    USHORT Type,
    USHORT Flags,
    UCHAR Command,
    UCHAR Version
    )

/*++

Routine Description:

    This routine appends the base and generic netlink headers to the given
    message, validating that there is enough space remaining in the buffer.
    Once the headers are appended, it moves the buffer's offset to the first
    byte after the headers and updates the valid data size.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which the message
        will be sent.

    Message - Supplies a pointer to the netlink message buffer to which the
        headers will be appended.

    PayloadLength - Supplies the length of the message data payload, in bytes.
        This does not include the header lengths.

    SequenceNumber - Supplies the sequence number for the message. This value
        is ignored unless NL_SOCKET_FLAG_NO_AUTO_SEQUENCE is set in the socket.

    Type - Supplies the netlink message type.

    Flags - Supplies a bitmask of netlink message flags. See
        NETLINK_HEADER_FLAG_* for definitions.

    Command - Supplies the generic message command.

    Version - Supplies the version of the generic message command.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PNETLINK_GENERIC_HEADER Header;
    ULONG MessageLength;
    INT Status;

    PayloadLength += NETLINK_GENERIC_HEADER_LENGTH;
    Status = NlAppendHeader(Socket,
                            Message,
                            PayloadLength,
                            SequenceNumber,
                            Type,
                            Flags);

    if (Status != 0) {
        return Status;
    }

    MessageLength = Message->BufferSize - Message->CurrentOffset;
    if (MessageLength < PayloadLength) {
        errno = ENOBUFS;
        return -1;
    }

    Header = Message->Buffer + Message->CurrentOffset;
    Header->Command = Command;
    Header->Version = Version;
    Header->Reserved = 0;

    //
    // Move the offset and data size to the first byte after the header.
    //

    Message->CurrentOffset += NETLINK_GENERIC_HEADER_LENGTH;
    Message->DataSize += NETLINK_GENERIC_HEADER_LENGTH;
    return 0;
}

LIBNETLINK_API
INT
NlGenericGetFamilyId (
    PNL_SOCKET Socket,
    PSTR FamilyName,
    PUSHORT FamilyId
    )

/*++

Routine Description:

    This routine queries the system for a message family ID, which is dynamic,
    using a well-known messsage family name.

Arguments:

    Socket - Supplies a pointer to the netlink socket to use to send the
        generic message.

    FamilyName - Supplies the family name string to use for looking up the type.

    FamilyId - Supplies a pointer that receives the message family ID to use as
        the netlink message header type.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NL_RECEIVE_PARAMETERS Parameters;
    INT Status;

    Status = NlpGenericSendGetFamilyCommand(Socket, NULL, FamilyName);
    if (Status != 0) {
        goto GetFamilyIdEnd;
    }

    //
    // Attempt to receive a new family message and parse it for the family ID.
    //

    Parameters.ReceiveRoutine = NlpGenericParseFamilyId;
    Parameters.ReceiveContext.Status = 0;
    Parameters.ReceiveContext.Type = NETLINK_GENERIC_ID_CONTROL;
    Parameters.ReceiveContext.PrivateContext = FamilyId;
    Parameters.Flags = NL_RECEIVE_FLAG_PORT_ID;
    Parameters.PortId = NETLINK_KERNEL_PORT_ID;
    Status = NlReceiveMessage(Socket, &Parameters);
    if (Status != 0) {
        goto GetFamilyIdEnd;
    }

    Status = Parameters.ReceiveContext.Status;

GetFamilyIdEnd:
    return Status;
}

LIBNETLINK_API
INT
NlGenericJoinMulticastGroup (
    PNL_SOCKET Socket,
    USHORT FamilyId,
    PSTR GroupName
    )

/*++

Routine Description:

    This routine joins the given socket to the multicast group specified by the
    family ID and group name.

Arguments:

    Socket - Supplies a pointer to the netlink socket requesting to join a
        multicast group.

    FamilyId - Supplies the ID of the family to which the multicast group
        belongs.

    GroupName - Supplies the name of the multicast group to join.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    NL_GENERIC_GROUP_ID_CONTEXT GroupContext;
    NL_RECEIVE_PARAMETERS Parameters;
    INT Status;

    Status = NlpGenericSendGetFamilyCommand(Socket, &FamilyId, NULL);
    if (Status != 0) {
        goto JoinMulticastGroupEnd;
    }

    //
    // Attempt to receive a new family message and parse it for the group ID.
    //

    GroupContext.GroupName = GroupName;
    Parameters.ReceiveRoutine = NlpGenericParseGroupId;
    Parameters.ReceiveContext.Status = 0;
    Parameters.ReceiveContext.Type = NETLINK_GENERIC_ID_CONTROL;
    Parameters.ReceiveContext.PrivateContext = &GroupContext;
    Parameters.Flags = NL_RECEIVE_FLAG_PORT_ID;
    Parameters.PortId = NETLINK_KERNEL_PORT_ID;
    Status = NlReceiveMessage(Socket, &Parameters);
    if (Status != 0) {
        goto JoinMulticastGroupEnd;
    }

    Status = Parameters.ReceiveContext.Status;
    if (Status != 0) {
        goto JoinMulticastGroupEnd;
    }

    //
    // Now that the group ID is identified. Join it!
    //

    Status = setsockopt(Socket->Socket,
                        SOL_NETLINK,
                        NETLINK_ADD_MEMBERSHIP,
                        &(GroupContext.GroupId),
                        sizeof(INT));

    if (Status != 0) {
        goto JoinMulticastGroupEnd;
    }

JoinMulticastGroupEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
NlpGenericSendGetFamilyCommand (
    PNL_SOCKET Socket,
    PUSHORT FamilyId,
    PSTR FamilyName
    )

/*++

Routine Description:

    This routine sends a get family command on the given socket, querying for
    the family identified by the given ID and/or name.

Arguments:

    Socket - Supplies a pointer to a netlink socket.

    FamilyId - Supplies an optional pointer to the ID of the family whose
        information is being queried.

    FamilyName - Supplies an optional string specifying the name of the family
        whose information is being queried.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    size_t FamilyNameLength;
    PNL_MESSAGE_BUFFER Message;
    ULONG MessageLength;
    ULONG PayloadLength;
    INT Status;

    Message = NULL;
    if (Socket->Protocol != NETLINK_GENERIC) {
        errno = EINVAL;
        Status = -1;
        goto SendGetFamilyCommandEnd;
    }

    if ((FamilyId == NULL) && (FamilyName == NULL)) {
        errno = EINVAL;
        Status = -1;
        goto SendGetFamilyCommandEnd;
    }

    //
    // Build and send a request for the given family.
    //

    if (FamilyId != NULL) {
        PayloadLength = NETLINK_ATTRIBUTE_SIZE(sizeof(USHORT));

    } else {

        ASSERT(FamilyName != NULL);

        FamilyNameLength = strlen(FamilyName) + 1;
        PayloadLength = NETLINK_ATTRIBUTE_SIZE(FamilyNameLength);
    }

    MessageLength = NETLINK_GENERIC_HEADER_LENGTH + PayloadLength;
    Status = NlAllocateBuffer(MessageLength, &Message);
    if (Status != 0) {
        goto SendGetFamilyCommandEnd;
    }

    Status = NlGenericAppendHeaders(Socket,
                                    Message,
                                    PayloadLength,
                                    0,
                                    NETLINK_GENERIC_ID_CONTROL,
                                    0,
                                    NETLINK_CONTROL_COMMAND_GET_FAMILY,
                                    0);

    if (Status != 0) {
        goto SendGetFamilyCommandEnd;
    }

    if (FamilyId != NULL) {
        Status = NlAppendAttribute(Message,
                                   NETLINK_CONTROL_ATTRIBUTE_FAMILY_ID,
                                   FamilyId,
                                   sizeof(USHORT));

    } else {

        ASSERT(FamilyName != NULL);

        Status = NlAppendAttribute(Message,
                                   NETLINK_CONTROL_ATTRIBUTE_FAMILY_NAME,
                                   FamilyName,
                                   FamilyNameLength);
    }

    if (Status != 0) {
        goto SendGetFamilyCommandEnd;
    }

    Status = NlSendMessage(Socket, Message, NETLINK_KERNEL_PORT_ID, 0, NULL);
    if (Status != 0) {
        goto SendGetFamilyCommandEnd;
    }

SendGetFamilyCommandEnd:
    if (Message != NULL) {
        NlFreeBuffer(Message);
    }

    return Status;
}

VOID
NlpGenericParseFamilyId (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    )

/*++

Routine Description:

    This routine parses a netlink message for a family ID attribute.

Arguments:

    Socket - Supplies a pointer to the netlink socket that received the message.

    Context - Supplies a pointer to the receive context given to the receive
        message handler.

    Message - Supplies a pointer to the beginning of the netlink message. The
        length of which can be obtained from the header; it was already
        validated.

Return Value:

    None.

--*/

{

    PVOID Attributes;
    PUSHORT FamilyId;
    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_HEADER Header;
    PUSHORT Id;
    USHORT IdLength;
    ULONG MessageLength;
    INT Status;

    FamilyId = (PUSHORT)Context->PrivateContext;
    Header = (PNETLINK_HEADER)Message;
    if (Header->Type != Context->Type) {
        errno = ENOMSG;
        Status = -1;
        goto ParseFamilyIdEnd;
    }

    MessageLength = Header->Length - NETLINK_HEADER_LENGTH;
    GenericHeader = NETLINK_DATA(Header);
    if ((MessageLength < NETLINK_GENERIC_HEADER_LENGTH) ||
        (GenericHeader->Command != NETLINK_CONTROL_COMMAND_NEW_FAMILY)) {

        errno = ENOMSG;
        Status = -1;
        goto ParseFamilyIdEnd;
    }

    MessageLength -= NETLINK_GENERIC_HEADER_LENGTH;
    Attributes = NETLINK_GENERIC_DATA(GenericHeader);
    Status = NlGetAttribute(Attributes,
                            MessageLength,
                            NETLINK_CONTROL_ATTRIBUTE_FAMILY_ID,
                            (PVOID *)&Id,
                            &IdLength);

    if (Status != 0) {
        goto ParseFamilyIdEnd;
    }

    if (IdLength != sizeof(USHORT)) {
        goto ParseFamilyIdEnd;
    }

    *FamilyId = *Id;

ParseFamilyIdEnd:
    Context->Status = Status;
    return;
}

VOID
NlpGenericParseGroupId (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    )

/*++

Routine Description:

    This routine parses a netlink message for a multicast group ID attribute.

Arguments:

    Socket - Supplies a pointer to the netlink socket that received the message.

    Context - Supplies a pointer to the receive context given to the receive
        message handler.

    Message - Supplies a pointer to the beginning of the netlink message. The
        length of which can be obtained from the header; it was already
        validated.

Return Value:

    None.

--*/

{

    PVOID Attributes;
    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_ATTRIBUTE Group;
    PNL_GENERIC_GROUP_ID_CONTEXT GroupContext;
    BOOL GroupFound;
    PINT GroupId;
    USHORT GroupIdLength;
    ULONG GroupLength;
    PSTR GroupName;
    ULONG GroupNameLength;
    PVOID Groups;
    USHORT GroupsLength;
    PNETLINK_HEADER Header;
    ULONG MessageLength;
    PSTR Name;
    USHORT NameLength;
    KSTATUS Status;

    Header = (PNETLINK_HEADER)Message;
    GroupContext = (PNL_GENERIC_GROUP_ID_CONTEXT)Context->PrivateContext;
    MessageLength = Header->Length;
    if (Header->Type != Context->Type) {
        errno = ENOMSG;
        Status = -1;
        goto ParseGroupIdEnd;
    }

    MessageLength -= NETLINK_HEADER_LENGTH;
    GenericHeader = NETLINK_DATA(Header);
    if ((MessageLength < NETLINK_GENERIC_HEADER_LENGTH) ||
        (GenericHeader->Command != NETLINK_CONTROL_COMMAND_NEW_FAMILY)) {

        errno = ENOMSG;
        Status = -1;
        goto ParseGroupIdEnd;
    }

    MessageLength -= NETLINK_GENERIC_HEADER_LENGTH;
    Attributes = NETLINK_GENERIC_DATA(GenericHeader);
    Status = NlGetAttribute(Attributes,
                            MessageLength,
                            NETLINK_CONTROL_ATTRIBUTE_MULTICAST_GROUPS,
                            &Groups,
                            &GroupsLength);

    if (Status != 0) {
        goto ParseGroupIdEnd;
    }

    //
    // Search the groups for a multicast name that matches.
    //

    GroupFound = FALSE;
    GroupName = GroupContext->GroupName;
    GroupNameLength = RtlStringLength(GroupName) + 1;
    Group = (PNETLINK_ATTRIBUTE)Groups;
    while (GroupsLength != 0) {
        if (GroupsLength < NETLINK_ATTRIBUTE_HEADER_LENGTH) {
            break;
        }

        GroupLength = Group->Length - NETLINK_ATTRIBUTE_HEADER_LENGTH;
        Status = NlGetAttribute(NETLINK_ATTRIBUTE_DATA(Group),
                                GroupLength,
                                NETLINK_CONTROL_MULTICAST_GROUP_ATTRIBUTE_NAME,
                                (PVOID *)&Name,
                                &NameLength);

        if (Status != 0) {
            break;
        }

        if ((NameLength == GroupNameLength) &&
            (RtlAreStringsEqual(GroupName, Name, NameLength) != FALSE)) {

            Status = NlGetAttribute(
                                  NETLINK_ATTRIBUTE_DATA(Group),
                                  GroupLength,
                                  NETLINK_CONTROL_MULTICAST_GROUP_ATTRIBUTE_ID,
                                  (PVOID *)&GroupId,
                                  &GroupIdLength);

            if (Status != 0) {
                break;
            }

            if (GroupIdLength != sizeof(INT)) {
                break;
            }

            GroupFound = TRUE;
            break;
        }

        ASSERT(GroupsLength >= NETLINK_ATTRIBUTE_SIZE(GroupLength));

        Group = (PVOID)Group + NETLINK_ATTRIBUTE_SIZE(GroupLength);
        GroupsLength -= NETLINK_ATTRIBUTE_SIZE(GroupLength);
    }

    if (GroupFound == FALSE) {
        errno = ENOENT;
        Status = -1;
        goto ParseGroupIdEnd;
    }

    GroupContext->GroupId = *GroupId;

ParseGroupIdEnd:
    Context->Status = Status;
    return;
}

