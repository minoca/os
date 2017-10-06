/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    stdio_ext.h

Abstract:

    This header contains definitions for the shady back alley stream APIs.
    These functions are not standardized, and portable applications should
    generally try to avoid them.

Author:

    Evan Green 1-Oct-2013

--*/

#ifndef _STDIO_EXT_H
#define _STDIO_EXT_H

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define locking types, passed as parameters and received as the return value
// of the setlocking function.
//

//
// Set this value to have the C library perform locking around stream
// operations. This is the default.
//

#define FSETLOCKING_INTERNAL 0

//
// Set this value to have the C library skip the internal locks.
// Synchronization must be handled by the caller.
//

#define FSETLOCKING_BYCALLER 1

//
// Pass this value in to avoid changing the type of locking (it only returns
// the current value). This value will never be returned by the setlocking
// function.
//

#define FSETLOCKING_QUERY 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
size_t
__fbufsize (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the size of the buffer currently used by the given
    stream.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns the size in bytes of the buffer currently used by the given
    stream.

--*/

LIBC_API
size_t
__fpending (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the number of bytes in the output buffer of the given
    stream. For wide-oriented streams the unit is wide characters. This
    function is undefined on buffers in reading mode, or opened read-only.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns the number of unflushed characters in the output buffer.

--*/

LIBC_API
int
__flbf (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns a non-zero value if the given stream is line buffered.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns a non-zero value if the stream is line buffered.

    Zero otherwise.

--*/

LIBC_API
int
__freadable (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns a non-zero value if the given stream allows reading.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns a non-zero value if the given stream allows reading.

    Zero otherwise.

--*/

LIBC_API
int
__fwritable (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns a non-zero value if the given stream allows writing
    (ie the stream was opened either in write or append mode).

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns a non-zero value if the given stream allows writing.

    Zero otherwise.

--*/

LIBC_API
int
__freading (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns a non-zero value if the given stream is read-only,
    or if the last operation on the stream was a read operation.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns a non-zero value if the stream is read-only or if the last
    operation was a read.

    Zero otherwise.

--*/

LIBC_API
int
__fwriting (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns a non-zero value if the given stream is write-only
    (or append-only), or if the last operation on the stream was a write
    operation.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns a non-zero value if the stream is write-only (or append-only), or
    if the last operation was a write.

    Zero otherwise.

--*/

LIBC_API
int
__fsetlocking (
    FILE *Stream,
    int LockingType
    );

/*++

Routine Description:

    This routine sets the type of locking the C library should perform on file
    stream operations.

Arguments:

    Stream - Supplies a pointer to the file stream to set locking for.

    LockingType - Supplies the new type of locking to set. See FSETLOCKING_*
        definitions.

Return Value:

    Returns the previous type of locking enabled on the stream.

--*/

LIBC_API
void
_flushlbf (
    void
    );

/*++

Routine Description:

    This routine flushes all line-buffered streams.

Arguments:

    None.

Return Value:

    None.

--*/

LIBC_API
void
__fpurge (
    FILE *Stream
    );

/*++

Routine Description:

    This routine clears the buffers of the given stream. For output streams
    this discards any unwritten output. For input streams this discards any
    input read from the underlying object but not yet obtained. This includes
    unget characters.

Arguments:

    Stream - Supplies a pointer to the file stream to purge.

Return Value:

    None.

--*/

LIBC_API
void
__fpurge_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine clears the buffers of the given stream. For output streams
    this discards any unwritten output. For input streams this discards any
    input read from the underlying object but not yet obtained. This includes
    unget characters. This routine does not acquire the stream's lock.

Arguments:

    Stream - Supplies a pointer to the file stream to purge.

Return Value:

    None.

--*/

LIBC_API
size_t
__freadahead (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the number of bytes remaining to be read from the
    input buffer of the given stream.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns the number of bytes or characters remaining to be read from the
    stream.

    Returns 0 on failure, and errno is set to contain more information.

--*/

LIBC_API
size_t
__freadahead_unlocked (
    FILE *Stream
    );

/*++

Routine Description:

    This routine returns the number of bytes remaining to be read from the
    input buffer of the given stream. This routine does not acquire the file
    stream lock.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns the number of bytes or characters remaining to be read from the
    stream.

    Returns 0 on failure, and errno is set to contain more information.

--*/

LIBC_API
void
__fseterr (
    FILE *Stream
    );

/*++

Routine Description:

    This routine sets the error indicator on the given stream.

Arguments:

    Stream - Supplies a pointer to the file stream to set the error indicator
        for.

Return Value:

    None.

--*/

#ifdef __cplusplus

}

#endif
#endif

