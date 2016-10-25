/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    seq.c

Abstract:

    This module implements the seq (sequence) utility, which simply prints out
    a sequence of numbers.

Author:

    Evan Green 9-May-2016

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

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SEQ_VERSION_MAJOR 1
#define SEQ_VERSION_MINOR 0

#define SEQ_USAGE                                                              \
    "usage: seq [options] [[first] increment] last\n"                          \
    "The seq utility prints a sequence of numbers between the given range.\n"  \
    "Options are:\n"                                                           \
    "  -f, --format=format -- Specify the printf-style format to print "       \
    "values in.\n"                                                             \
    "  -s, --separator=string -- Specify the separator (default newline).\n"   \
    "  -w, --equal-width -- Pad with leading zeros so all values have the \n"  \
    "      same width.\n"                                                      \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

//
// No permutations of arguments are allowed because something like -1 might be
// specified as an argument.
//

#define SEQ_OPTIONS_STRING "+f:s:whV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
SeqCheckFormat (
    PSTR Format
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SeqLongOptions[] = {
    {"format", no_argument, 0, 'f'},
    {"separator", no_argument, 0, 's'},
    {"equal-width", no_argument, 0, 'w'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
SeqMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the seq utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    PSTR Argument;
    INT ArgumentIndex;
    double Current;
    INT CurrentFractionDigits;
    PSTR CurrentSeparator;
    INT CurrentWidth;
    PSTR Decimal;
    double End;
    BOOL EqualWidth;
    PSTR Format;
    INT FractionDigits;
    double Increment;
    INT Index;
    INT LastCheck;
    INT Option;
    PSTR Separator;
    double Start;
    int Status;
    INT Width;

    EqualWidth = FALSE;
    Format = NULL;
    Separator = "\n";
    Start = 1;
    Increment = 1;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SEQ_OPTIONS_STRING,
                             SeqLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {

            //
            // Watch out for the possibility of a negative number as the first
            // argument.
            //

            if ((optind > 1) && (Arguments[optind - 1][0] == '-') &&
                (isdigit(Arguments[optind - 1][1]))) {

                optind -= 1;
                break;
            }

            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'f':
            Format = optarg;
            if (SeqCheckFormat(Format) == FALSE) {
                SwPrintError(0, Format, "Invalid format string");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 's':
            Separator = optarg;
            break;

        case 'w':
            EqualWidth = TRUE;
            break;

        case 'V':
            SwPrintVersion(SEQ_VERSION_MAJOR, SEQ_VERSION_MINOR);
            return 1;

        case 'h':
            printf(SEQ_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    Status = 1;
    if ((Format != NULL) && (EqualWidth != FALSE)) {
        SwPrintError(0, NULL, "Cannot have -f and -w together");
        goto MainEnd;
    }

    ArgumentIndex = optind;
    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0, NULL, "Argument expected");
        goto MainEnd;
    }

    LastCheck = ArgumentCount - 1;

    //
    // Get the end.
    //

    Argument = Arguments[ArgumentCount - 1];
    End = strtod(Argument, &AfterScan);
    if (*AfterScan != '\0') {
        SwPrintError(0, Argument, "Invalid value");
        goto MainEnd;
    }

    //
    // If there are more arguments, then there's a start. If there's a third
    // argument, there's an increment. Any more is an error.
    //

    if (ArgumentIndex + 1 < ArgumentCount) {

        //
        // For some reason the ending value isn't checked for width if there's
        // a start and/or increment.
        //

        LastCheck = ArgumentCount - 2;
        Argument = Arguments[ArgumentIndex];
        Start = strtod(Argument, &AfterScan);
        if (*AfterScan != '\0') {
            SwPrintError(0, Argument, "Invalid value");
            goto MainEnd;
        }

        if (ArgumentIndex + 2 < ArgumentCount) {
            if (ArgumentIndex + 3 < ArgumentCount) {
                SwPrintError(0, NULL, "Too many arguments");
                goto MainEnd;
            }

            Argument = Arguments[ArgumentIndex + 1];
            Increment = strtod(Argument, &AfterScan);
            if (*AfterScan != '\0') {
                SwPrintError(0, Argument, "Invalid value");
                goto MainEnd;
            }
        }
    }

    //
    // Figure out the proper width to print.
    //

    Width = 0;
    FractionDigits = 0;
    while (ArgumentIndex <= LastCheck) {
        Argument = Arguments[ArgumentIndex];
        Decimal = strchr(Argument, '.');
        if (Decimal == NULL) {
            CurrentWidth = strlen(Argument);
            CurrentFractionDigits = 0;

        } else {
            CurrentWidth = Decimal - Argument;
            CurrentFractionDigits = strlen(Decimal);
        }

        if (Width < CurrentWidth) {
            Width = CurrentWidth;
        }

        if (FractionDigits < CurrentFractionDigits) {
            FractionDigits = CurrentFractionDigits;
        }

        ArgumentIndex += 1;
    }

    if (FractionDigits != 0) {
        FractionDigits -= 1;
        if (FractionDigits != 0) {
            Width += FractionDigits + 1;
        }
    }

    if (EqualWidth == FALSE) {
        Width = 0;
    }

    Status = 0;
    Index = 0;
    Current = Start;
    CurrentSeparator = "";
    while (TRUE) {
        if (Increment >= 0) {
            if (Current > End) {
                break;
            }

        } else {
            if (Current < End) {
                break;
            }
        }

        //
        // Print with the given format if supplied, or the default format
        // otherwise.
        //

        if (Format != NULL) {
            if (printf("%s", CurrentSeparator) < 0) {
                Status = 1;
                break;
            }

            if (printf(Format, Current) < 0) {
                Status = 1;
                break;
            }

        } else {
            Status = printf("%s%0*.*f",
                            CurrentSeparator,
                            Width,
                            FractionDigits,
                            Current);

            if (Status < 0) {
                Status = 1;
                break;
            }
        }

        CurrentSeparator = Separator;
        Index += 1;
        Current = Start + (Index * Increment);
    }

    if (Index != 0) {
        printf("\n");
    }

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
SeqCheckFormat (
    PSTR Format
    )

/*++

Routine Description:

    This routine ensures the given format string is correct for a double
    argument.

Arguments:

    Format - Supplies a pointer to the potential format string.

Return Value:

    TRUE if the format is okay.

    FALSE if the format string is invalid.

--*/

{

    INT Specifiers;

    Specifiers = 0;
    while (*Format != '\0') {

        //
        // Skip anything that's not a format specifier.
        //

        if (*Format != '%') {
            Format += 1;
            continue;
        }

        Format += 1;

        //
        // Skip a literal percent.
        //

        if (*Format == '%') {
            Format += 1;
            continue;
        }

        //
        // Only one specifier is allowed.
        //

        Specifiers += 1;
        if (Specifiers > 1) {
            return FALSE;
        }

        //
        // Swallow the flags.
        //

        while ((*Format == '\'') || (*Format == '-') || (*Format == '+') ||
               (*Format == ' ') || (*Format == '#') || (*Format == '0')) {

            Format += 1;
        }

        //
        // Swallow the field width.
        //

        while (isdigit(*Format)) {
            Format += 1;
        }

        //
        // Swallow the precision.
        //

        if (*Format == '.') {
            Format += 1;
            while (isdigit(*Format)) {
                Format += 1;
            }
        }

        //
        // Now it had better be a specifier.
        //

        if ((*Format != 'f') && (*Format != 'F') && (*Format != 'e') &&
            (*Format != 'E') && (*Format != 'g') && (*Format != 'G') &&
            (*Format != 'A') && (*Format != 'a')) {

            return FALSE;
        }

        Format += 1;
    }

    return TRUE;
}

