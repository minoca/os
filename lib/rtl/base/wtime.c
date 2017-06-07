/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wtime.c

Abstract:

    This module implements wide calendar time support functions.

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

RTL_API
UINTN
RtlFormatDateWide (
    PWSTR StringBuffer,
    ULONG StringBufferSize,
    PWSTR Format,
    PCALENDAR_TIME CalendarTime
    )

/*++

Routine Description:

    This routine converts the given calendar time into a wide string governed
    by the given wide format string.

Arguments:

    StringBuffer - Supplies a pointer where the converted wide string will be
        returned, including the terminating null.

    StringBufferSize - Supplies the size of the string buffer in characters.

    Format - Supplies the wide format string to govern the conversion. Ordinary
        characters in the format string will be copied verbatim to the output
        string. Conversions will be substituted for their corresponding value
        in the provided calendar time. The conversions are equivalent to the
        non-wide format date function.

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
    PWSTR SavedFormat;
    WCHAR Sign;
    WCHAR Specifier;
    KSTATUS Status;
    PWSTR String;
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

        if (*Format == L'\0') {
            if (SavedFormat != NULL) {
                Format = SavedFormat;
                SavedFormat = NULL;
                continue;

            } else {
                *String = L'\0';
                break;
            }
        }

        //
        // Handle ordinary characters in the format.
        //

        if (*Format != L'%') {
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

        if (*Format == L'E') {
            Format += 1;
        }

        if (*Format == L'O') {
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
        case L'a':
            if ((CalendarTime->Weekday >= TimeZoneWeekdaySunday) &&
                (CalendarTime->Weekday <= TimeZoneWeekdaySaturday)) {

                CopyString =
                           RtlAbbreviatedWeekdayStrings[CalendarTime->Weekday];

            } else {
                return 0;
            }

            break;

        case L'A':
            if ((CalendarTime->Weekday >= TimeZoneWeekdaySunday) &&
                (CalendarTime->Weekday <= TimeZoneWeekdaySaturday)) {

                CopyString = RtlWeekdayStrings[CalendarTime->Weekday];

            } else {
                return 0;
            }

            break;

        case L'b':
        case L'h':
            if ((CalendarTime->Month >= TimeZoneMonthJanuary) &&
                (CalendarTime->Month <= TimeZoneMonthDecember)) {

                CopyString = RtlAbbreviatedMonthStrings[CalendarTime->Month];

            } else {
                return 0;
            }

            break;

        case L'B':
            if ((CalendarTime->Month >= TimeZoneMonthJanuary) &&
                (CalendarTime->Month <= TimeZoneMonthDecember)) {

                CopyString = RtlMonthStrings[CalendarTime->Month];

            } else {
                return 0;
            }

            break;

        case L'c':
            SavedFormat = Format;
            Format = L"%a %b %e %H:%M:%S %Y";
            continue;

        case L'C':
            Integer = CalendarTime->Year / YEARS_PER_CENTURY;
            ZeroPad = TRUE;
            break;

        case L'd':
            Integer = CalendarTime->Day;
            ZeroPad = TRUE;
            break;

        case L'D':
            SavedFormat = Format;
            Format = L"%m/%d/%y";
            continue;

        case L'e':
            Integer = CalendarTime->Day;
            break;

        case L'F':
            SavedFormat = Format;
            Format = L"%Y-%m-%d";
            continue;

        case L'g':
        case L'G':
            Status = RtlpCalculateIsoWeekNumber(CalendarTime->Year,
                                                CalendarTime->YearDay,
                                                CalendarTime->Weekday,
                                                NULL,
                                                &IsoYear);

            if (!KSUCCESS(Status)) {
                CopyStringSize = 0;

            } else {
                if (Specifier == L'g') {
                    Integer = IsoYear % YEARS_PER_CENTURY;
                    DefaultFieldSize = 2;

                } else {
                    Integer = IsoYear;
                    DefaultFieldSize = 4;
                }

                ZeroPad = TRUE;
            }

            break;

        case L'H':
            Integer = CalendarTime->Hour;
            ZeroPad = TRUE;
            break;

        case L'I':
            Integer = Hour12;
            ZeroPad = TRUE;
            break;

        case L'J':
            Integer = CalendarTime->Nanosecond;
            ZeroPad = TRUE;
            DefaultFieldSize = 9;
            break;

        case L'j':
            Integer = CalendarTime->YearDay + 1;
            ZeroPad = TRUE;
            DefaultFieldSize = 3;
            break;

        case L'm':
            Integer = CalendarTime->Month + 1;
            ZeroPad = TRUE;
            break;

        case L'M':
            Integer = CalendarTime->Minute;
            ZeroPad = TRUE;
            break;

        case L'N':
            Integer = CalendarTime->Nanosecond;
            ZeroPad = TRUE;
            DefaultFieldSize = 9;
            break;

        case L'n':
            WorkingBuffer[0] = '\n';
            CopyString = WorkingBuffer;
            CopyStringSize = 1;
            break;

        case L'p':
            CopyString = RtlAmPmStrings[0][Evening];
            break;

        case L'P':
            CopyString = RtlAmPmStrings[1][Evening];
            break;

        case L'q':
            Integer = CalendarTime->Nanosecond / 1000000;
            ZeroPad = TRUE;
            DefaultFieldSize = 3;
            break;

        case L'r':
            SavedFormat = Format;
            Format = L"%I:%M:%S %p";
            continue;

        case L'R':
            SavedFormat = Format;
            Format = L"%H:%M";
            continue;

        case L's':
            Status = RtlCalendarTimeToSystemTime(CalendarTime, &SystemTime);
            if (!KSUCCESS(Status)) {
                Integer = 0;

            } else {
                Integer = SystemTime.Seconds + SYSTEM_TIME_TO_EPOCH_DELTA;
            }

            break;

        case L'S':
            Integer = CalendarTime->Second;
            ZeroPad = TRUE;
            break;

        case L't':
            WorkingBuffer[0] = '\t';
            CopyString = WorkingBuffer;
            CopyStringSize = 1;
            break;

        case L'T':
            SavedFormat = Format;
            Format = L"%H:%M:%S";
            continue;

        case L'u':
            Integer = CalendarTime->Weekday;
            if (Integer == TimeZoneWeekdaySunday) {
                Integer = DAYS_PER_WEEK;
            }

            DefaultFieldSize = 1;
            break;

        case L'U':
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

        case L'V':
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

        case L'w':
            Integer = CalendarTime->Weekday;
            DefaultFieldSize = 1;
            break;

        case L'W':
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

        case L'x':
            SavedFormat = Format;
            Format = L"%m/%d/%y";
            continue;

        case L'X':
            SavedFormat = Format;
            Format = L"%H:%M:%S";
            continue;

        case L'y':
            Integer = CalendarTime->Year % YEARS_PER_CENTURY;
            ZeroPad = TRUE;
            break;

        case L'Y':
            Integer = CalendarTime->Year;
            ZeroPad = TRUE;
            DefaultFieldSize = 4;
            break;

        case L'z':
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

        case L'Z':
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

        case L'%':
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
                                                  "%0*d",
                                                  DefaultFieldSize,
                                                  Integer);

            } else {
                CopyStringSize = RtlPrintToString(WorkingBuffer,
                                                  sizeof(WorkingBuffer),
                                                  CharacterEncodingDefault,
                                                  "%*d",
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
        StringBuffer[StringBufferSize - 1] = L'\0';
    }

    //
    // Figure out the number of bytes that were written, and returned.
    //

    StringSize = StringBufferSize - StringSize;
    return StringSize;
}

//
// --------------------------------------------------------- Internal Functions
//

