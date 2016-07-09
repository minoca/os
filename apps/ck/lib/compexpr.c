/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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
CkpCompilePrimaryIdentifier (
    PCK_COMPILER Compiler,
    PLEXER_TOKEN Token,
    BOOL Store
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
CkpVisitLogicalExpression (
    PCK_COMPILER Compiler,
    PCK_AST_NODE Node
    )

/*++

Routine Description:

    This routine compiles a logical and or logical or expression.

Arguments:

    Compiler - Supplies a pointer to the compiler.

    Node - Supplies a pointer to the node to visit.

Return Value:

    None.

--*/

{

    UINTN Jump;
    CK_OPCODE Op;
    PLEXER_TOKEN Operator;

    CK_ASSERT((Node->Children == 1) || (Node->Children == 3));

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));
    if (Node->Children == 3) {
        Operator = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1);
        CkpComplainIfAssigning(Compiler, Operator, "Logical expression");
        if (Operator->Value == CkTokenLogicalOr) {
            Op = CkOpOr;

        } else {

            CK_ASSERT(Operator->Value == CkTokenLogicalAnd);

            Op = CkOpAnd;
        }

        Jump = CkpEmitJump(Compiler, Op);
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2));
        CkpPatchJump(Compiler, Jump);
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

    PLEXER_TOKEN Operator;

    CK_ASSERT((Node->Children == 1) || (Node->Children == 3));

    CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex));
    if (Node->Children == 3) {
        Operator = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1);
        CkpComplainIfAssigning(Compiler, Operator, "Binary expression");
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2));
        CkpEmitOperatorCall(Compiler, Operator->Value, 1, FALSE);
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
    PLEXER_TOKEN MethodToken;
    CK_SYMBOL Op;
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
    Compiler->Assign = Assign;
    OperatorToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 1);
    Operator = OperatorToken->Value;
    switch (Operator) {

    //
    // For increment and decrement, the expression to return is before the
    // increment/decrement, and it's already on the stack. Evaluate it again,
    // and then call the operator on it.
    //

    case CkTokenIncrement:
    case CkTokenDecrement:
        CkpVisitNode(Compiler, Expression);
        CkpEmitOperatorCall(Compiler, Operator, 0, FALSE);
        CkpEmitOp(Compiler, CkOpPop);
        break;

    //
    // Dot is a bound function call to an instance (ie call
    // mything.somefunc(...)). Parentheses represent an unbound call
    // (ie just got some object back and am now trying to call it).
    //

    case CkTokenDot:
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
            MethodToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex + 2);

            CK_ASSERT(MethodToken->Value == CkTokenIdentifier);

            Signature.Name = Compiler->Parser->Source + MethodToken->Position;
            Signature.Length = MethodToken->Size;
            Op = CkOpCall0;
            if (Expression->Symbol == CkNodePrimaryExpression) {
                SuperToken = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex);
                if (SuperToken->Value == CkTokenSuper) {
                    Op = CkOpSuperCall0;
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
        }

        break;

    //
    // Call the subscript operator method.
    //

    case CkTokenOpenBracket:

        CK_ASSERT(Node->Children == 4);

        Expression = CK_GET_AST_NODE(Compiler, Node->ChildIndex + 2);
        CkpVisitNode(Compiler, Expression);
        CkpEmitOperatorCall(Compiler, Operator, 1, Assign);
        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

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

    PCK_AST_NODE Constant;
    PLEXER_TOKEN Token;
    CK_VALUE Value;

    Token = CK_GET_AST_TOKEN(Compiler, Node->ChildIndex);

    //
    // If it's a ( expression ), go visit the inner expression.
    //

    if (Node->Children == 3) {
        CkpVisitNode(Compiler, CK_GET_AST_NODE(Compiler, Node->ChildIndex + 1));
        return;
    }

    Compiler->LastPrimaryToken = Token->Value;
    if ((Compiler->Assign != FALSE) && (Token->Value != CkTokenIdentifier)) {
        CkpCompileError(Compiler, Token, "Cannot assign to a constant");
        return;
    }

    switch (Token->Value) {
    case CkTokenIdentifier:
        CkpCompilePrimaryIdentifier(Compiler, Token, Compiler->Assign);
        break;

    case CkTokenConstant:
        Value = CkpReadSourceInteger(Compiler, Token, 10);
        if (CK_AS_INTEGER(Value) <= 8) {
            CkpEmitOp(Compiler, CkOpLiteral0 + CK_AS_INTEGER(Value));

        } else {
            CkpEmitConstant(Compiler, Value);
        }

        break;

    case CkTokenString:
        CkpEmitConstant(Compiler, CkpReadSourceString(Compiler, Token));
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
        CkpCallMethod(Compiler, 2, "_addEntry@2", 13);
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
        CkpCallMethod(Compiler, 1, "_addElement@1", 13);
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
    PSTR Name;
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
        Variable.Index = CkpSymbolTableFind(&(ClassCompiler->Fields),
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
    Variable.Index = CkpSymbolTableFind(
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

