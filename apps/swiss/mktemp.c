/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mktemp.c

Abstract:

    This module implements the mktemp (temporary file and directory creation)
    utility.

Author:

    Evan Green 11-Oct-2013

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
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MKTEMP_VERSION_MAJOR 1
#define MKTEMP_VERSION_MINOR 0

#define MKTEMP_USAGE                                                           \
    "usage: mktemp [-duq] [--suffix=SUFFIX] [--tmpdir[=DIR]] [-p DIR] [-t] "   \
    "template\n "                                                              \
    "The mktemp utility creates a temporary file or directory safely and \n"   \
    "prints its name. If no template is supplied, tmp.XXXXXXXXXX is used, \n"  \
    "and --tmpdir is implied. Valid options are:\n"                            \
    "  -d, --directory -- Create a directory, not a file.\n"                   \
    "  -u, --dry-run -- Do not create anything, just print a name.\n"          \
    "  -q, --quiet -- Suppress messages about file/directory creation.\n"      \
    "  --suffix=SUFFIX -- Append the given suffix to the template.\n"          \
    "  --tmpdir=DIR -- Prepend the given directory to the template. If not \n" \
    "        specified, prepend the value of the TMPDIR environment "          \
    "variable.\n"                                                              \
    "  -p DIR -- Use the given directory as a prefix.\n"                       \
    "  -t -- Interpret the template relative to a directory: TMPDIR if set, \n"\
    "        or the directory specified by -p, or /tmp.\n"                     \
    "  --help -- Display this help text and exit.\n"                           \
    "  --version -- Display the application version and exit.\n"

#define MKTEMP_OPTIONS_STRING "duqp:t"

//
// Define the name of the environment variable mktemp looks at to get the
// temporary directory prefix in some scenarios.
//

#define MKTEMP_DIRECTORY_VARIABLE "TMPDIR"

//
// Define the default template to use if none is provided.
//

#define MKTEMP_DEFAULT_TEMPLATE "tmp.XXXXXXXXXX"

//
// Define the permissions on temporary files.
//

#define TEMPORARY_FILE_PERMISSIONS (S_IRUSR | S_IWUSR)

//
// Define the permissions on a temporary directory.
//

#define TEMPORARY_DIRECTORY_PERMISSIONS (S_IRUSR | S_IWUSR | S_IXUSR)

//
// Define the minimum number of trailing X characters to enforce.
//

#define MKTEMP_MINIMUM_REPLACE_COUNT 3

//
// Define the number of times mktemp will try before giving up.
//

#define MKTEMP_TRY_COUNT 1000000

//
// Define mktemp option flags.
//

//
// Set this flag to create a directory instead of a file.
//

#define MKTEMP_OPTION_DIRECTORY 0x00000001

//
// Set this flag to only perform a dry run, not actually create a
// file/directory.
//

#define MKTEMP_OPTION_DRY_RUN 0x00000002

//
// Set this option to not print stuff.
//

#define MKTEMP_OPTION_QUIET 0x00000004

//
// Set this option to use the old directory order (the -t option).
//

#define MKTEMP_OPTION_OLD_DIRECTORY_ORDER 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
MktempReplaceTemplate (
    PSTR String,
    UINTN ReplaceCount
    );

//
// -------------------------------------------------------------------- Globals
//

struct option MktempLongOptions[] = {
    {"directory", no_argument, 0, 'd'},
    {"dry-run", no_argument, 0, 'u'},
    {"quiet", no_argument, 0, 'q'},
    {"suffix", required_argument, 0, 's'},
    {"tmpdir", optional_argument, 0, 'T'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
MktempMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the mktemp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AppendedPath;
    BOOL AppendedPathAllocated;
    ULONG AppendedPathSize;
    ULONG ArgumentIndex;
    INT Descriptor;
    size_t Length;
    INT Option;
    ULONG Options;
    PSTR Prefix;
    PSTR PrefixedPath;
    BOOL PrefixedPathAllocated;
    ULONG PrefixedPathSize;
    ULONG PrefixSize;
    size_t ReplaceCount;
    struct stat Stat;
    int Status;
    PSTR Suffix;
    ULONG SuffixSize;
    PSTR Template;
    PSTR TemplateCopy;
    ULONG TemplateSize;
    ULONG Try;
    PSTR Variable;

    AppendedPathAllocated = FALSE;
    Prefix = NULL;
    PrefixedPath = NULL;
    PrefixedPathAllocated = FALSE;
    Options = 0;
    Suffix = NULL;
    Template = NULL;
    TemplateCopy = NULL;
    srand(time(NULL) ^ getpid());

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             MKTEMP_OPTIONS_STRING,
                             MktempLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'd':
            Options |= MKTEMP_OPTION_DIRECTORY;
            break;

        case 'u':
            Options |= MKTEMP_OPTION_DRY_RUN;
            break;

        case 'q':
            Options |= MKTEMP_OPTION_QUIET;
            break;

        case 'p':
            Prefix = optarg;
            break;

        case 't':
            Options |= MKTEMP_OPTION_OLD_DIRECTORY_ORDER;
            break;

        case 's':
            Suffix = optarg;
            break;

        case 'T':
            Options &= ~MKTEMP_OPTION_OLD_DIRECTORY_ORDER;
            Prefix = optarg;
            if (Prefix == NULL) {
                Prefix = getenv(MKTEMP_DIRECTORY_VARIABLE);
            }

            break;

        case 'V':
            SwPrintVersion(MKTEMP_VERSION_MAJOR, MKTEMP_VERSION_MINOR);
            return 1;

        case 'h':
            printf(MKTEMP_USAGE);
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

    Template = NULL;
    if (ArgumentCount - ArgumentIndex != 0) {
        Template = Arguments[ArgumentIndex];
        ArgumentIndex += 1;

    //
    // If there was no template, use a default one, and get the prefix from the
    // environment if one is not already set.
    //

    } else {
        Template = MKTEMP_DEFAULT_TEMPLATE;
        Options |= MKTEMP_OPTION_OLD_DIRECTORY_ORDER;
    }

    //
    // If the old style directory order is in effect, then prefer the contents
    // of the variable TMPDIR, followed by the argument from -p, followed by
    // /tmp.
    //

    if ((Options & MKTEMP_OPTION_OLD_DIRECTORY_ORDER) != 0) {
        Variable = getenv(MKTEMP_DIRECTORY_VARIABLE);
        if (Variable != NULL) {
            Prefix = Variable;
        }

        if (Prefix == NULL) {
            Prefix = "/tmp";
        }
    }

    if (ArgumentCount - ArgumentIndex > 1) {
        SwPrintError(0, NULL, "Expected no more than one operand");
        Status = 1;
        goto MainEnd;
    }

    //
    // Ensure there are enough X characters to replace.
    //

    Length = strlen(Template);
    ReplaceCount = 0;
    while ((Length - ReplaceCount != 0) &&
           (Template[Length - 1 - ReplaceCount] == 'X')) {

        ReplaceCount += 1;
    }

    if (ReplaceCount < MKTEMP_MINIMUM_REPLACE_COUNT) {
        SwPrintError(0, Template, "Too few Xs in template");
        Status = 1;
        goto MainEnd;
    }

    TemplateSize = Length + 1;
    PrefixSize = 0;
    if (Prefix != NULL) {
        PrefixSize = strlen(Prefix) + 1;
    }

    SuffixSize = 0;
    if (Suffix != NULL) {
        SuffixSize = strlen(Suffix) + 1;
    }

    Status = -1;
    TemplateCopy = strdup(Template);
    if (TemplateCopy == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    for (Try = 0; Try < MKTEMP_TRY_COUNT; Try += 1) {
        MktempReplaceTemplate(TemplateCopy + Length - ReplaceCount,
                              ReplaceCount);

        //
        // Append the prefix if there is one.
        //

        if (Prefix != NULL) {
            Status = SwAppendPath(Prefix,
                                  PrefixSize,
                                  TemplateCopy,
                                  TemplateSize,
                                  &PrefixedPath,
                                  &PrefixedPathSize);

            if (Status == FALSE) {
                Status = ENOMEM;
                SwPrintError(Status, NULL, "Failed to create string");
                goto MainEnd;
            }

            PrefixedPathAllocated = TRUE;

        } else {
            PrefixedPath = TemplateCopy;
            PrefixedPathSize = TemplateSize;
        }

        //
        // Append the suffix if there is one.
        //

        if (Suffix != NULL) {
            Status = SwAppendPath(PrefixedPath,
                                  PrefixedPathSize,
                                  Suffix,
                                  SuffixSize,
                                  &AppendedPath,
                                  &AppendedPathSize);

            if (Status == FALSE) {
                Status = ENOMEM;
                SwPrintError(Status, NULL, "Failed to create string");
                goto MainEnd;
            }

            PrefixedPathAllocated = TRUE;

        } else {
            AppendedPath = PrefixedPath;
            AppendedPathSize = PrefixedPathSize;
        }

        //
        // Whew, now that the path is created, try to create it.
        //

        if ((Options & MKTEMP_OPTION_DRY_RUN) != 0) {
            Status = SwStat(AppendedPath, FALSE, &Stat);
            if (Status == ENOENT) {
                Status = 0;
                break;
            }

            if (Status != 0) {
                if ((Options & MKTEMP_OPTION_QUIET) == 0) {
                    SwPrintError(Status, AppendedPath, "Unable to stat");
                }

                goto MainEnd;
            }

        } else if ((Options & MKTEMP_OPTION_DIRECTORY) != 0) {
            Status = SwMakeDirectory(AppendedPath,
                                     TEMPORARY_DIRECTORY_PERMISSIONS);

            if (Status == 0) {
                break;
            }

            if (errno != EEXIST) {
                if ((Options & MKTEMP_OPTION_QUIET) == 0) {
                    SwPrintError(errno,
                                 AppendedPath,
                                 "Unable to create directory");
                }

                goto MainEnd;
            }

        } else {
            Descriptor = SwOpen(AppendedPath,
                                O_RDWR | O_CREAT | O_EXCL,
                                TEMPORARY_FILE_PERMISSIONS);

            if (Descriptor >= 0) {
                close(Descriptor);
                Status = 0;
                break;
            }

            if ((errno != EEXIST) && (errno != EISDIR)) {
                if ((Options & MKTEMP_OPTION_QUIET) == 0) {
                    SwPrintError(errno, AppendedPath, "Unable to create file");
                }

                goto MainEnd;
            }
        }

        //
        // Clean up.
        //

        if (PrefixedPathAllocated != FALSE) {
            free(PrefixedPath);
            PrefixedPath = NULL;
            PrefixedPathAllocated = FALSE;
        }

        if (AppendedPathAllocated != FALSE) {
            free(AppendedPath);
            AppendedPath = NULL;
            AppendedPathAllocated = FALSE;
        }

        assert(TemplateCopy != NULL);

        free(TemplateCopy);
        TemplateCopy = NULL;
    }

    if (Try == MKTEMP_TRY_COUNT) {
        SwPrintError(0, NULL, "Tried %d times and failed", MKTEMP_TRY_COUNT);
        goto MainEnd;
    }

    //
    // Print out what was found!
    //

    assert(AppendedPath != NULL);

    if ((Options & MKTEMP_OPTION_QUIET) == 0) {
        printf("%s\n", AppendedPath);
    }

    Status = 0;

MainEnd:
    if (PrefixedPathAllocated != FALSE) {
        free(PrefixedPath);
        PrefixedPath = NULL;
        PrefixedPathAllocated = FALSE;
    }

    if (AppendedPathAllocated != FALSE) {
        free(AppendedPath);
        AppendedPath = NULL;
        AppendedPathAllocated = FALSE;
    }

    free(TemplateCopy);
    TemplateCopy = NULL;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
MktempReplaceTemplate (
    PSTR String,
    UINTN ReplaceCount
    )

/*++

Routine Description:

    This routine creates random ASCII characters in the range of 0-9 and A-Z.

Arguments:

    String - Supplies a pointer to the template.

    ReplaceCount - Supplies the number of characters to replace.

Return Value:

    None.

--*/

{

    ULONG RandomIndex;
    INT Value;

    for (RandomIndex = 0; RandomIndex < ReplaceCount; RandomIndex += 1) {

        //
        // Create a random value using letters and numbers. Avoid relying
        // on case sensitivity just in case. For reference,
        // 36^5 is 60.4 million.
        //

        Value = rand() % 36;
        if (Value >= 10) {
            Value += 'A' - 10;

        } else {
            Value += '0';
        }

        String[RandomIndex] = Value;
    }

    return;
}

