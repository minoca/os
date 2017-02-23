/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    generic.c

Abstract:

    This module implements the generic netlink socket protocol.

Author:

    Chris Stevens 9-Feb-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Protocol drivers are supposed to be able to stand on their own (ie be able to
// be implemented outside the core net library). For the builtin ones, avoid
// including netcore.h, but still redefine those functions that would otherwise
// generate imports.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/netlink.h>
#include "generic.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum supported packet size of the generic netlink protocol,
// including the message headers.
//

#define NETLINK_GENERIC_MAX_PACKET_SIZE \
    (NETLINK_MAX_PACKET_SIZE - sizeof(NETLINK_HEADER))

//
// Define the default size of the generic netlink's receive data buffer, in
// bytes.
//

#define NETLINK_GENERIC_DEFAULT_RECEIVE_BUFFER_SIZE (256 * _1KB)

//
// Define the minimum receive buffer size.
//

#define NETLINK_GENERIC_MIN_RECEIVE_BUFFER_SIZE _2KB

//
// Define the default minimum number of bytes necessary for the generic netlink
// socket to become readable.
//

#define NETLINK_GENERIC_DEFAULT_RECEIVE_MINIMUM 1

//
// Define the minmum number of bytes necessary for generic netlink sockets to
// become writable. There is no minimum and bytes are immediately sent on the
// wire.
//

#define NETLINK_GENERIC_SEND_MINIMUM 1

//
// Define the default set of protocol flags for the netlink generic protocol.
//

#define NETLINK_GENERIC_DEFAULT_PROTOCOL_FLAGS \
    NET_PROTOCOL_FLAG_UNICAST_ONLY |           \
    NET_PROTOCOL_FLAG_NO_BIND_PERMISSIONS

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a generic netlink socket.

Members:

    NetlinkSocket - Stores the common netlink socket information.

    ReceivedPacketList - Stores the list of packets ready to be read by the
        user.

    ReceiveLock - Stores the lock that protects the received packets list,
        dropped packet count, and various receive buffer parameters. This lock
        must always be acquired at low level.

    ReceiveBufferTotalSize - Stores the total size of the receive buffer, in
        bytes. Packets that are received but will not fit in the buffer are
        discarded.

    ReceiveBufferFreeSize - Stores the receive buffer's free space, in bytes.
        Packets that are received but do not fit in the free space are
        discarded.

    ReceiveTimeout - Stores the maximum amount of time, in milliseconds, that
        the  socket will wait when receiving data.

    ReceiveMinimum - Stores the minimum amount of bytes that must be available
        before the socket is made readable. This is ignored.

    DroppedPacketCount - Stores the number of packets that have been dropped
        because the receive queue was full.

    MaxPacketSize - Stores the maximum size of UDP datagrams.

--*/

typedef struct _NETLINK_GENERIC_SOCKET {
    NETLINK_SOCKET NetlinkSocket;
    LIST_ENTRY ReceivedPacketList;
    PQUEUED_LOCK ReceiveLock;
    ULONG ReceiveBufferTotalSize;
    ULONG ReceiveBufferFreeSize;
    ULONG ReceiveTimeout;
    ULONG ReceiveMinimum;
    ULONG DroppedPacketCount;
    UINTN MaxPacketSize;
} NETLINK_GENERIC_SOCKET, *PNETLINK_GENERIC_SOCKET;

/*++

Structure Description:

    This structure defines a generic netlink received message.

Members:

    ListEntry - Stores pointers to the next and previous packets.

    Address - Stores the network address where this data came from.

    NetPacket - Stores a pointer to the network packet buffer holding the data.

--*/

typedef struct _NETLINK_GENERIC_RECEIVED_PACKET {
    LIST_ENTRY ListEntry;
    NETWORK_ADDRESS Address;
    PNET_PACKET_BUFFER NetPacket;
} NETLINK_GENERIC_RECEIVED_PACKET, *PNETLINK_GENERIC_RECEIVED_PACKET;

/*++

Structure Description:

    This structure defines a netlink generic socket option.

Members:

    InformationType - Stores the information type for the socket option.

    Option - Stores the type-specific option identifier.

    Size - Stores the size of the option value, in bytes.

    SetAllowed - Stores a boolean indicating whether or not the option is
        allowed to be set.

--*/

typedef struct _NETLINK_GENERIC_SOCKET_OPTION {
    SOCKET_INFORMATION_TYPE InformationType;
    UINTN Option;
    UINTN Size;
    BOOL SetAllowed;
} NETLINK_GENERIC_SOCKET_OPTION, *PNETLINK_GENERIC_SOCKET_OPTION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetlinkpGenericCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    );

VOID
NetlinkpGenericDestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetlinkpGenericBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetlinkpGenericListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetlinkpGenericAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    );

KSTATUS
NetlinkpGenericConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetlinkpGenericClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetlinkpGenericShutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    );

KSTATUS
NetlinkpGenericSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

VOID
NetlinkpGenericProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetlinkpGenericProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetlinkpGenericReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetlinkpGenericGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetlinkpGenericUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

KSTATUS
NetlinkpGenericJoinMulticastGroup (
    PNET_SOCKET Socket,
    ULONG GroupId
    );

VOID
NetlinkpGenericInsertReceivedPacket (
    PNETLINK_GENERIC_SOCKET Socket,
    PNETLINK_GENERIC_RECEIVED_PACKET Packet
    );

KSTATUS
NetlinkpGenericProcessReceivedKernelData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

PNETLINK_GENERIC_FAMILY
NetlinkpGenericLookupFamilyById (
    ULONG MessageType
    );

COMPARISON_RESULT
NetlinkpGenericCompareFamilies (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

VOID
NetlinkpGenericDestroyFamily (
    PNETLINK_GENERIC_FAMILY Family
    );

KSTATUS
NetlinkpGenericAllocateFamilyId (
    PULONG FamilyId
    );

KSTATUS
NetlinkpGenericAllocateMulticastGroups (
    PNETLINK_GENERIC_FAMILY Family
    );

VOID
NetlinkpGenericFreeMulticastGroups (
    PNETLINK_GENERIC_FAMILY Family
    );

KSTATUS
NetlinkpGenericValidateMulticastGroup (
    ULONG GroupId,
    BOOL LockHeld
    );

//
// -------------------------------------------------------------------- Globals
//

NET_PROTOCOL_ENTRY NetlinkGenericProtocol = {
    {NULL, NULL},
    NetSocketDatagram,
    SOCKET_INTERNET_PROTOCOL_NETLINK_GENERIC,
    NETLINK_GENERIC_DEFAULT_PROTOCOL_FLAGS,
    NULL,
    NULL,
    {{0}, {0}, {0}},
    {
        NetlinkpGenericCreateSocket,
        NetlinkpGenericDestroySocket,
        NetlinkpGenericBindToAddress,
        NetlinkpGenericListen,
        NetlinkpGenericAccept,
        NetlinkpGenericConnect,
        NetlinkpGenericClose,
        NetlinkpGenericShutdown,
        NetlinkpGenericSend,
        NetlinkpGenericProcessReceivedData,
        NetlinkpGenericProcessReceivedSocketData,
        NetlinkpGenericReceive,
        NetlinkpGenericGetSetInformation,
        NetlinkpGenericUserControl
    }
};

NETLINK_GENERIC_SOCKET_OPTION NetlinkGenericSocketOptions[] = {
    {
        SocketInformationBasic,
        SocketBasicOptionSendBufferSize,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionSendMinimum,
        sizeof(ULONG),
        FALSE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveBufferSize,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveMinimum,
        sizeof(ULONG),
        TRUE
    },

    {
        SocketInformationBasic,
        SocketBasicOptionReceiveTimeout,
        sizeof(SOCKET_TIME),
        TRUE
    },
};

NETLINK_PROTOCOL_INTERFACE NetlinkGenericProtocolInterface = {
    NetlinkpGenericJoinMulticastGroup
};

PIO_HANDLE NetlinkGenericSocketHandle;
PNET_SOCKET NetlinkGenericSocket;

//
// Store the lock and tree for storing the generic netlink families.
//

PSHARED_EXCLUSIVE_LOCK NetlinkGenericFamilyLock;
RED_BLACK_TREE NetlinkGenericFamilyTree;

//
// Store the next generic family message type to allocate.
//

ULONG NetlinkGenericFamilyNextId = NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM;

//
// Store a pointer to the multicast group bitmap and its size, in bytes.
//

PULONG NetlinkGenericMulticastBitmap = NULL;
ULONG NetlinkGenericMulticastBitmapSize = 0;

//
// ------------------------------------------------------------------ Functions
//

NETLINK_API
KSTATUS
NetlinkGenericRegisterFamily (
    PNETLINK_GENERIC_FAMILY_PROPERTIES Properties,
    PNETLINK_GENERIC_FAMILY *Family
    )

/*++

Routine Description:

    This routine registers a generic netlink family with the generic netlink
    core. The core will route messages with a message type equal to the
    family's ID to the provided interface.

Arguments:

    Properties - Supplies a pointer to the family properties. The netlink
        library will not reference this memory after the function returns, a
        copy will be made.

    Family - Supplies an optional pointer that receives a pointer to the
        registered family.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    UCHAR Command;
    ULONG CommandSize;
    PNETLINK_GENERIC_FAMILY FoundFamily;
    PRED_BLACK_TREE_NODE FoundNode;
    PNETLINK_GENERIC_MULTICAST_GROUP Group;
    ULONG Index;
    BOOL LockHeld;
    BOOL Match;
    ULONG MulticastSize;
    PNETLINK_GENERIC_FAMILY NewFamily;
    ULONG NewId;
    KSTATUS Status;

    LockHeld = FALSE;
    NewFamily = NULL;
    if (Properties->Version < NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION) {
        Status = STATUS_VERSION_MISMATCH;
        goto RegisterFamilyEnd;
    }

    if ((Properties->CommandCount == 0) || (Properties->Commands == NULL)) {
        Status = STATUS_INVALID_PARAMETER;
        goto RegisterFamilyEnd;
    }

    if ((Properties->NameLength == 1) ||
        (Properties->NameLength > NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH)) {

        Status = STATUS_INVALID_PARAMETER;
        goto RegisterFamilyEnd;
    }

    if ((Properties->Id < NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM) &&
        (Properties->Id != 0)) {

        Status = STATUS_INVALID_PARAMETER;
        goto RegisterFamilyEnd;
    }

    //
    // Allocate and initialize the new generic netlink family.
    //

    CommandSize = Properties->CommandCount * sizeof(NETLINK_GENERIC_COMMAND);
    MulticastSize = Properties->MulticastGroupCount *
                    sizeof(NETLINK_GENERIC_MULTICAST_GROUP);

    AllocationSize = sizeof(NETLINK_GENERIC_FAMILY) +
                     CommandSize +
                     MulticastSize;

    NewFamily = MmAllocatePagedPool(AllocationSize,
                                    NETLINK_GENERIC_ALLOCATION_TAG);

    if (NewFamily == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RegisterFamilyEnd;
    }

    RtlZeroMemory(NewFamily, sizeof(NETLINK_GENERIC_FAMILY));
    NewFamily->ReferenceCount = 1;
    RtlCopyMemory(&(NewFamily->Properties),
                  Properties,
                  sizeof(NETLINK_GENERIC_FAMILY_PROPERTIES));

    NewFamily->Properties.Commands = (PNETLINK_GENERIC_COMMAND)(NewFamily + 1);
    RtlCopyMemory(NewFamily->Properties.Commands,
                  Properties->Commands,
                  CommandSize);

    NewFamily->Properties.MulticastGroups = (PVOID)(NewFamily + 1) +
                                            CommandSize;

    RtlCopyMemory(NewFamily->Properties.MulticastGroups,
                  Properties->MulticastGroups,
                  MulticastSize);

    //
    // Acquire the family tree lock and attempt to insert this new family.
    //

    KeAcquireSharedExclusiveLockExclusive(NetlinkGenericFamilyLock);
    LockHeld = TRUE;

    //
    // Check to make sure the name is not a duplicate.
    //

    FoundNode = RtlRedBlackTreeGetLowestNode(&NetlinkGenericFamilyTree);
    while (FoundNode != NULL) {
        FoundFamily = RED_BLACK_TREE_VALUE(FoundNode,
                                           NETLINK_GENERIC_FAMILY,
                                           TreeNode);

        Match = RtlAreStringsEqual(FoundFamily->Properties.Name,
                                   NewFamily->Properties.Name,
                                   NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH);

        if (Match != FALSE) {
            Status = STATUS_DUPLICATE_ENTRY;
            goto RegisterFamilyEnd;
        }

        FoundNode = RtlRedBlackTreeGetNextNode(&NetlinkGenericFamilyTree,
                                               FALSE,
                                               FoundNode);
    }

    //
    // If the message type is zero, then one needs to be dynamically allocated.
    //

    if (NewFamily->Properties.Id == 0) {
        Status = NetlinkpGenericAllocateFamilyId(&NewId);
        if (!KSUCCESS(Status)) {
            goto RegisterFamilyEnd;
        }

        NewFamily->Properties.Id = NewId;

    //
    // Otherwise make sure the provided message type is not already in use.
    //

    } else {
        FoundNode = RtlRedBlackTreeSearch(&NetlinkGenericFamilyTree,
                                          &(NewFamily->TreeNode));

        if (FoundNode != NULL) {
            Status = STATUS_DUPLICATE_ENTRY;
            goto RegisterFamilyEnd;
        }
    }

    //
    // If the family has multicast groups, allocate a region.
    //

    if (NewFamily->Properties.MulticastGroupCount != 0) {
        Status = NetlinkpGenericAllocateMulticastGroups(NewFamily);
        if (!KSUCCESS(Status)) {
            goto RegisterFamilyEnd;
        }
    }

    //
    // Insert the new family into the tree.
    //

    RtlRedBlackTreeInsert(&NetlinkGenericFamilyTree, &(NewFamily->TreeNode));
    KeReleaseSharedExclusiveLockExclusive(NetlinkGenericFamilyLock);
    LockHeld = FALSE;
    Status = STATUS_SUCCESS;

    //
    // Blast out some notifications.
    //

    Command = NETLINK_CONTROL_COMMAND_NEW_FAMILY;
    NetlinkpGenericControlSendNotification(NewFamily, Command, NULL);
    for (Index = 0;
         Index < NewFamily->Properties.MulticastGroupCount;
         Index += 1) {

        Command = NETLINK_CONTROL_COMMAND_NEW_MULTICAST_GROUP;
        Group = &(NewFamily->Properties.MulticastGroups[Index]);
        NetlinkpGenericControlSendNotification(NewFamily, Command, Group);
    }

RegisterFamilyEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetlinkGenericFamilyLock);
    }

    if (!KSUCCESS(Status)) {
        if (NewFamily != INVALID_HANDLE) {
            NetlinkpGenericFamilyReleaseReference(NewFamily);
            NewFamily = INVALID_HANDLE;
        }
    }

    if (Family != NULL) {
        *Family = NewFamily;
    }

    return Status;
}

NETLINK_API
VOID
NetlinkGenericUnregisterFamily (
    PNETLINK_GENERIC_FAMILY Family
    )

/*++

Routine Description:

    This routine unregisters the given generic netlink family.

Arguments:

    Family - Supplies a pointer to the generic netlink family to unregister.

Return Value:

    None.

--*/

{

    UCHAR Command;
    PNETLINK_GENERIC_FAMILY FoundFamily;
    PRED_BLACK_TREE_NODE FoundNode;
    BOOL LockHeld;

    KeAcquireSharedExclusiveLockExclusive(NetlinkGenericFamilyLock);
    LockHeld = TRUE;
    FoundNode = RtlRedBlackTreeSearch(&NetlinkGenericFamilyTree,
                                      &(Family->TreeNode));

    if (FoundNode == NULL) {
        goto UnregisterFamilyEnd;
    }

    FoundFamily = RED_BLACK_TREE_VALUE(FoundNode,
                                       NETLINK_GENERIC_FAMILY,
                                       TreeNode);

    if (FoundFamily != Family) {
        goto UnregisterFamilyEnd;
    }

    RtlRedBlackTreeRemove(&NetlinkGenericFamilyTree, &(Family->TreeNode));

    //
    // If the family had allocated multicast groups, then release them now.
    //

    if ((Family->Properties.MulticastGroupCount != 0) &&
        (Family->MulticastGroupOffset != 0)) {

        NetlinkpGenericFreeMulticastGroups(Family);
    }

    KeReleaseSharedExclusiveLockExclusive(NetlinkGenericFamilyLock);
    LockHeld = FALSE;

    //
    // Before releasing the last reference, make sure the family is not in the
    // middle of receiving a packet. If it is, netcore could be able to call
    // into the driver that is unregistering the family. It would be bad for
    // that driver to disappear.
    //

    while (Family->ReferenceCount > 1) {
        KeYield();
    }

    Command = NETLINK_CONTROL_COMMAND_DELETE_FAMILY;
    NetlinkpGenericControlSendNotification(Family, Command, NULL);
    NetlinkpGenericFamilyReleaseReference(Family);

UnregisterFamilyEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetlinkGenericFamilyLock);
    }

    return;
}

NETLINK_API
KSTATUS
NetlinkGenericSendCommand (
    PNETLINK_GENERIC_FAMILY Family,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS DestinationAddress
    )

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

{

    KSTATUS Status;

    Status = NetlinkSendMessage(NetlinkGenericSocket,
                                Packet,
                                DestinationAddress);

    return Status;
}

NETLINK_API
KSTATUS
NetlinkGenericSendMulticastCommand (
    PNETLINK_GENERIC_FAMILY Family,
    PNET_PACKET_BUFFER Packet,
    ULONG GroupId
    )

/*++

Routine Description:

    This routine multicasts the given command packet to the specified group.
    The packet should already store the completed command, ready to send.

Arguments:

    Family - Supplies a pointer to the generic netlink family sending the
        multicast command.

    Packet - Supplies a pointer to the network packet to be sent.

    GroupId - Supplies the family's multicast group ID over which to send the
        command.

Return Value:

    Status code.

--*/

{

    PNETWORK_ADDRESS Destination;
    NETLINK_ADDRESS NetlinkDestination;

    //
    // The destination is based on the group ID, which must be adjusted by the
    // offset allocated when the family as registered. The caller does not know
    // about this offset.
    //

    NetlinkDestination.Domain = NetDomainNetlink;
    NetlinkDestination.Port = 0;
    NetlinkDestination.Group = Family->MulticastGroupOffset + GroupId;
    Destination = (PNETWORK_ADDRESS)&NetlinkDestination;
    return NetlinkGenericSendCommand(Family, Packet, Destination);
}

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
    )

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

{

    PNETLINK_GENERIC_HEADER Header;
    ULONG PacketLength;
    KSTATUS Status;

    Length += NETLINK_GENERIC_HEADER_LENGTH;
    Status = NetlinkAppendHeader(NetlinkGenericSocket,
                                 Packet,
                                 Length,
                                 SequenceNumber,
                                 Family->Properties.Id,
                                 Flags);

    if (!KSUCCESS(Status)) {
        goto AppendHeadersEnd;
    }

    PacketLength = Packet->FooterOffset - Packet->DataOffset;
    if (PacketLength < Length) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto AppendHeadersEnd;
    }

    Header = Packet->Buffer + Packet->DataOffset;
    Header->Command = Command;
    Header->Version = Version;
    Packet->DataOffset += NETLINK_GENERIC_HEADER_LENGTH;
    Header->Reserved = 0;
    Status = STATUS_SUCCESS;

AppendHeadersEnd:
    return Status;
}

VOID
NetpNetlinkGenericInitialize (
    ULONG Phase
    )

/*++

Routine Description:

    This routine initializes support for generic netlink sockets.

Arguments:

    Phase - Supplies the phase of the initialization. Phase 0 happens before
        the networking core registers with the kernel, meaning sockets cannot
        be created. Phase 1 happens after the networking core has registered
        with the kernel allowing socket creation.

Return Value:

    None.

--*/

{

    NETLINK_ADDRESS Address;
    KSTATUS Status;

    //
    // In phase 0, register the generic netlink socket handlers with the core
    // networking library so that it is ready to go when netcore registers with
    // the kernel.
    //

    if (Phase == 0) {
        NetlinkGenericFamilyLock = KeCreateSharedExclusiveLock();
        if (NetlinkGenericFamilyLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        RtlRedBlackTreeInitialize(&NetlinkGenericFamilyTree,
                                  0,
                                  NetlinkpGenericCompareFamilies);

        Status = NetRegisterProtocol(&NetlinkGenericProtocol, NULL);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

    //
    // In phase 1, create netcore's kernel-side generic netlink socket.
    //

    } else {

        ASSERT(Phase == 1);

        Status = IoSocketCreate(NetDomainNetlink,
                                NetSocketDatagram,
                                SOCKET_INTERNET_PROTOCOL_NETLINK_GENERIC,
                                0,
                                &NetlinkGenericSocketHandle);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status = IoGetSocketFromHandle(NetlinkGenericSocketHandle,
                                       (PVOID)&NetlinkGenericSocket);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        //
        // Add the kernel flag and bind it to port 0.
        //

        RtlAtomicOr32(&(NetlinkGenericSocket->Flags), NET_SOCKET_FLAG_KERNEL);
        RtlZeroMemory(&Address, sizeof(NETLINK_ADDRESS));
        Address.Domain = NetDomainNetlink;
        Status = IoSocketBindToAddress(TRUE,
                                       NetlinkGenericSocketHandle,
                                       NULL,
                                       &(Address.NetworkAddress),
                                       NULL,
                                       0);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        NetlinkpGenericControlInitialize();
    }

InitializeEnd:

    ASSERT(KSUCCESS(Status));

    return;
}

KSTATUS
NetlinkpGenericCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    )

/*++

Routine Description:

    This routine allocates resources associated with a new socket. The protocol
    driver is responsible for allocating the structure (with additional length
    for any of its context). The core networking library will fill in the
    common header when this routine returns.

Arguments:

    ProtocolEntry - Supplies a pointer to the protocol information.

    NetworkEntry - Supplies a pointer to the network information.

    NetworkProtocol - Supplies the raw protocol value for this socket used on
        the network. This value is network specific.

    NewSocket - Supplies a pointer where a pointer to a newly allocated
        socket structure will be returned. The caller is responsible for
        allocating the socket (and potentially a larger structure for its own
        context). The core network library will fill in the standard socket
        structure after this routine returns. In phase 1, this will contain
        a pointer to the socket allocated during phase 0.

    Phase - Supplies the socket creation phase. Phase 0 is the allocation phase
        and phase 1 is the advanced initialization phase, which is invoked
        after net core is done filling out common portions of the socket
        structure.

Return Value:

    Status code.

--*/

{

    PNETLINK_GENERIC_SOCKET GenericSocket;
    PNET_NETWORK_INITIALIZE_SOCKET InitializeSocket;
    ULONG MaxPacketSize;
    PNET_SOCKET NetSocket;
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation;
    KSTATUS Status;

    ASSERT(ProtocolEntry->Type == NetSocketDatagram);
    ASSERT(NetworkProtocol == ProtocolEntry->ParentProtocolNumber);
    ASSERT(NetworkProtocol == SOCKET_INTERNET_PROTOCOL_NETLINK_GENERIC);

    //
    // Netlink generic only operates in phase 0.
    //

    if (Phase != 0) {
        return STATUS_SUCCESS;
    }

    NetSocket = NULL;
    GenericSocket = MmAllocatePagedPool(sizeof(NETLINK_GENERIC_SOCKET),
                                        NETLINK_GENERIC_ALLOCATION_TAG);

    if (GenericSocket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GenericCreateSocketEnd;
    }

    RtlZeroMemory(GenericSocket, sizeof(NETLINK_GENERIC_SOCKET));
    NetSocket = &(GenericSocket->NetlinkSocket.NetSocket);
    NetSocket->KernelSocket.Protocol = NetworkProtocol;
    NetSocket->KernelSocket.ReferenceCount = 1;
    RtlCopyMemory(&(GenericSocket->NetlinkSocket.ProtocolInterface),
                  &NetlinkGenericProtocolInterface,
                  sizeof(NETLINK_PROTOCOL_INTERFACE));

    INITIALIZE_LIST_HEAD(&(GenericSocket->ReceivedPacketList));
    GenericSocket->ReceiveTimeout = WAIT_TIME_INDEFINITE;
    GenericSocket->ReceiveBufferTotalSize =
                                   NETLINK_GENERIC_DEFAULT_RECEIVE_BUFFER_SIZE;

    GenericSocket->ReceiveBufferFreeSize =
                                         GenericSocket->ReceiveBufferTotalSize;

    GenericSocket->ReceiveMinimum = NETLINK_GENERIC_DEFAULT_RECEIVE_MINIMUM;
    GenericSocket->MaxPacketSize = NETLINK_GENERIC_MAX_PACKET_SIZE;
    GenericSocket->ReceiveLock = KeCreateQueuedLock();
    if (GenericSocket->ReceiveLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GenericCreateSocketEnd;
    }

    //
    // Give the lower layers a chance to initialize. Start the maximum packet
    // size at the largest possible value.
    //

    PacketSizeInformation = &(NetSocket->PacketSizeInformation);
    PacketSizeInformation->MaxPacketSize = MAX_ULONG;
    InitializeSocket = NetworkEntry->Interface.InitializeSocket;
    Status = InitializeSocket(ProtocolEntry,
                              NetworkEntry,
                              NetworkProtocol,
                              NetSocket);

    if (!KSUCCESS(Status)) {
        goto GenericCreateSocketEnd;
    }

    //
    // If the max packet size is greater than what is allowed for a generic
    // netlink packet plus all the previous headers and footers, then truncate
    // the max packet size. Note that there is no additional header for generic
    // netlink packets.
    //

    MaxPacketSize = PacketSizeInformation->HeaderSize +
                    NETLINK_GENERIC_MAX_PACKET_SIZE +
                    PacketSizeInformation->FooterSize;

    if (PacketSizeInformation->MaxPacketSize > MaxPacketSize) {
        PacketSizeInformation->MaxPacketSize = MaxPacketSize;
    }

    PacketSizeInformation->HeaderSize += NETLINK_GENERIC_HEADER_LENGTH;
    Status = STATUS_SUCCESS;

GenericCreateSocketEnd:
    if (!KSUCCESS(Status)) {
        if (GenericSocket != NULL) {
            if (GenericSocket->ReceiveLock != NULL) {
                KeDestroyQueuedLock(GenericSocket->ReceiveLock);
            }

            MmFreePagedPool(GenericSocket);
            GenericSocket = NULL;
            NetSocket = NULL;
        }
    }

    *NewSocket = NetSocket;
    return Status;
}

VOID
NetlinkpGenericDestroySocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys resources associated with an open socket, officially
    marking the end of the kernel and core networking library's knowledge of
    this structure.

Arguments:

    Socket - Supplies a pointer to the socket to destroy. The core networking
        library will have already destroyed any resources inside the common
        header, the protocol should not reach through any pointers inside the
        socket header except the protocol and network entries.

Return Value:

    None. This routine is responsible for freeing the memory associated with
    the socket structure itself.

--*/

{

    PNETLINK_GENERIC_SOCKET GenericSocket;
    PNETLINK_GENERIC_RECEIVED_PACKET Packet;

    GenericSocket = (PNETLINK_GENERIC_SOCKET)Socket;

    //
    // Loop through and free any leftover packets.
    //

    KeAcquireQueuedLock(GenericSocket->ReceiveLock);
    while (!LIST_EMPTY(&(GenericSocket->ReceivedPacketList))) {
        Packet = LIST_VALUE(GenericSocket->ReceivedPacketList.Next,
                            NETLINK_GENERIC_RECEIVED_PACKET,
                            ListEntry);

        LIST_REMOVE(&(Packet->ListEntry));
        GenericSocket->ReceiveBufferFreeSize += Packet->NetPacket->DataSize;
        NetFreeBuffer(Packet->NetPacket);
        MmFreePagedPool(Packet);
    }

    ASSERT(GenericSocket->ReceiveBufferFreeSize ==
           GenericSocket->ReceiveBufferTotalSize);

    KeReleaseQueuedLock(GenericSocket->ReceiveLock);
    if (Socket->Network->Interface.DestroySocket != NULL) {
        Socket->Network->Interface.DestroySocket(Socket);
    }

    KeDestroyQueuedLock(GenericSocket->ReceiveLock);
    MmFreePagedPool(GenericSocket);
    return;
}

KSTATUS
NetlinkpGenericBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine binds the given socket to the specified network address.
    Usually this is a no-op for the protocol, it's simply responsible for
    passing the request down to the network layer.

Arguments:

    Socket - Supplies a pointer to the socket to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

Return Value:

    Status code.

--*/

{

    BOOL LockHeld;
    KSTATUS Status;

    LockHeld = FALSE;

    //
    // Netlink sockets are allowed to be rebound to different multicast groups.
    //

    if ((Socket->LocalReceiveAddress.Domain != NetDomainInvalid) &&
        (Socket->LocalReceiveAddress.Port != Address->Port)) {

        Status = STATUS_INVALID_PARAMETER;
        goto GenericBindToAddressEnd;
    }

    //
    // Only netlink addresses are supported.
    //

    if (Address->Domain != NetDomainNetlink) {
        Status = STATUS_NOT_SUPPORTED;
        goto GenericBindToAddressEnd;
    }

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.BindToAddress(Socket, Link, Address, 0);
    if (!KSUCCESS(Status)) {
        goto GenericBindToAddressEnd;
    }

    //
    // Begin listening immediately, as there is no explicit listen step for
    // generic netlink sockets.
    //

    Status = Socket->Network->Interface.Listen(Socket);
    if (!KSUCCESS(Status)) {
        goto GenericBindToAddressEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);

GenericBindToAddressEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    }

    return Status;
}

KSTATUS
NetlinkpGenericListen (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing clients to attempt to connect to it.

Arguments:

    Socket - Supplies a pointer to the socket to mark as listning.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetlinkpGenericAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    )

/*++

Routine Description:

    This routine accepts an incoming connection on a listening connection-based
    socket.

Arguments:

    Socket - Supplies a pointer to the socket to accept a connection from.

    NewConnectionSocket - Supplies a pointer where a new socket will be
        returned that represents the accepted connection with the remote
        host.

    RemoteAddress - Supplies a pointer where the address of the connected
        remote host will be returned.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetlinkpGenericConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine attempts to make an outgoing connection to a server.

Arguments:

    Socket - Supplies a pointer to the socket to use for the connection.

    Address - Supplies a pointer to the address to connect to.

Return Value:

    Status code.

--*/

{

    ULONG GroupId;
    PNETLINK_ADDRESS NetlinkAddress;
    KSTATUS Status;

    //
    // If there is a request to connect to a multicast group, then validate it.
    //

    NetlinkAddress = (PNETLINK_ADDRESS)Address;
    if (NetlinkAddress->Group != 0) {
        GroupId = NetlinkAddress->Group;
        Status = NetlinkpGenericValidateMulticastGroup(GroupId, FALSE);
        if (!KSUCCESS(Status)) {
            goto GenericConnectEnd;
        }
    }

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.Connect(Socket, Address);
    if (!KSUCCESS(Status)) {
        goto GenericConnectEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);

GenericConnectEnd:
    return Status;
}

KSTATUS
NetlinkpGenericClose (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine closes a socket connection.

Arguments:

    Socket - Supplies a pointer to the socket to shut down.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Close it at the lower level and then release the reference taken on
    // create if the close was successful.
    //

    Status = Socket->Network->Interface.Close(Socket);
    if (!KSUCCESS(Status)) {
        goto GenericCloseEnd;
    }

    IoSocketReleaseReference(&(Socket->KernelSocket));

GenericCloseEnd:
    return Status;
}

KSTATUS
NetlinkpGenericShutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    )

/*++

Routine Description:

    This routine shuts down communication with a given socket.

Arguments:

    Socket - Supplies a pointer to the socket.

    ShutdownType - Supplies the shutdown type to perform. See the
        SOCKET_SHUTDOWN_* definitions.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetlinkpGenericSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine sends the given data buffer through the network using a
    specific protocol.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request is
        coming from kernel mode (TRUE) or user mode (FALSE).

    Socket - Supplies a pointer to the socket to send the data to.

    Parameters - Supplies a pointer to the socket I/O parameters. This will
        always be a kernel mode pointer.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        send.

Return Value:

    Status code.

--*/

{

    UINTN BytesComplete;
    PNETWORK_ADDRESS Destination;
    NETWORK_ADDRESS DestinationLocal;
    PNETLINK_GENERIC_SOCKET GenericSocket;
    NETWORK_ADDRESS LocalAddress;
    PNET_PACKET_BUFFER Packet;
    NET_PACKET_LIST PacketList;
    UINTN Size;
    KSTATUS Status;

    ASSERT(Socket->PacketSizeInformation.MaxPacketSize >
           sizeof(NETLINK_HEADER));

    BytesComplete = 0;
    NET_INITIALIZE_PACKET_LIST(&PacketList);
    Size = Parameters->Size;
    GenericSocket = (PNETLINK_GENERIC_SOCKET)Socket;
    Parameters->SocketIoFlags = 0;
    Destination = Parameters->NetworkAddress;
    if ((Destination != NULL) && (FromKernelMode == FALSE)) {
        Status = MmCopyFromUserMode(&DestinationLocal,
                                    Destination,
                                    sizeof(NETWORK_ADDRESS));

        Destination = &DestinationLocal;
        if (!KSUCCESS(Status)) {
            goto GenericSendEnd;
        }
    }

    if ((Destination == NULL) ||
        (Destination->Domain == NetDomainInvalid)) {

        if (Socket->BindingType != SocketFullyBound) {
            Status = STATUS_NOT_CONFIGURED;
            goto GenericSendEnd;
        }

        Destination = &(Socket->RemoteAddress);
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto GenericSendEnd;
    }

    //
    // If the size is greater than the generic netlink socket's maximum packet
    // size, fail.
    //

    if (Size > GenericSocket->MaxPacketSize) {
        Status = STATUS_MESSAGE_TOO_LONG;
        goto GenericSendEnd;
    }

    //
    // If the socket is not yet bound, then at least try to bind it to a local
    // port. This bind attempt may race with another bind attempt, but leave it
    // to the socket owner to synchronize bind and send.
    //

    if (Socket->BindingType == SocketBindingInvalid) {
        RtlZeroMemory(&LocalAddress, sizeof(NETWORK_ADDRESS));
        LocalAddress.Domain = Socket->Network->Domain;
        Status = NetlinkpGenericBindToAddress(Socket, NULL, &LocalAddress);
        if (!KSUCCESS(Status)) {
            goto GenericSendEnd;
        }
    }

    //
    // Allocate a buffer for the packet.
    //

    Status = NetAllocateBuffer(0,
                               Size,
                               0,
                               NULL,
                               0,
                               &Packet);

    if (!KSUCCESS(Status)) {
        goto GenericSendEnd;
    }

    NET_ADD_PACKET_TO_LIST(Packet, &PacketList);

    //
    // Copy the data to the packet's buffer.
    //

    Status = MmCopyIoBufferData(IoBuffer,
                                Packet->Buffer + Packet->DataOffset,
                                BytesComplete,
                                Size - BytesComplete,
                                FALSE);

    if (!KSUCCESS(Status)) {
        goto GenericSendEnd;
    }

    //
    // Send the packet down to the network layer.
    //

    Status = Socket->Network->Interface.Send(Socket,
                                             Destination,
                                             NULL,
                                             &PacketList);

    if (!KSUCCESS(Status)) {
        goto GenericSendEnd;
    }

    Packet = NULL;
    BytesComplete = Size;

GenericSendEnd:
    Parameters->BytesCompleted = BytesComplete;
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

VOID
NetlinkpGenericProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    None.

--*/

{

    ASSERT(FALSE);

    return;
}

KSTATUS
NetlinkpGenericProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called for a particular socket to process a received packet
    that was sent to it.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PNETLINK_GENERIC_SOCKET GenericSocket;
    PNET_PACKET_BUFFER Packet;
    PNET_PACKET_BUFFER PacketCopy;
    ULONG PacketLength;
    PNETLINK_GENERIC_RECEIVED_PACKET ReceivePacket;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Packet = ReceiveContext->Packet;
    PacketCopy = NULL;
    GenericSocket = (PNETLINK_GENERIC_SOCKET)Socket;

    //
    // If this is a kernel socket is on the receiving end, then route the
    // packet directly to the kernel component.
    //

    if ((Socket->Flags & NET_SOCKET_FLAG_KERNEL) != 0) {
        return NetlinkpGenericProcessReceivedKernelData(Socket, ReceiveContext);
    }

    //
    // Create a received packet entry for this data. This routine is invoked by
    // the network core on multicast packets. Create a copy of the network
    // packet as it may need to be sent to multiple sockets, no single socket
    // can own it.
    //

    AllocationSize = sizeof(NETLINK_GENERIC_RECEIVED_PACKET);
    ReceivePacket = MmAllocatePagedPool(AllocationSize,
                                        NETLINK_GENERIC_ALLOCATION_TAG);

    if (ReceivePacket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ProcessReceivedSocketDataEnd;
    }

    RtlCopyMemory(&(ReceivePacket->Address),
                  ReceiveContext->Source,
                  sizeof(NETWORK_ADDRESS));

    //
    // If the original packet is a multicast packet, then its services are
    // needed again by another socket. Make a copy and save that.
    //

    if ((Packet->Flags & NET_PACKET_FLAG_MULTICAST) != 0) {
        PacketLength = Packet->FooterOffset - Packet->DataOffset;
        Status = NetAllocateBuffer(0, PacketLength, 0, NULL, 0, &PacketCopy);
        if (!KSUCCESS(Status)) {
            goto ProcessReceivedSocketDataEnd;
        }

        RtlCopyMemory(PacketCopy->Buffer + PacketCopy->DataOffset,
                      Packet->Buffer + Packet->DataOffset,
                      PacketLength);

        ReceivePacket->NetPacket = PacketCopy;

    //
    // Otherwise set the packet directly in the generic receive packet.
    //

    } else {
        ReceivePacket->NetPacket = Packet;
    }

    //
    // Work to insert the packet on the list of received packets.
    //

    NetlinkpGenericInsertReceivedPacket(GenericSocket, ReceivePacket);

ProcessReceivedSocketDataEnd:
    if (!KSUCCESS(Status)) {
        if (ReceivePacket != NULL) {
            MmFreePagedPool(ReceivePacket);
        }

        if (PacketCopy != NULL) {
            NetFreeBuffer(PacketCopy);
        }

        if ((Packet->Flags & NET_PACKET_FLAG_MULTICAST) == 0) {
            NetFreeBuffer(Packet);
            ReceiveContext->Packet = NULL;
        }
    }

    return Status;
}

KSTATUS
NetlinkpGenericReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine is called by the user to receive data from the socket on a
    particular protocol.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request is
        coming from kernel mode (TRUE) or user mode (FALSE).

    Socket - Supplies a pointer to the socket to receive data from.

    Parameters - Supplies a pointer to the socket I/O parameters.

    IoBuffer - Supplies a pointer to the I/O buffer where the received data
        will be returned.

Return Value:

    STATUS_SUCCESS if any bytes were read.

    STATUS_TIMEOUT if the request timed out.

    STATUS_BUFFER_TOO_SMALL if the incoming datagram was too large for the
        buffer. The remainder of the datagram is discarded in this case.

    Other error codes on other failures.

--*/

{

    PVOID Buffer;
    UINTN BytesComplete;
    ULONG CopySize;
    ULONGLONG CurrentTime;
    ULONGLONG EndTime;
    ULONG Flags;
    PNETLINK_GENERIC_SOCKET GenericSocket;
    BOOL LockHeld;
    PNETLINK_GENERIC_RECEIVED_PACKET Packet;
    PLIST_ENTRY PacketEntry;
    ULONG PacketSize;
    ULONG ReturnedEvents;
    ULONG ReturnSize;
    UINTN Size;
    KSTATUS Status;
    ULONGLONG TimeCounterFrequency;
    ULONG Timeout;
    ULONG WaitTime;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    BytesComplete = 0;
    EndTime = 0;
    LockHeld = FALSE;
    Flags = Parameters->SocketIoFlags;
    Parameters->SocketIoFlags = 0;
    if ((Flags & SOCKET_IO_OUT_OF_BAND) != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto GenericReceiveEnd;
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto GenericReceiveEnd;
    }

    Packet = NULL;
    Size = Parameters->Size;
    TimeCounterFrequency = 0;
    Timeout = Parameters->TimeoutInMilliseconds;
    GenericSocket = (PNETLINK_GENERIC_SOCKET)Socket;

    //
    // Set a timeout timer to give up on. The socket stores the maximum timeout.
    //

    if (Timeout > GenericSocket->ReceiveTimeout) {
        Timeout = GenericSocket->ReceiveTimeout;
    }

    if ((Timeout != 0) && (Timeout != WAIT_TIME_INDEFINITE)) {
        EndTime = KeGetRecentTimeCounter();
        EndTime += KeConvertMicrosecondsToTimeTicks(
                                       Timeout * MICROSECONDS_PER_MILLISECOND);

        TimeCounterFrequency = HlQueryTimeCounterFrequency();
    }

    //
    // Loop trying to get some data. This loop exits once one packet is read.
    //

    while (TRUE) {

        //
        // Wait for a packet to become available. Start by computing the wait
        // time.
        //

        if (Timeout == 0) {
            WaitTime = 0;

        } else if (Timeout != WAIT_TIME_INDEFINITE) {
            CurrentTime = KeGetRecentTimeCounter();
            WaitTime = (EndTime - CurrentTime) * MILLISECONDS_PER_SECOND /
                       TimeCounterFrequency;

        } else {
            WaitTime = WAIT_TIME_INDEFINITE;
        }

        //
        // Wait for something to maybe become available. If the wait fails due
        // to a timeout, interruption, or something else, then fail out.
        // Otherwise when the read event is signalled, there is at least one
        // packet to receive.
        //

        Status = IoWaitForIoObjectState(Socket->KernelSocket.IoState,
                                        POLL_EVENT_IN,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            goto GenericReceiveEnd;
        }

        if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
            if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                Status = STATUS_NO_NETWORK_CONNECTION;

            } else {
                Status = NET_SOCKET_GET_LAST_ERROR(Socket);
                if (KSUCCESS(Status)) {
                    Status = STATUS_DEVICE_IO_ERROR;
                }
            }

            goto GenericReceiveEnd;
        }

        KeAcquireQueuedLock(GenericSocket->ReceiveLock);
        LockHeld = TRUE;

        //
        // If another thread beat this one to the punch, try again.
        //

        if (LIST_EMPTY(&(GenericSocket->ReceivedPacketList)) != FALSE) {
            KeReleaseQueuedLock(GenericSocket->ReceiveLock);
            LockHeld = FALSE;
            continue;
        }

        //
        // This should be the first packet being read.
        //

        ASSERT(BytesComplete == 0);

        PacketEntry = GenericSocket->ReceivedPacketList.Next;
        Packet = LIST_VALUE(PacketEntry,
                            NETLINK_GENERIC_RECEIVED_PACKET,
                            ListEntry);

        PacketSize = Packet->NetPacket->FooterOffset -
                     Packet->NetPacket->DataOffset;

        ReturnSize = PacketSize;
        CopySize = ReturnSize;
        if (CopySize > Size) {
            Parameters->SocketIoFlags |= SOCKET_IO_DATA_TRUNCATED;
            CopySize = Size;

            //
            // The real packet size is only returned to the user on truncation
            // if the truncated flag was supplied to this routine. Default to
            // returning the truncated size.
            //

            if ((Flags & SOCKET_IO_DATA_TRUNCATED) == 0) {
                ReturnSize = CopySize;
            }
        }

        Buffer = Packet->NetPacket->Buffer + Packet->NetPacket->DataOffset;
        Status = MmCopyIoBufferData(IoBuffer,
                                    Buffer,
                                    0,
                                    CopySize,
                                    TRUE);

        if (!KSUCCESS(Status)) {
            goto GenericReceiveEnd;
        }

        //
        // Copy the packet address out to the caller if requested.
        //

        if (Parameters->NetworkAddress != NULL) {
            if (FromKernelMode != FALSE) {
                RtlCopyMemory(Parameters->NetworkAddress,
                              &(Packet->Address),
                              sizeof(NETWORK_ADDRESS));

            } else {
                Status = MmCopyToUserMode(Parameters->NetworkAddress,
                                          &(Packet->Address),
                                          sizeof(NETWORK_ADDRESS));

                if (!KSUCCESS(Status)) {
                    goto GenericReceiveEnd;
                }
            }
        }

        BytesComplete = ReturnSize;

        //
        // Remove the packet if not peeking.
        //

        if ((Flags & SOCKET_IO_PEEK) == 0) {
            LIST_REMOVE(&(Packet->ListEntry));
            GenericSocket->ReceiveBufferFreeSize += PacketSize;

            //
            // The total receive buffer size may have been decreased. Don't
            // increment the free size above the total.
            //

            if (GenericSocket->ReceiveBufferFreeSize >
                GenericSocket->ReceiveBufferTotalSize) {

                GenericSocket->ReceiveBufferFreeSize =
                                         GenericSocket->ReceiveBufferTotalSize;
            }

            MmFreePagedPool(Packet);

            //
            // Unsignal the IN event if there are no more packets.
            //

            if (LIST_EMPTY(&(GenericSocket->ReceivedPacketList)) != FALSE) {
                IoSetIoObjectState(Socket->KernelSocket.IoState,
                                   POLL_EVENT_IN,
                                   FALSE);
            }
        }

        //
        // Wait-all does not apply to netlink sockets. Break out.
        //

        Status = STATUS_SUCCESS;
        break;
    }

GenericReceiveEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(GenericSocket->ReceiveLock);
    }

    Parameters->BytesCompleted = BytesComplete;
    return Status;
}

KSTATUS
NetlinkpGenericGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN Option,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets properties of the given socket.

Arguments:

    Socket - Supplies a pointer to the socket to get or set information for.

    InformationType - Supplies the socket information type category to which
        specified option belongs.

    Option - Supplies the option to get or set, which is specific to the
        information type. The type of this value is generally
        SOCKET_<information_type>_OPTION.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input constains the size of the data
        buffer. On output, this contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the information type is incorrect.

    STATUS_BUFFER_TOO_SMALL if the data buffer is too small to receive the
        requested option.

    STATUS_NOT_SUPPORTED_BY_PROTOCOL if the socket option is not supported by
        the socket.

    STATUS_NOT_HANDLED if the protocol does not override the default behavior
        for a basic socket option.

--*/

{

    ULONG Count;
    PNETLINK_GENERIC_SOCKET_OPTION GenericOption;
    PNETLINK_GENERIC_SOCKET GenericSocket;
    ULONG Index;
    LONGLONG Milliseconds;
    PNET_PACKET_SIZE_INFORMATION SizeInformation;
    ULONG SizeOption;
    PSOCKET_TIME SocketTime;
    SOCKET_TIME SocketTimeBuffer;
    PVOID Source;
    KSTATUS Status;

    GenericSocket = (PNETLINK_GENERIC_SOCKET)Socket;
    if ((InformationType != SocketInformationBasic) &&
        (InformationType != SocketInformationNetlinkGeneric)) {

        Status = STATUS_NOT_SUPPORTED;
        goto GenericGetSetInformationEnd;
    }

    //
    // Search to see if the socket option is supported by the netlink generic
    // protocol.
    //

    Count = sizeof(NetlinkGenericSocketOptions) /
            sizeof(NetlinkGenericSocketOptions[0]);

    for (Index = 0; Index < Count; Index += 1) {
        GenericOption = &(NetlinkGenericSocketOptions[Index]);
        if ((GenericOption->InformationType == InformationType) &&
            (GenericOption->Option == Option)) {

            break;
        }
    }

    if (Index == Count) {
        if (InformationType == SocketInformationBasic) {
            Status = STATUS_NOT_HANDLED;

        } else {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        }

        goto GenericGetSetInformationEnd;
    }

    //
    // Handle failure cases common to all options.
    //

    if (Set != FALSE) {
        if (GenericOption->SetAllowed == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            goto GenericGetSetInformationEnd;
        }

        if (*DataSize < GenericOption->Size) {
            *DataSize = GenericOption->Size;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto GenericGetSetInformationEnd;
        }
    }

    //
    // There are currently no netlink generic protocol options.
    //

    ASSERT(InformationType != SocketInformationNetlinkGeneric);

    //
    // Parse the basic socket option, getting the information from the generic
    // socket or setting the new state in the generic socket.
    //

    Source = NULL;
    Status = STATUS_SUCCESS;
    switch ((SOCKET_BASIC_OPTION)Option) {
    case SocketBasicOptionSendBufferSize:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);
            if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                SizeOption = SOCKET_OPTION_MAX_ULONG;
            }

            SizeInformation = &(Socket->PacketSizeInformation);
            if (SizeOption > NETLINK_GENERIC_MAX_PACKET_SIZE) {
                SizeOption = NETLINK_GENERIC_MAX_PACKET_SIZE;

            } else if (SizeOption < SizeInformation->MaxPacketSize) {
                SizeOption = SizeInformation->MaxPacketSize;
            }

            GenericSocket->MaxPacketSize = SizeOption;

        } else {
            Source = &SizeOption;
            SizeOption = GenericSocket->MaxPacketSize;
        }

        break;

    case SocketBasicOptionSendMinimum:

        ASSERT(Set == FALSE);

        SizeOption = NETLINK_GENERIC_SEND_MINIMUM;
        Source = &SizeOption;
        break;

    case SocketBasicOptionReceiveBufferSize:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);
            if (SizeOption < NETLINK_GENERIC_MIN_RECEIVE_BUFFER_SIZE) {
                SizeOption = NETLINK_GENERIC_MIN_RECEIVE_BUFFER_SIZE;

            } else if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                SizeOption = SOCKET_OPTION_MAX_ULONG;
            }

            //
            // Set the receive buffer size and truncate the available free
            // space if necessary. Do not remove any packets that have already
            // been received. This is not meant to be a truncate call.
            //

            KeAcquireQueuedLock(GenericSocket->ReceiveLock);
            GenericSocket->ReceiveBufferTotalSize = SizeOption;
            if (GenericSocket->ReceiveBufferFreeSize > SizeOption) {
                GenericSocket->ReceiveBufferFreeSize = SizeOption;
            }

            KeReleaseQueuedLock(GenericSocket->ReceiveLock);

        } else {
            SizeOption = GenericSocket->ReceiveBufferTotalSize;
            Source = &SizeOption;
        }

        break;

    case SocketBasicOptionReceiveMinimum:
        if (Set != FALSE) {
            SizeOption = *((PULONG)Data);
            if (SizeOption > SOCKET_OPTION_MAX_ULONG) {
                SizeOption = SOCKET_OPTION_MAX_ULONG;
            }

            GenericSocket->ReceiveMinimum = SizeOption;

        } else {
            SizeOption = GenericSocket->ReceiveMinimum;
            Source = &SizeOption;
        }

        break;

    case SocketBasicOptionReceiveTimeout:
        if (Set != FALSE) {
            SocketTime = (PSOCKET_TIME)Data;
            if (SocketTime->Seconds < 0) {
                Status = STATUS_DOMAIN_ERROR;
                break;
            }

            Milliseconds = SocketTime->Seconds * MILLISECONDS_PER_SECOND;
            if (Milliseconds < SocketTime->Seconds) {
                Status = STATUS_DOMAIN_ERROR;
                break;
            }

            Milliseconds += SocketTime->Microseconds /
                            MICROSECONDS_PER_MILLISECOND;

            if ((Milliseconds < 0) || (Milliseconds > MAX_LONG)) {
                Status = STATUS_DOMAIN_ERROR;
                break;
            }

            GenericSocket->ReceiveTimeout = (ULONG)(LONG)Milliseconds;

        } else {
            Source = &SocketTimeBuffer;
            if (GenericSocket->ReceiveTimeout == WAIT_TIME_INDEFINITE) {
                SocketTimeBuffer.Seconds = 0;
                SocketTimeBuffer.Microseconds = 0;

            } else {
                SocketTimeBuffer.Seconds = GenericSocket->ReceiveTimeout /
                                           MILLISECONDS_PER_SECOND;

                SocketTimeBuffer.Microseconds = (GenericSocket->ReceiveTimeout %
                                                 MILLISECONDS_PER_SECOND) *
                                                MICROSECONDS_PER_MILLISECOND;
            }
        }

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_HANDLED;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto GenericGetSetInformationEnd;
    }

    //
    // Truncate all copies for get requests down to the required size and only
    // return the required size on set requests.
    //

    if (*DataSize > GenericOption->Size) {
        *DataSize = GenericOption->Size;
    }

    //
    // For get requests, copy the gathered information to the supplied data
    // buffer.
    //

    if (Set == FALSE) {

        ASSERT(Source != NULL);

        RtlCopyMemory(Data, Source, *DataSize);

        //
        // If the copy truncated the data, report that the given buffer was too
        // small. The caller can choose to ignore this if the truncated data is
        // enough.
        //

        if (*DataSize < GenericOption->Size) {
            *DataSize = GenericOption->Size;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto GenericGetSetInformationEnd;
        }
    }

GenericGetSetInformationEnd:
    return Status;
}

KSTATUS
NetlinkpGenericUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    )

/*++

Routine Description:

    This routine handles user control requests destined for a socket.

Arguments:

    Socket - Supplies a pointer to the socket.

    CodeNumber - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetlinkpGenericJoinMulticastGroup (
    PNET_SOCKET Socket,
    ULONG GroupId
    )

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

{

    KSTATUS Status;

    //
    // With the family lock held to prevent family's from unregistering,
    // validate the multicast group and then attempt to join it. This must be
    // done with the lock held or else a family could leave after validation
    // and then the socket would miss being removed from the multicast group it
    // is about to join.
    //

    KeAcquireSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    Status = NetlinkpGenericValidateMulticastGroup(GroupId, TRUE);
    if (!KSUCCESS(Status)) {
        goto JoinMulticastGroupEnd;
    }

    Status = NetlinkJoinMulticastGroup(Socket, GroupId);
    if (!KSUCCESS(Status)) {
        goto JoinMulticastGroupEnd;
    }

JoinMulticastGroupEnd:
    KeReleaseSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    return Status;
}

PNETLINK_GENERIC_FAMILY
NetlinkpGenericLookupFamilyById (
    ULONG FamilyId
    )

/*++

Routine Description:

    This routine searches the database of registered generic netlink families
    for one with the given ID. If successful, the family is returned with an
    added reference which the caller must release.

Arguments:

    FamilyId - Supplies the ID of the desired family.

Return Value:

    Returns a pointer to the generic netlink family on success or NULL on
    failure.

--*/

{

    PNETLINK_GENERIC_FAMILY FoundFamily;
    PRED_BLACK_TREE_NODE FoundNode;
    NETLINK_GENERIC_FAMILY SearchFamily;

    FoundFamily = NULL;
    SearchFamily.Properties.Id = FamilyId;
    KeAcquireSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    FoundNode = RtlRedBlackTreeSearch(&NetlinkGenericFamilyTree,
                                      &(SearchFamily.TreeNode));

    if (FoundNode != NULL) {
        FoundFamily = RED_BLACK_TREE_VALUE(FoundNode,
                                           NETLINK_GENERIC_FAMILY,
                                           TreeNode);

        NetlinkpGenericFamilyAddReference(FoundFamily);
    }

    KeReleaseSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    return FoundFamily;
}

PNETLINK_GENERIC_FAMILY
NetlinkpGenericLookupFamilyByName (
    PSTR FamilyName
    )

/*++

Routine Description:

    This routine searches the database of registered generic netlink families
    for one with the given name. If successful, the family is returned with an
    added reference which the caller must release.

Arguments:

    FamilyName - Supplies the name of the desired family.

Return Value:

    Returns a pointer to the generic netlink family on success or NULL on
    failure.

--*/

{

    PNETLINK_GENERIC_FAMILY Family;
    PNETLINK_GENERIC_FAMILY FoundFamily;
    BOOL Match;
    PRED_BLACK_TREE_NODE Node;

    FoundFamily = NULL;
    KeAcquireSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    Node = RtlRedBlackTreeGetLowestNode(&NetlinkGenericFamilyTree);
    while (Node != NULL) {
        Family = RED_BLACK_TREE_VALUE(Node, NETLINK_GENERIC_FAMILY, TreeNode);
        Match = RtlAreStringsEqual(Family->Properties.Name,
                                   FamilyName,
                                   NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH);

        if (Match != FALSE) {
            FoundFamily = Family;
            NetlinkpGenericFamilyAddReference(FoundFamily);
            break;
        }

        Node = RtlRedBlackTreeGetNextNode(&NetlinkGenericFamilyTree,
                                          FALSE,
                                          Node);
    }

    KeReleaseSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    return FoundFamily;
}

VOID
NetlinkpGenericFamilyAddReference (
    PNETLINK_GENERIC_FAMILY Family
    )

/*++

Routine Description:

    This routine increments the reference count of a generic netlink family.

Arguments:

    Family - Supplies a pointer to a generic netlink family.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Family->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
NetlinkpGenericFamilyReleaseReference (
    PNETLINK_GENERIC_FAMILY Family
    )

/*++

Routine Description:

    This routine decrements the reference count of a generic netlink family,
    releasing all of its resources if the reference count drops to zero.

Arguments:

    Family - Supplies a pointer to a generic netlink family.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Family->ReferenceCount), -1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        NetlinkpGenericDestroyFamily(Family);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetlinkpGenericInsertReceivedPacket (
    PNETLINK_GENERIC_SOCKET Socket,
    PNETLINK_GENERIC_RECEIVED_PACKET Packet
    )

/*++

Routine Description:

    This routine attempts to insert the given packet into the socket's list of
    received packets.

Arguments:

    Socket - Supplies a pointer to the generic netlink socket that received the
        packet.

    Packet - Supplies a pointer to the generic netlink packet that was received.

Return Value:

    None.

--*/

{

    ULONG PacketLength;

    PacketLength = Packet->NetPacket->FooterOffset -
                   Packet->NetPacket->DataOffset;

    KeAcquireQueuedLock(Socket->ReceiveLock);
    if (PacketLength <= Socket->ReceiveBufferFreeSize) {
        INSERT_BEFORE(&(Packet->ListEntry),
                      &(Socket->ReceivedPacketList));

        Socket->ReceiveBufferFreeSize -= PacketLength;

        ASSERT(Socket->ReceiveBufferFreeSize <
               Socket->ReceiveBufferTotalSize);

        //
        // One packet is always enough to notify a waiting receiver.
        //

        IoSetIoObjectState(Socket->NetlinkSocket.NetSocket.KernelSocket.IoState,
                           POLL_EVENT_IN,
                           TRUE);

        Packet = NULL;

    } else {
        Socket->DroppedPacketCount += 1;
    }

    KeReleaseQueuedLock(Socket->ReceiveLock);

    //
    // If the packet wasn't nulled out, that's an indication it wasn't added to
    // the list, so free it up.
    //

    if (Packet != NULL) {
        NetFreeBuffer(Packet->NetPacket);
        MmFreePagedPool(Packet);
    }

    return;
}

KSTATUS
NetlinkpGenericProcessReceivedKernelData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called to process a packet received by a kernel socket.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    Status code.

--*/

{

    PNETLINK_GENERIC_COMMAND Command;
    NETLINK_GENERIC_COMMAND_INFORMATION CommandInformation;
    PNETLINK_GENERIC_FAMILY Family;
    BOOL FoundCommand;
    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_HEADER Header;
    ULONG Index;
    NET_PACKET_BUFFER LocalPacket;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketLength;
    USHORT RequiredFlags;
    KSTATUS Status;

    Family = NULL;

    //
    // Make a local copy of the network packet buffer so that the offsets can
    // be modified. The original is not allowed to be modified.
    //

    Packet = ReceiveContext->Packet;
    RtlCopyMemory(&LocalPacket, Packet, sizeof(NET_PACKET_BUFFER));
    PacketLength = LocalPacket.FooterOffset - LocalPacket.DataOffset;
    Header = LocalPacket.Buffer + LocalPacket.DataOffset;

    ASSERT(PacketLength >= NETLINK_HEADER_LENGTH);
    ASSERT((Header->Flags & NETLINK_HEADER_FLAG_REQUEST) != 0);
    ASSERT(Header->Type >= NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM);
    ASSERT(Header->Length <= PacketLength);

    LocalPacket.DataOffset += NETLINK_HEADER_LENGTH;
    PacketLength -= NETLINK_HEADER_LENGTH;

    //
    // The generic header must be present.
    //

    GenericHeader = NETLINK_DATA(Header);
    if (PacketLength < NETLINK_GENERIC_HEADER_LENGTH) {
        RtlDebugPrint("NETLINK: packet does not have space for generic "
                      "header. %d expected, has %d.\n",
                      NETLINK_GENERIC_HEADER_LENGTH,
                      PacketLength);

        Status = STATUS_BUFFER_TOO_SMALL;
        goto ProcessReceivedKernelDataEnd;
    }

    LocalPacket.DataOffset += NETLINK_GENERIC_HEADER_LENGTH;
    PacketLength -= NETLINK_GENERIC_HEADER_LENGTH;

    //
    // Find the generic netlink family and call the command's callback.
    //

    Family = NetlinkpGenericLookupFamilyById(Header->Type);
    if (Family == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto ProcessReceivedKernelDataEnd;
    }

    FoundCommand = FALSE;
    for (Index = 0; Index < Family->Properties.CommandCount; Index += 1) {
        Command = &(Family->Properties.Commands[Index]);
        if (Command->CommandId == GenericHeader->Command) {
            FoundCommand = TRUE;
            break;
        }
    }

    if (FoundCommand == FALSE) {
        Status = STATUS_NOT_SUPPORTED;
        goto ProcessReceivedKernelDataEnd;
    }

    RequiredFlags = Command->RequiredFlags;
    if ((Header->Flags & RequiredFlags) != RequiredFlags) {
        Status = STATUS_NOT_SUPPORTED;
        goto ProcessReceivedKernelDataEnd;
    }

    CommandInformation.Message.SourceAddress = ReceiveContext->Source;
    CommandInformation.Message.DestinationAddress = ReceiveContext->Destination;
    CommandInformation.Message.SequenceNumber = Header->SequenceNumber;
    CommandInformation.Message.Type = Header->Type;
    CommandInformation.Command = GenericHeader->Command;
    CommandInformation.Version = GenericHeader->Version;
    Status = Command->ProcessCommand(Socket, &LocalPacket, &CommandInformation);
    if (!KSUCCESS(Status)) {
        goto ProcessReceivedKernelDataEnd;
    }

ProcessReceivedKernelDataEnd:
    if (Family != NULL) {
        NetlinkpGenericFamilyReleaseReference(Family);
    }

    return Status;
}

COMPARISON_RESULT
NetlinkpGenericCompareFamilies (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two netlink generic family Red-Black tree nodes.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PNETLINK_GENERIC_FAMILY FirstFamily;
    PNETLINK_GENERIC_FAMILY SecondFamily;

    FirstFamily = RED_BLACK_TREE_VALUE(FirstNode,
                                       NETLINK_GENERIC_FAMILY,
                                       TreeNode);

    SecondFamily = RED_BLACK_TREE_VALUE(SecondNode,
                                        NETLINK_GENERIC_FAMILY,
                                        TreeNode);

    if (FirstFamily->Properties.Id < SecondFamily->Properties.Id) {
        return ComparisonResultAscending;
    }

    if (FirstFamily->Properties.Id > SecondFamily->Properties.Id) {
        return ComparisonResultDescending;
    }

    return ComparisonResultSame;
}

VOID
NetlinkpGenericDestroyFamily (
    PNETLINK_GENERIC_FAMILY Family
    )

/*++

Routine Description:

    This routine destroys a generic netlink family and all of its resources. It
    should already be unregistered and removed from the global tree.

Arguments:

    Family - Supplies a pointer to a generic netlink family.

Return Value:

    None.

--*/

{

    MmFreePagedPool(Family);
    return;
}

KSTATUS
NetlinkpGenericAllocateFamilyId (
    PULONG FamilyId
    )

/*++

Routine Description:

    This routine attempts to allocate a free generic netlink family ID. It
    assumes the netlink generic family lock is held exclusively.

Arguments:

    FamilyId - Supplies a pointer that receives the newly allocated ID.

Return Value:

    Status code.

--*/

{

    PRED_BLACK_TREE_NODE FoundNode;
    ULONG NextId;
    NETLINK_GENERIC_FAMILY SearchFamily;
    KSTATUS Status;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(NetlinkGenericFamilyLock));

    Status = STATUS_INSUFFICIENT_RESOURCES;
    NextId = NetlinkGenericFamilyNextId;

    //
    // Iterate until all the possible message types have been tried.
    //

    do {
        SearchFamily.Properties.Id = NextId;
        NextId += 1;
        if (NextId < NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM) {
            NextId = NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM;
        }

        FoundNode = RtlRedBlackTreeSearch(&NetlinkGenericFamilyTree,
                                          &(SearchFamily.TreeNode));

        if (FoundNode == NULL) {
            *FamilyId = SearchFamily.Properties.Id;
            Status = STATUS_SUCCESS;
            break;
        }

    } while (NextId != NetlinkGenericFamilyNextId);

    //
    // Update the global to make the next search start where this left off.
    //

    NetlinkGenericFamilyNextId = NextId;
    return Status;
}

KSTATUS
NetlinkpGenericAllocateMulticastGroups (
    PNETLINK_GENERIC_FAMILY Family
    )

/*++

Routine Description:

    This routine allocates a region of the multicast group ID namespace for the
    given family. It assumes the netlink generic family lock is held
    exclusively.

Arguments:

    Family - Supplies a pointer to the netlink generic family in need of
        multicast group allocation.

Return Value:

    Status code.

--*/

{

    ULONG BitmapIndex;
    ULONG Count;
    PNETLINK_GENERIC_MULTICAST_GROUP Group;
    ULONG Index;
    ULONG Mask;
    PULONG NewBitmap;
    ULONG NewBitmapSize;
    ULONG NewGroups;
    ULONG Offset;
    ULONG Run;
    KSTATUS Status;
    ULONG Value;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(NetlinkGenericFamilyLock));

    Count = Family->Properties.MulticastGroupCount;

    //
    // Search through the existing bitmap for a run that can accomodate the
    // new multicast groups.
    //

    Offset = 0;
    Run = 0;
    for (BitmapIndex = 0;
         BitmapIndex < (NetlinkGenericMulticastBitmapSize / sizeof(ULONG));
         BitmapIndex += 1) {

        Value = NetlinkGenericMulticastBitmap[BitmapIndex];
        for (Index = 0; Index < (sizeof(ULONG) * BITS_PER_BYTE); Index += 1) {
            if ((Value & 0x1) != 0) {
                Offset += Run + 1;
                Run = 0;

            } else {
                Run += 1;
                if (Run == Count) {
                    break;
                }
            }

            Value >>= 1;
        }

        if (Run == Count) {
            break;
        }

        //
        // A multicast group ID of 0 should never be assigned.
        //

        ASSERT(Offset != 0);
    }

    //
    // If there is not enough space, allocate a bigger array. If the run
    // is not zero, that means that there was some space at the end of the
    // last ULONG of the bitmap. Account for that when added more ULONGs.
    //

    if (Run != Count) {
        NewGroups = Count - Run;

        //
        // Avoid allocating group ID 0. It is invalid.
        //

        if (Offset == 0) {
            NewGroups += 1;
            Offset = 1;
        }

        NewGroups = ALIGN_RANGE_UP(NewGroups, sizeof(ULONG) * BITS_PER_BYTE);
        NewBitmapSize = NetlinkGenericMulticastBitmapSize +
                        (NewGroups / BITS_PER_BYTE);

        NewBitmap = MmAllocatePagedPool(NewBitmapSize,
                                        NETLINK_GENERIC_ALLOCATION_TAG);

        if (NewBitmap == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AllocateMulticastGroupOffsetEnd;
        }

        RtlZeroMemory((PVOID)NewBitmap + NetlinkGenericMulticastBitmapSize,
                      (NewGroups / BITS_PER_BYTE));

        //
        // If there is an existing bitmap, copy it to the new bitmap and
        // then release it.
        //

        if (NetlinkGenericMulticastBitmap != NULL) {
            RtlCopyMemory(NewBitmap,
                          NetlinkGenericMulticastBitmap,
                          NetlinkGenericMulticastBitmapSize);

            MmFreePagedPool(NetlinkGenericMulticastBitmap);

        //
        // Otherwise, this is the first allocation. Reserve group ID 0.
        //

        } else {
            Index = NETLINK_SOCKET_BITMAP_INDEX(0);
            Mask = NETLINK_SOCKET_BITMAP_MASK(0);
            NewBitmap[Index] |= Mask;
        }

        NetlinkGenericMulticastBitmap = NewBitmap;
        NetlinkGenericMulticastBitmapSize = NewBitmapSize;
    }

    //
    // Set the newly allocated groups as reserved in the bitmap.
    //

    for (Index = 0; Index < Count; Index += 1) {
        Group = &(Family->Properties.MulticastGroups[Index]);
        Group->Id += Offset;
        BitmapIndex = NETLINK_SOCKET_BITMAP_INDEX(Group->Id);
        Mask = NETLINK_SOCKET_BITMAP_MASK(Group->Id);
        NetlinkGenericMulticastBitmap[BitmapIndex] |= Mask;
    }

    Family->MulticastGroupOffset = Offset;
    Status = STATUS_SUCCESS;

AllocateMulticastGroupOffsetEnd:
    return Status;
}

VOID
NetlinkpGenericFreeMulticastGroups (
    PNETLINK_GENERIC_FAMILY Family
    )

/*++

Routine Description:

    This routine releases the region of multicast group IDs allocated for the
    given netlink generic family. It assumes the netlink generic family lock is
    held exclusively.

Arguments:

    Family - Supplies a pointer to the netlink generic family whose multicast
        groups should be released.

Return Value:

    None.

--*/

{

    ULONG BitmapIndex;
    UCHAR Command;
    ULONG Count;
    PNETLINK_GENERIC_MULTICAST_GROUP Group;
    ULONG Index;
    ULONG Mask;
    ULONG Offset;
    ULONG Protocol;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(NetlinkGenericFamilyLock));

    Count = Family->Properties.MulticastGroupCount;
    Offset = Family->MulticastGroupOffset;

    //
    // Remove all sockets from these multicast groups.
    //

    Protocol = SOCKET_INTERNET_PROTOCOL_NETLINK_GENERIC;
    NetlinkRemoveSocketsFromMulticastGroups(Protocol, Offset, Count);

    //
    // Free the groups from the generic netlink bitmap.
    //

    for (Index = 0; Index < Count; Index += 1) {
        Group = &(Family->Properties.MulticastGroups[Index]);
        BitmapIndex = NETLINK_SOCKET_BITMAP_INDEX(Group->Id);
        Mask = NETLINK_SOCKET_BITMAP_MASK(Group->Id);
        NetlinkGenericMulticastBitmap[BitmapIndex] &= ~Mask;

        //
        // Announce the deletion of the multicast group.
        //

        Command = NETLINK_CONTROL_COMMAND_DELETE_MULTICAST_GROUP;
        NetlinkpGenericControlSendNotification(Family, Command, Group);
    }

    return;
}

KSTATUS
NetlinkpGenericValidateMulticastGroup (
    ULONG GroupId,
    BOOL LockHeld
    )

/*++

Routine Description:

    This routine validates that the group ID is valid for the netlink generic
    families.

Arguments:

    GroupId - Supplies the group ID that is to be validated.

    LockHeld - Supplies a boolean indicating whether or not the global generic
        family lock is held.

Return Value:

    Status code.

--*/

{

    ULONG Index;
    ULONG Mask;
    KSTATUS Status;

    ASSERT(GroupId != 0);

    if (LockHeld == FALSE) {
        KeAcquireSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    }

    Index = NETLINK_SOCKET_BITMAP_INDEX(GroupId);
    Mask = NETLINK_SOCKET_BITMAP_MASK(GroupId);
    if ((Index >= (NetlinkGenericMulticastBitmapSize / sizeof(ULONG))) ||
        ((NetlinkGenericMulticastBitmap[Index] & Mask) == 0)) {

        Status = STATUS_INVALID_PARAMETER;

    } else {
        Status = STATUS_SUCCESS;
    }

    if (LockHeld == FALSE) {
        KeReleaseSharedExclusiveLockShared(NetlinkGenericFamilyLock);
    }

    return Status;
}

