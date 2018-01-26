/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tzfmt.h

Abstract:

    This header contains definitions for the time zone binary file.

Author:

    Evan Green 2-Aug-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define TIME_ZONE_HEADER_MAGIC 0x6E5A6D54 // 'nZmT'

//
// Define the minimum and maximum years. Insert quip about lack of vision here.
//

#define MIN_TIME_ZONE_YEAR 1
#define MAX_TIME_ZONE_YEAR 9999

//
// Define the minimum and maximum dates. The minimum is
// Midnight on January 1, 0001, and the maximum is one second shy of Midnight
// on January 1, 10000.
//

#define MIN_TIME_ZONE_DATE (LONGLONG)(-63113904000LL)
#define MAX_TIME_ZONE_DATE (LONGLONG)(252423993599LL)

//
// Define the year in which time is "zero" (at midnight on January 1st, GMT).
// This works out to being on a 400 year cycle, which makes calculations easier.
//

#define TIME_ZONE_EPOCH_YEAR 2001
#define TIME_ZONE_EPOCH_WEEKDAY TimeZoneWeekdayMonday

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TIME_ZONE_MONTH {
    TimeZoneMonthJanuary,
    TimeZoneMonthFebruary,
    TimeZoneMonthMarch,
    TimeZoneMonthApril,
    TimeZoneMonthMay,
    TimeZoneMonthJune,
    TimeZoneMonthJuly,
    TimeZoneMonthAugust,
    TimeZoneMonthSeptember,
    TimeZoneMonthOctober,
    TimeZoneMonthNovember,
    TimeZoneMonthDecember,
    TimeZoneMonthCount
} TIME_ZONE_MONTH, *PTIME_ZONE_MONTH;

typedef enum _TIME_ZONE_WEEKDAY {
    TimeZoneWeekdaySunday,
    TimeZoneWeekdayMonday,
    TimeZoneWeekdayTuesday,
    TimeZoneWeekdayWednesday,
    TimeZoneWeekdayThursday,
    TimeZoneWeekdayFriday,
    TimeZoneWeekdaySaturday,
    TimeZoneWeekdayCount
} TIME_ZONE_WEEKDAY, *PTIME_ZONE_WEEKDAY;

typedef enum _TIME_ZONE_OCCASION_TYPE {
    TimeZoneOccasionInvalid,
    TimeZoneOccasionMonthDate,
    TimeZoneOccasionLastWeekday,
    TimeZoneOccasionGreaterOrEqualWeekday,
    TimeZoneOccasionLessOrEqualWeekday,
} TIME_ZONE_OCCASION_TYPE, *PTIME_ZONE_OCCASION_TYPE;

typedef enum _TIME_ZONE_LENS {
    TimeZoneLensInvalid,
    TimeZoneLensLocalTime,
    TimeZoneLensLocalStandardTime,
    TimeZoneLensUtc
} TIME_ZONE_LENS, *PTIME_ZONE_LENS;

/*++

Structure Description:

    This structure stores the global file header for the time zone file.

Members:

    Magic - Stores a known constant (see TIME_ZONE_HEADER_MAGIC).

    RuleOffset - Stores the file offset for the array of rules.

    RuleCount - Stores the count of rules.

    ZoneOffset - Stores the file offset for the array of time zones.

    ZoneCount - Stores the number of time zones in the array.

    ZoneEntryOffset - Stores the file offset for the entries in a time zone.

    ZoneEntryCount - Stores the number of zone entries in the array.

    LeapOffset - Stores the file offset for all the leap seconds.

    LeapCount - Stores the count of leap second entries in the array.

    StringsOffset - Stores the offset into the string table.

    StringsSize - Stores the size of the string table in bytes.

--*/

#pragma pack(push, 1)

typedef struct _TIME_ZONE_HEADER {
    ULONG Magic;
    ULONG RuleOffset;
    ULONG RuleCount;
    ULONG ZoneOffset;
    ULONG ZoneCount;
    ULONG ZoneEntryOffset;
    ULONG ZoneEntryCount;
    ULONG LeapOffset;
    ULONG LeapCount;
    ULONG StringsOffset;
    ULONG StringsSize;
} PACKED TIME_ZONE_HEADER, *PTIME_ZONE_HEADER;

/*++

Structure Description:

    This structure stores information about on occasion when a time zone or rule
    becomes active.

Members:

    Type - Stores the type of occasion (see TIME_ZONE_OCCASION_TYPE).

    MonthDay - Stores the day of the month when the rule becomes valid (if
        relevant to the type).

    Weekday - Stores the day of the week when the rule becomes valid (if
        relevant to the type).

--*/

typedef struct _TIME_ZONE_OCCASION {
    CHAR Type;
    CHAR MonthDay;
    CHAR Weekday;
} PACKED TIME_ZONE_OCCASION, *PTIME_ZONE_OCCASION;

/*++

Structure Description:

    This structure stores the structure of a time zone rule in the time zone
    file.

Members:

    Number - Stores the rule number for this rule.

    From - Stores the starting year this rule was in effect.

    To - Stores the ending year this rule was is in effect.

    Month - Stores the month this rule applies to.

    On - Stores the occasion this rule applies on.

    At - Storse the time this rule applies.

    AtLens - Stores the lens with which to view the at time.

    Padding - Stores padding used to align this structure.

    Save - Stores the amount of time to save in seconds.

    Letters - Stores an offset into the string table where the letters used for
        substitution are placed.

--*/

typedef struct _TIME_ZONE_RULE {
    ULONG Number;
    SHORT From;
    SHORT To;
    CHAR Month;
    TIME_ZONE_OCCASION On;
    LONG At;
    CHAR AtLens;
    CHAR Padding[3];
    LONG Save;
    ULONG Letters;
} PACKED TIME_ZONE_RULE, *PTIME_ZONE_RULE;

/*++

Structure Description:

    This structure stores information about a time zone.

Members:

    Name - Stores an offset into the string table where the name of this time
        zone resides.

    EntryIndex - Stores the index into the array of time zone entries where the
        entries for this time zone begin.

    EntryCount - Stores the count of valid time zone entries in the array.

--*/

typedef struct _TIME_ZONE {
    ULONG Name;
    ULONG EntryIndex;
    ULONG EntryCount;
} PACKED TIME_ZONE, *PTIME_ZONE;

/*++

Structure Description:

    This structure stores information about a time zone entry.

Members:

    GmtOffset - Stores the offset in seconds from GMT.

    Rules - Stores the number of a set of rules by which this time zone entry
        abides. This may be -1 if no rules apply.

    Save - Stores the direct amount of time to save.

    Format - Stores an offset into the string table where a string containing
        the abbreviated format of the time zone will be returned. If there's
        a slash, that separates the Standard time from Daylight time
        (ie PST/PDT). If there's a %s, then the "letters" string in the applied
        rule should be substituted.

    Until - Stores the date until which this zone entry applies.

--*/

typedef struct _TIME_ZONE_ENTRY {
    LONG GmtOffset;
    ULONG Rules;
    LONG Save;
    ULONG Format;
    LONGLONG Until;
} PACKED TIME_ZONE_ENTRY, *PTIME_ZONE_ENTRY;

/*++

Structure Description:

    This structure stores information about a leap second.

Members:

    Date - Stores the date at which this leap second occurred.

    Positive - Stores a non-zero value if the leap second was positive (a
        second was added to the normal course of time) or 0 if the second was
        negative (a second was removed from the normal course of time).

    LocalTime - Stores a non-zero value if the leap second was "rolling",
        meaning the given date is in local time. Stores 0 if the leap second
        was "stationary", meaning the given date is in GMT.

    Padding - Stores a reserved byte currently used to align the structure.

--*/

typedef struct _TIME_ZONE_LEAP_SECOND {
    LONGLONG Date;
    CHAR Positive;
    CHAR LocalTime;
    CHAR Padding;
} PACKED TIME_ZONE_LEAP_SECOND, *PTIME_ZONE_LEAP_SECOND;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
