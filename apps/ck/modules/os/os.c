/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    os.c

Abstract:

    This module implements the Chalk os module, which provides functionality
    from the underlying operating system.

Author:

    Evan Green 28-Aug-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "osp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpOsFork (
    PCK_VM Vm
    );

VOID
CkpOsWaitPid (
    PCK_VM Vm
    );

VOID
CkpOsExit (
    PCK_VM Vm
    );

VOID
CkpOsNproc (
    PCK_VM Vm
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkOsModuleValues[] = {
    {CkTypeInteger, "WNOHANG", NULL, WNOHANG},
    {CkTypeInteger, "WUNTRACED", NULL, WUNTRACED},
    {CkTypeInteger, "WCONTINUED", NULL, WCONTINUED},
    {CkTypeFunction, "fork", CkpOsFork, 0},
    {CkTypeFunction, "waitpid", CkpOsWaitPid, 2},
    {CkTypeFunction, "exit", CkpOsExit, 1},
    {CkTypeFunction, "nproc", CkpOsNproc, 0},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadOsModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the OS module. It is called to make the presence of
    the os module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CkPreloadForeignModule(Vm, "os", NULL, NULL, CkpOsModuleInit);
}

VOID
CkpOsModuleInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine populates the OS module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    //
    // Define the OsError exception.
    //

    CkPushString(Vm, "OsError", 7);
    CkGetVariable(Vm, 0, "Exception");
    CkPushClass(Vm, 0, 0);
    CkSetVariable(Vm, 0, "OsError");

    //
    // Register the functions and definitions.
    //

    CkDeclareVariables(Vm, 0, CkOsErrnoValues);
    CkDeclareVariables(Vm, 0, CkOsIoModuleValues);
    CkDeclareVariables(Vm, 0, CkOsUserValues);
    CkDeclareVariables(Vm, 0, CkOsModuleValues);
    CkpOsInitializeInfo(Vm);
    return;
}

VOID
CkpOsFork (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the fork call. It takes no parameters. In the child
    forked process, it returns 0. In the parent process, it returns the pid of
    the child. On error and on Windows an exception is raised.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns 0 in the child, or the pid in the parent. On failure, an exception
    is raised.

--*/

{

    pid_t Result;

    Result = fork();
    if (Result < 0) {
        CkpOsRaiseError(Vm, NULL);
        return;
    }

    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsWaitPid (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the waitpid call. It takes two parameters: a pid
    to wait for, and an integer bitfield of options to wait for.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns a list of [Pid, Status] on success. Status is either non-negative
    if the process exited, or negative if the process hit a signal (and either
    stopped or terminated). Status will be 0x1000 if the process is continued.

    Returns null if WNOHANG is specified and no children are ready.

    Raises an exception on failure and on Windows.

--*/

{

    pid_t Result;
    int Status;
    CK_INTEGER StatusValue;

    if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Status = 0;
    Result = waitpid(CkGetInteger(Vm, 1), &Status, CkGetInteger(Vm, 2));
    if (Result < 0) {
        CkpOsRaiseError(Vm, NULL);
        return;
    }

    if (Result == 0) {
        CkReturnNull(Vm);
        return;
    }

    CkPushList(Vm);
    CkPushInteger(Vm, Result);
    CkListSet(Vm, -2, 0);
    if (WIFEXITED(Status)) {
        StatusValue = WEXITSTATUS(Status);

    } else if (WIFSTOPPED(Status)) {
        StatusValue = -WSTOPSIG(Status);

    } else if (WIFSIGNALED(Status)) {
        StatusValue = -WTERMSIG(Status);

    } else if (WIFCONTINUED(Status)) {
        StatusValue = 0x1000;

    } else {
        CkpOsRaiseError(Vm, NULL);
        return;
    }

    CkPushInteger(Vm, StatusValue);
    CkListSet(Vm, -2, 1);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpOsExit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the exit call. It takes in an exit code, and does
    not return because the current process exits.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    This routine does not return. The process exits.

--*/

{

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    exit(CkGetInteger(Vm, 1));
    CkReturnInteger(Vm, -1LL);
    return;
}

VOID
CkpOsNproc (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine returns the number of processors online.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None. The routine will return the number of processors online, minimum 1.

--*/

{

    long Count;

    Count = sysconf(_SC_NPROCESSORS_ONLN);
    if (Count <= 0) {
        Count = 1;
    }

    CkReturnInteger(Vm, Count);
    return;
}

VOID
CkpOsRaiseError (
    PCK_VM Vm,
    PCSTR Path
    )

/*++

Routine Description:

    This routine raises an error associated with the current errno value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Path - Supplies an optional path.

Return Value:

    This routine does not return. The process exits.

--*/

{

    INT Error;
    PCSTR ErrorString;
    INT Length;
    CHAR LocalBuffer[2048];

    Error = errno;
    if (Path != NULL) {
        Length = snprintf(LocalBuffer,
                          sizeof(LocalBuffer),
                          "%s: %s",
                          Path,
                          strerror(Error));

        ErrorString = LocalBuffer;

    } else {
        ErrorString = strerror(Error);
        Length = strlen(ErrorString);
    }

    //
    // Create an OsError exception.
    //

    CkPushModule(Vm, "os");
    CkGetVariable(Vm, -1, "OsError");
    CkPushString(Vm, ErrorString, Length);
    CkCall(Vm, 1);

    //
    // Execute instance.errno = Error.
    //

    CkPushValue(Vm, -1);
    CkPushString(Vm, "errno", 5);
    CkPushInteger(Vm, Error);
    CkCallMethod(Vm, "__set", 2);
    CkStackPop(Vm);

    //
    // Also set instance.path if one was supplied.
    //

    CkPushValue(Vm, -1);
    CkPushString(Vm, "path", 4);
    if (Path != NULL) {
        CkPushString(Vm, Path, strlen(Path));

    } else {
        CkPushNull(Vm);
    }

    CkCallMethod(Vm, "__set", 2);
    CkStackPop(Vm);

    //
    // Raise the exception.
    //

    CkRaiseException(Vm, -1);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

