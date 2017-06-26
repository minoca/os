/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spawnos.h

Abstract:

    This header contains definitions for the spawn module.

Author:

    Evan Green 21-Jun-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#ifdef _WIN32

#include "spnwin32.h"

#define SPAWN_DEVNULL_PATH "nul"

#else

#define SPAWN_DEVNULL_PATH "/dev/null"

#endif

#define SPAWN_NONE -1LL
#define SPAWN_DEVNULL -2LL
#define SPAWN_PIPE -3LL

#define SPAWN_OPTION_SHELL       0x00000001
#define SPAWN_OPTION_CHECK       0x00000002
#define SPAWN_OPTION_CLOSE_FDS   0x00000004
#define SPAWN_OPTION_NEW_SESSION 0x00000008

#define CK_SPAWN_MAX_OUTPUT (1024 * 1024 * 1024)

#define SPAWN_DEBUG_BASIC_LAUNCH 0x00000001
#define SPAWN_DEBUG_DETAILED_LAUNCH 0x00000002
#define SPAWN_DEBUG_IO 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores a standard descriptor for a child.

Members:

    Fd - Stores the child descriptor number to use, or -1 for no change.

    ParentPipe - Stores the parent's side of the pipe if this descriptor is
        piped.

    CloseFd - Stores a file descriptor that needs to be closed if the child
        process isn't fully created.

--*/

typedef struct _SPAWN_DESCRIPTOR {
    int Fd;
    int ParentPipe;
    int CloseFd;
} SPAWN_DESCRIPTOR, *PSPAWN_DESCRIPTOR;

/*++

Structure Description:

    This structure stores the attributes passed when creating a new process.

Members:

    Stdin - Stores the file descriptor for stdin.

    Stdout - Stores the file descriptor for stdout.

    Stderr - Stores the file descriptor for stderr.

    Cwd - Stores an optional pointer to the working directory to switch to, or
        NULL for no change.

    Environment - Stores an optional pointer to an environment array, or NULL
        for no change.

    PassFds - Stores an optional pointer to a set of file descriptors not to
        close.

    PassFdCount - Stores the number of elements in the PassFds array.

    Arguments - Stores a pointer to the array of arguments to execute.

    Executable - Stores the executable to execute.

    Pid - Stores the returned pid of the new process.

    ProcessHandle - Stores the handle to the process (Windows only).

    Options - Stores the spawn options. See SPAWN_OPTION_* definitions.

    ErrorMessage - Stores the error message. This must be freed if populated.

    ReturnCode - Stores the return code populated when the process exits.

    Debug - Stores the debug mask, used to print more information to stderr.

--*/

typedef struct _SPAWN_ATTRIBUTES {
    SPAWN_DESCRIPTOR Stdin;
    SPAWN_DESCRIPTOR Stdout;
    SPAWN_DESCRIPTOR Stderr;
    const char *Cwd;
    char **Environment;
    int *PassFds;
    size_t PassFdCount;
    char **Arguments;
    const char *Executable;
    pid_t Pid;
    void *ProcessHandle;
    int Options;
    char *ErrorMessage;
    int ReturnCode;
    int Debug;
} SPAWN_ATTRIBUTES, *PSPAWN_ATTRIBUTES;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// OS-specific functions.
//

INT
CkpOsSpawn (
    PSPAWN_ATTRIBUTES Attributes
    );

/*++

Routine Description:

    This routine spawns a subprocess.

Arguments:

    Attributes - Supplies the attributes of the process to launch.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
CkpOsWait (
    PSPAWN_ATTRIBUTES Attributes,
    int Milliseconds
    );

/*++

Routine Description:

    This routine waits for the process to exit. It sets the return code if
    the process exited, and sets the return value.

Arguments:

    Attributes - Supplies a pointer to the attributes.

    Milliseconds - Supplies the number of milliseconds to wait.

Return Value:

    0 on success.

    1 on timeout.

    -1 on failure.

--*/

INT
CkpOsCommunicate (
    PSPAWN_ATTRIBUTES Attributes,
    const char *Input,
    size_t InputSize,
    int Milliseconds,
    char **OutData,
    size_t *OutDataSize,
    char **ErrorData,
    size_t *ErrorDataSize
    );

/*++

Routine Description:

    This routine communicates with the subprocess, and waits for it to
    terminate.

Arguments:

    Attributes - Supplies a pointer to the attributes.

    Input - Supplies an optional pointer to the input to send into the
        process.

    InputSize - Supplies the size of the input data in bytes.

    Milliseconds - Supplies the number of milliseconds to wait.

    OutData - Supplies a pointer where the data from stdout will be returned.
        The caller is responsible for freeing this buffer.

    OutDataSize - Supplies the number of bytes in the output data buffer.

    ErrorData - Supplies a pointer where the data from stderr will be returned.
        The caller is responsible for freeing this buffer.

    ErrorDataSize - Supplies a pointer where the size of the stderr data will
        be returned.

Return Value:

    0 on success.

    1 on timeout.

    -1 on failure.

--*/

INT
CkpOsSendSignal (
    PSPAWN_ATTRIBUTES Attributes,
    INT Signal
    );

/*++

Routine Description:

    This routine sends a signal to the process. On Windows, it calls
    TerminateProcess for SIGTERM and SIGKILL.

Arguments:

    Attributes - Supplies a pointer to the attributes.

    Signal - Supplies the signal to send to the process.

Return Value:

    0 on success.

    -1 on failure.

--*/

VOID
CkpOsTearDownSpawnAttributes (
    PSPAWN_ATTRIBUTES Attributes
    );

/*++

Routine Description:

    This routine closes all OS-specific resources associated with a spawn
    attributes structure.

Arguments:

    Attributes - Supplies a pointer to the attributes to tear down.

Return Value:

    None.

--*/

