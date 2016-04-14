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
NlGenericAppendHeaders (
    PNETLINK_LIBRARY_SOCKET Socket,
    PNETLINK_BUFFER Message,
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
        is ignored unless NETLINK_SOCKET_FLAG_NO_AUTO_SEQUENCE is set in the
        socket.

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
    PNETLINK_BUFFER Message;
    ULONG MessageLength;
    ULONG PayloadLength;
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
    PayloadLength = NETLINK_ATTRIBUTE_SIZE(FamilyNameLength);
    MessageLength = NETLINK_GENERIC_HEADER_LENGTH + PayloadLength;
    Status = NlAllocateBuffer(MessageLength, &Message);
    if (Status != 0) {
        goto GetFamilyIdEnd;
    }

    //
    // Fill out both the generic and base netlink headers.
    //

    Status = NlGenericAppendHeaders(Socket,
                                    Message,
                                    PayloadLength,
                                    0,
                                    NETLINK_GENERIC_ID_CONTROL,
                                    0,
                                    NETLINK_CONTROL_COMMAND_GET_FAMILY,
                                    0);

    if (Status != 0) {
        goto GetFamilyIdEnd;
    }

    //
    // Add the family name attribute.
    //

    Status = NlAppendAttribute(Message,
                               NETLINK_CONTROL_ATTRIBUTE_FAMILY_NAME,
                               FamilyName,
                               FamilyNameLength);

    if (Status != 0) {
        goto GetFamilyIdEnd;
    }

    //
    // Send off the family ID request message.
    //

    Status = NlSendMessage(Socket, Message, NETLINK_KERNEL_PORT_ID, 0, NULL);
    if (Status != 0) {
        goto GetFamilyIdEnd;
    }

    //
    // Attempt to receive a new family message.
    //

    Status = NlReceiveMessage(Socket, Socket->ReceiveBuffer, &PortId, NULL);
    if (Status != 0) {
        goto GetFamilyIdEnd;
    }

    Header = Socket->ReceiveBuffer->Buffer +
             Socket->ReceiveBuffer->CurrentOffset;

    BytesReceived = Socket->ReceiveBuffer->DataSize;
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
    Status = NlGetAttribute(Attributes,
                            BytesReceived,
                            NETLINK_CONTROL_ATTRIBUTE_FAMILY_ID,
                            (PVOID *)&Id,
                            &IdLength);

    if (Status != 0) {
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

    if (Status != 0) {
        goto GetFamilyIdEnd;
    }

GetFamilyIdEnd:
    if (Message != NULL) {
        NlFreeBuffer(Message);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

