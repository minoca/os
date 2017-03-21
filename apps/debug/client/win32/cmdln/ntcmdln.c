/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntcmdln.c

Abstract:

    This module implements the command line debugger on Windows platforms.

Author:

    Evan Green 8-May-2013

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <windows.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif

#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "dbgrprof.h"
#include "console.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
WINAPI
ConsoleControlHandler (
    DWORD ControlType
    );

PVOID
DbgrpWin32InputThread (
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

int DbgStandardInPipe[2] = {-1, -1};

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

    BOOL Result;

    //
    // Set the Control+C handler.
    //

    Result = SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);
    if (Result == FALSE) {
        DbgOut("Failed to set console control handler. Ctrl+C breakins will "
               "be unavailable.\n");
    }

    Result = DbgrOsCreatePipe(DbgStandardInPipe);
    if (Result != 0) {
        return FALSE;
    }

    Result = DbgrOsCreateThread(DbgrpWin32InputThread, NULL);
    if (Result != 0) {
        close(DbgStandardInPipe[1]);
        DbgStandardInPipe[1] = -1;
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

    //
    // Close the read side. The write side is closed by the input thread.
    //

    if (DbgStandardInPipe[0] != -1) {
        close(DbgStandardInPipe[0]);
        DbgStandardInPipe[0] = -1;
    }

    return;
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

    ssize_t BytesRead;
    char Character;
    UCHAR ControlKeyValue;

    ControlKeyValue = 0;
    BytesRead = read(DbgStandardInPipe[0], &Character, 1);
    if (BytesRead != 1) {
        return FALSE;
    }

    //
    // If it's a magic escape byte, read another byte to see if it's a literal
    // escape or just a poke from the remote threads.
    //

    if ((UCHAR)Character == 0xFF) {
        BytesRead = read(DbgStandardInPipe[0], &Character, 1);
        if (BytesRead != 1) {
            return FALSE;
        }

        if ((UCHAR)Character != 0xFF) {
            Character = 0;
            ControlKeyValue = KEY_REMOTE;
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

    unsigned char Message[2];

    //
    // Write the escaped "remote" sequence into the input pipe funnel.
    //

    Message[0] = 0xFF;
    Message[1] = 0x00;
    write(DbgStandardInPipe[1], Message, 2);
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

//
// --------------------------------------------------------- Internal Functions
//

BOOL
WINAPI
ConsoleControlHandler (
    DWORD ControlType
    )

/*++

Routine Description:

    This routine is called when a console control event comes in, such as
    Control+C, Control+Break, console close, logoff, etc. It responds only to
    Control+C, which requests a break in.

Arguments:

    ControlType - Supplies the control type. See CTRL_*_EVENT definitions.

Return Value:

    TRUE if this function handled the control event.

    FALSE if the next handler in the list should be called.

--*/

{

    if (ControlType == CTRL_C_EVENT) {
        DbgrRequestBreakIn();
        return TRUE;
    }

    return FALSE;
}

PVOID
DbgrpWin32InputThread (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is the entry point for the thread that shuttles standard input
    to a combined pipe. This is used because Windows can't select on a pipe and
    standard in.

Arguments:

    Parameter - Supplies a pointer supplied by the creator of the thread. In
        this case it is not used.

Return Value:

    NULL always.

--*/

{

    ssize_t BytesWritten;
    int Character;
    unsigned char Characters[2];

    while (DbgStandardInPipe[0] != -1) {
        Character = fgetc(stdin);
        if (Character == -1) {
            if ((errno == 0) || (errno == EINTR)) {
                continue;
            }

            break;
        }

        //
        // If it's the magic remote byte, send it twice to escape it.
        //

        if (Character == 0xFF) {
            Characters[0] = Character;
            Characters[1] = Character;
            BytesWritten = write(DbgStandardInPipe[1], Characters, 2);
            if (BytesWritten != 2) {
                break;
            }

        } else {
            BytesWritten = write(DbgStandardInPipe[1], &Character, 1);
            if (BytesWritten != 1) {
                break;
            }
        }
    }

    if (DbgStandardInPipe[1] != -1) {
        close(DbgStandardInPipe[1]);
    }

    return NULL;
}

