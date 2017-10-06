/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    utime.h

Abstract:

    This header contains definitions for modifying file modification and
    access times.

Author:

    Evan Green 3-Jul-2013

--*/

#ifndef _UTIME_H
#define _UTIME_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the times buffer used to pass a file modification and
    access time around together.

Members:

    actime - Stores the file access time.

    modtime - Stores the file modification time.

--*/

struct utimbuf {
    time_t actime;
    time_t modtime;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
utime (
    const char *Path,
    const struct utimbuf *Times
    );

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional pointer to a time buffer structure containing
        the access and modification times to set. If this parameter is NULL,
        the current time will be used.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

