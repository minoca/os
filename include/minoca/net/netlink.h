/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#define NETLINK_ALIGN(_Length) ALIGN_RANGE_UP(_Length, NETLINK_ALIGNMENT)

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
// This macro returns the total size, in bytes, consumed by a netlink
// attribute with the given data length, accounting for alignment.
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
// This macro determines the index into an socket's multicast bitmap array for
// a given multicast group ID.
//

#define NETLINK_SOCKET_BITMAP_INDEX(_GroupId) \
    ((_GroupId) / (sizeof(ULONG) * BITS_PER_BYTE))

//
// This macro determines in the mask for a particular group ID within an
// netlink socket's multicast bitmap.
//

#define NETLINK_SOCKET_BITMAP_MASK(_GroupId) \
    (1 << ((_GroupId) % (sizeof(ULONG) * BITS_PER_BYTE)))

//
// This macro determines the number of group IDs that the socket multicast
// bitmap currently supports.
//

#define NETLINK_SOCKET_BITMAP_GROUP_ID_COUNT(_Socket) \
    ((_Socket)->MulticastBitmapSize * BITS_PER_BYTE)

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

#define NETLINK_HEADER_FLAG_REQUEST   0x0001
#define NETLINK_HEADER_FLAG_MULTIPART 0x0002
#define NETLINK_HEADER_FLAG_ACK       0x0004
#define NETLINK_HEADER_FLAG_ECHO      0x0008
#define NETLINK_HEADER_FLAG_ROOT      0x0010
#define NETLINK_HEADER_FLAG_MATCH     0x0020
#define NETLINK_HEADER_FLAG_ATOMIC    0x0040

#define NETLINK_HEADER_FLAG_DUMP \
    (NETLINK_HEADER_FLAG_ROOT | NETLINK_HEADER_FLAG_MATCH)

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
// Define the maximum length of a multicast group name.
//

#define NETLINK_GENERIC_MAX_MULTICAST_GROUP_NAME 16

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

#define NETLINK_CONTROL_COMMAND_NEW_FAMILY 1
#define NETLINK_CONTROL_COMMAND_DELETE_FAMILY 2
#define NETLINK_CONTROL_COMMAND_GET_FAMILY 3
#define NETLINK_CONTROL_COMMAND_NEW_MULTICAST_GROUP 7
#define NETLINK_CONTROL_COMMAND_DELETE_MULTICAST_GROUP 8
#define NETLINK_CONTROL_COMMAND_MAX 255

//
// Define the generic control attributes.
//

#define NETLINK_CONTROL_ATTRIBUTE_FAMILY_ID 1
#define NETLINK_CONTROL_ATTRIBUTE_FAMILY_NAME 2
#define NETLINK_CONTROL_ATTRIBUTE_VERSION 3
#define NETLINK_CONTROL_ATTRIBUTE_HEADER_SIZE 4
#define NETLINK_CONTROL_ATTRIBUTE_MAX_ATTRIBUTE 5
#define NETLINK_CONTROL_ATTRIBUTE_OPERATIONS 6
#define NETLINK_CONTROL_ATTRIBUTE_MULTICAST_GROUPS 7

//
// Define the generic control multicast group names.
//

#define NETLINK_CONTROL_MULTICAST_NOTIFY_NAME "notify"

//
// Define the generic multicast group attributes.
//

#define NETLINK_CONTROL_MULTICAST_GROUP_ATTRIBUTE_NAME 1
#define NETLINK_CONTROL_MULTICAST_GROUP_ATTRIBUTE_ID 2

//
// Define the generic 802.11 command values.
//

#define NETLINK_80211_COMMAND_JOIN 1
#define NETLINK_80211_COMMAND_LEAVE 2
#define NETLINK_80211_COMMAND_SCAN_START 3
#define NETLINK_80211_COMMAND_SCAN_RESULT 4
#define NETLINK_80211_COMMAND_SCAN_GET_RESULTS 5
#define NETLINK_80211_COMMAND_SCAN_ABORTED 6
#define NETLINK_80211_COMMAND_MAX 255

//
// Define the generic 802.11 attributes.
//

#define NETLINK_80211_ATTRIBUTE_DEVICE_ID 1
#define NETLINK_80211_ATTRIBUTE_SSID 2
#define NETLINK_80211_ATTRIBUTE_BSSID 3
#define NETLINK_80211_ATTRIBUTE_PASSPHRASE 4
#define NETLINK_80211_ATTRIBUTE_BSS 5

//
// Define the 802.11 BSS attributes.
//

#define NETLINK_80211_BSS_ATTRIBUTE_BSSID 1
#define NETLINK_80211_BSS_ATTRIBUTE_CAPABILITY 2
#define NETLINK_80211_BSS_ATTRIBUTE_BEACON_INTERVAL 3
#define NETLINK_80211_BSS_ATTRIBUTE_SIGNAL_MBM 4
#define NETLINK_80211_BSS_ATTRIBUTE_STATUS 5
#define NETLINK_80211_BSS_ATTRIBUTE_INFORMATION_ELEMENTS 6

//
// Define the status values for the BSS status attribute.
//

#define NETLINK_80211_BSS_STATUS_NOT_CONNECTED 0
#define NETLINK_80211_BSS_STATUS_AUTHENTICATED 1
#define NETLINK_80211_BSS_STATUS_ASSOCIATED 2

//
// Define the generic 802.11 multicast group names.
//

#define NETLINK_80211_MULTICAST_SCAN_NAME "scan"

//
// ------------------------------------------------------ Data Type Definitions
//

#define NETLINK_API NET_API

typedef struct _NETLINK_GENERIC_FAMILY
    NETLINK_GENERIC_FAMILY, *PNETLINK_GENERIC_FAMILY;

/*++

Structure Description:

    This structure defines an netlink address.

Members:

    Domain - Stores the network domain of this address.

    Port - Stores the 32 bit port ID.

    Group - Stores the 32 bit group ID.

    NetworkAddress - Stores the unioned opaque version, used to ensure the
        structure is the proper size.

--*/

typedef struct _NETLINK_ADDRESS {
    union {
        struct {
            NET_DOMAIN_TYPE Domain;
            ULONG Port;
            ULONG Group;
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

#pragma pack(push, 1)

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
        The rest of the message payload follows the header.

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

#pragma pack(pop)

/*++

Structure Description:

    This structure defines the already parsed information for the message.

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

typedef struct _NETLINK_MESSAGE_INFORMATION {
    PNETWORK_ADDRESS SourceAddress;
    PNETWORK_ADDRESS DestinationAddress;
    ULONG SequenceNumber;
    USHORT Type;
} NETLINK_MESSAGE_INFORMATION, *PNETLINK_MESSAGE_INFORMATION;

/*++

Structure Description:

    This structure defines the already parsed information for the command.

Members:

    Message - Stores the base message parameters.

    Command - Stores the generic command value.

    Version - Stores the generic command version.

--*/

typedef struct _NETLINK_GENERIC_COMMAND_INFORMATION {
    NETLINK_MESSAGE_INFORMATION Message;
    UCHAR Command;
    UCHAR Version;
} NETLINK_GENERIC_COMMAND_INFORMATION, *PNETLINK_GENERIC_COMMAND_INFORMATION;

typedef
KSTATUS
(*PNETLINK_GENERIC_PROCESS_COMMAND) (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETLINK_GENERIC_COMMAND_INFORMATION Command
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

    Command - Supplies a pointer to the command information.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines a netlink generic command.

Members:

    CommandId - Stores the command ID value. This should match the generic
        netlink header values for the command's family.

    RequiredFlags - Stores a bitmask of flags that must be set in the
        requesting netlink message for this command to be processed.

    ProcessCommand - Stores a pointer to a function called when a packet of
        this command type is received by a generic netlink socket.

--*/

typedef struct _NETLINK_GENERIC_COMMAND {
    UCHAR CommandId;
    USHORT RequiredFlags;
    PNETLINK_GENERIC_PROCESS_COMMAND ProcessCommand;
} NETLINK_GENERIC_COMMAND, *PNETLINK_GENERIC_COMMAND;

/*++

Structure Description:

    This structure defines a generic netlink multicast group. The group's ID is
    dynamic and is based off the family's assigned group ID offset.

Members:

    Id - Stores the ID of the multicast group.

    NameLength - Stores the length of the multicast group name, in bytes.

    Name - Stores the name of the multicast group.

--*/

typedef struct _NETLINK_GENERIC_MULTICAST_GROUP {
    ULONG Id;
    ULONG NameLength;
    CHAR Name[NETLINK_GENERIC_MAX_MULTICAST_GROUP_NAME];
} NETLINK_GENERIC_MULTICAST_GROUP, *PNETLINK_GENERIC_MULTICAST_GROUP;

/*++

Structure Description:

    This structure defines a generic netlink family properties.

Members:

    Version - Stores the generic netlink family structure version. Set to
        NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION.

    Id - Stores the generic netlink family's ID. Set to zero upon registration
        to have the netlink core allocate an ID.

    NameLength - Stores the length of the family name, in bytes.

    Name - Stores the name of the generic family.

    Commands - Stores a pointer to an array of netlink generic commands.

    CommandCount - Stores the number of commands in the array.

    MulticastGroups - Stores a pointer to an array of multicast groups.

    MulticastGroupCount - Stores the number of multicast groups in the array.

--*/

typedef struct _NETLINK_GENERIC_FAMILY_PROPERTIES {
    ULONG Version;
    ULONG Id;
    ULONG NameLength;
    CHAR Name[NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH];
    PNETLINK_GENERIC_COMMAND Commands;
    ULONG CommandCount;
    PNETLINK_GENERIC_MULTICAST_GROUP MulticastGroups;
    ULONG MulticastGroupCount;
} NETLINK_GENERIC_FAMILY_PROPERTIES, *PNETLINK_GENERIC_FAMILY_PROPERTIES;

typedef
KSTATUS
(*PNETLINK_PROTOCOL_JOIN_MULTICAST_GROUP) (
    PNET_SOCKET Socket,
    ULONG GroupId
    );

/*++

Routine Description:

    This routine attempts to join the given multicast group by validating the
    group ID for the protocol and then joining the multicast group.

Arguments:

    Socket - Supplies a pointer to the network socket requesting to join a
        multicast group.

    GroupId - Supplies the ID of the multicast group to join.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines the protocol layer interface specific to netlink
    sockets.

Members:

    JoinMulticastGroup - Supplies a pointer to a function used to join a
        multicast group.

--*/

typedef struct _NETLINK_PROTOCOL_INTERFACE {
    PNETLINK_PROTOCOL_JOIN_MULTICAST_GROUP JoinMulticastGroup;
} NETLINK_PROTOCOL_INTERFACE, *PNETLINK_PROTOCOL_INTERFACE;

/*++

Structure Description:

    This structure defines a netlink socket.

Members:

    NetSocket - Stores the common core networking parameters.

    MulticastListEntry - Stores the socket's entry into the list of sockets
        signed up for at least one multicast group.

    MulticastBitmap - Stores a pointer to bitmap describing the multicast
        groups to which the socket belongs.

    MulticastBitmapSize - Stores the size of the multicast bitmap, in bytes.

    MulticastGroupCount - Stores the number of multicast groups to which the
        socket is joined.

    ProtocolInterface - Stores the interface presented to the netlink network
        layer for this type of netlink socket.

--*/

typedef struct _NETLINK_SOCKET {
    NET_SOCKET NetSocket;
    LIST_ENTRY MulticastListEntry;
    PULONG MulticastBitmap;
    ULONG MulticastBitmapSize;
    ULONG MulticastGroupCount;
    NETLINK_PROTOCOL_INTERFACE ProtocolInterface;
} NETLINK_SOCKET, *PNETLINK_SOCKET;

/*++

Enumeration Description:

    This enumeration describes the various socket options for the basic socket
    information class.

Values:

    NetlinkSocketOptionInvalid - Indicates an invalid option.

    NetlinkSocketOptionJoinMulticastGroup - Indicates that the socket intends
        to join a multicast group.

    NetlinkSocketOptionLeaveMulticastGroup - Indicates that the socket intends
        to leave a multicast group.

--*/

typedef enum _NETLINK_SOCKET_OPTION {
    NetlinkSocketOptionInvalid,
    NetlinkSocketOptionJoinMulticastGroup,
    NetlinkSocketOptionLeaveMulticastGroup
} NETLINK_SOCKET_OPTION, *PNETLINK_SOCKET_OPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

NETLINK_API
KSTATUS
NetlinkSendMessage (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress
    );

/*++

Routine Description:

    This routine sends a netlink message to the given destination address. The
    caller should have already filled the buffer with the netlink header.

Arguments:

    Socket - Supplies a pointer to the netlink socket over which to send the
        message.

    Packet - Supplies a pointer to the network packet to be sent.

    DestinationAddress - Supplies a pointer to the destination address to which
        the message will be sent.

Return Value:

    Status code.

--*/

NETLINK_API
KSTATUS
NetlinkSendMultipartMessage (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress,
    ULONG SequenceNumber
    );

/*++

Routine Description:

    This routine sends a multipart message packet. It will append the final
    DONE message, which the packet must have space for, reset the packet's data
    offset to the beginning and then send the entire packet off to the
    destination address.

Arguments:

    Socket - Supplies a pointer to the network socket from which the packet
        will be sent.

    Packet - Supplies a pointer to the network packet to send.

    DestinationAddress - Supplies a pointer to the network address to which the
        packet will be sent.

    SequenceNumber - Supplies the sequence number to set in the header of the
        DONE message that is appended to the packet.

Return Value:

    Status code.

--*/

NETLINK_API
KSTATUS
NetlinkAppendHeader (
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    ULONG Length,
    ULONG SequenceNumber,
    USHORT Type,
    USHORT Flags
    );

/*++

Routine Description:

    This routine appends a base netlink header to the given network packet. It
    validates if there is enough space remaining in the packet and moves the
    data offset forwards to the first byte after the header on success.

Arguments:

    Socket - Supplies a pointer to the socket that will send the packet. The
        header's port ID is taken from the socket's local address.

    Packet - Supplies a pointer to the network packet to which a base netlink
        header will be added.

    Length - Supplies the length of the netlink message, not including the
        header.

    SequenceNumber - Supplies the desired sequence number for the netlink
        message.

    Type - Supplies the message type to be set in the header.

    Flags - Supplies a bitmask of netlink message flags to be set. See
        NETLINK_HEADER_FLAG_* for definitions.

Return Value:

    Status code.

--*/

NETLINK_API
KSTATUS
NetlinkAppendAttribute (
    PNET_PACKET_BUFFER Packet,
    USHORT Type,
    PVOID Data,
    USHORT DataLength
    );

/*++

Routine Description:

    This routine appends a netlink attribute to the given network packet. It
    validates that there is enough space for the attribute and moves the
    packet's data offset to the first byte after the attribute. The exception
    to this rule is if a NULL data buffer is supplied; the packet's data offset
    is only moved to the first byte after the attribute header.

Arguments:

    Packet - Supplies a pointer to the network packet to which the attribute
        will be added.

    Type - Supplies the netlink attribute type.

    Data - Supplies an optional pointer to the attribute data to be stored in
        the network packet. Even if no data buffer is supplied, a data length
        may be supplied for the case of child attributes that are yet to be
        appended.

    DataLength - Supplies the length of the data, in bytes.

Return Value:

    Status code.

--*/

NETLINK_API
KSTATUS
NetlinkGetAttribute (
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

    Status code.

--*/

NETLINK_API
KSTATUS
NetlinkJoinMulticastGroup (
    PNET_SOCKET Socket,
    ULONG GroupId
    );

/*++

Routine Description:

    This routine joins a socket to a multicast group by updating the socket's
    multicast group bitmap and adding the socket to the global list of socket's
    joined to multicast groups.

Arguments:

    Socket - Supplies a pointer to the socket that is requesting to join a
        multicast group.

    GroupId - Supplies the ID of the multicast group to join.

Return Value:

    Status code.

--*/

NETLINK_API
VOID
NetlinkRemoveSocketsFromMulticastGroups (
    ULONG ParentProtocolNumber,
    ULONG GroupOffset,
    ULONG GroupCount
    );

/*++

Routine Description:

    This routine removes any socket listening for multicast message from the
    groups specified by the offset and count. It will only match sockets for
    the given protocol.

Arguments:

    ParentProtocolNumber - Supplies the protocol number of the protocol that
        owns the given range of multicast groups.

    GroupOffset - Supplies the offset into the multicast namespace for the
        range of multicast groups from which the sockets should be removed.

    GroupCount - Supplies the number of multicast groups from which the sockets
        should be removed.

Return Value:

    None.

--*/

NETLINK_API
KSTATUS
NetlinkGenericRegisterFamily (
    PNETLINK_GENERIC_FAMILY_PROPERTIES Properties,
    PNETLINK_GENERIC_FAMILY *Family
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

    Family - Supplies an optional pointer that receives a handle to the
        registered family.

Return Value:

    Status code.

--*/

NETLINK_API
VOID
NetlinkGenericUnregisterFamily (
    PNETLINK_GENERIC_FAMILY Family
    );

/*++

Routine Description:

    This routine unregisters the given generic netlink family.

Arguments:

    Family - Supplies a pointer to the generic netlink family to unregister.

Return Value:

    None.

--*/

NETLINK_API
KSTATUS
NetlinkGenericSendCommand (
    PNETLINK_GENERIC_FAMILY Family,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress
    );

/*++

Routine Description:

    This routine sends a generic netlink command. The generic header should
    already be filled out.

Arguments:

    Family - Supplies a pointer to the generic netlink family sending the
        command.

    Packet - Supplies a pointer to the network packet to be sent.

    DestinationAddress - Supplies a pointer to the destination address to which
        the command will be sent.

Return Value:

    Status code.

--*/

NETLINK_API
KSTATUS
NetlinkGenericSendMulticastCommand (
    PNETLINK_GENERIC_FAMILY Family,
    PNET_PACKET_BUFFER Packet,
    ULONG GroupId
    );

/*++

Routine Description:

    This routine multicasts the given packet to the specified group after
    filling its generic header and base netlink header in with the given
    command and information stored in the family structure.

Arguments:

    Family - Supplies a pointer to the generic netlink family sending the
        multicast command.

    Packet - Supplies a pointer to the network packet to be sent.

    GroupId - Supplies the family's multicast group ID over which to send the
        command.

Return Value:

    Status code.

--*/

NETLINK_API
KSTATUS
NetlinkGenericAppendHeaders (
    PNETLINK_GENERIC_FAMILY Family,
    PNET_PACKET_BUFFER Packet,
    ULONG Length,
    ULONG SequenceNumber,
    USHORT Flags,
    UCHAR Command,
    UCHAR Version
    );

/*++

Routine Description:

    This routine appends the base and generic netlink headers to the given
    packet, validating that there is enough space remaining in the buffer and
    moving the data offset forward to the first byte after the headers once
    they have been added.

Arguments:

    Family - Supplies a pointer to the netlink generic family to which the
        packet belongs.

    Packet - Supplies a pointer to the network packet to which the headers will
        be appended.

    Length - Supplies the length of the generic command payload, not including
        any headers.

    SequenceNumber - Supplies the desired sequence number for the netlink
        message.

    Flags - Supplies a bitmask of netlink message flags to be set. See
        NETLINK_HEADER_FLAG_* for definitions.

    Command - Supplies the generic netlink command to bet set in the header.

    Version - Supplies the version number of the command.

Return Value:

    Status code.

--*/

