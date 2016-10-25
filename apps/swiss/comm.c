/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    comm.c

Abstract:

    This module implements the comm utility, which for two files of sorted
    lines reports which lines are only in file A, which files are only in file
    B, and which lines are common to both files.

Author:

    Evan Green 6-Sep-2014

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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define COMM_VERSION_MAJOR 1
#define COMM_VERSION_MINOR 0

#define COMM_USAGE                                                             \
    "usage: cp [options] file1 file2\n"                                        \
    "The comm utility takes two sorted input files and reports in a \n"        \
    "three-column format which lines are unique to file 1, which lines are \n" \
    "unique to file 2, and which lines are shared between the two files. \n"   \
    "The files must be sorted, otherwise unexpected behavior might result."    \
    "Options are:\n"                                                           \
    "  -1 -- Suppress column 1 (lines unique to file 1).\n"                    \
    "  -2 -- Suppress column 2 (lines unique to file 2).\n"                    \
    "  -3 -- Suppress column 3 (lines unique to file 3).\n"                    \
    "  --check-order -- Report if the input is not sorted.\n"                  \
    "  --nocheck-order -- Remain silent about unsorted files.\n"               \
    "  --output-delimiter=string -- Use the given string as an output \n"      \
    "        delimiter. The default is a tab.\n"                               \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define COMM_OPTIONS_STRING "123CNOhV"

//
// This option is set when column 1 should be output.
//

#define COMM_OPTION_PRINT_FILE_A 0x00000001

//
// This option is set when column 2 should be output.
//

#define COMM_OPTION_PRINT_FILE_B 0x00000002

//
// This option is set when lines common to both files should be output.
//

#define COMM_OPTION_PRINT_COMMON 0x00000004

//
// This option is set when the input files should be checked against being
// sorted.
//

#define COMM_OPTION_CHECK_SORTING 0x00000008

//
// Define the default behavior when no options are specified.
//

#define COMM_DEFAULT_OPTIONS    \
    (COMM_OPTION_PRINT_FILE_A | \
     COMM_OPTION_PRINT_FILE_B | \
     COMM_OPTION_PRINT_COMMON | \
     COMM_OPTION_CHECK_SORTING)

//
// Define the default output delimiter.
//

#define COMM_DEFAULT_DELIMITER "\t"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option CommLongOptions[] = {
    {"check-order", no_argument, 0, 'C'},
    {"nocheck-order", no_argument, 0, 'N'},
    {"output-delimiter", required_argument, 0, 'O'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
CommMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the comm utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    BOOL AdvanceA;
    BOOL AdvanceB;
    ULONG ArgumentIndex;
    BOOL CheckA;
    BOOL CheckB;
    INT Compare;
    PSTR Delimiter;
    FILE *FileA;
    FILE *FileB;
    PSTR FileNameA;
    PSTR FileNameB;
    PSTR LineA;
    PSTR LineB;
    INT Option;
    ULONG Options;
    PSTR PreviousLineA;
    PSTR PreviousLineB;
    int Status;

    Delimiter = COMM_DEFAULT_DELIMITER;
    FileA = NULL;
    FileB = NULL;
    LineA = NULL;
    LineB = NULL;
    Options = COMM_DEFAULT_OPTIONS;
    PreviousLineA = NULL;
    PreviousLineB = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             COMM_OPTIONS_STRING,
                             CommLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case '1':
            Options &= ~COMM_OPTION_PRINT_FILE_A;
            break;

        case '2':
            Options &= ~COMM_OPTION_PRINT_FILE_B;
            break;

        case '3':
            Options &= ~COMM_OPTION_PRINT_COMMON;
            break;

        case 'C':
            Options |= COMM_OPTION_CHECK_SORTING;
            break;

        case 'N':
            Options &= ~COMM_OPTION_CHECK_SORTING;
            break;

        case 'O':
            Delimiter = optarg;
            break;

        case 'V':
            SwPrintVersion(COMM_VERSION_MAJOR, COMM_VERSION_MINOR);
            return 1;

        case 'h':
            printf(COMM_USAGE);
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

    if (ArgumentIndex + 2 != ArgumentCount) {
        SwPrintError(0, NULL, "Exactly two arguments expected");
        Status = 1;
        goto MainEnd;
    }

    //
    // Open up the two files.
    //

    FileNameA = Arguments[ArgumentIndex];
    FileNameB = Arguments[ArgumentIndex + 1];
    if (strcmp(FileNameA, "-") == 0) {
        FileA = stdin;

    } else {
        FileA = fopen(FileNameA, "r");
    }

    if (FileA == NULL) {
        Status = errno;
        SwPrintError(Status, FileNameA, "Failed to open");
        goto MainEnd;
    }

    if (strcmp(FileNameB, "-") == 0) {
        FileB = stdin;

    } else {
        FileB = fopen(FileNameB, "r");
    }

    if (FileB == NULL) {
        Status = errno;
        SwPrintError(Status, FileNameB, "Failed to open");
        goto MainEnd;
    }

    //
    // Prime the lines.
    //

    Status = SwReadLine(FileA, &LineA);
    if (Status != 0) {
        SwPrintError(Status, FileNameA, "Failed to read");
        goto MainEnd;
    }

    Status = SwReadLine(FileB, &LineB);
    if (Status != 0) {
        SwPrintError(Status, FileNameA, "Failed to read");
        goto MainEnd;
    }

    //
    // Loop printing the lines.
    //

    CheckA = FALSE;
    CheckB = FALSE;
    if ((Options & COMM_OPTION_CHECK_SORTING) != 0) {
        CheckA = TRUE;
        CheckB = TRUE;
    }

    while ((LineA != NULL) || (LineB != NULL)) {
        AdvanceA = FALSE;
        AdvanceB = FALSE;

        //
        // If it's just file B, then the line is unique to file B.
        //

        if (LineA == NULL) {
            printf("%s%s\n", Delimiter, LineB);
            AdvanceB = TRUE;

        //
        // If it's just file A, then the line is unique to file A.
        //

        } else if (LineB == NULL) {
            printf("%s\n", LineA);
            AdvanceA = TRUE;

        //
        // Both lines are present, compare them.
        //

        } else {
            Compare = strcmp(LineA, LineB);

            //
            // If A < B, then print A and advance A to try to catch it up to B.
            //

            if (Compare < 0) {
                printf("%s\n", LineA);
                AdvanceA = TRUE;

            //
            // If A > B, then print B and advance B to try to catch it up to A.
            //

            } else if (Compare > 0) {
                printf("%s%s\n", Delimiter, LineB);
                AdvanceB = TRUE;

            //
            // A == B, they're common! Advance both.
            //

            } else {
                printf("%s%s%s\n", Delimiter, Delimiter, LineA);
                AdvanceA = TRUE;
                AdvanceB = TRUE;
            }
        }

        //
        // Advance the lines.
        //

        if (AdvanceA != FALSE) {
            if ((PreviousLineA != NULL) && (CheckA != FALSE)) {
                Compare = strcmp(PreviousLineA, LineA);
                if (Compare > 0) {
                    SwPrintError(0, NULL, "File 1 is not in sorted order");
                    CheckA = FALSE;
                }
            }

            if (PreviousLineA != NULL) {
                free(PreviousLineA);
            }

            PreviousLineA = LineA;
            LineA = NULL;
            Status = SwReadLine(FileA, &LineA);
            if (Status != 0) {
                SwPrintError(Status, FileNameA, "Failed to read");
                goto MainEnd;
            }
        }

        if (AdvanceB != FALSE) {
            if ((PreviousLineB != NULL) && (CheckB != FALSE)) {
                Compare = strcmp(PreviousLineB, LineB);
                if (Compare > 0) {
                    SwPrintError(0, NULL, "File 2 is not in sorted order.");
                    CheckB = FALSE;
                }
            }

            if (PreviousLineB != NULL) {
                free(PreviousLineB);
            }

            PreviousLineB = LineB;
            LineB = NULL;
            Status = SwReadLine(FileB, &LineB);
            if (Status != 0) {
                SwPrintError(Status, FileNameB, "Failed to read");
                goto MainEnd;
            }
        }
    }

MainEnd:
    if ((FileA != NULL) && (FileA != stdin)) {
        fclose(FileA);
    }

    if ((FileB != NULL) && (FileB != stdin)) {
        fclose(FileB);
    }

    if (PreviousLineA != NULL) {
        free(PreviousLineA);
    }

    if (PreviousLineB != NULL) {
        free(PreviousLineB);
    }

    if (LineA != NULL) {
        free(LineA);
    }

    if (LineB != NULL) {
        free(LineB);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

