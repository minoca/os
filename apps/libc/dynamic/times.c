/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    times.c

Abstract:

    This module implements support for getting the current process' running
    time.

Author:

    Evan Green 23-Jun-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <unistd.h>
#include <sys/times.h>

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
clock_t
times (
    struct tms *Times
    )

/*++

Routine Description:

    This routine returns the running time for the current process and its
    children.

Arguments:

    Times - Supplies a pointer where the running time information will be
        returned.

Return Value:

    On success, returns the elapsed real time, in clock ticks, since an
    arbitrary time in the past (like boot time). This point does not change
    from one invocation of times within the process to another. On error, -1
    will be returned and errno will be set to indicate the error.

--*/

{

    RESOURCE_USAGE ChildrenUsage;
    ULONGLONG ClockFrequency;
    LONG ClockTicksPerSecond;
    clock_t ElapsedRealTime;
    ULONGLONG Microseconds;
    RESOURCE_USAGE ProcessUsage;
    KSTATUS Status;
    ULONGLONG TimeCounter;
    ULONGLONG TimeCounterFrequency;

    if (Times == NULL) {
        errno = EINVAL;
        return -1;
    }

    //
    // Get the clock ticks per second that the caller expects for all the
    // clock_t values.
    //

    ClockTicksPerSecond = sysconf(_SC_CLK_TCK);
    if (ClockTicksPerSecond == -1) {
        return -1;
    }

    //
    // Query the system for the current process times.
    //

    Status = OsGetResourceUsage(ResourceUsageRequestProcess,
                                -1,
                                &ProcessUsage,
                                &ClockFrequency);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    Status = OsGetResourceUsage(ResourceUsageRequestProcessChildren,
                                -1,
                                &ChildrenUsage,
                                NULL);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    //
    // Convert each of the process times into the clock_t times expected by the
    // caller.
    //

    Microseconds = (ProcessUsage.UserCycles * MICROSECONDS_PER_SECOND) /
                   ClockFrequency;

    Times->tms_utime = (Microseconds * ClockTicksPerSecond) /
                       MICROSECONDS_PER_SECOND;

    Microseconds = (ProcessUsage.KernelCycles * MICROSECONDS_PER_SECOND) /
                   ClockFrequency;

    Times->tms_stime = (Microseconds * ClockTicksPerSecond) /
                       MICROSECONDS_PER_SECOND;

    Microseconds = (ChildrenUsage.UserCycles * MICROSECONDS_PER_SECOND) /
                   ClockFrequency;

    Times->tms_cutime = (Microseconds * ClockTicksPerSecond) /
                        MICROSECONDS_PER_SECOND;

    Microseconds = (ChildrenUsage.KernelCycles * MICROSECONDS_PER_SECOND) /
                   ClockFrequency;

    Times->tms_cstime = (Microseconds * ClockTicksPerSecond) /
                        MICROSECONDS_PER_SECOND;

    //
    // The process time was successfully collected, get the elapsed real time
    // and convert it to clock ticks.
    //

    TimeCounter = OsGetRecentTimeCounter();
    TimeCounterFrequency = OsGetTimeCounterFrequency();
    Microseconds = (TimeCounter * MICROSECONDS_PER_SECOND) /
                   TimeCounterFrequency;

    ElapsedRealTime = (Microseconds * ClockTicksPerSecond) /
                      MICROSECONDS_PER_SECOND;

    return ElapsedRealTime;
}

//
// --------------------------------------------------------- Internal Functions
//

