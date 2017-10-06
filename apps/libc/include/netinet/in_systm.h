/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    in_systm.h

Abstract:

    This header contains system specific network type definitions.

Author:

    Evan Green 14-Jan-2015

--*/

#ifndef _NETINET_IN_SYSTM_H
#define _NETINET_IN_SYSTM_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define network-order types for compatibility with BSD applications.
//

typedef u_int16_t n_short;
typedef u_int32_t n_long;
typedef u_int32_t n_time;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

