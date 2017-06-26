/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    win32.c

Abstract:

    This module implements Windows-specific Chalk time functionality.

Author:

    Evan Green 5-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "timwin32.h"

//
// --------------------------------------------------------------------- Macros
//

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

int
clock_getres (
    unsigned int ClockId,
    struct timespec *Resolution
    )

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

{

    LARGE_INTEGER Frequency;
    ULONGLONG Nanoseconds;

    switch (ClockId) {
    case CLOCK_REALTIME:
        Nanoseconds = 100;
        break;

    case CLOCK_MONOTONIC:
        QueryPerformanceFrequency(&Frequency);
        Nanoseconds = 1000000000ULL / Frequency.QuadPart;
        break;

    default:
        errno = EINVAL;
        return -1;
    }

    Resolution->tv_sec = 0;
    Resolution->tv_nsec = Nanoseconds;
    return 0;
}

int
clock_gettime (
    unsigned int ClockId,
    struct timespec *Time
    )

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

{

    LARGE_INTEGER Counter;
    FILETIME FileTime;
    LARGE_INTEGER Frequency;
    ULONG Nanoseconds;
    ULONGLONG Now;
    ULONGLONG Seconds;

    switch (ClockId) {
    case CLOCK_REALTIME:
        GetSystemTimeAsFileTime(&FileTime);
        Now = (ULONGLONG)FileTime.dwLowDateTime +
              ((ULONGLONG)(FileTime.dwHighDateTime) << 32);

        Now -= 116444736000000000ULL;
        Seconds = Now / 10000000ULL;
        Nanoseconds = (Now % 10000000ULL) * 100ULL;
        break;

    case CLOCK_MONOTONIC:
        QueryPerformanceCounter(&Counter);
        QueryPerformanceFrequency(&Frequency);
        Seconds = Counter.QuadPart / Frequency.QuadPart;
        Now = Counter.QuadPart - (Seconds * Frequency.QuadPart);
        Nanoseconds = (Now * 1000000000ULL) / Frequency.QuadPart;
        break;

    default:
        errno = EINVAL;
        return -1;
    }

    Time->tv_sec = Seconds;
    Time->tv_nsec = Nanoseconds;
    return 0;
}

int
clock_settime (
    unsigned int ClockId,
    const struct timespec *NewTime
    )

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

{

    errno = ENOSYS;
    return -1;
}

int
nanosleep (
    const struct timespec *RequestedTime,
    struct timespec *RemainingTime
    )

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

{

    ULONGLONG Nanoseconds;
    ULONGLONG Seconds;

    assert(RemainingTime == NULL);

    Seconds = RequestedTime->tv_sec;
    Nanoseconds = RequestedTime->tv_nsec;
    while (Seconds > 0x100000) {
        Sleep(0x100000 * 1000);
        Seconds -= 0x100000;
    }

    Sleep((Seconds * 1000ULL) + (Nanoseconds / 1000000ULL));
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

