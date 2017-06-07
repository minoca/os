/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.c

Abstract:

    This module implements timekeeping functionality for the C Library.

Author:

    Evan Green 28-Jul-2013

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
#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include <minoca/lib/tzfmt.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines whether or not the process ID can be converted to a
// CPU time clock ID.
//

#define CAN_CONVERT_PROCESS_ID_TO_CPUTIME_ID(_ProcessId) ((_ProcessId) >= 0)

//
// These macros convert a process ID into its CPU time clock ID and vice versa.
//

#define CONVERT_PROCESS_ID_TO_CPUTIME_ID(_ProcessId) \
    (clockid_t)((_ProcessId) * -1)

#define CONVERT_CPUTIME_ID_TO_PROCESS_ID(_ClockId) \
    (pid_t)((_ClockId) * -1)

//
// This macros tests whether or not a given clock ID is a process's CPU time ID.
//

#define IS_PROCESS_CPUTIME_ID(_ClockId) ((pid_t)(_ClockId) < 0)

#define ASSERT_ITIMER_TYPES_EQUIVALENT()        \
    assert((ITIMER_REAL == ITimerReal) &&       \
           (ITIMER_VIRTUAL == ITimerVirtual) && \
           (ITIMER_PROF == ITimerProfile))

//
// ---------------------------------------------------------------- Definitions
//

#define ASCTIME_BUFFER_SIZE 26
#define ASCTIME_FORMAT "%a %b %d %H:%M:%S %Y\n"

#define GLOBAL_TIME_STRING_SIZE 128

#define CUSTOM_TIME_ZONE_NAME_MAX 8

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the template for a custom time zone, specified by the
    TZ variable.

Members:

    Header - Stores the time zone data header.

    StandardRule - Stores the rule for standard time.

    DaylightRule - Stores the rule for Daylight Saving time.

    Zone - Stores the time zone.

    ZoneEntry - Stores the time zone entry.

    Strings - Stores the string table.

--*/

typedef struct _CUSTOM_TIME_ZONE {
    TIME_ZONE_HEADER Header;
    TIME_ZONE_RULE StandardRule;
    TIME_ZONE_RULE DaylightRule;
    TIME_ZONE Zone;
    TIME_ZONE_ENTRY ZoneEntry;
    CHAR Strings[32];
} PACKED CUSTOM_TIME_ZONE, *PCUSTOM_TIME_ZONE;

/*++

Structure Description:

    This structure defines a per-process timer.

Members:

    Handle - Stores the OS API layer's timer handle.

    ClockId - Stores the ID of the clock used to back the timer.

--*/

typedef struct _TIMER {
    LONG Handle;
    clockid_t ClockId;
} TIMER, *PTIMER;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ClpCalendarTimeToStructTm (
    PCALENDAR_TIME CalendarTime,
    struct tm *StructTm
    );

VOID
ClpStructTmToCalendarTime (
    PCALENDAR_TIME CalendarTime,
    struct tm *StructTm
    );

VOID
ClpInitializeTimeZoneData (
    VOID
    );

INT
ClpCreateCustomTimeZone (
    PSTR TimeZoneVariable,
    PVOID *TimeZoneData,
    UINTN *TimeZoneDataSize
    );

INT
ClpReadCustomTimeZoneName (
    PSTR *Variable,
    CHAR ParsedName[CUSTOM_TIME_ZONE_NAME_MAX]
    );

INT
ClpReadCustomTimeOffset (
    PSTR *Variable,
    PLONG Seconds
    );

INT
ClpReadCustomTimeRule (
    PSTR *Variable,
    PTIME_ZONE_RULE Rule
    );

VOID
ClpAcquireTimeZoneLock (
    VOID
    );

VOID
ClpReleaseTimeZoneLock (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// This variable is set to zero if Daylight Saving time should never be applied
// for the timezone in use, or non-zero otherwise.
//

LIBC_API int daylight;

//
// This variable is set to the difference in seconds between Universal
// Coordinated Time (UTC) and local standard time.
//

LIBC_API long timezone;

//
// This variable is set to contain two pointers to strings. The first one
// points to the name of the timezone in standard time, and the second one
// pointers to the name of the timezone in Daylight Saving time.
//

LIBC_API char *tzname[2] = {"GMT", "GMT"};

//
// Store the ridiculous global time buffer used by some C library functions.
//

CHAR ClGlobalTimeString[GLOBAL_TIME_STRING_SIZE];
struct tm ClGlobalTimeStructure;

//
// Store the path to the time zone data loaded.
//

PSTR ClTimeZonePath;

//
// Set this boolean to debug parsing of the TZ variable.
//

BOOL ClDebugCustomTimeZoneParsing;

//
// Store the global time zone lock.
//

OS_LOCK ClTimeZoneLock;

//
// Store the timer backing the alarm function.
//

timer_t ClAlarm = -1;

//
// Store the number of days per month in non-leap years.
//

CHAR ClDaysPerMonth[MONTHS_PER_YEAR] = {
    31,
    28,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31
};

//
// Store the previous time zone variable pointer set.
//

PSTR ClPreviousTzVariable;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
unsigned int
alarm (
    unsigned int Seconds
    )

/*++

Routine Description:

    This routine converts causes the system to generate a SIGALRM signal for
    the process after the number of realtime seconds specified by the given
    parameter have elapsed. Processor scheduling delays may prevent the process
    from handling the signal as soon as it is generated. Alarm requests are not
    stacked; only one SIGALRM generation can be scheduled in this manner. If
    the SIGALRM signal has not yet been generated, the call shall result in
    rescheduling the time at which the SIGALRM signal is generated.

Arguments:

    Seconds - Supplies the number of seconds from now that the alarm should
        fire in. If this value is 0, then a pending alarm request, if any, is
        canceled.

Return Value:

    If there is a previous alarm request with time remaining, then the
    (non-zero) number of seconds until the alarm would have signaled is
    returned.

    0 otherwise. The specification for this function says that it cannot fail.
    In reality, it might, and errno should be checked if 0 is returned.

--*/

{

    timer_t Alarm;
    timer_t NewAlarm;
    struct itimerspec Rate;
    struct itimerspec RemainingTime;
    int Result;

    ASSERT(sizeof(ULONG) == sizeof(timer_t));

    //
    // If seconds is zero, cancel any existing alarm.
    //

    if (Seconds == 0) {
        Alarm = RtlAtomicExchange32((PULONG)(&ClAlarm), -1);
        if (Alarm != -1) {
            Result = timer_gettime(Alarm, &RemainingTime);
            timer_delete(Alarm);
            if (Result == 0) {
                return RemainingTime.it_value.tv_sec;
            }
        }

        return 0;
    }

    //
    // Attempt to atomically create the alarm timer if it hasn't been created
    // yet.
    //

    if (ClAlarm == -1) {
        Result = timer_create(CLOCK_REALTIME, NULL, &NewAlarm);
        if (Result != 0) {
            return -1;
        }

        Alarm = RtlAtomicCompareExchange32((PULONG)(&ClAlarm), NewAlarm, -1);

        //
        // If this routine lost the compare exchange, delete the newly created
        // timer and use the one that the winner created.
        //

        if (Alarm != -1) {
            timer_delete(NewAlarm);
        }
    }

    Alarm = ClAlarm;
    memset(&Rate, 0, sizeof(struct itimerspec));
    Rate.it_value.tv_sec = Seconds;
    Result = timer_settime(Alarm, 0, &Rate, &RemainingTime);
    if (Result != 0) {
        return 0;
    }

    return RemainingTime.it_value.tv_sec;
}

LIBC_API
clock_t
clock (
    void
    )

/*++

Routine Description:

    This routine returns the best approximation of the processor time used
    by the process since the process invocation.

Arguments:

    None.

Return Value:

    Returns the clock time used by the current process, which can be divided
    by CLOCKS_PER_SEC to get the number of seconds of processor time used by
    the process.

    -1 if the processor time is not available or cannot be represented.

--*/

{

    ULONGLONG Frequency;
    KSTATUS Status;
    ULONGLONG TotalMicroseconds;
    clock_t TotalTime;
    RESOURCE_USAGE Usage;

    Status = OsGetResourceUsage(ResourceUsageRequestProcess,
                                -1,
                                &Usage,
                                &Frequency);

    if (!KSUCCESS(Status)) {
        return -1;
    }

    assert(Frequency != 0);

    //
    // Calculate the total number of microseconds.
    //

    TotalMicroseconds = ((Usage.UserCycles + Usage.KernelCycles) *
                         MICROSECONDS_PER_SECOND) /
                        Frequency;

    //
    // Convert the microseconds to the expected clock time.
    //

    TotalTime = (TotalMicroseconds * CLOCKS_PER_SEC) / MICROSECONDS_PER_SECOND;
    return TotalTime;
}

LIBC_API
int
clock_getcpuclockid (
    pid_t ProcessId,
    clockid_t *ClockId
    )

/*++

Routine Description:

    This routine gets the clock ID for the CPU time clock of the given
    process.

Arguments:

    ProcessId - Supplies the ID of the process whose CPU time clock ID is being
        queried.

    ClockId - Supplies a pointer that receives the CPU time clock ID for the
        given process.

Return Value:

    0 on success. The returned clock ID will be in the clock ID parameter.

    Returns an error code on failure.

--*/

{

    PROCESS_ID CurrentProcessId;
    KSTATUS Status;

    //
    // A process ID of 0 is a request for the current process.
    //

    if (ProcessId == 0) {
        Status = OsGetProcessId(ProcessIdProcess, &CurrentProcessId);

        ASSERT(KSUCCESS(Status));
    }

    //
    // Check to make sure the process ID gels with the conversion routine.
    //

    if (CAN_CONVERT_PROCESS_ID_TO_CPUTIME_ID(ProcessId) == FALSE) {
        return EINVAL;
    }

    //
    // The clock ID for a process is just the process ID negated.
    //

    *ClockId = CONVERT_PROCESS_ID_TO_CPUTIME_ID(ProcessId);
    return 0;
}

LIBC_API
int
clock_getres (
    clockid_t ClockId,
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

    ULONGLONG Frequency;

    //
    // All supported clocks have a resolution of 1 nanosecond, in that the
    // nanosecond field returns is valid down to the last digit.
    //

    switch (ClockId) {
    case CLOCK_REALTIME:
    case CLOCK_MONOTONIC:
    case CLOCK_BOOTTIME:
    case CLOCK_MONOTONIC_RAW:
        Frequency = OsGetTimeCounterFrequency();
        if (Frequency <= 1) {
            Resolution->tv_sec = 1;
            Resolution->tv_nsec = 0;

        } else {
            Resolution->tv_sec = 0;
            Resolution->tv_nsec = NANOSECONDS_PER_SECOND / Frequency;
            if (Resolution->tv_nsec == 0) {
                Resolution->tv_nsec = 1;
            }
        }

        break;

    case CLOCK_REALTIME_COARSE:
    case CLOCK_MONOTONIC_COARSE:

        //
        // This is a bit of a lie because 1) the periodic frequency of the
        // clock can be changed and 2) dynamic tick varies this wildly from
        // moment to moment. But it's an okay guess.
        //

        Resolution->tv_sec = 0;
        Resolution->tv_nsec = (15625 * NANOSECONDS_PER_MICROSECOND);
        break;

    case CLOCK_PROCESS_CPUTIME_ID:
    case CLOCK_THREAD_CPUTIME_ID:
        Resolution->tv_sec = 0;
        Resolution->tv_nsec = 1;
        break;

    default:
        if (IS_PROCESS_CPUTIME_ID(ClockId) == FALSE) {
            errno = EINVAL;
            return -1;
        }

        Resolution->tv_sec = 0;
        Resolution->tv_nsec = 1;
        break;
    }

    return 0;
}

LIBC_API
int
clock_gettime (
    clockid_t ClockId,
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

    ULONGLONG Cycles;
    ULONGLONG Frequency;
    pid_t ProcessId;
    time_t Seconds;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;
    ULONGLONG TimeCounter;
    RESOURCE_USAGE Usage;

    //
    // The time parameter is required.
    //

    if (Time == NULL) {
        errno = EINVAL;
        return -1;
    }

    //
    // Get the time based on the clock type.
    //

    switch (ClockId) {
    case CLOCK_REALTIME:
        OsGetHighPrecisionSystemTime(&SystemTime);
        Time->tv_sec = ClpConvertSystemTimeToUnixTime(&SystemTime);
        Time->tv_nsec = SystemTime.Nanoseconds;
        break;

    case CLOCK_MONOTONIC:
    case CLOCK_BOOTTIME:
    case CLOCK_MONOTONIC_RAW:
        TimeCounter = OsQueryTimeCounter();
        Frequency = OsGetTimeCounterFrequency();
        ClpConvertCounterToSpecificTime(TimeCounter, Frequency, Time);
        break;

    case CLOCK_PROCESS_CPUTIME_ID:
        Status = OsGetResourceUsage(ResourceUsageRequestProcess,
                                    -1,
                                    &Usage,
                                    &Frequency);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

        assert(Frequency != 0);

        Cycles = Usage.UserCycles + Usage.KernelCycles;
        ClpConvertCounterToSpecificTime(Cycles, Frequency, Time);
        break;

    case CLOCK_THREAD_CPUTIME_ID:
        Status = OsGetResourceUsage(ResourceUsageRequestThread,
                                    -1,
                                    &Usage,
                                    &Frequency);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

        assert(Frequency != 0);

        Cycles = Usage.UserCycles + Usage.KernelCycles;
        ClpConvertCounterToSpecificTime(Cycles, Frequency, Time);
        break;

    case CLOCK_REALTIME_COARSE:
        OsGetSystemTime(&SystemTime);
        Seconds = ClpConvertSystemTimeToUnixTime(&SystemTime);
        Time->tv_sec = Seconds;
        Time->tv_nsec = SystemTime.Nanoseconds;
        break;

    case CLOCK_MONOTONIC_COARSE:
        TimeCounter = OsGetRecentTimeCounter();
        Frequency = OsGetTimeCounterFrequency();
        ClpConvertCounterToSpecificTime(TimeCounter, Frequency, Time);
        break;

    default:
        if (IS_PROCESS_CPUTIME_ID(ClockId) == FALSE) {
            errno = EINVAL;
            return -1;
        }

        ProcessId = CONVERT_CPUTIME_ID_TO_PROCESS_ID(ClockId);
        Status = OsGetResourceUsage(ResourceUsageRequestProcess,
                                    ProcessId,
                                    &Usage,
                                    &Frequency);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

        assert(Frequency != 0);

        Cycles = Usage.UserCycles + Usage.KernelCycles;
        ClpConvertCounterToSpecificTime(Cycles, Frequency, Time);
        break;
    }

    return 0;
}

LIBC_API
int
clock_settime (
    clockid_t ClockId,
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

    KSTATUS Status;
    SYSTEM_TIME SystemTime;
    ULONGLONG TimeCounter;

    //
    // A new time value is required.
    //

    if (NewTime == NULL) {
        errno = EINVAL;
        return -1;
    }

    //
    // Set the time based on the clock type.
    //

    switch (ClockId) {
    case CLOCK_REALTIME:
    case CLOCK_REALTIME_COARSE:

        //
        // Invalid nanoseconds are not allowed.
        //

        if ((NewTime->tv_nsec < 0) ||
            (NewTime->tv_nsec > NANOSECONDS_PER_SECOND)) {

            errno = EINVAL;
            return -1;
        }

        ClpConvertSpecificTimeToSystemTime(&SystemTime, NewTime);
        TimeCounter = OsGetRecentTimeCounter();
        Status = OsSetSystemTime(&SystemTime, TimeCounter);
        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

        break;

    default:
        errno = EPERM;
        return -1;
    }

    return 0;
}

LIBC_API
int
clock_nanosleep (
    clockid_t ClockId,
    int Flags,
    const struct timespec *RequestedTime,
    struct timespec *RemainingTime
    )

/*++

Routine Description:

    This routine suspends execution of the calling thread until either the
    given clock interval has expired or a signal is delivered. If absolute time
    is specified, then the thread will be suspended until the absolute time is
    reached or a signal is delivered.

Arguments:

    ClockId - Supplies the ID of the clock to use to measure the requested time.

    Flags - Supplies a bitmask of flags. See TIMER_*.

    RequestedTime - Supplies a pointer to the requested time interval to wait
        or the absolute time until which to wait.

    RemainingTime - Supplies an optional pointer that receives the remaining
        time if the thread is interrupted by a signal.

Return Value:

    0 on success or standard error value on failure or interruption.

--*/

{

    SYSTEM_TIME CurrentSystemTime;
    ULONGLONG DesiredEndTime;
    ULONGLONG EndTime;
    ULONGLONG Frequency;
    ULONGLONG RemainingTicks;
    ULONGLONG StartTime;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;

    if (RequestedTime == NULL) {
        return 0;
    }

    //
    // Perform the sleep based on the clock ID.
    //

    switch (ClockId) {

    //
    // The system does not have a way to sleep on the system time, so convert
    // real time clock requests into monotonic requests.
    //

    case CLOCK_REALTIME:
    case CLOCK_MONOTONIC:

        //
        // If an absolute time is supplied for the real-time clock, then it
        // needs to be converted from Unix time to system time and then
        // converted into a relative time.
        //

        Frequency = OsGetTimeCounterFrequency();
        if ((ClockId == CLOCK_REALTIME) && (Flags & TIMER_ABSTIME) != 0) {
            ClpConvertSpecificTimeToSystemTime(&SystemTime, RequestedTime);

            //
            // Sanity check against a recent system time value to make sure
            // the requested time isn't in the past. If it were too far in the
            // past the conversion routine below could break. If it is in the
            // past, then the sleep is over. Return immediately.
            //

            OsGetSystemTime(&CurrentSystemTime);
            if ((SystemTime.Seconds < CurrentSystemTime.Seconds) ||
                ((SystemTime.Seconds == CurrentSystemTime.Seconds) &&
                 (SystemTime.Nanoseconds < CurrentSystemTime.Nanoseconds))) {

                return 0;
            }

            //
            // Convert the absolute system time to an absolute time counter
            // value.
            //

            OsConvertSystemTimeToTimeCounter(&SystemTime, &DesiredEndTime);

        //
        // Otherwise convert the requested time into a time counter value. The
        // conversion is the same for relative and absolute.
        //

        } else {
            ClpConvertSpecificTimeToCounter(&DesiredEndTime,
                                            Frequency,
                                            RequestedTime);

            //
            // If an absolute time was not specified, then add in the start
            // time.
            //

            if ((Flags & TIMER_ABSTIME) == 0) {
                StartTime = OsQueryTimeCounter();
                DesiredEndTime += StartTime;
            }
        }

        Status = OsDelayExecution(TRUE, DesiredEndTime);

        //
        // If the wait was not successful, then return an error unless the end
        // time was reached.
        //

        if (!KSUCCESS(Status)) {
            EndTime = OsQueryTimeCounter();
            if (EndTime < DesiredEndTime) {

                //
                // Return the remaining time if requested and a relative time
                // was supplied. Make sure to check the original flags status
                // here as the local boolean may have changed.
                //

                if ((RemainingTime != NULL) && ((Flags & TIMER_ABSTIME) == 0)) {
                    RemainingTicks = DesiredEndTime - EndTime;
                    ClpConvertCounterToSpecificTime(RemainingTicks,
                                                    Frequency,
                                                    RemainingTime);
                }

                errno = ClConvertKstatusToErrorNumber(Status);
                return -1;
            }
        }

        break;

    default:
        errno = ENOTSUP;
        return -1;
    }

    return 0;
}

LIBC_API
char *
asctime (
    const struct tm *Time
    )

/*++

Routine Description:

    This routine converts the given time structure into a string. This routine
    is neither reentrant nor thread safe, and the results returned may be
    overwritten by subsequent calls to ctime, gmtime, and localtime. It is
    recommended that new applications use asctime_r. The format of the result
    takes the following form (as an example): "Tue Jan 28 11:38:09 1986".

Arguments:

    Time - Supplies a pointer to the time structure to convert.

Return Value:

    Returns a pointer to the buffer on success.

    NULL on failure.

--*/

{

    return asctime_r(Time, ClGlobalTimeString);
}

LIBC_API
char *
asctime_r (
    const struct tm *Time,
    char *Buffer
    )

/*++

Routine Description:

    This routine converts the given time structure into a string. This routine
    is reentrant and thread safe, as it uses only the passed in buffers. The
    format of the result takes the following form (as an example):
    "Tue Jan 28 11:38:09 1986".

Arguments:

    Time - Supplies a pointer to the time structure to convert.

    Buffer - Supplies a pointer to a buffer that must be at least 26 bytes in
        size.

Return Value:

    Returns a pointer to the buffer on success.

    NULL on failure.

--*/

{

    CALENDAR_TIME CalendarTime;
    UINTN Result;

    ClpStructTmToCalendarTime(&CalendarTime, (struct tm *)Time);
    Result = RtlFormatDate(Buffer,
                           ASCTIME_BUFFER_SIZE,
                           ASCTIME_FORMAT,
                           &CalendarTime);

    if (Result == 0) {
        return NULL;
    }

    return Buffer;
}

LIBC_API
char *
ctime (
    const time_t *TimeValue
    )

/*++

Routine Description:

    This routine converts the given time structure into a string. This routine
    is neither reentrant nor thread safe, and the results returned may be
    overwritten by subsequent calls to ctime, gmtime, and localtime. It is
    recommended that new applications use ctime_r. This routine is equivalent
    to calling asctime(localtime(Time)).

Arguments:

    TimeValue - Supplies the time value to convert.

Return Value:

    Returns a pointer to the buffer on success.

    NULL on failure.

--*/

{

    return ctime_r(TimeValue, ClGlobalTimeString);
}

LIBC_API
char *
ctime_r (
    const time_t *TimeValue,
    char *Buffer
    )

/*++

Routine Description:

    This routine converts the given time structure into a string. This routine
    is reentrant and thread safe, as it uses only the passed in buffers. This
    routine is equivalent to calling asctime(localtime(Time)).

Arguments:

    TimeValue - Supplies the time value to convert.

    Buffer - Supplies a pointer to a buffer that must be at least 26 bytes in
        size.

Return Value:

    Returns a pointer to the buffer on success.

    NULL on failure.

--*/

{

    struct tm *Result;
    struct tm TimeStructure;

    Result = localtime_r(TimeValue, &TimeStructure);
    if (Result == NULL) {
        return NULL;
    }

    return asctime_r(&TimeStructure, Buffer);
}

LIBC_API
double
difftime (
    time_t LeftTimeValue,
    time_t RightTimeValue
    )

/*++

Routine Description:

    This routine computes the difference between two time values:
    LeftTimeValue - RightTimeValue.

Arguments:

    LeftTimeValue - Supplies the first time value, the value to subtract from.

    RightTimeValue - Supplies the second time value, the value to subtract.

Return Value:

    Returns the number of seconds between the two times as a double.

--*/

{

    return (double)LeftTimeValue - (double)RightTimeValue;
}

LIBC_API
struct tm *
gmtime (
    const time_t *TimeValue
    )

/*++

Routine Description:

    This routine converts the given time value into a broken down calendar time
    in the GMT time zone. This routine is neither reentrant nor thread safe.

Arguments:

    TimeValue - Supplies a pointer to the time value to convert.

Return Value:

    Returns a pointer to a broken down time structure on success. This buffer
    may be overwritten by subsequent calls to gmtime or localtime.

--*/

{

    return gmtime_r(TimeValue, &ClGlobalTimeStructure);
}

LIBC_API
struct tm *
gmtime_r (
    const time_t *TimeValue,
    struct tm *Result
    )

/*++

Routine Description:

    This routine converts the given time value into a broken down calendar time
    in the GMT time zone. This routine is reentrant and thread safe.

Arguments:

    TimeValue - Supplies a pointer to the time value to convert.

    Result - Supplies a pointer where the result will be returned.

Return Value:

    Returns the result parameter on success.

    NULL on failure.

--*/

{

    CALENDAR_TIME CalendarTime;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;

    ClpConvertUnixTimeToSystemTime(&SystemTime, *TimeValue);
    Status = RtlSystemTimeToGmtCalendarTime(&SystemTime, &CalendarTime);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return NULL;
    }

    ClpCalendarTimeToStructTm(&CalendarTime, Result);
    return Result;
}

LIBC_API
struct tm *
localtime (
    const time_t *TimeValue
    )

/*++

Routine Description:

    This routine converts the given time value into a broken down calendar time
    in the current local time zone. This routine is neither reentrant nor
    thread safe.

Arguments:

    TimeValue - Supplies a pointer to the time value to convert.

Return Value:

    Returns a pointer to a broken down time structure on success. This buffer
    may be overwritten by subsequent calls to gmtime or localtime.

--*/

{

    return localtime_r(TimeValue, &ClGlobalTimeStructure);
}

LIBC_API
struct tm *
localtime_r (
    const time_t *TimeValue,
    struct tm *Result
    )

/*++

Routine Description:

    This routine converts the given time value into a broken down calendar time
    in the current local time zone. This routine is reentrant and thread safe.

Arguments:

    TimeValue - Supplies a pointer to the time value to convert.

    Result - Supplies a pointer where the result will be returned.

Return Value:

    Returns the result parameter on success.

    NULL on failure.

--*/

{

    CALENDAR_TIME CalendarTime;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;

    ClpConvertUnixTimeToSystemTime(&SystemTime, *TimeValue);
    ClpInitializeTimeZoneData();
    Status = RtlSystemTimeToLocalCalendarTime(&SystemTime, &CalendarTime);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return NULL;
    }

    ClpCalendarTimeToStructTm(&CalendarTime, Result);
    return Result;
}

LIBC_API
time_t
timegm (
    struct tm *Time
    )

/*++

Routine Description:

    This routine converts a broken down time structure, given in GMT time, back
    into its corresponding time value, in seconds since the Epoch, midnight on
    January 1, 1970 GMT. It will also normalize the given time structure so
    that each member is in the correct range.

Arguments:

    Time - Supplies a pointer to the broken down time structure.

Return Value:

    Returns the time value corresponding to the given broken down time.

    -1 on failure, and errno will be set to contain more information. Note that
    -1 can also be returned as a valid offset from the Epoch.

--*/

{

    CALENDAR_TIME CalendarTime;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;

    ClpStructTmToCalendarTime(&CalendarTime, Time);
    Status = RtlGmtCalendarTimeToSystemTime(&CalendarTime, &SystemTime);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    //
    // Copy this final calendar time back to the supplied time structure in
    // order to return a normalized calendar time with all the fields filled
    // out.
    //

    ClpCalendarTimeToStructTm(&CalendarTime, Time);
    return ClpConvertSystemTimeToUnixTime(&SystemTime);
}

LIBC_API
time_t
mktime (
    struct tm *Time
    )

/*++

Routine Description:

    This routine converts a broken down time structure, given in local time,
    back into its corresponding time value, in seconds since the Epoch,
    midnight on January 1, 1970 GMT. It will also normalize the given time
    structure so that each member is in the correct range.

Arguments:

    Time - Supplies a pointer to the broken down time structure.

Return Value:

    Returns the time value corresponding to the given broken down time.

    -1 on failure, and errno will be set to contain more information. Note that
    -1 can also be returned as a valid offset from the Epoch.

--*/

{

    CALENDAR_TIME CalendarTime;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;

    ClpStructTmToCalendarTime(&CalendarTime, Time);
    ClpInitializeTimeZoneData();
    Status = RtlLocalCalendarTimeToSystemTime(&CalendarTime, &SystemTime);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    //
    // Copy the calendar time back to the supplied time structure in order to
    // return a normalized calendar time with all the fields filled out.
    //

    ClpCalendarTimeToStructTm(&CalendarTime, Time);
    return ClpConvertSystemTimeToUnixTime(&SystemTime);
}

LIBC_API
size_t
strftime (
    char *Buffer,
    size_t BufferSize,
    const char *Format,
    const struct tm *Time
    )

/*++

Routine Description:

    This routine converts the given calendar time into a string governed by
    the given format string.

Arguments:

    Buffer - Supplies a pointer where the converted string will be returned.

    BufferSize - Supplies the size of the string buffer in bytes.

    Format - Supplies the format string to govern the conversion. Ordinary
        characters in the format string will be copied verbatim to the output
        string. Conversions will be substituted for their corresponding value
        in the provided calendar time. Conversions start with a '%' character,
        followed by an optional E or O character, followed by a conversion
        specifier. The conversion specifier can take the following values:

        %a - Replaced by the abbreviated weekday.
        %A - Replaced by the full weekday.
        %b - Replaced by the abbreviated month name.
        %B - Replaced by the full month name.
        %c - Replaced by the locale's appropriate date and time representation.
        %C - Replaced by the year divided by 100 (century) [00,99].
        %d - Replaced by the day of the month [01,31].
        %D - Equivalent to "%m/%d/%y".
        %e - Replaced by the day of the month [ 1,31]. A single digit is
             preceded by a space.
        %F - Equivalent to "%Y-%m-%d" (the ISO 8601:2001 date format).
        %G - The ISO 8601 week-based year [0001,9999]. The week-based year and
             the Gregorian year can differ in the first week of January.
        %h - Equivalent to %b (abbreviated month).
        %H - Replaced by the 24 hour clock hour [00,23].
        %I - Replaced by the 12 hour clock hour [01,12].
        %J - Replaced by the nanosecond [0,999999999].
        %j - Replaced by the day of the year [001,366].
        %m - Replaced by the month number [01,12].
        %M - Replaced by the minute [00,59].
        %N - Replaced by the microsecond [0,999999]
        %n - Replaced by a newline.
        %p - Replaced by "AM" or "PM".
        %P - Replaced by "am" or "pm".
        %q - Replaced by the millisecond [0,999].
        %r - Replaced by the time in AM/PM notation: "%I:%M:%S %p".
        %R - Replaced by the time in 24 hour notation: "%H:%M".
        %S - Replaced by the second [00,60].
        %s - Replaced by the number of seconds since 1970 GMT.
        %t - Replaced by a tab.
        %T - Replaced by the time: "%H:%M:%S".
        %u - Replaced by the weekday number, with 1 representing Monday [1,7].
        %U - Replaced by the week number of the year [00,53]. The first Sunday
             of January is the first day of week 1. Days before this are week 0.
        %V - Replaced by the week number of the year with Monday as the first
             day in the week [01,53]. If the week containing January 1st has 4
             or more days in the new year, it is considered week 1. Otherwise,
             it is the last week of the previous year, and the next week is 1.
        %w - Replaced by the weekday number [0,6], with 0 representing Sunday.
        %W - Replaced by the week number [00,53]. The first Monday of January
             is the first day of week 1. Days before this are in week 0.
        %x - Replaced by the locale's appropriate date representation.
        %X - Replaced by the locale's appropriate time representation.
        %y - Replaced by the last two digits of the year [00,99].
        %Y - Replaced by the full four digit year [0001,9999].
        %z - Replaced by the offset from UTC in the standard ISO 8601:2000
             standard format (+hhmm or -hhmm), or by no characters if no
             timezone is terminable. If the "is daylight saving" member of the
             calendar structure is greater than zero, then the daylight saving
             offset is used. If the dayslight saving member of the calendar
             structure is negative, no characters are returned.
        %Z - Replaced by the timezone name or abbreviation, or by no bytes if
             no timezone information exists.
        %% - Replaced by a literal '%'.

    Time - Supplies a pointer to the calendar time value to use in the
        substitution.

Return Value:

    Returns the number of characters written to the output buffer, not
    including the null terminator.

--*/

{

    CALENDAR_TIME CalendarTime;
    UINTN Result;

    ClpStructTmToCalendarTime(&CalendarTime, (struct tm *)Time);
    ClpInitializeTimeZoneData();
    Result = RtlFormatDate(Buffer, BufferSize, (PSTR)Format, &CalendarTime);
    if (Result != 0) {
        return Result - 1;
    }

    return Result;
}

LIBC_API
size_t
wcsftime (
    wchar_t *Buffer,
    size_t BufferSize,
    const wchar_t *Format,
    const struct tm *Time
    )

/*++

Routine Description:

    This routine converts the given calendar time into a wide string governed
    by the given format string.

Arguments:

    Buffer - Supplies a pointer where the converted wide string will be
        returned.

    BufferSize - Supplies the size of the string buffer in characters.

    Format - Supplies the wide format string to govern the conversion. Ordinary
        characters in the format string will be copied verbatim to the output
        string. Conversions will be substituted for their corresponding value
        in the provided calendar time. The conversions follow the same format
        as the non-wide print time function.

    Time - Supplies a pointer to the calendar time value to use in the
        substitution.

Return Value:

    Returns the number of characters written to the output buffer, including
    the null terminator.

--*/

{

    CALENDAR_TIME CalendarTime;
    ULONG Result;

    ClpStructTmToCalendarTime(&CalendarTime, (struct tm *)Time);
    ClpInitializeTimeZoneData();
    Result = RtlFormatDateWide(Buffer,
                               BufferSize,
                               (PWSTR)Format,
                               &CalendarTime);

    return Result;
}

LIBC_API
char *
strptime (
    const char *Buffer,
    const char *Format,
    struct tm *Time
    )

/*++

Routine Description:

    This routine scans the given input string into values in the calendar time,
    using the specified format.

Arguments:

    Buffer - Supplies a pointer to the null terminated string to scan.

    Format - Supplies the format string to govern the conversion. Ordinary
        characters in the format string will be scanned verbatim from the input.
        Whitespace characters in the format will cause all whitespace at the
        current position in the input to be scanned. Conversions will be
        scanned for their corresponding value in the provided calendar time.
        Conversions start with a '%' character, followed by an optional E or O
        character, followed by a conversion specifier. The conversion specifier
        can take the following values:

        %a - The day of the weekday name, either the full or abbreviated name.
        %A - Equivalent to %a.
        %b - The month name, either the full or abbreviated name.
        %B - Equivalent to %b.
        %c - Replaced by the locale's appropriate date and time representation.
        %C - The year divided by 100 (century) [00,99].
        %d - The day of the month [01,31].
        %D - Equivalent to "%m/%d/%y".
        %e - Equivalent to %d.
        %h - Equivalent to %b (month name).
        %H - The 24 hour clock hour [00,23].
        %I - The 12 hour clock hour [01,12].
        %J - Replaced by the nanosecond [0,999999999].
        %j - The day of the year [001,366].
        %m - The month number [01,12].
        %M - The minute [00,59].
        %N - The microsecond [0,999999]
        %n - Any whitespace.
        %p - The equivalent of "AM" or "PM".
        %q - The millisecond [0,999].
        %r - Replaced by the time in AM/PM notation: "%I:%M:%S %p".
        %R - Replaced by the time in 24 hour notation: "%H:%M".
        %S - The second [00,60].
        %t - Any white space.
        %T - Replaced by the time: "%H:%M:%S".
        %u - Replaced by the weekday number, with 1 representing Monday [1,7].
        %U - The week number of the year [00,53]. The first Sunday of January is
             the first day of week 1. Days before this are week 0.
        %w - The weekday number [0,6], with 0 representing Sunday.
        %W - The week number [00,53]. The first Monday of January is the first
             day of week 1. Days before this are in week 0.
        %x - Replaced by the locale's appropriate date representation.
        %X - Replaced by the locale's appropriate time representation.
        %y - The last two digits of the year [00,99].
        %Y - The full four digit year [0001,9999].
        %% - Replaced by a literal '%'.

    Time - Supplies a pointer to the calendar time value to place the
        values in. Only the values that are scanned in are modified.

Return Value:

    Returns the a pointer to the input string after the last character scanned.

    NULL if the result coult not be scanned.

--*/

{

    CALENDAR_TIME CalendarTime;
    PSTR Result;

    ClpInitializeTimeZoneData();
    Result = RtlScanDate((PSTR)Buffer, (PSTR)Format, &CalendarTime);
    if (Result == NULL) {
        return NULL;
    }

    ClpCalendarTimeToStructTm(&CalendarTime, Time);
    return Result;
}

LIBC_API
time_t
time (
    time_t *Result
    )

/*++

Routine Description:

    This routine returns the current time in terms of seconds from the Epoch,
    midnight on January 1, 1970 GMT.

Arguments:

    Result - Supplies an optional pointer where the current time will be
        returned. This will be the same as the return value.

Return Value:

    Returns the current time since the epoch.

--*/

{

    SYSTEM_TIME SystemTime;
    time_t Time;

    OsGetSystemTime(&SystemTime);
    Time = ClpConvertSystemTimeToUnixTime(&SystemTime);
    if (Result != NULL) {
        *Result = Time;
    }

    return Time;
}

LIBC_API
int
gettimeofday (
    struct timeval *Time,
    void *UnusedParameter
    )

/*++

Routine Description:

    This routine returns the current time in terms of seconds from the Epoch,
    midnight on January 1, 1970 GMT. The timezone is always GMT.

Arguments:

    Time - Supplies a pointer where the result will be returned.

    UnusedParameter - Supplies an unused parameter provided for legacy reasons.
        It used to store the current time zone.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    time_t Seconds;
    SYSTEM_TIME SystemTime;

    if (Time != NULL) {
        OsGetSystemTime(&SystemTime);
        Seconds = ClpConvertSystemTimeToUnixTime(&SystemTime);
        Time->tv_sec = Seconds;
        Time->tv_usec = SystemTime.Nanoseconds / NANOSECONDS_PER_MICROSECOND;
    }

    return 0;
}

LIBC_API
int
settimeofday (
    const struct timeval *NewTime,
    void *UnusedParameter
    )

/*++

Routine Description:

    This routine sets the current time in terms of seconds from the Epoch,
    midnight on January 1, 1970 GMT. The timezone is always GMT. The caller
    must have appropriate privileges to set the system time.

Arguments:

    NewTime - Supplies a pointer where the result will be returned.

    UnusedParameter - Supplies an unused parameter provided for legacy reasons.
        It used to provide the current time zone.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    KSTATUS Status;
    SYSTEM_TIME SystemTime;
    ULONGLONG TimeCounter;

    if (NewTime != NULL) {

        //
        // Invalid micrseconds are not allowed.
        //

        if ((NewTime->tv_usec < 0) ||
            (NewTime->tv_usec > MICROSECONDS_PER_SECOND)) {

            errno = EINVAL;
            return -1;
        }

        ClpConvertTimeValueToSystemTime(&SystemTime, NewTime);
        TimeCounter = OsGetRecentTimeCounter();
        Status = OsSetSystemTime(&SystemTime, TimeCounter);
        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }
    }

    return 0;
}

LIBC_API
int
timer_create (
    clockid_t ClockId,
    struct sigevent *Event,
    timer_t *TimerId
    )

/*++

Routine Description:

    This routine creates a new timer.

Arguments:

    ClockId - Supplies the clock type ID. See CLOCK_* definitions. The most
        common value here is CLOCK_REALTIME.

    Event - Supplies a pointer to an event structure describing what should
        happen when the timer expires. If this parameter is NULL, then the
        timer will be treated as if this structure had specified that a
        SIGALRM signal should be generated with the timer ID number set as the
        signal value.

    TimerId - Supplies a pointer where the timer ID number will be returned on
        success.

Return Value:

    0 on success. The returned timer ID will be in the timer parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    INT Result;
    ULONG Signal;
    UINTN SignalValue;
    KSTATUS Status;
    THREAD_ID ThreadId;
    PTHREAD_ID ThreadIdPointer;
    PTIMER Timer;

    Timer = NULL;
    if ((ClockId != CLOCK_REALTIME) &&
        (ClockId != CLOCK_MONOTONIC) &&
        (ClockId != CLOCK_PROCESS_CPUTIME_ID) &&
        (ClockId != CLOCK_THREAD_CPUTIME_ID)) {

        errno = EINVAL;
        Result = -1;
        goto TimerCreateEnd;
    }

    if ((ClockId == CLOCK_PROCESS_CPUTIME_ID) ||
        (ClockId == CLOCK_THREAD_CPUTIME_ID)) {

        errno = ENOTSUP;
        Result = -1;
        goto TimerCreateEnd;
    }

    Timer = malloc(sizeof(TIMER));
    if (Timer == NULL) {
        errno = ENOMEM;
        Result = -1;
        goto TimerCreateEnd;
    }

    ThreadIdPointer = NULL;
    if (Event != NULL) {
        if (Event->sigev_notify == SIGEV_THREAD_ID) {
            ThreadId = (THREAD_ID)Event->sigev_notify_thread_id;
            ThreadIdPointer = &ThreadId;

        } else {

            //
            // Currently creating a new thread to signal isn't supported. Add
            // this support if it comes up and is deemed necessary.
            //

            assert(Event->sigev_notify == SIGEV_SIGNAL);

        }

        Signal = Event->sigev_signo;
        SignalValue = Event->sigev_value.sival_int;

    } else {
        Signal = SIGALRM;
        SignalValue = (UINTN)Timer;
    }

    Status = OsCreateTimer(Signal,
                           &SignalValue,
                           ThreadIdPointer,
                           &(Timer->Handle));

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        Result = -1;
        goto TimerCreateEnd;
    }

    Timer->ClockId = ClockId;
    Result = 0;

TimerCreateEnd:
    if (Result == -1) {
        if (Timer != NULL) {
            free(Timer);
            Timer = NULL;
        }
    }

    *TimerId = (timer_t)Timer;
    return Result;
}

LIBC_API
int
timer_delete (
    timer_t TimerId
    )

/*++

Routine Description:

    This routine disarms and deletes the timer with the given ID.

Arguments:

    TimerId - Supplies the ID of the timer to delete.

Return Value:

    0 on success.

    -1 on failure and errno will be set to EINVAL if the given timer handle is
    invalid.

--*/

{

    KSTATUS Status;
    PTIMER Timer;

    Timer = (PTIMER)TimerId;
    if (Timer == NULL) {
        errno = EINVAL;
        return -1;
    }

    Status = OsDeleteTimer(Timer->Handle);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    free(Timer);
    return 0;
}

LIBC_API
int
timer_gettime (
    timer_t TimerId,
    struct itimerspec *Value
    )

/*++

Routine Description:

    This routine gets the current timer information for the given timer.

Arguments:

    TimerId - Supplies the ID of the timer to query.

    Value - Supplies a pointer where the remaining time on the timer will be
        returned.

Return Value:

    0 on success.

    -1 on failure and errno will be set to contain more information.

--*/

{

    ULONGLONG CurrentTime;
    ULONGLONG Delta;
    ULONGLONG Frequency;
    TIMER_INFORMATION Information;
    KSTATUS Status;
    PTIMER Timer;

    Timer = (PTIMER)TimerId;
    if (Timer == NULL) {
        errno = EINVAL;
        return -1;
    }

    Status = OsGetTimerInformation(Timer->Handle, &Information);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    //
    // Convert the absolute due time in time counter ticks into remaining
    // seconds and nanoseconds.
    //

    Frequency = OsGetTimeCounterFrequency();
    CurrentTime = OsQueryTimeCounter();
    Delta = Information.DueTime - CurrentTime;
    ClpConvertCounterToSpecificTime(Delta, Frequency, &(Value->it_value));

    //
    // Convert the period in time counter ticks to seconds and nanoseconds.
    //

    ClpConvertCounterToSpecificTime(Information.Period,
                                    Frequency,
                                    &(Value->it_interval));

    return 0;
}

LIBC_API
int
timer_settime (
    timer_t TimerId,
    int Flags,
    const struct itimerspec *Value,
    struct itimerspec *OldValue
    )

/*++

Routine Description:

    This routine sets the current timer information for the given timer.

Arguments:

    TimerId - Supplies the ID of the timer to set.

    Flags - Supplies a bitfield of flags. See TIMER_ABSTIME and friends.

    Value - Supplies a pointer where the remaining time on the timer will be
        returned.

    OldValue - Supplies an optional pointer where the structure containing the
        remaining time on the timer before this call will be returned.

Return Value:

    0 on success.

    -1 on failure and errno will be set to contain more information.

--*/

{

    ULONGLONG CurrentTime;
    BOOL CurrentTimeValid;
    ULONG Frequency;
    TIMER_INFORMATION Information;
    KSTATUS Status;
    SYSTEM_TIME SystemTime;
    ULONGLONG Time;
    PTIMER Timer;

    Timer = (PTIMER)TimerId;
    if (Timer == NULL) {
        errno = EINVAL;
        return -1;
    }

    CurrentTime = 0;
    CurrentTimeValid = FALSE;
    RtlZeroMemory(&Information, sizeof(TIMER_INFORMATION));
    Frequency = OsGetTimeCounterFrequency();

    //
    // If the absolute time is supplied for the real-time clock, then convert
    // the value into an absolute due time in time counter ticks.
    //

    if ((Timer->ClockId == CLOCK_REALTIME) && ((Flags & TIMER_ABSTIME) != 0)) {
        ClpConvertSpecificTimeToSystemTime(&SystemTime, &(Value->it_value));
        OsConvertSystemTimeToTimeCounter(&SystemTime, &(Information.DueTime));

    //
    // Otherwise convert the requested time into a timer counter value. The
    // conversion is the same for relative times and absolute monotonic clock
    // values.
    //

    } else {
        ClpConvertSpecificTimeToCounter(&(Information.DueTime),
                                        Frequency,
                                        &(Value->it_value));

        if ((Flags & TIMER_ABSTIME) == 0) {
            CurrentTime = OsQueryTimeCounter();
            CurrentTimeValid = TRUE;
            Information.DueTime += CurrentTime;
        }
    }

    //
    // Convert the relative period into a relative time counter value.
    //

    ClpConvertSpecificTimeToCounter(&(Information.Period),
                                    Frequency,
                                    &(Value->it_interval));

    Status = OsSetTimerInformation(Timer->Handle, &Information);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    //
    // Return the old value if requested.
    //

    if (OldValue != NULL) {

        //
        // Convert the absolute time counter into a relative time structure.
        //

        Time = Information.DueTime;
        if (Time != 0) {
            if (CurrentTimeValid == FALSE) {
                CurrentTime = OsQueryTimeCounter();
            }

            Time -= CurrentTime;
        }

        ClpConvertCounterToSpecificTime(Time,
                                        Frequency,
                                        &(OldValue->it_value));

        //
        // Convert the relative period in time counter ticks to a relative
        // time structure.
        //

        ClpConvertCounterToSpecificTime(Information.Period,
                                        Frequency,
                                        &(OldValue->it_interval));
    }

    return 0;
}

LIBC_API
int
timer_getoverrun (
    timer_t TimerId
    )

/*++

Routine Description:

    This routine returns the overrun count for the given timer. The overrun
    count can be queried during a signal, and represents the number of
    additional timer expiries that occurred before the signal was accepted by
    the process. If added to the count of signals that have occurred it
    represents the total number of expiries of the given periodic timer.

Arguments:

    TimerId - Supplies the timer to query.

Return Value:

    Returns the overrun count on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    TIMER_INFORMATION Information;
    KSTATUS Status;
    PTIMER Timer;

    Timer = (PTIMER)TimerId;
    Status = OsGetTimerInformation(Timer->Handle, &Information);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return Information.OverflowCount;
}

LIBC_API
int
getitimer (
    int Type,
    struct itimerval *CurrentValue
    )

/*++

Routine Description:

    This routine gets the current value of one of the interval timers.

Arguments:

    Type - Supplies the timer type to get information for. See ITIMER_*
        definitions for details.

    CurrentValue - Supplies a pointer where the current due time and period of
        the timer will be returned, in relative seconds from now. Zero will
        be returned in the value portion if the timer is not currently armed
        or has already expired.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    ULONGLONG DueTime;
    ULONGLONG Frequency;
    ULONGLONG Period;
    KSTATUS Status;

    ASSERT_ITIMER_TYPES_EQUIVALENT();

    if (Type == ITIMER_REAL) {
        Frequency = OsGetTimeCounterFrequency();

    } else {
        Frequency = OsGetProcessorCounterFrequency();
    }

    Status = OsGetITimer(Type, &DueTime, &Period);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClpConvertCounterToTimeValue(DueTime, Frequency, &(CurrentValue->it_value));
    ClpConvertCounterToTimeValue(Period,
                                 Frequency,
                                 &(CurrentValue->it_interval));

    return 0;
}

LIBC_API
int
setitimer (
    int Type,
    const struct itimerval *NewValue,
    struct itimerval *OldValue
    )

/*++

Routine Description:

    This routine sets the current value of one of the interval timers.

Arguments:

    Type - Supplies the timer type to get information for. See ITIMER_*
        definitions for details.

    NewValue - Supplies a pointer to the new relative value and period to set
        in the timer.

    OldValue - Supplies an optional pointer where the remaining time left
        on the timer and the period before the set operation will be returned.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    ULONGLONG DueTime;
    ULONGLONG Frequency;
    ULONGLONG Period;
    KSTATUS Status;

    ASSERT_ITIMER_TYPES_EQUIVALENT();

    if ((NewValue->it_value.tv_usec >= MICROSECONDS_PER_SECOND) ||
        (NewValue->it_interval.tv_usec >= MICROSECONDS_PER_SECOND)) {

        errno = EINVAL;
        return -1;
    }

    if (Type == ITIMER_REAL) {
        Frequency = OsGetTimeCounterFrequency();

    } else {
        Frequency = OsGetProcessorCounterFrequency();
    }

    ClpConvertTimeValueToCounter(&DueTime, Frequency, &(NewValue->it_value));
    ClpConvertTimeValueToCounter(&Period, Frequency, &(NewValue->it_interval));
    Status = OsSetITimer(Type, &DueTime, &Period);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    if (OldValue != NULL) {
        ClpConvertCounterToTimeValue(DueTime, Frequency, &(OldValue->it_value));
        ClpConvertCounterToTimeValue(Period,
                                     Frequency,
                                     &(OldValue->it_interval));
    }

    return 0;
}

LIBC_API
void
tzset (
    void
    )

/*++

Routine Description:

    This routine uses the values of the TZ environment variable to set time
    conversion information used by ctime, localtime, mktime, and strftime. If
    TZ is absent from the environment, a default timezone will be used.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ClpInitializeTimeZoneData();
    return;
}

LIBC_API
unsigned
sleep (
    unsigned Seconds
    )

/*++

Routine Description:

    This routine suspends execution of the calling thread until either the
    given number of realtime seconds has elapsed or a signal was delivered.

Arguments:

    Seconds - Supplies the number of seconds to sleep for.

Return Value:

    None.

--*/

{

    ULONGLONG DesiredEndTime;
    ULONGLONG EndTime;
    ULONGLONG Frequency;
    unsigned RemainingSeconds;
    ULONGLONG RemainingTicks;
    ULONGLONG StartTime;
    KSTATUS Status;

    StartTime = OsGetRecentTimeCounter();
    Status = OsDelayExecution(FALSE, Seconds * MICROSECONDS_PER_SECOND);

    //
    // If the wait was not successful, compute the remaining time on the wait.
    //

    if (!KSUCCESS(Status)) {
        Frequency = OsGetTimeCounterFrequency();
        DesiredEndTime = StartTime + (Seconds * Frequency);
        EndTime = OsGetRecentTimeCounter();
        if (EndTime >= DesiredEndTime) {
            return 0;
        }

        //
        // Round up to the nearest remaining seconds.
        //

        RemainingTicks = DesiredEndTime - EndTime;
        RemainingSeconds = (RemainingTicks + (Frequency - 1)) / Frequency;
        return RemainingSeconds;
    }

    return 0;
}

LIBC_API
int
usleep (
    useconds_t Microseconds
    )

/*++

Routine Description:

    This routine suspends execution of the calling thread until either the
    given number of realtime microseconds has elapsed or a signal was delivered.

Arguments:

    Microseconds - Supplies the number of microseconds to sleep for.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    OsDelayExecution(FALSE, Microseconds);
    return 0;
}

LIBC_API
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

    ULONGLONG DesiredEndTime;
    ULONGLONG EndTime;
    ULONGLONG Frequency;
    ULONGLONG RemainingTicks;
    KSTATUS Status;

    if (RequestedTime == NULL) {
        return 0;
    }

    //
    // Convert from the given time interval (in seconds and nanoseconds) to
    // time ticks. Be careful to round up.
    //

    Frequency = OsGetTimeCounterFrequency();
    ClpConvertSpecificTimeToCounter(&DesiredEndTime, Frequency, RequestedTime);
    DesiredEndTime += OsQueryTimeCounter();
    Status = OsDelayExecution(TRUE, DesiredEndTime);

    //
    // If the wait was not successful, compute the remaining time on the wait.
    //

    if (!KSUCCESS(Status)) {
        EndTime = OsQueryTimeCounter();
        if (EndTime >= DesiredEndTime) {
            return 0;
        }

        //
        // Return the remaining time if requested. Round up here to the nearest
        // nanosecond.
        //

        if (RemainingTime != NULL) {
            RemainingTicks = DesiredEndTime - EndTime;
            ClpConvertCounterToSpecificTime(RemainingTicks,
                                            Frequency,
                                            RemainingTime);
        }

        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

VOID
ClpInitializeTimeZoneSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for time zones.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OsInitializeLockDefault(&ClTimeZoneLock);
    RtlInitializeTimeZoneSupport(ClpAcquireTimeZoneLock,
                                 ClpReleaseTimeZoneLock,
                                 (PTIME_ZONE_REALLOCATE_FUNCTION)realloc);

    return;
}

time_t
ClpConvertSystemTimeToUnixTime (
    PSYSTEM_TIME SystemTime
    )

/*++

Routine Description:

    This routine converts the given system time structure into a time_t time
    value. Fractional seconds are truncated.

Arguments:

    SystemTime - Supplies a pointer to the system time structure.

Return Value:

    Returns the time_t value corresponding to the given system time.

--*/

{

    LONGLONG AdjustedSeconds;
    time_t Result;

    AdjustedSeconds = SystemTime->Seconds + SYSTEM_TIME_TO_EPOCH_DELTA;
    Result = (time_t)AdjustedSeconds;
    return Result;
}

VOID
ClpConvertUnixTimeToSystemTime (
    PSYSTEM_TIME SystemTime,
    time_t UnixTime
    )

/*++

Routine Description:

    This routine converts the given time_t value into a system time structure.
    Fractional seconds in the system time structure are set to zero.

Arguments:

    SystemTime - Supplies a pointer to the system time structure to initialize.

    UnixTime - Supplies the time to set.

Return Value:

    None.

--*/

{

    SystemTime->Seconds = UnixTime - SYSTEM_TIME_TO_EPOCH_DELTA;
    SystemTime->Nanoseconds = 0;
    return;
}

VOID
ClpConvertTimeValueToSystemTime (
    PSYSTEM_TIME SystemTime,
    const struct timeval *TimeValue
    )

/*++

Routine Description:

    This routine converts the given time value into a system time structure.

Arguments:

    SystemTime - Supplies a pointer to the system time structure to initialize.

    TimeValue - Supplies a pointer to the time value structure to be converted
        to system time.

Return Value:

    None.

--*/

{

    LONGLONG Microseconds;
    LONGLONG Seconds;

    //
    // First conver the seconds from Unix time to system time.
    //

    ClpConvertUnixTimeToSystemTime(SystemTime, TimeValue->tv_sec);

    //
    // Now handle the microseconds. Don't trust that the microseconds are
    // properly bound between 0 and 1 million.
    //

    Microseconds = (LONGLONG)TimeValue->tv_usec;
    if (Microseconds < 0) {
        SystemTime->Seconds -= 1;
        Microseconds += MICROSECONDS_PER_SECOND;

    } else if (Microseconds > MICROSECONDS_PER_SECOND) {
        Seconds = Microseconds / MICROSECONDS_PER_SECOND;
        SystemTime->Seconds += Seconds;
        Microseconds -= (Seconds * MICROSECONDS_PER_SECOND);
    }

    assert((Microseconds >= 0) && (Microseconds < MICROSECONDS_PER_SECOND));

    SystemTime->Nanoseconds = Microseconds * NANOSECONDS_PER_MICROSECOND;
    return;
}

VOID
ClpConvertSpecificTimeToSystemTime (
    PSYSTEM_TIME SystemTime,
    const struct timespec *SpecificTime
    )

/*++

Routine Description:

    This routine converts the given specific time into a system time structure.

Arguments:

    SystemTime - Supplies a pointer to the system time structure to initialize.

    SpecificTime - Supplies a pointer to the specific time structure to be
        converted to system time.

Return Value:

    None.

--*/

{

    LONGLONG Nanoseconds;
    LONGLONG Seconds;

    //
    // First conver the seconds from Unix time to system time.
    //

    ClpConvertUnixTimeToSystemTime(SystemTime, SpecificTime->tv_sec);

    //
    // Now handle the nanoseconds. Don't trust that the nanoseconds are
    // properly bound between 0 and 1 billion.
    //

    Nanoseconds = (LONGLONG)SpecificTime->tv_nsec;
    if (Nanoseconds < 0) {
        SystemTime->Seconds -= 1;
        Nanoseconds += NANOSECONDS_PER_SECOND;

    } else if (Nanoseconds > NANOSECONDS_PER_SECOND) {
        Seconds = Nanoseconds / NANOSECONDS_PER_SECOND;
        SystemTime->Seconds += Seconds;
        Nanoseconds -= (Seconds * NANOSECONDS_PER_SECOND);
    }

    assert((Nanoseconds >= 0) && (Nanoseconds < NANOSECONDS_PER_SECOND));

    SystemTime->Nanoseconds = Nanoseconds;
    return;
}

VOID
ClpConvertCounterToTimeValue (
    ULONGLONG Counter,
    ULONGLONG Frequency,
    struct timeval *TimeValue
    )

/*++

Routine Description:

    This routine converts a tick count at a known frequency into a time value
    structure, rounded up to the nearest microsecond.

Arguments:

    Counter - Supplies the counter value in ticks.

    Frequency - Supplies the frequency of the counter in Hertz. This must not
        be zero.

    TimeValue - Supplies a pointer where the time value equivalent will be
        returned.

Return Value:

    None.

--*/

{

    ULONG Microseconds;
    ULONGLONG Seconds;

    Seconds = Counter / Frequency;
    TimeValue->tv_sec = Seconds;
    Counter -= (Seconds * Frequency);
    Microseconds = ((Counter * MICROSECONDS_PER_SECOND) + (Frequency - 1)) /
                   Frequency;

    TimeValue->tv_usec = Microseconds;
    return;
}

VOID
ClpConvertTimeValueToCounter (
    PULONGLONG Counter,
    ULONGLONG Frequency,
    const struct timeval *TimeValue
    )

/*++

Routine Description:

    This routine converts a time value into a tick count at a known frequency,
    rounded up to the nearest tick.

Arguments:

    Counter - Supplies a pointer that receives the calculated counter value in
        ticks.

    Frequency - Supplies the frequency of the counter in Hertz. This must not
        be zero.

    TimeValue - Supplies a pointer to the time value.

Return Value:

    None.

--*/

{

    ULONGLONG LocalCounter;
    ULONGLONG Value;

    LocalCounter = (LONGLONG)TimeValue->tv_sec * Frequency;
    Value = (LONGLONG)TimeValue->tv_usec * Frequency;
    LocalCounter += (Value + (MICROSECONDS_PER_SECOND - 1)) /
                    MICROSECONDS_PER_SECOND;

    *Counter = LocalCounter;
    return;
}

VOID
ClpConvertCounterToSpecificTime (
    ULONGLONG Counter,
    ULONGLONG Frequency,
    struct timespec *SpecificTime
    )

/*++

Routine Description:

    This routine converts a tick count at a known frequency into a specific
    time structure, rounded up to the nearest nanosecond.

Arguments:

    Counter - Supplies the counter value in ticks.

    Frequency - Supplies the frequency of the counter in Hertz. This must not
        be zero.

    SpecificTime - Supplies a pointer where the specific time equivalent will
        be returned.

Return Value:

    None.

--*/

{

    ULONG Nanoseconds;
    ULONGLONG Seconds;

    Seconds = Counter / Frequency;
    SpecificTime->tv_sec = Seconds;
    Counter -= (Seconds * Frequency);
    Nanoseconds = ((Counter * NANOSECONDS_PER_SECOND) + (Frequency - 1)) /
                   Frequency;

    SpecificTime->tv_nsec = Nanoseconds;
    return;
}

VOID
ClpConvertSpecificTimeToCounter (
    PULONGLONG Counter,
    ULONGLONG Frequency,
    const struct timespec *SpecificTime
    )

/*++

Routine Description:

    This routine converts a specific time into a tick count at a known
    frequency, rounded up to the nearest tick.

Arguments:

    Counter - Supplies a pointer that receives the calculated counter value in
        ticks.

    Frequency - Supplies the frequency of the counter in Hertz. This must not
        be zero.

    SpecificTime - Supplies a pointer to the specific time.

Return Value:

    None.

--*/

{

    ULONGLONG LocalCounter;
    ULONGLONG Value;

    LocalCounter = (LONGLONG)SpecificTime->tv_sec * Frequency;
    Value = (LONGLONG)SpecificTime->tv_nsec * Frequency;
    LocalCounter += (Value + (NANOSECONDS_PER_SECOND - 1)) /
                    NANOSECONDS_PER_SECOND;

    *Counter = LocalCounter;
    return;
}

INT
ClpConvertSpecificTimeoutToSystemTimeout (
    const struct timespec *SpecificTimeout,
    PULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine converts the given specific timeout into a system timeout in
    milliseconds. The specific timeout's seconds and nanoseconds must not be
    negative and the nanoseconds must not be greater than 1 billion (the number
    of nanoseconds in a second). If the specific timeout is NULL, then the
    timeout in milliseconds will be set to an indefinite timeout.

Arguments:

    SpecificTimeout - Supplies an optional pointer to the specific timeout.

    TimeoutInMilliseconds - Supplies a pointer that receives the system timeout
        in milliseconds.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    time_t MaxSeconds;

    if (SpecificTimeout == NULL) {
        *TimeoutInMilliseconds = SYS_WAIT_TIME_INDEFINITE;
        return 0;
    }

    //
    // The specific timeout must be positive and the nanoseconds must be
    // between 0 and 1 billion.
    //

    if ((SpecificTimeout->tv_sec < 0) ||
        (SpecificTimeout->tv_nsec < 0) ||
        (SpecificTimeout->tv_nsec >= NANOSECONDS_PER_SECOND)){

        return EINVAL;
    }

    //
    // Accounting for the nanoseconds field adding at most 999 milliseconds, if
    // the seconds field is too large, truncate the value to the maximum system
    // timeout. This may inaccurately round up as the max wait time is not
    // evenly divisible by 1000, but that's just splitting hairs when the
    // timeout is around 49 days.
    //

    MaxSeconds = (SYS_WAIT_TIME_MAX - MILLISECONDS_PER_SECOND) /
                 MILLISECONDS_PER_SECOND;

    if (SpecificTimeout->tv_sec > MaxSeconds) {
        *TimeoutInMilliseconds = SYS_WAIT_TIME_MAX;

    //
    // Otherwise, calculate the milliseconds. This should be safe from overflow
    // as the nanoseconds and seconds were bound above.
    //

    } else {
        *TimeoutInMilliseconds =
                      (SpecificTimeout->tv_sec * MILLISECONDS_PER_SECOND) +
                      ((SpecificTimeout->tv_nsec +
                        (NANOSECONDS_PER_MILLISECOND - 1)) /
                       NANOSECONDS_PER_MILLISECOND);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ClpCalendarTimeToStructTm (
    PCALENDAR_TIME CalendarTime,
    struct tm *StructTm
    )

/*++

Routine Description:

    This routine converts the given calendar time structure into the C library
    tm structure.

Arguments:

    CalendarTime - Supplies a pointer to the calendar time.

    StructTm - Supplies a pointer where the tm structure will be returned.

Return Value:

    None.

--*/

{

    StructTm->tm_sec = CalendarTime->Second;
    StructTm->tm_min = CalendarTime->Minute;
    StructTm->tm_hour = CalendarTime->Hour;
    StructTm->tm_mday = CalendarTime->Day;
    StructTm->tm_mon = CalendarTime->Month;
    StructTm->tm_year = CalendarTime->Year - 1900;
    StructTm->tm_wday = CalendarTime->Weekday;
    StructTm->tm_yday = CalendarTime->YearDay;
    StructTm->tm_isdst = CalendarTime->IsDaylightSaving;
    StructTm->tm_nanosecond = CalendarTime->Nanosecond;
    StructTm->tm_gmtoff = CalendarTime->GmtOffset;
    StructTm->tm_zone = CalendarTime->TimeZone;
    return;
}

VOID
ClpStructTmToCalendarTime (
    PCALENDAR_TIME CalendarTime,
    struct tm *StructTm
    )

/*++

Routine Description:

    This routine converts the given C library tm structure into the calendar
    time structure.

Arguments:

    CalendarTime - Supplies a pointer to the calendar time.

    StructTm - Supplies a pointer where the tm structure will be returned.

Return Value:

    None.

--*/

{

    CalendarTime->Year = StructTm->tm_year + 1900;
    CalendarTime->Month = StructTm->tm_mon;
    CalendarTime->Day = StructTm->tm_mday;
    CalendarTime->Hour = StructTm->tm_hour;
    CalendarTime->Minute = StructTm->tm_min;
    CalendarTime->Second = StructTm->tm_sec;
    CalendarTime->Nanosecond = StructTm->tm_nanosecond;
    CalendarTime->Weekday = StructTm->tm_wday;
    CalendarTime->YearDay = StructTm->tm_yday;
    CalendarTime->IsDaylightSaving = StructTm->tm_isdst;
    CalendarTime->GmtOffset = StructTm->tm_gmtoff;
    CalendarTime->TimeZone = StructTm->tm_zone;
    return;
}

VOID
ClpInitializeTimeZoneData (
    VOID
    )

/*++

Routine Description:

    This routine ensures that the local time zone data is initialized before
    proceeding.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PCSTR DaylightName;
    LONG DaylightOffset;
    INT Errno;
    KSTATUS KStatus;
    PVOID OldData;
    ULONG OldDataSize;
    PCSTR StandardName;
    LONG StandardOffset;
    struct stat Stat;
    KSTATUS Status;
    PSTR Variable;
    PVOID ZoneData;
    UINTN ZoneDataSize;
    INT ZoneFile;
    PSTR ZoneName;
    PSTR ZonePath;

    Errno = errno;
    OldData = NULL;
    ZoneData = NULL;
    ZoneName = NULL;
    ZonePath = _PATH_TZ;
    Variable = getenv("TZ");
    if (Variable != NULL) {
        if ((ClPreviousTzVariable == NULL) ||
            (strcmp(ClPreviousTzVariable, Variable) != 0)) {

            if (ClPreviousTzVariable != NULL) {
                free(ClPreviousTzVariable);
            }

            ClPreviousTzVariable = strdup(Variable);
            if (ClPreviousTzVariable == NULL) {
                Status = -1;
                goto InitializeTimeZoneDataEnd;
            }

            Variable = ClPreviousTzVariable;

            //
            // If the variable starts with a colon or has a slash and no comma,
            // then use the OS-specific format (non-POSIX). This specifies
            // either a path to a timezone file to use (if it starts with a
            // slash) or a time zone name.
            //

            if ((*Variable == ':') ||
                ((strchr(Variable, '/') != NULL) &&
                 (strchr(Variable, ',') == NULL))) {

                if (*Variable == ':') {
                    Variable += 1;
                }

                if (*Variable == '/') {
                    ZonePath = Variable;

                } else {
                    ZonePath = _PATH_TZALMANAC;
                    ZoneName = Variable;
                }

            } else {
                Status = ClpCreateCustomTimeZone(Variable,
                                                 &ZoneData,
                                                 &ZoneDataSize);

                if (Status != 0) {
                    goto InitializeTimeZoneDataEnd;
                }
            }

        //
        // Fast path: The TZ variable is set but has not changed.
        //

        } else {
            return;
        }

    //
    // TZ is not set.
    //

    } else {

        //
        // If the zone data has already been loaded, then everything's already
        // initialized.
        //

        if (ClTimeZonePath != NULL) {
            return;
        }

        //
        // If it just went from set to unset, clear the variable value.
        //

        if (ClPreviousTzVariable != NULL) {
            free(ClPreviousTzVariable);
            ClPreviousTzVariable = NULL;
        }
    }

    //
    // If parsing the TZ variable already created a time zone structure, then
    // just use that. Clear the path.
    //

    if (ZoneData != NULL) {
        if (ClTimeZonePath != NULL) {
            free(ClTimeZonePath);
        }

        ClTimeZonePath = NULL;

    //
    // Load up the new data if needed.
    //

    } else if ((ClTimeZonePath == NULL) ||
               (strcmp(ClTimeZonePath, ZonePath) != 0)) {

        ZoneFile = open(ZonePath, O_RDONLY);
        if (ZoneFile < 0) {
            goto InitializeTimeZoneDataEnd;
        }

        if (fstat(ZoneFile, &Stat) != 0) {
            close(ZoneFile);
            goto InitializeTimeZoneDataEnd;
        }

        ZoneDataSize = Stat.st_size;
        ZoneData = mmap(NULL,
                        ZoneDataSize,
                        PROT_READ,
                        MAP_PRIVATE,
                        ZoneFile,
                        0);

        close(ZoneFile);
        if (ZoneData == MAP_FAILED) {
            goto InitializeTimeZoneDataEnd;
        }

        if (ClTimeZonePath != NULL) {
            free(ClTimeZonePath);
        }

        ClTimeZonePath = strdup(ZonePath);
    }

    //
    // If the zone data was never loaded, then it's the same as what was
    // loaded before. Just the name has changed.
    //

    if (ZoneData == NULL) {
        KStatus = RtlSelectTimeZone(ZoneName, NULL, NULL);

    } else {
        KStatus = RtlSetTimeZoneData(ZoneData,
                                    ZoneDataSize,
                                    ZoneName,
                                    &OldData,
                                    &OldDataSize,
                                    NULL,
                                    NULL);
    }

    if (!KSUCCESS(KStatus)) {
        Status = -1;
        goto InitializeTimeZoneDataEnd;
    }

    if ((OldData != NULL) && (OldData != ZoneData)) {
        munmap(OldData, OldDataSize);
    }

    //
    // Get the time zone names.
    //

    RtlGetTimeZoneNames(&StandardName,
                        &DaylightName,
                        &StandardOffset,
                        &DaylightOffset);

    tzname[0] = (PSTR)StandardName;
    tzname[1] = (PSTR)DaylightName;
    timezone = -StandardOffset;
    if (StandardOffset != DaylightOffset) {
        daylight = 1;

    } else {
        daylight = 0;
    }

    Status = 0;

InitializeTimeZoneDataEnd:
    if (Status != 0) {
        if (ZoneData != NULL) {
            munmap(ZoneData, ZoneDataSize);
        }
    }

    errno = Errno;
    return;
}

INT
ClpCreateCustomTimeZone (
    PSTR TimeZoneVariable,
    PVOID *TimeZoneData,
    UINTN *TimeZoneDataSize
    )

/*++

Routine Description:

    This routine parses the time zone variable to create a custom time zone
    definition.

Arguments:

    TimeZoneVariable - Supplies a pointer to the TZ environment variable value
        string.

    TimeZoneData - Supplies a pointer where the allocated time zone data will
        be returned on success. The caller is responsible for munmapping this
        data when finished.

    TimeZoneDataSize - Supplies a pointer where the size of the time zone data
        in bytes will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR CurrentString;
    PCUSTOM_TIME_ZONE Custom;
    UINTN CustomSize;
    CHAR DaylightName[CUSTOM_TIME_ZONE_NAME_MAX];
    LONG DaylightOffset;
    INT Phase;
    CHAR StandardName[CUSTOM_TIME_ZONE_NAME_MAX];
    LONG StandardOffset;
    INT Status;
    UINTN StringLength;
    PSTR Strings;

    Phase = 1;
    CurrentString = NULL;
    StandardOffset = 0;
    Strings = NULL;
    CustomSize = sizeof(CUSTOM_TIME_ZONE);

    //
    // Use mmap because that's what's normally used to map the data file, so
    // munmap is what gets used to free it.
    //

    Custom = mmap(NULL,
                  CustomSize,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS,
                  -1,
                  0);

    if (Custom == MAP_FAILED) {
        Custom = NULL;
        Status = errno;
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;
    Custom->Header.Magic = TIME_ZONE_HEADER_MAGIC;
    Custom->Header.RuleOffset = FIELD_OFFSET(CUSTOM_TIME_ZONE, StandardRule);
    Custom->Header.RuleCount = 2;
    Custom->Header.ZoneOffset = FIELD_OFFSET(CUSTOM_TIME_ZONE, Zone);
    Custom->Header.ZoneCount = 1;
    Custom->Header.ZoneEntryOffset = FIELD_OFFSET(CUSTOM_TIME_ZONE, ZoneEntry);
    Custom->Header.ZoneEntryCount = 1;
    Custom->Header.StringsOffset = FIELD_OFFSET(CUSTOM_TIME_ZONE, Strings);

    //
    // Ignore the custom format for now.
    //

    if (*TimeZoneVariable == ':') {
        Status = ENOTSUP;
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;
    DaylightName[0] = '\0';
    Status = ClpReadCustomTimeZoneName(&TimeZoneVariable, StandardName);
    if (Status != 0) {
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;
    if (*TimeZoneVariable != '\0') {
        Status = ClpReadCustomTimeOffset(&TimeZoneVariable, &StandardOffset);
        if (Status != 0) {
            goto CreateCustomTimeZoneEnd;
        }
    }

    Phase += 1;
    Strings = Custom->Strings;
    CurrentString = Strings;
    *CurrentString = '\0';
    CurrentString += 1;
    StringLength = strlen(StandardName);
    strcpy(CurrentString, StandardName);
    Custom->Zone.Name = CurrentString - Strings;
    Custom->Zone.EntryIndex = 0;
    Custom->Zone.EntryCount = 1;
    Custom->ZoneEntry.GmtOffset = StandardOffset;
    Custom->ZoneEntry.Rules = -1;
    Custom->ZoneEntry.Format = CurrentString - Strings;
    Custom->ZoneEntry.Until = MAX_TIME_ZONE_DATE;
    CurrentString += StringLength + 1;

    //
    // For strings like UTC and EST+5, the specification is complete.
    //

    if (*TimeZoneVariable == '\0') {
        Status = 0;
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;

    //
    // There must be a daylight name, and perhaps an offset.
    //

    Status = ClpReadCustomTimeZoneName(&TimeZoneVariable, DaylightName);
    if (Status != 0) {
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;
    DaylightOffset = StandardOffset + 3600;
    if ((*TimeZoneVariable == '+') || (*TimeZoneVariable == '-') ||
        (isdigit(*TimeZoneVariable))) {

        Status = ClpReadCustomTimeOffset(&TimeZoneVariable, &DaylightOffset);
        if (Status != 0) {
            goto CreateCustomTimeZoneEnd;
        }
    }

    Phase += 1;
    if (*TimeZoneVariable != ',') {
        Status = EINVAL;
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;
    TimeZoneVariable += 1;
    Custom->ZoneEntry.Rules = 1;

    //
    // Parse the start[/time],end[/time] forms.
    //

    Custom->DaylightRule.Number = 1;
    Custom->DaylightRule.Save = DaylightOffset - StandardOffset;
    Status = ClpReadCustomTimeRule(&TimeZoneVariable, &(Custom->DaylightRule));
    if (Status != 0) {
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;
    if (*TimeZoneVariable != ',') {
        Status = EINVAL;
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;
    TimeZoneVariable += 1;
    Custom->StandardRule.Number = 1;
    Custom->StandardRule.Save = 0;
    Status = ClpReadCustomTimeRule(&TimeZoneVariable, &(Custom->StandardRule));
    if (Status != 0) {
        goto CreateCustomTimeZoneEnd;
    }

    Phase += 1;
    if (*TimeZoneVariable != '\0') {
        Status = EINVAL;
        goto CreateCustomTimeZoneEnd;
    }

    //
    // Add the daylight name string, and make the zone entry string %s.
    //

    Custom->StandardRule.Letters = Custom->Zone.Name;
    StringLength = strlen(DaylightName);
    strcpy(CurrentString, DaylightName);
    Custom->DaylightRule.Letters = CurrentString - Strings;
    CurrentString += StringLength + 1;
    strcpy(CurrentString, "%s");
    Custom->ZoneEntry.Format = CurrentString - Strings;
    CurrentString += sizeof("%s");

    assert(CurrentString - Strings <= sizeof(Custom->Strings));

    Status = 0;

CreateCustomTimeZoneEnd:
    if (Custom != NULL) {
        Custom->Header.StringsSize = CurrentString - Strings;
    }

    if (ClDebugCustomTimeZoneParsing != FALSE) {
        if (Status == 0) {
            RtlDebugPrintTimeZoneData(Custom, CustomSize);

        } else {
            RtlDebugPrint("Failed to parse TZ variable, phase %d: %s\n",
                          Phase,
                          TimeZoneVariable);
        }
    }

    if (Status != 0) {
        if (Custom != NULL) {
            munmap(Custom, CustomSize);
            Custom = NULL;
            CustomSize = 0;
        }
    }

    *TimeZoneData = Custom;
    *TimeZoneDataSize = CustomSize;
    return Status;
}

INT
ClpReadCustomTimeZoneName (
    PSTR *Variable,
    CHAR ParsedName[CUSTOM_TIME_ZONE_NAME_MAX]
    )

/*++

Routine Description:

    This routine reads a time zone name from the TZ string.

Arguments:

    Variable - Supplies a pointer that on input contains a pointer to the start
        of the name. On output, this string pointer is advanced.

    ParsedName - Supplies a pointer where the null terminated name will be
        returned.

Return Value:

    0 on success.

    EINVAL if the name format is invalid.

--*/

{

    INTN Index;
    PSTR Name;

    Name = *Variable;
    Index = 0;
    while ((Index < CUSTOM_TIME_ZONE_NAME_MAX - 1) &&
           (isalpha(*Name))) {

        ParsedName[Index] = *Name;
        Name += 1;
        Index += 1;
    }

    ParsedName[Index] = '\0';
    while (isalpha(*Name)) {
        Name += 1;
        Index += 1;
    }

    *Variable = Name;
    if (Index < 3) {
        return EINVAL;
    }

    return 0;
}

INT
ClpReadCustomTimeOffset (
    PSTR *Variable,
    PLONG Seconds
    )

/*++

Routine Description:

    This routine reads a time zone offset from the TZ string.

Arguments:

    Variable - Supplies a pointer that on input contains a pointer to the start
        of the offset. On output, this string pointer is advanced.

    Seconds - Supplies a pointer where the seconds to add to UTC for the time
        zone will be returned on success.

Return Value:

    0 on success.

    EINVAL if the name format is invalid.

--*/

{

    BOOL Negative;
    INT Status;
    PSTR String;
    LONG Value;

    *Seconds = 0;
    String = *Variable;

    //
    // Parse [+-] hours.
    //

    Negative = FALSE;
    if (*String == '+') {
        String += 1;

    } else if (*String == '-') {
        Negative = TRUE;
        String += 1;
    }

    if (!isdigit(*String)) {
        Status = EINVAL;
        goto ReadCustomTimeOffsetEnd;
    }

    Value = *String - '0';
    String += 1;
    if (isdigit(*String)) {
        Value *= 10;
        Value += *String - '0';
        String += 1;
    }

    *Seconds += Value * 3600;
    if (*String != ':') {
        Status = 0;
        goto ReadCustomTimeOffsetEnd;
    }

    String += 1;

    //
    // Parse the minutes.
    //

    Value = *String - '0';
    String += 1;
    if (isdigit(*String)) {
        Value *= 10;
        Value += *String - '0';
        String += 1;
    }

    *Seconds += Value * 60;
    if (*String != ':') {
        Status = 0;
        goto ReadCustomTimeOffsetEnd;
    }

    String += 1;

    //
    // Parse the seconds.
    //

    Value = *String - '0';
    String += 1;
    if (isdigit(*String)) {
        Value *= 10;
        Value += *String - '0';
        String += 1;
    }

    *Seconds += Value;
    Status = 0;

ReadCustomTimeOffsetEnd:

    //
    // Positive values are west of the meridian, so are negative seconds.
    //

    if (Negative == FALSE) {
        *Seconds = -*Seconds;
    }

    *Variable = String;
    return Status;
}

INT
ClpReadCustomTimeRule (
    PSTR *Variable,
    PTIME_ZONE_RULE Rule
    )

/*++

Routine Description:

    This routine reads a time zone rule from the TZ string.

Arguments:

    Variable - Supplies a pointer that on input contains a pointer to the start
        of the offset. On output, this string pointer is advanced.

    Rule - Supplies a pointer where the rule change date will be returned on
        success.

Return Value:

    0 on success.

    EINVAL if the name format is invalid.

--*/

{

    PSTR AfterScan;
    LONG Day;
    LONG Month;
    INT Status;
    PSTR String;
    LONG Value;
    LONG Week;
    LONG Weekday;

    Month = 0;
    String = *Variable;
    Rule->Number = 1;
    Rule->From = 0;
    Rule->To = 9999;
    if (*String == 'J') {
        String += 1;
        Day = strtoul(String, &AfterScan, 10);
        if ((Day <= 0) || (Day > 365) || (AfterScan == String)) {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        String = AfterScan;
        Day -= 1;
        while ((Month < 12) && (Day > ClDaysPerMonth[Month])) {
            Day -= ClDaysPerMonth[Month];
            Month += 1;
        }

        Rule->On.Type = TimeZoneOccasionMonthDate;
        Rule->On.MonthDay = Day;

    } else if (isdigit(*String)) {
        Day = strtoul(String, &AfterScan, 10);
        if ((Day < 0) || (Day > 365) || (AfterScan == String)) {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        String = AfterScan;
        while ((Month < 12) && (Day > ClDaysPerMonth[Month])) {
            Day -= ClDaysPerMonth[Month];
            Month += 1;
        }

        Rule->On.Type = TimeZoneOccasionMonthDate;
        Rule->On.MonthDay = Day;

    } else if (*String == 'M') {
        String += 1;
        Month = strtoul(String, &AfterScan, 10);
        if ((Month < 1) || (Month > 12) || (AfterScan == String)) {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        Month -= 1;
        String = AfterScan;
        if (*String != '.') {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        String += 1;
        Week = strtoul(String, &AfterScan, 10);
        if ((Week < 1) || (Week > 5) || (AfterScan == String)) {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        if (Week == 1) {
            Rule->On.Type = TimeZoneOccasionGreaterOrEqualWeekday;
            Rule->On.MonthDay = 1;

        } else if (Week == 5) {
            Rule->On.Type = TimeZoneOccasionLastWeekday;

        } else {
            Rule->On.Type = TimeZoneOccasionGreaterOrEqualWeekday;

            //
            // This isn't really perfect, it can be off by a week.
            //

            Rule->On.MonthDay = (Week - 1) * 7;
        }

        String = AfterScan;
        if (*String != '.') {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        String += 1;
        Weekday = strtoul(String, &AfterScan, 10);
        if ((Weekday < 0) || (Weekday > 6) || (AfterScan == String)) {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        String = AfterScan;
        Rule->On.Weekday = Weekday;
    }

    Rule->Month = Month;
    Rule->At = 2 * 3600;
    Rule->AtLens = TimeZoneLensLocalTime;

    //
    // Parse an optional /time.
    //

    if (*String == '/') {
        String += 1;
        Value = strtoul(String, &AfterScan, 10);
        if (String == AfterScan) {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        String = AfterScan;
        if ((Value < -167) || (Value > 167)) {
            Status = EINVAL;
            goto ReadCustomTimeRuleEnd;
        }

        Rule->At = Value * 3600;
        if (*String == ':') {
            String += 1;
            Value = strtoul(String, &AfterScan, 10);
            if (String == AfterScan) {
                Status = EINVAL;
                goto ReadCustomTimeRuleEnd;
            }

            String = AfterScan;
            if ((Value < 0) || (Value > 59)) {
                Status = EINVAL;
                goto ReadCustomTimeRuleEnd;
            }

            Rule->At += Value * 60;
            if (*String == ':') {
                String += 1;
                Value = strtoul(String, &AfterScan, 10);
                if (String == AfterScan) {
                    Status = EINVAL;
                    goto ReadCustomTimeRuleEnd;
                }

                String = AfterScan;
                if ((Value < 0) || (Value > 59)) {
                    Status = EINVAL;
                    goto ReadCustomTimeRuleEnd;
                }

                Rule->At += Value;
            }
        }
    }

    Status = 0;

ReadCustomTimeRuleEnd:
    *Variable = String;
    return Status;
}

VOID
ClpAcquireTimeZoneLock (
    VOID
    )

/*++

Routine Description:

    This routine acquires the global time zone lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OsAcquireLock(&ClTimeZoneLock);
    return;
}

VOID
ClpReleaseTimeZoneLock (
    VOID
    )

/*++

Routine Description:

    This routine acquires the global time zone lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OsReleaseLock(&ClTimeZoneLock);
    return;
}

