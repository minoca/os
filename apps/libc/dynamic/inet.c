/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    inet.c

Abstract:

    This module implements support for internet helper operations.

Author:

    Evan Green 3-May-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <arpa/inet.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
uint32_t
htonl (
    uint32_t HostValue
    )

/*++

Routine Description:

    This routine converts a 32-bit value from host order to network order.

Arguments:

    HostValue - Supplies the integer to convert into network order.

Return Value:

    Returns the given integer in network order.

--*/

{

    return RtlByteSwapUlong(HostValue);
}

LIBC_API
uint32_t
ntohl (
    uint32_t NetworkValue
    )

/*++

Routine Description:

    This routine converts a 32-bit value from network order to host order.

Arguments:

    NetworkValue - Supplies the integer to convert into host order.

Return Value:

    Returns the given integer in host order.

--*/

{

    return RtlByteSwapUlong(NetworkValue);
}

LIBC_API
uint16_t
htons (
    uint16_t HostValue
    )

/*++

Routine Description:

    This routine converts a 16-bit value from host order to network order.

Arguments:

    HostValue - Supplies the integer to convert into network order.

Return Value:

    Returns the given integer in network order.

--*/

{

    return RtlByteSwapUshort(HostValue);
}

LIBC_API
uint16_t
ntohs (
    uint16_t NetworkValue
    )

/*++

Routine Description:

    This routine converts a 16-bit value from network order to host order.

Arguments:

    NetworkValue - Supplies the integer to convert into host order.

Return Value:

    Returns the given integer in host order.

--*/

{

    return RtlByteSwapUshort(NetworkValue);
}

//
// --------------------------------------------------------- Internal Functions
//

