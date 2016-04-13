/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBNETLINK_API
INT
NlGenericFillOutHeader (
    PNETLINK_LIBRARY_SOCKET Socket,
    PNETLINK_MESSAGE_BUFFER Message,
    UCHAR Command,
    UCHAR Version
    )

/*++

Routine Description:

    This routine fills out the generic netlink message header that's going to
    be sent. It will make sure there is enough room left in the supplied
    message buffer and add the header before the current data offset.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which the message
        will be sent.

    Message - Supplies a pointer to the netlink message buffer for which the
        header should be filled out.

    Command - Supplies the generic message command.

    Version - Supplies the version of the generic message command.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PNETLINK_GENERIC_HEADER Header;

    if (Message->DataOffset < NETLINK_GENERIC_HEADER_LENGTH) {
        errno = ENOBUFS;
        return -1;
    }

    Message->DataOffset -= NETLINK_GENERIC_HEADER_LENGTH;
    Header = Message->Buffer + Message->DataOffset;
    Header->Command = Command;
    Header->Version = Version;
    Header->Reserved = 0;
    return 0;
}

LIBNETLINK_API
INT
NlGenericGetFamilyId (
    PNETLINK_LIBRARY_SOCKET Socket,
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

    PVOID Attributes;
    ULONG BytesReceived;
    size_t FamilyNameLength;
    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_HEADER Header;
    PUSHORT Id;
    USHORT IdLength;
    PNETLINK_MESSAGE_BUFFER Message;
    ULONG MessageLength;
    ULONG MessageOffset;
    ULONG PortId;
    INT Status;

    Message = NULL;
    if (Socket->Protocol != NETLINK_GENERIC) {
        errno = EINVAL;
        Status = -1;
        goto GetFamilyIdEnd;
    }

    //
    // Allocate a netlink buffer to send the request.
    //

    FamilyNameLength = strlen(FamilyName) + 1;
    MessageLength = NETLINK_ATTRIBUTE_HEADER_LENGTH + FamilyNameLength;
    MessageLength = NETLINK_ALIGN(MessageLength);
    Status = NlAllocateBuffer(NETLINK_GENERIC_HEADER_LENGTH,
                              MessageLength,
                              0,
                              &Message);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    //
    // Add the family name attribute.
    //

    MessageOffset = 0;
    Status = NlGenericAddAttribute(Message,
                                   &MessageOffset,
                                   NETLINK_CONTROL_ATTRIBUTE_FAMILY_NAME,
                                   FamilyName,
                                   FamilyNameLength);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    //
    // Fill out both the generic and base netlink headers.
    //

    Status = NlGenericFillOutHeader(Socket,
                                    Message,
                                    NETLINK_CONTROL_COMMAND_GET_FAMILY,
                                    0);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    MessageLength += NETLINK_GENERIC_HEADER_LENGTH;
    Status = NlFillOutHeader(Socket,
                             Message,
                             MessageLength,
                             NETLINK_GENERIC_ID_CONTROL,
                             0);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    //
    // Send off the family ID request message.
    //

    Status = NlSendMessage(Socket, Message, NETLINK_KERNEL_PORT_ID, 0, NULL);
    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    //
    // Attempt to receive a new family message.
    //

    Status = NlReceiveMessage(Socket, Socket->ReceiveBuffer, &PortId, NULL);
    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    Header = Socket->ReceiveBuffer->Buffer + Socket->ReceiveBuffer->DataOffset;
    BytesReceived = Socket->ReceiveBuffer->FooterOffset -
                    Socket->ReceiveBuffer->DataOffset;

    if ((PortId != NETLINK_KERNEL_PORT_ID) ||
        (Header->Type != NETLINK_GENERIC_ID_CONTROL)) {

        errno = ENOMSG;
        Status = -1;
        goto GetFamilyIdEnd;
    }

    BytesReceived -= NETLINK_HEADER_LENGTH;
    GenericHeader = NETLINK_DATA(Header);
    if ((BytesReceived < NETLINK_GENERIC_HEADER_LENGTH) ||
        (GenericHeader->Command != NETLINK_CONTROL_COMMAND_NEW_FAMILY)) {

        errno = ENOMSG;
        Status = -1;
        goto GetFamilyIdEnd;
    }

    BytesReceived -= NETLINK_GENERIC_HEADER_LENGTH;
    Attributes = NETLINK_GENERIC_DATA(GenericHeader);
    Status = NlGenericGetAttribute(Attributes,
                                   BytesReceived,
                                   NETLINK_CONTROL_ATTRIBUTE_FAMILY_ID,
                                   (PVOID *)&Id,
                                   &IdLength);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    if (IdLength != sizeof(USHORT)) {
        goto GetFamilyIdEnd;
    }

    *FamilyId = *Id;

    //
    // Receive the ACK message.
    //

    Status = NlReceiveAcknowledgement(Socket,
                                      Socket->ReceiveBuffer,
                                      NETLINK_KERNEL_PORT_ID);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

GetFamilyIdEnd:
    if (Message != NULL) {
        NlFreeBuffer(Message);
    }

    return Status;
}

LIBNETLINK_API
INT
NlGenericAddAttribute (
    PNETLINK_MESSAGE_BUFFER Message,
    PULONG MessageOffset,
    USHORT Type,
    PVOID Data,
    USHORT DataLength
    )

/*++

Routine Description:

    This routine adds a netlink generic attribute to the given netlink message
    buffer, starting at the message buffer's current data offset plus the given
    message offset.

Arguments:

    Message - Supplies a pointer to the netlink message buffer on which to add
        the attribute.

    MessageOffset - Supplies a pointer to the offset within the message's data
        region where the attribute should be stored. On output, it receives the
        updated offset after the added attribute.

    Type - Supplies the netlink generic attribute type.

    Data - Supplies a pointer to the attribute data.

    DataLength - Supplies the length of the data, in bytes.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    ULONG AlignedLength;
    PNETLINK_ATTRIBUTE Attribute;
    USHORT AttributeLength;
    ULONG MessageLength;

    AttributeLength = NETLINK_ATTRIBUTE_LENGTH(DataLength);
    AlignedLength = NETLINK_ATTRIBUTE_SIZE(DataLength);
    MessageLength = Message->FooterOffset - Message->DataOffset;
    if ((*MessageOffset > MessageLength) ||
        (AlignedLength < DataLength) ||
        ((*MessageOffset + AlignedLength) < *MessageOffset) ||
        ((*MessageOffset + AlignedLength) > MessageLength)) {

        errno = EINVAL;
        return -1;
    }

    Attribute = Message->Buffer + Message->DataOffset + *MessageOffset;
    Attribute->Length = AttributeLength;
    Attribute->Type = Type;
    RtlCopyMemory(NETLINK_ATTRIBUTE_DATA(Attribute), Data, DataLength);
    *MessageOffset += AlignedLength;
    return 0;
}

LIBNETLINK_API
INT
NlGenericGetAttribute (
    PVOID Attributes,
    ULONG AttributesLength,
    USHORT Type,
    PVOID *Data,
    PUSHORT DataLength
    )

/*++

Routine Description:

    This routine parses the given attributes buffer and returns a pointer to
    the desired attribute.

Arguments:

    Attributes - Supplies a pointer to the start of the generic command
        attributes.

    AttributesLength - Supplies the length of the attributes buffer, in bytes.

    Type - Supplies the netlink generic attribute type.

    Data - Supplies a pointer that receives a pointer to the data for the
        requested attribute type.

    DataLength - Supplies a pointer that receives the length of the requested
        attribute data.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PNETLINK_ATTRIBUTE Attribute;
    ULONG AttributeSize;

    Attribute = (PNETLINK_ATTRIBUTE)Attributes;
    while (AttributesLength != 0) {
        if ((AttributesLength < NETLINK_ATTRIBUTE_HEADER_LENGTH) ||
            (AttributesLength < Attribute->Length)) {

            break;
        }

        if (Attribute->Type == Type) {
            *DataLength = Attribute->Length - NETLINK_ATTRIBUTE_HEADER_LENGTH;
            *Data = NETLINK_ATTRIBUTE_DATA(Attribute);
            return 0;
        }

        AttributeSize = NETLINK_ALIGN(Attribute->Length);
        Attribute = (PVOID)Attribute + AttributeSize;
        AttributesLength -= AttributeSize;
    }

    errno = ENOENT;
    return -1;
}

//
// --------------------------------------------------------- Internal Functions
//

