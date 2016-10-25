/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uos.c

Abstract:

    This module implements POSIX OS functions for mingen.

Author:

    Evan Green 17-Mar-2016

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#include "mingen.h"

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
MingenOsUname (
    CHAR Flavor,
    PSTR Buffer,
    ULONG Size
    )

/*++

Routine Description:

    This routine implements the OS-specific uname function.

Arguments:

    Flavor - Supplies the flavor of uname to get. Valid values are s, n, r, v,
        and m.

    Buffer - Supplies a buffer where the string will be returned on success.

    Size - Supplies the size of the buffer in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR Source;
    int Status;
    struct utsname UtsName;

    memset(&UtsName, 0, sizeof(UtsName));
    Status = uname(&UtsName);
    if (Status != 0) {
        return errno;
    }

    switch (Flavor) {
    case 's':
        Source = UtsName.sysname;
        break;

    case 'n':
        Source = UtsName.nodename;
        break;

    case 'r':
        Source = UtsName.release;
        break;

    case 'v':
        Source = UtsName.version;
        break;

    case 'm':
        Source = UtsName.machine;
        break;

    default:
        Status = EINVAL;
        return Status;
    }

    if ((Source != NULL) && (Size != 0)) {
        strncpy(Buffer, Source, Size);
        Buffer[Size - 1] = '\0';
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

