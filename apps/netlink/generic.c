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

#define NETLINK_GENERIC_HEADER_LENGTH \
    NETLINK_ALIGN(sizeof(NETLINK_GENERIC_HEADER))

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
NetlinkpGenericAddAttribute (
    PNETLINK_MESSAGE_BUFFER Message,
    ULONG MessageOffset,
    USHORT Type,
    PVOID Data,
    USHORT DataLength
    );

INT
NetlinkpGenericGetAttribute (
    PVOID Attributes,
    ULONG AttributesLength,
    USHORT Type,
    PVOID *Data,
    PUSHORT DataLength
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

NETLINK_API
INT
NetlinkGenericFillOutHeader (
    PNETLINK_SOCKET Socket,
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

NETLINK_API
INT
NetlinkGenericGetFamilyId (
    PNETLINK_SOCKET Socket,
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

    socklen_t AddressLength;
    PVOID Attributes;
    ssize_t BytesReceived;
    ssize_t BytesSent;
    size_t FamilyNameLength;
    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_HEADER Header;
    PUSHORT Id;
    USHORT IdLength;
    PNETLINK_MESSAGE_BUFFER Message;
    ULONG MessageLength;
    struct sockaddr_nl RemoteAddress;
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
    Status = NetlinkAllocateBuffer(NETLINK_GENERIC_HEADER_LENGTH,
                                   MessageLength,
                                   0,
                                   &Message);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    //
    // Add the family name attribute.
    //

    Status = NetlinkpGenericAddAttribute(
                                 Message,
                                 0,
                                 NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_NAME,
                                 FamilyName,
                                 FamilyNameLength);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    //
    // Fill out both the generic and base netlink headers.
    //

    Status = NetlinkGenericFillOutHeader(Socket,
                                         Message,
                                         NETLINK_GENERIC_CONTROL_GET_FAMILY,
                                         0);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    MessageLength += NETLINK_GENERIC_HEADER_LENGTH;
    Status = NetlinkFillOutHeader(Socket,
                                  Message,
                                  MessageLength,
                                  NETLINK_GENERIC_ID_CONTROL,
                                  NETLINK_HEADER_FLAG_REQUEST);

    if (Status == -1) {
        goto GetFamilyIdEnd;
    }

    //
    // Send off the family ID request message.
    //

    AddressLength = sizeof(struct sockaddr_nl);
    memset(&RemoteAddress, 0, AddressLength);
    RemoteAddress.nl_family = AF_NETLINK;
    RemoteAddress.nl_pid = NETLINK_KERNEL_PORT_ID;
    BytesSent = sendto(Socket->Socket,
                       Message->Buffer + Message->DataOffset,
                       Message->FooterOffset - Message->DataOffset,
                       0,
                       (const struct sockaddr *)&RemoteAddress,
                       AddressLength);

    if (BytesSent == -1) {
        Status = -1;
        goto GetFamilyIdEnd;
    }

    //
    // Look for a new family message.
    //

    while (TRUE) {
        AddressLength = sizeof(struct sockaddr_nl);
        BytesReceived = recvfrom(Socket->Socket,
                                 Socket->ReceiveBuffer->Buffer,
                                 Socket->ReceiveBuffer->BufferSize,
                                 0,
                                 (struct sockaddr *)&RemoteAddress,
                                 &AddressLength);

        if (BytesReceived == -1) {
            Status = -1;
            goto GetFamilyIdEnd;
        }

        if ((AddressLength != sizeof(struct sockaddr_nl)) ||
            (RemoteAddress.nl_pid != NETLINK_KERNEL_PORT_ID)) {

            continue;
        }

        Header = (PNETLINK_HEADER)Socket->ReceiveBuffer->Buffer;
        if ((BytesReceived < NETLINK_HEADER_LENGTH) ||
            (BytesReceived < Header->Length)) {

            continue;
        }

        BytesReceived -= NETLINK_HEADER_LENGTH;
        if ((Header->Type != NETLINK_GENERIC_ID_CONTROL) ||
            (Header->SequenceNumber != Socket->ReceiveNextSequence) ||
            (Header->PortId != NETLINK_KERNEL_PORT_ID)) {

            continue;
        }


        GenericHeader = NETLINK_DATA(Header);
        if ((BytesReceived < NETLINK_GENERIC_HEADER_LENGTH) ||
            (GenericHeader->Command != NETLINK_GENERIC_CONTROL_NEW_FAMILY)) {

            continue;
        }

        BytesReceived -= NETLINK_GENERIC_HEADER_LENGTH;
        Attributes = (PVOID)GenericHeader + NETLINK_GENERIC_HEADER_LENGTH;
        Status = NetlinkpGenericGetAttribute(
                                   Attributes,
                                   BytesReceived,
                                   NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_ID,
                                   (PVOID *)&Id,
                                   &IdLength);

        if (Status == -1) {
            continue;
        }

        if (IdLength != sizeof(USHORT)) {
            continue;
        }

        *FamilyId = *Id;
        RtlAtomicAdd32(&(Socket->ReceiveNextSequence), 1);
        break;
    }

GetFamilyIdEnd:
    if (Message != NULL) {
        NetlinkFreeBuffer(Message);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
NetlinkpGenericAddAttribute (
    PNETLINK_MESSAGE_BUFFER Message,
    ULONG MessageOffset,
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

    MessageOffset - Supplies the offset within the messages data region where
        the attribute should be stored.

    Type - Supplies the netlink generic attribute type.

    Data - Supplies a pointer to the attribute data.

    DataLength - Supplies the length of the data, in bytes.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PNETLINK_ATTRIBUTE Attribute;
    USHORT AttributeLength;
    ULONG AlignedLength;
    ULONG MessageLength;

    AttributeLength = DataLength + NETLINK_ATTRIBUTE_HEADER_LENGTH;
    AlignedLength = NETLINK_ALIGN(AttributeLength);
    MessageLength = Message->FooterOffset - Message->DataOffset;
    if ((MessageOffset > MessageLength) ||
        (AlignedLength < DataLength) ||
        ((MessageOffset + AlignedLength) < MessageOffset) ||
        ((MessageOffset + AlignedLength) > MessageLength)) {

        errno = EINVAL;
        return -1;
    }

    Attribute = Message->Buffer + Message->DataOffset + MessageOffset;
    Attribute->Length = AttributeLength;
    Attribute->Type = Type;
    RtlCopyMemory(NETLINK_ATTRIBUTE_DATA(Attribute), Data, DataLength);
    return 0;
}

INT
NetlinkpGenericGetAttribute (
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

    while (AttributesLength != 0) {
        if (AttributesLength < NETLINK_ATTRIBUTE_HEADER_LENGTH) {
            break;
        }

        Attribute = (PNETLINK_ATTRIBUTE)Attributes;
        if (Attribute->Type == Type) {
            *DataLength = Attribute->Length - NETLINK_ATTRIBUTE_HEADER_LENGTH;
            *Data = NETLINK_ATTRIBUTE_DATA(Attribute);
            return 0;
        }

        Attributes += NETLINK_ALIGN(Attribute->Length);
        AttributesLength -= NETLINK_ALIGN(Attribute->Length);
    }

    errno = ENOENT;
    return -1;
}

