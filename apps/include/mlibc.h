/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    mlibc.h

Abstract:

    This header contains definitions for implementation-specific functions
    provided by the Minoca C library.

Author:

    Evan Green 19-Nov-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <sys/socket.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
INT
ClConvertKstatusToErrorNumber (
    KSTATUS Status
    );

/*++

Routine Description:

    This routine converts a status code to a C library error number.

Arguments:

    Status - Supplies the status code.

Return Value:

    Returns the appropriate error number for the given status code.

--*/

LIBC_API
KSTATUS
ClConvertToNetworkAddress (
    const struct sockaddr *Address,
    UINTN AddressLength,
    PNETWORK_ADDRESS NetworkAddress,
    PSTR *Path,
    PUINTN PathSize
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

    Path - Supplies an optional pointer where a pointer to the path will be
        returned if this is a Unix address.

    PathSize - Supplies an optional pointer where the path size will be
        returned if this is a Unix address.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_ADDRESS on failure.

--*/

LIBC_API
KSTATUS
ClConvertFromNetworkAddress (
    PNETWORK_ADDRESS NetworkAddress,
    struct sockaddr *Address,
    socklen_t *AddressLength,
    PSTR Path,
    UINTN PathSize
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

    Path - Supplies the path, if this is a local Unix address.

    PathSize - Supplies the size of the path, if this is a local Unix address.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the address buffer is not big enough.

    STATUS_INVALID_ADDRESS on failure.

--*/

#ifdef __cplusplus

}

#endif

