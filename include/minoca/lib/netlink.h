/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#define LIBNETLINK_API __DLLIMPORT

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

#define NL_ANY_PORT_ID 0

//
// Set this flag in the netlink socket to receive KSTATUS error codes in
// netlink error messages. The default is to receive errno values.
//

#define NL_SOCKET_FLAG_REPORT_KSTATUS   0x00000001

//
// Set this flag in the netlink socket to disable the automatic setting of a
// sequence number on send and automatic validation of sequence numbers upon
// message reception.
//

#define NL_SOCKET_FLAG_NO_AUTO_SEQUENCE 0x00000002

//
// Supply this flag to the message reception routine to prevent it from waiting
// for an ACK before returning.
//

#define NL_RECEIVE_FLAG_NO_ACK_WAIT 0x00000001

//
// Supply these flags to the message reception routine to make sure the
// received messages come from either a given port ID or a multicast group.
//

#define NL_RECEIVE_FLAG_PORT_ID     0x00000002
#define NL_RECEIVE_FLAG_GROUP_MASK  0x00000004

//
// This flag is returned by the receive routine if an ACK was processed.
//

#define NL_RECEIVE_FLAG_ACK_RECEIVED 0x00000008

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

typedef struct _NL_MESSAGE_BUFFER {
    PVOID Buffer;
    ULONG BufferSize;
    ULONG DataSize;
    ULONG CurrentOffset;
} NL_MESSAGE_BUFFER, *PNL_MESSAGE_BUFFER;

/*++

Structure Description:

    This structure defines a socket for the netlink library.

Members:

    Socket - Stores the file descriptor for the associated C library socket.

    Protocol - Stores the netlink protocol over which the socket communicates.

    Flags - Stores a bitmask of netlink socket flags. See NL_SOCKET_FLAG_* for
        definitions.

    SendNextSequence - Stores the next sequence number to use in a netlink
        message header being sent.

    ReceiveNextSequence - Stores the next sequence number that is expected to
        be received.

    LocalAddress - Stores the local address for the socket.

    ReceiveBuffer - Stores a pointer to a scratch buffer that the socket can
        use to receive messages.

--*/

typedef struct _NL_SOCKET {
    INT Socket;
    ULONG Protocol;
    ULONG Flags;
    volatile ULONG SendNextSequence;
    volatile ULONG ReceiveNextSequence;
    struct sockaddr_nl LocalAddress;
    PNL_MESSAGE_BUFFER ReceiveBuffer;
} NL_SOCKET, *PNL_SOCKET;

/*++

Structure Description:

    This structure defines the context supplied to each invocation of the
    receive callback routine during receive message processing.

Members:

    Status - Stores the status from the receive callback routine.

    Type - Stores an optional message type that the receive callback routine
        can use to validate the message.

    PrivateContext - Stores an optional pointer to a private context to be used
        by the caller of the message receive handler on each invocation of the
        receive callback routine.

--*/

typedef struct _NL_RECEIVE_CONTEXT {
    INT Status;
    USHORT Type;
    PVOID PrivateContext;
} NL_RECEIVE_CONTEXT, *PNL_RECEIVE_CONTEXT;

typedef
VOID
(*PNL_RECEIVE_ROUTINE) (
    PNL_SOCKET Socket,
    PNL_RECEIVE_CONTEXT Context,
    PVOID Message
    );

/*++

Routine Description:

    This routine processes a protocol-layer netlink message.

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

/*++

Structure Description:

    This structure defines the parameters passed to receive a netlink message.

Members:

    ReceiveRoutine - Stores an optional pointer to a routine that will be
        called for each protocol layer message that is received.

    ReceiveContext - Stores a receive context that will be supplied to each
        invocation of the receive routine.

    Flags - Stores a bitmask of receive flags. See NL_RECEIVE_FLAG_* for
        definitions.

    PortId - Stores an optional port ID. If valid via the
        NL_RECEIVE_FLAG_PORT_ID, the receive message handler will skip any
        messages received from non-matching ports.

    GroupMask - Stores an optional multicast group mask. If valid via the
        NL_RECEIVE_FLAG_GROUP_MASK flag being set, the receive message handler
        will skip any messages received from non-matching groups.

--*/

typedef struct _NL_RECEIVE_PARAMETERS {
    PNL_RECEIVE_ROUTINE ReceiveRoutine;
    NL_RECEIVE_CONTEXT ReceiveContext;
    ULONG Flags;
    ULONG PortId;
    ULONG GroupMask;
} NL_RECEIVE_PARAMETERS, *PNL_RECEIVE_PARAMETERS;

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
    PNL_SOCKET *NewSocket
    );

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

LIBNETLINK_API
VOID
NlDestroySocket (
    PNL_SOCKET Socket
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
    PNL_MESSAGE_BUFFER *NewBuffer
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
    PNL_MESSAGE_BUFFER Buffer
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
    PNL_SOCKET Socket,
    PNL_MESSAGE_BUFFER Message,
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
        is ignored unless NL_SOCKET_FLAG_NO_AUTO_SEQUENCE is set in the socket.

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
    PNL_SOCKET Socket,
    PNL_MESSAGE_BUFFER Message,
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
    PNL_SOCKET Socket,
    PNL_RECEIVE_PARAMETERS Parameters
    );

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

LIBNETLINK_API
INT
NlAppendAttribute (
    PNL_MESSAGE_BUFFER Message,
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
    PNL_SOCKET Socket,
    PNL_MESSAGE_BUFFER Message,
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

LIBNETLINK_API
INT
NlGenericGetFamilyId (
    PNL_SOCKET Socket,
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

LIBNETLINK_API
INT
NlGenericJoinMulticastGroup (
    PNL_SOCKET Socket,
    USHORT FamilyId,
    PSTR GroupName
    );

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

#ifdef __cplusplus

}

#endif

