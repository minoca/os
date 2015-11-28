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
ChalkVisitSelectionStatement (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a selection statement (if or switch).

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

    ULONG ChildIndex;
    PCHALK_OBJECT Condition;
    PPARSER_NODE ParseNode;
    PCHALK_SCRIPT Script;
    INT Status;
    PLEXER_TOKEN Token;

    ParseNode = Node->ParseNode;

    assert(((ParseNode->TokenCount == 3) || (ParseNode->TokenCount == 4)) &&
           ((ParseNode->NodeCount == 2) || (ParseNode->NodeCount == 3)));

    Interpreter->LValue = NULL;
    Script = Node->Script;
    ChildIndex = Node->ChildIndex;

    //
    // Evaluate the condition.
    //

    if (Node->ChildIndex == 0) {

        assert(*Result == NULL);

        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[ChildIndex],
                               Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    Token = ParseNode->Tokens[0];
    Condition = *Result;
    *Result = NULL;
    ChalkPopNode(Interpreter);
    Node = NULL;
    if (Token->Value == ChalkTokenIf) {
        Status = 0;
        if (ChalkObjectGetBooleanValue(Condition) != FALSE) {
            Status = ChalkPushNode(Interpreter,
                                   ParseNode->Nodes[ChildIndex],
                                   Script,
                                   FALSE);

        } else {

            //
            // Evaluate the else body if there's an else portion.
            //

            if (ParseNode->NodeCount == 3) {

                assert((ParseNode->TokenCount == 4) &&
                       (ParseNode->NodeCount == 3) &&
                       (ParseNode->Tokens[3]->Value == ChalkTokenElse));

                Status = ChalkPushNode(Interpreter,
                                       ParseNode->Nodes[2],
                                       Script,
                                       FALSE);
            }
        }

    } else {

        //
        // This is where switch would go if implemented.
        //

        assert(FALSE);

        return EINVAL;
    }

    return Status;
}

INT
ChalkVisitIterationStatement (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an iteration statement (for, while, and do-while).

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

    PCHALK_OBJECT Condition;
    PPARSER_NODE ParseNode;
    INT PushIndex;
    INT Status;
    PLEXER_TOKEN Token;

    ParseNode = Node->ParseNode;

    assert(((ParseNode->TokenCount == 3) || (ParseNode->TokenCount == 5)) &&
           (ParseNode->NodeCount >= 2));

    Interpreter->LValue = NULL;
    Token = ParseNode->Tokens[0];
    PushIndex = Node->ChildIndex;
    switch (Token->Value) {
    case ChalkTokenDo:

        //
        // Evaluate the statement and the expression. The form is:
        // DO compound_statement WHILE ( expression ) ;
        //

        if (PushIndex == 2) {
            Condition = *Result;

            assert(Condition != NULL);

            if (ChalkObjectGetBooleanValue(Condition) != FALSE) {
                PushIndex = 0;

            } else {
                PushIndex = -1;
            }
        }

        break;

    case ChalkTokenFor:

        //
        // For loops look like:
        // FOR ( expression_statement expression_statement ) compound_statement
        // FOR ( expression_statement expression_statement expression)
        // compound_statement.
        // Index 0: Just starting out, evaluate the initial statement (push 0).
        // Index 1: Finished the initial statement, evaluate the condition
        // (push 1).
        // Index 2: Finished the expression, if false then exit. If true then
        // execute the compound statement (push N-1).
        // Index N: Finished the compound statement, execute the final
        // expression if it exists (push 2). Go back and execute the condition
        // again.
        // Index 3: Finished the final expression, go execute the condition
        // again (push 1).
        //

        if (PushIndex == 2) {
            Condition = *Result;

            assert(Condition != NULL);

            if (ChalkObjectGetBooleanValue(Condition) != FALSE) {
                PushIndex = ParseNode->NodeCount - 1;

            } else {
                PushIndex = -1;
            }

        } else if (PushIndex == ParseNode->NodeCount) {

            //
            // Push the final expression if there are 4 child nodes, or the
            // condition if there are 3.
            //

            PushIndex = ParseNode->NodeCount - 2;

        //
        // If the final expression just finished, go back and evaluate the
        // condition. This only hits for 4-node for statements.
        //

        } else if (PushIndex == 3) {
            PushIndex = 1;
        }

        break;

    case ChalkTokenWhile:

        //
        // While statements take the form:
        // WHILE ( expression ) compound_statement
        //

        if (PushIndex == 1) {
            Condition = *Result;

            assert(Condition != NULL);

            if (ChalkObjectGetBooleanValue(Condition) == FALSE) {
                PushIndex = -1;
            }

        } else if (PushIndex == 2) {
            PushIndex = 0;
        }

        break;

    default:

        assert(FALSE);

        return EINVAL;
    }

    Status = 0;
    if (PushIndex != -1) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[PushIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex = PushIndex + 1;

    } else {
        ChalkPopNode(Interpreter);
    }

    if (*Result != NULL) {
        ChalkObjectReleaseReference(*Result);
        *Result = NULL;
    }

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
        // Pop nodes off until a function scope is hit.
        //

        Destination = Interpreter->Node;
        while (Destination != NULL) {
            if ((Destination->BaseScope != NULL) &&
                (Destination->BaseScope->Function != FALSE)) {

                break;
            }

            Destination = Destination->Parent;
        }

        if (Destination != NULL) {
            Destination = Destination->Parent;
        }

        break;

    case ChalkTokenBreak:
    case ChalkTokenContinue:

        //
        // Go find the next while, do-while, or for loop.
        //

        Destination = Interpreter->Node;
        while (Destination != NULL) {
            ParseNode = Destination->ParseNode;
            if (ParseNode->GrammarElement == ChalkNodeIterationStatement) {
                break;
            }

            Destination = Destination->Parent;
        }

        if (Token->Value == ChalkTokenBreak) {
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

