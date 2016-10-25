/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.c

Abstract:

    This module implements time-based support routines for UEFI runtime
    services.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_EPOCH_YEAR 2001

#define EFI_MIN_YEAR 1
#define EFI_MAX_YEAR 9999

#define EFI_MIN_DATE (INT64)(-63113904000LL)
#define EFI_MAX_DATE (INT64)(252423993599LL)

#define EFI_NANOSECONDS_PER_SECOND 1000000000ULL

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipNormalizeTime (
    EFI_TIME *EfiTime
    );

INT32
EfipComputeYearForDays (
    INT32 *Days
    );

INT32
EfipComputeDaysForYear (
    INT32 Year
    );

//
// -------------------------------------------------------------------- Globals
//

UINT8 EfiDaysPerMonth[2][MONTHS_PER_YEAR] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

UINT16 EfiMonthDays[2][MONTHS_PER_YEAR] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiConvertCounterToEfiTime (
    INT64 Counter,
    EFI_TIME *EfiTime
    )

/*++

Routine Description:

    This routine converts from a second-based counter value to an EFI time
    structure.

Arguments:

    Counter - Supplies the count of seconds since January 1, 2001 GMT.

    EfiTime - Supplies a pointer where the EFI time fields will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the counter could not be converted.

--*/

{

    INT32 Day;
    INT32 Hour;
    INT Leap;
    INT32 Minute;
    INT32 Month;
    INT64 RawDays;
    INT32 Second;
    INT32 Year;

    if ((Counter > EFI_MAX_DATE) ||
        (Counter < EFI_MIN_DATE)) {

        return EFI_INVALID_PARAMETER;
    }

    EfiDivide64(Counter, SECONDS_PER_DAY, &RawDays, NULL);
    Counter -= RawDays * SECONDS_PER_DAY;
    if (Counter < 0) {
        Counter += SECONDS_PER_DAY;
        RawDays -= 1;
    }

    Year = EfipComputeYearForDays((INT32 *)&RawDays);
    Leap = 0;
    if (IS_LEAP_YEAR(Year)) {
        Leap = 1;
    }

    //
    // Subtract off the months.
    //

    Month = 0;
    Day = RawDays;
    while (Day >= EfiDaysPerMonth[Leap][Month]) {
        Day -= EfiDaysPerMonth[Leap][Month];
        Month += 1;
    }

    //
    // Days of the month start with 1.
    //

    Day += 1;

    //
    // Figure out the time of day.
    //

    Second = (LONG)Counter;
    Hour = Second / SECONDS_PER_HOUR;
    Second -= Hour * SECONDS_PER_HOUR;
    Minute = Second / SECONDS_PER_MINUTE;
    Second -= Minute * SECONDS_PER_MINUTE;

    //
    // Fill in the structure.
    //

    EfiTime->Year = Year;
    EfiTime->Month = Month + 1;
    EfiTime->Day = Day;
    EfiTime->Hour = Hour;
    EfiTime->Minute = Minute;
    EfiTime->Second = Second;
    EfiTime->Nanosecond = 0;
    return EFI_SUCCESS;
}

EFI_STATUS
EfiConvertEfiTimeToCounter (
    EFI_TIME *EfiTime,
    INT64 *Counter
    )

/*++

Routine Description:

    This routine converts from an EFI time structure into the number of seconds
    since January 1, 2001 GMT.

Arguments:

    EfiTime - Supplies a pointer to the EFI time to convert.

    Counter - Supplies a pointer where the count of seconds will be returned
        on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the counter could not be converted.

--*/

{

    INT64 Days;

    EfipNormalizeTime(EfiTime);
    if ((EfiTime->Year > EFI_MAX_YEAR) ||
        (EfiTime->Year < EFI_MIN_YEAR) ||
        (EfiTime->Month < 1) || (EfiTime->Month > 11) ||
        (EfiTime->Day < 1) || (EfiTime->Day > 31)) {

        return EFI_INVALID_PARAMETER;
    }

    Days = EfipComputeDaysForYear(EfiTime->Year);
    Days += EfiMonthDays[IS_LEAP_YEAR(EfiTime->Year)][EfiTime->Month - 1];
    Days += EfiTime->Day - 1;
    *Counter = ((LONGLONG)Days * SECONDS_PER_DAY) +
               (EfiTime->Hour * SECONDS_PER_HOUR) +
               (EfiTime->Minute * SECONDS_PER_MINUTE) +
               EfiTime->Second;

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipNormalizeTime (
    EFI_TIME *EfiTime
    )

/*++

Routine Description:

    This routine normalizes the fields in a calendar time structure, putting
    them in their proper ranges.

Arguments:

    EfiTime - Supplies a pointer to the calendar time structure to
        normalize.

Return Value:

    None.

--*/

{

    INT32 Day;
    INT32 Leap;

    //
    // Get nanoseconds, seconds, minutes, and hours into range.
    //

    if (EfiTime->Nanosecond >= EFI_NANOSECONDS_PER_SECOND) {
        EfiTime->Second += EfiTime->Nanosecond / EFI_NANOSECONDS_PER_SECOND;
        EfiTime->Nanosecond %= EFI_NANOSECONDS_PER_SECOND;
    }

    if (EfiTime->Second >= SECONDS_PER_MINUTE) {
        EfiTime->Minute += EfiTime->Second / SECONDS_PER_MINUTE;
        EfiTime->Second %= SECONDS_PER_MINUTE;
    }

    if (EfiTime->Minute >= MINUTES_PER_HOUR) {
        EfiTime->Hour += EfiTime->Minute / MINUTES_PER_HOUR;
        EfiTime->Minute %= MINUTES_PER_HOUR;
    }

    Day = 0;
    if (EfiTime->Hour >= HOURS_PER_DAY) {
        Day = EfiTime->Hour / HOURS_PER_DAY;
        EfiTime->Hour %= HOURS_PER_DAY;
    }

    //
    // Skip the days for now as they're tricky. Get the month into range.
    //

    EfiTime->Month -= 1;
    if (EfiTime->Month > MONTHS_PER_YEAR) {
        EfiTime->Year += EfiTime->Month / MONTHS_PER_YEAR;
        EfiTime->Month %= MONTHS_PER_YEAR;
    }

    EfiTime->Month += 1;

    //
    // Make the day positive.
    //

    Day += EfiTime->Day - 1;
    while (Day < 0) {
        EfiTime->Month -= 1;
        if (EfiTime->Month == 0) {
            EfiTime->Year -= 1;
            EfiTime->Month = MONTHS_PER_YEAR;
        }

        Leap = 0;
        if (IS_LEAP_YEAR(EfiTime->Year)) {
            Leap = 1;
        }

        Day += EfiDaysPerMonth[Leap][EfiTime->Month - 1];
    }

    //
    // Now get the day in range.
    //

    while (TRUE) {
        Leap = 0;
        if (IS_LEAP_YEAR(EfiTime->Year)) {
            Leap = 1;
        }

        if (Day < EfiDaysPerMonth[Leap][EfiTime->Month - 1]) {
            break;
        }

        Day -= EfiDaysPerMonth[Leap][EfiTime->Month - 1];
        EfiTime->Month += 1;
        if (EfiTime->Month > MONTHS_PER_YEAR) {
            EfiTime->Year += 1;
            EfiTime->Month = 1;
        }
    }

    EfiTime->Day = Day + 1;
    return;
}

INT32
EfipComputeYearForDays (
    INT32 *Days
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

    INT32 RemainingDays;
    INT32 Year;

    Year = EFI_EPOCH_YEAR;
    RemainingDays = *Days;

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

INT32
EfipComputeDaysForYear (
    INT32 Year
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

    INT32 Days;

    Days = 0;
    if (Year >= EFI_EPOCH_YEAR) {
        while (Year > EFI_EPOCH_YEAR) {
            Year -= 1;
            if (IS_LEAP_YEAR(Year)) {
                Days += DAYS_PER_LEAP_YEAR;

            } else {
                Days += DAYS_PER_YEAR;
            }
        }

    } else {
        while (Year < EFI_EPOCH_YEAR) {
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

