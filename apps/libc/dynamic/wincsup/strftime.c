/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    strftime.c

Abstract:

    This module implements support for C90 strftime on MinGW.

Author:

    Evan Green 11-Jan-2016

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#define LIBC_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
RtlpCalculateIsoWeekNumber (
    LONG Year,
    LONG YearDay,
    LONG Weekday,
    PLONG WeekNumber,
    PLONG IsoYear
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

size_t
ClStrftimeC90 (
    char *Buffer,
    size_t BufferSize,
    const char *Format,
    const struct tm *Time
    )

/*++

Routine Description:

    This routine implements a C90 strftime, using the underlying system's C89
    strftime.

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

    time_t EpochTime;
    LONG IsoWeekNumber;
    LONG IsoYear;
    KSTATUS KStatus;
    size_t Length;
    CHAR Specifier;
    CHAR SpecifierString[3];
    PSTR String;
    ULONG StringSize;
    int Weekday;
    CHAR WorkingBuffer[64];

    SpecifierString[0] = '%';
    SpecifierString[2] = '\0';
    String = Buffer;
    StringSize = BufferSize;
    while (StringSize != 0) {

        //
        // If this is the end of the format string, then either it's really the
        // end, or it's just the end of the temporary format string.
        //

        if (*Format == '\0') {
            *String = '\0';
            break;
        }

        //
        // Handle ordinary characters in the format.
        //

        if (*Format != '%') {
            *String = *Format;
            Format += 1;
            String += 1;
            StringSize -= 1;
            continue;
        }

        Format += 1;

        //
        // Pass over an E or an O for alternate representations. At some point
        // these should be supported.
        //

        if (*Format == 'E') {
            Format += 1;
        }

        if (*Format == 'O') {
            Format += 1;
        }

        Specifier = *Format;
        Format += 1;
        WorkingBuffer[0] = '\0';
        switch (Specifier) {
        case 'C':
            snprintf(WorkingBuffer,
                     sizeof(WorkingBuffer),
                     "%02d",
                     (Time->tm_year + 1900) / 100);

            break;

        case 'D':
            strftime(WorkingBuffer, sizeof(WorkingBuffer), "%m/%d/%y", Time);
            break;

        case 'e':
            snprintf(WorkingBuffer,
                     sizeof(WorkingBuffer),
                     "%2d",
                     Time->tm_mday);

            break;

        case 'F':
            strftime(WorkingBuffer, sizeof(WorkingBuffer), "%Y-%m-%d", Time);
            break;

        case 'G':
        case 'g':
        case 'V':
            KStatus = RtlpCalculateIsoWeekNumber(Time->tm_year + 1900,
                                                 Time->tm_yday,
                                                 Time->tm_wday,
                                                 &IsoWeekNumber,
                                                 &IsoYear);

            if (KSUCCESS(KStatus)) {
                switch (Specifier) {
                case 'G':
                    snprintf(WorkingBuffer,
                             sizeof(WorkingBuffer),
                             "%04d",
                             IsoYear);

                    break;

                case 'g':
                    snprintf(WorkingBuffer,
                             sizeof(WorkingBuffer),
                             "%02d",
                             IsoYear % 100);

                    break;

                case 'V':
                default:

                    assert(Specifier == 'V');

                    snprintf(WorkingBuffer,
                             sizeof(WorkingBuffer),
                             "%02d",
                             IsoWeekNumber);

                    break;
                }
            }

            break;

        case 'h':
            strftime(WorkingBuffer, sizeof(WorkingBuffer), "%b", Time);
            break;

        case 'J':
        case 'N':
        case 'q':
            snprintf(WorkingBuffer,
                     sizeof(WorkingBuffer),
                     "%d",
                     0);

            break;

        case 'n':
            strncpy(WorkingBuffer, "\n", sizeof(WorkingBuffer));
            break;

        case 'r':
            strftime(WorkingBuffer, sizeof(WorkingBuffer), "%I:%M:%S %p", Time);
            break;

        case 'R':
            strftime(WorkingBuffer, sizeof(WorkingBuffer), "%H:%M", Time);
            break;

        case 's':
            EpochTime = mktime((struct tm *)Time);
            snprintf(WorkingBuffer,
                     sizeof(WorkingBuffer),
                     "%lld",
                     (long long)EpochTime);

            break;

        case 't':
            strncpy(WorkingBuffer, "\t", sizeof(WorkingBuffer));
            break;

        case 'T':
            strftime(WorkingBuffer, sizeof(WorkingBuffer), "%H:%M:%S", Time);
            break;

        case 'u':
            Weekday = Time->tm_wday;
            if (Weekday == 0) {
                Weekday = 7;
            }

            snprintf(WorkingBuffer,
                     sizeof(WorkingBuffer),
                     "%d",
                     Weekday);

            break;

        //
        // Assume the default strftime has got it.
        //

        default:
            SpecifierString[1] = Specifier;
            strftime(WorkingBuffer,
                     sizeof(WorkingBuffer),
                     SpecifierString,
                     Time);

            break;
        }

        WorkingBuffer[sizeof(WorkingBuffer) - 1] = '\0';
        strncpy(String, WorkingBuffer, StringSize);
        Length = strlen(String);
        String += Length;
        StringSize -= Length;
    }

    //
    // Null terminate the string if it's completely filled up.
    //

    if ((StringSize == 0) && (BufferSize != 0)) {
        Buffer[BufferSize - 1] = '\0';
    }

    //
    // Figure out the number of bytes that were written, and returned.
    //

    StringSize = BufferSize - StringSize;
    return StringSize;
}

//
// --------------------------------------------------------- Internal Functions
//

