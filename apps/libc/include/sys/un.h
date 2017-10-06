/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    un.h

Abstract:

    This header contains definitions for managing Unix socket addresses.

Author:

    Evan Green 19-Jan-2015

--*/

#ifndef _SYS_UN_H
#define _SYS_UN_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/socket.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the maximum length of a Unix socket path.
//

#define UNIX_PATH_MAX 108

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a Unix socket address.

Members:

    sun_family - Stores the address family, AF_UNIX in this case.

    sun_path - Stores the null-terminated path string. The size of the socket
        address passed around should include the length of this string plus the
        null terminator.

--*/

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[UNIX_PATH_MAX];
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

