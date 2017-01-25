/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ifaddrs.h

Abstract:

    This header contains definitions for getting network interface addresses in
    the C Library.

Author:

    Chris Stevens 24-Jan-2017

--*/

#ifndef _IFADDRS_H
#define _IFADDRS_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a network interface.

Members:

    ifa_next - Stores a pointer to the next interface structure.

    ifa_name - Stores the null-terminated name of the interface.

    ifa_flags - Stores a bitmask of network interface flags. See IFF_* for
        definitions.

    ifa_addr - Stores a pointer to the network interface's address.

    ifa_netmask - Stores a pointer to the network interface's mask.

    ifa_broadaddr - Stores a pointer to the network interface's broadcast
        address.

    ifa_dstaddr - Stores a pointer to the network interface's P2P destination
        address.

    ifa_data - Stores a pointer to address family specific data.

--*/

struct ifaddrs {
    struct ifaddrs *ifa_next;
    char *ifa_name;
    u_int ifa_flags;
    struct sockaddr *ifa_addr;
    struct sockaddr *ifa_netmask;
    struct sockaddr *ifa_broadaddr;
    struct sockaddr *ifa_dstaddr;
    void *ifa_data;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
getifaddrs (
    struct ifaddrs **Interfaces
    );

/*++

Routine Description:

    This routine creates a linked list of network interfaces structures
    describing all of the network interfaces on the local system.

Arguments:

    Interfaces - Supplies a pointer that receives a pointer to the linked list
        of network interfaces.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

LIBC_API
void
freeifaddrs (
    struct ifaddrs *Interfaces
    );

/*++

Routine Description:

    This routine releases a list of network interfaces.

Arguments:

    Interfaces - Supplies a pointer to the list of network interfaces to
        release.

Return Value:

    None.

--*/

#ifdef __cplusplus

}

#endif
#endif

