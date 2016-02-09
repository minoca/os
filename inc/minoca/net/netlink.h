/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    netlink.h

Abstract:

    This header contains definitions for netlink sockets.

Author:

    Chris Stevens 9-Feb-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the current version number of netlink properties structure.
//

#define NETLINK_PROPERTIES_VERSION 1

//
// Define the standard netlink message types common to all netlink families.
//

#define NETLINK_MESSAGE_TYPE_NOP 0
#define NETLINK_MESSAGE_TYPE_ERROR 1
#define NETLINK_MESSAGE_TYPE_DONE 2
#define NETLINK_MESSAGE_TYPE_GENERIC 3

//
// Define the netlink message header flags.
//

#define NETLINK_HEADER_FLAG_REQUEST       0x0001
#define NETLINK_HEADER_FLAG_MORE_MESSAGES 0x0002
#define NETLINK_HEADER_FLAG_ACK           0x0004
#define NETLINK_HEADER_FLAG_ECHO          0x0008

//
// Define the maximum netlink packet size, including the header.
//

#define NETLINK_MAX_PACKET_SIZE MAX_ULONG

//
// ------------------------------------------------------ Data Type Definitions
//

#ifndef NET_API

#define NET_API DLLIMPORT

#endif

/*++

Structure Description:

    This structure defines an netlink address.

Members:

    Network - Stores the network type of this address.

    Port - Stores the 32 bit port ID.

    GroupMask - Stores the 32 bit group mask.

    NetworkAddress - Stores the unioned opaque version, used to ensure the
        structure is the proper size.

--*/

typedef struct _NETLINK_ADDRESS {
    union {
        struct {
            SOCKET_NETWORK Network;
            ULONG Port;
            ULONG GroupMask;
        };

        NETWORK_ADDRESS NetworkAddress;
    };

} NETLINK_ADDRESS, *PNETLINK_ADDRESS;

/*++

Structure Description:

    This structure defines the header of a netlink data message.

Members:

    Length - Stores the length of the netlink message, including the header.

    Type - Stores the message type. See NETLINK_MESSAGE_TYPE_* for definitions.

    Flags - Stores a bitmask of message flags. See NETLINK_HEADER_FLAG_* for
        definitions.

    SequenceNumber - Stores the sequence number of the netlink message.

    PortId - Stores the port ID of the sending socket.

--*/

typedef struct _NETLINK_HEADER {
    ULONG Length;
    USHORT Type;
    USHORT Flags;
    ULONG SequenceNumber;
    ULONG PortId;
} PACKED NETLINK_HEADER, *PNETLINK_HEADER;

/*++

Structure Description:

    This structure defines the data portion of a netlink error message.

Members:

    Error - Stores the error caused by the bad message.

    Header - Stores the header of the bad netlink message that caused the error.

--*/

typedef struct _NETLINK_ERROR_MESSAGE {
    INT Error;
    NETLINK_HEADER Header;
} PACKED NETLINK_ERROR_MESSAGE, *PNETLINK_ERROR_MESSAGE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

