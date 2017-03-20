/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tzset.c

Abstract:

    This module implements a program which allows the user to change the
    default time zone.

Author:

    Evan Green 20-Mar-2017

Environment:

    Minoca OS

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <minoca/lib/minocaos.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define TZSET_DEFAULT_ALMANAC_PATH "/usr/share/tz/tzdata"
#define TZSET_DEFAULT_ZONE_PATH "/etc/tz"

#define TZSET_VERSION_MAJOR 1
#define TZSET_VERSION_MINOR 0

#define TZSET_USAGE                                                            \
    "usage: tzset [options] zone_name\n"                                       \
    "       tzset --list\n"                                                    \
    "The tzset utility allows the user to change the default time zone. \n"    \
    "Options are:\n"                                                           \
    "  -i, --input=file -- Supply the path to the time zone almanac.\n"        \
    "      The default is " TZSET_DEFAULT_ALMANAC_PATH ".\n"                   \
    "  -o, --output=file -- Supply the output path for the filtered data.\n"   \
    "      The defailt is " TZSET_DEFAULT_ZONE_PATH ".\n"                      \
    "  -l, --list -- List all time zones in the almanac and exit.\n"           \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define TZSET_OPTIONS_STRING "i:o:lhV"

#define TZSET_OPTION_LIST 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option TzsetLongOptions[] = {
    {"input", required_argument, 0, 'i'},
    {"output", required_argument, 0, 'o'},
    {"list", no_argument, 0, 'l'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the tzset program, which allows the user to change
    the default time zone.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID InBuffer;
    FILE *InFile;
    PCSTR InPath;
    KSTATUS KStatus;
    INT Option;
    ULONG Options;
    PSTR OutBuffer;
    PSTR OutEnd;
    FILE *OutFile;
    PCSTR OutPath;
    ULONG OutSize;
    off_t InSize;
    struct stat Stat;
    INT Status;
    PSTR ZoneName;

    InBuffer = NULL;
    InFile = NULL;
    InPath = TZSET_DEFAULT_ALMANAC_PATH;
    OutBuffer = NULL;
    OutFile = NULL;
    OutPath = TZSET_DEFAULT_ZONE_PATH;
    Options = 0;
    Status = 1;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             TZSET_OPTIONS_STRING,
                             TzsetLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            goto MainEnd;
        }

        switch (Option) {
        case 'i':
            InPath = optarg;
            break;

        case 'o':
            OutPath = optarg;
            break;

        case 'l':
            Options |= TZSET_OPTION_LIST;
            break;

        case 'V':
            printf("tzset version %d.%d.\n",
                   TZSET_VERSION_MAJOR,
                   TZSET_VERSION_MINOR);

            return 1;

        case 'h':
            printf(TZSET_USAGE);
            return 1;

        default:

            assert(FALSE);

            goto MainEnd;
        }
    }

    if (stat(InPath, &Stat) != 0) {
        fprintf(stderr,
                "Error: Failed to stat %s: %s\n",
                InPath,
                strerror(errno));

        goto MainEnd;
    }

    InSize = Stat.st_size;
    InFile = fopen(InPath, "rb");
    if (InFile == NULL) {
        fprintf(stderr,
                "Error: Failed to open %s: %s\n",
                InPath,
                strerror(errno));

        goto MainEnd;
    }

    InBuffer = malloc(InSize);
    OutBuffer = malloc(OutSize);
    if ((InBuffer == NULL) || (OutBuffer == NULL)) {
        goto MainEnd;
    }

    if (fread(InBuffer, 1, InSize, InFile) != InSize) {
        fprintf(stderr, "Error: Read error: %s\n", strerror(errno));
        goto MainEnd;
    }

    //
    // If requested, just list the zones rather than saving a new one.
    //

    if ((Options & TZSET_OPTION_LIST) != 0) {
        KStatus = RtlListTimeZones(InBuffer, InSize, OutBuffer, &OutSize);
        if (!KSUCCESS(KStatus)) {
            if (KStatus == STATUS_FILE_CORRUPT) {
                fprintf(stderr, "Error: Invalid time zone data.\n");

            } else {
                fprintf(stderr,
                        "Error: Failed to get zone names: %d\n",
                        KStatus);
            }

            goto MainEnd;
        }

        //
        // List all the names, which end with a double null terminator.
        //

        OutEnd = OutBuffer + OutSize;
        ZoneName = OutBuffer;
        while ((ZoneName < OutEnd) && (*ZoneName != '\0')) {
            printf("%s\n", ZoneName);
            ZoneName += strlen(ZoneName) + 1;
        }

        Status = 0;
        goto MainEnd;
    }

    if (optind + 1 != ArgumentCount) {
        fprintf(stderr,
                "Error: Expected exactly one argument. "
                "See --help for usage.\n");

        goto MainEnd;
    }

    ZoneName = Arguments[optind];
    OutSize = InSize;
    KStatus = RtlFilterTimeZoneData(InBuffer,
                                    InSize,
                                    ZoneName,
                                    OutBuffer,
                                    &OutSize);

    if (!KSUCCESS(KStatus)) {
        if (KStatus == STATUS_NOT_FOUND) {
            fprintf(stderr, "Error: No such zone '%s'\n", ZoneName);

        } else if (KStatus == STATUS_FILE_CORRUPT) {
            fprintf(stderr, "Error: Invalid time zone data.\n");

        } else {
            fprintf(stderr, "Error: Failed to filter zone data: %d\n", KStatus);
        }

        goto MainEnd;
    }

    OutFile = fopen(OutPath, "wb");
    if (OutFile == NULL) {
        fprintf(stderr,
                "Error: Failed to open %s: %s.\n",
                OutPath,
                strerror(errno));

        goto MainEnd;
    }

    if (fwrite(OutBuffer, 1, OutSize, OutFile) != OutSize) {
        fprintf(stderr, "Error: Write error: %s\n", strerror(errno));
        goto MainEnd;
    }

    Status = 0;

MainEnd:
    if (InBuffer != NULL) {
        free(InBuffer);
    }

    if (OutBuffer != NULL) {
        free(OutBuffer);
    }

    if (InFile != NULL) {
        fclose(InFile);
    }

    if (OutFile != NULL) {
        fclose(OutFile);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

