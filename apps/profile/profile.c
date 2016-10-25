/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    profile.c

Abstract:

    This module implements the system profiler application.

Author:

    Chris Stevens 18-Jan-2015

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>
#include <minoca/kernel/sp.h>
#include <minoca/lib/mlibc.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// --------------------------------------------------------------------- Macros
//

#define PRINT_ERROR(...) fprintf(stderr, "\nprofile: " __VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

#define PROFILE_VERSION_MAJOR 1
#define PROFILE_VERSION_MINOR 0

#define PROFILE_USAGE                                                          \
    "usage: profile [-d <type>] [-e <type>]\n\n"                               \
    "The profile utility enables, disables or gets system profiling state.\n\n"\
    "Options:\n"                                                               \
    "  -d, --disable <type> -- Disable a system profiler. Valid values are \n" \
    "      stack, memory, thread, and all.\n"                                  \
    "  -e, --enable <type> -- Enable a system profiler. Valid values are \n"   \
    "      stack, memory, thread, all.\n"                                      \
    "  --help -- Display this help text.\n"                                    \
    "  --version -- Display the application version and exit.\n\n"

#define PROFILE_OPTIONS_STRING "e:d:Vh"

#define PROFILE_TYPE_COUNT 4

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the information for a given profile type option.

Members:

    Name - Stores the command line name for the profile type option.

    TypeFlags - Stores bitmask of type flags for the system profilers that
        correspond to the name. See PROFILER_TYPE_FLAG_* for definitions.

--*/

typedef struct _PROFILE_TYPE_DATA {
    PSTR Name;
    ULONG TypeFlags;
} PROFILE_TYPE_DATA, *PPROFILE_TYPE_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option ProfileLongOptions[] = {
    {"disable", required_argument, 0, 'd'},
    {"enable", required_argument, 0, 'e'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

PROFILE_TYPE_DATA ProfileTypeData[PROFILE_TYPE_COUNT] = {
    {
        "all",
        PROFILER_TYPE_FLAG_STACK_SAMPLING |
        PROFILER_TYPE_FLAG_MEMORY_STATISTICS |
        PROFILER_TYPE_FLAG_THREAD_STATISTICS
    },

    {
        "stack",
        PROFILER_TYPE_FLAG_STACK_SAMPLING
    },

    {
        "memory",
        PROFILER_TYPE_FLAG_MEMORY_STATISTICS
    },

    {
        "thread",
        PROFILER_TYPE_FLAG_THREAD_STATISTICS
    },
};

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the kernel test program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG DisableFlags;
    ULONG EnableFlags;
    ULONG Index;
    INT Option;
    INT ReturnValue;
    UINTN Size;
    SP_GET_SET_STATE_INFORMATION StateInformation;
    KSTATUS Status;

    DisableFlags = 0;
    EnableFlags = 0;
    ReturnValue = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             PROFILE_OPTIONS_STRING,
                             ProfileLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            ReturnValue = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'd':
            for (Index = 0; Index < PROFILE_TYPE_COUNT; Index += 1) {
                if (strcasecmp(optarg, ProfileTypeData[Index].Name) == 0) {
                    DisableFlags = ProfileTypeData[Index].TypeFlags;
                    break;
                }
            }

            if (Index == PROFILE_TYPE_COUNT) {
                PRINT_ERROR("Invalid profiling type: %s\n", optarg);
                ReturnValue = 1;
                goto MainEnd;
            }

            break;

        case 'e':
            for (Index = 0; Index < PROFILE_TYPE_COUNT; Index += 1) {
                if (strcasecmp(optarg, ProfileTypeData[Index].Name) == 0) {
                    EnableFlags = ProfileTypeData[Index].TypeFlags;
                    break;
                }
            }

            if (Index == PROFILE_TYPE_COUNT) {
                PRINT_ERROR("Invalid profiling type: %s\n", optarg);
                ReturnValue = 1;
                goto MainEnd;
            }

            break;

        case 'V':
            printf("profile version %d.%02d\n",
                   PROFILE_VERSION_MAJOR,
                   PROFILE_VERSION_MINOR);

            ReturnValue = 1;
            goto MainEnd;

        case 'h':
            printf(PROFILE_USAGE);
            return 1;

        default:

            assert(FALSE);

            ReturnValue = 1;
            goto MainEnd;
        }
    }

    //
    // If there is nothing to enable or disable, then just get and print the
    // status.
    //

    if ((EnableFlags == 0) && (DisableFlags == 0)) {
        Size = sizeof(SP_GET_SET_STATE_INFORMATION);
        RtlZeroMemory(&StateInformation, Size);
        Status = OsGetSetSystemInformation(SystemInformationSp,
                                           SpInformationGetSetState,
                                           &StateInformation,
                                           &Size,
                                           FALSE);

        if (!KSUCCESS(Status)) {
            ReturnValue = ClConvertKstatusToErrorNumber(Status);
            PRINT_ERROR("Failed to get profiling information: %s.\n",
                        strerror(ReturnValue));

            goto MainEnd;
        }

        if (Size < sizeof(SP_GET_SET_STATE_INFORMATION)) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            ReturnValue = ClConvertKstatusToErrorNumber(Status);
            PRINT_ERROR("Failed to get profiling information: %s.\n",
                        strerror(ReturnValue));

            goto MainEnd;
        }

        for (Index = 1; Index < PROFILE_TYPE_COUNT; Index += 1) {
            if ((StateInformation.ProfilerTypeFlags &
                 ProfileTypeData[Index].TypeFlags) != 0) {

                printf("%s - enabled\n", ProfileTypeData[Index].Name);

            } else {
                printf("%s - disabled\n", ProfileTypeData[Index].Name);
            }
        }

    //
    // Disable and enable the profiler types specified, unless they are equal.
    //

    } else if (DisableFlags != EnableFlags) {

        //
        // Don't disable anything that is about to be enabled.
        //

        DisableFlags &= ~EnableFlags;
        if (DisableFlags != 0) {
            Size = sizeof(SP_GET_SET_STATE_INFORMATION);
            StateInformation.Operation = SpGetSetStateOperationDisable;
            StateInformation.ProfilerTypeFlags = DisableFlags;
            Status = OsGetSetSystemInformation(SystemInformationSp,
                                               SpInformationGetSetState,
                                               &StateInformation,
                                               &Size,
                                               TRUE);

            if (!KSUCCESS(Status)) {
                ReturnValue = ClConvertKstatusToErrorNumber(Status);
                PRINT_ERROR("Failed to disable profiling information: %s.\n",
                            strerror(ReturnValue));

                goto MainEnd;
            }
        }

        if (EnableFlags != 0) {
            Size = sizeof(SP_GET_SET_STATE_INFORMATION);
            StateInformation.Operation = SpGetSetStateOperationEnable;
            StateInformation.ProfilerTypeFlags = EnableFlags;
            Status = OsGetSetSystemInformation(SystemInformationSp,
                                               SpInformationGetSetState,
                                               &StateInformation,
                                               &Size,
                                               TRUE);

            if (!KSUCCESS(Status)) {
                ReturnValue = ClConvertKstatusToErrorNumber(Status);
                PRINT_ERROR("Failed to enable profiling information: %s.\n",
                            strerror(ReturnValue));

                goto MainEnd;
            }
        }

    //
    // Tell the user that no action was taken.
    //

    } else {
        printf("Attempt to enable and disable the same profiling types "
               "ignored.\n");
    }

MainEnd:
    return ReturnValue;
}

