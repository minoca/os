/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    select.h

Abstract:

    This header contains select type and function definitions.

Author:

    Evan Green 15-Jan-2015

--*/

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>
#include <signal.h>
#include <time.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro is used to get the block index.
//

#define _FD_INDEX(_FileDescriptor) ((_FileDescriptor) / NFDBITS)

//
// This macro is used to get a mask within a block.
//

#define _FD_MASK(_FileDescriptor) \
    (1 << ((_FileDescriptor) % NFDBITS))

//
// This macro clears the bit for the file descriptor in the set.
//

#define FD_CLR(_FileDescriptor, _Set) \
    ((_Set)->fds_bits[_FD_INDEX(_FileDescriptor)] &= ~_FD_MASK(_FileDescriptor))

//
// This macro returns a non-zero value if the bit for the file descriptor is
// set in the given set.
//

#define FD_ISSET(_FileDescriptor, _Set) \
    (((_Set)->fds_bits[_FD_INDEX(_FileDescriptor)] & \
      _FD_MASK(_FileDescriptor)) != 0)

//
// This macro sets the bit for the file descriptor in the given set.
//

#define FD_SET(_FileDescriptor, _Set) \
    ((_Set)->fds_bits[_FD_INDEX(_FileDescriptor)] |= _FD_MASK(_FileDescriptor))

//
// This macro initializes the file descriptor set to be empty.
//

#if __SIZEOF_LONG__ == 8

#define FD_ZERO(_Set)               \
    do {                            \
        (_Set)->fds_bits[0] = 0;    \
        (_Set)->fds_bits[1] = 0;    \
                                    \
    } while (0)

#else

#define FD_ZERO(_Set)               \
    do {                            \
        (_Set)->fds_bits[0] = 0;    \
        (_Set)->fds_bits[1] = 0;    \
        (_Set)->fds_bits[2] = 0;    \
        (_Set)->fds_bits[3] = 0;    \
                                    \
    } while (0)

#endif

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the number of bits in an fd_mask.
//

#define NFDBITS (8 * (int)sizeof(fd_mask))

//
// Define the maximum number of file descriptors in the fd_set structure.
//

#define FD_SETSIZE 128

//
// ------------------------------------------------------ Data Type Definitions
//

typedef long int fd_mask;

/*++

Structure Description:

    This structure defines the type for file descriptor sets.

Members:

    fds_bits - Stores the array of bits representing the file descriptors.
        Users should avoid manipulating this value directly, but instead use the
        FD_CLR, FD_SET, FD_ZERO, and FD_ISSET macros.

--*/

typedef struct {
    fd_mask fds_bits[FD_SETSIZE / NFDBITS];
} fd_set;

/*++

Structure Description:

    This structure defines the type for a time value.

Members:

    tv_sec - Stores the number of seconds in this time value.

    tv_usec - Stores the number of microseconds in this time value.

--*/

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
select (
    int DescriptorCount,
    fd_set *ReadDescriptors,
    fd_set *WriteDescriptors,
    fd_set *ErrorDescriptors,
    struct timeval *Timeout
    );

/*++

Routine Description:

    This routine indicates which of the specified file descriptors are ready
    for reading, writing, and have error conditions.

Arguments:

    DescriptorCount - Supplies the range of file descriptors to be tested.
        This routine tests file descriptors in the range of 0 to the descriptor
        count - 1.

    ReadDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for reading. On
        output, contains the set of descriptors that are ready to be read.

    WriteDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for writing. On
        output, contains the set of descriptors that are ready to be written to.

    ErrorDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for errors. On
        output, contains the set of descriptors that have errors.

    Timeout - Supplies an optional to a structure that defines how long to wait
        for one or more of the descriptors to become ready. If all members of
        this structure are 0, the function will not block. If this argument is
        not supplied, the function will block indefinitely until one of the
        events is ready. If all three descriptor structure pointers is null,
        this routine will block for the specified amount of time and then
        return.

Return Value:

    On success, returns the total number of bits set in the resulting bitmaps.

    0 if the timeout expired.

    -1 on error, and errno will be set to contain more information.

--*/

LIBC_API
int
pselect (
    int DescriptorCount,
    fd_set *ReadDescriptors,
    fd_set *WriteDescriptors,
    fd_set *ErrorDescriptors,
    const struct timespec *Timeout,
    const sigset_t *SignalMask
    );

/*++

Routine Description:

    This routine indicates which of the specified file descriptors are ready
    for reading, writing, and have error conditions.

Arguments:

    DescriptorCount - Supplies the range of file descriptors to be tested.
        This routine tests file descriptors in the range of 0 to the descriptor
        count - 1.

    ReadDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for reading. On
        output, contains the set of descriptors that are ready to be read.

    WriteDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for writing. On
        output, contains the set of descriptors that are ready to be written to.

    ErrorDescriptors - Supplies an optional pointer to a set of descriptors that
        on input supplies the set of descriptors to be checked for errors. On
        output, contains the set of descriptors that have errors.

    Timeout - Supplies an optional to a structure that defines how long to wait
        for one or more of the descriptors to become ready. If all members of
        this structure are 0, the function will not block. If this argument is
        not supplied, the function will block indefinitely until one of the
        events is ready. If all three descriptor structure pointers is null,
        this routine will block for the specified amount of time and then
        return.

    SignalMask - Supplies an optional pointer to the signal mask to set for
        the duration of the wait.

Return Value:

    On success, returns the total number of bits set in the resulting bitmaps.

    0 if the timeout expired.

    -1 on error, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

