/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    if_dl.h

Abstract:

    This header contains definitions for link-layer socket addresses in the
    C Library.

Author:

    Chris Stevens 26-Jan-2017

--*/

#ifndef _IF_DL_H
#define _IF_DL_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

#ifdef __cplusplus

extern "C" {

#endif

//
// --------------------------------------------------------------------- Macros
//

#define LLADDR(_SocketAddress) \
    (void *)((_SocketAddress)->sdl_data + (_SocketAddress)->sdl_nlen)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a link-layer socket address.

Members:

    sdl_family - Stores the family name, which is always AF_LINK for link-layer
        addresses.

    sdl_len - Stores the total length of this socket address.

    sdl_index - Stores the system interface index if not set to zero.

    sdl_type - Stores the network interface type.

    sdl_nlen - Stores the length of the network interface name.

    sdl_alen - Stores the length of the link-layer address.

    sdl_slen - Stores the length of the link-layer selector.

    sdl_data - Stores the minimum data area that holds the socket name and
        address. This may be larger if necessary.

--*/

struct sockaddr_dl {
    sa_family_t sdl_family;
    u_char sdl_len;
    u_char sdl_index;
    u_char sdl_type;
    u_char sdl_nlen;
    u_char sdl_alen;
    u_char sdl_slen;
    u_char sdl_data[46];
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#ifdef __cplusplus

}

#endif
#endif

