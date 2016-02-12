/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    genctrl.c

Abstract:

    This module implements the generic netlink control family message handling.

Author:

    Chris Stevens 10-Feb-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Typically, generic netlink families are supposed to be able to stand on
// their own (i.e. be able to be implemented outside the core net library). For
// the built-in ones, avoid including netcore.h, but still redefine those
// functions that would otherwise generate imports. This, however, needs to
// include generic.h to get access to the tree of families.
//

#define NET_API DLLEXPORT

#include <minoca/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/netlink.h>
#include "generic.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
NetpNetlinkGenericControlProcessReceivedData (
    PIO_HANDLE Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

VOID
NetpNetlinkGenericControlGetFamily (
    PIO_HANDLE Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    );

//
// -------------------------------------------------------------------- Globals
//

NETLINK_GENERIC_FAMILY_PROPERTIES NetNetlinkGenericControlFamily = {
    NETLINK_GENERIC_FAMILY_PROPERTIES_VERSION,
    NETLINK_GENERIC_ID_CONTROL,
    NETLINK_GENERIC_CONTROL_NAME,
    {
        NetpNetlinkGenericControlProcessReceivedData
    }
};

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpNetlinkGenericControlInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the built in generic netlink control family.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    Status = NetNetlinkGenericRegisterFamily(&NetNetlinkGenericControlFamily,
                                             NULL);

    if (!KSUCCESS(Status)) {

        ASSERT(KSUCCESS(Status));

    }

    return;
}

VOID
NetpNetlinkGenericControlProcessReceivedData (
    PIO_HANDLE Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine is called to process a received generic netlink control packet.

Arguments:

    Socket - Supplies a handle to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    PNETLINK_GENERIC_HEADER GenericHeader;
    PNETLINK_HEADER NetlinkHeader;
    ULONG PacketLength;

    NetlinkHeader = (PNETLINK_HEADER)(Packet->Buffer + Packet->DataOffset);
    PacketLength = NetlinkHeader->Length;

    ASSERT(PacketLength <= (Packet->FooterOffset - Packet->DataOffset));
    ASSERT(PacketLength > sizeof(NETLINK_HEADER));

    PacketLength -= sizeof(NETLINK_HEADER);
    if (PacketLength < sizeof(NETLINK_GENERIC_HEADER)) {
        RtlDebugPrint("NETLINK: Invalid control packet did not contain a "
                      "generic netlink header.\n");

        return;
    }

    GenericHeader = (PNETLINK_GENERIC_HEADER)(NetlinkHeader + 1);

    //
    // Ignore all the unsupported commands for now.
    //

    if (GenericHeader->Command != NETLINK_GENERIC_CONTROL_GET_FAMILY) {
        return;
    }

    NetpNetlinkGenericControlGetFamily(Socket,
                                       Packet,
                                       SourceAddress,
                                       DestinationAddress);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
NetpNetlinkGenericControlGetFamily (
    PIO_HANDLE Socket,
    PNET_PACKET_BUFFER Packet,
    PNETWORK_ADDRESS SourceAddress,
    PNETWORK_ADDRESS DestinationAddress
    )

/*++

Routine Description:

    This routine is called to process a received generic netlink control packet.

Arguments:

    Socket - Supplies a handle to the socket that received the packet.

    Packet - Supplies a pointer to a structure describing the incoming packet.
        This structure may be used as a scratch space while this routine
        executes and the packet travels up the stack, but will not be accessed
        after this routine returns.

    SourceAddress - Supplies a pointer to the source (remote) address that the
        packet originated from. This memory will not be referenced once the
        function returns, it can be stack allocated.

    DestinationAddress - Supplies a pointer to the destination (local) address
        that the packet is heading to. This memory will not be referenced once
        the function returns, it can be stack allocated.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    PNETLINK_ATTRIBUTE Attribute;
    ULONG AttributeLength;
    PVOID Data;
    ULONG DataLength;
    PNETLINK_GENERIC_FAMILY Family;
    USHORT FamilyId;
    PSTR FamilyName;
    ULONG FamilyNameLength;
    PNETLINK_GENERIC_HEADER GenericHeader;
    PIO_BUFFER IoBuffer;
    PNETLINK_ADDRESS NetlinkAddress;
    PNETLINK_HEADER NetlinkHeader;
    PNET_SOCKET NetSocket;
    PVOID NextAttribute;
    ULONG PacketLength;
    SOCKET_IO_PARAMETERS Parameters;
    UINTN ReplyLength;
    ULONG SequenceNumber;

    IoBuffer = NULL;
    NetlinkHeader = (PNETLINK_HEADER)(Packet->Buffer + Packet->DataOffset);
    PacketLength = NetlinkHeader->Length;

    ASSERT(PacketLength <= (Packet->FooterOffset - Packet->DataOffset));
    ASSERT(PacketLength > sizeof(NETLINK_HEADER));

    PacketLength -= sizeof(NETLINK_HEADER);

    ASSERT(PacketLength >= sizeof(NETLINK_GENERIC_HEADER));

    GenericHeader = (PNETLINK_GENERIC_HEADER)(NetlinkHeader + 1);
    PacketLength -= sizeof(NETLINK_GENERIC_HEADER);

    //
    // Search the packet for an attribute that identifies the family.
    //

    Attribute = (PNETLINK_ATTRIBUTE)(GenericHeader + 1);

    ASSERT(IS_POINTER_ALIGNED(Attribute, NETLINK_ATTRIBUTE_ALIGNMENT) != FALSE);

    Family = NULL;
    while ((PacketLength != 0) && (Family == NULL)) {
        if ((PacketLength < sizeof(NETLINK_ATTRIBUTE)) ||
            (PacketLength < Attribute->Length)) {

            break;
        }

        Data = (PVOID)Attribute + sizeof(NETLINK_ATTRIBUTE);
        DataLength = Attribute->Length - sizeof(NETLINK_ATTRIBUTE);
        switch (Attribute->Type) {
        case NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_ID:
            if (DataLength != sizeof(USHORT)) {
                break;
            }

            FamilyId = *(PUSHORT)Data;
            Family = NetpNetlinkGenericLookupFamilyById(FamilyId);
            break;

        case NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_NAME:
            FamilyName = (PSTR)Data;
            Family = NetpNetlinkGenericLookupFamilyByName(FamilyName);
            break;

        default:
            break;
        }

        NextAttribute = (PVOID)Attribute + Attribute->Length;
        NextAttribute = ALIGN_POINTER_UP(NextAttribute,
                                         NETLINK_ATTRIBUTE_ALIGNMENT);

        AttributeLength = (ULONG)(NextAttribute - (PVOID)Attribute);
        if (AttributeLength > PacketLength) {
            break;
        }

        PacketLength -= AttributeLength;
    }

    if (Family == NULL) {
        goto GetFamilyEnd;
    }

    //
    // A family was found. Fill out the new family message and send it back to
    // the socket that requested the family information.
    //

    ReplyLength = sizeof(NETLINK_HEADER) + sizeof(NETLINK_GENERIC_HEADER);
    ReplyLength += sizeof(NETLINK_ATTRIBUTE) + sizeof(USHORT);
    ReplyLength = ALIGN_RANGE_UP(ReplyLength, NETLINK_ATTRIBUTE_ALIGNMENT);
    FamilyNameLength = RtlStringLength(Family->Properties.Name) + 1;
    ReplyLength += sizeof(NETLINK_ATTRIBUTE) + FamilyNameLength;
    IoBuffer = MmAllocatePagedIoBuffer(ReplyLength, 0);
    if (IoBuffer == NULL) {
        goto GetFamilyEnd;
    }

    ASSERT(IoBuffer->FragmentCount == 1);

    SequenceNumber = NetlinkHeader->SequenceNumber;
    NetlinkHeader = (PNETLINK_HEADER)IoBuffer->Fragment[0].VirtualAddress;
    NetlinkHeader->Length = ReplyLength;
    NetlinkHeader->Type = NETLINK_GENERIC_ID_CONTROL;
    NetlinkHeader->Flags = 0;
    NetlinkHeader->SequenceNumber = SequenceNumber;
    IoGetSocketFromHandle(Socket, (PVOID)&NetSocket);
    NetlinkAddress = (PNETLINK_ADDRESS)&(NetSocket->LocalAddress);
    NetlinkHeader->PortId = NetlinkAddress->Port;
    GenericHeader = (PNETLINK_GENERIC_HEADER)(NetlinkHeader + 1);
    GenericHeader->Command = NETLINK_GENERIC_CONTROL_NEW_FAMILY;
    GenericHeader->Version = 0;
    GenericHeader->Reserved = 0;
    Attribute = (PNETLINK_ATTRIBUTE)(GenericHeader + 1);
    Attribute->Length = sizeof(NETLINK_ATTRIBUTE) + sizeof(USHORT);
    Attribute->Type = NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_ID;
    *((PUSHORT)(Attribute + 1)) = Family->Properties.Id;
    Attribute = (PVOID)Attribute + Attribute->Length;
    Attribute = ALIGN_POINTER_UP(Attribute, NETLINK_ATTRIBUTE_ALIGNMENT);
    Attribute->Length = sizeof(NETLINK_ATTRIBUTE) + FamilyNameLength;
    Attribute->Type = NETLINK_GENERIC_CONTROL_ATTRIBUTE_FAMILY_NAME;
    RtlStringCopy((PVOID)(Attribute + 1),
                  Family->Properties.Name,
                  FamilyNameLength);

    //
    // Send the reply back to the source address.
    //

    RtlZeroMemory(&Parameters, sizeof(SOCKET_IO_PARAMETERS));
    Parameters.TimeoutInMilliseconds = WAIT_TIME_INDEFINITE;
    Parameters.NetworkAddress = SourceAddress;
    Parameters.Size = ReplyLength;
    IoSocketSendData(TRUE, Socket, &Parameters, IoBuffer);

GetFamilyEnd:
    if (Family != NULL) {
        NetpNetlinkGenericFamilyReleaseReference(Family);
    }

    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return;
}

