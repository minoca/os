/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#define NET_API DLLEXPORT

#include <minoca/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/acpi.h>
#include <minoca/smbios.h>
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
    PNET_LINK Link,
    PLIST_ENTRY PacketListHead,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    );

VOID
NetpEthernetProcessReceivedPacket (
    PNET_LINK Link,
    PNET_PACKET_BUFFER Packet
    );

VOID
NetpEthernetGetBroadcastAddress (
    PNETWORK_ADDRESS PhysicalNetworkAddress
    );

ULONG
NetpEthernetPrintAddress (
    PNETWORK_ADDRESS Address,
    PSTR Buffer,
    ULONG BufferLength
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
    PNET_PACKET_SIZE_INFORMATION SizeInformation;
    KSTATUS Status;

    DataLinkEntry.Type = NetDataLinkEthernet;
    SizeInformation = &(DataLinkEntry.PacketSizeInformation);
    SizeInformation->HeaderSize = ETHERNET_HEADER_SIZE;
    SizeInformation->FooterSize = 0;
    SizeInformation->MaxPacketSize = ETHERNET_HEADER_SIZE +
                                     ETHERNET_MAXIMUM_PAYLOAD_SIZE;

    Interface = &(DataLinkEntry.Interface);
    Interface->InitializeLink = NetpEthernetInitializeLink;
    Interface->DestroyLink = NetpEthernetDestroyLink;
    Interface->Send = NetpEthernetSend;
    Interface->ProcessReceivedPacket = NetpEthernetProcessReceivedPacket;
    Interface->GetBroadcastAddress = NetpEthernetGetBroadcastAddress;
    Interface->PrintAddress = NetpEthernetPrintAddress;
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

    return;
}

KSTATUS
NetpEthernetSend (
    PNET_LINK Link,
    PLIST_ENTRY PacketListHead,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    ULONG ProtocolNumber
    )

/*++

Routine Description:

    This routine sends data through the data link layer and out the link.

Arguments:

    Link - Supplies a pointer to the link on which to send the data.

    PacketListHead - Supplies a pointer to the head of the list of network
        packets to send. Data in these packets may be modified by this routine,
        but must not be used once this routine returns.

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
    PVOID DriverContext;
    PNET_PACKET_BUFFER Packet;

    CurrentEntry = PacketListHead->Next;
    while (CurrentEntry != PacketListHead) {
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

    DriverContext = Link->Properties.DriverContext;
    return Link->Properties.Interface.Send(DriverContext, PacketListHead);
}

VOID
NetpEthernetProcessReceivedPacket (
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

    PNET_NETWORK_ENTRY NetworkEntry;
    ULONG NetworkProtocol;

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
    NetworkEntry->Interface.ProcessReceivedData(Link, Packet);
    return;
}

VOID
NetpEthernetGetBroadcastAddress (
    PNETWORK_ADDRESS PhysicalNetworkAddress
    )

/*++

Routine Description:

    This routine gets the ethernet broadcast address.

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
    PhysicalNetworkAddress->Network = SocketNetworkPhysicalEthernet;
    PhysicalNetworkAddress->Port = 0;
    for (ByteIndex = 0; ByteIndex < ETHERNET_ADDRESS_SIZE; ByteIndex += 1) {
        BytePointer[ByteIndex] = 0xFF;
    }

    return;
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

    ASSERT(Address->Network == SocketNetworkPhysicalEthernet);

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

