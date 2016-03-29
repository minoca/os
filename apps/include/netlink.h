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

#ifndef NETLINK_API

#define NETLINK_API DLLIMPORT

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

    SendNextSequence - Stores the next sequence number to use in a netlink
        message header being sent.

    ReceiveNextSequence - Stores the next sequence number that is expected to
        be received.

    LocalAddress - Stores the local address for the socket.

    ReceiveBuffer - Stores a pointer to a scratch buffer that the socket can
        use to receive messages.

--*/

typedef struct _NETLINK_SOCKET {
    INT Socket;
    ULONG Protocol;
    volatile ULONG SendNextSequence;
    volatile ULONG ReceiveNextSequence;
    struct sockaddr_nl LocalAddress;
    PNETLINK_MESSAGE_BUFFER ReceiveBuffer;
} NETLINK_SOCKET, *PNETLINK_SOCKET;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

NETLINK_API
VOID
NetlinkInitialize (
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

NETLINK_API
INT
NetlinkCreateSocket (
    ULONG Protocol,
    ULONG PortId,
    PNETLINK_SOCKET *NewSocket
    );

/*++

Routine Description:

    This routine creates a netlink socket with the given protocol and port ID.

Arguments:

    Protocol - Supplies the netlink protocol to use for the socket.

    PortId - Supplies a specific port ID to use for the socket, if available.
        Supply NETLINK_ANY_PORT_ID to have the socket dynamically bind to an
        available port ID.

    NewSocket - Supplies a pointer that receives a pointer to the newly created
        socket.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

NETLINK_API
VOID
NetlinkDestroySocket (
    PNETLINK_SOCKET Socket
    );

/*++

Routine Description:

    This routine destroys a netlink socket and all its resources.

Arguments:

    Socket - Supplies a pointer to the netlink socket to destroy.

Return Value:

    None.

--*/

NETLINK_API
INT
NetlinkAllocateBuffer (
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

NETLINK_API
VOID
NetlinkFreeBuffer (
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

NETLINK_API
INT
NetlinkFillOutHeader (
    PNETLINK_SOCKET Socket,
    PNETLINK_MESSAGE_BUFFER Message,
    ULONG DataLength,
    USHORT Type,
    USHORT Flags
    );

/*++

Routine Description:

    This routine fills out the netlink message header that's going to be sent.
    It will make sure there is enough room left in the supplied message buffer
    and add the header before the current data offset.

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

NETLINK_API
INT
NetlinkGenericFillOutHeader (
    PNETLINK_SOCKET Socket,
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

NETLINK_API
INT
NetlinkGenericGetFamilyId (
    PNETLINK_SOCKET Socket,
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

