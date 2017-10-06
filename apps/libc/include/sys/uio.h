/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    uio.h

Abstract:

    This header contains definitions for scatter/gather I/O operations.

Author:

    Evan Green 15-Jan-2015

--*/

#ifndef _SYS_UIO_H
#define _SYS_UIO_H

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

    This structure defines the type for a portion of an I/O buffer.

Members:

    iov_base - Stores a pointer to the base of the data.

    iov_len - Stores the length of the data.

--*/

struct iovec {
    void *iov_base;
    size_t iov_len;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
ssize_t
readv (
    int FileDescriptor,
    const struct iovec *IoVector,
    int IoVectorCount
    );

/*++

Routine Description:

    This routine is equivalent to the read function, except that it places data
    into the buffers specified by the given I/O vector array.

Arguments:

    FileDescriptor - Supplies the file descriptor to read from.

    IoVector - Supplies a pointer to an array of I/O vectors.

    IoVectorCount - Supplies the number of elements in the I/O vector array.
        That is, IoVector[IoVectorCount - 1] will be the last array element
        accessed.

Return Value:

    Returns the same values a read would (the number of bytes read on success,
    or -1 on error with errno set to contain more information).

--*/

LIBC_API
ssize_t
writev (
    int FileDescriptor,
    const struct iovec *IoVector,
    int IoVectorCount
    );

/*++

Routine Description:

    This routine is equivalent to the write function, except that it reads data
    from the buffers specified by the given I/O vector array.

Arguments:

    FileDescriptor - Supplies the file descriptor to write to.

    IoVector - Supplies a pointer to an array of I/O vectors.

    IoVectorCount - Supplies the number of elements in the I/O vector array.
        That is, IoVector[IoVectorCount - 1] will be the last array element
        accessed.

Return Value:

    Returns the same values a write would (the number of bytes written on
    success, or -1 on error with errno set to contain more information).

--*/

#ifdef __cplusplus

}

#endif
#endif

