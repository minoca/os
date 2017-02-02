/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    compexpr.c

Abstract:

    This module implements support for compiling expressions in Chalk.

Author:

    Evan Green 9-Jun-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"
#include <minoca/lib/status.h>
#include <minoca/lib/yy.h>
#include "compiler.h"
#include "lang.h"
#include "compsup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpEmitNumericConstant (
    PCK_COMPILER Compiler,
    CK_VALUE Integer
    );

VOID
CkpCompilePrimaryIdentifier (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    BOOL Store
    );

VOID
CkpReadStringLiteralList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpVisitExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles an expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    BOOL Assign;
    ULONG LastIndex;

    //
    // An expression is either just an assignment expression, or it takes the
    // form expression , assignment_expression. In the form with the comma,
    // the first expression is discarded.
    //

    LastIndex = Node->ChildIndex + Node->Children - 1;
    if (Node->Children > 1) {
        Assign = Compiler->Assign;
        Compiler->Assign = FALSE;
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));
        Compiler->Assign = Assign;
        CkpEmitOp(Compiler, CkOpPop);
    }

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, LastIndex));
    return;
}

VOID
CkpVisitAssignmentExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles an assignment expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    UINTN CheckJump;
    PCK_AST_NODE LeftSide;
    BOOL OldAssign;
    CK_SYMBOL Operator;
    PCK_AST_NODE OperatorNode;
    PLEXER_TOKEN OperatorToken;
    PCK_AST_NODE RightSide;

    if (Node->Children == 1) {
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));
        return;
    }

    //
    // The assignment expression takes the form:
    // unary_expression assignment_operator assignment_expression.
    //

    CK_ASSERT(Node->Children == 3);

    OperatorNode = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1);
    OperatorToken = CK_GET_AST_TOKEN(Compiler, OperatorNode->ChildIndex);
    Operator = OperatorToken->Value;
    LeftSide = CK_GET_AST_NODE(Compiler, Node->ChildIndex);
    RightSide = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2);

    CK_ASSERT((LeftSide->Symbol == CkNodeUnaryExpression) &&
              (RightSide->Symbol == CkNodeAssignmentExpression));

    //
    // For straight assignment (most cases), evaluate the right side, then
    // evaluate the left side with the assignment flag set.
    //

    OldAssign = Compiler->Assign;
    if (Operator == CkTokenAssign) {
        CkpVisitNode(Compiler, RightSide);
        Compiler->Assign = TRUE;
        CkpVisitNode(Compiler, LeftSide);

    //
    // The ?= operator compiles to:
    // LeftSide ? LeftSide : LeftSide = RightSide.
    //

    } else if (Operator == CkTokenNullAssign) {
        CkpVisitNode(Compiler, LeftSide);

        //
        // Test the left side. If it is not false, jump over the assignment. If
        // it is false, the or op pops the value.
        //

        CheckJump = CkpEmitJump(Compiler, CkOpOr);
        CkpVisitNode(Compiler, RightSide);
        Compiler->Assign = TRUE;
        CkpVisitNode(Compiler, LeftSide);
        CkpPatchJump(Compiler, CheckJump);

    //
    // This is an operator assign (like *=).
    //

    } else {

        //
        // Evaluate the left side (receiver), then the right side (argument).
        // Finally, perform a method call for the operator.
        //

        CkpVisitNode(Compiler, LeftSide);
        CkpVisitNode(Compiler, RightSide);
        CkpEmitOperatorCall(Compiler, Operator, 1, FALSE);
        Compiler->Assign = TRUE;
        CkpVisitNode(Compiler, LeftSide);
    }

    Compiler->Assign = OldAssign;
    return;
}

VOID
CkpVisitConditionalExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a conditional expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    UINTN ElseJump;
    UINTN IfJump;

    CK_ASSERT((Node->Children == 1) || (Node->Children == 5));

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));
    if (Node->Children == 5) {
        CkpComplainIfAssigning(Compiler,
                               CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1),
                               "Conditional expression");

        IfJump = CkpEmitJump(Compiler, CkOpJumpIf);
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2));

        //
        // Jump over the else branch.
        //

        ElseJump = CkpEmitJump(Compiler, CkOpJump);
        CkpPatchJump(Compiler, IfJump);
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 4));
        CkpPatchJump(Compiler, ElseJump);
    }

    return;
}

VOID
CkpVisitBinaryExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a binary operator expression. Precedence isn't
    handled here since it's built into the grammar definition. The hierarchy
    of the nodes already reflects the correct precendence.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    UINTN Jump;
    CK_SYMBOL Operator;
    PLEXER_TOKEN OperatorToken;

    CK_ASSERT((Node->Children == 1) || (Node->Children == 3));

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));
    if (Node->Children == 3) {
        OperatorToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1);
        Operator = OperatorToken->Value;
        CkpComplainIfAssigning(Compiler, OperatorToken, "Binary expression");

        //
        // Logical AND and logical OR are handled separately since they contain
        // short circuit logic (and so can't be handled by an operator call).
        //

        Jump = -1;
        if (Operator == CkTokenLogicalOr) {
            Jump = CkpEmitJump(Compiler, CkOpOr);

        } else if (Operator == CkTokenLogicalAnd) {
            Jump = CkpEmitJump(Compiler, CkOpAnd);
        }

        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2));
        if ((Operator == CkTokenLogicalOr) || (Operator == CkTokenLogicalAnd)) {
            CkpPatchJump(Compiler, Jump);

        } else {
            CkpEmitOperatorCall(Compiler, Operator, 1, FALSE);
        }
    }

    return;
}

VOID
CkpVisitUnaryExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a unary expression: [+-~! ++ --] postfix_expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE Expression;
    CK_SYMBOL Operator;
    PCK_AST_NODE OperatorNode;
    PLEXER_TOKEN OperatorToken;

    CK_ASSERT(Node->Symbol == CkNodeUnaryExpression);

    if (Node->Children == 1) {
        Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex);
        CkpVisitNode(Compiler, Expression);
        return;
    }

    CK_ASSERT(Node->Children == 2);

    Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1);
    OperatorNode = CK_GET_AST_NODE(Compiler, Node->ChildIndex);
    if (OperatorNode->Symbol == CkNodeUnaryOperator) {
        OperatorToken = CK_GET_AST_TOKEN(Compiler, OperatorNode->ChildIndex);

    } else {
        OperatorToken = (PLEXER_TOKEN)OperatorNode;
    }

    CkpComplainIfAssigning(Compiler, OperatorToken, "Unary expression");
    Operator = OperatorToken->Value;

    //
    // The operator is either something like +-~!, pre-increment, or
    // pre-decrement. Either way, the resulting expression is the effect after
    // the operator. Evaluate the expression, then call the unary operator
    // function.
    //

    CkpVisitNode(Compiler, Expression);
    CkpEmitOperatorCall(Compiler, Operator, 0, FALSE);

    //
    // For pre-increment and pre-decrement, evaluate the expression again to
    // assign back to it.
    //

    if ((OperatorToken->Value == CkTokenIncrement) ||
        (OperatorToken->Value == CkTokenDecrement)) {

        Compiler->Assign = TRUE;
        CkpVisitNode(Compiler, Expression);
        Compiler->Assign = FALSE;
    }

    return;
}

VOID
CkpVisitPostfixExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a postfix expression: e.id(...), e[...], e(...), e++,
    e--, and just simply e.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE ArgumentsNode;
    BOOL Assign;
    PCK_AST_NODE Expression;
    PLEXER_TOKEN Identifier;
    CK_VALUE IdentifierString;
    CK_OPCODE Op;
    CK_SYMBOL Operator;
    PLEXER_TOKEN OperatorToken;
    CK_FUNCTION_SIGNATURE Signature;
    PLEXER_TOKEN SuperToken;

    CK_ASSERT(Node->Symbol == CkNodePostfixExpression);

    Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex);
    if (Node->Children == 1) {
        CkpVisitNode(Compiler, Expression);
        return;
    }

    //
    // Go get the expression first, without any assignment.
    //

    Assign = Compiler->Assign;
    Compiler->Assign = FALSE;
    CkpVisitNode(Compiler, Expression);
    OperatorToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1);
    Operator = OperatorToken->Value;
    switch (Operator) {

    //
    // For increment and decrement, the expression to return is before the
    // increment/decrement, and it's already on the stack. Evaluate it again,
    // and then call the operator on it. Then evaluate it a third time with
    // assignment.
    //

    case CkTokenIncrement:
    case CkTokenDecrement:
        CkpComplainIfAssigning(Compiler, OperatorToken, "Increment/decrement");
        CkpVisitNode(Compiler, Expression);
        CkpEmitOperatorCall(Compiler, Operator, 0, FALSE);
        Compiler->Assign = TRUE;
        CkpVisitNode(Compiler, Expression);
        Compiler->Assign = Assign;
        CkpEmitOp(Compiler, CkOpPop);
        break;

    //
    // Dot is a bound function call to an instance (ie call
    // mything.somefunc(...)). Parentheses represent an unbound call
    // (ie just got some object back and am now trying to call it).
    //

    case CkTokenDot:

        //
        // Handle just x.y, as that's a getter or setter.
        //

        if (Node->Children == 3) {
            Identifier = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 2);

            CK_ASSERT(Identifier->Value == CkTokenIdentifier);

            //
            // Convert the identifier into a string.
            //

            IdentifierString = CkpStringCreate(
                               Compiler->Parser->Vm,
                               Compiler->Parser->Source + Identifier->Position,
                               Identifier->Size);

            if (!CK_IS_STRING(IdentifierString)) {
                return;
            }

            CkpEmitConstant(Compiler, IdentifierString);

            //
            // If assigning, the rvalue is below the receiver. -1 is the inner
            // [expression], -2 is the receiver, so -3 is the rvalue. After
            // the operator, pop the return value, as the rvalue is still back
            // there.
            //

            if (Assign != FALSE) {
                CkpLoadLocal(Compiler, Compiler->StackSlots - 3);
            }

            CkpEmitOperatorCall(Compiler, Operator, 1, Assign);
            if (Assign != FALSE) {
                CkpEmitOp(Compiler, CkOpPop);
            }

            break;
        }

        //
        // Fall through, as this is a method call (ie x.y(...)).
        //

    case CkTokenOpenParentheses:
        CkpComplainIfAssigning(Compiler, OperatorToken, "Function call");

        //
        // Visit the arguments to push them all on the stack.
        //

        ArgumentsNode = CK_GET_AST_NODE(Compiler,
                                        Node->ChildIndex + Node->Children - 2);

        CkpVisitNode(Compiler, ArgumentsNode);

        //
        // Count the arguments.
        //

        CkZero(&Signature, sizeof(CK_FUNCTION_SIGNATURE));
        while (ArgumentsNode->Children > 1) {

            CK_ASSERT(ArgumentsNode->Symbol == CkNodeArgumentExpressionList);

            Signature.Arity += 1;
            ArgumentsNode = CK_GET_AST_NODE(Compiler,
                                            ArgumentsNode->ChildIndex);
        }

        if (ArgumentsNode->Children > 0) {
            Signature.Arity += 1;
        }

        if (Signature.Arity >= CK_MAX_ARGUMENTS) {
            CkpCompileError(Compiler, OperatorToken, "Too many arguments");
        }

        //
        // For the bound methods, get the method name as part of the signature,
        // and emit that full signature as the call op parameter.
        //

        if (Operator == CkTokenDot) {
            Identifier = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 2);

            CK_ASSERT(Identifier->Value == CkTokenIdentifier);

            Signature.Name = Compiler->Parser->Source + Identifier->Position;
            Signature.Length = Identifier->Size;
            Op = CkOpCall0;
            if ((Expression->Symbol == CkNodePostfixExpression) &&
                (Expression->Children == 1)) {

                Expression = CK_GET_AST_NODE(Compiler, Expression->ChildIndex);
                if (Expression->Symbol == CkNodePrimaryExpression) {
                    SuperToken = CK_GET_AST_TOKEN(Compiler,
                                                  Expression->ChildIndex);

                    if (SuperToken->Value == CkTokenSuper) {
                        Op = CkOpSuperCall0;
                    }
                }
            }

            //
            // Check to see if the primary expression is just a super token. If
            // it is then emit a super call. In all other cases super is
            // equivalent to "this".
            //

            CkpCallSignature(Compiler, Op, &Signature);

        //
        // For indirect calls, the thing to call has already been pushed onto
        // the stack in the receiver slot, as have all the arguments, so all
        // that's left to do is make the call.
        //

        } else {
            CkpEmitByteOp(Compiler, CkOpIndirectCall, Signature.Arity);

            //
            // Manually track stack usage since the op doesn't inherently
            // know it's stack effects.
            //

            Compiler->StackSlots -= Signature.Arity;
        }

        break;

    //
    // Call the subscript operator method.
    //

    case CkTokenOpenBracket:

        CK_ASSERT(Node->Children == 4);

        Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2);
        CkpVisitNode(Compiler, Expression);

        //
        // If assigning, the rvalue is below the receiver. -1 is the inner
        // [expression], -2 is the receiver, so -3 is the rvalue. After
        // the operator, pop the return value, as the rvalue is still back
        // there.
        //

        if (Assign != FALSE) {
            CkpLoadLocal(Compiler, Compiler->StackSlots - 3);
        }

        CkpEmitOperatorCall(Compiler, Operator, 1, Assign);
        if (Assign != FALSE) {
            CkpEmitOp(Compiler, CkOpPop);
        }

        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    Compiler->Assign = Assign;
    return;
}

VOID
CkpVisitPrimaryExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a primary expression: name, number, string, null,
    this, super, true, false, dict, list, and ( expression ).

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    INT Base;
    PCK_AST_NODE Constant;
    PCSTR Digit;
    PLEXER_TOKEN Token;
    CK_VALUE Value;

    //
    // If it's a ( expression ), go visit the inner expression.
    //

    if (Node->Children == 3) {
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1));
        return;
    }

    Token = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex);
    if ((Compiler->Assign != FALSE) && (Token->Value != CkTokenIdentifier)) {
        CkpCompileError(Compiler, Token, "Cannot assign to a constant");
        return;
    }

    switch (Token->Value) {
    case CkTokenIdentifier:
        CkpCompilePrimaryIdentifier(Compiler, Token, Compiler->Assign);
        break;

    case CkTokenConstant:
        Base = 10;
        Digit = Compiler->Parser->Source + Token->Position;
        if (*Digit == '0') {
            Base = 8;
        }

        Value = CkpReadSourceInteger(Compiler, Token, Base);
        CkpEmitNumericConstant(Compiler, Value);
        break;

    case CkTokenHexConstant:
        Value = CkpReadSourceInteger(Compiler, Token, 16);
        CkpEmitNumericConstant(Compiler, Value);
        break;

    case CkTokenBinaryConstant:
        Value = CkpReadSourceInteger(Compiler, Token, 2);
        CkpEmitNumericConstant(Compiler, Value);
        break;

    case CkTokenNull:
        CkpEmitOp(Compiler, CkOpNull);
        break;

    case CkTokenThis:
    case CkTokenSuper:
        CkpLoadThis(Compiler, Token);
        break;

    case CkTokenTrue:
        CkpEmitOp(Compiler, CkOpLiteral0 + 1);
        break;

    case CkTokenFalse:
        CkpEmitOp(Compiler, CkOpLiteral0);
        break;

    default:
        Constant = CK_GET_AST_NODE(Compiler, Node->ChildIndex);
        if ((Constant->Symbol == CkNodeDict) ||
            (Constant->Symbol == CkNodeList)) {

            CkpVisitNode(Compiler, Constant);

        } else if (Constant->Symbol == CkNodeStringLiteralList) {
            CkpReadStringLiteralList(Compiler, Constant);

        } else {

            CK_ASSERT(FALSE);

            CkpEmitOp(Compiler, CkOpNull);
        }

        break;
    }

    return;
}

VOID
CkpVisitDict (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a dictionary constant.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE DictElementList;

    CK_ASSERT(Compiler->Assign == FALSE);

    //
    // Call the list class to create a new list.
    //

    CkpLoadCoreVariable(Compiler, "Dict");
    CkpEmitByteOp(Compiler, CkOpIndirectCall, 0);

    //
    // Visit the children, which contains the elements on the list.
    //

    CK_ASSERT((Node->Symbol == CkNodeDict) && (Node->Children >= 2));

    DictElementList = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1);
    if (DictElementList->Symbol == CkNodeDictElementList) {
        CkpVisitNode(Compiler, DictElementList);
    }

    return;
}

VOID
CkpVisitDictElementList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine visits the dict element list node, which contains the inner
    elements of a defined dictionary.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE CurrentNode;
    ULONG LastChild;
    PCK_AST_NODE NextNode;

    CK_ASSERT(Compiler->Assign == FALSE);

    //
    // Get to the leftmost node.
    //

    CurrentNode = Node;
    while (CurrentNode->Children > 1) {
        NextNode = CK_GET_AST_NODE(Compiler, CurrentNode->ChildIndex);

        CK_ASSERT((CurrentNode->Symbol == NextNode->Symbol) &&
                  (NextNode < CurrentNode));

        CurrentNode = NextNode;
    }

    //
    // Loop up the children evaluating the expression, and then calling the
    // add entry function. This function is specially crafted for
    // initialization as the return value is always the dictionary iself.
    //

    while (TRUE) {
        LastChild = CurrentNode->ChildIndex + CurrentNode->Children - 1;
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, LastChild));
        CkpEmitMethodCall(Compiler, 2, "set@2", 5);
        if (CurrentNode == Node) {
            break;
        }

        CurrentNode = CK_GET_AST_NODE(Compiler, CurrentNode->Parent);
    }

    return;
}

VOID
CkpVisitList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a list constant.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE ListElementList;

    CK_ASSERT(Compiler->Assign == FALSE);

    //
    // Call the list class to create a new list.
    //

    CkpLoadCoreVariable(Compiler, "List");
    CkpEmitByteOp(Compiler, CkOpIndirectCall, 0);

    //
    // Visit the children, which contains the elements on the list.
    //

    CK_ASSERT((Node->Symbol == CkNodeList) && (Node->Children >= 2));

    ListElementList = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1);
    if (ListElementList->Symbol == CkNodeListElementList) {
        CkpVisitNode(Compiler, ListElementList);
    }

    return;
}

VOID
CkpVisitListElementList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine visits the list element list node, which contains the inner
    elements of a defined list.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE CurrentNode;
    ULONG LastChild;
    PCK_AST_NODE NextNode;

    CK_ASSERT(Compiler->Assign == FALSE);

    //
    // Get to the leftmost node.
    //

    CurrentNode = Node;
    while (CurrentNode->Children > 1) {
        NextNode = CK_GET_AST_NODE(Compiler, CurrentNode->ChildIndex);

        CK_ASSERT((CurrentNode->Symbol == NextNode->Symbol) &&
                  (NextNode < CurrentNode));

        CurrentNode = NextNode;
    }

    //
    // Loop up the children evaluating the expression, and then calling the
    // add element function. This function is specially crafted for
    // initialization as the return value is always the list iself.
    //

    while (TRUE) {
        LastChild = CurrentNode->ChildIndex + CurrentNode->Children - 1;
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, LastChild));
        CkpEmitMethodCall(Compiler, 1, "append@1", 8);
        if (CurrentNode == Node) {
            break;
        }

        CurrentNode = CK_GET_AST_NODE(Compiler, CurrentNode->Parent);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpEmitNumericConstant (
    PCK_COMPILER Compiler,
    CK_VALUE Integer
    )

/*++

Routine Description:

    This routine emits a numeric constant.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Integer - Supplies the integer value to emit.

Return Value:

    None.

--*/

{

    if (CK_AS_INTEGER(Integer) <= 8) {
        CkpEmitOp(Compiler, CkOpLiteral0 + CK_AS_INTEGER(Integer));

    } else {
        CkpEmitConstant(Compiler, Integer);
    }

    return;
}

VOID
CkpCompilePrimaryIdentifier (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    BOOL Store
    )

/*++

Routine Description:

    This routine compiles a load or store of a raw identifier value. This
    could be a local, upvalue, field, or global.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Token - Supplies a pointer to the identifier token.

    Store - Supplies a boolean indicating whether this is a load operation
        (FALSE) or a store operation (FALSE). If it is a load, the value will
        be pushed on the stack. If it is a store, the current value on the top
        of the stack will be stored into this location.

Return Value:

    None.

--*/

{

    PCK_CLASS_COMPILER ClassCompiler;
    PCSTR Name;
    CK_OPCODE Op;
    CK_VARIABLE Variable;

    Name = Compiler->Parser->Source + Token->Position;
    Variable = CkpResolveNonGlobal(Compiler, Name, Token->Size);
    if (Variable.Index != -1) {
        if (Store == FALSE) {
            CkpLoadVariable(Compiler, Variable);

        //
        // Perform a store of a local or upvalue.
        //

        } else {
            switch (Variable.Scope) {
            case CkScopeLocal:
                CkpEmitByteOp(Compiler, CkOpStoreLocal, Variable.Index);
                break;

            case CkScopeUpvalue:
                CkpEmitByteOp(Compiler, CkOpStoreUpvalue, Variable.Index);
                break;

            default:

                CK_ASSERT(FALSE);

                break;
            }
        }

        return;
    }

    //
    // Search for a field if currently inside a class definition somewhere.
    //

    ClassCompiler = CkpGetClassCompiler(Compiler);
    if ((ClassCompiler != NULL) && (ClassCompiler->InStatic == FALSE)) {
        Variable.Index = CkpStringTableFind(&(ClassCompiler->Fields),
                                            Name,
                                            Token->Size);
    }

    if (Variable.Index != -1) {

        //
        // If this is a method bound directly to the class, then use the faster
        // load/store this opcode.
        //

        if ((Compiler->Parent != NULL) &&
            (ClassCompiler == Compiler->Parent->EnclosingClass)) {

            Op = CkOpLoadFieldThis;
            if (Store != FALSE) {
                Op = CkOpStoreFieldThis;
            }

            CkpEmitByteOp(Compiler, Op, Variable.Index);

        //
        // Otherwise, push "this", which is an upvalue somewhere, and then
        // load/store the field.
        //

        } else {
            CkpLoadThis(Compiler, Token);
            Op = CkOpLoadField;
            if (Store != FALSE) {
                Op = CkOpStoreField;
            }

            CkpEmitByteOp(Compiler, Op, Variable.Index);
        }

        return;
    }

    //
    // Search for a global.
    //

    Variable.Scope = CkScopeModule;
    Variable.Index = CkpStringTableFind(
                                    &(Compiler->Parser->Module->VariableNames),
                                    Name,
                                    Token->Size);

    if (Variable.Index != -1) {
        if (Store != FALSE) {
            CkpEmitShortOp(Compiler, CkOpStoreModuleVariable, Variable.Index);

        } else {
            CkpLoadVariable(Compiler, Variable);
        }

        return;
    }

    CkpCompileError(Compiler, Token, "Undefined variable");
    return;
}

VOID
CkpReadStringLiteralList (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine emits a string literal.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the string literal list to visit.

Return Value:

    None.

--*/

{

    PCK_AST_NODE LiteralNode;
    CK_VALUE String;
    CK_VALUE String2;
    PCK_STRING StringObject;
    PCK_STRING StringObject2;
    PLEXER_TOKEN Token;
    PCK_VM Vm;

    Vm = Compiler->Parser->Vm;

    //
    // Get to the leftmost node.
    //

    LiteralNode = Node;
    while (LiteralNode->Children > 1) {
        LiteralNode = CK_GET_AST_NODE(Compiler, LiteralNode->ChildIndex);
    }

    //
    // Go backwards reading source strings.
    //

    Token = CK_GET_AST_TOKEN(Compiler, LiteralNode->ChildIndex);
    String = CkpReadSourceString(Compiler, Token);
    while (LiteralNode != Node) {
        LiteralNode = CK_GET_AST_NODE(Compiler, LiteralNode->Parent);

        CK_ASSERT(LiteralNode->Children == 2);

        Token = CK_GET_AST_TOKEN(Compiler, LiteralNode->ChildIndex + 1);
        if (!CK_IS_STRING(String)) {
            return;
        }

        //
        // Read the next string, and combine it with the first.
        //

        StringObject = CK_AS_STRING(String);
        CkpPushRoot(Vm, &(StringObject->Header));
        String2 = CkpReadSourceString(Compiler, Token);
        if (!CK_IS_STRING(String2)) {
            CkpPopRoot(Vm);
            return;
        }

        StringObject2 = CK_AS_STRING(String2);
        CkpPushRoot(Vm, &(StringObject2->Header));
        String = CkpStringFormat(Vm, "@@", String, String2);
        CkpPopRoot(Vm);
        CkpPopRoot(Vm);
    }

    CkpEmitConstant(Compiler, String);
    return;
}

