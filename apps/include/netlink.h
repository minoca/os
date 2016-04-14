/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    netlink.h

Abstract:

    This header contains definitions for the Minoca Netlink Library.

Author:

    Chris Stevens 24-Mar-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <sys/socket.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

#ifndef LIBNETLINK_API

#define LIBNETLINK_API DLLIMPORT

#endif

//
// Define the netlink address family.
//

#define AF_NETLINK 4

//
// Define the level number for the get/setsockopt that applies to all netlink
// sockets.
//

#define SOL_NETLINK 256

//
// Define the netlink socket options.
//

#define NETLINK_ADD_MEMBERSHIP 1
#define NETLINK_DROP_MEMBERSHIP 2

//
// Define the netlink socket protocols.
//

#define NETLINK_GENERIC 257

//
// Define the port ID value to supply on socket creation if the port does not
// matter.
//

#define NETLINK_ANY_PORT_ID 0

//
// Set this flag in the netlink socket to receive KSTATUS error codes in
// netlink error messages. The default is to receive errno values.
//

#define NETLINK_SOCKET_FLAG_REPORT_KSTATUS   0x00000001
#define NETLINK_SOCKET_FLAG_NO_AUTO_SEQUENCE 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a netlink family socket address.

Members:

    nl_family - Stores the address family, which is always AF_NETLINK for
        netlink addresses.

    nl_pad - Stores two bytes of padding.

    nl_pid - Stores the port ID for the address.

    nl_groups - Stores the multicast group information for the address.

--*/

struct sockaddr_nl {
    sa_family_t nl_family;
    unsigned short nl_pad;
    pid_t nl_pid;
    uint32_t nl_groups;
};

/*++

Structure Description:

    This structure defines information about a netlink message buffer.

Members:

    Buffer - Stores the address of the netlink buffer.

    BufferSize - Stores the size of the buffer, in bytes.

    DataSize - Stores the size of the valid data in the buffer, in bytes.

    CurrentOffset - Stores the byte offset into the buffer indicating where the
        next set of data will be appended.

--*/

typedef struct NETLINK_BUFFER {
    PVOID Buffer;
    ULONG BufferSize;
    ULONG DataSize;
    ULONG CurrentOffset;
} NETLINK_BUFFER, *PNETLINK_BUFFER;

/*++

Structure Description:

    This structure defines a socket for the netlink library.

Members:

    Socket - Stores the file descriptor for the associated C library socket.

    Protocol - Stores the netlink protocol over which the socket communicates.

    Flags - Stores a bitmask of netlink socket flags. See
        NETLINK_SOCKET_FLAG_* for definitions.

    SendNextSequence - Stores the next sequence number to use in a netlink
        message header being sent.

    ReceiveNextSequence - Stores the next sequence number that is expected to
        be received.

    LocalAddress - Stores the local address for the socket.

    ReceiveBuffer - Stores a pointer to a scratch buffer that the socket can
        use to receive messages.

--*/

typedef struct _NETLINK_LIBRARY_SOCKET {
    INT Socket;
    ULONG Protocol;
    ULONG Flags;
    volatile ULONG SendNextSequence;
    volatile ULONG ReceiveNextSequence;
    struct sockaddr_nl LocalAddress;
    PNETLINK_BUFFER ReceiveBuffer;
} NETLINK_LIBRARY_SOCKET, *PNETLINK_LIBRARY_SOCKET;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBNETLINK_API
VOID
NlInitialize (
    PVOID Environment
    );

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

LIBNETLINK_API
INT
NlCreateSocket (
    ULONG Protocol,
    ULONG PortId,
    ULONG Flags,
    PNETLINK_LIBRARY_SOCKET *NewSocket
    );

/*++

Routine Description:

    This routine creates a netlink socket with the given protocol and port ID.

Arguments:

    Protocol - Supplies the netlink protocol to use for the socket.

    PortId - Supplies a specific port ID to use for the socket, if available.
        Supply NETLINK_ANY_PORT_ID to have the socket dynamically bind to an
        available port ID.

    Flags - Supplies a bitmask of netlink socket flags. See
        NETLINK_SOCKET_FLAG_* for definitions.

    NewSocket - Supplies a pointer that receives a pointer to the newly created
        socket.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBNETLINK_API
VOID
NlDestroySocket (
    PNETLINK_LIBRARY_SOCKET Socket
    );

/*++

Routine Description:

    This routine destroys a netlink socket and all its resources.

Arguments:

    Socket - Supplies a pointer to the netlink socket to destroy.

Return Value:

    None.

--*/

LIBNETLINK_API
INT
NlAllocateBuffer (
    ULONG Size,
    PNETLINK_BUFFER *NewBuffer
    );

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

LIBNETLINK_API
VOID
NlFreeBuffer (
    PNETLINK_BUFFER Buffer
    );

/*++

Routine Description:

    This routine frees a previously allocated netlink message buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer to be released.

Return Value:

    None.

--*/

LIBNETLINK_API
INT
NlAppendHeader (
    PNETLINK_LIBRARY_SOCKET Socket,
    PNETLINK_BUFFER Message,
    ULONG PayloadLength,
    ULONG SequenceNumber,
    USHORT Type,
    USHORT Flags
    );

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
        is ignored unless NETLINK_SOCKET_FLAG_NO_AUTO_SEQUENCE is set in the
        socket.

    Type - Supplies the netlink message type.

    Flags - Supplies a bitmask of netlink message flags. See
        NETLINK_HEADER_FLAG_* for definitions.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBNETLINK_API
INT
NlSendMessage (
    PNETLINK_LIBRARY_SOCKET Socket,
    PNETLINK_BUFFER Message,
    ULONG PortId,
    ULONG GroupMask,
    PULONG BytesSent
    );

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

LIBNETLINK_API
INT
NlReceiveMessage (
    PNETLINK_LIBRARY_SOCKET Socket,
    PNETLINK_BUFFER Message,
    PULONG PortId,
    PULONG GroupMask
    );

/*++

Routine Description:

    This routine receives a netlink message for the given socket. It validates
    the received message to make sure the netlink header properly describes the
    number of byte received. The number of bytes received, in both error and
    success cases, can be retrieved from the message buffer.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to receive the
        message.

    Message - Supplies a pointer to a netlink message that receives the read
        data.

    PortId - Supplies an optional pointer that receives the port ID of the
        message sender.

    GroupMask - Supplies an optional pointer that receives the group mask of
        the received packet.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBNETLINK_API
INT
NlReceiveAcknowledgement (
    PNETLINK_LIBRARY_SOCKET Socket,
    PNETLINK_BUFFER Message,
    ULONG ExpectedPortId
    );

/*++

Routine Description:

    This routine receives a netlink acknowledgement message for the given
    socket. It validates the received message to make sure the netlink header
    properly describes the number of byte received. The number of bytes
    received, in both error and success cases, can be retrieved from the
    message buffer.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to receive the
        acknowledgement message.

    Message - Supplies a pointer to a netlink message that receives the read
        data.

    ExpectedPortId - Supplies the expected port ID of the socket acknowledging
        the message.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBNETLINK_API
INT
NlAppendAttribute (
    PNETLINK_BUFFER Message,
    USHORT Type,
    PVOID Data,
    USHORT DataLength
    );

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

LIBNETLINK_API
INT
NlGetAttribute (
    PVOID Attributes,
    ULONG AttributesLength,
    USHORT Type,
    PVOID *Data,
    PUSHORT DataLength
    );

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
    );

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

LIBNETLINK_API
INT
NlGenericGetFamilyId (
    PNETLINK_LIBRARY_SOCKET Socket,
    PSTR FamilyName,
    PUSHORT FamilyId
    );

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

#ifdef __cplusplus

}

#endif

