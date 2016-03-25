/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    netlink.h

Abstract:

    This header contains definitions for the Minoca Netlink Library.

Author:

    Chris Stevens 24-Mar-2016

--*/

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

#ifndef NETLINK_API

#define NETLINK_API DLLIMPORT

#endif

//
// Define the netlink address family.
//

#define AF_NETLINK 4

//
// ------------------------------------------------------ Data Type Definitions
//

struct sockaddr_nl {
    sa_family_t nl_family;
    unsigned short nl_pad;
    pid_t nl_pid;
    uint32_t nl_groups;
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

