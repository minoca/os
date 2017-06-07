/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.c

Abstract:

    This module implements calendar time support functions.

Author:

    Evan Green 6-Aug-2013

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "rtlp.h"
#include "time.h"
#include <minoca/lib/tzfmt.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// This value defines the two digit year beginning in which the 21st century is
// assumed.
//

#define TWO_DIGIT_YEAR_CUTOFF 70
#define TWENTIETH_CENTURY 1900
#define TWENTY_FIRST_CENTURY 2000

//
// Define the period of the entire Gregorian cycle.
//

#define GREGORIAN_CYCLE_YEARS 400
#define GREGORIAN_CYCLE_DAYS ((365 * 400) + 100 - 4 + 1)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
RtlpScanTimeStrings (
    PCSTR Input,
    PSTR *Strings,
    ULONG StringCount,
    PLONG Index,
    PLONG Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

RTL_API
KSTATUS
RtlSystemTimeToGmtCalendarTime (
    PSYSTEM_TIME SystemTime,
    PCALENDAR_TIME CalendarTime
    )

/*++

Routine Description:

    This routine converts the given system time into calendar time in the
    GMT time zone.

Arguments:

    SystemTime - Supplies a pointer to the system time to convert.

    CalendarTime - Supplies a pointer to the calendar time to initialize based
        on the given system time.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given system time is too funky.

--*/

{

    LONG Day;
    LONG Hour;
    INT Leap;
    LONG Minute;
    LONG Month;
    LONG RawDays;
    LONGLONG RawTime;
    LONG Second;
    LONG Weekday;
    LONG Year;

    RtlZeroMemory(CalendarTime, sizeof(CALENDAR_TIME));
    RawTime = SystemTime->Seconds;
    if ((RawTime > MAX_TIME_ZONE_DATE) ||
        (RawTime < MIN_TIME_ZONE_DATE)) {

        return STATUS_OUT_OF_BOUNDS;
    }

    RawDays = RawTime / SECONDS_PER_DAY;
    RawTime -= (LONGLONG)RawDays * SECONDS_PER_DAY;
    if (RawTime < 0) {
        RawTime += SECONDS_PER_DAY;
        RawDays -= 1;
    }

    Weekday = (TIME_ZONE_EPOCH_WEEKDAY + RawDays) % DAYS_PER_WEEK;
    if (Weekday < 0) {
        Weekday = DAYS_PER_WEEK + Weekday;
    }

    Year = RtlpComputeYearForDays(&RawDays);
    Leap = 0;
    if (IS_LEAP_YEAR(Year)) {
        Leap = 1;
    }

    //
    // Subtract off the months.
    //

    Month = 0;
    Day = RawDays;
    while (Day >= RtlDaysPerMonth[Leap][Month]) {
        Day -= RtlDaysPerMonth[Leap][Month];
        Month += 1;
    }

    //
    // Days of the month start with 1.
    //

    Day += 1;

    //
    // Figure out the time of day.
    //

    Second = (LONG)RawTime;
    Hour = Second / SECONDS_PER_HOUR;
    Second -= Hour * SECONDS_PER_HOUR;
    Minute = Second / SECONDS_PER_MINUTE;
    Second -= Minute * SECONDS_PER_MINUTE;

    //
    // Fill in the structure.
    //

    CalendarTime->Year = Year;
    CalendarTime->Month = Month;
    CalendarTime->Day = Day;
    CalendarTime->Hour = Hour;
    CalendarTime->Minute = Minute;
    CalendarTime->Second = Second;
    CalendarTime->Nanosecond = SystemTime->Nanoseconds;
    CalendarTime->Weekday = Weekday;
    CalendarTime->YearDay = RawDays;
    CalendarTime->IsDaylightSaving = FALSE;
    return STATUS_SUCCESS;
}

RTL_API
KSTATUS
RtlCalendarTimeToSystemTime (
    PCALENDAR_TIME CalendarTime,
    PSYSTEM_TIME SystemTime
    )

/*++

Routine Description:

    This routine converts the given calendar time into its corresponding system
    time.

Arguments:

    CalendarTime - Supplies a pointer to the calendar time to convert.

    SystemTime - Supplies a pointer where the system time will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given calendar time is too funky.

--*/

{

    LONGLONG Days;

    RtlpNormalizeCalendarTime(CalendarTime);
    if ((CalendarTime->Year > MAX_TIME_ZONE_YEAR) ||
        (CalendarTime->Year < MIN_TIME_ZONE_YEAR)) {

        return STATUS_OUT_OF_BOUNDS;
    }

    Days = RtlpComputeDaysForYear(CalendarTime->Year);

    //
    // The normalize function above ensures that the year day is correct.
    //

    Days += CalendarTime->YearDay;
    SystemTime->Nanoseconds = CalendarTime->Nanosecond;
    SystemTime->Seconds = ((LONGLONG)Days * SECONDS_PER_DAY) +
                          (CalendarTime->Hour * SECONDS_PER_HOUR) +
                          (CalendarTime->Minute * SECONDS_PER_MINUTE) +
                          CalendarTime->Second -
                          CalendarTime->GmtOffset;

    return STATUS_SUCCESS;
}

RTL_API
KSTATUS
RtlGmtCalendarTimeToSystemTime (
    PCALENDAR_TIME CalendarTime,
    PSYSTEM_TIME SystemTime
    )

/*++

Routine Description:

    This routine converts the given calendar time, assumed to be a GMT date and
    time, into its corresponding system time. On success, this routine will
    update the supplied calendar time to fill out all fields.

Arguments:

    CalendarTime - Supplies a pointer to the GMT calendar time to convert.

    SystemTime - Supplies a pointer where the system time will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS if the given GMT calendar time is too funky.

--*/

{

    CALENDAR_TIME GmtCalendarTime;
    KSTATUS Status;

    //
    // The supplied time is meant to be interpreted in the GMT time zone. Smash
    // the GMT offset and any time zone information.
    //

    CalendarTime->GmtOffset = 0;

    //
    // Convert the given GMT calendar time into a system time. This normalizes
    // the calendar time as well.
    //

    Status = RtlCalendarTimeToSystemTime(CalendarTime, SystemTime);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Convert the system time back to a GMT calendar time to get all the
    // fields filled out.
    //

    Status = RtlSystemTimeToGmtCalendarTime(SystemTime, &GmtCalendarTime);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // With success on the horizon, copy the fully qualified calendar time to
    // the supplied pointer.
    //

    RtlCopyMemory(CalendarTime, &GmtCalendarTime, sizeof(CALENDAR_TIME));
    return STATUS_SUCCESS;
}

RTL_API
UINTN
RtlFormatDate (
    PSTR StringBuffer,
    ULONG StringBufferSize,
    PSTR Format,
    PCALENDAR_TIME CalendarTime
    )

/*++

Routine Description:

    This routine converts the given calendar time into a string governed by
    the given format string.

Arguments:

    StringBuffer - Supplies a pointer where the converted string will be
        returned, including the terminating null.

    StringBufferSize - Supplies the size of the string buffer in bytes.

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
        %N - Replaced by the nanosecond [000000000,999999999]
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

    CalendarTime - Supplies a pointer to the calendar time value to use in the
        substitution.

Return Value:

    Returns the number of characters written to the output buffer, not
    including the null terminator.

--*/

{

    PSTR CopyString;
    ULONG CopyStringSize;
    ULONG DefaultFieldSize;
    BOOL Evening;
    LONG Hour12;
    LONGLONG Integer;
    LONG IsoYear;
    PSTR SavedFormat;
    CHAR Sign;
    CHAR Specifier;
    KSTATUS Status;
    PSTR String;
    ULONG StringSize;
    SYSTEM_TIME SystemTime;
    LONG WeekNumber;
    CHAR WorkingBuffer[13];
    BOOL ZeroPad;
    LONG ZoneOffset;
    LONG ZoneOffsetHours;
    LONG ZoneOffsetMinutes;

    Hour12 = CalendarTime->Hour;
    if (Hour12 == 0) {
        Hour12 = 12;

    } else if (Hour12 > 12) {
        Hour12 -= 12;
    }

    Evening = FALSE;
    if (CalendarTime->Hour >= 12) {
        Evening = TRUE;
    }

    SavedFormat = NULL;
    String = StringBuffer;
    StringSize = StringBufferSize;
    while (StringSize != 0) {

        //
        // If this is the end of the format string, then either it's really the
        // end, or it's just the end of the temporary format string.
        //

        if (*Format == '\0') {
            if (SavedFormat != NULL) {
                Format = SavedFormat;
                SavedFormat = NULL;
                continue;

            } else {
                *String = '\0';
                break;
            }
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
        CopyString = NULL;
        CopyStringSize = -1;
        Integer = -1;
        ZeroPad = FALSE;
        DefaultFieldSize = 2;
        switch (Specifier) {
        case 'a':
            if ((CalendarTime->Weekday >= TimeZoneWeekdaySunday) &&
                (CalendarTime->Weekday <= TimeZoneWeekdaySaturday)) {

                CopyString =
                           RtlAbbreviatedWeekdayStrings[CalendarTime->Weekday];

            } else {
                return 0;
            }

            break;

        case 'A':
            if ((CalendarTime->Weekday >= TimeZoneWeekdaySunday) &&
                (CalendarTime->Weekday <= TimeZoneWeekdaySaturday)) {

                CopyString = RtlWeekdayStrings[CalendarTime->Weekday];

            } else {
                return 0;
            }

            break;

        case 'b':
        case 'h':
            if ((CalendarTime->Month >= TimeZoneMonthJanuary) &&
                (CalendarTime->Month <= TimeZoneMonthDecember)) {

                CopyString = RtlAbbreviatedMonthStrings[CalendarTime->Month];

            } else {
                return 0;
            }

            break;

        case 'B':
            if ((CalendarTime->Month >= TimeZoneMonthJanuary) &&
                (CalendarTime->Month <= TimeZoneMonthDecember)) {

                CopyString = RtlMonthStrings[CalendarTime->Month];

            } else {
                return 0;
            }

            break;

        case 'c':
            SavedFormat = Format;
            Format = "%a %b %e %H:%M:%S %Y";
            continue;

        case 'C':
            Integer = CalendarTime->Year / YEARS_PER_CENTURY;
            ZeroPad = TRUE;
            break;

        case 'd':
            Integer = CalendarTime->Day;
            ZeroPad = TRUE;
            break;

        case 'D':
            SavedFormat = Format;
            Format = "%m/%d/%y";
            continue;

        case 'e':
            Integer = CalendarTime->Day;
            break;

        case 'F':
            SavedFormat = Format;
            Format = "%Y-%m-%d";
            continue;

        case 'g':
        case 'G':
            Status = RtlpCalculateIsoWeekNumber(CalendarTime->Year,
                                                CalendarTime->YearDay,
                                                CalendarTime->Weekday,
                                                NULL,
                                                &IsoYear);

            if (!KSUCCESS(Status)) {
                CopyStringSize = 0;

            } else {
                if (Specifier == 'g') {
                    Integer = IsoYear % YEARS_PER_CENTURY;
                    DefaultFieldSize = 2;

                } else {
                    Integer = IsoYear;
                    DefaultFieldSize = 4;
                }

                ZeroPad = TRUE;
            }

            break;

        case 'H':
            Integer = CalendarTime->Hour;
            ZeroPad = TRUE;
            break;

        case 'I':
            Integer = Hour12;
            ZeroPad = TRUE;
            break;

        case 'J':
            Integer = CalendarTime->Nanosecond;
            ZeroPad = TRUE;
            DefaultFieldSize = 9;
            break;

        case 'j':
            Integer = CalendarTime->YearDay + 1;
            ZeroPad = TRUE;
            DefaultFieldSize = 3;
            break;

        case 'm':
            Integer = CalendarTime->Month + 1;
            ZeroPad = TRUE;
            break;

        case 'M':
            Integer = CalendarTime->Minute;
            ZeroPad = TRUE;
            break;

        case 'N':
            Integer = CalendarTime->Nanosecond;
            ZeroPad = TRUE;
            DefaultFieldSize = 9;
            break;

        case 'n':
            WorkingBuffer[0] = '\n';
            CopyString = WorkingBuffer;
            CopyStringSize = 1;
            break;

        case 'p':
            CopyString = RtlAmPmStrings[0][Evening];
            break;

        case 'P':
            CopyString = RtlAmPmStrings[1][Evening];
            break;

        case 'q':
            Integer = CalendarTime->Nanosecond / 1000000;
            ZeroPad = TRUE;
            DefaultFieldSize = 3;
            break;

        case 'r':
            SavedFormat = Format;
            Format = "%I:%M:%S %p";
            continue;

        case 'R':
            SavedFormat = Format;
            Format = "%H:%M";
            continue;

        case 's':
            Status = RtlCalendarTimeToSystemTime(CalendarTime, &SystemTime);
            if (!KSUCCESS(Status)) {
                Integer = 0;

            } else {
                Integer = SystemTime.Seconds + SYSTEM_TIME_TO_EPOCH_DELTA;
            }

            break;

        case 'S':
            Integer = CalendarTime->Second;
            ZeroPad = TRUE;
            break;

        case 't':
            WorkingBuffer[0] = '\t';
            CopyString = WorkingBuffer;
            CopyStringSize = 1;
            break;

        case 'T':
            SavedFormat = Format;
            Format = "%H:%M:%S";
            continue;

        case 'u':
            Integer = CalendarTime->Weekday;
            if (Integer == TimeZoneWeekdaySunday) {
                Integer = DAYS_PER_WEEK;
            }

            DefaultFieldSize = 1;
            break;

        case 'U':
            Status = RtlpCalculateWeekNumber(CalendarTime->Year,
                                             CalendarTime->YearDay,
                                             TimeZoneWeekdaySunday,
                                             &WeekNumber);

            if (!KSUCCESS(Status)) {
                CopyStringSize = 0;

            } else {
                Integer = WeekNumber;
                ZeroPad = TRUE;
            }

            break;

        case 'V':
            Status = RtlpCalculateIsoWeekNumber(CalendarTime->Year,
                                                CalendarTime->YearDay,
                                                CalendarTime->Weekday,
                                                &WeekNumber,
                                                NULL);

            if (!KSUCCESS(Status)) {
                CopyStringSize = 0;

            } else {
                Integer = WeekNumber;
                ZeroPad = TRUE;
            }

            break;

        case 'w':
            Integer = CalendarTime->Weekday;
            DefaultFieldSize = 1;
            break;

        case 'W':
            Status = RtlpCalculateWeekNumber(CalendarTime->Year,
                                             CalendarTime->YearDay,
                                             TimeZoneWeekdayMonday,
                                             &WeekNumber);

            if (!KSUCCESS(Status)) {
                CopyStringSize = 0;

            } else {
                Integer = WeekNumber;
                ZeroPad = TRUE;
            }

            break;

        case 'x':
            SavedFormat = Format;
            Format = "%m/%d/%y";
            continue;

        case 'X':
            SavedFormat = Format;
            Format = "%H:%M:%S";
            continue;

        case 'y':
            Integer = CalendarTime->Year % YEARS_PER_CENTURY;
            ZeroPad = TRUE;
            break;

        case 'Y':
            Integer = CalendarTime->Year;
            ZeroPad = TRUE;
            DefaultFieldSize = 4;
            break;

        case 'z':
            ZoneOffset = CalendarTime->GmtOffset;
            Sign = '+';
            if (ZoneOffset < 0) {
                Sign = '-';
                ZoneOffset = -ZoneOffset;
            }

            ZoneOffsetHours = ZoneOffset / SECONDS_PER_HOUR;
            ZoneOffset %= SECONDS_PER_HOUR;
            ZoneOffsetMinutes = ZoneOffset / SECONDS_PER_MINUTE;
            CopyStringSize = RtlPrintToString(WorkingBuffer,
                                              sizeof(WorkingBuffer),
                                              CharacterEncodingDefault,
                                              "%c%02d%02d",
                                              Sign,
                                              ZoneOffsetHours,
                                              ZoneOffsetMinutes);

            if (CopyStringSize > sizeof(WorkingBuffer)) {
                CopyStringSize = sizeof(WorkingBuffer);
            }

            if (CopyStringSize != 0) {
                CopyStringSize -= 1;
            }

            CopyString = WorkingBuffer;
            break;

        case 'Z':
            CopyStringSize = sizeof(WorkingBuffer);
            CopyString = WorkingBuffer;
            if (CalendarTime->TimeZone != NULL) {
                CopyStringSize = RtlStringCopy(CopyString,
                                               CalendarTime->TimeZone,
                                               CopyStringSize);

                CopyStringSize -= 1;

            } else {
                WorkingBuffer[0] = '\0';
                CopyStringSize = 0;
            }

            break;

        case '%':
            WorkingBuffer[0] = '%';
            CopyString = WorkingBuffer;
            CopyStringSize = 1;
            break;

        default:
            CopyStringSize = 0;
            break;
        }

        //
        // If there's an integer, use that.
        //

        if (Integer != -1) {
            if (ZeroPad != FALSE) {
                CopyStringSize = RtlPrintToString(WorkingBuffer,
                                                  sizeof(WorkingBuffer),
                                                  CharacterEncodingDefault,
                                                  "%0*I64d",
                                                  DefaultFieldSize,
                                                  Integer);

            } else {
                CopyStringSize = RtlPrintToString(WorkingBuffer,
                                                  sizeof(WorkingBuffer),
                                                  CharacterEncodingDefault,
                                                  "%*I64d",
                                                  DefaultFieldSize,
                                                  Integer);
            }

            if (CopyStringSize > sizeof(WorkingBuffer)) {
                CopyStringSize = sizeof(WorkingBuffer);
            }

            if (CopyStringSize != 0) {
                CopyStringSize -= 1;
            }

            CopyString = WorkingBuffer;

        //
        // If the copy string size was left untouched, take its length.
        //

        } else if (CopyStringSize == -1) {

            ASSERT(CopyString != NULL);

            CopyStringSize = RtlStringLength(CopyString);
        }

        //
        // Copy characters over to the destination buffer.
        //

        while ((StringSize != 0) && (CopyStringSize != 0)) {
            *String = *CopyString;
            String += 1;
            StringSize -= 1;
            CopyString += 1;
            CopyStringSize -= 1;
        }
    }

    //
    // Null terminate the string if it's completely filled up.
    //

    if ((StringSize == 0) && (StringBufferSize != 0)) {
        StringBuffer[StringBufferSize - 1] = '\0';
    }

    //
    // Figure out the number of bytes that were written, and returned.
    //

    StringSize = StringBufferSize - StringSize;
    return StringSize;
}

RTL_API
PSTR
RtlScanDate (
    PCSTR StringBuffer,
    PCSTR Format,
    PCALENDAR_TIME CalendarTime
    )

/*++

Routine Description:

    This routine scans the given input string into values in the calendar time,
    using the specified format.

Arguments:

    StringBuffer - Supplies a pointer to the null terminated string to scan.

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

    CalendarTime - Supplies a pointer to the calendar time value to place the
        values in. Only the values that are scanned in are modified.

Return Value:

    Returns the a pointer to the input string after the last character scanned.

    NULL if the result coult not be scanned.

--*/

{

    BOOL Evening;
    LONG Integer;
    LONGLONG LongLong;
    PCSTR SavedFormat;
    BOOL ScanInteger;
    LONG Size;
    CHAR Specifier;
    KSTATUS Status;
    PCSTR String;
    ULONG StringSize;

    Evening = FALSE;
    SavedFormat = NULL;
    String = StringBuffer;
    while (*String != '\0') {

        //
        // If this is the end of the format string, then either it's really the
        // end, or it's just the end of the temporary format string.
        //

        if (*Format == '\0') {
            if (SavedFormat != NULL) {
                Format = SavedFormat;
                SavedFormat = NULL;
                continue;

            } else {
                break;
            }
        }

        //
        // Handle whitespace in the format.
        //

        if (RtlIsCharacterSpace(*Format) != FALSE) {
            while (RtlIsCharacterSpace(*String) != FALSE) {
                String += 1;
            }

            Format += 1;
            continue;

        //
        // Handle ordinary characters in the format.
        //

        } else if (*Format != '%') {
            if (*String != *Format) {
                Status = STATUS_NO_MATCH;
                goto ScanTimeEnd;
            }

            Format += 1;
            String += 1;
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
        ScanInteger = FALSE;
        switch (Specifier) {
        case 'a':
        case 'A':
            Status = RtlpScanTimeStrings(String,
                                         RtlWeekdayStrings,
                                         DAYS_PER_WEEK,
                                         &Integer,
                                         &Size);

            if (!KSUCCESS(Status)) {
                Status = RtlpScanTimeStrings(String,
                                             RtlAbbreviatedWeekdayStrings,
                                             DAYS_PER_WEEK,
                                             &Integer,
                                             &Size);
            }

            if (!KSUCCESS(Status)) {
                goto ScanTimeEnd;
            }

            CalendarTime->Weekday = Integer;
            String += Size;
            break;

        case 'b':
        case 'B':
        case 'h':
            Status = RtlpScanTimeStrings(String,
                                         RtlMonthStrings,
                                         MONTHS_PER_YEAR,
                                         &Integer,
                                         &Size);

            if (!KSUCCESS(Status)) {
                Status = RtlpScanTimeStrings(String,
                                             RtlAbbreviatedMonthStrings,
                                             MONTHS_PER_YEAR,
                                             &Integer,
                                             &Size);
            }

            if (!KSUCCESS(Status)) {
                goto ScanTimeEnd;
            }

            CalendarTime->Month = Integer;
            String += Size;
            break;

        case 'c':
            SavedFormat = Format;
            Format = "%a %b %e %H:%M:%S %Y";
            continue;

        case 'D':
            SavedFormat = Format;
            Format = "%m/%d/%y";
            continue;

        case 'n':
        case 't':
            while (RtlIsCharacterSpace(*String) != FALSE) {
                String += 1;
            }

            continue;

        case 'p':
            Status = RtlpScanTimeStrings(String,
                                         RtlAmPmStrings[0],
                                         2,
                                         &Integer,
                                         &Size);

            if (!KSUCCESS(Status)) {
                Status = RtlpScanTimeStrings(String,
                                             RtlAmPmStrings[1],
                                             2,
                                             &Integer,
                                             &Size);
            }

            if (!KSUCCESS(Status)) {
                goto ScanTimeEnd;
            }

            if (Integer == 1) {
                Evening = TRUE;
                if ((CalendarTime->Hour >= 0) && (CalendarTime->Hour <= 12)) {
                    CalendarTime->Hour += 12;
                }

            } else {
                if (CalendarTime->Hour == 12) {
                    CalendarTime->Hour = 0;
                }
            }

            String += Size;
            break;

        case 'r':
            SavedFormat = Format;
            Format = "%I:%M:%S %p";
            continue;

        case 'R':
            SavedFormat = Format;
            Format = "%H:%M";
            continue;

        case 'T':
            SavedFormat = Format;
            Format = "%H:%M:%S";
            continue;

        case 'x':
            SavedFormat = Format;
            Format = "%m/%d/%y";
            continue;

        case 'X':
            SavedFormat = Format;
            Format = "%H:%M:%S";
            continue;

        case 'C':
        case 'd':
        case 'e':
        case 'H':
        case 'I':
        case 'J':
        case 'j':
        case 'm':
        case 'M':
        case 'N':
        case 'q':
        case 'S':
        case 'U':
        case 'W':
        case 'w':
        case 'y':
        case 'Y':
            ScanInteger = TRUE;
            break;

        case '%':
            if (*String != '%') {
                Status = STATUS_NO_MATCH;
                goto ScanTimeEnd;
            }

            String += 1;
            break;

        default:
            break;
        }

        //
        // Scan an integer if desired.
        //

        if (ScanInteger != FALSE) {
            StringSize = MAX_ULONG;
            Status = RtlStringScanInteger(&String,
                                          &StringSize,
                                          10,
                                          TRUE,
                                          &LongLong);

            if (!KSUCCESS(Status)) {
                goto ScanTimeEnd;
            }

            Integer = (LONG)LongLong;

            //
            // Process now that the integer has been scanned.
            //

            switch (Specifier) {
            case 'C':
                CalendarTime->Year = (CalendarTime->Year % YEARS_PER_CENTURY) +
                                     Integer * YEARS_PER_CENTURY;

                break;

            case 'd':
            case 'e':
                if ((Integer <= 0) || (Integer > 31)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Day = Integer;
                break;

            case 'H':
                if ((Integer < 0) || (Integer >= HOURS_PER_DAY)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Hour = Integer;
                break;

            case 'I':
                if ((Integer <= 0) || (Integer > 12)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                if (Evening != FALSE) {
                    Integer += 12;

                } else {
                    if (Integer == 12) {
                        Integer = 0;
                    }
                }

                CalendarTime->Hour = Integer;
                break;

            case 'J':
                if ((Integer < 0) || (Integer >= NANOSECONDS_PER_SECOND)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Nanosecond = Integer;
                break;

            case 'j':
                if ((Integer <= 0) || (Integer > DAYS_PER_LEAP_YEAR)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->YearDay = Integer - 1;
                break;

            case 'm':
                if ((Integer <= 0) || (Integer > MONTHS_PER_YEAR)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Month = Integer - 1;
                break;

            case 'M':
                if ((Integer < 0) || (Integer >= SECONDS_PER_MINUTE)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Minute = Integer;
                break;

            case 'N':
                if ((Integer < 0) || (Integer >= MICROSECONDS_PER_SECOND)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Nanosecond = Integer * 1000;
                break;

            case 'q':
                if ((Integer < 0) || (Integer >= MILLISECONDS_PER_SECOND)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Nanosecond = Integer * 1000000;
                break;

            //
            // Seconds allows a value of 60 for the occasional leap second.
            //

            case 'S':
                if ((Integer < 0) || (Integer > SECONDS_PER_MINUTE)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Second = Integer;
                break;

            case 'U':
            case 'W':
                break;

            case 'w':
                if ((Integer < TimeZoneWeekdaySunday) ||
                    (Integer > TimeZoneWeekdaySaturday)) {

                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Weekday = Integer;
                break;

            case 'y':
                if ((Integer < 0) || (Integer >= YEARS_PER_CENTURY)) {
                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                if (Integer > TWO_DIGIT_YEAR_CUTOFF) {
                    CalendarTime->Year = TWENTY_FIRST_CENTURY + Integer;

                } else {
                    CalendarTime->Year = TWENTIETH_CENTURY + Integer;
                }

                break;

            case 'Y':
                if ((Integer < MIN_TIME_ZONE_YEAR) ||
                    (Integer > MAX_TIME_ZONE_YEAR)) {

                    Status = STATUS_OUT_OF_BOUNDS;
                    goto ScanTimeEnd;
                }

                CalendarTime->Year = Integer;
                break;

            default:
                break;
            }
        }
    }

    Status = STATUS_SUCCESS;

ScanTimeEnd:
    if (!KSUCCESS(Status)) {
        return NULL;
    }

    return (PSTR)String;
}

LONG
RtlpComputeYearForDays (
    PLONG Days
    )

/*++

Routine Description:

    This routine calculates the year given a number of days from the epoch.

Arguments:

    Days - Supplies a pointer to the number of days since the epoch. On
        completion, this will contain the number of remaining days after the
        years have been subtracted.

Return Value:

    Returns the year that the day resides in.

--*/

{

    LONG Cycles;
    LONG RemainingDays;
    LONG Year;

    Year = TIME_ZONE_EPOCH_YEAR;
    RemainingDays = *Days;

    //
    // Divide by the period for truly ridiculous dates.
    //

    if ((RemainingDays >= GREGORIAN_CYCLE_DAYS) ||
        (-RemainingDays >= GREGORIAN_CYCLE_DAYS)) {

        Cycles = RemainingDays / GREGORIAN_CYCLE_DAYS;
        Year += Cycles * GREGORIAN_CYCLE_YEARS;
        RemainingDays -= (Cycles * GREGORIAN_CYCLE_DAYS);
    }

    //
    // Subtract off any years after the epoch.
    //

    while (RemainingDays > 0) {
        if (IS_LEAP_YEAR(Year)) {
            RemainingDays -= DAYS_PER_LEAP_YEAR;

        } else {
            RemainingDays -= DAYS_PER_YEAR;
        }

        Year += 1;
    }

    //
    // The subtraction may have gone one too far, or the days may have
    // started negative. Either way, get the days up to a non-negative value.
    //

    while (RemainingDays < 0) {
        Year -= 1;
        if (IS_LEAP_YEAR(Year)) {
            RemainingDays += DAYS_PER_LEAP_YEAR;

        } else {
            RemainingDays += DAYS_PER_YEAR;
        }
    }

    *Days = RemainingDays;
    return Year;
}

LONG
RtlpComputeDaysForYear (
    LONG Year
    )

/*++

Routine Description:

    This routine calculates the number of days for the given year, relative to
    the epoch.

Arguments:

    Year - Supplies the target year.

Return Value:

    Returns the number of days since the epoch that January 1st of the given
    year occurred.

--*/

{

    LONG Cycles;
    LONG Days;

    Days = 0;
    if (((Year - TIME_ZONE_EPOCH_YEAR) >= GREGORIAN_CYCLE_YEARS) ||
        (-(Year - TIME_ZONE_EPOCH_YEAR) >= GREGORIAN_CYCLE_YEARS)) {

        Cycles = (Year - TIME_ZONE_EPOCH_YEAR) / GREGORIAN_CYCLE_YEARS;
        Year -= Cycles * GREGORIAN_CYCLE_YEARS;
        Days += Cycles * GREGORIAN_CYCLE_DAYS;
    }

    if (Year >= TIME_ZONE_EPOCH_YEAR) {
        while (Year > TIME_ZONE_EPOCH_YEAR) {
            Year -= 1;
            if (IS_LEAP_YEAR(Year)) {
                Days += DAYS_PER_LEAP_YEAR;

            } else {
                Days += DAYS_PER_YEAR;
            }
        }

    } else {
        while (Year < TIME_ZONE_EPOCH_YEAR) {
            if (IS_LEAP_YEAR(Year)) {
                Days -= DAYS_PER_LEAP_YEAR;

            } else {
                Days -= DAYS_PER_YEAR;
            }

            Year += 1;
        }
    }

    return Days;
}

KSTATUS
RtlpCalculateWeekdayForMonth (
    LONG Year,
    LONG Month,
    PLONG Weekday
    )

/*++

Routine Description:

    This routine calculates the weekday for the first of the month on the
    given month and year.

Arguments:

    Year - Supplies the year to calculate the weekday for.

    Month - Supplies the month to calculate the weekday for.

    Weekday - Supplies a pointer where the weekday will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS on failure.

--*/

{

    LONG Days;
    LONG Leap;
    LONG Modulo;

    if ((Year > MAX_TIME_ZONE_YEAR) || (Year < MIN_TIME_ZONE_YEAR)) {
        return STATUS_OUT_OF_BOUNDS;
    }

    Days = RtlpComputeDaysForYear(Year);
    Leap = 0;
    if (IS_LEAP_YEAR(Year)) {
        Leap = 1;
    }

    Days += RtlMonthDays[Leap][Month];
    Modulo = ((TIME_ZONE_EPOCH_WEEKDAY + Days) % DAYS_PER_WEEK);
    if (Modulo < 0) {
        Modulo = DAYS_PER_WEEK + Modulo;
    }

    *Weekday = Modulo;
    return STATUS_SUCCESS;
}

VOID
RtlpNormalizeCalendarTime (
    PCALENDAR_TIME CalendarTime
    )

/*++

Routine Description:

    This routine normalizes the fields in a calendar time structure, putting
    them in their proper ranges.

Arguments:

    CalendarTime - Supplies a pointer to the calendar time structure to
        normalize.

Return Value:

    None.

--*/

{

    LONG Day;
    LONG Leap;

    //
    // Get nanoseconds, seconds, minutes, and hours into range.
    //

    if ((CalendarTime->Nanosecond >= NANOSECONDS_PER_SECOND) ||
        (CalendarTime->Nanosecond < 0)) {

        CalendarTime->Second += CalendarTime->Nanosecond /
                                NANOSECONDS_PER_SECOND;

        CalendarTime->Nanosecond %= NANOSECONDS_PER_SECOND;
        if (CalendarTime->Nanosecond < 0) {
            CalendarTime->Nanosecond += NANOSECONDS_PER_SECOND;
            CalendarTime->Second -= 1;
        }
    }

    if ((CalendarTime->Second >= SECONDS_PER_MINUTE) ||
        (CalendarTime->Second < 0)) {

        CalendarTime->Minute += CalendarTime->Second / SECONDS_PER_MINUTE;
        CalendarTime->Second %= SECONDS_PER_MINUTE;
        if (CalendarTime->Second < 0) {
            CalendarTime->Second += SECONDS_PER_MINUTE;
            CalendarTime->Minute -= 1;
        }
    }

    if ((CalendarTime->Minute >= MINUTES_PER_HOUR) ||
        (CalendarTime->Minute < 0)) {

        CalendarTime->Hour += CalendarTime->Minute / MINUTES_PER_HOUR;
        CalendarTime->Minute %= MINUTES_PER_HOUR;
        if (CalendarTime->Minute < 0) {
            CalendarTime->Minute += MINUTES_PER_HOUR;
            CalendarTime->Hour -= 1;
        }
    }

    Day = 0;
    if ((CalendarTime->Hour >= HOURS_PER_DAY) ||
        (CalendarTime->Hour < 0)) {

        Day = CalendarTime->Hour / HOURS_PER_DAY;
        CalendarTime->Hour %= HOURS_PER_DAY;
        if (CalendarTime->Hour < 0) {
            CalendarTime->Hour += HOURS_PER_DAY;
            Day -= 1;
        }
    }

    //
    // Skip the days for now as they're tricky. Get the month into range.
    //

    if ((CalendarTime->Month >= MONTHS_PER_YEAR) ||
        (CalendarTime->Month < 0)) {

        CalendarTime->Year += CalendarTime->Month / MONTHS_PER_YEAR;
        CalendarTime->Month %= MONTHS_PER_YEAR;
        if (CalendarTime->Month < 0) {
            CalendarTime->Month += MONTHS_PER_YEAR;
            CalendarTime->Year -= 1;
        }
    }

    //
    // Make the day positive.
    //

    Day += CalendarTime->Day - 1;
    while (Day < 0) {
        CalendarTime->Month -= 1;
        if (CalendarTime->Month < 0) {
            CalendarTime->Year -= 1;
            CalendarTime->Month = MONTHS_PER_YEAR - 1;
        }

        Leap = 0;
        if (IS_LEAP_YEAR(CalendarTime->Year)) {
            Leap = 1;
        }

        Day += RtlDaysPerMonth[Leap][CalendarTime->Month];
    }

    //
    // Now get the day in range.
    //

    while (TRUE) {
        Leap = 0;
        if (IS_LEAP_YEAR(CalendarTime->Year)) {
            Leap = 1;
        }

        if (Day < RtlDaysPerMonth[Leap][CalendarTime->Month]) {
            break;
        }

        Day -= RtlDaysPerMonth[Leap][CalendarTime->Month];
        CalendarTime->Month += 1;
        if (CalendarTime->Month == MONTHS_PER_YEAR) {
            CalendarTime->Year += 1;
            CalendarTime->Month = 0;
        }
    }

    CalendarTime->Day = Day + 1;
    CalendarTime->YearDay = RtlMonthDays[Leap][CalendarTime->Month] + Day;
    Day = CalendarTime->YearDay + RtlpComputeDaysForYear(CalendarTime->Year);
    CalendarTime->Weekday = (TIME_ZONE_EPOCH_WEEKDAY + Day) % DAYS_PER_WEEK;
    if (CalendarTime->Weekday < 0) {
        CalendarTime->Weekday = DAYS_PER_WEEK + CalendarTime->Weekday;
    }

    return;
}

KSTATUS
RtlpCalculateWeekNumber (
    LONG Year,
    LONG YearDay,
    LONG StartingWeekday,
    PLONG WeekNumber
    )

/*++

Routine Description:

    This routine calculates the week number given a year and year day.

Arguments:

    Year - Supplies the year the week resides in.

    YearDay - Supplies the day of the of the year. Valid values are between 0
        and 365.

    StartingWeekday - Supplies the weekday of the start of the week. Usually
        this is Sunday (0) or Monday (1).

    WeekNumber - Supplies a pointer where the week number will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS on failure.

--*/

{

    KSTATUS Status;
    LONG Week1YearDay;
    LONG Weekday;

    //
    // Calculate the year day for week 1.
    //

    Status = RtlpCalculateWeekdayForMonth(Year, TimeZoneMonthJanuary, &Weekday);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Week1YearDay = StartingWeekday - Weekday;
    if (Week1YearDay < 0) {
        Week1YearDay += DAYS_PER_WEEK;
    }

    if (YearDay < Week1YearDay) {
        *WeekNumber = 0;

    } else {
        *WeekNumber = 1 + ((YearDay - Week1YearDay) / DAYS_PER_WEEK);
    }

    return STATUS_SUCCESS;
}

KSTATUS
RtlpCalculateIsoWeekNumber (
    LONG Year,
    LONG YearDay,
    LONG Weekday,
    PLONG WeekNumber,
    PLONG IsoYear
    )

/*++

Routine Description:

    This routine calculates the ISO 8601 week-based year week number and year.

Arguments:

    Year - Supplies the Gregorian calendar year.

    YearDay - Supplies the day of the of the year. Valid values are between 0
        and 365.

    Weekday - Supplies the day of the week. Valid values are between 0 and 6,
        with 0 as Sunday.

    WeekNumber - Supplies an optional pointer where the ISO week number will be
        returned.

    IsoYear - Supplies an optional pointer where the ISO year will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_OUT_OF_BOUNDS on failure.

--*/

{

    LONG Day;
    LONG FinalWeekNumber;
    KSTATUS Status;
    LONG WeekCount;
    LONG YearStartWeekday;

    //
    // Convert to an ISO weekday, where 1 is Monday and 7 is Sunday.
    //

    if (Weekday == 0) {
        Weekday = 7;
    }

    Day = YearDay - Weekday + DAYS_PER_WEEK + TimeZoneWeekdayWednesday;
    FinalWeekNumber = Day / DAYS_PER_WEEK;

    //
    // If the week number is zero, it actually belongs in the last week of the
    // previous year. If the week is 53, it might be the first week of the
    // next year.
    //

    if ((FinalWeekNumber == 0) || (FinalWeekNumber == 53)) {
        if (FinalWeekNumber == 0) {
            Year -= 1;
        }

        Status = RtlpCalculateWeekdayForMonth(Year,
                                              TimeZoneMonthJanuary,
                                              &YearStartWeekday);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        WeekCount = 52;
        if ((YearStartWeekday == TimeZoneWeekdayThursday) ||
            ((IS_LEAP_YEAR(Year)) &&
             (YearStartWeekday == TimeZoneWeekdayWednesday))) {

            WeekCount = 53;
        }

        if (FinalWeekNumber == 0) {
            FinalWeekNumber = WeekCount;

        } else {
            if (FinalWeekNumber > WeekCount) {
                Year += 1;
                FinalWeekNumber = 1;
            }
        }
    }

    if (WeekNumber != NULL) {
        *WeekNumber = FinalWeekNumber;
    }

    if (IsoYear != NULL) {
        *IsoYear = Year;
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
RtlpScanTimeStrings (
    PCSTR Input,
    PSTR *Strings,
    ULONG StringCount,
    PLONG Index,
    PLONG Size
    )

/*++

Routine Description:

    This routine attempts to scan one of the set of given time strings, case
    insensitively.

Arguments:

    Input - Supplies a pointer to the input string to scan.

    Strings - Supplies the array of possible strings to scan.

    StringCount - Supplies the number of strings in the array.

    Index - Supplies a pointer where the index of the matching string will be
        returned on success.

    Size - Supplies a pointer where the number of characters scanned will be
        returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_MATCH if none of the strings matched.

--*/

{

    CHAR CompareCharacter;
    ULONG CompareIndex;
    PSTR CompareString;
    CHAR InputCharacter;
    ULONG StringIndex;

    for (StringIndex = 0; StringIndex < StringCount; StringIndex += 1) {
        CompareIndex = 0;
        CompareString = Strings[StringIndex];
        while (TRUE) {
            CompareCharacter =
                   RtlConvertCharacterToLowerCase(CompareString[CompareIndex]);

            InputCharacter =
                           RtlConvertCharacterToLowerCase(Input[CompareIndex]);

            if ((CompareCharacter == '\0') || (InputCharacter == '\0') ||
                (CompareCharacter != InputCharacter)) {

                break;
            }

            CompareIndex += 1;
        }

        //
        // If there was a match, return it.
        //

        if (CompareCharacter == '\0') {
            *Index = StringIndex;
            *Size = CompareIndex;
            return STATUS_SUCCESS;
        }
    }

    return STATUS_NO_MATCH;
}

