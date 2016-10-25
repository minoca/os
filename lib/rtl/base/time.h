/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.h

Abstract:

    This header contains internal definitions for time functions within the
    runtime library base.

Author:

    Evan Green 31-Aug-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global pointer to the time zone data.
//

extern PVOID RtlTimeZoneData;
extern ULONG RtlTimeZoneDataSize;
extern ULONG RtlTimeZoneIndex;
extern PTIME_ZONE_LOCK_FUNCTION RtlAcquireTimeZoneLock;
extern PTIME_ZONE_LOCK_FUNCTION RtlReleaseTimeZoneLock;

//
// Store arrays of the names of the months and weeks.
//

extern PSTR RtlMonthStrings[MONTHS_PER_YEAR];
extern PSTR RtlAbbreviatedMonthStrings[MONTHS_PER_YEAR];
extern PSTR RtlWeekdayStrings[DAYS_PER_WEEK];
extern PSTR RtlAbbreviatedWeekdayStrings[DAYS_PER_WEEK];
extern PSTR RtlAmPmStrings[2][2];
extern CHAR RtlDaysPerMonth[2][MONTHS_PER_YEAR];
extern SHORT RtlMonthDays[2][MONTHS_PER_YEAR];

//
// -------------------------------------------------------- Function Prototypes
//

LONG
RtlpComputeYearForDays (
    PLONG Days
    );

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

LONG
RtlpComputeDaysForYear (
    LONG Year
    );

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

KSTATUS
RtlpCalculateWeekdayForMonth (
    LONG Year,
    LONG Month,
    PLONG Weekday
    );

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

VOID
RtlpNormalizeCalendarTime (
    PCALENDAR_TIME CalendarTime
    );

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

KSTATUS
RtlpCalculateWeekNumber (
    LONG Year,
    LONG YearDay,
    LONG StartingWeekday,
    PLONG WeekNumber
    );

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

KSTATUS
RtlpCalculateIsoWeekNumber (
    LONG Year,
    LONG YearDay,
    LONG Weekday,
    PLONG WeekNumber,
    PLONG IsoYear
    );

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

