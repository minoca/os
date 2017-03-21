/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    console.h

Abstract:

    This header contains definitions for the generic console functions required
    by the debugger.

Author:

    Evan Green 2-Jul-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define KEY_RETURN     0x10
#define KEY_UP         0x01
#define KEY_DOWN       0x02
#define KEY_ESCAPE     0x03
#define KEY_REMOTE     0x04

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
PVOID
(*PDBGR_THREAD_ROUTINE) (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine is the entry point prototype for a new thread.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread.

Return Value:

    Returns a pointer value.

--*/

//
// -------------------------------------------------------- Function Prototypes
//

//
// Functions callable by the OS support layer.
//

INT
DbgrMain (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine is the main entry point for the debugger.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of strings representing the arguments.

Return Value:

    Returns 0 on success, nonzero on failure.

--*/

VOID
DbgrRequestBreakIn (
    VOID
    );

/*++

Routine Description:

    This routine sends a break-in request to the target.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
DbgrDisplayCommandLineProfilerData (
    PROFILER_DATA_TYPE DataType,
    PROFILER_DISPLAY_REQUEST DisplayRequest,
    ULONG Threshold
    );

/*++

Routine Description:

    This routine displays the profiler data collected by the core debugging
    infrastructure to standard out.

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

BOOL
DbgrGetProfilerStackData (
    PSTACK_DATA_ENTRY *StackTreeRoot
    );

/*++

Routine Description:

    This routine processes and returns any pending profiling stack data. It
    will add it to the provided stack tree root. The caller is responsible for
    destroying the tree.

Arguments:

    StackTreeRoot - Supplies a pointer to a pointer to the root of the stack
        data tree. Upon return from the routine it will be updated with all the
        newly parsed data. If the root is null, a new root will be allocated.

Return Value:

    Returns TRUE when data is successfully returned, or FALSE on failure.

--*/

VOID
DbgrDestroyProfilerStackData (
    PSTACK_DATA_ENTRY Root
    );

/*++

Routine Description:

    This routine destroys a profiler stack data tree.

Arguments:

    Root - Supplies a pointer to the root element of the tree.

Return Value:

    None.

--*/

VOID
DbgrPrintProfilerStackData (
    PSTACK_DATA_ENTRY Root,
    ULONG Threshold
    );

/*++

Routine Description:

    This routine prints profiler stack data to standard out.

Arguments:

    Root - Supplies a pointer to the root of the profiler stack data tree.

    Threshold - Supplies the minimum percentage a stack entry hit must be in
        order to be displayed.

Return Value:

    None.

--*/

VOID
DbgrProfilerStackEntrySelected (
    PSTACK_DATA_ENTRY Root
    );

/*++

Routine Description:

    This routine is called when a profiler stack data entry is selected by the
    user.

Arguments:

    Root - Supplies a pointer to the root of the profiler stack data tree.

Return Value:

    None.

--*/

BOOL
DbgrGetProfilerMemoryData (
    PLIST_ENTRY *MemoryPoolListHead
    );

/*++

Routine Description:

    This routine processes and returns any pending profiling memory data.

Arguments:

    MemoryPoolListHead - Supplies a pointer to head of the memory pool list
        that is to be populated with the most up to date pool data.

Return Value:

    Returns TRUE when data is successfully returned, or FALSE on failure.

--*/

VOID
DbgrDestroyProfilerMemoryData (
    PLIST_ENTRY PoolListHead
    );

/*++

Routine Description:

    This routine destroys a profiler memory data list.

Arguments:

    PoolListHead - Supplies a pointer to the head of the memory pool list
        that is to be destroyed.

Return Value:

    None.

--*/

VOID
DbgrPrintProfilerMemoryData (
    PLIST_ENTRY MemoryPoolListHead,
    BOOL DeltaMode,
    ULONG ActiveCountThreshold
    );

/*++

Routine Description:

    This routine prints the statistics from the given memory pool list to the
    debugger console.

Arguments:

    MemoryPoolListHead - Supplies a pointer to the head of the memory pool
        list.

    DeltaMode - Supplies a boolean indicating whether or not the memory pool
        list represents a delta from a previous point in time.

    ActiveCountThreshold - Supplies the active count threshold. No statistics
        will be displayed for tags with an active count less than this
        threshold.

Return Value:

    None.

--*/

PLIST_ENTRY
DbgrSubtractMemoryStatistics (
    PLIST_ENTRY CurrentListHead,
    PLIST_ENTRY BaseListHead
    );

/*++

Routine Description:

    This routine subtracts the given current memory list from the base memory
    list, returning a list that contains the deltas for memory pool statistics.
    If this routine ever fails, it just returns the current list.

Arguments:

    CurrentListHead - Supplies a pointer to the head of the current list of
        memory pool data.

    BaseListHead - supplies a pointer to the head of the base line memory list
        from which the deltas are created.

Return Value:

    Returns a new memory pool list if possible. If the routine fails or there
    is no base line, then the current memory list is returned.

--*/

//
// Functions implemented by the OS support layer.
//

BOOL
DbgrOsInitializeConsole (
    PBOOL EchoCommands
    );

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

VOID
DbgrOsDestroyConsole (
    VOID
    );

/*++

Routine Description:

    This routine cleans up anything related to console functionality as a
    debugger is exiting.

Arguments:

    None.

Return Value:

    None.

--*/

INT
DbgrOsCreateThread (
    PDBGR_THREAD_ROUTINE ThreadRoutine,
    PVOID Parameter
    );

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

int
DbgrOsCreatePipe (
    int FileDescriptors[2]
    );

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

char *
DbgrOsGetUserName (
    void
    );

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

char *
DbgrOsGetHostName (
    void
    );

/*++

Routine Description:

    This routine returns the host name of the current machine.

Arguments:

    None.

Return Value:

    Returns a pointer to a string containing the user name if it can be found.
    The caller is responsible for freeing this memory.

--*/

VOID
DbgrOsPrepareToReadInput (
    VOID
    );

/*++

Routine Description:

    This routine is called before the debugger begins to read a line of input
    from the user.

Arguments:

    None.

Return Value:

    None.

--*/

BOOL
DbgrOsGetCharacter (
    PUCHAR Key,
    PUCHAR ControlKey
    );

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

VOID
DbgrOsRemoteInputAdded (
    VOID
    );

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

VOID
DbgrOsPostInputCallback (
    VOID
    );

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

BOOL
InitializeCommunications (
    PSTR Channel,
    ULONG Baudrate
    );

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

VOID
DestroyCommunications (
    VOID
    );

/*++

Routine Description:

    This routine tears down the debug communication channel.

Arguments:

    None.

Return Value:

    None.

--*/

BOOL
CommReceive (
    PVOID Buffer,
    ULONG BytesToRead
    );

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

BOOL
CommSend (
    PVOID Buffer,
    ULONG BytesToSend
    );

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

BOOL
CommReceiveBytesReady (
    VOID
    );

/*++

Routine Description:

    This routine determines whether or not bytes can be read from the
    debugger connection.

Arguments:

    None.

Return Value:

    TRUE if there are bytes ready to be read.

    FALSE if no bytes are ready at this time.

--*/

VOID
CommStall (
    ULONG Milliseconds
    );

/*++

Routine Description:

    This routine pauses for the given amount of time.

Arguments:

    Milliseconds - Supplies the amount of time, in milliseconds, to stall the
        current thread for.

Return Value:

    None.

--*/

BOOL
UiLoadSourceFile (
    PSTR Path,
    PVOID Contents,
    ULONGLONG Size
    );

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

BOOL
UiHighlightExecutingLine (
    LONG LineNumber,
    BOOL Enable
    );

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

VOID
UiEnableCommands (
    BOOL Enable
    );

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

VOID
UiSetCommandText (
    PSTR Text
    );

/*++

Routine Description:

    This routine sets the text inside the command edit box.

Arguments:

    Text - Supplies a pointer to a null terminated string to set the command
        text to.

Return Value:

    None.

--*/

VOID
UiSetPromptText (
    PSTR Text
    );

/*++

Routine Description:

    This routine sets the text inside the prompt edit box.

Arguments:

    Text - Supplies a pointer to a null terminated string to set the prompt
        text to.

Return Value:

    None.

--*/

VOID
UiDisplayProfilerData (
    PROFILER_DATA_TYPE DataType,
    PROFILER_DISPLAY_REQUEST DisplayRequest,
    ULONG Threshold
    );

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

HANDLE
CreateDebuggerLock (
    VOID
    );

/*++

Routine Description:

    This routine creates a debugger lock.

Arguments:

    None.

Return Value:

    Returns a handle to a debugger lock on success, or NULL on failure.

--*/

VOID
AcquireDebuggerLock (
    HANDLE Lock
    );

/*++

Routine Description:

    This routine acquires a debugger lock. This routine does not return until
    the lock is required.

Arguments:

    Lock - Supplies a handle to the lock that is to be acquired.

Return Value:

    None.

--*/

VOID
ReleaseDebuggerLock (
    HANDLE Lock
    );

/*++

Routine Description:

    This routine releases a debugger lock.

Arguments:

    Lock - Supplies a handle to the lock that is to be released.

Return Value:

    None.

--*/

VOID
DestroyDebuggerLock (
    HANDLE Lock
    );

/*++

Routine Description:

    This routine destroys a debugger lock.

Arguments:

    Lock - Supplies a handle to the lock that is to be destroyed.

Return Value:

    None.

--*/

