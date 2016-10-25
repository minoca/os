/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    misc.c

Abstract:

    This module implements miscellaneous OS support functions for the setup
    application on a Windows host.

Author:

    Evan Green 8-Oct-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "../setup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SetupOsReboot (
    VOID
    )

/*++

Routine Description:

    This routine reboots the machine.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    fprintf(stderr, "Not rebooting on Windows.\n");
    return ENOSYS;
}

INT
SetupOsGetPlatformName (
    PSTR *Name,
    PSETUP_RECIPE_ID Fallback
    )

/*++

Routine Description:

    This routine gets the platform name.

Arguments:

    Name - Supplies a pointer where a pointer to an allocated string containing
        the SMBIOS system information product name will be returned if
        available. The caller is responsible for freeing this memory when done.

    Fallback - Supplies a fallback platform to use if the given platform
        string was not returned or did not match a known platform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    *Name = NULL;
    return ENOSYS;
}

INT
SetupOsGetSystemMemorySize (
    PULONGLONG Megabytes
    )

/*++

Routine Description:

    This routine returns the number of megabytes of memory installed on the
    currently running system.

Arguments:

    Megabytes - Supplies a pointer to where the system memory capacity in
        megabytes will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    *Megabytes = 0;
    return ENOSYS;
}

//
// --------------------------------------------------------- Internal Functions
//

