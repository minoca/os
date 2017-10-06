/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    tcp.h

Abstract:

    This header contains definitions specific to the Transmission Control
    Protocol (TCP).

Author:

    Evan Green 18-Oct-2013

--*/

#ifndef _NETINET_TCP_H
#define _NETINET_TCP_H

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// TCP socket options.
//

//
// Set this option to avoid coalescing small segments. This options takes an
// integer.
//

#define TCP_NODELAY 1

//
// Set this option to configure the TCP keep alive timeout. The timeout
// indicates how long, in seconds, connection needs to be idle before sending
// the first keep alive probe. This option takes an integer.
//

#define TCP_KEEPIDLE 2

//
// Set this option to configure the TCP keep alive period. The period indicates
// the time, in seconds, between sending keep alive probes. This options takes
// an integer.
//

#define TCP_KEEPINTVL 3

//
// Set this option to configure the number of unanswered TCP keep alive probes
// that must be sent before the connection is aborted.
//

#define TCP_KEEPCNT 4

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

