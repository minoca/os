/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    expr.c

Abstract:

    This module implements support for expressions in the Chalk interpreter.

Author:

    Evan Green 14-Oct-2015

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

#include "chalkp.h"

//
// --------------------------------------------------------------------- Macros
//

#define CHALK_LOGICAL_OPERATOR(_Operator) \
    (((_Operator) == ChalkTokenLogicalAnd) || \
     ((_Operator) == ChalkTokenLogicalOr) || \
     ((_Operator) == ChalkTokenLogicalNot) || \
     ((_Operator) == ChalkTokenLessThan) || \
     ((_Operator) == ChalkTokenGreaterThan) || \
     ((_Operator) == ChalkTokenLessOrEqual) || \
     ((_Operator) == ChalkTokenGreaterOrEqual) || \
     ((_Operator) == ChalkTokenIsEqual) || \
     ((_Operator) == ChalkTokenIsNotEqual))

#define CHALK_UNARY_OPERATOR(_Operator) \
    (((_Operator) == ChalkTokenMinus) || \
     ((_Operator) == ChalkTokenLogicalNot) || \
     ((_Operator) == ChalkTokenBitNot) || \
     ((_Operator) == ChalkTokenIncrement) || \
     ((_Operator) == ChalkTokenDecrement))

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
ChalkDereference (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Object,
    PCHALK_OBJECT Index,
    PCHALK_OBJECT *Result
    );

INT
ChalkPerformArithmetic (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Left,
    PCHALK_OBJECT Right,
    CHALK_TOKEN_TYPE Operator,
    PCHALK_OBJECT *Result
    );

INT
ChalkIntegerMath (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Left,
    PCHALK_OBJECT Right,
    CHALK_TOKEN_TYPE Operator,
    PCHALK_OBJECT *Result
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ChalkVisitPostfixExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a postfix expression.

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

    PCHALK_OBJECT ArgumentList;
    PCHALK_OBJECT Expression;
    PCHALK_OBJECT Key;
    PCHALK_OBJECT *LValue;
    PCHALK_OBJECT NewExpression;
    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;

    ParseNode = Node->ParseNode;
    Status = 0;
    if (Node->ChildIndex != 0) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
    }

    //
    // If not all the child elements have been evaluated yet, go get them.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    //
    // A postfix expression should be of the form node[node], node(node),
    // or node++/--.
    //

    assert(((ParseNode->NodeCount == 1) || (ParseNode->NodeCount == 2)) &&
           ((ParseNode->TokenCount == 1) || (ParseNode->TokenCount == 2)));

    Expression = Node->Results[0];
    Token = ParseNode->Tokens[0];
    switch (Token->Value) {
    case ChalkTokenOpenBracket:

        assert((ParseNode->NodeCount == 2) && (ParseNode->TokenCount == 2));

        Key = Node->Results[1];
        Status = ChalkDereference(Interpreter,
                                  Expression,
                                  Key,
                                  Result);

        if (Status != 0) {
            goto VisitPostfixExpressionEnd;
        }

        break;

    case ChalkTokenOpenParentheses:

        assert((ParseNode->NodeCount == 2) && (ParseNode->TokenCount == 2));

        //
        // Pop the current node and push the function invocation.
        //

        ArgumentList = Node->Results[1];
        Node->Results[1] = NULL;
        ChalkPopNode(Interpreter);
        Status = ChalkInvokeFunction(Interpreter, Expression, ArgumentList);
        ChalkObjectReleaseReference(ArgumentList);
        goto VisitPostfixExpressionEnd;

    case ChalkTokenIncrement:
    case ChalkTokenDecrement:
        LValue = Interpreter->LValue;
        if (LValue == NULL) {
            fprintf(stderr, "Error: lvalue required for unary operator.\n");
            Status = EINVAL;
            goto VisitPostfixExpressionEnd;
        }

        Status = ChalkPerformArithmetic(Interpreter,
                                        Expression,
                                        NULL,
                                        Token->Value,
                                        &NewExpression);

        if (Status != 0) {
            goto VisitPostfixExpressionEnd;
        }

        //
        // Assign this value back. Also clear the LValue, as a++ = 4 is illegal.
        //

        assert(*LValue == Expression);

        *LValue = NewExpression;
        Interpreter->LValue = NULL;

        //
        // For post-increment/decrement, return the value before the operation.
        //

        *Result = Expression;
        Node->Results[0] = NULL;
        break;

    default:

        assert(FALSE);

        Status = EINVAL;
        goto VisitPostfixExpressionEnd;
    }

    ChalkPopNode(Interpreter);
    Status = 0;

VisitPostfixExpressionEnd:
    return Status;
}

INT
ChalkVisitArgumentExpressionList (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an argument expression list.

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

    PPARSER_NODE ParseNode;
    INT Status;

    Interpreter->LValue = NULL;
    ParseNode = Node->ParseNode;
    if (Node->ChildIndex != 0) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
    }

    //
    // If not all the child elements have been evaluated yet, go get them.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    //
    // Create a list from here of all the argument values.
    //

    *Result = ChalkCreateList(Node->Results, ParseNode->NodeCount);
    if (*Result == NULL) {
        return ENOMEM;
    }

    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitUnaryExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a unary expression.

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

    PCHALK_OBJECT *LValue;
    CHALK_TOKEN_TYPE Operator;
    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;
    PPARSER_NODE UnaryOperatorNode;

    ParseNode = Node->ParseNode;
    LValue = Interpreter->LValue;
    if (Node->ChildIndex != 0) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
    }

    //
    // If not all the child elements have been evaluated yet, go get them.
    // Don't bother pushing the unary operator, it's nothing but tokens.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        if (ParseNode->Nodes[Node->ChildIndex]->GrammarElement ==
            ChalkNodeUnaryOperator) {

            assert(ParseNode->NodeCount == 2);

            Node->ChildIndex += 1;
        }

        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    //
    // If there are two nodes, it's of the form unary_operator unary_expression.
    //

    if (ParseNode->NodeCount == 2) {
        UnaryOperatorNode = ParseNode->Nodes[0];

        assert((UnaryOperatorNode->NodeCount == 0) &&
               (UnaryOperatorNode->TokenCount == 1));

        Token = UnaryOperatorNode->Tokens[0];

    //
    // Otherwise, it must be of the form INC/DEC_OP unary_expression.
    //

    } else {

        assert((ParseNode->NodeCount == 1) &&
               (ParseNode->TokenCount == 1));

        Token = ParseNode->Tokens[0];
    }

    Operator = Token->Value;
    Status = ChalkPerformArithmetic(Interpreter,
                                    Node->Results[ParseNode->NodeCount - 1],
                                    NULL,
                                    Operator,
                                    Result);

    if (Status != 0) {
        return Status;
    }

    //
    // Assign the object back for increment and decrement.
    //

    if ((Operator == ChalkTokenIncrement) ||
        (Operator == ChalkTokenDecrement)) {

        if (LValue == NULL) {
            fprintf(stderr, "Error: lvalue required for unary operator.\n");
            ChalkObjectReleaseReference(*Result);
            *Result = NULL;
            return EINVAL;
        }

        if (*LValue != NULL) {
            ChalkObjectReleaseReference(*LValue);
        }

        *LValue = *Result;
        ChalkObjectAddReference(*Result);
    }

    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitUnaryOperator (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a unary operator.

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

    assert(FALSE);

    return 0;
}

INT
ChalkVisitMultiplicativeExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a multiplicative expression.

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

    PCHALK_OBJECT Answer;
    PCHALK_OBJECT Left;
    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;
    ULONG TokenIndex;

    Answer = NULL;
    ParseNode = Node->ParseNode;

    //
    // Multiplicative expressions are not assignable (ie. a * b = 4 is illegal).
    //

    Interpreter->LValue = NULL;
    if (Node->ChildIndex != 0) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
    }

    //
    // If not all the child elements have been evaluated yet, go get them.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    Status = 0;

    assert((ParseNode->NodeCount == ParseNode->TokenCount + 1) &&
           (ParseNode->TokenCount >= 1));

    //
    // Go from left to right processing equivalent operators (ie x + y - z).
    //

    Left = Node->Results[0];
    for (TokenIndex = 0; TokenIndex < ParseNode->TokenCount; TokenIndex += 1) {
        Token = ParseNode->Tokens[TokenIndex];
        Status = ChalkPerformArithmetic(Interpreter,
                                        Left,
                                        Node->Results[TokenIndex + 1],
                                        Token->Value,
                                        &Answer);

        if (Left != Node->Results[0]) {
            ChalkObjectReleaseReference(Left);
        }

        if (Status != 0) {
            Answer = NULL;
            break;
        }

        Left = Answer;
    }

    *Result = Answer;
    if (Status == 0) {
        ChalkPopNode(Interpreter);
    }

    return Status;
}

INT
ChalkVisitAdditiveExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an additive expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitShiftExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a shift expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitRelationalExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a relational expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitEqualityExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an equality expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitAndExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an and expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitExclusiveOrExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an exclusive or expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitInclusiveOrExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an inclusive or expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitLogicalAndExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a logical and expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitLogicalOrExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a logical or expression.

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

    return ChalkVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
ChalkVisitConditionalExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates a conditional expression.

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

    PPARSER_NODE ParseNode;
    INT Status;

    ParseNode = Node->ParseNode;

    assert((ParseNode->TokenCount == 2) && (ParseNode->NodeCount == 3));

    //
    // If the condition has been evaluated, find out what it is.
    //

    if (Node->ChildIndex == 1) {
        Node->Results[Node->ChildIndex - 1] = *Result;
        *Result = NULL;
        if (ChalkObjectGetBooleanValue(Node->Results[0]) != FALSE) {
            Node->ChildIndex = 1;

        } else {
            Node->ChildIndex = 2;
        }

        Interpreter->LValue = NULL;
    }

    //
    // Evaluate either the conditional or the result.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        if (Node->ChildIndex == 0) {
            Node->ChildIndex = 1;

        } else {

            //
            // Jump to the end if the result is being evaluated.
            //

            Node->ChildIndex = ParseNode->NodeCount;
        }

        return Status;
    }

    assert(*Result != NULL);

    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitAssignmentExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an assignment expression.

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

    PPARSER_NODE AssignmentOperator;
    PCHALK_OBJECT *LValue;
    CHALK_TOKEN_TYPE Operator;
    PPARSER_NODE ParseNode;
    ULONG PushIndex;
    INT Status;
    PLEXER_TOKEN Token;
    PCHALK_OBJECT Value;

    ParseNode = Node->ParseNode;

    //
    // Evaluate the expression first and then the lvalue.
    //

    switch (Node->ChildIndex) {
    case 0:

        assert((ParseNode->NodeCount == 3) && (*Result == NULL));

        PushIndex = 2;
        break;

    case 1:

        assert(*Result != NULL);

        Node->Results[2] = *Result;
        Interpreter->LValue = NULL;
        PushIndex = 0;
        break;

    case 2:

        assert(*Result != NULL);

        Node->Results[0] = *Result;
        PushIndex = -1;
        break;

    default:

        assert(FALSE);

        return EINVAL;
    }

    *Result = NULL;
    if (PushIndex != (ULONG)-1) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[PushIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    LValue = Interpreter->LValue;
    if (LValue == NULL) {
        fprintf(stderr, "Error: Object is not assignable.\n");
        return EINVAL;
    }

    Status = 0;
    AssignmentOperator = ParseNode->Nodes[1];

    assert((AssignmentOperator->GrammarElement ==
            ChalkNodeAssignmentOperator) &&
           (AssignmentOperator->NodeCount == 0) &&
           (AssignmentOperator->TokenCount == 1));

    Token = AssignmentOperator->Tokens[0];
    if (Token->Value == ChalkTokenAssign) {
        Value = Node->Results[2];
        Node->Results[2] = NULL;

    } else {
        switch (Token->Value) {
        case ChalkTokenLeftAssign:
            Operator = ChalkTokenLeftShift;
            break;

        case ChalkTokenRightAssign:
            Operator = ChalkTokenRightShift;
            break;

        case ChalkTokenAddAssign:
            Operator = ChalkTokenPlus;
            break;

        case ChalkTokenSubtractAssign:
            Operator = ChalkTokenMinus;
            break;

        case ChalkTokenMultiplyAssign:
            Operator = ChalkTokenAsterisk;
            break;

        case ChalkTokenDivideAssign:
            Operator = ChalkTokenDivide;
            break;

        case ChalkTokenModuloAssign:
            Operator = ChalkTokenModulo;
            break;

        case ChalkTokenAndAssign:
            Operator = ChalkTokenBitAnd;
            break;

        case ChalkTokenOrAssign:
            Operator = ChalkTokenBitOr;
            break;

        case ChalkTokenXorAssign:
            Operator = ChalkTokenXor;
            break;

        default:

            assert(FALSE);

            return EINVAL;
        }

        Status = ChalkPerformArithmetic(Interpreter,
                                        Node->Results[0],
                                        Node->Results[2],
                                        Operator,
                                        &Value);

        if (Status != 0) {
            return Status;
        }
    }

    //
    // Assign the value to the destination.
    //

    if (*LValue != NULL) {
        ChalkObjectReleaseReference(*LValue);
    }

    *LValue = Value;
    ChalkObjectAddReference(Value);
    *Result = Value;

    //
    // Clear the LValue, as the tree is built in such a way that a = b = 4
    // would be built as:
    // assignment
    //   a    assignment
    //           b  =  4
    // So an assignment expression is never the first node of another
    // assignment expression.
    //

    Interpreter->LValue = NULL;
    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitAssignmentOperator (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an assignment operator.

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

    assert(FALSE);

    return 0;
}

INT
ChalkVisitExpression (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an expression.

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

    PPARSER_NODE ParseNode;
    INT Status;

    ParseNode = Node->ParseNode;

    //
    // Discard all results but the last one.
    //

    if (Node->ChildIndex != ParseNode->NodeCount) {
        if (*Result != NULL) {
            ChalkObjectReleaseReference(*Result);
            *Result = NULL;
        }

        Interpreter->LValue = NULL;
    }

    //
    // If not all the child elements have been evaluated yet, go get them.
    //

    if (Node->ChildIndex < ParseNode->NodeCount) {
        Status = ChalkPushNode(Interpreter,
                               ParseNode->Nodes[Node->ChildIndex],
                               Node->Script,
                               FALSE);

        Node->ChildIndex += 1;
        return Status;
    }

    //
    // The expression evaluates to the last expression in the comma group, so
    // that lvalue and value are propagated up.
    //

    ChalkPopNode(Interpreter);
    return 0;
}

INT
ChalkVisitExpressionStatement (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_NODE Node,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine evaluates an expression statement.

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

    INT Status;

    //
    // Expression statements (; | expression ;) work just like expressions,
    // although there can only ever be one.
    //

    Status = ChalkVisitExpression(Interpreter, Node, Result);
    Interpreter->LValue = NULL;
    if (*Result != NULL) {
        ChalkObjectReleaseReference(*Result);
        *Result = NULL;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ChalkDereference (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Object,
    PCHALK_OBJECT Index,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine dereferences into a list or dictionary.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Object - Supplies a pointer to the object to peek inside of.

    Index - Supplies the index or key object to dereference with.

    Result - Supplies a pointer to the resulting object. The LValue will also
        be saved in the interpreter. This value will have an extra reference on
        it that the caller is responsible for freeing.

Return Value:

    0 on success.

    Returns an error number on failure (such as dereferencing into something
    like an integer).

--*/

{

    PCHALK_DICT_ENTRY DictEntry;
    PCHALK_OBJECT Element;
    LONGLONG ListIndex;
    INT Status;

    assert(*Result == NULL);

    //
    // Index into a list.
    //

    if (Object->Header.Type == ChalkObjectList) {
        if (Index->Header.Type != ChalkObjectInteger) {
            fprintf(stderr, "List index must be an integer.\n");
            Status = EINVAL;
            goto DereferenceEnd;
        }

        ListIndex = Index->Integer.Value;
        if ((ListIndex < 0) || (ListIndex >= MAX_ULONG)) {
            fprintf(stderr,
                    "Invalid list index %I64d.\n",
                    ListIndex);

            Status = EINVAL;
            goto DereferenceEnd;
        }

        //
        // If the value isn't there, create a zero and stick it in
        // there.
        //

        if ((ListIndex >= Object->List.Count) ||
            (Object->List.Array[ListIndex] == NULL)) {

            Element = ChalkCreateInteger(0);
            if (Element == NULL) {
                Status = ENOMEM;
                goto DereferenceEnd;
            }

            Status = ChalkListSetElement(Object, ListIndex, Element);
            if (Status != 0)  {
                ChalkObjectReleaseReference(Element);
                goto DereferenceEnd;
            }

        } else {
            Element = Object->List.Array[ListIndex];
            ChalkObjectAddReference(Element);
        }

        //
        // Set the LValue so this list element can be assigned.
        //

        Interpreter->LValue = &(Object->List.Array[ListIndex]);
        *Result = Element;

    //
    // Key into a dictionary.
    //

    } else if (Object->Header.Type == ChalkObjectDict) {
        DictEntry = ChalkDictLookup(Object, Index);
        if (DictEntry != NULL) {
            Element = DictEntry->Value;
            Interpreter->LValue = &(DictEntry->Value);
            ChalkObjectAddReference(Element);

        } else {

            //
            // Add a zero there if there wasn't one before.
            //

            Element = ChalkCreateInteger(0);
            if (Element == NULL) {
                Status = ENOMEM;
                goto DereferenceEnd;
            }

            Status = ChalkDictSetElement(Object,
                                         Index,
                                         Element,
                                         &(Interpreter->LValue));

            if (Status != 0)  {
                ChalkObjectReleaseReference(Element);
                goto DereferenceEnd;
            }
        }

        *Result = Element;

    } else {
        fprintf(stderr,
                "Cannot index into %s.\n",
                ChalkObjectTypeNames[Object->Header.Type]);

        Status = EINVAL;
        goto DereferenceEnd;
    }

    Status = 0;

DereferenceEnd:
    return Status;
}

INT
ChalkPerformArithmetic (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Left,
    PCHALK_OBJECT Right,
    CHALK_TOKEN_TYPE Operator,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine performs basic math on objects.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Left - Supplies a pointer to the left side of the operation.

    Right - Supplies a pointer to the right side of the operation. This is
        ignored for unary operators.

    Operator - Supplies the operation to perform.

    Result - Supplies a pointer where the result will be returned on success.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    INT Status;
    CHALK_OBJECT_TYPE Type;

    //
    // Lists and dictionaries can be added.
    //

    if (Operator == ChalkTokenPlus) {
        Type = Left->Header.Type;
        if (Type == Right->Header.Type) {
            if (Type == ChalkObjectList) {
                Status = ChalkListAdd(Left, Right);
                if (Status == 0) {
                    *Result = Left;
                    ChalkObjectAddReference(Left);
                }

                return Status;

            } else if (Type == ChalkObjectDict) {
                Status = ChalkDictAdd(Left, Right);
                if (Status == 0) {
                    *Result = Left;
                    ChalkObjectAddReference(Left);
                }

                return Status;

            } else if (Type == ChalkObjectString) {
                return ChalkStringAdd(Left, Right, Result);
            }
        }
    }

    Status = ChalkIntegerMath(Interpreter,
                              Left,
                              Right,
                              Operator,
                              Result);

    return Status;
}

INT
ChalkIntegerMath (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_OBJECT Left,
    PCHALK_OBJECT Right,
    CHALK_TOKEN_TYPE Operator,
    PCHALK_OBJECT *Result
    )

/*++

Routine Description:

    This routine performs basic math on objects.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Left - Supplies a pointer to the left side of the operation.

    Right - Supplies a pointer to the right side of the operation. This is
        ignored for unary operators.

    Operator - Supplies the operation to perform.

    Result - Supplies a pointer where the result will be returned on success.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

{

    LONGLONG LeftValue;
    LONGLONG ResultValue;
    LONGLONG RightValue;

    if (CHALK_LOGICAL_OPERATOR(Operator)) {
        LeftValue = ChalkObjectGetBooleanValue(Left);
        if (Operator != ChalkTokenLogicalNot) {

            assert(Right != NULL);

            RightValue = ChalkObjectGetBooleanValue(Right);
        }

    } else {
        if (Left->Header.Type != ChalkObjectInteger) {
            fprintf(stderr,
                    "Error: Operator expects integer, got %s.\n",
                    ChalkObjectTypeNames[Left->Header.Type]);

            return EINVAL;
        }

        LeftValue = Left->Integer.Value;

        //
        // Get the right value for binary operators. Minus is a little tricky
        // since it can be both unary and binary.
        //

        RightValue = 0;
        if ((!CHALK_UNARY_OPERATOR(Operator)) ||
            ((Operator == ChalkTokenMinus) && (Right != NULL))) {

            if (Right->Header.Type != ChalkObjectInteger) {
                fprintf(stderr,
                        "Error: Operator expects integer, got %s.\n",
                        ChalkObjectTypeNames[Right->Header.Type]);

                return EINVAL;
            }

            RightValue = Right->Integer.Value;
        }
    }

    switch (Operator) {
    case ChalkTokenIncrement:
        ResultValue = LeftValue + 1;
        break;

    case ChalkTokenDecrement:
        ResultValue = LeftValue - 1;
        break;

    case ChalkTokenPlus:
        ResultValue = LeftValue + RightValue;
        break;

    case ChalkTokenMinus:
        if (Right != NULL) {
            ResultValue = LeftValue - RightValue;

        } else {
            ResultValue = -LeftValue;
        }

        break;

    case ChalkTokenAsterisk:
        ResultValue = LeftValue * RightValue;
        break;

    case ChalkTokenDivide:
    case ChalkTokenModulo:
        if (RightValue == 0) {
            fprintf(stderr, "Error: Divide by zero.\n");
            return ERANGE;
        }

        if (Operator == ChalkTokenDivide) {
            ResultValue = LeftValue / RightValue;

        } else {
            ResultValue = LeftValue % RightValue;
        }

        break;

    case ChalkTokenLeftShift:
        ResultValue = LeftValue << RightValue;
        break;

    case ChalkTokenRightShift:
        ResultValue = LeftValue >> RightValue;
        break;

    case ChalkTokenBitAnd:
        ResultValue = LeftValue & RightValue;
        break;

    case ChalkTokenBitOr:
        ResultValue = LeftValue | RightValue;
        break;

    case ChalkTokenXor:
        ResultValue = LeftValue ^ RightValue;
        break;

    case ChalkTokenBitNot:
        ResultValue = ~LeftValue;
        break;

    case ChalkTokenLogicalNot:
        ResultValue = !LeftValue;
        break;

    case ChalkTokenLogicalAnd:
        ResultValue = (LeftValue && RightValue);
        break;

    case ChalkTokenLogicalOr:
        ResultValue = (LeftValue || RightValue);
        break;

    case ChalkTokenLessThan:
        ResultValue = (LeftValue < RightValue);
        break;

    case ChalkTokenGreaterThan:
        ResultValue = (LeftValue > RightValue);
        break;

    case ChalkTokenLessOrEqual:
        ResultValue = (LeftValue <= RightValue);
        break;

    case ChalkTokenGreaterOrEqual:
        ResultValue = (LeftValue >= RightValue);
        break;

    case ChalkTokenIsEqual:
        ResultValue = (LeftValue == RightValue);
        break;

    case ChalkTokenIsNotEqual:
        ResultValue = (LeftValue != RightValue);
        break;

    default:

        assert(FALSE);

        ResultValue = -1ULL;
        break;
    }

    *Result = ChalkCreateInteger(ResultValue);
    if (*Result == NULL) {
        return ENOMEM;
    }

    return 0;
}

