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

#include <osbase.h>
#include <mlibc.h>

#include <stdio.h>

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

