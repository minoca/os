/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ipc.h

Abstract:

    This header contains definitions for Inter-Process Communications in the
    C library.

Author:

    Evan Green 24-Mar-2017

--*/

#ifndef _SYS_IPC_H
#define _SYS_IPC_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// --------------------------------------------------------------------- Macros
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ---------------------------------------------------------------- Definitions
//

//
// Define flags for msgget, shmget, and shmget.
//

//
// Set this flag to create the object if it does not already exist.
//

#define IPC_CREAT 0x1000

//
// Set this flag in conjunction with IPC_CREAT to create the object and fail if
// it already exists.
//

#define IPC_EXCL  0x2000

//
// Define control commands for msgctl, semctl, and shmctl.
//

//
// Mark the object for deletion once all open references on it are closed.
//

#define IPC_RMID    1

//
// Set permission and ownership information on the object.
//

#define IPC_SET     2

//
// Get information about the object.
//

#define IPC_STAT    3

//
// Define the key value for a mapping that's always created.
//

#define IPC_PRIVATE ((key_t)0)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the permission information for an IPC shared object.

Members:

    uid - Stores the owner's user ID.

    gid - Stores the owner's group ID.

    cuid - Stores the creator's user ID.

    cgid - Stores the creator's group ID.

    mode - Stores the permission bits.

--*/

struct ipc_perm {
    uid_t uid;
    gid_t gid;
    uid_t cuid;
    gid_t cgid;
    unsigned int mode;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
key_t
ftok (
    const char *Path,
    int ProjectId
    );

/*++

Routine Description:

    This routine uses the identify of the given file and the least significant
    8 bits of the given project identifier to create a key suitable for use in
    the shmget, semget, or msgget functions.

Arguments:

    Path - Supplies a pointer to the path to the file whose identity should be
        involved in the key ID.

    ProjectId - Supplies an identifier whose least significant 8 bits will be
        worked into the result.

Return Value:

    Returns a key value suitable for use as a parameter to shmget, semget, or
    msgget.

    -1 on error, and errno will be set to the stat error information for the
    given file path.

--*/

#ifdef __cplusplus

}

#endif
#endif

