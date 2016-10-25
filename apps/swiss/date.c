/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    date.c

Abstract:

    This module implements the date utility, which prints or sets the current
    system time.

Author:

    Evan Green 8-Oct-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define DATE_VERSION_MAJOR 1
#define DATE_VERSION_MINOR 0

#define DATE_USAGE                                                             \
    "usage: date [-u] [+format]\n"                                             \
    "       date [-u] mmddHHMM[[cc]yy]\n"                                      \
    "The date utility prints the current date and time with no arguments.\n"   \
    "Valid options are:\n"                                                     \
    "  -u -- Perform time operations in UTC (GMT time).\n"                     \
    "  +format -- Print the date according to the specified format. The \n"    \
    "        format is the same as that given to the strftime function:\n"     \
    "        %a - Abbreviated weekday.\n"                                      \
    "        %A - Full weekday.\n"                                             \
    "        %b - Abbreviated month.\n"                                        \
    "        %B - Full month.\n"                                               \
    "        %c - Locale's appropriate date and time representation.\n"        \
    "        %C - Century (year divided by 100 and truncated to an integer).\n"\
    "        %d - Day of the month [01,31].\n"                                 \
    "        %D - Date in the format mm/dd/yy.\n"                              \
    "        %e - Day of the month in a two digit field with a leading space " \
    "fill [1,31].\n"                                                           \
    "        %h - Same as %b.\n"                                               \
    "        %H - Hour (24-hour clock) [00,23].\n"                             \
    "        %I - Hour (12-hour clock) [01,12].\n"                             \
    "        %j - Day of the year [001,366].\n"                                \
    "        %m - Month [01,12].\n"                                            \
    "        %M - Minute [00,59].\n"                                           \
    "        %n - A newline.\n"                                                \
    "        %N - Nanoseconds [000000000,999999999].\n"                        \
    "        %p - Locale's equivalent of AM or PM.\n"                          \
    "        %r - 12-hour clock time [01,12] using the AM/PM notation. \n"     \
    "             In POSIX, this is \"%I:%M:%S %p\".\n"                        \
    "        %s - Seconds since 1970 GMT.\n"                                   \
    "        %S - Seconds [00,60].\n"                                          \
    "        %t - A tab.\n"                                                    \
    "        %T - 24-hour time in the format \"HH:MM:SS\".\n"                  \
    "        %u - Weekday as a number [1,7] (1=Monday).\n"                     \
    "        %U - Week of the year (Sunday as the first day of the week) \n"   \
    "             [0,53]. All days in a year preceding the first sunday \n"    \
    "             are in week 0.\n"                                            \
    "        %V - Week of the year (Monday as the first day of the week) \n"   \
    "             [01,53]. If the week containing January 1 has four or \n"    \
    "             more days, it is week 1. Otherwise it is the last week \n"   \
    "             of the previous year.\n"                                     \
    "        %w - Weekday as a decimal [0,6] (0=Sunday).\n"                    \
    "        %W - Week of the year (Monday as the first day of the week) \n"   \
    "             [00,53]. All days preceding the first Monday are week 0.\n"  \
    "        %x - Locale's appropriate date representation.\n"                 \
    "        %X - Locale's appropriate time representation.\n"                 \
    "        %y - Year within century [00,99].\n"                              \
    "        %Y - Year with century.\n"                                        \
    "        %Z - Timezone name or nothing if no timezone is available.\n"     \
    "        %% - A percent sign character.\n\n"

#define DATE_OPTIONS_STRING "u"

#define DATE_OPTION_UTC 0x00000001

//
// Define a default argument if none is provided.
//

#define DATE_DEFAULT_ARGUMENT "+%a %b %d %H:%M:%S %Z %Y"

//
// Define the output format for set time.
//

#define DATE_SET_TIME_FORMAT "%a %b %d %H:%M:%S %Z %Y"

//
// Define the size of the buffer used for the strftime function.
//

#define DATE_TIME_FORMAT_SIZE 2048

//
// This value defines the two digit year beginning in which the 21st century is
// assumed.
//

#define TWO_DIGIT_YEAR_CUTOFF 70

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DateParseComponent (
    PSTR *String
    );

//
// -------------------------------------------------------------------- Globals
//

struct option DateLongOptions[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
DateMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the date utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    INT Option;
    ULONG Options;
    struct tm SetTime;
    int Status;
    time_t Time;
    PCHAR TimeBuffer;
    struct tm *TimeComponents;
    struct timeval TimeValue;
    int Year;

    Options = 0;
    TimeBuffer = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             DATE_OPTIONS_STRING,
                             DateLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'u':
            Options |= DATE_OPTION_UTC;
            break;

        case 'V':
            SwPrintVersion(DATE_VERSION_MAJOR, DATE_VERSION_MINOR);
            return 1;

        case 'h':
            puts(DATE_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    Argument = DATE_DEFAULT_ARGUMENT;
    if (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
    }

    //
    // Fail if there are too many arguments.
    //

    if (ArgumentIndex + 1 < ArgumentCount) {
        SwPrintError(0, Arguments[ArgumentIndex + 1], "Too many arguments");
        return 1;
    }

    Time = time(NULL);
    TimeBuffer = malloc(DATE_TIME_FORMAT_SIZE);
    if (TimeBuffer == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    //
    // If the first character of the argument is a +, then print the current
    // date in the specified format.
    //

    if (*Argument == '+') {
        if ((Options & DATE_OPTION_UTC) != 0) {
            TimeComponents = gmtime(&Time);

        } else {
            TimeComponents = localtime(&Time);
        }

        if (TimeComponents == NULL) {
            Status = errno;
            SwPrintError(Status, NULL, "Failed to get current time");
            goto MainEnd;
        }

        strftime(TimeBuffer,
                 DATE_TIME_FORMAT_SIZE,
                 Argument + 1,
                 TimeComponents);

        printf("%s\n", TimeBuffer);
        Status = 0;
        goto MainEnd;
    }

    memset(&SetTime, 0, sizeof(struct tm));

    //
    // Parse the string of the form mmddhhmm[[cc]yy].
    //

    Status = EINVAL;
    SetTime.tm_mon = DateParseComponent(&Argument);
    if (SetTime.tm_mon < 0) {
        SwPrintError(0, Argument, "Failed to parse month");
        goto MainEnd;
    }

    SetTime.tm_mon -= 1;
    if ((SetTime.tm_mon < 0) || (SetTime.tm_mon > 11)) {
        SwPrintError(0, Arguments[ArgumentIndex], "Invalid date");
        goto MainEnd;
    }

    SetTime.tm_mday = DateParseComponent(&Argument);
    if (SetTime.tm_mday < 0) {
        SwPrintError(0, Argument, "Failed to parse day");
        goto MainEnd;
    }

    if ((SetTime.tm_mday < 1) || (SetTime.tm_mday > 31)) {
        SwPrintError(0, Arguments[ArgumentIndex], "Invalid date");
        goto MainEnd;
    }

    SetTime.tm_hour = DateParseComponent(&Argument);
    if (SetTime.tm_hour < 0) {
        SwPrintError(0, Argument, "Failed to parse hour");
        goto MainEnd;
    }

    if ((SetTime.tm_hour < 0) || (SetTime.tm_hour > 23)) {
        SwPrintError(0, Arguments[ArgumentIndex], "Invalid date");
        goto MainEnd;
    }

    SetTime.tm_min = DateParseComponent(&Argument);
    if (SetTime.tm_min < 0) {
        SwPrintError(0, Argument, "Failed to parse minute");
        goto MainEnd;
    }

    if ((SetTime.tm_hour < 0) || (SetTime.tm_hour > 59)) {
        SwPrintError(0, Arguments[ArgumentIndex], "Invalid date");
        goto MainEnd;
    }

    if (*Argument != '\0') {
        SetTime.tm_year = DateParseComponent(&Argument);
        if (SetTime.tm_year < 0) {
            SwPrintError(0, Argument, "Failed to parse year/century");
            goto MainEnd;
        }

        if (*Argument != '\0') {
            Year = DateParseComponent(&Argument);
            if (Year < 0) {
                SwPrintError(0, Argument, "Failed to parse year");
                goto MainEnd;
            }

            SetTime.tm_year *= 100;
            SetTime.tm_year += Year;
            SetTime.tm_year -= 1900;

        //
        // If no century is supplied, then assume the user meant somewhere in
        // the late 1900's or early 2000's with the cutoff at 1970.
        //

        } else {
            if (SetTime.tm_year < TWO_DIGIT_YEAR_CUTOFF) {
                SetTime.tm_year += 100;
            }
        }

    //
    // Use the current year if no year was supplied.
    //

    } else {
        if ((Options & DATE_OPTION_UTC) != 0) {
            TimeComponents = gmtime(&Time);

        } else {
            TimeComponents = localtime(&Time);
        }

        if (TimeComponents == NULL) {
            Status = errno;
            SwPrintError(Status, NULL, "Failed to set time");
            goto MainEnd;
        }

        SetTime.tm_year = TimeComponents->tm_year;
    }

    if (*Argument != '\0') {
        SwPrintError(0, Argument, "Unexpected garbage at end of line");
        goto MainEnd;
    }

    //
    // The daylight savings of the supplied time are unknown.
    //

    SetTime.tm_isdst = -1;

    //
    // Interpret the parsed calendar time depending on the UTC option. If this
    // fails it will return -1 with errno set. Note that -1 without errno set
    // could be the result of a time that is 1 second before the Epoch.
    //

    if ((Options & DATE_OPTION_UTC) != 0) {
        Time = SwConvertGmtTime(&SetTime);

    } else {
        Time = mktime(&SetTime);
    }

    if ((Time == -1) && (errno != 0)) {
        Status = errno;
        SwPrintError(Status, Arguments[ArgumentIndex], "Invalid date");
        goto MainEnd;
    }

    TimeValue.tv_sec = Time;
    TimeValue.tv_usec = 0;
    Status = SwSetTimeOfDay(&TimeValue, NULL);
    if (Status != 0) {
        Status = errno;
        SwPrintError(Status, NULL, "Failed to set time");
        goto MainEnd;
    }

    //
    // Now print the updated time. The mktime and timegm routines modified the
    // parsed calendar time so that it is exactly what needs to be printed.
    //

    strftime(TimeBuffer,
             DATE_TIME_FORMAT_SIZE,
             DATE_SET_TIME_FORMAT,
             &SetTime);

    printf("%s\n", TimeBuffer);

MainEnd:
    if (TimeBuffer != NULL) {
        free(TimeBuffer);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DateParseComponent (
    PSTR *String
    )

/*++

Routine Description:

    This routine pulls the next two digits out of the string and returns their
    decimal numerical representation.

Arguments:

    String - Supplies a pointer that on input contains a pointer to a string.
        On output this value will be advanced.

Return Value:

    Returns the integer on success.

    -1 on failure.

--*/

{

    PSTR Argument;
    INT Value;

    Argument = *String;
    if ((!isdigit(*Argument)) || (!isdigit(*(Argument + 1)))) {
        return -1;
    }

    Value = ((*Argument - '0') * 10) + (*(Argument + 1) - '0');
    Argument += 2;
    *String = Argument;
    return Value;
}

