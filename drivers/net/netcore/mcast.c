/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mcast.c

Abstract:

    This module implements generic multicast support for sockets and links.

Author:

    Chris Stevens 25-Aug-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "netcore.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpFindLinkForMulticastRequest (
    PNET_NETWORK_ENTRY Network,
    PNET_SOCKET_MULTICAST_REQUEST Request,
    PNET_LINK_LOCAL_ADDRESS LinkResult
    );

KSTATUS
NetpUpdateMulticastAddressFilters (
    PNET_LINK Link
    );

PNET_SOCKET_MULTICAST_GROUP
NetpFindSocketMulticastGroup (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS MulticastAddress
    );

PNET_SOCKET_MULTICAST_GROUP
NetpCreateSocketMulticastGroup (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS MulticastAddress
    );

VOID
NetpDestroySocketMulticastGroup (
    PNET_SOCKET_MULTICAST_GROUP Group
    );

KSTATUS
NetpAcquireSocketMulticastLock (
    PNET_SOCKET Socket
    );

VOID
NetpReleaseSocketMulticastLock (
    PNET_SOCKET Socket
    );

PNET_LINK_MULTICAST_GROUP
NetpFindLinkMulticastGroup (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS MulticastAddress
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

NET_API
KSTATUS
NetInitializeMulticastSocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine initializes a network socket's multicast information.

Arguments:

    Socket - Supplies a pointer to the network socket to initialize.

Return Value:

    Status code.

--*/

{

    ASSERT(Socket->MulticastLock == NULL);
    ASSERT(Socket->MulticastInterface.LinkInformation.Link == NULL);

    RtlAtomicOr32(&(Socket->Flags), NET_SOCKET_FLAG_MULTICAST_LOOPBACK);
    INITIALIZE_LIST_HEAD(&(Socket->MulticastGroupList));
    return STATUS_SUCCESS;
}

NET_API
VOID
NetDestroyMulticastSocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys all the multicast state associated with the given
    socket.

Arguments:

    Socket - Supplies a pointer to the socket whose multicast state is to be
        destroyed.

Return Value:

    None.

--*/

{

    PNET_SOCKET_MULTICAST_GROUP Group;
    PNET_LINK Link;

    Link = Socket->MulticastInterface.LinkInformation.Link;
    if (Link != NULL) {
        NetLinkReleaseReference(Link);
    }

    if (LIST_EMPTY(&(Socket->MulticastGroupList)) != FALSE) {
        goto DestroyMulticastSocket;
    }

    ASSERT(Socket->MulticastLock != NULL);

    //
    // Run through the local list, leave each multicast group and destroy the
    // group structures.
    //

    while (LIST_EMPTY(&(Socket->MulticastGroupList)) == FALSE) {
        Group = LIST_VALUE(Socket->MulticastGroupList.Next,
                           NET_SOCKET_MULTICAST_GROUP,
                           ListEntry);

        LIST_REMOVE(&(Group->ListEntry));
        NetLeaveLinkMulticastGroup(Group->Link,
                                   Group->LinkAddress,
                                   &(Group->MulticastAddress));

        NetpDestroySocketMulticastGroup(Group);
    }

DestroyMulticastSocket:
    if (Socket->MulticastLock != NULL) {
        KeDestroyQueuedLock(Socket->MulticastLock);
    }

    return;
}

NET_API
KSTATUS
NetJoinSocketMulticastGroup (
    PNET_SOCKET Socket,
    PNET_SOCKET_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine joins the given socket to a multicast group.

Arguments:

    Socket - Supplies a pointer to a socket.

    Request - Supplies a pointer to the multicast join request. This stores
        the address of the multicast group to join along with interface
        information to indicate which link should join the group.

Return Value:

    Status code.

--*/

{

    PNET_SOCKET_MULTICAST_GROUP Group;
    NET_LINK_LOCAL_ADDRESS LinkResult;
    BOOL LockHeld;
    PNET_NETWORK_ENTRY Network;
    PNET_SOCKET_MULTICAST_GROUP NewGroup;
    KSTATUS Status;

    LinkResult.Link = NULL;
    LockHeld = FALSE;
    Network = Socket->Network;
    NewGroup = NULL;

    //
    // Attempt to find a network link that can reach the multicast address, or
    // find the one specified by the request.
    //

    Status = NetpFindLinkForMulticastRequest(Network, Request, &LinkResult);
    if (!KSUCCESS(Status)) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto SocketJoinMulticastGroupEnd;
    }

    Status = NetpAcquireSocketMulticastLock(Socket);
    if (!KSUCCESS(Status)) {
        goto SocketJoinMulticastGroupEnd;
    }

    LockHeld = TRUE;

    //
    // Check to see if this socket already joined the group.
    //

    Group = NetpFindSocketMulticastGroup(Socket,
                                         LinkResult.Link,
                                         LinkResult.LinkAddress,
                                         &(Request->MulticastAddress));

    if (Group != NULL) {
        Status = STATUS_ADDRESS_IN_USE;
        goto SocketJoinMulticastGroupEnd;
    }

    //
    // Prepare for success and allocate a new socket multicast group.
    //

    NewGroup = NetpCreateSocketMulticastGroup(LinkResult.Link,
                                              LinkResult.LinkAddress,
                                              &(Request->MulticastAddress));

    if (NewGroup == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SocketJoinMulticastGroupEnd;
    }

    //
    // Before officially adding the multicast group to the socket, make sure
    // the link joins the multicast group as well. This requires updating the
    // hardware filters and sending network-specific messages to alert routers
    // that this node is joining the multicast group. This all must happen with
    // the socket's multicast link lock held to serialze with other join and
    // leave requests.
    //

    Status = NetJoinLinkMulticastGroup(LinkResult.Link,
                                       LinkResult.LinkAddress,
                                       &(Request->MulticastAddress));

    if (!KSUCCESS(Status)) {
        goto SocketJoinMulticastGroupEnd;
    }

    INSERT_BEFORE(&(NewGroup->ListEntry),
                  &(Socket->MulticastGroupList));

SocketJoinMulticastGroupEnd:
    if (LockHeld != FALSE) {
        NetpReleaseSocketMulticastLock(Socket);
    }

    if (LinkResult.Link != NULL) {
        NetLinkReleaseReference(LinkResult.Link);
    }

    if (!KSUCCESS(Status)) {
        if (NewGroup != NULL) {
            NetpDestroySocketMulticastGroup(NewGroup);
        }
    }

    return Status;
}

NET_API
KSTATUS
NetLeaveSocketMulticastGroup (
    PNET_SOCKET Socket,
    PNET_SOCKET_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine removes the given socket from a multicast group.

Arguments:

    Socket - Supplies a pointer to a socket.

    Request - Supplies a pointer to the multicast leave request. This stores
        the multicast group address to leave and the address of the interface
        on which the socket joined the group.

Return Value:

    Status code.

--*/

{

    PNET_SOCKET_MULTICAST_GROUP Group;
    NET_LINK_LOCAL_ADDRESS LinkResult;
    BOOL LockHeld;
    PNET_NETWORK_ENTRY Network;
    KSTATUS Status;

    Group = NULL;
    LinkResult.Link = NULL;
    LockHeld = FALSE;
    Network = Socket->Network;

    //
    // If the multicast group list is empty, then this socket never joined any
    // multicast groups.
    //

    if (LIST_EMPTY(&(Socket->MulticastGroupList)) != FALSE) {
        Status = STATUS_INVALID_ADDRESS;
        goto SocketLeaveMulticastGroupEnd;
    }

    ASSERT(Socket->MulticastLock != NULL);

    //
    // Attempt to find a network link that can reach the multicast address, or
    // find the one specified by the request.
    //

    Status = NetpFindLinkForMulticastRequest(Network, Request, &LinkResult);
    if (!KSUCCESS(Status)) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto SocketLeaveMulticastGroupEnd;
    }

    //
    // Search through the multicast groups for a matching entry.
    //

    Status = NetpAcquireSocketMulticastLock(Socket);
    if (!KSUCCESS(Status)) {
        goto SocketLeaveMulticastGroupEnd;
    }

    LockHeld = TRUE;
    Group = NetpFindSocketMulticastGroup(Socket,
                                         LinkResult.Link,
                                         LinkResult.LinkAddress,
                                         &(Request->MulticastAddress));

    if (Group == NULL) {
        Status = STATUS_INVALID_ADDRESS;
        goto SocketLeaveMulticastGroupEnd;
    }

    //
    // Notify the link that this socket is leaving the group. This will trigger
    // any network-specific protocol actions. The socket's multicast lock is
    // held over this operation, but there shouldn't be high contention on an
    // individual socket's lock.
    //

    Status = NetLeaveLinkMulticastGroup(Group->Link,
                                        Group->LinkAddress,
                                        &(Group->MulticastAddress));

    if (!KSUCCESS(Status)) {
        goto SocketLeaveMulticastGroupEnd;
    }

    //
    // Remove the group from the list.
    //

    LIST_REMOVE(&(Group->ListEntry));
    NetpDestroySocketMulticastGroup(Group);

SocketLeaveMulticastGroupEnd:
    if (LockHeld != FALSE) {
        NetpReleaseSocketMulticastLock(Socket);
    }

    if (LinkResult.Link != NULL) {
        NetLinkReleaseReference(LinkResult.Link);
    }

    return Status;
}

NET_API
KSTATUS
NetSetSocketMulticastInterface (
    PNET_SOCKET Socket,
    PNET_SOCKET_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine sets a socket's default multicast interface.

Arguments:

    Socket - Supplies a pointer a socket.

    Request - Supplies a pointer to the request which dictates the default
        interface.

Return Value:

    Status code.

--*/

{

    NET_ADDRESS_TYPE AddressType;
    NET_LINK_LOCAL_ADDRESS LinkResult;
    NET_SOCKET_LINK_OVERRIDE NewInterface;
    PNET_LINK OldInterfaceLink;
    KSTATUS Status;

    NewInterface.LinkInformation.Link = NULL;
    LinkResult.Link = NULL;

    //
    // Find the appropriate link and link address entry for the specified
    // interface. If no interface is specified (an ID of zero and the
    // unspecified interface address), then reset the multicast interface.
    //

    AddressType = NetAddressUnicast;
    if (Request->InterfaceId == 0) {
        AddressType = Socket->Network->Interface.GetAddressType(
                                                 NULL,
                                                 NULL,
                                                 &(Request->InterfaceAddress));
    }

    if (AddressType == NetAddressAny) {
        RtlZeroMemory(&NewInterface, sizeof(NET_SOCKET_LINK_OVERRIDE));

    } else {
        Status = NetpFindLinkForMulticastRequest(Socket->Network,
                                                 Request,
                                                 &LinkResult);

        if (!KSUCCESS(Status)) {
            goto SetSocketMulticastInterface;
        }

        NetInitializeSocketLinkOverride(Socket, &LinkResult, &NewInterface);
    }

    Status = NetpAcquireSocketMulticastLock(Socket);
    if (!KSUCCESS(Status)) {
        goto SetSocketMulticastInterface;
    }

    OldInterfaceLink = Socket->MulticastInterface.LinkInformation.Link;
    RtlCopyMemory(&(Socket->MulticastInterface),
                  &NewInterface,
                  sizeof(NET_SOCKET_LINK_OVERRIDE));

    NetpReleaseSocketMulticastLock(Socket);
    if (OldInterfaceLink != NULL) {
        NetLinkReleaseReference(OldInterfaceLink);
    }

    //
    // The link reference was passed to the socket's multicast interface.
    //

    NewInterface.LinkInformation.Link = NULL;

SetSocketMulticastInterface:
    if (NewInterface.LinkInformation.Link != NULL) {
        NetLinkReleaseReference(NewInterface.LinkInformation.Link);
    }

    if (LinkResult.Link != NULL) {
        NetLinkReleaseReference(LinkResult.Link);
    }

    return Status;
}

NET_API
KSTATUS
NetGetSocketMulticastInterface (
    PNET_SOCKET Socket,
    PNET_SOCKET_MULTICAST_REQUEST Request
    )

/*++

Routine Description:

    This routine gets a socket's default multicast interface.

Arguments:

    Socket - Supplies a pointer a socket.

    Request - Supplies a pointer that receives the current interface.

Return Value:

    Status code.

--*/

{

    PNET_LINK Link;
    KSTATUS Status;

    Status = NetpAcquireSocketMulticastLock(Socket);
    if (!KSUCCESS(Status)) {
        goto GetSocketMulticastInterface;
    }

    RtlZeroMemory(&(Request->MulticastAddress), sizeof(NETWORK_ADDRESS));
    RtlCopyMemory(&(Request->InterfaceAddress),
                  &(Socket->MulticastInterface.LinkInformation.SendAddress),
                  sizeof(NETWORK_ADDRESS));

    Link = Socket->MulticastInterface.LinkInformation.Link;
    Request->InterfaceId = IoGetDeviceNumericId(Link->Properties.Device);
    NetpReleaseSocketMulticastLock(Socket);

GetSocketMulticastInterface:
    return Status;
}

NET_API
KSTATUS
NetJoinLinkMulticastGroup (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS MulticastAddress
    )

/*++

Routine Description:

    This routine joins the multicast group on a link. If this is the first
    request to join the supplied multicast group on the link, then the hardware
    is reprogrammed to include messages to the multicast group's physical layer
    address and the network is invoked to announce the join via a
    network-specific protocol.

Arguments:

    Link - Supplies a pointer to the network link joining the multicast group.

    LinkAddress - Supplies a pointer to the link address entry via which the
        link will join the group.

    MulticastAddress - Supplies a pointer to the multicast address of the group
        to join.

Return Value:

    Status code.

--*/

{

    PNET_LINK_MULTICAST_GROUP Group;
    BOOL LockHeld;
    PNET_NETWORK_ENTRY Network;
    PNET_LINK_MULTICAST_GROUP NewGroup;
    NET_NETWORK_MULTICAST_REQUEST Request;
    KSTATUS Status;

    LockHeld = FALSE;
    Network = LinkAddress->Network;
    NewGroup = NULL;

    //
    // This isn't going to get very far without network multicast support or
    // hardware filtering/promiscuous support.
    //

    if ((Network->Interface.JoinLeaveMulticastGroup == NULL) ||
        ((Link->Properties.Capabilities &
          NET_LINK_CAPABILITY_PROMISCUOUS_MODE) == 0)) {

        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        goto LinkJoinMulticastGroupEnd;
    }

    //
    // Search the link for the multicast group. If a matching group is found,
    // add to the count for this join request. A previous join already updated
    // the hardware filters and kicked off the network-specific join protocol.
    //

    while (TRUE) {
        KeAcquireQueuedLock(Link->QueuedLock);
        LockHeld = TRUE;
        Group = NetpFindLinkMulticastGroup(Link, LinkAddress, MulticastAddress);
        if (Group != NULL) {
            Group->JoinCount += 1;
            Status = STATUS_SUCCESS;
            goto LinkJoinMulticastGroupEnd;
        }

        //
        // If a group is not found the first time, release the lock and create
        // one before looping to search again.
        //

        if (NewGroup == NULL) {
            KeReleaseQueuedLock(Link->QueuedLock);
            LockHeld = FALSE;
            NewGroup = MmAllocatePagedPool(sizeof(NET_LINK_MULTICAST_GROUP),
                                           NET_CORE_ALLOCATION_TAG);

            if (NewGroup == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto LinkJoinMulticastGroupEnd;
            }

            RtlZeroMemory(NewGroup, sizeof(NET_LINK_MULTICAST_GROUP));
            NewGroup->LinkAddress = LinkAddress;
            NewGroup->JoinCount = 1;
            RtlCopyMemory(&(NewGroup->Address),
                          MulticastAddress,
                          sizeof(NETWORK_ADDRESS));

            continue;
        }

        //
        // No group was found a second time. Add the newly allocated group to
        // the link's list.
        //

        INSERT_BEFORE(&(NewGroup->ListEntry), &(Link->MulticastGroupList));
        break;
    }

    //
    // The hardware filters needs to be updated. The filters are updated with
    // the lock held as every group's address needs to be sent to the hardware.
    // It would also be bad to have a second join call run through before the
    // hardware is initialized.
    //

    Status = NetpUpdateMulticastAddressFilters(Link);
    if (!KSUCCESS(Status)) {
        LIST_REMOVE(&(NewGroup->ListEntry));
        goto LinkJoinMulticastGroupEnd;
    }

    NewGroup = NULL;
    KeReleaseQueuedLock(Link->QueuedLock);
    LockHeld = FALSE;

    //
    // Invoke the network layer to communicate that this link has joined the
    // multicast group. If this fails, make an attempt to leave the group.
    //

    Request.Link = Link;
    Request.LinkAddress = LinkAddress;
    Request.MulticastAddress = MulticastAddress;
    Status = Network->Interface.JoinLeaveMulticastGroup(&Request, TRUE);
    if (!KSUCCESS(Status)) {
        NetLeaveLinkMulticastGroup(Link, LinkAddress, MulticastAddress);
        goto LinkJoinMulticastGroupEnd;
    }

LinkJoinMulticastGroupEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Link->QueuedLock);
    }

    if (NewGroup != NULL) {
        MmFreePagedPool(NewGroup);
    }

    return Status;
}

NET_API
KSTATUS
NetLeaveLinkMulticastGroup (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS MulticastAddress
    )

/*++

Routine Description:

    This routine removes a link from a multicast. If this is the last request
    to leave a multicast group on the link, then the hardware is reprogrammed
    to filter out messages to the multicast group and a network-specific
    protocol is invoked to announce the link is leaving the group.

Arguments:

    Link - Supplies a pointer to the network link leaving the multicast group.

    LinkAddress - Supplies a pointer to the link address entry via which the
        link will leave the group.

    MulticastAddress - Supplies a pointer to the multicast address of the group
        to leave.

Return Value:

    Status code.

--*/

{

    PNET_LINK_MULTICAST_GROUP Group;
    BOOL LockHeld;
    PNET_NETWORK_ENTRY Network;
    NET_NETWORK_MULTICAST_REQUEST Request;
    KSTATUS Status;

    LockHeld = FALSE;
    Network = LinkAddress->Network;

    //
    // Search the link for the multicast group. If a matching group is not
    // found then the request fails.
    //

    KeAcquireQueuedLock(Link->QueuedLock);
    LockHeld = TRUE;
    Group = NetpFindLinkMulticastGroup(Link, LinkAddress, MulticastAddress);
    if (Group == NULL) {
        Status = STATUS_INVALID_ADDRESS;
        goto LinkLeaveMulticastGroupEnd;
    }

    //
    // If this is not the last reference on the group, the call is successful,
    // but takes no further action. The link as whole remains joined to the
    // multicast group.
    //

    Group->JoinCount -= 1;
    if (Group->JoinCount != 0) {
        Status = STATUS_SUCCESS;
        goto LinkLeaveMulticastGroupEnd;
    }

    //
    // Otherwise it's time for the group to go.
    //

    LIST_REMOVE(&(Group->ListEntry));

    //
    // Now that the group is out of the list, update the filters.
    //

    Status = NetpUpdateMulticastAddressFilters(Link);
    if (!KSUCCESS(Status)) {
        Group->JoinCount = 1;
        INSERT_BEFORE(&(Group->ListEntry), &(Link->MulticastGroupList));
        goto LinkLeaveMulticastGroupEnd;
    }

    //
    // Release the lock and trigger the network-specific work to announce that
    // this link has left the group.
    //

    KeReleaseQueuedLock(Link->QueuedLock);
    LockHeld = FALSE;
    Request.Link = Link;
    Request.LinkAddress = LinkAddress;
    Request.MulticastAddress = MulticastAddress;
    Network->Interface.JoinLeaveMulticastGroup(&Request, FALSE);
    MmFreePagedPool(Group);

LinkLeaveMulticastGroupEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Link->QueuedLock);
    }

    return Status;
}

NET_API
VOID
NetDestroyLinkMulticastGroups (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine destroys the links remaining multicast groups. It is meant to
    be called during link destruction and does not attempt to update the MAC
    address filters or notify the network. The link should have no references.

Arguments:

    Link - Supplies a pointer to the link whose multicast groups are being
        destroyed.

Return Value:

    None.

--*/

{

    PNET_LINK_MULTICAST_GROUP Group;

    ASSERT(Link->ReferenceCount == 0);
    ASSERT(Link->LinkUp == FALSE);

    while (LIST_EMPTY(&(Link->MulticastGroupList)) == FALSE) {
        Group = LIST_VALUE(Link->MulticastGroupList.Next,
                           NET_LINK_MULTICAST_GROUP,
                           ListEntry);

        //
        // Any groups still remaining should have a join count from 1. These
        // groups were joined when the link was initialized.
        //

        ASSERT(Group->JoinCount == 1);

        LIST_REMOVE(&(Group->ListEntry));
        MmFreePagedPool(Group);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
NetpFindLinkForMulticastRequest (
    PNET_NETWORK_ENTRY Network,
    PNET_SOCKET_MULTICAST_REQUEST Request,
    PNET_LINK_LOCAL_ADDRESS LinkResult
    )

/*++

Routine Description:

    This routine searches for a network link that matches the given multicast
    request. If the any address is supplied, then the multicast address will be
    used to find a link that can reach it. A reference is taken on the returned
    network link. The caller is responsible for releasing the reference.

Arguments:

    Network - Supplies a pointer to the network to which the addresses belong.

    Request - Supplies the multicast request for which a link needs to be
        found.

    LinkResult - Supplies a pointer that receives the link information,
        including the link, link address entry, and associated local address.

Return Value:

    Status code.

--*/

{

    NET_ADDRESS_TYPE AddressType;
    PDEVICE Device;
    PNET_LINK Link;
    KSTATUS Status;

    //
    // The interface ID can be used to find the desired link to use for the
    // multicast request.
    //

    Link = NULL;
    if (Request->InterfaceId != 0) {
        Device = IoGetDeviceByNumericId(Request->InterfaceId);
        if (Device == NULL) {
            Status = STATUS_NO_SUCH_DEVICE;
            goto FindLinkForMulticastRequest;
        }

        Status = NetLookupLinkByDevice(Device, &Link);
        if (!KSUCCESS(Status)) {
            goto FindLinkForMulticastRequest;
        }

    } else if (Network->Interface.GetAddressType != NULL) {
        AddressType = Network->Interface.GetAddressType(
                                                 NULL,
                                                 NULL,
                                                 &(Request->InterfaceAddress));

        //
        // If the any address is supplied for the interface, find a link that
        // can reach the multicast address.
        //

        if (AddressType == NetAddressAny) {
            Status = NetFindLinkForRemoteAddress(&(Request->MulticastAddress),
                                                 LinkResult);

            if (KSUCCESS(Status)) {
                goto FindLinkForMulticastRequest;
            }
        }
    }

    //
    // Otherwise a link result that matches the given address must be found.
    //

    Status = NetFindLinkForLocalAddress(&(Request->InterfaceAddress),
                                        Link,
                                        LinkResult);

    if (!KSUCCESS(Status)) {
        goto FindLinkForMulticastRequest;
    }

FindLinkForMulticastRequest:
    if (Link != NULL) {
        NetLinkReleaseReference(Link);
    }

    return Status;
}

KSTATUS
NetpUpdateMulticastAddressFilters (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine updates the given link's address filtering based on the
    multicast groups to which the link currently belongs. It will gather a list
    of all the physical layer addresses that need to be enabled and pass them
    to the hardware for it to update its filters. It falls back to enabling
    promiscuous mode if the link does not support multicast address filtering.

Arguments:

    Link - Supplies a pointer to the link whose filters are to be updated.

Return Value:

    Status code.

--*/

{

    PVOID Data;
    UINTN DataSize;
    ULONG Enable;
    PNET_DEVICE_LINK_GET_SET_INFORMATION GetSetInformation;
    NET_LINK_INFORMATION_TYPE InformationType;
    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(Link->QueuedLock) != FALSE);

    GetSetInformation = Link->Properties.Interface.GetSetInformation;

    //
    // Before resorting to promiscuous mode, attempt to set the link to
    // receive all multicast packets, if supported.
    //

    if ((Link->Properties.Capabilities &
         NET_LINK_CAPABILITY_MULTICAST_ALL) != 0) {

        InformationType = NetLinkInformationMulticastAll;

    //
    // As a last resort, the link should at least support promiscuous mode
    // to have allowed a multicast join request to make it this far.
    //

    } else {

        ASSERT((Link->Properties.Capabilities &
                NET_LINK_CAPABILITY_PROMISCUOUS_MODE) != 0);

        InformationType = NetLinkInformationPromiscuousMode;
    }

    Enable = FALSE;
    if (LIST_EMPTY(&(Link->MulticastGroupList)) == FALSE) {
        Enable = TRUE;
    }

    DataSize = sizeof(ULONG);
    Data = (PVOID)&Enable;
    Status = GetSetInformation(Link->Properties.DeviceContext,
                               InformationType,
                               Data,
                               &DataSize,
                               TRUE);

    return Status;
}

PNET_SOCKET_MULTICAST_GROUP
NetpFindSocketMulticastGroup (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS MulticastAddress
    )

/*++

Routine Description:

    This routine finds a multicast group in a socket's list of multicast groups.

Arguments:

    Socket - Supplies a pointer to the socket to search.

    Link - Supplies a pointer to the link associated with the multicast group.

    LinkAddress - Supplies a pointer to the link address entry associated with
        the multicast group.

    MulticastAddress - Supplies a pointer to the multicast address of the group.

Return Value:

    Returns a pointer to a socket multicast group on success or NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_SOCKET_MULTICAST_GROUP Group;
    COMPARISON_RESULT Result;

    ASSERT(KeIsQueuedLockHeld(Socket->MulticastLock) != FALSE);

    CurrentEntry = Socket->MulticastGroupList.Next;
    while (CurrentEntry != &(Socket->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, NET_SOCKET_MULTICAST_GROUP, ListEntry);
        if ((Group->Link == Link) && (Group->LinkAddress == LinkAddress)) {
            Result = NetCompareNetworkAddresses(&(Group->MulticastAddress),
                                                MulticastAddress);

            if (Result == ComparisonResultSame) {
                return Group;
            }
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

PNET_SOCKET_MULTICAST_GROUP
NetpCreateSocketMulticastGroup (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS MulticastAddress
    )

/*++

Routine Description:

    This routine creates a socket multicast group.

Arguments:

    Link - Supplies a pointer to the link on which the socket joined the group.

    LinkAddress - Supplies a pointer to the link address on which the socket
        joined the group.

    MulticastAddress - Supplies a pointer to the multicast group address.

Return Value:

    Returns a pointer to the new group on success or NULL on failure.

--*/

{

    PNET_SOCKET_MULTICAST_GROUP NewGroup;

    //
    // Prepare for success and allocate a new socket multicast group.
    //

    NewGroup = MmAllocatePagedPool(sizeof(NET_SOCKET_MULTICAST_GROUP),
                                   NET_CORE_ALLOCATION_TAG);

    if (NewGroup == NULL) {
        goto CreateSocketMulticastGroupEnd;
    }

    NetLinkAddReference(Link);
    NewGroup->Link = Link;
    NewGroup->LinkAddress = LinkAddress;
    RtlCopyMemory(&(NewGroup->MulticastAddress),
                  MulticastAddress,
                  sizeof(NETWORK_ADDRESS));

CreateSocketMulticastGroupEnd:
    return NewGroup;
}

VOID
NetpDestroySocketMulticastGroup (
    PNET_SOCKET_MULTICAST_GROUP Group
    )

/*++

Routine Description:

    This routine destroys the given socket multicast group.

Arguments:

    Group - Supplies a pointer to the group to destroy.

Return Value:

    None.

--*/

{

    NetLinkReleaseReference(Group->Link);
    MmFreePagedPool(Group);
    return;
}

KSTATUS
NetpAcquireSocketMulticastLock (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine acquires the given socket's multicast lock, allocating it on
    the fly if it does not already exist. This is done so most sockets don't
    have to allocate the lock, as most sockets don't perform multicast actions.

Arguments:

    Socket - Supplies a pointer to the network socket whose multicast lock is
        to be acquired.

Return Value:

    Status code.

--*/

{

    PQUEUED_LOCK NewLock;
    PQUEUED_LOCK OldLock;
    KSTATUS Status;

    //
    // If there is no multicast lock. Create one before going any further. This
    // is done on the fly so that most sockets don't need to allocate the lock
    // resource.
    //

    if (Socket->MulticastLock == NULL) {
        NewLock = KeCreateQueuedLock();
        if (NewLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AcquireMulticastLockEnd;
        }

        //
        // Try to exchange the lock into place.
        //

        OldLock = (PQUEUED_LOCK)RtlAtomicCompareExchange(
                                              (PUINTN)&(Socket->MulticastLock),
                                              (UINTN)NewLock,
                                              (UINTN)NULL);

        if (OldLock != NULL) {
            KeDestroyQueuedLock(NewLock);
        }
    }

    ASSERT(Socket->MulticastLock != NULL);

    KeAcquireQueuedLock(Socket->MulticastLock);
    Status = STATUS_SUCCESS;

AcquireMulticastLockEnd:
    return Status;
}

VOID
NetpReleaseSocketMulticastLock (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine releases the multicast lock for the given socket.

Arguments:

    Socket - Supplies a pointer to a network socket.

Return Value:

    None.

--*/

{

    ASSERT(Socket->MulticastLock != NULL);

    KeReleaseQueuedLock(Socket->MulticastLock);
    return;
}

PNET_LINK_MULTICAST_GROUP
NetpFindLinkMulticastGroup (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS MulticastAddress
    )

/*++

Routine Description:

    This routine finds a multicast group in a link's list of multicast groups.

Arguments:

    Link - Supplies a pointer to the link to search.

    LinkAddress - Supplies a pointer to the link address entry associated with
        the group.

    MulticastAddress - Supplies a pointer to the multicast address of the group.

Return Value:

    Returns a pointer to a link multicast group on success or NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_LINK_MULTICAST_GROUP Group;
    COMPARISON_RESULT Result;

    ASSERT(KeIsQueuedLockHeld(Link->QueuedLock) != FALSE);

    CurrentEntry = Link->MulticastGroupList.Next;
    while (CurrentEntry != &(Link->MulticastGroupList)) {
        Group = LIST_VALUE(CurrentEntry, NET_LINK_MULTICAST_GROUP, ListEntry);
        if (Group->LinkAddress == LinkAddress) {
            Result = NetCompareNetworkAddresses(&(Group->Address),
                                                MulticastAddress);

            if (Result == ComparisonResultSame) {
                return Group;
            }
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

