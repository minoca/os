/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ethernet.c

Abstract:

    This module implements functionality for Ethernet-based links.

Author:

    Evan Green 5-Apr-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Data link layer drivers are supposed to be able to stand on their own (ie be
// able to be implemented outside the core net library). For the builtin ones,
// avoid including netcore.h, but still redefine those functions that would
// otherwise generate imports.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/ip4.h>
#include <minoca/kernel/acpi.h>
#include <minoca/fw/smbios.h>
#include "ethernet.h"

//
// ---------------------------------------------------------------- Definitions
//

#define ETHERNET_ALLOCATION_TAG 0x72687445 // 'rhtE'

//
// Printed strings of ethernet addresses look something like:
// "12:34:56:78:9A:BC". Include the null terminator.
//

#define ETHERNET_STRING_LENGTH 18

//
// Define the Ethernet debug flags.
//

#define ETHERNET_DEBUG_FLAG_DROPPED_PACKETS 0x00000001

//
// Define the IPv4 address mask for the bits that get included in a multicast
// MAC address.
//

#define ETHERNET_IP4_MULTICAST_TO_MAC_MASK CPU_TO_NETWORK32(0x007FFFFF)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
NetpEthernetInitializeLink (
    PNET_LINK Link
    );

VOID
NetpEthernetDestroyLink (
    PNET_LINK Link
    );

KSTATUS
NetpEthernetSend (
    PVOID DataLinkContext,
    PNET_PACKET_LIST PacketList,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    );

VOID
NetpEthernetProcessReceivedPacket (
    PVOID DataLinkContext,
    PNET_PACKET_BUFFER Packet
    );

KSTATUS
NetpEthernetConvertToPhysicalAddress (
    PNETWORK_ADDRESS NetworkAddress,
    PNETWORK_ADDRESS PhysicalAddress,
    NET_ADDRESS_TYPE NetworkAddressType
    );

ULONG
NetpEthernetPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
    );

VOID
NetpEthernetGetPacketSizeInformation (
    PVOID DataLinkContext,
    PNET_PACKET_SIZE_INFORMATION PacketSizeInformation,
    ULONG Flags
    );

KSTATUS
NetpEthernetGetEthernetAddressFromSmbios (
    PULONG Address
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the lower 4 bytes of the created MAC address base. This value is
// incremented for each ethernet card that comes online without an assigned
// ethernet address.
//

ULONG NetEthernetInventedAddress;

//
// Store a bitmask of debug flags.
//

ULONG EthernetDebugFlags = 0;

//
// Stores the base MAC address for all IPv4 multicast addresses. The lower 23
// bits are taken from the lower 23-bits of the IPv4 address.
//

UCHAR NetEthernetIp4MulticastBase[ETHERNET_ADDRESS_SIZE] =
    {0x01, 0x00, 0x5E, 0x00, 0x00, 0x00};

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpEthernetInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for Ethernet frames.

Arguments:

    None.

Return Value:

    None.

--*/

{

    NET_DATA_LINK_ENTRY DataLinkEntry;
    HANDLE DataLinkHandle;
    PNET_DATA_LINK_INTERFACE Interface;
    KSTATUS Status;

    DataLinkEntry.Domain = NetDomainEthernet;
    Interface = &(DataLinkEntry.Interface);
    Interface->InitializeLink = NetpEthernetInitializeLink;
    Interface->DestroyLink = NetpEthernetDestroyLink;
    Interface->Send = NetpEthernetSend;
    Interface->ProcessReceivedPacket = NetpEthernetProcessReceivedPacket;
    Interface->ConvertToPhysicalAddress = NetpEthernetConvertToPhysicalAddress;
    Interface->PrintAddress = NetpEthernetPrintAddress;
    Interface->GetPacketSizeInformation = NetpEthernetGetPacketSizeInformation;
    Status = NetRegisterDataLinkLayer(&DataLinkEntry, &DataLinkHandle);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    return;
}

NET_API
BOOL
NetIsEthernetAddressValid (
    BYTE Address[ETHERNET_ADDRESS_SIZE]
    )

/*++

Routine Description:

    This routine determines if the given ethernet address is a valid individual
    address or not. This routine returns FALSE for 00:00:00:00:00:00 and
    FF:FF:FF:FF:FF:FF, and TRUE for everything else.

Arguments:

    Address - Supplies the address to check.

Return Value:

    TRUE if the ethernet address is a valid individual address.

    FALSE if the address is not valid.

--*/

{

    if ((Address[0] == 0) && (Address[1] == 0) && (Address[2] == 0) &&
        (Address[3] == 0) && (Address[4] == 0) && (Address[5] == 0)) {

        return FALSE;
    }

    if ((Address[0] == 0xFF) && (Address[1] == 0xFF) && (Address[2] == 0xFF) &&
        (Address[3] == 0xFF) && (Address[4] == 0xFF) && (Address[5] == 0xFF)) {

        return FALSE;
    }

    return TRUE;
}

NET_API
VOID
NetCreateEthernetAddress (
    BYTE Address[ETHERNET_ADDRESS_SIZE]
    )

/*++

Routine Description:

    This routine generates a random ethernet address.

Arguments:

    Address - Supplies the array where the new address will be stored.

Return Value:

    None.

--*/

{

    RUNLEVEL OldRunLevel;
    KSTATUS Status;
    ULONG Value;

    //
    // If no base has been assigned yet, get a random one.
    //

    if (NetEthernetInventedAddress == 0) {

        //
        // Use the SMBIOS table, which should hopefully have a platform
        // identifier in it, to compute an address that is unique to the
        // platform but remains constant across reboots. The beauty of this if
        // it works is it doesn't require any unique numbers to be stored in
        // the OS image.
        //

        Status = NetpEthernetGetEthernetAddressFromSmbios(
                                                  &NetEthernetInventedAddress);

        if (KSUCCESS(Status)) {
            Value = NetEthernetInventedAddress;

        //
        // If there is no SMBIOS table, use the processor counter to make a
        // random address up. This unfortunately changes across reboots.
        //

        } else {
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
            Value = HlQueryProcessorCounter() * 12345;
            KeLowerRunLevel(OldRunLevel);
            NetEthernetInventedAddress = Value;
        }

    } else {
        Value = RtlAtomicAdd32(&NetEthernetInventedAddress, 1);
    }

    //
    // Set the first byte to 2 to indicate a locally administered unicast
    // address.
    //

    Address[0] = 0x02;
    Address[1] = 0x00;
    RtlCopyMemory(&(Address[sizeof(USHORT)]), &Value, sizeof(ULONG));
    return;
}

KSTATUS
NetpEthernetInitializeLink (
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

    //
    // Ethernet does not need any extra state. It just expects to get the
    // network link passed back as the data context. No extra references on the
    // network link are taken because this data link context gets "destroyed"
    // when the network link's last reference is released.
    //

    Link->DataLinkContext = Link;
    return STATUS_SUCCESS;
}

VOID
NetpEthernetDestroyLink (
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

    Link->DataLinkContext = NULL;
    return;
}

KSTATUS
NetpEthernetSend (
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

    ULONG ByteIndex;
    PUCHAR CurrentElement;
    PLIST_ENTRY CurrentEntry;
    PVOID DeviceContext;
    PNET_LINK Link;
    PNET_PACKET_BUFFER Packet;
    KSTATUS Status;

    Link = (PNET_LINK)DataLinkContext;
    CurrentEntry = PacketList->Head.Next;
    while (CurrentEntry != &(PacketList->Head)) {
        Packet = LIST_VALUE(CurrentEntry, NET_PACKET_BUFFER, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(Packet->DataOffset >= ETHERNET_HEADER_SIZE);

        //
        // The length should not be bigger than the maximum allowed ethernet
        // packet.
        //

        ASSERT((Packet->FooterOffset - Packet->DataOffset) <=
               ETHERNET_MAXIMUM_PAYLOAD_SIZE);

        //
        // Copy the destination address.
        //

        Packet->DataOffset -= ETHERNET_HEADER_SIZE;
        CurrentElement = Packet->Buffer + Packet->DataOffset;
        if (DestinationPhysicalAddress != NULL) {
            RtlCopyMemory(CurrentElement,
                          &(DestinationPhysicalAddress->Address),
                          ETHERNET_ADDRESS_SIZE);

            CurrentElement += ETHERNET_ADDRESS_SIZE;

        //
        // If no destination address was supplied, use the broadcast address.
        //

        } else {
            for (ByteIndex = 0;
                 ByteIndex < ETHERNET_ADDRESS_SIZE;
                 ByteIndex += 1) {

                *CurrentElement = 0xFF;
                CurrentElement += 1;
            }
        }

        //
        // Copy the source address.
        //

        RtlCopyMemory(CurrentElement,
                      &(SourcePhysicalAddress->Address),
                      ETHERNET_ADDRESS_SIZE);

        CurrentElement += ETHERNET_ADDRESS_SIZE;

        //
        // Copy the protocol number.
        //

        *((PUSHORT)CurrentElement) = CPU_TO_NETWORK16((USHORT)ProtocolNumber);
    }

    DeviceContext = Link->Properties.DeviceContext;
    Status = Link->Properties.Interface.Send(DeviceContext, PacketList);

    //
    // If the link layer returns that the resource is in use it means it was
    // too busy to send all of the packets. Release the packets for it and
    // convert this into a success status.
    //

    if (Status == STATUS_RESOURCE_IN_USE) {
        if ((EthernetDebugFlags & ETHERNET_DEBUG_FLAG_DROPPED_PACKETS) != 0) {
            RtlDebugPrint("ETH: Link layer dropped %d packets.\n",
                          PacketList->Count);
        }

        NetDestroyBufferList(PacketList);
        Status = STATUS_SUCCESS;
    }

    return Status;
}

VOID
NetpEthernetProcessReceivedPacket (
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

    PNET_LINK Link;
    PNET_NETWORK_ENTRY NetworkEntry;
    ULONG NetworkProtocol;
    NET_RECEIVE_CONTEXT ReceiveContext;

    Link = (PNET_LINK)DataLinkContext;

    //
    // Get the network layer to deal with this.
    //

    NetworkProtocol = *((PUSHORT)(Packet->Buffer + Packet->DataOffset +
                                  (2 * ETHERNET_ADDRESS_SIZE)));

    NetworkProtocol = NETWORK_TO_CPU16(NetworkProtocol);
    NetworkEntry = NetGetNetworkEntry(NetworkProtocol);
    if (NetworkEntry == NULL) {
        RtlDebugPrint("Unknown protocol number 0x%x found in ethernet "
                      "header.\n",
                      NetworkProtocol);

        return;
    }

    //
    // Strip off the source MAC address, destination MAC address, and protocol
    // number.
    //

    Packet->DataOffset += (2 * ETHERNET_ADDRESS_SIZE) + sizeof(USHORT);
    RtlZeroMemory(&ReceiveContext, sizeof(NET_RECEIVE_CONTEXT));
    ReceiveContext.Packet = Packet;
    ReceiveContext.Link = Link;
    ReceiveContext.Network = NetworkEntry;
    NetworkEntry->Interface.ProcessReceivedData(&ReceiveContext);
    return;
}

KSTATUS
NetpEthernetConvertToPhysicalAddress (
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
                             ETHERNET_IP4_MULTICAST_TO_MAC_MASK;

            //
            // Copy the static base MAC address.
            //

            RtlCopyMemory(BytePointer,
                          NetEthernetIp4MulticastBase,
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
NetpEthernetPrintAddress (
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
        return ETHERNET_STRING_LENGTH;
    }

    ASSERT(Address->Domain == NetDomainEthernet);

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
NetpEthernetGetPacketSizeInformation (
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

    PacketSizeInformation->HeaderSize = ETHERNET_HEADER_SIZE;
    PacketSizeInformation->FooterSize = 0;
    PacketSizeInformation->MaxPacketSize = ETHERNET_HEADER_SIZE +
                                           ETHERNET_MAXIMUM_PAYLOAD_SIZE;

    PacketSizeInformation->MinPacketSize = ETHERNET_HEADER_SIZE +
                                           ETHERNET_MINIMUM_PAYLOAD_SIZE +
                                           ETHERNET_FOOTER_SIZE;

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
NetpEthernetGetEthernetAddressFromSmbios (
    PULONG Address
    )

/*++

Routine Description:

    This routine attempts to use the SMBIOS structures to invent a platform
    unique ethernet address.

Arguments:

    Address - Supplies a pointer where the lower 32 bits of the address will
        be returned on success.

Return Value:

    Returns the maximum length of any address if no network address is
    supplied.

    Returns the actual length of the network address string if a network address
    was supplied, including the null terminator.

--*/

{

    PSMBIOS_ENTRY_POINT EntryPoint;

    EntryPoint = AcpiFindTable(SMBIOS_ANCHOR_STRING_VALUE, NULL);
    if (EntryPoint == NULL) {
        return STATUS_NOT_FOUND;
    }

    //
    // Compute the CRC32 of the SMBIOS table structures, hoping that comes out
    // unique per platform.
    //

    *Address = RtlComputeCrc32(0,
                               EntryPoint + 1,
                               EntryPoint->StructureTableLength);

    return STATUS_SUCCESS;
}

