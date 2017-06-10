/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.ck

Abstract:

    This module implements support for time in Chalk.

Author:

    Evan Green 10-Jan-2016

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import _time;

//
// ---------------------------------------------------------------- Definitions
//

var daysPerMonth = [
    [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31],
    [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
];

var monthDays = [
    [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334],
    [0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335]
];

var DAYS_IN_4_YEARS = 1461;
var DAYS_IN_100_YEARS = 36524;
var DAYS_IN_400_YEARS = 146097;
var MAX_ORDINAL = 3652059;

//
// Define the ordinal for January 1, 1970.
//

var EPOCH_ORDINAL = 719163;

//
// ----------------------------------------------- Internal Function Prototypes
//

function
_dateToOrdinal (
    year,
    month,
    day
    );

function
_ordinalToDate (
    ordinal
    );

function
_isoWeek1 (
    year
    );

function
_daysBeforeYear (
    year
    );

function
_daysBeforeMonth (
    year,
    month
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Instantiate the two main time zones.
//

var utc;
var localTime;

//
// ------------------------------------------------------------------ Functions
//

function
isLeapYear (
    year
    )


/*++

Routine Description:

    This routine returns whether or not the given year is a leap year.

Arguments:

    year - Supplies the year.

Return Value:

    true if the given year is a leap year.

    false if the given year is not a leap year.

--*/

{

    return ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
}

class TimeZoneNotImplemented is Exception {}
class TimeOverflowError is Exception {}

//
// The TimeDelta class stores a relative time.
//

class TimeDelta {
    function
    __init (
        )


    /*++

    Routine Description:

        This routine initializes a time delta object with a delta of zero.

    Arguments:

        None.

    Return Value:

        Returns the initialized object.

    --*/

    {

        return this.__init(0, 0, 0);
    }

    function
    __init (
        days,
        seconds,
        nanoseconds
        )


    /*++

    Routine Description:

        This routine initializes a time delta object with the given values.

    Arguments:

        days - Supplies the number of days in the delta.

        seconds - Supplies the number of seconds in the delta. This will be
            normalized to a positive value between 0 and 86400 (exclusive).

        nanoseconds - Supplies the number of nanoseconds in the delta. This
            will be normalized to a positive value between 0 and 1000000000
            (exclusive).

    Return Value:

        Returns the initialized object.

    --*/

    {

        if ((!(days is Int)) || (!(seconds is Int)) ||
            (!(nanoseconds is Int))) {

            Core.raise(TypeError("Expected an integer"));
        }

        //
        // Normalize nanoseconds into seconds.
        //

        seconds += (nanoseconds / 1000000000);
        nanoseconds %= 1000000000;
        if (nanoseconds < 0) {
            nanoseconds += 1000000000;
            seconds -= 1;
        }

        //
        // Normalize seconds into days.
        //

        days += seconds / 86400;
        seconds %= 86400;
        if (seconds < 0) {
            seconds += 86400;
            days -= 1;
        }

        if ((days < -999999999) || (days > 999999999)) {
            Core.raise(TimeOverflowError());
        }

        super.__set("days", days);
        super.__set("seconds", seconds);
        super.__set("nanoseconds", nanoseconds);
        return this;
    }

    function
    totalSeconds (
        )


    /*++

    Routine Description:

        This routine returns the total number of seconds in the time delta
        object.

    Arguments:

        None.

    Return Value:

        Returns the number of seconds as an integer. Nanoseconds are ignored.

    --*/

    {

        return (this.days * 86400) + this.seconds;
    }

    function
    __set (
        key,
        value
        )


    /*++

    Routine Description:

        This routine prohibits setting of timedelta values, effectively making
        them immutable.

    Arguments:

        key - Supplies the key attempting to be set.

        value - Supplies the value attempting to be set.

    Return Value:

        Raises an error.

    --*/

    {

        Core.raise(ValueError("TimeDelta objects are immutable"));
    }

    function
    __add (
        right
        )


    /*++

    Routine Description:

        This routine adds two time delta's together.

    Arguments:

        right - Supplies the right side to add.

    Return Value:

        Returns a new time delta containing the sum.

    --*/

    {

        return TimeDelta(this.days + right.days,
                         this.seconds + right.seconds,
                         this.nanoseconds + right.nanoseconds);
    }

    function
    __sub (
        right
        )


    /*++

    Routine Description:

        This routine subtracts a time delta from this one.

    Arguments:

        right - Supplies the right side to add.

    Return Value:

        Returns the difference between the two time deltas.

    --*/

    {

        return TimeDelta(this.days - right.days,
                         this.seconds - right.seconds,
                         this.nanoseconds - right.nanoseconds);
    }

    function
    __eq (
        right
        )


    /*++

    Routine Description:

        This routine determines if two time deltas are equal.

    Arguments:

        right - Supplies the right side to compare against.

    Return Value:

        true if the values are equal.

        false if the values are not equal.

    --*/

    {

        if (!(right is TimeDelta)) {
            return false;
        }

        return (this.days == right.days) && (this.seconds == right.seconds) &&
               (this.nanoseconds == right.nanoseconds);
    }

    function
    __ne (
        right
        )


    /*++

    Routine Description:

        This routine determines if two time deltas are not equal.

    Arguments:

        right - Supplies the right side to compare against.

    Return Value:

        true if the values are not equal.

        false if the values are equal.

    --*/

    {

        return !this.__eq(right);
    }

    function
    __neg (
        )


    /*++

    Routine Description:

        This routine returns the negative of this time delta object, which
        simply negates the days.

    Arguments:

        None.

    Return Value:

        Returns a new time delta that is the negative of this time delta.

    --*/

    {

        return TimeDelta(-this.days, this.seconds, this.nanoseconds);
    }

    function
    abs (
        )


    /*++

    Routine Description:

        This routine returns the absolute value of the given time delta. It
        returns the same value if this object's days are >= 0, or -days if
        the days are less than zero.

    Arguments:

        None.

    Return Value:

        Returns a new time delta that is the absolute value.

    --*/

    {

        if (this.days >= 0) {
            return this;
        }

        return TimeDelta(-this.days, this.seconds, this.nanoseconds);
    }

    function
    __str (
        )


    /*++

    Routine Description:

        This routine returns a string representation of the time delta in the
        form "[D day[s], ][H]H:MM:SS[.NNNNNNNNN]".

    Arguments:

        None.

    Return Value:

        Returns a string representation of the time delta.

    --*/

    {

        var hours = this.seconds / 3600;
        var minutes = (this.seconds - (hours * 3600)) / 60;
        var result = "";
        var seconds = this.seconds % 60;

        if (this.days != 0) {
            if (this.days == 1) {
                result = "1 day, ";

            } else {
                result = "%d days, " % this.days;
            }
        }

        result += "%d:%02d:%02d" % [hours, minutes, seconds];
        if (this.nanoseconds != 0) {
            result += ".%09d" % this.nanoseconds;
        }

        return result;
    }

    function
    __repr (
        )


    /*++

    Routine Description:

        This routine returns a string representation of the time delta in the
        form "TimeDelta(D[, S[, N]])".

    Arguments:

        None.

    Return Value:

        Returns a string representation of the time delta.

    --*/

    {

        var result = "TimeDelta(%d" % this.days;

        if ((this.seconds != 0) || (this.nanoseconds != 0)) {
            result += ", %d" % this.seconds;
            if (this.nanoseconds != 0) {
                result += ", %d" % this.nanoseconds;
            }
        }

        return result + ")";
    }
}

//
// The Time class stores a date and time, and is immutable.
//

class Time {

    static
    function
    fromTimestamp (
        timestamp,
        nanoseconds,
        zone
        )


    /*++

    Routine Description:

        This routine initializes a time object with a number of seconds since
        January 1, 1970 GMT and a time zone.

    Arguments:

        timestamp - Supplies the number of seconds since 1970.

        nanoseconds - Supplies the nanoseconds value to set.

        zone - Supplies the time zone.

    Return Value:

        Returns a new Time instance corresponding to the given timestamp.

    --*/

    {

        var utcTime = Time.fromUtcTimestamp(timestamp, nanoseconds);

        if (zone == null) {
            zone = localTime;
        }

        return zone.fromUtc(utcTime.replaceZone(zone));
    }

    static
    function
    fromUtcTimestamp (
        timestamp,
        nanoseconds
        )


    /*++

    Routine Description:

        This routine initializes a time object with a number of seconds since
        January 1, 1970 GMT and a time zone.

    Arguments:

        timestamp - Supplies the number of seconds since 1970.

        nanoseconds - Supplies the nanoseconds value to set.

        zone - Supplies the time zone.

    Return Value:

        Returns a new Time instance corresponding to the given timestamp.

    --*/

    {

        var result = Time();

        result._set("second", timestamp);
        result._set("nanosecond", nanoseconds);
        result._set("zone", utc);
        result._normalize();
        return result;
    }

    static
    function
    fromOrdinal (
        ordinal,
        zone
        )


    /*++

    Routine Description:

        This routine initializes a time object to be midnight on the given
        ordinal day.

    Arguments:

        ordinal - Supplies the ordinal day.

        zone - Supplies the time zone.

    Return Value:

        Returns a new Time instance corresponding to midnight on the given
        ordinal date.

    --*/

    {

        var ymd = _ordinalToDate(ordinal);

        return Time(ymd[0], ymd[1], ymd[2], 0, 0, 0, 0, zone);
    }

    static
    function
    now (
        )


    /*++

    Routine Description:

        This routine returns the current date and time in the platform's
        local time zone.

    Arguments:

        None.

    Return Value:

        Returns a new Time instance corresponding to the current time.

    --*/

    {

        var currentTime = (_time.clock_gettime)(_time.CLOCK_REALTIME);

        return Time.fromTimestamp(currentTime[0], currentTime[1], localTime);
    }

    static
    function
    nowUtc (
        )


    /*++

    Routine Description:

        This routine returns the current date and time in UTC time.

    Arguments:

        None.

    Return Value:

        Returns a new Time instance corresponding to the current time in
        Universal Coordinated Time.

    --*/

    {

        var currentTime = (_time.clock_gettime)(_time.CLOCK_REALTIME);

        return Time.fromUtcTimestamp(currentTime[0], currentTime[1]);
    }

    function
    __init (
        )


    /*++

    Routine Description:

        This routine initializes a time object with a value of midnight
        January 1, 1970, GMT.

    Arguments:

        None.

    Return Value:

        Returns the initialized object.

    --*/

    {

        return this.__init(1970, 1, 1, 0, 0, 0, 0, utc);
    }

    function
    __init (
        year,
        month,
        day,
        hour,
        minute,
        second,
        nanosecond,
        zone
        )


    /*++

    Routine Description:

        This routine initializes a time object with the given values.

    Arguments:

        year - Supplies the year. Valid years are between 1 and 9999, inclusive.

        month - Supplies the month. Valid months are between 1 and 12,
            inclusive.

        day - Supplies the day of the month. Valid days are between 1 and
            however many days the given year and month have, inclusive.

        hour - Supplies the hour. Valid values are between 0 and 23, inclusive.

        minute - Supplies the minute. Valid minutes are between 0 and 59,
            inclusive.

        second - Supplies the second. Valid seconds are between 0 and 60,
            inclusive. 60 is allowed to account for leap seconds.

        nanosecond - Supplies the nanosecond. Valid values are between 0 and
            999999999, inclusive.

        zone - Supplies the time zone.

    Return Value:

        Returns the initialized object.

    --*/

    {

        var maxDay;

        if ((!(year is Int)) || (!(month is Int)) || (!(day is Int)) ||
            (!(hour is Int)) || (!(minute is Int)) || (!(second is Int)) ||
            (!(nanosecond is Int))) {

            Core.raise(TypeError("Expected an integer"));
        }

        if (zone == null) {
            Core.raise(TypeError("Expected a time zone"));
        }

        if ((year < 1) || (year > 9999)) {
            Core.raise(TimeOverflowError("Bad year %d" % year));
        }

        if ((month < 1) || (month > 12)) {
            Core.raise(TimeOverflowError("Bad month %d" % month));
        }

        maxDay = daysPerMonth[isLeapYear(year)][month - 1];
        if ((day < 1) || (day > maxDay)) {
            Core.raise(TimeOverflowError("Bad day %d (%02d/%04d has %d days)" %
                                         [day, month, year, maxDay]));
        }

        if ((hour < 0) || (hour > 23)) {
            Core.raise(TimeOverflowError("Bad hour %d" % hour));
        }

        if ((minute < 0) || (minute > 59)) {
            Core.raise(TimeOverflowError("Bad minute %d" % minute));
        }

        if ((second < 0) || (second > 60)) {
            Core.raise(TimeOverflowError("Bad second %d" % second));
        }

        if ((nanosecond < 0) || (nanosecond > 999999999)) {
            Core.raise(TimeOverflowError("Bad nanosecond %d" % nanosecond));
        }

        super.year = year;
        super.month = month;
        super.day = day;
        super.hour = hour;
        super.minute = minute;
        super.second = second;
        super.nanosecond = nanosecond;
        super.zone = zone;
        return this;
    }

    function
    replaceDate (
        year,
        month,
        day
        )

    /*++

    Routine Description:

        This routine returns a new date that is a copy of this one except with
        the given date.

    Arguments:

        year - Supplies the year the new date should have.

        month - Supplies the month the new date should have.

        day - Supplies the day the new day of the month the new date should
            have.

    Return Value:

        Returns a new Time instance with the given values, and the values of
        the current instance for time of day.

    --*/

    {

        return Time(year,
                    month,
                    day,
                    this.hour,
                    this.minute,
                    this.second,
                    this.nanosecond,
                    this.zone);
    }

    function
    replaceTime (
        hour,
        minute,
        second,
        nanosecond
        )

    /*++

    Routine Description:

        This routine returns a new date that is a copy of this one except with
        the given time.

    Arguments:

        hour - Supplies the hour the new time should have (between 0 and 23).

        minute - Supplies the minute the new time should have.

        second - Supplies the second the new time should have.

        nanosecond - Supplies the nanosecond the new time should have.

    Return Value:

        Returns a new Time instance with the given time of day and the same
        date as the current instance.

    --*/

    {

        return Time(this.year,
                    this.month,
                    this.day,
                    hour,
                    minute,
                    second,
                    nanosecond,
                    this.zone);
    }

    function
    midnight (
        )

    /*++

    Routine Description:

        This routine is shorthand for this.replacetime(0, 0, 0, 0).

    Arguments:

        None.

    Return Value:

        Returns a new time instance with the same date, at midnight.

    --*/

    {

        return Time(this.year, this.month, this.day, 0, 0, 0, 0, this.zone);
    }

    function
    replaceZone (
        zone
        )

    /*++

    Routine Description:

        This routine returns a new Time with a different time zone.

    Arguments:

        zone - Supplies the zone of the new date.

    Return Value:

        Returns a new Time instance with the same date and time as this
        instance, but with a new time zone.

    --*/

    {

        return Time(this.year,
                    this.month,
                    this.day,
                    this.hour,
                    this.minute,
                    this.second,
                    this.nanosecond,
                    zone);
    }

    function
    weekday (
        )

    /*++

    Routine Description:

        This routine returns the weekday for the time value instance.

    Arguments:

        None.

    Return Value:

        Returns the weekday, where Monday is 0 and Sunday is 6.

    --*/

    {

        //
        // 1/1/1 was theoretically a Monday.
        //

        return (_dateToOrdinal(this.year, this.month, this.day) + 6) % 7;
    }

    function
    isoWeekday (
        )

    /*++

    Routine Description:

        This routine returns the ISO weekday for time instance.

    Arguments:

        None.

    Return Value:

        Returns the weekday, where Monday is 1 and Sunday is 7.

    --*/

    {

        //
        // 1/1/1 was theoretically a Monday.
        //

        return this.weekday() + 1;
    }

    function
    isoCalendar (
        )

    /*++

    Routine Description:

        This routine returns the ISO year, week, and weekday for this instance's
        time.

    Arguments:

        None.

    Return Value:

        Returns the [year, ISO week, weekday].

    --*/

    {

        var day;
        var today;
        var week;
        var year = this.year;
        var yearStart = _isoWeek1(year);

        today = _dateToOrdinal(year, this.month, this.day);
        week = (today - yearStart) / 7;
        day = (today - yearStart) - (week * 7);
        if (day < 0) {
            day += 7;
            week -= 1;
        }

        if (week < 0) {
            year -= 1;
            week = _isoWeek1(year);
            week = (today - yearStart) / 7;
            day = (today - yearStart) - (week * 7);

        } else if ((week >= 52) && (today >= _isoWeek1(year + 1))) {
            week = 0;
            year += 1;
        }

        return [year, week + 1, day + 1];
    }

    function
    ordinal (
        )

    /*++

    Routine Description:

        This routine returns the ordinal day of this date, where 1/1/0001 is
        ordinal day 1.

    Arguments:

        None.

    Return Value:

        Returns the ordinal day.

    --*/

    {

        return _dateToOrdinal(this.year, this.month, this.day);
    }

    function
    tzName (
        )


    /*++

    Routine Description:

        This routine returns the name of the time zone: equivalent to
        this.zone.tzName(this).

    Arguments:

        None.

    Return Value:

        Returns the name of the current time zone.

    --*/

    {

        return this.zone.tzName(this);
    }

    function
    dst (
        )


    /*++

    Routine Description:

        This routine returns the current offset from UTC time.

    Arguments:

        None.

    Return Value:

        Returns a TimeDelta containing the current time zone offset for this
        time.

    --*/

    {

        return this.zone.dst(this);
    }

    function
    utcOffset (
        )


    /*++

    Routine Description:

        This routine returns the baseline offset from UTC of this timezone.

    Arguments:

        None.

    Return Value:

        Returns the equivalent of this.zone.utcOffset(null).

    --*/

    {

        return this.zone.utcOffset(this);
    }

    function
    moveZone (
        zone
        )


    /*++

    Routine Description:

        This routine returns the equivalent absolute time but with a different
        time zone.

    Arguments:

        Zone - Supplies the new zone to set.

    Return Value:

        Returns a new Time instance with the same absolute time but a different
        time zone.

    --*/

    {

        var utc;

        if (this.zone == zone) {
            return this;
        }

        //
        // Convert the value to UTC, and then attach this zone.
        //

        utc = this - this.utcOffset();
        utc = utc.replaceZone(zone);

        //
        // Convert from a UTC time to the zone's local time.
        //

        return zone.fromUtc(utc);
    }

    function
    timestamp (
        )


    /*++

    Routine Description:

        This routine returns the number of seconds since 1970 corresponding to
        the given time.

    Arguments:

        None.

    Return Value:

        Returns the number of seconds since 1970 corresponding to this date.

    --*/

    {

        var days = this.ordinal() - EPOCH_ORDINAL;
        var seconds = (days * 86400);

        seconds += (this.hour * 3600) + (this.minute * 60) + this.second;
        seconds -= this.utcOffset().totalSeconds();
        return seconds;
    }

    function
    compare (
        right
        )


    /*++

    Routine Description:

        This routine compares two time values in time.

    Arguments:

        right - Supplies the right side to compare against.

    Return Value:

        > 0 if this time is greater than the right.

        0 if this time is equal to the right.

        < 0 if this time is less than the right.

    --*/

    {

        var difference = this.timestamp() - right.timestamp();

        if (difference == 0) {
            difference = this.nanosecond - right.nanosecond;
        }

        return difference;
    }

    function
    tm (
        )


    /*++

    Routine Description:

        This routine returns a tm dictionary suitable for use with the lower
        level time functions.

    Arguments:

        None.

    Return Value:

        returns a tm dictionary.

    --*/

    {

        var dict;
        var yearDay;

        if (this.year < 1900) {
            Core.raise(
                    ValueError("Cannot create tm dict for years before 1900"));
        }

        yearDay = monthDays[isLeapYear(this.year)][this.month - 1] +
                  this.day - 1;

        dict = {
            "tm_sec": this.second,
            "tm_min": this.minute,
            "tm_hour": this.hour,
            "tm_mday": this.day,
            "tm_mon": this.month - 1,
            "tm_year": this.year - 1900,
            "tm_wday": (this.weekday() + 1) % 7,
            "tm_yday": yearDay,
            "tm_isdst": this.dst().seconds != 0,
            "tm_nanosecond": this.nanosecond,
            "tm_gmtoff": this.utcOffset().seconds,
            "tm_zone": this.tzName()
        };

        return dict;
    }

    function
    strftime (
        format
        )

    /*++

    Routine Description:

        This routine returns a formatted string of the current time.

    Arguments:

        Supplies the format string. Valid specifiers are:
            %a - Replaced by the abbreviated weekday.
            %A - Replaced by the full weekday.
            %b - Replaced by the abbreviated month name.
            %B - Replaced by the full month name.
            %c - Replaced by the locale's appropriate date and time
                 representation.
            %C - Replaced by the year divided by 100 (century) [00,99].
            %d - Replaced by the day of the month [01,31].
            %e - Replaced by the day of the month [ 1,31]. A single digit is
                 preceded by a space.
            %F - Equivalent to "%Y-%m-%d" (the ISO 8601:2001 date format).
            %G - The ISO 8601 week-based year [0001,9999]. The week-based year
                 and the Gregorian year can differ in the first week of January.
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
            %u - Replaced by the weekday number, with 1 representing Monday
                 [1,7].
            %V - Replaced by the week number of the year with Monday as the
                 first day in the week [01,53]. If the week containing January
                 1st has 4 or more days in the new year, it is considered week
                 1. Otherwise, it is the last week of the previous year, and
                 the next week is 1.
            %w - Replaced by the weekday number [0,6], with 0 representing
                 Sunday.
            %x - Replaced by the locale's appropriate date representation.
            %X - Replaced by the locale's appropriate time representation.
            %y - Replaced by the last two digits of the year [00,99].
            %Y - Replaced by the full four digit year [0001,9999].
            %z - Replaced by the offset from UTC in the standard ISO 8601:2000
                 standard format (+hhmm or -hhmm), or by no characters if no
                 timezone is terminable. If the "is daylight saving" member of
                 this calendar structure is greater than zero, then the
                 daylight saving offset is used. If the dayslight saving member
                 of the calendar structure is negative, no characters are
                 returned.
            %Z - Replaced by the timezone name or abbreviation, or by no bytes
                 if no timezone information exists.
            %% - Replaced by a literal '%'.


    Return Value:

        Returns the [year, ISO week, weekday].

    --*/

    {

        var index;
        var result = "";
        var specifier;
        var value;

        while (1) {
            index = format.indexOf("%");
            if (index == -1) {
                result += format;
                break;
            }

            result += format[0..index];
            specifier = format[index + 1];
            if (index == format.length()) {
                format = "";

            } else {
                format = format[(index + 2)...-1];
            }

            if ((specifier == "a") || (specifier == "A") ||
                (specifier == "b") || (specifier == "B") ||
                (specifier == "c") ||
                (specifier == "x") || (specifier == "X")) {

                value = "%" + specifier;
                result += (_time.strftime)(value, this.tm());

            } else if (specifier == "C") {
                result += "%02d" % (this.year / 100);

            } else if (specifier == "d") {
                result += "%02d" % this.day;

            } else if (specifier == "e") {
                result += "%2d" % this.day;

            } else if (specifier == "F") {
                result += "%04d-%02d-%02d" % [this.year, this.month, this.day];

            } else if (specifier == "G") {
                result += "%04d" % this.isoCalendar()[0];

            } else if (specifier == "H") {
                result += "%02d" % this.hour;

            } else if (specifier == "I") {
                value = this.hour;
                if (value == 0) {
                    value = 12;

                } else if (value > 12) {
                    value -= 12;
                }

                result += "%02d" % value;

            } else if (specifier == "J") {
                result += "%09d" % this.nanosecond;

            } else if (specifier == "j") {
                value = monthDays[isLeapYear(this.year)][this.month - 1];
                result += "%03d" % value;

            } else if (specifier == "m") {
                result += "%02d" % this.month;

            } else if (specifier == "M") {
                result += "%02d" % this.minute;

            } else if (specifier == "N") {
                result += "%06d" % (this.nanosecond / 1000);

            } else if (specifier == "n") {
                result += "\n";

            } else if (specifier == "p") {
                if ((this.hour < 12) || (this.hour == 23)) {
                    result += "AM";

                } else {
                    result += "PM";
                }

            } else if (specifier == "P") {
                if ((this.hour < 12) || (this.hour == 23)) {
                    result += "am";

                } else {
                    result += "pm";
                }

            } else if (specifier == "q") {
                result += "%03d" % (this.nanosecond / 1000000);

            } else if (specifier == "r") {
                result += this.strftime("%I:%M:%S %p");

            } else if (specifier == "R") {
                result += this.strftime("%H:%M");

            } else if (specifier == "S") {
                result += "%02d" % this.second;

            } else if (specifier == "s") {
                result += "%d" % this.timestamp();

            } else if (specifier == "t") {
                result += "\t";

            } else if (specifier == "T") {
                result += this.strftime("%H:%M:%S");

            } else if (specifier == "u") {
                result += "%d" % this.isoWeekday();

            } else if (specifier == "V") {
                result += "%02d" % this.isoCalendar()[1];

            } else if (specifier == "w") {
                result += "%d" % (this.isoWeekday() % 7);

            } else if (specifier == "x") {
                result += _time.dateString(this.tm());

            } else if (specifier == "X") {
                result += _time.timeString(this.tm());

            } else if (specifier == "y") {
                result += "%02d" % (this.year % 100);

            } else if (specifier == "Y") {
                result += "%04d" % this.year;

            } else if (specifier == "z") {
                value = this.zone.utcOffset(this).totalSeconds() / 60;
                if (value < 0) {
                    result += "-";
                    value = -value;

                } else {
                    result += "+";
                }

                result += "%02d%02d" % [value / 60, value % 60];

            } else if (specifier == "Z") {
                result += this.zone.tzName(this);

            } else if (specifier == "%") {
                result += "%";

            } else {
                Core.raise(FormatError("Invalid specifier '%s'" % specifier));
            }
        }

        return result;
    }

    function
    __str (
        )

    /*++

    Routine Description:

        This routine returns a string of the date, as
            YYYY-MM-DD HH:MM:SS[.nnnnnnnnn] TZ.

    Arguments:

        None.

    Return Value:

        Returns the string representation.

    --*/

    {

        var nanosecond = "";

        if (this.nanosecond != 0) {
            nanosecond = ".%09d" % this.nanosecond;
        }

        return "%04d-%02d-%02d %02d:%02d:%02d%s %s" %
               [this.year,
                this.month,
                this.day,
                this.hour,
                this.minute,
                this.second,
                nanosecond,
                this.zone.__str()];
    }

    function
    __repr (
        )

    /*++

    Routine Description:

        This routine returns a string of the date, as
            Time(y, m, d, h, m, s, ns, z).

    Arguments:

        None.

    Return Value:

        Returns the string representation.

    --*/

    {

        return "Time(%d, %d, %d, %d, %d, %d, %d, %s)" %
               [this.year,
                this.month,
                this.day,
                this.hour,
                this.minute,
                this.second,
                this.nanosecond,
                this.zone.__repr()];
    }

    function
    __add (
        right
        )


    /*++

    Routine Description:

        This routine adds a time delta to a time.

    Arguments:

        right - Supplies the time delta to add.

    Return Value:

        Returns a new time containing the time and time delta.

    --*/

    {

        var result;

        if (!(right is TimeDelta)) {
            Core.raise(TypeError("Expected a TimeDelta"));
        }

        result = Time(this.year,
                      this.month,
                      this.day,
                      this.hour,
                      this.minute,
                      this.second,
                      this.nanosecond,
                      this.zone);

        result._set("day", this.day + right.days);
        result._set("second", this.second + right.seconds);
        result._set("nanosecond", this.nanosecond + right.nanoseconds);
        result._normalize();
        return result;
    }

    function
    __sub (
        right
        )


    /*++

    Routine Description:

        This routine subtracts a time delta from this time.

    Arguments:

        right - Supplies the right side to add.

    Return Value:

        Returns a new time containing the adjusted time.

    --*/

    {

        var result;

        if (!(right is TimeDelta)) {
            Core.raise(TypeError("Expected a TimeDelta"));
        }

        result = Time(this.year,
                      this.month,
                      this.day,
                      this.hour,
                      this.minute,
                      this.second,
                      this.nanosecond,
                      this.zone);

        result._set("day", this.day - right.days);
        result._set("second", this.second - right.seconds);
        result._set("nanosecond", this.nanosecond - right.nanoseconds);
        result._normalize();
        return result;
    }

    function
    __eq (
        right
        )


    /*++

    Routine Description:

        This routine determines if two time deltas are equal.

    Arguments:

        right - Supplies the right side to compare against.

    Return Value:

        true if the values are equal.

        false if the values are not equal.

    --*/

    {

        if (!(right is Time)) {
            return false;
        }

        return (this.compare(right) == 0) && (this.zone == right.zone);
    }

    function
    __ne (
        right
        )


    /*++

    Routine Description:

        This routine determines if two time deltas are not equal.

    Arguments:

        right - Supplies the right side to compare against.

    Return Value:

        true if the values are not equal.

        false if the values are equal.

    --*/

    {

        return !this.__eq(right);
    }

    function
    _set (
        key,
        value
        )


    /*++

    Routine Description:

        This routine sets a key in the time instance. This routine should only
        be called internally by the Time class.

    Arguments:

        key - Supplies the key to set.

        value - Supplies the value to set.

    Return Value:

        None.

    --*/

    {

        return super.__set(key, value);
    }

    function
    _normalize (
        )

    /*++

    Routine Description:

        This routine normalizes the members of a Time instance.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var day;
        var daysInMonth;
        var leap;
        var month;
        var ordinal;
        var yearMonthDay;

        this._normalizePair("nanosecond", "second", 1000000000);
        this._normalizePair("second", "minute", 60);
        this._normalizePair("minute", "hour", 60);
        this._normalizePair("hour", "day", 24);
        super.month -= 1;
        this._normalizePair("month", "year", 12);
        month = this.month;
        leap = isLeapYear(this.year);
        daysInMonth = daysPerMonth[leap][month];
        day = this.day;
        if ((day < 1) || (day > daysInMonth)) {

            //
            // Adjusting by a day or so is easier than recomputing from scratch.
            //

            if (day == 0) {
                month -= 1;
                if (month >= 0) {
                    day = daysPerMonth[leap][month];

                } else {
                    super.year -= 1;
                    day = 31;
                    month = 11;
                }

            } else if (day == daysInMonth) {
                month += 1;
                day = 1;
                if (month >= 12) {
                    month = 0;
                    super.year += 1;
                }

            } else {
                ordinal = _dateToOrdinal(this.year, month + 1, 1) + day - 1;
                if ((ordinal < 1) || (ordinal > MAX_ORDINAL)) {
                    Core.raise(
                        TimeOverflowError("Ordinal %d out of range" % ordinal));
                }

                yearMonthDay = _ordinalToDate(ordinal);
                super.year = yearMonthDay[0];
                month = yearMonthDay[1] - 1;
                day = yearMonthDay[2];
            }
        }

        super.month = month + 1;
        super.day = day;
        return;
    }

    function
    _normalizePair (
        low,
        high,
        factor
        )

    /*++

    Routine Description:

        This routine normalizes two units.

    Arguments:

        low - Supplies the single unit member.

        high - Supplies the member representing factor * low.

        factor - Supplies how many lows go into a high.

    Return Value:

        None.

    --*/

    {

        var highCount;
        var lowValue = this.__get(low);

        if ((lowValue < 0) || (lowValue >= factor)) {
            highCount = lowValue / factor;
            lowValue = lowValue - (highCount * factor);
            if (lowValue < 0) {
                highCount -= 1;
                lowValue += factor;
            }

            highCount += this.__get(high);
            super.__set(low, lowValue);
            super.__set(high, highCount);
        }

        return;
    }
}

//
// The TimeZone class is an abstract class defining a time zone.
//

class TimeZone {
    function
    utcOffset (
        time
        )

    /*++

    Routine Description:

        This routine returns the offset from UTC to local time, in minutes
        east of UTC. Most implementations will probably return either a
        constant, or a constant plus this.dst(time).

    Arguments:

        time - Supplies an optional time.

    Return Value:

        Returns a TimeDelta containing the offset from UTC for the given date.

    --*/

    {

        Core.raise(TimeZoneNotImplemented("utcOffset not implemented"));
    }

    function
    dst (
        time
        )

    /*++

    Routine Description:

        This routine returns the daylight saving time adjustment for the given
        time.

    Arguments:

        time - Supplies the time.

    Return Value:

        Returns a TimeDelta containing the DST adjustment for the given time.

    --*/

    {

        Core.raise(TimeZoneNotImplemented("dst not implemented"));
    }

    function
    tzName (
        time
        )

    /*++

    Routine Description:

        This routine returns the name for the current time zone.

    Arguments:

        time - Supplies the time.

    Return Value:

        Returns an arbitrary string naming the time zone.

    --*/

    {

        Core.raise(TimeZoneNotImplemented("tzName not implemented"));
    }

    function
    fromUtc (
        time
        )

    /*++

    Routine Description:

        This routine adjusts a time specified in UTC time into local time.
        Most time zone implementations can accept the default implementation
        here.

    Arguments:

        time - Supplies the time.

    Return Value:

        Returns the adjusted time.

    --*/

    {

        var delta;
        var timeDst = time.dst();

        if (time.zone != this) {
            Core.raise(ValueError("Adjusting across different time zones"));
        }

        //
        // Get the standard offset.
        //

        delta = time.utcOffset() - timeDst;
        if (delta.totalSeconds() != 0) {
            time += delta;
            timeDst = time.dst();
        }

        if (timeDst.totalSeconds() != 0) {
            return time + timeDst;
        }

        return time;
    }

    function
    __str (
        )

    /*++

    Routine Description:

        This routine returns the string form of the given time zone. This
        implementation just returns this.tzName().

    Arguments:

        None.

    Return Value:

        Returns the time zone name.

    --*/

    {

        return this.tzName(null);
    }
}

//
// Default time zone class implementations
//

class FixedTimeZone is TimeZone {
    var _dst;
    var _offset;
    var _name;

    function
    __init (
        minutes,
        name
        )

    /*++

    Routine Description:

        This routine initializes a fixed time zone.

    Arguments:

        None.

    Return Value:

        Returns this.

    --*/

    {

        _offset = TimeDelta(0, minutes * 60, 0);
        _dst = TimeDelta(0, 0, 0);
        _name = name;
        return this;
    }

    function
    utcOffset (
        time
        )

    /*++

    Routine Description:

        This routine returns the offset from UTC to local time, in minutes
        east of UTC. Most implementations will probably return either a
        constant, or a constant plus this.dst(time).

    Arguments:

        time - Supplies an optional time.

    Return Value:

        Returns a TimeDelta containing the offset from UTC for the given date.

    --*/

    {

        return _offset;
    }

    function
    dst (
        time
        )

    /*++

    Routine Description:

        This routine returns the daylight saving time adjustment for the given
        time.

    Arguments:

        time - Supplies the time.

    Return Value:

        Returns a TimeDelta containing the DST adjustment for the given time.

    --*/

    {

        return _dst;
    }

    function
    tzName (
        time
        )

    /*++

    Routine Description:

        This routine returns the name for the current time zone.

    Arguments:

        time - Supplies the time.

    Return Value:

        Returns an arbitrary string naming the time zone.

    --*/

    {

        return _name;
    }
}

class UTC is FixedTimeZone {
    function
    __init (
        )

    /*++

    Routine Description:

        This routine initializes a UTC time zone instance.

    Arguments:

        None.

    Return Value:

        Returns this.

    --*/

    {

        return super.__init(0, "UTC");
    }

    function
    __repr (
        )

    /*++

    Routine Description:

        This routine returns the representation of the UTC time zone.

    Arguments:

        None.

    Return Value:

        Returns this.

    --*/

    {

        return "UTC()";
    }
}

class LocalTimeZone is TimeZone {
    var _std;
    var _dst;
    var _dstDifference;
    var _zero;

    function
    __init (
        )

    /*++

    Routine Description:

        This routine initializes a time zone instance that matches the platform.

    Arguments:

        None.

    Return Value:

        Returns this.

    --*/

    {

        var now;
        var tm;
        var utcTime;

        _zero = TimeDelta();
        _std = TimeDelta(0, -_time.timezone, 0);
        if (_time.daylight) {

            //
            // Find a time where daylight saving is set. Start with now.
            //

            now = (_time.time)();
            tm = (_time.localtime)(now);
            if (!tm.tm_isdst) {

                //
                // Try adding a month to the time until is_dst flips.
                //

                for (month in 0..12) {
                    now += 3600 * 24 * 30;
                    tm = (_time.localtime)(now);
                    if (tm.tm_isdst) {
                        break;
                    }
                }
            }

            //
            // If tm_isdst weirdly never flipped, then it's all constant.
            //

            if (!tm.tm_isdst) {
                _dst = _std;

            //
            // Create the same time in UTC and see how its timestamp differs
            // from the one that set isdst in local time.
            //

            } else {
                utcTime = Time(tm.tm_year + 1900,
                               tm.tm_mon + 1,
                               tm.tm_mday,
                               tm.tm_hour,
                               tm.tm_min,
                               tm.tm_sec,
                               0,
                               UTC());

                _dst = TimeDelta(0, utcTime.timestamp() - now, 0);
            }

        } else {
            _dst = _std;
        }

        _dstDifference = _dst - _std;
        return this;
    }

    function
    utcOffset (
        time
        )

    /*++

    Routine Description:

        This routine returns the offset from UTC to local time, in minutes
        east of UTC. Most implementations will probably return either a
        constant, or a constant plus this.dst(time).

    Arguments:

        time - Supplies an optional time.

    Return Value:

        Returns a TimeDelta containing the offset from UTC for the given date.

    --*/

    {

        if (this._isDst(time)) {
            return _dst;
        }

        return _std;
    }

    function
    dst (
        time
        )

    /*++

    Routine Description:

        This routine returns the daylight saving time adjustment for the given
        time.

    Arguments:

        time - Supplies the time.

    Return Value:

        Returns a TimeDelta containing the DST adjustment for the given time.

    --*/

    {

        if (this._isDst(time)) {
            return _dstDifference;
        }

        return _zero;
    }

    function
    tzName (
        time
        )

    /*++

    Routine Description:

        This routine returns the name for the current time zone.

    Arguments:

        time - Supplies the time.

    Return Value:

        Returns an arbitrary string naming the time zone.

    --*/

    {

        return _time.tzname[this._isDst(time)];
    }

    function
    _isDst (
        time
        )

    /*++

    Routine Description:

        This routine determines if the given time is in the platform's Daylight
        time or regular time.

    Arguments:

        time - Supplies the time.

    Return Value:

        false if the time is in standard time.

        true if the time is in daylight time.

    --*/

    {

        var localtm;
        var timestamp;
        var tm;

        if (time == null) {
            return false;
        }

        //
        // Manually create a tm dict because the real tm function calls
        // utcOffset and dst, which would cause infinite recursion.
        //

        tm = {
            "tm_sec": time.second,
            "tm_min": time.minute,
            "tm_hour": time.hour,
            "tm_mday": time.day,
            "tm_mon": time.month - 1,
            "tm_year": time.year - 1900,
            "tm_wday": (time.weekday() + 1) % 7,
        };

        timestamp = (_time.mktime)(tm);
        localtm = (_time.localtime)(timestamp);
        return localtm.tm_isdst;
    }

    function
    __repr (
        )

    /*++

    Routine Description:

        This routine returns the representation of the UTC time zone.

    Arguments:

        None.

    Return Value:

        Returns this.

    --*/

    {

        return "LocalTimeZone()";
    }
}

//
// --------------------------------------------------------- Internal Functions
//

function
_dateToOrdinal (
    year,
    month,
    day
    )

/*++

Routine Description:

    This routine converts a year, month, and date into an ordinal day (number
    of days since 1/1/0001).

Arguments:

    None.

Return Value:

    Returns the ordinal day of the year.

--*/

{

    return _daysBeforeYear(year) + _daysBeforeMonth(year, month) + day;
}

function
_ordinalToDate (
    ordinal
    )

/*++

Routine Description:

    This routine converts a number of days since January 1, 0001 into a year,
    month, and day.

Arguments:

    ordinal - Returns the number of days since January 1, 0001, pretending that
        the Gregorian calendar went back that far.

Return Value:

    Returns a list containing the [year, month, day] for the given ordinal.
    January is month 1.

--*/

{

    var cycle1;
    var cycle4;
    var cycle100;
    var cycle400;
    var leap;
    var month;
    var daysBefore;
    var result = [0, 0, 0];
    var value;

    //
    // Subtract 1 to get on the 400 year rotation.
    //

    ordinal -= 1;
    cycle400 = ordinal / DAYS_IN_400_YEARS;
    value = ordinal % DAYS_IN_400_YEARS;
    cycle100 = value / DAYS_IN_100_YEARS;
    value %= DAYS_IN_100_YEARS;
    cycle4 = value / DAYS_IN_4_YEARS;
    value %= DAYS_IN_4_YEARS;
    cycle1 = value / 365;
    value %= 365;
    result[0] = (cycle400 * 400) + 1 + (cycle100 * 100) + (cycle4 * 4) + cycle1;
    if ((cycle1 == 4) && (cycle100 == 4)) {
        result[0] -= 1;
        result[1] = 12;
        result[2] = 31;
        return result;
    }

    //
    // To figure out the month and day, take a guess that might be too large
    // and will never be too small), and back off if so.
    //

    leap = (cycle1 == 3) && ((cycle4 != 24) || (cycle100 == 3));
    month = ((value + 50) >> 5) - 1;
    daysBefore = monthDays[leap][month];
    if (daysBefore > value) {
        month -= 1;
        daysBefore = monthDays[leap][month];
    }

    result[1] = month + 1;
    result[2] = value - daysBefore + 1;
    return result;
}

function
_isoWeek1 (
    year
    )

/*++

Routine Description:

    This routine returns the ordinal number of the Monday starting week 1 of
    the given ISO year. The first ISO week of the year is the first week that
    contains a Thursday.

Arguments:

    year - Supplies the year.

Return Value:

    Returns the ordinal number of the start of the given ISO year.

--*/

{

    var firstDay = _dateToOrdinal(year, 1, 1);
    var firstMonday;
    var firstWeekday = (firstDay + 6) % 7;

    firstMonday = firstDay - firstWeekday;
    if (firstWeekday > 3) {
        firstMonday += 7;
    }

    return firstMonday;
}

function
_daysBeforeYear (
    year
    )

/*++

Routine Description:

    This routine returns the number of days between January 1, 0001, and
    January 1 of the given year (pretending the Gregorian calendar stretched
    that far back).

Arguments:

    year - Supplies the year to convert.

Return Value:

    Returns the number of days between January 1, 0001, and the start of the
    given year.

--*/

{

    //
    // Subtract 1 to get even with the 400 year cycle (which starts on year 1).
    //

    year -= 1;
    return (year * 365) + (year / 4) - (year / 100) + (year / 400);
}

function
_daysBeforeMonth (
    year,
    month
    )

/*++

Routine Description:

    This routine returns the number of days that occurred between the start of
    the year and the given month.

Arguments:

    year - Supplies the year to check, since it matters whether or not that
        year was a leap year.

    month - Supplies the month to check. January is 1.

Return Value:

    Returns the number of days between January 1st of the given year and the
    1st of the given month.

--*/

{

    return monthDays[isLeapYear(year)][month - 1];
}

//
// Instantiate the two main time zones.
//

utc = UTC();
localTime = LocalTimeZone();

