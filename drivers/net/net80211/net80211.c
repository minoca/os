/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    net80211.c

Abstract:

    This module implements the 802.11 networking core library.

Author:

    Chris Stevens 19-Oct-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "net80211.h"
#include <minoca/net/ip4.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Printed strings of ethernet addresses look something like:
// "12:34:56:78:9A:BC". Include the null terminator.
//

#define NET80211_ADDRESS_STRING_LENGTH 18

//
// Define the IPv4 address mask for the bits that get included in a multicast
// MAC address.
//

#define NET80211_IP4_MULTICAST_TO_MAC_MASK CPU_TO_NETWORK32(0x007FFFFF)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
Net80211DriverUnload (
    PVOID Driver
    );

KSTATUS
Net80211pInitializeLink (
    PNET_LINK Link
    );

VOID
Net80211pDestroyLink (
    PNET_LINK Link
    );

KSTATUS
Net80211pSend (
    PVOID DataLinkContext,
    PNET_PACKET_LIST PacketList,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    );

VOID
Net80211pProcessReceivedPacket (
    PVOID DataLinkContext,
    PNET_PACKET_BUFFER Packet
    );

KSTATUS
Net80211pConvertToPhysicalAddress (
    PNETWORK_ADDRESS NetworkAddress,
    PNETWORK_ADDRESS PhysicalAddress,
    NET_ADDRESS_TYPE NetworkAddressType
    );

ULONG
Net80211pPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

VOID
Net80211pGetPacketSizeInformation (
    PVOID DataLinkContext,
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation,
    ULONG Flags
    );

VOID
Net80211pDestroy80211Link (
    PNET80211_LINK Net80211Link
    );

KSTATUS
Net80211pGetSetNetworkDeviceInformation (
    PNET80211_LINK Link,
    PNETWORK_80211_DEVICE_INFORMATION Information,
    BOOL Set
    );

//
// -------------------------------------------------------------------- Globals
//

HANDLE Net80211DataLinkLayerHandle = INVALID_HANDLE;
UUID Net80211NetworkDeviceInformationUuid =
    NETWORK_80211_DEVICE_INFORMATION_UUID;

//
// Stores the base MAC address for all IPv4 multicast addresses. The lower 23
// bits are taken from the lower 23-bits of the IPv4 address.
//

UCHAR Net80211Ip4MulticastBase[ETHERNET_ADDRESS_SIZE] =
    {0x01, 0x00, 0x5E, 0x00, 0x00, 0x00};

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine implements the initial entry point of the 802.11 core
    library, called when the library is first loaded.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    Status code.

--*/

{

    NET_DATA_LINK_ENTRY DataLinkEntry;
    HANDLE DataLinkHandle;
    DRIVER_FUNCTION_TABLE FunctionTable;
    PNET_DATA_LINK_INTERFACE Interface;
    KSTATUS Status;

    ASSERT(Net80211DataLinkLayerHandle == INVALID_HANDLE);

    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.Unload = Net80211DriverUnload;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Register the 802.11 data link layer with the networking core.
    //

    DataLinkEntry.Domain = NetDomain80211;
    Interface = &(DataLinkEntry.Interface);
    Interface->InitializeLink = Net80211pInitializeLink;
    Interface->DestroyLink = Net80211pDestroyLink;
    Interface->Send = Net80211pSend;
    Interface->ProcessReceivedPacket = Net80211pProcessReceivedPacket;
    Interface->ConvertToPhysicalAddress = Net80211pConvertToPhysicalAddress;
    Interface->PrintAddress = Net80211pPrintAddress;
    Interface->GetPacketSizeInformation = Net80211pGetPacketSizeInformation;
    Status = NetRegisterDataLinkLayer(&DataLinkEntry, &DataLinkHandle);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    Net80211DataLinkLayerHandle = DataLinkHandle;

    //
    // Initialize any built-in networks.
    //

    Status = Net80211pEapolInitialize();
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    Status = Net80211pNetlinkInitialize();
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

DriverEntryEnd:
    if (!KSUCCESS(Status)) {
        if (Net80211DataLinkLayerHandle != INVALID_HANDLE) {
            NetUnregisterDataLinkLayer(Net80211DataLinkLayerHandle);
            Net80211DataLinkLayerHandle = INVALID_HANDLE;
        }
    }

    return Status;
}

VOID
Net80211DriverUnload (
    PVOID Driver
    )

/*++

Routine Description:

    This routine is called before a driver is about to be unloaded from memory.
    The driver should take this opportunity to free any resources it may have
    set up in the driver entry routine.

Arguments:

    Driver - Supplies a pointer to the driver being torn down.

Return Value:

    None.

--*/

{

    //
    // Tear down built-in networks.
    //

    Net80211pNetlinkDestroy();
    Net80211pEapolDestroy();

    //
    // Unregister the 802.11 data link layer from the networking core.
    //

    if (Net80211DataLinkLayerHandle != INVALID_HANDLE) {
        NetUnregisterDataLinkLayer(Net80211DataLinkLayerHandle);
        Net80211DataLinkLayerHandle = INVALID_HANDLE;
    }

    return;
}

NET80211_API
KSTATUS
Net80211AddLink (
    PNET80211_LINK_PROPERTIES Properties,
    PNET80211_LINK *NewLink
    )

/*++

Routine Description:

    This routine adds the device link to the 802.11 networking core. The device
    must be ready to start sending and receiving 802.11 management frames in
    order to establish a BSS connection.

Arguments:

    Properties - Supplies a pointer describing the properties and interface of
        the 802.11 link. This memory will not be referenced after the function
        returns, so this may be a stack allocated structure.

    NewLink - Supplies a pointer where a pointer to the new 802.11 link will be
        returned on success.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PNET80211_LINK Link;
    NET_LINK_PROPERTIES NetProperties;
    PNET_LINK NetworkLink;
    PNET80211_RATE_INFORMATION Rates;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Link = NULL;
    if (Properties->Version < NET80211_LINK_PROPERTIES_VERSION) {
        Status = STATUS_VERSION_MISMATCH;
        goto AddLinkEnd;
    }

    //
    // Convert the 802.11 properties to the networking core properties and
    // add the networking core link. In order for this to work like the
    // data link layers built into the networking core (e.g. Ethernet) the
    // networking core routine will call 802.11 back to have it create it's
    // private context.
    //

    RtlZeroMemory(&NetProperties, sizeof(NET_LINK_PROPERTIES));
    NetProperties.Version = NET_LINK_PROPERTIES_VERSION;
    NetProperties.TransmitAlignment = Properties->TransmitAlignment;
    NetProperties.Device = Properties->Device;
    NetProperties.DeviceContext = Properties->DeviceContext;
    NetProperties.PacketSizeInformation = Properties->PacketSizeInformation;
    NetProperties.Capabilities = Properties->LinkCapabilities;
    NetProperties.DataLinkType = NetDomain80211;
    NetProperties.MaxPhysicalAddress = Properties->MaxPhysicalAddress;
    NetProperties.PhysicalAddress = Properties->PhysicalAddress;
    NetProperties.Interface.Send = Properties->Interface.Send;
    NetProperties.Interface.GetSetInformation =
                                       Properties->Interface.GetSetInformation;

    NetProperties.Interface.DestroyLink = Properties->Interface.DestroyLink;
    Status = NetAddLink(&NetProperties, &NetworkLink);
    if (!KSUCCESS(Status)) {
        goto AddLinkEnd;
    }

    ASSERT(NetworkLink->DataLinkContext != NULL);

    Link = (PNET80211_LINK)NetworkLink->DataLinkContext;

    //
    // Copy the properties, except the pointer to the supported rates.
    //

    RtlCopyMemory(&(Link->Properties),
                  Properties,
                  sizeof(NET80211_LINK_PROPERTIES));

    Link->Properties.SupportedRates = NULL;

    //
    // All supported station modes currently set the ESS capability.
    //

    Link->Properties.Net80211Capabilities |= NET80211_CAPABILITY_ESS;

    //
    // The rate information has a dynamic length, so it needs to be reallocated
    // and copied.
    //

    AllocationSize = sizeof(NET80211_RATE_INFORMATION) +
                     (Properties->SupportedRates->Count * sizeof(UCHAR));

    Rates = MmAllocatePagedPool(AllocationSize, NET80211_ALLOCATION_TAG);
    if (Rates == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddLinkEnd;
    }

    Rates->Count = Properties->SupportedRates->Count;
    Rates->Rate = (PUCHAR)(Rates + 1);
    RtlCopyMemory(Rates->Rate,
                  Properties->SupportedRates->Rate,
                  Rates->Count * sizeof(UCHAR));

    Link->Properties.SupportedRates = Rates;

    //
    // All 802.11 network devices respond to 802.11 network device information
    // requests.
    //

    Status = IoRegisterDeviceInformation(Link->Properties.Device,
                                         &Net80211NetworkDeviceInformationUuid,
                                         TRUE);

    if (!KSUCCESS(Status)) {
        goto AddLinkEnd;
    }

    NetSetLinkState(Link->NetworkLink, FALSE, 0);
    *NewLink = Link;
    Status = STATUS_SUCCESS;

AddLinkEnd:
    if (!KSUCCESS(Status)) {
        if (Link != NULL) {
            Net80211RemoveLink(Link);
        }
    }

    return Status;
}

NET80211_API
VOID
Net80211RemoveLink (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine removes a link from the 802.11 core after its device has been
    removed. There may be outstanding references on the link, so the 802.11
    core will invoke the link destruction callback when all the references are
    released.

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
                                &Net80211NetworkDeviceInformationUuid,
                                FALSE);

    //
    // Remove the network link. When the last reference is released on the
    // network link it will call the data link destruction routine to destroy
    // the context.
    //

    Net80211pSetState(Link, Net80211StateUninitialized);
    NetRemoveLink(Link->NetworkLink);
    Net80211LinkReleaseReference(Link);
    return;
}

NET80211_API
VOID
Net80211LinkAddReference (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine increases the reference count on a 802.11 link.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose reference count
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

NET80211_API
VOID
Net80211LinkReleaseReference (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine decreases the reference count of a 802.11 link, and destroys
    the link if the reference count drops to zero.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose reference count
        should be decremented.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Link->ReferenceCount), -1);

    ASSERT(OldReferenceCount != 0);

    //
    // Since the 802.11 link is owned by the network link. It cannot and should
    // not actually be destroyed until the network link's last reference goes
    // away. So release the initial reference taken on the network link.
    //

    if (OldReferenceCount == 1) {
        NetLinkReleaseReference(Link->NetworkLink);
    }

    return;
}

NET80211_API
VOID
Net80211ProcessReceivedPacket (
    PNET80211_LINK Link,
    PNET80211_RECEIVE_PACKET Packet
    )

/*++

Routine Description:

    This routine is called by the low level WiFi driver to pass received
    packets onto the 802.11 core networking library for dispatching.

Arguments:

    Link - Supplies a pointer to the 802.11 link that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure and the network packet it contains may be used as a
        scratch space while this routine executes and the packet travels up the
        stack, but will not be accessed after this routine returns.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    PNET80211_FRAME_HEADER Header;

    NetProcessReceivedPacket(Link->NetworkLink, Packet->NetPacket);

    //
    // Update the RSSI for the BSS that sent the packet. If this station is
    // associated (i.e. has an active BSS) and is not scanning, then assume the
    // packet came from the associated BSS.
    //

    KeAcquireQueuedLock(Link->Lock);
    if ((Link->ActiveBss != NULL) &&
        ((Link->Flags & NET80211_LINK_FLAG_SCANNING) == 0)) {

        Bss = Link->ActiveBss;

    //
    // Otherwise search the list of BSS's and update the one with the matching
    // ID.
    //

    } else {
        Header = Packet->NetPacket->Buffer + Packet->NetPacket->DataOffset;
        Bss = Net80211pLookupBssEntry(Link, Header->Address2);
    }

    if (Bss != NULL) {
        Bss->State.Rssi = Packet->Rssi;
    }

    KeReleaseQueuedLock(Link->Lock);
    return;
}

NET80211_API
KSTATUS
Net80211GetSetLinkDeviceInformation (
    PNET80211_LINK Link,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets device information for an 802.11 link.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose device information is
        being retrieved or set.

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
    if (RtlAreUuidsEqual(Uuid, &Net80211NetworkDeviceInformationUuid)) {
        if (*DataSize < sizeof(NETWORK_80211_DEVICE_INFORMATION)) {
            *DataSize = sizeof(NETWORK_80211_DEVICE_INFORMATION);
            goto GetSetLinkDeviceInformationEnd;
        }

        *DataSize = sizeof(NETWORK_80211_DEVICE_INFORMATION);
        Status = Net80211pGetSetNetworkDeviceInformation(Link, Data, Set);
        goto GetSetLinkDeviceInformationEnd;

    } else {
        Status = NetGetSetLinkDeviceInformation(Link->NetworkLink,
                                                Uuid,
                                                Data,
                                                DataSize,
                                                Set);

        goto GetSetLinkDeviceInformationEnd;
    }

GetSetLinkDeviceInformationEnd:
    return Status;
}

KSTATUS
Net80211pInitializeLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine initializes any pieces of information needed by the data link
    layer for a new link.

Arguments:

    Link - Supplies a pointer to the new link.

Return Value:

    Status code.

--*/

{

    PNET80211_LINK Net80211Link;
    KSTATUS Status;

    Net80211Link = MmAllocatePagedPool(sizeof(NET80211_LINK),
                                       NET80211_ALLOCATION_TAG);

    if (Net80211Link == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeLinkEnd;
    }

    RtlZeroMemory(Net80211Link, sizeof(NET80211_LINK));
    Net80211Link->ReferenceCount = 1;
    NET_INITIALIZE_PACKET_LIST(&(Net80211Link->PausedPacketList));
    Net80211Link->Lock = KeCreateQueuedLock();
    if (Net80211Link->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeLinkEnd;
    }

    Net80211Link->ScanLock = KeCreateQueuedLock();
    if (Net80211Link->ScanLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeLinkEnd;
    }

    Net80211Link->StateTimer = KeCreateTimer(NET80211_ALLOCATION_TAG);
    if (Net80211Link->StateTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeLinkEnd;
    }

    Net80211Link->TimeoutDpc = KeCreateDpc(Net80211pStateTimeoutDpcRoutine,
                                           Net80211Link);

    if (Net80211Link->TimeoutDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeLinkEnd;
    }

    Net80211Link->TimeoutWorkItem = KeCreateWorkItem(
                                                   NULL,
                                                   WorkPriorityNormal,
                                                   Net80211pStateTimeoutWorker,
                                                   Net80211Link,
                                                   NET80211_ALLOCATION_TAG);

    if (Net80211Link->TimeoutWorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeLinkEnd;
    }

    Net80211Link->State = Net80211StateUninitialized;
    INITIALIZE_LIST_HEAD(&(Net80211Link->BssList));
    NetLinkAddReference(Link);
    Net80211Link->NetworkLink = Link;
    Link->DataLinkContext = Net80211Link;
    Status = STATUS_SUCCESS;

InitializeLinkEnd:
    if (!KSUCCESS(Status)) {
        Net80211pDestroy80211Link(Net80211Link);
    }

    return Status;
}

VOID
Net80211pDestroyLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine allows the data link layer to tear down any state before a
    link is destroyed.

Arguments:

    Link - Supplies a pointer to the dying link.

Return Value:

    None.

--*/

{

    if (Link->DataLinkContext != NULL) {
        Net80211pDestroy80211Link(Link->DataLinkContext);
        Link->DataLinkContext = NULL;
    }

    return;
}

KSTATUS
Net80211pSend (
    PVOID DataLinkContext,
    PNET_PACKET_LIST PacketList,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    )

/*++

Routine Description:

    This routine sends data through the data link layer and out the link.

Arguments:

    DataLinkContext - Supplies a pointer to the data link context for the
        link on which to send the data.

    PacketList - Supplies a pointer to a list of network packets to send. Data
        in these packets may be modified by this routine, but must not be used
        once this routine returns.

    SourcePhysicalAddress - Supplies a pointer to the source (local) physical
        network address.

    DestinationPhysicalAddress - Supplies the optional physical address of the
        destination, or at least the next hop. If NULL is provided, then the
        packets will be sent to the data link layer's broadcast address.

    ProtocolNumber - Supplies the protocol number of the data inside the data
        link header.

Return Value:

    Status code.

--*/

{

    PNET80211_LINK Link;
    KSTATUS Status;

    Link = (PNET80211_LINK)DataLinkContext;
    Status = Net80211pSendDataFrames(Link,
                                     PacketList,
                                     SourcePhysicalAddress,
                                     DestinationPhysicalAddress,
                                     ProtocolNumber);

    return Status;
}

VOID
Net80211pProcessReceivedPacket (
    PVOID DataLinkContext,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine is called to process a received ethernet packet.

Arguments:

    DataLinkContext - Supplies a pointer to the data link context for the link
        that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    ULONG FrameType;
    PNET80211_FRAME_HEADER Header;
    PNET80211_LINK Link;

    //
    // Parse the the 802.11 header to determine the kind of packet.
    //

    Link = (PNET80211_LINK)DataLinkContext;
    Header = Packet->Buffer + Packet->DataOffset;
    FrameType = NET80211_GET_FRAME_TYPE(Header);
    switch (FrameType) {
    case NET80211_FRAME_TYPE_DATA:
        Net80211pProcessDataFrame(Link, Packet);
        break;

    case NET80211_FRAME_TYPE_MANAGEMENT:
        Net80211pProcessManagementFrame(Link, Packet);
        break;

    case NET80211_FRAME_TYPE_CONTROL:
        Net80211pProcessControlFrame(Link, Packet);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
Net80211pConvertToPhysicalAddress (
    PNETWORK_ADDRESS NetworkAddress,
    PNETWORK_ADDRESS PhysicalAddress,
    NET_ADDRESS_TYPE NetworkAddressType
    )

/*++

Routine Description:

    This routine converts the given network address to a physical layer address
    based on the provided network address type.

Arguments:

    NetworkAddress - Supplies a pointer to the network layer address to convert.

    PhysicalAddress - Supplies a pointer to an address that receives the
        converted physical layer address.

    NetworkAddressType - Supplies the classified type of the given network
        address, which aids in conversion.

Return Value:

    Status code.

--*/

{

    ULONG ByteIndex;
    PUCHAR BytePointer;
    ULONG Ip4AddressMask;
    PUCHAR Ip4BytePointer;
    PIP4_ADDRESS Ip4Multicast;
    KSTATUS Status;

    BytePointer = (PUCHAR)(PhysicalAddress->Address);
    RtlZeroMemory(BytePointer, sizeof(PhysicalAddress->Address));
    PhysicalAddress->Domain = NetDomainEthernet;
    PhysicalAddress->Port = 0;
    Status = STATUS_SUCCESS;
    switch (NetworkAddressType) {

    //
    // The broadcast address is the same for all network addresses.
    //

    case NetAddressBroadcast:
        for (ByteIndex = 0; ByteIndex < ETHERNET_ADDRESS_SIZE; ByteIndex += 1) {
            BytePointer[ByteIndex] = 0xFF;
        }

        break;

    //
    // A multicast MAC address depends on the domain of the given network
    // address. This conversion is done at the physical layer because the
    // network layer shouldn't need to know anything about the underlying
    // physical layer and the conversion algorithm is specific the the physical
    // layer's address type.
    //

    case NetAddressMulticast:
        switch (NetworkAddress->Domain) {
        case NetDomainIp4:

            //
            // The IPv4 address is in network byte order, but the CPU byte
            // order low 23-bits need to be added to the MAC address. Get the
            // low bytes, but keep them in network order to avoid doing a swap.
            //

            Ip4Multicast = (PIP4_ADDRESS)NetworkAddress;
            Ip4AddressMask = Ip4Multicast->Address &
                             NET80211_IP4_MULTICAST_TO_MAC_MASK;

            //
            // Copy the static base MAC address.
            //

            RtlCopyMemory(BytePointer,
                          Net80211Ip4MulticastBase,
                          ETHERNET_ADDRESS_SIZE);

            //
            // Add the low 23-bits from the IP address to the MAC address,
            // keeping in mind that the IP bytes are in network order.
            //

            Ip4BytePointer = (PUCHAR)&Ip4AddressMask;
            BytePointer[3] |= Ip4BytePointer[1];
            BytePointer[4] = Ip4BytePointer[2];
            BytePointer[5] = Ip4BytePointer[3];
            break;

        default:
            Status = STATUS_NOT_SUPPORTED;
            break;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}

ULONG
Net80211pPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    )

/*++

Routine Description:

    This routine is called to convert a network address into a string, or
    determine the length of the buffer needed to convert an address into a
    string.

Arguments:

    Address - Supplies an optional pointer to a network address to convert to
        a string.

    Buffer - Supplies an optional pointer where the string representation of
        the address will be returned.

    BufferLength - Supplies the length of the supplied buffer, in bytes.

Return Value:

    Returns the maximum length of any address if no network address is
    supplied.

    Returns the actual length of the network address string if a network address
    was supplied, including the null terminator.

--*/

{

    PUCHAR BytePointer;
    ULONG Length;

    if (Address == NULL) {
        return NET80211_ADDRESS_STRING_LENGTH;
    }

    ASSERT(Address->Domain == NetDomain80211);

    BytePointer = (PUCHAR)(Address->Address);
    Length = RtlPrintToString(Buffer,
                              BufferLength,
                              CharacterEncodingAscii,
                              "%02X:%02X:%02X:%02X:%02X:%02X",
                              BytePointer[0],
                              BytePointer[1],
                              BytePointer[2],
                              BytePointer[3],
                              BytePointer[4],
                              BytePointer[5]);

    return Length;
}

VOID
Net80211pGetPacketSizeInformation (
    PVOID DataLinkContext,
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation,
    ULONG Flags
    )

/*++

Routine Description:

    This routine gets the current packet size information for the given link.
    As the number of required headers can be different for each link, the
    packet size information is not a constant for an entire data link layer.

Arguments:

    DataLinkContext - Supplies a pointer to the data link context of the link
        whose packet size information is being queried.

    PacketSizeInformation - Supplies a pointer to a structure that receives the
        link's data link layer packet size information.

    Flags - Supplies a bitmask of flags indicating which packet size
        information is desired. See NET_PACKET_SIZE_FLAG_* for definitions.

Return Value:

    None.

--*/

{

    PNET80211_BSS_ENTRY Bss;
    PNET80211_LINK Link;

    Link = (PNET80211_LINK)DataLinkContext;

    //
    // The header size depends on whether QoS is implemented. If QoS is not
    // implemented, then the header size is always the same. If QoS is
    // implemented, it depends on whether or not the other station implements
    // QoS.
    //

    PacketSizeInformation->HeaderSize = sizeof(NET80211_DATA_FRAME_HEADER) +
                                        sizeof(NET8022_LLC_HEADER) +
                                        sizeof(NET8022_SNAP_EXTENSION);

    PacketSizeInformation->FooterSize = 0;

    //
    // If encryption is required for the current BSS, then there is an
    // additional header and an additional footer.
    //

    if ((Flags & NET_PACKET_SIZE_FLAG_UNENCRYPTED) == 0) {
        Bss = Net80211pGetBss(Link);
        if (Bss != NULL) {
            if ((Bss->Encryption.Pairwise == NetworkEncryptionWpa2Eap) ||
                (Bss->Encryption.Pairwise == NetworkEncryptionWpa2Psk)) {

                PacketSizeInformation->FooterSize += NET80211_CCMP_MIC_SIZE;
                PacketSizeInformation->HeaderSize +=
                                                  sizeof(NET80211_CCMP_HEADER);
            }

            Net80211pBssEntryReleaseReference(Bss);
        }
    }

    PacketSizeInformation->MaxPacketSize = NET80211_MAX_DATA_FRAME_BODY_SIZE +
                                           PacketSizeInformation->HeaderSize +
                                           PacketSizeInformation->FooterSize;

    PacketSizeInformation->MinPacketSize = 0;
    return;
}

ULONG
Net80211pGetSequenceNumber (
    PNET80211_LINK Link
    )

/*++

Routine Description:

    This routine returns the sequence number to use for the given link.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose sequence number is
        requested.

Return Value:

    Returns the sequence number to use for the given link.

--*/

{

    return RtlAtomicAdd32(&(Link->SequenceNumber), 1);
}

KSTATUS
Net80211pSetChannel (
    PNET80211_LINK Link,
    ULONG Channel
    )

/*++

Routine Description:

    This routine sets the 802.11 link's channel to the given value.

Arguments:

    Link - Supplies a pointer to the 802.11 link whose channel is being updated.

    Channel - Supplies the channel to which the link should be set.

Return Value:

    Status code.

--*/

{

    PVOID DeviceContext;

    DeviceContext = Link->Properties.DeviceContext;
    return Link->Properties.Interface.SetChannel(DeviceContext, Channel);
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
Net80211pDestroy80211Link (
    PNET80211_LINK Net80211Link
    )

/*++

Routine Description:

    This routine destroys the given 802.11 link structure.

Arguments:

    Net80211Link - Supplies a pointer to the 802.11 link to destroy.

Return Value:

    None.

--*/

{

    //
    // Cancel the timer at the 802.11 layer before destroying it. This will
    // make sure that any lingering state transition worker does not actually
    // perform a state transition.
    //

    if (Net80211Link->StateTimer != NULL) {
        KeAcquireQueuedLock(Net80211Link->Lock);
        Net80211pCancelStateTransitionTimer(Net80211Link);
        KeReleaseQueuedLock(Net80211Link->Lock);
        KeDestroyTimer(Net80211Link->StateTimer);
    }

    if (Net80211Link->TimeoutDpc != NULL) {
        KeDestroyDpc(Net80211Link->TimeoutDpc);
    }

    //
    // As the timeout work item acquires the link's lock, make sure to flush
    // out any lingering run of the work item before destroying the lock.
    //

    if (Net80211Link->TimeoutWorkItem != NULL) {
        KeFlushWorkItem(Net80211Link->TimeoutWorkItem);
        KeDestroyWorkItem(Net80211Link->TimeoutWorkItem);
    }

    if (Net80211Link->Properties.SupportedRates != NULL) {
        MmFreePagedPool(Net80211Link->Properties.SupportedRates);
    }

    if (Net80211Link->Lock != NULL) {
        KeDestroyQueuedLock(Net80211Link->Lock);
    }

    if (Net80211Link->ScanLock != NULL) {
        KeDestroyQueuedLock(Net80211Link->ScanLock);
    }

    MmFreePagedPool(Net80211Link);
    return;
}

KSTATUS
Net80211pGetSetNetworkDeviceInformation (
    PNET80211_LINK Link,
    PNETWORK_80211_DEVICE_INFORMATION Information,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets the 802.11 network device information for a
    particular link.

Arguments:

    Link - Supplies a pointer to the link to work with.

    Information - Supplies a pointer that either receives the device
        information, or contains the new information to set. For set operations,
        the information buffer will contain the current settings on return.

    Set - Supplies a boolean indicating if the information should be set or
        returned.

Return Value:

    Status code.

--*/

{

    ULONG SsidLength;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (Information->Version < NETWORK_80211_DEVICE_INFORMATION_VERSION) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Set != FALSE) {
        return STATUS_NOT_SUPPORTED;
    }

    Information->Flags = 0;
    RtlCopyMemory(&(Information->PhysicalAddress),
                  &(Link->Properties.PhysicalAddress),
                  sizeof(NETWORK_ADDRESS));

    KeAcquireQueuedLock(Link->Lock);
    if ((Link->State == Net80211StateAssociated) ||
        (Link->State == Net80211StateEncrypted)) {

        ASSERT(Link->ActiveBss != NULL);

        Information->Flags |= NETWORK_80211_DEVICE_FLAG_ASSOCIATED;
        Information->Bssid.Domain = NetDomain80211;
        Information->Bssid.Port = 0;
        RtlCopyMemory(Information->Bssid.Address,
                      Link->ActiveBss->State.Bssid,
                      NET80211_ADDRESS_SIZE);

        SsidLength = NET80211_GET_ELEMENT_LENGTH(Link->ActiveBss->Ssid);
        RtlCopyMemory(Information->Ssid,
                      NET80211_GET_ELEMENT_DATA(Link->ActiveBss->Ssid),
                      SsidLength);

        Information->Ssid[SsidLength] = STRING_TERMINATOR;
        Information->Channel = Link->ActiveBss->State.Channel;
        Information->MaxRate = Link->ActiveBss->State.MaxRate *
                               NET80211_RATE_UNIT;

        Information->Rssi = Link->ActiveBss->State.Rssi;
        Information->PairwiseEncryption = Link->ActiveBss->Encryption.Pairwise;
        Information->GroupEncryption = Link->ActiveBss->Encryption.Group;
    }

    KeReleaseQueuedLock(Link->Lock);
    return STATUS_SUCCESS;
}

