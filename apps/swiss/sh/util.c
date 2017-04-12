/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    util.c

Abstract:

    This module implements utility functions for the sh shell.

Author:

    Evan Green 6-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the initial size of the buffer used to read standard output from the
// subshell.
//

#define SHELL_INITIAL_OUTPUT_BUFFER_SIZE 1024

#define SHELL_INITIAL_ARGUMENTS_SIZE 256
#define SHELL_INITIAL_FIELDS_COUNT 16

#define SHELL_DEFAULT_SEPARATORS " \t\n"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ShStringDequoteSubshellCommand (
    PSTR Input,
    PUINTN InputSize
    );

//
// -------------------------------------------------------------------- Globals
//

CHAR ShEmptyQuotedString[3] = {SHELL_CONTROL_QUOTE, SHELL_CONTROL_QUOTE, '\0'};

//
// ------------------------------------------------------------------ Functions
//

PSHELL
ShCreateShell (
    PSTR CommandName,
    UINTN CommandNameSize
    )

/*++

Routine Description:

    This routine creates a new shell object.

Arguments:

    CommandName - Supplies a pointer to the string containing the name of the
        command that invoked the shell. A copy of this buffer will be made.

    CommandNameSize - Supplies the length of the command name buffer in bytes
        including the null terminator.

Return Value:

    Returns a pointer to the allocated shell on success.

    NULL on failure.

--*/

{

    BOOL Result;
    PSHELL Shell;

    Result = FALSE;
    Shell = malloc(sizeof(SHELL));
    if (Shell == NULL) {
        goto CreateShellEnd;
    }

    memset(Shell, 0, sizeof(SHELL));
    Shell->PostForkCloseDescriptor = -1;
    if (CommandName != NULL) {
        Shell->CommandName = SwStringDuplicate(CommandName, CommandNameSize);
        if (Shell->CommandName == NULL) {
            goto CreateShellEnd;
        }

        Shell->CommandNameSize = CommandNameSize;
    }

    //
    // Initialize the lexer state.
    //

    Result = ShInitializeLexer(&(Shell->Lexer), NULL, NULL, 0);
    if (Result == FALSE) {
        goto CreateShellEnd;
    }

    INITIALIZE_LIST_HEAD(&(Shell->ExecutionStack));
    INITIALIZE_LIST_HEAD(&(Shell->VariableList));
    INITIALIZE_LIST_HEAD(&(Shell->ArgumentList));
    INITIALIZE_LIST_HEAD(&(Shell->FunctionList));
    INITIALIZE_LIST_HEAD(&(Shell->AliasList));
    INITIALIZE_LIST_HEAD(&(Shell->SignalActionList));
    INITIALIZE_LIST_HEAD(&(Shell->ActiveRedirectList));
    Result = ShInitializeVariables(Shell);
    if (Result == FALSE) {
        goto CreateShellEnd;
    }

    //
    // Save the Umask so it can be put back at the end.
    //

    Shell->OriginalUmask = umask(0);
    umask(Shell->OriginalUmask);
    Shell->ProcessId = SwGetProcessId();
    Result = TRUE;

CreateShellEnd:
    if (Result == FALSE) {
        if (Shell != NULL) {
            ShDestroyLexer(&(Shell->Lexer));
            if (Shell->CommandName != NULL) {
                free(Shell->CommandName);
            }

            free(Shell);
            Shell = NULL;
        }
    }

    return Shell;
}

VOID
ShDestroyShell (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine destroys a new shell object.

Arguments:

    Shell - Supplies a pointer to the shell to destroy.

Return Value:

    None.

--*/

{

    PSHELL_HERE_DOCUMENT HereDocument;

    if (Shell->PostForkCloseDescriptor != -1) {
        close(Shell->PostForkCloseDescriptor);
    }

    if (Shell->Prompt != NULL) {
        free(Shell->Prompt);
        Shell->Prompt = NULL;
    }

    ShRestoreRedirections(Shell, &(Shell->ActiveRedirectList));
    while (LIST_EMPTY(&(Shell->Lexer.HereDocumentList)) == FALSE) {
        HereDocument = LIST_VALUE(Shell->Lexer.HereDocumentList.Next,
                                  SHELL_HERE_DOCUMENT,
                                  ListEntry);

        LIST_REMOVE(&(HereDocument->ListEntry));
        ShDestroyHereDocument(HereDocument);
    }

    ShDestroyVariableList(&(Shell->VariableList));
    ShDestroyFunctionList(Shell);
    ShDestroyAliasList(Shell);
    ShDestroySignalActionList(&(Shell->SignalActionList));
    if (Shell->CommandName != NULL) {
        free(Shell->CommandName);
    }

    ShDestroyArgumentList(&(Shell->ArgumentList));
    ShDestroyLexer(&(Shell->Lexer));

    assert(LIST_EMPTY(&(Shell->ExecutionStack)) != FALSE);
    assert(LIST_EMPTY(&(Shell->ArgumentList)) != FALSE);

    umask(Shell->OriginalUmask);
    if (Shell->NonStandardError != NULL) {
        fclose(Shell->NonStandardError);
        Shell->NonStandardError = NULL;
    }

    free(Shell);
    return;
}

PSHELL
ShCreateSubshell (
    PSHELL Shell,
    PSTR Input,
    UINTN InputSize,
    BOOL DequoteForSubshell
    )

/*++

Routine Description:

    This routine creates a subshell based on the given parent shell.

Arguments:

    Shell - Supplies a pointer to the shell to copy.

    Input - Supplies a pointer to the input to feed the subshell with. This
        string will be copied, and potentially dequoted.

    InputSize - Supplies the size of the input string in bytes including the
        null terminator.

    DequoteForSubshell - Supplies a boolean indicating if backslashes that
        follow $, `, or \ should be removed.

Return Value:

    Returns a pointer to the initialized subshell on success.

--*/

{

    PSTR NewInputBuffer;
    INT NonStandardErrorCopy;
    BOOL Result;
    PSHELL Subshell;

    Subshell = ShCreateShell(Shell->CommandName, Shell->CommandNameSize);
    if (Subshell == NULL) {
        return NULL;
    }

    if (Shell->NonStandardError != NULL) {
        NonStandardErrorCopy = ShDup(Shell,
                                     fileno(Shell->NonStandardError),
                                     FALSE);

        if (NonStandardErrorCopy >= 0) {
            Subshell->NonStandardError = fdopen(NonStandardErrorCopy, "w");
            if (Subshell->NonStandardError == NULL) {
                ShClose(Shell, NonStandardErrorCopy);
            }
        }
    }

    Subshell->Options = Shell->Options;
    Subshell->Options &= ~(SHELL_OPTION_PRINT_PROMPTS |
                           SHELL_OPTION_READ_FROM_STDIN);

    Subshell->ProcessId = Shell->ProcessId;
    Result = ShCopyArgumentList(ShGetCurrentArgumentList(Shell),
                                &(Subshell->ArgumentList));

    if (Result == FALSE) {
        goto CreateSubshellEnd;
    }

    Result = ShCopyVariables(Shell, &(Subshell->VariableList));
    if (Result == FALSE) {
        goto CreateSubshellEnd;
    }

    Result = ShCopyFunctionList(Shell, Subshell);
    if (Result == FALSE) {
        goto CreateSubshellEnd;
    }

    Result = ShCopyAliases(Shell, Subshell);
    if (Result == FALSE) {
        goto CreateSubshellEnd;
    }

    if (Input != NULL) {

        assert(InputSize != 0);

        NewInputBuffer = SwStringDuplicate(Input, InputSize);
        if (NewInputBuffer == NULL) {
            Result = FALSE;
            goto CreateSubshellEnd;
        }

        if (Subshell->Lexer.InputBuffer != NULL) {
            free(Subshell->Lexer.InputBuffer);
        }

        if (DequoteForSubshell != FALSE) {
            ShStringDequoteSubshellCommand(NewInputBuffer, &(InputSize));
        }

        Subshell->Lexer.InputBuffer = NewInputBuffer;
        Subshell->Lexer.InputBufferSize = InputSize;
        Subshell->Lexer.InputBufferCapacity = Subshell->Lexer.InputBufferSize;
    }

    Result = TRUE;

CreateSubshellEnd:
    if (Result == FALSE) {
        if (Subshell != NULL) {
            ShDestroyShell(Subshell);
            Subshell = NULL;
        }
    }

    return Subshell;
}

BOOL
ShExecuteSubshell (
    PSHELL ParentShell,
    PSHELL Subshell,
    BOOL Asynchronous,
    PSTR *Output,
    PUINTN OutputSize,
    PINT ReturnValue
    )

/*++

Routine Description:

    This routine executes a subshell.

Arguments:

    ParentShell - Supplies a pointer to the parent shell that's executing this
        subshell.

    Subshell - Supplies a pointer to the subshell to execute.

    Asynchronous - Supplies a boolean indicating whether or not the execution
        should occur in the background.

    Output - Supplies an optional pointer that receives the contents of
        standard output. The caller is responsible for freeing this memory.

    OutputSize - Supplies a pointer where the size of the output in bytes
        will be returned, with no null terminator.

    ReturnValue - Supplies a pointer where the return value of the subshell
        will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    pid_t Child;
    UINTN Index;
    PSTR OriginalDirectory;
    INT OriginalOutput;
    PVOID OutputCollectionHandle;
    unsigned long OutputSizeLong;
    PSTR OutputString;
    INT Pipe[2];
    INT Result;
    INT Status;

    Child = -1;
    OriginalDirectory = NULL;
    OriginalOutput = -1;
    *Output = NULL;
    *OutputSize = 0;
    Pipe[0] = -1;
    Pipe[1] = -1;

    //
    // Create a pipe for reading standard out.
    //

    Result = ShCreatePipe(Pipe);
    if (Result == FALSE) {
        goto ExecuteSubshellEnd;
    }

    //
    // Wire up the write end of the pipe to standard output.
    //

    if (Pipe[1] != STDOUT_FILENO) {
        OriginalOutput = ShDup(ParentShell, STDOUT_FILENO, FALSE);
        if (OriginalOutput < 0) {
            Result = FALSE;
            goto ExecuteSubshellEnd;
        }

        ShDup2(ParentShell, Pipe[1], STDOUT_FILENO);
        ShClose(ParentShell, Pipe[1]);
        Pipe[1] = -1;
    }

    //
    // Get ready to read from the read end of the pipe.
    //

    Result = ShPrepareForOutputCollection(Pipe[0], &OutputCollectionHandle);
    if (Result == FALSE) {
        goto ExecuteSubshellEnd;
    }

    ShInitializeSignals(Subshell);
    if (SwForkSupported != FALSE) {
        Child = SwFork();
        if (Child == -1) {
            SwPrintError(errno, NULL, "Unable to fork");
            Result = FALSE;
            goto ExecuteSubshellEnd;

        //
        // If this is the child, run the command and exit.
        //

        } else if (Child == 0) {

            assert(ParentShell->PostForkCloseDescriptor == -1);

            ShClose(ParentShell, Pipe[0]);
            ShExecute(Subshell, ReturnValue);
            Subshell->Exited = TRUE;
            ShRunAtExitSignal(Subshell);
            exit(*ReturnValue);
        }

    //
    // Fork is not supported, so just run the command in this process
    // (presuming that the prepare for output collection spawned at least
    // another thread.
    //

    } else {
        OriginalDirectory = getcwd(NULL, 0);
        ShSetAllSignalDispositions(Subshell);
        Result = ShExecute(Subshell, ReturnValue);
        Subshell->Exited = TRUE;
        ShRunAtExitSignal(Subshell);
        if (Result == FALSE) {
            *ReturnValue = SHELL_ERROR_OPEN;
            goto ExecuteSubshellEnd;
        }

        ShOsConvertExitStatus(ReturnValue);
    }

    //
    // Restore standard out, now the child is the last process with the write
    // end open.
    //

    if (OriginalOutput >= 0) {
        ShDup2(ParentShell, OriginalOutput, STDOUT_FILENO);
        ShClose(ParentShell, OriginalOutput);
        OriginalOutput = -1;

    } else {

        //
        // The pipe was created at stdin/stdout, so close the write end of the
        // pipe.
        //

        ShClose(ParentShell, Pipe[1]);
        Pipe[1] = -1;
    }

    //
    // Collect the results.
    //

    Result = ShCollectOutput(OutputCollectionHandle, Output, &OutputSizeLong);
    if (Result == FALSE) {
        goto ExecuteSubshellEnd;
    }

    //
    // Strip out any null characters.
    //

    OutputString = *Output;
    Index = 0;
    while (Index < OutputSizeLong) {
        if (OutputString[Index] == '\0') {
            memmove(&(OutputString[Index]),
                    &(OutputString[Index + 1]),
                    OutputSizeLong - (Index + 1));

            OutputSizeLong -= 1;

        } else {
            Index += 1;
        }
    }

    *OutputSize = OutputSizeLong;

    //
    // If fork is supported, wait on the child process.
    //

    if (SwForkSupported != FALSE) {
        Result = SwWaitPid(Child, 0, ReturnValue);
        if (Result == -1) {
            *ReturnValue = SHELL_ERROR_OPEN;
            SwPrintError(errno, NULL, "Failed to wait for pid %d", Child);
            Result = FALSE;
            goto ExecuteSubshellEnd;
        }

        ShOsConvertExitStatus(ReturnValue);
    }

    Result = TRUE;

ExecuteSubshellEnd:
    ShSetAllSignalDispositions(ParentShell);

    //
    // Restore standard out.
    //

    if (OriginalOutput != -1) {
        ShDup2(ParentShell, OriginalOutput, STDOUT_FILENO);
        ShClose(ParentShell, OriginalOutput);
    }

    if (Pipe[0] != -1) {
        ShClose(ParentShell, Pipe[0]);
    }

    if (Pipe[1] != -1) {
        ShClose(ParentShell, Pipe[1]);
    }

    //
    // Restore the current directory.
    //

    if (OriginalDirectory != NULL) {
        Status = chdir(OriginalDirectory);
        if (Status != 0) {
            Result = FALSE;
        }

        free(OriginalDirectory);
    }

    return Result;
}

VOID
ShPrintPrompt (
    PSHELL Shell,
    ULONG PromptNumber
    )

/*++

Routine Description:

    This routine prints a shell prompt.

Arguments:

    Shell - Supplies a pointer to the shell.

    PromptNumber - Supplies the prompt number to print. Valid values are 1, 2,
        and 4 for PS1, PS2, and PS4.

Return Value:

    None. Failure to print the prompt is not considered fatal.

--*/

{

    PSTR ExpandedValue;
    UINTN ExpandedValueSize;
    PSTR Name;
    ULONG NameSize;
    BOOL Result;
    PSTR SpecialExpansions;
    UINTN SpecialExpansionsSize;
    PSTR Value;
    UINTN ValueSize;

    if ((PromptNumber != 4) &&
        (Shell->Options & SHELL_OPTION_PRINT_PROMPTS) == 0) {

        return;
    }

    //
    // Figure out which prompt to print.
    //

    if (PromptNumber == 1) {
        Name = SHELL_PS1;
        NameSize = sizeof(SHELL_PS1);

    } else if (PromptNumber == 2) {
        Name = SHELL_PS2;
        NameSize = sizeof(SHELL_PS2);

    } else {

        assert(PromptNumber == 4);

        Name = SHELL_PS4;
        NameSize = sizeof(SHELL_PS4);
    }

    //
    // Get the variable and expand it.
    //

    Result = ShGetVariable(Shell, Name, NameSize, &Value, &ValueSize);
    if (Result == FALSE) {
        return;
    }

    //
    // Perform special prompt expansions.
    //

    Result = ShExpandPrompt(Shell,
                            Value,
                            ValueSize,
                            &SpecialExpansions,
                            &SpecialExpansionsSize);

    if (Result == FALSE) {
        return;
    }

    //
    // Perform normal variable expansions.
    //

    Result = ShPerformExpansions(Shell,
                                 SpecialExpansions,
                                 SpecialExpansionsSize,
                                 SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT,
                                 &ExpandedValue,
                                 &ExpandedValueSize,
                                 NULL,
                                 NULL);

    free(SpecialExpansions);
    if (Result == FALSE) {
        return;
    }

    ShPrintTrace(Shell, "%s", ExpandedValue);

    //
    // Save the prompt for 1 and 2.
    //

    if ((PromptNumber == 1) || (PromptNumber == 2)) {
        if (Shell->Prompt != NULL) {
            free(Shell->Prompt);
        }

        Shell->Prompt = ExpandedValue;

    } else {
        free(ExpandedValue);
    }

    return;
}

VOID
ShStringDequote (
    PSTR String,
    UINTN StringSize,
    ULONG Options,
    PUINTN NewStringSize
    )

/*++

Routine Description:

    This routine performs an in-place removal of all shell control characters.

Arguments:

    String - Supplies a pointer to the string to de-quote.

    StringSize - Supplies the size of the string in bytes including the null
        terminator. On output this value will be updated to reflect the removal.

    Options - Supplies the bitfield of expansion options.
        See SHELL_DEQUOTE_* definitions.

    NewStringSize - Supplies a pointer where the adjusted size of the string
        will be returned.

Return Value:

    None.

--*/

{

    UINTN Index;

    Index = 0;
    while (Index < StringSize) {

        //
        // Remove quote control characters.
        //

        if (String[Index] == SHELL_CONTROL_QUOTE) {
            memmove(&(String[Index]),
                    &(String[Index + 1]),
                    StringSize - (Index + 1));

            StringSize -= 1;

        } else if (String[Index] == SHELL_CONTROL_ESCAPE) {

            //
            // When dequoting for the purposes of pattern matching, convert
            // to backslashes to escape out metacharacters.
            //

            if ((Options & SHELL_DEQUOTE_FOR_PATTERN_MATCHING) != 0) {
                String[Index] = '\\';
                Index += 1;

            } else {
                memmove(&(String[Index]),
                        &(String[Index + 1]),
                        StringSize - (Index + 1));

                StringSize -= 1;

                //
                // Advance beyond whatever the escaped character is.
                //

                Index += 1;
            }

        } else {
            Index += 1;
        }
    }

    if (NewStringSize != NULL) {
        *NewStringSize = StringSize;
    }

    return;
}

BOOL
ShStringAppend (
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PSTR Component,
    UINTN ComponentSize
    )

/*++

Routine Description:

    This routine adds a string onto the end of another string, separated by a
    space.

Arguments:

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    Component - Supplies a pointer to the component string to append after
        first appending a space to the original string buffer.

    ComponentSize - Supplies the size of the component string in bytes
        including the null terminator.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    PSTR NewBuffer;
    UINTN NewBufferSize;
    UINTN SizeNeeded;

    assert(ComponentSize != 0);

    //
    // If the argument buffer is completely NULL, allocate some space.
    //

    if (*StringBufferAddress == NULL) {
        *StringBufferAddress = malloc(SHELL_INITIAL_ARGUMENTS_SIZE);
        if (*StringBufferAddress == NULL) {
            return FALSE;
        }

        (*StringBufferAddress)[0] = '\0';
        *StringBufferSize = 1;
        *StringBufferCapacity = SHELL_INITIAL_ARGUMENTS_SIZE;
    }

    NewBufferSize = *StringBufferCapacity;

    //
    // Currently they both have null terminators so 1 could be subtracted, but
    // a space is added so that extra is getting used.
    //

    SizeNeeded = *StringBufferSize + ComponentSize;
    while (NewBufferSize < SizeNeeded) {
        NewBufferSize *= 2;
    }

    //
    // If the buffer needed isn't big enough, then reallocate space for it.
    //

    if (NewBufferSize != *StringBufferCapacity) {
        NewBuffer = realloc(*StringBufferAddress, NewBufferSize);
        if (NewBuffer == NULL) {
            *StringBufferSize = 0;
            *StringBufferCapacity = 0;
            return FALSE;
        }

        *StringBufferAddress = NewBuffer;
        *StringBufferCapacity = NewBufferSize;
    }

    assert(*StringBufferSize != 0);

    if (*StringBufferSize != 1) {
        strcpy(*StringBufferAddress + *StringBufferSize - 1, " ");
        *StringBufferSize += 1;
    }

    strcpy(*StringBufferAddress + *StringBufferSize - 1, Component);
    *StringBufferSize += ComponentSize - 1;
    return TRUE;
}

BOOL
ShStringFormatForReentry (
    PSTR String,
    UINTN StringSize,
    PSTR *FormattedString,
    PUINTN FormattedStringSize
    )

/*++

Routine Description:

    This routine creates a formatted version of the given string suitable for
    re-entry into the shell. This is accomplished by surrounding the string in
    single quotes, handling single quotes in the input string specially.

Arguments:

    String - Supplies a pointer to the string to format.

    StringSize - Supplies the size of the input string in bytes including the
        null terminator. If there is no null terminator, one will be added.

    FormattedString - Supplies a pointer where the formatted string will be
        returned on success. The caller will be responsible for freeing this
        memory.

    FormattedStringSize - Supplies a pointer where the formatted string size
        will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINTN AllocationSize;
    UINTN InputIndex;
    PSTR Output;
    UINTN OutputIndex;
    BOOL Result;

    Result = FALSE;
    if (String != NULL) {

        assert(StringSize != 0);

        if (String[StringSize - 1] == '\0') {
            StringSize -= 1;
        }
    }

    //
    // The allocation size will be the size of the original string, plus one
    // for a null terminator, plus two for surrounding single quotes, plus
    // another four for every single quote in the input string. For example,
    // the string a'b will come out 'a'"'"'b'.
    //

    AllocationSize = StringSize + 3;
    for (InputIndex = 0; InputIndex < StringSize; InputIndex += 1) {
        if (String[InputIndex] == '\'') {
            AllocationSize += 4;
        }
    }

    Output = malloc(AllocationSize);
    if (Output == NULL) {
        Result = FALSE;
        goto StringFormatForReentryEnd;
    }

    Output[0] = '\'';
    OutputIndex = 1;
    for (InputIndex = 0; InputIndex < StringSize; InputIndex += 1) {
        if (String[InputIndex] == '\0') {
            break;
        }

        //
        // Convert a ' into '"'"'.
        //

        if (String[InputIndex] == '\'') {
            Output[OutputIndex] = '\'';
            OutputIndex += 1;
            Output[OutputIndex] = '"';
            OutputIndex += 1;
            Output[OutputIndex] = '\'';
            OutputIndex += 1;
            Output[OutputIndex] = '"';
            OutputIndex += 1;
            Output[OutputIndex] = '\'';

        } else {
            Output[OutputIndex] = String[InputIndex];
        }

        OutputIndex += 1;
    }

    Output[OutputIndex] = '\'';
    OutputIndex += 1;
    Output[OutputIndex] = '\0';
    OutputIndex += 1;

    assert(OutputIndex == AllocationSize);

    Result = TRUE;

StringFormatForReentryEnd:
    if (Result == FALSE) {
        if (Output != NULL) {
            free(Output);
            Output = NULL;
        }

        AllocationSize = 0;
    }

    *FormattedString = Output;
    *FormattedStringSize = AllocationSize;
    return Result;
}

BOOL
ShFieldSplit (
    PSHELL Shell,
    PSTR *StringBuffer,
    PUINTN StringBufferSize,
    PLIST_ENTRY ExpansionList,
    ULONG MaxFieldCount,
    PSTR **FieldsArray,
    PULONG FieldsArrayCount
    )

/*++

Routine Description:

    This routine performs field splitting on the given string.

Arguments:

    Shell - Supplies a pointer to the shell instance.

    StringBuffer - Supplies a pointer where the address of the fields string
        buffer is on input. On output, this may contain a different buffer that
        all the fields point into.

    StringBufferSize - Supplies a pointer that contains the size of the fields
        string buffer. This value will be updated to reflect the new size.

    ExpansionList - Supplies a pointer to the list of expansions within this
        string.

    MaxFieldCount - Supplies a maximum number of fields to create. When this
        number is reached, the last field contains the rest of the string.
        Supply 0 to indicate no limit.

    FieldsArray - Supplies a pointer where the array of pointers to the fields
        will be returned. This array will contain a NULL entry at the end of it,
        though that entry will not be included in the field count. The caller
        is responsible for freeing this memory.

    FieldsArrayCount - Supplies a pointer where the number of elements in the
        returned field array will be returned on success. This number does not
        include the null terminator entry.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    CHAR Character;
    UINTN CurrentFieldSize;
    BOOL DeleteField;
    BOOL Delimit;
    PSHELL_EXPANSION_RANGE Expansion;
    PSTR *Field;
    ULONG FieldCapacity;
    ULONG FieldCount;
    ULONG FieldIndex;
    UINTN Index;
    BOOL InEmptyAtExpansion;
    BOOL InsideExpansion;
    PVOID NewBuffer;
    BOOL Quoted;
    BOOL Result;
    UINTN SeparatorCount;
    UINTN SeparatorIndex;
    PSTR Separators;
    BOOL SkipCharacter;
    PSTR String;
    UINTN StringSize;

    String = *StringBuffer;
    StringSize = *StringBufferSize;

    //
    // Allocate an initial array.
    //

    FieldCount = 0;
    FieldIndex = 0;
    FieldCapacity = SHELL_INITIAL_FIELDS_COUNT;
    Field = malloc(FieldCapacity * sizeof(PSTR));
    if (Field == NULL) {
        Result = FALSE;
        goto FieldSplitEnd;
    }

    memset(Field, 0, FieldCapacity * sizeof(PSTR));
    Field[0] = String;

    //
    // Get the field separator variable.
    //

    Result = ShGetVariable(Shell,
                           SHELL_IFS,
                           sizeof(SHELL_IFS),
                           &Separators,
                           &SeparatorCount);

    if (Result == FALSE) {
        Separators = SHELL_IFS_DEFAULT;
        SeparatorCount = sizeof(SHELL_IFS_DEFAULT);
    }

    if (SeparatorCount != 0) {
        SeparatorCount -= 1;
    }

    //
    // Tee up the first expansion.
    //

    Expansion = NULL;
    if (LIST_EMPTY(ExpansionList) == FALSE) {
        Expansion = LIST_VALUE(ExpansionList->Next,
                               SHELL_EXPANSION_RANGE,
                               ListEntry);
    }

    //
    // Loop through every character in the input.
    //

    CurrentFieldSize = 0;
    Delimit = FALSE;
    InEmptyAtExpansion = FALSE;
    InsideExpansion = FALSE;
    SkipCharacter = FALSE;
    Index = 0;
    Quoted = FALSE;
    while (Index < StringSize - 1) {
        Character = String[Index];

        //
        // If at the end of the expansion, move to the next expansion. Being
        // inside an expansion decides whether or not to look for field
        // separators or ordinary whitespace.
        //

        if (Expansion != NULL) {
            if ((Expansion->Type == ShellExpansionSplitOnNull) &&
                (Quoted != FALSE) &&
                (Index >= Expansion->Index)) {

                //
                // If there are no arguments, then an empty at expansion in
                // quotes may collapse to zero arguments.
                //

                if ((Expansion->Length == 0) &&
                    (LIST_EMPTY(ShGetCurrentArgumentList(Shell)) != FALSE)) {

                    InEmptyAtExpansion = TRUE;
                }
            }

            while ((Expansion != NULL) &&
                   (Index == Expansion->Index + Expansion->Length)) {

                InsideExpansion = FALSE;
                if (Expansion->ListEntry.Next != ExpansionList) {
                    Expansion = LIST_VALUE(Expansion->ListEntry.Next,
                                           SHELL_EXPANSION_RANGE,
                                           ListEntry);

                } else {
                    Expansion = NULL;
                }
            }

            //
            // If inside an expansion, look for field separators.
            //

            if ((Expansion != NULL) && (Index >= Expansion->Index)) {
                InsideExpansion = TRUE;
            }
        }

        //
        // If the character is an escape, skip it and the next character.
        //

        if (Character == SHELL_CONTROL_ESCAPE) {
            Index += 2;

            assert(Index <= StringSize - 1);

            CurrentFieldSize += 1;
            continue;

        } else if (Character == SHELL_CONTROL_QUOTE) {
            Quoted = !Quoted;
        }

        //
        // If not inside an expansion, look for quotes or blanks.
        //

        if (InsideExpansion == FALSE) {

            //
            // It's not a quote, and it's not inside an expansion, so look
            // for white space to field split on.
            //

            if ((Quoted == FALSE) && (isspace(Character))) {
                if (CurrentFieldSize != 0) {
                    Delimit = TRUE;

                } else {
                    SkipCharacter = TRUE;
                }
            }

        //
        // This is inside an expansion.
        //

        } else {
            switch (Expansion->Type) {
            case ShellExpansionSplitOnNull:
                if (Character == '\0') {
                    Delimit = TRUE;
                    break;
                }

                //
                // Fall throuth.
                //

            case ShellExpansionFieldSplit:
                if (Quoted != FALSE) {
                    break;
                }

                for (SeparatorIndex = 0;
                     SeparatorIndex < SeparatorCount;
                     SeparatorIndex += 1) {

                    //
                    // Treat carriage returns as equal to newlines. This is
                    // cheating a bit, but the hope is it fixes up CRLFs
                    // seamlessly without causing much other damage.
                    //

                    if ((Character == Separators[SeparatorIndex]) ||
                        ((Separators[SeparatorIndex] == '\n') &&
                         (Character == '\r'))) {

                        Delimit = TRUE;

                        //
                        // Whitespace separators are treated differently than
                        // other characters. Multiple whitespaces in a row are
                        // glossed over.
                        //

                        if ((CurrentFieldSize == 0) &&
                            ((Character == ' ') ||
                             (Character == '\n') || (Character == '\r') ||
                             (Character == '\t'))) {

                            Delimit = FALSE;
                            SkipCharacter = TRUE;
                        }

                        break;
                    }
                }

                break;

            case ShellExpansionNoFieldSplit:
                break;

            default:

                assert(FALSE);

                Result = FALSE;
                goto FieldSplitEnd;
            }
        }

        if (Delimit != FALSE) {
            Delimit = FALSE;
            SkipCharacter = TRUE;

            //
            // Stop if the desired maximum number of fields has been reached.
            //

            if (FieldIndex + 1 == MaxFieldCount) {
                break;
            }

            String[Index] = '\0';
            CurrentFieldSize = 0;

            //
            // Expand the array size if needed. Leave space for an empty field
            // at the end.
            //

            if (FieldIndex + 2 >= FieldCapacity) {
                FieldCapacity *= 2;
                NewBuffer = realloc(Field, FieldCapacity * sizeof(PSTR));
                if (NewBuffer == NULL) {
                    Result = FALSE;
                    goto FieldSplitEnd;
                }

                Field = NewBuffer;
                memset(Field + FieldIndex + 1,
                       0,
                       (FieldCapacity - FieldIndex - 1) * sizeof(PSTR));
            }

            DeleteField = FALSE;
            if (Index + 1 != StringSize) {

                //
                // For $@ expansions, an empty "$@" expands to zero fields, and
                // empty parameters within $@ are not removed.
                //

                if (InEmptyAtExpansion != FALSE) {
                    if (strcmp(Field[FieldIndex], ShEmptyQuotedString) == 0) {
                        DeleteField = TRUE;
                    }

                //
                // Outside of expansions, remove empty fields.
                //

                } else if (InsideExpansion == FALSE) {
                    if (Field[FieldIndex][0] == '\0') {
                        DeleteField = TRUE;
                    }
                }

                if (DeleteField == FALSE) {
                    FieldIndex += 1;
                }

                Field[FieldIndex] = String + Index + 1;
            }

            InEmptyAtExpansion = FALSE;
        }

        //
        // If there were two whitespaces in a row, advance the field to skip
        // over them.
        //

        if (SkipCharacter != FALSE) {
            SkipCharacter = FALSE;

            //
            // If this is the end of the string, the field can't be walked
            // forward anymore. Remove the field.
            //

            if (Index + 2 == StringSize) {

                //
                // If this is the first field it can't be removed. That means
                // the whole string was whitespace. Null out the string and
                // return the 1 empty field.
                //

                if (FieldIndex == 0) {
                    Field[0] = NULL;

                } else {
                    FieldIndex -= 1;
                }

            } else {
                Field[FieldIndex] = String + Index + 1;
            }

        } else {
            CurrentFieldSize += 1;
        }

        Index += 1;
    }

    Result = TRUE;
    FieldCount = FieldIndex;

    //
    // Check the last field.
    //

    DeleteField = FALSE;
    if (Field[FieldIndex] == NULL) {
        DeleteField = TRUE;

    } else if (InEmptyAtExpansion != FALSE) {
        if (strcmp(Field[FieldIndex], ShEmptyQuotedString) == 0) {
            DeleteField = TRUE;
        }

    //
    // Outside of expansions, remove empty fields.
    //

    } else if (InsideExpansion == FALSE) {
        if (Field[FieldIndex][0] == '\0') {
            DeleteField = TRUE;
        }
    }

    if (DeleteField == FALSE) {
        FieldCount += 1;
    }

    //
    // Null terminate the field array.
    //

    assert(FieldCount < FieldCapacity);

    Field[FieldCount] = NULL;

FieldSplitEnd:
    if (Result == FALSE) {
        if (Field != NULL) {
            free(Field);
            Field = NULL;
        }
    }

    *StringBuffer = String;
    *StringBufferSize = StringSize;
    *FieldsArray = Field;
    *FieldsArrayCount = FieldCount;
    return Result;
}

VOID
ShDeNullExpansions (
    PSHELL Shell,
    PSTR String,
    UINTN StringSize
    )

/*++

Routine Description:

    This routine removes the null separators from any expansion range that
    specified to split on nulls. This is called if field splitting didn't end
    up occurring.

Arguments:

    Shell - Supplies a pointer to the shell.

    String - Supplies a pointer to the string to de-null. This buffer will be
        modified.

    StringSize - Supplies the size of the input string in bytes including the
        null terminator.

Return Value:

    None.

--*/

{

    UINTN Index;
    CHAR Separator;
    PSTR Value;
    UINTN ValueSize;

    //
    // Use the first character of IFS if set, or NULL if IFS is unset.
    //

    Separator = ' ';
    Value = NULL;
    ShGetVariable(Shell, SHELL_IFS, sizeof(SHELL_IFS), &Value, &ValueSize);
    if (Value != NULL) {
        Separator = *Value;
    }

    //
    // Don't remove the null terminator.
    //

    if (StringSize != 0) {
        StringSize -= 1;
    }

    for (Index = 0; Index < StringSize; Index += 1) {
        if (String[Index] == '\0') {
            String[Index] = Separator;
        }
    }

    return;
}

VOID
ShPrintTrace (
    PSHELL Shell,
    PSTR Format,
    ...
    )

/*++

Routine Description:

    This routine prints out tracing information to standard error (as it was
    when the process was created). This avoids printing trace information to
    commands that have redirected standard error.

Arguments:

    Shell - Supplies a pointer to the shell.

    Format - Supplies the printf style format string.

    ... - Supplies the remaining arguments as dictated by the format.

Return Value:

    None.

--*/

{

    va_list ArgumentList;

    if ((Shell == NULL) || (Shell->NonStandardError == NULL)) {
        return;
    }

    va_start(ArgumentList, Format);
    vfprintf(Shell->NonStandardError, Format, ArgumentList);
    va_end(ArgumentList);
    fflush(Shell->NonStandardError);
    return;
}

int
ShDup (
    PSHELL Shell,
    int FileDescriptor,
    int Inheritable
    )

/*++

Routine Description:

    This routine duplicates the given file descriptor.

Arguments:

    Shell - Supplies a pointer to the shell.

    FileDescriptor - Supplies the file descriptor to duplicate.

    Inheritable - Supplies a boolean (zero or non-zero) indicating if the new
        handle should be inheritable outside this process.

Return Value:

    Returns the new file descriptor which represents a copy of the original
    file descriptor.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    INT Result;

    Result = ShOsDup(FileDescriptor);
    if (Result < 0) {
        return Result;
    }

    if (Inheritable == 0) {
        if (ShSetDescriptorFlags(Result, 0) != 0) {
            ShClose(Shell, Result);
            return -1;
        }
    }

    return Result;
}

int
ShDup2 (
    PSHELL Shell,
    int FileDescriptor,
    int CopyDescriptor
    )

/*++

Routine Description:

    This routine duplicates the given file descriptor to the destination
    descriptor, closing the original destination descriptor file along the way.

Arguments:

    Shell - Supplies a pointer to the shell.

    FileDescriptor - Supplies the file descriptor to duplicate.

    CopyDescriptor - Supplies the descriptor number of returned copy.

Return Value:

    Returns the new file descriptor which represents a copy of the original,
    which is also equal to the input copy descriptor parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    int Result;

    Result = dup2(FileDescriptor, CopyDescriptor);
    if (Result < 0) {
        return Result;
    }

    return CopyDescriptor;
}

int
ShClose (
    PSHELL Shell,
    int FileDescriptor
    )

/*++

Routine Description:

    This routine closes a file descriptor.

Arguments:

    Shell - Supplies a pointer to the shell.

    FileDescriptor - Supplies the file descriptor to close.

Return Value:

    0 on success.

    -1 if the file could not be closed properly. The state of the file
    descriptor is undefined, but in many cases is still open. The errno
    variable will be set to contain more detailed information.

--*/

{

    return close(FileDescriptor);
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ShStringDequoteSubshellCommand (
    PSTR Input,
    PUINTN InputSize
    )

/*++

Routine Description:

    This routine removes backslashes that are followed by $, `, or \,
    specifically for the inside of a subshell.

Arguments:

    Input - Supplies a pointer to the input string to de-slash.

    InputSize - Supplies a pointer that on input contains the size of the
        input string in bytes including the null terminator. On output, this
        value will be updated to reflect the potentially shrunken size.

Return Value:

    None.

--*/

{

    CHAR Character;
    UINTN Index;
    BOOL WasBackslash;

    Index = 0;
    WasBackslash = FALSE;
    while (Index < *InputSize) {
        Character = Input[Index];

        //
        // If the previous character was a backslash and this character is one
        // of $ ` or \, then remove the precending backslash.
        //

        if ((WasBackslash != FALSE) &&
            ((Character == '$') || (Character == '`') || (Character == '\\'))) {

            Index -= 1;
            SwStringRemoveRegion(Input, InputSize, Index, 1);
            WasBackslash = FALSE;

        } else if (Character == '\\') {
            WasBackslash = TRUE;

        } else {
            WasBackslash = FALSE;
        }

        Index += 1;
    }

    return;
}

