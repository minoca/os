/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    errno.c

Abstract:

    This module implements the error number global variable.

Author:

    Evan Green 11-Mar-2013

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
// Define the one and only error number global.
//

LIBC_API __THREAD int errno;

PSTR ClpErrorStrings[] = {
    "Success",
    "Operation not permitted",
    "No such file or directory",
    "No such process",
    "Interrupted system call",
    "I/O error",
    "No such device or address",
    "Argument list too long",
    "Exec format error",
    "Bad file descriptor",
    "No child processes",                       /* 10 */
    "Try again",
    "Out of memory",
    "Permission denied",
    "Bad address",
    "Block device required",
    "Device or resource busy",
    "File exists",
    "Invalid cross-device link",
    "No such device",
    "Not a directory",                          /* 20 */
    "Is a directory",
    "Invalid argument",
    "Too many files open in the system",
    "Too many files open",
    "Inappropriate ioctl for device",
    "Text file busy",
    "File too large",
    "No space left on device",
    "Illegal seek",
    "Read-only file system",                    /* 30 */
    "Too many links",
    "Broken pipe",
    "Numerical argument out of domain",
    "Numerical result out of range",
    "Resource deadlock would occur",
    "File name too long",
    "No record locks available",
    "Function not implemented",
    "Directory not empty",
    "Too many symbolic links encountered",      /* 40 */
    NULL,
    "No message of desired type",
    "Identifier removed",
    "Operation not supported",
    "Owner died",
    "State not recoverable",
    "Device not a stream",
    "No data available",
    "Timer expired",
    "Out of streams resources",                 /* 50 */
    "Link has been severed",
    "Protocol error",
    "Multihop attempted",
    "Bad message",
    "Value too large for defined data type",
    "Illegal byte sequence",
    "Socket operation on non-socket",
    "Destination address required",
    "Message too long",
    "Protocol wrong type for socket",           /* 60 */
    "Protocol not available",
    "Protocol not supported",
    "Operation not supported",
    "Address family not supported by protocol",
    "Address already in use",
    "Cannot assign requested address",
    "Network is down",
    "Network is unreachable",
    "Network dropped connection on reset",
    "Software caused connection abort",         /* 70 */
    "Connection reset by peer",
    "No buffer space available",
    "Transport endpoint is already connected",
    "Transport endpoint is not connected",
    "Connection timed out",
    "Connection refused",
    "No route to host",
    "Operation already in progress",
    "Operation now in progress",
    "Stale file handle",                        /* 80 */
    "Quota exceeded",
    "Operation canceled",
    "Protocol family not supported",
    "Cannot send after endpoint shutdown",
    "Host is down",
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
char *
strerror (
    int ErrorNumber
    )

/*++

Routine Description:

    This routine converts an error number into a human readable string. This
    routine is neither thread-safe nor reentrant.

Arguments:

    ErrorNumber - Supplies the error number to convert.

Return Value:

    Returns a pointer to a string describing the error.

--*/

{

    PSTR String;

    String = NULL;
    if (ErrorNumber < 0) {
        return "Unknown (less than zero passed to strerror)";
    }

    if (ErrorNumber < sizeof(ClpErrorStrings) / sizeof(ClpErrorStrings[0])) {
        String = ClpErrorStrings[ErrorNumber];
    }

    if (String == NULL) {
        String = "Unknown Error";
    }

    return String;
}

LIBC_API
int
strerror_r (
    int ErrorNumber,
    char *Buffer,
    size_t BufferSize
    )

/*++

Routine Description:

    This routine converts an error number into a human readable string in a
    thread safe and reentrant manner.

Arguments:

    ErrorNumber - Supplies the error number to convert.

    Buffer - Supplies a pointer to a buffer to fill with the error string.

    BufferSize - Supplies the size of the given buffer in bytes.

Return Value:

    Zero on success.

    Returns an error number on failure.

--*/

{

    PSTR String;

    if ((Buffer == NULL) || (BufferSize < 1)) {
        return EINVAL;
    }

    String = strerror(ErrorNumber);
    strncpy(Buffer, String, BufferSize);
    Buffer[BufferSize - 1] = '\0';
    return 0;
}

LIBC_API
void
perror (
    const char *String
    )

/*++

Routine Description:

    This routine maps the error number accessed through the symbol errno to
    a language-dependent error message, and prints that out to standard error
    with the given parameter.

Arguments:

    String - Supplies an optional pointer to a string to print before the
        error message. If this is supplied and of non-zero length, the format
        will be "<string>: <errno string>\n". Otherwise, the format will be
        "<errno string>\n".

Return Value:

    None.

--*/

{

    char *ErrorString;

    ErrorString = strerror(errno);
    if ((String != NULL) && (*String != '\0')) {
        fprintf(stderr, "%s: %s\n", String, ErrorString);

    } else {
        fprintf(stderr, "%s\n", ErrorString);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

