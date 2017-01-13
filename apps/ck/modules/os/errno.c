/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    errno.c

Abstract:

    This module implements OS-specific errno values for the Chalk os module.

Author:

    Evan Green 11-Jan-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <string.h>

#include "osp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpOsStrerror (
    PCK_VM Vm
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkOsErrnoValues[] = {
#ifdef EPERM
    {CkTypeInteger, "EPERM", NULL, EPERM},
#endif
#ifdef ENOENT
    {CkTypeInteger, "ENOENT", NULL, ENOENT},
#endif
#ifdef ESRCH
    {CkTypeInteger, "ESRCH", NULL, ESRCH},
#endif
#ifdef EINTR
    {CkTypeInteger, "EINTR", NULL, EINTR},
#endif
#ifdef EIO
    {CkTypeInteger, "EIO", NULL, EIO},
#endif
#ifdef ENXIO
    {CkTypeInteger, "ENXIO", NULL, ENXIO},
#endif
#ifdef E2BIG
    {CkTypeInteger, "E2BIG", NULL, E2BIG},
#endif
#ifdef ENOEXEC
    {CkTypeInteger, "ENOEXEC", NULL, ENOEXEC},
#endif
#ifdef EBADF
    {CkTypeInteger, "EBADF", NULL, EBADF},
#endif
#ifdef ECHILD
    {CkTypeInteger, "ECHILD", NULL, ECHILD},
#endif
#ifdef EAGAIN
    {CkTypeInteger, "EAGAIN", NULL, EAGAIN},
#endif
#ifdef ENOMEM
    {CkTypeInteger, "ENOMEM", NULL, ENOMEM},
#endif
#ifdef EACCES
    {CkTypeInteger, "EACCES", NULL, EACCES},
#endif
#ifdef EFAULT
    {CkTypeInteger, "EFAULT", NULL, EFAULT},
#endif
#ifdef ENOTBLK
    {CkTypeInteger, "ENOTBLK", NULL, ENOTBLK},
#endif
#ifdef EBUSY
    {CkTypeInteger, "EBUSY", NULL, EBUSY},
#endif
#ifdef EEXIST
    {CkTypeInteger, "EEXIST", NULL, EEXIST},
#endif
#ifdef EXDEV
    {CkTypeInteger, "EXDEV", NULL, EXDEV},
#endif
#ifdef ENODEV
    {CkTypeInteger, "ENODEV", NULL, ENODEV},
#endif
#ifdef ENOTDIR
    {CkTypeInteger, "ENOTDIR", NULL, ENOTDIR},
#endif
#ifdef EISDIR
    {CkTypeInteger, "EISDIR", NULL, EISDIR},
#endif
#ifdef EINVAL
    {CkTypeInteger, "EINVAL", NULL, EINVAL},
#endif
#ifdef ENFILE
    {CkTypeInteger, "ENFILE", NULL, ENFILE},
#endif
#ifdef EMFILE
    {CkTypeInteger, "EMFILE", NULL, EMFILE},
#endif
#ifdef ENOTTY
    {CkTypeInteger, "ENOTTY", NULL, ENOTTY},
#endif
#ifdef ETXTBSY
    {CkTypeInteger, "ETXTBSY", NULL, ETXTBSY},
#endif
#ifdef EFBIG
    {CkTypeInteger, "EFBIG", NULL, EFBIG},
#endif
#ifdef ENOSPC
    {CkTypeInteger, "ENOSPC", NULL, ENOSPC},
#endif
#ifdef ESPIPE
    {CkTypeInteger, "ESPIPE", NULL, ESPIPE},
#endif
#ifdef EROFS
    {CkTypeInteger, "EROFS", NULL, EROFS},
#endif
#ifdef EMLINK
    {CkTypeInteger, "EMLINK", NULL, EMLINK},
#endif
#ifdef EPIPE
    {CkTypeInteger, "EPIPE", NULL, EPIPE},
#endif
#ifdef EDOM
    {CkTypeInteger, "EDOM", NULL, EDOM},
#endif
#ifdef ERANGE
    {CkTypeInteger, "ERANGE", NULL, ERANGE},
#endif
#ifdef EDEADLOCK
    {CkTypeInteger, "EDEADLOCK", NULL, EDEADLOCK},
#endif
#ifdef ENAMETOOLONG
    {CkTypeInteger, "ENAMETOOLONG", NULL, ENAMETOOLONG},
#endif
#ifdef ENOLCK
    {CkTypeInteger, "ENOLCK", NULL, ENOLCK},
#endif
#ifdef ENOSYS
    {CkTypeInteger, "ENOSYS", NULL, ENOSYS},
#endif
#ifdef ENOTEMPTY
    {CkTypeInteger, "ENOTEMPTY", NULL, ENOTEMPTY},
#endif
#ifdef ELOOP
    {CkTypeInteger, "ELOOP", NULL, ELOOP},
#endif
#ifdef EWOULDBLOCK
    {CkTypeInteger, "EWOULDBLOCK", NULL, EWOULDBLOCK},
#endif
#ifdef ENOMSG
    {CkTypeInteger, "ENOMSG", NULL, ENOMSG},
#endif
#ifdef EIDRM
    {CkTypeInteger, "EIDRM", NULL, EIDRM},
#endif
#ifdef ENOTSUP
    {CkTypeInteger, "ENOTSUP", NULL, ENOTSUP},
#endif
#ifdef EOWNERDEAD
    {CkTypeInteger, "EOWNERDEAD", NULL, EOWNERDEAD},
#endif
#ifdef ENOTRECOVERABLE
    {CkTypeInteger, "ENOTRECOVERABLE", NULL, ENOTRECOVERABLE},
#endif
#ifdef ENOSTR
    {CkTypeInteger, "ENOSTR", NULL, ENOSTR},
#endif
#ifdef ENODATA
    {CkTypeInteger, "ENODATA", NULL, ENODATA},
#endif
#ifdef ETIME
    {CkTypeInteger, "ETIME", NULL, ETIME},
#endif
#ifdef ENOSR
    {CkTypeInteger, "ENOSR", NULL, ENOSR},
#endif
#ifdef ENOLINK
    {CkTypeInteger, "ENOLINK", NULL, ENOLINK},
#endif
#ifdef EPROTO
    {CkTypeInteger, "EPROTO", NULL, EPROTO},
#endif
#ifdef EMULTIHOP
    {CkTypeInteger, "EMULTIHOP", NULL, EMULTIHOP},
#endif
#ifdef EBADMSG
    {CkTypeInteger, "EBADMSG", NULL, EBADMSG},
#endif
#ifdef EOVERFLOW
    {CkTypeInteger, "EOVERFLOW", NULL, EOVERFLOW},
#endif
#ifdef EILSEQ
    {CkTypeInteger, "EILSEQ", NULL, EILSEQ},
#endif
#ifdef ENOTSOCK
    {CkTypeInteger, "ENOTSOCK", NULL, ENOTSOCK},
#endif
#ifdef EDESTADDRREQ
    {CkTypeInteger, "EDESTADDRREQ", NULL, EDESTADDRREQ},
#endif
#ifdef EMSGSIZE
    {CkTypeInteger, "EMSGSIZE", NULL, EMSGSIZE},
#endif
#ifdef EPROTOTYPE
    {CkTypeInteger, "EPROTOTYPE", NULL, EPROTOTYPE},
#endif
#ifdef ENOPROTOOPT
    {CkTypeInteger, "ENOPROTOOPT", NULL, ENOPROTOOPT},
#endif
#ifdef EPROTONOSUPPORT
    {CkTypeInteger, "EPROTONOSUPPORT", NULL, EPROTONOSUPPORT},
#endif
#ifdef EOPNOTSUPP
    {CkTypeInteger, "EOPNOTSUPP", NULL, EOPNOTSUPP},
#endif
#ifdef EAFNOSUPPORT
    {CkTypeInteger, "EAFNOSUPPORT", NULL, EAFNOSUPPORT},
#endif
#ifdef EADDRINUSE
    {CkTypeInteger, "EADDRINUSE", NULL, EADDRINUSE},
#endif
#ifdef EADDRNOTAVAIL
    {CkTypeInteger, "EADDRNOTAVAIL", NULL, EADDRNOTAVAIL},
#endif
#ifdef ENETDOWN
    {CkTypeInteger, "ENETDOWN", NULL, ENETDOWN},
#endif
#ifdef ENETUNREACH
    {CkTypeInteger, "ENETUNREACH", NULL, ENETUNREACH},
#endif
#ifdef ENETRESET
    {CkTypeInteger, "ENETRESET", NULL, ENETRESET},
#endif
#ifdef ECONNABORTED
    {CkTypeInteger, "ECONNABORTED", NULL, ECONNABORTED},
#endif
#ifdef ECONNRESET
    {CkTypeInteger, "ECONNRESET", NULL, ECONNRESET},
#endif
#ifdef ENOBUFS
    {CkTypeInteger, "ENOBUFS", NULL, ENOBUFS},
#endif
#ifdef EISCONN
    {CkTypeInteger, "EISCONN", NULL, EISCONN},
#endif
#ifdef ENOTCONN
    {CkTypeInteger, "ENOTCONN", NULL, ENOTCONN},
#endif
#ifdef ETIMEDOUT
    {CkTypeInteger, "ETIMEDOUT", NULL, ETIMEDOUT},
#endif
#ifdef ECONNREFUSED
    {CkTypeInteger, "ECONNREFUSED", NULL, ECONNREFUSED},
#endif
#ifdef EHOSTUNREACH
    {CkTypeInteger, "EHOSTUNREACH", NULL, EHOSTUNREACH},
#endif
#ifdef EALREADY
    {CkTypeInteger, "EALREADY", NULL, EALREADY},
#endif
#ifdef EINPROGRESS
    {CkTypeInteger, "EINPROGRESS", NULL, EINPROGRESS},
#endif
#ifdef ESTALE
    {CkTypeInteger, "ESTALE", NULL, ESTALE},
#endif
#ifdef EDQUOT
    {CkTypeInteger, "EDQUOT", NULL, EDQUOT},
#endif
#ifdef ECANCELED
    {CkTypeInteger, "ECANCELED", NULL, ECANCELED},
#endif
#ifdef EPFNOSUPPORT
    {CkTypeInteger, "EPFNOSUPPORT", NULL, EPFNOSUPPORT},
#endif
#ifdef ESHUTDOWN
    {CkTypeInteger, "ESHUTDOWN", NULL, ESHUTDOWN},
#endif
#ifdef EHOSTDOWN
    {CkTypeInteger, "EHOSTDOWN", NULL, EHOSTDOWN},
#endif
    {CkTypeFunction, "strerror", CkpOsStrerror, 0},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpOsStrerror (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine takes in an error number, and translates it to an error string.
    It returns an error string of the error, or an empty string if the error
    code is unknown.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Error;
    PCSTR String;

    //
    // The function is strerror(error).
    //

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    Error = CkGetInteger(Vm, 1);
    String = strerror(Error);
    if (String == NULL) {
        CkReturnString(Vm, "", 0);

    } else {
        CkReturnString(Vm, String, strlen(String));
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

