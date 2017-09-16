/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mount.c

Abstract:

    This module implements the mount program.

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

#define MOUNT_VERSION_MAJOR 1
#define MOUNT_VERSION_MINOR 0

#define MOUNT_USAGE                                                        \
    "usage: mount [--bind | --rbind] target mount_point\n\n"               \
    "The mount utility mounts the given target at the mount_point.\n\n"    \
    "Options:\n"                                                           \
    "  --bind -- Allows remounting content that is already available\n"    \
    "            elsewhere in the file hierarchy.\n"                       \
    "  --rbind -- Allows remounting content that is already available\n"   \
    "             elsewhere in the file hierarchy, including submounts.\n" \
    "  --help -- Display this help text.\n"                                \
    "  --version -- Display the application version and exit.\n\n"

#define MOUNT_OPTIONS_STRING "h"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
Mount (
    PSTR MountPointPath,
    PSTR TargetPath,
    ULONG Flags
    );

INT
PrintMountPoints (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

struct option MountLongOptions[] = {
    {"bind", no_argument, 0, 'b'},
    {"rbind", no_argument, 0, 'r'},
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

    This routine implements the mount user mode program.

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
    BOOL MountAttempted;
    PSTR MountPointPath;
    INT Option;
    INT ReturnValue;
    PSTR TargetPath;

    Flags = 0;
    MountAttempted = FALSE;
    MountPointPath = NULL;
    ReturnValue = 0;
    TargetPath = NULL;

    //
    // If there are no arguments, just print the mount points.
    //

    if (ArgumentCount == 1) {
        ReturnValue = PrintMountPoints();
        goto mainEnd;
    }

    //
    // There should be no more than four arguments.
    //

    if (ArgumentCount > 4) {
        ReturnValue = EINVAL;
        goto mainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             MOUNT_OPTIONS_STRING,
                             MountLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            ReturnValue = 1;
            goto mainEnd;
        }

        switch (Option) {
        case 'b':
            Flags |= SYS_MOUNT_FLAG_BIND;
            break;

        case 'r':
            Flags |= SYS_MOUNT_FLAG_BIND | SYS_MOUNT_FLAG_RECURSIVE;
            break;

        case 'V':
            printf("mount version %d.%02d\n",
                   MOUNT_VERSION_MAJOR,
                   MOUNT_VERSION_MINOR);

            ReturnValue = 1;
            goto mainEnd;

        case 'h':
            printf(MOUNT_USAGE);
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
        TargetPath = Arguments[ArgumentIndex];
        if (ArgumentIndex + 1 < ArgumentCount) {
            MountPointPath = Arguments[ArgumentIndex + 1];
        }
    }

    //
    // If a source and target path were not filled in, something went wrong.
    //

    if ((TargetPath == NULL) || (MountPointPath == NULL)) {
        fprintf(stderr, "mount: Argument expected.\n");
        ReturnValue = EINVAL;
        goto mainEnd;
    }

    ReturnValue = Mount(MountPointPath, TargetPath, Flags);
    MountAttempted = TRUE;

mainEnd:
    if ((ReturnValue == EINVAL) && (MountAttempted == FALSE)) {
        printf(MOUNT_USAGE);
    }

    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
Mount (
    PSTR MountPointPath,
    PSTR TargetPath,
    ULONG Flags
    )

/*++

Routine Description:

    This routine mounts the target path at the given directory path.

Arguments:

    MountPointPath - Supplies a pointer to the mount point path where the
        target is to be mounted.

    TargetPath - Supplies a pointer to the path of the target file, directory,
        volume, or device that is to be mounted.

    Flags - Supplies a bitmask of mount flags. See SYS_MOUNT_FLAG_* for
        definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT ReturnValue;
    KSTATUS Status;

    ReturnValue = 0;
    Status = OsMount(MountPointPath,
                     RtlStringLength(MountPointPath) + 1,
                     TargetPath,
                     RtlStringLength(TargetPath) + 1,
                     Flags);

    if (!KSUCCESS(Status)) {
        ReturnValue = ClConvertKstatusToErrorNumber(Status);
        fprintf(stderr,
                "Error: failed to mount %s at %s with status %d: %s.\n",
                MountPointPath,
                TargetPath,
                Status,
                strerror(ReturnValue));
    }

    return ReturnValue;
}

INT
PrintMountPoints (
    VOID
    )

/*++

Routine Description:

    This routine prints the mount points visible to the current process.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID Buffer;
    UINTN BufferSize;
    ULONG Flags;
    PMOUNT_POINT_ENTRY MountPointEntry;
    PSTR MountPointPath;
    PVOID Offset;
    INT Result;
    KSTATUS Status;
    PSTR TargetPath;

    Status = OsGetMountPoints(&Buffer, &BufferSize);
    if (!KSUCCESS(Status)) {
        Result = ClConvertKstatusToErrorNumber(Status);
        fprintf(stderr,
                "Error: failed to print mounts with status %d: %s.\n",
                Status,
                strerror(Result));

        return Result;
    }

    //
    // Loop through the buffer, printing mount points.
    //

    Offset = Buffer;
    while (BufferSize != 0) {
        MountPointEntry = (PMOUNT_POINT_ENTRY)Offset;
        TargetPath = (PVOID)MountPointEntry +
                     MountPointEntry->TargetPathOffset;

        MountPointPath = (PVOID)MountPointEntry +
                         MountPointEntry->MountPointPathOffset;

        printf("%s on %s", TargetPath, MountPointPath);
        if (MountPointEntry->Flags != 0) {
            Flags = MountPointEntry->Flags;
            printf(" (");
            while (Flags != 0) {
                if ((Flags & SYS_MOUNT_FLAG_READ) != 0) {
                    printf("r");
                    Flags &= ~SYS_MOUNT_FLAG_READ;

                } else if ((Flags & SYS_MOUNT_FLAG_WRITE) != 0) {
                    printf("rw");
                    Flags &= ~SYS_MOUNT_FLAG_WRITE;

                } else if ((Flags & SYS_MOUNT_FLAG_BIND) != 0) {
                    if ((Flags & SYS_MOUNT_FLAG_RECURSIVE) != 0) {
                        printf("rbind");
                        Flags &= ~SYS_MOUNT_FLAG_RECURSIVE;

                    } else {
                        printf("bind");
                    }

                    Flags &= ~SYS_MOUNT_FLAG_BIND;

                } else if ((Flags & SYS_MOUNT_FLAG_TARGET_UNLINKED) != 0) {
                    printf("deleted");
                    Flags &= ~SYS_MOUNT_FLAG_TARGET_UNLINKED;
                }

                if (Flags != 0) {
                   printf(", ");
                }
            }

            printf(")");
        }

        printf("\n");
        Offset = (PVOID)TargetPath + RtlStringLength(TargetPath) + 1;
        BufferSize -= (ULONGLONG)(Offset - (PVOID)MountPointEntry);
    }

    OsHeapFree(Buffer);
    return 0;
}

