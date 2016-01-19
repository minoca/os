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
    PCHALK_OBJECT ArgumentList,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine pushes a new function invocation on the interpreter stack.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Function - Supplies a pointer to the function object to execute.

    ArgumentList - Supplies a pointer to the argument values.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    PCHALK_OBJECT ArgumentNames;
    ULONG Count;
    PVOID FunctionContext;
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
    assert(*Result == NULL);

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

    //
    // If there's a C function to call, give it a ring.
    //

    if (Function->Function.CFunction != NULL) {
        FunctionContext = Function->Function.CFunctionContext;
        Status = Function->Function.CFunction(Interpreter,
                                              FunctionContext,
                                              Result);

        if (Status != 0) {
            goto InvokeFunctionEnd;
        }

        if (*Result == NULL) {
            *Result = ChalkCreateInteger(0);
            if (*Result == NULL) {
                Status = ENOMEM;
                goto InvokeFunctionEnd;
            }
        }

        ChalkPopNode(Interpreter);
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
    PCHALK_OBJECT Iteratee;
    PCHALK_OBJECT Iteration;
    PVOID *IteratorContext;
    PCHALK_OBJECT Name;
    PPARSER_NODE ParseNode;
    INT PushIndex;
    INT Status;
    PLEXER_TOKEN Token;
    PSTR TokenString;

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
        // For loops are overloaded for iteration. Handle that case in here,
        // detected by the fact that there are more tokens in the IN case.
        // This takes the form:
        // FOR ( IDENTIFIER IN expression ) compound_statement.
        //

        if (ParseNode->TokenCount == 5) {
            IteratorContext = (PVOID *)&(Node->Data);

            //
            // If the expression hasn't even been evaluated yet, then go get it.
            //

            if (PushIndex == 0) {
                break;

            //
            // If the expression was just evaluated, go get it and squirrel it
            // away.
            //

            } else if (PushIndex == 1) {
                Iteratee = *Result;

                assert(Iteratee != NULL);

                Node->Results[0] = Iteratee;
                *Result = NULL;
                if (Iteratee->Header.Type == ChalkObjectList) {
                    ChalkListInitializeIterator(Iteratee, IteratorContext);

                } else if (Iteratee->Header.Type == ChalkObjectDict) {
                    ChalkDictInitializeIterator(Iteratee, IteratorContext);

                } else {
                    fprintf(stderr,
                            "Error: %s is not iterable.\n",
                            ChalkObjectTypeNames[Iteratee->Header.Type]);

                    return EINVAL;
                }
            }

            //
            // Get the next value, and set it in the new variable.
            //

            Iteratee = Node->Results[0];
            if (Iteratee->Header.Type == ChalkObjectList) {
                Status = ChalkListIterate(Iteratee,
                                          IteratorContext,
                                          &Iteration);

                if (Iteration == NULL) {
                    ChalkListDestroyIterator(Iteratee, IteratorContext);
                    PushIndex = -1;
                    break;
                }

            } else if (Iteratee->Header.Type == ChalkObjectDict) {
                Status = ChalkDictIterate(Iteratee,
                                          IteratorContext,
                                          &Iteration);

                if (Iteration == NULL) {
                    ChalkDictDestroyIterator(Iteratee, IteratorContext);
                    PushIndex = -1;
                    break;
                }

            } else {

                assert(FALSE);

                Iteration = NULL;
                Status = EINVAL;
            }

            if (Status != 0) {
                *IteratorContext = NULL;
                return Status;
            }

            //
            // Get the identifier name string.
            //

            Token = ParseNode->Tokens[2];

            assert(Token->Value == ChalkTokenIdentifier);

            TokenString = Node->Script->Data + Token->Position;
            Name = ChalkCreateString(TokenString, Token->Size);
            if (Name == NULL) {
                Status = ENOMEM;
                return Status;
            }

            Status = ChalkSetVariable(Interpreter, Name, Iteration, NULL);
            ChalkObjectReleaseReference(Name);
            if (Status != 0) {
                return Status;
            }

            //
            // With the variable set for this iteration, go execute the
            // compound statement.
            //

            PushIndex = 1;

        //
        // Handle a normal for loop.
        //

        } else {

            assert(ParseNode->TokenCount == 3);

            //
            // For loops look like:
            // FOR ( expression_statement expression_statement )
            //     compound_statement
            // FOR ( expression_statement expression_statement expression)
            //     compound_statement.
            // Index 0: Just starting out, evaluate the initial statement
            // (push 0).
            // Index 1: Finished the initial statement, evaluate the condition
            // (push 1).
            // Index 2: Finished the expression, if false then exit. If true
            // then execute the compound statement (push N-1).
            // Index N: Finished the compound statement, execute the final
            // expression if it exists (push 2). Go back and execute the
            // condition again.
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

