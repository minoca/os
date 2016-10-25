/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    shuos.c

Abstract:

    This module implements POSIX operating system dependent functionality for
    the shell.

Author:

    Evan Green 22-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <errno.h>
#include "shos.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SHELL_OUTPUT_CHUNK_SIZE 1024
#define SHELL_INPUT_CHUNK_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
ShConvertToOsSignal (
    SHELL_SIGNAL Signal
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Remember the number of clock ticks per second.
//

long ShClockTicksPerSecond;

//
// Remember the original signal dispositions.
//

struct sigaction ShOriginalSignalDispositions[ShellSignalCount];
int ShOriginalSignalDispositionValid[ShellSignalCount];

//
// Store a global that's non-zero if this OS supports an executable permission
// bit.
//

int ShExecutableBitSupported = 1;

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

    char *NameCopy;
    char *NewHomePath;
    int NewHomePathSize;
    struct passwd *UserInformation;

    *HomePath = NULL;
    *HomePathSize = 0;
    if (UserSize <= 1) {
        NameCopy = NULL;

    } else {
        NameCopy = malloc(UserSize);
        if (NameCopy == NULL) {
            return 0;
        }

        memcpy(NameCopy, User, UserSize);
        NameCopy[UserSize - 1] = '\0';
    }

    UserInformation = getpwnam(NameCopy);
    if (NameCopy != NULL) {
        free(NameCopy);
    }

    if (UserInformation == NULL) {
        return 0;
    }

    NewHomePathSize = strlen(UserInformation->pw_dir) + 1;
    NewHomePath = malloc(NewHomePathSize);
    if (NewHomePath == NULL) {
        return 0;
    }

    memcpy(NewHomePath, UserInformation->pw_dir, NewHomePathSize);
    *HomePath = NewHomePath;
    *HomePathSize = NewHomePathSize;
    return 1;
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

    Result = pipe(Descriptors);
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

    *Handle = (void *)(long)FileDescriptorToRead;
    return 1;
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

    char *Buffer;
    unsigned long BufferCapacity;
    unsigned long BufferSize;
    ssize_t BytesRead;
    int Descriptor;
    char *NewBuffer;
    int Result;

    Buffer = NULL;
    BufferCapacity = 0;
    BufferSize = 0;
    Descriptor = (int)(long)Handle;
    Result = 1;

    //
    // Loop reading from the file descriptor, reallocating the buffer as
    // needed.
    //

    while (1) {
        if (BufferSize + SHELL_OUTPUT_CHUNK_SIZE > BufferCapacity) {
            if (BufferCapacity == 0) {
                BufferCapacity = SHELL_OUTPUT_CHUNK_SIZE;
            }

            while (BufferSize + SHELL_OUTPUT_CHUNK_SIZE > BufferCapacity) {
                BufferCapacity *= 2;
            }

            NewBuffer = realloc(Buffer, BufferCapacity);
            if (NewBuffer == NULL) {
                BufferCapacity = 0;
                BufferSize = 0;
                Result = 0;
                break;
            }

            Buffer = NewBuffer;
        }

        do {
            BytesRead = read(Descriptor,
                             Buffer + BufferSize,
                             SHELL_OUTPUT_CHUNK_SIZE);

        } while ((BytesRead < 0) && (errno == EINTR));

        if (BytesRead <= 0) {
            break;
        }

        BufferSize += BytesRead;
    }

    *Output = Buffer;
    *OutputSize = BufferSize;
    return Result;
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

    unsigned long BytesToWrite;
    ssize_t BytesWritten;
    pid_t Child;
    unsigned long TotalBytesWritten;

    //
    // Fork off into a new process that pushes the input into the pipe and
    // exits.
    //

    fflush(NULL);
    Child = fork();
    if (Child < 0) {
        return -1;
    }

    //
    // If this is the parent, the work here is done, so exit.
    //

    if (Child != 0) {
        close(Pipe[1]);
        Pipe[1] = -1;
        return Child;
    }

    //
    // This is the child process. Loop writing to the file descriptor.
    //

    close(Pipe[0]);
    TotalBytesWritten = 0;
    while (TotalBytesWritten != TextSize) {
        BytesToWrite = SHELL_INPUT_CHUNK_SIZE;
        if (TextSize - TotalBytesWritten < BytesToWrite) {
            BytesToWrite = TextSize - TotalBytesWritten;
        }

        do {
            BytesWritten = write(Pipe[1],
                                 Text + TotalBytesWritten,
                                 BytesToWrite);

        } while ((BytesWritten < 0) && (errno == EINTR));

        if (BytesWritten <= 0) {
            break;
        }

        TotalBytesWritten += BytesWritten;
    }

    //
    // Exit immediately, as this was a child process and shouldn't go back to
    // doing shell functions.
    //

    exit(0);
    return 0;
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

    char *Value;
    char *ValueCopy;

    Value = getenv(Name);
    if (Value == NULL) {
        return NULL;
    }

    ValueCopy = strdup(Value);
    if (ValueCopy == NULL) {
        return NULL;
    }

    return ValueCopy;
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

    if (setenv(Name, Value, 1) == 0) {
        return 1;
    }

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

    if (unsetenv(Name) == 0) {
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

    unsigned long long RemainingTicks;
    clock_t Result;
    struct tms TimesResult;

    //
    // If never calculated before, get the number of clock ticks per second.
    //

    if (ShClockTicksPerSecond <= 0) {
        ShClockTicksPerSecond = sysconf(_SC_CLK_TCK);
        if (ShClockTicksPerSecond <= 0) {
            return 0;
        }
    }

    Result = times(&TimesResult);
    if (Result == -1) {
        return 0;
    }

    //
    // Convert each time from ticks into minutes and microseconds. Start with
    // the shell user time.
    //

    Times->ShellUserMinutes = TimesResult.tms_utime /
                              ShClockTicksPerSecond / 60;

    RemainingTicks = TimesResult.tms_utime -
                     (Times->ShellUserMinutes * 60 * ShClockTicksPerSecond);

    Times->ShellUserMicroseconds = RemainingTicks * 1000000ULL /
                                   ShClockTicksPerSecond;

    //
    // Convert shell system time.
    //

    Times->ShellSystemMinutes = TimesResult.tms_stime /
                                ShClockTicksPerSecond / 60;

    RemainingTicks = TimesResult.tms_stime -
                     (Times->ShellSystemMinutes * 60 * ShClockTicksPerSecond);

    Times->ShellSystemMicroseconds = RemainingTicks * 1000000ULL /
                                     ShClockTicksPerSecond;

    //
    // Convert children user time.
    //

    Times->ChildrenUserMinutes = TimesResult.tms_cutime /
                                 ShClockTicksPerSecond / 60;

    RemainingTicks = TimesResult.tms_cutime -
                     (Times->ChildrenUserMinutes * 60 * ShClockTicksPerSecond);

    Times->ChildrenUserMicroseconds = RemainingTicks * 1000000ULL /
                                      ShClockTicksPerSecond;

    //
    // Convert children system time.
    //

    Times->ChildrenSystemMinutes = TimesResult.tms_cstime /
                                   ShClockTicksPerSecond / 60;

    RemainingTicks =
                   TimesResult.tms_cstime -
                   (Times->ChildrenSystemMinutes * 60 * ShClockTicksPerSecond);

    Times->ChildrenSystemMicroseconds = RemainingTicks * 1000000ULL /
                                        ShClockTicksPerSecond;

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

    struct sigaction OriginalAction;
    int OsSignalNumber;
    int Result;
    struct sigaction SignalAction;

    if (Signal == ShellSignalOnExit) {
        return 1;
    }

    memset(&SignalAction, 0, sizeof(struct sigaction));
    OsSignalNumber = ShConvertToOsSignal(Signal);
    if (OsSignalNumber == 0) {
        return 0;
    }

    switch (Disposition) {
    case ShellSignalDispositionDefault:
        SignalAction.sa_handler = SIG_DFL;
        break;

    case ShellSignalDispositionIgnore:
        SignalAction.sa_handler = SIG_IGN;
        break;

    case ShellSignalDispositionTrap:
        SignalAction.sa_handler = ShSignalHandler;
        break;

    default:
        return 0;
    }

    Result = sigaction(OsSignalNumber, &SignalAction, &OriginalAction);
    if (Result != 0) {
        return 0;
    }

    //
    // Save the original disposition for restoration later.
    //

    if (ShOriginalSignalDispositionValid[Signal] == 0) {
        ShOriginalSignalDispositions[Signal] = OriginalAction;
        ShOriginalSignalDispositionValid[Signal] = 1;
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

    int OsSignalNumber;
    int Result;
    int SignalIndex;

    for (SignalIndex = 0; SignalIndex < ShellSignalCount; SignalIndex += 1) {
        OsSignalNumber = ShConvertToOsSignal(SignalIndex);
        if (OsSignalNumber == 0) {
            continue;
        }

        if (ShOriginalSignalDispositionValid[SignalIndex] != 0) {
            Result = sigaction(OsSignalNumber,
                               &(ShOriginalSignalDispositions[SignalIndex]),
                               NULL);

            if (Result == 0) {
                ShOriginalSignalDispositionValid[SignalIndex] = 0;
            }
        }
    }

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

    *ExtensionList = NULL;
    *ElementCount = 0;
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

    int Flags;

    Flags = 0;
    if (Inheritable == 0) {
        Flags = FD_CLOEXEC;
    }

    return fcntl(FileDescriptor, F_SETFD, Flags);
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

    return fcntl(FileDescriptor, F_DUPFD, SHELL_MINIMUM_FILE_DESCRIPTOR);
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

    //
    // TODO: Handle stopped processes in the shell with job control.
    //

    assert(!WIFSTOPPED(*Status));

    if (WIFEXITED(*Status)) {
        *Status = WEXITSTATUS(*Status);

    } else {
        *Status = WTERMSIG(*Status) + SHELL_EXIT_SIGNALED;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

int
ShConvertToOsSignal (
    SHELL_SIGNAL Signal
    )

/*++

Routine Description:

    This routine converts a shell signal enumerator into its equivalent
    operating system dependent signal number.

Arguments:

    Signal - Supplies the signal to convert.

Return Value:

    Returns the equivalent signal number, or 0 if there is no equivalent.

--*/

{

    int OsSignalNumber;

    switch (Signal) {
    case ShellSignalHangup:
        OsSignalNumber = SIGHUP;
        break;

    case ShellSignalInterrupt:
        OsSignalNumber = SIGINT;
        break;

    case ShellSignalQuit:
        OsSignalNumber = SIGQUIT;
        break;

    case ShellSignalIllegalInstruction:
        OsSignalNumber = SIGILL;
        break;

    case ShellSignalTrap:
        OsSignalNumber = SIGTRAP;
        break;

    case ShellSignalAbort:
        OsSignalNumber = SIGABRT;
        break;

    case ShellSignalFloatingPointException:
        OsSignalNumber = SIGFPE;
        break;

    case ShellSignalKill:
        OsSignalNumber = SIGKILL;
        break;

    case ShellSignalBusError:
        OsSignalNumber = SIGBUS;
        break;

    case ShellSignalSegmentationFault:
        OsSignalNumber = SIGSEGV;
        break;

    case ShellSignalBadSystemCall:
        OsSignalNumber = SIGSYS;
        break;

    case ShellSignalPipe:
        OsSignalNumber = SIGPIPE;
        break;

    case ShellSignalAlarm:
        OsSignalNumber = SIGALRM;
        break;

    case ShellSignalTerminate:
        OsSignalNumber = SIGTERM;
        break;

    case ShellSignalUrgentData:
        OsSignalNumber = SIGURG;
        break;

    case ShellSignalStop:
        OsSignalNumber = SIGSTOP;
        break;

    case ShellSignalTerminalStop:
        OsSignalNumber = SIGTSTP;
        break;

    case ShellSignalContinue:
        OsSignalNumber = SIGCONT;
        break;

    case ShellSignalChild:
        OsSignalNumber = SIGCHLD;
        break;

    case ShellSignalTerminalInput:
        OsSignalNumber = SIGTTIN;
        break;

    case ShellSignalTerminalOutput:
        OsSignalNumber = SIGTTOU;
        break;

    case ShellSignalCpuTime:
        OsSignalNumber = SIGXCPU;
        break;

    case ShellSignalFileSize:
        OsSignalNumber = SIGXFSZ;
        break;

    case ShellSignalVirtualTimeAlarm:
        OsSignalNumber = SIGVTALRM;
        break;

    case ShellSignalProfiling:
        OsSignalNumber = SIGPROF;
        break;

    case ShellSignalWindowChange:
        OsSignalNumber = SIGWINCH;
        break;

    case ShellSignalUser1:
        OsSignalNumber = SIGUSR1;
        break;

    case ShellSignalUser2:
        OsSignalNumber = SIGUSR2;
        break;

    default:
        return 0;
    }

    return OsSignalNumber;
}

