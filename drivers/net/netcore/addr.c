/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    addr.c

Abstract:

    This module implements generic Network layer functionality, largely
    addressing.

Author:

    Evan Green 4-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "netcore.h"
#include "arp.h"
#include "dhcp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines whether or not reuse of the any address is allowed
// between the two sockets.
//

#define CAN_REUSE_ANY_ADDRESS(_NewSocket, _OldSocket)                    \
    ((((_NewSocket)->Flags & NET_SOCKET_FLAG_REUSE_ANY_ADDRESS) != 0) && \
     (((_OldSocket)->Flags & NET_SOCKET_FLAG_REUSE_ANY_ADDRESS) != 0))

//
// This macro determines whether or not reuse of the exact address is allowed
// between the two sockets.
//

#define CAN_REUSE_EXACT_ADDRESS(_NewSocket, _OldSocket)                    \
    ((((_NewSocket)->Flags & NET_SOCKET_FLAG_REUSE_EXACT_ADDRESS) != 0) && \
     (((_OldSocket)->Flags & NET_SOCKET_FLAG_REUSE_EXACT_ADDRESS) != 0))

//
// This macro determines whether or not reuse of the exact address in the time
// wait state is allowed between the two sockets.
//

#define CAN_REUSE_TIME_WAIT(_NewSocket, _OldSocket)                    \
    ((((_OldSocket)->Flags & NET_SOCKET_FLAG_TIME_WAIT) != 0) &&       \
     (((_NewSocket)->Flags & NET_SOCKET_FLAG_REUSE_TIME_WAIT) != 0) && \
     (((_OldSocket)->Flags & NET_SOCKET_FLAG_REUSE_TIME_WAIT) != 0))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait for an address translation to come back,
// in milliseconds.
//

#define ADDRESS_TRANSLATION_TIMEOUT (5 * MILLISECONDS_PER_SECOND)
#define ADDRESS_TRANSLATION_RETRY_INTERVAL MILLISECONDS_PER_SECOND

//
// Define the ephemeral port range.
//

#define NET_EPHEMERAL_PORT_START 49152
#define NET_EPHEMERAL_PORT_END   65536
#define NET_EPHEMERAL_PORT_COUNT \
    (NET_EPHEMERAL_PORT_END - NET_EPHEMERAL_PORT_START)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a translation between a network address and a
    physical one.

Members:

    TreeEntry - Stores the red black tree information for this node.

    NetworkAddress - Stores the network address, the key for the red black
        tree node.

    PhysicalAddress - Stores the physical address that corresponds to the
        network address.

--*/

typedef struct _ADDRESS_TRANSLATION_ENTRY {
    RED_BLACK_TREE_NODE TreeEntry;
    NETWORK_ADDRESS NetworkAddress;
    NETWORK_ADDRESS PhysicalAddress;
} ADDRESS_TRANSLATION_ENTRY, *PADDRESS_TRANSLATION_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
NetpDestroyLink (
    PNET_LINK Link
    );

VOID
NetpDeactivateSocketUnlocked (
    PNET_SOCKET Socket
    );

VOID
NetpDetachSockets (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    );

VOID
NetpDetachSocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpLookupAddressTranslation (
    PNET_LINK Link,
    PNETWORK_ADDRESS NetworkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    );

COMPARISON_RESULT
NetpCompareFullyBoundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

COMPARISON_RESULT
NetpMatchFullyBoundSocket (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS LocalAddress,
    PNETWORK_ADDRESS RemoteAddress
    );

COMPARISON_RESULT
NetpCompareLocallyBoundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

COMPARISON_RESULT
NetpCompareUnboundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

COMPARISON_RESULT
NetpCompareAddressTranslationEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

BOOL
NetpCheckLocalAddressAvailability (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS LocalAddress
    );

VOID
NetpGetPacketSizeInformation (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_SIZE_INFORMATION SizeInformation
    );

VOID
NetpDebugPrintNetworkAddress (
    PNET_NETWORK_ENTRY Network,
    PNETWORK_ADDRESS Address
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the list of available network links (things that can actually send
// packets). Any party accessing this list must have acquired the link list
// lock. The lock can only be acquired at low level.
//

LIST_ENTRY NetLinkList;
PSHARED_EXCLUSIVE_LOCK NetLinkListLock;

UUID NetNetworkDeviceInformationUuid = NETWORK_DEVICE_INFORMATION_UUID;

//
// ------------------------------------------------------------------ Functions
//

NET_API
KSTATUS
NetAddLink (
    PNET_LINK_PROPERTIES Properties,
    PNET_LINK *NewLink
    )

/*++

Routine Description:

    This routine adds a new network link based on the given properties. The
    link must be ready to send and receive traffic and have a valid physical
    layer address supplied in the properties.

Arguments:

    Properties - Supplies a pointer describing the properties and interface of
        the link. This memory will not be referenced after the function returns,
        so this may be a stack allocated structure.

    NewLink - Supplies a pointer where a pointer to the new link will be
        returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated for the
        structure.

--*/

{

    PNET_DATA_LINK_ENTRY CurrentDataLink;
    PLIST_ENTRY CurrentEntry;
    PNET_NETWORK_ENTRY CurrentNetwork;
    PNET_DATA_LINK_ENTRY FoundDataLink;
    PLIST_ENTRY LastEntry;
    PNET_LINK Link;
    BOOL LockHeld;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LastEntry = NULL;
    Link = NULL;
    LockHeld = FALSE;
    Status = STATUS_SUCCESS;
    if (Properties->Version < NET_LINK_PROPERTIES_VERSION) {
        Status = STATUS_VERSION_MISMATCH;
        goto AddLinkEnd;
    }

    if (Properties->TransmitAlignment == 0) {
        Properties->TransmitAlignment = 1;
    }

    if ((!POWER_OF_2(Properties->TransmitAlignment)) ||
        (Properties->PhysicalAddress.Domain == NetDomainInvalid) ||
        (Properties->MaxPhysicalAddress == 0) ||
        (Properties->Interface.Send == NULL) ||
        (Properties->Interface.GetSetInformation == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto AddLinkEnd;
    }

    Link = MmAllocatePagedPool(sizeof(NET_LINK), NET_CORE_ALLOCATION_TAG);
    if (Link == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddLinkEnd;
    }

    RtlZeroMemory(Link, sizeof(NET_LINK));
    Link->ReferenceCount = 1;
    RtlCopyMemory(&(Link->Properties), Properties, sizeof(NET_LINK_PROPERTIES));
    Link->QueuedLock = KeCreateQueuedLock();
    if (Link->QueuedLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddLinkEnd;
    }

    INITIALIZE_LIST_HEAD(&(Link->LinkAddressList));
    Link->AddressTranslationEvent = KeCreateEvent(NULL);
    if (Link->AddressTranslationEvent == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddLinkEnd;
    }

    KeSignalEvent(Link->AddressTranslationEvent, SignalOptionUnsignal);
    RtlRedBlackTreeInitialize(&(Link->AddressTranslationTree),
                              0,
                              NetpCompareAddressTranslationEntries);

    //
    // Find the appropriate data link layer and initialize it for this link.
    //

    FoundDataLink = NULL;
    KeAcquireSharedExclusiveLockShared(NetPluginListLock);
    LockHeld = TRUE;
    CurrentEntry = NetDataLinkList.Next;
    while (CurrentEntry != &NetDataLinkList) {
        CurrentDataLink = LIST_VALUE(CurrentEntry,
                                     NET_DATA_LINK_ENTRY,
                                     ListEntry);

        if (CurrentDataLink->Domain == Properties->DataLinkType) {
            FoundDataLink = CurrentDataLink;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (FoundDataLink == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto AddLinkEnd;
    }

    Status = FoundDataLink->Interface.InitializeLink(Link);
    if (!KSUCCESS(Status)) {
        goto AddLinkEnd;
    }

    Link->DataLinkEntry = FoundDataLink;

    //
    // Let the network layers have their shot at initializing state for this
    // link.
    //

    CurrentEntry = NetNetworkList.Next;
    while (CurrentEntry != &NetNetworkList) {
        CurrentNetwork = LIST_VALUE(CurrentEntry, NET_NETWORK_ENTRY, ListEntry);
        Status = CurrentNetwork->Interface.InitializeLink(Link);
        if (!KSUCCESS(Status)) {
            goto AddLinkEnd;
        }

        CurrentEntry = CurrentEntry->Next;
        LastEntry = CurrentEntry;
    }

    KeReleaseSharedExclusiveLockShared(NetPluginListLock);
    LockHeld = FALSE;

    //
    // All network devices respond to the network device information requests.
    //

    Status = IoRegisterDeviceInformation(Link->Properties.Device,
                                         &NetNetworkDeviceInformationUuid,
                                         TRUE);

    if (!KSUCCESS(Status)) {
        goto AddLinkEnd;
    }

    //
    // With success a sure thing, take a reference on the OS device that
    // registered the link with netcore. Its device context and driver need to
    // remain available as long as netcore can access the device link interface.
    //

    IoDeviceAddReference(Link->Properties.Device);

    //
    // Add the link to the global list. It is all ready to send and receive
    // data.
    //

    KeAcquireSharedExclusiveLockExclusive(NetLinkListLock);

    ASSERT(Link->ListEntry.Next == NULL);

    INSERT_BEFORE(&(Link->ListEntry), &NetLinkList);
    KeReleaseSharedExclusiveLockExclusive(NetLinkListLock);
    Status = STATUS_SUCCESS;

AddLinkEnd:
    if (!KSUCCESS(Status)) {
        if (Link != NULL) {
            IoRegisterDeviceInformation(Link->Properties.Device,
                                        &NetNetworkDeviceInformationUuid,
                                        FALSE);

            //
            // If some network layer entries have initialized already, call
            // them back to cancel.
            //

            if (LastEntry != NULL) {
                if (LockHeld == FALSE) {
                    KeAcquireSharedExclusiveLockShared(NetPluginListLock);
                    LockHeld = TRUE;
                }

                CurrentEntry = NetNetworkList.Next;
                while (CurrentEntry != LastEntry) {
                    CurrentNetwork = LIST_VALUE(CurrentEntry,
                                                NET_NETWORK_ENTRY,
                                                ListEntry);

                    CurrentNetwork->Interface.DestroyLink(Link);
                    CurrentEntry = CurrentEntry->Next;
                }

            }

            if (LockHeld != FALSE) {
                KeReleaseSharedExclusiveLockShared(NetPluginListLock);
                LockHeld = FALSE;
            }

            if (Link->DataLinkEntry != NULL) {
                Link->DataLinkEntry->Interface.DestroyLink(Link);
            }

            if (Link->QueuedLock != NULL) {
                KeDestroyQueuedLock(Link->QueuedLock);
            }

            if (Link->AddressTranslationEvent != NULL) {
                KeDestroyEvent(Link->AddressTranslationEvent);
            }

            MmFreePagedPool(Link);
            Link = NULL;
        }
    }

    ASSERT(LockHeld == FALSE);

    *NewLink = Link;
    return Status;
}

NET_API
VOID
NetLinkAddReference (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine increases the reference count on a network link.

Arguments:

    Link - Supplies a pointer to the network link whose reference count
        should be incremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Link->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) & (OldReferenceCount < 0x20000000));

    return;
}

NET_API
VOID
NetLinkReleaseReference (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine decreases the reference count of a network link, and destroys
    the link if the reference count drops to zero.

Arguments:

    Link - Supplies a pointer to the network link whose reference count
        should be decremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Link->ReferenceCount), -1);

    ASSERT(OldReferenceCount != 0);

    if (OldReferenceCount == 1) {
        NetpDestroyLink(Link);
    }

    return;
}

NET_API
VOID
NetSetLinkState (
    PNET_LINK Link,
    BOOL LinkUp,
    ULONGLONG LinkSpeed
    )

/*++

Routine Description:

    This routine sets the link state of the given link. The physical device
    layer is responsible for synchronizing link state changes.

Arguments:

    Link - Supplies a pointer to the link whose state is changing.

    LinkUp - Supplies a boolean indicating whether the link is active (TRUE) or
        disconnected (FALSE).

    LinkSpeed - Supplies the speed of the link, in bits per second.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    NET_DOMAIN_TYPE Domain;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    BOOL OriginalLinkUp;
    KSTATUS Status;
    PADDRESS_TRANSLATION_ENTRY Translation;
    PRED_BLACK_TREE Tree;
    PRED_BLACK_TREE_NODE TreeNode;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(Link != NULL);

    //
    // Link state is synchronized under the global link list lock.
    //

    KeAcquireSharedExclusiveLockExclusive(NetLinkListLock);
    OriginalLinkUp = Link->LinkUp;
    Link->LinkUp = LinkUp;
    Link->LinkSpeed = LinkSpeed;
    RtlDebugPrint("NET: ");
    NetDebugPrintAddress(&(Link->Properties.PhysicalAddress));
    if (LinkUp != FALSE) {
        RtlDebugPrint(" up, Speed %I64d mbps\n", LinkSpeed / 1000000ULL);

    } else {
        RtlDebugPrint(" down\n");
    }

    KeReleaseSharedExclusiveLockExclusive(NetLinkListLock);

    //
    // If the link state was not changed, then take no action.
    //

    if (LinkUp == OriginalLinkUp) {
        return;
    }

    //
    // If the link is now up, then use DHCP to get an address. It is assumed
    // that the link will not go down before handing off to DHCP.
    //

    if (LinkUp != FALSE) {

        ASSERT(Link->LinkUp != FALSE);
        ASSERT(LIST_EMPTY(&(Link->LinkAddressList)) == FALSE);

        //
        // If the link had previously gone down then the address translation
        // event was left signalled.
        //

        KeSignalEvent(Link->AddressTranslationEvent, SignalOptionUnsignal);

        //
        // Request an address for the first link.
        //

        LinkAddress = LIST_VALUE(Link->LinkAddressList.Next,
                                 NET_LINK_ADDRESS_ENTRY,
                                 ListEntry);

        Status = NetpDhcpBeginAssignment(Link, LinkAddress);
        if (!KSUCCESS(Status)) {

            //
            // TODO: Handle failed DHCP.
            //

            ASSERT(FALSE);

        }

    //
    // The link has gone down. Sockets can no longer take references on the
    // link via bind until it goes back up. It is assumed that the link will
    // not go back up while in the middle of this process to take it down.
    //

    } else {

        ASSERT(Link->LinkUp == FALSE);

        //
        // Clean up the address translation tree. If the link reconnects after
        // moving to a new network, some of the address translations may be
        // incorrect.
        //

        Tree = &(Link->AddressTranslationTree);
        KeAcquireQueuedLock(Link->QueuedLock);
        while (TRUE) {
            TreeNode = RtlRedBlackTreeGetLowestNode(Tree);
            if (TreeNode == NULL) {
                break;
            }

            RtlRedBlackTreeRemove(Tree, TreeNode);
            Translation = RED_BLACK_TREE_VALUE(TreeNode,
                                               ADDRESS_TRANSLATION_ENTRY,
                                               TreeEntry);

            MmFreePagedPool(Translation);
        }

        KeReleaseQueuedLock(Link->QueuedLock);

        //
        // Now that the address translation tree is empty, signal anyone
        // waiting for address translations on this event once and for all.
        //

        KeSignalEvent(Link->AddressTranslationEvent, SignalOptionSignalAll);

        //
        // Notify every fully bound, locally bound, and raw socket using this
        // link that the link has gone down. Sockets may be waiting on data or
        // in the middle of sending data.
        //

        NetpDetachSockets(Link, NULL);

        //
        // Now that the sockets are out of the way, go through and gut the
        // non-static link address entries.
        //

        KeAcquireQueuedLock(Link->QueuedLock);
        CurrentEntry = Link->LinkAddressList.Next;
        while (CurrentEntry != &(Link->LinkAddressList)) {
            LinkAddress = LIST_VALUE(CurrentEntry,
                                     NET_LINK_ADDRESS_ENTRY,
                                     ListEntry);

            CurrentEntry = CurrentEntry->Next;
            if (LinkAddress->Configured == FALSE) {
                continue;
            }

            //
            // If the link address was configured via DHCP, then release the IP
            // address.
            //

            if (LinkAddress->StaticAddress == FALSE) {

                //
                // Zero out the network address, except the network type which
                // is needed to reconfigure the link. The rest of the state can
                // be left stale.
                //

                Domain = LinkAddress->Address.Domain;
                RtlZeroMemory(&(LinkAddress->Address), sizeof(NETWORK_ADDRESS));
                LinkAddress->Address.Domain = Domain;

                //
                // Notify DHCP that the link and link address combination is
                // now invalid. It may have saved state.
                //

                NetpDhcpCancelLease(Link, LinkAddress);
            }

            LinkAddress->Configured = FALSE;
        }

        KeReleaseQueuedLock(Link->QueuedLock);
    }

    return;
}

NET_API
VOID
NetGetLinkState (
    PNET_LINK Link,
    PBOOL LinkUp,
    PULONGLONG LinkSpeed
    )

/*++

Routine Description:

    This routine gets the link state of the given link.

Arguments:

    Link - Supplies a pointer to the link whose state is being retrieved.

    LinkUp - Supplies a pointer that receives a boolean indicating whether the
        link is active (TRUE) or disconnected (FALSE). This parameter is
        optional.

    LinkSpeed - Supplies a pointer that receives the speed of the link, in bits
        per second. This parameter is optional.

Return Value:

    None.

--*/

{

    ASSERT(Link != NULL);

    if (LinkUp != NULL) {
        *LinkUp = Link->LinkUp;
    }

    if (LinkSpeed != NULL) {
        *LinkSpeed = Link->LinkSpeed;
    }

    return;
}

NET_API
KSTATUS
NetGetSetLinkDeviceInformation (
    PNET_LINK Link,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets device information for a link.

Arguments:

    Link - Supplies a pointer to the link whose device information is being
        retrieved or set.

    Uuid - Supplies a pointer to the information identifier.

    Data - Supplies a pointer to the data buffer.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer in bytes. On output, returns the needed size of the data buffer,
        even if the supplied buffer was nonexistant or too small.

    Set - Supplies a boolean indicating whether to get the information (FALSE)
        or set the information (TRUE).

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the supplied buffer was too small.

    STATUS_NOT_HANDLED if the given UUID was not recognized.

--*/

{

    KSTATUS Status;

    Status = STATUS_NOT_HANDLED;
    if (RtlAreUuidsEqual(Uuid, &NetNetworkDeviceInformationUuid) != FALSE) {
        if (*DataSize < sizeof(NETWORK_DEVICE_INFORMATION)) {
            *DataSize = sizeof(NETWORK_DEVICE_INFORMATION);
            Status = STATUS_BUFFER_TOO_SMALL;
            goto GetSetLinkDeviceInformationEnd;
        }

        *DataSize = sizeof(NETWORK_DEVICE_INFORMATION);
        Status = NetGetSetNetworkDeviceInformation(Link, NULL, Data, Set);
        goto GetSetLinkDeviceInformationEnd;
    }

GetSetLinkDeviceInformationEnd:
    return Status;
}

NET_API
VOID
NetRemoveLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine removes a link from the networking core after its device has
    been removed. This should not be used if the media has simply been removed.
    In that case, setting the link state to 'down' is suffiient. There may
    still be outstanding references on the link, so the networking core will
    call the device back to notify it when the link is destroyed.

Arguments:

    Link - Supplies a pointer to the link to remove.

Return Value:

    None.

--*/

{

    //
    // The device has been removed, the link should no longer respond to
    // information requests.
    //

    IoRegisterDeviceInformation(Link->Properties.Device,
                                &NetNetworkDeviceInformationUuid,
                                FALSE);

    //
    // If the link is still up, then send out the notice that is is actually
    // down.
    //

    if (Link->LinkUp != FALSE) {
        NetSetLinkState(Link, FALSE, 0);
    }

    //
    // Remove the link from the net link list. Disconnecting the link by
    // setting its state to down should have already stopped sockets from
    // taking new references on the link.
    //

    if (Link->ListEntry.Next != NULL) {
        KeAcquireSharedExclusiveLockExclusive(NetLinkListLock);
        LIST_REMOVE(&(Link->ListEntry));
        Link->ListEntry.Next = NULL;
        KeReleaseSharedExclusiveLockExclusive(NetLinkListLock);
    }

    //
    // Dereference the link. The final clean-up will be triggered once the last
    // reference is released.
    //

    NetLinkReleaseReference(Link);
    return;
}

NET_API
KSTATUS
NetFindLinkForLocalAddress (
    PNET_NETWORK_ENTRY Network,
    PNETWORK_ADDRESS LocalAddress,
    PNET_LINK Link,
    PNET_LINK_LOCAL_ADDRESS LinkResult
    )

/*++

Routine Description:

    This routine searches for a link and the associated address entry that
    matches the given local address. If a link is supplied as a hint, then the
    given link must be able to service the given address for this routine to
    succeed.

Arguments:

    Network - Supplies a pointer to the network entry to which the address
        belongs.

    LocalAddress - Supplies a pointer to the local address to test against.

    Link - Supplies an optional pointer to a link that the local address must
        be from.

    LinkResult - Supplies a pointer that receives the found link, link address
        entry, and local address.

Return Value:

    STATUS_SUCCESS if a link was found and bound with the socket.

    STATUS_INVALID_ADDRESS if no link was found to own that address.

    STATUS_NO_NETWORK_CONNECTION if no networks are available.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_LINK CurrentLink;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_INVALID_ADDRESS;
    KeAcquireSharedExclusiveLockShared(NetLinkListLock);
    if (LIST_EMPTY(&NetLinkList)) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto FindLinkForLocalAddress;
    }

    //
    // If there's a specific link being bound to, then just try to find the
    // address entry within that link.
    //

    if (Link != NULL) {
        if (Link->LinkUp == FALSE) {
            Status = STATUS_NO_NETWORK_CONNECTION;
            goto FindLinkForLocalAddress;
        }

        Status = NetFindEntryForAddress(Link,
                                        Network,
                                        LocalAddress,
                                        &LinkAddress);

    //
    // There is no specific link, so scan through them all.
    //

    } else {
        CurrentEntry = NetLinkList.Next;
        while (CurrentEntry != &NetLinkList) {
            CurrentLink = LIST_VALUE(CurrentEntry, NET_LINK, ListEntry);
            CurrentEntry = CurrentEntry->Next;

            //
            // Don't bother if the link is down.
            //

            if (CurrentLink->LinkUp == FALSE) {
                continue;
            }

            Status = NetFindEntryForAddress(CurrentLink,
                                            Network,
                                            LocalAddress,
                                            &LinkAddress);

            if (KSUCCESS(Status)) {
                Link = CurrentLink;
                break;
            }
        }
    }

    //
    // If a link address entry was found, then fill out the link information.
    //

    if (KSUCCESS(Status)) {
        NetLinkAddReference(Link);
        LinkResult->Link = Link;
        LinkResult->LinkAddress = LinkAddress;
        RtlCopyMemory(&(LinkResult->ReceiveAddress),
                      LocalAddress,
                      sizeof(NETWORK_ADDRESS));

        RtlCopyMemory(&(LinkResult->SendAddress),
                      &(LinkAddress->Address),
                      sizeof(NETWORK_ADDRESS));
    }

FindLinkForLocalAddress:
    KeReleaseSharedExclusiveLockShared(NetLinkListLock);
    return Status;
}

NET_API
KSTATUS
NetFindLinkForRemoteAddress (
    PNETWORK_ADDRESS RemoteAddress,
    PNET_LINK_LOCAL_ADDRESS LinkResult
    )

/*++

Routine Description:

    This routine searches for a link and associated address entry that can
    reach the given remote address.

Arguments:

    RemoteAddress - Supplies a pointer to the address to test against.

    LinkResult - Supplies a pointer that receives the link information,
        including the link, link address entry, and associated local address.

Return Value:

    STATUS_SUCCESS if a link was found and bound with the socket.

    STATUS_NO_NETWORK_CONNECTION if no networks are available.

--*/

{

    PNET_LINK CurrentLink;
    PNET_LINK_ADDRESS_ENTRY CurrentLinkAddressEntry;
    PLIST_ENTRY CurrentLinkEntry;
    PNET_LINK_ADDRESS_ENTRY FoundAddress;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireSharedExclusiveLockShared(NetLinkListLock);
    if (LIST_EMPTY(&NetLinkList)) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto FindLinkForDestinationAddressEnd;
    }

    Status = STATUS_NO_NETWORK_CONNECTION;
    FoundAddress = NULL;
    CurrentLinkEntry = NetLinkList.Next;
    while (CurrentLinkEntry != &NetLinkList) {
        CurrentLink = LIST_VALUE(CurrentLinkEntry, NET_LINK, ListEntry);
        CurrentLinkEntry = CurrentLinkEntry->Next;

        //
        // Don't bother if the link is down.
        //

        if (CurrentLink->LinkUp == FALSE) {
            continue;
        }

        //
        // TODO: Properly determine the route for this destination, rather
        // than just connecting through the first working network link and first
        // address inside it. Make sure to not use the routing tables if
        // SOCKET_IO_DONT_ROUTE is set at time of send/receive.
        //

        KeAcquireQueuedLock(CurrentLink->QueuedLock);

        ASSERT(!LIST_EMPTY(&(CurrentLink->LinkAddressList)));

        CurrentLinkAddressEntry = LIST_VALUE(CurrentLink->LinkAddressList.Next,
                                             NET_LINK_ADDRESS_ENTRY,
                                             ListEntry);

        if (CurrentLinkAddressEntry->Configured != FALSE) {
            FoundAddress = CurrentLinkAddressEntry;
            RtlCopyMemory(&(LinkResult->ReceiveAddress),
                          &(FoundAddress->Address),
                          sizeof(NETWORK_ADDRESS));

            RtlCopyMemory(&(LinkResult->SendAddress),
                          &(FoundAddress->Address),
                          sizeof(NETWORK_ADDRESS));

            ASSERT(LinkResult->SendAddress.Port == 0);
        }

        KeReleaseQueuedLock(CurrentLink->QueuedLock);

        //
        // If a suitable link address was not found, continue on to the next
        // link.
        //

        if (FoundAddress == NULL) {
            continue;
        }

        //
        // Fill out the link information. The local address was copied above
        // under the lock in order to prevent a torn read.
        //

        NetLinkAddReference(CurrentLink);
        LinkResult->Link = CurrentLink;
        LinkResult->LinkAddress = FoundAddress;
        Status = STATUS_SUCCESS;
        break;
    }

FindLinkForDestinationAddressEnd:
    KeReleaseSharedExclusiveLockShared(NetLinkListLock);
    return Status;
}

NET_API
KSTATUS
NetLookupLinkByDevice (
    PDEVICE Device,
    PNET_LINK *Link
    )

/*++

Routine Description:

    This routine looks for a link that belongs to the given device. If a link
    is found, a reference will be added. It is the callers responsibility to
    release this reference.

Arguments:

    Device - Supplies a pointer to the device for which the link is being
        searched.

    Link - Supplies a pointer that receives a pointer to the link, if found.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_LINK CurrentLink;
    KSTATUS Status;

    if (LIST_EMPTY(&NetLinkList) != FALSE) {
        return STATUS_NOT_FOUND;
    }

    Status = STATUS_NOT_FOUND;
    KeAcquireSharedExclusiveLockShared(NetLinkListLock);
    CurrentEntry = NetLinkList.Next;
    while (CurrentEntry != &NetLinkList) {
        CurrentLink = LIST_VALUE(CurrentEntry, NET_LINK, ListEntry);
        if (CurrentLink->Properties.Device == Device) {
            NetLinkAddReference(CurrentLink);
            *Link = CurrentLink;
            Status = STATUS_SUCCESS;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSharedExclusiveLockShared(NetLinkListLock);
    return Status;
}

NET_API
KSTATUS
NetCreateLinkAddressEntry (
    PNET_LINK Link,
    PNETWORK_ADDRESS Address,
    PNETWORK_ADDRESS Subnet,
    PNETWORK_ADDRESS DefaultGateway,
    BOOL StaticAddress,
    PNET_LINK_ADDRESS_ENTRY *NewLinkAddress
    )

/*++

Routine Description:

    This routine initializes a new network link address entry.

Arguments:

    Link - Supplies a pointer to the physical link that has the new network
        address.

    Address - Supplies an optional pointer to the address to assign to the link
        address entry.

    Subnet - Supplies an optional pointer to the subnet mask to assign to the
        link address entry.

    DefaultGateway - Supplies an optional pointer to the default gateway
        address to assign to the link address entry.

    StaticAddress - Supplies a boolean indicating if the provided information
        is a static configuration of the link address entry. This parameter is
        only used when the Address, Subnet, and DefaultGateway parameters are
        supplied.

    NewLinkAddress - Supplies a pointer where a pointer to the new link address
        will be returned. The new link address will also be inserted onto the
        link.

Return Value:

    Status code.

--*/

{

    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LinkAddress = MmAllocatePagedPool(sizeof(NET_LINK_ADDRESS_ENTRY),
                                      NET_CORE_ALLOCATION_TAG);

    if (LinkAddress == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateLinkAddressEnd;
    }

    //
    // Copy in the initial addressing parameters if supplied.
    //

    RtlZeroMemory(LinkAddress, sizeof(NET_LINK_ADDRESS_ENTRY));
    if (Address != NULL) {
        RtlCopyMemory(&(LinkAddress->Address),
                      Address,
                      sizeof(NETWORK_ADDRESS));
    }

    if (Subnet != NULL) {
        RtlCopyMemory(&(LinkAddress->Subnet), Subnet, sizeof(NETWORK_ADDRESS));
    }

    if (DefaultGateway != NULL) {
        RtlCopyMemory(&(LinkAddress->DefaultGateway),
                      DefaultGateway,
                      sizeof(NETWORK_ADDRESS));
    }

    //
    // Start the link address off with the built-in physical address.
    //

    RtlCopyMemory(&(LinkAddress->PhysicalAddress),
                  &(Link->Properties.PhysicalAddress),
                  sizeof(NETWORK_ADDRESS));

    //
    // If an address, subnet, and default gateway were supplied, then this link
    // address entry is as good as configured.
    //

    ASSERT(LinkAddress->Configured == FALSE);

    if ((Address != NULL) && (Subnet != NULL) && (DefaultGateway != NULL)) {
        LinkAddress->StaticAddress = StaticAddress;
        LinkAddress->Configured = TRUE;
    }

    //
    // Everything's good to go, add the address to the link's list.
    //

    KeAcquireQueuedLock(Link->QueuedLock);
    INSERT_AFTER(&(LinkAddress->ListEntry), &(Link->LinkAddressList));
    KeReleaseQueuedLock(Link->QueuedLock);
    Status = STATUS_SUCCESS;

CreateLinkAddressEnd:
    if (!KSUCCESS(Status)) {
        if (LinkAddress != NULL) {
            MmFreePagedPool(LinkAddress);
            LinkAddress = NULL;
        }
    }

    *NewLinkAddress = LinkAddress;
    return Status;
}

NET_API
VOID
NetDestroyLinkAddressEntry (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine removes and destroys a link address.

Arguments:

    Link - Supplies a pointer to the physical link that has the network address.

    LinkAddress - Supplies a pointer to the link address to remove and destroy.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(Link->QueuedLock);
    LIST_REMOVE(&(LinkAddress->ListEntry));
    KeReleaseQueuedLock(Link->QueuedLock);
    MmFreePagedPool(LinkAddress);
    return;
}

NET_API
KSTATUS
NetTranslateNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine translates a network level address to a physical address.

Arguments:

    NetworkAddress - Supplies a pointer to the network address to translate.

    Link - Supplies a pointer to the link to use.

    LinkAddress - Supplies a pointer to the link address entry to use for this
        request.

    PhysicalAddress - Supplies a pointer where the corresponding physical
        address for this network address will be returned.

Return Value:

    Status code.

--*/

{

    ULONGLONG EndTime;
    KSTATUS Status;
    ULONGLONG TimeDelta;

    EndTime = 0;

    //
    // Loop trying to get the address, and waiting for an answer.
    //

    while (TRUE) {
        Status = NetpLookupAddressTranslation(Link,
                                              NetworkAddress,
                                              PhysicalAddress);

        if (KSUCCESS(Status)) {
            break;
        }

        //
        // If the lookup failed once, but this is the first time, set an end
        // time to give up.
        //

        if (EndTime == 0) {
            TimeDelta = ADDRESS_TRANSLATION_TIMEOUT *
                        MICROSECONDS_PER_MILLISECOND;

            EndTime = KeGetRecentTimeCounter() +
                      KeConvertMicrosecondsToTimeTicks(TimeDelta);

            Status = NetpArpSendRequest(Link, LinkAddress, NetworkAddress);
            if (!KSUCCESS(Status)) {
                return Status;
            }

        //
        // If this loop has already been around at least once, look for a
        // timeout event.
        //

        } else if (KeGetRecentTimeCounter() >= EndTime) {
            Status = STATUS_TIMEOUT;
            break;
        }

        //
        // Wait for some new address translation to come in.
        //

        Status = KeWaitForEvent(Link->AddressTranslationEvent,
                                FALSE,
                                ADDRESS_TRANSLATION_RETRY_INTERVAL);

        //
        // On timeouts, re-send the ARP request.
        //

        if (Status == STATUS_TIMEOUT) {
            Status = NetpArpSendRequest(Link, LinkAddress, NetworkAddress);
            if (!KSUCCESS(Status)) {
                return Status;
            }
        }

        //
        // On all other failures to wait for the event, break.
        //

        if (!KSUCCESS(Status)) {
            break;
        }
    }

    return Status;
}

NET_API
KSTATUS
NetAddNetworkAddressTranslation (
    PNET_LINK Link,
    PNETWORK_ADDRESS NetworkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine adds a mapping between a network address and its associated
    physical address.

Arguments:

    Link - Supplies a pointer to the link receiving the mapping.

    NetworkAddress - Supplies a pointer to the network address whose physical
        mapping is known.

    PhysicalAddress - Supplies a pointer to the physical address corresponding
        to the network address.

Return Value:

    Status code.

--*/

{

    PADDRESS_TRANSLATION_ENTRY FoundEntry;
    PRED_BLACK_TREE_NODE FoundNode;
    BOOL LockHeld;
    PADDRESS_TRANSLATION_ENTRY NewEntry;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LockHeld = FALSE;

    //
    // Create the new address translation entry.
    //

    NewEntry = MmAllocatePagedPool(sizeof(ADDRESS_TRANSLATION_ENTRY),
                                   NET_CORE_ALLOCATION_TAG);

    if (NewEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddNetworkAddressTranslationEnd;
    }

    RtlCopyMemory(&(NewEntry->NetworkAddress),
                  NetworkAddress,
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(NewEntry->PhysicalAddress),
                  PhysicalAddress,
                  sizeof(NETWORK_ADDRESS));

    Status = STATUS_SUCCESS;
    KeAcquireQueuedLock(Link->QueuedLock);
    LockHeld = TRUE;
    FoundNode = RtlRedBlackTreeSearch(&(Link->AddressTranslationTree),
                                      &(NewEntry->TreeEntry));

    //
    // If a node is found, update it.
    //

    if (FoundNode != NULL) {
        FoundEntry = RED_BLACK_TREE_VALUE(FoundNode,
                                          ADDRESS_TRANSLATION_ENTRY,
                                          TreeEntry);

        RtlCopyMemory(&(FoundEntry->NetworkAddress),
                      NetworkAddress,
                      sizeof(NETWORK_ADDRESS));

        RtlCopyMemory(&(FoundEntry->PhysicalAddress),
                      PhysicalAddress,
                      sizeof(NETWORK_ADDRESS));

    //
    // No pre-existing entry exists for this network address, add the new
    // entry. Null out the local to indicate the entry was added.
    //

    } else {
        RtlRedBlackTreeInsert(&(Link->AddressTranslationTree),
                              &(NewEntry->TreeEntry));

        KeSignalEvent(Link->AddressTranslationEvent, SignalOptionPulse);
        NewEntry = NULL;
    }

AddNetworkAddressTranslationEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Link->QueuedLock);
    }

    if (NewEntry != NULL) {
        MmFreePagedPool(NewEntry);
    }

    return Status;
}

NET_API
KSTATUS
NetFindEntryForAddress (
    PNET_LINK Link,
    PNET_NETWORK_ENTRY Network,
    PNETWORK_ADDRESS Address,
    PNET_LINK_ADDRESS_ENTRY *AddressEntry
    )

/*++

Routine Description:

    This routine searches for a link address entry within the given link
    matching the desired address.

Arguments:

    Link - Supplies the link whose address entries should be searched.

    Network - Supplies an optional pointer to the network entry to which the
        address belongs.

    Address - Supplies the address to search for.

    AddressEntry - Supplies a pointer where the address entry will be returned
        on success.

Return Value:

    STATUS_SUCCESS if a link was found and bound with the socket.

    STATUS_INVALID_ADDRESS if no link was found to own that address.

--*/

{

    NET_ADDRESS_TYPE AddressType;
    COMPARISON_RESULT ComparisonResult;
    PNET_LINK_ADDRESS_ENTRY CurrentAddress;
    PLIST_ENTRY CurrentAddressEntry;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_INVALID_ADDRESS;
    *AddressEntry = NULL;

    //
    // Loop over all the addresses owned by this link.
    //

    KeAcquireQueuedLock(Link->QueuedLock);
    CurrentAddressEntry = Link->LinkAddressList.Next;
    while (CurrentAddressEntry != &(Link->LinkAddressList)) {
        CurrentAddress = LIST_VALUE(CurrentAddressEntry,
                                    NET_LINK_ADDRESS_ENTRY,
                                    ListEntry);

        CurrentAddressEntry = CurrentAddressEntry->Next;

        //
        // If the network is known, classify the address type using this link
        // address entry. It is necessary to classify the address for each link
        // address entry in case it is the subnet broadcast address.
        //

        if ((Network != NULL) &&
            (Network->Interface.GetAddressType != NULL)) {

            AddressType = Network->Interface.GetAddressType(Link,
                                                            CurrentAddress,
                                                            Address);

            //
            // If the address type is unknown, then it definitely cannot be
            // satisfied by this link address entry.
            //

            if (AddressType == NetAddressUnknown) {
                continue;
            }

        //
        // Otherwise, assume it is a unicast address, meaning it must exactly
        // match the link address entry's local address.
        //

        } else {
            AddressType = NetAddressUnicast;
        }

        //
        // Only a search for an any address can match a non-configured link
        // address entry.
        //

        if ((CurrentAddress->Configured == FALSE) &&
            (AddressType != NetAddressAny)) {

            continue;
        }

        //
        // The domain and port must always match.
        //

        if ((CurrentAddress->Address.Domain != Address->Domain) ||
            (CurrentAddress->Address.Port != Address->Port)) {

            continue;
        }

        //
        // The any, broadcast and multicast addresses only need the domain and
        // port to match.
        //

        if ((AddressType == NetAddressAny) ||
            (AddressType == NetAddressBroadcast) ||
            (AddressType == NetAddressMulticast)) {

            *AddressEntry = CurrentAddress;
            Status = STATUS_SUCCESS;
            break;
        }

        ASSERT(AddressType == NetAddressUnicast);

        //
        // A unicast address must match the link address entry's local address.
        //

        ComparisonResult = NetpCompareNetworkAddresses(
                                                    &(CurrentAddress->Address),
                                                    Address);

        if (ComparisonResult == ComparisonResultSame) {
            *AddressEntry = CurrentAddress;
            Status = STATUS_SUCCESS;
            break;
        }
    }

    KeReleaseQueuedLock(Link->QueuedLock);
    return Status;
}

NET_API
KSTATUS
NetActivateSocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine activates a socket, making it eligible to receive data.

Arguments:

    Socket - Supplies a pointer to the initialized socket to activate.

Return Value:

    Status code.

--*/

{

    if (Socket->BindingType == SocketBindingInvalid) {
        return STATUS_NOT_CONFIGURED;
    }

    //
    // Activate the socket and move on.
    //

    RtlAtomicOr32(&(Socket->Flags), NET_SOCKET_FLAG_ACTIVE);
    return STATUS_SUCCESS;
}

NET_API
VOID
NetDeactivateSocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine removes a socket from the socket tree it's on, removing it
    from eligibility to receive packets. If the socket is removed from the
    tree then a reference will be released.

Arguments:

    Socket - Supplies a pointer to the initialized socket to remove from the
        socket tree.

Return Value:

    None.

--*/

{

    if (((Socket->Flags & NET_SOCKET_FLAG_ACTIVE) == 0) &&
        (Socket->BindingType == SocketBindingInvalid)) {

        return;
    }

    KeAcquireSharedExclusiveLockExclusive(Socket->Protocol->SocketLock);
    NetpDeactivateSocketUnlocked(Socket);
    KeReleaseSharedExclusiveLockExclusive(Socket->Protocol->SocketLock);
    return;
}

NET_API
KSTATUS
NetBindSocket (
    PNET_SOCKET Socket,
    NET_SOCKET_BINDING_TYPE BindingType,
    PNET_LINK_LOCAL_ADDRESS LocalInformation,
    PNETWORK_ADDRESS RemoteAddress,
    ULONG Flags
    )

/*++

Routine Description:

    This routine officially binds a socket to a local address, local port,
    remote address and remote port tuple by adding it to the appropriate socket
    tree. It can also re-bind a socket in the case where it has already been
    bound to a different tree.

Arguments:

    Socket - Supplies a pointer to the initialized socket to bind.

    BindingType - Supplies the type of binding for the socket.

    LocalInformation - Supplies an optional pointer to the information for the
        local link or address to which the socket shall be bound. Use this for
        unbound sockets, leaving the link and link address NULL.

    RemoteAddress - Supplies an optional pointer to a remote address to use
        when fully binding the socket.

    Flags - Supplies a bitmask of binding flags. See NET_SOCKET_BINDING_FLAG_*
        for definitions.

Return Value:

    Status code.

--*/

{

    NET_ADDRESS_TYPE AddressType;
    ULONG AttemptIndex;
    BOOL Available;
    ULONG CurrentPort;
    PRED_BLACK_TREE_NODE ExistingNode;
    PNET_SOCKET ExistingSocket;
    PNET_LINK Link;
    NET_LINK_LOCAL_ADDRESS LocalInformationBuffer;
    BOOL LockHeld;
    PNET_NETWORK_ENTRY Network;
    ULONG OldFlags;
    ULONG OriginalPort;
    PNET_PROTOCOL_ENTRY Protocol;
    PNETWORK_ADDRESS ReceiveAddress;
    BOOL Reinsert;
    NET_SOCKET SearchSocket;
    PNETWORK_ADDRESS SendAddress;
    BOOL SkipLocalValidation;
    BOOL SkipRemoteValidation;
    KSTATUS Status;
    PRED_BLACK_TREE Tree;
    PNETWORK_ADDRESS ValidateAddress;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((LocalInformation != NULL) || (RemoteAddress != NULL));
    ASSERT((BindingType == SocketFullyBound) || (LocalInformation != NULL));
    ASSERT((BindingType != SocketFullyBound) || (RemoteAddress != NULL));

    LockHeld = FALSE;
    Protocol = Socket->Protocol;
    Network = Socket->Network;
    Reinsert = FALSE;

    //
    // If the socket is to be fully bound, then a remote address must have been
    // supplied. Make sure local information is present as well via an implicit
    // local binding.
    //

    if ((BindingType == SocketFullyBound) && (LocalInformation == NULL)) {
        OriginalPort = RemoteAddress->Port;
        RemoteAddress->Port = 0;
        Status = NetFindLinkForRemoteAddress(RemoteAddress,
                                             &LocalInformationBuffer);

        RemoteAddress->Port = OriginalPort;
        if (!KSUCCESS(Status)) {
            goto BindSocketEnd;
        }

        LocalInformation = &LocalInformationBuffer;
    }

    //
    // If the socket belongs to a connection based protocol, don't allow it to
    // be bound or connected to a multicast or broadcast address.
    //

    if ((Protocol->Flags & NET_PROTOCOL_FLAG_CONNECTION_BASED) != 0) {
        ValidateAddress = NULL;
        if (BindingType == SocketFullyBound) {
            ValidateAddress = RemoteAddress;
            Status = STATUS_DESTINATION_UNREACHABLE;

        } else if (BindingType == SocketLocallyBound) {
            ValidateAddress = &(LocalInformation->ReceiveAddress);
            Status = STATUS_INVALID_ADDRESS;
        }

        if (ValidateAddress != NULL) {
            AddressType = Network->Interface.GetAddressType(
                                                 LocalInformation->Link,
                                                 LocalInformation->LinkAddress,
                                                 ValidateAddress);

            if ((AddressType == NetAddressMulticast) ||
                (AddressType == NetAddressBroadcast)) {

                goto BindSocketEnd;
            }
        }
    }

    KeAcquireSharedExclusiveLockExclusive(Protocol->SocketLock);
    LockHeld = TRUE;

    //
    // By default, a socket is not allowed to become less bound (dubbed the act
    // of "unbinding").
    //

    if (((Flags & NET_SOCKET_BINDING_FLAG_ALLOW_UNBIND) == 0) &&
        (Socket->BindingType != SocketBindingInvalid) &&
        (Socket->BindingType > BindingType)) {

        Status = STATUS_INVALID_PARAMETER;
        goto BindSocketEnd;
    }

    //
    // By default, a socket is not allowed to rebind unless it is to the fully
    // bound state.
    //

    if (((Flags & NET_SOCKET_BINDING_FLAG_ALLOW_REBIND) == 0) &&
        (Socket->BindingType != SocketFullyBound) &&
        (Socket->BindingType == BindingType)) {

        Status = STATUS_INVALID_PARAMETER;
        goto BindSocketEnd;
    }

    //
    // Either the existing local port or the future local port better be zero
    // if they do not match.
    //

    if ((Socket->LocalReceiveAddress.Port !=
         LocalInformation->ReceiveAddress.Port) &&
        (Socket->LocalReceiveAddress.Port != 0) &&
        (LocalInformation->ReceiveAddress.Port != 0)) {

        Status = STATUS_INVALID_PARAMETER;
        goto BindSocketEnd;
    }

    //
    // If the socket is locally bound and destined to be fully bound, then the
    // link and link address entry better match. The supplied link was chosen
    // specifically as a link that can reach the remote address.
    //

    if ((Socket->Link != NULL) &&
        (BindingType == SocketFullyBound) &&
        ((Socket->Link != LocalInformation->Link) ||
         (Socket->LinkAddress != LocalInformation->LinkAddress))) {

        Status = STATUS_INVALID_PARAMETER;
        goto BindSocketEnd;
    }

    //
    // Determine the local address and link. Use the ones in the socket if
    // available and not meant to be overwritten. They should be set for
    // sockets that are fully or locally bound, with exception for sockets
    // locally bound to a global broadcast address.
    //

    if ((Flags & NET_SOCKET_BINDING_FLAG_OVERWRITE_LOCAL) != 0) {
        Link = NULL;

    } else {
        Link = Socket->Link;
        ReceiveAddress = &(Socket->LocalReceiveAddress);
        SendAddress = &(Socket->LocalSendAddress);
    }

    if (Link == NULL) {

        ASSERT(LocalInformation != NULL);

        Link = LocalInformation->Link;
        ReceiveAddress = &(LocalInformation->ReceiveAddress);
        SendAddress = &(LocalInformation->SendAddress);

        //
        // If the socket was previously bound, use the local port that was
        // already assigned.
        //

        if (Socket->BindingType != SocketBindingInvalid) {
            ReceiveAddress->Port = Socket->LocalReceiveAddress.Port;
            SendAddress->Port = Socket->LocalSendAddress.Port;
        }
    }

    //
    // Debug print the socket binding.
    //

    if (NetGlobalDebug != FALSE) {
        switch (BindingType) {
        case SocketUnbound:
            RtlDebugPrint("Net: Binding unbound socket %x.\n", Socket);
            break;

        case SocketLocallyBound:
            RtlDebugPrint("Net: Binding locally bound socket %x: ", Socket);
            NetpDebugPrintNetworkAddress(Socket->Network, ReceiveAddress);
            RtlDebugPrint("\n");
            break;

        case SocketFullyBound:
            RtlDebugPrint("Net: Binding fully bound socket %x, Local ", Socket);
            NetpDebugPrintNetworkAddress(Socket->Network, ReceiveAddress);
            RtlDebugPrint(", Remote ");
            NetpDebugPrintNetworkAddress(Socket->Network, RemoteAddress);
            RtlDebugPrint(".\n");
            break;

        default:

            ASSERT(FALSE);

            break;
        }
    }

    //
    // If the socket is bound to a link and the link is down, then do not
    // insert the socket.
    //
    // N.B. Because taking a link down requires iterating over the socket
    //      trees, this does not require any additional synchronization. The
    //      link state is updated and then it waits on the socket lock. So,
    //      either changing the link state acquired the socket lock first, in
    //      which case the link state is already set to 'down' and this should
    //      fail. Or this routine acquired the lock first and if it notices the
    //      link is down, great. If it doesn't, then the socket will get put in
    //      the tree and the process of taking the link down will clean it up.
    //      Of course the link could come back up after this check, but that's
    //      OK. It's up to the caller to try again.
    //

    if ((Link != NULL) && (Link->LinkUp == FALSE)) {
        NetpDetachSocket(Socket);
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto BindSocketEnd;
    }

    //
    // If the socket is already in a tree, temporarily remove it. It will
    // either get moved to the new tree or be restored, untouched, on error.
    // Don't release the reference taken when the socket was first put on the
    // tree. Pass it on to the new tree or keep it for the reinsert.
    //

    SkipLocalValidation = FALSE;
    SkipRemoteValidation = FALSE;
    if (Socket->BindingType != SocketBindingInvalid) {
        RtlRedBlackTreeRemove(&(Protocol->SocketTree[Socket->BindingType]),
                              &(Socket->U.TreeEntry));

        SkipLocalValidation = TRUE;
        Reinsert = TRUE;

    //
    // If the socket is the forked copy of some listening socket, skip
    // validation. This socket is allowed to share the same local address and
    // port.
    //

    } else if ((Socket->Flags & NET_SOCKET_FLAG_FORKED_LISTENER) != 0) {

        ASSERT(LocalInformation != NULL);
        ASSERT(BindingType == SocketLocallyBound);
        ASSERT(LocalInformation->ReceiveAddress.Port != 0);

        SkipLocalValidation = TRUE;
    }

    //
    // Skip both the local and remote address validation if requested.
    //

    if (((Flags & NET_SOCKET_BINDING_FLAG_SKIP_ADDRESS_VALIDATION) != 0) ||
        ((Protocol->Flags & NET_PROTOCOL_FLAG_PORTLESS) != 0)) {

        SkipLocalValidation = TRUE;
        SkipRemoteValidation = TRUE;
    }

    //
    // If no local port number is assigned, attempt to assign one from the
    // ephemeral port range. This will result in a unique tuple, even for fully
    // bound sockets. Some networks allow use of port zero, so skip this if
    // indicated by the binding flags.
    //

    if ((ReceiveAddress->Port == 0) &&
        ((Protocol->Flags & NET_PROTOCOL_FLAG_PORTLESS) == 0) &&
        ((Flags & NET_SOCKET_BINDING_FLAG_NO_PORT_ASSIGNMENT) == 0)) {

        ASSERT(SkipLocalValidation == FALSE);

        CurrentPort = HlQueryTimeCounter() % NET_EPHEMERAL_PORT_COUNT;

        //
        // Find an ephemeral port for this connection.
        //

        Status = STATUS_RESOURCE_IN_USE;
        for (AttemptIndex = 0;
             AttemptIndex < NET_EPHEMERAL_PORT_COUNT;
             AttemptIndex += 1) {

            ReceiveAddress->Port = CurrentPort + NET_EPHEMERAL_PORT_START;

            //
            // If the ephemeral port is already being used by a socket, then
            // try again.
            //

            Available = NetpCheckLocalAddressAvailability(Socket,
                                                          ReceiveAddress);

            if (Available != FALSE) {
                if (NetGlobalDebug != FALSE) {
                    RtlDebugPrint("Net: Using ephemeral port %d.\n",
                                  ReceiveAddress->Port);
                }

                Status = STATUS_SUCCESS;
                break;
            }

            CurrentPort += 1;
            if (CurrentPort >= NET_EPHEMERAL_PORT_COUNT) {
                CurrentPort = 0;
            }
        }

        if (!KSUCCESS(Status)) {
            if (NetGlobalDebug != FALSE) {
                RtlDebugPrint("Net: Rejecting binding for socket %x because "
                              "ephemeral ports exhausted.\n",
                              Socket);

                goto BindSocketEnd;
            }
        }

        ASSERT(SendAddress->Port == 0);

        SendAddress->Port = ReceiveAddress->Port;

    //
    // Do checks for the case where the port was already defined. If the socket
    // was previously in the tree, then the local address is OK. Just make sure
    // that if this is to be a fully bound socket, that the 5-tuple is unique.
    // The exception is if the existing fully bound socket is in the time wait
    // state; deactivate such a socket.
    //

    } else {
        if (SkipLocalValidation == FALSE) {
            Available = NetpCheckLocalAddressAvailability(Socket,
                                                          ReceiveAddress);

            if (Available == FALSE) {
                Status = STATUS_ADDRESS_IN_USE;
                goto BindSocketEnd;
            }
        }

        if ((SkipRemoteValidation == FALSE) &&
            (BindingType == SocketFullyBound)) {

            SearchSocket.Protocol = Socket->Protocol;
            RtlCopyMemory(&(SearchSocket.LocalReceiveAddress),
                          ReceiveAddress,
                          sizeof(NETWORK_ADDRESS));

            RtlCopyMemory(&(SearchSocket.RemoteAddress),
                          RemoteAddress,
                          sizeof(NETWORK_ADDRESS));

            Tree = &(Protocol->SocketTree[SocketFullyBound]);
            ExistingNode = RtlRedBlackTreeSearch(Tree,
                                                 &(SearchSocket.U.TreeEntry));

            if (ExistingNode != NULL) {
                ExistingSocket = RED_BLACK_TREE_VALUE(ExistingNode,
                                                      NET_SOCKET,
                                                      U.TreeEntry);

                if ((ExistingSocket->Flags & NET_SOCKET_FLAG_TIME_WAIT) != 0) {
                    NetpDeactivateSocketUnlocked(ExistingSocket);

                } else {
                    if (NetGlobalDebug != FALSE) {
                        RtlDebugPrint("Net: Rejected binding of socket %x "
                                      "because of existing socket %x.\n",
                                      Socket,
                                      ExistingSocket);
                    }

                    Status = STATUS_ADDRESS_IN_USE;
                    goto BindSocketEnd;
                }
            }
        }
    }

    //
    // This socket is good to go to use the remote address.
    //

    if (RemoteAddress != NULL) {

        ASSERT(BindingType == SocketFullyBound);

        RtlCopyMemory(&(Socket->RemoteAddress),
                      RemoteAddress,
                      sizeof(NETWORK_ADDRESS));
    }

    //
    // If the current local information is to be overwritten, then zero it out.
    //

    if (((Flags & NET_SOCKET_BINDING_FLAG_OVERWRITE_LOCAL) != 0) &&
        (Socket->Link != NULL)) {

        NetLinkReleaseReference(Socket->Link);
        Socket->Link = NULL;
        Socket->LinkAddress = NULL;
        RtlCopyMemory(&(Socket->PacketSizeInformation),
                      &(Socket->UnboundPacketSizeInformation),
                      sizeof(NET_PACKET_SIZE_INFORMATION));
    }

    //
    // Set the local information in the socket if it isn't already set.
    //

    if (Socket->Link == NULL) {

        ASSERT(LocalInformation != NULL);

        if (LocalInformation->Link != NULL) {

            ASSERT(LocalInformation->LinkAddress != NULL);

            NetLinkAddReference(LocalInformation->Link);
            Socket->Link = LocalInformation->Link;
            Socket->LinkAddress = LocalInformation->LinkAddress;

            //
            // Now is the time to update the socket's max packet size,
            // header size, and footer size based on the link.
            //

            NetpGetPacketSizeInformation(Socket->Link,
                                         Socket,
                                         &(Socket->PacketSizeInformation));
        }

        //
        // The receive address can only be updated if the socket is less than
        // locally bound or local overwrites are allowed. This is necessary to
        // prevent a socket locally bound to a broadcast or multicast address
        // from having that broadcast/multicast address being overwritten when
        // it connects to a remote address. The send address, however, should
        // be updated, as that is specific to the link that can reach the
        // remote address.
        //

        if ((Socket->BindingType < SocketLocallyBound) ||
            (Socket->BindingType == SocketBindingInvalid) ||
            ((Flags & NET_SOCKET_BINDING_FLAG_OVERWRITE_LOCAL) != 0)) {

            RtlCopyMemory(&(Socket->LocalReceiveAddress),
                          ReceiveAddress,
                          sizeof(NETWORK_ADDRESS));
        }

        RtlCopyMemory(&(Socket->LocalSendAddress),
                      SendAddress,
                      sizeof(NETWORK_ADDRESS));
    }

    //
    // Mark the socket as active if requested. If this is moving to the fully
    // bound state from another state, record whether or not it was previously
    // active.
    //

    if ((Flags & NET_SOCKET_BINDING_FLAG_ACTIVATE) != 0) {
        OldFlags = RtlAtomicOr32(&(Socket->Flags), NET_SOCKET_FLAG_ACTIVE);
        if ((BindingType == SocketFullyBound) &&
            (Socket->BindingType != SocketFullyBound) &&
            ((OldFlags & NET_SOCKET_FLAG_ACTIVE) != 0)) {

            RtlAtomicOr32(&(Socket->Flags), NET_SOCKET_FLAG_PREVIOUSLY_ACTIVE);
        }
    }

    //
    // If the socket wasn't already in a tree, increment the reference count on
    // the socket so that it cannot disappear while being in the tree.
    //

    if (Socket->BindingType == SocketBindingInvalid) {
        IoSocketAddReference(&(Socket->KernelSocket));
    }

    //
    // Welcome this new friend into the bound sockets tree.
    //

    RtlRedBlackTreeInsert(&(Protocol->SocketTree[BindingType]),
                          &(Socket->U.TreeEntry));

    Socket->BindingType = BindingType;
    Status = STATUS_SUCCESS;

BindSocketEnd:
    if (!KSUCCESS(Status)) {
        if (Reinsert != FALSE) {

            ASSERT(Socket->BindingType != SocketBindingInvalid);

            Tree = &(Protocol->SocketTree[Socket->BindingType]);
            RtlRedBlackTreeInsert(Tree, &(Socket->U.TreeEntry));
        }
    }

    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(Protocol->SocketLock);
    }

    if ((LocalInformation == &LocalInformationBuffer) &&
        (LocalInformationBuffer.Link != NULL)) {

        NetLinkReleaseReference(LocalInformationBuffer.Link);
    }

    return Status;
}

NET_API
KSTATUS
NetDisconnectSocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine disconnects a socket from the fully bound state, rolling it
    back to the locally bound state.

Arguments:

    Socket - Supplies a pointer to the socket to disconnect.

Return Value:

    Status code.

--*/

{

    PNET_PROTOCOL_ENTRY Protocol;
    KSTATUS Status;

    //
    // Disconnect only makes sense on fully bound sockets.
    //

    if (Socket->BindingType != SocketFullyBound) {
        return STATUS_INVALID_PARAMETER;
    }

    Protocol = Socket->Protocol;
    KeAcquireSharedExclusiveLockExclusive(Protocol->SocketLock);
    if (Socket->BindingType != SocketFullyBound) {
        Status = STATUS_INVALID_PARAMETER;
        goto DisconnectSocketEnd;
    }

    //
    // The disconnect just wipes out the remote address. The socket may
    // have been implicitly bound on the connect. So be it. It stays
    // locally bound.
    //

    RtlZeroMemory(&(Socket->RemoteAddress), sizeof(NETWORK_ADDRESS));

    //
    // If the socket was previously inactive before becoming fully bound,
    // return it to the inactive state and clear it from the last found
    // cache of one.
    //

    if ((Socket->Flags & NET_SOCKET_FLAG_PREVIOUSLY_ACTIVE) == 0) {
        RtlAtomicAnd32(&(Socket->Flags), ~NET_SOCKET_FLAG_ACTIVE);
        if (Socket == Protocol->LastSocket) {
            Protocol->LastSocket = NULL;
        }
    }

    //
    // Remove the socket from the fully bound tree and put it in the
    // locally bound tree. As the socket remains in the tree, the reference
    // on the link does not need to be updated.
    //

    RtlRedBlackTreeRemove(&(Protocol->SocketTree[SocketFullyBound]),
                          &(Socket->U.TreeEntry));

    RtlRedBlackTreeInsert(&(Protocol->SocketTree[SocketLocallyBound]),
                          &(Socket->U.TreeEntry));

    Socket->BindingType = SocketLocallyBound;

DisconnectSocketEnd:
    KeReleaseSharedExclusiveLockExclusive(Protocol->SocketLock);
    return Status;
}

NET_API
VOID
NetInitializeSocketLinkOverride (
    PNET_SOCKET Socket,
    PNET_LINK_LOCAL_ADDRESS LinkInformation,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride
    )

/*++

Routine Description:

    This routine initializes the given socket link override structure with the
    appropriate mix of socket and link information.

Arguments:

    Socket - Supplies a pointer to a network socket.

    LinkInformation - Supplies a pointer to link local address information.

    LinkOverride - Supplies a pointer to a socket link override structure that
        will be filled in by this routine.

Return Value:

    None.

--*/

{

    //
    // Since the unbound header size, footer size, and max packet size are
    // saved in the socket, there is no need to protect this under a socket
    // lock.
    //

    NetpGetPacketSizeInformation(LinkInformation->Link,
                                 Socket,
                                 &(LinkOverride->PacketSizeInformation));

    RtlCopyMemory(&(LinkOverride->LinkInformation),
                  LinkInformation,
                  sizeof(NET_LINK_LOCAL_ADDRESS));

    NetLinkAddReference(LinkOverride->LinkInformation.Link);
    return;
}

NET_API
KSTATUS
NetFindSocket (
    PNET_RECEIVE_CONTEXT ReceiveContext,
    PNET_SOCKET *Socket
    )

/*++

Routine Description:

    This routine attempts to find a socket on the receiving end of the given
    context based on matching the addresses and protocol. If the socket is
    found and returned, the reference count will be increased on it. It is the
    caller's responsiblity to release that reference. If this routine returns
    that more processing is required, then subsequent calls should pass the
    previously found socket back to the routine and the search will pick up
    where it left off.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context used to find
        the socket. This contains the remote address, local address, protocol,
        and network to match on.

    Socket - Supplies a pointer that receives a pointer to the found socket on
        output. On input, it can optionally contain a pointer to the socket
        from which the search for a new socket should start.

Return Value:

    STATUS_SUCCESS if a socket was found.

    STATUS_MORE_PROCESSING_REQUIRED if a socket was found, but more sockets
    may match the given address tuple.

    Error status code otherwise.

--*/

{

    NET_ADDRESS_TYPE AddressType;
    NET_SOCKET_BINDING_TYPE BindingType;
    BOOL FindAll;
    PRED_BLACK_TREE_NODE FoundNode;
    PNET_SOCKET FoundSocket;
    PNET_SOCKET LastSocket;
    PNETWORK_ADDRESS LocalAddress;
    PNET_NETWORK_ENTRY Network;
    PRED_BLACK_TREE_NODE NextNode;
    PNET_SOCKET NextSocket;
    PRED_BLACK_TREE_NODE PreviousNode;
    PNET_SOCKET PreviousSocket;
    PNET_PROTOCOL_ENTRY Protocol;
    PNETWORK_ADDRESS RemoteAddress;
    COMPARISON_RESULT Result;
    NET_SOCKET SearchEntry;
    KSTATUS Status;
    PRED_BLACK_TREE Tree;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    FoundSocket = NULL;
    LocalAddress = ReceiveContext->Destination;
    RemoteAddress = ReceiveContext->Source;
    Network = ReceiveContext->Network;
    Protocol = ReceiveContext->Protocol;
    PreviousSocket = *Socket;
    *Socket = NULL;

    //
    // Go get all the sockets if the protocol is always supposed to do that.
    //

    FindAll = FALSE;
    if ((Protocol->Flags & NET_PROTOCOL_FLAG_FIND_ALL_SOCKETS) != 0) {
        FindAll = TRUE;

    //
    // If broadcast and multicast addresses are allowed for this protocol
    // (the default), then test to see if the destination address is a
    // broadcast or multicast address. This test is not necessary if a previous
    // socket is supplied; assume that a previous invocation determined that
    // all sockets needed to be found for this address tuple.
    //

    } else if ((Protocol->Flags & NET_PROTOCOL_FLAG_UNICAST_ONLY) == 0) {
        if (PreviousSocket != NULL) {
            FindAll = TRUE;

        } else if (Network->Interface.GetAddressType != NULL) {
            AddressType = Network->Interface.GetAddressType(
                                                          ReceiveContext->Link,
                                                          NULL,
                                                          LocalAddress);

            if ((AddressType == NetAddressBroadcast) ||
                (AddressType == NetAddressMulticast)) {

                FindAll = TRUE;
            }
        }
    }

    //
    // Check to see if the given remote and local addresses match the last
    // fully bound socket found. This speeds up the search process when there
    // isn't a whole lot of activity. This cannot be done if multiple sockets
    // need to be found as it would start the search iteration in the wrong
    // location.
    //

    KeAcquireSharedExclusiveLockShared(Protocol->SocketLock);
    if (FindAll == FALSE) {
        LastSocket = Protocol->LastSocket;
        if (LastSocket != NULL) {

            ASSERT(LastSocket->BindingType == SocketFullyBound);

            Result = NetpMatchFullyBoundSocket(LastSocket,
                                               LocalAddress,
                                               RemoteAddress);

            if (Result == ComparisonResultSame) {
                FoundNode = NULL;
                FoundSocket = LastSocket;
                goto FindSocketEnd;
            }
        }
    }

    //
    // Fill out a fake socket entry for search purposes.
    //

    RtlCopyMemory(&(SearchEntry.LocalReceiveAddress),
                  LocalAddress,
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(SearchEntry.RemoteAddress),
                  RemoteAddress,
                  sizeof(NETWORK_ADDRESS));

    //
    // If only one socket needs to be found check each binding tree looking
    // for a match, starting with the most specified parameters (local and
    // remote address), and working towards the most generic parameters
    // (local port only).
    //

    if (FindAll == FALSE) {
        Tree = &(Protocol->SocketTree[SocketFullyBound]);
        FoundNode = RtlRedBlackTreeSearch(Tree, &(SearchEntry.U.TreeEntry));
        if (FoundNode != NULL) {
            goto FindSocketEnd;
        }

        Tree = &(Protocol->SocketTree[SocketLocallyBound]);
        FoundNode = RtlRedBlackTreeSearch(Tree, &(SearchEntry.U.TreeEntry));
        if (FoundNode != NULL) {
            goto FindSocketEnd;
        }

        Tree = &(Protocol->SocketTree[SocketUnbound]);
        FoundNode = RtlRedBlackTreeSearch(Tree, &(SearchEntry.U.TreeEntry));
        if (FoundNode != NULL) {
            goto FindSocketEnd;
        }

    //
    // Otherwise go about finding the lowest socket in the unbound tree that
    // matches the criteria. Return it. The caller should call again and this
    // will pick up where it left off, iterating through that first tree. When
    // that tree is exhausted of matches, it will move to the next tree. This
    // could greatly benefit from a hash table. RTL is yet to include a hash
    // table library as the problem of how to grow hash tables is yet to be
    // investigated.
    //

    } else {
        BindingType = SocketUnbound;
        if (PreviousSocket != NULL) {
            BindingType = PreviousSocket->BindingType;
        }

        FoundNode = NULL;
        while (BindingType < SocketBindingTypeCount) {
            Tree = &(Protocol->SocketTree[BindingType]);
            BindingType += 1;

            //
            // Pick up where the last search left off if a previous socket was
            // provided.
            //

            if (PreviousSocket != NULL) {
                PreviousNode = &(PreviousSocket->U.TreeEntry);
                while (TRUE) {
                    NextNode = RtlRedBlackTreeGetNextNode(Tree,
                                                          FALSE,
                                                          PreviousNode);

                    if (NextNode == NULL) {
                        break;
                    }

                    NextSocket = RED_BLACK_TREE_VALUE(NextNode,
                                                      NET_SOCKET,
                                                      U.TreeEntry);

                    if ((NextSocket->Flags & NET_SOCKET_FLAG_ACTIVE) == 0) {
                        PreviousNode = NextNode;
                        continue;
                    }

                    break;
                }

                if (NextNode != NULL) {
                    Result = Tree->CompareFunction(Tree,
                                                   NextNode,
                                                   &(SearchEntry.U.TreeEntry));

                    if (Result == ComparisonResultSame) {
                        FoundNode = NextNode;
                        goto FindSocketEnd;
                    }
                }

                //
                // There are no more matching sockets in this tree. Skip to the
                // next tree.
                //

                PreviousSocket = NULL;
                continue;

            //
            // Otherwise find the first matching, active socket in the new tree.
            //

            } else {
                NextNode = RtlRedBlackTreeSearch(Tree,
                                                 &(SearchEntry.U.TreeEntry));

                if (NextNode == NULL) {
                    continue;
                }

                //
                // A match was found. Find the lowest match in the tree. When
                // the loop exits, it will be the previous node touched.
                //

                do {
                    PreviousNode = NextNode;
                    NextNode = RtlRedBlackTreeGetNextNode(Tree,
                                                          TRUE,
                                                          PreviousNode);

                    if (NextNode == NULL) {
                        break;
                    }

                    Result = Tree->CompareFunction(Tree,
                                                   NextNode,
                                                   &(SearchEntry.U.TreeEntry));

                } while (Result == ComparisonResultSame);

                //
                // Now move forward finding the first active socket that
                // matches.
                //

                NextNode = PreviousNode;
                do {
                    NextSocket = RED_BLACK_TREE_VALUE(NextNode,
                                                      NET_SOCKET,
                                                      U.TreeEntry);

                    if ((NextSocket->Flags & NET_SOCKET_FLAG_ACTIVE) != 0) {
                        FoundNode = NextNode;
                        goto FindSocketEnd;
                    }

                    NextNode = RtlRedBlackTreeGetNextNode(Tree,
                                                          FALSE,
                                                          NextNode);

                    if (NextNode == NULL) {
                        break;
                    }

                    Result = Tree->CompareFunction(Tree,
                                                   NextNode,
                                                   &(SearchEntry.U.TreeEntry));

                } while (Result == ComparisonResultSame);

                //
                // If no active sockets were found, move to the next tree.
                //

                continue;
            }
        }
    }

FindSocketEnd:
    if (FoundNode != NULL) {
        FoundSocket = RED_BLACK_TREE_VALUE(FoundNode, NET_SOCKET, U.TreeEntry);
    }

    Status = STATUS_NOT_FOUND;
    if (FoundSocket != NULL) {

        //
        // If the socket is not active, act as if it were never seen. The
        // cached socket should never be found as inactive. Deactivating the
        // cached socket clears the cache.
        //

        if ((FoundSocket->Flags & NET_SOCKET_FLAG_ACTIVE) == 0) {

            ASSERT(FoundSocket != Protocol->LastSocket);

            FoundSocket = NULL;

        //
        // Otherwise, increment the reference count so the socket cannot
        // disappear once the lock is released.
        //

        } else {
            IoSocketAddReference(&(FoundSocket->KernelSocket));
            if (FindAll != FALSE) {
                Status = STATUS_MORE_PROCESSING_REQUIRED;

            } else {
                if (FoundSocket->BindingType == SocketFullyBound) {
                    Protocol->LastSocket = FoundSocket;
                }

                Status = STATUS_SUCCESS;
            }
        }
    }

    KeReleaseSharedExclusiveLockShared(Protocol->SocketLock);
    *Socket = FoundSocket;
    return Status;
}

NET_API
KSTATUS
NetGetSetNetworkDeviceInformation (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry,
    PNETWORK_DEVICE_INFORMATION Information,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the network device information for a particular
    link.

Arguments:

    Link - Supplies a pointer to the link to work with.

    LinkAddressEntry - Supplies an optional pointer to the specific address
        entry to set. If NULL, a link address entry matching the network type
        contained in the information will be found.

    Information - Supplies a pointer that either receives the device
        information, or contains the new information to set. For set operations,
        the information buffer will contain the current settings on return.

    Set - Supplies a boolean indicating if the information should be set or
        returned.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONG DnsServerIndex;
    NET_DOMAIN_TYPE Domain;
    BOOL OriginalConfiguredState;
    BOOL SameAddress;
    BOOL StaticAddress;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (Information->Version < NETWORK_DEVICE_INFORMATION_VERSION) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Currently only IPv4 is supported.
    //

    Domain = Information->Domain;
    if (Domain != NetDomainIp4) {
        return STATUS_INVALID_CONFIGURATION;
    }

    KeAcquireQueuedLock(Link->QueuedLock);

    //
    // If the caller passed in a link address entry, ensure it corresponds to
    // the network type they are working with.
    //

    if (LinkAddressEntry != NULL) {
        if (Information->Domain != LinkAddressEntry->Address.Domain) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto GetSetNetworkDeviceInformationEnd;
        }

    //
    // Find a link address entry for this network type.
    //

    } else {
        CurrentEntry = Link->LinkAddressList.Next;
        while (CurrentEntry != &(Link->LinkAddressList)) {
            LinkAddressEntry = LIST_VALUE(CurrentEntry,
                                          NET_LINK_ADDRESS_ENTRY,
                                          ListEntry);

            if (LinkAddressEntry->Address.Domain == Information->Domain) {
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (CurrentEntry == &(Link->LinkAddressList)) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto GetSetNetworkDeviceInformationEnd;
        }
    }

    if (Set != FALSE) {
        StaticAddress = TRUE;
        SameAddress = FALSE;

        //
        // If the caller is setting up the link, copy the parameters in.
        //

        if ((Information->Flags & NETWORK_DEVICE_FLAG_CONFIGURED) != 0) {
            if ((Information->Address.Domain != Domain) ||
                (Information->Subnet.Domain != Domain) ||
                (Information->Gateway.Domain != Domain) ||
                ((Information->ConfigurationMethod !=
                  NetworkAddressConfigurationStatic) &&
                 (Information->ConfigurationMethod !=
                  NetworkAddressConfigurationDhcp))) {

                Status = STATUS_INVALID_CONFIGURATION;
                goto GetSetNetworkDeviceInformationEnd;
            }

            if (Information->DnsServerCount > NETWORK_DEVICE_MAX_DNS_SERVERS) {
                Information->DnsServerCount = NETWORK_DEVICE_MAX_DNS_SERVERS;
            }

            for (DnsServerIndex = 0;
                 DnsServerIndex < Information->DnsServerCount;
                 DnsServerIndex += 1) {

                if (Information->DnsServers[DnsServerIndex].Domain != Domain) {
                    Status = STATUS_INVALID_CONFIGURATION;
                    goto GetSetNetworkDeviceInformationEnd;
                }
            }

            SameAddress = RtlCompareMemory(&(LinkAddressEntry->Address),
                                           &(Information->Address),
                                           sizeof(NETWORK_ADDRESS));

            if (SameAddress == FALSE) {
                RtlCopyMemory(&(LinkAddressEntry->Address),
                              &(Information->Address),
                              sizeof(NETWORK_ADDRESS));
            }

            LinkAddressEntry->Address.Port = 0;
            RtlCopyMemory(&(LinkAddressEntry->Subnet),
                          &(Information->Subnet),
                          sizeof(NETWORK_ADDRESS));

            LinkAddressEntry->Subnet.Port = 0;
            RtlCopyMemory(&(LinkAddressEntry->DefaultGateway),
                          &(Information->Gateway),
                          sizeof(NETWORK_ADDRESS));

            LinkAddressEntry->DefaultGateway.Port = 0;
            for (DnsServerIndex = 0;
                 DnsServerIndex < Information->DnsServerCount;
                 DnsServerIndex += 1) {

                RtlCopyMemory(&(LinkAddressEntry->DnsServer[DnsServerIndex]),
                              &(Information->DnsServers[DnsServerIndex]),
                              sizeof(NETWORK_ADDRESS));
            }

            LinkAddressEntry->DnsServerCount = Information->DnsServerCount;
            LinkAddressEntry->StaticAddress = TRUE;
            if (Information->ConfigurationMethod ==
                NetworkAddressConfigurationDhcp) {

                LinkAddressEntry->StaticAddress = FALSE;
                RtlCopyMemory(&(LinkAddressEntry->LeaseServerAddress),
                              &(Information->LeaseServerAddress),
                              sizeof(NETWORK_ADDRESS));

                RtlCopyMemory(&(LinkAddressEntry->LeaseStartTime),
                              &(Information->LeaseStartTime),
                              sizeof(SYSTEM_TIME));

                RtlCopyMemory(&(LinkAddressEntry->LeaseEndTime),
                              &(Information->LeaseEndTime),
                              sizeof(SYSTEM_TIME));
            }

            LinkAddressEntry->Configured = TRUE;

        //
        // Unconfigure the link and bring it down.
        //

        } else {

            //
            // If the link address is not static, then zero the address,
            // leaving the network type.
            //

            StaticAddress = TRUE;
            if (LinkAddressEntry->StaticAddress == FALSE) {
                Domain = LinkAddressEntry->Address.Domain;
                RtlZeroMemory(&(LinkAddressEntry->Address),
                              sizeof(NETWORK_ADDRESS));

                LinkAddressEntry->Address.Domain = Domain;
                StaticAddress = FALSE;
            }

            LinkAddressEntry->Configured = FALSE;
        }

        //
        // If the address is changing or going down, invalidate all sockets
        // using the address. Make sure that the link address entry is not
        // marked as configured, otherwise new sockets could show up while the
        // link's queued lock is relesaed.
        //

        if (SameAddress == FALSE) {
            OriginalConfiguredState = LinkAddressEntry->Configured;
            LinkAddressEntry->Configured = FALSE;
            KeReleaseQueuedLock(Link->QueuedLock);

            //
            // Notify DHCP that the link and link address combination is now
            // invalid. It may have saved state.
            //

            if (((Information->Flags & NETWORK_DEVICE_FLAG_CONFIGURED) == 0) &&
                (StaticAddress == FALSE)) {

                NetpDhcpCancelLease(Link, LinkAddressEntry);
            }

            //
            // Notify every fully bound, locally bound, and raw socket using
            // this link and link address pair that the link address is being
            // disabled. Sockets may be waiting on data or in the middle of
            // sending data.
            //

            NetpDetachSockets(Link, LinkAddressEntry);
            KeAcquireQueuedLock(Link->QueuedLock);
            LinkAddressEntry->Configured = OriginalConfiguredState;
        }
    }

    //
    // Now that the information has potentially been set, get the new
    // information.
    //

    Information->Flags = 0;
    RtlCopyMemory(&(Information->PhysicalAddress),
                  &(LinkAddressEntry->PhysicalAddress),
                  sizeof(NETWORK_ADDRESS));

    if (Link->LinkUp != FALSE) {
        Information->Flags |= NETWORK_DEVICE_FLAG_MEDIA_CONNECTED;
    }

    if (LinkAddressEntry->Configured == FALSE) {
        Information->ConfigurationMethod = NetworkAddressConfigurationNone;
        Status = STATUS_SUCCESS;
        goto GetSetNetworkDeviceInformationEnd;
    }

    Information->Flags |= NETWORK_DEVICE_FLAG_CONFIGURED;
    Information->ConfigurationMethod = NetworkAddressConfigurationDhcp;
    if (LinkAddressEntry->StaticAddress != FALSE) {
        Information->ConfigurationMethod = NetworkAddressConfigurationStatic;
    }

    RtlCopyMemory(&(Information->Address),
                  &(LinkAddressEntry->Address),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information->Subnet),
                  &(LinkAddressEntry->Subnet),
                  sizeof(NETWORK_ADDRESS));

    RtlCopyMemory(&(Information->Gateway),
                  &(LinkAddressEntry->DefaultGateway),
                  sizeof(NETWORK_ADDRESS));

    Information->DnsServerCount = LinkAddressEntry->DnsServerCount;
    for (DnsServerIndex = 0;
         DnsServerIndex < Information->DnsServerCount;
         DnsServerIndex += 1) {

        RtlCopyMemory(&(Information->DnsServers[DnsServerIndex]),
                      &(LinkAddressEntry->DnsServer[DnsServerIndex]),
                      sizeof(NETWORK_ADDRESS));
    }

    if (LinkAddressEntry->StaticAddress == FALSE) {
        RtlCopyMemory(&(Information->LeaseServerAddress),
                      &(LinkAddressEntry->LeaseServerAddress),
                      sizeof(NETWORK_ADDRESS));

        RtlCopyMemory(&(Information->LeaseStartTime),
                      &(LinkAddressEntry->LeaseStartTime),
                      sizeof(SYSTEM_TIME));

        RtlCopyMemory(&(Information->LeaseEndTime),
                      &(LinkAddressEntry->LeaseEndTime),
                      sizeof(SYSTEM_TIME));
    }

    Status = STATUS_SUCCESS;

GetSetNetworkDeviceInformationEnd:
    KeReleaseQueuedLock(Link->QueuedLock);
    return Status;
}

NET_API
COMPARISON_RESULT
NetCompareNetworkAddresses (
    PNETWORK_ADDRESS FirstAddress,
    PNETWORK_ADDRESS SecondAddress
    )

/*++

Routine Description:

    This routine compares two network addresses.

Arguments:

    FirstAddress - Supplies a pointer to the left side of the comparison.

    SecondAddress - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    return NetpCompareNetworkAddresses(FirstAddress, SecondAddress);
}

KSTATUS
NetpInitializeNetworkLayer (
    VOID
    )

/*++

Routine Description:

    This routine initialize support for generic Network layer functionality.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    NetLinkListLock = KeCreateSharedExclusiveLock();
    if (NetLinkListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeNetworkLayerEnd;
    }

    INITIALIZE_LIST_HEAD(&NetLinkList);
    Status = STATUS_SUCCESS;

InitializeNetworkLayerEnd:
    if (!KSUCCESS(Status)) {
        if (NetLinkListLock != NULL) {
            KeDestroySharedExclusiveLock(NetLinkListLock);
            NetLinkListLock = NULL;
        }
    }

    return Status;
}

COMPARISON_RESULT
NetpCompareNetworkAddresses (
    PNETWORK_ADDRESS FirstAddress,
    PNETWORK_ADDRESS SecondAddress
    )

/*++

Routine Description:

    This routine compares two network addresses.

Arguments:

    FirstAddress - Supplies a pointer to the left side of the comparison.

    SecondAddress - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    ULONG PartIndex;

    if (FirstAddress == SecondAddress) {
        return ComparisonResultSame;
    }

    //
    // Very likely the ports will disagree, so check those first.
    //

    if (FirstAddress->Port < SecondAddress->Port) {
        return ComparisonResultAscending;

    } else if (FirstAddress->Port > SecondAddress->Port) {
        return ComparisonResultDescending;
    }

    //
    // Compare the networks before the addresses. This is necessary because
    // binding requires a search for addresses of the same protocol and network
    // that use the same port. Sorting by network before address makes this
    // easier.
    //

    if (FirstAddress->Domain < SecondAddress->Domain) {
        return ComparisonResultAscending;

    } else if (FirstAddress->Domain > SecondAddress->Domain) {
        return ComparisonResultDescending;
    }

    //
    // Check the address itself.
    //

    for (PartIndex = 0;
         PartIndex < MAX_NETWORK_ADDRESS_SIZE / sizeof(UINTN);
         PartIndex += 1) {

        if (FirstAddress->Address[PartIndex] <
            SecondAddress->Address[PartIndex]) {

            return ComparisonResultAscending;

        } else if (FirstAddress->Address[PartIndex] >
                   SecondAddress->Address[PartIndex]) {

            return ComparisonResultDescending;
        }

        //
        // The parts here are equal, move on to the next part.
        //

    }

    //
    // Well, nothing's not different, so they must be the same.
    //

    return ComparisonResultSame;
}

COMPARISON_RESULT
NetpCompareFullyBoundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two fully bound sockets, where both the local and
    remote addresses are fixed.

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

    PNET_SOCKET FirstSocket;
    COMPARISON_RESULT Result;
    PNET_SOCKET SecondSocket;

    FirstSocket = RED_BLACK_TREE_VALUE(FirstNode, NET_SOCKET, U.TreeEntry);
    SecondSocket = RED_BLACK_TREE_VALUE(SecondNode, NET_SOCKET, U.TreeEntry);
    Result = NetpMatchFullyBoundSocket(FirstSocket,
                                       &(SecondSocket->LocalReceiveAddress),
                                       &(SecondSocket->RemoteAddress));

    return Result;
}

COMPARISON_RESULT
NetpCompareLocallyBoundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two locally bound sockets, where the local address
    and port are fixed.

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

    PNET_SOCKET FirstSocket;
    COMPARISON_RESULT Result;
    PNET_SOCKET SecondSocket;

    FirstSocket = RED_BLACK_TREE_VALUE(FirstNode, NET_SOCKET, U.TreeEntry);
    SecondSocket = RED_BLACK_TREE_VALUE(SecondNode, NET_SOCKET, U.TreeEntry);
    Result = NetpCompareNetworkAddresses(&(FirstSocket->LocalReceiveAddress),
                                         &(SecondSocket->LocalReceiveAddress));

    return Result;
}

COMPARISON_RESULT
NetpCompareUnboundSockets (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two unbound sockets, meaning only the local port
    number is known.

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

    PNETWORK_ADDRESS FirstLocalAddress;
    PNET_SOCKET FirstSocket;
    PNETWORK_ADDRESS SecondLocalAddress;
    PNET_SOCKET SecondSocket;

    FirstSocket = RED_BLACK_TREE_VALUE(FirstNode, NET_SOCKET, U.TreeEntry);
    SecondSocket = RED_BLACK_TREE_VALUE(SecondNode, NET_SOCKET, U.TreeEntry);

    //
    // Compare the local port numbers.
    //

    FirstLocalAddress = &(FirstSocket->LocalReceiveAddress);
    SecondLocalAddress = &(SecondSocket->LocalReceiveAddress);
    if (FirstLocalAddress->Port < SecondLocalAddress->Port) {
        return ComparisonResultAscending;

    } else if (FirstLocalAddress->Port > SecondLocalAddress->Port) {
        return ComparisonResultDescending;
    }

    //
    // Compare the networks.
    //

    if (FirstLocalAddress->Domain < SecondLocalAddress->Domain) {
        return ComparisonResultAscending;

    } else if (FirstLocalAddress->Domain > SecondLocalAddress->Domain) {
        return ComparisonResultDescending;
    }

    return ComparisonResultSame;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpDestroyLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine destroys the state for the given link.

Arguments:

    Link - Supplies a pointer to the link that needs to be destroyed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_NETWORK_ENTRY CurrentNetwork;
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry;

    ASSERT(Link->ReferenceCount == 0);
    ASSERT(Link->ListEntry.Next == NULL);

    //
    // Destroy all the link address entries. Don't bother to lock the list as
    // all the references are gone.
    //

    while (LIST_EMPTY(&(Link->LinkAddressList)) == FALSE) {
        LinkAddressEntry = LIST_VALUE(Link->LinkAddressList.Next,
                                      NET_LINK_ADDRESS_ENTRY,
                                      ListEntry);

        LIST_REMOVE(&(LinkAddressEntry->ListEntry));
        MmFreePagedPool(LinkAddressEntry);
    }

    KeDestroyEvent(Link->AddressTranslationEvent);
    KeAcquireSharedExclusiveLockShared(NetPluginListLock);
    CurrentEntry = NetNetworkList.Next;
    while (CurrentEntry != &NetNetworkList) {
        CurrentNetwork = LIST_VALUE(CurrentEntry, NET_NETWORK_ENTRY, ListEntry);
        CurrentNetwork->Interface.DestroyLink(Link);
        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSharedExclusiveLockShared(NetPluginListLock);
    Link->DataLinkEntry->Interface.DestroyLink(Link);
    Link->Properties.Interface.DestroyLink(Link->Properties.DeviceContext);
    IoDeviceReleaseReference(Link->Properties.Device);
    MmFreePagedPool(Link);
    return;
}

VOID
NetpDeactivateSocketUnlocked (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine deactivates and unbinds a socket, preventing the socket from
    receiving incoming packets. It assumes that the net socket tree lock is
    already held. It does not, however, disassociate a socket from its local
    or remote address. Those are still valid properties of the socket, while
    its on its way out.

Arguments:

    Socket - Supplies a pointer to the initialized socket to remove from the
        socket tree.

Return Value:

    None.

--*/

{

    PNET_PROTOCOL_ENTRY Protocol;
    PRED_BLACK_TREE Tree;

    Protocol = Socket->Protocol;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(Protocol->SocketLock) != FALSE);
    ASSERT(Socket->BindingType < SocketBindingTypeCount);

    if (((Socket->Flags & NET_SOCKET_FLAG_ACTIVE) == 0) &&
        (Socket->BindingType == SocketBindingInvalid)) {

        ASSERT(Socket != Protocol->LastSocket);

        return;
    }

    RtlAtomicAnd32(&(Socket->Flags), ~NET_SOCKET_FLAG_ACTIVE);
    Tree = &(Protocol->SocketTree[Socket->BindingType]);
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Deactivating socket %x\n", Socket);
    }

    //
    // Remove this old friend from the tree.
    //

    RtlRedBlackTreeRemove(Tree, &(Socket->U.TreeEntry));
    Socket->BindingType = SocketBindingInvalid;

    //
    // If it was in the socket "cache", then remove it.
    //

    if (Socket == Protocol->LastSocket) {
        Protocol->LastSocket = NULL;
    }

    //
    // Release that reference that was added when the socket was added to the
    // tree. This should not be the last reference on the kernel socket.
    //

    ASSERT(Socket->KernelSocket.ReferenceCount > 1);

    IoSocketReleaseReference(&(Socket->KernelSocket));
    return;
}

VOID
NetpDetachSockets (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress
    )

/*++

Routine Description:

    This routine detaches all of the sockets associated with the given link
    and optional link address.

Arguments:

    Link - Supplies a pointer to the network link whose sockets are to be
        detached.

    LinkAddress - Supplies a pointer to an optional link address entry. If
        supplied then only the link's sockets bound to the given link address
        will be detached.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PRED_BLACK_TREE_NODE Node;
    PNET_PROTOCOL_ENTRY Protocol;
    PNET_SOCKET Socket;
    PRED_BLACK_TREE Tree;

    //
    // The fully and locally bound socket trees must be pruned for each
    // protocol.
    //

    KeAcquireSharedExclusiveLockShared(NetPluginListLock);
    CurrentEntry = NetProtocolList.Next;
    while (CurrentEntry != &NetProtocolList) {
        Protocol = LIST_VALUE(CurrentEntry, NET_PROTOCOL_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        KeAcquireSharedExclusiveLockExclusive(Protocol->SocketLock);
        Tree = &(Protocol->SocketTree[SocketFullyBound]);
        Node = RtlRedBlackTreeGetNextNode(Tree, FALSE, NULL);
        while (Node != NULL) {
            Socket = RED_BLACK_TREE_VALUE(Node, NET_SOCKET, U.TreeEntry);
            Node = RtlRedBlackTreeGetNextNode(Tree, FALSE, Node);
            if ((Socket->Link != Link) ||
                ((LinkAddress != NULL) &&
                 (Socket->LinkAddress != LinkAddress))) {

                continue;
            }

            NetpDetachSocket(Socket);
        }

        //
        // Do the same for locally bound sockets using this link.
        //

        Tree = &(Protocol->SocketTree[SocketLocallyBound]);
        Node = RtlRedBlackTreeGetNextNode(Tree, FALSE, NULL);
        while (Node != NULL) {
            Socket = RED_BLACK_TREE_VALUE(Node, NET_SOCKET, U.TreeEntry);
            Node = RtlRedBlackTreeGetNextNode(Tree, FALSE, Node);
            if ((Socket->Link != Link) ||
                ((LinkAddress != NULL) &&
                 (Socket->LinkAddress != LinkAddress))) {

                continue;
            }

            NetpDetachSocket(Socket);
        }

        KeReleaseSharedExclusiveLockExclusive(Protocol->SocketLock);
    }

    KeReleaseSharedExclusiveLockShared(NetPluginListLock);
    return;
}

VOID
NetpDetachSocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine detaches a socket from all activity as a result of its link
    going down. It assumes the socket lock is held.

Arguments:

    Socket - Supplies a pointer to the network socket that is to be unbound
        from its link.

Return Value:

    None.

--*/

{

    ASSERT((Socket->Link->LinkUp == FALSE) ||
           (Socket->LinkAddress->Configured == FALSE));

    ASSERT((Socket->BindingType == SocketLocallyBound) ||
           (Socket->BindingType == SocketFullyBound));

    NetpDeactivateSocketUnlocked(Socket);
    NET_SOCKET_SET_LAST_ERROR(Socket, STATUS_NO_NETWORK_CONNECTION);
    IoSetIoObjectState(Socket->KernelSocket.IoState,
                       POLL_EVENT_DISCONNECTED,
                       TRUE);

    return;
}

KSTATUS
NetpLookupAddressTranslation (
    PNET_LINK Link,
    PNETWORK_ADDRESS NetworkAddress,
    PNETWORK_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine performs a lookup from network address to physical address
    using the link address translation tree.

Arguments:

    Link - Supplies a pointer to the link that supposedly owns the network
        address.

    NetworkAddress - Supplies the network address to look up.

    PhysicalAddress - Supplies a pointer where the corresponding physical
        address for this network address will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if no corresponding entry could be found.

--*/

{

    PADDRESS_TRANSLATION_ENTRY FoundEntry;
    PRED_BLACK_TREE_NODE FoundNode;
    ADDRESS_TRANSLATION_ENTRY SearchEntry;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    RtlCopyMemory(&(SearchEntry.NetworkAddress),
                  NetworkAddress,
                  sizeof(NETWORK_ADDRESS));

    SearchEntry.NetworkAddress.Port = 0;
    Status = STATUS_NOT_FOUND;
    KeAcquireQueuedLock(Link->QueuedLock);
    FoundNode = RtlRedBlackTreeSearch(&(Link->AddressTranslationTree),
                                      &(SearchEntry.TreeEntry));

    //
    // If a node is found, copy the translation into the result while the lock
    // is still held to avoid racing with someone destroying this node.
    //

    if (FoundNode != NULL) {
        FoundEntry = RED_BLACK_TREE_VALUE(FoundNode,
                                          ADDRESS_TRANSLATION_ENTRY,
                                          TreeEntry);

        RtlCopyMemory(PhysicalAddress,
                      &(FoundEntry->PhysicalAddress),
                      sizeof(NETWORK_ADDRESS));

        Status = STATUS_SUCCESS;
    }

    KeReleaseQueuedLock(Link->QueuedLock);
    return Status;
}

COMPARISON_RESULT
NetpMatchFullyBoundSocket (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS LocalAddress,
    PNETWORK_ADDRESS RemoteAddress
    )

/*++

Routine Description:

    This routine compares a socket to a local address and remote address to
    determine if the socket matches the provided information in a fully bound
    way.

Arguments:

    Socket - Supplies a pointer to a socket to match against the given data.

    LocalAddress - Supplies a pointer to a local network address.

    RemoteAddress - Supplies a pointer to a remote network address.

Return Value:

    Same if the socket has the same values as the data.

    Ascending if the socket's values are less than the data's.

    Descending if the socket's values are greater than the data's.

--*/

{

    ULONG PartIndex;
    COMPARISON_RESULT Result;

    //
    // Compare the local port and local network first. This is required because
    // binding needs to look for fully-bound sockets already using the same
    // local port. This allows bind to iterate over a sub-tree that contains
    // only matching local ports.
    //

    if (Socket->LocalReceiveAddress.Port < LocalAddress->Port) {
        return ComparisonResultAscending;

    } else if (Socket->LocalReceiveAddress.Port > LocalAddress->Port) {
        return ComparisonResultDescending;
    }

    if (Socket->LocalReceiveAddress.Domain < LocalAddress->Domain) {
        return ComparisonResultAscending;

    } else if (Socket->LocalReceiveAddress.Domain > LocalAddress->Domain) {
        return ComparisonResultDescending;
    }

    //
    // The nodes are really only the same if the local and remote addresses are
    // the same. The remote address is the more likely to be different, so try
    // that one first.
    //

    Result = NetpCompareNetworkAddresses(&(Socket->RemoteAddress),
                                         RemoteAddress);

    if (Result != ComparisonResultSame) {
        return Result;
    }

    //
    // Ugh, their remote addresses are the same, check the local addresses.
    //

    for (PartIndex = 0;
         PartIndex < MAX_NETWORK_ADDRESS_SIZE / sizeof(UINTN);
         PartIndex += 1) {

        if (Socket->LocalReceiveAddress.Address[PartIndex] <
            LocalAddress->Address[PartIndex]) {

            return ComparisonResultAscending;

        } else if (Socket->LocalReceiveAddress.Address[PartIndex] >
                   LocalAddress->Address[PartIndex]) {

            return ComparisonResultDescending;
        }

        //
        // The parts here are equal, move on to the next part.
        //

    }

    return ComparisonResultSame;
}

COMPARISON_RESULT
NetpCompareAddressTranslationEntries (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes, in this case two
    network address translation entries.

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

    PADDRESS_TRANSLATION_ENTRY FirstEntry;
    COMPARISON_RESULT Result;
    PADDRESS_TRANSLATION_ENTRY SecondEntry;

    FirstEntry = RED_BLACK_TREE_VALUE(FirstNode,
                                      ADDRESS_TRANSLATION_ENTRY,
                                      TreeEntry);

    SecondEntry = RED_BLACK_TREE_VALUE(SecondNode,
                                       ADDRESS_TRANSLATION_ENTRY,
                                       TreeEntry);

    Result = NetpCompareNetworkAddresses(&(FirstEntry->NetworkAddress),
                                         &(SecondEntry->NetworkAddress));

    return Result;
}

BOOL
NetpCheckLocalAddressAvailability (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS LocalAddress
    )

/*++

Routine Description:

    This routine determines whether or not the given local address can be used
    by the given socket. It takes into account address and port reusability as
    indicated by the socket's flags. It assumes that the socket lock is held.

Arguments:

    Socket - Supplies a pointer to the socket whose local address is to be
        validated.

    LocalAddress - Supplies a pointer to a local network address to validate.

Return Value:

    Returns TRUE if the given local address is OK for the socket to use or
    FALSE otherwise.

--*/

{

    BOOL AddressesMatch;
    BOOL AvailableAddress;
    BOOL DeactivateSocket;
    BOOL Descending;
    PRED_BLACK_TREE_NODE FirstFound;
    BOOL FirstFoundMatched;
    PRED_BLACK_TREE_NODE FoundNode;
    PNET_SOCKET FoundSocket;
    ULONG PartIndex;
    PNET_PROTOCOL_ENTRY Protocol;
    NET_SOCKET SearchSocket;
    PRED_BLACK_TREE Tree;
    BOOL UnspecifiedAddress;

    Protocol = Socket->Protocol;
    FoundSocket = NULL;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(Protocol->SocketLock) != FALSE);

    //
    // Remember if the supplied socket is for the unspecified address.
    //

    UnspecifiedAddress = TRUE;
    for (PartIndex = 0;
         PartIndex < MAX_NETWORK_ADDRESS_SIZE / sizeof(UINTN);
         PartIndex += 1) {

        if (LocalAddress->Address[PartIndex] != 0) {
            UnspecifiedAddress = FALSE;
            break;
        }
    }

    //
    // Create a search entry that does not have a remote address.
    //

    RtlCopyMemory(&(SearchSocket.LocalReceiveAddress),
                  LocalAddress,
                  sizeof(NETWORK_ADDRESS));

    RtlZeroMemory(&(SearchSocket.RemoteAddress), sizeof(NETWORK_ADDRESS));

    //
    // Assume this is going to be a resounding success.
    //

    AvailableAddress = TRUE;

    //
    // Search the tree of fully bound sockets for any using this local address
    // and port combination. Because the search entry's remote address is zero,
    // this should never match exactly, and just return the lowest entry in the
    // tree that matches on network and local port. The compare routine looks
    // at remote address before local address, so this may end up doing a bit
    // of iterating to get through all the necessary entries.
    //

    DeactivateSocket = FALSE;
    Tree = &(Protocol->SocketTree[SocketFullyBound]);
    FoundNode = RtlRedBlackTreeSearchClosest(Tree,
                                             &(SearchSocket.U.TreeEntry),
                                             TRUE);

    while (FoundNode != NULL) {
        FoundSocket = RED_BLACK_TREE_VALUE(FoundNode, NET_SOCKET, U.TreeEntry);
        if (FoundSocket->LocalReceiveAddress.Port != LocalAddress->Port) {
            break;
        }

        if (FoundSocket->LocalReceiveAddress.Domain != LocalAddress->Domain) {
            break;
        }

        //
        // If the supplied socket contains the unspecified address, do not
        // compare it with the found address. It should never match. But if
        // both sockets do not allow address reuse with the any address, then
        // do not allow the any address to use the port.
        //

        if (UnspecifiedAddress != FALSE) {
            if (CAN_REUSE_ANY_ADDRESS(Socket, FoundSocket) == FALSE) {
                AvailableAddress = FALSE;
                break;
            }

        //
        // Otherwise test to see if the addresses match.
        //

        } else {
            AddressesMatch = TRUE;
            for (PartIndex = 0;
                 PartIndex < MAX_NETWORK_ADDRESS_SIZE / sizeof(UINTN);
                 PartIndex += 1) {

                if (FoundSocket->LocalReceiveAddress.Address[PartIndex] !=
                    LocalAddress->Address[PartIndex]) {

                    AddressesMatch = FALSE;
                    break;
                }
            }

            //
            // If the addresses match, then the new socket is only allowed to
            // use the address if either both sockets allow exact address reuse
            // or both sockets allow time wait state address reuse and the
            // found socket is in the time wait state. Deactivate any sockets
            // found in the time wait state if the address is going to be
            // reused.
            //

            if (AddressesMatch != FALSE) {
                if ((CAN_REUSE_EXACT_ADDRESS(Socket, FoundSocket) == FALSE) &&
                    (CAN_REUSE_TIME_WAIT(Socket, FoundSocket) == FALSE)) {

                    AvailableAddress = FALSE;
                    break;
                }

                if ((FoundSocket->Flags & NET_SOCKET_FLAG_TIME_WAIT) != 0) {
                    DeactivateSocket = TRUE;
                }
            }
        }

        //
        // So far, so good. Try the next node.
        //

        FoundNode = RtlRedBlackTreeGetNextNode(Tree, FALSE, FoundNode);

        //
        // If the last socket needed deactivating, do it now that the iteration
        // has moved on. Removing a node does not break iteration.
        //

        if (DeactivateSocket != FALSE) {
            NetpDeactivateSocketUnlocked(FoundSocket);
            DeactivateSocket = FALSE;
        }
    }

    //
    // Exit now if it has already been determined that the address is not valid
    // for use.
    //

    if (AvailableAddress == FALSE) {
        goto CheckLocalAddressAvailabilityEnd;
    }

    //
    // Search the tree of locally bound sockets for any using this local
    // address and port combination. If the search socket is using the
    // unspecified address, then this will not match. It should return the
    // lowest entry in the tree that shares the same port and network. If the
    // search socket is using a complete local address, then this may need to
    // search in both directions on the tree if the first node matches.
    //

    Tree = &(Protocol->SocketTree[SocketLocallyBound]);
    FirstFound = RtlRedBlackTreeSearchClosest(Tree,
                                              &(SearchSocket.U.TreeEntry),
                                              TRUE);

    Descending = FALSE;
    FoundNode = FirstFound;
    FirstFoundMatched = FALSE;
    while (FoundNode != NULL) {
        while (FoundNode != NULL) {
            FoundSocket = RED_BLACK_TREE_VALUE(FoundNode,
                                               NET_SOCKET,
                                               U.TreeEntry);

            if (FoundSocket->LocalReceiveAddress.Port != LocalAddress->Port) {
                break;
            }

            if (FoundSocket->LocalReceiveAddress.Domain !=
                LocalAddress->Domain) {

                break;
            }

            //
            // Locally bound sockets should not be in the time wait state.
            //

            ASSERT((FoundSocket->Flags & NET_SOCKET_FLAG_TIME_WAIT) == 0);

            //
            // If the supplied socket contains the unspecified address, do not
            // compare it with the found address. It should never match. But if
            // both sockets do not allow any address reuse, then do not allow
            // the unspecified address to use the port.
            //

            if (UnspecifiedAddress != FALSE) {
                if (CAN_REUSE_ANY_ADDRESS(Socket, FoundSocket) == FALSE) {
                    AvailableAddress = FALSE;
                    break;
                }

            //
            // Otherwise test to see if the addresses match.
            //

            } else {
                AddressesMatch = TRUE;
                for (PartIndex = 0;
                     PartIndex < MAX_NETWORK_ADDRESS_SIZE / sizeof(UINTN);
                     PartIndex += 1) {

                    if (FoundSocket->LocalReceiveAddress.Address[PartIndex] !=
                        LocalAddress->Address[PartIndex]) {

                        AddressesMatch = FALSE;
                        break;
                    }
                }

                //
                // If the local addresses do not match, then this has gone
                // beyond the range of any matches.
                //

                if (AddressesMatch == FALSE) {
                    break;
                }

                //
                // Record if this was the first found and it matched.
                //

                if (FoundNode == FirstFound) {
                    FirstFoundMatched = TRUE;
                }

                //
                // If the addresses match, then the new socket is only allowed
                // to use the address if both sockets allow exact address reuse.
                //

                if (CAN_REUSE_EXACT_ADDRESS(Socket, FoundSocket) == FALSE) {
                    AvailableAddress = FALSE;
                    break;
                }
            }

            //
            // So far, so good. Try the next node.
            //

            FoundNode = RtlRedBlackTreeGetNextNode(Tree, Descending, FoundNode);
        }

        //
        // Exit now if it has already been determined that the address is not
        // valid for use.
        //

        if (AvailableAddress == FALSE) {
            goto CheckLocalAddressAvailabilityEnd;
        }

        //
        // If the first found was not a match, then the tree does not need to
        // be searched in the descending direction.
        //

        if (FirstFoundMatched == FALSE) {
            break;
        }

        ASSERT(UnspecifiedAddress == FALSE);

        //
        // Switch the search direction once and start over from the node before
        // the first found.
        //

        if (Descending != FALSE) {
            break;
        }

        Descending = TRUE;
        FoundNode = RtlRedBlackTreeGetNextNode(Tree, Descending, FirstFound);
    }

    //
    // Search the tree of unbound sockets for any using this local port. This
    // has to deal with the same situation as the locally bound search because
    // the closest may match with other matches entries both above and below in
    // the tree.
    //

    Tree = &(Protocol->SocketTree[SocketUnbound]);
    FirstFound = RtlRedBlackTreeSearchClosest(Tree,
                                              &(SearchSocket.U.TreeEntry),
                                              TRUE);

    Descending = FALSE;
    FoundNode = FirstFound;
    FirstFoundMatched = FALSE;
    while (FoundNode != NULL) {
        while (FoundNode != NULL) {
            FoundSocket = RED_BLACK_TREE_VALUE(FoundNode,
                                               NET_SOCKET,
                                               U.TreeEntry);

            if (FoundSocket->LocalReceiveAddress.Port != LocalAddress->Port) {
                break;
            }

            if (FoundSocket->LocalReceiveAddress.Domain !=
                LocalAddress->Domain) {

                break;
            }

            //
            // If the first found got this far, then it's a match.
            //

            if (FoundNode == FirstFound) {
                FirstFoundMatched = TRUE;
            }

            //
            // An unbound socket should not be in the time-wait state.
            //

            ASSERT((FoundSocket->Flags & NET_SOCKET_FLAG_TIME_WAIT) == 0);

            //
            // If the supplied socket has an unspecified address, then the
            // addresses match as well. The only way for the new socket to use
            // the address is if reusing the exact address is allowed on both
            // sockets.
            //

            if (UnspecifiedAddress != FALSE) {
                if (CAN_REUSE_EXACT_ADDRESS(Socket, FoundSocket) == FALSE) {
                    AvailableAddress = FALSE;
                    break;
                }

            //
            // Otherwise, the addresses are different. Reuse of the port is
            // only allowed if reusing the any address is allowed on both
            // sockets.
            //

            } else {
                if (CAN_REUSE_ANY_ADDRESS(Socket, FoundSocket) == FALSE) {
                    AvailableAddress = FALSE;
                    break;
                }
            }

            //
            // So far, so good. Try the next node.
            //

            FoundNode = RtlRedBlackTreeGetNextNode(Tree, Descending, FoundNode);
        }

        //
        // Exit now if it has already been determined that the address is not
        // valid for use.
        //

        if (AvailableAddress == FALSE) {
            goto CheckLocalAddressAvailabilityEnd;
        }

        //
        // If the first found was not a match, then the tree does not need to
        // be searched in the descending direction.
        //

        if (FirstFoundMatched == FALSE) {
            break;
        }

        //
        // Switch the search direction once and start over from the node before
        // the first found.
        //

        if (Descending != FALSE) {
            break;
        }

        Descending = TRUE;
        FoundNode = RtlRedBlackTreeGetNextNode(Tree, Descending, FirstFound);
    }

CheckLocalAddressAvailabilityEnd:
    if (AvailableAddress == FALSE) {
        if (NetGlobalDebug != FALSE) {

            ASSERT(FoundSocket != NULL);

            RtlDebugPrint("Net: Rejected address availability of socket %x "
                          "because of existing socket %x.\n",
                          Socket,
                          FoundSocket);
        }
    }

    return AvailableAddress;
}

VOID
NetpGetPacketSizeInformation (
    PNET_LINK Link,
    PNET_SOCKET Socket,
    PNET_PACKET_SIZE_INFORMATION SizeInformation
    )

/*++

Routine Description:

    This routine calculates the packet size information given an link and a
    socket. It uses the unbound packet size information from the socket in
    order to calculate the resulting size information.

Arguments:

    Link - Supplies a pointer to a network link.

    Socket - Supplies a pointer to a network socket.

    SizeInformation - Supplies a pointer to a packet size information structure
        that receives the calculated max packet, header, and footer size to
        use for sending packets from the given socket out over the given
        network link.

Return Value:

    None.

--*/

{

    PNET_DATA_LINK_ENTRY DataLinkEntry;
    NET_PACKET_SIZE_INFORMATION DataLinkInformation;
    ULONG FooterSize;
    ULONG HeaderSize;
    ULONG MaxPacketSize;
    ULONG MinPacketSize;

    //
    // Add the data link layer's header and footer sizes to the socket's
    // unbound max packet size. If this is greater than the allowed maximum
    // packet size for the data link layer, then truncate it.
    //

    DataLinkEntry = Link->DataLinkEntry;
    DataLinkEntry->Interface.GetPacketSizeInformation(Link->DataLinkContext,
                                                      &DataLinkInformation,
                                                      0);

    MaxPacketSize = DataLinkInformation.HeaderSize +
                    Socket->UnboundPacketSizeInformation.MaxPacketSize +
                    DataLinkInformation.FooterSize;

    if (MaxPacketSize > DataLinkInformation.MaxPacketSize) {
        MaxPacketSize = DataLinkInformation.MaxPacketSize;
    }

    //
    // Add the data link layer's header and footer sizes to the socket's
    // unbound minimum packet size. The maximum of the minimum packet size is
    // what wins here.
    //

    MinPacketSize = DataLinkInformation.HeaderSize +
                    Socket->UnboundPacketSizeInformation.MinPacketSize +
                    DataLinkInformation.FooterSize;

    if (MinPacketSize < DataLinkInformation.MinPacketSize) {
        MinPacketSize = DataLinkInformation.MinPacketSize;
    }

    //
    // Repeat for the device link layer, truncating the allowed maximum packet
    // size if necessary.
    //

    MaxPacketSize = Link->Properties.PacketSizeInformation.HeaderSize +
                    MaxPacketSize +
                    Link->Properties.PacketSizeInformation.FooterSize;

    if (MaxPacketSize > Link->Properties.PacketSizeInformation.MaxPacketSize) {
        MaxPacketSize = Link->Properties.PacketSizeInformation.MaxPacketSize;
    }

    //
    // Repeat for the device link layer, increasing the the minimum packet
    // size if necessary.
    //

    MinPacketSize = Link->Properties.PacketSizeInformation.HeaderSize +
                    MinPacketSize +
                    Link->Properties.PacketSizeInformation.FooterSize;

    if (MinPacketSize < Link->Properties.PacketSizeInformation.MinPacketSize) {
        MinPacketSize = Link->Properties.PacketSizeInformation.MinPacketSize;
    }

    SizeInformation->MaxPacketSize = MaxPacketSize;
    SizeInformation->MinPacketSize = MinPacketSize;

    //
    // The headers and footers of all layers are included in the final tally.
    //

    HeaderSize = Socket->UnboundPacketSizeInformation.HeaderSize +
                 DataLinkInformation.HeaderSize +
                 Link->Properties.PacketSizeInformation.HeaderSize;

    SizeInformation->HeaderSize = HeaderSize;
    FooterSize = Socket->UnboundPacketSizeInformation.FooterSize +
                 DataLinkInformation.FooterSize +
                 Link->Properties.PacketSizeInformation.FooterSize;

    SizeInformation->FooterSize = FooterSize;
    return;
}

VOID
NetpDebugPrintNetworkAddress (
    PNET_NETWORK_ENTRY Network,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine prints the given address to the debug console. It must belong
    to the given network.

Arguments:

    Network - Supplies a pointer to the network to which the address belongs.

    Address - Supplies a pointer to the address to print.

Return Value:

    None.

--*/

{

    ULONG Length;
    CHAR StringBuffer[NET_PRINT_ADDRESS_STRING_LENGTH];

    ASSERT(Network->Domain == Address->Domain);

    StringBuffer[0] = '\0';
    Length = Network->Interface.PrintAddress(Address,
                                             StringBuffer,
                                             NET_PRINT_ADDRESS_STRING_LENGTH);

    ASSERT(Length <= NET_PRINT_ADDRESS_STRING_LENGTH);

    StringBuffer[NET_PRINT_ADDRESS_STRING_LENGTH - 1] = '\0';
    RtlDebugPrint("%s", StringBuffer);
    return;
}

