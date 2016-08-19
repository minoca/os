/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    netlinkp.h

Abstract:

    This header contains internal definitions for the Minoca netlink library.

Author:

    Chris Stevens 24-Mar-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#define LIBNETLINK_API __DLLEXPORT

#include <minoca/lib/minocaos.h>
#include <minoca/net/netdrv.h>
#include <minoca/net/netlink.h>
#include <minoca/lib/mlibc.h>
#include <minoca/lib/netlink.h>

//
// ---------------------------------------------------------------- Definitions
//

#define NETLINK_SCRATCH_BUFFER_SIZE (4096 - NETLINK_HEADER_LENGTH)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

