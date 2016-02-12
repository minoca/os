/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

#define NET_API DLLEXPORT

#include <minoca/driver.h>
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
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a generic netlink socket.

Members:

    NetSocket - Stores the common core networking parameters.

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
        before the socket is made readable.

    DroppedPacketCount - Stores the number of packets that have been dropped
        because the receive queue was full.

    MaxPacketSize - Stores the maximum size of UDP datagrams.

--*/

typedef struct _NETLINK_GENERIC_SOCKET {
    NET_SOCKET NetSocket;
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

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpNetlinkGenericCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket
    );

VOID
NetpNetlinkGenericDestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpNetlinkGenericBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpNetlinkGenericListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpNetlinkGenericAccept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    );

KSTATUS
NetpNetlinkGenericConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpNetlinkGenericClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetpNetlinkGenericShutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    );

KSTATUS
NetpNetlinkGenericSend (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

VOID
NetpNetlinkGenericProcessReceivedData (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress,
    PNET_PROTOCOL_ENTRY ProtocolEntry
    );

VOID
NetpNetlinkGenericProcessReceivedSocketData (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

KSTATUS
NetpNetlinkGenericReceive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetpNetlinkGenericGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetpNetlinkGenericUserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

VOID
NetpNetlinkGenericInsertReceivedPacket (
    PNETLINK_GENERIC_SOCKET Socket,
    PNETLINK_GENERIC_RECEIVED_PACKET Packet
    );

VOID
NetpNetlinkGenericProcessReceivedKernelData (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

PNETLINK_GENERIC_FAMILY
NetpNetlinkGenericLookupFamilyById (
    ULONG MessageType
    );

COMPARISON_RESULT
NetpNetlinkGenericCompareFamilies (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

VOID
NetpNetlinkGenericDestroyFamily (
    PNETLINK_GENERIC_FAMILY Family
    );

KSTATUS
NetpNetlinkGenericAllocateFamilyId (
    PULONG FamilyId
    );

//
// -------------------------------------------------------------------- Globals
//

NET_PROTOCOL_ENTRY NetNetlinkGenericProtocol = {
    {NULL, NULL},
    SocketTypeDatagram,
    SOCKET_INTERNET_PROTOCOL_NETLINK_GENERIC,
    NULL,
    NULL,
    {{0}, {0}, {0}},
    {
        NetpNetlinkGenericCreateSocket,
        NetpNetlinkGenericDestroySocket,
        NetpNetlinkGenericBindToAddress,
        NetpNetlinkGenericListen,
        NetpNetlinkGenericAccept,
        NetpNetlinkGenericConnect,
        NetpNetlinkGenericClose,
        NetpNetlinkGenericShutdown,
        NetpNetlinkGenericSend,
        NetpNetlinkGenericProcessReceivedData,
        NetpNetlinkGenericProcessReceivedSocketData,
        NetpNetlinkGenericReceive,
        NetpNetlinkGenericGetSetInformation,
        NetpNetlinkGenericUserControl
    }
};

PIO_HANDLE NetNetlinkGenericSocketHandle;
PNET_SOCKET NetNetlinkGenericSocket;

//
// Store the lock and tree for storing the generic netlink families.
//

PSHARED_EXCLUSIVE_LOCK NetNetlinkGenericFamilyLock;
RED_BLACK_TREE NetNetlinkGenericFamilyTree;

//
// Store the next generic family message type to allocate.
//

ULONG NetNetlinkGenericFamilyNextId = NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM;

//
// ------------------------------------------------------------------ Functions
//

NET_API
KSTATUS
NetNetlinkGenericRegisterFamily (
    PNETLINK_GENERIC_FAMILY_PROPERTIES Properties,
    PHANDLE FamilyHandle
    )

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

{

    PNETLINK_GENERIC_FAMILY Family;
    PNETLINK_GENERIC_FAMILY FoundFamily;
    PRED_BLACK_TREE_NODE FoundNode;
    BOOL LockHeld;
    BOOL Match;
    ULONG NameLength;
    ULONG NewId;
    KSTATUS Status;

    LockHeld = FALSE;
    Family = NULL;
    if (Properties->Version < NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION) {
        Status = STATUS_VERSION_MISMATCH;
        goto RegisterFamilyEnd;
    }

    if (Properties->Interface.ProcessReceivedData == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto RegisterFamilyEnd;
    }

    NameLength = RtlStringLength(Properties->Name) + 1;
    if ((NameLength == 1) ||
        (NameLength > NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH)) {

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

    Family = MmAllocatePagedPool(sizeof(NETLINK_GENERIC_FAMILY),
                                 NETLINK_GENERIC_ALLOCATION_TAG);

    if (Family == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RegisterFamilyEnd;
    }

    RtlZeroMemory(Family, sizeof(NETLINK_GENERIC_FAMILY));
    Family->ReferenceCount = 1;
    RtlCopyMemory(&(Family->Properties),
                  Properties,
                  sizeof(NETLINK_GENERIC_FAMILY_PROPERTIES));

    //
    // Acquire the family tree lock and attempt to insert this new family.
    //

    KeAcquireSharedExclusiveLockExclusive(NetNetlinkGenericFamilyLock);
    LockHeld = TRUE;

    //
    // Check to make sure the name is not a duplicate.
    //

    FoundNode = RtlRedBlackTreeGetLowestNode(&NetNetlinkGenericFamilyTree);
    while (FoundNode != NULL) {
        FoundFamily = RED_BLACK_TREE_VALUE(FoundNode,
                                           NETLINK_GENERIC_FAMILY,
                                           TreeNode);

        Match = RtlAreStringsEqual(FoundFamily->Properties.Name,
                                   Family->Properties.Name,
                                   NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH);

        if (Match != FALSE) {
            Status = STATUS_DUPLICATE_ENTRY;
            goto RegisterFamilyEnd;
        }
    }

    //
    // If the message type is zero, then one needs to be dynamically allocated.
    //

    if (Family->Properties.Id == 0) {
        Status = NetpNetlinkGenericAllocateFamilyId(&NewId);
        if (!KSUCCESS(Status)) {
            goto RegisterFamilyEnd;
        }

        Family->Properties.Id = NewId;

    //
    // Otherwise make sure the provided message type is not already in use.
    //

    } else {
        FoundNode = RtlRedBlackTreeSearch(&NetNetlinkGenericFamilyTree,
                                          &(Family->TreeNode));

        if (FoundNode != NULL) {
            Status = STATUS_DUPLICATE_ENTRY;
            goto RegisterFamilyEnd;
        }
    }

    //
    // Insert the new family into the tree.
    //

    RtlRedBlackTreeInsert(&NetNetlinkGenericFamilyTree, &(Family->TreeNode));
    Status = STATUS_SUCCESS;

RegisterFamilyEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetNetlinkGenericFamilyLock);
    }

    if (!KSUCCESS(Status)) {
        if (Family != NULL) {
            NetpNetlinkGenericFamilyReleaseReference(Family);
            Family = NULL;
        }
    }

    if (FamilyHandle != NULL) {
        *FamilyHandle = Family;
    }

    return Status;
}

NET_API
VOID
NetNetlinkGenericUnregisterFamily (
    PHANDLE FamilyHandle
    )

/*++

Routine Description:

    This routine unregisters the given generic netlink family.

Arguments:

    FamilyHandle - Supplies a pointer to the generic netlink family to
        unregister.

Return Value:

    None.

--*/

{

    PNETLINK_GENERIC_FAMILY Family;
    PNETLINK_GENERIC_FAMILY FoundFamily;
    PRED_BLACK_TREE_NODE FoundNode;
    BOOL LockHeld;

    Family = (PNETLINK_GENERIC_FAMILY)FamilyHandle;
    KeAcquireSharedExclusiveLockExclusive(NetNetlinkGenericFamilyLock);
    LockHeld = TRUE;
    FoundNode = RtlRedBlackTreeSearch(&NetNetlinkGenericFamilyTree,
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

    RtlRedBlackTreeRemove(&NetNetlinkGenericFamilyTree, &(Family->TreeNode));
    KeReleaseSharedExclusiveLockExclusive(NetNetlinkGenericFamilyLock);
    LockHeld = FALSE;

    //
    // Before releasing the last reference, make sure the family is not in the
    // middle of receiving packet. If it is, netcore could be able to call into
    // the driver that is unregistering the family. It would be bad for that
    // driver to disappear.
    //

    while (Family->ReferenceCount > 1) {
        KeYield();
    }

    NetpNetlinkGenericFamilyReleaseReference(Family);

UnregisterFamilyEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetNetlinkGenericFamilyLock);
    }

    return;
}

VOID
NetpNetlinkGenericInitialize (
    ULONG Phase
    )

/*++

Routine Description:

    This routine initializes support for UDP sockets.

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
        NetNetlinkGenericFamilyLock = KeCreateSharedExclusiveLock();
        if (NetNetlinkGenericFamilyLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeEnd;
        }

        RtlRedBlackTreeInitialize(&NetNetlinkGenericFamilyTree,
                                  0,
                                  NetpNetlinkGenericCompareFamilies);

        Status = NetRegisterProtocol(&NetNetlinkGenericProtocol, NULL);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

    //
    // In phase 1, create netcore's kernel-side generic netlink socket.
    //

    } else {

        ASSERT(Phase == 1);

        Status = IoSocketCreate(SocketNetworkNetlink,
                                SocketTypeDatagram,
                                SOCKET_INTERNET_PROTOCOL_NETLINK_GENERIC,
                                0,
                                &NetNetlinkGenericSocketHandle);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status = IoGetSocketFromHandle(NetNetlinkGenericSocketHandle,
                                       (PVOID)&NetNetlinkGenericSocket);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        //
        // Add the kernel flag and bind it to port 0.
        //

        RtlAtomicOr32(&(NetNetlinkGenericSocket->Flags),
                      NET_SOCKET_FLAG_KERNEL);

        RtlZeroMemory(&Address, sizeof(NETLINK_ADDRESS));
        Address.Network = SocketNetworkNetlink;
        Status = IoSocketBindToAddress(TRUE,
                                       NetNetlinkGenericSocketHandle,
                                       NULL,
                                       &(Address.NetworkAddress),
                                       NULL,
                                       0);

        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        NetpNetlinkGenericControlInitialize();
    }

InitializeEnd:

    ASSERT(KSUCCESS(Status));

    return;
}

KSTATUS
NetpNetlinkGenericCreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket
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
        structure after this routine returns.

Return Value:

    Status code.

--*/

{

    PNETLINK_GENERIC_SOCKET GenericSocket;
    PNET_NETWORK_INITIALIZE_SOCKET InitializeSocket;
    ULONG MaxPacketSize;
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation;
    KSTATUS Status;

    ASSERT(ProtocolEntry->Type == SocketTypeDatagram);
    ASSERT(NetworkProtocol == ProtocolEntry->ParentProtocolNumber);
    ASSERT(NetworkProtocol == SOCKET_INTERNET_PROTOCOL_NETLINK_GENERIC);

    GenericSocket = MmAllocatePagedPool(sizeof(NETLINK_GENERIC_SOCKET),
                                        NETLINK_GENERIC_ALLOCATION_TAG);

    if (GenericSocket == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto NetlinkGenericCreateSocketEnd;
    }

    RtlZeroMemory(GenericSocket, sizeof(NETLINK_GENERIC_SOCKET));
    GenericSocket->NetSocket.KernelSocket.Protocol = NetworkProtocol;
    GenericSocket->NetSocket.KernelSocket.ReferenceCount = 1;
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
        goto NetlinkGenericCreateSocketEnd;
    }

    //
    // Give the lower layers a chance to initialize. Start the maximum packet
    // size at the largest possible value.
    //

    PacketSizeInformation = &(GenericSocket->NetSocket.PacketSizeInformation);
    PacketSizeInformation->MaxPacketSize = MAX_ULONG;
    InitializeSocket = NetworkEntry->Interface.InitializeSocket;
    Status = InitializeSocket(ProtocolEntry,
                              NetworkEntry,
                              NetworkProtocol,
                              &(GenericSocket->NetSocket));

    if (!KSUCCESS(Status)) {
        goto NetlinkGenericCreateSocketEnd;
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

    Status = STATUS_SUCCESS;

NetlinkGenericCreateSocketEnd:
    if (!KSUCCESS(Status)) {
        if (GenericSocket != NULL) {
            if (GenericSocket->ReceiveLock != NULL) {
                KeDestroyQueuedLock(GenericSocket->ReceiveLock);
            }

            MmFreePagedPool(GenericSocket);
            GenericSocket = NULL;
        }
    }

    *NewSocket = &(GenericSocket->NetSocket);
    return Status;
}

VOID
NetpNetlinkGenericDestroySocket (
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
    KeDestroyQueuedLock(GenericSocket->ReceiveLock);
    MmFreePagedPool(GenericSocket);
    return;
}

KSTATUS
NetpNetlinkGenericBindToAddress (
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

    KSTATUS Status;

    if (Socket->LocalAddress.Network != SocketNetworkInvalid) {
        Status = STATUS_INVALID_PARAMETER;
        goto NetlinkGenericBindToAddressEnd;
    }

    //
    // Only netlink addresses are supported.
    //

    if (Address->Network != SocketNetworkNetlink) {
        Status = STATUS_NOT_SUPPORTED;
        goto NetlinkGenericBindToAddressEnd;
    }

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.BindToAddress(Socket, Link, Address);
    if (!KSUCCESS(Status)) {
        goto NetlinkGenericBindToAddressEnd;
    }

    //
    // Begin listening immediately, as there is no explicit listen step for
    // generic netlink sockets.
    //

    Status = Socket->Network->Interface.Listen(Socket);
    if (!KSUCCESS(Status)) {
        goto NetlinkGenericBindToAddressEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);

NetlinkGenericBindToAddressEnd:
    return Status;
}

KSTATUS
NetpNetlinkGenericListen (
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
NetpNetlinkGenericAccept (
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
NetpNetlinkGenericConnect (
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

    KSTATUS Status;

    //
    // Pass the request down to the network layer.
    //

    Status = Socket->Network->Interface.Connect(Socket, Address);
    if (!KSUCCESS(Status)) {
        goto NetlinkGenericConnectEnd;
    }

    IoSetIoObjectState(Socket->KernelSocket.IoState, POLL_EVENT_OUT, TRUE);

NetlinkGenericConnectEnd:
    return Status;
}

KSTATUS
NetpNetlinkGenericClose (
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
        goto NetlinkGenericCloseEnd;
    }

    IoSocketReleaseReference(&(Socket->KernelSocket));

NetlinkGenericCloseEnd:
    return Status;
}

KSTATUS
NetpNetlinkGenericShutdown (
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
NetpNetlinkGenericSend (
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
            goto NetlinkGenericSendEnd;
        }
    }

    if ((Destination == NULL) ||
        (Destination->Network == SocketNetworkInvalid)) {

        if (Socket->BindingType != SocketFullyBound) {
            Status = STATUS_NOT_CONFIGURED;
            goto NetlinkGenericSendEnd;
        }

        Destination = &(Socket->RemoteAddress);
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto NetlinkGenericSendEnd;
    }

    //
    // If the size is greater than the generic netlink socket's maximum packet
    // size, fail.
    //

    if (Size > GenericSocket->MaxPacketSize) {
        Status = STATUS_MESSAGE_TOO_LONG;
        goto NetlinkGenericSendEnd;
    }

    //
    // If the socket is not yet bound, then at least try to bind it to a local
    // port. This bind attempt may race with another bind attempt, but leave it
    // to the socket owner to synchronize bind and send.
    //

    if (Socket->BindingType == SocketBindingInvalid) {
        RtlZeroMemory(&LocalAddress, sizeof(NETWORK_ADDRESS));
        LocalAddress.Network = Socket->Network->Type;
        Status = NetpNetlinkGenericBindToAddress(Socket, NULL, &LocalAddress);
        if (!KSUCCESS(Status)) {
            goto NetlinkGenericSendEnd;
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
        goto NetlinkGenericSendEnd;
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
        goto NetlinkGenericSendEnd;
    }

    //
    // Send the packet down to the network layer.
    //

    Status = Socket->Network->Interface.Send(Socket,
                                             Destination,
                                             NULL,
                                             &PacketList);

    if (!KSUCCESS(Status)) {
        goto NetlinkGenericSendEnd;
    }

    Packet = NULL;
    BytesComplete = Size;

NetlinkGenericSendEnd:
    Parameters->Size = BytesComplete;
    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&PacketList);
    }

    return Status;
}

VOID
NetpNetlinkGenericProcessReceivedData (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress,
    PNET_PROTOCOL_ENTRY ProtocolEntry
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    Link - Supplies a pointer to the link that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This routine takes ownership of this structure and will either pass it
        along for later reading by the destination socket or release it.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

    ProtocolEntry - Supplies a pointer to this protocol's protocol entry.

Return Value:

    None.

--*/

{

    ULONG AllocationSize;
    BOOL FreePacket;
    PNETLINK_GENERIC_SOCKET GenericSocket;
    PNETLINK_HEADER Header;
    PNETLINK_GENERIC_RECEIVED_PACKET ReceivePacket;
    PNET_SOCKET Socket;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // This routine is an exception to the rule handling the supplied network
    // packet buffer. As the buffer is not backed by a physical device, it can
    // be directly passed to the received socket, avoiding a copy.
    //

    FreePacket = TRUE;
    Header = (PNETLINK_HEADER)(Packet->Buffer + Packet->DataOffset);
    if (Header->Length > (Packet->FooterOffset - Packet->DataOffset)) {
        RtlDebugPrint("Invalid generic netlink length %d is bigger than packet "
                      "data, which is only %d bytes large.\n",
                      Header->Length,
                      (Packet->FooterOffset - Packet->DataOffset));

        goto ProcessReceivedDataEnd;
    }

    if (Header->Length < sizeof(NETLINK_HEADER)) {
        RtlDebugPrint("Invalid generic netlink length %d is smaller than the "
                      "netlink header size %d\n",
                      Header->Length,
                      sizeof(NETLINK_HEADER));

        goto ProcessReceivedDataEnd;
    }

    //
    // If this is a multicast packet, then send it to all appropriate sockets
    // with the help of netcore. Once complete, release the packet.
    //

    if ((Packet->Flags & NET_PACKET_FLAG_MULTICAST) != 0) {
        NetProcessReceivedMulticastData(Link,
                                        Packet,
                                        ProtocolEntry,
                                        SourceAddress,
                                        DestinationAddress);

        goto ProcessReceivedDataEnd;
    }

    //
    // Find a socket willing to take this packet.
    //

    Socket = NetFindSocket(ProtocolEntry, DestinationAddress, SourceAddress);
    if (Socket == NULL) {
        goto ProcessReceivedDataEnd;
    }

    GenericSocket = (PNETLINK_GENERIC_SOCKET)Socket;

    //
    // If this is a kernel socket is on the receiving end, then route the
    // packet directly to the kernel component.
    //

    if ((Socket->Flags & NET_SOCKET_FLAG_KERNEL) != 0) {
        NetpNetlinkGenericProcessReceivedKernelData(Link,
                                                    Socket,
                                                    Packet,
                                                    SourceAddress,
                                                    DestinationAddress);

        goto ProcessReceivedDataEnd;
    }

    //
    // Create a received packet entry for this data.
    //

    AllocationSize = sizeof(NETLINK_GENERIC_RECEIVED_PACKET);
    ReceivePacket = MmAllocatePagedPool(AllocationSize,
                                        NETLINK_GENERIC_ALLOCATION_TAG);

    if (ReceivePacket == NULL) {
        goto ProcessReceivedDataEnd;
    }

    RtlCopyMemory(&(ReceivePacket->Address),
                  SourceAddress,
                  sizeof(NETWORK_ADDRESS));

    //
    // Netlink sockets are an exception to the rule of the packet not being
    // touched after this routine returns. The packet is not owned by a link
    // and thus is not backed by device memory. So it is safe to borrow it.
    //

    ReceivePacket->NetPacket = Packet;
    FreePacket = FALSE;

    //
    // Work to insert the packet on the list of received packets.
    //

    NetpNetlinkGenericInsertReceivedPacket(GenericSocket, ReceivePacket);

    //
    // Release the reference on the socket added by the find socket call.
    //

    IoSocketReleaseReference(&(Socket->KernelSocket));

ProcessReceivedDataEnd:
    if (FreePacket != FALSE) {
        NetFreeBuffer(Packet);
    }

    return;
}

VOID
NetpNetlinkGenericProcessReceivedSocketData (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine is called for a particular socket to process a received packet
    that was sent to it.

Arguments:

    Link - Supplies a pointer to the network link that received the packet.

    Socket - Supplies a pointer to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may not be used as a scratch space and must not be
        modified by this routine.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

Return Value:

    None.

--*/

{

    ULONG AllocationSize;
    PNETLINK_GENERIC_SOCKET GenericSocket;
    PNET_PACKET_BUFFER PacketCopy;
    ULONG PacketLength;
    PNETLINK_GENERIC_RECEIVED_PACKET ReceivePacket;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    PacketCopy = NULL;
    GenericSocket = (PNETLINK_GENERIC_SOCKET)Socket;

    //
    // If this is a kernel socket is on the receiving end, then route the
    // packet directly to the kernel component.
    //

    if ((Socket->Flags & NET_SOCKET_FLAG_KERNEL) != 0) {
        NetpNetlinkGenericProcessReceivedKernelData(Link,
                                                    Socket,
                                                    Packet,
                                                    SourceAddress,
                                                    DestinationAddress);

        return;
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
                  SourceAddress,
                  sizeof(NETWORK_ADDRESS));

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
    // Work to insert the packet on the list of received packets.
    //

    NetpNetlinkGenericInsertReceivedPacket(GenericSocket, ReceivePacket);

ProcessReceivedSocketDataEnd:
    if (!KSUCCESS(Status)) {
        if (ReceivePacket != NULL) {
            MmFreePagedPool(ReceivePacket);
        }

        if (PacketCopy != NULL) {
            NetFreeBuffer(PacketCopy);
        }
    }

    return;
}

KSTATUS
NetpNetlinkGenericReceive (
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

    ULONG AvailableBytes;
    PVOID Buffer;
    UINTN BytesComplete;
    COMPARISON_RESULT Compare;
    PLIST_ENTRY CurrentEntry;
    ULONGLONG CurrentTime;
    ULONGLONG EndTime;
    ULONG Flags;
    PNETLINK_GENERIC_SOCKET GenericSocket;
    PIO_OBJECT_STATE IoState;
    ULONG OriginallyAvailable;
    PNETLINK_GENERIC_RECEIVED_PACKET Packet;
    ULONG PacketSize;
    ULONG ReturnedEvents;
    UINTN Size;
    NETWORK_ADDRESS SourceAddressLocal;
    KSTATUS Status;
    ULONGLONG TimeCounterFrequency;
    ULONG Timeout;
    ULONG WaitTime;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    BytesComplete = 0;
    EndTime = 0;
    Flags = Parameters->SocketIoFlags;
    Parameters->SocketIoFlags = 0;
    if ((Flags & SOCKET_IO_OUT_OF_BAND) != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto NetlinkGenericReceiveEnd;
    }

    //
    // Fail if there's ancillary data.
    //

    if (Parameters->ControlDataSize != 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto NetlinkGenericReceiveEnd;
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
    // Loop trying to get some data.
    //

    Status = STATUS_SUCCESS;
    while (BytesComplete < Size) {

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
        // Otherwise when the read event is signalled, there are at least the
        // receive minimum bytes available.
        //

        IoState = GenericSocket->NetSocket.KernelSocket.IoState;
        Status = IoWaitForIoObjectState(IoState,
                                        POLL_EVENT_IN,
                                        TRUE,
                                        WaitTime,
                                        &ReturnedEvents);

        if (!KSUCCESS(Status)) {
            break;
        }

        if ((ReturnedEvents & POLL_ERROR_EVENTS) != 0) {
            if ((ReturnedEvents & POLL_EVENT_DISCONNECTED) != 0) {
                Status = STATUS_NO_NETWORK_CONNECTION;

            } else {
                Status = NET_SOCKET_GET_LAST_ERROR(&(GenericSocket->NetSocket));
                if (KSUCCESS(Status)) {
                    Status = STATUS_DEVICE_IO_ERROR;
                }
            }

            break;
        }

        KeAcquireQueuedLock(GenericSocket->ReceiveLock);
        OriginallyAvailable = GenericSocket->ReceiveBufferTotalSize -
                              GenericSocket->ReceiveBufferFreeSize;

        CurrentEntry = GenericSocket->ReceivedPacketList.Next;
        while (CurrentEntry != &(GenericSocket->ReceivedPacketList)) {
            Packet = LIST_VALUE(CurrentEntry,
                                NETLINK_GENERIC_RECEIVED_PACKET,
                                ListEntry);

            CurrentEntry = CurrentEntry->Next;

            //
            // Don't cross boundaries between different remotes.
            //

            if (BytesComplete != 0) {
                Compare = NetCompareNetworkAddresses(&SourceAddressLocal,
                                                     &(Packet->Address));

                if (Compare != ComparisonResultSame) {
                    break;
                }
            }

            PacketSize = Packet->NetPacket->FooterOffset -
                         Packet->NetPacket->DataOffset;

            if (PacketSize > (Size - BytesComplete)) {
                if (BytesComplete != 0) {
                    break;
                }

                Parameters->SocketIoFlags |= SOCKET_IO_DATA_TRUNCATED;
                PacketSize = Size - BytesComplete;
                Status = STATUS_BUFFER_TOO_SMALL;
            }

            Buffer = Packet->NetPacket->Buffer + Packet->NetPacket->DataOffset;
            Status = MmCopyIoBufferData(IoBuffer,
                                        Buffer,
                                        BytesComplete,
                                        PacketSize,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                break;
            }

            BytesComplete += PacketSize;

            //
            // Copy the packet address over to the local variable to avoid
            // crossing remote host boundaries.
            //

            RtlCopyMemory(&SourceAddressLocal,
                          &(Packet->Address),
                          sizeof(NETWORK_ADDRESS));

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
                        break;
                    }
                }
            }

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
            }
        }

        //
        // Unsignal the IN event if this read caused the avaiable data to drop
        // below the minimum.
        //

        AvailableBytes = GenericSocket->ReceiveBufferTotalSize -
                         GenericSocket->ReceiveBufferFreeSize;

        if ((OriginallyAvailable >= GenericSocket->ReceiveMinimum) &&
            (AvailableBytes < GenericSocket->ReceiveMinimum)) {

            IoSetIoObjectState(GenericSocket->NetSocket.KernelSocket.IoState,
                               POLL_EVENT_IN,
                               FALSE);
        }

        KeReleaseQueuedLock(GenericSocket->ReceiveLock);
        if (!KSUCCESS(Status)) {
            break;
        }

        //
        // Unless being forced to receive everything, break out if something
        // was read.
        //

        if ((Flags & SOCKET_IO_WAIT_ALL) == 0) {
            if (BytesComplete != 0) {
                break;
            }
        }
    }

NetlinkGenericReceiveEnd:
    Parameters->Size = BytesComplete;
    return Status;
}

KSTATUS
NetpNetlinkGenericGetSetInformation (
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

--*/

{

    SOCKET_BASIC_OPTION BasicOption;
    PBOOL BooleanOption;
    PNETLINK_GENERIC_SOCKET GenericSocket;
    PNET_PACKET_SIZE_INFORMATION SizeInformation;
    PULONG SizeOption;
    KSTATUS Status;
    PULONG TimeoutOption;

    GenericSocket = (PNETLINK_GENERIC_SOCKET)Socket;
    if ((InformationType != SocketInformationTypeBasic) &&
        (InformationType != SocketInformationTypeNetlinkGeneric)) {

        Status = STATUS_INVALID_PARAMETER;
        goto UdpGetSetInformationEnd;
    }

    Status = STATUS_SUCCESS;
    if (InformationType == SocketInformationTypeBasic) {
        BasicOption = (SOCKET_BASIC_OPTION)Option;
        switch (BasicOption) {
        case SocketBasicOptionSendBufferSize:
            if (Set != FALSE) {
                if (*DataSize != sizeof(ULONG)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                SizeOption = (PULONG)Data;
                SizeInformation = &(Socket->PacketSizeInformation);
                if (*SizeOption > NETLINK_GENERIC_MAX_PACKET_SIZE) {
                    GenericSocket->MaxPacketSize =
                                               NETLINK_GENERIC_MAX_PACKET_SIZE;

                } else if (*SizeOption < SizeInformation->MaxPacketSize) {
                    GenericSocket->MaxPacketSize =
                                                SizeInformation->MaxPacketSize;

                } else {
                    GenericSocket->MaxPacketSize = *SizeOption;
                }

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                SizeOption = (PULONG)Data;
                *SizeOption = GenericSocket->MaxPacketSize;
            }

            break;

        case SocketBasicOptionSendMinimum:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                SizeOption = (PULONG)Data;
                *SizeOption = NETLINK_GENERIC_SEND_MINIMUM;
            }

            break;

        case SocketBasicOptionSendTimeout:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                TimeoutOption = (PULONG)Data;
                *TimeoutOption = WAIT_TIME_INDEFINITE;
            }

            break;

        case SocketBasicOptionReceiveBufferSize:
            if (Set != FALSE) {
                if (*DataSize != sizeof(ULONG)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                SizeOption = (PULONG)Data;
                KeAcquireQueuedLock(GenericSocket->ReceiveLock);
                if (*SizeOption < NETLINK_GENERIC_MIN_RECEIVE_BUFFER_SIZE) {
                    GenericSocket->ReceiveBufferTotalSize =
                                       NETLINK_GENERIC_MIN_RECEIVE_BUFFER_SIZE;

                } else {
                    GenericSocket->ReceiveBufferTotalSize = *SizeOption;
                }

                //
                // Truncate the available free space if necessary. Do not
                // remove any packets that have already been received. This is
                // not meant to be a truncate call.
                //

                if (GenericSocket->ReceiveBufferFreeSize >
                    GenericSocket->ReceiveBufferTotalSize) {

                    GenericSocket->ReceiveBufferFreeSize =
                                         GenericSocket->ReceiveBufferTotalSize;
                }

                KeReleaseQueuedLock(GenericSocket->ReceiveLock);

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                SizeOption = (PULONG)Data;
                *SizeOption = GenericSocket->ReceiveBufferTotalSize;
            }

            break;

        case SocketBasicOptionReceiveMinimum:
            if (Set != FALSE) {
                if (*DataSize != sizeof(ULONG)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                SizeOption = (PULONG)Data;
                GenericSocket->ReceiveMinimum = *SizeOption;

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                SizeOption = (PULONG)Data;
                *SizeOption = GenericSocket->ReceiveMinimum;
            }

            break;

        case SocketBasicOptionReceiveTimeout:
            if (Set != FALSE) {
                if (*DataSize != sizeof(ULONG)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                TimeoutOption = (PULONG)Data;
                GenericSocket->ReceiveTimeout = *TimeoutOption;

            } else {
                if (*DataSize < sizeof(ULONG)) {
                    *DataSize = sizeof(ULONG);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                TimeoutOption = (PULONG)Data;
                *TimeoutOption = GenericSocket->ReceiveTimeout;
            }

            break;

        case SocketBasicOptionAcceptConnections:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;

            } else {
                if (*DataSize < sizeof(BOOL)) {
                    *DataSize = sizeof(BOOL);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                *BooleanOption = FALSE;
            }

            break;

        case SocketBasicOptionBroadcastEnabled:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;

            } else {
                if (*DataSize < sizeof(BOOL)) {
                    *DataSize = sizeof(BOOL);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                *BooleanOption = FALSE;
            }

            break;

        default:
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

    } else {

        ASSERT(InformationType == SocketInformationTypeNetlinkGeneric);

        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
    }

UdpGetSetInformationEnd:
    return Status;
}

KSTATUS
NetpNetlinkGenericUserControl (
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

PNETLINK_GENERIC_FAMILY
NetpNetlinkGenericLookupFamilyById (
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
    KeAcquireSharedExclusiveLockShared(NetNetlinkGenericFamilyLock);
    FoundNode = RtlRedBlackTreeSearch(&NetNetlinkGenericFamilyTree,
                                      &(SearchFamily.TreeNode));

    if (FoundNode != NULL) {
        FoundFamily = RED_BLACK_TREE_VALUE(FoundNode,
                                           NETLINK_GENERIC_FAMILY,
                                           TreeNode);

        NetpNetlinkGenericFamilyAddReference(FoundFamily);
    }

    KeReleaseSharedExclusiveLockShared(NetNetlinkGenericFamilyLock);
    return FoundFamily;
}

PNETLINK_GENERIC_FAMILY
NetpNetlinkGenericLookupFamilyByName (
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
    KeAcquireSharedExclusiveLockShared(NetNetlinkGenericFamilyLock);
    Node = RtlRedBlackTreeGetLowestNode(&NetNetlinkGenericFamilyTree);
    while (Node != NULL) {
        Family = RED_BLACK_TREE_VALUE(Node, NETLINK_GENERIC_FAMILY, TreeNode);
        Match = RtlAreStringsEqual(Family->Properties.Name,
                                   FamilyName,
                                   NETLINK_GENERIC_MAX_FAMILY_NAME_LENGTH);

        if (Match != FALSE) {
            FoundFamily = Family;
            NetpNetlinkGenericFamilyAddReference(FoundFamily);
            break;
        }
    }

    KeReleaseSharedExclusiveLockShared(NetNetlinkGenericFamilyLock);
    return FoundFamily;
}

VOID
NetpNetlinkGenericFamilyAddReference (
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
NetpNetlinkGenericFamilyReleaseReference (
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
        NetpNetlinkGenericDestroyFamily(Family);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpNetlinkGenericInsertReceivedPacket (
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

    ULONG AvailableBytes;
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
        // Signal the event if enough bytes have been received.
        //

        AvailableBytes = Socket->ReceiveBufferTotalSize -
                         Socket->ReceiveBufferFreeSize;

        if (AvailableBytes >= Socket->ReceiveMinimum) {
            IoSetIoObjectState(Socket->NetSocket.KernelSocket.IoState,
                               POLL_EVENT_IN,
                               TRUE);
        }

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

VOID
NetpNetlinkGenericProcessReceivedKernelData (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine is called to process a packet received by a kernel socket.

Arguments:

    Link - Supplies a pointer to the link that received the packet.

    Socket - Supplies a pointer to the network socket that received the packet.

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

    None.

--*/

{

    PNETLINK_GENERIC_FAMILY Family;
    PNETLINK_HEADER Header;

    //
    // There should only be one generic netlink kernel socket.
    //

    ASSERT(Socket == NetNetlinkGenericSocket);

    //
    // Validate the standard netlink header for a kernel socket. The length
    // should have already been evaluated.
    //

    Header = (PNETLINK_HEADER)(Packet->Buffer + Packet->DataOffset);

    ASSERT(Header->Length >= sizeof(NETLINK_HEADER));
    ASSERT(Header->Length <= (Packet->FooterOffset - Packet->DataOffset));

    //
    // The kernel only handles requests.
    //

    if ((Header->Flags & NETLINK_HEADER_FLAG_REQUEST) == 0) {
        goto ProcessReceivedKernelDataEnd;
    }

    //
    // The protocol layer does not handle control messages.
    //

    if (Header->Type < NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM) {
        goto ProcessReceivedKernelDataEnd;
    }

    //
    // Find the generic netlink type interface and call the reception routine.
    //

    Family = NetpNetlinkGenericLookupFamilyById(Header->Type);
    if (Family == NULL) {
        goto ProcessReceivedKernelDataEnd;
    }

    Family->Properties.Interface.ProcessReceivedData(
                                                 NetNetlinkGenericSocketHandle,
                                                 Packet,
                                                 SourceAddress,
                                                 DestinationAddress);

    NetpNetlinkGenericFamilyReleaseReference(Family);

ProcessReceivedKernelDataEnd:
    return;
}

COMPARISON_RESULT
NetpNetlinkGenericCompareFamilies (
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
NetpNetlinkGenericDestroyFamily (
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
NetpNetlinkGenericAllocateFamilyId (
    PULONG FamilyId
    )

/*++

Routine Description:

    This routine attempts to allocate a free generic netlink family ID.

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

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(NetNetlinkGenericFamilyLock));

    Status = STATUS_INSUFFICIENT_RESOURCES;
    NextId = NetNetlinkGenericFamilyNextId;

    //
    // Iterate until all the possible message types have been tried.
    //

    do {
        SearchFamily.Properties.Id = NextId;
        NextId += 1;
        if (NextId < NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM) {
            NextId = NETLINK_MESSAGE_TYPE_PROTOCOL_MINIMUM;
        }

        FoundNode = RtlRedBlackTreeSearch(&NetNetlinkGenericFamilyTree,
                                          &(SearchFamily.TreeNode));

        if (FoundNode == NULL) {
            *FamilyId = SearchFamily.Properties.Id;
            Status = STATUS_SUCCESS;
            break;
        }

    } while (NextId != NetNetlinkGenericFamilyNextId);

    //
    // Update the global to make the next search start where this left off.
    //

    NetNetlinkGenericFamilyNextId = NextId;
    return Status;
}

