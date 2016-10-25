/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    statvfs.c

Abstract:

    This module implements support for getting information about file systems.

Author:

    Evan Green 15-Jan-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <sys/statvfs.h>

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

LIBC_API
int
statvfs (
    const char *Path,
    struct statvfs *Information
    )

/*++

Routine Description:

    This routine returns information about the file system containing the
    given path.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the path
        to a file.

    Information - Supplies a pointer where the file system information will
        be returned.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

{

    //
    // TODO: Implement statvfs.
    //

    errno = ENOSYS;
    return -1;
}

LIBC_API
int
fstatvfs (
    int FileDescriptor,
    struct statvfs *Information
    )

/*++

Routine Description:

    This routine returns information about the file system containing the
    given file descriptor.

Arguments:

    FileDescriptor - Supplies an open file descriptor whose file system
        properties are desired.

    Information - Supplies a pointer where the file system information will
        be returned.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

{

    //
    // TODO: Implement fstatvfs.
    //

    errno = ENOSYS;
    return -1;
}

//
// --------------------------------------------------------- Internal Functions
//

