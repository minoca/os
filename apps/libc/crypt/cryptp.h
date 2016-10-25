/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cryptp.h

Abstract:

    This header contains internal definitions for the C crypt library.

Author:

    Evan Green 6-Mar-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#define LIBCRYPT_API __DLLEXPORT

//
// --------------------------------------------------------------------- Macros
//

//
// This macro zeros memory and ensures that the compiler doesn't optimize away
// the memset.
//

#define SECURITY_ZERO(_Buffer, _Size)                                       \
    {                                                                       \
        memset((_Buffer), 0, (_Size));                                      \
        *(volatile char *)(_Buffer) = *((volatile char *)(_Buffer) + 1);    \
    }

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
