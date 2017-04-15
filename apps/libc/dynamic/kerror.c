/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kerror.c

Abstract:

    This module implements the conversion between a KSTATUS code and a C
    library errno code.

Author:

    Evan Green 2-Jun-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

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
// Define the array that converts a KSTATUS code to an error number. Please
// keep this table up to date as new kernel status codes are defined.
//

int ClpStatusToErrorNumber[] = {
    0,
    EINVAL,
    ENOENT,
    EINVAL,
    EINTR,
    EFAULT,
    EPIPE,
    ENOMEM,
    EBADF,
    EINVAL,
    EINVAL,         /* 10 */
    ENOMEM,
    EIO,
    EINVAL,
    ENODEV,
    ENOENT,
    ENOENT,
    0,
    ERANGE,
    EINVAL,
    ENAMETOOLONG,   /* 20 */
    ENOEXEC,
    ENOTSUP,
    ENOMEM,
    EEXIST,
    ENODEV,
    ENODEV,
    EILSEQ,
    EILSEQ,
    EINVAL,
    EINVAL,         /* 30 */
    ENOSYS,
    EINVAL,
    ENOBUFS,
    EACCES,
    ENOBUFS,
    EIO,
    EINVAL,
    EISDIR,
    ENOTDIR,
    EIO,            /* 40 */
    ENOSPC,
    EEXIST,
    ENOENT,
    EBUSY,
    EINVAL,
    ERANGE,
    ERANGE,
    EILSEQ,
    EIO,
    EAGAIN,         /* 50 */
    EINVAL,
    EINVAL,
    EDOM,
    EINVAL,
    ETIME,
    EIO,
    EAGAIN,
    ENOBUFS,
    EINVAL,
    EILSEQ,         /* 60 */
    ECANCELED,
    EWOULDBLOCK,
    EOVERFLOW,
    ENOSYS,
    EIO,
    EILSEQ,
    ESRCH,
    ESRCH,
    EADDRNOTAVAIL,
    ENETDOWN,       /* 70 */
    ENETUNREACH,
    ECONNRESET,
    EISCONN,
    ECONNREFUSED,
    ECONNREFUSED,
    EADDRINUSE,
    ENOTSOCK,
    EWOULDBLOCK,
    EAGAIN,
    EILSEQ,         /* 80 */
    ERANGE,
    EAGAIN,
    EAGAIN,
    EILSEQ,
    ENOTTY,
    ENODEV,
    ENOTEMPTY,
    EXDEV,
    EINVAL,
    EINVAL,         /* 90 */
    EINVAL,
    ECHILD,
    EILSEQ,
    ENFILE,
    ENOTBLK,
    ENODEV,
    EALREADY,
    ENOMEM,
    ENOPROTOOPT,
    EMSGSIZE,       /* 100 */
    ENOTCONN,
    EDESTADDRREQ,
    EPERM,
    ELOOP,
    EPIPE,
    ENXIO,
    EAFNOSUPPORT,
    EPROTONOSUPPORT,
    EDOM,
    ENODEV,         /* 110 */
    EDEADLK,
    EINTR,
    EINTR
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
INT
ClConvertKstatusToErrorNumber (
    KSTATUS Status
    )

/*++

Routine Description:

    This routine converts a status code to a C library error number.

Arguments:

    Status - Supplies the status code.

Return Value:

    Returns the appropriate error number for the given status code.

--*/

{

    ULONG ElementCount;
    INT Error;
    ULONG Index;

    ElementCount = sizeof(ClpStatusToErrorNumber) /
                   sizeof(ClpStatusToErrorNumber[0]);

    Index = KSTATUS_CODE(Status);
    if (Index >= ElementCount) {
        RtlDebugPrint("Error: Could not convert status %d to error number. A "
                      "developer needs to update the errno table.\n",
                      Status);

        assert(FALSE);

        return EINVAL;
    }

    Error = ClpStatusToErrorNumber[Index];
    return Error;
}

//
// --------------------------------------------------------- Internal Functions
//

