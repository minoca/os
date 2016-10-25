/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cmp.c

Abstract:

    This module implements the cmp (compare utility).

Author:

    Evan Green 4-Sep-2013

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

#define CMP_VERSION_MAJOR 1
#define CMP_VERSION_MINOR 0

#define CMP_USAGE                                                              \
    "usage: cmp [-l | -s] file1 file2\n"                                       \
    "The cmp utility compares two files. It writes no output if both files \n" \
    "are the same. Under default options, it writes the byte and line \n"      \
    "number at which the first difference occurred. Options are:\n"            \
    "  -l, --verbose -- Write the byte number (decimal) and the differing \n"  \
    "        bytes (octal) for each difference.\n"                             \
    "  -s, --quiet, --silent -- Write nothing for differing files. Return \n"  \
    "        exit status only.\n"                                              \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Show the application version information and exit.\n\n"    \
    "The operands are paths to files to compare. If - is supplied for \n"      \
    "either file, standard in will be used.\n"                                 \
    "The cmp utility returns 0 if the files are identical, 1 if the files \n"  \
    "are different or of different size, and >1 if an error occurred.\n\n"

#define CMP_OPTIONS_STRING "ls"

//
// Define cmp options.
//

//
// Set this option to print each byte difference.
//

#define CMP_OPTION_VERBOSE 0x00000001

//
// Set this option to print nothing.
//

#define CMP_OPTION_SILENT 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option CmpLongOptions[] = {
    {"verbose", no_argument, 0, 'l'},
    {"quiet", no_argument, 0, 's'},
    {"silent", no_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
CmpMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the cmp (compare) utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONG ArgumentIndex;
    INT Character1;
    INT Character2;
    ULONGLONG CharacterNumber;
    FILE *File1;
    FILE *File2;
    ULONGLONG LineNumber;
    INT Option;
    ULONG Options;
    PSTR Path1;
    PSTR Path2;
    ULONG SourceCount;
    int Status;

    File1 = NULL;
    File2 = NULL;
    SourceCount = 0;
    Options = COPY_OPTION_FOLLOW_OPERAND_LINKS;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CMP_OPTIONS_STRING,
                             CmpLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 2;
            goto MainEnd;
        }

        switch (Option) {
        case 'l':
            Options |= CMP_OPTION_VERBOSE;
            Options &= ~CMP_OPTION_SILENT;
            break;

        case 's':
            Options |= CMP_OPTION_SILENT;
            Options &= ~CMP_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(CMP_VERSION_MAJOR, CMP_VERSION_MINOR);
            return 2;

        case 'h':
            printf(CMP_USAGE);
            return 2;

        default:

            assert(FALSE);

            Status = 2;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    SourceCount = ArgumentCount - ArgumentIndex;

    //
    // Fail if there were not enough arguments.
    //

    if ((SourceCount != 2) && (SourceCount != 1)) {
        SwPrintError(0,
                     NULL,
                     "One or two arguments expected. Try --help for usage");

        return 2;
    }

    Path1 = Arguments[ArgumentIndex];
    Path2 = NULL;
    if (SourceCount == 2) {
        Path2 = Arguments[ArgumentIndex + 1];
    }

    //
    // Open up the files.
    //

    if (strcmp(Path1, "-") == 0) {
        Path1 = "<stdin>";
        File1 = stdin;

    } else {
        File1 = fopen(Path1, "rb");
        if (File1 == NULL) {
            Status = errno;
            SwPrintError(Status, Path1, "Unable to open");
            goto MainEnd;
        }
    }

    if ((Path2 == NULL) || (strcmp(Path2, "-") == 0)) {
        Path2 = "<stdin>";
        File2 = stdin;

    } else {
        File2 = fopen(Path2, "rb");
        if (File2 == NULL) {
            Status = errno;
            SwPrintError(Status, Path2, "Unable to open");
            goto MainEnd;
        }
    }

    //
    // If standard in was selected for anything, then change standard in to
    // binary mode.
    //

    if ((File1 == stdin) || (File2 == stdin)) {
        Status = SwSetBinaryMode(fileno(stdin), TRUE);
        if (Status != 0) {
            SwPrintError(Status, NULL, "Failed to set stdin binary mode.");
            goto MainEnd;
        }
    }

    //
    // Perform the comparison.
    //

    CharacterNumber = 1;
    LineNumber = 1;
    Status = 0;
    while (TRUE) {
        Character1 = fgetc(File1);
        Character2 = fgetc(File2);

        //
        // Handle one or both of the files ending.
        //

        if (Character1 == EOF) {
            if (Character2 != EOF) {
                if ((Options & CMP_OPTION_SILENT) == 0) {
                    fprintf(stderr, "cmp: EOF on %s\n", Path1);
                }

                Status = 1;
            }

            break;

        } else if (Character2 == EOF) {
            if ((Options & CMP_OPTION_SILENT) == 0) {
                fprintf(stderr, "cmp: EOF on %s\n", Path2);
            }

            Status = 1;
            break;
        }

        //
        // Neither character is EOF. If they're different, report it.
        //

        if (Character1 != Character2) {
            Status = 1;
            if ((Options & CMP_OPTION_VERBOSE) != 0) {
                printf("%lld %o %o\n",
                       CharacterNumber,
                       Character1,
                       Character2);

            } else {
                if ((Options & CMP_OPTION_SILENT) == 0) {
                    printf("%s %s differ: char %lld, line %lld\n",
                           Path1,
                           Path2,
                           CharacterNumber,
                           LineNumber);
                }

                break;
            }
        }

        //
        // Advance the character and line numbers.
        //

        CharacterNumber += 1;
        if (Character1 == '\n') {
            LineNumber += 1;
        }
    }

    //
    // Return an error if either of the streams are funky.
    //

    if ((ferror(File1) != 0) || (ferror(File2) != 0)) {
        Status = errno;
        if (Status == 0) {
            Status = 2;
        }
    }

MainEnd:
    if ((File1 != NULL) && (File1 != stdin)) {
        fclose(File1);
    }

    if ((File2 != NULL) && (File2 != stdin)) {
        fclose(File2);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

