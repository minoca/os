/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    netcore.c

Abstract:

    This module implements the networking core library.

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
#include "ethernet.h"

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
NetCreateSocket (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    PSOCKET *NewSocket
    );

VOID
NetDestroySocket (
    PSOCKET Socket
    );

KSTATUS
NetBindToAddress (
    PSOCKET Socket,
    PVOID Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetListen (
    PSOCKET Socket,
    ULONG BacklogCount
    );

KSTATUS
NetAccept (
    PSOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    );

KSTATUS
NetConnect (
    PSOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetCloseSocket (
    PSOCKET Socket
    );

KSTATUS
NetSendData (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetReceiveData (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetGetSetSocketInformation (
    PSOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetShutdown (
    PSOCKET Socket,
    ULONG ShutdownType
    );

KSTATUS
NetUserControl (
    PSOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

VOID
NetpDestroyProtocol (
    PNET_PROTOCOL_ENTRY Protocol
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the lists of supported types for various networking layers.
//

LIST_ENTRY NetProtocolList;
LIST_ENTRY NetNetworkList;
LIST_ENTRY NetDataLinkList;
PSHARED_EXCLUSIVE_LOCK NetPluginListLock;

BOOL NetInitialized = FALSE;

//
// Define the global debug flag, which propagates throughout the networking
// subsystem.
//

BOOL NetGlobalDebug = FALSE;

//
// Optimize a bit by storing pointers directly to the super common network and
// protocol entries.
//

PNET_NETWORK_ENTRY NetArpNetworkEntry = NULL;
PNET_NETWORK_ENTRY NetIp4NetworkEntry = NULL;
PNET_NETWORK_ENTRY NetIp6NetworkEntry = NULL;

PNET_PROTOCOL_ENTRY NetTcpProtocolEntry = NULL;
PNET_PROTOCOL_ENTRY NetUdpProtocolEntry = NULL;
PNET_PROTOCOL_ENTRY NetRawProtocolEntry = NULL;

NET_INTERFACE NetInterface = {
    NetCreateSocket,
    NetDestroySocket,
    NetBindToAddress,
    NetListen,
    NetAccept,
    NetConnect,
    NetCloseSocket,
    NetSendData,
    NetReceiveData,
    NetGetSetSocketInformation,
    NetShutdown,
    NetUserControl
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine implements the initial entry point of the networking core
    library, called when the library is first loaded.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    Status code.

--*/

{

    BOOL BuffersInitialized;
    KSTATUS Status;

    ASSERT(NetInitialized == FALSE);

    BuffersInitialized = FALSE;

    //
    // The core networking driver never goes away, even if the driver that
    // imported it is unloaded.
    //

    IoDriverAddReference(Driver);
    INITIALIZE_LIST_HEAD(&NetProtocolList);
    INITIALIZE_LIST_HEAD(&NetNetworkList);
    INITIALIZE_LIST_HEAD(&NetDataLinkList);
    NetPluginListLock = KeCreateSharedExclusiveLock();
    if (NetPluginListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DriverEntryEnd;
    }

    Status = NetpInitializeBuffers();
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    BuffersInitialized = TRUE;
    Status = NetpInitializeNetworkLayer();
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Set up the built in protocols, networks, data links and miscellaneous
    // components.
    //

    NetpEthernetInitialize();
    NetpIp4Initialize();
    NetpArpInitialize();
    NetpUdpInitialize();
    NetpTcpInitialize();
    NetpRawInitialize();
    NetpDhcpInitialize();
    NetpNetlinkInitialize();
    NetpNetlinkGenericInitialize(0);

    //
    // Set up the networking interface to the kernel.
    //

    IoInitializeCoreNetworking(&NetInterface);
    NetInitialized = TRUE;
    Status = STATUS_SUCCESS;

    //
    // Handle any post-registration work for the built in protocols, networks,
    // data links and miscellaneous components.
    //

    NetpNetlinkGenericInitialize(1);

DriverEntryEnd:
    if (!KSUCCESS(Status)) {
        if (NetPluginListLock != NULL) {
            KeDestroySharedExclusiveLock(NetPluginListLock);
            NetPluginListLock = NULL;
        }

        if (BuffersInitialized != FALSE) {
            NetpDestroyBuffers();
        }
    }

    return Status;
}

NET_API
KSTATUS
NetRegisterProtocol (
    PNET_PROTOCOL_ENTRY NewProtocol,
    PHANDLE ProtocolHandle
    )

/*++

Routine Description:

    This routine registers a new protocol type with the core networking
    library.

Arguments:

    NewProtocol - Supplies a pointer to the protocol information. The core
        library will not reference this memory after the function returns, a
        copy will be made.

    ProtocolHandle - Supplies an optional pointer that receives a handle to the
        registered protocol on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if part of the structure isn't filled out
    correctly.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

    STATUS_DUPLICATE_ENTRY if the socket type is already registered.

--*/

{

    PLIST_ENTRY CurrentEntry;
    HANDLE Handle;
    BOOL LockHeld;
    PNET_PROTOCOL_ENTRY NewProtocolCopy;
    PNET_PROTOCOL_ENTRY Protocol;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LockHeld = FALSE;
    Handle = INVALID_HANDLE;
    NewProtocolCopy = NULL;
    if ((NewProtocol->Type == NetSocketInvalid) ||
        (NewProtocol->Interface.CreateSocket == NULL) ||
        (NewProtocol->Interface.DestroySocket == NULL) ||
        (NewProtocol->Interface.BindToAddress == NULL) ||
        (NewProtocol->Interface.Listen == NULL) ||
        (NewProtocol->Interface.Accept == NULL) ||
        (NewProtocol->Interface.Connect == NULL) ||
        (NewProtocol->Interface.Close == NULL) ||
        (NewProtocol->Interface.Shutdown == NULL) ||
        (NewProtocol->Interface.Send == NULL) ||
        (NewProtocol->Interface.ProcessReceivedData == NULL) ||
        (NewProtocol->Interface.Receive == NULL) ||
        (NewProtocol->Interface.GetSetInformation == NULL) ||
        (NewProtocol->Interface.UserControl == NULL) ||
        (NewProtocol->Interface.ProcessReceivedSocketData == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto RegisterProtocolEnd;
    }

    //
    // Create a copy of the new protocol.
    //

    NewProtocolCopy = MmAllocatePagedPool(sizeof(NET_PROTOCOL_ENTRY),
                                          NET_CORE_ALLOCATION_TAG);

    if (NewProtocolCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RegisterProtocolEnd;
    }

    RtlCopyMemory(NewProtocolCopy, NewProtocol, sizeof(NET_PROTOCOL_ENTRY));
    NewProtocolCopy->SocketLock = KeCreateSharedExclusiveLock();
    if (NewProtocolCopy->SocketLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RegisterProtocolEnd;
    }

    RtlRedBlackTreeInitialize(&(NewProtocolCopy->SocketTree[SocketUnbound]),
                              0,
                              NetpCompareUnboundSockets);

    RtlRedBlackTreeInitialize(
                            &(NewProtocolCopy->SocketTree[SocketLocallyBound]),
                            0,
                            NetpCompareLocallyBoundSockets);

    RtlRedBlackTreeInitialize(&(NewProtocolCopy->SocketTree[SocketFullyBound]),
                              0,
                              NetpCompareFullyBoundSockets);

    KeAcquireSharedExclusiveLockExclusive(NetPluginListLock);
    LockHeld = TRUE;

    //
    // Loop through looking for a previous registration with this protocol type
    // and parent protocol number pair.
    //

    CurrentEntry = NetProtocolList.Next;
    while (CurrentEntry != &NetProtocolList) {
        Protocol = LIST_VALUE(CurrentEntry, NET_PROTOCOL_ENTRY, ListEntry);
        if ((Protocol->Type == NewProtocol->Type) &&
            (Protocol->ParentProtocolNumber ==
             NewProtocol->ParentProtocolNumber)) {

            Status = STATUS_DUPLICATE_ENTRY;
            goto RegisterProtocolEnd;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // There are no duplicates, add this entry to the back of the list.
    //

    INSERT_BEFORE(&(NewProtocolCopy->ListEntry), &NetProtocolList);

    //
    // Save the common ones for quick access.
    //

    if ((NewProtocolCopy->Type == NetSocketStream) &&
        (NewProtocolCopy->ParentProtocolNumber ==
         SOCKET_INTERNET_PROTOCOL_TCP)) {

        NetTcpProtocolEntry = NewProtocolCopy;

    } else if ((NewProtocolCopy->Type == NetSocketDatagram) &&
               (NewProtocolCopy->ParentProtocolNumber ==
                SOCKET_INTERNET_PROTOCOL_UDP)) {

        NetUdpProtocolEntry = NewProtocolCopy;

    } else if (NewProtocolCopy->Type == NetSocketRaw) {
        NetRawProtocolEntry = NewProtocolCopy;
    }

    Status = STATUS_SUCCESS;
    Handle = NewProtocolCopy;

RegisterProtocolEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetPluginListLock);
    }

    if (!KSUCCESS(Status)) {
        if (NewProtocolCopy != NULL) {
            NetpDestroyProtocol(NewProtocolCopy);
        }
    }

    if (ProtocolHandle != NULL) {
        *ProtocolHandle = Handle;
    }

    return Status;
}

NET_API
VOID
NetUnregisterProtocol (
    HANDLE ProtocolHandle
    )

/*++

Routine Description:

    This routine unregisters the given protocol from the core networking
    library.

Arguments:

    ProtocolHandle - Supplies the handle to the protocol to unregister.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_PROTOCOL_ENTRY FoundProtocol;
    PNET_PROTOCOL_ENTRY Protocol;

    FoundProtocol = NULL;

    //
    // Loop through looking for a previous registration with this protocol
    // handle.
    //

    KeAcquireSharedExclusiveLockExclusive(NetPluginListLock);
    CurrentEntry = NetProtocolList.Next;
    while (CurrentEntry != &NetProtocolList) {
        Protocol = LIST_VALUE(CurrentEntry, NET_PROTOCOL_ENTRY, ListEntry);
        if (Protocol == ProtocolHandle) {
            FoundProtocol = Protocol;
            LIST_REMOVE(CurrentEntry);
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSharedExclusiveLockExclusive(NetPluginListLock);
    if (FoundProtocol != NULL) {
        NetpDestroyProtocol(FoundProtocol);
    }

    return;
}

NET_API
KSTATUS
NetRegisterNetworkLayer (
    PNET_NETWORK_ENTRY NewNetworkEntry,
    PHANDLE NetworkHandle
    )

/*++

Routine Description:

    This routine registers a new network type with the core networking library.

Arguments:

    NewNetworkEntry - Supplies a pointer to the network information. The core
        library will not reference this memory after the function returns, a
        copy will be made.

    NetworkHandle - Supplies a pointer that receives a handle to the
        registered network layer on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if part of the structure isn't filled out
    correctly.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

    STATUS_DUPLICATE_ENTRY if the network type is already registered.

--*/

{

    PLIST_ENTRY CurrentEntry;
    HANDLE Handle;
    BOOL LockHeld;
    PNET_NETWORK_ENTRY Network;
    PNET_NETWORK_ENTRY NewNetworkCopy;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LockHeld = FALSE;
    Handle = INVALID_HANDLE;
    NewNetworkCopy = NULL;
    if ((NewNetworkEntry->Domain == NetDomainInvalid) ||
        (NewNetworkEntry->Interface.InitializeLink == NULL) ||
        (NewNetworkEntry->Interface.DestroyLink == NULL) ||
        (NewNetworkEntry->Interface.InitializeSocket == NULL) ||
        (NewNetworkEntry->Interface.BindToAddress == NULL) ||
        (NewNetworkEntry->Interface.Listen == NULL) ||
        (NewNetworkEntry->Interface.Connect == NULL) ||
        (NewNetworkEntry->Interface.Disconnect == NULL) ||
        (NewNetworkEntry->Interface.Close == NULL) ||
        (NewNetworkEntry->Interface.Send == NULL) ||
        (NewNetworkEntry->Interface.ProcessReceivedData == NULL) ||
        (NewNetworkEntry->Interface.PrintAddress == NULL) ||
        (NewNetworkEntry->Interface.GetSetInformation == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto RegisterNetworkLayerEnd;
    }

    //
    // Create a copy of the new network entry.
    //

    NewNetworkCopy = MmAllocatePagedPool(sizeof(NET_NETWORK_ENTRY),
                                         NET_CORE_ALLOCATION_TAG);

    if (NewNetworkCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RegisterNetworkLayerEnd;
    }

    RtlCopyMemory(NewNetworkCopy, NewNetworkEntry, sizeof(NET_NETWORK_ENTRY));
    KeAcquireSharedExclusiveLockExclusive(NetPluginListLock);
    LockHeld = TRUE;

    //
    // Loop through looking for a previous registration with this network layer
    // number.
    //

    CurrentEntry = NetNetworkList.Next;
    while (CurrentEntry != &NetNetworkList) {
        Network = LIST_VALUE(CurrentEntry, NET_NETWORK_ENTRY, ListEntry);
        if (Network->Domain == NewNetworkEntry->Domain) {
            Status = STATUS_DUPLICATE_ENTRY;
            goto RegisterNetworkLayerEnd;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // There are no duplicates, add this entry to the back of the list.
    //

    INSERT_BEFORE(&(NewNetworkCopy->ListEntry), &NetNetworkList);

    //
    // Save quick-access pointers for the common ones.
    //

    if (NewNetworkCopy->Domain == NetDomainIp4) {
        NetIp4NetworkEntry = NewNetworkCopy;

    } else if (NewNetworkCopy->Domain == NetDomainIp6) {
        NetIp6NetworkEntry = NewNetworkCopy;

    } else if (NewNetworkCopy->Domain == NetDomainArp) {
        NetArpNetworkEntry = NewNetworkCopy;
    }

    Status = STATUS_SUCCESS;
    Handle = NewNetworkCopy;

RegisterNetworkLayerEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetPluginListLock);
    }

    if (!KSUCCESS(Status)) {
        if (NewNetworkCopy != NULL) {
            MmFreePagedPool(NewNetworkCopy);
        }
    }

    if (NetworkHandle != NULL) {
        *NetworkHandle = Handle;
    }

    return Status;
}

NET_API
VOID
NetUnregisterNetworkLayer (
    HANDLE NetworkHandle
    )

/*++

Routine Description:

    This routine unregisters the given network layer from the core networking
    library.

Arguments:

    NetworkHandle - Supplies the handle to the network layer to unregister.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_NETWORK_ENTRY FoundNetwork;
    PNET_NETWORK_ENTRY Network;

    FoundNetwork = NULL;

    //
    // Loop through looking for a previous registration with this network
    // handle.
    //

    KeAcquireSharedExclusiveLockExclusive(NetPluginListLock);
    CurrentEntry = NetNetworkList.Next;
    while (CurrentEntry != &NetNetworkList) {
        Network = LIST_VALUE(CurrentEntry, NET_NETWORK_ENTRY, ListEntry);
        if (Network == NetworkHandle) {
            FoundNetwork = Network;
            LIST_REMOVE(CurrentEntry);
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSharedExclusiveLockExclusive(NetPluginListLock);
    if (FoundNetwork != NULL) {
        MmFreePagedPool(FoundNetwork);
    }

    return;
}

NET_API
KSTATUS
NetRegisterDataLinkLayer (
    PNET_DATA_LINK_ENTRY NewDataLinkEntry,
    PHANDLE DataLinkHandle
    )

/*++

Routine Description:

    This routine registers a new data link type with the core networking
    library.

Arguments:

    NewDataLinkEntry - Supplies a pointer to the link information. The core
        library will not reference this memory after the function returns, a
        copy will be made.

    DataLinkHandle - Supplies a pointer that receives a handle to the
        registered data link layer on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if part of the structure isn't filled out
    correctly.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

    STATUS_DUPLICATE_ENTRY if the link type is already registered.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_DATA_LINK_ENTRY DataLink;
    HANDLE Handle;
    BOOL LockHeld;
    PNET_DATA_LINK_ENTRY NewDataLinkCopy;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    LockHeld = FALSE;
    Handle = INVALID_HANDLE;
    NewDataLinkCopy = NULL;
    if ((NewDataLinkEntry->Domain == NetDomainInvalid) ||
        (NewDataLinkEntry->Interface.InitializeLink == NULL) ||
        (NewDataLinkEntry->Interface.DestroyLink == NULL) ||
        (NewDataLinkEntry->Interface.Send == NULL) ||
        (NewDataLinkEntry->Interface.ProcessReceivedPacket == NULL) ||
        (NewDataLinkEntry->Interface.GetBroadcastAddress == NULL) ||
        (NewDataLinkEntry->Interface.PrintAddress == NULL) ||
        (NewDataLinkEntry->Interface.GetPacketSizeInformation == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto RegisterDataLinkLayerEnd;
    }

    //
    // Create a copy of the new link entry.
    //

    NewDataLinkCopy = MmAllocatePagedPool(sizeof(NET_DATA_LINK_ENTRY),
                                          NET_CORE_ALLOCATION_TAG);

    if (NewDataLinkCopy == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RegisterDataLinkLayerEnd;
    }

    RtlCopyMemory(NewDataLinkCopy,
                  NewDataLinkEntry,
                  sizeof(NET_DATA_LINK_ENTRY));

    KeAcquireSharedExclusiveLockExclusive(NetPluginListLock);
    LockHeld = TRUE;

    //
    // Loop through looking for a previous registration with this data link
    // type.
    //

    CurrentEntry = NetDataLinkList.Next;
    while (CurrentEntry != &NetDataLinkList) {
        DataLink = LIST_VALUE(CurrentEntry, NET_DATA_LINK_ENTRY, ListEntry);
        if (DataLink->Domain == NewDataLinkEntry->Domain) {
            Status = STATUS_DUPLICATE_ENTRY;
            goto RegisterDataLinkLayerEnd;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // There are no duplicates, add this entry to the back of the list.
    //

    INSERT_BEFORE(&(NewDataLinkCopy->ListEntry), &NetDataLinkList);
    Status = STATUS_SUCCESS;
    Handle = NewDataLinkCopy;

RegisterDataLinkLayerEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(NetPluginListLock);
    }

    if (!KSUCCESS(Status)) {
        if (NewDataLinkCopy != NULL) {
            MmFreePagedPool(NewDataLinkCopy);
        }
    }

    if (DataLinkHandle != NULL) {
        *DataLinkHandle = Handle;
    }

    return Status;
}

NET_API
VOID
NetUnregisterDataLinkLayer (
    HANDLE DataLinkHandle
    )

/*++

Routine Description:

    This routine unregisters the given data link layer from the core networking
    library.

Arguments:

    DataLinkHandle - Supplies the handle to the data link layer to unregister.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_DATA_LINK_ENTRY DataLink;
    PNET_DATA_LINK_ENTRY FoundDataLink;

    FoundDataLink = NULL;

    //
    // Loop through looking for a previous registration with this data link
    // handle.
    //

    KeAcquireSharedExclusiveLockExclusive(NetPluginListLock);
    CurrentEntry = NetDataLinkList.Next;
    while (CurrentEntry != &NetDataLinkList) {
        DataLink = LIST_VALUE(CurrentEntry, NET_DATA_LINK_ENTRY, ListEntry);
        if (DataLink == DataLinkHandle) {
            FoundDataLink = DataLink;
            LIST_REMOVE(CurrentEntry);
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSharedExclusiveLockExclusive(NetPluginListLock);
    if (FoundDataLink != NULL) {
        MmFreePagedPool(FoundDataLink);
    }

    return;
}

NET_API
PNET_NETWORK_ENTRY
NetGetNetworkEntry (
    ULONG ParentProtocolNumber
    )

/*++

Routine Description:

    This routine looks up a registered network layer given the parent protocol
    number.

Arguments:

    ParentProtocolNumber - Supplies the parent protocol number of the desired
        network layer.

Return Value:

    Returns a pointer to the network layer entry on success.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_NETWORK_ENTRY NetworkEntry;
    BOOL NetworkFound;

    //
    // Try the common ones first before going heavy by acquiring the lock and
    // looping over the list.
    //

    if (ParentProtocolNumber == IP4_PROTOCOL_NUMBER) {
        NetworkEntry = NetIp4NetworkEntry;

    } else if (ParentProtocolNumber == ARP_PROTOCOL_NUMBER) {
        NetworkEntry = NetArpNetworkEntry;

    } else if (ParentProtocolNumber == IP6_PROTOCOL_NUMBER) {
        NetworkEntry = NetIp6NetworkEntry;

    } else {

        //
        // Search through the list of known network entries.
        //

        NetworkEntry = NULL;
        NetworkFound = FALSE;
        KeAcquireSharedExclusiveLockShared(NetPluginListLock);
        CurrentEntry = NetNetworkList.Next;
        while (CurrentEntry != &NetNetworkList) {
            NetworkEntry = LIST_VALUE(CurrentEntry,
                                      NET_NETWORK_ENTRY,
                                      ListEntry);

            if (NetworkEntry->ParentProtocolNumber == ParentProtocolNumber) {
                NetworkFound = TRUE;
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        KeReleaseSharedExclusiveLockShared(NetPluginListLock);
        if (NetworkFound == FALSE) {
            return NULL;
        }
    }

    return NetworkEntry;
}

NET_API
PNET_PROTOCOL_ENTRY
NetGetProtocolEntry (
    ULONG ParentProtocolNumber
    )

/*++

Routine Description:

    This routine looks up a registered protocol layer given the parent protocol
    number.

Arguments:

    ParentProtocolNumber - Supplies the parent protocol number of the desired
        protocol layer.

Return Value:

    Returns a pointer to the protocol layer entry on success.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_PROTOCOL_ENTRY ProtocolEntry;
    BOOL ProtocolFound;

    //
    // Try the common ones first before getting heavy with the spin lock.
    //

    if (ParentProtocolNumber == SOCKET_INTERNET_PROTOCOL_TCP) {
        ProtocolEntry = NetTcpProtocolEntry;

    } else if (ParentProtocolNumber == SOCKET_INTERNET_PROTOCOL_UDP) {
        ProtocolEntry = NetUdpProtocolEntry;

    } else if (ParentProtocolNumber == SOCKET_INTERNET_PROTOCOL_RAW) {
        ProtocolEntry = NetRawProtocolEntry;

    } else {

        //
        // Search through the list of known protocols.
        //

        ProtocolEntry = NULL;
        ProtocolFound = FALSE;
        KeAcquireSharedExclusiveLockShared(NetPluginListLock);
        CurrentEntry = NetProtocolList.Next;
        while (CurrentEntry != &NetProtocolList) {
            ProtocolEntry = LIST_VALUE(CurrentEntry,
                                       NET_PROTOCOL_ENTRY,
                                       ListEntry);

            if (ProtocolEntry->ParentProtocolNumber == ParentProtocolNumber) {
                ProtocolFound = TRUE;
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        KeReleaseSharedExclusiveLockShared(NetPluginListLock);
        if (ProtocolFound == FALSE) {
            return NULL;
        }
    }

    return ProtocolEntry;
}

NET_API
VOID
NetProcessReceivedPacket (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine is called by the low level NIC driver to pass received packets
    onto the core networking library for dispatching.

Arguments:

    Link - Supplies a pointer to the link that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    //
    // Call the data link layer to process the packet.
    //

    Link->DataLinkEntry->Interface.ProcessReceivedPacket(Link->DataLinkContext,
                                                         Packet);

    return;
}

NET_API
BOOL
NetGetGlobalDebugFlag (
    VOID
    )

/*++

Routine Description:

    This routine returns the current value of the global networking debug flag.

Arguments:

    None.

Return Value:

    TRUE if debug information should be collected throughout the networking
    subsystem.

    FALSE if verbose debug information should be suppressed globally.

--*/

{

    return NetGlobalDebug;
}

NET_API
VOID
NetDebugPrintAddress (
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine prints the given address to the debug console.

Arguments:

    Address - Supplies a pointer to the address to print.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_DATA_LINK_ENTRY DataLinkEntry;
    ULONG Length;
    PNET_NETWORK_ENTRY NetworkEntry;
    CHAR StringBuffer[NET_PRINT_ADDRESS_STRING_LENGTH];

    if (Address == NULL) {
        RtlDebugPrint("(NullNetworkAddress)");
        return;
    }

    StringBuffer[0] = '\0';

    //
    // If the address is a physical one, find the data link layer and print the
    // string.
    //

    KeAcquireSharedExclusiveLockShared(NetPluginListLock);
    if (NET_IS_DOMAIN_PHYSICAL(Address->Domain) != FALSE) {
        CurrentEntry = NetDataLinkList.Next;
        while (CurrentEntry != &NetDataLinkList) {
            DataLinkEntry = LIST_VALUE(CurrentEntry,
                                       NET_DATA_LINK_ENTRY,
                                       ListEntry);

            if (DataLinkEntry->Domain == Address->Domain) {
                Length = DataLinkEntry->Interface.PrintAddress(
                                              Address,
                                              StringBuffer,
                                              NET_PRINT_ADDRESS_STRING_LENGTH);

                ASSERT(Length <= NET_PRINT_ADDRESS_STRING_LENGTH);

                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

    //
    // Otherwise, find the network layer and print this string.
    //

    } else {
        CurrentEntry = NetNetworkList.Next;
        while (CurrentEntry != &NetNetworkList) {
            NetworkEntry = LIST_VALUE(CurrentEntry,
                                      NET_NETWORK_ENTRY,
                                      ListEntry);

            if (NetworkEntry->Domain == Address->Domain) {
                Length = NetworkEntry->Interface.PrintAddress(
                                              Address,
                                              StringBuffer,
                                              NET_PRINT_ADDRESS_STRING_LENGTH);

                ASSERT(Length <= NET_PRINT_ADDRESS_STRING_LENGTH);

                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }
    }

    KeReleaseSharedExclusiveLockShared(NetPluginListLock);
    StringBuffer[NET_PRINT_ADDRESS_STRING_LENGTH - 1] = '\0';
    RtlDebugPrint("%s", StringBuffer);
    return;
}

KSTATUS
NetCreateSocket (
    NET_DOMAIN_TYPE Domain,
    NET_SOCKET_TYPE Type,
    ULONG Protocol,
    PSOCKET *NewSocket
    )

/*++

Routine Description:

    This routine allocates resources associated with a new socket. The core
    networking driver is responsible for allocating the structure (with
    additional length for any of its context). The kernel will fill in the
    common header when this routine returns.

Arguments:

    Domain - Supplies the network domain to use on the socket.

    Type - Supplies the socket connection type.

    Protocol - Supplies the raw protocol value for this socket used on the
        network. This value is network specific.

    NewSocket - Supplies a pointer where a pointer to a newly allocated
        socket structure will be returned. The caller is responsible for
        allocating the socket (and potentially a larger structure for its own
        context). The kernel will fill in the standard socket structure after
        this routine returns.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PNET_NETWORK_ENTRY NetworkEntry;
    BOOL NetworkFound;
    PNET_PROTOCOL_ENTRY ProtocolEntry;
    BOOL ProtocolFound;
    PNET_SOCKET Socket;
    KSTATUS Status;

    Socket = NULL;

    //
    // Attempt to find a handler for this protocol and network. Make sure that
    // the supplied network protocol matches the found protocol entry's parent
    // protocol. If not, then it's a protocol for a different network.
    //

    ProtocolEntry = NULL;
    ProtocolFound = FALSE;
    NetworkEntry = NULL;
    NetworkFound = FALSE;
    KeAcquireSharedExclusiveLockShared(NetPluginListLock);
    CurrentEntry = NetProtocolList.Next;
    while (CurrentEntry != &NetProtocolList) {
        ProtocolEntry = LIST_VALUE(CurrentEntry, NET_PROTOCOL_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (ProtocolEntry->Type != Type) {
            continue;
        }

        if ((Type != NetSocketRaw) &&
            (Protocol != 0) &&
            (ProtocolEntry->ParentProtocolNumber != Protocol)) {

            continue;
        }

        ProtocolFound = TRUE;
        break;
    }

    CurrentEntry = NetNetworkList.Next;
    while (CurrentEntry != &NetNetworkList) {
        NetworkEntry = LIST_VALUE(CurrentEntry, NET_NETWORK_ENTRY, ListEntry);
        if (NetworkEntry->Domain == Domain) {
            NetworkFound = TRUE;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSharedExclusiveLockShared(NetPluginListLock);
    if (NetworkFound == FALSE) {
        Status = STATUS_DOMAIN_NOT_SUPPORTED;
        goto CreateSocketEnd;
    }

    if (ProtocolFound == FALSE) {
        Status = STATUS_PROTOCOL_NOT_SUPPORTED;
        goto CreateSocketEnd;
    }

    if (Protocol == 0) {
        Protocol = ProtocolEntry->ParentProtocolNumber;
    }

    //
    // Call the protocol to create the socket.
    //

    Status = ProtocolEntry->Interface.CreateSocket(ProtocolEntry,
                                                   NetworkEntry,
                                                   Protocol,
                                                   &Socket);

    if (!KSUCCESS(Status)) {
        goto CreateSocketEnd;
    }

    //
    // Initialize core networking common parameters.
    //

    Socket->Protocol = ProtocolEntry;
    Socket->Network = NetworkEntry;
    Socket->BindingType = SocketBindingInvalid;
    Socket->LastError = STATUS_SUCCESS;
    RtlCopyMemory(&(Socket->UnboundPacketSizeInformation),
                  &(Socket->PacketSizeInformation),
                  sizeof(NET_PACKET_SIZE_INFORMATION));

CreateSocketEnd:
    if (!KSUCCESS(Status)) {
        if (Socket != NULL) {
            ProtocolEntry->Interface.Close(Socket);
            Socket = NULL;
        }
    }

    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Create socket (%d, %d, %d): %x: %x\n",
                      Domain,
                      Type,
                      Protocol,
                      Socket,
                      Status);
    }

    *NewSocket = (PSOCKET)Socket;
    return Status;
}

VOID
NetDestroySocket (
    PSOCKET Socket
    )

/*++

Routine Description:

    This routine destroys resources associated with an open socket, officially
    marking the end of the kernel's knowledge of this structure. It is called
    automatically when a socket's reference count drops to zero.

Arguments:

    Socket - Supplies a pointer to the socket to destroy. The kernel will have
        already destroyed any resources inside the common header, the core
        networking library should not reach through any pointers inside the
        socket header.

Return Value:

    None. This routine is responsible for freeing the memory associated with
    the socket structure itself.

--*/

{

    PNET_SOCKET NetSocket;

    NetSocket = (PNET_SOCKET)Socket;
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Destroy socket %x\n", NetSocket);
    }

    if (NetSocket->Link != NULL) {
        NetLinkReleaseReference(NetSocket->Link);
        NetSocket->Link = NULL;
    }

    NetSocket->Protocol->Interface.DestroySocket(NetSocket);
    return;
}

KSTATUS
NetBindToAddress (
    PSOCKET Socket,
    PVOID Link,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine binds the socket to the given address and starts listening for
    client requests.

Arguments:

    Socket - Supplies a pointer to the socket to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

Return Value:

    Status code.

--*/

{

    PNET_SOCKET NetSocket;
    KSTATUS Status;

    NetSocket = (PNET_SOCKET)Socket;
    Status = NetSocket->Protocol->Interface.BindToAddress(NetSocket,
                                                          Link,
                                                          Address);

    if (!KSUCCESS(Status)) {
        goto BindToAddressEnd;
    }

    Status = STATUS_SUCCESS;

BindToAddressEnd:
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Bind socket %x to ", NetSocket);
        NetDebugPrintAddress(Address);
        RtlDebugPrint(" on link %08x: %x.\n", Link, Status);
    }

    return Status;
}

KSTATUS
NetListen (
    PSOCKET Socket,
    ULONG BacklogCount
    )

/*++

Routine Description:

    This routine adds a bound socket to the list of listening sockets,
    officially allowing sockets to attempt to connect to it.

Arguments:

    Socket - Supplies a pointer to the socket to mark as listening.

    BacklogCount - Supplies the number of attempted connections that can be
        queued before additional connections are refused.

Return Value:

    Status code.

--*/

{

    NETWORK_ADDRESS LocalAddress;
    PNET_SOCKET NetSocket;
    KSTATUS Status;

    NetSocket = (PNET_SOCKET)Socket;
    if ((BacklogCount == 0) || (BacklogCount > NET_MAX_INCOMING_CONNECTIONS)) {
        BacklogCount = NET_MAX_INCOMING_CONNECTIONS;
    }

    NetSocket->MaxIncomingConnections = BacklogCount;

    //
    // If the socket is not yet bound, bind it to any address and a random
    // port.
    //

    if (NetSocket->BindingType == SocketBindingInvalid) {
        RtlZeroMemory(&LocalAddress, sizeof(NETWORK_ADDRESS));
        LocalAddress.Domain = NetSocket->Network->Domain;
        Status = NetBindToAddress(Socket, NULL, &LocalAddress);
        if (!KSUCCESS(Status)) {
            if (NetGlobalDebug != FALSE) {
                RtlDebugPrint("Net: Socket %x "
                              "Implicit bind in listen failed: %x\n",
                              Status);
            }

            return Status;
        }
    }

    Status = NetSocket->Protocol->Interface.Listen(NetSocket);
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Socket %x Listen %d: %x\n",
                      NetSocket,
                      BacklogCount,
                      Status);
    }

    return Status;
}

KSTATUS
NetAccept (
    PSOCKET Socket,
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

    PNET_SOCKET NetSocket;
    KSTATUS Status;

    NetSocket = (PNET_SOCKET)Socket;
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Socket %x accept...\n", NetSocket);
    }

    Status = NetSocket->Protocol->Interface.Accept(NetSocket,
                                                   NewConnectionSocket,
                                                   RemoteAddress);

    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Socket %x accepted ", NetSocket);
        if (RemoteAddress != NULL) {
            NetDebugPrintAddress(RemoteAddress);
        }

        RtlDebugPrint("\n");
    }

    return Status;
}

KSTATUS
NetConnect (
    PSOCKET Socket,
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

    PNET_SOCKET NetSocket;
    KSTATUS Status;

    NetSocket = (PNET_SOCKET)Socket;
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Connecting socket %x to ", NetSocket);
        NetDebugPrintAddress(Address);
        RtlDebugPrint("...\n");
    }

    Status = NetSocket->Protocol->Interface.Connect(NetSocket, Address);
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Connect socket %x to ", NetSocket);
        NetDebugPrintAddress(Address);
        RtlDebugPrint(" : %x\n", Status);
    }

    return Status;
}

KSTATUS
NetCloseSocket (
    PSOCKET Socket
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

    PNET_SOCKET NetSocket;

    NetSocket = (PNET_SOCKET)Socket;
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Close %x\n", NetSocket);
    }

    return NetSocket->Protocol->Interface.Close(NetSocket);
}

KSTATUS
NetSendData (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine sends the given data buffer through the network.

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

    PNET_SOCKET NetSocket;
    KSTATUS Status;

    NetSocket = (PNET_SOCKET)Socket;
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Sending %ld on socket %x...\n",
                      Parameters->Size,
                      NetSocket);
    }

    Status = NetSocket->Protocol->Interface.Send(FromKernelMode,
                                                 NetSocket,
                                                 Parameters,
                                                 IoBuffer);

    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Sent %ld on socket %x: %x.\n",
                      Parameters->Size,
                      NetSocket,
                      Status);
    }

    return Status;
}

KSTATUS
NetReceiveData (
    BOOL FromKernelMode,
    PSOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine is called by the user to receive data from the socket.

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

    PNET_SOCKET NetSocket;
    KSTATUS Status;

    NetSocket = (PNET_SOCKET)Socket;
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Receiving %ld on socket %x...\n",
                      Parameters->Size,
                      NetSocket);
    }

    Status = NetSocket->Protocol->Interface.Receive(FromKernelMode,
                                                    NetSocket,
                                                    Parameters,
                                                    IoBuffer);

    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Received %ld on socket %x: %x.\n",
                      Parameters->Size,
                      NetSocket,
                      Status);
    }

    return Status;
}

KSTATUS
NetGetSetSocketInformation (
    PSOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN Option,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine returns information about the given socket.

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

    PNETWORK_ADDRESS AddressOption;
    SOCKET_BASIC_OPTION BasicOption;
    PBOOL BooleanOption;
    PNET_DOMAIN_TYPE Domain;
    PULONG ErrorOption;
    ULONG Flags;
    PNET_SOCKET NetSocket;
    PNET_PROTOCOL_ENTRY Protocol;
    KSTATUS Status;
    PNET_SOCKET_TYPE Type;

    NetSocket = (PNET_SOCKET)Socket;
    Protocol = NetSocket->Protocol;
    Status = STATUS_SUCCESS;

    //
    // If the requested option is something that this layer can answer, then
    // answer it.
    //

    switch (InformationType) {
    case SocketInformationTypeBasic:
        BasicOption = (SOCKET_BASIC_OPTION)Option;
        switch (BasicOption) {
        case SocketBasicOptionType:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
                break;
            }

            if (*DataSize < sizeof(NET_SOCKET_TYPE)) {
                *DataSize = sizeof(NET_SOCKET_TYPE);
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            Type = (PNET_SOCKET_TYPE)Data;
            *Type = NetSocket->KernelSocket.Type;
            *DataSize = sizeof(NET_SOCKET_TYPE);
            break;

        case SocketBasicOptionDomain:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
                break;
            }

            if (*DataSize < sizeof(NET_DOMAIN_TYPE)) {
                *DataSize = sizeof(NET_DOMAIN_TYPE);
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            Domain = (PNET_DOMAIN_TYPE)Data;
            *Domain = NetSocket->KernelSocket.Domain;
            *DataSize = sizeof(NET_DOMAIN_TYPE);
            break;

        case SocketBasicOptionLocalAddress:
        case SocketBasicOptionRemoteAddress:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
                break;
            }

            if (*DataSize < sizeof(NETWORK_ADDRESS)) {
                *DataSize = sizeof(NETWORK_ADDRESS);
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            //
            // Socket addresses are synchronized on the protocol's socket tree.
            // Acquire the lock to not return a torn address.
            //

            if (NetSocket->KernelSocket.Type == NetSocketRaw) {
                KeAcquireSharedExclusiveLockShared(NetRawSocketsLock);

            } else {
                KeAcquireSharedExclusiveLockShared(Protocol->SocketLock);
            }

            AddressOption = Data;
            if (BasicOption == SocketBasicOptionLocalAddress) {
                RtlCopyMemory(AddressOption,
                              &(NetSocket->LocalAddress),
                              sizeof(NETWORK_ADDRESS));

                //
                // Even if the local address is not yet initialized, return the
                // any address and any port on the correct network.
                //

                if (AddressOption->Domain == NetDomainInvalid) {
                    AddressOption->Domain = NetSocket->KernelSocket.Domain;
                }

                if (NetSocket->KernelSocket.Type == NetSocketRaw) {
                    AddressOption->Port = NetSocket->KernelSocket.Protocol;
                }

            } else {

                ASSERT(BasicOption == SocketBasicOptionRemoteAddress);

                RtlCopyMemory(AddressOption,
                              &(NetSocket->RemoteAddress),
                              sizeof(NETWORK_ADDRESS));
            }

            if (NetSocket->KernelSocket.Type == NetSocketRaw) {
                KeReleaseSharedExclusiveLockShared(NetRawSocketsLock);

            } else {
                KeReleaseSharedExclusiveLockShared(Protocol->SocketLock);
            }

            *DataSize = sizeof(NETWORK_ADDRESS);
            break;

        case SocketBasicOptionReuseAnyAddress:
            if (Set != FALSE) {
                if (*DataSize != sizeof(BOOL)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                if (*BooleanOption != FALSE) {
                    RtlAtomicOr32(&(NetSocket->Flags),
                                  NET_SOCKET_FLAG_REUSE_ANY_ADDRESS);

                } else {
                    RtlAtomicAnd32(&(NetSocket->Flags),
                                   ~NET_SOCKET_FLAG_REUSE_ANY_ADDRESS);
                }

            } else {
                if (*DataSize < sizeof(BOOL)) {
                    *DataSize = sizeof(BOOL);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                Flags = NetSocket->Flags;
                if ((Flags & NET_SOCKET_FLAG_REUSE_ANY_ADDRESS) != 0) {
                    *BooleanOption = TRUE;

                } else {
                    *BooleanOption = FALSE;
                }
            }

            break;

        case SocketBasicOptionReuseTimeWait:
            if (Set != FALSE) {
                if (*DataSize != sizeof(BOOL)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                if (*BooleanOption != FALSE) {
                    RtlAtomicOr32(&(NetSocket->Flags),
                                  NET_SOCKET_FLAG_REUSE_TIME_WAIT);

                } else {
                    RtlAtomicAnd32(&(NetSocket->Flags),
                                   ~NET_SOCKET_FLAG_REUSE_TIME_WAIT);
                }

            } else {
                if (*DataSize < sizeof(BOOL)) {
                    *DataSize = sizeof(BOOL);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                if ((NetSocket->Flags & NET_SOCKET_FLAG_REUSE_TIME_WAIT) != 0) {
                    *BooleanOption = TRUE;

                } else {
                    *BooleanOption = FALSE;
                }
            }

            break;

        case SocketBasicOptionReuseExactAddress:
            if (Set != FALSE) {
                if (*DataSize != sizeof(BOOL)) {
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                if (*BooleanOption != FALSE) {
                    RtlAtomicOr32(&(NetSocket->Flags),
                                  NET_SOCKET_FLAG_REUSE_EXACT_ADDRESS);

                } else {
                    RtlAtomicAnd32(&(NetSocket->Flags),
                                   ~NET_SOCKET_FLAG_REUSE_EXACT_ADDRESS);
                }

            } else {
                if (*DataSize < sizeof(BOOL)) {
                    *DataSize = sizeof(BOOL);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                BooleanOption = (PBOOL)Data;
                Flags = NetSocket->Flags;
                if ((Flags & NET_SOCKET_FLAG_REUSE_EXACT_ADDRESS) != 0) {
                    *BooleanOption = TRUE;

                } else {
                    *BooleanOption = FALSE;
                }
            }

            break;

        case SocketBasicOptionErrorStatus:
            if (Set != FALSE) {
                Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;

            } else {
                if (*DataSize < sizeof(KSTATUS)) {
                    *DataSize = sizeof(KSTATUS);
                    Status = STATUS_BUFFER_TOO_SMALL;
                    break;
                }

                ASSERT(sizeof(KSTATUS) == sizeof(ULONG));

                ErrorOption = (PULONG)Data;
                *ErrorOption = NET_SOCKET_GET_AND_CLEAR_LAST_ERROR(NetSocket);
            }

            break;

        case SocketBasicOptionDebug:
        case SocketBasicOptionInlineOutOfBand:
        case SocketBasicOptionRoutingDisabled:

            //
            // TODO: Implement network routing and urgent messages.
            //

            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;

        default:

            //
            // Send the call down to the protocol layer, which can handle some
            // of the basic options as well.
            //

            Status = NetSocket->Protocol->Interface.GetSetInformation(
                                                               NetSocket,
                                                               InformationType,
                                                               Option,
                                                               Data,
                                                               DataSize,
                                                               Set);

            if (!KSUCCESS(Status)) {
                goto GetSetSocketInformationEnd;
            }

            break;
        }

        break;

    //
    // IPv4 and IPv6 socket information is handled at the network layer.
    //

    case SocketInformationTypeIp4:
    case SocketInformationTypeIp6:
    case SocketInformationTypeNetlink:
        Status = NetSocket->Network->Interface.GetSetInformation(
                                                               NetSocket,
                                                               InformationType,
                                                               Option,
                                                               Data,
                                                               DataSize,
                                                               Set);

        if (!KSUCCESS(Status)) {
            goto GetSetSocketInformationEnd;
        }

        break;

    //
    // TCP and UDP socket information is handled at the protocol layer.
    //

    case SocketInformationTypeTcp:
    case SocketInformationTypeUdp:
    case SocketInformationTypeNetlinkGeneric:
        Status = NetSocket->Protocol->Interface.GetSetInformation(
                                                               NetSocket,
                                                               InformationType,
                                                               Option,
                                                               Data,
                                                               DataSize,
                                                               Set);

        if (!KSUCCESS(Status)) {
            goto GetSetSocketInformationEnd;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto GetSetSocketInformationEnd;
    }

GetSetSocketInformationEnd:
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: GetSetSocketInformation on socket %x: %x.\n",
                      NetSocket,
                      Status);
    }

    return Status;
}

KSTATUS
NetShutdown (
    PSOCKET Socket,
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

    PNET_SOCKET NetSocket;
    KSTATUS Status;

    NetSocket = (PNET_SOCKET)Socket;
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Shutdown socket %x, %d...\n",
                      NetSocket,
                      ShutdownType);
    }

    Status = NetSocket->Protocol->Interface.Shutdown(NetSocket, ShutdownType);
    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: Shutdown socket %x, %d: %x\n",
                      NetSocket,
                      ShutdownType,
                      Status);
    }

    return Status;
}

KSTATUS
NetUserControl (
    PSOCKET Socket,
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

    PNET_SOCKET NetSocket;
    KSTATUS Status;

    NetSocket = (PNET_SOCKET)Socket;
    Status = NetSocket->Protocol->Interface.UserControl(NetSocket,
                                                        CodeNumber,
                                                        FromKernelMode,
                                                        ContextBuffer,
                                                        ContextBufferSize);

    if (NetGlobalDebug != FALSE) {
        RtlDebugPrint("Net: UserControl socket %x, %d: %x\n",
                      NetSocket,
                      CodeNumber,
                      Status);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpDestroyProtocol (
    PNET_PROTOCOL_ENTRY Protocol
    )

/*++

Routine Description:

    This routine destroys the given protocol and all its resources.

Arguments:

    Protocol - Supplies a pointer to the protocol to destroy.

Return Value:

    None.

--*/

{

    if (Protocol->SocketLock != NULL) {
        KeDestroySharedExclusiveLock(Protocol->SocketLock);
    }

    MmFreePagedPool(Protocol);
    return;
}

