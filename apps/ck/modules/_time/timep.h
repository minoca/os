/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timep.h

Abstract:

    This header contains definitions for the OS-level time module.

Author:

    Evan Green 5-Jun-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/chalk.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#ifdef _WIN32

#include "timwin32.h"

#elif defined(__APPLE__)

#define HAVE_TM_GMTOFF
#define HAVE_TM_ZONE

#else

#define HAVE_TM_GMTOFF
#define HAVE_TM_NANOSECOND
#define HAVE_TM_ZONE

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Generic time module functions
//

VOID
CkpTimeModuleInit (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine populates the _time module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

