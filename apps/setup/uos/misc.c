/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    misc.c

Abstract:

    This module implements miscellaneous OS support functions for the setup
    application.

Author:

    Evan Green 19-Jan-2016

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
#include <sys/wait.h>
#include <unistd.h>

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

    pid_t Child;
    int Status;

    Child = fork();
    if (Child == 0) {
        execlp("/sbin/reboot", "/sbin/reboot", NULL);
        exit(1);

    } else if (Child == -1) {
        fprintf(stderr, "Failed to fork.\n");
        return errno;

    } else {
        waitpid(Child, &Status, 0);
        if (!WIFEXITED(Status)) {
            fprintf(stderr, "Failed to execute /sbin/reboot.\n");
        }
    }

    return errno;
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

    //
    // TODO: Consider common-izing the Minoca code that looks through the
    // SMBIOS tables, which can be accessed at on Linux with root access at
    // /sys/firmware/dmi/tables/DMI.
    //

    if (access("/sys/firmware/efi", F_OK) != F_OK) {
        *Name = strdup("x86 PC");
        if (Fallback != NULL) {
            *Fallback = SetupRecipePc;
        }

    } else {
        *Name = strdup("x86 UEFI-based PC");
        if (Fallback != NULL) {
            *Fallback = SetupRecipePcEfi;
        }
    }

    if (*Name == NULL) {
        return ENOMEM;
    }

    return 0;
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

    long PageCount;
    long PageSize;

    *Megabytes = 0;
    PageSize = sysconf(_SC_PAGE_SIZE);
    if (PageSize == -1) {
        return errno;
    }

    PageCount = sysconf(_SC_PHYS_PAGES);
    if (PageCount == -1) {
        return errno;
    }

    *Megabytes = ((ULONGLONG)PageSize * (ULONGLONG)PageCount) / 0x100000ULL;
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

