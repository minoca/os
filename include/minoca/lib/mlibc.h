/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
// Define the current version number of the network conversion interface.
//

#define CL_NETWORK_CONVERSION_INTERFACE_VERSION 1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CL_CONVERSION_TYPE {
    ClConversionInvalid,
    ClConversionNetworkAddress
} CL_CONVERSION_TYPE, *PCL_CONVERSION_TYPE;

typedef
KSTATUS
(*PCL_CONVERT_TO_NETWORK_ADDRESS) (
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

typedef
KSTATUS
(*PCL_CONVERT_FROM_NETWORK_ADDRESS) (
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

/*++

Structure Description:

    This structure defines the network conversion interface between C library
    types and Minoca types.

Members:

    Version - Stores the version number of this structure. Set to
        CL_NETWORK_CONVERSION_INTERFACE_VERSION.

    AddressFamily - Supplies the C library address family for the conversion
        interface.

    AddressDomain - Supplies the Minoca address domain for the conversion
        interface.

    ToNetworkAddress - Supplies a pointer to a function that converts a
        sockaddr address structure to a network address structure.

    FromNetworkAddress - Supplies a pointer to a function that converts a
        network address structure to a sockaddr address structure.

--*/

typedef struct _CL_NETWORK_CONVERSION_INTERFACE {
    ULONG Version;
    sa_family_t AddressFamily;
    NET_DOMAIN_TYPE AddressDomain;
    PCL_CONVERT_TO_NETWORK_ADDRESS ToNetworkAddress;
    PCL_CONVERT_FROM_NETWORK_ADDRESS FromNetworkAddress;
} CL_NETWORK_CONVERSION_INTERFACE, *PCL_NETWORK_CONVERSION_INTERFACE;

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
    socklen_t AddressLength,
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

LIBC_API
KSTATUS
ClRegisterTypeConversionInterface (
    CL_CONVERSION_TYPE Type,
    PVOID Interface,
    BOOL Register
    );

/*++

Routine Description:

    This routine registers or de-registers a C library type conversion
    interface.

Arguments:

    Type - Supplies the type of the conversion interface being registered.

    Interface - Supplies a pointer to the conversion interface. This memory
        will not be referenced after the function returns, so this may be a
        stack allocated structure.

    Register - Supplies a boolean indicating whether or not the given interface
        should be registered or de-registered.

Return Value:

    Status code.

--*/

#ifdef __cplusplus

}

#endif

