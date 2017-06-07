/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timwin32.h

Abstract:

    This header contains OS time definitions for Windows.

Author:

    Evan Green 5-Jun-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores time in a form more accurate than one second.

Members:

    tv_sec - Stores the count of seconds.

    tv_nsec - Stores the count of nanoseconds.

--*/

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

int
clock_getres (
    unsigned int ClockId,
    struct timespec *Resolution
    );

/*++

Routine Description:

    This routine gets the resolution for the given clock. Time values for calls
    to get or set this clock will be limited by the precision of the resolution.

Arguments:

    ClockId - Supplies the ID of the clock whose resolution is to be queried.

    Resolution - Supplies a pointer that receives the resolution of the given
        clock.

Return Value:

    0 on success. The returned resolution will be in the resolution parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

int
clock_gettime (
    unsigned int ClockId,
    struct timespec *Time
    );

/*++

Routine Description:

    This routine gets the current time for the given clock.

Arguments:

    ClockId - Supplies the ID of the clock whose time is being queried.

    Time - Supplies a pointer that receives the current value of the queried
        clock.

Return Value:

    0 on success. The returned time will be in the time parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

int
clock_settime (
    unsigned int ClockId,
    const struct timespec *NewTime
    );

/*++

Routine Description:

    This routine sets the time for the given clock.

Arguments:

    ClockId - Supplies the ID of the clock whose time is to be set.

    NewTime - Supplies a pointer to the new time to set for the given clock.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

int
nanosleep (
    const struct timespec *RequestedTime,
    struct timespec *RemainingTime
    );

/*++

Routine Description:

    This routine suspends execution of the calling thread until either the
    given requested time elapses or a signal was delivered. If a signal was
    delivered, then the time remaining in the interval is reported.

Arguments:

    RequestedTime - Supplies a pointer to the requested interval wait time.

    RemainingTime - Supplies an optional pointer that receives the time
        remaining in the interval if the routine is interrupted by a signal.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

