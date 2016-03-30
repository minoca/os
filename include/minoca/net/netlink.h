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
// --------------------------------------------------------------------- Macros
//

//
// This macro returns the required alignment for a given length. All headers,
// attributes, and messages must be aligned.
//

#define NETLINK_ALIGN(_Length) \
    (((_Length) + NETLINK_ALIGNMENT - 1) & ~(NETLINK_ALIGNMENT - 1))

//
// This macro returns the aligned size of a netlink message header.
//

#define NETLINK_HEADER_LENGTH NETLINK_ALIGN(sizeof(NETLINK_HEADER))

//
// This macro evaluates to a pointer to the ancillary data following a netlink
// header structure.
//

#define NETLINK_DATA(_Header) ((PVOID)(_Header) + NETLINK_HEADER_LENGTH)

//
// This macro returns the aligned aize of a netlink message header.
//

#define NETLINK_ATTRIBUTE_HEADER_LENGTH NETLINK_ALIGN(sizeof(NETLINK_ATTRIBUTE))

//
// This macro evaluates to a pointer to the ancillary data following a netlink
// attribute header structure.
//

#define NETLINK_ATTRIBUTE_DATA(_Header) \
    ((PVOID)(_Header) + NETLINK_ATTRIBUTE_HEADER_LENGTH)

//
// This macro returns the length of the netlink attribute, based on the data
// length, that should be set in the attribute header.
//

#define NETLINK_ATTRIBUTE_LENGTH(_DataLength) \
    (NETLINK_ATTRIBUTE_HEADER_LENGTH + (_DataLength))

//
// This macro returns the total size, in bytes, consumed by the netlink
// attribute, accounting for alignment.
//

#define NETLINK_ATTRIBUTE_SIZE(_DataLength) \
    NETLINK_ALIGN(NETLINK_ATTRIBUTE_LENGTH(_DataLength))

//
// This macro returns the aligned size of the generic netlink message header.
//

#define NETLINK_GENERIC_HEADER_LENGTH \
    NETLINK_ALIGN(sizeof(NETLINK_GENERIC_HEADER))

//
// This macro evaluates to a pointer to the ancillary data following a netlink
// generic header structure.
//

#define NETLINK_GENERIC_DATA(_Header) \
    ((PVOID)(_Header) + NETLINK_GENERIC_HEADER_LENGTH)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the current version number of netlink properties structure.
//

#define NETLINK_PROPERTIES_VERSION 1

//
// Define the alignment for all netlink messages, message headers, and message
// attributes.
//

#define NETLINK_ALIGNMENT 4

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
// Define the port ID of the kernel.
//

#define NETLINK_KERNEL_PORT_ID 0

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
// Define the names of the netlink generic families.
//

#define NETLINK_GENERIC_CONTROL_NAME "nlctrl"
#define NETLINK_GENERIC_80211_NAME   "nl80211"

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

/*++

Structure Description:

    This structure defines the parameters to use when sending a netlink message
    or parsing a received message.

Members:

    SourceAddress - Stores a pointer to the source address for the command.
        This memory will not be referenced once the function returns; it can be
        stack allocated.

    DestinationAddress - Stores a pointer to the destination address for the
        command. This memory will not be referenced once the function returns;
        it can be stack allocated.

    SequenceNumber - Stores the sequence number of the command.

    Type - Stores the netlink message type.

--*/

typedef struct _NETLINK_MESSAGE_PARAMETERS {
    PNETWORK_ADDRESS SourceAddress;
    PNETWORK_ADDRESS DestinationAddress;
    ULONG SequenceNumber;
    USHORT Type;
} NETLINK_MESSAGE_PARAMETERS, *PNETLINK_MESSAGE_PARAMETERS;

/*++

Structure Description:

    This structure defines the parameters to use when sending a generic command
    or parsing a received command.

Members:

    Message - Stores the base message parameters.

    Command - Stores the generic command value.

    Version - Stores the generic command version.

--*/

typedef struct _NETLINK_GENERIC_COMMAND_PARAMETERS {
    NETLINK_MESSAGE_PARAMETERS Message;
    UCHAR Command;
    UCHAR Version;
} NETLINK_GENERIC_COMMAND_PARAMETERS, *PNETLINK_GENERIC_COMMAND_PARAMETERS;

typedef
KSTATUS
(*PNETLINK_GENERIC_PROCESS_COMMAND) (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
    );

/*++

Routine Description:

    This routine is called to process a received generic netlink packet for
    a given command type.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    Parameters - Supplies a pointer to the command parameters.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines a netlink generic command.

Members:

    CommandId - Stores the command ID value. This should match the generic
        netlink header values for the command's family.

    ProcessCommand - Stores a pointer to a function called when a packet of
        this command type is received by a generic netlink socket.

--*/

typedef struct _NETLINK_GENERIC_COMMAND {
    UCHAR CommandId;
    PNETLINK_GENERIC_PROCESS_COMMAND ProcessCommand;
} NETLINK_GENERIC_COMMAND, *PNETLINK_GENERIC_COMMAND;

/*++

Structure Description:

    This structure defines a generic netlink family properties.

Members:

    Version - Stores the generic netlink family structure version. Set to
        NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION.

    Id - Stores the generic netlink family's ID. Set to zero upon registration
        to have the netlink core allocate an ID.

    Name - Stores the name of the generic family.

    Commands - Stores a pointer to an array of netlink generic commands.

    CommandCount - Stores the number of commands in the array.

--*/

typedef struct _NETLINK_GENERIC_FAMILY_PROPERTIES {
    ULONG Version;
    ULONG Id;
    CHAR Name[NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH];
    PNETLINK_GENERIC_COMMAND Commands;
    ULONG CommandCount;
} NETLINK_GENERIC_FAMILY_PROPERTIES, *PNETLINK_GENERIC_FAMILY_PROPERTIES;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

NET_API
KSTATUS
NetNetlinkSendMessage (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_MESSAGE_PARAMETERS Parameters
    );

/*++

Routine Description:

    This routine sends a netlink message, filling out the header based on the
    parameters.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to send the
        command.

    Packet - Supplies a pointer to the network packet to be sent.

    Parameters - Supplies a pointer to the message parameters.

Return Value:

    Status code.

--*/

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

NET_API
KSTATUS
NetNetlinkGenericSendCommand (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_PARAMETERS Parameters
    );

/*++

Routine Description:

    This routine sends a generic netlink command, filling out the generic
    header and netlink header.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to send the
        command.

    Packet - Supplies a pointer to the network packet to be sent.

    Parameters - Supplies a pointer to the generic command parameters.

Return Value:

    Status code.

--*/

