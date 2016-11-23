/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uio.c

Abstract:

    This module implements the user I/O vector read and write routines.

Author:

    Evan Green 23-Jan-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

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
ssize_t
readv (
    int FileDescriptor,
    const struct iovec *IoVector,
    int IoVectorCount
    )

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

{

    UINTN BytesCompleted;
    UINTN Index;
    UINTN Size;
    KSTATUS Status;

    Size = 0;
    for (Index = 0; Index < IoVectorCount; Index += 1) {
        Size += IoVector[Index].iov_len;
    }

    Status = OsPerformVectoredIo((HANDLE)(INTN)FileDescriptor,
                                 IO_OFFSET_NONE,
                                 Size,
                                 0,
                                 SYS_WAIT_TIME_INDEFINITE,
                                 (PIO_VECTOR)IoVector,
                                 IoVectorCount,
                                 &BytesCompleted);

    if (Status == STATUS_TIMEOUT) {
        errno = EAGAIN;
        return -1;

    } else if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        if (BytesCompleted == 0) {
            BytesCompleted = -1;
        }
    }

    return (ssize_t)BytesCompleted;
}

LIBC_API
ssize_t
writev (
    int FileDescriptor,
    const struct iovec *IoVector,
    int IoVectorCount
    )

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

{

    UINTN BytesCompleted;
    UINTN Index;
    UINTN Size;
    KSTATUS Status;

    Size = 0;
    for (Index = 0; Index < IoVectorCount; Index += 1) {
        Size += IoVector[Index].iov_len;
    }

    Status = OsPerformVectoredIo((HANDLE)(INTN)FileDescriptor,
                                 IO_OFFSET_NONE,
                                 Size,
                                 SYS_IO_FLAG_WRITE,
                                 SYS_WAIT_TIME_INDEFINITE,
                                 (PIO_VECTOR)IoVector,
                                 IoVectorCount,
                                 &BytesCompleted);

    if (Status == STATUS_TIMEOUT) {
        errno = EAGAIN;
        return -1;

    } else if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        if (BytesCompleted == 0) {
            BytesCompleted = -1;
        }
    }

    return (ssize_t)BytesCompleted;
}

//
// --------------------------------------------------------- Internal Functions
//

