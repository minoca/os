/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    netlink.c

Abstract:

    This module implements the netlink library functions.

Author:

    Chris Stevens 24-Mar-2016

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "netlinkp.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns the aligned size of the netlink message buffer structure.
//

#define NL_MESSAGE_BUFFER_SIZE NETLINK_ALIGN(sizeof(NL_MESSAGE_BUFFER))

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
NetlinkpConvertToNetworkAddress (
    const struct sockaddr *Address,
    socklen_t AddressLength,
    PNETWORK_ADDRESS NetworkAddress
    );

/*++

Routine Description:

    This routine converts a sockaddr address structure into a network address
    structure.

Arguments:

    Address - Supplies a pointer to the address structure.

    AddressLength - Supplies the length of the address structure in bytes.

    NetworkAddress - Supplies a pointer where the corresponding network address
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_ADDRESS on failure.

--*/

KSTATUS
NetlinkpConvertFromNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    struct sockaddr *Address,
    socklen_t *AddressLength
    );

/*++

Routine Description:

    This routine converts a network address structure into a sockaddr structure.

Arguments:

    NetworkAddress - Supplies a pointer to the network address to convert.

    Address - Supplies a pointer where the address structure will be returned.

    AddressLength - Supplies a pointer that on input contains the length of the
        specified Address structure, and on output returns the length of the
        returned address. If the supplied buffer is not big enough to hold the
        address, the address is truncated, and the larger needed buffer size
        will be returned here.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the address buffer is not big enough.

    STATUS_INVALID_ADDRESS on failure.

--*/

//
// -------------------------------------------------------------------- Globals
//

CL_NETWORK_CONVERSION_INTERFACE NetlinkAddressConversionInterface = {
    CL_NETWORK_CONVERSION_INTERFACE_VERSION,
    AF_NETLINK,
    NetDomainNetlink,
    NetlinkpConvertToNetworkAddress,
    NetlinkpConvertFromNetworkAddress
};

//
// ------------------------------------------------------------------ Functions
//

//
// TODO: Make this a constructor.
//

LIBNETLINK_API
VOID
NlInitialize (
    PVOID Environment
    )

/*++

Routine Description:

    This routine initializes the Minoca Netlink Library. This routine is
    normally called by statically linked assembly within a program, and unless
    developing outside the usual paradigm should not need to call this routine
    directly.

Arguments:

    Environment - Supplies a pointer to the environment information.

Return Value:

    None.

--*/

{

    ClRegisterTypeConversionInterface(ClConversionNetworkAddress,
                                      &NetlinkAddressConversionInterface,
                                      TRUE);

    return;
}

LIBNETLINK_API
INT
NlCreateSocket (
    ULONG Protocol,
    ULONG PortId,
    ULONG Flags,
    PNL_SOCKET *NewSocket
    )

/*++

Routine Description:

    This routine creates a netlink socket with the given protocol and port ID.

Arguments:

    Protocol - Supplies the netlink protocol to use for the socket.

    PortId - Supplies a specific port ID to use for the socket, if available.
        Supply NL_ANY_PORT_ID to have the socket dynamically bind to an
        available port ID.

    Flags - Supplies a bitmask of netlink socket flags. See NL_SOCKET_FLAG_*
        for definitions.

    NewSocket - Supplies a pointer that receives a pointer to the newly created
        socket.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    socklen_t AddressLength;
    PNL_SOCKET Socket;
    INT Status;

    Socket = malloc(sizeof(NL_SOCKET));
    if (Socket == NULL) {
        errno = ENOMEM;
        return -1;
    }

    memset(Socket, 0, sizeof(NL_SOCKET));
    Status = NlAllocateBuffer(NETLINK_SCRATCH_BUFFER_SIZE,
                              &(Socket->ReceiveBuffer));

    if (Status != 0) {
        goto CreateSocketEnd;
    }

    Socket->Protocol = Protocol;
    Socket->Flags = Flags;
    Socket->Socket = socket(AF_NETLINK, SOCK_DGRAM, Protocol);
    if (Socket->Socket == -1) {
        Status = -1;
        goto CreateSocketEnd;
    }

    //
    // Bind the socket. If the supplied port ID is NL_ANY_PORT_ID, then an
    // ephemeral port will be assign. Otherwise the socket will be bound to the
    // given port if it's available.
    //

    AddressLength = sizeof(struct sockaddr_nl);
    Socket->LocalAddress.nl_family = AF_NETLINK;
    Socket->LocalAddress.nl_pid = PortId;
    Status = bind(Socket->Socket,
                  (struct sockaddr *)&(Socket->LocalAddress),
                  AddressLength);

    if (Status != 0) {
        goto CreateSocketEnd;
    }

    Status = getsockname(Socket->Socket,
                         (struct sockaddr *)&(Socket->LocalAddress),
                         &AddressLength);

    if (Status != 0) {
        goto CreateSocketEnd;
    }

    ASSERT((Socket->LocalAddress.nl_pid != NL_ANY_PORT_ID) &&
           (Socket->LocalAddress.nl_pid != NETLINK_KERNEL_PORT_ID));

CreateSocketEnd:
    if (Status != 0) {
        if (Socket != NULL) {
            NlDestroySocket(Socket);
            Socket = NULL;
        }
    }

    *NewSocket = Socket;
    return Status;
}

LIBNETLINK_API
VOID
NlDestroySocket (
    PNL_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys a netlink socket and all its resources.

Arguments:

    Socket - Supplies a pointer to the netlink socket to destroy.

Return Value:

    None.

--*/

{

    if (Socket->Socket != -1) {
        close(Socket->Socket);
    }

    if (Socket->ReceiveBuffer != NULL) {
        NlFreeBuffer(Socket->ReceiveBuffer);
    }

    free(Socket);
    return;
}

LIBNETLINK_API
INT
NlAllocateBuffer (
    ULONG Size,
    PNL_MESSAGE_BUFFER *NewBuffer
    )

/*++

Routine Description:

    This routine allocates a netlink message buffer. It always adds on space
    for the base netlink message header.

Arguments:

    Size - Supplies the number of data bytes needed.

    NewBuffer - Supplies a pointer where a pointer to the new allocation will
        be returned on success.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PNL_MESSAGE_BUFFER Buffer;
    INT Status;
    ULONG TotalSize;

    Buffer = NULL;
    Status = -1;
    if (Size != NETLINK_ALIGN(Size)) {
        errno = EINVAL;
        goto AllocateBufferEnd;
    }

    Size = NETLINK_ALIGN(Size);
    TotalSize = NETLINK_HEADER_LENGTH + Size;
    if (TotalSize < Size) {
        errno = EINVAL;
        goto AllocateBufferEnd;
    }

    Buffer = malloc(TotalSize + NL_MESSAGE_BUFFER_SIZE);
    if (Buffer == NULL) {
        errno = ENOMEM;
        goto AllocateBufferEnd;
    }

    Buffer->Buffer = (PVOID)Buffer + NL_MESSAGE_BUFFER_SIZE;
    Buffer->BufferSize = TotalSize;
    Buffer->DataSize = 0;
    Buffer->CurrentOffset = 0;
    Status = 0;

AllocateBufferEnd:
    if (Status != 0) {
        if (Buffer != NULL) {
            free(Buffer);
            Buffer = NULL;
        }
    }

    *NewBuffer = Buffer;
    return Status;
}

LIBNETLINK_API
VOID
NlFreeBuffer (
    PNL_MESSAGE_BUFFER Buffer
    )

/*++

Routine Description:

    This routine frees a previously allocated netlink message buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer to be released.

Return Value:

    None.

--*/

{

    free(Buffer);
    return;
}

LIBNETLINK_API
INT
NlAppendHeader (
    PNL_SOCKET Socket,
    PNL_MESSAGE_BUFFER Message,
    ULONG PayloadLength,
    ULONG SequenceNumber,
    USHORT Type,
    USHORT Flags
    )

/*++

Routine Description:

    This routine appends a base netlink header to the message. It will make
    sure there is enough room left in the supplied message buffer, add the
    header before at the current offset and update the offset and valid data
    size when complete. It always adds the ACK and REQUEST flags.

Arguments:

    Socket - Supplies a pointer to the netlink socket that is sending the
        message.

    Message - Supplies a pointer to the netlink message buffer to which the
        header should be appended.

    PayloadLength - Supplies the length of the message data payload, in bytes.
        This does not include the base netlink header length.

    SequenceNumber - Supplies the sequence number for the message. This value
        is ignored unless NL_SOCKET_FLAG_NO_AUTO_SEQUENCE is set in the socket.

    Type - Supplies the netlink message type.

    Flags - Supplies a bitmask of netlink message flags. See
        NETLINK_HEADER_FLAG_* for definitions.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PNETLINK_HEADER Header;
    ULONG MessageLength;

    PayloadLength += NETLINK_HEADER_LENGTH;
    MessageLength = Message->BufferSize - Message->CurrentOffset;
    if (MessageLength < PayloadLength) {
        errno = ENOBUFS;
        return -1;
    }

    Flags |= NETLINK_HEADER_FLAG_ACK | NETLINK_HEADER_FLAG_REQUEST;
    Header = Message->Buffer + Message->CurrentOffset;
    Header->Length = PayloadLength;
    Header->Type = Type;
    Header->Flags = Flags;
    if ((Socket->Flags & NL_SOCKET_FLAG_NO_AUTO_SEQUENCE) != 0) {
        Header->SequenceNumber = SequenceNumber;

    } else {
        Header->SequenceNumber = RtlAtomicAdd32(&(Socket->SendNextSequence), 1);
    }

    Header->PortId = Socket->LocalAddress.nl_pid;

    //
    // Move the offset and data size to the first byte after the header.
    //

    Message->CurrentOffset += NETLINK_HEADER_LENGTH;
    Message->DataSize += NETLINK_HEADER_LENGTH;
    return 0;
}

LIBNETLINK_API
INT
NlSendMessage (
    PNL_SOCKET Socket,
    PNL_MESSAGE_BUFFER Message,
    ULONG PortId,
    ULONG GroupMask,
    PULONG BytesSent
    )

/*++

Routine Description:

    This routine sends a netlink message for the given socket.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to send the
        message.

    Message - Supplies a pointer to the message to be sent. The routine will
        attempt to send the entire message between the data offset and footer
        offset.

    PortId - Supplies the port ID of the recipient of the message.

    GroupMask - Supplies the group mask of the message recipients.

    BytesSent - Supplies an optional pointer the receives the number of bytes
        sent on success.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    struct sockaddr_nl Address;
    ssize_t LocalBytesSent;
    INT Status;

    memset(&Address, 0, sizeof(struct sockaddr_nl));
    Address.nl_family = AF_NETLINK;
    Address.nl_pid = PortId;
    Address.nl_groups = GroupMask;
    LocalBytesSent = sendto(Socket->Socket,
                            Message->Buffer,
                            Message->DataSize,
                            0,
                            (struct sockaddr *)&Address,
                            sizeof(struct sockaddr_nl));

    if (LocalBytesSent < 0) {
        Status = -1;

    } else {
        if (BytesSent != NULL) {
            *BytesSent = LocalBytesSent;
        }

        Status = 0;
    }

    return Status;
}

LIBNETLINK_API
INT
NlReceiveMessage (
    PNL_SOCKET Socket,
    PNL_RECEIVE_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine receives one or more netlink messages, does some simple
    validation, handles the default netlink message types, and calls the given
    receive routine callback for each protocol layer message.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to receive the
        message.

    Parameters - Supplies a pointer to the receive message parameters.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    struct sockaddr_nl Address;
    socklen_t AddressLength;
    INTN BytesRemaining;
    PINT Error;
    ULONG ErrorLength;
    KSTATUS ErrorStatus;
    INT ErrorValue;
    ULONG Flags;
    BOOL GroupMatch;
    PNETLINK_HEADER Header;
    PNL_MESSAGE_BUFFER Message;
    BOOL Multipart;
    BOOL PortMatch;
    BOOL ReceiveMore;
    INT Status;

    Flags = Parameters->Flags;
    Parameters->Flags = 0;
    Message = Socket->ReceiveBuffer;
    Multipart = FALSE;
    ReceiveMore = TRUE;
    while ((ReceiveMore != FALSE) || (Multipart != FALSE)) {
        Message->CurrentOffset = 0;
        Message->DataSize = 0;
        AddressLength = sizeof(struct sockaddr_nl);
        BytesRemaining = recvfrom(Socket->Socket,
                                  Message->Buffer,
                                  Message->BufferSize,
                                  0,
                                  (struct sockaddr *)&Address,
                                  &AddressLength);

        if (BytesRemaining < 0) {
            Status = -1;
            goto ReceiveMessageEnd;
        }

        Message->DataSize = BytesRemaining;
        if ((AddressLength != sizeof(struct sockaddr_nl)) ||
            (Address.nl_family != AF_NETLINK)) {

            errno = EAFNOSUPPORT;
            Status = -1;
            goto ReceiveMessageEnd;
        }

        //
        // If supplied, validate the port and/or group. Skipping any messages
        // that do not match at least one of them.
        //

        PortMatch = TRUE;
        GroupMatch = TRUE;
        if (((Flags & NL_RECEIVE_FLAG_PORT_ID) != 0) &&
            (Address.nl_pid != Parameters->PortId)) {

            PortMatch = FALSE;
        }

        if (((Flags & NL_RECEIVE_FLAG_GROUP_MASK) != 0) &&
            ((Address.nl_groups & Parameters->GroupMask) == 0)) {

            GroupMatch = FALSE;
        }

        if ((PortMatch == FALSE) && (GroupMatch == FALSE)) {
            continue;
        }

        //
        // If the caller is not expecting an ACK, then do not wait for one.
        //

        if ((Flags & NL_RECEIVE_FLAG_NO_ACK_WAIT) != 0) {
            ReceiveMore = FALSE;
        }

        //
        // Process each message in the buffer.
        //

        while (BytesRemaining > 0) {
            Header = Message->Buffer + Message->CurrentOffset;
            if ((BytesRemaining < NETLINK_HEADER_LENGTH) ||
                (BytesRemaining < Header->Length)) {

                break;
            }

            //
            // If there is no multi-part flag, then there shouldn't be a reason
            // to read another message.
            //

            if ((Header->Flags & NETLINK_HEADER_FLAG_MULTIPART) != 0) {
                Multipart = TRUE;
            }

            //
            // Validate the sequence number, but skip this on multicast
            // messages. Sequence numbers don't make much sense for out-of-band
            // multicast messages.
            //

            if (((Socket->Flags & NL_SOCKET_FLAG_NO_AUTO_SEQUENCE) == 0) &&
                (Address.nl_pid == Socket->LocalAddress.nl_pid) &&
                (Header->SequenceNumber != Socket->ReceiveNextSequence)) {

                errno = EILSEQ;
                Status = -1;
                goto ReceiveMessageEnd;
            }

            //
            // If an error is received, stop receiving. If an ACK is received,
            // finish processing this set of messages, but don't read any more.
            //

            if (Header->Type == NETLINK_MESSAGE_TYPE_ERROR) {
                if (((Socket->Flags & NL_SOCKET_FLAG_NO_AUTO_SEQUENCE) == 0) &&
                    (Address.nl_pid == Socket->LocalAddress.nl_pid)) {

                    RtlAtomicAdd32(&(Socket->ReceiveNextSequence), 1);
                }

                ErrorLength = NETLINK_HEADER_LENGTH +
                              sizeof(NETLINK_ERROR_MESSAGE);

                if (Header->Length < ErrorLength) {
                    errno = ENOMSG;
                    Status = -1;
                    goto ReceiveMessageEnd;
                }

                //
                // Convert from KSTATUS to the C library value for errno.
                //

                Error = NETLINK_DATA(Header);
                ErrorStatus = (KSTATUS)*Error;
                ErrorValue = ClConvertKstatusToErrorNumber(ErrorStatus);

                //
                // If the library consumer did not specifically ask for KSTATUS
                // errors, then all error messages need to be converted.
                //

                if ((Socket->Flags & NL_SOCKET_FLAG_REPORT_KSTATUS) == 0) {
                    *Error = ErrorValue;
                }

                if (!KSUCCESS(ErrorStatus)) {
                    errno = ErrorValue;
                    Status = -1;
                    goto ReceiveMessageEnd;
                }

                //
                // Receives should not exit until an ACK has been seen, unless
                // the caller specifically requested to not wait.
                //

                if ((Flags & NL_RECEIVE_FLAG_NO_ACK_WAIT) == 0) {
                    ReceiveMore = FALSE;
                }

                Parameters->Flags |= NL_RECEIVE_FLAG_ACK_RECEIVED;

            //
            // If this is the last message in a multi-part message, then stop
            // receiving more data.
            //

            } else if (Header->Type == NETLINK_MESSAGE_TYPE_DONE) {
                Multipart = FALSE;

            //
            // For all protocol layer messages, invoke the given callback.
            //

            } else if ((Parameters->ReceiveRoutine != NULL) &&
                       (Header->Type >=
                        NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM)) {

                Parameters->ReceiveRoutine(Socket,
                                           &(Parameters->ReceiveContext),
                                           Header);
            }

            //
            // Skip along to the next message.
            //

            Message->CurrentOffset += NETLINK_ALIGN(Header->Length);
            BytesRemaining -= NETLINK_ALIGN(Header->Length);
        }
    }

    Status = 0;

ReceiveMessageEnd:
    return Status;
}

LIBNETLINK_API
INT
NlAppendAttribute (
    PNL_MESSAGE_BUFFER Message,
    USHORT Type,
    PVOID Data,
    USHORT DataLength
    )

/*++

Routine Description:

    This routine appends a netlink attribute to the given message. It validates
    that there is enough space for the attribute and moves the message buffer's
    offset to the first byte after the attribute. It also updates the buffer's
    valid data size. The exception to this rule is if a NULL data buffer is
    supplied; the buffer's data offset and size will only be updated for the
    attribute's header.

Arguments:

    Message - Supplies a pointer to the netlink message buffer to which the
        attribute will be added.

    Type - Supplies the netlink attribute type.

    Data - Supplies an optional pointer to the attribute data to be stored in
        the buffer. Even if no data buffer is supplied, a data length may be
        supplied for the case of child attributes that are yet to be appended.

    DataLength - Supplies the length of the data, in bytes.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PNETLINK_ATTRIBUTE Attribute;
    ULONG AttributeLength;
    ULONG MessageLength;

    AttributeLength = NETLINK_ATTRIBUTE_SIZE(DataLength);
    MessageLength = Message->BufferSize - Message->CurrentOffset;
    if (MessageLength < AttributeLength) {
        return ENOBUFS;
    }

    Attribute = Message->Buffer + Message->CurrentOffset;
    Attribute->Length = NETLINK_ATTRIBUTE_LENGTH(DataLength);
    Attribute->Type = Type;
    if (Data != NULL) {
        RtlCopyMemory(NETLINK_ATTRIBUTE_DATA(Attribute), Data, DataLength);
        Message->CurrentOffset += AttributeLength;
        Message->DataSize += AttributeLength;

    } else {
        Message->CurrentOffset += NETLINK_ATTRIBUTE_HEADER_LENGTH;
        Message->DataSize += NETLINK_ATTRIBUTE_HEADER_LENGTH;
    }

    return 0;
}

LIBNETLINK_API
INT
NlGetAttribute (
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

KSTATUS
NetlinkpConvertToNetworkAddress (
    const struct sockaddr *Address,
    socklen_t AddressLength,
    PNETWORK_ADDRESS NetworkAddress
    )

/*++

Routine Description:

    This routine converts a sockaddr address structure into a network address
    structure.

Arguments:

    Address - Supplies a pointer to the address structure.

    AddressLength - Supplies the length of the address structure in bytes.

    NetworkAddress - Supplies a pointer where the corresponding network address
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_ADDRESS on failure.

--*/

{

    struct sockaddr_nl *NetlinkAddress;

    if ((AddressLength < sizeof(struct sockaddr_nl)) ||
        (Address->sa_family != AF_NETLINK)) {

        return STATUS_INVALID_ADDRESS;
    }

    NetlinkAddress = (struct sockaddr_nl *)Address;
    NetworkAddress->Domain = NetDomainNetlink;
    NetworkAddress->Port = NetlinkAddress->nl_pid;
    NetworkAddress->Address[0] = (UINTN)NetlinkAddress->nl_groups;
    return STATUS_SUCCESS;
}

KSTATUS
NetlinkpConvertFromNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    struct sockaddr *Address,
    socklen_t *AddressLength
    )

/*++

Routine Description:

    This routine converts a network address structure into a sockaddr structure.

Arguments:

    NetworkAddress - Supplies a pointer to the network address to convert.

    Address - Supplies a pointer where the address structure will be returned.

    AddressLength - Supplies a pointer that on input contains the length of the
        specified Address structure, and on output returns the length of the
        returned address. If the supplied buffer is not big enough to hold the
        address, the address is truncated, and the larger needed buffer size
        will be returned here.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the address buffer is not big enough.

    STATUS_INVALID_ADDRESS on failure.

--*/

{

    UINTN CopySize;
    struct sockaddr_nl NetlinkAddress;
    KSTATUS Status;

    if (NetworkAddress->Domain != NetDomainNetlink) {
        return STATUS_INVALID_ADDRESS;
    }

    Status = STATUS_SUCCESS;
    NetlinkAddress.nl_family = AF_NETLINK;
    NetlinkAddress.nl_pad = 0;
    NetlinkAddress.nl_pid = NetworkAddress->Port;
    NetlinkAddress.nl_groups = (ULONG)NetworkAddress->Address[0];
    CopySize = sizeof(NetlinkAddress);
    if (CopySize > *AddressLength) {
        CopySize = *AddressLength;
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    RtlCopyMemory(Address, &NetlinkAddress, CopySize);
    *AddressLength = sizeof(NetlinkAddress);
    return Status;
}

