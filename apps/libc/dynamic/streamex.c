/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    streamex.c

Abstract:

    This module implements support for the shady back alley stream APIs.
    These functions are not standardized, and portable applications should
    generally try to avoid them.

Author:

    Evan Green 1-Oct-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
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
// ------------------------------------------------------------------ Functions
//

LIBC_API
size_t
__fbufsize (
    FILE *Stream
    )

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

{

    return Stream->BufferSize;
}

LIBC_API
size_t
__fpending (
    FILE *Stream
    )

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

{

    return Stream->BufferValidSize;
}

LIBC_API
int
__flbf (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns a non-zero value if the given stream is line buffered.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns a non-zero value if the stream is line buffered.

    Zero otherwise.

--*/

{

    if (Stream->BufferMode == _IOLBF) {
        return 1;
    }

    return 0;
}

LIBC_API
int
__freadable (
    FILE *Stream
    )

/*++

Routine Description:

    This routine returns a non-zero value if the given stream allows reading.

Arguments:

    Stream - Supplies a pointer to the file stream to query.

Return Value:

    Returns a non-zero value if the given stream allows reading.

    Zero otherwise.

--*/

{

    if ((Stream->Flags & FILE_FLAG_CAN_READ) != 0) {
        return 1;
    }

    return 0;
}

LIBC_API
int
__fwritable (
    FILE *Stream
    )

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

{

    if ((Stream->OpenFlags & (O_WRONLY | O_APPEND)) != 0) {
        return 1;
    }

    return 0;
}

LIBC_API
int
__freading (
    FILE *Stream
    )

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

{

    if ((Stream->Flags & FILE_FLAG_READ_LAST) != 0) {
        return 1;
    }

    return 0;
}

LIBC_API
int
__fwriting (
    FILE *Stream
    )

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

{

    if ((Stream->Flags & FILE_FLAG_READ_LAST) == 0) {
        return 1;
    }

    return 0;
}

LIBC_API
int
__fsetlocking (
    FILE *Stream,
    int LockingType
    )

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

{

    int PreviousType;

    PreviousType = FSETLOCKING_INTERNAL;
    if ((Stream->Flags & FILE_FLAG_DISABLE_LOCKING) != 0) {
        PreviousType = FSETLOCKING_BYCALLER;
    }

    if (LockingType == FSETLOCKING_INTERNAL) {
        Stream->Flags &= ~FILE_FLAG_DISABLE_LOCKING;

    } else if (LockingType == FSETLOCKING_BYCALLER) {
        Stream->Flags |= FILE_FLAG_DISABLE_LOCKING;
    }

    return PreviousType;
}

LIBC_API
void
_flushlbf (
    void
    )

/*++

Routine Description:

    This routine flushes all line-buffered streams.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Just flush everything.
    //

    fflush(NULL);
    return;
}

LIBC_API
void
__fpurge (
    FILE *Stream
    )

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

{

    ClpLockStream(Stream);
    __fpurge_unlocked(Stream);
    ClpUnlockStream(Stream);
    return;
}

LIBC_API
void
__fpurge_unlocked (
    FILE *Stream
    )

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

{

    Stream->BufferNextIndex = 0;
    Stream->BufferValidSize = 0;
    Stream->Flags &= ~FILE_FLAG_UNGET_VALID;
    return;
}

LIBC_API
size_t
__freadahead (
    FILE *Stream
    )

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

{

    size_t Result;

    ClpLockStream(Stream);
    Result = __freadahead_unlocked(Stream);
    ClpUnlockStream(Stream);
    return Result;
}

LIBC_API
size_t
__freadahead_unlocked (
    FILE *Stream
    )

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

{

    ULONG ReadAheadSize;

    ReadAheadSize = Stream->BufferValidSize - Stream->BufferNextIndex;
    if ((Stream->Flags & FILE_FLAG_WIDE_ORIENTED) != 0) {
        ReadAheadSize /= sizeof(WCHAR);
    }

    if ((Stream->Flags & FILE_FLAG_UNGET_VALID) != 0) {
        ReadAheadSize += 1;
    }

    return ReadAheadSize;
}

LIBC_API
void
__fseterr (
    FILE *Stream
    )

/*++

Routine Description:

    This routine sets the error indicator on the given stream.

Arguments:

    Stream - Supplies a pointer to the file stream to set the error indicator
        for.

Return Value:

    None.

--*/

{

    Stream->Flags |= FILE_FLAG_ERROR;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

