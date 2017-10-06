/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    spawn.h

Abstract:

    This header contains definitions for the posix_spawn

Author:

    Evan Green 20-Jul-2016

--*/

#ifndef _SPAWN_H
#define _SPAWN_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Set this flag to have the child's effective user and group IDs set back to
// its real IDs.
//

#define POSIX_SPAWN_RESETIDS        0x00000001

//
// Set this flag to have the child's process group ID set as defined in the
// attributes. If no attribute was set and this flag is set, the process group
// ID will be set to the process ID of the new child.
//

#define POSIX_SPAWN_SETPGROUP       0x00000002

//
// Set this flag to set the scheduling parameter of the child process.
//

#define POSIX_SPAWN_SETSCHEDPARAM   0x00000004

//
// Set this flag to set the scheduling policy of the child process.
//

#define POSIX_SPAWN_SETSCHEDULER    0x00000008

//
// Set this flag to reset the signals defined in the setsigdefault attribute
// back to their default disposition.
//

#define POSIX_SPAWN_SETSIGDEF       0x00000010

//
// Set this flag to set the signal mask specified in the setsigmask attribute
// in the child process.
//

#define POSIX_SPAWN_SETSIGMASK      0x00000020

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the opaque attribute types. Users should use the init/destroy and
// get/set methods defined below in this file to manipulate these types.
//

typedef struct __posix_spawnattr_t *posix_spawnattr_t;
typedef struct __posix_spawn_file_actions_t *posix_spawn_file_actions_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
posix_spawn (
    pid_t *ChildPid,
    const char *Path,
    const posix_spawn_file_actions_t *FileActions,
    const posix_spawnattr_t *Attributes,
    char *const Arguments[],
    char *const Environment[]
    );

/*++

Routine Description:

    This routine spawns a new child process.

Arguments:

    ChildPid - Supplies an optional pointer where the child process ID will be
        returned on success.

    Path - Supplies a pointer to the file path to execute.

    FileActions - Supplies an optional pointer to the file actions to execute
        in the child.

    Attributes - Supplies an optional pointer to the spawn attributes that
        affect various properties of the child.

    Arguments - Supplies the arguments to pass to the new child.

    Environment - Supplies an optional pointer to the environment to pass
        to the new child.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnp (
    pid_t *ChildPid,
    const char *File,
    const posix_spawn_file_actions_t *FileActions,
    const posix_spawnattr_t *Attributes,
    char *const Arguments[],
    char *const Environment[]
    );

/*++

Routine Description:

    This routine spawns a new child process. It is identical to posix_spawn
    except the path is searched to find the file argument.

Arguments:

    ChildPid - Supplies an optional pointer where the child process ID will be
        returned on success.

    File - Supplies a pointer to the file to execute. If this path contains a
        slash, it will be used as a relative path from the current directory
        or an absolute path. If this path does not contain a slash, it will
        use the PATH environment variable from the new child environment
        to attempt to find the path.

    FileActions - Supplies an optional pointer to the file actions to execute
        in the child.

    Attributes - Supplies an optional pointer to the spawn attributes that
        affect various properties of the child.

    Arguments - Supplies the arguments to pass to the new child.

    Environment - Supplies an optional pointer to the environment to pass
        to the new child.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

//
// File action functions
//

LIBC_API
int
posix_spawn_file_actions_init (
    posix_spawn_file_actions_t *FileActions
    );

/*++

Routine Description:

    This routine initializes a set of posix spawn file action.

Arguments:

    FileActions - Supplies a pointer to the file actions to initialize.

Return Value:

    0 on success. The caller must call the corresponding destroy routine to
    avoid leaking resources.

    Returns an error number on failure. The caller should not call destroy on
    this object.

--*/

LIBC_API
int
posix_spawn_file_actions_destroy (
    posix_spawn_file_actions_t *FileActions
    );

/*++

Routine Description:

    This routine destroys a set of posix spawn file actions.

Arguments:

    FileActions - Supplies a pointer to the file actions to destroy.

Return Value:

    0 always.

--*/

LIBC_API
int
posix_spawn_file_actions_addopen (
    posix_spawn_file_actions_t *FileActions,
    int FileDescriptor,
    const char *Path,
    int OpenFlags,
    mode_t CreatePermissions
    );

/*++

Routine Description:

    This routine adds an open call to the set of file attributes. The spawn
    function will attempt to open the given file in the child.

Arguments:

    FileActions - Supplies a pointer to the initialized file actions.

    FileDescriptor - Supplies the descriptor number to set the open file to.

    Path - Supplies a pointer to the path to open.

    OpenFlags - Supplies the set of open flags to use when opening the file.
        See O_* flags or the definition of the open function for details.

    CreatePermissions - Supplies the permissions to set on the new file if it
        is creates. See S_I* definitions or the definition of the open function
        for details.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawn_file_actions_adddup2 (
    posix_spawn_file_actions_t *FileActions,
    int FileDescriptor,
    int DestinationDescriptor
    );

/*++

Routine Description:

    This routine adds a dup2 call to the set of file attributes. The spawn
    function will attempt to duplicate the given descriptor in the child.

Arguments:

    FileActions - Supplies a pointer to the initialized file actions.

    FileDescriptor - Supplies the descriptor to copy.

    DestinationDescriptor - Supplies the descriptor number to copy the
        descriptor to.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawn_file_actions_addclose (
    posix_spawn_file_actions_t *FileActions,
    int FileDescriptor
    );

/*++

Routine Description:

    This routine adds a close call to the set of file attributes. The spawn
    function will attempt to close the given descriptor in the child.

Arguments:

    FileActions - Supplies a pointer to the initialized file actions.

    FileDescriptor - Supplies the descriptor to close.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

//
// Spawn attribute functions
//

LIBC_API
int
posix_spawnattr_init (
    posix_spawnattr_t *Attributes
    );

/*++

Routine Description:

    This routine initializes a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the attributes to initialize.

Return Value:

    0 on success. The caller must call the corresponding destroy routine to
    avoid leaking resources.

    Returns an error number on failure. The caller should not call destroy in
    this case.

--*/

LIBC_API
int
posix_spawnattr_destroy (
    posix_spawnattr_t *Attributes
    );

/*++

Routine Description:

    This routine destroys a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the attributes to destroy.

Return Value:

    0 always.

--*/

//
// Spawn attribute get functions
//

LIBC_API
int
posix_spawnattr_getflags (
    const posix_spawnattr_t *Attributes,
    short *Flags
    );

/*++

Routine Description:

    This routine returns the current flags on a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    Flags - Supplies a pointer where the flags will be returned on success.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_getpgroup (
    const posix_spawnattr_t *Attributes,
    pid_t *ProcessGroup
    );

/*++

Routine Description:

    This routine returns the current process group on a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    ProcessGroup - Supplies a pointer where the process group will be returned
        on success.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_getschedparam (
    const posix_spawnattr_t *Attributes,
    struct sched_param *Parameters
    );

/*++

Routine Description:

    This routine returns the current scheduling parameters on a set of spawn
    attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    Parameters - Supplies a pointer where the scheduling parameters will be
        returned on success.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_getschedpolicy (
    const posix_spawnattr_t *Attributes,
    int *Policy
    );

/*++

Routine Description:

    This routine returns the current scheduling policy on a set of spawn
    attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    Policy - Supplies a pointer where the scheduling policy will be returned on
        success.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_getsigdefault (
    const posix_spawnattr_t *Attributes,
    sigset_t *DefaultSignals
    );

/*++

Routine Description:

    This routine returns the current default signal set on a set of spawn
    attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    DefaultSignals - Supplies a pointer where the set of signals to be returned
        to their default dispositions will be returned on success.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_getsigmask (
    const posix_spawnattr_t *Attributes,
    sigset_t *Mask
    );

/*++

Routine Description:

    This routine returns the current signal mask on a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    Mask - Supplies a pointer where the signal mask to be set on the child
        process will be returned on success.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

//
// Spawn attribute set functions
//

LIBC_API
int
posix_spawnattr_setflags (
    posix_spawnattr_t *Attributes,
    short Flags
    );

/*++

Routine Description:

    This routine sets the current flags on a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    Flags - Supplies the new flags to set.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_setpgroup (
    posix_spawnattr_t *Attributes,
    pid_t ProcessGroup
    );

/*++

Routine Description:

    This routine sets the current process group on a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    ProcessGroup - Supplies the process group to set the child to.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_setschedparam (
    posix_spawnattr_t *Attributes,
    const struct sched_param *Parameters
    );

/*++

Routine Description:

    This routine sets the current scheduling parameters on a set of spawn
    attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    Parameters - Supplies a pointer to the scheduling parameters to set in the
        child.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_setschedpolicy (
    posix_spawnattr_t *Attributes,
    int Policy
    );

/*++

Routine Description:

    This routine sets the current scheduling policy on a set of spawn
    attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    Policy - Supplies the scheduling policy to set in the child.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_setsigdefault (
    posix_spawnattr_t *Attributes,
    const sigset_t *DefaultSignals
    );

/*++

Routine Description:

    This routine sets the current default signal set on a set of spawn
    attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    DefaultSignals - Supplies a pointer to the set of signals to return to
        their default dispositions in the child.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

LIBC_API
int
posix_spawnattr_setsigmask (
    posix_spawnattr_t *Attributes,
    const sigset_t *Mask
    );

/*++

Routine Description:

    This routine sets the current signal mask on a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the initialized attributes.

    Mask - Supplies a pointer to the signal mask to set in the child.

Return Value:

    0 on success (always).

    Returns an error number on failure.

--*/

#ifdef __cplusplus

}

#endif
#endif

