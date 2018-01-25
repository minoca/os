/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    arp.c

Abstract:

    This module implements support for the Address Resolution Protocol, which
    translates network layer addresses (such as IP addresses) to physical
    addresses (such as MAC addresses).

Author:

    Evan Green 5-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Network layer drivers are supposed to be able to stand on their own (i.e. be
// able to be implemented outside the core net library). For the builtin ones,
// avoid including netcore.h, but still redefine those functions that would
// otherwise generate imports.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/ip4.h>
#include <minoca/net/arp.h>

//
// ---------------------------------------------------------------- Definitions
//

#define ARP_HARDWARE_TYPE_ETHERNET 1

#define ARP_OPERATION_REQUEST      1
#define ARP_OPERATION_REPLY        2

//
// Define the packet size for Ethernet + IPv4 requests.
//

#define ARP_ETHERNET_IP4_SIZE      28

//
// Define the number of times to retry an address translation.
//

#define ARP_ADDRESS_TRANSLATION_RETRY_COUNT 3

//
// Define the amount of time to wait for an address translation to come back
// before retrying, in milliseconds.
//

#define ARP_ADDRESS_TRANSLATION_RETRY_INTERVAL MILLISECONDS_PER_SECOND

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the structure of an ARP packet. After the members of
    the structure comes the sender hardware address, sender protocol address,
    target hardware address, and optional target protocol address. The size of
    these fields depends on the lengths defined in the beginning of the packet.

Members:

    HardwareType - Stores the link protocol type. Ethernet is 1.

    ProtocolType - Stores the network protocol for which the ARP request is
        intended (an EtherType number). IPv4 is 0x0800.

    HardwareAddressLength - Stores the length of a hardware address. Ethernet
        addresses are 6 bytes.

    ProtocolAddressLength - Stores the length of the protocol address. IPv4
        addresses are 4 bytes.

    Operation - Stores the operation code for the ARP packet. 1 is request and
        2 is reply.

    SenderHardwareAddress - Stores the media address of the sender.

    SenderProtocolAddress - Stores the internetwork address of the sender.

    TargetHardwareAddress - Stores the media address of the intended receiver.

    TargetProtocolAddress - Storse the internetwork address of the intended
        receiver.
--*/

#pragma pack(push, 1)

typedef struct _ARP_PACKET {
    USHORT HardwareType;
    USHORT ProtocolType;
    UCHAR HardwareAddressLength;
    UCHAR ProtocolAddressLength;
    USHORT Operation;
} PACKED ARP_PACKET, *PARP_PACKET;

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpArpInitializeLink (
    PNET_LINK Link
    );

VOID
NetpArpDestroyLink (
    PNET_LINK Link
    );

VOID
NetpArpProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpArpGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN Option,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetpArpTranslateAddress (
    PNET_TRANSLATION_REQUEST Request
    );

KSTATUS
NetpArpSendRequest (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS QueryAddress
    );

KSTATUS
NetpArpSendReply (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS DestinationNetworkAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL NetArpDebug = FALSE;

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpArpInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for ARP packets.

Arguments:

    None.

Return Value:

    None.

--*/

{

    NET_NETWORK_ENTRY NetworkEntry;
    KSTATUS Status;

    if (NetArpDebug == FALSE) {
        NetArpDebug = NetGetGlobalDebugFlag();
    }

    //
    // Register the ARP handlers with the core networking library.
    //

    RtlZeroMemory(&NetworkEntry, sizeof(NET_NETWORK_ENTRY));
    NetworkEntry.Domain = NetDomainArp;
    NetworkEntry.ParentProtocolNumber = ARP_PROTOCOL_NUMBER;
    NetworkEntry.Interface.InitializeLink = NetpArpInitializeLink;
    NetworkEntry.Interface.DestroyLink = NetpArpDestroyLink;
    NetworkEntry.Interface.ProcessReceivedData = NetpArpProcessReceivedData;
    NetworkEntry.Interface.GetSetInformation = NetpArpGetSetInformation;
    Status = NetRegisterNetworkLayer(&NetworkEntry, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
NetpArpInitializeLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine initializes any pieces of information needed by the network
    layer for a new link.

Arguments:

    Link - Supplies a pointer to the new link.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

VOID
NetpArpDestroyLink (
    PNET_LINK Link
    )

/*++

Routine Description:

    This routine allows the network layer to tear down any state before a link
    is destroyed.

Arguments:

    Link - Supplies a pointer to the dying link.

Return Value:

    None.

--*/

{

    return;
}

VOID
NetpArpProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link and packet information.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    PARP_PACKET ArpPacket;
    PUCHAR CurrentPointer;
    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry;
    USHORT Operation;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketSize;
    NETWORK_ADDRESS SenderNetworkAddress;
    NETWORK_ADDRESS SenderPhysicalAddress;
    KSTATUS Status;
    NETWORK_ADDRESS TargetNetworkAddress;
    NETWORK_ADDRESS TargetPhysicalAddress;

    Packet = ReceiveContext->Packet;
    Link = ReceiveContext->Link;

    //
    // Skip packets that are too small.
    //

    ArpPacket = (PARP_PACKET)(Packet->Buffer + Packet->DataOffset);
    PacketSize = Packet->FooterOffset - Packet->DataOffset;
    if ((PacketSize < sizeof(ARP_PACKET)) ||
        (PacketSize <
         (sizeof(ARP_PACKET) + (2 * ArpPacket->ProtocolAddressLength) +
          (2 * ArpPacket->HardwareAddressLength)))) {

        return;
    }

    //
    // Skip packets that are not Ethernet + IPv4.
    //

    if ((NETWORK_TO_CPU16(ArpPacket->HardwareType) !=
         ARP_HARDWARE_TYPE_ETHERNET) ||
        (ArpPacket->HardwareAddressLength != ETHERNET_ADDRESS_SIZE)) {

        return;
    }

    if ((NETWORK_TO_CPU16(ArpPacket->ProtocolType) != IP4_PROTOCOL_NUMBER) ||
        (ArpPacket->ProtocolAddressLength != IP4_ADDRESS_SIZE)) {

        return;
    }

    //
    // Grab the sender and target network and physical addresses.
    //

    RtlZeroMemory(&SenderNetworkAddress, sizeof(NETWORK_ADDRESS));
    RtlZeroMemory(&SenderPhysicalAddress, sizeof(NETWORK_ADDRESS));
    RtlZeroMemory(&TargetNetworkAddress, sizeof(NETWORK_ADDRESS));
    RtlZeroMemory(&TargetPhysicalAddress, sizeof(NETWORK_ADDRESS));
    SenderPhysicalAddress.Domain = Link->DataLinkEntry->Domain;
    CurrentPointer = (PUCHAR)(ArpPacket + 1);
    RtlCopyMemory(&(SenderPhysicalAddress.Address),
                  CurrentPointer,
                  ETHERNET_ADDRESS_SIZE);

    CurrentPointer += ETHERNET_ADDRESS_SIZE;
    SenderNetworkAddress.Domain = NetDomainIp4;
    RtlCopyMemory(&(SenderNetworkAddress.Address),
                  CurrentPointer,
                  IP4_ADDRESS_SIZE);

    CurrentPointer += IP4_ADDRESS_SIZE;
    TargetPhysicalAddress.Domain = Link->DataLinkEntry->Domain;
    RtlCopyMemory(&(TargetPhysicalAddress.Address),
                  CurrentPointer,
                  ETHERNET_ADDRESS_SIZE);

    CurrentPointer += ETHERNET_ADDRESS_SIZE;
    TargetNetworkAddress.Domain = NetDomainIp4;
    RtlCopyMemory(&(TargetNetworkAddress.Address),
                  CurrentPointer,
                  IP4_ADDRESS_SIZE);

    Operation = NETWORK_TO_CPU16(ArpPacket->Operation);

    //
    // Handle request packets.
    //

    if (Operation == ARP_OPERATION_REQUEST) {
        if (NetArpDebug != FALSE) {
            RtlDebugPrint("ARP RX: Who has ");
            NetDebugPrintAddress(&TargetNetworkAddress);
            RtlDebugPrint("? Tell ");
            NetDebugPrintAddress(&SenderNetworkAddress);
            RtlDebugPrint(" (");
            NetDebugPrintAddress(&SenderPhysicalAddress);
            RtlDebugPrint(")\n");
        }

        Status = NetFindEntryForAddress(Link,
                                        &TargetNetworkAddress,
                                        &LinkAddressEntry);

        if (!KSUCCESS(Status)) {
            return;
        }

        //
        // Requests themselves are translations. Remember this translation.
        //

        NetAddAddressTranslation(Link,
                                 &SenderNetworkAddress,
                                 &SenderPhysicalAddress);

        NetpArpSendReply(Link,
                         LinkAddressEntry,
                         &SenderNetworkAddress,
                         &SenderPhysicalAddress);

        return;

    } else if (Operation != ARP_OPERATION_REPLY) {
        return;
    }

    //
    // Debug print the response.
    //

    if (NetArpDebug != FALSE) {
        RtlDebugPrint("ARP RX: ");
        NetDebugPrintAddress(&SenderNetworkAddress);
        RtlDebugPrint(" is at ");
        NetDebugPrintAddress(&SenderPhysicalAddress);
        RtlDebugPrint("\n");
    }

    //
    // Add the translation entry.
    //

    NetAddAddressTranslation(Link,
                             &SenderNetworkAddress,
                             &SenderPhysicalAddress);

    return;
}

KSTATUS
NetpArpGetSetInformation (
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

    Status code.

--*/

{

    SOCKET_ARP_OPTION ArpOption;
    UINTN RequiredSize;
    KSTATUS Status;
    PNET_TRANSLATION_REQUEST TranslationRequest;

    if (InformationType != SocketInformationArp) {
        Status = STATUS_INVALID_PARAMETER;
        goto ArpGetSetInformationEnd;
    }

    RequiredSize = 0;
    Status = STATUS_SUCCESS;
    ArpOption = (SOCKET_ARP_OPTION)Option;
    switch (ArpOption) {
    case SocketArpOptionTranslateAddress:
        if (Set != FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

        RequiredSize = sizeof(NET_TRANSLATION_REQUEST);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        TranslationRequest = (PNET_TRANSLATION_REQUEST)Data;
        Status = NetpArpTranslateAddress(TranslationRequest);
        break;

    default:
        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto ArpGetSetInformationEnd;
    }

ArpGetSetInformationEnd:
    return Status;
}

KSTATUS
NetpArpTranslateAddress (
    PNET_TRANSLATION_REQUEST Request
    )

/*++

Routine Description:

    This routine translates a network level address to a physical address.

Arguments:

    Request - Supplies a pointer to the translation request.

Return Value:

    Status code.

--*/

{

    PNET_LINK Link;
    PNET_LINK_ADDRESS_ENTRY LinkAddress;
    PNETWORK_ADDRESS QueryAddress;
    ULONG SendCount;
    BOOL SendRequest;
    KSTATUS Status;
    PNET_TRANSLATION_ENTRY Translation;

    Link = Request->Link;
    LinkAddress = Request->LinkAddress;
    QueryAddress = Request->QueryAddress;

    ASSERT(LinkAddress->Network->Domain == QueryAddress->Domain);

    //
    // Loop trying to get the address, and waiting for an answer.
    //

    SendRequest = TRUE;
    SendCount = ARP_ADDRESS_TRANSLATION_RETRY_COUNT;
    while (TRUE) {
        Translation = NetLookupAddressTranslation(Link, QueryAddress);
        if (Translation != NULL) {
            Status = STATUS_SUCCESS;
            break;
        }

        //
        // If the lookup failed and a request needs to be sent, send it off.
        // But if all of the allowed attempts have been made, fail.
        //

        if (SendRequest != FALSE) {
            if (SendCount == 0) {
                Status = STATUS_TIMEOUT;
                break;
            }

            Status = NetpArpSendRequest(Link, LinkAddress, QueryAddress);
            if (!KSUCCESS(Status)) {
                break;
            }

            SendCount -= 1;
            SendRequest = FALSE;
        }

        //
        // Wait for some new address translation to come in.
        //

        Status = KeWaitForEvent(Link->AddressTranslationEvent,
                                FALSE,
                                ARP_ADDRESS_TRANSLATION_RETRY_INTERVAL);

        //
        // On timeouts, re-send the translation request.
        //

        if (Status == STATUS_TIMEOUT) {
            SendRequest = TRUE;

        //
        // On all other failures to wait for the event, break.
        //

        } else if (!KSUCCESS(Status)) {
            break;
        }
    }

    Request->Translation = Translation;
    return Status;
}

KSTATUS
NetpArpSendRequest (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS QueryAddress
    )

/*++

Routine Description:

    This routine allocates, assembles, and sends an ARP request to translate
    the given network address into a physical address. This routine returns
    as soon as the ARP request is successfully queued for transmission.

Arguments:

    Link - Supplies a pointer to the link to send the request down.

    LinkAddress - Supplies a pointer to the the source address of the request.

    QueryAddress - Supplies a pointer to the network address to ask about.

Return Value:

    STATUS_SUCCESS if the request was successfully sent off.

    STATUS_INSUFFICIENT_RESOURCES if the transmission buffer couldn't be
    allocated.

    Other errors on other failures.

--*/

{

    PARP_PACKET ArpPacket;
    PUCHAR CurrentPointer;
    ULONG Flags;
    BOOL LockHeld;
    PNET_PACKET_BUFFER NetPacket;
    NET_PACKET_LIST NetPacketList;
    PNET_DATA_LINK_SEND Send;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    NET_INITIALIZE_PACKET_LIST(&NetPacketList);
    LockHeld = FALSE;

    //
    // Allocate a buffer to send down to the network card.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    ArpPacket = NULL;
    Status = NetAllocateBuffer(0,
                               ARP_ETHERNET_IP4_SIZE,
                               0,
                               Link,
                               Flags,
                               &NetPacket);

    if (!KSUCCESS(Status)) {
        goto ArpSendRequestEnd;
    }

    NET_ADD_PACKET_TO_LIST(NetPacket, &NetPacketList);
    ArpPacket = NetPacket->Buffer + NetPacket->DataOffset;
    ArpPacket->HardwareType = CPU_TO_NETWORK16(ARP_HARDWARE_TYPE_ETHERNET);

    ASSERT(QueryAddress->Domain == NetDomainIp4);

    ArpPacket->ProtocolType = CPU_TO_NETWORK16(IP4_PROTOCOL_NUMBER);
    ArpPacket->ProtocolAddressLength = IP4_ADDRESS_SIZE;
    ArpPacket->HardwareAddressLength = ETHERNET_ADDRESS_SIZE;
    ArpPacket->Operation = CPU_TO_NETWORK16(ARP_OPERATION_REQUEST);

    //
    // Copy the sender's hardware address.
    //

    CurrentPointer = (PUCHAR)(ArpPacket + 1);
    RtlCopyMemory(CurrentPointer,
                  &(LinkAddress->PhysicalAddress.Address),
                  ETHERNET_ADDRESS_SIZE);

    CurrentPointer += ETHERNET_ADDRESS_SIZE;

    //
    // Make sure the link is still configured before copying its network
    // addresses. This assumes that the physical address does not change for
    // the lifetime of a link address entry, configured or not.
    //

    KeAcquireQueuedLock(Link->QueuedLock);
    LockHeld = TRUE;
    if (LinkAddress->State < NetLinkAddressConfigured) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto ArpSendRequestEnd;
    }

    //
    // Copy the sender's network address.
    //

    ASSERT(LinkAddress->Address.Domain == NetDomainIp4);

    RtlCopyMemory(CurrentPointer,
                  &(LinkAddress->Address.Address),
                  IP4_ADDRESS_SIZE);

    CurrentPointer += IP4_ADDRESS_SIZE;
    KeReleaseQueuedLock(Link->QueuedLock);
    LockHeld = FALSE;

    //
    // Zero out the target hardware address.
    //

    RtlZeroMemory(CurrentPointer, ETHERNET_ADDRESS_SIZE);
    CurrentPointer += ETHERNET_ADDRESS_SIZE;

    //
    // Copy the target network address.
    //

    RtlCopyMemory(CurrentPointer, &(QueryAddress->Address), IP4_ADDRESS_SIZE);
    CurrentPointer += IP4_ADDRESS_SIZE;

    ASSERT(((UINTN)CurrentPointer - (UINTN)ArpPacket) == ARP_ETHERNET_IP4_SIZE);

    //
    // Debug print the request.
    //

    if (NetArpDebug != FALSE) {
        RtlDebugPrint("ARP TX: Who has ");
        NetDebugPrintAddress(QueryAddress);
        RtlDebugPrint("? Tell ");
        NetDebugPrintAddress(&(LinkAddress->PhysicalAddress));
        RtlDebugPrint("\n");
    }

    //
    // Send the request off to the link.
    //

    Send = Link->DataLinkEntry->Interface.Send;
    Status = Send(Link->DataLinkContext,
                  &NetPacketList,
                  &(LinkAddress->PhysicalAddress),
                  NULL,
                  ARP_PROTOCOL_NUMBER);

    if (!KSUCCESS(Status)) {
        goto ArpSendRequestEnd;
    }

ArpSendRequestEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Link->QueuedLock);
    }

    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&NetPacketList);
    }

    return Status;
}

KSTATUS
NetpArpSendReply (
    PNET_LINK Link,
    PNET_LINK_ADDRESS_ENTRY LinkAddress,
    PNETWORK_ADDRESS DestinationNetworkAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress
    )

/*++

Routine Description:

    This routine allocates, assembles, and sends an ARP reply to communicate
    the physical address of one of the network addresses owned by this machine.
    This routine returns as soon as the ARP request is successfully queued for
    transmission.

Arguments:

    Link - Supplies a pointer to the link to send the reply down.

    LinkAddress - Supplies the source address of the reply.

    DestinationNetworkAddress - Supplies a pointer to the network address to
        send the response to.

    DestinationPhysicalAddress - Supplies a pointer to the physical address to
        send the response to.

Return Value:

    STATUS_SUCCESS if the request was successfully sent off.

    STATUS_INSUFFICIENT_RESOURCES if the transmission buffer couldn't be
    allocated.

    Other errors on other failures.

--*/

{

    PARP_PACKET ArpPacket;
    PUCHAR CurrentPointer;
    ULONG Flags;
    BOOL LockHeld;
    PNET_PACKET_BUFFER NetPacket;
    NET_PACKET_LIST NetPacketList;
    NETWORK_ADDRESS NetworkAddress;
    PNET_DATA_LINK_SEND Send;
    KSTATUS Status;

    LockHeld = FALSE;
    NET_INITIALIZE_PACKET_LIST(&NetPacketList);

    //
    // Allocate a buffer to send down to the network card.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    ArpPacket = NULL;
    Status = NetAllocateBuffer(0,
                               ARP_ETHERNET_IP4_SIZE,
                               0,
                               Link,
                               Flags,
                               &NetPacket);

    if (!KSUCCESS(Status)) {
        goto ArpSendReplyEnd;
    }

    NET_ADD_PACKET_TO_LIST(NetPacket, &NetPacketList);
    ArpPacket = NetPacket->Buffer + NetPacket->DataOffset;
    ArpPacket->HardwareType = CPU_TO_NETWORK16(ARP_HARDWARE_TYPE_ETHERNET);

    ASSERT(DestinationNetworkAddress->Domain == NetDomainIp4);
    ASSERT(DestinationPhysicalAddress->Domain == Link->DataLinkEntry->Domain);

    ArpPacket->ProtocolType = CPU_TO_NETWORK16(IP4_PROTOCOL_NUMBER);
    ArpPacket->ProtocolAddressLength = 4;
    ArpPacket->HardwareAddressLength = ETHERNET_ADDRESS_SIZE;
    ArpPacket->Operation = CPU_TO_NETWORK16(ARP_OPERATION_REPLY);

    //
    // Copy the sender's hardware address.
    //

    CurrentPointer = (PUCHAR)(ArpPacket + 1);
    RtlCopyMemory(CurrentPointer,
                  &(LinkAddress->PhysicalAddress.Address),
                  ETHERNET_ADDRESS_SIZE);

    CurrentPointer += ETHERNET_ADDRESS_SIZE;

    //
    // Make sure the link is still configured before copying its network
    // addresses. This assumes that the physical address does not change for
    // the lifetime of a link address entry, configured or not.
    //

    KeAcquireQueuedLock(Link->QueuedLock);
    LockHeld = TRUE;
    if (LinkAddress->State < NetLinkAddressConfigured) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto ArpSendReplyEnd;
    }

    //
    // Store the network address if debugging is enabled.
    //

    if (NetArpDebug != FALSE) {
        RtlCopyMemory(&NetworkAddress,
                      &(LinkAddress->Address),
                      sizeof(NETWORK_ADDRESS));
    }

    //
    // Copy the sender's network address.
    //

    ASSERT(LinkAddress->Address.Domain == NetDomainIp4);

    RtlCopyMemory(CurrentPointer,
                  &(LinkAddress->Address.Address),
                  IP4_ADDRESS_SIZE);

    CurrentPointer += IP4_ADDRESS_SIZE;
    KeReleaseQueuedLock(Link->QueuedLock);
    LockHeld = FALSE;

    //
    // Copy the target hardware address.
    //

    RtlCopyMemory(CurrentPointer,
                  DestinationPhysicalAddress->Address,
                  ETHERNET_ADDRESS_SIZE);

    CurrentPointer += ETHERNET_ADDRESS_SIZE;

    //
    // Copy the target network address.
    //

    RtlCopyMemory(CurrentPointer,
                  DestinationNetworkAddress->Address,
                  IP4_ADDRESS_SIZE);

    CurrentPointer += IP4_ADDRESS_SIZE;

    ASSERT(((UINTN)CurrentPointer - (UINTN)ArpPacket) == ARP_ETHERNET_IP4_SIZE);

    //
    // Debug print the request.
    //

    if (NetArpDebug != FALSE) {
        RtlDebugPrint("ARP TX: ");
        NetDebugPrintAddress(&NetworkAddress);
        RtlDebugPrint(" is at ");
        NetDebugPrintAddress(&(LinkAddress->PhysicalAddress));
        RtlDebugPrint(" (sent to ");
        NetDebugPrintAddress(DestinationNetworkAddress);
        RtlDebugPrint(" ");
        NetDebugPrintAddress(DestinationPhysicalAddress);
        RtlDebugPrint(")\n");
    }

    //
    // Send the request off to the link.
    //

    Send = Link->DataLinkEntry->Interface.Send;
    Status = Send(Link->DataLinkContext,
                  &NetPacketList,
                  &(LinkAddress->PhysicalAddress),
                  DestinationPhysicalAddress,
                  ARP_PROTOCOL_NUMBER);

    if (!KSUCCESS(Status)) {
        goto ArpSendReplyEnd;
    }

ArpSendReplyEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Link->QueuedLock);
    }

    if (!KSUCCESS(Status)) {
        NetDestroyBufferList(&NetPacketList);
    }

    return Status;
}

