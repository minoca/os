/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.h

Abstract:

    This header contains time related definitions.

Author:

    Evan Green 22-Jul-2013

--*/

#ifndef _TIME_H
#define _TIME_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <signal.h>
#include <stddef.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the types of clocks that can be accessed.
//

//
// Wall clock time
//

#define CLOCK_REALTIME              0

//
// High resolution hardware timer
//

#define CLOCK_MONOTONIC             1

//
// CPU time for the process
//

#define CLOCK_PROCESS_CPUTIME_ID    2

//
// CPU time for the thread
//

#define CLOCK_THREAD_CPUTIME_ID     3

//
// Monotonic clock, unscaled
//

#define CLOCK_MONOTONIC_RAW         4

//
// Recent realtime clock value, updated regularly.
//

#define CLOCK_REALTIME_COARSE       5

//
// Recent monotonic clock value, updated regularly.
//

#define CLOCK_MONOTONIC_COARSE      6

//
// Monotonic clock value plus time spent in suspension.
//

#define CLOCK_BOOTTIME              7

//
// Define the flags that can be passed to the set timer function.
//

//
// Set this flag to indicate that the value to be set is an absolute time.
//

#define TIMER_ABSTIME 0x00000001

//
// Define the value to convert the units returned in the clock function to
// seconds.
//

#define CLOCKS_PER_SEC 1000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about an interval timer.

Members:

    it_interval - Stores the period of the timer for periodic timers, or 0.0 if
        the timer is a one-shot timer.

    it_value - Stores the due time of the timer.

--*/

struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

/*++

Structure Description:

    This structure describes the C library concept of calendar time.

Members:

    tm_sec - Stores the second. Valid values are between 0 and 60 (for leap
        seconds).

    tm_min - Stores the minute. Valid values are between 0 and 59.

    tm_hour - Stores the hour. Valid values are between 0 and 23.

    tm_mday - Stores the day of the month. Valid values are between 1 and 31.

    tm_mon - Stores the month. Valid values are between 0 and 11.

    tm_year - Stores the number of years since 1900. Valid values are between
        -1899 and 8099 (for actual calendar years between 1 and 9999).

    tm_wday - Stores the day of the week. Valid values are between 0 and 6,
        with 0 being Sunday and 6 being Saturday.

    tm_yday - Stores the day of the year. Valid values are between 0 and 365.

    tm_isdst - Stores a value indicating if the given time is represented in
        daylight saving time. Usually 0 indicates standard time, 1 indicates
        daylight saving time, and -1 indicates "unknown".

    tm_nanosecond - Stores the nanosecond. Valid values are between 0 and
        999,999,999.

    tm_gmtoff - Stores the offset from Greenwich Mean Time in seconds that
        this time is interpreted in.

    tm_zone - Stores a pointer to a constant string containing the time zone
        name. The user should not modify or free this buffer.

--*/

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    int tm_nanosecond;
    int tm_gmtoff;
    const char *tm_zone;
};

//
// -------------------------------------------------------------------- Globals
//

//
// This variable is set to zero if Daylight Saving time should never be applied
// for the timezone in use, or non-zero otherwise.
//

LIBC_API extern int daylight;

//
// This variable is set to the difference in seconds between Universal
// Coordinated Time (UTC) and local standard time.
//

LIBC_API extern long timezone;

//
// This variable is set to contain two pointers to strings. The first one
// points to the name of the timezone in standard time, and the second one
// pointers to the name of the timezone in Daylight Saving time.
//

LIBC_API extern char *tzname[2];

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
clock_t
clock (
    void
    );

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

LIBC_API
int
clock_getcpuclockid (
    pid_t ProcessId,
    clockid_t *ClockId
    );

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

    -1 on failure, and errno will be set to contain more information.

--*/

LIBC_API
int
clock_getres (
    clockid_t ClockId,
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

LIBC_API
int
clock_gettime (
    clockid_t ClockId,
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

LIBC_API
int
clock_settime (
    clockid_t ClockId,
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

LIBC_API
int
clock_nanosleep (
    clockid_t ClockId,
    int Flags,
    const struct timespec *RequestedTime,
    struct timespec *RemainingTime
    );

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

LIBC_API
char *
asctime (
    const struct tm *Time
    );

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

LIBC_API
char *
asctime_r (
    const struct tm *Time,
    char *Buffer
    );

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

LIBC_API
char *
ctime (
    const time_t *TimeValue
    );

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

LIBC_API
char *
ctime_r (
    const time_t *TimeValue,
    char *Buffer
    );

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

LIBC_API
double
difftime (
    time_t LeftTimeValue,
    time_t RightTimeValue
    );

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

LIBC_API
struct tm *
gmtime (
    const time_t *TimeValue
    );

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

LIBC_API
struct tm *
gmtime_r (
    const time_t *TimeValue,
    struct tm *Result
    );

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

LIBC_API
struct tm *
localtime (
    const time_t *TimeValue
    );

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

LIBC_API
struct tm *
localtime_r (
    const time_t *TimeValue,
    struct tm *Result
    );

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

LIBC_API
time_t
timegm (
    struct tm *Time
    );

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

LIBC_API
time_t
mktime (
    struct tm *Time
    );

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

LIBC_API
size_t
strftime (
    char *Buffer,
    size_t BufferSize,
    const char *Format,
    const struct tm *Time
    );

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

LIBC_API
char *
strptime (
    const char *Buffer,
    const char *Format,
    struct tm *Time
    );

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

LIBC_API
time_t
time (
    time_t *Result
    );

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

LIBC_API
int
timer_create (
    clockid_t ClockId,
    struct sigevent *Event,
    timer_t *TimerId
    );

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

LIBC_API
int
timer_delete (
    timer_t TimerId
    );

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

LIBC_API
int
timer_gettime (
    timer_t TimerId,
    struct itimerspec *Value
    );

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

LIBC_API
int
timer_settime (
    timer_t TimerId,
    int Flags,
    const struct itimerspec *Value,
    struct itimerspec *OldValue
    );

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

LIBC_API
int
timer_getoverrun (
    timer_t TimerId
    );

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

LIBC_API
void
tzset (
    void
    );

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

LIBC_API
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

#ifdef __cplusplus

}

#endif
#endif

