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

#include <minoca/driver.h>
#include <minoca/acpi.h>
#include <minoca/smbios.h>
#include "netcore.h"
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
NetpEthernetInitializeSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET NewSocket
    )

/*++

Routine Description:

    This routine initializes any pieces of information needed by the link
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

    ULONG MaxPacketSize;

    //
    // Calculate the maximum allowed Ethernet packet size, including headers
    // and footers from lower layers. If this is less than the current maximum
    // packet size, then update the size.
    //

    MaxPacketSize = NewSocket->HeaderSize +
                    ETHERNET_HEADER_SIZE +
                    ETHERNET_MAXIMUM_PAYLOAD_SIZE +
                    NewSocket->FooterSize;

    if (NewSocket->MaxPacketSize > MaxPacketSize) {
        NewSocket->MaxPacketSize = MaxPacketSize;
    }

    //
    // Add the Ethernet header size to the socket's headers. Do not add the
    // footer size as CRC off-loading takes care of it.
    //

    NewSocket->HeaderSize += ETHERNET_HEADER_SIZE;
    return STATUS_SUCCESS;
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
    PhysicalNetworkAddress->Network = SocketNetworkPhysical;
    PhysicalNetworkAddress->Port = 0;
    for (ByteIndex = 0; ByteIndex < ETHERNET_ADDRESS_SIZE; ByteIndex += 1) {
        BytePointer[ByteIndex] = 0xFF;
    }

    return;
}

VOID
NetpEthernetAddHeader (
    PNET_PACKET_BUFFER SendBuffer,
    PNETWORK_ADDRESS SourcePhysicalAddress,
    PNETWORK_ADDRESS DestinationPhysicalAddress,
    USHORT ProtocolNumber
    )

/*++

Routine Description:

    This routine adds headers to an ethernet packet.

Arguments:

    SendBuffer - Supplies a pointer to the sending buffer to add the headers
        to.

    SourcePhysicalAddress - Supplies a pointer to the source (local) physical
        network address.

    DestinationPhysicalAddress - Supplies the physical address of the
        destination, or at least the next hop.

    ProtocolNumber - Supplies the protocol number of the data inside the
        ethernet header.

Return Value:

    None.

--*/

{

    ULONG ByteIndex;
    PUCHAR CurrentElement;

    ASSERT(SendBuffer->DataOffset >= ETHERNET_HEADER_SIZE);

    //
    // The length should not be bigger than the maximum allowed ethernet packet.
    //

    ASSERT((SendBuffer->FooterOffset - SendBuffer->DataOffset) <=
           ETHERNET_MAXIMUM_PAYLOAD_SIZE);

    //
    // Copy the destination address.
    //

    SendBuffer->DataOffset -= ETHERNET_HEADER_SIZE;
    CurrentElement = SendBuffer->Buffer + SendBuffer->DataOffset;
    if (DestinationPhysicalAddress != NULL) {
        RtlCopyMemory(CurrentElement,
                      &(DestinationPhysicalAddress->Address),
                      ETHERNET_ADDRESS_SIZE);

        CurrentElement += ETHERNET_ADDRESS_SIZE;

    //
    // If no destination address was supplied, use the broadcast address.
    //

    } else {
        for (ByteIndex = 0; ByteIndex < ETHERNET_ADDRESS_SIZE; ByteIndex += 1) {
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

    *((PUSHORT)CurrentElement) = CPU_TO_NETWORK16(ProtocolNumber);
    return;
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

    ASSERT(Address->Network == SocketNetworkPhysical);

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

