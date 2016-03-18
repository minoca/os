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

#define NETLINK_MESSAGE_TYPE_NOP 1
#define NETLINK_MESSAGE_TYPE_ERROR 2
#define NETLINK_MESSAGE_TYPE_DONE 3
#define NETLINK_MESSAGE_TYPE_OVERRUN 4
#define NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM 16

//
// Define the netlink message header flags.
//

#define NETLINK_HEADER_FLAG_REQUEST       0x0001
#define NETLINK_HEADER_FLAG_MORE_MESSAGES 0x0002
#define NETLINK_HEADER_FLAG_ACK           0x0004
#define NETLINK_HEADER_FLAG_ECHO          0x0008

//
// Define the alignment for netlink attribute headers.
//

#define NETLINK_ATTRIBUTE_ALIGNMENT 4

//
// Define the maximum netlink packet size, including the header.
//

#define NETLINK_MAX_PACKET_SIZE MAX_ULONG

//
// Define the current version of the generic netlink family properties
// structure.
//

#define NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION 1

//
// Define the maximum length of a generic netlink family name.
//

#define NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH 16

//
// Define the standard generic netlink message types.
//

#define NETLINK_GENERIC_ID_CONTROL NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM

//
// Define the name on the generic control family expected by user mode
// application.
//

#define NETLINK_GENERIC_CONTROL_NAME "nlctrl"

//
// Define the generic control command values.
//

#define NETLINK_GENERIC_CONTROL_NEW_FAMILY 1
#define NETLINK_GENERIC_CONTROL_GET_FAMILY 3

//
// Define the generic control attributes.
//

#define NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_ID 1
#define NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_NAME 2
#define NETLINK_GENERIC_CONTROL_ATTRIBUTE_VERSION 3
#define NETLINK_GENERIC_CONTROL_ATTRIBUTE_HEADER_SIZE 4
#define NETLINK_GENERIC_CONTROL_ATTRIBUTE_MAX_ATTRIBUTE 5
#define NETLINK_GENERIC_CONTROL_ATTRIBUTE_OPERATIONS 6
#define NETLINK_GENERIC_CONTROL_ATTRIBUTE_MULTICAST_GROUPS 7

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

    Domain - Stores the network domain of this address.

    Port - Stores the 32 bit port ID.

    GroupMask - Stores the 32 bit group mask.

    NetworkAddress - Stores the unioned opaque version, used to ensure the
        structure is the proper size.

--*/

typedef struct _NETLINK_ADDRESS {
    union {
        struct {
            NET_DOMAIN_TYPE Domain;
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

    Type - Stores the message type. See NETLINK_MESSAGE_TYPE_* for global
        definitions. Otherwise this stores protocol-specific message types.

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

/*++

Structure Description:

    This structure defines a netlink attribute.

Members:

    Length - Stores the length of the attribute, in bytes, including the header.

    Type - Stores the message-specific attribute type.

--*/

typedef struct _NETLINK_ATTRIBUTE {
    USHORT Length;
    USHORT Type;
} PACKED NETLINK_ATTRIBUTE, *PNETLINK_ATTRIBUTE;

/*++

Structure Description:

    This structure defines the header for a generic netlink message.

Members:

    Command - Stores the generic message command value.

    Version - Stores the generic message version.

    Reserved - Stores 2 reserved bytes.

--*/

typedef struct _NETLINK_GENERIC_HEADER {
    UCHAR Command;
    UCHAR Version;
    USHORT Reserved;
} PACKED NETLINK_GENERIC_HEADER, *PNETLINK_GENERIC_HEADER;

typedef
VOID
(*PNETLINK_GENERIC_FAMILY_PROCESS_RECEIVED_DATA) (
    PIO_HANDLE Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

/*++

Routine Description:

    This routine is called to process a received generic netlink packet.

Arguments:

    Socket - Supplies a handle to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

/*++

Structure Description:

    This structure defines the generic netlink family interface.

Members:

    ProcessReceivedData - Stores a pointer to a function called when a packet
        is received by the netlink network.

--*/

typedef struct _NETLINK_GENERIC_FAMILY_INTERFACE {
    PNETLINK_GENERIC_FAMILY_PROCESS_RECEIVED_DATA ProcessReceivedData;
} NETLINK_GENERIC_FAMILY_INTERFACE, *PNETLINK_GENERIC_FAMILY_INTERFACE;

/*++

Structure Description:

    This structure defines a generic netlink family properties.

Members:

    Version - Stores the generic netlink family structure version. Set to
        NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION.

    Id - Stores the generic netlink family's ID. Set to zero upon registration
        to have the netlink core allocate an ID.

    Name - Stores the name of the generic family.

    Interface - Stores the interface presented to the networking core for this
        generic netlink family.

--*/

typedef struct _NETLINK_GENERIC_FAMILY_PROPERTIES {
    ULONG Version;
    ULONG Id;
    CHAR Name[NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH];
    NETLINK_GENERIC_FAMILY_INTERFACE Interface;
} NETLINK_GENERIC_FAMILY_PROPERTIES, *PNETLINK_GENERIC_FAMILY_PROPERTIES;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

NET_API
KSTATUS
NetNetlinkGenericRegisterFamily (
    PNETLINK_GENERIC_FAMILY_PROPERTIES Properties,
    PHANDLE FamilyHandle
    );

/*++

Routine Description:

    This routine registers a generic netlink family with the generic netlink
    core. The core will route messages with a message type equal to the
    family's ID to the provided interface.

Arguments:

    Properties - Supplies a pointer to the family properties. The netlink
        library  will not reference this memory after the function returns, a
        copy will be made.

    FamilyHandle - Supplies an optional pointer that receives a handle to the
        registered family.

Return Value:

    Status code.

--*/

NET_API
VOID
NetNetlinkGenericUnregisterFamily (
    PHANDLE FamilyHandle
    );

/*++

Routine Description:

    This routine unregisters the given generic netlink family.

Arguments:

    FamilyHandle - Supplies a pointer to the generic netlink family to
        unregister.

Return Value:

    None.

--*/

