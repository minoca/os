/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    cflow.c

Abstract:

    This module implements support for control flow in the Chalk interpreter.

Author:

    Evan Green 19-Nov-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chalkp.h"

//
// ---------------------------------------------------------------- Definitions
//

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
// ------------------------------------------------------------------ Functions
//

INT
ChalkInvokeFunction (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Function,
    PCHALK_OBJECT ArgumentList
    )

/*++

Routine Description:

    This routine pushes a new function invocation on the interpreter stack.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Function - Supplies a pointer to the function object to execute.

    ArgumentList - Supplies a pointer to the argument values.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PCHALK_OBJECT ArgumentNames;
    ULONG Count;
    ULONG Index;
    PCHALK_OBJECT *LValue;
    PCHALK_OBJECT Name;
    INT Status;
    PCHALK_OBJECT Value;

    if (Function->Header.Type != ChalkObjectFunction) {
        fprintf(stderr,
                "%s is not callable.\n",
                ChalkObjectTypeNames[Function->Header.Type]);

        Status = EINVAL;
        goto InvokeFunctionEnd;
    }

    assert(ArgumentList->Header.Type == ChalkObjectList);

    //
    // Validate the argument count.
    //

    Count = 0;
    ArgumentNames = Function->Function.Arguments;
    if (ArgumentNames != NULL) {
        Count = ArgumentNames->List.Count;
    }

    if (Count != ArgumentList->List.Count) {
        fprintf(stderr,
                "Function takes %d arguments, got %d.\n",
                Count,
                ArgumentList->List.Count);

        Status = EINVAL;
        goto InvokeFunctionEnd;
    }

    Status = ChalkPushNode(Interpreter,
                           Function->Function.Body,
                           Function->Function.Script,
                           TRUE);

    if (Status != 0) {
        goto InvokeFunctionEnd;
    }

    assert(Interpreter->Node->ParseNode == Function->Function.Body);

    Interpreter->Scope->Function = TRUE;

    //
    // Add the arguments to the base scope.
    //

    for (Index = 0; Index < Count; Index += 1) {

        assert((ArgumentNames->List.Array[Index] != NULL) &&
               (ArgumentList->List.Array[Index] != NULL));

        Name = ArgumentNames->List.Array[Index];

        assert(Name->Header.Type == ChalkObjectString);

        Value = ArgumentList->List.Array[Index];
        if (!CHALK_PASS_BY_REFERENCE(Value->Header.Type)) {
            Value = ChalkObjectCopy(Value);
            if (Value == NULL) {
                goto InvokeFunctionEnd;
            }
        }

        Status = ChalkSetVariable(Interpreter, Name, Value, &LValue);
        if (Value != ArgumentList->List.Array[Index]) {
            ChalkObjectReleaseReference(Value);
        }

        if (Status != 0) {
            goto InvokeFunctionEnd;
        }
    }

InvokeFunctionEnd:
    return Status;
}

INT
ChalkVisitJumpStatement (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a jump statement.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PCHALK_NODE Destination;
    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;

    ParseNode = Node->ParseNode;
    if (Node->ChildIndex != 0) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
    }

    //
    // If not all the expressions have been evaluated yet, go get them.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    Interpreter->LValue = NULL;

    assert((ParseNode->TokenCount >= 2) && (ParseNode->NodeCount <= 1));

    Token = ParseNode->Tokens[0];

    //
    // Grab the expression from return expr.
    //

    if (ParseNode->NodeCount != 0) {

        assert(Token->Value == ChalkTokenReturn);

        *Result = Node->Results[0];
        Node->Results[0] = NULL;
    }

    Destination = Node;
    switch (Token->Value) {
    case ChalkTokenReturn:

        //
        // Pop nodes off until a compound statement is hit. Because dicts were
        // added to the grammar, compound statements can only exist as part
        // of a function definition, and so are sufficient for determining
        // where the end of the function is.
        //

        Destination = Interpreter->Node;
        while (Destination != NULL) {
            ParseNode = Destination->ParseNode;
            if (ParseNode->GrammarElement == ChalkNodeCompoundStatement) {
                break;
            }

            Destination = Destination->Parent;
        }

        if (Destination != NULL) {
            Destination = Destination->Parent;
        }

        break;

    default:

        assert(FALSE);

        return EINVAL;
    }

    //
    // Pop scope to the destination if there is one.
    //

    if (Destination != Node) {
        while (Interpreter->Node != Destination) {
            ChalkPopNode(Interpreter);
        }

    //
    // Just pop this node.
    //

    } else {
        ChalkPopNode(Interpreter);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

