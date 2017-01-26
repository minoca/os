/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    if_types.h

Abstract:

    This header contains definitions for network interface types in the C
    library.

Author:

    Chris Stevens 26-Jan-2017

--*/

#ifndef _IF_TYPES_H
#define _IF_TYPES_H

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#include <libcbase.h>

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the network interface type.
//

#define IFT_ETHER 1
#define IFT_IEEE80211 2

//
// ------------------------------------------------------ Data Type Definitions
//

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

