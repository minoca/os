/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    expr.c

Abstract:

    This module implements support for expressions in the setup interpreter.

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

#include "../setup.h"
#include <minoca/lib/yy.h>

//
// --------------------------------------------------------------------- Macros
//

#define SETUP_LOGICAL_OPERATOR(_Operator) \
    (((_Operator) == SetupTokenLogicalAnd) || \
     ((_Operator) == SetupTokenLogicalOr) || \
     ((_Operator) == SetupTokenLogicalNot) || \
     ((_Operator) == SetupTokenLessThan) || \
     ((_Operator) == SetupTokenGreaterThan) || \
     ((_Operator) == SetupTokenLessOrEqual) || \
     ((_Operator) == SetupTokenGreaterOrEqual) || \
     ((_Operator) == SetupTokenIsEqual) || \
     ((_Operator) == SetupTokenIsNotEqual))

#define SETUP_UNARY_OPERATOR(_Operator) \
    (((_Operator) == SetupTokenMinus) || \
     ((_Operator) == SetupTokenLogicalNot) || \
     ((_Operator) == SetupTokenBitNot) || \
     ((_Operator) == SetupTokenIncrement) || \
     ((_Operator) == SetupTokenDecrement))

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
SetupPerformArithmetic (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Left,
    PSETUP_OBJECT Right,
    SETUP_TOKEN_TYPE Operator,
    PSETUP_OBJECT *Result
    );

INT
SetupIntegerMath (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Left,
    PSETUP_OBJECT Right,
    SETUP_TOKEN_TYPE Operator,
    PSETUP_OBJECT *Result
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SetupVisitPostfixExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    PSETUP_DICT_ENTRY DictEntry;
    PSETUP_OBJECT Expression;
    PSETUP_OBJECT ExpressionValue;
    PSETUP_OBJECT Key;
    LONGLONG ListIndex;
    PSETUP_OBJECT NewExpression;
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
        case SetupTokenOpenBracket:

            assert(NodeIndex < ParseNode->NodeCount);

            Key = Node->Results[NodeIndex];
            NodeIndex += 1;

            //
            // Dereference the value if needed.
            //

            ExpressionValue = Expression;
            if (Expression->Header.Type == SetupObjectReference) {
                ExpressionValue = Expression->Reference.Value;
            }

            //
            // Index into a list.
            //

            if (ExpressionValue->Header.Type == SetupObjectList) {
                if (Key->Header.Type != SetupObjectInteger) {
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

                    NewExpression = SetupCreateInteger(0);
                    if (NewExpression == NULL) {
                        Status = ENOMEM;
                        goto VisitPostfixExpressionEnd;
                    }

                    Status = SetupListSetElement(ExpressionValue,
                                                 ListIndex,
                                                 NewExpression);

                    if (Status != 0)  {
                        SetupObjectReleaseReference(NewExpression);
                        goto VisitPostfixExpressionEnd;
                    }

                } else {
                    NewExpression = ExpressionValue->List.Array[ListIndex];
                    SetupObjectAddReference(NewExpression);
                }

                SetupObjectReleaseReference(Expression);
                Expression = NewExpression;

            //
            // Key into a dictionary.
            //

            } else if (ExpressionValue->Header.Type == SetupObjectDict) {
                DictEntry = SetupDictLookup(ExpressionValue, Key);
                if (DictEntry != NULL) {
                    NewExpression = DictEntry->Value;
                    SetupObjectAddReference(NewExpression);

                } else {

                    //
                    // Add a zero there if there wasn't one before.
                    //

                    NewExpression = SetupCreateInteger(0);
                    if (NewExpression == NULL) {
                        Status = ENOMEM;
                        goto VisitPostfixExpressionEnd;
                    }

                    Status = SetupDictSetElement(ExpressionValue,
                                                 Key,
                                                 NewExpression);

                    if (Status != 0)  {
                        SetupObjectReleaseReference(NewExpression);
                        goto VisitPostfixExpressionEnd;
                    }
                }

                SetupObjectReleaseReference(Expression);
                Expression = NewExpression;

            } else {
                fprintf(stderr,
                        "Cannot index into %s.\n",
                        SetupObjectTypeNames[ExpressionValue->Header.Type]);

                Status = EINVAL;
                goto VisitPostfixExpressionEnd;
            }

            break;

        //
        // Ignore the close bracket that came with an earlier open bracket.
        //

        case SetupTokenCloseBracket:
            break;

        case SetupTokenIncrement:
        case SetupTokenDecrement:
            Status = SetupPerformArithmetic(Interpreter,
                                            Expression,
                                            NULL,
                                            Token->Value,
                                            &NewExpression);

            if (Status != 0) {
                goto VisitPostfixExpressionEnd;
            }

            SetupObjectReleaseReference(Expression);
            Expression = NewExpression;
            break;
        }
    }

    Status = 0;

VisitPostfixExpressionEnd:
    if (Status != 0) {
        if (Expression != NULL) {
            SetupObjectReleaseReference(Expression);
            Expression = NULL;
        }
    }

    *Result = Expression;
    return Status;
}

INT
SetupVisitUnaryExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;
    PPARSER_NODE UnaryOperatorNode;

    ParseNode = Node->ParseNode;

    assert(ParseNode->NodeCount == 2);

    UnaryOperatorNode = ParseNode->Nodes[0];

    assert((UnaryOperatorNode->NodeCount == 0) &&
           (UnaryOperatorNode->TokenCount == 1));

    Token = UnaryOperatorNode->Tokens[0];
    Status = SetupPerformArithmetic(Interpreter,
                                    Node->Results[1],
                                    NULL,
                                    Token->Value,
                                    Result);

    return Status;
}

INT
SetupVisitUnaryOperator (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return 0;
}

INT
SetupVisitMultiplicativeExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    PSETUP_OBJECT Answer;
    PSETUP_OBJECT Left;
    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;
    ULONG TokenIndex;

    Answer = NULL;
    ParseNode = Node->ParseNode;
    Status = STATUS_SUCCESS;

    assert((ParseNode->NodeCount == ParseNode->TokenCount + 1) &&
           (ParseNode->TokenCount >= 1));

    //
    // Go from left to right processing equivalent operators (ie x + y - z).
    //

    Left = Node->Results[0];
    for (TokenIndex = 0; TokenIndex < ParseNode->TokenCount; TokenIndex += 1) {
        Token = ParseNode->Tokens[TokenIndex];
        Status = SetupPerformArithmetic(Interpreter,
                                        Left,
                                        Node->Results[TokenIndex + 1],
                                        Token->Value,
                                        &Answer);

        if (Left != Node->Results[0]) {
            SetupObjectReleaseReference(Left);
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
SetupVisitAdditiveExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitShiftExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitRelationalExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitEqualityExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitAndExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitExclusiveOrExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitInclusiveOrExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitLogicalAndExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitLogicalOrExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return SetupVisitMultiplicativeExpression(Interpreter, Node, Result);
}

INT
SetupVisitConditionalExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    if (SetupObjectGetBooleanValue(Node->Results[0]) != FALSE) {
        *Result = Node->Results[1];
        Node->Results[1] = NULL;

    } else {
        *Result = Node->Results[2];
        Node->Results[2] = NULL;
    }

    return 0;
}

INT
SetupVisitAssignmentExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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
    PSETUP_OBJECT Destination;
    SETUP_TOKEN_TYPE Operator;
    PPARSER_NODE ParseNode;
    INT Status;
    PLEXER_TOKEN Token;
    PSETUP_OBJECT Value;

    ParseNode = Node->ParseNode;

    assert((ParseNode->NodeCount == 3) && (ParseNode->TokenCount == 0));

    AssignmentOperator = ParseNode->Nodes[1];

    assert((AssignmentOperator->NodeCount == 0) &&
           (AssignmentOperator->TokenCount == 1));

    Token = AssignmentOperator->Tokens[0];
    if (Token->Value == SetupTokenAssign) {
        Value = Node->Results[2];
        Node->Results[2] = NULL;

    } else {
        switch (Token->Value) {
        case SetupTokenLeftAssign:
            Operator = SetupTokenLeftShift;
            break;

        case SetupTokenRightAssign:
            Operator = SetupTokenRightShift;
            break;

        case SetupTokenAddAssign:
            Operator = SetupTokenPlus;
            break;

        case SetupTokenSubtractAssign:
            Operator = SetupTokenMinus;
            break;

        case SetupTokenMultiplyAssign:
            Operator = SetupTokenAsterisk;
            break;

        case SetupTokenDivideAssign:
            Operator = SetupTokenDivide;
            break;

        case SetupTokenModuloAssign:
            Operator = SetupTokenModulo;
            break;

        case SetupTokenAndAssign:
            Operator = SetupTokenBitAnd;
            break;

        case SetupTokenOrAssign:
            Operator = SetupTokenBitOr;
            break;

        case SetupTokenXorAssign:
            Operator = SetupTokenXor;
            break;

        default:

            assert(FALSE);

            return EINVAL;
        }

        Status = SetupPerformArithmetic(Interpreter,
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

    Destination = Node->Results[0];
    Status = SetupObjectAssign(Destination, Value);
    if (Status == 0) {
        Node->Results[0] = NULL;
        *Result = Destination;

    } else {
        Destination = NULL;
    }

    SetupObjectReleaseReference(Value);
    return Status;
}

INT
SetupVisitAssignmentOperator (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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

    return 0;
}

INT
SetupVisitExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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
    // assignment expression.
    //

    *Result = Node->Results[0];
    Node->Results[0] = NULL;
    return 0;
}

INT
SetupVisitExpressionStatement (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
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
    // The statement itself does not evaluate to anything.
    //

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupPerformArithmetic (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Left,
    PSETUP_OBJECT Right,
    SETUP_TOKEN_TYPE Operator,
    PSETUP_OBJECT *Result
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

    PSETUP_OBJECT LeftValue;
    PSETUP_OBJECT RightValue;
    INT Status;
    SETUP_OBJECT_TYPE Type;

    //
    // Lists and dictionaries can be added.
    //

    if (Operator == SetupTokenPlus) {
        LeftValue = Left;
        if (LeftValue->Header.Type == SetupObjectReference) {
            LeftValue = LeftValue->Reference.Value;
        }

        RightValue = Right;
        if (RightValue->Header.Type == SetupObjectReference) {
            RightValue = RightValue->Reference.Value;
        }

        Type = LeftValue->Header.Type;
        if (Type == RightValue->Header.Type) {
            if (Type == SetupObjectList) {
                return SetupListAdd(LeftValue, RightValue, Result);

            } else if (Type == SetupObjectDict) {
                return SetupDictAdd(LeftValue, RightValue, Result);

            } else if (Type == SetupObjectString) {
                return SetupStringAdd(LeftValue, RightValue, Result);
            }
        }
    }

    Status = SetupIntegerMath(Interpreter,
                              Left,
                              Right,
                              Operator,
                              Result);

    return Status;
}

INT
SetupIntegerMath (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_OBJECT Left,
    PSETUP_OBJECT Right,
    SETUP_TOKEN_TYPE Operator,
    PSETUP_OBJECT *Result
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

    if (SETUP_LOGICAL_OPERATOR(Operator)) {
        LeftValue = SetupObjectGetBooleanValue(Left);
        if (Operator != SetupTokenLogicalNot) {

            assert(Right != NULL);

            RightValue = SetupObjectGetBooleanValue(Right);
        }

    } else {
        if (Left->Header.Type != SetupObjectInteger) {
            fprintf(stderr,
                    "Error: Operator expects integer, got %s.\n",
                    SetupObjectTypeNames[Left->Header.Type]);

            return EINVAL;
        }

        LeftValue = Left->Integer.Value;

        //
        // Get the right value for binary operators. Minus is a little tricky
        // since it can be both unary and binary.
        //

        RightValue = 0;
        if ((!SETUP_UNARY_OPERATOR(Operator)) ||
            ((Operator == SetupTokenMinus) && (Right != NULL))) {

            if (Right->Header.Type != SetupObjectInteger) {
                fprintf(stderr,
                        "Error: Operator expects integer, got %s.\n",
                        SetupObjectTypeNames[Right->Header.Type]);

                return EINVAL;
            }

            RightValue = Right->Integer.Value;
        }
    }

    switch (Operator) {
    case SetupTokenIncrement:
        ResultValue = LeftValue + 1;
        break;

    case SetupTokenDecrement:
        ResultValue = LeftValue - 1;
        break;

    case SetupTokenPlus:
        ResultValue = LeftValue + RightValue;
        break;

    case SetupTokenMinus:
        if (Right != NULL) {
            ResultValue = LeftValue - RightValue;

        } else {
            ResultValue = -LeftValue;
        }

        break;

    case SetupTokenAsterisk:
        ResultValue = LeftValue * RightValue;
        break;

    case SetupTokenDivide:
    case SetupTokenModulo:
        if (RightValue == 0) {
            fprintf(stderr, "Error: Divide by zero.\n");
            return ERANGE;
        }

        if (Operator == SetupTokenDivide) {
            ResultValue = LeftValue / RightValue;

        } else {
            ResultValue = LeftValue % RightValue;
        }

        break;

    case SetupTokenLeftShift:
        ResultValue = LeftValue << RightValue;
        break;

    case SetupTokenRightShift:
        ResultValue = LeftValue >> RightValue;
        break;

    case SetupTokenBitAnd:
        ResultValue = LeftValue & RightValue;
        break;

    case SetupTokenBitOr:
        ResultValue = LeftValue | RightValue;
        break;

    case SetupTokenXor:
        ResultValue = LeftValue ^ RightValue;
        break;

    case SetupTokenBitNot:
        ResultValue = ~LeftValue;
        break;

    case SetupTokenLogicalNot:
        ResultValue = !LeftValue;
        break;

    case SetupTokenLogicalAnd:
        ResultValue = (LeftValue && RightValue);
        break;

    case SetupTokenLogicalOr:
        ResultValue = (LeftValue || RightValue);
        break;

    case SetupTokenLessThan:
        ResultValue = (LeftValue < RightValue);
        break;

    case SetupTokenGreaterThan:
        ResultValue = (LeftValue > RightValue);
        break;

    case SetupTokenLessOrEqual:
        ResultValue = (LeftValue <= RightValue);
        break;

    case SetupTokenGreaterOrEqual:
        ResultValue = (LeftValue >= RightValue);
        break;

    case SetupTokenIsEqual:
        ResultValue = (LeftValue == RightValue);
        break;

    case SetupTokenIsNotEqual:
        ResultValue = (LeftValue != RightValue);
        break;

    default:

        assert(FALSE);

        ResultValue = -1ULL;
        break;
    }

    *Result = SetupCreateInteger(ResultValue);
    if (*Result == NULL) {
        return ENOMEM;
    }

    return 0;
}

