/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    banner.c

Abstract:

    This module implements the banner tool, which can be used to toggle the
    banner thread on and off.

Author:

    Evan Green 31-Jul-2017

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <minoca/lib/minocaos.h>
#include <minoca/lib/mlibc.h>

//
// ---------------------------------------------------------------- Definitions
//

#define BANNER_VERSION_MAJOR 1
#define BANNER_VERSION_MINOR 0

#define BANNER_USAGE \
    "usage: banner [options] \n" \
    "The banner utility can be used to turn the banner thread on or off.\n" \
    "If no options are given, the default is to toggle.\n" \
    "Options are:\n" \
    "  -d, --disable -- Turn the banner thread off.\n" \
    "  -e, --enable -- Turn the banner thread on.\n" \
    "  -t, --toggle -- Toggle the banner thread.\n" \
    "  -h, --help -- Show this help text.\n" \
    "  -V, --version -- Prints application version information and exits.\n"

#define BANNER_OPTIONS "dethV"

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _BANNER_ACTION {
    BannerActionUnspecified,
    BannerActionEnable,
    BannerActionDisable,
    BannerActionToggle
} BANNER_ACTION, *PBANNER_ACTION;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option BannerLongOptions[] = {
    {"disable", no_argument, 0, 'd'},
    {"enable", no_argument, 0, 'e'},
    {"toggle", no_argument, 0, 't'},
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

    This routine implements the main entry point for the banner application.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    BANNER_ACTION Action;
    INT Option;
    INT Result;
    UINTN Size;
    KSTATUS Status;
    ULONG Value;

    Action = BannerActionUnspecified;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             BANNER_OPTIONS,
                             BannerLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Result = 1;
            goto mainEnd;
        }

        switch (Option) {
        case 'd':
            Action = BannerActionDisable;
            break;

        case 'e':
            Action = BannerActionEnable;
            break;

        case 't':
            Action = BannerActionToggle;
            break;

        case 'V':
            printf("banner version %d.%d.\n",
                   BANNER_VERSION_MAJOR,
                   BANNER_VERSION_MINOR);

            return 1;

        case 'h':
            printf(BANNER_USAGE);
            return 1;

        default:

            assert(FALSE);

            Result = 1;
            goto mainEnd;
        }
    }

    Size = sizeof(Value);
    Value = TRUE;
    switch (Action) {
    case BannerActionDisable:
        Value = FALSE;

        //
        // Fall through.
        //

    case BannerActionEnable:
        Status = OsGetSetSystemInformation(SystemInformationKe,
                                           KeInformationBannerThread,
                                           &Value,
                                           &Size,
                                           TRUE);

        break;

    case BannerActionUnspecified:
    case BannerActionToggle:
    default:
        Status = OsGetSetSystemInformation(SystemInformationKe,
                                           KeInformationBannerThread,
                                           &Value,
                                           &Size,
                                           FALSE);

        if (!KSUCCESS(Status)) {
            fprintf(stderr, "Failed to get banner thread status: %d\n", Status);
            Result = 1;
            goto mainEnd;
        }

        Value = !Value;
        Size = sizeof(Value);
        Status = OsGetSetSystemInformation(SystemInformationKe,
                                           KeInformationBannerThread,
                                           &Value,
                                           &Size,
                                           TRUE);

        break;
    }

    if (!KSUCCESS(Status)) {
        fprintf(stderr, "Failed to set banner thread status: %d\n", Status);
        Result = 1;
        goto mainEnd;
    }

    Result = 0;

mainEnd:
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

