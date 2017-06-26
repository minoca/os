/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spawn.c

Abstract:

    This module implements the spawn module, which can be used to launch
    child processes from Chalk.

Author:

    Evan Green 21-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "spawnp.h"

//
// --------------------------------------------------------------------- Macros
//

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
CkpChildProcessInit (
    PCK_VM Vm
    );

VOID
CkpChildProcessGet (
    PCK_VM Vm
    );

VOID
CkpChildProcessSet (
    PCK_VM Vm
    );

VOID
CkpChildProcessLaunch (
    PCK_VM Vm
    );

VOID
CkpChildProcessPoll (
    PCK_VM Vm
    );

VOID
CkpChildProcessWait (
    PCK_VM Vm
    );

VOID
CkpChildProcessCommunicate (
    PCK_VM Vm
    );

VOID
CkpChildProcessTerminate (
    PCK_VM Vm
    );

VOID
CkpChildProcessKill (
    PCK_VM Vm
    );

INT
CkpSpawnGetDescriptor (
    PCK_VM Vm,
    PCSTR Name,
    PSPAWN_DESCRIPTOR Descriptor
    );

PSTR *
CkpSpawnCreateEnvironment (
    PCK_VM Vm
    );

INT
CkpSpawnGetStringList (
    PCK_VM Vm,
    PCSTR Name,
    BOOL Optional,
    char ***NewList
    );

INT
CkpSpawnWait (
    PCK_VM Vm,
    PSPAWN_ATTRIBUTES Attributes,
    INT Milliseconds
    );

INT
CkpSpawnSetReturnCode (
    PCK_VM Vm,
    PSPAWN_ATTRIBUTES Attributes
    );

VOID
CkpDestroySpawnAttributes (
    PVOID Attributes
    );

VOID
CkpTearDownSpawnAttributes (
    PSPAWN_ATTRIBUTES Attributes
    );

VOID
CkpSpawnRaiseSpawnError (
    PCK_VM Vm,
    PSPAWN_ATTRIBUTES Attributes
    );

VOID
CkpSpawnRaiseError (
    PCK_VM Vm,
    PCSTR ExceptionType,
    PCSTR Message
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkSpawnModuleValues[] = {
    {CkTypeInteger, "NONE", NULL, SPAWN_NONE},
    {CkTypeInteger, "DEVNULL", NULL, SPAWN_DEVNULL},
    {CkTypeInteger, "PIPE", NULL, SPAWN_PIPE},
    {CkTypeInteger, "OPTION_SHELL", NULL, SPAWN_OPTION_SHELL},
    {CkTypeInteger, "OPTION_CHECK", NULL, SPAWN_OPTION_CHECK},
    {CkTypeInteger, "OPTION_CLOSE_FDS", NULL, SPAWN_OPTION_CLOSE_FDS},
    {CkTypeInteger, "OPTION_NEW_SESSION", NULL, SPAWN_OPTION_NEW_SESSION},
    {CkTypeInteger, "DEBUG_BASIC_LAUNCH", NULL, SPAWN_DEBUG_BASIC_LAUNCH},
    {CkTypeInteger, "DEBUG_DETAILED_LAUNCH", NULL, SPAWN_DEBUG_DETAILED_LAUNCH},
    {CkTypeInteger, "DEBUG_IO", NULL, SPAWN_DEBUG_IO},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadSpawnModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the spawn module. It is called to make the presence
    of the os module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CkPreloadForeignModule(Vm, "spawn", NULL, NULL, CkpSpawnModuleInit);
}

VOID
CkpSpawnModuleInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine populates the spawn module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkPushString(Vm, "SpawnError", 10);
    CkGetVariable(Vm, 0, "Exception");
    CkPushClass(Vm, 0, 0);
    CkSetVariable(Vm, 0, "SpawnError");
    CkPushString(Vm, "TimeoutExpired", 14);
    CkGetVariable(Vm, 0, "Exception");
    CkPushClass(Vm, 0, 0);
    CkSetVariable(Vm, 0, "TimeoutExpired");
    CkPushString(Vm, "ProcessExited", 13);
    CkGetVariable(Vm, 0, "Exception");
    CkPushClass(Vm, 0, 0);
    CkSetVariable(Vm, 0, "ProcessExited");
    CkPushString(Vm, "ChildProcessError", 17);
    CkGetVariable(Vm, 0, "Exception");
    CkPushClass(Vm, 0, 0);
    CkSetVariable(Vm, 0, "ChildProcessError");

    //
    // Register the functions and definitions.
    //

    CkDeclareVariables(Vm, 0, CkSpawnModuleValues);

    //
    // Create the LzmaEncoder class.
    //

    CkPushString(Vm, "ChildProcess", 12);
    CkGetVariable(Vm, 0, "Object");
    CkPushClass(Vm, 0, 2);
    CkPushValue(Vm, -1);
    CkSetVariable(Vm, 0, "ChildProcess");
    CkPushFunction(Vm, CkpChildProcessInit, "__init", 0, 0);
    CkPushString(Vm, "__init", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessInit, "__init", 1, 0);
    CkPushString(Vm, "__init", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessGet, "__get", 1, 0);
    CkPushString(Vm, "__get", 5);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessSet, "__set", 2, 0);
    CkPushString(Vm, "__set", 5);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessLaunch, "launch", 0, 0);
    CkPushString(Vm, "launch", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessPoll, "poll", 0, 0);
    CkPushString(Vm, "poll", 4);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessWait, "wait", 1, 0);
    CkPushString(Vm, "wait", 4);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessCommunicate, "communicate", 2, 0);
    CkPushString(Vm, "communicate", 11);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessTerminate, "terminate", 0, 0);
    CkPushString(Vm, "terminate", 9);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpChildProcessGet, "kill", 0, 0);
    CkPushString(Vm, "kill", 4);
    CkBindMethod(Vm, 1);
    CkStackPop(Vm);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpChildProcessInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine initializes a new ChildProcess instance.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    UINTN ArgumentCount;
    PSPAWN_ATTRIBUTES Attributes;

    ArgumentCount = CkGetStackSize(Vm) - 1;

    //
    // Create the dict information, and push an extra copy of the dict.
    //

    CkPushDict(Vm);
    CkPushValue(Vm, -1);
    CkSetField(Vm, 0);

    //
    // Create the attributes structure.
    //

    Attributes = malloc(sizeof(SPAWN_ATTRIBUTES));
    if (Attributes == NULL) {
        CkRaiseBasicException(Vm, "MemoryError", "");
        return;
    }

    memset(Attributes, 0, sizeof(SPAWN_ATTRIBUTES));
    Attributes->Stdin.Fd = -1;
    Attributes->Stdin.CloseFd = -1;
    Attributes->Stdin.ParentPipe = -1;
    Attributes->Stdout.Fd = -1;
    Attributes->Stdout.CloseFd = -1;
    Attributes->Stdout.ParentPipe = -1;
    Attributes->Stderr.Fd = -1;
    Attributes->Stderr.CloseFd = -1;
    Attributes->Stderr.ParentPipe = -1;
    CkPushData(Vm, Attributes, CkpDestroySpawnAttributes);
    CkSetField(Vm, 1);

    //
    // Set the optional args.
    //

    CkPushString(Vm, "args", 4);
    if (ArgumentCount == 1) {
        CkPushValue(Vm, 1);

    } else {
        CkPushNull(Vm);
    }

    CkDictSet(Vm, -3);
    CkPushString(Vm, "stdin", 5);
    CkPushInteger(Vm, SPAWN_NONE);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "stdout", 6);
    CkPushInteger(Vm, SPAWN_NONE);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "stderr", 6);
    CkPushInteger(Vm, SPAWN_NONE);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "options", 7);
    CkPushInteger(Vm, 0);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "cwd", 3);
    CkPushNull(Vm);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "env", 3);
    CkPushNull(Vm);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "passFds", 7);
    CkPushNull(Vm);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "executable", 10);
    CkPushNull(Vm);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "return", 6);
    CkPushNull(Vm);
    CkDictSet(Vm, -3);
    CkPushString(Vm, "pid", 3);
    CkPushNull(Vm);
    CkDictSet(Vm, -3);
    return;
}

VOID
CkpChildProcessGet (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the __get function for the ChildProcess. It takes a
    key and returns a value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkGetField(Vm, 0);
    CkPushValue(Vm, 1);
    if (CkDictGet(Vm, 2) == FALSE) {
        CkRaiseBasicException(Vm, "KeyError", "Key not found");
        return;
    }

    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpChildProcessSet (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the __set function for the ChildProcess. It takes
    two arguments: a key and a value, and sets that value for the key in the
    ChildProcess. Returns null.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkGetField(Vm, 0);
    CkPushValue(Vm, 1);
    CkPushValue(Vm, 2);
    CkDictSet(Vm, 3);
    CkReturnNull(Vm);
    return;
}

VOID
CkpChildProcessLaunch (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine starts the child process, if it has not yet been started.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSPAWN_ATTRIBUTES Attributes;
    UINTN Index;
    INT Status;

    CkGetField(Vm, 1);
    Attributes = CkGetData(Vm, -1);
    CkStackPop(Vm);
    CkGetField(Vm, 0);

    //
    // If the process is already launched, don't launch it again, just return
    // the pid.
    //

    if (Attributes->Pid != 0) {
        if (Attributes->Pid < 0) {
            CkpSpawnRaiseError(Vm, "ProcessExited", "Process exited");

        } else {
            CkReturnInteger(Vm, Attributes->Pid);
        }

        return;
    }

    Status = CkpSpawnGetDescriptor(Vm, "stdin", &(Attributes->Stdin));
    if (Status != 0) {
        goto ChildProcessLaunchEnd;
    }

    Status = CkpSpawnGetDescriptor(Vm, "stdout", &(Attributes->Stdout));
    if (Status != 0) {
        goto ChildProcessLaunchEnd;
    }

    Status = CkpSpawnGetDescriptor(Vm, "stderr", &(Attributes->Stderr));
    if (Status != 0) {
        goto ChildProcessLaunchEnd;
    }

    CkPushString(Vm, "cwd", 3);
    if (CkDictGet(Vm, -2)) {
        Attributes->Cwd = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    Attributes->Environment = CkpSpawnCreateEnvironment(Vm);
    Status = CkpSpawnGetStringList(Vm, "args", FALSE, &(Attributes->Arguments));
    if (Status != 0) {
        goto ChildProcessLaunchEnd;
    }

    CkPushString(Vm, "executable", 10);
    if (CkDictGet(Vm, -2)) {
        Attributes->Executable = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    CkPushString(Vm, "options", 7);
    if (CkDictGet(Vm, -2)) {
        Attributes->Options = CkGetInteger(Vm, -1);
        CkStackPop(Vm);

        //
        // Consider implementing closing all other FDs if it's needed.
        //

        if ((Attributes->Options & SPAWN_OPTION_CLOSE_FDS) != 0) {
            CkpSpawnRaiseError(Vm,
                               "ValueError",
                               "CLOSE_FDS not currently implemented");

            Status = -1;
            goto ChildProcessLaunchEnd;
        }
    }

    CkPushString(Vm, "debug", 5);
    if (CkDictGet(Vm, -2)) {
        Attributes->Debug = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    //
    // Get the list of descriptors not to close, if present.
    //

    CkPushString(Vm, "passFds", 7);
    if (CkDictGet(Vm, -2)) {
        Attributes->PassFdCount = CkListSize(Vm, -1);
        Attributes->PassFds = malloc(sizeof(int) * Attributes->PassFdCount);
        if (Attributes->PassFds == NULL) {
            CkRaiseBasicException(Vm, "MemoryError", "");
            Status = -1;
            goto ChildProcessLaunchEnd;
        }

        for (Index = 0; Index < Attributes->PassFdCount; Index += 1) {
            CkListGet(Vm, -1, Index);
            if (!CkIsInteger(Vm, -1)) {
                CkRaiseBasicException(Vm,
                                      "TypeError",
                                      "Expected an integer in passFds");

                Status = -1;
                goto ChildProcessLaunchEnd;
            }

            Attributes->PassFds[Index] = CkGetInteger(Vm, -1);
            CkStackPop(Vm);
        }

        CkStackPop(Vm);
    }

    //
    // Call out to the OS-specific part to actually spawn the process.
    //

    Status = CkpOsSpawn(Attributes);
    if (Status != 0) {
        CkpSpawnRaiseSpawnError(Vm, Attributes);
        goto ChildProcessLaunchEnd;
    }

    //
    // Set the in/out/error pipe file descriptors.
    //

    if (Attributes->Stdin.ParentPipe >= 0) {
        CkPushString(Vm, "stdin", 5);
        CkPushInteger(Vm, Attributes->Stdin.ParentPipe);
        CkDictSet(Vm, -3);
    }

    if (Attributes->Stdout.ParentPipe >= 0) {
        CkPushString(Vm, "stdout", 6);
        CkPushInteger(Vm, Attributes->Stdout.ParentPipe);
        CkDictSet(Vm, -3);
    }

    if (Attributes->Stderr.ParentPipe >= 0) {
        CkPushString(Vm, "stderr", 6);
        CkPushInteger(Vm, Attributes->Stderr.ParentPipe);
        CkDictSet(Vm, -3);
    }

    CkStackPop(Vm);

ChildProcessLaunchEnd:
    if (Attributes->Environment != NULL) {
        free(Attributes->Environment);
        Attributes->Environment = NULL;
    }

    if (Attributes->Arguments != NULL) {
        free(Attributes->Arguments);
        Attributes->Arguments = NULL;
    }

    if (Attributes->PassFds != NULL) {
        free(Attributes->PassFds);
        Attributes->PassFds = NULL;
    }

    if (Attributes->ErrorMessage != NULL) {
        free(Attributes->ErrorMessage);
        Attributes->ErrorMessage = NULL;
    }

    if (Status != 0) {
        CkpTearDownSpawnAttributes(Attributes);

    } else {
        CkReturnInteger(Vm, Attributes->Pid);
    }

    return;
}

VOID
CkpChildProcessPoll (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine determines if the child process has exited yet, and sets the
    returncode if it has.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSPAWN_ATTRIBUTES Attributes;
    INT Status;

    CkGetField(Vm, 1);
    Attributes = CkGetData(Vm, -1);
    CkStackPop(Vm);
    Status = CkpSpawnWait(Vm, Attributes, 0);
    if (Status == 1) {
        CkReturnNull(Vm);
    }

    return;
}

VOID
CkpChildProcessWait (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine waits for the child process to exit, and returns its return
    code. On timeout, a TimeoutExpired error is raised. On failure, a
    SpawnError is raised.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSPAWN_ATTRIBUTES Attributes;
    INT Status;

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    CkGetField(Vm, 1);
    Attributes = CkGetData(Vm, -1);
    CkStackPop(Vm);
    Status = CkpSpawnWait(Vm, Attributes, CkGetInteger(Vm, 1));
    if (Status == 1) {
        CkpSpawnRaiseError(Vm, "TimeoutExpired", "Timeout expired");
        return;
    }

    return;
}

VOID
CkpChildProcessCommunicate (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine communicates with the child process. It takes two arguments:
    an optional input to send to the process and a timeout in milliseconds.
    Upon return, a list containing stdout and stderr data will be returned.
    The caller must have launched the child process with pipe options to get
    any data across. On timeout, a TimeoutExpired error is raised.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSPAWN_ATTRIBUTES Attributes;
    PSTR ErrorData;
    size_t ErrorDataSize;
    BOOL ExceptionRaised;
    PCSTR Input;
    UINTN InputSize;
    PSTR OutData;
    size_t OutDataSize;
    INT Status;
    CK_INTEGER Timeout;

    ErrorData = NULL;
    ErrorDataSize = 0;
    ExceptionRaised = FALSE;
    Input = NULL;
    InputSize = 0;
    OutData = NULL;
    OutDataSize = 0;
    Input = CkGetString(Vm, 1, &InputSize);
    Timeout = CkGetInteger(Vm, 2);
    CkGetField(Vm, 1);
    Attributes = CkGetData(Vm, -1);
    CkStackPop(Vm);
    Status = CkpOsCommunicate(Attributes,
                              Input,
                              InputSize,
                              Timeout,
                              &OutData,
                              &OutDataSize,
                              &ErrorData,
                              &ErrorDataSize);

    if (Attributes->Pid < 0) {
        if (CkpSpawnSetReturnCode(Vm, Attributes) < 0) {
            ExceptionRaised = TRUE;
            if (Attributes->ErrorMessage != NULL) {
                free(Attributes->ErrorMessage);
                Attributes->ErrorMessage = NULL;
            }
        }
    }

    if (ExceptionRaised == FALSE) {
        if (Status == 1) {
            CkpSpawnRaiseError(Vm, "TimeoutExpired", "Timeout expired");
            ExceptionRaised = TRUE;

        } else if (Status != 0) {
            CkpSpawnRaiseSpawnError(Vm, Attributes);
            ExceptionRaised = TRUE;

        } else if ((InputSize == 0) && (OutDataSize == 0) &&
                   (ErrorDataSize == 0)) {

            assert(Attributes->Pid < 0);

            CkpSpawnRaiseError(Vm, "ProcessExited", "Process Exited");
            ExceptionRaised = TRUE;
        }
    }

    if (ExceptionRaised == FALSE) {
        CkPushList(Vm);
        CkPushString(Vm, OutData, OutDataSize);
        CkListSet(Vm, -2, 0);
        CkPushString(Vm, ErrorData, ErrorDataSize);
        CkListSet(Vm, -2, 1);
        CkStackReplace(Vm, 0);
    }

    if (OutData != NULL) {
        free(OutData);
    }

    if (ErrorData != NULL) {
        free(ErrorData);
    }

    return;
}

VOID
CkpChildProcessTerminate (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine sends a SIGTERM to the child process. On Windows, it calls
    TerminateProcess.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSPAWN_ATTRIBUTES Attributes;
    INT Status;

    CkGetField(Vm, 1);
    Attributes = CkGetData(Vm, -1);
    CkStackPop(Vm);
    Status = CkpOsSendSignal(Attributes, SIGTERM);
    if (Status != 0) {
        CkpSpawnRaiseSpawnError(Vm, Attributes);
        return;
    }

    return;
}

VOID
CkpChildProcessKill (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine sends a SIGKILL to the child process. On Windows, it calls
    TerminateProcess.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSPAWN_ATTRIBUTES Attributes;
    INT Status;

    CkGetField(Vm, 1);
    Attributes = CkGetData(Vm, -1);
    CkStackPop(Vm);
    Status = CkpOsSendSignal(Attributes, SIGKILL);
    if (Status != 0) {
        CkpSpawnRaiseSpawnError(Vm, Attributes);
        return;
    }

    return;
}

INT
CkpSpawnGetDescriptor (
    PCK_VM Vm,
    PCSTR Name,
    PSPAWN_DESCRIPTOR Descriptor
    )

/*++

Routine Description:

    This routine gets or sets up a standard descriptor.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies the name of the descriptor to get within the field
        dictionary.

    Descriptor - Supplies a pointer where the descriptor information will be
        returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    CK_INTEGER Integer;
    int Pipe[2];

    CkGetField(Vm, 0);
    CkPushString(Vm, Name, strlen(Name));
    CkDictGet(Vm, -2);
    if (!CkIsInteger(Vm, -1)) {
        CkRaiseBasicException(Vm,
                              "TypeError",
                              "Expected an integer for %s",
                              Name);

        return -1;
    }

    Integer = CkGetInteger(Vm, -1);
    CkStackPop(Vm);
    CkStackPop(Vm);
    if (Integer == SPAWN_DEVNULL) {
        Descriptor->Fd = open(SPAWN_DEVNULL_PATH, O_RDWR);
        if (Descriptor->Fd < 0) {
            CkpSpawnRaiseError(Vm, "SpawnError", "Failed to open null device");
            return -1;
        }

        Descriptor->CloseFd = Descriptor->Fd;

    } else if (Integer == SPAWN_PIPE) {
        if (pipe(Pipe) != 0) {
            CkpSpawnRaiseError(Vm, "SpawnError", strerror(errno));
            return -1;
        }

        if (strcmp(Name, "stdin") == 0) {
            Descriptor->Fd = Pipe[0];
            Descriptor->ParentPipe = Pipe[1];

        } else {
            Descriptor->Fd = Pipe[1];
            Descriptor->ParentPipe = Pipe[0];
        }

        Descriptor->CloseFd = Descriptor->Fd;

    } else if (Integer > 0) {
        Descriptor->Fd = Integer;

    } else {
        Descriptor->Fd = -1;
    }

    return 0;
}

PSTR *
CkpSpawnCreateEnvironment (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine creates an environment from a dictionary.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    0 on success.

    -1 if an exception was raised.

--*/

{

    PSTR *Array;
    PSTR Buffer;
    UINTN Count;
    PSTR Current;
    PSTR End;
    UINTN Index;
    PCSTR Key;
    UINTN KeyLength;
    UINTN NeededSize;
    PSTR NewBuffer;
    UINTN NewCapacity;
    PCSTR Value;
    UINTN ValueLength;

    Array = NULL;
    Buffer = NULL;
    Current = NULL;
    End = NULL;
    CkGetField(Vm, 0);
    CkPushString(Vm, "env", 3);
    if (!CkDictGet(Vm, -2)) {
        CkStackPop(Vm);
        return NULL;
    }

    //
    // Iterate over all the keys in the dictionary.
    //

    Count = 0;
    CkPushNull(Vm);
    while (CkDictIterate(Vm, -2)) {
        Key = CkGetString(Vm, -2, &KeyLength);
        Value = CkGetString(Vm, -1, &ValueLength);
        CkStackPop(Vm);
        CkStackPop(Vm);
        if ((Key == NULL) || (Value == NULL) || (KeyLength == 0)) {
            continue;
        }

        //
        // Space is needed for key=value\0.
        //

        NeededSize = KeyLength + ValueLength + 2;
        if (Current + NeededSize > End) {
            if (Buffer == NULL) {
                NewCapacity = 1024;

            } else {
                NewCapacity = (End - Buffer) * 2;
            }

            while (NewCapacity < (Current - Buffer) + NeededSize) {
                NewCapacity *= 2;
            }

            NewBuffer = realloc(Buffer, NewCapacity);
            if ((NewCapacity >= CK_SPAWN_MAX_OUTPUT) || (NewBuffer == NULL)) {
                free(NewBuffer);
                free(Buffer);
                Buffer = NULL;
                goto SpawnCreateEnvironmentEnd;
            }

            Current = NewBuffer + (Current - Buffer);
            Buffer = NewBuffer;
            End = Buffer + NewCapacity;
        }

        Current += snprintf(Current, End - Current, "%s=%s", Key, Value) + 1;
        Count += 1;
    }

    if (Count == 0) {

        assert(Buffer == NULL);

        goto SpawnCreateEnvironmentEnd;
    }

    //
    // Create a single allocation containing enough space for the array and
    // all the entries.
    //

    Array = malloc(((Count + 1) * sizeof(PSTR)) + (Current - Buffer) + 1);
    if (Array == NULL) {
        goto SpawnCreateEnvironmentEnd;
    }

    //
    // Copy the contents in.
    //

    memcpy(&(Array[Count + 1]), Buffer, Current - Buffer);

    //
    // Assign the array elements within the string.
    //

    Current = (PSTR)&(Array[Count + 1]);
    for (Index = 0; Index < Count; Index += 1) {
        Array[Index] = Current;
        Current += strlen(Current) + 1;
    }

    Array[Index] = NULL;
    *Current = '\0';

SpawnCreateEnvironmentEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    //
    // Pop the iterator, dict, and field.
    //

    CkStackPop(Vm);
    CkStackPop(Vm);
    CkStackPop(Vm);
    return Array;
}

INT
CkpSpawnGetStringList (
    PCK_VM Vm,
    PCSTR Name,
    BOOL Optional,
    char ***NewList
    )

/*++

Routine Description:

    This routine creates a string list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies the name of the list to get within the field
        dictionary.

    Optional - Supplies a boolean indicating if the list is optional or not.

    NewList - Supplies a pointer where the array will be returned.

Return Value:

    0 on success.

    -1 if an exception was raised.

--*/

{

    char **Array;
    UINTN Index;
    UINTN Size;

    *NewList = NULL;
    CkGetField(Vm, 0);
    CkPushString(Vm, Name, strlen(Name));
    CkDictGet(Vm, -2);
    if ((Optional != FALSE) && (CkIsNull(Vm, -1))) {
        CkStackPop(Vm);
        CkStackPop(Vm);
        return 0;
    }

    if (!CkIsList(Vm, -1)) {
        CkRaiseBasicException(Vm,
                              "TypeError",
                              "Expected a list for %s",
                              Name);

        return -1;
    }

    Size = CkListSize(Vm, -1);
    if ((Optional == FALSE) && (Size == 0)) {
        CkRaiseBasicException(Vm,
                              "ValueError",
                              "Expected non-empty list for %s",
                              Name);

        return -1;
    }

    Array = malloc(sizeof(char *) * (Size + 1));
    if (Array == NULL) {
        CkRaiseBasicException(Vm, "MemoryError", "");
        return -1;
    }

    Array[Size] = NULL;
    for (Index = 0; Index < Size; Index += 1) {
        CkListGet(Vm, -1, Index);
        Array[Index] = (PSTR)CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
        if (Array[Index] == NULL) {
            CkRaiseBasicException(Vm,
                                  "TypeError",
                                  "Expected a string at index %d of %s",
                                  (int)Index,
                                  Name);

            break;
        }
    }

    CkStackPop(Vm);
    CkStackPop(Vm);
    if (Index != Size) {
        free(Array);
        return -1;
    }

    *NewList = Array;
    return 0;
}

INT
CkpSpawnWait (
    PCK_VM Vm,
    PSPAWN_ATTRIBUTES Attributes,
    INT Milliseconds
    )

/*++

Routine Description:

    This routine waits for the process to exit. It sets the return code if
    the process exited, and sets the return value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Attributes - Supplies a pointer to the attributes.

    Milliseconds - Supplies the number of milliseconds to wait.

Return Value:

    0 on success.

    1 on timeout.

    Non-zero if an exception was raised.

--*/

{

    INT Status;

    if (Attributes->Pid > 0) {
        Status = CkpOsWait(Attributes, Milliseconds);
        if (Status != 0) {

            //
            // If the request timed out, just return back to the caller without
            // necessarily raising an exception.
            //

            if (Status == 1) {
                return Status;
            }

            CkpSpawnRaiseSpawnError(Vm, Attributes);

        //
        // The wait succeeded, so set the return code.
        //

        } else {
            Status = CkpSpawnSetReturnCode(Vm, Attributes);
            if (Status == 0) {
                CkReturnInteger(Vm, Attributes->ReturnCode);
            }

            return 0;
        }
    }

    //
    // If the process is finished, return the return code.
    //

    if (Attributes->Pid == -1) {
        CkReturnInteger(Vm, Attributes->ReturnCode);

    //
    // The process is not yet finished or not yet started, return null.
    //

    } else {
        CkReturnNull(Vm);
    }

    return 0;
}

INT
CkpSpawnSetReturnCode (
    PCK_VM Vm,
    PSPAWN_ATTRIBUTES Attributes
    )

/*++

Routine Description:

    This routine sets the publicly visible return code. If the check option is
    set, it may raise an exception as well.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Attributes - Supplies a pointer to the attributes.

Return Value:

    0 on success.

    -1 if an exception was raised.

--*/

{

    CHAR Message[128];

    CkGetField(Vm, 0);
    CkPushString(Vm, "returncode", 10);
    CkPushInteger(Vm, Attributes->ReturnCode);
    CkDictSet(Vm, -3);
    CkStackPop(Vm);
    if (((Attributes->Options & SPAWN_OPTION_CHECK) != 0) &&
        (Attributes->ReturnCode != 0)) {

        snprintf(Message,
                 sizeof(Message),
                 "Child exited with status %d",
                 Attributes->ReturnCode);

        CkpSpawnRaiseError(Vm, "ChildProcessError", Message);
        return -1;
    }

    return 0;
}

VOID
CkpDestroySpawnAttributes (
    PVOID Attributes
    )

/*++

Routine Description:

    This routine closes all resources associated with a spawn attributes
    structure and frees the structure.

Arguments:

    Attributes - Supplies a pointer to the attributes to tear down.

Return Value:

    None.

--*/

{

    CkpTearDownSpawnAttributes(Attributes);
    free(Attributes);
    return;
}

VOID
CkpTearDownSpawnAttributes (
    PSPAWN_ATTRIBUTES Attributes
    )

/*++

Routine Description:

    This routine closes all resources associated with a spawn attributes
    structure.

Arguments:

    Attributes - Supplies a pointer to the attributes to tear down.

Return Value:

    None.

--*/

{

    assert(Attributes->Environment == NULL);
    assert(Attributes->Arguments == NULL);
    assert(Attributes->PassFds == NULL);
    assert(Attributes->ErrorMessage == NULL);

    if (Attributes->Stdin.ParentPipe >= 0) {
        close(Attributes->Stdin.ParentPipe);
        Attributes->Stdin.ParentPipe = -1;
    }

    if (Attributes->Stdin.CloseFd >= 0) {
        close(Attributes->Stdin.CloseFd);
        Attributes->Stdin.CloseFd = -1;
    }

    if (Attributes->Stdout.ParentPipe >= 0) {
        close(Attributes->Stdout.ParentPipe);
        Attributes->Stdout.ParentPipe = -1;
    }

    if (Attributes->Stdout.CloseFd >= 0) {
        close(Attributes->Stdout.CloseFd);
        Attributes->Stdout.CloseFd = -1;
    }

    if (Attributes->Stderr.ParentPipe >= 0) {
        close(Attributes->Stderr.ParentPipe);
        Attributes->Stderr.ParentPipe = -1;
    }

    if (Attributes->Stderr.CloseFd >= 0) {
        close(Attributes->Stderr.CloseFd);
        Attributes->Stderr.CloseFd = -1;
    }

    CkpOsTearDownSpawnAttributes(Attributes);
    return;
}

VOID
CkpSpawnRaiseSpawnError (
    PCK_VM Vm,
    PSPAWN_ATTRIBUTES Attributes
    )

/*++

Routine Description:

    This routine raises a spawn error. If there is an error message it is used
    and freed. Otherwise the errno description is used.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Attributes - Supplies a pointer to the attributes to tear down.

Return Value:

    None.

--*/

{

    if (Attributes->ErrorMessage != NULL) {
        CkpSpawnRaiseError(Vm, "SpawnError", Attributes->ErrorMessage);
        free(Attributes->ErrorMessage);
        Attributes->ErrorMessage = NULL;

    } else {
        CkpSpawnRaiseError(Vm, "SpawnError", strerror(errno));
    }

    return;
}

VOID
CkpSpawnRaiseError (
    PCK_VM Vm,
    PCSTR ExceptionType,
    PCSTR Message
    )

/*++

Routine Description:

    This routine raises an error with a message.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ExceptionType - Supplies a pointer to the type of exception to raise.

    Message - Supplies the message to send along with the exception.

Return Value:

    None. The foreign function should return as soon as possible and not
    manipulate the Chalk stack any longer.

--*/

{

    //
    // Create an exception.
    //

    CkPushModule(Vm, "spawn");
    CkGetVariable(Vm, -1, ExceptionType);
    CkPushString(Vm, Message, strlen(Message));
    CkCall(Vm, 1);

    //
    // Raise the exception.
    //

    CkRaiseException(Vm, -1);
    return;
}

