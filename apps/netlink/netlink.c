/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    netlink.c

Abstract:

    This module implements the netlink library functions.

Author:

    Chris Stevens 24-Mar-2016

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "netlinkp.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns the aligned size of the netlink message buffer structure.
//

#define NETLINK_MESSAGE_BUFFER_SIZE \
    NETLINK_ALIGN(sizeof(NETLINK_MESSAGE_BUFFER))

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
NetlinkConvertToNetworkAddress (
    const struct sockaddr *Address,
    socklen_t AddressLength,
    PNETWORK_ADDRESS NetworkAddress
    );

/*++

Routine Description:

    This routine converts a sockaddr address structure into a network address
    structure.

Arguments:

    Address - Supplies a pointer to the address structure.

    AddressLength - Supplies the length of the address structure in bytes.

    NetworkAddress - Supplies a pointer where the corresponding network address
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_ADDRESS on failure.

--*/

KSTATUS
NetlinkConvertFromNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    struct sockaddr *Address,
    socklen_t *AddressLength
    );

/*++

Routine Description:

    This routine converts a network address structure into a sockaddr structure.

Arguments:

    NetworkAddress - Supplies a pointer to the network address to convert.

    Address - Supplies a pointer where the address structure will be returned.

    AddressLength - Supplies a pointer that on input contains the length of the
        specified Address structure, and on output returns the length of the
        returned address. If the supplied buffer is not big enough to hold the
        address, the address is truncated, and the larger needed buffer size
        will be returned here.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the address buffer is not big enough.

    STATUS_INVALID_ADDRESS on failure.

--*/

//
// -------------------------------------------------------------------- Globals
//

CL_NETWORK_CONVERSION_INTERFACE NetlinkAddressConversionInterface = {
    CL_NETWORK_CONVERSION_INTERFACE_VERSION,
    AF_NETLINK,
    NetDomainNetlink,
    NetlinkConvertToNetworkAddress,
    NetlinkConvertFromNetworkAddress
};

//
// ------------------------------------------------------------------ Functions
//

NETLINK_API
VOID
NetlinkInitialize (
    PVOID Environment
    )

/*++

Routine Description:

    This routine initializes the Minoca Netlink Library. This routine is
    normally called by statically linked assembly within a program, and unless
    developing outside the usual paradigm should not need to call this routine
    directly.

Arguments:

    Environment - Supplies a pointer to the environment information.

Return Value:

    None.

--*/

{

    ClRegisterTypeConversionInterface(ClConversionNetworkAddress,
                                      &NetlinkAddressConversionInterface,
                                      TRUE);

    return;
}

NETLINK_API
INT
NetlinkCreateSocket (
    ULONG Protocol,
    ULONG PortId,
    PNETLINK_SOCKET *NewSocket
    )

/*++

Routine Description:

    This routine creates a netlink socket with the given protocol and port ID.

Arguments:

    Protocol - Supplies the netlink protocol to use for the socket.

    PortId - Supplies a specific port ID to use for the socket, if available.
        Supply NETLINK_ANY_PORT_ID to have the socket dynamically bind to an
        available port ID.

    NewSocket - Supplies a pointer that receives a pointer to the newly created
        socket.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    socklen_t AddressLength;
    PNETLINK_SOCKET Socket;
    INT Status;

    Socket = malloc(sizeof(NETLINK_SOCKET));
    if (Socket == NULL) {
        errno = ENOMEM;
        return -1;
    }

    memset(Socket, 0, sizeof(NETLINK_SOCKET));
    Status = NetlinkAllocateBuffer(0,
                                   NETLINK_SCRATCH_BUFFER_SIZE,
                                   0,
                                   &(Socket->ReceiveBuffer));

    if (Status == -1) {
        goto CreateSocketEnd;
    }

    Socket->Protocol = Protocol;
    Socket->Socket = socket(AF_NETLINK, SOCK_DGRAM, Protocol);
    if (Socket->Socket == -1) {
        Status = -1;
        goto CreateSocketEnd;
    }

    //
    // Bind the socket. If the supplied port ID is NETLINK_ANY_PORT_ID, then
    // an ephemeral port will be assign. Otherwise the socket will be bound to
    // the given port if it's available.
    //

    AddressLength = sizeof(struct sockaddr_nl);
    Socket->LocalAddress.nl_family = AF_NETLINK;
    Socket->LocalAddress.nl_pid = PortId;
    Status = bind(Socket->Socket,
                  (struct sockaddr *)&(Socket->LocalAddress),
                  AddressLength);

    if (Status == -1) {
        goto CreateSocketEnd;
    }

    Status = getsockname(Socket->Socket,
                         (struct sockaddr *)&(Socket->LocalAddress),
                         &AddressLength);

    if (Status == -1) {
        goto CreateSocketEnd;
    }

    ASSERT((Socket->LocalAddress.nl_pid != NETLINK_ANY_PORT_ID) &&
           (Socket->LocalAddress.nl_pid != NETLINK_KERNEL_PORT_ID));

CreateSocketEnd:
    if (Status == -1) {
        if (Socket != NULL) {
            NetlinkDestroySocket(Socket);
            Socket = NULL;
        }
    }

    *NewSocket = Socket;
    return Status;
}

NETLINK_API
VOID
NetlinkDestroySocket (
    PNETLINK_SOCKET Socket
    )

/*++

Routine Description:

    This routine destroys a netlink socket and all its resources.

Arguments:

    Socket - Supplies a pointer to the netlink socket to destroy.

Return Value:

    None.

--*/

{

    if (Socket->Socket != -1) {
        close(Socket->Socket);
    }

    if (Socket->ReceiveBuffer != NULL) {
        NetlinkFreeBuffer(Socket->ReceiveBuffer);
    }

    free(Socket);
    return;
}

NETLINK_API
INT
NetlinkAllocateBuffer (
    ULONG HeaderSize,
    ULONG Size,
    ULONG FooterSize,
    PNETLINK_MESSAGE_BUFFER *NewBuffer
    )

/*++

Routine Description:

    This routine allocates a netlink message buffer. It always adds on space
    for the base netlink message header.

Arguments:

    HeaderSize - Supplies the number of header bytes needed, not including the
        base netlink message header.

    Size - Supplies the number of data bytes needed.

    FooterSize - Supplies the number of footer bytes needed.

    NewBuffer - Supplies a pointer where a pointer to the new allocation will
        be returned on success.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    ULONG AllocationSize;
    PNETLINK_MESSAGE_BUFFER Buffer;
    INT Status;
    ULONG TotalSize;

    Buffer = NULL;
    Status = -1;
    if ((HeaderSize > (HeaderSize + NETLINK_HEADER_LENGTH)) ||
        (Size > NETLINK_ALIGN(Size))) {

        errno = EINVAL;
        goto AllocateBufferEnd;
    }

    HeaderSize += NETLINK_HEADER_LENGTH;
    Size = NETLINK_ALIGN(Size);
    TotalSize = HeaderSize + Size;
    if (TotalSize < Size) {
        errno = EINVAL;
        goto AllocateBufferEnd;
    }

    TotalSize += FooterSize;
    if (TotalSize < FooterSize) {
        errno = EINVAL;
        goto AllocateBufferEnd;
    }

    AllocationSize = TotalSize + NETLINK_MESSAGE_BUFFER_SIZE;
    Buffer = malloc(AllocationSize);
    if (Buffer == NULL) {
        errno = ENOMEM;
        goto AllocateBufferEnd;
    }

    Buffer->Buffer = Buffer + NETLINK_MESSAGE_BUFFER_SIZE;
    Buffer->BufferSize = TotalSize;
    Buffer->DataOffset = HeaderSize;
    Buffer->FooterOffset = HeaderSize + Size;
    Status = 0;

AllocateBufferEnd:
    if (Status != 0) {
        if (Buffer != NULL) {
            free(Buffer);
            Buffer = NULL;
        }
    }

    *NewBuffer = Buffer;
    return Status;
}

NETLINK_API
VOID
NetlinkFreeBuffer (
    PNETLINK_MESSAGE_BUFFER Buffer
    )

/*++

Routine Description:

    This routine frees a previously allocated netlink message buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer to be released.

Return Value:

    None.

--*/

{

    free(Buffer);
    return;
}

NETLINK_API
INT
NetlinkFillOutHeader (
    PNETLINK_SOCKET Socket,
    PNETLINK_MESSAGE_BUFFER Message,
    ULONG DataLength,
    USHORT Type,
    USHORT Flags
    )

/*++

Routine Description:

    This routine fills out the netlink message header that's going to be sent.
    It will make sure there is enough room left in the supplied message buffer
    and add the header before the current data offset.

Arguments:

    Socket - Supplies a pointer to the netlink socket that is sending the
        message.

    Message - Supplies a pointer to the netlink message buffer for which the
        header should be filled out.

    DataLength - Supplies the length of the message data payload, in bytes.

    Type - Supplies the netlink message type.

    Flags - Supplies a bitmask of netlink message flags. See
        NETLINK_HEADER_FLAG_* for definitions.

Return Value:

    0 on success.

    -1 on error, and the errno variable will be set to contain more information.

--*/

{

    PNETLINK_HEADER Header;

    if (Message->DataOffset < NETLINK_HEADER_LENGTH) {
        errno = ENOBUFS;
        return -1;
    }

    if (((Message->FooterOffset - Message->DataOffset) < DataLength) ||
        ((DataLength + NETLINK_HEADER_LENGTH) < DataLength)) {

        errno = EMSGSIZE;
        return -1;
    }

    Message->DataOffset -= NETLINK_HEADER_LENGTH;
    Header = Message->Buffer + Message->DataOffset;
    Header->Length = DataLength + NETLINK_HEADER_LENGTH;
    Header->Type = Type;
    Header->Flags = Flags;
    Header->SequenceNumber = RtlAtomicAdd32(&(Socket->SendNextSequence), 1);
    Header->PortId = Socket->LocalAddress.nl_pid;
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
NetlinkConvertToNetworkAddress (
    const struct sockaddr *Address,
    socklen_t AddressLength,
    PNETWORK_ADDRESS NetworkAddress
    )

/*++

Routine Description:

    This routine converts a sockaddr address structure into a network address
    structure.

Arguments:

    Address - Supplies a pointer to the address structure.

    AddressLength - Supplies the length of the address structure in bytes.

    NetworkAddress - Supplies a pointer where the corresponding network address
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_ADDRESS on failure.

--*/

{

    struct sockaddr_nl *NetlinkAddress;

    if ((AddressLength < sizeof(struct sockaddr_nl)) ||
        (Address->sa_family != AF_NETLINK)) {

        return STATUS_INVALID_ADDRESS;
    }

    NetlinkAddress = (struct sockaddr_nl *)Address;
    NetworkAddress->Domain = NetDomainNetlink;
    NetworkAddress->Port = NetlinkAddress->nl_pid;
    NetworkAddress->Address[0] = (UINTN)NetlinkAddress->nl_groups;
    return STATUS_SUCCESS;
}

KSTATUS
NetlinkConvertFromNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    struct sockaddr *Address,
    socklen_t *AddressLength
    )

/*++

Routine Description:

    This routine converts a network address structure into a sockaddr structure.

Arguments:

    NetworkAddress - Supplies a pointer to the network address to convert.

    Address - Supplies a pointer where the address structure will be returned.

    AddressLength - Supplies a pointer that on input contains the length of the
        specified Address structure, and on output returns the length of the
        returned address. If the supplied buffer is not big enough to hold the
        address, the address is truncated, and the larger needed buffer size
        will be returned here.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the address buffer is not big enough.

    STATUS_INVALID_ADDRESS on failure.

--*/

{

    UINTN CopySize;
    struct sockaddr_nl NetlinkAddress;
    KSTATUS Status;

    if (NetworkAddress->Domain != NetDomainNetlink) {
        return STATUS_INVALID_ADDRESS;
    }

    Status = STATUS_SUCCESS;
    NetlinkAddress.nl_family = AF_NETLINK;
    NetlinkAddress.nl_pad = 0;
    NetlinkAddress.nl_pid = NetworkAddress->Port;
    NetlinkAddress.nl_groups = (ULONG)NetworkAddress->Address[0];
    CopySize = sizeof(NetlinkAddress);
    if (CopySize > *AddressLength) {
        CopySize = *AddressLength;
        Status = STATUS_BUFFER_TOO_SMALL;
    }

    RtlCopyMemory(Address, &NetlinkAddress, CopySize);
    *AddressLength = sizeof(NetlinkAddress);
    return Status;
}

