/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

//
// ---------------------------------------------------------------- Definitions
//

//
// Printed strings of ethernet addresses look something like:
// "12:34:56:78:9A:BC". Include the null terminator.
//

#define NET80211_ADDRESS_STRING_LENGTH 18

//
// Define the test SSID and its passphrase.
//

#define NET80211_TEST_SSID "mtest"
#define NET80211_TEST_SSID_LENGTH RtlStringLength(NET80211_TEST_SSID)
#define NET80211_TEST_PASSPHRASE "minocatest"
#define NET80211_TEST_PASSPHRASE_LENGTH \
    RtlStringLength(NET80211_TEST_PASSPHRASE)

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
    PNET_LINK Link,
    PNET_PACKET_LIST PacketList,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    );

VOID
Net80211pProcessReceivedPacket (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

VOID
Net80211pGetBroadcastAddress (
    PNETWORK_ADDRESS PhysicalNetworkAddress
    );

ULONG
Net80211pPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

VOID
Net80211pGetPacketSizeInformation (
    PNET_LINK Link,
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation
    );

VOID
Net80211pDestroy80211Link (
    PNET80211_LINK Net80211Link
    );

//
// -------------------------------------------------------------------- Globals
//

HANDLE Net80211DataLinkLayerHandle = INVALID_HANDLE;

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

    DataLinkEntry.Type = NetDataLink80211;
    Interface = &(DataLinkEntry.Interface);
    Interface->InitializeLink = Net80211pInitializeLink;
    Interface->DestroyLink = Net80211pDestroyLink;
    Interface->Send = Net80211pSend;
    Interface->ProcessReceivedPacket = Net80211pProcessReceivedPacket;
    Interface->GetBroadcastAddress = Net80211pGetBroadcastAddress;
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
Net80211InitializeLink (
    PNET_LINK Link,
    PNET80211_LINK_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine initializes the networking core link for use by the 802.11
    core. The link starts disassociated from any BSS and must be started first
    before it can join a BSS.

Arguments:

    Link - Supplies a pointer to the network link that was created for the
        802.11 device by the networking core.

    Properties - Supplies a pointer describing the properties and interface of
        the 802.11 link. This memory will not be referenced after the function
        returns, so this may be a stack allocated structure.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PNET80211_LINK Net80211Link;
    PNET80211_RATE_INFORMATION Rates;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (Link->Properties.DataLinkType != NetDataLink80211) {
        Status = STATUS_INVALID_PARAMETER;
        goto InitializeLinkEnd;
    }

    if (Properties->Version < NET_LINK_PROPERTIES_VERSION) {
        Status = STATUS_VERSION_MISMATCH;
        goto InitializeLinkEnd;
    }

    Net80211Link = Link->DataLinkContext;
    if (Net80211Link == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto InitializeLinkEnd;
    }

    //
    // Copy the properties, except the pointer to the supported rates.
    //

    RtlCopyMemory(&(Net80211Link->Properties),
                  Properties,
                  sizeof(NET80211_LINK_PROPERTIES));

    Net80211Link->Properties.SupportedRates = NULL;

    //
    // All supported station modes currently set the ESS capability.
    //

    Net80211Link->Properties.Capabilities |= NET80211_CAPABILITY_FLAG_ESS;

    //
    // The rate information has a dynamic length, so it needs to be reallocated
    // and copied.
    //

    AllocationSize = sizeof(NET80211_RATE_INFORMATION) +
                     (Properties->SupportedRates->Count * sizeof(UCHAR));

    Rates = MmAllocatePagedPool(AllocationSize, NET80211_ALLOCATION_TAG);
    if (Rates == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeLinkEnd;
    }

    RtlCopyMemory(Rates,
                  Properties->SupportedRates,
                  sizeof(NET80211_RATE_INFORMATION));

    Rates->Rate = (PUCHAR)(Rates + 1);
    RtlCopyMemory(Rates->Rate,
                  Properties->SupportedRates->Rate,
                  Rates->Count * sizeof(UCHAR));

    Net80211Link->Properties.SupportedRates = Rates;
    Status = STATUS_SUCCESS;

InitializeLinkEnd:
    return Status;
}

NET80211_API
KSTATUS
Net80211StartLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine is called when an 802.11 link is fully set up and ready to
    send and receive frames. It will kick off the background task of
    associating with the default BSS, determine the link's data rate and bring
    the link to the point at which it can begin handling socket traffic. As a
    result, there is no need for an 802.11 device driver to set the link state
    to "up". This routine must be called at low level.

Arguments:

    Link - Supplies a pointer to the link to start.

Return Value:

    Status code.

--*/

{

    PNET80211_LINK Net80211Link;
    NET80211_SCAN_STATE Scan;

    Net80211Link = Link->DataLinkContext;
    Net80211Link->State = Net80211StateInitialized;
    RtlZeroMemory(&Scan, sizeof(NET80211_SCAN_STATE));
    Scan.Link = Link;
    Scan.Flags = NET80211_SCAN_FLAG_JOIN | NET80211_SCAN_FLAG_BROADCAST;
    Scan.SsidLength = NET80211_TEST_SSID_LENGTH;
    RtlCopyMemory(Scan.Ssid, NET80211_TEST_SSID, NET80211_TEST_SSID_LENGTH);
    Scan.PassphraseLength = NET80211_TEST_PASSPHRASE_LENGTH;
    RtlCopyMemory(Scan.Passphrase,
                  NET80211_TEST_PASSPHRASE,
                  NET80211_TEST_PASSPHRASE_LENGTH);

    return Net80211pStartScan(Link, &Scan);
}

NET80211_API
VOID
Net80211StopLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine is called when an 802.11 link has gone down and is no longer
    able to send or receive frames. This will update the link state and reset
    the 802.11 core in preparation for a subsequence start request.

Arguments:

    Link - Supplies a pointer to the link to stop.

Return Value:

    None.

--*/

{

    PNET80211_LINK Net80211Link;

    Net80211Link = Link->DataLinkContext;
    Net80211Link->State = Net80211StateUninitialized;
    NetSetLinkState(Link, FALSE, NET_SPEED_NONE);
    return;
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
    Net80211Link->Lock = KeCreateQueuedLock();
    if (Net80211Link->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeLinkEnd;
    }

    Net80211Link->State = Net80211StateUninitialized;
    INITIALIZE_LIST_HEAD(&(Net80211Link->BssList));
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
    PNET_LINK Link,
    PNET_PACKET_LIST PacketList,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    )

/*++

Routine Description:

    This routine sends data through the data link layer and out the link.

Arguments:

    Link - Supplies a pointer to the link on which to send the data.

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

    PNET80211_LINK Net80211Link;
    KSTATUS Status;

    //
    // Packets can only be sent if the link has associated with an access point.
    //

    Net80211Link = Link->DataLinkContext;
    if ((Net80211Link->State != Net80211StateAssociated) &&
        (Net80211Link->State != Net80211StateEncrypted)) {

        return STATUS_NOT_READY;
    }

    Status = Net80211pSendDataFrames(Link,
                                     PacketList,
                                     SourcePhysicalAddress,
                                     DestinationPhysicalAddress,
                                     ProtocolNumber);

    return Status;
}

VOID
Net80211pProcessReceivedPacket (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine is called to process a received ethernet packet.

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

    ULONG FrameType;
    PNET80211_FRAME_HEADER Header;

    //
    // Parse the the 802.11 header to determine the kind of packet.
    //

    Header = Packet->Buffer + Packet->DataOffset;
    FrameType = NET80211_GET_FRAME_TYPE(Header);
    switch (FrameType) {
    case NET80211_FRAME_TYPE_CONTROL:
        Net80211pProcessControlFrame(Link, Packet);
        break;

    case NET80211_FRAME_TYPE_DATA:
        Net80211pProcessDataFrame(Link, Packet);
        break;

    case NET80211_FRAME_TYPE_MANAGEMENT:
        Net80211pProcessManagementFrame(Link, Packet);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
Net80211pGetBroadcastAddress (
    PNETWORK_ADDRESS PhysicalNetworkAddress
    )

/*++

Routine Description:

    This routine gets the 802.11 broadcast address.

Arguments:

    PhysicalNetworkAddress - Supplies a pointer where the physical network
        broadcast address will be returned.

Return Value:

    None.

--*/

{

    ULONG ByteIndex;
    PUCHAR BytePointer;

    BytePointer = (PUCHAR)(PhysicalNetworkAddress->Address);
    RtlZeroMemory(BytePointer, sizeof(PhysicalNetworkAddress->Address));
    PhysicalNetworkAddress->Network = SocketNetworkPhysical80211;
    PhysicalNetworkAddress->Port = 0;
    for (ByteIndex = 0; ByteIndex < NET80211_ADDRESS_SIZE; ByteIndex += 1) {
        BytePointer[ByteIndex] = 0xFF;
    }

    return;
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

    ASSERT(Address->Network == SocketNetworkPhysical80211);

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
    PNET_LINK Link,
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation
    )

/*++

Routine Description:

    This routine gets the current packet size information for the given link.
    As the number of required headers can be different for each link, the
    packet size information is not a constant for an entire data link layer.

Arguments:

    Link - Supplies a pointer to the link whose packet size information is
        being queried.

    PacketSizeInformation - Supplies a pointer to a structure that receives the
        link's data link layer packet size information.

Return Value:

    None.

--*/

{

    PNET80211_LINK Net80211Link;

    Net80211Link = (PNET80211_LINK)Link->DataLinkContext;

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
    // If encryption is enable on the link, then there is an additional header
    // and an additional footer.
    //

    if (Net80211Link->State == Net80211StateEncrypted) {
        PacketSizeInformation->HeaderSize += sizeof(NET80211_CCMP_HEADER);
        PacketSizeInformation->FooterSize += NET80211_CCMP_MIC_SIZE;
    }

    PacketSizeInformation->MaxPacketSize = NET80211_MAX_DATA_FRAME_BODY_SIZE +
                                           PacketSizeInformation->HeaderSize +
                                           PacketSizeInformation->FooterSize;

    PacketSizeInformation->MinPacketSize = 0;
    return;
}

ULONG
Net80211pGetSequenceNumber (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine returns the sequence number to use for the given link.

Arguments:

    Link - Supplies a pointer to the link whose sequence number is requested.

Return Value:

    Returns the sequence number to use for the given link.

--*/

{

    PNET80211_LINK Net80211Link;
    ULONG SequenceNumber;

    Net80211Link = Link->DataLinkContext;
    SequenceNumber = RtlAtomicAdd32(&(Net80211Link->SequenceNumber), 1);
    return SequenceNumber;
}

KSTATUS
Net80211pSetChannel (
    PNET_LINK Link,
    ULONG Channel
    )

/*++

Routine Description:

    This routine sets the 802.11 link's channel to the given value.

Arguments:

    Link - Supplies a pointer to the link whose channel is being updated.

    Channel - Supplies the channel to which the link should be set.

Return Value:

    Status code.

--*/

{

    PVOID DriverContext;
    PNET80211_LINK Net80211Link;
    KSTATUS Status;

    Net80211Link = Link->DataLinkContext;
    DriverContext = Net80211Link->Properties.DriverContext;
    Status = Net80211Link->Properties.Interface.SetChannel(DriverContext,
                                                           Channel);

    return Status;
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

    if (Net80211Link->Properties.SupportedRates != NULL) {
        MmFreePagedPool(Net80211Link->Properties.SupportedRates);
    }

    if (Net80211Link->Lock != NULL) {
        KeDestroyQueuedLock(Net80211Link->Lock);
    }

    MmFreePagedPool(Net80211Link);
    return;
}

