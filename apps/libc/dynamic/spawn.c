/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spawn.c

Abstract:

    This module implements support for the posix_spawn* family of functions.

Author:

    Evan Green 20-Jul-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _POSIX_SPAWN_ACTION {
    SpawnActionInvalid,
    SpawnActionOpen,
    SpawnActionDup2,
    SpawnActionClose,
} POSIX_SPAWN_ACTION, *PPOSIX_SPAWN_ACTION;

/*++

Structure Description:

    This structure stores an attribute related to spawn.

Members:

    Flags - Stores the flags associated with the attribute. See
        POSIX_SPAWN_* definitions.

    ProcessGroup - Stores the process group to set the child process to.

    SchedulerParameter - Stores the scheduling parameter to set in the child
        process.

    SchedulerPolicy - Stores the scheduler policy to set in the process.

    DefaultMask - Stores the mask of signals to return back to the default.

    SignalMask - Stores the mask of signals to block.

--*/

typedef struct __posix_spawnattr_t {
    short Flags;
    pid_t ProcessGroup;
    struct sched_param SchedulerParameter;
    int SchedulerPolicy;
    sigset_t DefaultMask;
    sigset_t SignalMask;
} POSIX_SPAWN_ATTRIBUTES, *PPOSIX_SPAWN_ATTRIBUTES;

/*++

Structure Description:

    This structure stores an open file spawn action.

Members:

    Descriptor - Stores the descriptor to set the opened file to.

    Path - Stores a pointer to the path to open.

    OpenFlags - Stores the open flags to open the path with.

    CreateMode - Stores the creation mode to set if the file was newly created.

--*/

typedef struct _POSIX_SPAWN_OPEN {
    INT Descriptor;
    PSTR Path;
    INT OpenFlags;
    mode_t CreateMode;
} POSIX_SPAWN_OPEN, *PPOSIX_SPAWN_OPEN;

/*++

Structure Description:

    This structure stores a dup2 file spawn action.

Members:

    Descriptor - Stores the descriptor to duplicate.

    NewDescriptor - Stores the new file descriptor to duplicate the source to.

--*/

typedef struct _POSIX_SPAWN_DUP2 {
    INT Descriptor;
    INT NewDescriptor;
} POSIX_SPAWN_DUP2, *PPOSIX_SPAWN_DUP2;

/*++

Structure Description:

    This structure stores a close file spawn action.

Members:

    Descriptor - Stores the descriptor to close.

--*/

typedef struct _POSIX_SPAWN_CLOSE {
    INT Descriptor;
} POSIX_SPAWN_CLOSE, *PPOSIX_SPAWN_CLOSE;

/*++

Structure Description:

    This structure stores a file attribute related to spawn.

Members:

    EntryList - Stores the head of the list of POSIX_SPAWN_FILE_ENTRY actions.

--*/

typedef struct __posix_spawn_file_actions_t {
    LIST_ENTRY EntryList;
} POSIX_SPAWN_FILE_ACTION, *PPOSIX_SPAWN_FILE_ACTION;

/*++

Structure Description:

    This structure stores a spawn file action entry.

Members:

    ListEntry - Stores pointers to the next and previous file actions.

    Action - Stores the action type.

    U - Stores the union of action type parameters.

        Open - Stores the parameters for an open action.

        Dup2 - Stores the parameters for a dup2 action.

        Close - Stores the parameters for a close action.

--*/

typedef struct _POSIX_SPAWN_FILE_ENTRY {
    LIST_ENTRY ListEntry;
    POSIX_SPAWN_ACTION Action;
    union {
        POSIX_SPAWN_OPEN Open;
        POSIX_SPAWN_DUP2 Dup2;
        POSIX_SPAWN_CLOSE Close;
    } U;

} POSIX_SPAWN_FILE_ENTRY, *PPOSIX_SPAWN_FILE_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ClpPosixSpawn (
    pid_t *ChildPid,
    const char *Path,
    PPOSIX_SPAWN_FILE_ACTION *FileActions,
    PPOSIX_SPAWN_ATTRIBUTES *Attributes,
    char *const Arguments[],
    char *const Environment[],
    BOOL UsePath
    );

INT
ClpProcessSpawnAttributes (
    PPOSIX_SPAWN_ATTRIBUTES Attributes
    );

INT
ClpProcessSpawnFileActions (
    PPOSIX_SPAWN_FILE_ACTION Actions
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
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
    )

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

{

    INT Status;

    Status = ClpPosixSpawn(ChildPid,
                           Path,
                           (PPOSIX_SPAWN_FILE_ACTION *)FileActions,
                           (PPOSIX_SPAWN_ATTRIBUTES *)Attributes,
                           Arguments,
                           Environment,
                           FALSE);

    return Status;
}

LIBC_API
int
posix_spawnp (
    pid_t *ChildPid,
    const char *File,
    const posix_spawn_file_actions_t *FileActions,
    const posix_spawnattr_t *Attributes,
    char *const Arguments[],
    char *const Environment[]
    )

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

{

    INT Status;

    Status = ClpPosixSpawn(ChildPid,
                           File,
                           (PPOSIX_SPAWN_FILE_ACTION *)FileActions,
                           (PPOSIX_SPAWN_ATTRIBUTES *)Attributes,
                           Arguments,
                           Environment,
                           TRUE);

    return Status;
}

//
// File action functions
//

LIBC_API
int
posix_spawn_file_actions_init (
    posix_spawn_file_actions_t *FileActions
    )

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

{

    PPOSIX_SPAWN_FILE_ACTION Action;

    Action = malloc(sizeof(POSIX_SPAWN_FILE_ACTION));
    if (Action == NULL) {
        *FileActions = NULL;
        return errno;
    }

    INITIALIZE_LIST_HEAD(&(Action->EntryList));
    *FileActions = Action;
    return 0;
}

LIBC_API
int
posix_spawn_file_actions_destroy (
    posix_spawn_file_actions_t *FileActions
    )

/*++

Routine Description:

    This routine destroys a set of posix spawn file actions.

Arguments:

    FileActions - Supplies a pointer to the file actions to destroy.

Return Value:

    0 always.

--*/

{

    PPOSIX_SPAWN_FILE_ACTION Action;
    PLIST_ENTRY CurrentEntry;
    PPOSIX_SPAWN_FILE_ENTRY Entry;

    Action = *FileActions;
    *FileActions = NULL;
    if (Action == NULL) {
        return 0;
    }

    CurrentEntry = Action->EntryList.Next;
    while (CurrentEntry != &(Action->EntryList)) {
        Entry = LIST_VALUE(CurrentEntry, POSIX_SPAWN_FILE_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        free(Entry);
    }

    free(Action);
    return 0;
}

LIBC_API
int
posix_spawn_file_actions_addopen (
    posix_spawn_file_actions_t *FileActions,
    int FileDescriptor,
    const char *Path,
    int OpenFlags,
    mode_t CreatePermissions
    )

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

{

    PPOSIX_SPAWN_FILE_ACTION Action;
    PPOSIX_SPAWN_FILE_ENTRY Entry;
    size_t PathLength;

    Action = *FileActions;
    if (FileDescriptor < 0) {
        return EBADF;
    }

    PathLength = strlen(Path);
    Entry = malloc(sizeof(POSIX_SPAWN_FILE_ENTRY) + PathLength + 1);
    if (Entry == NULL) {
        return ENOMEM;
    }

    Entry->Action = SpawnActionOpen;
    Entry->U.Open.Path = (PSTR)(Entry + 1);
    memcpy(Entry->U.Open.Path, Path, PathLength + 1);
    Entry->U.Open.Descriptor = FileDescriptor;
    Entry->U.Open.OpenFlags = OpenFlags;
    Entry->U.Open.CreateMode = CreatePermissions;
    INSERT_BEFORE(&(Entry->ListEntry), &(Action->EntryList));
    return 0;
}

LIBC_API
int
posix_spawn_file_actions_adddup2 (
    posix_spawn_file_actions_t *FileActions,
    int FileDescriptor,
    int DestinationDescriptor
    )

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

{

    PPOSIX_SPAWN_FILE_ACTION Action;
    PPOSIX_SPAWN_FILE_ENTRY Entry;

    Action = *FileActions;
    if ((FileDescriptor < 0) || (DestinationDescriptor < 0)) {
        return EBADF;
    }

    Entry = malloc(sizeof(POSIX_SPAWN_FILE_ENTRY));
    if (Entry == NULL) {
        return ENOMEM;
    }

    Entry->Action = SpawnActionDup2;
    Entry->U.Dup2.Descriptor = FileDescriptor;
    Entry->U.Dup2.NewDescriptor = DestinationDescriptor;
    INSERT_BEFORE(&(Entry->ListEntry), &(Action->EntryList));
    return 0;
}

LIBC_API
int
posix_spawn_file_actions_addclose (
    posix_spawn_file_actions_t *FileActions,
    int FileDescriptor
    )

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

{

    PPOSIX_SPAWN_FILE_ACTION Action;
    PPOSIX_SPAWN_FILE_ENTRY Entry;

    Action = *FileActions;
    if (FileDescriptor < 0) {
        return EBADF;
    }

    Entry = malloc(sizeof(POSIX_SPAWN_FILE_ENTRY));
    if (Entry == NULL) {
        return ENOMEM;
    }

    Entry->Action = SpawnActionClose;
    Entry->U.Dup2.Descriptor = FileDescriptor;
    INSERT_BEFORE(&(Entry->ListEntry), &(Action->EntryList));
    return 0;
}

//
// Spawn attribute functions
//

LIBC_API
int
posix_spawnattr_init (
    posix_spawnattr_t *Attributes
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES NewAttributes;

    NewAttributes = calloc(sizeof(POSIX_SPAWN_ATTRIBUTES), 1);
    if (NewAttributes == NULL) {
        *Attributes = NULL;
        return -1;
    }

    *Attributes = NewAttributes;
    return 0;
}

LIBC_API
int
posix_spawnattr_destroy (
    posix_spawnattr_t *Attributes
    )

/*++

Routine Description:

    This routine destroys a set of spawn attributes.

Arguments:

    Attributes - Supplies a pointer to the attributes to destroy.

Return Value:

    0 always.

--*/

{

    free(*Attributes);
    return 0;
}

//
// Spawn attribute get functions
//

LIBC_API
int
posix_spawnattr_getflags (
    const posix_spawnattr_t *Attributes,
    short *Flags
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    *Flags = SpawnAttributes->Flags;
    return 0;
}

LIBC_API
int
posix_spawnattr_getpgroup (
    const posix_spawnattr_t *Attributes,
    pid_t *ProcessGroup
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    *ProcessGroup = SpawnAttributes->ProcessGroup;
    return 0;
}

LIBC_API
int
posix_spawnattr_getschedparam (
    const posix_spawnattr_t *Attributes,
    struct sched_param *Parameters
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    *Parameters = SpawnAttributes->SchedulerParameter;
    return 0;
}

LIBC_API
int
posix_spawnattr_getschedpolicy (
    const posix_spawnattr_t *Attributes,
    int *Policy
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    *Policy = SpawnAttributes->SchedulerPolicy;
    return 0;
}

LIBC_API
int
posix_spawnattr_getsigdefault (
    const posix_spawnattr_t *Attributes,
    sigset_t *DefaultSignals
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    *DefaultSignals = SpawnAttributes->DefaultMask;
    return 0;
}

LIBC_API
int
posix_spawnattr_getsigmask (
    const posix_spawnattr_t *Attributes,
    sigset_t *Mask
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    *Mask = SpawnAttributes->SignalMask;
    return 0;
}

//
// Spawn attribute set functions
//

LIBC_API
int
posix_spawnattr_setflags (
    posix_spawnattr_t *Attributes,
    short Flags
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    SpawnAttributes->Flags = Flags;
    return 0;
}

LIBC_API
int
posix_spawnattr_setpgroup (
    posix_spawnattr_t *Attributes,
    pid_t ProcessGroup
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    SpawnAttributes->ProcessGroup = ProcessGroup;
    return 0;
}

LIBC_API
int
posix_spawnattr_setschedparam (
    posix_spawnattr_t *Attributes,
    const struct sched_param *Parameters
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    SpawnAttributes->SchedulerParameter = *Parameters;
    return 0;
}

LIBC_API
int
posix_spawnattr_setschedpolicy (
    posix_spawnattr_t *Attributes,
    int Policy
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    SpawnAttributes->SchedulerPolicy = Policy;
    return 0;
}

LIBC_API
int
posix_spawnattr_setsigdefault (
    posix_spawnattr_t *Attributes,
    const sigset_t *DefaultSignals
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    SpawnAttributes->DefaultMask = *DefaultSignals;
    return 0;
}

LIBC_API
int
posix_spawnattr_setsigmask (
    posix_spawnattr_t *Attributes,
    const sigset_t *Mask
    )

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

{

    PPOSIX_SPAWN_ATTRIBUTES SpawnAttributes;

    SpawnAttributes = *Attributes;
    SpawnAttributes->SignalMask = *Mask;
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ClpPosixSpawn (
    pid_t *ChildPid,
    const char *Path,
    PPOSIX_SPAWN_FILE_ACTION *FileActions,
    PPOSIX_SPAWN_ATTRIBUTES *Attributes,
    char *const Arguments[],
    char *const Environment[],
    BOOL UsePath
    )

/*++

Routine Description:

    This routine executes the posix spawn function.

Arguments:

    ChildPid - Supplies an optional pointer where the child process ID will be
        returned on success.

    Path - Supplies a pointer to the file path to execute.

    FileActions - Supplies an optional pointer to the file actions to execute
        in the child.

    Attributes - Supplies an optional pointer to the spawn attributes.

    Arguments - Supplies the arguments to pass to the new child.

    Environment - Supplies the environment to pass to the new child.

    UsePath - Supplies a boolean indicating whether to use the exec*p functions
        to find the path of the executable or just the regular exec function.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    volatile int Error;
    pid_t Pid;

    //
    // TODO: Use vfork when it is implemented.
    //

    Error = 0;
    Pid = fork();
    if (Pid == -1) {
        return errno;

    //
    // In the child, process the attributes and execute the image. With vfork,
    // the values set in error here will be accessible by the parent, since
    // vfork temporarily shares the memory of the parent.
    //

    } else if (Pid == 0) {
        if (Attributes != NULL) {
            Error = ClpProcessSpawnAttributes(*Attributes);
            if (Error != 0) {
                _exit(127);
            }
        }

        if (FileActions != NULL) {
            Error = ClpProcessSpawnFileActions(*FileActions);
            if (Error != 0) {
                _exit(127);
            }
        }

        if (Environment == NULL) {
            Environment = environ;
        }

        if (UsePath != FALSE) {
            execvpe(Path, Arguments, Environment);

        } else {
            execve(Path, Arguments, Environment);
        }

        //
        // Oops, getting this far means exec didn't succeed. Fail.
        //

        Error = errno;
        _exit(127);

    //
    // In the parent, just return the child.
    //

    } else {

        //
        // If the child had a problem, then with vfork the error variable will
        // be set, and this routine can return a more detailed status. Reap the
        // child here.
        //

        if (Error != 0) {
            waitpid(Pid, NULL, 0);

        } else {
            if (ChildPid != NULL) {
                *ChildPid = Pid;
            }
        }
    }

    return Error;
}

INT
ClpProcessSpawnAttributes (
    PPOSIX_SPAWN_ATTRIBUTES Attributes
    )

/*++

Routine Description:

    This routine performs the actions specified by the given posix spawn
    attributes.

Arguments:

    Attributes - Supplies a pointer to the attributes to put into effect.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    struct sigaction Action;
    int Signal;

    if ((Attributes->Flags & POSIX_SPAWN_SETPGROUP) != 0) {
        if (setpgid(0, Attributes->ProcessGroup) != 0) {
            return errno;
        }
    }

    //
    // TODO: Set the scheduler policy and scheduler parameter.
    //

    if ((Attributes->Flags & POSIX_SPAWN_RESETIDS) != 0) {
        if (setegid(getgid()) != 0) {
            return errno;
        }

        if (seteuid(getuid()) != 0) {
            return errno;
        }
    }

    if ((Attributes->Flags & POSIX_SPAWN_SETSIGMASK) != 0) {
        if (sigprocmask(SIG_SETMASK, &(Attributes->SignalMask), NULL) != 0) {
            return errno;
        }
    }

    //
    // If desired, reset any signals mentioned in the default mask back to
    // the default disposition.
    //

    if ((Attributes->Flags & POSIX_SPAWN_SETSIGDEF) != 0) {
        memset(&Action, 0, sizeof(Action));
        Action.sa_handler = SIG_DFL;
        for (Signal = 1; Signal < NSIG; Signal += 1) {
            if (sigismember(&(Attributes->DefaultMask), Signal) != 0) {
                if (sigaction(Signal, &Action, NULL) != 0) {
                    return errno;
                }
            }
        }
    }

    return 0;
}

INT
ClpProcessSpawnFileActions (
    PPOSIX_SPAWN_FILE_ACTION Actions
    )

/*++

Routine Description:

    This routine performs the actions specified by the given posix spawn
    file actions.

Arguments:

    Actions - Supplies a pointer to the actions to perform.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    INT Descriptor;
    PPOSIX_SPAWN_FILE_ENTRY Entry;

    CurrentEntry = Actions->EntryList.Next;
    while (CurrentEntry != &(Actions->EntryList)) {
        Entry = LIST_VALUE(CurrentEntry, POSIX_SPAWN_FILE_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        switch (Entry->Action) {
        case SpawnActionOpen:
            Descriptor = open(Entry->U.Open.Path,
                              Entry->U.Open.OpenFlags,
                              Entry->U.Open.CreateMode);

            if (Descriptor < 0) {
                return errno;
            }

            if (Descriptor != Entry->U.Open.Descriptor) {
                if (dup2(Descriptor, Entry->U.Open.Descriptor) < 0) {
                    close(Descriptor);
                    return errno;
                }

                close(Descriptor);
            }

            break;

        case SpawnActionDup2:
            if (dup2(Entry->U.Dup2.Descriptor, Entry->U.Dup2.NewDescriptor) <
                0) {

                return errno;
            }

            if (fcntl(Entry->U.Dup2.NewDescriptor, F_SETFD, 0) < 0) {
                return errno;
            }

            break;

        case SpawnActionClose:
            close(Entry->U.Close.Descriptor);
            break;

        default:

            assert(FALSE);

            return EINVAL;
        }
    }

    return 0;
}

