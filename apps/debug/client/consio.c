/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    consio.c

Abstract:

    This module implements standard input and output functionality for the
    debugger.

Author:

    Evan Green 30-Dec-2013

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/debug/spproto.h>
#include <minoca/lib/im.h>
#include <minoca/debug/dbgext.h>
#include "disasm.h"
#include "dbgapi.h"
#include "dbgrprof.h"
#include "console.h"
#include "symbols.h"
#include "dbgrcomm.h"
#include "dbgsym.h"
#include "extsp.h"
#include "consio.h"
#include "remsrv.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define DBGR_IO_BUFFER_SIZE 1024

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
DbgrpFormatWriteCharacter (
    INT Character,
    PPRINT_FORMAT_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the global context. Currently only one debugger context
// is supported for output functions.
//

PDEBUGGER_CONTEXT DbgConsoleContext;

//
// ------------------------------------------------------------------ Functions
//

INT
DbgrInitializeConsoleIo (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes console I/O for the debugger.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    BOOL EchoCommands;
    INT Result;

    assert(DbgConsoleContext == NULL);

    DbgConsoleContext = Context;
    EchoCommands = FALSE;
    Result = DbgrOsInitializeConsole(&EchoCommands);
    if (Result == FALSE) {
        Result = EINVAL;
        goto InitializeConsoleIoEnd;
    }

    if (EchoCommands != FALSE) {
        Context->Flags |= DEBUGGER_FLAG_ECHO_COMMANDS;
    }

    assert(Context->StandardOut.ConsoleBuffer == NULL);

    Context->StandardOut.ConsoleBuffer = malloc(DBGR_IO_BUFFER_SIZE);
    if (Context->StandardOut.ConsoleBuffer == NULL) {
        Result = ENOMEM;
        goto InitializeConsoleIoEnd;
    }

    Context->StandardOut.ConsoleBufferCapacity = DBGR_IO_BUFFER_SIZE;
    Context->StandardOut.ConsoleBufferSize = 0;
    Context->StandardOut.Lock = CreateDebuggerLock();
    if (Context->StandardOut.Lock == NULL) {
        Result = ENOMEM;
        goto InitializeConsoleIoEnd;
    }

    Context->StandardIn.Lock = CreateDebuggerLock();
    if (Context->StandardIn.Lock == NULL) {
        Result = ENOMEM;
        goto InitializeConsoleIoEnd;
    }

    Result = 0;

InitializeConsoleIoEnd:
    return Result;
}

VOID
DbgrDestroyConsoleIo (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys console I/O for the debugger.

Arguments:

    Context - Supplies a pointer to the debugger context.

Return Value:

    None.

--*/

{

    DbgrOsDestroyConsole();
    if (Context->StandardOut.ConsoleBuffer != NULL) {
        free(Context->StandardOut.ConsoleBuffer);
    }

    if (Context->StandardOut.Prompt != NULL) {
        free(Context->StandardOut.Prompt);
    }

    if (Context->StandardOut.Lock != NULL) {
        DestroyDebuggerLock(Context->StandardOut.Lock);
    }

    if (Context->StandardIn.Lock != NULL) {
        DestroyDebuggerLock(Context->StandardIn.Lock);
    }

    DbgConsoleContext = NULL;
    return;
}

INT
DbgOut (
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine prints a formatted string to the debugger console.

Arguments:

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

{

    va_list Arguments;
    int Result;

    assert(DbgConsoleContext != NULL);

    va_start(Arguments, Format);
    Result = DbgOutVaList(DbgConsoleContext, Format, Arguments);
    va_end(Arguments);
    return Result;
}

INT
DbgOutVaList (
    PDEBUGGER_CONTEXT Context,
    const char *Format,
    va_list Arguments
    )

/*++

Routine Description:

    This routine prints a formatted string to the given debugger console.

Arguments:

    Context - Supplies a pointer to the debugger context to output to.

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of bytes successfully converted. A null terminator is
    not written.

    Returns a negative number if an error was encountered.

--*/

{

    PRINT_FORMAT_CONTEXT PrintContext;

    if (Context == NULL) {
        Context = DbgConsoleContext;
    }

    memset(&PrintContext, 0, sizeof(PRINT_FORMAT_CONTEXT));
    PrintContext.Context = Context;
    PrintContext.WriteCharacter = DbgrpFormatWriteCharacter;
    RtlInitializeMultibyteState(&(PrintContext.State),
                                CharacterEncodingDefault);

    AcquireDebuggerLock(Context->StandardOut.Lock);
    RtlFormat(&PrintContext, (PSTR)Format, Arguments);

    //
    // If something was written poke all the clients to send the data along to
    // them too.
    //

    if (PrintContext.CharactersWritten != 0) {
        DbgrpServerNotifyClients(Context);
    }

    ReleaseDebuggerLock(Context->StandardOut.Lock);
    return PrintContext.CharactersWritten;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
DbgrpFormatWriteCharacter (
    INT Character,
    PPRINT_FORMAT_CONTEXT Context
    )

/*++

Routine Description:

    This routine writes a character to the output during a printf-style
    formatting operation.

Arguments:

    Character - Supplies the character to be written.

    Context - Supplies a pointer to the printf-context.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PDEBUGGER_CONTEXT DebuggerContext;
    PVOID NewBuffer;
    ULONGLONG NewCapacity;
    PDEBUGGER_STANDARD_OUT Out;

    DebuggerContext = Context->Context;
    Out = &(DebuggerContext->StandardOut);

    //
    // Reallocate the console buffer if needed.
    //

    if (Out->ConsoleBufferSize + 2 > Out->ConsoleBufferCapacity) {
        NewCapacity = Out->ConsoleBufferCapacity * 2;

        assert(NewCapacity > Out->ConsoleBufferSize + 2);

        NewBuffer = realloc(Out->ConsoleBuffer, NewCapacity);
        if (NewBuffer == NULL) {
            return FALSE;
        }

        Out->ConsoleBuffer = NewBuffer;
        Out->ConsoleBufferCapacity = NewCapacity;
    }

    //
    // Add the character to the console buffer.
    //

    Out->ConsoleBuffer[Out->ConsoleBufferSize] = Character;
    Out->ConsoleBufferSize += 1;
    if (fputc(Character, stdout) == -1) {
        return FALSE;
    }

    return TRUE;
}

