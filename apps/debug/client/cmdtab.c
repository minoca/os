/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cmdtab.c

Abstract:

    This module defines the command table for the debugger.

Author:

    Evan Green 11-Dec-2013

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
#include "dbgapi.h"
#include "symbols.h"
#include "dbgrprof.h"
#include "dbgrcomm.h"
#include "extsp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define DBG_DISPATCH_EXCEPTION_DESCRIPTION "Runs a debugger extension command."
#define DBG_SWITCH_PROCESSOR_DESCRIPTION \
    "Switch to another processor (kernel mode) or thread (user mode)."

#define DBGR_QUIT_DESCRIPTION "Exits the local debugger."
#define DBGR_GET_SET_REGISTERS_DESCRIPTION "Get and set registers."
#define DBGR_GET_SPECIAL_REGISTERS_DESCRIPTION "Get and set special registers."
#define DBGR_GO_DESCRIPTION "Continue execution (go)."
#define DBGR_RETURN_TO_CALLER_DESCRIPTION \
    "Continue to calling function (go up)."

#define DBGR_STEP_INTO_DESCRIPTION "Step in."
#define DBGR_STEP_OVER_DESCRIPTION "Step over."
#define DBGR_SET_SOURCE_STEPPING_DESCRIPTION \
    "Enable or disable source line level stepping."

#define DBGR_SOURCE_LINE_PRINTING_DESCRIPTION \
    "Enable or disable printing of file and line numbers with each address."

#define DBGR_SOURCE_AT_ADDRESS_DESCRIPTION \
    "Display the source file and line number for the given address."

#define DBGR_DISASSEMBLE_DESCRIPTION \
    "Disassemble instructions at the current or specified address."

#define DBGR_PRINT_CALL_STACK_DESCRIPTION "Print the current call stack."
#define DBGR_PRINT_CALL_STACK_NUMBERED_DESCRIPTION \
    "Print the current call stack with frame numbers."

#define DBGR_SEARCH_SYMBOLS_DESCRIPTION "Search for a symbol by name."
#define DBGR_DUMP_TYPE_DESCRIPTION "Dump information about a type."
#define DBGR_DUMP_LIST_DESCRIPTION "Dump a doubly-linked list."
#define DBGR_EVALUATE_DESCRIPTION "Evaluate a numeric or symbolic expression."
#define DBGR_PRINT_LOCALS_DESCRIPTION "Dump local variables."
#define DBGR_DUMP_BYTES_DESCRIPTION "Dump bytes (8-bit) from memory."
#define DBGR_DUMP_CHARACTERS_DESCRIPTION "Dump characters (8-bit) from memory."
#define DBGR_DUMP_WORDS_DESCRIPTION "Dump words (16-bit) from memory."
#define DBGR_DUMP_DWORDS_DESCRIPTION "Dump double words (32-bit) from memory."
#define DBGR_DUMP_QWORDS_DESCRIPTION "Dump quad words (64-bit) from memory."
#define DBGR_EDIT_BYTES_DESCRIPTION "Edit bytes (8-bit) in memory."
#define DBGR_EDIT_WORDS_DESCRIPTION "Edit words (16-bit) in memory."
#define DBGR_EDIT_DWORDS_DESCRIPTION "Edit double-words (32-bit) in memory."
#define DBGR_EDIT_QWORDS_DESCRIPTION "Edit quad-words (64-bit) in memory."
#define DBGR_SET_FRAME_DESCRIPTION "Set the current call stack frame."
#define DBGR_LIST_BREAKPOINTS_DESCRIPTION "List all breakpoints."
#define DBGR_ENABLE_BREAKPOINT_DESCRIPTION "Enable a breakpoint by number."
#define DBGR_DISABLE_BREAKPOINT_DESCRIPTION "Disable a breakpoint by number."
#define DBGR_CREATE_BREAK_POINT_DESCRIPTION "Create a breakpoint."
#define DBGR_DELETE_BREAK_POINT_DESCRIPTION \
    "Clear (delete) a breakpoint by number."

#define DBGR_SET_SYMBOL_PATH_DESCRIPTION "Get or set the symbol search path."
#define DBGR_APPEND_SYMBOL_PATH_DESCRIPTION \
    "Append a path to the symbol search path."

#define DBGR_SET_SOURCE_PATH_DESCRIPTION "Get or set the source search path."
#define DBGR_APPEND_SOURCE_PATH_DESCRIPTION \
    "Append a path to the source search path."

#define DBGR_RELOAD_SYMBOLS_DESCRIPTION \
    "Reload all symbols from the symbol search path."

#define DBGR_LOAD_EXTENSION_DESCRIPTION "Load a debugger extension."
#define DBGR_UNLOAD_EXTENSION_DESCRIPTION \
    "Unload a debugger extension (use * to unload all)."

#define DBGR_PRINT_PROCESSOR_BLOCK_DESCRIPTION \
    "Dump the current processor block (kernel mode)."

#define DBGR_DUMP_POINTER_SYMBOLS_DESCRIPTION \
    "Dump any addresses found for memory at the given location."

#define DBGR_PROFILE_DESCRIPTION \
    "Profiler commands."

#define DBGR_REBOOT_DESCRIPTION "Forcefully reboot the target machine."
#define DBGR_HELP_DESCRIPTION "Show this help text."
#define DBGR_SERVER_DESCRIPTION \
    "Start a remote server so that others can connect to this session."

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DbgrHelpCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the command table, which is terminated with an entry of NULLs. The
// first entry is always the extension command entry, and the second entry must
// be the switch processor command.
//

DEBUGGER_COMMAND_ENTRY DbgrCommandTable[] = {
    {"!", DbgDispatchExtension, DBG_DISPATCH_EXCEPTION_DESCRIPTION},
    {"~", DbgrSwitchProcessor, DBG_SWITCH_PROCESSOR_DESCRIPTION},
    {"q", DbgrQuit, DBGR_QUIT_DESCRIPTION},
    {"r", DbgrGetSetRegisters, DBGR_GET_SET_REGISTERS_DESCRIPTION},
    {"rs", DbgrGetSetSpecialRegisters, DBGR_GET_SPECIAL_REGISTERS_DESCRIPTION},
    {"g", DbgrGo, DBGR_GO_DESCRIPTION},
    {"gu", DbgrReturnToCaller, DBGR_RETURN_TO_CALLER_DESCRIPTION},
    {"t", DbgrStep, DBGR_STEP_INTO_DESCRIPTION},
    {"p", DbgrStep, DBGR_STEP_OVER_DESCRIPTION},
    {"ss", DbgrSetSourceStepping, DBGR_SET_SOURCE_STEPPING_DESCRIPTION},
    {"sl", DbgrSetSourceLinePrinting, DBGR_SOURCE_LINE_PRINTING_DESCRIPTION},
    {"so", DbgrShowSourceAtAddressCommand, DBGR_SOURCE_AT_ADDRESS_DESCRIPTION},
    {"u", DbgrDisassemble, DBGR_DISASSEMBLE_DESCRIPTION},
    {"k", DbgrPrintCallStack, DBGR_PRINT_CALL_STACK_DESCRIPTION},
    {"kn", DbgrPrintCallStack, DBGR_PRINT_CALL_STACK_NUMBERED_DESCRIPTION},
    {"x", DbgrSearchSymbols, DBGR_SEARCH_SYMBOLS_DESCRIPTION},
    {"dt", DbgrDumpTypeCommand, DBGR_DUMP_TYPE_DESCRIPTION},
    {"dl", DbgrDumpList, DBGR_DUMP_LIST_DESCRIPTION},
    {"?", DbgrEvaluate, DBGR_EVALUATE_DESCRIPTION},
    {"dv", DbgrPrintLocals, DBGR_PRINT_LOCALS_DESCRIPTION},
    {"db", DbgrDumpMemory, DBGR_DUMP_BYTES_DESCRIPTION},
    {"dc", DbgrDumpMemory, DBGR_DUMP_CHARACTERS_DESCRIPTION},
    {"dw", DbgrDumpMemory, DBGR_DUMP_WORDS_DESCRIPTION},
    {"dd", DbgrDumpMemory, DBGR_DUMP_DWORDS_DESCRIPTION},
    {"dq", DbgrDumpMemory, DBGR_DUMP_QWORDS_DESCRIPTION},
    {"eb", DbgrEditMemory, DBGR_EDIT_BYTES_DESCRIPTION},
    {"ew", DbgrEditMemory, DBGR_EDIT_WORDS_DESCRIPTION},
    {"ed", DbgrEditMemory, DBGR_EDIT_DWORDS_DESCRIPTION},
    {"eq", DbgrEditMemory, DBGR_EDIT_QWORDS_DESCRIPTION},
    {"frame", DbgrSetFrame, DBGR_SET_FRAME_DESCRIPTION},
    {"bl", DbgrListBreakPoints, DBGR_LIST_BREAKPOINTS_DESCRIPTION},
    {"be", DbgrEnableBreakPoint, DBGR_ENABLE_BREAKPOINT_DESCRIPTION},
    {"bd", DbgrEnableBreakPoint, DBGR_DISABLE_BREAKPOINT_DESCRIPTION},
    {"bp", DbgrCreateBreakPoint, DBGR_CREATE_BREAK_POINT_DESCRIPTION},
    {"bc", DbgrDeleteBreakPoint, DBGR_DELETE_BREAK_POINT_DESCRIPTION},
    {"sympath", DbgrSetSymbolPathCommand, DBGR_SET_SYMBOL_PATH_DESCRIPTION},
    {"sympath+", DbgrSetSymbolPathCommand, DBGR_APPEND_SYMBOL_PATH_DESCRIPTION},
    {"srcpath", DbgrSetSourcePathCommand, DBGR_SET_SOURCE_PATH_DESCRIPTION},
    {"srcpath+", DbgrSetSourcePathCommand, DBGR_APPEND_SOURCE_PATH_DESCRIPTION},
    {"reload", DbgrReloadSymbols, DBGR_RELOAD_SYMBOLS_DESCRIPTION},
    {"load", DbgrLoadExtension, DBGR_LOAD_EXTENSION_DESCRIPTION},
    {"unload", DbgrLoadExtension, DBGR_UNLOAD_EXTENSION_DESCRIPTION},
    {"proc", DbgrPrintProcessorBlock, DBGR_PRINT_PROCESSOR_BLOCK_DESCRIPTION},
    {"dps", DbgrDumpPointerSymbols, DBGR_DUMP_POINTER_SYMBOLS_DESCRIPTION},
    {"profile", DbgrProfileCommand, DBGR_PROFILE_DESCRIPTION},
    {"reboot", DbgrRebootCommand, DBGR_REBOOT_DESCRIPTION},
    {"help", DbgrHelpCommand, DBGR_HELP_DESCRIPTION},
    {"server", DbgrServerCommand, DBGR_SERVER_DESCRIPTION},
    {NULL, NULL, NULL}
};

//
// ------------------------------------------------------------------ Functions
//

PDEBUGGER_COMMAND_ENTRY
DbgrLookupCommand (
    PSTR Command
    )

/*++

Routine Description:

    This routine attempts to find a debugger command entry.

Arguments:

    Command - Supplies a pointer to the null-terminated string containing the
        name of the command that was invoked. This command is split on the
        period character, and the first segment is looked up.

Return Value:

    Returns a pointer to the command entry on success.

    NULL if there no such command, or on failure.

--*/

{

    PSTR CommandCopy;
    ULONG CommandIndex;
    PDEBUGGER_COMMAND_ENTRY Entry;
    PSTR Period;

    CommandCopy = NULL;
    Period = strchr(Command, '.');
    if (Period != NULL) {
        CommandCopy = strdup(Command);
        if (CommandCopy == NULL) {
            return NULL;
        }

        Period = strchr(CommandCopy, '.');
        *Period = '\0';
        Command = CommandCopy;
    }

    //
    // The extension command is special as it's not delimited by a period.
    //

    if (*Command == '!') {
        Entry = &(DbgrCommandTable[0]);
        goto LookupCommandEnd;

    //
    // The switch processor command is also special as its format is ~N, where
    // N is a number argument.
    //

    } else if (*Command == '~') {
        Entry = &(DbgrCommandTable[1]);
        goto LookupCommandEnd;
    }

    //
    // Loop looking for a command that matches. Case is not important.
    //

    CommandIndex = 0;
    while (DbgrCommandTable[CommandIndex].Command != NULL) {
        Entry = &(DbgrCommandTable[CommandIndex]);
        if (strcasecmp(Command, Entry->Command) == 0) {
            goto LookupCommandEnd;
        }

        CommandIndex += 1;
    }

    Entry = NULL;

LookupCommandEnd:
    if (CommandCopy != NULL) {
        free(CommandCopy);
    }

    return Entry;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DbgrHelpCommand (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    )

/*++

Routine Description:

    This routine prints a description of all available commands.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    ULONG CommandIndex;
    PDEBUGGER_COMMAND_ENTRY Entry;

    CommandIndex = 0;
    while (DbgrCommandTable[CommandIndex].Command != NULL) {
        Entry = &(DbgrCommandTable[CommandIndex]);
        DbgOut("%s -- %s\n", Entry->Command, Entry->HelpText);
        CommandIndex += 1;
    }

    return 0;
}

