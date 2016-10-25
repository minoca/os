/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgrtl.h

Abstract:

    This header contains definitions for the runtime library used in the
    debugger client.

Author:

    Evan Green 19-Feb-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define the RTL library functions as exports since they're statically linked
// in here.
//

#define RTL_API __DLLEXPORT

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>

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
