/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    shos.h

Abstract:

    This header contains definitions for the operating system interface to the
    shell.

Author:

    Evan Green 14-Jun-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of file descriptors (aka redirection numbers) reserved for
// the user. If the user goes above this, they risk colliding with file
// descriptors open by the shell itself.
//

#define SHELL_MINIMUM_FILE_DESCRIPTOR 10

//
// Define the base status number returned when the OS exit status indicates
// the process terminated due to a signal or other abnormal condition.
//

#define SHELL_EXIT_SIGNALED 256

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
void
(*PSH_THREAD_ROUTINE) (
    void *Context
    );

/*++

Routine Description:

    This routine runs a new thread.

Arguments:

    Context - Supplies an opaque pointer of context from the thread creator.

Return Value:

    None.

--*/

typedef enum _SHELL_SIGNAL {
    ShellSignalOnExit                 = 0,
    ShellSignalHangup                 = 1,
    ShellSignalInterrupt              = 2,
    ShellSignalQuit                   = 3,
    ShellSignalIllegalInstruction     = 4,
    ShellSignalTrap                   = 5,
    ShellSignalAbort                  = 6,
    ShellSignalFloatingPointException = 8,
    ShellSignalKill                   = 9,
    ShellSignalBusError               = 10,
    ShellSignalSegmentationFault      = 11,
    ShellSignalBadSystemCall          = 12,
    ShellSignalPipe                   = 13,
    ShellSignalAlarm                  = 14,
    ShellSignalTerminate              = 15,
    ShellSignalUrgentData             = 16,
    ShellSignalStop                   = 17,
    ShellSignalTerminalStop           = 18,
    ShellSignalContinue               = 19,
    ShellSignalChild                  = 20,
    ShellSignalTerminalInput          = 21,
    ShellSignalTerminalOutput         = 22,
    ShellSignalCpuTime                = 24,
    ShellSignalFileSize               = 25,
    ShellSignalVirtualTimeAlarm       = 26,
    ShellSignalProfiling              = 27,
    ShellSignalWindowChange           = 28,
    ShellSignalUser1                  = 30,
    ShellSignalUser2                  = 31,
    ShellSignalCount,
    ShellSignalInvalid                = 101,
} SHELL_SIGNAL, *PSHELL_SIGNAL;

typedef enum _SHELL_SIGNAL_DISPOSITION {
    ShellSignalDispositionDefault,
    ShellSignalDispositionIgnore,
    ShellSignalDispositionTrap,
} SHELL_SIGNAL_DISPOSITION, *PSHELL_SIGNAL_DISPOSITION;

/*++

Structure Description:

    This structure defines the shell process times information, returned by the
    times builtin.

Members:

    ShellUserMinutes - Stores the number of minutes the shell has spent in
        user mode code.

    ShellUserMicroseconds - Stores the number of microseconds the shell has
        spent in user mode code in addition to the number of minutes.

    ShellSystemMinutes - Stores the number of minutes the shell has spent in
        kerel mode code.

    ShellSystemMicroseconds - Stores the number of microseconds the shell has
        spent in kernel mode code in addition to the number of minutes.

    ChildrenUserMinutes - Stores the number of minutes the shell's chilren have
        spent in user mode code.

    ChildrenUserMicroseconds - Stores the number of microseconds the shell's
        children have spent in user mode code in addition to the number of
        minutes.

    ChildrenSystemMinutes - Stores the number of minutes the shell's children
        have spent in kerel mode code.

    ChildrenSystemMicroseconds - Stores the number of microseconds the shell's
        children have spent in kernel mode code in addition to the number of
        minutes.

--*/

typedef struct _SHELL_PROCESS_TIMES {
    unsigned long long ShellUserMinutes;
    unsigned long ShellUserMicroseconds;
    unsigned long long ShellSystemMinutes;
    unsigned long ShellSystemMicroseconds;
    unsigned long long ChildrenUserMinutes;
    unsigned long ChildrenUserMicroseconds;
    unsigned long long ChildrenSystemMinutes;
    unsigned long ChildrenSystemMicroseconds;
} SHELL_PROCESS_TIMES, *PSHELL_PROCESS_TIMES;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a global that's non-zero if this OS supports an executable permission
// bit.
//

extern int ShExecutableBitSupported;

//
// -------------------------------------------------------- Function Prototypes
//

//
// Common shell functions the OS layer can call.
//

void
ShSignalHandler (
    int SignalNumber
    );

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

//
// OS functions called by the common shell portion.
//

int
ShGetHomeDirectory (
    char *User,
    int UserSize,
    char **HomePath,
    int *HomePathSize
    );

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

int
ShCreatePipe (
    int Descriptors[2]
    );

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

int
ShPrepareForOutputCollection (
    int FileDescriptorToRead,
    void **Handle
    );

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

int
ShCollectOutput (
    void *Handle,
    char **Output,
    unsigned long *OutputSize
    );

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

int
ShPushInputText (
    char *Text,
    unsigned long TextSize,
    int Pipe[2]
    );

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

int
ShFixUpPath (
    char **Path,
    unsigned long *PathSize
    );

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

char *
ShGetEnvironmentVariable (
    char *Name
    );

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

int
ShSetEnvironmentVariable (
    char *Name,
    char *Value
    );

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

int
ShUnsetEnvironmentVariable (
    char *Name
    );

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

int
ShGetExecutionTimes (
    PSHELL_PROCESS_TIMES Times
    );

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

int
ShSetSignalDisposition (
    SHELL_SIGNAL Signal,
    SHELL_SIGNAL_DISPOSITION Disposition
    );

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

void
ShRestoreOriginalSignalDispositions (
    void
    );

/*++

Routine Description:

    This routine restores all the signal dispositions back to their original
    state.

Arguments:

    None.

Return Value:

    None.

--*/

void
ShGetExecutableExtensions (
    char ***ExtensionList,
    unsigned int *ElementCount
    );

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

int
ShSetDescriptorFlags (
    int FileDescriptor,
    int Inheritable
    );

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

int
ShOsDup (
    int FileDescriptor
    );

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

void
ShOsConvertExitStatus (
    int *Status
    );

/*++

Routine Description:

    This routine converts an OS exit status code into a shell exit status code.

Arguments:

    Status - Supplies a pointer that on input contains the OS-specific exit
        status code. On output, returns the shell exit status code.

Return Value:

    None.

--*/

