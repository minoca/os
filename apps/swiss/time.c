/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    time.c

Abstract:

    This module implements the time utility.

Author:

    Chris Stevens 29-Oct-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TIME_VERSION_MAJOR 1
#define TIME_VERSION_MINOR 0

#define TIME_USAGE                                                             \
    "usage: time [-p] utility [arguments...]\n"                                \
    "The time utility invokes the specified utility with any associated\n"     \
    "arguments and writes time statistics for the utility to standard\n"       \
    "error.Options are:\n"                                                     \
    "  -p, --portability -- Writes the time statistics to standard error in\n" \
    "      the POSIX compliant format:\n"                                      \
    "\n"                                                                       \
    "      real %%f\n"                                                         \
    "      user %%f\n"                                                         \
    "      sys %%f\n"                                                          \
    "\n"                                                                       \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define TIME_OPTIONS_STRING "+phV"
#define TIME_OPTION_USE_PORTABILITY_FORMAT 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
TimepExecuteUtility (
    INT ArgumentCount,
    PSTR *Arguments,
    ULONG Options
    );

VOID
TimepSubtractTimeval (
    struct timeval *Timeval1,
    struct timeval *Timeval2,
    struct timeval *Result
    );

//
// -------------------------------------------------------------------- Globals
//

struct option TimeLongOptions[] = {
    {"portability", no_argument, 0, 'p'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
TimeMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the time utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    INT Option;
    ULONG Options;
    INT Status;

    Options = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TIME_OPTIONS_STRING,
                             TimeLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'p':
            Options |= TIME_OPTION_USE_PORTABILITY_FORMAT;
            break;

        case 'V':
            SwPrintVersion(TIME_VERSION_MAJOR, TIME_VERSION_MINOR);
            return 1;

        case 'h':
            printf(TIME_USAGE);
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

    //
    // Execute the utility stored in the remaining arguments and print the time
    // statistics based on the collected options.
    //

    Status = TimepExecuteUtility((ArgumentCount - ArgumentIndex),
                                 &(Arguments[ArgumentIndex]),
                                 Options);

    if (Status != 0) {
        goto MainEnd;
    }

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
TimepExecuteUtility (
    INT ArgumentCount,
    PSTR *Arguments,
    ULONG Options
    )

/*++

Routine Description:

    This routine executes the given utility, the first argument, and prints the
    timing statistics for the utility's execution to standard error.

Arguments:

    ArgumentCount - Supplies the number of arguments for the utility, including
        the utility itself.

    Arguments - Supplies an array of arguments for the utility where the first
        argument is the name of the utility.

    Options - Supplies a bitmask of options that apply to the time statistics.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    struct timeval RealEndTime;
    struct timeval RealStartTime;
    struct timeval RealTime;
    INT ReturnValue;
    INT Status;
    struct timeval SystemEndTime;
    struct timeval SystemStartTime;
    struct timeval SystemTime;
    struct timeval UserEndTime;
    struct timeval UserStartTime;
    struct timeval UserTime;

    //
    // Get the current real, user, and system times for the start of execution.
    //

    Status = SwGetTimes(&RealStartTime, &UserStartTime, &SystemStartTime);
    if (Status != 0) {
        goto ExecuteUtilityEnd;
    }

    //
    // Run the utility if it exists.
    //

    ReturnValue = 0;
    if (ArgumentCount != 0) {
        Status = SwRunCommand(Arguments[0],
                              Arguments,
                              ArgumentCount,
                              0,
                              &ReturnValue);

        if (Status != 0) {
            SwPrintError(Status, Arguments[0], "Unable to exec");
            goto ExecuteUtilityEnd;
        }
    }

    //
    // Get the current real, user, and system times for the end of execution.
    //

    Status = SwGetTimes(&RealEndTime, &UserEndTime, &SystemEndTime);
    if (Status != 0) {
        goto ExecuteUtilityEnd;
    }

    TimepSubtractTimeval(&RealEndTime, &RealStartTime, &RealTime);
    TimepSubtractTimeval(&UserEndTime, &UserStartTime, &UserTime);
    TimepSubtractTimeval(&SystemEndTime, &SystemStartTime, &SystemTime);

    //
    // Now display the times to standard error.
    //

    fprintf(stderr,
            "\nreal %ld.%06d\nuser %ld.%06d\nsys %ld.%06d\n",
            (long)RealTime.tv_sec,
            (int)RealTime.tv_usec,
            (long)UserTime.tv_sec,
            (int)UserTime.tv_usec,
            (long)SystemTime.tv_sec,
            (int)SystemTime.tv_usec);

    Status = ReturnValue;

ExecuteUtilityEnd:
    return Status;
}

VOID
TimepSubtractTimeval (
    struct timeval *Timeval1,
    struct timeval *Timeval2,
    struct timeval *Result
    )

/*++

Routine Description:

    This routine subtracts one timeval from another and returns the result.

Arguments:

    Timeval1 - Supplies a pointer to the time value that is to be subtracted
        from.

    Timeval2 - Supplies a pointer to the time value to subtract.

    Result - Supplies a pointer that will receive the result of the subtraction.

Return Value:

    None.

--*/

{

    Result->tv_sec = Timeval1->tv_sec - Timeval2->tv_sec;
    Result->tv_usec = Timeval1->tv_usec - Timeval2->tv_usec;
    if (Result->tv_usec < 0) {
        Result->tv_sec -= 1;
        Result->tv_usec += 1000000;
    }

    return;
}

