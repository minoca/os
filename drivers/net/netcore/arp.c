/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#define NET_API DLLEXPORT

#include <minoca/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/ip4.h>
#include "ethernet.h"

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

typedef struct _ARP_PACKET {
    USHORT HardwareType;
    USHORT ProtocolType;
    UCHAR HardwareAddressLength;
    UCHAR ProtocolAddressLength;
    USHORT Operation;
} PACKED ARP_PACKET, *PARP_PACKET;

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

KSTATUS
NetpArpInitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    );

KSTATUS
NetpArpBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpArpListen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpArpConnect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpArpDisconnect (
    PNET_SOCKET Socket
    );

KSTATUS
NetpArpClose (
    PNET_SOCKET Socket
    );

KSTATUS
NetpArpSend (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Destination,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride,
    PLIST_ENTRY PacketListHead
    );

VOID
NetpArpProcessReceivedData (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

ULONG
NetpArpPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

KSTATUS
NetpArpGetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
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
    NetworkEntry.Type = SocketNetworkArp;
    NetworkEntry.ParentProtocolNumber = ARP_PROTOCOL_NUMBER;
    NetworkEntry.Interface.InitializeLink = NetpArpInitializeLink;
    NetworkEntry.Interface.DestroyLink = NetpArpDestroyLink;
    NetworkEntry.Interface.InitializeSocket = NetpArpInitializeSocket;
    NetworkEntry.Interface.BindToAddress = NetpArpBindToAddress;
    NetworkEntry.Interface.Listen = NetpArpListen;
    NetworkEntry.Interface.Connect = NetpArpConnect;
    NetworkEntry.Interface.Disconnect = NetpArpDisconnect;
    NetworkEntry.Interface.Close = NetpArpClose;
    NetworkEntry.Interface.Send = NetpArpSend;
    NetworkEntry.Interface.ProcessReceivedData = NetpArpProcessReceivedData;
    NetworkEntry.Interface.PrintAddress = NetpArpPrintAddress;
    NetworkEntry.Interface.GetSetInformation = NetpArpGetSetInformation;
    Status = NetRegisterNetworkLayer(&NetworkEntry, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
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

    LinkAddress - Supplies the source address of the request.

    QueryAddress - Supplies the network address to ask about.

Return Value:

    STATUS_SUCCESS if the request was successfully sent off.

    STATUS_INSUFFICIENT_RESOURCES if the transmission buffer couldn't be
    allocated.

    Other errors on other failures.

--*/

{

    PNET_PACKET_BUFFER Buffer;
    LIST_ENTRY BufferListHead;
    PUCHAR CurrentPointer;
    PNET_DATA_LINK_ENTRY DataLinkEntry;
    ULONG Flags;
    BOOL LockHeld;
    PARP_PACKET Packet;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    INITIALIZE_LIST_HEAD(&BufferListHead);
    LockHeld = FALSE;

    //
    // Allocate a buffer to send down to the network card.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Packet = NULL;
    Status = NetAllocateBuffer(0,
                               ARP_ETHERNET_IP4_SIZE,
                               0,
                               Link,
                               Flags,
                               &Buffer);

    if (!KSUCCESS(Status)) {
        goto ArpSendRequestEnd;
    }

    INSERT_BEFORE(&(Buffer->ListEntry), &BufferListHead);
    Packet = Buffer->Buffer + Buffer->DataOffset;
    Packet->HardwareType = CPU_TO_NETWORK16(ARP_HARDWARE_TYPE_ETHERNET);

    ASSERT(QueryAddress->Network == SocketNetworkIp4);

    Packet->ProtocolType = CPU_TO_NETWORK16(IP4_PROTOCOL_NUMBER);
    Packet->ProtocolAddressLength = IP4_ADDRESS_SIZE;
    Packet->HardwareAddressLength = ETHERNET_ADDRESS_SIZE;
    Packet->Operation = CPU_TO_NETWORK16(ARP_OPERATION_REQUEST);

    //
    // Copy the sender's hardware address.
    //

    CurrentPointer = (PUCHAR)(Packet + 1);
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
    if (LinkAddress->Configured == FALSE) {
        Status = STATUS_NO_NETWORK_CONNECTION;
        goto ArpSendRequestEnd;
    }

    //
    // Copy the sender's network address.
    //

    ASSERT(LinkAddress->Address.Network == SocketNetworkIp4);

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

    ASSERT(((UINTN)CurrentPointer - (UINTN)Packet) == ARP_ETHERNET_IP4_SIZE);

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

    DataLinkEntry = Link->DataLinkEntry;
    Status = DataLinkEntry->Interface.Send(Link,
                                           &BufferListHead,
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
        while (LIST_EMPTY(&BufferListHead) == FALSE) {
            Buffer = LIST_VALUE(BufferListHead.Next,
                                NET_PACKET_BUFFER,
                                ListEntry);

            LIST_REMOVE(&(Buffer->ListEntry));
            NetFreeBuffer(Buffer);
        }
    }

    return Status;
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

KSTATUS
NetpArpInitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    )

/*++

Routine Description:

    This routine initializes any pieces of information needed by the network
    layer for the socket. The core networking library will fill in the common
    header when this routine returns.

Arguments:

    ProtocolEntry - Supplies a pointer to the protocol information.

    NetworkEntry - Supplies a pointer to the network information.

    NetworkProtocol - Supplies the raw protocol value for this socket used on
        the network. This value is network specific.

    NewSocket - Supplies a pointer to the new socket. The network layer should
        at the very least add any needed header size.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpArpBindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine binds the given socket to the specified network address.

Arguments:

    Socket - Supplies a pointer to the socket to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpArpListen (
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

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpArpConnect (
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

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpArpDisconnect (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine will disconnect the given socket from its remote address.

Arguments:

    Socket - Supplies a pointer to the socket to disconnect.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpArpClose (
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

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

KSTATUS
NetpArpSend (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Destination,
    PNET_SOCKET_LINK_OVERRIDE LinkOverride,
    PLIST_ENTRY PacketListHead
    )

/*++

Routine Description:

    This routine sends data through the network.

Arguments:

    Socket - Supplies a pointer to the socket to send the data to.

    Destination - Supplies a pointer to the network address to send to.

    LinkOverride - Supplies an optional pointer to a structure that contains
        all the necessary information to send data out a link on behalf
        of the given socket.

    PacketListHead - Supplies a pointer to the head of a list of network
        packets to send. Data these packets may be modified by this routine,
        but must not be used once this routine returns.

Return Value:

    Status code. It is assumed that either all packets are submitted (if
    success is returned) or none of the packets were submitted (if a failing
    status is returned).

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
}

VOID
NetpArpProcessReceivedData (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    )

/*++

Routine Description:

    This routine is called to process a received packet.

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

    PARP_PACKET ArpPacket;
    PUCHAR CurrentPointer;
    PNET_LINK_ADDRESS_ENTRY LinkAddressEntry;
    USHORT Operation;
    ULONG PacketSize;
    NETWORK_ADDRESS SenderNetworkAddress;
    NETWORK_ADDRESS SenderPhysicalAddress;
    KSTATUS Status;
    NETWORK_ADDRESS TargetNetworkAddress;
    NETWORK_ADDRESS TargetPhysicalAddress;

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
    SenderPhysicalAddress.Network = (SOCKET_NETWORK)Link->DataLinkEntry->Type;
    CurrentPointer = (PUCHAR)(ArpPacket + 1);
    RtlCopyMemory(&(SenderPhysicalAddress.Address),
                  CurrentPointer,
                  ETHERNET_ADDRESS_SIZE);

    CurrentPointer += ETHERNET_ADDRESS_SIZE;
    SenderNetworkAddress.Network = SocketNetworkIp4;
    RtlCopyMemory(&(SenderNetworkAddress.Address),
                  CurrentPointer,
                  IP4_ADDRESS_SIZE);

    CurrentPointer += IP4_ADDRESS_SIZE;
    TargetPhysicalAddress.Network = (SOCKET_NETWORK)Link->DataLinkEntry->Type;
    RtlCopyMemory(&(TargetPhysicalAddress.Address),
                  CurrentPointer,
                  ETHERNET_ADDRESS_SIZE);

    CurrentPointer += ETHERNET_ADDRESS_SIZE;
    TargetNetworkAddress.Network = SocketNetworkIp4;
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
                                        FALSE,
                                        &LinkAddressEntry);

        if (!KSUCCESS(Status)) {
            return;
        }

        //
        // Requests themselves are translations. Remember this translation.
        //

        NetAddNetworkAddressTranslation(Link,
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

    NetAddNetworkAddressTranslation(Link,
                                    &SenderNetworkAddress,
                                    &SenderPhysicalAddress);

    return;
}

ULONG
NetpArpPrintAddress (
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

    //
    // There is no such thing as an ARP address. Everything is broadcast.
    //

    ASSERT(FALSE);

    return 0;
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

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the information type is incorrect.

    STATUS_BUFFER_TOO_SMALL if the data buffer is too small to receive the
        requested option.

    STATUS_NOT_SUPPORTED_BY_PROTOCOL if the socket option is not supported by
        the socket.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_SUPPORTED;
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

    PNET_PACKET_BUFFER Buffer;
    LIST_ENTRY BufferListHead;
    PUCHAR CurrentPointer;
    PNET_DATA_LINK_ENTRY DataLinkEntry;
    ULONG Flags;
    BOOL LockHeld;
    NETWORK_ADDRESS NetworkAddress;
    PARP_PACKET Packet;
    KSTATUS Status;

    LockHeld = FALSE;
    INITIALIZE_LIST_HEAD(&BufferListHead);

    //
    // Allocate a buffer to send down to the network card.
    //

    Flags = NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DEVICE_LINK_FOOTERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_HEADERS |
            NET_ALLOCATE_BUFFER_FLAG_ADD_DATA_LINK_FOOTERS;

    Packet = NULL;
    Status = NetAllocateBuffer(0,
                               ARP_ETHERNET_IP4_SIZE,
                               0,
                               Link,
                               Flags,
                               &Buffer);

    if (!KSUCCESS(Status)) {
        goto ArpSendReplyEnd;
    }

    INSERT_BEFORE(&(Buffer->ListEntry), &BufferListHead);
    Packet = Buffer->Buffer + Buffer->DataOffset;
    Packet->HardwareType = CPU_TO_NETWORK16(ARP_HARDWARE_TYPE_ETHERNET);

    ASSERT(DestinationNetworkAddress->Network == SocketNetworkIp4);
    ASSERT(DestinationPhysicalAddress->Network ==
           (SOCKET_NETWORK)Link->DataLinkEntry->Type);

    Packet->ProtocolType = CPU_TO_NETWORK16(IP4_PROTOCOL_NUMBER);
    Packet->ProtocolAddressLength = 4;
    Packet->HardwareAddressLength = ETHERNET_ADDRESS_SIZE;
    Packet->Operation = CPU_TO_NETWORK16(ARP_OPERATION_REPLY);

    //
    // Copy the sender's hardware address.
    //

    CurrentPointer = (PUCHAR)(Packet + 1);
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
    if (LinkAddress->Configured == FALSE) {
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

    ASSERT(LinkAddress->Address.Network == SocketNetworkIp4);

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

    ASSERT(((UINTN)CurrentPointer - (UINTN)Packet) == ARP_ETHERNET_IP4_SIZE);

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

    DataLinkEntry = Link->DataLinkEntry;
    Status = DataLinkEntry->Interface.Send(Link,
                                           &BufferListHead,
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
        while (LIST_EMPTY(&BufferListHead) == FALSE) {
            Buffer = LIST_VALUE(BufferListHead.Next,
                                NET_PACKET_BUFFER,
                                ListEntry);

            LIST_REMOVE(&(Buffer->ListEntry));
            NetFreeBuffer(Buffer);
        }
    }

    return Status;
}

