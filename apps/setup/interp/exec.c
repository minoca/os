/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    exec.c

Abstract:

    This module handles execution details for the setup interpreter.

Author:

    Evan Green 14-Oct-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../setup.h"
#include "visit.h"
#include <minoca/lib/yy.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupExecuteScript (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_SCRIPT Script
    );

INT
SetupExecute (
    PSETUP_INTERPRETER Interpreter
    );

VOID
SetupUnloadScript (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_SCRIPT Script
    );

INT
SetupPushNode (
    PSETUP_INTERPRETER Interpreter,
    PVOID ParseTree,
    PSETUP_SCRIPT Script,
    BOOL Function
    );

VOID
SetupPopNode (
    PSETUP_INTERPRETER Interpreter
    );

INT
SetupPushScope (
    PSETUP_INTERPRETER Interpreter,
    BOOL Function
    );

VOID
SetupPopScope (
    PSETUP_INTERPRETER Interpreter
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL SetupDebugNodeVisits = FALSE;
BOOL SetupDebugFinalGlobals = FALSE;

PSETUP_NODE_VISIT SetupNodeVisit[SetupNodeEnd - SetupNodeBegin] = {
    SetupVisitListElementList,
    SetupVisitList,
    SetupVisitDictElement,
    SetupVisitDictElementList,
    SetupVisitDict,
    SetupVisitPrimaryExpression,
    SetupVisitPostfixExpression,
    SetupVisitUnaryExpression,
    SetupVisitUnaryOperator,
    SetupVisitMultiplicativeExpression,
    SetupVisitAdditiveExpression,
    SetupVisitShiftExpression,
    SetupVisitRelationalExpression,
    SetupVisitEqualityExpression,
    SetupVisitAndExpression,
    SetupVisitExclusiveOrExpression,
    SetupVisitInclusiveOrExpression,
    SetupVisitLogicalAndExpression,
    SetupVisitLogicalOrExpression,
    SetupVisitConditionalExpression,
    SetupVisitAssignmentExpression,
    SetupVisitAssignmentOperator,
    SetupVisitExpression,
    SetupVisitStatementList,
    SetupVisitExpressionStatement,
    SetupVisitTranslationUnit,
};

//
// ------------------------------------------------------------------ Functions
//

INT
SetupInitializeInterpreter (
    PSETUP_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine initializes a setup interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to initialize.

Return Value:

    None.

--*/

{

    memset(Interpreter, 0, sizeof(SETUP_INTERPRETER));
    INITIALIZE_LIST_HEAD(&(Interpreter->ScriptList));
    Interpreter->Global.Dict = SetupCreateDict(NULL);
    if (Interpreter->Global.Dict == NULL) {
        return ENOMEM;
    }

    return 0;
}

VOID
SetupDestroyInterpreter (
    PSETUP_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine destroys a setup interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to destroy.

Return Value:

    None.

--*/

{

    PSETUP_SCRIPT Script;

    if (Interpreter->Global.Dict != NULL) {
        SetupObjectReleaseReference(Interpreter->Global.Dict);
        Interpreter->Global.Dict = NULL;
    }

    while (!LIST_EMPTY(&(Interpreter->ScriptList))) {
        Script = LIST_VALUE(Interpreter->ScriptList.Next,
                            SETUP_SCRIPT,
                            ListEntry);

        SetupUnloadScript(Interpreter, Script);
    }

    return;
}

INT
SetupLoadScriptBuffer (
    PSETUP_INTERPRETER Interpreter,
    PSTR Path,
    PSTR Buffer,
    ULONG Size,
    ULONG Order
    )

/*++

Routine Description:

    This routine executes the given interpreted script.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to a string describing where this script came
        from. This pointer is used directly and should stick around while the
        script is loaded.

    Buffer - Supplies a pointer to the buffer containing the script to
        execute. A copy of this buffer is created.

    Size - Supplies the size of the buffer in bytes. A null terminator will be
        added if one is not present.

    Order - Supplies the order identifier for ordering which scripts should run
        when. Supply 0 to run the script now.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSETUP_SCRIPT Script;
    INT Status;

    if (Size == 0) {
        return EINVAL;
    }

    Script = malloc(sizeof(SETUP_SCRIPT));
    if (Script == NULL) {
        return ENOMEM;
    }

    Status = 0;
    memset(Script, 0, sizeof(SETUP_SCRIPT));
    Script->Order = Order;
    Script->Path = Path;
    Script->Data = malloc(Size + 1);
    if (Script->Data == NULL) {
        free(Script);
    }

    memcpy(Script->Data, Buffer, Size);
    Script->Data[Size] = '\0';
    Script->Size = Size;
    INSERT_BEFORE(&(Script->ListEntry), &(Interpreter->ScriptList));
    if (Script->Order == 0) {
        Status = SetupExecuteScript(Interpreter, Script);
        if (Status != 0) {
            free(Script->Data);
            free(Script);
        }
    }

    return Status;
}

INT
SetupLoadScriptFile (
    PSETUP_INTERPRETER Interpreter,
    PSTR Path,
    ULONG Order
    )

/*++

Routine Description:

    This routine executes the given interpreted script from a file.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to the path of the file to load.

    Order - Supplies the order identifier for ordering which scripts should run
        when. Supply 0 to run the script now.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    FILE *File;
    size_t Read;
    PSETUP_SCRIPT Script;
    struct stat Stat;
    INT Status;
    size_t TotalRead;

    File = NULL;
    Script = NULL;
    Status = stat(Path, &Stat);
    if (Status != 0) {
        fprintf(stderr, "Cannot open %s.\n", Path);
        Status = errno;
        goto LoadScriptFileEnd;
    }

    if (!S_ISREG(Stat.st_mode)) {
        fprintf(stderr, "Path %s is invalid type.\n", Path);
        Status = EINVAL;
        goto LoadScriptFileEnd;
    }

    File = fopen(Path, "r");
    if (File == NULL) {
        fprintf(stderr, "Cannot open %s.\n", Path);
        Status = errno;
        goto LoadScriptFileEnd;
    }

    Script = malloc(sizeof(SETUP_SCRIPT));
    if (Script == NULL) {
        Status = errno;
        goto LoadScriptFileEnd;
    }

    memset(Script, 0, sizeof(SETUP_SCRIPT));
    Script->Order = Order;
    Script->Path = Path;
    Script->Data = malloc(Stat.st_size + 1);
    if (Script->Data == NULL) {
        Status = errno;
        goto LoadScriptFileEnd;
    }

    TotalRead = 0;
    while (TotalRead < Stat.st_size) {
        Read = fread(Script->Data + TotalRead,
                     1,
                     Stat.st_size - TotalRead,
                     File);

        if (Read <= 0) {
            if (ferror(File)) {
                perror("Cannot read");
                goto LoadScriptFileEnd;
            }

            break;
        }

        TotalRead += Read;
    }

    Script->Data[TotalRead] = '\0';
    if (Script->Data[TotalRead] != '\0') {
        TotalRead += 1;
    }

    Script->Size = TotalRead;
    INSERT_BEFORE(&(Script->ListEntry), &(Interpreter->ScriptList));
    if (Script->Order == 0) {
        Status = SetupExecuteScript(Interpreter, Script);
        if (Status != 0) {
            goto LoadScriptFileEnd;
        }
    }

LoadScriptFileEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (Status != 0) {
        perror("Error");
        if (Script != NULL) {
            if (Script->Data != NULL) {
                free(Script->Data);
            }

            free(Script);
        }
    }

    return Status;
}

INT
SetupExecuteDeferredScripts (
    PSETUP_INTERPRETER Interpreter,
    ULONG Order
    )

/*++

Routine Description:

    This routine executes scripts that have been loaded but not yet run.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Order - Supplies the order identifier. Any scripts with this order
        identifier that have not yet been run will be run now.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSETUP_SCRIPT Script;
    INT Status;

    Status = 0;
    CurrentEntry = Interpreter->ScriptList.Next;
    while (CurrentEntry != &(Interpreter->ScriptList)) {
        Script = LIST_VALUE(CurrentEntry, SETUP_SCRIPT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Script->ParseTree != NULL) || (Script->Order != Order)) {
            continue;
        }

        Status = SetupExecuteScript(Interpreter, Script);
        if (Status != 0) {
            goto ExecuteDeferredScriptsEnd;
        }
    }

ExecuteDeferredScriptsEnd:
    return Status;
}

PSETUP_OBJECT
SetupGetVariable (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Name,
    PSETUP_OBJECT **LValue
    )

/*++

Routine Description:

    This routine attempts to find a variable by the given name.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Name - Supplies a pointer to the name string to find.

    LValue - Supplies an optional pointer where an LValue pointer will be
        returned on success. The caller can use the return of this pointer to
        assign into the dictionary element later.

Return Value:

    Returns a pointer to the variable value with an increased reference count
    on success.

    NULL if no such variable exists.

--*/

{

    PSETUP_DICT_ENTRY Result;
    PSETUP_SCOPE Scope;

    assert(Name->Header.Type == SetupObjectString);

    //
    // Loop searching in all visible scopes.
    //

    Scope = Interpreter->Scope;
    while (Scope != NULL) {
        Result = SetupDictLookup(Scope->Dict, Name);
        if (Result != NULL) {
            SetupObjectAddReference(Result->Value);
            if (LValue != NULL) {
                *LValue = &(Result->Value);
            }

            return Result->Value;
        }

        if (Scope->Function != FALSE) {
            break;
        }

        Scope = Scope->Parent;
    }

    //
    // Also search the global scope.
    //

    Result = SetupDictLookup(Interpreter->Global.Dict, Name);
    if (Result != NULL) {
        SetupObjectAddReference(Result->Value);
        if (LValue != NULL) {
            *LValue = &(Result->Value);
        }

        return Result->Value;
    }

    return NULL;
}

INT
SetupSetVariable (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Name,
    PSETUP_OBJECT Value,
    PSETUP_OBJECT **LValue
    )

/*++

Routine Description:

    This routine sets or creates a new variable in the current scope.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Name - Supplies a pointer to the name string to create.

    Value - Supplies a pointer to the variable value.

    LValue - Supplies an optional pointer where an LValue pointer will be
        returned on success. The caller can use the return of this pointer to
        assign into the dictionary element later.

Return Value:

    Returns a pointer to the variable value with an increased reference count
    on success. The caller should release this reference when finished.

    NULL if no such variable exists.

--*/

{

    PSETUP_SCOPE Scope;

    Scope = Interpreter->Scope;
    if (Scope == NULL) {
        Scope = &(Interpreter->Global);
    }

    return SetupDictSetElement(Scope->Dict, Name, Value, LValue);
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupExecuteScript (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_SCRIPT Script
    )

/*++

Routine Description:

    This routine executes the given interpreted script.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Script - Supplies a pointer to the script to execute.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;

    assert(Script->ParseTree == NULL);

    Status = SetupParseScript(Script, &(Script->ParseTree));
    if (Status != 0) {
        Status = ENOMEM;
        goto ExecuteScriptEnd;
    }

    Status = SetupPushNode(Interpreter, Script->ParseTree, Script, FALSE);
    if (Status != 0) {
        goto ExecuteScriptEnd;
    }

    Status = SetupExecute(Interpreter);
    if (Status != 0) {
        goto ExecuteScriptEnd;
    }

    if (SetupDebugFinalGlobals != FALSE) {
        printf("Globals: ");
        SetupPrintObject(Interpreter->Global.Dict, 0);
        printf("\n");
    }

ExecuteScriptEnd:
    return Status;
}

INT
SetupExecute (
    PSETUP_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine executes the given interpreter setup.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to run on.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONG GrammarIndex;
    PSETUP_NODE Node;
    PSETUP_NODE Parent;
    PPARSER_NODE ParseNode;
    PSETUP_OBJECT Result;
    INT Status;
    PSETUP_NODE_VISIT VisitFunction;

    while (Interpreter->Node != NULL) {
        Node = Interpreter->Node;
        ParseNode = Node->ParseNode;

        //
        // If this is not the end, visit the next child.
        //

        if (Node->ChildIndex < ParseNode->NodeCount) {
            Status = SetupPushNode(Interpreter,
                                   ParseNode->Nodes[Node->ChildIndex],
                                   Node->Script,
                                   FALSE);

            if (Status != 0) {
                break;
            }

            Node->ChildIndex += 1;

        //
        // All the children have been popped, so visit this node and pop it.
        //

        } else {
            Result = NULL;
            GrammarIndex = ParseNode->GrammarElement - SetupNodeBegin;
            VisitFunction = SetupNodeVisit[GrammarIndex];
            if (SetupDebugNodeVisits != FALSE) {
                printf("%*s%s 0x%x 0x%x\n",
                       Interpreter->NodeDepth,
                       "",
                       SetupGetNodeGrammarName(Node),
                       Node,
                       ParseNode);
            }

            Status = VisitFunction(Interpreter, Node, &Result);
            if (Status != 0) {
                fprintf(stderr,
                        "Interpreter error around %s:%d:%d: %s.\n",
                        Node->Script->Path,
                        ParseNode->StartToken->Line,
                        ParseNode->StartToken->Column,
                        strerror(Status));

                break;
            }

            //
            // Move the result of the visitation (the return value) up into the
            // parent node.
            //

            if (Node != Interpreter->Node) {
                Parent = Interpreter->Node;

            } else {
                Parent = Node->Parent;
            }

            if (Parent != NULL) {

                assert(Parent->ChildIndex != 0);

                Parent->Results[Parent->ChildIndex - 1] = Result;

                //
                // Move the LValue of the first node up to the parent.
                //

                if (Parent->ChildIndex - 1 == 0) {
                    Parent->LValue = Node->LValue;
                }

            } else if (Result != NULL) {
                SetupObjectReleaseReference(Result);
            }

            //
            // Remove this node from the execution stack unless it already was
            // (ie break or return).
            //

            if (Node == Interpreter->Node) {
                SetupPopNode(Interpreter);
            }
        }
    }

    while (Interpreter->Node != NULL) {
        SetupPopNode(Interpreter);
    }

    return Status;
}

VOID
SetupUnloadScript (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_SCRIPT Script
    )

/*++

Routine Description:

    This routine unloads and frees a script.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Script - Supplies a pointer to the script to unload.

Return Value:

    None.

--*/

{

    if (Script->ListEntry.Next != NULL) {
        LIST_REMOVE(&(Script->ListEntry));
    }

    if (Script->Data != NULL) {
        free(Script->Data);
    }

    free(Script);
    return;
}

INT
SetupPushNode (
    PSETUP_INTERPRETER Interpreter,
    PVOID ParseTree,
    PSETUP_SCRIPT Script,
    BOOL Function
    )

/*++

Routine Description:

    This routine pushes a new node onto the current interpreter execution.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    ParseTree - Supplies a pointer to the parse tree of the new node.

    Script - Supplies a pointer to the script this tree came from.

    Function - Supplies a boolean indicating if this is a function scope or
        not.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSETUP_NODE Node;
    PPARSER_NODE ParseNode;
    ULONG Size;
    INT Status;

    ParseNode = ParseTree;
    Size = sizeof(SETUP_NODE) + (ParseNode->NodeCount * sizeof(PVOID));
    Node = malloc(Size);
    if (Node == NULL) {
        return ENOMEM;
    }

    memset(Node, 0, Size);
    Node->Parent = Interpreter->Node;
    Node->ParseNode = ParseTree;
    Node->Script = Script;
    Node->Results = (PSETUP_OBJECT *)(Node + 1);
    if (Function != FALSE) {
        Status = SetupPushScope(Interpreter, Function);
        if (Status != 0) {
            free(Node);
            return Status;
        }

        Node->BaseScope = Interpreter->Scope;
    }

    Interpreter->Node = Node;
    Interpreter->NodeDepth += 1;
    return 0;
}

VOID
SetupPopNode (
    PSETUP_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine pops the current node off the execution stack.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

Return Value:

    None.

--*/

{

    ULONG Count;
    ULONG Index;
    PSETUP_NODE Node;
    PPARSER_NODE ParseNode;

    Node = Interpreter->Node;
    ParseNode = Node->ParseNode;

    assert(Interpreter->NodeDepth != 0);

    //
    // Free any intermediate results.
    //

    Count = ParseNode->NodeCount;
    for (Index = 0; Index < Count; Index += 1) {
        if (Node->Results[Index] != NULL) {
            SetupObjectReleaseReference(Node->Results[Index]);
        }
    }

    if (Node->BaseScope != NULL) {
        while (Interpreter->Scope != Node->BaseScope) {
            SetupPopScope(Interpreter);
        }

        SetupPopScope(Interpreter);
    }

    Interpreter->Node = Node->Parent;
    Interpreter->NodeDepth -= 1;
    free(Node);
    return;
}

INT
SetupPushScope (
    PSETUP_INTERPRETER Interpreter,
    BOOL Function
    )

/*++

Routine Description:

    This routine pushes a new scope onto the current execution node.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Function - Supplies a boolean indicating if this is a function scope or
        not.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSETUP_SCOPE Scope;

    Scope = malloc(sizeof(SETUP_SCOPE));
    if (Scope == NULL) {
        return ENOMEM;
    }

    memset(Scope, 0, sizeof(SETUP_SCOPE));
    Scope->Parent = Interpreter->Scope;
    Scope->Function = Function;
    Scope->Dict = SetupCreateDict(NULL);
    if (Scope->Dict == NULL) {
        free(Scope);
        return ENOMEM;
    }

    Interpreter->Scope = Scope;
    return 0;
}

VOID
SetupPopScope (
    PSETUP_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine pops the current scope off the execution node.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

Return Value:

    None.

--*/

{

    PSETUP_SCOPE Scope;

    Scope = Interpreter->Scope;
    Interpreter->Scope = Scope->Parent;
    SetupObjectReleaseReference(Scope->Dict);
    free(Scope);
    return;
}

