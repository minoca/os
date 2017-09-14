/*++

Copyright (c) 2017 Minoca Corp. All Rights Reserved

Module Name:

    icmp6.c

Abstract:

    This module implements support for the Internet Control Message Protocol
    version 6, which encapsulates a range of IPv6 message types including NDP
    and MLD.

Author:

    Chris Stevens 23-Aug-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Network layer drivers are supposed to be able to stand on their own (i.e. be
// able to be implemented outside the core net library). For the builtin once,
// avoid including netcore.h, but still redefine those functions that would
// otherwise generate imports.
//

#define NET_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/ip6.h>
#include <minoca/net/icmp6.h>
#include "mld.h"
#include "ndp.h"

//
// While ICMPv6 is built into netcore and the same binary as IPv6, share the
// well-known addresses. This could easily be changed if the binaries need to
// be separated out.
//

#include "ip6addr.h"

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
NetpIcmp6CreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    );

VOID
NetpIcmp6DestroySocket (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIcmp6BindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpIcmp6Listen (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIcmp6Accept (
    PNET_SOCKET Socket,
    PIO_HANDLE *NewConnectionSocket,
    PNETWORK_ADDRESS RemoteAddress
    );

KSTATUS
NetpIcmp6Connect (
    PNET_SOCKET Socket,
    PNETWORK_ADDRESS Address
    );

KSTATUS
NetpIcmp6Close (
    PNET_SOCKET Socket
    );

KSTATUS
NetpIcmp6Shutdown (
    PNET_SOCKET Socket,
    ULONG ShutdownType
    );

KSTATUS
NetpIcmp6Send (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

VOID
NetpIcmp6ProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpIcmp6ProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    );

KSTATUS
NetpIcmp6Receive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    );

KSTATUS
NetpIcmp6GetSetInformation (
    PNET_SOCKET Socket,
    SOCKET_INFORMATION_TYPE InformationType,
    UINTN SocketOption,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
NetpIcmp6UserControl (
    PNET_SOCKET Socket,
    ULONG CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    );

//
// -------------------------------------------------------------------- Globals
//

NET_PROTOCOL_ENTRY NetIcmp6Protocol = {
    {NULL, NULL},
    NetSocketDatagram,
    SOCKET_INTERNET_PROTOCOL_ICMP6,
    0,
    NULL,
    NULL,
    {{0}, {0}, {0}},
    {
        NetpIcmp6CreateSocket,
        NetpIcmp6DestroySocket,
        NetpIcmp6BindToAddress,
        NetpIcmp6Listen,
        NetpIcmp6Accept,
        NetpIcmp6Connect,
        NetpIcmp6Close,
        NetpIcmp6Shutdown,
        NetpIcmp6Send,
        NetpIcmp6ProcessReceivedData,
        NetpIcmp6ProcessReceivedSocketData,
        NetpIcmp6Receive,
        NetpIcmp6GetSetInformation,
        NetpIcmp6UserControl
    }
};

//
// ------------------------------------------------------------------ Functions
//

VOID
NetpIcmp6Initialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for the ICMPv6 protocol.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Register the ICMPv6 socket handlers with the core networking library.
    //

    Status = NetRegisterProtocol(&NetIcmp6Protocol, NULL);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

    }

    //
    // Initialize any sub-protocols of ICMPv6.
    //

    NetpMldInitialize();
    NetpNdpInitialize();
    return;
}

KSTATUS
NetpIcmp6CreateSocket (
    PNET_PROTOCOL_ENTRY ProtocolEntry,
    PNET_NETWORK_ENTRY NetworkEntry,
    ULONG NetworkProtocol,
    PNET_SOCKET *NewSocket,
    ULONG Phase
    )

/*++

Routine Description:

    This routine allocates resources associated with a new socket. The protocol
    driver is responsible for allocating the structure (with additional length
    for any of its context). The core networking library will fill in the
    common header when this routine returns.

Arguments:

    ProtocolEntry - Supplies a pointer to the protocol information.

    NetworkEntry - Supplies a pointer to the network information.

    NetworkProtocol - Supplies the raw protocol value for this socket used on
        the network. This value is network specific.

    NewSocket - Supplies a pointer where a pointer to a newly allocated
        socket structure will be returned. The caller is responsible for
        allocating the socket (and potentially a larger structure for its own
        context). The core network library will fill in the standard socket
        structure after this routine returns. In phase 1, this will contain
        a pointer to the socket allocated during phase 0.

    Phase - Supplies the socket creation phase. Phase 0 is the allocation phase
        and phase 1 is the advanced initialization phase, which is invoked
        after net core is done filling out common portions of the socket
        structure.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

VOID
NetpIcmp6DestroySocket (
    PNET_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys resources associated with an open socket, officially
    marking the end of the kernel and core networking library's knowledge of
    this structure.

Arguments:

    Socket - Supplies a pointer to the socket to destroy. The core networking
        library will have already destroyed any resources inside the common
        header, the protocol should not reach through any pointers inside the
        socket header except the protocol and network entries.

Return Value:

    None. This routine is responsible for freeing the memory associated with
    the socket structure itself.

--*/

{

    return;
}

KSTATUS
NetpIcmp6BindToAddress (
    PNET_SOCKET Socket,
    PNET_LINK Link,
    PNETWORK_ADDRESS Address
    )

/*++

Routine Description:

    This routine binds the given socket to the specified network address.
    Usually this is a no-op for the protocol, it's simply responsible for
    passing the request down to the network layer.

Arguments:

    Socket - Supplies a pointer to the socket to bind.

    Link - Supplies an optional pointer to a link to bind to.

    Address - Supplies a pointer to the address to bind the socket to.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIcmp6Listen (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIcmp6Accept (
    PNET_SOCKET Socket,
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIcmp6Connect (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIcmp6Close (
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIcmp6Shutdown (
    PNET_SOCKET Socket,
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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIcmp6Send (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine sends the given data buffer through the network using a
    specific protocol.

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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

VOID
NetpIcmp6ProcessReceivedData (
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called to process a received packet.

Arguments:

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    None. When the function returns, the memory associated with the packet may
    be reclaimed and reused.

--*/

{

    USHORT Checksum;
    PICMP6_HEADER Icmp6Header;
    PNET_PACKET_BUFFER Packet;
    ULONG PacketSize;

    //
    // Validate the ICMPv6 header.
    //

    Packet = ReceiveContext->Packet;
    PacketSize = Packet->FooterOffset - Packet->DataOffset;
    if (PacketSize < sizeof(ICMP6_HEADER)) {
        RtlDebugPrint("ICMP6: Packet length (0x%08x) less than header size "
                      "(0x%08x)\n",
                      PacketSize,
                      sizeof(ICMP6_HEADER));

        return;
    }

    Icmp6Header = Packet->Buffer + Packet->DataOffset;
    Checksum = NetChecksumPseudoHeaderAndData(ReceiveContext->Network,
                                              Icmp6Header,
                                              PacketSize,
                                              ReceiveContext->Source,
                                              ReceiveContext->Destination,
                                              SOCKET_INTERNET_PROTOCOL_ICMP6);

    if (Checksum != 0) {
        RtlDebugPrint("ICMP6: Invalid checksum 0x%04x.\n", Checksum);
        return;
    }

    //
    // Act according to the ICMPv6 message.
    //

    Packet->DataOffset += sizeof(ICMP6_HEADER);
    switch (Icmp6Header->Type) {
    case ICMP6_MESSAGE_TYPE_MLD_QUERY:
    case ICMP6_MESSAGE_TYPE_MLD_REPORT:
    case ICMP6_MESSAGE_TYPE_MLD_DONE:
    case ICMP6_MESSAGE_TYPE_MLD2_REPORT:
        NetpMldProcessReceivedData(ReceiveContext);
        break;

    case ICMP6_MESSAGE_TYPE_NDP_ROUTER_SOLICITATION:
    case ICMP6_MESSAGE_TYPE_NDP_ROUTER_ADVERTISEMENT:
    case ICMP6_MESSAGE_TYPE_NDP_NEIGHBOR_SOLICITATION:
    case ICMP6_MESSAGE_TYPE_NDP_NEIGHBOR_ADVERTISEMENT:
    case ICMP6_MESSAGE_TYPE_NDP_REDIRECT:
        NetpNdpProcessReceivedData(ReceiveContext);
        break;

    default:
        break;
    }

    return;
}

KSTATUS
NetpIcmp6ProcessReceivedSocketData (
    PNET_SOCKET Socket,
    PNET_RECEIVE_CONTEXT ReceiveContext
    )

/*++

Routine Description:

    This routine is called for a particular socket to process a received packet
    that was sent to it.

Arguments:

    Socket - Supplies a pointer to the socket that received the packet.

    ReceiveContext - Supplies a pointer to the receive context that stores the
        link, packet, network, protocol, and source and destination addresses.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIcmp6Receive (
    BOOL FromKernelMode,
    PNET_SOCKET Socket,
    PSOCKET_IO_PARAMETERS Parameters,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine is called by the user to receive data from the socket on a
    particular protocol.

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

    return STATUS_NOT_SUPPORTED_BY_PROTOCOL;
}

KSTATUS
NetpIcmp6GetSetInformation (
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

    STATUS_NOT_HANDLED if the protocol does not override the default behavior
        for a basic socket option.

--*/

{

    PICMP6_ADDRESS_CONFIGURATION_REQUEST ConfigurationRequest;
    SOCKET_ICMP6_OPTION Icmp6;
    PIP6_ADDRESS MulticastAddress;
    PNET_NETWORK_MULTICAST_REQUEST MulticastRequest;
    UINTN RequiredSize;
    PVOID Source;
    KSTATUS Status;

    if (InformationType != SocketInformationIcmp6) {
        Status = STATUS_INVALID_PARAMETER;
        goto Icmp6GetSetInformationEnd;
    }

    RequiredSize = 0;
    Source = NULL;
    Status = STATUS_SUCCESS;
    Icmp6 = (SOCKET_ICMP6_OPTION)Option;
    switch (Icmp6) {
    case SocketIcmp6OptionJoinMulticastGroup:
    case SocketIcmp6OptionLeaveMulticastGroup:
        if (Set == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

        RequiredSize = sizeof(NET_NETWORK_MULTICAST_REQUEST);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        MulticastRequest = (PNET_NETWORK_MULTICAST_REQUEST)Data;
        MulticastAddress = (PIP6_ADDRESS)MulticastRequest->MulticastAddress;
        if ((MulticastAddress->Domain != NetDomainIp6) ||
            (!IP6_IS_MULTICAST_ADDRESS(MulticastAddress->Address))) {

            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        if (Icmp6 == SocketIcmp6OptionJoinMulticastGroup) {
            Status = NetpMldJoinMulticastGroup(MulticastRequest);

        } else {
            Status = NetpMldLeaveMulticastGroup(MulticastRequest);
        }

        break;

    case SocketIcmp6OptionConfigureAddress:
        if (Set == FALSE) {
            Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
            break;
        }

        RequiredSize = sizeof(ICMP6_ADDRESS_CONFIGURATION_REQUEST);
        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        ConfigurationRequest = (PICMP6_ADDRESS_CONFIGURATION_REQUEST)Data;
        Status = NetpNdpConfigureAddress(ConfigurationRequest->Link,
                                         ConfigurationRequest->LinkAddress,
                                         ConfigurationRequest->Configure);

        break;

    default:
        Status = STATUS_NOT_SUPPORTED_BY_PROTOCOL;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto Icmp6GetSetInformationEnd;
    }

    //
    // Truncate all copies for get requests down to the required size and
    // always return the required size on set requests.
    //

    if (*DataSize > RequiredSize) {
        *DataSize = RequiredSize;
    }

    //
    // For get requests, copy the gathered information to the supplied data
    // buffer.
    //

    if (Set == FALSE) {

        ASSERT(Source != NULL);

        RtlCopyMemory(Data, Source, *DataSize);

        //
        // If the copy truncated the data, report that the given buffer was too
        // small. The caller can choose to ignore this if the truncated data is
        // enough.
        //

        if (*DataSize < RequiredSize) {
            *DataSize = RequiredSize;
            Status = STATUS_BUFFER_TOO_SMALL;
            goto Icmp6GetSetInformationEnd;
        }
    }

Icmp6GetSetInformationEnd:
    return Status;
}

KSTATUS
NetpIcmp6UserControl (
    PNET_SOCKET Socket,
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

    return STATUS_NOT_SUPPORTED;
}

//
// --------------------------------------------------------- Internal Functions
//

