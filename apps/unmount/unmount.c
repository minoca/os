/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    unmount.c

Abstract:

    This module implements the unmount program.

Author:

    Chris Stevens 30-Jul-2013

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>
#include <minoca/lib/mlibc.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define UNMOUNT_VERSION_MAJOR 1
#define UNMOUNT_VERSION_MINOR 0

#define UNMOUNT_USAGE                                                          \
    "usage: umount [-Rl] mount_point\n\n"                                     \
    "Options:\n"                                                               \
    "  -l --lazy -- Lazily unmount the device from the directory, preventing\n"\
    "        new accesses, but don't clean up until all references are"        \
    "        dropped.\n"                                                       \
    "  -R --recursive -- Recursively unmount the specified mount point.\n"     \
    "  --help -- Display this help text.\n"

#define UNMOUNT_OPTIONS_STRING "lR"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
Unmount (
    PSTR MountPointPath,
    ULONG Flags
    );

//
// -------------------------------------------------------------------- Globals
//

struct option UnmountLongOptions[] = {
    {"lazy", no_argument, 0, 'l'},
    {"recursive", no_argument, 0, 'R'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
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

    This routine implements the unmount user mode program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG ArgumentIndex;
    ULONG Flags;
    PSTR MountPointPath;
    INT Option;
    INT ReturnValue;
    BOOL UnmountAttempted;

    Flags = 0;
    ReturnValue = 0;
    MountPointPath = NULL;
    UnmountAttempted = FALSE;

    //
    // There should be no more than three arguments.
    //

    if (ArgumentCount > 3) {
        ReturnValue = EINVAL;
        goto mainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             UNMOUNT_OPTIONS_STRING,
                             UnmountLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            ReturnValue = 1;
            goto mainEnd;
        }

        switch (Option) {
        case 'l':
            Flags |= SYS_MOUNT_FLAG_DETACH;
            break;

        case 'R':
            Flags |= SYS_MOUNT_FLAG_RECURSIVE;
            break;

        case 'V':
            printf("unmount version %d.%02d\n",
                   UNMOUNT_VERSION_MAJOR,
                   UNMOUNT_VERSION_MINOR);

            ReturnValue = 1;
            goto mainEnd;

        case 'h':
            printf(UNMOUNT_USAGE);
            return 1;

        default:

            assert(FALSE);

            ReturnValue = 1;
            goto mainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        MountPointPath = Arguments[ArgumentIndex];
    }

    //
    // There must be a target path.
    //

    if (MountPointPath == NULL) {
        ReturnValue = EINVAL;
        goto mainEnd;
    }

    //
    // Attempt the unmount.
    //

    ReturnValue = Unmount(MountPointPath, Flags);
    UnmountAttempted = TRUE;

mainEnd:
    if ((ReturnValue == EINVAL) && (UnmountAttempted == FALSE)) {
        printf(UNMOUNT_USAGE);
    }

    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
Unmount (
    PSTR MountPointPath,
    ULONG Flags
    )

/*++

Routine Description:

    This routine unmounts the given directory.

Arguments:

    MountPointPath - Supplies a pointer to the mount point path to be unmounted.

    Flags - Supplies the flags to send with the unmount command.
        See SYS_MOUNT_FLAG_* definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT ReturnValue;
    KSTATUS Status;

    ReturnValue = 0;
    Status = OsUnmount(MountPointPath,
                       RtlStringLength(MountPointPath) + 1,
                       Flags);

    if (!KSUCCESS(Status)) {
        ReturnValue = ClConvertKstatusToErrorNumber(Status);
        fprintf(stderr,
                "Error: failed to unmount %s with error %d: %s.\n",
                MountPointPath,
                Status,
                strerror(ReturnValue));
    }

    return ReturnValue;
}

