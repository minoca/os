/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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
    EINVAL,
    ENOMEM,
    EIO,
    EINVAL,
    ENODEV,
    ENOENT,
    ENOENT,         /* 10 */
    0,
    ERANGE,
    EINVAL,
    ENAMETOOLONG,
    ENOEXEC,
    ENOTSUP,
    ENOMEM,
    EEXIST,
    ENODEV,
    ENODEV,
    EILSEQ,
    EILSEQ,
    EINVAL,
    EINVAL,
    ENOSYS,
    EINVAL,         /* 20 */
    ENOBUFS,
    EACCES,
    ENOBUFS,
    EIO,
    EINVAL,
    EISDIR,
    ENOTDIR,
    EIO,
    ENOSPC,
    EEXIST,
    ENOENT,
    EBUSY,
    EINVAL,
    ERANGE,
    ERANGE,
    EILSEQ,         /* 30 */
    EIO,
    EAGAIN,
    EINVAL,
    EINVAL,
    EDOM,
    EINVAL,
    ETIME,
    EIO,
    EAGAIN,
    ENOBUFS,
    EINVAL,
    EILSEQ,
    ECANCELED,
    EWOULDBLOCK,
    EOVERFLOW,
    ENOSYS,         /* 40 */
    EIO,
    EILSEQ,
    ESRCH,
    ESRCH,
    EADDRNOTAVAIL,
    ENETDOWN,
    ENETUNREACH,
    ECONNRESET,
    EISCONN,
    ECONNREFUSED,
    ECONNREFUSED,
    EADDRINUSE,
    ENOTSOCK,
    EWOULDBLOCK,
    EAGAIN,
    EILSEQ,         /* 50 */
    EOVERFLOW,
    EAGAIN,
    EAGAIN,
    EILSEQ,
    ENOTTY,
    ENODEV,
    ENOTEMPTY,
    EXDEV,
    EINVAL,
    EINVAL,
    EINVAL,
    ECHILD,
    EILSEQ,
    ENFILE,
    ENOTBLK,
    ENODEV,         /* 60 */
    EALREADY,
    ENOMEM,
    ENOPROTOOPT,
    EMSGSIZE,
    ENOTCONN,
    EDESTADDRREQ,
    EPERM,
    ELOOP,
    EPIPE,
    ENXIO,
    EAFNOSUPPORT,
    EPROTONOSUPPORT,
    EDOM,
    ENODEV
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
        RtlDebugPrint("Error: Could not convert status %X to error number. A "
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

