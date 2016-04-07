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

#define NETLINK_SOCKET_FLAG_REPORT_KSTATUS 0x00000001

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

    This structure defines information about a netlink message.

Members:

    Buffer - Stores the address of the netlink buffer.

    BufferSize - Stores the size of the buffer, in bytes.

    DataOffset - Stores the offset from the beginning of the buffer to the
        beginning of the valid data. The next lower netlink layer should put
        its own headers right before this offset.

    FooterOffset - Stores the offset from the beginning of the buffer to the
        beginning of the footer data (i.e. the location to store the first byte
        of the next netlink layer's footer).

--*/

typedef struct NETLINK_MESSAGE_BUFFER {
    PVOID Buffer;
    ULONG BufferSize;
    ULONG DataOffset;
    ULONG FooterOffset;
} NETLINK_MESSAGE_BUFFER, *PNETLINK_MESSAGE_BUFFER;

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
    PNETLINK_MESSAGE_BUFFER ReceiveBuffer;
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
    ULONG HeaderSize,
    ULONG Size,
    ULONG FooterSize,
    PNETLINK_MESSAGE_BUFFER *NewBuffer
    );

/*++

Routine Description:

    This routine allocates a netlink message buffer. It always adds on space
    for the base netlink message header.

Arguments:

    HeaderSize - Supplies the number of header bytes needed, not including the
        base netlink message header.

    Size - Supplies the number of data bytes needed.

    FooterSize - Supplies the number of footer bytes needed.

    NewBuffer - Supplies a pointer where a pointer to the new allocation will
        be returned on success.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

LIBNETLINK_API
VOID
NlFreeBuffer (
    PNETLINK_MESSAGE_BUFFER Buffer
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
NlFillOutHeader (
    PNETLINK_LIBRARY_SOCKET Socket,
    PNETLINK_MESSAGE_BUFFER Message,
    ULONG DataLength,
    USHORT Type,
    USHORT Flags
    );

/*++

Routine Description:

    This routine fills out the netlink message header that's going to be sent.
    It will make sure there is enough room left in the supplied message buffer
    and add the header before the current data offset. It always adds the ACK
    and REQUEST flags.

Arguments:

    Socket - Supplies a pointer to the netlink socket that is sending the
        message.

    Message - Supplies a pointer to the netlink message buffer for which the
        header should be filled out.

    DataLength - Supplies the length of the message data payload, in bytes.

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
    PNETLINK_MESSAGE_BUFFER Message,
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
    PNETLINK_MESSAGE_BUFFER Message,
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
    PNETLINK_MESSAGE_BUFFER Message,
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
NlGenericFillOutHeader (
    PNETLINK_LIBRARY_SOCKET Socket,
    PNETLINK_MESSAGE_BUFFER Message,
    UCHAR Command,
    UCHAR Version
    );

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

LIBNETLINK_API
INT
NlGenericAddAttribute (
    PNETLINK_MESSAGE_BUFFER Message,
    PULONG MessageOffset,
    USHORT Type,
    PVOID Data,
    USHORT DataLength
    );

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

LIBNETLINK_API
INT
NlGenericGetAttribute (
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

#ifdef __cplusplus

}

#endif

