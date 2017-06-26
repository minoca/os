/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    win32.c

Abstract:

    This module implements Windows-specific Chalk spawn functionality.

Author:

    Evan Green 21-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <windows.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "spawnos.h"

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

char *
CkpEscapeArguments (
    char **Arguments
    );

char *
CkpJoinArguments (
    char **Arguments,
    char JoinCharacter
    );

char *
CkpGetLastError (
    VOID
    );

VOID
CkpSetInheritFlag (
    int Descriptor,
    int Inheritable
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the global pipe number for this PID.
//

INT CkSpawnPipeNumber = 0;

//
// ------------------------------------------------------------------ Functions
//

int
pipe (
    int Descriptors[2]
    )

/*++

Routine Description:

    This routine creates a new pipe.

Arguments:

    Descriptors - Supplies an array where the read and write descriptors for
        the pipe will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    OVERLAPPED Overlapped;
    CHAR PipeName[128];
    HANDLE ReadSide;
    INT Status;
    HANDLE WriteSide;

    ReadSide = INVALID_HANDLE_VALUE;
    WriteSide = INVALID_HANDLE_VALUE;
    Status = -1;
    snprintf(PipeName,
             sizeof(PipeName),
             "\\\\.\\pipe\\ckspawn%d_%d",
             getpid(),
             CkSpawnPipeNumber);

    CkSpawnPipeNumber += 1;
    memset(&Overlapped, 0, sizeof(Overlapped));
    Overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (Overlapped.hEvent == NULL) {
        errno = ENOMEM;
        return -1;
    }

    ReadSide = CreateNamedPipe(PipeName,
                               PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                               PIPE_TYPE_BYTE | PIPE_WAIT,
                               1,
                               4096,
                               4096,
                               0,
                               NULL);

    if (ReadSide == INVALID_HANDLE_VALUE) {
        goto pipeEnd;
    }

    ConnectNamedPipe(ReadSide, &Overlapped);
    WriteSide = CreateFile(PipeName,
                           GENERIC_WRITE,
                           0,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED,
                           NULL);

    if (WriteSide == INVALID_HANDLE_VALUE) {
        goto pipeEnd;
    }

    SetHandleInformation(WriteSide, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    SetHandleInformation(ReadSide, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    assert(HasOverlappedIoCompleted(&Overlapped));

    Descriptors[0] = _open_osfhandle((intptr_t)ReadSide, _O_BINARY | _O_RDONLY);
    if (Descriptors[0] < 0) {
        goto pipeEnd;
    }

    Descriptors[1] = _open_osfhandle((intptr_t)WriteSide, _O_BINARY);
    if (Descriptors[1] < 0) {
        close(Descriptors[0]);
    }

    Status = 0;

pipeEnd:
    if (Status != 0) {
        errno = EINVAL;
    }

    if (Overlapped.hEvent != NULL) {
        CloseHandle(Overlapped.hEvent);
    }

    return Status;
}

INT
CkpOsSpawn (
    PSPAWN_ATTRIBUTES Attributes
    )

/*++

Routine Description:

    This routine spawns a subprocess.

Arguments:

    Attributes - Supplies the attributes of the process to launch.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR CommandLine;
    DWORD CreationFlags;
    PSTR Environment;
    PSTR *EnvironmentPointer;
    PCSTR Executable;
    PROCESS_INFORMATION ProcessInfo;
    BOOL Result;
    PSTR Shell;
    PSTR ShellArguments[4];
    STARTUPINFO StartupInfo;
    INT Status;

    memset(&StartupInfo, 0, sizeof(StartupInfo));
    CreationFlags = 0;
    Environment = NULL;
    Shell = NULL;
    ShellArguments[2] = NULL;
    Executable = Attributes->Executable;
    if (Executable == NULL) {
        Executable = Attributes->Arguments[0];
    }

    if ((Attributes->Options & SPAWN_OPTION_SHELL) != 0) {
        ShellArguments[1] = "/c";
        ShellArguments[3] = NULL;
        Shell = getenv("SHELL");
        if (Shell != NULL) {
            ShellArguments[1] = "-c";
            ShellArguments[2] = CkpJoinArguments(Attributes->Arguments, ' ');

        } else {
            Shell = getenv("ComSpec");
            if (Shell == NULL) {
                Shell = "cmd.exe";
            }

            ShellArguments[2] = CkpEscapeArguments(Attributes->Arguments);
        }

        Executable = Shell;
        ShellArguments[0] = Shell;
        CommandLine = CkpEscapeArguments(ShellArguments);

    } else {
        CommandLine = CkpEscapeArguments(Attributes->Arguments);
    }

    if (Attributes->Environment != NULL) {

        //
        // Use the fact that the environment is a single allocation with the
        // giant string at the end. It's even double null terminated.
        //

        EnvironmentPointer = Attributes->Environment;
        while (*EnvironmentPointer != NULL) {
            EnvironmentPointer += 1;
        }

        Environment = (PSTR)(EnvironmentPointer + 1);
    }

    if ((Attributes->Stdin.Fd >= 0) ||
        (Attributes->Stdout.Fd >= 0) ||
        (Attributes->Stderr.Fd >= 0)) {

        StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
        if (Attributes->Stdin.ParentPipe >= 0) {
            CkpSetInheritFlag(Attributes->Stdin.ParentPipe, 0);
        }

        if (Attributes->Stdout.ParentPipe >= 0) {
            CkpSetInheritFlag(Attributes->Stdout.ParentPipe, 0);
        }

        if (Attributes->Stderr.ParentPipe >= 0) {
            CkpSetInheritFlag(Attributes->Stderr.ParentPipe, 0);
        }

        if (Attributes->Stdin.Fd >= 0) {
            StartupInfo.hStdInput =
                                  (HANDLE)_get_osfhandle(Attributes->Stdin.Fd);

        } else {
            StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        }

        if (Attributes->Stdout.Fd >= 0) {
            StartupInfo.hStdOutput =
                                 (HANDLE)_get_osfhandle(Attributes->Stdout.Fd);

        } else {
            StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        }

        if (Attributes->Stderr.Fd >= 0) {
            StartupInfo.hStdError =
                                 (HANDLE)_get_osfhandle(Attributes->Stderr.Fd);

        } else {
            StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        }
    }

    Result = CreateProcess(Executable,
                           CommandLine,
                           NULL,
                           NULL,
                           TRUE,
                           CreationFlags,
                           Environment,
                           Attributes->Cwd,
                           &StartupInfo,
                           &ProcessInfo);

    if ((Attributes->Debug &
         (SPAWN_DEBUG_BASIC_LAUNCH | SPAWN_DEBUG_DETAILED_LAUNCH)) != 0) {

        fprintf(stderr,
                "Launch%s %s\n",
                Result ? "ed" : " failed",
                Executable);
    }

    if ((Attributes->Debug & SPAWN_DEBUG_DETAILED_LAUNCH) != 0) {
        fprintf(stderr,
                "CommandLine: %s\n"
                "CreationFlags: 0x%x\n"
                "Cwd: %s\n"
                "Stdin/stdout/stderr: 0x%x/0x%x/0x%x\n"
                "Pid/Handle: %d/0x%x\n",
                CommandLine,
                CreationFlags,
                Attributes->Cwd,
                StartupInfo.hStdInput,
                StartupInfo.hStdOutput,
                StartupInfo.hStdError,
                ProcessInfo.dwProcessId,
                ProcessInfo.hProcess);
    }

    //
    // Close the child sides of the file descriptors so that the parent
    // sides will close up when the child dies.
    //

    if (Attributes->Stdin.CloseFd >= 0) {
        close(Attributes->Stdin.CloseFd);
        Attributes->Stdin.CloseFd = -1;
    }

    if (Attributes->Stdout.CloseFd >= 0) {
        close(Attributes->Stdout.CloseFd);
        Attributes->Stdout.CloseFd = -1;
    }

    if (Attributes->Stderr.CloseFd >= 0) {
        close(Attributes->Stderr.CloseFd);
        Attributes->Stderr.CloseFd = -1;
    }

    if (Result != FALSE) {
        Status = 0;
        Attributes->Pid = ProcessInfo.dwProcessId;
        Attributes->ProcessHandle = ProcessInfo.hProcess;
        CloseHandle(ProcessInfo.hThread);

    } else {
        Status = -1;
        Attributes->ErrorMessage = CkpGetLastError();
    }

    if (ShellArguments[2] != NULL) {
        free(ShellArguments[2]);
    }

    if (CommandLine != NULL) {
        free(CommandLine);
    }

    return Status;
}

INT
CkpOsWait (
    PSPAWN_ATTRIBUTES Attributes,
    int Milliseconds
    )

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

{

    DWORD ExitCode;
    DWORD Status;

    if (Milliseconds < 0) {
        Milliseconds = INFINITE;
    }

    Status = WaitForSingleObject(Attributes->ProcessHandle, Milliseconds);
    if (Status == WAIT_TIMEOUT) {
        return 1;

    } else if (Status == WAIT_OBJECT_0) {
        if (GetExitCodeProcess(Attributes->ProcessHandle, &ExitCode)) {
            Attributes->ReturnCode = ExitCode;
            CloseHandle(Attributes->ProcessHandle);
            Attributes->ProcessHandle = INVALID_HANDLE_VALUE;
            Attributes->Pid = -1;
            return 0;
        }
    }

    Attributes->ErrorMessage = CkpGetLastError();
    return -1;
}

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
    )

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

{

    DWORD BytesDone;
    PVOID Error;
    size_t ErrorCapacity;
    HANDLE ErrorHandle;
    size_t ErrorSize;
    BOOL ErrorSubmitted;
    DWORD ExitCode;
    HANDLE InHandle;
    BOOL InSubmitted;
    DWORD LastError;
    PVOID NewBuffer;
    DWORD ObjectCount;
    HANDLE Objects[4];
    PVOID Out;
    size_t OutCapacity;
    HANDLE OutHandle;
    size_t OutSize;
    BOOL OutSubmitted;
    OVERLAPPED OverError;
    OVERLAPPED OverIn;
    OVERLAPPED OverOut;
    pid_t Pid;
    BOOL Result;
    int ReturnValue;
    DWORD Status;
    BOOL Wait;

    Error = NULL;
    ErrorHandle = INVALID_HANDLE_VALUE;
    ErrorSize = 0;
    ErrorSubmitted = FALSE;
    InHandle = INVALID_HANDLE_VALUE;
    InSubmitted = FALSE;
    Out = NULL;
    OutHandle = INVALID_HANDLE_VALUE;
    OutSize = 0;
    OutSubmitted = FALSE;
    ObjectCount = 0;
    if (Attributes->ProcessHandle != INVALID_HANDLE_VALUE) {
        Objects[0] = Attributes->ProcessHandle;
        ObjectCount = 1;
    }

    Pid = Attributes->Pid;
    ReturnValue = -1;
    if (Milliseconds < 0) {
        Milliseconds = INFINITE;
    }

    memset(&OverError, 0, sizeof(OVERLAPPED));
    memset(&OverIn, 0, sizeof(OVERLAPPED));
    memset(&OverOut, 0, sizeof(OVERLAPPED));

    //
    // Add the pipe handles involved to the array of things to wait on.
    //

    if (Attributes->Stdout.ParentPipe >= 0) {
        OutCapacity = 2048;
        Out = malloc(OutCapacity);
        if (Out == NULL) {
            return -1;
        }

        OverOut.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (OverOut.hEvent == NULL) {
            goto OsCommunicateEnd;
        }

        Objects[ObjectCount] = OverOut.hEvent;
        OutHandle = (HANDLE)_get_osfhandle(Attributes->Stdout.ParentPipe);
        ObjectCount += 1;
    }

    if (Attributes->Stderr.ParentPipe >= 0) {
        ErrorCapacity = 2048;
        Error = malloc(ErrorCapacity);
        if (Error == NULL) {
            goto OsCommunicateEnd;
        }

        OverError.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (OverError.hEvent == NULL) {
            goto OsCommunicateEnd;
        }

        Objects[ObjectCount] = OverError.hEvent;
        ErrorHandle = (HANDLE)_get_osfhandle(Attributes->Stderr.ParentPipe);
        ObjectCount += 1;
    }

    if ((InputSize != 0) && (Attributes->Stdin.ParentPipe >= 0)) {
        OverIn.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (OverIn.hEvent == NULL) {
            goto OsCommunicateEnd;
        }

        Objects[ObjectCount] = OverIn.hEvent;
        InHandle = (HANDLE)_get_osfhandle(Attributes->Stdin.ParentPipe);
        ObjectCount += 1;
    }

    //
    // Loop working on data.
    //

    while (TRUE) {

        //
        // Kick off the input write.
        //

        if ((InputSize != 0) && (InSubmitted == FALSE)) {
            Result = WriteFile(InHandle, Input, InputSize, NULL, &OverIn);
            if ((Result != FALSE) || (GetLastError() == ERROR_IO_PENDING)) {
                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr,
                            "Communicate %d start write %d\n",
                            Pid,
                            InputSize);
                }

                InSubmitted = TRUE;
            }
        }

        //
        // Kick off the stdout read.
        //

        if ((OutHandle != INVALID_HANDLE_VALUE) && (OutSubmitted == FALSE)) {
            if (OutSize >= OutCapacity) {
                if (OutCapacity >= CK_SPAWN_MAX_OUTPUT) {
                    errno = ENOMEM;
                    ReturnValue = -1;
                    goto OsCommunicateEnd;
                }

                OutCapacity *= 2;
                NewBuffer = realloc(Out, OutCapacity);
                if (NewBuffer == NULL) {
                    ReturnValue = -1;
                    goto OsCommunicateEnd;
                }

                Out = NewBuffer;
            }

            Result = ReadFile(OutHandle,
                              Out + OutSize,
                              OutCapacity - OutSize,
                              NULL,
                              &OverOut);

            if ((Result != FALSE) || (GetLastError() == ERROR_IO_PENDING)) {
                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr, "Communicate %d start read stdout.\n", Pid);
                }

                OutSubmitted = TRUE;
            }
        }

        //
        // Kick off the stderr read.
        //

        if ((ErrorHandle != INVALID_HANDLE_VALUE) &&
            (ErrorSubmitted == FALSE)) {

            if (ErrorSize >= ErrorCapacity) {
                if (ErrorCapacity >= CK_SPAWN_MAX_OUTPUT) {
                    errno = ENOMEM;
                    ReturnValue = -1;
                    goto OsCommunicateEnd;
                }

                ErrorCapacity *= 2;
                NewBuffer = realloc(Error, ErrorCapacity);
                if (NewBuffer == NULL) {
                    ReturnValue = -1;
                    goto OsCommunicateEnd;
                }

                Error = NewBuffer;
            }

            Result = ReadFile(ErrorHandle,
                              Error + ErrorSize,
                              ErrorCapacity - ErrorSize,
                              NULL,
                              &OverError);

            if ((Result != FALSE) || (GetLastError() == ERROR_IO_PENDING)) {
                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr, "Communicate %d start read stderr.\n", Pid);
                }

                ErrorSubmitted = TRUE;
            }
        }

        //
        // If the process is long since dead and no more output or error is
        // being collected, stop.
        //

        if ((Attributes->ProcessHandle == INVALID_HANDLE_VALUE) &&
            (OutSubmitted == FALSE) && (ErrorSubmitted == FALSE)) {

            ReturnValue = 0;
            break;
        }

        //
        // Sleep waiting for the process to exit or for one of the handles
        // to become ready. Do the wait first so that if the process finishes,
        // the remaining output/error can be read.
        //

        Status = WaitForMultipleObjects(ObjectCount,
                                        Objects,
                                        FALSE,
                                        Milliseconds);

        if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
            fprintf(stderr, "Communicate %d wait: 0x%x\n", Pid, Status);
        }

        Wait = FALSE;
        if (Status == WAIT_TIMEOUT) {
            Wait = TRUE;

        //
        // If the process exited, then wait for stdout/stderr to complete.
        //

        } else if ((Status == WAIT_OBJECT_0) &&
                   (Attributes->ProcessHandle != INVALID_HANDLE_VALUE)) {

            Wait = TRUE;
            ObjectCount -= 1;
            Objects[0] = Objects[ObjectCount];

        } else if ((Status < WAIT_OBJECT_0) ||
                   (Status >= WAIT_OBJECT_0 + ObjectCount)) {

            Attributes->ErrorMessage = CkpGetLastError();
            ReturnValue = -1;
            goto OsCommunicateEnd;
        }

        if (Wait != FALSE) {
            if (InSubmitted != FALSE) {
                CancelIo(InHandle);
            }

            if (OutSubmitted != FALSE) {
                CancelIo(OutHandle);
            }

            if (ErrorSubmitted != FALSE) {
                CancelIo(ErrorHandle);
            }
        }

        //
        // Check on the input status.
        //

        if ((InSubmitted != FALSE) &&
            ((Wait != FALSE) || (HasOverlappedIoCompleted(&OverIn)))) {

            if (HasOverlappedIoCompleted(&OverIn)) {
                InSubmitted = FALSE;
            }

            Result = GetOverlappedResult(InHandle,
                                         &OverIn,
                                         &BytesDone,
                                         Wait);

            if (Result != FALSE) {
                ResetEvent(OverIn.hEvent);
                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr,
                            "Communicate %d stdin wrote %d.\n",
                            Pid,
                            BytesDone);
                }

                Input += BytesDone;
                InputSize -= BytesDone;
                if (InputSize == 0) {

                    assert(Objects[ObjectCount - 1] == OverIn.hEvent);

                    ObjectCount -= 1;
                }

            } else {
                LastError = GetLastError();
                if ((LastError != ERROR_BROKEN_PIPE) &&
                    ((Wait == FALSE) ||
                     (LastError != ERROR_OPERATION_ABORTED))) {

                    Attributes->ErrorMessage = CkpGetLastError();
                    ReturnValue = -1;
                    goto OsCommunicateEnd;
                }
            }
        }

        //
        // Check the stdout status.
        //

        if ((OutSubmitted != FALSE) &&
            ((Wait != FALSE) || (HasOverlappedIoCompleted(&OverOut)))) {

            if (HasOverlappedIoCompleted(&OverOut)) {
                OutSubmitted = FALSE;
            }

            Result = GetOverlappedResult(OutHandle,
                                         &OverOut,
                                         &BytesDone,
                                         Wait);

            if (Result != FALSE) {
                OutSize += BytesDone;
                ResetEvent(OverOut.hEvent);
                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr,
                            "Communicate %d stdout read %d.\n",
                            Pid,
                            BytesDone);
                }

            } else {
                LastError = GetLastError();
                if ((LastError != ERROR_BROKEN_PIPE) &&
                    ((Wait == FALSE) ||
                     (LastError != ERROR_OPERATION_ABORTED))) {

                    Attributes->ErrorMessage = CkpGetLastError();
                    ReturnValue = -1;
                    goto OsCommunicateEnd;
                }
            }
        }

        //
        // Check the stderr status.
        //

        if ((ErrorSubmitted != FALSE) &&
            ((Wait != FALSE) || (HasOverlappedIoCompleted(&OverError)))) {

            if (HasOverlappedIoCompleted(&OverError)) {
                ErrorSubmitted = FALSE;
            }

            Result = GetOverlappedResult(ErrorHandle,
                                         &OverError,
                                         &BytesDone,
                                         Wait);

            if (Result != FALSE) {
                ErrorSize += BytesDone;
                ResetEvent(OverError.hEvent);
                if ((Attributes->Debug & SPAWN_DEBUG_IO) != 0) {
                    fprintf(stderr,
                            "Communicate %d stderr read %d.\n",
                            Pid,
                            BytesDone);
                }

            } else {
                LastError = GetLastError();
                if ((LastError != ERROR_BROKEN_PIPE) &&
                    ((Wait == FALSE) ||
                     (LastError != ERROR_OPERATION_ABORTED))) {

                    Attributes->ErrorMessage = CkpGetLastError();
                    ReturnValue = -1;
                    goto OsCommunicateEnd;
                }
            }
        }

        //
        // If the process died, stop looping now that all the stdout and
        // stderr have been read.
        //

        if ((Status == WAIT_OBJECT_0) &&
            (Attributes->ProcessHandle != INVALID_HANDLE_VALUE)) {

            assert((InSubmitted == FALSE) && (OutSubmitted == FALSE) &&
                   (ErrorSubmitted == FALSE));

            if (GetExitCodeProcess(Attributes->ProcessHandle, &ExitCode)) {
                Attributes->ReturnCode = ExitCode;
                CloseHandle(Attributes->ProcessHandle);
                Attributes->ProcessHandle = INVALID_HANDLE_VALUE;
                Attributes->Pid = -1;
            }

            ReturnValue = 0;
            break;

        } else if (Status == WAIT_TIMEOUT) {
            ReturnValue = 1;
            if (OutSize + ErrorSize != 0) {
                ReturnValue = 0;
            }

            break;
        }
    }

OsCommunicateEnd:
    if (OverIn.hEvent != NULL) {
        CloseHandle(OverIn.hEvent);
    }

    if (OverOut.hEvent != NULL) {
        CloseHandle(OverOut.hEvent);
    }

    if (OverError.hEvent != NULL) {
        CloseHandle(OverError.hEvent);
    }

    if (ReturnValue < 0) {
        OutSize = 0;
        ErrorSize = 0;
        if (InSubmitted != FALSE) {
            CancelIo(InHandle);
        }

        if (OutSubmitted != FALSE) {
            CancelIo(OutHandle);
        }

        if (ErrorSubmitted != FALSE) {
            CancelIo(ErrorHandle);
        }
    }

    assert((InSubmitted == FALSE) && (OutSubmitted == FALSE) &&
           (ErrorSubmitted == FALSE));

    if ((OutSize == 0) && (Out != NULL)) {
        free(Out);
        Out = NULL;
    }

    if ((ErrorSize == 0) && (Error != NULL)) {
        free(Error);
        Error = NULL;
    }

    *OutData = Out;
    *OutDataSize = OutSize;
    *ErrorData = Error;
    *ErrorDataSize = ErrorSize;
    return ReturnValue;
}

INT
CkpOsSendSignal (
    PSPAWN_ATTRIBUTES Attributes,
    INT Signal
    )

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

{

    if (Attributes->ProcessHandle != INVALID_HANDLE_VALUE) {
        TerminateProcess(Attributes->ProcessHandle, -Signal);
    }

    return 0;
}

VOID
CkpOsTearDownSpawnAttributes (
    PSPAWN_ATTRIBUTES Attributes
    )

/*++

Routine Description:

    This routine closes all OS-specific resources associated with a spawn
    attributes structure.

Arguments:

    Attributes - Supplies a pointer to the attributes to tear down.

Return Value:

    None.

--*/

{

    if (Attributes->ProcessHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(Attributes->ProcessHandle);
        Attributes->ProcessHandle = INVALID_HANDLE_VALUE;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

char *
CkpEscapeArguments (
    char **Arguments
    )

/*++

Routine Description:

    This routine creates a copy of the given arguments, surrounded by double
    quotes and escaped.

Arguments:

    Arguments - Supplies the arguments to copy. Nothing will be sliced off of
        this array, just added to the beginning of it.

Return Value:

    Returns the Win32 command line.

    NULL on allocation failure.

--*/

{

    ULONG AllocationSize;
    INT ArgumentCount;
    ULONG ArgumentIndex;
    PSTR CommandLine;
    BOOL NextIsQuote;
    PSTR Search;
    PSTR Source;
    PSTR String;

    ArgumentCount = 0;
    while (Arguments[ArgumentCount] != NULL) {
        ArgumentCount += 1;
    }

    assert(ArgumentCount != 0);

    AllocationSize = 1;
    for (ArgumentIndex = 0;
         Arguments[ArgumentIndex] != NULL;
         ArgumentIndex += 1) {

        //
        // In the worst case, the whole argument is quotes, so double it, then
        // add four for "<string>"<space>.
        //

        AllocationSize += (strlen(Arguments[ArgumentIndex]) * 2) + 4;
    }

    //
    // Fail explicitly if the command line is too big.
    //

    if (AllocationSize >= 32768) {
        return NULL;
    }

    String = malloc(AllocationSize);
    if (String == NULL) {
        return NULL;
    }

    CommandLine = String;
    for (ArgumentIndex = 0;
         Arguments[ArgumentIndex] != NULL;
         ArgumentIndex += 1) {

        Source = Arguments[ArgumentIndex];
        Search = Source;

        //
        // If there are no spaces, backslashes, or double quotes, then there's
        // no need to escape.
        //

        if ((*Source != '\0') && (strpbrk(Source, " \"\t\n\v\f\\") == NULL)) {
            while (*Source != '\0') {
                *String = *Source;
                String += 1;
                Source += 1;
            }

            *String = ' ';
            String += 1;
            continue;
        }

        NextIsQuote = FALSE;
        *String = '"';
        String += 1;
        while (*Source != '\0') {

            //
            // Search out ahead and look to see if the next non-backslash is
            // a double quote. If it is, every pair of double backslashes gets
            // collapsed, so add an extra to it. For example "\\a" comes out
            // with two backslashes, "\\\\" comes out as two backslashes, and
            // "\"a" comes out as "a.
            //

            if (Search == Source) {
                while (*Search == '\\') {
                    Search += 1;
                }

                if ((*Search == '\0') || (*Search == '"')) {
                    NextIsQuote = TRUE;

                } else {
                    NextIsQuote = FALSE;
                }

                Search += 1;
            }

            //
            // If there's a backslash and the next thing is a quote, put
            // another backslash in front of it.
            //

            if ((NextIsQuote != FALSE) && (*Source == '\\')) {
                *String = '\\';
                String += 1;
            }

            //
            // If this is a quote, put a backslash in front of it to escape it.
            //

            if (*Source == '"') {
                *String = '\\';
                String += 1;
            }

            *String = *Source;
            String += 1;
            Source += 1;
        }

        *String = '"';
        String += 1;
        *String = ' ';
        String += 1;
    }

    *String = '\0';
    return CommandLine;
}

char *
CkpJoinArguments (
    char **Arguments,
    char JoinCharacter
    )

/*++

Routine Description:

    This routine joins the arguments array with the given character into a
    single string.

Arguments:

    Arguments - Supplies the null-terminated list of strings to copy.

    JoinCharacter - Supplies the character to join the strings with.

Return Value:

    Returns the Win32 command line.

    NULL on allocation failure.

--*/

{

    size_t ArgumentCount;
    PSTR CommandLine;
    PSTR Current;
    size_t Size;

    ArgumentCount = 0;
    Size = 1;
    while (Arguments[ArgumentCount] != NULL) {
        Size += strlen(Arguments[ArgumentCount]) + 1;
        ArgumentCount += 1;
    }

    CommandLine = malloc(Size);
    if (CommandLine == NULL) {
        return NULL;
    }

    Current = CommandLine;
    ArgumentCount = 0;
    while (Arguments[ArgumentCount] != NULL) {
        Size = strlen(Arguments[ArgumentCount]);
        memcpy(Current, Arguments[ArgumentCount], Size);
        if (Arguments[ArgumentCount + 1] == NULL) {
            Current[Size] = '\0';

        } else {
            Current[Size] = JoinCharacter;
        }

        Current += Size + 1;
        ArgumentCount += 1;
    }

    *Current = '\0';
    return CommandLine;
}

char *
CkpGetLastError (
    VOID
    )

/*++

Routine Description:

    This routine returns a string describing the last error.

Arguments:

    None.

Return Value:

    Returns an allocated string describing the last error. The caller is
    responsible for freeing this string.

    NULL on allocation failure.

--*/

{

    DWORD Flags;
    size_t Index;
    PCHAR MessageBuffer;
    PSTR Result;

    Flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS;

    FormatMessage(Flags,
                  NULL,
                  GetLastError(),
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR)&MessageBuffer,
                  0,
                  NULL);

    Result = strdup(MessageBuffer);
    if (Result != NULL) {
        Index = strlen(Result);
        while ((Index >= 1) &&
               ((Result[Index - 1] == '\r') ||
                (Result[Index - 1] == '\n') ||
                (Result[Index - 1] == '.'))) {

            Result[Index - 1] = '\0';
            Index -= 1;
        }
    }

    LocalFree(MessageBuffer);
    return Result;
}

VOID
CkpSetInheritFlag (
    int Descriptor,
    int Inheritable
    )

/*++

Routine Description:

    This routine sets the inheritable flag on a given descriptor.

Arguments:

    Descriptor - Supplies the descriptor to set the flag or.

    Inheritable - Supplies a non-zero value if the handle should be inheritable,
        or zero if the handle should not be inheritable.

Return Value:

    None.

--*/

{

    DWORD Flags;
    HANDLE Handle;

    Flags = 0;
    if (Inheritable != 0) {
        Flags |= HANDLE_FLAG_INHERIT;
    }

    Handle = (HANDLE)_get_osfhandle(Descriptor);
    SetHandleInformation(Handle, HANDLE_FLAG_INHERIT, Flags);
    return;
}

