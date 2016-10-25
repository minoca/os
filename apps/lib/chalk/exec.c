/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    exec.c

Abstract:

    This module handles execution details for the Chalk interpreter.

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

#include "chalkp.h"
#include "visit.h"

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
ChalkExecuteScript (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_SCRIPT Script,
    PCHALK_OBJECT *ReturnValue
    );

INT
ChalkExecute (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT *ReturnValue
    );

PCHALK_SCRIPT
ChalkCreateScript (
    PCHALK_INTERPRETER Interpreter,
    PSTR Path,
    ULONG Order
    );

VOID
ChalkUnloadScript (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_SCRIPT Script
    );

INT
ChalkPushScope (
    PCHALK_INTERPRETER Interpreter
    );

VOID
ChalkPopScope (
    PCHALK_INTERPRETER Interpreter
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL ChalkDebugNodeVisits = FALSE;
BOOL ChalkDebugFinalGlobals = FALSE;

PCHALK_NODE_VISIT ChalkNodeVisit[ChalkNodeEnd - ChalkNodeBegin] = {
    ChalkVisitListElementList,
    ChalkVisitList,
    ChalkVisitDictElement,
    ChalkVisitDictElementList,
    ChalkVisitDict,
    ChalkVisitPrimaryExpression,
    ChalkVisitPostfixExpression,
    ChalkVisitArgumentExpressionList,
    ChalkVisitUnaryExpression,
    ChalkVisitUnaryOperator,
    ChalkVisitMultiplicativeExpression,
    ChalkVisitAdditiveExpression,
    ChalkVisitShiftExpression,
    ChalkVisitRelationalExpression,
    ChalkVisitEqualityExpression,
    ChalkVisitAndExpression,
    ChalkVisitExclusiveOrExpression,
    ChalkVisitInclusiveOrExpression,
    ChalkVisitLogicalAndExpression,
    ChalkVisitLogicalOrExpression,
    ChalkVisitConditionalExpression,
    ChalkVisitAssignmentExpression,
    ChalkVisitAssignmentOperator,
    ChalkVisitExpression,
    ChalkVisitStatement,
    ChalkVisitCompoundStatement,
    ChalkVisitStatementList,
    ChalkVisitExpressionStatement,
    ChalkVisitSelectionStatement,
    ChalkVisitIterationStatement,
    ChalkVisitJumpStatement,
    ChalkVisitTranslationUnit,
    ChalkVisitExternalDeclaration,
    ChalkVisitIdentifierList,
    ChalkVisitFunctionDefinition,
};

//
// ------------------------------------------------------------------ Functions
//

INT
ChalkInitializeInterpreter (
    PCHALK_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine initializes a Chalk interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to initialize.

Return Value:

    None.

--*/

{

    INT Status;

    memset(Interpreter, 0, sizeof(CHALK_INTERPRETER));
    INITIALIZE_LIST_HEAD(&(Interpreter->ScriptList));
    Interpreter->Global.Dict = ChalkCreateDict(NULL);
    if (Interpreter->Global.Dict == NULL) {
        Status = ENOMEM;
        goto InitializeInterpreterEnd;
    }

    Interpreter->Generation = 1;

    //
    // Add the builtin functions.
    //

    Status = ChalkRegisterFunctions(Interpreter, NULL, ChalkBuiltinFunctions);
    if (Status != 0) {
        goto InitializeInterpreterEnd;
    }

    Status = 0;

InitializeInterpreterEnd:
    if (Status != 0) {
        ChalkDestroyInterpreter(Interpreter);
    }

    return Status;
}

VOID
ChalkDestroyInterpreter (
    PCHALK_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine destroys a Chalk interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to destroy.

Return Value:

    None.

--*/

{

    PCHALK_SCRIPT Script;

    if (Interpreter->Global.Dict != NULL) {
        ChalkObjectReleaseReference(Interpreter->Global.Dict);
        Interpreter->Global.Dict = NULL;
    }

    while (!LIST_EMPTY(&(Interpreter->ScriptList))) {
        Script = LIST_VALUE(Interpreter->ScriptList.Next,
                            CHALK_SCRIPT,
                            ListEntry);

        ChalkUnloadScript(Interpreter, Script);
    }

    return;
}

INT
ChalkClearInterpreter (
    PCHALK_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine clears the global variable scope back to its original state
    for the given Chalk interpreter. Loaded scripts are still saved, but the
    interpreter state is as if they had never been executed.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to reset.

Return Value:

    0 on success.

    Returns an error number if the context could not be fully reinitialized.

--*/

{

    INT Status;

    if (Interpreter->Global.Dict != NULL) {
        ChalkObjectReleaseReference(Interpreter->Global.Dict);
        Interpreter->Global.Dict = NULL;
    }

    Interpreter->Generation += 1;
    Interpreter->Global.Dict = ChalkCreateDict(NULL);
    if (Interpreter->Global.Dict == NULL) {
        Status = ENOMEM;
        goto ClearInterpreterEnd;
    }

    //
    // Add the builtin functions.
    //

    Status = ChalkRegisterFunctions(Interpreter, NULL, ChalkBuiltinFunctions);
    if (Status != 0) {
        goto ClearInterpreterEnd;
    }

ClearInterpreterEnd:
    return Status;
}

INT
ChalkLoadScriptBuffer (
    PCHALK_INTERPRETER Interpreter,
    PSTR Path,
    PSTR Buffer,
    ULONG Size,
    ULONG Order,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine executes the given interpreted script.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to a string describing where this script came
        from. A copy of this string is made.

    Buffer - Supplies a pointer to the buffer containing the script to
        execute. A copy of this buffer is created.

    Size - Supplies the size of the buffer in bytes. A null terminator will be
        added if one is not present.

    Order - Supplies the order identifier for ordering which scripts should run
        when. Supply 0 to run the script now.

    ReturnValue - Supplies an optional pointer where the return value from
        the script will be returned. It is the caller's responsibility to
        release the object. This is only filled in if the order is zero
        (so the script is executed now).

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PCHALK_SCRIPT Script;
    INT Status;

    if (Size == 0) {
        return EINVAL;
    }

    Script = ChalkCreateScript(Interpreter, Path, Order);
    if (Script == NULL) {
        return ENOMEM;
    }

    Status = 0;
    Script->Data = ChalkAllocate(Size + 1);
    if (Script->Data == NULL) {
        ChalkUnloadScript(Interpreter, Script);
    }

    memcpy(Script->Data, Buffer, Size);
    Script->Data[Size] = '\0';
    Script->Size = Size;
    INSERT_BEFORE(&(Script->ListEntry), &(Interpreter->ScriptList));
    if (Script->Order == 0) {
        Status = ChalkExecuteScript(Interpreter, Script, ReturnValue);
        if (Status != 0) {
            ChalkUnloadScript(Interpreter, Script);
        }
    }

    return Status;
}

INT
ChalkLoadScriptFile (
    PCHALK_INTERPRETER Interpreter,
    PSTR Path,
    ULONG Order,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine executes the given interpreted script from a file.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Path - Supplies a pointer to the path of the file to load. A copy of this
        string is made.

    Order - Supplies the order identifier for ordering which scripts should run
        when. Supply 0 to run the script now.

    ReturnValue - Supplies an optional pointer where the return value from
        the script will be returned. It is the caller's responsibility to
        release the object. This is only filled in if the order is zero
        (so the script is executed now).

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    FILE *File;
    size_t Read;
    PCHALK_SCRIPT Script;
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

    Script = ChalkCreateScript(Interpreter, Path, Order);
    if (Script == NULL) {
        Status = ENOMEM;
        goto LoadScriptFileEnd;
    }

    Script->Data = ChalkAllocate(Stat.st_size + 1);
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
        Status = ChalkExecuteScript(Interpreter, Script, ReturnValue);
        if (Status != 0) {
            LIST_REMOVE(&(Script->ListEntry));
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
            ChalkUnloadScript(Interpreter, Script);
        }
    }

    return Status;
}

INT
ChalkExecuteDeferredScripts (
    PCHALK_INTERPRETER Interpreter,
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
    PCHALK_SCRIPT Script;
    INT Status;

    Status = 0;
    CurrentEntry = Interpreter->ScriptList.Next;
    while (CurrentEntry != &(Interpreter->ScriptList)) {
        Script = LIST_VALUE(CurrentEntry, CHALK_SCRIPT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Script->Generation == Interpreter->Generation) ||
            (Script->Order != Order)) {

            continue;
        }

        Status = ChalkExecuteScript(Interpreter, Script, NULL);
        if (Status != 0) {
            goto ExecuteDeferredScriptsEnd;
        }
    }

ExecuteDeferredScriptsEnd:
    return Status;
}

INT
ChalkUnloadScriptsByOrder (
    PCHALK_INTERPRETER Interpreter,
    ULONG Order
    )

/*++

Routine Description:

    This routine unloads all scripts of a given order. It also resets the
    interpreter context.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Order - Supplies the order identifier. Any scripts with this order
        identifier will be unloaded. Supply 0 to unload all scripts.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PCHALK_SCRIPT Script;
    INT Status;

    Status = ChalkClearInterpreter(Interpreter);
    if (Status != 0) {
        goto UnloadScriptsByOrderEnd;
    }

    CurrentEntry = Interpreter->ScriptList.Next;
    while (CurrentEntry != &(Interpreter->ScriptList)) {
        Script = LIST_VALUE(CurrentEntry, CHALK_SCRIPT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Order == 0) || (Script->Order == Order)) {

            //
            // It would be bad to unload a script whose functions are
            // still visible in the global context.
            //

            assert(Script->Generation != Interpreter->Generation);

            ChalkUnloadScript(Interpreter, Script);
        }
    }

    Status = 0;

UnloadScriptsByOrderEnd:
    return Status;
}

INT
ChalkExecuteFunction (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Function,
    PCHALK_OBJECT ArgumentList,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine executes a Chalk function and returns the result.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Function - Supplies a pointer to the function object to execute.

    ArgumentList - Supplies a pointer to the argument values.

    ReturnValue - Supplies an optional pointer where a pointer to the
        evaluation will be returned. It is the caller's responsibility to
        release this reference.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PCHALK_OBJECT Return;
    INT Status;

    Return = NULL;
    if ((Function == NULL) || (Function->Header.Type != ChalkObjectFunction)) {
        Status = EINVAL;
        goto ExecuteFunctionEnd;
    }

    Status = ChalkInvokeFunction(Interpreter, Function, ArgumentList, &Return);
    if (Status != 0) {
        goto ExecuteFunctionEnd;
    }

    //
    // It could have been that the Chalk function was actually a C function,
    // and it's already done.
    //

    if (Interpreter->Node == NULL) {
        goto ExecuteFunctionEnd;
    }

    Status = ChalkExecute(Interpreter, &Return);

ExecuteFunctionEnd:
    if (ReturnValue != NULL) {
        *ReturnValue = Return;
        Return = NULL;
    }

    if (Return != NULL) {
        ChalkObjectReleaseReference(Return);
    }

    return Status;
}

PCHALK_OBJECT
ChalkGetVariable (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Name,
    PCHALK_OBJECT **LValue
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

    PCHALK_DICT_ENTRY Result;
    PCHALK_SCOPE Scope;

    assert(Name->Header.Type == ChalkObjectString);

    //
    // Loop searching in all visible scopes.
    //

    Scope = Interpreter->Scope;
    while (Scope != NULL) {
        Result = ChalkDictLookup(Scope->Dict, Name);
        if (Result != NULL) {
            ChalkObjectAddReference(Result->Value);
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

    Result = ChalkDictLookup(Interpreter->Global.Dict, Name);
    if (Result != NULL) {
        ChalkObjectAddReference(Result->Value);
        if (LValue != NULL) {
            *LValue = &(Result->Value);
        }

        return Result->Value;
    }

    return NULL;
}

INT
ChalkSetVariable (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Name,
    PCHALK_OBJECT Value,
    PCHALK_OBJECT **LValue
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

    PCHALK_SCOPE Scope;

    Scope = Interpreter->Scope;
    if (Scope == NULL) {
        Scope = &(Interpreter->Global);
    }

    return ChalkDictSetElement(Scope->Dict, Name, Value, LValue);
}

INT
ChalkPushNode (
    PCHALK_INTERPRETER Interpreter,
    PVOID ParseTree,
    PCHALK_SCRIPT Script,
    BOOL NewScope
    )

/*++

Routine Description:

    This routine pushes a new node onto the current interpreter execution.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    ParseTree - Supplies a pointer to the parse tree of the new node.

    Script - Supplies a pointer to the script this tree came from.

    NewScope - Supplies a boolean indicating if this node creates a new scope.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PCHALK_NODE Node;
    PPARSER_NODE ParseNode;
    ULONG Size;
    INT Status;

    ParseNode = ParseTree;
    Size = sizeof(CHALK_NODE);
    if (ParseNode != NULL) {
        Size += ParseNode->NodeCount * sizeof(PVOID);
    }

    Node = ChalkAllocate(Size);
    if (Node == NULL) {
        return ENOMEM;
    }

    memset(Node, 0, Size);
    Node->Parent = Interpreter->Node;
    Node->ParseNode = ParseTree;
    Node->Script = Script;
    Node->Results = (PCHALK_OBJECT *)(Node + 1);
    if (NewScope != FALSE) {
        Status = ChalkPushScope(Interpreter);
        if (Status != 0) {
            ChalkFree(Node);
            return Status;
        }

        Node->BaseScope = Interpreter->Scope;
    }

    Interpreter->Node = Node;
    Interpreter->NodeDepth += 1;
    return 0;
}

VOID
ChalkPopNode (
    PCHALK_INTERPRETER Interpreter
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
    PCHALK_NODE Node;
    PPARSER_NODE ParseNode;

    Node = Interpreter->Node;
    ParseNode = Node->ParseNode;

    assert(Interpreter->NodeDepth != 0);

    //
    // Free any intermediate results.
    //

    if (ParseNode != NULL) {
        Count = ParseNode->NodeCount;
        for (Index = 0; Index < Count; Index += 1) {
            if (Node->Results[Index] != NULL) {
                ChalkObjectReleaseReference(Node->Results[Index]);
            }
        }
    }

    if (Node->BaseScope != NULL) {
        while (Interpreter->Scope != Node->BaseScope) {
            ChalkPopScope(Interpreter);
        }

        ChalkPopScope(Interpreter);
    }

    Interpreter->Node = Node->Parent;
    Interpreter->NodeDepth -= 1;
    ChalkFree(Node);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ChalkExecuteScript (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_SCRIPT Script,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine executes the given interpreted script.

Arguments:

    Interpreter - Supplies a pointer to the initialized interpreter.

    Script - Supplies a pointer to the script to execute.

    ReturnValue - Supplies an optional pointer where the return value from
        the script will be returned. It is the caller's responsibility to
        release the object.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;

    if (Script->ParseTree == NULL) {
        Status = ChalkParseScript(Interpreter, Script, &(Script->ParseTree));
        if (Status != 0) {
            Status = ENOMEM;
            goto ExecuteScriptEnd;
        }
    }

    Status = ChalkPushNode(Interpreter, Script->ParseTree, Script, FALSE);
    if (Status != 0) {
        goto ExecuteScriptEnd;
    }

    Script->Generation = Interpreter->Generation;
    Status = ChalkExecute(Interpreter, ReturnValue);
    if (Status != 0) {
        goto ExecuteScriptEnd;
    }

    if (ChalkDebugFinalGlobals != FALSE) {
        printf("Globals: ");
        ChalkPrintObject(stdout, Interpreter->Global.Dict, 0);
        printf("\n");
    }

ExecuteScriptEnd:
    return Status;
}

INT
ChalkExecute (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine executes the given interpreter setup.

Arguments:

    Interpreter - Supplies a pointer to the interpreter to run on.

    ReturnValue - Supplies an optional pointer where a pointer to the return
        object will be returned. The caller is responsible for releasing the
        reference on this object.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONG GrammarIndex;
    PCHALK_NODE Node;
    PPARSER_NODE ParseNode;
    PCHALK_OBJECT Result;
    PCHALK_SCRIPT Script;
    INT Status;
    PCHALK_NODE_VISIT VisitFunction;

    //
    // Just keep visiting nodes until there are no more.
    //

    Result = NULL;
    while (Interpreter->Node != NULL) {
        Node = Interpreter->Node;
        Script = Node->Script;
        ParseNode = Node->ParseNode;
        GrammarIndex = ParseNode->GrammarElement - ChalkNodeBegin;
        VisitFunction = ChalkNodeVisit[GrammarIndex];
        if (ChalkDebugNodeVisits != FALSE) {
            printf("%*s%s %p %p [%s:%d:%d]\n",
                   Interpreter->NodeDepth,
                   "",
                   ChalkGetNodeGrammarName(Node),
                   Node,
                   ParseNode,
                   Script->Path,
                   ParseNode->StartToken->Line,
                   ParseNode->StartToken->Column);
        }

        Status = VisitFunction(Interpreter, Node, &Result);
        if (Status != 0) {
            fprintf(stderr,
                    "Interpreter error around %s:%d:%d: %s.\n",
                    Script->Path,
                    ParseNode->StartToken->Line,
                    ParseNode->StartToken->Column,
                    strerror(Status));

            break;
        }
    }

    //
    // If there was an error, pop any remaining nodes from the stack.
    //

    assert((Status != 0) || (Interpreter->Node == NULL));

    while (Interpreter->Node != NULL) {
        ChalkPopNode(Interpreter);
    }

    if (ReturnValue != NULL) {
        *ReturnValue = Result;

    } else {
        if (Result != NULL) {
            ChalkObjectReleaseReference(Result);
        }
    }

    return Status;
}

PCHALK_SCRIPT
ChalkCreateScript (
    PCHALK_INTERPRETER Interpreter,
    PSTR Path,
    ULONG Order
    )

/*++

Routine Description:

    This routine creates a new chalk script object.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Path - Supplies a pointer to a string describing where this script came
        from. A copy of this pointer is made.

    Order - Supplies the order identifier for ordering which scripts should run
        when.

Return Value:

    Returns a pointer to the script on success.

    NULL on allocation failure.

--*/

{

    PPARSER Parser;
    PCHALK_SCRIPT Script;
    INT Status;

    Script = ChalkAllocate(sizeof(CHALK_SCRIPT));
    if (Script == NULL) {
        Status = ENOMEM;
        goto CreateScriptEnd;
    }

    memset(Script, 0, sizeof(CHALK_SCRIPT));
    Parser = ChalkAllocate(sizeof(PARSER));
    if (Parser == NULL) {
        Status = ENOMEM;
        goto CreateScriptEnd;
    }

    memset(Parser, 0, sizeof(PARSER));
    Script->Parser = Parser;
    Script->Order = Order;
    Script->Path = strdup(Path);
    if (Script->Path == NULL) {
        Status = ENOMEM;
        goto CreateScriptEnd;
    }

    Parser->Flags = 0;
    Parser->Allocate = (PYY_ALLOCATE)ChalkAllocate;
    Parser->Free = ChalkFree;
    Parser->GetToken = ChalkLexGetToken;
    Parser->Grammar = ChalkGrammar;
    Parser->GrammarBase = ChalkNodeBegin;
    Parser->GrammarEnd = ChalkNodeEnd;
    Parser->GrammarStart = ChalkNodeTranslationUnit;
    Parser->MaxRecursion = 500;
    Status = 0;

CreateScriptEnd:
    if (Status != 0) {
        ChalkUnloadScript(Interpreter, Script);
        Script = NULL;
    }

    return Script;
}

VOID
ChalkUnloadScript (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_SCRIPT Script
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
        ChalkFree(Script->Data);
    }

    if (Script->ParseTree != NULL) {
        YyDestroyNode(Script->Parser, Script->ParseTree);
    }

    if (Script->Parser != NULL) {
        YyParserDestroy(Script->Parser);
        ChalkFree(Script->Parser);
    }

    if (Script->Path != NULL) {
        free(Script->Path);
    }

    ChalkFree(Script);
    return;
}

INT
ChalkPushScope (
    PCHALK_INTERPRETER Interpreter
    )

/*++

Routine Description:

    This routine pushes a new scope onto the current execution node.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PCHALK_SCOPE Scope;

    Scope = ChalkAllocate(sizeof(CHALK_SCOPE));
    if (Scope == NULL) {
        return ENOMEM;
    }

    memset(Scope, 0, sizeof(CHALK_SCOPE));
    Scope->Parent = Interpreter->Scope;
    Scope->Dict = ChalkCreateDict(NULL);
    if (Scope->Dict == NULL) {
        ChalkFree(Scope);
        return ENOMEM;
    }

    Interpreter->Scope = Scope;
    return 0;
}

VOID
ChalkPopScope (
    PCHALK_INTERPRETER Interpreter
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

    PCHALK_SCOPE Scope;

    Scope = Interpreter->Scope;
    Interpreter->Scope = Scope->Parent;
    ChalkObjectReleaseReference(Scope->Dict);
    ChalkFree(Scope);
    return;
}

