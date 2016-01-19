/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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
    "No error",
    "Operation not permitted",
    "No such file or directory",
    "No such process",
    "Interrupted",
    "I/O error",
    "No such device or address",
    "Too big",
    "Not executable",
    "Invalid file descriptor",
    "No child processes",                   /* 10 */
    "Try again",
    "Out of memory",
    "Permission denied",
    "Bad address",
    "Not a block device",
    "Resource in use",
    "File exists",
    "Improper cross device link",
    "No such device",
    "Not a directory",                      /* 20 */
    "Is a directory",
    "Invalid argument",
    "Too many files open in the system",
    "Too many files open",
    "Not a TTY",
    "Text file busy",
    "File too large",
    "Device full",
    "Invalid seek",
    "Read-only file system",                /* 30 */
    "Too many links",
    "Broken pipe",
    "Math domain error",
    "Invalid range",
    "Deadlock detected",
    "Name too long",
    "No record locks available",
    "Function not implemented",
    "Directory not empty",
    "Too many symbolic links",              /* 40 */
    NULL,
    "No message",
    "Identifier removed",
    "Operation not supported",
    "Owner dead",
    "Not recoverable",
    "Not a stream",
    "No data available",
    "Timer expired",
    "Out of streams resources",             /* 50 */
    "No link",
    "Protocol error",
    "Multihop attempted",
    "Bad message",
    "Overflow",
    "Illegal sequence",
    "Not a socket",
    "Destination address required",
    "Message too long",
    "Invalid protocol type",                /* 60 */
    "Protocol not available",
    "Protocol not supported",
    "Operation not supported",
    "Address family not supported",
    "Address already in use",
    "Address not available",
    "Network down",
    "Network unreachable",
    "Network reset",
    "Connection aborted",                   /* 70 */
    "Connection reset",
    "No buffer space available",
    "Already connected",
    "Not connected",
    "Connection timed out",
    "Connection refused",
    "Host unreachable",
    "Operation already in progress",
    "Operation in progress",
    "Stale handle",                         /* 80 */
    "Quota exceeded",
    "Operation canceled",
    "Protocol family not supported",
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
    if ((String != NULL) && (strlen(String) != 0)) {
        fprintf(stderr, "%s: %s\n", String, ErrorString);

    } else {
        fprintf(stderr, "%s\n", ErrorString);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

