/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    var.c

Abstract:

    This module implements support for environment variables in the shell.

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
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Note that the default PS1 here is overridden for interactive shells.
//

#define SHELL_PS1_ROOT_DEFAULT "# "
#define SHELL_PS1_DEFAULT "$ "
#define SHELL_PS2_DEFAULT "> "
#define SHELL_PS4_DEFAULT "+ (\\L) "
#define SHELL_LINE_NUMBER_DEFAULT "999999999"
#define SHELL_RANDOM_DEFAULT "99999"
#define SHELL_RANDOM_MAX 65535
#define SHELL_OPTION_INDEX_DEFAULT "1"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSHELL_VARIABLE
ShGetVariableInScope (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    PLIST_ENTRY *ListHead
    );

PSHELL_VARIABLE
ShGetVariableInList (
    PLIST_ENTRY ListHead,
    PSTR Name,
    UINTN NameSize,
    ULONG NameHash
    );

BOOL
ShSetVariableInList (
    PLIST_ENTRY ListHead,
    PSTR Name,
    UINTN NameSize,
    PSTR Value,
    UINTN ValueSize,
    BOOL Exported,
    BOOL ReadOnly,
    BOOL Set
    );

PSHELL_VARIABLE
ShCreateVariable (
    PSTR Name,
    UINTN NameSize,
    ULONG NameHash,
    PSTR Value,
    UINTN ValueSize,
    BOOL Exported,
    BOOL ReadOnly,
    BOOL Set
    );

BOOL
ShCopyVariablesOnList (
    PLIST_ENTRY Source,
    PLIST_ENTRY Destination
    );

VOID
ShDestroyVariable (
    PSHELL_VARIABLE Variable,
    BOOL RestoreEnvironment
    );

VOID
ShPrintAllVariables (
    PSHELL Shell,
    BOOL Exported,
    BOOL ReadOnly
    );

VOID
ShPrintVariablesInList (
    PSHELL Shell,
    PLIST_ENTRY ListHead,
    BOOL Exported,
    BOOL ReadOnly
    );

INT
ShBuiltinExportOrReadOnly (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments,
    BOOL Export,
    BOOL ReadOnly
    );

ULONG
ShHashName (
    PSTR Name,
    UINTN NameSize
    );

//
// -------------------------------------------------------------------- Globals
//

extern char **environ;

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShInitializeVariables (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine performs some variable initialization in the shell, as well as
    handling the ENV variable.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    struct stat DotStat;
    UINTN EnvironmentIndex;
    PSTR EnvironmentVariable;
    PSTR Equals;
    PSTR Name;
    ULONG NameHash;
    UINTN NameIndex;
    UINTN NameSize;
    unsigned long PathSize;
    struct stat PwdStat;
    BOOL Result;
    PSTR Value;
    UINTN ValueSize;
    PSHELL_VARIABLE Variable;

    //
    // Set up the exported environment variables.
    //

    EnvironmentIndex = 0;
    while (environ[EnvironmentIndex] != NULL) {
        EnvironmentVariable = environ[EnvironmentIndex];
        EnvironmentIndex += 1;
        Equals = strchr(EnvironmentVariable, '=');
        if ((Equals == NULL) || (Equals == EnvironmentVariable)) {
            continue;
        }

        NameSize = (UINTN)Equals - (UINTN)EnvironmentVariable + 1;
        Name = SwStringDuplicate(EnvironmentVariable, NameSize);
        if (Name == NULL) {
            Result = FALSE;
            goto InitializeVariablesEnd;
        }

        //
        // Change any illegal characters to underscores.
        //

        for (NameIndex = 0; NameIndex < NameSize; NameIndex += 1) {
            if (NameIndex == 0) {
                if (!SHELL_NAME_FIRST_CHARACTER(Name[NameIndex])) {
                    Name[NameIndex] = '_';
                }

            } else if (!SHELL_NAME_CHARACTER(Name[NameIndex])) {
                Name[NameIndex] = '_';
            }
        }

        //
        // Skip variables that shouldn't be initialized from the environment.
        //

        if ((NameSize == 4) && (strncmp(Name, SHELL_IFS, NameSize - 1) == 0)) {
            free(Name);
            continue;
        }

        Value = Equals + 1;
        ValueSize = strlen(Value) + 1;
        NameHash = ShHashName(Name, NameSize);

        //
        // If there are duplicate variables in the environment, use the latest.
        //

        Variable = ShGetVariableInList(&(Shell->VariableList),
                                       Name,
                                       NameSize,
                                       NameHash);

        if (Variable != NULL) {
            LIST_REMOVE(&(Variable->ListEntry));
            Variable->ListEntry.Next = NULL;
            ShDestroyVariable(Variable, FALSE);
        }

        //
        // Create the variable manually to avoid setting the variable in the
        // environment again, which might cause environ to get reallocated.
        // This would be bad as this function may have cached the old value.
        //

        Variable = ShCreateVariable(Name,
                                    NameSize,
                                    NameHash,
                                    Value,
                                    ValueSize,
                                    TRUE,
                                    FALSE,
                                    TRUE);

        if (Variable != NULL) {
            INSERT_BEFORE(&(Variable->ListEntry), &(Shell->VariableList));
        }

        free(Name);
    }

    //
    // Set up the PWD variable if it's not already set or seems to mismatch
    // with ".". Distrust if the file serial number is zero, as Windows for
    // instance returns that for everything.
    //

    Result = ShGetVariable(Shell,
                           SHELL_PWD,
                           sizeof(SHELL_PWD),
                           &Value,
                           &ValueSize);

    if ((Result == FALSE) ||
        (SwStat(Value, TRUE, &PwdStat) != 0) ||
        (SwStat(".", TRUE, &DotStat) != 0) ||
        (DotStat.st_dev != PwdStat.st_dev) ||
        (DotStat.st_ino != PwdStat.st_ino) ||
        (PwdStat.st_ino == 0)) {

        Result = ShGetCurrentDirectory(&Value, &ValueSize);
        if (Result != FALSE) {
            ShSetVariableWithProperties(Shell,
                                        SHELL_PWD,
                                        sizeof(SHELL_PWD),
                                        Value,
                                        ValueSize,
                                        TRUE,
                                        FALSE,
                                        TRUE);

            free(Value);
        }
    }

    //
    // On Windows, convert Path to PATH, and fix it up.
    //

    if (SwForkSupported == 0) {
        Result = ShGetVariable(Shell, "Path", 5, &Value, &ValueSize);
        if (Result != FALSE) {
            Value = SwStringDuplicate(Value, ValueSize);
            if (Value != NULL) {
                PathSize = ValueSize;
                if (ShFixUpPath(&Value, &PathSize) != 0) {
                    ValueSize = PathSize;
                    ShSetVariableWithProperties(Shell,
                                                SHELL_PATH,
                                                sizeof(SHELL_PATH),
                                                Value,
                                                ValueSize,
                                                TRUE,
                                                FALSE,
                                                TRUE);

                    free(Value);
                }
            }
        }
    }

    //
    // Set the default PS1, PS2, and PS4 if not already set.
    //

    Result = ShGetVariable(Shell,
                           SHELL_PS1,
                           sizeof(SHELL_PS1),
                           &Value,
                           &ValueSize);

    if (Result == FALSE) {
        if (SwGetEffectiveUserId() == 0) {
            Value = SHELL_PS1_ROOT_DEFAULT;
            ValueSize = sizeof(SHELL_PS1_ROOT_DEFAULT);

        } else {
            Value = SHELL_PS1_DEFAULT;
            ValueSize = sizeof(SHELL_PS1_DEFAULT);
        }

        Result = ShSetVariable(Shell,
                               SHELL_PS1,
                               sizeof(SHELL_PS1),
                               Value,
                               ValueSize);

        if (Result == FALSE) {
            goto InitializeVariablesEnd;
        }
    }

    Result = ShGetVariable(Shell,
                           SHELL_PS2,
                           sizeof(SHELL_PS2),
                           &Value,
                           &ValueSize);

    if (Result == FALSE) {
        Result = ShSetVariable(Shell,
                               SHELL_PS2,
                               sizeof(SHELL_PS2),
                               SHELL_PS2_DEFAULT,
                               sizeof(SHELL_PS2_DEFAULT));

        if (Result == FALSE) {
            goto InitializeVariablesEnd;
        }
    }

    Result = ShGetVariable(Shell,
                           SHELL_PS4,
                           sizeof(SHELL_PS4),
                           &Value,
                           &ValueSize);

    if (Result == FALSE) {
        Result = ShSetVariable(Shell,
                               SHELL_PS4,
                               sizeof(SHELL_PS4),
                               SHELL_PS4_DEFAULT,
                               sizeof(SHELL_PS4_DEFAULT));

        if (Result == FALSE) {
            goto InitializeVariablesEnd;
        }
    }

    Result = ShSetVariable(Shell,
                           SHELL_IFS,
                           sizeof(SHELL_IFS),
                           SHELL_IFS_DEFAULT,
                           sizeof(SHELL_IFS_DEFAULT));

    if (Result == FALSE) {
        goto InitializeVariablesEnd;
    }

    //
    // Set up a line number variable and a random variable..
    //

    Result = ShSetVariable(Shell,
                           SHELL_LINE_NUMBER,
                           sizeof(SHELL_LINE_NUMBER),
                           SHELL_LINE_NUMBER_DEFAULT,
                           sizeof(SHELL_LINE_NUMBER_DEFAULT));

    if (Result == FALSE) {
        goto InitializeVariablesEnd;
    }

    Result = ShSetVariable(Shell,
                           SHELL_RANDOM,
                           sizeof(SHELL_RANDOM),
                           SHELL_RANDOM_DEFAULT,
                           sizeof(SHELL_RANDOM_DEFAULT));

    if (Result == FALSE) {
        goto InitializeVariablesEnd;
    }

    Result = ShSetVariable(Shell,
                           SHELL_OPTION_INDEX,
                           sizeof(SHELL_OPTION_INDEX),
                           SHELL_OPTION_INDEX_DEFAULT,
                           sizeof(SHELL_OPTION_INDEX_DEFAULT));

    if (Result == FALSE) {
        goto InitializeVariablesEnd;
    }

InitializeVariablesEnd:
    return Result;
}

BOOL
ShGetVariable (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    PSTR *Value,
    PUINTN ValueSize
    )

/*++

Routine Description:

    This routine gets the value of the given environment variable.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to get.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Value - Supplies a pointer where a pointer to the value string of the
        variable will be returned on success. The caller does not own this
        memory and should not edit or free it.

    ValueSize - Supplies an optional pointer where the the size of the value
        string in bytes including the null terminator will be returned.

Return Value:

    TRUE if the variable is set (null or not).

    FALSE if the variable is unset.

--*/

{

    INT Difference;
    PSHELL_VARIABLE Variable;
    UINTN VariableValueSize;

    Variable = ShGetVariableInScope(Shell, Name, NameSize, NULL);
    VariableValueSize = 0;
    if (Variable != NULL) {
        VariableValueSize = Variable->ValueSize;
        if (Variable->Set == FALSE) {
            return FALSE;
        }
    }

    //
    // If this is the special line number variable, change the value.
    //

    Difference = strncmp(Name,
                         SHELL_LINE_NUMBER,
                         sizeof(SHELL_LINE_NUMBER) - 1);

    if ((Difference == 0) &&
        (Variable != NULL) && (Variable->Value != NULL) &&
        (Variable->ValueSize == sizeof(SHELL_LINE_NUMBER_DEFAULT))) {

        VariableValueSize = snprintf(Variable->Value,
                                     Variable->ValueSize,
                                     "%d",
                                     Shell->ExecutingLineNumber);

        VariableValueSize += 1;

    //
    // Also change the value if this is the special random variable.
    //

    } else {
        Difference = strncmp(Name, SHELL_RANDOM, sizeof(SHELL_RANDOM) - 1);
        if ((Difference == 0) &&
            (Variable != NULL) && (Variable->Value != NULL) &&
            (Variable->ValueSize == sizeof(SHELL_RANDOM_DEFAULT))) {

            VariableValueSize = snprintf(Variable->Value,
                                         Variable->ValueSize,
                                         "%d",
                                         rand() % SHELL_RANDOM_MAX);

            VariableValueSize += 1;
        }
    }

    if (ValueSize != NULL) {
        *ValueSize = VariableValueSize;
    }

    if (Value != NULL) {
        if (Variable != NULL) {
            *Value = Variable->Value;

        } else {
            *Value = NULL;
        }
    }

    if (Variable != NULL) {
        return TRUE;
    }

    return FALSE;
}

BOOL
ShSetVariable (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    PSTR Value,
    UINTN ValueSize
    )

/*++

Routine Description:

    This routine sets a shell variable in the proper scope.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to set.
        A copy of this string will be made.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Value - Supplies the value string of the variable to set. This may get
        expanded. A copy of this string will be made.

    ValueSize - Supplies the size of the value string in bytes, including the
        null terminator.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PLIST_ENTRY ListHead;
    BOOL Result;
    PSHELL_VARIABLE Variable;

    Variable = ShGetVariableInScope(Shell, Name, NameSize, &ListHead);
    if (Variable == NULL) {
        ListHead = &(Shell->VariableList);
    }

    Result = ShSetVariableInList(ListHead,
                                 Name,
                                 NameSize,
                                 Value,
                                 ValueSize,
                                 FALSE,
                                 FALSE,
                                 TRUE);

    return Result;
}

BOOL
ShSetVariableWithProperties (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    PSTR Value,
    UINTN ValueSize,
    BOOL Exported,
    BOOL ReadOnly,
    BOOL Set
    )

/*++

Routine Description:

    This routine sets a shell variable in the proper scope.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to set.
        A copy of this string will be made.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Value - Supplies the value string of the variable to set. This may get
        expanded. A copy of this string will be made.

    ValueSize - Supplies the size of the value string in bytes, including the
        null terminator.

    Exported - Supplies a boolean indicating if the variable should be marked
        for export.

    ReadOnly - Supplies a boolean indicating if the variable should be marked
        read-only.

    Set - Supplies a boolean indicating if the variable should be set or not.
        If this value is FALSE, the value parameter will be ignored.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PLIST_ENTRY ListHead;
    BOOL Result;
    PSHELL_VARIABLE Variable;

    Variable = ShGetVariableInScope(Shell, Name, NameSize, &ListHead);
    if (Variable == NULL) {
        ListHead = &(Shell->VariableList);
    }

    Result = ShSetVariableInList(ListHead,
                                 Name,
                                 NameSize,
                                 Value,
                                 ValueSize,
                                 Exported,
                                 ReadOnly,
                                 Set);

    return Result;
}

BOOL
ShUnsetVariableOrFunction (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    SHELL_UNSET_TYPE Type
    )

/*++

Routine Description:

    This routine unsets an environment variable or function.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to
        unset.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Type - Supplies the type of unset to perform: either the default behavior
        to try to unset a variable and then a function, or to just try to unset
        either a variable or a function.

Return Value:

    TRUE if the variable was successfully unset or was not previously set.

    FALSE if the variable is read-only and cannot be unset.

--*/

{

    PSHELL_FUNCTION ShellFunction;
    PSHELL_VARIABLE Variable;

    if ((Type == ShellUnsetDefault) || (Type == ShellUnsetVariable)) {
        Variable = ShGetVariableInScope(Shell, Name, NameSize, NULL);
        if (Variable != NULL) {
            if (Variable->ReadOnly != FALSE) {
                PRINT_ERROR("Variable %s is read only.\n", Variable->Name);
                return FALSE;
            }

            //
            // If the variable is exported, unset it in the environment too.
            //

            if (Variable->Exported != FALSE) {
                ShUnsetEnvironmentVariable(Variable->Name);
            }

            //
            // The variable is neither unset nor read-only, destroy it, and
            // don't put back any original environment variable.
            //

            LIST_REMOVE(&(Variable->ListEntry));
            Variable->ListEntry.Next = NULL;
            ShDestroyVariable(Variable, FALSE);
            return TRUE;
        }

        //
        // Fall through to try and unset a function if allowed.
        //

    }

    if ((Type == ShellUnsetDefault) || (Type == ShellUnsetFunction)) {
        ShellFunction = ShGetFunction(Shell, Name, NameSize);
        if (ShellFunction != NULL) {
            LIST_REMOVE(&(ShellFunction->ListEntry));
            ShReleaseNode(ShellFunction->Node);
            free(ShellFunction);
            return TRUE;
        }
    }

    //
    // It's not a failure to unset a variable or function that was not
    // previously set.
    //

    return TRUE;
}

BOOL
ShExecuteVariableAssignments (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    )

/*++

Routine Description:

    This routine performs any variable assignments in the given node.

Arguments:

    Shell - Supplies a pointer to the shell.

    ExecutionNode - Supplies a pointer to the node containing the assignments.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_ASSIGNMENT Assignment;
    PLIST_ENTRY CurrentEntry;
    PSTR ExpandedValue;
    UINTN ExpandedValueSize;
    PSHELL_NODE Node;
    BOOL Result;
    BOOL SetInShell;
    PSHELL_NODE_SIMPLE_COMMAND SimpleCommand;

    ExpandedValue = NULL;
    Node = ExecutionNode->Node;

    assert(Node->Type == ShellNodeSimpleCommand);

    SimpleCommand = &(Node->U.SimpleCommand);

    //
    // Shortcut the usual case where there are no assignments.
    //

    if (LIST_EMPTY(&(SimpleCommand->AssignmentList)) != FALSE) {
        return TRUE;
    }

    //
    // If the command is null then assignments go directly to this shell,
    // otherwise they go to the node only.
    //

    SetInShell = FALSE;
    if ((SimpleCommand->Arguments == NULL) ||
        (SimpleCommand->ArgumentsSize <= 1)) {

        SetInShell = TRUE;
    }

    CurrentEntry = SimpleCommand->AssignmentList.Next;
    while (CurrentEntry != &(SimpleCommand->AssignmentList)) {
        Assignment = LIST_VALUE(CurrentEntry, SHELL_ASSIGNMENT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Result = ShPerformExpansions(Shell,
                                     Assignment->Value,
                                     Assignment->ValueSize,
                                     SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT,
                                     &ExpandedValue,
                                     &ExpandedValueSize,
                                     NULL,
                                     NULL);

        if (Result == FALSE) {
            goto ExecuteVariableAssignmentsEnd;
        }

        if ((Shell->Options & SHELL_OPTION_TRACE_COMMAND) != 0) {
            ShPrintTrace(Shell, "%s=%s ", Assignment->Name, ExpandedValue);
        }

        if (SetInShell != FALSE) {
            Result = ShSetVariable(Shell,
                                   Assignment->Name,
                                   Assignment->NameSize,
                                   ExpandedValue,
                                   ExpandedValueSize);

        } else {

            //
            // Variables set for the duration of a command are exported to that
            // command.
            //

            Result = ShSetVariableInList(&(ExecutionNode->VariableList),
                                         Assignment->Name,
                                         Assignment->NameSize,
                                         ExpandedValue,
                                         ExpandedValueSize,
                                         TRUE,
                                         FALSE,
                                         TRUE);
        }

        if (Result == FALSE) {
            goto ExecuteVariableAssignmentsEnd;
        }

        free(ExpandedValue);
        ExpandedValue = NULL;
    }

    Result = TRUE;

ExecuteVariableAssignmentsEnd:
    if (ExpandedValue != NULL) {
        free(ExpandedValue);
    }

    return Result;
}

BOOL
ShCopyVariables (
    PSHELL Source,
    PLIST_ENTRY DestinationList
    )

/*++

Routine Description:

    This routine copies all the variables visible in the current shell over to
    the new shell.

Arguments:

    Source - Supplies a pointer to the shell containing the variables to copy.

    DestinationList - Supplies a pointer to the head of the list where the
        copies will be put.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSHELL_EXECUTION_NODE ExecutionNode;
    BOOL Result;

    //
    // Copy the variables set in the shell first.
    //

    Result = ShCopyVariablesOnList(&(Source->VariableList), DestinationList);
    if (Result == FALSE) {
        return FALSE;
    }

    //
    // Loop through every node on the stack and add those assignments. Do it
    // backwards so the ones at the front of the list (most recent) override
    // the ones below.
    //

    CurrentEntry = Source->ExecutionStack.Previous;
    while (CurrentEntry != &(Source->ExecutionStack)) {
        ExecutionNode = LIST_VALUE(CurrentEntry,
                                   SHELL_EXECUTION_NODE,
                                   ListEntry);

        CurrentEntry = CurrentEntry->Previous;
        Result = ShCopyVariablesOnList(&(ExecutionNode->VariableList),
                                       DestinationList);

        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID
ShDestroyVariableList (
    PLIST_ENTRY List
    )

/*++

Routine Description:

    This routine destroys an environment variable list.

Arguments:

    List - Supplies a pointer to the list to destroy.

Return Value:

    None.

--*/

{

    PSHELL_VARIABLE Variable;

    while (LIST_EMPTY(List) == FALSE) {
        Variable = LIST_VALUE(List->Next, SHELL_VARIABLE, ListEntry);
        LIST_REMOVE(&(Variable->ListEntry));
        Variable->ListEntry.Next = NULL;

        //
        // If the variable is exported and had an original value, then restore
        // the original value.
        //

        ShDestroyVariable(Variable, TRUE);
    }

    return;
}

PSHELL_FUNCTION
ShGetFunction (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize
    )

/*++

Routine Description:

    This routine returns a pointer to the function information for a function
    of the given name.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to get.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

Return Value:

    Returns a pointer to the variable on success.

    NULL if the variable could not be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSHELL_FUNCTION Function;
    PSTR FunctionName;

    assert(NameSize > 1);

    //
    // Look through each function and try to match.
    //

    CurrentEntry = Shell->FunctionList.Next;
    while (CurrentEntry != &(Shell->FunctionList)) {
        Function = LIST_VALUE(CurrentEntry, SHELL_FUNCTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FunctionName = Function->Node->U.Function.Name;
        if ((strncmp(Name, FunctionName, NameSize - 1) == 0) &&
            (FunctionName[NameSize - 1] == '\0')) {

            return Function;
        }
    }

    return NULL;
}

BOOL
ShDeclareFunction (
    PSHELL Shell,
    PSHELL_NODE Function
    )

/*++

Routine Description:

    This routine sets a function declaration in the given shell.

Arguments:

    Shell - Supplies an optional pointer to the shell.

    Function - Supplies a pointer to the function to set.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_FUNCTION NewFunction;
    BOOL Result;

    //
    // Look to see if the function is already set in the list.
    //

    NewFunction = ShGetFunction(Shell,
                                Function->U.Function.Name,
                                Function->U.Function.NameSize);

    if (NewFunction != NULL) {
        if (NewFunction->Node != NULL) {
            ShReleaseNode(NewFunction->Node);
        }

        NewFunction->Node = Function;
        ShRetainNode(Function);
        Result = TRUE;
        goto DeclareFunctionEnd;
    }

    //
    // The variable doesn't exist, at least not on this list. Create it.
    //

    NewFunction = malloc(sizeof(SHELL_FUNCTION));
    if (NewFunction == NULL) {
        Result = FALSE;
        goto DeclareFunctionEnd;
    }

    NewFunction->Node = Function;
    ShRetainNode(Function);
    INSERT_BEFORE(&(NewFunction->ListEntry), &(Shell->FunctionList));
    Result = TRUE;

DeclareFunctionEnd:
    if (Result == FALSE) {
        if (NewFunction != NULL) {
            free(NewFunction);
        }
    }

    return Result;
}

BOOL
ShCopyFunctionList (
    PSHELL Source,
    PSHELL Destination
    )

/*++

Routine Description:

    This routine copies the list of declared functions from one shell to
    another.

Arguments:

    Source - Supplies a pointer to the shell containing the function
        definitions.

    Destination - Supplies a pointer to the shell where the function
        definitions should be copied to.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSHELL_FUNCTION Function;
    BOOL Result;

    CurrentEntry = Source->FunctionList.Next;
    while (CurrentEntry != &(Source->FunctionList)) {
        Function = LIST_VALUE(CurrentEntry, SHELL_FUNCTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Result = ShDeclareFunction(Destination, Function->Node);
        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID
ShDestroyFunctionList (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine cleans up the list of functions on the given shell.

Arguments:

    Shell - Supplies a pointer to the dying shell.

Return Value:

    None.

--*/

{

    PSHELL_FUNCTION Function;

    while (LIST_EMPTY(&(Shell->FunctionList)) == FALSE) {
        Function = LIST_VALUE(Shell->FunctionList.Next,
                              SHELL_FUNCTION,
                              ListEntry);

        LIST_REMOVE(&(Function->ListEntry));
        ShReleaseNode(Function->Node);
        free(Function);
    }

    return;
}

BOOL
ShCreateArgumentList (
    PSTR *Arguments,
    ULONG ArgumentCount,
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine creates an argument list based on the command arguments.

Arguments:

    Arguments - Supplies a pointer to an array of strings representing the
        arguments. Even the first value in this array is supplied as an
        argument, so adjust this variable if passing parameters directly from
        the command line as the first parameter is usually the command name,
        not an argument.

    ArgumentCount - Supplies the number of arguments in the argument list.

    ListHead - Supplies a pointer to the initialized list head where the
        arguments should be placed.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG ArgumentIndex;
    PSTR ArgumentString;
    UINTN NameLength;
    PSHELL_ARGUMENT NewArgument;
    BOOL Result;

    ShDestroyArgumentList(ListHead);
    NewArgument = NULL;
    Result = FALSE;
    for (ArgumentIndex = 0; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        ArgumentString = Arguments[ArgumentIndex];
        NameLength = strlen(ArgumentString) + 1;
        NewArgument = malloc(sizeof(SHELL_ARGUMENT));
        if (NewArgument == NULL) {
            goto CreateArgumentListEnd;
        }

        NewArgument->Name = SwStringDuplicate(ArgumentString, NameLength);
        if (NewArgument->Name == NULL) {
            goto CreateArgumentListEnd;
        }

        NewArgument->NameSize = NameLength;
        INSERT_BEFORE(&(NewArgument->ListEntry), ListHead);
        NewArgument = NULL;
    }

    Result = TRUE;

CreateArgumentListEnd:
    if (Result == FALSE) {
        if (NewArgument != NULL) {
            if (NewArgument->Name != NULL) {
                free(NewArgument->Name);
            }

            free(NewArgument);
        }

        ShDestroyArgumentList(ListHead);
    }

    return Result;
}

BOOL
ShCopyArgumentList (
    PLIST_ENTRY SourceList,
    PLIST_ENTRY DestinationList
    )

/*++

Routine Description:

    This routine copies an existing argument list to a new one.

Arguments:

    SourceList - Supplies a pointer to the argument list to copy.

    DestinationList - Supplies a pointer where the copied entries will be
        added.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_ARGUMENT Argument;
    PLIST_ENTRY CurrentEntry;
    PSHELL_ARGUMENT NewArgument;
    BOOL Result;

    ShDestroyArgumentList(DestinationList);
    Result = FALSE;
    CurrentEntry = SourceList->Next;
    while (CurrentEntry != SourceList) {
        Argument = LIST_VALUE(CurrentEntry, SHELL_ARGUMENT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        NewArgument = malloc(sizeof(SHELL_ARGUMENT));
        if (NewArgument == NULL) {
            goto CopyArgumentListEnd;
        }

        NewArgument->Name = SwStringDuplicate(Argument->Name,
                                              Argument->NameSize);

        if (NewArgument->Name == NULL) {
            goto CopyArgumentListEnd;
        }

        NewArgument->NameSize = Argument->NameSize;
        INSERT_BEFORE(&(NewArgument->ListEntry), DestinationList);
    }

    Result = TRUE;

CopyArgumentListEnd:
    if (Result == FALSE) {
        ShDestroyArgumentList(DestinationList);
    }

    return Result;
}

VOID
ShDestroyArgumentList (
    PLIST_ENTRY ListHead
    )

/*++

Routine Description:

    This routine destroys an argument list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of arguments.

Return Value:

    None.

--*/

{

    PSHELL_ARGUMENT Argument;

    while (LIST_EMPTY(ListHead) == FALSE) {
        Argument = LIST_VALUE(ListHead->Next, SHELL_ARGUMENT, ListEntry);
        LIST_REMOVE(&(Argument->ListEntry));
        if (Argument->Name != NULL) {
            free(Argument->Name);
        }

        free(Argument);
    }

    return;
}

INT
ShBuiltinSet (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin set command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns 0 if the variables were unset.

    Returns greater than zero if one or more variables could not be unset.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    PLIST_ENTRY ArgumentList;
    UINTN ArgumentSize;
    BOOL GotDoubleDash;
    BOOL Result;
    BOOL Set;

    //
    // With no arguments, set just prints all the variables and exits.
    //

    if (ArgumentCount == 1) {
        ShPrintAllVariables(Shell, FALSE, FALSE);
        return 0;
    }

    GotDoubleDash = FALSE;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];

        //
        // First look out for the longform options.
        //

        if ((strcmp(Argument, "-o") == 0) || (strcmp(Argument, "+o") == 0)) {
            Set = TRUE;
            if (Argument[0] == '+') {
                Set = FALSE;
            }

            if (ArgumentIndex == ArgumentCount - 1) {
                Argument = "";
                ArgumentSize = 1;

            } else {
                ArgumentIndex += 1;
                Argument = Arguments[ArgumentIndex];
                ArgumentSize = strlen(Arguments[ArgumentIndex + 1]) + 1;
            }

            Result = ShSetOptions(Shell,
                                  Argument,
                                  ArgumentSize,
                                  TRUE,
                                  Set,
                                  NULL);

            if (Result == FALSE) {
                PRINT_ERROR("Error: Unknown option %s.\n",
                            Arguments[ArgumentIndex + 1]);

                return EINVAL;
            }

            continue;
        }

        //
        // Stop processing for --.
        //

        if (strcmp(Argument, "--") == 0) {
            GotDoubleDash = TRUE;
            ArgumentIndex += 1;
            break;
        }

        if ((Argument[0] == '-') || (Argument[0] == '+')) {
            Result = ShSetOptions(Shell,
                                  Argument,
                                  strlen(Argument) + 1,
                                  FALSE,
                                  FALSE,
                                  NULL);

            if (Result == FALSE) {
                return EINVAL;
            }

        //
        // This is a positional argument.
        //

        } else {
            break;
        }
    }

    //
    // If this isn't the last argument or the double dash was specified, reset
    // the positional arguments.
    //

    if ((ArgumentIndex != ArgumentCount) || (GotDoubleDash != FALSE)) {
        ArgumentList = ShGetCurrentArgumentList(Shell);
        Result = ShCreateArgumentList(Arguments + ArgumentIndex,
                                      ArgumentCount - ArgumentIndex,
                                      ArgumentList);

        if (Result == FALSE) {
            return ENOMEM;
        }
    }

    return 0;
}

INT
ShBuiltinUnset (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin unset command for unsetting variables
    or functions.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    Returns 0 if the variables were unset.

    Returns greater than zero if one or more variables could not be unset.

--*/

{

    PSTR Argument;
    INT  ArgumentIndex;
    BOOL ProcessOptions;
    BOOL Result;
    ULONG ReturnValue;
    SHELL_UNSET_TYPE UnsetType;

    ProcessOptions = TRUE;
    ReturnValue = 0;
    UnsetType = ShellUnsetDefault;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];

        //
        // Stop processing options immediately if the first character is not a
        // dash.
        //

        if (*Argument != '-') {
            ProcessOptions = FALSE;
        }

        //
        // If option processing is over, then this must be a variable or
        // function.
        //

        if (ProcessOptions == FALSE) {
            Result = ShUnsetVariableOrFunction(Shell,
                                               Argument,
                                               strlen(Argument) + 1,
                                               UnsetType);

            if (Result == FALSE) {
                ReturnValue += 1;
            }

            continue;
        }

        //
        // Skip to the next argument and stop processing options if the double
        // dash is reached.
        //

        if (strcmp(Argument, "--") == 0) {
            ProcessOptions = FALSE;
            continue;
        }

        Argument += 1;
        while (*Argument != '\0') {
            switch (*Argument) {
            case 'v':
                if (UnsetType == ShellUnsetFunction) {
                    fprintf(stderr,
                            "unset: cannot unset a function and a variable\n");

                    ReturnValue = 1;
                    goto BuiltinUnsetEnd;
                }

                UnsetType = ShellUnsetVariable;
                break;

            case 'f':
                if (UnsetType == ShellUnsetVariable) {
                    fprintf(stderr,
                            "unset: cannot unset a function and a variable\n");

                    ReturnValue = 1;
                    goto BuiltinUnsetEnd;
                }

                UnsetType = ShellUnsetFunction;
                break;

            default:
                fprintf(stderr, "unset: invalid option -%c\n", *Argument);
                fprintf(stderr, "usage: unset [-f] [-v] [name...]\n");
                ReturnValue = 2;
                goto BuiltinUnsetEnd;
            }

            Argument += 1;
        }
    }

BuiltinUnsetEnd:
    return ReturnValue;
}

INT
ShBuiltinExport (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin export command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns 0 if the variables were exported.

    Returns greater than zero if one or more variables could not be exported.

--*/

{

    INT ReturnValue;

    ReturnValue = ShBuiltinExportOrReadOnly(Shell,
                                            ArgumentCount,
                                            Arguments,
                                            TRUE,
                                            FALSE);

    return ReturnValue;
}

INT
ShBuiltinReadOnly (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin readonly command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns 0 if the variables were made read-only.

    Returns greater than zero if one or more variables could not be made
    read-only.

--*/

{

    INT ReturnValue;

    ReturnValue = ShBuiltinExportOrReadOnly(Shell,
                                            ArgumentCount,
                                            Arguments,
                                            FALSE,
                                            TRUE);

    return ReturnValue;
}

INT
ShBuiltinLocal (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine implements the builtin local command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns 0 if the variables were made local.

    Returns greater than zero if one or more variables could not be made local.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    PLIST_ENTRY CurrentEntry;
    PSTR Equals;
    PSHELL_EXECUTION_NODE ExecutionNode;
    PSHELL_VARIABLE ExistingVariable;
    BOOL Exported;
    PLIST_ENTRY ListHead;
    UINTN NameSize;
    BOOL ReadOnly;
    BOOL Result;
    INT ReturnValue;
    BOOL Set;
    PSTR Value;
    UINTN ValueSize;

    //
    // Get the currently executing function.
    //

    CurrentEntry = Shell->ExecutionStack.Next;
    while (CurrentEntry != &(Shell->ExecutionStack)) {
        ExecutionNode = LIST_VALUE(CurrentEntry,
                                   SHELL_EXECUTION_NODE,
                                   ListEntry);

        if (ExecutionNode->Node->Type == ShellNodeFunction) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &(Shell->ExecutionStack)) {
        PRINT_ERROR("local: Not called from within a function.\n");
        ReturnValue = 1;
        goto BuiltinLocalEnd;
    }

    //
    // Loop through and parse all the variables.
    //

    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        Value = NULL;
        ValueSize = 0;
        Equals = strchr(Argument, '=');
        if (Equals != NULL) {
            *Equals = '\0';
            Value = Equals + 1;
            ValueSize = strlen(Value) + 1;
        }

        NameSize = strlen(Argument);
        if ((Value == NULL) && (strcmp(Argument, "-") == 0)) {
            if ((ExecutionNode->Flags & SHELL_EXECUTION_RESTORE_OPTIONS) == 0) {
                ExecutionNode->Flags |= SHELL_EXECUTION_RESTORE_OPTIONS;
                ExecutionNode->SavedOptions = Shell->Options;
            }

            continue;
        }

        //
        // Ensure the variable name is valid.
        //

        if (ShIsName(Argument, NameSize) == FALSE) {
            if (ArgumentIndex == 1) {
                PRINT_ERROR("local: %s: Bad variable name.\n", Argument);
                ReturnValue = 1;
                goto BuiltinLocalEnd;

            } else {
                continue;
            }
        }

        Exported = FALSE;
        ReadOnly = FALSE;
        Set = FALSE;
        ListHead = &(ExecutionNode->VariableList);
        ExistingVariable = ShGetVariableInScope(Shell,
                                                Argument,
                                                NameSize + 1,
                                                NULL);

        if (ExistingVariable != NULL) {
            Exported = ExistingVariable->Exported;
            ReadOnly = ExistingVariable->ReadOnly;
            Set = ExistingVariable->Set;
            if (Value == NULL) {
                Value = ExistingVariable->Value;
                ValueSize = ExistingVariable->ValueSize;

            } else if (ReadOnly != FALSE) {
                PRINT_ERROR("local: Variable %s is read-only.\n", Argument);
                ReturnValue = 1;
                goto BuiltinLocalEnd;
            }
        }

        if (Value != NULL) {
            Set = TRUE;
        }

        //
        // Set the new variable in the scope of the function.
        //

        Result = ShSetVariableInList(ListHead,
                                     Argument,
                                     NameSize + 1,
                                     Value,
                                     ValueSize,
                                     Exported,
                                     ReadOnly,
                                     Set);

        if (Result == FALSE) {
            ReturnValue = 1;
            goto BuiltinLocalEnd;
        }
    }

    ReturnValue = 0;

BuiltinLocalEnd:
    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

PSHELL_VARIABLE
ShGetVariableInScope (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    PLIST_ENTRY *ListHead
    )

/*++

Routine Description:

    This routine gets the variable structure for the proper scopoe.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to get.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    ListHead - Supplies an optional pointer where the head of the list where
        the variable was found will be returned.

Return Value:

    Returns a pointer to the variable on success.

    NULL if the variable could not be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSHELL_EXECUTION_NODE ExecutionNode;
    ULONG NameHash;
    PSHELL_VARIABLE Variable;

    NameHash = ShHashName(Name, NameSize);

    //
    // Look through each element on the stack (starting with the newest) to
    // try and find this variable.
    //

    CurrentEntry = Shell->ExecutionStack.Next;
    while (CurrentEntry != &(Shell->ExecutionStack)) {
        ExecutionNode = LIST_VALUE(CurrentEntry,
                                   SHELL_EXECUTION_NODE,
                                   ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if (LIST_EMPTY(&(ExecutionNode->VariableList)) != FALSE) {
            continue;
        }

        Variable = ShGetVariableInList(&(ExecutionNode->VariableList),
                                       Name,
                                       NameSize,
                                       NameHash);

        if (Variable != NULL) {
            if (ListHead != NULL) {
                *ListHead = &(ExecutionNode->VariableList);
            }

            return Variable;
        }
    }

    //
    // Try the shell itself.
    //

    Variable = ShGetVariableInList(&(Shell->VariableList),
                                   Name,
                                   NameSize,
                                   NameHash);

    if (Variable != NULL) {
        if (ListHead != NULL) {
            *ListHead = &(Shell->VariableList);
        }
    }

    return Variable;
}

PSHELL_VARIABLE
ShGetVariableInList (
    PLIST_ENTRY ListHead,
    PSTR Name,
    UINTN NameSize,
    ULONG NameHash
    )

/*++

Routine Description:

    This routine gets the value of the given environment variable searching
    through a list of environment variable structures.

Arguments:

    ListHead - Supplies a pointer to the head of hte list to search.

    Name - Supplies a pointer to the string of the name of the variable to get.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    NameHash - Supplies the hash of the name.

Return Value:

    Returns a pointer to the variable on success.

    NULL if the variable could not be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSHELL_VARIABLE Variable;

    assert(NameSize > 1);

    //
    // Look through each variable and try to match.
    //

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Variable = LIST_VALUE(CurrentEntry, SHELL_VARIABLE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Variable->Hash != NameHash) {
            continue;
        }

        if ((strncmp(Name, Variable->Name, NameSize - 1) == 0) &&
            (Variable->Name[NameSize - 1] == '\0')) {

            return Variable;
        }
    }

    return NULL;
}

BOOL
ShSetVariableInList (
    PLIST_ENTRY ListHead,
    PSTR Name,
    UINTN NameSize,
    PSTR Value,
    UINTN ValueSize,
    BOOL Exported,
    BOOL ReadOnly,
    BOOL Set
    )

/*++

Routine Description:

    This routine sets an environment variable in the given list (of either a
    node or a shell).

Arguments:

    ListHead - Supplies the head of the list of environment variables to add
        this one to.

    Name - Supplies a pointer to the string of the name of the variable to set.
        A copy of this string will be made.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Value - Supplies the value string of the variable to set. This may get
        expanded. A copy of this string will be made.

    ValueSize - Supplies the size of the value string in bytes, including the
        null terminator.

    Exported - Supplies a boolean indicating if the variable should be marked
        for export.

    ReadOnly - Supplies a boolean indicating if the variable should be marked
        read-only.

    Set - Supplies a boolean indicating if the variable should be set or not.
        If this value is FALSE, the value parameter will be ignored.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG NameHash;
    unsigned long PathSize;
    BOOL Result;
    PSTR ValueCopy;
    PSHELL_VARIABLE Variable;

    ValueCopy = NULL;
    if (Value != NULL) {

        assert(Set != FALSE);
        assert(ValueSize != 0);

        ValueCopy = SwStringDuplicate(Value, ValueSize);
        if (ValueCopy == NULL) {
            return FALSE;
        }
    }

    //
    // Allow some operating system dependent stuff to happen to the path
    // variable. This is really just a workaround for Windows.
    //

    if ((ValueCopy != NULL) && (strncmp(Name, SHELL_PATH, NameSize - 1) == 0)) {
        PathSize = ValueSize;
        Result = ShFixUpPath(&ValueCopy, &PathSize);
        ValueSize = PathSize;
        if (Result == FALSE) {
            goto SetVariableInListEnd;
        }
    }

    NameHash = ShHashName(Name, NameSize);

    //
    // Look to see if the variable is already set in the list.
    //

    Variable = ShGetVariableInList(ListHead, Name, NameSize, NameHash);
    if (Variable != NULL) {

        //
        // Fail if the variable is read-only.
        //

        if (Variable->ReadOnly != FALSE) {
            PRINT_ERROR("Variable %s is read-only.\n", Variable->Name);
            Result = FALSE;
            goto SetVariableInListEnd;
        }

        //
        // If the variable is being set, assign the new value.
        //

        if (Set != FALSE) {
            if (Variable->Value != NULL) {
                free(Variable->Value);
            }

            Variable->Value = ValueCopy;
            Variable->ValueSize = ValueSize;
            Variable->Set = Set;
        }

        //
        // Only assign the fancy properties if they're being set.
        //

        if (Exported != FALSE) {
            Variable->Exported = Exported;
        }

        if (ReadOnly != FALSE) {
            Variable->ReadOnly = ReadOnly;
        }

        ValueCopy = NULL;
        Result = TRUE;
        goto SetVariableInListEnd;
    }

    //
    // The variable doesn't exist, at least not on this list. Create it.
    //

    Variable = ShCreateVariable(Name,
                                NameSize,
                                NameHash,
                                ValueCopy,
                                ValueSize,
                                Exported,
                                ReadOnly,
                                Set);

    if (Variable == NULL) {
        Result = FALSE;
        goto SetVariableInListEnd;
    }

    INSERT_BEFORE(&(Variable->ListEntry), ListHead);
    Result = TRUE;

SetVariableInListEnd:
    if (Result != FALSE) {

        assert(Variable != NULL);

        if (Variable->Exported != FALSE) {
            ShSetEnvironmentVariable(Variable->Name, Variable->Value);
        }
    }

    if (ValueCopy != NULL) {
        free(ValueCopy);
    }

    return Result;
}

PSHELL_VARIABLE
ShCreateVariable (
    PSTR Name,
    UINTN NameSize,
    ULONG NameHash,
    PSTR Value,
    UINTN ValueSize,
    BOOL Exported,
    BOOL ReadOnly,
    BOOL Set
    )

/*++

Routine Description:

    This routine sets an environment variable in the given shell node.

Arguments:

    Name - Supplies a pointer to the string of the name of the variable to set.
        A copy of this string will be made.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    NameHash - Supplies the hash of the variable name.

    Value - Supplies the value string of the variable to set. This may get
        expanded. A copy of this string will be made.

    ValueSize - Supplies the size of the value string in bytes, including the
        null terminator.

    Exported - Supplies a boolean indicating if the variable should be marked
        for export.

    ReadOnly - Supplies a boolean indicating if the variable should be marked
        read-only.

    Set - Supplies a boolean indicating if the variable should be set or not.
        If this value is FALSE, the value parameter will be ignored.

Return Value:

    Returns a pointer to the new structure on success.

    NULL on allocation failure.

--*/

{

    BOOL Result;
    PSHELL_VARIABLE Variable;

    assert((Name != NULL) && (NameSize != 0));

    Result = FALSE;
    Variable = malloc(sizeof(SHELL_VARIABLE));
    if (Variable == NULL) {
        goto CreateVariableEnd;
    }

    memset(Variable, 0, sizeof(SHELL_VARIABLE));
    Variable->Name = SwStringDuplicate(Name, NameSize);
    if (Variable->Name == NULL) {
        goto CreateVariableEnd;
    }

    Variable->NameSize = NameSize;
    Variable->Hash = NameHash;
    if (Value != NULL) {

        assert(ValueSize != 0);
        assert(Set != FALSE);

        Variable->Value = SwStringDuplicate(Value, ValueSize);
        if (Variable->Value == NULL) {
            goto CreateVariableEnd;
        }

    } else {

        assert(ValueSize == 0);
    }

    Variable->ValueSize = ValueSize;
    Variable->Exported = Exported;
    Variable->ReadOnly = ReadOnly;
    Variable->Set = Set;
    if (Exported != FALSE) {
        Variable->OriginalValue = ShGetEnvironmentVariable(Variable->Name);
        if (Variable->OriginalValue == NULL) {
            Variable->OriginalValueSize = 0;

        } else {
            Variable->OriginalValueSize = strlen(Variable->OriginalValue) + 1;
        }
    }

    Result = TRUE;

CreateVariableEnd:
    if (Result == FALSE) {
        if (Variable != NULL) {
            if (Variable->Name != NULL) {
                free(Variable->Name);
            }

            if (Variable->Value != NULL) {
                free(Variable->Value);
            }

            free(Variable);
            Variable = NULL;
        }
    }

    return Variable;
}

BOOL
ShCopyVariablesOnList (
    PLIST_ENTRY Source,
    PLIST_ENTRY Destination
    )

/*++

Routine Description:

    This routine copies all the variables from one list to another. Any
    variables with conflicting names already on the destination list will be
    overwritten.

Arguments:

    Source - Supplies a pointer to the head of the list containing the
        variables to copy.

    Destination - Supplies a pointer to the head of the list where the copies
        will be put.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    BOOL Result;
    PSHELL_VARIABLE Variable;

    CurrentEntry = Source->Next;
    while (CurrentEntry != Source) {
        Variable = LIST_VALUE(CurrentEntry, SHELL_VARIABLE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Result = ShSetVariableInList(Destination,
                                     Variable->Name,
                                     Variable->NameSize,
                                     Variable->Value,
                                     Variable->ValueSize,
                                     Variable->Exported,
                                     Variable->ReadOnly,
                                     Variable->Set);

        if (Result == FALSE) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID
ShDestroyVariable (
    PSHELL_VARIABLE Variable,
    BOOL RestoreEnvironment
    )

/*++

Routine Description:

    This routine destroys an environment variable. It assumes the variable has
    already been removed from any lists.

Arguments:

    Variable - Supplies a pointer to the variable to destroy.

    RestoreEnvironment - Supplies a boolean indicating whether or not the
        corresponding environment variable should be restored to its original
        value.

Return Value:

    None.

--*/

{

    assert(Variable->ListEntry.Next == NULL);

    if ((RestoreEnvironment != FALSE) && (Variable->Exported != FALSE)) {
        ShSetEnvironmentVariable(Variable->Name, Variable->OriginalValue);
    }

    if (Variable->Name != NULL) {
        free(Variable->Name);
    }

    if (Variable->Value != NULL) {
        free(Variable->Value);
    }

    if (Variable->OriginalValue != NULL) {
        free(Variable->OriginalValue);
    }

    free(Variable);
    return;
}

VOID
ShPrintAllVariables (
    PSHELL Shell,
    BOOL Exported,
    BOOL ReadOnly
    )

/*++

Routine Description:

    This routine prints all variables visible in the current context of the
    shell.

Arguments:

    Shell - Supplies a pointer to the shell containing the variables to print.

    Exported - Supplies a boolean indicating whether to print the exported
        variables only.

    ReadOnly - Supplies a boolean indicating whether to print the read-only
        variables only.

Return Value:

    None.

--*/

{

    BOOL Result;
    LIST_ENTRY VariableList;

    //
    // Create a copy of the variable list in the current shell, which sorts out
    // de-duping and scope.
    //

    INITIALIZE_LIST_HEAD(&VariableList);
    Result = ShCopyVariables(Shell, &VariableList);
    if (Result == FALSE) {
        PRINT_ERROR("Could not create variable list.\n");
        return;
    }

    ShPrintVariablesInList(Shell, &VariableList, Exported, ReadOnly);
    ShDestroyVariableList(&VariableList);
    return;
}

VOID
ShPrintVariablesInList (
    PSHELL Shell,
    PLIST_ENTRY ListHead,
    BOOL Exported,
    BOOL ReadOnly
    )

/*++

Routine Description:

    This routine prints all the variables in the given list, skipping the ones
    that already exist in a more local contest.

Arguments:

    Shell - Supplies a pointer to the shell.

    ListHead - Supplies the head of the list of variables to print.

    Exported - Supplies a boolean indicating if only exported variables should
        be printed.

    ReadOnly - Supplies a boolean indicating if only read-only variables should
        be printed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSTR FormattedString;
    UINTN FormattedStringSize;
    BOOL Result;
    PSHELL_VARIABLE Variable;

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Variable = LIST_VALUE(CurrentEntry, SHELL_VARIABLE, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Skip any special variables.
        //

        if ((strcmp(Variable->Name, SHELL_LINE_NUMBER) == 0) ||
            (strcmp(Variable->Name, SHELL_RANDOM) == 0)) {

            continue;
        }

        //
        // Skip this variable if it's not set and the filters are off.
        //

        if ((Exported == FALSE) &&
            (ReadOnly == FALSE) &&
            (Variable->Set == FALSE)) {

            continue;
        }

        if ((Exported != FALSE) && (Variable->Exported == FALSE)) {
            continue;
        }

        if ((ReadOnly != FALSE) && (Variable->ReadOnly == FALSE)) {
            continue;
        }

        Result = ShStringFormatForReentry(Variable->Value,
                                          Variable->ValueSize,
                                          &FormattedString,
                                          &FormattedStringSize);

        if (Result == FALSE) {
            continue;
        }

        if (Exported != FALSE) {
            printf("export ");

        } else if (ReadOnly != FALSE) {
            printf("readonly ");
        }

        if (Variable->Set == FALSE) {
            printf("%s\n", Variable->Name);

        } else {
            printf("%s=%s\n", Variable->Name, FormattedString);
        }

        free(FormattedString);
    }

    return;
}

INT
ShBuiltinExportOrReadOnly (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments,
    BOOL Export,
    BOOL ReadOnly
    )

/*++

Routine Description:

    This routine implements the builtin export and readonly commands.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

    Export - Supplies a boolean indicating if this is an export command. If this
        is TRUE then the readonly flag is expected to be false.

    ReadOnly - Supplies a boolean indicating if this is a readonly command. If
        this is TRUE then the export flag is expected to be false.

Return Value:

    Returns 0 if the variables were unset.

    Returns greater than zero if one or more variables could not be unset.

--*/

{

    PSTR Argument;
    UINTN ArgumentIndex;
    UINTN ArgumentSize;
    PSTR CommandName;
    PSTR Equals;
    PSTR Name;
    UINTN NameSize;
    BOOL Result;
    INT ReturnValue;
    PSTR Value;
    UINTN ValueSize;

    //
    // Exactly one of these flags is supposed to be set.
    //

    assert(((Export != FALSE) && (ReadOnly == FALSE)) ||
           ((Export == FALSE) && (ReadOnly != FALSE)));

    if (Export != FALSE) {
        CommandName = "export";

    } else {
        CommandName = "readonly";
    }

    //
    // With no arguments, just prints all the variables and exit.
    //

    if (ArgumentCount == 1) {
        ShPrintAllVariables(Shell, Export, ReadOnly);
        return 0;
    }

    ReturnValue = 0;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        ArgumentSize = strlen(Argument) + 1;
        if (strcmp(Argument, "-p") == 0) {
            ShPrintAllVariables(Shell, Export, ReadOnly);
            break;
        }

        Name = Argument;
        NameSize = ArgumentSize;
        Equals = strchr(Argument, '=');
        if (Equals == NULL) {
            if (ShIsName(Name, NameSize - 1) == FALSE) {
                PRINT_ERROR("%s: Bad variable name %s.\n", CommandName, Name);
                ReturnValue = 1;
                break;
            }

            Result = ShSetVariableWithProperties(Shell,
                                                 Name,
                                                 NameSize,
                                                 NULL,
                                                 0,
                                                 Export,
                                                 ReadOnly,
                                                 FALSE);

            if (Result == FALSE) {
                ReturnValue = 1;
                break;
            }

        //
        // Export with an assignment.
        //

        } else {
            NameSize = (UINTN)Equals - (UINTN)Name + 1;
            if (ShIsName(Name, NameSize - 1) == FALSE) {
                PRINT_ERROR("%s: Bad variable name %s.\n", CommandName, Name);
                ReturnValue = 1;
                break;
            }

            Value = Equals + 1;
            ValueSize = ArgumentSize - ((UINTN)Value - (UINTN)Argument);
            Result = ShSetVariableWithProperties(Shell,
                                                 Name,
                                                 NameSize,
                                                 Value,
                                                 ValueSize,
                                                 Export,
                                                 ReadOnly,
                                                 TRUE);

            if (Result == FALSE) {
                ReturnValue = 1;
                break;
            }
        }
    }

    return ReturnValue;
}

ULONG
ShHashName (
    PSTR Name,
    UINTN NameSize
    )

/*++

Routine Description:

    This routine hashes a variable name. For those paying close attention,
    this happens to be same hash function as the ELF image format.

Arguments:

    Name - Supplies a pointer to the name to hash.

    NameSize - Supplies the size to hash.

Return Value:

    Returns the hash of the name.

--*/

{

    ULONG Hash;
    ULONG Temporary;

    assert(NameSize != 0);

    NameSize -= 1;
    Hash = 0;
    while (NameSize != 0) {
        Hash = (Hash << 4) + *Name;
        Temporary = Hash & 0xF0000000;
        if (Temporary != 0) {
            Hash ^= Temporary >> 24;
        }

        Hash &= ~Temporary;
        Name += 1;
        NameSize -= 1;
    }

    return Hash;
}

