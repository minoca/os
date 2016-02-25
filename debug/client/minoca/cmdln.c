/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    cmdln.c

Abstract:

    This module implements the command line debugger.

Author:

    Evan Green 27-May-2013

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <osbase.h>
#include <mlibc.h>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "dbgapi.h"
#include "dbgrprof.h"
#include "console.h"
#include "userdbg.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define a comfortable size that will fit most complete module lists.
//

#define INITIAL_MODULE_LIST_SIZE 512

//
// Define a comfortable size that will fit most complete thread lists.
//

#define INITIAL_THREAD_LIST_SIZE 256

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the ID of the terminals' initial process group.
//

pid_t DbgInitialTerminalInputForegroundProcessGroup;
pid_t DbgInitialTerminalOutputForegroundProcessGroup;
pid_t DbgInitialTerminalErrorForegroundProcessGroup;

//
// Store the ID of the currently broken in process.
//

pid_t DbgTargetProcessId;

//
// Store the desired terminal parameters and the previous terminal parameters.
//

struct termios DbgTerminalSettings;
struct termios DbgOriginalTerminalSettings;

//
// Store the ID of the terminal's original foreground process group.
//

pid_t DbgOrignalTerminalForegroundProcessGroupId;

//
// Store the remote pipe.
//

int DbgRemoteInputPipe[2] = {-1, -1};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the program. It collects the
    options passed to it, and creates the output image.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    return DbgrMain(ArgumentCount, Arguments);
}

BOOL
DbgrOsInitializeConsole (
    PBOOL EchoCommands
    )

/*++

Routine Description:

    This routine performs any initialization steps necessary before the console
    can be used.

Arguments:

    EchoCommands - Supplies a pointer where a boolean will be returned
        indicating if the debugger should echo commands received (TRUE) or if
        the console has already echoed the command (FALSE).

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    int Result;

    if (tcgetattr(STDIN_FILENO, &DbgTerminalSettings) != 0) {
        perror("Cannot get terminal settings");
        return FALSE;
    }

    //
    // Set 8 bit characters.
    //

    DbgTerminalSettings.c_cflag |= CS8;

    //
    // Change the local mode to enable canonical mode, echo, erase, extended
    // functions, and signal characters.
    //

    DbgTerminalSettings.c_lflag |= ECHO | ICANON | ISIG | ECHONL;
    DbgInitialTerminalInputForegroundProcessGroup = tcgetpgrp(STDIN_FILENO);
    DbgInitialTerminalOutputForegroundProcessGroup = tcgetpgrp(STDOUT_FILENO);
    DbgInitialTerminalErrorForegroundProcessGroup = tcgetpgrp(STDERR_FILENO);
    Result = pipe(DbgRemoteInputPipe);
    if (Result != 0) {
        return FALSE;
    }

    return TRUE;
}

VOID
DbgrOsDestroyConsole (
    VOID
    )

/*++

Routine Description:

    This routine cleans up anything related to console functionality as a
    debugger is exiting.

Arguments:

    None.

Return Value:

    None.

--*/

{

    struct sigaction OriginalAction;
    int Result;
    struct sigaction SignalAction;

    //
    // Temporarily ignore SIGTTOU as the current process may not be in the
    // foreground process group, causing the SIGTTOU signal to fire.
    //

    memset(&SignalAction, 0, sizeof(struct sigaction));
    SignalAction.sa_handler = SIG_IGN;
    Result = sigaction(SIGTTOU, &SignalAction, &OriginalAction);
    if (Result == 0) {
        tcsetpgrp(STDIN_FILENO, DbgInitialTerminalInputForegroundProcessGroup);
        tcsetpgrp(STDOUT_FILENO,
                  DbgInitialTerminalOutputForegroundProcessGroup);

        tcsetpgrp(STDERR_FILENO, DbgInitialTerminalErrorForegroundProcessGroup);
        sigaction(SIGTTOU, &OriginalAction, NULL);
    }

    if (DbgRemoteInputPipe[0] != -1) {
        close(DbgRemoteInputPipe[0]);
    }

    if (DbgRemoteInputPipe[1] != -1) {
        close(DbgRemoteInputPipe[1]);
    }

    return;
}

INT
DbgrOsCreateThread (
    PDBGR_THREAD_ROUTINE ThreadRoutine,
    PVOID Parameter
    )

/*++

Routine Description:

    This routine creates a new thread.

Arguments:

    ThreadRoutine - Supplies a pointer to the routine to run in the new thread.
        The thread is destroyed when the supplied routine returns.

    Parameter - Supplies a pointer to a parameter to pass to the thread.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;
    KSTATUS Status;

    //
    // TODO: Convert this to pthread_create.
    //

    Result = 0;
    Status = OsCreateThread(NULL,
                            0,
                            ThreadRoutine,
                            Parameter,
                            NULL,
                            0,
                            NULL,
                            NULL);

    if (!KSUCCESS(Status)) {
        Result = ClConvertKstatusToErrorNumber(Status);
    }

    return Result;
}

int
DbgrOsCreatePipe (
    int FileDescriptors[2]
    )

/*++

Routine Description:

    This routine creates an anonymous pipe.

Arguments:

    FileDescriptors - Supplies a pointer where handles will be returned
        representing the read and write ends of the pipe.

Return Value:

    0 on success.

    -1 on failure. The errno variable will be set to indicate the error.

--*/

{

    return pipe(FileDescriptors);
}

char *
DbgrOsGetUserName (
    void
    )

/*++

Routine Description:

    This routine returns the user name of the current process.

Arguments:

    None.

Return Value:

    Returns a pointer to a string containing the user name if it can be found.
    The caller should not free or modify this memory, and it may be reused on
    subsequent calls.

--*/

{

    struct passwd *Information;

    Information = getpwuid(geteuid());
    if ((Information == NULL) || (Information->pw_name == NULL) ||
        (strlen(Information->pw_name) == 0)) {

        return getenv("USER");
    }

    return Information->pw_name;
}

char *
DbgrOsGetHostName (
    void
    )

/*++

Routine Description:

    This routine returns the host name of the current machine.

Arguments:

    None.

Return Value:

    Returns a pointer to a string containing the user name if it can be found.
    The caller is responsible for freeing this memory.

--*/

{

    char LocalHost[100];
    int Result;

    Result = gethostname(LocalHost, sizeof(LocalHost));
    if (Result != 0) {
        return NULL;
    }

    return strdup(LocalHost);
}

VOID
DbgrOsPrepareToReadInput (
    VOID
    )

/*++

Routine Description:

    This routine is called before the debugger begins to read a line of input
    from the user.

Arguments:

    None.

Return Value:

    None.

--*/

{

    struct sigaction OriginalAction;
    int Result;
    struct sigaction SignalAction;

    if (tcgetattr(STDIN_FILENO, &DbgOriginalTerminalSettings) != 0) {
        return;
    }

    DbgOrignalTerminalForegroundProcessGroupId = tcgetpgrp(STDIN_FILENO);
    tcsetattr(STDIN_FILENO, TCSANOW, &DbgTerminalSettings);

    //
    // Make the debugger's process group the foreground process ground. This
    // was saved when the debugger launched. Ignore SIGTTOU for this operation,
    // otherwise the debugger will be sent a stop signal as its in the
    // background process group.
    //

    memset(&SignalAction, 0, sizeof(struct sigaction));
    SignalAction.sa_handler = SIG_IGN;
    Result = sigaction(SIGTTOU, &SignalAction, &OriginalAction);
    if (Result == 0) {
        tcsetpgrp(STDIN_FILENO, DbgInitialTerminalInputForegroundProcessGroup);
        sigaction(SIGTTOU, &OriginalAction, NULL);
    }

    return;
}

BOOL
DbgrOsGetCharacter (
    PUCHAR Key,
    PUCHAR ControlKey
    )

/*++

Routine Description:

    This routine gets one character from the standard input console.

Arguments:

    Key - Supplies a pointer that receives the printable character. If this
        parameter is NULL, printing characters will be discarded from the input
        buffer.

    ControlKey - Supplies a pointer that receives the non-printable character.
        If this parameter is NULL, non-printing characters will be discarded
        from the input buffer.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    int Character;
    UCHAR ControlKeyValue;
    struct pollfd Events[2];
    int Result;

    ControlKeyValue = 0;

    //
    // If standard in is ready to go, then just read that.
    //

    if (__freadahead(stdin) != 0) {
        Character = fgetc(stdin);

        assert(Character != -1);

    } else {
        while (TRUE) {
            fflush(NULL);

            //
            // Wait for either standard in or a remote command.
            //

            Events[0].fd = fileno(stdin);
            Events[0].events = POLLIN;
            Events[0].revents = 0;
            Events[1].fd = DbgRemoteInputPipe[1];
            Events[1].events = POLLIN;
            Events[1].revents = 0;
            Result = poll(Events, 2, -1);
            if (Result == -1) {
                if (errno == EINTR) {
                    continue;

                } else {
                    DbgOut("Failed to poll: %s\n", strerror(errno));
                    return FALSE;
                }
            }

            //
            // Grab a character from standard in.
            //

            if ((Events[0].revents & POLLIN) != 0) {
                Character = fgetc(stdin);
                if (Character == -1) {
                    if (errno == EINTR) {
                        continue;
                    }

                    DbgOut("Failed to get character. Errno %d\n", errno);
                    return FALSE;
                }

                break;

            //
            // Perform the read from the pipe to get the data out. The data
            // itself doesn't matter, the pipe is just a signaling mechanism.
            //

            } else if ((Events[1].revents & POLLIN) != 0) {
                read(DbgRemoteInputPipe[1], &Character, 1);
                Character = 0;
                ControlKeyValue = KEY_REMOTE;
                break;

            } else {
                DbgOut("Poll succeeded, but nothing available.\n");
            }
        }
    }

    //
    // Handle non-printing characters.
    //

    if (Character == '\n') {
        Character = 0;
        ControlKeyValue = KEY_RETURN;
    }

    if (Key != NULL) {
        *Key = Character;
    }

    if (ControlKey != NULL) {
        *ControlKey = ControlKeyValue;
    }

    return TRUE;
}

VOID
DbgrOsRemoteInputAdded (
    VOID
    )

/*++

Routine Description:

    This routine is called after a remote command is received and placed on the
    standard input remote command list. It wakes up a thread blocked on local
    user input in an OS-dependent fashion.

Arguments:

    None.

Return Value:

    None.

--*/

{

    char Character;

    //
    // It doesn't matter what the character is, just write something into the
    // pipe being used for inter-thread communication.
    //

    Character = 'r';
    write(DbgRemoteInputPipe[1], &Character, 1);
    return;
}

VOID
DbgrOsPostInputCallback (
    VOID
    )

/*++

Routine Description:

    This routine is called after a line of input is read from the user, giving
    the OS specific code a chance to restore anything it did in the prepare
    to read input function.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // This change of the foreground process group does not need to ignore
    // SIGTTOU because the debugger should be in the current foreground process
    // group.
    //

    tcsetpgrp(STDIN_FILENO, DbgOrignalTerminalForegroundProcessGroupId);
    tcsetattr(STDIN_FILENO, TCSANOW, &DbgOriginalTerminalSettings);
    return;
}

BOOL
UiLoadSourceFile (
    PSTR Path,
    PVOID Contents,
    ULONGLONG Size
    )

/*++

Routine Description:

    This routine loads the contents of a file into the source window.

Arguments:

    Path - Supplies the path of the file being loaded. If this is NULL, then
        the source window should be cleared.

    Contents - Supplies the source file data. This can be NULL.

    Size - Supplies the size of the source file data in bytes.

Return Value:

    Returns TRUE if there was no error, or FALSE if there was an error.

--*/

{

    return TRUE;
}

BOOL
UiHighlightExecutingLine (
    LONG LineNumber,
    BOOL Enable
    )

/*++

Routine Description:

    This routine highlights the currently executing source line and scrolls to
    it.

Arguments:

    LineNumber - Supplies the 1-based line number to highlight (ie the first
        line in the source file is line 1).

    Enable - Supplies a flag indicating whether to highlight this line (TRUE)
        or restore the line to its original color (FALSE).

Return Value:

    Returns TRUE if there was no error, or FALSE if there was an error.

--*/

{

    return TRUE;
}

VOID
UiEnableCommands (
    BOOL Enable
    )

/*++

Routine Description:

    This routine enables or disables the command edit control from being
    enabled. If disabled, the edit control will be made read only.

Arguments:

    Enable - Supplies a flag indicating whether or not to enable (TRUE) or
        disable (FALSE) the command box.

Return Value:

    None.

--*/

{

    return;
}

VOID
UiSetCommandText (
    PSTR Text
    )

/*++

Routine Description:

    This routine sets the text inside the command edit box.

Arguments:

    Text - Supplies a pointer to a null terminated string to set the command
        text to.

Return Value:

    None.

--*/

{

    return;
}

VOID
UiSetPromptText (
    PSTR Text
    )

/*++

Routine Description:

    This routine sets the text inside the prompt edit box.

Arguments:

    Text - Supplies a pointer to a null terminated string to set the prompt
        text to.

Return Value:

    None.

--*/

{

    return;
}

VOID
UiDisplayProfilerData (
    PROFILER_DATA_TYPE DataType,
    PROFILER_DISPLAY_REQUEST DisplayRequest,
    ULONG Threshold
    )

/*++

Routine Description:

    This routine displays the profiler data collected by the core debugging
    infrastructure.

Arguments:

    DataType - Supplies the type of profiler data that is to be displayed.

    DisplayRequest - Supplies a value requesting a display action, which can
        either be to display data once, continually, or to stop continually
        displaying data.

    Threshold - Supplies the minimum percentage a stack entry hit must be in
        order to be displayed.

Return Value:

    None.

--*/

{

    DbgrDisplayCommandLineProfilerData(DataType, DisplayRequest, Threshold);
    return;
}

BOOL
InitializeCommunications (
    PSTR Channel,
    ULONG Baudrate
    )

/*++

Routine Description:

    This routine initializes the communication medium the debugger uses to
    communicate with the target.

Arguments:

    Channel - Supplies a description of the communication medium.

    Baudrate - Supplies the baudrate to use for serial based communications.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    DbgOut("Error: Kernel debugging is not yet supported.\n");
    return FALSE;
}

VOID
DestroyCommunications (
    VOID
    )

/*++

Routine Description:

    This routine tears down the debug communication channel.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

BOOL
CommReceive (
    PVOID Buffer,
    ULONG BytesToRead
    )

/*++

Routine Description:

    This routine receives a number of bytes from the debugger/debuggee
    connection.

Arguments:

    Buffer - Supplies a pointer to the buffer where the data should be returned.

    BytesToRead - Supplies the number of bytes that should be received into the
        buffer.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    DbgOut("Error: Kernel debugging is not yet supported.\n");
    return FALSE;
}

BOOL
CommSend (
    PVOID Buffer,
    ULONG BytesToSend
    )

/*++

Routine Description:

    This routine sends a number of bytes through the debugger/debuggee
    connection.

Arguments:

    Buffer - Supplies a pointer to the buffer where the data to be sent resides.

    BytesToSend - Supplies the number of bytes that should be sent.

Return Value:

    Returns TRUE on success, FALSE on failure.

--*/

{

    DbgOut("Error: Kernel debugging is not yet supported.\n");
    return FALSE;
}

ULONG
CommReceiveBytesReady (
    VOID
    )

/*++

Routine Description:

    This routine determines how many bytes of data are ready to be read from
    the communication channel.

Arguments:

    None.

Return Value:

    Returns the number of bytes ready to be read.

--*/

{

    DbgOut("Error: Kernel debugging is not yet supported.\n");
    return 0;
}

VOID
CommStall (
    ULONG Milliseconds
    )

/*++

Routine Description:

    This routine pauses for the given amount of time.

Arguments:

    Milliseconds - Supplies the amount of time, in milliseconds, to stall the
        current thread for.

Return Value:

    None.

--*/

{

    poll(NULL, 0, Milliseconds);
    return;
}

BOOL
LaunchChildProcess (
    ULONG ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine launches a new child process to be debugged.

Arguments:

    ArgumentCount - Supplies the number of command line arguments for the
        executable.

    Arguments - Supplies the array of arguments to pass.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    pid_t Child;
    struct sigaction OriginalAction;
    int Result;
    struct sigaction SignalAction;
    KSTATUS Status;

    //
    // Set SIGTTOU to be ignored. Both the child and the parent are going to
    // attempt to make the child's process group the foreground process group
    // of the controlling terminal. If the child gets there first, it would be
    // calling from a background process group. The parent would also fail if
    // STDIN, STDOUT, and/or STDERR are attached to the same terminal.
    //

    memset(&SignalAction, 0, sizeof(struct sigaction));
    SignalAction.sa_handler = SIG_IGN;
    Result = sigaction(SIGTTOU, &SignalAction, &OriginalAction);
    if (Result != 0) {
        return FALSE;
    }

    Child = fork();
    if (Child == -1) {
        DbgOut("Error: Failed to fork into new process. Errno: %d\n", errno);
        return FALSE;
    }

    //
    // If this is the child process, enable debugging and launch the process.
    //

    if (Child == 0) {
        Status = OsDebug(DebugCommandEnableDebugging, 0, NULL, NULL, 0, 0);
        if (!KSUCCESS(Status)) {
            DbgOut("Error: Failed to enable debugging on child process. "
                   "Status %x\n",
                   Status);

            exit(1);
        }

        //
        // Just like a child would in a shell, create a new process group and
        // make it the controlling terminal's foreground process group.
        //

        Child = getpid();
        setpgid(Child, Child);
        tcsetpgrp(STDOUT_FILENO, Child);
        tcsetpgrp(STDIN_FILENO, Child);
        tcsetpgrp(STDERR_FILENO, Child);

        //
        // Be the ball.
        //

        Result = execvp(Arguments[0], (char *const *)Arguments);
        DbgOut("Error: Failed to execute image \"%s\"\n", Arguments[0]);
        exit(Result);

    //
    // If this is the parent, make sure the child is in its own process group
    // and is the foreground process ground of the controlling terminal. Then
    // continue on.
    //

    } else {
        setpgid(Child, Child);
        tcsetpgrp(STDOUT_FILENO, Child);
        tcsetpgrp(STDIN_FILENO, Child);
        tcsetpgrp(STDERR_FILENO, Child);
        DbgOut("Created process %x.\n", Child);

        //
        // Return SIGTTOU to its original state. This does not need to happen
        // in the child as the exec call wipes out the original handlers.
        //

        sigaction(SIGTTOU, &OriginalAction, NULL);
    }

    return TRUE;
}

BOOL
DbgpUserContinue (
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine sends the "go" command to the target, signaling to continue
    execution.

Arguments:

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    KSTATUS Status;

    Status = OsDebug(DebugCommandContinue,
                     DbgTargetProcessId,
                     NULL,
                     NULL,
                     0,
                     SignalToDeliver);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to continue process %x. Status %x\n",
               DbgTargetProcessId,
               Status);

        return FALSE;
    }

    return TRUE;
}

BOOL
DbgpUserSetRegisters (
    PREGISTERS_UNION Registers
    )

/*++

Routine Description:

    This routine sets the registers of the debugging target.

Arguments:

    Registers - Supplies a pointer to the registers to set. All register values
        will be written.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    BREAK_NOTIFICATION Break;
    KSTATUS Status;

    //
    // Get the break information.
    //

    Status = OsDebug(DebugCommandGetBreakInformation,
                     DbgTargetProcessId,
                     NULL,
                     &Break,
                     sizeof(Break),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to get break information. Status %x\n", Status);
        return FALSE;
    }

    //
    // Set the registers and then set the break information.
    //

    RtlCopyMemory(&(Break.Registers), Registers, sizeof(REGISTERS_UNION));
    Status = OsDebug(DebugCommandSetBreakInformation,
                     DbgTargetProcessId,
                     NULL,
                     &Break,
                     sizeof(Break),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to set break information. Status %x\n", Status);
        return FALSE;
    }

    return TRUE;
}

BOOL
DbgpUserSingleStep (
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine steps the target by one instruction.

Arguments:

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE if there was an error.

--*/

{

    KSTATUS Status;

    Status = OsDebug(DebugCommandSingleStep,
                     DbgTargetProcessId,
                     NULL,
                     NULL,
                     0,
                     SignalToDeliver);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to continue process %x. Status %x\n",
               DbgTargetProcessId,
               Status);

        return FALSE;
    }

    return TRUE;
}

BOOL
DbgpUserWaitForEvent (
    PDEBUGGER_EVENT Event
    )

/*++

Routine Description:

    This routine gets an event from the target, such as a break event or other
    exception.

Arguments:

    Event - Supplies a pointer where the event details will be returned.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    pid_t Process;
    int ProcessStatus;
    KSTATUS Status;

    //
    // Block until something happens.
    //

    while (TRUE) {
        Process = waitpid(-1, &ProcessStatus, WUNTRACED | WCONTINUED);
        if (Process == -1) {
            if (errno == EINTR) {
                continue;
            }

            DbgOut("Error: Failed to wait(): %s\n", strerror(errno));
            return FALSE;
        }

        break;
    }

    //
    // Handle the process exiting.
    //

    if (WIFEXITED(ProcessStatus)) {
        Event->Type = DebuggerEventShutdown;
        Event->ShutdownNotification.ShutdownType = ShutdownTypeExit;
        Event->ShutdownNotification.Process = Process;
        Event->ShutdownNotification.ExitStatus = ProcessStatus;
        DbgTargetProcessId = -1;
        return TRUE;
    }

    DbgTargetProcessId = Process;

    //
    // Get the break information.
    //

    Status = OsDebug(DebugCommandGetBreakInformation,
                     Process,
                     NULL,
                     &(Event->BreakNotification),
                     sizeof(Event->BreakNotification),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to get break information. Status %x\n", Status);
        return FALSE;
    }

    assert(Event->BreakNotification.Process == Process);

    //
    // Get the signal information.
    //

    Status = OsDebug(DebugCommandGetSignalInformation,
                     Process,
                     NULL,
                     &(Event->SignalParameters),
                     sizeof(Event->SignalParameters),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to get signal information. Status %x\n", Status);
        return FALSE;
    }

    Event->Type = DebuggerEventBreak;
    return TRUE;
}

BOOL
DbgpUserRangeStep (
    PRANGE_STEP RangeStep,
    ULONG SignalToDeliver
    )

/*++

Routine Description:

    This routine continues execution until a range of execution addresses is
    reached.

Arguments:

    RangeStep - Supplies a pointer to the range to go to.

    SignalToDeliver - Supplies the signal number to actually send to the
        application. For kernel debugging, this parameter is ignored.

Return Value:

    Returns TRUE if successful, or FALSE on failure.

--*/

{

    PROCESS_DEBUG_BREAK_RANGE BreakRange;
    KSTATUS Status;

    RtlZeroMemory(&BreakRange, sizeof(PROCESS_DEBUG_BREAK_RANGE));
    BreakRange.BreakRangeStart = (PVOID)(UINTN)RangeStep->BreakRangeMinimum;
    BreakRange.BreakRangeEnd = (PVOID)(UINTN)RangeStep->BreakRangeMaximum;
    BreakRange.RangeHoleStart = (PVOID)(UINTN)RangeStep->RangeHoleMinimum;
    BreakRange.RangeHoleEnd = (PVOID)(UINTN)RangeStep->RangeHoleMaximum;
    Status = OsDebug(DebugCommandRangeStep,
                     DbgTargetProcessId,
                     NULL,
                     &BreakRange,
                     sizeof(PROCESS_DEBUG_BREAK_RANGE),
                     SignalToDeliver);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Failed to range step process %x. Status %x.\n",
               DbgTargetProcessId,
               Status);

        return FALSE;
    }

    return TRUE;
}

BOOL
DbgpUserReadWriteMemory (
    BOOL WriteOperation,
    BOOL VirtualMemory,
    ULONGLONG Address,
    PVOID Buffer,
    ULONG BufferSize,
    PULONG BytesCompleted
    )

/*++

Routine Description:

    This routine retrieves or writes to the target's memory.

Arguments:

    WriteOperation - Supplies a flag indicating whether this is a read
        operation (FALSE) or a write operation (TRUE).

    VirtualMemory - Supplies a flag indicating whether the memory accessed
        should be virtual or physical.

    Address - Supplies the address to read from or write to in the target's
        memory.

    Buffer - Supplies a pointer to the buffer where the memory contents will be
        returned for read operations, or supplies a pointer to the values to
        write to memory on for write operations.

    BufferSize - Supplies the size of the supplied buffer, in bytes.

    BytesCompleted - Supplies a pointer that receive the number of bytes that
        were actually read from or written to the target.

Return Value:

    Returns TRUE if the operation was successful.

    FALSE if there was an error.

--*/

{

    DEBUG_COMMAND_TYPE Command;
    KSTATUS Status;

    if (VirtualMemory == FALSE) {
        DbgOut("Error: Writing to physical memory in user mode is not "
               "allowed.\n");

        return FALSE;
    }

    Command = DebugCommandReadMemory;
    if (WriteOperation != FALSE) {
        Command = DebugCommandWriteMemory;
    }

    Status = OsDebug(Command,
                     DbgTargetProcessId,
                     (PVOID)(UINTN)Address,
                     Buffer,
                     BufferSize,
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Unable to read memory at %I64x. Status %x\n",
               Address,
               Status);

        if (BytesCompleted != NULL) {
            *BytesCompleted = 0;
        }

        return FALSE;
    }

    if (BytesCompleted != NULL) {
        *BytesCompleted = BufferSize;
    }

    return TRUE;
}

BOOL
DbgpUserGetThreadList (
    PULONG ThreadCount,
    PULONG *ThreadIds
    )

/*++

Routine Description:

    This routine gets the list of active threads in the process (or active
    processors in the machine for kernel mode).

Arguments:

    ThreadCount - Supplies a pointer where the number of threads will be
        returned on success.

    ThreadIds - Supplies a pointer where an array of thread IDs (or processor
        numbers) will be returned on success. It is the caller's responsibility
        to free this memory.

Return Value:

    Returns TRUE if successful, FALSE on failure.

--*/

{

    PULONG ListBuffer;
    PULONG ListToReturn;
    ULONG Size;
    KSTATUS Status;
    PTHREAD_ID ThreadIdList;
    ULONG ThreadIndex;

    Size = INITIAL_THREAD_LIST_SIZE;
    ListBuffer = malloc(Size);
    if (ListBuffer == NULL) {
        DbgOut("Error: Failed to allocate %d bytes for thread list.\n", Size);
        return FALSE;
    }

    ListToReturn = malloc(Size);
    if (ListToReturn == NULL) {
        DbgOut("Error: Failed to allocate %d bytes for thread list.\n", Size);
        return FALSE;
    }

    while (TRUE) {
        Status = OsDebug(DebugCommandGetThreadList,
                         DbgTargetProcessId,
                         NULL,
                         ListBuffer,
                         Size,
                         0);

        //
        // On success, the buffer starts with a thread count, and then an array
        // of that may IDs. Copy the thread IDs over to the list.
        //

        if (KSUCCESS(Status)) {
            *ThreadCount = *ListBuffer;
            ThreadIdList = (PTHREAD_ID)(ListBuffer + 1);
            for (ThreadIndex = 0;
                 ThreadIndex < *ThreadCount;
                 ThreadIndex += 1) {

                ListToReturn[ThreadIndex] = (ULONG)ThreadIdList[ThreadIndex];
            }

            free(ListBuffer);
            *ThreadIds = ListToReturn;
            return TRUE;
        }

        //
        // Double the size of the buffer and try again.
        //

        if (Status == STATUS_BUFFER_TOO_SMALL) {
            free(ListBuffer);
            free(ListToReturn);
            Size *= 2;
            ListBuffer = malloc(Size);
            if (ListBuffer == NULL) {
                DbgOut("Error: Failed to allocate %d bytes for module list.\n",
                       Size);

                return FALSE;
            }

            ListToReturn = malloc(Size);
            if (ListToReturn == NULL) {
                DbgOut("Error: Failed to allocate %d bytes for module list.\n",
                       Size);

                return FALSE;
            }

            continue;
        }

        //
        // Some other error occurred.
        //

        DbgOut("Error: Unable to get module list for process %x. Status %x\n",
               DbgTargetProcessId,
               Status);

        free(ListBuffer);
        free(ListToReturn);
        return FALSE;
    }

    //
    // Execution should never get here.
    //

    return FALSE;
}

BOOL
DbgpUserSwitchThread (
    ULONG ThreadId,
    PDEBUGGER_EVENT NewBreakInformation
    )

/*++

Routine Description:

    This routine switches the debugger to another thread.

Arguments:

    ThreadId - Supplies the ID of the thread to switch to.

    NewBreakInformation - Supplies a pointer where the updated break information
        will be returned.

Return Value:

    Returns TRUE if successful, or FALSE if there was no change.

--*/

{

    BOOL Result;
    KSTATUS Status;
    ULONG ThreadCount;
    ULONG ThreadIndex;
    PULONG ThreadList;
    BOOL Valid;

    //
    // First ensure that the destination thread is a viable thread.
    //

    Result = DbgpUserGetThreadList(&ThreadCount, &ThreadList);
    if (Result == FALSE) {
        DbgOut("Error: Unable to get thread list for thread switch.\n");
        return FALSE;
    }

    Valid = FALSE;
    for (ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex += 1) {
        if (ThreadList[ThreadIndex] == ThreadId) {
            Valid = TRUE;
            break;
        }
    }

    if (Valid == FALSE) {
        DbgOut("Error: %x does not appear to be a valid thread.\n", ThreadId);
        return FALSE;
    }

    Status = OsDebug(DebugCommandSwitchThread,
                     DbgTargetProcessId,
                     (PVOID)ThreadId,
                     NULL,
                     0,
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Unable to switch to thread %x. Status %x\n",
               ThreadId,
               Status);

        return FALSE;
    }

    //
    // Get the new break information.
    //

    Status = OsDebug(DebugCommandGetBreakInformation,
                     DbgTargetProcessId,
                     NULL,
                     &(NewBreakInformation->BreakNotification),
                     sizeof(NewBreakInformation->BreakNotification),
                     0);

    if (!KSUCCESS(Status)) {
        DbgOut("Error: Unable to get break information after thread switch. "
               "Status %x\n",
               Status);
    }

    return TRUE;
}

BOOL
DbgpUserGetLoadedModuleList (
    PMODULE_LIST_HEADER *ModuleList
    )

/*++

Routine Description:

    This routine retrieves the list of loaded binaries from the kernel
    debugging target.

Arguments:

    ModuleList - Supplies a pointer where a pointer to the loaded module header
        and subsequent array of entries will be returned. It is the caller's
        responsibility to free this allocated memory when finished.

Return Value:

    Returns TRUE on success, or FALSE on failure.

--*/

{

    PMODULE_LIST_HEADER List;
    ULONG Size;
    KSTATUS Status;

    Size = INITIAL_MODULE_LIST_SIZE;
    List = malloc(Size);
    if (List == NULL) {
        DbgOut("Error: Failed to allocate %d bytes for module list.\n", Size);
        return FALSE;
    }

    while (TRUE) {
        Status = OsDebug(DebugCommandGetLoadedModules,
                         DbgTargetProcessId,
                         NULL,
                         List,
                         Size,
                         0);

        if (KSUCCESS(Status)) {
            *ModuleList = List;
            return TRUE;
        }

        //
        // Double the size of the buffer and try again.
        //

        if (Status == STATUS_BUFFER_TOO_SMALL) {
            free(List);
            Size *= 2;
            List = malloc(Size);
            if (List == NULL) {
                DbgOut("Error: Failed to allocate %d bytes for module list.\n",
                       Size);

                return FALSE;
            }

            continue;
        }

        //
        // Some other error occurred.
        //

        DbgOut("Error: Unable to get module list for process %x. Status %x\n",
               DbgTargetProcessId,
               Status);

        free(List);
        return FALSE;
    }

    //
    // Execution should never get here.
    //

    return FALSE;
}

VOID
DbgpUserRequestBreakIn (
    VOID
    )

/*++

Routine Description:

    This routine attempts to stop the running target.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

ULONG
DbgpUserGetSignalToDeliver (
    ULONG SignalNumber
    )

/*++

Routine Description:

    This routine returns the value for the "signal to deliver" parameters when
    letting the target continue. For user mode processes, breaks into the
    debugger occur because of signal delivery, and the debugger has the choice
    of whether or not to actually deliver a signal.

Arguments:

    SignalNumber - Supplies the last signal caught by the debugger.

Return Value:

    Returns the signal to deliver for the upcoming target continuation.

    0 if no signal should be delivered to the target.

--*/

{

    //
    // Never deliver traps or keyboard interrupts.
    //

    if ((SignalNumber == SIGNAL_TRAP) ||
        (SignalNumber == SIGNAL_KEYBOARD_INTERRUPT)) {

        return 0;
    }

    //
    // Otherwise, deliver the signal.
    //

    return SignalNumber;
}

HANDLE
CreateDebuggerLock (
    VOID
    )

/*++

Routine Description:

    This routine creates a debugger lock.

Arguments:

    None.

Return Value:

    Returns a handle to a debugger lock on success, or NULL on failure.

--*/

{

    pthread_mutex_t *Lock;

    Lock = malloc(sizeof(pthread_mutex_t));
    if (Lock == NULL) {
        return NULL;
    }

    pthread_mutex_init(Lock, NULL);
    return (HANDLE)Lock;
}

VOID
AcquireDebuggerLock (
    HANDLE Lock
    )

/*++

Routine Description:

    This routine acquires a debugger lock. This routine does not return until
    the lock is required.

Arguments:

    Lock - Supplies a handle to the lock that is to be acquired.

Return Value:

    None.

--*/

{

    pthread_mutex_t *Mutex;

    Mutex = (pthread_mutex_t *)Lock;
    pthread_mutex_lock(Mutex);
    return;
}

VOID
ReleaseDebuggerLock (
    HANDLE Lock
    )

/*++

Routine Description:

    This routine releases a debugger lock.

Arguments:

    Lock - Supplies a handle to the lock that is to be released.

Return Value:

    None.

--*/

{

    pthread_mutex_t *Mutex;

    Mutex = (pthread_mutex_t *)Lock;
    pthread_mutex_unlock(Mutex);
    return;
}

VOID
DestroyDebuggerLock (
    HANDLE Lock
    )

/*++

Routine Description:

    This routine destroys a debugger lock.

Arguments:

    Lock - Supplies a handle to the lock that is to be destroyed.

Return Value:

    None.

--*/

{

    pthread_mutex_t *Mutex;

    Mutex = (pthread_mutex_t *)Lock;
    pthread_mutex_destroy(Mutex);
    free(Mutex);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

