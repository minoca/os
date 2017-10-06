/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    poll.h

Abstract:

    This header contains definitions for the poll function.

Author:

    Evan Green 3-May-2013

--*/

#ifndef _POLL_H
#define _POLL_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// This value specifies that data other than high priority data may be read
// without blocking.
//

#define POLLIN 0x0001
#define POLLRDNORM POLLIN

//
// This value specifies that priority data may be read without blocking.
//

#define POLLRDBAND 0x0002
#define POLLPRI POLLRDBAND

//
// This value specifies that normal data may be written without blocking.
//

#define POLLOUT 0x0004
#define POLLWRNORM POLLOUT

//
// This value specifies that priority data may be written.
//

#define POLLWRBAND 0x0008

//
// This value specifies that the descriptor suffered an error. It is only set
// in the return events, and is ignored if set in events.
//

#define POLLERR 0x0010

//
// This value specifies that the device backing the I/O descriptor has
// disconnected. It is only set in the return events, and is ignored if set
// in events.
//

#define POLLHUP 0x0020

//
// This value indicates that the specified file descriptor is invalid. It is
// only set in return events, and is ignored if set in events.
//

#define POLLNVAL 0x0040

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a polled file descriptor.

Members:

    fd - Stores the descriptor being polled.

    events - Stores the mask of events to poll on.

    revents - Stores the mask of events that apply to this descriptor.

--*/

struct pollfd {
    int fd;
    short events;
    short revents;
};

//
// Define the size of the "number of file descriptors" type.
//

typedef unsigned long nfds_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
poll (
    struct pollfd PollDescriptors[],
    nfds_t DescriptorCount,
    int Timeout
    );

/*++

Routine Description:

    This routine blocks waiting for specified activity on a range of file
    descriptors.

Arguments:

    PollDescriptors - Supplies an array of poll descriptor structures,
        indicating which descriptors should be waited on and which events
        should qualify in each descriptor.

    DescriptorCount - Supplies the number of descriptors in the array.

    Timeout - Supplies the amount of time in milliseconds to block before
        giving up and returning anyway. Supply 0 to not block at all, and
        supply -1 to wait for an indefinite amount of time.

Return Value:

    Returns a positive number to indicate success and the number of file
    descriptors that had events occur.

    Returns 0 to indicate a timeout.

    Returns -1 to indicate an error, and errno will be set to contain more
    information.

--*/

LIBC_API
int
ppoll (
    struct pollfd PollDescriptors[],
    nfds_t DescriptorCount,
    const struct timespec *Timeout,
    const sigset_t *SignalMask
    );

/*++

Routine Description:

    This routine blocks waiting for specified activity on a range of file
    descriptors.

Arguments:

    PollDescriptors - Supplies an array of poll descriptor structures,
        indicating which descriptors should be waited on and which events
        should qualify in each descriptor.

    DescriptorCount - Supplies the number of descriptors in the array.

    Timeout - Supplies the amount of time to block before giving up and
        returning anyway. Supply 0 to not block at all, and supply -1 to wait
        for an indefinite amount of time. The timeout will be at least as long
        as supplied, but may also be rounded up.

    SignalMask - Supplies an optional pointer to a signal mask to set
        atomically for the duration of the wait.

Return Value:

    Returns a positive number to indicate success and the number of file
    descriptors that had events occur.

    Returns 0 to indicate a timeout.

    Returns -1 to indicate an error, and errno will be set to contain more
    information.

--*/

#ifdef __cplusplus

}

#endif
#endif

