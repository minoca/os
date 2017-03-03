/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    touch.c

Abstract:

    This module implements support for the "touch" utility.

Author:

    Evan Green 16-Aug-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the year after which it's assumed the century is 1900.
//

#define TWO_DIGIT_YEAR_CUTOFF 70

#define TOUCH_VERSION_MAJOR 1
#define TOUCH_VERSION_MINOR 0

#define TOUCH_USAGE                                                            \
    "usage: touch [-acm][-r reference_file | -t time] file...\n\n"             \
    "The touch utility shall change the modification time, access time, or \n" \
    "both of a file. It can also be used to create new files. If neither \n"   \
    "-a nor -m is specified, touch behaves as if both are specified. \n"       \
    "Options are:\n"                                                           \
    "  -a -- Change the access time of a file.\n"                              \
    "  -c, --no-create -- Do not create the file if it does not exist.\n"      \
    "  -m -- Change the modification time of the file.\n"                      \
    "  -r, --reference <reference file> -- Use the corresponding time of \n"   \
    "        the given reference file instead of the current time.\n"          \
    "  -t, --time <time> -- Use the specified time instead of the current \n"  \
    "        time. The time option shall be a decimal number of the form:\n"   \
    "        [[CC]YY]MMDDhhmm[.SS]. If the century is not given but the \n"    \
    "        year is, then years >70 are in the 1900s.\n"                      \
    "  --help -- Display this help text and exit.\n"                           \
    "  --version -- Display the version number and exit.\n\n"

#define TOUCH_OPTIONS_STRING "acmr:t:"

//
// Define touch options.
//

//
// Set this flag to change the access time.
//

#define TOUCH_OPTION_ACCESS_TIME 0x00000001

//
// Set this flag to change the modification time.
//

#define TOUCH_OPTION_MODIFICATION_TIME 0x00000002

//
// Set this flag to avoid creating the file if it doesn't exist.
//

#define TOUCH_OPTION_NO_CREATE 0x00000004

//
// Define the permissions used for files created by touch.
//

#define TOUCH_CREATE_PERMISSIONS \
    (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IWOTH)

//
// Define the length of the touch date strings, which follows the format
// [CC]YYMMDDhhmm when ignoring the [.SS] on the end.
//

#define TOUCH_DATE_BASE_LENGTH 10
#define TOUCH_DATE_FULL_YEAR_LENGTH 12

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
TouchParseTimeString (
    PSTR TimeString,
    time_t *Time
    );

//
// -------------------------------------------------------------------- Globals
//

struct option TouchLongOptions[] = {
    {"no-create", no_argument, 0, 'c'},
    {"reference", required_argument, 0, 'r'},
    {"time", required_argument, 0, 't'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
TouchMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the touch utility.

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
    PSTR FirstSource;
    time_t NewAccessTime;
    time_t NewModificationTime;
    struct utimbuf NewTimes;
    struct utimbuf *NewTimesPointer;
    int OpenDescriptor;
    INT Option;
    ULONG Options;
    PSTR ReferenceFile;
    struct stat Stat;
    int Status;
    PSTR TimeString;
    int TotalStatus;

    FirstSource = NULL;
    NewTimesPointer = &NewTimes;
    Options = 0;
    ReferenceFile = NULL;
    Status = 0;
    TimeString = NULL;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TOUCH_OPTIONS_STRING,
                             TouchLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            return Status;
        }

        switch (Option) {
        case 'a':
            Options |= TOUCH_OPTION_ACCESS_TIME;
            break;

        case 'c':
            Options |= TOUCH_OPTION_NO_CREATE;
            break;

        case 'm':
            Options |= TOUCH_OPTION_MODIFICATION_TIME;
            break;

        case 'r':
            ReferenceFile = optarg;

            assert(ReferenceFile != NULL);

            break;

        case 't':
            TimeString = optarg;

            assert(TimeString != NULL);

            break;

        case 'V':
            SwPrintVersion(TOUCH_VERSION_MAJOR, TOUCH_VERSION_MINOR);
            return 1;

        case 'h':
            printf(TOUCH_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            return Status;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        FirstSource = Arguments[ArgumentIndex];
    }

    //
    // Fail if there's nothing to touch.
    //

    if (FirstSource == NULL) {
        SwPrintError(0, NULL, "Argument expected. Try --help for usage");
        return 1;
    }

    //
    // If neither access nor modification time was specified, have a can-do
    // attitude and do both.
    //

    if ((Options &
         (TOUCH_OPTION_ACCESS_TIME | TOUCH_OPTION_MODIFICATION_TIME)) == 0) {

        Options |= TOUCH_OPTION_ACCESS_TIME | TOUCH_OPTION_MODIFICATION_TIME;
    }

    //
    // If there was a reference file, use that to get the times.
    //

    if (ReferenceFile != NULL) {
        Status = SwStat(ReferenceFile, TRUE, &Stat);
        if (Status != 0) {
            SwPrintError(Status,
                         ReferenceFile,
                         "Unable to stat reference file");

            return Status;
        }

        NewAccessTime = Stat.st_atime;
        NewModificationTime = Stat.st_mtime;

    //
    // If a time was specified, parse that out.
    //

    } else if (TimeString != NULL) {
        Status = TouchParseTimeString(TimeString, &NewAccessTime);
        if (Status != 0) {
            SwPrintError(Status, TimeString, "Unable to parse time");
            return Status;
        }

        NewModificationTime = NewAccessTime;

    //
    // If nothing was specified, use the current time. Set the pointer to
    // NULL to allow the system to potentially use a granularity better than
    // seconds. Otherwise the nanoseconds get truncated to zero, which can
    // create strange ordering issues.
    //

    } else {
        NewAccessTime = time(NULL);
        NewModificationTime = NewAccessTime;
        if ((Options &
             (TOUCH_OPTION_ACCESS_TIME | TOUCH_OPTION_MODIFICATION_TIME)) ==
            (TOUCH_OPTION_ACCESS_TIME | TOUCH_OPTION_MODIFICATION_TIME)) {

            NewTimesPointer = NULL;
        }
    }

    //
    // Loop through the arguments again and perform the touching.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        OpenDescriptor = -1;

        //
        // Stat the file. If that was unsuccessful, attempt to create it.
        //

        Status = SwStat(Argument, TRUE, &Stat);
        if (Status != 0) {
            if ((Options & TOUCH_OPTION_NO_CREATE) != 0) {
                continue;
            }

            OpenDescriptor = creat(Argument, TOUCH_CREATE_PERMISSIONS);
            if (OpenDescriptor < 0) {
                TotalStatus = errno;
                SwPrintError(TotalStatus, Argument, "Cannot create");
            }

            Status = SwStat(Argument, TRUE, &Stat);
            if (OpenDescriptor >= 0) {
                close(OpenDescriptor);
                OpenDescriptor = -1;
            }

            if (Status != 0) {
                if (TotalStatus == 0) {
                    TotalStatus = Status;
                }

                SwPrintError(Status, Argument, "Cannot stat");
                continue;
            }
        }

        //
        // To get here the stat must have succeeded. Set the new file times to
        // the current file times, and then update the desired ones.
        //

        assert(Status == 0);

        NewTimes.actime = Stat.st_atime;
        NewTimes.modtime = Stat.st_mtime;
        if ((Options & TOUCH_OPTION_ACCESS_TIME) != 0) {
            NewTimes.actime = NewAccessTime;
        }

        if ((Options & TOUCH_OPTION_MODIFICATION_TIME) != 0) {
            NewTimes.modtime = NewModificationTime;
        }

        Status = utime(Argument, NewTimesPointer);
        if (Status != 0) {
            Status = errno;
            if (TotalStatus == 0) {
                TotalStatus = Status;
            }

            SwPrintError(Status, Argument, "Failed to touch");
        }
    }

    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
TouchParseTimeString (
    PSTR TimeString,
    time_t *Time
    )

/*++

Routine Description:

    This routine parses a touch command line timestamp in the form
    [[CC]YY]MMDDhhmm[.SS].

Arguments:

    TimeString - Supplies a pointer to the time string to parse.

    Time - Supplies a pointer where the time will be returned.

Return Value:

    0 on success.

    EINVAL if the format was invalid.

--*/

{

    PSTR AfterScan;
    size_t BaseLength;
    struct tm Fields;
    PSTR Period;
    CHAR Portion[5];
    LONG Value;

    memset(&Fields, 0, sizeof(Fields));
    Period = strchr(TimeString, '.');
    if (Period != NULL) {
        BaseLength = (UINTN)Period - (UINTN)TimeString;

    } else {
        BaseLength = strlen(TimeString);
    }

    if ((BaseLength != TOUCH_DATE_BASE_LENGTH) &&
        (BaseLength != TOUCH_DATE_FULL_YEAR_LENGTH)) {

        return EINVAL;
    }

    //
    // Grab the year, either with or without the century.
    //

    if (BaseLength == TOUCH_DATE_FULL_YEAR_LENGTH) {
        memcpy(Portion, TimeString, 4);
        Portion[4] = '\0';
        Value = strtol(Portion, &AfterScan, 10);
        if ((Value < 0) || (AfterScan == Portion)) {
            return EINVAL;
        }

        Fields.tm_year = Value - 1900;
        TimeString += 4;

    } else {
        memcpy(Portion, TimeString, 2);
        Portion[2] = '\0';
        Value = strtol(Portion, &AfterScan, 10);
        if ((Value < 0) || (AfterScan == Portion)) {
            return EINVAL;
        }

        //
        // Below a certain value is assumed to be in the 2000s, otherwise it
        // must be in the 1900s. Way down the line this will have to change.
        //

        if (Value < TWO_DIGIT_YEAR_CUTOFF) {
            Fields.tm_year = 100 + Value;

        } else {
            Fields.tm_year = Value;
        }

        TimeString += 2;
    }

    //
    // Scan the month.
    //

    memcpy(Portion, TimeString, 2);
    Portion[2] = '\0';
    TimeString += 2;
    Value = strtol(Portion, &AfterScan, 10);
    if ((Value <= 0) || (AfterScan == Portion)) {
        return EINVAL;
    }

    Fields.tm_mon = Value - 1;

    //
    // Scan the day of the month.
    //

    memcpy(Portion, TimeString, 2);
    Portion[2] = '\0';
    TimeString += 2;
    Value = strtol(Portion, &AfterScan, 10);
    if ((Value <= 0) || (AfterScan == Portion)) {
        return EINVAL;
    }

    Fields.tm_mday = Value;

    //
    // Scan the hour.
    //

    memcpy(Portion, TimeString, 2);
    Portion[2] = '\0';
    TimeString += 2;
    Value = strtol(Portion, &AfterScan, 10);
    if ((Value < 0) || (AfterScan == Portion)) {
        return EINVAL;
    }

    Fields.tm_hour = Value;

    //
    // Scan the minute.
    //

    memcpy(Portion, TimeString, 2);
    Portion[2] = '\0';
    TimeString += 2;
    Value = strtol(Portion, &AfterScan, 10);
    if ((Value < 0) || (AfterScan == Portion)) {
        return EINVAL;
    }

    Fields.tm_min = Value;

    //
    // If there was a period, scan the second as well.
    //

    if (Period != NULL) {
        Period += 1;
        Value = strtol(Period, &AfterScan, 10);
        if ((Value < 0) || (AfterScan == Period)) {
            return EINVAL;
        }

        Fields.tm_sec = Value;
    }

    //
    // Convert the time fields to a time value.
    //

    *Time = mktime(&Fields);
    if (*Time == -1) {
        return errno;
    }

    return 0;
}

