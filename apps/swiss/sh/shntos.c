/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    shntos.c

Abstract:

    This module implements NT operating system dependent functionality for the
    shell.

Author:

    Evan Green 13-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "shos.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the pipe buffer size. It's so large to prevent an earlier stage of
// an execution pipeline from blocking during execution.
//

#define SHELL_NT_PIPE_SIZE (1024 * 1024 * 10)

#define SHELL_NT_OUTPUT_CHUNK_SIZE 1024
#define SHELL_NT_INPUT_CHUNK_SIZE 1024

//
// Define the unix null device and the corresponding Windows device.
//

#define SHELL_NT_UNIX_NULL "/dev/null"
#define SHELL_NT_NULL "nul"

//
// Define the minimum path size.
//

#define SHELL_NT_PATH_SIZE_MINIMUM sizeof(SHELL_NT_UNIX_NULL)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a shell output collection context.

Members:

    Thread - Stores the thread handle of the thread servicing this request.

    Handle - Stores the handle to read.

    Buffer - Stores a pointer to the buffer containing the read output.

    BufferSize - Stores the size of the buffer including a null terminator.

    BufferCapacity - Stores the capacity of the buffer.

--*/

typedef struct _SHELL_NT_OUTPUT_COLLECTION {
    uintptr_t Thread;
    int Handle;
    void *Buffer;
    unsigned long BufferSize;
    unsigned long BufferCapacity;
} SHELL_NT_OUTPUT_COLLECTION, *PSHELL_NT_OUTPUT_COLLECTION;

/*++

Structure Description:

    This structure defines the context for a thread that pushes input into a
    pipe.

Members:

    Handle - Stores the pipe to write.

    Buffer - Stores a pointer to the buffer containing the input to write.

    BufferSize - Stores the size of the buffer including a null terminator.

--*/

typedef struct _SHELL_NT_INPUT_PUSH_CONTEXT {
    int Handle;
    void *Buffer;
    unsigned long BufferSize;
} SHELL_NT_INPUT_PUSH_CONTEXT, *PSHELL_NT_INPUT_PUSH_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

unsigned
__stdcall
ShOutputCollectionThread (
    void *Context
    );

void
ShPushInputThread (
    void *Context
    );

void
ShNtSignalHandler (
    int SignalNumber
    );

void
ShPrintLastError (
    void
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the executable extensions.
//

PSTR ShNtExecutableExtensions[] = {
    ".exe",
    ".bat",
    ".cmd",
    ".com"
};

//
// Store a global that's non-zero if this OS supports an executable permission
// bit.
//

int ShExecutableBitSupported = 0;

//
// ------------------------------------------------------------------ Functions
//

int
ShGetHomeDirectory (
    char *User,
    int UserSize,
    char **HomePath,
    int *HomePathSize
    )

/*++

Routine Description:

    This routine gets the given user's home directory.

Arguments:

    User - Supplies a pointer to the user string to query.

    UserSize - Supplies the size of the user string in bytes.

    HomePath - Supplies a pointer where the given user's home directory will be
        returned on success.

    HomePathSize - Supplies a pointer where the size of the home directory
        string will be returned on success.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    char *Home;
    char *NewHomePath;
    int NewHomePathSize;
    int Result;

    NewHomePath = NULL;
    NewHomePathSize = 0;
    Result = 0;

    //
    // On Windows, always return the current user's home directory.
    //

    Home = getenv("HOMEPATH");
    if (Home == NULL) {
        goto GetHomeDirectoryEnd;
    }

    NewHomePathSize = strlen(Home) + 1;
    NewHomePath = malloc(NewHomePathSize);
    if (NewHomePath == NULL) {
        goto GetHomeDirectoryEnd;
    }

    strcpy(NewHomePath, Home);
    Result = 1;

GetHomeDirectoryEnd:
    if (Result == 0) {
        if (NewHomePath != NULL) {
            free(NewHomePath);
            NewHomePath = NULL;
        }

        NewHomePathSize = 0;
    }

    *HomePath = NewHomePath;
    *HomePathSize = NewHomePathSize;
    return Result;
}

int
ShCreatePipe (
    int Descriptors[2]
    )

/*++

Routine Description:

    This routine creates an anonymous pipe.

Arguments:

    Descriptors - Supplies the array where the pipe's read and write ends will
        be returned.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    int Result;

    Result = _pipe(Descriptors, SHELL_NT_PIPE_SIZE, _O_BINARY);
    if (Result == 0) {
        return 1;
    }

    return 0;
}

int
ShPrepareForOutputCollection (
    int FileDescriptorToRead,
    void **Handle
    )

/*++

Routine Description:

    This routine is called before a subshell is going to run where the output
    will be collected.

Arguments:

    FileDescriptorToRead - Supplies the file descriptor that will be read to
        collect output.

    Handle - Supplies a pointer where an opaque token representing the
        collection will be returned.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    PSHELL_NT_OUTPUT_COLLECTION Context;
    int Result;
    unsigned ThreadId;

    //
    // Create an output collection context, initialize the file handle, and
    // spin up the thread which will read the file descriptor.
    //

    Result = 0;
    Context = malloc(sizeof(SHELL_NT_OUTPUT_COLLECTION));
    if (Context == NULL) {
        goto PrepareForOutputCollectionEnd;
    }

    memset(Context, 0, sizeof(SHELL_NT_OUTPUT_COLLECTION));
    Context->Handle = FileDescriptorToRead;
    Context->Thread = _beginthreadex(NULL,
                                     0,
                                     ShOutputCollectionThread,
                                     Context,
                                     0,
                                     &ThreadId);

    if (Context->Thread == 0) {
        goto PrepareForOutputCollectionEnd;
    }

    Result = 1;

PrepareForOutputCollectionEnd:
    if (Result == 1) {
        *Handle = Context;
    }

    return Result;
}

int
ShCollectOutput (
    void *Handle,
    char **Output,
    unsigned long *OutputSize
    )

/*++

Routine Description:

    This routine is called after a subshell has executed to collect its output.

Arguments:

    Handle - Supplies a pointer to the handle returned by the prepare for
        output collection function.

    Output - Supplies a pointer where the output will be returned. It is the
        caller's responsibility to free this memory.

    OutputSize - Supplies a pointer where the size of the output will be
        returned on success, with no null terminator.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    PSHELL_NT_OUTPUT_COLLECTION Context;

    *Output = NULL;
    *OutputSize = 0;
    Context = (PSHELL_NT_OUTPUT_COLLECTION)Handle;

    //
    // Wait for the thread to exit.
    //

    WaitForSingleObject((HANDLE)(Context->Thread), INFINITE);
    CloseHandle((HANDLE)(Context->Thread));
    if (Context->BufferSize == 0) {
        if (Context->Buffer != NULL) {
            free(Context->Buffer);
            Context->Buffer = NULL;
        }

        Context->BufferSize = 0;
    }

    *Output = Context->Buffer;
    *OutputSize = Context->BufferSize;
    free(Context);
    return 1;
}

int
ShPushInputText (
    char *Text,
    unsigned long TextSize,
    int Pipe[2]
    )

/*++

Routine Description:

    This routine forks an executable or thread to push the given input into the
    given pipe.

Arguments:

    Text - Supplies a pointer to the string containing the text to push into
        the input.

    TextSize - Supplies the number of bytes to write.

    Pipe - Supplies the pipe to write into. This routine is responsible for
        closing the write end of the pipe.

Return Value:

    Returns the pid that was created (and needs to be waited for) on success.

    0 on success if no pid needs to be waited on.

    -1 on failure.

--*/

{

    PSHELL_NT_INPUT_PUSH_CONTEXT Context;
    int Result;
    uintptr_t Thread;

    Result = -1;
    Context = malloc(sizeof(SHELL_NT_INPUT_PUSH_CONTEXT));
    if (Context == NULL) {
        goto PushInputTextEnd;
    }

    memset(Context, 0, sizeof(SHELL_NT_INPUT_PUSH_CONTEXT));
    Context->Handle = -1;

    //
    // Copy the document.
    //

    Context->Buffer = malloc(TextSize);
    if (Context->Buffer == NULL) {
        goto PushInputTextEnd;
    }

    memcpy(Context->Buffer, Text, TextSize);
    Context->BufferSize = TextSize;
    Context->Handle = Pipe[1];
    if (Context->Handle == -1) {
        goto PushInputTextEnd;
    }

    if (ShSetDescriptorFlags(Context->Handle, 0) != 0) {
        goto PushInputTextEnd;
    }

    Thread = _beginthread(ShPushInputThread, 0, Context);
    if (Thread == -1) {
        goto PushInputTextEnd;
    }

    Result = 0;

PushInputTextEnd:
    if (Result != 0) {
        if (Context != NULL) {
            if (Context->Buffer != NULL) {
                free(Context->Buffer);
            }

            if (Context->Handle != -1) {
                close(Context->Handle);
            }

            free(Context);
        }
    }

    return Result;
}

int
ShFixUpPath (
    char **Path,
    unsigned long *PathSize
    )

/*++

Routine Description:

    This routine performs any operating system dependent modifications to a
    path or path list.

Arguments:

    Path - Supplies a pointer where a pointer to the path variable value will
        be on input. This value may get replaced by a different buffer.

    PathSize - Supplies a pointer to the size of the path variable value on
        input. This value will be updated if the path size changes.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    unsigned long long BufferSize;
    char Character;
    char *Input;
    unsigned long long InputIndex;
    unsigned long long InputSize;
    char *Output;
    unsigned long long OutputIndex;

    //
    // Just double the buffer as a worst-case scenario.
    //

    Input = *Path;
    InputSize = *PathSize;
    BufferSize = InputSize * 2;
    if (BufferSize < SHELL_NT_PATH_SIZE_MINIMUM) {
        BufferSize = SHELL_NT_PATH_SIZE_MINIMUM;
    }

    Output = malloc(BufferSize);
    if (Output == NULL) {
        return 0;
    }

    OutputIndex = 0;
    for (InputIndex = 0; InputIndex < InputSize; InputIndex += 1) {
        Character = Input[InputIndex];

        //
        // Convert backslashes to forward slashes. Windows under MinGW gets it
        // and it makes everything else in the shell understand path
        // separators.
        //

        if (Character == '\\') {
            Character = '/';
        }

        Output[OutputIndex] = Character;
        OutputIndex += 1;
        if (Character == '\0') {
            break;
        }
    }

    free(*Path);

    //
    // Watch out for the special null device.
    //

    if (strcmp(Output, SHELL_NT_UNIX_NULL) == 0) {
        strcpy(Output, SHELL_NT_NULL);
        OutputIndex = strlen(Output) + 1;
    }

    *Path = Output;
    *PathSize = OutputIndex;
    return 1;
}

char *
ShGetEnvironmentVariable (
    char *Name
    )

/*++

Routine Description:

    This routine gets the current value of an environment variable.

Arguments:

    Name - Supplies a pointer to the null terminated string representing the
        name of the variable to get.

Return Value:

    Returns a pointer to an allocated buffer containing the value of the
    variable on success. The caller must call free to reclaim this buffer when
    finished.

    NULL on failure or if the variable is not set.

--*/

{

    if (getenv(Name) == NULL) {
        return NULL;
    }

    return strdup(getenv(Name));
}

int
ShSetEnvironmentVariable (
    char *Name,
    char *Value
    )

/*++

Routine Description:

    This routine sets the value of an environment variable.

Arguments:

    Name - Supplies a pointer to the null terminated string representing the
        name of the variable to set.

    Value - Supplies a pointer to the null terminated string containing the
        value to set for the given environment variable.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    size_t Length;
    char *String;

    if (Value == NULL) {
        Value = "";
    }

    Length = strlen(Name) + strlen(Value) + 2;
    String = malloc(Length);
    if (String == NULL) {
        return 0;
    }

    snprintf(String, Length, "%s=%s", Name, Value);
    if (putenv(String) == 0) {
        return 1;
    }

    free(String);
    return 0;
}

int
ShUnsetEnvironmentVariable (
    char *Name
    )

/*++

Routine Description:

    This routine unsets the value of an environment variable, deleting it
    from the environment.

Arguments:

    Name - Supplies a pointer to the null terminated string representing the
        name of the variable to unset.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    size_t Length;
    int Result;
    char *String;

    Length = strlen(Name) + 2;
    String = malloc(Length);
    if (String == NULL) {
        return 0;
    }

    snprintf(String, Length, "%s=", Name);
    Result = putenv(String);
    free(String);
    if (Result == 0) {
        return 1;
    }

    return 0;
}

int
ShGetExecutionTimes (
    PSHELL_PROCESS_TIMES Times
    )

/*++

Routine Description:

    This routine returns the execution time information from the kernel.

Arguments:

    Times - Supplies a pointer where the execution time information will be
        returned.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    FILETIME CreationTime;
    FILETIME ExitTime;
    FILETIME KernelTime;
    unsigned long long Microseconds;
    BOOL Result;
    FILETIME UserTime;

    memset(Times, 0, sizeof(SHELL_PROCESS_TIMES));
    Result = GetProcessTimes(GetCurrentProcess(),
                             &CreationTime,
                             &ExitTime,
                             &KernelTime,
                             &UserTime);

    if (Result == FALSE) {
        return 0;
    }

    Microseconds = (((unsigned long long)UserTime.dwHighDateTime << 32) |
                    UserTime.dwLowDateTime) / 10;

    Times->ShellUserMinutes = Microseconds / 60000000;
    Times->ShellUserMicroseconds = Microseconds % 60000000;
    Microseconds = (((unsigned long long)KernelTime.dwHighDateTime << 32) |
                    KernelTime.dwLowDateTime) / 10;

    Times->ShellSystemMinutes = Microseconds / 60000000;
    Times->ShellSystemMicroseconds = Microseconds % 60000000;
    return 1;
}

int
ShSetSignalDisposition (
    SHELL_SIGNAL Signal,
    SHELL_SIGNAL_DISPOSITION Disposition
    )

/*++

Routine Description:

    This routine sets the disposition with the OS for the given signal.

Arguments:

    Signal - Supplies the signal to change.

    Disposition - Supplies the disposition of the signal.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    int OsSignalNumber;

    switch (Signal) {
    case ShellSignalInterrupt:
        OsSignalNumber = SIGINT;
        break;

    case ShellSignalIllegalInstruction:
        OsSignalNumber = SIGILL;
        break;

    case ShellSignalFloatingPointException:
        OsSignalNumber = SIGFPE;
        break;

    case ShellSignalSegmentationFault:
        OsSignalNumber = SIGSEGV;
        break;

    case ShellSignalTerminate:
        OsSignalNumber = SIGTERM;
        break;

    case ShellSignalAbort:
        OsSignalNumber = SIGABRT;
        break;

    default:
        return 1;
    }

    switch (Disposition) {
    case ShellSignalDispositionDefault:
        signal(OsSignalNumber, SIG_DFL);
        break;

    case ShellSignalDispositionIgnore:
        signal(OsSignalNumber, SIG_IGN);
        break;

    case ShellSignalDispositionTrap:
        signal(OsSignalNumber, ShNtSignalHandler);
        break;

    default:
        return 0;
    }

    return 1;
}

void
ShRestoreOriginalSignalDispositions (
    void
    )

/*++

Routine Description:

    This routine restores all the signal dispositions back to their original
    state.

Arguments:

    None.

Return Value:

    None.

--*/

{

    signal(SIGINT, SIG_DFL);
    signal(SIGILL, SIG_DFL);
    signal(SIGFPE, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGABRT, SIG_DFL);
    return;
}

void
ShGetExecutableExtensions (
    char ***ExtensionList,
    unsigned int *ElementCount
    )

/*++

Routine Description:

    This routine gets the list of extensions to try when looking for an
    executable.

Arguments:

    ExtensionList - Supplies a pointer that will receive an array of strings
        containing the executable extensions. Yes, this is a triple character
        pointer (ie a pointer that returns a list). The caller does not own
        this memory and must not modify or free it.

    ElementCount - Supplies a pointer where the number of elements in the
        extension list will be returned.

Return Value:

    None.

--*/

{

    *ElementCount = sizeof(ShNtExecutableExtensions) /
                    sizeof(ShNtExecutableExtensions[0]);

    *ExtensionList = ShNtExecutableExtensions;
    return;
}

int
ShSetDescriptorFlags (
    int FileDescriptor,
    int Inheritable
    )

/*++

Routine Description:

    This routine sets the file flags for the given descriptor.

Arguments:

    FileDescriptor - Supplies the open file descriptor.

    Inheritable - Supplies a boolean indicating if this descriptor should be
        passed on to spawned processes.

Return Value:

    Zero on success.

    Non-zero on failure.

--*/

{

    DWORD Flags;
    HANDLE Handle;
    BOOL Result;

    Flags = 0;
    if (Inheritable != 0) {
        Flags |= HANDLE_FLAG_INHERIT;
    }

    Handle = (HANDLE)_get_osfhandle(FileDescriptor);
    Result = SetHandleInformation(Handle, HANDLE_FLAG_INHERIT, Flags);
    if ((Result == 0) &&
        ((Inheritable != 0) && (GetLastError() != ERROR_INVALID_PARAMETER))) {

        ShPrintLastError();
        return 0;
    }

    return 0;
}

int
ShOsDup (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine duplicates the given file descriptor above the application
    reserved area.

Arguments:

    FileDescriptor - Supplies the open file descriptor.

Return Value:

    Returns the duplicate file descriptor on success.

    -1 on failure.

--*/

{

    int ExtraDescriptors[SHELL_MINIMUM_FILE_DESCRIPTOR];
    int ExtraIndex;
    int FreeIndex;
    int Result;

    //
    // Call dup in a loop until a file descriptor above the user range is
    // found.
    //

    ExtraIndex = 0;
    Result = dup(FileDescriptor);
    while (Result < SHELL_MINIMUM_FILE_DESCRIPTOR) {
        ExtraDescriptors[ExtraIndex] = Result;
        ExtraIndex += 1;
        Result = dup(Result);
        if (Result < 0) {
            break;
        }
    }

    //
    // Release the extra descriptors.
    //

    for (FreeIndex = 0; FreeIndex < ExtraIndex; FreeIndex += 1) {
        close(ExtraDescriptors[FreeIndex]);
    }

    return Result;
}

void
ShOsConvertExitStatus (
    int *Status
    )

/*++

Routine Description:

    This routine converts an OS exit status code into a shell exit status code.

Arguments:

    Status - Supplies a pointer that on input contains the OS-specific exit
        status code. On output, returns the shell exit status code.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

unsigned
__stdcall
ShOutputCollectionThread (
    void *Context
    )

/*++

Routine Description:

    This routine implements the output collection thread on Windows.

Arguments:

    Context - Supplies a pointer to the thread context, in this case it's a
        pointer to the NT output collection structure.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ssize_t BytesRead;
    void *NewBuffer;
    PSHELL_NT_OUTPUT_COLLECTION OutputContext;

    OutputContext = (PSHELL_NT_OUTPUT_COLLECTION)Context;

    //
    // Loop reading from the file descriptor, reallocating the buffer as
    // needed.
    //

    while (TRUE) {
        if (OutputContext->BufferSize + SHELL_NT_OUTPUT_CHUNK_SIZE >
            OutputContext->BufferCapacity) {

            if (OutputContext->BufferCapacity == 0) {
                OutputContext->BufferCapacity = SHELL_NT_OUTPUT_CHUNK_SIZE;
            }

            while (OutputContext->BufferSize + SHELL_NT_OUTPUT_CHUNK_SIZE >
                   OutputContext->BufferCapacity) {

                OutputContext->BufferCapacity *= 2;
            }

            NewBuffer = realloc(OutputContext->Buffer,
                                OutputContext->BufferCapacity);

            if (NewBuffer == NULL) {
                OutputContext->BufferCapacity = 0;
                OutputContext->BufferSize = 0;
                break;
            }

            OutputContext->Buffer = NewBuffer;
        }

        BytesRead = read(OutputContext->Handle,
                         OutputContext->Buffer + OutputContext->BufferSize,
                         SHELL_NT_OUTPUT_CHUNK_SIZE);

        if (BytesRead <= 0) {
            break;
        }

        OutputContext->BufferSize += BytesRead;
    }

    _endthreadex(0);
    return 0;
}

void
ShPushInputThread (
    void *Context
    )

/*++

Routine Description:

    This routine implements the input push thread on windows.

Arguments:

    Context - Supplies a pointer to the thread context, in this case it's a
        pointer to the NT input push context.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    unsigned long BytesToWrite;
    ssize_t BytesWritten;
    PSHELL_NT_INPUT_PUSH_CONTEXT InputContext;
    unsigned long TotalBytesWritten;

    InputContext = (PSHELL_NT_INPUT_PUSH_CONTEXT)Context;

    //
    // Loop writing to the file descriptor.
    //

    TotalBytesWritten = 0;
    while (TotalBytesWritten != InputContext->BufferSize) {
        BytesToWrite = SHELL_NT_INPUT_CHUNK_SIZE;
        if (InputContext->BufferSize - TotalBytesWritten < BytesToWrite) {
            BytesToWrite = InputContext->BufferSize - TotalBytesWritten;
        }

        BytesWritten = write(InputContext->Handle,
                             InputContext->Buffer + TotalBytesWritten,
                             BytesToWrite);

        if (BytesWritten <= 0) {
            break;
        }

        TotalBytesWritten += BytesWritten;
    }

    //
    // Release the resources.
    //

    if (InputContext->Buffer != NULL) {
        free(InputContext->Buffer);
    }

    close(InputContext->Handle);
    free(InputContext);
    return;
}

void
ShNtSignalHandler (
    int SignalNumber
    )

/*++

Routine Description:

    This routine is called when a signal comes in. It marks the signal as
    pending and makes an effort to get out as quickly as possible. The signal
    execution environment is fairly hostile, so there's not much that could be
    done anyway.

Arguments:

    SignalNumber - Supplies the signal number of the signal that came in.

Return Value:

    None.

--*/

{

    //
    // Re-apply the signal for trapping.
    //

    signal(SignalNumber, ShNtSignalHandler);
    ShSignalHandler(SignalNumber);
    return;
}

void
ShPrintLastError (
    void
    )

/*++

Routine Description:

    This routine prints the result of GetLastError.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD Flags;
    PCHAR MessageBuffer;

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

    fprintf(stderr, "Last Error: %s\n", MessageBuffer);
    LocalFree(MessageBuffer);
    return;
}

