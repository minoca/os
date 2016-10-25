/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    linux.c

Abstract:

    This module implements Linux-specific support for running Swiss.

Author:

    Evan Green 19-Jan-2016

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

int
SwGetProcessIdList (
    pid_t *ProcessIdList,
    size_t *ProcessIdListSize
    )

/*++

Routine Description:

    This routine returns a list of identifiers for the currently running
    processes.

Arguments:

    ProcessIdList - Supplies a pointer to an array of process IDs that is
        filled in by the routine. NULL can be supplied to determine the
        required size.

    ProcessIdListSize - Supplies a pointer that on input contains the size of
        the process ID list, in bytes. On output, it contains the actual size
        of the process ID list needed, in bytes.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    errno = ENOSYS;
    return -1;
}

int
SwGetProcessInformation (
    pid_t ProcessId,
    PSWISS_PROCESS_INFORMATION *ProcessInformation
    )

/*++

Routine Description:

    This routine gets process information for the specified process.

Arguments:

    ProcessId - Supplies the ID of the process whose information is to be
        gathered.

    ProcessInformation - Supplies a pointer that receives a pointer to process
        information structure. The caller is expected to free the buffer.

Return Value:

    0 on success, or -1 on failure.

--*/

{

    errno = ENOSYS;
    return -1;
}

void
SwDestroyProcessInformation (
    PSWISS_PROCESS_INFORMATION ProcessInformation
    )

/*++

Routine Description:

    This routine destroys an allocated swiss process information structure.

Arguments:

    ProcessInformation - Supplies a pointer to the process informaiton to
        release.

Return Value:

    None.

--*/

{

    return;
}

int
SwResetSystem (
    SWISS_REBOOT_TYPE RebootType
    )

/*++

Routine Description:

    This routine resets the running system.

Arguments:

    RebootType - Supplies the type of reboot to perform.

Return Value:

    0 if the reboot was successfully requested.

    Non-zero on error.

--*/

{

    return ENOSYS;
}

int
SwCloseFrom (
    int Descriptor
    )

/*++

Routine Description:

    This routine closes all open file descriptors greater than or equal to
    the given descriptor.

Arguments:

    Descriptor - Supplies the minimum descriptor to close.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    int CurrentDescriptor;
    DIR *Directory;
    struct dirent *Entry;

    Directory = opendir("/proc/self/fd");
    if (Directory == NULL) {
        return -1;
    }

    while (TRUE) {
        Entry = readdir(Directory);
        if (Entry == NULL) {
            break;
        }

        if ((strcmp(Entry->d_name, ".") == 0) ||
            (strcmp(Entry->d_name, "..") == 0)) {

            continue;
        }

        if ((sscanf(Entry->d_name, "%d", &CurrentDescriptor) == 1) &&
            (CurrentDescriptor >= Descriptor) &&
            (CurrentDescriptor != dirfd(Directory))) {

            close(CurrentDescriptor);
        }
    }

    closedir(Directory);
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

