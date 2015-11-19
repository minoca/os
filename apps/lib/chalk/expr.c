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

    PCHALK_DICT_ENTRY DictEntry;
    PCHALK_OBJECT Expression;
    PCHALK_OBJECT ExpressionValue;
    PCHALK_OBJECT Key;
    LONGLONG ListIndex;
    PCHALK_OBJECT *LValue;
    PCHALK_OBJECT NewExpression;
    ULONG NodeIndex;
    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;
    ULONG TokenIndex;

    ParseNode = Node->ParseNode;
    Expression = Node->Results[0];
    Node->Results[0] = NULL;
    NodeIndex = 1;
    for (TokenIndex = 0; TokenIndex < ParseNode->TokenCount; TokenIndex += 1) {
        Token = ParseNode->Tokens[TokenIndex];
        switch (Token->Value) {
        case ChalkTokenOpenBracket:

            assert(NodeIndex < ParseNode->NodeCount);

            Key = Node->Results[NodeIndex];
            NodeIndex += 1;

            //
            // Dereference the value if needed.
            //

            ExpressionValue = Expression;
            if (Expression->Header.Type == ChalkObjectReference) {
                ExpressionValue = Expression->Reference.Value;
            }

            //
            // Index into a list.
            //

            if (ExpressionValue->Header.Type == ChalkObjectList) {
                if (Key->Header.Type != ChalkObjectInteger) {
                    fprintf(stderr, "List index must be an integer.\n");
                    Status = EINVAL;
                    goto VisitPostfixExpressionEnd;
                }

                ListIndex = Key->Integer.Value;
                if ((ListIndex < 0) || (ListIndex >= MAX_ULONG)) {
                    fprintf(stderr,
                            "Invalid list index %I64d.\n",
                            ListIndex);

                    Status = EINVAL;
                    goto VisitPostfixExpressionEnd;
                }

                //
                // If the value isn't there, create a zero and stick it in
                // there.
                //

                if ((ListIndex >= ExpressionValue->List.Count) ||
                    (ExpressionValue->List.Array[ListIndex] == NULL)) {

                    NewExpression = ChalkCreateInteger(0);
                    if (NewExpression == NULL) {
                        Status = ENOMEM;
                        goto VisitPostfixExpressionEnd;
                    }

                    Status = ChalkListSetElement(ExpressionValue,
                                                 ListIndex,
                                                 NewExpression);

                    if (Status != 0)  {
                        ChalkObjectReleaseReference(NewExpression);
                        goto VisitPostfixExpressionEnd;
                    }

                } else {
                    NewExpression = ExpressionValue->List.Array[ListIndex];
                    ChalkObjectAddReference(NewExpression);
                }

                //
                // Set the LValue so this list element can be assigned.
                //

                Node->LValue = &(ExpressionValue->List.Array[ListIndex]);
                ChalkObjectReleaseReference(Expression);
                Expression = NewExpression;

            //
            // Key into a dictionary.
            //

            } else if (ExpressionValue->Header.Type == ChalkObjectDict) {
                DictEntry = ChalkDictLookup(ExpressionValue, Key);
                if (DictEntry != NULL) {
                    NewExpression = DictEntry->Value;
                    Node->LValue = &(DictEntry->Value);
                    ChalkObjectAddReference(NewExpression);

                } else {

                    //
                    // Add a zero there if there wasn't one before.
                    //

                    NewExpression = ChalkCreateInteger(0);
                    if (NewExpression == NULL) {
                        Status = ENOMEM;
                        goto VisitPostfixExpressionEnd;
                    }

                    Status = ChalkDictSetElement(ExpressionValue,
                                                 Key,
                                                 NewExpression,
                                                 &(Node->LValue));

                    if (Status != 0)  {
                        ChalkObjectReleaseReference(NewExpression);
                        goto VisitPostfixExpressionEnd;
                    }
                }

                ChalkObjectReleaseReference(Expression);
                Expression = NewExpression;

            } else {
                fprintf(stderr,
                        "Cannot index into %s.\n",
                        ChalkObjectTypeNames[ExpressionValue->Header.Type]);

                Status = EINVAL;
                goto VisitPostfixExpressionEnd;
            }

            break;

        //
        // Ignore the close bracket that came with an earlier open bracket.
        //

        case ChalkTokenCloseBracket:
            break;

        case ChalkTokenIncrement:
        case ChalkTokenDecrement:
            LValue = Node->LValue;
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
            // Assign this value back, but leave the expression as the original
            // value (post increment/decrement). Also clear the LValue,
            // as a++ = 4 is illegal.
            //

            if (*LValue != NULL) {
                ChalkObjectReleaseReference(*LValue);
            }

            *LValue = NewExpression;
            Node->LValue = NULL;
            break;

        default:

            assert(FALSE);

            Status = EINVAL;
            goto VisitPostfixExpressionEnd;
        }
    }

    Status = 0;

VisitPostfixExpressionEnd:
    if (Status != 0) {
        if (Expression != NULL) {
            ChalkObjectReleaseReference(Expression);
            Expression = NULL;
        }
    }

    *Result = Expression;
    return Status;
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

    LValue = Node->LValue;
    ParseNode = Node->ParseNode;

    //
    // Unary expressions are not assignable (ie ++a = 4 is illegal).
    //

    Node->LValue = NULL;

    assert(ParseNode->NodeCount == 2);

    UnaryOperatorNode = ParseNode->Nodes[0];

    assert((UnaryOperatorNode->NodeCount == 0) &&
           (UnaryOperatorNode->TokenCount == 1));

    Token = UnaryOperatorNode->Tokens[0];
    Operator = Token->Value;
    Status = ChalkPerformArithmetic(Interpreter,
                                    Node->Results[1],
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

    return Status;
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

    assert(Node->LValue == NULL);

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
    Status = STATUS_SUCCESS;

    //
    // Multiplicative expressions are not assignable (ie. a * b = 4 is illegal).
    //

    Node->LValue = NULL;

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

        if (!KSUCCESS(Status)) {
            Answer = NULL;
            break;
        }

        Left = Answer;
    }

    *Result = Answer;
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

    ParseNode = Node->ParseNode;

    assert((ParseNode->TokenCount == 2) && (ParseNode->NodeCount == 3));

    if (ChalkObjectGetBooleanValue(Node->Results[0]) != FALSE) {
        *Result = Node->Results[1];
        Node->Results[1] = NULL;

    } else {
        *Result = Node->Results[2];
        Node->Results[2] = NULL;
    }

    //
    // a ? b : c = 4 is illegal.
    //

    Node->LValue = NULL;
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
    INT Status;
    PLEXER_TOKEN Token;
    PCHALK_OBJECT Value;

    LValue = Node->LValue;
    if (LValue == NULL) {
        fprintf(stderr, "Error: Object is not assignable.\n");
        return EINVAL;
    }

    ParseNode = Node->ParseNode;
    Status = 0;

    assert((ParseNode->NodeCount == 3) && (ParseNode->TokenCount == 0));

    AssignmentOperator = ParseNode->Nodes[1];

    assert((AssignmentOperator->NodeCount == 0) &&
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
    // Clear the LValue, even though the tree is built in such a way that
    // a = b = 4 would be built as:
    // assignment
    //   a    assignment
    //           b  =  4
    // So an assignment expression is never the first node of another
    // assignment expression.
    //

    Node->LValue = NULL;
    return Status;
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

    assert(Node->LValue == NULL);

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

    //
    // The expression is the first result. Anything else is a side effect
    // assignment expression. Allos the LValue to propagate up.
    //

    *Result = Node->Results[0];
    Node->Results[0] = NULL;
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

    //
    // The statement itself does not evaluate to anything, but cannot somehow
    // be assigned to.
    //

    Node->LValue = NULL;
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

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

    PCHALK_OBJECT LeftValue;
    PCHALK_OBJECT RightValue;
    INT Status;
    CHALK_OBJECT_TYPE Type;

    //
    // Lists and dictionaries can be added.
    //

    if (Operator == ChalkTokenPlus) {
        LeftValue = Left;
        if (LeftValue->Header.Type == ChalkObjectReference) {
            LeftValue = LeftValue->Reference.Value;
        }

        RightValue = Right;
        if (RightValue->Header.Type == ChalkObjectReference) {
            RightValue = RightValue->Reference.Value;
        }

        Type = LeftValue->Header.Type;
        if (Type == RightValue->Header.Type) {
            if (Type == ChalkObjectList) {
                Status = ChalkListAdd(LeftValue, RightValue);
                if (Status == 0) {
                    *Result = LeftValue;
                    ChalkObjectAddReference(LeftValue);
                }

                return Status;

            } else if (Type == ChalkObjectDict) {
                Status = ChalkDictAdd(LeftValue, RightValue);
                if (Status == 0) {
                    *Result = LeftValue;
                    ChalkObjectAddReference(LeftValue);
                }

                return Status;

            } else if (Type == ChalkObjectString) {
                return ChalkStringAdd(LeftValue, RightValue, Result);
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

