/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    builtin.c

Abstract:

    This module implements support for the builtin shell utilities.

Author:

    Evan Green 20-Aug-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include "../swiss.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../swlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns non-zero if the given node type is a for, while, or until
// loop.
//

#define SHELL_LOOP_NODE(_NodeType)                                       \
    (((_NodeType) == ShellNodeFor) || ((_NodeType) == ShellNodeWhile) || \
     ((_NodeType) == ShellNodeUntil))

//
// ---------------------------------------------------------------- Definitions
//

#define SHELL_READ_INITIAL_STRING_SIZE 32

#define SHELL_MAX_OPTION_INDEX_LENGTH 12
#define RUBOUT_CHARACTER 0x7F

//
// Define the default builtin path used by the command builtin.
//

#define SHELL_COMMAND_BUILTIN_PATH "/bin:/usr/bin:/usr/local/bin"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ShBuiltinBreak (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinContinue (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinReturn (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinExit (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinBreakOrContinue (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments,
    BOOL Break
    );

INT
ShBuiltinReturnOrExit (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments,
    BOOL Exit
    );

INT
ShBuiltinNop (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinFalse (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinDot (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinExec (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinRead (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinShift (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinTimes (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinUmask (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinGetopts (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinCommand (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinType (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

INT
ShBuiltinTypeOrCommand (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments,
    BOOL IsType
    );

INT
ShClassifyCommand (
    PSHELL Shell,
    PSTR Command,
    BOOL Verbose
    );

INT
ShGetNextOption (
    PSHELL Shell,
    ULONG ArgumentCount,
    PSTR *Arguments,
    PINT ArgumentIndex,
    PSTR Options,
    PCHAR Option,
    PSTR *OptionArgument,
    PBOOL EndOfOptions
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Stores the expected index for the next call to the built-in option
// processing.
//

INT ShOptionsIndex;

//
// Stores the string index of the next option character to process.
//

ULONG ShNextOptionCharacter;

//
// Stores whether or not a "--" end of arguments marker has been seen by the
// built-in get options command.
//

BOOL ShSeenDoubleDash;

//
// Define the shell reserved keywords.
//

PSTR ShReservedWords[] = {
    "if",
    "then",
    "else",
    "elif",
    "fi",
    "do",
    "done",
    "case",
    "esac",
    "while",
    "until",
    "for",
    "{",
    "}",
    "!",
    "in",
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

PSHELL_BUILTIN_COMMAND
ShIsBuiltinCommand (
    PSTR Command
    )

/*++

Routine Description:

    This routine determines if the given command name is a built in command,
    and returns a pointer to the command function if it is.

Arguments:

    Command - Supplies the null terminated string of the command.

Return Value:

    Returns a pointer to the command entry point function if the given string
    is a built-in command.

    NULL if the command is not a built-in command.

--*/

{

    PSHELL_BUILTIN_COMMAND EntryPoint;

    EntryPoint = NULL;
    switch (*Command) {
    case ':':
        if (*(Command + 1) == '\0') {
            EntryPoint = ShBuiltinNop;
        }

        break;

    case 'a':
        if (strcmp(Command + 1, "lias") == 0) {
            EntryPoint = ShBuiltinAlias;
        }

        break;

    case 'b':
        if (strcmp(Command + 1, "reak") == 0) {
            EntryPoint = ShBuiltinBreak;
        }

        break;

    case 'c':
        if (strcmp(Command + 1, "d") == 0) {
            EntryPoint = ShBuiltinCd;

        } else if (strcmp(Command + 1, "ommand") == 0) {
            EntryPoint = ShBuiltinCommand;

        } else if (strcmp(Command + 1, "ontinue") == 0) {
            EntryPoint = ShBuiltinContinue;
        }

        break;

    case '.':
        if (*(Command + 1) == '\0') {
            EntryPoint = ShBuiltinDot;
        }

        break;

    case 'e':
        if (strcmp(Command + 1, "val") == 0) {
            EntryPoint = ShBuiltinEval;

        } else if (strcmp(Command + 1, "xec") == 0) {
            EntryPoint = ShBuiltinExec;

        } else if (strcmp(Command + 1, "xit") == 0) {
            EntryPoint = ShBuiltinExit;

        } else if (strcmp(Command + 1, "xport") == 0) {
            EntryPoint = ShBuiltinExport;
        }

        break;

    case 'f':
        if (strcmp(Command + 1, "alse") == 0) {
            EntryPoint = ShBuiltinFalse;
        }

        break;

    case 'g':
        if (strcmp(Command + 1, "etopts") == 0) {
            EntryPoint = ShBuiltinGetopts;
        }

        break;

    case 'l':
        if (strcmp(Command + 1, "ocal") == 0) {
            EntryPoint = ShBuiltinLocal;
        }

        break;

    case 'p':
        if (strcmp(Command + 1, "wd") == 0) {
            EntryPoint = ShBuiltinPwd;
        }

        break;

    case 'r':
        if (strcmp(Command + 1, "ead") == 0) {
            EntryPoint = ShBuiltinRead;

        } else if (strcmp(Command + 1, "eadonly") == 0) {
            EntryPoint = ShBuiltinReadOnly;

        } else if (strcmp(Command + 1, "eturn") == 0) {
            EntryPoint = ShBuiltinReturn;
        }

        break;

    case 's':
        if (strcmp(Command + 1, "et") == 0) {
            EntryPoint = ShBuiltinSet;

        } else if (strcmp(Command + 1, "hift") == 0) {
            EntryPoint = ShBuiltinShift;
        }

        break;

    case 't':
        if (strcmp(Command + 1, "imes") == 0) {
            EntryPoint = ShBuiltinTimes;

        } else if (strcmp(Command + 1, "rap") == 0) {
            EntryPoint = ShBuiltinTrap;

        } else if (strcmp(Command + 1, "rue") == 0) {
            EntryPoint = ShBuiltinNop;

        } else if (strcmp(Command + 1, "ype") == 0) {
            EntryPoint = ShBuiltinType;
        }

        break;

    case 'u':
        if (strcmp(Command + 1, "mask") == 0) {
            EntryPoint = ShBuiltinUmask;

        } else if (strcmp(Command + 1, "nalias") == 0) {
            EntryPoint = ShBuiltinUnalias;

        } else if (strcmp(Command + 1, "nset") == 0) {
            EntryPoint = ShBuiltinUnset;
        }

        break;

    default:
        break;
    }

    return EntryPoint;
}

INT
ShRunBuiltinCommand (
    PSHELL Shell,
    PSHELL_BUILTIN_COMMAND Command,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine runs a shell builtin command.

Arguments:

    Shell - Supplies a pointer to the shell.

    Command - Supplies a pointer to the command function to run.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    INT Result;

    //
    // Run the command.
    //

    Result = Command(Shell, ArgumentCount, Arguments);
    fflush(NULL);
    return Result;
}

INT
ShBuiltinEval (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the eval command, which collects all the parameters
    together separated by spaces and reexecutes them in the shell.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    Returns the return value of the command it executes.

--*/

{

    INT ArgumentIndex;
    UINTN ArgumentSize;
    PSTR Input;
    UINTN InputIndex;
    UINTN InputSize;
    ULONG OldOptions;
    SHELL_LEXER_STATE OriginalLexer;
    BOOL Result;
    INT ReturnValue;

    Input = NULL;
    if (ArgumentCount < 2) {
        return 0;
    }

    //
    // Loop through once to figure out how big the input buffer needs to be.
    //

    InputSize = 0;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        ArgumentSize = strlen(Arguments[ArgumentIndex]);
        if (ArgumentSize == 0) {
            continue;
        }

        InputSize += ArgumentSize + 1;
    }

    if (InputSize == 0) {
        return 0;
    }

    //
    // Create the buffer consisting of all the strings separated by spaces.
    //

    Input = malloc(InputSize);
    if (Input == NULL) {
        ReturnValue = ENOMEM;
        goto BuiltinEvalEnd;
    }

    InputIndex = 0;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        ArgumentSize = strlen(Arguments[ArgumentIndex]);
        if (ArgumentSize == 0) {
            continue;
        }

        memcpy(Input + InputIndex, Arguments[ArgumentIndex], ArgumentSize);
        InputIndex += ArgumentSize;
        if (ArgumentIndex == ArgumentCount - 1) {
            Input[InputIndex] = '\0';

        } else {
            Input[InputIndex] = ' ';
        }

        InputIndex += 1;
    }

    assert(InputIndex == InputSize);

    //
    // Save the original lexer and re-initialize the lexer for this new file.
    //

    memcpy(&OriginalLexer, &(Shell->Lexer), sizeof(SHELL_LEXER_STATE));
    Result = ShInitializeLexer(&(Shell->Lexer), NULL, Input, InputSize);
    if (Result == FALSE) {
        ReturnValue = 1;
        memcpy(&(Shell->Lexer), &OriginalLexer, sizeof(SHELL_LEXER_STATE));
        goto BuiltinEvalEnd;
    }

    OldOptions = Shell->Options;
    Shell->Options &= ~SHELL_OPTION_PRINT_PROMPTS;
    Shell->Options |= SHELL_OPTION_INPUT_BUFFER_ONLY;

    //
    // Run the commands.
    //

    Result = ShExecute(Shell, &ReturnValue);

    //
    // Turn the print prompts flag back on if it was set before.
    //

    Shell->Options &= ~SHELL_OPTION_INPUT_BUFFER_ONLY;
    Shell->Options |= OldOptions &
                      (SHELL_OPTION_PRINT_PROMPTS |
                       SHELL_OPTION_INPUT_BUFFER_ONLY);

    //
    // Restore the original lexer.
    //

    ShDestroyLexer(&(Shell->Lexer));
    memcpy(&(Shell->Lexer), &OriginalLexer, sizeof(SHELL_LEXER_STATE));
    if ((Result == FALSE) && (ReturnValue == 0)) {
        ReturnValue = 1;
    }

BuiltinEvalEnd:
    if (Input != NULL) {
        free(Input);
    }

    return ReturnValue;
}

INT
ShRunScriptInContext (
    PSHELL Shell,
    PSTR FilePath,
    ULONG FilePathSize
    )

/*++

Routine Description:

    This routine executes the given script in the current context.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    FilePath - Supplies a pointer to the path of the script to run.

    FilePathSize - Supplies the size of the file path script in bytes,
        including the null terminator.

Return Value:

    Returns the return value from running (or failing to load) the script.

--*/

{

    FILE *NewFile;
    INT NewFileDescriptor;
    INT NewFileDescriptorHigh;
    SHELL_LEXER_STATE OriginalLexer;
    ULONG OriginalOptions;
    BOOL Result;
    INT ReturnValue;

    NewFile = NULL;
    NewFileDescriptor = -1;
    NewFileDescriptorHigh = -1;
    ReturnValue = 0;

    //
    // Open up the new file to be read for commands. Make sure it's out of the
    // user file descriptor range.
    //

    NewFileDescriptor = SwOpen(FilePath, O_RDONLY | O_BINARY, 0);
    if (NewFileDescriptor < 0) {
        SwPrintError(errno, FilePath, "Unable to open");
        ReturnValue = SHELL_ERROR_OPEN;
        goto RunScriptInContextEnd;
    }

    if (NewFileDescriptor >= SHELL_MINIMUM_FILE_DESCRIPTOR) {
        NewFileDescriptorHigh = NewFileDescriptor;
        NewFileDescriptor = -1;

    } else {
        NewFileDescriptorHigh = ShDup(Shell, NewFileDescriptor, FALSE);
        if (NewFileDescriptorHigh < 0) {
            SwPrintError(errno, FilePath, "Unable to dup");
            ReturnValue = SHELL_ERROR_OPEN;
            goto RunScriptInContextEnd;
        }

        assert(NewFileDescriptorHigh >= SHELL_MINIMUM_FILE_DESCRIPTOR);

        close(NewFileDescriptor);
        NewFileDescriptor = -1;
    }

    NewFile = fdopen(NewFileDescriptorHigh, "rb");
    if (NewFile == NULL) {
        SwPrintError(errno, FilePath, "Unable to Open");
        ReturnValue = SHELL_ERROR_OPEN;
        goto RunScriptInContextEnd;
    }

    NewFileDescriptorHigh = -1;

    //
    // Save the original lexer and re-initialize the lexer for this new file.
    //

    memcpy(&OriginalLexer, &(Shell->Lexer), sizeof(SHELL_LEXER_STATE));
    Result = ShInitializeLexer(&(Shell->Lexer), NewFile, NULL, 0);
    if (Result == FALSE) {
        memcpy(&(Shell->Lexer), &OriginalLexer, sizeof(SHELL_LEXER_STATE));
        goto RunScriptInContextEnd;
    }

    NewFile = NULL;
    OriginalOptions = Shell->Options &
                      (SHELL_OPTION_PRINT_PROMPTS |
                       SHELL_OPTION_INTERACTIVE |
                       SHELL_OPTION_RAW_INPUT |
                       SHELL_OPTION_INPUT_BUFFER_ONLY);

    Shell->Options &= ~OriginalOptions;
    Shell->LastReturnValue = 0;

    //
    // Run the commands.
    //

    Result = ShExecute(Shell, &ReturnValue);
    Shell->Options |= OriginalOptions;

    //
    // The signals may have been modified by a subshell and then restored. The
    // problem is that the interactive option was not set. Reset the signal
    // dispositions now that the options have been restored.
    //

    ShSetAllSignalDispositions(Shell);

    //
    // Restore the original lexer.
    //

    ShDestroyLexer(&(Shell->Lexer));
    memcpy(&(Shell->Lexer), &OriginalLexer, sizeof(SHELL_LEXER_STATE));
    if ((Result == FALSE) && (ReturnValue == 0)) {
        ReturnValue = 1;
    }

RunScriptInContextEnd:
    if (NewFileDescriptor >= 0) {
        close(NewFileDescriptor);
    }

    if (NewFileDescriptorHigh >= 0) {
        close(NewFileDescriptorHigh);
    }

    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ShBuiltinBreak (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin break statement.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns the integer return value from the built-in command, which will be
    placed in the node on top of the execution stack. This may not be the node
    that executed this command, as this command may have popped things off the
    execution stack (such as a return, break, or continue will do).

--*/

{

    return ShBuiltinBreakOrContinue(Shell, ArgumentCount, Arguments, TRUE);
}

INT
ShBuiltinContinue (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin break statement.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns the integer return value from the built-in command, which will be
    placed in the node on top of the execution stack. This may not be the node
    that executed this command, as this command may have popped things off the
    execution stack (such as a return, break, or continue will do).

--*/

{

    return ShBuiltinBreakOrContinue(Shell, ArgumentCount, Arguments, FALSE);
}

INT
ShBuiltinReturn (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin return statement.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns the integer return value from the built-in command, which will be
    placed in the node on top of the execution stack. This may not be the node
    that executed this command, as this command may have popped things off the
    execution stack (such as a return, break, or continue will do).

--*/

{

    return ShBuiltinReturnOrExit(Shell, ArgumentCount, Arguments, FALSE);
}

INT
ShBuiltinExit (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin exit statement.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns the integer return value from the built-in command, which will be
    placed in the node on top of the execution stack. This may not be the node
    that executed this command, as this command may have popped things off the
    execution stack (such as a return, break, or continue will do).

--*/

{

    return ShBuiltinReturnOrExit(Shell, ArgumentCount, Arguments, TRUE);
}

INT
ShBuiltinBreakOrContinue (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments,
    BOOL Break
    )

/*++

Routine Description:

    This routine implements the guts of the built in break and continue
    statements.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

    Break - Supplies a boolean indicating if this is a break command (TRUE) or
        a continue command (FALSE).

Return Value:

    Returns the integer return value from the built-in command, which will be
    placed in the node on top of the execution stack. This may not be the node
    that executed this command, as this command may have popped things off the
    execution stack (such as a return, break, or continue will do).

--*/

{

    PSTR AfterScan;
    PLIST_ENTRY CurrentEntry;
    PSHELL_EXECUTION_NODE DestinationLoop;
    ULONG LoopCount;
    PSHELL_EXECUTION_NODE Node;

    //
    // Get the argument to how many loops to exit if there is one.
    //

    LoopCount = 1;
    if (ArgumentCount > 2) {
        return 1;

    } else if (ArgumentCount == 2) {
        LoopCount = strtoul(Arguments[1], &AfterScan, 10);
        if ((LoopCount < 1) || (*AfterScan != '\0')) {
            PRINT_ERROR("sh: break: Invalid count\n");
            return 1;
        }
    }

    assert(LoopCount >= 1);

    if (LIST_EMPTY(&(Shell->ExecutionStack)) != FALSE) {
        return 0;
    }

    //
    // Get the node corresponding to loop N, or if loop N is greater than the
    // number of loops, then just get the outermost loop.
    //

    DestinationLoop = NULL;
    CurrentEntry = Shell->ExecutionStack.Next;
    while (CurrentEntry != &(Shell->ExecutionStack)) {
        Node = LIST_VALUE(CurrentEntry, SHELL_EXECUTION_NODE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (SHELL_LOOP_NODE(Node->Node->Type)) {
            DestinationLoop = Node;
            LoopCount -= 1;
            if (LoopCount == 0) {
                break;
            }
        }
    }

    //
    // If there were no loops on the whole stack, then just return happily.
    //

    if (DestinationLoop == NULL) {
        return 0;
    }

    //
    // Remove nodes up until the destination loop.
    //

    CurrentEntry = Shell->ExecutionStack.Next;
    while (CurrentEntry != &(Shell->ExecutionStack)) {
        Node = LIST_VALUE(CurrentEntry, SHELL_EXECUTION_NODE, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // If this is the destination node, it's removed for breaks, but not
        // for continues.
        //

        if (Node == DestinationLoop) {
            if (Break == FALSE) {
                break;
            }
        }

        //
        // Don't worry about freeing the node, as all the functions on this
        // execution stack are also on the real stack. They'll notice they
        // were removed and return immediately.
        //

        LIST_REMOVE(&(Node->ListEntry));
        Node->ListEntry.Next = NULL;

        //
        // Stop if this is the destination node.
        //

        if (Node == DestinationLoop) {
            break;
        }
    }

    return 0;
}

INT
ShBuiltinReturnOrExit (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments,
    BOOL Exit
    )

/*++

Routine Description:

    This routine implements the return and exit functions.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

    Exit - Supplies a boolean indicating if this is an exit command (TRUE) or
        a return command (FALSE).

Return Value:

    Returns the integer return value from the built-in command, which will be
    placed in the node on top of the execution stack. This may not be the node
    that executed this command, as this command may have popped things off the
    execution stack (such as a return, break, or continue will do).

--*/

{

    PSTR AfterScan;
    PLIST_ENTRY CurrentEntry;
    PSHELL_EXECUTION_NODE Node;
    LONG ReturnValue;

    //
    // Get the return value argument if there is one.
    //

    ReturnValue = Shell->LastReturnValue;
    if (ArgumentCount >= 2) {
        ReturnValue = strtoul(Arguments[1], &AfterScan, 10);
        if (*AfterScan != '\0') {
            PRINT_ERROR("sh: return: invalid argument '%s'\n", Arguments[1]);
            ReturnValue = Shell->LastReturnValue;
        }
    }

    if (LIST_EMPTY(&(Shell->ExecutionStack)) != FALSE) {
        return ReturnValue;
    }

    //
    // Remove nodes up until either the first function for return statements or
    // until there are none for exit statements.
    //

    CurrentEntry = Shell->ExecutionStack.Next;
    while (CurrentEntry != &(Shell->ExecutionStack)) {
        Node = LIST_VALUE(CurrentEntry, SHELL_EXECUTION_NODE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        LIST_REMOVE(&(Node->ListEntry));
        Node->ListEntry.Next = NULL;

        //
        // If this was an executing function and it's a return statement, then
        // stop here.
        //

        if ((Exit == FALSE) && (Node->Node->Type == ShellNodeFunction) &&
            ((Node->Flags & SHELL_EXECUTION_BODY) != 0)) {

            break;
        }
    }

    if (Exit != FALSE) {
        Shell->Exited = TRUE;
    }

    return ReturnValue;
}

INT
ShBuiltinNop (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the no-op colon (:) command. It also doubles as the
    true command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 always.

--*/

{

    return 0;
}

INT
ShBuiltinFalse (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin false command, which just fails
    everything.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    1 always.

--*/

{

    return 1;
}

INT
ShBuiltinDot (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the dot command, which executes commands from the
    given file in the current environment.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    0 always.

--*/

{

    PSTR FullCommandPath;
    ULONG FullCommandPathSize;
    BOOL Result;
    INT ReturnValue;

    if (ArgumentCount < 2) {
        return 0;
    }

    //
    // Find the command.
    //

    ReturnValue = 0;
    Result = ShLocateCommand(Shell,
                             Arguments[1],
                             strlen(Arguments[1]) + 1,
                             FALSE,
                             &FullCommandPath,
                             &FullCommandPathSize,
                             &ReturnValue);

    if (Result == FALSE) {
        goto BuiltinDotEnd;
    }

    if (ReturnValue != 0) {
        if (ReturnValue == SHELL_ERROR_OPEN) {
            PRINT_ERROR("sh: %s: Command not found.\n", Arguments[1]);

        } else if (ReturnValue == SHELL_ERROR_EXECUTE) {
            PRINT_ERROR("sh: %s: Permission denied.\n", Arguments[1]);
        }

        goto BuiltinDotEnd;
    }

    ReturnValue = ShRunScriptInContext(Shell,
                                       FullCommandPath,
                                       FullCommandPathSize);

    if ((Result == FALSE) && (ReturnValue == 0)) {
        ReturnValue = 1;
    }

BuiltinDotEnd:
    if ((FullCommandPath != NULL) && (FullCommandPath != Arguments[1])) {
        free(FullCommandPath);
    }

    return ReturnValue;
}

INT
ShBuiltinExec (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the exec command, which makes the current shell
    into the given program.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    0 if the command ran successfully. In this case the shell's exited flag
    will be set.

    Returns an error code if the app failed to launch.

--*/

{

    PSHELL_ACTIVE_REDIRECT ActiveRedirect;
    PSHELL_BUILTIN_COMMAND BuiltinCommand;
    PSHELL_EXECUTION_NODE ExecutionNode;
    PSTR FullCommandPath;
    ULONG FullCommandPathSize;
    INT Result;
    INT ReturnValue;

    FullCommandPath = NULL;

    //
    // If there are no arguments, pull off any active redirection entries so
    // they are not undone when the command finishes.
    //

    if (ArgumentCount <= 1) {
        ExecutionNode = LIST_VALUE(Shell->ExecutionStack.Next,
                                   SHELL_EXECUTION_NODE,
                                   ListEntry);

        assert(ExecutionNode->Node->Type == ShellNodeSimpleCommand);

        while (LIST_EMPTY(&(ExecutionNode->ActiveRedirectList)) == FALSE) {
            ActiveRedirect = LIST_VALUE(ExecutionNode->ActiveRedirectList.Next,
                                        SHELL_ACTIVE_REDIRECT,
                                        ListEntry);

            LIST_REMOVE(&(ActiveRedirect->ListEntry));
            INSERT_BEFORE(&(ActiveRedirect->ListEntry),
                          &(Shell->ActiveRedirectList));
        }

        return 0;
    }

    Arguments += 1;
    ArgumentCount -= 1;

    //
    // Check to see if this is a builtin command, and run it if it is.
    //

    BuiltinCommand = ShIsBuiltinCommand(Arguments[0]);
    if (BuiltinCommand != NULL) {
        ReturnValue = ShRunBuiltinCommand(Shell,
                                          BuiltinCommand,
                                          ArgumentCount,
                                          Arguments);

        Shell->Exited = TRUE;
        Shell->SkipExitSignal = TRUE;
        goto BuiltinExecEnd;

    } else {

        //
        // If fork is supported, then actually try to exec the item.
        //

        ReturnValue = 0;
        if (SwForkSupported != FALSE) {
            Result = ShLocateCommand(Shell,
                                     Arguments[0],
                                     strlen(Arguments[0]) + 1,
                                     TRUE,
                                     &FullCommandPath,
                                     &FullCommandPathSize,
                                     &ReturnValue);

            if (Result == FALSE) {
                if (ReturnValue == 0) {
                    ReturnValue = 1;
                }

                goto BuiltinExecEnd;
            }

            if (ReturnValue != 0) {
                if (ReturnValue == SHELL_ERROR_OPEN) {
                    PRINT_ERROR("sh: %s: Command not found.\n", Arguments[0]);

                } else if (ReturnValue == SHELL_ERROR_EXECUTE) {
                    PRINT_ERROR("sh: %s: Permission denied.\n", Arguments[0]);
                }

                Shell->ReturnValue = ReturnValue;
                goto BuiltinExecEnd;
            }

            fflush(NULL);

            //
            // Execute the destination image. If this fails, exit immediately
            // anyway.
            //

            ShRestoreOriginalSignalDispositions();
            ReturnValue = SwExec(FullCommandPath, Arguments, ArgumentCount);
            ShSetAllSignalDispositions(Shell);
            SwPrintError(ReturnValue, FullCommandPath, "Failed to exec");
            Shell->ReturnValue = ReturnValue;

        //
        // If fork is not supported, then subshells never forked, and this
        // process needs to unwind back up to that. Run the command, then
        // go back up to the previous subshell.
        //

        } else {
            Result = ShRunCommand(Shell,
                                  Arguments[0],
                                  Arguments,
                                  ArgumentCount,
                                  FALSE,
                                  &ReturnValue);

            if (Result == 0) {
                ShOsConvertExitStatus(&ReturnValue);
                Shell->ReturnValue = ReturnValue;
                Shell->Exited = TRUE;
                Shell->SkipExitSignal = TRUE;
                goto BuiltinExecEnd;

            } else {
                SwPrintError(Result, Arguments[0], "Failed to exec");
                ReturnValue = 1;
            }
        }
    }

BuiltinExecEnd:
    if ((FullCommandPath != NULL) && (FullCommandPath != Arguments[0])) {
        free(FullCommandPath);
    }

    return ReturnValue;
}

INT
ShBuiltinRead (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the read command, which reads a line from standard
    in, splits it, and assigns variable names given on the command line to the
    given fields.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    0 if the command ran successfully. In this case the shell's exited flag
    will be set.

    Returns an error code if the app failed to launch.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    UINTN ArgumentSize;
    UCHAR Character;
    BOOL EndOfFileDetected;
    SHELL_EXPANSION_RANGE Expansion;
    LIST_ENTRY ExpansionList;
    PSTR Field;
    ULONG FieldCount;
    PSTR *Fields;
    ULONG FieldSize;
    BOOL IgnoreBackslash;
    PSTR Line;
    UINTN LineCapacity;
    UINTN LineSize;
    PSTR NewBuffer;
    BOOL Result;
    INT Status;
    BOOL WasBackslash;

    assert(ArgumentCount != 0);

    EndOfFileDetected = FALSE;
    Fields = NULL;

    //
    // Skip over the "read" argument.
    //

    Arguments += 1;
    ArgumentCount -= 1;
    IgnoreBackslash = FALSE;
    if ((ArgumentCount != 0) && (strcmp(Arguments[0], "-r") == 0)) {
        IgnoreBackslash = TRUE;
        Arguments += 1;
        ArgumentCount -= 1;
    }

    //
    // Read a line of input.
    //

    LineCapacity = SHELL_READ_INITIAL_STRING_SIZE;
    Line = malloc(LineCapacity);
    if (Line == NULL) {
        Status = ENOMEM;
        goto BuiltinReadEnd;
    }

    LineSize = 0;
    WasBackslash = FALSE;
    while (TRUE) {
        do {
            Status = read(STDIN_FILENO, &Character, 1);

        } while ((Status < 0) && (errno == EINTR));

        if (Status < 0) {
            Status = errno;
            ShPrintTrace(Shell, "sh: Failed read: %s.\n", strerror(Status));
            goto BuiltinReadEnd;
        }

        if (Status == 0) {
            EndOfFileDetected = TRUE;
            break;
        }

        if (Character == '\n') {

            //
            // A backslash followed by a newline is a line continuation.
            // Remove the backslash from the input line.
            //

            if (WasBackslash != FALSE) {
                continue;

            } else {

                //
                // Remove any carriage returns that may have strayed along.
                //

                if ((LineSize != 0) && (Line[LineSize - 1] == '\r')) {
                    LineSize -= 1;
                }

                break;
            }

        //
        // A backslash followed by any character preserves the literal meaning
        // of that character. Remove the backslash from the input line.
        //

        } else if (WasBackslash != FALSE) {
            LineSize -= 1;
        }

        //
        // Allocate more space for the line if needed. Always have enough
        // space for the terminator as well.
        //

        if (LineSize + 2 >= LineCapacity) {
            LineCapacity *= 2;
            NewBuffer = realloc(Line, LineCapacity);
            if (NewBuffer == NULL) {
                Status = ENOMEM;
                goto BuiltinReadEnd;
            }

            Line = NewBuffer;
        }

        Line[LineSize] = Character;
        LineSize += 1;

        //
        // Keep track of whether or not the previous character was a backslash,
        // but only if backslashes are not being ignored.
        //

        if ((Character == '\\') && (IgnoreBackslash == FALSE)) {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }
    }

    //
    // Terminate the line.
    //

    Line[LineSize] = '\0';
    LineSize += 1;

    //
    // Split the line into fields.
    //

    INITIALIZE_LIST_HEAD(&ExpansionList);
    Expansion.Type = ShellExpansionFieldSplit;
    Expansion.Index = 0;
    Expansion.Length = LineSize;
    INSERT_BEFORE(&(Expansion.ListEntry), &ExpansionList);
    Result = ShFieldSplit(Shell,
                          &Line,
                          &LineSize,
                          &ExpansionList,
                          ArgumentCount,
                          &Fields,
                          &FieldCount);

    if (Result == FALSE) {
        Status = 1;
        goto BuiltinReadEnd;
    }

    //
    // Assign every argument to the field.
    //

    for (ArgumentIndex = 0; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        ArgumentSize = strlen(Argument) + 1;
        if (ShIsName(Argument, ArgumentSize) == FALSE) {
            PRINT_ERROR("read: Invalid variable name '%s'.\n", Argument);
            Status = EINVAL;
            goto BuiltinReadEnd;
        }

        if (ArgumentIndex < FieldCount) {
            Field = Fields[ArgumentIndex];

        } else {
            Field = "";
        }

        FieldSize = strlen(Field) + 1;
        Result = ShSetVariable(Shell,
                               Argument,
                               ArgumentSize,
                               Field,
                               FieldSize);

        if (Result == FALSE) {
            PRINT_ERROR("read: Unable to set variable '%s'.\n", Argument);
            Status = 1;
            goto BuiltinReadEnd;
        }
    }

    Status = 0;
    if (EndOfFileDetected != FALSE) {
        Status = 1;
    }

BuiltinReadEnd:
    if (Fields != NULL) {
        free(Fields);
    }

    if (Line != NULL) {
        free(Line);
    }

    return Status;
}

INT
ShBuiltinShift (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the shift command, which chomps away at the
    positional arguments.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line. The
        only valid values are 1 and 2 (only one optional argument expected).

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    1 on failure.

    0 on success.

--*/

{

    PSTR AfterScan;
    PSHELL_ARGUMENT Argument;
    PLIST_ENTRY ArgumentList;
    PLIST_ENTRY CurrentEntry;
    ULONG ShellArgumentCount;
    ULONG ShiftCount;
    ULONG ShiftIndex;

    ShellArgumentCount = 0;
    ArgumentList = ShGetCurrentArgumentList(Shell);

    //
    // Loop through once to count arguments.
    //

    CurrentEntry = ArgumentList->Next;
    while (CurrentEntry != ArgumentList) {
        Argument = LIST_VALUE(CurrentEntry, SHELL_ARGUMENT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        ShellArgumentCount += 1;
    }

    //
    // Convert the optional argument to a shift count.
    //

    ShiftCount = 1;
    if (ArgumentCount > 1) {
        ShiftCount = strtol(Arguments[1], &AfterScan, 10);
        if ((AfterScan == Arguments[1]) || (*AfterScan != '\0')) {
            PRINT_ERROR("shift: Illegal number %s.\n", Arguments[1]);
            return 1;
        }
    }

    //
    // Don't overextend.
    //

    if (ShiftCount > ShellArgumentCount) {
        PRINT_ERROR("shift: Can't shift by %d, only %d arguments.\n",
                    ShiftCount,
                    ShellArgumentCount);

        return 1;
    }

    //
    // Pull arguments off the list.
    //

    for (ShiftIndex = 0; ShiftIndex < ShiftCount; ShiftIndex += 1) {

        assert(LIST_EMPTY(ArgumentList) == FALSE);

        Argument = LIST_VALUE(ArgumentList->Next, SHELL_ARGUMENT, ListEntry);
        LIST_REMOVE(&(Argument->ListEntry));
        if (Argument->Name != NULL) {
            free(Argument->Name);
        }

        free(Argument);
    }

    return 0;
}

INT
ShBuiltinTimes (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the times command, which prints execution
    statistics about the shell and its children.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    BOOL Result;
    SHELL_PROCESS_TIMES Times;

    Result = ShGetExecutionTimes(&Times);
    if (Result == FALSE) {
        return 1;
    }

    //
    // Floats are for losers.
    //

    printf("%lldm%ld.%06lds %lldm%ld.%06lds\n"
           "%lldm%ld.%06lds %lldm%ld.%06lds\n",
           Times.ShellUserMinutes,
           Times.ShellUserMicroseconds / 1000000,
           Times.ShellUserMicroseconds % 1000000,
           Times.ShellSystemMinutes,
           Times.ShellSystemMicroseconds / 1000000,
           Times.ShellSystemMicroseconds % 1000000,
           Times.ChildrenUserMinutes,
           Times.ChildrenUserMicroseconds / 1000000,
           Times.ChildrenUserMicroseconds % 1000000,
           Times.ChildrenSystemMinutes,
           Times.ChildrenSystemMicroseconds / 1000000,
           Times.ChildrenSystemMicroseconds % 1000000);

    return 0;
}

INT
ShBuiltinUmask (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the uname builtin command, which changes the
    umask of the process the current shell is running in.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    mode_t Mask;
    PSTR ModeString;
    mode_t OriginalMask;
    BOOL Result;
    BOOL Symbolic;

    Symbolic = FALSE;
    if (ArgumentCount > 3) {
        fprintf(stderr, "usage: umask [-S] [mask]\n");
        return 1;
    }

    ModeString = NULL;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        if (strcmp(Argument, "-S") == 0) {
            Symbolic = TRUE;

        } else {
            ModeString = Argument;
        }
    }

    OriginalMask = umask(0);
    umask(OriginalMask);

    //
    // If a mode string was supplied, create the and set the new mask.
    //

    if (ModeString != NULL) {
        Mask = OriginalMask;
        Result = SwParseFilePermissionsString(ModeString, FALSE, &Mask);
        if (Result == FALSE) {
            fprintf(stderr,
                    "umask: Could not parse mode string '%s'.\n",
                    ModeString);

            return 1;
        }

        umask(Mask);

    //
    // If there's no mode string, print the current mask out.
    //

    } else {
        if (Symbolic != FALSE) {

            //
            // Go through the motions to print out something pretty.
            //

            Mask = OriginalMask;
            printf("u=");
            if ((Mask & S_IRUSR) == 0) {
                fputc('r', stdout);
            }

            if ((Mask & S_IWUSR) == 0) {
                fputc('w', stdout);
            }

            if ((Mask & S_IXUSR) == 0) {
                fputc('x', stdout);
            }

            printf(",g=");
            if ((Mask & S_IRGRP) == 0) {
                fputc('r', stdout);
            }

            if ((Mask & S_IWGRP) == 0) {
                fputc('w', stdout);
            }

            if ((Mask & S_IXGRP) == 0) {
                fputc('x', stdout);
            }

            printf(",o=");
            if ((Mask & S_IROTH) == 0) {
                fputc('r', stdout);
            }

            if ((Mask & S_IWOTH) == 0) {
                fputc('w', stdout);
            }

            if ((Mask & S_IXOTH) == 0) {
                fputc('x', stdout);
            }

            printf("\n");

        //
        // Just print out the octal value.
        //

        } else {
            printf("%04o\n", OriginalMask);
        }
    }

    return 0;
}

INT
ShBuiltinGetopts (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the getopts builtin command, which parses
    positional parameters or the supplied arguments for command.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 on success.

    Returns greater than zero if an error occurred.

--*/

{

    PSTR AfterScan;
    PSTR Argument;
    ULONG ArgumentIndex;
    INT BytesConverted;
    BOOL EndOfOptions;
    PSTR ExpandedArguments;
    UINTN ExpandedArgumentsSize;
    CHAR NewOptionIndex[SHELL_MAX_OPTION_INDEX_LENGTH];
    CHAR NewOptionValueBuffer[2];
    PSTR NewOptionVariable;
    PSTR OptionArgument;
    CHAR OptionCharacter;
    PSTR OptionIndexString;
    ULONG OptionsArgumentCount;
    PSTR *OptionsArguments;
    INT OptionsIndex;
    PSTR OptionsString;
    BOOL Result;
    INT ReturnValue;
    INT Status;
    BOOL UsingPositionalParameters;

    OptionArgument = NULL;
    ReturnValue = 0;

    //
    // There are no options to the getopts utility, but eat up arguments
    // looking for bad options.
    //

    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];

        //
        // Break out if a non-argument is seen.
        //

        if (*Argument != '-') {
            break;
        }

        //
        // Break out and advance the argument index if the "--" is seen.
        //

        Argument += 1;
        if (strcmp(Argument, "-") == 0) {
            ArgumentIndex += 1;
            break;
        }

        //
        // Anything else is an error.
        //

        fprintf(stderr, "getopts: invalid option '-%c'.\n", *Argument);
        fprintf(stderr, "usage: getopts optstring name [arg...]\n");
        return 2;
    }

    //
    // Argument index holds the position of the option string. If there aren't
    // at least two parameters remaining, then exit.
    //

    if ((ArgumentCount - ArgumentIndex) < 2) {
        fprintf(stderr, "usage: getopts optstring name [arg...]\n");
        return 2;
    }

    OptionsString = Arguments[ArgumentIndex];
    NewOptionVariable = Arguments[ArgumentIndex + 1];
    ArgumentIndex += 2;

    //
    // Try to get the option index. If it's not there or invalid, then reset to
    // 1.
    //

    OptionsIndex = 1;
    Result = ShGetVariable(Shell,
                           SHELL_OPTION_INDEX,
                           sizeof(SHELL_OPTION_INDEX),
                           &OptionIndexString,
                           NULL);

    if (Result != FALSE) {
        OptionsIndex = (int)strtol(OptionIndexString, &AfterScan, 10);
        if ((OptionIndexString == AfterScan) || (*AfterScan != '\0')) {
            OptionsIndex = 1;
        }
    }

    //
    // If the options index is less than 1, then reset it.
    //

    if (OptionsIndex < 1) {
        ShOptionsIndex = OptionsIndex;
        OptionsIndex = 1;
    }

    //
    // Reset if the caller tried to manipluate the option index.
    //

    if (ShOptionsIndex != OptionsIndex) {
        ShOptionsIndex = OptionsIndex;
        ShNextOptionCharacter = 0;
        ShSeenDoubleDash = FALSE;
    }

    //
    // If arguments are present, those are preferred to the positional
    // parameters.
    //

    UsingPositionalParameters = FALSE;
    if (ArgumentIndex != ArgumentCount) {
        OptionsArgumentCount = ArgumentCount - ArgumentIndex;
        OptionsArguments = &(Arguments[ArgumentIndex]);

    } else {
        Result = ShPerformExpansions(Shell,
                                     "$@",
                                     sizeof("$@"),
                                     0,
                                     &ExpandedArguments,
                                     &ExpandedArgumentsSize,
                                     &OptionsArguments,
                                     &OptionsArgumentCount);

        if (Result == FALSE) {
            ReturnValue += 1;
            goto BuiltinGetoptsEnd;
        }

        UsingPositionalParameters = TRUE;
    }

    //
    // The options index is off by one because the arguments array does not
    // have the command as the first entry.
    //

    OptionsIndex -= 1;

    //
    // Get the next option using the built-in parser rather than the C library
    // getopts() routine. Things are handled slightly differently with regards
    // to the option string.
    //

    Status = ShGetNextOption(Shell,
                             OptionsArgumentCount,
                             OptionsArguments,
                             &OptionsIndex,
                             OptionsString,
                             &OptionCharacter,
                             &OptionArgument,
                             &EndOfOptions);

    if (Status == 0) {
        ReturnValue += 1;
        goto BuiltinGetoptsEnd;
    }

    //
    // Shift the options index back as it is stored considering the command as
    // 0.
    //

    OptionsIndex += 1;
    ShOptionsIndex = OptionsIndex;

    //
    // Update the environment variables.
    //

    BytesConverted = snprintf(NewOptionIndex,
                              SHELL_MAX_OPTION_INDEX_LENGTH,
                              "%d",
                              OptionsIndex);

    if (BytesConverted < 0) {
        ReturnValue += 1;
        goto BuiltinGetoptsEnd;
    }

    Result = ShSetVariable(Shell,
                           SHELL_OPTION_INDEX,
                           sizeof(SHELL_OPTION_INDEX),
                           NewOptionIndex,
                           BytesConverted + 1);

    if (Result == FALSE) {
        ReturnValue += 1;
        goto BuiltinGetoptsEnd;
    }

    NewOptionValueBuffer[0] = OptionCharacter;
    NewOptionValueBuffer[1] = '\0';
    BytesConverted = 1;
    Result = ShSetVariable(Shell,
                           NewOptionVariable,
                           strlen(NewOptionVariable) + 1,
                           NewOptionValueBuffer,
                           BytesConverted + 1);

    if (Result == FALSE) {
        ReturnValue += 1;
        goto BuiltinGetoptsEnd;
    }

    if (OptionArgument != NULL) {
        Result = ShSetVariable(Shell,
                               SHELL_OPTION_ARGUMENT,
                               sizeof(SHELL_OPTION_ARGUMENT),
                               OptionArgument,
                               strlen(OptionArgument) + 1);

        if (Result == FALSE) {
            ReturnValue += 1;
            goto BuiltinGetoptsEnd;
        }

    } else {
        ShUnsetVariableOrFunction(Shell,
                                  SHELL_OPTION_ARGUMENT,
                                  sizeof(SHELL_OPTION_ARGUMENT),
                                  ShellUnsetDefault);
    }

    //
    // If the end of options was reached, return a non-zero value.
    //

    if (EndOfOptions != FALSE) {
        ReturnValue += 1;
    }

BuiltinGetoptsEnd:
    if (OptionArgument != NULL) {
        free(OptionArgument);
    }

    if (UsingPositionalParameters != FALSE) {
        if (ExpandedArguments != NULL) {
            free(ExpandedArguments);
        }

        if (OptionsArguments != NULL) {
            free(OptionsArguments);
        }
    }

    return ReturnValue;
}

INT
ShBuiltinCommand (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the command builtin command, which runs the given
    command without invoking shell functions.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    INT Result;

    Result = ShBuiltinTypeOrCommand(Shell, ArgumentCount, Arguments, FALSE);
    return Result;
}

INT
ShBuiltinType (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the 'type' builtin command, which describes the
    given commands.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    INT Result;

    Result = ShBuiltinTypeOrCommand(Shell, ArgumentCount, Arguments, TRUE);
    return Result;
}

INT
ShBuiltinTypeOrCommand (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments,
    BOOL IsType
    )

/*++

Routine Description:

    This routine implements the command or type builtins, which run or describe
    a command.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

    IsType - Supplies a boolean indicating whether this is the type builtin
        function (TRUE) or the command builtin function (FALSE).

Return Value:

    0 on success.

    1 on failure.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    PSHELL_BUILTIN_COMMAND BuiltinCommand;
    BOOL DefaultPath;
    PSTR OriginalPath;
    UINTN OriginalPathSize;
    BOOL PrintPath;
    INT ReturnValue;
    INT TotalReturnValue;
    BOOL Verbose;

    DefaultPath = FALSE;
    OriginalPath = NULL;
    PrintPath = FALSE;
    Verbose = FALSE;
    TotalReturnValue = 0;

    //
    // Type acts just like command -v.
    //

    if (IsType != FALSE) {
        Verbose = TRUE;
        ArgumentIndex = 1;

    //
    // Handle the arguments for the command command.
    //

    } else {
        for (ArgumentIndex = 1;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            Argument = Arguments[ArgumentIndex];
            if (*Argument != '-') {
                break;
            }

            Argument += 1;
            if (*Argument == '-') {
                break;
            }

            while (*Argument != '\0') {
                switch (*Argument) {
                case 'p':
                    DefaultPath = TRUE;
                    break;

                case 'v':
                    PrintPath = TRUE;
                    break;

                case 'V':
                    Verbose = TRUE;
                    break;

                default:
                    fprintf(stderr, "command: Invalid option %c.\n", *Argument);
                    break;
                }

                Argument += 1;
            }
        }

        if (PrintPath != FALSE) {
            Verbose = FALSE;
        }
    }

    Arguments += ArgumentIndex;
    ArgumentCount -= ArgumentIndex;

    //
    // If the command is empty, don't do much.
    //

    if ((ArgumentCount == 0) || (*(Arguments[0]) == '\0')) {
        return 0;
    }

    if (DefaultPath != FALSE) {
        ShGetVariable(Shell,
                      SHELL_PATH,
                      sizeof(SHELL_PATH),
                      &OriginalPath,
                      &OriginalPathSize);

        OriginalPath = SwStringDuplicate(OriginalPath, OriginalPathSize);
        ShSetVariableWithProperties(Shell,
                                    SHELL_PATH,
                                    sizeof(SHELL_PATH),
                                    SHELL_COMMAND_BUILTIN_PATH,
                                    sizeof(SHELL_COMMAND_BUILTIN_PATH),
                                    TRUE,
                                    FALSE,
                                    TRUE);
    }

    if ((Verbose != FALSE) || (PrintPath != FALSE)) {

        //
        // Loop over all the arguments, though if this is not the 'type'
        // builtin this will break after the first iteration.
        //

        while (ArgumentCount != 0) {
            ReturnValue = ShClassifyCommand(Shell, Arguments[0], Verbose);
            if (ReturnValue != 0) {
                TotalReturnValue = ReturnValue;
            }

            if (IsType == FALSE) {
                break;
            }

            Arguments += 1;
            ArgumentCount -= 1;
        }

    //
    // Really run the command.
    //

    } else {
        BuiltinCommand = ShIsBuiltinCommand(Arguments[0]);
        if (BuiltinCommand != NULL) {
            ReturnValue = ShRunBuiltinCommand(Shell,
                                              BuiltinCommand,
                                              ArgumentCount,
                                              Arguments);

        } else {
            ShRunCommand(Shell,
                         Arguments[0],
                         Arguments,
                         ArgumentCount,
                         FALSE,
                         &ReturnValue);
        }
    }

    if ((TotalReturnValue == 0) && (ReturnValue != 0)) {
        TotalReturnValue = ReturnValue;
    }

    if (OriginalPath != NULL) {
        ShSetVariable(Shell,
                      SHELL_PATH,
                      sizeof(SHELL_PATH),
                      OriginalPath,
                      OriginalPathSize);
    }

    return TotalReturnValue;
}

INT
ShClassifyCommand (
    PSHELL Shell,
    PSTR Command,
    BOOL Verbose
    )

/*++

Routine Description:

    This routine classifies and prints the classification for the given command.

Arguments:

    Shell - Supplies a pointer to the shell.

    Command - Supplies a pointer to the command to classify.

    Verbose - Supplies a boolean indicating whether to print the verbose
        description or just the command path (or name).

Return Value:

    0 on success.

    127 if the command could not be found.

--*/

{

    PSHELL_ALIAS Alias;
    PSHELL_BUILTIN_COMMAND BuiltinCommand;
    PSTR FullCommandPath;
    ULONG FullCommandPathSize;
    PSHELL_FUNCTION Function;
    PSTR ReservedWord;
    UINTN ReservedWordIndex;
    BOOL Result;
    INT ReturnValue;

    //
    // First look to see if it is a reserved word.
    //

    ReservedWordIndex = 0;
    while (ShReservedWords[ReservedWordIndex] != NULL) {
        ReservedWord = ShReservedWords[ReservedWordIndex];
        if (strcmp(Command, ReservedWord) == 0) {
            if (Verbose != FALSE) {
                printf("%s is a shell keyword\n", Command);

            } else {
                printf("%s\n", Command);
            }

            return 0;
        }

        ReservedWordIndex += 1;
    }

    BuiltinCommand = ShIsBuiltinCommand(Command);
    if (BuiltinCommand != NULL) {
        ReturnValue = 0;
        if (Verbose != FALSE) {
            printf("%s is a shell builtin\n", Command);

        } else {
            printf("%s\n", Command);
        }

        return 0;
    }

    //
    // Then look to see if it is an alias.
    //

    Alias = ShLookupAlias(Shell, Command, strlen(Command) + 1);
    if (Alias != NULL) {
        if (Verbose != FALSE) {
            printf("%s is an alias for %s\n", Command, Alias->Value);

        } else {
            printf("alias %s='%s'\n", Command, Alias->Value);
        }

        return 0;
    }

    //
    // Look to see if this is a function.
    //

    Function = ShGetFunction(Shell, Command, strlen(Command) + 1);
    if (Function != NULL) {
        if (Verbose != FALSE) {
            printf("%s is a shell function\n", Command);

        } else {
            printf("%s\n", Command);
        }

        return 0;
    }

    //
    // Attempt to locate the command in the path.
    //

    ReturnValue = 0;
    FullCommandPath = NULL;
    Result = ShLocateCommand(Shell,
                             Command,
                             strlen(Command) + 1,
                             TRUE,
                             &FullCommandPath,
                             &FullCommandPathSize,
                             &ReturnValue);

    if (Result == FALSE) {
        ReturnValue = SHELL_ERROR_OPEN;
    }

    if (ReturnValue != 0) {
        if (Verbose != FALSE) {
            ShPrintTrace(Shell, "sh: %s: Command not found.\n", Command);
        }

    } else {
        if (Verbose != FALSE) {
            printf("%s is %s\n", Command, FullCommandPath);

        } else {
            printf("%s\n", FullCommandPath);
        }
    }

    if ((FullCommandPath != NULL) && (FullCommandPath != Command)) {
        free(FullCommandPath);
    }

    return ReturnValue;
}

INT
ShGetNextOption (
    PSHELL Shell,
    ULONG ArgumentCount,
    PSTR *Arguments,
    PINT ArgumentIndex,
    PSTR Options,
    PCHAR Option,
    PSTR *OptionArgument,
    PBOOL EndOfOptions
    )

/*++

Routine Description:

    This routine gets the next option from the list of arguments. Starting at
    the given argument index, it determines if that argument string begins with
    an option.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments in the arguments array.

    Arguments - Supplies the arguments to parse. The first array item is the
        first argument (e.g. positional parameter 1). On return it will contain
        the index of the next argument to parse.

    ArgumentIndex - Supplies the index into the argument array that should be
        analyzed for an option.

    Options - Supplies the options string.

    Option - Supplies a pointer that receives the parsed option.

    OptionArgument - Supplies a pointer that receives the option's argument
        string. The caller is responsible for releasing this memory.

    EndOfOptions - Supplies a pointer that receives a boolean indicating
        whether or not the end of options has been reached.

Return Value:

    1 on success.

    0 on failure.

--*/

{

    PSTR Argument;
    CHAR CharacterString[2];
    PSTR OptionError;
    BOOL Result;
    BOOL StartsWithColon;

    *OptionArgument = NULL;
    *EndOfOptions = FALSE;

    //
    // If the argument index is beyond the bounds of the array, then return as
    // if the end of the options was reached.
    //

    if (*ArgumentIndex >= ArgumentCount) {
        *ArgumentIndex = ArgumentCount;
        *Option = '?';
        ShNextOptionCharacter = 0;
        *EndOfOptions = TRUE;
        return 1;
    }

    //
    // If the next option character value is 0, then this argument has not been
    // processed.
    //

    Argument = Arguments[*ArgumentIndex];
    if (ShNextOptionCharacter == 0) {

        //
        // If the argument does not start with a dash, then this is the end of
        // the arguments.
        //

        if (*Argument != '-') {
            *Option = '?';
            *EndOfOptions = TRUE;
            return 1;
        }

        Argument += 1;

        //
        // If this is the "--" argument, then it's also the end of the line.
        //

        if ((*Argument == '-') && (*(Argument + 1) == '\0')) {
            *Option = '?';

            //
            // If the "--" has not yet been seen, then the index needs to jump
            // to the next argument.
            //

            if (ShSeenDoubleDash == FALSE) {
                *ArgumentIndex += 1;
                ShSeenDoubleDash = TRUE;
            }

            *EndOfOptions = TRUE;
            return 1;
        }

        //
        // Ok. There might be some options in this argument, start looking at
        // the second character.
        //

        ShNextOptionCharacter = 1;

    } else {
        Argument += ShNextOptionCharacter;
    }

    assert(*Argument != '\0');

    StartsWithColon = FALSE;
    if (*Options == ':') {
        StartsWithColon = TRUE;
        Options += 1;
    }

    //
    // Loop over every acceptible option.
    //

    while (*Options != '\0') {

        //
        // Keep looking if they are not equal.
        //

        if ((!isalnum(*Options)) || (*Argument != *Options)) {
            Options += 1;
            continue;
        }

        //
        // They're equal, look to see if the next character is a colon.
        //

        *Option = *Options;
        Options += 1;
        ShNextOptionCharacter += 1;
        Argument += 1;

        //
        // If no argument is required, then work here is done.
        //

        if (*Options != ':') {

            //
            // If the next character of the argument is the terminator, then
            // up the index and reset the option character.
            //

            if (*Argument == '\0') {
                ShNextOptionCharacter = 0;
                *ArgumentIndex += 1;
            }

            return 1;
        }

        Options += 1;

        //
        // An argument is required. Optional arguments are not supported for
        // the built-in shell option parsing. If the next character of the
        // argument is not null, then the argument is the remainder.
        //

        ShNextOptionCharacter = 0;
        if (*Argument != '\0') {
            *OptionArgument = strdup(Argument);
            if (*OptionArgument == NULL) {
                return 0;
            }

            *ArgumentIndex += 1;
            return 1;
        }

        //
        // It must be the next argument. If there is no next argument, that's a
        // problem.
        //

        if (*ArgumentIndex >= (ArgumentCount - 1)) {
            *ArgumentIndex += 1;
            if (StartsWithColon != FALSE) {
                CharacterString[0] = *Option;
                CharacterString[1] = '\0';
                *OptionArgument = strdup(CharacterString);
                if (*OptionArgument == NULL) {
                    return 0;
                }

                *Option = ':';

            } else {

                assert(*OptionArgument == NULL);

                Result = ShGetVariable(Shell,
                                       SHELL_OPTION_ERROR,
                                       sizeof(SHELL_OPTION_ERROR),
                                       &OptionError,
                                       NULL);

                if ((Result == FALSE) || (strcmp(OptionError, "0") != 0)) {
                    fprintf(stderr,
                            "%s: option -%c requires an argument.\n",
                            Shell->CommandName,
                            *Option);
                }

                *Option = '?';
            }

            return 1;
        }

        *ArgumentIndex += 1;
        *OptionArgument = strdup(Arguments[*ArgumentIndex]);
        if (*OptionArgument == NULL) {
            return 0;
        }

        *ArgumentIndex += 1;
        return 1;
    }

    //
    // The argument doesn't match any of the acceptable options.
    //

    if (StartsWithColon != FALSE) {
        CharacterString[0] = *Option;
        CharacterString[1] = '\0';
        *OptionArgument = strdup(CharacterString);
        if (*OptionArgument == NULL) {
            return 0;
        }

    } else {

        assert(*OptionArgument == NULL);

        Result = ShGetVariable(Shell,
                               SHELL_OPTION_ERROR,
                               sizeof(SHELL_OPTION_ERROR),
                               &OptionError,
                               NULL);

        if ((Result == FALSE) || (strcmp(OptionError, "0") != 0)) {
            fprintf(stderr,
                    "%s: Invalid option -%c.\n",
                    Shell->CommandName,
                    *Argument);
        }
    }

    //
    // Skip to the next option. It could be in the next argument or next
    // character.
    //

    Argument += 1;
    if (*Argument == '\0') {
        ShNextOptionCharacter = 0;
        *ArgumentIndex += 1;

    } else {
        ShNextOptionCharacter += 1;
    }

    *Option = '?';
    return 1;
}

